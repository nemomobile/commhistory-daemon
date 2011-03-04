#include "connection.h"
#include "contact-manager.h"

namespace Tp
{

const Tp::Feature Tp::Connection::FeatureCore = Tp::Feature(Tp::Connection::staticMetaObject.className(), 0, true);
const Tp::Feature Tp::Connection::FeatureSimplePresence = Tp::Feature(Tp::Connection::staticMetaObject.className(), 1, true);

struct Connection::Private
{
    QStringList m_interfaces;
    ContactManagerPtr m_manager;
    uint m_selfHandle;
};

Connection::Connection(const QString &objectPath) :
        StatefulDBusProxy(objectPath),
        mPriv(new Private)
{
    mPriv->m_manager = ContactManagerPtr(new ContactManager());
}

Connection::~Connection()
{
    delete mPriv;
}

QStringList Connection::interfaces() const
{
    return mPriv->m_interfaces;
}

bool Connection::hasInterface(const char * interface)
{
    return mPriv->m_interfaces.contains(QString(interface));
}

ContactManagerPtr Connection::contactManager() const
{
    return mPriv->m_manager;
}

uint Connection::selfHandle() const
{
    return mPriv->m_selfHandle;
}

void Connection::ut_setInterfaces(const QStringList& interfaces)
{
    mPriv->m_interfaces = interfaces;
}

void Connection::ut_setSelfHandle(uint handle)
{
    mPriv->m_selfHandle = handle;
}
}
