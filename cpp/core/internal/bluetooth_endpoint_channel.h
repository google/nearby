#ifndef CORE_INTERNAL_BLUETOOTH_ENDPOINT_CHANNEL_H_
#define CORE_INTERNAL_BLUETOOTH_ENDPOINT_CHANNEL_H_

#include "core/internal/base_endpoint_channel.h"
#include "core/internal/medium_manager.h"
#include "platform/api/bluetooth_classic.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
class BluetoothEndpointChannel : public BaseEndpointChannel<Platform> {
 public:
  static Ptr<BluetoothEndpointChannel<Platform> > createOutgoing(
      Ptr<MediumManager<Platform> > medium_manager, const string& channel_name,
      Ptr<BluetoothSocket> bluetooth_socket);
  static Ptr<BluetoothEndpointChannel<Platform> > createIncoming(
      Ptr<MediumManager<Platform> > medium_manager, const string& channel_name,
      Ptr<BluetoothSocket> bluetooth_socket);

  ~BluetoothEndpointChannel() override;

  proto::connections::Medium getMedium() override;

 protected:
  void closeImpl() override;

 private:
  BluetoothEndpointChannel(const string& channel_name,
                           Ptr<BluetoothSocket> bluetooth_socket);

  ScopedPtr<Ptr<BluetoothSocket> > bluetooth_socket_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/bluetooth_endpoint_channel.cc"

#endif  // CORE_INTERNAL_BLUETOOTH_ENDPOINT_CHANNEL_H_
