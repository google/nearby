/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SECUREMESSAGE_CRYPTO_OPS_H_
#define SECUREMESSAGE_CRYPTO_OPS_H_

#include <cstdint>
#include <memory>
#include <string>

#include "securemessage/byte_buffer.h"
#include "securemessage/common.h"

namespace securemessage {

//
// Encapsulates the cryptographic operations used by the {@code SecureMessage*}
// classes.
//
class CryptoOps {
 public:
  //
  // Enum of supported signature schemes.
  //
  enum SigType {
    HMAC_SHA256,
    ECDSA_P256_SHA256,
    RSA2048_SHA256,
    SIG_TYPE_END  // used for testing
  };

  //
  // Enum of supported encryption types
  //
  enum EncType {
    NONE,
    AES_256_CBC,
    ENC_TYPE_END  // used for testing
  };

  //
  // Key Algorithms we support
  //
  enum KeyAlgorithm {
    AES_256_KEY,
    ECDSA_KEY,
    RSA_KEY
  };

  //
  // Key Types we believe to exist
  //
  enum KeyType {
    PUBLIC,
    PRIVATE,
    SECRET
  };

  //
  // Represents a base key class, SecretKey and PublicKey are derived from this
  // class
  //
  class Key {
   public:
    //
    // The default constructor is deleted so that class consumers cannot create
    // a generic key class.  They must use a particular key type class.
    //
    Key() = delete;

    ByteBuffer data() { return data_; }
    const ByteBuffer& data() const { return data_; }
    KeyAlgorithm algorithm() const { return algorithm_; }
    KeyType type() const { return type_; }

   protected:
    //
    // Constructor cannot be called so that consumers can't simply create a
    // generic key.  Instead, they must use a particular key type class.
    //
    Key(const ByteBuffer& data, KeyAlgorithm algorithm, KeyType type) {
      data_ = data;
      algorithm_ = algorithm;
      type_ = type;
    }

    ByteBuffer data_;
    KeyAlgorithm algorithm_;
    KeyType type_;
  };

  //
  // We have three key types: Public, Private, and Secret (symmetric)
  //
  class PublicKey : public Key {
   public:
    PublicKey(const string& data, KeyAlgorithm algorithm)
        : Key(ByteBuffer(data), algorithm, PUBLIC) {}
    PublicKey(const ByteBuffer& data, KeyAlgorithm algorithm)
        : Key(data, algorithm, PUBLIC) {}
  };

  class PrivateKey : public Key {
   public:
    PrivateKey(const string& data, KeyAlgorithm algorithm)
        : Key(ByteBuffer(data), algorithm, PRIVATE) {}
    PrivateKey(const ByteBuffer& data, KeyAlgorithm algorithm)
        : Key(data, algorithm, PRIVATE) {}
  };

  class SecretKey : public Key {
   public:
    SecretKey(const string& data, KeyAlgorithm algorithm)
        : Key(ByteBuffer(data), algorithm, SECRET) {}
    SecretKey(const ByteBuffer& data, KeyAlgorithm algorithm)
        : Key(data, algorithm, SECRET) {}
  };

  struct KeyPair {
    std::unique_ptr<PrivateKey> private_key;
    std::unique_ptr<PublicKey> public_key;

    KeyPair(std::unique_ptr<PublicKey> pub,
            std::unique_ptr<PrivateKey> priv) {
      public_key = std::move(pub);
      private_key = std::move(priv);
    }
  };

  //
  // Implements HKDF (RFC 5869) with the SHA-256 hash and a 256-bit output key
  // length.
  //
  // @param inputKeyMaterial master key from which to derive sub-keys
  // @param salt a (public) randomly generated 256-bit input that can be re-used
  // @param info arbitrary information that is bound to the derived key (i.e.,
  // used in its creation)
  // @return a std::unique_ptr<string> holding the derived key bytes =
  // HKDF-SHA256(inputKeyMaterial, salt, info) on success or nullptr on error
  //
  static std::unique_ptr<string> Hkdf(const string& inputKeyMaterial,
                                      const string& salt,
                                      const string& info);

  //
  // A key derivation function specific to this library, which accepts a {@code
  // masterKey} and an arbitrary {@code purpose} describing the intended
  // application of the derived sub-key, and produces a derived AES - 256 key
  // safe to use as if it were independent of any other derived key which used
  // a different {@code purpose}.
  //
  // @param masterKey any key suitable for use with HmacSHA256
  // @param purpose a UTF-8 encoded string describing the intended purpose of
  // derived key
  // @return a derived Key suitable for use with AES-256
  //
  static std::unique_ptr<SecretKey> DeriveAes256KeyFor(
      const SecretKey& masterKey, const string& purpose);

  //
  // SHA 256 length, in bytes
  //
  static const unsigned int kSha256DigestSize = 32;

  //
  // Salt length, in bytes
  //
  static const unsigned int kSaltSize = 32;

  // AES 256 key length, in bytes
  static const unsigned int kAesKeySize = 32;

  //
  // Truncated hash output length, in bytes.
  //
  static const unsigned int kDigestLength = 20;

  //
  // Computes a collision-resistant hash of {@link #kDigestLength} bytes
  // (using a truncated SHA-256 output).
  //
  // @return a std::unique_ptr holding the digest or a nullptr on error
  //
  static std::unique_ptr<string> Digest(const string& data);

  //
  // Decrypts {@code ciphertext} using the algorithm specified in {@code
  // encType}, with the specified {@code iv} and {@code decryptionKey}.
  //
  // @return a std::unique_ptr holding the decrypted data or a nullptr on error
  //
  static std::unique_ptr<string> Decrypt(const SecretKey& decryptionKey,
                                         EncType encType,
                                         const string& iv,
                                         const string& ciphertext);

  //
  // Encrypts {@code plaintext} using the algorithm specified in {@code
  // encType}, with the specified * {@code iv} and {@code encryptionKey}.
  //
  // @param rng source of randomness to be used with the specified cipher, if
  // necessary
  // @return a std::unique_ptr holding the encrypted data or nullptr on error
  static std::unique_ptr<string> Encrypt(const SecretKey& encryptionKey,
                                         EncType encType,
                                         const string& iv,
                                         const string& plaintext);

  //
  // Runs the Diffie-Hellman algorithm.
  //
  // @param private_key must have the same algorithm as the peer_key
  // @param peer_key must be the matching public key
  // @return an AES secret key created from the SHA-256 of the secret created
  // here or nullptr on error.
  //
  static std::unique_ptr<SecretKey> KeyAgreementSha256(
      const PrivateKey& private_key,
      const PublicKey& peer_key);

  //
  // Generate a random IV appropriate for use with the algorithm specified in
  // {@code encType}.
  //
  // @return a freshly generated IV (a random byte sequence of appropriate
  // length)
  //
  static std::unique_ptr<string> GenerateIv(EncType encType);

  //
  // Signs {@code data} using the algorithm specified by {@code sigType} with
  // {@code signingKey}.
  //
  // @param rng is required for public key signature schemes
  // @return a std::unique_ptr holding a string that represents the raw
  // signature or a nullptr on error
  //
  static std::unique_ptr<string> Sign(SigType sigType,
                                      const Key& signingKey,
                                      const string& data);

  //
  // Verifies the {@code signature} on {@code data} using the algorithm
  // specified by {@code sigType} with {@code verificationKey}.
  //
  // @return true iff the signature is verified
  //
  static bool Verify(SigType sigType,
                     const Key& verificationKey,
                     const string& signature,
                     const string& data);

  //
  // Imports a P256 EC Public Key using the x and y coordinates of the curve.
  //
  // @return nullptr if the point is invalid
  //
  static std::unique_ptr<PublicKey> ImportEcP256Key(const string& x,
                                                    const string& y);

  //
  // Exports the x and y coordinates of a P256 EC Public Key.
  //
  // @return false iff the key cannot be exported (for example, incorrect type)
  //
  static bool ExportEcP256Key(const PublicKey& key, string *x, string *y);

  //
  // Imports a 2048 bit RSA Public Key using the modulus {@code n} and exponent
  // {@code e}.
  //
  // @return nullptr if the parameters are invalid
  //
  static std::unique_ptr<PublicKey> ImportRsa2048Key(const string& n,
                                                     int32_t e);

  //
  // Exports the modulus and exponent of a 2048 bit RSA Public Key.
  //
  // @return false iff the key cannot be exported (for example, incorrect type)
  //
  static bool ExportRsa2048Key(const PublicKey& key, string* n, int32_t* e);

  //
  // Returns true if a sigType is a public key scheme
  //
  static bool IsPublicKeyScheme(SigType sigType);

  //
  // Indicates whether a "tag" is needed next to the plaintext body inside the
  // ciphertext, to prevent the same ciphertext from being reused with someone
  // else's signature on it.
  //
  static bool TaggedPlaintextRequired(const Key& signing_key,
                                      SigType sig_type,
                                      const Key& encryption_key);

  // Generates a 256 bit AES secret key
  static std::unique_ptr<SecretKey> GenerateAes256SecretKey();

  // Generates a P256 EC key pair
  static std::unique_ptr<KeyPair> GenerateEcP256KeyPair();

  // Generates a 2048 bit RSA key pair
  static std::unique_ptr<KeyPair> GenerateRsa2048KeyPair();

  //
  // @return SHA-256(UTF-8 encoded input)
  //
  // Will return a 32 byte ByteBuffer representing the SHA256 of the input bytes
  // Will return a nullptr if an internal error occurs
  //
  static std::unique_ptr<ByteBuffer> Sha256(const ByteBuffer& message);

  //
  // @return SHA-512(UTF-8 encoded input)
  //
  // Will return a 64 byte ByteBuffer representing the SHA512 of the input bytes
  // Will return a nullptr if an internal error occurs
  //
  static std::unique_ptr<ByteBuffer> Sha512(const ByteBuffer& message);

  // Returns a ByteBuffer of |length|, which is filled with securely generated
  // random values.
  static std::unique_ptr<ByteBuffer> SecureRandom(size_t length);

 private:
  //
  // @return SHA256HMAC of specified message using provided key or nullptr on
  // error
  static std::unique_ptr<ByteBuffer> Sha256hmac(const ByteBuffer& key,
                                                const ByteBuffer& message);

  //
  // A salt value specific to this library, generated as
  // SHA-256("SecureMessage")
  //
  // We have to initialize salt in crypto_ops.cc because C++ doesn't allow
  // initialization and declaration in the same place.
  //
  static const uint8_t kSalt[kSaltSize];

  //
  // The HKDF (RFC 5869) extraction function, using the SHA-256 hash
  // function. This function is used to pre-process the inputKeyMaterial and mix
  // it with the salt, producing output suitable for use with HKDF expansion
  // function (which produces the actual derived key).
  //
  // @see #hkdfSha256Expand(ByteBuffer, ByteBuffer)
  // @return a std::unique_ptr<ByteBuffer> with HMAC-SHA256(salt,
  // inputKeyMaterial) (salt is the "key" for the HMAC) on success or a nullptr
  // on error
  //
  static std::unique_ptr<ByteBuffer> HkdfSha256Extract(
      const ByteBuffer& inputKeyMaterial, const ByteBuffer& salt);

  //
  // Special case of HKDF (RFC 5869) expansion function, using the SHA-256
  // hash function and allowing for a maximum output length of 256 bits.
  //
  // @param pseudoRandomKey should be generated by
  //        {@link #hkdfSha256Expand(ByteBuffer, ByteBuffer}
  // @param info arbitrary information the derived key should be bound to
  // @return a std::unique_ptr<ByteBuffer> holding derived key bytes =
  // HMAC-SHA256(pseudoRandomKey, info | 0x01) on success or a nullptr on
  // error
  //
  static std::unique_ptr<ByteBuffer> HkdfSha256Expand(
      const ByteBuffer& pseudoRandomKey, const ByteBuffer& info);

  //
  // Performs AES 256 CBC decryption of {@code ciphertext} with the specified
  // {@code iv} and {@code decryptionKey}.  Assumes PKCS#5 padding is used
  //
  // @return a std::unique_ptr holding the plaintext (decrypted) data or a
  // nullptr on error
  //
  static std::unique_ptr<ByteBuffer> Aes256CBCDecrypt(
      const SecretKey& decryptionKey,
      const ByteBuffer& iv,
      const ByteBuffer& ciphertext);

  //
  // Performs AES 256 CBC encryption of {@code plaintext} with the specified
  // {@code iv} and {@code encryptionKey}.  PKCS#5 is used.
  //
  // @return a std::unique_ptr holding the ciphertext (encrypted) data or a
  // nullptr on error
  //
  static std::unique_ptr<ByteBuffer> Aes256CBCEncrypt(
      const SecretKey& encryptionKey,
      const ByteBuffer& iv,
      const ByteBuffer& ciphertext);

  //
  // Get the purpose of an encryption scheme.  Basically a string representation
  // of the enum.
  //
  static string GetPurpose(EncType encType);

  //
  // Get the purpose of a signature scheme.  Basically a string representation
  // of the enum.
  //
  static string GetPurpose(SigType sigType);

  // Computes the ECDSA P256 SHA 256 MAC
  //
  // @param data the data to be signed
  // @param signingKey the ECDSA private key used for signing.
  // @return std::unique_ptr holding string that represents the MAC or nullptr
  // on error
  //
  static std::unique_ptr<ByteBuffer> EcdsaP256Sha256Sign(
      const PrivateKey& private_key,
      const ByteBuffer& data);

  // Verifies the ECDSA P256 SHA 256 signature
  //
  // @param signature to be verified
  // @param data the data over which the signature was computed
  // @param verificationKey the ECDSA public key used for verification.
  // @return bool true if signature is valid, false otherwise
  //
  static bool EcdsaP256Sha256Verify(const PublicKey& public_key,
                                    const ByteBuffer& signature,
                                    const ByteBuffer& data);

  //
  // Runs the Elliptic Curve variant of the Diffie-Hellman algorithm.
  //
  // @param private_key must have the algorithm ECDSA_KEY
  // @param peer_key must have the algorithm ECDSA_KEY
  // @return an AES secret key created from the SHA-256 of the secret created
  // by applying ECDH to the private_key and the peer_key.
  //
  static std::unique_ptr<ByteBuffer> EcdhKeyAgreement(
      const PrivateKey& private_key,
      const PublicKey& peer_key);


  // Computes the RSA 2048 SHA 256 signature
  //
  // @param data the data to be signed
  // @param signingKey the RSA Private key used to sign the data.  The key
  // should be in DER form.
  // @return std::unique_ptr holding string that represents the MAC or nullptr
  // on error
  //
  // TODO(aczeskis): refactor sign and verify to be virtual functions of key
  // objects
  //
  static std::unique_ptr<ByteBuffer> Rsa2048Sha256Sign(
      const PrivateKey& private_key, const ByteBuffer& data);

  // Verifies the RSA 2048 SHA 256 MAC
  //
  // @param signature to be verified
  // @param data the data over which the signature was computed
  // @param verificationKey the RSA public key used for verification
  // @return bool true if signature is valid, false otherwise
  //
  static bool Rsa2048Sha256Verify(const PublicKey& public_key,
                                  const ByteBuffer& signature,
                                  const ByteBuffer& data);

  static bool IsValidEcP256CoordinateEncoding(const string& bytes);

  static bool IsValidRsa2048ModulusEncoding(const string& bytes);

  //
  // Converts int32 value to an array of bytes represented as a string. The
  // bytes are in big-endian order.
  //
  static string Int32BytesToString(int32_t value);

  //
  // Converts a string of bytes to an int32 value. Fails if the bytes do not fit
  // in the int32 type. The bytes are in big-endian order.
  //
  static bool StringToInt32Bytes(const string& value, int32_t* result);

  // For testing private helper functions
  friend class CryptoOpsTest;

  virtual ~CryptoOps();
};

}  // namespace securemessage
#endif  // SECUREMESSAGE_CRYPTO_OPS_H_
