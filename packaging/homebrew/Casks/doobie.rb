# Doobie — Homebrew cask
#
# This file is the source of truth for the cask that lives in the user's
# tap at  https://github.com/DatanoiseTV/homebrew-doobie  in
# Casks/doobie.rb. Whenever a new Doobie release ships, copy this file
# to that tap, update `version` + `sha256`, and push.
#
# Usage on a user's machine:
#   brew tap DatanoiseTV/doobie
#   brew install --cask doobie
#
# The cask grabs the unsigned zip from the GitHub release, expands it,
# installs the three bundles, and runs `xattr -cr` on each to strip the
# Gatekeeper quarantine attribute (which would otherwise block load on
# first run, since the bundles aren't notarized while the Apple
# Developer agreement is being re-accepted).
#
# When signed + notarized .pkg releases resume, this file should be
# replaced with the `pkg` variant — see the commented stanza at the
# bottom for the shape.

cask "doobie" do
  version "0.20.1"
  # SHA-256 of the zip on the GitHub release. Recompute on every version
  # bump: `shasum -a 256 Doobie-<ver>-macOS-unsigned.zip`.
  sha256 "0acc998733f5bbb49ad278ef6c29f80e66837904f7ad16058e137a6c1461169f"

  url "https://github.com/DatanoiseTV/doobie/releases/download/v#{version}/Doobie-#{version}-macOS-unsigned.zip"
  name "Doobie"
  desc "Analog dub delay — VST3, AU, Standalone"
  homepage "https://github.com/DatanoiseTV/doobie"

  # The release zip's top-level directory is `Doobie-<version>-macOS-unsigned/`
  # (see packaging/macos/build-zip.sh).
  audio_unit_plugin "Doobie-#{version}-macOS-unsigned/Doobie.component"
  vst3_plugin       "Doobie-#{version}-macOS-unsigned/Doobie.vst3"
  app               "Doobie-#{version}-macOS-unsigned/Doobie.app"

  # Strip the Gatekeeper quarantine attribute that brew applied while
  # downloading. Done per-bundle because each lands in a different
  # location, and `xattr -cr` is idempotent so re-runs are safe.
  postflight do
    [
      "#{HOMEBREW_PREFIX}/Library/Audio/Plug-Ins/Components/Doobie.component",
      "#{HOMEBREW_PREFIX}/Library/Audio/Plug-Ins/VST3/Doobie.vst3",
      "#{appdir}/Doobie.app",
    ].each do |path|
      system_command "/usr/bin/xattr", args: ["-cr", path], sudo: false if File.exist?(path)
    end
  end

  zap trash: [
    "~/Library/Audio/Presets/DatanoiseTV/Doobie",
    "~/Library/Application Support/DatanoiseTV/Doobie",
    "~/Library/Caches/com.datanoisetv.doobie",
    "~/Library/Preferences/com.datanoisetv.doobie.plist",
  ]
end

# When signed + notarized .pkg releases come back, replace the cask
# body above with the .pkg variant below (and drop the `postflight`):
#
#   cask "doobie" do
#     version "X.Y.Z"
#     sha256 "<sha of the .pkg>"
#     url "https://github.com/DatanoiseTV/doobie/releases/download/v#{version}/Doobie-#{version}-macOS.pkg"
#     name "Doobie"
#     desc "Analog dub delay — VST3, AU, Standalone"
#     homepage "https://github.com/DatanoiseTV/doobie"
#     pkg "Doobie-#{version}-macOS.pkg"
#     uninstall pkgutil: [
#       "com.datanoisetv.doobie.au",
#       "com.datanoisetv.doobie.vst3",
#       "com.datanoisetv.doobie.standalone",
#     ]
#   end
