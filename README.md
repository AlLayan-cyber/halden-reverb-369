# Halden Reverb 369

Lexicon-style stereo reverb by **Moon Audio Engineering** (vendor folder: *Moon Music Label*), built with **JUCE 8** and **CMake**.

## What's inside
- 7 algorithms: Random Hall (random modulation), Concert Hall, Hall, Chamber, Room, Vintage Plate, Ambience
- Main controls: Pre-Delay, Size, Decay, Damping, Mix (100 % = fully wet)
- ADVANCED drawer: Shape, Spread, Diffusion, Bass, X-Over, Spin, Wander, Width, Ducking (glow ring shows live reduction), Post EQ
- FX rack: Pump (tempo-synced), Shimmer (−12 / −7 / +7 / +12, Glide), Freeze, Riser, Mono Bass, Gate (tempo-synced)
- Input / output faders with square-dot LED meters, spectrum LCD with 4 s peak hold (click to reset)
- Window scale 75 / 100 / 125 / 150 %

## Formats built by GitHub Actions
- **macOS (Universal arm64 + x86_64)**: `.vst3` and `.component` (AU), DSP tests run first
- **Windows (x64)**: `.vst3`

## Local build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DHALDEN_BUILD_TESTS=ON
cmake --build build --config Release --target HaldenTests HaldenReverb369_VST3 HaldenReverb369_AU -j 4
./build/HaldenTests_artefacts/Release/HaldenTests
```
Windows: `cmake -B build -A x64` then `cmake --build build --config Release --target HaldenReverb369_VST3`.

## Install
- macOS VST3 → `/Library/Audio/Plug-Ins/VST3/`, AU → `/Library/Audio/Plug-Ins/Components/`
- Windows VST3 → `C:\Program Files\Common Files\VST3\`
- Unsigned build on macOS: `xattr -cr "/Library/Audio/Plug-Ins/Components/Halden Reverb 369.component"` (same for the `.vst3`)
