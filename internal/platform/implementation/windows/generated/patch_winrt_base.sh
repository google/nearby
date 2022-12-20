#!/bin/bash

patch \
  --forward \
  -r - \
  third_party/nearby/internal/platform/implementation/windows/generated/winrt/base.h \
  third_party/nearby/internal/platform/implementation/windows/generated/base-no-coroutines.patch
