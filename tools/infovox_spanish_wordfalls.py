"""Pitch-contour and tempo edit of genuine Infovox 230 word renders.
Infovox itself supplies all phonemes and voice timbre. Praat PSOLA edits pitch
and duration while aiming to preserve its formants.
"""
from pathlib import Path
import numpy as np
import soundfile as sf
import parselmouth
from parselmouth.praat import call

root = Path(".")
word_labels = [
    ("Música", 0),
    ("electrónica", 0),
    ("Figura", 1),
    ("rítmica", 1),
    ("Arte", 2),
    ("política", 2),
    ("La", 3),
    ("era", 3),
    ("atómica", 3),
]
start_hz = 122.0
end_hz = 94.0
duration_factor = 4.0 / 3.0

def trim_edges(x, sr):
    # Thresholded RMS envelope in 10-ms windows; preserve soft consonant tails.
    hop = max(1, int(sr * 0.01))
    nblocks = int(np.ceil(len(x) / hop))
    padded = np.pad(x, (0, nblocks * hop - len(x)))
    rms = np.sqrt(np.mean(padded.reshape(-1, hop)**2, axis=1))
    threshold = max(float(np.max(rms)) * 0.013, 0.0006)
    active = np.flatnonzero(rms >= threshold)
    if len(active) == 0:
        raise RuntimeError("No detectable voice in a word")
    start = max(0, int(active[0]) * hop - int(sr * 0.026))
    end = min(len(x), (int(active[-1]) + 1) * hop + int(sr * 0.042))
    return x[start:end]

def shape_word(x, sr):
    speech = parselmouth.Sound(x, sampling_frequency=sr)
    manipulation = call(speech, "To Manipulation", 0.01, 60, 250)
    duration = speech.get_total_duration()
    if duration < 0.07:
        raise RuntimeError(f"Short word: {duration}")
    pitch = call(manipulation, "Extract pitch tier")
    call(pitch, "Remove points between", 0, duration)
    # Separate full downward F0 slope for every single word.
    call(pitch, "Add point", 0.012, start_hz)
    call(pitch, "Add point", max(0.015, duration - 0.012), end_hz)
    call([pitch, manipulation], "Replace pitch tier")
    dur_tier = call(manipulation, "Extract duration tier")
    call(dur_tier, "Add point", 0.012, duration_factor)
    call(dur_tier, "Add point", max(0.015, duration - 0.012), duration_factor)
    call([dur_tier, manipulation], "Replace duration tier")
    out = call(manipulation, "Get resynthesis (overlap-add)").values[0]
    return np.asarray(out, dtype=np.float64)

parts = []
old_group = None
sr_ref = None
logs = []
for idx, (label, group) in enumerate(word_labels):
    audio_file = root / "infovox_words" / f"word_{idx:02d}.wav"
    x, sr = sf.read(audio_file)
    if x.ndim > 1:
        x = x.mean(axis=1)
    if sr_ref is None:
        sr_ref = sr
    if sr != sr_ref:
        raise RuntimeError(f"Sample-rate mismatch: {sr} vs {sr_ref}")
    x = trim_edges(np.asarray(x, dtype=np.float64), sr)
    x_new = shape_word(x, sr)
    if old_group is not None:
        gap = 0.40 if group != old_group else 0.065
        parts.append(np.zeros(round(gap * sr), dtype=np.float64))
    parts.append(x_new)
    logs.append(f"{idx} {label}: source {len(x)/sr:.3f}s, rendered {len(x_new)/sr:.3f}s, pitch {start_hz:g}->{end_hz:g}Hz")
    old_group = group

result = np.concatenate([np.zeros(int(sr_ref * .04)), *parts, np.zeros(int(sr_ref * .12))])
peak = float(np.max(np.abs(result)))
if peak > 0.94:
    result *= 0.94 / peak
path = "Infovox_Spanish_Male_Wordfalls_Slower.wav"
sf.write(path, result, sr_ref, subtype="PCM_16")
logs.append(f"Final file: {path}, {len(result)/sr_ref:.3f}s, {sr_ref} Hz, peak {peak:.4f}")
Path("render_report.txt").write_text("\n".join(logs) + "\n", encoding="utf-8")
print("\n".join(logs))
