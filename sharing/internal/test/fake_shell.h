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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_SHELL_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_SHELL_H_

#include <filesystem>  // NOLINT(build/c++17)
#include <functional>
#include <ostream>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "sharing/internal/api/shell.h"
#include "sharing/internal/public/logging.h"

namespace nearby {

class FakeShell : public api::Shell {
 public:
  FakeShell() = default;

  // Open file by application. The mock method doesn't use the path
  // parameter. The callback result is controlled by return_error_.
  void Open(const std::filesystem::path& path,
            std::function<void(absl::Status)> callback) override {
    if (!std::filesystem::exists(path)) {
      NL_LOG(WARNING) << "the path " << path << " is not existed.";
    }

    if (return_error_) {
      std::move(callback)(absl::UnknownError(absl::StrCat("error code:", 12)));
      return;
    }

    std::move(callback)(absl::OkStatus());
  }

  // Mock methods.
  void set_return_error(bool return_error) { return_error_ = return_error; }

 private:
  bool return_error_ = false;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_SHELL_H_
