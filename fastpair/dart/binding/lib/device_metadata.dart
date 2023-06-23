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

// Detail information of a observed device. Compared to the device information
// get from GetObservedDeviceResponse, this class only store the device
// information that is related to UI showing.
class DeviceMetadata {
  String id;
  String name;
  String imageUrl;

  DeviceMetadata({
    required this.id,
    required this.name,
    required this.imageUrl,
  });

  factory DeviceMetadata.fromJson(dynamic data) {
    return DeviceMetadata(
      id: data['id'],
      name: data['name'] == '' ? null : data['name'],
      imageUrl: data['imageUrl'] == '' ? null : data['imageUrl'],
    );
  }
}
