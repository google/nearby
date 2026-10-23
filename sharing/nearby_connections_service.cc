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

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "connections/listeners.h"
#include "connections/payload.h"
#include "connections/payload_type.h"
#include "connections/status.h"
#include "connections/strategy.h"
#include "internal/base/file_path.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/file.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connections_types.h"

namespace nearby {
namespace sharing {

using ::nearby::connections::PayloadType;
using ::nearby::connections::ResultCallback;

using NcPayload = ::nearby::connections::Payload;
using NcStatus = ::nearby::connections::Status;
using NcStrategy = ::nearby::connections::Strategy;

Status ConvertToStatus(NcStatus status) {
  return static_cast<Status>(status.value);
}

Payload ConvertToPayload(NcPayload payload) {
  switch (payload.GetType()) {
    case PayloadType::kBytes: {
      const ByteArray& bytes = payload.AsBytes();
      std::string data = std::string(bytes);
      return Payload(payload.GetId(),
                     std::vector<uint8_t>(data.begin(), data.end()));
    }
    case PayloadType::kFile: {
      std::string file_path = payload.AsFile()->GetFilePath();
      std::string parent_folder = payload.GetParentFolder();
      VLOG(1) << __func__ << ": Payload file_path=" << file_path
              << ", parent_folder = " << parent_folder;
      return Payload(payload.GetId(), FilePath(file_path), parent_folder);
    }
    default:
      return Payload();
  }
}

NcPayload ConvertToServicePayload(Payload payload) {
  switch (payload.content.type) {
    case PayloadContent::Type::kFile: {
      std::string file_path = payload.content.file_payload.file_path.ToString();
      std::string file_name =
          payload.content.file_payload.file_path.GetFileName().ToString();
      std::string parent_folder = payload.content.file_payload.parent_folder;
      std::replace(parent_folder.begin(), parent_folder.end(), '\\', '/');
      VLOG(1) << __func__ << ": NC Payload file_path=" << file_path
              << ", parent_folder = " << parent_folder;
      nearby::InputFile input_file(file_path);
      NcPayload nc_payload(payload.id, parent_folder, file_name,
                           std::move(input_file));
      return nc_payload;
    }
    case PayloadContent::Type::kBytes: {
      std::vector<uint8_t> bytes = payload.content.bytes_payload.bytes;
      return NcPayload(payload.id,
                       ByteArray(std::string(bytes.begin(), bytes.end())));
    }
    default:
      return NcPayload();
  }
}

ResultCallback BuildResultCallback(
    std::function<void(Status status)> callback) {
  return ResultCallback{[&, callback = std::move(callback)](NcStatus status) {
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
