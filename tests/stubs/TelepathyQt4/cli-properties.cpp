#include "cli-properties.h"

namespace Tp
{
namespace Client
{
PropertiesInterfaceInterface::PropertiesInterfaceInterface(
   const QString& busName,
    const QString& objectPath,
    QObject*) : AbstractInterface(busName, objectPath, staticInterfaceName())
{

}

PropertiesInterfaceInterface::PropertiesInterfaceInterface(const Tp::AbstractInterface&) :
        AbstractInterface(QString(), QString(), staticInterfaceName())
{
}
} // client
} // Tp
