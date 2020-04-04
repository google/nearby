#!/usr/bin/python3

import os
import shutil
import sys

def copy_files_to_oss_project(src_root, dst_root):
  shutil.rmtree(dst_root + "/cpp", ignore_errors=True)
  shutil.rmtree(dst_root + "/proto", ignore_errors=True)
  shutil.copytree(src_root + "/proto", dst_root + "/proto/")
  shutil.copytree(src_root + "/cpp/platform/", dst_root + "/cpp/platform/")
  shutil.copytree(src_root + "/connections/core/", dst_root + "/cpp/core/")
  shutil.copytree(src_root + "/connections/proto/", dst_root + "/proto/connections/")

def post_process_oss_files(path):
  modified_total = 0
  top_level = True
  top_dirs = ["cpp", "proto"]
  transforms = (
    ("third_party/", ""),
    ("location/nearby/connections/core", "core"),
    ("location/nearby/cpp/platform", "platform"),
    ("security/cryptauth/lib/securegcm", "securegcm"),
    ("testing/base/public/gmock.h", "gmock/gmock.h"),
    ("testing/base/public/gunit.h", "gtest/gtest.h"),
    ("net/proto2/compat/public/message_lite.h", "google/protobuf/message_lite.h"),
    ("LOCATION_NEARBY_CONNECTIONS_", ""),
    ("LOCATION_NEARBY_CPP_", ""),
    ("location/nearby/proto", "proto"),
    ("location/nearby/connections/proto", "proto/connections"),
    ("_portable_proto.pb.h", ".pb.h"),
    (".proto.h", ".pb.h"),
  )
  for root, dirs, files in os.walk(path):
    if top_level:
      # we must convert cpp/ and proto/ subtrees.
      # everything else is not parsed.
      dirs.clear()
      dirs.extend(top_dirs)
      top_level = False
      continue
    for file in files:
      fname = root + "/" + file
      if file in ["METADATA"]:
        os.remove(fname)
        continue
      modified = False
      lines=[]
      with open(fname, "r") as f:
        for line in f:
          orig = line
          for lookup, substitute in transforms:
            line = line.replace(lookup, substitute)
          if orig != line:
            modified = True
          lines.append(line)
      if modified:
        with open(fname, "w") as f:
          for line in lines:
            f.write(line)
        modified_total += 1
  return modified_total

def main(args):
  src = "/google/src/cloud/%s/%s/google3/location/nearby" % (os.environ["USER"], args[1])
  dst = args[2]
  copy_files_to_oss_project(src, dst)
  total = post_process_oss_files(dst)
  print("Total modified: {} files".format(total))

if __name__ == "__main__":
  sys.exit(main(sys.argv))
