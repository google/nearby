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

#include "sharing/attachment_compare.h"
#include "sharing/file_attachment.h"
#include "sharing/text_attachment.h"
#include "sharing/wifi_credentials_attachment.h"

namespace nearby::sharing {

bool operator==(const TextAttachment& lhs, const TextAttachment& rhs) {
  return lhs.id() == rhs.id() && lhs.family() == rhs.family() &&
         lhs.size() == rhs.size() && lhs.batch_id() == rhs.batch_id() &&
         lhs.source_type() == rhs.source_type() && lhs.type() == rhs.type() &&
         lhs.text_title() == rhs.text_title() &&
         lhs.text_body() == rhs.text_body() &&
         lhs.mime_type() == rhs.mime_type();
}

bool operator==(const FileAttachment& lhs, const FileAttachment& rhs) {
  return lhs.id() == rhs.id() && lhs.family() == rhs.family() &&
         lhs.size() == rhs.size() && lhs.batch_id() == rhs.batch_id() &&
         lhs.source_type() == rhs.source_type() &&
         lhs.file_name() == rhs.file_name() &&
         lhs.mime_type() == rhs.mime_type() && lhs.type() == rhs.type() &&
         lhs.file_path() == rhs.file_path() &&
         lhs.parent_folder() == rhs.parent_folder();
}

bool operator==(const WifiCredentialsAttachment& lhs,
                const WifiCredentialsAttachment& rhs) {
  return lhs.id() == rhs.id() && lhs.family() == rhs.family() &&
         lhs.size() == rhs.size() && lhs.batch_id() == rhs.batch_id() &&
         lhs.ssid() == rhs.ssid() &&
         lhs.security_type() == rhs.security_type() &&
         lhs.password() == rhs.password() && lhs.is_hidden() == rhs.is_hidden();
}

}  // namespace nearby::sharing
