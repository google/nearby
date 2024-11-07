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

#include "sharing/nearby_sharing_service_impl.h"

#include <stdint.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <filesystem>  // NOLINT(build/c++17)
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/casts.h"
#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "internal/analytics/mock_event_logger.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/implementation/signin_attempt.h"
#include "internal/test/fake_account_manager.h"
#include "internal/test/fake_device_info.h"
#include "internal/test/fake_task_runner.h"
#include "internal/test/mock_account_observer.h"
#include "sharing/advertisement.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/attachment_container.h"
#include "sharing/certificates/fake_nearby_share_certificate_manager.h"
#include "sharing/certificates/nearby_share_certificate_manager_impl.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/certificates/test_util.h"
#include "sharing/common/compatible_u8_string.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/common/nearby_share_prefs.h"
#include "sharing/constants.h"
#include "sharing/contacts/fake_nearby_share_contact_manager.h"
#include "sharing/contacts/nearby_share_contact_manager_impl.h"
#include "sharing/fake_nearby_connection.h"
#include "sharing/fake_nearby_connections_manager.h"
#include "sharing/fast_initiation/fake_nearby_fast_initiation.h"
#include "sharing/fast_initiation/nearby_fast_initiation_impl.h"
#include "sharing/file_attachment.h"
#include "sharing/flags/generated/nearby_sharing_feature_flags.h"
#include "sharing/incoming_share_session.h"
#include "sharing/internal/api/mock_app_info.h"
#include "sharing/internal/api/mock_sharing_platform.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/public/connectivity_manager.h"
#include "sharing/internal/test/fake_bluetooth_adapter.h"
#include "sharing/internal/test/fake_connectivity_manager.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/internal/test/fake_preference_manager.h"
#include "sharing/internal/test/fake_wifi_adapter.h"
#include "sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager_impl.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/nearby_sharing_service.h"
#include "sharing/nearby_sharing_settings.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/rpc_resources.pb.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_target.h"
#include "sharing/share_target_discovered_callback.h"
#include "sharing/text_attachment.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_metadata_matchers.h"
#include "sharing/transfer_update_callback.h"
#include "sharing/wifi_credentials_attachment.h"
#include "google/protobuf/repeated_ptr_field.h"

namespace nearby {
namespace sharing {
namespace {

using ConnectionType = ::nearby::ConnectivityManager::ConnectionType;
using SendSurfaceState =
    ::nearby::sharing::NearbySharingService::SendSurfaceState;
using ::nearby::sharing::api::MockAppInfo;
using ::nearby::sharing::api::PreferenceManager;
using ::nearby::sharing::proto::DataUsage;
using ::nearby::sharing::proto::DeviceVisibility;
using ::nearby::sharing::proto::FastInitiationNotificationState;
using ::nearby::sharing::service::proto::ConnectionResponseFrame;
using ::nearby::sharing::service::proto::FileMetadata;
using ::nearby::sharing::service::proto::Frame;
using ::nearby::sharing::service::proto::IntroductionFrame;
using ::nearby::sharing::service::proto::PairedKeyResultFrame;
using ::nearby::sharing::service::proto::TextMetadata;
using ::nearby::sharing::service::proto::V1Frame;
using ::testing::_;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;

class MockTransferUpdateCallback : public TransferUpdateCallback {
 public:
  ~MockTransferUpdateCallback() override = default;

  MOCK_METHOD(void, OnTransferUpdate,
              (const ShareTarget& shareTarget,
               const AttachmentContainer& attachmentContainer,
               const TransferMetadata& transferMetadata),
              (override));
};

class MockShareTargetDiscoveredCallback : public ShareTargetDiscoveredCallback {
 public:
  ~MockShareTargetDiscoveredCallback() override = default;

  MOCK_METHOD(void, OnShareTargetDiscovered, (const ShareTarget& share_target),
              (override));
  MOCK_METHOD(void, OnShareTargetLost, (const ShareTarget& share_target),
              (override));
  MOCK_METHOD(void, OnShareTargetUpdated, (const ShareTarget& share_target),
              (override));
};

}  // namespace

namespace NearbySharingServiceUnitTests {

constexpr absl::Duration kDelta = absl::Milliseconds(100);

// Used to wait for absl::Notification to finish.
constexpr absl::Duration kWaitTimeout = absl::Milliseconds(500);
constexpr absl::Duration kTaskWaitTimeout = absl::Seconds(2);

constexpr char kServiceId[] = "NearbySharing";
constexpr char kDeviceName[] = "test_device_name";
constexpr ShareTargetType kDeviceType = ShareTargetType::kPhone;
constexpr char kEndpointId[] = "test_endpoint_id";
constexpr char kTextPayload[] = "Test text payload";
constexpr char kFourDigitToken[] = "1953";
constexpr absl::string_view kTestAccountId = "test_account";
constexpr uint8_t kVendorId = 0;

constexpr int64_t kFreeDiskSpace = 10000;

const std::vector<uint8_t>& GetToken() {
  static std::vector<uint8_t>* token = new std::vector<uint8_t>({0, 1, 2});
  return *token;
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

const std::vector<uint8_t>& GetOutgoingConnectionSignedData() {
  static std::vector<uint8_t>* outgoing_connection_signed_data =
      new std::vector<uint8_t>(
          {0x30, 0x45, 0x02, 0x21, 0x00, 0xf9, 0xc9, 0xa8, 0x89, 0x96, 0x6e,
           0x5c, 0xea, 0x0a, 0x60, 0x37, 0x3a, 0x84, 0x7d, 0xf5, 0x31, 0x82,
           0x74, 0xb9, 0xde, 0x3f, 0x64, 0x1b, 0xff, 0x4f, 0x54, 0x31, 0x1f,
           0x9e, 0x63, 0x68, 0xca, 0x02, 0x20, 0x52, 0x43, 0x46, 0xa7, 0x6f,
           0xcb, 0x96, 0x50, 0x86, 0xfd, 0x6f, 0x9f, 0x7e, 0x50, 0xa7, 0xa0,
           0x9b, 0xdf, 0xae, 0x79, 0x42, 0x47, 0xd9, 0x60, 0x71, 0x91, 0x7a,
           0xbb, 0x81, 0x9b, 0x0d, 0x2e});
  return *outgoing_connection_signed_data;
}

constexpr int kFilePayloadId = 111;
constexpr int kPayloadSize = 1000000;

const std::vector<uint8_t>& GetValidIntroductionFramePayloadIds() {
  static std::vector<uint8_t>* valid_introduction_frame_payload_ids =
      new std::vector<uint8_t>({1, 2, 3, kFilePayloadId});
  return *valid_introduction_frame_payload_ids;
}

constexpr size_t kMaxCertificateDownloadsDuringDiscovery = 3u;
constexpr absl::Duration kCertificateDownloadDuringDiscoveryPeriod =
    absl::Seconds(10);

std::unique_ptr<Payload> GetFilePayload(int64_t payload_id) {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() / absl::StrCat(payload_id);
  InputFile input_file{path};
  return std::make_unique<Payload>(input_file);
}

std::unique_ptr<Payload> GetTextPayload(int64_t payload_id,
                                        absl::string_view text) {
  return std::make_unique<Payload>(
      std::vector<uint8_t>(text.begin(), text.end()));
}

std::unique_ptr<Frame> GetValidIntroductionFrame() {
  IntroductionFrame* introduction_frame =
      IntroductionFrame::default_instance().New();
  auto text_metadatas = introduction_frame->mutable_text_metadata();
  introduction_frame->set_start_transfer(true);

  for (int i = 1; i <= 3; ++i) {
    nearby::sharing::service::proto::TextMetadata* text_metadata =
        nearby::sharing::service::proto::TextMetadata::default_instance().New();

    text_metadata->set_text_title(absl::StrCat("title ", i));
    text_metadata->set_type(
        static_cast<nearby::sharing::service::proto::TextMetadata::Type>(i));
    text_metadata->set_payload_id(i);
    text_metadata->set_size(kPayloadSize);
    text_metadata->set_id(i);

    text_metadatas->AddAllocated(text_metadata);
  }

  auto file_metadatas = introduction_frame->mutable_file_metadata();

  nearby::sharing::service::proto::FileMetadata* file_metadata =
      nearby::sharing::service::proto::FileMetadata::default_instance().New();
  file_metadata->set_name("unit_test_nearby_share_name_\x80");
  file_metadata->set_type(nearby::sharing::service::proto::FileMetadata::VIDEO);
  file_metadata->set_payload_id(kFilePayloadId);
  file_metadata->set_size(kPayloadSize);
  file_metadata->set_mime_type("mime type");
  file_metadata->set_id(100);
  file_metadatas->AddAllocated(file_metadata);

  V1Frame* v1_frame = V1Frame::default_instance().New();
  v1_frame->set_type(V1Frame::INTRODUCTION);
  v1_frame->set_allocated_introduction(introduction_frame);

  Frame* frame = Frame::default_instance().New();
  frame->set_version(Frame::V1);
  frame->set_allocated_v1(std::move(v1_frame));
  return std::unique_ptr<Frame>(frame);
}

std::unique_ptr<Frame> GetEmptyIntroductionFrame() {
  V1Frame* v1_frame = V1Frame::default_instance().New();
  v1_frame->set_type(V1Frame::INTRODUCTION);
  v1_frame->set_allocated_introduction(
      IntroductionFrame::default_instance().New());

  Frame* frame = Frame::default_instance().New();
  frame->set_version(Frame::V1);
  frame->set_allocated_v1(v1_frame);
  return std::unique_ptr<Frame>(frame);
}

std::unique_ptr<Frame> GetConnectionResponseFrame(
    ConnectionResponseFrame::Status status) {
  V1Frame* v1_frame = V1Frame::default_instance().New();
  v1_frame->set_type(V1Frame::RESPONSE);
  ConnectionResponseFrame* response_frame =
      ConnectionResponseFrame::default_instance().New();
  response_frame->set_status(status);
  v1_frame->set_allocated_connection_response(response_frame);

  Frame* frame = Frame::default_instance().New();
  frame->set_version(Frame::V1);
  frame->set_allocated_v1(v1_frame);
  return std::unique_ptr<Frame>(frame);
}

std::unique_ptr<Frame> GetCancelFrame() {
  V1Frame* v1_frame = V1Frame::default_instance().New();
  v1_frame->set_type(V1Frame::CANCEL);

  Frame* frame = Frame::default_instance().New();
  frame->set_version(Frame::V1);
  frame->set_allocated_v1(v1_frame);
  return std::unique_ptr<Frame>(frame);
}

std::unique_ptr<AttachmentContainer> CreateTextAttachments(
    std::vector<std::string> texts) {
  auto attachment_container = std::make_unique<AttachmentContainer>();
  for (auto& text : texts) {
    attachment_container->AddTextAttachment(
        TextAttachment(service::proto::TextMetadata::TEXT, std::move(text),
                       /*text_title=*/std::nullopt,
                       /*mime_type=*/std::nullopt));
  }
  return attachment_container;
}

std::unique_ptr<AttachmentContainer> CreateFileAttachments(
    std::vector<std::filesystem::path> file_paths) {
  auto attachment_container = std::make_unique<AttachmentContainer>();
  for (auto& file_path : file_paths) {
    attachment_container->AddFileAttachment(
        FileAttachment(std::move(file_path)));
  }
  return attachment_container;
}

std::unique_ptr<AttachmentContainer> CreateWifiCredentialAttachments(
    std::string ssid, std::string password) {
  auto attachment_container = std::make_unique<AttachmentContainer>();
  attachment_container->AddWifiCredentialsAttachment(WifiCredentialsAttachment(
      std::move(ssid), service::proto::WifiCredentialsMetadata::WPA_PSK,
      std::move(password)));
  return attachment_container;
}

class NearbySharingServiceImplTest : public testing::Test {
 public:
  NearbySharingServiceImplTest() = default;
  ~NearbySharingServiceImplTest() override = default;

  void SetUp() override {
    ON_CALL(mock_sharing_platform_, GetDeviceInfo)
        .WillByDefault(ReturnRef(fake_device_info_));
    ON_CALL(mock_sharing_platform_, GetPreferenceManager)
        .WillByDefault(ReturnRef(preference_manager_));
    ON_CALL(mock_sharing_platform_, GetAccountManager)
        .WillByDefault(ReturnRef(fake_account_manager_));
    ON_CALL(mock_sharing_platform_, UpdateFileOriginMetadata)
        .WillByDefault(Return(true));
    auto mock_app_info = std::make_unique<StrictMock<MockAppInfo>>();
    mock_app_info_ = mock_app_info.get();
    EXPECT_CALL(mock_sharing_platform_, CreateAppInfo())
        .WillOnce(Return(std::move(mock_app_info)));
    NearbyShareLocalDeviceDataManagerImpl::Factory::SetFactoryForTesting(
        &local_device_data_manager_factory_);
    NearbyShareContactManagerImpl::Factory::SetFactoryForTesting(
        &contact_manager_factory_);
    NearbyShareCertificateManagerImpl::Factory::SetFactoryForTesting(
        &certificate_manager_factory_);
    nearby_fast_initiation_factory_ =
        std::make_unique<FakeNearbyFastInitiation::Factory>();
    NearbyFastInitiationImpl::Factory::SetFactoryForTesting(
        nearby_fast_initiation_factory_.get());
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_sharing_feature::kEnableMediumWifiLan,
        true);

    prefs::RegisterNearbySharingPrefs(preference_manager_);
    auto fake_task_runner =
        std::make_unique<FakeTaskRunner>(fake_context_.fake_clock(), 1);
    sharing_service_task_runner_ = fake_task_runner.get();
    connection_ =
        std::make_unique<FakeNearbyConnection>(fake_task_runner.get());
    SetBluetoothIsPresent(true);
    SetBluetoothIsPowered(true);
    SetScreenLocked(false);
    SetConnectionType(ConnectionType::kWifi);
    analytics_recorder_ = std::make_unique<analytics::AnalyticsRecorder>(
        /*vendor_id=*/0, /*event_logger=*/nullptr);

    service_ = CreateService(std::move(fake_task_runner));
  }

  void TearDown() override {
    if (service_) Shutdown();
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_sharing_feature::kEnableMediumWifiLan,
        true);
    NearbyShareLocalDeviceDataManagerImpl::Factory::SetFactoryForTesting(
        nullptr);
    NearbyShareContactManagerImpl::Factory::SetFactoryForTesting(nullptr);
    NearbyShareCertificateManagerImpl::Factory::SetFactoryForTesting(nullptr);
    NearbyFlags::GetInstance().ResetOverridedValues();
    nearby_fast_initiation_factory_.reset();
  }

  void SetConnectionType(ConnectionType type) {
    FakeConnectivityManager* connectivity_manager =
        down_cast<FakeConnectivityManager*>(
            fake_context_.GetConnectivityManager());
    connectivity_manager->SetConnectionType(type);
  }

  std::unique_ptr<NearbySharingServiceImpl> CreateService(
      std::unique_ptr<FakeTaskRunner> task_runner) {
    fake_nearby_connections_manager_ = new FakeNearbyConnectionsManager();
    return std::make_unique<NearbySharingServiceImpl>(
        std::move(task_runner), &fake_context_, mock_sharing_platform_,
        absl::WrapUnique(fake_nearby_connections_manager_),
        analytics_recorder_.get());
  }

  void SetVisibility(DeviceVisibility visibility) {
    service_->GetSettings()->SetVisibility(visibility);
    FlushTesting();
  }

  void SetFastInitiationNotificationState(
      FastInitiationNotificationState state) {
    service_->GetSettings()->SetFastInitiationNotificationState(state);
    FlushTesting();
  }

  void SetBluetoothIsPresent(bool present) {
    FakeBluetoothAdapter& bluetooth_adapter =
        down_cast<FakeBluetoothAdapter&>(fake_context_.GetBluetoothAdapter());
    bluetooth_adapter.ReceivedAdapterPresentChangedFromOs(present);
    FlushTesting();
  }

  void SetBluetoothIsPowered(bool powered) {
    FakeBluetoothAdapter& bluetooth_adapter =
        down_cast<FakeBluetoothAdapter&>(fake_context_.GetBluetoothAdapter());
    bluetooth_adapter.ReceivedAdapterPoweredChangedFromOs(powered);
    FlushTesting();
  }

  void SetWifiIsPresent(bool present) {
    FakeWifiAdapter& wifi_adapter =
        down_cast<FakeWifiAdapter&>(fake_context_.GetWifiAdapter());
    wifi_adapter.ReceivedAdapterPresentChangedFromOs(present);
    FlushTesting();
  }

  void SetWifiIsPowered(bool powered) {
    FakeWifiAdapter& wifi_adapter =
        down_cast<FakeWifiAdapter&>(fake_context_.GetWifiAdapter());
    wifi_adapter.ReceivedAdapterPoweredChangedFromOs(powered);
    FlushTesting();
  }

  void SetLanIsConnected(bool connected) {
    FakeConnectivityManager* connectivity_manager =
        down_cast<FakeConnectivityManager*>(
            fake_context_.GetConnectivityManager());
    connectivity_manager->SetLanConnected(connected);
    FlushTesting();
  }

  void FastForward(absl::Duration duration) {
    fake_context_.fake_clock()->FastForward(duration);
    FlushTesting();
  }

  void SetScreenLocked(bool locked) {
    fake_device_info_.SetScreenLocked(locked);
    FlushTesting();
  }

  NearbySharingService::StatusCodes RegisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      ShareTargetDiscoveredCallback* discovery_callback, SendSurfaceState state,
      Advertisement::BlockedVendorId vendor_id =
          static_cast<Advertisement::BlockedVendorId>(kVendorId)) {
    NearbySharingService::StatusCodes result =
        NearbySharingService::StatusCodes::kError;
    absl::Notification notification;
    service_->RegisterSendSurface(
        transfer_callback, discovery_callback, state, vendor_id,
        /*disable_wifi_hotspot=*/false,
        [&](NearbySharingService::StatusCodes status_codes) {
          result = status_codes;
          notification.Notify();
        });

    EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kTaskWaitTimeout));
    return result;
  }

  NearbySharingService::StatusCodes UnregisterSendSurface(
      TransferUpdateCallback* transfer_callback) {
    NearbySharingService::StatusCodes result =
        NearbySharingService::StatusCodes::kError;
    absl::Notification notification;
    service_->UnregisterSendSurface(
        transfer_callback, [&](NearbySharingService::StatusCodes status_codes) {
          result = status_codes;
          notification.Notify();
        });

    EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kTaskWaitTimeout));
    return result;
  }

  NearbySharingService::StatusCodes RegisterReceiveSurface(
      TransferUpdateCallback* transfer_callback,
      NearbySharingService::ReceiveSurfaceState state,
      uint8_t vendor_id = kVendorId) {
    NearbySharingService::StatusCodes result =
        NearbySharingService::StatusCodes::kError;
    absl::Notification notification;
    service_->RegisterReceiveSurface(
        transfer_callback, state,
        static_cast<Advertisement::BlockedVendorId>(vendor_id),
        [&](NearbySharingService::StatusCodes status_codes) {
          result = status_codes;
          notification.Notify();
        });

    EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kTaskWaitTimeout));
    return result;
  }

  NearbySharingService::StatusCodes UnregisterReceiveSurface(
      TransferUpdateCallback* transfer_callback) {
    NearbySharingService::StatusCodes result =
        NearbySharingService::StatusCodes::kError;
    absl::Notification notification;
    service_->UnregisterReceiveSurface(
        transfer_callback, [&](NearbySharingService::StatusCodes status_codes) {
          result = status_codes;
          notification.Notify();
        });

    EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kTaskWaitTimeout));
    return result;
  }

  NearbySharingService::StatusCodes SendAttachments(
      int64_t share_target_id,
      std::unique_ptr<AttachmentContainer> attachment_container) {
    NearbySharingService::StatusCodes result =
        NearbySharingService::StatusCodes::kError;
    absl::Notification notification;
    service_->SendAttachments(
        share_target_id, std::move(attachment_container),
        [&](NearbySharingService::StatusCodes status_codes) {
          result = status_codes;
          notification.Notify();
        });

    EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kTaskWaitTimeout));
    return result;
  }

  void Shutdown() {
    NearbySharingService::StatusCodes result =
        NearbySharingService::StatusCodes::kError;
    absl::Notification notification;
    service_->Shutdown([&](NearbySharingService::StatusCodes status_codes) {
      result = status_codes;
      notification.Notify();
    });

    EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kTaskWaitTimeout));
    EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  }

  void SetUpForegroundReceiveSurface(
      NiceMock<MockTransferUpdateCallback>& callback) {
    NearbySharingService::StatusCodes result = RegisterReceiveSurface(
        &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
    EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
    EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  }

  void SetUpBackgroundReceiveSurface(
      NiceMock<MockTransferUpdateCallback>& callback) {
    NearbySharingService::StatusCodes result = RegisterReceiveSurface(
        &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
    EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
    EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  }

  void ProcessLatestPublicCertificateDecryption(size_t expected_num_calls,
                                                bool success,
                                                bool for_self_share = false) {
    // Ensure that all pending mojo messages are processed and the certificate
    // manager state is as expected up to this point.
    std::vector<
        FakeNearbyShareCertificateManager::GetDecryptedPublicCertificateCall>&
        calls = certificate_manager()->get_decrypted_public_certificate_calls();

    ASSERT_FALSE(calls.empty());
    EXPECT_EQ(calls.size(), expected_num_calls);
    EXPECT_EQ(GetNearbyShareTestEncryptedMetadataKey().salt(),
              calls.back().encrypted_metadata_key.salt());
    EXPECT_EQ(GetNearbyShareTestEncryptedMetadataKey().encrypted_key(),
              calls.back().encrypted_metadata_key.encrypted_key());

    if (success) {
      nearby::sharing::proto::PublicCertificate cert =
          GetNearbyShareTestPublicCertificate(
              DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
      cert.set_for_self_share(for_self_share);
      std::move(calls.back().callback)(
          NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
              cert, GetNearbyShareTestEncryptedMetadataKey()));
    } else {
      std::move(calls.back().callback)(std::nullopt);
    }
    FlushTesting();
  }

  void SetUpKeyVerification(bool is_incoming,
                            PairedKeyResultFrame::Status status) {
    SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
    local_device_data_manager()->SetDeviceName(kDeviceName);

    Frame frame;
    frame.set_version(Frame::V1);
    V1Frame* v1_frame = frame.mutable_v1();
    v1_frame->set_type(V1Frame::PAIRED_KEY_ENCRYPTION);
    nearby::sharing::service::proto::PairedKeyEncryptionFrame*
        paired_key_encryption_frame =
            nearby::sharing::service::proto::PairedKeyEncryptionFrame::
                default_instance()
                    .New();
    paired_key_encryption_frame->set_signed_data(
        is_incoming ? std::string(GetIncomingConnectionSignedData().begin(),
                                  GetIncomingConnectionSignedData().end())
                    : std::string(GetOutgoingConnectionSignedData().begin(),
                                  GetOutgoingConnectionSignedData().end()));
    paired_key_encryption_frame->set_secret_id_hash(
        std::string(GetPrivateCertificateHashAuthToken().begin(),
                    GetPrivateCertificateHashAuthToken().end()));
    v1_frame->set_allocated_paired_key_encryption(paired_key_encryption_frame);
    std::vector<uint8_t> encryption_bytes(frame.ByteSizeLong());
    frame.SerializeToArray(encryption_bytes.data(), encryption_bytes.size());

    connection_->AppendReadableData(encryption_bytes);
    FlushTesting();

    Frame result_frame;
    result_frame.set_version(Frame::V1);
    V1Frame* result_v1_frame = result_frame.mutable_v1();
    result_v1_frame->set_type(V1Frame::PAIRED_KEY_RESULT);
    PairedKeyResultFrame* paired_key_result_frame =
        result_v1_frame->mutable_paired_key_result();
    paired_key_result_frame->set_status(status);
    std::vector<uint8_t> result_bytes(result_frame.ByteSizeLong());
    result_frame.SerializeToArray(result_bytes.data(), result_bytes.size());

    connection_->AppendReadableData(result_bytes);
    FlushTesting();
  }

  std::vector<uint8_t> CreateTestEndpointInfo(uint8_t vendor_id = kVendorId) {
    std::unique_ptr<Advertisement> advertisement = Advertisement::NewInstance(
        GetNearbyShareTestEncryptedMetadataKey().salt(),
        GetNearbyShareTestEncryptedMetadataKey().encrypted_key(), kDeviceType,
        kDeviceName, vendor_id);
    return advertisement->ToEndpointInfo();
  }

  void SetUpIntroductionFrameDecoder(bool return_empty_introduction_frame) {
    std::unique_ptr<Frame> frame;
    if (return_empty_introduction_frame) {
      frame = GetEmptyIntroductionFrame();
    } else {
      frame = GetValidIntroductionFrame();
    }
    std::vector<uint8_t> bytes(frame->ByteSizeLong());
    frame->SerializeToArray(bytes.data(), bytes.size());

    connection_->AppendReadableData(bytes);
    FlushTesting();
  }

  void SendConnectionResponse(ConnectionResponseFrame::Status status) {
    std::unique_ptr<Frame> frame = GetConnectionResponseFrame(status);
    std::vector<uint8_t> bytes(frame->ByteSizeLong());
    frame->SerializeToArray(bytes.data(), bytes.size());
    connection_->AppendReadableData(bytes);
    FlushTesting();
  }

  void SendCancel() {
    std::unique_ptr<Frame> frame = GetCancelFrame();
    std::vector<uint8_t> bytes(frame->ByteSizeLong());
    frame->SerializeToArray(bytes.data(), bytes.size());
    connection_->AppendReadableData(bytes);
    FlushTesting();
  }

  int64_t SetUpIncomingConnection(
      NiceMock<MockTransferUpdateCallback>& callback, bool is_foreground = true,
      bool for_self_share = false) {
    fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                                GetToken());

    SetUpIntroductionFrameDecoder(/*return_empty_introduction_frame=*/false);

    int64_t share_target_id;
    SetConnectionType(ConnectionType::kWifi);
    absl::Notification notification;
    EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
        .WillOnce(testing::Invoke([&](const ShareTarget& incoming_share_target,
                                      const AttachmentContainer& container,
                                      const TransferMetadata& metadata) {
          EXPECT_FALSE(metadata.is_final_status());
          TransferMetadata::Status expected_status;

          if (for_self_share) {
            expected_status =
                TransferMetadata::Status::kAwaitingRemoteAcceptance;
          } else {
            expected_status =
                TransferMetadata::Status::kAwaitingLocalConfirmation;
          }

          EXPECT_EQ(metadata.status(), expected_status);
          share_target_id = incoming_share_target.id;
          notification.Notify();
        }));

    SetUpKeyVerification(
        /*is_incoming=*/true, PairedKeyResultFrame::SUCCESS);
    if (is_foreground) {
      SetUpForegroundReceiveSurface(callback);
    } else {
      SetUpBackgroundReceiveSurface(callback);
    }
    EXPECT_CALL(*mock_app_info_, SetActiveFlag());
    service_->OnIncomingConnection(kEndpointId, CreateTestEndpointInfo(),
                                   connection_.get());
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                             /*success=*/true, for_self_share);
    EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
    return share_target_id;
  }

  void AcceptConnection(NiceMock<MockTransferUpdateCallback>& callback,
                        int64_t share_target_id,
                        absl::string_view endpoint_id) {
    // After accepting the transfer, we get another update of
    // kAwaitingRemoteAcceptance.
    EXPECT_CALL(callback,
                OnTransferUpdate(
                    testing::_, testing::_,
                    nearby::sharing::HasStatus(
                        TransferMetadata::Status::kAwaitingRemoteAcceptance)));
    absl::Notification accept_notification;
    service_->Accept(
        share_target_id,
        [&accept_notification](
            nearby::sharing::NearbySharingService::StatusCodes status_code) {
          EXPECT_EQ(status_code, NearbySharingServiceImpl::StatusCodes::kOk);
          accept_notification.Notify();
        });
    EXPECT_TRUE(
        accept_notification.WaitForNotificationWithTimeout(kWaitTimeout));
    EXPECT_TRUE(
        fake_nearby_connections_manager_->DidUpgradeBandwidth(endpoint_id));
  }

  int64_t SetUpOutgoingShareTarget(
      MockTransferUpdateCallback& transfer_callback,
      MockShareTargetDiscoveredCallback& discovery_callback) {
    SetUpKeyVerification(
        /*is_incoming=*/false, PairedKeyResultFrame::SUCCESS);

    fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                                GetToken());
    fake_nearby_connections_manager_->set_nearby_connection(connection_.get());
    return DiscoverShareTarget(transfer_callback, discovery_callback);
  }

  int64_t DiscoverShareTarget(
      MockTransferUpdateCallback& transfer_callback,
      MockShareTargetDiscoveredCallback& discovery_callback) {
    SetConnectionType(ConnectionType::kWifi);

    // Start discovering, to ensure a discovery listener is registered.
    EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                  SendSurfaceState::kForeground),
              NearbySharingService::StatusCodes::kOk);
    EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

    int64_t discovered_target_id;
    // Discover a new endpoint, with fields set up a valid certificate.
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered)
        .WillOnce([&discovered_target_id](ShareTarget share_target) {
          discovered_target_id = share_target.id;
        });

    auto endpoint_info = std::make_unique<DiscoveredEndpointInfo>(
        CreateTestEndpointInfo(), kServiceId);
    fake_nearby_connections_manager_->OnEndpointFound(kEndpointId,
                                                      std::move(endpoint_info));
    FlushTesting();
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                             /*success=*/true);
    return discovered_target_id;
  }

  Frame GetWrittenFrame() {
    EXPECT_TRUE(
        sharing_service_task_runner_->SyncWithTimeout(kTaskWaitTimeout));
    std::vector<uint8_t> data = connection_->GetWrittenData();
    Frame frame;
    frame.ParseFromArray(data.data(), data.size());
    return frame;
  }

  bool ExpectPairedKeyEncryptionFrame() {
    Frame frame = GetWrittenFrame();
    if (!frame.has_v1()) {
      return false;
    }

    if (!frame.v1().has_paired_key_encryption()) {
      return false;
    }

    return true;
  }

  bool ExpectPairedKeyResultFrame() {
    Frame frame = GetWrittenFrame();
    if (!frame.has_v1()) {
      return false;
    }

    if (!frame.v1().has_paired_key_result()) {
      return false;
    }

    return true;
  }

  bool ExpectConnectionResponseFrame(ConnectionResponseFrame::Status status) {
    Frame frame = GetWrittenFrame();
    if (!frame.has_v1()) {
      return false;
    }

    if (!frame.v1().has_connection_response()) {
      return false;
    }

    if (status != frame.v1().connection_response().status()) {
      return false;
    }

    return true;
  }

  std::optional<IntroductionFrame> ExpectIntroductionFrame() {
    Frame frame = GetWrittenFrame();
    if (!frame.has_v1()) {
      return std::nullopt;
    }

    if (!frame.v1().has_introduction()) {
      return std::nullopt;
    }

    return frame.v1().introduction();
  }

  bool ExpectCancelFrame() {
    Frame frame = GetWrittenFrame();
    if (!frame.has_v1()) {
      return false;
    }

    if (frame.v1().type() != V1Frame::CANCEL) {
      return false;
    }

    return true;
  }

  // Optionally, |new_share_target| is updated with the ShareTargets sent to
  // OnTransferUpdate() calls.
  void ExpectTransferUpdates(
      MockTransferUpdateCallback& transfer_callback, int64_t share_target_id,
      const std::vector<TransferMetadata::Status>& updates,
      std::function<void()> callback) {
    expect_transfer_updates_count_ = 0;
    expect_transfer_updates_callback_ = std::move(callback);
    auto& expectation =
        EXPECT_CALL(transfer_callback, OnTransferUpdate).Times(updates.size());

    for (TransferMetadata::Status status : updates) {
      expectation.WillOnce(testing::Invoke(
          [this, share_target_id, status, update_size = updates.size()](
              const ShareTarget& share_target,
              const AttachmentContainer& container,
              const TransferMetadata& metadata) {
            EXPECT_EQ(share_target.id, share_target_id);
            EXPECT_EQ(metadata.status(), status);

            ++expect_transfer_updates_count_;
            if (expect_transfer_updates_count_ == update_size) {
              expect_transfer_updates_callback_();
            }
          }));
    }
  }

  // Returns the modified ShareTarget received from a TransferUpdate.
  void SetUpOutgoingConnectionUntilAccept(
      MockTransferUpdateCallback& transfer_callback, int64_t share_target_id) {
    ExpectTransferUpdates(transfer_callback, share_target_id,
                          {TransferMetadata::Status::kConnecting,
                           TransferMetadata::Status::kAwaitingRemoteAcceptance},
                          [] {});

    absl::Notification send_notification;
    NearbySharingServiceImpl::StatusCodes send_result;
    EXPECT_CALL(*mock_app_info_, SetActiveFlag());
    service_->SendAttachments(
        share_target_id, CreateTextAttachments({kTextPayload}),
        [&](NearbySharingServiceImpl::StatusCodes status_codes) {
          send_result = status_codes;
          send_notification.Notify();
        });

    EXPECT_TRUE(
        send_notification.WaitForNotificationWithTimeout(kTaskWaitTimeout));
    EXPECT_EQ(send_result, NearbySharingServiceImpl::StatusCodes::kOk);

    FlushTesting();
    // Verify data sent to the remote device so far.
    if (!ExpectPairedKeyEncryptionFrame()) {
      return;
    }

    if (!ExpectPairedKeyResultFrame()) {
      return;
    }

    if (!ExpectIntroductionFrame().has_value()) {
      return;
    }
  }

  struct PayloadInfo {
    int64_t payload_id;
    std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener> listener;
  };

  PayloadInfo AcceptAndSendPayload(
      MockTransferUpdateCallback& transfer_callback, int64_t share_target_id) {
    PayloadInfo info = {};
    fake_nearby_connections_manager_->set_send_payload_callback(
        [&](std::unique_ptr<Payload> payload,
            std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>
                listener) {
          ASSERT_TRUE(payload->content.is_bytes());
          std::vector<uint8_t> bytes = payload->content.bytes_payload.bytes;
          EXPECT_EQ(kTextPayload, std::string(bytes.begin(), bytes.end()));
          info.payload_id = payload->id;
          info.listener = listener;
        });

    // We're now waiting for the remote device to respond with the accept
    // result.
    ExpectTransferUpdates(transfer_callback, share_target_id,
                          {TransferMetadata::Status::kInProgress}, [] {});

    // Kick off send process by accepting the transfer from the remote device.
    SendConnectionResponse(ConnectionResponseFrame::ACCEPT);
    return info;
  }

  void FinishOutgoingTransfer(MockTransferUpdateCallback& transfer_callback,
                              int64_t share_target_id, bool complete,
                              const PayloadInfo& info) {
    // Simulate a successful transfer via Nearby Connections.
    ExpectTransferUpdates(transfer_callback, share_target_id,
                          {TransferMetadata::Status::kInProgress,
                           complete ? TransferMetadata::Status::kComplete
                                    : TransferMetadata::Status::kFailed},
                          [] {});

    sharing_service_task_runner_->PostTask([info = info]() {
      auto payload_transfer_update = std::make_unique<PayloadTransferUpdate>(
          info.payload_id, PayloadStatus::kSuccess,
          /*total_bytes=*/strlen(kTextPayload),
          /*bytes_transferred=*/strlen(kTextPayload));
      if (auto listener = info.listener.lock()) {
        listener->OnStatusUpdate(std::move(payload_transfer_update),
                                 /*upgraded_medium=*/std::nullopt);
      }
    });
    EXPECT_TRUE(sharing_service_task_runner_->SyncWithTimeout(kWaitTimeout));
  }

  std::unique_ptr<Advertisement> GetCurrentAdvertisement() {
    auto endpoint_info =
        fake_nearby_connections_manager_->advertising_endpoint_info();
    if (!endpoint_info) return nullptr;

    return Advertisement::FromEndpointInfo(absl::MakeSpan(
        *fake_nearby_connections_manager_->advertising_endpoint_info()));
  }

  std::vector<uint8_t> CreateInvalidTestEndpointInfo() {
    std::unique_ptr<Advertisement> advertisement = Advertisement::NewInstance(
        GetNearbyShareTestEncryptedMetadataKey().salt(),
        GetNearbyShareTestEncryptedMetadataKey().encrypted_key(), kDeviceType,
        std::nullopt, kVendorId);
    return advertisement->ToEndpointInfo();
  }

  void FindInvalidEndpoint(absl::string_view endpoint_id) {
    fake_nearby_connections_manager_->OnEndpointFound(
        endpoint_id, std::make_unique<DiscoveredEndpointInfo>(
                         CreateInvalidTestEndpointInfo(), kServiceId));
    FlushTesting();
  }

  void FindEndpoint(absl::string_view endpoint_id) {
    FindEndpointWithVendorId(endpoint_id, kVendorId);
  }

  void FindEndpointWithVendorId(absl::string_view endpoint_id,
                                uint8_t vendor_id) {
    fake_nearby_connections_manager_->OnEndpointFound(
        endpoint_id, std::make_unique<DiscoveredEndpointInfo>(
                         CreateTestEndpointInfo(vendor_id), kServiceId));
    FlushTesting();
  }

  void LoseEndpoint(absl::string_view endpoint_id) {
    fake_nearby_connections_manager_->OnEndpointLost(endpoint_id);
    FlushTesting();
  }

  // This method sets up an incoming connection and performs the steps
  // required to simulate a successful incoming transfer.
  void SuccessfullyReceiveTransfer() {
    NiceMock<MockTransferUpdateCallback> callback;
    int64_t share_target_id = SetUpIncomingConnection(callback);
    AcceptConnection(callback, share_target_id, kEndpointId);

    // Fail to accept again.
    service_->Accept(
        share_target_id,
        [&](NearbySharingServiceImpl::StatusCodes status_code) {
          EXPECT_EQ(status_code,
                    NearbySharingServiceImpl::StatusCodes::kOutOfOrderApiCall);
        });

    fake_nearby_connections_manager_->SetIncomingPayload(
        kFilePayloadId, GetFilePayload(kFilePayloadId));

    for (int64_t id : GetValidIntroductionFramePayloadIds()) {
      // Update file payload at the end.
      if (id == kFilePayloadId) continue;

      fake_nearby_connections_manager_->SetIncomingPayload(
          id, GetTextPayload(id, kTextPayload));

      std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener> listener =
          fake_nearby_connections_manager_->GetRegisteredPayloadStatusListener(
              id);

      absl::Notification progress_notification;

      EXPECT_CALL(callback,
                  OnTransferUpdate(testing::_, testing::_, testing::_))
          .WillOnce(testing::Invoke([&](const ShareTarget& share_target,
                                        const AttachmentContainer& container,
                                        TransferMetadata metadata) {
            EXPECT_FALSE(metadata.is_final_status());
            EXPECT_EQ(metadata.status(), TransferMetadata::Status::kInProgress);
            progress_notification.Notify();
          }));

      sharing_service_task_runner_->PostTask([id, listener = listener]() {
        PayloadTransferUpdate payload =
            PayloadTransferUpdate(id, PayloadStatus::kSuccess,
                                  /*total_bytes=*/kPayloadSize,
                                  /*bytes_transferred=*/kPayloadSize);
        if (auto locked_listener = listener.lock()) {
          locked_listener->OnStatusUpdate(
              std::make_unique<PayloadTransferUpdate>(payload),
              /*upgraded_medium=*/std::nullopt);
        }
      });

      EXPECT_TRUE(
          progress_notification.WaitForNotificationWithTimeout(kWaitTimeout));
      FastForward(kMinProgressUpdateFrequency);
    }

    std::filesystem::path file_path;
    absl::Notification success_notification;
    EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
        .WillOnce(testing::Invoke([&](const ShareTarget& share_target,
                                      const AttachmentContainer& container,
                                      TransferMetadata metadata) {
          EXPECT_TRUE(metadata.is_final_status());
          EXPECT_EQ(metadata.status(), TransferMetadata::Status::kComplete);

          ASSERT_TRUE(container.HasAttachments());
          EXPECT_EQ(1u, container.GetFileAttachments().size());
          for (const FileAttachment& file : container.GetFileAttachments()) {
            EXPECT_TRUE(file.file_path());
            file_path = *file.file_path();
          }

          EXPECT_EQ(3u, container.GetTextAttachments().size());
          for (const TextAttachment& text : container.GetTextAttachments()) {
            EXPECT_EQ(text.text_body(), kTextPayload);
          }

          success_notification.Notify();
        }));

    sharing_service_task_runner_->PostTask([this]() {
      std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener> listener =
          fake_nearby_connections_manager_->GetRegisteredPayloadStatusListener(
              kFilePayloadId);

      PayloadTransferUpdate payload =
          PayloadTransferUpdate(kFilePayloadId, PayloadStatus::kSuccess,
                                /*total_bytes=*/kPayloadSize,
                                /*bytes_transferred=*/kPayloadSize);
      if (auto locked_listener = listener.lock()) {
        locked_listener->OnStatusUpdate(
            std::make_unique<PayloadTransferUpdate>(payload),
            /*upgraded_medium=*/std::nullopt);
      }
    });

    EXPECT_TRUE(
        success_notification.WaitForNotificationWithTimeout(kWaitTimeout));
    FlushTesting();
    EXPECT_FALSE(
        fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId)
            .has_value());
    EXPECT_FALSE(fake_nearby_connections_manager_->has_incoming_payloads());

    // To avoid UAF in OnIncomingTransferUpdate().
    UnregisterReceiveSurface(&callback);

    // Remove test file.
    std::filesystem::remove(file_path);
  }

  void FlushTesting() {
    absl::SleepFor(absl::Milliseconds(200));
    EXPECT_TRUE(
        sharing_service_task_runner_->SyncWithTimeout(absl::Milliseconds(200)));
  }

  void SetDiskSpace(size_t size) {
    fake_device_info_.SetAvailableDiskSpaceInBytes(
        std::filesystem::temp_directory_path(), size);
  }

  void ResetDiskSpace() { fake_device_info_.ResetDiskSpace(); }

 protected:
  FakeNearbyShareLocalDeviceDataManager* local_device_data_manager() {
    EXPECT_EQ(local_device_data_manager_factory_.instances().size(), 1u);
    return local_device_data_manager_factory_.instances().back();
  }

  FakeNearbyShareCertificateManager* certificate_manager() {
    EXPECT_EQ(certificate_manager_factory_.instances().size(), 1u);
    return certificate_manager_factory_.instances().back();
  }

  FakeAccountManager& account_manager() { return fake_account_manager_; }

  std::filesystem::path CreateTestFile(absl::string_view name,
                                       const std::vector<uint8_t>& content) {
    std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    std::FILE* file = std::fopen(path.string().c_str(), "w+");
    std::fwrite(content.data(), 1, content.size(), file);
    std::fclose(file);
    return path;
  }

  PreferenceManager& preference_manager() { return preference_manager_; }

  api::MockSharingPlatform mock_sharing_platform_;
  nearby::FakePreferenceManager preference_manager_;
  FakeAccountManager fake_account_manager_;
  FakeContext fake_context_;
  FakeDeviceInfo fake_device_info_;
  FakeNearbyConnectionsManager* fake_nearby_connections_manager_ = nullptr;
  FakeNearbyShareLocalDeviceDataManager::Factory
      local_device_data_manager_factory_;
  FakeNearbyShareContactManager::Factory contact_manager_factory_;
  FakeNearbyShareCertificateManager::Factory certificate_manager_factory_;
  std::unique_ptr<FakeNearbyFastInitiation::Factory>
      nearby_fast_initiation_factory_;
  std::unique_ptr<FakeNearbyConnection> connection_;
  StrictMock<MockAppInfo>* mock_app_info_ = nullptr;
  std::unique_ptr<analytics::AnalyticsRecorder> analytics_recorder_;
  std::unique_ptr<NearbySharingServiceImpl> service_;
  int expect_transfer_updates_count_ = 0;
  std::function<void()> expect_transfer_updates_callback_;
  FakeTaskRunner* sharing_service_task_runner_ = nullptr;
};

struct ValidSendSurfaceTestData {
  bool bluetooth_enabled;
  ConnectionType connection_type;
} kValidSendSurfaceTestData[] = {
    // No network connection, only bluetooth available
    {true, ConnectionType::kNone},
    // Wifi available
    {true, ConnectionType::kWifi},
    // Ethernet available
    {true, ConnectionType::kEthernet},
    // 3G available
    {true, ConnectionType::k3G}};

class NearbySharingServiceImplValidSendTest
    : public NearbySharingServiceImplTest,
      public testing::WithParamInterface<ValidSendSurfaceTestData> {};

struct InvalidSendSurfaceTestData {
} kInvalidSendSurfaceTestData[] = {
    // Screen locked
    {/*screen_locked=*/},
    // No network connection and no bluetooth
    {/*screen_locked=*/},
    // 3G available and no bluetooth
    {/*screen_locked=*/},
    // Wi-Fi available and no bluetooth (invalid until Wi-Fi LAN is supported)
    {/*screen_locked=*/},
    // Ethernet available and no bluetooth (invalid until Wi-Fi LAN is
    // supported)
    {/*screen_locked=*/}};

using ResponseFrameStatus = ConnectionResponseFrame::Status;

struct SendFailureTestData {
  ResponseFrameStatus response_status;
  TransferMetadata::Status expected_status;
} kSendFailureTestData[] = {
    {service::proto::ConnectionResponseFrame::REJECT,
     TransferMetadata::Status::kRejected},
    {service::proto::ConnectionResponseFrame::NOT_ENOUGH_SPACE,
     TransferMetadata::Status::kNotEnoughSpace},
    {service::proto::ConnectionResponseFrame::UNSUPPORTED_ATTACHMENT_TYPE,
     TransferMetadata::Status::kUnsupportedAttachmentType},
    {service::proto::ConnectionResponseFrame::TIMED_OUT,
     TransferMetadata::Status::kTimedOut},
    {service::proto::ConnectionResponseFrame::UNKNOWN,
     TransferMetadata::Status::kFailed},
};

class NearbySharingServiceImplSendFailureTest
    : public NearbySharingServiceImplTest,
      public testing::WithParamInterface<SendFailureTestData> {};

class TestObserver : public NearbySharingService::Observer {
 public:
  explicit TestObserver(NearbySharingService* service) : service_(service) {
    service_->AddObserver(this);
  }

  ~TestObserver() override {
    // Check if Shutdown has been called.
    if (service_ != nullptr) {
      service_->RemoveObserver(this);
    }
  }

  void OnHighVisibilityChanged(bool in_high_visibility) override {
    in_high_visibility_ = in_high_visibility;
  }

  void OnStartAdvertisingFailure() override {
    on_start_advertising_failure_called_ = true;
  }

  void OnFastInitiationDevicesDetected() override {
    devices_detected_called_ = true;
  }
  void OnFastInitiationDevicesNotDetected() override {
    devices_not_detected_called_ = true;
  }
  void OnFastInitiationScanningStopped() override {
    scanning_stopped_called_ = true;
  }

  void OnBluetoothStatusChanged(AdapterState state) override {
    bluetooth_state_ = state;
  }

  void OnWifiStatusChanged(AdapterState state) override { wifi_state_ = state; }

  void OnLanStatusChanged(AdapterState state) override { lan_state_ = state; }

  void OnCredentialError() override { credential_error_called_ = true; }

  void OnShutdown() override {
    shutdown_called_ = true;
    service_->RemoveObserver(this);
    service_ = nullptr;
  }

  bool in_high_visibility_ = false;
  bool shutdown_called_ = false;
  bool on_start_advertising_failure_called_ = false;
  bool devices_detected_called_ = false;
  bool devices_not_detected_called_ = false;
  bool scanning_stopped_called_ = false;
  bool credential_error_called_ = false;
  NearbySharingService* service_;
  AdapterState bluetooth_state_ = AdapterState::INVALID;
  AdapterState wifi_state_ = AdapterState::INVALID;
  AdapterState lan_state_ = AdapterState::INVALID;
};

TEST_F(NearbySharingServiceImplTest, StartFastInitiationAdvertising) {
  FakeNearbyFastInitiation* fast_initiation =
      nearby_fast_initiation_factory_->GetNearbyFastInitiation();
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kOk);
  EXPECT_EQ(fast_initiation->StartAdvertisingCount(), 1);

  // Call RegisterSendSurface a second time and make sure StartAdvertising is
  // not called again.
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kInvalidArgument);
  EXPECT_EQ(fast_initiation->StartAdvertisingCount(), 1);
}

TEST_F(NearbySharingServiceImplTest, StartFastInitiationAdvertisingError) {
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  nearby_fast_initiation_factory_->GetNearbyFastInitiation()
      ->SetStartAdvertisingError(true);
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kOk);
}

TEST_F(NearbySharingServiceImplTest,
       BackgroundStartFastInitiationAdvertisingError) {
  FakeNearbyFastInitiation* fast_initiation =
      nearby_fast_initiation_factory_->GetNearbyFastInitiation();
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kBackground),
            NearbySharingService::StatusCodes::kOk);
  EXPECT_EQ(fast_initiation->StartAdvertisingCount(), 0);
}

TEST_F(NearbySharingServiceImplTest,
       StartFastInitiationAdvertising_BluetoothNotPresent) {
  SetConnectionType(ConnectionType::kNone);
  SetBluetoothIsPresent(false);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kNoAvailableConnectionMedium);
}

TEST_F(NearbySharingServiceImplTest,
       StartFastInitiationAdvertising_BluetoothNotPowered) {
  SetConnectionType(ConnectionType::kNone);
  SetBluetoothIsPowered(false);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kNoAvailableConnectionMedium);
}

TEST_F(NearbySharingServiceImplTest, StopFastInitiationAdvertising) {
  FakeNearbyFastInitiation* fast_initiation =
      nearby_fast_initiation_factory_->GetNearbyFastInitiation();
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kOk);
  EXPECT_EQ(fast_initiation->StartAdvertisingCount(), 1);
  EXPECT_EQ(UnregisterSendSurface(&transfer_callback),
            NearbySharingService::StatusCodes::kOk);
  EXPECT_EQ(fast_initiation->StartAdvertisingCount(),
            fast_initiation->StopAdvertisingCount());
}

TEST_F(NearbySharingServiceImplTest,
       StopFastInitiationAdvertising_BluetoothBecomesNotPresent) {
  FakeNearbyFastInitiation* fast_initiation =
      nearby_fast_initiation_factory_->GetNearbyFastInitiation();
  SetConnectionType(ConnectionType::kNone);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kOk);
  SetBluetoothIsPresent(false);
  EXPECT_EQ(fast_initiation->StartAdvertisingCount(), 1);
  EXPECT_EQ(fast_initiation->StopAdvertisingCount(), 1);
}

TEST_F(NearbySharingServiceImplTest,
       StopFastInitiationAdvertising_BluetoothBecomesNotPowered) {
  FakeNearbyFastInitiation* fast_initiation =
      nearby_fast_initiation_factory_->GetNearbyFastInitiation();
  SetConnectionType(ConnectionType::kNone);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kOk);
  SetBluetoothIsPowered(false);
  EXPECT_EQ(fast_initiation->StartAdvertisingCount(), 1);
  EXPECT_EQ(fast_initiation->StopAdvertisingCount(), 1);
}

TEST_F(NearbySharingServiceImplTest, FastInitiationScanning_StartAndStop) {
  FakeNearbyFastInitiation* fast_initiation =
      nearby_fast_initiation_factory_->GetNearbyFastInitiation();
  SetConnectionType(ConnectionType::kWifi);

  EXPECT_EQ(fast_initiation->StartScanningCount(), 1);
  EXPECT_EQ(fast_initiation->StopScanningCount(), 0);

  // Trigger a call to StopFastInitiationScanning().
  SetBluetoothIsPowered(false);
  EXPECT_EQ(fast_initiation->StartScanningCount(), 1);
  EXPECT_EQ(fast_initiation->StopScanningCount(), 1);

  // Trigger a call to StartFastInitiationScanning().
  SetBluetoothIsPowered(true);
  EXPECT_TRUE(sharing_service_task_runner_->SyncWithTimeout(kTaskWaitTimeout));
  EXPECT_EQ(fast_initiation->StartScanningCount(), 2);
  EXPECT_EQ(fast_initiation->StopScanningCount(), 1);
}

TEST_F(NearbySharingServiceImplTest, FastInitiationScanning_NotifyObservers) {
  FakeNearbyFastInitiation* fast_initiation =
      nearby_fast_initiation_factory_->GetNearbyFastInitiation();
  SetConnectionType(ConnectionType::kBluetooth);

  TestObserver observer(service_.get());

  ASSERT_EQ(fast_initiation->StartScanningCount(), 1);

  fast_initiation->FireDevicesDetected();
  EXPECT_TRUE(observer.devices_detected_called_);
  fast_initiation->FireDevicesNotDetected();
  EXPECT_TRUE(observer.devices_not_detected_called_);

  // Remove the observer before it goes out of scope.
  service_->RemoveObserver(&observer);
  FlushTesting();
}

TEST_F(NearbySharingServiceImplTest,
       FastInitiationScanning_PostTransferCooldown) {
  FakeNearbyFastInitiation* fast_initiation =
      nearby_fast_initiation_factory_->GetNearbyFastInitiation();
  SetConnectionType(ConnectionType::kBluetooth);

  // Make sure we started scanning once
  EXPECT_EQ(fast_initiation->StartScanningCount(), 1);
  EXPECT_EQ(fast_initiation->StopScanningCount(), 0);

  SuccessfullyReceiveTransfer();

  // Make sure we stopped scanning and didn't restart... yet.
  EXPECT_EQ(fast_initiation->StartScanningCount(), 1);
  EXPECT_EQ(fast_initiation->StopScanningCount(), 1);

  // Fast-forward 10s to pass through the cooldown period.
  FastForward(absl::Seconds(10));

  // Make sure we restarted Fast Initiation scanning.
  EXPECT_EQ(fast_initiation->StartScanningCount(), 2);
  EXPECT_EQ(fast_initiation->StopScanningCount(), 1);
}

TEST_F(NearbySharingServiceImplTest,
       ForegroundRegisterSendSurfaceStartsDiscovering) {
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_F(NearbySharingServiceImplTest,
       ForegroundRegisterSendSurfaceTwiceKeepsDiscovering) {
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kInvalidArgument);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_F(NearbySharingServiceImplTest,
       RegisterSendSurfaceAlreadyReceivingNotDiscovering) {
  NiceMock<MockTransferUpdateCallback> callback;
  int64_t share_target_id = SetUpIncomingConnection(callback);
  AcceptConnection(callback, share_target_id, kEndpointId);
  EXPECT_FALSE(connection_->IsClosed());

  MockTransferUpdateCallback send_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(RegisterSendSurface(&send_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kTransferAlreadyInProgress);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());

  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest,
       BackgroundRegisterSendSurfaceNotDiscovering) {
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kBackground),
            NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest,
       DifferentSurfaceRegisterSendSurfaceTwiceKeepsDiscovering) {
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kBackground),
            NearbySharingService::StatusCodes::kInvalidArgument);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_F(NearbySharingServiceImplTest,
       RegisterSendSurfaceEndpointFoundDiscoveryCallbackNotified) {
  SetConnectionType(ConnectionType::kWifi);

  // Start discovering, to ensure a discovery listener is registered.
  MockTransferUpdateCallback transfer_callback;
  NiceMock<MockShareTargetDiscoveredCallback> discovery_callback;
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  // Discover a new endpoint, with fields set up a valid certificate.
  EXPECT_CALL(discovery_callback, OnShareTargetDiscovered)
      .WillOnce([&](ShareTarget share_target) {
        EXPECT_FALSE(share_target.is_incoming);
        EXPECT_TRUE(share_target.is_known);
        EXPECT_EQ(share_target.device_name, kDeviceName);
        EXPECT_EQ(share_target.type, kDeviceType);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_NE(share_target.device_id, kEndpointId);
        EXPECT_EQ(share_target.full_name, kTestMetadataFullName);
      });
  fake_nearby_connections_manager_->OnEndpointFound(
      kEndpointId, std::make_unique<DiscoveredEndpointInfo>(
                       CreateTestEndpointInfo(), kServiceId));
  FlushTesting();
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  // Register another send surface, which will automatically catch up
  // discovered endpoints.
  MockTransferUpdateCallback transfer_callback2;
  NiceMock<MockShareTargetDiscoveredCallback> discovery_callback2;
  EXPECT_CALL(discovery_callback2, OnShareTargetDiscovered)
      .WillOnce([&](ShareTarget share_target) {
        EXPECT_EQ(share_target.device_name, kDeviceName);
      });

  EXPECT_EQ(RegisterSendSurface(&transfer_callback2, &discovery_callback2,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kOk);

  // Shut down the service while the discovery callbacks are still in
  // scope. OnShareTargetLost() will be invoked during shutdown.
  Shutdown();
  service_.reset();
}

TEST_F(NearbySharingServiceImplTest, RegisterSendSurfaceEmptyCertificate) {
  SetConnectionType(ConnectionType::kWifi);

  // Start discovering, to ensure a discovery listener is registered.
  MockTransferUpdateCallback transfer_callback;
  NiceMock<MockShareTargetDiscoveredCallback> discovery_callback;
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  // Discover a new endpoint, with fields set up a valid certificate.
  EXPECT_CALL(discovery_callback, OnShareTargetDiscovered)
      .WillOnce([](ShareTarget share_target) {
        EXPECT_FALSE(share_target.is_incoming);
        EXPECT_FALSE(share_target.is_known);
        EXPECT_EQ(share_target.device_name, kDeviceName);
        EXPECT_FALSE(share_target.image_url);
        EXPECT_EQ(share_target.type, kDeviceType);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_EQ(share_target.device_id, kEndpointId);
        EXPECT_FALSE(share_target.full_name);
      });
  fake_nearby_connections_manager_->OnEndpointFound(
      kEndpointId, std::make_unique<DiscoveredEndpointInfo>(
                       CreateTestEndpointInfo(), kServiceId));
  FlushTesting();
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/false);
  // Register another send surface, which will automatically catch up
  // discovered endpoints.
  MockTransferUpdateCallback transfer_callback2;
  NiceMock<MockShareTargetDiscoveredCallback> discovery_callback2;
  EXPECT_CALL(discovery_callback2, OnShareTargetDiscovered)
      .WillOnce([](ShareTarget share_target) {
        EXPECT_EQ(share_target.device_name, kDeviceName);
      });

  EXPECT_EQ(RegisterSendSurface(&transfer_callback2, &discovery_callback2,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kOk);

  // Shut down the service while the discovery callbacks are still in scope.
  // OnShareTargetLost() will be invoked during shutdown.
  Shutdown();
  service_.reset();
}

TEST_P(NearbySharingServiceImplValidSendTest,
       RegisterSendSurfaceIsDiscovering) {
  SetBluetoothIsPresent(GetParam().bluetooth_enabled);
  SetConnectionType(GetParam().connection_type);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kOk);
  FlushTesting();
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
}

INSTANTIATE_TEST_SUITE_P(NearbySharingServiceImplTest,
                         NearbySharingServiceImplValidSendTest,
                         testing::ValuesIn(kValidSendSurfaceTestData));

TEST_F(NearbySharingServiceImplTest, UnregisterSendSurfaceStopsDiscovering) {
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  EXPECT_EQ(UnregisterSendSurface(&transfer_callback),
            NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest,
       UnregisterSendSurfaceDifferentCallbackKeepDiscovering) {
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  MockTransferUpdateCallback transfer_callback2;
  MockShareTargetDiscoveredCallback discovery_callback2;
  EXPECT_EQ(UnregisterSendSurface(&transfer_callback2),
            NearbySharingService::StatusCodes::kError);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_F(NearbySharingServiceImplTest, UnregisterSendSurfaceNeverRegistered) {
  SetConnectionType(ConnectionType::kWifi);

  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(UnregisterSendSurface(&transfer_callback),
            NearbySharingService::StatusCodes::kError);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_F(NearbySharingServiceImplTest,
       ForegroundRegisterReceiveSurfaceIsAdvertisingAllContacts) {
  SetConnectionType(ConnectionType::kWifi);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  ::nearby::AccountManager::Account account;
  account.id = kTestAccountId;
  account_manager().SetAccount(account);
  local_device_data_manager()->SetDeviceName(kDeviceName);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_EQ(fake_nearby_connections_manager_->advertising_power_level(),
            PowerLevel::kHighPower);
  ASSERT_TRUE(fake_nearby_connections_manager_->advertising_endpoint_info());
  std::unique_ptr<Advertisement> advertisement = GetCurrentAdvertisement();
  ASSERT_NE(advertisement, nullptr);
  EXPECT_EQ(advertisement->device_name(), std::nullopt);
  EXPECT_EQ(advertisement->device_type(), ShareTargetType::kLaptop);
  auto& test_metadata_key = GetNearbyShareTestEncryptedMetadataKey();
  EXPECT_EQ(test_metadata_key.salt(), advertisement->salt());
  EXPECT_EQ(test_metadata_key.encrypted_key(),
            advertisement->encrypted_metadata_key());
  account_manager().SetAccount(std::nullopt);
}

TEST_F(NearbySharingServiceImplTest,
       ForegroundRegisterReceiveSurfaceIsAdvertisingNoOne) {
  SetConnectionType(ConnectionType::kWifi);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
  local_device_data_manager()->SetDeviceName(kDeviceName);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_EQ(PowerLevel::kHighPower,
            fake_nearby_connections_manager_->advertising_power_level());
  ASSERT_TRUE(fake_nearby_connections_manager_->advertising_endpoint_info());
  std::unique_ptr<Advertisement> advertisement = GetCurrentAdvertisement();
  ASSERT_NE(advertisement, nullptr);
  EXPECT_EQ(advertisement->device_name(), std::nullopt);
  EXPECT_EQ(ShareTargetType::kLaptop, advertisement->device_type());
  // Expecting random metadata key.
  EXPECT_EQ(static_cast<size_t>(sharing::Advertisement::kSaltSize),
            advertisement->salt().size());
  EXPECT_EQ(static_cast<size_t>(
                sharing::Advertisement::kMetadataEncryptionKeyHashByteSize),
            advertisement->encrypted_metadata_key().size());
}

TEST_F(NearbySharingServiceImplTest,
       BackgroundRegisterReceiveSurfaceIsAdvertisingSelectedContacts) {
  SetConnectionType(ConnectionType::kWifi);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS);
  ::nearby::AccountManager::Account account;
  account.id = kTestAccountId;
  account_manager().SetAccount(account);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_EQ(PowerLevel::kLowPower,
            fake_nearby_connections_manager_->advertising_power_level());
  ASSERT_TRUE(fake_nearby_connections_manager_->advertising_endpoint_info());
  std::unique_ptr<Advertisement> advertisement = GetCurrentAdvertisement();
  ASSERT_NE(advertisement, nullptr);
  EXPECT_FALSE(advertisement->device_name());
  EXPECT_EQ(ShareTargetType::kLaptop, advertisement->device_type());
  auto& test_metadata_key = GetNearbyShareTestEncryptedMetadataKey();
  EXPECT_EQ(test_metadata_key.salt(), advertisement->salt());
  EXPECT_EQ(test_metadata_key.encrypted_key(),
            advertisement->encrypted_metadata_key());
  account_manager().SetAccount(std::nullopt);
}

TEST_F(NearbySharingServiceImplTest,
       RegisterReceiveSurfaceTwiceSameCallbackKeepAdvertising) {
  SetConnectionType(ConnectionType::kWifi);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  NearbySharingService::StatusCodes result2 = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result2, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       RegisterReceiveSurfaceTwiceKeepAdvertising) {
  SetConnectionType(ConnectionType::kWifi);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  MockTransferUpdateCallback callback2;
  NearbySharingService::StatusCodes result2 = RegisterReceiveSurface(
      &callback2, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result2, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       DataUsageChangedRegisterReceiveSurfaceRestartsAdvertising) {
  SetConnectionType(ConnectionType::kWifi);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  preference_manager().SetInteger(
      prefs::kNearbySharingDataUsageName,
      static_cast<int>(DataUsage::OFFLINE_DATA_USAGE));
  FlushTesting();
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_EQ(DataUsage::OFFLINE_DATA_USAGE,
            fake_nearby_connections_manager_->advertising_data_usage());

  preference_manager().SetInteger(
      prefs::kNearbySharingDataUsageName,
      static_cast<int>(DataUsage::ONLINE_DATA_USAGE));
  FlushTesting();
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_EQ(DataUsage::ONLINE_DATA_USAGE,
            fake_nearby_connections_manager_->advertising_data_usage());
}

TEST_F(
    NearbySharingServiceImplTest,
    UnregisterForegroundReceiveSurfaceVisibilityAllContactsRestartAdvertising) {
  TestObserver observer(service_.get());
  SetConnectionType(ConnectionType::kWifi);
  preference_manager().SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS));
  FlushTesting();

  // Register both foreground and background receive surfaces
  MockTransferUpdateCallback background_transfer_callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &background_transfer_callback,
      NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  MockTransferUpdateCallback foreground_transfer_callback;
  result = RegisterReceiveSurface(
      &foreground_transfer_callback,
      NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  // Unregister the foreground surface. Advertising is stopped and restarted
  // with low power. The service reports InHighVisibility until the
  // StopAdvertising callback is called.
  FakeNearbyConnectionsManager::ConnectionsCallback stop_advertising_callback =
      fake_nearby_connections_manager_->GetStopAdvertisingCallback();
  FakeNearbyConnectionsManager::ConnectionsCallback start_advertising_callback =
      fake_nearby_connections_manager_->GetStartAdvertisingCallback();
  result = UnregisterReceiveSurface(&foreground_transfer_callback);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(observer.in_high_visibility_);

  std::move(stop_advertising_callback)(ConnectionsStatus::kSuccess);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(observer.in_high_visibility_);

  std::move(start_advertising_callback)(ConnectionsStatus::kSuccess);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(observer.in_high_visibility_);
}

TEST_F(NearbySharingServiceImplTest,
       RegisterReceiveSurfaceWithVendorId_StartAdvertisingVendorId) {
  SetConnectionType(ConnectionType::kWifi);
  preference_manager().SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_EVERYONE));
  FlushTesting();

  // Register background receive surface with vendor ID 1.
  MockTransferUpdateCallback background_transfer_callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &background_transfer_callback,
      NearbySharingService::ReceiveSurfaceState::kBackground,
      static_cast<uint8_t>(Advertisement::BlockedVendorId::kSamsung));
  ASSERT_EQ(result, NearbySharingService::StatusCodes::kOk);
  ASSERT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  auto endpoint_info =
      fake_nearby_connections_manager_->advertising_endpoint_info();
  auto advertisement = Advertisement::FromEndpointInfo(*endpoint_info);
  EXPECT_EQ(advertisement->vendor_id(),
            static_cast<uint8_t>(Advertisement::BlockedVendorId::kSamsung));
}

TEST_F(NearbySharingServiceImplTest,
       RegisterReceiveSurfaceWithVendorId_DoesNotAdvertiseInContacts) {
  SetConnectionType(ConnectionType::kWifi);
  preference_manager().SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS));
  FlushTesting();

  // Register background receive surface with vendor ID 1.
  MockTransferUpdateCallback background_transfer_callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &background_transfer_callback,
      NearbySharingService::ReceiveSurfaceState::kBackground,
      static_cast<uint8_t>(Advertisement::BlockedVendorId::kSamsung));
  ASSERT_EQ(result, NearbySharingService::StatusCodes::kOk);
  ASSERT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  auto endpoint_info =
      fake_nearby_connections_manager_->advertising_endpoint_info();
  auto advertisement = Advertisement::FromEndpointInfo(*endpoint_info);
  EXPECT_EQ(advertisement->vendor_id(),
            static_cast<uint8_t>(Advertisement::BlockedVendorId::kNone));
}

TEST_F(NearbySharingServiceImplTest,
       RegisterReceiveSurfaceWithDifferentVendorIdIsBlocked) {
  SetConnectionType(ConnectionType::kWifi);
  preference_manager().SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS));
  FlushTesting();

  // Register background receive surface with vendor ID 1.
  MockTransferUpdateCallback background_transfer_callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &background_transfer_callback,
      NearbySharingService::ReceiveSurfaceState::kBackground,
      static_cast<uint8_t>(Advertisement::BlockedVendorId::kSamsung));
  ASSERT_EQ(result, NearbySharingService::StatusCodes::kOk);
  ASSERT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  // Register foreground receive surface with different vendor ID.
  MockTransferUpdateCallback foreground_transfer_callback;
  result = RegisterReceiveSurface(
      &foreground_transfer_callback,
      NearbySharingService::ReceiveSurfaceState::kForeground, /*vendor_id=*/2);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kInvalidArgument);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       RegisterReceiveSurfaceWithVendorId_OkWithBgNoVendorId) {
  SetConnectionType(ConnectionType::kWifi);
  preference_manager().SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_EVERYONE));
  FlushTesting();

  // Register background receive surface with vendor ID 0.
  MockTransferUpdateCallback background_transfer_callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &background_transfer_callback,
      NearbySharingService::ReceiveSurfaceState::kBackground,
      static_cast<uint8_t>(Advertisement::BlockedVendorId::kNone));
  ASSERT_EQ(result, NearbySharingService::StatusCodes::kOk);
  ASSERT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  // Register foreground receive surface with vendor ID 1.
  MockTransferUpdateCallback foreground_transfer_callback;
  result = RegisterReceiveSurface(
      &foreground_transfer_callback,
      NearbySharingService::ReceiveSurfaceState::kForeground,
      static_cast<uint8_t>(Advertisement::BlockedVendorId::kSamsung));
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  // Verify endpoint info is advertising the foreground surface.
  auto endpoint_info =
      fake_nearby_connections_manager_->advertising_endpoint_info();
  auto advertisement = Advertisement::FromEndpointInfo(*endpoint_info);
  EXPECT_EQ(advertisement->vendor_id(),
            static_cast<uint8_t>(Advertisement::BlockedVendorId::kSamsung));
}

TEST_F(NearbySharingServiceImplTest,
       NoNetworkRegisterReceiveSurfaceIsAdvertising) {
  MockTransferUpdateCallback callback;
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  // Succeeds since bluetooth is present.
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       NoBluetoothNoNetworkRegisterForegroundReceiveSurfaceNotAdvertising) {
  SetConnectionType(ConnectionType::kNone);
  SetBluetoothIsPresent(false);

  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result,
            NearbySharingService::StatusCodes::kNoAvailableConnectionMedium);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest,
       NoBluetoothNoNetworkRegisterBackgroundReceiveSurfaceWorks) {
  SetConnectionType(ConnectionType::kNone);
  SetBluetoothIsPresent(false);

  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result,
            NearbySharingService::StatusCodes::kNoAvailableConnectionMedium);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest, WifiRegisterReceiveSurfaceIsAdvertising) {
  SetConnectionType(ConnectionType::kWifi);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       EthernetRegisterReceiveSurfaceIsAdvertising) {
  SetConnectionType(ConnectionType::kEthernet);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       ThreeGRegisterReceiveSurfaceIsAdvertising) {
  SetConnectionType(ConnectionType::k3G);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  // Since bluetooth is on, connection still succeeds.
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       NoBluetoothWifiReceiveSurfaceIsAdvertising) {
  SetBluetoothIsPresent(false);
  SetConnectionType(ConnectionType::kWifi);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);

  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       NoBluetoothEthernetReceiveSurfaceIsAdvertising) {
  SetBluetoothIsPresent(false);
  SetConnectionType(ConnectionType::kEthernet);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);

  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       NoBluetoothThreeGReceiveSurfaceNotAdvertising) {
  SetBluetoothIsPresent(false);
  SetConnectionType(ConnectionType::k3G);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result,
            NearbySharingService::StatusCodes::kNoAvailableConnectionMedium);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest,
       ForegroundReceiveSurfaceNoOneVisibilityIsAdvertising) {
  SetConnectionType(ConnectionType::kWifi);
  preference_manager().SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE));
  FlushTesting();
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       BackgroundReceiveSurfaceNoOneVisibilityNotAdvertising) {
  SetConnectionType(ConnectionType::kWifi);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
  preference_manager().SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED));
  FlushTesting();
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest,
       BackgroundReceiveSurfaceVisibilityToNoOneStopsAdvertising) {
  SetConnectionType(ConnectionType::kWifi);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS);
  FlushTesting();
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  preference_manager().SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_HIDDEN));
  FlushTesting();
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest, ValidateLoginStateWhenSettingVisibility) {
  absl::Notification set_visibility_notification;
  NearbySharingService::StatusCodes set_visibility_status;
  SetConnectionType(ConnectionType::kWifi);
  FlushTesting();
  service_->SetVisibility(
      DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS, absl::ZeroDuration(),
      [&set_visibility_notification,
       &set_visibility_status](NearbySharingService::StatusCodes status) {
        set_visibility_status = status;
        set_visibility_notification.Notify();
      });
  // We should not have been able to set all contacts visibility because we are
  // not logged in.
  ASSERT_TRUE(
      set_visibility_notification.WaitForNotificationWithTimeout(kWaitTimeout));
  EXPECT_EQ(set_visibility_status,
            NearbySharingService::StatusCodes::kInvalidArgument);
  // Create account.
  AccountManager::Account account;
  account.id = kTestAccountId;
  // Log in.
  account_manager().SetAccount(account);
  std::unique_ptr<SigninAttempt> signin_attempt =
      service_->GetAccountManager()->Login("test_client_id",
                                           "test_client_secret");
  account_manager().NotifyLogin(kTestAccountId);
  // We are now logged in, so we should be able to set all contacts visibility.
  absl::Notification set_visibility_notification2;
  NearbySharingService::StatusCodes set_visibility_status2;
  service_->SetVisibility(
      DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS, absl::ZeroDuration(),
      [&set_visibility_notification2,
       &set_visibility_status2](NearbySharingService::StatusCodes status) {
        set_visibility_status2 = status;
        set_visibility_notification2.Notify();
      });
  ASSERT_TRUE(set_visibility_notification2.WaitForNotificationWithTimeout(
      kWaitTimeout));
  EXPECT_EQ(set_visibility_status2, NearbySharingService::StatusCodes::kOk);
}

TEST_F(NearbySharingServiceImplTest,
       BackgroundReceiveSurfaceVisibilityToAllContactsStartsAdvertising) {
  SetConnectionType(ConnectionType::kWifi);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED);
  FlushTesting();
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());

  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  FlushTesting();
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       ForegroundReceiveSurfaceSelectedContactsVisibilityIsAdvertising) {
  SetConnectionType(ConnectionType::kWifi);
  preference_manager().SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS));
  FlushTesting();
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       BackgroundReceiveSurfaceSelectedContactsVisibilityIsAdvertising) {
  SetConnectionType(ConnectionType::kWifi);
  preference_manager().SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS));
  FlushTesting();
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       ForegroundReceiveSurfaceAllContactsVisibilityIsAdvertising) {
  SetConnectionType(ConnectionType::kWifi);
  preference_manager().SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS));
  FlushTesting();
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest,
       BackgroundReceiveSurfaceAllContactsVisibilityNotAdvertising) {
  SetConnectionType(ConnectionType::kWifi);
  preference_manager().SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS));
  FlushTesting();
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest, UnregisterReceiveSurfaceStopsAdvertising) {
  SetConnectionType(ConnectionType::kWifi);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  NearbySharingService::StatusCodes result2 =
      UnregisterReceiveSurface(&callback);
  EXPECT_EQ(result2, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest,
       UnregisterReceiveSurfaceDifferentCallbackKeepAdvertising) {
  SetConnectionType(ConnectionType::kWifi);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());

  MockTransferUpdateCallback callback2;
  NearbySharingService::StatusCodes result2 =
      UnregisterReceiveSurface(&callback2);
  EXPECT_EQ(result2, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest, UnregisterReceiveSurfaceNeverRegistered) {
  SetConnectionType(ConnectionType::kWifi);

  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result =
      UnregisterReceiveSurface(&callback);
  // This is no longer considered an error condition.
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
}

TEST_F(NearbySharingServiceImplTest, IncomingConnectionClosedAfterShutdown) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              GetToken());

  SetConnectionType(ConnectionType::kWifi);
  NiceMock<MockTransferUpdateCallback> callback;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
      .Times(0);

  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  SetUpForegroundReceiveSurface(callback);
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());
  Shutdown();

  service_->OnIncomingConnection(kEndpointId, CreateTestEndpointInfo(),
                                 connection_.get());

  sharing_service_task_runner_->SyncWithTimeout(kTaskWaitTimeout);
  service_.reset();
}

TEST_F(NearbySharingServiceImplTest,
       IncomingConnectionClosedBeforeCertDecryption) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              GetToken());

  SetConnectionType(ConnectionType::kWifi);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  NiceMock<MockTransferUpdateCallback> callback;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke([](const ShareTarget& share_target,
                                   const AttachmentContainer& container,
                                   TransferMetadata metadata) {
        EXPECT_TRUE(metadata.is_final_status());
        EXPECT_EQ(TransferMetadata::Status::kFailed, metadata.status());
      }));

  SetUpForegroundReceiveSurface(callback);
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());
  service_->OnIncomingConnection(kEndpointId, CreateTestEndpointInfo(),
                                 connection_.get());
  sharing_service_task_runner_->PostTask([this]() { connection_->Close(); });
  sharing_service_task_runner_->SyncWithTimeout(kTaskWaitTimeout);

  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest,
       IncomingConnectionClosedReadingIntroduction) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              GetToken());

  SetConnectionType(ConnectionType::kWifi);
  NiceMock<MockTransferUpdateCallback> callback;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
      .Times(0);

  SetUpKeyVerification(/*is_incoming=*/true,
                       service::proto::PairedKeyResultFrame::SUCCESS);
  SetUpForegroundReceiveSurface(callback);
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());
  service_->OnIncomingConnection(kEndpointId, CreateTestEndpointInfo(),
                                 connection_.get());
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);

  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, IncomingConnectionEmptyIntroductionFrame) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              GetToken());
  SetUpIntroductionFrameDecoder(/*return_empty_introduction_frame=*/true);

  SetConnectionType(ConnectionType::kWifi);
  NiceMock<MockTransferUpdateCallback> callback;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke([](const ShareTarget& share_target,
                                   const AttachmentContainer& container,
                                   TransferMetadata metadata) {
        EXPECT_TRUE(metadata.is_final_status());
        EXPECT_EQ(metadata.status(),
                  TransferMetadata::Status::kUnsupportedAttachmentType);
        EXPECT_TRUE(share_target.is_incoming);
        EXPECT_TRUE(share_target.is_known);
        EXPECT_FALSE(container.HasAttachments());
        EXPECT_EQ(share_target.device_name, kDeviceName);
        EXPECT_EQ(share_target.type, kDeviceType);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_NE(share_target.device_id, kEndpointId);
        EXPECT_EQ(share_target.full_name, kTestMetadataFullName);
      }));

  SetUpKeyVerification(/*is_incoming=*/true,
                       service::proto::PairedKeyResultFrame::SUCCESS);
  SetUpForegroundReceiveSurface(callback);
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());
  service_->OnIncomingConnection(kEndpointId, CreateTestEndpointInfo(),
                                 connection_.get());
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  // Check data written to connection_.
  ASSERT_TRUE(ExpectPairedKeyEncryptionFrame());
  ASSERT_TRUE(ExpectPairedKeyResultFrame());
  ASSERT_TRUE(ExpectConnectionResponseFrame(
      service::proto::ConnectionResponseFrame::UNSUPPORTED_ATTACHMENT_TYPE));

  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest,
       IncomingConnectionValidIntroductionFrameInvalidCertificate) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              GetToken());
  SetUpIntroductionFrameDecoder(/*return_empty_introduction_frame=*/false);

  SetConnectionType(ConnectionType::kWifi);
  NiceMock<MockTransferUpdateCallback> callback;

  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke([](const ShareTarget& share_target,
                                   const AttachmentContainer& container,
                                   TransferMetadata metadata) {
        EXPECT_FALSE(metadata.is_final_status());
        EXPECT_EQ(TransferMetadata::Status::kAwaitingLocalConfirmation,
                  metadata.status());
        EXPECT_TRUE(share_target.is_incoming);
        EXPECT_FALSE(share_target.is_known);
        EXPECT_TRUE(container.HasAttachments());
        EXPECT_EQ(container.GetTextAttachments().size(), 3u);
        EXPECT_EQ(container.GetFileAttachments().size(), 1u);
        EXPECT_EQ(share_target.device_name, kDeviceName);
        EXPECT_FALSE(share_target.image_url);
        EXPECT_EQ(share_target.type, kDeviceType);
        EXPECT_EQ(share_target.device_id, kEndpointId);
        EXPECT_FALSE(share_target.full_name);
      }));

  SetUpKeyVerification(/*is_incoming=*/true,
                       service::proto::PairedKeyResultFrame::SUCCESS);
  SetUpForegroundReceiveSurface(callback);
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());
  service_->OnIncomingConnection(kEndpointId, CreateTestEndpointInfo(),
                                 connection_.get());
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/false);
  EXPECT_FALSE(connection_->IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, IncomingConnectionTimedOut) {
  NiceMock<MockTransferUpdateCallback> callback;
  SetUpIncomingConnection(callback);
  EXPECT_FALSE(connection_->IsClosed());

  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke([](const ShareTarget& share_target,
                                   const AttachmentContainer& container,
                                   TransferMetadata metadata) {
        EXPECT_TRUE(metadata.is_final_status());
        EXPECT_EQ(metadata.status(), TransferMetadata::Status::kTimedOut);
      }));

  FastForward(kReadResponseFrameTimeout);

  // Waits for delay to close connection.
  FastForward(kIncomingRejectionDelay);
  EXPECT_TRUE(connection_->IsClosed());
}

TEST_F(NearbySharingServiceImplTest,
       IncomingConnectionClosedWaitingLocalConfirmation) {
  NiceMock<MockTransferUpdateCallback> callback;
  SetUpIncomingConnection(callback);

  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke([](const ShareTarget& share_target,
                                   const AttachmentContainer& container,
                                   TransferMetadata metadata) {
        EXPECT_TRUE(metadata.is_final_status());
        EXPECT_EQ(metadata.status(), TransferMetadata::Status::kFailed);
      }));

  sharing_service_task_runner_->PostTask([this]() { connection_->Close(); });
  sharing_service_task_runner_->SyncWithTimeout(kTaskWaitTimeout);

  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, IncomingConnectionOutOfStorage) {
  SetDiskSpace(kFreeDiskSpace);
  preference_manager().SetString(
      prefs::kNearbySharingCustomSavePath,
      GetCompatibleU8String(fake_device_info_.GetDownloadPath().u8string()));
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              GetToken());

  // Set a huge file size in introduction frame to go out of storage.
  Frame frame;
  frame.set_version(Frame::V1);
  V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::INTRODUCTION);
  IntroductionFrame* introduction_frame = v1_frame->mutable_introduction();
  nearby::sharing::service::proto::FileMetadata* file_metadata =
      introduction_frame->add_file_metadata();
  file_metadata->set_name("name");
  file_metadata->set_type(nearby::sharing::service::proto::FileMetadata::AUDIO);
  file_metadata->set_payload_id(1);
  file_metadata->set_size(kFreeDiskSpace + 1);
  file_metadata->set_mime_type("mime type");
  file_metadata->set_id(123);
  std::vector<uint8_t> bytes(frame.ByteSizeLong());
  frame.SerializeToArray(bytes.data(), bytes.size());
  connection_->AppendReadableData(std::move(bytes));
  FlushTesting();

  SetConnectionType(ConnectionType::kWifi);
  NiceMock<MockTransferUpdateCallback> callback;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke([](const ShareTarget& share_target,
                                   const AttachmentContainer& container,
                                   TransferMetadata metadata) {
        EXPECT_TRUE(share_target.is_incoming);
        EXPECT_TRUE(share_target.is_known);
        EXPECT_TRUE(container.HasAttachments());
        EXPECT_EQ(container.GetTextAttachments().size(), 0u);
        EXPECT_EQ(container.GetFileAttachments().size(), 1u);
        EXPECT_EQ(share_target.device_name, kDeviceName);
        EXPECT_EQ(share_target.type, kDeviceType);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_NE(share_target.device_id, kEndpointId);
        EXPECT_EQ(share_target.full_name, kTestMetadataFullName);
        EXPECT_EQ(metadata.status(), TransferMetadata::Status::kNotEnoughSpace);
      }));

  SetUpKeyVerification(
      /*is_incoming=*/true, PairedKeyResultFrame::SUCCESS);
  SetUpForegroundReceiveSurface(callback);
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());
  service_->OnIncomingConnection(kEndpointId, CreateTestEndpointInfo(),
                                 connection_.get());
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
  ResetDiskSpace();
}

TEST_F(NearbySharingServiceImplTest, IncomingConnectionFileSizeOverflow) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              GetToken());

  // Set file size sum huge to check for overflow.
  Frame frame;
  frame.set_version(Frame::V1);
  V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::INTRODUCTION);
  IntroductionFrame* introduction_frame = v1_frame->mutable_introduction();
  nearby::sharing::service::proto::FileMetadata* file_metadata =
      introduction_frame->add_file_metadata();
  file_metadata->set_name("name_1");
  file_metadata->set_type(nearby::sharing::service::proto::FileMetadata::AUDIO);
  file_metadata->set_payload_id(1);
  file_metadata->set_size(std::numeric_limits<int64_t>::max());
  file_metadata->set_mime_type("mime type");
  file_metadata->set_id(123);
  nearby::sharing::service::proto::FileMetadata* file2_metadata =
      introduction_frame->add_file_metadata();
  file2_metadata->set_name("name_2");
  file2_metadata->set_type(
      nearby::sharing::service::proto::FileMetadata::VIDEO);
  file2_metadata->set_payload_id(2);
  file2_metadata->set_size(100);
  file2_metadata->set_mime_type("mime type");
  file2_metadata->set_id(124);
  std::vector<uint8_t> bytes(frame.ByteSizeLong());
  frame.SerializeToArray(bytes.data(), bytes.size());
  connection_->AppendReadableData(std::move(bytes));
  FlushTesting();

  SetConnectionType(ConnectionType::kWifi);
  NiceMock<MockTransferUpdateCallback> callback;

  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke([](const ShareTarget& share_target,
                                   const AttachmentContainer& container,
                                   TransferMetadata metadata) {
        EXPECT_TRUE(share_target.is_incoming);
        EXPECT_TRUE(share_target.is_known);
        EXPECT_EQ(share_target.device_name, kDeviceName);
        EXPECT_EQ(share_target.type, kDeviceType);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_NE(share_target.device_id, kEndpointId);
        EXPECT_EQ(share_target.full_name, kTestMetadataFullName);
        EXPECT_EQ(metadata.status(), TransferMetadata::Status::kNotEnoughSpace);
      }));

  SetUpKeyVerification(
      /*is_incoming=*/true, PairedKeyResultFrame::SUCCESS);
  SetUpForegroundReceiveSurface(callback);
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());
  service_->OnIncomingConnection(kEndpointId, CreateTestEndpointInfo(),
                                 connection_.get());
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest,
       IncomingConnectionValidIntroductionFrameValidCertificate) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              GetToken());
  SetUpIntroductionFrameDecoder(/*return_empty_introduction_frame=*/false);

  SetConnectionType(ConnectionType::kWifi);
  NiceMock<MockTransferUpdateCallback> callback;
  absl::Notification notification;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
      .WillOnce(
          testing::Invoke([&notification](const ShareTarget& share_target,
                                          const AttachmentContainer& container,
                                          TransferMetadata metadata) {
            EXPECT_FALSE(metadata.is_final_status());
            EXPECT_EQ(TransferMetadata::Status::kAwaitingLocalConfirmation,
                      metadata.status());
            EXPECT_TRUE(share_target.is_incoming);
            EXPECT_TRUE(share_target.is_known);
            EXPECT_TRUE(container.HasAttachments());
            EXPECT_EQ(container.GetTextAttachments().size(), 3u);
            EXPECT_EQ(container.GetFileAttachments().size(), 1u);
            EXPECT_EQ(share_target.device_name, kDeviceName);
            EXPECT_EQ(share_target.type, kDeviceType);
            EXPECT_TRUE(share_target.device_id);
            EXPECT_NE(share_target.device_id, kEndpointId);
            EXPECT_EQ(share_target.full_name, kTestMetadataFullName);
            EXPECT_FALSE(share_target.for_self_share);
            EXPECT_FALSE(metadata.is_self_share());
            EXPECT_TRUE(metadata.token().has_value());
            notification.Notify();
          }));

  SetUpKeyVerification(/*is_incoming=*/true, PairedKeyResultFrame::SUCCESS);
  SetUpForegroundReceiveSurface(callback);
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());
  service_->OnIncomingConnection(kEndpointId, CreateTestEndpointInfo(),
                                 connection_.get());
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));

  EXPECT_FALSE(connection_->IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, AcceptInvalidShareTarget) {
  absl::Notification notification;
  service_->Accept(
      98765L, [&](NearbySharingServiceImpl::StatusCodes status_code) {
        EXPECT_EQ(status_code,
                  NearbySharingServiceImpl::StatusCodes::kInvalidArgument);
        notification.Notify();
      });

  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

TEST_F(NearbySharingServiceImplTest, AcceptValidShareTarget) {
  NiceMock<MockTransferUpdateCallback> callback;
  int64_t share_target_id = SetUpIncomingConnection(callback);
  AcceptConnection(callback, share_target_id, kEndpointId);

  // Check data written to connection_.
  EXPECT_TRUE(ExpectPairedKeyEncryptionFrame());
  EXPECT_TRUE(ExpectPairedKeyResultFrame());
  EXPECT_TRUE(ExpectConnectionResponseFrame(
      service::proto::ConnectionResponseFrame::ACCEPT));

  EXPECT_FALSE(connection_->IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, AcceptValidShareTargetPayloadSuccessful) {
  SuccessfullyReceiveTransfer();
}

TEST_F(NearbySharingServiceImplTest,
       AcceptValidShareTargetPayloadSuccessfulIncomingPayloadNotFound) {
  NiceMock<MockTransferUpdateCallback> callback;
  int64_t share_target_id = SetUpIncomingConnection(callback);
  AcceptConnection(callback, share_target_id, kEndpointId);

  fake_nearby_connections_manager_->SetIncomingPayload(
      kFilePayloadId, GetFilePayload(kFilePayloadId));

  for (int64_t id : GetValidIntroductionFramePayloadIds()) {
    // Update file payload at the end.
    if (id == kFilePayloadId) continue;

    // Deliberately not calling SetIncomingPayload() for text payloads to check
    // for failure condition.

    absl::Notification progress_notification;
    EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
        .WillOnce(testing::Invoke([&](const ShareTarget& share_target,
                                      const AttachmentContainer& container,
                                      TransferMetadata metadata) {
          EXPECT_FALSE(metadata.is_final_status());
          EXPECT_EQ(metadata.status(), TransferMetadata::Status::kInProgress);
          progress_notification.Notify();
        }));

    sharing_service_task_runner_->PostTask([this, id]() {
      std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener> listener =
          fake_nearby_connections_manager_->GetRegisteredPayloadStatusListener(
              id);
      ASSERT_FALSE(listener.expired());

      auto payload = std::make_unique<PayloadTransferUpdate>(
          id, PayloadStatus::kSuccess,
          /*total_bytes=*/kPayloadSize,
          /*bytes_transferred=*/kPayloadSize);
      if (auto locked_listener = listener.lock()) {
        locked_listener->OnStatusUpdate(std::move(payload),
                                        /*upgraded_medium=*/std::nullopt);
      }
    });
    EXPECT_TRUE(
        progress_notification.WaitForNotificationWithTimeout(kWaitTimeout));
    FastForward(kMinProgressUpdateFrequency);
  }

  absl::Notification success_notification;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke([&](const ShareTarget& share_target,
                                    const AttachmentContainer& container,
                                    TransferMetadata metadata) {
        EXPECT_TRUE(metadata.is_final_status());
        EXPECT_EQ(metadata.status(),
                  TransferMetadata::Status::kIncompletePayloads);
        ASSERT_TRUE(container.HasAttachments());
        EXPECT_EQ(container.GetFileAttachments().size(), 1u);
        const FileAttachment& file = container.GetFileAttachments()[0];
        EXPECT_FALSE(file.file_path());
        success_notification.Notify();
      }));

  sharing_service_task_runner_->PostTask([this]() {
    std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener> listener =
        fake_nearby_connections_manager_->GetRegisteredPayloadStatusListener(
            kFilePayloadId);
    ASSERT_FALSE(listener.expired());

    auto payload = std::make_unique<PayloadTransferUpdate>(
        kFilePayloadId, PayloadStatus::kSuccess,
        /*total_bytes=*/kPayloadSize,
        /*bytes_transferred=*/kPayloadSize);
    if (auto locked_listener = listener.lock()) {
      locked_listener->OnStatusUpdate(std::move(payload),
                                      /*upgraded_medium=*/std::nullopt);
    }
  });
  EXPECT_TRUE(
      success_notification.WaitForNotificationWithTimeout(kWaitTimeout));

  EXPECT_FALSE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId)
          .has_value());
  EXPECT_FALSE(fake_nearby_connections_manager_->has_incoming_payloads());

  // File deletion runs in a ThreadPool.
  EXPECT_TRUE(
      sharing_service_task_runner_->SyncWithTimeout(absl::Milliseconds(200)));

  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, AcceptValidShareTargetPayloadFailed) {
  NiceMock<MockTransferUpdateCallback> callback;
  int64_t share_target_id = SetUpIncomingConnection(callback);
  AcceptConnection(callback, share_target_id, kEndpointId);

  absl::Notification failure_notification;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke([&](const ShareTarget& share_target,
                                    const AttachmentContainer& container,
                                    TransferMetadata metadata) {
        EXPECT_TRUE(metadata.is_final_status());
        EXPECT_EQ(metadata.status(), TransferMetadata::Status::kFailed);
        ASSERT_TRUE(container.HasAttachments());
        EXPECT_EQ(container.GetFileAttachments().size(), 1u);
        const FileAttachment& file = container.GetFileAttachments()[0];
        EXPECT_FALSE(file.file_path());
        failure_notification.Notify();
      }));

  sharing_service_task_runner_->PostTask([this]() {
    std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener> listener =
        fake_nearby_connections_manager_->GetRegisteredPayloadStatusListener(
            kFilePayloadId);
    ASSERT_FALSE(listener.expired());

    auto payload = std::make_unique<PayloadTransferUpdate>(
        kFilePayloadId, PayloadStatus::kFailure,
        /*total_bytes=*/kPayloadSize,
        /*bytes_transferred=*/kPayloadSize);
    if (auto locked_listener = listener.lock()) {
      locked_listener->OnStatusUpdate(std::move(payload),
                                      /*upgraded_medium=*/std::nullopt);
    }
  });

  EXPECT_TRUE(
      failure_notification.WaitForNotificationWithTimeout(kWaitTimeout));

  EXPECT_FALSE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId)
          .has_value());
  EXPECT_FALSE(fake_nearby_connections_manager_->has_incoming_payloads());

  // File deletion runs in a ThreadPool.
  EXPECT_TRUE(
      sharing_service_task_runner_->SyncWithTimeout(absl::Milliseconds(200)));

  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, AcceptValidShareTargetPayloadCancelled) {
  NiceMock<MockTransferUpdateCallback> callback;
  int64_t share_target_id = SetUpIncomingConnection(callback);
  AcceptConnection(callback, share_target_id, kEndpointId);

  absl::Notification failure_notification;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke([&](const ShareTarget& share_target,
                                    const AttachmentContainer& container,
                                    TransferMetadata metadata) {
        EXPECT_TRUE(metadata.is_final_status());
        EXPECT_EQ(metadata.status(), TransferMetadata::Status::kCancelled);
        ASSERT_TRUE(container.HasAttachments());
        EXPECT_EQ(container.GetFileAttachments().size(), 1u);
        const FileAttachment& file = container.GetFileAttachments()[0];
        EXPECT_FALSE(file.file_path());
        failure_notification.Notify();
      }));

  sharing_service_task_runner_->PostTask([this]() {
    std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener> listener =
        fake_nearby_connections_manager_->GetRegisteredPayloadStatusListener(
            kFilePayloadId);
    ASSERT_FALSE(listener.expired());

    auto payload = std::make_unique<PayloadTransferUpdate>(
        kFilePayloadId, PayloadStatus::kCanceled,
        /*total_bytes=*/kPayloadSize,
        /*bytes_transferred=*/kPayloadSize);
    if (auto locked_listener = listener.lock()) {
      locked_listener->OnStatusUpdate(std::move(payload),
                                      /*upgraded_medium=*/std::nullopt);
    }
  });
  EXPECT_TRUE(
      failure_notification.WaitForNotificationWithTimeout(kWaitTimeout));

  EXPECT_FALSE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId)
          .has_value());
  EXPECT_FALSE(fake_nearby_connections_manager_->has_incoming_payloads());

  // File deletion runs in a ThreadPool.
  EXPECT_TRUE(
      sharing_service_task_runner_->SyncWithTimeout(absl::Milliseconds(200)));

  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, RejectInvalidShareTarget) {
  absl::Notification notification;
  service_->Reject(
      98456L, [&](NearbySharingServiceImpl::StatusCodes status_code) {
        EXPECT_EQ(status_code,
                  NearbySharingServiceImpl::StatusCodes::kInvalidArgument);
        notification.Notify();
      });

  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

TEST_F(NearbySharingServiceImplTest, RejectValidShareTarget) {
  NiceMock<MockTransferUpdateCallback> callback;
  int64_t share_target_id = SetUpIncomingConnection(callback);

  absl::Notification notification;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke([](const ShareTarget& share_target,
                                   const AttachmentContainer& container,
                                   TransferMetadata metadata) {
        EXPECT_TRUE(metadata.is_final_status());
        EXPECT_EQ(metadata.status(), TransferMetadata::Status::kRejected);
      }));

  service_->Reject(
      share_target_id, [&](NearbySharingServiceImpl::StatusCodes status_code) {
        EXPECT_EQ(status_code, NearbySharingServiceImpl::StatusCodes::kOk);
        notification.Notify();
      });

  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));

  // Check data written to connection_.
  EXPECT_TRUE(ExpectPairedKeyEncryptionFrame());
  EXPECT_TRUE(ExpectPairedKeyResultFrame());
  EXPECT_TRUE(ExpectConnectionResponseFrame(ConnectionResponseFrame::REJECT));

  FastForward(kIncomingRejectionDelay + kDelta);
  EXPECT_TRUE(connection_->IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest,
       IncomingConnectionKeyVerificationRunnerStatusUnable) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              GetToken());
  SetUpIntroductionFrameDecoder(/*return_empty_introduction_frame=*/false);

  SetConnectionType(ConnectionType::kWifi);
  NiceMock<MockTransferUpdateCallback> callback;
  absl::Notification notification;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke([&](const ShareTarget& share_target,
                                    const AttachmentContainer& container,
                                    TransferMetadata metadata) {
        EXPECT_FALSE(metadata.is_final_status());
        EXPECT_EQ(metadata.status(),
                  TransferMetadata::Status::kAwaitingLocalConfirmation);
        EXPECT_TRUE(share_target.is_incoming);
        EXPECT_TRUE(share_target.is_known);
        EXPECT_TRUE(container.HasAttachments());
        EXPECT_EQ(container.GetTextAttachments().size(), 3u);
        EXPECT_EQ(container.GetFileAttachments().size(), 1u);
        EXPECT_EQ(share_target.device_name, kDeviceName);
        EXPECT_EQ(share_target.type, kDeviceType);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_NE(share_target.device_id, kEndpointId);
        EXPECT_EQ(share_target.full_name, kTestMetadataFullName);
        EXPECT_EQ(metadata.token(), kFourDigitToken);
        notification.Notify();
      }));

  SetUpKeyVerification(/*is_incoming=*/true, PairedKeyResultFrame::UNABLE);
  SetUpForegroundReceiveSurface(callback);
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());

  service_->OnIncomingConnection(kEndpointId, CreateTestEndpointInfo(),
                                 connection_.get());
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));

  EXPECT_FALSE(connection_->IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest,
       IncomingConnectionKeyVerificationRunnerStatusUnableLowPower) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              GetToken());
  SetUpIntroductionFrameDecoder(/*return_empty_introduction_frame=*/false);

  SetConnectionType(ConnectionType::kWifi);
  NiceMock<MockTransferUpdateCallback> callback;
  absl::Notification notification;
  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke([&](const ShareTarget& share_target,
                                    const AttachmentContainer& container,
                                    TransferMetadata metadata) {
        EXPECT_FALSE(metadata.is_final_status());
        EXPECT_EQ(TransferMetadata::Status::kAwaitingLocalConfirmation,
                  metadata.status());
        EXPECT_TRUE(share_target.is_incoming);
        EXPECT_TRUE(share_target.is_known);
        EXPECT_TRUE(container.HasAttachments());
        EXPECT_EQ(container.GetTextAttachments().size(), 3u);
        EXPECT_EQ(container.GetFileAttachments().size(), 1u);
        EXPECT_EQ(share_target.device_name, kDeviceName);
        EXPECT_EQ(share_target.type, kDeviceType);
        EXPECT_TRUE(share_target.device_id);
        EXPECT_NE(share_target.device_id, kEndpointId);
        EXPECT_EQ(share_target.full_name, kTestMetadataFullName);

        EXPECT_EQ(kFourDigitToken, metadata.token());
        notification.Notify();
      }));

  SetUpKeyVerification(/*is_incoming=*/true, PairedKeyResultFrame::UNABLE);

  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());

  service_->OnIncomingConnection(kEndpointId, CreateTestEndpointInfo(),
                                 connection_.get());
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));

  EXPECT_FALSE(connection_->IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest,
       IncomingConnectionKeyVerificationRunnerStatusFail) {
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              GetToken());

  SetConnectionType(ConnectionType::kWifi);
  NiceMock<MockTransferUpdateCallback> callback;

  SetUpKeyVerification(/*is_incoming=*/true, PairedKeyResultFrame::FAIL);
  SetUpForegroundReceiveSurface(callback);

  // Ensures that introduction is never received for failed key verification.
  std::string intro = "introduction_frame";
  std::vector<uint8_t> bytes(intro.begin(), intro.end());
  connection_->AppendReadableData(bytes);
  FlushTesting();
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());
  EXPECT_CALL(callback,
              OnTransferUpdate(
                  testing::_, testing::_,
                  nearby::sharing::HasStatus(
                      TransferMetadata::Status::kDeviceAuthenticationFailed)));

  service_->OnIncomingConnection(kEndpointId, CreateTestEndpointInfo(),
                                 connection_.get());
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);

  // Ensure that the messages sent by ProcessLatestPublicCertificateDecryption
  // are processed prior to checking if connection is closed.
  EXPECT_TRUE(
      sharing_service_task_runner_->SyncWithTimeout(absl::Milliseconds(200)));
  EXPECT_TRUE(connection_->IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest,
       IncomingConnectionEmptyAuthTokenKeyVerificationRunnerStatusFail) {
  SetConnectionType(ConnectionType::kWifi);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_EVERYONE);
  NiceMock<MockTransferUpdateCallback> callback;

  SetUpForegroundReceiveSurface(callback);

  // Ensures that introduction is never received for empty auth token.
  std::string intro = "introduction_frame";
  std::vector<uint8_t> bytes(intro.begin(), intro.end());
  connection_->AppendReadableData(bytes);
  FlushTesting();
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());

  service_->OnIncomingConnection(kEndpointId, CreateTestEndpointInfo(),
                                 connection_.get());
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/true);

  EXPECT_TRUE(connection_->IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, RegisterReceiveSurfaceAlreadyReceiving) {
  NiceMock<MockTransferUpdateCallback> callback;
  SetUpIncomingConnection(callback);
  EXPECT_FALSE(connection_->IsClosed());

  EXPECT_EQ(
      RegisterReceiveSurface(
          &callback, NearbySharingService::ReceiveSurfaceState::kForeground),
      NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());

  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, RegisterReceiveSurfaceWhileDiscovering) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &transfer_callback,
      NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
}

TEST_F(NearbySharingServiceImplTest, SendAttachmentsWithoutAttachments) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  int64_t target_id =
      DiscoverShareTarget(transfer_callback, discovery_callback);

  EXPECT_EQ(SendAttachments(target_id, /*attachment_container=*/nullptr),
            NearbySharingServiceImpl::StatusCodes::kInvalidArgument);

  UnregisterSendSurface(&transfer_callback);
}

TEST_F(NearbySharingServiceImplTest, RegisterReceiveSurfaceWhileSending) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  int64_t target_id =
      SetUpOutgoingShareTarget(transfer_callback, discovery_callback);

  absl::Notification notification;
  ExpectTransferUpdates(transfer_callback, target_id,
                        {TransferMetadata::Status::kConnecting,
                         TransferMetadata::Status::kAwaitingRemoteAcceptance},
                        [&]() { notification.Notify(); });
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());
  EXPECT_EQ(SendAttachments(target_id, CreateTextAttachments({kTextPayload})),
            NearbySharingServiceImpl::StatusCodes::kOk);
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));

  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &transfer_callback,
      NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);

  UnregisterSendSurface(&transfer_callback);
}

TEST_F(NearbySharingServiceImplTest, SendTextAlreadySending) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  int64_t target_id =
      SetUpOutgoingShareTarget(transfer_callback, discovery_callback);

  absl::Notification notification;
  ExpectTransferUpdates(transfer_callback, target_id,
                        {TransferMetadata::Status::kConnecting,
                         TransferMetadata::Status::kAwaitingRemoteAcceptance},
                        [&]() { notification.Notify(); });
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());
  EXPECT_EQ(SendAttachments(target_id, CreateTextAttachments({kTextPayload})),
            NearbySharingServiceImpl::StatusCodes::kOk);
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));

  // We're now in the sending state, try to send again should fail
  EXPECT_EQ(SendAttachments(target_id, CreateTextAttachments({kTextPayload})),
            NearbySharingServiceImpl::StatusCodes::kOutOfOrderApiCall);

  UnregisterSendSurface(&transfer_callback);
}

TEST_F(NearbySharingServiceImplTest, SendTextWithoutScanning) {
  EXPECT_EQ(SendAttachments(325626L, CreateTextAttachments({kTextPayload})),
            NearbySharingServiceImpl::StatusCodes::kOutOfOrderApiCall);
}

TEST_F(NearbySharingServiceImplTest, SendTextUnknownTarget) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  DiscoverShareTarget(transfer_callback, discovery_callback);

  EXPECT_EQ(SendAttachments(12345L, CreateTextAttachments({kTextPayload})),
            NearbySharingServiceImpl::StatusCodes::kInvalidArgument);
  UnregisterSendSurface(&transfer_callback);
}

TEST_F(NearbySharingServiceImplTest, SendTextFailedCreateEndpointInfo) {
  // Set name with too many characters.
  local_device_data_manager()->SetDeviceName(std::string(300, 'a'));

  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  int64_t target_id =
      DiscoverShareTarget(transfer_callback, discovery_callback);

  EXPECT_EQ(SendAttachments(target_id, CreateTextAttachments({kTextPayload})),
            NearbySharingServiceImpl::StatusCodes::kError);

  UnregisterSendSurface(&transfer_callback);
}

TEST_F(NearbySharingServiceImplTest, SendTextFailedToConnect) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  // Call DiscoverShareTarget() instead of SetUpOutgoingShareTarget() as we want
  // to fail before key verification is done.
  int64_t target_id =
      DiscoverShareTarget(transfer_callback, discovery_callback);

  absl::Notification notification;
  ExpectTransferUpdates(transfer_callback, target_id,
                        {TransferMetadata::Status::kConnecting,
                         TransferMetadata::Status::kFailed},
                        [&]() { notification.Notify(); });
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());

  EXPECT_EQ(SendAttachments(target_id, CreateTextAttachments({kTextPayload})),
            NearbySharingServiceImpl::StatusCodes::kOk);
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));

  UnregisterSendSurface(&transfer_callback);
}

TEST_F(NearbySharingServiceImplTest, SendTextFailedKeyVerification) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  int64_t target_id =
      DiscoverShareTarget(transfer_callback, discovery_callback);

  absl::Notification notification;
  ExpectTransferUpdates(transfer_callback, target_id,
                        {TransferMetadata::Status::kConnecting,
                         TransferMetadata::Status::kDeviceAuthenticationFailed},
                        [&]() { notification.Notify(); });

  SetUpKeyVerification(/*is_incoming=*/false, PairedKeyResultFrame::FAIL);
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              GetToken());
  fake_nearby_connections_manager_->set_nearby_connection(connection_.get());
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());

  EXPECT_EQ(SendAttachments(target_id, CreateTextAttachments({kTextPayload})),
            NearbySharingServiceImpl::StatusCodes::kOk);
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));

  UnregisterSendSurface(&transfer_callback);
}

TEST_F(NearbySharingServiceImplTest, SendTextUnableToVerifyKey) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  int64_t target_id =
      DiscoverShareTarget(transfer_callback, discovery_callback);

  absl::Notification notification;
  ExpectTransferUpdates(transfer_callback, target_id,
                        {TransferMetadata::Status::kConnecting,
                         TransferMetadata::Status::kAwaitingRemoteAcceptance},
                        [&]() { notification.Notify(); });

  SetUpKeyVerification(/*is_incoming=*/false, PairedKeyResultFrame::UNABLE);
  fake_nearby_connections_manager_->SetRawAuthenticationToken(kEndpointId,
                                                              GetToken());
  fake_nearby_connections_manager_->set_nearby_connection(connection_.get());
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());

  EXPECT_EQ(SendAttachments(target_id, CreateTextAttachments({kTextPayload})),
            NearbySharingServiceImpl::StatusCodes::kOk);
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));

  UnregisterSendSurface(&transfer_callback);
}

INSTANTIATE_TEST_SUITE_P(NearbySharingServiceImplSendFailureTest,
                         NearbySharingServiceImplSendFailureTest,
                         testing::ValuesIn(kSendFailureTestData));

TEST_P(NearbySharingServiceImplSendFailureTest, SendTextRemoteFailure) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  int64_t target_id =
      SetUpOutgoingShareTarget(transfer_callback, discovery_callback);

  absl::Notification notification;
  ExpectTransferUpdates(transfer_callback, target_id,
                        {TransferMetadata::Status::kConnecting,
                         TransferMetadata::Status::kAwaitingRemoteAcceptance},
                        [&]() { notification.Notify(); });
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());

  EXPECT_EQ(SendAttachments(target_id, CreateTextAttachments({kTextPayload})),
            NearbySharingServiceImpl::StatusCodes::kOk);
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));

  // Verify data sent to the remote device so far.
  EXPECT_TRUE(ExpectPairedKeyEncryptionFrame());
  EXPECT_TRUE(ExpectPairedKeyResultFrame());
  EXPECT_TRUE(ExpectIntroductionFrame().has_value());

  // We're now waiting for the remote device to respond with the accept
  absl::Notification reject_notification;
  ExpectTransferUpdates(transfer_callback, target_id,
                        {GetParam().expected_status},
                        [&]() { reject_notification.Notify(); });

  // Cancel the transfer by rejecting it.
  SendConnectionResponse(GetParam().response_status);
  EXPECT_TRUE(reject_notification.WaitForNotificationWithTimeout(kWaitTimeout));

  EXPECT_TRUE(connection_->IsClosed());

  UnregisterSendSurface(&transfer_callback);
}

TEST_F(NearbySharingServiceImplTest, SendFileWithEmptyPath) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  int64_t target_id =
      DiscoverShareTarget(transfer_callback, discovery_callback);

  EXPECT_EQ(SendAttachments(target_id, CreateFileAttachments({""})),
            NearbySharingServiceImpl::StatusCodes::kInvalidArgument);

  UnregisterSendSurface(&transfer_callback);
}

TEST_P(NearbySharingServiceImplSendFailureTest, SendFilesRemoteFailure) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  int64_t target_id =
      SetUpOutgoingShareTarget(transfer_callback, discovery_callback);

  std::vector<uint8_t> test_data = {'T', 'e', 's', 't'};
  std::filesystem::path path = CreateTestFile("text.txt", test_data);

  absl::Notification notification;
  ExpectTransferUpdates(transfer_callback, target_id,
                        {TransferMetadata::Status::kConnecting,
                         TransferMetadata::Status::kAwaitingRemoteAcceptance},
                        [&]() { notification.Notify(); });
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());

  EXPECT_EQ(SendAttachments(target_id, CreateFileAttachments({path})),
            NearbySharingServiceImpl::StatusCodes::kOk);
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));

  // Verify data sent to the remote device so far.
  EXPECT_TRUE(ExpectPairedKeyEncryptionFrame());
  EXPECT_TRUE(ExpectPairedKeyResultFrame());
  EXPECT_TRUE(ExpectIntroductionFrame().has_value());

  // We're now waiting for the remote device to respond with the accept
  absl::Notification reject_notification;
  ExpectTransferUpdates(transfer_callback, target_id,
                        {GetParam().expected_status},
                        [&]() { reject_notification.Notify(); });

  // Cancel the transfer by rejecting it.
  SendConnectionResponse(GetParam().response_status);
  EXPECT_TRUE(reject_notification.WaitForNotificationWithTimeout(kWaitTimeout));

  EXPECT_TRUE(connection_->IsClosed());

  UnregisterSendSurface(&transfer_callback);
}

TEST_F(NearbySharingServiceImplTest, SendTextDisconnectTimeout) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  AccountManager::Account account;
  account.id = kTestAccountId;
  account_manager().SetAccount(account);
  int64_t target_id =
      SetUpOutgoingShareTarget(transfer_callback, discovery_callback);

  absl::Notification notification;
  ExpectTransferUpdates(transfer_callback, target_id,
                        {TransferMetadata::Status::kConnecting,
                         TransferMetadata::Status::kAwaitingRemoteAcceptance},
                        [&]() { notification.Notify(); });
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());

  EXPECT_EQ(SendAttachments(target_id, CreateTextAttachments({kTextPayload})),
            NearbySharingServiceImpl::StatusCodes::kOk);
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));

  // Verify data sent to the remote device so far.
  EXPECT_TRUE(ExpectPairedKeyEncryptionFrame());
  EXPECT_TRUE(ExpectPairedKeyResultFrame());
  std::optional<IntroductionFrame> intro = ExpectIntroductionFrame();

  ASSERT_TRUE(intro.has_value());
  ASSERT_EQ(intro->text_metadata_size(), 1);
  auto meta = intro->text_metadata(0);

  EXPECT_EQ(meta.text_title(), kTextPayload);
  EXPECT_EQ(static_cast<size_t>(meta.size()), strlen(kTextPayload));
  EXPECT_EQ(meta.type(), TextMetadata::TEXT);

  ASSERT_TRUE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId)
          .has_value());
  std::unique_ptr<Advertisement> advertisement =
      Advertisement::FromEndpointInfo(absl::Span<const uint8_t>(
          *fake_nearby_connections_manager_->connection_endpoint_info(
              kEndpointId)));
  ASSERT_TRUE(advertisement);
  EXPECT_EQ(advertisement->device_name(), kDeviceName);
  EXPECT_EQ(advertisement->device_type(), ShareTargetType::kLaptop);
  auto& test_metadata_key = GetNearbyShareTestEncryptedMetadataKey();
  EXPECT_EQ(advertisement->salt(), test_metadata_key.salt());
  EXPECT_EQ(advertisement->encrypted_metadata_key(),
            test_metadata_key.encrypted_key());

  PayloadInfo info = AcceptAndSendPayload(transfer_callback, target_id);
  FinishOutgoingTransfer(transfer_callback, target_id, /*complete=*/false,
                         info);

  // We should not have called disconnect yet as we want to wait for 1 minute to
  // make sure all outgoing packets have been sent properly.
  EXPECT_TRUE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId)
          .has_value());

  // Forward time until we send the disconnect request to Nearby
  FastForward(kOutgoingDisconnectionDelay);

  // Disconnect timeout calls FakeConnection::Close which does not call
  // ConnectionsManager::Disconnect, so the FakeConnectionsManager still thinks
  // the connection is open.
  EXPECT_TRUE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId)
          .has_value());

  UnregisterSendSurface(&transfer_callback);
  account_manager().SetAccount(std::nullopt);
}

TEST_F(NearbySharingServiceImplTest, SendTextSuccessClosedConnection) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  int64_t target_id =
      SetUpOutgoingShareTarget(transfer_callback, discovery_callback);
  SetUpOutgoingConnectionUntilAccept(transfer_callback, target_id);
  PayloadInfo info = AcceptAndSendPayload(transfer_callback, target_id);
  FinishOutgoingTransfer(transfer_callback, target_id, /*complete=*/true, info);

  // We should not have called disconnect yet as we want to wait for 1 minute
  // to make sure all outgoing packets have been sent properly.
  EXPECT_TRUE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId)
          .has_value());

  // Call disconnect on the connection early before the timeout has passed.
  sharing_service_task_runner_->PostTask([this]() { connection_->Close(); });
  EXPECT_TRUE(sharing_service_task_runner_->SyncWithTimeout(kTaskWaitTimeout));

  // FakeConnection::Close does not call ConnectionsManager::Disconnect, so
  // the FakeConnectionsManager still thinks the connection is open.
  // Expect that we haven't called disconnect again as the endpoint is already
  // disconnected.
  EXPECT_TRUE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId)
          .has_value());

  // Make sure the scheduled disconnect callback does nothing.
  FastForward(kOutgoingDisconnectionDelay);

  // The disconnection_timeout_alarm should have been cancelled, so
  // ConnectionsManager::Disconnect should not have been called.
  EXPECT_TRUE(
      fake_nearby_connections_manager_->connection_endpoint_info(kEndpointId)
          .has_value());

  UnregisterSendSurface(&transfer_callback);
}

TEST_F(NearbySharingServiceImplTest, SendFilesSuccess) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  int64_t target_id =
      SetUpOutgoingShareTarget(transfer_callback, discovery_callback);

  std::vector<uint8_t> test_data = {'T', 'e', 's', 't'};
  std::string file_name = "test.txt";
  std::filesystem::path path = CreateTestFile(file_name, test_data);

  absl::Notification introduction_notification;
  ExpectTransferUpdates(transfer_callback, target_id,
                        {TransferMetadata::Status::kConnecting,
                         TransferMetadata::Status::kAwaitingRemoteAcceptance},
                        [&]() { introduction_notification.Notify(); });
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());

  EXPECT_EQ(SendAttachments(target_id, CreateFileAttachments({path})),
            NearbySharingServiceImpl::StatusCodes::kOk);
  EXPECT_TRUE(
      introduction_notification.WaitForNotificationWithTimeout(kWaitTimeout));

  // Verify data sent to the remote device so far.
  EXPECT_TRUE(ExpectPairedKeyEncryptionFrame());
  EXPECT_TRUE(ExpectPairedKeyResultFrame());
  auto intro = ExpectIntroductionFrame();
  ASSERT_TRUE(intro.has_value());
  ASSERT_EQ(intro->file_metadata_size(), 1);
  auto meta = intro->file_metadata(0);

  EXPECT_EQ(meta.name(), file_name);
  EXPECT_EQ(meta.mime_type(), "text/plain");
  EXPECT_EQ(test_data.size(), static_cast<size_t>(meta.size()));
  EXPECT_EQ(meta.type(), FileMetadata::UNKNOWN);

  // Expect the file payload to be sent in the end.
  absl::Notification payload_notification;
  fake_nearby_connections_manager_->set_send_payload_callback(
      [&](std::unique_ptr<nearby::sharing::Payload> payload,
          std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>
              listener) {
        ASSERT_TRUE(payload->content.is_file());
        std::filesystem::path file = payload->content.file_payload.file.path;
        ASSERT_TRUE(std::filesystem::exists(file));

        payload_notification.Notify();
      });

  // We're now waiting for the remote device to respond with the accept
  // result.
  absl::Notification accept_notification;
  ExpectTransferUpdates(transfer_callback, target_id,
                        {TransferMetadata::Status::kInProgress},
                        [&]() { accept_notification.Notify(); });

  // Kick off send process by accepting the transfer from the remote device.
  SendConnectionResponse(ConnectionResponseFrame::ACCEPT);

  EXPECT_TRUE(accept_notification.WaitForNotificationWithTimeout(kWaitTimeout));
  EXPECT_TRUE(
      payload_notification.WaitForNotificationWithTimeout(kWaitTimeout));

  UnregisterSendSurface(&transfer_callback);
}

TEST_F(NearbySharingServiceImplTest, SendWifiCredentialsSuccess) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_sharing_feature::
          kEnableTransferCancellationOptimization,
      true);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  int64_t target_id =
      SetUpOutgoingShareTarget(transfer_callback, discovery_callback);

  absl::Notification introduction_notification;
  ExpectTransferUpdates(transfer_callback, target_id,
                        {TransferMetadata::Status::kConnecting,
                         TransferMetadata::Status::kAwaitingRemoteAcceptance},
                        [&]() { introduction_notification.Notify(); });
  EXPECT_CALL(*mock_app_info_, SetActiveFlag());

  EXPECT_EQ(SendAttachments(target_id, CreateWifiCredentialAttachments(
                                           "GoogleGuest", "password")),
            NearbySharingServiceImpl::StatusCodes::kOk);
  EXPECT_TRUE(
      introduction_notification.WaitForNotificationWithTimeout(kWaitTimeout));

  // Verify data sent to the remote device so far.
  EXPECT_TRUE(ExpectPairedKeyEncryptionFrame());
  EXPECT_TRUE(ExpectPairedKeyResultFrame());
  std::optional<IntroductionFrame> intro = ExpectIntroductionFrame();

  ASSERT_TRUE(intro.has_value());
  ASSERT_EQ(intro->wifi_credentials_metadata_size(), 1);
  auto meta = intro->wifi_credentials_metadata(0);

  EXPECT_EQ(meta.ssid(), "GoogleGuest");
  EXPECT_EQ(meta.security_type(),
            service::proto::WifiCredentialsMetadata::WPA_PSK);

  // Expect the wifi credential payload to be sent in the end.
  absl::Notification payload_notification;
  fake_nearby_connections_manager_->set_send_payload_callback(
      [&](std::unique_ptr<nearby::sharing::Payload> payload,
          std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>
              listener) {
        ASSERT_TRUE(payload->content.is_bytes());
        std::vector<uint8_t> bytes = payload->content.bytes_payload.bytes;
        nearby::sharing::service::proto::WifiCredentials wifi_credentials;
        ASSERT_TRUE(
            wifi_credentials.ParseFromArray(bytes.data(), bytes.size()));
        EXPECT_EQ(wifi_credentials.password(), "password");
        EXPECT_FALSE(wifi_credentials.hidden_ssid());

        payload_notification.Notify();
      });

  // We're now waiting for the remote device to respond with the accept
  // result.
  absl::Notification accept_notification;
  ExpectTransferUpdates(transfer_callback, target_id,
                        {TransferMetadata::Status::kInProgress},
                        [&]() { accept_notification.Notify(); });

  // Kick off send process by accepting the transfer from the remote device.
  SendConnectionResponse(ConnectionResponseFrame::ACCEPT);

  EXPECT_TRUE(accept_notification.WaitForNotificationWithTimeout(kWaitTimeout));
  EXPECT_TRUE(
      payload_notification.WaitForNotificationWithTimeout(kWaitTimeout));

  UnregisterSendSurface(&transfer_callback);
}

TEST_F(NearbySharingServiceImplTest, CancelSenderInitiator) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  int64_t target_id =
      SetUpOutgoingShareTarget(transfer_callback, discovery_callback);
  SetUpOutgoingConnectionUntilAccept(transfer_callback, target_id);
  PayloadInfo info = AcceptAndSendPayload(transfer_callback, target_id);

  // After we stop scanning, we check back in after kInvalidateDelay
  // milliseconds to make sure that we stopped in order to send a file and
  // not because the user left the page. We have to fast-forward here.
  // otherwise, we will hit this callback when trying to fast-forward by
  // kInitiatorCancelDelay below.
  FastForward(kInvalidateDelay);

  absl::Notification notification;
  EXPECT_CALL(transfer_callback,
              OnTransferUpdate(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke([&](const ShareTarget& share_target,
                                    const AttachmentContainer& container,
                                    TransferMetadata metadata) {
        EXPECT_EQ(share_target.id, target_id);
        EXPECT_EQ(metadata.status(), TransferMetadata::Status::kCancelled);
      }));
  EXPECT_FALSE(
      fake_nearby_connections_manager_->WasPayloadCanceled(info.payload_id));
  // The initiator of the cancellation explicitly calls Cancel().
  service_->Cancel(
      target_id, [&](NearbySharingServiceImpl::StatusCodes status_code) {
        EXPECT_EQ(status_code, NearbySharingServiceImpl::StatusCodes::kOk);
        notification.Notify();
      });
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  EXPECT_TRUE(
      fake_nearby_connections_manager_->WasPayloadCanceled(info.payload_id));

  // After the TransferMetadata::Status::kCancelled update, we expect other
  // classes to unregister the send surface.
  UnregisterSendSurface(&transfer_callback);

  // The initiator of the cancel should send a cancel frame to the other device,
  // then wait a few seconds before disconnecting to allow for processing on the
  // other device.
  EXPECT_TRUE(ExpectCancelFrame());
  EXPECT_FALSE(connection_->IsClosed());
  FastForward(kInitiatorCancelDelay);
  EXPECT_TRUE(connection_->IsClosed());
}

TEST_F(NearbySharingServiceImplTest, CancelSenderNoninitiator) {
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  int64_t target_id =
      SetUpOutgoingShareTarget(transfer_callback, discovery_callback);
  SetUpOutgoingConnectionUntilAccept(transfer_callback, target_id);
  PayloadInfo info = AcceptAndSendPayload(transfer_callback, target_id);

  absl::Notification notification;
  EXPECT_CALL(transfer_callback,
              OnTransferUpdate(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke([&](const ShareTarget& share_target,
                                    const AttachmentContainer& container,
                                    TransferMetadata metadata) {
        EXPECT_EQ(share_target.id, target_id);
        EXPECT_EQ(metadata.status(), TransferMetadata::Status::kCancelled);
        notification.Notify();
      }));
  EXPECT_FALSE(
      fake_nearby_connections_manager_->WasPayloadCanceled(info.payload_id));
  // The non-initiator of the cancellation processes a cancellation frame from
  // the initiator.
  SendCancel();
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  EXPECT_TRUE(
      fake_nearby_connections_manager_->WasPayloadCanceled(info.payload_id));

  // The non-initiator should close the connection immediately
  EXPECT_TRUE(connection_->IsClosed());
}

TEST_F(NearbySharingServiceImplTest, CancelReceiverInitiator) {
  NiceMock<MockTransferUpdateCallback> transfer_callback;
  int64_t target_id = SetUpIncomingConnection(transfer_callback);
  AcceptConnection(transfer_callback, target_id, kEndpointId);
  ASSERT_TRUE(ExpectPairedKeyEncryptionFrame());
  ASSERT_TRUE(ExpectPairedKeyResultFrame());

  absl::Notification notification;
  EXPECT_CALL(transfer_callback,
              OnTransferUpdate(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke([&](const ShareTarget& share_target,
                                    const AttachmentContainer& container,
                                    TransferMetadata metadata) {
        EXPECT_EQ(share_target.id, target_id);
        EXPECT_EQ(metadata.status(), TransferMetadata::Status::kCancelled);
      }));
  EXPECT_FALSE(
      fake_nearby_connections_manager_->WasPayloadCanceled(kFilePayloadId));
  // The initiator of the cancellation explicitly calls Cancel().
  service_->Cancel(
      target_id, [&](NearbySharingServiceImpl::StatusCodes status_code) {
        EXPECT_EQ(NearbySharingServiceImpl::StatusCodes::kOk, status_code);
        notification.Notify();
      });
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  EXPECT_TRUE(
      fake_nearby_connections_manager_->WasPayloadCanceled(kFilePayloadId));

  // After the TransferMetadata::Status::kCancelled update, we expect other
  // classes to unregister the receive surface.
  UnregisterReceiveSurface(&transfer_callback);

  // This is a response frame generated by accept().
  GetWrittenFrame();
  // The initiator of the cancel should send a cancel frame to the other device,
  // then wait a few seconds before disconnecting to allow for processing on the
  // other device.
  ASSERT_TRUE(ExpectCancelFrame());
  EXPECT_FALSE(connection_->IsClosed());
  FastForward(kInitiatorCancelDelay);
  EXPECT_TRUE(connection_->IsClosed());
}

TEST_F(NearbySharingServiceImplTest, CancelReceiverNoninitiator) {
  NiceMock<MockTransferUpdateCallback> transfer_callback;
  int64_t target_id = SetUpIncomingConnection(transfer_callback);
  AcceptConnection(transfer_callback, target_id, kEndpointId);
  ExpectPairedKeyEncryptionFrame();
  ExpectPairedKeyResultFrame();

  absl::Notification notification;
  EXPECT_CALL(transfer_callback,
              OnTransferUpdate(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke([&](const ShareTarget& share_target,
                                    const AttachmentContainer& container,
                                    TransferMetadata metadata) {
        EXPECT_EQ(target_id, share_target.id);
        EXPECT_EQ(TransferMetadata::Status::kCancelled, metadata.status());
        notification.Notify();
      }));
  EXPECT_FALSE(
      fake_nearby_connections_manager_->WasPayloadCanceled(kFilePayloadId));
  // The non-initiator of the cancellation processes a cancellation frame from
  // the initiator.
  SendCancel();
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  EXPECT_TRUE(
      fake_nearby_connections_manager_->WasPayloadCanceled(kFilePayloadId));

  // The non-initiator should close the connection immediately
  EXPECT_TRUE(connection_->IsClosed());
}

TEST_F(NearbySharingServiceImplTest,
       RegisterForegroundReceiveSurfaceEntersHighVisibility) {
  TestObserver observer(service_.get());
  NiceMock<MockTransferUpdateCallback> callback;

  SetConnectionType(ConnectionType::kWifi);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_EVERYONE);
  local_device_data_manager()->SetDeviceName(kDeviceName);

  // To start, we should not be in high visibility state.
  EXPECT_FALSE(observer.in_high_visibility_);
  EXPECT_FALSE(observer.on_start_advertising_failure_called_);

  // If we register a foreground surface we should end up in high visibility
  // state.
  SetUpForegroundReceiveSurface(callback);

  // At this point we should have a new high visibility state and the observer
  // should have been called as well.
  EXPECT_TRUE(observer.in_high_visibility_);
  EXPECT_FALSE(observer.on_start_advertising_failure_called_);

  // If we unregister the foreground receive surface we should no longer be in
  // high visibility and the observer should be notified.
  EXPECT_EQ(UnregisterReceiveSurface(&callback),
            NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(observer.in_high_visibility_);

  // Remove the observer before it goes out of scope.
  service_->RemoveObserver(&observer);
  FlushTesting();
}

TEST_F(NearbySharingServiceImplTest, AddObserverSendsInitialAdapterState) {
  SetBluetoothIsPresent(true);
  SetBluetoothIsPowered(false);
  SetWifiIsPresent(false);
  SetWifiIsPowered(false);
  SetLanIsConnected(true);

  TestObserver observer(service_.get());
  FlushTesting();

  EXPECT_EQ(observer.bluetooth_state_,
            NearbySharingService::Observer::AdapterState::DISABLED);
  EXPECT_EQ(observer.lan_state_,
            NearbySharingService::Observer::AdapterState::ENABLED);

  // Remove the observer before it goes out of scope.
  service_->RemoveObserver(&observer);
}

TEST_F(NearbySharingServiceImplTest, AddObserverBluetoothAdapterUpdate) {
  SetBluetoothIsPresent(false);
  SetBluetoothIsPowered(false);
  SetWifiIsPresent(false);
  SetWifiIsPowered(false);
  SetLanIsConnected(false);

  TestObserver observer(service_.get());
  FlushTesting();
  EXPECT_EQ(observer.bluetooth_state_,
            NearbySharingService::Observer::AdapterState::NOT_PRESENT);

  SetBluetoothIsPresent(true);
  SetBluetoothIsPowered(false);
  EXPECT_EQ(observer.bluetooth_state_,
            NearbySharingService::Observer::AdapterState::DISABLED);

  SetBluetoothIsPresent(true);
  SetBluetoothIsPowered(true);
  EXPECT_EQ(observer.bluetooth_state_,
            NearbySharingService::Observer::AdapterState::ENABLED);

  SetBluetoothIsPresent(false);
  SetBluetoothIsPowered(true);
  EXPECT_EQ(observer.bluetooth_state_,
            NearbySharingService::Observer::AdapterState::NOT_PRESENT);

  // Remove the observer before it goes out of scope.
  service_->RemoveObserver(&observer);
}

TEST_F(NearbySharingServiceImplTest, AddObserverLanAdapterUpdate) {
  SetBluetoothIsPresent(false);
  SetBluetoothIsPowered(false);
  SetWifiIsPresent(false);
  SetWifiIsPowered(false);
  SetLanIsConnected(false);

  TestObserver observer(service_.get());
  FlushTesting();
  EXPECT_EQ(observer.lan_state_,
            NearbySharingService::Observer::AdapterState::DISABLED);

  SetLanIsConnected(true);
  EXPECT_EQ(observer.lan_state_,
            NearbySharingService::Observer::AdapterState::ENABLED);

  // Remove the observer before it goes out of scope.
  service_->RemoveObserver(&observer);
}

TEST_F(NearbySharingServiceImplTest, ShutdownCallsObservers) {
  TestObserver observer(service_.get());
  EXPECT_FALSE(observer.shutdown_called_);
  Shutdown();
  EXPECT_TRUE(observer.shutdown_called_);
  // Prevent a double shutdown.
  service_.reset();
}

TEST_F(NearbySharingServiceImplTest, RotateBackgroundAdvertisementPeriodic) {
  certificate_manager()->set_next_salt({0x00, 0x01});
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  NiceMock<MockTransferUpdateCallback> callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  auto endpoint_info_initial =
      fake_nearby_connections_manager_->advertising_endpoint_info();

  certificate_manager()->set_next_salt({0x00, 0x02});
  FastForward(absl::Seconds(870));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  auto endpoint_info_rotated =
      fake_nearby_connections_manager_->advertising_endpoint_info();
  EXPECT_NE(endpoint_info_initial, endpoint_info_rotated);
}

TEST_F(NearbySharingServiceImplTest,
       RotateBackgroundAdvertisementPrivateCertificatesChange) {
  certificate_manager()->set_next_salt({0x00, 0x01});
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  NiceMock<MockTransferUpdateCallback> callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  auto endpoint_info_initial =
      fake_nearby_connections_manager_->advertising_endpoint_info();

  certificate_manager()->set_next_salt({0x00, 0x02});
  certificate_manager()->NotifyPrivateCertificatesChanged();
  EXPECT_TRUE(sharing_service_task_runner_->SyncWithTimeout(kTaskWaitTimeout));
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  auto endpoint_info_rotated =
      fake_nearby_connections_manager_->advertising_endpoint_info();
  EXPECT_NE(endpoint_info_initial, endpoint_info_rotated);
  UnregisterReceiveSurface(&callback);
  EXPECT_TRUE(sharing_service_task_runner_->SyncWithTimeout(kTaskWaitTimeout));
}

TEST_F(NearbySharingServiceImplTest, OrderedEndpointDiscoveryEvents) {
  SetConnectionType(ConnectionType::kWifi);

  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;

  // Start discovering, to ensure a discovery listener is registered.
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  // Ensure that the endpoint discovered and lost event are process
  // sequentially. This is particularly important due to the asynchronous
  // operations needed to handle endpoint discovery.
  //
  // Order of events:
  //   - Nearby Connections discovers endpoint 1
  //   - Nearby Connections loses endpoint 1
  //   - Nearby Share processes these two events in order.
  //   - Nearby Connections discovers endpoint 2
  //   - Nearby Connections discovers endpoint 3
  //   - Nearby Connections loses endpoint 3
  //   - Nearby Connections loses endpoint 2
  //   - Nearby Share processes these four events in order.
  {
    absl::Notification notification;
    FindEndpoint(/*endpoint_id=*/"1");
    LoseEndpoint(/*endpoint_id=*/"1");
    InSequence s;
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered);
    EXPECT_CALL(discovery_callback, OnShareTargetLost)
        .WillOnce([&](ShareTarget share_target) { notification.Notify(); });

    // Needed for discovery processing.
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                             /*success=*/true);
    EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  }
  {
    absl::Notification notification;
    FindEndpoint(/*endpoint_id=*/"2");
    FindEndpoint(/*endpoint_id=*/"3");
    LoseEndpoint(/*endpoint_id=*/"3");
    LoseEndpoint(/*endpoint_id=*/"2");
    InSequence s;
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered)
        .WillOnce([](ShareTarget share_target) {
          EXPECT_EQ(share_target.device_id, "2");
        });
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered)
        .WillOnce([](ShareTarget share_target) {
          EXPECT_EQ(share_target.device_id, "3");
        });
    EXPECT_CALL(discovery_callback, OnShareTargetLost)
        .WillOnce([](ShareTarget share_target) {
          EXPECT_EQ(share_target.device_id, "3");
        });
    EXPECT_CALL(discovery_callback, OnShareTargetLost)
        .WillOnce([&](ShareTarget share_target) {
          EXPECT_EQ(share_target.device_id, "2");
          notification.Notify();
        });

    // Needed for discovery processing. Fail, then the ShareTarget device ID is
    // set to the endpoint ID, which we use above to verify the correct endpoint
    // ID processing order.
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/2,
                                             /*success=*/false);
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/3,
                                             /*success=*/false);

    EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  }
}

TEST_F(NearbySharingServiceImplTest,
       RetryDiscoveredEndpointsNoDownloadIfDecryption) {
  // Start discovery.
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  RegisterSendSurface(&transfer_callback, &discovery_callback,
                      SendSurfaceState::kForeground);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            1u);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
  // Order of events:
  // - Discover endpoint 1 --> decrypts public certificate
  // - Fire certificate download timer --> no download because no cached
  //                                       advertisements
  {
    absl::Notification notification;
    FindEndpoint(/*endpoint_id=*/"1");
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered)
        .WillOnce([&](ShareTarget share_target) { notification.Notify(); });
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                             /*success=*/true);
    EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  }

  FastForward(kCertificateDownloadDuringDiscoveryPeriod);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            1u);

  EXPECT_CALL(discovery_callback, OnShareTargetLost);
  Shutdown();
  service_.reset();
}

TEST_F(NearbySharingServiceImplTest, DedupSameEndpointId) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_sharing_feature::kApplyEndpointsDedup,
      true);
  // Start discovery.
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  RegisterSendSurface(&transfer_callback, &discovery_callback,
                      SendSurfaceState::kForeground);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            1u);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
  {
    absl::Notification notification;
    // vendor_id is default to 0.
    FindEndpoint(/*endpoint_id=*/"1");
    FindEndpointWithVendorId(
        /*endpoint_id=*/"1",
        static_cast<uint8_t>(Advertisement::BlockedVendorId::kSamsung));

    ::testing::InSequence s;
    ShareTarget share_target_1;
    ShareTarget share_target_2;
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered(_))
        .WillOnce(SaveArg<0>(&share_target_1));

    EXPECT_CALL(discovery_callback, OnShareTargetUpdated(_))
        .WillOnce([&](ShareTarget share_target) {
          share_target_2 = share_target;
          notification.Notify();
        });

    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                             /*success=*/true);
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/2,
                                             /*success=*/true);
    EXPECT_EQ(share_target_1.id, share_target_2.id);
    // Vendor_id updated.
    EXPECT_EQ(share_target_1.vendor_id, 0);
    EXPECT_EQ(share_target_2.vendor_id,
              static_cast<uint8_t>(Advertisement::BlockedVendorId::kSamsung));
    EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  }
  Shutdown();
  service_.reset();
}

TEST_F(NearbySharingServiceImplTest,
       OnLostDedupSameEndpointIdBeforeExpiryNoOnShareTargetLost) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_sharing_feature::kApplyEndpointsDedup,
      true);
  // Start discovery.
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  RegisterSendSurface(&transfer_callback, &discovery_callback,
                      SendSurfaceState::kForeground);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            1u);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
  {
    absl::Notification notification;
    ::testing::InSequence s;
    ShareTarget share_target_1;
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered(_))
        .WillOnce(SaveArg<0>(&share_target_1));
    // vendor_id is default to 0.
    FindEndpoint(/*endpoint_id=*/"1");
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                             /*success=*/true);
    // Finish processing all the HandleEndpointDiscovered events.
    FlushTesting();

    // No call to OnShareTargetLost.
    LoseEndpoint(/*endpoint_id=*/"1");

    // Fastforward to just before the cache expiry time.
    FastForward(
        absl::Milliseconds(NearbyFlags::GetInstance().GetInt64Flag(
                               config_package_nearby::nearby_sharing_feature::
                                   kDiscoveryCacheLostExpiryMs) -
                           10));
    FindEndpointWithVendorId(
        /*endpoint_id=*/"1",
        static_cast<uint8_t>(Advertisement::BlockedVendorId::kSamsung));
    ShareTarget share_target_2;
    EXPECT_CALL(discovery_callback, OnShareTargetUpdated(_))
        .WillOnce([&](ShareTarget share_target) {
          share_target_2 = share_target;
          notification.Notify();
        });

    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/2,
                                             /*success=*/true);
    EXPECT_EQ(share_target_1.id, share_target_2.id);
    // Vendor_id updated.
    EXPECT_EQ(share_target_1.vendor_id, 0);
    EXPECT_EQ(share_target_2.vendor_id,
              static_cast<uint8_t>(Advertisement::BlockedVendorId::kSamsung));
    EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  }
  EXPECT_CALL(discovery_callback, OnShareTargetLost).Times(1);
  Shutdown();
  service_.reset();
}

TEST_F(NearbySharingServiceImplTest, OnLostDedupSameEndpointIdAfterExpiry) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_sharing_feature::kApplyEndpointsDedup,
      true);
  // Start discovery.
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  RegisterSendSurface(&transfer_callback, &discovery_callback,
                      SendSurfaceState::kForeground);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            1u);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
  {
    absl::Notification notification;
    ::testing::InSequence s;
    ShareTarget share_target_1;

    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered(_))
        .WillOnce(SaveArg<0>(&share_target_1));
    // vendor_id is default to 0.
    FindEndpoint(/*endpoint_id=*/"1");
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                             /*success=*/true);
    // Finish processing all the HandleEndpointDiscovered related events before
    // fast forwarding to avoid race condition.
    FlushTesting();

    ShareTarget share_target_1_lost;
    EXPECT_CALL(discovery_callback, OnShareTargetLost)
        .WillOnce(SaveArg<0>(&share_target_1_lost));
    LoseEndpoint(/*endpoint_id=*/"1");

    // Fast forward to after the cache expiry time.
    FastForward(
        absl::Milliseconds(NearbyFlags::GetInstance().GetInt64Flag(
                               config_package_nearby::nearby_sharing_feature::
                                   kDiscoveryCacheLostExpiryMs) +
                           10));
    FindEndpointWithVendorId(
        /*endpoint_id=*/"1",
        static_cast<uint8_t>(Advertisement::BlockedVendorId::kSamsung));
    ShareTarget share_target_2;
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered(_))
        .WillOnce([&](ShareTarget share_target) {
          share_target_2 = share_target;
          notification.Notify();
        });

    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/2,
                                             /*success=*/true);
    EXPECT_EQ(share_target_1_lost.id, share_target_1.id);
    // Cache entry expires and the share_target ID is not preserved.
    EXPECT_NE(share_target_1.id, share_target_2.id);

    EXPECT_EQ(share_target_1.vendor_id, 0);
    EXPECT_EQ(share_target_2.vendor_id,
              static_cast<uint8_t>(Advertisement::BlockedVendorId::kSamsung));
    EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  }
  EXPECT_CALL(discovery_callback, OnShareTargetLost).Times(1);
  Shutdown();
  service_.reset();
}

TEST_F(NearbySharingServiceImplTest,
       RetryDiscoveredEndpointsDownloadCertsAndRetryDecryption) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_sharing_feature::kApplyEndpointsDedup,
      false);
  // Start discovery.
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  RegisterSendSurface(&transfer_callback, &discovery_callback,
                      SendSurfaceState::kForeground);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            1u);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
  // Order of events:
  // - Discover endpoint 1 --> decrypts public certificate
  // - Discover endpoint 2 --> cannot decrypt public certificate
  // - Discover endpoint 3 --> decrypts public certificate
  // - Discover endpoint 4 --> cannot decrypt public certificate
  // - Lose endpoint 3
  // - Fire certificate download timer --> certificates downloaded
  // - (Re)discover endpoints 2 and 4
  {
    absl::Notification notification;
    FindInvalidEndpoint(/*endpoint_id=*/"1");
    FindInvalidEndpoint(/*endpoint_id=*/"2");
    FindInvalidEndpoint(/*endpoint_id=*/"3");
    FindInvalidEndpoint(/*endpoint_id=*/"4");
    LoseEndpoint(/*endpoint_id=*/"3");
    ::testing::InSequence s;
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered).Times(2);
    EXPECT_CALL(discovery_callback, OnShareTargetLost)
        .WillOnce([&](ShareTarget share_target) { notification.Notify(); });
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                             /*success=*/true);
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/2,
                                             /*success=*/false);
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/3,
                                             /*success=*/true);
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/4,
                                             /*success=*/false);
    EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  }
  FastForward(kCertificateDownloadDuringDiscoveryPeriod);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            2u);
  certificate_manager()->NotifyPublicCertificatesDownloaded();
  FlushTesting();
  {
    absl::Notification notification;
    ::testing::InSequence s;
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered);
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered)
        .WillOnce([&](ShareTarget share_target) { notification.Notify(); });
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/5,
                                             /*success=*/true);
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/6,
                                             /*success=*/true);
    EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  }
  EXPECT_CALL(discovery_callback, OnShareTargetLost).Times(3);
  Shutdown();
  service_.reset();
}

// This test verifies the de-dup logic. Since certificates are the same, all the
// share targets are duplicates.
TEST_F(NearbySharingServiceImplTest, EndpointDedupBasedOnDeviceId) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_sharing_feature::kApplyEndpointsDedup,
      true);
  // Make kDiscoveryCacheLostExpiryMs larger than
  // kCertificateDownloadDuringDiscoveryPeriod (10s).
  NearbyFlags::GetInstance().OverrideInt64FlagValue(
      config_package_nearby::nearby_sharing_feature::
          kDiscoveryCacheLostExpiryMs,
      20000);  // 20s
  // Start discovery.
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  RegisterSendSurface(&transfer_callback, &discovery_callback,
                      SendSurfaceState::kForeground);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            1u);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
  // Order of events:
  // - Discover endpoint 1 --> decrypts public certificate
  // - Discover endpoint 2 --> cannot decrypt public certificate
  // - Discover endpoint 3 --> decrypts public certificate
  //                       --> De-dup endpoint 1 with endpoint 3
  // - Discover endpoint 4 --> cannot decrypt public certificate
  // - Lose endpoint 3     --> endpoint 3 is added to discovery_cache
  // ----------------------------------------------------------------
  // - Fire certificate download timer --> certificates downloaded
  // - {x|y} can be 2 or 4, as the (Re)discover order is non-deterministic
  // - (Re)discover endpoints x before endpoint 3 discovery cache expiry
  //                            --> De-dup endpoint 3 with endpoint x
  // - (Re)discover endpoints y --> De-dup endpoint x with endpoint y
  {
    absl::Notification notification;
    ::testing::InSequence s;
    // vendor_id is default to 0.
    FindEndpoint(/*endpoint_id=*/"1");
    ShareTarget share_target_ep_1;
    EXPECT_CALL(discovery_callback, OnShareTargetDiscovered(_))
        .WillOnce(SaveArg<0>(&share_target_ep_1));
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                             /*success=*/true);
    FlushTesting();

    FindInvalidEndpoint(/*endpoint_id=*/"2");
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/2,
                                             /*success=*/false);
    FlushTesting();

    FindEndpointWithVendorId(
        /*endpoint_id=*/"3",
        static_cast<uint8_t>(Advertisement::BlockedVendorId::kSamsung));
    // Update endpoint 1 to endpoint 3
    ShareTarget share_target_ep_3;
    EXPECT_CALL(discovery_callback, OnShareTargetUpdated(_))
        .WillOnce([&](ShareTarget share_target) {
          share_target_ep_3 = share_target;
          notification.Notify();
        });

    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/3,
                                             /*success=*/true);
    FlushTesting();

    FindInvalidEndpoint(/*endpoint_id=*/"4");
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/4,
                                             /*success=*/false);
    FlushTesting();

    LoseEndpoint(/*endpoint_id=*/"3");
    FlushTesting();

    EXPECT_EQ(share_target_ep_1.id, share_target_ep_3.id);
    // Vendor_id updated.
    EXPECT_EQ(share_target_ep_3.vendor_id,
              static_cast<uint8_t>(Advertisement::BlockedVendorId::kSamsung));
    EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  }
  // kDiscoveryCacheLostExpiryMs is set to be larger than
  // kCertificateDownloadDuringDiscoveryPeriod so the cache_entry is unexpired.
  FastForward(kCertificateDownloadDuringDiscoveryPeriod);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            2u);
  certificate_manager()->NotifyPublicCertificatesDownloaded();
  FlushTesting();
  ShareTarget share_target_updated;
  {
    absl::Notification notification;
    ::testing::InSequence s;
    // Update endpoint 3
    EXPECT_CALL(discovery_callback, OnShareTargetUpdated(_)).Times(1);
    // One of the re-discovered endpoint updates the other one. The ordering
    // is non-deterministic.
    EXPECT_CALL(discovery_callback, OnShareTargetUpdated)
        .WillOnce([&](ShareTarget share_target) {
          share_target_updated = share_target;
          notification.Notify();
        });
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/5,
                                             /*success=*/true);
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/6,
                                             /*success=*/true);
    EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  }
  EXPECT_CALL(discovery_callback, OnShareTargetLost).Times(1);

  FlushTesting();

  // Send Text to the updated endpoint.
  SetUpKeyVerification(
      /*is_incoming=*/false, PairedKeyResultFrame::SUCCESS);
  // Since we don't know exactly which endpoint is updated, we need to set the
  // token for both of them.
  fake_nearby_connections_manager_->SetRawAuthenticationToken(
      /*endpoint_id=*/"4", GetToken());
  fake_nearby_connections_manager_->SetRawAuthenticationToken(
      /*endpoint_id=*/"2", GetToken());
  fake_nearby_connections_manager_->set_nearby_connection(connection_.get());

  SetUpOutgoingConnectionUntilAccept(transfer_callback,
                                     share_target_updated.id);

  PayloadInfo info =
      AcceptAndSendPayload(transfer_callback, share_target_updated.id);
  FinishOutgoingTransfer(transfer_callback, share_target_updated.id,
                         /*complete=*/true, info);

  EXPECT_TRUE(fake_nearby_connections_manager_
                  ->connection_endpoint_info(/*endpoint_id=*/"4")
                  .has_value() ||
              fake_nearby_connections_manager_
                  ->connection_endpoint_info(/*endpoint_id=*/"2")
                  .has_value());

  Shutdown();
  service_.reset();
}

TEST_F(NearbySharingServiceImplTest,
       RetryDiscoveredEndpointsDiscoveryRestartClearsCache) {
  // Start discovery.
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  RegisterSendSurface(&transfer_callback, &discovery_callback,
                      SendSurfaceState::kForeground);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            1u);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
  // Order of events:
  // - Discover endpoint 1 --> cannot decrypt public certificate
  // - Stop discovery
  // - Certificate download timer not running; not discovering
  // - Start discovery
  // - Fire certificate download timer --> certificates not downloaded; cached
  //                                       advertisement map has been cleared
  FindInvalidEndpoint(/*endpoint_id=*/"1");
  InSequence s;
  EXPECT_CALL(discovery_callback, OnShareTargetDiscovered).Times(0);
  EXPECT_CALL(discovery_callback, OnShareTargetLost).Times(0);
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/false);
  UnregisterSendSurface(&transfer_callback);
  FastForward(kCertificateDownloadDuringDiscoveryPeriod);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            1u);
  RegisterSendSurface(&transfer_callback, &discovery_callback,
                      SendSurfaceState::kForeground);
  // Note: Certificate downloads are also requested in RegisterSendSurface; this
  // is not related to the retry timer.
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            2u);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
  FastForward(kCertificateDownloadDuringDiscoveryPeriod);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            2u);
  Shutdown();
  service_.reset();
}

TEST_F(NearbySharingServiceImplTest,
       RetryDiscoveredEndpointsWhenCannotDecrypted) {
  // Start discovery.
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  RegisterSendSurface(&transfer_callback, &discovery_callback,
                      SendSurfaceState::kForeground);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            1u);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
  FindInvalidEndpoint(/*endpoint_id=*/"1");

  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/false);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            1u);
  // Because there is one pending advertisement to resolve, it will cause public
  // certificates download.
  FastForward(kCertificateDownloadDuringDiscoveryPeriod);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            2u);
  certificate_manager()->NotifyPublicCertificatesDownloaded();
  FlushTesting();
  // Don't download public certificates when it is tried.
  FastForward(kCertificateDownloadDuringDiscoveryPeriod);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            2u);
  FlushTesting();
  // Don't download public certificates in case of the endpoint is discovered
  // again.
  FindInvalidEndpoint(/*endpoint_id=*/"1");
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/2,
                                           /*success=*/false);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            2u);
  Shutdown();
  service_.reset();
}

TEST_F(NearbySharingServiceImplTest, RetryDiscoveredEndpointsDownloadLimit) {
  // Start discovery.
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  RegisterSendSurface(&transfer_callback, &discovery_callback,
                      SendSurfaceState::kForeground);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            1u);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
  // Order of events:
  // - x3:
  //   - (Re)discover endpoint 1 --> cannot decrypt public certificate
  //   - Fire certificate download timer --> certificates downloaded
  // - Rediscover endpoint 1 --> cannot decrypt public certificate
  // - Fire certificate download timer --> no download; limit reached
  // - Restart discovery which resets limit counter
  for (size_t i = 1; i <= kMaxCertificateDownloadsDuringDiscovery; ++i) {
    FindInvalidEndpoint(/*endpoint_id=*/absl::StrCat(i));
  }

  for (size_t i = 1; i <= kMaxCertificateDownloadsDuringDiscovery; ++i) {
    SCOPED_TRACE(i);
    ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/i,
                                             /*success=*/false);
    FastForward(kCertificateDownloadDuringDiscoveryPeriod);
    EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
              1u + i);
    certificate_manager()->NotifyPublicCertificatesDownloaded();
    FlushTesting();
  }
  FastForward(kCertificateDownloadDuringDiscoveryPeriod);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            1u + kMaxCertificateDownloadsDuringDiscovery);
  UnregisterSendSurface(&transfer_callback);
  RegisterSendSurface(&transfer_callback, &discovery_callback,
                      SendSurfaceState::kForeground);
  // Note: Certificate downloads are also requested in RegisterSendSurface; this
  // is not related to the retry timer.
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            2u + kMaxCertificateDownloadsDuringDiscovery);
  FindInvalidEndpoint(/*endpoint_id=*/"1");
  ProcessLatestPublicCertificateDecryption(
      /*expected_num_calls=*/1u + kMaxCertificateDownloadsDuringDiscovery,
      /*success=*/false);
  FastForward(kCertificateDownloadDuringDiscoveryPeriod);
  EXPECT_EQ(certificate_manager()->num_download_public_certificates_calls(),
            3u + kMaxCertificateDownloadsDuringDiscovery);

  Shutdown();
  service_.reset();
}

TEST_F(NearbySharingServiceImplTest,
       ScreenLockedRegisterReceiveSurfaceNotAdvertising) {
  SetScreenLocked(true);
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest, BlockTargetWithSameVendorId) {
  InSequence s;
  // Register send surface with vendor ID 1 that requests blocking.
  MockTransferUpdateCallback callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  ASSERT_EQ(RegisterSendSurface(&callback, &discovery_callback,
                                SendSurfaceState::kForeground,
                                Advertisement::BlockedVendorId::kSamsung),
            NearbySharingService::StatusCodes::kOk);
  // Verify service will not report discovered.
  EXPECT_CALL(discovery_callback, OnShareTargetDiscovered(testing::_)).Times(0);
  // Find endpoint with vendor ID 1.
  FindEndpointWithVendorId(
      /*endpoint_id=*/"1",
      static_cast<uint8_t>(Advertisement::BlockedVendorId::kSamsung));
  ProcessLatestPublicCertificateDecryption(/*expected_num_calls=*/1,
                                           /*success=*/false);
}

TEST_F(NearbySharingServiceImplTest,
       RegisterSendSurfaceWithDifferentVendorIdIsBlocked) {
  SetConnectionType(ConnectionType::kWifi);
  preference_manager().SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS));
  FlushTesting();

  // Register background send surface with vendor ID 1.
  MockShareTargetDiscoveredCallback background_discovered_callback;
  MockTransferUpdateCallback background_transfer_callback;
  NearbySharingService::StatusCodes result = RegisterSendSurface(
      &background_transfer_callback, &background_discovered_callback,
      NearbySharingService::SendSurfaceState::kBackground,
      Advertisement::BlockedVendorId::kSamsung);
  ASSERT_EQ(result, NearbySharingService::StatusCodes::kOk);
  // Register foreground send surface with different vendor ID.
  MockShareTargetDiscoveredCallback foreground_discovered_callback;
  MockTransferUpdateCallback foreground_transfer_callback;
  result = RegisterSendSurface(
      &foreground_transfer_callback, &foreground_discovered_callback,
      NearbySharingService::SendSurfaceState::kForeground,
      static_cast<Advertisement::BlockedVendorId>(2));
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kInvalidArgument);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_F(NearbySharingServiceImplTest, ScreenLocksDuringAdvertising) {
  SetConnectionType(ConnectionType::kWifi);
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());

  SetScreenLocked(true);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());

  SetScreenLocked(false);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  EXPECT_FALSE(fake_nearby_connections_manager_->is_shutdown());
}

TEST_F(NearbySharingServiceImplTest, ScreenLocksDuringDiscovery) {
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;
  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  SetScreenLocked(true);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsDiscovering());
  SetScreenLocked(false);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());
}

TEST_F(NearbySharingServiceImplTest, CreateShareTarget) {
  std::unique_ptr<Advertisement> advertisement = Advertisement::NewInstance(
      GetNearbyShareTestEncryptedMetadataKey().salt(),
      GetNearbyShareTestEncryptedMetadataKey().encrypted_key(), kDeviceType,
      kDeviceName, kVendorId);

  // Flip |for_self_share| to true to ensure the resulting ShareTarget picks
  // this up.
  nearby::sharing::proto::PublicCertificate certificate_proto =
      GetNearbyShareTestPublicCertificate(
          DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  certificate_proto.set_for_self_share(true);

  std::optional<NearbyShareDecryptedPublicCertificate> certificate =
      NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
          certificate_proto, GetNearbyShareTestEncryptedMetadataKey());
  ASSERT_TRUE(certificate.has_value());
  ASSERT_EQ(certificate_proto.for_self_share(), certificate->for_self_share());

  std::optional<ShareTarget> share_target = service_->CreateShareTarget(
      kEndpointId, *advertisement, certificate, /*is_incoming=*/true);

  ASSERT_TRUE(share_target.has_value());
  EXPECT_EQ(kDeviceName, share_target->device_name);
  EXPECT_EQ(kDeviceType, share_target->type);
  EXPECT_EQ(certificate_proto.for_self_share(), share_target->for_self_share);

  // Test when |certificate| is null.
  share_target = service_->CreateShareTarget(kEndpointId, *advertisement,
                                             /*certificate=*/std::nullopt,
                                             /*is_incoming=*/true);
  ASSERT_TRUE(share_target.has_value());
  EXPECT_EQ(kDeviceName, share_target->device_name);
  EXPECT_EQ(kDeviceType, share_target->type);
  EXPECT_EQ(kVendorId, share_target->vendor_id);
  EXPECT_FALSE(share_target->for_self_share);
}

TEST_F(NearbySharingServiceImplTest, SelfShareAutoAccept) {
  // We create an incoming connection corresponding to a certificate where the
  // |for_self_share| field is set to 'true'. This value will be propagated to
  // the ShareTarget, which will be used as a signal for the service to
  // automatically accept the transfer when Self Share is enabled. This is
  // similar to other tests (see "AcceptValidShareTarget") but without the
  // explicit call to service_->Accept().
  NiceMock<MockTransferUpdateCallback> callback;
  int64_t share_target_id =
      SetUpIncomingConnection(callback, /*is_foreground=*/false,
                              /*for_self_share=*/true);

  // Should fail to call accept.
  service_->Accept(
      share_target_id, [&](NearbySharingServiceImpl::StatusCodes status_code) {
        EXPECT_EQ(status_code,
                  NearbySharingServiceImpl::StatusCodes::kOutOfOrderApiCall);
      });

  // Check data written to connection_.
  ExpectPairedKeyEncryptionFrame();
  ExpectPairedKeyResultFrame();
  ExpectConnectionResponseFrame(ConnectionResponseFrame::ACCEPT);

  EXPECT_FALSE(connection_->IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
  FlushTesting();
  Shutdown();
  service_.reset();
}

TEST_F(NearbySharingServiceImplTest, SelfShareNoAutoAcceptInForeground) {
  NiceMock<MockTransferUpdateCallback> callback;
  int64_t share_target_id =
      SetUpIncomingConnection(callback, /*is_foreground=*/true,
                              /*for_self_share=*/true);

  EXPECT_CALL(callback, OnTransferUpdate(testing::_, testing::_, testing::_))
      .Times(0);

  service_->Accept(
      share_target_id, [&](NearbySharingServiceImpl::StatusCodes status_code) {
        EXPECT_EQ(status_code,
                  NearbySharingServiceImpl::StatusCodes::kOutOfOrderApiCall);
      });

  // Check data written to connection_.
  ExpectPairedKeyEncryptionFrame();
  ExpectPairedKeyResultFrame();
  ExpectConnectionResponseFrame(ConnectionResponseFrame::ACCEPT);

  EXPECT_FALSE(connection_->IsClosed());

  // To avoid UAF in OnIncomingTransferUpdate().
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, ObserveAccountLoginAndLogout) {
  MockAccountObserver account_observer;
  absl::Notification notification;
  service_->GetAccountManager()->AddObserver(&account_observer);

  EXPECT_CALL(account_observer, OnLoginSucceeded(kTestAccountId)).Times(1);
  AccountManager::Account account;
  account.id = kTestAccountId;
  account_manager().SetAccount(account);
  std::unique_ptr<SigninAttempt> signin_attempt =
      service_->GetAccountManager()->Login("test_client_id",
                                           "test_client_secret");
  account_manager().NotifyLogin(kTestAccountId);
  EXPECT_CALL(account_observer, OnLogoutSucceeded(kTestAccountId, false))
      .Times(1);
  absl::Notification logout_notification;
  service_->GetAccountManager()->Logout([&](absl::Status status) {
    EXPECT_TRUE(status.ok());
    logout_notification.Notify();
  });
  EXPECT_TRUE(logout_notification.WaitForNotificationWithTimeout(kWaitTimeout));
  service_->GetAccountManager()->RemoveObserver(&account_observer);
  EXPECT_TRUE(sharing_service_task_runner_->SyncWithTimeout(kTaskWaitTimeout));
}

TEST_F(NearbySharingServiceImplTest, LoginAndLogoutShouldResetSettings) {
  SetConnectionType(ConnectionType::kWifi);

  // Used to check whether the setting is cleared after login.
  service_->GetSettings()->SetIsAnalyticsEnabled(true);

  // Create account.
  AccountManager::Account account;
  account.id = kTestAccountId;

  // Login user.
  account_manager().SetAccount(account);
  std::unique_ptr<SigninAttempt> signin_attempt =
      service_->GetAccountManager()->Login("test_client_id",
                                           "test_client_secret");
  EXPECT_TRUE(sharing_service_task_runner_->SyncWithTimeout(kTaskWaitTimeout));
  EXPECT_TRUE(service_->GetSettings()->GetIsAnalyticsEnabled());
  ASSERT_TRUE(service_->GetAccountManager()->GetCurrentAccount().has_value());
  EXPECT_EQ(service_->GetAccountManager()->GetCurrentAccount()->id,
            kTestAccountId);

  // Logout user.
  absl::Notification logout_notification;
  service_->GetAccountManager()->Logout([&](absl::Status status) {
    EXPECT_TRUE(status.ok());
    logout_notification.Notify();
  });
  EXPECT_TRUE(logout_notification.WaitForNotificationWithTimeout(kWaitTimeout));
  absl::SleepFor(absl::Milliseconds(100));
  // data collection flag is not reset on logout.
  EXPECT_TRUE(service_->GetSettings()->GetIsAnalyticsEnabled());
  EXPECT_FALSE(service_->GetAccountManager()->GetCurrentAccount().has_value());
  EXPECT_TRUE(sharing_service_task_runner_->SyncWithTimeout(kTaskWaitTimeout));
}

TEST_F(NearbySharingServiceImplTest, LoginShouldSetContactsVisibility) {
  SetConnectionType(ConnectionType::kWifi);

  // Create account.
  AccountManager::Account account;
  account.id = kTestAccountId;

  // Login user.
  account_manager().SetAccount(account);
  std::unique_ptr<SigninAttempt> signin_attempt =
      service_->GetAccountManager()->Login("test_client_id",
                                           "test_client_secret");
  account_manager().NotifyLogin(kTestAccountId);
  ASSERT_TRUE(sharing_service_task_runner_->SyncWithTimeout(kTaskWaitTimeout));

  EXPECT_EQ(service_->GetSettings()->GetVisibility(),
            DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
}

TEST_F(NearbySharingServiceImplTest, LogoutShouldSetValidVisibility) {
  SetConnectionType(ConnectionType::kWifi);

  // Create account.
  AccountManager::Account account;
  account.id = kTestAccountId;

  // Login user.
  account_manager().SetAccount(account);
  std::unique_ptr<SigninAttempt> signin_attempt =
      service_->GetAccountManager()->Login("test_client_id",
                                           "test_client_secret");
  EXPECT_TRUE(sharing_service_task_runner_->SyncWithTimeout(kTaskWaitTimeout));

  // Set visibility.
  service_->GetSettings()->SetVisibility(
      DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);

  // Logout user.
  absl::Notification logout_notification;
  service_->GetAccountManager()->Logout([&](absl::Status status) {
    EXPECT_TRUE(status.ok());
    logout_notification.Notify();
  });
  EXPECT_TRUE(logout_notification.WaitForNotificationWithTimeout(kWaitTimeout));
  EXPECT_TRUE(sharing_service_task_runner_->SyncWithTimeout(kTaskWaitTimeout));
  EXPECT_EQ(service_->GetSettings()->GetVisibility(),
            DeviceVisibility::DEVICE_VISIBILITY_HIDDEN);

  // Login user.
  account_manager().SetAccount(account);
  std::unique_ptr<SigninAttempt> signin_attempt2 =
      service_->GetAccountManager()->Login("test_client_id",
                                           "test_client_secret");
  EXPECT_TRUE(sharing_service_task_runner_->SyncWithTimeout(kTaskWaitTimeout));

  // Set visibility.
  service_->GetSettings()->SetVisibility(
      DeviceVisibility::DEVICE_VISIBILITY_EVERYONE);

  // Logout user.
  absl::Notification logout2_notification;
  service_->GetAccountManager()->Logout([&](absl::Status status) {
    EXPECT_TRUE(status.ok());
    logout2_notification.Notify();
  });
  EXPECT_TRUE(
      logout2_notification.WaitForNotificationWithTimeout(kWaitTimeout));
  EXPECT_TRUE(sharing_service_task_runner_->SyncWithTimeout(kTaskWaitTimeout));
  EXPECT_EQ(service_->GetSettings()->GetVisibility(),
            DeviceVisibility::DEVICE_VISIBILITY_EVERYONE);
  EXPECT_TRUE(sharing_service_task_runner_->SyncWithTimeout(kTaskWaitTimeout));
}

TEST_F(NearbySharingServiceImplTest, LoginAndLogoutNoStopRunningSurfaces) {
  SetConnectionType(ConnectionType::kWifi);
  MockTransferUpdateCallback transfer_callback;
  MockShareTargetDiscoveredCallback discovery_callback;

  EXPECT_EQ(RegisterSendSurface(&transfer_callback, &discovery_callback,
                                SendSurfaceState::kForeground),
            NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  // Set an account and use it to login.
  AccountManager::Account account;
  account.id = kTestAccountId;
  account_manager().SetAccount(account);
  std::unique_ptr<SigninAttempt> signin_attempt =
      service_->GetAccountManager()->Login("test_client_id",
                                           "test_client_secret");
  EXPECT_TRUE(fake_nearby_connections_manager_->IsDiscovering());

  // Logout user.
  absl::Notification logout_notification;
  service_->GetAccountManager()->Logout([&](absl::Status status) {
    EXPECT_TRUE(status.ok());
    logout_notification.Notify();
  });
  EXPECT_TRUE(logout_notification.WaitForNotificationWithTimeout(kWaitTimeout));
  UnregisterSendSurface(&transfer_callback);
  EXPECT_TRUE(sharing_service_task_runner_->SyncWithTimeout(kTaskWaitTimeout));
}

TEST_F(NearbySharingServiceImplTest,
       IsReceivingEnabledWithRegisterReceiveSurfaceForeground) {
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest,
       IsReceivingDisabledWithRegisterReceiveSurfaceForeground) {
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest,
       IsReceivingEnabledWithRegisterReceiveSurfaceBackground) {
  MockTransferUpdateCallback callback;
  service_->GetSettings()->SetVisibility(
      DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_TRUE(sharing_service_task_runner_->SyncWithTimeout(kTaskWaitTimeout));
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsAdvertising());
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest,
       IsReceivingDisabledWithRegisterReceiveSurfaceBackground) {
  MockTransferUpdateCallback callback;
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kBackground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, NoAdvertisingWhenHidden) {
  MockTransferUpdateCallback callback;
  SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_HIDDEN);
  NearbySharingService::StatusCodes result = RegisterReceiveSurface(
      &callback, NearbySharingService::ReceiveSurfaceState::kForeground);
  EXPECT_EQ(result, NearbySharingService::StatusCodes::kOk);
  EXPECT_FALSE(fake_nearby_connections_manager_->IsAdvertising());
  UnregisterReceiveSurface(&callback);
}

TEST_F(NearbySharingServiceImplTest, RemoveIncomingPayloads) {
  fake_nearby_connections_manager_->AddUnknownFilePathsToDeleteForTesting(
      "test1.txt");
  fake_nearby_connections_manager_->AddUnknownFilePathsToDeleteForTesting(
      "test2.txt");
  auto unknown_file_paths_to_delete =
      fake_nearby_connections_manager_->GetUnknownFilePathsToDeleteForTesting();
  EXPECT_EQ(unknown_file_paths_to_delete.size(), 2);
  EXPECT_THAT(unknown_file_paths_to_delete,
              UnorderedElementsAre("test1.txt", "test2.txt"));
  nearby::analytics::MockEventLogger mock_event_logger;
  analytics::AnalyticsRecorder analytics_recorder{/*vendor_id=*/0,
                                                  &mock_event_logger};
  ShareTarget share_target;
  share_target.is_incoming = true;
  IncomingShareSession session(
      fake_context_.fake_clock(), *sharing_service_task_runner_,
      fake_nearby_connections_manager_, analytics_recorder, "endpoint_id",
      share_target,
      [](const IncomingShareSession&, const TransferMetadata&) {});
  service_->RemoveIncomingPayloads(session);
  EXPECT_EQ(
      fake_nearby_connections_manager_->GetUnknownFilePathsToDeleteForTesting()
          .size(),
      0);

  // Test GetAndClearUnknownFilePathsToDelete
  fake_nearby_connections_manager_->AddUnknownFilePathsToDeleteForTesting(
      "test1.txt");
  fake_nearby_connections_manager_->AddUnknownFilePathsToDeleteForTesting(
      "test2.txt");
  unknown_file_paths_to_delete =
      fake_nearby_connections_manager_->GetAndClearUnknownFilePathsToDelete();
  EXPECT_EQ(unknown_file_paths_to_delete.size(), 2);
  EXPECT_EQ(
      fake_nearby_connections_manager_->GetUnknownFilePathsToDeleteForTesting()
          .size(),
      0);
}

TEST_F(NearbySharingServiceImplTest, NotifyLogoutSucceededWithCredentialError) {
  TestObserver observer(service_.get());
  ::nearby::AccountManager::Account account;
  account.id = kTestAccountId;
  account_manager().SetAccount(account);

  account_manager().NotifyCredentialError();
  FlushTesting();

  EXPECT_TRUE(observer.credential_error_called_);

  // Remove the observer before it goes out of scope.
  service_->RemoveObserver(&observer);
  FlushTesting();
}

}  // namespace NearbySharingServiceUnitTests
}  // namespace sharing
}  // namespace nearby
