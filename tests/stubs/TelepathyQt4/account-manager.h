
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

    QList<AccountPtr> validAccounts();

    static AccountManagerPtr create();

    AccountPtr accountForPath(const QString &path);

    void ut_clearAccountList();
    void ut_restoreDefaultState();

    void ut_addAccount( AccountPtr );
    void ut_deletAccount( AccountPtr );

Q_SIGNALS:
    void accountCreated(const QString &path);
    void accountRemoved(const QString &path);
    void accountValidityChanged(const QString &path, bool valid);

private slots:
    void ut_accountValidityChanged(const bool);

private:
    QList<AccountPtr> m_fakeAccountList;


};

} // Tp

#endif
