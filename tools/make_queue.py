#!/usr/bin/env python3
"""Generate queue_placeholders.json: every hook flagged `.Reversed = false`.

Each entry: addr, name, file, line, tags (VIRTUAL / SELF_REDIRECT), and the
open upstream PRs whose file/class overlaps it (Upstream First rule: those are
resolved by merge/cherry-pick, not by writing new code).
"""
import json
import os
import re
from collections import Counter

REPO = r"D:\projects\gta_sa_reversed"
SRC = os.path.join(REPO, "source")

# file-stem substring -> open upstream PRs (number, mergeable?) that may cover it
UPSTREAM = {
    "camera": [(1519, True), (1520, True), (1521, True), (1522, True), (1523, True), (1524, True)],
    "cam": [(1461, False)],
    "vehicle": [(1527, True)],
    "genericgamestorage": [(1518, True)],
    "entryexitmanager": [(1513, True)],
    "coronas": [(1504, True)],
    "fire": [(1498, True), (1335, False)],
    "curves": [(1326, True)],
    "pedgroups": [(1176, True)],
    "autopilot": [(1165, True)],
    "pedlist": [(1167, False)],
    "projectileinfo": [(1469, False), (1471, False), (1472, False)],
    "radiotrackmanager": [(1459, False), (1460, False), (1468, False)],
    "bike": [(1366, False)],
    "pedgeometryanalyser": [(1434, False)],
    "font": [(1414, False)],
    "train": [(1154, False)],
    "conversation": [(1152, False)],
    "decisionmaker": [(1143, False)],
    "monstertruck": [(1475, False), (1500, False)],
    "waterlevel": [(1499, False)],
    "collision": [(1466, False)],
    "audio": [(1225, False)],
    "script": [(1136, True), (1216, False)],
}

LINE = re.compile(r'RH_Scoped\w*Install\(\s*([~\w]+)\s*(?:,\s*"[^"]*")?\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*')
FLAG = re.compile(r'\.Reversed\s*=\s*false', re.I)


def scan_file(path, rel):
    try:
        text = open(path, "r", encoding="utf-8", errors="replace").read()
    except OSError:
        return []
    out = []
    for i, line in enumerate(text.splitlines(), 1):
        if "Install(" not in line or not FLAG.search(line):
            continue
        if line.lstrip().startswith("//"):
            continue
        m = LINE.search(line)
        if not m:
            continue
        name, addr_s = m.group(1), m.group(2)
        addr = int(addr_s, 0)
        selfredir = False
        dm = re.search(r'\b' + re.escape(name) + r'\s*\(', text)
        if dm and addr:
            end = text.find("\n}\n", dm.start())
            body = text[dm.start():end if end != -1 else dm.start() + 3000]
            selfredir = ("plugin::Call" in body) and (("%x" % addr) in body.lower())
        stem = os.path.basename(path)
        stem = os.path.splitext(stem)[0].lower()
        prs = []
        for k, v in UPSTREAM.items():
            if k in stem:
                prs = [{"pr": n, "mergeable": ok} for n, ok in v]
                break
        tags = []
        if addr == 0:
            tags.append("VIRTUAL")
        if selfredir:
            tags.append("REDIRECT_BODY")  # body is a plugin::Call stub to the original address
        out.append({
            "addr": addr, "addr_hex": "0x%X" % addr, "name": name,
            "file": rel, "line": i, "tags": tags, "upstream_prs": prs,
        })
    return out


def main():
    entries = []
    for dirpath, _, files in os.walk(SRC):
        for fn in files:
            if not fn.endswith((".cpp", ".h", ".hpp")):
                continue
            p = os.path.join(dirpath, fn)
            rel = os.path.relpath(p, REPO).replace("\\", "/")
            entries.extend(scan_file(p, rel))
    entries.sort(key=lambda e: (bool(e["tags"]), e["file"], e["addr"]))
    with open(os.path.join(REPO, "queue_placeholders.json"), "w", encoding="utf-8") as f:
        json.dump(entries, f, indent=1)
    c = Counter(e["file"] for e in entries)
    print("TOTAL:", len(entries))
    print("SELF_REDIRECT-tagged:", sum(1 for e in entries if "SELF_REDIRECT" in e["tags"]))
    print("VIRTUAL:", sum(1 for e in entries if "VIRTUAL" in e["tags"]))
    print("covered by open upstream PRs:", sum(1 for e in entries if e["upstream_prs"]))
    print("mergeable-PR covered:", sum(1 for e in entries if any(p["mergeable"] for p in e["upstream_prs"])))
    for k, v in c.most_common(20):
        print("%4d  %s" % (v, k))


if __name__ == "__main__":
    main()
