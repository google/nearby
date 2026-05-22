# WebRtc support for Nearby Connections

This directory contains the implementation to support WebRtc in the Nearby
Connection library.

All dependencies on webrtc MUST be limited to targets in this directory.

## To Enable WebRtc support

To enabled WebRtc support undefine the ```NO_WEBRTC``` preprocessor symbol in
the file **connections/implementation/mediums/mediums.cc**.

When building using bazel, pass the build flag
```--//:enable_webrtc=true``` to set the correct symbol.

Make sure the binary is linked with an implementation of the
```WebRtcImplementationPlatform```.