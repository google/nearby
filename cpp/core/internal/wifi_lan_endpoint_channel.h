#ifndef CORE_INTERNAL_WIFI_LAN_ENDPOINT_CHANNEL_H_
#define CORE_INTERNAL_WIFI_LAN_ENDPOINT_CHANNEL_H_

#include "core/internal/base_endpoint_channel.h"
#include "platform/public/wifi_lan.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

class WifiLanEndpointChannel final : public BaseEndpointChannel {
 public:
  // Creates both outgoing and incoming WifiLan channels.
  WifiLanEndpointChannel(const std::string& channel_name,
                         WifiLanSocket socket);

  proto::connections::Medium GetMedium() const override;

 private:
  void CloseImpl() override;

  WifiLanSocket wifi_lan_socket_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_WIFI_LAN_ENDPOINT_CHANNEL_H_
