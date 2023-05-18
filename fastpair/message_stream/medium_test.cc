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

#include "fastpair/message_stream/medium.h"

#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "testing/fuzzing/fuzztest.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fastpair/common/constant.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/message_stream/fake_medium_observer.h"
#include "fastpair/message_stream/fake_provider.h"
#include "fastpair/message_stream/message.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

namespace {

using ::testing::status::StatusIs;

class MediumEnvironmentStarter {
 public:
  MediumEnvironmentStarter() { MediumEnvironment::Instance().Start(); }
  ~MediumEnvironmentStarter() { MediumEnvironment::Instance().Stop(); }
};

class MediumTest : public testing::Test {
 protected:
  void SetUp() override { MediumEnvironment::Instance().Start(); }
  void TearDown() override {
    provider_.Shutdown();
    MediumEnvironment::Instance().Stop();
  }

  // The medium environment must be initialized (started) before adding
  // adapters.
  MediumEnvironmentStarter env_;
  BluetoothAdapter seeker_adapter_;
  BluetoothClassicMedium seeker_medium_{seeker_adapter_};
  FakeProvider provider_;
  FakeMediumObserver observer_;
};

TEST_F(MediumTest, ConnectWithNonExistingDeviceFails) {
  FastPairDevice fp_device("model id", "ble address",
                           Protocol::kFastPairRetroactivePairing);
  fp_device.SetPublicAddress("11:22:33:44:55:66");
  Medium medium =
      Medium(fp_device, std::optional<BluetoothClassicMedium*>(&seeker_medium_),
             observer_);

  ASSERT_OK(medium.OpenRfcomm());
  ASSERT_TRUE(observer_.connection_result_.Get().ok());
  EXPECT_THAT(observer_.connection_result_.Get().GetResult(),
              StatusIs(absl::StatusCode::kUnavailable));
}

TEST_F(MediumTest, Connect) {
  FastPairDevice fp_device("model id", "ble address",
                           Protocol::kFastPairRetroactivePairing);
  fp_device.SetPublicAddress(provider_.GetMacAddress());
  provider_.DiscoverProvider(seeker_medium_);
  provider_.EnableProviderRfcomm();
  Medium medium =
      Medium(fp_device, std::optional<BluetoothClassicMedium*>(&seeker_medium_),
             observer_);

  ASSERT_OK(medium.OpenRfcomm());
  ASSERT_TRUE(observer_.connection_result_.Get().ok());
  EXPECT_OK(observer_.connection_result_.Get().GetResult());
}

TEST_F(MediumTest, ProviderDisconnectsCallsOnDisconnectCallback) {
  FastPairDevice fp_device("model id", "ble address",
                           Protocol::kFastPairRetroactivePairing);
  fp_device.SetPublicAddress(provider_.GetMacAddress());
  provider_.DiscoverProvider(seeker_medium_);
  provider_.EnableProviderRfcomm();
  Medium medium =
      Medium(fp_device, std::optional<BluetoothClassicMedium*>(&seeker_medium_),
             observer_);
  ASSERT_OK(medium.OpenRfcomm());
  ASSERT_TRUE(observer_.connection_result_.Get().ok());

  provider_.DisableProviderRfcomm();

  ASSERT_TRUE(observer_.disconnected_reason_.Get().ok());
  EXPECT_THAT(observer_.disconnected_reason_.Get().GetResult(),
              StatusIs(absl::StatusCode::kDataLoss));
}

TEST_F(MediumTest, DisconnectSendFails) {
  FastPairDevice fp_device("model id", "ble address",
                           Protocol::kFastPairRetroactivePairing);
  fp_device.SetPublicAddress(provider_.GetMacAddress());
  provider_.DiscoverProvider(seeker_medium_);
  provider_.EnableProviderRfcomm();
  Medium medium =
      Medium(fp_device, std::optional<BluetoothClassicMedium*>(&seeker_medium_),
             observer_);
  ASSERT_OK(medium.OpenRfcomm());
  ASSERT_TRUE(observer_.connection_result_.Get().ok());

  ASSERT_OK(medium.Disconnect());

  // Medium is disconnected, Send should fail
  EXPECT_THAT(medium.Send(Message{}, false),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(MediumTest, SendMessage) {
  Message message = {.message_group = MessageGroup::kDeviceInformationEvent,
                     .message_code = MessageCode::kSessionNonce,
                     .payload = absl::HexStringToBytes("ABCDEF")};
  // See the format definition in
  // https://developers.google.com/nearby/fast-pair/specifications/extensions/messagestreamhttps://developers.google.com/nearby/fast-pair/specifications/extensions/messagestream
  std::string expected_result = absl::HexStringToBytes("030A0003ABCDEF");
  FastPairDevice fp_device("model id", "ble address",
                           Protocol::kFastPairRetroactivePairing);
  fp_device.SetPublicAddress(provider_.GetMacAddress());
  provider_.DiscoverProvider(seeker_medium_);
  provider_.EnableProviderRfcomm();
  Medium medium =
      Medium(fp_device, std::optional<BluetoothClassicMedium*>(&seeker_medium_),
             observer_);
  ASSERT_OK(medium.OpenRfcomm());
  ASSERT_TRUE(observer_.connection_result_.Get().ok());

  EXPECT_OK(medium.Send(message, false));

  Future<std::string> result =
      provider_.ReadProviderBytes(expected_result.size());
  ASSERT_TRUE(result.Get().ok());
  EXPECT_EQ(result.Get().GetResult(), expected_result);
}

TEST_F(MediumTest, ReceiveMessage) {
  Message expected_message = {
      .message_group = MessageGroup::kDeviceInformationEvent,
      .message_code = MessageCode::kSessionNonce,
      .payload = absl::HexStringToBytes("ABCDEF")};
  std::string input = absl::HexStringToBytes("030A0003ABCDEF");
  FastPairDevice fp_device("model id", "ble address",
                           Protocol::kFastPairRetroactivePairing);
  fp_device.SetPublicAddress(provider_.GetMacAddress());
  provider_.DiscoverProvider(seeker_medium_);
  provider_.EnableProviderRfcomm();
  Medium medium =
      Medium(fp_device, std::optional<BluetoothClassicMedium*>(&seeker_medium_),
             observer_);
  ASSERT_OK(medium.OpenRfcomm());
  ASSERT_TRUE(observer_.connection_result_.Get().ok());

  provider_.WriteProviderBytes(input);

  ASSERT_OK(observer_.WaitForMessages(1, absl::Seconds(10)));
  std::vector<Message> messages = observer_.GetMessages();
  ASSERT_EQ(messages.size(), 1);
  EXPECT_EQ(messages[0], expected_message);
}

class MediumFuzzTest : public fuzztest::PerIterationFixtureAdapter<MediumTest> {
 public:
  void HandlesAnyInput(absl::string_view input) {
    FastPairDevice fp_device("model id", "ble address",
                             Protocol::kFastPairRetroactivePairing);
    fp_device.SetPublicAddress(provider_.GetMacAddress());
    provider_.DiscoverProvider(seeker_medium_);
    provider_.EnableProviderRfcomm();
    Medium medium = Medium(
        fp_device, std::optional<BluetoothClassicMedium*>(&seeker_medium_),
        observer_);
    ASSERT_OK(medium.OpenRfcomm());
    ASSERT_TRUE(observer_.connection_result_.Get().ok());

    provider_.WriteProviderBytes(std::string(input));
    provider_.DisableProviderRfcomm();
  }

  void HandlesValidInput(uint8_t group, uint8_t code,
                         absl::string_view payload) {
    Message expected_message = {
        .message_group = static_cast<MessageGroup>(group),
        .message_code = static_cast<MessageCode>(code),
        .payload = std::string(payload)};
    FastPairDevice fp_device("model id", "ble address",
                             Protocol::kFastPairRetroactivePairing);
    fp_device.SetPublicAddress(provider_.GetMacAddress());
    provider_.DiscoverProvider(seeker_medium_);
    provider_.EnableProviderRfcomm();
    Medium medium = Medium(
        fp_device, std::optional<BluetoothClassicMedium*>(&seeker_medium_),
        observer_);
    ASSERT_OK(medium.OpenRfcomm());
    ASSERT_TRUE(observer_.connection_result_.Get().ok());

    provider_.WriteProviderBytes(
        {static_cast<char>(group), static_cast<char>(code)});
    uint16_t length = payload.length();
    provider_.WriteProviderBytes(
        {static_cast<char>(length >> 8), static_cast<char>(length)});
    provider_.WriteProviderBytes(std::string(payload));
    ASSERT_OK(observer_.WaitForMessages(1, absl::Seconds(10)));
    std::vector<Message> messages = observer_.GetMessages();
    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0], expected_message);
    provider_.DisableProviderRfcomm();
  }
};

FUZZ_TEST_F(MediumFuzzTest, HandlesAnyInput)
    .WithDomains(fuzztest::Arbitrary<std::string>());

FUZZ_TEST_F(MediumFuzzTest, HandlesValidInput)
    .WithDomains(/*group=*/fuzztest::Arbitrary<uint8_t>(),
                 /*code=*/fuzztest::Arbitrary<uint8_t>(),
                 /*payload=*/fuzztest::Arbitrary<std::string>());

}  // namespace
}  // namespace fastpair
}  // namespace nearby
