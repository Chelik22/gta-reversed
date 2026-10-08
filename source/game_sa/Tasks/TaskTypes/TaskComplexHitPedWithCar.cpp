#include "StdInc.h"

#include "TaskComplexHitPedWithCar.h"
#include "PedGeometryAnalyser.h"
#include "TaskSimpleHurtPedWithCar.h"
#include "TaskSimpleLeaveGroup.h"
#include "TaskComplexFallAndGetUp.h"
#include "TaskSimpleDuck.h"
#include "TaskComplexKillPedOnFoot.h"
#include "TaskSimpleStandStill.h"
#include "TaskComplexEvasiveStep.h"
#include "TaskSimpleHitFromBehind.h"
#include "TaskSimpleKillPedWithCar.h"

void CTaskComplexHitPedWithCar::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexHitPedWithCar, 0x86f294, 11);
    RH_ScopedCategory(); // TODO: Change this to the appropriate category!

    RH_ScopedInstall(Constructor, 0x6539A0);
    RH_ScopedInstall(Destructor, 0x653A30);

    RH_ScopedGlobalInstall(ComputeEvasiveStepMoveDir, 0x653B40);

    RH_ScopedInstall(HitHurtsPed, 0x653AE0);
    RH_ScopedInstall(CreateSubTask, 0x6560E0);

    RH_ScopedVMTInstall(Clone, 0x6559B0);
    RH_ScopedVMTInstall(GetTaskType, 0x653A20);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x657AF0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x656300);
    RH_ScopedVMTInstall(ControlSubTask, 0x653A90);

}

// 0x6539A0
CTaskComplexHitPedWithCar::CTaskComplexHitPedWithCar(CVehicle* veh, float impulseMagnitude) :
    m_Veh{veh},
    m_ImpulseMag{impulseMagnitude}
{
    CEntity::SafeRegisterRef(m_Veh);
}

CTaskComplexHitPedWithCar::CTaskComplexHitPedWithCar(const CTaskComplexHitPedWithCar& o) :
    CTaskComplexHitPedWithCar{o.m_Veh, o.m_ImpulseMag}
{
}

// 0x653A30
CTaskComplexHitPedWithCar::~CTaskComplexHitPedWithCar() {
    CEntity::SafeCleanUpRef(m_Veh);
}

// 0x653B40
CVector CTaskComplexHitPedWithCar::ComputeEvasiveStepMoveDir(const CPed* ped, CVehicle* veh) {
    switch ((eDirection)CPedGeometryAnalyser::ComputeEntityHitSide(*ped, *veh)) {
    case eDirection::FORWARD:
        return veh->GetForward();
    case eDirection::LEFT:
        return -veh->GetRight();     // * -1.0f (0x858C1C)
    case eDirection::BACKWARD:
        return -veh->GetForward();   // * -1.0f (0x858C1C)
    case eDirection::RIGHT:
        return veh->GetRight();
    default:
        NOTSA_UNREACHABLE();
    }
}
// 0x653AE0
bool CTaskComplexHitPedWithCar::HitHurtsPed(CPed* ped) {
    // Min impulse magnitude for the ped to get hurt
    const auto minImpulseToHurt = ped->IsPlayer()
        ? 10.0f // 0x86F124
        : 6.0f; // 0x86F11C

    if (m_ImpulseMag > minImpulseToHurt) {
        return true;
    }

    // Ped was slammed into the ground (downward) hard enough, and the hit was still reasonably strong
    return ped->m_vecLastCollisionImpactVelocity.z < -0.8f // 0x858C9C
        && m_ImpulseMag > 3.0f;                            // 0x858B3C
}
// 0x6560E0
CTask* CTaskComplexHitPedWithCar::CreateSubTask(eTaskType taskType) {
    switch (taskType) {
    case TASK_COMPLEX_EVASIVE_STEP:
        return new CTaskComplexEvasiveStep{ m_Veh, m_MoveDir };
    case TASK_NONE:
        return new CTaskSimpleLeaveGroup{};
    case TASK_COMPLEX_FALL_AND_GET_UP: {
        AnimationId fallAnimId;
        switch (m_PedHitSide) { // hit side of the ped relative to the car (eDirection)
        case 0: // eDirection::FORWARD
            fallAnimId = ANIM_ID_KO_SKID_BACK;
            break;
        case 1: // eDirection::LEFT
            fallAnimId = ANIM_ID_KO_SPIN_R;
            break;
        case 2: // eDirection::BACKWARD
            fallAnimId = ANIM_ID_KO_SKID_FRONT;
            break;
        case 3: // eDirection::RIGHT
            fallAnimId = ANIM_ID_KO_SPIN_L;
            break;
        default:
            fallAnimId = (AnimationId)taskType; // Original default: the task type constant itself (dead code, hit side is 0..3)
            break;
        }
        return new CTaskComplexFallAndGetUp{ fallAnimId, ANIM_GROUP_DEFAULT, m_DownTime };
    }
    case TASK_SIMPLE_HIT_BEHIND: // 0x19E=414 (NOT TASK_SIMPLE_DUCK=415) — verified by disasm
        return new CTaskSimpleHitFromBehind{};
    case TASK_SIMPLE_KILL_PED_WITH_CAR:
        return new CTaskSimpleKillPedWithCar{ m_Veh, m_ImpulseMag };
    case TASK_SIMPLE_HURT_PED_WITH_CAR:
        return new CTaskSimpleHurtPedWithCar{ m_Veh, m_ImpulseMag };
    case TASK_FINISHED:
    default:
        return nullptr;
    }
}
// 0x657AF0
CTask* CTaskComplexHitPedWithCar::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_EVASIVE_STEP:
    case TASK_NONE:
    case TASK_COMPLEX_FALL_AND_GET_UP:
    case TASK_SIMPLE_HIT_BEHIND: // 0x19E=414 (NOT TASK_SIMPLE_DUCK=415) — verified by disasm
    case TASK_SIMPLE_KILL_PED_WITH_CAR:
        return CreateSubTask(TASK_FINISHED); // Returns null — the task is finished
    case TASK_SIMPLE_HURT_PED_WITH_CAR:
        if (reinterpret_cast<CTaskSimpleHurtPedWithCar*>(m_pSubTask)->m_bWillKillPed) {
            return nullptr;
        }
        return CreateSubTask(TASK_COMPLEX_FALL_AND_GET_UP);
    default:
        return nullptr;
    }
}
// 0x656300
CTask* CTaskComplexHitPedWithCar::CreateFirstSubTask(CPed* ped) {
    m_PedHitSide = CPedGeometryAnalyser::ComputePedHitSide(*ped, *m_Veh);

    // Min impulse magnitude for the hit to kill the ped
    const auto minImpulseToKill = ped->IsPlayer()
        ? 20.0f // 0x86F128
        : 12.0f; // 0x86F120

    if (m_ImpulseMag > minImpulseToKill) {
        return CreateSubTask(TASK_SIMPLE_KILL_PED_WITH_CAR);
    }

    if (HitHurtsPed(ped)) {
        return CreateSubTask(TASK_SIMPLE_HURT_PED_WITH_CAR);
    }

    m_MoveDir = ComputeEvasiveStepMoveDir(ped, m_Veh);
    return CreateSubTask(TASK_COMPLEX_EVASIVE_STEP);
}
// 0x653A90
CTask* CTaskComplexHitPedWithCar::ControlSubTask(CPed* ped) {
    return m_pSubTask;
}

// 0x653A20
eTaskType CTaskComplexHitPedWithCar::GetTaskType() const {
    return Type; // TASK_COMPLEX_HIT_PED_WITH_CAR (0x1F9 = 505)
}

// 0x6559B0
CTask* CTaskComplexHitPedWithCar::Clone() const {
    return new CTaskComplexHitPedWithCar{ m_Veh, m_ImpulseMag };
}
