#ifndef PLATFORM_IMPL_LINUX_DBUS_H_
#define PLATFORM_IMPL_LINUX_DBUS_H_

#include "internal/platform/logging.h"
#include <sdbus-c++/IConnection.h>

#define DBUS_LOG_METHOD_CALL_ERROR(p, m, e)                                    \
  do {                                                                         \
    NEARBY_LOGS(ERROR) << __func__ << ": Got error '" << (e).getName()         \
                       << "' with message '" << (e).getMessage()               \
                       << "' while calling " << (m) << " on object "           \
                       << (p)->getObjectPath();                                \
  } while (false)

#define DBUS_LOG_PROPERTY_GET_ERROR(p, prop, e)                                \
  do {                                                                         \
    NEARBY_LOGS(ERROR) << __func__ << ": Got error '" << (e).getName()         \
                       << "' with message '" << (e).getMessage()               \
                       << "' while getting property " << (prop)                \
                       << " on object " << (p)->getObjectPath();               \
  } while (false)

#define DBUS_LOG_PROPERTY_SET_ERROR(p, prop, e)                                \
  do {                                                                         \
    NEARBY_LOGS(ERROR) << __func__ << ": Got error '" << (e).getName()         \
                       << "' with message '" << (e).getMessage()               \
                       << "' while setting property " << (prop)                \
                       << " on object " << (p)->getObjectPath();               \
  } while (false)

namespace nearby {
namespace linux {
extern sdbus::IConnection &getSystemBusConnection();
extern sdbus::IConnection &getDefaultBusConnection();
} // namespace linux
} // namespace nearby
#endif
