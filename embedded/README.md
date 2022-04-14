# Nearby embedded SDK library

This repository contains Nearby SDK library for embedded systems. Nearby SDK
implements the Fast Pair protocol and its future versions per
https://developers.google.com/nearby/fast-pair/spec


## Threading model

Nearby SDK assumes a single-threading model. All calls to the SDK must be on the same thread, or protected with a mutex. That includes:
- client API, for example `nearby_fp_client_SetAdvertisement()`
- ble and other event callbacks
- timers

## Porting guidelines

1. Follow the steps at [Fast Pair Help](https://developers.google.com/nearby/fast-pair/help) to
   register a Model Id for your device.
1. Review `nearby_config.h` and disable features, if any, that you don't
   wish to support.
1. Implement the HAL defined in `nearby_platform_*.h` headers.
1. Set platform specific compile flags in `config.mk` - see 
   `target/gLinux/config.mk` for inspiration, and add your platform in
   `build.sh`, or use your own build system.
1. Compile with `./build.sh <your platform>`
1. Start advertising in your application. For example
    ```
    nearby_fp_client_Init(NULL);
    nearby_fp_client_SetAdvertisement(NEARBY_FP_ADVERTISEMENT_DISCOVERABLE);
    ```
1. Use [Fast Pair Validator](https://play.google.com/store/apps/details?id=com.google.location.nearby.apps.fastpair.validator) to verify that your device is behaving correctly.