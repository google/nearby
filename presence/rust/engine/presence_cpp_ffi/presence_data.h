// Defines data structures passed from Rust to C.
// All these data structures are built from C codes and opaque to Rust.
#ifndef presence_data_h
#define presence_data_h

#include <cstdint>
#include <vector>

enum class PresenceMedium;

typedef struct {
  std::vector<int> actions;
} PresenceDevice;

struct PresenceDiscoveryResult {
  enum PresenceMedium medium;
  PresenceDevice device;
};

struct PresenceBleScanRequest {
  int priority;
  std::vector<uint32_t> actions;
};

#endif // presence_data_h