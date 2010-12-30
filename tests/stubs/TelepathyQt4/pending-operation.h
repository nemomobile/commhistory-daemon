#ifndef _TelepathyQt4_pendingoperation_h_HEADER_GUARD_
#define _TelepathyQt4_pendingoperation_h_HEADER_GUARD_

#include <QObject>

namespace Tp
{
    class PendingOperation : public QObject
    {
        Q_OBJECT
    public:

        PendingOperation();
        bool isError();
        QString errorName() const;
        QString errorMessage() const;

    signals:

      void finished( Tp::PendingOperation* pOp );

    public slots:
        void fakeAsyncFinish();
    };
}

#endif
