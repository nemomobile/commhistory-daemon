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

#ifndef MESSAGE_REVIVER_H
#define MESSAGE_REVIVER_H

#include <QObject>
#include <TelepathyQt4/Connection>

namespace CommHistory {
    class EventModel;
}

namespace RTComLogger
{
class ConnectionUtils;

/*!
 * \class MessageReviver
 * \brief class responsible for checking any unhandled messages and redelivering them
 *  using stored messages interface
 *  Once all stored messages are processed, the object will delete itself.
 */
class MessageReviver : public QObject
{
    Q_OBJECT

public:
    explicit MessageReviver(ConnectionUtils *connectionUtils,
                            QObject *parent = NULL);

public Q_SLOTS:
    void checkConnection(const Tp::ConnectionPtr& connection);

private Q_SLOTS:
    void onGetStoredMessages(QDBusPendingCallWatcher *call);
private:
    void updateTokens(const QStringList &tokens, Tp::ConnectionPtr &connection);
    void fetchMessages(const Tp::ConnectionPtr &connection);
    void timerEvent(QTimerEvent *event);
    void handleMessages(Tp::ConnectionPtr &connection);
    bool isConnectionHandled(const Tp::ConnectionPtr &connection);

protected:
    // keep connections while fetching stored messages
    QHash<QDBusPendingCallWatcher*, Tp::ConnectionPtr> m_Connections;
    // keep connections while waiting timer time outs
    QHash<int, Tp::ConnectionPtr> m_TimerConnections;
    QHash<QString, QSet<QString> > m_MessageTokens;

    QHash<QString,int> m_Retries;

#ifdef UNIT_TEST
    friend class Ut_MessageReviver;
#endif
};

} // namespace RTComLogger

#endif // MESSAGE_REVIVER_H
