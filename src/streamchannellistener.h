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

#ifndef STREAM_CHANNEL_LISTENER_H
#define STREAM_CHANNEL_LISTENER_H

#include <QDateTime>
#include <QTimerEvent>
#include <TelepathyQt4/Types>
#include <time.h>
#include "channellistener.h"

namespace CommHistory {
    class Event;
}

namespace RTComLogger
{

/*!
 * \class StreamChannelListener
 * \brief class responsible for listening and logging activity on a stream
 * channel, i.e. calls
 */
class StreamChannelListener : public ChannelListener
{
    Q_OBJECT

public:
    explicit StreamChannelListener(const Tp::AccountPtr &account,
                          const Tp::ChannelPtr &channel,
                          const Tp::MethodInvocationContextPtr<> &context,
                          QObject *parent = 0);

    virtual ~StreamChannelListener();

private Q_SLOTS:
    void slotGroupMembersChanged(
            const Tp::Contacts &groupMembersAdded,
            const Tp::Contacts &groupLocalPendingMembersAdded,
            const Tp::Contacts &groupRemotePendingMembersAdded,
            const Tp::Contacts &groupMembersRemoved,
            const Tp::Channel::GroupMemberChangeDetails &details);
    void slotStreamStateChanged(const Tp::StreamedMediaStreamPtr &stream,
                                Tp::MediaStreamState state);
    virtual void invalidated(Tp::DBusProxy *proxy,
        const QString &errorName, const QString &errorMessage);
    void slotServicePointChanged(const Tp::ServicePoint &servicePoint);
    void slotEventsCommitted(QList<CommHistory::Event> events, bool successful);

private:
    void channelReady();
    bool addEvent();
    void channelListenerReady();
    void callStarted();
    void callEnded();
    void timerEvent(QTimerEvent *event);

private:
    bool m_CallStarted;
    bool m_CallEnded;
    time_t m_callStartTime;
    CommHistory::Event m_Event;
    bool m_EventAdded;
    int m_LoggingTimerId;

    bool m_eventCommitted;
    Tp::DBusProxy *m_pProxy;
    QString m_errorName;
    QString m_errorMessage;
};

} // namespace RTComLogger

#endif // CHANNEL_LISTENER_H
