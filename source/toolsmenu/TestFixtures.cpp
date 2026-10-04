#include "StdInc.h"

#include "DebugModules/DebugModules.h"
#include "DebugModules/Script/MissionDebugModule.h"
#include "UIRenderer.h"
#include "Entity/Ped/Ped.h"
#include "Entity/Vehicle/Vehicle.h"
#include "Entity/Object/Object.h"
#include "Enums/ePedType.h"
#include "Explosion.h"
#include "Audio/AudioEngine.h"
#include "Wanted.h"
#include <algorithm>

// NOTSA(test): exported test fixtures for the headless test rig
// (D:\projects\gta_sa_tester). Local-only, never ship in PRs.

// Deferred mission start: only raises the module's flags here; the actual
// StartMission() runs on the main thread inside RenderWindow() (that is why
// m_IsOpen is raised too - the deferred start is processed there).
extern "C" __declspec(dllexport) void __cdecl TestStartMission(int missionId) {
    auto* mod = notsa::ui::UIRenderer::GetSingleton().GetDebugModules().GetModule<MissionDebugModule>();
    if (!mod) {
        return;
    }
    mod->TestRaiseStartFlag(missionId);
}

// ---------------------------------------------------------------- game state
struct TestGameState {
    DWORD  playerPed;
    DWORD  vehicle;
    DWORD  onFoot;
    DWORD  health;      // 0..100
    DWORD  armor;
    DWORD  weaponType;  // active weapon type id
    DWORD  wantedLevel; // 0..6
    float  x, y, z;
};

extern "C" __declspec(dllexport) void __cdecl TestGetState(TestGameState* out) {
    memset(out, 0, sizeof(*out));
    CPlayerPed* ped = FindPlayerPed();
    if (!ped) {
        return;
    }
    out->playerPed = (DWORD)ped;
    const CVector pos = ped->GetPosition();
    out->x = pos.x; out->y = pos.y; out->z = pos.z;
    out->health = (DWORD)ped->m_fHealth;
    out->armor  = 0; // armor member not exposed on CPlayerPed - skip
    out->weaponType = (DWORD)ped->GetActiveWeapon().m_Type;
    if (CVehicle* veh = ped->GetVehicleIfInOne()) {
        out->vehicle = (DWORD)veh;
        out->onFoot  = 0;
    } else {
        out->onFoot = 1;
    }
    if (CWanted* w = ped->GetPlayerWanted()) {
        out->wantedLevel = (DWORD)w->GetWantedLevel();
    }
}

// ---------------------------------------------------------------- domain fixtures
extern "C" __declspec(dllexport) void __cdecl TestExplodeAt(float x, float y, float z) {
    CExplosion::AddExplosion(nullptr, nullptr, EXPLOSION_GRENADE, {x, y, z}, 0u, true, 70.0f, true);
}

extern "C" __declspec(dllexport) void __cdecl TestSetWanted(int level) {
    if (CPlayerPed* ped = FindPlayerPed()) {
        if (CWanted* w = ped->GetPlayerWanted()) {
            w->SetWantedLevel((eWantedLevel)std::clamp(level, 0, 6));
        }
    }
}

extern "C" __declspec(dllexport) void __cdecl TestAudioEvent(int soundId) {
    CVector pos{0.f, 0.f, 0.f};
    AudioEngine.ReportMissionAudioEvent((uint16)soundId, pos);
}

extern "C" __declspec(dllexport) void __cdecl TestSpawnPed(int modelId) {
    CPlayerPed* player = FindPlayerPed();
    if (!player) {
        return;
    }
    CVector pos = player->GetPosition() + CVector{2.0f, 0.0f, 0.0f};
    // heap-allocated on purpose: ownership goes to the world/pool (also
    // exercises the entity cleanup paths under test)
    auto* ped = new CCivilianPed(PED_TYPE_CIVMALE, (eModelID)modelId);
    ped->m_matrix->SetTranslate(pos);
    ped->SetModelIndex((eModelID)modelId);
    CWorld::Add(ped);
}

// ---------------------------------------------------------------- control API (MCP)
extern "C" __declspec(dllexport) void __cdecl TestTeleport(float x, float y, float z) {
    if (CPlayerPed* ped = FindPlayerPed()) {
        ped->Teleport({x, y, z}, false);
        CGameLogic::ClearSkip(false);
    }
}

extern "C" __declspec(dllexport) void __cdecl TestSetHealth(float hp) {
    if (CPlayerPed* ped = FindPlayerPed()) {
        ped->m_fHealth = hp;
    }
}

extern "C" __declspec(dllexport) void __cdecl TestGiveWeapon(int weaponType, int ammo) {
    if (CPlayerPed* ped = FindPlayerPed()) {
        ped->GiveWeapon((eWeaponType)weaponType, (uint32)ammo, false);
    }
}

// Live pad control from the host: called every ~15 ms by the harness worker.
// Values: -128..128 for sticks, 0/255 for buttons. Button offsets: +0 stickX,
// +2 stickY, +16 DPadUp, +20 DPadLeft, +22 DPadRight, +28 Square, +30 Triangle,
// +32 Cross, +34 Circle.
static volatile short* PadBtn(int off) {
    return (volatile short*)(0xB73458 + off);
}
extern "C" __declspec(dllexport) void __cdecl TestSetPad(int stickX, int stickY, int cross, int square, int triangle, int circle) {
    *PadBtn(0)  = (short)stickX;
    *PadBtn(2)  = (short)stickY;
    *PadBtn(32) = (short)cross;
    *PadBtn(28) = (short)square;
    *PadBtn(30) = (short)triangle;
    *PadBtn(34) = (short)circle;
}
extern "C" __declspec(dllexport) void __cdecl TestClearPad() {
    for (int off = 0; off <= 34; off += 2) {
        *PadBtn(off) = 0;
    }
}
