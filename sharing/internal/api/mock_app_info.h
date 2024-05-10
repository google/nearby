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

#include "sharing/internal/api/app_info.h"

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
};

}  // namespace nearby::sharing::api

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_APP_INFO_H_
