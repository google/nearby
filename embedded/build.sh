#!/bin/sh
# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#
# Parameters:
#   ${1} architecture (default: gLinux)
#   ${2} make target (e.g. "dist")
#     (remaining params are also passed to make)
#
# Checks for environment variables:
#     NEARBY_MAKEFILE_DEBUG: if set, passes -d argument to make
#     many others, not fully documented here yet
#
# e.g.
#  ./build.sh gLinux dist DEBUG=1

if [ "${1}" == "help" ] ; then
  echo "Usage:"
  echo "    ./build.sh <target> [dist] [args]"
  echo ""
  echo "Optional arguments:"
  echo "    DEBUG=1"
  echo "    -j72                           # parallelizes build using 72 cores"
  exit -1;
fi

if [ "${1}" != "" ] ; then
  ARCH="${1}"
  shift 1
else
  ARCH=gLinux
fi

CC=arm-none-eabi-gcc
AR=arm-none-eabi-ar

UNAME_OUT="$(uname -s)"
case "${UNAME_OUT}" in
    Linux*)     MACHINE=linux;;
    Darwin*)    MACHINE=mac;;
    CYGWIN*)    MACHINE=cygwin;;
    MINGW*)     MACHINE=mingw;;
    *)          MACHINE=windows;;
esac

if [ "${ARCH}" = "gLinux" ] ; then
  if [ -x /usr/bin/clang ] ; then
    CC=clang
  else
    CC=clang-3.8
  fi
  AR=ar
else
  echo "Invalid target specified on command line ${ARCH}"
  exit 1
fi

export NEARBY=$(cd `dirname ${0}` && pwd)

PLATFORM=$(uname -s | grep -i '\(mingw\|cygwin\)')
echo $PLATFORM

make_args=
if [ ! -z "$NEARBY_MAKEFILE_DEBUG" ]; then
  echo "NEARBY_MAKEFILE_DEBUG is set, passing -d to make"
  make_args+=" -d"
fi

make \
  $make_args \
  -C "${NEARBY}" \
  CC="${CC}" \
  AR="${AR}" \
  ARCH="${ARCH}" \
  $@

make_rc=$?

# NOTE: if make fails, it is important that the bad return code is bubbled up
# to the caller of this script.
# We don't want automated processes to keep going if there was a build error.

if [ $make_rc -ne 0 ]; then
  exit $make_rc
fi
