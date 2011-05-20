/* *** This file is part of commhistory-daemon. ***
 *
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 * All rights reserved.
 *
 * Contact: Alvi Hirvela <alvi.hirvela@nokia.com>
 *
 * This software, including documentation, is protected by copyright
 * controlled by Nokia Corporation. All rights are reserved. Copying,
 * including reproducing, storing, adapting or translating, any or all of
 * this material requires the prior written consent of Nokia Corporation.
 * This material also contains confidential information which may not be
 * disclosed to others without the prior written consent of Nokia.
 */

#ifndef OLDDATADELETER_H
#define OLDDATADELETER_H

#include <QObject>

#define DAYS_TO_MS(days) days*24*60*60*1000

class QTimer;
class QModelIndex;

namespace RTComLogger
{
    class AccountSpecificCallModel;
}

namespace RTComLogger
{

class OldDataDeleter : public QObject
{
    Q_OBJECT

public:
    OldDataDeleter(QObject *parent = 0);
    ~OldDataDeleter();

private Q_SLOTS:
    void slotCleanUp();
    void slotDeleteCalls();
    void slotRowsRemoved(const QModelIndex& index, int start, int end);

private:
    QTimer *m_pCleanUpTimer;
    RTComLogger::AccountSpecificCallModel *m_pCallModel;
};

} // namespace RTComLogger

#endif
