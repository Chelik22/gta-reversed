#include "StdInc.h"

#include "TaskComplexGoToPointAnyMeans.h"

#include "TaskSimpleCreateCarAndGetIn.h"
#include "TaskComplexEnterCarAsDriver.h"
#include "TaskComplexLeaveCar.h"
#include "TaskComplexDriveToPoint.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskComplexFollowNodeRoute.h"
#include "CarEnterExit.h"

// 0x5FFEE0 - CPedIntelligence helper (unreversed in this repository):
// returns the vehicle the ped could commandeer to reach the target
static CVehicle* FindVehicleToCommandeer(CPed* ped) {
    return plugin::CallMethodAndReturn<CVehicle*, 0x5FFEE0, CPed*>(ped);
}

void CTaskComplexGoToPointAnyMeans::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexGoToPointAnyMeans, 0x86FF68, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedOverloadedInstall(Constructor, "1", 0x66B720, CTaskComplexGoToPointAnyMeans*(CTaskComplexGoToPointAnyMeans::*)(int32, CVector const&, float, int32));
    RH_ScopedOverloadedInstall(Constructor, "2", 0x66B790, CTaskComplexGoToPointAnyMeans*(CTaskComplexGoToPointAnyMeans::*)(int32, CVector const&, CVehicle*, float, int32));
    RH_ScopedInstall(Destructor, 0x66B830);
    RH_ScopedInstall(CreateSubTask, 0x6705D0);
    RH_ScopedInstall(CreateNextSubTask, 0x6728A0);
    RH_ScopedInstall(CreateFirstSubTask, 0x6729C0);
    RH_ScopedInstall(ControlSubTask, 0x672A50);
}

// 0x66B720
CTaskComplexGoToPointAnyMeans::CTaskComplexGoToPointAnyMeans(int32 moveState, const CVector& posn, float radius, int32 modelId) : CTaskComplex() {
    m_Pos = posn;
    m_fRadius = radius;
    m_MoveState = static_cast<eMoveState>(moveState);
    m_nModelId = modelId;
    m_nStartTimeInMs = 0;
    m_nTimeOffsetInMs = 0;
    m_bRefreshTime = false;
    m_bResetStartTime = false;
    m_Vehicle = nullptr;
}

// optimized (DRY)
// 0x66B790
CTaskComplexGoToPointAnyMeans::CTaskComplexGoToPointAnyMeans(int32 moveState, const CVector& posn, CVehicle* vehicle, float radius, int32 modelId)
    : CTaskComplexGoToPointAnyMeans(moveState, posn, radius, modelId)
{
    m_Vehicle = vehicle;
    CEntity::SafeRegisterRef(m_Vehicle);
}

// 0x66B830
CTaskComplexGoToPointAnyMeans::~CTaskComplexGoToPointAnyMeans() {
    CEntity::SafeCleanUpRef(m_Vehicle);
}

// 0x6705D0
CTask* CTaskComplexGoToPointAnyMeans::CreateSubTask(int32 taskType, CPed* ped) {
    switch (taskType) {
    case TASK_SIMPLE_CREATE_CAR_AND_GET_IN:
        return new CTaskSimpleCreateCarAndGetIn(ped->IsInVehicle() ? ped->m_pVehicle->GetPosition() : ped->GetPosition(), m_nModelId);

    case TASK_COMPLEX_ENTER_CAR_AS_DRIVER:
        return new CTaskComplexEnterCarAsDriver(m_Vehicle);

    case TASK_COMPLEX_LEAVE_CAR:
        return new CTaskComplexLeaveCar(ped->m_pVehicle, 0, 0, true, false);

    case TASK_COMPLEX_CAR_DRIVE_TO_POINT:
        return new CTaskComplexDriveToPoint(m_Vehicle, m_Pos, -1.0f, false, (eModelID)-1, -1.0f, (eCarDrivingStyle)-1);

    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
        return new CTaskComplexGoToPointAndStandStill(m_MoveState, m_Pos, m_fRadius, StaticRef<float>(0x86FC88), false, false);

    case TASK_COMPLEX_FOLLOW_NODE_ROUTE:
        return new CTaskComplexFollowNodeRoute(m_MoveState, m_Pos, m_fRadius, StaticRef<float>(0x86FCA4), StaticRef<float>(0x86FCA8), false, -1, true);

    default:
        return nullptr;
    }
}

// 0x6728A0
CTask* CTaskComplexGoToPointAnyMeans::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_CREATE_CAR_AND_GET_IN:
    case TASK_COMPLEX_ENTER_CAR_AS_DRIVER:
        return CreateSubTask(ped->bInVehicle && ped->m_pVehicle ? TASK_COMPLEX_CAR_DRIVE_TO_POINT : TASK_COMPLEX_FOLLOW_NODE_ROUTE, ped);
    case TASK_COMPLEX_LEAVE_CAR:
        return CreateSubTask(TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL, ped);
    case TASK_COMPLEX_CAR_DRIVE_TO_POINT:
        return CreateSubTask(TASK_COMPLEX_LEAVE_CAR, ped);
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
    case TASK_COMPLEX_FOLLOW_NODE_ROUTE:
        return CreateSubTask(TASK_FINISHED, ped);
    case TASK_FINISHED:
    default:
        return nullptr;
    }
}

// 0x6729C0
CTask* CTaskComplexGoToPointAnyMeans::CreateFirstSubTask(CPed* ped) {
    if (m_Vehicle) {
        return CreateSubTask(ped->IsInVehicle() ? TASK_COMPLEX_CAR_DRIVE_TO_POINT : TASK_COMPLEX_ENTER_CAR_AS_DRIVER, ped);
    }

    if (ped->IsInVehicle() && ped->m_pVehicle->IsDriver(ped))
        return CreateSubTask(TASK_COMPLEX_CAR_DRIVE_TO_POINT, ped);
    else
        return CreateSubTask(TASK_COMPLEX_FOLLOW_NODE_ROUTE, ped);
}

// 0x672A50
CTask* CTaskComplexGoToPointAnyMeans::ControlSubTask(CPed* ped) {
    if (m_pSubTask->GetTaskType() != TASK_COMPLEX_FOLLOW_NODE_ROUTE) {
        return m_pSubTask;
    }

    if (m_nModelId != -1) {
        if (!m_bRefreshTime) {
            m_nStartTimeInMs  = CTimer::m_snTimeInMilliseconds;
            m_nTimeOffsetInMs = 3000;
            m_bRefreshTime    = true;
        }
        if (m_bResetStartTime) {
            m_nStartTimeInMs  = CTimer::m_snTimeInMilliseconds;
            m_bResetStartTime = false;
        }
        if (CTimer::m_snTimeInMilliseconds >= m_nStartTimeInMs + m_nTimeOffsetInMs) {
            const auto t     = CreateSubTask(TASK_SIMPLE_CREATE_CAR_AND_GET_IN, ped);
            m_nTimeOffsetInMs = 3000;
            m_bRefreshTime    = true;
            m_nStartTimeInMs  = CTimer::m_snTimeInMilliseconds;
            return t;
        }
    }

    // Only look for a (better) vehicle if we're still far away from the target
    const CVector dist = ped->GetPosition() - m_Pos;
    if (dist.SquaredMagnitude() > StaticRef<float>(0x86FD04)) {
        if (const auto veh = FindVehicleToCommandeer(ped)) { // 0x5FFEE0 - CPedIntelligence helper, unreversed here
            if (veh != m_Vehicle && CCarEnterExit::IsVehicleStealable(veh, ped)) {
                m_Vehicle = veh;
                const auto t = CreateSubTask(TASK_COMPLEX_ENTER_CAR_AS_DRIVER, ped);
                CEntity::SafeRegisterRef(m_Vehicle);
                return t;
            }
        }
    }
    return m_pSubTask;
}
