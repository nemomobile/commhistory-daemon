
#include "pending-operation.h"

#include <QString>
#include <QDebug>

Tp::PendingOperation::PendingOperation()
{
}

bool Tp::PendingOperation::isError()
{
    return false;
//    return true;
}

QString Tp::PendingOperation::errorName() const
{
//    return QString();
    return "#### FakeError";
}

QString Tp::PendingOperation::errorMessage() const
{
//    return QString();
    return "#### FakeError-Message";
}

void Tp::PendingOperation::fakeAsyncFinish()
{
//    qDebug() << "######### Fake finished";
    emit finished( this );
}
