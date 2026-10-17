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

#ifndef LOCATION_NEARBY_CPP_SHARING_CLIENTS_WINDOWS_APP_APP_CONTROLLER_H_
#define LOCATION_NEARBY_CPP_SHARING_CLIENTS_WINDOWS_APP_APP_CONTROLLER_H_

#include <windows.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "location/nearby/cpp/sharing/clients/windows/app/wifi_join.h"
#include "location/nearby/sharing/lib/analytics/analytics_recorder_impl.h"
#include "sharing/internal/impl/windows/windows_platform.h"
#include "sharing/nearby_sharing_service.h"
#include "sharing/share_target_discovered_callback.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_update_callback.h"

namespace nearby::sharing::windows_app {

// Window message carrying a heap-allocated std::function<void()> in LPARAM;
// the window procedure runs and deletes it on the UI thread.
inline constexpr UINT kRunOnUiThreadMessage = WM_APP + 1;

enum class VisibilityMode { kHidden, kEveryoneFor10Minutes, kEveryone };

struct NearbyDevice {
  int64_t id;
  std::string name;
};

// A snapshot of one transfer, copied off the service thread.
struct TransferView {
  int64_t share_target_id = 0;
  std::string device_name;
  bool incoming = false;
  TransferMetadata::Status status = TransferMetadata::Status::kUnknown;
  float progress = 0;
  bool is_final = false;
  std::optional<std::string> token;
  std::string summary;
  std::vector<std::wstring> file_paths;
  std::vector<std::string> texts;
  std::vector<WifiNetwork> wifi_networks;
};

// Owns the Nearby Sharing service and reports its events on the UI thread.
class AppController {
 public:
  class Listener {
   public:
    virtual ~Listener() = default;
    virtual void OnNearbyDevicesChanged(
        const std::vector<NearbyDevice>& devices) = 0;
    virtual void OnTransferUpdated(const TransferView& transfer) = 0;
  };

  // `ui_window` receives kRunOnUiThreadMessage; `listener` must outlive this.
  AppController(HWND ui_window, Listener* listener);
  ~AppController();

  std::string GetDeviceName() const;
  // Reports false with no change if the name is empty or too long.
  bool SetDeviceName(const std::string& name);
  std::wstring GetSaveFolder() const;
  void SetSaveFolder(const std::wstring& folder);
  VisibilityMode GetVisibilityMode() const;
  void SetVisibilityMode(VisibilityMode mode);
  // Foreground while the window is shown, background while in the tray.
  void SetReceivingInForeground(bool foreground);

  // Starts discovering nearby devices to send `files` to.
  void StartSending(std::vector<std::wstring> files);
  void StopSending();
  bool IsSending() const { return !pending_files_.empty(); }
  void SendTo(int64_t share_target_id);

  void Accept(int64_t share_target_id);
  void Reject(int64_t share_target_id);
  void Cancel(int64_t share_target_id);

 private:
  class TransferObserver : public TransferUpdateCallback {
   public:
    TransferObserver(AppController& controller, bool incoming)
        : controller_(controller), incoming_(incoming) {}
    void OnTransferUpdate(const ShareTarget& share_target,
                          const AttachmentContainer& attachment_container,
                          const TransferMetadata& transfer_metadata) override;

   private:
    AppController& controller_;
    bool incoming_;
  };

  class DiscoveryObserver : public ShareTargetDiscoveredCallback {
   public:
    explicit DiscoveryObserver(AppController& controller)
        : controller_(controller) {}
    void OnShareTargetDiscovered(const ShareTarget& share_target) override;
    void OnShareTargetLost(const ShareTarget& share_target) override;
    void OnShareTargetUpdated(const ShareTarget& share_target) override;

   private:
    AppController& controller_;
  };

  void PostToUi(std::function<void()> task);
  void UpdateDevice(const ShareTarget& share_target, bool lost);

  HWND ui_window_;
  Listener* listener_;
  std::unique_ptr<sharing::windows::WindowsPlatform> platform_;
  std::unique_ptr<analytics::AnalyticsRecorderImpl> analytics_recorder_;
  NearbySharingService* service_ = nullptr;
  TransferObserver receive_observer_{*this, /*incoming=*/true};
  TransferObserver send_observer_{*this, /*incoming=*/false};
  DiscoveryObserver discovery_observer_{*this};

  // UI-thread state.
  std::vector<std::wstring> pending_files_;
  std::vector<NearbyDevice> devices_;
};

}  // namespace nearby::sharing::windows_app

#endif  // LOCATION_NEARBY_CPP_SHARING_CLIENTS_WINDOWS_APP_APP_CONTROLLER_H_
