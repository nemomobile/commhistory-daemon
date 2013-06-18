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

!include( ../common-vars.pri ):error( "Unable to install common-vars.pri" )

TEMPLATE = subdirs
SUBDIRS = ut_notificationmanager \
          ut_textchannellistener \
          ut_streamchannellistener \
          ut_messagereviver
CONFIG += ordered

# make sure the destination path exists
!system( mkdir -p $${OUT_PWD}/bin ) : \
    error( "Unable to create bin dir for tests." )

#-----------------------------------------------------------------------------
# generate test xml
#-----------------------------------------------------------------------------
!system( ./do_tests_xml.sh $${OUT_PWD}/bin \
                    $${PROJECT_NAME} \
                     \"$${SUBDIRS}\" ) : \
     error("Error running do_tests_xml.sh")
QMAKE_CLEAN += $${OUT_PWD}/bin/tests.xml

#-----------------------------------------------------------------------------
# installation setup
#-----------------------------------------------------------------------------
!include( ../common-installs-config.pri ) : \
         error( "Unable to include common-installs-config.pri!" )
autotests.files = $${OUT_PWD}/bin/*
autotests.path  = /opt/tests/$${PROJECT_NAME}/
INSTALLS += autotests
