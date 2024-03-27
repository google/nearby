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
- [x]  Supported
- [ ]  Support is possible but not implemented
- N/A: Support is not possible or does not make sense.

## Android
|Mediums           |Advertising           |Scanning              |Data                  |
|------------------|----------------------|----------------------|----------------------|
|Bluetooth Classic |<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|
|BLE (Fast)        |<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|N/A                   |
|BLE (GATT)        |<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|
|BLE (Extended)    |<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|N/A                   |
|BLE (L2CAP)       |N/A                   |N/A                   |<ul><li>[x] </li></ul>|
|Wi-Fi LAN         |<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|
|Wi-Fi Hotspot     |N/A                   |N/A                   |<ul><li>[x] </li></ul>|
|Wi-Fi Direct      |N/A                   |N/A                   |<ul><li>[x] </li></ul>|
|Wi-Fi Aware       |<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|
|WebRTC            |N/A                   |N/A                   |<ul><li>[x] </li></ul>|
|NFC               |<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|
|USB               |<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|
|AWDL              |N/A                   |N/A                   |N/A                   |

## ChromeOS
|Mediums           |Advertising           |Scanning              |Data                  |
|------------------|----------------------|----------------------|----------------------|
|Bluetooth Classic |<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|
|BLE (Fast)        |<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|N/A                   |
|BLE (GATT)        |<ul><li>[ ] </li></ul>|<ul><li>[ ] </li></ul>|<ul><li>[ ] </li></ul>|
|BLE (Extended)    |<ul><li>[ ] </li></ul>|<ul><li>[ ] </li></ul>|N/A                   |
|BLE (L2CAP)       |N/A                   |N/A                   |N/A                   |
|Wi-Fi LAN         |<ul><li>[ ] </li></ul>|<ul><li>[ ] </li></ul>|<ul><li>[x] </li></ul>|
|Wi-Fi Hotspot     |N/A                   |N/A                   |<ul><li>[ ] </li></ul>|
|Wi-Fi Direct      |N/A                   |N/A                   |<ul><li>[ ] </li></ul>|
|Wi-Fi Aware       |N/A                   |N/A                   |N/A                   |
|WebRTC            |N/A                   |N/A                   |<ul><li>[x] </li></ul>|
|NFC               |N/A                   |N/A                   |N/A                   |
|USB               |<ul><li>[ ] </li></ul>|<ul><li>[ ] </li></ul>|<ul><li>[ ] </li></ul>|
|AWDL              |N/A                   |N/A                   |N/A                   |

## Windows
|Mediums           |Advertising           |Scanning              |Data                  |
|------------------|----------------------|----------------------|----------------------|
|Bluetooth Classic |N/A                   |<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|
|BLE (Fast)        |<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|N/A                   |
|BLE (GATT)        |<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|<ul><li>[ ] </li></ul>|
|BLE (Extended)    |<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|N/A                   |
|BLE (L2CAP)       |N/A                   |N/A                   |N/A                   |
|Wi-Fi LAN         |<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|
|Wi-Fi Hotspot     |N/A                   |N/A                   |<ul><li>[x] </li></ul>|
|Wi-Fi Direct      |N/A                   |N/A                   |<ul><li>[ ] </li></ul>|
|Wi-Fi Aware       |N/A                   |N/A                   |N/A                   |
|WebRTC            |N/A                   |N/A                   |<ul><li>[ ] </li></ul>|
|NFC               |N/A                   |N/A                   |N/A                   |
|USB               |<ul><li>[ ] </li></ul>|<ul><li>[ ] </li></ul>|<ul><li>[ ] </li></ul>|
|AWDL              |N/A                   |N/A                   |N/A                   |

## iOS/macOS
|Mediums           |Advertising           |Scanning              |Data                  |
|------------------|----------------------|----------------------|----------------------|
|Bluetooth Classic |N/A                   |<ul><li>[ ] </li></ul>|<ul><li>[ ] </li></ul>|
|BLE (Fast)        |<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|N/A                   |
|BLE (GATT)        |<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|
|BLE (Extended)    |N/A                   |<ul><li>[ ] </li></ul>|N/A                   |
|BLE (L2CAP)       |N/A                   |N/A                   |<ul><li>[ ] </li></ul>|
|Wi-Fi LAN         |<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|<ul><li>[x] </li></ul>|
|Wi-Fi Hotspot     |N/A                   |N/A                   |<ul><li>[ ] </li></ul>|
|Wi-Fi Direct      |N/A                   |N/A                   |N/A                   |
|Wi-Fi Aware       |N/A                   |N/A                   |N/A                   |
|WebRTC            |N/A                   |N/A                   |<ul><li>[ ] </li></ul>|
|NFC               |N/A                   |<ul><li>[ ] </li></ul>|N/A                   |
|USB               |N/A                   |N/A                   |N/A                   |
|AWDL              |<ul><li>[ ] </li></ul>|<ul><li>[ ] </li></ul>|<ul><li>[ ] </li></ul>|
