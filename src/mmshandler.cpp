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
#include <CommHistory/groupmanager.h>

using namespace RTComLogger;
using namespace CommHistory;

MmsHandler::MmsHandler(QObject* parent)
    : QObject(parent), m_isRegistered(false), groupManager(0)
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
    // Should be ManualNotificationStatus if not triggered automatically
    event.setStatus(Event::WaitingStatus);

    if (!setGroupForEvent(event)) {
        qCritical() << "Failed to handle group for MMS notification event; message dropped:" << event.toString();
        return QString();
    }

    EventModel model;
    if (!model.addEvent(event)) {
        qCritical() << "Failed to save MMS notification event; message dropped" << event.toString();
        return QString();
    }

    DEBUG() << "Created MMS notification event:" << event.toString();
    return QString::number(event.id());
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
    bool storageFailed = false;
    QString freeText;
    foreach (const MmsPart &part, parts) {
        QString path = copyMessagePartFile(part.fileName, event.id(), part.contentId);
        if (path.isEmpty()) {
            qCritical() << "Failed copying message part to storage; message dropped:" << event.id() << part.fileName;
            storageFailed = true;
            break;
        }

        MessagePart msgPart;
        msgPart.setContentId(part.contentId);
        msgPart.setContentType(part.contentType);
        msgPart.setPath(path);
        eventParts.append(msgPart);

        // All text/ parts are concatenated for the message content
        if (msgPart.contentType().startsWith("text/plain")) {
            QString text = msgPart.plainTextContent();
            if (!text.isEmpty()) {
                if (!freeText.isEmpty())
                    freeText.append('\n');
                freeText.append(text);
            }
        }
    }
    event.setMessageParts(eventParts);
    event.setFreeText(freeText);

    if (!storageFailed && !model.modifyEvent(event)) {
        qCritical() << "Failed updating MMS received event:" << event.toString();
        storageFailed = true;
    }

    if (storageFailed) {
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
    for (int i = 0; i < name.size(); i++) {
        if (name[i] < '0' || (name[i] > '9' && name[i] < 'A') || (name[i] > 'Z' && name[i] < 'a') || (name[i] > 'z'))
            name[i] = '_';
    }
    return name;
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
            qCritical() << "Cannot copy message part file" << sourcePath << "to" << file.fileName();
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

void MmsHandler::deliveryReport(const QString &imsi, const QString &mmsId, const QString &recipient, int status)
{
    Q_UNUSED(imsi);
    Q_UNUSED(mmsId);
    Q_UNUSED(recipient);
    Q_UNUSED(status);
}

void MmsHandler::messageSendStateChanged(const QString &recId, int state)
{
    Q_UNUSED(recId);
    Q_UNUSED(state);
}

void MmsHandler::messageSent(const QString &recId, const QString &mmsId)
{
    Q_UNUSED(recId);
    Q_UNUSED(mmsId);
}

void MmsHandler::readReport(const QString &imsi, const QString &mmsId, const QString &recipient, int status)
{
    Q_UNUSED(imsi);
    Q_UNUSED(mmsId);
    Q_UNUSED(recipient);
    Q_UNUSED(status);
}

