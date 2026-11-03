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

#import <Foundation/Foundation.h>
#import <Network/Network.h>

NS_ASSUME_NONNULL_BEGIN

/// Role of the local endpoint in a Nearby TLS handshake.
typedef NS_ENUM(NSInteger, GNCTLSRole) {
  /// The local endpoint initiates the outbound TLS connection (`ClientHello`).
  GNCTLSRoleClient,
  /// The local endpoint accepts the inbound TLS connection (`ServerHello`).
  GNCTLSRoleServer,
};

/// Builds a non-TLS `nw_parameters_t` for the given parameters.
///
/// @param includePeerToPeer Whether to include peer-to-peer (P2P) parameters.
FOUNDATION_EXPORT
nw_parameters_t _Nullable GNCBuildNonTLSParameters(BOOL includePeerToPeer) NS_RETURNS_RETAINED;

/// Builds a TLS `nw_parameters_t` for the given parameters using the default client role.
///
/// When `GNCFeatureFlags.awdlEnabled` is YES, configures TLS 1.3 mTLS; otherwise,
/// configures legacy TLS 1.2 PSK (`TLS_PSK_WITH_AES_128_GCM_SHA256`).
///
/// @param PSK The PSK to use for the TLS parameters.
/// @param identity The identity to use for the TLS parameters.
/// @param includePeerToPeer Whether to include peer-to-peer (P2P) parameters.
FOUNDATION_EXPORT
nw_parameters_t _Nullable GNCBuildTLSParameters(NSData *PSK, NSData *identity,
                                                BOOL includePeerToPeer) NS_RETURNS_RETAINED;

/// Builds a TLS `nw_parameters_t` for the given parameters and explicit role.
///
/// When `GNCFeatureFlags.awdlEnabled` is YES, configures TLS 1.3 mTLS with ephemeral P-256 key
/// generation and HMAC-bound certificate serial verification for `GNCTLSRoleClient`, or a dynamic
/// `ClientHello`-inspecting framer on `GNCTLSRoleServer` that negotiates either TLS 1.3 mTLS or
/// legacy TLS 1.2 PSK; when NO, configures legacy TLS 1.2 PSK.
///
/// @param PSK The PSK to use for the TLS parameters.
/// @param identity The identity to use for the TLS parameters.
/// @param includePeerToPeer Whether to include peer-to-peer (P2P) parameters.
/// @param role Whether the local endpoint is the TLS client (initiator) or server (responder).
FOUNDATION_EXPORT
nw_parameters_t _Nullable GNCBuildTLSParametersForRole(NSData *PSK, NSData *identity,
                                                       BOOL includePeerToPeer,
                                                       GNCTLSRole role) NS_RETURNS_RETAINED;

/// Builds a legacy TLS 1.2 PSK (`TLS_PSK_WITH_AES_128_GCM_SHA256`) `nw_parameters_t`.
///
/// @param PSK The PSK shared secret to use for the TLS parameters.
/// @param identity The PSK identity to use for the TLS parameters.
/// @param includePeerToPeer Whether to include peer-to-peer (P2P) parameters.
FOUNDATION_EXPORT
nw_parameters_t _Nullable GNCBuildTLS12PSKParameters(NSData *PSK, NSData *identity,
                                                     BOOL includePeerToPeer) NS_RETURNS_RETAINED;

NS_ASSUME_NONNULL_END
