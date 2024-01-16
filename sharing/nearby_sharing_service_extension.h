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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_EXTENSION_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_EXTENSION_H_

#include <string>

#include "absl/strings/string_view.h"
#include "internal/network/url.h"
#include "sharing/internal/public/context.h"
#include "sharing/nearby_sharing_service.h"
#include "sharing/nearby_sharing_settings.h"
#include "sharing/share_target.h"

namespace nearby {
namespace sharing {

class NearbySharingServiceExtension {
 public:
  NearbySharingServiceExtension(Context* context, NearbyShareSettings* settings)
      : context_(context), settings_(settings) {}

  // Opens attachments from the remote |share_target|.
  NearbySharingService::StatusCodes Open(const ShareTarget& share_target);

  // Opens an url target on a browser instance.
  void OpenUrl(const ::nearby::network::Url& url);

  // Copies text to cache/clipboard.
  void CopyText(absl::string_view text);

  // Persists and joins the Wi-Fi network.
  void JoinWifiNetwork(absl::string_view ssid, absl::string_view password);

  // Returns the QR Code Url.
  std::string GetQrCodeUrl() const { return qr_code_url_; }

 private:
  Context* context_ = nullptr;
  NearbyShareSettings* settings_ = nullptr;

  // The qr code url for the current session containing Advertising token,
  // Connection token and Sender Public Key.
  std::string qr_code_url_ = "http://near.by/launch_by_qrcode";
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_EXTENSION_H_
