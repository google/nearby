// Copyright 2024 Google LLC
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

#include "connections/implementation/mediums/multiplex/multiplex_output_stream.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "connections/implementation/mediums/multiplex/multiplex_frames.h"
#include "internal/platform/atomic_boolean.h"
#include "internal/platform/base64_utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/multi_thread_executor.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/pipe.h"
#include "proto/mediums/multiplex_frames.pb.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace multiplex {

constexpr absl::string_view kServiceId_1 = "serviceId_1";
constexpr absl::string_view kServiceId_2 = "serviceId_2";
constexpr absl::string_view kNoSalt = "";
constexpr absl::string_view kSalt_1 = "DNFG";
constexpr absl::string_view kSalt_2 = "YFRT";

using ::location::nearby::mediums::ConnectionResponseFrame;
using ::location::nearby::mediums::MultiplexControlFrame;
using ::location::nearby::mediums::MultiplexFrame;

class MultiplexOutputStreamTest : public ::testing::Test {
 protected:
  ExceptionOr<MultiplexFrame> ReadFrame() {
    ExceptionOr<std::int32_t> read_int = Base64Utils::ReadInt(reader_.get());
    if (!read_int.ok()) return read_int.GetException();
    if (read_int.result() <= 0) return {Exception::kFailed};

    ExceptionOr<ByteArray> received_data =
        reader_->ReadExactly(read_int.result());
    if (!received_data.ok()) return received_data.GetException();
    auto bytes = std::move(received_data.result());
    return FromBytes(bytes);
  }

  AtomicBoolean enabled_{true};
  std::pair<std::unique_ptr<InputStream>, std::unique_ptr<OutputStream>> pipe_ =
      CreatePipe();

  std::unique_ptr<InputStream> reader_ = std::move(pipe_.first);
  std::unique_ptr<OutputStream> writer_ = std::move(pipe_.second);
  std::unique_ptr<MultiplexOutputStream> multiplex_output_stream_;
};

TEST_F(MultiplexOutputStreamTest, SendConnectionRequestFrame) {
  multiplex_output_stream_ = std::make_unique<MultiplexOutputStream>(
      writer_.get(), enabled_);
  EXPECT_TRUE(multiplex_output_stream_->WriteConnectionRequestFrame(
      std::string(kServiceId_1), std::string(kNoSalt)));

  auto request = ReadFrame();
  ASSERT_TRUE(request.ok());
  auto frame = request.result();
  EXPECT_EQ(frame.control_frame().control_frame_type(),
            MultiplexControlFrame::CONNECTION_REQUEST);
  EXPECT_EQ(frame.header().salted_service_id_hash(),
            std::string(GenerateServiceIdHashWithSalt(std::string(kServiceId_1),
                                                      std::string(kNoSalt))));

  multiplex_output_stream_->Shutdown();
}

TEST_F(MultiplexOutputStreamTest, SendConnectionRequestFrameDisabled) {
  enabled_.Set(false);
  multiplex_output_stream_ = std::make_unique<MultiplexOutputStream>(
      writer_.get(), enabled_);
  EXPECT_FALSE(multiplex_output_stream_->WriteConnectionRequestFrame(
      std::string(kServiceId_1), std::string(kNoSalt)));

  multiplex_output_stream_->Shutdown();
}

TEST_F(MultiplexOutputStreamTest, SendConnectionResponseFrame) {
  multiplex_output_stream_ = std::make_unique<MultiplexOutputStream>(
      writer_.get(), enabled_);
  EXPECT_TRUE(multiplex_output_stream_->WriteConnectionResponseFrame(
      GenerateServiceIdHash(std::string(kServiceId_1)), std::string(kNoSalt),
      ConnectionResponseFrame::CONNECTION_ACCEPTED));

  auto response = ReadFrame();
  ASSERT_TRUE(response.ok());
  auto frame = response.result();
  EXPECT_EQ(frame.control_frame().control_frame_type(),
            MultiplexControlFrame::CONNECTION_RESPONSE);
  EXPECT_EQ(frame.header().salted_service_id_hash(),
            std::string(GenerateServiceIdHashWithSalt(std::string(kServiceId_1),
                                                      std::string(kNoSalt))));
  EXPECT_EQ(frame.control_frame()
                .connection_response_frame()
                .connection_response_code(),
            ConnectionResponseFrame::CONNECTION_ACCEPTED);

  multiplex_output_stream_->Shutdown();
}

TEST_F(MultiplexOutputStreamTest, SendConnectionResponseFrameDisabled) {
  enabled_.Set(false);
  multiplex_output_stream_ = std::make_unique<MultiplexOutputStream>(
      writer_.get(), enabled_);
  EXPECT_FALSE(multiplex_output_stream_->WriteConnectionResponseFrame(
      GenerateServiceIdHash(std::string(kServiceId_1)), std::string(kNoSalt),
      ConnectionResponseFrame::CONNECTION_ACCEPTED));

  multiplex_output_stream_->Shutdown();
}

TEST_F(MultiplexOutputStreamTest, CloseVirtualStreamFailed) {
  multiplex_output_stream_ = std::make_unique<MultiplexOutputStream>(
      writer_.get(), enabled_);
  EXPECT_FALSE(multiplex_output_stream_->Close(std::string(kServiceId_1)));

  multiplex_output_stream_->Shutdown();
}

TEST_F(MultiplexOutputStreamTest, CloseVirtualStreamSuccess) {
  multiplex_output_stream_ = std::make_unique<MultiplexOutputStream>(
      writer_.get(), enabled_);
  EXPECT_FALSE(multiplex_output_stream_->Close(std::string(kServiceId_1)));

  multiplex_output_stream_->CreateVirtualOutputStream(std::string(kServiceId_1),
                                                      std::string(kNoSalt));
  EXPECT_TRUE(multiplex_output_stream_->Close(std::string(kServiceId_1)));

  auto request = ReadFrame();
  ASSERT_TRUE(request.ok());
  auto frame = request.result();
  EXPECT_EQ(frame.control_frame().control_frame_type(),
            MultiplexControlFrame::DISCONNECTION);
  EXPECT_EQ(frame.header().salted_service_id_hash(),
            std::string(GenerateServiceIdHashWithSalt(std::string(kServiceId_1),
                                                      std::string(kNoSalt))));

  multiplex_output_stream_->Shutdown();
}

TEST_F(MultiplexOutputStreamTest, CreateVirtualStream_SendData) {
  multiplex_output_stream_ = std::make_unique<MultiplexOutputStream>(
      writer_.get(), enabled_);

  auto virtual_output_stream =
      multiplex_output_stream_->CreateVirtualOutputStream(
          std::string(kServiceId_1), std::string(kSalt_1));

  const ByteArray data("abcdefghijklmnopqrstuvwxyz");
  virtual_output_stream->Write(data);
  virtual_output_stream->Flush();
  auto frame_data = ReadFrame();
  ASSERT_TRUE(frame_data.ok());
  auto frame = frame_data.result();
  EXPECT_EQ(frame.frame_type(), MultiplexFrame::DATA_FRAME);
  EXPECT_EQ(frame.header().salted_service_id_hash(),
            std::string(GenerateServiceIdHashWithSalt(std::string(kServiceId_1),
                                                      std::string(kSalt_1))));
  EXPECT_EQ(frame.data_frame().data(), std::string(data));

  multiplex_output_stream_->Shutdown();
}

TEST_F(MultiplexOutputStreamTest, CreateTwoVirtualStreams_SendData) {
  multiplex_output_stream_ = std::make_unique<MultiplexOutputStream>(
      writer_.get(), enabled_);

  auto virtual_output_stream_1 =
      multiplex_output_stream_->CreateVirtualOutputStreamForFirstVirtualSocket(
          std::string(kServiceId_1), std::string(kSalt_1));
  auto virtual_output_stream_2 =
      multiplex_output_stream_->CreateVirtualOutputStreamForFirstVirtualSocket(
          std::string(kServiceId_2), std::string(kSalt_2));

  const ByteArray data_1("abcdefg");
  const ByteArray data_2("hijklmn");
  MultiThreadExecutor executor(2);
  CountDownLatch latch(2);
  executor.Execute([&virtual_output_stream_1, &latch, &data_1]() {
    absl::SleepFor(absl::Milliseconds(100));
    virtual_output_stream_1->Write(data_1);
    virtual_output_stream_1->Flush();
    latch.CountDown();
  });
  executor.Execute([&virtual_output_stream_2, &latch, &data_2]() {
    virtual_output_stream_2->Write(data_2);
    virtual_output_stream_2->Flush();
    latch.CountDown();
  });
  EXPECT_TRUE(latch.Await(absl::Milliseconds(5000)).result());

  auto frame_data = ReadFrame();
  ASSERT_TRUE(frame_data.ok());
  auto frame = frame_data.result();
  EXPECT_EQ(frame.frame_type(), MultiplexFrame::DATA_FRAME);
  bool first_frame_is_data_1 = true;
  if (frame.header().salted_service_id_hash() ==
            std::string(GenerateServiceIdHashWithSalt(std::string(kServiceId_1),
                                                      std::string(kSalt_1)))) {
    EXPECT_EQ(frame.data_frame().data(), std::string(data_1));
    NEARBY_LOGS(INFO) << "Read first virtual stream frame first.";
  } else {
    EXPECT_EQ(frame.header().salted_service_id_hash(),
            std::string(GenerateServiceIdHashWithSalt(std::string(kServiceId_2),
                                                      std::string(kSalt_2))));
    EXPECT_EQ(frame.data_frame().data(), std::string(data_2));
    first_frame_is_data_1 = false;
    NEARBY_LOGS(INFO) << "Read second virtual stream frame first.";
  }

  frame_data = ReadFrame();
  ASSERT_TRUE(frame_data.ok());
  frame = frame_data.result();
  EXPECT_EQ(frame.frame_type(), MultiplexFrame::DATA_FRAME);
  if (first_frame_is_data_1) {
    EXPECT_EQ(frame.data_frame().data(), std::string(data_2));
  } else {
    EXPECT_EQ(frame.data_frame().data(), std::string(data_1));
  }
  multiplex_output_stream_->Shutdown();
}

}  // namespace multiplex
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
