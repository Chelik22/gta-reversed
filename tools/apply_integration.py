#!/usr/bin/env python3
"""Apply integration agent outputs: copy new_*.cpp files to repo, build, commit.

Usage: python tools/apply_integration.py [--dry-run]
"""
import os
import shutil
import subprocess
import sys

INTEGRATION_DIR = r"D:\projects\keep\batch_out\_integration"
REPO = r"D:\projects\gta_sa_reversed"

# Map: output filename → repo path
TARGETS = {
    "new_Weapon.cpp": "source/game_sa/Weapon.cpp",
    "new_ProjectileInfo.cpp": "source/game_sa/ProjectileInfo.cpp",
    "new_TaskComplexPlayHandSignalAnim.cpp": "source/game_sa/Tasks/TaskTypes/TaskComplexPlayHandSignalAnim.cpp",
    "new_AERadioTrackManager.cpp": "source/game_sa/Audio/Managers/AERadioTrackManager.cpp",
    "new_Bike.cpp": "source/game_sa/Entity/Vehicle/Bike.cpp",
    "new_Ped.cpp": "source/game_sa/Ped.cpp",
    "new_InteriorGroup_c.cpp": "source/game_sa/Interior/InteriorGroup_c.cpp",
    "new_ClothesBuilder.cpp": "source/game_sa/ClothesBuilder.cpp",
    "new_Hud.cpp": "source/game_sa/Hud.cpp",
    "new_Plane.cpp": "source/game_sa/Entity/Vehicle/Plane.cpp",
    "new_FxEmitterBP.cpp": "source/game_sa/Fx/FxEmitterBP.cpp",
    "new_RunningScript.cpp": "source/game_sa/Scripts/RunningScript.cpp",
    "new_Train.cpp": "source/game_sa/Entity/Vehicle/Train.cpp",
    "new_TaskComplexSmartFleePoint.cpp": "source/game_sa/Tasks/TaskTypes/TaskComplexSmartFleePoint.cpp",
}


def main():
    dry_run = "--dry-run" in sys.argv
    applied = []
    skipped = []

    for new_file, repo_path in sorted(TARGETS.items()):
        src = os.path.join(INTEGRATION_DIR, new_file)
        dst = os.path.join(REPO, repo_path)

        if not os.path.exists(src):
            skipped.append((new_file, "output not found"))
            continue

        # Check the file is not empty and has actual content
        size = os.path.getsize(src)
        if size < 100:
            skipped.append((new_file, f"too small ({size}B)"))
            continue

        # Check it has function definitions (not just comments)
        content = open(src, encoding="utf-8", errors="replace").read()
        if "plugin::Call" not in content and "InjectHooks" not in content:
            skipped.append((new_file, "no functions/hooks found"))
            continue

        if dry_run:
            print(f"WOULD APPLY: {new_file} -> {repo_path} ({size}B)")
            applied.append(new_file)
        else:
            shutil.copy2(src, dst)
            print(f"APPLIED: {new_file} -> {repo_path} ({size}B)")
            applied.append(new_file)

    print(f"\n=== {len(applied)} applied, {len(skipped)} skipped ===")
    for name, reason in skipped:
        print(f"  SKIP: {name} ({reason})")

    if not dry_run and applied:
        print("\nBuilding...")
        result = subprocess.run(
            ["cmake", "--build", "build", "--config", "Release"],
            cwd=REPO, capture_output=True, text=True, timeout=600
        )
        errors = result.stdout.count("error C") + result.stderr.count("error C")
        print(f"Build: {errors} errors")
        if errors == 0:
            print("GREEN! Ready to commit.")
        else:
            # Show first few errors
            for line in (result.stdout + result.stderr).split("\n"):
                if "error C" in line:
                    print(f"  {line.strip()[:100]}")
                    if len([l for l in [line] if "error C" in l]) > 5:
                        break

    return 0


if __name__ == "__main__":
    sys.exit(main())
