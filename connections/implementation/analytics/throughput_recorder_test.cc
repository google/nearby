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

class ThroughputRecorderTest : public testing::TestWithParam<bool> {
 protected:
  ThroughputRecorderTest() = default;
  ~ThroughputRecorderTest() override {
    ThroughputRecorderContainer::GetInstance().ClearForTest();
  }

  ThroughputRecorderContainer& tp_recorder_container_ =
      ThroughputRecorderContainer::GetInstance();
};

INSTANTIATE_TEST_SUITE_P(ParametrisedTestThroughputRecorderTest,
                         ThroughputRecorderTest, testing::Values(true, false));

TEST(ThroughputRecorder, CalculateThroughputKBps) {
  EXPECT_EQ(ThroughputRecorderContainer::CalculateThroughputKBps(
                kTotalByteSize1GB, kTotalMillis10Sec),
            kTPResultKBPerSec);
  EXPECT_EQ(ThroughputRecorderContainer::CalculateThroughputKBps(
                kTotalByteSize1GB, 0),
            0);
}

TEST(ThroughputRecorder, CalculateThroughputMBps) {
  EXPECT_EQ(ThroughputRecorderContainer::CalculateThroughputMBps(kTPKBPerSec),
            kTPResultMBPerSec);
}

TEST(ThroughputRecorderContainer, InstanceCreate_ContainerSize) {
  ThroughputRecorderContainer& TPRecorderContainer =
      ThroughputRecorderContainer::GetInstance();
  TPRecorderContainer.Start(kPayloadIdA,
                            connections::PayloadDirection::OUTGOING_PAYLOAD,
                            connections::PayloadType::kFile);
  TPRecorderContainer.Start(kPayloadIdB,
                            connections::PayloadDirection::INCOMING_PAYLOAD,
                            connections::PayloadType::kFile);
  EXPECT_EQ(ThroughputRecorderContainer::GetInstance().GetSize(), 2);
  ThroughputRecorderContainer::GetInstance().ClearForTest();
  EXPECT_EQ(ThroughputRecorderContainer::GetInstance().GetSize(), 0);
}

TEST_F(ThroughputRecorderTest, OnFrameSentSaveTransferredSize) {
  tp_recorder_container_.Start(kPayloadIdA,
                               connections::PayloadDirection::OUTGOING_PAYLOAD,
                               connections::PayloadType::kFile);

  PacketMetaData packet_meta_data;
  packet_meta_data.SetPacketSize(kFrameSize);
  tp_recorder_container_.OnFrameSent(
      kPayloadIdA, connections::PayloadDirection::OUTGOING_PAYLOAD,
      location::nearby::proto::connections::BLE, packet_meta_data);
  tp_recorder_container_.OnFrameSent(
      kPayloadIdA, connections::PayloadDirection::OUTGOING_PAYLOAD,
      location::nearby::proto::connections::BLE, packet_meta_data);
  tp_recorder_container_.OnFrameSent(
      kPayloadIdA, connections::PayloadDirection::OUTGOING_PAYLOAD,
      location::nearby::proto::connections::BLE, packet_meta_data);

  EXPECT_EQ(tp_recorder_container_.GetTotalByteSize(
                kPayloadIdA, connections::PayloadDirection::OUTGOING_PAYLOAD,
                location::nearby::proto::connections::BLE),
            kFrameSize * 3);
}

TEST_F(ThroughputRecorderTest, OnIgnoreUnkownPaylaodType) {
  tp_recorder_container_.Start(kPayloadIdA,
                               connections::PayloadDirection::OUTGOING_PAYLOAD,
                               connections::PayloadType::kUnknown);

  PacketMetaData packet_meta_data;
  tp_recorder_container_.OnFrameSent(
      kPayloadIdA, connections::PayloadDirection::OUTGOING_PAYLOAD,
      location::nearby::proto::connections::BLE, packet_meta_data);
  EXPECT_EQ(tp_recorder_container_.GetThroughputsSize(
                kPayloadIdA, connections::PayloadDirection::OUTGOING_PAYLOAD),
            0);

  tp_recorder_container_.Start(kPayloadIdA,
                               connections::PayloadDirection::INCOMING_PAYLOAD,
                               connections::PayloadType::kUnknown);
  tp_recorder_container_.OnFrameReceived(
      kPayloadIdA, connections::PayloadDirection::INCOMING_PAYLOAD,
      location::nearby::proto::connections::BLE, packet_meta_data);
  EXPECT_EQ(tp_recorder_container_.GetThroughputsSize(
                kPayloadIdA, connections::PayloadDirection::INCOMING_PAYLOAD),
            0);
}

TEST_P(ThroughputRecorderTest, OnFrameSentStopAndDump) {
  tp_recorder_container_.Start(kPayloadIdA,
                               connections::PayloadDirection::OUTGOING_PAYLOAD,
                               connections::PayloadType::kFile);

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
  tp_recorder_container_.OnFrameSent(
      kPayloadIdA, connections::PayloadDirection::OUTGOING_PAYLOAD,
      location::nearby::proto::connections::BLE, packet_meta_data);
  EXPECT_EQ(tp_recorder_container_.GetDurationMillis(
                kPayloadIdA, connections::PayloadDirection::OUTGOING_PAYLOAD),
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
  tp_recorder_container_.OnFrameSent(
      kPayloadIdA, connections::PayloadDirection::OUTGOING_PAYLOAD,
      location::nearby::proto::connections::BLE, packet_meta_data);

  if (GetParam() == true) {
    LOG(INFO) << "MarkAsSuccess";
    tp_recorder_container_.MarkAsSuccess(
        kPayloadIdA, connections::PayloadDirection::OUTGOING_PAYLOAD);
  }
  int throughput_kbps = tp_recorder_container_.StopTPRecorder(
      kPayloadIdA, connections::PayloadDirection::OUTGOING_PAYLOAD);
  EXPECT_NE(throughput_kbps, 0);
  EXPECT_EQ(tp_recorder_container_.GetSize(), 0);
}

TEST_F(ThroughputRecorderTest, OnFrameSentStopAndDumpForMultiMeadium) {
  tp_recorder_container_.Start(kPayloadIdA,
                               connections::PayloadDirection::OUTGOING_PAYLOAD,
                               connections::PayloadType::kFile);

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
  tp_recorder_container_.OnFrameSent(
      kPayloadIdA, connections::PayloadDirection::OUTGOING_PAYLOAD,
      location::nearby::proto::connections::BLE, packet_meta_data1);

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
  tp_recorder_container_.OnFrameSent(
      kPayloadIdA, connections::PayloadDirection::OUTGOING_PAYLOAD,
      location::nearby::proto::connections::WIFI_LAN, packet_meta_data2);

  tp_recorder_container_.MarkAsSuccess(
      kPayloadIdA, connections::PayloadDirection::OUTGOING_PAYLOAD);
  int throughput_kbps = tp_recorder_container_.StopTPRecorder(
      kPayloadIdA, connections::PayloadDirection::OUTGOING_PAYLOAD);
  EXPECT_NE(throughput_kbps, 0);
}

TEST_F(ThroughputRecorderTest, OnFrameReceivedCheckDurationMillis) {
  tp_recorder_container_.Start(kPayloadIdA,
                               connections::PayloadDirection::INCOMING_PAYLOAD,
                               connections::PayloadType::kFile);

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
  tp_recorder_container_.OnFrameReceived(
      kPayloadIdA, connections::PayloadDirection::INCOMING_PAYLOAD,
      location::nearby::proto::connections::BLE, packet_meta_data);
  EXPECT_EQ(tp_recorder_container_.GetDurationMillis(
                kPayloadIdA, connections::PayloadDirection::INCOMING_PAYLOAD),
            packet_meta_data.GetEncryptionTimeInMillis() +
                packet_meta_data.GetFileIoTimeInMillis() +
                packet_meta_data.GetSocketIoTimeInMillis());
}

TEST_F(ThroughputRecorderTest, OnTPRecorderNotStarted) {
  tp_recorder_container_.Start(kPayloadIdA,
                               connections::PayloadDirection::OUTGOING_PAYLOAD,
                               connections::PayloadType::kUnknown);
  EXPECT_FALSE(tp_recorder_container_.Dump(
      kPayloadIdA, connections::PayloadDirection::OUTGOING_PAYLOAD,
      location::nearby::proto::connections::BLE));
}

}  // namespace
}  // namespace analytics
}  // namespace nearby
