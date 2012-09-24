#include "ContactManager"

namespace Tp
{

ContactManager::ContactManager()
{

}

ContactManager::~ContactManager()
{
}

Features ContactManager::supportedFeatures() const
{
    return m_supportedFeatures;
}

PendingContacts* ContactManager::contactsForHandles(const UIntList&,
        const Features&)
{
    return new PendingContacts();
}

PendingContacts* ContactManager::upgradeContacts(const QList<ContactPtr>&,
        const Features&)
{
    return new PendingContacts();
}
void ContactManager::ut_setSupportedFeatures(const Features& features)
{
    m_supportedFeatures = features;
}

void ContactManager::ut_emitGroupMembersChanged(const QString &group,
                                const Tp::Contacts &groupMembersAdded,
                                const Tp::Contacts &groupMembersRemoved,
                                const Tp::Channel::GroupMemberChangeDetails &details)
{
    emit groupMembersChanged(group, groupMembersAdded, groupMembersRemoved, details);
}

}
