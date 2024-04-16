#!/bin/bash

COMPILED_PROTO_PATH="compiled_proto"

${PROTOC} --cpp_out=${COMPILED_PROTO_PATH} internal/proto/analytics/connections_log.proto
${PROTOC} --cpp_out=${COMPILED_PROTO_PATH} internal/proto/analytics/experiments_log.proto
${PROTOC} --cpp_out=${COMPILED_PROTO_PATH} internal/proto/analytics/fast_pair_log.proto
${PROTOC} --cpp_out=${COMPILED_PROTO_PATH} sharing/proto/analytics/nearby_sharing_log.proto
${PROTOC} --cpp_out=${COMPILED_PROTO_PATH} proto/fast_pair_enums.proto
${PROTOC} --cpp_out=${COMPILED_PROTO_PATH} proto/sharing_enums.proto
