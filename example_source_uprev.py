"""A tast test for essential inputs binaries on real devices."""

from google3.chrome.cros.buildbucket import copybara_source_uprev


def get_recipe():
  """Runs the tast test in buildbucket."""
  return copybara_source_uprev.Google3ToGithubToGerritSourceUprev(
      gerrit_site='https://chromium-review.googlesource.com',
      gerrit_project='chromium/src',
      deps_revision_tag='nearby_revision',
      srcroot='third_party/nearby_connections')
