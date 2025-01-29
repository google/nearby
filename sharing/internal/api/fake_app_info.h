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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_FAKE_APP_INFO_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_FAKE_APP_INFO_H_

#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "sharing/internal/api/app_info.h"

namespace nearby::sharing::api {

class FakeAppInfo : public nearby::api::AppInfo {
 public:
  // No op functions can be enabled for testing.
  std::optional<std::string> GetAppVersion() override { return app_version_; }

  void SetAppVersion(absl::string_view app_version) {
    app_version_ = app_version;
  }

  // No op functions can be enabled for testing.
  std::optional<std::string> GetAppLanguage() override {
    // no op
    return std::nullopt;
  }

  std::optional<std::string> GetUpdateTrack() override {
    // no op
    return std::nullopt;
  }

  std::optional<std::string> GetAppInstallSource() override {
    // no op
    return std::nullopt;
  }

  bool GetFirstRunDone() override {
    // no op
    return false;
  }

  bool SetFirstRunDone(bool value) override {
    // no op
    return false;
  }

  bool SetActiveFlag() override {
    // no op
    return false;
  }

 private:
  std::string app_version_ = {};
};

}  // namespace nearby::sharing::api

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_FAKE_APP_INFO_H_
