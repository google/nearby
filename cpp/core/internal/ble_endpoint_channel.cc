#include "core/internal/ble_endpoint_channel.h"

#include <string>

namespace location {
namespace nearby {
namespace connections {

Ptr<BLEEndpointChannel> BLEEndpointChannel::createOutgoing(
    Ptr<MediumManager<Platform> > medium_manager, const string& channel_name,
    Ptr<BLESocket> ble_socket) {
  return MakePtr(new BLEEndpointChannel(channel_name, ble_socket));
}

Ptr<BLEEndpointChannel> BLEEndpointChannel::createIncoming(
    Ptr<MediumManager<Platform> > medium_manager, const string& channel_name,
    Ptr<BLESocket> ble_socket) {
  return MakePtr(new BLEEndpointChannel(channel_name, ble_socket));
}

BLEEndpointChannel::BLEEndpointChannel(const string& channel_name,
                                       Ptr<BLESocket> ble_socket)
    : BaseEndpointChannel(channel_name, ble_socket->getInputStream(),
                          ble_socket->getOutputStream()),
      ble_socket_(ble_socket) {}

BLEEndpointChannel::~BLEEndpointChannel() {}

proto::connections::Medium BLEEndpointChannel::getMedium() {
  return proto::connections::Medium::BLE;
}

void BLEEndpointChannel::closeImpl() {
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
