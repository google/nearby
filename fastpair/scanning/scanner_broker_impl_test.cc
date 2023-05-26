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

#include "gtest/gtest.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/protocol.h"
#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"
#include "fastpair/scanning/scanner_broker.h"
#include "fastpair/server_access/fake_fast_pair_repository.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace fastpair {
namespace {

constexpr absl::Duration kTaskWaitTimeout = absl::Milliseconds(1000);
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

class ScannerBrokerImplTest : public testing::Test {
 protected:
  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_F(ScannerBrokerImplTest, CanStartScanning) {
  env_.Start();
  // Setup FakeFastPairRepository
  std::string decoded_key;
  absl::Base64Unescape(kPublicAntiSpoof, &decoded_key);
  SingleThreadExecutor executor;
  FastPairDeviceRepository devices(&executor);
  proto::Device metadata;
  auto repository_ = std::make_unique<FakeFastPairRepository>();
  metadata.mutable_anti_spoofing_key_pair()->set_public_key(decoded_key);
  repository_->SetFakeMetadata(kModelId, metadata);

  // Create Fast Pair Scanner and add its observer
  Mediums mediums_1;
  auto scanner_broker =
      std::make_unique<ScannerBrokerImpl>(mediums_1, &executor, &devices);
  CountDownLatch accept_latch(1);
  CountDownLatch lost_latch(1);
  ScannerBrokerObserver observer(scanner_broker.get(), &accept_latch,
                                 &lost_latch);

  // Create Advertiser and startAdvertising
  Mediums mediums_2;
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{absl::HexStringToBytes(kModelId)};
  std::string fast_pair_service_uuid(kFastPairServiceUuid);
  mediums_2.GetBle().GetMedium().StartAdvertising(
      service_id, advertisement_bytes, fast_pair_service_uuid);

  // Fast Pair scanner startScanning
  auto scanning_session =
      scanner_broker->StartScanning(Protocol::kFastPairInitialPairing);

  // Notify device found
  EXPECT_TRUE(accept_latch.Await(kTaskWaitTimeout).result());

  // Advertiser stopAdvertising
  mediums_2.GetBle().GetMedium().StopAdvertising(service_id);

  // Notify device lost
  EXPECT_TRUE(lost_latch.Await(kTaskWaitTimeout).result());
  scanning_session.reset();
  env_.Stop();
}
}  // namespace
}  // namespace fastpair
}  // namespace nearby
