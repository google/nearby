# CocoaPods for iOS

The root podspecs build Nearby Connections from source for iOS 13 and later.
`NearbyConnections` contains the Swift API. `NearbyCoreAdapter` contains its
Objective-C adapter, Nearby's C++ implementation, and the protobuf, ukey2, JSON,
and hashing sources pinned by the repository's submodules. `NearbyAbseil` and
`NearbyBoringSSL` build the source distributions used by Nearby's Swift package.
No prebuilt XCFramework or protobuf code generation step is required.

## Installation

Until the pods are published to the CocoaPods registry, declare **all four**
pods in the application's Podfile. CocoaPods dependencies cannot specify a Git
URL, so the Swift pod alone cannot locate unpublished dependencies.

```ruby
platform :ios, '13.0'

target 'YourApp' do
  use_frameworks! :linkage => :static

  nearby = {
    :git => 'https://github.com/google/nearby.git',
    :branch => 'main',
    :submodules => true,
  }
  pod 'NearbyCoreAdapter', nearby
  pod 'NearbyConnections', nearby
  pod 'NearbyAbseil', :podspec =>
    'https://raw.githubusercontent.com/google/nearby/main/NearbyAbseil.podspec'
  pod 'NearbyBoringSSL', :podspec =>
    'https://raw.githubusercontent.com/google/nearby/main/NearbyBoringSSL.podspec'
end
```

Use a revision containing the podspecs. For reproducible builds, replace
`:branch => 'main'` with `:commit => '<full commit SHA>'` for both pods and commit
the application's `Podfile.lock`. Use the same commit in the dependency podspec
URLs in place of `main`. CocoaPods checks out the required submodules.

For a local checkout, first run `git submodule update --init --recursive`, then
replace the `nearby` hash with `{ :path => '/path/to/nearby' }` and the
dependency podspec URLs with `/path/to/nearby/NearbyAbseil.podspec` and
`/path/to/nearby/NearbyBoringSSL.podspec`. Their source remains external to the
Nearby checkout.

Run `pod install`, open the application's `.xcworkspace`, and import the Swift
API with `import NearbyConnections`. An Objective-C wrapper can depend on and
import `NearbyCoreAdapter` directly. C++ implementation headers are kept out of
the adapter's public module so Swift clients do not need C++ interoperability.

When migrating an existing app, remove its Nearby Swift Package dependency
before adding the pods. Linking both distributions would include two copies of
the same Objective-C classes.

Applications still need the permissions and capabilities required by their
chosen Nearby transports. Packaging does not configure these for the app.

## Validation and maintenance

With Xcode, Swift, Ruby, and CocoaPods 1.16.2 installed, run from the repository
root:

```shell
ruby connections/swift/CocoaPods/check_sources.rb
pod lib lint NearbyConnections.podspec --platforms=ios \
  --include-podspecs=NearbyCoreAdapter.podspec --test-specs=Tests \
  --external-podspecs='{NearbyAbseil,NearbyBoringSSL}.podspec'
```

The source check compares CocoaPods' resolved compilation files with
`swift package dump-package`. Keep the native source exclusions aligned with
`Package.swift` when files are added or removed. The consumer tests import both
public modules and check payloads, connection configuration, and SHA-256 and
HMAC test vectors without starting radio operations.

The implementation dependencies use Nearby-specific pod names to keep their
source versions and header settings independent of other pods. Abseil is pinned
to the January 2026 revision used by SwiftPM. BoringSSL uses the same 0.7.2
distribution, including its unprefixed API and portable C implementation.
Validate changes to these pins against their SwiftPM manifests and with the
consumer tests.

The `cocoapods-<version>` source tags are reserved for CocoaPods releases. A
maintainer must create the corresponding tag and publish all four specs
before registry-only installation is available. Git and local installation do
not require a published CocoaPods release.
