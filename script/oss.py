#!/usr/bin/python3

# Copyright 2020 Google LLC
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


import argparse
import os
import re
import shutil
import sys

copy_header="""Copyright 2020 Google LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.""".split("\n")

headline_match=".*Copyright [0-9,\- ]+ Google.*"

MISSING = 0
HEADLINE = 1
PARTIAL = 2
FULL = 3


def has_copyright(lines, max_lookup=3):
  pos = 0
  result = MISSING
  headline = re.compile(headline_match)
  for line in lines:
    pos += 1 # points to the next line
    if headline.match(line) != None:
      result = HEADLINE
      break
    if pos > max_lookup:
      return result

  for line in copy_header[1:]:
    if pos >= len(lines):
      return result
    if lines[pos].find(line) < 0:
      return result
    else:
      result = PARTIAL
      pos += 1

  return FULL


def add_copyright(lines, prefix, offset):
  new_lines = lines[0:offset]
  if offset:
    new_lines.append("\n")
  for line in copy_header:
    if line:
      new_lines.append(prefix + " " + line + "\n")
    else:
      new_lines.append(prefix + "\n")
  new_lines.append("\n")
  new_lines.extend(lines[offset:])
  return new_lines


def copy_files_to_oss_project(src_root, dst_root):
  shutil.rmtree(dst_root + "/cpp", ignore_errors=True)
  shutil.rmtree(dst_root + "/proto", ignore_errors=True)
  shutil.copytree(src_root + "/proto", dst_root + "/proto/")
  shutil.copytree(src_root + "/cpp/platform/", dst_root + "/cpp/platform/")
  shutil.copytree(src_root + "/cpp/platform_v2/", dst_root + "/cpp/platform_v2/")
  shutil.copytree(src_root + "/connections/core/", dst_root + "/cpp/core/")
  shutil.copytree(src_root + "/connections/core_v2/", dst_root + "/cpp/core_v2/")
  shutil.copytree(src_root + "/connections/proto/", dst_root + "/proto/connections/")
  shutil.copytree(src_root + "/mediums/proto/", dst_root + "/proto/mediums/")


def detect_file_copy_header_options(fname, lines):
  if not lines:
    return None # ignore empty file
  suffixes = [".cc", ".cpp", ".cxx", ".c", ".h", ".hpp", ".inc", ".mm", ".proto"]
  for suffix in suffixes:
    if fname.endswith(suffix):
      return ("//", 0)
  if (fname in ["CMakeLists.txt", "BUILD.gn", "BUILD"]) or (
              fname.startswith("CMakeLists") or fname.endswith(".cmake")):
    return ("#", 0)
  if lines[0].startswith("#!"):
    return ("#", 1)
  return None


def post_process_oss_files(path, args):
  modified_total = 0
  top_level = True
  if args.all:
    top_level = False # no special actions to take at top level
  else:
    top_dirs = ["cpp", "proto"]
  transforms = (
    ("::google3_proto_compat::MessageLite", "::google::protobuf::MessageLite"),
    ("third_party/webrtc/files/stable/", ""),
    ("webrtc/files/stable/", ""),
    ("third_party/", ""),
    ("location/nearby/connections/core_v2", "core_v2"),
    ("location/nearby/connections/core", "core"),
    ("location/nearby/cpp/platform_v2", "platform_v2"),
    ("location/nearby/cpp/platform", "platform"),
    ("security/cryptauth/lib/securegcm", "securegcm"),
    ("testing/base/public/gmock.h", "gmock/gmock.h"),
    ("testing/base/public/gunit.h", "gtest/gtest.h"),
    ("net/proto2/compat/public/message_lite.h",
     "google/protobuf/message_lite.h"),
    ("LOCATION_NEARBY_CONNECTIONS_", ""),
    ("LOCATION_NEARBY_CPP_", ""),
    ("location/nearby/proto", "proto"),
    ("location/nearby/connections/proto", "proto/connections"),
    ("_portable_proto.pb.h", ".pb.h"),
    (".proto.h", ".pb.h"),
    ("smhasher/MurmurHash3.h", "smhasher/src/MurmurHash3.h"),
  )

  for root, dirs, files in os.walk(path):
    if top_level and top_dirs:
      # we must convert cpp/ and proto/ subtrees.
      # everything else is not parsed.
      dirs.clear()
      dirs.extend(top_dirs)
      top_level = False
      continue
    for file in files:
      fname = root + "/" + file
      print("parsing: {}".format(fname))
      if file in ["METADATA"]:
        os.remove(fname)
        continue
      modified = False
      lines=[]
      google3_ignore = False
      with open(fname, "r") as f:
        add_proto_lite_runtime = args.proto_lite_runtime and fname.endswith(".proto")

        for line in f:
          orig = line

          if not args.no_subst:
            for lookup, substitute in transforms:
              line = line.replace(lookup, substitute)
            if orig != line:
              modified = True

          if args.google3_filter:
            if line.find("nearby:google3-only") >= 0:
              modified = True
              continue
            if line.find("nearby:google3-begin") >= 0:
              modified = True
              google3_ignore = True
              continue
            if line.find("nearby:google3-end") >= 0:
              modified = True
              google3_ignore = False
              continue
            if google3_ignore:
              modified = True
              continue

          if add_proto_lite_runtime and line.startswith("option "):
            if not line.startswith("option optimize_for = LITE_RUNTIME"):
              lines.append("option optimize_for = LITE_RUNTIME;\n")
              modified = True
            # LITE_RUNTIME should be added only once per file.
            add_proto_lite_runtime = False

          lines.append(line)

      if args.fix_oss_headers:
        options = detect_file_copy_header_options(file, lines)
        if options is not None:
          if not has_copyright(lines):
            prefix, offset = options
            lines = add_copyright(lines, prefix, offset)
            modified = True

      if modified:
        with open(fname, "w") as f:
          for line in lines:
            f.write(line)
        modified_total += 1
    if args.no_recurse:
      break

  return modified_total


def main():
  parser = argparse.ArgumentParser('Opensource Nearby Release Tool')
  parser.add_argument('target', action='store', type=str, nargs="+", default=[])
  parser.add_argument('--workspace', action='store', default="")
  parser.add_argument('--all', action='store_true', default=False)
  parser.add_argument('--fix-oss-headers', action='store_true', default=False)
  parser.add_argument('--google3-filter', action='store_true', default=False)
  parser.add_argument('--no-copy', action='store_true', default=False)
  parser.add_argument('--no-subst', action='store_true', default=False)
  parser.add_argument('--no-recurse', action='store_true', default=False)
  parser.add_argument('--proto-lite-runtime', action='store_true', default=False)
  args = parser.parse_args()
  if args.google3_filter:
    print("google3-specific code will be removed")
  if args.workspace:
    src = "/google/src/cloud/%s/%s/google3/location/nearby" % (os.environ["USER"], args.workspace)
  else:
     args.no_copy = True
  if len(args.target) == 1:
    dst = args.target[0]
  else:
    args.no_copy = True
    args.all = True
  if not args.no_copy: copy_files_to_oss_project(src, dst)
  total = 0
  for dst in args.target:
    total += post_process_oss_files(dst, args)
  print("Total modified: {} files".format(total))


if __name__ == "__main__":
  sys.exit(main())
