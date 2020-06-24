#ifndef CORE_V2_INTERNAL_MEDIUMS_WEBRTC_PEER_ID_H_
#define CORE_V2_INTERNAL_MEDIUMS_WEBRTC_PEER_ID_H_

#include <memory>
#include <string>

#include "platform_v2/base/byte_array.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

// PeerId is used as an identifier to exchange SDP messages to establish WebRTC
// p2p connection. An empty PeerId is considered to be invalid.
class PeerId {
 public:
  PeerId() = default;
  explicit PeerId(const std::string& id) : id_(id) {}
  ~PeerId() = default;

  static PeerId FromRandom();
  static PeerId FromSeed(const ByteArray& seed);

  bool IsValid() const;

  const std::string& GetId() const { return id_; }

 private:
  std::string id_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_MEDIUMS_WEBRTC_PEER_ID_H_
