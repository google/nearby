// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "core/internal/ble_advertisement.h"

#include <algorithm>

#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"

namespace location {
namespace nearby {
namespace connections {

const std::uint32_t BLEAdvertisement::kServiceIdHashLength = 3;

const std::uint32_t BLEAdvertisement::kVersionAndPcpLength = 1;
// Should be defined as EndpointManager<Platform>::kEndpointIdLength, but that
// involves making BLEAdvertisement templatized on Platform just for
// that one little thing, so forego it (at least for now).
const std::uint32_t BLEAdvertisement::kEndpointIdLength = 4;
const std::uint32_t BLEAdvertisement::kEndpointNameSizeLength = 1;
const std::uint32_t BLEAdvertisement::kBluetoothMacAddressLength = 6;
const std::uint32_t BLEAdvertisement::kMinAdvertisementLength =
    kVersionAndPcpLength + kServiceIdHashLength + kEndpointIdLength +
    kEndpointNameSizeLength + kBluetoothMacAddressLength;
const std::uint32_t BLEAdvertisement::kMaxEndpointNameLength = 131;

const std::uint16_t BLEAdvertisement::kVersionBitmask = 0x0E0;
const std::uint16_t BLEAdvertisement::kPCPBitmask = 0x01F;
const std::uint16_t BLEAdvertisement::kEndpointNameLengthBitmask = 0x0FF;

Ptr<BLEAdvertisement> BLEAdvertisement::fromBytes(
    ConstPtr<ByteArray> ble_advertisement_bytes) {
  if (ble_advertisement_bytes.isNull()) {
    // TODO(ahlee): Logger.atDebug().log("Cannot deserialize BleAdvertisement:
    // null bytes passed in.");
    return Ptr<BLEAdvertisement>();
  }

  if (ble_advertisement_bytes->size() < kMinAdvertisementLength) {
    // TODO(ahlee): Logger.atDebug().log("Cannot deserialize BleAdvertisement:
    // expecting min %d raw bytes, got %d", kMinAdvertisementLength,
    // ble_advertisement_bytes->size());
    return Ptr<BLEAdvertisement>();
  }

  // Start reading the bytes.
  const char* ble_advertisement_bytes_read_ptr =
      ble_advertisement_bytes->getData();

  // The first 3 bits are supposed to be the version.
  Version::Value version = static_cast<Version::Value>(
      (*ble_advertisement_bytes_read_ptr & kVersionBitmask) >> 5);
  if (version != Version::V1) {
    // TODO(ahlee): logger.atDebug().log("Cannot deserialize BleAdvertisement:
    // unsupported Version %d", version);
    return Ptr<BLEAdvertisement>();
  }

  PCP::Value pcp =
      static_cast<PCP::Value>(*ble_advertisement_bytes_read_ptr & kPCPBitmask);
  ble_advertisement_bytes_read_ptr++;
  if (pcp != PCP::P2P_CLUSTER && pcp != PCP::P2P_STAR &&
      pcp != PCP::P2P_POINT_TO_POINT) {
    // TODO(ahlee): logger.atDebug().log("Cannot deserialize BleAdvertisement:
    // unsupported V1 PCP %d", pcp);
    return Ptr<BLEAdvertisement>();
  }

  // Avoid leaks.
  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(MakeConstPtr(
      new ByteArray(ble_advertisement_bytes_read_ptr, kServiceIdHashLength)));
  ble_advertisement_bytes_read_ptr += kServiceIdHashLength;

  std::string endpoint_id(ble_advertisement_bytes_read_ptr, kEndpointIdLength);
  ble_advertisement_bytes_read_ptr += kEndpointIdLength;

  std::uint32_t expected_endpoint_name_length = static_cast<std::uint32_t>(
      *ble_advertisement_bytes_read_ptr & kEndpointNameLengthBitmask);
  ble_advertisement_bytes_read_ptr++;

  // Check that the stated endpoint_name_length is the same as what we
  // received (based off of the length of ble_advertisement_bytes).
  std::uint32_t actual_endpoint_name_length =
      computeEndpointNameLength(ble_advertisement_bytes);
  if (actual_endpoint_name_length < expected_endpoint_name_length) {
    // TODO(ahlee): Logger.atDebug().log("Cannot deserialize BleAdvertisement:
    // expected endpointName to be %d bytes, got %d bytes",
    // expected_endpoint_name_length, actual_endpoint_name_length);
    return Ptr<BLEAdvertisement>();
  }

  std::string endpoint_name(ble_advertisement_bytes_read_ptr,
                            expected_endpoint_name_length);
  ble_advertisement_bytes_read_ptr += expected_endpoint_name_length;

  // Avoid leaks.
  ScopedPtr<ConstPtr<ByteArray> > scoped_bluetooth_mac_address_bytes(
      MakeConstPtr(new ByteArray(ble_advertisement_bytes_read_ptr,
                                 kBluetoothMacAddressLength)));
  std::string bluetooth_mac_address;
  // If the Bluetooth MAC Address bytes are unset or invalid, leave the string
  // empty. Otherwise, convert it to the proper colon delimited format.
  if (!isBluetoothMacAddressUnset(scoped_bluetooth_mac_address_bytes.get())) {
    bluetooth_mac_address = hexBytesToColonDelimitedString(
        scoped_bluetooth_mac_address_bytes.get());
  }

  return MakePtr(
      new BLEAdvertisement(version, pcp, scoped_service_id_hash.release(),
                           endpoint_id, endpoint_name, bluetooth_mac_address));
}

ConstPtr<ByteArray> BLEAdvertisement::toBytes(
    Version::Value version, PCP::Value pcp, ConstPtr<ByteArray> service_id_hash,
    const std::string& endpoint_id, const std::string& endpoint_name,
    const std::string& bluetooth_mac_address) {
  if (version != Version::V1) {
    // TODO(ahlee): logger.atDebug().log("Cannot serialize BleAdvertisement:
    // unsupported Version %d", version);
    return ConstPtr<ByteArray>();
  }

  if (pcp != PCP::P2P_CLUSTER && pcp != PCP::P2P_STAR &&
      pcp != PCP::P2P_POINT_TO_POINT) {
    // TODO(ahlee): logger.atDebug().log("Cannot serialize BleAdvertisement:
    // unsupported V1 PCP %d", pcp);
    return ConstPtr<ByteArray>();
  }

  if (endpoint_name.size() > kMaxEndpointNameLength) {
    // TODO(ahlee): logger.atDebug().log("Cannot serialize BleAdvertisement:
    // expected an endpointName of at most %d bytes but got %d",
    // kMaxEndpoingNameLength, endpoint_name.size());
    return ConstPtr<ByteArray>();
  }

  std::uint32_t ble_advertisement_length =
      computeAdvertisementLength(endpoint_name);
  Ptr<ByteArray> ble_advertisement_bytes{
      new ByteArray{ble_advertisement_length}};
  char* ble_advertisement_bytes_write_ptr = ble_advertisement_bytes->getData();

  // The first 3 bits are the Version.
  char version_and_pcp_byte =
      static_cast<char>((version << 5) & kVersionBitmask);
  // The next 5 bits are the PCP.
  version_and_pcp_byte |= static_cast<char>(pcp & kPCPBitmask);
  *ble_advertisement_bytes_write_ptr = version_and_pcp_byte;
  ble_advertisement_bytes_write_ptr++;

  // The next 24 bits are the service id hash.
  memcpy(ble_advertisement_bytes_write_ptr, service_id_hash->getData(),
         kServiceIdHashLength);
  ble_advertisement_bytes_write_ptr += kServiceIdHashLength;

  // The next 32 bits are the endpoint id.
  memcpy(ble_advertisement_bytes_write_ptr, endpoint_id.data(),
         kEndpointIdLength);
  ble_advertisement_bytes_write_ptr += kEndpointIdLength;

  // The next 8 bits are the length of the endpoint name.
  *ble_advertisement_bytes_write_ptr =
      static_cast<char>(endpoint_name.size() & kEndpointNameLengthBitmask);
  ble_advertisement_bytes_write_ptr++;

  // The next x bits are the endpoint name. (Max length is 131 bytes).
  memcpy(ble_advertisement_bytes_write_ptr, endpoint_name.data(),
         endpoint_name.size());
  ble_advertisement_bytes_write_ptr += endpoint_name.size();

  // The next 48 bits are the bluetooth mac address. If bluetooth_mac_address is
  // invalid or empty, we get back a null byte array.
  // Avoid leaks.
  ScopedPtr<ConstPtr<ByteArray> > scoped_bluetooth_mac_address_bytes(
      bluetoothMacAddressToHexBytes(bluetooth_mac_address));
  if (!scoped_bluetooth_mac_address_bytes.isNull()) {
    memcpy(ble_advertisement_bytes_write_ptr,
           scoped_bluetooth_mac_address_bytes->getData(),
           kBluetoothMacAddressLength);
  }
  ble_advertisement_bytes_write_ptr += kBluetoothMacAddressLength;

  return ConstifyPtr(ble_advertisement_bytes);
}

std::string BLEAdvertisement::hexBytesToColonDelimitedString(
    ConstPtr<ByteArray> hex_bytes) {
  // Convert the hex bytes to a string.
  std::string colon_delimited_string(absl::BytesToHexString(
      hex_bytes->asString()));
  absl::AsciiStrToUpper(&colon_delimited_string);

  // Insert the colons.
  for (int i = colon_delimited_string.length() - 2; i > 0; i -= 2) {
    colon_delimited_string.insert(i, ":");
  }
  return colon_delimited_string;
}

// TODO(ahlee): Rename to bluetoothMacAddressHexStringToBytes
ConstPtr<ByteArray> BLEAdvertisement::bluetoothMacAddressToHexBytes(
    const std::string& bluetooth_mac_address) {
  std::string bt_mac_address(bluetooth_mac_address);

  // Remove the colon delimiters.
  bt_mac_address.erase(
      std::remove(bt_mac_address.begin(), bt_mac_address.end(), ':'),
      bt_mac_address.end());

  // If the bluetooth mac address is invalid (wrong size), return a null byte
  // array.
  if (bt_mac_address.length() != kBluetoothMacAddressLength * 2) {
    return ConstPtr<ByteArray>();
  }

  // Convert to bytes.
  std::string bt_mac_address_bytes(absl::HexStringToBytes(bt_mac_address));
  return MakeConstPtr(
      new ByteArray(bt_mac_address_bytes.data(), bt_mac_address_bytes.size()));
}

bool BLEAdvertisement::isBluetoothMacAddressUnset(
    ConstPtr<ByteArray> bluetooth_mac_address_bytes) {
  for (int i = 0; i < bluetooth_mac_address_bytes->size(); i++) {
    if (bluetooth_mac_address_bytes->getData()[i] != 0) {
      return false;
    }
  }
  return true;
}

std::uint32_t BLEAdvertisement::computeEndpointNameLength(
    ConstPtr<ByteArray> ble_advertisement_bytes) {
  return ble_advertisement_bytes->size() - kMinAdvertisementLength;
}

std::uint32_t BLEAdvertisement::computeAdvertisementLength(
    const std::string& endpoint_name) {
  return kMinAdvertisementLength + endpoint_name.size();
}

BLEAdvertisement::BLEAdvertisement(Version::Value version, PCP::Value pcp,
                                   ConstPtr<ByteArray> service_id_hash,
                                   const std::string& endpoint_id,
                                   const std::string& endpoint_name,
                                   const std::string& bluetooth_mac_address)
    : version_(version),
      pcp_(pcp),
      service_id_hash_(service_id_hash),
      endpoint_id_(endpoint_id),
      endpoint_name_(endpoint_name),
      bluetooth_mac_address_(bluetooth_mac_address) {}

BLEAdvertisement::~BLEAdvertisement() {
  // Nothing to do.
}

BLEAdvertisement::Version::Value BLEAdvertisement::getVersion() const {
  return version_;
}

PCP::Value BLEAdvertisement::getPCP() const { return pcp_; }

std::string BLEAdvertisement::getEndpointId() const { return endpoint_id_; }

ConstPtr<ByteArray> BLEAdvertisement::getServiceIdHash() const {
  return service_id_hash_.get();
}

std::string BLEAdvertisement::getEndpointName() const { return endpoint_name_; }

std::string BLEAdvertisement::getBluetoothMacAddress() const {
  return bluetooth_mac_address_;
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
