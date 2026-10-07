#!/usr/bin/env python3
"""Add /DEBUG to the Release|Win32 linker flags of the generated vcxproj.

setup.py full rebuilds regenerate the project and wipe the /DEBUG patch,
leaving a public-symbol-only PDB. Re-run this after every full rebuild when a
symbol-rich PDB is needed (bin_diff/objdiff_cli depend on it), then relink:
    python tools/patch_debug_link.py
    cmake --build build --config Release
"""
import glob
import io
import re
import sys


def find_vcproj():
    hits = glob.glob(r"D:\projects\gta_sa_reversed\build\**\gta_reversed.vcxproj", recursive=True)
    return hits[0] if hits else None


def main():
    path = find_vcproj()
    if not path:
        print("vcxproj not found under build/")
        return 1
    s = io.open(path, encoding="utf-8").read()
    grp = re.search(
        r'<ItemDefinitionGroup[^>]*Condition="[^"]*Release\|Win32[^"]*".*?</ItemDefinitionGroup>',
        s, re.S)
    if not grp:
        print("Release|Win32 ItemDefinitionGroup not found")
        return 1
    block = grp.group(0)
    if "GenerateDebugInformation" in block:
        new_block = re.sub(r"<GenerateDebugInformation>[^<]*</GenerateDebugInformation>",
                           "<GenerateDebugInformation>true</GenerateDebugInformation>", block)
    else:
        new_block = block.replace("</Link>",
                                  "      <GenerateDebugInformation>true</GenerateDebugInformation>\n    </Link>", 1)
    if new_block == block:
        print("already patched:", path)
        return 0
    s = s[:grp.start()] + new_block + s[grp.end():]
    io.open(path, "w", encoding="utf-8").write(s)
    print("patched /DEBUG into", path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
