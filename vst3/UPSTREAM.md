# JVOICE source

The VOICE DSP is forked from JIANT's published preview pin:

- Repository: https://github.com/juanjiant-bit/JIANT
- Preview pin: `web/PREVIEW` in upstream main
- Commit: `4815517085a828e5261f8d4125292adb24962ee7`
- Version: V10.7.16
- Source: `firmware/src/eng_formant.c`, `sing.c`, `sing_ext.c`, `dsp.c`, `core.h`
- Language tables: `tools/gen_lyrics.py`, `tools/lyrics/`
- Russian transliteration: `lyricText()` in upstream `web/editor.html`

JVOICE additions are GPL-3.0-only, like the upstream firmware. Copyright notices
are preserved by the source generator. CMUdict retains its BSD notice; see
`LICENSES/BSD-2-Clause-CMUdict.txt`. The VST3 SDK is separately MIT licensed and
is not vendored; build against the SDK commit in BUILDING.md.

The desktop adapter uses independent articulation for eight notes, a desktop
envelope, linear voice summation and a manual master gain. This is a port, not a bit-for-bit
render of the entire FM-1 signal chain. Firmware reverb, delay, modulation and
hardware interfaces are not linked into the plugin.

JVoice's Russian consonant/vowel profiles and contextual reader are desktop
adaptations. Standalone consonants and phoneme stepping run in the common
articulation engine. Original firmware files remain unchanged.
