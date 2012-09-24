#ifndef _TelepathyQt4_pending_contacts_h_HEADER_GUARD_
#define _TelepathyQt4_pending_contacts_h_HEADER_GUARD_

#include "Types"
#include "PendingOperation"
#include "Contact"

namespace Tp
{

class PendingContacts : public PendingOperation {
        Q_OBJECT
    public:
        PendingContacts();
        QList<Tp::ContactPtr> contacts();

        void ut_setPendingContacts(const QList<Tp::ContactPtr>& contacts);

    private:
        QList<Tp::ContactPtr> m_contacts;
};

} // Tp

#endif
