/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2014-2015 Jolla Ltd.
** Contact: Slava Monich <slava.monich@jolla.com>
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

#include "mmshandler.h"
#include "mmspart.h"
#include "constants.h"
#include "notificationmanager.h"
#include "debug.h"
#include <CommHistory/Event>
#include <CommHistory/EventModel>
#include <CommHistory/SingleEventModel>
#include <CommHistory/ConversationModel>
#include <CommHistory/commonutils.h>
#include <CommHistory/groupmanager.h>
#include <CommHistory/constants.h>
#include <CommHistory/mmsconstants.h>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <contextproperty.h>
#include <mgconfitem.h>
#include <unistd.h>

using namespace RTComLogger;
using namespace CommHistory;

#ifdef DEBUG_COMMHISTORY
#  define DEBUG_(x) qDebug() << "MmsHandler:" << x
#else
#  define DEBUG_(x) ((void)0)
#endif

MmsHandler::MmsHandler(QObject* parent)
    : MessageHandlerBase(parent, MMS_HANDLER_PATH, MMS_HANDLER_SERVICE)
    , m_cellularStatusProperty(new ContextProperty("Cellular.Status", this))
    , m_roamingAllowedProperty(new ContextProperty("Cellular.DataRoamingAllowed", this))
    , m_subscriberIdentityProperty(new ContextProperty("Cellular.SubscriberIdentity", this))
    , m_sendMessageFlags(NULL)
    , m_automaticDownload(NULL)
    , m_sendReadReports(NULL)
{
    qDBusRegisterMetaType<MmsPart>();
    qDBusRegisterMetaType<MmsPartList>();
    qDBusRegisterMetaType<QList<CommHistory::Event> >();

    connect(m_cellularStatusProperty, SIGNAL(valueChanged()), SLOT(onDataProhibitedChanged()));
    connect(m_roamingAllowedProperty, SIGNAL(valueChanged()), SLOT(onDataProhibitedChanged()));
    connect(m_subscriberIdentityProperty, SIGNAL(valueChanged()), SLOT(onSubscriberIdentityChanged()));
    onSubscriberIdentityChanged();

    QDBusConnection dbus(QDBusConnection::sessionBus());
    if (!dbus.connect(QString(), COMM_HISTORY_OBJECT_PATH, COMM_HISTORY_INTERFACE,
        EVENTS_UPDATED_SIGNAL, this, SLOT(onEventsUpdated(QList<CommHistory::Event>)))) {
        qWarning() << "MmsHandler: failed to register" << EVENTS_UPDATED_SIGNAL << "handler";
    }
    if (!dbus.connect(QString(), COMM_HISTORY_OBJECT_PATH, COMM_HISTORY_INTERFACE,
        GROUPS_UPDATED_FULL_SIGNAL, this, SLOT(onGroupsUpdatedFull(QList<CommHistory::Group>)))) {
        qWarning() << "MmsHandler: failed to register" << GROUPS_UPDATED_FULL_SIGNAL << "handler";
    }
}

QDBusPendingCall MmsHandler::callEngine(const QString &method, const QVariantList &args)
{
    QDBusMessage call(QDBusMessage::createMethodCall(MMS_ENGINE_SERVICE, MMS_ENGINE_PATH,
        MMS_ENGINE_INTERFACE, method));
    call.setArguments(args);
    return MMS_ENGINE_BUS.asyncCall(call);
}

QString MmsHandler::messageNotification(const QString &imsi, const QString &from,
        const QString &subject, uint expiry, const QByteArray &data)
{
    Event event;
    event.setType(Event::MMSEvent);
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(event.startTime());
    event.setDirection(Event::Inbound);
    event.setLocalUid(RING_ACCOUNT_PATH);
    event.setRemoteUid(from);
    event.setSubject(subject);
    event.setExtraProperty(MMS_PROPERTY_IMSI, imsi);
    event.setExtraProperty(MMS_PROPERTY_EXPIRY, expiry);
    event.setExtraProperty(MMS_PROPERTY_PUSH_DATA, data.toBase64());

    // The default action is to download MMS automatically
    const bool manualDownload = (!isDataProhibited() && m_automaticDownload) ?
        !m_automaticDownload->value(true).toBool() :
        true;

    DEBUG_("manualDownload is" << manualDownload);
    event.setStatus(manualDownload ? Event::ManualNotificationStatus : Event::WaitingStatus);

    if (!setGroupForEvent(event)) {
        qCritical() << "Failed to handle group for MMS notification event; message dropped:" << event.toString();
        return QString();
    }

    EventModel model;
    if (!model.addEvent(event)) {
        qCritical() << "Failed to save MMS notification event; message dropped" << event.toString();
        return QString();
    }

    if (!manualDownload) {
        m_activeEvents.append(event.id());
    } else {
        // Show a notification when manual download is needed
        NotificationManager::instance()->showNotification(event, from, Group::ChatTypeP2P);
    }

    DEBUG_("Created MMS notification event:" << event.toString());
    return manualDownload ? QString() : QString::number(event.id());
}

enum MessageReceiveState {
    Receiving = 0,
    Deferred,
    NoSpace,
    Decoding,
    RecvError,
    Garbage
};

void MmsHandler::messageReceiveStateChanged(const QString &recId, int state)
{
    Event event;
    SingleEventModel model;
    if (model.getEventById(recId.toInt()))
        event = model.event();

    if (!event.isValid()) {
        qWarning() << "Ignoring MMS message receive state for unknown event" << recId;
        m_activeEvents.removeOne(recId.toInt());
        return;
    }

    Event::EventStatus newStatus = event.status();
    switch (state) {
        case Deferred:
            newStatus = Event::WaitingStatus;
            break;
        case Receiving:
        case Decoding:
            newStatus = Event::DownloadingStatus;
            break;
        case NoSpace:
        case RecvError:
            // Avoid overwriting the status for cancelled receive calls
            if (event.status() == Event::ManualNotificationStatus)
                return;
            newStatus = Event::TemporarilyFailedStatus;
            break;
        case Garbage:
            newStatus = Event::PermanentlyFailedStatus;
            break;
    }

    if (newStatus != event.status()) {
        event.setStatus(newStatus);
        if (!model.modifyEvent(event))
            qWarning() << "Failed updating MMS event status for" << recId;

        if (newStatus != Event::WaitingStatus && newStatus != Event::DownloadingStatus) {
            m_activeEvents.removeOne(event.id());
            NotificationManager::instance()->showNotification(event, event.remoteUid(), Group::ChatTypeP2P);
        }
    }
}

void MmsHandler::messageReceived(const QString &recId, const QString &mmsId, const QString &from,
        const QStringList &to, const QStringList &cc, const QString &subj, uint date, int priority,
        const QString &cls, bool readReport, MmsPartList parts)
{
    Event event;
    SingleEventModel model;
    if (model.getEventById(recId.toInt()))
        event = model.event();

    m_activeEvents.removeOne(recId.toInt());

    if (!event.isValid()) {
        // Create new event
        event.setType(Event::MMSEvent);
        event.setEndTime(QDateTime::currentDateTime());
        event.setDirection(Event::Inbound);
        event.setLocalUid(RING_ACCOUNT_PATH);
        event.setRemoteUid(from);
        if (!setGroupForEvent(event)) {
            qCritical() << "Failed to handle group for MMS received event; message dropped:" << event.toString();
            return;
        }
    }

    // Update event properties
    event.setSubject(subj);
    event.setStartTime(QDateTime::fromTime_t(date));
    event.setMmsId(mmsId);
    event.setToList(to);
    event.setCcList(cc);
    event.setReportRead(readReport);
    event.setStatus(Event::ReceivedStatus);
    Q_UNUSED(priority);
    Q_UNUSED(cls);

    // We no longer need expiry and push data properties but we may need
    // to keep IMSI property until the message is read
    event.setExtraProperty(MMS_PROPERTY_EXPIRY, QVariant());
    event.setExtraProperty(MMS_PROPERTY_PUSH_DATA, QVariant());
    if (!readReport) event.setExtraProperty(MMS_PROPERTY_IMSI, QVariant());

    // Change UID/group if necessary
    if (event.remoteUid() != from) {
        int oldGroup = event.groupId();
        event.setRemoteUid(from);
        if (!setGroupForEvent(event))
            qCritical() << "Failed handling group for MMS received event";

        if (oldGroup != event.groupId()) {
            int newGroup = event.groupId();
            event.setGroupId(oldGroup);
            if (!model.moveEvent(event, newGroup))
                qCritical() << "Failed moving MMS received event from group" << oldGroup << "to" << newGroup << event.toString();
            event.setGroupId(newGroup);
        }
    }

    // If there wasn't a matching notification, save first to get the event ID before message parts
    if (event.id() < 0 && !model.addEvent(event)) {
        qCritical() << "Failed adding MMS received event; message dropped: " << event.toString();
        return;
    }

    QList<MessagePart> eventParts;
    QString freeText;
    bool ok = copyMmsPartFiles(parts, event.id(), eventParts, freeText);
    if (ok) {
        event.setMessageParts(eventParts);
        event.setFreeText(freeText);

        if (!model.modifyEvent(event)) {
            qCritical() << "Failed updating MMS received event:" << event.toString();
            ok = false;
        }
    }

    if (!ok) {
        // Clean up copied MMS parts, and try to set TemporarilyFailed on the event
        foreach (const MessagePart &part, eventParts)
            QFile::remove(part.path());

        // Re-query event to avoid wiping out notification data
        if (model.getEventById(event.id())) {
            event = model.event();
            if (event.isValid()) {
                event.setStatus(Event::TemporarilyFailedStatus);
                model.modifyEvent(event);
                NotificationManager::instance()->showNotification(event, from, Group::ChatTypeP2P);
            }
        }

        return;
    }

    NotificationManager::instance()->showNotification(event, from, Group::ChatTypeP2P);
    DEBUG_("message " << recId << "received with" << eventParts.size() << "parts:" << event.toString());
}

// Caller is responsible for cleaning up copied files on failure
bool MmsHandler::copyMmsPartFiles(const MmsPartList &parts, int eventId, QList<MessagePart> &eventParts, QString &freeText)
{
    foreach (const MmsPart &part, parts) {
        QString path = copyMessagePartFile(part.fileName, eventId, part.contentId);
        if (path.isEmpty()) {
            qCritical() << "Failed copying message part to storage; message dropped:" << eventId << part.fileName;
            return false;
        }

        MessagePart msgPart;
        msgPart.setContentId(part.contentId);
        msgPart.setContentType(part.contentType);
        msgPart.setPath(path);
        eventParts.append(msgPart);

        // All text/ parts are concatenated for the message content
        if (msgPart.contentType().startsWith("text/plain")) {
            QString text = msgPart.plainTextContent().trimmed();
            if (!text.isEmpty()) {
                if (!freeText.isEmpty())
                    freeText.append('\n');
                freeText.append(text);
            }
        }
    }

    return true;
}

QString MmsHandler::copyMessagePartFile(const QString &sourcePath, int eventId, const QString &contentId)
{
    QString filePath = messagePartPath(eventId, contentId);

    // First try to create a hard link
    if (link(sourcePath.toLatin1(), filePath.toLatin1()) < 0) {
        // If that fails, do a normal copy
        QFile file(sourcePath);
        unlink(filePath.toLatin1()); // File may already exist
        if (!file.copy(filePath)) {
            qCritical() << "Cannot copy message part file" << sourcePath << "to" << filePath;
            return QString();
        }
    }

    return filePath;
}

void MmsHandler::messageSendStateChanged(const QString &recId, int state)
{
    enum MessageSendState {
        Encoding = 0,
        TooBig,
        Sending,
        Deferred,
        NoSpace,
        SendError,
        Refused
    };

    Event event;
    SingleEventModel model;
    if (model.getEventById(recId.toInt()))
        event = model.event();

    if (!event.isValid()) {
        qWarning() << "Ignoring MMS message send state for unknown event" << recId;
        m_activeEvents.removeOne(recId.toInt());
        return;
    }

    Event::EventStatus newStatus = event.status();
    switch (state) {
        case Encoding:
        case Sending:
        case Deferred:
            newStatus = Event::SendingStatus;
            break;
        case TooBig:
        case NoSpace:
        case SendError:
            newStatus = Event::TemporarilyFailedStatus;
            break;
        case Refused:
            newStatus = Event::PermanentlyFailedStatus;
            break;
    }

    if (newStatus != event.status()) {
        event.setStatus(newStatus);
        if (!model.modifyEvent(event))
            qWarning() << "Failed updating MMS event status for" << recId;

        if (newStatus != Event::SendingStatus) {
            m_activeEvents.removeOne(event.id());
            NotificationManager::instance()->showNotification(event, event.remoteUid(), Group::ChatTypeP2P);
        }
    }
}

void MmsHandler::messageSent(const QString &recId, const QString &mmsId)
{
    Event event;
    SingleEventModel model;
    if (model.getEventById(recId.toInt()))
        event = model.event();

    m_activeEvents.removeOne(recId.toInt());

    if (!event.isValid()) {
        qWarning() << "Ignoring MMS message sent state for unknown event" << recId;
        return;
    }

    event.setStatus(Event::SentStatus);
    event.setMmsId(mmsId);
    if (!model.modifyEvent(event))
        qWarning() << "Failed updating MMS event sent status for" << recId;
}

void MmsHandler::deliveryReport(const QString &imsi, const QString &mmsId, const QString &recipient, int status)
{
    Q_UNUSED(imsi);
    Q_UNUSED(recipient); // No handling for read/delivery reports from multiple recipients

    enum DeliveryStatus {
        Indeterminate = 0,
        Expired,
        Retrieved,
        Rejected,
        Deferred,
        Unrecognized,
        Forwarded
    };

    Event event;
    SingleEventModel model;
    if (model.getEventByTokens(QString(), mmsId, -1))
        event = model.event();

    if (!event.isValid()) {
        qWarning() << "Ignoring MMS message delivery state for unknown event" << mmsId;
        return;
    }

    switch (status) {
        case Expired:
        case Rejected:
        case Unrecognized:
            event.setStatus(Event::TemporarilyFailedStatus);
            break;
        case Retrieved:
            event.setStatus(Event::DeliveredStatus);
            break;
        case Indeterminate:
        case Deferred:
        case Forwarded:
            // Are there any more appropriate states here?
            break;
    }

    if (!model.modifyEvent(event))
        qWarning() << "Failed updating MMS event sent status for" << mmsId;
}

void MmsHandler::readReport(const QString &imsi, const QString &mmsId, const QString &recipient, int status)
{
    Q_UNUSED(imsi);
    Q_UNUSED(recipient); // No handling for read/delivery reports from multiple recipients

    Event event;
    SingleEventModel model;
    if (model.getEventByTokens(QString(), mmsId, -1))
        event = model.event();

    if (!event.isValid()) {
        qWarning() << "Ignoring MMS message read state for unknown event" << mmsId;
        return;
    }

    if (status == 0)
        event.setReadStatus(Event::ReadStatusRead);
    else
        event.setReadStatus(Event::ReadStatusDeleted);

    if (!model.modifyEvent(event))
        qWarning() << "Failed updating MMS event sent status for" << mmsId;
}

void MmsHandler::readReportSendStatus(const QString &recId, int status)
{
    enum ReadReportStatus {
        ReadReportOK = 0,
        ReadReportTransientError,
        ReadReportPermanentError
    };

    DEBUG_(recId << "read report status" << status);
    if (status != ReadReportTransientError) {
        SingleEventModel model;
        if (model.getEventById(recId.toInt())) {
            Event event(model.event());
            event.setExtraProperty(MMS_PROPERTY_IMSI, QVariant());
            if (!model.modifyEvent(event)) {
                qWarning() << "Failed to update MMS event" << event.id();
            }
        } else {
            qWarning() << "Ignoring read report completion for unknown event" << recId;
        }
    }
}

static QStringList normalizeNumberList(const QStringList &in)
{
    QStringList out;
    out.reserve(in.size());
    foreach (const QString &s, in)
        out.append(CommHistory::normalizePhoneNumber(s, false));
    return out;
}

int MmsHandler::sendMessage(const QStringList &to, const QStringList &cc, const QStringList &bcc,
        const QString &subject, MmsPartList parts)
{
    Event event;
    event.setType(Event::MMSEvent);
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(event.startTime());
    event.setDirection(Event::Outbound);
    event.setLocalUid(RING_ACCOUNT_PATH);
    event.setSubject(subject);
    event.setStatus(Event::SendingStatus);
    event.setIsRead(true);

    event.setRemoteUid(CommHistory::normalizePhoneNumber(to[0], false)); // XXX Wrong for group conversations!
    event.setToList(normalizeNumberList(to));
    event.setCcList(normalizeNumberList(cc));
    event.setBccList(normalizeNumberList(bcc));

    // XXX Group conversations not yet supported
    if (to.size() + cc.size() + bcc.size() > 1) {
        qCritical() << "Ignoring outgoing group MMS event; this is not yet implemented:" << event.toString();
        return -1;
    }

    if (!setGroupForEvent(event)) {
        qCritical() << "Failed to handle group for MMS send event; message dropped:" << event.toString();
        return -1;
    }

    // Save to get an event ID
    SingleEventModel model;
    if (!model.addEvent(event)) {
        qCritical() << "Failed adding outgoing MMS event:" << event.toString();
        return -1;
    }

    // Copy message parts
    QList<MessagePart> eventParts;
    QString freeText;
    bool ok = copyMmsPartFiles(parts, event.id(), eventParts, freeText);
    if (ok) {
        event.setMessageParts(eventParts);
        event.setFreeText(freeText);

        if (!model.modifyEvent(event)) {
            qCritical() << "Failed modifying outgoing MMS event:" << event.toString();
            ok = false;
        }
    }

    if (!ok) {
        // Clean up copied MMS parts
        foreach (const MessagePart &part, eventParts)
            QFile::remove(part.path());
        // Re-query event to avoid wiping out notification data
        if (event.id() >= 0 && model.getEventById(event.id())) {
            event = model.event();
            if (event.isValid()) {
                event.setStatus(Event::PermanentlyFailedStatus);
                model.modifyEvent(event);
            }
        }
    } else if (isDataProhibited()) {
        qWarning() << "Refusing to send MMS message due to data roaming restrictions";
        event.setStatus(Event::TemporarilyFailedStatus);
        model.modifyEvent(event);
    } else {
        sendMessageFromEvent(event);
    }

    if (event.status() >= Event::TemporarilyFailedStatus)
        NotificationManager::instance()->showNotification(event, event.remoteUid(), Group::ChatTypeP2P);
    return event.id();
}

void MmsHandler::sendMessageFromEvent(int eventId)
{
    Event event;
    SingleEventModel model;
    if (model.getEventById(eventId))
        event = model.event();

    if (!event.isValid() || event.type() != Event::MMSEvent || event.direction() != Event::Outbound) {
        qCritical() << "Ignoring MMS sendMessageFromEvent with irrelevant event:" << event.toString();
        return;
    }

    if (event.toList().size() + event.ccList().size() + event.bccList().size() < 1) {
        qCritical() << "Ignoring MMS sendMessageFromEvent with no recipients:" << event.toString();
        return;
    }

    if (event.messageParts().size() < 1) {
        qCritical() << "Ignoring MMS sendMessageFromEvent with no parts:" << event.toString();
        return;
    }

    if (event.status() != Event::SendingStatus) {
        event.setStatus(Event::SendingStatus);
        model.modifyEvent(event);
    }

    sendMessageFromEvent(event);
}

void MmsHandler::sendMessageFromEvent(Event &event)
{
    MmsPartList parts;
    foreach (const MessagePart &part, event.messageParts()) {
        MmsPart p = { part.path(), part.contentType(), part.contentId() };
        parts.append(p);
    }

    unsigned int flags = m_sendMessageFlags ? m_sendMessageFlags->value().toInt() : 0;
    DEBUG_("send flag are" << flags);

    QVariantList args;
    args << event.id() << QString() << event.toList() << event.ccList() << event.bccList()
         << event.subject() << flags << QVariant::fromValue(parts);

    m_activeEvents.append(event.id());

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(callEngine("sendMessage", args), this);
    watcher->setProperty("mms-event-id", event.id());
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), SLOT(onSendMessageFinished(QDBusPendingCallWatcher*)));
}

void MmsHandler::onSendMessageFinished(QDBusPendingCallWatcher *call)
{
    bool ok = false;
    int eventId = call->property("mms-event-id").toInt(&ok);

    Event event;
    SingleEventModel model;
    if (ok && model.getEventById(eventId))
        event = model.event();

    QDBusPendingReply<QString> reply = *call;
    if (reply.isError()) {
        qCritical() << "Call to MmsEngine sendMessage failed:" << reply.error();
        event.setStatus(Event::TemporarilyFailedStatus);
        NotificationManager::instance()->showNotification(event, event.remoteUid(), Group::ChatTypeP2P);
    } else {
        event.setExtraProperty(MMS_PROPERTY_IMSI, reply.value());
    }

    if (!model.modifyEvent(event))
        qCritical() << "Updating outgoing MMS event after sendMessage call failed:" << event.toString();

    call->deleteLater();
}

bool MmsHandler::isDataProhibited()
{
    if (m_cellularStatusProperty->value().toString() != "roaming")
        return false;
    if (!m_roamingAllowedProperty->value().toBool())
        return true;

    // TODO: This property should be monitored asynchronously to avoid blocking dbus queries
    QDBusInterface interface("com.jolla.Connectiond", "/Connectiond");
    // For now, treat "always ask" like "never"
    if (interface.property("askRoaming").toBool())
        return true;
    return false;
}

bool MmsHandler::canSendReadReports()
{
    QString imsi = m_subscriberIdentityProperty->value().toString();
    return !imsi.isEmpty() && !isDataProhibited();
}

void MmsHandler::onDataProhibitedChanged()
{
    if (!m_activeEvents.isEmpty() && isDataProhibited()) {
        qWarning() << "Cancelling" << m_activeEvents.size() << "active MMS events due to roaming restrictions";
        // Cancel any active events to prevent automatic retries
        foreach (int eventId, m_activeEvents) {
            callEngine("cancel", QVariantList() << eventId);
        }
        m_activeEvents.clear();
    }
}

void MmsHandler::onSubscriberIdentityChanged()
{
    QString imsi = m_subscriberIdentityProperty->value().toString();
    DEBUG_("SubscriberIdentity =" << m_subscriberIdentityProperty->value() << imsi);
    delete m_sendMessageFlags;
    delete m_automaticDownload;
    delete m_sendReadReports;
    if (imsi.isEmpty()) {
        m_sendMessageFlags = NULL;
        m_automaticDownload = NULL;
        m_sendReadReports = NULL;
    } else {
        QString dir("/imsi/" + imsi + "/mms/");
        m_sendMessageFlags = new MGConfItem(dir + "send-flags", this);
        m_automaticDownload = new MGConfItem(dir + "automatic-download", this);
        m_sendReadReports = new MGConfItem(dir + "send-read-reports", this);
    }
}

void MmsHandler::eventMarkedAsRead(CommHistory::Event &event)
{
    // Caller already checked canSendReadReports() so mobile data is allowed
    const bool sendReadReports = (m_sendReadReports &&
        m_sendReadReports->value(false).toBool());
    QString imsi = event.extraProperty(MMS_PROPERTY_IMSI).toString();
    if (sendReadReports) {
        DEBUG_("sending read report for" << event.id());
        QVariantList args;
        args << event.id() << imsi << event.mmsId() << event.remoteUid() << 0;
        callEngine("sendReadReport", args);
    } else {
        DEBUG_("not allowed to send read report for" << event.id());
        event.setExtraProperty(MMS_PROPERTY_IMSI, QVariant());
        SingleEventModel model;
        if (!model.modifyEvent(event)) {
            qWarning() << "Failed to update MMS event" << event.id();
        }
    }
}

void MmsHandler::onEventsUpdated(const QList<CommHistory::Event> &events)
{
    DEBUG_(events.count() << "event(s) updated");
    if (!canSendReadReports()) {
        DEBUG_("can't send anything at the moment");
    } else {
        for (int i=0; i<events.count(); i++) {
            Event event(events.at(i));
            DEBUG_(i << ":" << event.toString());
            if (event.type() == Event::MMSEvent &&
                event.direction() == Event::Inbound &&
                event.isRead() && event.reportRead() &&
                !event.mmsId().isEmpty()) {
                QString imsi = event.extraProperty(MMS_PROPERTY_IMSI).toString();
                if (!imsi.isEmpty()) {
                    eventMarkedAsRead(event);
                }
            }
        }
    }
}

void MmsHandler::onGroupsUpdatedFull(const QList<CommHistory::Group> &groups)
{
    DEBUG_(groups.count() << "group(s) updated");
    if (!canSendReadReports()) {
        DEBUG_("can't send anything at the moment");
    } else {
        int i;
        QList<int> groupIds;
        for (i=0; i<groups.count(); i++) {
            Group group(groups.at(i));
            DEBUG_(i << ":" << group.toString());
            groupIds.append(group.id());
        }
        ConversationModel model;
        model.setQueryMode(EventModel::SyncQuery);
        model.setTreeMode(false);
        model.setFilterForMmsReadReport();
        if (model.getEvents(groupIds)) {
            DEBUG_(model.rowCount() << "MMS event(s) found");
            for (i=0; i<model.rowCount(); i++) {
                Event event(model.event(model.index(i, 0)));
                eventMarkedAsRead(event);
            }
        } else {
            qWarning() << "Failed to query MMS events";
        }
    }
}
