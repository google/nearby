// Constants defined by enums shared between C and Rust.
#ifndef presence_enums_h
#define presence_enums_h

enum PresenceMedium {
  RRESENCE_MEDIUM_UNKNOWN = 0,
  PRESENCE_MEDIUM_BLE,
  PRESENCE_MEDIUM_WIFI_RTT,
  PRESENCE_MEDIUM_UWB,
  PRESENCE_MEDIUM_MDNS,
};

#endif // presence_enums_h