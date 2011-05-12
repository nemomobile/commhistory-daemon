#ifndef ACCOUNTOPERATIONSOBSERVER_H
#define ACCOUNTOPERATIONSOBSERVER_H

#include <QObject>
#include <QList>

#include <TelepathyQt4/AccountManager>
#include <TelepathyQt4/Account>
#include <TelepathyQt4/PendingOperation>

namespace CommHistory
{
    class GroupModel;
    class CallModel;
}

/**
\class AccountOperationsObserver
\brief
*/
class AccountOperationsObserver : public QObject
{
    Q_OBJECT

public:
    AccountOperationsObserver(Tp::AccountManagerPtr accountManager, QObject* parent = 0);

private Q_SLOTS:
    void slotAccountManagerReady(Tp::PendingOperation *op);
    void slotConnectToRemovalSignal(const Tp::AccountPtr &account);
    void slotAccountRemoved();
    void slotDeleteConversations();
    void slotDeleteCalls();

private:
    CommHistory::GroupModel *m_pGroupModel;
    CommHistory::CallModel *m_pCallModel;
    Tp::AccountManagerPtr m_AccountManager;
    QList<QString> m_accountPathsForConvs;
    QList<QString> m_accountPathsForCalls;
};

#endif // ACCOUNTOPERATIONSOBSERVER_H
