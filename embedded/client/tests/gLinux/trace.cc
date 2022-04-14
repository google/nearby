// Copyright 2022 Google LLC
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

#include <stdlib.h>

#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <sstream>

#include "nearby_platform_trace.h"

void nearby_platform_Trace(nearby_platform_TraceLevel level,
                           const char *filename, int lineno, const char *fmt,
                           ...) {
  char buff[1024];
  va_list args;
  va_start(args, fmt);

  vsnprintf(buff, sizeof(buff), fmt, args);
  std::cout << filename << ":" << lineno << " " << buff << std::endl;
  va_end(args);
}

void nearby_platfrom_CrashOnAssert(const char *filename, int lineno,
                                   const char *reason) {
  fprintf(stderr, "ASSERT %s, %s:%d\n", reason, filename, lineno);
  abort();
}

void nearby_platform_TraceInit(void) {}
