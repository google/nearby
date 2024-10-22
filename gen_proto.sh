#!/bin/bash

# Copyright 2024 Google LLC
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

COMPILED_PROTO_PATH="compiled_proto"

${PROTOC} --cpp_out=${COMPILED_PROTO_PATH} internal/proto/analytics/connections_log.proto
${PROTOC} --cpp_out=${COMPILED_PROTO_PATH} internal/proto/analytics/fast_pair_log.proto
${PROTOC} --cpp_out=${COMPILED_PROTO_PATH} sharing/proto/analytics/nearby_sharing_log.proto
${PROTOC} --cpp_out=${COMPILED_PROTO_PATH} proto/fast_pair_enums.proto
${PROTOC} --cpp_out=${COMPILED_PROTO_PATH} proto/sharing_enums.proto
