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

    QObject* object();
    void ut_setObject( QObject* obj );

    bool isReady() const;

    Tp::PendingReady* becomeReady(const Features &requestedFeatures = Features());

    void ut_setPendingDelay( const quint32 delay );


Q_SIGNALS:

    void ut_validityChanged( bool );


protected:

    quint32 m_pendingDelay;

private:

    bool m_isReadyFuse;
    // for getting the the derived object
    QObject* m_pObject;

};

};
#endif // PENDINGREADY_H
