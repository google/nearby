// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use core::marker::PhantomData;
use fpp::presence_detector::PresenceDetector;
use lazy_static::lazy_static;
use rand::Rng;
use std::collections::HashMap;
use std::sync::{Mutex, MutexGuard};

pub(crate) struct HandleMap<T> {
    _marker: PhantomData<T>,
    map: HashMap<u64, T>,
}

impl<T> HandleMap<T> {
    pub(crate) fn init() -> Self {
        Self {
            _marker: Default::default(),
            map: HashMap::new(),
        }
    }

    /// inserts an entry into the map and returns the randomly generated handle to the entry
    pub(crate) fn insert(&mut self, data: T) -> u64 {
        let mut rng = rand::thread_rng();
        let mut handle: u64 = rng.gen();

        while self.map.contains_key(&handle) {
            handle = rng.gen();
        }

        assert!(self.map.insert(handle, data).is_none());
        handle
    }

    /// Removes an entry at a given handle returning an Option of the owned value
    pub(crate) fn remove(&mut self, handle: &u64) -> Option<T> {
        self.map.remove(handle)
    }

    /// Gets a reference to the entry stored at the specified handle
    pub(crate) fn get(&mut self, handle: &u64) -> Option<&mut T> {
        self.map.get_mut(handle)
    }
}

// Returns a threadsafe instance of the global static hashmap tracking the PresenceDetector handles
pub(crate) fn get_presence_detector_handle_map(
) -> MutexGuard<'static, HandleMap<Box<PresenceDetector>>> {
    PRESENCE_DETECTOR_HANDLE_MAP
        .lock()
        .unwrap_or_else(|err_guard| err_guard.into_inner())
}

// Global hashmap to track valid pointers, this is a safety precaution to make sure we are not
// reading from unsafe memory address's passed in by the caller
lazy_static! {
    static ref PRESENCE_DETECTOR_HANDLE_MAP: Mutex<HandleMap<Box<PresenceDetector>>> =
        Mutex::new(HandleMap::init());
}
