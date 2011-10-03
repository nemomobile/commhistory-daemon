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

using namespace RTComLogger;

PersonalNotification::PersonalNotification(QObject* parent) : QObject(parent),
    m_eventType(CommHistory::Event::UnknownType),
    m_chatType(CommHistory::Group::ChatTypeP2P),
    m_contactId(0),
    m_hasPendingEvents(false)
{
}

PersonalNotification::PersonalNotification(const QString& remoteUid,
                                           const QString& account,
                                           CommHistory::Event::EventType eventType,
                                           const QString& channelTargetId,
                                           CommHistory::Group::ChatType chatType,
                                           int contactId,
                                           const QString& lastNotification,
                                          QObject* parent) :
    QObject(parent), m_remoteUid(remoteUid), m_account(account),
    m_eventType(eventType), m_targetId(channelTargetId), m_chatType(chatType),
    m_contactId(contactId), m_notificationText(lastNotification),
    m_hasPendingEvents(false)
{
}

PersonalNotification::PersonalNotification(const PersonalNotification& other) :
    QObject(other.parent())
{
    *this = other;
}

PersonalNotification& PersonalNotification::operator=(const PersonalNotification& other)
{
    setParent(other.parent());
    setRemoteUid(other.remoteUid());
    setEventType(other.eventType());
    setTargetId(other.targetId());
    setChatType(other.chatType());
    setAccount(other.account());
    setContactId(other.contactId());
    setNotificationText(other.notificationText());
    setHasPendingEvents(other.hasPendingEvents());
    setChatName(other.chatName());
    setEventToken(other.eventToken());
    setSmsReplaceNumber(other.smsReplaceNumber());
    return *this;
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

int PersonalNotification::contactId() const
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
}

void PersonalNotification::setAccount(const QString& account)
{
    m_account = account;
}

void PersonalNotification::setEventType(uint eventType)
{
    m_eventType = eventType;
}

void PersonalNotification::setTargetId(const QString& targetId)
{
    m_targetId = targetId;
}

void PersonalNotification::setChatType(uint chatType)
{
    m_chatType = chatType;
}

void PersonalNotification::setContactId(int contactId)
{
    m_contactId = contactId;
}

void PersonalNotification::setNotificationText(const QString& notificationText)
{
    m_notificationText = notificationText;
}

void PersonalNotification::setHasPendingEvents(bool hasPendingEvents)
{
    m_hasPendingEvents = hasPendingEvents;
}

void PersonalNotification::setChatName(const QString& chatName)
{
    m_chatName = chatName;
}

void PersonalNotification::setEventToken(const QString &eventToken)
{
    m_eventToken = eventToken;
}

void PersonalNotification::setSmsReplaceNumber(const QString &number)
{
    m_smsReplaceNumber = number;
}

bool PersonalNotification::operator == (const PersonalNotification& other) const
{
    if(eventType() == other.eventType()
       && targetId() == other.targetId()
       && chatType() == other.chatType()
       && account() == other.account()) {

        // With MUC notifications compare chatNames, with other type of
        // notifications compare remoteUids.
        if ((!chatName().isEmpty() && chatName() == other.chatName())
            || (chatName().isEmpty() && remoteUid() == other.remoteUid())) {

            return true;
        }
    }
    return false;
}

bool PersonalNotification::operator != (const PersonalNotification& other) const
{
    return !(*this == other);
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

