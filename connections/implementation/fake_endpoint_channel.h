// Copyright 2022 Google LLC
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

#ifndef NEARBY_CONNECTIONS_IMPLEMENTATION_FAKE_ENDPOINT_CHANNEL_H_
#define NEARBY_CONNECTIONS_IMPLEMENTATION_FAKE_ENDPOINT_CHANNEL_H_

#include <string>

#include "connections/implementation/endpoint_channel.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"

namespace nearby {
namespace connections {

// An endpoint channel implementation used for testing. The read and write
// output can be set.
class FakeEndpointChannel : public EndpointChannel {
 public:
  using Medium = ::location::nearby::proto::connections::Medium;

  FakeEndpointChannel(Medium medium, const std::string& service_id)
      : medium_(medium), service_id_(service_id) {}

  ~FakeEndpointChannel() override = default;

  // EndpointChannel:
  ExceptionOr<ByteArray> Read() override {
    read_timestamp_ = SystemClock::ElapsedRealtime();
    return read_output_;
  }
  ExceptionOr<ByteArray> Read(PacketMetaData& packet_meta_data) override {
    read_timestamp_ = SystemClock::ElapsedRealtime();
    return read_output_;
  }
  Exception Write(const ByteArray& data) override {
    write_timestamp_ = SystemClock::ElapsedRealtime();
    return write_output_;
  }
  Exception Write(const ByteArray& data,
                  PacketMetaData& packet_meta_data) override {
    write_timestamp_ = SystemClock::ElapsedRealtime();
    return write_output_;
  }
  void Close() override { is_closed_ = true; }
  void Close(location::nearby::proto::connections::DisconnectionReason reason)
      override {
    is_closed_ = true;
    disconnection_reason_ = reason;
  }
  void Close(
      location::nearby::proto::connections::DisconnectionReason reason,
      location::nearby::analytics::proto::ConnectionsLog::
          EstablishedConnection::SafeDisconnectionResult result) override {
    Close(reason);
  }
  location::nearby::proto::connections::ConnectionTechnology GetTechnology()
      const override {
    return location::nearby::proto::connections::ConnectionTechnology::
        CONNECTION_TECHNOLOGY_BLE_GATT;
  }
  location::nearby::proto::connections::ConnectionBand GetBand()
      const override {
    return location::nearby::proto::connections::ConnectionBand::
        CONNECTION_BAND_CELLULAR_BAND_2G;
  }
  int GetFrequency() const override { return 0; }
  int GetTryCount() const override { return 0; }
  std::string GetType() const override { return "fake-channel-type"; }
  std::string GetServiceId() const override { return service_id_; }
  std::string GetName() const override { return "fake-channel-" + service_id_; }
  Medium GetMedium() const override { return medium_; }
  int GetMaxTransmitPacketSize() const override { return 512; }
  void EnableEncryption(std::shared_ptr<EncryptionContext> context) override {}
  void DisableEncryption() override {}
  bool IsEncrypted() override { return false; }
  ExceptionOr<ByteArray> TryDecrypt(const ByteArray& data) override {
    return Exception::kFailed;
  }
  bool IsPaused() const override { return is_paused_; }
  void Pause() override { is_paused_ = true; }
  void Resume() override { is_paused_ = false; }
  absl::Time GetLastReadTimestamp() const override { return read_timestamp_; }
  absl::Time GetLastWriteTimestamp() const override { return write_timestamp_; }
  void SetAnalyticsRecorder(analytics::AnalyticsRecorder* analytics_recorder,
                            const std::string& endpoint_id) override {}

  void set_read_output(ExceptionOr<ByteArray> output) { read_output_ = output; }
  void set_write_output(Exception output) { write_output_ = output; }
  bool is_closed() const { return is_closed_; }
  location::nearby::proto::connections::DisconnectionReason
  disconnection_reason() const {
    return disconnection_reason_;
  }

 private:
  ExceptionOr<ByteArray> read_output_;
  Exception write_output_{Exception::kSuccess};
  Medium medium_;
  std::string service_id_;
  absl::Time read_timestamp_ = absl::InfinitePast();
  absl::Time write_timestamp_ = absl::InfinitePast();
  bool is_closed_ = false;
  bool is_paused_ = false;
  location::nearby::proto::connections::DisconnectionReason
      disconnection_reason_;
};

}  // namespace connections
}  // namespace nearby

#endif  // NEARBY_CONNECTIONS_IMPLEMENTATION_FAKE_ENDPOINT_CHANNEL_H_
