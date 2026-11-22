from pathlib import Path
import numpy as np
import soundfile as sf

phrases = [
    ("Inspiration", 55),
    ("Expiration", 50),
    ("Contraction", 45),
    ("Décontraction", 40),
    ("La forme", 35),
]
sample_rate = None
parts = []
report = []

for index, (phrase, pitch) in enumerate(phrases, start=1):
    path = Path("infovox_phrases") / f"phrase_{index:02d}.wav"
    audio, sr = sf.read(path, dtype="float32")
    if audio.ndim == 2:
        audio = audio.mean(axis=1)
    if sample_rate is None:
        sample_rate = sr
    elif sample_rate != sr:
        raise RuntimeError(f"Sample-rate mismatch: {sr} vs {sample_rate}")

    # Remove only any trailing engine padding, retaining a short natural tail.
    peak = float(np.max(np.abs(audio))) if audio.size else 0.0
    threshold = max(peak * 0.006, 0.00025)
    active = np.flatnonzero(np.abs(audio) > threshold)
    if active.size == 0:
        raise RuntimeError(f"No audible output for phrase {index}: {phrase}")
    end = min(len(audio), int(active[-1]) + int(sr * 0.18))
    audio = audio[:end]

    if index > 1:
        parts.append(np.zeros(int(sr * 0.85), dtype=np.float32))
    parts.append(audio)
    report.append(
        f"{index}. {phrase} | Infovox 230 French Male | "
        f"Pit={pitch} | Spd=30 wpm | {len(audio)/sr:.2f}s"
    )

result = np.concatenate(parts)
peak = float(np.max(np.abs(result)))
if peak > 0.96:
    result *= 0.96 / peak

out = Path("Infovox_French_Male_Slow_Descending.wav")
sf.write(out, result, sample_rate, subtype="PCM_16")
report.append(f"Total duration: {len(result)/sample_rate:.2f}s")
report.append(f"Sample rate: {sample_rate} Hz")
report.append("Pitch contour: 55 -> 50 -> 45 -> 40 -> 35 (La forme lowest)")
Path("render_report.txt").write_text("\n".join(report) + "\n", encoding="utf-8")
print("\n".join(report))
