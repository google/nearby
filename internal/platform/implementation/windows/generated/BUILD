load("@rules_cc//cc:cc_library.bzl", "cc_library")

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
licenses(["notice"])

cc_library(
    name = "types",
    hdrs = glob(["**/*.h"]),
    includes = ["."],
    linkopts = [
        "wininet.lib",
        "advapi32.lib",
        "bcrypt.lib",
        "cfgmgr32.lib",
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
        "wtsapi32.lib",
        "comsuppwd.lib",
        "setupapi.lib",
        "dnsapi.lib",
        "wlanapi.lib",
        "shlwapi.lib",
        "runtimeobject.lib",
    ],
    visibility = [
        "//internal:__subpackages__",
        "//internal/platform/implementation/windows:__subpackages__",
        "//location/nearby/apps/better_together/plugins:__subpackages__",
        "//location/nearby/apps/better_together/windows/nearby_share:__pkg__",
        "//sharing:__subpackages__",
    ],
)
