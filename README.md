# Nearby Connections Library

This is not an officially supported Google product.

**Coathored by:**
*  (Java/C++) Varun Kapoor “reznor”
*  (Java) Maria-Ines Carrera “marianines”
*  (Java) Will Harmon “xlythe”
*  (Java/C++/ObjC) Alex Kang “alexanderkang”
*  (Java/C++) Amanda Lee “ahlee”
*  (C++) Tracy Zhou “tracyzhou”
*  (ObjC) Dan Webb “dwebb”
*  (C++) John Kaczor “johngk”
*  (C++/ObjC) Edwin Wu “edwinwu”
*  (C++) Alexey Polyudov “apolyudov”

**Status:** Implemented in C++

**Design reviewers:** TODO

**Implementation reviewer**: TODO

**Last Updated:** TODO

# Overview

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

# Checkout, build, test instructions
## Checkout
pre-requisites: git
```
git clone https://github.com/google/nearby-connections
cd nearby-connections
git submodule update --init --recursive
```

this is a "source root" directory of the project

## Build
pre-requisites:
openssl, cmake, c++ toolchain (c++17-capable)

from "source root", run:

```
mkdir build; cd build
cmake -Dnearby_USE_LOCAL_PROTOBUF=ON -Dnearby_USE_LOCAL_ABSL=ON ..
make
```
## Running unit tests

from "source root/build", run:

```
ctest -V
```
