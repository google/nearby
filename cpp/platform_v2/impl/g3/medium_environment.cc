#include "platform_v2/impl/g3/medium_environment.h"

namespace location {
namespace nearby {
namespace g3 {

MediumEnvironment& MediumEnvironment::Instance() {
  static std::aligned_storage_t<sizeof(MediumEnvironment),
                                alignof(MediumEnvironment)>
      storage;
  static MediumEnvironment* env = new (&storage) MediumEnvironment();
  return *env;
}

void MediumEnvironment::Reset() {
  absl::MutexLock lock(&mutex_);
  bluetooth_adapters_.clear();
}

void MediumEnvironment::OnBluetoothAdapterChangedState(
    BluetoothAdapter& adapter) {
  absl::MutexLock lock(&mutex_);
  // We don't care if there is an adapter already since all we store is a
  // pointer.
  bluetooth_adapters_.emplace(&adapter);
  // TODO(apolyudov): Add event propagation code when Medium registration is
  // implemented.
}

}  // namespace g3
}  // namespace nearby
}  // namespace location
