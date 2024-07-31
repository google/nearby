// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_TEST_MOCK_HTTP_CLIENT_H_
#define THIRD_PARTY_NEARBY_INTERNAL_TEST_MOCK_HTTP_CLIENT_H_

#include <memory>

#include "gmock/gmock.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "internal/network/http_client.h"
#include "internal/network/http_request.h"
#include "internal/network/http_response.h"

namespace nearby::network {

class MockHttpClient : public HttpClient {
 public:
  MOCK_METHOD(void, StartRequest,
              (const HttpRequest& request,
               absl::Duration connection_timeout,
               absl::AnyInvocable<void(const absl::StatusOr<HttpResponse>&)>),
              (override));
  MOCK_METHOD(void, StartCancellableRequest,
              (std::unique_ptr<CancellableRequest> request,
               absl::Duration connection_timeout,
               absl::AnyInvocable<void(const absl::StatusOr<HttpResponse>&)>),
              (override));
  MOCK_METHOD(absl::StatusOr<HttpResponse>, GetResponse,
              (const HttpRequest&, absl::Duration connection_timeout),
              (override));
};

}  // namespace nearby::network

#endif  // THIRD_PARTY_NEARBY_INTERNAL_TEST_MOCK_HTTP_CLIENT_H_
