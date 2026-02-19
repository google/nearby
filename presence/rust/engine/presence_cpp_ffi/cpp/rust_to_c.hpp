// Build data structures passed from Rust to C.
// The functions will be FFIed to Rust, which are used by Rust codes to build data structures
// passed into C.

// Forward declaration. The enum is already defined in Rust.
/// <div rustbindgen hide></div>
enum class PresenceMedium;

extern "C" {
// Build BLE scan request for the BLE system API.
struct PresenceBleScanRequest* presence_ble_scan_request_new(int priority);
void presence_ble_scan_request_add_action(PresenceBleScanRequest* request,
                                          int action);

// Build discovery result for the client.
struct PresenceDiscoveryResult* presence_discovery_result_new(enum PresenceMedium medium);
void presence_discovery_result_add_action(PresenceDiscoveryResult* result,
                                          int action);
}