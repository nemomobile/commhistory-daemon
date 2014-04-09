/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2014 Jolla Ltd.
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
#include <CommHistory/commonutils.h>
#include <CommHistory/groupmanager.h>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <contextproperty.h>

using namespace RTComLogger;
using namespace CommHistory;

MmsHandler::MmsHandler(QObject* parent)
    : QObject(parent)
    , m_isRegistered(false)
    , groupManager(0)
    , m_cellularStatusProperty(new ContextProperty("Cellular.Status", this))
    , m_roamingAllowedProperty(new ContextProperty("Cellular.DataRoamingAllowed", this))
{
    qDBusRegisterMetaType<MmsPart>();
    qDBusRegisterMetaType<MmsPartList>();

    QDBusConnection dbus = QDBusConnection::systemBus();
    if (!dbus.isConnected()) {
        qCritical() << "ERROR: No DBus system bus found!";
        return;
    }

    if (parent) {
        if (!dbus.registerObject("/", this)) {
            qWarning() << "Object registration failed:" << dbus.lastError();;
        } else if (!dbus.registerService("org.nemomobile.MmsHandler")) {
            qWarning() << "Service registration failed:" << dbus.lastError();
        } else {
            m_isRegistered = true;
        }
    }

    connect(m_cellularStatusProperty, SIGNAL(valueChanged()), SLOT(onDataProhibitedChanged()));
    connect(m_roamingAllowedProperty, SIGNAL(valueChanged()), SLOT(onDataProhibitedChanged()));
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
    event.setExtraProperty("mms-notification-imsi", imsi);
    event.setExtraProperty("mms-expiry", expiry);
    event.setExtraProperty("mms-push-data", data.toBase64());

    bool manualDownload = isDataProhibited();
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

    DEBUG() << "Created MMS notification event:" << event.toString();
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
        event = model.event(model.index(0, 0));

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

        if (newStatus != Event::WaitingStatus && newStatus != Event::DownloadingStatus)
            m_activeEvents.removeOne(event.id());
    }
}

void MmsHandler::messageReceived(const QString &recId, const QString &mmsId, const QString &from,
        const QStringList &to, const QStringList &cc, const QString &subj, uint date, int priority,
        const QString &cls, bool readReport, MmsPartList parts)
{
    Event event;
    SingleEventModel model;
    if (model.getEventById(recId.toInt()))
        event = model.event(model.index(0, 0));

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

    // Remove MMS notification properties
    event.setExtraProperty("mms-notification-imsi", QVariant());
    event.setExtraProperty("mms-expiry", QVariant());
    event.setExtraProperty("mms-push-data", QVariant());

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
            event = model.event(model.index(0, 0));
            if (event.isValid()) {
                event.setStatus(Event::TemporarilyFailedStatus);
                model.modifyEvent(event);
            }
        }

        return;
    }

    NotificationManager::instance()->showNotification(event, from, Group::ChatTypeP2P);
    DEBUG() << "MMS message " << recId << "received with" << eventParts.size() << "parts:" << event.toString();
}

static QString sanitizeName(QString name)
{
    return name.replace(QRegularExpression("[^-.0-9a-zA-Z]"), "_");
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
    QDir dataDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/commhistory/data/%1/").arg(eventId));
    if (!dataDir.exists() && !dataDir.mkpath(".")) {
        qCritical() << "Cannot create directory for MMS message parts:" << dataDir.path();
        return QString();
    }

    QString filePath = dataDir.filePath(sanitizeName(contentId));

    // First try to create a hard link
    if (link(sourcePath.toLatin1(), filePath.toLatin1()) < 0) {
        // If that fails, do a normal copy
        QFile file(sourcePath);
        if (!file.copy(filePath)) {
            qCritical() << "Cannot copy message part file" << sourcePath << "to" << filePath;
            return QString();
        }
    }

    return filePath;
}

bool MmsHandler::setGroupForEvent(Event &event)
{
    if (!groupManager) {
        groupManager = new GroupManager(this);
        if (!groupManager->getGroups(RING_ACCOUNT_PATH)) {
            delete groupManager;
            groupManager = 0;
            return false;
        }
    }

    GroupObject *group = groupManager->findGroup(RING_ACCOUNT_PATH, event.remoteUid());
    if (group) {
        event.setGroupId(group->id());
        return true;
    }

    DEBUG() << "Creating new group for MMS" << event.remoteUid();
    Group newGroup;
    newGroup.setLocalUid(RING_ACCOUNT_PATH);
    newGroup.setRemoteUids(QStringList() << event.remoteUid());
    if (!groupManager->addGroup(newGroup)) {
        qCritical() << "Failed adding new group for MMS" << newGroup.toString();
        return false;
    }

    event.setGroupId(newGroup.id());
    return true;
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
        event = model.event(model.index(0, 0));

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

        if (newStatus != Event::SendingStatus)
            m_activeEvents.removeOne(event.id());
    }
}

void MmsHandler::messageSent(const QString &recId, const QString &mmsId)
{
    Event event;
    SingleEventModel model;
    if (model.getEventById(recId.toInt()))
        event = model.event(model.index(0, 0));

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
        event = model.event(model.index(0, 0));

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
        event = model.event(model.index(0, 0));

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
            event = model.event(model.index(0, 0));
            if (event.isValid()) {
                event.setStatus(Event::PermanentlyFailedStatus);
                model.modifyEvent(event);
            }
        }
        return event.id();
    }

    if (isDataProhibited()) {
        qWarning() << "Refusing to send MMS message due to data roaming restrictions";
        event.setStatus(Event::TemporarilyFailedStatus);
        model.modifyEvent(event);
        return event.id();
    }

    sendMessageFromEvent(event);
    return event.id();
}

void MmsHandler::sendMessageFromEvent(int eventId)
{
    Event event;
    SingleEventModel model;
    if (model.getEventById(eventId))
        event = model.event(model.index(0, 0));

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

    QVariantList args;
    args << event.id() << QString() << event.toList() << event.ccList() << event.bccList()
         << event.subject() << unsigned(0) << QVariant::fromValue(parts);

    m_activeEvents.append(event.id());

    QDBusMessage call = QDBusMessage::createMethodCall("org.nemomobile.MmsEngine", "/", "org.nemomobile.MmsEngine", "sendMessage");
    call.setArguments(args);
    QDBusPendingCall reply = QDBusConnection::systemBus().asyncCall(call);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    watcher->setProperty("mms-event-id", event.id());
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), SLOT(sendMessageFinished(QDBusPendingCallWatcher*)));
}

void MmsHandler::sendMessageFinished(QDBusPendingCallWatcher *call)
{
    bool ok = false;
    int eventId = call->property("mms-event-id").toInt(&ok);

    Event event;
    SingleEventModel model;
    if (ok && model.getEventById(eventId))
        event = model.event(model.index(0, 0));

    QDBusPendingReply<QString> reply = *call;
    if (reply.isError()) {
        qCritical() << "Call to MmsEngine sendMessage failed:" << reply.error();
        event.setStatus(Event::TemporarilyFailedStatus);
    } else {
        event.setExtraProperty("mms-notification-imsi", reply.value());
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

void MmsHandler::onDataProhibitedChanged()
{
    if (!m_activeEvents.isEmpty() && isDataProhibited()) {
        qWarning() << "Cancelling" << m_activeEvents.size() << "active MMS events due to roaming restrictions";
        // Cancel any active events to prevent automatic retries
        foreach (int eventId, m_activeEvents) {
            QDBusMessage call = QDBusMessage::createMethodCall("org.nemomobile.MmsEngine", "/", "org.nemomobile.MmsEngine", "cancel");
            call.setArguments(QVariantList() << eventId);
            QDBusConnection::systemBus().asyncCall(call);
        }
        m_activeEvents.clear();
    }
}

