// Copyright 2022-2023 Google LLC
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

#include "connections/implementation/analytics/throughput_recorder.h"

#include <stdint.h>

#include <ostream>
#include <string>

#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/logging.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace analytics {
namespace {
// TODO(b/246693797): Add unit tests coverage for throughput recorder code

constexpr int64_t kPayloadIdA = 123456789;
constexpr int64_t kPayloadIdB = 987654321;
constexpr int kFrameSize = 10 * 64 * 1024;
constexpr int64_t kTotalByteSize1GB = 1024 * 1024 * 1024;
constexpr int64_t kTotalMillis10Sec = 10 * 1000;
constexpr int kTPResultKBPerSec = 1024 * 1024 / 10;
constexpr int kTPKBPerSec = 100 * 1024;
constexpr int kTPResultMBPerSec = 100;

// class ThroughputRecorderTest : public testing::Test {
class ThroughputRecorderTest : public testing::TestWithParam<bool> {
 protected:
  ThroughputRecorderTest() = default;
  ~ThroughputRecorderTest() override {
    ThroughputRecorderContainer::GetInstance().Shutdown();
  }

  ThroughputRecorderContainer& tp_recorder_container_ =
      ThroughputRecorderContainer::GetInstance();
};

INSTANTIATE_TEST_SUITE_P(ParametrisedTestThroughputRecorderTest,
                         ThroughputRecorderTest, testing::Values(true, false));

TEST(ThroughputRecorder, CalculateThroughputKBps) {
  EXPECT_EQ(ThroughputRecorder::CalculateThroughputKBps(kTotalByteSize1GB,
                                                        kTotalMillis10Sec),
            kTPResultKBPerSec);
  EXPECT_EQ(ThroughputRecorder::CalculateThroughputKBps(kTotalByteSize1GB, 0),
            0);
}

TEST(ThroughputRecorder, CalculateThroughputMBps) {
  EXPECT_EQ(ThroughputRecorder::CalculateThroughputMBps(kTPKBPerSec),
            kTPResultMBPerSec);
}

TEST(ThroughputRecorderContainer, InstanceCreate_ContainerSize) {
  ThroughputRecorderContainer& TPRecorderContainer =
      ThroughputRecorderContainer::GetInstance();
  TPRecorderContainer.GetTPRecorder(kPayloadIdA,
                                    PayloadDirection::OUTGOING_PAYLOAD);
  TPRecorderContainer.GetTPRecorder(kPayloadIdB,
                                    PayloadDirection::INCOMING_PAYLOAD);
  EXPECT_EQ(ThroughputRecorderContainer::GetInstance().GetSize(), 2);
  ThroughputRecorderContainer::GetInstance().Shutdown();
  EXPECT_EQ(ThroughputRecorderContainer::GetInstance().GetSize(), 0);
}

TEST_F(ThroughputRecorderTest, OnFrameSentSaveTransferredSize) {
  auto TPRecorder = tp_recorder_container_.GetTPRecorder(
      kPayloadIdA, PayloadDirection::OUTGOING_PAYLOAD);
  TPRecorder->Start(PayloadType::kFile, PayloadDirection::OUTGOING_PAYLOAD);

  PacketMetaData packet_meta_data;
  packet_meta_data.SetPacketSize(kFrameSize);
  TPRecorder->OnFrameSent(location::nearby::proto::connections::BLE,
                          packet_meta_data);
  TPRecorder->OnFrameSent(location::nearby::proto::connections::BLE,
                          packet_meta_data);
  TPRecorder->OnFrameSent(location::nearby::proto::connections::BLE,
                          packet_meta_data);

  auto throughput =
      TPRecorder->GetThroughput(location::nearby::proto::connections::BLE, 0);
  EXPECT_EQ(throughput.GetTotalByteSize(), kFrameSize * 3);
}

TEST_F(ThroughputRecorderTest, OnIgnoreUnkownPaylaodType) {
  auto TPRecorder = tp_recorder_container_.GetTPRecorder(
      kPayloadIdA, PayloadDirection::OUTGOING_PAYLOAD);
  TPRecorder->Start(PayloadType::kUnknown, PayloadDirection::OUTGOING_PAYLOAD);

  PacketMetaData packet_meta_data;
  TPRecorder->OnFrameSent(location::nearby::proto::connections::BLE,
                          packet_meta_data);
  EXPECT_EQ(TPRecorder->GetThroughputsSize(), 0);

  TPRecorder->Start(PayloadType::kUnknown, PayloadDirection::INCOMING_PAYLOAD);
  TPRecorder->OnFrameReceived(location::nearby::proto::connections::BLE,
                              packet_meta_data);
  EXPECT_EQ(TPRecorder->GetThroughputsSize(), 0);
}

TEST_P(ThroughputRecorderTest, OnFrameSentStopAndDump) {
  auto TPRecorder = tp_recorder_container_.GetTPRecorder(
      kPayloadIdA, PayloadDirection::OUTGOING_PAYLOAD);
  TPRecorder->Start(PayloadType::kFile, PayloadDirection::OUTGOING_PAYLOAD);

  PacketMetaData packet_meta_data;
  packet_meta_data.SetPacketSize(kFrameSize);
  packet_meta_data.StartFileIo();
  absl::SleepFor(absl::Milliseconds(5));
  packet_meta_data.StopFileIo();
  packet_meta_data.StartEncryption();
  absl::SleepFor(absl::Milliseconds(6));
  packet_meta_data.StopEncryption();
  packet_meta_data.StartSocketIo();
  absl::SleepFor(absl::Milliseconds(7));
  packet_meta_data.StopSocketIo();
  TPRecorder->OnFrameSent(location::nearby::proto::connections::BLE,
                          packet_meta_data);
  EXPECT_EQ(TPRecorder->GetDurationMillis(),
            packet_meta_data.GetEncryptionTimeInMillis() +
                packet_meta_data.GetFileIoTimeInMillis() +
                packet_meta_data.GetSocketIoTimeInMillis());

  packet_meta_data.SetPacketSize(kFrameSize);
  packet_meta_data.StartFileIo();
  absl::SleepFor(absl::Milliseconds(15));
  packet_meta_data.StopFileIo();
  packet_meta_data.StartEncryption();
  absl::SleepFor(absl::Milliseconds(16));
  packet_meta_data.StopEncryption();
  packet_meta_data.StartSocketIo();
  absl::SleepFor(absl::Milliseconds(17));
  packet_meta_data.StopSocketIo();
  TPRecorder->OnFrameSent(location::nearby::proto::connections::BLE,
                          packet_meta_data);

  if (GetParam() == true) {
    NEARBY_LOGS(INFO) << "MarkAsSuccess";
    TPRecorder->MarkAsSuccess();
  }
  EXPECT_TRUE(TPRecorder->Stop());
  EXPECT_NE(TPRecorder->GetThroughputKbps(), 0);
}

TEST_F(ThroughputRecorderTest, OnFrameSentStopAndDumpForMultiMeadium) {
  auto TPRecorder = tp_recorder_container_.GetTPRecorder(
      kPayloadIdA, PayloadDirection::OUTGOING_PAYLOAD);
  TPRecorder->Start(PayloadType::kFile, PayloadDirection::OUTGOING_PAYLOAD);

  PacketMetaData packet_meta_data1;
  packet_meta_data1.SetPacketSize(kFrameSize);
  packet_meta_data1.StartFileIo();
  absl::SleepFor(absl::Milliseconds(5));
  packet_meta_data1.StopFileIo();
  packet_meta_data1.StartEncryption();
  absl::SleepFor(absl::Milliseconds(6));
  packet_meta_data1.StopEncryption();
  packet_meta_data1.StartSocketIo();
  absl::SleepFor(absl::Milliseconds(7));
  packet_meta_data1.StopSocketIo();
  TPRecorder->OnFrameSent(location::nearby::proto::connections::BLE,
                          packet_meta_data1);

  PacketMetaData packet_meta_data2;
  packet_meta_data2.SetPacketSize(kFrameSize);
  packet_meta_data2.StartFileIo();
  absl::SleepFor(absl::Milliseconds(15));
  packet_meta_data2.StopFileIo();
  packet_meta_data2.StartEncryption();
  absl::SleepFor(absl::Milliseconds(16));
  packet_meta_data2.StopEncryption();
  packet_meta_data2.StartSocketIo();
  absl::SleepFor(absl::Milliseconds(17));
  packet_meta_data2.StopSocketIo();
  TPRecorder->OnFrameSent(location::nearby::proto::connections::WIFI_LAN,
                          packet_meta_data2);

  TPRecorder->MarkAsSuccess();
  EXPECT_TRUE(TPRecorder->Stop());
  EXPECT_NE(TPRecorder->GetThroughputKbps(), 0);
}

TEST_F(ThroughputRecorderTest, OnFrameReceivedCheckDurationMillis) {
  auto TPRecorder = tp_recorder_container_.GetTPRecorder(
      kPayloadIdA, PayloadDirection::INCOMING_PAYLOAD);
  TPRecorder->Start(PayloadType::kFile, PayloadDirection::INCOMING_PAYLOAD);

  PacketMetaData packet_meta_data;
  packet_meta_data.SetPacketSize(kFrameSize);
  packet_meta_data.StartFileIo();
  absl::SleepFor(absl::Milliseconds(5));
  packet_meta_data.StopFileIo();
  packet_meta_data.StartEncryption();
  absl::SleepFor(absl::Milliseconds(6));
  packet_meta_data.StopEncryption();
  packet_meta_data.StartSocketIo();
  absl::SleepFor(absl::Milliseconds(7));
  packet_meta_data.StopSocketIo();
  TPRecorder->OnFrameReceived(location::nearby::proto::connections::BLE,
                              packet_meta_data);
  EXPECT_EQ(TPRecorder->GetDurationMillis(),
            packet_meta_data.GetEncryptionTimeInMillis() +
                packet_meta_data.GetFileIoTimeInMillis() +
                packet_meta_data.GetSocketIoTimeInMillis());
}

TEST_F(ThroughputRecorderTest, OnTPRecorderNotStarted) {
  auto TPRecorder = tp_recorder_container_.GetTPRecorder(
      kPayloadIdA, PayloadDirection::OUTGOING_PAYLOAD);
  auto throughput =
      TPRecorder->GetThroughput(location::nearby::proto::connections::BLE, 0);
  EXPECT_FALSE(throughput.dump());
}

}  // namespace
}  // namespace analytics
}  // namespace nearby
