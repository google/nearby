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

import 'ffi_types.dart';

class FastpairBinding {
  static final _lib = DynamicLibrary.open('fastpair_dart.dll');

  late final InitMediatorDartType initMediator = _lib
      .lookup<NativeFunction<InitMediatorType>>(
        'InitMediatorDart',
      )
      .asFunction();

  late final CloseMediatorDartType closeMediator = _lib
      .lookup<NativeFunction<CloseMediatorType>>('CloseMediator')
      .asFunction();

  late final StartFastPairScanningDartType startFastPairScanning = _lib
      .lookup<NativeFunction<StartFastPairScanningType>>('StartScanDart')
      .asFunction();

  late final InitializeApiDartType initializeApi = _lib
      .lookup<NativeFunction<InitializeApiType>>('Dart_InitializeApiDL')
      .asFunction();

  late final AddNotificationControllerObserverDartType
      addNotificationControllerObserver = _lib
          .lookup<NativeFunction<AddNotificationControllerObserverType>>(
              'AddNotificationControllerObserverDart')
          .asFunction();

  late final RemoveNotificationControllerObserverDartType
      removeNotificationControllerObserver = _lib
          .lookup<NativeFunction<RemoveNotificationControllerObserverType>>(
              'RemoveNotificationControllerObserverDart')
          .asFunction();

  late final DiscoveryClickedDartType discoveryClicked = _lib
      .lookup<NativeFunction<DiscoveryClickedType>>('DiscoveryClickedDart')
      .asFunction();
}
