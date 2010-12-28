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


#include "cli-properties.h"

#include "Constants"
#include "Contact"
#include "DBusProxy"
#include "Types"
#include "ReadyObject"
#include "AbstractInterface"

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
    };
}

class Channel : public StatefulDBusProxy,
                public ReadyObject,
                public RefCounted
{
    Q_OBJECT
    Q_DISABLE_COPY(Channel)

public:

    template <typename Interface>
    inline Interface *interface() const
    {
        return new Interface;
    }

    QStringList interfaces() const {return QStringList();}

    static const Feature FeatureCore;
    static const Feature FeatureConferenceInitialInviteeContacts;

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
        GroupMemberChangeDetails();
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
    void ut_setImmutableProperties(const QVariantMap &immutableProperties);
    void ut_setConnection(const ConnectionPtr &connection);

private:
    struct Private;
    friend struct Private;
    QSharedDataPointer<Private> mPriv;
};

} // Tp

Q_DECLARE_METATYPE(Tp::Channel::GroupMemberChangeDetails);

#endif
