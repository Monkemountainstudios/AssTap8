# AssTap 8

AssTap 8 is a JUCE standalone audio application by MonkeMountainStudios / Sonic Onion.

The `Source` directory is the completed, tested Windows application source. The shared application and DSP source is intentionally kept identical across Windows and macOS. Platform support is supplied by JUCE and the build configuration.

## Supported standalone builds

- Windows x64 (`.exe`)
- macOS universal (`.app`, Intel x86_64 and Apple Silicon arm64; macOS 11 or newer)

The build is pinned to JUCE 9.0.1, matching the JUCE version used for the completed Windows build.

## Build with CMake

CMake 3.22 or newer, Git, and a platform compiler are required. If `ASSTAP8_JUCE_SOURCE_DIR` is omitted, CMake downloads JUCE 9.0.1 automatically.

Windows (Visual Studio):

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

macOS (Xcode, universal binary):

```bash
cmake -S . -B build -G Xcode \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release --parallel
```

Artifacts are written below `build/ASSTAP_8_artefacts/Release/`.

## Projucer

`ASSTAP_8.jucer` retains the Visual Studio exporter and adds a macOS Xcode exporter. Its module paths expect a JUCE checkout beside this repository (`../JUCE/modules`). CMake is the reproducible build used by CI and does not require that layout.

## Releases

Every push to `main` builds both platforms in GitHub Actions. Pushing a tag such as `v1.0.0` creates a GitHub release containing the Windows executable and universal macOS zip.

The automated macOS artifact is ad-hoc signed so its bundle remains intact. Public distribution without Gatekeeper warnings additionally requires Developer ID signing and Apple notarization credentials.
