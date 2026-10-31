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

#include <functional>
#include <memory>
#include <vector>

#include "internal/base/file_path.h"
#include "internal/platform/task_runner.h"

namespace nearby {
namespace sharing {

// This class manages async File IO for Nearby Share file payloads. Opening and
// releasing files need to run on a MayBlock task runner.
class NearbyFileHandler {
 public:
  using DeleteFilesFromDiskCallback = std::function<void()>;

  // Pass in a TaskRunner to use for testing.
  explicit NearbyFileHandler(std::unique_ptr<TaskRunner> runner = nullptr);
  ~NearbyFileHandler();

  void DeleteFilesFromDisk(std::vector<FilePath> file_paths,
                           DeleteFilesFromDiskCallback callback);

 private:
  std::unique_ptr<TaskRunner> sequenced_task_runner_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_FILE_HANDLER_H_
