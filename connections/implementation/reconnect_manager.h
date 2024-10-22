// Copyright 2023 Google LLC
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

#ifndef CORE_INTERNAL_RECONNECTION_MANAGER_H_
#define CORE_INTERNAL_RECONNECTION_MANAGER_H_

#include <memory>
#include <string>
#include <utility>

#include "securegcm/ukey2_handshake.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/encryption_runner.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/mediums/bluetooth_classic.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/mutex.h"
#include "internal/platform/scheduled_executor.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace connections {
using AutoReconnectFrame = ::location::nearby::connections::AutoReconnectFrame;
using OfflineFrame = ::location::nearby::connections::OfflineFrame;
using Medium = ::location::nearby::proto::connections::Medium;
using DisconnectionReason =
    ::location::nearby::proto::connections::DisconnectionReason;

class ReconnectManager {
 public:
  ReconnectManager(Mediums& mediums, EndpointChannelManager& channel_manager);
  ~ReconnectManager();

  struct AutoReconnectCallback {
    absl::AnyInvocable<void(ClientProxy*, const std::string& endpoint_id)>
        on_reconnect_success_cb;
    absl::AnyInvocable<void(ClientProxy*, const std::string& endpoint_id,
                             bool send_disconnection_notification,
                             DisconnectionReason disconnection_reason)>
        on_reconnect_failure_cb;
  };

  struct ReconnectMetadata {
    ReconnectMetadata(bool is_incoming, AutoReconnectCallback callback,
                      bool send_disconnection_notification,
                      DisconnectionReason disconnection_reason,
                      const std::string& reconnect_service_id)
        : reconnect_service_id(reconnect_service_id),
          is_incoming(is_incoming),
          send_disconnection_notification(send_disconnection_notification),
          disconnection_reason(disconnection_reason) {
      reconnect_cb = std::move(callback);
    }
    ~ReconnectMetadata() noexcept = default;
    ReconnectMetadata(ReconnectMetadata&&) = default;
    ReconnectMetadata& operator=(ReconnectMetadata&&) = default;

    AutoReconnectCallback reconnect_cb;
    std::string reconnect_service_id;
    bool is_incoming;
    bool send_disconnection_notification;
    DisconnectionReason disconnection_reason =
        DisconnectionReason::UNKNOWN_DISCONNECTION_REASON;
  };

  // The entry point for AutoReconect, this API will do the auto reconnect for
  // specified "endpoint_id" which connection was lost before.
  bool AutoReconnect(ClientProxy* client, const std::string& endpoint_id,
                     AutoReconnectCallback& callback,
                     bool send_disconnection_notification,
                     DisconnectionReason disconnection_reason);

 private:
  class MediumConnectionProcessor {
   public:
    virtual ~MediumConnectionProcessor() = default;

    virtual bool IsMediumRadioOn() const = 0;
    virtual bool IsListeningForIncomingConnections() const = 0;
    virtual bool StartListeningForIncomingConnections() = 0;
    virtual void StopListeningForIncomingConnections() = 0;
    virtual bool ConnectOverMedium() = 0;
    virtual bool SupportEncryptionDisabled() = 0;
    virtual void QuietlyCloseChannelAndSocket() = 0;
  };

  class BaseMediumImpl : public MediumConnectionProcessor {
   public:
    BaseMediumImpl(ClientProxy* client, const std::string& endpoint_id,
                   const std::string& reconnect_service_id, bool is_incoming,
                   Medium medium, Mediums* mediums,
                   EndpointChannelManager* channel_manager,
                   ReconnectManager& reconnect_manager)
        : client_(client),
          endpoint_id_(endpoint_id),
          reconnect_service_id_(reconnect_service_id),
          is_incoming_(is_incoming),
          medium_(medium),
          mediums_(mediums),
          channel_manager_(channel_manager),
          reconnect_manager_(reconnect_manager) {}
    ~BaseMediumImpl() override = default;

    bool Run();

   protected:
    ClientProxy* client_;
    std::string endpoint_id_;
    std::string reconnect_service_id_;
    bool is_incoming_;
    Medium medium_ = Medium::UNKNOWN_MEDIUM;
    Mediums* mediums_;
    EndpointChannelManager* channel_manager_;
    std::unique_ptr<EndpointChannel> reconnect_channel_;
    ReconnectManager& reconnect_manager_;
    void OnIncomingConnection(const std::string& reconnect_service_id);

   private:
    bool RehostForIncomingConnections();
    bool ReconnectToRemoteDevice();

    std::string ReadClientIntroductionFrame(EndpointChannel* endpoint_channel);
    bool ReadClientIntroductionAckFrame(EndpointChannel* endpoint_channel);
    bool ReplaceChannelForEndpoint(
        ClientProxy* client, const std::string& endpoint_id,
        std::unique_ptr<EndpointChannel> new_channel,
        bool support_encryption_disabled,
        absl::AnyInvocable<void(void)> stop_listening_incoming_connection);
    EncryptionRunner::ResultListener GetResultListener();
    void OnEncryptionSuccessRunnable(
        const std::string& endpoint_id,
        std::unique_ptr<securegcm::UKey2Handshake> ukey2,
        const std::string& auth_token, const ByteArray& raw_auth_token);
    void OnEncryptionFailureRunnable(const std::string& endpoint_id,
                                     EndpointChannel* endpoint_channel);
    void ProcessSuccessfulReconnection(
        const std::string& endpoint_id,
        absl::AnyInvocable<void(void)> stop_listening_incoming_connection);
    void ProcessFailedReconnection(
        const std::string& endpoint_id,
        absl::AnyInvocable<void(void)> stop_listening_incoming_connection);
    void StopListeningIfAllConnected(
        const std::string& reconnect_service_id,
        absl::AnyInvocable<void(void)> stop_listening_incoming_connection,
        bool force_stop);
    bool HasPendingIncomingConnections(const std::string& reconnect_service_id);
    void CancelClearHostTimeoutAlarm(const std::string& service_id);
    void ClearReconnectData(const std::string& service_id, bool is_incoming);

    std::unique_ptr<CountDownLatch> wait_encryption_to_finish_;
    bool replace_channel_succeed_;
  };

  class BluetoothImpl : public BaseMediumImpl {
   public:
    BluetoothImpl(ClientProxy* client_proxy, const std::string& endpoint_id,
                 const std::string& reconnect_service_id, bool is_incoming,
                 Medium medium, Mediums* mediums,
                 EndpointChannelManager* channel_manager,
                 ReconnectManager& reconnect_manager)
        : BaseMediumImpl(client_proxy, endpoint_id, reconnect_service_id,
                         is_incoming, medium, mediums, channel_manager,
                         reconnect_manager),
          bluetooth_medium_(mediums_->GetBluetoothClassic()) {}

    bool IsMediumRadioOn() const override;
    bool IsListeningForIncomingConnections() const override;
    bool StartListeningForIncomingConnections() override;
    void StopListeningForIncomingConnections() override;
    bool ConnectOverMedium() override;
    bool SupportEncryptionDisabled() override;
    void QuietlyCloseChannelAndSocket() override;

   private:
    void OnIncomingBluetoothConnection(ClientProxy* client,
                                       const std::string& upgrade_service_id,
                                       BluetoothSocket socket);

    BluetoothClassic& bluetooth_medium_;
    BluetoothSocket bluetooth_socket_;
  };

  bool Start(bool is_incoming, ClientProxy* client_proxy,
             const std::string& endpoint_id,
             const std::string& reconnect_service_id, Medium medium);
  bool RunOnce(bool is_incoming, ClientProxy* client,
               const std::string& endpoint_id,
               const std::string& reconnect_service_id, Medium medium);

  void ClearReconnectData(ClientProxy* client,
                          const std::string& reconnect_service_id,
                          bool is_incoming);
  void Shutdown();

  Mediums* mediums_;
  EndpointChannelManager* channel_manager_;
  EncryptionRunner encryption_runner_;

  SingleThreadExecutor reconnect_executor_;
  ScheduledExecutor alarm_executor_;
  SingleThreadExecutor incoming_connection_cb_executor_;
  SingleThreadExecutor encryption_cb_executor_;

  mutable RecursiveMutex mutex_;
  absl::flat_hash_map<std::string, std::unique_ptr<CancelableAlarm>>
      listen_timeout_alarm_by_service_id_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string, std::unique_ptr<EndpointChannel>>
      new_endpoint_channels_;
  absl::flat_hash_map<std::string, ReconnectMetadata> endpoint_id_metadata_map_;
  absl::flat_hash_set<std::string> resumed_endpoints_;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_RECONNECTION_MANAGER_H_
