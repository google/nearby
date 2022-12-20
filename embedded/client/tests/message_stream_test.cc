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

#include <deque>
#include <iostream>
#include <vector>

#include "fakes.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "nearby.h"
#include "nearby_message_stream.h"

#if NEARBY_FP_MESSAGE_STREAM
constexpr uint64_t kPeerAddress = 0x101112;
constexpr size_t kBufferSize = 64;
constexpr size_t kMaxPayloadSize =
    kBufferSize - sizeof(nearby_message_stream_Metadata);
constexpr size_t kHeaderSize = 4;

using ::testing::ElementsAreArray;

class StreamMessage {
 public:
  explicit StreamMessage(nearby_message_stream_Message* message)
      : StreamMessage(message->message_group, message->message_code,
                      std::vector<uint8_t>(message->data,
                                           message->data + message->length)) {}
  StreamMessage(uint8_t group, uint8_t code) : group_(group), code_(code) {}
  StreamMessage(uint8_t group, uint8_t code, std::vector<uint8_t> data)
      : group_(group), code_(code), data_(data) {}

  bool operator==(const StreamMessage& b) const {
    return this->group_ == b.group_ && this->code_ == b.code_ &&
           this->data_ == b.data_;
  }

 private:
  unsigned int group_;
  unsigned int code_;
  std::vector<uint8_t> data_;
  friend std::ostream& operator<<(std::ostream& os,
                                  const StreamMessage& message);
};

static std::string VecToString(std::vector<uint8_t> data) {
  std::stringstream output;
  output << "0x" << std::hex;
  for (int i = 0; i < data.size(); i++) {
    output << std::setfill('0') << std::setw(2) << (unsigned)data[i];
  }
  return output.str();
}

std::ostream& operator<<(std::ostream& os, const StreamMessage& message) {
  os << std::hex;
  os << "group: 0x" << std::setfill('0') << std::setw(2) << message.group_;
  os << " code: 0x" << std::setfill('0') << std::setw(2) << message.code_;
  os << std::dec << " length: " << message.data_.size();
  if (message.data_.size() > 0) {
    os << " data: " << VecToString(message.data_);
  }
  return os;
}

uint8_t buffer[kBufferSize];

void OnMessageReceived(uint64_t peer_address,
                       nearby_message_stream_Message* message);

class MessageStreamTest : public ::testing::Test {
 public:
  void Read(const uint8_t* data, size_t length) {
    nearby_message_stream_Read(&stream_state_, data, length);
  }

  void ReadByteByByte(const uint8_t* data, size_t length) {
    for (int i = 0; i < length; i++) {
      nearby_message_stream_Read(&stream_state_, data + i, 1);
    }
  }

  void Send(const nearby_message_stream_Message* message) {
    nearby_message_stream_Send(kPeerAddress, message);
  }

  void SendAck(const nearby_message_stream_Message* message) {
    nearby_message_stream_SendAck(kPeerAddress, message);
  }

  void SendNack(const nearby_message_stream_Message* message,
                uint8_t fail_reason) {
    nearby_message_stream_SendNack(kPeerAddress, message, fail_reason);
  }

 protected:
  void SetUp() override;

  std::deque<StreamMessage> received_messages_;

 private:
  void AddMessage(nearby_message_stream_Message* message) {
    received_messages_.emplace_back(StreamMessage(message));
  }
  // To allow access to |AddMessage|
  friend void OnMessageReceived(uint64_t peer_address,
                                nearby_message_stream_Message* message);

  nearby_message_stream_State stream_state_ = {
      .on_message_received = OnMessageReceived,
      .peer_address = kPeerAddress,
      .length = sizeof(buffer),
      .buffer = buffer};
}* test_fixture;

void MessageStreamTest::SetUp() {
  test_fixture = this;
  nearby_test_fakes_GetRfcommOutput(kPeerAddress).clear();
  nearby_message_stream_Init(&stream_state_);
}

void OnMessageReceived(uint64_t peer_address,
                       nearby_message_stream_Message* message) {
  EXPECT_EQ(kPeerAddress, peer_address);
  test_fixture->AddMessage(message);
}

TEST_F(MessageStreamTest, ReadWholeMessageWithNoData) {
  uint8_t group = 120;
  uint8_t code = 130;
  uint8_t message[] = {group, code, 0, 0};

  Read(message, sizeof(message));

  ASSERT_EQ(1, received_messages_.size());
  ASSERT_EQ(StreamMessage(group, code), received_messages_[0]);
}

TEST_F(MessageStreamTest, ReadMessageInChunksWithNoData) {
  uint8_t group = 120;
  uint8_t code = 130;
  uint8_t message[] = {group, code, 0, 0};

  ReadByteByByte(message, sizeof(message));

  ASSERT_EQ(1, received_messages_.size());
  ASSERT_EQ(StreamMessage(group, code), received_messages_[0]);
}

TEST_F(MessageStreamTest, ReadWholeMessageSmallPayload) {
  uint8_t group = 120;
  uint8_t code = 130;
  uint8_t message[] = {group, code, 0, 1, 30};

  Read(message, sizeof(message));

  ASSERT_EQ(1, received_messages_.size());
  ASSERT_EQ(StreamMessage(group, code,
                          std::vector<uint8_t>(message + kHeaderSize,
                                               message + sizeof(message))),
            received_messages_[0]);
}

TEST_F(MessageStreamTest, ReadMessageInChunksSmallPayload) {
  uint8_t group = 120;
  uint8_t code = 130;
  uint8_t message[] = {group, code, 0, 1, 30};

  ReadByteByByte(message, sizeof(message));

  ASSERT_EQ(1, received_messages_.size());
  ASSERT_EQ(StreamMessage(group, code,
                          std::vector<uint8_t>(message + kHeaderSize,
                                               message + sizeof(message))),
            received_messages_[0]);
}

TEST_F(MessageStreamTest, ReadWholeMessageMaximumPayloadSize) {
  uint8_t group = 120;
  uint8_t code = 130;
  uint8_t message[kHeaderSize + kMaxPayloadSize];
  message[0] = group;
  message[1] = code;
  message[2] = kMaxPayloadSize >> 8;
  message[3] = kMaxPayloadSize;
  for (unsigned i = 0; i < kMaxPayloadSize; i++) {
    message[kHeaderSize + i] = i;
  }

  Read(message, sizeof(message));

  ASSERT_EQ(1, received_messages_.size());
  ASSERT_EQ(StreamMessage(group, code,
                          std::vector<uint8_t>(message + kHeaderSize,
                                               message + sizeof(message))),
            received_messages_[0]);
}

TEST_F(MessageStreamTest, ReadWholeMessageTruncatedPayload) {
  uint8_t group = 120;
  uint8_t code = 130;
  constexpr size_t kPayloadSize = 0x102;
  uint8_t message[kHeaderSize + kPayloadSize];
  message[0] = group;
  message[1] = code;
  message[2] = 0x01;
  message[3] = 0x02;
  for (unsigned i = 0; i < kPayloadSize; i++) {
    message[kHeaderSize + i] = i;
  }

  Read(message, sizeof(message));

  ASSERT_EQ(1, received_messages_.size());
  ASSERT_EQ(StreamMessage(
                group, code,
                std::vector<uint8_t>(message + kHeaderSize,
                                     message + kHeaderSize + kMaxPayloadSize)),
            received_messages_[0]);
}

TEST_F(MessageStreamTest, ReadMessageInChunksTruncatedPayload) {
  uint8_t group = 120;
  uint8_t code = 130;
  constexpr size_t kPayloadSize = 0x102;
  uint8_t message[kHeaderSize + kPayloadSize];
  message[0] = group;
  message[1] = code;
  message[2] = 0x01;
  message[3] = 0x02;
  for (unsigned i = 0; i < kPayloadSize; i++) {
    message[kHeaderSize + i] = i;
  }

  ReadByteByByte(message, sizeof(message));

  ASSERT_EQ(1, received_messages_.size());
  ASSERT_EQ(StreamMessage(
                group, code,
                std::vector<uint8_t>(message + kHeaderSize,
                                     message + kHeaderSize + kMaxPayloadSize)),
            received_messages_[0]);
}

TEST_F(MessageStreamTest, ReadTwoMessages) {
  uint8_t group = 120;
  uint8_t code = 130;
  uint8_t group2 = 121;
  uint8_t code2 = 131;
  uint8_t message[] = {group, code, 0, 1, 30, group2, code2, 0, 2, 31, 32};

  Read(message, sizeof(message));

  ASSERT_EQ(2, received_messages_.size());
  ASSERT_EQ(StreamMessage(group, code,
                          std::vector<uint8_t>(message + 4, message + 5)),
            received_messages_[0]);
  ASSERT_EQ(StreamMessage(group2, code2,
                          std::vector<uint8_t>(message + 9, message + 11)),
            received_messages_[1]);
}

TEST_F(MessageStreamTest, ReadTwoMessagesByteByByte) {
  uint8_t group = 120;
  uint8_t code = 130;
  uint8_t group2 = 121;
  uint8_t code2 = 131;
  uint8_t message[] = {group, code, 0, 1, 30, group2, code2, 0, 2, 31, 32};

  ReadByteByByte(message, sizeof(message));

  ASSERT_EQ(2, received_messages_.size());
  ASSERT_EQ(StreamMessage(group, code,
                          std::vector<uint8_t>(message + 4, message + 5)),
            received_messages_[0]);
  ASSERT_EQ(StreamMessage(group2, code2,
                          std::vector<uint8_t>(message + 9, message + 11)),
            received_messages_[1]);
}

TEST_F(MessageStreamTest, SendMessageNoPayload) {
  nearby_message_stream_Message message{
      .message_group = 10,
      .message_code = 20,
      .length = 0,
      .data = nullptr,
  };
  constexpr uint8_t kExpectedOutput[] = {10, 20, 0, 0};

  Send(&message);

  ASSERT_THAT(
      kExpectedOutput,
      ElementsAreArray(nearby_test_fakes_GetRfcommOutput(kPeerAddress)));
}

TEST_F(MessageStreamTest, SendMessageWithPayload) {
  uint8_t payload[] = {20, 21, 22, 23, 24};
  nearby_message_stream_Message message{
      .message_group = 10,
      .message_code = 11,
      .length = sizeof(payload),
      .data = payload,
  };
  constexpr uint8_t kExpectedOutput[] = {10, 11, 0, sizeof(payload), 20, 21,
                                         22, 23, 24};

  Send(&message);

  ASSERT_THAT(
      kExpectedOutput,
      ElementsAreArray(nearby_test_fakes_GetRfcommOutput(kPeerAddress)));
}

TEST_F(MessageStreamTest, SendAck) {
  nearby_message_stream_Message message{
      .message_group = 10,
      .message_code = 20,
      .length = 0,
      .data = nullptr,
  };
  constexpr uint8_t kExpectedOutput[] = {0xFF, 1, 0, 2, 10, 20};

  SendAck(&message);

  ASSERT_THAT(
      kExpectedOutput,
      ElementsAreArray(nearby_test_fakes_GetRfcommOutput(kPeerAddress)));
}

TEST_F(MessageStreamTest, SendNack) {
  nearby_message_stream_Message message{
      .message_group = 10,
      .message_code = 20,
      .length = 0,
      .data = nullptr,
  };
  constexpr uint8_t kFailReason = 0x88;
  constexpr uint8_t kExpectedOutput[] = {0xFF, 2, 0, 3, kFailReason, 10, 20};

  SendNack(&message, kFailReason);

  ASSERT_THAT(
      kExpectedOutput,
      ElementsAreArray(nearby_test_fakes_GetRfcommOutput(kPeerAddress)));
}
#endif /* NEARBY_FP_MESSAGE_STREAM */

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
