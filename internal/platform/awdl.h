// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_AWDL_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_AWDL_H_
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/types/optional.h"
#include "internal/platform/blocking_queue_stream.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/awdl.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/listeners.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/socket.h"

namespace nearby {

class AwdlSocket : public MediumSocket {
 public:
  AwdlSocket()
      : MediumSocket(location::nearby::proto::connections::Medium::AWDL) {}
  AwdlSocket(const AwdlSocket&) = default;
  AwdlSocket& operator=(const AwdlSocket&) = default;
  ~AwdlSocket() override = default;

  // Creates a physical AwdlSocket from a platform implementation.
  explicit AwdlSocket(std::unique_ptr<api::AwdlSocket> socket)
      : MediumSocket(location::nearby::proto::connections::Medium::AWDL),
        impl_(socket.release()) {}

  // Creates a virtual AwdlSocket from a virtual output stream.
  explicit AwdlSocket(OutputStream* virtual_output_stream)
      : MediumSocket(location::nearby::proto::connections::Medium::AWDL),
        blocking_queue_input_stream_(std::make_shared<BlockingQueueStream>()),
        virtual_output_stream_(virtual_output_stream),
        is_virtual_socket_(true) {}

  // Returns the InputStream of the AwdlSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the AwdlSocket object is destroyed.
  InputStream& GetInputStream() override {
    return IsVirtualSocket() ? *blocking_queue_input_stream_
                             : impl_->GetInputStream();
  }

  // Returns the OutputStream of the AwdlSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the AwdlSocket object is destroyed.
  OutputStream& GetOutputStream() override {
    return IsVirtualSocket() ? *virtual_output_stream_
                             : impl_->GetOutputStream();
  }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override {
    if (IsVirtualSocket()) {
      NEARBY_LOGS(INFO) << "Multiplex: Closing virtual socket: " << this;
      blocking_queue_input_stream_->Close();
      virtual_output_stream_->Close();
      CloseLocal();  // This will trigger MultiplexSocket::OnVirtualSocketClosed
      return {Exception::kSuccess};
    }
    return impl_->Close();
  }

  // Returns true if this is a virtual socket.
  bool IsVirtualSocket() override { return is_virtual_socket_; }

  // Creates a virtual socket only with outputstream.
  MediumSocket* CreateVirtualSocket(
      const std::string& salted_service_id_hash_key, OutputStream* outputstream,
      location::nearby::proto::connections::Medium medium,
      absl::flat_hash_map<std::string, std::shared_ptr<MediumSocket>>*
          virtual_sockets_ptr) override;

  /** Feeds the received incoming data to the client. */
  void FeedIncomingData(ByteArray data) override {
    if (!IsVirtualSocket()) {
      NEARBY_LOGS(INFO) << "Feeding data on a physical socket is not allowed.";
      return;
    }
    blocking_queue_input_stream_->Write(data);
  }

  // Returns true if a socket is usable. If this method returns false,
  // it is not safe to call any other method.
  // NOTE(socket validity):
  // Socket created by a default public constructor is not valid, because
  // it is missing platform implementation.
  // The only way to obtain a valid socket is through connection, such as
  // an object returned by AwdlMedium::Connect
  // These methods may also return an invalid socket if connection failed for
  // any reason.
  bool IsValid() const {
    if (is_virtual_socket_) return true;
    return impl_ != nullptr;
  }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging purposes.
  // Returned reference will remain valid for while AwdlSocket object is
  // itself valid. Typically AwdlSocket lifetime matches duration of the
  // connection, and is controlled by end user, since they hold the instance.
  api::AwdlSocket& GetImpl() { return *impl_; }

 private:
  std::shared_ptr<api::AwdlSocket> impl_;
  absl::flat_hash_map<std::string, std::shared_ptr<MediumSocket>>*
      virtual_sockets_ptr_ = nullptr;
  std::shared_ptr<BlockingQueueStream> blocking_queue_input_stream_ = nullptr;
  OutputStream* virtual_output_stream_ = nullptr;
  bool is_virtual_socket_ = false;
};

class AwdlServerSocket final {
 public:
  AwdlServerSocket() = default;
  AwdlServerSocket(const AwdlServerSocket&) = default;
  AwdlServerSocket& operator=(const AwdlServerSocket&) = default;
  ~AwdlServerSocket() = default;
  explicit AwdlServerSocket(std::unique_ptr<api::AwdlServerSocket> socket)
      : impl_(std::move(socket)) {}

  // Returns ip address.
  std::string GetIPAddress() { return impl_->GetIPAddress(); }

  // Returns port.
  int GetPort() { return impl_->GetPort(); }

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  AwdlSocket Accept() {
    std::unique_ptr<api::AwdlSocket> socket = impl_->Accept();
    if (!socket) {
      NEARBY_LOGS(INFO) << "AwdlServerSocket Accept() failed on server socket: "
                        << this;
    }
    return AwdlSocket(std::move(socket));
  }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() {
    NEARBY_LOGS(INFO) << "AwdlServerSocket Closing:: " << this;
    return impl_->Close();
  }

  bool IsValid() const { return impl_ != nullptr; }
  api::AwdlServerSocket& GetImpl() { return *impl_; }

 private:
  std::shared_ptr<api::AwdlServerSocket> impl_;
};

// Container of operations that can be performed over the Awdl medium.
class AwdlMedium {
 public:
  using Platform = api::ImplementationPlatform;

  struct DiscoveredServiceCallback {
    absl::AnyInvocable<void(NsdServiceInfo service_info,
                            const std::string& service_type)>
        service_discovered_cb =
            DefaultCallback<NsdServiceInfo, const std::string&>();
    absl::AnyInvocable<void(NsdServiceInfo service_info,
                            const std::string& service_type)>
        service_lost_cb = DefaultCallback<NsdServiceInfo, const std::string&>();
  };

  struct DiscoveryCallbackInfo {
    std::string service_id;
    DiscoveredServiceCallback medium_callback;
  };

  AwdlMedium() : impl_(Platform::CreateAwdlMedium()) {}
  ~AwdlMedium() = default;

  // Starts Awdl advertising.
  //
  // nsd_service_info - NsdServiceInfo data that's advertised through mDNS
  //                    service.
  // On success if the service is now advertising.
  // On error if the service cannot start to advertise or the nsd_type in
  // NsdServiceInfo has been passed previously which StopAdvertising is not
  // been called.
  bool StartAdvertising(const NsdServiceInfo& nsd_service_info);

  // Stops Awdl advertising.
  //
  // nsd_service_info - NsdServiceInfo data that's advertised through mDNS
  //                    service.
  // On success if the service stops advertising.
  // On error if the service cannot stop advertising or the nsd_type in
  // NsdServiceInfo cannot be found.
  bool StopAdvertising(const NsdServiceInfo& nsd_service_info);

  // Returns true once the Awdl discovery has been initiated.
  bool StartDiscovery(const std::string& service_id,
                      const std::string& service_type,
                      DiscoveredServiceCallback callback);

  // Returns true once service_type is associated to existing callback. If the
  // callback is the last found then Awdl discovery will be stopped.
  bool StopDiscovery(const std::string& service_type);

  // Returns a new AwdlSocket.
  // On Success, AwdlSocket::IsValid() returns true.
  AwdlSocket ConnectToService(const NsdServiceInfo& remote_service_info,
                              CancellationFlag* cancellation_flag);

  // Returns a new AwdlSocket by ip address and port.
  // On Success, AwdlSocket::IsValid()returns true.
  AwdlSocket ConnectToService(const std::string& ip_address, int port,
                              CancellationFlag* cancellation_flag);

  // Returns a new AwdlServerSocket.
  // On Success, AwdlServerSocket::IsValid() returns true.
  AwdlServerSocket ListenForService(int port = 0) {
    return AwdlServerSocket(impl_->ListenForService(port));
  }

  // Returns the port range as a pair of min and max port.
  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange() {
    return impl_->GetDynamicPortRange();
  }

  // Both SW and HW support Awdl Medium
  bool IsValid() const {
    // When impl_ is not nullptr, it means platform layer SW is implemented for
    // this Medium
    if (impl_ == nullptr) return false;
    // A network connection to a primary router exist, also implied that HW is
    // existed and enabled.
    return impl_->IsNetworkConnected();
  }

  api::AwdlMedium& GetImpl() { return *impl_; }

 private:
  Mutex mutex_;
  std::unique_ptr<api::AwdlMedium> impl_;

  // Used to keep the map from service type to discovery callback.
  absl::flat_hash_map<std::string, std::unique_ptr<DiscoveryCallbackInfo>>
      service_type_to_callback_map_ ABSL_GUARDED_BY(mutex_);

  // Used to keep the map from service type to services with the type.
  absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>
      service_type_to_services_map_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_AWDL_H_
