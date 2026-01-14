// Copyright 2025 Google LLC
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

#include "internal/platform/implementation/linux/ble_l2cap_socket.h"

#include <sys/socket.h>
#include <unistd.h>

#include <string>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "gtest/gtest.h"

namespace nearby {
namespace linux {
namespace {

class SocketPair final {
 public:
  SocketPair() {
    int fds[2] = {-1, -1};
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    left_ = fds[0];
    right_ = fds[1];
  }

  ~SocketPair() {
    if (left_ >= 0) close(left_);
    if (right_ >= 0) close(right_);
  }

  int left() const { return left_; }
  int right() const { return right_; }

  void CloseRight() {
    if (right_ >= 0) {
      close(right_);
      right_ = -1;
    }
  }

 private:
  int left_ = -1;
  int right_ = -1;
};

TEST(BleL2capSocketTest, OutputStreamWritesData) {
  SocketPair pair;
  BleL2capSocket socket(pair.left(), /*peripheral_id=*/1);

  ByteArray payload("hello");
  Exception write_result = socket.GetOutputStream().Write(payload);
  EXPECT_TRUE(write_result.Ok());

  char buffer[5];
  ssize_t bytes_read = recv(pair.right(), buffer, sizeof(buffer), 0);
  ASSERT_EQ(bytes_read, sizeof(buffer));
  EXPECT_EQ(std::string(buffer, sizeof(buffer)), "hello");
}

TEST(BleL2capSocketTest, InputStreamReadsData) {
  SocketPair pair;
  BleL2capSocket socket(pair.left(), /*peripheral_id=*/1);

  const char* payload = "world";
  ASSERT_EQ(send(pair.right(), payload, 5, 0), 5);

  ExceptionOr<ByteArray> read_result = socket.GetInputStream().Read(5);
  ASSERT_TRUE(read_result.ok());
  EXPECT_EQ(read_result.result().string_data(), "world");
}

TEST(BleL2capSocketTest, InputStreamReturnsEmptyOnPeerClose) {
  SocketPair pair;
  BleL2capSocket socket(pair.left(), /*peripheral_id=*/1);

  pair.CloseRight();
  ExceptionOr<ByteArray> read_result = socket.GetInputStream().Read(4);
  ASSERT_TRUE(read_result.ok());
  EXPECT_TRUE(read_result.result().Empty());
}

TEST(BleL2capSocketTest, OutputStreamWriteFailsAfterClose) {
  SocketPair pair;
  BleL2capSocket socket(pair.left(), /*peripheral_id=*/1);

  ASSERT_TRUE(socket.GetOutputStream().Close().Ok());
  Exception write_result =
      socket.GetOutputStream().Write(ByteArray("data"));
  EXPECT_EQ(write_result.value, Exception::kIo);
}

TEST(BleL2capSocketTest, CloseNotifierInvoked) {
  SocketPair pair;
  BleL2capSocket socket(pair.left(), /*peripheral_id=*/1);

  bool notified = false;
  socket.SetCloseNotifier([&notified]() { notified = true; });
  ASSERT_TRUE(socket.Close().Ok());
  EXPECT_TRUE(notified);
}

}  // namespace
}  // namespace linux
}  // namespace nearby
