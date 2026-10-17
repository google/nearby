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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_APP_INFO_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_APP_INFO_H_

#include <optional>
#include <string>

#include "sharing/internal/api/app_info.h"

namespace nearby::sharing::windows {

// App facts for a build that ships without an updater: there is no update
// track, install source or Omaha active ping.
class AppInfo : public nearby::api::AppInfo {
 public:
  std::optional<std::string> GetAppVersion() override;
  std::optional<std::string> GetAppLanguage() override;
  std::optional<std::string> GetUpdateTrack() override { return std::nullopt; }
  std::optional<std::string> GetAppInstallSource() override {
    return std::nullopt;
  }
  bool GetFirstRunDone() override;
  bool SetFirstRunDone(bool value) override;
  bool SetActiveFlag() override { return true; }
};

}  // namespace nearby::sharing::windows

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_APP_INFO_H_
