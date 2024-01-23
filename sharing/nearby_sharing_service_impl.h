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

#include <filesystem>  // NOLINT(build/c++17)
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "internal/analytics/event_logger.h"
#include "internal/base/observer_list.h"
#include "internal/network/url.h"
#include "internal/platform/device_info.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/platform/task_runner.h"
#include "internal/platform/timer.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/advertisement.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/attachment.h"
#include "sharing/attachment_info.h"
#include "sharing/certificates/nearby_share_certificate_manager.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/certificates/nearby_share_private_certificate.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/common/nearby_share_profile_info_provider.h"
#include "sharing/fast_initiation/nearby_fast_initiation.h"
#include "sharing/incoming_share_target_info.h"
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
#include "sharing/nearby_sharing_decoder.h"
#include "sharing/nearby_sharing_service.h"
#include "sharing/nearby_sharing_service_extension.h"
#include "sharing/nearby_sharing_settings.h"
#include "sharing/outgoing_share_target_info.h"
#include "sharing/paired_key_verification_runner.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_target.h"
#include "sharing/share_target_discovered_callback.h"
#include "sharing/share_target_info.h"
#include "sharing/text_attachment.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_update_callback.h"
#include "sharing/wifi_credentials_attachment.h"

namespace nearby {
namespace sharing {

class NearbyShareContactManager;

namespace NearbySharingServiceUnitTests {
class NearbySharingServiceImplTest_CreateShareTarget_Test;
};

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

 public:
  NearbySharingServiceImpl(
      Context* context, nearby::sharing::api::SharingPlatform& sharing_platform,
      NearbySharingDecoder* decoder,
      std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager,
      nearby::analytics::EventLogger* event_logger = nullptr);
  ~NearbySharingServiceImpl() override;

  // NearbySharingService
  void AddObserver(NearbySharingService::Observer* observer) override;
  void RemoveObserver(NearbySharingService::Observer* observer) override;
  bool HasObserver(NearbySharingService::Observer* observer) override;
  void Shutdown(
      std::function<void(StatusCodes)> status_codes_callback) override;
  void RegisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      ShareTargetDiscoveredCallback* discovery_callback, SendSurfaceState state,
      std::function<void(StatusCodes)> status_codes_callback) override;
  void UnregisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      ShareTargetDiscoveredCallback* discovery_callback,
      std::function<void(StatusCodes)> status_codes_callback) override;
  void RegisterReceiveSurface(
      TransferUpdateCallback* transfer_callback, ReceiveSurfaceState state,
      std::function<void(StatusCodes)> status_codes_callback) override;
  void UnregisterReceiveSurface(
      TransferUpdateCallback* transfer_callback,
      std::function<void(StatusCodes)> status_codes_callback) override;
  void ClearForegroundReceiveSurfaces(
      std::function<void(StatusCodes)> status_codes_callback) override;
  bool IsInHighVisibility() const override;
  bool IsTransferring() const override;
  bool IsReceivingFile() const override;
  bool IsSendingFile() const override;
  bool IsScanning() const override;
  bool IsConnecting() const override;
  bool IsBluetoothPresent() const override;
  bool IsBluetoothPowered() const override;
  bool IsExtendedAdvertisingSupported() const override;
  bool IsLanConnected() const override;
  bool IsWifiPresent() const override;
  bool IsWifiPowered() const override;
  std::string GetQrCodeUrl() const override;
  void SendAttachments(
      const ShareTarget& share_target,
      std::vector<std::unique_ptr<Attachment>> attachments,
      std::function<void(StatusCodes)> status_codes_callback) override;
  void Accept(const ShareTarget& share_target,
              std::function<void(StatusCodes status_codes)>
                  status_codes_callback) override;
  void Reject(const ShareTarget& share_target,
              std::function<void(StatusCodes status_codes)>
                  status_codes_callback) override;
  void Cancel(const ShareTarget& share_target,
              std::function<void(StatusCodes status_codes)>
                  status_codes_callback) override;
  bool DidLocalUserCancelTransfer(const ShareTarget& share_target) override;
  void Open(const ShareTarget& share_target,
            std::function<void(StatusCodes status_codes)> status_codes_callback)
      override;
  void OpenUrl(const ::nearby::network::Url& url) override;
  void CopyText(absl::string_view text) override;
  void JoinWifiNetwork(absl::string_view ssid,
                       absl::string_view password) override;
  void SetArcTransferCleanupCallback(std::function<void()> callback) override;
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
  // Internal implementation of methods to avoid using recursive mutex.
  StatusCodes InternalUnregisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      ShareTargetDiscoveredCallback* discovery_callback);
  StatusCodes InternalUnregisterReceiveSurface(
      TransferUpdateCallback* transfer_callback);

  // NearbyShareSettings::Observer:
  void OnSettingChanged(absl::string_view key, const Data& data) override;
  void OnIsFastInitiationHardwareSupportedChanged(bool is_supported) override;

  void OnEnabledChanged(bool enabled);
  void OnFastInitiationNotificationStateChanged(
      proto::FastInitiationNotificationState state);
  void OnDeviceNameChanged(absl::string_view device_name);
  void OnDataUsageChanged(proto::DataUsage data_usage);
  void OnCustomSavePathChanged(absl::string_view custom_save_path);
  void OnVisibilityChanged(proto::DeviceVisibility visibility);
  void OnAllowedContactsChanged(absl::Span<const std::string> allowed_contacts);
  void OnIsOnboardingCompleteChanged(bool is_complete);
  void OnIsReceivingChanged(bool is_receiving);

  // NearbyShareCertificateManager::Observer:
  void OnPublicCertificatesDownloaded() override;
  void OnPrivateCertificatesChanged() override;

  // AccountManager::Observer:
  void OnLoginSucceeded(absl::string_view account_id) override;
  void OnLogoutSucceeded(absl::string_view account_id) override;

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

  ObserverList<TransferUpdateCallback>& GetReceiveCallbacksFromState(
      ReceiveSurfaceState state);
  bool IsVisibleInBackground(proto::DeviceVisibility visibility);
  std::optional<std::vector<uint8_t>> CreateEndpointInfo(
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
  void OnOutgoingAdvertisementDecoded(
      absl::string_view endpoint_id, absl::Span<const uint8_t> endpoint_info,
      std::unique_ptr<Advertisement> advertisement);
  void OnOutgoingDecryptedCertificate(
      absl::string_view endpoint_id, absl::Span<const uint8_t> endpoint_info,
      std::unique_ptr<Advertisement> advertisement,
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
  void RemoveOutgoingShareTargetWithEndpointId(absl::string_view endpoint_id);

  void OnTransferComplete();
  void OnTransferStarted(bool is_incoming);

  void ReceivePayloads(
      ShareTarget share_target,
      std::function<void(StatusCodes status_codes)> status_codes_callback);
  StatusCodes SendPayloads(const ShareTarget& share_target);
  void OnUniquePathFetched(int64_t attachment_id, int64_t payload_id,
                           std::function<void(Status)> callback,
                           std::filesystem::path path);
  void OnPayloadPathRegistered(Status status);
  void OnPayloadPathsRegistered(
      const ShareTarget& share_target, std::unique_ptr<bool> aggregated_success,
      std::function<void(StatusCodes status_codes)> status_codes_callback);

  void OnOutgoingConnection(const ShareTarget& share_target,
                            absl::Time connect_start_time,
                            NearbyConnection* connection);
  void SendIntroduction(const ShareTarget& share_target,
                        std::optional<std::string> four_digit_token);

  void CreatePayloads(ShareTarget share_target,
                      std::function<void(ShareTarget, bool)> callback);
  void OnCreatePayloads(std::vector<uint8_t> endpoint_info,
                        ShareTarget share_target, bool success);
  void OnOpenFiles(ShareTarget share_target,
                   std::function<void(ShareTarget, bool)> callback,
                   std::vector<NearbyFileHandler::FileInfo> files);
  std::vector<Payload> CreateTextPayloads(
      const std::vector<TextAttachment>& attachments);
  std::vector<Payload> CreateWifiCredentialsPayloads(
      const std::vector<WifiCredentialsAttachment>& attachments);

  void WriteResponseFrame(
      NearbyConnection& connection,
      nearby::sharing::service::proto::ConnectionResponseFrame::Status
          response_status);
  void WriteCancelFrame(NearbyConnection& connection);
  void WriteProgressUpdateFrame(NearbyConnection& connection,
                                std::optional<bool> start_transfer,
                                std::optional<float> progress);
  void Fail(const ShareTarget& share_target, TransferMetadata::Status status);
  void OnIncomingAdvertisementDecoded(
      absl::string_view endpoint_id, ShareTarget placeholder_share_target,
      std::unique_ptr<Advertisement> advertisement);
  void OnIncomingTransferUpdate(const ShareTarget& share_target,
                                const TransferMetadata& metadata);
  void OnOutgoingTransferUpdate(const ShareTarget& share_target,
                                const TransferMetadata& metadata);
  void CloseConnection(const ShareTarget& share_target);
  void OnIncomingDecryptedCertificate(
      absl::string_view endpoint_id,
      std::unique_ptr<Advertisement> advertisement,
      ShareTarget placeholder_share_target,
      std::optional<NearbyShareDecryptedPublicCertificate> certificate);
  void RunPairedKeyVerification(
      const ShareTarget& share_target, absl::string_view endpoint_id,
      std::function<
          void(PairedKeyVerificationRunner::PairedKeyVerificationResult,
               ::location::nearby::proto::sharing::OSType)>
          callback);
  void OnIncomingConnectionKeyVerificationDone(
      ShareTarget share_target, std::optional<std::string> four_digit_token,
      PairedKeyVerificationRunner::PairedKeyVerificationResult result,
      ::location::nearby::proto::sharing::OSType share_target_os_type);
  void OnOutgoingConnectionKeyVerificationDone(
      const ShareTarget& share_target,
      std::optional<std::string> four_digit_token,
      PairedKeyVerificationRunner::PairedKeyVerificationResult result,
      ::location::nearby::proto::sharing::OSType share_target_os_type);
  void RefreshUIOnDisconnection(ShareTarget share_target);
  void ReceiveIntroduction(ShareTarget share_target,
                           std::optional<std::string> four_digit_token);
  void OnReceivedIntroduction(
      ShareTarget share_target, std::optional<std::string> four_digit_token,
      std::optional<nearby::sharing::service::proto::V1Frame> frame);
  void ReceiveConnectionResponse(ShareTarget share_target);
  void OnReceiveConnectionResponse(
      ShareTarget share_target,
      std::optional<nearby::sharing::service::proto::V1Frame> frame);
  void OnStorageCheckCompleted(ShareTarget share_target,
                               std::optional<std::string> four_digit_token,
                               bool is_out_of_storage);
  void OnFrameRead(
      ShareTarget share_target,
      std::optional<nearby::sharing::service::proto::V1Frame> frame);
  void HandleCertificateInfoFrame(
      const nearby::sharing::service::proto::CertificateInfoFrame&
          certificate_frame);
  void HandleProgressUpdateFrame(
      const ShareTarget& share_target,
      const nearby::sharing::service::proto::ProgressUpdateFrame&
          progress_update_frame);

  void OnIncomingConnectionDisconnected(const ShareTarget& share_target);
  void OnOutgoingConnectionDisconnected(const ShareTarget& share_target);

  void OnIncomingMutualAcceptanceTimeout(const ShareTarget& share_target);
  void OnOutgoingMutualAcceptanceTimeout(const ShareTarget& share_target);

  void Cleanup();

  std::optional<ShareTarget> CreateShareTarget(
      absl::string_view endpoint_id,
      std::unique_ptr<Advertisement> advertisement,
      std::optional<NearbyShareDecryptedPublicCertificate> certificate,
      bool is_incoming);

  void OnPayloadTransferUpdate(ShareTarget share_target,
                               TransferMetadata metadata);
  bool OnIncomingPayloadsComplete(ShareTarget& share_target);
  void RemoveIncomingPayloads(ShareTarget share_target);
  void Disconnect(const ShareTarget& share_target, TransferMetadata metadata);
  void OnDisconnectingConnectionTimeout(absl::string_view endpoint_id);
  void OnDisconnectingConnectionDisconnected(const ShareTarget& share_target,
                                             absl::string_view endpoint_id);

  ShareTargetInfo& GetOrCreateShareTargetInfo(const ShareTarget& share_target,
                                              absl::string_view endpoint_id);

  ShareTargetInfo* GetShareTargetInfo(const ShareTarget& share_target);
  IncomingShareTargetInfo* GetIncomingShareTargetInfo(
      const ShareTarget& share_target);
  OutgoingShareTargetInfo* GetOutgoingShareTargetInfo(
      const ShareTarget& share_target);

  NearbyConnection* GetConnection(const ShareTarget& share_target);
  std::optional<std::vector<uint8_t>> GetBluetoothMacAddressForShareTarget(
      const ShareTarget& share_target);

  void ClearOutgoingShareTargetInfoMap();
  void SetAttachmentPayloadId(const Attachment& attachment, int64_t payload_id);
  std::optional<int64_t> GetAttachmentPayloadId(int64_t attachment_id);
  void UnregisterShareTarget(const ShareTarget& share_target);

  void OnStartAdvertisingResult(bool used_device_name, Status status);
  void OnStopAdvertisingResult(Status status);
  void OnStartDiscoveryResult(Status status);
  void SetInHighVisibility(bool in_high_visibility);

  // Note: |share_target| is intentionally passed by value. A share target
  // reference could likely be invalidated by the owner during the multistep
  // cancellation process.
  void DoCancel(
      ShareTarget share_target,
      std::function<void(StatusCodes status_codes)> status_codes_callback,
      bool is_initiator_of_cancellation);

  void AbortAndCloseConnectionIfNecessary(TransferMetadata::Status status,
                                          const ShareTarget& share_target);

  // Monitor connectivity changes.
  void OnNetworkChanged(nearby::ConnectivityManager::ConnectionType type);
  void OnLanConnectedChanged(bool connected);

  // Resets all settings of the nearby sharing service.
  // Resets user preferences to a valid logged out state when |logout| is true.
  // This will clear all preferences, but preserve onboarding state and revert
  // visibility to a state that is valid when logged out. For example:
  // `contacts` -> `off`.
  void ResetAllSettings(bool logout);

  // Checks whether SDK should auto-accept remote attachments.
  bool ShouldSelfShareAutoAccept(const ShareTarget& share_target) const;

  // Checks whether we should accept transfer.
  bool ReadyToAccept(const ShareTarget& share_target,
                     TransferMetadata::Status status) const;

  // Runs API/task on the service thread to avoid UI block.
  void RunOnNearbySharingServiceThread(absl::string_view task_name,
                                       std::function<void()> task);

  // Runs API/task on the service thread with delayed time.
  void RunOnNearbySharingServiceThreadDelayed(absl::string_view task_name,
                                              absl::Duration delay,
                                              std::function<void()> task);

  // Runs API/task on a random thread.
  void RunOnAnyThread(absl::string_view task_name, std::function<void()> task);

  // Returns a 1-based position.It is used by group share feature.
  int GetConnectedShareTargetPos(const ShareTarget& target);

  // Returns the share target count. It is used by group share feature.
  int GetConnectedShareTargetCount();

  // Returns use case of sender. It is used by group share feature.
  ::location::nearby::proto::sharing::SharingUseCase GetSenderUseCase();

  // Calculates transport type on share target.
  TransportType GetTransportType(const ShareTarget& share_target) const;

  // Update file path for the file attachment.
  void UpdateFilePath(ShareTarget& share_target);

  Context* const context_;
  nearby::DeviceInfo& device_info_;
  nearby::sharing::api::PreferenceManager& preference_manager_;
  AccountManager& account_manager_;
  NearbySharingDecoder* const decoder_;

  std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager_;
  // Scanner which is non-null when we are performing a background scan for
  // remote devices that are attempting to share.
  std::unique_ptr<nearby::sharing::api::SharingRpcClientFactory>
      nearby_share_client_factory_;
  std::unique_ptr<NearbyShareProfileInfoProvider> profile_info_provider_;
  std::unique_ptr<NearbyShareLocalDeviceDataManager> local_device_data_manager_;
  std::unique_ptr<NearbyShareContactManager> contact_manager_;
  std::unique_ptr<NearbyShareCertificateManager> certificate_manager_;
  std::unique_ptr<NearbyFastInitiation> nearby_fast_initiation_;

  // Used to create analytics events.
  std::unique_ptr<analytics::AnalyticsRecorder> analytics_recorder_;

  // Used to maintain the settings of nearby sharing.
  std::unique_ptr<NearbyShareSettings> settings_;

  // Accesses the extension methods to Nearby Sharing service.
  std::unique_ptr<NearbySharingServiceExtension> service_extension_;
  NearbyFileHandler file_handler_;
  bool is_screen_locked_ = false;
  std::unique_ptr<Timer> rotate_background_advertisement_timer_;
  std::unique_ptr<Timer> certificate_download_during_discovery_timer_;
  std::unique_ptr<Timer> process_shutdown_pending_timer_;

  // A list of service observers.
  ObserverList<NearbySharingService::Observer> observers_;
  // A list of foreground receivers.
  ObserverList<TransferUpdateCallback> foreground_receive_callbacks_;
  // A list of background receivers.
  ObserverList<TransferUpdateCallback> background_receive_callbacks_;
  // A list of foreground receivers for transfer updates on the send surface.
  ObserverList<TransferUpdateCallback> foreground_send_transfer_callbacks_;
  // A list of foreground receivers for discovered device updates on the send
  // surface.
  ObserverList<ShareTargetDiscoveredCallback>
      foreground_send_discovery_callbacks_;
  // A list of background receivers for transfer updates on the send surface.
  ObserverList<TransferUpdateCallback> background_send_transfer_callbacks_;
  // A list of background receivers for discovered device updates on the send
  // surface.
  ObserverList<ShareTargetDiscoveredCallback>
      background_send_discovery_callbacks_;

  // Registers the most recent TransferMetadata and ShareTarget used for
  // transitioning notifications between foreground surfaces and background
  // surfaces. Empty if no metadata is available.
  std::optional<std::pair<ShareTarget, TransferMetadata>>
      last_incoming_metadata_;
  // The most recent outgoing TransferMetadata and ShareTarget.
  std::optional<std::pair<ShareTarget, TransferMetadata>>
      last_outgoing_metadata_;
  // A map of ShareTarget id to IncomingShareTargetInfo. This lets us know which
  // Nearby Connections endpoint and public certificate are related to the
  // incoming share target.
  absl::flat_hash_map<int64_t, IncomingShareTargetInfo>
      incoming_share_target_info_map_;
  // A map of endpoint id to ShareTarget, where each ShareTarget entry
  // directly corresponds to a OutgoingShareTargetInfo entry in
  // outgoing_share_target_info_map_;
  absl::flat_hash_map<std::string, ShareTarget> outgoing_share_target_map_;
  // A map of ShareTarget id to OutgoingShareTargetInfo. This lets us know which
  // endpoint and public certificate are related to the outgoing share target.
  absl::flat_hash_map<int64_t, OutgoingShareTargetInfo>
      outgoing_share_target_info_map_;
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

  // A mapping of Attachment ID to additional AttachmentInfo related to the
  // Attachment.
  absl::flat_hash_map<int64_t, AttachmentInfo> attachment_info_map_;

  // This alarm is used to disconnect the sharing connection if both sides do
  // not press accept within the timeout.
  std::unique_ptr<Timer> mutual_acceptance_timeout_alarm_;

  // A map of ShareTarget id to disconnection timeout callback. Used to only
  // disconnect after a timeout to keep sending any pending payloads.
  absl::flat_hash_map<std::string, std::unique_ptr<Timer>>
      disconnection_timeout_alarms_;

  // The current advertising power level. PowerLevel::kUnknown while not
  // advertising.
  PowerLevel advertising_power_level_ = PowerLevel::kUnknown;
  // True if we are currently scanning for remote devices.
  bool is_scanning_ = false;
  // True if we're currently sending or receiving a file.
  bool is_transferring_ = false;
  // True if we're currently receiving a file.
  bool is_receiving_files_ = false;
  // True if we're currently sending a file.
  bool is_sending_files_ = false;
  // True if we're currently attempting to connect to a remote device.
  bool is_connecting_ = false;
  // The time scanning began.
  absl::Time scanning_start_timestamp_;
  // True when we are advertising with a device name visible to everyone.
  bool in_high_visibility_ = false;
  // The time attachments are sent after a share target is selected. This is
  // used to time the process from selecting a share target to writing the
  // introduction frame (last frame before receiver gets notified).
  absl::Time send_attachments_timestamp_;
  // Whether an incoming share has been accepted, and we are waiting to log the
  // time from acceptance to the start of payload transfer.
  bool is_waiting_to_record_accept_to_transfer_start_metric_ = false;
  // Time at which an incoming transfer was accepted. This is used to calculate
  // the time between an incoming share being accepted and the first payload
  // byte being processed.
  absl::Time incoming_share_accepted_timestamp_;
  std::unique_ptr<Timer> clear_recent_nearby_process_shutdown_count_timer_;

  // Used to debounce OnNetworkChanged processing.
  std::unique_ptr<Timer> on_network_changed_delay_timer_;

  // Used to prevent the "Device nearby is sharing" notification from appearing
  // immediately after a completed share.
  std::unique_ptr<Timer> fast_initiation_scanner_cooldown_timer_;

  // A queue of endpoint-discovered and endpoint-lost events that ensures the
  // events are processed sequentially, in the order received from Nearby
  // Connections. An event is processed either immediately, if there are no
  // other events in the queue, or as soon as the previous event processing
  // finishes. When processing finishes, the event is removed from the queue.
  std::queue<std::function<void()>> endpoint_discovery_events_;

  // Called when cleanup for ARC is needed as part of the transfer.
  std::function<void()> arc_transfer_cleanup_callback_;

  // Used to run nearby sharing service APIs.
  std::unique_ptr<TaskRunner> service_thread_ = nullptr;

  // Shouldn't schedule new task after shutting down, and skip task if the
  // object is null.
  std::shared_ptr<bool> is_shutting_down_ = nullptr;

  // Tracks the path registration.
  struct PathRegistrationStatus {
    ShareTarget share_target;
    uint32_t expected_count;
    uint32_t current_count;
    std::function<void(StatusCodes status_codes)> status_codes_callback;
    bool status;
  };

  PathRegistrationStatus path_registration_status_;

  // Used to identify current scanning session.
  int64_t scanning_session_id_ = 0;

  // Used to identify current advertising session.
  int64_t advertising_session_id_ = 0;

  // Used to identify current receiving session.
  int64_t receiving_session_id_ = 0;

  // Used to track the time of screen unlock.
  absl::Time screen_unlock_time_;

  // Whether to update the file paths in transfer progress.
  bool update_file_paths_in_progress_ = false;

  // Used to track the time when share sheet activity starts
  absl::Time share_foreground_send_surface_start_timestamp_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_
