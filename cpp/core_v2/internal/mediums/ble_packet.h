#ifndef CORE_V2_INTERNAL_MEDIUMS_BLE_PACKET_H_
#define CORE_V2_INTERNAL_MEDIUMS_BLE_PACKET_H_

#include <limits>

#include "platform_v2/base/byte_array.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

// Represents the format of data sent over Ble sockets.
//
// [SERVICE_ID_HASH][DATA]
//
// See go/nearby-ble-design for more information.
class BlePacket {
 public:
  static const std::uint32_t kServiceIdHashLength = 3;

  BlePacket() = default;
  BlePacket(const ByteArray& service_id_hash, const ByteArray& data);
  explicit BlePacket(const ByteArray& ble_packet_byte);
  ~BlePacket() = default;

  BlePacket(const BlePacket&) = default;
  BlePacket& operator=(const BlePacket&) = default;
  BlePacket(BlePacket&&) = default;
  BlePacket& operator=(BlePacket&&) = default;

  explicit operator ByteArray() const;

  bool IsValid() const { return !service_id_hash_.Empty(); }
  ByteArray GetServiceIdHash() const { return service_id_hash_; }
  ByteArray GetData() const { return data_; }

 private:
  static const std::uint32_t kMaxDataSize =
      std::numeric_limits<int32_t>::max() - kServiceIdHashLength;

  ByteArray service_id_hash_;
  ByteArray data_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_MEDIUMS_BLE_PACKET_H_
