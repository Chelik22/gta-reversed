#include "StdInc.h"

#include "TaskSimpleFightingControl.h"
#include "PedStats.h"
#include "TaskSimpleFight.h"

#include "EventObjectCollision.h"
#include "EventBuildingCollision.h"

constexpr auto FIGHT_CTRL_MAX_ATTACK_ANGLE_RAD = DegreesToRadians(15);
constexpr auto FIGHT_CTRL_FIGHT_IDLE_TIME = 60000.f;

// Сверено с декомпилом 0x62A0A0 + дизасм ctor 0x61DC10 (поправки против фрагмента fresh/0x62A0A0.cpp):
//  - раскладка хедера репо ВЕРНА (дизасм ctor: arg2(angleRad)->+0x10 m_angleRad, arg3(range)->+0x14
//    m_maxAttackRange, +0x18=1.0f m_unk3, +0x1C=0 m_nextAttackTime, +0x20=0 m_someTime), правка хедера НЕ нужна;
//  - время следующей атаки в декомпиле это +0x1C => в репо поле называется m_nextAttackTime (не m_unk3);
//  - комбо-диапазон (m_aComboData[combo].m_fRanges) пишется в +0x14 => m_maxAttackRange (не m_angleRad);
//  - ранний выход: оригинал возвращает true и при отсутствии цели, и при m_bool (0x62A0CE: mov al,1);
//  - проверка типа цели: (byte)(target+0x36) & 7 != 3 => GetType() != ENTITY_TYPE_PED (m_info @+0x36);
//  - NOTE: в ctor оригинала m_unk3 = 1.0f (0x61DC71: mov [esi+0x18], 0x3F800000) - текущий реверснутый
//    ctor репо этого не пишет; на ProcessPed не влияет (поле +0x18 здесь не читается).
void CTaskSimpleFightingControl::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleFightingControl, 0x86d700, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x61DC10);
    RH_ScopedInstall(Destructor, 0x61DCA0);

    RH_ScopedInstall(CalcMoveCommand, 0x624B50);

    RH_ScopedVMTInstall(Clone, 0x622EB0);
    RH_ScopedVMTInstall(GetTaskType, 0x61DC90);
    RH_ScopedVMTInstall(MakeAbortable, 0x61DD00);
    RH_ScopedVMTInstall(ProcessPed, 0x62A0A0);
}

// 0x61DC10
CTaskSimpleFightingControl::CTaskSimpleFightingControl(CEntity* entityToFight, float angleRad, float attackRange) :
    m_target{entityToFight},
    m_angleRad{angleRad},
    m_maxAttackRange{ attackRange }
{
    CEntity::SafeRegisterRef(m_target);
}

// NOTSA
CTaskSimpleFightingControl::CTaskSimpleFightingControl(const CTaskSimpleFightingControl& o) :
    CTaskSimpleFightingControl{o.m_target, o.m_angleRad, o.m_maxAttackRange }
{
}

// 0x61DCA0
CTaskSimpleFightingControl::~CTaskSimpleFightingControl() {
    CEntity::SafeCleanUpRef(m_target);
}

// 0x624B50
int16 CTaskSimpleFightingControl::CalcMoveCommand(CPed* ped) {
    if (!m_target) {
        return -1;
    }

    const auto& targetPos = m_target->GetPosition();
    const auto& pedPos    = ped->GetPosition();

    const auto segmentPedToTarget = targetPos - pedPos;

    // Check if target is outside of the attacking angle
    if (std::abs(CGeneral::LimitRadianAngle(segmentPedToTarget.Heading()) - ped->m_fCurrentRotation) >= FIGHT_CTRL_MAX_ATTACK_ANGLE_RAD) {
        m_nextAttackTime += (uint32)CTimer::GetTimeStepInMS();
        return 0;
    }

    switch (m_target->GetType()) {
    case ENTITY_TYPE_PED: {
        const auto range = segmentPedToTarget.Magnitude() - m_maxAttackRange;
        if (range > 0.1f) {
            return 3;
        }

        const auto rnd = rand();

        if (range > -0.1f) { // Inverted
            return (rnd % 16 == 0) ? 7 : -1;
        }

        if (range >= 0.8f) {
            if (rand() % 64 == 0) {
                return 8;
            }
            if (rand() % 64 == 0) {
                return 10;
            }
        } else if (rnd % 16 == 0) {
            return 9;
        }
     
        break;
    }
    case ENTITY_TYPE_VEHICLE: {
        CVector planes[4]{};
        float planesDot[4]{};
        CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanesUncachedAll(targetPos.z, *m_target, &planes, planesDot);

        const auto hitSide = CPedGeometryAnalyser::ComputeEntityHitSide(pedPos, *m_target);
        if ((DotProduct(planes[hitSide], pedPos) + planesDot[hitSide]) >= 0.7f) {
            return 3;
        }

        break;
    }
    default: {
        if (segmentPedToTarget.Magnitude() - m_maxAttackRange > 0.3f) {
            return 3;
        }
        break;
    }
    }

    return -1;
}

// 0x61DD00
bool CTaskSimpleFightingControl::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    switch (priority) {
    case ABORT_PRIORITY_LEISURE:
        return false;
    case ABORT_PRIORITY_IMMEDIATE:
        return true;
    case ABORT_PRIORITY_URGENT: {
        if (event) {
            if (event->GetEventPriority() < 22) {
                return false;
            }

            switch (event->GetEventType()) {
            case EVENT_PED_COLLISION_WITH_PED:
            case EVENT_PED_COLLISION_WITH_PLAYER:
            case EVENT_PLAYER_COLLISION_WITH_PED: {
                return false;
            }
            case EVENT_VEHICLE_COLLISION:
            case EVENT_OBJECT_COLLISION:
            case EVENT_BUILDING_COLLISION: {
                if (!m_target) {
                    return true;
                }

                // This is not how they did it, but I'd like to avoid using blackmagic.
                // Once we reverse all stuff related to these events we should make a common base class
                // like `CEventEntityCollision` or something similar.
                const auto impactNormal = [event] {
                    switch (event->GetEventType()) {
                    case EVENT_VEHICLE_COLLISION:
                        return static_cast<const CEventVehicleCollision*>(event)->m_impactNormal;
                    case EVENT_OBJECT_COLLISION:
                        return static_cast<const CEventObjectCollision*>(event)->m_impactNormal;
                    case EVENT_BUILDING_COLLISION:
                        return static_cast<const CEventBuildingCollision*>(event)->m_impactNormal;
                    default:
                        NOTSA_UNREACHABLE();
                    }
                }();

                // Check if vector are opposing each other with 45deg tolerance
                if (-DotProduct(Normalized(m_target->GetPosition() - ped->GetPosition()), impactNormal) < SQRT_2 / 2) {
                    return false; // They aren't
                }
            }
            }
        }

        if (ped->GetIntelligence()->GetTaskFighting()) {
            return ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_ATTACK)->MakeAbortable(ped, priority, event);
        }

        return true;
    }
    default:
        NOTSA_UNREACHABLE();
    }
}

// 0x62A0A0
bool CTaskSimpleFightingControl::ProcessPed(CPed* ped) {
    if (!m_target || m_bool) {
        return true; // 0x62A0CE: mov al,1 - в оригинале true в обоих случаях
    }

    // Вероятностный сид атаки: рейтинг стрельбы или флаги статов для некопов-не-миссионцев
    auto attackRate = (float)ped->m_nWeaponShootingRate; // +0x719
    if (!ped->IsCreatedBy(PED_MISSION) && ped->m_nPedType != PED_TYPE_COP && ped->m_nWeaponShootingRate == 0x28) {
        attackRate = (float)ped->m_pStats->m_flags; // CPedStat+0x30
    }

    ped->GiveWeaponAtStartOfFight(); // 0x5E8AB0

    int32 command = 0; // iStack_14
    auto* fightTask = ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_ATTACK); // 0x681810
    if (!fightTask) {
        fightTask = new CTaskSimpleFight{ m_target, 0, (uint32)attackRate }; // 0x61A5A0 + ctor 0x61C470 (idlePeriod = _ftol2 от attackRate)
        ped->GetTaskManager().SetTaskSecondary(fightTask, TASK_SECONDARY_ATTACK); // 0x681B60
        m_nextAttackTime = 0; // +0x1C
    } else if (fightTask->GetTaskType() != TASK_SIMPLE_FIGHT) { // 1016 (0x3F8)
        return fightTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr); // vtable+0x18
    } else if (m_nextAttackTime > CTimer::GetTimeInMS()) { // DAT_00b7cb84 < *(uint*)(this+0x1C)
        command = 2;
        // 2.0f * rate * 0.025f <= rand(0..100)?
        const auto rand100 = CGeneral::GetRandomNumberInRange(0.0f, 100.0f); // 0x41BD90
        if (m_target->GetType() != ENTITY_TYPE_PED // оригинал: (byte)(target+0x36) & 7 != 3
            || m_someTime != 0 // +0x20
            || 2.0f * attackRate * 0.025f <= rand100
        ) {
            if (m_someTime != 0) {
                // NOTE: FPU-аргумент _ftol2 (0x821B40) в декомпиле потерян (вероятнее rand01*m_someTime)
                const auto sub = (uint32)CGeneral::GetRandomNumberInRange(0.0f, (float)m_someTime);
                m_someTime = sub < m_someTime ? m_someTime - sub : 0;
            }
        } else {
            m_someTime = (uint32)CGeneral::GetRandomNumberInRange(500, 2000); // 0x407180, DAT_008d2e58/5c
        }
    } else {
        m_nextAttackTime = 0;
        command = 0xB;
        if (ped->m_nFightingStyle != STYLE_STANDARD && ped->m_aWeapons[ped->m_nActiveWeaponSlot].m_Type == WEAPON_UNARMED) {
            command = 0xC;
        }
    }

    // LAB_0062a2a4
    auto* fight = static_cast<CTaskSimpleFight*>(
        ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_ATTACK)
    );
    const int32 combo = std::max<int32>((int8)fight->m_nComboSet - 4, 0); // оригинал: (x & ((x<0)-1))
    m_maxAttackRange = CTaskSimpleFight::m_aComboData[combo].m_fRanges; // 0xC170D4[combo] (+4; subscript декомпила в DWORD-единицах = stride 0x88)

    if (m_nextAttackTime == 0) {
        // Инлайн в оригинале: m_nextAttackTime = (int)(_ftol2 0x821B40 от rand) + now; DAT_008d2e4c = 2000
        m_nextAttackTime = (uint32)CGeneral::GetRandomNumberInRange(0, 2000) + CTimer::GetTimeInMS();
    }

    if (fight->m_nComboSet < 2) {
        const auto moveCmd = (uint16)CalcMoveCommand(ped); // 0x624B50
        if (moveCmd > 0x7FFF) {
            // Отрицательный (int16) командный код -> остаётся command из стейт-машины выше
            return fight->ControlFight(m_target, (uint8)command); // 0x61C5E0
        }
        command = (int16)moveCmd;
    }
    return fight->ControlFight(m_target, (uint8)command);
}
