// Copyright 2022 Google LLC
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

#include "internal/platform/implementation/windows/ble.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "connections/implementation/mediums/ble/ble_advertisement_header.h"
#include "connections/implementation/mediums/ble/bloom_filter.h"
#include "connections/implementation/mediums/utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/mac_address.h"
#include "internal/platform/uuid.h"
#include "winrt/Windows.Devices.Bluetooth.Advertisement.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.Storage.Streams.h"
#include "winrt/base.h"

namespace nearby {
namespace windows {
namespace {
using ::nearby::connections::mediums::BitSetImpl;
using ::nearby::connections::mediums::BleAdvertisementHeader;
using ::nearby::connections::mediums::BloomFilter;
using ::testing::NiceMock;
using ::testing::Return;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisement;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementDataSection;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementReceivedEventArgs;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementType;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementReceivedEventArgs;
using ::winrt::Windows::Storage::Streams::ByteOrder;
using ::winrt::Windows::Storage::Streams::DataWriter;

constexpr uint64_t kTestServiceUuidMsb = 0x0000ABCD00001000;
constexpr uint64_t kTestServiceUuidLsb = 0x800000805F9B34FB;
constexpr Uuid kTestServiceUuid(kTestServiceUuidMsb, kTestServiceUuidLsb);

constexpr uint64_t kCopresenceServiceUuidMsb = 0x0000FEF300001000;
constexpr uint64_t kCopresenceServiceUuidLsb = 0x800000805F9B34FB;
constexpr Uuid kCopresenceServiceUuid(kCopresenceServiceUuidMsb,
                                      kCopresenceServiceUuidLsb);

constexpr uint16_t kAlternateUuid16 = 0x1234;
constexpr uint16_t kSecondAlternateUuid16 = 0x5678;
constexpr Uuid kAlternateUuid(0x0000123400001000, 0x800000805F9B34FB);
constexpr uint8_t kUuid16ServiceDataType = 0x16;
constexpr uint64_t kValidBluetoothAddress = 0x0000112233445566ULL;
constexpr uint64_t kInvalidBluetoothAddress = 0x0001112233445566ULL;
constexpr absl::string_view kTestServiceId = "test_service_id";
constexpr absl::string_view kSecondTestServiceId = "second_test_service_id";

class FakeAdvertisementReceivedEventArgs
    : public winrt::implements<FakeAdvertisementReceivedEventArgs,
                               IBluetoothLEAdvertisementReceivedEventArgs> {
 public:
  FakeAdvertisementReceivedEventArgs(uint64_t bluetooth_address,
                                     BluetoothLEAdvertisement advertisement)
      : bluetooth_address_(bluetooth_address),
        advertisement_(std::move(advertisement)) {}

  int16_t RawSignalStrengthInDBm() const { return -50; }
  uint64_t BluetoothAddress() const { return bluetooth_address_; }
  BluetoothLEAdvertisementType AdvertisementType() const {
    return BluetoothLEAdvertisementType::ConnectableUndirected;
  }
  winrt::Windows::Foundation::DateTime Timestamp() const { return {}; }
  BluetoothLEAdvertisement Advertisement() const { return advertisement_; }

 private:
  uint64_t bluetooth_address_;
  BluetoothLEAdvertisement advertisement_;
};

BluetoothLEAdvertisementReceivedEventArgs CreateReceivedEventArgs(
    uint64_t bluetooth_address, BluetoothLEAdvertisement advertisement) {
  return winrt::make<FakeAdvertisementReceivedEventArgs>(
             bluetooth_address, std::move(advertisement))
      .as<BluetoothLEAdvertisementReceivedEventArgs>();
}

void AppendUuid16ServiceDataSection(BluetoothLEAdvertisement& advertisement,
                                    uint16_t uuid16,
                                    absl::string_view payload) {
  DataWriter data_writer;
  data_writer.ByteOrder(ByteOrder::LittleEndian);
  data_writer.WriteUInt16(uuid16);
  for (char byte : payload) {
    data_writer.WriteByte(static_cast<uint8_t>(byte));
  }
  BluetoothLEAdvertisementDataSection section(kUuid16ServiceDataType,
                                              data_writer.DetachBuffer());
  advertisement.DataSections().Append(section);
}

class MockBluetoothAdapter : public BluetoothAdapter {
 public:
  MOCK_METHOD(bool, SetStatus, (Status status), (override));
  MOCK_METHOD(bool, IsEnabled, (), (const, override));
  MOCK_METHOD(ScanMode, GetScanMode, (), (const, override));
  MOCK_METHOD(bool, SetScanMode, (ScanMode scan_mode), (override));
  MOCK_METHOD(std::string, GetName, (), (const, override));
  MOCK_METHOD(bool, SetName, (absl::string_view name, bool persist),
              (override));
  MOCK_METHOD(MacAddress, GetMacAddress, (), (const, override));
  MOCK_METHOD(bool, IsExtendedAdvertisingSupported, (), (const, override));
};

}  // namespace

class BleMediumTest : public ::testing::Test {
 protected:
  BleMediumTest() {
    ble_medium_ = std::make_unique<BleMedium>(bluetooth_adapter_);
  }

 protected:
  size_t GetWatchersCount() {
    absl::MutexLock lock(ble_medium_->mutex_);
    return ble_medium_->watchers_.size();
  }

  std::unique_ptr<api::ble::BleMedium::ScanningSession>
  StartBleV2ScanningForTest(const Uuid& service_uuid,
                            api::ble::BleMedium::ScanningCallback callback) {
    {
      absl::MutexLock lock(ble_medium_->mutex_);
      if (ble_medium_->watchers_.empty()) {
        ble_medium_->watchers_.push_back(nullptr);
      }
    }
    return ble_medium_->StartScanning(
        service_uuid, api::ble::TxPowerLevel::kHigh, std::move(callback));
  }

  void DeliverAdvertisementFound(
      const BluetoothLEAdvertisementReceivedEventArgs& args) {
    ble_medium_->AdvertisementFoundHandler(nullptr, args);
  }

  NiceMock<MockBluetoothAdapter> bluetooth_adapter_;
  std::unique_ptr<BleMedium> ble_medium_;
};

namespace {

TEST_F(BleMediumTest, StartScanningAdapterDisabled) {
  EXPECT_CALL(bluetooth_adapter_, IsEnabled()).WillOnce(Return(false));
  Uuid service_uuid = kTestServiceUuid;
  api::ble::BleMedium::ScanCallback callback;

  EXPECT_EQ(false, ble_medium_->StartScanning(service_uuid,
                                              api::ble::TxPowerLevel::kHigh,
                                              std::move(callback)));

  EXPECT_EQ(GetWatchersCount(), 0);
}

TEST_F(BleMediumTest, StartScanningAdapterNoAdapterOnForge) {
  EXPECT_CALL(bluetooth_adapter_, IsEnabled()).WillOnce(Return(true));
  Uuid service_uuid = kTestServiceUuid;
  api::ble::BleMedium::ScanCallback callback;

  EXPECT_EQ(false, ble_medium_->StartScanning(service_uuid,
                                              api::ble::TxPowerLevel::kHigh,
                                              std::move(callback)));

  EXPECT_EQ(GetWatchersCount(), 0);
}

TEST_F(BleMediumTest, StartBleV2ScanningAdapterDisabled) {
  EXPECT_CALL(bluetooth_adapter_, IsEnabled()).WillOnce(Return(false));
  api::ble::BleMedium::ScanningCallback callback;

  EXPECT_EQ(ble_medium_->StartScanning(kCopresenceServiceUuid,
                                       api::ble::TxPowerLevel::kHigh,
                                       std::move(callback)),
            nullptr);
  EXPECT_EQ(GetWatchersCount(), 0);
}

TEST_F(BleMediumTest,
       AdvertisementFoundHandlerDeliversAlternateServiceUuidHeader) {
  EXPECT_CALL(bluetooth_adapter_, IsEnabled()).WillRepeatedly(Return(true));
  EXPECT_CALL(bluetooth_adapter_, IsExtendedAdvertisingSupported())
      .WillRepeatedly(Return(false));

  ble_medium_->AddAlternateUuidForService(kAlternateUuid16,
                                          std::string(kTestServiceId));

  int found_count = 0;
  api::ble::BlePeripheral::UniqueId found_peripheral_id = 0;
  api::ble::BleAdvertisementData found_data;
  auto session = StartBleV2ScanningForTest(
      kCopresenceServiceUuid,
      {
          .advertisement_found_cb =
              [&](api::ble::BlePeripheral::UniqueId peripheral_id,
                  api::ble::BleAdvertisementData advertisement_data) {
                ++found_count;
                found_peripheral_id = peripheral_id;
                found_data = std::move(advertisement_data);
              },
      });
  ASSERT_NE(session, nullptr);

  // Also include a malformed (< 2-byte) 0x16 section to verify safe handling.
  BluetoothLEAdvertisement advertisement;
  DataWriter short_writer;
  short_writer.WriteByte(0x01);
  advertisement.DataSections().Append(BluetoothLEAdvertisementDataSection(
      kUuid16ServiceDataType, short_writer.DetachBuffer()));
  AppendUuid16ServiceDataSection(advertisement, kAlternateUuid16, "alt_data");

  DeliverAdvertisementFound(
      CreateReceivedEventArgs(kValidBluetoothAddress, advertisement));

  ASSERT_EQ(found_count, 1);
  EXPECT_EQ(found_peripheral_id, kValidBluetoothAddress);
  EXPECT_FALSE(found_data.is_extended_advertisement);
  ASSERT_TRUE(found_data.service_data.contains(kCopresenceServiceUuid));

  BleAdvertisementHeader header(
      found_data.service_data.at(kCopresenceServiceUuid));
  ASSERT_TRUE(header.IsValid());
  EXPECT_EQ(header.GetVersion(), BleAdvertisementHeader::Version::kV2);
  EXPECT_FALSE(header.IsSupportExtendedAdvertisement());
  EXPECT_EQ(header.GetNumSlots(), 1);
  constexpr size_t kBloomFilterBytes =
      BleAdvertisementHeader::kServiceIdBloomFilterByteLength;
  BloomFilter bloom_filter(std::make_unique<BitSetImpl<kBloomFilterBytes>>(),
                           header.GetServiceIdBloomFilter());
  EXPECT_TRUE(bloom_filter.PossiblyContains(std::string(kTestServiceId)));
  EXPECT_FALSE(bloom_filter.PossiblyContains("unrelated_service"));

  MacAddress expected_mac;
  ASSERT_TRUE(MacAddress::FromUint64(kValidBluetoothAddress, expected_mac));
  EXPECT_EQ(header.GetAdvertisementHash(),
            connections::Utils::Sha256Hash(
                expected_mac.ToString(),
                BleAdvertisementHeader::kAdvertisementHashByteLength));

  EXPECT_TRUE(session->stop_scanning().ok());
}

TEST_F(BleMediumTest,
       AdvertisementFoundHandlerDeliversAlternateUuidFromServiceUuidsList) {
  EXPECT_CALL(bluetooth_adapter_, IsEnabled()).WillRepeatedly(Return(true));
  EXPECT_CALL(bluetooth_adapter_, IsExtendedAdvertisingSupported())
      .WillRepeatedly(Return(false));

  ble_medium_->AddAlternateUuidForService(kAlternateUuid16,
                                          std::string(kTestServiceId));

  int found_count = 0;
  api::ble::BleAdvertisementData found_data;
  auto session = StartBleV2ScanningForTest(
      kCopresenceServiceUuid,
      {
          .advertisement_found_cb =
              [&](api::ble::BlePeripheral::UniqueId peripheral_id,
                  api::ble::BleAdvertisementData advertisement_data) {
                ++found_count;
                found_data = std::move(advertisement_data);
              },
      });
  ASSERT_NE(session, nullptr);

  BluetoothLEAdvertisement advertisement;
  advertisement.ServiceUuids().Append(
      nearby_uuid_to_winrt_guid(kAlternateUuid));
  // Also append kAlternateUuid16 in 0x16 data on a second check to verify
  // deduplication when both ServiceUuids() and 0x16 section list it.
  DeliverAdvertisementFound(
      CreateReceivedEventArgs(kValidBluetoothAddress, advertisement));
  ASSERT_EQ(found_count, 1);
  ASSERT_TRUE(found_data.service_data.contains(kCopresenceServiceUuid));
  BleAdvertisementHeader header(
      found_data.service_data.at(kCopresenceServiceUuid));
  EXPECT_TRUE(header.IsValid());

  AppendUuid16ServiceDataSection(advertisement, kAlternateUuid16, "alt_data");
  DeliverAdvertisementFound(
      CreateReceivedEventArgs(kValidBluetoothAddress, advertisement));
  EXPECT_EQ(found_count, 2);

  EXPECT_TRUE(session->stop_scanning().ok());
}

TEST_F(BleMediumTest,
       AdvertisementFoundHandlerIgnoresAlternateUuidWithExtendedAdv) {
  EXPECT_CALL(bluetooth_adapter_, IsEnabled()).WillRepeatedly(Return(true));
  EXPECT_CALL(bluetooth_adapter_, IsExtendedAdvertisingSupported())
      .WillRepeatedly(Return(true));

  ble_medium_->AddAlternateUuidForService(kAlternateUuid16,
                                          std::string(kTestServiceId));

  int found_count = 0;
  auto session = StartBleV2ScanningForTest(
      kCopresenceServiceUuid,
      {
          .advertisement_found_cb =
              [&](api::ble::BlePeripheral::UniqueId peripheral_id,
                  api::ble::BleAdvertisementData advertisement_data) {
                ++found_count;
              },
      });
  ASSERT_NE(session, nullptr);

  BluetoothLEAdvertisement advertisement;
  advertisement.ServiceUuids().Append(
      nearby_uuid_to_winrt_guid(kAlternateUuid));
  AppendUuid16ServiceDataSection(advertisement, kAlternateUuid16, "alt_data");

  DeliverAdvertisementFound(
      CreateReceivedEventArgs(kValidBluetoothAddress, advertisement));
  EXPECT_EQ(found_count, 0);

  EXPECT_TRUE(session->stop_scanning().ok());
}

TEST_F(BleMediumTest,
       AdvertisementFoundHandlerPrefersPrimaryServiceDataOverAlternateUuid) {
  EXPECT_CALL(bluetooth_adapter_, IsEnabled()).WillRepeatedly(Return(true));
  EXPECT_CALL(bluetooth_adapter_, IsExtendedAdvertisingSupported())
      .WillRepeatedly(Return(false));

  ble_medium_->AddAlternateUuidForService(kAlternateUuid16,
                                          std::string(kTestServiceId));

  int found_count = 0;
  api::ble::BleAdvertisementData found_data;
  auto session = StartBleV2ScanningForTest(
      kCopresenceServiceUuid,
      {
          .advertisement_found_cb =
              [&](api::ble::BlePeripheral::UniqueId peripheral_id,
                  api::ble::BleAdvertisementData advertisement_data) {
                ++found_count;
                found_data = std::move(advertisement_data);
              },
      });
  ASSERT_NE(session, nullptr);

  BluetoothLEAdvertisement advertisement;
  // Note: primary 0x16 service data without 0xFEF3 in ServiceUuids().
  AppendUuid16ServiceDataSection(advertisement, 0xFEF3, "primary_payload");
  AppendUuid16ServiceDataSection(advertisement, kAlternateUuid16, "alt_data");

  DeliverAdvertisementFound(
      CreateReceivedEventArgs(kValidBluetoothAddress, advertisement));

  ASSERT_EQ(found_count, 1);
  EXPECT_FALSE(found_data.is_extended_advertisement);
  ASSERT_TRUE(found_data.service_data.contains(kCopresenceServiceUuid));
  EXPECT_EQ(found_data.service_data.at(kCopresenceServiceUuid),
            ByteArray("primary_payload"));

  EXPECT_TRUE(session->stop_scanning().ok());
}

TEST_F(BleMediumTest, AdvertisementFoundHandlerRejectsInvalidMacAddress) {
  EXPECT_CALL(bluetooth_adapter_, IsEnabled()).WillRepeatedly(Return(true));
  EXPECT_CALL(bluetooth_adapter_, IsExtendedAdvertisingSupported())
      .WillRepeatedly(Return(false));

  ble_medium_->AddAlternateUuidForService(kAlternateUuid16,
                                          std::string(kTestServiceId));

  int found_count = 0;
  auto session = StartBleV2ScanningForTest(
      kCopresenceServiceUuid,
      {
          .advertisement_found_cb =
              [&](api::ble::BlePeripheral::UniqueId peripheral_id,
                  api::ble::BleAdvertisementData advertisement_data) {
                ++found_count;
              },
      });
  ASSERT_NE(session, nullptr);

  BluetoothLEAdvertisement advertisement;
  AppendUuid16ServiceDataSection(advertisement, kAlternateUuid16, "alt_data");

  DeliverAdvertisementFound(
      CreateReceivedEventArgs(kInvalidBluetoothAddress, advertisement));
  EXPECT_EQ(found_count, 0);

  EXPECT_TRUE(session->stop_scanning().ok());
}

TEST_F(BleMediumTest,
       AdvertisementFoundHandlerNotifiesMultipleSessionsAndClearsOnStop) {
  EXPECT_CALL(bluetooth_adapter_, IsEnabled()).WillRepeatedly(Return(true));
  EXPECT_CALL(bluetooth_adapter_, IsExtendedAdvertisingSupported())
      .WillRepeatedly(Return(false));

  ble_medium_->AddAlternateUuidForService(kAlternateUuid16,
                                          std::string(kTestServiceId));
  ble_medium_->AddAlternateUuidForService(kSecondAlternateUuid16,
                                          std::string(kSecondTestServiceId));

  int copresence_session_1_count = 0;
  int copresence_session_2_count = 0;
  int other_uuid_session_count = 0;
  api::ble::BleAdvertisementData other_uuid_data;

  std::unique_ptr<api::ble::BleMedium::ScanningSession> session_1;
  session_1 = StartBleV2ScanningForTest(
      kCopresenceServiceUuid,
      {.advertisement_found_cb =
           [&](api::ble::BlePeripheral::UniqueId,
               api::ble::BleAdvertisementData data) {
             if (data.service_data.contains(kCopresenceServiceUuid)) {
               ++copresence_session_1_count;
             }
             // Verify re-entrant stop_scanning() inside callback does not
             // deadlock on mutex_.
             if (copresence_session_1_count == 2) {
               EXPECT_TRUE(session_1->stop_scanning().ok());
             }
           }});
  auto session_2 = StartBleV2ScanningForTest(
      kCopresenceServiceUuid,
      {.advertisement_found_cb =
           [&](api::ble::BlePeripheral::UniqueId,
               api::ble::BleAdvertisementData data) {
             if (data.service_data.contains(kCopresenceServiceUuid)) {
               ++copresence_session_2_count;
             }
           }});
  auto session_3 = StartBleV2ScanningForTest(
      kTestServiceUuid,
      {.advertisement_found_cb =
           [&](api::ble::BlePeripheral::UniqueId,
               api::ble::BleAdvertisementData data) {
             ++other_uuid_session_count;
             other_uuid_data = std::move(data);
           }});
  ASSERT_NE(session_1, nullptr);
  ASSERT_NE(session_2, nullptr);
  ASSERT_NE(session_3, nullptr);

  // Alternate UUID packet with two distinct alternate UUIDs reaches all active
  // scanning sessions with both service IDs in the bloom filter.
  BluetoothLEAdvertisement alt_advertisement;
  AppendUuid16ServiceDataSection(alt_advertisement, kAlternateUuid16,
                                 "alt_data_1");
  AppendUuid16ServiceDataSection(alt_advertisement, kSecondAlternateUuid16,
                                 "alt_data_2");
  DeliverAdvertisementFound(
      CreateReceivedEventArgs(kValidBluetoothAddress, alt_advertisement));

  EXPECT_EQ(copresence_session_1_count, 1);
  EXPECT_EQ(copresence_session_2_count, 1);
  EXPECT_EQ(other_uuid_session_count, 1);
  ASSERT_TRUE(other_uuid_data.service_data.contains(kTestServiceUuid));
  BleAdvertisementHeader multi_alt_header(
      other_uuid_data.service_data.at(kTestServiceUuid));
  ASSERT_TRUE(multi_alt_header.IsValid());
  constexpr size_t kBloomFilterBytes =
      BleAdvertisementHeader::kServiceIdBloomFilterByteLength;
  BloomFilter bloom_filter(std::make_unique<BitSetImpl<kBloomFilterBytes>>(),
                           multi_alt_header.GetServiceIdBloomFilter());
  EXPECT_TRUE(bloom_filter.PossiblyContains(std::string(kTestServiceId)));
  EXPECT_TRUE(bloom_filter.PossiblyContains(std::string(kSecondTestServiceId)));

  // Primary Copresence packet only notifies Copresence sessions (and session_1
  // stops itself re-entrantly inside its callback), not kTestServiceUuid.
  BluetoothLEAdvertisement primary_advertisement;
  AppendUuid16ServiceDataSection(primary_advertisement, 0xFEF3, "primary");
  DeliverAdvertisementFound(
      CreateReceivedEventArgs(kValidBluetoothAddress, primary_advertisement));

  EXPECT_EQ(copresence_session_1_count, 2);
  EXPECT_EQ(copresence_session_2_count, 2);
  EXPECT_EQ(other_uuid_session_count, 1);

  // After partial stop (session_1 stopped, session_2 and session_3 still
  // active), alternate_uuids_for_service_ is preserved.
  DeliverAdvertisementFound(
      CreateReceivedEventArgs(kValidBluetoothAddress, alt_advertisement));
  EXPECT_EQ(copresence_session_1_count, 2);
  EXPECT_EQ(copresence_session_2_count, 3);
  EXPECT_EQ(other_uuid_session_count, 2);

  EXPECT_TRUE(session_2->stop_scanning().ok());
  EXPECT_TRUE(session_3->stop_scanning().ok());
  EXPECT_EQ(GetWatchersCount(), 0);

  // After all sessions stop, alternate_uuids_for_service_ is cleared.
  int post_stop_count = 0;
  auto new_session = StartBleV2ScanningForTest(
      kCopresenceServiceUuid,
      {.advertisement_found_cb =
           [&](api::ble::BlePeripheral::UniqueId,
               api::ble::BleAdvertisementData) { ++post_stop_count; }});
  ASSERT_NE(new_session, nullptr);
  DeliverAdvertisementFound(
      CreateReceivedEventArgs(kValidBluetoothAddress, alt_advertisement));
  EXPECT_EQ(post_stop_count, 0);
  EXPECT_TRUE(new_session->stop_scanning().ok());
}

TEST(BleMedium, DISABLED_StartAdvertising) {
  BluetoothAdapter bluetoothAdapter;
  BleMedium ble_medium(bluetoothAdapter);

  api::ble::BleAdvertisementData advertising_data;
  api::ble::BleAdvertisementData scan_response_data;

  EXPECT_TRUE(ble_medium.StartAdvertising(
      advertising_data, {.tx_power_level = api::ble::TxPowerLevel::kHigh,
                         .is_connectable = true}));
}

TEST(BleMedium, DISABLED_StopAdvertising) {
  BluetoothAdapter bluetoothAdapter;
  BleMedium ble_medium(bluetoothAdapter);

  api::ble::BleAdvertisementData advertising_data;

  EXPECT_TRUE(ble_medium.StartAdvertising(
      advertising_data, {.tx_power_level = api::ble::TxPowerLevel::kHigh,
                         .is_connectable = true}));
  EXPECT_TRUE(ble_medium.StopAdvertising());
}

TEST(BleMedium, DISABLED_StartScanning) {
  bool scan_response_received = false;
  absl::Notification scan_response_notification;

  BluetoothAdapter bluetoothAdapter;
  BleMedium ble_medium(bluetoothAdapter);
  Uuid service_uuid;

  api::ble::BleMedium::ScanCallback callback;
  callback.advertisement_found_cb =
      [&scan_response_received, &scan_response_notification](
          api::ble::BlePeripheral::UniqueId peripheral_id,
          const api::ble::BleAdvertisementData& advertisement_data) {
        scan_response_received = true;
        scan_response_notification.Notify();
      };

  EXPECT_TRUE(ble_medium.StartScanning(
      service_uuid, api::ble::TxPowerLevel::kHigh, std::move(callback)));

  EXPECT_TRUE(scan_response_notification.WaitForNotificationWithTimeout(
      absl::Seconds(5)));
  EXPECT_TRUE(scan_response_received);
}

TEST(BleMedium, DISABLED_StopScanning) {
  BluetoothAdapter bluetoothAdapter;
  BleMedium ble_medium(bluetoothAdapter);
  Uuid service_uuid;

  api::ble::BleMedium::ScanCallback callback;
  callback.advertisement_found_cb =
      [](api::ble::BlePeripheral::UniqueId peripheral_id,
         const api::ble::BleAdvertisementData& advertisement_data) {};

  EXPECT_TRUE(ble_medium.StartScanning(
      service_uuid, api::ble::TxPowerLevel::kHigh, std::move(callback)));

  EXPECT_TRUE(ble_medium.StopScanning());
}

TEST(BleMedium, DISABLED_StartThenStopScanning) {
  bool scan_response_received = false;
  absl::Notification scan_response_notification;

  BluetoothAdapter bluetoothAdapter;
  BleMedium ble_medium(bluetoothAdapter);
  Uuid service_uuid;

  api::ble::BleMedium::ScanningCallback callback;
  callback.advertisement_found_cb =
      [&scan_response_received, &scan_response_notification](
          api::ble::BlePeripheral::UniqueId peripheral_id,
          const api::ble::BleAdvertisementData& advertisement_data) {
        scan_response_received = true;
        scan_response_notification.Notify();
      };

  auto scanning_session = ble_medium.StartScanning(
      service_uuid, api::ble::TxPowerLevel::kHigh, std::move(callback));
  EXPECT_NE(scanning_session, nullptr);
  EXPECT_TRUE(scanning_session->stop_scanning().ok());
}

}  // namespace
}  // namespace windows
}  // namespace nearby
