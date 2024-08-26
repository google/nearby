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

#include "connections/implementation/mediums/ble_v2/discovered_peripheral_tracker.h"

#include <atomic>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/mediums/ble_v2/advertisement_read_result.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement_header.h"
#include "connections/implementation/mediums/ble_v2/ble_utils.h"
#include "connections/implementation/mediums/ble_v2/bloom_filter.h"
#include "connections/implementation/mediums/ble_v2/discovered_peripheral_callback.h"
#include "connections/implementation/mediums/ble_v2/instant_on_lost_advertisement.h"
#include "connections/implementation/mediums/utils.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace connections {
namespace mediums {

namespace {

constexpr absl::Duration kWaitDuration = absl::Milliseconds(1000);
constexpr absl::string_view kFastAdvertisementServiceUuid = "FE2C";
constexpr absl::string_view kServiceIdA = "A";
constexpr absl::string_view kServiceIdB = "B";
constexpr absl::string_view kData = "\x04\x02\x00";
constexpr absl::string_view kData2 = "\x07\x00\x07";
constexpr absl::string_view kDeviceToken = "\x04\x20";

ByteArray CreateFastBleAdvertisement(const ByteArray& data,
                                     const ByteArray& device_token) {
  return ByteArray(BleAdvertisement(
      BleAdvertisement::Version::kV2, BleAdvertisement::SocketVersion::kV2,
      /*service_id_hash=*/ByteArray{}, data, device_token,
      BleAdvertisementHeader::kDefaultPsmValue));
}

ByteArray CreateBleAdvertisement(const std::string& service_id,
                                 const ByteArray& data,
                                 const ByteArray& device_token) {
  return ByteArray(BleAdvertisement(
      BleAdvertisement::Version::kV2, BleAdvertisement::SocketVersion::kV2,
      bleutils::GenerateServiceIdHash(service_id), data, device_token,
      BleAdvertisementHeader::kDefaultPsmValue));
}

// Legacy advertisement is not supported any more but we fake the legacy one
// by setting version and socket version as kV1 and using legacy hash function.
ByteArray CreateLegacyBleAdvertisement(const std::string& service_id,
                                       const ByteArray& data,
                                       const ByteArray& device_token) {
  return ByteArray(BleAdvertisement(
      BleAdvertisement::Version::kV1, BleAdvertisement::SocketVersion::kV1,
      bleutils::GenerateServiceIdHash(service_id,
                                      BleAdvertisement::Version::kV1),
      data, device_token, BleAdvertisementHeader::kDefaultPsmValue));
}

BleAdvertisementHeader CreateFastBleAdvertisementHeader(
    const ByteArray& advertisement_bytes) {
  BloomFilter bloom_filter(
      std::make_unique<BitSetImpl<
          BleAdvertisementHeader::kServiceIdBloomFilterByteLength>>());

  return BleAdvertisementHeader(
      BleAdvertisementHeader::Version::kV2, /*extended_advertisement=*/false,
      /*num_slots=*/1, ByteArray(bloom_filter),
      bleutils::GenerateAdvertisementHash(advertisement_bytes),
      /*psm=*/BleAdvertisementHeader::kDefaultPsmValue);
}

ByteArray CreateBleAdvertisementHeader(const ByteArray& advertisement_hash,
                                       int psm,
                                       std::vector<std::string>& service_ids) {
  BloomFilter service_id_bloom_filter(
      std::make_unique<BitSetImpl<
          BleAdvertisementHeader::kServiceIdBloomFilterByteLength>>());

  for (const std::string& service_id : service_ids) {
    service_id_bloom_filter.Add(service_id);
  }

  return ByteArray(BleAdvertisementHeader(BleAdvertisementHeader::Version::kV2,
                                          /*extended_advertisement=*/false,
                                          /*num_slots=*/service_ids.size(),
                                          ByteArray(service_id_bloom_filter),
                                          advertisement_hash, psm));
}

ByteArray CreateBleAdvertisementHeader(const ByteArray& advertisement_hash,
                                       std::vector<std::string>& service_ids) {
  return CreateBleAdvertisementHeader(advertisement_hash,
                                      BleAdvertisementHeader::kDefaultPsmValue,
                                      service_ids);
}

ByteArray GenerateRandomAdvertisementHash() {
  ByteArray random_advertisement_hash = Utils::GenerateRandomBytes(
      BleAdvertisementHeader::kAdvertisementHashByteLength);

  return random_advertisement_hash;
}

class MockDiscoveredPeripheralCallback : public DiscoveredPeripheralCallback {
 public:
  MOCK_METHOD(void, OnPeripheralDiscovered,
              (BleV2Peripheral, const std::string&, const ByteArray&, bool),
              ());
  MOCK_METHOD(void, OnPeripheralLost,
              (BleV2Peripheral, const std::string&, const ByteArray&, bool),
              ());
  MOCK_METHOD(void, OnInstantLost,
              (BleV2Peripheral, const std::string&, const ByteArray&, bool),
              ());
  MOCK_METHOD(void, OnLegacyDeviceDiscovered, (), ());
};

class DiscoveredPeripheralTrackerTest : public testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_connections_feature::
            kDisableBluetoothClassicScanning,
        false);
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_connections_feature::kEnableInstantOnLost,
        false);
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_connections_feature::
            kEnableGattQueryInThread,
        GetParam());
    MediumEnvironment::Instance().Start();
    adapter_peripheral_ = std::make_unique<BluetoothAdapter>();
    adapter_central_ = std::make_unique<BluetoothAdapter>();
    ble_peripheral_ = std::make_unique<BleV2Medium>(*adapter_peripheral_);
    ble_central_ = std::make_unique<BleV2Medium>(*adapter_central_);
  }

  void TearDown() override {
    MediumEnvironment::Instance().Stop();
    NearbyFlags::GetInstance().ResetOverridedValues();
  }

  BleV2Peripheral CreateBlePeripheral() {
    return ble_central_->GetRemotePeripheral(
        adapter_peripheral_->GetMacAddress());
  }

  // Simulates to see a fast advertisement.
  void FindFastAdvertisement(
      const api::ble_v2::BleAdvertisementData& advertisement_data,
      const std::vector<ByteArray>& advertisement_bytes_list,
      CountDownLatch& fetch_latch) {
    BleV2Peripheral peripheral = CreateBlePeripheral();

    discovered_peripheral_tracker_.ProcessFoundBleAdvertisement(
        peripheral, advertisement_data,
        GetAdvertisementFetcher(fetch_latch, advertisement_bytes_list));
  }

  // Simulates to see a regular advertisement.
  void FindAdvertisement(
      const api::ble_v2::BleAdvertisementData& advertisement_data,
      const std::vector<ByteArray>& advertisement_bytes_list,
      CountDownLatch& fetch_latch) {
    BleV2Peripheral peripheral = CreateBlePeripheral();

    discovered_peripheral_tracker_.ProcessFoundBleAdvertisement(
        peripheral, advertisement_data,
        GetAdvertisementFetcher(fetch_latch, advertisement_bytes_list));
  }

  void FindAdvertisementWithSlowFetcher(
      const api::ble_v2::BleAdvertisementData& advertisement_data,
      const std::vector<ByteArray>& advertisement_bytes_list,
      CountDownLatch& fetch_latch) {
    BleV2Peripheral peripheral = CreateBlePeripheral();

    discovered_peripheral_tracker_.ProcessFoundBleAdvertisement(
        peripheral, advertisement_data,
        GetSlowAdvertisementFetcher(fetch_latch, advertisement_bytes_list));
  }

  int GetFetchAdvertisementCallbackCount() const {
    MutexLock lock(&mutex_);
    return fetch_count_;
  }

  void DisableBluetoothScanning() {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_connections_feature::
            kDisableBluetoothClassicScanning,
        true);
  }

  void EnableInstantOnLost() {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_connections_feature::kEnableInstantOnLost,
        true);
  }

  void EnableFetchGattAdvertisementInThread() {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_connections_feature::
            kEnableGattQueryInThread,
        true);
  }

 protected:
  // A stub Advertisement fetcher.
  DiscoveredPeripheralTracker::AdvertisementFetcher GetAdvertisementFetcher(
      CountDownLatch& fetch_latch,
      const std::vector<ByteArray>& advertisement_bytes_list) {
    return [this, &fetch_latch, advertisement_bytes_list](
               BleV2Peripheral peripheral, int num_slots, int psm,
               const std::vector<std::string>& interesting_service_ids,
               mediums::AdvertisementReadResult& advertisement_read_result) {
      MutexLock lock(&mutex_);
      fetch_count_++;
      int slot = 0;
      for (const auto& advertisement_bytes : advertisement_bytes_list) {
        advertisement_read_result.AddAdvertisement(slot++, advertisement_bytes);
      }
      advertisement_read_result.RecordLastReadStatus(
          /*is_success=*/true);
      fetch_latch.CountDown();
    };
  }

  DiscoveredPeripheralTracker::AdvertisementFetcher GetSlowAdvertisementFetcher(
      CountDownLatch& fetch_latch,
      const std::vector<ByteArray>& advertisement_bytes_list) {
    return [this, &fetch_latch, advertisement_bytes_list](
               BleV2Peripheral peripheral, int num_slots, int psm,
               const std::vector<std::string>& interesting_service_ids,
               mediums::AdvertisementReadResult& advertisement_read_result) {
      MutexLock lock(&mutex_);
      fetch_count_++;
      int slot = 0;
      // In real environment, the GATT fetch may run about 3-5 seconds.
      absl::SleepFor(absl::Milliseconds(200));
      for (const auto& advertisement_bytes : advertisement_bytes_list) {
        advertisement_read_result.AddAdvertisement(slot++, advertisement_bytes);
      }
      advertisement_read_result.RecordLastReadStatus(
          /*is_success=*/true);
      fetch_latch.CountDown();
    };
  }

  std::unique_ptr<BluetoothAdapter> adapter_peripheral_;
  std::unique_ptr<BluetoothAdapter> adapter_central_;
  std::unique_ptr<BleV2Medium> ble_peripheral_;
  std::unique_ptr<BleV2Medium> ble_central_;
  mutable Mutex mutex_;
  int fetch_count_ ABSL_GUARDED_BY(mutex_) = 0;
  DiscoveredPeripheralTracker discovered_peripheral_tracker_;
};

TEST_P(DiscoveredPeripheralTrackerTest,
       FoundFastAdvertisementPeripheralDiscovered) {
  ByteArray fast_advertisement_bytes = CreateFastBleAdvertisement(
      ByteArray(std::string(kData)), ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(1);
  CountDownLatch fetch_latch(1);

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
      },
      Uuid(kFastAdvertisementServiceUuid));

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!fast_advertisement_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {Uuid(kFastAdvertisementServiceUuid), fast_advertisement_bytes});
  }

  FindFastAdvertisement(advertisement_data, {}, fetch_latch);

  // We should receive a client callback of a peripheral discovery without a
  // GATT read.
  fetch_latch.Await(kWaitDuration);
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 0);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       ReportFoundLegacyDeviceWhenFoundBleAdvertisementPeripheralDiscovered) {
  DisableBluetoothScanning();
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(1);
  CountDownLatch legacy_found_latch(1);
  CountDownLatch fetch_latch(1);

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {.peripheral_discovered_cb =
           [&found_latch](
               BleV2Peripheral peripheral, const std::string& service_id,
               const ByteArray& advertisement_bytes, bool fast_advertisement) {
             EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
             EXPECT_FALSE(fast_advertisement);
             found_latch.CountDown();
           },
       .legacy_device_discovered_cb =
           [&legacy_found_latch]() { legacy_found_latch.CountDown(); }},
      {});

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);

  // We should receive a client callback of a peripheral discovery.
  fetch_latch.Await(kWaitDuration);
  EXPECT_TRUE(legacy_found_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       CanStartMultipleTrackingWithSameServiceId) {
  ByteArray fast_advertisement_bytes = CreateFastBleAdvertisement(
      ByteArray(std::string(kData)), ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(3);
  CountDownLatch fetch_latch(3);
  std::atomic<int> callback_times = 0;

  // 1st tracking.
  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&callback_times, &found_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                callback_times++;
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
      },
      Uuid(kFastAdvertisementServiceUuid));

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!fast_advertisement_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {Uuid(kFastAdvertisementServiceUuid), fast_advertisement_bytes});
  }

  FindFastAdvertisement(advertisement_data, {}, fetch_latch);

  // 2nd tracking.
  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&callback_times, &found_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                callback_times++;
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
      },
      Uuid(kFastAdvertisementServiceUuid));

  FindFastAdvertisement(advertisement_data, {}, fetch_latch);

  // 3rd tracking.
  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&callback_times, &found_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                callback_times++;
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
      },
      Uuid(kFastAdvertisementServiceUuid));

  FindFastAdvertisement(advertisement_data, {}, fetch_latch);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       FoundFastAdvertisementDuplicateAdvertisements) {
  ByteArray fast_advertisement_bytes = CreateFastBleAdvertisement(
      ByteArray(std::string(kData)), ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(3);
  CountDownLatch fetch_latch(3);
  std::atomic<int> callback_times = 0;

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&callback_times, &found_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                callback_times++;
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
      },
      Uuid(kFastAdvertisementServiceUuid));

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!fast_advertisement_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {Uuid(kFastAdvertisementServiceUuid), fast_advertisement_bytes});
  }

  FindFastAdvertisement(advertisement_data, {}, fetch_latch);
  FindFastAdvertisement(advertisement_data, {}, fetch_latch);
  FindFastAdvertisement(advertisement_data, {}, fetch_latch);

  // We should only receive ONE client callback of a peripheral discovery with
  // ZERO GATT reads.
  fetch_latch.Await(kWaitDuration * 3);
  EXPECT_EQ(callback_times, 1);
  // The count down won't be finished since the callback only been called once.
  // Let it time out.
  EXPECT_FALSE(found_latch.Await(3 * kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 0);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       FoundFastAdvertisementUntrackedFastAdvertisementServiceUuid) {
  ByteArray fast_advertisement_bytes = CreateFastBleAdvertisement(
      ByteArray(std::string(kData)), ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(1);
  CountDownLatch fetch_latch(1);

  // Start tracking a service ID and then process a discovery containing a valid
  // fast advertisement, but under a different service UUID.
  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
      },
      Uuid("FE3C"));

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!fast_advertisement_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {Uuid(kFastAdvertisementServiceUuid), fast_advertisement_bytes});
  }

  FindFastAdvertisement(advertisement_data, {}, fetch_latch);

  // We should get no new discoveries.
  fetch_latch.Await(kWaitDuration);
  EXPECT_FALSE(found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 0);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       FoundFastAdvertisementAndGattAdvertisementSimultaneously) {
  std::vector<std::string> service_ids = {std::string(kServiceIdB)};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray fast_advertisement_bytes = CreateFastBleAdvertisement(
      ByteArray(std::string(kData)), ByteArray(std::string(kDeviceToken)));
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdB), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch_a(1);
  CountDownLatch found_latch_b(1);
  CountDownLatch fetch_latch(1);

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&found_latch_a](BleV2Peripheral peripheral,
                               const std::string& service_id,
                               const ByteArray& advertisement_bytes,
                               bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                found_latch_a.CountDown();
              },
      },
      Uuid(kFastAdvertisementServiceUuid));
  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdB),
      {
          .peripheral_discovered_cb =
              [&found_latch_b](BleV2Peripheral peripheral,
                               const std::string& service_id,
                               const ByteArray& advertisement_bytes,
                               bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                found_latch_b.CountDown();
              },
      },
      {});

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }
  if (!fast_advertisement_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {Uuid(kFastAdvertisementServiceUuid), fast_advertisement_bytes});
  }

  FindFastAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);

  // We should receive two separate peripheral discoveries.
  fetch_latch.Await(kWaitDuration);
  EXPECT_TRUE(found_latch_a.Await(kWaitDuration).result());
  EXPECT_TRUE(found_latch_b.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       FoundBleAdvertisementPeripheralDiscovered) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(1);
  CountDownLatch fetch_latch(1);

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
      },
      {});

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);

  // We should receive a client callback of a peripheral discovery.
  fetch_latch.Await(kWaitDuration);
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       FoundBleAdvertisementLegacyPeripheralDiscovered) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray legacy_advertisement_bytes = CreateLegacyBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(1);
  CountDownLatch fetch_latch(1);

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&found_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { found_latch.CountDown(); },
      },
      {});

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data, {legacy_advertisement_bytes},
                    fetch_latch);

  // We should not receive a client callback with the Version/SocketVersion kV1.
  fetch_latch.Await(kWaitDuration);
  EXPECT_FALSE(found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       FoundBleAdvertisementFavorLatestPeripheral) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData2)),
      ByteArray(std::string(kDeviceToken)));
  ByteArray legacy_advertisement_bytes = CreateLegacyBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(1);
  CountDownLatch fetch_latch(1);
  std::atomic<int> callback_times = 0;

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&callback_times, &found_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                callback_times++;
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData2)));
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
      },
      {});

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data,
                    {advertisement_bytes, legacy_advertisement_bytes},
                    fetch_latch);

  // We should only receive one callback with data from the V2 GATT
  // advertisement.
  fetch_latch.Await(kWaitDuration);
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(callback_times, 1);
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       FoundBleAdvertisementDuplicateAdvertisements) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(3);
  CountDownLatch fetch_latch(3);
  std::atomic<int> callback_times = 0;

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&callback_times, &found_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                callback_times++;
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
      },
      {});

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);
  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);
  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);

  // We should only receive ONE client callback of a peripheral discovery. And
  // we should also do only ONE GATT read.
  fetch_latch.Await(kWaitDuration);
  EXPECT_EQ(callback_times, 1);
  // The count down won't be finished since the callback only been called once.
  // Let it time out.
  EXPECT_FALSE(found_latch.Await(3 * kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       FoundBleAdvertisementUntrackedServiceId) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA),
                                          std::string(kServiceIdB)};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdB), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(1);
  CountDownLatch fetch_latch(1);

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&found_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { found_latch.CountDown(); },
      },
      {});

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);

  // We should get no new discoveries.
  fetch_latch.Await(kWaitDuration);
  EXPECT_FALSE(found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       LostPeripheralForFastAdvertisementLost) {
  std::vector<std::string> service_ids = {};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray fast_advertisement_bytes = CreateFastBleAdvertisement(
      ByteArray(std::string(kData)), ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(2);
  CountDownLatch fetch_latch(1);
  std::atomic<int> lost_callback_times = 0;

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch, &lost_callback_times](
                  BleV2Peripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                lost_callback_times++;
                lost_latch.CountDown();
              },
      },
      Uuid(kFastAdvertisementServiceUuid));

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }
  if (!fast_advertisement_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {Uuid(kFastAdvertisementServiceUuid), fast_advertisement_bytes});
  }

  FindFastAdvertisement(advertisement_data, {fast_advertisement_bytes},
                        fetch_latch);

  // We should receive a client callback of a peripheral discovery.
  fetch_latch.Await(kWaitDuration);
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 0);

  // Then, go through two cycles of onLost. The first cycle should include the
  // recently discovered peripheral in its 'found' pool. The second one should
  // trigger the onLost callback.
  discovered_peripheral_tracker_.ProcessLostGattAdvertisements();
  discovered_peripheral_tracker_.ProcessLostGattAdvertisements();

  // We should receive a client callback of a lost peripheral.
  EXPECT_FALSE(lost_latch.Await(kWaitDuration).result());
  EXPECT_EQ(lost_callback_times, 1);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       FoundFastAdvertisementAlmostLostPeripheral) {
  std::vector<std::string> service_ids = {};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray fast_advertisement_bytes = CreateFastBleAdvertisement(
      ByteArray(std::string(kData)), ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);
  CountDownLatch fetch_latch(1);

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { lost_latch.CountDown(); },
      },
      Uuid(kFastAdvertisementServiceUuid));

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }
  if (!fast_advertisement_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {Uuid(kFastAdvertisementServiceUuid), fast_advertisement_bytes});
  }

  // Start tracking a service ID and then process a loop of discoveries and
  // onLost alarms.
  for (int i = 0; i < 20; i++) {
    FindFastAdvertisement(advertisement_data, {fast_advertisement_bytes},
                          fetch_latch);
    discovered_peripheral_tracker_.ProcessLostGattAdvertisements();
  }

  // We should only receive ONE client callback of a peripheral discovery, ZERO
  // GATT reads, and no onLost calls.
  fetch_latch.Await(kWaitDuration);
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 0);
  EXPECT_FALSE(lost_latch.Await(kWaitDuration).result());
}

TEST_P(DiscoveredPeripheralTrackerTest, LostPeripheralForAdvertisementLost) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);
  CountDownLatch fetch_latch(1);

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { lost_latch.CountDown(); },
      },
      {});

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);

  // We should receive a client callback of a peripheral discovery.
  fetch_latch.Await(kWaitDuration);
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);

  // Then, go through two cycles of onLost. The first cycle should include the
  // recently discovered peripheral in its 'found' pool. The second one should
  // trigger the onLost callback.
  discovered_peripheral_tracker_.ProcessLostGattAdvertisements();
  discovered_peripheral_tracker_.ProcessLostGattAdvertisements();

  // We should receive a client callback of a lost peripheral
  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());
}

TEST_P(DiscoveredPeripheralTrackerTest,
       LostPeripheralForFastAndGattAdvertisementLost) {
  std::vector<std::string> service_ids = {std::string(kServiceIdB)};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray fast_advertisement_bytes = CreateFastBleAdvertisement(
      ByteArray(std::string(kData)), ByteArray(std::string(kDeviceToken)));
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdB), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch_a(1);
  CountDownLatch found_latch_b(1);
  CountDownLatch lost_latch_a(1);
  CountDownLatch lost_latch_b(1);
  CountDownLatch fetch_latch(1);

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&found_latch_a](BleV2Peripheral peripheral,
                               const std::string& service_id,
                               const ByteArray& advertisement_bytes,
                               bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                found_latch_a.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch_a](
                  BleV2Peripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { lost_latch_a.CountDown(); },
      },
      Uuid(kFastAdvertisementServiceUuid));
  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdB),
      {
          .peripheral_discovered_cb =
              [&found_latch_b](BleV2Peripheral peripheral,
                               const std::string& service_id,
                               const ByteArray& advertisement_bytes,
                               bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                found_latch_b.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch_b](
                  BleV2Peripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { lost_latch_b.CountDown(); },
      },
      {});

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }
  if (!fast_advertisement_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {Uuid(kFastAdvertisementServiceUuid), fast_advertisement_bytes});
  }

  FindFastAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);

  // We should receive two separate peripheral discoveries.
  fetch_latch.Await(kWaitDuration);
  EXPECT_TRUE(found_latch_a.Await(kWaitDuration).result());
  EXPECT_TRUE(found_latch_b.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);

  // Then, go through two cycles of onLost. The first cycle should include the
  // recently discovered peripheral in its 'found' pool. The second one should
  // trigger the onLost callback.
  discovered_peripheral_tracker_.ProcessLostGattAdvertisements();
  discovered_peripheral_tracker_.ProcessLostGattAdvertisements();

  // We should receive two client callbacks of a lost peripheral from each
  // service ID.
  EXPECT_TRUE(lost_latch_a.Await(kWaitDuration).result());
  EXPECT_TRUE(lost_latch_b.Await(kWaitDuration).result());
}

TEST_P(DiscoveredPeripheralTrackerTest,
       LostPeripheralNotCallbackForUntrackedServiceId) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);
  CountDownLatch fetch_latch(1);

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { lost_latch.CountDown(); },
      },
      {});

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);

  // We should receive a client callback of a peripheral discovery.
  fetch_latch.Await(kWaitDuration);
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);

  // Then, stop tracking the service ID and go through two cycles of onLost.
  discovered_peripheral_tracker_.StopTracking(std::string(kServiceIdA));
  discovered_peripheral_tracker_.ProcessLostGattAdvertisements();
  discovered_peripheral_tracker_.ProcessLostGattAdvertisements();

  // We should NOT receive a client callback of a lost peripheral
  EXPECT_FALSE(lost_latch.Await(kWaitDuration).result());
}

TEST_P(DiscoveredPeripheralTrackerTest, LostPeripheralForInstantOnLost) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_hash = GenerateRandomAdvertisementHash();
  ByteArray advertisement_header_bytes =
      CreateBleAdvertisementHeader(advertisement_hash, service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);
  CountDownLatch fetch_latch(1);

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { lost_latch.CountDown(); },
      },
      {});

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);

  // We should receive a client callback of a peripheral discovery.
  fetch_latch.Await(kWaitDuration);
  ASSERT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);

  auto advertisement = InstantOnLostAdvertisement::CreateFromHashes(
      std::list<std::string>({std::string(advertisement_hash)}));
  ASSERT_OK(advertisement);
  api::ble_v2::BleAdvertisementData loss_advertisement_data{};
  loss_advertisement_data.service_data.insert(
      {bleutils::kCopresenceServiceUuid, ByteArray(advertisement->ToBytes())});

  FindAdvertisement(loss_advertisement_data,
                    {ByteArray(advertisement->ToBytes())}, fetch_latch);

  // Then, go through a cycle of onLost. Since we triggered a forced loss via
  // the instant on los advertisement, the lost call should trigger the onLost
  // client callback.
  discovered_peripheral_tracker_.ProcessLostGattAdvertisements();

  // We should receive a client callback of a lost peripheral
  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());
}

TEST_P(DiscoveredPeripheralTrackerTest, InstantLostPeripheralForInstantOnLost) {
  EnableInstantOnLost();
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_hash = GenerateRandomAdvertisementHash();
  ByteArray advertisement_header_bytes =
      CreateBleAdvertisementHeader(advertisement_hash, service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);
  CountDownLatch fetch_latch(1);

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
          .instant_lost_cb =
              [&lost_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { lost_latch.CountDown(); },
      },
      {});

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);

  // We should receive a client callback of a peripheral discovery.
  fetch_latch.Await(kWaitDuration);
  ASSERT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);

  auto advertisement = InstantOnLostAdvertisement::CreateFromHashes(
      std::list<std::string>({std::string(advertisement_hash)}));
  ASSERT_OK(advertisement);
  api::ble_v2::BleAdvertisementData loss_advertisement_data{};
  loss_advertisement_data.service_data.insert(
      {bleutils::kCopresenceServiceUuid, ByteArray(advertisement->ToBytes())});

  FindAdvertisement(loss_advertisement_data,
                    {ByteArray(advertisement->ToBytes())}, fetch_latch);

  // Then, go through a cycle of onLost. Since we triggered a forced loss via
  // the instant on los advertisement, the lost call should trigger the onLost
  // client callback.
  discovered_peripheral_tracker_.ProcessLostGattAdvertisements();

  // We should receive a client callback of a lost peripheral
  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());
}

TEST_P(DiscoveredPeripheralTrackerTest,
       IgnoreFoundAdvertisementForInstantOnLost) {
  EnableInstantOnLost();
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_hash = GenerateRandomAdvertisementHash();
  ByteArray advertisement_header_bytes =
      CreateBleAdvertisementHeader(advertisement_hash, service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));

  CountDownLatch fetch_latch(1);
  MockDiscoveredPeripheralCallback mock_callback;

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&mock_callback](BleV2Peripheral peripheral,
                               const std::string& service_id,
                               const ByteArray& advertisement_bytes,
                               bool fast_advertisement) {
                mock_callback.OnPeripheralDiscovered(peripheral, service_id,
                                                     advertisement_bytes,
                                                     fast_advertisement);
              },
          .instant_lost_cb =
              [&mock_callback](BleV2Peripheral peripheral,
                               const std::string& service_id,
                               const ByteArray& advertisement_bytes,
                               bool fast_advertisement) {
                mock_callback.OnInstantLost(peripheral, service_id,
                                            advertisement_bytes,
                                            fast_advertisement);
              },
      },
      {});

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  EXPECT_CALL(mock_callback, OnPeripheralDiscovered).Times(1);
  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);
  fetch_latch.Await(kWaitDuration);

  // We should receive a client callback of a peripheral discovery.
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);

  CountDownLatch fetch_latch2(1);
  auto advertisement = InstantOnLostAdvertisement::CreateFromHashes(
      std::list<std::string>({std::string(advertisement_hash)}));
  ASSERT_OK(advertisement);
  api::ble_v2::BleAdvertisementData loss_advertisement_data{};
  loss_advertisement_data.service_data.insert(
      {bleutils::kCopresenceServiceUuid, ByteArray(advertisement->ToBytes())});

  EXPECT_CALL(mock_callback, OnInstantLost).Times(1);
  FindAdvertisement(loss_advertisement_data,
                    {ByteArray(advertisement->ToBytes())}, fetch_latch2);
  fetch_latch2.Await(kWaitDuration);

  // Then, go through a cycle of onLost. Since we triggered a forced loss via
  // the instant on los advertisement, the lost call should trigger the onLost
  // client callback.
  discovered_peripheral_tracker_.ProcessLostGattAdvertisements();

  // Lost advertisement should not be reported.
  CountDownLatch fetch_latch3(1);
  EXPECT_CALL(mock_callback, OnPeripheralDiscovered).Times(0);
  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch3);
  fetch_latch3.Await(kWaitDuration);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       LostPeripheralWithFastAdvertisementForInstantOnLost) {
  ByteArray fast_advertisement_bytes = CreateFastBleAdvertisement(
      ByteArray(std::string(kData)), ByteArray(std::string(kDeviceToken)));

  // Used to get advertisement hash from fast advertisement bytes.
  BleAdvertisementHeader ble_advertisement_header =
      CreateFastBleAdvertisementHeader(fast_advertisement_bytes);

  ByteArray advertisement_hash =
      ble_advertisement_header.GetAdvertisementHash();

  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);
  CountDownLatch fetch_latch(1);

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { lost_latch.CountDown(); },
      },
      Uuid(kFastAdvertisementServiceUuid));

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!fast_advertisement_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {Uuid(kFastAdvertisementServiceUuid), fast_advertisement_bytes});
  }

  FindFastAdvertisement(advertisement_data, {}, fetch_latch);

  // We should receive a client callback of a peripheral discovery.
  fetch_latch.Await(kWaitDuration);
  ASSERT_TRUE(found_latch.Await(kWaitDuration).result());

  auto advertisement = InstantOnLostAdvertisement::CreateFromHashes(
      std::list<std::string>({std::string(advertisement_hash)}));
  ASSERT_OK(advertisement);
  api::ble_v2::BleAdvertisementData loss_advertisement_data{};
  loss_advertisement_data.service_data.insert(
      {bleutils::kCopresenceServiceUuid, ByteArray(advertisement->ToBytes())});

  FindAdvertisement(loss_advertisement_data,
                    {ByteArray(advertisement->ToBytes())}, fetch_latch);

  // We should receive a client callback of a lost peripheral
  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());
}

TEST_P(DiscoveredPeripheralTrackerTest, HandleDummyAdvertisement) {
  auto flag = nearby::FeatureFlags::Flags{
      .enable_invoking_legacy_device_discovered_cb = true,
  };
  MediumEnvironment::Instance().SetFeatureFlags(flag);
  api::ble_v2::BleAdvertisementData advertising_data{};
  advertising_data.is_extended_advertisement = false;
  ByteArray encoded_bytes{
      DiscoveredPeripheralTracker::kDummyAdvertisementValue};
  advertising_data.service_data.insert(
      {mediums::bleutils::kCopresenceServiceUuid, encoded_bytes});
  CountDownLatch fetch_latch(1);
  CountDownLatch legacy_device_found_latch(1);

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [](BleV2Peripheral peripheral, const std::string& service_id,
                 const ByteArray& advertisement_bytes,
                 bool fast_advertisement) {
                FAIL() << "Should NOT report found for dummy advertisement";
              },
          .legacy_device_discovered_cb =
              [&legacy_device_found_latch]() {
                legacy_device_found_latch.CountDown();
              },
      },
      {});

  FindAdvertisement(advertising_data, {}, fetch_latch);

  EXPECT_TRUE(legacy_device_found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 0);
}

TEST_P(DiscoveredPeripheralTrackerTest, SkipDummyAdvertisement) {
  auto flag = nearby::FeatureFlags::Flags{
      .enable_invoking_legacy_device_discovered_cb = false,
  };
  MediumEnvironment::Instance().SetFeatureFlags(flag);
  api::ble_v2::BleAdvertisementData advertising_data{};
  advertising_data.is_extended_advertisement = false;
  ByteArray encoded_bytes{
      DiscoveredPeripheralTracker::kDummyAdvertisementValue};
  advertising_data.service_data.insert(
      {mediums::bleutils::kCopresenceServiceUuid, encoded_bytes});
  CountDownLatch fetch_latch(1);
  CountDownLatch legacy_device_found_latch(1);

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [](BleV2Peripheral peripheral, const std::string& service_id,
                 const ByteArray& advertisement_bytes,
                 bool fast_advertisement) {
                FAIL() << "Should NOT report found for dummy advertisement";
              },
          .legacy_device_discovered_cb =
              []() {
                FAIL() << "Should NOT report found for dummy advertisement";
              },
      },
      {});

  FindAdvertisement(advertising_data, {}, fetch_latch);

  EXPECT_FALSE(legacy_device_found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 0);
}

TEST_P(DiscoveredPeripheralTrackerTest, FetchGattAdvertisementInThread) {
  EnableFetchGattAdvertisementInThread();
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(1);
  CountDownLatch fetch_latch(1);

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
      },
      {});

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisementWithSlowFetcher(advertisement_data, {advertisement_bytes},
                                   fetch_latch);

  // We should receive a client callback of a peripheral discovery.
  fetch_latch.Await(kWaitDuration);
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       IgnoreGattAdvertisementResultWhentrackingStoppedInThread) {
  EnableFetchGattAdvertisementInThread();
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(1);
  CountDownLatch fetch_latch(1);

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
      },
      {});

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisementWithSlowFetcher(advertisement_data, {advertisement_bytes},
                                   fetch_latch);

  // We should receive a client callback of a peripheral discovery.
  absl::SleepFor(absl::Milliseconds(20));
  discovered_peripheral_tracker_.StopTracking(std::string(kServiceIdA));
  fetch_latch.Await(kWaitDuration);
  EXPECT_FALSE(found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       FetchMultipleGattAdvertisementResultsInThread) {
  EnableFetchGattAdvertisementInThread();
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray advertisement_header_bytes_2 = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  ByteArray advertisement_bytes_2 = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData2)),
      ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(2);
  CountDownLatch fetch_latch(2);

  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      {
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
      },
      {});

  api::ble_v2::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisementWithSlowFetcher(advertisement_data, {advertisement_bytes},
                                   fetch_latch);

  api::ble_v2::BleAdvertisementData advertisement_data_2{};
  if (!advertisement_header_bytes_2.Empty()) {
    advertisement_data_2.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes_2});
  }

  FindAdvertisementWithSlowFetcher(advertisement_data_2,
                                   {advertisement_bytes_2}, fetch_latch);

  // We should receive a client callback of a peripheral discovery.
  fetch_latch.Await(kWaitDuration);
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 2);
}

INSTANTIATE_TEST_SUITE_P(DiscoveredPeripheralTrackerFlagsTest,
                         DiscoveredPeripheralTrackerTest,
                         /*kEnableGattQueryInThread=*/testing::Bool());

}  // namespace

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
