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
  spec.name = 'NearbyAbseil'
  spec.version = '20260107.1'
  spec.summary = 'Abseil implementation dependency for Nearby Connections.'
  spec.homepage = 'https://github.com/google/nearby'
  spec.license = { :type => 'Apache-2.0', :file => 'LICENSE' }
  spec.author = 'Google LLC'
  # Pin the source distribution used by Package.swift's jan-lts dependency.
  spec.source = {
    :http => 'https://github.com/bourdakos1/abseil-cpp-SwiftPM/archive/ecabd65f38702137240fd2599f710b0f5cd89cf1.tar.gz',
    :sha256 => '7d9eff8e65241cee2b3800090b03341fec5cc466a7dc8bcca3695f8a1fb91a5d',
  }
  spec.ios.deployment_target = '13.0'
  spec.source_files = 'absl/**/*.{h,cc}'
  spec.exclude_files = [
    'absl/random/benchmarks.cc',
    'absl/time/internal/cctz/src/time_zone_name_win.cc',
  ]
  spec.project_header_files = 'absl/**/*.h'
  spec.header_mappings_dir = '.'
  spec.preserve_paths = 'absl/**/*'
  spec.module_map = false
  spec.resource_bundles = { 'NearbyAbseil' => ['PrivacyInfo.xcprivacy'] }
  spec.frameworks = 'CoreFoundation'
  spec.libraries = 'c++'
  spec.pod_target_xcconfig = {
    # Flattened header maps would make absl/time/time.h shadow the C time.h.
    'USE_HEADERMAP' => 'NO',
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++17',
    'HEADER_SEARCH_PATHS' => '$(inherited) "$(PODS_TARGET_SRCROOT)"',
  }
end
