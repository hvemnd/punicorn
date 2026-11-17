#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The biggest symbols of the firmware, from `objdump -t` (the build workflow adds this to its log).
  sym_sizes.py SYMS.txt [N]"""
import re
import sys

rows = []
for ln in open(sys.argv[1], errors="replace"):
    p = ln.split()
    if len(p) < 4:
        continue
    hexes = [x for x in p[:-1] if re.fullmatch(r"[0-9a-fA-F]{8,16}", x)]
    if len(hexes) < 2:
        continue
    sect = next((x for x in p if x.startswith(".")), "?")
    rows.append((int(hexes[-1], 16), sect, p[-1]))
rows.sort(reverse=True)
n = int(sys.argv[2]) if len(sys.argv) > 2 else 260
tot = {}
for size, sect, name in rows:
    tot[sect] = tot.get(sect, 0) + size
print("---- symbol bytes per section:", ", ".join(f"{k} {v}" for k, v in sorted(tot.items())))
print("---- biggest symbols")
for size, sect, name in rows[:n]:
    print(f"{size:8d} {sect:10s} {name}")
kid = sum(s for s, _, nm in rows if re.match(r"(kid|kw|KID|KW|puni|pg_|PUNI|un_|sc_)", nm))
print(f"---- kid/puni symbols: {kid} B")
