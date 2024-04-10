
// Copyright 2024 Google LLC
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

#ifndef CORE_INTERNAL_MEDIUMS_MULTIPLEX_MULTIPLEX_FRAMES_H_
#define CORE_INTERNAL_MEDIUMS_MULTIPLEX_MULTIPLEX_FRAMES_H_

#include <string>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "proto/mediums/multiplex_frames.pb.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace multiplex {

constexpr int kServiceIdHashLength = 4;

// Serialize/Deserialize MultiplexFrame messages.

// Parses incoming MultiplexFrame message.
// Returns MultiplexFrame if parser was able to understand it, or
// Exception::kInvalidProtocolBuffer, if parser failed.

// Generates a service ID hash bytes with {@link
// MultiplexFrames#SERVICE_ID_HASH_LENGTH}.
ByteArray GenerateServiceIdHash(const std::string& service_id);

// Generates a service ID hash bytes with salt and {@link
// MultiplexFrames#SERVICE_ID_HASH_LENGTH}.
ByteArray GenerateServiceIdHashWithSalt(const std::string& service_id,
                                        std::string salt);

// Converts the service Id hash bytes to a Base64 encoded string to be used as a
// {@code Map} key.
std::string GenerateServiceIdHashKey(const ByteArray& service_id_hash);

// Generates a service ID hash bytes with {@link
// MultiplexFrames#SERVICE_ID_HASH_LENGTH} and converts to a Base64 encoded
// string to be used as a {@code Map} key.
std::string GenerateServiceIdHashKey(const std::string& service_id);

// Generates a service ID hash bytes with salt and {@link
// MultiplexFrames#SERVICE_ID_HASH_LENGTH} and converts to a Base64 encoded
// string to be used as a { @code Map } key.
std::string GenerateServiceIdHashKeyWithSalt(const std::string& service_id,
                                             std::string salt);

// Build a MultiplexFrame Connection Request frame Bytes stream.
// @param service_id The service ID of the connection.
// @param service_id_hash_salt The salt used to generate the service ID hash.
ByteArray ForConnectionRequest(const std::string& service_id,
                               const std::string& service_id_hash_salt);

// Build a MultiplexFrame Connection Response frame Bytes stream.
// @param salted_service_id_hash The salted service ID hash.
// @param service_id_hash_salt The salt used to generate the service ID hash.
// @param response_code The response code of the connection.
ByteArray ForConnectionResponse(
    const ByteArray& salted_service_id_hash,
    const std::string& service_id_hash_salt,
    location::nearby::mediums::ConnectionResponseFrame::ConnectionResponseCode
        response_code);

// Build a MultiplexFrame Disconnection frame Bytes stream.
// @param service_id The service ID of the connection.
// @param service_id_hash_salt The salt used to generate the service ID hash.
ByteArray ForDisconnection(const std::string& service_id,
                           const std::string& service_id_hash_salt);

// Build a MultiplexFrame Data frame Bytes stream.
// @param service_id The service ID of the connection.
// @param service_id_hash_salt The salt used to generate the service ID hash.
// @param should_pass_salt Whether to pass the salt in the data frame.
// @param data The data to send.
ByteArray ForData(const std::string& service_id,
                  const std::string& service_id_hash_salt,
                  bool should_pass_salt, const ByteArray& data);

ExceptionOr<location::nearby::mediums::MultiplexFrame> FromBytes(
    const ByteArray& multiplex_frame_bytes);

bool IsControlFrame(
    location::nearby::mediums::MultiplexFrame::MultiplexFrameType frame_type);
bool IsDataFrame(
    location::nearby::mediums::MultiplexFrame::MultiplexFrameType frame_type);
bool IsValid(const location::nearby::mediums::MultiplexFrame& frame);
bool IsValidControlFrame(
    const location::nearby::mediums::MultiplexFrame& frame);
bool IsValidDataFrame(const location::nearby::mediums::MultiplexFrame& frame);
bool IsMultiplexFrame(const ByteArray& data);

}  // namespace multiplex
}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_MULTIPLEX_MULTIPLEX_FRAMES_H_
