#include "core/internal/bluetooth_endpoint_channel.h"

#include <string>

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
Ptr<BluetoothEndpointChannel<Platform> >
BluetoothEndpointChannel<Platform>::createOutgoing(
    Ptr<MediumManager<Platform> > medium_manager, const string& channel_name,
    Ptr<BluetoothSocket> bluetooth_socket) {
  return MakePtr(
      new BluetoothEndpointChannel<Platform>(channel_name, bluetooth_socket));
}

template <typename Platform>
Ptr<BluetoothEndpointChannel<Platform> >
BluetoothEndpointChannel<Platform>::createIncoming(
    Ptr<MediumManager<Platform> > medium_manager, const string& channel_name,
    Ptr<BluetoothSocket> bluetooth_socket) {
  return MakePtr(
      new BluetoothEndpointChannel<Platform>(channel_name, bluetooth_socket));
}

template <typename Platform>
BluetoothEndpointChannel<Platform>::BluetoothEndpointChannel(
    const string& channel_name, Ptr<BluetoothSocket> bluetooth_socket)
    : BaseEndpointChannel<Platform>(channel_name,
                                    bluetooth_socket->getInputStream(),
                                    bluetooth_socket->getOutputStream()),
      bluetooth_socket_(bluetooth_socket) {}

template <typename Platform>
BluetoothEndpointChannel<Platform>::~BluetoothEndpointChannel() {}

template <typename Platform>
proto::connections::Medium BluetoothEndpointChannel<Platform>::getMedium() {
  return proto::connections::Medium::BLUETOOTH;
}

template <typename Platform>
void BluetoothEndpointChannel<Platform>::closeImpl() {
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
