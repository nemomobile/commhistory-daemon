/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Reto Zingg <reto.zingg@nokia.com>
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the GNU Lesser General Public License version 2.1 as
** published by the Free Software Foundation.
**
** This library is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
** or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
** License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this library; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
**
******************************************************************************/

#ifndef ACCOUNTOPERATIONSOBSERVER_H
#define ACCOUNTOPERATIONSOBSERVER_H

#include <TelepathyQt4/AccountManager>
#include <TelepathyQt4/Account>
#include <TelepathyQt4/PendingOperation>

namespace CommHistory
{
    class GroupModel;
    class CallModel;
    class Event;
}

namespace RTComLogger
{
    class AccountSpecificCallModel;
}

namespace RTComLogger
{
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

Q_SIGNALS:
    void removeAccountNotifications(const QString &accountPath);

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
    void slotConnectToSignals(const Tp::AccountPtr &account);

    /*!
     * \brief Slot getting called when telepathy account changes state.
     *
     * Removes notifications of the account is disabled. Does nothing when account is enabled.
     */
    void slotAccountStateChanged(bool isEnabled);
    /*!
     * \brief Slot getting called when telepathy account is removed.
     *
     * Adds path of the removed account into a list used when calling methods to
     * remove conversations and calls either a) directly or b) via signal from models
     * indicating they are ready. Uses CommHistory::GroupModel from NotificationManager
     * and creates and populates CommHistory::CallModel by itself. Additionally removes
     * all notifications of the account.
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
    /*!
     * \brief Slot getting called when a row is deleted from AccountSpecificCallModel.
     *
     * \param index Parent item under which the rows are being removed.
     * \param start Starting row for removal.
     * \param end   Ending row for removal.
     *
     */
    void slotRowsRemoved(const QModelIndex& index, int start, int end);

private:
    /*!
     * \brief Connects to signals of existing accounts and also to the signal
     * from Tp::AccountManager indicating adding of a new account.
     *
     */
    void connectToAccounts();

private:
    CommHistory::GroupModel *m_pGroupModel;
    Tp::AccountManagerPtr m_AccountManager;
    QList<QString> m_accountPathsForConvs; // Conversations of these account paths should be removed.
    QMap<QString,RTComLogger::AccountSpecificCallModel*> m_accountPathsForCalls; // Calls of these account paths should be removed.
    QMap<QString, Tp::AccountPtr> m_accounts;
};

} // namespace RTComLogger

#endif // ACCOUNTOPERATIONSOBSERVER_H
