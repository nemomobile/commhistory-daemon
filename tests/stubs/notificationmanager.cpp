#include <QContactManager>
#include <CommHistory/GroupModel>

#include "channellistener.h"

#include "notificationmanager.h"

using namespace RTComLogger;

NotificationManager* NotificationManager::m_pInstance = 0;

NotificationManager::NotificationManager(QObject *parent) :
    QObject(parent)
{
    m_pContactManager = new QContactManager(QLatin1String("tracker"));
    m_GroupModel = new CommHistory::GroupModel(this);
    m_GroupModel->enableContactChanges(false);

    if (!m_GroupModel->getGroups()) {
        qCritical() << "Failed to request group "
                    << m_GroupModel->lastError().text();
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

void NotificationManager::showNotification(ChannelListener *channelListener,
                      const CommHistory::Event& event,
                      const QString &channelTargetId,
                      CommHistory::Group::ChatType chatType)
{
    qDebug() << channelListener << event.toString() << channelTargetId << chatType;
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
