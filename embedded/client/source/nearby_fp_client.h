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

#ifndef NEARBY_FP_CLIENT_H
#define NEARBY_FP_CLIENT_H

// clang-format off
#include "nearby_config.h"
// clang-format on

#include "nearby.h"
#include "nearby_event.h"
#include "nearby_message_stream.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct {
  // Optional event callback
  void (*on_event)(nearby_event_Event* event);
} nearby_fp_client_Callbacks;

// Seeker information structure
// Returns current information on a given seeker.
typedef struct {
  uint64_t peer_address;   // address of peer/seeker
  uint8_t capabilities;    // Capability bits
                           // Bit 0: Companion app is installed/not installed.
                           // Bit 1: Silence mode is supported/not supported.
  uint8_t platform_type;   // platform type code (Android = 0x01)
  uint8_t platform_build;  // Platform build number (Android Pie=0x1c)
} nearby_fp_client_SeekerInfo;

// No advertisement, default state
#define NEARBY_FP_ADVERTISEMENT_NONE 0x00
// The device is discoverable and can be paired with
#define NEARBY_FP_ADVERTISEMENT_DISCOVERABLE 0x01
// The device is not discoverable
#define NEARBY_FP_ADVERTISEMENT_NON_DISCOVERABLE 0x02
// Ask the Seeker to show pairing UI indication. This flag can be combined with
// NEARBY_FP_ADVERTISEMENT_NON_DISCOVERABLE
#define NEARBY_FP_ADVERTISEMENT_PAIRING_UI_INDICATOR 0x04
#if NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
// Include battery and charging info in the advertisement. This flag can be
// combined with NEARBY_FP_ADVERTISEMENT_NON_DISCOVERABLE
#define NEARBY_FP_ADVERTISEMENT_INCLUDE_BATTERY_INFO 0x08
// Ask the Seeker to show Battery UI indication. This flag can be combined with
// NEARBY_FP_ADVERTISEMENT_INCLUDE_BATTERY_INFO
#define NEARBY_FP_ADVERTISEMENT_BATTERY_UI_INDICATOR 0x10
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */
#ifdef NEARBY_FP_ENABLE_SASS
// Include SASS advertisement. This flag can be
// combined with NEARBY_FP_ADVERTISEMENT_NON_DISCOVERABLE
#define NEARBY_FP_ADVERTISEMENT_SASS 0x20
#endif /* NEARBY_FP_ENABLE_SASS */

// Sets Fast Pair advertisement type
nearby_platform_status nearby_fp_client_SetAdvertisement(int mode);

// Initalizes Fast Pair provider. The |callbacks| are optional - can be NULL.
nearby_platform_status nearby_fp_client_Init(
    const nearby_fp_client_Callbacks* callbacks);

#if NEARBY_FP_MESSAGE_STREAM
// Serializes and sends |message| over Message Stream
nearby_platform_status nearby_fp_client_SendMessage(
    uint64_t peer_address, const nearby_message_stream_Message* message);

// Sends out an ACK message for a received |message|. Note that not all messages
// require an ACK
nearby_platform_status nearby_fp_client_SendAck(
    const nearby_event_MessageStreamReceived* message);

// Sends out a NACK message for a received |message| with |fail_reason| reason.
nearby_platform_status nearby_fp_client_SendNack(
    const nearby_event_MessageStreamReceived* message, uint8_t fail_reason);

// Sends out a enable or disable silence mode, which stops audio from being
// routed to the provider.
nearby_platform_status nearby_fp_client_SetSilenceMode(uint64_t peer_address,
                                                       bool enable);

// Sends log buffer full message to seeker, which passes it on to the companion
// app to manage log collection.
nearby_platform_status nearby_fp_client_SignalLogBufferFull(
    uint64_t peer_address);

// Gets information on currently connected seekers.
// An array of `SeekerInfo` structures is passed in, along with the length.
// On output, `seeker_info_length` is the actual number of structures read.
// Returns an error if input array is too small to hold information about
// all connected seekers.
nearby_platform_status nearby_fp_client_GetSeekerInfo(
    nearby_fp_client_SeekerInfo* seeker_info, size_t* seeker_info_length);

#endif /* NEARBY_FP_MESSAGE_STREAM */

#ifdef __cplusplus
}
#endif

#endif /* NEARBY_FP_CLIENT_H */
