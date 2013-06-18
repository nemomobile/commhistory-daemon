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

#-----------------------------------------------------------------------------
# Project file for test ut_textchannellistener
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# common test configuration
#-----------------------------------------------------------------------------
!include(../tests.pri) : error( "Unable to include test.pri" )

!include( ../stubs/stubs.pri ) : error("Unable to include stubs/stubs.pri")
INCLUDEPATH = ../stubs/ $${INCLUDEPATH}

#-----------------------------------------------------------------------------
# test specific configuration
#-----------------------------------------------------------------------------

TARGET = ut_textchannellistener

equals(QT_MAJOR_VERSION, 4): CONFIG += mlocale
equals(QT_MAJOR_VERSION, 5): PKGCONFIG += mlocale5

TEST_SOURCES += $$COMMHISTORYDSRCDIR/textchannellistener.cpp \
                $$COMMHISTORYDSRCDIR/channellistener.cpp

TEST_HEADERS += $$COMMHISTORYDSRCDIR/textchannellistener.h \
                $$COMMHISTORYDSRCDIR/channellistener.h

HEADERS     += ut_textchannellistener.h \
            $$TEST_HEADERS

SOURCES     += ut_textchannellistener.cpp \
            $$TEST_SOURCES

DESTDIR = ../bin
QT += dbus

# End of File

