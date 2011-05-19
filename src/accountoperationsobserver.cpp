#include "accountspecificcallmodel.h"
#include "accountoperationsobserver.h"
#include "notificationmanager.h"

#include <TelepathyQt4/PendingReady>

#include <CommHistory/GroupModel>
#include <CommHistory/CallModel>

using namespace RTComLogger;

AccountOperationsObserver::AccountOperationsObserver(Tp::AccountManagerPtr accountManager, QObject* parent) :
    QObject(parent),
    m_pGroupModel(0),
    m_AccountManager(accountManager)
{
    qDebug() << Q_FUNC_INFO << "START";

    if (!m_AccountManager.isNull()) {
        if (!m_AccountManager->isReady()) {
            qDebug() << Q_FUNC_INFO << "Account manager is not ready. Making it ready...";
            connect(m_AccountManager->becomeReady(),
                SIGNAL(finished(Tp::PendingOperation *)),
                SLOT(slotAccountManagerReady(Tp::PendingOperation *)));
        } else {
            qDebug() << Q_FUNC_INFO << "Account manager is already ready.";
            connectToAccounts();
        }
    } else {
        qWarning() << "Account manager is null!";
    }

    qDebug() << Q_FUNC_INFO << "END";
}

void AccountOperationsObserver::connectToAccounts()
{
    qDebug() << Q_FUNC_INFO << "START";

    // First of all we must connect to listen removed()-signals of the accounts created in the future:
    connect(m_AccountManager.data(),
        SIGNAL(newAccount(const Tp::AccountPtr &)),
        SLOT(slotConnectToRemovalSignal(const Tp::AccountPtr &)),
        Qt::UniqueConnection);

    // Connect to listen removal of accounts that already exist:
    foreach(const Tp::AccountPtr &account, m_AccountManager->allAccounts()) {
        slotConnectToRemovalSignal(account);
    }

    qDebug() << Q_FUNC_INFO << "END";
}

// S L O T S

void AccountOperationsObserver::slotAccountManagerReady(Tp::PendingOperation *op)
{
    qDebug() << Q_FUNC_INFO << "START";

    if (op->isError()) {
        qWarning() << "Account manager cannot become ready:" << op->errorName() << "-" << op->errorMessage();
        return;
    }

    connectToAccounts();

    qDebug() << Q_FUNC_INFO << "END";
}

void AccountOperationsObserver::slotConnectToRemovalSignal(const Tp::AccountPtr &account)
{
    qDebug() << Q_FUNC_INFO << "START";

    connect(account.data(),
            SIGNAL(removed()),
            SLOT(slotAccountRemoved()),
            Qt::UniqueConnection);

    qDebug() << Q_FUNC_INFO << "END";
}

void AccountOperationsObserver::slotAccountRemoved()
{
    qDebug() << Q_FUNC_INFO << "START";

    Tp::Account *account = qobject_cast<Tp::Account *>(sender());

    if (account) {
        QString accountPath = account->objectPath();
        qDebug() << Q_FUNC_INFO << "Account " << accountPath << " removed.";

        /* Store paths of removed accounts into lists so that if call and/or group models are not
           yet ready while more accounts are being removed then all of those removed accounts
           are really handled. */
        m_accountPathsForConvs.append(accountPath);

        AccountSpecificCallModel *callModel = new AccountSpecificCallModel(this);
        callModel->setPropertyMask(CommHistory::Event::PropertySet()
                                   << CommHistory::Event::Type
                                   << CommHistory::Event::LocalUid);

        connect(callModel,
                SIGNAL(modelReady(bool)),
                this,
                SLOT(slotDeleteCalls()),
                Qt::UniqueConnection);

        connect(callModel,
                SIGNAL(rowsRemoved(const QModelIndex&,int,int)),
                this,
                SLOT(slotRowsRemoved(const QModelIndex&,int,int)),
                Qt::UniqueConnection);

        callModel->getEvents(accountPath);
        m_accountPathsForCalls.insert(accountPath, callModel);

        if (!m_pGroupModel) {
            // Notification manager already has a group model so using that instead of creating a separate one:
            m_pGroupModel = NotificationManager::instance()->groupModel();
        }

        if (m_pGroupModel && !m_pGroupModel->isReady()) {
            connect(m_pGroupModel,
                    SIGNAL(modelReady(bool)),
                    this,
                    SLOT(slotDeleteConversations()),
                    Qt::UniqueConnection);
        } else {
            slotDeleteConversations();
        }
    }

    qDebug() << Q_FUNC_INFO << "END";
}

void AccountOperationsObserver::slotDeleteConversations()
{
    qDebug() << Q_FUNC_INFO << "START";

    if (!m_pGroupModel)
        return;

    QList<int> groupsToBeDeleted;

    foreach (QString accountPath, m_accountPathsForConvs) {
        for (int i=0;i<m_pGroupModel->rowCount();i++) {
            QModelIndex index = m_pGroupModel->index(i,0);
            if (index.isValid()) {
                CommHistory::Group group = m_pGroupModel->group(index);
                int groupId = group.id();
                QString localId = group.localUid();
                if (localId == accountPath) {
                    qDebug() << Q_FUNC_INFO << "Group " << groupId << " to be deleted";
                    groupsToBeDeleted.append(groupId);
                }
            }
        }
    }

    if (!groupsToBeDeleted.isEmpty()) {
        if (!m_pGroupModel->deleteGroups(groupsToBeDeleted, true)) {
            qWarning() << "Error while deleting groups: " << groupsToBeDeleted;
        }
    }

    m_accountPathsForConvs.clear();

    qDebug() << Q_FUNC_INFO << "END";
}

void AccountOperationsObserver::slotDeleteCalls()
{
    qDebug() << Q_FUNC_INFO << "START";

    AccountSpecificCallModel *callModel = qobject_cast<AccountSpecificCallModel *>(sender());

    if (!callModel) {
        qWarning() << "Null AccountSpecificCallModel!";
        return;
    }

    QList<CommHistory::Event> callsToBeDeleted;

    for (int i=0;i<callModel->rowCount();i++) {
        QModelIndex index = callModel->index(i,0);
        if (index.isValid()) {
            CommHistory::Event callEvent = callModel->event(index);
            qDebug() << Q_FUNC_INFO << "Call " << callEvent.id() << " to be deleted";
            callsToBeDeleted.append(callEvent);
        }
     }

    foreach(CommHistory::Event callEvent, callsToBeDeleted) {
        // No deleteEvents implemented, so calling deleteEvent for every event to be removed:
        if (!callModel->deleteEvent(callEvent)) {
            qWarning() << "Error while deleting call " << callEvent.id();
        }
     }

    if (callsToBeDeleted.isEmpty()) {
        QString accountPath = m_accountPathsForCalls.key(callModel);
        qDebug() << Q_FUNC_INFO << "No calls to be deleted for " << accountPath << ". We can delete the AccountSpecificCallModel.";
        m_accountPathsForCalls.take(accountPath)->deleteLater();
    }

    qDebug() << Q_FUNC_INFO << "END";
}

void AccountOperationsObserver::slotRowsRemoved(const QModelIndex& index, int start, int end)
{
    Q_UNUSED(index)

    qDebug() << Q_FUNC_INFO << "start: " << start << " end: " << end;

    AccountSpecificCallModel *callModel = qobject_cast<AccountSpecificCallModel *>(sender());

    if (!callModel) {
        qWarning() << "Null AccountSpecificCallModel!";
        return;
    }

    if (callModel->rowCount() == 0) {
        qDebug() << Q_FUNC_INFO << "Last event in AccountSpecificCallModel deleted. We can delete the model.";
        QString accountPath = m_accountPathsForCalls.key(callModel);
        m_accountPathsForCalls.take(accountPath)->deleteLater();
    }

    qDebug() << Q_FUNC_INFO << "END";
}
