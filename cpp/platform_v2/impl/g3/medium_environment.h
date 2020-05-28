#ifndef PLATFORM_V2_IMPL_G3_MEDIUM_ENVIRONMENT_H_
#define PLATFORM_V2_IMPL_G3_MEDIUM_ENVIRONMENT_H_

#include <new>
#include <string>
#include <type_traits>

#include "platform_v2/api/bluetooth_classic.h"
#include "platform_v2/impl/g3/bluetooth_adapter.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"

namespace location {
namespace nearby {
namespace g3 {

// MediumEnvironment is a simulated environment which allowes multiple instances
// of simulated HW devices to "work" together as if they are physical.
// For each medium type it provides necessary methods to implement
// advertising, discovery and establishment of a data link.
class MediumEnvironment {
 public:
  ~MediumEnvironment() = default;
  // Singleton constructor/accessor.
  static MediumEnvironment& Instance();

  // Clear state. No notifications are sent.
  void Reset() ABSL_LOCKS_EXCLUDED(mutex_);

  // Add an adapter to internal container.
  // Notify BluetoothClassicMediums if any that adapter state has changed.
  void OnBluetoothAdapterChangedState(BluetoothAdapter& adapter)
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  MediumEnvironment() = default;
  absl::Mutex mutex_;
  absl::flat_hash_set<BluetoothAdapter*> bluetooth_adapters_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_IMPL_G3_MEDIUM_ENVIRONMENT_H_
