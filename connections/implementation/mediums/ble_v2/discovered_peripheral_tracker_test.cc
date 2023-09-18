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

#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "connections/implementation/mediums/ble_v2/advertisement_read_result.h"
#include "connections/implementation/mediums/ble_v2/ble_utils.h"
#include "connections/implementation/mediums/ble_v2/bloom_filter.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"

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

class DiscoveredPeripheralTrackerTest : public testing::Test {
 public:
  void SetUp() override {
    MediumEnvironment::Instance().Start();
    adapter_peripheral_ = std::make_unique<BluetoothAdapter>();
    adapter_central_ = std::make_unique<BluetoothAdapter>();
    ble_peripheral_ = std::make_unique<BleV2Medium>(*adapter_peripheral_);
    ble_central_ = std::make_unique<BleV2Medium>(*adapter_central_);
  }

  void TearDown() override { MediumEnvironment::Instance().Stop(); }

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

  int GetFetchAdvertisementCallbackCount() const {
    MutexLock lock(&mutex_);
    return fetch_count_;
  }

 protected:
  // A stub Advertisement fetcher.
  DiscoveredPeripheralTracker::AdvertisementFetcher GetAdvertisementFetcher(
      CountDownLatch& fetch_latch,
      const std::vector<ByteArray>& advertisement_bytes_list) {
    return [this, &fetch_latch, &advertisement_bytes_list](
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

  std::unique_ptr<BluetoothAdapter> adapter_peripheral_;
  std::unique_ptr<BluetoothAdapter> adapter_central_;
  std::unique_ptr<BleV2Medium> ble_peripheral_;
  std::unique_ptr<BleV2Medium> ble_central_;
  mutable Mutex mutex_;
  int fetch_count_ ABSL_GUARDED_BY(mutex_) = 0;
  DiscoveredPeripheralTracker discovered_peripheral_tracker_;
};

TEST_F(DiscoveredPeripheralTrackerTest,
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

  api::ble_v2::BleAdvertisementData advertisement_data;
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

TEST_F(DiscoveredPeripheralTrackerTest,
       CanStartMultipleTrackingWithSameServiceId) {
  ByteArray fast_advertisement_bytes = CreateFastBleAdvertisement(
      ByteArray(std::string(kData)), ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(3);
  CountDownLatch fetch_latch(3);
  int callback_times = 0;

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

  api::ble_v2::BleAdvertisementData advertisement_data;
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

TEST_F(DiscoveredPeripheralTrackerTest,
       FoundFastAdvertisementDuplicateAdvertisements) {
  ByteArray fast_advertisement_bytes = CreateFastBleAdvertisement(
      ByteArray(std::string(kData)), ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(3);
  CountDownLatch fetch_latch(3);
  int callback_times = 0;

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

  api::ble_v2::BleAdvertisementData advertisement_data;
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

TEST_F(DiscoveredPeripheralTrackerTest,
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

  api::ble_v2::BleAdvertisementData advertisement_data;
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

TEST_F(DiscoveredPeripheralTrackerTest,
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

  api::ble_v2::BleAdvertisementData advertisement_data;
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

TEST_F(DiscoveredPeripheralTrackerTest,
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

  api::ble_v2::BleAdvertisementData advertisement_data;
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

TEST_F(DiscoveredPeripheralTrackerTest,
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

  api::ble_v2::BleAdvertisementData advertisement_data;
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

TEST_F(DiscoveredPeripheralTrackerTest,
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
  int callback_times = 0;

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

  api::ble_v2::BleAdvertisementData advertisement_data;
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
  EXPECT_EQ(callback_times, 1);
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);
}

TEST_F(DiscoveredPeripheralTrackerTest,
       FoundBleAdvertisementDuplicateAdvertisements) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(3);
  CountDownLatch fetch_latch(3);
  int callback_times = 0;

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

  api::ble_v2::BleAdvertisementData advertisement_data;
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

TEST_F(DiscoveredPeripheralTrackerTest,
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

  api::ble_v2::BleAdvertisementData advertisement_data;
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

TEST_F(DiscoveredPeripheralTrackerTest,
       LostPeripheralForFastAdvertisementLost) {
  std::vector<std::string> service_ids = {};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray fast_advertisement_bytes = CreateFastBleAdvertisement(
      ByteArray(std::string(kData)), ByteArray(std::string(kDeviceToken)));
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(2);
  CountDownLatch fetch_latch(1);
  int lost_callback_times = 0;

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

  api::ble_v2::BleAdvertisementData advertisement_data;
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

TEST_F(DiscoveredPeripheralTrackerTest,
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

  api::ble_v2::BleAdvertisementData advertisement_data;
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

TEST_F(DiscoveredPeripheralTrackerTest, LostPeripheralForAdvertisementLost) {
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

  api::ble_v2::BleAdvertisementData advertisement_data;
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
  // recently discovered eripheral in its 'found' pool. The second one should
  // trigger the onLost callback.
  discovered_peripheral_tracker_.ProcessLostGattAdvertisements();
  discovered_peripheral_tracker_.ProcessLostGattAdvertisements();

  // We should receive a client callback of a lost peripheral
  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());
}

TEST_F(DiscoveredPeripheralTrackerTest,
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

  api::ble_v2::BleAdvertisementData advertisement_data;
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

TEST_F(DiscoveredPeripheralTrackerTest,
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

  api::ble_v2::BleAdvertisementData advertisement_data;
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

}  // namespace

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
