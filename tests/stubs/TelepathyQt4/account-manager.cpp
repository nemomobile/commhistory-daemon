
#include "account-manager.h"
#include <QDebug>
#include <QTimer>

namespace Tp
{

AccountManager::AccountManager()
        :StatelessDBusProxy()
{
    m_fakeAccountSet.ut_setAccountManager(AccountManagerPtr(this));
}

AccountManagerPtr  AccountManager::create()
{
    return AccountManagerPtr(new AccountManager());
}


void Tp::AccountManager::ut_addAccount(AccountPtr accPtr)
{
    m_fakeAccountSet.ut_accounts().append(accPtr);
}

void AccountManager::ut_deletAccount(AccountPtr accPtr)
{
    m_fakeAccountSet.ut_accounts().removeAll(accPtr);
}

AccountSetPtr AccountManager::validAccounts() const
{
    return AccountSetPtr(const_cast<AccountSet*>(&m_fakeAccountSet));
}

AccountPtr AccountManager::accountForPath(const QString &path) const
{
    foreach (AccountPtr acc, m_fakeAccountSet.accounts()) {
        if (acc && acc->objectPath() == path)
            return acc;
    }
    return AccountPtr(0);
}

}
