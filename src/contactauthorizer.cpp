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

#include "contactauthorizer.h"
#include "constants.h"
#include "locstrings.h"

// Qt
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QMessageBox>
#include <QUuid>

// MeeGo
#include <MNotification>
#include <MRemoteAction>

// Tp
#include <TelepathyQt4/AvatarData>
#include <TelepathyQt4/ContactManager>
#include <TelepathyQt4/PendingReady>
#include <TelepathyQt4/PendingContacts>
#include <TelepathyQt4/Contact>

// constants
static const char* AuthorizationNotificationType = "x-nokia.messaging.authorizationrequest";

#define OBJECT_PATH QLatin1String("/")
#define BUDDY_AUTHORIZER_SERVICE_NAME    QLatin1String("com.nokia.buddyauthorizer-service")
#define BUDDY_AUTHORIZER_INTERFACE_NAME  QLatin1String("com.nokia.buddyauthorizerinterface")
#define ACTIVATE_AUTHIRIZATION_METHOD    QLatin1String("activateAuthorization")
#define BUDDY_AUTHORIZER_UI_METHOD       QLatin1String("showAuthorisationQuery")
#define BUDDY_AUTHORIZER_UI_SIGNAL       QLatin1String("dialogDismissed")

#define BUDDY_AUTHORIZER_APP_NAME    QLatin1String("com.nokia.buddyauthorizer")
#define BUDDY_AUTHORIZER_APP_IF    QLatin1String("com.nokia.MApplicationIf")
#define BUDDY_AUTHORIZER_APP_EXIT    QLatin1String("exit")

using namespace RTComLogger;

ContactAuthorizer::ContactAuthorizer(const Tp::ConnectionPtr& connection,
                                     const Tp::AccountPtr& account,
                                     QObject *parent) :
    QObject(parent), m_connection(connection), m_account(account), m_requestTapped(false)
{
    qDebug() << Q_FUNC_INFO;

    connect(connection.data(), SIGNAL(invalidated(Tp::DBusProxy*,QString,QString)),
            this, SLOT(slotConnectionInvalidated(Tp::DBusProxy*,QString,QString)));

    if (connection->status() == Tp::ConnectionStatusConnected) {
        qDebug() << Q_FUNC_INFO << "Account's' connection status is CONNECTED!";
        if(connection->isReady(Tp::Connection::FeatureRoster)) {
            listenToAuthorization(connection);
        } else {
            qDebug() << Q_FUNC_INFO << "Asking FeatureRoster to be downloaded...";
            slotConnectionStatusChanged(connection->status());
        }
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "Account is not connected yet. Waiting to be connected...";
        connect(connection.data(), SIGNAL(statusChanged(Tp::ConnectionStatus)),
                this, SLOT(slotConnectionStatusChanged(Tp::ConnectionStatus)));
    }
}

ContactAuthorizer::~ContactAuthorizer()
{
    qDebug() << Q_FUNC_INFO;
}

void ContactAuthorizer::slotConnectionStatusChanged(Tp::ConnectionStatus connectionStatus)
{
    qDebug() << Q_FUNC_INFO << "Connection status changed to " << connectionStatus;

    if (connectionStatus == Tp::ConnectionStatusConnected) {
        qDebug() << Q_FUNC_INFO << "Asking FeatureRoster to be downloaded...";
        Tp::PendingOperation* po = m_connection->becomeReady(Tp::Connection::FeatureCore
                                | Tp::Connection::FeatureRoster);
        connect(po, SIGNAL(finished(Tp::PendingOperation*)),
                SLOT(slotConnectionFeaturesReady(Tp::PendingOperation*)));
    }
}

void ContactAuthorizer::slotConnectionFeaturesReady(Tp::PendingOperation* op)
{
    qDebug() << Q_FUNC_INFO;

    if(op && !op->isError() && !m_connection.isNull() && m_connection->isReady())
        listenToAuthorization(m_connection);
}

void ContactAuthorizer::listenToAuthorization(const Tp::ConnectionPtr& connection)
{
    qDebug() << Q_FUNC_INFO;

    m_pContactManager = connection->contactManager();

    if(m_pContactManager){
        // Connect to listen invitation requests:
        connect(m_pContactManager.data(),
                SIGNAL(presencePublicationRequested(const Tp::Contacts)),
                SLOT(slotPresencePublicationRequested(const Tp::Contacts &)),
                Qt::UniqueConnection);

        // Check if there are pending invitation requests in ContactManager's queue (like if invitation
        // request has been triggered during offline mode):
        Tp::Contacts contacts = m_pContactManager->allKnownContacts();
        if (!contacts.isEmpty())
            slotPresencePublicationRequested(contacts);
    }
}

void ContactAuthorizer::slotPresencePublicationRequested(const Tp::Contacts &contacts)
{
    qDebug() << Q_FUNC_INFO;

    Tp::Contacts pendingContacts;
    QList<MNotification*> notifications = MNotification::notifications();

    foreach(Tp::ContactPtr contact, contacts) {
        QString notificationIdentifier = contact->id() + m_account->objectPath();
        Tp::Contact::PresenceState state = contact->publishState();
        qDebug() << Q_FUNC_INFO << "Publish state for contact " << contact->id() << " is " << state;
        if (state == Tp::Contact::PresenceStateAsk) {
            qDebug() << Q_FUNC_INFO << "Contact " << contact->id() << " has requested an invitation.";

            // listen to the changes in case they are done somewhere else
            connect(contact.data(),
                    SIGNAL(publishStateChanged(Tp::Contact::PresenceState,const QString &)),
                    SLOT(slotPublishStateChanged(Tp::Contact::PresenceState,const QString &)),
                    Qt::UniqueConnection);

            bool notificationExists = false;
            // Invitation requests should not be shown if they already exist as notifications
            foreach (MNotification *n, notifications) {
               if (n && notificationIdentifier == n->identifier()) {
                   qDebug() << Q_FUNC_INFO << "Invitation request is already being shown as a notification.";
                   notificationExists = true;
                   break;
                }
            }

            if (!notificationExists)
                pendingContacts.insert(contact);
        }
    }

    qDeleteAll(notifications);
    notifications.clear();

    if(!pendingContacts.isEmpty()){
        if(m_pContactManager
           && m_pContactManager->supportedFeatures().contains(Tp::Contact::FeatureAvatarData)){
            qDebug() << Q_FUNC_INFO << "Adding auth. requests to pending queue to wait for avatar";
            upgradeContacts(pendingContacts);
            queueAuthorization(pendingContacts, m_authRequestWaitingForAvatar);
        } else {
            qDebug() << Q_FUNC_INFO << "Adding auth. requests to main queue";
            queueAuthorization(pendingContacts, m_authRequests);
        }
    }

    fireAuthorisationRequest();
}

void ContactAuthorizer::slotPublishStateChanged(Tp::Contact::PresenceState state,
                                                const QString &message)
{
    Q_UNUSED(message);

    Tp::Contact *contact = qobject_cast<Tp::Contact*>(sender());

    if (state != Tp::Contact::PresenceStateAsk && contact) {
        QList<MNotification*> notifications = MNotification::notifications();

        //remove from requests list
        Request r;
        r.contact = Tp::ContactPtr(contact);

        m_publishedAuthRequests.removeOne(r);
        m_authRequests.removeOne(r);
        m_authRequestWaitingForAvatar.removeOne(r);

        // invalidate current request, does not matter what user select locally, the contact is in or out already
        if (m_ongoingRequest ==  r)
            m_ongoingRequest = Request();

        //remove from notifications
        QString notificationIdentifier = contact->id() + m_account->objectPath();
        foreach (MNotification *n, notifications) {
            if (n && notificationIdentifier == n->identifier()) {
                if (!n->remove()) {
                    qWarning() << "Failed to remove notification";
                }
                break;
            }
        }

        qDeleteAll(notifications);
    }
}

void ContactAuthorizer::queueAuthorization(const Tp::Contacts contacts,
                                           AuthRequests& requestQueue)
{
    qDebug() << Q_FUNC_INFO;

    if(contacts.count() == 0)
        return;

    QSet<Tp::ContactPtr>::const_iterator i;
    for (i = contacts.constBegin(); i != contacts.constEnd(); ++i){
        qDebug() << Q_FUNC_INFO << "Queuing request";
        Request request;
        request.contact = *i;
        request.message = request.contact->publishStateMessage();
        if(!requestQueue.contains(request)){
            requestQueue.append(request);
        }
     }
}

void ContactAuthorizer::queueAuthorization(const Tp::ContactPtr& contact,
                                           const QString& avatarFile)
{
    qDebug() << Q_FUNC_INFO;

    Request tmpRequest;
    tmpRequest.contact = contact;

    if(m_authRequestWaitingForAvatar.contains(tmpRequest)) {
        // If this is a request that didn't already get an avatar, let's put it the list of
        // the pending requests and fire it.
        qDebug() << Q_FUNC_INFO << "adding request to pending requests queue to wait for an avatar";

        Request r = m_authRequestWaitingForAvatar.value
            (m_authRequestWaitingForAvatar.indexOf(tmpRequest));
        r.filename = avatarFile;

       if(!m_authRequests.contains(r)) {
            m_authRequests.append(r);
            m_authRequestWaitingForAvatar.removeOne(r);
            fireAuthorisationRequest();
       }
    } else if(m_authRequests.contains(tmpRequest)) {
        // This is a pending request that already got an avatar (someway or another) but has
        // not been published yet. Let's simply update it.
        qDebug() << Q_FUNC_INFO << "pending request already having avatar, not published yet, updating avatar to it";
        int index = m_authRequests.indexOf(tmpRequest);
        Request r = m_authRequests.value(index);
        r.filename = avatarFile;
        m_authRequests.replace(index,r);
    } else if(m_publishedAuthRequests.contains(tmpRequest)) {
        // This is a request that was already published. Let's change the avatar, update the
        // and hope the notification and hope the user hasn't tapped it yet :)
        qDebug() << Q_FUNC_INFO << "Already published request, but did not contain avatar.";
        int index = m_publishedAuthRequests.indexOf(tmpRequest);
        Request r = m_publishedAuthRequests.value(index);
        if (r.filename != avatarFile) {
            qDebug() << Q_FUNC_INFO << "Avatar file has really changed. Updating notification.";
        r.filename = avatarFile;
            m_publishedAuthRequests.replace(index,r);

            QList<MNotification*> notifications = MNotification::notifications();
            MNotification *notificationFromList = 0;
            foreach (MNotification *n, notifications) {
               if (n && r.notificationId == n->identifier()) {
                   notificationFromList = n;
                   break;
                }
            }

            if (notificationFromList && notificationFromList->isPublished()) {
                QList<QVariant> args;
                args.append(QVariant(r.contact->id()));
                args.append(QVariant(m_account->objectPath()));
                args.append(QVariant(r.filename));
                args.append(QVariant(r.message));
                args.append(QVariant(r.transactionId));
                args.append(QVariant(m_account->uniqueIdentifier()));

                MRemoteAction remoteAction(COMM_HISTORY_SERVICE_NAME,
                                           COMM_HISTORY_OBJECT_PATH,
                                           COMM_HISTORY_INTERFACE,
                                           ACTIVATE_AUTHIRIZATION_METHOD,
                                           args);

                notificationFromList->setAction(remoteAction);
                notificationFromList->publish();
                qDebug() << Q_FUNC_INFO << "Notification updated and re-published.";
            }

            qDeleteAll(notifications);
            notifications.clear();
        } // if (r.filename != avatarFile) {
    }
}

void ContactAuthorizer::upgradeContacts(const Tp::Contacts& contacts)
{
    qDebug() << Q_FUNC_INFO;

    if (!m_pContactManager)
        return;

    Tp::Features features;
    features << Tp::Contact::FeatureAlias
            << Tp::Contact::FeatureAvatarData
            << Tp::Contact::FeatureAvatarToken;

    Tp::PendingContacts * pc =
            m_pContactManager->upgradeContacts(contacts.toList(), features);

    foreach(Tp::ContactPtr i, contacts.toList()) {
        connect(i.data(),
                SIGNAL(avatarDataChanged(Tp::AvatarData)),
                SLOT(slotAvatarDataChanged(Tp::AvatarData)));
        connect(i.data(),
                SIGNAL(avatarTokenChanged(const QString&)),
                SLOT(slotAvatarTokenChanged(const QString&)));
    }

    connect(pc, SIGNAL(finished(Tp::PendingOperation*)),
            this, SLOT(slotContactsUpgraded(Tp::PendingOperation*)));
}

void ContactAuthorizer::slotContactsUpgraded(Tp::PendingOperation* op)
{
    qDebug() << Q_FUNC_INFO;

    Tp::PendingContacts * pc = qobject_cast<Tp::PendingContacts*>(op);
    if (pc == 0)
        return;

    // Handle those cases in which we have the avatar already, so that avatarDataChanged is never
    // emitted. Notice that the quthorization request won't be queued twice, because of the check
    // in queueAuthorization().
    QList<Tp::ContactPtr> contacts = pc->contacts();
    foreach(Tp::ContactPtr i, contacts) {
        Tp::Contact *c = i.data();
        Tp::AvatarData ad = c->avatarData();
        QString avatarToken = c->avatarToken();
        Q_UNUSED(avatarToken)
        queueAuthorization(i, ad.fileName);
    }

}

void ContactAuthorizer::slotAvatarDataChanged(const Tp::AvatarData &data)
{
    qDebug() << Q_FUNC_INFO;

    if (!m_pContactManager)
        return;

    Tp::Contact *contact = qobject_cast<Tp::Contact*>(sender());
    if (contact) {
        Tp::ContactPtr contactPtr (m_pContactManager->lookupContactByHandle(contact->handle().first()));
        queueAuthorization(contactPtr, data.fileName);
    }
}

void ContactAuthorizer::slotAvatarTokenChanged(const QString& avatarToken)
{
    Q_UNUSED(avatarToken)

    qDebug() << Q_FUNC_INFO;

    /*
    if (!m_pContactManager)
        return;

    Tp::Contact *contact = qobject_cast<Tp::Contact*>(sender());
    if (contact) {
        Tp::ContactPtr contactPtr
            (m_pContactManager->lookupContactByHandle(contact->handle().first()));
        disconnect(contact,
                   SIGNAL(avatarTokenChanged(const QString&)),
                   this,
                   SLOT(slotAvatarTokenChanged(const QString&)));
    }
    */
}

void ContactAuthorizer::fireAuthorisationRequest()
{
    qDebug() << Q_FUNC_INFO;

    if(!m_connection)
        return;

    foreach(Request request, m_authRequests) {
        QString id;
        QString filename;
        QString message;
        QString transactionId;

        if(!request.contact->id().isEmpty()){
            id = request.contact->id();
            filename = request.filename;
            message = request.message;
            transactionId = request.transactionId;
        } else {
            m_authRequests.removeOne(request);
            continue;
        }

        QList<QVariant> args;
        args.append(QVariant(id));
        args.append(QVariant(m_account->objectPath()));
        args.append(QVariant(filename));
        args.append(QVariant(message));
        args.append(QVariant(transactionId));
        args.append(QVariant(m_account->uniqueIdentifier()));

        MNotification notification(AuthorizationNotificationType,
                                   request.contact->alias().isEmpty() ?
                                       request.contact->id() :
                                       request.contact->alias(),
                                   txt_qtn_pers_authorization_req);

        MRemoteAction remoteAction(COMM_HISTORY_SERVICE_NAME,
                                   COMM_HISTORY_OBJECT_PATH,
                                   COMM_HISTORY_INTERFACE,
                                   ACTIVATE_AUTHIRIZATION_METHOD,
                                   args);

        notification.setAction(remoteAction);
        notification.setIdentifier(id + m_account->objectPath());
        notification.publish();
        request.notificationId = id + m_account->objectPath();
        qDebug() << Q_FUNC_INFO << "MNotification published with notification id: " << request.notificationId;
        m_authRequests.removeOne(request);
        if(!m_publishedAuthRequests.contains(request)){
            qDebug() << Q_FUNC_INFO << "Appending request to published auth requests queue";
            m_publishedAuthRequests.append(request);
        }
    }
}

void ContactAuthorizer::slotShowAuthorizationDialog(const QString& contactId,
                                                    const QString& accountPath,
                                                    const QString& filename,
                                                    const QString& message,
                                                    const QString& transactionId,
                                                    const QString& unused)
{
    Q_UNUSED(unused)

    qDebug() << Q_FUNC_INFO << "Authorization dialog asked to be shown for account: " << accountPath << " requester being contact id: " << contactId;
    qDebug() << Q_FUNC_INFO << "Contact's avatar filename: " << filename;

    if(m_account && m_account->objectPath() != accountPath) {
        qDebug() << Q_FUNC_INFO << "This ContactsAuthorizer does not serve the given account, but " << m_account->objectPath();
        return;
    }

    // Request tapped first time:
    if (!m_requestTapped)
        m_requestTapped = true;
    else
        return;

    bool requestFound = false;
    foreach(Request request, m_publishedAuthRequests) {
        // Remove requests that don't have a valid contact associated with them.
        // Why should it happen in the first place, by the way?
        if(request.contact->id().isEmpty()) {
           m_publishedAuthRequests.removeOne(request);
           continue;
        }

        if(request.contact->id() == contactId) {
            // We found a request amongst the ones we were keeping track of, i.e. one that was
            // generating during the same session (not before a reboot).
            // This means we can start the UI right away.
            requestFound = true;
            qDebug() << Q_FUNC_INFO << "Found published auth request with values:";
            qDebug() << Q_FUNC_INFO << "notification id: " << request.notificationId;
            qDebug() << Q_FUNC_INFO << "contact id: " << request.contact->id();
            qDebug() << Q_FUNC_INFO << "avatar filename: " << request.filename;
            startBuddyAuthorizerUI(request);
            break;
        }
    }

    // If we couldn't find a request, it means that the user has tapped on an authorization
    // request that was issued before a reboot, so we have to create the request again and
    // find the contact.
    if(!requestFound && m_pContactManager) {
        Request req;
        Tp::PendingContacts *pendingContacts;
        QVariant reqVariant;

        req.notificationId = contactId + accountPath;
        req.filename = filename;
        req.message = message;
        req.transactionId = transactionId;

        reqVariant.setValue<Request>(req);
        pendingContacts = m_pContactManager->contactsForIdentifiers(QStringList() << contactId);
        pendingContacts->setProperty("request", reqVariant);
        connect(pendingContacts,
                SIGNAL(finished(Tp::PendingOperation*)),
                this,
                SLOT(slotRequestContactsReady(Tp::PendingOperation*)));
    }
}

void ContactAuthorizer::slotDialogDismissed(const QString& dialogId, int result)
{
    qDebug() << Q_FUNC_INFO;

    if(m_ongoingRequest.isValid() && m_ongoingRequest.transactionId == dialogId) {
        QDBusConnection::sessionBus().disconnect(BUDDY_AUTHORIZER_SERVICE_NAME,
                                                 OBJECT_PATH,
                                                 BUDDY_AUTHORIZER_INTERFACE_NAME,
                                                 BUDDY_AUTHORIZER_UI_SIGNAL,
                                                 this,
                                                 SLOT(slotDialogDismissed(QString,int)));

        qDebug() << "DIALOG DISMISSED WITH RESULT = " << result;
        switch (result) {
        case QMessageBox::Yes:
            authorizeContact(m_ongoingRequest.contact);
            break;
        case QMessageBox::No:
            blockContact(m_ongoingRequest.contact);
            break;
        case QMessageBox::Ignore:
            removeNotificationForOngoingRequest();
            m_ongoingRequest = Request();
            break;
        default:
            m_ongoingRequest = Request();
        }

        m_requestTapped = false;
    }
}

void ContactAuthorizer::startBuddyAuthorizerUI(class Request& request)
{
    qDebug() << Q_FUNC_INFO;


    QDBusInterface interface(BUDDY_AUTHORIZER_SERVICE_NAME,
                             OBJECT_PATH,
                             BUDDY_AUTHORIZER_INTERFACE_NAME);

    QUuid uid = QUuid::createUuid();
    m_ongoingRequest = request;
    // We update the transactionId each time we start the authorization UI, because we associate it
    // with the dialog.
    m_ongoingRequest.transactionId = uid;

    QDBusPendingCall call = interface.asyncCall(BUDDY_AUTHORIZER_UI_METHOD,
                                                QVariant(uid),
                                                QVariant(request.filename),
                                                QVariant(request.contact->id()),
                                                QVariant(request.contact->alias()),
                                                QVariant(request.message),
                                                QVariant(m_account->objectPath()));
    QDBusPendingCallWatcher* watcher = new QDBusPendingCallWatcher(call,this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(slotAuthoriserQueryFinished(QDBusPendingCallWatcher*)));
}

void ContactAuthorizer::slotAuthoriserQueryFinished(QDBusPendingCallWatcher* watcher)
{
    qDebug() << Q_FUNC_INFO;

    if(m_ongoingRequest.isValid() &&  watcher){

        QDBusPendingReply<> reply = *watcher;

         if (reply.isError()) {
             qDebug() << Q_FUNC_INFO << reply.error() << reply.reply();
             watcher->deleteLater();
             Request empty;
             m_ongoingRequest = empty;
             m_requestTapped = false;
         } else if(reply.isFinished()){
             QDBusConnection::sessionBus().connect(BUDDY_AUTHORIZER_SERVICE_NAME,
                                                   OBJECT_PATH,
                                                   BUDDY_AUTHORIZER_INTERFACE_NAME,
                                                   BUDDY_AUTHORIZER_UI_SIGNAL,
                                                   this,
                                                   SLOT(slotDialogDismissed(QString,int)));
             watcher->deleteLater();
         } else {
            qDebug() << Q_FUNC_INFO << "Reply is not finished";
         }
    }
}

void ContactAuthorizer::authorizeContact(const Tp::ContactPtr& contact)
{
    qDebug() << Q_FUNC_INFO;

    if(m_pContactManager && contact) {
        Tp::PendingOperation* po = m_pContactManager->authorizePresencePublication(
                QList<Tp::ContactPtr>() << contact);
        connect(po, SIGNAL(finished(Tp::PendingOperation*)),
                this, SLOT(slotAuthorizationAccepted(Tp::PendingOperation*)));
    }
}

void ContactAuthorizer::slotAuthorizationAccepted(Tp::PendingOperation *operation)
{
    qDebug() << Q_FUNC_INFO;

    if (operation && operation->isError()) {
        qWarning() << "Unable to accept authorization request: " << operation->errorMessage();
        return;
    }

    removeNotificationForOngoingRequest();
    m_ongoingRequest = Request();
}

void ContactAuthorizer::blockContact(const Tp::ContactPtr& contact)
{
    qDebug() << Q_FUNC_INFO;

    if(m_pContactManager && contact) {
        Tp::PendingOperation *po = m_pContactManager->blockContacts
            (QList<Tp::ContactPtr>() << contact);
        connect(po, SIGNAL(finished(Tp::PendingOperation*)),
                this, SLOT(slotContactBlocked(Tp::PendingOperation*)));
    }
}

void ContactAuthorizer::slotContactBlocked(Tp::PendingOperation *operation)
{
    if (operation && operation->isError()) {
        qWarning() << "Unable to block contact: " << operation->errorMessage();
        return;
    }

    removeNotificationForOngoingRequest();
    m_ongoingRequest = Request();
}

void ContactAuthorizer::removeNotificationForOngoingRequest()
{
    qDebug() << Q_FUNC_INFO;

    QList<MNotification*> notifications = MNotification::notifications();
    foreach (MNotification *n, notifications) {
       if (n && m_ongoingRequest.notificationId == n->identifier()) {
           if (!n->remove()) {
               qWarning() << "Failed to remove notification.";
               return;
           }
        }
    }
    qDeleteAll(notifications);
    notifications.clear();

    // Let's also remove the request from the list of published ones.
    m_publishedAuthRequests.removeOne(m_ongoingRequest);
    m_ongoingRequest = Request();
}

void ContactAuthorizer::slotConnectionInvalidated(Tp::DBusProxy*,
                                                  const QString&,
                                                  const QString&)
{
    qDebug() << Q_FUNC_INFO;

    deleteLater();

    // close ui if it is open
    if(m_ongoingRequest.isValid()){
        QDBusInterface interface(BUDDY_AUTHORIZER_APP_NAME,
                                 OBJECT_PATH,
                                 BUDDY_AUTHORIZER_APP_IF);
        interface.asyncCall(BUDDY_AUTHORIZER_APP_EXIT);
    }
}

void ContactAuthorizer::slotRequestContactsReady(Tp::PendingOperation *operation)
{
    // We received that contact we asked for, when dealing with an authorization request that
    // was issued before a reboot. Now we can put the contact in the request and continue.

    qDebug() << Q_FUNC_INFO;

    if (operation && operation->isError()) {
        qWarning() << "No contacts" << operation->errorMessage();
        return;
    }

    Tp::PendingContacts* pendingContacts = static_cast<Tp::PendingContacts*>(operation);
    if (pendingContacts) {
        QList<Tp::ContactPtr> contacts(pendingContacts->contacts());

        if (contacts.count() == 1) {
            Tp::ContactPtr targetContact = contacts.value(0);
            QVariant var = operation->property("request");
            Request req = var.value<Request>();
            req.contact = targetContact;

            // By the way, let's now add the request to the list of published ones, so we don't
            // have to go through this again, in case the user taps on the same authorization
            // request again after ignoring the one we're dealing with now.
            m_publishedAuthRequests.append(req);

            // We can now finally start the UI.
            startBuddyAuthorizerUI(req);
        } else {
            qCritical() << Q_FUNC_INFO << "Invalid contacts list";
        }
    } else {
        qCritical() << Q_FUNC_INFO << "Invalid TpPendingOperation when requesting target contact";
    }
}

