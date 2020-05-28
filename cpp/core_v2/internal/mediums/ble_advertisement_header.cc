#include "core_v2/internal/mediums/ble_advertisement_header.h"

#include <inttypes.h>

#include "platform_v2/base/base64_utils.h"
#include "platform_v2/public/logging.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

BleAdvertisementHeader::BleAdvertisementHeader(
    Version version, int num_slots, const ByteArray &service_id_bloom_filter,
    const ByteArray &advertisement_hash) {
  // TODO(edwinwu): Checks if num_slots needs to be >= 0
  if (version != Version::kV2 ||
      service_id_bloom_filter.size() != kServiceIdBloomFilterLength ||
      advertisement_hash.size() != kAdvertisementHashLength) {
    return;
  }

  version_ = version;
  num_slots_ = num_slots;
  service_id_bloom_filter_ = service_id_bloom_filter;
  advertisement_hash_ = advertisement_hash;
}

BleAdvertisementHeader::BleAdvertisementHeader(
    const std::string &ble_advertisement_header_string) {
  ByteArray ble_advertisement_header_bytes =
      Base64Utils::Decode(ble_advertisement_header_string);

  if (ble_advertisement_header_bytes.Empty()) {
    NEARBY_LOG(
        ERROR,
        "Cannot deserialize BLEAdvertisementHeader: failed Base64 decoding");
    return;
  }

  if (ble_advertisement_header_bytes.size() < kMinAdvertisementHeaderLength) {
    NEARBY_LOG(ERROR,
               "Cannot deserialize BleAdvertisementHeader: expecting min %u "
               "raw bytes, got %" PRIu64 " instead",
               kMinAdvertisementHeaderLength,
               ble_advertisement_header_bytes.size());
    return;
  }

  // Start reading the bytes.
  auto *ble_advertisement_header_read_ptr =
      ble_advertisement_header_bytes.data();

  // The first 3 bits are supposed to be the version.
  version_ = static_cast<Version>(
      (*ble_advertisement_header_read_ptr & kVersionBitmask) >> 5);
  if (version_ != Version::kV2) {
    NEARBY_LOG(
        ERROR,
        "Cannot deserialize BleAdvertisementHeader: unsupported Version %d",
        version_);
    return;
  }
  // The last 5 bits of the first byte represent the number of slots.
  num_slots_ = static_cast<std::uint32_t>(*ble_advertisement_header_read_ptr &
                                          kNumSlotsBitmask);
  ble_advertisement_header_read_ptr++;

  // Service ID bloom filter.
  service_id_bloom_filter_ =
      ByteArray(ble_advertisement_header_read_ptr, kServiceIdBloomFilterLength);
  ble_advertisement_header_read_ptr += kServiceIdBloomFilterLength;

  // Advertisement hash.
  advertisement_hash_ =
      ByteArray(ble_advertisement_header_read_ptr, kAdvertisementHashLength);
  ble_advertisement_header_read_ptr += kAdvertisementHashLength;
}

BleAdvertisementHeader::operator std::string() const {
  if (!IsValid()) {
    return "";
  }

  std::string out;

  // The first 3 bits are the Version.
  char version_and_num_slots_byte =
      (static_cast<char>(version_) << 5) & kVersionBitmask;
  // The next 5 bits are the number of slots.
  version_and_num_slots_byte |=
      static_cast<char>(num_slots_) & kNumSlotsBitmask;
  out.reserve(1 + service_id_bloom_filter_.size() + advertisement_hash_.size());
  out.append(1, version_and_num_slots_byte);
  out.append(std::string(service_id_bloom_filter_));
  out.append(std::string(advertisement_hash_));

  return Base64Utils::Encode(ByteArray(std::move(out)));
}

bool BleAdvertisementHeader::operator<(
    const BleAdvertisementHeader &rhs) const {
  if (this->GetVersion() != rhs.GetVersion()) {
    return this->GetVersion() < rhs.GetVersion();
  }
  if (this->GetNumSlots() != rhs.GetNumSlots()) {
    return this->GetNumSlots() < rhs.GetNumSlots();
  }
  if (this->GetServiceIdBloomFilter() != rhs.GetServiceIdBloomFilter()) {
    return this->GetServiceIdBloomFilter() < rhs.GetServiceIdBloomFilter();
  }
  return this->GetAdvertisementHash() < rhs.GetAdvertisementHash();
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
