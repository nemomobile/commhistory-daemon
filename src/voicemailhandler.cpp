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

// Qt includes
#include <QCoreApplication>
#include <QDebug>

// contacts
#include <QContactManager>
#include <QContactFetchRequest>
#include <QContactDetailFilter>
#include <QContactLocalIdFilter>
#include <QContactPhoneNumber>
#include <QContactGuid>

// Our includes
#include "voicemailhandler.h"
#include "notificationmanager.h"
#include "locstrings.h"
#include "constants.h"

using namespace RTComLogger;

VoiceMailHandler* VoiceMailHandler::m_pInstance = 0;

// constructor
//
VoiceMailHandler::VoiceMailHandler(QObject* parent)
        : QObject(parent)
        , m_pContactManager(0)
        , m_localContactId(0)
{
    qDebug() << Q_FUNC_INFO;
}

VoiceMailHandler::~VoiceMailHandler()
{
    qDebug() << Q_FUNC_INFO;
}

void VoiceMailHandler::init()
{
    qDebug() << Q_FUNC_INFO;

    m_pContactManager = NotificationManager::instance()->contactManager();

    if (m_pContactManager) {
        connect(m_pContactManager,
                SIGNAL(contactsAdded(const QList<QContactLocalId>&)),
                SLOT(slotContactsAdded(const QList<QContactLocalId>&)));
        connect(m_pContactManager,
                SIGNAL(contactsRemoved(const QList<QContactLocalId>&)),
                SLOT(slotContactsRemoved(const QList<QContactLocalId>&)));
        connect(m_pContactManager,
                SIGNAL(contactsChanged(const QList<QContactLocalId>&)),
                SLOT(slotContactsChanged(const QList<QContactLocalId>&)));

        fetchVoiceMailContact();
    }
}

VoiceMailHandler* VoiceMailHandler::instance()
{
    qDebug() << Q_FUNC_INFO;

    if (!m_pInstance) {
        m_pInstance = new VoiceMailHandler(QCoreApplication::instance());
        m_pInstance->init();
    }

    return m_pInstance;
}

bool VoiceMailHandler::isVoiceMailNumber(QString phoneNumber)
{
    qDebug() << Q_FUNC_INFO << "Voice mail numbers are " << m_voiceMailPhoneNumbers << ", compared number is " << phoneNumber;
    return m_voiceMailPhoneNumbers.contains(phoneNumber);
}

void VoiceMailHandler::fetchVoiceMailContact()
{
    qDebug() << Q_FUNC_INFO;

    QContactDetailFilter filter;
    filter.setDetailDefinitionName(QContactGuid::DefinitionName,
                                   QContactGuid::FieldGuid);
    filter.setValue(VOICEMAIL_CONTACT_GUID);

    QStringList details;
    details << QContactPhoneNumber::DefinitionName;

    startContactRequest(filter, details, SLOT(slotVoiceMailContactsAvailable()));
}

void VoiceMailHandler::slotContactsAdded(const QList<QContactLocalId> &contactIds)
{    
    if (contactIds.isEmpty())
        return;

    qDebug() << Q_FUNC_INFO;

    qDebug() << Q_FUNC_INFO << "Contact(s) having local id(s) " << contactIds << " added.";

    QContactLocalIdFilter filter;
    filter.setIds(contactIds);

    QStringList details;
    details << QContactGuid::DefinitionName;
    details << QContactPhoneNumber::DefinitionName;

    startContactRequest(filter, details, SLOT(slotContactsAvailable()));
}

void VoiceMailHandler::slotContactsRemoved(const QList<QContactLocalId> &contactIds)
{
    if (contactIds.isEmpty())
        return;

    qDebug() << Q_FUNC_INFO;

    qDebug() << Q_FUNC_INFO << "Contact(s) having local id(s) " << contactIds << " removed.";

    foreach (QContactLocalId id, contactIds) {
        if (id == m_localContactId) {
            qDebug() << Q_FUNC_INFO << "Removed contact is voice mail contact.";
            m_voiceMailPhoneNumbers.clear();
            m_localContactId = 0;
        }
    }
}

void VoiceMailHandler::slotContactsChanged(const QList<QContactLocalId> &contactIds)
{
    if (contactIds.isEmpty())
        return;

    qDebug() << Q_FUNC_INFO;

    qDebug() << Q_FUNC_INFO << "Contact(s) having local id(s) " << contactIds << " changed.";

    foreach (QContactLocalId id, contactIds) {
        if (id == m_localContactId) {
            qDebug() << Q_FUNC_INFO << "Changed contact is voice mail contact.";
            fetchVoiceMailContact();
            break;
        }
    }
}

QContactFetchRequest* VoiceMailHandler::startContactRequest(QContactFilter &filter, QStringList &details, const char *resultSlot)
{
    qDebug() << Q_FUNC_INFO;

    QContactFetchRequest* request = new QContactFetchRequest();
    request->setManager(m_pContactManager);
    request->setParent(this);

    connect(request,
            SIGNAL(resultsAvailable()),
            resultSlot);

    request->setFilter(filter);

    QContactFetchHint hint;
    hint.setDetailDefinitionsHint(details);
    request->setFetchHint(hint);

    request->start();

    // setup timeout for request
    QTimer *timer = new QTimer(request);
    connect(timer, SIGNAL(timeout()),
            this, SLOT(slotContactRequestTimeout()));
    timer->start(CONTACT_REQUEST_TIMEROUT);

    return request;
}

void VoiceMailHandler::slotContactRequestTimeout()
{
    qDebug() << Q_FUNC_INFO;

    Q_ASSERT(sender());

    QContactFetchRequest *request = qobject_cast<QContactFetchRequest*>(sender()->parent());

    Q_ASSERT(request);

    request->deleteLater();
}

void VoiceMailHandler::slotVoiceMailContactsAvailable()
{
    qDebug() << Q_FUNC_INFO;

    QContactFetchRequest *request = qobject_cast<QContactFetchRequest *>(sender());

    if(!request || !request->isFinished()) {
        return;
    }

    QList<QContact> contacts = request->contacts();

    qDebug() << __PRETTY_FUNCTION__ << "Number of voice mail contacts returned: " << contacts.size();

    if (!contacts.isEmpty()) {
        // There should be just one voice mail contact (that can have multiple numbers).
        QContact voiceMailContact = contacts.first();
        if (!voiceMailContact.isEmpty()) {
            m_localContactId = voiceMailContact.localId();
            qDebug() << __PRETTY_FUNCTION__ << "Local id of voice mail contact is: " << m_localContactId;
            QList<QContactPhoneNumber> phoneNumbers = voiceMailContact.details<QContactPhoneNumber>();

            m_voiceMailPhoneNumbers.clear();

            foreach (QContactPhoneNumber phoneNumber, phoneNumbers) {
                m_voiceMailPhoneNumbers << phoneNumber.number();
            }
            qDebug() << __PRETTY_FUNCTION__ << "Voice mail phone numbers are: " << m_voiceMailPhoneNumbers;
        } else {
            qDebug() << __PRETTY_FUNCTION__ << "Empty voice mail contact!";
        }
    } else {
        qDebug() << __PRETTY_FUNCTION__ << "No voice mail contacts found!";
    }

    request->deleteLater();
}

void VoiceMailHandler::slotContactsAvailable()
{
    qDebug() << Q_FUNC_INFO;

    QContactFetchRequest *request = qobject_cast<QContactFetchRequest *>(sender());

    if(!request || !request->isFinished()) {
        return;
    }

    QList<QContact> contacts = request->contacts();

    qDebug() << __PRETTY_FUNCTION__ << "Number of contacts returned: " << contacts.size();

    foreach (QContact contact, contacts) {
        if (!contact.isEmpty()) {
            qDebug() << __PRETTY_FUNCTION__ << "Contact " << contact.localId() << " was added.";
            if (!(contact.details<QContactGuid>(QContactGuid::FieldGuid, VOICEMAIL_CONTACT_GUID)).isEmpty()) {
                qDebug() << __PRETTY_FUNCTION__ << "Added contact is a voice mail contact";
                m_localContactId = contact.localId();
                QList<QContactPhoneNumber> phoneNumbers = contact.details<QContactPhoneNumber>();

                m_voiceMailPhoneNumbers.clear();

                foreach (QContactPhoneNumber phoneNumber, phoneNumbers) {
                     m_voiceMailPhoneNumbers << phoneNumber.number();
                 }
                qDebug() << __PRETTY_FUNCTION__ << "Voice mail phone numbers added are: " << m_voiceMailPhoneNumbers;
            } else {
                qDebug() << __PRETTY_FUNCTION__ << "No voice mail GUID for this contact!";
            }
        } else {
            qDebug() << __PRETTY_FUNCTION__ << "Empty contact returned!";
        }
    }

    request->deleteLater();
}
