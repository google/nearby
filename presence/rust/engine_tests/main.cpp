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
