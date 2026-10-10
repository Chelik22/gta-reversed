// ============================================================================
// INTEGRATED FILE — source/game_sa/Tasks/TaskTypes/TaskComplexGoToPointAiming.cpp
// Функции: CreateSubTask (0x6688D0), CreateFirstSubTask (0x66DDB0), ControlSubTask (0x6689E0)
//
// ИСТОЧНИКИ: фрагменты batch_out/fresh/0x6688D0.cpp, 0x66DDB0.cpp, 0x6689E0.cpp.
// 0x66DDB0 верифицирован VERIFY-wf5a (FIXED: проверяется m_nWeaponFire, не тип оружия);
// 0x6689E0 дополнительно ревьюился агентом f5-misc1 (исправлена нормализация
// ControlGunMove: moveDir = local * (1/dist), не * sqrt(S)).
//
// РАСКЛАДКА: хедер репо СОВПАДАЕТ с оригиналом (VERIFY-wf5a; CTaskComplex = 0xC,
// TaskComplex.h:72): m_moveState@0xC, m_aimAtEntity@0x10, m_aimPos@0x14, m_movePos@0x20,
// m_moveTargetRadius@0x2C, m_slowDownDistance@0x30, m_newTargetSet@0x34 (sizeof 0x38).
// Устаревшее замечание фрагмента 0x6689E0 о несовпадении раскладки НЕАКТУАЛЬНО —
// правка хедера не требуется. TASK_COMPLEX_GO_TO_POINT_AIMING = 924 (0x39C) —
// сверен по Enums/eTaskType.h, значение оригинала.
//
// ПРАВКИ ИНТЕГРАТОРА против фрагментов:
//  - 0x6689E0: приватные поля CMatrix mtx.m_right/m_forward заменены на публичные
//    GetRight()/GetForward() (Core/Matrix.h:91,94), Dot() -> DotProduct(const CVector&, const CVector&)
//    (Core/Vector.h:294).
// Сверено в репо: CTaskComplexGoToPointAndStandStill ctor 6 аргов + GoToPoint(pt,r,r2,bool)
// (TaskComplexGoToPointAndStandStill.h:25,43), CTaskSimpleGunControl(targetEntity,targetPos,
// moveTarget,cmd,burst,dur) (TaskSimpleGunControl.h:49), CTaskSimpleUseGun(targetEntity,
// vecTarget,cmd,burst,bAimImmediate) + ControlGun(CPed*,CEntity*,eGunCommand) +
// ControlGunMove(const CVector2D&) (TaskSimpleUseGun.h:23,37,38),
// CWeaponInfo::GetWeaponInfo(eWeaponType,eWeaponSkill) (WeaponInfo.h:118),
// CTaskManager::GetTaskSecondary/SetTaskSecondary (TaskManager.h:88,179),
// CPedIntelligence::GetTaskUseGun (PedIntelligence.h:99),
// CPed::GetWeaponSkill() (Ped.h:496), m_aWeapons/m_nActiveWeaponSlot (Ped.h:312,316).
// Константы: 0.5f (0x86FC84), 2.0f (0x86FC88), -1.0f (0x858C1C), 1.0f (0x858624).
// Проверка this->GetTaskType() == TASK_COMPLEX_GO_TO_POINT_SHOOTING сохранена как в
// оригинале (для Aiming/924 всегда false).
// ============================================================================

#include "StdInc.h"

#include "TaskComplexGoToPointAiming.h"

#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskSimpleGunControl.h"
#include "TaskSimpleUseGun.h"

void CTaskComplexGoToPointAiming::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexGoToPointAiming, 0x86fe00, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x668790);
    RH_ScopedInstall(Destructor, 0x668870);
    RH_ScopedInstall(CreateSubTask, 0x6688D0);

    RH_ScopedVMTInstall(Clone, 0x66CD80);
    RH_ScopedVMTInstall(GetTaskType, 0x668860);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x66DD70);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x66DDB0);
    RH_ScopedVMTInstall(ControlSubTask, 0x6689E0);
}

// 0x668790
CTaskComplexGoToPointAiming::CTaskComplexGoToPointAiming(
    eMoveState moveState,
    const CVector& movePos,
    CEntity* aimAtEntity,
    CVector aimPos,
    const float targetRadius,
    const float slowDownDist
) :
    m_aimPos{aimPos},
    m_aimAtEntity{aimAtEntity},
    m_moveState{moveState},
    m_movePos{movePos},
    m_moveTargetRadius{targetRadius},
    m_slowDownDistance{slowDownDist}
{
    CEntity::SafeRegisterRef(m_aimAtEntity);
}

CTaskComplexGoToPointAiming::CTaskComplexGoToPointAiming(const CTaskComplexGoToPointAiming& o) :
    CTaskComplexGoToPointAiming{
        o.m_moveState,
        o.m_movePos,
        o.m_aimAtEntity,
        o.m_aimPos,
        o.m_moveTargetRadius,
        o.m_slowDownDistance
    }
{
}

// 0x668870
CTaskComplexGoToPointAiming::~CTaskComplexGoToPointAiming() {
    CEntity::SafeCleanUpRef(m_aimAtEntity);
}

// 0x6688D0
CTask* CTaskComplexGoToPointAiming::CreateSubTask(eTaskType taskType) {
    switch (taskType) {
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL: // 0x387
        return new CTaskComplexGoToPointAndStandStill{ m_moveState, m_movePos, m_moveTargetRadius, m_slowDownDistance, false, false };

    case TASK_SIMPLE_GUN_CTRL: { // 0x3FC
        // В оригинале moveTarget = NULL (обёртка хука это допускает: `moveTarget ? *moveTarget : CVector{}`) —
        // передаём пустой CVector, поле m_moveTarget остаётся нулевым
        const auto cmd = GetTaskType() == TASK_COMPLEX_GO_TO_POINT_SHOOTING
            ? eGunCommand::FIREBURST
            : eGunCommand::NONE;
        return new CTaskSimpleGunControl{ m_aimAtEntity, m_aimPos, CVector{}, cmd, 1, 600000 };
    }

    default:
        return nullptr;
    }
}

// 0x66DD70
CTask* CTaskComplexGoToPointAiming::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
    case TASK_SIMPLE_GUN_CTRL:
        return CreateSubTask(TASK_FINISHED);
    default:
        NOTSA_UNREACHABLE();
    }
}

// 0x66DDB0
CTask* CTaskComplexGoToPointAiming::CreateFirstSubTask(CPed* ped) {
    m_newTargetSet = false; // byte @ 0x34 &= ~1

    const auto& wep     = ped->m_aWeapons[ped->m_nActiveWeaponSlot];
    const auto  wepInfo = CWeaponInfo::GetWeaponInfo(wep.m_Type, ped->GetWeaponSkill());

    if (wepInfo->flags.bAimWithArm) {
        // Есть вторичная задача атаки - прерываем её; нет - создаём
        if (const auto secondary = ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_ATTACK)) {
            secondary->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr);
        } else {
            // NOTE: в оригинале стоит проверка GetTaskType() == TASK_COMPLEX_GO_TO_POINT_SHOOTING (0x385),
            //       что для этого таска (Type == TASK_COMPLEX_GO_TO_POINT_AIMING) фактически всегда false - сохранено как есть.
            const auto useGun = GetTaskType() == TASK_COMPLEX_GO_TO_POINT_SHOOTING
                ? new CTaskSimpleUseGun(m_aimAtEntity, m_aimPos, eGunCommand::FIREBURST, 3, false)
                : new CTaskSimpleUseGun(m_aimAtEntity, m_aimPos, eGunCommand::AIM, 1, false);
            ped->GetTaskManager().SetTaskSecondary(useGun, TASK_SECONDARY_ATTACK);
        }
    } else if (wepInfo->flags.bCanAim
        && (wepInfo->m_nWeaponFire == WEAPON_FIRE_INSTANT_HIT || wepInfo->m_nWeaponFire == WEAPON_FIRE_AREA_EFFECT)
    ) {
        // Декомпил (0x66DDB0.c:25) проверяет *wepInfo == 1 || == 3 — это m_nWeaponFire
        // (ПЕРВОЕ поле CWeaponInfo, WeaponInfo.h:48), а не тип оружия
        return CreateSubTask(TASK_SIMPLE_GUN_CTRL);
    }

    return new CTaskComplexGoToPointAndStandStill(m_moveState, m_movePos, m_moveTargetRadius, m_slowDownDistance, false, false);
}

// 0x6689E0
CTask* CTaskComplexGoToPointAiming::ControlSubTask(CPed* ped) {
    const auto wi = CWeaponInfo::GetWeaponInfo(ped->m_aWeapons[ped->m_nActiveWeaponSlot].m_Type, ped->GetWeaponSkill());

    if (!m_newTargetSet) {
        switch (m_pSubTask->GetTaskType()) {
        case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL: { // 903
            static_cast<CTaskComplexGoToPointAndStandStill*>(m_pSubTask)->GoToPoint(m_movePos, 0.5f /* 0x86FC84 */, 2.0f /* 0x86FC88 */, false);

            if (wi->flags.bAimWithArm) {
                auto* gunTask = ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_ATTACK);
                if (!gunTask) {
                    ped->GetTaskManager().SetTaskSecondary(
                        new CTaskSimpleUseGun{ m_aimAtEntity, m_aimPos, eGunCommand::AIM, 1, false },
                        TASK_SECONDARY_ATTACK
                    );
                } else if (!ped->GetIntelligence()->GetTaskUseGun()) {
                    gunTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr);
                } else {
                    auto* useGun = static_cast<CTaskSimpleUseGun*>(
                        ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_ATTACK)
                    );
                    if (GetTaskType() == TASK_COMPLEX_GO_TO_POINT_SHOOTING && (rand() & 0x3F) == 0) {
                        useGun->ControlGun(ped, m_aimAtEntity, eGunCommand::FIRE);
                    } else {
                        useGun->ControlGun(ped, m_aimAtEntity, eGunCommand::AIM);
                    }
                }
            }
            break;
        }
        case TASK_SIMPLE_GUN_CTRL: { // 1020
            if (ped->GetIntelligence()->GetTaskUseGun()) {
                // Инлайн-блок оригинала: локальное 2D-направление к m_movePos
                const auto  d       = m_movePos - ped->GetPosition();
                const auto& mtx     = ped->GetMatrix();
                const float localX  = DotProduct(d, mtx.GetRight());
                const float localY  = -DotProduct(d, mtx.GetForward()); // оригинал: fmul dword [0x858C1C] (-1.0)
                const float S       = localX * localX + localY * localY;
                const float r       = m_moveTargetRadius;

                CVector2D moveDir;
                if (S <= r * r) { // fcompp S vs r² (0x668AD7) + test ah,0x41: below/equal -> {0,0}
                    moveDir = CVector2D{};
                } else {
                    const float dist = std::sqrt(S);
                    moveDir.x = localX / dist; // fdivr [0x858624]: 1.0f/dist (0x668AE4), НЕ умножение
                    moveDir.y = localY / dist;
                }
                static_cast<CTaskSimpleUseGun*>(
                    ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_ATTACK)
                )->ControlGunMove(moveDir);
            }
            break;
        }
        }
    } else {
        return CreateFirstSubTask(ped); // vtable+0x24 (0x66DDB0)
    }
    return m_pSubTask;
}
