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

#include "presence/implementation/np_ldt.h"

// Placeholder, empty implementations of LDT utilities. They will be replaced
// with implementations in Rust.

NpLdtHandle NpLdtCreate(NpLdtAesConfig aes_config, NpLdtKeySeed key_seed,
                        NpMetadataKeyHmac known_hmac) {
  return 0;
}

NP_LDT_RESULT NpLdtClose(NpLdtHandle handle) { return NP_LDT_SUCCESS; }

NP_LDT_RESULT NpLdtEncrypt(NpLdtHandle handle, uint8_t* buffer,
                           size_t buffer_len, NpLdtSalt salt) {
  return NP_LDT_SUCCESS;
}

NP_LDT_RESULT NpLdtDecryptAndVerify(NpLdtHandle handle, uint8_t* buffer,
                                    size_t buffer_len, NpLdtSalt salt) {
  return NP_LDT_SUCCESS;
}
