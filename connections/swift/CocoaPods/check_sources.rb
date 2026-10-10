# Copyright 2026 Google LLC
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

require 'cocoapods'
require 'json'
require 'open3'
require 'set'

root = Pathname.new(__dir__).join('../../..').realpath
manifest, status = Open3.capture2('swift', 'package', 'dump-package', :chdir => root.to_s)
abort 'Unable to read Package.swift' unless status.success?
targets = JSON.parse(manifest).fetch('targets').to_h { |target| [target.fetch('name'), target] }
extensions = %w[.c .cc .cpp .cxx .m .mm .swift]

def local_targets(name, targets)
  dependencies = targets.fetch(name).fetch('dependencies').flat_map do |dependency|
    local_name = (dependency['target'] || dependency['byName'] || []).first
    targets.key?(local_name) ? local_targets(local_name, targets) : []
  end
  [name] + dependencies
end

# Compare compilation units, not headers: CocoaPods exposes only the Objective-C
# adapter headers, while the native sources use the original repository layout.
{
  'NearbyCoreAdapter' => local_targets('NearbyCoreAdapter', targets).uniq,
  'NearbyConnections' => %w[NearbyConnections],
}.each do |pod, names|
  expected = names.flat_map do |name|
    target = targets.fetch(name)
    directory = root.join(target.fetch('path'))
    excluded = target.fetch('exclude', []).map { |path| directory.join(path).cleanpath.to_s }
    target.fetch('sources').flat_map do |source|
      path = directory.join(source).cleanpath
      abort "Missing #{path}; initialize the repository's submodules" unless path.exist?
      paths = path.directory? ? Dir.glob(path.join('**/*').to_s) : [path.to_s]
      paths.reject { |file| excluded.any? { |entry| file == entry || file.start_with?(entry + '/') } }
    end
  end.select { |path| extensions.include?(File.extname(path)) }.to_set

  spec = Pod::Specification.from_file(root.join("#{pod}.podspec"))
  accessor = Pod::Sandbox::FileAccessor.new(root, spec.consumer(:ios))
  actual = accessor.source_files.select { |path| extensions.include?(path.extname) }.map(&:to_s).to_set
  missing = expected - actual
  extra = actual - expected
  unless missing.empty? && extra.empty?
    warn "#{pod} sources differ from Package.swift:"
    missing.sort.each { |path| warn "  Missing: #{Pathname.new(path).relative_path_from(root)}" }
    extra.sort.each { |path| warn "  Extra: #{Pathname.new(path).relative_path_from(root)}" }
    exit 1
  end
  puts "#{pod}: #{actual.size} compilation units match Package.swift"
end
