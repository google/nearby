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

#ifndef LOCATION_NEARBY_CPP_SHARING_CLIENTS_WINDOWS_APP_TRAY_ICON_H_
#define LOCATION_NEARBY_CPP_SHARING_CLIENTS_WINDOWS_APP_TRAY_ICON_H_

// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on

#include <string>

namespace nearby::sharing::windows_app {

// Notification-area icon. Mouse events arrive at the owner window as
// kTrayCallbackMessage with the event in LOWORD(lParam).
class TrayIcon {
 public:
  static constexpr UINT kTrayCallbackMessage = WM_APP + 2;

  TrayIcon(HWND owner, HICON icon, const std::wstring& tooltip);
  ~TrayIcon();

  // Re-adds the icon after Explorer restarts (TaskbarCreated).
  void Restore();
  void ShowBalloon(const std::wstring& title, const std::wstring& text);

 private:
  NOTIFYICONDATAW data_ = {};
};

}  // namespace nearby::sharing::windows_app

#endif  // LOCATION_NEARBY_CPP_SHARING_CLIENTS_WINDOWS_APP_TRAY_ICON_H_
