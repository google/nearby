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
import 'dart:isolate';

import 'package:mobile.flutter.contrib.disposable/disposable.dart';

/// A convenience class for listening to a [ReceivePort].
///
/// This can be used to listen to ongoing updates from the c++ layer.
class Listener with AutoDisposerMixin {
  Listener() {
    autoDisposeCustom(_port.close);
    autoDisposeCustom(_subscription.cancel);
  }

  final _port = ReceivePort();
  late final _subscription = _port.listen(null);

  int get port => _port.sendPort.nativePort;

  void onData(void Function(dynamic data)? handleData) =>
      _subscription.onData(handleData);
}
