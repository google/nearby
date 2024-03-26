# Nearby Connection

Nearby Connections is a high level protocol on top of Bluetooth/Wi-Fi that acts
as a medium-agnostic socket. Devices are able to advertise, scan, and connect
with one another over any shared medium (eg. BT <-> BT).
Once connected, the two devices share a list of all supported mediums and
attempt to upgrade to the one with the highest bandwidth (eg. BT -> Wi-Fi).
The connection is encrypted, reliable, and fully duplex. BYTE, FILE, and STREAM
payloads are all supported and will be chunked & transferred internally and
recombined on the receiving device.
See [Nearby Connections Overview](https://developers.google.com/nearby/connections/overview)
for more information.

# Checkout the source tree

```shell
git clone https://github.com/google/nearby
cd nearby
git submodule update --init --recursive
```

# Building Nearby, Unit Testing and Sample Apps

We support multiple platforms including Linux, iOS & Windows.

## Building for Linux

> **NOTE:** Linux has no mediums implemented.

Currently we support building from source using [bazel](https://bazel.build). Other BUILD system such as cmake may be added later.

### Prerequisites:

1. Bazel
2. clang with support for c++ 17+
3. Openssl libcrypto.so (-lssl;-lcrypto).


To build the Nearby Connection Core library:

```shell
CC=clang CXX=clang++ bazel build -s --check_visibility=false //connections:core  --spawn_strategy=standalone --verbose_failures
```


## Building for macOS and iOS

> **NOTE:** The only medium supported is Wi-Fi LAN.

Currently we support building with [Swift Package Manager](https://www.swift.org/package-manager).

### Prerequisites:

1. Xcode. Available from Apple Store.
2. Google Protobuf Compiler (protoc). If you have [homebrew](https://brew.sh/) installed, you can do `brew install protobuf`.

To build the Nearby Connection library:

```shell
swift build
```

# Supported Mediums
### Legend
:heavy_check_mark: Supported
:black_medium_square: Support is possible but not implemented
n/a: Support is not possible or does not make sense.

|Android           |                      |                      |                      |
|------------------|----------------------|----------------------|----------------------|
|Mediums           |Advertising           |Scanning              |Data                  |
|Bluetooth Classic |:heavy_check_mark:    |:heavy_check_mark:    |:heavy_check_mark:    |
|BLE (Fast)        |:heavy_check_mark:    |:heavy_check_mark:    |n/a                   |
|BLE (GATT)        |:heavy_check_mark:    |:heavy_check_mark:    |:heavy_check_mark:    |
|BLE (Extended)    |:heavy_check_mark:    |:heavy_check_mark:    |n/a                   |
|BLE (L2CAP)       |n/a                   |n/a                   |:heavy_check_mark:    |
|WiFi LAN          |:heavy_check_mark:    |:heavy_check_mark:    |:heavy_check_mark:    |
|WiFi Hotspot      |n/a                   |n/a                   |:heavy_check_mark:    |
|WiFi Direct       |n/a                   |n/a                   |:heavy_check_mark:    |
|WiFi Aware        |:heavy_check_mark:    |:heavy_check_mark:    |:heavy_check_mark:    |
|WebRTC            |n/a                   |n/a                   |:heavy_check_mark:    |
|NFC               |:heavy_check_mark:    |:heavy_check_mark:    |:heavy_check_mark:    |
|USB               |:heavy_check_mark:    |:heavy_check_mark:    |:heavy_check_mark:    |
|AWDL              |n/a                   |n/a                   |n/a                   |

|ChromeOS          |                      |                      |                      |
|------------------|----------------------|----------------------|----------------------|
|Mediums           |Advertising           |Scanning              |Data                  |
|Bluetooth Classic |:heavy_check_mark:    |:heavy_check_mark:    |:heavy_check_mark:    |
|BLE (Fast)        |:heavy_check_mark:    |:heavy_check_mark:    |n/a                   |
|BLE (GATT)        |:black_medium_square: |:black_medium_square: |:black_medium_square: |
|BLE (Extended)    |:black_medium_square: |:black_medium_square: |n/a                   |
|BLE (L2CAP)       |n/a                   |n/a                   |n/a                   |
|WiFi LAN          |:black_medium_square: |:black_medium_square: |:heavy_check_mark:    |
|WiFi Hotspot      |n/a                   |n/a                   |:black_medium_square: |
|WiFi Direct       |n/a                   |n/a                   |:black_medium_square: |
|WiFi Aware        |n/a                   |n/a                   |n/a                   |
|WebRTC            |n/a                   |n/a                   |:heavy_check_mark:    |
|NFC               |n/a                   |n/a                   |n/a                   |
|USB               |:black_medium_square: |:black_medium_square: |:black_medium_square: |
|AWDL              |n/a                   |n/a                   |n/a                   |

|Windows           |                      |                      |                      |
|------------------|----------------------|----------------------|----------------------|
|Mediums           |Advertising           |Scanning              |Data                  |
|Bluetooth Classic |n/a                   |:heavy_check_mark:    |:heavy_check_mark:    |
|BLE (Fast)        |:heavy_check_mark:    |:heavy_check_mark:    |n/a                   |
|BLE (GATT)        |:heavy_check_mark:    |:heavy_check_mark:    |:black_medium_square: |
|BLE (Extended)    |:heavy_check_mark:    |:heavy_check_mark:    |n/a                   |
|BLE (L2CAP)       |n/a                   |n/a                   |n/a                   |
|WiFi LAN          |:heavy_check_mark:    |:heavy_check_mark:    |:heavy_check_mark:    |
|WiFi Hotspot      |n/a                   |n/a                   |:heavy_check_mark:    |
|WiFi Direct       |n/a                   |n/a                   |:black_medium_square: |
|WiFi Aware        |n/a                   |n/a                   |n/a                   |
|WebRTC            |n/a                   |n/a                   |:black_medium_square: |
|NFC               |n/a                   |n/a                   |n/a                   |
|USB               |:black_medium_square: |:black_medium_square: |:black_medium_square: |
|AWDL              |n/a                   |n/a                   |n/a                   |

|iOS/macOS         |                      |                      |                      |
|------------------|----------------------|----------------------|----------------------|
|Mediums           |Advertising           |Scanning              |Data                  |
|Bluetooth Classic |n/a                   |:black_medium_square: |:black_medium_square: |
|BLE (Fast)        |:heavy_check_mark:    |:heavy_check_mark:    |n/a                   |
|BLE (GATT)        |:heavy_check_mark:    |:heavy_check_mark:    |:heavy_check_mark:    |
|BLE (Extended)    |n/a                   |:black_medium_square: |n/a                   |
|BLE (L2CAP)       |n/a                   |n/a                   |:black_medium_square: |
|WiFi LAN          |:heavy_check_mark:    |:heavy_check_mark:    |:heavy_check_mark:    |
|WiFi Hotspot      |n/a                   |n/a                   |:black_medium_square: |
|WiFi Direct       |n/a                   |n/a                   |n/a                   |
|WiFi Aware        |n/a                   |n/a                   |n/a                   |
|WebRTC            |n/a                   |n/a                   |:black_medium_square: |
|NFC               |n/a                   |:black_medium_square: |n/a                   |
|USB               |n/a                   |n/a                   |n/a                   |
|AWDL              |:black_medium_square: |:black_medium_square: |:black_medium_square: |
