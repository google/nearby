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

#include "location/nearby/cpp/sharing/clients/windows/app/main_window.h"

// clang-format off
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shobjidl.h>
// clang-format on

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "location/nearby/cpp/sharing/clients/windows/app/app_controller.h"
#include "location/nearby/cpp/sharing/clients/windows/app/tray_icon.h"
#include "sharing/transfer_metadata.h"

namespace nearby::sharing::windows_app {
namespace {

using Status = TransferMetadata::Status;

enum ControlId : int {
  kVisibilityCombo = 100,
  kSendButton,
  kStopButton,
  kDevicesList,
  kTransfersList,
  kRenameButton,
  kFolderButton,
  kStartupCheck,
};

enum MenuId : int {
  kMenuOpen = 200,
  kMenuHidden,
  kMenuEveryoneFor10Minutes,
  kMenuEveryone,
  kMenuSendFiles,
  kMenuQuit,
};

// WM_COPYDATA from a second instance: NUL-separated file paths to send.
constexpr ULONG_PTR kCopyDataSendFiles = 1;

std::wstring Widen(std::string_view text) {
  if (text.empty()) return L"";
  int size = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                 static_cast<int>(text.size()), nullptr, 0);
  std::wstring wide(size, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      wide.data(), size);
  return wide;
}

std::wstring StatusText(const TransferView& transfer) {
  switch (transfer.status) {
    case Status::kConnecting:
      return L"Connecting…";
    case Status::kAwaitingLocalConfirmation:
      return L"Waiting for you to accept";
    case Status::kAwaitingRemoteAcceptance:
      return transfer.token.has_value()
                 ? L"Waiting · PIN " + Widen(*transfer.token)
                 : L"Waiting for the other device";
    case Status::kInProgress:
      return std::to_wstring(static_cast<int>(transfer.progress)) + L"%";
    case Status::kComplete:
      return L"Done";
    case Status::kRejected:
      return L"Declined";
    case Status::kCancelled:
      return L"Cancelled";
    case Status::kTimedOut:
      return L"Timed out";
    case Status::kNotEnoughSpace:
      return L"Not enough space";
    case Status::kUnsupportedAttachmentType:
      return L"Unsupported";
    default:
      return transfer.is_final ? L"Failed" : L"Preparing…";
  }
}

void CopyToClipboard(HWND owner, const std::wstring& text) {
  if (!OpenClipboard(owner)) return;
  EmptyClipboard();
  size_t bytes = (text.size() + 1) * sizeof(wchar_t);
  if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
    memcpy(GlobalLock(memory), text.c_str(), bytes);
    GlobalUnlock(memory);
    if (!SetClipboardData(CF_UNICODETEXT, memory)) GlobalFree(memory);
  }
  CloseClipboard();
}

constexpr wchar_t kRunKey[] =
    L"Software\Microsoft\Windows\CurrentVersion\Run";
constexpr wchar_t kRunValue[] = L"QuickShareArm64";

bool StartsAtLogin() {
  return RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, RRF_RT_REG_SZ,
                      nullptr, nullptr, nullptr) == ERROR_SUCCESS;
}

void SetStartAtLogin(bool enabled) {
  if (!enabled) {
    RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKey, kRunValue);
    return;
  }
  wchar_t exe[MAX_PATH];
  GetModuleFileNameW(nullptr, exe, MAX_PATH);
  std::wstring command = L"\"" + std::wstring(exe) + L"\" --background";
  RegSetKeyValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, REG_SZ,
                  command.c_str(),
                  static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
}

std::string Narrow(const std::wstring& text) {
  if (text.empty()) return "";
  int size = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                 static_cast<int>(text.size()), nullptr, 0,
                                 nullptr, nullptr);
  std::string narrow(size, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      narrow.data(), size, nullptr, nullptr);
  return narrow;
}

std::wstring WindowText(HWND window) {
  std::wstring text(GetWindowTextLengthW(window), L'\0');
  GetWindowTextW(window, text.data(), static_cast<int>(text.size() + 1));
  return text;
}

bool IsWebLink(const std::wstring& text) {
  return text.rfind(L"https://", 0) == 0 || text.rfind(L"http://", 0) == 0;
}

void SetListViewText(HWND list, int row, int column, const std::wstring& text) {
  LVITEMW item = {};
  item.iSubItem = column;
  item.pszText = const_cast<wchar_t*>(text.c_str());
  SendMessageW(list, LVM_SETITEMTEXTW, row, reinterpret_cast<LPARAM>(&item));
}

}  // namespace

std::unique_ptr<MainWindow> MainWindow::Create(HINSTANCE instance) {
  WNDCLASSEXW window_class = {sizeof(window_class)};
  window_class.lpfnWndProc = WindowProc;
  window_class.hInstance = instance;
  window_class.lpszClassName = kWindowClass;
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
  if (window_class.hIcon == nullptr) {
    window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  }
  RegisterClassExW(&window_class);

  auto window = std::unique_ptr<MainWindow>(new MainWindow(instance));
  window->icon_ = window_class.hIcon;
  CreateWindowExW(0, kWindowClass, L"Quick Share", WS_OVERLAPPEDWINDOW,
                  CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                  nullptr, nullptr, instance, window.get());
  return window->hwnd_ != nullptr ? std::move(window) : nullptr;
}

MainWindow::~MainWindow() {
  if (hwnd_ != nullptr) {
    quitting_ = true;
    DestroyWindow(hwnd_);
  }
  if (font_ != nullptr) DeleteObject(font_);
}

void MainWindow::Show() {
  if (!IsWindowVisible(hwnd_)) {
    controller_->SetReceivingInForeground(true);
  }
  ShowWindow(hwnd_, IsIconic(hwnd_) ? SW_RESTORE : SW_SHOW);
  SetForegroundWindow(hwnd_);
  SendMessageW(visibility_combo_, CB_SETCURSEL,
               static_cast<int>(controller_->GetVisibilityMode()), 0);
}

void MainWindow::SendFiles(std::vector<std::wstring> files) {
  if (files.empty()) return;
  Show();
  controller_->StartSending(std::move(files));
  UpdateSendingUi();
}

void MainWindow::OnNearbyDevicesChanged(
    const std::vector<NearbyDevice>& devices) {
  devices_ = devices;
  SendMessageW(devices_list_, LB_RESETCONTENT, 0, 0);
  for (const NearbyDevice& device : devices_) {
    SendMessageW(devices_list_, LB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(Widen(device.name).c_str()));
  }
}

void MainWindow::OnTransferUpdated(const TransferView& transfer) {
  auto it = std::find_if(transfers_.begin(), transfers_.end(),
                         [&](const TransferView& existing) {
                           return existing.share_target_id ==
                                      transfer.share_target_id &&
                                  existing.incoming == transfer.incoming;
                         });
  int row;
  if (it == transfers_.end() || it->is_final) {
    // A new transfer, or a new one with a device whose last one finished.
    transfers_.insert(transfers_.begin(), transfer);
    LVITEMW item = {};
    item.mask = LVIF_TEXT;
    std::wstring device = Widen(transfer.device_name);
    item.pszText = device.data();
    SendMessageW(transfers_list_, LVM_INSERTITEMW, 0,
                 reinterpret_cast<LPARAM>(&item));
    row = 0;
  } else {
    row = static_cast<int>(it - transfers_.begin());
    *it = transfer;
  }
  SetListViewText(transfers_list_, row, 1,
                  transfer.incoming ? L"Receiving" : L"Sending");
  SetListViewText(transfers_list_, row, 2, Widen(transfer.summary));
  SetListViewText(transfers_list_, row, 3, StatusText(transfer));
  UpdateTaskbarProgress();

  std::pair<int64_t, bool> key{transfer.share_target_id, transfer.incoming};
  if (transfer.incoming &&
      transfer.status == Status::kAwaitingLocalConfirmation &&
      !confirming_.contains(key)) {
    confirming_.insert(key);
    ConfirmIncoming(transfer);
    return;
  }
  if (!transfer.is_final) return;
  confirming_.erase(key);

  bool visible = IsWindowVisible(hwnd_);
  if (transfer.incoming && transfer.status == Status::kComplete) {
    std::wstring text = L"From " + Widen(transfer.device_name);
    if (!transfer.texts.empty()) {
      CopyToClipboard(hwnd_, Widen(transfer.texts.front()));
      text += L" · copied to the clipboard";
    }
    tray_icon_->ShowBalloon(L"Received " + Widen(transfer.summary), text);
    for (const WifiNetwork& network : transfer.wifi_networks) {
      OfferToJoin(network, Widen(transfer.device_name));
    }
  } else if (!transfer.incoming) {
    controller_->StopSending();
    UpdateSendingUi();
    if (!visible) {
      tray_icon_->ShowBalloon(
          transfer.status == Status::kComplete ? L"Sent" : L"Not sent",
          Widen(transfer.summary) + L" · " + StatusText(transfer));
    }
  }
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT message, WPARAM wparam,
                                        LPARAM lparam) {
  if (message == WM_NCCREATE) {
    auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    auto* window = static_cast<MainWindow*>(create->lpCreateParams);
    window->hwnd_ = hwnd;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
  }
  auto* window =
      reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (window == nullptr) return DefWindowProcW(hwnd, message, wparam, lparam);
  return window->HandleMessage(message, wparam, lparam);
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
  if (taskbar_created_message_ != 0 && message == taskbar_created_message_) {
    if (tray_icon_ != nullptr) tray_icon_->Restore();
    return 0;
  }
  if (taskbar_button_created_message_ != 0 &&
      message == taskbar_button_created_message_) {
    if (taskbar_ == nullptr &&
        SUCCEEDED(CoCreateInstance(CLSID_TaskbarList, nullptr,
                                   CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&taskbar_)))) {
      taskbar_->HrInit();
    }
    return 0;
  }
  switch (message) {
    case WM_CREATE: {
      dpi_ = GetDpiForWindow(hwnd_);
      UpdateFont();
      CreateControls();
      SetWindowPos(hwnd_, nullptr, 0, 0, Scale(620), Scale(700),
                   SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
      controller_ = std::make_unique<AppController>(hwnd_, this);
      SetWindowTextW(name_edit_, Widen(controller_->GetDeviceName()).c_str());
      SetWindowTextW(folder_path_, controller_->GetSaveFolder().c_str());
      SendMessageW(startup_check_, BM_SETCHECK,
                   StartsAtLogin() ? BST_CHECKED : BST_UNCHECKED, 0);
      SendMessageW(visibility_combo_, CB_SETCURSEL,
                   static_cast<int>(controller_->GetVisibilityMode()), 0);
      UpdateSendingUi();
      tray_icon_ = std::make_unique<TrayIcon>(hwnd_, icon_, L"Quick Share");
      taskbar_created_message_ = RegisterWindowMessageW(L"TaskbarCreated");
      taskbar_button_created_message_ =
          RegisterWindowMessageW(L"TaskbarButtonCreated");
      DragAcceptFiles(hwnd_, TRUE);
      return 0;
    }
    case WM_SIZE:
      Layout();
      return 0;
    case WM_CTLCOLORSTATIC: {
      // Labels share the window background instead of the dialog gray.
      auto dc = reinterpret_cast<HDC>(wparam);
      SetBkColor(dc, GetSysColor(COLOR_WINDOW));
      SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
      return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    }
    case WM_GETMINMAXINFO: {
      auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
      info->ptMinTrackSize = {Scale(480), Scale(480)};
      return 0;
    }
    case WM_DPICHANGED: {
      dpi_ = HIWORD(wparam);
      UpdateFont();
      const RECT* suggested = reinterpret_cast<const RECT*>(lparam);
      SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                   suggested->right - suggested->left,
                   suggested->bottom - suggested->top,
                   SWP_NOZORDER | SWP_NOACTIVATE);
      return 0;
    }
    case WM_COMMAND:
      OnCommand(LOWORD(wparam), HIWORD(wparam));
      return 0;
    case WM_NOTIFY: {
      auto* header = reinterpret_cast<NMHDR*>(lparam);
      if (header->idFrom == kTransfersList && header->code == NM_DBLCLK) {
        int row = reinterpret_cast<NMITEMACTIVATE*>(lparam)->iItem;
        if (row >= 0 && row < static_cast<int>(transfers_.size())) {
          OpenTransferResult(row);
        }
      }
      return 0;
    }
    case WM_DROPFILES: {
      auto drop = reinterpret_cast<HDROP>(wparam);
      std::vector<std::wstring> files;
      UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
      for (UINT i = 0; i < count; ++i) {
        std::wstring path(DragQueryFileW(drop, i, nullptr, 0), L'\0');
        DragQueryFileW(drop, i, path.data(), static_cast<UINT>(path.size() + 1));
        files.push_back(std::move(path));
      }
      DragFinish(drop);
      SendFiles(std::move(files));
      return 0;
    }
    case WM_COPYDATA: {
      auto* data = reinterpret_cast<COPYDATASTRUCT*>(lparam);
      std::vector<std::wstring> files;
      if (data->dwData == kCopyDataSendFiles && data->cbData > 0) {
        std::wstring_view paths(static_cast<const wchar_t*>(data->lpData),
                                data->cbData / sizeof(wchar_t));
        size_t start = 0;
        while (start < paths.size()) {
          size_t end = paths.find(L'\0', start);
          if (end == std::wstring_view::npos) end = paths.size();
          if (end > start) files.emplace_back(paths.substr(start, end - start));
          start = end + 1;
        }
      }
      if (files.empty()) {
        Show();
      } else {
        SendFiles(std::move(files));
      }
      return TRUE;
    }
    case kRunOnUiThreadMessage: {
      std::unique_ptr<std::function<void()>> task(
          reinterpret_cast<std::function<void()>*>(lparam));
      (*task)();
      return 0;
    }
    case TrayIcon::kTrayCallbackMessage:
      switch (LOWORD(lparam)) {
        case NIN_SELECT:
        case NIN_KEYSELECT:
        case NIN_BALLOONUSERCLICK:
          Show();
          break;
        case WM_CONTEXTMENU:
          ShowTrayMenu();
          break;
      }
      return 0;
    case WM_CLOSE:
      if (!quitting_) {
        HideToTray();
        return 0;
      }
      break;
    case WM_DESTROY:
      DragAcceptFiles(hwnd_, FALSE);
      if (taskbar_ != nullptr) {
        taskbar_->Release();
        taskbar_ = nullptr;
      }
      tray_icon_.reset();
      controller_.reset();
      hwnd_ = nullptr;
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd_, message, wparam, lparam);
}

void MainWindow::CreateControls() {
  auto create = [&](const wchar_t* type, const wchar_t* text, DWORD style,
                    int id, DWORD ex_style = 0) {
    HWND control = CreateWindowExW(
        ex_style, type, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    return control;
  };
  name_label_ = create(WC_STATICW, L"Device name:", SS_LEFT, 0);
  name_edit_ = create(WC_EDITW, L"", ES_AUTOHSCROLL | WS_TABSTOP, 0,
                      WS_EX_CLIENTEDGE);
  rename_button_ = create(WC_BUTTONW, L"Rename", BS_PUSHBUTTON | WS_TABSTOP,
                          kRenameButton);
  folder_label_ = create(WC_STATICW, L"Save files to:", SS_LEFT, 0);
  folder_path_ = create(WC_STATICW, L"", SS_LEFT | SS_PATHELLIPSIS, 0);
  folder_button_ = create(WC_BUTTONW, L"Change…", BS_PUSHBUTTON | WS_TABSTOP,
                          kFolderButton);
  startup_check_ =
      create(WC_BUTTONW, L"Start Quick Share when I sign in",
             BS_AUTOCHECKBOX | WS_TABSTOP, kStartupCheck);
  visibility_label_ = create(WC_STATICW, L"Receive from:", SS_LEFT, 0);
  visibility_combo_ = create(WC_COMBOBOXW, L"",
                             CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
                             kVisibilityCombo);
  for (const wchar_t* option :
       {L"No one", L"Everyone for 10 minutes", L"Everyone"}) {
    SendMessageW(visibility_combo_, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(option));
  }
  send_button_ = create(WC_BUTTONW, L"Send files…",
                        BS_PUSHBUTTON | WS_TABSTOP, kSendButton);
  stop_button_ = create(WC_BUTTONW, L"Stop sending",
                        BS_PUSHBUTTON | WS_TABSTOP, kStopButton);
  devices_label_ = create(WC_STATICW, L"", SS_LEFT | SS_ENDELLIPSIS, 0);
  devices_list_ =
      create(WC_LISTBOXW, L"", LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP,
             kDevicesList, WS_EX_CLIENTEDGE);
  transfers_label_ = create(
      WC_STATICW, L"Transfers (double-click to open or cancel):", SS_LEFT, 0);
  transfers_list_ = create(
      WC_LISTVIEWW, L"",
      LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP,
      kTransfersList, WS_EX_CLIENTEDGE);
  SendMessageW(transfers_list_, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
               LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
  const std::pair<const wchar_t*, int> columns[] = {
      {L"Device", 150}, {L"Direction", 90}, {L"Items", 190}, {L"Status", 130}};
  for (int i = 0; i < 4; ++i) {
    LVCOLUMNW column = {};
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.pszText = const_cast<wchar_t*>(columns[i].first);
    column.cx = Scale(columns[i].second);
    SendMessageW(transfers_list_, LVM_INSERTCOLUMNW, i,
                 reinterpret_cast<LPARAM>(&column));
  }
}

void MainWindow::Layout() {
  if (transfers_list_ == nullptr) return;
  RECT client;
  GetClientRect(hwnd_, &client);
  const int margin = Scale(12);
  const int row = Scale(24);
  const int width = client.right - 2 * margin;
  const int label_width = Scale(104);
  const int field_x = margin + label_width;
  const int button_width = Scale(100);
  const int field_width = width - label_width - button_width - Scale(8);
  int y = margin;
  MoveWindow(name_label_, margin, y + Scale(3), label_width, row, TRUE);
  MoveWindow(name_edit_, field_x, y, field_width, row, TRUE);
  MoveWindow(rename_button_, field_x + field_width + Scale(8), y, button_width,
             row, TRUE);
  y += row + Scale(8);
  MoveWindow(visibility_label_, margin, y + Scale(3), label_width, row, TRUE);
  MoveWindow(visibility_combo_, field_x, y, Scale(240), Scale(200), TRUE);
  y += row + Scale(8);
  MoveWindow(folder_label_, margin, y + Scale(3), label_width, row, TRUE);
  MoveWindow(folder_path_, field_x, y + Scale(3), field_width, row, TRUE);
  MoveWindow(folder_button_, field_x + field_width + Scale(8), y, button_width,
             row, TRUE);
  y += row + Scale(8);
  MoveWindow(startup_check_, field_x, y, field_width, row, TRUE);
  y += row + Scale(14);
  MoveWindow(send_button_, margin, y, Scale(140), Scale(30), TRUE);
  MoveWindow(stop_button_, margin + Scale(150), y, Scale(140), Scale(30), TRUE);
  y += Scale(30) + Scale(14);
  MoveWindow(devices_label_, margin, y, width, row, TRUE);
  y += row;
  MoveWindow(devices_list_, margin, y, width, Scale(120), TRUE);
  y += Scale(120) + Scale(14);
  MoveWindow(transfers_label_, margin, y, width, row, TRUE);
  y += row;
  MoveWindow(transfers_list_, margin, y, width,
             std::max<int>(Scale(80), client.bottom - margin - y), TRUE);
}

void MainWindow::UpdateFont() {
  NONCLIENTMETRICSW metrics = {sizeof(metrics)};
  SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics,
                             0, dpi_);
  HFONT old_font = font_;
  font_ = CreateFontIndirectW(&metrics.lfMessageFont);
  for (HWND control :
       {name_label_, name_edit_, rename_button_, folder_label_, folder_path_,
        folder_button_, startup_check_, visibility_label_, visibility_combo_,
        send_button_, stop_button_, devices_label_, devices_list_,
        transfers_label_, transfers_list_}) {
    if (control != nullptr) {
      SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
  }
  if (old_font != nullptr) DeleteObject(old_font);
}

void MainWindow::HideToTray() {
  ShowWindow(hwnd_, SW_HIDE);
  controller_->SetReceivingInForeground(false);
}

void MainWindow::ShowTrayMenu() {
  HMENU menu = CreatePopupMenu();
  VisibilityMode mode = controller_->GetVisibilityMode();
  auto radio = [&](int id, const wchar_t* text, VisibilityMode item_mode) {
    AppendMenuW(menu, MF_STRING | (mode == item_mode ? MF_CHECKED : 0), id,
                text);
  };
  AppendMenuW(menu, MF_STRING, kMenuOpen, L"Open Quick Share");
  AppendMenuW(menu, MF_STRING, kMenuSendFiles, L"Send files…");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  radio(kMenuHidden, L"Receive from no one", VisibilityMode::kHidden);
  radio(kMenuEveryoneFor10Minutes, L"Receive from everyone for 10 minutes",
        VisibilityMode::kEveryoneFor10Minutes);
  radio(kMenuEveryone, L"Receive from everyone", VisibilityMode::kEveryone);
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kMenuQuit, L"Quit");
  SetMenuDefaultItem(menu, kMenuOpen, FALSE);
  POINT cursor;
  GetCursorPos(&cursor);
  SetForegroundWindow(hwnd_);
  TrackPopupMenu(menu, TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, hwnd_, nullptr);
  PostMessageW(hwnd_, WM_NULL, 0, 0);
  DestroyMenu(menu);
}

void MainWindow::PickFilesToSend() {
  IFileOpenDialog* dialog = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                              CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&dialog)))) {
    return;
  }
  DWORD options = 0;
  dialog->GetOptions(&options);
  dialog->SetOptions(options | FOS_ALLOWMULTISELECT | FOS_FORCEFILESYSTEM |
                     FOS_FILEMUSTEXIST);
  dialog->SetTitle(L"Choose files to send");
  std::vector<std::wstring> files;
  IShellItemArray* items = nullptr;
  if (SUCCEEDED(dialog->Show(hwnd_)) && SUCCEEDED(dialog->GetResults(&items))) {
    DWORD count = 0;
    items->GetCount(&count);
    for (DWORD i = 0; i < count; ++i) {
      IShellItem* item = nullptr;
      PWSTR path = nullptr;
      if (SUCCEEDED(items->GetItemAt(i, &item)) &&
          SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
        files.emplace_back(path);
        CoTaskMemFree(path);
      }
      if (item != nullptr) item->Release();
    }
    items->Release();
  }
  dialog->Release();
  SendFiles(std::move(files));
}

void MainWindow::OnCommand(int id, int notification) {
  switch (id) {
    case kVisibilityCombo:
      if (notification == CBN_SELCHANGE) {
        controller_->SetVisibilityMode(static_cast<VisibilityMode>(
            SendMessageW(visibility_combo_, CB_GETCURSEL, 0, 0)));
      }
      break;
    case kSendButton:
    case kMenuSendFiles:
      PickFilesToSend();
      break;
    case kStopButton:
      controller_->StopSending();
      UpdateSendingUi();
      break;
    case kRenameButton:
      RenameDevice();
      break;
    case kFolderButton:
      PickSaveFolder();
      break;
    case kStartupCheck:
      SetStartAtLogin(SendMessageW(startup_check_, BM_GETCHECK, 0, 0) ==
                      BST_CHECKED);
      break;
    case kDevicesList:
      if (notification == LBN_DBLCLK) {
        auto index = SendMessageW(devices_list_, LB_GETCURSEL, 0, 0);
        if (index >= 0 && index < static_cast<LRESULT>(devices_.size())) {
          controller_->SendTo(devices_[index].id);
        }
      }
      break;
    case kMenuOpen:
      Show();
      break;
    case kMenuHidden:
    case kMenuEveryoneFor10Minutes:
    case kMenuEveryone:
      controller_->SetVisibilityMode(
          static_cast<VisibilityMode>(id - kMenuHidden));
      SendMessageW(visibility_combo_, CB_SETCURSEL, id - kMenuHidden, 0);
      break;
    case kMenuQuit:
      quitting_ = true;
      DestroyWindow(hwnd_);
      break;
  }
}

void MainWindow::ConfirmIncoming(const TransferView& transfer) {
  Show();
  std::wstring device = Widen(transfer.device_name);
  std::wstring heading = device + L" wants to share " + Widen(transfer.summary);
  std::wstring content =
      transfer.token.has_value()
          ? L"Make sure this PIN matches the one on " + device + L": " +
                Widen(*transfer.token)
          : L"Accept only if you expect this share.";
  const TASKDIALOG_BUTTON buttons[] = {{IDOK, L"Accept"},
                                       {IDCANCEL, L"Decline"}};
  TASKDIALOGCONFIG config = {sizeof(config)};
  config.hwndParent = hwnd_;
  config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
  config.pszWindowTitle = L"Quick Share";
  config.pszMainIcon = TD_INFORMATION_ICON;
  config.pszMainInstruction = heading.c_str();
  config.pszContent = content.c_str();
  config.cButtons = 2;
  config.pButtons = buttons;
  config.nDefaultButton = IDOK;
  int button = IDCANCEL;
  if (FAILED(TaskDialogIndirect(&config, &button, nullptr, nullptr))) {
    button = IDCANCEL;
  }
  if (button == IDOK) {
    controller_->Accept(transfer.share_target_id);
  } else {
    controller_->Reject(transfer.share_target_id);
  }
}

void MainWindow::OpenTransferResult(int row) {
  const TransferView& transfer = transfers_[row];
  if (!transfer.is_final) {
    if (MessageBoxW(hwnd_, L"Cancel this transfer?", L"Quick Share",
                    MB_YESNO | MB_ICONQUESTION) == IDYES) {
      controller_->Cancel(transfer.share_target_id);
    }
    return;
  }
  if (!transfer.incoming || transfer.status != Status::kComplete) return;
  if (!transfer.file_paths.empty()) {
    std::wstring arguments = L"/select,\"" + transfer.file_paths.front() + L"\"";
    ShellExecuteW(hwnd_, L"open", L"explorer.exe", arguments.c_str(), nullptr,
                  SW_SHOWNORMAL);
  } else if (!transfer.texts.empty()) {
    std::wstring text = Widen(transfer.texts.front());
    if (IsWebLink(text)) {
      ShellExecuteW(hwnd_, L"open", text.c_str(), nullptr, nullptr,
                    SW_SHOWNORMAL);
    } else {
      CopyToClipboard(hwnd_, text);
    }
  }
}

void MainWindow::UpdateSendingUi() {
  bool sending = controller_->IsSending();
  SetWindowTextW(devices_label_,
                 sending ? L"Nearby devices (double-click one to send):"
                         : L"Choose files to send, or drop them on this "
                           L"window.");
  EnableWindow(stop_button_, sending);
  EnableWindow(devices_list_, sending);
}

void MainWindow::RenameDevice() {
  if (!controller_->SetDeviceName(Narrow(WindowText(name_edit_)))) {
    MessageBoxW(hwnd_, L"Enter a device name that is not empty or too long.",
                L"Quick Share", MB_OK | MB_ICONWARNING);
  }
  SetWindowTextW(name_edit_, Widen(controller_->GetDeviceName()).c_str());
}

void MainWindow::PickSaveFolder() {
  IFileOpenDialog* dialog = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                              CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&dialog)))) {
    return;
  }
  DWORD options = 0;
  dialog->GetOptions(&options);
  dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
  dialog->SetTitle(L"Save received files to");
  IShellItem* item = nullptr;
  PWSTR path = nullptr;
  if (SUCCEEDED(dialog->Show(hwnd_)) && SUCCEEDED(dialog->GetResult(&item)) &&
      SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
    controller_->SetSaveFolder(path);
    SetWindowTextW(folder_path_, path);
    CoTaskMemFree(path);
  }
  if (item != nullptr) item->Release();
  dialog->Release();
}

void MainWindow::UpdateTaskbarProgress() {
  if (taskbar_ == nullptr) return;
  float total = 0;
  int active = 0;
  for (const TransferView& transfer : transfers_) {
    if (transfer.status == Status::kInProgress) {
      total += transfer.progress;
      ++active;
    }
  }
  if (active == 0) {
    taskbar_->SetProgressState(hwnd_, TBPF_NOPROGRESS);
    return;
  }
  taskbar_->SetProgressState(hwnd_, TBPF_NORMAL);
  taskbar_->SetProgressValue(hwnd_, static_cast<ULONGLONG>(total),
                             100ULL * active);
}

void MainWindow::OfferToJoin(const WifiNetwork& network,
                             const std::wstring& device) {
  std::wstring ssid = Widen(network.ssid);
  std::wstring heading = L"Join the Wi-Fi network " + ssid + L"?";
  std::wstring content = device + L" shared this network with you.";
  const TASKDIALOG_BUTTON buttons[] = {{IDOK, L"Join"}, {IDCANCEL, L"Not now"}};
  TASKDIALOGCONFIG config = {sizeof(config)};
  config.hwndParent = hwnd_;
  config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
  config.pszWindowTitle = L"Quick Share";
  config.pszMainIcon = TD_INFORMATION_ICON;
  config.pszMainInstruction = heading.c_str();
  config.pszContent = content.c_str();
  config.cButtons = 2;
  config.pButtons = buttons;
  int button = IDCANCEL;
  if (FAILED(TaskDialogIndirect(&config, &button, nullptr, nullptr)) ||
      button != IDOK) {
    return;
  }
  std::wstring error = JoinWifiNetwork(network);
  if (error.empty()) {
    tray_icon_->ShowBalloon(L"Joining " + ssid, L"The network was saved.");
  } else {
    MessageBoxW(hwnd_, error.c_str(), L"Quick Share", MB_OK | MB_ICONWARNING);
  }
}

}  // namespace nearby::sharing::windows_app
