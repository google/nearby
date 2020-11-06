#ifndef CORE_INTERNAL_OFFLINE_FRAMES_VALIDATOR_H_
#define CORE_INTERNAL_OFFLINE_FRAMES_VALIDATOR_H_

#include "proto/connections/offline_wire_formats.pb.h"
#include "platform/base/exception.h"

namespace location {
namespace nearby {
namespace connections {
namespace parser {

Exception EnsureValidOfflineFrame(const OfflineFrame& offline_frame);

}  // namespace parser
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_OFFLINE_FRAMES_VALIDATOR_H_
