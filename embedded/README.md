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

## Optional modules
The optional modules provide partial implementation of HAL interfaces using common libraries, mbedtls in particular.

1. *mbedtls* located in `common/source/mbedtls/mbedtls.c` implements `nearby_platform_Sha256Start()`, `nearby_platform_Sha256Update()`, `nearby_platform_Sha256Finish()`, `nearby_platform_Aes128Encrypt()`, `nearby_platform_Aes128Decrypt()`.

Nearby SDK can be configured to use the MBEDTLS package, commonly available on ARM
implementations, with the config.mk flag `NEARBY_PLATFORM_USE_MBEDTLS`.

2. *gen_secret* located in `common/source/mbedtls/gen_secret.c` implements `nearby_platform_GenSec256r1Secret()`.

*gen_secret* generates a shared secret based on a given private key on platforms that don't support hardware SE. *gen_secret* module is enabled `NEARBY_PLATFORM_USE_MBEDTLS` is set and `NEARBY_PLATFORM_HAS_SE` is *not* set. When `NEARBY_PLATFORM_HAS_SE` is set, the platform needs to provide their own `nearby_platform_GenSec256r1Secret()` routine.
