#ifndef CORE_INTERNAL_BLUETOOTH_ENDPOINT_CHANNEL_H_
#define CORE_INTERNAL_BLUETOOTH_ENDPOINT_CHANNEL_H_

#include <string>

#include "core/internal/base_endpoint_channel.h"
#include "platform/public/bluetooth_classic.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

class BluetoothEndpointChannel final : public BaseEndpointChannel {
 public:
  // Creates both outgoing and incoming BT channels.
  BluetoothEndpointChannel(const std::string& channel_name,
                           BluetoothSocket bluetooth_socket);

  proto::connections::Medium GetMedium() const override;

  int GetMaxTransmitPacketSize() const override;

 private:
  static constexpr int kDefaultBTMaxTransmitPacketSize = 1980;  // 990 * 2 Bytes

  void CloseImpl() override;

  BluetoothSocket bluetooth_socket_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_BLUETOOTH_ENDPOINT_CHANNEL_H_
