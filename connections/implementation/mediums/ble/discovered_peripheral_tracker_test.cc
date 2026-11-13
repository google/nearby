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

#include "connections/implementation/mediums/ble/discovered_peripheral_tracker.h"

#include <atomic>
#include <list>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/base/thread_annotations.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "connections/implementation/mediums/advertisements/dct_advertisement.h"
#include "connections/implementation/mediums/ble/advertisement_read_result.h"
#include "connections/implementation/mediums/ble/ble_advertisement.h"
#include "connections/implementation/mediums/ble/ble_advertisement_header.h"
#include "connections/implementation/mediums/ble/ble_utils.h"
#include "connections/implementation/mediums/ble/bloom_filter.h"
#include "connections/implementation/mediums/ble/discovered_peripheral_callback.h"
#include "connections/implementation/mediums/ble/instant_on_lost_advertisement.h"
#include "connections/implementation/mediums/utils.h"
#include "connections/implementation/pcp.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/ble.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/mac_address.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace connections {
namespace mediums {

namespace {
using ::testing::UnorderedElementsAre;

constexpr absl::Duration kWaitDuration = absl::Milliseconds(1000);
constexpr absl::string_view kFastAdvertisementServiceUuid = "FE2C";
constexpr absl::string_view kServiceIdA = "A";
constexpr absl::string_view kServiceIdB = "B";
constexpr absl::string_view kData = "\x04\x02\x00";
constexpr absl::string_view kData2 = "\x07\x00\x07";
constexpr absl::string_view kDeviceToken = "\x04\x20";
constexpr absl::string_view kDeviceName = "device";
constexpr absl::Duration kDefaultGattFetchDelay = absl::Milliseconds(200);

// Delay to allow background threads to complete during teardown.
// This helps prevent use-after-free errors on resources like FakeClock
// used by background tasks (e.g., in PendingJobRegistry).
constexpr absl::Duration kTeardownDelay = absl::Milliseconds(400);

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

ByteArray CreateDctAdvertisement(const std::string& service_id,
                                 const std::string& device_name) {
  auto dct_advertisement = advertisements::ble::DctAdvertisement::Create(
      service_id, device_name, 0xf100, 0x01);
  return ByteArray(dct_advertisement->ToData());
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

ByteArray CreateExtendedBleAdvertisementHeader(
    const ByteArray& advertisement_hash,
    std::vector<std::string>& service_ids) {
  BloomFilter service_id_bloom_filter(
      std::make_unique<BitSetImpl<
          BleAdvertisementHeader::kServiceIdBloomFilterByteLength>>());

  for (const std::string& service_id : service_ids) {
    service_id_bloom_filter.Add(service_id);
  }

  return ByteArray(BleAdvertisementHeader(
      BleAdvertisementHeader::Version::kV2,
      /*extended_advertisement=*/true,
      /*num_slots=*/service_ids.size(), ByteArray(service_id_bloom_filter),
      advertisement_hash, BleAdvertisementHeader::kDefaultPsmValue));
}

ByteArray GenerateRandomAdvertisementHash() {
  ByteArray random_advertisement_hash = Utils::GenerateRandomBytes(
      BleAdvertisementHeader::kAdvertisementHashByteLength);

  return random_advertisement_hash;
}

class MockDiscoveredPeripheralCallback : public DiscoveredPeripheralCallback {
 public:
  MOCK_METHOD(void, OnPeripheralDiscovered,
              (BlePeripheral, const std::string&, const ByteArray&, bool), ());
  MOCK_METHOD(void, OnPeripheralLost,
              (BlePeripheral, const std::string&, const ByteArray&, bool), ());
  MOCK_METHOD(void, OnInstantLost,
              (BlePeripheral, const std::string&, const ByteArray&, bool), ());
  MOCK_METHOD(void, OnLegacyDeviceDiscovered, (), ());
};

}  // namespace

class DiscoveredPeripheralTrackerTest
    : public testing::TestWithParam<
          std::tuple</*is_extended_advertisement_available=*/bool>> {
 public:
  void SetUp() override {
    bool is_extended_advertisement_available = std::get<0>(GetParam());
    EnvironmentConfig config{
        .webrtc_enabled = false,
        .use_simulated_clock = true,
    };
    MediumEnvironment::Instance().Start(config);
    discovered_peripheral_tracker_ = absl::WrapUnique(
        new DiscoveredPeripheralTracker(is_extended_advertisement_available,
                                        /*start_fetch_executor=*/false));
    // Do not start the fetch executor since we will manually run the GATT
    // fetch loop in the test.
    adapter_peripheral_ = std::make_unique<BluetoothAdapter>();
    adapter_central_ = std::make_unique<BluetoothAdapter>();
    ble_peripheral_ = std::make_unique<BleMedium>(*adapter_peripheral_);
    ble_central_ = std::make_unique<BleMedium>(*adapter_central_);
  }

  void TearDown() override {
    discovered_peripheral_tracker_.reset();

    // Add a small delay to allow background threads to complete.
    absl::SleepFor(kTeardownDelay);
    MediumEnvironment::Instance().Stop();
    NearbyFlags::GetInstance().ResetOverridedValues();
  }

  BlePeripheral CreateBlePeripheral() {
    return BlePeripheral(*ble_central_,
                         adapter_peripheral_->GetAddress().address());
  }

  void SetupMultipleAdvertisementsState(
      const BleAdvertisementHeader& header,
      const BleAdvertisement& advertisement_1,
      const BleAdvertisement& advertisement_2) {
    absl::MutexLock lock(discovered_peripheral_tracker_->mutex_);
    ByteArray advertisement_bytes_1 = advertisement_1.ByteArrayWithExtraField();
    ByteArray advertisement_bytes_2 = advertisement_2.ByteArrayWithExtraField();
    std::vector<const ByteArray*> gatt_advertisement_bytes_list = {
        &advertisement_bytes_1, &advertisement_bytes_2};

    discovered_peripheral_tracker_->HandleRawGattAdvertisements(
        CreateBlePeripheral(), header, gatt_advertisement_bytes_list,
        /*service_uuid=*/{});
  }

  void RegisterServiceIdCallback(const std::string& service_id,
                                 absl::Mutex& lost_mutex, int& lost_count) {
    discovered_peripheral_tracker_->StartTracking(
        service_id, /*include_dct_advertisement=*/false, Pcp::kP2pPointToPoint,
        {
            .instant_lost_cb =
                [&lost_mutex, &lost_count](BlePeripheral peripheral,
                                           const std::string& service_id,
                                           const ByteArray& advertisement_bytes,
                                           bool fast_advertisement) {
                  absl::MutexLock lock(lost_mutex);
                  ++lost_count;
                },
        },
        /*fast_advertisement_service_uuid=*/{});
  }

  bool CallHandleOnLostAdvertisementLocked(
      const api::ble::BleAdvertisementData& advertisement_data) {
    absl::MutexLock lock(discovered_peripheral_tracker_->mutex_);
    return discovered_peripheral_tracker_->HandleOnLostAdvertisementLocked(
        advertisement_data);
  }

  // Simulates to see a fast advertisement.
  void FindFastAdvertisement(
      const api::ble::BleAdvertisementData& advertisement_data,
      const std::vector<ByteArray>& advertisement_bytes_list,
      absl::Notification& fetch_latch) {
    BlePeripheral peripheral = CreateBlePeripheral();

    discovered_peripheral_tracker_->ProcessFoundBleAdvertisement(
        peripheral, advertisement_data,
        GetAdvertisementFetcher(fetch_latch, advertisement_bytes_list));
  }

  // Simulates to see an extended advertisement, which don't need to fetch
  // advertisement from GATT.
  void FindExtendedAdvertisement(
      const api::ble::BleAdvertisementData& advertisement_data,
      absl::Notification& fetch_latch) {
    BlePeripheral peripheral = CreateBlePeripheral();
    DiscoveredPeripheralTracker::AdvertisementFetcher placeholder_fetcher =
        [](BlePeripheral, int, int, const std::vector<std::string>&,
           mediums::AdvertisementReadResult&) {};
    discovered_peripheral_tracker_->ProcessFoundBleAdvertisement(
        peripheral, advertisement_data, std::move(placeholder_fetcher));
    if (!fetch_latch.HasBeenNotified()) {
      fetch_latch.Notify();
    }
  }

  // Simulates to see a regular advertisement.
  void FindAdvertisement(
      const api::ble::BleAdvertisementData& advertisement_data,
      const std::vector<ByteArray>& advertisement_bytes_list,
      absl::Notification& fetch_latch) {
    BlePeripheral peripheral = CreateBlePeripheral();

    discovered_peripheral_tracker_->ProcessFoundBleAdvertisement(
        peripheral, advertisement_data,
        GetAdvertisementFetcher(fetch_latch, advertisement_bytes_list));
  }

  void FindAdvertisementWithDelay(
      const api::ble::BleAdvertisementData& advertisement_data,
      const std::vector<ByteArray>& advertisement_bytes_list,
      absl::Notification& fetch_latch, absl::Duration delay) {
    BlePeripheral peripheral = CreateBlePeripheral();

    discovered_peripheral_tracker_->ProcessFoundBleAdvertisement(
        peripheral, advertisement_data,
        GetAdvertisementFetcherWithDelay(fetch_latch, advertisement_bytes_list,
                                         delay));
  }

  int GetFetchAdvertisementCallbackCount() const {
    absl::MutexLock lock(mutex_);
    return fetch_count_;
  }

 protected:
  // A stub Advertisement fetcher.
  DiscoveredPeripheralTracker::AdvertisementFetcher GetAdvertisementFetcher(
      absl::Notification& fetch_latch,
      const std::vector<ByteArray>& advertisement_bytes_list) {
    return [this, &fetch_latch, advertisement_bytes_list](
               BlePeripheral peripheral, int num_slots, int psm,
               const std::vector<std::string>& interesting_service_ids,
               mediums::AdvertisementReadResult& advertisement_read_result) {
      absl::MutexLock lock(mutex_);
      fetch_count_++;
      int slot = 0;
      for (const auto& advertisement_bytes : advertisement_bytes_list) {
        advertisement_read_result.AddAdvertisement(slot++, advertisement_bytes);
      }
      advertisement_read_result.RecordLastReadStatus(
          /*is_success=*/true);
      if (!fetch_latch.HasBeenNotified()) {
        fetch_latch.Notify();
      }
    };
  }

  DiscoveredPeripheralTracker::AdvertisementFetcher
  GetAdvertisementFetcherWithDelay(
      absl::Notification& fetch_latch,
      const std::vector<ByteArray>& advertisement_bytes_list,
      absl::Duration delay) {
    return [this, &fetch_latch, advertisement_bytes_list, delay](
               BlePeripheral peripheral, int num_slots, int psm,
               const std::vector<std::string>& interesting_service_ids,
               mediums::AdvertisementReadResult& advertisement_read_result) {
      absl::MutexLock lock(mutex_);
      fetch_count_++;
      int slot = 0;
      // In real environment, the GATT fetch may run about 3-5 seconds.
      absl::SleepFor(delay);
      for (const auto& advertisement_bytes : advertisement_bytes_list) {
        advertisement_read_result.AddAdvertisement(slot++, advertisement_bytes);
      }
      advertisement_read_result.RecordLastReadStatus(
          /*is_success=*/true);
      if (!fetch_latch.HasBeenNotified()) {
        fetch_latch.Notify();
      }
    };
  }

  std::unique_ptr<BluetoothAdapter> adapter_peripheral_;
  std::unique_ptr<BluetoothAdapter> adapter_central_;
  std::unique_ptr<BleMedium> ble_peripheral_;
  std::unique_ptr<BleMedium> ble_central_;
  mutable absl::Mutex mutex_;
  int fetch_count_ ABSL_GUARDED_BY(mutex_) = 0;
  std::unique_ptr<DiscoveredPeripheralTracker> discovered_peripheral_tracker_;
};

TEST_P(DiscoveredPeripheralTrackerTest,
       FoundFastAdvertisementPeripheralDiscovered) {
  ByteArray fast_advertisement_bytes = CreateFastBleAdvertisement(
      ByteArray(std::string(kData)), ByteArray(std::string(kDeviceToken)));
  absl::Notification found_latch;
  absl::Notification fetch_latch;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                if (!found_latch.HasBeenNotified()) {
                  found_latch.Notify();
                }
              },
      },
      Uuid(kFastAdvertisementServiceUuid));
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
  if (!fast_advertisement_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {Uuid(kFastAdvertisementServiceUuid), fast_advertisement_bytes});
  }

  FindFastAdvertisement(advertisement_data, {}, fetch_latch);

  // We should receive a client callback of a peripheral discovery without a
  // GATT read.
  fetch_latch.WaitForNotificationWithTimeout(kWaitDuration);
  EXPECT_TRUE(found_latch.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 0);
}

TEST_P(DiscoveredPeripheralTrackerTest, DctAdvertisementPeripheralDiscovered) {
  ByteArray dct_advertisement_bytes = CreateDctAdvertisement(
      std::string(kServiceIdA), std::string(kDeviceName));
  absl::Notification found_latch;
  absl::Notification fetch_latch;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), true, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_FALSE(fast_advertisement);
                if (!found_latch.HasBeenNotified()) {
                  found_latch.Notify();
                }
              },
      },
      Uuid(kFastAdvertisementServiceUuid));
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
  advertisement_data.service_data.insert(
      {bleutils::kDctServiceUuid, dct_advertisement_bytes});

  FindAdvertisement(advertisement_data, {}, fetch_latch);

  // We should receive a client callback of a peripheral discovery without a
  // GATT read.
  fetch_latch.WaitForNotificationWithTimeout(kWaitDuration);
  EXPECT_TRUE(found_latch.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 0);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       ReportFoundLegacyDeviceWhenFoundBleAdvertisementPeripheralDiscovered) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  absl::Notification found_latch;
  absl::Notification legacy_found_latch;
  absl::Notification fetch_latch;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {.peripheral_discovered_cb =
           [&found_latch](
               BlePeripheral peripheral, const std::string& service_id,
               const ByteArray& advertisement_bytes, bool fast_advertisement) {
             EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
             EXPECT_FALSE(fast_advertisement);
             if (!found_latch.HasBeenNotified()) {
               found_latch.Notify();
             }
           },
       .legacy_device_discovered_cb =
           [&legacy_found_latch]() {
             if (!legacy_found_latch.HasBeenNotified()) {
               legacy_found_latch.Notify();
             }
           }},
      {});
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);

  // We should receive a client callback of a peripheral discovery.
  fetch_latch.WaitForNotificationWithTimeout(kWaitDuration);
  EXPECT_TRUE(legacy_found_latch.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_TRUE(found_latch.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       CanStartMultipleTrackingWithSameServiceId) {
  ByteArray fast_advertisement_bytes = CreateFastBleAdvertisement(
      ByteArray(std::string(kData)), ByteArray(std::string(kDeviceToken)));
  absl::Notification found_latch_1;
  absl::Notification found_latch_2;
  absl::Notification found_latch_3;
  absl::Notification fetch_latch_1;
  absl::Notification fetch_latch_2;
  absl::Notification fetch_latch_3;
  std::atomic<int> callback_times = 0;

  // 1st tracking.
  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&callback_times, &found_latch_1](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                callback_times++;
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                if (!found_latch_1.HasBeenNotified()) {
                  found_latch_1.Notify();
                }
              },
      },
      Uuid(kFastAdvertisementServiceUuid));
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
  if (!fast_advertisement_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {Uuid(kFastAdvertisementServiceUuid), fast_advertisement_bytes});
  }

  FindFastAdvertisement(advertisement_data, {}, fetch_latch_1);

  // 2nd tracking.
  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&callback_times, &found_latch_2](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                callback_times++;
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                if (!found_latch_2.HasBeenNotified()) {
                  found_latch_2.Notify();
                }
              },
      },
      Uuid(kFastAdvertisementServiceUuid));

  FindFastAdvertisement(advertisement_data, {}, fetch_latch_2);

  // 3rd tracking.
  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&callback_times, &found_latch_3](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                callback_times++;
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                if (!found_latch_3.HasBeenNotified()) {
                  found_latch_3.Notify();
                }
              },
      },
      Uuid(kFastAdvertisementServiceUuid));

  FindFastAdvertisement(advertisement_data, {}, fetch_latch_3);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       FoundFastAdvertisementDuplicateAdvertisements) {
  ByteArray fast_advertisement_bytes = CreateFastBleAdvertisement(
      ByteArray(std::string(kData)), ByteArray(std::string(kDeviceToken)));
  absl::Mutex found_mutex;
  int found_count = 0;
  absl::Notification fetch_latch_1;
  absl::Notification fetch_latch_2;
  absl::Notification fetch_latch_3;
  std::atomic<int> callback_times = 0;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&callback_times, &found_mutex, &found_count](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                callback_times++;
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                absl::MutexLock lock(found_mutex);
                found_count++;
              },
      },
      Uuid(kFastAdvertisementServiceUuid));
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
  if (!fast_advertisement_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {Uuid(kFastAdvertisementServiceUuid), fast_advertisement_bytes});
  }

  FindFastAdvertisement(advertisement_data, {}, fetch_latch_1);
  FindFastAdvertisement(advertisement_data, {}, fetch_latch_2);
  FindFastAdvertisement(advertisement_data, {}, fetch_latch_3);

  // We should only receive ONE client callback of a peripheral discovery with
  // ZERO GATT reads.
  fetch_latch_1.WaitForNotificationWithTimeout(kWaitDuration * 3);
  EXPECT_EQ(callback_times, 1);
  // The count won't reach 3 since the callback only been called once.
  // Let it time out.
  {
    absl::MutexLock lock(found_mutex);
    EXPECT_FALSE(found_mutex.AwaitWithTimeout(
        absl::Condition(+[](int* c) { return *c >= 3; }, &found_count),
        3 * kWaitDuration));
  }
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 0);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       FoundFastAdvertisementUntrackedFastAdvertisementServiceUuid) {
  ByteArray fast_advertisement_bytes = CreateFastBleAdvertisement(
      ByteArray(std::string(kData)), ByteArray(std::string(kDeviceToken)));
  absl::Notification found_latch;
  absl::Notification fetch_latch;

  // Start tracking a service ID and then process a discovery containing a valid
  // fast advertisement, but under a different service UUID.
  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                if (!found_latch.HasBeenNotified()) {
                  found_latch.Notify();
                }
              },
      },
      Uuid("FE3C"));
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
  if (!fast_advertisement_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {Uuid(kFastAdvertisementServiceUuid), fast_advertisement_bytes});
  }

  FindFastAdvertisement(advertisement_data, {}, fetch_latch);

  // We should get no new discoveries.
  fetch_latch.WaitForNotificationWithTimeout(kWaitDuration);
  EXPECT_FALSE(found_latch.WaitForNotificationWithTimeout(kWaitDuration));
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
  absl::Notification found_latch_a;
  absl::Notification found_latch_b;
  absl::Notification fetch_latch;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch_a](BlePeripheral peripheral,
                               const std::string& service_id,
                               const ByteArray& advertisement_bytes,
                               bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                if (!found_latch_a.HasBeenNotified()) {
                  found_latch_a.Notify();
                }
              },
      },
      Uuid(kFastAdvertisementServiceUuid));
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdB), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch_b](BlePeripheral peripheral,
                               const std::string& service_id,
                               const ByteArray& advertisement_bytes,
                               bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                if (!found_latch_b.HasBeenNotified()) {
                  found_latch_b.Notify();
                }
              },
      },
      {});

  api::ble::BleAdvertisementData advertisement_data{};
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
  fetch_latch.WaitForNotificationWithTimeout(kWaitDuration);
  EXPECT_TRUE(found_latch_a.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_TRUE(found_latch_b.WaitForNotificationWithTimeout(kWaitDuration));
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
  absl::Notification found_latch;
  absl::Notification fetch_latch;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                if (!found_latch.HasBeenNotified()) {
                  found_latch.Notify();
                }
              },
      },
      {});
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);

  // We should receive a client callback of a peripheral discovery.
  fetch_latch.WaitForNotificationWithTimeout(kWaitDuration);
  EXPECT_TRUE(found_latch.WaitForNotificationWithTimeout(kWaitDuration));
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
  absl::Notification found_latch;
  absl::Notification fetch_latch;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                if (!found_latch.HasBeenNotified()) {
                  found_latch.Notify();
                }
              },
      },
      {});
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data, {legacy_advertisement_bytes},
                    fetch_latch);

  // We should not receive a client callback with the Version/SocketVersion kV1.
  fetch_latch.WaitForNotificationWithTimeout(kWaitDuration);
  EXPECT_FALSE(found_latch.WaitForNotificationWithTimeout(kWaitDuration));
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
  absl::Notification found_latch;
  absl::Notification fetch_latch;
  std::atomic<int> callback_times = 0;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&callback_times, &found_latch](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                callback_times++;
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData2)));
                EXPECT_FALSE(fast_advertisement);
                if (!found_latch.HasBeenNotified()) {
                  found_latch.Notify();
                }
              },
      },
      {});
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data,
                    {advertisement_bytes, legacy_advertisement_bytes},
                    fetch_latch);

  // We should only receive one callback with data from the V2 GATT
  // advertisement.
  fetch_latch.WaitForNotificationWithTimeout(kWaitDuration);
  EXPECT_TRUE(found_latch.WaitForNotificationWithTimeout(kWaitDuration));
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
  absl::Mutex found_mutex;
  int found_count = 0;
  absl::Notification fetch_latch_1;
  absl::Notification fetch_latch_2;
  absl::Notification fetch_latch_3;
  std::atomic<int> callback_times = 0;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&callback_times, &found_mutex, &found_count](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                callback_times++;
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                absl::MutexLock lock(found_mutex);
                found_count++;
              },
      },
      {});
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch_1);
  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch_2);
  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch_3);

  // We should only receive ONE client callback of a peripheral discovery. And
  // we should also do only ONE GATT read.
  {
    absl::MutexLock lock(mutex_);
    auto fetch_cond = [this]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
      return fetch_count_ >= 3;
    };
    mutex_.AwaitWithTimeout(absl::Condition(&fetch_cond), kWaitDuration);
  }
  EXPECT_EQ(callback_times, 1);
  // The count won't reach 3 since the callback only been called once.
  // Let it time out.
  {
    absl::MutexLock lock(found_mutex);
    EXPECT_FALSE(found_mutex.AwaitWithTimeout(
        absl::Condition(+[](int* c) { return *c >= 3; }, &found_count),
        3 * kWaitDuration));
  }
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
  absl::Notification found_latch;
  absl::Notification fetch_latch;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                if (!found_latch.HasBeenNotified()) {
                  found_latch.Notify();
                }
              },
      },
      {});
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);

  // We should get no new discoveries.
  fetch_latch.WaitForNotificationWithTimeout(kWaitDuration);
  EXPECT_FALSE(found_latch.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       LostPeripheralForFastAdvertisementLost) {
  std::vector<std::string> service_ids = {};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray fast_advertisement_bytes = CreateFastBleAdvertisement(
      ByteArray(std::string(kData)), ByteArray(std::string(kDeviceToken)));
  absl::Notification found_latch;
  absl::Mutex lost_mutex;
  int lost_count = 0;
  absl::Notification fetch_latch;
  std::atomic<int> lost_callback_times = 0;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                if (!found_latch.HasBeenNotified()) {
                  found_latch.Notify();
                }
              },
          .peripheral_lost_cb =
              [&lost_mutex, &lost_count, &lost_callback_times](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                lost_callback_times++;
                absl::MutexLock lock(lost_mutex);
                lost_count++;
              },
      },
      Uuid(kFastAdvertisementServiceUuid));
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
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
  fetch_latch.WaitForNotificationWithTimeout(kWaitDuration);
  EXPECT_TRUE(found_latch.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 0);

  // Then, go through two cycles of onLost. The first cycle should include the
  // recently discovered peripheral in its 'found' pool. The second one should
  // trigger the onLost callback.
  discovered_peripheral_tracker_->ProcessLostGattAdvertisements();
  discovered_peripheral_tracker_->ProcessLostGattAdvertisements();

  // We should receive a client callback of a lost peripheral.
  {
    absl::MutexLock lock(lost_mutex);
    EXPECT_FALSE(lost_mutex.AwaitWithTimeout(
        absl::Condition(+[](int* c) { return *c >= 2; }, &lost_count),
        kWaitDuration));
  }
  EXPECT_EQ(lost_callback_times, 1);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       FoundFastAdvertisementAlmostLostPeripheral) {
  std::vector<std::string> service_ids = {};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray fast_advertisement_bytes = CreateFastBleAdvertisement(
      ByteArray(std::string(kData)), ByteArray(std::string(kDeviceToken)));
  absl::Notification found_latch;
  absl::Notification lost_latch;
  absl::Notification fetch_latch;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                if (!found_latch.HasBeenNotified()) {
                  found_latch.Notify();
                }
              },
          .peripheral_lost_cb =
              [&lost_latch](BlePeripheral peripheral,
                            const std::string& service_id,
                            const ByteArray& advertisement_bytes,
                            bool fast_advertisement) {
                if (!lost_latch.HasBeenNotified()) {
                  lost_latch.Notify();
                }
              },
      },
      Uuid(kFastAdvertisementServiceUuid));
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
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
    discovered_peripheral_tracker_->ProcessLostGattAdvertisements();
  }

  // We should only receive ONE client callback of a peripheral discovery, ZERO
  // GATT reads, and no onLost calls.
  fetch_latch.WaitForNotificationWithTimeout(kWaitDuration);
  EXPECT_TRUE(found_latch.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 0);
  EXPECT_FALSE(lost_latch.WaitForNotificationWithTimeout(kWaitDuration));
}

TEST_P(DiscoveredPeripheralTrackerTest, LostPeripheralForAdvertisementLost) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  absl::Notification found_latch;
  absl::Notification lost_latch;
  absl::Notification fetch_latch;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                if (!found_latch.HasBeenNotified()) {
                  found_latch.Notify();
                }
              },
          .peripheral_lost_cb =
              [&lost_latch](BlePeripheral peripheral,
                            const std::string& service_id,
                            const ByteArray& advertisement_bytes,
                            bool fast_advertisement) {
                if (!lost_latch.HasBeenNotified()) {
                  lost_latch.Notify();
                }
              },
      },
      {});
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);

  // We should receive a client callback of a peripheral discovery.
  fetch_latch.WaitForNotificationWithTimeout(kWaitDuration);
  EXPECT_TRUE(found_latch.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);

  // Then, go through two cycles of onLost. The first cycle should include the
  // recently discovered peripheral in its 'found' pool. The second one should
  // trigger the onLost callback.
  discovered_peripheral_tracker_->ProcessLostGattAdvertisements();
  discovered_peripheral_tracker_->ProcessLostGattAdvertisements();

  // We should receive a client callback of a lost peripheral
  EXPECT_TRUE(lost_latch.WaitForNotificationWithTimeout(kWaitDuration));
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
  absl::Notification found_latch_a;
  absl::Notification found_latch_b;
  absl::Notification lost_latch_a;
  absl::Notification lost_latch_b;
  absl::Notification fetch_latch;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch_a](BlePeripheral peripheral,
                               const std::string& service_id,
                               const ByteArray& advertisement_bytes,
                               bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                if (!found_latch_a.HasBeenNotified()) {
                  found_latch_a.Notify();
                }
              },
          .peripheral_lost_cb =
              [&lost_latch_a](BlePeripheral peripheral,
                              const std::string& service_id,
                              const ByteArray& advertisement_bytes,
                              bool fast_advertisement) {
                if (!lost_latch_a.HasBeenNotified()) {
                  lost_latch_a.Notify();
                }
              },
      },
      Uuid(kFastAdvertisementServiceUuid));
  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdB), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch_b](BlePeripheral peripheral,
                               const std::string& service_id,
                               const ByteArray& advertisement_bytes,
                               bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                if (!found_latch_b.HasBeenNotified()) {
                  found_latch_b.Notify();
                }
              },
          .peripheral_lost_cb =
              [&lost_latch_b](BlePeripheral peripheral,
                              const std::string& service_id,
                              const ByteArray& advertisement_bytes,
                              bool fast_advertisement) {
                if (!lost_latch_b.HasBeenNotified()) {
                  lost_latch_b.Notify();
                }
              },
      },
      {});
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
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
  fetch_latch.WaitForNotificationWithTimeout(kWaitDuration);
  EXPECT_TRUE(found_latch_a.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_TRUE(found_latch_b.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);

  // Then, go through two cycles of onLost. The first cycle should include the
  // recently discovered peripheral in its 'found' pool. The second one should
  // trigger the onLost callback.
  discovered_peripheral_tracker_->ProcessLostGattAdvertisements();
  discovered_peripheral_tracker_->ProcessLostGattAdvertisements();

  // We should receive two client callbacks of a lost peripheral from each
  // service ID.
  EXPECT_TRUE(lost_latch_a.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_TRUE(lost_latch_b.WaitForNotificationWithTimeout(kWaitDuration));
}

TEST_P(DiscoveredPeripheralTrackerTest,
       LostPeripheralNotCallbackForUntrackedServiceId) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  absl::Notification found_latch;
  absl::Notification lost_latch;
  absl::Notification fetch_latch;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                if (!found_latch.HasBeenNotified()) {
                  found_latch.Notify();
                }
              },
          .peripheral_lost_cb =
              [&lost_latch](BlePeripheral peripheral,
                            const std::string& service_id,
                            const ByteArray& advertisement_bytes,
                            bool fast_advertisement) {
                if (!lost_latch.HasBeenNotified()) {
                  lost_latch.Notify();
                }
              },
      },
      {});
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);

  // We should receive a client callback of a peripheral discovery.
  fetch_latch.WaitForNotificationWithTimeout(kWaitDuration);
  EXPECT_TRUE(found_latch.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);

  // Then, stop tracking the service ID and go through two cycles of onLost.
  discovered_peripheral_tracker_->StopTracking(std::string(kServiceIdA));
  discovered_peripheral_tracker_->ProcessLostGattAdvertisements();
  discovered_peripheral_tracker_->ProcessLostGattAdvertisements();

  // We should NOT receive a client callback of a lost peripheral
  EXPECT_FALSE(lost_latch.WaitForNotificationWithTimeout(kWaitDuration));
}

TEST_P(DiscoveredPeripheralTrackerTest, InstantLostPeripheralForInstantOnLost) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_hash = GenerateRandomAdvertisementHash();
  ByteArray advertisement_header_bytes =
      CreateBleAdvertisementHeader(advertisement_hash, service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  absl::Notification found_latch;
  absl::Notification lost_latch;
  absl::Notification fetch_latch;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                if (!found_latch.HasBeenNotified()) {
                  found_latch.Notify();
                }
              },
          .instant_lost_cb =
              [&lost_latch](BlePeripheral peripheral,
                            const std::string& service_id,
                            const ByteArray& advertisement_bytes,
                            bool fast_advertisement) {
                if (!lost_latch.HasBeenNotified()) {
                  lost_latch.Notify();
                }
              },
      },
      {});
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);

  // We should receive a client callback of a peripheral discovery.
  fetch_latch.WaitForNotificationWithTimeout(kWaitDuration);
  ASSERT_TRUE(found_latch.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);

  auto advertisement = InstantOnLostAdvertisement::CreateFromHashes(
      std::list<std::string>({std::string(
          bleutils::GenerateAdvertisementHash(advertisement_bytes))}));
  ASSERT_OK(advertisement);
  api::ble::BleAdvertisementData loss_advertisement_data{};
  loss_advertisement_data.service_data.insert(
      {bleutils::kCopresenceServiceUuid, ByteArray(advertisement->ToBytes())});

  FindAdvertisement(loss_advertisement_data,
                    {ByteArray(advertisement->ToBytes())}, fetch_latch);

  // Then, go through a cycle of onLost. Since we triggered a forced loss via
  // the instant on los advertisement, the lost call should trigger the onLost
  // client callback.
  discovered_peripheral_tracker_->ProcessLostGattAdvertisements();

  // We should receive a client callback of a lost peripheral
  EXPECT_TRUE(lost_latch.WaitForNotificationWithTimeout(kWaitDuration));
}

TEST_P(DiscoveredPeripheralTrackerTest,
       InstantLostPeripheralForInstantOnLost_MultipleAdvertisements) {
  ByteArray advertisement_hash = GenerateRandomAdvertisementHash();
  BleAdvertisementHeader header(BleAdvertisementHeader::Version::kV2,
                                /*extended_advertisement=*/false,
                                /*num_slots=*/1, ByteArray{},  // bloom filter
                                advertisement_hash,
                                BleAdvertisementHeader::kDefaultPsmValue);

  ByteArray advertisement_bytes_1 = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  ByteArray advertisement_bytes_2 = CreateBleAdvertisement(
      std::string(kServiceIdB), ByteArray(std::string(kData2)),
      ByteArray(std::string(kDeviceToken)));

  auto adv_status_or_1 =
      BleAdvertisement::CreateBleAdvertisement(advertisement_bytes_1);
  ASSERT_OK(adv_status_or_1);
  BleAdvertisement advertisement_1 = adv_status_or_1.value();

  auto adv_status_or_2 =
      BleAdvertisement::CreateBleAdvertisement(advertisement_bytes_2);
  ASSERT_OK(adv_status_or_2);
  BleAdvertisement advertisement_2 = adv_status_or_2.value();

  // Register callbacks for both Service A and Service B.
  absl::Mutex lost_mutex;
  int lost_count = 0;
  RegisterServiceIdCallback(std::string(kServiceIdA), lost_mutex, lost_count);
  RegisterServiceIdCallback(std::string(kServiceIdB), lost_mutex, lost_count);

  // Use helper method to set up state manually.
  SetupMultipleAdvertisementsState(header, advertisement_1, advertisement_2);

  // Create OnLost advertisement for advertisement_1's hash.
  auto advertisement = InstantOnLostAdvertisement::CreateFromHashes(
      std::list<std::string>({std::string(bleutils::GenerateAdvertisementHash(
          advertisement_1.ByteArrayWithExtraField()))}));
  ASSERT_OK(advertisement);

  api::ble::BleAdvertisementData loss_advertisement_data{};
  loss_advertisement_data.service_data.insert(
      {bleutils::kCopresenceServiceUuid, ByteArray(advertisement->ToBytes())});

  // Call HandleOnLostAdvertisementLocked using helper.
  bool result = CallHandleOnLostAdvertisementLocked(loss_advertisement_data);
  EXPECT_TRUE(result);

  // Verify that both are lost (callback triggered twice).
  {
    absl::MutexLock lock(lost_mutex);
    EXPECT_TRUE(lost_mutex.AwaitWithTimeout(
        absl::Condition(+[](int* c) { return *c >= 2; }, &lost_count),
        kWaitDuration));
  }
}

TEST_P(DiscoveredPeripheralTrackerTest,
       IgnoreFoundAdvertisementForInstantOnLost) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_hash = GenerateRandomAdvertisementHash();
  ByteArray advertisement_header_bytes =
      CreateBleAdvertisementHeader(advertisement_hash, service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));

  absl::Notification fetch_latch;
  MockDiscoveredPeripheralCallback mock_callback;
  absl::Notification discovered_latch;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&mock_callback, &discovered_latch](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                mock_callback.OnPeripheralDiscovered(peripheral, service_id,
                                                     advertisement_bytes,
                                                     fast_advertisement);
                if (!discovered_latch.HasBeenNotified()) {
                  discovered_latch.Notify();
                }
              },
          .instant_lost_cb =
              [&mock_callback](BlePeripheral peripheral,
                               const std::string& service_id,
                               const ByteArray& advertisement_bytes,
                               bool fast_advertisement) {
                mock_callback.OnInstantLost(peripheral, service_id,
                                            advertisement_bytes,
                                            fast_advertisement);
              },
      },
      {});
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  EXPECT_CALL(mock_callback, OnPeripheralDiscovered).Times(1);
  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch);
  fetch_latch.WaitForNotificationWithTimeout(kWaitDuration);
  EXPECT_TRUE(discovered_latch.WaitForNotificationWithTimeout(kWaitDuration));

  // We should receive a client callback of a peripheral discovery.
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);

  absl::Notification fetch_latch2;
  auto advertisement = InstantOnLostAdvertisement::CreateFromHashes(
      std::list<std::string>({std::string(
          bleutils::GenerateAdvertisementHash(advertisement_bytes))}));
  ASSERT_OK(advertisement);
  api::ble::BleAdvertisementData loss_advertisement_data{};
  loss_advertisement_data.service_data.insert(
      {bleutils::kCopresenceServiceUuid, ByteArray(advertisement->ToBytes())});

  EXPECT_CALL(mock_callback, OnInstantLost).Times(1);
  FindAdvertisement(loss_advertisement_data,
                    {ByteArray(advertisement->ToBytes())}, fetch_latch2);
  fetch_latch2.WaitForNotificationWithTimeout(kWaitDuration);

  // Then, go through a cycle of onLost. Since we triggered a forced loss via
  // the instant on los advertisement, the lost call should trigger the onLost
  // client callback.
  discovered_peripheral_tracker_->ProcessLostGattAdvertisements();

  // Lost advertisement should not be reported.
  absl::Notification fetch_latch3;
  EXPECT_CALL(mock_callback, OnPeripheralDiscovered).Times(0);
  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch3);
  fetch_latch3.WaitForNotificationWithTimeout(kWaitDuration);
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

  absl::Notification found_latch;
  absl::Notification lost_latch;
  absl::Notification fetch_latch;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                if (!found_latch.HasBeenNotified()) {
                  found_latch.Notify();
                }
              },
          .instant_lost_cb =
              [&lost_latch](BlePeripheral peripheral,
                            const std::string& service_id,
                            const ByteArray& advertisement_bytes,
                            bool fast_advertisement) {
                if (!lost_latch.HasBeenNotified()) {
                  lost_latch.Notify();
                }
              },
      },
      Uuid(kFastAdvertisementServiceUuid));
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
  if (!fast_advertisement_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {Uuid(kFastAdvertisementServiceUuid), fast_advertisement_bytes});
  }

  FindFastAdvertisement(advertisement_data, {}, fetch_latch);

  // We should receive a client callback of a peripheral discovery.
  fetch_latch.WaitForNotificationWithTimeout(kWaitDuration);
  ASSERT_TRUE(found_latch.WaitForNotificationWithTimeout(kWaitDuration));

  auto advertisement = InstantOnLostAdvertisement::CreateFromHashes(
      std::list<std::string>({std::string(advertisement_hash)}));
  ASSERT_OK(advertisement);
  api::ble::BleAdvertisementData loss_advertisement_data{};
  loss_advertisement_data.service_data.insert(
      {bleutils::kCopresenceServiceUuid, ByteArray(advertisement->ToBytes())});

  FindAdvertisement(loss_advertisement_data,
                    {ByteArray(advertisement->ToBytes())}, fetch_latch);

  // We should receive a client callback of a lost peripheral
  EXPECT_TRUE(lost_latch.WaitForNotificationWithTimeout(kWaitDuration));
}

TEST_P(DiscoveredPeripheralTrackerTest, HandleDummyAdvertisement) {
  auto flag = nearby::FeatureFlags::Flags{
      .enable_invoking_legacy_device_discovered_cb = true,
  };
  MediumEnvironment::Instance().SetFeatureFlags(flag);
  api::ble::BleAdvertisementData advertising_data{};
  advertising_data.is_extended_advertisement = false;
  ByteArray encoded_bytes{
      DiscoveredPeripheralTracker::kDummyAdvertisementValue};
  advertising_data.service_data.insert(
      {mediums::bleutils::kCopresenceServiceUuid, encoded_bytes});
  absl::Notification fetch_latch;
  absl::Notification legacy_device_found_latch;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
               [](BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                 FAIL() << "Should NOT report found for dummy advertisement";
               },
          .legacy_device_discovered_cb =
              [&legacy_device_found_latch]() {
                if (!legacy_device_found_latch.HasBeenNotified()) {
                  legacy_device_found_latch.Notify();
                }
              },
      },
      {});
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  FindAdvertisement(advertising_data, {}, fetch_latch);

  EXPECT_TRUE(
      legacy_device_found_latch.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 0);
}

TEST_P(DiscoveredPeripheralTrackerTest, SkipDummyAdvertisement) {
  auto flag = nearby::FeatureFlags::Flags{
      .enable_invoking_legacy_device_discovered_cb = false,
  };
  MediumEnvironment::Instance().SetFeatureFlags(flag);
  api::ble::BleAdvertisementData advertising_data{};
  advertising_data.is_extended_advertisement = false;
  ByteArray encoded_bytes{
      DiscoveredPeripheralTracker::kDummyAdvertisementValue};
  advertising_data.service_data.insert(
      {mediums::bleutils::kCopresenceServiceUuid, encoded_bytes});
  absl::Notification fetch_latch;
  absl::Notification legacy_device_found_latch;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [](BlePeripheral peripheral, const std::string& service_id,
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
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  FindAdvertisement(advertising_data, {}, fetch_latch);

  EXPECT_FALSE(
      legacy_device_found_latch.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 0);
}

TEST_P(DiscoveredPeripheralTrackerTest, FetchGattAdvertisementInThread) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  absl::Notification found_latch;
  absl::Notification fetch_latch;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                if (!found_latch.HasBeenNotified()) {
                  found_latch.Notify();
                }
              },
      },
      {});
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisementWithDelay(advertisement_data, {advertisement_bytes},
                             fetch_latch, kDefaultGattFetchDelay);

  // We should receive a client callback of a peripheral discovery.
  EXPECT_TRUE(fetch_latch.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_TRUE(found_latch.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       IgnoreGattAdvertisementResultWhentrackingStoppedInThread) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_header_bytes = CreateBleAdvertisementHeader(
      GenerateRandomAdvertisementHash(), service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  absl::Notification found_latch;
  absl::Notification fetch_latch;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                if (!found_latch.HasBeenNotified()) {
                  found_latch.Notify();
                }
              },
      },
      {});
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisementWithDelay(advertisement_data, {advertisement_bytes},
                             fetch_latch, kDefaultGattFetchDelay);

  // We should receive a client callback of a peripheral discovery.
  absl::SleepFor(absl::Milliseconds(20));
  discovered_peripheral_tracker_->StopTracking(std::string(kServiceIdA));
  EXPECT_TRUE(fetch_latch.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_FALSE(found_latch.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       FetchMultipleGattAdvertisementResultsInThread) {
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
  absl::Mutex found_mutex;
  int found_count = 0;
  absl::Notification fetch_latch_1;
  absl::Notification fetch_latch_2;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_mutex, &found_count](BlePeripheral peripheral,
                                           const std::string& service_id,
                                           const ByteArray& advertisement_bytes,
                                           bool fast_advertisement) {
                EXPECT_FALSE(fast_advertisement);
                absl::MutexLock lock(found_mutex);
                found_count++;
              },
      },
      {});
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  api::ble::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisementWithDelay(advertisement_data, {advertisement_bytes},
                             fetch_latch_1, kDefaultGattFetchDelay);

  api::ble::BleAdvertisementData advertisement_data_2{};
  if (!advertisement_header_bytes_2.Empty()) {
    advertisement_data_2.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes_2});
  }

  FindAdvertisementWithDelay(advertisement_data_2, {advertisement_bytes_2},
                             fetch_latch_2, kDefaultGattFetchDelay);

  // We should receive a client callback of a peripheral discovery.
  EXPECT_TRUE(fetch_latch_1.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_TRUE(fetch_latch_2.WaitForNotificationWithTimeout(kWaitDuration));
  {
    absl::MutexLock lock(found_mutex);
    EXPECT_TRUE(found_mutex.AwaitWithTimeout(
        absl::Condition(+[](int* c) { return *c >= 2; }, &found_count),
        kWaitDuration));
  }
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 2);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       OnlyGattAdvertisementReceivedOnDeviceWithExtended) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_hash = GenerateRandomAdvertisementHash();
  ByteArray advertisement_header_bytes =
      CreateExtendedBleAdvertisementHeader(advertisement_hash, service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  absl::Notification found_latch;
  absl::Notification fetch_latch_1;
  absl::Notification fetch_latch_2;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                if (!found_latch.HasBeenNotified()) {
                  found_latch.Notify();
                }
              },
      },
      bleutils::kCopresenceServiceUuid);
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  // 1. First receive a GATT advertisement data, it should be skipped.
  api::ble::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch_1);

  // 2. Receive GATT advertisement data again after 4 seconds, it should access
  // GATT server.
  MediumEnvironment::Instance().FastForward(absl::Seconds(4));
  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch_2);

  // We should receive a client callback of a peripheral discovery.
  fetch_latch_2.WaitForNotificationWithTimeout(kWaitDuration);
  EXPECT_TRUE(found_latch.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);
}

TEST_P(DiscoveredPeripheralTrackerTest, SkipExpiredGattAdvertisement) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_hash = GenerateRandomAdvertisementHash();
  ByteArray advertisement_header_bytes =
      CreateExtendedBleAdvertisementHeader(advertisement_hash, service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  absl::Notification found_latch;
  absl::Notification fetch_latch_1;
  absl::Notification fetch_latch_2;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_FALSE(fast_advertisement);
                if (!found_latch.HasBeenNotified()) {
                  found_latch.Notify();
                }
              },
      },
      bleutils::kCopresenceServiceUuid);

  // 1. First receive a GATT advertisement data, it will expire after 15s.
  api::ble::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisementWithDelay(advertisement_data, {advertisement_bytes},
                             fetch_latch_1, kDefaultGattFetchDelay);

  // 2. The GATT advertisement is already queued and will be skipped.
  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch_2);
  MediumEnvironment::Instance().FastForward(absl::Seconds(20));
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  // We should not receive a client callback of a peripheral discovery.
  fetch_latch_1.WaitForNotificationWithTimeout(kWaitDuration);
  EXPECT_FALSE(found_latch.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 0);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       DiscoveredOnceWhenGattAndExtendedAdvertisementReceived) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_hash = GenerateRandomAdvertisementHash();
  ByteArray advertisement_header_bytes =
      CreateExtendedBleAdvertisementHeader(advertisement_hash, service_ids);
  ByteArray advertisement_bytes = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string(kData)),
      ByteArray(std::string(kDeviceToken)));
  absl::Notification found_latch;
  absl::Notification fetch_latch_1;
  absl::Notification fetch_latch_2;
  std::atomic<int> callback_count = 0;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_latch, &callback_count](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                ++callback_count;
                if (!found_latch.HasBeenNotified()) {
                  found_latch.Notify();
                }
              },
      },
      bleutils::kCopresenceServiceUuid);
  MediumEnvironment::Instance().FastForward(absl::Seconds(4));
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  // 1. Received extended advertisement.
  api::ble::BleAdvertisementData extended_advertisement_data{};
  extended_advertisement_data.is_extended_advertisement = true;
  extended_advertisement_data.service_data.insert(
      {bleutils::kCopresenceServiceUuid, advertisement_bytes});

  FindExtendedAdvertisement(extended_advertisement_data, fetch_latch_1);

  // 2. Received GATT advertisement.
  api::ble::BleAdvertisementData advertisement_data{};
  if (!advertisement_header_bytes.Empty()) {
    advertisement_data.service_data.insert(
        {bleutils::kCopresenceServiceUuid, advertisement_header_bytes});
  }

  FindAdvertisement(advertisement_data, {advertisement_bytes}, fetch_latch_2);

  EXPECT_TRUE(fetch_latch_1.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_TRUE(fetch_latch_2.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_TRUE(found_latch.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 1);
  EXPECT_EQ(callback_count, 1);
}

TEST_P(DiscoveredPeripheralTrackerTest,
       FindGattAdvertisementInHigherPriorityThanExtendedGattAdvertisement) {
  std::vector<std::string> service_ids = {std::string(kServiceIdA)};
  ByteArray advertisement_hash_a = GenerateRandomAdvertisementHash();
  ByteArray advertisement_header_a =
      CreateBleAdvertisementHeader(advertisement_hash_a, service_ids);
  ByteArray advertisement_a = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string("advertisement_a")),
      ByteArray(std::string(kDeviceToken)));
  ByteArray advertisement_hash_b = GenerateRandomAdvertisementHash();
  ByteArray advertisement_header_b =
      CreateExtendedBleAdvertisementHeader(advertisement_hash_b, service_ids);
  ByteArray advertisement_b = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string("advertisement_b")),
      ByteArray(std::string(kDeviceToken)));
  ByteArray advertisement_hash_c = GenerateRandomAdvertisementHash();
  ByteArray advertisement_header_c =
      CreateBleAdvertisementHeader(advertisement_hash_c, service_ids);
  ByteArray advertisement_c = CreateBleAdvertisement(
      std::string(kServiceIdA), ByteArray(std::string("advertisement_c")),
      ByteArray(std::string(kDeviceToken)));
  absl::Mutex found_mutex;
  absl::Notification fetch_latch_a;
  absl::Notification fetch_latch_b;
  absl::Notification fetch_latch_c;
  std::vector<std::string> discovered_peripheral;

  discovered_peripheral_tracker_->StartTracking(
      std::string(kServiceIdA), false, Pcp::kP2pPointToPoint,
      {
          .peripheral_discovered_cb =
              [&found_mutex, &discovered_peripheral](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                absl::MutexLock lock(found_mutex);
                discovered_peripheral.push_back(
                    std::string(advertisement_bytes));
              },
      },
      bleutils::kCopresenceServiceUuid);
  MediumEnvironment::Instance().FastForward(absl::Seconds(4));
  discovered_peripheral_tracker_->StartFetchExecutorForTesting();

  // 1. Find peripheral A with GATT advertisement.
  api::ble::BleAdvertisementData advertisement_data_a{};
  advertisement_data_a.service_data.insert(
      {bleutils::kCopresenceServiceUuid, advertisement_header_a});
  FindAdvertisementWithDelay(advertisement_data_a, {advertisement_a},
                             fetch_latch_a, kDefaultGattFetchDelay);

  // 2. Find peripheral B with extended advertisement.
  api::ble::BleAdvertisementData advertisement_data_b{};
  advertisement_data_b.service_data.insert(
      {bleutils::kCopresenceServiceUuid, advertisement_header_b});
  FindAdvertisement(advertisement_data_b, {advertisement_b}, fetch_latch_b);

  // 3. Find peripheral C with GATT advertisement.
  api::ble::BleAdvertisementData advertisement_data_c{};
  advertisement_data_c.service_data.insert(
      {bleutils::kCopresenceServiceUuid, advertisement_header_c});
  FindAdvertisement(advertisement_data_c, {advertisement_c}, fetch_latch_c);

  EXPECT_TRUE(fetch_latch_a.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_TRUE(fetch_latch_b.WaitForNotificationWithTimeout(kWaitDuration));
  EXPECT_TRUE(fetch_latch_c.WaitForNotificationWithTimeout(kWaitDuration));
  {
    absl::MutexLock lock(found_mutex);
    EXPECT_TRUE(found_mutex.AwaitWithTimeout(
        absl::Condition(
            +[](std::vector<std::string>* v) { return v->size() >= 3; },
            &discovered_peripheral),
        kWaitDuration));
    ASSERT_EQ(discovered_peripheral.size(), 3);
    EXPECT_THAT(discovered_peripheral,
                UnorderedElementsAre("advertisement_a", "advertisement_c",
                                     "advertisement_b"));
  }
}

INSTANTIATE_TEST_SUITE_P(
    DiscoveredPeripheralTrackerFlagsTest, DiscoveredPeripheralTrackerTest,
    ::testing::Combine(
        /*is_extended_advertisement_available=*/testing::Bool()));

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
