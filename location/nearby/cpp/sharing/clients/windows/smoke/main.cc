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
// Console smoke test for the Windows ARM64 build: starts the Nearby Sharing
// service, becomes visible to everyone for 10 minutes, auto-accepts incoming
// shares and logs every transfer update. Press Enter to shut down.

#include <iostream>
#include <string>

#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "location/nearby/sharing/lib/analytics/analytics_recorder_impl.h"
#include "sharing/advertisement.h"
#include "sharing/attachment_container.h"
#include "sharing/nearby_sharing_service.h"
#include "sharing/nearby_sharing_service_factory.h"
#include "sharing/nearby_sharing_settings.h"
#include "sharing/internal/impl/windows/windows_platform.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/share_target.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_update_callback.h"

namespace {

using ::nearby::sharing::AttachmentContainer;
using ::nearby::sharing::NearbySharingService;
using ::nearby::sharing::ShareTarget;
using ::nearby::sharing::TransferMetadata;

class AutoAcceptReceiver : public nearby::sharing::TransferUpdateCallback {
 public:
  explicit AutoAcceptReceiver(NearbySharingService& service)
      : service_(service) {}

  void OnTransferUpdate(const ShareTarget& share_target,
                        const AttachmentContainer& attachment_container,
                        const TransferMetadata& transfer_metadata) override {
    std::cout << "[" << share_target.device_name << "] "
              << TransferMetadata::StatusToString(transfer_metadata.status())
              << " " << static_cast<int>(transfer_metadata.progress()) << "%"
              << std::endl;
    if (transfer_metadata.status() ==
        TransferMetadata::Status::kAwaitingLocalConfirmation) {
      service_.Accept(share_target.id, [](auto status) {
        std::cout << "Accept: " << NearbySharingService::StatusCodeToString(status)
                  << std::endl;
      });
    }
  }

 private:
  NearbySharingService& service_;
};

void PrintStatus(const char* what, NearbySharingService::StatusCodes status) {
  std::cout << what << ": " << NearbySharingService::StatusCodeToString(status)
            << std::endl;
}

}  // namespace

int main() {
  nearby::sharing::windows::WindowsPlatform platform;
  nearby::sharing::analytics::AnalyticsRecorderImpl analytics_recorder(
      /*vendor_id=*/0, /*event_logger=*/nullptr);
  NearbySharingService* service =
      nearby::sharing::NearbySharingServiceFactory::GetInstance()
          ->CreateSharingService(platform, &analytics_recorder,
                                 /*event_logger=*/nullptr,
                                 /*supports_file_sync=*/false);

  std::cout << "Device name: " << service->GetSettings()->GetDeviceName()
            << "\nBluetooth present=" << service->IsBluetoothPresent()
            << " powered=" << service->IsBluetoothPowered()
            << " LAN=" << service->IsLanConnected() << std::endl;

  service->SetVisibility(
      nearby::sharing::proto::DeviceVisibility::DEVICE_VISIBILITY_EVERYONE,
      absl::Minutes(10),
      [](auto status) { PrintStatus("SetVisibility", status); });

  AutoAcceptReceiver receiver(*service);
  service->RegisterReceiveSurface(
      &receiver, NearbySharingService::ReceiveSurfaceState::kForeground,
      nearby::sharing::Advertisement::BlockedVendorId::kNone,
      [](auto status) { PrintStatus("RegisterReceiveSurface", status); });

  std::cout << "Visible to everyone. Share to this PC from a phone; press "
               "Enter to quit."
            << std::endl;
  std::string line;
  std::getline(std::cin, line);

  absl::Notification shutdown;
  service->Shutdown([&shutdown](auto status) {
    PrintStatus("Shutdown", status);
    shutdown.Notify();
  });
  shutdown.WaitForNotificationWithTimeout(absl::Seconds(10));
  return 0;
}
