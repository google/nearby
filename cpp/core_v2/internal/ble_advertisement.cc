#include "core_v2/internal/ble_advertisement.h"

#include <inttypes.h>

#include "platform_v2/public/logging.h"
#include "absl/strings/escaping.h"

namespace location {
namespace nearby {
namespace connections {

BleAdvertisement::BleAdvertisement(Version version, Pcp pcp,
                                   const ByteArray& service_id_hash,
                                   const std::string& endpoint_id,
                                   const std::string& endpoint_name,
                                   const std::string& bluetooth_mac_address) {
  if (version != Version::kV1 ||
      service_id_hash.size() != kServiceIdHashLength || endpoint_id.empty() ||
      endpoint_id.length() != kEndpointIdLength ||
      endpoint_name.length() > kMaxEndpointNameLength) {
    return;
  }

  switch (pcp) {
    case Pcp::kP2pCluster:  // Fall through
    case Pcp::kP2pStar:     // Fall through
    case Pcp::kP2pPointToPoint:
      break;
    default:
      return;
  }

  version_ = version;
  pcp_ = pcp;
  service_id_hash_ = service_id_hash;
  endpoint_id_ = endpoint_id;
  endpoint_name_ = endpoint_name;
  if (!BluetoothMacAddressHexStringToBytes(bluetooth_mac_address).Empty()) {
    bluetooth_mac_address_ = bluetooth_mac_address;
  }
}

BleAdvertisement::BleAdvertisement(const ByteArray& ble_advertisement_bytes) {
  if (ble_advertisement_bytes.Empty()) {
    NEARBY_LOG(ERROR,
               "Cannot deserialize BleAdvertisement: null bytes passed in.");
    return;
  }

  if (ble_advertisement_bytes.size() < kMinAdvertisementLength) {
    NEARBY_LOG(ERROR,
               "Cannot deserialize BleAdvertisement: expecting min %d raw "
               "bytes, got %" PRIu64,
               kMinAdvertisementLength, ble_advertisement_bytes.size());
    return;
  }

  // Start reading the bytes.
  auto* ble_advertisement_bytes_read_ptr = ble_advertisement_bytes.data();

  // The first 3 bits are supposed to be the version.
  version_ = static_cast<Version>(
      (*ble_advertisement_bytes_read_ptr & kVersionBitmask) >> 5);
  if (version_ != Version::kV1) {
    NEARBY_LOG(ERROR,
               "Cannot deserialize BleAdvertisement: unsupported Version %d",
               version_);
    return;
  }

  pcp_ = static_cast<Pcp>(*ble_advertisement_bytes_read_ptr & kPcpBitmask);
  ble_advertisement_bytes_read_ptr++;
  switch (pcp_) {
    case Pcp::kP2pCluster:  // Fall through
    case Pcp::kP2pStar:     // Fall through
    case Pcp::kP2pPointToPoint: {
      // The next 24 bits are supposed to be the service_id_hash.
      service_id_hash_ =
          ByteArray(ble_advertisement_bytes_read_ptr, kServiceIdHashLength);
      ble_advertisement_bytes_read_ptr += kServiceIdHashLength;

      // The next 32 bits are supposed to be the endpoint_id.
      endpoint_id_ =
          std::string(ble_advertisement_bytes_read_ptr, kEndpointIdLength);
      ble_advertisement_bytes_read_ptr += kEndpointIdLength;

      // The next 8 bits are the length of the endpoint name.
      auto expected_endpoint_name_length = static_cast<std::uint32_t>(
          *ble_advertisement_bytes_read_ptr & kEndpointNameLengthBitmask);
      ble_advertisement_bytes_read_ptr++;

      // The next x bits are the endpoint name. (Max length is 131 bytes).
      // Check that the stated endpoint_name_length is the same as what we
      // received (based off of the length of ble_advertisement_bytes).
      auto actual_endpoint_name_length =
          ComputeEndpointNameLength(ble_advertisement_bytes);
      if (actual_endpoint_name_length < expected_endpoint_name_length) {
        NEARBY_LOG(
            ERROR,
            "Cannot deserialize BleAdvertisement: expected endpointName to "
            "be %d bytes, got %d bytes",
            expected_endpoint_name_length, actual_endpoint_name_length);

        // Clear enpoint_id for validadity.
        endpoint_id_.clear();
        return;
      }
      endpoint_name_ = std::string(ble_advertisement_bytes_read_ptr,
                                   expected_endpoint_name_length);
      ble_advertisement_bytes_read_ptr += expected_endpoint_name_length;

      // The next 48 bits are the bluetooth mac address.
      auto bluetooth_mac_address_bytes = ByteArray(
          ble_advertisement_bytes_read_ptr, kBluetoothMacAddressLength);
      // If the Bluetooth MAC Address bytes are unset or invalid, leave the
      // string empty. Otherwise, convert it to the proper colon delimited
      // format.
      if (!IsBluetoothMacAddressUnset(bluetooth_mac_address_bytes)) {
        bluetooth_mac_address_ =
            HexBytesToColonDelimitedString(bluetooth_mac_address_bytes);
      }
      break;
    }

    default:
      // TODO(edwinwu): [ANALYTICIZE] This either represents corruption over
      // the air, or older versions of GmsCore intermingling with newer
      // ones.
      NEARBY_LOG(ERROR,
                 "Cannot deserialize BleAdvertisement: uunsupported V1 PCP %d",
                 pcp_);
      break;
  }
}

BleAdvertisement::operator ByteArray() const {
  if (!IsValid()) {
    return ByteArray();
  }

  std::string out;

  // The first 3 bits are the Version.
  char version_and_pcp_byte =
      (static_cast<char>(version_) << 5) & kVersionBitmask;
  // The next 5 bits are the Pcp.
  version_and_pcp_byte |= static_cast<char>(pcp_) & kPcpBitmask;
  out.reserve(1 + service_id_hash_.size() + kEndpointIdLength + 1 +
              endpoint_name_.size() + kBluetoothMacAddressLength);
  out.append(1, version_and_pcp_byte);
  out.append(std::string(service_id_hash_));
  out.append(endpoint_id_);
  out.append(1, endpoint_name_.size());
  out.append(endpoint_name_);
  // The next 48 bits are the bluetooth mac address. If bluetooth_mac_address is
  // invalid or empty, we get back a null byte array.
  auto bluetooth_mac_address_bytes(
      BluetoothMacAddressHexStringToBytes(bluetooth_mac_address_));
  if (!bluetooth_mac_address_bytes.Empty()) {
    out.append(bluetooth_mac_address_bytes.data(), kBluetoothMacAddressLength);
  }

  return ByteArray(std::move(out));
}

std::uint32_t BleAdvertisement::ComputeEndpointNameLength(
    const ByteArray& ble_advertisement_bytes) const {
  return ble_advertisement_bytes.size() - kMinAdvertisementLength;
}

ByteArray BleAdvertisement::BluetoothMacAddressHexStringToBytes(
    const std::string& bluetooth_mac_address) const {
  std::string bt_mac_address(bluetooth_mac_address);

  // Remove the colon delimiters.
  bt_mac_address.erase(
      std::remove(bt_mac_address.begin(), bt_mac_address.end(), ':'),
      bt_mac_address.end());

  // If the bluetooth mac address is invalid (wrong size), return a null byte
  // array.
  if (bt_mac_address.length() != kBluetoothMacAddressLength * 2) {
    return ByteArray();
  }

  // Convert to bytes. If MAC Address bytes are unset, return a null byte array.
  auto bt_mac_address_string(absl::HexStringToBytes(bt_mac_address));
  auto bt_mac_address_bytes =
      ByteArray(bt_mac_address_string.data(), bt_mac_address_string.size());
  if (IsBluetoothMacAddressUnset(bt_mac_address_bytes)) {
    return ByteArray();
  }
  return bt_mac_address_bytes;
}

std::string BleAdvertisement::HexBytesToColonDelimitedString(
    const ByteArray& hex_bytes) const {
  // Convert the hex bytes to a string.
  std::string colon_delimited_string(
      absl::BytesToHexString(std::string(hex_bytes.data(), hex_bytes.size())));
  absl::AsciiStrToUpper(&colon_delimited_string);

  // Insert the colons.
  for (int i = colon_delimited_string.length() - 2; i > 0; i -= 2) {
    colon_delimited_string.insert(i, ":");
  }
  return colon_delimited_string;
}

bool BleAdvertisement::IsBluetoothMacAddressUnset(
    const ByteArray& bluetooth_mac_address_bytes) const {
  for (int i = 0; i < bluetooth_mac_address_bytes.size(); i++) {
    if (bluetooth_mac_address_bytes.data()[i] != 0) {
      return false;
    }
  }
  return true;
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
