// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_APP_INFO_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_APP_INFO_H_

#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "sharing/internal/api/app_info.h"
#include "sharing/internal/api/fake_app_info.h"

#include "gmock/gmock.h"

namespace nearby::sharing::api {

class MockAppInfo : public nearby::api::AppInfo {
 public:
  MockAppInfo() = default;
  MockAppInfo(const MockAppInfo&) = delete;
  MockAppInfo& operator=(const MockAppInfo&) = delete;
  ~MockAppInfo() override = default;

  MOCK_METHOD(std::optional<std::string>, GetAppVersion, (), (override));

  MOCK_METHOD(std::optional<std::string>, GetAppLanguage, (), (override));

  MOCK_METHOD(std::optional<std::string>, GetUpdateTrack, (), (override));

  MOCK_METHOD(std::optional<std::string>, GetAppInstallSource, (), (override));

  MOCK_METHOD(bool, GetFirstRunDone, (), (override));

  MOCK_METHOD(bool, SetFirstRunDone, (bool value), (override));

  MOCK_METHOD(bool, SetActiveFlag, (), (override));

  // Delegates the default actions of the methods to a FakeAppInfo object.
  // This must be called *before* the custom ON_CALL() statements.
  void FakeAppVersion(absl::string_view app_version) {
    fake_.SetAppVersion(app_version);
    ON_CALL(*this, GetAppVersion).WillByDefault([this]() {
      return fake_.GetAppVersion();
    });
  }

 private:
  FakeAppInfo fake_;
};

}  // namespace nearby::sharing::api

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_APP_INFO_H_
