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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_AUTH_STATUS_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_AUTH_STATUS_H_

namespace nearby {

enum AuthStatus {
  AUTH_STATUS_UNSPECIFIED = 0,
  // Request completed successfully, the results should be in the correct order
  // up to the given count.
  SUCCESS = 1,

  // Request encountered a generic error.
  GENERIC_ERROR = 2,

  // Request as specified is not supported.
  UNSUPPORTED = 3,

  // Request failed and should be retried soon.
  TEMPORARILY_UNAVAILABLE = 4,

  // Request failed due to an unavailable resource.
  UNAVAILABLE_RESOURCE = 5,

  // The request failed due to an invalid argument.
  INVALID_ARGUMENT = 6,

  // In case the status could not be retrieved.
  UNKNOWN_STATUS = 7,

  // Currently used as a way to signal an ETag mismatch.
  PRECONDITION_FAILED = 8,

  // The operation failed due to insufficient permissions.
  PERMISSION_DENIED = 9,

  // The resource exists, but the requested attribute of it does not.
  MISSING_ATTRIBUTE = 10,

  // The method was interrupted and the caller should exit the current unit of
  // work immediately.
  INTERRUPTED = 11,

  // User signed in with an unexpected account.
  SIGNED_IN_WITH_WRONG_ACCOUNT = 12,

  // Used when data cannot be parsed properly.
  PARSE_ERROR = 13,

  // Used to report that the local HTTP server for receiving the authorization
  // code cannot be created.
  CANT_CREATE_AUTH_SERVER = 14,

  // Used to report that the system browser for authenticating the user cannot
  // be open.
  CANT_OPEN_BROWSER_FOR_AUTH = 15,

  // Used to report that the authorization code cannot be received.
  CANT_RECEIVE_AUTH_CODE = 16,

  // Used to report that the account is blocked (e.g. CAA).
  ACCOUNT_BLOCKED = 17,

  // Receiving the authorization code failed because it took longer than the
  // timeout.
  AUTH_CODE_TIMEOUT_EXCEEDED = 18,
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_AUTH_STATUS_H_
