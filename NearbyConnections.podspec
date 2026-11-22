# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

Pod::Spec.new do |spec|
  spec.name = 'NearbyConnections'
  spec.version = '0.0.1'
  spec.summary = 'Discover, connect to, and exchange data with nearby devices.'
  spec.homepage = 'https://github.com/google/nearby'
  spec.license = { :type => 'Apache-2.0', :file => 'LICENSE' }
  spec.author = 'Google LLC'
  spec.source = { :git => 'https://github.com/google/nearby.git', :tag => "cocoapods-#{spec.version}", :submodules => true }
  spec.ios.deployment_target = '13.0'
  spec.swift_version = '5.0'
  spec.source_files = 'connections/swift/NearbyConnections/Sources/**/*.swift'
  spec.dependency 'NearbyCoreAdapter', spec.version.to_s

  spec.test_spec 'Tests' do |tests|
    tests.source_files = 'connections/swift/CocoaPods/Tests/*.{swift,mm}'
    tests.pod_target_xcconfig = {
      'CLANG_CXX_LANGUAGE_STANDARD' => 'c++20',
      'HEADER_SEARCH_PATHS' => '$(inherited) "$(PODS_TARGET_SRCROOT)" "$(PODS_ROOT)/NearbyAbseil" "$(PODS_ROOT)/NearbyBoringSSL/src/include"',
    }
  end
end
