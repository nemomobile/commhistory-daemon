
#include "types.h"

namespace Tp {

void registerTypes()
{
}

Feature::Feature(const QString &className, uint id, bool critical )
{
    Q_UNUSED( className );
    Q_UNUSED( id );
    Q_UNUSED( critical );
}

Feature::Feature(const Feature &other)
        :QPair<QString,uint>()
{
    Q_UNUSED( other );
}

Feature::~Feature()
{
}


QDBusArgument& operator<<(QDBusArgument& arg, const PropertyValue&){return arg;}
const QDBusArgument& operator>>(const QDBusArgument& arg, PropertyValue&){return arg;}

QDBusArgument& operator<<(QDBusArgument& arg, const PropertySpec&){return arg;}
const QDBusArgument& operator>>(const QDBusArgument& arg, PropertySpec&){return arg;}

QDBusArgument& operator<<(QDBusArgument& arg, const ServicePoint&){return arg;}
const QDBusArgument& operator>>(const QDBusArgument& arg, ServicePoint&){return arg;}

}
//Q_DECLARE_METATYPE(Tp::SimplePresence)
