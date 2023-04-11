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

NpLdtEncryptHandle NpLdtEncryptCreate(NpLdtKeySeed key_seed) {
  NpLdtEncryptHandle handle = {0};
  return handle;
}

NpLdtDecryptHandle NpLdtDecryptCreate(NpLdtKeySeed key_seed, NpMetadataKeyHmac hmac_tag) {
  NpLdtDecryptHandle handle = {0};
  return handle;
}

NP_LDT_RESULT NpLdtEncryptClose(NpLdtEncryptHandle handle) { return NP_LDT_SUCCESS; }

NP_LDT_RESULT NpLdtDecryptClose(NpLdtDecryptHandle handle) { return NP_LDT_SUCCESS; }

NP_LDT_RESULT NpLdtEncrypt(NpLdtEncryptHandle handle, uint8_t* buffer,
                           size_t buffer_len, NpLdtSalt salt) {
  return NP_LDT_SUCCESS;
}

NP_LDT_RESULT NpLdtDecryptAndVerify(NpLdtDecryptHandle handle, uint8_t* buffer,
                                    size_t buffer_len, NpLdtSalt salt) {
  return NP_LDT_SUCCESS;
}
