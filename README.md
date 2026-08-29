# AssTap-8
### Asynchronous Tape Machine

AssTap-8 is an eight-track asynchronous audio-loop machine for Windows and macOS.

Each Tape plays and evolves independently. Rather than synchronising material to a common clock, AssTap-8 allows loops to drift, mutate, reverse, filter, change level and pan, move through their source material, and gradually form new relationships over time.

It is intended as an instrument for generative music, ambient composition, experimental looping, sound collage, and turning perfectly sensible recordings into things they were never supposed to become.

## The basic idea

Load up to eight audio files.

Each Tape has its own playback state and can run independently of every other Tape. Enable **MUTATE** and the machine will gradually make decisions about that Tape over time.

AssTap-8 is deliberately not a conventional synchronised looper. There is no requirement for matching tempos, matching lengths, bars, beats, or even remotely compatible source material.

A three-second sound can coexist with a five-minute recording.

Two copies of the same performance can slowly separate.

Long recordings containing silence can create events that disappear and unexpectedly return much later.

Or you can fill all eight Tapes with goats. The machine will not stop you.

## Controls

 [Read the AssTap-8 manual](AT8Manual.pdf)
Basically you load sound into each "tape" and if you want press mutate. The machine will then mutate the playback as set by the Probability setting.

The activity displays beneath each "Tape" show the movement and destination of the major mutation processes:

`LOOP  FILTER  VOL  PAN  DIR  OCT`

You just set back and listen. 

## Using space as a musical tool

The source recording is part of the composition.

A short, continuously sounding sample tends to produce dense repeating or drone-like material.

Longer recordings containing pauses and empty space allow AssTap-8 to move through both sound and silence. Individual events may disappear for long periods before returning in unexpected combinations.

There is no correct way to prepare material for AssTap-8.

But by all means fill it with eight goats bleating if that's your thing.

## Design philosophy

AssTap-8 is based around slow, continuous change rather than rapid randomisation.

A Tape generally begins a journey, completes that journey, and then decides what to do next. Parameters move toward destinations rather than simply jumping between random values.

The intention is to create an evolving system that can be listened to and played, rather than a randomisation button that continually throws new settings at the audio.

## Credits

### Concept, design and direction
**Magnus Lassila / Monke Mountain Studios**

AssTap-8 was conceived, developed, tested and directed by Magnus Lassila as part of the Monke Mountain Studios collection of experimental music machines.

### Development
AssTap-8 was developed using **JUCE**.

Programming and development were carried out through an AI-assisted workflow using **OpenAI ChatGPT** and **OpenAI Codex**, under the direction and extensive testing of Magnus Lassila.

ChatGPT was used throughout the design and development process for technical implementation, DSP development, debugging, interface development, experimentation and documentation.

Codex was used for code inspection, implementation assistance, project maintenance and platform/build work.

### Framework
**JUCE**  
Copyright © Raw Material Software Limited.

JUCE is used under its applicable licensing terms.

Please see the JUCE licence and the project's accompanying licence/third-party notices for further information.

## Testing

AssTap-8 grew through extensive real-world use rather than being designed solely from a specification.

It has been tested with material ranging from very short samples to multi-minute recordings, spoken word, drums, complete musical performances, environmental recordings and combinations that probably should never have been attempted.

The strange results are generally intentional.

## Platforms

- Windows — Standalone
- macOS — Standalone

## Version

**AssTap-8 1.0**

## Licence

Licence information will be included with the release.

---

**AssTap-8 // Asynchronous Tape Machine**  
Monke Mountain Studios
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
