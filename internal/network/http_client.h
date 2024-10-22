// Copyright 2021-2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_CLIENT_H_
#define THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_CLIENT_H_

#include <memory>

#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "internal/network/http_request.h"
#include "internal/network/http_response.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"

namespace nearby {
namespace network {

class HttpClient {
 public:
  class CancellableRequest {
   public:
    explicit CancellableRequest(const HttpRequest& request)
        : http_request_(request) {}
    ~CancellableRequest() = default;

    bool is_cancelled() ABSL_LOCKS_EXCLUDED(mutex_) {
      MutexLock lock(&mutex_);
      return is_cancelled_;
    }

    void cancel() ABSL_LOCKS_EXCLUDED(mutex_) {
      MutexLock lock(&mutex_);
      is_cancelled_ = true;
    }

    const HttpRequest& http_request() ABSL_LOCKS_EXCLUDED(mutex_) {
      MutexLock lock(&mutex_);
      return http_request_;
    }

   private:
    Mutex mutex_;
    bool is_cancelled_ ABSL_GUARDED_BY(mutex_) = false;
    HttpRequest http_request_ ABSL_GUARDED_BY(mutex_);
  };

  virtual ~HttpClient() = default;

  // Starts HTTP request in asynchronization mode.
  virtual void StartRequest(
      const HttpRequest& request,
      absl::AnyInvocable<void(const absl::StatusOr<HttpResponse>&)>
          callback) = 0;

  // Starts cancellable request in asynchronization mode.
  virtual void StartCancellableRequest(
      std::unique_ptr<CancellableRequest> request,
      absl::AnyInvocable<void(const absl::StatusOr<HttpResponse>&)>
          callback) = 0;

  // Gets HTTP response in synchronization mode.
  virtual absl::StatusOr<HttpResponse> GetResponse(
      const HttpRequest& request) = 0;

  // The error may be corrected if retried at a later time.
  static bool IsRetryableHttpError(absl::Status status) {
    return absl::IsUnavailable(status) || absl::IsFailedPrecondition(status);
  }
};

}  // namespace network
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_CLIENT_H_
