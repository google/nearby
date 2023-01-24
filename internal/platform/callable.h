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

#ifndef PLATFORM_BASE_CALLABLE_H_
#define PLATFORM_BASE_CALLABLE_H_

#include "absl/functional/any_invocable.h"
#include "internal/platform/exception.h"

namespace nearby {

// The Callable is and object intended to be executed by a thread, that is able
// to return a value of specified type T.
// It must be invokable without arguments. It must return a value implicitly
// convertible to ExceptionOr<T>.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Callable.html
template <typename T>
using Callable = absl::AnyInvocable<ExceptionOr<T>()>;

}  // namespace nearby

#endif  // PLATFORM_BASE_CALLABLE_H_
