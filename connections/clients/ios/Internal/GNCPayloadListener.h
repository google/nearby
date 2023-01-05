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

#ifdef __cplusplus

#import <Foundation/Foundation.h>

#include <string>

#import "connections/clients/ios/Internal/GNCCore.h"
#import "connections/clients/ios/Public/NearbyConnections/GNCConnection.h"

NS_ASSUME_NONNULL_BEGIN

@class GNCPayloadInfo;

namespace nearby {
namespace connections {

/** This fetches a GNCConnectionHandlers object. */
typedef GNCConnectionHandlers *_Nonnull (^GNCConnectionHandlersProvider)();

/** This fetches a payload dictionary. */
typedef NSMutableDictionary<NSNumber *, GNCPayloadInfo *> *_Nonnull (^GNCPayloadsProvider)();

/** This is the payload handler for an advertiser or discoverer. */
class GNCPayloadListener : public PayloadListener {
 public:
  GNCPayloadListener(GNCCore *core, GNCConnectionHandlersProvider handlersProvider,
                     GNCPayloadsProvider payloadsProvider)
      : core_(core), handlers_provider_(handlersProvider), payloads_provider_(payloadsProvider) {}

  void OnPayload(const std::string &endpoint_id, Payload payload);
  void OnPayloadProgress(const std::string &endpoint_id, const PayloadProgressInfo &info);

 private:
  GNCCore *core_;
  GNCConnectionHandlersProvider handlers_provider_;
  GNCPayloadsProvider payloads_provider_;
};

}  // namespace connections
}  // namespace nearby

NS_ASSUME_NONNULL_END

#endif
