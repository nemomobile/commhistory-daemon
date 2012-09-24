
#ifndef _TelepathyQt4_cli_account_manager_h_HEADER_GUARD_
#define _TelepathyQt4_cli_account_manager_h_HEADER_GUARD_

#include "Types"
#include "DBusProxy"
#include "Account"
#include "AccountSet"

namespace Tp
{

class AccountManager : public StatelessDBusProxy
{
    Q_OBJECT
    Q_DISABLE_COPY(AccountManager)

public:

    AccountManager();

    static AccountManagerPtr create();

    AccountSetPtr validAccounts() const;
    AccountPtr accountForPath(const QString &path) const;

    void ut_addAccount( AccountPtr acc);
    void ut_deletAccount( AccountPtr acc);

private:
    AccountSet m_fakeAccountSet;
};

} // Tp

#endif
