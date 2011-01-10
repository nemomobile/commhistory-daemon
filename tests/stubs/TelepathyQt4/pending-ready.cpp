#include "pending-ready.h"

#include "DBusProxy"

namespace Tp
{

struct PendingReady::Private
{
    DBusProxyPtr proxy;
};

PendingReady::PendingReady():
        mPriv(new Private())
{
}

PendingReady::~PendingReady()
{
    delete mPriv;
}

DBusProxyPtr PendingReady::proxy() const
{
    return mPriv->proxy;
}

void PendingReady::ut_setProxy(DBusProxyPtr &proxy)
{
    mPriv->proxy = proxy;
}

} //Tp
