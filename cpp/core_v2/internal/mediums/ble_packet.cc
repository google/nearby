#include "core_v2/internal/mediums/ble_packet.h"

#include "platform_v2/public/logging.h"

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

  const char *ble_packet_bytes_read_ptr = ble_packet_bytes.data();
  service_id_hash_ =
      ByteArray(ble_packet_bytes_read_ptr, kServiceIdHashLength);
  ble_packet_bytes_read_ptr += kServiceIdHashLength;

  data_ = ByteArray(ble_packet_bytes_read_ptr,
                    ble_packet_bytes.size() - kServiceIdHashLength);
}

BlePacket::operator ByteArray() const {
  if (!IsValid()) {
    return ByteArray();
  }

  std::string out;

  out.reserve(service_id_hash_.size() + data_.size());
  out.append(std::string(service_id_hash_));
  out.append(std::string(data_));

  return ByteArray(std::move(out));
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
