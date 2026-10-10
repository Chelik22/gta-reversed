#include "StdInc.h"

#include "TaskSimpleDead.h"

#include "CarEnterExit.h"            // CCarEnterExit::ComputeTargetDoorToExit (0x64F110)
#include "Shadows.h"                 // CShadows::StoreStaticShadow / AddPermanentShadow, gpBloodPoolTex
#include "Animation/AnimManager.h"   // CAnimManager::BlendAnimation (0x4D4610)
#include "Localisation.h"            // CLocalisation::Blood (0x56D230)
#include "Events/EventDeadPed.h"     // CEventDeadPed (0x4ADEA0)
#include "RwHelper.h"                // GetEventGlobalGroup (0x4ABA50)
#include "WaterLevel.h"              // CWaterLevel::GetWaterLevelNoWaves (0x6E8580, ревертнута)
#include "AccidentManager.h"         // CAccidentManager::GetInstance (0x56CF20) / ReportAccident (0x56CE80)

// Сверено с декомпилом 0x630600 + дизасм 0x63085B..0x630F02 (поправки против фрагмента fresh/0x630600.cpp):
//  - в первый-тайм блок возвращены 2 потерянных вызова: CAccidentManager::GetInstance()->ReportAccident(ped)
//    (0x56CF20/0x56CE80, обе ревертнуты в репо);
//  - снятие флага у FLOOR_HIT_F: байт assoc+0x2E &= 0xF7 == старший байт m_Flags (@0x2C, uint16):
//    m_Flags &= ~ANIMATION_IS_FRONT (0x800), а НЕ ~0x8;
//  - skill в GetWeaponInfo = eWeaponSkill::STD (=1);
//  - финал: return false всегда (0x630F02: xor al,al).
void CTaskSimpleDead::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleDead, 0x86DEA4, 9);
    RH_ScopedCategory("Tasks/TaskTypes");
    RH_ScopedInstall(ProcessPed, 0x630600);
}

// NOTSA: *deathTime* originally int32
// 0x630590
CTaskSimpleDead::CTaskSimpleDead(uint32 deathTime, bool hasDrowned) :
    m_nDeathTimeMS{deathTime},
    m_bHasDrowned{hasDrowned}
{
}

// 0x636100
CTaskSimpleDead::CTaskSimpleDead(const CTaskSimpleDead& o) :
    CTaskSimpleDead{o.m_nDeathTimeMS, o.m_bHasDrowned}
{
}

// 0x630600
bool CTaskSimpleDead::ProcessPed(CPed* ped) {
    bool bWasStanding    = false;
    bool bIsPlayerTarget = false;

    if (m_bFirstTime) {
        if (ped->bInVehicle) {
            // ANIM_ID_CAR_DEAD_LHS(187)/RHS(188) по двери выхода
            const auto exitDoor = CCarEnterExit::ComputeTargetDoorToExit(ped->m_pVehicle, ped);
            CAnimManager::BlendAnimation(
                ped->GetRpClump(),
                (AssocGroupId)0,
                exitDoor == 10 || exitDoor == 11 ? ANIM_ID_CAR_DEAD_LHS : ANIM_ID_CAR_DEAD_RHS,
                4.0f
            );
        }
        ped->SetPedState(PEDSTATE_DEAD); // 0x37
        m_bFirstTime = false;

        bWasStanding = ped->bIsStanding || ped->bWasStanding;
        bIsPlayerTarget =
            FindPlayerPed(0)->m_pTargetedObject == ped
            || (FindPlayerPed(1) && FindPlayerPed(1)->m_pTargetedObject == ped);

        if (!m_bHasDrowned && !ped->m_standingOnEntity && !bIsPlayerTarget) {
            ped->m_bUsesCollision = false;
        }
        ped->m_fHealth = 0.0f;

        const auto& weapon = ped->m_aWeapons[ped->m_nActiveWeaponSlot];
        ped->RemoveWeaponModel(CWeaponInfo::GetWeaponInfo(weapon.m_Type, eWeaponSkill::STD)->m_nModelId1);
        ped->m_nActiveWeaponSlot = 0;

        if (!ped->IsPlayer()) {
            ped->RemoveWeaponAnims(0, -1000.0f); // 0xC47A0000
            ped->CreateDeadPedWeaponPickups();
            ped->CreateDeadPedMoney();
        }

        CEventDeadPed event{ ped, m_bHasDrowned, m_nDeathTimeMS };
        GetEventGlobalGroup()->Add(&event, false);

        CAccidentManager::GetInstance()->ReportAccident(ped); // 0x56CF20 + 0x56CE80 (в фрагменте были потеряны)
    }

    // Затухание/расчёт наклона
    bool skipSlope = false;
    if (!m_bFirstTime && m_bHasDrowned && ped->bIsStanding) {
        // Утонувший, но стоящий: сбросить коллизию и сыграть FLOOR_HIT_F
        ped->m_bUsesCollision = false;
        m_bHasDrowned         = false;
        if (const auto assoc = CAnimManager::BlendAnimation(ped->GetRpClump(), (AssocGroupId)0, ANIM_ID_FLOOR_HIT_F, 8.0f)) {
            assoc->m_Flags &= ~uint16(ANIMATION_IS_FRONT); // оригинал: byte assoc+0x2E &= 0xF7 (старший байт m_Flags)
        }
    } else if (
        !ped->m_bUsesCollision
        || m_bHasDrowned
        || !ped->bIsStanding
        || ped->m_standingOnEntity
        || FindPlayerPed(0)->m_pTargetedObject == ped
        || (FindPlayerPed(1) && FindPlayerPed(1)->m_pTargetedObject == ped)
        || bIsPlayerTarget
    ) {
        if (!bWasStanding) {
            skipSlope = true; // goto LAB_00630932
        }
    } else {
        ped->m_bUsesCollision = false;
    }

    // Наклон тела: проекции field_578 на оси матрицы педа, клампятся в [-1..1] (0x858C1C=-1.0f, 0x858624=1.0f)
    if (!skipSlope) {
        const auto& mtrx = *ped->m_matrix; // NB: оригинал читает оси без проверки матрицы (0x630858)
        const auto  vec  = ped->field_578; // CPed+0x578
        ped->m_pedIK.m_fSlopeRoll  = std::acos(std::clamp(DotProduct(vec, mtrx.GetRight()),   -1.0f, 1.0f)); // +0x520 (acos=0x821E70)
        ped->m_pedIK.m_fSlopePitch = std::acos(std::clamp(DotProduct(vec, mtrx.GetForward()), -1.0f, 1.0f)); // +0x518
    }

    // LAB_00630932
    ped->DeadPedMakesTyresBloody();
    if (CLocalisation::Blood() && !m_bHasDrowned) {
        const auto elapsed = CTimer::GetTimeInMS() - m_nDeathTimeMS;

        // Радиус кровавого пятна: 0 -> 0.75 за 5 сек
        auto radius = 0.0f;
        if (elapsed > 1999) {
            radius = 0.7500001f; // 0x85B900
            if (elapsed < 0x1B59) { // < 7001
                radius = (float)(int32)(elapsed - 2000) * 0.00015f;
            }
        }

        // Пешеходы рядом топчатся в крови - ТОЛЬКО первые 8 слотов сканера (0x6309A8: блок [-2..+5] от intel+0x138)
        for (const auto ent : std::span{ ped->GetIntelligence()->m_pedScanner.m_apEntities }.first(8)) {
            if (!ent) {
                continue;
            }
            const auto other = static_cast<CPed*>(ent);
            if ((other->GetPosition() - ped->GetPosition()).SquaredMagnitude() < radius * radius) {
                other->m_nLookTime          = 200; // +0x750 (НЕ m_nDeathTimeMS: он в +0x758, Ped.h:335)
                other->bDoBloodyFootprints  = true; // +0x46C bit 28
            }
        }

        if (elapsed > 2000 && !m_bBloodPuddleCreated) {
            const auto pos = ped->GetPosition();

            const auto sinceStart = elapsed - 2000;
            const auto frameDt    = CTimer::GetTimeInMS() - CTimer::GetPreviousTimeInMS();
            if (sinceStart <= frameDt) {
                float waterLevel{};
                if (CWaterLevel::GetWaterLevelNoWaves(pos, &waterLevel)) { // 0x6E8580
                    if (pos.z < waterLevel) {
                        m_bBloodPuddleCreated = true; // труп в воде - лужи не будет
                    }
                }
            }
            if (!m_bBloodPuddleCreated && CLocalisation::Blood()) {
                if (sinceStart < 5000) {
                    // ИД тени = адрес задачи + 0x11 (уникален на инстанс), как в декомпиле
                    CShadows::StoreStaticShadow(
                        (uint32)(uintptr_t)this + 0x11,
                        SHADOW_DEFAULT,
                        gpBloodPoolTex,
                        pos,
                        (float)(int32)sinceStart * 0.00015f,
                        0.0f,
                        0.0f,
                        -0.00015f * (float)(int32)sinceStart,
                        (int16)0xFF,
                        200, 0, 0,
                        4.0f,
                        1.0f,
                        40.0f,
                        false,
                        0.0f
                    );
                } else {
                    CShadows::AddPermanentShadow(
                        SHADOW_DEFAULT,
                        gpBloodPoolTex,
                        const_cast<CVector*>(&pos),
                        0.7500001f, 0.0f,      // 0x3F400001
                        0.0f, -0.7500001f,     // 0xBF400001
                        (int16)0xFF,
                        200, 0, 0,
                        4.0f,
                        40000,
                        1.0f
                    );
                    m_bBloodPuddleCreated = true;
                }
            }
        }
    }

    if (!m_bHasDrowned) {
        ped->m_pedIK.m_nFlags |= 8; // bSlopePitch
        ped->m_vecMoveSpeed   = CVector{};
    } else {
        ped->bIsStanding  = false;
        ped->bWasStanding = false;
    }
    return false; // 0x630F02: xor al,al - оригинал ВСЕГДА возвращает false
}
