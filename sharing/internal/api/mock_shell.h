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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_SHELL_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_SHELL_H_

#include <filesystem>  // NOLINT
#include <functional>

#include "gmock/gmock.h"
#include "absl/status/status.h"
#include "sharing/internal/api/shell.h"

namespace nearby::sharing::api {

class MockShell : public nearby::api::Shell {
 public:
  MockShell() = default;
  MockShell(const MockShell&) = delete;
  MockShell& operator=(const MockShell&) = delete;
  ~MockShell() override = default;

  MOCK_METHOD(void, Open,
              (const std::filesystem::path& path,
               std::function<void(absl::Status)> callback),
              (override));
};

}  // namespace nearby::sharing::api

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_SHELL_H_
