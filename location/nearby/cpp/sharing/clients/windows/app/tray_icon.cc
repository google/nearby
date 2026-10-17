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

#include "location/nearby/cpp/sharing/clients/windows/app/tray_icon.h"

#include <windows.h>
#include <shellapi.h>

#include <string>

namespace nearby::sharing::windows_app {

TrayIcon::TrayIcon(HWND owner, HICON icon, const std::wstring& tooltip) {
  data_.cbSize = sizeof(data_);
  data_.hWnd = owner;
  data_.uID = 1;
  data_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
  data_.uCallbackMessage = kTrayCallbackMessage;
  data_.hIcon = icon;
  wcsncpy_s(data_.szTip, tooltip.c_str(), _TRUNCATE);
  data_.uVersion = NOTIFYICON_VERSION_4;
  Restore();
}

TrayIcon::~TrayIcon() { Shell_NotifyIconW(NIM_DELETE, &data_); }

void TrayIcon::Restore() {
  Shell_NotifyIconW(NIM_ADD, &data_);
  Shell_NotifyIconW(NIM_SETVERSION, &data_);
}

void TrayIcon::ShowBalloon(const std::wstring& title, const std::wstring& text) {
  NOTIFYICONDATAW balloon = data_;
  balloon.uFlags = NIF_INFO;
  balloon.dwInfoFlags = NIIF_INFO | NIIF_RESPECT_QUIET_TIME;
  wcsncpy_s(balloon.szInfoTitle, title.c_str(), _TRUNCATE);
  wcsncpy_s(balloon.szInfo, text.c_str(), _TRUNCATE);
  Shell_NotifyIconW(NIM_MODIFY, &balloon);
}

}  // namespace nearby::sharing::windows_app
