#include "ready-object.h"

#include <QTimer>

Tp::ReadyObject::ReadyObject():
        m_isReady(false),
        m_pendingDelay(0),
        m_pPendingReady(0)
{
}

Tp::PendingReady* Tp::ReadyObject::becomeReady(const Features &requestedFeatures)
{
    Q_UNUSED( requestedFeatures );
    m_isReady = true;
    m_pPendingReady = new PendingReady();
    QTimer::singleShot(m_pendingDelay, m_pPendingReady, SLOT(fakeAsyncFinish()) );
    return m_pPendingReady;
}

void Tp::ReadyObject::ut_setPendingDelay( const quint32 delay )
{
    if( ( delay > 0 ) && ( delay != m_pendingDelay ) ) {
        m_pendingDelay = delay;
    }
}

Tp::PendingReady* Tp::ReadyObject::ut_pendingReady()
{
    return m_pPendingReady;
}
