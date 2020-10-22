#ifndef PLATFORM_BASE_SOCKET_H_
#define PLATFORM_BASE_SOCKET_H_

#include "platform/base/input_stream.h"
#include "platform/base/output_stream.h"

namespace location {
namespace nearby {

// A socket is an endpoint for communication between two machines.
//
// https://docs.oracle.com/javase/8/docs/api/java/net/Socket.html
class Socket {
 public:
  virtual ~Socket() = default;

  virtual InputStream& GetInputStream() = 0;
  virtual OutputStream& GetOutputStream() = 0;
  virtual void Close() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_BASE_SOCKET_H_
