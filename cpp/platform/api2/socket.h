#ifndef PLATFORM_API2_SOCKET_H_
#define PLATFORM_API2_SOCKET_H_

#include "platform/api2/input_stream.h"
#include "platform/api2/output_stream.h"

namespace location {
namespace nearby {

// A socket is an endpoint for communication between two machines.
//
// https://docs.oracle.com/javase/8/docs/api/java/net/Socket.html
class Socket {
 public:
  virtual ~Socket() {}

  virtual InputStream& GetInputStream() = 0;
  virtual OutputStream& GetOutputStream() = 0;
  virtual void Close() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_SOCKET_H_
