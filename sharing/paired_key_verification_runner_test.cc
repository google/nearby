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

#include "sharing/paired_key_verification_runner.h"

#include <stdint.h>

#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "internal/platform/task_runner.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_task_runner.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/certificates/fake_nearby_share_certificate_manager.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/certificates/test_util.h"
#include "sharing/fake_nearby_connection.h"
#include "sharing/incoming_frames_reader.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connection.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/rpc_resources.pb.h"
#include "sharing/proto/wire_format.pb.h"

namespace nearby {
namespace sharing {
namespace {

using V1Frame = ::nearby::sharing::service::proto::V1Frame;
using PairedKeyResultFrame =
    ::nearby::sharing::service::proto::PairedKeyResultFrame;
using ::nearby::sharing::proto::DeviceVisibility;
using PairedKeyVerificationResult =
    PairedKeyVerificationRunner::PairedKeyVerificationResult;
using ::location::nearby::proto::sharing::OSType;

const std::vector<uint8_t>& GetAuthToken() {
  static std::vector<uint8_t>* auth_token = new std::vector<uint8_t>({0, 1, 2});
  return *auth_token;
}

const std::vector<uint8_t>& GetPrivateCertificateHashAuthToken() {
  static std::vector<uint8_t>* private_certificate_hash_auth_token =
      new std::vector<uint8_t>({0x8b, 0xcb, 0xa2, 0xf8, 0xe4, 0x06});
  return *private_certificate_hash_auth_token;
}

const std::vector<uint8_t>& GetIncomingConnectionSignedData() {
  static std::vector<uint8_t>* incoming_connection_signed_data =
      new std::vector<uint8_t>(
          {0x30, 0x45, 0x02, 0x20, 0x4f, 0x83, 0x72, 0xbd, 0x02, 0x70, 0xd9,
           0xda, 0x62, 0x83, 0x5d, 0xb2, 0xdc, 0x6e, 0x3f, 0xa6, 0xa8, 0xa1,
           0x4f, 0x5f, 0xd3, 0xe3, 0xd9, 0x1a, 0x5d, 0x2d, 0x61, 0xd2, 0x6c,
           0xdd, 0x8d, 0xa5, 0x02, 0x21, 0x00, 0xd4, 0xe1, 0x1d, 0x14, 0xcb,
           0x58, 0xf7, 0x02, 0xd5, 0xab, 0x48, 0xe2, 0x2f, 0xcb, 0xc0, 0x53,
           0x41, 0x06, 0x50, 0x65, 0x95, 0x19, 0xa9, 0x22, 0x92, 0x00, 0x42,
           0x01, 0x26, 0x25, 0xcb, 0x8c});
  return *incoming_connection_signed_data;
}

const std::vector<uint8_t>& GetInvalidIncomingConnectionSignedData() {
  static std::vector<uint8_t>* incoming_connection_signed_data =
      new std::vector<uint8_t>(
          {0x30, 0x45, 0x02, 0x20, 0x4f, 0x83, 0x72, 0xbd, 0x02, 0x70, 0xd9,
           0xda, 0x61, 0x83, 0x5d, 0xb2, 0xdc, 0x6e, 0x3f, 0xa6, 0xa8, 0xa1,
           0x4f, 0x5f, 0xd3, 0xe3, 0xd9, 0x1a, 0x5d, 0x2d, 0x61, 0xd2, 0x6c,
           0xdd, 0x8d, 0xa5, 0x02, 0x21, 0x05, 0xd4, 0xe1, 0x1d, 0x14, 0xcb,
           0x58, 0xf7, 0x02, 0xd5, 0xab, 0x48, 0xe2, 0x2f, 0xcb, 0xc0, 0x53,
           0x41, 0x06, 0x50, 0x65, 0x95, 0x19, 0xa9, 0x22, 0x92, 0x00, 0x42,
           0x01, 0x26, 0x25, 0xcb, 0x82});
  return *incoming_connection_signed_data;
}

std::list<PairedKeyResultFrame> GeneratePairedKeyResultFrame() {
  std::list<PairedKeyResultFrame> result;
  PairedKeyResultFrame frame;
  frame.set_status(PairedKeyResultFrame::UNKNOWN);
  result.push_back(frame);

  frame.set_status(PairedKeyResultFrame::SUCCESS);
  frame.set_os_type(OSType::ANDROID);
  result.push_back(frame);

  frame.set_status(PairedKeyResultFrame::FAIL);
  frame.set_os_type(OSType::UNKNOWN_OS_TYPE);
  result.push_back(frame);

  frame.set_status(PairedKeyResultFrame::UNABLE);
  frame.set_os_type(OSType::WINDOWS);
  result.push_back(frame);

  return result;
}

std::list<PairedKeyVerificationRunner::VisibilityHistory>
GenerateVisibilityHistory() {
  constexpr DeviceVisibility kValidVisibilities[] = {
      DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS,
      DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE,
      DeviceVisibility::DEVICE_VISIBILITY_EVERYONE,
      DeviceVisibility::DEVICE_VISIBILITY_HIDDEN,
  };
  std::list<PairedKeyVerificationRunner::VisibilityHistory> result;
  for (DeviceVisibility visibility : kValidVisibilities) {
    for (DeviceVisibility last_visibility : kValidVisibilities) {
      result.push_back({visibility, last_visibility, absl::UnixEpoch()});
    }
  }
  return result;
}

const absl::Duration kTimeout = absl::Seconds(1);

class MockIncomingFramesReader : public IncomingFramesReader {
 public:
  MockIncomingFramesReader(TaskRunner& service_thread,
                           NearbyConnection* connection)
      : IncomingFramesReader(service_thread, connection) {}

  MOCK_METHOD(void, ReadFrame,
              (std::function<void(std::optional<V1Frame>)> callback),
              (override));

  MOCK_METHOD(void, ReadFrame,
              (service::proto::V1Frame_FrameType frame_type,
               std::function<void(std::optional<V1Frame>)> callback,
               absl::Duration timeout),
              (override));
};

PairedKeyVerificationRunner::PairedKeyVerificationResult Merge(
    PairedKeyVerificationRunner::PairedKeyVerificationResult local_result,
    PairedKeyResultFrame::Status remote_result) {
  if (remote_result == PairedKeyResultFrame::FAIL ||
      local_result ==
          PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail) {
    return PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail;
  }

  if (remote_result == PairedKeyResultFrame::SUCCESS &&
      local_result ==
          PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess) {
    return PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess;
  }

  return PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable;
}

class PairedKeyVerificationRunnerTest : public testing::Test {
 public:
  enum class ReturnFrameType {
    // Return absl::nullopt for the frame.
    kNull,
    // Return an empty frame.
    kEmpty,
    // Return a valid frame.
    kValid,
    // Return a valid optional frame.
    kOptionalValid,
    // Return an invalid frame with both signed signature.
    kInValid,
  };

  PairedKeyVerificationRunnerTest()
      : frames_reader_(fake_task_runner_, &connection_) {}

  void SetUp() override {
    GetFakeClock()->FastForward(absl::Minutes(15));
  }

  void RunVerification(
      bool is_incoming, bool use_valid_public_certificate,
      const PairedKeyVerificationRunner::VisibilityHistory& visibility_history,
      PairedKeyVerificationRunner::PairedKeyVerificationResult expected_result,
      OSType expected_os_type = OSType::UNKNOWN_OS_TYPE) {
    std::optional<NearbyShareDecryptedPublicCertificate> public_certificate =
        use_valid_public_certificate
            ? std::make_optional<NearbyShareDecryptedPublicCertificate>(
                  GetNearbyShareTestDecryptedPublicCertificate())
            : std::nullopt;

    auto runner = std::make_shared<PairedKeyVerificationRunner>(
        &fake_clock_, OSType::WINDOWS, is_incoming, visibility_history,
        GetAuthToken(), &connection_, std::move(public_certificate),
        &certificate_manager_, &frames_reader_, kTimeout);

    runner->Run(
        [&, expected_result, expected_os_type](
            PairedKeyVerificationRunner::PairedKeyVerificationResult result,
            OSType remote_os_type) {
          EXPECT_EQ(expected_result, result);
          EXPECT_EQ(expected_os_type, remote_os_type);
        });
  }

  void SetUpPairedKeyEncryptionFrame(ReturnFrameType frame_type) {
    EXPECT_CALL(frames_reader_,
                ReadFrame(testing::Eq(V1Frame::PAIRED_KEY_ENCRYPTION),
                          testing::_, testing::Eq(kTimeout)))
        .WillOnce(testing::WithArg<1>(testing::Invoke(
            [frame_type](std::function<void(std::optional<V1Frame>)> callback) {
              if (frame_type == ReturnFrameType::kNull) {
                std::move(callback)(std::nullopt);
                return;
              }

              auto frame = V1Frame();

              if (frame_type == ReturnFrameType::kValid) {
                nearby::sharing::service::proto::PairedKeyEncryptionFrame*
                    encryption_frame = frame.mutable_paired_key_encryption();
                encryption_frame->set_signed_data(
                    GetIncomingConnectionSignedData().data(),
                    GetIncomingConnectionSignedData().size());
                encryption_frame->set_secret_id_hash(
                    GetPrivateCertificateHashAuthToken().data(),
                    GetPrivateCertificateHashAuthToken().size());
              } else if (frame_type == ReturnFrameType::kOptionalValid) {
                nearby::sharing::service::proto::PairedKeyEncryptionFrame*
                    encryption_frame = frame.mutable_paired_key_encryption();
                encryption_frame->set_signed_data(
                    GetInvalidIncomingConnectionSignedData().data(),
                    GetInvalidIncomingConnectionSignedData().size());
                encryption_frame->set_optional_signed_data(
                    GetIncomingConnectionSignedData().data(),
                    GetIncomingConnectionSignedData().size());
                encryption_frame->set_secret_id_hash(
                    GetPrivateCertificateHashAuthToken().data(),
                    GetPrivateCertificateHashAuthToken().size());
              } else if (frame_type == ReturnFrameType::kInValid) {
                nearby::sharing::service::proto::PairedKeyEncryptionFrame*
                    encryption_frame = frame.mutable_paired_key_encryption();
                encryption_frame->set_signed_data(
                    GetInvalidIncomingConnectionSignedData().data(),
                    GetInvalidIncomingConnectionSignedData().size());
                encryption_frame->set_optional_signed_data(
                    GetInvalidIncomingConnectionSignedData().data(),
                    GetInvalidIncomingConnectionSignedData().size());
                encryption_frame->set_secret_id_hash(
                    GetPrivateCertificateHashAuthToken().data(),
                    GetPrivateCertificateHashAuthToken().size());
              } else {
                nearby::sharing::service::proto::PairedKeyEncryptionFrame*
                    encryption_frame = frame.mutable_paired_key_encryption();
                encryption_frame->clear_signed_data();
                encryption_frame->clear_secret_id_hash();
              }

              std::move(callback)(std::move(frame));
            })));
  }

  void SetUpPairedKeyResultFrame(
      ReturnFrameType frame_type,
      PairedKeyResultFrame::Status status = PairedKeyResultFrame::UNKNOWN,
      OSType os_type = OSType::UNKNOWN_OS_TYPE) {
    EXPECT_CALL(frames_reader_,
                ReadFrame(testing::Eq(V1Frame::PAIRED_KEY_RESULT), testing::_,
                          testing::Eq(kTimeout)))
        .WillOnce(testing::WithArg<1>(testing::Invoke(
            [=](std::function<void(std::optional<V1Frame>)> callback) {
              if (frame_type == ReturnFrameType::kNull) {
                std::move(callback)(std::nullopt);
                return;
              }

              auto frame = V1Frame();

              PairedKeyResultFrame* result_frame =
                  frame.mutable_paired_key_result();

              result_frame->set_status(status);
              result_frame->set_os_type(os_type);

              std::move(callback)(std::move(frame));
            })));
  }

  nearby::sharing::service::proto::Frame GetWrittenFrame() {
    std::vector<uint8_t> data = connection_.GetWrittenData();
    nearby::sharing::service::proto::Frame frame;
    frame.ParseFromArray(data.data(), data.size());
    return frame;
  }

  void ExpectPairedKeyEncryptionFrameSent() {
    nearby::sharing::service::proto::Frame frame = GetWrittenFrame();
    ASSERT_TRUE(frame.has_v1());
    ASSERT_TRUE(frame.v1().has_paired_key_encryption());
  }

  void ExpectPairedKeyResultFrameSent(PairedKeyResultFrame::Status status) {
    nearby::sharing::service::proto::Frame frame = GetWrittenFrame();
    ASSERT_TRUE(frame.has_v1());
    ASSERT_TRUE(frame.v1().has_paired_key_result());
    EXPECT_EQ(status, frame.v1().paired_key_result().status());
  }

  FakeClock* GetFakeClock() { return &fake_clock_; }

 private:
  FakeClock fake_clock_;
  FakeTaskRunner fake_task_runner_ {&fake_clock_, 1};
  FakeNearbyConnection connection_;
  testing::NiceMock<MockIncomingFramesReader> frames_reader_;
  FakeNearbyShareCertificateManager certificate_manager_;
};

TEST_F(PairedKeyVerificationRunnerTest,
       NullCertificate_InvalidPairedKeyEncryptionFrame) {
  // Empty key encryption frame fails the certificate verification.
  SetUpPairedKeyEncryptionFrame(ReturnFrameType::kEmpty);
  SetUpPairedKeyResultFrame(ReturnFrameType::kValid);

  RunVerification(
      true,
      /*use_valid_public_certificate=*/false,
      {.visibility = DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS,
       .last_visibility = DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS,
       .last_visibility_time = GetFakeClock()->Now()},
      /*expected_result=*/
      PairedKeyVerificationResult::kUnable);

  ExpectPairedKeyEncryptionFrameSent();
  ExpectPairedKeyResultFrameSent(PairedKeyResultFrame::UNABLE);
}

TEST_F(PairedKeyVerificationRunnerTest,
       ValidPairedKeyEncryptionFrame_ResultFrameTimedOut) {
  SetUpPairedKeyEncryptionFrame(ReturnFrameType::kValid);

  // Null result frame fails the certificate verification process.
  SetUpPairedKeyResultFrame(ReturnFrameType::kNull);

  RunVerification(
      true,
      /*use_valid_public_certificate=*/true,
      {.visibility = DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS,
       .last_visibility = DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS,
       .last_visibility_time = GetFakeClock()->Now()},
      /*expected_result=*/
      PairedKeyVerificationResult::kFail);

  ExpectPairedKeyEncryptionFrameSent();
  ExpectPairedKeyResultFrameSent(PairedKeyResultFrame::SUCCESS);
}

struct TestParameters {
  bool is_incoming;
  bool has_valid_certificate;
  PairedKeyVerificationRunnerTest::ReturnFrameType encryption_frame_type;
  PairedKeyVerificationRunner::PairedKeyVerificationResult result;
} kParameters[] = {
    {true, true, PairedKeyVerificationRunnerTest::ReturnFrameType::kNull,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail},
    {true, true, PairedKeyVerificationRunnerTest::ReturnFrameType::kEmpty,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail},
    {true, true, PairedKeyVerificationRunnerTest::ReturnFrameType::kValid,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess},
    {true, true,
     PairedKeyVerificationRunnerTest::ReturnFrameType::kOptionalValid,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess},
    {true, true, PairedKeyVerificationRunnerTest::ReturnFrameType::kInValid,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail},
    {true, false, PairedKeyVerificationRunnerTest::ReturnFrameType::kNull,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail},
    {true, false, PairedKeyVerificationRunnerTest::ReturnFrameType::kEmpty,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable},
    {true, false, PairedKeyVerificationRunnerTest::ReturnFrameType::kValid,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable},
    {true, false,
     PairedKeyVerificationRunnerTest::ReturnFrameType::kOptionalValid,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable},
    {true, false, PairedKeyVerificationRunnerTest::ReturnFrameType::kInValid,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable},
};

using KeyVerificationTestParam =
    std::tuple<TestParameters, PairedKeyResultFrame,
               PairedKeyVerificationRunner::VisibilityHistory>;

class ParameterisedPairedKeyVerificationRunnerTest
    : public PairedKeyVerificationRunnerTest,
      public testing::WithParamInterface<KeyVerificationTestParam> {};

TEST_P(ParameterisedPairedKeyVerificationRunnerTest,
       ValidEncryptionFrame_ValidResultFrame) {
  const TestParameters& params = std::get<0>(GetParam());
  PairedKeyResultFrame result_frame = std::get<1>(GetParam());
  PairedKeyVerificationRunner::VisibilityHistory visibility_history =
      std::get<2>(GetParam());
  PairedKeyVerificationRunner::PairedKeyVerificationResult expected_result =
      Merge(params.result, result_frame.status());

  NL_LOG(ERROR) << "ValidEncryptionFrame_ValidResultFrame: " << "is_incoming="
                << params.is_incoming
                << ", has_valid_cert=" << params.has_valid_certificate
                << ", encryption_frame_type="
                << (int)params.encryption_frame_type
                << ", result=" << (int)params.result
                << ", expected_result=" << (int)expected_result
                << ", result_frame=" << (int)result_frame.status()
                << ", visibility=" << (int)visibility_history.visibility
                << ", last_visibility="
                << (int)visibility_history.last_visibility;

  SetUpPairedKeyEncryptionFrame(params.encryption_frame_type);
  bool encryption_frame_timeout =
      params.encryption_frame_type ==
      PairedKeyVerificationRunnerTest::ReturnFrameType::kNull;
  if (!encryption_frame_timeout) {
    // Result frame is only expected if Encryption frame read does not time out.
    SetUpPairedKeyResultFrame(
        PairedKeyVerificationRunnerTest::ReturnFrameType::kValid,
        result_frame.status(),
        result_frame.has_os_type() ? result_frame.os_type()
                                   : OSType::UNKNOWN_OS_TYPE);
  }

  // If our visibility has no certificates, then downgrade expected result to
  // kUnable if it is not expected to fail.
  if ((visibility_history.visibility ==
           DeviceVisibility::DEVICE_VISIBILITY_EVERYONE ||
       visibility_history.visibility ==
           DeviceVisibility::DEVICE_VISIBILITY_HIDDEN) &&
      (visibility_history.last_visibility ==
           DeviceVisibility::DEVICE_VISIBILITY_EVERYONE ||
       visibility_history.last_visibility ==
           DeviceVisibility::DEVICE_VISIBILITY_HIDDEN)) {
    if (expected_result ==
        PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess) {
      expected_result =
          PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable;
    }
  }
  visibility_history.last_visibility_time = GetFakeClock()->Now();
  RunVerification(
      /*is_incoming=*/params.is_incoming,
      /*use_valid_public_certificate=*/params.has_valid_certificate,
      visibility_history, expected_result,
      result_frame.has_os_type() && !encryption_frame_timeout
          ? result_frame.os_type()
          : OSType::UNKNOWN_OS_TYPE);

  ExpectPairedKeyEncryptionFrameSent();

  if (encryption_frame_timeout) {
    // If timed out waiting from PairedKeyEncryptionFrame, no result frame would
    // be sent.
    return;
  }

  // Check for result frame sent.
  if (!params.has_valid_certificate) {
    ExpectPairedKeyResultFrameSent(PairedKeyResultFrame::UNABLE);
    return;
  }

  if (params.encryption_frame_type ==
      PairedKeyVerificationRunnerTest::ReturnFrameType::kEmpty) {
    ExpectPairedKeyResultFrameSent(PairedKeyResultFrame::FAIL);
    return;
  }

  if (params.encryption_frame_type ==
      PairedKeyVerificationRunnerTest::ReturnFrameType::kInValid) {
    ExpectPairedKeyResultFrameSent(PairedKeyResultFrame::FAIL);
    return;
  }

  ExpectPairedKeyResultFrameSent(PairedKeyResultFrame::SUCCESS);
}

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/, ParameterisedPairedKeyVerificationRunnerTest,
    testing::Combine(testing::ValuesIn(kParameters),
                     testing::ValuesIn(GeneratePairedKeyResultFrame()),
                     testing::ValuesIn(GenerateVisibilityHistory())));

}  // namespace
}  // namespace sharing
}  // namespace nearby
