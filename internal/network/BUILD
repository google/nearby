# Copyright 2022 Google LLC
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

licenses(["notice"])

cc_library(
    name = "url",
    srcs = [
        "url.cc",
        "utils.cc",
    ],
    hdrs = [
        "url.h",
        "utils.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
        "//internal:__pkg__",
        "//internal:__subpackages__",
        "//location/nearby/cpp/sharing:__subpackages__",
        "//sharing:__subpackages__",
    ],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "types",
    srcs = [
        "http_request.cc",
        "http_response.cc",
        "http_status_code.cc",
    ],
    hdrs = [
        "http_body.h",
        "http_client.h",
        "http_client_factory.h",
        "http_request.h",
        "http_response.h",
        "http_status_code.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
        "//internal:__pkg__",
        "//internal:__subpackages__",
        "//location/nearby/cpp/sharing:__subpackages__",
        "//sharing:__subpackages__",
    ],
    deps = [
        ":url",
        "//internal/platform:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "nearby_http_client",
    srcs = [
        "http_client_impl.cc",
    ],
    hdrs = [
        "debug.h",
        "http_client_factory_impl.h",
        "http_client_impl.h",
    ],
    defines = ["_SILENCE_CLANG_COROUTINE_MESSAGE"],
    visibility = [
        "//fastpair:__subpackages__",
        "//internal:__pkg__",
        "//internal:__subpackages__",
        "//sharing:__subpackages__",
    ],
    deps = [
        ":types",
        "//internal/platform:types",
        "//internal/platform/implementation:comm",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "nearby_http_client_test",
    size = "small",
    timeout = "short",
    srcs = [
        "http_client_impl_test.cc",
        "http_request_test.cc",
        "http_response_test.cc",
        "http_status_code_test.cc",
        "url_test.cc",
        "utils_test.cc",
    ],
    shard_count = 8,
    deps = [
        ":nearby_http_client",
        ":types",
        ":url",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
