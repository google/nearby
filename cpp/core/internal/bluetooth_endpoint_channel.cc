#include "core/internal/bluetooth_endpoint_channel.h"

#include <string>

namespace location {
namespace nearby {
namespace connections {

Ptr<BluetoothEndpointChannel> BluetoothEndpointChannel::createOutgoing(
    Ptr<MediumManager<Platform> > medium_manager, const string& channel_name,
    Ptr<BluetoothSocket> bluetooth_socket) {
  return MakePtr(new BluetoothEndpointChannel(channel_name, bluetooth_socket));
}

Ptr<BluetoothEndpointChannel> BluetoothEndpointChannel::createIncoming(
    Ptr<MediumManager<Platform> > medium_manager, const string& channel_name,
    Ptr<BluetoothSocket> bluetooth_socket) {
  return MakePtr(new BluetoothEndpointChannel(channel_name, bluetooth_socket));
}

BluetoothEndpointChannel::BluetoothEndpointChannel(
    const string& channel_name, Ptr<BluetoothSocket> bluetooth_socket)
    : BaseEndpointChannel(channel_name, bluetooth_socket->getInputStream(),
                          bluetooth_socket->getOutputStream()),
      bluetooth_socket_(bluetooth_socket) {}

BluetoothEndpointChannel::~BluetoothEndpointChannel() {}

proto::connections::Medium BluetoothEndpointChannel::getMedium() {
  return proto::connections::Medium::BLUETOOTH;
}

void BluetoothEndpointChannel::closeImpl() {
  Exception::Value exception = bluetooth_socket_->close();
  if (exception != Exception::NONE) {
    if (exception == Exception::IO) {
      // TODO(tracyzhou): Add logging.
    }
  }
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
