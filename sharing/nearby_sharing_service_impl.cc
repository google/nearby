// Copyright 2022-2023 Google LLC
//
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

#include "sharing/nearby_sharing_service_impl.h"

#include <stdint.h>

#include <cstdlib>
#include <ctime>
#include <filesystem>  // NOLINT(build/c++17)
#include <functional>
#include <ios>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/random/random.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "internal/analytics/event_logger.h"
#include "internal/base/bluetooth_address.h"
#include "internal/base/observer_list.h"
#include "internal/flags/nearby_flags.h"
#include "internal/network/url.h"
#include "internal/platform/device_info.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/platform/implementation/device_info.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/advertisement.h"
#include "sharing/analytics/analytics_information.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/attachment.h"
#include "sharing/attachment_info.h"
#include "sharing/certificates/common.h"
#include "sharing/certificates/nearby_share_certificate_manager.h"
#include "sharing/certificates/nearby_share_certificate_manager_impl.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "sharing/common/compatible_u8_string.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/common/nearby_share_prefs.h"
#include "sharing/constants.h"
#include "sharing/contacts/nearby_share_contact_manager.h"
#include "sharing/contacts/nearby_share_contact_manager_impl.h"
#include "sharing/fast_initiation/nearby_fast_initiation.h"
#include "sharing/fast_initiation/nearby_fast_initiation_impl.h"
#include "sharing/file_attachment.h"
#include "sharing/flags/generated/nearby_sharing_feature_flags.h"
#include "sharing/incoming_frames_reader.h"
#include "sharing/incoming_share_target_info.h"
#include "sharing/internal/api/bluetooth_adapter.h"
#include "sharing/internal/api/sharing_platform.h"
#include "sharing/internal/api/sharing_rpc_notifier.h"
#include "sharing/internal/api/wifi_adapter.h"
#include "sharing/internal/base/encode.h"
#include "sharing/internal/public/connectivity_manager.h"
#include "sharing/internal/public/context.h"
#include "sharing/internal/public/logging.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager_impl.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/nearby_file_handler.h"
#include "sharing/nearby_share_profile_info_provider_impl.h"
#include "sharing/nearby_sharing_decoder.h"
#include "sharing/nearby_sharing_service.h"
#include "sharing/nearby_sharing_service_extension.h"
#include "sharing/nearby_sharing_settings.h"
#include "sharing/nearby_sharing_util.h"
#include "sharing/outgoing_share_target_info.h"
#include "sharing/paired_key_verification_runner.h"
#include "sharing/payload_tracker.h"
#include "sharing/proto/encrypted_metadata.pb.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/scheduling/nearby_share_scheduler_utils.h"
#include "sharing/share_target.h"
#include "sharing/share_target_discovered_callback.h"
#include "sharing/share_target_info.h"
#include "sharing/text_attachment.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_metadata_builder.h"
#include "sharing/transfer_update_callback.h"
#include "sharing/wifi_credentials_attachment.h"

namespace nearby {
namespace sharing {
namespace {

using ::nearby::sharing::api::SharingPlatform;
using ::nearby::sharing::proto::DataUsage;
using ::nearby::sharing::proto::DeviceVisibility;
using ::nearby::sharing::proto::FastInitiationNotificationState;
using Type = ::nearby::sharing::service::proto::TextMetadata;
using ::location::nearby::proto::sharing::AttachmentTransmissionStatus;
using ::location::nearby::proto::sharing::EstablishConnectionStatus;
using ::location::nearby::proto::sharing::OSType;
using ::location::nearby::proto::sharing::ResponseToIntroduction;
using ::location::nearby::proto::sharing::SessionStatus;

constexpr absl::Duration kBackgroundAdvertisementRotationDelayMin =
    absl::Minutes(12);
// 870 seconds represents 14:30 minutes
constexpr absl::Duration kBackgroundAdvertisementRotationDelayMax =
    absl::Seconds(870);
constexpr absl::Duration kInvalidateSurfaceStateDelayAfterTransferDone =
    absl::Milliseconds(3000);
constexpr absl::Duration kProcessShutdownPendingTimerDelay =  // NOLINT
    absl::Seconds(15);
constexpr absl::Duration kProcessNetworkChangeTimerDelay = absl::Seconds(1);

// Cooldown period after a successful incoming share before we allow the "Device
// nearby is sharing" notification to appear again.
constexpr absl::Duration kFastInitiationScannerCooldown = absl::Seconds(8);

// The maximum number of certificate downloads that can be performed during a
// discovery session.
constexpr size_t kMaxCertificateDownloadsDuringDiscovery = 3u;
// The time between certificate downloads during a discovery session. The
// download is only attempted if there are discovered, contact-based
// advertisements that cannot decrypt any currently stored public certificates.
constexpr absl::Duration kCertificateDownloadDuringDiscoveryPeriod =
    absl::Seconds(10);

constexpr absl::string_view kConnectionListenerName = "nearby-share-service";
constexpr absl::string_view kScreenStateListenerName = "nearby-share-service";
constexpr absl::string_view kProfileRelativePath = "Google/Nearby/Sharing";

// Wraps a call to OnTransferUpdate() to filter any updates after receiving a
// final status.
class TransferUpdateDecorator : public TransferUpdateCallback {
 public:
  using Callback =
      std::function<void(const ShareTarget&, const TransferMetadata&)>;

  explicit TransferUpdateDecorator(Callback callback)
      : callback_(std::move(callback)) {}
  TransferUpdateDecorator(const TransferUpdateDecorator&) = delete;
  TransferUpdateDecorator& operator=(const TransferUpdateDecorator&) = delete;
  ~TransferUpdateDecorator() override = default;

  void OnTransferUpdate(const ShareTarget& share_target,
                        const TransferMetadata& transfer_metadata) override {
    if (got_final_status_) {
      // If we already got a final status, we can ignore any subsequent final
      // statuses caused by race conditions.
      NL_VLOG(1)
          << __func__ << ": Transfer update decorator swallowed "
          << "status update because a final status was already received: "
          << share_target.id << ": "
          << TransferMetadata::StatusToString(transfer_metadata.status());
      return;
    }
    got_final_status_ = transfer_metadata.is_final_status();
    callback_(share_target, transfer_metadata);
  }

 private:
  bool got_final_status_ = false;
  Callback callback_;
};

}  // namespace

NearbySharingServiceImpl::NearbySharingServiceImpl(
    Context* context, SharingPlatform& sharing_platform,
    NearbySharingDecoder* decoder,
    std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager,
    nearby::analytics::EventLogger* event_logger)
    : context_(context),
      device_info_(sharing_platform.GetDeviceInfo()),
      preference_manager_(sharing_platform.GetPreferenceManager()),
      account_manager_(sharing_platform.GetAccountManager()),
      decoder_(decoder),
      nearby_connections_manager_(std::move(nearby_connections_manager)),
      nearby_share_client_factory_(
          sharing_platform.CreateSharingRpcClientFactory(event_logger)),
      profile_info_provider_(
          std::make_unique<NearbyShareProfileInfoProviderImpl>(
              device_info_, account_manager_)),
      local_device_data_manager_(
          NearbyShareLocalDeviceDataManagerImpl::Factory::Create(
              context_, preference_manager_, account_manager_, device_info_,
              nearby_share_client_factory_.get(),
              profile_info_provider_.get())),
      contact_manager_(NearbyShareContactManagerImpl::Factory::Create(
          context_, preference_manager_, account_manager_,
          nearby_share_client_factory_.get(),
          local_device_data_manager_.get())),
      nearby_fast_initiation_(
          NearbyFastInitiationImpl::Factory::Create(context_)),
      analytics_recorder_(
          std::make_unique<analytics::AnalyticsRecorder>(event_logger)),
      settings_(std::make_unique<NearbyShareSettings>(
          context_, context_->GetClock(), device_info_, preference_manager_,
          local_device_data_manager_.get(), event_logger)),
      service_extension_(std::make_unique<NearbySharingServiceExtension>(
          context_, settings_.get())) {
  NL_DCHECK(decoder_);
  NL_DCHECK(nearby_connections_manager_);

  service_thread_ = context_->CreateSequencedTaskRunner();

  certificate_download_during_discovery_timer_ = context_->CreateTimer();
  on_network_changed_delay_timer_ = context_->CreateTimer();
  mutual_acceptance_timeout_alarm_ = context_->CreateTimer();
  rotate_background_advertisement_timer_ = context_->CreateTimer();
  fast_initiation_scanner_cooldown_timer_ = context_->CreateTimer();

  is_shutting_down_ = std::make_unique<bool>(false);
  std::filesystem::path path = device_info_.GetAppDataPath();

  std::filesystem::path full_database_path =
      path / std::string(kProfileRelativePath);
  certificate_manager_ = NearbyShareCertificateManagerImpl::Factory::Create(
      context_, sharing_platform, local_device_data_manager_.get(),
      contact_manager_.get(), full_database_path.string(),
      nearby_share_client_factory_.get()),

  certificate_manager_->AddObserver(this);
  context_->GetConnectivityManager()->RegisterConnectionListener(
      kConnectionListenerName,
      [this](nearby::ConnectivityManager::ConnectionType type,
             bool is_lan_connected) {
        OnNetworkChanged(type);
        OnLanConnectedChanged(is_lan_connected);
      });

  screen_unlock_time_ = context_->GetClock()->Now();
  is_screen_locked_ = device_info_.IsScreenLocked();
  device_info_.RegisterScreenLockedListener(
      kScreenStateListenerName,
      [&](nearby::api::DeviceInfo::ScreenStatus screen_status) {
        OnLockStateChanged(screen_status ==
                           nearby::api::DeviceInfo::ScreenStatus::kLocked);
      });

  account_manager_.AddObserver(this);
  settings_->AddSettingsObserver(this);
  nearby_fast_initiation_->AddObserver(this);
  // Setup saving path.
  std::string custom_save_path = settings_->GetCustomSavePath();
  NL_LOG(INFO) << __func__ << ": Set custom save path: " << custom_save_path;
  nearby_connections_manager_->SetCustomSavePath(custom_save_path);

  if (settings_->GetEnabled()) {
    local_device_data_manager_->Start();
    contact_manager_->Start();
    certificate_manager_->Start();
  }

  update_file_paths_in_progress_ = false;

  SetupBluetoothAdapter();
}

NearbySharingServiceImpl::~NearbySharingServiceImpl() = default;

void NearbySharingServiceImpl::Shutdown(
    std::function<void(StatusCodes)> status_codes_callback) {
  RunOnNearbySharingServiceThread(
      "api_shutdown",
      [&, status_codes_callback = std::move(status_codes_callback)]() {
        *is_shutting_down_ = true;
        for (auto* observer : observers_.GetObservers()) {
          observer->OnShutdown();
        }

        observers_.Clear();

        StopAdvertising();
        if (IsBackgroundScanningFeatureEnabled()) {
          StopFastInitiationScanning();
        }
        StopFastInitiationAdvertising();
        StopScanning();
        nearby_connections_manager_->Shutdown();

        Cleanup();

        certificate_manager_->RemoveObserver(this);
        account_manager_.RemoveObserver(this);
        context_->GetConnectivityManager()->UnregisterConnectionListener(
            kConnectionListenerName);
        context_->GetBluetoothAdapter().RemoveObserver(this);
        nearby_fast_initiation_->RemoveObserver(this);

        on_network_changed_delay_timer_->Stop();

        foreground_receive_callbacks_.Clear();
        background_receive_callbacks_.Clear();

        device_info_.UnregisterScreenLockedListener(
            kScreenStateListenerName);

        settings_->RemoveSettingsObserver(this);

        if (settings_->GetEnabled()) {
          local_device_data_manager_->Stop();
          contact_manager_->Stop();
          certificate_manager_->Stop();
        }

        is_shutting_down_ = nullptr;
        std::move(status_codes_callback)(StatusCodes::kOk);
      });
}

void NearbySharingServiceImpl::Cleanup() {
  SetInHighVisibility(false);

  endpoint_discovery_events_ = {};

  ClearOutgoingShareTargetInfoMap();
  incoming_share_target_info_map_.clear();
  discovered_advertisements_to_retry_map_.clear();
  discovered_advertisements_retried_set_.clear();

  foreground_send_transfer_callbacks_.Clear();
  background_send_transfer_callbacks_.Clear();
  foreground_send_discovery_callbacks_.Clear();
  background_send_discovery_callbacks_.Clear();

  last_incoming_metadata_.reset();
  last_outgoing_metadata_.reset();
  attachment_info_map_.clear();
  locally_cancelled_share_target_ids_.clear();

  mutual_acceptance_timeout_alarm_->Stop();
  disconnection_timeout_alarms_.clear();

  is_scanning_ = false;
  is_transferring_ = false;
  is_receiving_files_ = false;
  is_sending_files_ = false;
  is_connecting_ = false;
  advertising_power_level_ = PowerLevel::kUnknown;

  certificate_download_during_discovery_timer_->Stop();
  rotate_background_advertisement_timer_->Stop();
}

void NearbySharingServiceImpl::AddObserver(
    NearbySharingService::Observer* observer) {
  observers_.AddObserver(observer);
}

void NearbySharingServiceImpl::RemoveObserver(
    NearbySharingService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool NearbySharingServiceImpl::HasObserver(
    NearbySharingService::Observer* observer) {
  return observers_.HasObserver(observer);
}

void NearbySharingServiceImpl::RegisterSendSurface(
    TransferUpdateCallback* transfer_callback,
    ShareTargetDiscoveredCallback* discovery_callback, SendSurfaceState state,
    std::function<void(StatusCodes)> status_codes_callback) {
  RunOnNearbySharingServiceThread(
      "api_register_send_surface",
      [&, transfer_callback, discovery_callback, state,
       status_codes_callback = std::move(status_codes_callback)]() {
        NL_DCHECK(transfer_callback);
        NL_DCHECK(discovery_callback);
        NL_DCHECK_NE(static_cast<int>(state),
                     static_cast<int>(SendSurfaceState::kUnknown));
        NL_LOG(INFO) << __func__
                     << ": RegisterSendSurface is called with state: "
                     << (state == SendSurfaceState::kForeground ? "Foreground"
                                                                : "Background")
                     << ", transfer_callback: " << transfer_callback;

        if (foreground_send_transfer_callbacks_.HasObserver(
                transfer_callback) ||
            background_send_transfer_callbacks_.HasObserver(
                transfer_callback)) {
          NL_VLOG(1)
              << __func__
              << ": RegisterSendSurface failed. Already registered for a "
                 "different state.";
          std::move(status_codes_callback)(StatusCodes::kError);
          return;
        }

        if (state == SendSurfaceState::kForeground) {
          // Only check this error case for foreground senders
          if (!HasAvailableConnectionMediums()) {
            NL_VLOG(1) << __func__ << ": No available connection medium.";
            std::move(status_codes_callback)(
                StatusCodes::kNoAvailableConnectionMedium);
            return;
          }

          foreground_send_transfer_callbacks_.AddObserver(transfer_callback);
          foreground_send_discovery_callbacks_.AddObserver(discovery_callback);
        } else {
          background_send_transfer_callbacks_.AddObserver(transfer_callback);
          background_send_discovery_callbacks_.AddObserver(discovery_callback);
        }

        if (is_receiving_files_) {
          InternalUnregisterSendSurface(transfer_callback, discovery_callback);
          NL_VLOG(1)
              << __func__
              << ": Ignore registering (and unregistering if registered) send "
                 "surface because we're currently receiving files.";
          std::move(status_codes_callback)(
              StatusCodes::kTransferAlreadyInProgress);
          return;
        }

        // If the share sheet to be registered is a foreground surface, let it
        // catch up with most recent transfer metadata immediately.
        if (state == SendSurfaceState::kForeground &&
            last_outgoing_metadata_.has_value()) {
          // When a new share sheet is registered, we want to immediately show
          // the in-progress bar.
          discovery_callback->OnShareTargetDiscovered(
              last_outgoing_metadata_->first);
          transfer_callback->OnTransferUpdate(last_outgoing_metadata_->first,
                                              last_outgoing_metadata_->second);
        }

        // Sync down data from Nearby server when the sending flow starts,
        // making our best effort to have fresh contact and certificate data.
        // There is no need to wait for these calls to finish. The periodic
        // server requests will typically be sufficient, but we don't want the
        // user to be blocked for hours waiting for a periodic sync.
        if (state == SendSurfaceState::kForeground &&
            !last_outgoing_metadata_) {
          NL_VLOG(1) << __func__
                     << ": Downloading local device data, contacts, and "
                        "certificates from "
                     << "Nearby server at start of sending flow.";
          local_device_data_manager_->DownloadDeviceData();
          contact_manager_->DownloadContacts();
          certificate_manager_->DownloadPublicCertificates();
        }

        // Let newly registered send surface catch up with discovered share
        // targets from current scanning session.
        if (is_scanning_) {
          for (const auto& item : outgoing_share_target_map_) {
            discovery_callback->OnShareTargetDiscovered(item.second);
          }
        }

        // Set Share Start time for Foreground Send Surfaces
        if (state == SendSurfaceState::kForeground) {
          share_foreground_send_surface_start_timestamp_ =
              context_->GetClock()->Now();
        }

        NL_VLOG(1) << __func__
                   << ": A SendSurface has been registered for state: "
                   << SendSurfaceStateToString(state);

        NL_VLOG(1)
            << "RegisterSendSurface: foreground_send_transfer_callbacks_:"
            << foreground_send_transfer_callbacks_.size()
            << ", foreground_send_discovery_callbacks_:"
            << foreground_send_discovery_callbacks_.size()
            << ", background_send_transfer_callbacks_:"
            << background_send_transfer_callbacks_.size()
            << ", background_send_discovery_callbacks_"
            << background_send_discovery_callbacks_.size();

        InvalidateSendSurfaceState();
        std::move(status_codes_callback)(StatusCodes::kOk);
      });
}

void NearbySharingServiceImpl::UnregisterSendSurface(
    TransferUpdateCallback* transfer_callback,
    ShareTargetDiscoveredCallback* discovery_callback,
    std::function<void(StatusCodes)> status_codes_callback) {
  RunOnNearbySharingServiceThread(
      "api_unregister_send_surface",
      [&, transfer_callback, discovery_callback,
       status_codes_callback = std::move(status_codes_callback)]() {
        StatusCodes status_codes = InternalUnregisterSendSurface(
            transfer_callback, discovery_callback);

        NL_VLOG(1)
            << "UnregisterSendSurface: foreground_send_transfer_callbacks_:"
            << foreground_send_transfer_callbacks_.size()
            << ", foreground_send_discovery_callbacks_:"
            << foreground_send_discovery_callbacks_.size()
            << ", background_send_transfer_callbacks_:"
            << background_send_transfer_callbacks_.size()
            << ", background_send_discovery_callbacks_"
            << background_send_discovery_callbacks_.size();

        std::move(status_codes_callback)(status_codes);
      });
}

void NearbySharingServiceImpl::RegisterReceiveSurface(
    TransferUpdateCallback* transfer_callback, ReceiveSurfaceState state,
    std::function<void(StatusCodes)> status_codes_callback) {
  RunOnNearbySharingServiceThread(
      "api_register_receive_surface",
      [&, transfer_callback, state,
       status_codes_callback = std::move(status_codes_callback)]() {
        NL_DCHECK(transfer_callback);
        NL_DCHECK_NE(static_cast<int>(state),
                     static_cast<int>(ReceiveSurfaceState::kUnknown));

        NL_LOG(INFO) << __func__
                     << ": RegisterReceiveSurface is called with state: "
                     << (state == ReceiveSurfaceState::kForeground
                             ? "Foreground"
                             : "Background")
                     << ", transfer_callback: " << transfer_callback;

        // Check available mediums.
        if (!HasAvailableConnectionMediums()) {
          NL_VLOG(1) << __func__ << ": No available connection medium.";
          std::move(status_codes_callback)(
              StatusCodes::kNoAvailableConnectionMedium);
          return;
        }

        // We specifically allow re-registering without error, so it is clear to
        // caller that the transfer_callback is currently registered.
        if (GetReceiveCallbacksFromState(state).HasObserver(
                transfer_callback)) {
          NL_VLOG(1) << __func__
                     << ": transfer callback already registered, ignoring";
          std::move(status_codes_callback)(StatusCodes::kOk);
          return;
        } else if (foreground_receive_callbacks_.HasObserver(
                       transfer_callback) ||
                   background_receive_callbacks_.HasObserver(
                       transfer_callback)) {
          NL_LOG(ERROR) << __func__
                        << ":  transfer callback already registered but for a "
                           "different state.";
          std::move(status_codes_callback)(StatusCodes::kError);
          return;
        }

        // If the receive surface to be registered is a foreground surface, let
        // it catch up with most recent transfer metadata immediately.
        if (state == ReceiveSurfaceState::kForeground &&
            last_incoming_metadata_) {
          transfer_callback->OnTransferUpdate(last_incoming_metadata_->first,
                                              last_incoming_metadata_->second);
        }

        GetReceiveCallbacksFromState(state).AddObserver(transfer_callback);

        NL_VLOG(1) << __func__ << ": A ReceiveSurface("
                   << ReceiveSurfaceStateToString(state)
                   << ") has been registered";

        if (state == ReceiveSurfaceState::kForeground) {
          if (!IsBluetoothPresent()) {
            NL_LOG(ERROR) << __func__ << ": Bluetooth is not present.";
          } else if (!IsBluetoothPowered()) {
            NL_LOG(WARNING) << __func__ << ": Bluetooth is not powered.";
          } else {
            NL_VLOG(1) << __func__ << ": This device's MAC address is: "
                       << nearby::device::CanonicalizeBluetoothAddress(
                              *context_->GetBluetoothAdapter().GetAddress());
          }
        }

        NL_VLOG(1) << "RegisterReceiveSurface: foreground_receive_callbacks_:"
                   << foreground_receive_callbacks_.size()
                   << ", background_receive_callbacks_:"
                   << background_receive_callbacks_.size();

        InvalidateReceiveSurfaceState();
        std::move(status_codes_callback)(StatusCodes::kOk);
      });
}

void NearbySharingServiceImpl::UnregisterReceiveSurface(
    TransferUpdateCallback* transfer_callback,
    std::function<void(StatusCodes)> status_codes_callback) {
  RunOnNearbySharingServiceThread(
      "api_unregister_receive_surface",
      [&, transfer_callback,
       status_codes_callback = std::move(status_codes_callback)]() {
        StatusCodes status_codes =
            InternalUnregisterReceiveSurface(transfer_callback);
        NL_VLOG(1) << "UnregisterReceiveSurface: foreground_receive_callbacks_:"
                   << foreground_receive_callbacks_.size()
                   << ", background_receive_callbacks_:"
                   << background_receive_callbacks_.size();
        std::move(status_codes_callback)(status_codes);
        return;
      });
}

void NearbySharingServiceImpl::ClearForegroundReceiveSurfaces(
    std::function<void(StatusCodes)> status_codes_callback) {
  RunOnNearbySharingServiceThread(
      "api_clear_foreground_receive_surfaces",
      [&, status_codes_callback = std::move(status_codes_callback)]() {
        std::vector<TransferUpdateCallback*> fg_receivers;
        for (auto& callback : foreground_receive_callbacks_.GetObservers())
          fg_receivers.push_back(callback);

        StatusCodes status = StatusCodes::kOk;
        for (TransferUpdateCallback* callback : fg_receivers) {
          if (InternalUnregisterReceiveSurface(callback) != StatusCodes::kOk)
            status = StatusCodes::kError;
        }
        std::move(status_codes_callback)(status);
      });
}

bool NearbySharingServiceImpl::IsInHighVisibility() const {
  return in_high_visibility_;
}

bool NearbySharingServiceImpl::IsTransferring() const {
  return is_transferring_;
}

bool NearbySharingServiceImpl::IsReceivingFile() const {
  return is_receiving_files_;
}

bool NearbySharingServiceImpl::IsSendingFile() const {
  return is_sending_files_;
}

bool NearbySharingServiceImpl::IsScanning() const { return is_scanning_; }

bool NearbySharingServiceImpl::IsConnecting() const { return is_connecting_; }

std::string NearbySharingServiceImpl::GetQrCodeUrl() const {
  return service_extension_->GetQrCodeUrl();
}

void NearbySharingServiceImpl::SendAttachments(
    const ShareTarget& share_target,
    std::vector<std::unique_ptr<Attachment>> attachments,
    std::function<void(StatusCodes)> status_codes_callback) {
  ShareTarget share_target_copy = share_target;
  for (std::unique_ptr<Attachment>& attachment : attachments) {
    attachment->MoveToShareTarget(share_target_copy);
  }

  RunOnNearbySharingServiceThread(
      "api_send_attachments",
      [&, share_target, share_target_copy = std::move(share_target_copy),
       status_codes_callback = std::move(status_codes_callback)]() {
        if (!is_scanning_) {
          NL_LOG(WARNING) << __func__
                          << ": Failed to send attachments. Not scanning.";
          std::move(status_codes_callback)(StatusCodes::kError);
          return;
        }

        // |is_scanning_| means at least one send transfer callback.
        NL_DCHECK(!foreground_send_transfer_callbacks_.empty() ||
                  !background_send_transfer_callbacks_.empty());
        // |is_scanning_| and |is_transferring_| are mutually exclusive.
        NL_DCHECK(!is_transferring_);

        ShareTargetInfo* info = GetShareTargetInfo(share_target);
        if (!info || !info->endpoint_id()) {
          NL_LOG(WARNING)
              << __func__
              << ": Failed to send attachments. Unknown ShareTarget.";
          std::move(status_codes_callback)(StatusCodes::kError);
          return;
        }

        // Set session ID.
        info->set_session_id(analytics_recorder_->GenerateNextId());

        if (!share_target_copy.has_attachments()) {
          NL_LOG(WARNING) << __func__ << ": No attachments to send.";
          std::move(status_codes_callback)(StatusCodes::kError);
          return;
        }

        // For sending advertisement from scanner, the request advertisement
        // should always be visible to everyone.
        std::optional<std::vector<uint8_t>> endpoint_info =
            CreateEndpointInfo(local_device_data_manager_->GetDeviceName());
        if (!endpoint_info) {
          NL_LOG(WARNING) << __func__
                          << ": Could not create local endpoint info.";
          std::move(status_codes_callback)(StatusCodes::kError);
          return;
        }

        info->set_transfer_update_callback(
            std::make_unique<TransferUpdateDecorator>(
                [&](const ShareTarget& share_target,
                    const TransferMetadata& transfer_metadata) {
                  OnOutgoingTransferUpdate(share_target, transfer_metadata);
                }));

        // Log analytics event of sending start.
        analytics_recorder_->NewSendStart(
            info->session_id(),
            /*transfer_position=*/GetConnectedShareTargetPos(share_target),
            /*concurrent_connections=*/GetConnectedShareTargetCount(),
            share_target);

        send_attachments_timestamp_ = context_->GetClock()->Now();
        OnTransferStarted(/*is_incoming=*/false);
        is_connecting_ = true;
        InvalidateSendSurfaceState();

        // Send process initialized successfully, from now on status updated
        // will be sent out via OnOutgoingTransferUpdate().
        info->transfer_update_callback()->OnTransferUpdate(
            share_target_copy,
            TransferMetadataBuilder()
                .set_status(TransferMetadata::Status::kConnecting)
                .build());

        CreatePayloads(std::move(share_target_copy),
                       [this, endpoint_info = std::move(*endpoint_info)](
                           ShareTarget share_target, bool success) {
                         // Log analytics event of describing attachments.
                         analytics_recorder_->NewDescribeAttachments(
                             share_target.GetAttachments());

                         OnCreatePayloads(std::move(endpoint_info),
                                          share_target, success);
                       });

        std::move(status_codes_callback)(StatusCodes::kOk);
      });
}

void NearbySharingServiceImpl::Accept(
    const ShareTarget& share_target,
    std::function<void(StatusCodes status_codes)> status_codes_callback) {
  RunOnNearbySharingServiceThread(
      "api_accept",
      [&, share_target,
       status_codes_callback = std::move(status_codes_callback)]() {
        // Log analytics event of responding to introduction.
        analytics_recorder_->NewRespondToIntroduction(
            ResponseToIntroduction::ACCEPT_INTRODUCTION, receiving_session_id_);

        ShareTargetInfo* info = GetShareTargetInfo(share_target);
        if (!info || !info->connection()) {
          NL_LOG(WARNING) << __func__
                          << ": Accept invoked for unknown share target";
          std::move(status_codes_callback)(StatusCodes::kOutOfOrderApiCall);
          return;
        }

        std::optional<std::pair<ShareTarget, TransferMetadata>> metadata =
            share_target.is_incoming ? last_incoming_metadata_
                                     : last_outgoing_metadata_;
        if (!ReadyToAccept(share_target,
                           metadata.has_value()
                               ? metadata->second.status()
                               : TransferMetadata::Status::kUnknown)) {
          NL_LOG(WARNING) << __func__ << ": out of order API call.";
          status_codes_callback(StatusCodes::kOutOfOrderApiCall);
          return;
        }

        is_waiting_to_record_accept_to_transfer_start_metric_ =
            share_target.is_incoming;
        if (share_target.is_incoming) {
          incoming_share_accepted_timestamp_ = context_->GetClock()->Now();
          ReceivePayloads(share_target, std::move(status_codes_callback));
          return;
        }

        std::move(status_codes_callback)(SendPayloads(share_target));
      });
}

void NearbySharingServiceImpl::Reject(
    const ShareTarget& share_target,
    std::function<void(StatusCodes status_codes)> status_codes_callback) {
  RunOnNearbySharingServiceThread(
      "api_reject",
      [&, share_target,
       status_codes_callback = std::move(status_codes_callback)]() {
        // Log analytics event of responding to introduction.
        analytics_recorder_->NewRespondToIntroduction(
            ResponseToIntroduction::REJECT_INTRODUCTION, receiving_session_id_);

        ShareTargetInfo* info = GetShareTargetInfo(share_target);
        if (!info || !info->connection()) {
          NL_LOG(WARNING) << __func__
                          << ": Reject invoked for unknown share target";
          std::move(status_codes_callback)(StatusCodes::kOutOfOrderApiCall);
          return;
        }

        NearbyConnection* connection = info->connection();

        RunOnNearbySharingServiceThreadDelayed(
            "incoming_rejection_delay", kIncomingRejectionDelay,
            [&, share_target]() { CloseConnection(share_target); });

        connection->SetDisconnectionListener([&, share_target]() {
          RunOnNearbySharingServiceThread(
              "disconnection_listener",
              [&, share_target]() { UnregisterShareTarget(share_target); });
        });

        WriteResponseFrame(
            *connection,
            nearby::sharing::service::proto::ConnectionResponseFrame::REJECT);
        NL_VLOG(1) << __func__
                   << ": Successfully wrote a rejection response frame";

        if (info->transfer_update_callback()) {
          info->transfer_update_callback()->OnTransferUpdate(
              share_target, TransferMetadataBuilder()
                                .set_status(TransferMetadata::Status::kRejected)
                                .build());
        }

        std::move(status_codes_callback)(StatusCodes::kOk);
      });
}

void NearbySharingServiceImpl::Cancel(
    const ShareTarget& share_target,
    std::function<void(StatusCodes status_codes)> status_codes_callback) {
  RunOnAnyThread("api_cancel", [&, share_target,
                                status_codes_callback =
                                    std::move(status_codes_callback)]() {
    NL_LOG(INFO) << __func__ << ": User canceled transfer";
    if (locally_cancelled_share_target_ids_.contains(share_target.id)) {
      NL_LOG(WARNING) << __func__ << ": Cancel is called again.";
      status_codes_callback(StatusCodes::kOutOfOrderApiCall);
      return;
    }
    locally_cancelled_share_target_ids_.insert(share_target.id);
    DoCancel(share_target, std::move(status_codes_callback),
             /*is_initiator_of_cancellation=*/true);
  });
}

// Note: |share_target| is intentionally passed by value. A share target
// reference could likely be invalidated by the owner during the multistep
// cancellation process.
void NearbySharingServiceImpl::DoCancel(
    ShareTarget share_target,
    std::function<void(StatusCodes status_codes)> status_codes_callback,
    bool is_initiator_of_cancellation) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->endpoint_id()) {
    NL_LOG(ERROR) << __func__
                  << ": Cancel invoked for unknown share target, returning "
                     "kOutOfOrderApiCall";
    // Make sure to clean up files just in case.
    if (!update_file_paths_in_progress_) {
      UpdateFilePath(share_target);
    }
    RemoveIncomingPayloads(share_target);
    std::move(status_codes_callback)(StatusCodes::kOutOfOrderApiCall);
    return;
  }

  // For metrics.
  all_cancelled_share_target_ids_.insert(share_target.id);

  // Cancel all ongoing payload transfers before invoking the transfer update
  // callback. Invoking the transfer update callback first could result in
  // payload cleanup before we have a chance to cancel the payload via Nearby
  // Connections, and the payload tracker might not receive the expected
  // cancellation signals. Also, note that there might not be any ongoing
  // payload transfer, for example, if a connection has not been established
  // yet.
  for (int64_t attachment_id : share_target.GetAttachmentIds()) {
    std::optional<int64_t> payload_id = GetAttachmentPayloadId(attachment_id);
    if (payload_id) {
      nearby_connections_manager_->Cancel(*payload_id);
    }
  }

  // Inform the user that the transfer has been cancelled before disconnecting
  // because subsequent disconnections might be interpreted as failure. The
  // TransferUpdateDecorator will ignore subsequent statuses in favor of this
  // cancelled status. Note that the transfer update callback might have already
  // been invoked as a result of the payload cancellations above, but again,
  // superfluous status updates are handled gracefully by the
  // TransferUpdateDecorator.
  if (info->transfer_update_callback()) {
    info->transfer_update_callback()->OnTransferUpdate(
        share_target, TransferMetadataBuilder()
                          .set_status(TransferMetadata::Status::kCancelled)
                          .build());
  }

  // If a connection exists, close the connection. Note: The initiator of a
  // cancellation waits for a short delay before closing the connection,
  // allowing for final processing by the other device. Otherwise, disconnect
  // from endpoint id directly. Note: A share attempt can be cancelled by the
  // user before a connection is fully established, in which case,
  // info->connection() will be null.
  if (info->connection()) {
    NL_LOG(INFO) << "Disconnect fully established endpoint id:"
                 << *info->endpoint_id();
    if (is_initiator_of_cancellation) {
      info->connection()->SetDisconnectionListener(
          [&, share_target, info]() {
            info->set_connection(nullptr);
            RunOnNearbySharingServiceThread(
                "api_unregister_share_target", [&, share_target]() {
                  NL_LOG(INFO)
                      << "Unregister share target in disconnection listener.";
                  UnregisterShareTarget(std::move(share_target));
                });
          });

      RunOnNearbySharingServiceThreadDelayed(
          "initiator_cancel_delay", kInitiatorCancelDelay, [&, share_target]() {
            NL_LOG(INFO) << "Close connection after cancellation delay.";
            CloseConnection(share_target);
          });

      WriteCancelFrame(*info->connection());
    } else {
      info->connection()->Close();
    }
  } else {
    NL_LOG(INFO) << "Disconnect endpoint id:" << *info->endpoint_id();
    nearby_connections_manager_->Disconnect(*info->endpoint_id());
    UnregisterShareTarget(share_target);
  }

  std::move(status_codes_callback)(StatusCodes::kOk);
}

bool NearbySharingServiceImpl::DidLocalUserCancelTransfer(
    const ShareTarget& share_target) {
  return absl::c_linear_search(locally_cancelled_share_target_ids_,
                               share_target.id);
}

void NearbySharingServiceImpl::Open(
    const ShareTarget& share_target,
    std::function<void(StatusCodes status_codes)> status_codes_callback) {
  RunOnAnyThread(
      "api_open", [&, share_target,
                   status_codes_callback = std::move(status_codes_callback)]() {
        NL_LOG(INFO) << __func__ << ": Open is called for share_target: "
                     << share_target.ToString();

        // Log analytics event of opening received attachments.
        ShareTargetInfo* info = GetShareTargetInfo(share_target);
        analytics_recorder_->NewOpenReceivedAttachments(
            share_target.GetAttachments(),
            info != nullptr ? info->session_id() : 0);

        status_codes_callback(service_extension_->Open(share_target));
      });
}

void NearbySharingServiceImpl::OpenUrl(const ::nearby::network::Url& url) {
  RunOnAnyThread("api_open_url",
                 [&, url]() { service_extension_->OpenUrl(url); });
}

void NearbySharingServiceImpl::CopyText(absl::string_view text) {
  RunOnAnyThread("api_copy_text", [&, text = std::string(text)]() {
    service_extension_->CopyText(text);
  });
}

void NearbySharingServiceImpl::JoinWifiNetwork(absl::string_view ssid,
                                               absl::string_view password) {
  RunOnAnyThread("api_join_wifi_network", [&, ssid = std::string(ssid),
                                           password = std::string(password)]() {
    service_extension_->JoinWifiNetwork(ssid, password);
  });
}

void NearbySharingServiceImpl::SetArcTransferCleanupCallback(
    std::function<void()> callback) {
  // In the case where multiple Nearby Share sessions are started, successive
  // Nearby Share bubbles shown will prevent the user from sharing while the
  // initial bubble is still active. For the successive bubble(s), we want to
  // make sure only the original cleanup callback is valid.
  // Also in the following case:
  // 1. CrOS starts a receive transfer.
  // 2. ARC starts a send transfer and |arc_transfer_cleanup_callback_| is set
  //    erroneously if |is_transferring_| check is missing.
  // As multiple transfers cannot occur at the same time, a "Can't Share" error
  // will occur. When the transfer in [1] finishes and another ARC Nearby Share
  // session starts, the |arc_transfer_cleanup_callback_| can't be set if a
  // value is already set to ensure all clean up is performed. Hence, check if
  // not |is_transferring_| before setting |arc_transfer_cleanup_callback_|.
  if (!is_transferring_ && arc_transfer_cleanup_callback_ == nullptr) {
    arc_transfer_cleanup_callback_ = std::move(callback);
  }
}

NearbyShareSettings* NearbySharingServiceImpl::GetSettings() {
  return settings_.get();
}

nearby::sharing::api::SharingRpcNotifier*
NearbySharingServiceImpl::GetRpcNotifier() {
  return nearby_share_client_factory_->GetRpcNotifier();
}

NearbyShareLocalDeviceDataManager*
NearbySharingServiceImpl::GetLocalDeviceDataManager() {
  return local_device_data_manager_.get();
}

NearbyShareContactManager* NearbySharingServiceImpl::GetContactManager() {
  return contact_manager_.get();
}

NearbyShareCertificateManager*
NearbySharingServiceImpl::GetCertificateManager() {
  return certificate_manager_.get();
}

AccountManager* NearbySharingServiceImpl::GetAccountManager() {
  return &account_manager_;
}

void NearbySharingServiceImpl::OnIncomingConnection(
    absl::string_view endpoint_id, absl::Span<const uint8_t> endpoint_info,
    NearbyConnection* connection) {
  NL_DCHECK(connection);

  // Sync down data from Nearby server when the receiving flow starts, making
  // our best effort to have fresh contact and certificate data. There is no
  // need to wait for these calls to finish. The periodic server requests will
  // typically be sufficient, but we don't want the user to be blocked for
  // hours waiting for a periodic sync.
  NL_VLOG(1)
      << __func__
      << ": Downloading local device data, contacts, and certificates from "
      << "Nearby server at start of receiving flow.";
  local_device_data_manager_->DownloadDeviceData();
  contact_manager_->DownloadContacts();
  certificate_manager_->DownloadPublicCertificates();

  ShareTarget placeholder_share_target;
  placeholder_share_target.is_incoming = true;
  ShareTargetInfo& share_target_info =
      GetOrCreateShareTargetInfo(placeholder_share_target, endpoint_id);
  share_target_info.set_connection(connection);

  // Set receiving session id.
  receiving_session_id_ = analytics_recorder_->GenerateNextId();

  connection->SetDisconnectionListener([this, placeholder_share_target]() {
    RunOnNearbySharingServiceThread(
        "disconnection_listener", [&, placeholder_share_target]() {
          RefreshUIOnDisconnection(placeholder_share_target);
        });
  });

  std::unique_ptr<Advertisement> advertisement =
      decoder_->DecodeAdvertisement(endpoint_info);
  OnIncomingAdvertisementDecoded(endpoint_id,
                                 std::move(placeholder_share_target),
                                 std::move(advertisement));
}

NearbySharingService::StatusCodes
NearbySharingServiceImpl::InternalUnregisterSendSurface(
    TransferUpdateCallback* transfer_callback,
    ShareTargetDiscoveredCallback* discovery_callback) {
  NL_DCHECK(transfer_callback);
  NL_DCHECK(discovery_callback);
  NL_LOG(INFO) << __func__ << ": UnregisterSendSurface is called"
               << ", transfer_callback: " << transfer_callback;

  if (!foreground_send_transfer_callbacks_.HasObserver(transfer_callback) &&
      !background_send_transfer_callbacks_.HasObserver(transfer_callback)) {
    NL_VLOG(1)
        << __func__
        << ": unregisterSendSurface failed. Unknown TransferUpdateCallback";
    return StatusCodes::kError;
  }

  if (!foreground_send_transfer_callbacks_.empty() && last_outgoing_metadata_ &&
      last_outgoing_metadata_->second.is_final_status()) {
    // We already saw the final status in the foreground
    // Nullify it so the next time the user opens sharing, it starts the UI from
    // the beginning
    last_outgoing_metadata_.reset();
  }

  SendSurfaceState state = SendSurfaceState::kUnknown;
  if (foreground_send_transfer_callbacks_.HasObserver(transfer_callback)) {
    foreground_send_transfer_callbacks_.RemoveObserver(transfer_callback);
    foreground_send_discovery_callbacks_.RemoveObserver(discovery_callback);
    state = SendSurfaceState::kForeground;
  } else {
    background_send_transfer_callbacks_.RemoveObserver(transfer_callback);
    background_send_discovery_callbacks_.RemoveObserver(discovery_callback);
    state = SendSurfaceState::kBackground;
  }

  // Displays the most recent payload status processed by foreground surfaces on
  // background surfaces.
  if (foreground_send_transfer_callbacks_.empty() && last_outgoing_metadata_) {
    for (auto& background_transfer_callback :
         background_send_transfer_callbacks_.GetObservers()) {
      background_transfer_callback->OnTransferUpdate(
          last_outgoing_metadata_->first, last_outgoing_metadata_->second);
    }
  }

  NL_VLOG(1) << __func__ << ": A SendSurface has been unregistered: "
             << SendSurfaceStateToString(state);
  InvalidateSurfaceState();
  return StatusCodes::kOk;
}

NearbySharingService::StatusCodes
NearbySharingServiceImpl::InternalUnregisterReceiveSurface(
    TransferUpdateCallback* transfer_callback) {
  NL_DCHECK(transfer_callback);

  NL_LOG(INFO) << __func__ << ": UnregisterReceiveSurface is called"
               << ", transfer_callback: " << transfer_callback;

  bool is_foreground =
      foreground_receive_callbacks_.HasObserver(transfer_callback);
  bool is_background =
      background_receive_callbacks_.HasObserver(transfer_callback);
  if (!is_foreground && !is_background) {
    NL_VLOG(1) << __func__
               << ": Unknown transfer callback was un-registered, ignoring.";
    // We intentionally allow this be successful so the caller can be sure
    // they are not registered anymore.
    return StatusCodes::kOk;
  }

  if (!foreground_receive_callbacks_.empty() && last_incoming_metadata_ &&
      last_incoming_metadata_->second.is_final_status()) {
    // We already saw the final status in the foreground.
    // Nullify it so the next time the user opens sharing, it starts the UI from
    // the beginning
    last_incoming_metadata_.reset();
  }

  if (is_foreground) {
    foreground_receive_callbacks_.RemoveObserver(transfer_callback);
  } else {
    background_receive_callbacks_.RemoveObserver(transfer_callback);
  }

  // Displays the most recent payload status processed by foreground surfaces on
  // background surface.
  if (foreground_receive_callbacks_.empty() && last_incoming_metadata_) {
    for (auto& background_callback :
         background_receive_callbacks_.GetObservers()) {
      background_callback->OnTransferUpdate(last_incoming_metadata_->first,
                                            last_incoming_metadata_->second);
    }
  }

  NL_VLOG(1) << __func__ << ": A ReceiveSurface("
             << (is_foreground ? "foreground" : "background")
             << ") has been unregistered";
  InvalidateSurfaceState();
  return StatusCodes::kOk;
}

std::string NearbySharingServiceImpl::Dump() const {
  std::stringstream sstream;

  // Dump Nearby Sharing Service state
  sstream << std::boolalpha;
  sstream << "Nearby Sharing Service State" << std::endl;
  sstream << "  IsScanning: " << IsScanning() << std::endl;
  sstream << "  IsConnecting: " << IsConnecting() << std::endl;
  sstream << "  IsTransferring: " << IsTransferring() << std::endl;
  sstream << "  IsSendingFile: " << IsSendingFile() << std::endl;
  sstream << "  IsReceivingFile: " << IsReceivingFile() << std::endl;

  sstream << "  IsScreenLocked: " << device_info_.IsScreenLocked()
          << std::endl;
  sstream << "  IsBluetoothPresent: " << IsBluetoothPresent() << std::endl;
  sstream << "  IsBluetoothPowered: " << IsBluetoothPowered() << std::endl;
  sstream << "  IsExtendedAdvertisingSupported: "
          << IsExtendedAdvertisingSupported() << std::endl;
  sstream << "  UpdateTrack: "
          << NearbyFlags::GetInstance().GetStringFlag(
                 sharing::config_package_nearby::nearby_sharing_feature::
                     kUpdateTrack)
          << std::endl;
  sstream << std::noboolalpha;
  sstream << std::endl;

  // Dump scheduled tasks
  sstream << "Nearby Tasks/Certificates State" << std::endl;
  sstream << " Download & upload contacts: "
          << ConvertToReadableSchedule(
                 preference_manager_,
                 prefs::kNearbySharingSchedulerContactDownloadAndUploadName)
          << std::endl;
  sstream << " Download device data: "
          << ConvertToReadableSchedule(
                 preference_manager_,
                 prefs::kNearbySharingSchedulerDownloadDeviceDataName)
          << std::endl;
  sstream << " Download public certificates: "
          << ConvertToReadableSchedule(
                 preference_manager_,
                 prefs::kNearbySharingSchedulerDownloadPublicCertificatesName)
          << std::endl;
  sstream << " Upload contacts periodically: "
          << ConvertToReadableSchedule(
                 preference_manager_,
                 prefs::kNearbySharingSchedulerPeriodicContactUploadName)
          << std::endl;
  sstream
      << " Upload local device certificates: "
      << ConvertToReadableSchedule(
             preference_manager_,
             prefs::kNearbySharingSchedulerUploadLocalDeviceCertificatesName)
      << std::endl;
  sstream << " Upload device name: "
          << ConvertToReadableSchedule(
                 preference_manager_,
                 prefs::kNearbySharingSchedulerUploadDeviceNameName)
          << std::endl;
  sstream << " Private certificates expiration: "
          << ConvertToReadableSchedule(
                 preference_manager_,
                 prefs::kNearbySharingSchedulerPrivateCertificateExpirationName)
          << std::endl;
  sstream << " Public certificates expiration: "
          << ConvertToReadableSchedule(
                 preference_manager_,
                 prefs::kNearbySharingSchedulerPublicCertificateExpirationName)
          << std::endl;

  // Dump certificates information.
  if (NearbyFlags::GetInstance().GetBoolFlag(
          sharing::config_package_nearby::nearby_sharing_feature::
              kEnableCertificatesDump)) {
    sstream << std::endl;
    sstream << certificate_manager_->Dump();
  }

  sstream << std::endl;
  sstream << nearby_connections_manager_->Dump();
  return sstream.str();
}

// Private methods for NearbyShareSettings::Observer.
void NearbySharingServiceImpl::OnSettingChanged(absl::string_view key,
                                                const Data& data) {
  if (key == prefs::kNearbySharingEnabledName) {
    bool enabled = data.value.as_bool;
    OnEnabledChanged(enabled);
  } else if (key == prefs::kNearbySharingFastInitiationNotificationStateName) {
    FastInitiationNotificationState state =
        static_cast<FastInitiationNotificationState>(data.value.as_int64);
    OnFastInitiationNotificationStateChanged(state);
  } else if (key == prefs::kNearbySharingDataUsageName) {
    DataUsage data_usage = static_cast<DataUsage>(data.value.as_int64);
    OnDataUsageChanged(data_usage);
  } else if (key == prefs::kNearbySharingCustomSavePath) {
    absl::string_view custom_save_path = data.value.as_string;
    OnCustomSavePathChanged(custom_save_path);
  } else if (key == prefs::kNearbySharingBackgroundVisibilityName) {
    DeviceVisibility visibility =
        static_cast<DeviceVisibility>(data.value.as_int64);
    OnVisibilityChanged(visibility);
  } else if (key == prefs::kNearbySharingOnboardingCompleteName) {
    bool is_complete = data.value.as_bool;
    OnIsOnboardingCompleteChanged(is_complete);
  } else if (key == prefs::kNearbySharingIsReceivingName) {
    bool is_receiving = data.value.as_bool;
    OnIsReceivingChanged(is_receiving);
  }
}

void NearbySharingServiceImpl::OnEnabledChanged(bool enabled) {
  RunOnNearbySharingServiceThread("on_enabled_changed", [&, enabled]() {
    if (enabled) {
      NL_VLOG(1) << __func__ << ": Nearby sharing enabled!";
      local_device_data_manager_->Start();
      contact_manager_->Start();
      certificate_manager_->Start();
    } else {
      NL_VLOG(1) << __func__ << ": Nearby sharing disabled!";
      StopAdvertising();
      StopScanning();
      nearby_connections_manager_->Shutdown();
      local_device_data_manager_->Stop();
      contact_manager_->Stop();
      certificate_manager_->Stop();
    }

    InvalidateSurfaceState();
  });
}

void NearbySharingServiceImpl::OnFastInitiationNotificationStateChanged(
    FastInitiationNotificationState state) {
  RunOnNearbySharingServiceThread(
      "on_fast_initiation_notification_state_changed", [&, state]() {
        if (!IsBackgroundScanningFeatureEnabled()) {
          return;
        }

        NL_VLOG(1) << __func__ << ": Fast initiation Notification state: "
                   << static_cast<int>(state);
        // Runs through a series of checks to determine if background scanning
        // should be started or stopped.
        InvalidateReceiveSurfaceState();
      });
}

void NearbySharingServiceImpl::OnIsFastInitiationHardwareSupportedChanged(
    bool is_supported) {}

void NearbySharingServiceImpl::OnDataUsageChanged(DataUsage data_usage) {
  RunOnNearbySharingServiceThread("on_data_usage_changed", [&, data_usage]() {
    NL_LOG(INFO) << __func__ << ": Nearby sharing data usage changed to "
                 << DataUsage_Name(data_usage);
    StopAdvertisingAndInvalidateSurfaceState();
  });
}

void NearbySharingServiceImpl::OnCustomSavePathChanged(
    absl::string_view custom_save_path) {
  RunOnNearbySharingServiceThread(
      "on_custom_save_path_changed",
      [&, custom_save_path = std::string(custom_save_path)]() {
        NL_LOG(INFO) << __func__
                     << ": Nearby sharing custom save path changed to "
                     << custom_save_path;
        nearby_connections_manager_->SetCustomSavePath(custom_save_path);
      });
}

void NearbySharingServiceImpl::OnVisibilityChanged(
    DeviceVisibility visibility) {
  RunOnNearbySharingServiceThread("on_visibility_changed", [&, visibility]() {
    NL_LOG(INFO) << __func__ << ": Nearby sharing visibility changed to "
                 << DeviceVisibility_Name(visibility);
    StopAdvertisingAndInvalidateSurfaceState();
  });
}

void NearbySharingServiceImpl::OnIsOnboardingCompleteChanged(bool is_complete) {
  // Log the event to analytics when is_complete is true.
  if (is_complete) {
    analytics_recorder_->NewAcceptAgreements();
  }
}

void NearbySharingServiceImpl::OnIsReceivingChanged(bool is_receiving) {
  RunOnNearbySharingServiceThread(
      "on_is_receiving_changed", [&, is_receiving]() {
        NL_LOG(INFO) << __func__ << ": Nearby sharing receiving changed to "
                     << is_receiving;
        InvalidateSurfaceState();
      });
}

// NearbyShareCertificateManager::Observer:
void NearbySharingServiceImpl::OnPublicCertificatesDownloaded() {
  if (!is_scanning_ || discovered_advertisements_to_retry_map_.empty()) {
    return;
  }

  NL_LOG(INFO) << __func__
               << ": Public certificates downloaded while scanning. "
               << "Retrying decryption with "
               << discovered_advertisements_to_retry_map_.size()
               << " previously discovered advertisements.";
  const auto map_copy = discovered_advertisements_to_retry_map_;
  discovered_advertisements_to_retry_map_.clear();
  for (const auto& id_info_pair : map_copy) {
    discovered_advertisements_retried_set_.insert(id_info_pair.first);
    OnEndpointDiscovered(id_info_pair.first, id_info_pair.second);
  }
}

void NearbySharingServiceImpl::OnPrivateCertificatesChanged() {
  RunOnNearbySharingServiceThread("on-private-certificates-changed", [&]() {
    StopAdvertisingAndInvalidateSurfaceState();
  });
}

void NearbySharingServiceImpl::OnLoginSucceeded(absl::string_view account_id) {
  RunOnNearbySharingServiceThread(
      "on_login_succeeded", [&, account_id = std::string(account_id)]() {
        NL_LOG(INFO) << __func__ << ": Account login.";

        ResetAllSettings(/*logout=*/false);
      });
}

void NearbySharingServiceImpl::OnLogoutSucceeded(absl::string_view account_id) {
  RunOnNearbySharingServiceThread(
      "on_logout_succeeded", [&, account_id = std::string(account_id)]() {
        NL_LOG(INFO) << __func__ << ": Account logout.";

        // Reset all settings.
        ResetAllSettings(/*logout=*/true);
      });
}

// NearbyConnectionsManager::DiscoveryListener:
void NearbySharingServiceImpl::OnEndpointDiscovered(
    absl::string_view endpoint_id, absl::Span<const uint8_t> endpoint_info) {
  // The calling thread may already be completed when calling lambda. We
  // make a local copy of calling parameter to avoid possible memory
  // issue.
  std::vector<uint8_t> endpoint_info_copy{endpoint_info.begin(),
                                          endpoint_info.end()};
  RunOnNearbySharingServiceThread(
      "on_endpoint_discovered",
      [&, endpoint_id = std::string(endpoint_id),
       endpoint_info_copy = std::move(endpoint_info_copy)]() {
        AddEndpointDiscoveryEvent([&, endpoint_id, endpoint_info_copy]() {
          HandleEndpointDiscovered(endpoint_id, endpoint_info_copy);
        });
      });
}

void NearbySharingServiceImpl::OnEndpointLost(absl::string_view endpoint_id) {
  RunOnNearbySharingServiceThread(
      "on_endpoint_lost", [&, endpoint_id = std::string(endpoint_id)]() {
        AddEndpointDiscoveryEvent(
            [&, endpoint_id]() { HandleEndpointLost(endpoint_id); });
      });
}

void NearbySharingServiceImpl::OnLockStateChanged(bool locked) {
  RunOnNearbySharingServiceThread("on_lock_state_changed", [&, locked]() {
    NL_VLOG(1) << __func__ << ": Screen lock state changed. (" << locked << ")";
    is_screen_locked_ = locked;
    InvalidateSurfaceState();
  });
}

void NearbySharingServiceImpl::AdapterPresentChanged(
    sharing::api::BluetoothAdapter* adapter, bool present) {
  RunOnNearbySharingServiceThread("bt_adapter_present_changed", [&, present]() {
    NL_VLOG(1) << __func__ << ": Bluetooth adapter present state changed. ("
               << present << ")";
    for (auto& observer : observers_.GetObservers()) {
      observer->OnBluetoothStatusChanged();
    }
    InvalidateSurfaceState();
  });
}

void NearbySharingServiceImpl::AdapterPoweredChanged(
    sharing::api::BluetoothAdapter* adapter, bool powered) {
  RunOnNearbySharingServiceThread("bt_adapter_power_changed", [&, powered]() {
    NL_VLOG(1) << __func__ << ": Bluetooth adapter power state changed. ("
               << powered << ")";
    for (auto& observer : observers_.GetObservers()) {
      observer->OnBluetoothStatusChanged();
    }
    InvalidateSurfaceState();
  });
}

void NearbySharingServiceImpl::AdapterPresentChanged(
    sharing::api::WifiAdapter* adapter, bool present) {
  RunOnNearbySharingServiceThread(
      "wifi_adapter_present_changed", [&, present]() {
        NL_VLOG(1) << __func__ << ": Wifi adapter present state changed. ("
                   << present << ")";
        for (auto& observer : observers_.GetObservers()) {
          observer->OnWifiStatusChanged();
        }
        InvalidateSurfaceState();
      });
}

void NearbySharingServiceImpl::AdapterPoweredChanged(
    sharing::api::WifiAdapter* adapter, bool powered) {
  RunOnNearbySharingServiceThread("wifi_adapter_power_changed", [&, powered]() {
    NL_VLOG(1) << __func__ << ": Wifi adapter power state changed. (" << powered
               << ")";
    for (auto& observer : observers_.GetObservers()) {
      observer->OnWifiStatusChanged();
    }
    InvalidateSurfaceState();
  });
}

void NearbySharingServiceImpl::HardwareErrorReported(
    NearbyFastInitiation* fast_init) {
  RunOnNearbySharingServiceThread("hardware_error_reported", [&]() {
    NL_VLOG(1) << __func__ << ": Hardware error reported, need to restart PC.";
    for (auto& observer : observers_.GetObservers()) {
      observer->OnIrrecoverableHardwareErrorReported();
    }
    InvalidateSurfaceState();
  });
}

void NearbySharingServiceImpl::SetupBluetoothAdapter() {
  NL_VLOG(1) << __func__ << ": Setup bluetooth adapter.";
  context_->GetBluetoothAdapter().AddObserver(this);
  InvalidateSurfaceState();
}

ObserverList<TransferUpdateCallback>&
NearbySharingServiceImpl::GetReceiveCallbacksFromState(
    ReceiveSurfaceState state) {
  switch (state) {
    case ReceiveSurfaceState::kForeground:
      return foreground_receive_callbacks_;
    case ReceiveSurfaceState::kBackground:
      return background_receive_callbacks_;
    case ReceiveSurfaceState::kUnknown:
      return foreground_receive_callbacks_;
  }
}

bool NearbySharingServiceImpl::IsVisibleInBackground(
    DeviceVisibility visibility) {
  return visibility == DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS ||
         visibility == DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS ||
         visibility == DeviceVisibility::DEVICE_VISIBILITY_EVERYONE ||
         visibility == DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE;
}

std::optional<std::vector<uint8_t>>
NearbySharingServiceImpl::CreateEndpointInfo(
    const std::optional<std::string>& device_name) const {
  std::vector<uint8_t> salt;
  std::vector<uint8_t> encrypted_key;

  if (account_manager_.GetCurrentAccount().has_value()) {
    // If the user already signed in, setup contacts certificate for everyone
    // mode to show correct user icon on remote device.
    DeviceVisibility visibility = settings_->GetVisibility();
    if (visibility == proto::DEVICE_VISIBILITY_EVERYONE) {
      // Make sure using all contacts certificate for everyone mode
      visibility = proto::DEVICE_VISIBILITY_ALL_CONTACTS;
    }

    std::optional<NearbyShareEncryptedMetadataKey> encrypted_metadata_key =
        certificate_manager_->EncryptPrivateCertificateMetadataKey(visibility);
    if (encrypted_metadata_key.has_value()) {
      salt = encrypted_metadata_key->salt();
      encrypted_key = encrypted_metadata_key->encrypted_key();
    } else {
      NL_LOG(WARNING) << __func__
                      << ": Failed to encrypt private certificate metadata key "
                      << "for advertisement.";
    }
  }

  // Generate random metadata key for non-login user or failed to generate
  // metadata keys for login user.

  if (salt.empty() || encrypted_key.empty()) {
    salt = GenerateRandomBytes(sharing::Advertisement::kSaltSize);
    encrypted_key = GenerateRandomBytes(
        sharing::Advertisement::kMetadataEncryptionKeyHashByteSize);
  }

  ShareTargetType device_type =
      static_cast<ShareTargetType>(device_info_.GetDeviceType());

  std::unique_ptr<Advertisement> advertisement = Advertisement::NewInstance(
      std::move(salt), std::move(encrypted_key), device_type, device_name);
  if (advertisement) {
    return advertisement->ToEndpointInfo();
  } else {
    return std::nullopt;
  }
}

void NearbySharingServiceImpl::StartFastInitiationAdvertising() {
  NL_VLOG(1) << __func__ << ": Starting fast initiation advertising.";

  if (nearby_fast_initiation_->IsAdvertising()) {
    return;
  }

  nearby_fast_initiation_->StartAdvertising(
      NearbyFastInitiation::FastInitType::kSilent,
      [&]() { OnStartFastInitiationAdvertising(); },
      [&]() { OnStartFastInitiationAdvertisingError(); });
  NL_VLOG(1) << __func__ << ": Fast initiation advertising in kSilent mode.";

  // Log analytics event of sending fast initiation.
  analytics_recorder_->NewSendFastInitialization();
}

void NearbySharingServiceImpl::OnStartFastInitiationAdvertising() {
  NL_VLOG(1) << __func__ << ": Started fast initiation advertising.";
}

void NearbySharingServiceImpl::OnStartFastInitiationAdvertisingError() {
  NL_LOG(ERROR) << __func__ << ": Failed to start fast initiation advertising.";
}

void NearbySharingServiceImpl::StopFastInitiationAdvertising() {
  NL_VLOG(1) << __func__ << ": Stopping fast initiation advertising.";

  if (!nearby_fast_initiation_->IsAdvertising()) {
    return;
  }

  nearby_fast_initiation_->StopAdvertising(
      [&]() { OnStopFastInitiationAdvertising(); });
}

void NearbySharingServiceImpl::OnStopFastInitiationAdvertising() {
  NL_VLOG(1) << __func__ << ": Stopped fast initiation advertising";
}

// Processes endpoint discovered/lost events. We queue up the events to ensure
// each discovered or lost event is fully handled before the next is run. For
// example, we don't want to start processing an endpoint-lost event before
// the corresponding endpoint-discovered event is finished. This is especially
// important because of the asynchronous steps required to process an
// endpoint-discovered event.
void NearbySharingServiceImpl::AddEndpointDiscoveryEvent(
    std::function<void()> event) {
  endpoint_discovery_events_.push(std::move(event));
  if (endpoint_discovery_events_.size() == 1u) {
    auto discovery_event = std::move(endpoint_discovery_events_.front());
    discovery_event();
  }
}

void NearbySharingServiceImpl::HandleEndpointDiscovered(
    absl::string_view endpoint_id, absl::Span<const uint8_t> endpoint_info) {
  NL_VLOG(1) << __func__ << ": endpoint_id=" << endpoint_id
             << ", endpoint_info=" << nearby::utils::HexEncode(endpoint_info);
  if (!is_scanning_) {
    NL_VLOG(1)
        << __func__
        << ": Ignoring discovered endpoint because we're no longer scanning";
    FinishEndpointDiscoveryEvent();
    return;
  }

  std::unique_ptr<Advertisement> advertisement =
      decoder_->DecodeAdvertisement(endpoint_info);
  OnOutgoingAdvertisementDecoded(endpoint_id, endpoint_info,
                                 std::move(advertisement));
}

void NearbySharingServiceImpl::HandleEndpointLost(
    absl::string_view endpoint_id) {
  NL_VLOG(1) << __func__ << ": endpoint_id=" << endpoint_id;

  if (!is_scanning_) {
    NL_VLOG(1) << __func__
               << ": Ignoring lost endpoint because we're no longer scanning";
    FinishEndpointDiscoveryEvent();
    return;
  }

  discovered_advertisements_to_retry_map_.erase(endpoint_id);
  discovered_advertisements_retried_set_.erase(endpoint_id);
  RemoveOutgoingShareTargetWithEndpointId(endpoint_id);
  FinishEndpointDiscoveryEvent();
}

void NearbySharingServiceImpl::FinishEndpointDiscoveryEvent() {
  NL_DCHECK(!endpoint_discovery_events_.empty());
  NL_DCHECK(endpoint_discovery_events_.front() == nullptr);
  endpoint_discovery_events_.pop();

  // Handle the next queued up endpoint discovered/lost event.
  if (!endpoint_discovery_events_.empty()) {
    NL_DCHECK(endpoint_discovery_events_.front() != nullptr);
    auto discovery_event = std::move(endpoint_discovery_events_.front());
    discovery_event();
  }
}

void NearbySharingServiceImpl::OnOutgoingAdvertisementDecoded(
    absl::string_view endpoint_id, absl::Span<const uint8_t> endpoint_info,
    std::unique_ptr<Advertisement> advertisement) {
  if (!advertisement) {
    NL_LOG(WARNING) << __func__
                    << ": Failed to parse discovered advertisement.";
    FinishEndpointDiscoveryEvent();
    return;
  }

  // Now we will report endpoints met before in NearbyConnectionsManager.
  // Check outgoingShareTargetInfoMap first and pass the same shareTarget if we
  // found one.

  // Looking for the ShareTarget based on endpoint id.
  if (outgoing_share_target_map_.find(endpoint_id) !=
      outgoing_share_target_map_.end()) {
    FinishEndpointDiscoveryEvent();
    return;
  }

  // Once we get the advertisement, the first thing to do is decrypt the
  // certificate.
  NearbyShareEncryptedMetadataKey encrypted_metadata_key(
      advertisement->salt(), advertisement->encrypted_metadata_key());

  std::string endpoint_id_copy = std::string(endpoint_id);
  std::vector<uint8_t> endpoint_info_copy{endpoint_info.begin(),
                                          endpoint_info.end()};
  GetCertificateManager()->GetDecryptedPublicCertificate(
      std::move(encrypted_metadata_key),
      [this, endpoint_id_copy, endpoint_info_copy,
       advertisement_copy =
           *advertisement](std::optional<NearbyShareDecryptedPublicCertificate>
                               decrypted_public_certificate) {
        std::unique_ptr<Advertisement> advertisement =
            Advertisement::NewInstance(
                advertisement_copy.salt(),
                advertisement_copy.encrypted_metadata_key(),
                advertisement_copy.device_type(),
                advertisement_copy.device_name());
        OnOutgoingDecryptedCertificate(endpoint_id_copy, endpoint_info_copy,
                                       std::move(advertisement),
                                       decrypted_public_certificate);
      });
}

void NearbySharingServiceImpl::OnOutgoingDecryptedCertificate(
    absl::string_view endpoint_id, absl::Span<const uint8_t> endpoint_info,
    std::unique_ptr<Advertisement> advertisement,
    std::optional<NearbyShareDecryptedPublicCertificate> certificate) {
  // Check again for this endpoint id, to avoid race conditions.
  if (outgoing_share_target_map_.find(endpoint_id) !=
      outgoing_share_target_map_.end()) {
    FinishEndpointDiscoveryEvent();
    return;
  }

  // The certificate provides the device name, in order to create a ShareTarget
  // to represent this remote device.
  std::optional<ShareTarget> share_target = CreateShareTarget(
      endpoint_id, std::move(advertisement), std::move(certificate),
      /*is_incoming=*/false);
  if (!share_target.has_value()) {
    if (discovered_advertisements_retried_set_.contains(endpoint_id)) {
      NL_LOG(INFO)
          << __func__
          << ": Don't try to download public certificates again for endpoint="
          << endpoint_id;
      FinishEndpointDiscoveryEvent();
      return;
    }

    NL_LOG(INFO)
        << __func__ << ": Failed to convert discovered advertisement to share "
        << "target. Ignoring endpoint until next certificate download.";
    std::vector<uint8_t> endpoint_info_data(endpoint_info.begin(),
                                            endpoint_info.end());

    discovered_advertisements_to_retry_map_[endpoint_id] = endpoint_info_data;
    FinishEndpointDiscoveryEvent();
    return;
  }

  // Update the endpoint id for the share target.
  NL_LOG(INFO) << __func__
               << ": An endpoint has been discovered, with an advertisement "
                  "containing a valid share target.";

  // Log analytics event of discovering share target.
  analytics_recorder_->NewDiscoverShareTarget(
      *share_target, scanning_session_id_,
      absl::ToInt64Milliseconds(context_->GetClock()->Now() -
                                scanning_start_timestamp_),
      /*flow_id=*/1, /*referrer_package=*/std::nullopt,
      share_foreground_send_surface_start_timestamp_ == absl::InfinitePast()
          ? -1
          : absl::ToInt64Milliseconds(
                context_->GetClock()->Now() -
                share_foreground_send_surface_start_timestamp_));

  // Notifies the user that we discovered a device.
  NL_VLOG(1) << __func__ << ": There are "
             << (foreground_send_discovery_callbacks_.size() +
                 background_send_discovery_callbacks_.size())
             << " discovery callbacks be called.";

  for (ShareTargetDiscoveredCallback* discovery_callback :
       foreground_send_discovery_callbacks_.GetObservers()) {
    discovery_callback->OnShareTargetDiscovered(*share_target);
  }
  for (ShareTargetDiscoveredCallback* discovery_callback :
       background_send_discovery_callbacks_.GetObservers()) {
    discovery_callback->OnShareTargetDiscovered(*share_target);
  }

  NL_VLOG(1) << __func__ << ": Reported OnShareTargetDiscovered "
             << (context_->GetClock()->Now() - scanning_start_timestamp_);

  FinishEndpointDiscoveryEvent();
}

void NearbySharingServiceImpl::ScheduleCertificateDownloadDuringDiscovery(
    size_t attempt_count) {
  if (attempt_count >= kMaxCertificateDownloadsDuringDiscovery) {
    return;
  }

  if (certificate_download_during_discovery_timer_->IsRunning()) {
    certificate_download_during_discovery_timer_->Stop();
  }

  certificate_download_during_discovery_timer_->Start(
      absl::ToInt64Milliseconds(kCertificateDownloadDuringDiscoveryPeriod), 0,
      [&, attempt_count]() {
        OnCertificateDownloadDuringDiscoveryTimerFired(attempt_count);
      });
}

void NearbySharingServiceImpl::OnCertificateDownloadDuringDiscoveryTimerFired(
    size_t attempt_count) {
  if (!is_scanning_) {
    return;
  }

  if (!discovered_advertisements_to_retry_map_.empty()) {
    NL_VLOG(1) << __func__ << ": Detected "
               << discovered_advertisements_to_retry_map_.size()
               << " discovered advertisements that could not decrypt any "
               << "public certificates. Re-downloading certificates.";
    certificate_manager_->DownloadPublicCertificates();
    ++attempt_count;
  }

  ScheduleCertificateDownloadDuringDiscovery(attempt_count);
}

bool NearbySharingServiceImpl::IsBluetoothPresent() const {
  return context_->GetBluetoothAdapter().IsPresent();
}

bool NearbySharingServiceImpl::IsBluetoothPowered() const {
  return context_->GetBluetoothAdapter().IsPowered();
}

bool NearbySharingServiceImpl::IsExtendedAdvertisingSupported() const {
  return context_->GetBluetoothAdapter().IsExtendedAdvertisingSupported();
}

bool NearbySharingServiceImpl::IsLanConnected() const {
  return context_->GetConnectivityManager()->IsLanConnected();
}

bool NearbySharingServiceImpl::IsWifiPresent() const {
  return context_->GetWifiAdapter().IsPresent();
}

bool NearbySharingServiceImpl::IsWifiPowered() const {
  return context_->GetWifiAdapter().IsPowered();
}

bool NearbySharingServiceImpl::HasAvailableConnectionMediums() {
  // Check if Wi-Fi or Ethernet LAN is off.  Advertisements won't work, so
  // disable them, unless bluetooth is known to be enabled. Not all platforms
  // have bluetooth, so Wi-Fi LAN is a platform-agnostic check.
  bool is_wifi_lan_enabled = NearbyFlags::GetInstance().GetBoolFlag(
      config_package_nearby::nearby_sharing_feature::kEnableMediumWifiLan);

  ConnectivityManager::ConnectionType connection_type =
      context_->GetConnectivityManager()->GetConnectionType();

  bool hasNetworkConnection =
      connection_type == ConnectivityManager::ConnectionType::kWifi ||
      connection_type == ConnectivityManager::ConnectionType::kEthernet;

  return IsBluetoothPowered() || (is_wifi_lan_enabled && hasNetworkConnection);
}

void NearbySharingServiceImpl::InvalidateSurfaceState() {
  InvalidateSendSurfaceState();
  InvalidateReceiveSurfaceState();
}

void NearbySharingServiceImpl::InvalidateSendSurfaceState() {
  InvalidateScanningState();
  InvalidateFastInitiationAdvertising();
}

void NearbySharingServiceImpl::InvalidateScanningState() {
  // Stop scanning when screen is off.
  if (is_screen_locked_) {
    StopScanning();
    NL_VLOG(1) << __func__
               << ": Stopping discovery because the screen is locked.";
    return;
  }

  if (!HasAvailableConnectionMediums()) {
    StopScanning();
    NL_VLOG(1) << __func__
               << ": Stopping scanning because both bluetooth and wifi LAN are "
                  "disabled.";
    return;
  }

  // Nearby Sharing is disabled. Don't advertise.
  if (!settings_->GetEnabled()) {
    StopScanning();
    NL_VLOG(1) << __func__
               << ": Stopping discovery because Nearby Sharing is disabled.";
    return;
  }

  if (is_transferring_ || is_connecting_) {
    StopScanning();
    NL_VLOG(1)
        << __func__
        << ": Stopping discovery because we're currently in the midst of a "
           "transfer.";
    return;
  }

  if (foreground_send_transfer_callbacks_.empty()) {
    StopScanning();
    NL_VLOG(1) << __func__
               << ": Stopping discovery because no scanning surface has been "
                  "registered.";
    return;
  }

  // Screen is on, Bluetooth is enabled, and Nearby Sharing is enabled! Start
  // discovery.
  StartScanning();
}

void NearbySharingServiceImpl::InvalidateFastInitiationAdvertising() {
  // Screen is off. Do no work.
  if (is_screen_locked_) {
    StopFastInitiationAdvertising();
    NL_VLOG(1) << __func__
               << ": Stopping fast initiation advertising because the "
                  "screen is locked.";
    return;
  }

  if (!IsBluetoothPowered()) {
    StopFastInitiationAdvertising();
    NL_VLOG(1) << __func__
               << ": Stopping fast initiation advertising because "
                  "bluetooth is disabled due to powered off.";
    return;
  }

  // Nearby Sharing is disabled. Don't advertise.
  if (!settings_->GetEnabled()) {
    StopFastInitiationAdvertising();
    NL_VLOG(1) << __func__
               << ": Stopping fast initiation advertising because Nearby "
                  "Sharing is disabled.";
    return;
  }

  if (is_transferring_ || is_connecting_) {
    StopFastInitiationAdvertising();
    NL_VLOG(1) << __func__
               << ": Stopping fast initiation advertising because we're "
                  "currently in the midst of a "
                  "transfer.";
    return;
  }

  if (foreground_send_transfer_callbacks_.empty()) {
    StopFastInitiationAdvertising();
    NL_VLOG(1) << __func__
               << ": Stopping fast initiation advertising because no send "
                  "surface is registered.";
    return;
  }

  StartFastInitiationAdvertising();
}

void NearbySharingServiceImpl::InvalidateReceiveSurfaceState() {
  InvalidateAdvertisingState();
  if (IsBackgroundScanningFeatureEnabled()) {
    InvalidateFastInitiationScanning();
  }
}

void NearbySharingServiceImpl::InvalidateAdvertisingState() {
  if (!settings_->GetIsReceiving()) {
    StopAdvertising();
    NL_VLOG(1) << __func__
               << ": Stopping advertising because receiving is disabled.";
    return;
  }

  // Do not advertise on lock screen unless Self Share is enabled.
  if (is_screen_locked_ &&
      !NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_sharing_feature::kEnableSelfShareUi)) {
    StopAdvertising();
    NL_VLOG(1) << __func__
               << ": Stopping advertising because the screen is locked.";
    return;
  }

  if (!HasAvailableConnectionMediums()) {
    StopAdvertising();
    NL_VLOG(1)
        << __func__
        << ": Stopping advertising because both bluetooth and wifi LAN are "
           "disabled.";
    return;
  }

  // Nearby Sharing is disabled. Don't advertise.
  if (!settings_->GetEnabled()) {
    StopAdvertising();
    NL_VLOG(1) << __func__
               << ": Stopping advertising because Nearby Sharing is disabled.";
    return;
  }

  // We're scanning for other nearby devices. Don't advertise.
  if (is_scanning_) {
    StopAdvertising();
    NL_VLOG(1)
        << __func__
        << ": Stopping advertising because we're scanning for other devices.";
    return;
  }

  if (is_transferring_) {
    StopAdvertising();
    NL_VLOG(1)
        << __func__
        << ": Stopping advertising because we're currently in the midst of "
           "a transfer.";
    return;
  }

  if (foreground_receive_callbacks_.empty() &&
      background_receive_callbacks_.empty()) {
    StopAdvertising();
    NL_VLOG(1)
        << __func__
        << ": Stopping advertising because no receive surface is registered.";
    return;
  }

  if (!IsVisibleInBackground(settings_->GetVisibility()) &&
      foreground_receive_callbacks_.empty()) {
    StopAdvertising();
    NL_VLOG(1)
        << __func__
        << ": Stopping advertising because no high power receive surface "
           "is registered and device is visible to NO_ONE.";
    return;
  }

  PowerLevel power_level;
  if (!foreground_receive_callbacks_.empty()) {
    power_level = PowerLevel::kHighPower;
  } else {
    power_level = PowerLevel::kLowPower;
  }

  DataUsage data_usage = settings_->GetDataUsage();
  if (advertising_power_level_ != PowerLevel::kUnknown) {
    if (power_level == advertising_power_level_) {
      NL_VLOG(1) << __func__
                 << ": Ignoring, already advertising with power level "
                 << PowerLevelToString(advertising_power_level_)
                 << " and data usage preference "
                 << static_cast<int>(data_usage);
      return;
    }

    StopAdvertising();
    NL_VLOG(1) << __func__ << ": Restart advertising with power level "
               << PowerLevelToString(power_level)
               << " and data usage preference " << static_cast<int>(data_usage);
  }

  std::optional<std::string> device_name;
  if (settings_->GetVisibility() ==
      DeviceVisibility::DEVICE_VISIBILITY_EVERYONE) {
    device_name = local_device_data_manager_->GetDeviceName();
  }

  // Starts advertising through Nearby Connections. Caller is expected to ensure
  // |listener| remains valid until StopAdvertising is called.
  std::optional<std::vector<uint8_t>> endpoint_info =
      CreateEndpointInfo(device_name);
  if (!endpoint_info) {
    NL_VLOG(1) << __func__
               << ": Unable to advertise since could not parse the "
                  "endpoint info from the advertisement.";
    return;
  }
  bool used_device_name = device_name.has_value();
  if (used_device_name) {
    for (auto& observer : observers_.GetObservers()) {
      observer->OnHighVisibilityChangeRequested();
    }
  }

  advertising_session_id_ = analytics_recorder_->GenerateNextId();

  nearby_connections_manager_->StartAdvertising(
      *endpoint_info,
      /*listener=*/this, power_level, data_usage,
      [&, used_device_name, data_usage](Status status) {
        // Log analytics event of advertising start.
        analytics_recorder_->NewAdvertiseDevicePresenceStart(
            advertising_session_id_,
            used_device_name ? DeviceVisibility::DEVICE_VISIBILITY_EVERYONE
                             : settings_->GetVisibility(),
            status == Status::kSuccess ? SessionStatus::SUCCEEDED_SESSION_STATUS
                                       : SessionStatus::FAILED_SESSION_STATUS,
            data_usage, std::nullopt);

        OnStartAdvertisingResult(used_device_name, status);
      });

  advertising_power_level_ = power_level;
  NL_VLOG(1) << __func__
             << ": StartAdvertising requested over Nearby Connections: "
             << " power level: " << PowerLevelToString(power_level)
             << " visibility: "
             << DeviceVisibility_Name(settings_->GetVisibility())
             << " data usage: " << DataUsage_Name(data_usage)
             << " advertise device name?: "
             << (device_name.has_value() ? "yes" : "no");

  ScheduleRotateBackgroundAdvertisementTimer();
}

void NearbySharingServiceImpl::StopAdvertising() {
  if (advertising_power_level_ == PowerLevel::kUnknown) {
    NL_VLOG(1) << __func__ << ": Not currently advertising, ignoring.";
    return;
  }

  nearby_connections_manager_->StopAdvertising([&](Status status) {
    // Log analytics event of advertising end.
    analytics_recorder_->NewAdvertiseDevicePresenceEnd(advertising_session_id_);
    OnStopAdvertisingResult(status);
  });

  NL_VLOG(1) << __func__ << ": Stop advertising requested";

  // Set power level to unknown immediately instead of waiting for the callback.
  // In the case of restarting advertising (e.g. turning off high visibility
  // with contact-based enabled), StartAdvertising will be called
  // immediately after StopAdvertising and will fail if the power level
  // indicates already advertising.
  advertising_power_level_ = PowerLevel::kUnknown;
}

void NearbySharingServiceImpl::StartScanning() {
  NL_DCHECK(settings_->GetEnabled());
  NL_DCHECK(!is_screen_locked_);
  NL_DCHECK(HasAvailableConnectionMediums());
  NL_DCHECK(!foreground_send_transfer_callbacks_.empty());

  if (is_scanning_) {
    NL_VLOG(1) << __func__ << ": We're currently scanning, ignoring.";
    return;
  }

  scanning_start_timestamp_ = context_->GetClock()->Now();
  share_foreground_send_surface_start_timestamp_ = absl::InfinitePast();
  is_scanning_ = true;
  InvalidateReceiveSurfaceState();

  ClearOutgoingShareTargetInfoMap();
  discovered_advertisements_to_retry_map_.clear();
  discovered_advertisements_retried_set_.clear();

  scanning_session_id_ = analytics_recorder_->GenerateNextId();

  nearby_connections_manager_->StartDiscovery(
      /*listener=*/this, settings_->GetDataUsage(), [&](Status status) {
        // Log analytics event of starting discovery.
        analytics::AnalyticsInformation analytics_information;
        analytics_information.send_surface_state =
            foreground_send_discovery_callbacks_.empty()
                ? analytics::SendSurfaceState::kBackground
                : analytics::SendSurfaceState::kForeground;
        analytics_recorder_->NewScanForShareTargetsStart(
            scanning_session_id_,
            status == Status::kSuccess ? SessionStatus::SUCCEEDED_SESSION_STATUS
                                       : SessionStatus::FAILED_SESSION_STATUS,
            analytics_information,
            /*flow_id=*/1, /*referrer_package=*/std::nullopt);
        OnStartDiscoveryResult(status);
      });

  InvalidateSendSurfaceState();
  NL_VLOG(1) << __func__ << ": Scanning has started";
}

NearbySharingService::StatusCodes NearbySharingServiceImpl::StopScanning() {
  if (!is_scanning_) {
    NL_VLOG(1) << __func__ << ": Not currently scanning, ignoring.";
    return StatusCodes::kStatusAlreadyStopped;
  }

  // Log analytics event of scanning end.
  analytics_recorder_->NewScanForShareTargetsEnd(scanning_session_id_);

  nearby_connections_manager_->StopDiscovery();
  is_scanning_ = false;

  certificate_download_during_discovery_timer_->Stop();
  discovered_advertisements_to_retry_map_.clear();
  discovered_advertisements_retried_set_.clear();

  // Note: We don't know if we stopped scanning in preparation to send a file,
  // or we stopped because the user left the page. We'll invalidate after a
  // short delay.

  RunOnNearbySharingServiceThreadDelayed("invalidate_delay", kInvalidateDelay,
                                         [&]() { InvalidateSurfaceState(); });

  NL_VLOG(1) << __func__ << ": Scanning has stopped.";
  return StatusCodes::kOk;
}

void NearbySharingServiceImpl::StopAdvertisingAndInvalidateSurfaceState() {
  if (advertising_power_level_ != PowerLevel::kUnknown) StopAdvertising();
  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::InvalidateFastInitiationScanning() {
  if (!IsBackgroundScanningFeatureEnabled()) return;

  bool is_hardware_offloading_supported =
      IsBluetoothPresent() && nearby_fast_initiation_->IsScanOffloadSupported();

  // Hardware offloading support is computed when the bluetooth adapter becomes
  // available. We set the hardware supported state on |settings_| to notify the
  // UI of state changes. InvalidateFastInitiationScanning gets triggered on
  // adapter change events.
  settings_->SetIsFastInitiationHardwareSupported(
      is_hardware_offloading_supported);

  if (fast_initiation_scanner_cooldown_timer_->IsRunning()) {
    NL_VLOG(1) << __func__
               << ": Stopping background scanning due to post-transfer "
                  "cooldown period";
    StopFastInitiationScanning();
    return;
  }

  if (settings_->GetFastInitiationNotificationState() !=
      FastInitiationNotificationState::ENABLED_FAST_INIT) {
    NL_VLOG(1) << __func__
               << ": Stopping background scanning; fast initiation "
                  "notification is disabled";
    StopFastInitiationScanning();
    return;
  }

  if (!settings_->GetEnabled()) {
    NL_VLOG(1) << __func__
               << ": Stopping background scanning because Nearby Sharing "
                  "is disabled";
    StopFastInitiationScanning();
    return;
  }

  // Screen is off. Do no work.
  if (is_screen_locked_) {
    NL_VLOG(1)
        << __func__
        << ": Stopping background scanning because the screen is locked.";
    StopFastInitiationScanning();
    return;
  }

  if (!IsBluetoothPowered()) {
    NL_VLOG(1)
        << __func__
        << ": Stopping background scanning because bluetooth is powered down.";
    StopFastInitiationScanning();
    return;
  }

  // We're scanning for other nearby devices. Don't background scan.
  if (is_scanning_) {
    NL_VLOG(1) << __func__
               << ": Stopping background scanning because we're scanning "
                  "for other devices.";
    StopFastInitiationScanning();
    return;
  }

  if (is_transferring_) {
    NL_VLOG(1) << __func__
               << ": Stopping background scanning because we're currently "
                  "in the midst of a transfer.";
    StopFastInitiationScanning();
    return;
  }

  if (advertising_power_level_ == PowerLevel::kHighPower) {
    NL_VLOG(1) << __func__
               << ": Stopping background scanning because we're already "
                  "in high visibility mode.";
    StopFastInitiationScanning();
    return;
  }

  if (!is_hardware_offloading_supported) {
    NL_VLOG(1) << __func__
               << ": Stopping background scanning because hardware "
                  "support is not available or not ready.";
    StopFastInitiationScanning();
    return;
  }

  StartFastInitiationScanning();
}

void NearbySharingServiceImpl::StartFastInitiationScanning() {
  NL_VLOG(1) << __func__ << ": Starting background scanning.";

  if (nearby_fast_initiation_->IsScanning()) {
    return;
  }

  nearby_fast_initiation_->StartScanning(
      [&]() { OnFastInitiationDevicesDetected(); },
      [&]() { OnFastInitiationDevicesNotDetected(); },
      [&]() { StopFastInitiationScanning(); });
}

void NearbySharingServiceImpl::OnFastInitiationDevicesDetected() {
  NL_VLOG(1) << __func__;

  for (auto& observer : observers_.GetObservers()) {
    observer->OnFastInitiationDevicesDetected();
  }
}

void NearbySharingServiceImpl::OnFastInitiationDevicesNotDetected() {
  NL_VLOG(1) << __func__;
  for (auto& observer : observers_.GetObservers()) {
    observer->OnFastInitiationDevicesNotDetected();
  }
}

void NearbySharingServiceImpl::StopFastInitiationScanning() {
  NL_VLOG(1) << __func__ << ": Stop fast initiation scanning.";
  if (!nearby_fast_initiation_->IsScanning()) {
    return;
  }

  nearby_fast_initiation_->StopScanning([&]() {
    NL_VLOG(1) << __func__ << ": Stopped fast initiation scanning.";
  });

  for (auto& observer : observers_.GetObservers()) {
    observer->OnFastInitiationScanningStopped();
  }
  NL_VLOG(1) << __func__ << ": Stopped background scanning.";
}

void NearbySharingServiceImpl::ScheduleRotateBackgroundAdvertisementTimer() {
  absl::BitGen bitgen;
  uint64_t delayRangeMilliseconds =
      absl::ToInt64Milliseconds(kBackgroundAdvertisementRotationDelayMax -
                                kBackgroundAdvertisementRotationDelayMin);
  uint64_t bias = absl::Uniform(bitgen, 0u, delayRangeMilliseconds);
  uint64_t delayMilliseconds =
      bias +
      absl::ToInt64Milliseconds(kBackgroundAdvertisementRotationDelayMin);
  if (rotate_background_advertisement_timer_->IsRunning()) {
    rotate_background_advertisement_timer_->Stop();
  }
  rotate_background_advertisement_timer_->Start(delayMilliseconds, 0, [&]() {
    OnRotateBackgroundAdvertisementTimerFired();
  });
}

void NearbySharingServiceImpl::OnRotateBackgroundAdvertisementTimerFired() {
  NL_LOG(INFO) << __func__ << ": Rotate background advertisement timer fired.";

  RunOnNearbySharingServiceThread(
      "on-rotate-background-advertisement-timer-fired", [&]() {
        if (!foreground_receive_callbacks_.empty()) {
          rotate_background_advertisement_timer_->Stop();
          ScheduleRotateBackgroundAdvertisementTimer();
        } else {
          StopAdvertising();
          InvalidateSurfaceState();
        }
      });
}

void NearbySharingServiceImpl::RemoveOutgoingShareTargetWithEndpointId(
    absl::string_view endpoint_id) {
  auto it = outgoing_share_target_map_.find(endpoint_id);
  if (it == outgoing_share_target_map_.end()) {
    return;
  }

  NL_VLOG(1) << __func__ << ": Removing (endpoint_id=" << it->first
             << ", share_target.id=" << it->second.id
             << ") from outgoing share target map";
  ShareTarget share_target = std::move(it->second);
  outgoing_share_target_map_.erase(it);

  auto info_it = outgoing_share_target_info_map_.find(share_target.id);
  if (info_it != outgoing_share_target_info_map_.end()) {
    outgoing_share_target_info_map_.erase(info_it);
  } else {
    NL_LOG(WARNING) << __func__ << ": share_target.id=" << it->second.id
                    << " not found in outgoing share target info map.";
    return;
  }

  for (ShareTargetDiscoveredCallback* discovery_callback :
       foreground_send_discovery_callbacks_.GetObservers()) {
    if (discovery_callback != nullptr) {
      discovery_callback->OnShareTargetLost(share_target);
    } else {
      NL_LOG(WARNING) << __func__
                      << "Foreground Discovery Callback is not exist";
    }
  }
  for (ShareTargetDiscoveredCallback* discovery_callback :
       background_send_discovery_callbacks_.GetObservers()) {
    if (discovery_callback != nullptr) {
      discovery_callback->OnShareTargetLost(share_target);
    } else {
      NL_LOG(WARNING) << __func__
                      << "Background Discovery Callback is not exist";
    }
  }

  NL_VLOG(1) << __func__ << ": Reported OnShareTargetLost";
}

void NearbySharingServiceImpl::OnTransferComplete() {
  bool was_sending_files = is_sending_files_;
  is_receiving_files_ = false;
  is_transferring_ = false;
  is_sending_files_ = false;

  // Cleanup ARC after send transfer completes since reading from file
  // descriptor(s) are done at this point even though there could be Nearby
  // Connection frames cached that are not yet sent to the remote device.
  if (was_sending_files && arc_transfer_cleanup_callback_) {
    arc_transfer_cleanup_callback_();
  }

  NL_VLOG(1) << __func__ << ": NearbySharing state change transfer finished";
  // Files transfer is done! Receivers can immediately cancel, but senders
  // should add a short delay to ensure the final in-flight packet(s) make
  // it to the remote device.
  RunOnNearbySharingServiceThreadDelayed(
      "transfer_done_delay",
      was_sending_files ? kInvalidateSurfaceStateDelayAfterTransferDone
                        : absl::Milliseconds(1),
      [&]() { InvalidateSurfaceState(); });
}

void NearbySharingServiceImpl::OnTransferStarted(bool is_incoming) {
  is_transferring_ = true;
  if (is_incoming) {
    is_receiving_files_ = true;
  } else {
    is_sending_files_ = true;
  }
  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::ReceivePayloads(
    ShareTarget share_target,
    std::function<void(StatusCodes status_codes)> status_codes_callback) {
  mutual_acceptance_timeout_alarm_->Stop();

  std::filesystem::path download_path =
      std::filesystem::u8path(settings_->GetCustomSavePath());

  // Register payload path for all valid file payloads.
  absl::flat_hash_map<int64_t, std::filesystem::path> valid_file_payloads;
  for (auto& file : share_target.file_attachments) {
    std::optional<int64_t> payload_id = GetAttachmentPayloadId(file.id());
    if (!payload_id) {
      NL_LOG(WARNING)
          << __func__
          << ": Failed to register payload path for attachment id - "
          << file.id();
      continue;
    }

    std::filesystem::path file_path =
        download_path / std::filesystem::u8path(file.file_name().cbegin(),
                                                file.file_name().cend());
    valid_file_payloads.emplace(file.id(), std::move(file_path));
  }

  auto aggregated_success = std::make_unique<bool>(true);

  if (valid_file_payloads.empty()) {
    OnPayloadPathsRegistered(share_target, std::move(aggregated_success),
                             std::move(status_codes_callback));
    return;
  }

  path_registration_status_.share_target = share_target;
  path_registration_status_.expected_count = valid_file_payloads.size();
  path_registration_status_.current_count = 0;
  path_registration_status_.status_codes_callback =
      std::move(status_codes_callback);
  path_registration_status_.status = true;

  for (const auto& payload : valid_file_payloads) {
    std::optional<int64_t> payload_id = GetAttachmentPayloadId(payload.first);
    NL_DCHECK(payload_id);

    file_handler_.GetUniquePath(
        payload.second,
        [&, attachment_id = payload.first,
         payload_id = *payload_id](std::filesystem::path unique_path) {
          OnUniquePathFetched(
              attachment_id, payload_id,
              [&](Status status) { OnPayloadPathRegistered(status); },
              unique_path);
        });
  }
}

NearbySharingService::StatusCodes NearbySharingServiceImpl::SendPayloads(
    const ShareTarget& share_target) {
  NL_VLOG(1) << __func__ << ": Preparing to send payloads to "
             << share_target.id;
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    NL_LOG(WARNING) << __func__
                    << ": Failed to send payload due to missing connection.";
    return StatusCodes::kOutOfOrderApiCall;
  }
  if (!info->transfer_update_callback()) {
    NL_LOG(WARNING)
        << __func__
        << ": Failed to send payload due to missing transfer update "
           "callback. Disconnecting.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingTransferUpdateCallback, share_target);
    return StatusCodes::kOutOfOrderApiCall;
  }

  // Log analytics event of sending attachment start.
  analytics_recorder_->NewSendAttachmentsStart(
      info->session_id(), share_target.GetAttachments(),
      /*transfer_position=*/GetConnectedShareTargetPos(share_target),
      /*concurrent_connections=*/GetConnectedShareTargetCount());

  info->transfer_update_callback()->OnTransferUpdate(
      share_target,
      TransferMetadataBuilder()
          .set_token(info->token())
          .set_status(TransferMetadata::Status::kAwaitingRemoteAcceptance)
          .build());

  if (!info->endpoint_id()) {
    NL_LOG(WARNING) << __func__
                    << ": Failed to send payload due to missing endpoint id.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingEndpointId, share_target);
    return StatusCodes::kOutOfOrderApiCall;
  }

  ReceiveConnectionResponse(share_target);
  return StatusCodes::kOk;
}

void NearbySharingServiceImpl::OnUniquePathFetched(
    int64_t attachment_id, int64_t payload_id,
    std::function<void(Status)> callback, std::filesystem::path file_path) {
  attachment_info_map_[attachment_id].file_path = file_path;
  nearby_connections_manager_->RegisterPayloadPath(payload_id, file_path,
                                                   std::move(callback));
}

void NearbySharingServiceImpl::OnPayloadPathRegistered(Status status) {
  if (status != Status::kSuccess) {
    path_registration_status_.status = false;
  }

  path_registration_status_.current_count += 1;
  if (path_registration_status_.current_count ==
      path_registration_status_.expected_count) {
    OnPayloadPathsRegistered(
        path_registration_status_.share_target,
        std::make_unique<bool>(path_registration_status_.status),
        std::move(path_registration_status_.status_codes_callback));
  }
}

void NearbySharingServiceImpl::OnPayloadPathsRegistered(
    const ShareTarget& share_target, std::unique_ptr<bool> aggregated_success,
    std::function<void(StatusCodes status_codes)> status_codes_callback) {
  NL_DCHECK(aggregated_success);
  if (!*aggregated_success) {
    NL_LOG(WARNING)
        << __func__
        << ": Not all payload paths could be registered successfully.";
    std::move(status_codes_callback)(StatusCodes::kError);
    return;
  }

  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    NL_LOG(WARNING) << __func__ << ": Accept invoked for unknown share target";
    std::move(status_codes_callback)(StatusCodes::kOutOfOrderApiCall);
    return;
  }
  NearbyConnection* connection = info->connection();

  if (!info->transfer_update_callback()) {
    NL_LOG(WARNING) << __func__
                    << ": Accept invoked for share target without transfer "
                       "update callback. Disconnecting.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingTransferUpdateCallback, share_target);
    std::move(status_codes_callback)(StatusCodes::kOutOfOrderApiCall);
    return;
  }

  // Log analytics event of starting to receive payloads.
  analytics_recorder_->NewReceiveAttachmentsStart(
      receiving_session_id_, share_target.GetAttachments());

  info->set_payload_tracker(std::make_shared<PayloadTracker>(
      context_, share_target, attachment_info_map_,
      [&](ShareTarget share_target, TransferMetadata transfer_metadata) {
        OnPayloadTransferUpdate(share_target, transfer_metadata);
      }));

  // Register status listener for all payloads.
  for (int64_t attachment_id : share_target.GetAttachmentIds()) {
    std::optional<int64_t> payload_id = GetAttachmentPayloadId(attachment_id);
    if (!payload_id) {
      NL_LOG(WARNING) << __func__
                      << ": Failed to retrieve payload for attachment id - "
                      << attachment_id;
      continue;
    }

    NL_VLOG(1) << __func__ << ": Started listening for progress on payload - "
               << *payload_id;

    nearby_connections_manager_->RegisterPayloadStatusListener(
        *payload_id, info->payload_tracker());

    NL_VLOG(1) << __func__ << ": Accepted incoming files from share target - "
               << share_target.id;
  }

  WriteResponseFrame(
      *connection,
      nearby::sharing::service::proto::ConnectionResponseFrame::ACCEPT);
  NL_VLOG(1) << __func__ << ": Successfully wrote response frame";

  info->transfer_update_callback()->OnTransferUpdate(
      share_target,
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kAwaitingRemoteAcceptance)
          .set_token(info->token())
          .build());

  std::optional<std::string> endpoint_id = info->endpoint_id();
  if (endpoint_id.has_value()) {
    if (share_target.GetTotalAttachmentsSize() >=
        kAttachmentsSizeThresholdOverHighQualityMedium) {
      // Upgrade bandwidth regardless of advertising visibility because either
      // the system or the user has verified the sender's identity; the
      // stable identifiers potentially exposed by performing a bandwidth
      // upgrade are no longer a concern.
      NL_LOG(INFO) << __func__ << ": Upgrade bandwidth when receiving accept.";
      nearby_connections_manager_->UpgradeBandwidth(*endpoint_id);
    }
  } else {
    NL_LOG(WARNING) << __func__
                    << ": Failed to initiate bandwidth upgrade. No endpoint_id "
                       "found for target - "
                    << share_target.id;
    std::move(status_codes_callback)(StatusCodes::kOutOfOrderApiCall);
    return;
  }

  std::move(status_codes_callback)(StatusCodes::kOk);
}

void NearbySharingServiceImpl::OnOutgoingConnection(
    const ShareTarget& share_target, absl::Time connect_start_time,
    NearbyConnection* connection) {
  OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
  bool success = info && info->endpoint_id() && connection;

  if (!success) {
    NL_LOG(WARNING) << __func__
                    << ": Failed to initiate connection to share target "
                    << share_target.id;
    TransferMetadata::Status transfer_status =
        TransferMetadata::Status::kFailedToInitiateOutgoingConnection;
    if (info != nullptr &&
        info->connection_layer_status() == Status::kTimeout) {
      transfer_status = TransferMetadata::Status::kTimedOut;
      info->set_connection_layer_status(Status::kUnknown);
    }
    AbortAndCloseConnectionIfNecessary(transfer_status, share_target);
    return;
  }

  info->set_connection(connection);

  // Log analytics event of establishing connection.
  analytics_recorder_->NewEstablishConnection(
      info->session_id(), EstablishConnectionStatus::CONNECTION_STATUS_SUCCESS,
      share_target,
      /*transfer_position=*/GetConnectedShareTargetPos(share_target),
      /*concurrent_connections=*/GetConnectedShareTargetCount(),
      info->connection_start_time().has_value()
          ? absl::ToInt64Milliseconds((context_->GetClock()->Now() -
                                       *(info->connection_start_time())))
          : 0,
      std::nullopt);

  connection->SetDisconnectionListener([&, share_target]() {
    RunOnNearbySharingServiceThread(
        "disconnection_listener", [&, share_target]() {
          OnOutgoingConnectionDisconnected(share_target);
        });
  });

  std::optional<std::string> four_digit_token = TokenToFourDigitString(
      nearby_connections_manager_->GetRawAuthenticationToken(
          *info->endpoint_id()));

  RunPairedKeyVerification(
      share_target, *info->endpoint_id(),
      [&, share_target, four_digit_token = std::move(four_digit_token)](
          PairedKeyVerificationRunner::PairedKeyVerificationResult result,
          OSType remote_os_type) {
        OnOutgoingConnectionKeyVerificationDone(share_target, four_digit_token,
                                                result, remote_os_type);
      });
}

void NearbySharingServiceImpl::SendIntroduction(
    const ShareTarget& share_target,
    std::optional<std::string> four_digit_token) {
  // We successfully connected! Now lets build up Payloads for all the files we
  // want to send them. We won't send any just yet, but we'll send the Payload
  // IDs in our introduction frame so that they know what to expect if they
  // accept.
  NL_VLOG(1) << __func__ << ": Preparing to send introduction to "
             << share_target.id;

  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    NL_LOG(WARNING) << __func__ << ": No NearbyConnection tied to "
                    << share_target.id;
    return;
  }

  // Log analytics event of sending introduction.
  analytics_recorder_->NewSendIntroduction(
      info->session_id(), share_target,
      /*transfer_position=*/GetConnectedShareTargetPos(share_target),
      /*concurrent_connections=*/GetConnectedShareTargetCount(),
      info->os_type());

  NearbyConnection* connection = info->connection();

  if (!info->transfer_update_callback()) {
    NL_LOG(WARNING) << __func__
                    << ": No transfer update callback, disconnecting.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingTransferUpdateCallback, share_target);
    return;
  }

  if (foreground_send_transfer_callbacks_.empty() &&
      background_send_transfer_callbacks_.empty()) {
    NL_LOG(WARNING) << __func__ << ": No transfer callbacks, disconnecting.";
    connection->Close();
    return;
  }

  // Build the introduction.
  auto introduction =
      std::make_unique<nearby::sharing::service::proto::IntroductionFrame>();
  introduction->set_start_transfer(true);
  NL_VLOG(1) << __func__ << ": Sending attachments to " << share_target.id;

  // Write introduction of file payloads.
  for (const auto& file : share_target.file_attachments) {
    std::optional<int64_t> payload_id = GetAttachmentPayloadId(file.id());
    if (!payload_id) {
      NL_VLOG(1) << __func__ << ": Skipping unknown file attachment";
      continue;
    }
    auto* file_metadata = introduction->add_file_metadata();
    file_metadata->set_id(file.id());
    file_metadata->set_name(absl::StrCat(file.file_name()));
    file_metadata->set_payload_id(*payload_id);
    file_metadata->set_type(file.type());
    file_metadata->set_mime_type(absl::StrCat(file.mime_type()));
    file_metadata->set_size(file.size());
  }

  // Write introduction of text payloads.
  for (const auto& text : share_target.text_attachments) {
    std::optional<int64_t> payload_id = GetAttachmentPayloadId(text.id());
    if (!payload_id) {
      NL_VLOG(1) << __func__ << ": Skipping unknown text attachment";
      continue;
    }
    auto* text_metadata = introduction->add_text_metadata();
    text_metadata->set_id(text.id());
    text_metadata->set_text_title(std::string(text.text_title()));
    text_metadata->set_type(text.type());
    text_metadata->set_size(text.size());
    text_metadata->set_payload_id(*payload_id);
  }

  // Write introduction of Wi-Fi credentials payloads.
  for (const auto& wifi_credentials :
       share_target.wifi_credentials_attachments) {
    std::optional<int64_t> payload_id =
        GetAttachmentPayloadId(wifi_credentials.id());
    if (!payload_id) {
      NL_VLOG(1) << __func__
                 << ": Skipping unknown WiFi credentials attachment";
      continue;
    }
    auto* wifi_credentials_metadata =
        introduction->add_wifi_credentials_metadata();
    wifi_credentials_metadata->set_id(wifi_credentials.id());
    wifi_credentials_metadata->set_ssid(std::string(wifi_credentials.ssid()));
    wifi_credentials_metadata->set_security_type(
        wifi_credentials.security_type());
    wifi_credentials_metadata->set_payload_id(*payload_id);
  }

  if (introduction->file_metadata_size() == 0 &&
      introduction->text_metadata_size() == 0 &&
      introduction->wifi_credentials_metadata_size() == 0) {
    NL_LOG(WARNING) << __func__
                    << ": No payloads tied to transfer, disconnecting.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingPayloads, share_target);
    return;
  }

  // Write the introduction to the remote device.
  nearby::sharing::service::proto::Frame frame;
  frame.set_version(nearby::sharing::service::proto::Frame::V1);
  nearby::sharing::service::proto::V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(nearby::sharing::service::proto::V1Frame::INTRODUCTION);
  v1_frame->set_allocated_introduction(introduction.release());

  std::vector<uint8_t> data(frame.ByteSizeLong());
  frame.SerializeToArray(data.data(), frame.ByteSizeLong());
  connection->Write(std::move(data));

  // We've successfully written the introduction, so we now have to wait for the
  // remote side to accept.
  NL_VLOG(1) << __func__ << ": Successfully wrote the introduction frame";

  mutual_acceptance_timeout_alarm_->Stop();
  mutual_acceptance_timeout_alarm_->Start(
      absl::ToInt64Milliseconds(kReadResponseFrameTimeout), 0,
      [&, share_target]() { OnOutgoingMutualAcceptanceTimeout(share_target); });

  info->transfer_update_callback()->OnTransferUpdate(
      share_target,
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kAwaitingLocalConfirmation)
          .set_token(four_digit_token)
          .build());
}

void NearbySharingServiceImpl::CreatePayloads(
    ShareTarget share_target, std::function<void(ShareTarget, bool)> callback) {
  OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
  if (!info || !share_target.has_attachments()) {
    std::move(callback)(std::move(share_target), /*success=*/false);
    return;
  }

  if (!info->file_payloads().empty() || !info->text_payloads().empty() ||
      !info->wifi_credentials_payloads().empty()) {
    // We may have already created the payloads in the case of retry, so we can
    // skip this step.
    std::move(callback)(std::move(share_target), /*success=*/false);
    return;
  }

  info->set_text_payloads(CreateTextPayloads(share_target.text_attachments));
  info->set_wifi_credentials_payloads(
      CreateWifiCredentialsPayloads(share_target.wifi_credentials_attachments));
  if (share_target.file_attachments.empty()) {
    std::move(callback)(std::move(share_target), /*success=*/true);
    return;
  }

  std::vector<std::filesystem::path> file_paths;
  for (const FileAttachment& attachment : share_target.file_attachments) {
    if (!attachment.file_path()) {
      NL_LOG(WARNING) << __func__ << ": Got file attachment without path";
      std::move(callback)(std::move(share_target), /*success=*/false);
      return;
    }
    file_paths.push_back(*attachment.file_path());
  }

  file_handler_.OpenFiles(
      std::move(file_paths),
      [&, share_target = std::move(share_target),
       callback = std::move(callback)](
          std::vector<NearbyFileHandler::FileInfo> file_infos) {
        OnOpenFiles(std::move(share_target), std::move(callback),
                    std::move(file_infos));
      });
}

void NearbySharingServiceImpl::OnCreatePayloads(
    std::vector<uint8_t> endpoint_info, ShareTarget share_target,
    bool success) {
  OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
  bool has_payloads = info && (!info->text_payloads().empty() ||
                               !info->file_payloads().empty());
  if (!success || !has_payloads || !info->endpoint_id()) {
    NL_LOG(WARNING) << __func__
                    << ": Failed to send file to remote ShareTarget. Failed to "
                       "create payloads.";
    if (info && info->transfer_update_callback()) {
      info->transfer_update_callback()->OnTransferUpdate(
          share_target,
          TransferMetadataBuilder()
              .set_status(TransferMetadata::Status::kMediaUnavailable)
              .build());
    }
    return;
  }

  std::optional<std::vector<uint8_t>> bluetooth_mac_address =
      GetBluetoothMacAddressForShareTarget(share_target);

  // For metrics.
  all_cancelled_share_target_ids_.clear();

  info->set_connection_start_time(context_->GetClock()->Now());

  nearby_connections_manager_->Connect(
      std::move(endpoint_info), *info->endpoint_id(),
      std::move(bluetooth_mac_address), settings_->GetDataUsage(),
      GetTransportType(share_target),
      [&, share_target, info](NearbyConnection* connection, Status status) {
        // Log analytics event of new connection.
        info->set_connection_layer_status(status);
        if (connection == nullptr) {
          analytics_recorder_->NewEstablishConnection(
              info->session_id(),
              EstablishConnectionStatus::CONNECTION_STATUS_FAILURE,
              share_target,
              /*transfer_position=*/GetConnectedShareTargetPos(share_target),
              /*concurrent_connections=*/GetConnectedShareTargetCount(),
              info->connection_start_time().has_value()
                  ? absl::ToInt64Milliseconds(context_->GetClock()->Now() -
                                              *(info->connection_start_time()))
                  : 0,
              std::nullopt);
        }

        OnOutgoingConnection(share_target, context_->GetClock()->Now(),
                             connection);
      });
}

void NearbySharingServiceImpl::OnOpenFiles(
    ShareTarget share_target, std::function<void(ShareTarget, bool)> callback,
    std::vector<NearbyFileHandler::FileInfo> files) {
  OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
  if (!info || files.size() != share_target.file_attachments.size()) {
    std::move(callback)(std::move(share_target), /*success=*/false);
    return;
  }

  std::vector<Payload> payloads;
  payloads.reserve(files.size());

  for (size_t i = 0; i < files.size(); ++i) {
    FileAttachment& attachment = share_target.file_attachments[i];
    attachment.set_size(files[i].size);
    InputFile input_file;
    input_file.path = files[i].file_path;
    Payload payload(input_file, attachment.parent_folder());
    payload.content.file_payload.size = files[i].size;
    SetAttachmentPayloadId(attachment, payload.id);
    payloads.push_back(std::move(payload));
  }

  info->set_file_payloads(std::move(payloads));
  std::move(callback)(std::move(share_target), /*success=*/true);
}

std::vector<Payload> NearbySharingServiceImpl::CreateTextPayloads(
    const std::vector<TextAttachment>& attachments) {
  std::vector<Payload> payloads;
  payloads.reserve(attachments.size());
  for (const TextAttachment& attachment : attachments) {
    absl::string_view body = attachment.text_body();
    std::vector<uint8_t> bytes(body.begin(), body.end());

    Payload payload{bytes};
    SetAttachmentPayloadId(attachment, payload.id);
    payloads.push_back(std::move(payload));
  }
  return payloads;
}

std::vector<Payload> NearbySharingServiceImpl::CreateWifiCredentialsPayloads(
    const std::vector<WifiCredentialsAttachment>& attachments) {
  std::vector<Payload> payloads;
  payloads.reserve(attachments.size());
  for (const WifiCredentialsAttachment& attachment : attachments) {
    nearby::sharing::service::proto::WifiCredentials wifi_credentials;
    wifi_credentials.set_password(std::string(attachment.password()));
    wifi_credentials.set_hidden_ssid(attachment.is_hidden());

    std::vector<uint8_t> bytes(wifi_credentials.ByteSizeLong());
    wifi_credentials.SerializeToArray(bytes.data(),
                                      wifi_credentials.ByteSizeLong());

    Payload payload{bytes};
    SetAttachmentPayloadId(attachment, payload.id);
    payloads.push_back(std::move(payload));
  }
  return payloads;
}

void NearbySharingServiceImpl::WriteResponseFrame(
    NearbyConnection& connection,
    nearby::sharing::service::proto::ConnectionResponseFrame::Status
        response_status) {
  nearby::sharing::service::proto::Frame frame;
  frame.set_version(nearby::sharing::service::proto::Frame::V1);
  nearby::sharing::service::proto::V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(nearby::sharing::service::proto::V1Frame::RESPONSE);
  v1_frame->mutable_connection_response()->set_status(response_status);

  std::vector<uint8_t> data(frame.ByteSizeLong());
  frame.SerializeToArray(data.data(), frame.ByteSizeLong());

  connection.Write(std::move(data));
}

void NearbySharingServiceImpl::WriteCancelFrame(NearbyConnection& connection) {
  NL_LOG(INFO) << __func__ << ": Writing cancel frame.";

  nearby::sharing::service::proto::Frame frame;
  frame.set_version(nearby::sharing::service::proto::Frame::V1);
  nearby::sharing::service::proto::V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(nearby::sharing::service::proto::V1Frame::CANCEL);

  std::vector<uint8_t> data(frame.ByteSizeLong());
  frame.SerializeToArray(data.data(), frame.ByteSizeLong());

  connection.Write(std::move(data));
}

void NearbySharingServiceImpl::WriteProgressUpdateFrame(
    NearbyConnection& connection, std::optional<bool> start_transfer,
    std::optional<float> progress) {
  NL_LOG(INFO) << __func__ << ": Writing progress update frame. start_transfer="
               << (start_transfer.has_value() ? *start_transfer : false)
               << ", progress=" << (progress.has_value() ? *progress : 0.0);
  nearby::sharing::service::proto::Frame frame;
  frame.set_version(nearby::sharing::service::proto::Frame::V1);
  nearby::sharing::service::proto::V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(nearby::sharing::service::proto::V1Frame::PROGRESS_UPDATE);
  nearby::sharing::service::proto::ProgressUpdateFrame* progress_frame =
      v1_frame->mutable_progress_update();
  if (start_transfer.has_value()) {
    progress_frame->set_start_transfer(*start_transfer);
  }
  if (progress.has_value()) {
    progress_frame->set_progress(*progress);
  }

  std::vector<uint8_t> data(frame.ByteSizeLong());
  frame.SerializeToArray(data.data(), frame.ByteSizeLong());

  connection.Write(std::move(data));
}

void NearbySharingServiceImpl::Fail(const ShareTarget& share_target,
                                    TransferMetadata::Status status) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    NL_LOG(WARNING) << __func__ << ": Fail invoked for unknown share target.";
    return;
  }
  NearbyConnection* connection = info->connection();

  RunOnNearbySharingServiceThreadDelayed(
      "incoming_rejection_delay", kIncomingRejectionDelay,
      [&, share_target]() { CloseConnection(share_target); });

  connection->SetDisconnectionListener([&, share_target]() {
    RunOnNearbySharingServiceThread(
        "disconnection_listener",
        [&, share_target]() { RefreshUIOnDisconnection(share_target); });
  });

  // Send response to remote device.
  nearby::sharing::service::proto::ConnectionResponseFrame::Status
      response_status;
  switch (status) {
    case TransferMetadata::Status::kNotEnoughSpace:
      response_status = nearby::sharing::service::proto::
          ConnectionResponseFrame::NOT_ENOUGH_SPACE;
      break;

    case TransferMetadata::Status::kUnsupportedAttachmentType:
      response_status = nearby::sharing::service::proto::
          ConnectionResponseFrame::UNSUPPORTED_ATTACHMENT_TYPE;
      break;

    case TransferMetadata::Status::kTimedOut:
      response_status =
          nearby::sharing::service::proto::ConnectionResponseFrame::TIMED_OUT;
      break;

    default:
      response_status =
          nearby::sharing::service::proto::ConnectionResponseFrame::UNKNOWN;
      break;
  }

  WriteResponseFrame(*connection, response_status);

  if (info->transfer_update_callback()) {
    info->transfer_update_callback()->OnTransferUpdate(
        share_target, TransferMetadataBuilder().set_status(status).build());
  }
}

void NearbySharingServiceImpl::OnIncomingAdvertisementDecoded(
    absl::string_view endpoint_id, ShareTarget placeholder_share_target,
    std::unique_ptr<Advertisement> advertisement) {
  NearbyConnection* connection = GetConnection(placeholder_share_target);
  if (!connection) {
    NL_LOG(WARNING) << __func__ << ": Invalid connection for endpoint id - "
                    << endpoint_id;
    return;
  }

  if (!advertisement) {
    NL_LOG(WARNING) << __func__
                    << ": Failed to parse incoming connection from endpoint - "
                    << endpoint_id << ", disconnecting.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kDecodeAdvertisementFailed,
        placeholder_share_target);
    return;
  }

  NearbyShareEncryptedMetadataKey encrypted_metadata_key(
      advertisement->salt(), advertisement->encrypted_metadata_key());

  // Because we cannot apply std::move on Advertisement in lambda, copy to pass
  // data to lambda.
  GetCertificateManager()->GetDecryptedPublicCertificate(
      std::move(encrypted_metadata_key),
      [this, endpoint_id, advertisement_copy = *advertisement,
       placeholder_share_target = std::move(placeholder_share_target)](
          std::optional<NearbyShareDecryptedPublicCertificate>
              decrypted_public_certificate) {
        std::unique_ptr<Advertisement> advertisement =
            Advertisement::NewInstance(
                advertisement_copy.salt(),
                advertisement_copy.encrypted_metadata_key(),
                advertisement_copy.device_type(),
                advertisement_copy.device_name());
        OnIncomingDecryptedCertificate(endpoint_id, std::move(advertisement),
                                       std::move(placeholder_share_target),
                                       decrypted_public_certificate);
      });
}

void NearbySharingServiceImpl::OnIncomingTransferUpdate(
    const ShareTarget& share_target, const TransferMetadata& metadata) {
  // kInProgress status is logged extensively elsewhere so avoid the spam.
  if (metadata.status() != TransferMetadata::Status::kInProgress) {
    NL_VLOG(1) << __func__ << ": Nearby Share service: "
               << "Incoming transfer update for share target with ID "
               << share_target.id << ": "
               << TransferMetadata::StatusToString(metadata.status());
  }
  if (metadata.status() != TransferMetadata::Status::kCancelled &&
      metadata.status() != TransferMetadata::Status::kRejected) {
    last_incoming_metadata_ =
        std::make_pair(share_target, TransferMetadataBuilder::Clone(metadata)
                                         .set_is_original(false)
                                         .build());
  } else {
    last_incoming_metadata_ = std::nullopt;
  }

  if (metadata.is_final_status()) {
    // Log analytics event of receiving attachment end.
    int64_t received_bytes =
        share_target.GetTotalAttachmentsSize() * metadata.progress() / 100;
    AttachmentTransmissionStatus transmission_status =
        ConvertToTransmissionStatus(metadata.status());

    analytics_recorder_->NewReceiveAttachmentsEnd(
        receiving_session_id_, received_bytes, transmission_status,
        /* referrer_package=*/std::nullopt);

    OnTransferComplete();
    if (metadata.status() != TransferMetadata::Status::kComplete) {
      // For any type of failure, lets make sure any pending files get cleaned
      // up.
      RemoveIncomingPayloads(share_target);
    }
  } else if (metadata.status() ==
             TransferMetadata::Status::kAwaitingLocalConfirmation) {
    OnTransferStarted(/*is_incoming=*/true);
  }

  ObserverList<TransferUpdateCallback>& transfer_callbacks =
      foreground_receive_callbacks_.empty() ? background_receive_callbacks_
                                            : foreground_receive_callbacks_;

  for (TransferUpdateCallback* callback : transfer_callbacks.GetObservers()) {
    callback->OnTransferUpdate(share_target, metadata);
  }
}

void NearbySharingServiceImpl::OnOutgoingTransferUpdate(
    const ShareTarget& share_target, const TransferMetadata& metadata) {
  // kInProgress status is logged extensively elsewhere so avoid the spam.
  if (metadata.status() != TransferMetadata::Status::kInProgress) {
    NL_VLOG(1) << __func__ << ": Nearby Share service: "
               << "Outgoing transfer update for share target with ID "
               << share_target.id << ": "
               << TransferMetadata::StatusToString(metadata.status());
  }

  OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
  if (metadata.is_final_status()) {
    // Log analytics event of sending attachment end.
    int64_t sent_bytes =
        share_target.GetTotalAttachmentsSize() * metadata.progress() / 100;
    AttachmentTransmissionStatus transmission_status =
        ConvertToTransmissionStatus(metadata.status());

    if (info == nullptr) {
      // The situation may happen when user cancel connection during
      // establishing connection.
      NL_LOG(INFO) << "No share target info is created for share_target:"
                   << share_target.device_name;
    } else {
      analytics_recorder_->NewSendAttachmentsEnd(
          info->session_id(), sent_bytes, share_target, transmission_status,
          /*transfer_position=*/GetConnectedShareTargetPos(share_target),
          /*concurrent_connections=*/GetConnectedShareTargetCount(),
          /*duration_millis=*/info->connection_start_time().has_value()
              ? absl::ToInt64Milliseconds(context_->GetClock()->Now() -
                                          *(info->connection_start_time()))
              : 0,
          /*referrer_package=*/std::nullopt,
          ConvertToConnectionLayerStatus(info->connection_layer_status()),
          info->os_type());
    }
    is_connecting_ = false;
    OnTransferComplete();
  } else if (metadata.status() == TransferMetadata::Status::kMediaDownloading ||
             metadata.status() ==
                 TransferMetadata::Status::kAwaitingLocalConfirmation) {
    is_connecting_ = false;
    OnTransferStarted(/*is_incoming=*/false);
  }

  bool has_foreground_send_surface =
      !foreground_send_transfer_callbacks_.empty();
  ObserverList<TransferUpdateCallback>& transfer_callbacks =
      has_foreground_send_surface ? foreground_send_transfer_callbacks_
                                  : background_send_transfer_callbacks_;
  if (info) {
    // only call transfer update when having share target info.
    for (TransferUpdateCallback* callback : transfer_callbacks.GetObservers()) {
      callback->OnTransferUpdate(share_target, metadata);
    }

    // check whether need to send next payload.
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_sharing_feature::
                kEnableTransferCancellationOptimization)) {
      if (metadata.in_progress_attachment_transferred_bytes().has_value() &&
          metadata.in_progress_attachment_total_bytes().has_value() &&
          *metadata.in_progress_attachment_transferred_bytes() ==
              *metadata.in_progress_attachment_total_bytes()) {
        std::optional<Payload> payload = info->ExtractNextPayload();
        if (payload.has_value()) {
          NL_LOG(INFO) << __func__ << ": Send  payload " << payload->id;
          nearby_connections_manager_->Send(*info->endpoint_id(),
                                            std::make_unique<Payload>(*payload),
                                            info->payload_tracker());
        } else {
          NL_LOG(WARNING) << __func__ << ": There is no paylaods to send.";
        }
      }
    }
  }

  if (has_foreground_send_surface && metadata.is_final_status()) {
    last_outgoing_metadata_ = std::nullopt;
  } else {
    last_outgoing_metadata_ =
        std::make_pair(share_target, TransferMetadataBuilder::Clone(metadata)
                                         .set_is_original(false)
                                         .build());
  }
}

void NearbySharingServiceImpl::CloseConnection(
    const ShareTarget& share_target) {
  NearbyConnection* connection = GetConnection(share_target);
  if (!connection) {
    NL_LOG(WARNING) << __func__ << ": Invalid connection for target - "
                    << share_target.id;
    return;
  }
  connection->Close();
}

void NearbySharingServiceImpl::OnIncomingDecryptedCertificate(
    absl::string_view endpoint_id, std::unique_ptr<Advertisement> advertisement,
    ShareTarget placeholder_share_target,
    std::optional<NearbyShareDecryptedPublicCertificate> certificate) {
  NearbyConnection* connection = GetConnection(placeholder_share_target);
  if (!connection) {
    NL_VLOG(1) << __func__ << ": Invalid connection for endpoint id - "
               << endpoint_id;
    return;
  }

  // Remove placeholder share target since we are creating the actual share
  // target below.
  incoming_share_target_info_map_.erase(placeholder_share_target.id);

  std::optional<ShareTarget> share_target =
      CreateShareTarget(endpoint_id, std::move(advertisement),
                        std::move(certificate), /*is_incoming=*/true);

  if (!share_target) {
    NL_LOG(WARNING) << __func__
                    << ": Failed to convert advertisement to share target for "
                       "incoming connection, disconnecting";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingShareTarget,
        placeholder_share_target);
    return;
  }

  NL_VLOG(1) << __func__ << ": Received incoming connection from "
             << share_target->id;

  ShareTargetInfo* share_target_info = GetShareTargetInfo(*share_target);
  NL_DCHECK(share_target_info);
  share_target_info->set_connection(connection);

  share_target_info->set_transfer_update_callback(
      std::make_unique<TransferUpdateDecorator>(
          [&](const ShareTarget& share_target,
              const TransferMetadata& transfer_metadata) {
            OnIncomingTransferUpdate(share_target, transfer_metadata);
          }));

  connection->SetDisconnectionListener([&, share_target = *share_target]() {
    RunOnNearbySharingServiceThread(
        "disconnection_listener",
        [&, share_target]() { RefreshUIOnDisconnection(share_target); });
  });

  std::optional<std::string> four_digit_token = TokenToFourDigitString(
      nearby_connections_manager_->GetRawAuthenticationToken(endpoint_id));

  RunPairedKeyVerification(
      *share_target, endpoint_id,
      [&, share_target = *share_target,
       four_digit_token = std::move(four_digit_token)](
          PairedKeyVerificationRunner::PairedKeyVerificationResult
              verification_result,
          OSType remote_os_type) {
        OnIncomingConnectionKeyVerificationDone(share_target, four_digit_token,
                                                verification_result,
                                                remote_os_type);
      });
}

void NearbySharingServiceImpl::RunPairedKeyVerification(
    const ShareTarget& share_target, absl::string_view endpoint_id,
    std::function<void(PairedKeyVerificationRunner::PairedKeyVerificationResult,
                       OSType)>
        callback) {
  std::optional<std::vector<uint8_t>> token =
      nearby_connections_manager_->GetRawAuthenticationToken(endpoint_id);
  if (!token) {
    NL_VLOG(1) << __func__
               << ": Failed to read authentication token from endpoint - "
               << endpoint_id;
    std::move(callback)(
        PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail,
        OSType::UNKNOWN_OS_TYPE);
    return;
  }

  ShareTargetInfo* share_target_info = GetShareTargetInfo(share_target);
  NL_DCHECK(share_target_info);

  share_target_info->set_frames_reader(std::make_shared<IncomingFramesReader>(
      context_, decoder_, share_target_info->connection()));

  bool restrict_to_contacts = share_target.is_incoming &&
                              settings_->GetVisibility() !=
                                  DeviceVisibility::DEVICE_VISIBILITY_EVERYONE;
  bool self_share_feature_enabled = NearbyFlags::GetInstance().GetBoolFlag(
      config_package_nearby::nearby_sharing_feature::kEnableSelfShare);
  share_target_info->set_key_verification_runner(
      std::make_shared<PairedKeyVerificationRunner>(
          context_->GetClock(), device_info_, GetSettings(),
          self_share_feature_enabled, share_target, endpoint_id, *token,
          share_target_info->connection(), share_target_info->certificate(),
          GetCertificateManager(), restrict_to_contacts,
          share_target_info->frames_reader(), kReadFramesTimeout));
  share_target_info->key_verification_runner()->Run(std::move(callback));
}

void NearbySharingServiceImpl::OnIncomingConnectionKeyVerificationDone(
    ShareTarget share_target, std::optional<std::string> four_digit_token,
    PairedKeyVerificationRunner::PairedKeyVerificationResult result,
    OSType share_target_os_type) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection() || !info->endpoint_id()) {
    NL_VLOG(1) << __func__ << ": Invalid connection or endpoint id";
    return;
  }

  info->set_os_type(share_target_os_type);

  switch (result) {
    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail:
      NL_VLOG(1) << __func__ << ": Paired key handshake failed for target "
                 << share_target.id << ". Disconnecting.";
      AbortAndCloseConnectionIfNecessary(
          TransferMetadata::Status::kPairedKeyVerificationFailed, share_target);
      return;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess:
      NL_VLOG(1) << __func__ << ": Paired key handshake succeeded for target - "
                 << share_target.id;
      ReceiveIntroduction(share_target, /*four_digit_token=*/std::nullopt);
      break;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable:
      NL_VLOG(1) << __func__
                 << ": Unable to verify paired key encryption when "
                    "receiving connection from target - "
                 << share_target.id;
      if (four_digit_token) info->set_token(*four_digit_token);

      ReceiveIntroduction(share_target, std::move(four_digit_token));
      break;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnknown:
      NL_VLOG(1) << __func__
                 << ": Unknown PairedKeyVerificationResult for target "
                 << share_target.id << ". Disconnecting.";
      AbortAndCloseConnectionIfNecessary(
          TransferMetadata::Status::kPairedKeyVerificationFailed, share_target);
      break;
  }
}

void NearbySharingServiceImpl::OnOutgoingConnectionKeyVerificationDone(
    const ShareTarget& share_target,
    std::optional<std::string> four_digit_token,
    PairedKeyVerificationRunner::PairedKeyVerificationResult result,
    OSType share_target_os_type) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    return;
  }

  if (!info->transfer_update_callback()) {
    NL_VLOG(1) << __func__ << ": No transfer update callback. Disconnecting.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingTransferUpdateCallback, share_target);
    return;
  }

  info->set_os_type(share_target_os_type);

  switch (result) {
    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail:
      NL_VLOG(1) << __func__ << ": Paired key handshake failed for target "
                 << share_target.id << ". Disconnecting.";
      AbortAndCloseConnectionIfNecessary(
          TransferMetadata::Status::kPairedKeyVerificationFailed, share_target);
      return;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess:
      NL_VLOG(1) << __func__ << ": Paired key handshake succeeded for target - "
                 << share_target.id;
      SendIntroduction(share_target, /*four_digit_token=*/std::nullopt);
      SendPayloads(share_target);
      return;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable:
      NL_VLOG(1) << __func__
                 << ": Unable to verify paired key encryption when "
                    "initiating connection to target - "
                 << share_target.id;

      if (four_digit_token) {
        info->set_token(*four_digit_token);
      }

      if (NearbyFlags::GetInstance().GetBoolFlag(
              config_package_nearby::nearby_sharing_feature::
                  kSenderSkipsConfirmation)) {
        NL_VLOG(1) << __func__
                   << ": Sender-side verification is disabled. Skipping "
                      "token comparison with "
                   << share_target.id;
        SendIntroduction(share_target, /*four_digit_token=*/std::nullopt);
        SendPayloads(share_target);
      } else {
        SendIntroduction(share_target, std::move(four_digit_token));
      }
      return;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnknown:
      NL_VLOG(1) << __func__
                 << ": Unknown PairedKeyVerificationResult for target "
                 << share_target.id << ". Disconnecting.";
      AbortAndCloseConnectionIfNecessary(
          TransferMetadata::Status::kPairedKeyVerificationFailed, share_target);
      break;
  }
}

void NearbySharingServiceImpl::RefreshUIOnDisconnection(
    ShareTarget share_target) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (info && info->transfer_update_callback()) {
    info->transfer_update_callback()->OnTransferUpdate(
        share_target,
        TransferMetadataBuilder()
            .set_status(
                TransferMetadata::Status::kAwaitingRemoteAcceptanceFailed)
            .build());
  }

  UnregisterShareTarget(share_target);
}

void NearbySharingServiceImpl::ReceiveIntroduction(
    ShareTarget share_target, std::optional<std::string> four_digit_token) {
  NL_LOG(INFO) << __func__ << ": Receiving introduction from "
               << share_target.id;
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  NL_DCHECK(info && info->connection());

  info->frames_reader()->ReadFrame(
      nearby::sharing::service::proto::V1Frame::INTRODUCTION,
      [&, share_target = std::move(share_target),
       four_digit_token = std::move(four_digit_token)](
          std::optional<nearby::sharing::service::proto::V1Frame> frame) {
        OnReceivedIntroduction(std::move(share_target),
                               std::move(four_digit_token), std::move(frame));
      },
      kReadFramesTimeout);
}

void NearbySharingServiceImpl::OnReceivedIntroduction(
    ShareTarget share_target, std::optional<std::string> four_digit_token,
    std::optional<nearby::sharing::service::proto::V1Frame> frame) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    NL_LOG(WARNING)
        << __func__
        << ": Ignore received introduction, due to no connection established.";
    return;
  }

  if (!frame.has_value()) {
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kInvalidIntroductionFrame, share_target);
    NL_LOG(WARNING) << __func__ << ": Invalid introduction frame";
    return;
  }

  NL_LOG(INFO) << __func__ << ": Successfully read the introduction frame.";

  int64_t file_size_sum = 0;

  nearby::sharing::service::proto::IntroductionFrame introduction_frame =
      std::move(frame->introduction());

  for (const auto& file : introduction_frame.file_metadata()) {
    if (file.size() <= 0) {
      Fail(share_target, TransferMetadata::Status::kUnsupportedAttachmentType);
      NL_LOG(WARNING)
          << __func__
          << ": Ignore introduction, due to invalid attachment size";
      return;
    }

    NL_VLOG(1) << __func__ << ": Found file attachment: id=" << file.id()
               << ", type= " << file.type() << ", size=" << file.size()
               << ", payload_id=" << file.payload_id()
               << ", parent_folder=" << file.parent_folder()
               << ", mime_type=" << file.mime_type();
    FileAttachment attachment(file.id(), file.size(), file.name(),
                              file.mime_type(), file.type(),
                              file.parent_folder());
    SetAttachmentPayloadId(attachment, file.payload_id());
    share_target.file_attachments.push_back(std::move(attachment));

    file_size_sum += file.size();
    if (file_size_sum < 0) {
      Fail(share_target, TransferMetadata::Status::kNotEnoughSpace);
      NL_LOG(WARNING) << __func__
                      << ": Ignoring introduction, total file size overflowed "
                         "64 bit integer.";
      return;
    }
  }

  for (const auto& text : introduction_frame.text_metadata()) {
    if (text.size() <= 0) {
      Fail(share_target, TransferMetadata::Status::kUnsupportedAttachmentType);
      NL_LOG(WARNING)
          << __func__
          << ": Ignore introduction, due to invalid attachment size";
      return;
    }

    NL_VLOG(1) << __func__ << ": Found text attachment: id=" << text.id()
               << ", type= " << text.type() << ", size=" << text.size()
               << ", payload_id=" << text.payload_id();
    TextAttachment attachment(text.id(), text.type(), text.text_title(),
                              text.size());
    SetAttachmentPayloadId(attachment, text.payload_id());
    share_target.text_attachments.push_back(std::move(attachment));
  }

  if (kSupportReceivingWifiCredentials) {
    for (const auto& wifi_credentials :
         introduction_frame.wifi_credentials_metadata()) {
      NL_VLOG(1) << __func__ << ": Found WiFi credentials attachment: id="
                 << wifi_credentials.id()
                 << ", ssid= " << wifi_credentials.ssid()
                 << ", payload_id=" << wifi_credentials.payload_id();
      WifiCredentialsAttachment attachment(wifi_credentials.id(),
                                           wifi_credentials.ssid(),
                                           wifi_credentials.security_type());
      SetAttachmentPayloadId(attachment, wifi_credentials.payload_id());
      share_target.wifi_credentials_attachments.push_back(
          std::move(attachment));
    }
  }

  if (!share_target.has_attachments()) {
    NL_LOG(WARNING) << __func__
                    << ": No attachment is found for this share target. It can "
                       "be result of unrecognizable attachment type";
    Fail(share_target, TransferMetadata::Status::kUnsupportedAttachmentType);

    NL_VLOG(1) << __func__
               << ": We don't support the attachments sent by the sender. "
                  "We have informed "
               << share_target.id;
    return;
  }

  // Log analytics event of receiving introduction.
  analytics_recorder_->NewReceiveIntroduction(
      receiving_session_id_, share_target, /*referrer_package=*/std::nullopt,
      info->os_type());

  if (file_size_sum == 0) {
    OnStorageCheckCompleted(std::move(share_target),
                            std::move(four_digit_token),
                            /*is_out_of_storage=*/false);
    return;
  }

  if (introduction_frame.has_start_transfer() &&
      introduction_frame.start_transfer()) {
    if (info->endpoint_id().has_value() &&
        share_target.GetTotalAttachmentsSize() >=
            kAttachmentsSizeThresholdOverHighQualityMedium) {
      NL_LOG(INFO)
          << __func__
          << ": Upgrade bandwidth when receiving an introduction frame.";
      nearby_connections_manager_->UpgradeBandwidth(*info->endpoint_id());
    }
  }

  std::filesystem::path download_path =
      std::filesystem::u8path(settings_->GetCustomSavePath());

  bool is_out_of_storage =
      IsOutOfStorage(device_info_, download_path, file_size_sum);

  OnStorageCheckCompleted(std::move(share_target), std::move(four_digit_token),
                          is_out_of_storage);
}

void NearbySharingServiceImpl::ReceiveConnectionResponse(
    ShareTarget share_target) {
  NL_VLOG(1) << __func__ << ": Receiving response frame from "
             << share_target.id;
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  NL_DCHECK(info && info->connection());

  info->frames_reader()->ReadFrame(
      nearby::sharing::service::proto::V1Frame::RESPONSE,
      [&, share_target = std::move(share_target)](
          std::optional<nearby::sharing::service::proto::V1Frame> frame) {
        OnReceiveConnectionResponse(share_target, std::move(frame));
      },
      kReadResponseFrameTimeout);
}

void NearbySharingServiceImpl::OnReceiveConnectionResponse(
    ShareTarget share_target,
    std::optional<nearby::sharing::service::proto::V1Frame> frame) {
  OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    NL_LOG(WARNING) << __func__
                    << ": Ignore received connection response, due to no "
                       "connection established.";
    return;
  }

  if (!info->transfer_update_callback()) {
    NL_LOG(WARNING) << __func__
                    << ": No transfer update callback. Disconnecting.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingTransferUpdateCallback, share_target);
    return;
  }

  if (!frame) {
    NL_LOG(WARNING)
        << __func__
        << ": Failed to read a response from the remote device. Disconnecting.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kFailedToReadOutgoingConnectionResponse,
        share_target);
    return;
  }

  mutual_acceptance_timeout_alarm_->Stop();

  NL_VLOG(1) << __func__
             << ": Successfully read the connection response frame.";

  nearby::sharing::service::proto::ConnectionResponseFrame response =
      std::move(frame->connection_response());
  switch (response.status()) {
    case nearby::sharing::service::proto::ConnectionResponseFrame::ACCEPT: {
      // Write progress update frame to remote machine.
      WriteProgressUpdateFrame(*info->connection(), true, std::nullopt);

      info->frames_reader()->ReadFrame(
          [&, share_target](
              std::optional<nearby::sharing::service::proto::V1Frame> frame) {
            OnFrameRead(share_target, std::move(frame));
          });

      info->transfer_update_callback()->OnTransferUpdate(
          share_target, TransferMetadataBuilder()
                            .set_status(TransferMetadata::Status::kInProgress)
                            .build());

      info->set_payload_tracker(std::make_unique<PayloadTracker>(
          context_, share_target, attachment_info_map_,
          [&](ShareTarget share_target, TransferMetadata transfer_metadata) {
            OnPayloadTransferUpdate(share_target, transfer_metadata);
          }));

      if (NearbyFlags::GetInstance().GetBoolFlag(
              config_package_nearby::nearby_sharing_feature::
                  kEnableTransferCancellationOptimization)) {
        std::optional<Payload> payload = info->ExtractNextPayload();
        if (payload.has_value()) {
          NL_LOG(INFO) << __func__ << ": Send  payload " << payload->id;

          nearby_connections_manager_->Send(*info->endpoint_id(),
                                            std::make_unique<Payload>(*payload),
                                            info->payload_tracker());
        } else {
          NL_LOG(WARNING) << __func__ << ": There is no payloads to send.";
        }
      } else {
        for (auto& payload : info->ExtractTextPayloads()) {
          nearby_connections_manager_->Send(*info->endpoint_id(),
                                            std::make_unique<Payload>(payload),
                                            info->payload_tracker());
        }
        for (auto& payload : info->ExtractFilePayloads()) {
          nearby_connections_manager_->Send(*info->endpoint_id(),
                                            std::make_unique<Payload>(payload),
                                            info->payload_tracker());
        }
      }
      NL_VLOG(1)
          << __func__
          << ": The connection was accepted. Payloads are now being sent.";
      break;
    }
    case nearby::sharing::service::proto::ConnectionResponseFrame::REJECT:
      AbortAndCloseConnectionIfNecessary(TransferMetadata::Status::kRejected,
                                         share_target);
      NL_VLOG(1)
          << __func__
          << ": The connection was rejected. The connection has been closed.";
      break;
    case nearby::sharing::service::proto::ConnectionResponseFrame::
        NOT_ENOUGH_SPACE:
      AbortAndCloseConnectionIfNecessary(
          TransferMetadata::Status::kNotEnoughSpace, share_target);
      NL_VLOG(1) << __func__
                 << ": The connection was rejected because the remote device "
                    "does not have enough space for our attachments. The "
                    "connection has been closed.";
      break;
    case nearby::sharing::service::proto::ConnectionResponseFrame::
        UNSUPPORTED_ATTACHMENT_TYPE:
      AbortAndCloseConnectionIfNecessary(
          TransferMetadata::Status::kUnsupportedAttachmentType, share_target);
      NL_VLOG(1) << __func__
                 << ": The connection was rejected because the remote device "
                    "does not support the attachments we were sending. The "
                    "connection has been closed.";
      break;
    case nearby::sharing::service::proto::ConnectionResponseFrame::TIMED_OUT:
      AbortAndCloseConnectionIfNecessary(TransferMetadata::Status::kTimedOut,
                                         share_target);
      NL_VLOG(1) << __func__
                 << ": The connection was rejected because the remote device "
                    "timed out. The connection has been closed.";
      break;
    default:
      AbortAndCloseConnectionIfNecessary(TransferMetadata::Status::kFailed,
                                         share_target);
      NL_VLOG(1) << __func__
                 << ": The connection failed. The connection has been closed.";
      break;
  }
}

void NearbySharingServiceImpl::OnStorageCheckCompleted(
    ShareTarget share_target, std::optional<std::string> four_digit_token,
    bool is_out_of_storage) {
  if (is_out_of_storage) {
    Fail(share_target, TransferMetadata::Status::kNotEnoughSpace);
    NL_LOG(WARNING) << __func__
                    << ": Not enough space on the receiver. We have informed "
                    << share_target.id;
    return;
  }

  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    NL_LOG(WARNING) << __func__ << ": Invalid connection for share target - "
                    << share_target.id;
    return;
  }
  NearbyConnection* connection = info->connection();

  if (!info->transfer_update_callback()) {
    NL_VLOG(1) << __func__ << ": No transfer update callback. Disconnecting.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingTransferUpdateCallback, share_target);
    return;
  }

  mutual_acceptance_timeout_alarm_->Stop();
  mutual_acceptance_timeout_alarm_->Start(
      absl::ToInt64Milliseconds(kReadResponseFrameTimeout), 0,
      [&, share_target]() { OnIncomingMutualAcceptanceTimeout(share_target); });

  bool is_self_share =
      NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_sharing_feature::kEnableSelfShare) &&
      !four_digit_token.has_value() && share_target.for_self_share;
  bool is_self_share_auto_accept = ShouldSelfShareAutoAccept(share_target);

  if (!is_self_share_auto_accept) {
    TransferMetadataBuilder transfer_metadata_builder;
    transfer_metadata_builder.set_status(
        TransferMetadata::Status::kAwaitingLocalConfirmation);
    transfer_metadata_builder.set_token(four_digit_token);
    transfer_metadata_builder.set_is_self_share(is_self_share);

    info->transfer_update_callback()->OnTransferUpdate(
        share_target, transfer_metadata_builder.build());
  } else {
    // Don't need to send kAwaitingLocalConfirmation for auto accept of Self
    // share.
    OnTransferStarted(/*is_incoming=*/true);
  }

  if (!incoming_share_target_info_map_.count(share_target.id)) {
    NL_VLOG(1) << __func__ << ": IncomingShareTarget not found, disconnecting "
               << share_target.id;
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingShareTarget, share_target);
    return;
  }

  connection->SetDisconnectionListener([&, share_target]() {
    RunOnNearbySharingServiceThread(
        "disconnection_listener", [&, share_target]() {
          OnIncomingConnectionDisconnected(share_target);
        });
  });

  auto* frames_reader = info->frames_reader();
  if (!frames_reader) {
    NL_LOG(WARNING) << __func__
                    << ": Stopped reading further frames, due to no connection "
                       "established.";
    return;
  }

  if (is_self_share_auto_accept) {
    NL_LOG(INFO) << __func__ << ": Auto-accepting self share.";
    Accept(share_target, [&](StatusCodes status_codes) {
      NL_LOG(INFO) << __func__ << ": Auto-accepting result: "
                   << static_cast<int>(status_codes);
    });
  }

  frames_reader->ReadFrame(
      [&, share_target = std::move(share_target)](
          std::optional<nearby::sharing::service::proto::V1Frame> frame) {
        OnFrameRead(std::move(share_target), std::move(frame));
      });
}

void NearbySharingServiceImpl::OnFrameRead(
    ShareTarget share_target,
    std::optional<nearby::sharing::service::proto::V1Frame> frame) {
  if (!frame.has_value()) {
    // This is the case when the connection has been closed since we wait
    // indefinitely for incoming frames.
    return;
  }

  switch (frame->type()) {
    case nearby::sharing::service::proto::V1Frame::CANCEL:
      RunOnAnyThread("cancel_transfer", [&, share_target]() {
        NL_LOG(INFO) << __func__
                     << ": Read the cancel frame, closing connection";
        DoCancel(
            share_target, [&](StatusCodes status_codes) {},
            /*is_initiator_of_cancellation=*/false);
      });
      break;

    case nearby::sharing::service::proto::V1Frame::CERTIFICATE_INFO:
      HandleCertificateInfoFrame(frame->certificate_info());
      break;

    case nearby::sharing::service::proto::V1Frame::PROGRESS_UPDATE:
      HandleProgressUpdateFrame(share_target, frame->progress_update());
      break;

    default:
      NL_LOG(ERROR) << __func__ << ": Discarding unknown frame of type";
      break;
  }

  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->frames_reader()) {
    NL_LOG(WARNING) << __func__
                    << ": Stopped reading further frames, due to no connection "
                       "established.";
    return;
  }

  info->frames_reader()->ReadFrame(
      [&, share_target = std::move(share_target)](
          std::optional<nearby::sharing::service::proto::V1Frame> frame) {
        OnFrameRead(share_target, std::move(frame));
      });
}

void NearbySharingServiceImpl::HandleCertificateInfoFrame(
    const nearby::sharing::service::proto::CertificateInfoFrame&
        certificate_frame) {
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_sharing_feature::kEnableSelfShare)) {
    return;
  }
}

void NearbySharingServiceImpl::HandleProgressUpdateFrame(
    const ShareTarget& share_target,
    const nearby::sharing::service::proto::ProgressUpdateFrame&
        progress_update_frame) {
  if (progress_update_frame.has_start_transfer() &&
      progress_update_frame.start_transfer()) {
    ShareTargetInfo* info = GetShareTargetInfo(share_target);

    if (info != nullptr && info->endpoint_id().has_value() &&
        share_target.GetTotalAttachmentsSize() >=
            kAttachmentsSizeThresholdOverHighQualityMedium) {
      NL_LOG(INFO)
          << __func__
          << ": Upgrade bandwidth when receiving progress update frame "
             "for endpoint "
          << (*info->endpoint_id());
      nearby_connections_manager_->UpgradeBandwidth(*info->endpoint_id());
    }
  }

  if (progress_update_frame.has_progress()) {
    NL_LOG(WARNING) << __func__ << ": Current progress for ShareTarget "
                    << share_target.id << " is "
                    << progress_update_frame.progress();
  }
}

void NearbySharingServiceImpl::OnIncomingConnectionDisconnected(
    const ShareTarget& share_target) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (info && info->transfer_update_callback()) {
    info->transfer_update_callback()->OnTransferUpdate(
        share_target,
        TransferMetadataBuilder()
            .set_status(TransferMetadata::Status::kUnexpectedDisconnection)
            .build());
  }
  UnregisterShareTarget(share_target);
}

void NearbySharingServiceImpl::OnOutgoingConnectionDisconnected(
    const ShareTarget& share_target) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (info && info->transfer_update_callback()) {
    info->transfer_update_callback()->OnTransferUpdate(
        share_target,
        TransferMetadataBuilder()
            .set_status(TransferMetadata::Status::kUnexpectedDisconnection)
            .build());
  }
  UnregisterShareTarget(share_target);
}

void NearbySharingServiceImpl::OnIncomingMutualAcceptanceTimeout(
    const ShareTarget& share_target) {
  NL_DCHECK(share_target.is_incoming);

  NL_VLOG(1)
      << __func__
      << ": Incoming mutual acceptance timed out, closing connection for "
      << share_target.id;

  Fail(share_target, TransferMetadata::Status::kTimedOut);
}

void NearbySharingServiceImpl::OnOutgoingMutualAcceptanceTimeout(
    const ShareTarget& share_target) {
  NL_DCHECK(!share_target.is_incoming);

  NL_VLOG(1)
      << __func__
      << ": Outgoing mutual acceptance timed out, closing connection for "
      << share_target.id;

  AbortAndCloseConnectionIfNecessary(TransferMetadata::Status::kTimedOut,
                                     share_target);
}

std::optional<ShareTarget> NearbySharingServiceImpl::CreateShareTarget(
    absl::string_view endpoint_id, std::unique_ptr<Advertisement> advertisement,
    std::optional<NearbyShareDecryptedPublicCertificate> certificate,
    bool is_incoming) {
  NL_DCHECK(advertisement);

  if (!advertisement->device_name() && !certificate.has_value()) {
    NL_VLOG(1) << __func__
               << ": Failed to retrieve public certificate for contact "
                  "only advertisement.";
    return std::nullopt;
  }

  std::optional<std::string> device_name =
      GetDeviceName(advertisement.get(), certificate);
  if (!device_name.has_value()) {
    NL_VLOG(1) << __func__
               << ": Failed to retrieve device name for advertisement.";
    return std::nullopt;
  }

  ShareTarget target;
  target.type = advertisement->device_type();
  target.device_name = std::move(*device_name);
  target.is_incoming = is_incoming;
  target.device_id = GetDeviceId(endpoint_id, certificate);
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_sharing_feature::kEnableSelfShare)) {
    target.for_self_share = certificate && certificate->for_self_share();
  }

  ShareTargetInfo& info = GetOrCreateShareTargetInfo(target, endpoint_id);

  if (certificate.has_value()) {
    if (certificate->unencrypted_metadata().has_full_name())
      target.full_name = certificate->unencrypted_metadata().full_name();

    if (certificate->unencrypted_metadata().has_icon_url()) {
      absl::StatusOr<::nearby::network::Url> url =
          ::nearby::network::Url::Create(
              certificate->unencrypted_metadata().icon_url());
      if (url.ok()) {
        target.image_url = url.value();
      } else {
        target.image_url = std::nullopt;
      }
    }

    target.is_known = true;
    info.set_certificate(std::move(*certificate));
  }

  return target;
}

void NearbySharingServiceImpl::OnPayloadTransferUpdate(
    ShareTarget share_target, TransferMetadata metadata) {
  bool is_in_progress =
      metadata.status() == TransferMetadata::Status::kInProgress;

  if (is_in_progress && share_target.is_incoming &&
      is_waiting_to_record_accept_to_transfer_start_metric_) {
    is_waiting_to_record_accept_to_transfer_start_metric_ = false;
  }

  // kInProgress status is logged extensively elsewhere so avoid the spam.
  if (!is_in_progress) {
    NL_VLOG(1) << __func__ << ": Nearby Share service: "
               << "Payload transfer update for share target with ID "
               << share_target.id << ": "
               << TransferMetadata::StatusToString(metadata.status());
  }

  // Update file paths during progress. It may impact transfer speed.
  // TODO: b/289290115 - Revisit UpdateFilePath to enhance transfer speed for
  // MacOS.
  if (update_file_paths_in_progress_ && share_target.is_incoming) {
    UpdateFilePath(share_target);
  }

  if (metadata.status() == TransferMetadata::Status::kComplete &&
      share_target.is_incoming) {
    if (!OnIncomingPayloadsComplete(share_target)) {
      metadata = TransferMetadataBuilder()
                     .set_status(TransferMetadata::Status::kIncompletePayloads)
                     .build();

      // Reset file paths for file attachments.
      for (auto& file : share_target.file_attachments)
        file.set_file_path(std::nullopt);

      // Reset body of text attachments.
      for (auto& text : share_target.text_attachments)
        text.set_text_body(std::string());

      // Reset password of Wi-Fi credentials attachments.
      for (auto& wifi_credentials : share_target.wifi_credentials_attachments) {
        wifi_credentials.set_password(std::string());
        wifi_credentials.set_is_hidden(false);
      }
    }

    if (IsBackgroundScanningFeatureEnabled()) {
      fast_initiation_scanner_cooldown_timer_->Stop();
      fast_initiation_scanner_cooldown_timer_->Start(
          absl::ToInt64Milliseconds(kFastInitiationScannerCooldown), 0, [&]() {
            fast_initiation_scanner_cooldown_timer_->Stop();
            InvalidateFastInitiationScanning();
          });
    }
  } else if (metadata.status() == TransferMetadata::Status::kCancelled &&
             share_target.is_incoming) {
    NL_VLOG(1) << __func__ << ": Update file paths for cancelled transfer";
    if (!update_file_paths_in_progress_) {
      UpdateFilePath(share_target);
    }
  }

  // Make sure to call this before calling Disconnect, or we risk losing some
  // transfer updates in the receive case due to the Disconnect call cleaning up
  // share targets.
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (info && info->transfer_update_callback())
    info->transfer_update_callback()->OnTransferUpdate(share_target, metadata);

  // Cancellation has its own disconnection strategy, possibly adding a delay
  // before disconnection to provide the other party time to process the
  // cancellation.
  if (TransferMetadata::IsFinalStatus(metadata.status()) &&
      metadata.status() != TransferMetadata::Status::kCancelled) {
    Disconnect(share_target, metadata);
  }
}

bool NearbySharingServiceImpl::OnIncomingPayloadsComplete(
    ShareTarget& share_target) {
  NL_DCHECK(share_target.is_incoming);

  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    NL_VLOG(1) << __func__ << ": Connection not found for target - "
               << share_target.id;

    return false;
  }
  NearbyConnection* connection = info->connection();

  connection->SetDisconnectionListener([&, share_target]() {
    RunOnNearbySharingServiceThread(
        "disconnection_listener",
        [&, share_target]() { UnregisterShareTarget(share_target); });
  });

  if (!update_file_paths_in_progress_) {
    UpdateFilePath(share_target);
  }

  for (auto& text : share_target.text_attachments) {
    AttachmentInfo& attachment_info = attachment_info_map_[text.id()];
    std::optional<int64_t> payload_id = attachment_info.payload_id;
    if (!payload_id) {
      NL_LOG(WARNING) << __func__ << ": No payload id found for text - "
                      << text.id();
      return false;
    }

    Payload* incoming_payload =
        nearby_connections_manager_->GetIncomingPayload(*payload_id);
    if (!incoming_payload || !incoming_payload->content.is_bytes()) {
      NL_LOG(WARNING) << __func__ << ": No payload found for text - "
                      << text.id();
      return false;
    }

    std::vector<uint8_t> bytes = incoming_payload->content.bytes_payload.bytes;
    if (bytes.empty()) {
      NL_LOG(WARNING)
          << __func__
          << ": Incoming bytes is empty for text payload with payload_id - "
          << *payload_id;
      return false;
    }

    std::string text_body(bytes.begin(), bytes.end());
    text.set_text_body(text_body);

    attachment_info.text_body = std::move(text_body);
  }

  for (auto& wifi_credentials_attachment :
       share_target.wifi_credentials_attachments) {
    AttachmentInfo& attachment_info =
        attachment_info_map_[wifi_credentials_attachment.id()];
    std::optional<int64_t> payload_id = attachment_info.payload_id;
    if (!payload_id) {
      NL_LOG(WARNING) << __func__
                      << ": No payload id found for WiFi credentials - "
                      << wifi_credentials_attachment.id();
      return false;
    }

    Payload* incoming_payload =
        nearby_connections_manager_->GetIncomingPayload(*payload_id);
    if (!incoming_payload || !incoming_payload->content.is_bytes()) {
      NL_LOG(WARNING) << __func__
                      << ": No payload found for WiFi credentials - "
                      << wifi_credentials_attachment.id();
      return false;
    }

    std::vector<uint8_t> bytes = incoming_payload->content.bytes_payload.bytes;
    if (bytes.empty()) {
      NL_LOG(WARNING) << __func__
                      << ": Incoming bytes is empty for WiFi credentials "
                         "payload with payload_id - "
                      << *payload_id;
      return false;
    }

    auto wifi_credentials =
        std::make_unique<nearby::sharing::service::proto::WifiCredentials>();
    if (!wifi_credentials->ParseFromArray(bytes.data(), bytes.size())) {
      NL_LOG(WARNING) << __func__
                      << ": Incoming bytes is invalid for WiFi credentials "
                         "payload with payload_id - "
                      << *payload_id;
      return false;
    }

    wifi_credentials_attachment.set_password(wifi_credentials->password());
    wifi_credentials_attachment.set_is_hidden(wifi_credentials->hidden_ssid());
  }

  return true;
}

void NearbySharingServiceImpl::UpdateFilePath(ShareTarget& share_target) {
  for (auto& file : share_target.file_attachments) {
    // Skip file if it already has file_path set.
    if (file.file_path().has_value()) {
      continue;
    }
    AttachmentInfo& attachment_info = attachment_info_map_[file.id()];
    std::optional<int64_t> payload_id = attachment_info.payload_id;
    if (!payload_id) {
      NL_LOG(WARNING) << __func__ << ": No payload id found for file - "
                      << file.id();
      continue;
    }

    Payload* incoming_payload =
        nearby_connections_manager_->GetIncomingPayload(*payload_id);
    if (!incoming_payload || !incoming_payload->content.is_file()) {
      NL_LOG(WARNING) << __func__ << ": No payload found for file - "
                      << file.id();
      continue;
    }

    auto file_path = incoming_payload->content.file_payload.file.path;
    NL_VLOG(1) << __func__ << ": Updated file_path="
               << GetCompatibleU8String(file_path.u8string());
    file.set_file_path(file_path);
  }
}

void NearbySharingServiceImpl::RemoveIncomingPayloads(
    ShareTarget share_target) {
  if (!share_target.is_incoming) {
    return;
  }

  NL_LOG(INFO) << __func__ << ": Cleaning up payloads due to transfer failure";
  nearby_connections_manager_->ClearIncomingPayloads();
  std::vector<std::filesystem::path> files_for_deletion;
  for (const auto& file : share_target.file_attachments) {
    if (!file.file_path().has_value()) continue;
    auto file_path = *file.file_path();
    NL_VLOG(1) << __func__
               << ": file_path=" << GetCompatibleU8String(file_path.u8string());
    if (attachment_info_map_.find(file.id()) == attachment_info_map_.end()) {
      continue;
    }
    files_for_deletion.push_back(file_path);
  }
  file_handler_.DeleteFilesFromDisk(std::move(files_for_deletion), []() {});
}

void NearbySharingServiceImpl::Disconnect(const ShareTarget& share_target,
                                          TransferMetadata metadata) {
  ShareTargetInfo* share_target_info = GetShareTargetInfo(share_target);
  if (!share_target_info) {
    NL_LOG(WARNING)
        << __func__
        << ": Failed to disconnect. No share target info found for target - "
        << share_target.id;
    return;
  }

  std::optional<std::string> endpoint_id = share_target_info->endpoint_id();
  if (!endpoint_id.has_value()) {
    NL_LOG(WARNING)
        << __func__
        << ": Failed to disconnect. No endpoint id found for share target - "
        << share_target.id;
    return;
  }

  // Failed to send or receive. No point in continuing, so disconnect
  // immediately.
  if (metadata.status() != TransferMetadata::Status::kComplete) {
    if (share_target_info->connection()) {
      share_target_info->connection()->Close();
    } else {
      nearby_connections_manager_->Disconnect(*endpoint_id);
    }
    return;
  }

  // Files received successfully. Receivers can immediately cancel.
  if (share_target.is_incoming) {
    if (share_target_info->connection()) {
      share_target_info->connection()->Close();
    } else {
      nearby_connections_manager_->Disconnect(*endpoint_id);
    }
    return;
  }

  // Disconnect after a timeout to make sure any pending payloads are sent.
  //
  // We assign endpoint_id = *endpoint_id here since endpoint_id is
  // std::optional<std::string> so the lambda will capture the string by value.
  // For absl::string_view, please make it sure you wrap the string_view object
  // with std::string() so that it captures the string by value correctly.
  auto timer = context_->CreateTimer();
  timer->Start(absl::ToInt64Milliseconds(kOutgoingDisconnectionDelay), 0,
               [&, endpoint_id = *endpoint_id]() {
                 OnDisconnectingConnectionTimeout(endpoint_id);
               });

  disconnection_timeout_alarms_[*endpoint_id] = std::move(timer);

  // Stop the disconnection timeout if the connection has been closed already.
  //
  // We assign endpoint_id = *endpoint_id here since endpoint_id is
  // std::optional<std::string> so the lambda will capture the string by value.
  // For absl::string_view, please make it sure you wrap the string_view object
  // with std::string() so that it captures the string by value correctly.
  if (share_target_info->connection()) {
    share_target_info->connection()->SetDisconnectionListener(
        [&, share_target, share_target_info, endpoint_id = *endpoint_id]() {
          share_target_info->set_connection(nullptr);
          RunOnNearbySharingServiceThread(
              "disconnection_listener", [&, share_target, endpoint_id]() {
                OnDisconnectingConnectionDisconnected(share_target,
                                                      endpoint_id);
              });
        });
  }
}

void NearbySharingServiceImpl::OnDisconnectingConnectionTimeout(
    absl::string_view endpoint_id) {
  RunOnNearbySharingServiceThread(
      "on_disconnecting_connection_timeout",
      [&, endpoint_id = std::string(endpoint_id)]() {
        disconnection_timeout_alarms_.erase(endpoint_id);
      });
  nearby_connections_manager_->Disconnect(endpoint_id);
}

void NearbySharingServiceImpl::OnDisconnectingConnectionDisconnected(
    const ShareTarget& share_target, absl::string_view endpoint_id) {
  disconnection_timeout_alarms_.erase(endpoint_id);
  UnregisterShareTarget(share_target);
}

ShareTargetInfo& NearbySharingServiceImpl::GetOrCreateShareTargetInfo(
    const ShareTarget& share_target, absl::string_view endpoint_id) {
  if (share_target.is_incoming) {
    auto& info = incoming_share_target_info_map_[share_target.id];
    info.set_endpoint_id(std::string(endpoint_id));
    return info;
  } else {
    // We need to explicitly remove any previous share target for
    // |endpoint_id| if one exists, notifying observers that a share target is
    // lost.
    const auto it = outgoing_share_target_map_.find(endpoint_id);
    if (it != outgoing_share_target_map_.end() &&
        it->second.id != share_target.id) {
      RemoveOutgoingShareTargetWithEndpointId(endpoint_id);
    }

    NL_VLOG(1) << __func__ << ": Adding (endpoint_id=" << endpoint_id
               << ", share_target_id=" << share_target.id
               << ") to outgoing share target map";
    outgoing_share_target_map_.insert_or_assign(endpoint_id, share_target);
    auto& info = outgoing_share_target_info_map_[share_target.id];
    info.set_endpoint_id(std::string(endpoint_id));
    info.set_connection_layer_status(Status::kUnknown);
    return info;
  }
}

ShareTargetInfo* NearbySharingServiceImpl::GetShareTargetInfo(
    const ShareTarget& share_target) {
  if (share_target.is_incoming)
    return GetIncomingShareTargetInfo(share_target);
  else
    return GetOutgoingShareTargetInfo(share_target);
}

IncomingShareTargetInfo* NearbySharingServiceImpl::GetIncomingShareTargetInfo(
    const ShareTarget& share_target) {
  auto it = incoming_share_target_info_map_.find(share_target.id);
  if (it == incoming_share_target_info_map_.end()) {
    return nullptr;
  }

  return &it->second;
}

OutgoingShareTargetInfo* NearbySharingServiceImpl::GetOutgoingShareTargetInfo(
    const ShareTarget& share_target) {
  auto it = outgoing_share_target_info_map_.find(share_target.id);
  if (it == outgoing_share_target_info_map_.end()) {
    return nullptr;
  }

  return &it->second;
}

NearbyConnection* NearbySharingServiceImpl::GetConnection(
    const ShareTarget& share_target) {
  ShareTargetInfo* share_target_info = GetShareTargetInfo(share_target);
  return share_target_info ? share_target_info->connection() : nullptr;
}

std::optional<std::vector<uint8_t>>
NearbySharingServiceImpl::GetBluetoothMacAddressForShareTarget(
    const ShareTarget& share_target) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info) {
    NL_LOG(ERROR) << __func__ << ": No ShareTargetInfo found for "
                  << "share target id: " << share_target.id;
    return std::nullopt;
  }

  const std::optional<NearbyShareDecryptedPublicCertificate>& certificate =
      info->certificate();
  if (!certificate) {
    NL_LOG(ERROR) << __func__ << ": No decrypted public certificate found for "
                  << "share target id: " << share_target.id;
    return std::nullopt;
  }

  return GetBluetoothMacAddressFromCertificate(*certificate);
}

void NearbySharingServiceImpl::ClearOutgoingShareTargetInfoMap() {
  NL_VLOG(1) << __func__ << ": Clearing outgoing share target map.";
  while (!outgoing_share_target_map_.empty()) {
    RemoveOutgoingShareTargetWithEndpointId(
        /*endpoint_id=*/outgoing_share_target_map_.begin()->first);
  }
  NL_DCHECK(outgoing_share_target_map_.empty());
  NL_DCHECK(outgoing_share_target_info_map_.empty());
}

void NearbySharingServiceImpl::SetAttachmentPayloadId(
    const Attachment& attachment, int64_t payload_id) {
  attachment_info_map_[attachment.id()].payload_id = payload_id;
}

std::optional<int64_t> NearbySharingServiceImpl::GetAttachmentPayloadId(
    int64_t attachment_id) {
  auto it = attachment_info_map_.find(attachment_id);
  if (it == attachment_info_map_.end()) return std::nullopt;

  return it->second.payload_id;
}

void NearbySharingServiceImpl::UnregisterShareTarget(
    const ShareTarget& share_target) {
  NL_VLOG(1) << __func__ << ": Unregistering share target - "
             << share_target.id;

  // For metrics.
  all_cancelled_share_target_ids_.erase(share_target.id);

  if (share_target.is_incoming) {
    if (last_incoming_metadata_ &&
        last_incoming_metadata_->first.id == share_target.id) {
      last_incoming_metadata_.reset();
    }

    // Clear legacy incoming payloads to release resources.
    nearby_connections_manager_->ClearIncomingPayloads();
    incoming_share_target_info_map_.erase(share_target.id);
  } else {
    if (last_outgoing_metadata_ &&
        last_outgoing_metadata_->first.id == share_target.id) {
      last_outgoing_metadata_.reset();
    }
    // Find the endpoint id that matches the given share target.
    std::optional<std::string> endpoint_id;
    auto it = outgoing_share_target_info_map_.find(share_target.id);
    if (it != outgoing_share_target_info_map_.end())
      endpoint_id = it->second.endpoint_id();

    if (endpoint_id.has_value()) {
      RemoveOutgoingShareTargetWithEndpointId(*endpoint_id);
      mutual_acceptance_timeout_alarm_->Stop();
      return;
    }

    // Be careful not to clear out the share target info map if a new session
    // was started during the cancellation delay.
    if (!is_scanning_ && !is_transferring_) {
      ClearOutgoingShareTargetInfoMap();
    }

    NL_VLOG(1) << __func__ << ": Unregister share target: " << share_target.id;
  }
  mutual_acceptance_timeout_alarm_->Stop();
}

void NearbySharingServiceImpl::OnStartAdvertisingResult(bool used_device_name,
                                                        Status status) {
  if (status == Status::kSuccess) {
    NL_VLOG(1) << __func__
               << ": StartAdvertising over Nearby Connections was successful.";
    SetInHighVisibility(used_device_name);
  } else {
    NL_LOG(ERROR) << __func__
                  << ": StartAdvertising over Nearby Connections failed: "
                  << NearbyConnectionsManager::ConnectionsStatusToString(
                         status);
    SetInHighVisibility(false);
    for (auto& observer : observers_.GetObservers()) {
      observer->OnStartAdvertisingFailure();
    }
  }
}

void NearbySharingServiceImpl::OnStopAdvertisingResult(Status status) {
  if (status == Status::kSuccess) {
    NL_VLOG(1) << __func__
               << ": StopAdvertising over Nearby Connections was successful.";
  } else {
    NL_LOG(ERROR) << __func__
                  << ": StopAdvertising over Nearby Connections failed: "
                  << NearbyConnectionsManager::ConnectionsStatusToString(
                         status);
  }

  // The |advertising_power_level_| is set in |StopAdvertising| instead of
  // here at the callback because when restarting advertising,
  // |StartAdvertising| is called immediately after |StopAdvertising| without
  // waiting for the callback. Nearby Connections queues the requests and
  // completes them in order, so waiting for Stop to complete is unnecessary,
  // but Start will fail if the |advertising_power_level_| indicates we are
  // already advertising.
  SetInHighVisibility(false);
}

void NearbySharingServiceImpl::OnStartDiscoveryResult(Status status) {
  bool success = status == Status::kSuccess;
  if (success) {
    NL_VLOG(1) << __func__
               << ": StartDiscovery over Nearby Connections was successful.";

    // Periodically download certificates if there are discovered, contact-based
    // advertisements that cannot decrypt any currently stored certificates.
    ScheduleCertificateDownloadDuringDiscovery(/*attempt_count=*/0);
  } else {
    NL_LOG(ERROR) << __func__
                  << ": StartDiscovery over Nearby Connections failed: "
                  << NearbyConnectionsManager::ConnectionsStatusToString(
                         status);
  }
  for (auto& observer : observers_.GetObservers()) {
    observer->OnStartDiscoveryResult(success);
  }
}

void NearbySharingServiceImpl::SetInHighVisibility(
    bool new_in_high_visibility) {
  if (in_high_visibility_ == new_in_high_visibility) {
    return;
  }

  in_high_visibility_ = new_in_high_visibility;
  for (auto& observer : observers_.GetObservers()) {
    observer->OnHighVisibilityChanged(in_high_visibility_);
  }
}

void NearbySharingServiceImpl::AbortAndCloseConnectionIfNecessary(
    TransferMetadata::Status status, const ShareTarget& share_target) {
  RunOnNearbySharingServiceThread(
      "abort_and_close_connection_if_necessary", [&, status, share_target]() {
        TransferMetadata metadata =
            TransferMetadataBuilder().set_status(status).build();
        ShareTargetInfo* info = GetShareTargetInfo(share_target);

        if (info == nullptr) {
          NL_LOG(WARNING) << ": Share target " << share_target.id << " lost";
          return;
        }

        // First invoke the appropriate transfer callback with the final
        // |status|.
        if (info && info->transfer_update_callback()) {
          info->transfer_update_callback()->OnTransferUpdate(share_target,
                                                             metadata);
        } else if (share_target.is_incoming) {
          OnIncomingTransferUpdate(share_target, metadata);
        } else {
          OnOutgoingTransferUpdate(share_target, metadata);
        }

        // Close connection if necessary.
        if (info && info->connection()) {
          // Ensure that the disconnect listener is set to UnregisterShareTarget
          // because the other listeners also try to record a final status
          // metric.
          info->connection()->SetDisconnectionListener([&, share_target]() {
            RunOnNearbySharingServiceThread(
                "disconnection_listener",
                [&, share_target]() { UnregisterShareTarget(share_target); });
          });

          info->connection()->Close();
        }
      });
}

void NearbySharingServiceImpl::OnNetworkChanged(
    nearby::ConnectivityManager::ConnectionType type) {
  on_network_changed_delay_timer_->Stop();
  on_network_changed_delay_timer_->Start(
      absl::ToInt64Milliseconds(kProcessNetworkChangeTimerDelay), 0, [&]() {
        RunOnNearbySharingServiceThread("on-network-changed", [&]() {
          StopAdvertisingAndInvalidateSurfaceState();
        });
      });
}

void NearbySharingServiceImpl::OnLanConnectedChanged(bool connected) {
  RunOnNearbySharingServiceThread("lan_connection_changed", [&, connected]() {
    NL_VLOG(1) << __func__
               << ": LAN Connection state changed. (Connected: " << connected
               << ")";
    for (auto& observer : observers_.GetObservers()) {
      observer->OnLanStatusChanged();
    }
  });
}

void NearbySharingServiceImpl::ResetAllSettings(bool logout) {
  NL_LOG(INFO) << __func__ << ": Reset all settings!";

  // Stop all services.
  StopAdvertising();
  StopScanning();
  nearby_connections_manager_->Shutdown();
  local_device_data_manager_->Stop();
  contact_manager_->Stop();
  certificate_manager_->Stop();

  // Reset preferences for logout.
  if (logout) {
    settings_->RemoveSettingsObserver(this);
    // Visibility has a impact on the UI. So let's set this one first
    // so that the UI can be as responsive as possible.
    DeviceVisibility visibility = settings_->GetVisibility();
    bool is_temporarily_visible = settings_->GetIsTemporarilyVisible();
    // When logged out the visibility can either be "everyone" or "hidden". If
    // the visibility wasn't already "always everyone", change it to "hidden".
    if (visibility == DeviceVisibility::DEVICE_VISIBILITY_EVERYONE &&
        !is_temporarily_visible) {
      settings_->SetIsReceiving(true);
    } else {
      settings_->SetIsReceiving(false);
      settings_->SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_HIDDEN);
    }

    // There is no valid fallback visibility when logged out until we support
    // "off" as an option.
    settings_->SetFallbackVisibility(
        DeviceVisibility::DEVICE_VISIBILITY_EVERYONE);
    prefs::RegisterNearbySharingPrefs(preference_manager_,
                                      /*skip_persistent_ones=*/true);

    settings_->AddSettingsObserver(this);
    certificate_manager_->ClearPublicCertificates([&](bool result) {
      NL_LOG(INFO) << "Clear public certificates. result: " << result;
    });
  } else {
    // should clear scheduled task to make it works immediately
    settings_->RemoveSettingsObserver(this);
    prefs::ResetSchedulers(preference_manager_);
    settings_->AddSettingsObserver(this);
    settings_->OnLocalDeviceDataChanged(/*did_device_name_change=*/true,
                                        /*did_full_name_change=*/false,
                                        /*did_icon_url_change=*/false);
    // Set default visibility to kAllContacts if logged-in and onboarding.
    if (!settings_->IsOnboardingComplete()) {
      NL_LOG(INFO) << __func__
                   << ": Set visibility to kAllContacts since user is "
                      "logged-in during onboarding";
      settings_->SetVisibility(
          DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
    }
  }

  // Start services again.
  local_device_data_manager_->Start();
  contact_manager_->Start();
  certificate_manager_->Start();

  InvalidateSurfaceState();
}

bool NearbySharingServiceImpl::ShouldSelfShareAutoAccept(
    const ShareTarget& share_target) const {
  // Auto-accept self shares when not in high-visibility mode.
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_sharing_feature::kEnableSelfShare) &&
      share_target.for_self_share) {
    return true;
  }

  return false;
}

bool NearbySharingServiceImpl::ReadyToAccept(
    const ShareTarget& share_target, TransferMetadata::Status status) const {
  if (status == TransferMetadata::Status::kAwaitingLocalConfirmation) {
    return true;
  }

  if (ShouldSelfShareAutoAccept(share_target) &&
      status == TransferMetadata::Status::kUnknown) {
    return true;
  }

  return false;
}

void NearbySharingServiceImpl::RunOnNearbySharingServiceThread(
    absl::string_view task_name, std::function<void()> task) {
  if (is_shutting_down_ == nullptr || *is_shutting_down_) {
    NL_LOG(WARNING) << __func__ << ": Skip the task " << task_name
                    << " due to service is shutting down.";
    return;
  }

  NL_LOG(INFO) << __func__ << ": Scheduled to run task " << task_name
               << " on API thread.";

  service_thread_->PostTask(
      [&, is_shutting_down = std::weak_ptr<bool>(is_shutting_down_),
       task_name = std::string(task_name), task = std::move(task)]() {
        std::shared_ptr<bool> is_shutting = is_shutting_down.lock();
        if (is_shutting == nullptr || *is_shutting) {
          NL_LOG(WARNING) << __func__ << ": Give up the task " << task_name
                          << " due to service is shutting down.";
          return;
        }

        NL_LOG(INFO) << __func__ << ": Started to run task " << task_name
                     << " on API thread.";
        task();

        NL_LOG(INFO) << __func__ << ": Completed to run task " << task_name
                     << " on API thread.";
      });
}

void NearbySharingServiceImpl::RunOnNearbySharingServiceThreadDelayed(
    absl::string_view task_name, absl::Duration delay,
    std::function<void()> task) {
  if (is_shutting_down_ == nullptr || *is_shutting_down_) {
    NL_LOG(WARNING) << __func__ << ": Skip the delayed task " << task_name
                    << " due to service is shutting down.";
    return;
  }

  NL_LOG(INFO) << __func__ << ": Scheduled to run delayed task " << task_name
               << " on API thread.";
  service_thread_->PostDelayedTask(
      delay, [&, is_shutting_down = std::weak_ptr<bool>(is_shutting_down_),
              task_name = std::string(task_name), task = std::move(task)]() {
        std::shared_ptr<bool> is_shutting = is_shutting_down.lock();
        if (is_shutting == nullptr || *is_shutting) {
          NL_LOG(WARNING) << __func__ << ": Give up the delayed task "
                          << task_name << " due to service is shutting down.";
          return;
        }

        NL_LOG(INFO) << __func__ << ": Started to run delayed task "
                     << task_name << " on API thread.";
        task();

        NL_LOG(INFO) << __func__ << ": Completed to run delayed task "
                     << task_name << " on API thread.";
      });
}

void NearbySharingServiceImpl::RunOnAnyThread(absl::string_view task_name,
                                              std::function<void()> task) {
  if (is_shutting_down_ == nullptr || *is_shutting_down_) {
    NL_LOG(WARNING) << __func__ << ": Skip the task " << task_name
                    << " due to service is shutting down.";
    return;
  }

  NL_LOG(INFO) << __func__ << ": Scheduled to run task " << task_name
               << " on API thread.";
  context_->GetTaskRunner()->PostTask(
      [&, is_shutting_down = std::weak_ptr<bool>(is_shutting_down_),
       task_name = std::string(task_name), task = std::move(task)]() {
        std::shared_ptr<bool> is_shutting = is_shutting_down.lock();
        if (is_shutting == nullptr || *is_shutting) {
          NL_LOG(WARNING) << __func__ << ": Give up the delayed task "
                          << task_name << " due to service is shutting down.";
          return;
        }

        NL_LOG(INFO) << __func__ << ": Started to run task " << task_name
                     << " on API thread.";
        task();

        NL_LOG(INFO) << __func__ << ": Completed to run task " << task_name
                     << " on API thread.";
      });
}

int NearbySharingServiceImpl::GetConnectedShareTargetPos(
    const ShareTarget& target) {
  // Returns 1 before group sharing is enabled.
  return 1;
}

int NearbySharingServiceImpl::GetConnectedShareTargetCount() {
  // Returns 1 before group sharing is enabled.
  return 1;
}

::location::nearby::proto::sharing::SharingUseCase
NearbySharingServiceImpl::GetSenderUseCase() {
  // Returns unknown before group sharing is enabled.
  return ::location::nearby::proto::sharing::SharingUseCase::USE_CASE_UNKNOWN;
}

TransportType NearbySharingServiceImpl::GetTransportType(
    const ShareTarget& share_target) const {
  if (share_target.GetTotalAttachmentsSize() >
      kAttachmentsSizeThresholdOverHighQualityMedium) {
    NL_LOG(INFO) << __func__ << ": Transport type is kHighQuality";
    return TransportType::kHighQuality;
  }

  if (share_target.file_attachments.empty()) {
    NL_LOG(INFO) << __func__ << ": Transport type is kNonDisruptive";
    return TransportType::kNonDisruptive;
  }

  NL_LOG(INFO) << __func__ << ": Transport type is kAny";
  return TransportType::kAny;
}

void NearbySharingServiceImpl::UpdateFilePathsInProgress(
    bool update_file_paths) {
  update_file_paths_in_progress_ = update_file_paths;
  NL_LOG(INFO) << __func__
               << ": Update file paths in progress: " << update_file_paths;
}

}  // namespace sharing
}  // namespace nearby
