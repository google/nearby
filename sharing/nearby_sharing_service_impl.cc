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

#include <algorithm>
#include <array>
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
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/functional/bind_front.h"
#include "absl/random/random.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
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
#include "internal/platform/task_runner.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/advertisement.h"
#include "sharing/analytics/analytics_information.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/attachment_container.h"
#include "sharing/certificates/common.h"
#include "sharing/certificates/nearby_share_certificate_manager.h"
#include "sharing/certificates/nearby_share_certificate_manager_impl.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/certificates/nearby_share_encrypted_metadata_key.h"
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
#include "sharing/incoming_share_session.h"
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
#include "sharing/outgoing_share_session.h"
#include "sharing/paired_key_verification_runner.h"
#include "sharing/proto/encrypted_metadata.pb.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/scheduling/nearby_share_scheduler_utils.h"
#include "sharing/share_session.h"
#include "sharing/share_target.h"
#include "sharing/share_target_discovered_callback.h"
#include "sharing/thread_timer.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_metadata_builder.h"
#include "sharing/transfer_update_callback.h"
#include "sharing/wrapped_share_target_discovered_callback.h"

namespace nearby::sharing {
namespace {

using BlockedVendorId = ::nearby::sharing::Advertisement::BlockedVendorId;
using ::location::nearby::proto::sharing::AttachmentTransmissionStatus;
using ::location::nearby::proto::sharing::EstablishConnectionStatus;
using ::location::nearby::proto::sharing::OSType;
using ::location::nearby::proto::sharing::ResponseToIntroduction;
using ::location::nearby::proto::sharing::SessionStatus;
using ::nearby::sharing::api::SharingPlatform;
using ::nearby::sharing::proto::DataUsage;
using ::nearby::sharing::proto::DeviceVisibility;
using ::nearby::sharing::service::proto::ConnectionResponseFrame;
using ::nearby::sharing::service::proto::IntroductionFrame;

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

bool ShouldBlockSurfaceRegistration(BlockedVendorId registering_vendor_id,
                                    BlockedVendorId blocked_vendor_id) {
  return blocked_vendor_id != BlockedVendorId::kNone &&
         registering_vendor_id != BlockedVendorId::kNone &&
         registering_vendor_id != blocked_vendor_id;
}

NearbySharingService::Observer::AdapterState MapAdapterState(bool is_present,
                                                             bool is_powered) {
  NearbySharingService::Observer::AdapterState state =
      NearbySharingService::Observer::AdapterState::NOT_PRESENT;
  if (is_present) {
    if (is_powered) {
      state = NearbySharingService::Observer::AdapterState::ENABLED;
    } else {
      state = NearbySharingService::Observer::AdapterState::DISABLED;
    }
  }
  return state;
}

OSType ToProtoOsType(::nearby::api::DeviceInfo::OsType os_type) {
  switch (os_type) {
    case ::nearby::api::DeviceInfo::OsType::kAndroid:
      return OSType::ANDROID;
    case ::nearby::api::DeviceInfo::OsType::kChromeOs:
      return OSType::CHROME_OS;
    case ::nearby::api::DeviceInfo::OsType::kWindows:
      return OSType::WINDOWS;
    case ::nearby::api::DeviceInfo::OsType::kIos:
      return OSType::IOS;
    case ::nearby::api::DeviceInfo::OsType::kMacOS:
      return OSType::MACOS;
    case ::nearby::api::DeviceInfo::OsType::kUnknown:
      return OSType::UNKNOWN_OS_TYPE;
  }
}

}  // namespace

NearbySharingServiceImpl::NearbySharingServiceImpl(
    int32_t vendor_id, std::unique_ptr<TaskRunner> service_thread,
    Context* context, SharingPlatform& sharing_platform,
    std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager,
    nearby::analytics::EventLogger* event_logger)
    : service_thread_(std::move(service_thread)),
      context_(context),
      device_info_(sharing_platform.GetDeviceInfo()),
      preference_manager_(sharing_platform.GetPreferenceManager()),
      account_manager_(sharing_platform.GetAccountManager()),
      nearby_connections_manager_(std::move(nearby_connections_manager)),
      analytics_recorder_(std::make_unique<analytics::AnalyticsRecorder>(
          vendor_id, event_logger)),
      nearby_share_client_factory_(
          sharing_platform.CreateSharingRpcClientFactory(
              analytics_recorder_.get())),
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
      settings_(std::make_unique<NearbyShareSettings>(
          context_, context_->GetClock(), device_info_, preference_manager_,
          local_device_data_manager_.get(), analytics_recorder_.get())),
      service_extension_(std::make_unique<NearbySharingServiceExtension>()),
      file_handler_(sharing_platform),
      app_info_(sharing_platform.CreateAppInfo()) {
  NL_DCHECK(nearby_connections_manager_);

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
      [this](nearby::api::DeviceInfo::ScreenStatus screen_status) {
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

  local_device_data_manager_->Start();
  contact_manager_->Start();
  certificate_manager_->Start();
  update_file_paths_in_progress_ = false;

  SetupBluetoothAdapter();
}

NearbySharingServiceImpl::~NearbySharingServiceImpl() = default;

void NearbySharingServiceImpl::Shutdown(
    std::function<void(StatusCodes)> status_codes_callback) {
  RunOnNearbySharingServiceThread(
      "api_shutdown",
      [this, status_codes_callback = std::move(status_codes_callback)]() {
        *is_shutting_down_ = true;
        for (auto* observer : observers_.GetObservers()) {
          observer->OnShutdown();
        }

        observers_.Clear();

        StopAdvertising();
        StopFastInitiationScanning();
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

        on_network_changed_delay_timer_.reset();

        foreground_receive_callbacks_map_.clear();
        background_receive_callbacks_map_.clear();

        device_info_.UnregisterScreenLockedListener(kScreenStateListenerName);

        settings_->RemoveSettingsObserver(this);

        local_device_data_manager_->Stop();
        contact_manager_->Stop();
        certificate_manager_->Stop();

        is_shutting_down_ = nullptr;
        std::move(status_codes_callback)(StatusCodes::kOk);
      });
}

bool NearbySharingServiceImpl::IsShuttingDown() {
  return (is_shutting_down_ == nullptr || *is_shutting_down_);
}

void NearbySharingServiceImpl::Cleanup() {
  SetInHighVisibility(false);

  endpoint_discovery_events_ = {};

  ClearOutgoingShareSessionMap();
  discovery_cache_.clear();
  for (auto& it : incoming_share_session_map_) {
    it.second.OnDisconnect();
  }
  // Clear out the incoming_share_session_map_ before destroying the sessions.
  // Session destruction can trigger callbacks that access the map.  That is not
  // allowed while the map is being modified.
  absl::flat_hash_map<int64_t, IncomingShareSession> tmp_incoming_session_map;
  tmp_incoming_session_map.swap(incoming_share_session_map_);
  tmp_incoming_session_map.clear();

  discovered_advertisements_to_retry_map_.clear();
  discovered_advertisements_retried_set_.clear();

  foreground_send_surface_map_.clear();
  background_send_surface_map_.clear();

  last_incoming_metadata_.reset();
  last_outgoing_metadata_.reset();
  locally_cancelled_share_target_ids_.clear();

  disconnection_timeout_alarms_.clear();

  is_scanning_ = false;
  is_transferring_ = false;
  is_receiving_files_ = false;
  is_sending_files_ = false;
  is_connecting_ = false;
  advertising_power_level_ = PowerLevel::kUnknown;

  certificate_download_during_discovery_timer_.reset();
  rotate_background_advertisement_timer_.reset();
}

void NearbySharingServiceImpl::SendInitialAdapterState(
    NearbySharingService::Observer* observer) {
  RunOnNearbySharingServiceThread(
      "send_initial_adapter_state", [this, observer]() {
        // |observer| may have been removed before the task is run.  This is not
        // sufficient to catch all cases, but without taking some form of
        // ownership of the observer, this is the best we can do.
        if (!observers_.HasObserver(observer)) {
          return;
        }
        observer->OnBluetoothStatusChanged(
            MapAdapterState(context_->GetBluetoothAdapter().IsPresent(),
                            context_->GetBluetoothAdapter().IsPowered()));
        observer->OnLanStatusChanged(
            context_->GetConnectivityManager()->IsLanConnected()
                ? NearbySharingService::Observer::AdapterState::ENABLED
                : NearbySharingService::Observer::AdapterState::DISABLED);
      });
}

void NearbySharingServiceImpl::AddObserver(
    NearbySharingService::Observer* observer) {
  SendInitialAdapterState(observer);
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
  RegisterSendSurface(transfer_callback, discovery_callback, state,
                      BlockedVendorId::kNone, /*disable_wifi_hotspot=*/false,
                      std::move(status_codes_callback));
}
void NearbySharingServiceImpl::RegisterSendSurface(
    TransferUpdateCallback* transfer_callback,
    ShareTargetDiscoveredCallback* discovery_callback, SendSurfaceState state,
    BlockedVendorId blocked_vendor_id, bool disable_wifi_hotspot,
    std::function<void(StatusCodes)> status_codes_callback) {
  RunOnNearbySharingServiceThread(
      "api_register_send_surface",
      [this, transfer_callback, discovery_callback, state, blocked_vendor_id,
       disable_wifi_hotspot,
       status_codes_callback = std::move(status_codes_callback)]() {
        if (state != SendSurfaceState::kForeground &&
            state != SendSurfaceState::kBackground) {
          NL_LOG(ERROR) << __func__ << ": Invalid SendSurfaceState: "
                        << static_cast<int>(state);
          std::move(status_codes_callback)(StatusCodes::kInvalidArgument);
          return;
        }
        NL_DCHECK(transfer_callback);
        NL_DCHECK(discovery_callback);
        NL_LOG(INFO) << __func__
                     << ": RegisterSendSurface is called with state: "
                     << (state == SendSurfaceState::kForeground ? "Foreground"
                                                                : "Background")
                     << ", blocked_vendor_id: "
                     << static_cast<uint8_t>(blocked_vendor_id)
                     << ", disable_wifi_hotspot: " << disable_wifi_hotspot
                     << ", transfer_callback: " << transfer_callback;

        if (foreground_send_surface_map_.contains(transfer_callback) ||
            background_send_surface_map_.contains(transfer_callback)) {
          NL_VLOG(1)
              << __func__
              << ": RegisterSendSurface failed. Already registered for a "
                 "different state.";
          std::move(status_codes_callback)(StatusCodes::kInvalidArgument);
          return;
        }
        BlockedVendorId sending_id = GetSendingVendorId();
        if (ShouldBlockSurfaceRegistration(blocked_vendor_id, sending_id)) {
          NL_LOG(INFO) << __func__
                       << ": RegisterSendSurface failed. Already registered to "
                          "block a different vendor ID "
                       << static_cast<uint32_t>(sending_id);
          std::move(status_codes_callback)(StatusCodes::kInvalidArgument);
          return;
        }
        WrappedShareTargetDiscoveredCallback wrapped_callback(
            discovery_callback, blocked_vendor_id, disable_wifi_hotspot);

        if (state == SendSurfaceState::kForeground) {
          // Only check this error case for foreground senders
          if (!HasAvailableConnectionMediums()) {
            NL_VLOG(1) << __func__ << ": No available connection medium.";
            std::move(status_codes_callback)(
                StatusCodes::kNoAvailableConnectionMedium);
            return;
          }

          foreground_send_surface_map_.insert(
              {transfer_callback, wrapped_callback});
        } else {
          background_send_surface_map_.insert(
              {transfer_callback, wrapped_callback});
        }

        if (is_receiving_files_) {
          InternalUnregisterSendSurface(transfer_callback);
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
          auto& [share_target, attachment_container, transfer_metadata] =
              *last_outgoing_metadata_;
          // TODO(b/341740930): Make sure we absolutely do not deliver updates
          // to blocked targets, and block any interaction with them if the
          // request comes from a surface with the blocked vendor ID.
          wrapped_callback.OnShareTargetDiscovered(share_target);
          transfer_callback->OnTransferUpdate(
              share_target, attachment_container, transfer_metadata);
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
            NL_LOG(INFO) << "Reporting discovered target "
                         << item.second.ToString()
                         << " when registering send surface";
            wrapped_callback.OnShareTargetDiscovered(item.second);
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

        NL_VLOG(1) << "RegisterSendSurface: foreground_send_surface_map_:"
                   << foreground_send_surface_map_.size()
                   << ", background_send_surface_map_:"
                   << background_send_surface_map_.size();

        InvalidateSendSurfaceState();
        std::move(status_codes_callback)(StatusCodes::kOk);
      });
}

void NearbySharingServiceImpl::UnregisterSendSurface(
    TransferUpdateCallback* transfer_callback,
    std::function<void(StatusCodes)> status_codes_callback) {
  RunOnNearbySharingServiceThread(
      "api_unregister_send_surface",
      [this, transfer_callback,
       status_codes_callback = std::move(status_codes_callback)]() {
        StatusCodes status_codes =
            InternalUnregisterSendSurface(transfer_callback);

        NL_VLOG(1) << "UnregisterSendSurface: foreground_send_surface_map_:"
                   << foreground_send_surface_map_.size()
                   << ", background_send_surface_map_:"
                   << background_send_surface_map_.size();

        std::move(status_codes_callback)(status_codes);
      });
}

void NearbySharingServiceImpl::RegisterReceiveSurface(
    TransferUpdateCallback* transfer_callback, ReceiveSurfaceState state,
    BlockedVendorId vendor_id,
    std::function<void(StatusCodes)> status_codes_callback) {
  RunOnNearbySharingServiceThread(
      "api_register_receive_surface",
      [this, transfer_callback, state, vendor_id,
       status_codes_callback = std::move(status_codes_callback)]() {
        if (state != ReceiveSurfaceState::kForeground &&
            state != ReceiveSurfaceState::kBackground) {
          NL_LOG(ERROR) << __func__ << ": Invalid ReceiveSurfaceState: "
                        << static_cast<int>(state);
          std::move(status_codes_callback)(StatusCodes::kInvalidArgument);
          return;
        }
        NL_DCHECK(transfer_callback);

        NL_LOG(INFO) << __func__
                     << ": RegisterReceiveSurface is called with state: "
                     << (state == ReceiveSurfaceState::kForeground
                             ? "Foreground"
                             : "Background")
                     << ", transfer_callback: " << transfer_callback
                     << ", vendor_id: " << static_cast<uint32_t>(vendor_id);

        // Check available mediums.
        if (!HasAvailableConnectionMediums()) {
          NL_VLOG(1) << __func__ << ": No available connection medium.";
          std::move(status_codes_callback)(
              StatusCodes::kNoAvailableConnectionMedium);
          return;
        }
        BlockedVendorId before_registration_vendor_id = GetReceivingVendorId();

        // We specifically allow re-registering without error, so it is clear to
        // caller that the transfer_callback is currently registered.
        if (GetReceiveCallbacksMapFromState(state).contains(
                transfer_callback)) {
          NL_VLOG(1) << __func__
                     << ": transfer callback already registered, ignoring";
          std::move(status_codes_callback)(StatusCodes::kOk);
          return;
        }
        if (foreground_receive_callbacks_map_.contains(transfer_callback) ||
            background_receive_callbacks_map_.contains(transfer_callback)) {
          NL_LOG(ERROR) << __func__
                        << ":  transfer callback already registered but for a "
                           "different state.";
          std::move(status_codes_callback)(StatusCodes::kInvalidArgument);
          return;
        }
        if (ShouldBlockSurfaceRegistration(vendor_id,
                                           before_registration_vendor_id)) {
          // Block alternate vendor ID registration.
          NL_LOG(ERROR) << __func__
                        << ":  disallowing registration of a receive surface "
                           "that has vendor_id "
                        << static_cast<uint32_t>(vendor_id)
                        << " because the current vendor_id is "
                        << static_cast<uint32_t>(GetReceivingVendorId());
          std::move(status_codes_callback)(StatusCodes::kInvalidArgument);
          return;
        }

        // If the receive surface to be registered is a foreground surface, let
        // it catch up with most recent transfer metadata immediately.
        if (state == ReceiveSurfaceState::kForeground &&
            last_incoming_metadata_) {
          auto& [share_target, attachment_container, transfer_metadata] =
              *last_incoming_metadata_;
          transfer_callback->OnTransferUpdate(
              share_target, attachment_container, transfer_metadata);
        }

        GetReceiveCallbacksMapFromState(state).insert(
            {transfer_callback, vendor_id});

        NL_VLOG(1) << __func__ << ": A ReceiveSurface("
                   << ReceiveSurfaceStateToString(state)
                   << ") has been registered";

        if (state == ReceiveSurfaceState::kForeground) {
          if (!IsBluetoothPresent()) {
            NL_LOG(ERROR) << __func__ << ": Bluetooth is not present.";
          } else if (!IsBluetoothPowered()) {
            NL_LOG(WARNING) << __func__ << ": Bluetooth is not powered.";
          } else {
            NL_VLOG(1)
                << __func__ << ": This device's MAC address is: "
                << nearby::device::CanonicalizeBluetoothAddress(
                       context_->GetBluetoothAdapter().GetAddress().value_or(
                           std::array<uint8_t, 6>{}));
          }
        }

        NL_VLOG(1) << "RegisterReceiveSurface: foreground_receive_callbacks_:"
                   << foreground_receive_callbacks_map_.size()
                   << ", background_receive_callbacks_:"
                   << background_receive_callbacks_map_.size();

        InvalidateReceiveSurfaceState();
        std::move(status_codes_callback)(StatusCodes::kOk);
      });
}

void NearbySharingServiceImpl::UnregisterReceiveSurface(
    TransferUpdateCallback* transfer_callback,
    std::function<void(StatusCodes)> status_codes_callback) {
  RunOnNearbySharingServiceThread(
      "api_unregister_receive_surface",
      [this, transfer_callback,
       status_codes_callback = std::move(status_codes_callback)]() {
        StatusCodes status_codes =
            InternalUnregisterReceiveSurface(transfer_callback);
        NL_VLOG(1) << "UnregisterReceiveSurface: foreground_receive_callbacks_:"
                   << foreground_receive_callbacks_map_.size()
                   << ", background_receive_callbacks_:"
                   << background_receive_callbacks_map_.size();
        std::move(status_codes_callback)(status_codes);
        return;
      });
}

void NearbySharingServiceImpl::ClearForegroundReceiveSurfaces(
    std::function<void(StatusCodes)> status_codes_callback) {
  RunOnNearbySharingServiceThread(
      "api_clear_foreground_receive_surfaces",
      [this, status_codes_callback = std::move(status_codes_callback)]() {
        std::vector<TransferUpdateCallback*> fg_receivers;
        for (const auto& callback : foreground_receive_callbacks_map_) {
          fg_receivers.push_back(callback.first);
        }

        StatusCodes status = StatusCodes::kOk;
        for (TransferUpdateCallback* callback : fg_receivers) {
          if (InternalUnregisterReceiveSurface(callback) != StatusCodes::kOk)
            status = StatusCodes::kError;
        }
        std::move(status_codes_callback)(status);
      });
}

bool NearbySharingServiceImpl::IsTransferring() const {
  return is_transferring_;
}

bool NearbySharingServiceImpl::IsScanning() const { return is_scanning_; }

std::string NearbySharingServiceImpl::GetQrCodeUrl() const {
  return service_extension_->GetQrCodeUrl();
}

void NearbySharingServiceImpl::SendAttachments(
    int64_t share_target_id,
    std::unique_ptr<AttachmentContainer> attachment_container,
    std::function<void(StatusCodes)> status_codes_callback) {
  RunOnNearbySharingServiceThread(
      "api_send_attachments",
      [this, share_target_id,
       attachment_container = std::move(attachment_container),
       status_codes_callback = std::move(status_codes_callback)]() mutable {
        if (!is_scanning_) {
          NL_LOG(WARNING) << __func__
                          << ": Failed to send attachments. Not scanning.";
          std::move(status_codes_callback)(StatusCodes::kOutOfOrderApiCall);
          return;
        }

        // |is_scanning_| means at least one send transfer callback.
        NL_DCHECK(!foreground_send_surface_map_.empty() ||
                  !background_send_surface_map_.empty());
        // |is_scanning_| and |is_transferring_| are mutually exclusive.
        NL_DCHECK(!is_transferring_);

        if (!attachment_container || !attachment_container->HasAttachments()) {
          NL_LOG(WARNING) << __func__ << ": No attachments to send.";
          std::move(status_codes_callback)(StatusCodes::kInvalidArgument);
          return;
        }
        for (const FileAttachment& attachment :
             attachment_container->GetFileAttachments()) {
          if (!attachment.file_path() || attachment.file_path()->empty()) {
            NL_LOG(WARNING) << __func__ << ": Got file attachment without path";
            std::move(status_codes_callback)(StatusCodes::kInvalidArgument);
            return;
          }
        }
        // Outgoing connections always announces with contacts visibility.
        std::optional<std::vector<uint8_t>> endpoint_info =
            CreateEndpointInfo(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS,
                               local_device_data_manager_->GetDeviceName());
        if (!endpoint_info) {
          NL_LOG(WARNING) << __func__
                          << ": Could not create local endpoint info.";
          std::move(status_codes_callback)(StatusCodes::kError);
          return;
        }

        OutgoingShareSession* session =
            GetOutgoingShareSession(share_target_id);
        if (!session) {
          NL_LOG(WARNING)
              << __func__
              << ": Failed to send attachments. Unknown ShareTarget.";
          std::move(status_codes_callback)(StatusCodes::kInvalidArgument);
          return;
        }

        session->SetAttachmentContainer(std::move(*attachment_container));

        app_info_->SetActiveFlag();
        // Set session ID.
        session->set_session_id(analytics_recorder_->GenerateNextId());

        // Log analytics event of sending start.
        analytics_recorder_->NewSendStart(
            session->session_id(),
            /*transfer_position=*/GetConnectedShareTargetPos(),
            /*concurrent_connections=*/GetConnectedShareTargetCount(),
            session->share_target());

        OnTransferStarted(/*is_incoming=*/false);
        is_connecting_ = true;
        InvalidateSendSurfaceState();

        // Send process initialized successfully, from now on status updated
        // will be sent out via OnOutgoingTransferUpdate().
        session->UpdateTransferMetadata(
            TransferMetadataBuilder()
                .set_status(TransferMetadata::Status::kConnecting)
                .build());

        CreatePayloads(
            *session, [this, endpoint_info = std::move(*endpoint_info)](
                          OutgoingShareSession& session, bool success) {
              OnCreatePayloads(std::move(endpoint_info), session, success);
            });

        std::move(status_codes_callback)(StatusCodes::kOk);
      });
}

bool NearbySharingServiceImpl::OutgoingSessionAccept(
    OutgoingShareSession& session) {
  return session.AcceptTransfer(
      absl::bind_front(&NearbySharingServiceImpl::OnReceiveConnectionResponse,
                       this, session.share_target().id));
}

void NearbySharingServiceImpl::Accept(
    int64_t share_target_id,
    std::function<void(StatusCodes status_codes)> status_codes_callback) {
  RunOnNearbySharingServiceThread(
      "api_accept",
      [this, share_target_id,
       status_codes_callback = std::move(status_codes_callback)]() {
        IncomingShareSession* incoming_session =
            GetIncomingShareSession(share_target_id);
        if (incoming_session != nullptr) {
          // Incoming session.
          bool accept_success = incoming_session->AcceptTransfer(
              context_->GetClock(),
              absl::bind_front(
                  &NearbySharingServiceImpl::IncomingPayloadTransferUpdate,
                  this));
          std::move(status_codes_callback)(
              accept_success ? StatusCodes::kOk
                             : StatusCodes::kOutOfOrderApiCall);
          return;
        }
        OutgoingShareSession* outgoing_session =
            GetOutgoingShareSession(share_target_id);
        if (outgoing_session != nullptr) {
          // Outgoing session.
          bool accept_success = OutgoingSessionAccept(*outgoing_session);
          std::move(status_codes_callback)(
              accept_success ? StatusCodes::kOk
                             : StatusCodes::kOutOfOrderApiCall);
          return;
        }
        NL_LOG(WARNING) << __func__
                        << ": Accept invoked for unknown share target";
        std::move(status_codes_callback)(StatusCodes::kInvalidArgument);
      });
}

void NearbySharingServiceImpl::Reject(
    int64_t share_target_id,
    std::function<void(StatusCodes status_codes)> status_codes_callback) {
  RunOnNearbySharingServiceThread(
      "api_reject",
      [this, share_target_id,
       status_codes_callback = std::move(status_codes_callback)]() {
        ShareSession* session = GetShareSession(share_target_id);
        if (session == nullptr) {
          NL_LOG(WARNING) << __func__
                          << ": Reject invoked for unknown share target";
          std::move(status_codes_callback)(StatusCodes::kInvalidArgument);
          return;
        }
        if (!session->IsConnected()) {
          NL_LOG(WARNING) << __func__
                          << ": Reject invoked for unconnected share target";
          std::move(status_codes_callback)(StatusCodes::kOutOfOrderApiCall);
          return;
        }
        // Log analytics event of responding to introduction.
        analytics_recorder_->NewRespondToIntroduction(
            ResponseToIntroduction::REJECT_INTRODUCTION, session->session_id());

        RunOnNearbySharingServiceThreadDelayed(
            "incoming_rejection_delay", kIncomingRejectionDelay,
            [this, share_target_id]() { CloseConnection(share_target_id); });
        // kRejected status already sent below, no need to send on disconnect.
        session->set_disconnect_status(TransferMetadata::Status::kUnknown);

        session->WriteResponseFrame(ConnectionResponseFrame::REJECT);
        NL_VLOG(1) << __func__
                   << ": Successfully wrote a rejection response frame";

        session->UpdateTransferMetadata(
            TransferMetadataBuilder()
                .set_status(TransferMetadata::Status::kRejected)
                .build());

        std::move(status_codes_callback)(StatusCodes::kOk);
      });
}

void NearbySharingServiceImpl::Cancel(
    int64_t share_target_id,
    std::function<void(StatusCodes status_codes)> status_codes_callback) {
  RunOnAnyThread("api_cancel", [this, share_target_id,
                                status_codes_callback =
                                    std::move(status_codes_callback)]() {
    NL_LOG(INFO) << __func__ << ": User canceled transfer";
    if (locally_cancelled_share_target_ids_.contains(share_target_id)) {
      NL_LOG(WARNING) << __func__ << ": Cancel is called again.";
      status_codes_callback(StatusCodes::kOutOfOrderApiCall);
      return;
    }
    locally_cancelled_share_target_ids_.insert(share_target_id);
    DoCancel(share_target_id, std::move(status_codes_callback),
             /*is_initiator_of_cancellation=*/true);
  });
}

// Note: |share_target| is intentionally passed by value. A share target
// reference could likely be invalidated by the owner during the multistep
// cancellation process.
void NearbySharingServiceImpl::DoCancel(
    int64_t share_target_id,
    std::function<void(StatusCodes status_codes)> status_codes_callback,
    bool is_initiator_of_cancellation) {
  ShareSession* session = GetShareSession(share_target_id);
  if (session == nullptr) {
    NL_LOG(WARNING) << __func__ << ": Cancel invoked for unknown share target";
    std::move(status_codes_callback)(StatusCodes::kInvalidArgument);
    return;
  }

  // For metrics.
  all_cancelled_share_target_ids_.insert(share_target_id);

  // Cancel all ongoing payload transfers before invoking the transfer update
  // callback. Invoking the transfer update callback first could result in
  // payload cleanup before we have a chance to cancel the payload via Nearby
  // Connections, and the payload tracker might not receive the expected
  // cancellation signals. Also, note that there might not be any ongoing
  // payload transfer, for example, if a connection has not been established
  // yet.
  session->CancelPayloads();

  // Inform the user that the transfer has been cancelled before disconnecting
  // because subsequent disconnections might be interpreted as failure.
  // UpdateTransferMetadata will ignore subsequent statuses in favor of this
  // cancelled status. Note that the transfer update callback might have already
  // been invoked as a result of the payload cancellations above, but again,
  // superfluous status updates are handled gracefully by the
  // UpdateTransferMetadata.
  session->UpdateTransferMetadata(
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kCancelled)
          .build());

  // If a connection exists, close the connection. Note: The initiator of a
  // cancellation waits for a short delay before closing the connection,
  // allowing for final processing by the other device. Otherwise, disconnect
  // from endpoint id directly. Note: A share attempt can be cancelled by the
  // user before a connection is fully established, in which case,
  // session->connection() will be null.
  if (session->IsConnected()) {
    NL_LOG(INFO) << "Disconnect fully established endpoint id:"
                 << session->endpoint_id();
    if (is_initiator_of_cancellation) {
      // kCancelled status already sent above, no need to send on disconnect.
      session->set_disconnect_status(TransferMetadata::Status::kUnknown);

      RunOnNearbySharingServiceThreadDelayed(
          "initiator_cancel_delay", kInitiatorCancelDelay,
          [this, share_target_id]() {
            NL_LOG(INFO) << "Close connection after cancellation delay.";
            CloseConnection(share_target_id);
          });

      session->WriteCancelFrame();
    } else {
      session->connection()->Close();
    }
  } else {
    NL_LOG(INFO) << "Disconnect endpoint id:" << session->endpoint_id();
    nearby_connections_manager_->Disconnect(session->endpoint_id());
    UnregisterShareTarget(share_target_id);
  }

  std::move(status_codes_callback)(StatusCodes::kOk);
}

bool NearbySharingServiceImpl::DidLocalUserCancelTransfer(
    int64_t share_target_id) {
  return absl::c_linear_search(locally_cancelled_share_target_ids_,
                               share_target_id);
}

void NearbySharingServiceImpl::SetVisibility(
    proto::DeviceVisibility visibility, absl::Duration expiration,
    absl::AnyInvocable<void(StatusCodes status_code) &&> callback) {
  RunOnNearbySharingServiceThread(
      "api_set_visibility",
      [this, visibility, expiration, callback = std::move(callback)]() mutable {
        NL_LOG(INFO) << __func__ << ": SetVisibility is called";
        if (account_manager_.GetCurrentAccount() == std::nullopt) {
          switch (visibility) {
            case proto::DeviceVisibility::DEVICE_VISIBILITY_EVERYONE:
            case proto::DeviceVisibility::DEVICE_VISIBILITY_HIDDEN:
              break;
            default:
              NL_LOG(WARNING)
                  << __func__
                  << ": SetVisibility failed for visibility: " << visibility
                  << ". No account.";
              std::move(callback)(StatusCodes::kInvalidArgument);
              return;
          }
        }
        settings_->SetVisibility(visibility, expiration);
        std::move(callback)(StatusCodes::kOk);
      });
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

  app_info_->SetActiveFlag();

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
  int64_t placeholder_share_target_id = placeholder_share_target.id;
  IncomingShareSession& session = CreateIncomingShareSession(
      placeholder_share_target, endpoint_id, /*certificate=*/std::nullopt);
  session.set_session_id(analytics_recorder_->GenerateNextId());
  session.OnConnected(context_->GetClock()->Now(),
                      nearby_connections_manager_.get(), connection);
  connection->SetDisconnectionListener([this, placeholder_share_target_id]() {
    OnConnectionDisconnected(placeholder_share_target_id);
  });

  std::unique_ptr<Advertisement> advertisement =
      DecodeAdvertisement(endpoint_info);
  OnIncomingAdvertisementDecoded(endpoint_id, session,
                                 std::move(advertisement));
}

NearbySharingService::StatusCodes
NearbySharingServiceImpl::InternalUnregisterSendSurface(
    TransferUpdateCallback* transfer_callback) {
  NL_DCHECK(transfer_callback);
  NL_LOG(INFO) << __func__ << ": UnregisterSendSurface is called"
               << ", transfer_callback: " << transfer_callback;

  if (!foreground_send_surface_map_.contains(transfer_callback) &&
      !background_send_surface_map_.contains(transfer_callback)) {
    NL_VLOG(1)
        << __func__
        << ": unregisterSendSurface failed. Unknown TransferUpdateCallback";
    return StatusCodes::kError;
  }

  if (!foreground_send_surface_map_.empty() && last_outgoing_metadata_ &&
      std::get<2>(*last_outgoing_metadata_).is_final_status()) {
    // We already saw the final status in the foreground
    // Nullify it so the next time the user opens sharing, it starts the UI from
    // the beginning
    last_outgoing_metadata_.reset();
  }

  SendSurfaceState state = SendSurfaceState::kUnknown;
  if (foreground_send_surface_map_.contains(transfer_callback)) {
    foreground_send_surface_map_.erase(transfer_callback);
    state = SendSurfaceState::kForeground;
  } else {
    background_send_surface_map_.erase(transfer_callback);
    state = SendSurfaceState::kBackground;
  }

  // Displays the most recent payload status processed by foreground surfaces on
  // background surfaces.
  if (foreground_send_surface_map_.empty() && last_outgoing_metadata_) {
    auto& [share_target, attachment_container, transfer_metadata] =
        *last_outgoing_metadata_;
    for (auto& background_transfer_callback : background_send_surface_map_) {
      background_transfer_callback.first->OnTransferUpdate(
          share_target, attachment_container, transfer_metadata);
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
      foreground_receive_callbacks_map_.contains(transfer_callback);
  bool is_background =
      background_receive_callbacks_map_.contains(transfer_callback);
  if (!is_foreground && !is_background) {
    NL_VLOG(1) << __func__
               << ": Unknown transfer callback was un-registered, ignoring.";
    // We intentionally allow this be successful so the caller can be sure
    // they are not registered anymore.
    return StatusCodes::kOk;
  }

  if (!foreground_receive_callbacks_map_.empty() && last_incoming_metadata_ &&
      std::get<2>(*last_incoming_metadata_).is_final_status()) {
    // We already saw the final status in the foreground.
    // Nullify it so the next time the user opens sharing, it starts the UI from
    // the beginning
    last_incoming_metadata_.reset();
  }

  if (is_foreground) {
    foreground_receive_callbacks_map_.erase(transfer_callback);
  } else {
    background_receive_callbacks_map_.erase(transfer_callback);
  }

  // Displays the most recent payload status processed by foreground surfaces on
  // background surface.
  if (foreground_receive_callbacks_map_.empty() && last_incoming_metadata_) {
    auto& [share_target, attachment_container, transfer_metadata] =
        *last_incoming_metadata_;
    for (auto& background_callback : background_receive_callbacks_map_) {
      background_callback.first->OnTransferUpdate(
          share_target, attachment_container, transfer_metadata);
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
  sstream << "  IsConnecting: " << is_connecting_ << std::endl;
  sstream << "  IsTransferring: " << is_transferring_ << std::endl;
  sstream << "  IsSendingFile: " << is_sending_files_ << std::endl;
  sstream << "  IsReceivingFile: " << is_receiving_files_ << std::endl;

  sstream << "  IsScreenLocked: " << device_info_.IsScreenLocked() << std::endl;
  sstream << "  IsBluetoothPresent: " << IsBluetoothPresent() << std::endl;
  sstream << "  IsBluetoothPowered: " << IsBluetoothPowered() << std::endl;
  sstream << "  IsExtendedAdvertisingSupported: "
          << IsExtendedAdvertisingSupported() << std::endl;
  sstream << "  Registered send surfaces: " << std::endl;
  for (const auto& [transfer_callback, discovery_callback] :
       foreground_send_surface_map_) {
    sstream << absl::StrFormat(
                   "    Foreground, vendor ID: %d",
                   static_cast<uint8_t>(discovery_callback.BlockedVendorId()))
            << std::endl;
  }
  for (const auto& [transfer_callback, discovery_callback] :
       background_send_surface_map_) {
    sstream << absl::StrFormat(
                   "    Background, vendor ID: %d",
                   static_cast<uint8_t>(discovery_callback.BlockedVendorId()))
            << std::endl;
  }
  sstream << "  Registered receive surfaces: " << std::endl;
  for (const auto& [callback, vendor_id] : foreground_receive_callbacks_map_) {
    sstream << absl::StrFormat("    Foreground, vendor ID: %d",
                               static_cast<uint8_t>(vendor_id))
            << std::endl;
  }
  for (const auto& [callback, vendor_id] : background_receive_callbacks_map_) {
    sstream << absl::StrFormat("    Background, vendor ID: %d",
                               static_cast<uint8_t>(vendor_id))
            << std::endl;
  }
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
  if (key == prefs::kNearbySharingDataUsageName) {
    DataUsage data_usage = static_cast<DataUsage>(data.value.as_int64);
    OnDataUsageChanged(data_usage);
  } else if (key == prefs::kNearbySharingCustomSavePath) {
    absl::string_view custom_save_path = data.value.as_string;
    OnCustomSavePathChanged(custom_save_path);
  } else if (key == prefs::kNearbySharingBackgroundVisibilityName) {
    DeviceVisibility visibility =
        static_cast<DeviceVisibility>(data.value.as_int64);
    OnVisibilityChanged(visibility);
  }
}

void NearbySharingServiceImpl::OnDataUsageChanged(DataUsage data_usage) {
  RunOnNearbySharingServiceThread(
      "on_data_usage_changed", [this, data_usage]() {
        NL_LOG(INFO) << __func__ << ": Nearby sharing data usage changed to "
                     << DataUsage_Name(data_usage);
        StopAdvertisingAndInvalidateSurfaceState();
      });
}

void NearbySharingServiceImpl::OnCustomSavePathChanged(
    absl::string_view custom_save_path) {
  RunOnNearbySharingServiceThread(
      "on_custom_save_path_changed",
      [this, custom_save_path = std::string(custom_save_path)]() {
        NL_LOG(INFO) << __func__
                     << ": Nearby sharing custom save path changed to "
                     << custom_save_path;
        nearby_connections_manager_->SetCustomSavePath(custom_save_path);
      });
}

void NearbySharingServiceImpl::OnVisibilityChanged(
    DeviceVisibility visibility) {
  RunOnNearbySharingServiceThread(
      "on_visibility_changed", [this, visibility]() {
        NL_LOG(INFO) << __func__ << ": Nearby sharing visibility changed to "
                     << DeviceVisibility_Name(visibility);
        StopAdvertisingAndInvalidateSurfaceState();
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
  RunOnNearbySharingServiceThread("on-private-certificates-changed", [this]() {
    StopAdvertisingAndInvalidateSurfaceState();
  });
}

void NearbySharingServiceImpl::OnLoginSucceeded(absl::string_view account_id) {
  RunOnNearbySharingServiceThread("on_login_succeeded", [this]() {
    NL_LOG(INFO) << __func__ << ": Account login.";

    ResetAllSettings(/*logout=*/false);
  });
}

void NearbySharingServiceImpl::OnLogoutSucceeded(absl::string_view account_id,
                                                 bool credential_error) {
  RunOnNearbySharingServiceThread(
      "on_logout_succeeded", [this, credential_error]() {
        NL_LOG(INFO) << __func__ << ": Account logout.";

        // Reset all settings.
        ResetAllSettings(/*logout=*/true);
        if (credential_error) {
          for (auto& observer : observers_.GetObservers()) {
            observer->OnCredentialError();
          }
        }
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
      [this, endpoint_id = std::string(endpoint_id),
       endpoint_info_copy = std::move(endpoint_info_copy)]() {
        AddEndpointDiscoveryEvent([this, endpoint_id, endpoint_info_copy]() {
          HandleEndpointDiscovered(endpoint_id, endpoint_info_copy);
        });
      });
}

void NearbySharingServiceImpl::OnEndpointLost(absl::string_view endpoint_id) {
  RunOnNearbySharingServiceThread(
      "on_endpoint_lost", [this, endpoint_id = std::string(endpoint_id)]() {
        AddEndpointDiscoveryEvent(
            [this, endpoint_id]() { HandleEndpointLost(endpoint_id); });
      });
}

void NearbySharingServiceImpl::OnLockStateChanged(bool locked) {
  RunOnNearbySharingServiceThread("on_lock_state_changed", [this, locked]() {
    NL_VLOG(1) << __func__ << ": Screen lock state changed. (" << locked << ")";
    is_screen_locked_ = locked;
    InvalidateSurfaceState();
  });
}

void NearbySharingServiceImpl::AdapterPresentChanged(
    sharing::api::BluetoothAdapter* adapter, bool present) {
  RunOnNearbySharingServiceThread(
      "bt_adapter_present_changed", [this, adapter, present]() {
        NL_VLOG(1) << __func__ << ": Bluetooth adapter present state changed. ("
                   << present << ")";
        NearbySharingService::Observer::AdapterState state =
            MapAdapterState(present, adapter->IsPowered());
        for (auto& observer : observers_.GetObservers()) {
          observer->OnBluetoothStatusChanged(state);
        }
        InvalidateSurfaceState();
      });
}

void NearbySharingServiceImpl::AdapterPoweredChanged(
    sharing::api::BluetoothAdapter* adapter, bool powered) {
  RunOnNearbySharingServiceThread(
      "bt_adapter_power_changed", [this, adapter, powered]() {
        NL_VLOG(1) << __func__ << ": Bluetooth adapter power state changed. ("
                   << powered << ")";
        NearbySharingService::Observer::AdapterState state =
            MapAdapterState(adapter->IsPresent(), powered);
        for (auto& observer : observers_.GetObservers()) {
          observer->OnBluetoothStatusChanged(state);
        }
        InvalidateSurfaceState();
      });
}

void NearbySharingServiceImpl::AdapterPresentChanged(
    sharing::api::WifiAdapter* adapter, bool present) {
  RunOnNearbySharingServiceThread(
      "wifi_adapter_present_changed", [this, adapter, present]() {
        NL_VLOG(1) << __func__ << ": Wifi adapter present state changed. ("
                   << present << ")";
        NearbySharingService::Observer::AdapterState state =
            MapAdapterState(present, adapter->IsPowered());
        for (auto& observer : observers_.GetObservers()) {
          observer->OnWifiStatusChanged(state);
        }
        InvalidateSurfaceState();
      });
}

void NearbySharingServiceImpl::AdapterPoweredChanged(
    sharing::api::WifiAdapter* adapter, bool powered) {
  RunOnNearbySharingServiceThread(
      "wifi_adapter_power_changed", [this, adapter, powered]() {
        NL_VLOG(1) << __func__ << ": Wifi adapter power state changed. ("
                   << powered << ")";
        NearbySharingService::Observer::AdapterState state =
            MapAdapterState(adapter->IsPresent(), powered);
        for (auto& observer : observers_.GetObservers()) {
          observer->OnWifiStatusChanged(state);
        }
        InvalidateSurfaceState();
      });
}

void NearbySharingServiceImpl::HardwareErrorReported(
    NearbyFastInitiation* fast_init) {
  RunOnNearbySharingServiceThread("hardware_error_reported", [this]() {
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

BlockedVendorId NearbySharingServiceImpl::GetReceivingVendorId() const {
  // Prefer a vendor ID provided by a foreground surface.
  auto fg_vendor_it =
      std::find_if(foreground_receive_callbacks_map_.begin(),
                   foreground_receive_callbacks_map_.end(),
                   [](const auto& receive_callback) {
                     return receive_callback.second != BlockedVendorId::kNone;
                   });
  if (fg_vendor_it != foreground_receive_callbacks_map_.end()) {
    return fg_vendor_it->second;
  }
  // Look through background surfaces.
  auto bg_vendor_it =
      std::find_if(background_receive_callbacks_map_.begin(),
                   background_receive_callbacks_map_.end(),
                   [](const auto& receive_callback) {
                     return receive_callback.second != BlockedVendorId::kNone;
                   });
  if (bg_vendor_it != background_receive_callbacks_map_.end()) {
    return bg_vendor_it->second;
  }
  return BlockedVendorId::kNone;
}

BlockedVendorId NearbySharingServiceImpl::GetSendingVendorId() const {
  // Prefer a vendor ID provided by a foreground surface.
  auto fg_vendor_it = std::find_if(
      foreground_send_surface_map_.begin(), foreground_send_surface_map_.end(),
      [](const auto& send_surface) {
        return send_surface.second.BlockedVendorId() != BlockedVendorId::kNone;
      });
  if (fg_vendor_it != foreground_send_surface_map_.end()) {
    return fg_vendor_it->second.BlockedVendorId();
  }
  // Look through background surfaces.
  auto bg_vendor_it = std::find_if(
      background_send_surface_map_.begin(), background_send_surface_map_.end(),
      [](const auto& send_surface) {
        return send_surface.second.BlockedVendorId() != BlockedVendorId::kNone;
      });
  if (bg_vendor_it != background_send_surface_map_.end()) {
    return bg_vendor_it->second.BlockedVendorId();
  }
  return BlockedVendorId::kNone;
}

bool NearbySharingServiceImpl::GetDisableWifiHotspotState() const {
  for (const auto& it : foreground_send_surface_map_) {
    if (it.second.disable_wifi_hotspot()) {
      return true;
    }
  }
  return false;
}

absl::flat_hash_map<TransferUpdateCallback*, BlockedVendorId>&
NearbySharingServiceImpl::GetReceiveCallbacksMapFromState(
    ReceiveSurfaceState state) {
  switch (state) {
    case ReceiveSurfaceState::kForeground:
      return foreground_receive_callbacks_map_;
    case ReceiveSurfaceState::kBackground:
      return background_receive_callbacks_map_;
    case ReceiveSurfaceState::kUnknown:
      return foreground_receive_callbacks_map_;
  }
}

bool NearbySharingServiceImpl::IsVisibleInBackground(
    DeviceVisibility visibility) {
  return visibility != DeviceVisibility::DEVICE_VISIBILITY_HIDDEN &&
         visibility != DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED;
}

std::optional<std::vector<uint8_t>>
NearbySharingServiceImpl::CreateEndpointInfo(
    DeviceVisibility visibility,
    const std::optional<std::string>& device_name) const {
  std::vector<uint8_t> salt;
  std::vector<uint8_t> encrypted_key;

  if (account_manager_.GetCurrentAccount().has_value()) {
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
      std::move(salt), std::move(encrypted_key), device_type, device_name,
      visibility == DeviceVisibility::DEVICE_VISIBILITY_EVERYONE
          ? static_cast<uint8_t>(GetReceivingVendorId())
          : static_cast<uint8_t>(BlockedVendorId::kNone));
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
      [this]() { OnStartFastInitiationAdvertising(); },
      [this]() { OnStartFastInitiationAdvertisingError(); });
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
      [this]() { OnStopFastInitiationAdvertising(); });
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
  VLOG(1) << __func__ << ": endpoint_id=" << endpoint_id
          << ", endpoint_info=" << nearby::utils::HexEncode(endpoint_info)
          << " time: " << context_->GetClock()->Now();
  if (!is_scanning_) {
    NL_VLOG(1)
        << __func__
        << ": Ignoring discovered endpoint because we're no longer scanning";
    FinishEndpointDiscoveryEvent();
    return;
  }

  std::unique_ptr<Advertisement> advertisement =
      DecodeAdvertisement(endpoint_info);
  if (!advertisement) {
    NL_LOG(WARNING) << __func__
                    << ": Failed to parse discovered advertisement.";
    FinishEndpointDiscoveryEvent();
    return;
  }

  // Now we will report endpoints met before in NearbyConnectionsManager.
  // Check outgoingShareSessionMap first and pass the same shareTarget if we
  // found one.

  // Looking for the ShareTarget based on endpoint id.
  if (!NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_sharing_feature::
              kApplyEndpointsDedup)) {
    if (outgoing_share_target_map_.find(endpoint_id) !=
        outgoing_share_target_map_.end()) {
      LOG(INFO) << __func__ << ": Ignoring endpoint_id: " << endpoint_id
                << " because it is already in the "
                << "outgoingShareTargetMap.";
      FinishEndpointDiscoveryEvent();
      return;
    }
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
        RunOnNearbySharingServiceThread(
            "outgoing_decrypted_certificate",
            [this, endpoint_id_copy, endpoint_info_copy, advertisement_copy,
             decrypted_public_certificate]() {
              LOG(INFO) << __func__ << ": Decrypted public certificate";
              OnOutgoingDecryptedCertificate(
                  endpoint_id_copy, endpoint_info_copy, advertisement_copy,
                  decrypted_public_certificate);
            });
      });
}

void NearbySharingServiceImpl::HandleEndpointLost(
    absl::string_view endpoint_id) {
  VLOG(1) << __func__ << ": endpoint_id=" << endpoint_id
          << " time: " << context_->GetClock()->Now();

  if (!is_scanning_) {
    NL_VLOG(1) << __func__
               << ": Ignoring lost endpoint because we're no longer scanning";
    FinishEndpointDiscoveryEvent();
    return;
  }

  discovered_advertisements_to_retry_map_.erase(endpoint_id);
  discovered_advertisements_retried_set_.erase(endpoint_id);
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_sharing_feature::
              kApplyEndpointsDedup)) {
    MoveToDiscoveryCache(endpoint_id);
  } else {
    RemoveOutgoingShareTargetAndReportLost(endpoint_id);
  }
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

void NearbySharingServiceImpl::OnOutgoingDecryptedCertificate(
    absl::string_view endpoint_id, absl::Span<const uint8_t> endpoint_info,
    const Advertisement& advertisement,
    std::optional<NearbyShareDecryptedPublicCertificate> certificate) {
  bool apply_endpoints_dedup = NearbyFlags::GetInstance().GetBoolFlag(
      config_package_nearby::nearby_sharing_feature::kApplyEndpointsDedup);
  if (!apply_endpoints_dedup) {
    // Check again for this endpoint id, to avoid race conditions.
    if (outgoing_share_target_map_.find(endpoint_id) !=
        outgoing_share_target_map_.end()) {
      LOG(INFO) << __func__ << ": Ignoring endpoint_id: " << endpoint_id
                << " because it is already in the "
                << "outgoingShareTargetMap.";
      FinishEndpointDiscoveryEvent();
      return;
    }
  }

  // The certificate provides the device name, in order to create a ShareTarget
  // to represent this remote device.
  std::optional<ShareTarget> share_target =
      CreateShareTarget(endpoint_id, advertisement, certificate,
                        /*is_incoming=*/false);
  if (!share_target.has_value()) {
    if (discovered_advertisements_retried_set_.contains(endpoint_id)) {
      LOG(INFO)
          << __func__
          << ": Don't try to download public certificates again for endpoint="
          << endpoint_id;
      FinishEndpointDiscoveryEvent();
      return;
    }

    LOG(INFO) << __func__
              << ": Failed to convert discovered advertisement to share "
              << "target. Ignoring endpoint: " << endpoint_id
              << " until next certificate download.";
    std::vector<uint8_t> endpoint_info_data(endpoint_info.begin(),
                                            endpoint_info.end());

    discovered_advertisements_to_retry_map_[endpoint_id] = endpoint_info_data;
    FinishEndpointDiscoveryEvent();
    return;
  }
  if (apply_endpoints_dedup) {
    if (FindDuplicateInOutgoingShareTargets(endpoint_id, *share_target)) {
      DeduplicateInOutgoingShareTarget(*share_target, endpoint_id,
                                       std::move(certificate));
      FinishEndpointDiscoveryEvent();
      return;
    }
    if (FindDuplicateInDiscoveryCache(endpoint_id, *share_target)) {
      DeDuplicateInDiscoveryCache(*share_target, endpoint_id,
                                  std::move(certificate));
      FinishEndpointDiscoveryEvent();
      return;
    }
  }

  VLOG(1) << __func__ << ": Adding (endpoint_id=" << endpoint_id
          << ", share_target_id=" << share_target->id
          << ") to outgoing share target map";
  outgoing_share_target_map_.insert_or_assign(endpoint_id, *share_target);
  CreateOutgoingShareSession(*share_target, endpoint_id,
                             std::move(certificate));

  // Update the endpoint id for the share target.
  LOG(INFO) << __func__ << ": An endpoint: " << endpoint_id
            << " has been discovered, with an advertisement "
               "containing a valid share target with id: "
            << share_target->id;

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
  VLOG(1) << __func__ << ": There are "
          << (foreground_send_surface_map_.size() +
              background_send_surface_map_.size())
          << " discovery callbacks be called.";

  for (auto& entry : foreground_send_surface_map_) {
    entry.second.OnShareTargetDiscovered(*share_target);
  }
  for (auto& entry : background_send_surface_map_) {
    entry.second.OnShareTargetDiscovered(*share_target);
  }

  VLOG(1) << __func__ << ": Reported OnShareTargetDiscovered at timestamp: "
          << (context_->GetClock()->Now() - scanning_start_timestamp_);

  FinishEndpointDiscoveryEvent();
}

void NearbySharingServiceImpl::ScheduleCertificateDownloadDuringDiscovery(
    size_t attempt_count) {
  if (attempt_count >= kMaxCertificateDownloadsDuringDiscovery) {
    return;
  }

  certificate_download_during_discovery_timer_ = std::make_unique<ThreadTimer>(
      *service_thread_, "certificate_download_during_discovery_timer",
      kCertificateDownloadDuringDiscoveryPeriod, [this, attempt_count]() {
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

  if (is_transferring_ || is_connecting_) {
    StopScanning();
    NL_VLOG(1)
        << __func__
        << ": Stopping discovery because we're currently in the midst of a "
           "transfer.";
    return;
  }

  if (foreground_send_surface_map_.empty()) {
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

  if (is_transferring_ || is_connecting_) {
    StopFastInitiationAdvertising();
    NL_VLOG(1) << __func__
               << ": Stopping fast initiation advertising because we're "
                  "currently in the midst of a "
                  "transfer.";
    return;
  }

  if (foreground_send_surface_map_.empty()) {
    StopFastInitiationAdvertising();
    NL_VLOG(1)
        << __func__
        << ": Stopping fast initiation advertising because no foreground send "
           "surface is registered.";
    return;
  }

  StartFastInitiationAdvertising();
}

void NearbySharingServiceImpl::InvalidateReceiveSurfaceState() {
  InvalidateAdvertisingState();
  InvalidateFastInitiationScanning();
}

void NearbySharingServiceImpl::InvalidateAdvertisingState() {
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

  if (is_transferring_) {
    StopAdvertising();
    NL_VLOG(1)
        << __func__
        << ": Stopping advertising because we're currently in the midst of "
           "a transfer.";
    return;
  }

  if (foreground_receive_callbacks_map_.empty() &&
      background_receive_callbacks_map_.empty()) {
    StopAdvertising();
    NL_VLOG(1)
        << __func__
        << ": Stopping advertising because no receive surface is registered.";
    return;
  }

  // We should only advertise if the user has set the visibility to something
  // other than HIDDEN or UNSPECIFIED.
  if (!IsVisibleInBackground(settings_->GetVisibility())) {
    StopAdvertising();
    NL_VLOG(1) << __func__
               << ": Stopping advertising because device is visible to NO_ONE.";
    return;
  }

  PowerLevel power_level;
  if (!foreground_receive_callbacks_map_.empty()) {
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
  DeviceVisibility visibility = settings_->GetVisibility();
  if (visibility == DeviceVisibility::DEVICE_VISIBILITY_EVERYONE) {
    device_name = local_device_data_manager_->GetDeviceName();
  }

  // Starts advertising through Nearby Connections. Caller is expected to ensure
  // |listener| remains valid until StopAdvertising is called.
  std::optional<std::vector<uint8_t>> endpoint_info =
      CreateEndpointInfo(visibility, device_name);
  if (!endpoint_info) {
    NL_VLOG(1) << __func__
               << ": Unable to advertise since could not parse the "
                  "endpoint info from the advertisement.";
    return;
  }
  if (device_name.has_value()) {
    for (auto& observer : observers_.GetObservers()) {
      observer->OnHighVisibilityChangeRequested();
    }
  }

  advertising_session_id_ = analytics_recorder_->GenerateNextId();

  VLOG(1) << "Advertising endpoint_info: "
          << absl::BytesToHexString(absl::string_view(
                 reinterpret_cast<const char*>(endpoint_info->data()),
                 endpoint_info->size()));

  nearby_connections_manager_->StartAdvertising(
      *endpoint_info,
      /*listener=*/this, power_level, data_usage,
      visibility == DeviceVisibility::DEVICE_VISIBILITY_EVERYONE,
      [this, visibility, data_usage](Status status) {
        // Log analytics event of advertising start.
        analytics_recorder_->NewAdvertiseDevicePresenceStart(
            advertising_session_id_, visibility,
            status == Status::kSuccess ? SessionStatus::SUCCEEDED_SESSION_STATUS
                                       : SessionStatus::FAILED_SESSION_STATUS,
            data_usage, std::nullopt);

        OnStartAdvertisingResult(
            visibility == DeviceVisibility::DEVICE_VISIBILITY_EVERYONE, status);
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

  nearby_connections_manager_->StopAdvertising([this](Status status) {
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
  NL_DCHECK(!is_screen_locked_);
  NL_DCHECK(HasAvailableConnectionMediums());
  NL_DCHECK(!foreground_send_surface_map_.empty());

  if (is_scanning_) {
    NL_VLOG(1) << __func__ << ": We're currently scanning, ignoring.";
    return;
  }

  scanning_start_timestamp_ = context_->GetClock()->Now();
  share_foreground_send_surface_start_timestamp_ = absl::InfinitePast();
  is_scanning_ = true;
  InvalidateReceiveSurfaceState();

  ClearOutgoingShareSessionMap();
  discovery_cache_.clear();
  discovered_advertisements_to_retry_map_.clear();
  discovered_advertisements_retried_set_.clear();

  scanning_session_id_ = analytics_recorder_->GenerateNextId();

  nearby_connections_manager_->StartDiscovery(
      /*listener=*/this, settings_->GetDataUsage(), [this](Status status) {
        // Log analytics event of starting discovery.
        analytics::AnalyticsInformation analytics_information;
        analytics_information.send_surface_state =
            foreground_send_surface_map_.empty()
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

  certificate_download_during_discovery_timer_.reset();
  discovered_advertisements_to_retry_map_.clear();
  discovered_advertisements_retried_set_.clear();

  TriggerDiscoveryCacheExpiryTimers();

  // Note: We don't know if we stopped scanning in preparation to send a file,
  // or we stopped because the user left the page. We'll invalidate after a
  // short delay.

  RunOnNearbySharingServiceThreadDelayed(
      "invalidate_delay", kInvalidateDelay,
      [this]() { InvalidateSurfaceState(); });

  NL_VLOG(1) << __func__ << ": Scanning has stopped.";
  return StatusCodes::kOk;
}

void NearbySharingServiceImpl::StopAdvertisingAndInvalidateSurfaceState() {
  if (advertising_power_level_ != PowerLevel::kUnknown) StopAdvertising();
  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::InvalidateFastInitiationScanning() {
  bool is_hardware_offloading_supported =
      IsBluetoothPresent() && nearby_fast_initiation_->IsScanOffloadSupported();

  // Hardware offloading support is computed when the bluetooth adapter becomes
  // available. We set the hardware supported state on |settings_| to notify the
  // UI of state changes. InvalidateFastInitiationScanning gets triggered on
  // adapter change events.
  settings_->SetIsFastInitiationHardwareSupported(
      is_hardware_offloading_supported);

  if (fast_initiation_scanner_cooldown_timer_ &&
      fast_initiation_scanner_cooldown_timer_->IsRunning()) {
    NL_VLOG(1) << __func__
               << ": Stopping background scanning due to post-transfer "
                  "cooldown period";
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
      [this]() { OnFastInitiationDevicesDetected(); },
      [this]() { OnFastInitiationDevicesNotDetected(); },
      [this]() { StopFastInitiationScanning(); });
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

  nearby_fast_initiation_->StopScanning([]() {
    NL_VLOG(1) << __func__ << ": Stopped fast initiation scanning.";
  });

  for (auto& observer : observers_.GetObservers()) {
    observer->OnFastInitiationScanningStopped();
  }
  NL_VLOG(1) << __func__ << ": Stopped background scanning.";
}

void NearbySharingServiceImpl::ScheduleRotateBackgroundAdvertisementTimer() {
  absl::BitGen bitgen;
  uint64_t delayMilliseconds = absl::Uniform(
      bitgen,
      absl::ToInt64Milliseconds(kBackgroundAdvertisementRotationDelayMin),
      absl::ToInt64Milliseconds(kBackgroundAdvertisementRotationDelayMax));
  rotate_background_advertisement_timer_ = std::make_unique<ThreadTimer>(
      *service_thread_, "rotate_background_advertisement_timer",
      absl::Milliseconds(delayMilliseconds),
      [this]() { OnRotateBackgroundAdvertisementTimerFired(); });
}

void NearbySharingServiceImpl::OnRotateBackgroundAdvertisementTimerFired() {
  NL_LOG(INFO) << __func__ << ": Rotate background advertisement timer fired.";

  if (!foreground_receive_callbacks_map_.empty()) {
    ScheduleRotateBackgroundAdvertisementTimer();
  } else {
    StopAdvertising();
    InvalidateSurfaceState();
  }
}

void NearbySharingServiceImpl::RemoveOutgoingShareTargetAndReportLost(
    absl::string_view endpoint_id) {
  std::optional<ShareTarget> share_target_opt =
      RemoveOutgoingShareTargetWithEndpointId(endpoint_id);
  if (!share_target_opt.has_value()) {
    return;
  }
  for (auto& entry : foreground_send_surface_map_) {
    entry.second.OnShareTargetLost(share_target_opt.value());
  }
  for (auto& entry : background_send_surface_map_) {
    entry.second.OnShareTargetLost(share_target_opt.value());
  }

  VLOG(1) << __func__
          << ": Reported OnShareTargetLost for EndpointId: " << endpoint_id;
}

void NearbySharingServiceImpl::OnTransferComplete() {
  bool was_sending_files = is_sending_files_;
  is_receiving_files_ = false;
  is_transferring_ = false;
  is_sending_files_ = false;

  NL_VLOG(1) << __func__ << ": NearbySharing state change transfer finished";
  // Files transfer is done! Receivers can immediately cancel, but senders
  // should add a short delay to ensure the final in-flight packet(s) make
  // it to the remote device.
  RunOnNearbySharingServiceThreadDelayed(
      "transfer_done_delay",
      was_sending_files ? kInvalidateSurfaceStateDelayAfterTransferDone
                        : absl::Milliseconds(1),
      [this]() { InvalidateSurfaceState(); });
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

void NearbySharingServiceImpl::OnOutgoingConnection(
    absl::Time connect_start_time, NearbyConnection* connection,
    OutgoingShareSession& session) {
  int64_t share_target_id = session.share_target().id;
  if (!session.OnConnected(connect_start_time,
                           nearby_connections_manager_.get(), connection)) {
    session.Abort(session.disconnect_status());
    return;
  }

  connection->SetDisconnectionListener(
      [this, share_target_id]() { OnConnectionDisconnected(share_target_id); });

  // Log analytics event of establishing connection.
  analytics_recorder_->NewEstablishConnection(
      session.session_id(),
      EstablishConnectionStatus::CONNECTION_STATUS_SUCCESS,
      session.share_target(),
      /*transfer_position=*/GetConnectedShareTargetPos(),
      /*concurrent_connections=*/GetConnectedShareTargetCount(),
      absl::ToInt64Milliseconds(
          (context_->GetClock()->Now() - connect_start_time)),
      /*referrer_package=*/std::nullopt);

  std::optional<std::vector<uint8_t>> token =
      nearby_connections_manager_->GetRawAuthenticationToken(
          session.endpoint_id());
  if (!token.has_value()) {
    session.Abort(TransferMetadata::Status::kDeviceAuthenticationFailed);
    return;
  }
  session.RunPairedKeyVerification(
      context_->GetClock(), ToProtoOsType(device_info_.GetOsType()),
      {
          .visibility = settings_->GetVisibility(),
          .last_visibility = settings_->GetLastVisibility(),
          .last_visibility_time = settings_->GetLastVisibilityTimestamp(),
      },
      GetCertificateManager(), *token,
      absl::bind_front(
          &NearbySharingServiceImpl::OnOutgoingConnectionKeyVerificationDone,
          this, share_target_id));
}

void NearbySharingServiceImpl::CreatePayloads(
    OutgoingShareSession& session,
    std::function<void(OutgoingShareSession&, bool)> callback) {
  int64_t share_target_id = session.share_target().id;
  if (!session.file_payloads().empty() || !session.text_payloads().empty() ||
      !session.wifi_credentials_payloads().empty()) {
    // We may have already created the payloads in the case of retry, so we can
    // skip this step.
    std::move(callback)(session, /*success=*/false);
    return;
  }
  session.CreateTextPayloads();
  session.CreateWifiCredentialsPayloads();
  file_handler_.OpenFiles(
      session.GetFilePaths(),
      [this, share_target_id, callback = std::move(callback)](
          std::vector<NearbyFileHandler::FileInfo> file_infos) {
        RunOnNearbySharingServiceThread(
            "open_files",
            [this, share_target_id, callback = std::move(callback),
             file_infos = std::move(file_infos)]() {
              OutgoingShareSession* session =
                  GetOutgoingShareSession(share_target_id);
              if (session == nullptr) {
                return;
              }
              bool result = session->CreateFilePayloads(file_infos);
              std::move(callback)(*session, result);
            });
      });
}

void NearbySharingServiceImpl::OnCreatePayloads(
    std::vector<uint8_t> endpoint_info, OutgoingShareSession& session,
    bool success) {
  bool has_payloads = !session.text_payloads().empty() ||
                      !session.file_payloads().empty() ||
                      !session.wifi_credentials_payloads().empty();
  if (!success || !has_payloads) {
    NL_LOG(WARNING) << __func__
                    << ": Failed to send file to remote ShareTarget. Failed to "
                       "create payloads.";
    session.UpdateTransferMetadata(
        TransferMetadataBuilder()
            .set_status(TransferMetadata::Status::kMediaUnavailable)
            .build());
    return;
  }
  // Log analytics event of describing attachments.
  analytics_recorder_->NewDescribeAttachments(session.attachment_container());

  std::optional<std::vector<uint8_t>> bluetooth_mac_address =
      GetBluetoothMacAddressForShareTarget(session);

  // For metrics.
  all_cancelled_share_target_ids_.clear();

  int64_t share_target_id = session.share_target().id;
  absl::Time connection_start_time = context_->GetClock()->Now();

  // TODO(b/343281329): do not request wifi hotspot medium if
  // disable_wifi_hotspot option has been requested and device is currently
  // connected to Wifi.
  nearby_connections_manager_->Connect(
      std::move(endpoint_info), session.endpoint_id(),
      std::move(bluetooth_mac_address), settings_->GetDataUsage(),
      GetTransportType(session.attachment_container()),
      [this, connection_start_time, share_target_id](
          NearbyConnection* connection, Status status) {
        OutgoingShareSession* session =
            GetOutgoingShareSession(share_target_id);
        if (session == nullptr) {
          NL_LOG(WARNING) << __func__
                          << "Nearby connection connected, but share target "
                          << share_target_id << " already disconnected.";
          return;
        }
        // Log analytics event of new connection.
        session->set_connection_layer_status(status);
        if (connection == nullptr) {
          analytics_recorder_->NewEstablishConnection(
              session->session_id(),
              EstablishConnectionStatus::CONNECTION_STATUS_FAILURE,
              session->share_target(),
              /*transfer_position=*/
              GetConnectedShareTargetPos(),
              /*concurrent_connections=*/GetConnectedShareTargetCount(),
              absl::ToInt64Milliseconds(context_->GetClock()->Now() -
                                        connection_start_time),
              std::nullopt);
        }

        OnOutgoingConnection(connection_start_time, connection, *session);
      });
}

void NearbySharingServiceImpl::Fail(IncomingShareSession& session,
                                    TransferMetadata::Status status) {
  RunOnNearbySharingServiceThreadDelayed(
      "incoming_rejection_delay", kIncomingRejectionDelay,
      absl::bind_front(&NearbySharingServiceImpl::CloseConnection, this,
                       session.share_target().id));

  session.SendFailureResponse(status);
}

void NearbySharingServiceImpl::OnIncomingAdvertisementDecoded(
    absl::string_view endpoint_id, IncomingShareSession& session,
    std::unique_ptr<Advertisement> advertisement) {
  int64_t placeholder_share_target_id = session.share_target().id;

  if (!advertisement) {
    NL_LOG(WARNING) << __func__
                    << ": Failed to parse incoming connection from endpoint - "
                    << endpoint_id << ", disconnecting.";
    session.Abort(TransferMetadata::Status::kFailed);
    return;
  }

  NearbyShareEncryptedMetadataKey encrypted_metadata_key(
      advertisement->salt(), advertisement->encrypted_metadata_key());

  // Because we cannot apply std::move on Advertisement in lambda, copy to pass
  // data to lambda.
  GetCertificateManager()->GetDecryptedPublicCertificate(
      std::move(encrypted_metadata_key),
      [this, endpoint_id, advertisement_copy = *advertisement,
       placeholder_share_target_id](
          std::optional<NearbyShareDecryptedPublicCertificate>
              decrypted_public_certificate) {
        RunOnNearbySharingServiceThread(
            "incoming_decrypted_certificate",
            // capture endpoint_id string_view as a std::string to ensure the
            // data does not go out of scope.
            [this, endpoint_id = std::string(endpoint_id), advertisement_copy,
             placeholder_share_target_id,
             decrypted_public_certificate =
                 std::move(decrypted_public_certificate)]() {
              OnIncomingDecryptedCertificate(endpoint_id, advertisement_copy,
                                             placeholder_share_target_id,
                                             decrypted_public_certificate);
            });
      });
}

void NearbySharingServiceImpl::OnIncomingTransferUpdate(
    const IncomingShareSession& session, const TransferMetadata& metadata) {
  // kInProgress status is logged extensively elsewhere so avoid the spam.
  if (metadata.status() != TransferMetadata::Status::kInProgress) {
    NL_VLOG(1) << __func__ << ": Nearby Share service: "
               << "Incoming transfer update for share target with ID "
               << session.share_target().id << ": "
               << TransferMetadata::StatusToString(metadata.status());
  }
  if (metadata.status() != TransferMetadata::Status::kCancelled &&
      metadata.status() != TransferMetadata::Status::kRejected) {
    last_incoming_metadata_ =
        std::make_tuple(session.share_target(), session.attachment_container(),
                        TransferMetadataBuilder::Clone(metadata)
                            .set_is_original(false)
                            .build());
  } else {
    last_incoming_metadata_ = std::nullopt;
  }

  if (metadata.is_final_status()) {
    // Log analytics event of receiving attachment end.
    int64_t received_bytes =
        session.attachment_container().GetTotalAttachmentsSize() *
        metadata.progress() / 100;
    AttachmentTransmissionStatus transmission_status =
        ConvertToTransmissionStatus(metadata.status());

    analytics_recorder_->NewReceiveAttachmentsEnd(
        session.session_id(), received_bytes, transmission_status,
        /* referrer_package=*/std::nullopt);

    OnTransferComplete();
    if (metadata.status() != TransferMetadata::Status::kComplete) {
      // For any type of failure, lets make sure any pending files get cleaned
      // up.
      RemoveIncomingPayloads(session);
    } else {
      if (!nearby_connections_manager_->GetAndClearUnknownFilePathsToDelete()
               .empty()) {
        NL_LOG(WARNING) << __func__ << ": Unknown file paths are not empty.";
      }
    }
  } else if (metadata.status() ==
             TransferMetadata::Status::kAwaitingLocalConfirmation) {
    OnTransferStarted(/*is_incoming=*/true);
  }

  auto callbacks = foreground_receive_callbacks_map_;
  if (callbacks.empty()) {
    callbacks = background_receive_callbacks_map_;
  }

  for (auto& callback : callbacks) {
    callback.first->OnTransferUpdate(session.share_target(),
                                     session.attachment_container(), metadata);
  }
}

void NearbySharingServiceImpl::OnOutgoingTransferUpdate(
    OutgoingShareSession& session, const TransferMetadata& metadata) {
  // kInProgress status is logged extensively elsewhere so avoid the spam.
  if (metadata.status() != TransferMetadata::Status::kInProgress) {
    NL_VLOG(1) << __func__ << ": Nearby Share service: "
               << "Outgoing transfer update for share target with ID "
               << session.share_target().id << ": "
               << TransferMetadata::StatusToString(metadata.status());
  }

  if (metadata.is_final_status()) {
    // Log analytics event of sending attachment end.
    int64_t sent_bytes =
        session.attachment_container().GetTotalAttachmentsSize() *
        metadata.progress() / 100;
    AttachmentTransmissionStatus transmission_status =
        ConvertToTransmissionStatus(metadata.status());

    analytics_recorder_->NewSendAttachmentsEnd(
        session.session_id(), sent_bytes, session.share_target(),
        transmission_status,
        /*transfer_position=*/GetConnectedShareTargetPos(),
        /*concurrent_connections=*/GetConnectedShareTargetCount(),
        /*duration_millis=*/
        session.connection_start_time().has_value()
            ? absl::ToInt64Milliseconds(context_->GetClock()->Now() -
                                        *(session.connection_start_time()))
            : 0,
        /*referrer_package=*/std::nullopt,
        ConvertToConnectionLayerStatus(session.connection_layer_status()),
        session.os_type());
    is_connecting_ = false;
    OnTransferComplete();
  } else if (metadata.status() ==
             TransferMetadata::Status::kAwaitingLocalConfirmation) {
    is_connecting_ = false;
    OnTransferStarted(/*is_incoming=*/false);
  }

  bool has_foreground_send_surface = !foreground_send_surface_map_.empty();
  // only call transfer update when having share session.
  if (has_foreground_send_surface) {
    for (auto& entry : foreground_send_surface_map_) {
      entry.first->OnTransferUpdate(session.share_target(),
                                    session.attachment_container(), metadata);
    }
  } else {
    for (auto& entry : background_send_surface_map_) {
      entry.first->OnTransferUpdate(session.share_target(),
                                    session.attachment_container(), metadata);
    }
  }

  // check whether need to send next payload.
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_sharing_feature::
              kEnableTransferCancellationOptimization)) {
    if (metadata.in_progress_attachment_transferred_bytes().has_value() &&
        metadata.in_progress_attachment_total_bytes().has_value() &&
        *metadata.in_progress_attachment_transferred_bytes() ==
            *metadata.in_progress_attachment_total_bytes()) {
      session.SendNextPayload();
    }
  }

  if (has_foreground_send_surface && metadata.is_final_status()) {
    last_outgoing_metadata_ = std::nullopt;
  } else {
    last_outgoing_metadata_ =
        std::make_tuple(session.share_target(), session.attachment_container(),
                        TransferMetadataBuilder::Clone(metadata)
                            .set_is_original(false)
                            .build());
  }
}

void NearbySharingServiceImpl::CloseConnection(int64_t share_target_id) {
  ShareSession* session = GetShareSession(share_target_id);
  if (session != nullptr && session->IsConnected()) {
    session->connection()->Close();
    return;
  }
  NL_LOG(WARNING) << __func__ << ": Invalid connection for target - "
                  << share_target_id;
}

void NearbySharingServiceImpl::OnIncomingDecryptedCertificate(
    absl::string_view endpoint_id, const Advertisement& advertisement,
    int64_t placeholder_share_target_id,
    std::optional<NearbyShareDecryptedPublicCertificate> certificate) {
  auto it = incoming_share_session_map_.find(placeholder_share_target_id);
  if (it == incoming_share_session_map_.end()) {
    NL_VLOG(1) << __func__ << ": Invalid connection for endpoint id - "
               << endpoint_id;
    return;
  }
  if (!it->second.IsConnected()) {
    NL_VLOG(1) << __func__ << ": Connection has been closed for endpoint id - "
               << endpoint_id;
    incoming_share_session_map_.erase(it);
    return;
  }
  NearbyConnection* connection = it->second.connection();
  int64_t session_id = it->second.session_id();

  std::optional<ShareTarget> share_target =
      CreateShareTarget(endpoint_id, advertisement, certificate,
                        /*is_incoming=*/true);
  if (!share_target) {
    NL_LOG(WARNING) << __func__
                    << ": Failed to convert advertisement to share target for "
                       "incoming connection, disconnecting";
    it->second.Abort(TransferMetadata::Status::kFailed);
    return;
  }
  // Remove placeholder share target since we are creating the actual share
  // target below.
  incoming_share_session_map_.erase(it);

  int64_t share_target_id = share_target->id;
  NL_VLOG(1) << __func__ << ": Received incoming connection from "
             << share_target_id;

  IncomingShareSession& session = CreateIncomingShareSession(
      *share_target, endpoint_id, std::move(certificate));
  // Copy session id from placeholder session to actual session.
  session.set_session_id(session_id);
  session.OnConnected(context_->GetClock()->Now(),
                      nearby_connections_manager_.get(), connection);
  // Need to rebind the disconnect listener to the new share target id.
  connection->SetDisconnectionListener(
      [this, share_target_id]() { OnConnectionDisconnected(share_target_id); });

  std::optional<std::vector<uint8_t>> token =
      nearby_connections_manager_->GetRawAuthenticationToken(
          session.endpoint_id());

  if (!token.has_value()) {
    session.Abort(TransferMetadata::Status::kDeviceAuthenticationFailed);
    return;
  }
  session.RunPairedKeyVerification(
      context_->GetClock(), ToProtoOsType(device_info_.GetOsType()),
      {
          .visibility = settings_->GetVisibility(),
          .last_visibility = settings_->GetLastVisibility(),
          .last_visibility_time = settings_->GetLastVisibilityTimestamp(),
      },
      GetCertificateManager(), *token,
      absl::bind_front(
          &NearbySharingServiceImpl::OnIncomingConnectionKeyVerificationDone,
          this, share_target_id));
}

void NearbySharingServiceImpl::OnIncomingConnectionKeyVerificationDone(
    int64_t share_target_id,
    PairedKeyVerificationRunner::PairedKeyVerificationResult result,
    OSType share_target_os_type) {
  IncomingShareSession* session = GetIncomingShareSession(share_target_id);
  if (!session || !session->IsConnected()) {
    NL_VLOG(1) << __func__ << ": Invalid connection or endpoint id";
    return;
  }
  if (!session->ProcessKeyVerificationResult(
          result, share_target_os_type,
          absl::bind_front(&NearbySharingServiceImpl::OnReceivedIntroduction,
                           this, share_target_id))) {
    session->Abort(TransferMetadata::Status::kDeviceAuthenticationFailed);
  }
}

void NearbySharingServiceImpl::OnOutgoingConnectionKeyVerificationDone(
    int64_t share_target_id,
    PairedKeyVerificationRunner::PairedKeyVerificationResult result,
    OSType share_target_os_type) {
  OutgoingShareSession* session = GetOutgoingShareSession(share_target_id);
  if (!session || !session->IsConnected()) {
    return;
  }

  if (!session->ProcessKeyVerificationResult(result, share_target_os_type)) {
    session->Abort(TransferMetadata::Status::kDeviceAuthenticationFailed);
    return;
  }

  NL_VLOG(1) << __func__ << ": Preparing to send introduction to "
             << share_target_id;
  if (!session->SendIntroduction([this, share_target_id]() {
        NL_VLOG(1)
            << "Outgoing mutual acceptance timed out, closing connection for "
            << share_target_id;
        OutgoingShareSession* session =
            GetOutgoingShareSession(share_target_id);
        if (session == nullptr) {
          return;
        }
        session->Abort(TransferMetadata::Status::kTimedOut);
      })) {
    NL_LOG(WARNING) << __func__
                    << ": No payloads tied to transfer, disconnecting.";
    session->Abort(TransferMetadata::Status::kMediaUnavailable);
    return;
  }
  // Auto Accept if key verification is successful or skip sender confirmation.
  if (session->token().empty() ||
      NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_sharing_feature::
              kSenderSkipsConfirmation)) {
    OutgoingSessionAccept(*session);
  } else {
    session->UpdateTransferMetadata(
        TransferMetadataBuilder()
            .set_status(TransferMetadata::Status::kAwaitingLocalConfirmation)
            .set_token(session->token())
            .build());
  }
}

void NearbySharingServiceImpl::OnReceivedIntroduction(
    int64_t share_target_id, std::optional<IntroductionFrame> frame) {
  IncomingShareSession* session = GetIncomingShareSession(share_target_id);
  if (!session || !session->IsConnected()) {
    NL_LOG(WARNING)
        << __func__
        << ": Ignore received introduction, due to no connection established.";
    return;
  }

  if (!frame.has_value()) {
    session->Abort(TransferMetadata::Status::kFailed);
    NL_LOG(WARNING) << __func__ << ": Invalid introduction frame";
    return;
  }

  NL_LOG(INFO) << __func__ << ": Successfully read the introduction frame.";

  std::optional<TransferMetadata::Status> status =
      session->ProcessIntroduction(*frame);
  if (status.has_value()) {
    Fail(*session, *status);
    return;
  }

  // Log analytics event of receiving introduction.
  analytics_recorder_->NewReceiveIntroduction(
      session->session_id(), session->share_target(),
      /*referrer_package=*/std::nullopt, session->os_type());

  if (IsOutOfStorage(device_info_,
                     std::filesystem::u8path(settings_->GetCustomSavePath()),
                     session->attachment_container().GetStorageSize())) {
    Fail(*session, TransferMetadata::Status::kNotEnoughSpace);
    NL_LOG(WARNING) << __func__
                    << ": Not enough space on the receiver. We have informed "
                    << share_target_id;
    return;
  }

  OnStorageCheckCompleted(*session);
}

void NearbySharingServiceImpl::OnReceiveConnectionResponse(
    int64_t share_target_id, std::optional<ConnectionResponseFrame> frame) {
  OutgoingShareSession* session = GetOutgoingShareSession(share_target_id);
  if (!session || !session->IsConnected()) {
    NL_LOG(WARNING) << __func__
                    << ": Ignore received connection response, due to no "
                       "connection established.";
    return;
  }

  std::optional<TransferMetadata::Status> status =
      session->HandleConnectionResponse(std::move(frame));
  if (status.has_value()) {
    session->Abort(*status);
    return;
  }
  session->SendPayloads(
      NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_sharing_feature::
              kEnableTransferCancellationOptimization),
      context_->GetClock(),
      [this, share_target_id](
          std::optional<nearby::sharing::service::proto::V1Frame> frame) {
        OnFrameRead(share_target_id, std::move(frame));
      },
      absl::bind_front(&NearbySharingServiceImpl::OutgoingPayloadTransferUpdate,
                       this));
}

void NearbySharingServiceImpl::OnStorageCheckCompleted(
    IncomingShareSession& session) {
  if (!session.ReadyForTransfer(
          [this, share_target_id = session.share_target().id]() {
            NL_VLOG(1) << "Incoming mutual acceptance timed out, closing "
                          "connection for "
                       << share_target_id;
            IncomingShareSession* session =
                GetIncomingShareSession(share_target_id);
            if (session != nullptr) {
              Fail(*session, TransferMetadata::Status::kTimedOut);
            }
          },
          absl::bind_front(&NearbySharingServiceImpl::OnFrameRead, this,
                           session.share_target().id))) {
    return;
  }
  // Don't need to wait for user to accept for Self share.
  NL_LOG(INFO) << __func__ << ": Auto-accepting self share.";
  session.AcceptTransfer(
      context_->GetClock(),
      absl::bind_front(&NearbySharingServiceImpl::IncomingPayloadTransferUpdate,
                       this));
  OnTransferStarted(/*is_incoming=*/true);
}

void NearbySharingServiceImpl::OnFrameRead(
    int64_t share_target_id,
    std::optional<nearby::sharing::service::proto::V1Frame> frame) {
  if (!frame.has_value()) {
    // This is the case when the connection has been closed since we wait
    // indefinitely for incoming frames.
    return;
  }

  switch (frame->type()) {
    case nearby::sharing::service::proto::V1Frame::CANCEL:
      RunOnAnyThread("cancel_transfer", [this, share_target_id]() {
        NL_LOG(INFO) << __func__
                     << ": Read the cancel frame, closing connection";
        DoCancel(
            share_target_id, [](StatusCodes status_codes) {},
            /*is_initiator_of_cancellation=*/false);
      });
      break;

    case nearby::sharing::service::proto::V1Frame::CERTIFICATE_INFO:
      // No-op, no longer used.
      break;

    case nearby::sharing::service::proto::V1Frame::PROGRESS_UPDATE:
      // No-op, no longer used.
      break;

    default:
      NL_LOG(ERROR) << __func__ << ": Discarding unknown frame of type";
      break;
  }

  ShareSession* session = GetShareSession(share_target_id);
  if (!session || !session->frames_reader()) {
    NL_LOG(WARNING) << __func__
                    << ": Stopped reading further frames, due to no connection "
                       "established.";
    return;
  }

  session->frames_reader()->ReadFrame(
      [this, share_target_id](
          std::optional<nearby::sharing::service::proto::V1Frame> frame) {
        OnFrameRead(share_target_id, std::move(frame));
      });
}

void NearbySharingServiceImpl::OnConnectionDisconnected(
    int64_t share_target_id) {
  if (IsShuttingDown()) {
    return;
  }
  ShareSession* session = GetShareSession(share_target_id);
  if (session != nullptr) {
    session->OnDisconnect();
  }
  UnregisterShareTarget(share_target_id);
}

std::optional<ShareTarget> NearbySharingServiceImpl::CreateShareTarget(
    absl::string_view endpoint_id, const Advertisement& advertisement,
    const std::optional<NearbyShareDecryptedPublicCertificate>& certificate,
    bool is_incoming) {
  if (!advertisement.device_name() && !certificate.has_value()) {
    NL_VLOG(1) << __func__
               << ": Failed to retrieve public certificate for contact "
                  "only advertisement.";
    return std::nullopt;
  }

  std::optional<std::string> device_name =
      GetDeviceName(advertisement, certificate);
  if (!device_name.has_value()) {
    NL_VLOG(1) << __func__
               << ": Failed to retrieve device name for advertisement.";
    return std::nullopt;
  }

  ShareTarget target;
  target.type = advertisement.device_type();
  target.device_name = std::move(*device_name);
  target.is_incoming = is_incoming;
  target.device_id = GetDeviceId(endpoint_id, certificate);
  target.vendor_id = advertisement.vendor_id();
  if (certificate.has_value()) {
    target.for_self_share = certificate->for_self_share();

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
    // Always prefer the certificate's vendor ID if available.
    if (certificate->unencrypted_metadata().has_vendor_id()) {
      target.vendor_id = certificate->unencrypted_metadata().vendor_id();
    }
    target.is_known = true;
  }
  return target;
}

void NearbySharingServiceImpl::IncomingPayloadTransferUpdate(
    int64_t share_target_id, TransferMetadata metadata) {
  IncomingShareSession* session = GetIncomingShareSession(share_target_id);
  if (!session) {
    // ShareTarget already disconnected.
    NL_LOG(WARNING)
        << "Received payload update after share target disconnected: "
        << share_target_id;
    return;
  }
  std::pair<bool, bool> result =
      session->PayloadTransferUpdate(update_file_paths_in_progress_, metadata);
  if (result.first) {
    if (!result.second) {
      OnIncomingFilesMetadataUpdated(share_target_id, std::move(metadata),
                                     /*success=*/false);
      return;
    }
    file_handler_.UpdateFilesOriginMetadata(
        session->GetPayloadFilePaths(),
        absl::bind_front(
            &NearbySharingServiceImpl::OnIncomingFilesMetadataUpdated, this,
            share_target_id, std::move(metadata)));
  }
}

void NearbySharingServiceImpl::OnIncomingFilesMetadataUpdated(
    int64_t share_target_id, TransferMetadata metadata, bool success) {
  if (!success) {
    metadata = TransferMetadataBuilder()
                   .set_status(TransferMetadata::Status::kIncompletePayloads)
                   .build();
  }
  RunOnNearbySharingServiceThread(
      "update_files_origin_metadata",
      [this, share_target_id, metadata = std::move(metadata)]() {
        IncomingShareSession* session =
            GetIncomingShareSession(share_target_id);
        if (!session) {
          // ShareTarget already disconnected.
          return;
        }
        fast_initiation_scanner_cooldown_timer_ = std::make_unique<ThreadTimer>(
            *service_thread_, "fast_initiation_scanner_cooldown_timer",
            kFastInitiationScannerCooldown, [this]() {
              fast_initiation_scanner_cooldown_timer_.reset();
              InvalidateFastInitiationScanning();
            });
        // Make sure to call this before calling Disconnect, or we risk losing
        // some transfer updates in the receive case due to the Disconnect call
        // cleaning up share targets.
        session->UpdateTransferMetadata(metadata);

        if (TransferMetadata::IsFinalStatus(metadata.status())) {
          // Cancellation has its own disconnection strategy, possibly adding a
          // delay before disconnection to provide the other party time to
          // process the cancellation.
          if (metadata.status() != TransferMetadata::Status::kCancelled) {
            session->Disconnect();
          }
        }
      });
}

void NearbySharingServiceImpl::OutgoingPayloadTransferUpdate(
    int64_t share_target_id, TransferMetadata metadata) {
  OutgoingShareSession* session = GetOutgoingShareSession(share_target_id);
  if (!session) {
    // ShareTarget already disconnected.
    NL_LOG(WARNING)
        << "Received payload update after share target disconnected: "
        << share_target_id;
    return;
  }

  // kInProgress status is logged extensively elsewhere so avoid the spam.
  if (metadata.status() != TransferMetadata::Status::kInProgress) {
    NL_VLOG(1) << __func__ << ": Nearby Share service: "
               << "Payload transfer update for share target with ID "
               << share_target_id << ": "
               << TransferMetadata::StatusToString(metadata.status());
  }

  // When kComplete is received from PayloadTracker, we need to wait for
  // receiver to close connection before we know that the transfer has
  // succeeded.  Here we delay the kComplete update until receiver disconnects.
  // A 1 min timer is setup so that if we do not receive disconnect from
  // receiver, we assume the transfer has failed.
  if (metadata.status() == TransferMetadata::Status::kComplete) {
    session->DelayCompleteMetadata(metadata);
    auto timer = std::make_unique<ThreadTimer>(
        *service_thread_, "disconnection_timeout_alarm",
        kOutgoingDisconnectionDelay,
        [this, share_target_id = session->share_target().id]() {
          OutgoingShareSession* session =
              GetOutgoingShareSession(share_target_id);
          if (session != nullptr) {
            session->DisconnectionTimeout();
          }
        });
    disconnection_timeout_alarms_[session->endpoint_id()] = std::move(timer);
    return;
  }
  // Make sure to call this before calling Disconnect, or we risk losing some
  // transfer updates in the receive case due to the Disconnect call cleaning up
  // share targets.
  session->UpdateTransferMetadata(metadata);

  // Cancellation has its own disconnection strategy, possibly adding a delay
  // before disconnection to provide the other party time to process the
  // cancellation.
  if (TransferMetadata::IsFinalStatus(metadata.status()) &&
      metadata.status() != TransferMetadata::Status::kCancelled) {
    // Failed to send. No point in continuing, so disconnect immediately.
    session->Disconnect();
  }
}

void NearbySharingServiceImpl::RemoveIncomingPayloads(
    const IncomingShareSession& session) {
  NL_LOG(INFO) << __func__ << ": Cleaning up payloads due to transfer failure";
  nearby_connections_manager_->ClearIncomingPayloads();
  std::vector<std::filesystem::path> files_for_deletion;
  auto file_paths_to_delete =
      nearby_connections_manager_->GetAndClearUnknownFilePathsToDelete();
  for (auto it = file_paths_to_delete.begin(); it != file_paths_to_delete.end();
       ++it) {
    NL_VLOG(1) << __func__ << ": Has unknown file path to delete.";
    files_for_deletion.push_back(*it);
  }
  std::vector<std::filesystem::path> payload_file_path =
      session.GetPayloadFilePaths();
  files_for_deletion.insert(files_for_deletion.end(), payload_file_path.begin(),
                            payload_file_path.end());
  file_handler_.DeleteFilesFromDisk(std::move(files_for_deletion), []() {});
}

IncomingShareSession& NearbySharingServiceImpl::CreateIncomingShareSession(
    const ShareTarget& share_target, absl::string_view endpoint_id,
    std::optional<NearbyShareDecryptedPublicCertificate> certificate) {
  NL_DCHECK(share_target.is_incoming);
  auto [it, inserted] = incoming_share_session_map_.try_emplace(
      share_target.id, *service_thread_, *analytics_recorder_,
      std::string(endpoint_id), share_target,
      absl::bind_front(&NearbySharingServiceImpl::OnIncomingTransferUpdate,
                       this));
  if (!inserted) {
    NL_LOG(ERROR) << __func__ << ": Incoming share target id already exists "
                  << share_target.id;
  }
  if (certificate.has_value()) {
    it->second.set_certificate(std::move(*certificate));
  }
  return it->second;
}

void NearbySharingServiceImpl::DeduplicateInOutgoingShareTarget(
    const ShareTarget& share_target, absl::string_view endpoint_id,
    std::optional<NearbyShareDecryptedPublicCertificate> certificate) {
  // TODO(b/343764269): may need to update last_outgoing_metadata_ if the
  // deduped target id matches the one in last_outgoing_metadata_.
  // But since we do not modify the share target of a connected session, it may
  // not happen.

  auto session_it = outgoing_share_session_map_.find(share_target.id);
  if (session_it == outgoing_share_session_map_.end()) {
    LOG(WARNING) << __func__ << ": share_target.id=" << share_target.id
                 << " not found in outgoing share session map.";
    return;
  }
  if (session_it->second.IsConnected()) {
    LOG(INFO) << __func__ << ": share_target.id=" << share_target.id
              << " is connected, not updating outgoing_share_session_map_.";
    return;
  }
  session_it->second.UpdateSessionForDedup(share_target, std::move(certificate),
                                           endpoint_id);

  for (auto& entry : foreground_send_surface_map_) {
    entry.second.OnShareTargetUpdated(share_target);
  }
  for (auto& entry : background_send_surface_map_) {
    entry.second.OnShareTargetUpdated(share_target);
  }

  LOG(INFO) << __func__
            << ": [Dedupped] Reported OnShareTargetUpdated to all surfaces "
               "for share_target: "
            << share_target.ToString();
}

void NearbySharingServiceImpl::DeDuplicateInDiscoveryCache(
    const ShareTarget& share_target, absl::string_view endpoint_id,
    std::optional<NearbyShareDecryptedPublicCertificate> certificate) {
  CreateOutgoingShareSession(share_target, endpoint_id, std::move(certificate));
  for (auto& entry : foreground_send_surface_map_) {
    entry.second.OnShareTargetUpdated(share_target);
  }
  for (auto& entry : background_send_surface_map_) {
    entry.second.OnShareTargetUpdated(share_target);
  }

  LOG(INFO) << __func__
            << ": [Dedupped] Reported OnShareTargetUpdated to all surfaces "
               "for share_target: "
            << share_target.ToString();
}

bool NearbySharingServiceImpl::FindDuplicateInDiscoveryCache(
    absl::string_view endpoint_id, ShareTarget& share_target) {
  auto it = discovery_cache_.find(endpoint_id);
  if (it != discovery_cache_.end()) {
    // If endpoint info changes for an endpoint ID, NC will send a rediscovery
    // event for the same endpoint id.
    LOG(INFO) << __func__
              << ": [Dedupped] Found duplicate endpoint_id: " << endpoint_id
              << ", share_target.id changed from: " << share_target.id << " to "
              << it->second.share_target.id;
    share_target.id = it->second.share_target.id;
    discovery_cache_.erase(it);
    outgoing_share_target_map_.insert_or_assign(endpoint_id, share_target);
    return true;
  }

  for (auto it = discovery_cache_.begin(); it != discovery_cache_.end(); ++it) {
    if (it->second.share_target.device_id == share_target.device_id) {
      LOG(INFO) << __func__
                << ": [Dedupped] Found duplicate device_id, share_target.id "
                   "changed from: "
                << share_target.id << " to " << it->second.share_target.id
                << ". New endpoint_id: " << endpoint_id;
      share_target.id = it->second.share_target.id;
      discovery_cache_.erase(it);
      outgoing_share_target_map_.insert_or_assign(endpoint_id, share_target);
      return true;
    }
  }
  return false;
}

bool NearbySharingServiceImpl::FindDuplicateInOutgoingShareTargets(
    absl::string_view endpoint_id, ShareTarget& share_target) {
  // If the duplicate is found, share_target.id needs to be updated to the old
  // "discovered" share_target_id so OnShareTargetUpdated matches a target that
  // was discovered before.

  auto it = outgoing_share_target_map_.find(endpoint_id);
  if (it != outgoing_share_target_map_.end()) {
    // If endpoint info changes for an endpoint ID, NC will send a rediscovery
    // event for the same endpoint id.
    LOG(INFO) << __func__
              << ": [Dedupped] Found duplicate endpoint_id: " << endpoint_id
              << " in outgoing_share_target_map, share_target.id changed from: "
              << share_target.id << " to " << it->second.id;
    share_target.id = it->second.id;
    it->second = share_target;
    return true;
  }

  for (auto it = outgoing_share_target_map_.begin();
       it != outgoing_share_target_map_.end(); ++it) {
    if (it->second.device_id == share_target.device_id) {
      LOG(INFO)
          << __func__
          << ": [Dedupped] Found duplicate device_id, endpoint ID "
             "changed from: "
          << it->first << " to " << endpoint_id
          << " in outgoing_share_target_map, share_target.id changed from: "
          << share_target.id << " to " << it->second.id;
      share_target.id = it->second.id;
      outgoing_share_target_map_.erase(it);
      outgoing_share_target_map_.insert_or_assign(endpoint_id, share_target);
      return true;
    }
  }
  return false;
}

std::optional<ShareTarget>
NearbySharingServiceImpl::RemoveOutgoingShareTargetWithEndpointId(
    absl::string_view endpoint_id) {
  VLOG(1) << "Outgoing connection to " << endpoint_id
          << " disconnected, cancel disconnection timer";
  disconnection_timeout_alarms_.erase(endpoint_id);
  auto it = outgoing_share_target_map_.find(endpoint_id);
  if (it == outgoing_share_target_map_.end()) {
    LOG(WARNING) << __func__ << ": endpoint_id=" << endpoint_id
                 << " not found in outgoing share target map.";
    return std::nullopt;
  }

  VLOG(1) << __func__ << ": Removing (endpoint_id=" << it->first
          << ", share_target.id=" << it->second.id
          << ") from outgoing share target map";
  std::optional<ShareTarget> share_target =
      std::move(outgoing_share_target_map_.extract(it).mapped());

  // Do not destroy the session until it has been removed from the map.
  // Session destruction can trigger callbacks that traverses the map and it
  // cannot access the map while it is being modified.
  auto session_it = outgoing_share_session_map_.find(share_target->id);
  if (session_it == outgoing_share_session_map_.end()) {
    LOG(WARNING) << __func__ << ": share_target.id=" << share_target->id
                 << " not found in outgoing share session map.";
  } else {
    outgoing_share_session_map_.extract(session_it).mapped().OnDisconnect();
  }
  return share_target;
}

void NearbySharingServiceImpl::TriggerDiscoveryCacheExpiryTimers() {
  for (auto it = discovery_cache_.begin(); it != discovery_cache_.end(); ++it) {
    for (auto& entry : foreground_send_surface_map_) {
      entry.second.OnShareTargetLost(it->second.share_target);
    }
    for (auto& entry : background_send_surface_map_) {
      entry.second.OnShareTargetLost(it->second.share_target);
    }
  }
  discovery_cache_.clear();
}

void NearbySharingServiceImpl::MoveToDiscoveryCache(
    absl::string_view endpoint_id) {
  std::optional<ShareTarget> share_target_opt =
      RemoveOutgoingShareTargetWithEndpointId(endpoint_id);
  if (!share_target_opt.has_value()) {
    return;
  }
  DiscoveryCacheEntry cache_entry;
  cache_entry.share_target = std::move(share_target_opt.value());
  cache_entry.expiry_timer = std::make_unique<ThreadTimer>(
      *service_thread_, "discovery_cache_timeout",
      absl::Milliseconds(NearbyFlags::GetInstance().GetInt64Flag(
          config_package_nearby::nearby_sharing_feature::
              kDiscoveryCacheLostExpiryMs)),
      [this, endpoint_id = std::string(endpoint_id)]() {
        auto it = discovery_cache_.find(endpoint_id);
        if (it == discovery_cache_.end()) {
          LOG(WARNING) << "Trying to remove endpoint_id: " << endpoint_id
                       << " from discovery cache, but cannot find it";
          return;
        }
        LOG(INFO) << ": Removing (endpoint_id=" << endpoint_id
                  << ", share_target.id=" << it->second.share_target.id
                  << ") from discovery cache";
        ShareTarget share_target =
            std::move(discovery_cache_.extract(it).mapped().share_target);

        for (auto& entry : foreground_send_surface_map_) {
          entry.second.OnShareTargetLost(share_target);
        }
        for (auto& entry : background_send_surface_map_) {
          entry.second.OnShareTargetLost(share_target);
        }

        VLOG(1) << __func__
                << ": [Dedupped] Reported OnShareTargetLost to all surfaces "
                   "for share_target: "
                << share_target.ToString();
      });
  discovery_cache_.insert_or_assign(endpoint_id, std::move(cache_entry));
}

void NearbySharingServiceImpl::CreateOutgoingShareSession(
    const ShareTarget& share_target, absl::string_view endpoint_id,
    std::optional<NearbyShareDecryptedPublicCertificate> certificate) {
  auto [it_out, inserted] = outgoing_share_session_map_.try_emplace(
      share_target.id, *service_thread_, *analytics_recorder_,
      std::string(endpoint_id), share_target,
      absl::bind_front(&NearbySharingServiceImpl::OnOutgoingTransferUpdate,
                       this));
  if (!inserted) {
    LOG(WARNING) << __func__ << ": share_target.id=" << share_target.id
                 << " already exists in outgoing share session map. This "
                    "should NOT happen";
  } else {
    auto& session = it_out->second;
    session.set_connection_layer_status(Status::kUnknown);
    if (certificate.has_value()) {
      session.set_certificate(std::move(*certificate));
    }
  }
}

ShareSession* NearbySharingServiceImpl::GetShareSession(
    int64_t share_target_id) {
  ShareSession* result = GetIncomingShareSession(share_target_id);
  if (result != nullptr) {
    return result;
  }
  return GetOutgoingShareSession(share_target_id);
}

IncomingShareSession* NearbySharingServiceImpl::GetIncomingShareSession(
    int64_t share_target_id) {
  auto it = incoming_share_session_map_.find(share_target_id);
  if (it == incoming_share_session_map_.end()) {
    return nullptr;
  }

  return &it->second;
}

OutgoingShareSession* NearbySharingServiceImpl::GetOutgoingShareSession(
    int64_t share_target_id) {
  auto it = outgoing_share_session_map_.find(share_target_id);
  if (it == outgoing_share_session_map_.end()) {
    return nullptr;
  }

  return &it->second;
}

std::optional<std::vector<uint8_t>>
NearbySharingServiceImpl::GetBluetoothMacAddressForShareTarget(
    OutgoingShareSession& session) {
  const std::optional<NearbyShareDecryptedPublicCertificate>& certificate =
      session.certificate();
  if (!certificate) {
    NL_LOG(ERROR) << __func__ << ": No decrypted public certificate found for "
                  << "share target id: " << session.share_target().id;
    return std::nullopt;
  }

  return GetBluetoothMacAddressFromCertificate(*certificate);
}

void NearbySharingServiceImpl::ClearOutgoingShareSessionMap() {
  NL_VLOG(1) << __func__ << ": Clearing outgoing share target map.";
  while (!outgoing_share_target_map_.empty()) {
    RemoveOutgoingShareTargetAndReportLost(
        /*endpoint_id=*/outgoing_share_target_map_.begin()->first);
  }
  NL_DCHECK(outgoing_share_target_map_.empty());
  NL_DCHECK(outgoing_share_session_map_.empty());
}

void NearbySharingServiceImpl::UnregisterShareTarget(int64_t share_target_id) {
  LOG(INFO) << __func__ << ": Unregistering share target " << share_target_id;

  // For metrics.
  all_cancelled_share_target_ids_.erase(share_target_id);

  // If share target ID is found in incoming_share_session_map_, then it's an
  // incoming share target.
  bool is_incoming = (incoming_share_session_map_.erase(share_target_id) > 0);
  if (is_incoming) {
    if (last_incoming_metadata_ &&
        std::get<0>(*last_incoming_metadata_).id == share_target_id) {
      last_incoming_metadata_.reset();
    }

    // Clear legacy incoming payloads to release resources.
    nearby_connections_manager_->ClearIncomingPayloads();
  } else {
    if (last_outgoing_metadata_ &&
        std::get<0>(*last_outgoing_metadata_).id == share_target_id) {
      last_outgoing_metadata_.reset();
    }
    // Find the endpoint id that matches the given share target.
    auto it = outgoing_share_session_map_.find(share_target_id);
    if (it != outgoing_share_session_map_.end()) {
      RemoveOutgoingShareTargetAndReportLost(it->second.endpoint_id());
    } else {
      // Be careful not to clear out the share session map if a new session was
      // started during the cancellation delay.
      if (!is_scanning_ && !is_transferring_) {
        LOG(INFO) << "Cannot find session for target " << share_target_id
                  << " clearing all outgoing sessions.";
        ClearOutgoingShareSessionMap();
      }
      TriggerDiscoveryCacheExpiryTimers();
    }

    NL_VLOG(1) << __func__ << ": Unregister share target: " << share_target_id;
  }
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

void NearbySharingServiceImpl::OnNetworkChanged(
    nearby::ConnectivityManager::ConnectionType type) {
  on_network_changed_delay_timer_ = std::make_unique<ThreadTimer>(
      *service_thread_, "on_network_changed_delay_timer",
      kProcessNetworkChangeTimerDelay,
      [this]() { StopAdvertisingAndInvalidateSurfaceState(); });
}

void NearbySharingServiceImpl::OnLanConnectedChanged(bool connected) {
  RunOnNearbySharingServiceThread(
      "lan_connection_changed", [this, connected]() {
        NL_VLOG(1) << __func__ << ": LAN Connection state changed. (Connected: "
                   << connected << ")";
        NearbySharingService::Observer::AdapterState state =
            connected ? NearbySharingService::Observer::AdapterState::ENABLED
                      : NearbySharingService::Observer::AdapterState::DISABLED;
        for (auto& observer : observers_.GetObservers()) {
          observer->OnLanStatusChanged(state);
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
    NearbyShareSettings::FallbackVisibilityInfo fallback_visibility =
        settings_->GetFallbackVisibility();
    bool is_temporarily_visible =
        fallback_visibility.visibility !=
        DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED;
    // When logged out the visibility can either be "everyone" or "hidden". If
    // the visibility wasn't already "always everyone", change it to "hidden".
    if (visibility != DeviceVisibility::DEVICE_VISIBILITY_EVERYONE ||
        is_temporarily_visible) {
      settings_->SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_HIDDEN);
    }
    prefs::RegisterNearbySharingPrefs(preference_manager_,
                                      /*skip_persistent_ones=*/true);

    settings_->AddSettingsObserver(this);
    certificate_manager_->ClearPublicCertificates([](bool result) {
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
    // Set contacts visibility when logging in so the user is ready to share
    // immediately. Notify observers as well.
    settings_->SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  }

  // Start services again.
  local_device_data_manager_->Start();
  contact_manager_->Start();
  certificate_manager_->Start();

  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::RunOnNearbySharingServiceThread(
    absl::string_view task_name, absl::AnyInvocable<void()> task) {
  if (IsShuttingDown()) {
    LOG(WARNING) << __func__ << ": Skip the task " << task_name
                 << " due to service is shutting down.";
    return;
  }

  LOG(INFO) << __func__ << ": Scheduled to run task " << task_name
            << " on API thread.";

  service_thread_->PostTask(
      [this, is_shutting_down = std::weak_ptr<bool>(is_shutting_down_),
       task_name = std::string(task_name), task = std::move(task)]() mutable {
        std::shared_ptr<bool> is_shutting = is_shutting_down.lock();
        if (is_shutting == nullptr || *is_shutting) {
          LOG(WARNING) << __func__ << ": Give up the task " << task_name
                       << " due to service is shutting down.";
          return;
        }

        LOG(INFO) << __func__ << ": Started to run task " << task_name
                  << " on API thread. " << context_->GetClock()->Now();
        task();

        LOG(INFO) << __func__ << ": Completed to run task " << task_name
                  << " on API thread.";
      });
}

void NearbySharingServiceImpl::RunOnNearbySharingServiceThreadDelayed(
    absl::string_view task_name, absl::Duration delay,
    absl::AnyInvocable<void()> task) {
  if (IsShuttingDown()) {
    LOG(WARNING) << __func__ << ": Skip the delayed task " << task_name
                 << " due to service is shutting down.";
    return;
  }

  LOG(INFO) << __func__ << ": Scheduled to run delayed task " << task_name
            << " on API thread.";
  service_thread_->PostDelayedTask(
      delay,
      [is_shutting_down = std::weak_ptr<bool>(is_shutting_down_),
       task_name = std::string(task_name), task = std::move(task)]() mutable {
        std::shared_ptr<bool> is_shutting = is_shutting_down.lock();
        if (is_shutting == nullptr || *is_shutting) {
          LOG(WARNING) << __func__ << ": Give up the delayed task " << task_name
                       << " due to service is shutting down.";
          return;
        }

        LOG(INFO) << __func__ << ": Started to run delayed task " << task_name
                  << " on API thread.";
        task();

        LOG(INFO) << __func__ << ": Completed to run delayed task " << task_name
                  << " on API thread.";
      });
}

void NearbySharingServiceImpl::RunOnAnyThread(absl::string_view task_name,
                                              absl::AnyInvocable<void()> task) {
  if (IsShuttingDown()) {
    LOG(WARNING) << __func__ << ": Skip the task " << task_name
                 << " due to service is shutting down.";
    return;
  }

  LOG(INFO) << __func__ << ": Scheduled to run task " << task_name
            << " on runner thread.";
  context_->GetTaskRunner()->PostTask(
      [is_shutting_down = std::weak_ptr<bool>(is_shutting_down_),
       task_name = std::string(task_name), task = std::move(task)]() mutable {
        std::shared_ptr<bool> is_shutting = is_shutting_down.lock();
        if (is_shutting == nullptr || *is_shutting) {
          LOG(WARNING) << __func__ << ": Give up the task on runner thread "
                       << task_name << " due to service is shutting down.";
          return;
        }

        LOG(INFO) << __func__ << ": Started to run task " << task_name
                  << " on runner thread.";
        task();

        LOG(INFO) << __func__ << ": Completed to run task " << task_name
                  << " on runner thread.";
      });
}

int NearbySharingServiceImpl::GetConnectedShareTargetPos() {
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
    const AttachmentContainer& container) const {
  if (container.GetTotalAttachmentsSize() >
      kAttachmentsSizeThresholdOverHighQualityMedium) {
    if (GetDisableWifiHotspotState()) {
      LOG(INFO) << "Transport type is kHighQuality|kNonDisruptive";
      return TransportType::kHighQualityNonDisruptive;
    }
    LOG(INFO) << "Transport type is kHighQuality";
    return TransportType::kHighQuality;
  }

  if (container.GetFileAttachments().empty()) {
    LOG(INFO) << "Transport type is kNonDisruptive";
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

}  // namespace nearby::sharing
