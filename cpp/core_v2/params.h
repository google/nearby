#ifndef CORE_V2_PARAMS_H_
#define CORE_V2_PARAMS_H_

#include <string>

#include "core_v2/listeners.h"

namespace location {
namespace nearby {
namespace connections {

// Used by Discovery in Core::RequestConnection().
// Used by Advertising in Core::StartAdvertising().
struct ConnectionRequestInfo {
  // name     - A human readable name for this endpoint, to appear on
  //            other devices.
  // listener - A set of callbacks notified when remote endpoints request a
  //            connection to this endpoint.
  std::string name;
  ConnectionListener listener;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_PARAMS_H_
