# Release checklist

1. Create the public GitHub repo (the source must be public: JUCE AGPL + our AGPL) and push. CI builds all platforms.
2. Download the macOS zip from the Actions run. Unsigned plugins work in DAWs but macOS Gatekeeper warns.
   For a clean install: Apple Developer Program ($99/yr), a "Developer ID Application" certificate, then sign and
   notarise `.vst3` and `.component` (`codesign --options runtime`, `xcrun notarytool`, `xcrun stapler`).
   Windows: unsigned works; SmartScreen may warn on a downloaded installer/zip.
3. One real check in a DAW on the Mac (FL Studio): loads, shows moving bands, resize, automation of a parameter,
   save + reload the project, "Save as default" colours.
4. Put the final zip + the repo link on manymalon.com; the About box says "source code is available from manymalon.com".
5. Optional courtesy: tell Max Mikhailov (Seven Phases) the port exists.
