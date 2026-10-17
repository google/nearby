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

#ifndef LOCATION_NEARBY_CPP_SHARING_CLIENTS_WINDOWS_APP_MAIN_WINDOW_H_
#define LOCATION_NEARBY_CPP_SHARING_CLIENTS_WINDOWS_APP_MAIN_WINDOW_H_

// clang-format off
#include <windows.h>
#include <shobjidl.h>
// clang-format on

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "location/nearby/cpp/sharing/clients/windows/app/app_controller.h"
#include "location/nearby/cpp/sharing/clients/windows/app/tray_icon.h"

namespace nearby::sharing::windows_app {

// The Quick Share window. Closing it hides it to the notification area, where
// the app keeps receiving in the background until Quit.
class MainWindow : public AppController::Listener {
 public:
  static constexpr wchar_t kWindowClass[] = L"QuickShareArm64Window";

  // Creates the (hidden) window and starts the sharing service.
  static std::unique_ptr<MainWindow> Create(HINSTANCE instance);
  ~MainWindow() override;

  void Show();
  // Starts a send session for files passed on the command line or from
  // another instance.
  void SendFiles(std::vector<std::wstring> files);

  // AppController::Listener:
  void OnNearbyDevicesChanged(const std::vector<NearbyDevice>& devices) override;
  void OnTransferUpdated(const TransferView& transfer) override;

 private:
  explicit MainWindow(HINSTANCE instance) : instance_(instance) {}

  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam,
                                     LPARAM lparam);
  LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
  void CreateControls();
  void Layout();
  void UpdateFont();
  int Scale(int value) const { return MulDiv(value, dpi_, 96); }

  void HideToTray();
  void ShowTrayMenu();
  void PickFilesToSend();
  void OnCommand(int id, int notification);
  void ConfirmIncoming(const TransferView& transfer);
  void OpenTransferResult(int row);
  void UpdateSendingUi();
  void RenameDevice();
  void PickSaveFolder();
  void UpdateTaskbarProgress();
  void OfferToJoin(const WifiNetwork& network, const std::wstring& device);

  HINSTANCE instance_;
  HWND hwnd_ = nullptr;
  UINT dpi_ = 96;
  HFONT font_ = nullptr;
  HICON icon_ = nullptr;
  UINT taskbar_created_message_ = 0;
  UINT taskbar_button_created_message_ = 0;
  ITaskbarList3* taskbar_ = nullptr;

  HWND name_label_ = nullptr;
  HWND name_edit_ = nullptr;
  HWND rename_button_ = nullptr;
  HWND folder_label_ = nullptr;
  HWND folder_path_ = nullptr;
  HWND folder_button_ = nullptr;
  HWND startup_check_ = nullptr;
  HWND visibility_label_ = nullptr;
  HWND visibility_combo_ = nullptr;
  HWND send_button_ = nullptr;
  HWND stop_button_ = nullptr;
  HWND devices_label_ = nullptr;
  HWND devices_list_ = nullptr;
  HWND transfers_label_ = nullptr;
  HWND transfers_list_ = nullptr;

  std::unique_ptr<AppController> controller_;
  std::unique_ptr<TrayIcon> tray_icon_;
  std::vector<NearbyDevice> devices_;
  // One row per (share target, direction), in transfers_list_ order.
  std::vector<TransferView> transfers_;
  std::set<std::pair<int64_t, bool>> confirming_;
  bool quitting_ = false;
};

}  // namespace nearby::sharing::windows_app

#endif  // LOCATION_NEARBY_CPP_SHARING_CLIENTS_WINDOWS_APP_MAIN_WINDOW_H_
