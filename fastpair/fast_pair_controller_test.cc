// Copyright 2023 Google LLC
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

#include "fastpair/fast_pair_controller.h"

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/escaping.h"
#include "fastpair/message_stream/fake_provider.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

namespace {

class MediumEnvironmentStarter {
 public:
  MediumEnvironmentStarter() { MediumEnvironment::Instance().Start(); }
  ~MediumEnvironmentStarter() { MediumEnvironment::Instance().Stop(); }
};

class FastPairControllerTest : public testing::Test {
 protected:
  void SetUp() override {
    BluetoothClassicMedium& seeker_medium =
        mediums_.GetBluetoothClassic().GetMedium();
    provider_.DiscoverProvider(seeker_medium);
    std::string address = provider_.GetMacAddress();
    NEARBY_LOGS(INFO) << "Provider address: " << address;
    remote_device_ = seeker_medium.GetRemoteDevice(provider_.GetMacAddress());
    ASSERT_TRUE(remote_device_.IsValid());
    fast_pair_device_ =
        std::make_unique<FastPairDevice>(Protocol::kFastPairRetroactivePairing);
    fast_pair_device_->SetPublicAddress(remote_device_.GetMacAddress());
  }

  void TearDown() override {
    executor_.Shutdown();
    provider_.Shutdown();
    MediumEnvironment::Instance().Stop();
  }

  // The medium environment must be initialized (started) before adding
  // adapters.
  MediumEnvironmentStarter env_;
  SingleThreadExecutor executor_;
  Mediums mediums_;
  FakeProvider provider_;
  BluetoothDevice remote_device_;
  std::unique_ptr<FastPairDevice> fast_pair_device_;
};

TEST_F(FastPairControllerTest, Constructor) {
  FastPairController controller(&mediums_, &*fast_pair_device_, &executor_);
}

TEST_F(FastPairControllerTest, OpenMessageStream) {
  FastPairController controller(&mediums_, &*fast_pair_device_, &executor_);

  EXPECT_OK(controller.OpenMessageStream());
}

}  // namespace

}  // namespace fastpair
}  // namespace nearby
