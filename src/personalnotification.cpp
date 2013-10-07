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

#include "personalnotification.h"
#include "notificationmanager.h"
#include "notificationgroup.h"
#include "locstrings.h"
#include "debug.h"

#include <CommHistory/commonutils.h>

#include <MNotificationGroup>
#include <MLocale>

using namespace RTComLogger;

PersonalNotification::PersonalNotification(QObject* parent) : QObject(parent),
    m_eventType(CommHistory::Event::UnknownType),
    m_chatType(CommHistory::Group::ChatTypeP2P),
    m_contactId(0),
    m_hasPendingEvents(false),
    m_notification(0)
{
}

PersonalNotification::PersonalNotification(const QString& remoteUid,
                                           const QString& account,
                                           CommHistory::Event::EventType eventType,
                                           const QString& channelTargetId,
                                           CommHistory::Group::ChatType chatType,
                                           uint contactId,
                                           const QString& lastNotification,
                                          QObject* parent) :
    QObject(parent), m_remoteUid(remoteUid), m_account(account),
    m_eventType(eventType), m_targetId(channelTargetId), m_chatType(chatType),
    m_contactId(contactId), m_notificationText(lastNotification),
    m_hasPendingEvents(true),
    m_notification(0)
{
}

PersonalNotification::~PersonalNotification()
{
    delete m_notification;
}

void PersonalNotification::publishNotification(NotificationGroup *group)
{
    QString name;

    // voicemail notifications shouldn't have contact name
    if (m_eventType != CommHistory::Event::VoicemailEvent)
        name = notificationName();

    QString activateAction = NotificationManager::instance()->action(group, this, false);
    QString event = NotificationGroup::groupType(m_eventType);

    if (!m_notification)
        m_notification = new MNotification(event);

    if (group && group->notificationGroup())
        m_notification->setGroup(*group->notificationGroup());

    m_notification->setSummary(name);
    m_notification->setBody(notificationText());
    m_notification->setAction(MRemoteAction(activateAction, this));
    m_notification->publish();

    setHasPendingEvents(false);

    DEBUG() << event << name << m_notification->body() << activateAction;
}

void PersonalNotification::removeNotification()
{
    DEBUG() << "removing notification" << m_notification;
    if (m_notification)
        m_notification->remove();
    delete m_notification;
    m_notification = 0;

    setHasPendingEvents(false);
}

QString PersonalNotification::notificationName() const
{
    if (!chatName().isEmpty())
        return chatName();
    else if (!contactName().isEmpty())
        return contactName();
    else if (remoteUid() == QLatin1String("<hidden>"))
        return txt_qtn_call_type_private;
    else if (CommHistory::normalizePhoneNumber(remoteUid()).isEmpty())
        return remoteUid();

    ML10N::MLocale locale;
    return locale.toLocalizedNumbers(remoteUid());
}

QString PersonalNotification::remoteUid() const
{
    return m_remoteUid;
}

QString PersonalNotification::account() const
{
    return m_account;
}

uint PersonalNotification::eventType() const
{
    return m_eventType;
}

QString PersonalNotification::targetId() const
{
    return m_targetId;
}

uint PersonalNotification::chatType() const
{
    return m_chatType;
}

QString PersonalNotification::contactName() const
{
    return m_contactName;
}

uint PersonalNotification::contactId() const
{
    return m_contactId;
}


QString PersonalNotification::notificationText() const
{
    return m_notificationText;
}

bool PersonalNotification::hasPendingEvents() const
{
    return m_hasPendingEvents;
}

QString PersonalNotification::chatName() const
{
    return m_chatName;
}

QString PersonalNotification::eventToken() const
{
    return m_eventToken;
}

QString PersonalNotification::smsReplaceNumber() const
{
    return m_smsReplaceNumber;
}

void PersonalNotification::setRemoteUid(const QString& remoteUid)
{
    m_remoteUid = remoteUid;
    setHasPendingEvents(true);
}

void PersonalNotification::setAccount(const QString& account)
{
    m_account = account;
    setHasPendingEvents(true);
}

void PersonalNotification::setEventType(uint eventType)
{
    m_eventType = eventType;
    setHasPendingEvents(true);
}

void PersonalNotification::setTargetId(const QString& targetId)
{
    m_targetId = targetId;
    setHasPendingEvents(true);
}

void PersonalNotification::setChatType(uint chatType)
{
    m_chatType = chatType;
    setHasPendingEvents(true);
}

void PersonalNotification::setContactName(const QString& contactName)
{
    m_contactName = contactName;
    setHasPendingEvents(true);
}

void PersonalNotification::setContactId(uint contactId)
{
    m_contactId = contactId;
    setHasPendingEvents(true);
}

void PersonalNotification::setNotificationText(const QString& notificationText)
{
    m_notificationText = notificationText;
    setHasPendingEvents(true);
}

void PersonalNotification::setHasPendingEvents(bool hasPendingEvents)
{
    if (m_hasPendingEvents != hasPendingEvents) {
        m_hasPendingEvents = hasPendingEvents;
        emit hasPendingEventsChanged(hasPendingEvents);
    }
}

void PersonalNotification::setChatName(const QString& chatName)
{
    m_chatName = chatName;
    setHasPendingEvents(true);
}

void PersonalNotification::setEventToken(const QString &eventToken)
{
    m_eventToken = eventToken;
    setHasPendingEvents(true);
}

void PersonalNotification::setSmsReplaceNumber(const QString &number)
{
    m_smsReplaceNumber = number;
    setHasPendingEvents(true);
}

QDataStream& operator<<(QDataStream &out, const RTComLogger::PersonalNotification &key)
{
    key.serialize(out, key);
    return out;
}

QDataStream& operator>>(QDataStream &in, RTComLogger::PersonalNotification &key)
{
    key.deSerialize(in, key);
    return in;
}

