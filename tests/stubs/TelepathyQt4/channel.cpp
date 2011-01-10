#include "channel.h"
#include "Connection"

#include <QVariantMap>

using namespace Tp;

const Tp::Feature Tp::Channel::FeatureCore = Tp::Feature(Tp::Channel::staticMetaObject.className(), 0, true);

struct Channel::Private : public QSharedData
{
    ConnectionPtr m_connection;
    QVariantMap m_immutableProperties;
    QString m_channelType;
    uint m_targetHandleType;
    uint m_targetHandle;
    bool m_isRequested;
    Contacts m_groupContacts;
    bool m_groupAreHandleOwnersAvailable;
    HandleOwnerMap m_groupHandleOwners;
    ContactPtr m_groupSelfContact;
};


Channel::Channel(const QString &objectPath) : StatefulDBusProxy(objectPath),
                                                               mPriv(new Private)
{
}

QStringList Channel::interfaces() const
{
    return m_interfaces;
}

Channel::~Channel()
{
}

ConnectionPtr Channel::connection() const
{
    return mPriv->m_connection;
}

QVariantMap Channel::immutableProperties() const
{
    return mPriv->m_immutableProperties;
}

QString Channel::channelType() const
{
    return mPriv->m_channelType;
}

uint Channel::targetHandleType() const
{
    return mPriv->m_targetHandleType;
}

uint Channel::targetHandle() const
{
    return mPriv->m_targetHandle;
}

bool Channel::isRequested() const
{
    return mPriv->m_isRequested;
}

Contacts Channel::groupContacts() const
{
    return mPriv->m_groupContacts;
}

bool Channel::groupAreHandleOwnersAvailable() const
{
    return mPriv->m_groupAreHandleOwnersAvailable;
}

HandleOwnerMap Channel::groupHandleOwners() const
{
    return mPriv->m_groupHandleOwners;
}

ContactPtr Channel::groupSelfContact() const
{
    return mPriv->m_groupSelfContact;
}

void Channel::ut_setInterfaces(const QStringList& interfaces)
{
    m_interfaces = interfaces;
}

void Channel::ut_setChannelType(const QString& channelType)
{
    mPriv->m_channelType = channelType;
}

void Channel::ut_setTargetHandleType(uint targetHandleType)
{
    mPriv->m_targetHandleType = targetHandleType;
}

void Channel::ut_setTargetHandle(uint targetHandle)
{
    mPriv->m_targetHandle = targetHandle;
}

void Channel::ut_setGroupContacts(const Contacts& contacts)
{
    mPriv->m_groupContacts = contacts;
}

void Channel::ut_setGroupAreHandleOwnersAvailable(bool handlesAvailable)
{
    mPriv->m_groupAreHandleOwnersAvailable = handlesAvailable;
}

void Channel::ut_setGroupHandleOwners(const HandleOwnerMap& ownerMap)
{
    mPriv->m_groupHandleOwners = ownerMap;
}

void Channel::ut_setGroupSelfContact(const ContactPtr& selfContact)
{
    mPriv->m_groupSelfContact = selfContact;
}

void Channel::ut_setImmutableProperties(const QVariantMap &immutableProperties)
{
    mPriv->m_immutableProperties = immutableProperties;
}

void Channel::ut_setConnection(const ConnectionPtr &connection)
{
    mPriv->m_connection = connection;
}

void Channel::ut_setIsRequested(bool requested)
{
    mPriv->m_isRequested = requested;
}
