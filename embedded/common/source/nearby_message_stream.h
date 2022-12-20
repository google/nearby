// Copyright 2022 Google LLC
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

#ifndef NEARBY_MESSAGE_STREAM_H
#define NEARBY_MESSAGE_STREAM_H

// clang-format off
#include "nearby_config.h"
// clang-format on

#include "nearby.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// Message group and codes for ACK and NACK messages
#define ACKNOWLEDGEMENT_GROUP 0xFF
#define ACK_CODE 1
#define NACK_CODE 2

#define FAIL_REASON_INVALID_MAC 3
#define FAIL_REASON_REDUNDANT_DEVICE_ACTION 4

// Message sent and received over the message stream
// See: https://developers.google.com/nearby/fast-pair/spec#MessageStream
typedef struct {
  uint8_t message_group;
  uint8_t message_code;
  // |data| length in native encoding
  uint16_t length;
  // message payload in big endian encoding
  uint8_t* data;
} nearby_message_stream_Message;

// State and configuration of message stream parser
typedef struct {
  // Callback triggered when a complete message is read from the input stream
  void (*on_message_received)(uint64_t peer_address,
                              nearby_message_stream_Message* message);
  // |peer_address| that is passed to |on_message_received|
  uint64_t peer_address;
  // Length of |buffer|
  size_t length;
  // The buffer used for storing an incoming message. The parser uses the buffer
  // to store nearby_message_stream_Metadata, so not all of the space is
  // available for message payload
  uint8_t* buffer;
} nearby_message_stream_State;

typedef struct {
  nearby_message_stream_Message message;
  uint16_t bytes_read;
} nearby_message_stream_Metadata;

#if NEARBY_FP_MESSAGE_STREAM
// Initializes the parser
void nearby_message_stream_Init(const nearby_message_stream_State* state);

// Reads and deserializes data from an input stream. It is OK to pass in
// incomplete packets. When the parses reads a complete message, it calls
// |on_message_received|. If the message payload is too big to fit the buffer -
// bigger than GetMaxPayloadSize(), then the payload is truncated.
void nearby_message_stream_Read(const nearby_message_stream_State* state,
                                const uint8_t* data, size_t length);

// Serializes and sends |message| out using nearby_platform_SendMessageStream()
nearby_platform_status nearby_message_stream_Send(
    uint64_t peer_address, const nearby_message_stream_Message* message);

// Sends out an ACK message for a received |message|. Note that not all messages
// require an ACK
nearby_platform_status nearby_message_stream_SendAck(
    uint64_t peer_address, const nearby_message_stream_Message* message);

// Sends out a NACK message for a received |message| with |fail_reason| reason.
nearby_platform_status nearby_message_stream_SendNack(
    uint64_t peer_address, const nearby_message_stream_Message* message,
    uint8_t fail_reason);

// Returns the maximum message payload size
uint16_t nearby_message_stream_GetMaxPayloadSize(
    const nearby_message_stream_State* state);

#endif /* NEARBY_FP_MESSAGE_STREAM */
#ifdef __cplusplus
}
#endif

#endif  // NEARBY_MESSAGE_STREAM_H
