#ifndef CORE_INTERNAL_BLE_ENDPOINT_CHANNEL_H_
#define CORE_INTERNAL_BLE_ENDPOINT_CHANNEL_H_

#include "core/internal/base_endpoint_channel.h"
#include "platform/public/ble.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

class BleEndpointChannel final : public BaseEndpointChannel {
 public:
  // Creates both outgoing and incoming Ble channels.
  BleEndpointChannel(const std::string& channel_name, BleSocket socket);

  proto::connections::Medium GetMedium() const override;

  int GetMaxTransmitPacketSize() const override;

 private:
  static constexpr int kDefaultBleMaxTransmitPacketSize = 512;  // 512 bytes

  void CloseImpl() override;

  BleSocket ble_socket_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_BLE_ENDPOINT_CHANNEL_H_
