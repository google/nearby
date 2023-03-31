// Copyright 2022-2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_CLIENT_IMPL_H_
#define THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_CLIENT_IMPL_H_

#include <functional>
#include <memory>

#include "absl/base/thread_annotations.h"
#include "absl/status/statusor.h"
#include "internal/network/http_client.h"
#include "internal/platform/multi_thread_executor.h"
#include "internal/platform/mutex.h"

namespace nearby {
namespace network {

class NearbyHttpClient : public HttpClient {
 public:
  NearbyHttpClient();
  ~NearbyHttpClient() override = default;

  NearbyHttpClient(const NearbyHttpClient&) = default;
  NearbyHttpClient& operator=(const NearbyHttpClient&) = default;
  NearbyHttpClient(NearbyHttpClient&&) = default;
  NearbyHttpClient& operator=(NearbyHttpClient&&) = default;

  // Starts HTTP request in asynchronization mode.
  void StartRequest(const HttpRequest& request,
                    std::function<void(const absl::StatusOr<HttpResponse>&)>
                        callback) override ABSL_LOCKS_EXCLUDED(mutex_);

  // Gets HTTP response in synchronization mode.
  absl::StatusOr<HttpResponse> GetResponse(const HttpRequest& request) override;

 private:
  absl::StatusOr<HttpResponse> InternalGetResponse(const HttpRequest& request);

  Mutex mutex_;
  std::unique_ptr<MultiThreadExecutor> network_executor_ = nullptr;
};

}  // namespace network
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_CLIENT_IMPL_H_
