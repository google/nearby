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
load("//tools/build_defs/golden_test:golden_sh_test.bzl", "golden_sh_test")

licenses(["notice"])

cc_library(
    name = "types",
    linkopts = [
        "advapi32.lib",
        "comdlg32.lib",
        "gdi32.lib",
        "kernel32.lib",
        "odbc32.lib",
        "odbccp32.lib",
        "ole32.lib",
        "oleaut32.lib",
        "shell32.lib",
        "user32.lib",
        "uuid.lib",
        "winmm.lib",
        "winspool.lib",
        "comsuppwd.lib",
        "setupapi.lib",
        "dnsapi.lib",
        "wlanapi.lib",
        "shlwapi.lib",
    ],
    textual_hdrs = glob(["**/*.h"]),
    visibility = [
        "//connections/windows:__subpackages__",
        "//internal/platform/implementation/windows:__subpackages__",
        "//location/nearby/cpp/fastpair/internal:__subpackages__",
        "//location/nearby/cpp/sharing/implementation/internal:__subpackages__",
    ],
)

golden_sh_test(
    name = "base_has_no_coroutines",
    srcs = ["patch_winrt_base.sh"],
    args = [
        "$(location :winrt/base.h)",
        "$(location :base-no-coroutines.patch)",
        "base-no-coroutines.h",
    ],
    data = [
        ":base-no-coroutines.patch",
        ":winrt/base.h",
    ],
    golden = ":winrt/base.h",
    verbose = True,
)
