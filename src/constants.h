/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Alexander Shalamov <alexander.shalamov@nokia.com>
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

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <CommHistory/Event>

namespace RTComLogger {

#define OBSERVED_CONVERSATION_KEY QLatin1String("Messaging.ObservedConversation")
#define INBOX_KEY QLatin1String("Messaging.Inbox")
#define CALL_HISTORY_KEY QLatin1String("CallHistory.Inbox")

// Used to generate duiremoteaction strings
#define OBJECT_PATH QLatin1String("/")

#define COMM_HISTORY_SERVICE_NAME    QLatin1String("com.nokia.CommHistory")
#define COMM_HISTORY_OBJECT_PATH     QLatin1String("/com/nokia/CommHistory")
#define COMM_HISTORY_INTERFACE       QLatin1String("com.nokia.CommHistoryIf")
#define ACTIVATE_NOTIFICATION_METHOD QLatin1String("activateNotification")

#define MESSAGING_SERVICE_NAME    QLatin1String("com.nokia.Messaging")
#define MESSAGING_INTERFACE       QLatin1String("com.nokia.MessagingIf")
#define SHOW_INBOX_METHOD         QLatin1String("showInbox")
#define START_CONVERSATION_METHOD QLatin1String("startConversation")

#define CALL_HISTORY_SERVICE_NAME QLatin1String("com.nokia.telephony.callhistory")
#define CALL_HISTORY_OBJECT_PATH  QLatin1String("/org/maemo/m")
#define CALL_HISTORY_INTERFACE    QLatin1String("com.nokia.MApplicationIf")
#define CALL_HISTORY_METHOD       QLatin1String("launch")
#define CALL_HISTORY_PARAMETER    QLatin1String("callHistory")

#define CALL_SERVICE_NAME QLatin1String("Com.Nokia.Telephony.CallUi")
#define CALL_INTERFACE    QLatin1String("Com.Nokia.Telephony.CallUi")
#define CALL_OBJECT_PATH  QLatin1String("/Call")
#define CALL_METHOD       QLatin1String("RequestCall")
#define VOICEMAIL_METHOD  QLatin1String("CallVoicemail")

#define CSD_SIM_SERVICE_NAME       QLatin1String("com.nokia.csd.SIM")
#define CSD_SIM_OBJECT_PATH        QLatin1String("/com/nokia/csd/sim")
#define CSD_SIM_INTERFACE          QLatin1String("com.nokia.csd.SIM")
#define CSD_SIM_SETTINGS_INTERFACE QLatin1String("com.nokia.csd.SIM.Settings")
#define CSD_SIM_SET_MWI_METHOD     QLatin1String("SetMessageWaitingIndicationStatus")
#define CSD_SIM_GET_MWI_METHOD     QLatin1String("GetMessageWaitingIndicationStatus")
#define CSD_SIM_GET_SIM_STATUS     QLatin1String("GetSIMStatus")
#define CSD_SIM_STATUS_SIGNAL      QLatin1String("SIMStatus")
#define CSD_SIM_STATUS_OK          QLatin1String("Ok")
#define CSD_SIM_STATUS_REMOVED     QLatin1String("Removed")
#define CSD_SIM_STATUS_NO_SIM      QLatin1String("NoSIM")

#define RING_ACCOUNT_PATH QLatin1String("/org/freedesktop/Telepathy/Account/ring/tel/ring")
#define MMS_ACCOUNT_PATH  QLatin1String("/org/freedesktop/Telepathy/Account/mmscm/mms/mms0")

#define TELEPATHY_CHANNEL_INTERFACE_PERSISTENT_ID QLatin1String("com.nokia.Telepathy.Channel.Interface.Persistent.PersistentID")

// DUI didnt define namespace for events, remove this
#define CONTACT_STORAGE_TYPE QLatin1String("tracker")
// notification threshold is 3 seconds
#define NOTIFICATION_THRESHOLD 3000
// threshold to trigger contact request after added/modified contacts signals
#define CONTACT_REQUEST_THRESHOLD 5000

// events
struct EventTypes {
    int type;
    const char* event;
};

static const EventTypes _eventTypes[] =
{
    {CommHistory::Event::IMEvent,       "x-nokia.messaging.im"},
    {CommHistory::Event::SMSEvent,      "x-nokia.messaging.sms"},
    {CommHistory::Event::MMSEvent,      "x-nokia.messaging.mms"},
    {CommHistory::Event::CallEvent,     "x-nokia.call.missed"},
    {CommHistory::Event::VoicemailEvent,"x-nokia.messaging.voicemail"}
};

static const int _eventTypesCount = sizeof(_eventTypes) / sizeof(EventTypes);
}

#endif //#define CONSTANTS_H
