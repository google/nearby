package(default_visibility = [
    "//core:__subpackages__",
    "//platform:__subpackages__",
    "//location/nearby/setup/core:__subpackages__",
])

cc_library(
    name = "api",
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "ble.h",
        "ble_v2.h",
        "bluetooth_adapter.h",
        "bluetooth_classic.h",
        "condition_variable.h",
        "count_down_latch.h",
        "executor.h",
        "future.h",
        "hash_utils.h",
        "input_file.h",
        "input_stream.h",
        "lock.h",
        "multi_thread_executor.h",
        "output_file.h",
        "output_stream.h",
        "scheduled_executor.h",
        "settable_future.h",
        "single_thread_executor.h",
        "socket.h",
        "submittable_executor.h",
        "system_clock.h",
        "thread_utils.h",
        "wifi.h",
    ],
    deps = [
        "//platform:types",
        "//platform/port:down_cast",
        "//platform/port:string",
    ],
)

cc_library(
    name = "lock",
    hdrs = ["lock.h"],
    visibility = [
        "//platform:__subpackages__",
    ],
)

cc_library(
    name = "condition_variable",
    hdrs = ["condition_variable.h"],
    visibility = [
        "//platform:__subpackages__",
    ],
    deps = ["//platform:types"],
)
