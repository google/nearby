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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_APP_INFO_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_APP_INFO_H_

#include <optional>
#include <string>

namespace nearby {
namespace api {

// AppInfo provides information about the app, such as version, update track and
// language.
class AppInfo {
 public:
  virtual ~AppInfo() = default;

  // The current app version, e.g. "1.0.4.2".
  virtual std::optional<std::string> GetAppVersion() = 0;

  // The language the user uses in the app. This should only include the
  // language code and not the region code, e.g. "en" NOT "en_US".
  virtual std::optional<std::string> GetAppLanguage() = 0;

  // The track to update the app.
  //
  // In NearbyShare Windows app, it's from the registry value set by the
  // installer, and could be "NearbyManualQA", "NearbyDeveloper",
  // "NearbyDogfood".
  //
  // It's a string instead of enum to provide flexibility so that any new value
  // can be recognized without adding the enum value first.
  virtual std::optional<std::string> GetUpdateTrack() = 0;

  // Indicates how the client desktop application was installed on the device,
  // e.g. manually installed by the user or OEM preinstall.
  virtual std::optional<std::string> GetAppInstallSource() = 0;

  // Whether the app install event has already been logged.
  virtual bool GetFirstRunDone() = 0;

  // Sets whether the app install event has been logged.
  virtual bool SetFirstRunDone(bool value) = 0;

  // Update flag to track active users.
  virtual bool SetActiveFlag() = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_APP_INFO_H_
