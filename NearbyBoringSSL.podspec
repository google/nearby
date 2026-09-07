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
  spec.name = 'NearbyBoringSSL'
  spec.version = '0.7.2'
  spec.summary = 'BoringSSL implementation dependency for Nearby Connections.'
  spec.homepage = 'https://github.com/google/nearby'
  spec.license = { :type => 'Mixed', :file => 'LICENSE' }
  spec.author = 'Google LLC'
  spec.source = {
    :git => 'https://github.com/firebase/boringssl-SwiftPM.git',
    :tag => spec.version.to_s,
  }
  spec.ios.deployment_target = '13.0'
  spec.source_files = [
    'err_data.c',
    'src/{crypto,ssl,third_party/fiat,include}/**/*.{c,cc,h}',
  ]
  spec.exclude_files = [
    'src/**/*_test.cc',
    'src/**/test/**/*',
    'src/crypto/fipsmodule/bcm.c',
  ]
  spec.project_header_files = 'src/**/*.h'
  spec.header_mappings_dir = '.'
  spec.preserve_paths = 'src/**/*'
  spec.module_map = false
  spec.libraries = 'c++'
  spec.pod_target_xcconfig = {
    'USE_HEADERMAP' => 'NO',
    'CLANG_CXX_LANGUAGE_STANDARD' => 'gnu++14',
    'GCC_PREPROCESSOR_DEFINITIONS' => '$(inherited) OPENSSL_NO_ASM=1',
    'HEADER_SEARCH_PATHS' => '$(inherited) "$(PODS_TARGET_SRCROOT)" "$(PODS_TARGET_SRCROOT)/src/include"',
  }
end
