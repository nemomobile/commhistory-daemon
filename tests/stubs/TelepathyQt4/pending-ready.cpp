#include "pending-ready.h"

#include <QTimer>

Tp::PendingReady::PendingReady()
        :Tp::PendingOperation()
        ,m_pendingDelay ( 1000 )
        ,m_isReadyFuse (false)
        ,m_pObject ( 0 )
{
}

QObject* Tp::PendingReady::object()
{
    return m_pObject;
}

void Tp::PendingReady::ut_setObject(QObject* obj)
{
    m_pObject = obj;
}

bool Tp::PendingReady::isReady() const
{
    return m_isReadyFuse;
}

Tp::PendingReady* Tp::PendingReady::becomeReady(const Features &requestedFeatures)
{
    Q_UNUSED( requestedFeatures );
    m_isReadyFuse = true;
    emit ut_validityChanged( m_isReadyFuse );
    QTimer::singleShot(m_pendingDelay, this, SLOT(fakeAsyncFinish()) );
    return this;
}

void Tp::PendingReady::ut_setPendingDelay( const quint32 delay )
{
    if( ( delay > 0 ) && ( delay != m_pendingDelay ) ) {
        m_pendingDelay = delay;
    }
}
