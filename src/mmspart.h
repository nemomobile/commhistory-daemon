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

#ifndef MMSPART_H
#define MMSPART_H

#include <QtDBus>
#include <QString>

struct MmsPart
{
    QString fileName;
    QString contentType;
    QString contentId;
};

inline QDBusArgument& operator<<(QDBusArgument &arg, const MmsPart &part)
{
    arg.beginStructure();
    arg << part.fileName << part.contentType << part.contentId;
    arg.endStructure();
    return arg;
}

inline const QDBusArgument& operator>>(const QDBusArgument &arg, MmsPart &part)
{
    arg.beginStructure();
    arg >> part.fileName >> part.contentType >> part.contentId;
    arg.endStructure();
    return arg;
}

typedef QList<MmsPart> MmsPartList;

Q_DECLARE_METATYPE(MmsPart);
Q_DECLARE_METATYPE(MmsPartList);

#endif // MMSPART_H
