/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2013 Jolla Ltd.
** Contact: John Brooks <john.brooks@jolla.com>
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

#ifndef LASTDIALEDCACHE_H
#define LASTDIALEDCACHE_H

#include <QObject>
#include <CommHistory/CallModel>

namespace RTComLogger {

/* Write the last dialed number from the call log to a cache file,
 * and update the number when items are removed from the call log.
 *
 * This is a hack to provide functionality necessary for bluez,
 * which is otherwise unable to access commhistory data in any sane way.
 */
class LastDialedCache : public QObject
{
    Q_OBJECT

public:
    LastDialedCache(QObject *parent);

private slots:
    void onRowsInserted(const QModelIndex &parent, int start, int end);
    void onRowsRemoved(const QModelIndex &parent, int start, int end);
    void onModelReset();

private:
    QString filePath;
    CommHistory::CallModel *model;

    void writeLastDialed(const QString &number);
    void removeLastDialed();
};

}

#endif
