// Copyright 2025 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWParameters.h"

#import <CommonCrypto/CommonHMAC.h>
#import <Foundation/Foundation.h>
#import <Network/Network.h>
#import <Security/SecProtocolOptions.h>
#import <Security/Security.h>
#include <errno.h>
#include <string.h>

#include <openssl/asn1.h>
#include <openssl/bn.h>
#include <openssl/digest.h>
#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/evp.h>  // NOLINT(google3-security-unsafe-crypto-dependencies)
#include <openssl/mem.h>
#include <openssl/nid.h>
#include <openssl/x509.h>

#import "internal/platform/implementation/apple/Flags/GNCFeatureFlags.h"
#import "internal/platform/implementation/apple/Log/GNCLogger.h"

NS_ASSUME_NONNULL_BEGIN

extern SecIdentityRef _Nullable SecIdentityCreate(  // NOLINT(google-objc-function-naming)
    CFAllocatorRef _Nullable allocator, SecCertificateRef certificate, SecKeyRef privateKey);

/// Role label bound into the client's HMAC-SHA256 certificate serial number.
static NSString *const kClientKeyLabel = @"NEARBY-TLS13-CLIENT-KEY-v1";
/// Role label bound into the server's HMAC-SHA256 certificate serial number.
static NSString *const kServerKeyLabel = @"NEARBY-TLS13-SERVER-KEY-v1";
/// X.509 Common Name for ephemeral Nearby upgrade certificates.
static const char *const kCertificateCommonName = "NearbyUpgradeMedium";
/// Cryptographic and TLS wire format byte lengths and constants.
enum {
  kP256UncompressedPointLength = 65,
  kP256ScalarLength = 32,
  kP256X963PrivateKeyLength = 97,
  kP256SPKILength = 91,
  kCertificateSerialLength = 16,
  kX509Version3 = 2,
};

// 26-byte ASN.1 DER SubjectPublicKeyInfo prefix for an uncompressed P-256 public key (RFC 5480).
static const uint8_t kP256SPKIPrefix[26] = {
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,
    0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00,
};

/// Computes the 16-byte clamped `HMAC-SHA256(psk, roleLabel || spkiBytes)` certificate serial
/// number with `outSerial[0] = (outSerial[0] & 0x7F) | 0x40`.
BOOL GNCComputeExpectedSerialBytes(NSData *psk, NSString *roleLabel, const uint8_t *spkiBytes,
                                   size_t spkiLen, uint8_t outSerial[kCertificateSerialLength]) {
  if (psk.length == 0 || roleLabel.length == 0 || !spkiBytes || spkiLen != kP256SPKILength ||
      !outSerial) {
    return NO;
  }
  const char *labelUtf8 = roleLabel.UTF8String;
  if (!labelUtf8) {
    return NO;
  }
  uint8_t digest[CC_SHA256_DIGEST_LENGTH];
  CCHmacContext ctx;
  CCHmacInit(&ctx, kCCHmacAlgSHA256, psk.bytes, psk.length);
  CCHmacUpdate(&ctx, labelUtf8, strlen(labelUtf8));
  CCHmacUpdate(&ctx, spkiBytes, spkiLen);
  CCHmacFinal(&ctx, digest);
  memcpy(outSerial, digest, kCertificateSerialLength);
  OPENSSL_cleanse(digest, sizeof(digest));
  outSerial[0] = (outSerial[0] & 0x7F) | 0x40;
  return YES;
}

/// Generates an ephemeral P-256 keypair and self-signed X.509v3 certificate whose serial number
/// is bound to `psk` and `roleLabel`, returning a retained `SecIdentityRef`.
SecIdentityRef _Nullable GNCCreateSelfSignedCertificate(NSData *psk,
                                                        NSString *roleLabel) CF_RETURNS_RETAINED {
  if (psk.length == 0 || roleLabel.length == 0) {
    return NULL;
  }
  EC_KEY *ecKey = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  if (!ecKey || EC_KEY_generate_key(ecKey) != 1) {
    EC_KEY_free(ecKey);
    return NULL;
  }

  const EC_GROUP *group = EC_KEY_get0_group(ecKey);
  const EC_POINT *pubPoint = EC_KEY_get0_public_key(ecKey);
  const BIGNUM *privScalar = EC_KEY_get0_private_key(ecKey);
  uint8_t x963PrivateKey[kP256X963PrivateKeyLength];
  BOOL keyBytesOk =
      (group != NULL && pubPoint != NULL && privScalar != NULL &&
       EC_POINT_point2oct(group, pubPoint, POINT_CONVERSION_UNCOMPRESSED, x963PrivateKey,
                          kP256UncompressedPointLength, NULL) == kP256UncompressedPointLength &&
       BN_bn2bin_padded(x963PrivateKey + kP256UncompressedPointLength, kP256ScalarLength,
                        privScalar) == 1);
  if (!keyBytesOk) {
    OPENSSL_cleanse(x963PrivateKey, sizeof(x963PrivateKey));
    EC_KEY_free(ecKey);
    return NULL;
  }

  EVP_PKEY *pkey = EVP_PKEY_new();
  if (!pkey || EVP_PKEY_assign_EC_KEY(pkey, ecKey) != 1) {
    OPENSSL_cleanse(x963PrivateKey, sizeof(x963PrivateKey));
    EC_KEY_free(ecKey);
    EVP_PKEY_free(pkey);
    return NULL;
  }

  uint8_t *spkiDer = NULL;  // NOLINT(modernize-use-nullptr)
  int spkiLen = i2d_PUBKEY(pkey, &spkiDer);
  uint8_t serialBytes[kCertificateSerialLength];
  BOOL serialOk =
      (spkiLen == kP256SPKILength && spkiDer != NULL &&
       GNCComputeExpectedSerialBytes(psk, roleLabel, spkiDer, (size_t)spkiLen, serialBytes));
  OPENSSL_free(spkiDer);
  if (!serialOk) {
    OPENSSL_cleanse(x963PrivateKey, sizeof(x963PrivateKey));
    EVP_PKEY_free(pkey);
    return NULL;
  }

  X509 *x509 = X509_new();
  BIGNUM *serialBn =
      BN_bin2bn(serialBytes, sizeof(serialBytes), NULL);  // NOLINT(modernize-use-nullptr)
  if (!x509 || !serialBn) {
    BN_free(serialBn);
    X509_free(x509);
    OPENSSL_cleanse(x963PrivateKey, sizeof(x963PrivateKey));
    EVP_PKEY_free(pkey);
    return NULL;
  }

  BOOL ok = (X509_set_version(x509, kX509Version3) == 1 &&
             BN_to_ASN1_INTEGER(serialBn, X509_get_serialNumber(x509)) != NULL &&
             X509_gmtime_adj(X509_get_notBefore(x509), -86400L) != NULL &&
             X509_gmtime_adj(X509_get_notAfter(x509), 604800L) != NULL);
  BN_free(serialBn);
  X509_NAME *name = X509_get_subject_name(x509);
  ok = ok && name != NULL &&
       X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const uint8_t *)kCertificateCommonName,
                                  -1, -1, 0) == 1 &&
       X509_set_issuer_name(x509, name) == 1 && X509_set_pubkey(x509, pkey) == 1 &&
       X509_sign(x509, pkey, EVP_sha256()) > 0;

  uint8_t *certDer = NULL;  // NOLINT(modernize-use-nullptr)
  int certDerLen = ok ? i2d_X509(x509, &certDer) : -1;
  X509_free(x509);
  EVP_PKEY_free(pkey);
  if (certDerLen <= 0 || !certDer) {
    OPENSSL_free(certDer);
    OPENSSL_cleanse(x963PrivateKey, sizeof(x963PrivateKey));
    return NULL;
  }

  NSData *certData = [NSData dataWithBytes:certDer length:(NSUInteger)certDerLen];
  OPENSSL_free(certDer);
  SecCertificateRef certRef =
      SecCertificateCreateWithData(kCFAllocatorDefault, (__bridge CFDataRef)certData);
  if (!certRef) {
    OPENSSL_cleanse(x963PrivateKey, sizeof(x963PrivateKey));
    return NULL;
  }

  NSDictionary *keyAttributes = @{
    (__bridge id)kSecAttrKeyType : (__bridge id)kSecAttrKeyTypeECSECPrimeRandom,
    (__bridge id)kSecAttrKeyClass : (__bridge id)kSecAttrKeyClassPrivate,
    (__bridge id)kSecAttrKeySizeInBits : @(256),
  };
  CFDataRef keyData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, x963PrivateKey,
                                                  sizeof(x963PrivateKey), kCFAllocatorNull);
  SecKeyRef privateKeyRef =
      keyData ? SecKeyCreateWithData(keyData, (__bridge CFDictionaryRef)keyAttributes, NULL) : NULL;
  if (keyData) CFRelease(keyData);
  OPENSSL_cleanse(x963PrivateKey, sizeof(x963PrivateKey));
  if (!privateKeyRef) {
    CFRelease(certRef);
    return NULL;
  }

  SecIdentityRef identityRef = SecIdentityCreate(kCFAllocatorDefault, certRef, privateKeyRef);
  CFRelease(certRef);
  CFRelease(privateKeyRef);
  return identityRef;
}

/// Configures legacy TLS 1.2 PSK (`TLS_PSK_WITH_AES_128_GCM_SHA256`) on `options`.
BOOL GNCConfigureTLS12PSKOptions(sec_protocol_options_t _Nullable options, NSData *PSK,
                                 NSData *identity) {
  if (!options) {
    GNCLoggerError(@"[GNCNWParameters] Invalid parameter of options.");
    return NO;
  }

  if (PSK.length == 0 || identity.length == 0) {
    GNCLoggerError(@"[GNCNWParameters] Invalid parameter of psk or identity.");
    return NO;
  }

  // Use tls_protocol_version_TLSv12 as TLS protocol version, which is the only version support PSK
  // on iOS.
  sec_protocol_options_set_min_tls_protocol_version(options, tls_protocol_version_TLSv12);
  sec_protocol_options_set_max_tls_protocol_version(options, tls_protocol_version_TLSv12);

  sec_protocol_options_append_tls_ciphersuite(options, TLS_PSK_WITH_AES_128_GCM_SHA256);

  // Using DISPATCH_DATA_DESTRUCTOR_DEFAULT will cause dispatch_data_create() to explicitly make a
  // copy of the data, and delete the copy when the dispatch_data_t is deallocated.
  dispatch_data_t psk_secret_dispatch_data =
      dispatch_data_create([PSK bytes], [PSK length], nil, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

  dispatch_data_t psk_identity_dispatch_data = dispatch_data_create(
      [identity bytes], [identity length], nil, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

  if (!psk_secret_dispatch_data || !psk_identity_dispatch_data) {
    GNCLoggerError(@"[GNCNWParameters] Failed to create dispatch_data_t for PSK.");
    return NO;
  }

  // Add the Pre-Shared Key and its identity to the TLS options
  sec_protocol_options_add_pre_shared_key(options, psk_secret_dispatch_data,
                                          psk_identity_dispatch_data);

  GNCLoggerInfo(@"[GNCNWParameters] Successfully configured TLS 1.2 PSK options.");
  return YES;
}

/// Verifies that `trustRef` contains a single self-signed P-256 leaf certificate whose serial
/// number matches the expected HMAC-SHA256 over `expectedPeerRoleLabel` and the peer SPKI.
BOOL GNCVerifyPeerTrust(sec_trust_t trustRef, NSData *psk, NSString *expectedPeerRoleLabel) {
  SecTrustRef trust = sec_trust_copy_ref(trustRef);
  if (!trust) {
    return NO;
  }
  BOOL verified = NO;
  SecCertificateRef leafCert = NULL;
  CFArrayRef chain = NULL;
  if (SecTrustGetCertificateCount(trust) == 1) {
    if (@available(macOS 12.0, iOS 15.0, watchOS 8.0, tvOS 15.0, *)) {
      chain = SecTrustCopyCertificateChain(trust);
      if (chain && CFArrayGetCount(chain) == 1) {
        leafCert = (SecCertificateRef)CFArrayGetValueAtIndex(chain, 0);
      }
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
      leafCert = SecTrustGetCertificateAtIndex(trust, 0);
#pragma clang diagnostic pop
    }
  }
  SecPolicyRef basicPolicy = leafCert ? SecPolicyCreateBasicX509() : NULL;
  if (basicPolicy) {
    if (SecTrustSetPolicies(trust, basicPolicy) == errSecSuccess &&
        SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef) @[ (__bridge id)leafCert ]) ==
            errSecSuccess &&
        SecTrustSetAnchorCertificatesOnly(trust, true) == errSecSuccess &&
        SecTrustEvaluateWithError(trust, NULL)) {
      SecKeyRef peerKey = SecCertificateCopyKey(leafCert);
      CFDataRef peerKeyData = peerKey ? SecKeyCopyExternalRepresentation(peerKey, NULL) : NULL;
      CFDataRef actualSerialData = SecCertificateCopySerialNumberData(leafCert, NULL);
      if (peerKeyData && actualSerialData &&
          CFDataGetLength(peerKeyData) == (CFIndex)kP256UncompressedPointLength &&
          CFDataGetBytePtr(peerKeyData)[0] == 0x04 &&
          CFDataGetLength(actualSerialData) == (CFIndex)kCertificateSerialLength) {
        uint8_t spkiBytes[kP256SPKILength];
        memcpy(spkiBytes, kP256SPKIPrefix, sizeof(kP256SPKIPrefix));
        memcpy(spkiBytes + sizeof(kP256SPKIPrefix), CFDataGetBytePtr(peerKeyData),
               kP256UncompressedPointLength);
        uint8_t expectedSerial[kCertificateSerialLength];
        if (GNCComputeExpectedSerialBytes(psk, expectedPeerRoleLabel, spkiBytes, sizeof(spkiBytes),
                                          expectedSerial)) {
          verified = (timingsafe_bcmp(CFDataGetBytePtr(actualSerialData), expectedSerial,
                                      kCertificateSerialLength) == 0);
        }
      }
      if (actualSerialData) CFRelease(actualSerialData);
      if (peerKeyData) CFRelease(peerKeyData);
      if (peerKey) CFRelease(peerKey);
    }
    CFRelease(basicPolicy);
  }
  if (chain) CFRelease(chain);
  CFRelease(trust);
  return verified;
}

/// Configures TLS 1.3 mutual authentication on `options` using an ephemeral self-signed P-256
/// identity and HMAC-SHA256 peer certificate serial verification for `role`.
BOOL GNCConfigureTLS13MutualAuthOptions(sec_protocol_options_t _Nullable options, NSData *PSK,
                                        NSData *identity, GNCTLSRole role) {
  if (!options) {
    GNCLoggerError(@"[GNCNWParameters] Invalid parameter of options.");
    return NO;
  }

  if (PSK.length == 0 || identity.length == 0) {
    GNCLoggerError(@"[GNCNWParameters] Invalid parameter of psk or identity.");
    return NO;
  }

  NSString *localRoleLabel = (role == GNCTLSRoleClient) ? kClientKeyLabel : kServerKeyLabel;
  NSString *expectedPeerRoleLabel = (role == GNCTLSRoleClient) ? kServerKeyLabel : kClientKeyLabel;
  SecIdentityRef identityRef = GNCCreateSelfSignedCertificate(PSK, localRoleLabel);
  if (!identityRef) {
    GNCLoggerError(@"[GNCNWParameters] Failed to create in-memory SecIdentityRef.");
    return NO;
  }

  sec_identity_t secIdentity = sec_identity_create(identityRef);
  CFRelease(identityRef);
  if (!secIdentity) {
    GNCLoggerError(@"[GNCNWParameters] Failed to create sec_identity_t.");
    return NO;
  }

  sec_protocol_options_set_local_identity(options, secIdentity);
  sec_protocol_options_set_min_tls_protocol_version(options, tls_protocol_version_TLSv13);
  sec_protocol_options_set_max_tls_protocol_version(options, tls_protocol_version_TLSv13);
  sec_protocol_options_set_peer_authentication_required(options, true);

  NSData *pskCopy = [PSK copy];
  dispatch_queue_t verifyQueue =
      dispatch_queue_create("com.google.nearby.awdl.tls.verify", DISPATCH_QUEUE_SERIAL);
  sec_protocol_options_set_verify_block(
      options,
      ^(sec_protocol_metadata_t metadata, sec_trust_t trust_ref,
        sec_protocol_verify_complete_t complete) {
        complete(GNCVerifyPeerTrust(trust_ref, pskCopy, expectedPeerRoleLabel));
      },
      verifyQueue);

  GNCLoggerInfo(@"[GNCNWParameters] Successfully configured TLS 1.3 mTLS options.");
  return YES;
}

nw_parameters_t _Nullable GNCBuildNonTLSParameters(BOOL includePeerToPeer) {
  nw_parameters_t parameters =
      nw_parameters_create_secure_tcp(/*tls*/ NW_PARAMETERS_DISABLE_PROTOCOL,
                                      /*tcp*/ NW_PARAMETERS_DEFAULT_CONFIGURATION);
  nw_parameters_set_include_peer_to_peer(parameters, includePeerToPeer);
  return parameters;
}

nw_parameters_t _Nullable GNCBuildTLS12PSKParameters(NSData *PSK, NSData *identity,
                                                     BOOL includePeerToPeer) {
  __block BOOL TLSWasConfigured = NO;
  nw_parameters_t parameters = nw_parameters_create_secure_tcp(
      ^(nw_protocol_options_t default_tls_options) {
        sec_protocol_options_t sec_protocol_options =
            nw_tls_copy_sec_protocol_options(default_tls_options);
        TLSWasConfigured = GNCConfigureTLS12PSKOptions(sec_protocol_options, PSK, identity);
      },
      /*tcp*/ NW_PARAMETERS_DEFAULT_CONFIGURATION);
  if (!TLSWasConfigured) {
    GNCLoggerError(@"[GNCNWParameters] Failed to configure TLS 1.2 PSK options.");
    return nil;
  }
  nw_parameters_set_include_peer_to_peer(parameters, includePeerToPeer);
  return parameters;
}

nw_parameters_t _Nullable GNCBuildTLSParameters(NSData *PSK, NSData *identity,
                                                BOOL includePeerToPeer) {
  return GNCBuildTLSParametersForRole(PSK, identity, includePeerToPeer, GNCTLSRoleClient);
}

nw_parameters_t _Nullable GNCBuildTLSParametersForRole(NSData *PSK, NSData *identity,
                                                       BOOL includePeerToPeer, GNCTLSRole role) {
  if (!GNCFeatureFlags.awdlEnabled) {
    return GNCBuildTLS12PSKParameters(PSK, identity, includePeerToPeer);
  }

  __block BOOL TLSWasConfigured = NO;
  nw_parameters_t parameters = nw_parameters_create_secure_tcp(
      ^(nw_protocol_options_t default_tls_options) {
        sec_protocol_options_t sec_protocol_options =
            nw_tls_copy_sec_protocol_options(default_tls_options);
        TLSWasConfigured =
            GNCConfigureTLS13MutualAuthOptions(sec_protocol_options, PSK, identity, role);
      },
      /*tcp*/ NW_PARAMETERS_DEFAULT_CONFIGURATION);
  if (!TLSWasConfigured) {
    GNCLoggerError(@"[GNCNWParameters] Failed to configure TLS 1.3 mTLS options.");
    return nil;
  }

  nw_parameters_set_include_peer_to_peer(parameters, includePeerToPeer);
  return parameters;
}

NS_ASSUME_NONNULL_END
