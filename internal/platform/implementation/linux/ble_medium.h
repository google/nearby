#ifndef PLATFORM_IMPL_LINUX_API_BLE_MEDIUM_H_
#define PLATFORM_IMPL_LINUX_API_BLE_MEDIUM_H_

#include "internal/platform/implementation/ble.h"

namespace nearby {
namespace linux {
// Container of operations that can be performed over the BLE medium.
class BleMedium : public api::BleMedium {
public:
  BleMedium() {}
  ~BleMedium() = default;

  bool StartAdvertising(
      const std::string &service_id, const ByteArray &advertisement_bytes,
      const std::string &fast_advertisement_service_uuid) override {
    return false;
  }
  bool StopAdvertising(const std::string &service_id) override { return false; }

  // Returns true once the BLE scan has been initiated.
  bool StartScanning(const std::string &service_id,
                     const std::string &fast_advertisement_service_uuid,
                     DiscoveredPeripheralCallback callback) override {
    return false;
  }

  // Returns true once BLE scanning for service_id is well and truly stopped;
  // after this returns, there must be no more invocations of the
  // DiscoveredPeripheralCallback passed in to StartScanning() for service_id.
  bool StopScanning(const std::string &service_id) override { return false; }

  // Callback that is invoked when a new connection is accepted.
  using AcceptedConnectionCallback = absl::AnyInvocable<void(
      api::BleSocket &socket, const std::string &service_id)>;

  // Returns true once BLE socket connection requests to service_id can be
  // accepted.
  bool StartAcceptingConnections(const std::string &service_id,
                                 AcceptedConnectionCallback callback) override {
    return false;
  }
  bool StopAcceptingConnections(const std::string &service_id) override {
    return false;
  }

  // Connects to a BLE peripheral.
  // On success, returns a new BleSocket.
  // On error, returns nullptr.
  std::unique_ptr<api::BleSocket>
  Connect(api::BlePeripheral &peripheral, const std::string &service_id,
          CancellationFlag *cancellation_flag) override {
    return nullptr;
  }
};
} // namespace linux
} // namespace nearby

#endif
