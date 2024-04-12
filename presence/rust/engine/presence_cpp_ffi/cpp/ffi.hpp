// FFI the platform C APIs to Rust.
#include "../presence_enums.h"

extern "C" {
struct PresenceBleScanRequest* presence_ble_scan_request_new(int priority);
void presence_ble_scan_request_add_action(PresenceBleScanRequest* request,
                                          int action);

// Build discovery result for the client.
struct PresenceDiscoveryResult* presence_discovery_result_new(enum PresenceMedium medium);
void presence_discovery_result_add_action(PresenceDiscoveryResult* result,
                                          int action);
}