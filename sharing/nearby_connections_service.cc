// Copyright 2022 Google LLC
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

#include "sharing/nearby_connections_service.h"

#include <wrapper_internal_exception_macros.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>  // NOLINT(build/c++17)
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "sharing/internal/base/utf_string_conversions.h"
#include "sharing/nearby_connections_types.h"

namespace nearby {
namespace sharing {

Status ConvertToStatus(NcStatus status) {
  return static_cast<Status>(status.value);
}

Payload ConvertToPayload(NcPayload payload) {
  switch (payload.GetType()) {
    case NcPayloadType::kBytes: {
      NcByteArray bytes = payload.AsBytes();
      std::string data = std::string(bytes);
      return Payload(payload.GetId(),
                     std::vector<uint8_t>(data.begin(), data.end()));
    }
    case NcPayloadType::kFile: {
      std::filesystem::path file_path;
      std::string parent_folder;
      // Initialize path with UTF8 cause crash on Windows with configured
      // locale.
      try {
        file_path = payload.AsFile()->GetFilePath();
        if (!std::filesystem::exists(file_path)) {
          file_path = utils::Utf8ToWide(payload.AsFile()->GetFilePath());
          parent_folder = payload.GetParentFolder();
        }
      } catch (std::exception exception) {
        file_path = utils::Utf8ToWide(payload.AsFile()->GetFilePath());
        parent_folder = payload.GetParentFolder();
      } catch (...) {
        file_path = utils::Utf8ToWide(payload.AsFile()->GetFilePath());
        parent_folder = payload.GetParentFolder();
      }
      return Payload(payload.GetId(), InputFile(file_path), parent_folder);
      NEARBY_LOGS(VERBOSE) << __func__ << ": Payload file_path=" << file_path
                           << ", parent_folder = " << parent_folder;
    }
    default:
      return Payload();
  }
}

NcPayload ConvertToServicePayload(Payload payload) {
  switch (payload.content.type) {
    case PayloadContent::Type::kFile: {
      // On Windows, a crash may happen when access string() of path if it is
      // using wchar. Apply UTF8 to avoid the cross-platform issues.
      std::string file_path;
      std::string file_name;
      std::string parent_folder;
      int64_t file_size = payload.content.file_payload.size;
      try {
        file_path =
            utils::WideToUtf8(payload.content.file_payload.file.path.wstring());
        file_name = utils::WideToUtf8(
            payload.content.file_payload.file.path.filename().wstring());
      } catch (std::exception e) {
        file_path = payload.content.file_payload.file.path.string();
        file_name = payload.content.file_payload.file.path.filename().string();
      } catch (...) {
        file_path = payload.content.file_payload.file.path.string();
        file_name = payload.content.file_payload.file.path.filename().string();
      }
      parent_folder = payload.content.file_payload.parent_folder;
      std::replace(parent_folder.begin(), parent_folder.end(), '\\', '/');
      NEARBY_LOGS(VERBOSE) << __func__ << ": NC Payload file_path=" << file_path
                           << ", parent_folder = " << parent_folder;
      nearby::InputFile input_file(file_path, file_size);
      NcPayload nc_payload(payload.id, parent_folder, file_name,
                           std::move(input_file));
      return nc_payload;
    }
    case PayloadContent::Type::kBytes: {
      std::vector<uint8_t> bytes = payload.content.bytes_payload.bytes;
      return NcPayload(payload.id,
                       NcByteArray(std::string(bytes.begin(), bytes.end())));
    }
    default:
      return NcPayload();
  }
}

NcResultCallback BuildResultCallback(
    std::function<void(Status status)> callback) {
  return NcResultCallback{[&, callback = std::move(callback)](NcStatus status) {
    callback(ConvertToStatus(status));
  }};
}

NcStrategy ConvertToServiceStrategy(Strategy strategy) {
  switch (strategy) {
    case Strategy::kP2pCluster:
      return NcStrategy::kP2pCluster;
    case Strategy::kP2pPointToPoint:
      return NcStrategy::kP2pPointToPoint;
    case Strategy::kP2pStar:
      return NcStrategy::kP2pStar;
  }
}

}  // namespace sharing
}  // namespace nearby
