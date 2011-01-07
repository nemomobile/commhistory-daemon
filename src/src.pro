###############################################################################
#
# This file is part of commhistory-daemon.
#
# Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
# Contact: Alexander Shalamov <alexander.shalamov@nokia.com>
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License version 2.1 as
# published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
#
###############################################################################

include( ../common-project-config.pri )
include( ../common-vars.pri )

# -----------------------------------------------------------------------------
# target setup
# -----------------------------------------------------------------------------
TARGET = commhistoryd

# -----------------------------------------------------------------------------
# dependencies
# -----------------------------------------------------------------------------
MOBILITY += contacts \
            versit

CONFIG += debug \
          qdbus \
          link_pkgconfig \
          mobility

QT += sql \
      gui

PKGCONFIG += TelepathyQt4 \
             commhistory \
             contextsubscriber-1.0 \
             meegotouch \
             MNgfClient \
             RTComTelepathyQt4

# -----------------------------------------------------------------------------
# input
# -----------------------------------------------------------------------------
HEADERS += logger.h \
           channellistener.h \
           textchannellistener.h \
           streamchannellistener.h \
           loggerclientobserver.h \
           notificationmanager.h \
           serialisable.h \
           notificationgroup.h \
           personalnotification.h \
           reportnotification.h \
           commhistoryifadaptor.h \
           commhistoryservice.h \
           locstrings.h \
           messagereviver.h \
           connectionutils.h \
           contactauthorizationlistener.h \
           contactauthorizer.h \
           mwilistener.h \
           constants.h

SOURCES += main.cpp \
           logger.cpp \
           channellistener.cpp \
           textchannellistener.cpp \
           streamchannellistener.cpp \
           loggerclientobserver.cpp \
           notificationmanager.cpp \
           serialisable.cpp \
           notificationgroup.cpp \
           personalnotification.cpp \
           reportnotification.cpp \
           commhistoryifadaptor.cpp \
           commhistoryservice.cpp \
           messagereviver.cpp \
           connectionutils.cpp \
           contactauthorizationlistener.cpp \
           contactauthorizer.cpp \
           mwilistener.cpp

# -----------------------------------------------------------------------------
# common installation setup
# NOTE: remember to set headers.files before this include to have the headers
# properly setup.
# -----------------------------------------------------------------------------
include( ../common-installs-config.pri )

# -----------------------------------------------------------------------------
# Installation target for application resources
# -----------------------------------------------------------------------------
notification_types.path  = $${INSTALL_PREFIX}/share/meegotouch/notifications/eventtypes
notification_types.files = $${_PRO_FILE_PWD_}/../data/notifications/*.conf
INSTALLS                += notification_types
