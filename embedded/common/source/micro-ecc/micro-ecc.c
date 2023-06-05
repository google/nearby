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
#include <nearby.h>
#include <uECC.h>
#include <uECC_vli.h>

#include "nearby_fp_library.h"
#include "nearby_platform_se.h"
#include "nearby_trace.h"

nearby_platform_status nearby_platform_GetSecp160r1PublicKey(
    const uint8_t private_key[32], uint8_t output[20],
    uint8_t *hashed_flag_field) {
  uECC_Curve curve = uECC_secp160r1();
  uint8_t public_key[40];
  uint8_t priv[48];

  memset(priv, 0, 48);
  memset(public_key, 0, 40);
  memset(output, 0, 20);
  for (int i = 0; i < 32; i++) {
    priv[i] = private_key[31 - i];
  }

  uECC_vli_mmod((uECC_word_t *)priv, (uECC_word_t *)priv, uECC_curve_n(curve),
                uECC_curve_num_n_words(curve));

  // r should be aligned to the curve’s size: zeros should be added as most
  // significant bits if its representation is shorter than 160/256 bits, or the
  // most significant bits should be truncated if its representation is larger
  // than 160/256 bits.
  // make sure big endian format is passed as SHA256 input
  for (int i = 0; i < 20; i++) {
    output[i] = priv[19 - i];
  }

  if (nearby_fp_Sha256(public_key, output, 20) != kNearbyStatusOK) {
    NEARBY_TRACE(ERROR, "failed to generate hashed field");
    *hashed_flag_field = 0;
  } else {
    // the least significant byte of SHA256(r)
    *hashed_flag_field = public_key[31];
  }

  memset(public_key, 0, 40);
  int ret = uECC_compute_public_key(priv, public_key, curve);

  memset(output, 0, 20);
  if (ret == 1) {
    NEARBY_TRACE(INFO, "public key generated");
    for (int i = 0; i < 20; i++) {
      output[i] = public_key[19 - i];
    }
    return kNearbyStatusOK;
  }
  NEARBY_TRACE(ERROR, "failed to generate public key: %d", ret);
  return kNearbyStatusError;
}
