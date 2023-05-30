// Copyright 2023 Google LLC
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
#include "fastpair/message_stream/fake_provider.h"

#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "fastpair/common/constant.h"
#include <openssl/base.h>
#include <openssl/bn.h>
#include <openssl/ec_key.h>

namespace nearby {
namespace fastpair {

namespace {

static EC_POINT *load_public_key(absl::string_view public_key) {
  CHECK_EQ(public_key.size(), kPublicKeyByteSize);
  BN_CTX *bn_ctx;
  EC_KEY *key;
  EC_POINT *point;
  const EC_GROUP *group;
  uint8_t oct_key[kPublicKeyByteSize + 1];

  oct_key[0] = 0x04;
  memcpy(oct_key + 1, public_key.data(), 64);
  bn_ctx = BN_CTX_new();
  key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  group = EC_KEY_get0_group(key);
  point = EC_POINT_new(group);
  CHECK_EQ(EC_POINT_oct2point(group, point, oct_key, sizeof(oct_key), bn_ctx),
           1);
  BN_CTX_free(bn_ctx);
  EC_KEY_free(key);
  return point;
}

static BIGNUM *load_private_key(absl::string_view private_key) {
  CHECK_EQ(private_key.length(), 32);
  uint8_t buffer[37];
  buffer[0] = buffer[1] = buffer[2] = 0;
  buffer[3] = 33;
  buffer[4] = 0;
  memcpy(buffer + 5, private_key.data(), 32);
  return BN_mpi2bn(buffer, sizeof(buffer), nullptr);
}

std::string Aes128Decrypt(absl::string_view input, absl::string_view key) {
  CHECK_EQ(input.size(), kAesBlockByteSize);
  CHECK_EQ(key.size(), kSharedSecretKeyByteSize);
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  int input_length = kAesBlockByteSize;
  int output_length = kAesBlockByteSize;
  std::string output(output_length, 0);

  CHECK_EQ(
      EVP_DecryptInit(ctx, EVP_aes_128_ecb(),
                      reinterpret_cast<const uint8_t *>(key.data()), nullptr),
      1);
  CHECK_EQ(EVP_CIPHER_CTX_set_padding(ctx, 0), 1);
  CHECK_EQ(EVP_EncryptUpdate(
               ctx, reinterpret_cast<uint8_t *>(output.data()), &output_length,
               reinterpret_cast<const uint8_t *>(input.data()), input_length),
           1);

  EVP_CIPHER_CTX_free(ctx);
  return output;
}

std::string Aes128Encrypt(absl::string_view input, absl::string_view key) {
  CHECK_EQ(input.size(), kAesBlockByteSize);
  CHECK_EQ(key.size(), kSharedSecretKeyByteSize);
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  int input_length = kAesBlockByteSize;
  int output_length = kAesBlockByteSize;
  std::string output(output_length, 0);

  CHECK_EQ(
      EVP_EncryptInit(ctx, EVP_aes_128_ecb(),
                      reinterpret_cast<const uint8_t *>(key.data()), nullptr),
      1);
  CHECK_EQ(EVP_CIPHER_CTX_set_padding(ctx, 0), 1);
  CHECK_EQ(EVP_DecryptUpdate(
               ctx, reinterpret_cast<uint8_t *>(output.data()), &output_length,
               reinterpret_cast<const uint8_t *>(input.data()), input_length),
           1);

  EVP_CIPHER_CTX_free(ctx);
  return output;
}

}  // namespace

void FakeProvider::LoadAntiSpoofingKey(absl::string_view private_key,
                                       absl::string_view public_key) {
  EC_KEY *key;
  BIGNUM *prv;
  EC_POINT *pub;

  prv = load_private_key(private_key);
  CHECK_NE(prv, nullptr);
  pub = load_public_key(public_key);
  CHECK_NE(pub, nullptr);

  key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);

  CHECK_EQ(EC_KEY_set_private_key(key, prv), 1);
  CHECK_EQ(EC_KEY_set_public_key(key, pub), 1);

  anti_spoofing_key_.reset(EVP_PKEY_new());
  CHECK_EQ(EVP_PKEY_assign_EC_KEY(anti_spoofing_key_.get(), key), 1);

  BN_free(prv);
  EC_POINT_free(pub);
}

std::string FakeProvider::DecryptKbpRequest(absl::string_view request) {
  NEARBY_LOGS(INFO) << "Encrypted KBP request "
                    << absl::BytesToHexString(request);
  CHECK_EQ(request.size(), kEncryptedDataByteSize + kPublicKeyByteSize);
  absl::string_view encrypted = request.substr(0, kEncryptedDataByteSize);
  absl::string_view remote_public_key =
      request.substr(kEncryptedDataByteSize, kPublicKeyByteSize);
  std::string shared_secret = CreateSharedSecret(remote_public_key);
  std::string decrypted = Aes128Decrypt(encrypted, shared_secret);
  NEARBY_LOGS(INFO) << "Decrypted KBP request "
                    << absl::BytesToHexString(decrypted);
  account_key_ = shared_secret;
  return decrypted;
}

std::string FakeProvider::Encrypt(absl::string_view data) {
  return Aes128Encrypt(data, account_key_);
}

std::string FakeProvider::GenSec256r1Secret(
    absl::string_view remote_party_public_key) {
  EVP_PKEY_CTX *ctx;
  EVP_PKEY *peerkey;
  EC_POINT *peer_point;
  EC_KEY *ec_peer_key;
  size_t secret_len;
  std::string secret(32, 0);

  /* Create the context for the shared secret derivation */
  ctx = EVP_PKEY_CTX_new(anti_spoofing_key_.get(), nullptr);
  CHECK_NE(ctx, nullptr);

  /* Initialise */
  CHECK_EQ(EVP_PKEY_derive_init(ctx), 1);

  peer_point = load_public_key(remote_party_public_key);
  CHECK_NE(peer_point, nullptr);

  ec_peer_key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);

  CHECK_EQ(EC_KEY_set_public_key(ec_peer_key, peer_point), 1);

  peerkey = EVP_PKEY_new();
  CHECK_EQ(EVP_PKEY_assign_EC_KEY(peerkey, ec_peer_key), 1);

  /* Provide the peer public key */
  CHECK_EQ(EVP_PKEY_derive_set_peer(ctx, peerkey), 1);

  /* Determine buffer length for shared secret */
  CHECK_EQ(EVP_PKEY_derive(ctx, nullptr, &secret_len), 1);

  CHECK_EQ(secret_len, secret.size());

  /* Derive the shared secret */
  CHECK_EQ(EVP_PKEY_derive(ctx, reinterpret_cast<uint8_t *>(secret.data()),
                           &secret_len),
           1);

  EVP_PKEY_CTX_free(ctx);
  EC_POINT_free(peer_point);
  EVP_PKEY_free(peerkey);
  return secret;
}

std::string FakeProvider::CreateSharedSecret(
    absl::string_view remote_public_key) {
  std::string secret = GenSec256r1Secret(remote_public_key);
  return std::string(
      Crypto::Sha256(secret).AsStringView().substr(0, kAccountKeySize));
}

void FakeProvider::StartGattServer(
    BleV2Medium::ServerGattConnectionCallback callback) {
  gatt_server_ = ble_.StartGattServer(std::move(callback));
}

absl::Status FakeProvider::NotifyKeyBasedPairing(ByteArray response) {
  CHECK_NE(gatt_server_, nullptr);
  CHECK(key_based_characteristic_.has_value());
  return gatt_server_->NotifyCharacteristicChanged(*key_based_characteristic_,
                                                   false, response);
}

void FakeProvider::StartDiscoverableAdvertisement(absl::string_view model_id) {
  advertising_ = true;
  ble_v1_.StartAdvertising(std::string(kServiceID),
                           ByteArray(absl::HexStringToBytes(model_id)),
                           std::string(kFastPairServiceUuid));
}

void FakeProvider::StopAdvertising() {
  if (advertising_) {
    advertising_ = false;
    ble_v1_.StopAdvertising(std::string(kServiceID));
  }
}

}  // namespace fastpair
}  // namespace nearby
