#ifndef CORE_V2_INTERNAL_PCP_H_
#define CORE_V2_INTERNAL_PCP_H_

namespace location {
namespace nearby {
namespace connections {

// The PreConnectionProtocol (PCP) defines the combinations of interactions
// between the techniques (ultrasound audio, Bluetooth device names, BLE
// advertisements) used for offline Advertisement + Discovery, and identifies
// the steps to go through on each device.
//
// See go/nearby-offline-data-interchange-formats for more.
enum class Pcp {
  kUnknown = 0,
  kP2pStar = 1,
  kP2pCluster = 2,
  kP2pPointToPoint = 3,
  // PCP is only allocated 5 bits in our data interchange formats, so there can
  // never be more than 31 PCP values.
};
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_PCP_H_
