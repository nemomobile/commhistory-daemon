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

#include "streamchannellistener.h"
#include "notificationmanager.h"

// Qt
#include <QDebug>

// libcommhistory
#include <CommHistory/EventModel>
#include <CommHistory/Event>

// TpQt4
#include <TelepathyQt4/StreamedMediaChannel>

#define CURRENT_SERVICE_POINT_PROPERTY_NAME ("CurrentServicePoint")
#define INITIAL_SERVICE_POINT_PROPERTY QLatin1String(TELEPATHY_INTERFACE_CHANNEL_INTERFACE_SERVICE_POINT ".InitialServicePoint")

#define SAVING_INTERVAL 5*60000 // 5 minute

using namespace RTComLogger;

StreamChannelListener::StreamChannelListener(const Tp::AccountPtr &account,
                                             const Tp::ChannelPtr &channel,
                                             const Tp::MethodInvocationContextPtr<> &context,
                                             QObject *parent)
    : ChannelListener(account, channel, context, parent),
      m_CallStarted(false),
      m_CallEnded(false),
      m_callStartTime(0),
      m_EventAdded(false),
      m_LoggingTimerId(0),
      m_eventCommitted(false),
      m_pProxy(0)
{
    qDebug() << __PRETTY_FUNCTION__;

    invocationContextFinished();

    makeChannelReady(Tp::StreamedMediaChannel::FeatureStreams);

    connect(&(eventModel()), SIGNAL(eventsCommitted(QList<CommHistory::Event>, bool)),
            this, SLOT(slotEventsCommitted(QList<CommHistory::Event>, bool)));

    m_Event.setStartTime(QDateTime::currentDateTime());
    m_Event.setEndTime(m_Event.startTime());
    m_Event.setType(CommHistory::Event::CallEvent);
    if (m_Account)
        m_Event.setLocalUid(m_Account->objectPath());
    m_Event.setRemoteUid(targetId());

    m_Direction = CommHistory::Event::Inbound;

    QVariantMap properties = m_Channel->immutableProperties();
    if (properties.contains(QLatin1String(TELEPATHY_INTERFACE_CHANNEL ".Requested"))) {
        if (properties.value(QLatin1String(TELEPATHY_INTERFACE_CHANNEL ".Requested")).toBool())
            m_Direction = CommHistory::Event::Outbound;
    }
    m_Event.setDirection(m_Direction);

    QVariant spProp = m_Channel->immutableProperties().value(INITIAL_SERVICE_POINT_PROPERTY);
    if (spProp.isValid()) {
        const Tp::ServicePoint sp = qdbus_cast<Tp::ServicePoint>(spProp);
        if (sp.servicePointType == Tp::ServicePointTypeEmergency) {
            qDebug() << Q_FUNC_INFO << "*** EMERGENCY CALL, service =" << sp.service;
            m_Event.setIsEmergencyCall(true);
        }
    }

    // postpone adding event for incoming event to speed up call handling
    if (m_Direction == CommHistory::Event::Outbound && !addEvent())
        qWarning() << Q_FUNC_INFO << "failed to add event";
}

StreamChannelListener::~StreamChannelListener()
{
}

void StreamChannelListener::callStarted()
{
    qDebug() << Q_FUNC_INFO;

    if (m_CallStarted)
        return;

    m_CallStarted = true;
    m_CallEnded = false;
    m_Event.setStartTime(QDateTime::currentDateTime());
    m_Event.setEndTime(m_Event.startTime());

    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    m_callStartTime = tp.tv_sec;

    qDebug() << Q_FUNC_INFO << m_Event.startTime();

    if (addEvent())
        m_LoggingTimerId = startTimer(SAVING_INTERVAL);
}

void StreamChannelListener::callEnded()
{
    qDebug() << Q_FUNC_INFO;

    m_CallEnded = true;

    struct timespec tp;
    QDateTime endTime;
    if (clock_gettime(CLOCK_MONOTONIC, &tp) != -1) {
        int duration = tp.tv_sec - m_callStartTime;
        endTime = m_Event.startTime().addSecs(duration);
    } else {
        endTime = QDateTime::currentDateTime();
    }

    m_Event.setEndTime(endTime);

    if (m_LoggingTimerId > 0) {
        killTimer(m_LoggingTimerId);
        m_LoggingTimerId = 0;
    }
}

void StreamChannelListener::channelReady()
{
    qDebug() << __PRETTY_FUNCTION__;

    Tp::StreamedMediaChannelPtr mediaChannel = Tp::StreamedMediaChannelPtr::dynamicCast(m_Channel);

    if (mediaChannel) {
        connect(mediaChannel.data(),
                SIGNAL(groupMembersChanged(const Tp::Contacts&,
                                           const Tp::Contacts&,
                                           const Tp::Contacts&,
                                           const Tp::Contacts&,
                                           const Tp::Channel::GroupMemberChangeDetails&)),
                 this,
                 SLOT(slotGroupMembersChanged(const Tp::Contacts&,
                                            const Tp::Contacts&,
                                            const Tp::Contacts&,
                                            const Tp::Contacts&,
                                            const Tp::Channel::GroupMemberChangeDetails&)));
        connect(mediaChannel.data(),
                SIGNAL(streamStateChanged(const Tp::StreamedMediaStreamPtr &, Tp::MediaStreamState)),
                this,
                SLOT(slotStreamStateChanged(const Tp::StreamedMediaStreamPtr &, Tp::MediaStreamState)));

        if (!m_Event.isEmergencyCall()) {
            Tp::Client::ChannelInterfaceServicePointInterface *servicePointIf =
                    mediaChannel->optionalInterface<Tp::Client::ChannelInterfaceServicePointInterface>();
            if (servicePointIf) {
                connect(servicePointIf, SIGNAL(ServicePointChanged(const Tp::ServicePoint &)),
                        this, SLOT(slotServicePointChanged(const Tp::ServicePoint &)));

                const Tp::ServicePoint sp = servicePointIf->property(
                        CURRENT_SERVICE_POINT_PROPERTY_NAME).value<Tp::ServicePoint>();
                if (sp.servicePointType == Tp::ServicePointTypeEmergency) {
                    qDebug() << Q_FUNC_INFO << "*** EMERGENCY CALL, service =" << sp.service;
                    m_Event.setIsEmergencyCall(true);
                }
            }
        }
    } else {
        qCritical() << Q_FUNC_INFO << "Wrong channel - Null";
    }
}

void StreamChannelListener::slotGroupMembersChanged(
                            const Tp::Contacts &groupMembersAdded,
                            const Tp::Contacts &groupLocalPendingMembersAdded,
                            const Tp::Contacts &groupRemotePendingMembersAdded,
                            const Tp::Contacts &groupMembersRemoved,
                            const Tp::Channel::GroupMemberChangeDetails &details)
{
    Q_UNUSED(groupLocalPendingMembersAdded)
    Q_UNUSED(groupRemotePendingMembersAdded)
    Q_UNUSED(details)

    qDebug() << __PRETTY_FUNCTION__;

    Tp::StreamedMediaChannelPtr mediaChannel = Tp::StreamedMediaChannelPtr::dynamicCast(m_Channel);

    if(!mediaChannel.isNull()) {
        if (!m_CallStarted) {
            // call not started, new member added, members > 1
            if (!groupMembersAdded.isEmpty() &&
                mediaChannel->groupContacts().count() >= 2) {
                Tp::StreamedMediaStreams streams = mediaChannel->streamsForType(Tp::MediaStreamTypeAudio);

                if (streams.count()
                    && streams.first()->localSendingState() == Tp::StreamedMediaStream::SendingStateSending) {
                    callStarted();
                }

            // if call is not started and nobody was added to channel and
            // number if contacts less than 2 and someone was removed, call
            // is rejected (when actor == self) or missed.
            } else if (groupMembersAdded.isEmpty() &&
                       mediaChannel->groupContacts().count() < 2 &&
                       !groupMembersRemoved.isEmpty()) {

                const Tp::ContactPtr self(mediaChannel->groupSelfContact());
                bool locallyMissed = (details.actor() == self &&
                                      details.reason() == Tp::ChannelGroupChangeReasonNoAnswer &&
                                      groupMembersRemoved.contains(self));
                bool remotelyMissed = (details.actor() != self);

                if ( (locallyMissed || remotelyMissed)
                    && m_Direction == CommHistory::Event::Inbound) {
                    qDebug() << "call missed";
                    m_Event.setIsMissedCall(true);
                }

                m_CallEnded = false;
                m_CallStarted = false;
            }

        // call already started, if not ended and someone was removed from channel
        // and number of contacts on channel is less than 2, call ended.
        } else if (!m_CallEnded && !groupMembersRemoved.isEmpty()) {
            if (mediaChannel->groupContacts().count() < 2)
                callEnded();
        }
    } else {
        qCritical() << "StreamChannelListener::onGroupMembersChanged() channel is null!";
    }
}

void StreamChannelListener::slotStreamStateChanged(const Tp::StreamedMediaStreamPtr &stream,
                                                   Tp::MediaStreamState state)
{
    if (stream->type() == Tp::MediaStreamTypeVideo) {
        bool modified = false;

        if (state == Tp::MediaStreamStateConnected) {
            m_Event.setIsVideoCall(true);
            modified = true;
        } else if (state == Tp::MediaStreamStateDisconnected
                   && !m_CallEnded) {
            m_Event.setIsVideoCall(false);
            modified = true;
        }

        if (modified && m_EventAdded) {
            // already added -> addEvent() will modify
            addEvent();
        }
    }
    if (stream->type() == Tp::MediaStreamTypeAudio
        && state == Tp::MediaStreamStateConnected) {
        Tp::StreamedMediaChannelPtr mediaChannel =
            Tp::StreamedMediaChannelPtr::dynamicCast(m_Channel);
        if (!mediaChannel.isNull() && mediaChannel->groupContacts().count() >= 2)
            callStarted();
    }
}

void StreamChannelListener::invalidated(Tp::DBusProxy *proxy,
            const QString &errorName, const QString &errorMessage)
{
    qDebug() << __PRETTY_FUNCTION__ << errorName << errorMessage;

    QDateTime currentTime = QDateTime::currentDateTime();

    // ensure correct time for missed calls
    if (!m_Event.startTime().isValid())
        m_Event.setStartTime(currentTime);

    if (!m_CallEnded) {
        if (m_CallStarted)
            // call started but aborted in the middle
            m_Event.setEndTime(currentTime);
        else
            // not started (and ended) call should have 0 duration
            m_Event.setEndTime(m_Event.startTime());
    }

    // catch connection manager crashes and other weird events
    // FIXME: this may not cover all cases(?). Error.Cancelled usually
    // means "hung up by local user", but if the remote hangs up before
    // we get MembersChanged, tpqt probably returns the same error.
    if ( (m_Direction == CommHistory::Event::Inbound) && (!m_CallStarted)) {
        if (!errorName.isEmpty() && errorName != TELEPATHY_ERROR_CANCELLED)
            m_Event.setIsMissedCall(true);
    }

    if (addEvent() && m_Event.isMissedCall()) {
        NotificationManager* nManager = NotificationManager::instance();
        nManager->showNotification(m_Event);
    }

    // don't quit and destroy the event model before the final call
    // event has been saved (and the corresponding eventsUpdated()
    // signal emitted)
    if (m_eventCommitted) {
        ChannelListener::invalidated(proxy,errorName,errorMessage);
    } else {
        m_pProxy = proxy;
        m_errorName = errorName;
        m_errorMessage = errorMessage;
    }
}

void StreamChannelListener::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_LoggingTimerId && m_EventAdded) {
        m_Event.setEndTime(QDateTime::currentDateTime());
        m_eventCommitted = false;
        eventModel().modifyEvent(m_Event);
    }
}

bool StreamChannelListener::addEvent()
{
    qDebug() << __PRETTY_FUNCTION__;

    bool result = false;

    if (m_EventAdded) {
        m_eventCommitted = false;
        result = eventModel().modifyEvent(m_Event);
    } else {
        result = eventModel().addEvent(m_Event);
        m_EventAdded = result;
    }

    if (result == false) {
        qCritical() << "failed to add event";
    }

    return result;
}

void StreamChannelListener::slotServicePointChanged(const Tp::ServicePoint &servicePoint)
{
    qDebug() << Q_FUNC_INFO;

    if (servicePoint.servicePointType == Tp::ServicePointTypeEmergency) {
        qDebug() << Q_FUNC_INFO << "*** EMERGENCY CALL, service =" << servicePoint.service;
        m_Event.setIsEmergencyCall(true);
    }
}

void StreamChannelListener::slotEventsCommitted(QList<CommHistory::Event> events, bool successful)
{
    Q_UNUSED(events);
    Q_UNUSED(successful);

    qDebug() << Q_FUNC_INFO;

    if (m_pProxy) {
        ChannelListener::invalidated(m_pProxy, m_errorName, m_errorMessage);
    } else {
        m_eventCommitted = true;
    }
}
