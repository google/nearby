#include "core/internal/ble_endpoint_channel.h"

#include <string>

#include "platform/public/ble.h"
#include "platform/public/logging.h"

namespace location {
namespace nearby {
namespace connections {

namespace {

OutputStream* GetOutputStreamOrNull(BleSocket& socket) {
  if (socket.GetRemotePeripheral().IsValid()) return &socket.GetOutputStream();
  return nullptr;
}

InputStream* GetInputStreamOrNull(BleSocket& socket) {
  if (socket.GetRemotePeripheral().IsValid()) return &socket.GetInputStream();
  return nullptr;
}

}  // namespace

BleEndpointChannel::BleEndpointChannel(const std::string& channel_name,
                                       BleSocket socket)
    : BaseEndpointChannel(channel_name, GetInputStreamOrNull(socket),
                          GetOutputStreamOrNull(socket)),
      ble_socket_(std::move(socket)) {}

proto::connections::Medium BleEndpointChannel::GetMedium() const {
  return proto::connections::Medium::BLE;
}

int BleEndpointChannel::GetMaxTransmitPacketSize() const {
  return kDefaultBleMaxTransmitPacketSize;
}

void BleEndpointChannel::CloseImpl() {
  auto status = ble_socket_.Close();
  if (!status.Ok()) {
    NEARBY_LOG(INFO, "Failed to close Ble socket: exception=%d", status.value);
  }
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
