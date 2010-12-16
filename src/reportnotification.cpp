/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Alexander Shalamov <alexander.shalamov@nokia.com>
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

#include "reportnotification.h"
#include "locstrings.h"

// commhistory
#include <CommHistory/Group>

#include <QDebug>

using namespace RTComLogger;

ReportNotification::ReportNotification(QObject* parent) : PersonalNotification(parent), m_reportType(DELIVERY_REPORT)
{
}

ReportNotification::ReportNotification(const QString& remoteUid,
                                       const QString& account,
                                       int reportType,
                                       QObject* parent) :
    PersonalNotification(remoteUid, account, CommHistory::Event::UnknownType,
                         QString(), CommHistory::Group::ChatTypeP2P,
                         0, QString(), parent),
    m_reportType(reportType)
{
}

ReportNotification::ReportNotification(const ReportNotification& other) :
    PersonalNotification(other.parent())
{
    *this = other;
}

ReportNotification& ReportNotification::operator=(const ReportNotification& other)
{
    PersonalNotification::operator=(other);

    m_reportType = other.m_reportType;
    return *this;
}

int ReportNotification::reportType() const
{
    return m_reportType;
}

void ReportNotification::setReportType(int reportType)
{
    m_reportType = reportType;
}

bool ReportNotification::operator == (const ReportNotification& other) const
{
    if (PersonalNotification::operator==(other) &&
        m_reportType == other.m_reportType)
    {
        return true;
    }

    return false;
}

bool ReportNotification::operator != (const ReportNotification& other) const
{
    return !(*this == other);
}

void ReportNotification::updateNotificationText(const QString &report_from)
{
    switch (m_reportType) {
        case DELIVERY_REPORT:
            setNotificationText(txt_qtn_msg_notification_delivered(report_from));
            break;
        case READ_REPORT:
            setNotificationText(txt_qtn_msg_notification_read(report_from));
            break;
        case DELETED_WITHOUT_READING_REPORT:
            setNotificationText(txt_qtn_msg_notification_deleted(report_from));
            break;
        default:
            qWarning() << "Failed to update report notification text for unknown type: " << m_reportType;
    }
}

QDataStream& operator<<(QDataStream &out, const RTComLogger::ReportNotification &key)
{
    key.serialize(out, key);
    return out;
}

QDataStream& operator>>(QDataStream &in, RTComLogger::ReportNotification &key)
{
    key.deSerialize(in, key);
    return in;
}

