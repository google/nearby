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

#include "sharing/nearby_file_handler.h"

#include <stdint.h>
#include <filesystem>  // NOLINT(build/c++17)
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "internal/base/files.h"
#include "internal/platform/task_runner_impl.h"
#include "sharing/common/compatible_u8_string.h"
#include "sharing/internal/api/sharing_platform.h"
#include "sharing/internal/public/logging.h"

namespace nearby {
namespace sharing {
namespace {

using ::nearby::sharing::api::SharingPlatform;

// Called on the FileTaskRunner to actually open the files passed.
std::vector<NearbyFileHandler::FileInfo> DoOpenFiles(
    absl::Span<const std::filesystem::path> file_paths) {
  std::vector<NearbyFileHandler::FileInfo> files;
  for (const auto& file_path : file_paths) {
    std::optional<uintmax_t> size = GetFileSize(file_path);
    if (!size.has_value()) {
      NL_LOG(ERROR) << __func__ << ": Failed to open file. File="
                    << GetCompatibleU8String(file_path.u8string());
      return {};
    }
    files.push_back({*size, file_path});
  }
  return files;
}

}  // namespace

NearbyFileHandler::NearbyFileHandler(SharingPlatform& platform)
    : platform_(platform) {
  sequenced_task_runner_ = std::make_unique<TaskRunnerImpl>(1);
}

NearbyFileHandler::~NearbyFileHandler() = default;

void NearbyFileHandler::OpenFiles(std::vector<std::filesystem::path> file_paths,
                                  OpenFilesCallback callback) {
  sequenced_task_runner_->PostTask(
      [callback = std::move(callback), file_paths = std::move(file_paths)]() {
        auto opened_files = DoOpenFiles(file_paths);
        callback(opened_files);
      });
}

void NearbyFileHandler::DeleteFilesFromDisk(
    std::vector<std::filesystem::path> file_paths,
    DeleteFilesFromDiskCallback callback) {
  sequenced_task_runner_->PostTask([callback = std::move(callback),
                                    file_paths = std::move(file_paths)]() {
    // wait 1 second to make the file being released from another process.
    absl::SleepFor(absl::Seconds(1));
    for (const auto& file_path : file_paths) {
      if (!std::filesystem::exists(file_path)) {
        continue;
      }
      if (RemoveFile(file_path)) {
        NL_VLOG(1) << __func__ << ": Removed partial file. File="
                   << GetCompatibleU8String(file_path.u8string());
      } else {
        // Try once more after 3 seconds.
        absl::SleepFor(absl::Seconds(3));
        if (RemoveFile(file_path)) {
          NL_VLOG(1) << __func__
                     << ": Removed partial file after additional delay. File="
                     << GetCompatibleU8String(file_path.u8string());
        } else {
          NL_LOG(ERROR) << __func__ << "Can't remove file: "
                        << GetCompatibleU8String(file_path.u8string());
        }
      }
    }
    callback();
  });
}

void NearbyFileHandler::UpdateFilesOriginMetadata(
    std::vector<std::filesystem::path> file_paths,
    absl::AnyInvocable<void(bool success)> callback) {
  sequenced_task_runner_->PostTask(
      [this, callback = std::move(callback),
       file_paths = std::move(file_paths)]() mutable {
        std::move(callback)(platform_.UpdateFileOriginMetadata(file_paths));
      });
}

}  // namespace sharing
}  // namespace nearby
