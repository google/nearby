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
#ifndef LOCATION_NEARBY_SHARING_LIB_ACCOUNT_SIGNIN_ATTEMPT_H_
#define LOCATION_NEARBY_SHARING_LIB_ACCOUNT_SIGNIN_ATTEMPT_H_

namespace nearby::sharing {

// An in-progress sign-in; destroying it abandons the attempt.
class SigninAttempt {
 public:
  virtual ~SigninAttempt() = default;
};

}  // namespace nearby::sharing

#endif  // LOCATION_NEARBY_SHARING_LIB_ACCOUNT_SIGNIN_ATTEMPT_H_
