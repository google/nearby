// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_NEARBY_INTERNAL_RPC_UTILS_H_
#define THIRD_PARTY_NEARBY_INTERNAL_RPC_UTILS_H_

#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "third_party/grpc/include/grpcpp/client_context.h"
#include "third_party/grpc/include/grpcpp/support/status.h"

namespace nearby::rpc {

// Converts a gRPC status to an absl status.
inline absl::Status GrpcStatusToAbslStatus(const grpc::Status& status) {
  if (status.ok()) {
    return absl::OkStatus();
  }
  return absl::Status(static_cast<absl::StatusCode>(status.error_code()),
                      std::string(status.error_message()));
}

// A struct that holds the arguments for an async RPC call. This is used to
// ensure that the RPC arguments outlive the RPC call.
template <typename Request, typename Response>
struct AsyncRpcArgs {
  grpc::ClientContext context;
  Request request;
  Response response;
  absl::AnyInvocable<void(const absl::StatusOr<Response>& response) &&>
      callback;
};

}  // namespace nearby::rpc

#endif  // THIRD_PARTY_NEARBY_INTERNAL_RPC_UTILS_H_
