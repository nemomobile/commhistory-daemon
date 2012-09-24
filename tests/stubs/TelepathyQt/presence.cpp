#include "Presence"

namespace Tp
{

struct Presence::Private : public QSharedData
{
    QString m_status;
    QString m_statusMessage;
};

Presence::Presence() : mPriv(new Private)
{
}

Presence::~Presence()
{
}

Presence::Presence(const Presence& other) : mPriv(other.mPriv)
{
}

Presence::Presence(const QString& status, const QString& statusMessage) : mPriv(new Private)
{
    ut_setStatus(status);
    ut_setStatusMessage(statusMessage);
}

Presence Presence::offline(const QString & statusMessage)
{
    return Presence(QString("offline"), statusMessage);
}

QString Presence::status() const
{
    return mPriv->m_status;
}

QString Presence::statusMessage() const
{
    return mPriv->m_statusMessage;
}

void Presence::ut_setStatus(const QString& status)
{
    mPriv->m_status = status;
}

void Presence::ut_setStatusMessage(const QString& statusMessage)
{
    mPriv->m_statusMessage = statusMessage;
}

Presence &Presence::operator=(const Presence &other)
{
    mPriv = other.mPriv;
    return *this;
}

}
