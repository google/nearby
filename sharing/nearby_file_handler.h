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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_FILE_HANDLER_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_FILE_HANDLER_H_

#include <stdint.h>

#include <filesystem>  // NOLINT(build/c++17)
#include <functional>
#include <memory>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "internal/platform/task_runner.h"
#include "sharing/internal/api/sharing_platform.h"

namespace nearby {
namespace sharing {

// This class manages async File IO for Nearby Share file payloads. Opening and
// releasing files need to run on a MayBlock task runner.
class NearbyFileHandler {
 public:
  struct FileInfo {
    uint64_t size;
    std::filesystem::path file_path;
  };

  using OpenFilesCallback = std::function<void(std::vector<FileInfo>)>;
  using DeleteFilesFromDiskCallback = std::function<void()>;

  explicit NearbyFileHandler(nearby::sharing::api::SharingPlatform& platform);
  ~NearbyFileHandler();

  // Open the files given in |file_paths| and return the opened files sizes via
  // |callback|. If any file fails to open, return an empty list.
  void OpenFiles(std::vector<std::filesystem::path> file_paths,
                 OpenFilesCallback callback);

  void DeleteFilesFromDisk(std::vector<std::filesystem::path> file_paths,
                           DeleteFilesFromDiskCallback callback);

  // On platforms where it is supported, tag the transferred files as
  // originating from an untrusted source.
  void UpdateFilesOriginMetadata(
      std::vector<std::filesystem::path> file_paths,
      absl::AnyInvocable<void(bool success)> callback);

 private:
  nearby::sharing::api::SharingPlatform& platform_;
  std::unique_ptr<TaskRunner> sequenced_task_runner_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_FILE_HANDLER_H_
