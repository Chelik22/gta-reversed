#include "StdInc.h"

#include "TaskComplexCarSlowBeDraggedOutAndStandUp.h"

#include "TaskComplexCarSlowBeDraggedOut.h"
#include "TaskComplexGetUpAndStandStill.h"
#include "TaskComplexLeaveCar.h"
#include "TaskSimpleCarSetPedOut.h"
#include "Events/EventDeath.h"

void CTaskComplexCarSlowBeDraggedOutAndStandUp::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexCarSlowBeDraggedOutAndStandUp, 0x86EF80, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x648620);
    RH_ScopedInstall(Destructor, 0x648690);
    RH_ScopedInstall(CreateSubTask, 0x648710);
    RH_ScopedInstall(Clone, 0x64A190);
    RH_ScopedInstall(GetTaskType, 0x648680);
    RH_ScopedInstall(MakeAbortable, 0x6486F0);
    RH_ScopedInstall(CreateNextSubTask, 0x6488F0);
    RH_ScopedInstall(CreateFirstSubTask, 0x648A10);
    RH_ScopedInstall(ControlSubTask, 0x648B80);
}

// 0x648620
CTaskComplexCarSlowBeDraggedOutAndStandUp::CTaskComplexCarSlowBeDraggedOutAndStandUp(CVehicle* vehicle, int32 a3) : CTaskComplex() {
    m_Vehicle = vehicle;
    dword10 = a3;
    CEntity::SafeRegisterRef(m_Vehicle);
}

// 0x648690
CTaskComplexCarSlowBeDraggedOutAndStandUp::~CTaskComplexCarSlowBeDraggedOutAndStandUp() {
    CEntity::SafeCleanUpRef(m_Vehicle);
}

// 0x648710
CTask* CTaskComplexCarSlowBeDraggedOutAndStandUp::CreateSubTask(eTaskType taskType, CPed* ped) {
    switch (taskType) {
    case TASK_SIMPLE_CAR_SET_PED_OUT: { // 0x330
        // 0x6478B0
        return new CTaskSimpleCarSetPedOut(ped->m_pVehicle, (eTargetDoor)dword10, true);
    }
    case TASK_COMPLEX_GET_UP_AND_STAND_STILL: { // 0xCE
        // 0x678130
        return new CTaskComplexGetUpAndStandStill();
    }
    case TASK_COMPLEX_LEAVE_CAR: { // 0x2C0
        const auto animGroup = m_Vehicle->m_pHandlingData->m_nAnimGroup;
        if (animGroup == 0x0F) {
            return new CTaskComplexLeaveCar(ped->m_pVehicle, 8, 0, false, true);
        }
        if (animGroup == 0x18) {
            return new CTaskComplexLeaveCar(ped->m_pVehicle, 10, 0, false, true);
        }
        if (dword10 == 8 && animGroup == 0x10) {
            return new CTaskComplexLeaveCar(ped->m_pVehicle, 8, 0, false, true);
        }
        return nullptr;
    }
    case TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT: { // 0x337
        // 0x648490
        return new CTaskComplexCarSlowBeDraggedOut(m_Vehicle, (eTargetDoor)dword10, false);
    }
    default:
        return nullptr;
    }
}

// 0x6486F0
bool CTaskComplexCarSlowBeDraggedOutAndStandUp::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (priority == ABORT_PRIORITY_URGENT) {
        // NOTE: The original used a jump-table dispatch which Ghidra couldn't recover;
        // the only reachable behaviour is aborting the sub task.
        return m_pSubTask->MakeAbortable(ped, priority, event);
    }
    return false;
}

// 0x6488F0
CTask* CTaskComplexCarSlowBeDraggedOutAndStandUp::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT: { // 0x337
        if (ped->IsAlive() && ped->m_fHealth < 0.0f) {
            return new CTaskComplexGetUpAndStandStill();
        }
        // The ped dies: add a death event to its event group
        // (0x4ADDF0 = CEventDeath, 0x4AB420 = CEventGroup::Add, 0x4ADE90 = CEventDeath dtor)
        CEventDeath death{false};
        ped->GetEventGroup().Add(&death, false);
        return nullptr;
    }
    case TASK_COMPLEX_GET_UP_AND_STAND_STILL: // 0xCE - calls `ped->IsPlayer()` (0x5DF8F0) and falls through
    case TASK_COMPLEX_LEAVE_CAR:              // 0x2C0
    case TASK_SIMPLE_CAR_SET_PED_OUT:         // 0x330
    default:
        return nullptr;
    }
}

// 0x648A10
CTask* CTaskComplexCarSlowBeDraggedOutAndStandUp::CreateFirstSubTask(CPed* ped) {
    if (ped == FindPlayerPed()) {
        if (ped->m_pVehicle) {
            ped->m_pVehicle->m_vehicleAudio.PlayerAboutToExitVehicleAsDriver(); // 0x4F5BA0
        }
    } else if (ped->m_pVehicle && ped->m_pVehicle->m_pDriver == ped) {
        ped->SetRadioStation(); // 0x5DFD90
    }

    const auto animGroup = ped->m_pVehicle->m_pHandlingData->m_nAnimGroup;
    if (animGroup == 0x0F || animGroup == 0x18 || (dword10 == 8 && animGroup == 0x10)) {
        return CreateSubTask(TASK_COMPLEX_LEAVE_CAR, ped);
    }

    // Play a "jacked car" audio cue based on the vehicle's anim group
    switch (animGroup) {
    case 0x00:
    case 0x0D:
    case 0x0E:
    case 0x12:
    case 0x13:
    case 0x16:
    case 0x17:
    case 0x1C:
        if (dword10 == 10) {
            ped->GetAE().AddAudioEvent(AE_PED_JACKED_CAR_PUNCH);
        } else if (dword10 == 8) {
            ped->GetAE().AddAudioEvent(AE_PED_JACKED_CAR_HEAD_BANG);
        }
        break;
    case 0x01:
        ped->GetAE().AddAudioEvent(AE_PED_JACKED_CAR_KICK);
        break;
    case 0x11:
        ped->GetAE().AddAudioEvent(AE_PED_JACKED_DOZER);
        break;
    }

    return CreateSubTask(TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT, ped);
}

// 0x648B80
CTask* CTaskComplexCarSlowBeDraggedOutAndStandUp::ControlSubTask(CPed* ped) {
    return m_pSubTask;
}
