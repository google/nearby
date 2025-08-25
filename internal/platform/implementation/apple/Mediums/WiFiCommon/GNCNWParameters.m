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

#import <Foundation/Foundation.h>
#import <Network/Network.h>
#import <Security/SecProtocolOptions.h>

#import "internal/platform/implementation/apple/Log/GNCLogger.h"

NS_ASSUME_NONNULL_BEGIN

BOOL GNCConfigureTLSOptions(sec_protocol_options_t _Nullable options, NSData *PSK,
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

  GNCLoggerInfo(@"[GNCNWParameters] Successfully configured TLS options.");
  return YES;
}

nw_parameters_t _Nullable GNCBuildNonTLSParameters(BOOL includePeerToPeer) {
  nw_parameters_t parameters =
      nw_parameters_create_secure_tcp(/*tls*/ NW_PARAMETERS_DISABLE_PROTOCOL,
                                      /*tcp*/ NW_PARAMETERS_DEFAULT_CONFIGURATION);
  nw_parameters_set_include_peer_to_peer(parameters, includePeerToPeer);
  return parameters;
}

nw_parameters_t _Nullable GNCBuildTLSParameters(NSData *PSK, NSData *identity,
                                                BOOL includePeerToPeer) {
  __block BOOL TLSWasConfigured = NO;
  nw_parameters_t parameters = nw_parameters_create_secure_tcp(
      ^(nw_protocol_options_t default_tls_options) {
        // Configure the TLS protocol options.
        sec_protocol_options_t sec_protocol_options =
            nw_tls_copy_sec_protocol_options(default_tls_options);
        TLSWasConfigured = GNCConfigureTLSOptions(sec_protocol_options, PSK, identity);
      },
      /*tcp*/ NW_PARAMETERS_DEFAULT_CONFIGURATION);
  if (!TLSWasConfigured) {
    GNCLoggerError(@"[GNCNWParameters] Failed to configure TLS options.");
    return nil;
  }

  nw_parameters_set_include_peer_to_peer(parameters, includePeerToPeer);
  return parameters;
}

NS_ASSUME_NONNULL_END
