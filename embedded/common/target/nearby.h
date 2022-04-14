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

#ifndef NEARBY_H
#define NEARBY_H

#include "nearby_types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define AES_MESSAGE_SIZE_BYTES 16
#define FP_SERVICE_UUID_SIZE 2
#define FP_MODEL_ID_SIZE 3
#define ACCOUNT_KEY_SIZE_BYTES AES_MESSAGE_SIZE_BYTES

typedef enum {
  kNearbyStatusOK = 0,
  kNearbyStatusError,
  kNearbyStatusTimeout,
  kNearbyStatusResourceExhausted,
  kNearbyStatusUnimplemented,
  kNearbyStatusUnsupported,
  kNearbyStatusInvalidInput,
  kNearbyStatusRedundantAction
} nearby_platform_status;

#ifdef __cplusplus
}
#endif

#endif /* NEARBY_H */
