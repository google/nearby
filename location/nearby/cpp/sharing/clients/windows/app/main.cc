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

// Quick Share for Windows on ARM64.
//
//   quick_share.exe                    Opens the window.
//   quick_share.exe --background       Starts in the notification area.
//   quick_share.exe --send FILE...     Sends files (used by the Explorer menu).
//
// Only one instance runs; later launches hand their files to it.

// clang-format off
#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>
// clang-format on

#include <memory>
#include <string>
#include <vector>

#include "location/nearby/cpp/sharing/clients/windows/app/log_file.h"
#include "location/nearby/cpp/sharing/clients/windows/app/main_window.h"

// Common Controls 6: themed controls and TaskDialog.
#pragma comment(linker, \
                "/manifestdependency:\"type='win32' "                 \
                "name='Microsoft.Windows.Common-Controls' "           \
                "version='6.0.0.0' processorArchitecture='*' "        \
                "publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

using ::nearby::sharing::windows_app::MainWindow;

constexpr wchar_t kInstanceMutex[] = L"Local\\QuickShareArm64";
constexpr ULONG_PTR kCopyDataSendFiles = 1;

struct Arguments {
  bool background = false;
  std::vector<std::wstring> files;
};

Arguments ParseArguments() {
  Arguments arguments;
  int count = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &count);
  bool sending = false;
  for (int i = 1; i < count; ++i) {
    std::wstring argument = argv[i];
    if (argument == L"--background") {
      arguments.background = true;
    } else if (argument == L"--send") {
      sending = true;
    } else if (sending) {
      arguments.files.push_back(std::move(argument));
    }
  }
  LocalFree(argv);
  return arguments;
}

// Hands the files to the running instance; returns false if there is none.
bool ForwardToRunningInstance(const Arguments& arguments) {
  HWND existing = FindWindowW(MainWindow::kWindowClass, nullptr);
  if (existing == nullptr) return false;
  std::wstring paths;
  for (const std::wstring& file : arguments.files) {
    paths += file;
    paths.push_back(L'\0');
  }
  COPYDATASTRUCT data = {};
  data.dwData = kCopyDataSendFiles;
  data.cbData = static_cast<DWORD>(paths.size() * sizeof(wchar_t));
  data.lpData = paths.data();
  DWORD process_id = 0;
  GetWindowThreadProcessId(existing, &process_id);
  AllowSetForegroundWindow(process_id);
  SendMessageW(existing, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data));
  return true;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  Arguments arguments = ParseArguments();

  HANDLE mutex = CreateMutexW(nullptr, FALSE, kInstanceMutex);
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    if (!arguments.background) ForwardToRunningInstance(arguments);
    return 0;
  }

  nearby::sharing::windows_app::StartFileLogging();

  // The UI thread is an STA for the shell dialogs; the MTA is kept alive for
  // the WinRT objects the sharing platform creates there.
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  CO_MTA_USAGE_COOKIE mta_cookie = nullptr;
  CoIncrementMTAUsage(&mta_cookie);
  INITCOMMONCONTROLSEX controls = {sizeof(controls),
                                   ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
  InitCommonControlsEx(&controls);

  std::unique_ptr<MainWindow> window = MainWindow::Create(instance);
  if (window == nullptr) return 1;
  if (!arguments.files.empty()) {
    window->SendFiles(arguments.files);
  } else if (!arguments.background) {
    window->Show();
  }

  MSG message;
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  window.reset();
  CoDecrementMTAUsage(mta_cookie);
  CoUninitialize();
  CloseHandle(mutex);
  return 0;
}
