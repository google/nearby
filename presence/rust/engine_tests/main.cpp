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

#include <iostream>

#include "engine.h"

using namespace std;

int main(int argc, char **argv) {
  cout << "C main started" << endl;

  const int conditionCnt = 2;
  DiscoveryCondition conditions[conditionCnt] = {
     {1, 2, 3}, // action, identity type, accuracy.
     {4, 5, 6},
  };
  DiscoveryConditionList condition_list = {
     &conditions[0],
     conditionCnt,
  };

  DiscoveryEngineRequest request = {
    0, // priority
    condition_list, // condition
  };

  auto request_echoed = echo_request(&request);
  cout << "C main received the echoed DiscoveryEngineRequest:" << endl;
  cout << "priority: " << request_echoed->priority << endl;
  auto conditions_echoed = request_echoed->conditions;
  for (int i = 0; i < conditions_echoed.count; i++) {
     cout << "Condition: " << i << endl;
     cout << "action: " << conditions_echoed.items[i].action << endl;
  }
  free_engine_request(request_echoed);
}
