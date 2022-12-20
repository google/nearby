licenses(["notice"])

# Copyright 2020 Google LLC
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
package(default_visibility = ["//:__subpackages__"])

licenses(["notice"])

cc_library(
    name = "crypto",
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
        "random.cc",
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
        "random.h",
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
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "crypto_unittests",
    size = "large",
    srcs = [
        "aead_unittest.cc",
        "ec_private_key_unittest.cc",
        "ec_signature_creator_unittest.cc",
        "encryptor_unittest.cc",
        "hmac_unittest.cc",
        "random_unittest.cc",
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
        ":crypto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)
