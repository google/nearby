# Nearby C++ Library

The repository contains the Nearby project C++ library code. This is not an officially supported Google product.

# About the Nearby Project

## Near Connection
Nearby Connections is a high level protocol on top of Bluetooth/WiFi that acts
as a medium-agnostic socket. Devices are able to advertise, scan, and connect
with one another over any shared medium (eg. BT <-> BT).
Once connected, the two devices share a list of all supported mediums and
attempt to upgrade to the one with the highest bandwidth (eg. BT -> WiFi).
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
We support multiple platforms including Linux, iOS & Windows. The ultimate goal is to build from source. The offical build system we support is [bazel] (https://bazel.build). Before that is accomplished, we provide precompiled libraries as stop-gap solutions. See the following pages for platform specific instructions.

* [iOS](https://github.com/google/nearby/blob/master/docs/ios_build.md)


**Last Updated:** November 2021
