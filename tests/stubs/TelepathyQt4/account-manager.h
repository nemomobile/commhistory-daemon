
#ifndef _TelepathyQt4_cli_account_manager_h_HEADER_GUARD_
#define _TelepathyQt4_cli_account_manager_h_HEADER_GUARD_

#include "Types"
#include "PendingReady"
#include "Account"

#include <QObject>
#include <QAtomicInt>

namespace Tp
{

class AccountManager : public PendingReady, public QAtomicInt
{
    Q_OBJECT
    Q_DISABLE_COPY(AccountManager)

public:

    AccountManager();

    static AccountManagerPtr create();

    void ut_addAccount( AccountPtr );
    void ut_deletAccount( AccountPtr );

private:
    QList<AccountPtr> m_fakeAccountList;


};

} // Tp

#endif
