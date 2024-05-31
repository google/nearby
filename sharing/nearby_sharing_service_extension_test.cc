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
#include "sharing/attachment_container.h"
#include "sharing/file_attachment.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/internal/test/fake_preference_manager.h"
#include "sharing/internal/test/fake_shell.h"
#include "sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"
#include "sharing/nearby_sharing_service.h"
#include "sharing/nearby_sharing_settings.h"
#include "sharing/proto/wire_format.pb.h"
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

TEST_F(NearbySharingServiceExtensionTest, OpenEmptyAttachments) {
  StatusCodes status_codes = service_extension()->Open(AttachmentContainer());
  EXPECT_EQ(status_codes, StatusCodes::kInvalidArgument);
}

TEST_F(NearbySharingServiceExtensionTest, OpenBothFileAndTextAttachments) {
  AttachmentContainer container;
  container.AddTextAttachment(
      TextAttachment(TextMetadata::TEXT, "body", "title", "mime"));
  container.AddFileAttachment(
      FileAttachment(std::filesystem::temp_directory_path() / "test.g1"));
  StatusCodes status_codes = service_extension()->Open(container);
  EXPECT_EQ(status_codes, StatusCodes::kError);
}

TEST_F(NearbySharingServiceExtensionTest, OpenOneTextAttachment) {
  AttachmentContainer container;
  container.AddTextAttachment(
      TextAttachment(TextMetadata::TEXT, "body", "title", "mime"));
  StatusCodes status_codes = service_extension()->Open(container);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
}

TEST_F(NearbySharingServiceExtensionTest, OpenMoreThanOneTextAttachments) {
  AttachmentContainer container;
  container.AddTextAttachment(
      TextAttachment(TextMetadata::TEXT, "body", "title1", "mime"));
  container.AddTextAttachment(
      TextAttachment(TextMetadata::TEXT, "body", "title2", "mime"));
  StatusCodes status_codes = service_extension()->Open(container);
  EXPECT_EQ(status_codes, StatusCodes::kError);
}

TEST_F(NearbySharingServiceExtensionTest, OpenUrlAttacchment) {
  AttachmentContainer container;
  container.AddTextAttachment(TextAttachment(
      TextMetadata::URL, "http://www.google.com", std::nullopt, std::nullopt));
  StatusCodes status_codes = service_extension()->Open(container);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
}

TEST_F(NearbySharingServiceExtensionTest, OpenTextAddressAttacchment) {
  AttachmentContainer container;
  container.AddTextAttachment(TextAttachment(TextMetadata::ADDRESS, "body",
                                             std::nullopt, std::nullopt));
  StatusCodes status_codes = service_extension()->Open(container);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
}

TEST_F(NearbySharingServiceExtensionTest, OpenWifiAttacchment) {
  AttachmentContainer container;
  container.AddWifiCredentialsAttachment(WifiCredentialsAttachment(
      "ssid", service::proto::WifiCredentialsMetadata::WPA_PSK));
  StatusCodes status_codes = service_extension()->Open(container);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
}

TEST_F(NearbySharingServiceExtensionTest, OpenMultipleWifiAttacchments) {
  AttachmentContainer container;
  container.AddWifiCredentialsAttachment(WifiCredentialsAttachment(
      "ssid1", service::proto::WifiCredentialsMetadata::WPA_PSK));
  container.AddWifiCredentialsAttachment(WifiCredentialsAttachment(
      "ssid2", service::proto::WifiCredentialsMetadata::WPA_PSK));
  StatusCodes status_codes = service_extension()->Open(container);
  EXPECT_EQ(status_codes, StatusCodes::kError);
}

TEST_F(NearbySharingServiceExtensionTest, OpenOneFileAttacchment) {
  AttachmentContainer container;
  container.AddFileAttachment(FileAttachment(
      /*id=*/1234, /*size=*/1000, /*file_name=*/"test.png",
      /*mime_type=*/"image", /*type=*/FileMetadata::IMAGE));
  StatusCodes status_codes = service_extension()->Open(container);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
}

TEST_F(NearbySharingServiceExtensionTest, OpenUseDownloadFolder) {
  AttachmentContainer container;
  container.AddFileAttachment(
      FileAttachment(std::filesystem::temp_directory_path() / "test.g1"));
  container.AddFileAttachment(
      FileAttachment(std::filesystem::temp_directory_path() / "test.g2"));
  StatusCodes status_codes = service_extension()->Open(container);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
  FakeShell& shell = *context()->fake_shell();
  shell.set_return_error(true);
  status_codes = service_extension()->Open(container);
  EXPECT_NE(status_codes, StatusCodes::kOk);
  shell.set_return_error(false);
  status_codes = service_extension()->Open(container);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
}

TEST_F(NearbySharingServiceExtensionTest, OpenUseDefaultApplication) {
  AttachmentContainer container;
  NearbySharingService::StatusCodes status_codes;
  container.AddFileAttachment(
      FileAttachment(std::filesystem::temp_directory_path() / "test.jpg"));
  status_codes = service_extension()->Open(container);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
  container.AddFileAttachment(
      FileAttachment(std::filesystem::temp_directory_path() / "test.wav"));
  status_codes = service_extension()->Open(container);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
  container.AddFileAttachment(
      FileAttachment(std::filesystem::temp_directory_path() / "test.wmv"));
  status_codes = service_extension()->Open(container);
  EXPECT_EQ(status_codes, StatusCodes::kOk);
}

TEST_F(NearbySharingServiceExtensionTest, GetQrCodeUrl) {
  EXPECT_EQ(service_extension()->GetQrCodeUrl(), "https://near.by/qrcode");
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
