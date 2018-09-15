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

#include "MessageHeader.hpp"

#include "CRawStream.hpp"

namespace Telegram {

namespace MTProto {

CRawStream &operator>>(CRawStream &stream, MessageHeader &message)
{
    stream >> message.messageId;
    stream >> message.sequenceNumber;
    stream >> message.contentLength;
    return stream;
}

CRawStream &operator<<(CRawStream &stream, const MessageHeader &message)
{
    stream << message.messageId;
    stream << message.sequenceNumber;
    stream << message.contentLength;
    return stream;
}

CRawStream &operator>>(CRawStream &stream, FullMessageHeader &header)
{
    stream >> header.serverSalt;
    stream >> header.sessionId;
    stream >> header.messageId;
    stream >> header.sequenceNumber;
    stream >> header.contentLength;
    return stream;
}

CRawStream &operator<<(CRawStream &stream, const FullMessageHeader &header)
{
    stream << header.serverSalt;
    stream << header.sessionId;
    stream << header.messageId;
    stream << header.sequenceNumber;
    stream << header.contentLength;
    return stream;
}

} // MTProto

} // Telegram
