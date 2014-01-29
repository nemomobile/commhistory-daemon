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

#ifndef MMSHANDLER_H
#define MMSHANDLER_H

#include <QObject>
#include "mmspart.h"

namespace CommHistory {
    class Event;
    class GroupManager;
}

class MmsHandler : public QObject
{
    Q_OBJECT

public:
    explicit MmsHandler(QObject *parent);

public Q_SLOTS:
    QString messageNotification(const QString &imsi, const QString &from, const QString &subject,
            uint expiry, const QByteArray &data);
    void messageReceiveStateChanged(const QString &recId, int state);
    void messageReceived(const QString &recId, const QString &mmsId, const QString &from,
            const QStringList &to, const QStringList &cc, const QString &subj, uint date, int priority,
            const QString &cls, bool readReport, MmsPartList parts);

    void deliveryReport(const QString &imsi, const QString &mmsId, const QString &recipient, int status);
    void messageSendStateChanged(const QString &recId, int state);
    void messageSent(const QString &recId, const QString &mmsId);
    void readReport(const QString &imsi, const QString &mmsId, const QString &recipient, int status);

private:
    bool m_isRegistered;
    CommHistory::GroupManager *groupManager;

    bool setGroupForEvent(CommHistory::Event &event);
    QString copyMessagePartFile(const QString &sourcePath, int eventId, const QString &contentId);
};

#endif // MMSHANDLER_H
