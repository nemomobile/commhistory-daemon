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

#ifndef ACCOUNTSPECIFIC_CALLMODEL_P_H
#define ACCOUNTSPECIFIC_CALLMODEL_P_H

#include <CommHistory/eventmodel_p.h>

#include "accountspecificcallmodel.h"

namespace RTComLogger
{

using namespace CommHistory;

class AccountSpecificCallModelPrivate : public EventModelPrivate
{
    Q_OBJECT
    Q_DECLARE_PUBLIC( AccountSpecificCallModel );

public:
    AccountSpecificCallModelPrivate( EventModel *model );

    bool acceptsEvent( const Event &event ) const;

public:
    QString filterAccountPath;
    QDateTime filterReferenceDate;
};

}

#endif
