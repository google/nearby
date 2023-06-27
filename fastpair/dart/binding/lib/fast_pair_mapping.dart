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
import 'dart:typed_data';

import 'package:auto_disposable/auto_disposable.dart';

import 'listener.dart';
import 'bindings.dart';
import 'ffi_types.dart';

import 'package:third_party.nearby.fastpair.dart.proto/callbacks.pb.dart'
    as proto;
import 'package:third_party.nearby.fastpair.dart.proto/enum.pb.dart'
    as enumproto;

class FastPairMapping with AutoDisposerMixin {
  final binding = FastpairBinding();
  late final Pointer<Uint64> _fastpairPointer;

  FastPairMapping._internal() {
    final result = binding.initializeApi(NativeApi.initializeApiDLData);
    if (result != 0) {
      throw 'failed to init API.';
    }
    _fastpairPointer = binding.initMediator();
    if (_fastpairPointer == nullptr) {
      throw 'initMediator failed: _fastpairPointer is null.';
    }
    autoDisposeCustom(() => binding.closeMediator(_fastpairPointer));
  }
  static final _instance = FastPairMapping._internal();

  factory FastPairMapping() => _instance;

  void startScan() {
    binding.startFastPairScanning(_fastpairPointer);
  }

  Listener addNotificationControllerObserver(
      OnMetadataChanged onMetadataChanged) {
    final listener = Listener();

    listener.onData((data) {
      if (data is Uint8List) {
        final params = proto.DeviceDownloadedCallbackData.fromBuffer(data);
        onMetadataChanged(
          deviceMetadata: List<DeviceMetadata>.from(
            params.devices.map(
              (device) => DeviceMetadata(
                  id: device.id.toString(),
                  name: device.name,
                  imageUrl: device.imageUrl),
            ),
          ),
        );
      }
    });

    binding.addNotificationControllerObserver(_fastpairPointer, listener.port);

    return listener;
  }

  void removeNotificationControllerObserver(dynamic listener) {
    binding.removeNotificationControllerObserver(
      _fastpairPointer,
      listener.port,
    );
  }

  void connectClicked() {
    binding.discoveryClicked(_fastpairPointer,
        enumproto.DiscoveryAction.DISCOVERY_ACTION_PAIR_TO_DEVICE.value);
  }

  void learnMoreClicked() {
    binding.discoveryClicked(_fastpairPointer,
        enumproto.DiscoveryAction.DISCOVERY_ACTION_LEARN_MORE.value);
  }

  void dismissedByUserClicked() {
    binding.discoveryClicked(_fastpairPointer,
        enumproto.DiscoveryAction.DISCOVERY_ACTION_DISMISSED_BY_USER.value);
  }

  void unknownAction() {
    binding.discoveryClicked(_fastpairPointer,
        enumproto.DiscoveryAction.DISCOVERY_ACTION_UNKNOWN.value);
  }
}
