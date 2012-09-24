#include "PendingContacts"

namespace Tp
{

PendingContacts::PendingContacts()
{
}

QList<Tp::ContactPtr> PendingContacts::contacts()
{
    return m_contacts;
}

void PendingContacts::ut_setPendingContacts(const QList<Tp::ContactPtr>& contacts)
{
    m_contacts = contacts;
}

} // Tp
