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
  on app is installed/not installed.
                           // Bit 1: Silence mode is supported/not supported.
  uint8_t platform_type;   // platform type code (Android = 0x01)
  uint8_t platform_build;  // Platform build number (Android Pie=0x1c)
} nearby_fp_client_SeekerInfo;


// Include SASS advertisement. This flag can be
// combined with NEARBY_FP_ADVERTISEMENT_NON_DISCOVERABLE
#define NEARBY_FP_ADVERTISEMENT_SASS 0x20
#endif /* NEARBY_FP_ENABLE_SASS */


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
