# Nearby Share sample app

This app is a demonstration of how to bind to and use the Nearby Share slice.

## Build instructions
From the root of the repository:
```
// Run this before importing into Android Studio!
$ cd sharing/android/example
$ gradle wrapper
$ ./gradlew build
```
For the gradle wrapper, we recommend Gradle 8.0+.

## Key callouts
MainViewModel - contains the business logic for processing and binding to the slice, as well as the lifecycle of the slice.

MainActivity - hosts the `MainView` composable and provides the intent to fill in the slice's PendingIntent action with, providing the data to send via Nearby Share.

## Screenshots
<p align="left">
    <img alt="No devices available screenshot" src="screenshots/no_devices.png" width="33%">
    <img alt="Devices available screenshot" src="screenshots/devices.png" width="33%">
    <img alt="Consent prompt screenshot" src="screenshots/consent.png" width="33%">
</p>
