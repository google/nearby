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

#ifndef THIRD_PARTY_NEARBY_SHARING_ATTACHMENT_COMPARE_H_
#define THIRD_PARTY_NEARBY_SHARING_ATTACHMENT_COMPARE_H_

#include "sharing/file_attachment.h"
#include "sharing/text_attachment.h"
#include "sharing/wifi_credentials_attachment.h"

namespace nearby::sharing {

bool operator==(const TextAttachment& lhs, const TextAttachment& rhs);
bool operator==(const FileAttachment& lhs, const FileAttachment& rhs);
bool operator==(const WifiCredentialsAttachment& lhs,
                const WifiCredentialsAttachment& rhs);

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_ATTACHMENT_COMPARE_H_
