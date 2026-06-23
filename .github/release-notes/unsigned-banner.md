## macOS build is currently unsigned

Doobie's Apple Developer agreement is in the process of being
re-accepted. While that's pending, the macOS build ships as an
unsigned `.zip` instead of the usual signed + notarized `.pkg`
installer. Drop the bundles into:

  ~/Library/Audio/Plug-Ins/Components/Doobie.component
  ~/Library/Audio/Plug-Ins/VST3/Doobie.vst3
  /Applications/Doobie.app   (or anywhere)

Then strip the Gatekeeper quarantine attribute once per bundle:

```
xattr -cr ~/Library/Audio/Plug-Ins/Components/Doobie.component
xattr -cr ~/Library/Audio/Plug-Ins/VST3/Doobie.vst3
xattr -cr /Applications/Doobie.app
```

Or install via the Homebrew tap, which does the xattr step for you:

```
brew tap DatanoiseTV/doobie
brew install --cask doobie
```

Linux is unaffected (no signing concept).

---

