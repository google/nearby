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

#include "connections/implementation/mediums/multiplex/multiplex_frames.h"

#include <string>
#include <utility>

#include "connections/implementation/mediums/utils.h"
#include "internal/platform/base64_utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/logging.h"


namespace nearby {
namespace connections {
namespace mediums {
namespace multiplex {

using ::location::nearby::mediums::MultiplexFrame;
using ::location::nearby::mediums::MultiplexControlFrame;
using ::location::nearby::mediums::ConnectionResponseFrame;

ByteArray GenerateServiceIdHash(const std::string& service_id) {
  return Utils::Sha256Hash(service_id, kServiceIdHashLength);
}

ByteArray GenerateServiceIdHashWithSalt(const std::string& service_id,
                                        std::string salt) {
  if (salt.empty()) {
    return GenerateServiceIdHash(service_id);
  }

  return Utils::Sha256Hash(service_id + salt, kServiceIdHashLength);
}

std::string GenerateServiceIdHashKey(const ByteArray& service_id_hash) {
  return Base64Utils::Encode(service_id_hash);
}

std::string GenerateServiceIdHashKey(const std::string& service_id) {
  return GenerateServiceIdHashKey(GenerateServiceIdHash(service_id));
}

std::string GenerateServiceIdHashKeyWithSalt(const std::string& service_id,
                                             std::string salt) {
  return GenerateServiceIdHashKey(
      GenerateServiceIdHashWithSalt(service_id, salt));
}

ByteArray ToBytes(MultiplexFrame&& frame) {
  ByteArray bytes(frame.ByteSizeLong());
  frame.SerializeToArray(bytes.data(), bytes.size());
  return bytes;
}

ByteArray ForConnectionRequest(const std::string& service_id,
                               const std::string& service_id_hash_salt) {
  MultiplexFrame frame;

  frame.set_frame_type(MultiplexFrame::CONTROL_FRAME);
  auto* header = frame.mutable_header();
  header->set_salted_service_id_hash(std::string(
      GenerateServiceIdHashWithSalt(service_id, service_id_hash_salt)));
  header->set_service_id_hash_salt(service_id_hash_salt);

  auto* control_frame = frame.mutable_control_frame();
  control_frame->set_control_frame_type(
      MultiplexControlFrame::CONNECTION_REQUEST);

  return ToBytes(std::move(frame));
}

ByteArray ForConnectionResponse(
    const ByteArray& salted_service_id_hash,
    const std::string& service_id_hash_salt,
    ConnectionResponseFrame::ConnectionResponseCode response_code) {
  MultiplexFrame frame;

  frame.set_frame_type(MultiplexFrame::CONTROL_FRAME);
  auto* header = frame.mutable_header();
  header->set_salted_service_id_hash(std::string(salted_service_id_hash));
  header->set_service_id_hash_salt(service_id_hash_salt);

  auto* control_frame = frame.mutable_control_frame();
  control_frame->set_control_frame_type(
      MultiplexControlFrame::CONNECTION_RESPONSE);

  auto* response_frame = control_frame->mutable_connection_response_frame();
  response_frame->set_connection_response_code(response_code);

  return ToBytes(std::move(frame));
}

ByteArray ForDisconnection(const std::string& service_id,
                           const std::string& service_id_hash_salt) {
  MultiplexFrame frame;

  frame.set_frame_type(MultiplexFrame::CONTROL_FRAME);
  auto* header = frame.mutable_header();
  header->set_salted_service_id_hash(std::string(
      GenerateServiceIdHashWithSalt(service_id, service_id_hash_salt)));
  header->set_service_id_hash_salt(service_id_hash_salt);

  auto* control_frame = frame.mutable_control_frame();
  control_frame->set_control_frame_type(
      MultiplexControlFrame::DISCONNECTION);

  return ToBytes(std::move(frame));
}

ByteArray ForData(const std::string& service_id,
                  const std::string& service_id_hash_salt,
                  bool should_pass_salt, const ByteArray& data) {
  MultiplexFrame frame;

  frame.set_frame_type(MultiplexFrame::DATA_FRAME);
  auto* header = frame.mutable_header();
  header->set_salted_service_id_hash(std::string(
      GenerateServiceIdHashWithSalt(service_id, service_id_hash_salt)));
  if (should_pass_salt) {
    header->set_service_id_hash_salt(service_id_hash_salt);
  }

  auto* data_frame = frame.mutable_data_frame();
  data_frame->set_data(std::string(std::move(data)));

  return ToBytes(std::move(frame));
}

ExceptionOr<MultiplexFrame> FromBytes(const ByteArray& multiplex_frame_bytes){
  MultiplexFrame frame;

  if (frame.ParseFromString(std::string(multiplex_frame_bytes))) {
    if (!IsValid(frame)) {
      return ExceptionOr<MultiplexFrame>(Exception::kInvalidProtocolBuffer);
    }
    return ExceptionOr<MultiplexFrame>(std::move(frame));
  } else {
    return ExceptionOr<MultiplexFrame>(Exception::kInvalidProtocolBuffer);
  }
}

bool IsControlFrame(MultiplexFrame::MultiplexFrameType frame_type) {
  return frame_type == MultiplexFrame::CONTROL_FRAME;
}

bool IsDataFrame(MultiplexFrame::MultiplexFrameType frame_type) {
  return frame_type == MultiplexFrame::DATA_FRAME;
}

bool IsValid(const MultiplexFrame& frame) {
  switch (frame.frame_type()) {
    case MultiplexFrame::CONTROL_FRAME:
      return IsValidControlFrame(frame);
    case MultiplexFrame::DATA_FRAME:
      return IsValidDataFrame(frame);
    default:
      return false;
  }
}

bool IsValidControlFrame(const MultiplexFrame& frame) {
    if (!frame.has_control_frame()) {
        return false;
    }

    switch (frame.control_frame().control_frame_type()) {
        case MultiplexControlFrame::CONNECTION_REQUEST:
        case MultiplexControlFrame::CONNECTION_RESPONSE:
        case MultiplexControlFrame::DISCONNECTION:
          if (frame.header().salted_service_id_hash().size() ==
              kServiceIdHashLength) {
            return true;
          }
          break;
        default:
            break;
    }

    return false;
}

bool IsValidDataFrame(const MultiplexFrame& frame) {
  return frame.has_data_frame() &&
         frame.header().salted_service_id_hash().size() == kServiceIdHashLength;
}

bool IsMultiplexFrame(const ByteArray& data) {
  ExceptionOr<MultiplexFrame> frame = FromBytes(data);
  if (!frame.ok()) {
    return false;
  } else {
    NEARBY_LOGS(INFO) << "Checked data is a multiplex frame. Is Control ? "
                      << frame.result().has_control_frame() << ", is data ? "
                      << frame.result().has_data_frame();
    return true;
  }
}

}  // namespace multiplex
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
