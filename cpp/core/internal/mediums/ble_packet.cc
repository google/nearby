#include "core/internal/mediums/ble_packet.h"

#include <limits>

#include "platform/logging.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

const std::uint32_t BLEPacket::kServiceIdHashLength = 3;

const std::uint32_t BLEPacket::kMinPacketLength = kServiceIdHashLength;
const std::uint32_t BLEPacket::kMaxDataSize =
    std::numeric_limits<int32_t>::max() - kMinPacketLength;

ConstPtr<BLEPacket> BLEPacket::fromBytes(ConstPtr<ByteArray> ble_packet_bytes) {
  if (ble_packet_bytes.isNull()) {
    NEARBY_LOG(INFO, "Cannot deserialize BLEPacket: null bytes passed in");
    return ConstPtr<BLEPacket>();
  }

  if (ble_packet_bytes->size() < kMinPacketLength) {
    NEARBY_LOG(
        INFO,
        "Cannot deserialize BLEPacket: expecting min %u raw bytes, got %zu",
        kMinPacketLength, ble_packet_bytes->size());
    return ConstPtr<BLEPacket>();
  }

  // Now, time to read the bytes!
  const char *ble_packet_bytes_read_ptr = ble_packet_bytes->getData();

  // 1. Service ID hash.
  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(MakeConstPtr(
      new ByteArray(ble_packet_bytes_read_ptr, kServiceIdHashLength)));
  ble_packet_bytes_read_ptr += kServiceIdHashLength;

  // 2. Data.
  size_t data_size = computeDataSize(ble_packet_bytes);
  ScopedPtr<ConstPtr<ByteArray> > scoped_data(
      MakeConstPtr(new ByteArray(ble_packet_bytes_read_ptr, data_size)));
  ble_packet_bytes_read_ptr += data_size;

  return MakeConstPtr(
      new BLEPacket(scoped_service_id_hash.release(), scoped_data.release()));
}

ConstPtr<ByteArray> BLEPacket::toBytes(ConstPtr<ByteArray> service_id_hash,
                                       ConstPtr<ByteArray> data) {
  if (service_id_hash->size() != kServiceIdHashLength) {
    NEARBY_LOG(
        INFO,
        "Cannot serialize BLEPacket: expected a service_id_hash of %u bytes, "
        "but got %zu",
        kServiceIdHashLength, service_id_hash->size());
    return ConstPtr<ByteArray>();
  }

  if (data->size() > kMaxDataSize) {
    NEARBY_LOG(INFO,
               "Cannot serialize BLEPacket: expected data of at most %u bytes, "
               "but got %zu",
               kMaxDataSize, data->size());
    return ConstPtr<ByteArray>();
  }

  // Initialize the bytes.
  size_t packet_length = computePacketLength(data);
  Ptr<ByteArray> packet_bytes{new ByteArray{packet_length}};
  char *packet_bytes_write_ptr = packet_bytes->getData();

  // 1. Service ID hash.
  memcpy(packet_bytes_write_ptr, service_id_hash->getData(),
         kServiceIdHashLength);
  packet_bytes_write_ptr += kServiceIdHashLength;

  // 2. Data.
  memcpy(packet_bytes_write_ptr, data->getData(), data->size());
  packet_bytes_write_ptr += data->size();

  return ConstifyPtr(packet_bytes);
}

size_t BLEPacket::computeDataSize(ConstPtr<ByteArray> ble_packet_bytes) {
  return ble_packet_bytes->size() - kMinPacketLength;
}

size_t BLEPacket::computePacketLength(ConstPtr<ByteArray> data) {
  // The packet length is the minimum length + the length of the data.
  return kMinPacketLength + data->size();
}

BLEPacket::BLEPacket(ConstPtr<ByteArray> service_id_hash,
                     ConstPtr<ByteArray> data)
    : service_id_hash_(service_id_hash), data_(data) {}

BLEPacket::~BLEPacket() {
  // Nothing to do.
}

ConstPtr<ByteArray> BLEPacket::getServiceIdHash() const {
  return service_id_hash_.get();
}

ConstPtr<ByteArray> BLEPacket::getData() const { return data_.get(); }

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
