// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import 'dart:ffi';

import 'device_metadata.dart';

typedef InitMediatorType = Pointer<Uint64> Function();
typedef InitMediatorDartType = Pointer<Uint64> Function();

typedef CloseMediatorType = Void Function(Pointer<Uint64> /*service*/);
typedef CloseMediatorDartType = void Function(Pointer<Uint64> /*service*/);

typedef StartFastPairScanningType = Void Function(Pointer<Uint64> /*service*/);
typedef StartFastPairScanningDartType = void Function(
    Pointer<Uint64> /*service*/);

typedef AddNotificationControllerObserverType = Void Function(
  Pointer<Uint64> /*service*/,
  Int64 /* callback*/,
);
typedef AddNotificationControllerObserverDartType = void Function(
  Pointer<Uint64> /*service*/,
  int /* callback*/,
);

typedef RemoveNotificationControllerObserverType = Void Function(
  Pointer<Uint64> /*service*/,
  Int64 /* callback*/,
);
typedef RemoveNotificationControllerObserverDartType = void Function(
  Pointer<Uint64> /*service*/,
  int /* callback*/,
);

typedef DiscoveryClickedType = Void Function(
  Pointer<Uint64> /*service*/,
  Int64 /* callback*/,
);
typedef DiscoveryClickedDartType = void Function(
  Pointer<Uint64> /*service*/,
  int /* callback*/,
);

typedef InitializeApiType = IntPtr Function(Pointer<Void>);
typedef InitializeApiDartType = int Function(Pointer<Void>);

//callback type
typedef OnMetadataChanged = void Function({
  required List<DeviceMetadata> deviceMetadata,
});
