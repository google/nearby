#!/bin/bash

src=${1:-third_party/nearby/internal/platform/implementation/windows/generated/winrt/base.h}
patch=${2:-third_party/nearby/internal/platform/implementation/windows/generated/base-no-coroutines.patch}
out=${3:-$src}

if [[ $src != $out ]]; then
  cp $src $out;
fi
patch $out $patch --forward -r - > /dev/null
cat $out
