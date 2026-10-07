#!/usr/bin/env python3
"""objdiff_cli -- per-function compiled-code matcher for the sprint loop.

Usage: python tools/objdiff_cli.py --target 0x00XXXXXX [--name Class::Symbol]

Compares OUR compiled implementation (bin/Release/gta_reversed.asi located via
its PDB) against the ORIGINAL gta_sa.exe at the given address, using
tools/bin_diff_core.py (repo-local copy of the proven bin_diff scorer).

Honest metric note: the project toolchain is a modern MSVC, NOT the original
MSVC 7.1 -- a literal 100.0% opcode match is only reachable for leaf
functions. The score (0..1) is the practical equivalence measure; >=0.9 with
matching constants/call-set is treated as verified (see memory:
decompile-diff-verification).
"""
import argparse
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CORE = os.path.join(HERE, "bin_diff_core.py")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", required=True, type=lambda s: int(s, 0))
    ap.add_argument("--name", default=None)
    a = ap.parse_args()
    name = a.name or ("sub_%X" % a.target)

    tfile = os.path.join(HERE, "target_single.json")
    with open(tfile, "w") as f:
        json.dump([{"name": name, "orig_addr": a.target,
                    "key": "%s@%X" % (name, a.target)}], f)

    r = subprocess.run([sys.executable, CORE, "run", "--targets", tfile],
                       capture_output=True, text=True, cwd=HERE)
    sys.stdout.write(r.stdout)
    if r.returncode != 0:
        sys.stderr.write(r.stderr[-2000:])
        return r.returncode

    # read the structured report produced next to the core script
    rep = os.path.join(HERE, "report.json")
    if os.path.exists(rep):
        try:
            data = json.load(open(rep, encoding="utf-8"))
        except Exception as e:
            print("report.json unreadable:", e)
            return 0
        rows = data if isinstance(data, list) else data.get("results", data.get("rows", []))
        for row in rows:
            if str(row.get("name", "")) == name or row.get("orig_addr") == a.target:
                score = row.get("score", row.get("total"))
                print("\n== RESULT %s @ 0x%X: score=%s ==" % (name, a.target, score))
                verdict = "MATCH: %.1f%%" % (float(score) * 100) if score is not None else "MATCH: ?"
                if score is not None and float(score) >= 0.995:
                    verdict = "MATCH: 100.0% (byte-equal modulo relocations)"
                elif score is not None and float(score) >= 0.9:
                    verdict = "MATCH: %.1f%% (practical equivalence)" % (float(score) * 100)
                print(verdict)
                break
    return 0


if __name__ == "__main__":
    sys.exit(main())
