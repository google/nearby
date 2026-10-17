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
#include "location/nearby/cpp/sharing/clients/windows/app/log_file.h"

// clang-format off
#include <windows.h>
#include <shlobj.h>
// clang-format on

#include <string>

#include "absl/base/log_severity.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"
#include "absl/log/log_sink_registry.h"
#include "absl/synchronization/mutex.h"

namespace nearby::sharing::windows_app {
namespace {

class FileLogSink : public absl::LogSink {
 public:
  explicit FileLogSink(HANDLE file) : file_(file) {}

  void Send(const absl::LogEntry& entry) override {
    absl::string_view text = entry.text_message_with_prefix_and_newline();
    absl::MutexLock lock(mutex_);
    DWORD written = 0;
    WriteFile(file_, text.data(), static_cast<DWORD>(text.size()), &written,
              nullptr);
  }

 private:
  absl::Mutex mutex_;
  HANDLE file_;
};

std::wstring LogFolder() {
  PWSTR local_app_data = nullptr;
  std::wstring folder;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr,
                                     &local_app_data))) {
    folder = std::wstring(local_app_data) + L"\\QuickShareArm64";
    CoTaskMemFree(local_app_data);
    CreateDirectoryW(folder.c_str(), nullptr);
    folder += L"\\Logs";
    CreateDirectoryW(folder.c_str(), nullptr);
  }
  return folder;
}

}  // namespace

void StartFileLogging() {
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfinity);
  std::wstring folder = LogFolder();
  if (folder.empty()) return;
  std::wstring path = folder + L"\\quick_share.log";
  MoveFileExW(path.c_str(), (folder + L"\\quick_share.old.log").c_str(),
              MOVEFILE_REPLACE_EXISTING);
  HANDLE file =
      CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return;
  // Lives for the whole process, like the log registry itself.
  absl::AddLogSink(new FileLogSink(file));
}

}  // namespace nearby::sharing::windows_app
