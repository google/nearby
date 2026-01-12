// Copyright 2020 Google LLC
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

#include "connections/implementation/base_endpoint_channel.h"

#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "connections/implementation/analytics/analytics_recorder.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/offline_frames.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/base64_utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace connections {

namespace {
using ::location::nearby::analytics::proto::ConnectionsLog;
using ::location::nearby::proto::connections::Medium::BLE;
using ::location::nearby::proto::connections::Medium::BLE_L2CAP;
using DisconnectionReason =
    ::location::nearby::proto::connections::DisconnectionReason;

Exception WriteInt(OutputStream* writer, std::int32_t value) {
  return Base64Utils::WriteInt(writer, value);
}

}  // namespace

BaseEndpointChannel::BaseEndpointChannel(const std::string& service_id,
                                         const std::string& channel_name,
                                         InputStream* reader,
                                         OutputStream* writer)
    : BaseEndpointChannel(
          service_id, channel_name, reader, writer,
          // TODO(edwinwu): Below values should be retrieved from a base socket,
          // the #MediumSocket in Android counterpart, from which all the
          // derived medium sockets should derived, and implement the supported
          // values and leave the default values in base #MediumSocket.
          /*ConnectionTechnology*/
          location::nearby::proto::connections::
              CONNECTION_TECHNOLOGY_UNKNOWN_TECHNOLOGY,
          /*ConnectionBand*/
          location::nearby::proto::connections::CONNECTION_BAND_UNKNOWN_BAND,
          /*frequency*/ -1,
          /*try_count*/ 0) {}

BaseEndpointChannel::BaseEndpointChannel(
    const std::string& service_id, const std::string& channel_name,
    InputStream* reader, OutputStream* writer,
    location::nearby::proto::connections::ConnectionTechnology technology,
    location::nearby::proto::connections::ConnectionBand band, int frequency,
    int try_count)
    : service_id_(service_id),
      channel_name_(channel_name),
      max_allowed_read_bytes_(GetMaxAllowedReadBytes()),
      default_max_transmit_packet_size_(GetDefaultMaxTransmitPacketSize()),
      reader_(reader),
      writer_(writer),
      technology_(technology),
      band_(band),
      frequency_(frequency),
      try_count_(try_count) {}

ExceptionOr<ByteArray> BaseEndpointChannel::Read() {
  PacketMetaData packet_meta_data;
  return Read(packet_meta_data);
}

ExceptionOr<ByteArray> BaseEndpointChannel::Read(
    PacketMetaData& packet_meta_data) {
  ByteArray result;
  {
    MutexLock lock(&reader_mutex_);

    packet_meta_data.StartSocketIo();
    ExceptionOr<std::int32_t> read_int;
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::
                kRefactorBleL2cap)) {
      ExceptionOr<ByteArray> read_control_block_bytes = DispatchPacket();
      if (!read_control_block_bytes.ok()) {
        LOG(WARNING) << __func__ << ": Failed to dispatch packet: "
                     << read_control_block_bytes.exception();
        return ExceptionOr<ByteArray>(read_control_block_bytes.exception());
      }

      read_int = (GetMedium() == BLE_L2CAP) ? ReadPayloadLength()
                                            : Base64Utils::ReadInt(reader_);
    } else {
      read_int = Base64Utils::ReadInt(reader_);
    }
    if (!read_int.ok()) {
      return ExceptionOr<ByteArray>(read_int.exception());
    }

    if (read_int.result() < 0 || read_int.result() > max_allowed_read_bytes_) {
      LOG(WARNING) << __func__ << ": Read an invalid number of bytes: "
                   << read_int.result();
      return ExceptionOr<ByteArray>(Exception::kIo);
    }

    ExceptionOr<ByteArray> read_bytes = reader_->ReadExactly(read_int.result());
    if (!read_bytes.ok()) {
      return read_bytes;
    }
    packet_meta_data.StopSocketIo();
    packet_meta_data.SetPacketSize(read_int.result() + sizeof(std::int32_t));
    result = std::move(read_bytes.result());
  }

  {
    MutexLock crypto_lock(&crypto_mutex_);
    Exception message_exception{Exception::kInvalidProtocolBuffer};
    if (IsEncryptionEnabledLocked()) {
      // If encryption is enabled, decode the message.
      std::string input(std::move(result));
      packet_meta_data.StartEncryption();
      std::unique_ptr<std::string> decrypted_data =
          crypto_context_->DecodeMessageFromPeer(input);
      if (decrypted_data) {
        result = ByteArray(std::move(*decrypted_data));
      } else {
        // It could be a protocol race, where remote party sends a KEEP_ALIVE
        // before encryption is setup on their side, and we receive it after
        // we switched to encryption mode.
        // In this case, we verify that message is indeed a valid KEEP_ALIVE,
        // and let it through if it is, otherwise message is erased.
        // TODO(apolyudov): verify this happens at most once per session.
        result = {};
        auto parsed = parser::FromBytes(ByteArray(input));
        if (parsed.ok()) {
          if (parser::GetFrameType(parsed.result()) ==
              location::nearby::connections::V1Frame::KEEP_ALIVE) {
            LOG(INFO) << __func__
                      << ": Read unencrypted KEEP_ALIVE on encrypted channel.";
            result = ByteArray(input);
          } else {
            LOG(WARNING) << __func__
                         << ": Read unexpected unencrypted frame of type "
                         << parser::GetFrameType(parsed.result());
          }
        } else {
          message_exception.value = parsed.exception();
          LOG(WARNING) << __func__
                       << ": Unable to parse data as unencrypted message.";
        }
      }
      packet_meta_data.StopEncryption();
      if (result.Empty()) {
        LOG(WARNING) << __func__ << ": Unable to parse read result.";
        return ExceptionOr<ByteArray>(message_exception);
      }
    }
  }

  {
    MutexLock lock(&last_read_mutex_);
    last_read_timestamp_ = SystemClock::ElapsedRealtime();
  }
  return ExceptionOr<ByteArray>(result);
}

Exception BaseEndpointChannel::Write(const ByteArray& data) {
  PacketMetaData packet_meta_data;
  return Write(data.AsStringView(), packet_meta_data);
}

Exception BaseEndpointChannel::Write(absl::string_view data,
                                     PacketMetaData& packet_meta_data) {
  {
    MutexLock pause_lock(&is_paused_mutex_);
    if (is_paused_) {
      BlockUntilUnpaused();
    }
  }

  absl::string_view  data_to_write = data;
  // Make sure encrypted message is value until end of function.
  std::unique_ptr<std::string> encrypted;
  {
    // Holding both mutexes is necessary to prevent the keep alive and payload
    // threads from writing encrypted messages out of order which causes a
    // failure to decrypt on the reader side. However we need to release the
    // crypto lock after encrypting to ensure read decryption is not blocked.
    MutexLock lock(&writer_mutex_);
    {
      MutexLock crypto_lock(&crypto_mutex_);
      if (IsEncryptionEnabledLocked()) {
        // If encryption is enabled, encode the message.
        packet_meta_data.StartEncryption();
        encrypted = crypto_context_->EncodeMessageToPeer(data);
        packet_meta_data.StopEncryption();
        if (!encrypted) {
          LOG(WARNING) << __func__ << ": Failed to encrypt data.";
          return {Exception::kIo};
        }
        data_to_write = *encrypted;
      }
    }

    size_t data_size = data_to_write.size();
    if (data_size < 0 || data_size > max_allowed_read_bytes_) {
      LOG(WARNING) << __func__
                   << ": Write an invalid number of bytes: " << data_size;
      return {Exception::kIo};
    }

    packet_meta_data.StartSocketIo();
    Exception write_exception;
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::
                kRefactorBleL2cap) &&
        (GetMedium() == BLE || GetMedium() == BLE_L2CAP)) {
      write_exception = WritePayloadLength(data_size);
    } else {
      write_exception = WriteInt(writer_, static_cast<std::int32_t>(data_size));
    }
    if (write_exception.Raised()) {
      LOG(WARNING) << __func__
                   << ": Failed to write header: " << write_exception.value;
      return write_exception;
    }
    write_exception = writer_->Write(data_to_write);
    if (write_exception.Raised()) {
      LOG(WARNING) << __func__
                   << ": Failed to write data: " << write_exception.value;
      return write_exception;
    }
    Exception flush_exception = writer_->Flush();
    if (flush_exception.Raised()) {
      LOG(WARNING) << __func__
                   << ": Failed to flush writer: " << flush_exception.value;
      return flush_exception;
    }
    packet_meta_data.StopSocketIo();
    packet_meta_data.SetPacketSize(data_size + sizeof(std::uint32_t));
  }

  {
    MutexLock lock(&last_write_mutex_);
    last_write_timestamp_ = SystemClock::ElapsedRealtime();
  }
  return {Exception::kSuccess};
}

void BaseEndpointChannel::Close() {
  {
    // In case channel is paused, resume it first thing.
    MutexLock lock(&is_paused_mutex_);
    if (is_closed_) {
      VLOG(1) << "EndpointChannel already closed";
      return;
    }
    is_closed_ = true;
    UnblockPausedWriter();
  }
  CloseIo();
  CloseImpl();
}

void BaseEndpointChannel::CloseIo() {
  // Keep this method dedicated to reader and writer handling an nothing else.
  {
    // Do not take reader_mutex_ here: read may be in progress, and it will
    // deadlock. Calling Close() with Read() in progress will terminate the
    // IO and Read() will proceed normally (with Exception::kIo).
    Exception exception = reader_->Close();
    if (!exception.Ok()) {
      LOG(WARNING) << __func__
                   << ": Exception closing reader: " << exception.value;
    }
  }
  {
    // Do not take writer_mutex_ here: write may be in progress, and it will
    // deadlock. Calling Close() with Write() in progress will terminate the
    // IO and Write() will proceed normally (with Exception::kIo).
    Exception exception = writer_->Close();
    if (!exception.Ok()) {
      LOG(WARNING) << __func__
                   << ": Exception closing writer: " << exception.value;
    }
  }
}

uint32_t BaseEndpointChannel::GetNextKeepAliveSeqNo() const {
  MutexLock lock(&keep_alive_mutex_);
  return next_keep_alive_seq_no_++;
}

void BaseEndpointChannel::SetAnalyticsRecorder(
    analytics::AnalyticsRecorder* analytics_recorder,
    const std::string& endpoint_id) {
  analytics_recorder_ = analytics_recorder;
  endpoint_id_ = endpoint_id;
}

void BaseEndpointChannel::Close(
    location::nearby::proto::connections::DisconnectionReason reason) {
  Close(reason, ConnectionsLog::EstablishedConnection::SAFE_DISCONNECTION);
}

void BaseEndpointChannel::Close(
    location::nearby::proto::connections::DisconnectionReason reason,
    SafeDisconnectionResult result) {
  LOG(INFO) << __func__ << ": Closing endpoint channel, reason: " << reason;
  Close();

  if (analytics_recorder_ != nullptr && !endpoint_id_.empty()) {
    analytics_recorder_->OnConnectionClosed(endpoint_id_, GetMedium(), reason,
                                            result);
  }
}

bool BaseEndpointChannel::IsClosed() const {
  MutexLock lock(&is_paused_mutex_);
  return is_closed_;
}

std::string BaseEndpointChannel::GetType() const {
  MutexLock crypto_lock(&crypto_mutex_);
  std::string subtype = IsEncryptionEnabledLocked() ? "ENCRYPTED_" : "";
  std::string medium = location::nearby::proto::connections::Medium_Name(
      location::nearby::proto::connections::Medium::UNKNOWN_MEDIUM);

  if (GetMedium() !=
      location::nearby::proto::connections::Medium::UNKNOWN_MEDIUM) {
    medium = absl::StrCat(
        subtype,
        location::nearby::proto::connections::Medium_Name(GetMedium()));
  }
  return medium;
}

std::string BaseEndpointChannel::GetServiceId() const { return service_id_; }

std::string BaseEndpointChannel::GetName() const { return channel_name_; }

int BaseEndpointChannel::GetMaxTransmitPacketSize() const {
  // Return default value if the medium never define it's chunk size.
  return default_max_transmit_packet_size_;
}

void BaseEndpointChannel::EnableEncryption(
    std::shared_ptr<EncryptionContext> context) {
  MutexLock crypto_lock(&crypto_mutex_);
  crypto_context_ = context;
}

void BaseEndpointChannel::DisableEncryption() {
  MutexLock crypto_lock(&crypto_mutex_);
  crypto_context_.reset();
}

bool BaseEndpointChannel::IsEncrypted() {
  MutexLock crypto_lock(&crypto_mutex_);
  return IsEncryptionEnabledLocked();
}

ExceptionOr<ByteArray> BaseEndpointChannel::TryDecrypt(const ByteArray& data) {
  MutexLock crypto_lock(&crypto_mutex_);
  if (!IsEncryptionEnabledLocked()) {
    return Exception::kFailed;
  }
  std::unique_ptr<std::string> decrypted_data =
      crypto_context_->DecodeMessageFromPeer(data.string_data());
  if (decrypted_data) {
    return ExceptionOr<ByteArray>(ByteArray(std::move(*decrypted_data)));
  }
  return Exception::kExecution;
}

bool BaseEndpointChannel::IsPaused() const {
  MutexLock lock(&is_paused_mutex_);
  return is_paused_;
}

void BaseEndpointChannel::Pause() {
  MutexLock lock(&is_paused_mutex_);
  is_paused_ = true;
}

void BaseEndpointChannel::Resume() {
  MutexLock lock(&is_paused_mutex_);
  is_paused_ = false;
  is_paused_cond_.Notify();
}

absl::Time BaseEndpointChannel::GetLastReadTimestamp() const {
  MutexLock lock(&last_read_mutex_);
  return last_read_timestamp_;
}

absl::Time BaseEndpointChannel::GetLastWriteTimestamp() const {
  MutexLock lock(&last_write_mutex_);
  return last_write_timestamp_;
}

location::nearby::proto::connections::ConnectionTechnology
BaseEndpointChannel::GetTechnology() const {
  return technology_;
}

// Returns the used wifi band of this EndpointChannel.
location::nearby::proto::connections::ConnectionBand
BaseEndpointChannel::GetBand() const {
  return band_;
}

//  Returns the used wifi frequency of this EndpointChannel.
int BaseEndpointChannel::GetFrequency() const { return frequency_; }

// Returns the try count of this EndpointChannel.
int BaseEndpointChannel::GetTryCount() const { return try_count_; }

int BaseEndpointChannel::GetMaxAllowedReadBytes() const {
  int64_t max_allowed_read_bytes = NearbyFlags::GetInstance().GetInt64Flag(
      config_package_nearby::nearby_connections_feature::
          kMediumMaxAllowedReadBytes);
  return max_allowed_read_bytes >= INT_MAX ? INT_MAX : max_allowed_read_bytes;
}

int BaseEndpointChannel::GetDefaultMaxTransmitPacketSize() const {
  int32_t default_max_transmit_packet_size =
      NearbyFlags::GetInstance().GetInt64Flag(
          config_package_nearby::nearby_connections_feature::
              kMediumDefaultMaxTransmitPacketSize);
  return default_max_transmit_packet_size >= INT_MAX
             ? INT_MAX
             : default_max_transmit_packet_size;
}

bool BaseEndpointChannel::IsEncryptionEnabledLocked() const {
  return crypto_context_ != nullptr;
}

void BaseEndpointChannel::BlockUntilUnpaused() {
  // For more on how this works, see
  // https://docs.oracle.com/javase/tutorial/essential/concurrency/guardmeth.html
  while (is_paused_) {
    Exception wait_succeeded = is_paused_cond_.Wait();
    if (!wait_succeeded.Ok()) {
      LOG(WARNING) << __func__
                   << ": Failure waiting to unpause: " << wait_succeeded.value;
      return;
    }
  }
}

void BaseEndpointChannel::UnblockPausedWriter() {
  // For more on how this works, see
  // https://docs.oracle.com/javase/tutorial/essential/concurrency/guardmeth.html
  is_paused_ = false;
  is_paused_cond_.Notify();
}

std::unique_ptr<std::string> BaseEndpointChannel::EncodeMessageForTests(
    absl::string_view data) {
  MutexLock lock(&crypto_mutex_);
  DCHECK(IsEncryptionEnabledLocked());
  return crypto_context_->EncodeMessageToPeer(data);
}

}  // namespace connections
}  // namespace nearby
