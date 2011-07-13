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

#include <QMetaProperty>
#include <QVariant>

#include "serialisable.h"

using namespace RTComLogger;

Serialisable::Serialisable()
{
}

Serialisable::~Serialisable()
{
}

void Serialisable::serialize(QDataStream& out, const QObject& object) const
{
    for(int i = 1; i < object.metaObject()->propertyCount(); i++) {
       QMetaProperty prop = object.metaObject()->property(i);
       const char* propName = prop.name();
       out << object.property(propName);
    }
}

void Serialisable::deSerialize(QDataStream& in, QObject& object)
{
    for(int i = 1; i < object.metaObject()->propertyCount(); i++) {
       QMetaProperty prop = object.metaObject()->property(i);
       const char* propName = prop.name();
       QVariant variant;
       in >> variant;
       object.setProperty(propName, variant);
    }
}
