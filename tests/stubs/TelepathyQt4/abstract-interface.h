/* Abstract base class for Telepathy interfaces
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _TelepathyQt4_abstract_interface_h_HEADER_GUARD_
#define _TelepathyQt4_abstract_interface_h_HEADER_GUARD_

#include <QDBusAbstractInterface>

namespace Tp
{

class AbstractInterface : public QDBusAbstractInterface
{
    Q_OBJECT
    Q_DISABLE_COPY(AbstractInterface)

    #define UT_FAKE_INTERFACE "com.nokia.fake.interface"

public:
    AbstractInterface(const QString &service = QString(),
                      const QString &path = QString(),
                      const char *interface = UT_FAKE_INTERFACE,
                      const QDBusConnection &connection = QDBusConnection::sessionBus(),
                      QObject *parent = 0) : QDBusAbstractInterface(service,
                                                                    path,
                                                                    interface,
                                                                    connection,
                                                                    parent)
                                             ,m_isValid(true)
    {
        Q_UNUSED(service)
        Q_UNUSED(path)
        Q_UNUSED(interface)
        Q_UNUSED(connection)
        Q_UNUSED(parent)
    }

    virtual ~AbstractInterface(){}

    bool isValid() const {return m_isValid;}

public:
    void ut_setIsValid(bool valid) {m_isValid = valid;}

private:
    bool m_isValid;
};

} // Tp

#endif
