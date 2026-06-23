# Doobie Homebrew cask

`Casks/doobie.rb` is the source-of-truth Homebrew cask for Doobie.

It does **not** live here at runtime — Homebrew expects taps to be
**separate** GitHub repositories named `homebrew-<tapname>`. Users
install via:

```
brew tap DatanoiseTV/doobie
brew install --cask doobie
```

…which Homebrew resolves to the repo
`https://github.com/DatanoiseTV/homebrew-doobie`.

This `packaging/homebrew/` directory is where the cask file is
**authored** alongside the rest of the project, so the source-of-truth
isn't split across two repos. On each release, the cask is copied to
the tap repo and pushed.

## One-time tap setup

1. Create the public repo `DatanoiseTV/homebrew-doobie` on GitHub.
   No build-step / CI needed; Homebrew reads `Casks/*.rb` directly.
2. Inside that repo, create a single directory `Casks/` and copy
   `Casks/doobie.rb` from here into it.
3. Push.

That's it. From then on `brew tap DatanoiseTV/doobie` and
`brew install --cask doobie` work for any user.

## Release workflow (per version)

After the GitHub Release for `vX.Y.Z` is published with the
`Doobie-X.Y.Z-macOS-unsigned.zip` asset attached:

```sh
# In a local clone of the tap repo.
ZIP_URL="https://github.com/DatanoiseTV/doobie/releases/download/vX.Y.Z/Doobie-X.Y.Z-macOS-unsigned.zip"
SHA=$(curl -sL "$ZIP_URL" | shasum -a 256 | awk '{print $1}')

# Copy the latest cask from the main doobie repo and update version + sha.
cp /path/to/doobie/packaging/homebrew/Casks/doobie.rb Casks/doobie.rb
sed -i '' "s/^  version .*/  version \"X.Y.Z\"/" Casks/doobie.rb
sed -i '' "s/^  sha256 :no_check.*/  sha256 \"$SHA\"/" Casks/doobie.rb
# Or sha256 :no_check for nightly-style unsigned drops where you don't
# want to chase the hash; users get a warning but the cask still works.

git add Casks/doobie.rb
git commit -m "Doobie X.Y.Z"
git push
```

Optionally automate this from `release.yml` — fetch the tap, sed in
the new version + sha, push back. Not done yet to keep the unsigned-
release scaffolding self-contained.

## Why an external tap rather than homebrew-cask proper?

[homebrew-cask](https://github.com/Homebrew/homebrew-cask) only
accepts signed + notarized artifacts. Until Doobie's macOS releases
are signed again, the cask has to live in a personal tap.

When signed releases resume, the alternative cask body in
`Casks/doobie.rb` (commented at the bottom) is the form to switch to,
and at that point Doobie could be submitted to homebrew-cask proper
and the tap deprecated.
