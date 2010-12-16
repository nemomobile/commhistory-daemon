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

#ifndef REPORTNOTIFICATION_H
#define REPORTNOTIFICATION_H

#include <QObject>
#include <QString>
#include <QMetaType>

#include "personalnotification.h"

namespace RTComLogger {

class ReportNotification : public PersonalNotification
{
    Q_OBJECT

    Q_PROPERTY(int reportType READ reportType WRITE setReportType)

public:
    ReportNotification(QObject* parent = 0);
    ReportNotification(const QString& remoteUid,
                       const QString& account,
                       int reportType,
                       QObject* parent = 0);
    ReportNotification(const ReportNotification& other);
    ReportNotification& operator = (const ReportNotification& other);
    bool operator == (const ReportNotification& other) const;
    bool operator != (const ReportNotification& other) const;

public:
    enum {
        DELIVERY_REPORT,
        READ_REPORT,
        DELETED_WITHOUT_READING_REPORT,
    };

    int reportType() const;

    void setReportType(int reportType);

    void updateNotificationText(const QString &report_from);
private:
    int m_reportType;
};

} // namespace

Q_DECLARE_METATYPE(RTComLogger::ReportNotification)

QDataStream& operator<<(QDataStream &out, const RTComLogger::ReportNotification &key);
QDataStream& operator>>(QDataStream &in, RTComLogger::ReportNotification &key);

#endif // REPORTNOTIFICATION_H
