/*
 * This file is part of TelepathyQt4
 *
 * Copyright (C) 2008-2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2008-2009 Nokia Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _TelepathyQt4_dbus_proxy_h_HEADER_GUARD_
#define _TelepathyQt4_dbus_proxy_h_HEADER_GUARD_

#include "ready-object.h"

class QDBusConnection;
class QDBusError;

namespace Tp
{

class DBusProxy : public Object, public ReadyObject
{
    Q_OBJECT
    Q_DISABLE_COPY(DBusProxy)

public:
    DBusProxy(const QString &objectPath = QString()) : ReadyObject(),
    m_objectPath(objectPath){}
    virtual ~DBusProxy();

    QString objectPath() const {return m_objectPath;}
    bool isValid() const {return m_isValid;}

Q_SIGNALS:
    void invalidated(Tp::DBusProxy *proxy,
            const QString &errorName, const QString &errorMessage);

public: // ut
    void ut_setIsValid(bool valid)
    {
        m_isValid=valid;
    }

    void ut_invalidate(const QString &errorName, const QString &errorMessage)
    {
        ut_setIsValid(false);
        emit invalidated(this, errorName, errorMessage);
    }

private:
    QString m_objectPath;
    bool m_isValid;
};

class StatelessDBusProxy : public DBusProxy
{
    Q_OBJECT
    Q_DISABLE_COPY(StatelessDBusProxy)

public:
    StatelessDBusProxy(const QString &objectPath = QString())
        : DBusProxy(objectPath){}
};

class StatefulDBusProxy : public DBusProxy
{
    Q_OBJECT
    Q_DISABLE_COPY(StatefulDBusProxy)

public:
    StatefulDBusProxy(const QString &objectPath = QString())
    : DBusProxy(objectPath){}
};

} // Tp

#endif
