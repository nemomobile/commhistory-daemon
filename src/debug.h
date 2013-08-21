/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2013 Jolla 
** Contact: Joona Petrell <joona.petrell@jolla.com>
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

#ifndef DEBUG_H
#define DEBUG_H

#include <QDebug>

// Define this if you'd like to see debug messages from Commhistory Daemon
// #define DEBUG_COMMHISTORY
#ifdef DEBUG_COMMHISTORY
# define DEBUG qDebug
#else
# define DEBUG if (0) qDebug
#endif

#endif // DEBUG_H
