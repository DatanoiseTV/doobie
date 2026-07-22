## macOS and Windows builds are currently unsigned

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

### Windows

The Windows build is also unsigned (no Authenticode certificate yet).
Windows SmartScreen may warn the first time you run the Standalone
`Doobie.exe` — click "More info" then "Run anyway". Plugins:

    VST3: copy `Doobie.vst3` to `C:\Program Files\Common Files\VST3\`
    CLAP: copy `Doobie.clap` to `C:\Program Files\Common Files\CLAP\`

The WebView interface uses the Microsoft WebView2 Runtime, which is
preinstalled on Windows 11 and on any up-to-date Windows 10.

---

