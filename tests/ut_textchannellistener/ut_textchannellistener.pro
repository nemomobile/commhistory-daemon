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

#-----------------------------------------------------------------------------
# Project file for test ut_textchannellistener
#-----------------------------------------------------------------------------

!include( ../../common-project-config.pri ) : error( "Unable to include common-project-config.pri!" )
!include( ../../common-vars.pri ) : error( "Unable to include common-project-config.pri!" )

#-----------------------------------------------------------------------------
# common test configuration
#-----------------------------------------------------------------------------
!include(../tests.pri) : error( "Unable to include test.pri" )

#-----------------------------------------------------------------------------
# test specific configuration
#-----------------------------------------------------------------------------

TARGET = ut_textchannellistener
CONFIG += link_pkgconfig \
          mobility
LIBS += -lQtContacts -lQtVersit
PKGCONFIG += contextsubscriber-1.0 \
             meegotouch

HEADERS     += ut_textchannellistener.h \
            $$TEST_HEADERS

SOURCES     += ut_textchannellistener.cpp \
            $$TEST_SOURCES

DESTDIR = ../bin
QT += dbus

# End of File

