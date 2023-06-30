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

#include "fastpair/message_stream/message_stream.h"

#include <memory>
#include <optional>
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
#include "internal/platform/implementation/device_info.h"
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

class FakeObserver : public MessageStream::Observer {
 public:
  void OnConnectionResult(absl::Status result) override {
    NEARBY_LOGS(INFO) << "OnConnectionResult " << result;
    connection_result_.Set(result);
  }

  void OnDisconnected(absl::Status status) override {
    NEARBY_LOGS(INFO) << "OnDisconnected " << status;
    disconnected_reason_.Set(status);
  }

  void OnEnableSilenceMode(bool enable) override { silence_mode_.Set(enable); }

  void OnLogBufferFull() override { log_buffer_full_.Set(true); }

  void OnModelId(absl::string_view model_id) override {
    model_id_.Set(std::string(model_id));
  }

  void OnBleAddressUpdated(absl::string_view address) override {
    ble_address_updated_.Set(std::string(address));
  }

  void OnBatteryUpdated(
      std::vector<MessageStream::BatteryInfo> battery_levels) override {
    battery_levels_.Set(battery_levels);
  }

  void OnRemainingBatteryTime(absl::Duration duration) override {
    remaining_battery_time_.Set(duration);
  }

  bool OnRing(uint8_t components, absl::Duration duration) override {
    on_ring_event_.Set({components, duration});
    // This allows us to test returning ACK/NACK to the seeker.
    return components != 0xAB;
  }
  Future<absl::Status> connection_result_;
  Future<absl::Status> disconnected_reason_;
  Future<std::string> model_id_;
  Future<std::string> ble_address_updated_;
  Future<std::vector<MessageStream::BatteryInfo>> battery_levels_;
  Future<absl::Duration> remaining_battery_time_;
  Future<bool> silence_mode_;
  Future<bool> log_buffer_full_;
  struct OnRingData {
    uint8_t components;
    absl::Duration duration;
  };
  Future<OnRingData> on_ring_event_;
};

class MessageStreamTest : public testing::Test {
 protected:
  void SetUp() override {
    MediumEnvironment::Instance().Start();

    fp_device_.SetPublicAddress(provider_.GetMacAddress());
    provider_.DiscoverProvider(seeker_medium_);
    provider_.EnableProviderRfcomm();
  }
  void TearDown() override {
    provider_.Shutdown();
    MediumEnvironment::Instance().Stop();
  }

  MessageStream OpenMessageStream() {
    MessageStream message_stream = MessageStream(
        fp_device_, std::optional<BluetoothClassicMedium*>(&seeker_medium_),
        observer_);
    CHECK_OK(message_stream.OpenRfcomm());
    CHECK(observer_.connection_result_.Get().ok());
    CHECK(observer_.connection_result_.Get().GetResult().ok());
    return message_stream;
  }

  void VerifySentMessage(absl::string_view bytes) {
    Future<std::string> result = provider_.ReadProviderBytes(bytes.size());
    ASSERT_TRUE(result.Get().ok());
    ASSERT_EQ(result.Get().GetResult(), bytes);
  }
  // The medium environment must be initialized (started) before adding
  // adapters.
  MediumEnvironmentStarter env_;
  BluetoothAdapter seeker_adapter_;
  BluetoothClassicMedium seeker_medium_{seeker_adapter_};
  FakeProvider provider_;
  FastPairDevice fp_device_{"model id", "ble address",
                            Protocol::kFastPairRetroactivePairing};

  FakeObserver observer_;
};

TEST_F(MessageStreamTest, ConnectRfcomm) {
  MessageStream message_stream = MessageStream(
      fp_device_, std::optional<BluetoothClassicMedium*>(&seeker_medium_),
      observer_);

  ASSERT_OK(message_stream.OpenRfcomm());

  ASSERT_TRUE(observer_.connection_result_.Get().ok());
  EXPECT_OK(observer_.connection_result_.Get().GetResult());
}

TEST_F(MessageStreamTest, Disconnect) {
  MessageStream message_stream = OpenMessageStream();

  ASSERT_OK(message_stream.Disconnect());

  ASSERT_THAT(message_stream.SendCapabilities(/*companion_app_installed=*/true,
                                              /*supports_silence_mode=*/false),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(MessageStreamTest, ProviderDisconnectCallsOnDisconnectCallback) {
  MessageStream message_stream = OpenMessageStream();

  provider_.DisableProviderRfcomm();

  ASSERT_TRUE(observer_.disconnected_reason_.Get().ok());
  EXPECT_THAT(observer_.disconnected_reason_.Get().GetResult(),
              StatusIs(absl::StatusCode::kDataLoss));
}

TEST_F(MessageStreamTest, SendCapabilites) {
  MessageStream message_stream = OpenMessageStream();

  ASSERT_OK(message_stream.SendCapabilities(/*companion_app_installed=*/true,
                                            /*supports_silence_mode=*/false));
  VerifySentMessage(absl::HexStringToBytes("0307000102"));

  ASSERT_OK(message_stream.SendCapabilities(/*companion_app_installed=*/false,
                                            /*supports_silence_mode=*/true));
  VerifySentMessage(absl::HexStringToBytes("0307000101"));

  ASSERT_OK(message_stream.SendCapabilities(/*companion_app_installed=*/true,
                                            /*supports_silence_mode=*/true));
  VerifySentMessage(absl::HexStringToBytes("0307000103"));

  ASSERT_OK(message_stream.SendCapabilities(/*companion_app_installed=*/false,
                                            /*supports_silence_mode=*/false));
  VerifySentMessage(absl::HexStringToBytes("0307000100"));
}

TEST_F(MessageStreamTest, RingAcked) {
  constexpr uint8_t kComponents = 0x50;
  MessageStream message_stream = OpenMessageStream();

  Future<bool> result = message_stream.Ring(kComponents, absl::Minutes(10));

  VerifySentMessage(absl::HexStringToBytes("04010002500A"));
  provider_.WriteProviderBytes(absl::HexStringToBytes("FF0100020401"));
  ASSERT_TRUE(result.Get().ok());
  ASSERT_TRUE(result.Get().GetResult());
}

TEST_F(MessageStreamTest, RingNacked) {
  constexpr uint8_t kComponents = 0x50;
  MessageStream message_stream = OpenMessageStream();

  Future<bool> result = message_stream.Ring(kComponents, absl::Minutes(10));

  VerifySentMessage(absl::HexStringToBytes("04010002500A"));
  provider_.WriteProviderBytes(absl::HexStringToBytes("FF0200020401"));
  ASSERT_FALSE(result.Get().GetResult());
}

TEST_F(MessageStreamTest, RingNoResponse) {
  constexpr uint8_t kComponents = 0x50;
  MessageStream message_stream = OpenMessageStream();

  Future<bool> result = message_stream.Ring(kComponents, absl::Minutes(10));

  VerifySentMessage(absl::HexStringToBytes("04010002500A"));
  ASSERT_FALSE(result.Get().ok());
  EXPECT_EQ(result.Get().exception(), Exception::kTimeout);
}

TEST_F(MessageStreamTest, GetActiveComponents) {
  MessageStream message_stream = OpenMessageStream();

  Future<uint8_t> result = message_stream.GetActiveComponents();

  VerifySentMessage(absl::HexStringToBytes("0305"));
  provider_.WriteProviderBytes(absl::HexStringToBytes("03060001AB"));
  ASSERT_TRUE(result.Get().ok());
  ASSERT_EQ(result.Get().GetResult(), 0xAB);
}

TEST_F(MessageStreamTest, GetActiveComponentsEmptyResponse) {
  MessageStream message_stream = OpenMessageStream();

  Future<uint8_t> result = message_stream.GetActiveComponents();

  VerifySentMessage(absl::HexStringToBytes("0305"));
  provider_.WriteProviderBytes(absl::HexStringToBytes("03060000"));
  ASSERT_TRUE(result.Get().ok());
  ASSERT_EQ(result.Get().GetResult(), 0);
}

TEST_F(MessageStreamTest, GetActiveComponentsNoResponse) {
  MessageStream message_stream = OpenMessageStream();

  Future<uint8_t> result = message_stream.GetActiveComponents();

  VerifySentMessage(absl::HexStringToBytes("0305"));
  ASSERT_FALSE(result.Get().ok());
  EXPECT_EQ(result.Get().exception(), Exception::kTimeout);
}

TEST_F(MessageStreamTest, ReceiveModelId) {
  MessageStream message_stream = OpenMessageStream();

  provider_.WriteProviderBytes(absl::HexStringToBytes("03010003ABCDEF"));

  ASSERT_TRUE(observer_.model_id_.Get().ok());
  ASSERT_EQ(observer_.model_id_.Get().GetResult(),
            absl::HexStringToBytes("ABCDEF"));
}

TEST_F(MessageStreamTest, ReceiveEnableSilenceMode) {
  MessageStream message_stream = OpenMessageStream();

  provider_.WriteProviderBytes(absl::HexStringToBytes("01010000"));

  ASSERT_TRUE(observer_.silence_mode_.Get().ok());
  ASSERT_TRUE(observer_.silence_mode_.Get().GetResult());
}

TEST_F(MessageStreamTest, ReceiveDisableSilenceMode) {
  MessageStream message_stream = OpenMessageStream();

  provider_.WriteProviderBytes(absl::HexStringToBytes("01020000"));

  EXPECT_FALSE(observer_.silence_mode_.Get());
}

TEST_F(MessageStreamTest, ReceiveLogBufferFull) {
  MessageStream message_stream = OpenMessageStream();

  provider_.WriteProviderBytes(absl::HexStringToBytes("02010000"));

  ASSERT_TRUE(observer_.log_buffer_full_.Get().ok());
  ASSERT_TRUE(observer_.log_buffer_full_.Get().GetResult());
}

TEST_F(MessageStreamTest, ReceiveOnRingEmpty) {
  MessageStream message_stream = OpenMessageStream();

  provider_.WriteProviderBytes(absl::HexStringToBytes("04010000"));

  ASSERT_TRUE(observer_.on_ring_event_.Get().ok());
  ASSERT_EQ(observer_.on_ring_event_.Get().GetResult().components, 0);
  ASSERT_EQ(observer_.on_ring_event_.Get().GetResult().duration,
            absl::ZeroDuration());
  VerifySentMessage(absl::HexStringToBytes("FF0100020401"));
}

TEST_F(MessageStreamTest, ReceiveOnRingWithComponents) {
  MessageStream message_stream = OpenMessageStream();

  provider_.WriteProviderBytes(absl::HexStringToBytes("04010001CD"));

  ASSERT_TRUE(observer_.on_ring_event_.Get().ok());
  ASSERT_EQ(observer_.on_ring_event_.Get().GetResult().components, 0xCD);
  ASSERT_EQ(observer_.on_ring_event_.Get().GetResult().duration,
            absl::ZeroDuration());
  VerifySentMessage(absl::HexStringToBytes("FF0100020401"));
}

TEST_F(MessageStreamTest, ReceiveOnRingWithTimeout) {
  MessageStream message_stream = OpenMessageStream();

  provider_.WriteProviderBytes(absl::HexStringToBytes("04010002CDFE"));

  ASSERT_TRUE(observer_.on_ring_event_.Get().ok());
  ASSERT_EQ(observer_.on_ring_event_.Get().GetResult().components, 0xCD);
  ASSERT_EQ(observer_.on_ring_event_.Get().GetResult().duration,
            absl::Minutes(0xFE));
  VerifySentMessage(absl::HexStringToBytes("FF0100020401"));
}

TEST_F(MessageStreamTest, OnRingFailSendsNack) {
  MessageStream message_stream = OpenMessageStream();

  provider_.WriteProviderBytes(absl::HexStringToBytes("04010002ABFE"));

  ASSERT_TRUE(observer_.on_ring_event_.Get().ok());
  ASSERT_EQ(observer_.on_ring_event_.Get().GetResult().components, 0xAB);
  ASSERT_EQ(observer_.on_ring_event_.Get().GetResult().duration,
            absl::Minutes(0xFE));
  VerifySentMessage(absl::HexStringToBytes("FF0200020401"));
}

TEST_F(MessageStreamTest, SendPlatformType) {
  MessageStream message_stream = OpenMessageStream();

  ASSERT_OK(
      message_stream.SendPlatformType(api::DeviceInfo::OsType::kAndroid, 0x1C));
  VerifySentMessage(absl::HexStringToBytes("03080002011C"));
}

TEST_F(MessageStreamTest, ReceiveBleAddressUpdated) {
  MessageStream message_stream = OpenMessageStream();

  provider_.WriteProviderBytes(absl::HexStringToBytes("03020006AABBCCDDEEFF"));

  ASSERT_TRUE(observer_.ble_address_updated_.Get().ok());
  EXPECT_EQ(observer_.ble_address_updated_.Get().GetResult(),
            "AA:BB:CC:DD:EE:FF");
}

TEST_F(MessageStreamTest, ReceiveBatteryUpdated) {
  MessageStream message_stream = OpenMessageStream();

  // The values copied from the specification.
  provider_.WriteProviderBytes(absl::HexStringToBytes("0303000357417F"));

  ASSERT_TRUE(observer_.battery_levels_.Get().ok());
  ASSERT_EQ(observer_.battery_levels_.Get().result().size(), 3);
  EXPECT_FALSE(observer_.battery_levels_.Get().result()[0].is_charging);
  EXPECT_EQ(observer_.battery_levels_.Get().result()[0].level, 87);
  EXPECT_FALSE(observer_.battery_levels_.Get().result()[1].is_charging);
  EXPECT_EQ(observer_.battery_levels_.Get().result()[1].level, 65);
  EXPECT_FALSE(observer_.battery_levels_.Get().result()[2].is_charging);
  EXPECT_FALSE(observer_.battery_levels_.Get().result()[2].level.has_value());
}

TEST_F(MessageStreamTest, ReceiveBatteryUpdatedOneBattery) {
  MessageStream message_stream = OpenMessageStream();

  provider_.WriteProviderBytes(absl::HexStringToBytes("030300019A"));

  ASSERT_TRUE(observer_.battery_levels_.Get().ok());
  ASSERT_EQ(observer_.battery_levels_.Get().result().size(), 1);
  EXPECT_TRUE(observer_.battery_levels_.Get().result()[0].is_charging);
  EXPECT_EQ(observer_.battery_levels_.Get().result()[0].level, 0x1A);
}

TEST_F(MessageStreamTest, ReceiveRemainingBatteryTime) {
  MessageStream message_stream = OpenMessageStream();

  // The values copied from the specification
  provider_.WriteProviderBytes(absl::HexStringToBytes("03040001F0"));

  ASSERT_TRUE(observer_.remaining_battery_time_.Get().ok());
  EXPECT_EQ(observer_.remaining_battery_time_.Get().GetResult(),
            absl::Minutes(240));
}

TEST_F(MessageStreamTest, ReceiveRemainingBatteryTimeHighValue) {
  MessageStream message_stream = OpenMessageStream();

  provider_.WriteProviderBytes(absl::HexStringToBytes("03040002ABCD"));

  ASSERT_TRUE(observer_.remaining_battery_time_.Get().ok());
  EXPECT_EQ(observer_.remaining_battery_time_.Get().GetResult(),
            absl::Minutes(0xABCD));
}

template <typename T>
std::vector<T> GetAllEnums(int min_value, int max_value) {
  int count = max_value - min_value + 1;
  std::vector<T> enums(count);
  for (int i = 0; i < count; i++) {
    enums[i] = static_cast<T>(min_value + i);
  }
  return enums;
}

// MessageGroup received over the wire is an 8-bit number.
auto AnyMessageGroup() {
  return fuzztest::ElementOf(GetAllEnums<MessageGroup>(0, 255));
}

// MessageCode received over the wire is an 8-bit number.
auto AnyMessageCode() {
  return fuzztest::ElementOf(GetAllEnums<MessageCode>(0, 255));
}

void HandlesAnyMessage(MessageGroup group, MessageCode code,
                       absl::string_view payload) {
  FastPairDevice fp_device("model id", "ble address",
                           Protocol::kFastPairRetroactivePairing);
  FakeObserver observer;
  MessageStream message_stream(fp_device, std::nullopt, observer);
  message_stream.OnReceived(Message{.message_group = group,
                                    .message_code = code,
                                    .payload = std::string(payload)});
}

FUZZ_TEST(MessageStreamFuzzTest, HandlesAnyMessage)
    .WithDomains(AnyMessageGroup(), AnyMessageCode(),
                 fuzztest::Arbitrary<std::string>());

}  // namespace
}  // namespace fastpair
}  // namespace nearby
