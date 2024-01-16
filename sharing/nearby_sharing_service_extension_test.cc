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

#include "sharing/nearby_sharing_service_extension.h"

#include <filesystem>  // NOLINT(build/c++17)
#include <memory>
#include <optional>

#include "gtest/gtest.h"
#include "internal/test/fake_device_info.h"
#include "sharing/file_attachment.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/internal/test/fake_preference_manager.h"
#include "sharing/internal/test/fake_shell.h"
#include "sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"
#include "sharing/nearby_sharing_service.h"
#include "sharing/nearby_sharing_settings.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_target.h"
#include "sharing/text_attachment.h"
#include "sharing/wifi_credentials_attachment.h"

namespace nearby {
namespace sharing {
namespace {

using StatusCodes = NearbySharingService::StatusCodes;
using ::nearby::sharing::service::proto::FileMetadata;
using ::nearby::sharing::service::proto::TextMetadata;

class NearbySharingServiceExtensionTest : public ::testing::Test {
 public:
  NearbySharingServiceExtensionTest() = default;

  void SetUp() override {
    service_extension_ = std::make_unique<NearbySharingServiceExtension>(
        &context_, &nearby_share_settings_);
  }

  NearbySharingServiceExtension* service_extension() {
    return service_extension_.get();
  }

  FakeContext* context() { return &context_; }

 private:
  std::unique_ptr<NearbySharingServiceExtension> service_extension_;
  nearby::FakeDeviceInfo device_info_;
  nearby::FakePreferenceManager preference_manager_;
  FakeContext context_;
  FakeNearbyShareLocalDeviceDataManager local_device_data_manager_{"test"};
  NearbyShareSettings nearby_share_settings_{&context_, context_.GetClock(),
                                             device_info_, preference_manager_,
                                             &local_device_data_manager_};
};

TEST_F(NearbySharingServiceExtensionTest,
       OpenSharedTargetNoFileAndTextAttachment) {
  ShareTarget share_target;
  StatusCodes status_codes = service_extension()->Open(share_target);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
}

TEST_F(NearbySharingServiceExtensionTest,
       OpenSharedTargetBothFileAndTextAttachments) {
  ShareTarget share_target;
  share_target.text_attachments = {
      TextAttachment(TextMetadata::TEXT, "body", "title", "mime")};
  share_target.file_attachments = {
      FileAttachment(std::filesystem::temp_directory_path() / "test.g1")};
  StatusCodes status_codes = service_extension()->Open(share_target);
  EXPECT_EQ(status_codes, StatusCodes::kError);
}

TEST_F(NearbySharingServiceExtensionTest, OpenSharedTargetOneTextAttachment) {
  ShareTarget share_target;
  share_target.text_attachments = {
      TextAttachment(TextMetadata::TEXT, "body", "title", "mime")};
  StatusCodes status_codes = service_extension()->Open(share_target);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
}

TEST_F(NearbySharingServiceExtensionTest,
       OpenSharedMoreThanOneTextAttachments) {
  ShareTarget share_target;
  share_target.text_attachments = {
      TextAttachment(TextMetadata::TEXT, "body", "title1", "mime"),
      TextAttachment(TextMetadata::TEXT, "body", "title2", "mime")};
  StatusCodes status_codes = service_extension()->Open(share_target);
  EXPECT_EQ(status_codes, StatusCodes::kError);
}

TEST_F(NearbySharingServiceExtensionTest, OpenShareTargetWithUrlAttacchment) {
  ShareTarget share_target;
  share_target.text_attachments = {TextAttachment(
      TextMetadata::URL, "http://www.google.com", std::nullopt, std::nullopt)};
  StatusCodes status_codes = service_extension()->Open(share_target);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
}

TEST_F(NearbySharingServiceExtensionTest,
       OpenShareTargetWithTextAddressAttacchment) {
  ShareTarget share_target;
  share_target.text_attachments = {TextAttachment(TextMetadata::ADDRESS, "body",
                                                  std::nullopt, std::nullopt)};
  StatusCodes status_codes = service_extension()->Open(share_target);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
}

TEST_F(NearbySharingServiceExtensionTest, OpenShareTargetWithWifiAttacchment) {
  ShareTarget share_target;
  share_target.wifi_credentials_attachments = {WifiCredentialsAttachment(
      "ssid", service::proto::WifiCredentialsMetadata::WPA_PSK)};
  StatusCodes status_codes = service_extension()->Open(share_target);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
}

TEST_F(NearbySharingServiceExtensionTest,
       OpenShareTargetWithMultipleWifiAttacchments) {
  ShareTarget share_target;
  share_target.wifi_credentials_attachments = {
      WifiCredentialsAttachment(
          "ssid1", service::proto::WifiCredentialsMetadata::WPA_PSK),
      WifiCredentialsAttachment(
          "ssid2", service::proto::WifiCredentialsMetadata::WPA_PSK)};
  StatusCodes status_codes = service_extension()->Open(share_target);
  EXPECT_EQ(status_codes, StatusCodes::kError);
}

TEST_F(NearbySharingServiceExtensionTest,
       OpenShareTargetWithOneFileAttacchment) {
  ShareTarget share_target;
  share_target.file_attachments = {FileAttachment(
      /*id=*/1234, /*size=*/1000, /*file_name=*/"test.png",
      /*mime_type=*/"image", /*type=*/FileMetadata::IMAGE)};
  StatusCodes status_codes = service_extension()->Open(share_target);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
}

TEST_F(NearbySharingServiceExtensionTest, OpenSharedTargetUseDownloadFolder) {
  ShareTarget share_target;
  share_target.file_attachments = {
      FileAttachment(std::filesystem::temp_directory_path() / "test.g1"),
      FileAttachment(std::filesystem::temp_directory_path() / "test.g2")};
  StatusCodes status_codes = service_extension()->Open(share_target);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
  auto& shell = dynamic_cast<FakeShell&>(context()->GetShell());
  shell.set_return_error(true);
  status_codes = service_extension()->Open(share_target);
  EXPECT_NE(status_codes, StatusCodes::kOk);
  shell.set_return_error(false);
  status_codes = service_extension()->Open(share_target);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
}

TEST_F(NearbySharingServiceExtensionTest,
       OpenSharedTargetUseDefaultApplication) {
  ShareTarget share_target;
  NearbySharingService::StatusCodes status_codes;
  share_target.file_attachments = {
      FileAttachment(std::filesystem::temp_directory_path() / "test.jpg")};
  status_codes = service_extension()->Open(share_target);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
  share_target.file_attachments = {
      FileAttachment(std::filesystem::temp_directory_path() / "test.wav")};
  status_codes = service_extension()->Open(share_target);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
  share_target.file_attachments = {
      FileAttachment(std::filesystem::temp_directory_path() / "test.wmv")};
  status_codes = service_extension()->Open(share_target);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
}

TEST_F(NearbySharingServiceExtensionTest, GetQrCodeUrl) {
  EXPECT_EQ(service_extension()->GetQrCodeUrl(),
            "http://near.by/launch_by_qrcode");
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
