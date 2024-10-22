# Feature flags generation

This directory contains the generated Nearby Share feature flags definition
file.

## Adding flags

New flags need to be added to google3/googledata/experiments/mobile/nearby/features/nearby_sharing_feature.gcl.

To generate code for the new flags, run:

```
blaze build //third_party/nearby/sharing/flags:nearby_sharing_feature_flags_cpp_consts
```

The creates the generated file in *blaze-genfiles/third_party/nearby/sharing/flags/nearby_sharing_feature_flags.h*.  Copy this file to google3/third_party/nearby/sharing/flags/generated/nearby_sharing_feature_flags.h
and include in your CL for submission.
