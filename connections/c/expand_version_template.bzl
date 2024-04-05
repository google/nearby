# Copyright 2023 Google LLC
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

"""Rule for specialized expansion of template files. This performs a simple search over the template
file for the keys $VERSION and $VS_VERSION and replaces them with the corresponding values, derived 
from the version provided. Supports make variables.

Typical usage:
  load("expand_version_template.bzl", "expand_version_template")
  expand_version_template(
      name = "ExpandMyTemplate",
      out = "my.txt",
      template = "my.template",
      version = varref("VERSION"),
  )

Args:
  name: The name of the rule.
  out: The destination of the expanded file
  template: The template file to expand
  version: A string containing the version number. Supports make variables.
"""

def expand_version_template_impl(ctx):
    version = ctx.expand_make_variables(
        "expand_version_template",
        ctx.attr.version,
        {},
    )
    vs_version = version.replace(".", ",")
    ctx.actions.expand_template(
        template = ctx.file.template,
        output = ctx.outputs.out,
        substitutions = {
            "$VERSION": version,
            "$VS_VERSION": vs_version,
        },
    )

expand_version_template = rule(
    implementation = expand_version_template_impl,
    attrs = {
        "template": attr.label(mandatory = True, allow_single_file = True),
        "version": attr.string(mandatory = False),
        "out": attr.output(mandatory = True),
    },
)
