#ifndef CORE_INTERNAL_MEDIUMS_WEBRTC_PEER_ID_H_
#define CORE_INTERNAL_MEDIUMS_WEBRTC_PEER_ID_H_

#include "platform/api/hash_utils.h"
#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

// PeerId is used as an identifier to exchange SDP messages to establish WebRTC
// p2p connection.
class PeerId {
 public:
  explicit PeerId(const string& id) : id_(id) {}
  ~PeerId() = default;

  static ConstPtr<PeerId> FromRandom(Ptr<HashUtils> hash_utils);
  static ConstPtr<PeerId> FromSeed(ConstPtr<ByteArray> seed,
                                   Ptr<HashUtils> hash_utils);

  const string& GetId() const { return id_; }

 private:
  const string id_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_PEER_ID_H_
