#include "core/internal/bluetooth_endpoint_channel.h"

#include <string>

#include "platform/public/bluetooth_classic.h"
#include "platform/public/logging.h"

namespace location {
namespace nearby {
namespace connections {

namespace {

OutputStream* GetOutputStreamOrNull(BluetoothSocket& socket) {
  if (socket.GetRemoteDevice().IsValid()) return &socket.GetOutputStream();
  return nullptr;
}

InputStream* GetInputStreamOrNull(BluetoothSocket& socket) {
  if (socket.GetRemoteDevice().IsValid()) return &socket.GetInputStream();
  return nullptr;
}

}  // namespace

BluetoothEndpointChannel::BluetoothEndpointChannel(
    const std::string& channel_name, BluetoothSocket socket)
    : BaseEndpointChannel(channel_name, GetInputStreamOrNull(socket),
                          GetOutputStreamOrNull(socket)),
      bluetooth_socket_(std::move(socket)) {}

proto::connections::Medium BluetoothEndpointChannel::GetMedium() const {
  return proto::connections::Medium::BLUETOOTH;
}

int BluetoothEndpointChannel::GetMaxTransmitPacketSize() const {
  return kDefaultBTMaxTransmitPacketSize;
}

void BluetoothEndpointChannel::CloseImpl() {
  auto status = bluetooth_socket_.Close();
  if (!status.Ok()) {
    NEARBY_LOG(INFO, "Failed to close BT socket: exception=%d", status.value);
  }
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
