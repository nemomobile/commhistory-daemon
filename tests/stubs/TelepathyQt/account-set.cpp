#include "account-set.h"
#include "account.h"

#include "AccountManager"

namespace Tp
{

AccountSet::AccountSet()
{
}

AccountSet::~AccountSet()
{
}

AccountSet::AccountSet(const AccountManagerPtr &accountManager) :
        m_accountManager(accountManager)
{
}

AccountManagerPtr AccountSet::accountManager() const
{
    return m_accountManager;
}

QList<AccountPtr> AccountSet::ut_accounts()
{
    return m_accounts;
}

void AccountSet::ut_setAccountManager(AccountManagerPtr am)
{
    m_accountManager = am;
}

} // Tp
