#ifndef CORE_STATUS_H_
#define CORE_STATUS_H_

namespace location {
namespace nearby {
namespace connections {

struct Status {
  enum Value {
    SUCCESS,
    ERROR,
    OUT_OF_ORDER_API_CALL,
    ALREADY_HAVE_ACTIVE_STRATEGY,
    ALREADY_ADVERTISING,
    ALREADY_DISCOVERING,
    ENDPOINT_IO_ERROR,
    ENDPOINT_UNKNOWN,
    CONNECTION_REJECTED,
    ALREADY_CONNECTED_TO_ENDPOINT,
    NOT_CONNECTED_TO_ENDPOINT,
    BLUETOOTH_ERROR,
    PAYLOAD_UNKNOWN,
  };
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_STATUS_H_
