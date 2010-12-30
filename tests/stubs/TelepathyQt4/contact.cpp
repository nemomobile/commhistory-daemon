#include "contact.h"
#include "ReferencedHandles"
#include "Presence"

namespace Tp
{

struct Contact::Private
{
    QString m_id;
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


Presence Contact::presence() const
{
    return mPriv->m_presence;
}

void Contact::ut_setId(const QString& id)
{
    mPriv->m_id = id;
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
