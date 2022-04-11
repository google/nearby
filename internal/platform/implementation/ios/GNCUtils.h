// Copyright 2020 Google LLC
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

NS_ASSUME_NONNULL_BEGIN

// Current version of socket protocol.
extern const int GNCSocketVersion;

#ifdef __cplusplus
extern "C" {
#endif

/// Generates a SHA-256 hash (32 bytes) from a data object.
NSData *_Nullable GNCSha256Data(NSData *data);

/// Generates a SHA-256 hash (32 bytes) from a string.
NSData *_Nullable GNCSha256String(NSString *string);

/// Generates an MD5 hash (16 bytes) from a data object.
NSData *_Nullable GNCMd5Data(NSData *data);

/// Generates an MD5 hash (16 bytes) from a string.
NSData *_Nullable GNCMd5String(NSString *string);

#ifdef __cplusplus
}  // extern "C"
#endif

NS_ASSUME_NONNULL_END
