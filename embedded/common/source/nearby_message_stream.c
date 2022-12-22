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

#include "nearby_message_stream.h"

#include "nearby_platform_bt.h"

#define HEADER_SIZE 4
#define ACK_MESSAGE_SIZE 6
#define NACK_MESSAGE_SIZE 7
#define ACKNOWLEDGEMENT_GROUP 0xFF
#define ACK_CODE 1
#define NACK_CODE 2

#if NEARBY_FP_MESSAGE_STREAM
void nearby_message_stream_Init(const nearby_message_stream_State* state) {
  nearby_message_stream_Metadata* metadata =
      (nearby_message_stream_Metadata*)state->buffer;
  memset(metadata, 0, sizeof(nearby_message_stream_Metadata));
  metadata->message.data =
      state->buffer + sizeof(nearby_message_stream_Metadata);
}

void nearby_message_stream_Read(const nearby_message_stream_State* state,
                                const uint8_t* data, size_t length) {
  nearby_message_stream_Metadata* metadata =
      (nearby_message_stream_Metadata*)state->buffer;
  nearby_message_stream_Message* message = &metadata->message;
  uint16_t available_space =
      state->length - sizeof(nearby_message_stream_Metadata);
  while (length > 0) {
    switch (metadata->bytes_read) {
      case 0:
        message->message_group = *data;
        break;
      case 1:
        message->message_code = *data;
        break;
      case 2:
        message->length = ((uint16_t)*data) << 8;
        break;
      case 3:
        message->length += *data;
        break;
      default: {
        uint16_t offset = metadata->bytes_read - HEADER_SIZE;
        if (offset < message->length && offset < available_space) {
          message->data[offset] = *data;
        }
      }
    }
    data++;
    length--;
    metadata->bytes_read++;
    if (metadata->bytes_read - HEADER_SIZE == message->length) {
      if (message->length > available_space) {
        // Message truncated
        message->length = available_space;
      }
      state->on_message_received(state->peer_address, message);
      message->length = 0;
      metadata->bytes_read = 0;
    }
  }
}

nearby_platform_status nearby_message_stream_Send(
    uint64_t peer_address, const nearby_message_stream_Message* message) {
  nearby_platform_status status;
  uint8_t header[HEADER_SIZE];
  header[0] = message->message_group;
  header[1] = message->message_code;
  header[2] = message->length >> 8;
  header[3] = message->length;
  status =
      nearby_platform_SendMessageStream(peer_address, header, sizeof(header));
  if (kNearbyStatusOK == status && message->length > 0) {
    status = nearby_platform_SendMessageStream(peer_address, message->data,
                                               message->length);
  }
  return status;
}

nearby_platform_status nearby_message_stream_SendAck(
    uint64_t peer_address, const nearby_message_stream_Message* message) {
  uint8_t ack[ACK_MESSAGE_SIZE];
  ack[0] = ACKNOWLEDGEMENT_GROUP;
  ack[1] = ACK_CODE;
  ack[2] = 0;
  ack[3] = ACK_MESSAGE_SIZE - HEADER_SIZE;
  ack[4] = message->message_group;
  ack[5] = message->message_code;
  return nearby_platform_SendMessageStream(peer_address, ack, sizeof(ack));
}

nearby_platform_status nearby_message_stream_SendNack(
    uint64_t peer_address, const nearby_message_stream_Message* message,
    uint8_t fail_reason) {
  uint8_t nack[NACK_MESSAGE_SIZE];
  nack[0] = ACKNOWLEDGEMENT_GROUP;
  nack[1] = NACK_CODE;
  nack[2] = 0;
  nack[3] = NACK_MESSAGE_SIZE - HEADER_SIZE;
  nack[4] = fail_reason;
  nack[5] = message->message_group;
  nack[6] = message->message_code;
  return nearby_platform_SendMessageStream(peer_address, nack, sizeof(nack));
}

uint16_t nearby_message_stream_GetMaxPayloadSize(
    const nearby_message_stream_State* state) {
  return state->length - sizeof(nearby_message_stream_Metadata);
}

#endif /* NEARBY_FP_MESSAGE_STREAM */
