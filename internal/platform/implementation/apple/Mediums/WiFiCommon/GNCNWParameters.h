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

/// Builds a non-TLS `nw_parameters_t` for the given parameters.
///
/// @param includePeerToPeer Whether to include peer-to-peer (P2P) parameters.
FOUNDATION_EXPORT
nw_parameters_t _Nullable GNCBuildNonTLSParameters(BOOL includePeerToPeer) NS_RETURNS_RETAINED;

/// Builds a TLS `nw_parameters_t` for the given parameters.
///
/// @param PSK The PSK to use for the TLS parameters.
/// @param identity The identity to use for the TLS parameters.
/// @param includePeerToPeer Whether to include peer-to-peer (P2P) parameters.
FOUNDATION_EXPORT
nw_parameters_t _Nullable GNCBuildTLSParameters(NSData *PSK, NSData *identity,
                                                BOOL includePeerToPeer) NS_RETURNS_RETAINED;

NS_ASSUME_NONNULL_END
