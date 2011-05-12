#ifndef ACCOUNTOPERATIONSOBSERVER_H
#define ACCOUNTOPERATIONSOBSERVER_H

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
\brief Listens telepathy accounts being removed and when that happens, removes both
       conversations and calls belonging to that removed account.
*/
class AccountOperationsObserver : public QObject
{
    Q_OBJECT

public:
    /*!
     * \brief Constructor.
     *
     * Checks if the Tp::AccountManager delivered as a parameter is ready and if not
     * then tries to get it ready.
     *
     * \param accountManager Telepathy AccountManager
     * \param parent Parent object
     */
    AccountOperationsObserver(Tp::AccountManagerPtr accountManager, QObject* parent = 0);

private Q_SLOTS:
    /*!
     * \brief Slot getting called when Tp::AccountManager is ready.
     *
     * \param op Tp::PendingOperation indicating if there was an error or not when
     *           getting Tp::AccountManager ready.
     */
    void slotAccountManagerReady(Tp::PendingOperation *op);
    /*!
     * \brief Connects removed()-signal of the given Tp::Account to slotAccountRemoved().
     *
     * \param account Tp::Account whose removed() signal is connected to be listened.
     */
    void slotConnectToRemovalSignal(const Tp::AccountPtr &account);
    /*!
     * \brief Slot getting called when telepathy account is removed.
     *
     * Adds path of the removed account into a list used when calling methods to
     * remove conversations and calls either a) directly or b) via signal from models
     * indicating they are ready. Uses CommHistory::GroupModel from NotificationManager
     * and creates and populates CommHistory::CallModel by itself.
     *
     */
    void slotAccountRemoved();
    /*!
     * \brief Deletes conversations of an account.
     *
     * Deletes all conversations of the account added into the list of removed accounts
     * in slotAccountRemoved().
     *
     */
    void slotDeleteConversations();
    /*!
     * \brief Deletes calls of an account.
     *
     * Deletes all calls of the account added into the list of removed accounts
     * in slotAccountRemoved().
     *
     */
    void slotDeleteCalls();

private:
    /*!
     * \brief Connects to signals of existing accounts and also to the signal
     * from Tp::AccountManager indicating adding of a new account.
     *
     */
    void connectToAccounts();

private:
    CommHistory::GroupModel *m_pGroupModel;
    CommHistory::CallModel *m_pCallModel;
    Tp::AccountManagerPtr m_AccountManager;
    QList<QString> m_accountPathsForConvs; // Conversations of these account paths should be removed.
    QList<QString> m_accountPathsForCalls; // Calls of these account paths should be removed.
};

#endif // ACCOUNTOPERATIONSOBSERVER_H
