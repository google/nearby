#ifndef PLATFORM_PORT_DOWN_CAST_H_
#define PLATFORM_PORT_DOWN_CAST_H_

#include "platform/port/config.h"

#if NEARBY_USE_RTTI
#define DOWN_CAST dynamic_cast
#else
#define DOWN_CAST static_cast
#endif

#endif  // PLATFORM_PORT_DOWN_CAST_H_
