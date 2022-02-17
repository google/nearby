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

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "connections/implementation/mediums/ble_v2/ble_utils.h"
#include "connections/implementation/mediums/ble_v2/bloom_filter.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

namespace {

constexpr absl::Duration kWaitDuration = absl::Milliseconds(1000);
constexpr absl::string_view kFastAdvertisementServiceUuid =
    "0000FE2C-0000-1000-8000-00805F9B34FB";
constexpr absl::string_view kServiceIdA = "A";
constexpr absl::string_view kMacAddress1 = "4C:8B:1D:CE:BA:D1";
constexpr absl::string_view kData = "\x04\x02\x00";
constexpr absl::string_view kDeviceToken = "\x04\x20";

ByteArray CreateFastBleAdvertisement(const ByteArray& data,
                                     const ByteArray& device_token) {
  return ByteArray(BleAdvertisement(
      BleAdvertisement::Version::kV2, BleAdvertisement::SocketVersion::kV2,
      /*service_id_hash=*/ByteArray{}, data, device_token,
      BleAdvertisementHeader::kDefaultPsmValue));
}

// A stub BlePeripheral implementation.
class BlePeripheralStub : public api::ble_v2::BlePeripheral {
 public:
  explicit BlePeripheralStub(absl::string_view mac_address) {
    mac_address_ = mac_address;
  }

  std::string GetId() const override { return mac_address_; }

 private:
  std::string mac_address_;
};

class DiscoveredPeripheralTrackerTest : public testing::Test {
 public:
  void SetUp() override {}

  BleV2Peripheral CreateBlePeripheral(absl::string_view mac_address) {
    ble_peripheral_ = std::make_unique<BlePeripheralStub>(mac_address);
    return BleV2Peripheral(ble_peripheral_.get());
  }

  // Simulates to see a fast advertisement.
  void FindFastAdvertisement(
      const api::ble_v2::BleAdvertisementData& advertisement_data,
      const std::vector<ByteArray>& advertisement_bytes_list,
      CountDownLatch& fetch_latch) {
    BleV2Peripheral peripheral = CreateBlePeripheral(kMacAddress1);

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
    return {
        .fetch_advertisements =
            [this, &fetch_latch, &advertisement_bytes_list](
                BleV2Peripheral& peripheral, int num_slots,
                AdvertisementReadResult* arr, int psm,
                const std::vector<std::string>& interesting_service_ids)
            -> std::unique_ptr<AdvertisementReadResult> {
          MutexLock lock(&mutex_);
          fetch_count_++;
          auto advertisement_read_result =
              std::make_unique<AdvertisementReadResult>();
          int slot = 0;
          for (const auto& advertisement_bytes : advertisement_bytes_list) {
            advertisement_read_result->AddAdvertisement(slot++,
                                                        advertisement_bytes);
          }
          advertisement_read_result->RecordLastReadStatus(/*isSuccess=*/true);
          fetch_latch.CountDown();
          return advertisement_read_result;
        },
    };
  }

  mutable Mutex mutex_;
  int fetch_count_ ABSL_GUARDED_BY(mutex_) = 0;
  std::unique_ptr<BlePeripheralStub> ble_peripheral_;
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
              [&found_latch](BlePeripheral& peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
      },
      std::string(kFastAdvertisementServiceUuid));

  api::ble_v2::BleAdvertisementData advertisement_data;
  if (!fast_advertisement_bytes.Empty()) {
    advertisement_data.service_uuids.insert(
        std::string(kFastAdvertisementServiceUuid));
    advertisement_data.service_data.insert(
        {std::string(kFastAdvertisementServiceUuid), fast_advertisement_bytes});
  }

  FindFastAdvertisement(advertisement_data, {}, fetch_latch);

  // We should receive a client callback of a peripheral discovery without a
  // GATT read.
  fetch_latch.Await(kWaitDuration);
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 0);
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
                  BlePeripheral& peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                callback_times++;
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
      },
      std::string(kFastAdvertisementServiceUuid));

  api::ble_v2::BleAdvertisementData advertisement_data;
  if (!fast_advertisement_bytes.Empty()) {
    advertisement_data.service_uuids.insert(
        std::string(kFastAdvertisementServiceUuid));
    advertisement_data.service_data.insert(
        {std::string(kFastAdvertisementServiceUuid), fast_advertisement_bytes});
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
              [&found_latch](BlePeripheral& peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_EQ(advertisement_bytes, ByteArray(std::string(kData)));
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
      },
      "0000FE2C-0000-1000-8000-00805F9B34FC");

  api::ble_v2::BleAdvertisementData advertisement_data;
  if (!fast_advertisement_bytes.Empty()) {
    advertisement_data.service_uuids.insert(
        std::string(kFastAdvertisementServiceUuid));
    advertisement_data.service_data.insert(
        {std::string(kFastAdvertisementServiceUuid), fast_advertisement_bytes});
  }

  FindFastAdvertisement(advertisement_data, {}, fetch_latch);

  // We should get no new discoveries.
  fetch_latch.Await(kWaitDuration);
  EXPECT_FALSE(found_latch.Await(kWaitDuration).result());
  EXPECT_EQ(GetFetchAdvertisementCallbackCount(), 0);
}

}  // namespace

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
