#ifndef PLATFORM_PORT_STRING_H_
#define PLATFORM_PORT_STRING_H_

#include <string>

#include "platform/port/config.h"

#if NEARBY_USE_STD_STRING
using std::string;
#endif

#endif  // PLATFORM_PORT_STRING_H_
