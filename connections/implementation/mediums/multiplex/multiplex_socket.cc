// Copyright 2024 Google LLC
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

#include "connections/implementation/mediums/multiplex/multiplex_socket.h"

#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "connections/implementation/mediums/multiplex/multiplex_frames.h"
#include "connections/implementation/mediums/multiplex/multiplex_output_stream.h"
#include "connections/implementation/mediums/utils.h"
#include "internal/platform/atomic_boolean.h"
#include "internal/platform/base64_utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/future.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/socket.h"
#include "internal/platform/types.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace multiplex {

namespace {
// It is defined for the receiver which send the first packet to the sender
// without getting salt from it yet. The fake salt reminds sender to get the
// correct socket from `virtualSockets` without remapping it.
constexpr absl::string_view kFakeSalt = "RECEIVER_CONDIMENT";

// The max duration to wait for the reader thread to stop.
constexpr absl::Duration kTimeoutForReaderThreadStop = absl::Milliseconds(100);

}  // namespace

using ::location::nearby::mediums::ConnectionResponseFrame;
using ::location::nearby::mediums::MultiplexControlFrame;
using ::location::nearby::mediums::MultiplexDataFrame;
using ::location::nearby::mediums::MultiplexFrame;
using ::location::nearby::proto::connections::Medium;
using ::location::nearby::proto::connections::Medium_Name;
using ConnectionResponseCode = ConnectionResponseFrame::ConnectionResponseCode;

// AtomicBoolean is trivial destructible, so it is safe to use it as a static
// variable.
AtomicBoolean MultiplexSocket::is_shutting_down_{false}; // NOLINT

void MultiplexSocket::ListenForIncomingConnection(
    const std::string& service_id, Medium type,
    MultiplexIncomingConnectionCb incoming_connection_cb) {
  GetIncomingConnectionCallbacks().emplace(
      std::pair<std::string, Medium>(service_id, type),
      std::move(incoming_connection_cb));
}

void MultiplexSocket::StopListeningForIncomingConnection(
    const std::string& service_id, Medium type) {
  GetIncomingConnectionCallbacks().erase(
      std::pair<std::string, Medium>(service_id, type));
}

MultiplexSocket::MultiplexSocket(std::shared_ptr<MediumSocket> physical_socket)
    : physical_socket_ptr_(physical_socket),
      multiplex_output_stream_{&physical_socket_ptr_->GetOutputStream(),
                               enabled_},
      physical_reader_(&physical_socket_ptr_->GetInputStream()),
      medium_(physical_socket_ptr_->GetMedium()) {}

absl::flat_hash_map<std::pair<std::string, Medium>,
                    MultiplexIncomingConnectionCb>&
MultiplexSocket::GetIncomingConnectionCallbacks() {
  static std::aligned_storage_t<
      sizeof(absl::flat_hash_map<std::pair<std::string, Medium>,
                                 MultiplexIncomingConnectionCb>),
      alignof(absl::flat_hash_map<std::pair<std::string, Medium>,
                                  MultiplexIncomingConnectionCb>)>
      storage;
  static absl::flat_hash_map<std::pair<std::string, Medium>,
                             MultiplexIncomingConnectionCb>*
      incoming_connection_callbacks =
          new (&storage) absl::flat_hash_map<std::pair<std::string, Medium>,
                                             MultiplexIncomingConnectionCb>();

  return *incoming_connection_callbacks;
}

MultiplexSocket* MultiplexSocket::CreateIncomingSocket(
    std::shared_ptr<MediumSocket> physical_socket,
    const std::string& service_id, std::int32_t first_frame_len) {
  while (is_shutting_down_.Get()) {
    NEARBY_LOGS(WARNING)
        << "Shutting down is going on, wait for 2ms to create incoming socket";
    absl::SleepFor(absl::Milliseconds(2));
  }
  static MultiplexSocket* multiplex_incoming_socket = nullptr;
  switch (physical_socket->GetMedium()) {
    case Medium::BLUETOOTH:
      static std::aligned_storage_t<sizeof(MultiplexSocket),
                                    alignof(MultiplexSocket)>
          storage_bt;
      multiplex_incoming_socket =
          new (&storage_bt) MultiplexSocket(physical_socket);
      break;
    case Medium::BLE:
      static std::aligned_storage_t<sizeof(MultiplexSocket),
                                    alignof(MultiplexSocket)>
          storage_ble;
      multiplex_incoming_socket =
          new (&storage_ble) MultiplexSocket(physical_socket);
      break;
    case Medium::WIFI_LAN:
      static std::aligned_storage_t<sizeof(MultiplexSocket),
                                    alignof(MultiplexSocket)>
          storage_wlan;
      multiplex_incoming_socket =
          new (&storage_wlan) MultiplexSocket(physical_socket);
      break;
    default:
      NEARBY_LOGS(ERROR) << __func__ << "Unsupported medium: "
                         << physical_socket->GetMedium();
      multiplex_incoming_socket = nullptr;
      return multiplex_incoming_socket;
  }
  NEARBY_LOGS(INFO) << "CreateIncomingSocket with serviceId=" << service_id
                    << ", serviceIdHashSalt=" << kFakeSalt;

  multiplex_incoming_socket->CreateFirstVirtualSocket(service_id,
                                                      (std::string)kFakeSalt);
  multiplex_incoming_socket->StartReaderThread(first_frame_len);

  return multiplex_incoming_socket;
}

MultiplexSocket* MultiplexSocket::CreateOutgoingSocket(
    std::shared_ptr<MediumSocket> physical_socket,
    const std::string& service_id, const std::string& service_id_hash_salt) {
  while (is_shutting_down_.Get()) {
    NEARBY_LOGS(WARNING)
        << "Shutting down is going on, wait for 2ms to create outgoing socket";
    absl::SleepFor(absl::Milliseconds(2));
  }
  static MultiplexSocket* multiplex_outgoing_socket = nullptr;
  switch (physical_socket->GetMedium()) {
    case Medium::BLUETOOTH:
      static std::aligned_storage_t<sizeof(MultiplexSocket),
                                    alignof(MultiplexSocket)>
          storage_bt;
      multiplex_outgoing_socket =
          new (&storage_bt) MultiplexSocket(physical_socket);
      break;
    case Medium::BLE:
      static std::aligned_storage_t<sizeof(MultiplexSocket),
                                    alignof(MultiplexSocket)>
          storage_ble;
      multiplex_outgoing_socket =
          new (&storage_ble) MultiplexSocket(physical_socket);
      break;
    case Medium::WIFI_LAN:
      static std::aligned_storage_t<sizeof(MultiplexSocket),
                                    alignof(MultiplexSocket)>
          storage_wlan;
      multiplex_outgoing_socket =
          new (&storage_wlan) MultiplexSocket(physical_socket);
      break;
    default:
      NEARBY_LOGS(ERROR) << __func__ << "Unsupported medium: "
                         << physical_socket->GetMedium();
      return multiplex_outgoing_socket;
  }
  NEARBY_LOGS(INFO) << "CreateOutgoingSocket with serviceId=" << service_id
                    << ", serviceIdHashSalt=" << service_id_hash_salt;

  multiplex_outgoing_socket->CreateFirstVirtualSocket(service_id,
                                                      service_id_hash_salt);
  multiplex_outgoing_socket->StartReaderThread(0);
  return multiplex_outgoing_socket;
}

MultiplexSocket* MultiplexSocket::CreateOutgoingSocket(
    std::shared_ptr<MediumSocket> physical_socket,
    const std::string& service_id) {
  return CreateOutgoingSocket(physical_socket, service_id,
                              Utils::GenerateSalt());
}

MediumSocket* MultiplexSocket::CreateFirstVirtualSocket(
    const std::string& service_id, const std::string& service_id_hash_salt) {
  auto output_stream =
      multiplex_output_stream_.CreateVirtualOutputStreamForFirstVirtualSocket(
          service_id, service_id_hash_salt);

  MutexLock lock(&virtual_socket_mutex_);
  std::string salted_service_id_hash_key =
      GenerateServiceIdHashKeyWithSalt(service_id, service_id_hash_salt);
  NEARBY_LOGS(INFO) << __func__ << " for service_id=" << service_id
                    << ", salt=" << service_id_hash_salt
                    << ", salted_service_id_hash_key="
                    << salted_service_id_hash_key;
  MediumSocket* virtual_socket = physical_socket_ptr_->CreateVirtualSocket(
      salted_service_id_hash_key, output_stream, medium_, &virtual_sockets_);

  virtual_socket->AddOnSocketClosedListener(
      std::make_unique<absl::AnyInvocable<void()>>(
          [this, service_id]() { OnVirtualSocketClosed(service_id); }));

  if (!IsEnabled()) {
    NEARBY_LOGS(INFO) << __func__ << ": Register multiplex enabled callback";
    virtual_socket->RegisterMultiplexEnabledCallback(enable_cb_);
  }

  return virtual_socket;
}

MediumSocket* MultiplexSocket::CreateVirtualSocket(
    const std::string& service_id, const std::string& service_id_hash_salt) {
  auto output_stream = multiplex_output_stream_.CreateVirtualOutputStream(
      service_id, service_id_hash_salt);
  MutexLock lock(&virtual_socket_mutex_);
  std::string salted_service_id_hash_key =
      GenerateServiceIdHashKeyWithSalt(service_id, service_id_hash_salt);

  NEARBY_LOGS(INFO) << __func__ << "service_id=" << service_id
                    << ", salt=" << service_id_hash_salt
                    << ", salted_service_id_hash_key="
                    << salted_service_id_hash_key;

  MediumSocket* virtual_socket = physical_socket_ptr_->CreateVirtualSocket(
      salted_service_id_hash_key, output_stream, medium_, &virtual_sockets_);

  virtual_socket->AddOnSocketClosedListener(
      std::make_unique<absl::AnyInvocable<void()>>(
          [this, service_id]() { OnVirtualSocketClosed(service_id); }));

  return virtual_socket;
}

MediumSocket* MultiplexSocket::GetVirtualSocket(const std::string& service_id) {
  MutexLock lock(&virtual_socket_mutex_);
  NEARBY_LOGS(INFO) << __func__ << " service_id=" << service_id << ", Salt="
                    << multiplex_output_stream_.GetServiceIdHashSalt(service_id)
                    << ", virtual_sockets_.size()=" << virtual_sockets_.size();
  auto item = virtual_sockets_.find(GenerateServiceIdHashKeyWithSalt(
      service_id, multiplex_output_stream_.GetServiceIdHashSalt(service_id)));
  if (item == virtual_sockets_.end()) {
    NEARBY_LOGS(INFO) << "Not found!";
    return nullptr;
  }
  return item->second.get();
}

int MultiplexSocket::GetVirtualSocketCount() {
  MutexLock lock(&virtual_socket_mutex_);
  return virtual_sockets_.size();
}

void MultiplexSocket::ListVirtualSocket() {
  NEARBY_LOGS(INFO) << __func__
                    << " virtual_sockets_.size()=" << virtual_sockets_.size();
  for (auto& [service_id_hash_key, virtual_socket] : virtual_sockets_) {
    NEARBY_LOGS(INFO) << __func__
                      << " service_id_hash_key=" << service_id_hash_key
                      << ", virtual_socket=" << virtual_socket;
  }
}

std::shared_ptr<Future<ConnectionResponseCode>>
MultiplexSocket::RegisterConnectionResponse(const std::string& service_id) {
  auto future = std::make_shared<Future<ConnectionResponseCode>>();
  connection_response_futures_.emplace(service_id, future);

  return future;
}

void MultiplexSocket::UnRegisterConnectionResponse(
    const std::string& service_id) {
  connection_response_futures_.erase(service_id);
}

MediumSocket* MultiplexSocket::EstablishVirtualSocket(
    const std::string& service_id) {
  if (!IsEnabled()) {
    NEARBY_LOGS(ERROR)
        << "MultiplexSocket is disabled, cannot establish virtual socket.";
    return nullptr;
  }

  std::string service_id_hash_salt = Utils::GenerateSalt();
  auto future = RegisterConnectionResponse(service_id);

  multiplex_output_stream_.WriteConnectionRequestFrame(service_id,
                                                       service_id_hash_salt);
  auto result =
      future->Get(FeatureFlags::GetInstance()
                      .GetFlags()
                      .multiplex_socket_connection_response_timeout_millis);
  if (!result.ok()) {
    NEARBY_LOGS(ERROR) << __func__
                       << "EstablishVirtualSocket failed with response code="
                       << result.exception();
    return nullptr;
  }

  ConnectionResponseCode response_code = result.GetResult();
  switch (response_code) {
    case ConnectionResponseFrame::CONNECTION_ACCEPTED:
      NEARBY_LOGS(INFO) << "EstablishVirtualSocket after remote response to"
                           " accept the connection with service_id="
                        << service_id
                        << ", service_id_hash_salt=" << service_id_hash_salt;
      return CreateVirtualSocket(service_id, service_id_hash_salt);
    case ConnectionResponseFrame::NOT_LISTENING:
      NEARBY_LOGS(ERROR) << "EstablishVirtualSocket failed for service_id="
                         << service_id
                         << ", service_id_hash_salt=" << service_id_hash_salt
                         << " with response code=NOT_LISTENING";
      break;
    default:
      NEARBY_LOGS(ERROR) << "EstablishVirtualSocket failed for service_id="
                         << service_id
                         << ", service_id_hash_salt=" << service_id_hash_salt
                         << " with response code=UNKNOWN_RESPONSE_CODE";
      break;
  }
  return nullptr;
}

void MultiplexSocket::StartReaderThread(std::int32_t first_frame_len) {
  if (is_shutdown_) {
    NEARBY_LOGS(WARNING) << "Stop to start reader thread since socket is "
                            "shutdown.";
    return;
  }
  reader_thread_shutdown_barrier_ = std::make_unique<CountDownLatch>(1);
  physical_reader_thread_.Execute([this, first_frame_len]() {
    NEARBY_LOGS(INFO) << __func__ << " Reader thread starts.";
    auto first_frame_len_copy = first_frame_len;
    while (!is_shutdown_) {
      bool fail = false;
      ExceptionOr<ByteArray> bytes;
      ExceptionOr<std::int32_t> read_int;
      if (first_frame_len_copy > 0) {
        read_int = ExceptionOr<std::int32_t>(first_frame_len);
        first_frame_len_copy = 0;
      } else {
        read_int = Base64Utils::ReadInt(physical_reader_);
      }
      if (!read_int.ok()) {
        NEARBY_LOGS(WARNING)
            << __func__ << "Failed to read. Exception:" << read_int.exception();
        fail = true;
      } else {
        auto length = read_int.result();
        NEARBY_VLOG(1) << __func__ << " length:" << length;

        if (length < 0 || length > FeatureFlags::GetInstance()
                                       .GetFlags()
                                       .connection_max_frame_length) {
          // Ignore the failure because not only one client use this
          // connection.
          NEARBY_LOGS(WARNING)
              << __func__ << "Failed to read because received a invalid length "
              << length << ", but continue to read.";
          continue;
        }

        bytes = physical_reader_->ReadExactly(length);
        if (!bytes.ok()) {
          NEARBY_LOGS(WARNING)
              << __func__ << "Read data exception:" << bytes.exception();
          fail = true;
        }
      }
      if (fail) {
        reader_thread_shutdown_barrier_->CountDown();
        return;
      }

      ExceptionOr<MultiplexFrame> frame_exc =
          multiplex::FromBytes(bytes.result());
      if (!frame_exc.ok()) {
        HandleOfflineFrame(bytes.result());
        continue;
      }

      if (!IsEnabled()) {
        // The reader thread will only be enabled when local device
        // supports multiplex if we received a multiplex frame from
        // the remote, it means that the remote and the local both
        // support multiplex as well. So it is safe to just turn on
        // the feature at this point.
        NEARBY_LOGS(INFO)
            << __func__
            << " Received a multiplex frame while not enabled, enable "
               "multiplex.";
        Enable();
      }
      const auto& frame = frame_exc.result();
      auto salted_service_id_hash =
          ByteArray{std::move(frame.header().salted_service_id_hash())};
      auto service_id_hash_salt = frame.header().has_service_id_hash_salt()
                                      ? frame.header().service_id_hash_salt()
                                      : "";
      switch (frame.frame_type()) {
        case MultiplexFrame::CONTROL_FRAME:
          HandleControlFrame(salted_service_id_hash, service_id_hash_salt,
                             frame.control_frame());
          break;
        case MultiplexFrame::DATA_FRAME:
          NEARBY_VLOG(1) << "service_id_hash_salt: " << service_id_hash_salt;
          HandleDataFrame(salted_service_id_hash, service_id_hash_salt,
                          frame.data_frame());
          break;
        default:
          NEARBY_LOGS(WARNING)
              << __func__ << " Received MultiplexFrame with unknown frame type "
              << frame.frame_type();
      }
    }
  });
}

void MultiplexSocket::HandleOfflineFrame(const ByteArray& bytes) {
  MutexLock lock(&virtual_socket_mutex_);
  NEARBY_LOGS(INFO) << __func__
                    << " Virtual_socket num:" << virtual_sockets_.size();
  if (virtual_sockets_.size() == 1) {
    auto item = virtual_sockets_.begin();
    if (item->second == nullptr) {
      NEARBY_LOGS(WARNING) << "Expected one live socket, but found null.";
      return;
    }
    NEARBY_LOGS(INFO) << __func__ << "FeedIncomingData:" << std::string(bytes);
    item->second->FeedIncomingData(Base64Utils::IntToBytes(bytes.size()));
    item->second->FeedIncomingData(bytes);
  }
}

void MultiplexSocket::HandleControlFrame(
    const ByteArray& salted_service_id_hash,
    const std::string& service_id_hash_salt,
    const MultiplexControlFrame& frame) {
  switch (frame.control_frame_type()) {
    case MultiplexControlFrame::CONNECTION_REQUEST:
      RunOffloadThread("CONNECTION_REQUEST", [this, salted_service_id_hash,
                                              service_id_hash_salt] {
        HandleConnectionRequest(salted_service_id_hash, service_id_hash_salt);
      });
      break;
    case MultiplexControlFrame::CONNECTION_RESPONSE:
      NEARBY_LOGS(INFO)
          << __func__ << "Received an CONNECTION_RESPONSE frame."
          << " salted_service_id_hash: " << std::string(salted_service_id_hash)
          << ", service_id_hash_salt: " << service_id_hash_salt
          << ", ConnectionResponseCode: "
          << frame.connection_response_frame().connection_response_code();

      RunOffloadThread("CONNECTION_RESPONSE", [this, salted_service_id_hash,
                                               service_id_hash_salt,
                                               frame = frame] {
        HandleConnectionResponse(salted_service_id_hash, service_id_hash_salt,
                                 frame.connection_response_frame());
      });
      break;
    case MultiplexControlFrame::DISCONNECTION:
      RunOffloadThread("DISCONNECTION", [this, salted_service_id_hash] {
        HandleDisconnection(salted_service_id_hash);
      });
      break;
    default:
      NEARBY_LOGS(WARNING) << __func__ << "Received an unknown frame type "
                           << frame.control_frame_type();
      break;
  }
}

void MultiplexSocket::HandleConnectionRequest(
    const ByteArray& salted_service_id_hash,
    const std::string& service_id_hash_salt) {
  if (!IsEnabled()) {
    NEARBY_LOGS(WARNING) << "Received a CONNECTION_REQUEST frame on medium "
                         << Medium_Name(medium_)
                         << " but status is disabled, ignore it.";
    return;
  }

  std::string salted_service_id_hash_key =
      GenerateServiceIdHashKey(salted_service_id_hash);
  MultiplexIncomingConnectionCb* incoming_connection_callback = nullptr;
  std::string listening_service_id = "";
  for (auto& [service_id_medium_pair, callback] :
       GetIncomingConnectionCallbacks()) {
    if (GenerateServiceIdHashWithSalt(service_id_medium_pair.first,
                                      service_id_hash_salt) ==
        salted_service_id_hash) {
      incoming_connection_callback = &callback;
      listening_service_id = service_id_medium_pair.first;
    }
  }

  if (incoming_connection_callback == nullptr || listening_service_id.empty()) {
    NEARBY_LOGS(INFO) << "There's no client listening for hash salt : "
                      << service_id_hash_salt
                      << ", hash key : " << salted_service_id_hash_key
                      << " on medium " << Medium_Name(medium_);

    NEARBY_LOGS(INFO) << "The size of incomingConnectionCallbacks : "
                      << GetIncomingConnectionCallbacks().size();
    if (!multiplex_output_stream_.WriteConnectionResponseFrame(
            salted_service_id_hash, service_id_hash_salt,
            ConnectionResponseFrame::NOT_LISTENING)) {
      NEARBY_LOGS(INFO) << __func__ << "Failed to write NOT_LISTENING frame.";
    }
    return;
  }
  NEARBY_LOGS(INFO) << "Accept new virtual socket request service ID : "
                    << listening_service_id
                    << ", hash salt : " << service_id_hash_salt
                    << ", hash key : " << salted_service_id_hash_key
                    << " on medium " << Medium_Name(medium_);

  if (!multiplex_output_stream_.WriteConnectionResponseFrame(
          salted_service_id_hash, service_id_hash_salt,
          ConnectionResponseFrame::CONNECTION_ACCEPTED)) {
    NEARBY_LOGS(INFO) << "Failed to write CONNECTION_ACCEPTED frame.";
    return;
  }

  NEARBY_VLOG(1)
      << "EstablishVirtualSocket after local device accept the connection "
         "with serviceId="
      << listening_service_id << ", serviceIdHashSalt=" << service_id_hash_salt;
  MediumSocket* virtual_socket =
      CreateVirtualSocket(listening_service_id, service_id_hash_salt);
  (*incoming_connection_callback)(std::move(listening_service_id),
                                  virtual_socket);
}

void MultiplexSocket::HandleConnectionResponse(
    const ByteArray& salted_service_id_hash,
    const std::string& service_id_hash_salt,
    const ConnectionResponseFrame& frame) {
  NEARBY_LOGS(INFO) << __func__ << "connection_response_code: "
                    << frame.connection_response_code();
  for (auto& [service_id, future] : connection_response_futures_) {
    if (GenerateServiceIdHashWithSalt(service_id, service_id_hash_salt) ==
        salted_service_id_hash) {
      if (future != nullptr) {
        future->Set(frame.connection_response_code());
        NEARBY_LOGS(INFO) << __func__
                          << "Set the future for serviceId=" << service_id
                          << ", serviceIdHashSalt=" << service_id_hash_salt
                          << " with response code="
                          << frame.connection_response_code();
        return;
      }
    }
  }

  NEARBY_LOGS(WARNING)
      << __func__
      << "Received a CONNECTION_RESPONSE frame but no client waiting for "
         "service ID Hash Key"
      << GenerateServiceIdHashKey(salted_service_id_hash);
}

void MultiplexSocket::HandleDisconnection(
    const ByteArray& salted_service_id_hash) {
  std::string salted_service_id_hash_key =
      GenerateServiceIdHashKey(salted_service_id_hash);
  {
    MutexLock lock(&virtual_socket_mutex_);
    auto item = virtual_sockets_.find(salted_service_id_hash_key);
    if (item != virtual_sockets_.end()) {
      NEARBY_LOGS(INFO)
          << "Received a DISCONNECTION frame to disconnect virtual socket for "
             "salted service ID Hash Key "
          << salted_service_id_hash_key;
    } else {
      NEARBY_LOGS(WARNING)
          << "Received a DISCONNECTION frame but there's no alive socket to "
             "disconnect for service ID Hash Key "
          << salted_service_id_hash_key;
    }
  }
}

void MultiplexSocket::HandleDataFrame(const ByteArray& salted_service_id_hash,
                                      const std::string& service_id_hash_salt,
                                      const MultiplexDataFrame& frame) {
  std::string salted_service_id_hash_key =
      GenerateServiceIdHashKey(salted_service_id_hash);
  MediumSocket* virtual_socket = nullptr;
  if (service_id_hash_salt.empty()) {
    {
      MutexLock lock(&virtual_socket_mutex_);
      auto item = virtual_sockets_.find(salted_service_id_hash_key);
      if (item != virtual_sockets_.end()) {
        virtual_socket = item->second.get();
      }
    }
  } else {
    virtual_socket =
        ReMapAndGetVirtualSocket(salted_service_id_hash, service_id_hash_salt);
  }

  if (virtual_socket != nullptr) {
    NEARBY_VLOG(1)
        << "Received a DATA frame to feed virtual socket for salted service ID "
           "Hash Key "
        << salted_service_id_hash_key;
    virtual_socket->FeedIncomingData(ByteArray(frame.data()));
  } else {
    NEARBY_LOGS(WARNING)
        << "Received a DATA frame but there's no alive socket to feed for "
           "salted service ID Hash Key "
        << salted_service_id_hash_key;
  }
}

void MultiplexSocket::OnPhysicalSocketClosed() {
  RunOffloadThread("Shutdown", [this]() { Shutdown(); });
}

void MultiplexSocket::OnVirtualSocketClosed(const std::string& service_id) {
  NEARBY_LOGS(INFO) << __func__ << " for service_id:" << service_id;
  CountDownLatch latch(1);
  bool shutdown = false;
  RunOffloadThread("VirtualSocketClosed", [this, service_id, &latch,
                                           &shutdown]() {
    NEARBY_LOGS(INFO) << "Try to close Virtual socket: " << service_id;
    MediumSocket* virtual_socket = GetVirtualSocket(service_id);
    {
      MutexLock lock(&virtual_socket_mutex_);
      NEARBY_LOGS(INFO) << "virtual_socket:" << virtual_socket;
      if (virtual_socket != nullptr) {
        auto salted_service_id_hash_key = GenerateServiceIdHashKeyWithSalt(
            service_id,
            multiplex_output_stream_.GetServiceIdHashSalt(service_id));
        multiplex_output_stream_.Close(service_id);
        virtual_sockets_.erase(salted_service_id_hash_key);
        NEARBY_LOGS(INFO) << "Erase Virtual socket with service_id: "
                          << service_id
                          << ", hash_key: " << salted_service_id_hash_key;
        ListVirtualSocket();

        if (virtual_sockets_.empty()) {
          NEARBY_LOGS(INFO) << "Close the physical socket because all virtual "
                               "sockets disconnected.";
          is_shutting_down_.Set(true);
          Shutdown();
          shutdown = true;
        }
      } else {
        NEARBY_LOGS(INFO) << "Virtual socket(" << service_id << ") not found";
      }
    }
    latch.CountDown();
  });

  if (!latch.Await(absl::Milliseconds(1000)).result()) {
    NEARBY_LOGS(ERROR) << "Timeout to close virtual socket";
  }

  if (shutdown) {
    NEARBY_LOGS(INFO)
        << "Shutdown single_thread_offloader_ and physical_reader_thread_";
    single_thread_offloader_.Shutdown();
    physical_reader_thread_.Shutdown();
    is_shutting_down_.Set(false);
  }
}

MediumSocket* MultiplexSocket::ReMapAndGetVirtualSocket(
    const ByteArray& salted_service_id_hash,
    const std::string& service_id_hash_salt) {
  std::string salted_service_id_hash_key =
      GenerateServiceIdHashKey(salted_service_id_hash);
  NEARBY_VLOG(1) << "ReMapAndGetVirtualSocket with serviceIdHashSalt="
                 << service_id_hash_salt
                 << ", saltedServiceIdHashKey=" << salted_service_id_hash_key;
  {
    MutexLock lock(&virtual_socket_mutex_);
    for (auto& [hash_key, virtual_socket] : virtual_sockets_) {
      auto output_stream =
          down_cast<MultiplexOutputStream::VirtualOutputStream*>(
              &(virtual_socket->GetOutputStream()));
      if (output_stream == nullptr) {
        continue;
      }
      if (!output_stream->IsFirstVirtualOutputStream()) {
        continue;
      }
      if ((service_id_hash_salt == kFakeSalt) ||
          (hash_key == salted_service_id_hash_key)) {
        return virtual_socket.get();
      } else {
        NEARBY_LOGS(INFO) << "Remap the virtualSockets.";
        output_stream->SetserviceIdHashSalt(service_id_hash_salt);
        auto virtual_socket_tmp = virtual_socket;
        NEARBY_LOGS(INFO) << "virtual_socket before:" << virtual_socket;
        virtual_sockets_.erase(hash_key);
        virtual_sockets_[salted_service_id_hash_key] = virtual_socket_tmp;
        ListVirtualSocket();
        return virtual_socket_tmp.get();
      }
    }
  }

  NEARBY_LOGS(INFO) << "Failed to remap the virtualSockets.";
  return nullptr;
}

void MultiplexSocket::RunOffloadThread(const std::string& name,
                                       absl::AnyInvocable<void()> runnable) {
  single_thread_offloader_.Execute(name, std::move(runnable));
}

void MultiplexSocket::Shutdown() {
  NEARBY_LOGS(INFO) << __func__ << " start";
  if (is_shutdown_) {
    NEARBY_LOGS(INFO) << __func__ << " Already shutdown";
    return;
  }

  multiplex_output_stream_.Shutdown();
  physical_socket_ptr_->Close();

  if (reader_thread_shutdown_barrier_) {
    reader_thread_shutdown_barrier_->Await(kTimeoutForReaderThreadStop);
  }

  GetIncomingConnectionCallbacks().clear();
  connection_response_futures_.clear();

  is_shutdown_ = true;
  enabled_.Set(false);
  NEARBY_LOGS(INFO) << __func__ << " end";
}

void MultiplexSocket::ShutdownAll() {
  NEARBY_LOGS(INFO) << __func__ << " start";
  if (is_shutdown_) {
    NEARBY_LOGS(WARNING) << __func__ << " Already shutdown";
    return;
  }

  CountDownLatch latch(1);
  RunOffloadThread("VirtualSocketClosed", [this, &latch]() {
    {
      MutexLock lock(&virtual_socket_mutex_);
      multiplex_output_stream_.CloseAll();
      virtual_sockets_.clear();

      Shutdown();
    }
    latch.CountDown();
  });

  if (!latch
           .Await(FeatureFlags::GetInstance()
                      .GetFlags()
                      .mediums_frame_write_timeout_millis +
                  absl::Milliseconds(100))
           .result()) {
    NEARBY_LOGS(ERROR) << "Timeout to close virtual socket";
  }

  NEARBY_LOGS(INFO)
      << "Shutdown single_thread_offloader_ and physical_reader_thread_";
  single_thread_offloader_.Shutdown();
  physical_reader_thread_.Shutdown();
  NEARBY_LOGS(INFO) << __func__ << " end";
}

}  // namespace multiplex
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
