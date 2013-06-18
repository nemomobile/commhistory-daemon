#include <CommHistory/GroupModel>
#include <QCoreApplication>

#include "notificationmanager.h"

using namespace RTComLogger;

NotificationManager* NotificationManager::m_pInstance = 0;

NotificationManager::NotificationManager(QObject *parent) :
    QObject(parent)
{
#ifdef USING_QTPIM
    // Temporary override until qtpim supports QTCONTACTS_MANAGER_OVERRIDE
    m_pContactManager = new QContactManager(QString::fromLatin1("org.nemomobile.contacts.sqlite"));
#else
    m_pContactManager = new QContactManager;
#endif
    m_GroupModel = new CommHistory::GroupModel(this);
    m_GroupModel->enableContactChanges(false);

    if (!m_GroupModel->getGroups()) {
        qCritical() << "Failed to request group ";
        delete m_GroupModel;
        m_GroupModel = 0;
    }
}

NotificationManager* NotificationManager::instance()
{
    if (!m_pInstance) {
        m_pInstance = new NotificationManager(QCoreApplication::instance());
    }

    return m_pInstance;
}

void NotificationManager::showNotification(const CommHistory::Event& event,
                      const QString &channelTargetId,
                      CommHistory::Group::ChatType chatType)
{
    qDebug() << event.toString() << channelTargetId << chatType;
    Notification n;
    n.event = event;
    n.channelTargetId = channelTargetId;
    n.chatType = chatType;

    postedNotifications.append(n);
}


void NotificationManager::showVoicemailNotification(int count)
{
    voicemailNotifications = count;
}

CommHistory::GroupModel* NotificationManager::groupModel()
{
    return m_GroupModel;
}

QContactManager* NotificationManager::contactManager()
{
    return m_pContactManager;
}
