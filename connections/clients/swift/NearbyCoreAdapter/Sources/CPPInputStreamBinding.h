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

#import <Foundation/Foundation.h>

#ifdef __cplusplus

namespace nearby {
class InputStream;
}

#endif

@interface CPPInputStreamBinding : NSObject

/**
 * Creates and attaches a @c CPPInputStreamBinding object to the provided stream as an associated
 * object.
 *
 * This association makes it possible for a c++ pointer to live as long as the original user
 * provided @c NSInputStream object.
 *
 * @param stream The stream to become associated with.
 */
+ (void)bindToStream:(NSInputStream *)stream;

#ifdef __cplusplus
/**
 * Retreives a reference to the c++ pointer associated to the provided stream.
 *
 * CPPInputStreamBinding::bindToStream: must be called on the this stream before calling this
 * function.
 *
 * @param stream The stream that has a c++ pointer associated with it.
 */
+ (nearby::InputStream &)getRefFromStream:(NSInputStream *)stream;
#endif

@end
