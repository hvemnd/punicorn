#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build and run the host simulator (tools/punisim.c) and make its pictures into one PNG sheet.
  tools/punisim.py OUT.png "script" [columns] [scale]"""
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
exe = ROOT / "build" / "punisim"
srcs = [ROOT / "tools" / "punisim.c", ROOT / "web" / "emu" / "felucca_web.c", *(ROOT / "firmware" / "src").glob("*.c")]
if not exe.exists() or any(s.stat().st_mtime > exe.stat().st_mtime for s in srcs):
    if not (ROOT / "build" / "gen" / "felucca_tables.h").exists():
        sys.path.insert(0, str(ROOT / "tools"))
        import build
        build.generate()
    subprocess.run(["cc", "-O2", "-ffp-contract=off", "-w", "-Ibuild/gen", "-Ifirmware/src", "-o", str(exe),
                    "tools/punisim.c", "-lm"], cwd=ROOT, check=True)
out, script = Path(sys.argv[1]), sys.argv[2]
cols = int(sys.argv[3]) if len(sys.argv) > 3 else 6
scale = int(sys.argv[4]) if len(sys.argv) > 4 else 1
with tempfile.TemporaryDirectory() as d:
    r = subprocess.run([str(exe), d, script], capture_output=True, text=True)
    sys.stdout.write(r.stdout + r.stderr)
    if r.returncode:
        raise SystemExit(f"punisim: exit {r.returncode}")
    pics = sorted(Path(d).glob("*.ppm"))
    if pics:
        ims = [Image.open(p).convert("RGB") for p in pics]
        rows = (len(ims) + cols - 1) // cols
        sheet = Image.new("RGB", (cols * 244 - 4, rows * 244 - 4), (40, 40, 40))
        for i, im in enumerate(ims):
            sheet.paste(im, ((i % cols) * 244, (i // cols) * 244))
        if scale != 1:
            sheet = sheet.resize((sheet.width * scale, sheet.height * scale), Image.NEAREST)
        sheet.save(out)
        print(f"{len(ims)} pictures -> {out}")
    wav = Path(d) / "audio.wav"
    if wav.exists():
        out.with_suffix(".wav").write_bytes(wav.read_bytes())
