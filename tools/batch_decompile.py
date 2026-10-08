#!/usr/bin/env python3
"""Batch-export Ghidra decompiles for wave-1 sprint functions (one JVM)."""
import json
import os
import sys
sys.setrecursionlimit(100000)

os.environ.setdefault("GHIDRA_INSTALL_DIR", r"D:\tools\ghidra_12.1.3_PUBLIC")
import pyghidra

WAVE_FILES = [
    "Interior_c.cpp", "InteriorGroup_c.cpp", "PedGeometryAnalyser.cpp", "Train.cpp",
    "RunningScript.cpp", "AEScriptAudioEntity.cpp", "TaskComplexSmartFleePoint.cpp",
    "Plane.cpp", "ClothesBuilder.cpp", "Hud.cpp", "TaskComplexHitPedWithCar.cpp",
    "Shadows.cpp", "FxEmitterBP.cpp",
]
OUT = r"D:\projects\keep\decompiles"

q = json.load(open(r"D:\projects\gta_sa_reversed\queue_placeholders.json"))
targets = [e for e in q if e["file"].split("/")[-1] in WAVE_FILES]
print("wave-1 targets:", len(targets), flush=True)

pyghidra.start()
import jpype
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

GhidraProject = jpype.JPackage("ghidra").base.project.GhidraProject
proj = GhidraProject.openProject(r"D:\projects\keep\ghidra", "gtasa", True)
prog = proj.openProgram("/", "gta_sa.exe", False)
fm = prog.getFunctionManager()
af = prog.getAddressFactory().getDefaultAddressSpace()
di = DecompInterface()
di.openProgram(prog)

os.makedirs(OUT, exist_ok=True)
ok = fail = 0
for i, t in enumerate(targets):
    addr = af.getAddress(t["addr"])
    fn = fm.getFunctionAt(addr) or fm.getFunctionContaining(addr)
    if fn is None:
        from ghidra.app.cmd.function import CreateFunctionCmd
        CreateFunctionCmd(addr).applyTo(prog, ConsoleTaskMonitor())
        fn = fm.getFunctionAt(addr)
    if fn is None:
        print("NOFN", t["addr_hex"], flush=True)
        fail += 1
        continue
    res = di.decompileFunction(fn, 120, ConsoleTaskMonitor())
    body = res.getDecompiledFunction().getC() if res.decompileCompleted() else "// DECOMPILE_FAILED: " + res.getErrorMessage()
    called = []
    for insn in prog.getListing().getInstructions(fn.getBody(), True):
        for ref in insn.getReferencesFrom():
            to = ref.getToAddress()
            if to is None:
                continue
            tf = fm.getFunctionAt(to)
            if tf is not None:
                called.append("%s %s" % (to, tf.getName()))
    with open(os.path.join(OUT, t["addr_hex"] + ".c"), "w", encoding="utf-8") as f:
        f.write("// %s @ %s (%s:%d)%s\n// CALLED: %s\n\n%s\n" % (
            t["name"], t["addr_hex"], t["file"], t["line"],
            " [IN_STASH: " + t["in_stash"] + "]" if t.get("in_stash") else "",
            ", ".join(sorted(set(called))[:25]), body))
    ok += 1
    if (i + 1) % 20 == 0:
        print("[%d/%d]" % (i + 1, len(targets)), flush=True)
print("DONE ok=%d fail=%d" % (ok, fail), flush=True)
proj.close(prog)
proj.close()
