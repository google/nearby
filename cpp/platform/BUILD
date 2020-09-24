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

cc_library(
    name = "utils",
    srcs = [
        "base64_utils.cc",
        "cancelable_alarm.cc",
        "pipe.cc",
        "prng.cc",
        "reliability_utils.cc",
    ],
    hdrs = [
        "base64_utils.h",
        "cancelable_alarm.h",
        "pipe.h",
        "prng.h",
        "reliability_utils.h",
        "synchronized.h",
    ],
    visibility = [
        "//core:__subpackages__",
        "//platform/impl:__subpackages__",
        "//location/nearby/setup/core/internal:__subpackages__",
    ],
    deps = [
        ":types",
        "//platform/api",
        "//platform/port:string",
        "//absl/strings",
        "//absl/time",
    ],
)

cc_library(
    name = "types",
    hdrs = [
        "byte_array.h",
        "callable.h",
        "cancelable.h",
        "container_of.h",
        "exception.h",
        "ptr.h",
        "runnable.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//core:__subpackages__",
        "//platform:__subpackages__",
        "//location/nearby/setup/core:__subpackages__",
    ],
    deps = [
        ":logging",
        "//platform/port:down_cast",
        "//platform/port:string",
    ],
)

cc_library(
    name = "logging",
    hdrs = [
        "logging.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//core:__subpackages__",
    ],
    deps = [
        "//absl/base",
        "//absl/base:raw_logging_internal",
    ],
)

cc_test(
    name = "platform_test",
    timeout = "short",
    srcs = [
        "atomic_reference_test.cc",
        "byte_array_test.cc",
        "container_of_test.cc",
        "pipe_test.cc",
        "prng_test.cc",
        "ptr_test.cc",
        "settable_future_test.cc",
    ],
    deps = [
        ":utils",
        "//platform:types",
        "//platform/api",
        "//platform/impl/g3",
        "//platform/port:string",
        "//testing/base/public:gunit_main",
        "//absl/time",
    ],
)
