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

#include "sharing/nearby_connections_stream_buffer_manager.h"

#include <stddef.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

namespace nearby {
namespace sharing {
namespace {

class FakeStream : public NcInputStream {
 public:
  FakeStream() = default;
  ~FakeStream() override = default;
  FakeStream(const FakeStream&) = delete;
  FakeStream& operator=(const FakeStream&) = delete;

  NcExceptionOr<NcByteArray> Read(std::int64_t size) override {
    if (should_throw_exception_) {
      return NcException::kIo;
    }
    return NcExceptionOr<NcByteArray>(NcByteArray(std::string(size, '\0')));
  }

  NcExceptionOr<size_t> Skip(size_t offset) override {
    if (should_throw_exception_) {
      return NcException::kIo;
    }
    return NcExceptionOr<size_t>(0);
  }

  NcException Close() override {
    if (should_throw_exception_) {
      return {.value = NcException::kIo};
    }
    return {.value = NcException::kSuccess};
  }

  bool should_throw_exception_ = false;
};

}  // namespace

struct CreatePayloadStreamResult {
  NcPayload payload;
  FakeStream* stream;
};

class NearbyConnectionsStreamBufferManagerTest : public testing::Test {
 protected:
  CreatePayloadStreamResult CreatePayload(int64_t payload_id) {
    CreatePayloadStreamResult payload_and_stream;
    auto stream = std::make_unique<FakeStream>();
    FakeStream* stream_ptr = stream.get();

    payload_and_stream.stream = stream_ptr;

    NcPayload payload(payload_id, std::move(stream));
    payload_and_stream.payload = std::move(payload);

    return payload_and_stream;
  }

  NearbyConnectionsStreamBufferManager buffer_manager_;
};

TEST_F(NearbyConnectionsStreamBufferManagerTest,
       SingleStreamTrackingAndCheckingTransferredSize) {
  CreatePayloadStreamResult payload_and_stream =
      CreatePayload(/*payload_id=*/1);

  buffer_manager_.StartTrackingPayload(std::move(payload_and_stream.payload));
  EXPECT_TRUE(buffer_manager_.IsTrackingPayload(/*payload_id=*/1));

  buffer_manager_.HandleBytesTransferred(
      /*payload_id=*/1,
      /*cumulative_bytes_transferred_so_far=*/1980);
  buffer_manager_.HandleBytesTransferred(
      /*payload_id=*/1,
      /*cumulative_bytes_transferred_so_far=*/2500);

  NcByteArray array =
      buffer_manager_.GetCompletePayloadAndStopTracking(/*payload_id=*/1);
  EXPECT_FALSE(buffer_manager_.IsTrackingPayload(/*payload_id=*/1));
  EXPECT_EQ(array.size(), 2500u);
}

TEST_F(NearbyConnectionsStreamBufferManagerTest,
       MultipleStreamTrackingAndCheckingTransferredSize) {
  CreatePayloadStreamResult payload_and_stream_1 =
      CreatePayload(/*payload_id=*/1);
  CreatePayloadStreamResult payload_and_stream_2 =
      CreatePayload(/*payload_id=*/2);

  buffer_manager_.StartTrackingPayload(std::move(payload_and_stream_1.payload));
  EXPECT_TRUE(buffer_manager_.IsTrackingPayload(/*payload_id=*/1));

  buffer_manager_.StartTrackingPayload(std::move(payload_and_stream_2.payload));
  EXPECT_TRUE(buffer_manager_.IsTrackingPayload(/*payload_id=*/2));

  buffer_manager_.HandleBytesTransferred(
      /*payload_id=*/1,
      /*cumulative_bytes_transferred_so_far=*/1980);
  buffer_manager_.HandleBytesTransferred(
      /*payload_id=*/2,
      /*cumulative_bytes_transferred_so_far=*/1980);
  buffer_manager_.HandleBytesTransferred(
      /*payload_id=*/1,
      /*cumulative_bytes_transferred_so_far=*/2500);
  buffer_manager_.HandleBytesTransferred(
      /*payload_id=*/2,
      /*cumulative_bytes_transferred_so_far=*/3000);

  NcByteArray array1 =
      buffer_manager_.GetCompletePayloadAndStopTracking(/*payload_id=*/1);
  EXPECT_FALSE(buffer_manager_.IsTrackingPayload(/*payload_id=*/1));
  EXPECT_EQ(array1.size(), 2500u);

  NcByteArray array2 =
      buffer_manager_.GetCompletePayloadAndStopTracking(/*payload_id=*/2);
  EXPECT_FALSE(buffer_manager_.IsTrackingPayload(/*payload_id=*/2));
  EXPECT_EQ(array2.size(), 3000u);
}

TEST_F(NearbyConnectionsStreamBufferManagerTest,
       SingleStreamCheckTrackingFailure) {
  CreatePayloadStreamResult payload_and_stream =
      CreatePayload(/*payload_id=*/1);

  buffer_manager_.StartTrackingPayload(std::move(payload_and_stream.payload));
  EXPECT_TRUE(buffer_manager_.IsTrackingPayload(/*payload_id=*/1));

  buffer_manager_.HandleBytesTransferred(
      /*payload_id=*/1,
      /*cumulative_bytes_transferred_so_far=*/1980);
  buffer_manager_.StopTrackingFailedPayload(/*payload_id=*/1);
  EXPECT_FALSE(buffer_manager_.IsTrackingPayload(/*payload_id=*/1));
}

TEST_F(NearbyConnectionsStreamBufferManagerTest, SingleStreamCheckException) {
  CreatePayloadStreamResult payload_and_stream =
      CreatePayload(/*payload_id=*/1);

  buffer_manager_.StartTrackingPayload(std::move(payload_and_stream.payload));
  EXPECT_TRUE(buffer_manager_.IsTrackingPayload(/*payload_id=*/1));

  payload_and_stream.stream->should_throw_exception_ = true;
  buffer_manager_.HandleBytesTransferred(
      /*payload_id=*/1,
      /*cumulative_bytes_transferred_so_far=*/1980);

  EXPECT_FALSE(buffer_manager_.IsTrackingPayload(/*payload_id=*/1));
}

}  // namespace sharing
}  // namespace nearby
