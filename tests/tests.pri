###############################################################################
#
# This file is part of commhistory-daemon.
#
# Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
# Contact: Reto Zingg <reto.zingg@nokia.com>
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

!include( ../common-project-config.pri ) : error( "Unable to include common-project-config.pri!" )
!include( ../common-vars.pri ) : error( "Unable to include common-vars.pri!" )

DEFINES -= QT_NO_DEBUG QT_NO_DEBUG_OUTPUT QT_NO_WARNING_OUTPUT
DEFINES += UNIT_TEST
QT          += testlib dbus sql gui
TEMPLATE     = app
INCLUDEPATH += . .. \
               ../../src

equals(QT_MAJOR_VERSION, 4) {
    CONFIG += mobility
    MOBILITY += contacts versit
    PKGCONFIG += contextsubscriber-1.0 mlite commhistory
}
equals(QT_MAJOR_VERSION, 5) {
    PKGCONFIG += Qt5Contacts Qt5Versit
    PKGCONFIG += contextkit-statefs mlite5 commhistory-qt5
}

COMMHISTORYDSRCDIR = ../../src
DEPENDPATH  += $${INCLUDEPATH}

!include( ../common-installs-config.pri ) : \
    error( "Unable to include common-installs-config.pri!" )
# override default path for tests
target.path = /opt/tests/$${PROJECT_NAME}
