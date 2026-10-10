#!/usr/bin/env python3
"""Apply INT-wave integration outputs (final_*.cpp/.h) to the repo.

Mapping strategy: explicit table first, then fuzzy basename match against
the known target files of this wave. Anything ambiguous is REPORTED, not guessed
(lesson from wave-4: FurnitureSubGroup_c.cpp underscore != path separator).

Usage:
  python tools/apply_int1.py            # dry-run: show mapping
  python tools/apply_int1.py --apply    # copy files
"""
import os
import shutil
import sys

INTEGRATION_DIR = r"D:\projects\keep\batch_out\_integration"
REPO = r"D:\projects\gta_sa_reversed"

# Explicit map for INT1 (20 files / 28 functions)
EXPLICIT = {
    "final_Audio_Entities_AEPedAudioEntity.cpp": "source/game_sa/Audio/Entities/AEPedAudioEntity.cpp",
    "final_Audio_Entities_AEVehicleAudioEntity.cpp": "source/game_sa/Audio/Entities/AEVehicleAudioEntity.cpp",
    "final_Audio_Hardware_AEStreamingChannel.cpp": "source/game_sa/Audio/Hardware/AEStreamingChannel.cpp",
    "final_Audio_Hardware_AEStreamingChannel.h": "source/game_sa/Audio/Hardware/AEStreamingChannel.h",
    "final_Conversations_ConversationForPed.cpp": "source/game_sa/Conversations/ConversationForPed.cpp",
    "final_CarCtrl.cpp": "source/game_sa/CarCtrl.cpp",
    "final_Entity_Ped_PlayerPed.cpp": "source/game_sa/Entity/Ped/PlayerPed.cpp",
    "final_Entity_Vehicle_Vehicle.cpp": "source/game_sa/Entity/Vehicle/Vehicle.cpp",
    "final_EntityScanner.cpp": "source/game_sa/EntityScanner.cpp",
    "final_DecisionMakers_Decision.cpp": "source/game_sa/DecisionMakers/Decision.cpp",
    "final_Rope.cpp": "source/game_sa/Rope.cpp",
    "final_Ropes.cpp": "source/game_sa/Ropes.cpp",
    "final_ScriptResourceManager.cpp": "source/game_sa/ScriptResourceManager.cpp",
    "final_WeaponEffects.cpp": "source/game_sa/WeaponEffects.cpp",
    "final_Plugins_RpAnimBlendPlugin_RpAnimBlend.cpp": "source/game_sa/Plugins/RpAnimBlendPlugin/RpAnimBlend.cpp",
    "final_Tasks_Allocators_PedGroup_PedGroupDefaultTaskAllocatorSitInLeaderCar.h":
        "source/game_sa/Tasks/Allocators/PedGroup/PedGroupDefaultTaskAllocatorSitInLeaderCar.h",
    "final_Tasks_Allocators_TaskAllocatorKillThreatsBasic.cpp":
        "source/game_sa/Tasks/Allocators/TaskAllocatorKillThreatsBasic.cpp",
    "final_Tasks_TaskTypes_TaskComplexDestroyCar.cpp": "source/game_sa/Tasks/TaskTypes/TaskComplexDestroyCar.cpp",
    "final_Tasks_TaskTypes_TaskComplexDragPedFromCar.cpp": "source/game_sa/Tasks/TaskTypes/TaskComplexDragPedFromCar.cpp",
    "final_Tasks_TaskTypes_TaskSimpleDead.cpp": "source/game_sa/Tasks/TaskTypes/TaskSimpleDead.cpp",
    "final_Tasks_TaskTypes_TaskSimpleFightingControl.cpp": "source/game_sa/Tasks/TaskTypes/TaskSimpleFightingControl.cpp",
}

# Repo basenames for fuzzy fallback (basename without extension -> repo relpath)
FUZZY_BASES = {}


def build_fuzzy():
    for root, _dirs, files in os.walk(os.path.join(REPO, "source")):
        for f in files:
            if f.endswith((".cpp", ".h")):
                FUZZY_BASES.setdefault(f, os.path.relpath(os.path.join(root, f), REPO).replace("\\", "/"))


def map_file(name: str):
    if name in EXPLICIT:
        return EXPLICIT[name]
    # fuzzy: final_<flat> -> try to find repo file whose basename matches the tail of flat name
    stem = name[len("final_"):]
    for suffix_len in range(len(stem), 4, -1):
        cand = stem[-suffix_len:] if suffix_len != len(stem) else stem
        if "." in cand:
            key = cand
            if key in FUZZY_BASES:
                return FUZZY_BASES[key]
    return None


def main():
    apply = "--apply" in sys.argv
    build_fuzzy()
    files = sorted(f for f in os.listdir(INTEGRATION_DIR) if f.startswith("final_") and f.endswith((".cpp", ".h")))
    if not files:
        print("No final_* outputs yet.")
        return
    ok, manual, done_before = [], [], []
    for f in files:
        src = os.path.join(INTEGRATION_DIR, f)
        if os.path.getsize(src) < 200:
            manual.append((f, f"SUSPICIOUS: only {os.path.getsize(src)}B"))
            continue
        rel = map_file(f)
        if not rel:
            manual.append((f, "NO MAPPING — map manually"))
            continue
        dst = os.path.join(REPO, rel)
        if not os.path.exists(os.path.dirname(dst)):
            manual.append((f, f"DIR MISSING: {rel}"))
            continue
        if os.path.exists(dst) and open(src, "rb").read() == open(dst, "rb").read():
            done_before.append((f, rel))
            continue
        ok.append((f, rel, os.path.getsize(src)))
    print(f"== {len(ok)} to apply, {len(done_before)} already applied, {len(manual)} need attention ==")
    for f, rel, sz in ok:
        print(f"  {f} ({sz}B) -> {rel}")
        if apply:
            shutil.copy2(os.path.join(INTEGRATION_DIR, f), os.path.join(REPO, rel))
    for f, rel in done_before:
        print(f"  .. {f} = identical to repo ({rel}) — skipped")
    for f, why in manual:
        print(f"  !! {f}: {why}")
    if not apply:
        print("\n(dry-run; pass --apply to copy)")


if __name__ == "__main__":
    main()
