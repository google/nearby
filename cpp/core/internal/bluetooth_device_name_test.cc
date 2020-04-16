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
#include "platform/port/string.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

const BluetoothDeviceName::Version::Value version =
    BluetoothDeviceName::Version::V1;
const PCP::Value pcp = PCP::P2P_CLUSTER;
const char endpoint_id[] = "AB12";
const char service_id_hash_bytes[] = {0x0A, 0x0B, 0x0C};
const char endpoint_name[] = "RAWK + ROWL!";

TEST(BluetoothDeviceNameTest, SerializationDeserializationWorks) {
  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  std::string bluetooth_device_name_string = BluetoothDeviceName::asString(
      version, pcp, endpoint_id, ConstifyPtr(scoped_service_id_hash.get()),
      endpoint_name);
  ScopedPtr<Ptr<BluetoothDeviceName> > scoped_bluetooth_device_name(
      BluetoothDeviceName::fromString(bluetooth_device_name_string));

  ASSERT_EQ(pcp, scoped_bluetooth_device_name->getPCP());
  ASSERT_EQ(version, scoped_bluetooth_device_name->getVersion());
  ASSERT_EQ(endpoint_id, scoped_bluetooth_device_name->getEndpointId());
  ASSERT_EQ(sizeof(service_id_hash_bytes) / sizeof(char),
            scoped_bluetooth_device_name->getServiceIdHash()->size());
  ASSERT_EQ(0,
            memcmp(service_id_hash_bytes,
                   scoped_bluetooth_device_name->getServiceIdHash()->getData(),
                   scoped_bluetooth_device_name->getServiceIdHash()->size()));
  ASSERT_EQ(endpoint_name, scoped_bluetooth_device_name->getEndpointName());
}

TEST(BluetoothDeviceNameTest,
     SerializationDeserializationWorksWithEmptyEndpointName) {
  std::string empty_endpoint_name;

  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  std::string bluetooth_device_name_string = BluetoothDeviceName::asString(
      version, pcp, endpoint_id, ConstifyPtr(scoped_service_id_hash.get()),
      empty_endpoint_name);
  ScopedPtr<Ptr<BluetoothDeviceName> > scoped_bluetooth_device_name(
      BluetoothDeviceName::fromString(bluetooth_device_name_string));

  ASSERT_EQ(pcp, scoped_bluetooth_device_name->getPCP());
  ASSERT_EQ(version, scoped_bluetooth_device_name->getVersion());
  ASSERT_EQ(endpoint_id, scoped_bluetooth_device_name->getEndpointId());
  ASSERT_EQ(sizeof(service_id_hash_bytes) / sizeof(char),
            scoped_bluetooth_device_name->getServiceIdHash()->size());
  ASSERT_EQ(0,
            memcmp(service_id_hash_bytes,
                   scoped_bluetooth_device_name->getServiceIdHash()->getData(),
                   scoped_bluetooth_device_name->getServiceIdHash()->size()));
  ASSERT_EQ(empty_endpoint_name,
            scoped_bluetooth_device_name->getEndpointName());
}

TEST(BluetoothDeviceNameTest, SerializationFailsWithBadVersion) {
  BluetoothDeviceName::Version::Value bad_version =
      static_cast<BluetoothDeviceName::Version::Value>(666);

  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  std::string bluetooth_device_name_string = BluetoothDeviceName::asString(
      bad_version, pcp, endpoint_id, ConstifyPtr(scoped_service_id_hash.get()),
      endpoint_name);

  ASSERT_TRUE(bluetooth_device_name_string.empty());
}

TEST(BluetoothDeviceNameTest, SerializationFailsWithBadPCP) {
  PCP::Value bad_pcp = static_cast<PCP::Value>(666);

  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  std::string bluetooth_device_name_string = BluetoothDeviceName::asString(
      version, bad_pcp, endpoint_id, ConstifyPtr(scoped_service_id_hash.get()),
      endpoint_name);

  ASSERT_TRUE(bluetooth_device_name_string.empty());
}

TEST(BluetoothDeviceNameTest, SerializationFailsWithShortEndpointId) {
  std::string short_endpoint_id("AB1");

  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  std::string bluetooth_device_name_string = BluetoothDeviceName::asString(
      version, pcp, short_endpoint_id,
      ConstifyPtr(scoped_service_id_hash.get()), endpoint_name);

  ASSERT_TRUE(bluetooth_device_name_string.empty());
}

TEST(BluetoothDeviceNameTest, SerializationFailsWithLongEndpointId) {
  std::string long_endpoint_id("AB12X");

  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  std::string bluetooth_device_name_string = BluetoothDeviceName::asString(
      version, pcp, long_endpoint_id, ConstifyPtr(scoped_service_id_hash.get()),
      endpoint_name);

  ASSERT_TRUE(bluetooth_device_name_string.empty());
}

TEST(BluetoothDeviceNameTest, SerializationFailsWithShortServiceIdHash) {
  char short_service_id_hash_bytes[] = {0x0A, 0x0B};

  ScopedPtr<Ptr<ByteArray> > scoped_short_service_id_hash(
      new ByteArray(short_service_id_hash_bytes,
                    sizeof(short_service_id_hash_bytes) / sizeof(char)));

  std::string bluetooth_device_name_string = BluetoothDeviceName::asString(
      version, pcp, endpoint_id,
      ConstifyPtr(scoped_short_service_id_hash.get()), endpoint_name);

  ASSERT_TRUE(bluetooth_device_name_string.empty());
}

TEST(BluetoothDeviceNameTest, SerializationFailsWithLongServiceIdHash) {
  char long_service_id_hash_bytes[] = {0x0A, 0x0B, 0x0C, 0x0D};

  ScopedPtr<Ptr<ByteArray> > scoped_long_service_id_hash(
      new ByteArray(long_service_id_hash_bytes,
                    sizeof(long_service_id_hash_bytes) / sizeof(char)));

  std::string bluetooth_device_name_string = BluetoothDeviceName::asString(
      version, pcp, endpoint_id, ConstifyPtr(scoped_long_service_id_hash.get()),
      endpoint_name);

  ASSERT_TRUE(bluetooth_device_name_string.empty());
}

TEST(BluetoothDeviceNameTest, DeserializationFailsWithShortLength) {
  char bluetooth_device_name_bytes[] = {'X'};

  ScopedPtr<Ptr<ByteArray> > scoped_bluetooth_device_name_bytes(
      new ByteArray(bluetooth_device_name_bytes,
                    sizeof(bluetooth_device_name_bytes) / sizeof(char)));

  ScopedPtr<Ptr<BluetoothDeviceName> > scoped_bluetooth_device_name(
      BluetoothDeviceName::fromString(Base64Utils::encode(
          ConstifyPtr(scoped_bluetooth_device_name_bytes.get()))));

  ASSERT_TRUE(scoped_bluetooth_device_name.isNull());
}

TEST(BluetoothDeviceNameTest, DeserializationFailsWithWrongEndpointNameLength) {
  // Serialize good data into a good Bluetooth Device Name.
  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  std::string bluetooth_device_name_string = BluetoothDeviceName::asString(
      version, pcp, endpoint_id, ConstifyPtr(scoped_service_id_hash.get()),
      endpoint_name);

  // Base64-decode the good Bluetooth Device Name.
  ScopedPtr<Ptr<ByteArray> > scoped_bluetooth_device_name_bytes(
      Base64Utils::decode(bluetooth_device_name_string));
  // Corrupt the EndpointNameLength bits (120-127) by reversing all of them.
  std::string corrupt_bluetooth_device_name_bytes(
      scoped_bluetooth_device_name_bytes->getData(),
      scoped_bluetooth_device_name_bytes->size());
  corrupt_bluetooth_device_name_bytes[15] ^= 0x0FF;
  // Base64-encode the corrupted bytes into a corrupt Bluetooth Device Name.
  ScopedPtr<Ptr<ByteArray> > scoped_corrupt_bluetooth_device_name_bytes(
      new ByteArray(corrupt_bluetooth_device_name_bytes.data(),
                    corrupt_bluetooth_device_name_bytes.size()));
  std::string corrupt_bluetooth_device_name_string(Base64Utils::encode(
      ConstifyPtr(scoped_corrupt_bluetooth_device_name_bytes.get())));

  // And deserialize the corrupt Bluetooth Device Name.
  ScopedPtr<Ptr<BluetoothDeviceName> > scoped_bluetooth_device_name(
      BluetoothDeviceName::fromString(corrupt_bluetooth_device_name_string));

  ASSERT_TRUE(scoped_bluetooth_device_name.isNull());
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
