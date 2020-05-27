#include "XmppUser.hpp"

namespace Telegram {

namespace Server {

void XmppUser::setDcId(quint32 dcId)
{
    m_dcId = dcId;
}

void XmppUser::setJid(const QString &jid)
{
    m_jid = jid;
    if (!m_userId) {
        m_userId = ::qHash(jid);
        m_box.setUserId(userId());
    }
}

} // Server

} // Telegram