// Copyright 2023 Google LLC
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

use thiserror::Error;

/// Library error type.
#[non_exhaustive]
#[derive(Error, Debug, PartialEq, Clone)]
pub enum FpError {
    /// Reported when a requested resource could not be accessed, either because
    /// it's missing or because the user lacks access permissions.
    #[error("not found: {0}")]
    AccessDenied(String),
    /// Reported when Fast Pair functionality does not behave according to the
    /// specification. For example, advertisement packets with invalid lengths,
    /// JSON data with bad formatting, etc.
    #[error("contract violation: {0}")]
    ContractViolation(String),
    /// Reported when trying to invoke Fast Pair functionality that is currently
    /// not implemented.
    #[error("feature not implemented: {0}")]
    NotImplemented(String),
    /// Reported when a bug occurs inside the library. Whenever a seemingly
    /// impossible error condition arises where you could call `expect()`,
    /// return this error instead.
    #[error("internal error: {0}")]
    Internal(String),
    /// Reported when an error was intentionally raised by test code.
    #[error("intentional error")]
    #[cfg(test)]
    Test,
}
