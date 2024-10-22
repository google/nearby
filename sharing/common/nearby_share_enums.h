// Copyright 2021-2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_COMMON_NEARBY_SHARE_ENUMS_H_
#define THIRD_PARTY_NEARBY_SHARING_COMMON_NEARBY_SHARE_ENUMS_H_

namespace nearby {
namespace sharing {

// Represents the advertising bluetooth power for Nearby Connections.
enum class PowerLevel {
  kUnknown = 0,
  kLowPower = 1,
  kMediumPower = 2,
  kHighPower = 3,
  kMaxValue = kHighPower
};

enum class DeviceNameValidationResult {
  // The device name was valid.
  kValid = 0,
  // The device name must not be empty.
  kErrorEmpty = 1,
  // The device name is too long.
  kErrorTooLong = 2,
  // The device name is not valid UTF-8.
  kErrorNotValidUtf8 = 3
};

// Describes the type of device for a ShareTarget.
// The numeric values are used to encode/decode advertisement bytes, and must
// be kept in sync with Android implementation.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange()
enum class ShareTargetType {
  // Unknown device type.
  kUnknown = 0,
  // A phone.
  kPhone = 1,
  // A tablet.
  kTablet = 2,
  // A laptop.
  kLaptop = 3,
  // A car.
  kCar = 4,
  // A foldable.
  kFoldable = 5,
};
// LINT.ThenChange(
//  //depot/google3/location/nearby/cpp/sharing/clients/proto/share_target.proto:ShareTarget.ShareTargetType,
//  //depot/google3/location/nearby/sharing/sdk/quick_share/protos/share_target.proto:ShareTarget,
//  //depot/google3/third_party/nearby/sharing/advertisement.cc:IsKnownDeviceValue,
//  //depot/google3/java/com/google/android/gmscore/integ/client/nearby/src/com/google/android/gms/nearby/sharing/ShareTarget.java:Type,
//  //depot/google3/location/nearby/cpp/sharing/clients/dart/platform/lib/types/share_target.dart:ShareTargetType
//)

// This enum combines both text and file share attachment types into a single
// enum that more directly maps to what is shown to the user for preview.
enum class ShareType {
  // A generic non-file text share.
  kText,
  // A text share representing a url, opened in browser.
  kUrl,
  // A text share representing a phone number, opened in dialer.
  kPhone,
  // A text share representing an address, opened in browser.
  kAddress,
  // Multiple files are being shared, we don't capture the specific types.
  kMultipleFiles,
  // Single file attachment with a mime type of 'image/*'.
  kImageFile,
  // Single file attachment with a mime type of 'video/*'.
  kVideoFile,
  // Single file attachment with a mime type of 'audio/*'.
  kAudioFile,
  // Single file attachment with a mime type of 'application/pdf'.
  kPdfFile,
  // Single file attachment with a mime type of
  // 'application/vnd.google-apps.document'.
  kGoogleDocsFile,
  // Single file attachment with a mime type of
  // 'application/vnd.google-apps.spreadsheet'.
  kGoogleSheetsFile,
  // Single file attachment with a mime type of
  // 'application/vnd.google-apps.presentation'.
  kGoogleSlidesFile,
  // Single file attachment with mime type of
  // 'text/plain'
  kTextFile,
  // Single file attachment with un-matched mime type.
  kUnknownFile,
  // Single WiFi credentials attachment.
  kWifiCredentials,
};

enum class FileSenderType {
  kUnknown = 0,
  // The user sends files using context menu.
  kContextMenu = 1,
  // The user sends files using drag and drop.
  kDragAndDrop = 2,
  // The user sends files by clicking the "Select files" button.
  kSelectFilesButton = 3,
  // The user sends files by pasting (e.g. ctrl+v) into the Nearby Share app.
  kPaste = 4,
  // The user sends files by clicking the "Select folders" button.
  kSelectFoldersButton = 5,
  kMaxValue = kSelectFoldersButton
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_COMMON_NEARBY_SHARE_ENUMS_H_
