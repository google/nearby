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

namespace nearby {
namespace sharing {

class NearbySharingServiceExtension {
 public:
  NearbySharingServiceExtension() = default;

  // Returns the QR Code Url.
  std::string GetQrCodeUrl() const { return qr_code_url_; }

 private:
  // The qr code url for the current session containing the Sender Public Key.
  std::string qr_code_url_ = "https://near.by/qrcode";
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_EXTENSION_H_
