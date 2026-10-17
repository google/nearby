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
#ifndef LOCATION_NEARBY_CPP_SHARING_CLIENTS_WINDOWS_APP_LOG_FILE_H_
#define LOCATION_NEARBY_CPP_SHARING_CLIENTS_WINDOWS_APP_LOG_FILE_H_

namespace nearby::sharing::windows_app {

// Sends all logs to %LOCALAPPDATA%\QuickShareArm64\Logs\quick_share.log; the
// GUI process has no console. The previous run's log is kept as .old.log.
void StartFileLogging();

}  // namespace nearby::sharing::windows_app

#endif  // LOCATION_NEARBY_CPP_SHARING_CLIENTS_WINDOWS_APP_LOG_FILE_H_
