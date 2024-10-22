// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_TRANSFER_UPDATE_CALLBACK_H_
#define THIRD_PARTY_NEARBY_SHARING_TRANSFER_UPDATE_CALLBACK_H_

#include "sharing/attachment_container.h"
#include "sharing/share_target.h"
#include "sharing/transfer_metadata.h"

namespace nearby {
namespace sharing {

// Reports the transfer status for an ongoing transfer with a |share_target|.
class TransferUpdateCallback {
 public:
  virtual ~TransferUpdateCallback() = default;

  virtual void OnTransferUpdate(const ShareTarget& share_target,
                                const AttachmentContainer& attachment_container,
                                const TransferMetadata& transfer_metadata) = 0;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_TRANSFER_UPDATE_CALLBACK_H_
