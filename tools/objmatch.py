#!/usr/bin/env python3
"""objmatch.py -- byte-compare a VC7.1 matching unit's .text against the
original gta_sa.exe function.

Usage: python tools/objmatch.py --obj unit.obj --addr 0x5EAF60 [--size N]

Relocated 4-byte slots in the .obj (external refs: calls, globals) hold
placeholders and are masked out of the comparison (the sprint counts a match
as "100% modulo relocations"). Prints per-offset verdicts and a diff when
bytes diverge.
"""
import argparse
import struct
import sys

from capstone import Cs, CS_ARCH_X86, CS_MODE_32

EXE = r"D:\projects\keep\ghidra\gta_sa.exe"
EXE_BASE = 0x400000


def load_exe():
    d = open(EXE, "rb").read()
    e = struct.unpack_from("<I", d, 0x3C)[0]
    nsec = struct.unpack_from("<H", d, e + 6)[0]
    optsz = struct.unpack_from("<H", d, e + 20)[0]
    secs = []
    for i in range(nsec):
        o = e + 24 + optsz + i * 40
        name = d[o:o + 8].rstrip(b"\0").decode("latin1")
        vsz, va, rawsz, rawptr = struct.unpack_from("<IIII", d, o + 8)
        secs.append((name, va, max(vsz, rawsz), rawsz, rawptr))
    return d, secs


def read_va(d, secs, va, n):
    rva = va - EXE_BASE
    for name, sva, ssz, rawsz, rawptr in secs:
        if sva <= rva < sva + ssz:
            off = rawptr + (rva - sva)
            return d[off:off + n]
    return None


def coff_text(obj_path):
    d = open(obj_path, "rb").read()
    if d[:2] != b"\x4c\x01":
        raise SystemExit("not COFF i386: " + obj_path)
    nsec = struct.unpack_from("<H", d, 2)[0]
    optsz = struct.unpack_from("<H", d, 16)[0]
    symtab = struct.unpack_from("<I", d, 8)[0]
    nsyms = struct.unpack_from("<I", d, 12)[0]
    secs = {}
    for i in range(nsec):
        o = 20 + optsz + i * 40
        name = d[o:o + 8].rstrip(b"\0").decode("latin1")
        vsz, va, rawsz, rawptr = struct.unpack_from("<IIII", d, o + 8)
        secs[name] = (rawsz, rawptr)
    if ".text" not in secs:
        raise SystemExit("no .text")
    rawsz, rawptr = secs[".text"]
    code = d[rawptr:rawptr + rawsz]
    # relocations live right after the section's raw data
    relptr = rawptr + rawsz
    nrel = struct.unpack_from("<I", d, relptr)[0]
    mask = [False] * len(code)
    for k in range(nrel):
        ro = relptr + 4 + k * 10
        if ro + 10 > len(d):
            break
        offv, symv, typ = struct.unpack_from("<IIB", d, ro)
        for b in range(offv, min(offv + 4, len(code))):
            mask[b] = True
    return code, mask


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--obj", required=True)
    ap.add_argument("--addr", required=True, type=lambda s: int(s, 0))
    ap.add_argument("--size", type=int, default=None)
    a = ap.parse_args()

    ours, mask = coff_text(a.obj)
    d, secs = load_exe()
    size = a.size or len(ours)
    orig = read_va(d, secs, a.addr, size)
    if orig is None:
        raise SystemExit("cannot read exe at %#x" % a.addr)

    n = min(len(ours), len(orig))
    mism, masked = [], 0
    for i in range(n):
        if mask[i]:
            masked += 1
            continue
        if ours[i] != orig[i]:
            mism.append(i)
    total = n - masked
    pct = 100.0 * (total - len(mism)) / total if total else 0.0
    print("obj .text %d bytes vs orig %d bytes | masked reloc bytes: %d" % (len(ours), len(orig), masked))
    print("MATCH: %.1f%% (%d/%d non-reloc bytes)%s" % (pct, total - len(mism), total,
          "  <= BYTE-IDENTICAL (modulo relocs)" if not mism else ""))
    if mism:
        md = Cs(CS_ARCH_X86, CS_MODE_32)
        od = list(md.disasm(orig[:n], a.addr))
        # print context around first few mismatches
        shown = 0
        for i in mism:
            if shown >= 6:
                print("... %d more mismatched bytes" % (len(mism) - 6))
                break
            ctx = max(0, i - 8)
            print("-- mismatch at obj offset %#x (orig %#x)" % (i, a.addr + i))
            for ins in od:
                if ctx <= ins.address - a.addr < i + 8:
                    mark = " <== " if ins.address - a.addr <= i < ins.address - a.addr + ins.size else ""
                    print("   %08X %-8s %-30s | ours %s%s" % (ins.address, ins.mnemonic, ins.op_str,
                                                              ours[ins.address - a.addr:ins.address - a.addr + ins.size].hex(), mark))
            shown += 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
