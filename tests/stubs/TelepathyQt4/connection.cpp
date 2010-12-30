#include "connection.h"
#include "contact-manager.h"

namespace Tp
{

const Tp::Feature Tp::Connection::FeatureCore = Tp::Feature(Tp::Connection::staticMetaObject.className(), 0, true);
const Tp::Feature Tp::Connection::FeatureSimplePresence = Tp::Feature(Tp::Connection::staticMetaObject.className(), 1, true);

struct Connection::Private
{
    QStringList m_interfaces;
    ContactManager* m_manager;
};

Connection::Connection(const QString &objectPath, QObject *parent) :
        StatefulDBusProxy(objectPath, parent),
        mPriv(new Private)
{
    mPriv->m_manager = new ContactManager();
}

Connection::~Connection()
{
    delete mPriv->m_manager;
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

ContactManager* Connection::contactManager() const
{
    return mPriv->m_manager;
}

void Connection::ut_setInterfaces(const QStringList& interfaces)
{
    mPriv->m_interfaces = interfaces;
}

}
