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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_CLIENT_FACTORY_IMPL_H_
#define THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_CLIENT_FACTORY_IMPL_H_

#include <memory>

#include "internal/network/http_client_factory.h"
#include "internal/network/http_client_impl.h"

namespace nearby {
namespace network {

class HttpClientFactoryImpl : public HttpClientFactory {
 public:
  std::unique_ptr<HttpClient> CreateInstance() override {
    return std::make_unique<NearbyHttpClient>();
  }
};

}  // namespace network
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_CLIENT_FACTORY_IMPL_H_
