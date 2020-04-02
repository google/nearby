#ifndef PLATFORM_PORT_CONFIG_H_
#define PLATFORM_PORT_CONFIG_H_

// Clients can modify this file to customize the Nearby C++ codebase as per
// their particular constraints and environments.

// Note: Every entry in this file should conform to the following format, to
// give precedence to command-line options (-D) that set these symbols:
//
//   #ifndef XXX
//   #define XXX 0/1
//   #endif

#ifndef NEARBY_USE_STD_STRING
#define NEARBY_USE_STD_STRING 1
#endif

#ifndef NEARBY_USE_RTTI
#define NEARBY_USE_RTTI 0
#endif

#endif  // PLATFORM_PORT_CONFIG_H_
