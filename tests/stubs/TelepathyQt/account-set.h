
#ifndef _TelepathyQt4_account_set_h_HEADER_GUARD_
#define _TelepathyQt4_account_set_h_HEADER_GUARD_

//#include "Filter"
//#include "Object"
#include "Types"
#include "Account"

#include <QList>
#include <QString>
#include <QVariantMap>

namespace Tp
{

class AccountSet : public Object
{
    Q_OBJECT
    Q_DISABLE_COPY(AccountSet)

public:
    AccountSet();
    AccountSet(const AccountManagerPtr &accountManager);
    virtual ~AccountSet();

    AccountManagerPtr accountManager() const;

    //AccountFilterConstPtr filter() const;

    QList<AccountPtr> accounts() const {return m_accounts;}

//Q_SIGNALS:
//    void accountAdded(const Tp::AccountPtr &account);
//    void accountRemoved(const Tp::AccountPtr &account);

public:
    QList<AccountPtr> ut_accounts();
    void ut_setAccountManager(AccountManagerPtr am);

private:
    AccountManagerPtr m_accountManager;
    QList<AccountPtr> m_accounts;
};

} // Tp

#endif
