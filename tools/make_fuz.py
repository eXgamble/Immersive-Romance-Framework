"""One generated line -> a Skyrim .fuz: 16-bit mono WAV -> xWMA (Creation Kit's xWMAEncode) + .lip (CK LipGenerator), packed.
Usage: make_fuz.py <audio> <text> <out.fuz>"""
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

FFMPEG = r"C:\ProgramData\HP\LCDDisplayHelper\bin\ffmpeg.exe"
TOOLS = Path(r"C:\Steam\steamapps\common\Skyrim Special Edition\Tools\LipGen\LipGenerator")

audio, text, out = Path(sys.argv[1]), sys.argv[2], Path(sys.argv[3])
with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    wav = tmp / "line.wav"
    subprocess.run([FFMPEG, "-y", "-loglevel", "error", "-i", str(audio), "-ar", "44100", "-ac", "1", "-sample_fmt", "s16", str(wav)], check=True)
    xwm = tmp / "line.xwm"
    subprocess.run([str(TOOLS / "xWMAEncode.exe"), "-b", "48000", str(wav), str(xwm)], check=True, capture_output=True)
    # LipGenerator wants FonixData.cdf beside it: run it in its own folder
    r = subprocess.run([str(TOOLS / "LipGenerator.exe"), str(wav), text, "-Language:USEnglish"], cwd=TOOLS, capture_output=True, text=True)
    lip = wav.with_suffix(".lip")
    if not lip.exists():
        sys.exit(f"no .lip made: {r.stdout} {r.stderr}")
    lip_bytes, xwm_bytes = lip.read_bytes(), xwm.read_bytes()
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(b"FUZE" + struct.pack("<II", 1, len(lip_bytes)) + lip_bytes + xwm_bytes)
    print(f"{out.name}: lip {len(lip_bytes)} B, xwm {len(xwm_bytes)} B")
