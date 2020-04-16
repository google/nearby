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

#include "core/internal/bluetooth_device_name.h"

#include <cstring>

#include "platform/base64_utils.h"

namespace location {
namespace nearby {
namespace connections {

const std::uint32_t BluetoothDeviceName::kServiceIdHashLength = 3;

const std::uint32_t BluetoothDeviceName::kMaxBluetoothDeviceNameLength = 147;
// Should be defined as ClientProxy<Platform>::kEndpointIdLength, but that
// involves making BluetoothDeviceName templatized on Platform just for
// that one little thing, so forego it (at least for now).
const std::uint32_t BluetoothDeviceName::kEndpointIdLength = 4;
const std::uint32_t BluetoothDeviceName::kReservedLength = 7;
const std::uint32_t BluetoothDeviceName::kMaxEndpointNameLength = 131;
const std::uint32_t BluetoothDeviceName::kMinBluetoothDeviceNameLength =
    kMaxBluetoothDeviceNameLength - kMaxEndpointNameLength;

const std::uint16_t BluetoothDeviceName::kVersionBitmask = 0x0E0;
const std::uint16_t BluetoothDeviceName::kPCPBitmask = 0x01F;
const std::uint16_t BluetoothDeviceName::kEndpointNameLengthBitmask = 0x0FF;

Ptr<BluetoothDeviceName> BluetoothDeviceName::fromString(
    const std::string& bluetooth_device_name_string) {
  ScopedPtr<Ptr<ByteArray> > scoped_bluetooth_device_name_bytes(
      Base64Utils::decode(bluetooth_device_name_string));
  if (scoped_bluetooth_device_name_bytes.isNull()) {
    // TODO(reznor): logger.atDebug().log("Cannot deserialize
    // BluetoothDeviceName: failed Base64 decoding of %s",
    // bluetoothDeviceNameString);
    return Ptr<BluetoothDeviceName>();
  }

  if (scoped_bluetooth_device_name_bytes->size() >
      kMaxBluetoothDeviceNameLength) {
    // TODO(reznor): logger.atDebug().log("Cannot deserialize
    // BluetoothDeviceName: expecting max %d raw bytes, got %d",
    // MAX_BLUETOOTH_DEVICE_NAME_LENGTH, bluetoothDeviceNameBytes.length);
    return Ptr<BluetoothDeviceName>();
  }

  if (scoped_bluetooth_device_name_bytes->size() <
      kMinBluetoothDeviceNameLength) {
    // TODO(reznor): logger.atDebug().log("Cannot deserialize
    // BluetoothDeviceName: expecting min %d raw bytes, got %d",
    // MIN_BLUETOOTH_DEVICE_NAME_LENGTH, bluetoothDeviceNameBytes.length);
    return Ptr<BluetoothDeviceName>();
  }

  // The first 3 bits are supposed to be the version.
  Version::Value version = static_cast<Version::Value>(
      (scoped_bluetooth_device_name_bytes->getData()[0] & kVersionBitmask) >>
      5);

  switch (version) {
    case Version::V1:
      return createV1BluetoothDeviceName(
          ConstifyPtr(scoped_bluetooth_device_name_bytes.get()));

    default:
      // TODO(reznor): [ANALYTICIZE] This either represents corruption over the
      // air, or older versions of GmsCore intermingling with newer ones.

      // TODO(reznor): logger.atDebug().log("Cannot deserialize
      // BluetoothDeviceName: unsupported Version %d", version);
      return Ptr<BluetoothDeviceName>();
  }
}

std::string BluetoothDeviceName::asString(Version::Value version,
                                          PCP::Value pcp,
                                          const std::string& endpoint_id,
                                          ConstPtr<ByteArray> service_id_hash,
                                          const std::string& endpoint_name) {
  std::string usable_endpoint_name(endpoint_name);
  if (endpoint_name.size() > kMaxEndpointNameLength) {
    // TODO(reznor): logger.atWarning().log("While serializing Advertisement,
    // truncating Endpoint Name %s (%d bytes) down to %d bytes", endpointName,
    // endpointNameBytes.length, MAX_ENDPOINT_NAME_LENGTH);
    usable_endpoint_name.erase(kMaxEndpointNameLength);
  }
  ScopedPtr<Ptr<ByteArray> > scoped_endpoint_name_bytes(
      new ByteArray(usable_endpoint_name.data(), usable_endpoint_name.size()));

  Ptr<ByteArray> bluetooth_device_name_bytes;
  switch (version) {
    case Version::V1:
      bluetooth_device_name_bytes =
          createV1Bytes(pcp, endpoint_id, service_id_hash,
                        ConstifyPtr(scoped_endpoint_name_bytes.get()));
      if (bluetooth_device_name_bytes.isNull()) {
        return "";
      }
      break;

    default:
      // TODO(reznor): logger.atDebug().log("Cannot serialize
      // BluetoothDeviceName: unsupported Version %d", version);
      return "";
  }
  ScopedPtr<Ptr<ByteArray> > scoped_bluetooth_device_name_bytes(
      bluetooth_device_name_bytes);

  // BluetoothDeviceName needs to be binary safe, so apply a Base64 encoding
  // over the raw bytes.
  return Base64Utils::encode(
      ConstifyPtr(scoped_bluetooth_device_name_bytes.get()));
}

Ptr<BluetoothDeviceName> BluetoothDeviceName::createV1BluetoothDeviceName(
    ConstPtr<ByteArray> bluetooth_device_name_bytes) {
  const char* bluetooth_device_name_bytes_read_ptr =
      bluetooth_device_name_bytes->getData();

  // The first 5 bits of the V1 payload are supposed to be the PCP.
  PCP::Value pcp = static_cast<PCP::Value>(
      *bluetooth_device_name_bytes_read_ptr & kPCPBitmask);
  bluetooth_device_name_bytes_read_ptr++;

  switch (pcp) {
    case PCP::P2P_CLUSTER:  // Fall through
    case PCP::P2P_STAR:     // Fall through
    case PCP::P2P_POINT_TO_POINT: {
      // The next 32 bits are supposed to be the endpoint_id.
      std::string endpoint_id(bluetooth_device_name_bytes_read_ptr,
                              kEndpointIdLength);
      bluetooth_device_name_bytes_read_ptr += kEndpointIdLength;

      // The next 24 bits are supposed to be the scoped_service_id_hash.
      ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(
          MakeConstPtr(new ByteArray(bluetooth_device_name_bytes_read_ptr,
                                     kServiceIdHashLength)));
      bluetooth_device_name_bytes_read_ptr += kServiceIdHashLength;

      // The next 56 bits are supposed to be reserved, and can be left
      // untouched.
      bluetooth_device_name_bytes_read_ptr += kReservedLength;

      // The next 8 bits are supposed to be the length of the endpoint_name.
      std::uint32_t expected_endpoint_name_length = static_cast<std::uint32_t>(
          *bluetooth_device_name_bytes_read_ptr & kEndpointNameLengthBitmask);
      bluetooth_device_name_bytes_read_ptr++;

      // Check that the stated endpoint_name_length is the same as what we
      // received (based off of the length of bluetooth_device_name_bytes).
      std::uint32_t actual_endpoint_name_length =
          computeEndpointNameLength(bluetooth_device_name_bytes);
      if (actual_endpoint_name_length != expected_endpoint_name_length) {
        // TODO(reznor): logger.atDebug().log("Cannot deserialize
        // BluetoothDeviceName: expected endpointName to be %d bytes, got %d
        // bytes", expectedEndpointNameLength, actualEndpointNameLength);
        return Ptr<BluetoothDeviceName>();
      }

      std::string endpoint_name(bluetooth_device_name_bytes_read_ptr,
                                actual_endpoint_name_length);
      bluetooth_device_name_bytes_read_ptr += actual_endpoint_name_length;

      return MakePtr(new BluetoothDeviceName(Version::V1, pcp, endpoint_id,
                                             scoped_service_id_hash.release(),
                                             endpoint_name));
    }
    default:
      // TODO(reznor): [ANALYTICIZE] This either represents corruption over the
      // air, or older versions of GmsCore intermingling with newer ones.

      // TODO(reznor): logger.atDebug().log("Cannot deserialize
      // BluetoothDeviceName: unsupported V1 PCP %d", pcp);
      return Ptr<BluetoothDeviceName>();
  }
}

std::uint32_t BluetoothDeviceName::computeEndpointNameLength(
    ConstPtr<ByteArray> bluetooth_device_name_bytes) {
  return kMaxEndpointNameLength -
         (kMaxBluetoothDeviceNameLength - bluetooth_device_name_bytes->size());
}

std::uint32_t BluetoothDeviceName::computeBluetoothDeviceNameLength(
    ConstPtr<ByteArray> endpoint_name_bytes) {
  return kMaxBluetoothDeviceNameLength -
         (kMaxEndpointNameLength - endpoint_name_bytes->size());
}

Ptr<ByteArray> BluetoothDeviceName::createV1Bytes(
    PCP::Value pcp, const std::string& endpoint_id,
    ConstPtr<ByteArray> service_id_hash,
    ConstPtr<ByteArray> endpoint_name_bytes) {
  std::uint32_t bluetooth_device_name_length =
      computeBluetoothDeviceNameLength(endpoint_name_bytes);
  Ptr<ByteArray> bluetooth_device_name_bytes{
      new ByteArray{bluetooth_device_name_length}};

  char* bluetooth_device_name_bytes_write_ptr =
      bluetooth_device_name_bytes->getData();

  // The first 3 bits are the Version.
  char version_and_pcp_byte =
      static_cast<char>((Version::V1 << 5) & kVersionBitmask);
  // The next 5 bits are the PCP.
  version_and_pcp_byte |= static_cast<char>(pcp & kPCPBitmask);
  *bluetooth_device_name_bytes_write_ptr = version_and_pcp_byte;
  bluetooth_device_name_bytes_write_ptr++;

  switch (pcp) {
    case PCP::P2P_CLUSTER:  // Fall through
    case PCP::P2P_STAR:     // Fall through
    case PCP::P2P_POINT_TO_POINT:
      // The next 32 bits are the endpoint_id.
      if (endpoint_id.size() != kEndpointIdLength) {
        // TODO(reznor): logger.atDebug().log("Cannot serialize
        // BluetoothDeviceName: V1 Endpoint ID %s (%d bytes) should be exactly
        // %d bytes", endpointId, endpointId.length(), ENDPOINT_ID_LENGTH);
        return Ptr<ByteArray>();
      }
      memcpy(bluetooth_device_name_bytes_write_ptr, endpoint_id.data(),
             kEndpointIdLength);
      bluetooth_device_name_bytes_write_ptr += kEndpointIdLength;

      // The next 24 bits are the service_id_hash.
      if (service_id_hash->size() != kServiceIdHashLength) {
        // TODO(reznor): logger.atDebug().log("Cannot serialize
        // BluetoothDeviceName: V1 ServiceID hash (%d bytes) should be exactly
        // %d bytes", serviceIdHash.length, SERVICE_ID_HASH_LENGTH);
        return Ptr<ByteArray>();
      }
      memcpy(bluetooth_device_name_bytes_write_ptr, service_id_hash->getData(),
             kServiceIdHashLength);
      bluetooth_device_name_bytes_write_ptr += kServiceIdHashLength;

      // The next 56 bits are reserved, and should all be zeroed out, so do
      // that and then jump over 56 bits to position things for the next write.
      memset(bluetooth_device_name_bytes_write_ptr, 0, kReservedLength);
      bluetooth_device_name_bytes_write_ptr += kReservedLength;

      // The next 8 bits are the length of the endpoint_name.
      *bluetooth_device_name_bytes_write_ptr = static_cast<char>(
          endpoint_name_bytes->size() & kEndpointNameLengthBitmask);
      bluetooth_device_name_bytes_write_ptr++;

      // The remaining bits are filled with the endpoint_name.
      memcpy(bluetooth_device_name_bytes_write_ptr,
             endpoint_name_bytes->getData(), endpoint_name_bytes->size());
      bluetooth_device_name_bytes_write_ptr += endpoint_name_bytes->size();

      break;
    default:
      // TODO(reznor): logger.atDebug().log("Cannot serialize
      // BluetoothDeviceName: unsupported V1 PCP %d", pcp);
      return Ptr<ByteArray>();
  }

  return bluetooth_device_name_bytes;
}

BluetoothDeviceName::BluetoothDeviceName(Version::Value version, PCP::Value pcp,
                                         const std::string& endpoint_id,
                                         ConstPtr<ByteArray> service_id_hash,
                                         const std::string& endpoint_name)
    : version_(version),
      pcp_(pcp),
      endpoint_id_(endpoint_id),
      service_id_hash_(service_id_hash),
      endpoint_name_(endpoint_name) {}

BluetoothDeviceName::~BluetoothDeviceName() {
  // Nothing to do.
}

BluetoothDeviceName::Version::Value BluetoothDeviceName::getVersion() const {
  return version_;
}

PCP::Value BluetoothDeviceName::getPCP() const { return pcp_; }

std::string BluetoothDeviceName::getEndpointId() const { return endpoint_id_; }

ConstPtr<ByteArray> BluetoothDeviceName::getServiceIdHash() const {
  return service_id_hash_.get();
}

std::string BluetoothDeviceName::getEndpointName() const {
  return endpoint_name_;
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
