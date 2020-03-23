#include "core/internal/ble_endpoint_channel.h"

#include <string>

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
Ptr<BLEEndpointChannel<Platform> >
BLEEndpointChannel<Platform>::createOutgoing(
    Ptr<MediumManager<Platform> > medium_manager, const string& channel_name,
    Ptr<BLESocket> ble_socket) {
  return MakePtr(
      new BLEEndpointChannel<Platform>(channel_name, ble_socket));
}

template <typename Platform>
Ptr<BLEEndpointChannel<Platform> >
BLEEndpointChannel<Platform>::createIncoming(
    Ptr<MediumManager<Platform> > medium_manager, const string& channel_name,
    Ptr<BLESocket> ble_socket) {
  return MakePtr(
      new BLEEndpointChannel<Platform>(channel_name, ble_socket));
}

template <typename Platform>
BLEEndpointChannel<Platform>::BLEEndpointChannel(
    const string& channel_name, Ptr<BLESocket> ble_socket)
    : BaseEndpointChannel<Platform>(channel_name,
                                    ble_socket->getInputStream(),
                                    ble_socket->getOutputStream()),
      ble_socket_(ble_socket) {}

template <typename Platform>
BLEEndpointChannel<Platform>::~BLEEndpointChannel() {}

template <typename Platform>
proto::connections::Medium BLEEndpointChannel<Platform>::getMedium() {
  return proto::connections::Medium::BLE;
}

template <typename Platform>
void BLEEndpointChannel<Platform>::closeImpl() {
  Exception::Value exception = ble_socket_->close();
  if (exception != Exception::NONE) {
    if (exception == Exception::IO) {
      // TODO(ahlee): Add logging.
    }
  }
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
