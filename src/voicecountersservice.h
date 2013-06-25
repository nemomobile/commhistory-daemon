/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2013 Jolla.
** Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
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

#ifndef VOICECOUNTERSSERVICE_H
#define VOICECOUNTERSSERVICE_H

#include <QtCore/QObject>
#include <QtCore/QVariantMap>
#include <QtCore/QDateTime>
#include <QtCore/QMap>

namespace CommHistory {
class Event;
}

class VoiceCountersService : public QObject
{
    Q_OBJECT

public:
    explicit VoiceCountersService(QObject *parent = 0);
    ~VoiceCountersService();

    bool isRegistered() const;

    void addCall(const CommHistory::Event &event, bool callCompleted = true);

    static VoiceCountersService *instance();

signals:
    void countersChanged();

public slots:
    QVariantMap getCounters();
    void resetCounters();

private:
    void addReceivedCall(const QDateTime &startTime, const QDateTime &endTime);
    void addDialledCall(const QDateTime &startTime, const QDateTime &endTime);

    bool m_isRegistered;
    QMap<int, QDateTime> m_activeDialledCalls;
    QMap<int, QDateTime> m_activeReceivedCalls;
};

#endif // VOICECOUNTERSSERVICE_H
