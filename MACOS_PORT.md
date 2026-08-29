# macOS port assessment

## Shared source

No Windows-specific APIs, drive paths, registry calls, Win32 handles, ASIO calls, or preprocessor branches occur in `Source`. No shared source change is required for macOS.

- Audio output is opened through `juce::AudioAppComponent`; JUCE selects CoreAudio on macOS and retains WASAPI/DirectSound/ASIO support on Windows.
- Audio file selection uses `juce::FileChooser`.
- Recordings use `juce::File::userDocumentsDirectory`, producing `~/Documents/AssTap Recordings` on macOS and the equivalent Documents folder on Windows.
- Embedded UI artwork is compiled from the existing image-data headers, so rendering inputs are identical.
- Window sizing and layout are JUCE code and remain shared.

## Platform/build changes

- Added an Xcode macOS exporter to `ASSTAP_8.jucer`.
- Added a CMake build pinned to the same JUCE release used by the tested Windows project: 9.0.1.
- Added a universal macOS CI build (`arm64` + `x86_64`) targeting macOS 11 or newer.
- Preserved the Windows-only JUCE backend flags only under `WIN32`; CoreAudio uses JUCE's macOS defaults.
- Added packaging and ad-hoc code signing for CI artifacts.

## Release signing

The CI macOS bundle can run when the user explicitly allows an unidentified application, but a polished public release needs:

1. Apple Developer Program membership.
2. A Developer ID Application certificate exported as a password-protected `.p12`.
3. Apple notarization credentials (App Store Connect API key is preferred).
4. Those values stored as encrypted GitHub Actions secrets.

Signing and notarization are packaging steps and require no DSP, audio, or UI source changes.

