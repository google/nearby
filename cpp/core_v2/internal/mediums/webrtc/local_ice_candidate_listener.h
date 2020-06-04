#ifndef CORE_V2_INTERNAL_MEDIUMS_WEBRTC_LOCAL_ICE_CANDIDATE_LISTENER_H_
#define CORE_V2_INTERNAL_MEDIUMS_WEBRTC_LOCAL_ICE_CANDIDATE_LISTENER_H_

#include "core_v2/listeners.h"
#include "webrtc/api/peer_connection_interface.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

// Callbacks from local ice candidate collection.
struct LocalIceCandidateListener {
  // Called when a new local ice candidate has been found.
  std::function<void(const webrtc::IceCandidateInterface*)>
      local_ice_candidate_found_cb = location::nearby::DefaultCallback<
          const webrtc::IceCandidateInterface*>();
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_MEDIUMS_WEBRTC_LOCAL_ICE_CANDIDATE_LISTENER_H_
