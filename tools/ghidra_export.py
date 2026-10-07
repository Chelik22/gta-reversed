#!/usr/bin/env python3
"""Ghidra headless export for one original function.

Usage: python tools/ghidra_export.py --addr 0x00XXXXXX

Prints: decompiled C pseudocode, direct call targets (resolved names),
and .data/.rdata addresses referenced from the body (global singletons).
"""
import argparse
import os
import sys

os.environ.setdefault("GHIDRA_INSTALL_DIR", r"D:\tools\ghidra_12.1.3_PUBLIC")
import pyghidra  # noqa: E402

PROJECT_DIR = r"D:\projects\keep\ghidra"
PROJECT = "gtasa"
PROGRAM = "gta_sa.exe"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--addr", required=True, type=lambda s: int(s, 0))
    a = ap.parse_args()

    pyghidra.start()
    import jpype
    from ghidra.app.decompiler import DecompInterface
    from ghidra.util.task import ConsoleTaskMonitor

    GhidraProject = jpype.JPackage("ghidra").base.project.GhidraProject
    proj = GhidraProject.openProject(PROJECT_DIR, PROJECT, True)
    prog = proj.openProgram("/", PROGRAM, False)
    try:
        fm = prog.getFunctionManager()
        af = prog.getAddressFactory().getDefaultAddressSpace()
        addr = af.getAddress(a.addr)
        fn = fm.getFunctionContaining(addr) or fm.getFunctionAt(addr)
        if fn is None:
            from ghidra.app.cmd.function import CreateFunctionCmd
            CreateFunctionCmd(addr).applyTo(prog, ConsoleTaskMonitor())
            fn = fm.getFunctionAt(addr)
        if fn is None:
            print("NO FUNCTION at", hex(a.addr))
            return 1
        print("// FUNCTION: %s @ %s  size=%d" % (fn.getName(), fn.getEntryPoint(), fn.getBody().getNumAddresses()))
        di = DecompInterface()
        di.openProgram(prog)
        res = di.decompileFunction(fn, 120, ConsoleTaskMonitor())
        if res.decompileCompleted():
            print(res.getDecompiledFunction().getC())
        else:
            print("// decompile failed:", res.getErrorMessage())

        called = {}
        data_refs = set()
        for insn in prog.getListing().getInstructions(fn.getBody(), True):
            for ref in insn.getReferencesFrom():
                to = ref.getToAddress()
                if to is None:
                    continue
                tf = fm.getFunctionAt(to)
                if tf is not None:
                    called[str(to)] = tf.getName()
                else:
                    data_refs.add(str(to))
        print("\n// ---- CALLED FUNCTIONS ----")
        for k in sorted(called):
            print("//   %s  %s" % (k, called[k]))
        print("\n// ---- DATA REFS (globals) ----")
        for k in sorted(data_refs):
            try:
                sym = prog.getSymbolTable().getPrimarySymbol(af.getAddress(int(k, 16)))
            except (ValueError, TypeError):
                sym = None  # Ghidra sometimes emits 'Stack[-0x10]' pseudo-refs
            print("//   %s  %s" % (k, sym.getName() if sym else "?"))
        return 0
    finally:
        proj.close(prog)
        proj.close()


if __name__ == "__main__":
    sys.exit(main())
