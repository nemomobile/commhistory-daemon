/*
 * This file is part of TelepathyQt4
 *
 * Copyright (C) 2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2008 Nokia Corporation
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

#ifndef _TelepathyQt4_channel_h_HEADER_GUARD_
#define _TelepathyQt4_channel_h_HEADER_GUARD_


#include "Constants"
#include "Contact"
#include "DBusProxy"
#include "Types"
#include "ReadyObject"
#include "AbstractInterface"
#include "Properties"

#include <QSet>
#include <QSharedDataPointer>
#include <QVariantMap>

namespace Tp
{

class Connection;
class PendingOperation;
class PendingReady;

namespace Client {
    class ChannelInterface : public Tp::AbstractInterface {
    public:
        static inline const char *staticInterfaceName()
        {
            return "org.freedesktop.Telepathy.Channel";
        }
    };

    class ChannelInterfaceServicePointInterface : public Tp::AbstractInterface
    {
        Q_OBJECT

    public:
        static inline const char *staticInterfaceName()
        {
            return "org.freedesktop.Telepathy.Channel.Interface.ServicePoint";
        }

        ChannelInterfaceServicePointInterface() {};

    public:

    //    inline Tp::PendingVariant *requestPropertyCurrentServicePoint() const
    //    {
    //        return internalRequestProperty(QLatin1String("CurrentServicePoint"));
    //    }

    //    Tp::PendingVariantMap *requestAllProperties() const
    //    {
    //        return internalRequestAllProperties();
    //    }

    Q_SIGNALS:
        void ServicePointChanged(const Tp::ServicePoint& servicePoint);
    };

}

class Channel : public StatefulDBusProxy
{
    Q_OBJECT
    Q_DISABLE_COPY(Channel)

public:

    Channel(const QString &objectPath = QString());

    QStringList interfaces() const;

    template <typename Interface>
    inline Interface *interface()
    {
        if (!m_interfaces.contains(Interface::staticInterfaceName()))
            return 0;

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

    virtual ~Channel();

    ConnectionPtr connection() const;

    QVariantMap immutableProperties() const;

    QString channelType() const;

    uint targetHandleType() const;
    uint targetHandle() const;

    bool isRequested() const;

    /**
     * TODO: have parameters on these like
     * Contacts groupContacts(bool includeSelfContact = true);
     */
    Contacts groupContacts() const;

    class GroupMemberChangeDetails
    {
    public:
        GroupMemberChangeDetails(){};
        GroupMemberChangeDetails(const ContactPtr &actor, uint reason) :
                m_actor(actor), m_reason(reason){}

        ContactPtr actor() const {return m_actor;};
        uint reason() const {return m_reason;};

    private:
        friend class Channel;

        ContactPtr m_actor;
        uint m_reason;
    };

    bool groupAreHandleOwnersAvailable() const;
    HandleOwnerMap groupHandleOwners() const;

    ContactPtr groupSelfContact() const;

Q_SIGNALS:
    void groupMembersChanged(
            const Tp::Contacts &groupMembersAdded,
            const Tp::Contacts &groupLocalPendingMembersAdded,
            const Tp::Contacts &groupRemotePendingMembersAdded,
            const Tp::Contacts &groupMembersRemoved,
            const Tp::Channel::GroupMemberChangeDetails &details);

    void groupHandleOwnersChanged(const Tp::HandleOwnerMap &owners,
            const Tp::UIntList &added, const Tp::UIntList &removed);


public: // stub methods
    void ut_setInterfaces(const QStringList& interfaces);
    void ut_setChannelType(const QString& channelType);
    void ut_setTargetHandleType(uint targetHandleType);
    void ut_setTargetHandle(uint targetHandle);
    void ut_setGroupContacts(const Contacts& contacts);
    void ut_setGroupAreHandleOwnersAvailable(bool handlesAvailable);
    void ut_setGroupHandleOwners(const HandleOwnerMap& ownerMap);
    void ut_setGroupSelfContact(const ContactPtr& selfContact);
    void ut_setImmutableProperties(const QVariantMap &immutableProperties);
    void ut_setConnection(const ConnectionPtr &connection);
    void ut_setIsRequested(bool requested);
    void ut_emitGroupMembersChanged(const Tp::Contacts &groupMembersAdded,
                                    const Tp::Contacts &groupLocalPendingMembersAdded,
                                    const Tp::Contacts &groupRemotePendingMembersAdded,
                                    const Tp::Contacts &groupMembersRemoved,
                                    const Tp::Channel::GroupMemberChangeDetails &details)
    {
        emit groupMembersChanged(groupMembersAdded,
                                 groupLocalPendingMembersAdded,
                                 groupRemotePendingMembersAdded,
                                 groupMembersRemoved,
                                 details);
    }

private:
    struct Private;
    friend struct Private;
    QSharedDataPointer<Private> mPriv;
    QStringList m_interfaces;
    QHash<QString, QObject*> mIfs;
};

} // Tp

Q_DECLARE_METATYPE(Tp::Channel::GroupMemberChangeDetails);

#endif
