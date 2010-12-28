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

//#include <TelepathyQt4/_gen/cli-connection.h>


#include "Contact"
//#include <TelepathyQt4/DBus>
#include "DBusProxy"
//#include <TelepathyQt4/OptionalInterfaceFactory>
//#include <TelepathyQt4/ReadinessHelper>
#include "ReadyObject"
#include "Types"
//#include <TelepathyQt4/SharedPtr>

#include "ReferencedHandles"

#include "Constants"
//#include <TelepathyQt4/Types>

#include <QSet>
#include <QString>
#include <QStringList>

namespace Tp
{

class Channel;
class ConnectionCapabilities;
class Contact;
class ContactManager;
class PendingChannel;
class PendingContactAttributes;
class PendingHandles;
class PendingOperation;
class PendingReady;

class  Connection : public StatefulDBusProxy,
                   /*public OptionalInterfaceFactory<Connection>,*/
                   public ReadyObject,
                   public RefCounted
{
    Q_OBJECT
    Q_DISABLE_COPY(Connection)
    Q_ENUMS(Status)

public:

    template <typename Interface>
    inline Interface *interface() const
    {
        return new Interface;
    }

    static const Feature FeatureCore;
    static const Feature FeatureSelfContact;
    static const Feature FeatureSimplePresence;
    static const Feature FeatureRoster;
    static const Feature FeatureRosterGroups;
    static const Feature FeatureAccountBalance; // TODO unit tests for this

    enum Status {
        StatusDisconnected = ConnectionStatusDisconnected,
        StatusConnecting = ConnectionStatusConnecting,
        StatusConnected = ConnectionStatusConnected,
        StatusUnknown = 0xFFFFFFFF
    };

    virtual ~Connection();

    QString cmName() const;
    QString protocolName() const;

    inline QStringList interfaces() const { return QStringList(); }

    inline bool hasInterface(const char *)
    {
        return true;
    }


    Status status() const;

    class ErrorDetails
    {
        public:
            ErrorDetails();
            ErrorDetails(const QVariantMap &details);
            ErrorDetails(const ErrorDetails &other);
            ~ErrorDetails();

            ErrorDetails &operator=(const ErrorDetails &other);

            bool isValid() const { return mPriv.constData() != 0; }

            bool hasDebugMessage() const
            {
                return allDetails().contains(QLatin1String("debug-message"));
            }

            QString debugMessage() const
            {
                return qdbus_cast<QString>(allDetails().value(QLatin1String("debug-message")));
            }

            QVariantMap allDetails() const;

        private:
            friend class Connection;

            struct Private;
            friend struct Private;
            QSharedDataPointer<Private> mPriv;
    };

    const ErrorDetails &errorDetails() const;

    uint selfHandle() const;
    ContactPtr selfContact() const;

    PendingOperation *setSelfPresence(const QString &status, const QString &statusMessage);

    ConnectionCapabilities *capabilities() const;

    PendingChannel *createChannel(const QVariantMap &request);
    PendingChannel *ensureChannel(const QVariantMap &request);

    PendingReady *requestConnect(const Features &requestedFeatures = Features());
    PendingOperation *requestDisconnect();

    PendingHandles *requestHandles(uint handleType, const QStringList &names);
    PendingHandles *referenceHandles(uint handleType, const UIntList &handles);

    PendingContactAttributes *contactAttributes(const UIntList &handles,
            const QStringList &interfaces, bool reference = true);
    QStringList contactAttributeInterfaces() const;
    ContactManager *contactManager() const;

Q_SIGNALS:
    void statusChanged(Tp::Connection::Status newStatus);

    void selfHandleChanged(uint newHandle);
    // FIXME: might not need this when Renaming is fixed and mapped to Contacts
    void selfContactChanged();

private:
    class PendingConnect;
    friend class PendingChannel;
    friend class PendingConnect;
    friend class PendingContactAttributes;
    friend class PendingContacts;
    friend class PendingHandles;
    friend class ReferencedHandles;

    void refHandle(uint type, uint handle);
    void unrefHandle(uint type, uint handle);
    void handleRequestLanded(uint type);

    struct Private;
    friend struct Private;
    Private *mPriv;
};

} // Tp

Q_DECLARE_METATYPE(Tp::Connection::ErrorDetails);

#endif
