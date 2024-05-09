licenses(["notice"])

# Copyright 2023 Google LLC
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
package(default_visibility = [
    "//:__subpackages__",
    "//location/nearby/cpp/sharing/implementation:__subpackages__",
])

licenses(["notice"])

# /**********************************
# * WARNING -- DO NOT MODIFY *
# **********************************/
# Only add things to this directory that keep it in sync with Chromium's crypto library found at
# https://source.chromium.org/chromium/chromium/src/+/main:crypto/
# New classes that don't originate from there should be added to
# google3/third_party/nearby/internal/crypto instead.
cc_library(
    name = "crypto_cros",
    srcs = [
        "aead.cc",
        "ec_private_key.cc",
        "ec_signature_creator.cc",
        "ec_signature_creator_impl.cc",
        "encryptor.cc",
        "hkdf.cc",
        "hmac.cc",
        "nearby_base.cc",
        "openssl_util.cc",
        "rsa_private_key.cc",
        "secure_hash.cc",
        "secure_util.cc",
        "sha2.cc",
        "signature_verifier.cc",
        "symmetric_key.cc",
    ],
    hdrs = [
        "aead.h",
        "crypto_export.h",
        "ec_private_key.h",
        "ec_signature_creator.h",
        "ec_signature_creator_impl.h",
        "encryptor.h",
        "hkdf.h",
        "hmac.h",
        "nearby_base.h",
        "openssl_util.h",
        "rsa_private_key.h",
        "secure_hash.h",
        "secure_util.h",
        "sha2.h",
        "signature_verifier.h",
        "symmetric_key.h",
    ],
    copts = [
        "-DCRYPTO_IMPLEMENTATION",
        "-Ithird_party",
    ],
    deps = [
        "@boringssl//:crypto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "crypto_cros_unittests",
    size = "large",
    srcs = [
        "aead_unittest.cc",
        "ec_private_key_unittest.cc",
        "ec_signature_creator_unittest.cc",
        "encryptor_unittest.cc",
        "hmac_unittest.cc",
        "rsa_private_key_unittest.cc",
        "secure_hash_unittest.cc",
        "sha2_unittest.cc",
        "signature_verifier_unittest.cc",
        "symmetric_key_unittest.cc",
    ],
    copts = [
        "-DUNIT_TEST",
        "-Wno-inconsistent-missing-override",
        "-Wno-non-virtual-dtor",
        "-Ithird_party",
    ],
    deps = [
        ":crypto_cros",
        "//internal/platform/implementation:types",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)
