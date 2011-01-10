#ifndef PENDINGREADY_H
#define PENDINGREADY_H

#include "PendingOperation"
#include "Types"

namespace Tp
{

class PendingReady : public PendingOperation
{
    Q_OBJECT
public:
    PendingReady();
    ~PendingReady();

    DBusProxyPtr proxy() const;

    void ut_setProxy(DBusProxyPtr &proxy);

private:
    struct Private;
    friend struct Private;
    Private *mPriv;
};

};
#endif // PENDINGREADY_H
