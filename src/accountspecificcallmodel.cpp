/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Alvi Hirvela <alvi.hirvela@nokia.com>
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
#include <QDebug>

#include <CommHistory/eventsquery.h>
#include "accountspecificcallmodel.h"
#include "accountspecificcallmodel_p.h"

using namespace RTComLogger;
using namespace CommHistory;

/* ************************************************************************** *
 * ******** P R I V A T E   C L A S S   I M P L E M E N T A T I O N ********* *
 * ************************************************************************** */

AccountSpecificCallModelPrivate::AccountSpecificCallModelPrivate(EventModel *model)
        : EventModelPrivate(model)
        , filterAccountPath(QString())
{
}


bool AccountSpecificCallModelPrivate::acceptsEvent(const Event &event) const
{
    qDebug() << __PRETTY_FUNCTION__ << event.id();

    if (event.type() != Event::CallEvent)
        return false;

    if (!filterAccountPath.isEmpty() && event.localUid() != filterAccountPath)
        return false;

    return true;
}

/* ************************************************************************** *
 * ********* P U B L I C   C L A S S   I M P L E M E N T A T I O N ********** *
 * ************************************************************************** */

AccountSpecificCallModel::AccountSpecificCallModel(QObject *parent)
        : EventModel(*new AccountSpecificCallModelPrivate(this), parent)
{
    qDebug() << __PRETTY_FUNCTION__;
}

AccountSpecificCallModel::~AccountSpecificCallModel()
{
    qDebug() << __PRETTY_FUNCTION__;
}

bool AccountSpecificCallModel::getEvents(QString accountPath)
{
    Q_D(AccountSpecificCallModel);

    qDebug() << __PRETTY_FUNCTION__ << "Account path: " << accountPath;

    d->filterAccountPath = accountPath;

    reset();
    d->clearEvents();

    EventsQuery query(d->propertyMask);
    query.addPattern(QLatin1String("%1 a nmo:Call .")).variable(Event::Id);
    query.addPattern(QString(QLatin1String("{%2 nmo:to [nco:hasContactMedium <telepathy:%1>]} "
                                           "UNION "
                                           "{%2 nmo:from [nco:hasContactMedium <telepathy:%1>]}"))
                     .arg(accountPath))
                     .variable(Event::Id);

    return d->executeQuery(query);
}
