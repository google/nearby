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

#include "connections/implementation/connections_authentication_transport.h"

#include <memory>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/mock_endpoint_channel.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"

namespace nearby {
namespace connections {
namespace {

using ::testing::_;

TEST(ConnectionsAuthenticationTransportTest, TestWriteMessage) {
  std::vector<std::string> messages;
  auto channel = std::make_shared<MockEndpointChannel>();
  ConnectionsAuthenticationTransport transport(channel);
  EXPECT_CALL(*channel, Write(_))
      .WillOnce([&messages](absl::string_view data) {
        messages.push_back(std::string(data));
        return Exception{
            .value = Exception::Value::kSuccess,
        };
      });
  transport.WriteMessage("hello world");
  EXPECT_THAT(messages, testing::ElementsAre("hello world"));
}

TEST(ConnectionsAuthenticationTransportTest, TestReadMessage) {
  std::vector<std::string> messages;
  auto channel = std::make_shared<MockEndpointChannel>();
  ConnectionsAuthenticationTransport transport(channel);
  messages.push_back("hello world");
  EXPECT_CALL(*channel, Read()).WillOnce([&messages]() {
    std::string ret = messages[0];
    messages.erase(messages.begin());
    return ExceptionOr<ByteArray>(ByteArray(ret));
  });
  EXPECT_EQ(transport.ReadMessage(), "hello world");
}

TEST(ConnectionsAuthenticationTransportTest, TestReadMessageFail) {
  std::vector<std::string> messages;
  auto channel = std::make_shared<MockEndpointChannel>();
  ConnectionsAuthenticationTransport transport(channel);
  messages.push_back("hello world");
  EXPECT_CALL(*channel, Read()).WillOnce([]() {
    return ExceptionOr<ByteArray>(Exception::Value::kIo);
  });
  EXPECT_EQ(transport.ReadMessage(), "");
}

}  // namespace
}  // namespace connections
}  // namespace nearby
