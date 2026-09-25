// Copyright 2020 Google LLC
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

#ifndef CORE_INTERNAL_INTERNAL_PAYLOAD_FACTORY_H_
#define CORE_INTERNAL_INTERNAL_PAYLOAD_FACTORY_H_

#include <memory>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/internal_payload.h"
#include "connections/payload.h"
#include "internal/platform/expected.h"

namespace nearby {
namespace connections {

// Creates an OutgoingInternalPayload representing an outgoing Payload.
ErrorOr<std::unique_ptr<OutgoingInternalPayload>> CreateOutgoingInternalPayload(
    Payload payload);

// Creates an IncomingInternalPayload representing an incoming Payload from a
// remote endpoint. If `is_file_path_in_use` is provided and returns true for
// the resolved file path of a FILE payload, creation fails with
// IO_FILE_OPENING_ERROR before opening the output file.
ErrorOr<std::unique_ptr<IncomingInternalPayload>> CreateIncomingInternalPayload(
    const location::nearby::connections::PayloadTransferFrame& frame,
    const std::string& custom_save_path,
    absl::AnyInvocable<bool(absl::string_view)> is_file_path_in_use = nullptr);

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_INTERNAL_PAYLOAD_FACTORY_H_
