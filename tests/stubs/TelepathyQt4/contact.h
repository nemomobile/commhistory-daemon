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

#ifndef _TelepathyQt4_contact_h_HEADER_GUARD_
#define _TelepathyQt4_contact_h_HEADER_GUARD_

#include "Types"

#include <QObject>
#include <QSet>
#include <QSharedPointer>
#include <QVariantMap>

namespace Tp
{

class Presence;
class ReferencedHandles;

class Contact : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Contact);

public:
    static const Feature FeatureAlias;
    static const Feature FeatureAvatarData;
    static const Feature FeatureAvatarToken;
    static const Feature FeatureCapabilities;
    static const Feature FeatureInfo;
    static const Feature FeatureLocation;
    static const Feature FeatureSimplePresence;

    enum PresenceState {
         PresenceStateNo,
         PresenceStateAsk,
         PresenceStateYes
    };

    Contact();
    ~Contact();

    ReferencedHandles handle() const;
    QString id() const;
    QString alias() const;

    Presence presence() const;

Q_SIGNALS:
    void presenceChanged(const Tp::Presence &presence);

public: // ut
    void ut_setId(const QString& id);
    void ut_setAlias(const QString& alias);
    void ut_setHandle(const ReferencedHandles& handles);
    void ut_setHandle(uint handle);
    void ut_setPresence(const Presence& presence);
    void ut_emitPresenceChanged();

private:

    struct Private;
    friend class ContactManager;
    friend struct Private;
    Private *mPriv;
};

typedef QSet<ContactPtr> Contacts;

// FIXME: (API/ABI break) Remove once Contact is a SharedPtr and add a new qHash(SharedPtr<T>)
inline uint qHash(const ContactPtr &contact)
{
    return qHash(contact.data());
}

} // Tp

#endif
