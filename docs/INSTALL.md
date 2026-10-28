# Installing Malon Frequency Analyzer

## macOS (VST3 + AU, universal: Apple Silicon and Intel)

Copy the plugins to:

| Format | Folder (all users) | Folder (just you) |
|---|---|---|
| VST3 | `/Library/Audio/Plug-Ins/VST3/Malon Frequency Analyzer.vst3` | `~/Library/Audio/Plug-Ins/VST3/` |
| AU | `/Library/Audio/Plug-Ins/Components/Malon Frequency Analyzer.component` | `~/Library/Audio/Plug-Ins/Components/` |

### Getting past Gatekeeper (the build is not signed or notarised yet)

macOS marks downloaded files as quarantined, so it may say the plugin "cannot be opened" or "is damaged".
Remove the quarantine flag once, in Terminal, after copying:

```
xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Malon Frequency Analyzer.vst3"
xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/Components/Malon Frequency Analyzer.component"
```

(Use `~/Library/...` paths if you installed for just yourself. You can also run the `xattr` command on the
unzipped files before copying.) If a plugin was already blocked, open System Settings > Privacy & Security and
click "Open Anyway" next to the message about Malon Frequency Analyzer, then rescan plugins.

Then rescan plugins in your DAW. In FL Studio: Options > Manage plugins > Find installed plugins.
For AU in other hosts, if it doesn't show up, log out and in, or run `killall -9 AudioComponentRegistrar`.

## Windows (VST3)

Copy `Malon Frequency Analyzer.vst3` to `C:\Program Files\Common Files\VST3\`, then rescan plugins.
SmartScreen may warn about a downloaded zip because it is unsigned; choose "More info > Run anyway",
or right-click the zip > Properties > Unblock before extracting.
