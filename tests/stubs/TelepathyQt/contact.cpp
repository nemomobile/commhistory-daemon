#include "contact.h"
#include "ReferencedHandles"
#include "Presence"

namespace Tp
{

    const Feature Contact::FeatureAlias = Feature(QLatin1String(Contact::staticMetaObject.className()), 0, false);
    const Feature Contact::FeatureAvatarData = Feature(QLatin1String(Contact::staticMetaObject.className()), 1, false);
    const Feature Contact::FeatureAvatarToken = Feature(QLatin1String(Contact::staticMetaObject.className()), 2, false);
    const Feature Contact::FeatureCapabilities = Feature(QLatin1String(Contact::staticMetaObject.className()), 3, false);
    const Feature Contact::FeatureInfo = Feature(QLatin1String(Contact::staticMetaObject.className()), 4, false);
    const Feature Contact::FeatureLocation = Feature(QLatin1String(Contact::staticMetaObject.className()), 5, false);
    const Feature Contact::FeatureSimplePresence = Feature(QLatin1String(Contact::staticMetaObject.className()), 6, false);


struct Contact::Private
{
    QString m_id;
    QString m_alias;
    ReferencedHandles m_referencedHandles;
    Presence m_presence;
};


Contact::Contact() : mPriv(new Private)
{}

Contact::~Contact()
{
    delete mPriv;
}

ReferencedHandles Contact::handle() const
{
    return mPriv->m_referencedHandles;
}

QString Contact::id() const
{
    return mPriv->m_id;
}

QString Contact::alias() const
{
    if (mPriv->m_alias.isEmpty())
        return mPriv->m_id;
    else
        return mPriv->m_alias;
}


Presence Contact::presence() const
{
    return mPriv->m_presence;
}

void Contact::ut_setId(const QString& id)
{
    mPriv->m_id = id;
}

void Contact::ut_setAlias(const QString& alias)
{
    mPriv->m_alias = alias;
}

void Contact::ut_setHandle(const ReferencedHandles& handles)
{
    mPriv->m_referencedHandles = handles;
}

void Contact::ut_setHandle(uint handle)
{
    mPriv->m_referencedHandles = Tp::ReferencedHandles(HandleTypeContact,
                                                UIntList() << handle);
}

void Contact::ut_setPresence(const Presence& presence)
{
    mPriv->m_presence = presence;
}

void Contact::ut_emitPresenceChanged()
{
    emit presenceChanged(mPriv->m_presence);
}

}
