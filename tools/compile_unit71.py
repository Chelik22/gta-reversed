#!/usr/bin/env python3
"""compile_unit71.py -- compile a standalone matching unit with the PERIOD
MSVC 7.1 toolchain (cl 13.10.3077, the GTA SA compiler) and dump its .text.

Usage:
  python tools/compile_unit71.py <file.cpp> [--flags "/O2 /Ot /G6 /Ob1 /Zp4"]

Toolchain: D:\\tools\\msvc71 (archaic-msvc/msvc710 + genuine msvcr71/msvcp71
from python.org MSI / wxPython 2.8.7.1-py25 installer).

Writes <file>.obj next to the source and prints a capstone disassembly of
.text with relocations marked (relocated operands show as 'RELOC' since the
.obj holds placeholders). Compare against tools/ghidra_export.py output of
the original address for the byte-matching pass.
"""
import argparse
import os
import struct
import subprocess
import sys

TOOLCHAIN = r"D:\tools\msvc71"
BIN = os.path.join(TOOLCHAIN, "Vc7", "bin")
INCLUDE = os.path.join(TOOLCHAIN, "Vc7", "include") + ";" + os.path.join(TOOLCHAIN, "Vc7", "PlatformSDK", "Include")
DEFAULT_FLAGS = "/c /O2 /Ot /G6 /Ob1 /Zp4 /GF /GS-"

from capstone import Cs, CS_ARCH_X86, CS_MODE_32  # noqa: E402


def coff_text_and_relocs(obj_path):
    d = open(obj_path, "rb").read()
    if d[:2] != b"\x4c\x01":
        raise SystemExit("not a COFF i386 object: " + obj_path)
    nsec = struct.unpack_from("<H", d, 2)[0]
    optsz = struct.unpack_from("<H", d, 16)[0]
    symtab = struct.unpack_from("<I", d, 8)[0]
    nsyms = struct.unpack_from("<I", d, 12)[0]
    strtab = symtab + nsyms * 18

    def cstr(off):
        return d[off:d.index(b"\0", off)].decode("latin1")

    secs, relocs = {}, {}
    for i in range(nsec):
        o = 20 + optsz + i * 40
        name = d[o:o + 8].rstrip(b"\0").decode("latin1")
        if name.startswith("/"):
            name = cstr(strtab + int(name[1:]))
        vsz, va, rawsz, rawptr = struct.unpack_from("<IIII", d, o + 8)
        secs[name] = (va, rawsz, rawptr)
    for name, (sva, srawsz, srawptr) in secs.items():
        base = 20 + optsz + nsec * 40
        # relocation table follows raw data of each section
        relptr = srawptr + srawsz
        nrel = struct.unpack_from("<I", d, relptr)[0] if relptr + 4 <= len(d) else 0
        rr = []
        for k in range(nrel):
            ro = relptr + 4 + k * 10
            if ro + 10 > len(d):
                break
            offv, symv, typ = struct.unpack_from("<IIB", d, ro)
            rr.append((offv, typ))
        relocs[name] = rr
    return secs, relocs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source")
    ap.add_argument("--flags", default=DEFAULT_FLAGS)
    a = ap.parse_args()

    src = os.path.abspath(a.source)
    obj = os.path.splitext(src)[0] + ".obj"
    env = {
        "SYSTEMROOT": r"C:\WINDOWS",
        "PATH": BIN,
        "INCLUDE": INCLUDE,
        "TMP": r"D:\tools\msvc71\test",
        "TEMP": r"D:\tools\msvc71\test",
    }
    r = subprocess.run([os.path.join(BIN, "cl.exe")] + a.flags.split() + [src],
                       env=env, capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(obj):
        sys.stdout.write(r.stdout)
        sys.stderr.write(r.stderr)
        raise SystemExit("compile failed rc=%d" % r.returncode)
    if r.stdout.strip():
        sys.stdout.write(r.stdout)

    secs, relocs = coff_text_and_relocs(obj)
    if ".text" not in secs:
        raise SystemExit("no .text in " + obj)
    va, rawsz, rawptr = secs[".text"]
    d = open(obj, "rb").read()
    code = d[rawptr:rawptr + rawsz]
    print("; .text %d bytes, %d relocs" % (rawsz, len(relocs[".text"])))
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    for i in md.disasm(code, va):
        mark = " ; RELOC" if any(i.address <= off < i.address + i.size for off, _ in relocs[".text"]) else ""
        print("%08x  %-24s %s%s" % (i.address, i.mnemonic + " " + i.op_str, hex(i.bytes.__len__()), mark))
    print("; obj: " + obj)
    return 0


if __name__ == "__main__":
    sys.exit(main())
