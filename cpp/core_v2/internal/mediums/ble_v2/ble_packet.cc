#include "core_v2/internal/mediums/ble_v2/ble_packet.h"

#include "platform_v2/base/base_input_stream.h"
#include "platform_v2/public/logging.h"
#include "absl/strings/str_cat.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

BlePacket::BlePacket(const ByteArray& service_id_hash, const ByteArray& data) {
  if (service_id_hash.size() != kServiceIdHashLength ||
      data.size() > kMaxDataSize) {
    return;
  }
  service_id_hash_ = service_id_hash;
  data_ = data;
}

BlePacket::BlePacket(const ByteArray& ble_packet_bytes) {
  if (ble_packet_bytes.Empty()) {
    NEARBY_LOG(ERROR, "Cannot deserialize BlePacket: null bytes passed in");
    return;
  }

  if (ble_packet_bytes.size() < kServiceIdHashLength) {
    NEARBY_LOG(
        INFO,
        "Cannot deserialize BlePacket: expecting min %u raw bytes, got %zu",
        kServiceIdHashLength, ble_packet_bytes.size());
    return;
  }

  ByteArray packet_bytes{ble_packet_bytes};
  BaseInputStream base_input_stream{packet_bytes};
  // The first 3 bytes are supposed to be the service_id_hash.
  service_id_hash_ = base_input_stream.ReadBytes(kServiceIdHashLength);

  // The rest bytes are supposed to be the data.
  data_ = base_input_stream.ReadBytes(ble_packet_bytes.size() -
                                      kServiceIdHashLength);
}

BlePacket::operator ByteArray() const {
  if (!IsValid()) {
    return ByteArray();
  }

  std::string out =
      absl::StrCat(std::string(service_id_hash_), std::string(data_));

  return ByteArray(std::move(out));
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
