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

#include "fastpair/scanning/scanner_broker_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/account_key.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/protocol.h"
#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"
#include "fastpair/repository/fake_fast_pair_repository.h"
#include "fastpair/repository/fast_pair_device_repository.h"
#include "fastpair/scanning/scanner_broker.h"
#include "fastpair/testing/fast_pair_service_data_creator.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {
namespace {
constexpr int kNotDiscoverableAdvHeader = 0b00000110;
constexpr int kAccountKeyFilterHeader = 0b01100000;
constexpr int kSaltHeader = 0b00010001;
constexpr absl::string_view kAccountKeyFilter("112233445566");
constexpr absl::string_view kSalt("01");
constexpr absl::string_view kServiceID{"Fast Pair"};
constexpr absl::string_view kModelId{"718c17"};
constexpr absl::string_view kFastPairServiceUuid{
    "0000FE2C-0000-1000-8000-00805F9B34FB"};
constexpr absl::string_view kPublicAntiSpoof =
    "Wuyr48lD3txnUhGiMF1IfzlTwRxxe+wMB1HLzP+"
    "0wVcljfT3XPoiy1fntlneziyLD5knDVAJSE+RM/zlPRP/Jg==";
class ScannerBrokerObserver : public ScannerBroker::Observer {
 public:
  explicit ScannerBrokerObserver(ScannerBroker* scanner_broker,
                                 CountDownLatch* accept_latch,
                                 CountDownLatch* lost_latch) {
    accept_latch_ = accept_latch;
    lost_latch_ = lost_latch;
    scanner_broker->AddObserver(this);
  }

  void OnDeviceFound(FastPairDevice& device) override {
    accept_latch_->CountDown();
  }

  void OnDeviceLost(FastPairDevice& device) override {
    lost_latch_->CountDown();
  }

  CountDownLatch* accept_latch_ = nullptr;
  CountDownLatch* lost_latch_ = nullptr;
};

class MediumEnvironmentStarter {
 public:
  MediumEnvironmentStarter() { MediumEnvironment::Instance().Start(); }
  ~MediumEnvironmentStarter() { MediumEnvironment::Instance().Stop(); }
};

class ScannerBrokerImplTest : public testing::Test {
 protected:
  void SetUp() override {
    MediumEnvironment::Instance().Start();
    advertiser_ble_address_ =
        mediums_advertiser_.GetBle().GetMedium().GetAdapter().GetMacAddress();
  }

  void TearDown() override { MediumEnvironment::Instance().Stop(); }

  // The medium environment must be initialized (started)
  // before registering medium.
  MediumEnvironmentStarter env_;
  Mediums mediums_scanner_;
  Mediums mediums_advertiser_;
  std::string advertiser_ble_address_;
};

TEST_F(ScannerBrokerImplTest, FoundDiscoverableAdvertisement) {
  SingleThreadExecutor executor;
  FastPairDeviceRepository devices{&executor};

  // Setup FakeFastPairRepository
  std::string decoded_key;
  absl::Base64Unescape(kPublicAntiSpoof, &decoded_key);
  proto::Device metadata;
  auto repository_ = std::make_unique<FakeFastPairRepository>();
  metadata.mutable_anti_spoofing_key_pair()->set_public_key(decoded_key);
  repository_->SetFakeMetadata(kModelId, metadata);

  // Create Scanner and ScannerBrokerObserver
  auto scanner_broker = std::make_unique<ScannerBrokerImpl>(
      mediums_scanner_, &executor, &devices);
  CountDownLatch accept_latch(1);
  CountDownLatch lost_latch(1);
  CountDownLatch device_removed(1);
  FastPairDeviceRepository::RemoveDeviceCallback callback =
      [&](const FastPairDevice& device) {
        EXPECT_EQ(device.GetBleAddress(), advertiser_ble_address_);
        device_removed.CountDown();
      };
  devices.AddObserver(&callback);
  ScannerBrokerObserver observer(scanner_broker.get(), &accept_latch,
                                 &lost_latch);

  // Create Advertiser and startAdvertising
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{absl::HexStringToBytes(kModelId)};
  std::string fast_pair_service_uuid(kFastPairServiceUuid);
  mediums_advertiser_.GetBle().GetMedium().StartAdvertising(
      service_id, advertisement_bytes, fast_pair_service_uuid);

  // Fast Pair scanner startScanning
  auto scanning_session =
      scanner_broker->StartScanning(Protocol::kFastPairInitialPairing);

  // Notify device found
  accept_latch.Await();

  // Advertiser stopAdvertising
  mediums_advertiser_.GetBle().GetMedium().StopAdvertising(service_id);

  // Notify device lost
  lost_latch.Await();
  device_removed.Await();
  scanning_session.reset();
}

TEST_F(ScannerBrokerImplTest, DISABLED_FoundNonDiscoverableAdvertisement) {
  SingleThreadExecutor executor;
  FastPairDeviceRepository devices{&executor};

  // Setup FakeFastPairRepository
  auto repository = std::make_unique<FakeFastPairRepository>();
  proto::Device metadata;
  repository->SetFakeMetadata(kModelId, metadata);
  repository->SetResultOfCheckIfAssociatedWithCurrentAccount(AccountKey(),
                                                             kModelId);

  // Create Scanner and ScannerBrokerObserver
  auto scanner_broker = std::make_unique<ScannerBrokerImpl>(
      mediums_scanner_, &executor, &devices);
  CountDownLatch accept_latch(1);
  CountDownLatch lost_latch(1);
  CountDownLatch device_removed(1);
  FastPairDeviceRepository::RemoveDeviceCallback callback =
      [&](const FastPairDevice& device) {
        EXPECT_EQ(device.GetBleAddress(), advertiser_ble_address_);
        device_removed.CountDown();
      };
  devices.AddObserver(&callback);
  ScannerBrokerObserver observer(scanner_broker.get(), &accept_latch,
                                 &lost_latch);

  // Create Advertiser and startAdvertising
  std::string service_id(kServiceID);
  std::vector<uint8_t> service_data =
      FastPairServiceDataCreator::Builder()
          .SetHeader(kNotDiscoverableAdvHeader)
          .SetModelId(kModelId)
          .AddExtraFieldHeader(kAccountKeyFilterHeader)
          .AddExtraField(kAccountKeyFilter)
          .AddExtraFieldHeader(kSaltHeader)
          .AddExtraField(kSalt)
          .Build()
          ->CreateServiceData();
  ByteArray advertisement_bytes(
      std::string(service_data.begin(), service_data.end()));
  std::string fast_pair_service_uuid(kFastPairServiceUuid);
  mediums_advertiser_.GetBle().GetMedium().StartAdvertising(
      service_id, advertisement_bytes, fast_pair_service_uuid);

  // Fast Pair scanner startScanning
  auto scanning_session =
      scanner_broker->StartScanning(Protocol::kFastPairInitialPairing);

  // Notify device found
  accept_latch.Await();

  // Advertiser stopAdvertising
  mediums_advertiser_.GetBle().GetMedium().StopAdvertising(service_id);

  // Notify device lost
  lost_latch.Await();
  device_removed.Await();
  scanning_session.reset();
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
