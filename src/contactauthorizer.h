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

#ifndef CONTACTAUTHORIZER_H
#define CONTACTAUTHORIZER_H

// Qt
#include <QObject>
#include <QHash>

// Tp
#include <TelepathyQt4/Account>
#include <TelepathyQt4/Contact>
#include <TelepathyQt4/Connection>
#include <TelepathyQt4/ContactManager>

class MNotification;

namespace RTComLogger
{

class Request {
public:
    bool operator==(const Request& other) const
    {
        if(contact && other.contact
           && !contact->handle().isEmpty() && !other.contact->handle().isEmpty()
           && contact->handle().first() == other.contact->handle().first()){
            return true;
        } else {
            return false;
        }
    }

    Request& operator=(const Request& other){
        if (this != &other) {
            notificationId = other.notificationId;
            contact = other.contact;
            message = other.message;
            filename = other.filename;
            transactionId = other.transactionId;
        }
        return *this;
    }

    bool isValid(){return !contact.isNull();}

    QString notificationId;
    Tp::ContactPtr contact;
    QString message;
    QString filename;
    QUuid transactionId;
};

typedef QList<Request> AuthRequests;

class ContactAuthorizer : public QObject
{
    Q_OBJECT

public:
    explicit ContactAuthorizer(const Tp::ConnectionPtr& connection,
                               const Tp::AccountPtr& account,
                               QObject *parent = 0);
    virtual ~ContactAuthorizer();

private Q_SLOTS:
    void slotShowAuthorizationDialog(const QString& contactId,
                                     const QString& accountPath,
                                     const QString& filename,
                                     const QString& message,
                                     const QString& transactionId,
                                     const QString& unused);
    void slotPresencePublicationRequested(const Tp::Contacts &contacts);
    void slotContactsUpgraded(Tp::PendingOperation *);
    void slotConnectionInvalidated(Tp::DBusProxy*,const QString&, const QString&);
    void slotConnectionFeaturesReady(Tp::PendingOperation* op);
    void slotAuthoriserQueryFinished(QDBusPendingCallWatcher*);
    void slotDialogDismissed(const QString& dialogId, int result);
    void slotAvatarDataChanged(const Tp::AvatarData &);
    void slotAvatarTokenChanged(const QString& avatarToken);
    void slotRequestContactsReady(Tp::PendingOperation* operation);
    void slotAuthorizationAccepted(Tp::PendingOperation* operation);
    void slotContactBlocked(Tp::PendingOperation* operation);
    void slotConnectionStatusChanged(Tp::ConnectionStatus connectionStatus);
    void slotPublishStateChanged(Tp::Contact::PresenceState state,
                                 const QString &message);

private:
    void listenToAuthorization(const Tp::ConnectionPtr& connection);
    void requestAvatar(const Tp::ContactPtr& contact);
    void queueAuthorization(const Tp::Contacts contacts,
                            AuthRequests& requestQueue);
    void queueAuthorization(const Tp::ContactPtr& contact,
                            const QString& avatarFile);
    void upgradeContacts(const Tp::Contacts& contacts);
    void fireAuthorisationRequest();
    void startBuddyAuthorizerUI(class Request& request);
    void authorizeContact(const Tp::ContactPtr& contact);
    void blockContact(const Tp::ContactPtr& contact);
    void removeNotificationForOngoingRequest();

private:
    Tp::ConnectionPtr m_connection;
    Tp::AccountPtr m_account;
    Tp::ContactManagerPtr m_pContactManager;
    // ready to be fired auth., requests
    AuthRequests m_authRequests;
    // Published auth requests
    AuthRequests m_publishedAuthRequests;
    // some information is still being fetched
    AuthRequests m_authRequestWaitingForAvatar;
    // request that is handled at the moment
    Request m_ongoingRequest;

    bool m_requestTapped;
};

} // namespace RTComLogger

Q_DECLARE_METATYPE(RTComLogger::Request)

#endif // CONTACTAUTHORIZER_H
