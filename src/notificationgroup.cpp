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

#include <QHash>
#include "notificationgroup.h"

using namespace RTComLogger;

NotificationGroup::NotificationGroup(QObject* parent) : QObject(parent),
    m_type(-1)
{
}

NotificationGroup::NotificationGroup(int notificationGroupType,
                                         QObject* parent) :
    QObject(parent), m_type(notificationGroupType)
{
}

NotificationGroup::NotificationGroup(const NotificationGroup& other) :
    QObject(other.parent())
{
    *this = other;
}

NotificationGroup& NotificationGroup::operator=(const NotificationGroup& other)
{
    setParent(other.parent());
    setType(other.type());
    return *this;
}

bool NotificationGroup::operator == (const NotificationGroup& other) const
{
    return (type() == other.type());
}

bool NotificationGroup::operator != (const NotificationGroup& other) const
{
    return !(*this == other);
}

bool NotificationGroup::isValid() const
{
    return (m_type != -1);
}

int NotificationGroup::type() const
{
    return m_type;
}

void NotificationGroup::setType(int notificationType)
{
    m_type = notificationType;
}

QDataStream& operator<<(QDataStream &out, const RTComLogger::NotificationGroup &key)
{
    key.serialize(out, key);
    return out;
}

QDataStream& operator>>(QDataStream &in, RTComLogger::NotificationGroup &key)
{
    key.deSerialize(in, key);
    return in;
}

uint qHash(const RTComLogger::NotificationGroup& group)
{
    return qHash(group.type());
}
