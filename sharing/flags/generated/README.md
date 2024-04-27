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

Alternatively, once the flag change has been submitted, the generated flags file
can be updated by launching the **Build and submit generated code** workflow in
the [better_together Rapid job](https://rapid.corp.google.com/#/project/better_together).
Launch the workflow on the latest release candidate that contains the CL for the
flag change.  The generated flag file will be auto submitted.