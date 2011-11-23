/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Reto Zingg <reto.zingg@nokia.com>
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the GNU Lesser General Public License version 2.1 as
** published by the Free Software Foundation.
**
** This library is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
** or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
** License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this library; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
**
******************************************************************************/

// INCLUDES
#include "ut_notificationmanager.h"
#include "locstrings.h"
#include "constants.h"
#include "mwilistener.h"

// Qt includes
#include <QDebug>
#include <QTest>
#include <QDateTime>
#include <MNotification>
#include <MNotificationGroup>

#define CONTACT_1_REMOTE_ID QLatin1String("td@localhost")
#define CONTACT_2_REMOTE_ID QLatin1String("td2@localhost")
#define CONTACT_1_ID 1
#define CONTACT_2_ID 2
#define CONTACT_ID_NONE -1
#define DUT_ACCOUNT_PATH QLatin1String("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0")
#define MESSAGE_TEXT QLatin1String("Testing notifications!")

using namespace RTComLogger;

namespace {
    void justWait(int msec)
    {
        QTime timer;
        timer.start();
        while (timer.elapsed() < msec)
            QCoreApplication::processEvents();
    }
}

MNotificationGroup* Ut_NotificationManager::getGroup(int eventType, int msec)
{
    QTime timer;
    timer.start();
    while (timer.elapsed() <= msec) {
        QList<MNotificationGroup *> mgtGroups = MNotificationGroup::notificationGroups();
        foreach(MNotificationGroup *mgtGroup, mgtGroups)
            if (mgtGroup->eventType() == NotificationManager::eventType(eventType))
                return mgtGroup;

        QCoreApplication::processEvents();
    }

    return 0;
}

void Ut_NotificationManager::removeGroup(int eventType)
{
    QList<MNotificationGroup *> mgtGroups = MNotificationGroup::notificationGroups();
    foreach(MNotificationGroup *mgtGroup, mgtGroups)
        if (mgtGroup->eventType() == NotificationManager::eventType(eventType))
            mgtGroup->remove();
}

Ut_NotificationManager::Ut_NotificationManager() : eventId(1)
{
}

Ut_NotificationManager::~Ut_NotificationManager()
{
}

/*!
 * This function will be called before the first testfunction is executed.
 */
void Ut_NotificationManager::initTestCase()
{
    nm = NotificationManager::instance();
    nm->m_Notifications.clear();

    nm->m_pMWIListener->disconnect(nm);
}

/*!
 * This function will be called after the last testfunction was executed.
 */
void Ut_NotificationManager::cleanupTestCase()
{
    delete nm;
    nm = 0;

}

/*!
 * This function will be called before each testfunction is executed.
 */
void Ut_NotificationManager::init()
{
}

/*!
 * This unction will be called after every testfunction.
 */
void Ut_NotificationManager::cleanup()
{
}

CommHistory::Event Ut_NotificationManager::createImEvent(const QString& remoteUid, int contactId)
{
    eventId++;
    CommHistory::Event event;
    event.setType(CommHistory::Event::IMEvent);
    event.setDirection(CommHistory::Event::Inbound);
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setLocalUid(DUT_ACCOUNT_PATH);
    event.setRemoteUid(remoteUid);
    event.setFreeText(MESSAGE_TEXT);
    event.setMessageToken(MESSAGE_TEXT + QString::number(eventId));
    event.setContactId(contactId);
    event.setGroupId(1);
    event.setId(eventId);
    return event;
}

PersonalNotification Ut_NotificationManager::createPersonalNotification(const CommHistory::Event &event)
{
    return PersonalNotification(event.remoteUid(), event.localUid(), event.type(),
                                event.remoteUid(), CommHistory::Group::ChatTypeP2P,
                                event.contactId());
}

CommHistory::Event Ut_NotificationManager::createMissedCallEvent(const QString& remoteUid, int contactId)
{
    CommHistory::Event event;
    event.setType(CommHistory::Event::CallEvent);
    event.setDirection(CommHistory::Event::Inbound);
    event.setIsMissedCall (true);
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setLocalUid(DUT_ACCOUNT_PATH);
    event.setRemoteUid(remoteUid);
    event.setContactId(contactId);
    event.setId(eventId);
    eventId++;
    return event;
}

void Ut_NotificationManager::testAddImNotification()
{
    CommHistory::Event event = createImEvent(CONTACT_1_REMOTE_ID, CONTACT_1_ID);
    // One notification from contact 1
    NotificationManager* nm = NotificationManager::instance();
    nm->addNotification(createPersonalNotification(event));
    NotificationGroup group = nm->notificationGroup(event.type());

    QVERIFY(nm->countContacts(group) == 1);
    QVERIFY(nm->countNotifications(group) == 1);
    QVERIFY(nm->removeNotificationGroup(event.type()));
    QVERIFY(nm->countContacts(group) == 0);
    QVERIFY(nm->countNotifications(group) == 0);

    // Multiple notifications from contact 1
    CommHistory::Event event1 = createImEvent(CONTACT_1_REMOTE_ID, CONTACT_1_ID);
    CommHistory::Event event2 = createImEvent(CONTACT_1_REMOTE_ID, CONTACT_1_ID);
    nm->addNotification(createPersonalNotification(event1));
    nm->addNotification(createPersonalNotification(event2));
    QVERIFY(nm->countContacts(group) == 1);
    QVERIFY(nm->countNotifications(group) == 2);
    QVERIFY(nm->removeNotificationGroup(event1.type()));
    QVERIFY(nm->countContacts(group) == 0);
    QVERIFY(nm->countNotifications(group) == 0);
}

void Ut_NotificationManager::testAddGroupImNotification()
{
    NotificationManager* nm = NotificationManager::instance();
    CommHistory::Event event1 = createImEvent(CONTACT_1_REMOTE_ID, CONTACT_1_ID);
    CommHistory::Event event2 = createImEvent(CONTACT_2_REMOTE_ID, CONTACT_2_ID);
    nm->addNotification(createPersonalNotification(event1));
    nm->addNotification(createPersonalNotification(event2));
    NotificationGroup group = nm->notificationGroup(event1.type());

    QVERIFY(nm->countContacts(group) == 2);
    QVERIFY(nm->countNotifications(group) == 2);
    QVERIFY(nm->removeNotificationGroup(event1.type()));
    QVERIFY(nm->countContacts(group) == 0);
    QVERIFY(nm->countNotifications(group) == 0);
}

void Ut_NotificationManager::testAddMissedCallNotification()
{
    CommHistory::Event event = createMissedCallEvent(CONTACT_1_REMOTE_ID, CONTACT_1_ID);
    // One notification from contact 1
    NotificationManager* nm = NotificationManager::instance();
    nm->addNotification(createPersonalNotification(event));
    NotificationGroup group = nm->notificationGroup(event.type());

    QVERIFY(nm->countNotifications(group) == 1);
    QVERIFY(nm->countContacts(group) == 1);
    QVERIFY(nm->removeNotificationGroup(event.type()));
    QVERIFY(nm->countNotifications(group) == 0);
    QVERIFY(nm->countContacts(group) == 0);

    // Multiple notifications from contact 1
    CommHistory::Event event1 = createMissedCallEvent(CONTACT_1_REMOTE_ID, CONTACT_1_ID);
    CommHistory::Event event2 = createMissedCallEvent(CONTACT_1_REMOTE_ID, CONTACT_1_ID);
    nm->addNotification(createPersonalNotification(event1));
    nm->addNotification(createPersonalNotification(event2));
    QVERIFY(nm->countNotifications(group) == 2);
    QVERIFY(nm->countContacts(group) == 1);
    QVERIFY(nm->removeNotificationGroup(event1.type()));
    QVERIFY(nm->countNotifications(group) == 0);
    QVERIFY(nm->countContacts(group) == 0);
}

void Ut_NotificationManager::testAddGroupMissedCallNotification()
{
    NotificationManager* nm = NotificationManager::instance();
    CommHistory::Event event1 = createMissedCallEvent(CONTACT_1_REMOTE_ID, CONTACT_1_ID);
    CommHistory::Event event2 = createMissedCallEvent(CONTACT_2_REMOTE_ID, CONTACT_2_ID);
    nm->addNotification(createPersonalNotification(event1));
    nm->addNotification(createPersonalNotification(event2));
    NotificationGroup group = nm->notificationGroup(event1.type());

    QVERIFY(nm->countNotifications(group) == 2);
    QVERIFY(nm->countContacts(group) == 2);
    QVERIFY(nm->removeNotificationGroup(event1.type()));
    QVERIFY(nm->countNotifications(group) == 0);
    QVERIFY(nm->countContacts(group) == 0);

}

void Ut_NotificationManager::testRemoveNotificationGrouop()
{
    NotificationManager* nm = NotificationManager::instance();

    // One IM notification from contact 1
    CommHistory::Event event = createImEvent(CONTACT_1_REMOTE_ID, CONTACT_1_ID);
    nm->addNotification(createPersonalNotification(event));

    // 1st time to remove NotificationGroup, return true
    QVERIFY(nm->removeNotificationGroup(event.type()));
    // 2nd time to remove the same NotificationGroup, return false
    QVERIFY(!nm->removeNotificationGroup(event.type()));

    //Mutile IM notifications from contact 1
    CommHistory::Event event1 = createMissedCallEvent(CONTACT_1_REMOTE_ID, CONTACT_1_ID);
    CommHistory::Event event2 = createMissedCallEvent(CONTACT_1_REMOTE_ID, CONTACT_1_ID);
    nm->addNotification(createPersonalNotification(event1));
    nm->addNotification(createPersonalNotification(event2));

    // 1st time to remove NotificationGroup, return true
    QVERIFY(nm->removeNotificationGroup(event1.type()));
    // 2nd time to remove the same NotificationGroup, return false
    QVERIFY(!nm->removeNotificationGroup(event1.type()));
}

void Ut_NotificationManager::testSaveAndLoadNotificationState()
{
    NotificationManager* nm = NotificationManager::instance();
    CommHistory::Event event1 = createImEvent(CONTACT_1_REMOTE_ID, CONTACT_1_ID);
    CommHistory::Event event2 = createImEvent(CONTACT_1_REMOTE_ID, CONTACT_1_ID);
    CommHistory::Event event3 = createImEvent(CONTACT_2_REMOTE_ID, CONTACT_2_ID);
    CommHistory::Event event4 = createImEvent(CONTACT_2_REMOTE_ID, CONTACT_2_ID);
    nm->addNotification(createPersonalNotification(event1));
    nm->addNotification(createPersonalNotification(event2));
    nm->addNotification(createPersonalNotification(event3));
    nm->addNotification(createPersonalNotification(event4));
    nm->saveState();
    nm->m_Notifications.clear();
    nm->loadState();

    NotificationGroup group = nm->notificationGroup(event1.type());

    QVERIFY(nm->countContacts(group) == 2);
    QVERIFY(nm->countNotifications(group) == 4);

    QVERIFY(nm->removeNotificationGroup(event1.type()));

    QVERIFY(nm->countContacts(group) == 0);
    QVERIFY(nm->countNotifications(group) == 0);
    nm->saveState();
    nm->m_Notifications.clear();
    nm->loadState();
    QVERIFY(nm->m_Notifications.count() == 0);
}

void Ut_NotificationManager::testImNotification()
{
    CommHistory::Event event = createImEvent(CONTACT_1_REMOTE_ID, CONTACT_1_ID);
    // One notification from contact 1
    NotificationManager* nm = NotificationManager::instance();
    nm->m_NotificationTimer.stop();
    nm->removeNotificationGroup(event.type());
    removeGroup(event.type());

    QVERIFY(getGroup(event.type(), 2000) == 0);

    nm->showNotification(event, CONTACT_1_REMOTE_ID);

    justWait(2000);

    MNotificationGroup *mgtGroup = getGroup(event.type(), 5000);
    QVERIFY(mgtGroup);
    QVERIFY(mgtGroup->isPublished());
    QVERIFY(mgtGroup->summary().localeAwareCompare(CONTACT_1_REMOTE_ID));
    QCOMPARE(mgtGroup->body(), MESSAGE_TEXT);

    NotificationGroup group = nm->notificationGroup(event.type());

    QVERIFY(nm->countContacts(group) == 1);
    QVERIFY(nm->countNotifications(group) == 1);

    justWait(NOTIFICATION_THRESHOLD + 500);

    // Multiple notifications from contact 1
    CommHistory::Event event1 = createImEvent(CONTACT_1_REMOTE_ID, CONTACT_1_ID);
    nm->showNotification(event1, CONTACT_1_REMOTE_ID);

    justWait(1000);

    mgtGroup = getGroup(event.type(), 5000);
    QVERIFY(mgtGroup);
    QVERIFY(mgtGroup->isPublished());
    QVERIFY(mgtGroup->summary().localeAwareCompare(CONTACT_1_REMOTE_ID));
    QCOMPARE(mgtGroup->body(), txt_qtn_msg_notification_new_message(2));

    QVERIFY(nm->countContacts(group) == 1);
    QVERIFY(nm->countNotifications(group) == 2);

    justWait(NOTIFICATION_THRESHOLD + 500);

    CommHistory::Event event2 = createImEvent(CONTACT_2_REMOTE_ID, CONTACT_2_ID);
    nm->showNotification(event2, CONTACT_2_REMOTE_ID);

    justWait(2000); //wait for contact resolving

    mgtGroup = getGroup(event.type(), 5000);
    QVERIFY(mgtGroup);
    QVERIFY(mgtGroup->isPublished());
    QVERIFY(mgtGroup->summary().contains(CONTACT_1_REMOTE_ID));
    QVERIFY(mgtGroup->summary().contains(CONTACT_2_REMOTE_ID));
    QVERIFY(mgtGroup->summary().indexOf(CONTACT_2_REMOTE_ID) < mgtGroup->summary().indexOf(CONTACT_1_REMOTE_ID));
    QCOMPARE(mgtGroup->body(), txt_qtn_msg_notification_new_message(3));
}

void Ut_NotificationManager::testVoicemail()
{
    // One notification from contact 1
    NotificationManager* nm = NotificationManager::instance();
    nm->m_NotificationTimer.stop();
    nm->removeNotificationGroup(CommHistory::Event::VoicemailEvent);
    removeGroup(CommHistory::Event::VoicemailEvent);

    QVERIFY(getGroup(CommHistory::Event::VoicemailEvent, 10) == 0);

    nm->slotMWICountChanged(1);
    justWait(1000);

    MNotificationGroup *mgtGroup = getGroup(CommHistory::Event::VoicemailEvent, 5000);
    QVERIFY(mgtGroup);
    QVERIFY(mgtGroup->isPublished());
    QVERIFY(mgtGroup->summary().isEmpty());
    QCOMPARE(mgtGroup->body(), txt_qtn_call_voicemail_notification(1));

    justWait(NOTIFICATION_THRESHOLD + 500);

    nm->slotMWICountChanged(2);

    justWait(1000);
    mgtGroup = getGroup(CommHistory::Event::VoicemailEvent, 5000);
    QVERIFY(mgtGroup);
    QVERIFY(mgtGroup->isPublished());
    QVERIFY(mgtGroup->summary().isEmpty());
    QCOMPARE(mgtGroup->body(), txt_qtn_call_voicemail_notification(2));

    justWait(NOTIFICATION_THRESHOLD + 500);

    nm->slotMWICountChanged(-1); // unknown number

    justWait(1000);
    mgtGroup = getGroup(CommHistory::Event::VoicemailEvent, 5000);
    QVERIFY(mgtGroup);
    QVERIFY(mgtGroup->isPublished());
    QVERIFY(mgtGroup->summary().isEmpty());
    QCOMPARE(mgtGroup->body(), txt_qtn_call_voicemail_notification(1));

    justWait(NOTIFICATION_THRESHOLD + 500);

    nm->slotMWICountChanged(0);
    justWait(1000);

    mgtGroup = getGroup(CommHistory::Event::VoicemailEvent, 1);
    QVERIFY(mgtGroup == 0);
}

void Ut_NotificationManager::testMissedCallNotification()
{
    CommHistory::Event event = createMissedCallEvent(CONTACT_1_REMOTE_ID, CONTACT_1_ID);
    // One notification from contact 1
    NotificationManager* nm = NotificationManager::instance();
    nm->m_NotificationTimer.stop();
    nm->removeNotificationGroup(event.type());
    removeGroup(event.type());

    QVERIFY(getGroup(event.type(), 10) == 0);

    nm->showNotification(event, CONTACT_1_REMOTE_ID);

    justWait(1000);

    MNotificationGroup *mgtGroup = getGroup(event.type(), 5000);
    QVERIFY(mgtGroup);
    QVERIFY(mgtGroup->isPublished());
    QVERIFY(mgtGroup->summary().localeAwareCompare(CONTACT_1_REMOTE_ID));
    QCOMPARE(mgtGroup->body(), txt_qtn_call_missed(1));

    NotificationGroup group = nm->notificationGroup(event.type());

    QVERIFY(nm->countContacts(group) == 1);
    QVERIFY(nm->countNotifications(group) == 1);

    justWait(NOTIFICATION_THRESHOLD + 500);

    // Multiple notifications from contact 1
    CommHistory::Event event1 = createMissedCallEvent(CONTACT_1_REMOTE_ID, CONTACT_1_ID);
    nm->showNotification(event1, CONTACT_1_REMOTE_ID);

    justWait(1000);

    mgtGroup = getGroup(event.type(), 5000);
    QVERIFY(mgtGroup);
    QVERIFY(mgtGroup->isPublished());
    QVERIFY(mgtGroup->summary().localeAwareCompare(CONTACT_1_REMOTE_ID));
    QCOMPARE(mgtGroup->body(), txt_qtn_call_missed(2));

    QVERIFY(nm->countContacts(group) == 1);
    QVERIFY(nm->countNotifications(group) == 2);

    justWait(NOTIFICATION_THRESHOLD + 500);

    CommHistory::Event event2 = createMissedCallEvent(CONTACT_2_REMOTE_ID, CONTACT_2_ID);
    nm->showNotification(event2, CONTACT_2_REMOTE_ID);

    justWait(2000); //wait for contact resolving

    mgtGroup = getGroup(event.type(), 5000);
    QVERIFY(mgtGroup);
    QVERIFY(mgtGroup->isPublished());
    QVERIFY(mgtGroup->summary().contains(CONTACT_1_REMOTE_ID));
    QVERIFY(mgtGroup->summary().contains(CONTACT_2_REMOTE_ID));
    QCOMPARE(mgtGroup->body(), txt_qtn_call_missed(3));
}

QTEST_MAIN(Ut_NotificationManager)
