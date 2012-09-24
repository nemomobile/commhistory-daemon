/*
 * This file is part of TelepathyQt4
 *
 * Copyright (C) 2008, 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2008, 2009 Nokia Corporation
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

#ifndef _TelepathyQt4_connection_h_HEADER_GUARD_
#define _TelepathyQt4_connection_h_HEADER_GUARD_

#include "Contact"
#include "DBusProxy"
#include "ReadyObject"
#include "Types"
#include "ReferencedHandles"
#include "Constants"
#include "AbstractInterface"

#include <QSet>
#include <QString>
#include <QStringList>

namespace Tp
{

class ContactManager;

namespace Client
{
namespace DBus
{
    class PropertiesInterface : public Tp::AbstractInterface
    {
        Q_OBJECT

    public:
        static inline const char *staticInterfaceName()
        {
            return "org.freedesktop.DBus.Properties";
        }

        PropertiesInterface(){}

    public Q_SLOTS:
        inline QDBusPendingReply<QDBusVariant> Get(const QString& interfaceName, const QString& propertyName)
        {
            QList<QVariant> argumentList;
            argumentList << QVariant::fromValue(interfaceName) << QVariant::fromValue(propertyName);
            return asyncCallWithArgumentList(QLatin1String("Get"), argumentList);
        }
    };
}
}

class  Connection : public StatefulDBusProxy
{
    Q_OBJECT
    Q_DISABLE_COPY(Connection)
    Q_ENUMS(Status)

public:

    Connection(const QString &objectPath = QString());

    template <typename Interface>
    inline Interface *interface()
    {
        if (mIfs.contains(Interface::staticInterfaceName()))
            return static_cast<Interface*>(mIfs.value(Interface::staticInterfaceName()));

        Interface *newIf = new Interface;
        mIfs.insert(Interface::staticInterfaceName(), newIf);
        return newIf;
    }

    template <typename Interface>
    inline Interface *optionalInterface() {
        return interface<Interface>();
    }

    static const Feature FeatureCore;
    static const Feature FeatureSimplePresence;

    virtual ~Connection();

    QStringList interfaces() const;

    bool hasInterface(const char *);

    ContactManagerPtr contactManager() const;

    uint selfHandle() const;

public: // ut
    void ut_setInterfaces(const QStringList& interfaces);
    void ut_setSelfHandle(uint handle);

private:
    struct Private;
    friend struct Private;
    Private *mPriv;
    QHash<QString, QObject*> mIfs;
};

} // Tp

#endif
