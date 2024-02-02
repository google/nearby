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
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/flags/nearby_flags.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_device_info.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/certificates/fake_nearby_share_certificate_manager.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/certificates/test_util.h"
#include "sharing/fake_nearby_connection.h"
#include "sharing/flags/generated/nearby_sharing_feature_flags.h"
#include "sharing/incoming_frames_reader.h"
#include "sharing/internal/public/context.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/internal/test/fake_preference_manager.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_sharing_decoder.h"
#include "sharing/nearby_sharing_decoder_impl.h"
#include "sharing/nearby_sharing_settings.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/rpc_resources.pb.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_target.h"

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

constexpr char kEndpointId[] = "test_endpoint_id";

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

const absl::Duration kTimeout = absl::Seconds(1);

class MockNearbyShareLocalDeviceDataManager
    : public NearbyShareLocalDeviceDataManager {
 public:
  MOCK_METHOD(std::string, GetId, (), (override));
  MOCK_METHOD(std::string, GetDeviceName, (), (override, const));
  MOCK_METHOD(std::optional<std::string>, GetFullName, (), (override, const));
  MOCK_METHOD(std::optional<std::string>, GetIconUrl, (), (override, const));
  MOCK_METHOD(DeviceNameValidationResult, ValidateDeviceName,
              (absl::string_view), (override));
  MOCK_METHOD(DeviceNameValidationResult, SetDeviceName, (absl::string_view),
              (override));
  MOCK_METHOD(void, DownloadDeviceData, (), (override));
  MOCK_METHOD(void, UploadContacts,
              (std::vector<nearby::sharing::proto::Contact>,
               UploadCompleteCallback),
              (override));
  MOCK_METHOD(void, UploadCertificates,
              (std::vector<nearby::sharing::proto::PublicCertificate>,
               UploadCompleteCallback),
              (override));
  MOCK_METHOD(void, OnStart, (), (override));
  MOCK_METHOD(void, OnStop, (), (override));
};

class MockIncomingFramesReader : public IncomingFramesReader {
 public:
  MockIncomingFramesReader(Context* context, NearbySharingDecoder* decoder,
                           NearbyConnection* connection)
      : IncomingFramesReader(context, decoder, connection) {}

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
      : frames_reader_(&context_, &decoder_, &connection_) {}

  void SetUp() override {
    nearby_share_settings_ = std::make_unique<NearbyShareSettings>(
        &context_, context_.GetClock(), fake_device_info_, preference_manager_,
        &local_device_data_manager_);
    nearby_share_settings_->SetVisibility(
        DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
    FastForward(absl::Minutes(15));
    share_target_.is_incoming = true;
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_sharing_feature::kEnableSelfShare, true);
  }

  void RunVerification(
      bool use_valid_public_certificate, bool restricted_to_contacts,
      PairedKeyVerificationRunner::PairedKeyVerificationResult expected_result,
      OSType expected_os_type = OSType::UNKNOWN_OS_TYPE) {
    std::optional<NearbyShareDecryptedPublicCertificate> public_certificate =
        use_valid_public_certificate
            ? std::make_optional<NearbyShareDecryptedPublicCertificate>(
                  GetNearbyShareTestDecryptedPublicCertificate())
            : std::nullopt;

    bool self_share_feature_enabled = NearbyFlags::GetInstance().GetBoolFlag(
        config_package_nearby::nearby_sharing_feature::kEnableSelfShare);
    auto runner = std::make_shared<PairedKeyVerificationRunner>(
        context_.GetClock(), fake_device_info_, nearby_share_settings_.get(),
        self_share_feature_enabled, share_target_, kEndpointId, GetAuthToken(),
        &connection_, std::move(public_certificate), &certificate_manager_,
        restricted_to_contacts, &frames_reader_, kTimeout);

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
            [this,
             frame_type](std::function<void(std::optional<V1Frame>)> callback) {
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
                // make sure the optional codes are executed
                nearby_share_settings_->SetVisibility(
                    DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
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

  void ExpectCertificateInfoSent() {}

  void ExpectPairedKeyResultFrameSent(PairedKeyResultFrame::Status status) {
    nearby::sharing::service::proto::Frame frame = GetWrittenFrame();
    ASSERT_TRUE(frame.has_v1());
    ASSERT_TRUE(frame.v1().has_paired_key_result());
    EXPECT_EQ(status, frame.v1().paired_key_result().status());
  }

  void FastForward(absl::Duration duration) {
    context_.fake_clock()->FastForward(duration);
  }

 protected:
  ShareTarget share_target_;

 private:
  nearby::FakePreferenceManager preference_manager_;
  FakeDeviceInfo fake_device_info_;
  FakeContext context_;
  FakeNearbyConnection connection_;
  NearbySharingDecoderImpl decoder_;
  testing::NiceMock<MockIncomingFramesReader> frames_reader_;
  FakeNearbyShareCertificateManager certificate_manager_;
  ::testing::NiceMock<MockNearbyShareLocalDeviceDataManager>
      local_device_data_manager_;
  std::unique_ptr<NearbyShareSettings> nearby_share_settings_;
};

TEST_F(PairedKeyVerificationRunnerTest,
       NullCertificate_InvalidPairedKeyEncryptionFrame_RestrictToContacts) {
  // Empty key encryption frame fails the certificate verification.
  SetUpPairedKeyEncryptionFrame(ReturnFrameType::kEmpty);

  RunVerification(
      /*use_valid_public_certificate=*/false,
      /*restricted_to_contacts=*/true,
      /*expected_result=*/
      PairedKeyVerificationResult::kFail);

  ExpectPairedKeyEncryptionFrameSent();
}

TEST_F(PairedKeyVerificationRunnerTest,
       ValidPairedKeyEncryptionFrame_ResultFrameTimedOut) {
  SetUpPairedKeyEncryptionFrame(ReturnFrameType::kValid);

  // Null result frame fails the certificate verification process.
  SetUpPairedKeyResultFrame(ReturnFrameType::kNull);

  RunVerification(
      /*use_valid_public_certificate=*/true,
      /*restricted_to_contacts=*/false,
      /*expected_result=*/
      PairedKeyVerificationResult::kFail);

  ExpectPairedKeyEncryptionFrameSent();
  ExpectPairedKeyResultFrameSent(PairedKeyResultFrame::UNABLE);
}

struct TestParameters {
  bool is_target_known;
  bool is_valid_certificate;
  PairedKeyVerificationRunnerTest::ReturnFrameType encryption_frame_type;
  PairedKeyVerificationRunner::PairedKeyVerificationResult result;
} kParameters[] = {
    {true, true, PairedKeyVerificationRunnerTest::ReturnFrameType::kValid,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess},
    {true, true, PairedKeyVerificationRunnerTest::ReturnFrameType::kEmpty,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail},
    {true, false, PairedKeyVerificationRunnerTest::ReturnFrameType::kValid,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable},
    {true, false, PairedKeyVerificationRunnerTest::ReturnFrameType::kEmpty,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable},
    {false, true, PairedKeyVerificationRunnerTest::ReturnFrameType::kValid,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable},
    {true, true,
     PairedKeyVerificationRunnerTest::ReturnFrameType::kOptionalValid,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess},
    {true, true, PairedKeyVerificationRunnerTest::ReturnFrameType::kInValid,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail},
};

using KeyVerificationTestParam =
    std::tuple<TestParameters, service::proto::PairedKeyResultFrame>;

class ParameterisedPairedKeyVerificationRunnerTest
    : public PairedKeyVerificationRunnerTest,
      public testing::WithParamInterface<KeyVerificationTestParam> {};

TEST_P(ParameterisedPairedKeyVerificationRunnerTest,
       ValidEncryptionFrame_ValidResultFrame) {
  const TestParameters& params = std::get<0>(GetParam());
  PairedKeyResultFrame result_frame = std::get<1>(GetParam());
  PairedKeyVerificationRunner::PairedKeyVerificationResult expected_result =
      Merge(params.result, result_frame.status());

  share_target_.is_known = params.is_target_known;

  SetUpPairedKeyEncryptionFrame(params.encryption_frame_type);
  SetUpPairedKeyResultFrame(
      PairedKeyVerificationRunnerTest::ReturnFrameType::kValid,
      result_frame.status(),
      result_frame.has_os_type() ? result_frame.os_type()
                                 : OSType::UNKNOWN_OS_TYPE);

  RunVerification(
      /*use_valid_public_certificate=*/params.is_valid_certificate,
      /*restricted_to_contacts=*/false, expected_result,
      result_frame.has_os_type() ? result_frame.os_type()
                                 : OSType::UNKNOWN_OS_TYPE);

  ExpectPairedKeyEncryptionFrameSent();
  if (params.encryption_frame_type ==
      PairedKeyVerificationRunnerTest::ReturnFrameType::kValid)
    ExpectCertificateInfoSent();

  // Check for result frame sent.
  if (!params.is_valid_certificate) {
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

  if (params.is_target_known) {
    ExpectPairedKeyResultFrameSent(PairedKeyResultFrame::SUCCESS);
  } else {
    ExpectPairedKeyResultFrameSent(PairedKeyResultFrame::UNABLE);
  }
}

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/, ParameterisedPairedKeyVerificationRunnerTest,
    testing::Combine(testing::ValuesIn(kParameters),
                     testing::ValuesIn(GeneratePairedKeyResultFrame())));

}  // namespace
}  // namespace sharing
}  // namespace nearby
