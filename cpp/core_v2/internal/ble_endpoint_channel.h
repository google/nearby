#ifndef CORE_V2_INTERNAL_BLE_ENDPOINT_CHANNEL_H_
#define CORE_V2_INTERNAL_BLE_ENDPOINT_CHANNEL_H_

#include "core_v2/internal/base_endpoint_channel.h"
#include "platform_v2/public/ble.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

class BleEndpointChannel final : public BaseEndpointChannel {
 public:
  // Creates both outgoing and incoming Ble channels.
  BleEndpointChannel(const std::string& channel_name, BleSocket socket);

  proto::connections::Medium GetMedium() const override;

 private:
  void CloseImpl() override;

  BleSocket ble_socket_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_BLE_ENDPOINT_CHANNEL_H_
