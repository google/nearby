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

pub enum BleDataSection {
    ServiceData16BitUUid = 0x16,
}

pub struct ServiceData<U: Copy> {
    uuid: U,
    data: Vec<u8>,
}

impl<U: Copy> ServiceData<U> {
    pub fn new(uuid: U, data: Vec<u8>) -> Self {
        ServiceData { uuid, data }
    }

    pub fn uuid(&self) -> U {
        self.uuid
    }

    pub fn data(&self) -> &Vec<u8> {
        &self.data
    }
}
