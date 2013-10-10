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

#include "lastdialedcache.h"
#include "debug.h"
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>

using namespace RTComLogger;
using namespace CommHistory;

LastDialedCache::LastDialedCache(QObject *parent)
    : QObject(parent)
{
    filePath = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/last-dialed";

    model = new CallModel(this);
    connect(model, SIGNAL(modelReset()), SLOT(onModelReset()));
    connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)), SLOT(onRowsInserted(QModelIndex,int,int)));
    connect(model, SIGNAL(rowsRemoved(QModelIndex,int,int)), SLOT(onRowsRemoved(QModelIndex,int,int)));

    model->setTreeMode(false);
    model->setSorting(CallModel::SortByTime);
    model->setFilterType(CallEvent::DialedCallType);
    model->setLimit(1);
    model->getEvents();
}

void LastDialedCache::onRowsInserted(const QModelIndex &parent, int start, int end)
{
    Q_UNUSED(parent);
    Q_UNUSED(end);

    if (start == 0)
        writeLastDialed(model->event(model->index(0, 0)).remoteUid());
}

void LastDialedCache::onRowsRemoved(const QModelIndex &parent, int start, int end)
{
    Q_UNUSED(parent);
    Q_UNUSED(end);

    if (model->rowCount() == 0)
        removeLastDialed();
    else if (start == 0)
        writeLastDialed(model->event(model->index(0, 0)).remoteUid());
}

void LastDialedCache::onModelReset()
{
    if (model->rowCount() == 0)
        removeLastDialed();
    else
        writeLastDialed(model->event(model->index(0, 0)).remoteUid());
}

void LastDialedCache::writeLastDialed(const QString &number)
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Cannot open last dialed cache file:" << file.errorString();
        return;
    }

    DEBUG() << "Writing last dialed number to file:" << number;
    file.write(number.toLatin1());
    if (!file.commit())
        qWarning() << "Writing last dialed cache failed:" << file.errorString();
}

void LastDialedCache::removeLastDialed()
{
    DEBUG() << "Removing last dialed number file";
    QFile::remove(filePath);
}

