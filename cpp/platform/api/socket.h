#ifndef PLATFORM_API_SOCKET_H_
#define PLATFORM_API_SOCKET_H_

#include "platform/api/input_stream.h"
#include "platform/api/output_stream.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// A socket is an endpoint for communication between two machines.
//
// https://docs.oracle.com/javase/8/docs/api/java/net/Socket.html
class Socket {
 public:
  virtual ~Socket() {}

  virtual Ptr<InputStream> getInputStream() = 0;
  virtual Ptr<OutputStream> getOutputStream() = 0;
  virtual void close() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_SOCKET_H_
