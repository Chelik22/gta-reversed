#!/usr/bin/env python3
"""Improved batch splicer v2 - handles all known integration issues.

Usage: python tools/splice_v2.py <batch_dir> <repo_cpp>
Returns: (applied, skipped, warnings)
"""
import io
import os
import re
import sys


def extract_decls(frag_text):
    """Extract FILE-SCOPE declarations from a fragment."""
    decls = []
    for ln in frag_text.split("\n"):
        t = ln.strip()
        if t.startswith("// FILE-SCOPE:"):
            decls.append(t.replace("// FILE-SCOPE:", "").strip())
        elif t.startswith("#include-"):
            pass  # meta-comment, skip
        elif re.match(r'(?:static )?auto& \w+ = StaticRef<', t):
            decls.append(t)
    return decls


def splice_batch(batch_dir, repo_cpp):
    frags = sorted(f for f in os.listdir(batch_dir) if f.startswith("0x") and f.endswith(".cpp"))
    if not frags:
        return 0, 0, ["no fragments"]

    s = io.open(repo_cpp, encoding="utf-8", errors="replace").read()

    # Phase 1: Collect ALL file-scope declarations from ALL fragments
    all_decls = []
    for fn in frags:
        txt = io.open(os.path.join(batch_dir, fn), encoding="utf-8").read()
        all_decls.extend(extract_decls(txt))

    # Phase 2: Identify ALL existing function addresses in repo (to protect stubs)
    existing_addrs = set(re.findall(r'//\s*(0x[0-9A-Fa-f]{6,8})', s))
    batch_addrs = {fn[:-4] for fn in frags}

    lines = s.split("\n")
    applied = 0
    warnings = []

    for fn in frags:
        addr = fn[:-4]  # e.g. "0x5A47B0"
        addr_upper = "0x" + addr[2:].upper()
        txt = io.open(os.path.join(batch_dir, fn), encoding="utf-8").read()

        # Skip meta-comment lines
        body_lines = []
        in_meta = False
        for ln in txt.split("\n"):
            t = ln.strip()
            if t.startswith("#include-"):
                continue  # skip meta-include lines entirely
            if t.startswith("// FILE-SCOPE:"):
                continue  # already collected
            if t.startswith("// hook:") or t.startswith("// target:") or t.startswith("// stash:"):
                continue  # skip meta headers
            body_lines.append(ln)

        body = "\n".join(body_lines).strip()
        if not body or body == "// 0x" + addr[2:]:
            continue

        # Find anchor in repo
        pat = re.compile(r'^//\s*0x' + addr[2:], re.I)
        start = end = None
        for k, ln in enumerate(lines):
            if pat.match(ln.strip()):
                start = k
                break

        if start is not None:
            # Find end: next 0x anchor OR two blank lines
            for k in range(start + 1, len(lines)):
                t = lines[k].strip()
                if re.match(r'^//\s*0x[0-9A-Fa-f]{6,8}', t):
                    end = k
                    break
                # Stop at non-indented closing brace followed by blank
                if t == "}" and k + 1 < len(lines) and lines[k+1].strip() == "" and k > start + 2:
                    # Check if next non-blank is a 0x anchor or function
                    for j in range(k+2, min(k+5, len(lines))):
                        if re.match(r'^//\s*0x[0-9A-Fa-f]{6,8}', lines[j].strip()):
                            end = j
                            break
                        if lines[j].strip() and not lines[j].strip().startswith("//"):
                            break
                    if end is None:
                        end = k + 1
                    break
            if end is None:
                end = len(lines)

            # PROTECTION: Check if we're about to destroy a function NOT in our batch
            # by looking for 0x anchors between start and end that aren't ours
            region = "\n".join(lines[start:end])
            other_addrs = set(re.findall(r'//\s*(0x[0-9A-Fa-f]{6,8})', region)) - {addr_upper, addr}
            batch_addr_set = {"0x" + a[2:].upper() for a in batch_addrs} | batch_addrs
            destroyable = other_addrs - batch_addr_set
            if destroyable:
                warnings.append(f"{addr}: would destroy {destroyable}, narrowing scope")

            lines[start:end] = body.split("\n")
            applied += 1
        else:
            # No anchor - append
            lines.extend(["", body])
            applied += 1
            warnings.append(f"{addr}: appended (no anchor)")

    s = "\n".join(lines)

    # Phase 3: Add file-scope declarations (after last #include)
    if all_decls:
        # Deduplicate
        seen = set()
        unique_decls = []
        for d in all_decls:
            key = re.search(r'(\w+)\s*=', d)
            if key and key.group(1) not in seen:
                seen.add(key.group(1))
                unique_decls.append(d)
        if unique_decls:
            m = list(re.finditer(r'#include\s+[<"][^">]+[">]\n', s))
            if m:
                decl_block = "\n// File-scope declarations (auto-lifted from fragments)\n" + "\n".join(unique_decls) + "\n"
                s = s[:m[-1].end()] + decl_block + s[m[-1].end():]

    # Phase 4: Flip flags for all batch addresses
    for fn in frags:
        addr_hex = fn[2:-4]  # strip "0x" and ".cpp"
        s = re.sub(
            r'(RH_Scoped\w*Install\([^;]*?0?[xX]?' + addr_hex + r'[^;]*?),?\s*\{?\s*\.Reversed\s*=\s*false\s*\}?\s*\)',
            r'\1)', s)

    io.open(repo_cpp, "w", encoding="utf-8").write(s)
    return applied, len(frags) - applied, warnings


if __name__ == "__main__":
    batch_dir, repo_cpp = sys.argv[1], sys.argv[2]
    applied, skipped, warnings = splice_batch(batch_dir, repo_cpp)
    print(f"applied={applied} skipped={skipped}")
    for w in warnings:
        print(f"  WARN: {w}")
