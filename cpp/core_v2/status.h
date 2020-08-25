#ifndef CORE_V2_STATUS_H_
#define CORE_V2_STATUS_H_

namespace location {
namespace nearby {
namespace connections {

// Protocol operation result: kSuccess, if operation was successful;
// descriptive error code otherwise.
struct Status {
  // Status is a struct, so it is possible to pass some context about failure,
  // by adding extra fields to it when necessary, and not change any of the
  // method signatures.
  enum Value {
    kSuccess,
    kError,
    kOutOfOrderApiCall,
    kAlreadyHaveActiveStrategy,
    kAlreadyAdvertising,
    kAlreadyDiscovering,
    kEndpointIoError,
    kEndpointUnknown,
    kConnectionRejected,
    kAlreadyConnectedToEndpoint,
    kNotConnectedToEndpoint,
    kBluetoothError,
    kBleError,
    kWifiLanError,
    kPayloadUnknown,
  };
  Value value {kError};
  bool Ok() const { return value == kSuccess; }
};

inline bool operator==(const Status& a, const Status& b) {
  return a.value == b.value;
}

inline bool operator!=(const Status& a, const Status& b) {
  return !(a == b);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_STATUS_H_
