# Build JVoice

Requirements: CMake >= 3.25, Python >= 3.10, C/C++17 compiler, VST3 SDK.
No JieLi firmware toolchain or hardware SDK is needed.

SDK version tested: Steinberg VST3 SDK 3.8.1,
commit `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96` (MIT).

```sh
git clone --recurse-submodules https://github.com/steinbergmedia/vst3sdk.git
git -C vst3sdk checkout 3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96
git -C vst3sdk submodule update --init --recursive
cmake -S vst3 -B vst3/build -DVST3_SDK_ROOT=/absolute/path/to/vst3sdk
cmake --build vst3/build --config Release
ctest --test-dir vst3/build -C Release --output-on-failure
```

On Windows use a Visual Studio 2022 x64 developer shell; add `-A x64` when
configuring. The bundle is `vst3/build/JVoice.vst3`. Run the SDK validator
on that directory (its executable is under `vst3/build/bin`).

For a Linux engine-only build, add `-DVOICE_BUILD_PLUGIN=OFF` and specify
`-DCMAKE_BUILD_TYPE=Release`. Cross compilation with MinGW-w64 GCC 14 POSIX
is also supported using a conventional CMake Windows toolchain file.

`tools/prepare_engine.py` generates an adaptation of the pinned firmware:
per-instance storage, thread-local binding, nine internal articulation tracks
(one lyric cursor plus eight notes), larger lyric buffers and 16-bit text
offsets. Firmware sources remain unchanged. English words are resolved
directly at note boundaries rather than using the firmware's byte-offset
text cache. No allocation or blocking mutex waits occur in the audio callback.

Tests cover the text limit including 512-byte transliteration, UTF-8 validation,
extended languages, polyphony, independent plugin instances, old notes surviving
text edits, note ID pairing, state serialization, keyswitches, sound and release.
On Windows plugin_test also attaches the real Win32 editor and enters Cyrillic.
The SDK validator checks the VST3 ABI and lifecycle. Version 0.2 also tests
multiline editing, unrestricted language changes, transport restart, automation
gestures and event points, timbre controls, stereo spread, polyphonic detune
and 1–8-layer unison.

Version 0.3 adds stereo sidechain carrier input, fixed-rate input/output
interpolation, release and per-channel pitch bend.
Runtime tests cover state duplication into independent instances, host
automation handlers, concurrent DSP, and two Win32 editor windows.
No Studio One installation is bundled; its particular project routing
requires verification in the user's DAW.

Version 0.3.1 removes all voice-count gain normalization and adds an automatable
manual Master Volume (-60..+12 dB, default 0 dB). Tests check linear eight-note
summation, a silent note/release not ducking other voices in either carrier mode,
manual dB gain on the whole output and version-3 state migration.
The experimental frication randomization has been removed from the DSP, editor
and host parameter list; its old parameter ID is retired without renumbering.
Ext. Input Gain (-24..+24 dB, default 0 dB) adjusts only the tonal input, with
a fixed +12 dB calibration relative to 0.3.0 at the new zero position.
