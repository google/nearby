#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>

enum class MeasurementConfidence {
 Low,
 Medium,
 High,
 Unknown,
};


enum class PresenceDataSource {
 Ble,
 Uwb,
 Nan,
 Unknown,
};


enum class ProximityState {
 Unknown,
 Tap,
 Reach,
 ShortRange,
 LongRange,
 Far,
};


struct PresenceDetector;


template<typename T>
struct COption {
 const T *value;
 bool present;
};


struct BleScanResult {
 uint64_t device_id;
 COption<int16_t> tx_power;
 int16_t rssi;
 uint64_t elapsed_real_time;
};


struct ProximityEstimate {
 uint64_t device_id;
 double distance;
 MeasurementConfidence distance_confidence;
 u128 elapsed_real_time_millis;
 PresenceDataSource source;
};

struct ProximityStateResult {
 uint64_t device_id;
 ProximityState current_proximity_state;
 ProximityEstimate current_proximity_estimate;
};


struct ProximityStateOptions {
 double tap_threshold_meters;
 double reach_threshold_meters;
 double short_range_threshold_meters;
 double long_range_threshold_meters;
 double hysteresis_percentage;
 double hysteresis_tap_extra_debounce_meters;
 uint8_t consecutive_scans_required;
};

extern "C" {

PresenceDetector *presence_detector_create();

void update_ble_scan_result(PresenceDetector *presence_detector,
                            BleScanResult ble_scan_result,
                            ProximityStateResult *proximity_state_result);

void fpp_configure_options(PresenceDetector *presence_detector,
                           ProximityStateOptions proximity_state_options);

void fpp_get_proximity_estimate(PresenceDetector *presence_detector,
                                uint64_t device_id,
                                ProximityEstimate *proximity_estimate);

int presence_detector_free(PresenceDetector *presence_detector);

} // extern "C"
