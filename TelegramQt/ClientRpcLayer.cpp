/*
   Copyright (C) 2018 Alexandr Akulich <akulichalexander@gmail.com>

   This file is a part of TelegramQt library.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

 */

#include "ClientRpcLayer.hpp"
#include "IgnoredMessageNotification.hpp"
#include "SendPackageHelper.hpp"
#include "CTelegramStream.hpp"
#include "Utils.hpp"
#include "Debug_p.hpp"
#include "CAppInformation.hpp"
#include "PendingRpcOperation.hpp"

#include "MTProto/MessageHeader.hpp"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(c_clientRpcLayerCategory, "telegram.client.rpclayer", QtDebugMsg)

namespace Telegram {

namespace Client {

RpcLayer::RpcLayer(QObject *parent) :
    BaseRpcLayer(parent)
{
}

void RpcLayer::setAppInformation(CAppInformation *appInfo)
{
    m_appInfo = appInfo;
}

void RpcLayer::setSessionId(quint64 newSessionId)
{
    m_sessionId = newSessionId;
}

bool RpcLayer::processRpcQuery(const QByteArray &data, quint64 serverMessageId)
{
    Q_UNUSED(serverMessageId)
    CTelegramStream stream(data);
    TLValue value;
    stream >> value;
    switch (value) {
    case TLValue::NewSessionCreated:
        processSessionCreated(stream);
        break;
    case TLValue::MsgContainer:
        processContainer(stream);
        break;
    case TLValue::RpcResult:
        qCDebug(c_clientRpcLayerCategory) << "processRpcQuery(stream);";
    {
        quint64 messageId = 0;
        stream >> messageId;
        PendingRpcOperation *op = m_operations.value(messageId);
        if (!op) {
            qCWarning(c_clientRpcLayerCategory) << "processRpcQuery():" << "Unhandled operation" << messageId;
            return false;
        }
        op->setFinishedWithReplyData(stream.readAll());
#define DUMP_CLIENT_RPC_PACKETS
#ifdef DUMP_CLIENT_RPC_PACKETS
        qCDebug(c_clientRpcLayerCategory) << "Client: Answer for message" << messageId << "op:" << op;
        qCDebug(c_clientRpcLayerCategory).noquote() << "Client: RPC Reply bytes:" << op->replyData().size() << op->replyData().toHex();
#endif
        qCDebug(c_clientRpcLayerCategory) << "processRpcQuery():" << "Set finished op" << op << "messageId:" << messageId << "error:" << op->errorDetails();
    }
        break;
    case TLValue::MsgsAck:
        qCDebug(c_clientRpcLayerCategory) << "processMessageAck(stream);";
        break;
    case TLValue::BadMsgNotification:
    case TLValue::BadServerSalt:
        processIgnoredMessageNotification(stream);
        break;
    case TLValue::GzipPacked:
        qCDebug(c_clientRpcLayerCategory) << "processGzipPackedRpcQuery(stream);";
        break;
    case TLValue::Pong:
        qCDebug(c_clientRpcLayerCategory) << "processPingPong(stream);";
        break;
    default:
        qCDebug(c_clientRpcLayerCategory) << Q_FUNC_INFO << "value:" << value;
        break;
    }
    return false;
}

void RpcLayer::processSessionCreated(CTelegramStream &stream)
{
    // https://core.telegram.org/mtproto/service_messages#new-session-creation-notification
    quint64 firstMsgId;
    quint64 uniqueId;
    quint64 serverSalt;

    stream >> firstMsgId;
    stream >> uniqueId;
    stream >> serverSalt;
    qCDebug(c_clientRpcLayerCategory) << "processSessionCreated(stream)"
                                      << firstMsgId
                                      << uniqueId
                                      << serverSalt;
}

bool RpcLayer::processContainer(CTelegramStream &stream)
{
    // https://core.telegram.org/mtproto/service_messages#simple-container
    quint32 itemsCount;
    stream >> itemsCount;
    qCDebug(c_clientRpcLayerCategory) << "processContainer(stream)" << itemsCount;

    bool processed = true;

    for (quint32 i = 0; i < itemsCount; ++i) {
        quint64 id;
        stream >> id;
        //TODO: ack

        quint32 seqNo;
        stream >> seqNo;

        quint32 size;
        stream >> size;

        processed = processRpcQuery(stream.readBytes(size), id) && processed;
    }
    return processed;
}

void RpcLayer::processIgnoredMessageNotification(CTelegramStream &stream)
{
    qCDebug(c_clientRpcLayerCategory) << "processIgnoredMessageNotification(stream);";
    // https://core.telegram.org/mtproto/service_messages_about_messages#notice-of-ignored-error-message
    MTProto::IgnoredMessageNotification notification;
    stream >> notification;

    switch (notification.errorCode) {
    case MTProto::IgnoredMessageNotification::IncorrectServerSalt:
        m_sendHelper->setServerSalt(m_receivedServerSalt);
        qCDebug(c_clientRpcLayerCategory) << "Local serverSalt fixed to" << m_receivedServerSalt;
        resendRpcMessage(notification.messageId);
        break;
    default:
        qCWarning(c_clientRpcLayerCategory) << "Unhandled error:" << notification.toString();
        break;
    }
}

bool RpcLayer::processDecryptedMessageHeader(const MTProto::FullMessageHeader &header)
{
    if (m_sendHelper->serverSalt() != header.serverSalt) {
        qCDebug(c_clientRpcLayerCategory) << Q_FUNC_INFO << "Received different server salt:" << m_receivedServerSalt << "(remote) vs" << m_sendHelper->serverSalt() << "(local)";
        m_receivedServerSalt = header.serverSalt;
    }

    if (m_sessionId != header.sessionId) {
        qCWarning(c_clientRpcLayerCategory) << Q_FUNC_INFO << "Session Id is wrong.";
        return false;
    }
    return true;
}

QByteArray RpcLayer::getEncryptionKeyPart() const
{
    return m_sendHelper->getClientKeyPart();
}

QByteArray RpcLayer::getVerificationKeyPart() const
{
    return m_sendHelper->getServerKeyPart();
}

bool RpcLayer::sendRpc(PendingRpcOperation *operation)
{
    quint64 messageId = 0;
    // We have to add InitConnection here because sendPackage() implementation is shared with server
    if (m_sequenceNumber == 0) {
        // sendPackage() adjusts sequence number
        messageId = sendPackage(getInitConnection() + operation->requestData(), SendMode::Client);
    } else {
        messageId = sendPackage(operation->requestData(), SendMode::Client);
    }
    if (!messageId) {
        return false;
    }
    operation->setRequestId(messageId);
    operation->setConnection(m_sendHelper->getConnection());
    m_operations.insert(messageId, operation);
    return true;
}

bool RpcLayer::resendRpcMessage(quint64 messageId)
{
    PendingRpcOperation *operation = m_operations.value(messageId);
    if (!operation) {
        qCWarning(c_clientRpcLayerCategory) << "Unable to find the message to resend" << messageId;
        return false;
    }
    qCDebug(c_clientRpcLayerCategory) << "Resend message"
                                      << messageId
                                      << TLValue::firstFromArray(operation->requestData());
    return sendRpc(operation);
}

void RpcLayer::onConnectionFailed()
{
    for (PendingRpcOperation *op : m_operations) {
        if (!op->isFinished()) {
            op->setFinishedWithError({{ PendingOperation::c_text(), QStringLiteral("connection failed")}});
        }
    }
    m_operations.clear();
}

QByteArray RpcLayer::getInitConnection() const
{
#ifdef DEVELOPER_BUILD
    qCDebug(c_clientRpcLayerCategory) << Q_FUNC_INFO << "layer" << TLValue::CurrentLayer;
#endif
    CTelegramStream outputStream(CTelegramStream::WriteOnly);
    outputStream << TLValue::InvokeWithLayer;
    outputStream << TLValue::CurrentLayer;
    outputStream << TLValue::InitConnection;
    outputStream << m_appInfo->appId();
    outputStream << m_appInfo->deviceInfo();
    outputStream << m_appInfo->osInfo();
    outputStream << m_appInfo->appVersion();
#if TELEGRAMQT_LAYER >= 67
    outputStream << m_appInfo->languageCode(); // System language
    outputStream << QString(); // Langpack
#endif
    outputStream << m_appInfo->languageCode(); // Lang code
    return outputStream.getData();
}

} // Client namespace

} // Telegram namespace
