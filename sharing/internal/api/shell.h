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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_SHELL_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_SHELL_H_

#include <filesystem>  // NOLINT(build/c++17)
#include <functional>

#include "absl/status/status.h"

namespace nearby {
namespace api {

// Shell defines interfaces to interact with platform core features.
class Shell {
 public:
  virtual ~Shell() = default;

  // Opens the |path| file or folder with default application. if |path|
  // is a directory, will open the folder by explore, otherwise it will
  // try to open the file with application supports it. |callback| is called
  // when the open operation completed, and absl::StatusCodes::kOk returned
  // only when open the path successfully.
  virtual void Open(const std::filesystem::path& path,
                    std::function<void(absl::Status)> callback) = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_SHELL_H_
