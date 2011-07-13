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

#include <CommHistory/EventModel>
#include <CommHistory/TrackerIO>

#include <TpExtensions/Connection> // stored messages if

#include "constants.h"
#include "messagereviver.h"
#include "connectionutils.h"

using namespace RTComLogger;
using namespace CommHistory;

#define STORED_MESSAGES_CHECK_INTERVAL 30000 //msec
#define MAX_RETRIES 10

MessageReviver::MessageReviver(ConnectionUtils *connectionUtils,
                               QObject *parent) :
    QObject(parent)
{
    connect(connectionUtils,
            SIGNAL(connectionReady(Tp::ConnectionPtr)),
            SLOT(checkConnection(Tp::ConnectionPtr)));
}

void MessageReviver::checkConnection(const Tp::ConnectionPtr& connection)
{
    if (!connection.isNull()
        && connection->isValid()
        && connection->hasInterface(CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface::staticInterfaceName())
        && !isConnectionHandled(connection)
        && m_Retries[connection->objectPath()] < MAX_RETRIES) {
        fetchMessages(connection);
        m_Retries[connection->objectPath()] = m_Retries[connection->objectPath()] + 1;
    }
}

bool MessageReviver::isConnectionHandled(const Tp::ConnectionPtr &connection)
{
    return m_Connections.key(connection) != 0
           || m_TimerConnections.key(connection) != 0;
}

void MessageReviver::fetchMessages(const Tp::ConnectionPtr &connection)
{
    Tp::Client::DBus::PropertiesInterface *props = connection->interface<Tp::Client::DBus::PropertiesInterface>();

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher
        (props->Get(CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface::staticInterfaceName(),
                    QLatin1String("StoredMessages")),
         this);

    connect(watcher,
            SIGNAL(finished(QDBusPendingCallWatcher*)),
            SLOT(onGetStoredMessages(QDBusPendingCallWatcher*)));
    m_Connections.insert(watcher, connection);
}

void MessageReviver::onGetStoredMessages(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<QDBusVariant> reply = *call;
    Tp::ConnectionPtr connection = m_Connections.take(call);

    if (reply.isError()) {
        qWarning() << reply.error().name() << "-" << reply.error().message();
        call->deleteLater();
        return;
    }

    updateTokens(reply.value().variant().toStringList(), connection);

    call->deleteLater();
}

void MessageReviver::updateTokens(const QStringList &tokens,
                                  Tp::ConnectionPtr &connection)
{
    QSet<QString> currentTokens = tokens.toSet();
    QSet<QString> initialTokens = m_MessageTokens.take(connection->objectPath());
    bool modified = false;

    if (connection.isNull() || !connection->isValid()) {
        qDebug() << "Connection is not valid anymore, abort";
        return;
    }

    if (initialTokens.isEmpty()) { // it's empty
        // only if m_MessageTokens didn't have it
        initialTokens = currentTokens;
        modified = true;
    } else {
        int setSize = initialTokens.size();
        initialTokens.intersect(currentTokens);
        modified = (setSize != initialTokens.size());
    }

    if (modified) {
        if (!initialTokens.isEmpty()) {
            m_MessageTokens.insert(connection->objectPath(), initialTokens);

            int timerId = startTimer(STORED_MESSAGES_CHECK_INTERVAL);
            if (timerId > 0) {
                m_TimerConnections.insert(timerId, connection);
            } else {
                qWarning() << "Failed to start timer";
            }
        }
    } else if (!initialTokens.isEmpty()) {
        m_MessageTokens.insert(connection->objectPath(), initialTokens);
        // get tp-glib connection to finally expunge/deliver messages
        handleMessages(connection);
    }
}

void MessageReviver::timerEvent(QTimerEvent *event)
{
    Tp::ConnectionPtr connection = m_TimerConnections.take(event->timerId());

    if (!connection.isNull() && connection->isValid())
        fetchMessages(connection);

    killTimer(event->timerId());
}

void MessageReviver::handleMessages(Tp::ConnectionPtr &connection)
{
    QStringList toRevive;
    QStringList toBury;

    EventModel model;
    QSet<QString> messageTokens = m_MessageTokens.take(connection->objectPath());

    foreach (QString token, messageTokens) {
        Event event;
        if (model.trackerIO().getEventByMessageToken(token, event)) {
            qDebug() << "bury " << token;
            toBury << token;
        } else {
            qDebug() << "revive " << token;
            toRevive << token;
        }
    }

    CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface* storedMessages =
            connection->interface<CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface>();

    if (storedMessages) {
        if (!toBury.isEmpty())
            storedMessages->ExpungeMessages(toBury);

        if (!toRevive.isEmpty())
            storedMessages->DeliverStoredMessages(toRevive);
    } else {
        qCritical() << Q_FUNC_INFO << "No StoredMessage if";
    }
}

