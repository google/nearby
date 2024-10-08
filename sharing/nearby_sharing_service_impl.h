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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <tuple>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "internal/analytics/event_logger.h"
#include "internal/base/observer_list.h"
#include "internal/platform/device_info.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/platform/task_runner.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/advertisement.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/attachment_container.h"
#include "sharing/certificates/nearby_share_certificate_manager.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/certificates/nearby_share_private_certificate.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/common/nearby_share_profile_info_provider.h"
#include "sharing/fast_initiation/nearby_fast_initiation.h"
#include "sharing/incoming_share_session.h"
#include "sharing/internal/api/app_info.h"
#include "sharing/internal/api/bluetooth_adapter.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/api/sharing_platform.h"
#include "sharing/internal/api/sharing_rpc_client.h"
#include "sharing/internal/api/sharing_rpc_notifier.h"
#include "sharing/internal/api/wifi_adapter.h"
#include "sharing/internal/public/connectivity_manager.h"
#include "sharing/internal/public/context.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/nearby_file_handler.h"
#include "sharing/nearby_sharing_service.h"
#include "sharing/nearby_sharing_service_extension.h"
#include "sharing/nearby_sharing_settings.h"
#include "sharing/outgoing_share_session.h"
#include "sharing/paired_key_verification_runner.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_session.h"
#include "sharing/share_target.h"
#include "sharing/share_target_discovered_callback.h"
#include "sharing/thread_timer.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_update_callback.h"
#include "sharing/wrapped_share_target_discovered_callback.h"

namespace nearby::sharing {
class NearbyShareContactManager;

namespace NearbySharingServiceUnitTests {
class NearbySharingServiceImplTest_CreateShareTarget_Test;
class NearbySharingServiceImplTest_RemoveIncomingPayloads_Test;
};  // namespace NearbySharingServiceUnitTests

// All methods should be called from the same sequence that created the service.
class NearbySharingServiceImpl
    : public NearbySharingService,
      public NearbyShareSettings::Observer,
      public NearbyShareCertificateManager::Observer,
      public ::nearby::AccountManager::Observer,
      public NearbyFastInitiation::Observer,
      public sharing::api::BluetoothAdapter::Observer,
      public sharing::api::WifiAdapter::Observer,
      public NearbyConnectionsManager::IncomingConnectionListener,
      public NearbyConnectionsManager::DiscoveryListener {
  FRIEND_TEST(NearbySharingServiceUnitTests::NearbySharingServiceImplTest,
              CreateShareTarget);
  FRIEND_TEST(NearbySharingServiceUnitTests::NearbySharingServiceImplTest,
              RemoveIncomingPayloads);

 public:
  NearbySharingServiceImpl(
      int32_t vendor_id, std::unique_ptr<nearby::TaskRunner> service_thread,
      Context* context, nearby::sharing::api::SharingPlatform& sharing_platform,
      std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager,
      nearby::analytics::EventLogger* event_logger = nullptr);
  ~NearbySharingServiceImpl() override;

  // NearbySharingService
  void AddObserver(NearbySharingService::Observer* observer) override;
  void RemoveObserver(NearbySharingService::Observer* observer) override;
  bool HasObserver(NearbySharingService::Observer* observer) override;
  void Shutdown(
      std::function<void(StatusCodes)> status_codes_callback) override;
  ABSL_DEPRECATED("Use the variant with vendor ID instead.")
  void RegisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      ShareTargetDiscoveredCallback* discovery_callback, SendSurfaceState state,
      std::function<void(StatusCodes)> status_codes_callback) override;
  void RegisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      ShareTargetDiscoveredCallback* discovery_callback, SendSurfaceState state,
      Advertisement::BlockedVendorId blocked_vendor_id,
      bool disable_wifi_hotspot,
      std::function<void(StatusCodes)> status_codes_callback) override;
  void UnregisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      std::function<void(StatusCodes)> status_codes_callback) override;
  void RegisterReceiveSurface(
      TransferUpdateCallback* transfer_callback, ReceiveSurfaceState state,
      Advertisement::BlockedVendorId vendor_id,
      std::function<void(StatusCodes)> status_codes_callback) override;
  void UnregisterReceiveSurface(
      TransferUpdateCallback* transfer_callback,
      std::function<void(StatusCodes)> status_codes_callback) override;
  void ClearForegroundReceiveSurfaces(
      std::function<void(StatusCodes)> status_codes_callback) override;
  bool IsTransferring() const override;
  bool IsScanning() const override;
  bool IsBluetoothPresent() const override;
  bool IsBluetoothPowered() const override;
  bool IsExtendedAdvertisingSupported() const override;
  bool IsLanConnected() const override;
  bool IsWifiPresent() const override;
  bool IsWifiPowered() const override;
  std::string GetQrCodeUrl() const override;
  void SendAttachments(
      int64_t share_target_id,
      std::unique_ptr<AttachmentContainer> attachment_container,
      std::function<void(StatusCodes)> status_codes_callback) override;
  void Accept(int64_t share_target_id,
              std::function<void(StatusCodes status_codes)>
                  status_codes_callback) override;
  void Reject(int64_t share_target_id,
              std::function<void(StatusCodes status_codes)>
                  status_codes_callback) override;
  void Cancel(int64_t share_target_id,
              std::function<void(StatusCodes status_codes)>
                  status_codes_callback) override;
  bool DidLocalUserCancelTransfer(int64_t share_target_id) override;
  void SetVisibility(
      proto::DeviceVisibility visibility, absl::Duration expiration,
      absl::AnyInvocable<void(StatusCodes status_code) &&> callback) override;
  NearbyShareSettings* GetSettings() override;
  nearby::sharing::api::SharingRpcNotifier* GetRpcNotifier() override;
  NearbyShareLocalDeviceDataManager* GetLocalDeviceDataManager() override;
  NearbyShareContactManager* GetContactManager() override;
  NearbyShareCertificateManager* GetCertificateManager() override;
  AccountManager* GetAccountManager() override;

  // NearbyConnectionsManager::IncomingConnectionListener:
  void OnIncomingConnection(absl::string_view endpoint_id,
                            absl::Span<const uint8_t> endpoint_info,
                            NearbyConnection* connection) override;

  std::string Dump() const override;

  void UpdateFilePathsInProgress(bool update) override;

 private:
  // Cache a recently lost share target to be re-discovered.
  // Purged after expiry_timer.
  struct DiscoveryCacheEntry {
    // If needed, we can add "state" field to model "Tomb" state.
    std::unique_ptr<ThreadTimer> expiry_timer;
    ShareTarget share_target;
  };
  // Internal implementation of methods to avoid using recursive mutex.
  StatusCodes InternalUnregisterSendSurface(
      TransferUpdateCallback* transfer_callback);
  StatusCodes InternalUnregisterReceiveSurface(
      TransferUpdateCallback* transfer_callback);

  // NearbyShareSettings::Observer:
  void OnSettingChanged(absl::string_view key, const Data& data) override;
  void OnIsFastInitiationHardwareSupportedChanged(bool is_supported) override {}

  void OnDeviceNameChanged(absl::string_view device_name);
  void OnDataUsageChanged(proto::DataUsage data_usage);
  void OnCustomSavePathChanged(absl::string_view custom_save_path);
  void OnVisibilityChanged(proto::DeviceVisibility visibility);

  // NearbyShareCertificateManager::Observer:
  void OnPublicCertificatesDownloaded() override;
  void OnPrivateCertificatesChanged() override;

  // AccountManager::Observer:
  void OnLoginSucceeded(absl::string_view account_id) override;
  void OnLogoutSucceeded(absl::string_view account_id,
                         bool credential_error) override;

  // NearbyConnectionsManager::DiscoveryListener:
  void OnEndpointDiscovered(absl::string_view endpoint_id,
                            absl::Span<const uint8_t> endpoint_info) override;
  void OnEndpointLost(absl::string_view endpoint_id) override;

  // Handle the state changes of screen lock.
  void OnLockStateChanged(bool locked);

  // Handle the state changes of bluetooth adapter.
  void AdapterPresentChanged(sharing::api::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(sharing::api::BluetoothAdapter* adapter,
                             bool powered) override;

  // Handle the state changes of Wi-Fi adapter.
  void AdapterPresentChanged(sharing::api::WifiAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(sharing::api::WifiAdapter* adapter,
                             bool powered) override;

  // Handle the hardware error reported that requires PC restart.
  void HardwareErrorReported(NearbyFastInitiation* fast_init) override;

  void SetupBluetoothAdapter();

  absl::flat_hash_map<TransferUpdateCallback*, Advertisement::BlockedVendorId>&
  GetReceiveCallbacksMapFromState(ReceiveSurfaceState state);
  // Retrieves the current receiving vendor ID, defaulting to kNone if no
  // surface has been registered with a different vendor ID. This function will
  // always return one vendor ID, preferring a foreground surface vendor ID if
  // available.
  Advertisement::BlockedVendorId GetReceivingVendorId() const;
  // Retrieves the current sending vendor ID, defaulting to kNone if no
  // surface has been registered with a different vendor ID. This function will
  // always return one vendor ID, preferring a foreground surface vendor ID if
  // available.
  Advertisement::BlockedVendorId GetSendingVendorId() const;

  // Returns true if any foreground send surfaces has requested to disable wifi
  // hotspot.
  bool GetDisableWifiHotspotState() const;

  bool IsVisibleInBackground(proto::DeviceVisibility visibility);
  std::optional<std::vector<uint8_t>> CreateEndpointInfo(
      proto::DeviceVisibility visibility,
      const std::optional<std::string>& device_name) const;
  void StartFastInitiationAdvertising();
  void OnStartFastInitiationAdvertising();
  void OnStartFastInitiationAdvertisingError();
  void StopFastInitiationAdvertising();
  void OnStopFastInitiationAdvertising();

  // Processes endpoint discovered/lost events. We queue up the events to ensure
  // each discovered or lost event is fully handled before the next is run. For
  // example, we don't want to start processing an endpoint-lost event before
  // the corresponding endpoint-discovered event is finished. This is especially
  // important because of the asynchronous steps required to process an
  // endpoint-discovered event.
  void AddEndpointDiscoveryEvent(std::function<void()> event);
  void HandleEndpointDiscovered(absl::string_view endpoint_id,
                                absl::Span<const uint8_t> endpoint_info);
  void HandleEndpointLost(absl::string_view endpoint_id);
  void FinishEndpointDiscoveryEvent();
  void OnOutgoingDecryptedCertificate(
      absl::string_view endpoint_id, absl::Span<const uint8_t> endpoint_info,
      const Advertisement& advertisement,
      std::optional<NearbyShareDecryptedPublicCertificate> certificate);
  void ScheduleCertificateDownloadDuringDiscovery(size_t attempt_count);
  void OnCertificateDownloadDuringDiscoveryTimerFired(size_t attempt_count);

  bool HasAvailableConnectionMediums();
  void InvalidateSurfaceState();
  void InvalidateSendSurfaceState();
  void InvalidateScanningState();
  void InvalidateFastInitiationAdvertising();
  void InvalidateReceiveSurfaceState();
  void InvalidateAdvertisingState();
  void StopAdvertising();
  void StartScanning();
  StatusCodes StopScanning();
  void StopAdvertisingAndInvalidateSurfaceState();

  void InvalidateFastInitiationScanning();
  void StartFastInitiationScanning();
  void OnFastInitiationDevicesDetected();
  void OnFastInitiationDevicesNotDetected();
  void StopFastInitiationScanning();

  void ScheduleRotateBackgroundAdvertisementTimer();
  void OnRotateBackgroundAdvertisementTimerFired();
  // Returns the share target if it has been removed, std::nullopt otherwise.
  std::optional<ShareTarget> RemoveOutgoingShareTargetWithEndpointId(
      absl::string_view endpoint_id);
  void RemoveOutgoingShareTargetAndReportLost(absl::string_view endpoint_id);

  void OnTransferComplete();
  void OnTransferStarted(bool is_incoming);

  void OnOutgoingConnection(absl::Time connect_start_time,
                            NearbyConnection* connection,
                            OutgoingShareSession& session);

  void CreatePayloads(
      OutgoingShareSession& session,
      std::function<void(OutgoingShareSession&, bool)> callback);
  void OnCreatePayloads(std::vector<uint8_t> endpoint_info,
                        OutgoingShareSession& session, bool success);

  void Fail(IncomingShareSession& session, TransferMetadata::Status status);
  void OnIncomingAdvertisementDecoded(
      absl::string_view endpoint_id, IncomingShareSession& session,
      std::unique_ptr<Advertisement> advertisement);
  void OnIncomingTransferUpdate(const IncomingShareSession& session,
                                const TransferMetadata& metadata);
  void OnOutgoingTransferUpdate(OutgoingShareSession& session,
                                const TransferMetadata& metadata);
  void CloseConnection(int64_t share_target_id);
  void OnIncomingDecryptedCertificate(
      absl::string_view endpoint_id, const Advertisement& advertisement,
      int64_t placeholder_share_target_id,
      std::optional<NearbyShareDecryptedPublicCertificate> certificate);
  void OnIncomingConnectionKeyVerificationDone(
      int64_t share_target_id,
      PairedKeyVerificationRunner::PairedKeyVerificationResult result,
      ::location::nearby::proto::sharing::OSType share_target_os_type);
  void OnOutgoingConnectionKeyVerificationDone(
      int64_t share_target_id,
      PairedKeyVerificationRunner::PairedKeyVerificationResult result,
      ::location::nearby::proto::sharing::OSType share_target_os_type);
  void OnReceivedIntroduction(
      int64_t share_target_id,
      std::optional<nearby::sharing::service::proto::IntroductionFrame> frame);
  void OnReceiveConnectionResponse(
      int64_t share_target_id,
      std::optional<nearby::sharing::service::proto::ConnectionResponseFrame>
          frame);
  void OnStorageCheckCompleted(IncomingShareSession& session);
  void OnFrameRead(
      int64_t share_target_id,
      std::optional<nearby::sharing::service::proto::V1Frame> frame);

  void OnConnectionDisconnected(int64_t share_target_id);

  void Cleanup();

  std::optional<ShareTarget> CreateShareTarget(
      absl::string_view endpoint_id, const Advertisement& advertisement,
      const std::optional<NearbyShareDecryptedPublicCertificate>& certificate,
      bool is_incoming);

  void IncomingPayloadTransferUpdate(
      int64_t share_target_id, TransferMetadata metadata);
  void OutgoingPayloadTransferUpdate(
      int64_t share_target_id, TransferMetadata metadata);

  void RemoveIncomingPayloads(const IncomingShareSession& session);

  IncomingShareSession& CreateIncomingShareSession(
      const ShareTarget& share_target, absl::string_view endpoint_id,
      std::optional<NearbyShareDecryptedPublicCertificate> certificate);
  void CreateOutgoingShareSession(
      const ShareTarget& share_target, absl::string_view endpoint_id,
      std::optional<NearbyShareDecryptedPublicCertificate> certificate);

  void MoveToDiscoveryCache(absl::string_view endpoint_id);
  // Immediately expire all timers in the discovery cache. (i.e. report
  // ShareTargetLost)
  void TriggerDiscoveryCacheExpiryTimers();
  // Update the entry in outgoing_share_session_map_ with the new share target
  // and OnShareTargetUpdated is called.
  void DeduplicateInOutgoingShareTarget(
      const ShareTarget& share_target, absl::string_view endpoint_id,
      std::optional<NearbyShareDecryptedPublicCertificate> certificate);

  // Add an entry to the outgoing_share_session_map_
  // and OnShareTargetUpdated is called.
  void DeDuplicateInDiscoveryCache(
      const ShareTarget& share_target, absl::string_view endpoint_id,
      std::optional<NearbyShareDecryptedPublicCertificate> certificate);

  // Looks for a duplicate of the share target in the outgoing share
  // target map. The share target's id is changed to match an existing target if
  // available. Returns true if the duplicate is found.
  bool FindDuplicateInOutgoingShareTargets(absl::string_view endpoint_id,
                                           ShareTarget& share_target);

  // Looks for a duplicate of the share target in the discovery cache.
  // If found, move the share target to the outgoing share target map.
  // The share target's id is updated to match the cached entry.
  // Returns true if the duplicate is found.
  bool FindDuplicateInDiscoveryCache(absl::string_view endpoint_id,
                                     ShareTarget& share_target);

  ShareSession* GetShareSession(int64_t share_target_id);
  IncomingShareSession* GetIncomingShareSession(int64_t share_target_id);
  OutgoingShareSession* GetOutgoingShareSession(int64_t share_target_id);

  std::optional<std::vector<uint8_t>> GetBluetoothMacAddressForShareTarget(
      OutgoingShareSession& session);

  void ClearOutgoingShareSessionMap();
  void UnregisterShareTarget(int64_t share_target_id);

  void OnStartAdvertisingResult(bool used_device_name, Status status);
  void OnStopAdvertisingResult(Status status);
  void OnStartDiscoveryResult(Status status);
  void SetInHighVisibility(bool in_high_visibility);

  // Note: |share_target| is intentionally passed by value. A share target
  // reference could likely be invalidated by the owner during the multistep
  // cancellation process.
  void DoCancel(
      int64_t share_target_id,
      std::function<void(StatusCodes status_codes)> status_codes_callback,
      bool is_initiator_of_cancellation);

  // Monitor connectivity changes.
  void OnNetworkChanged(nearby::ConnectivityManager::ConnectionType type);
  void OnLanConnectedChanged(bool connected);

  // Resets all settings of the nearby sharing service.
  // Resets user preferences to a valid logged out state when |logout| is true.
  // This will clear all preferences, but preserve onboarding state and revert
  // visibility to a state that is valid when logged out. For example:
  // `contacts` -> `off`.
  void ResetAllSettings(bool logout);

  // Runs API/task on the service thread to avoid UI block.
  void RunOnNearbySharingServiceThread(absl::string_view task_name,
                                       absl::AnyInvocable<void()> task);

  // Runs API/task on the service thread with delayed time.
  void RunOnNearbySharingServiceThreadDelayed(absl::string_view task_name,
                                              absl::Duration delay,
                                              absl::AnyInvocable<void()> task);

  // Runs API/task on a random thread.
  void RunOnAnyThread(absl::string_view task_name,
                      absl::AnyInvocable<void()> task);

  // Returns a 1-based position.It is used by group share feature.
  int GetConnectedShareTargetPos();

  // Returns the share target count. It is used by group share feature.
  int GetConnectedShareTargetCount();

  // Returns use case of sender. It is used by group share feature.
  ::location::nearby::proto::sharing::SharingUseCase GetSenderUseCase();

  // Calculates transport type based on attachment size.
  TransportType GetTransportType(const AttachmentContainer& container) const;

  // Update file path for the file attachment.
  void UpdateFilePath(AttachmentContainer& container);
  // Returns true if Shutdown() has been called.
  bool IsShuttingDown();

  // Send initial adapter state to observer for each supported adapter.
  void SendInitialAdapterState(NearbySharingService::Observer* observer);

  bool OutgoingSessionAccept(OutgoingShareSession& session);
  void OnIncomingFilesMetadataUpdated(int64_t share_target_id,
                                      TransferMetadata metadata, bool success);

  // Used to run nearby sharing service APIs.
  std::unique_ptr<TaskRunner> service_thread_;
  Context* const context_;
  nearby::DeviceInfo& device_info_;
  nearby::sharing::api::PreferenceManager& preference_manager_;
  AccountManager& account_manager_;

  std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager_;
  // Used to create analytics events.
  std::unique_ptr<analytics::AnalyticsRecorder> analytics_recorder_;
  std::unique_ptr<nearby::sharing::api::SharingRpcClientFactory>
      nearby_share_client_factory_;
  std::unique_ptr<NearbyShareProfileInfoProvider> profile_info_provider_;
  std::unique_ptr<NearbyShareLocalDeviceDataManager> local_device_data_manager_;
  std::unique_ptr<NearbyShareContactManager> contact_manager_;
  std::unique_ptr<NearbyShareCertificateManager> certificate_manager_;
  std::unique_ptr<NearbyFastInitiation> nearby_fast_initiation_;

  // Used to maintain the settings of nearby sharing.
  std::unique_ptr<NearbyShareSettings> settings_;

  // Accesses the extension methods to Nearby Sharing service.
  std::unique_ptr<NearbySharingServiceExtension> service_extension_;
  NearbyFileHandler file_handler_;
  bool is_screen_locked_ = false;
  std::unique_ptr<ThreadTimer> rotate_background_advertisement_timer_;
  std::unique_ptr<ThreadTimer> certificate_download_during_discovery_timer_;

  // A list of service observers.
  ObserverList<NearbySharingService::Observer> observers_;
  // A map of foreground receiver callbacks -> vendor ID.
  absl::flat_hash_map<TransferUpdateCallback*, Advertisement::BlockedVendorId>
      foreground_receive_callbacks_map_;
  // A map of background receiver callbacks -> vendor ID.
  absl::flat_hash_map<TransferUpdateCallback*, Advertisement::BlockedVendorId>
      background_receive_callbacks_map_;
  // A mapping of foreground transfer callbacks to foreground send surface data.
  absl::flat_hash_map<TransferUpdateCallback*,
                      WrappedShareTargetDiscoveredCallback>
      foreground_send_surface_map_;
  // A mapping of background transfer callbacks to background send surface data.
  absl::flat_hash_map<TransferUpdateCallback*,
                      WrappedShareTargetDiscoveredCallback>
      background_send_surface_map_;

  // Registers the most recent TransferMetadata and ShareTarget used for
  // transitioning notifications between foreground surfaces and background
  // surfaces. Empty if no metadata is available.
  std::optional<std::tuple<ShareTarget, AttachmentContainer, TransferMetadata>>
      last_incoming_metadata_;
  // The most recent outgoing TransferMetadata and ShareTarget.
  std::optional<std::tuple<ShareTarget, AttachmentContainer, TransferMetadata>>
      last_outgoing_metadata_;
  // A map of ShareTarget id to IncomingShareSession. This lets us know which
  // Nearby Connections endpoint and public certificate are related to the
  // incoming share target.
  absl::flat_hash_map<int64_t, IncomingShareSession>
      incoming_share_session_map_;
  // A map of endpoint id to ShareTarget, where each ShareTarget entry
  // directly corresponds to a OutgoingShareSession entry in
  // outgoing_share_target_info_map_;
  absl::flat_hash_map<std::string, ShareTarget> outgoing_share_target_map_;
  // A map of ShareTarget id to OutgoingShareSession. This lets us know which
  // endpoint and public certificate are related to the outgoing share target.
  absl::flat_hash_map<int64_t, OutgoingShareSession>
      outgoing_share_session_map_;
  // A map of Endpoint id to DiscoveryCacheEntry.
  absl::flat_hash_map<std::string, DiscoveryCacheEntry> discovery_cache_;
  // For metrics. The IDs of ShareTargets that are cancelled while trying to
  // establish an outgoing connection.
  absl::flat_hash_set<int64_t> all_cancelled_share_target_ids_;
  // The IDs of ShareTargets that we cancelled the transfer to.
  absl::flat_hash_set<int64_t> locally_cancelled_share_target_ids_;
  // A map from endpoint ID to endpoint info from discovered, contact-based
  // advertisements that could not decrypt any available public certificates.
  // During discovery, if certificates are downloaded, we revisit this map and
  // retry certificate decryption.
  absl::flat_hash_map<std::string, std::vector<uint8_t>>
      discovered_advertisements_to_retry_map_;
  // If the discovered advertisements are retried when public certificates
  // downloaded, we put it in to the retry set. The retried endpoints will not
  // cause new download of public certificates. The purpose is to reduce the
  // unnecessary backend API call.
  absl::flat_hash_set<std::string> discovered_advertisements_retried_set_;

  // A map of ShareTarget id to disconnection timeout callback. Used to only
  // disconnect after a timeout to keep sending any pending payloads.
  absl::flat_hash_map<std::string, std::unique_ptr<ThreadTimer>>
      disconnection_timeout_alarms_;

  // The current advertising power level. PowerLevel::kUnknown while not
  // advertising.
  PowerLevel advertising_power_level_ = PowerLevel::kUnknown;
  // True if we are currently scanning for remote devices.
  std::atomic_bool is_scanning_{false};
  // True if we're currently sending or receiving a file.
  std::atomic_bool is_transferring_{false};
  // True if we're currently receiving a file.
  std::atomic_bool is_receiving_files_{false};
  // True if we're currently sending a file.
  std::atomic_bool is_sending_files_{false};
  // True if we're currently attempting to connect to a remote device.
  std::atomic_bool is_connecting_{false};
  // The time scanning began.
  absl::Time scanning_start_timestamp_;
  // True when we are advertising with a device name visible to everyone.
  std::atomic_bool in_high_visibility_{false};

  // Used to debounce OnNetworkChanged processing.
  std::unique_ptr<ThreadTimer> on_network_changed_delay_timer_;

  // Used to prevent the "Device nearby is sharing" notification from appearing
  // immediately after a completed share.
  std::unique_ptr<ThreadTimer> fast_initiation_scanner_cooldown_timer_;

  // A queue of endpoint-discovered and endpoint-lost events that ensures the
  // events are processed sequentially, in the order received from Nearby
  // Connections. An event is processed either immediately, if there are no
  // other events in the queue, or as soon as the previous event processing
  // finishes. When processing finishes, the event is removed from the queue.
  std::queue<std::function<void()>> endpoint_discovery_events_;

  // Shouldn't schedule new task after shutting down, and skip task if the
  // object is null.
  std::shared_ptr<bool> is_shutting_down_ = nullptr;

  // Used to identify current scanning session.
  int64_t scanning_session_id_ = 0;

  // Used to identify current advertising session.
  int64_t advertising_session_id_ = 0;

  // Used to track the time of screen unlock.
  absl::Time screen_unlock_time_;

  // Whether to update the file paths in transfer progress.
  bool update_file_paths_in_progress_ = false;

  // Used to track the time when share sheet activity starts
  absl::Time share_foreground_send_surface_start_timestamp_;
  std::unique_ptr<nearby::api::AppInfo> app_info_;
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_
