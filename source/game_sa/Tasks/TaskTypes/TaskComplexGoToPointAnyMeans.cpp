// ============================================================================
// INTEGRATED FILE — source/game_sa/Tasks/TaskTypes/TaskComplexGoToPointAnyMeans.cpp
// Функции: CreateSubTask (0x6705D0), CreateNextSubTask (0x6728A0), ControlSubTask (0x672A50)
//
// ИСТОЧНИКИ: фрагменты batch_out/fresh/0x6705D0.cpp, 0x6728A0.cpp, 0x672A50.cpp
// (основа — stash_wt, расхождения с декомпилами исправлены; все три верифицированы
// VERIFY-wf5a со статусом OK — сверены с D:\projects\keep\decompiles\0x6705D0.c и др.)
//
// ПРАВКИ ИНТЕГРАТОРА: без изменений тел; сняты {.Reversed = false} у трёх хуков.
// Сигнатуры сверены с хедерами репо: CreateSubTask(int32, CPed*),
// CreateNextSubTask/ControlSubTask(CPed*) (TaskComplexGoToPointAnyMeans.h:30-34);
// поля m_Pos@0xC/m_MoveState@0x18/m_fRadius@0x1C/m_Vehicle@0x20/m_nModelId@0x24/
// m_nStartTimeInMs@0x28/m_nTimeOffsetInMs@0x2C/m_bRefreshTime@0x30/m_bResetStartTime@0x31
// = декомпилу (VALIDATE_SIZE 0x34).
//
// Сверено в репо: CTaskSimpleCreateCarAndGetIn(CVector const&, int32)
// (TaskSimpleCreateCarAndGetIn.h:19), CTaskComplexEnterCarAsDriver(CVehicle*)
// (TaskComplexEnterCarAsDriver.h:16), CTaskComplexLeaveCar(veh,0,0,true,false)
// (TaskComplexLeaveCar.h:34), CTaskComplexDriveToPoint(veh,pt,speed,int,eModelID,radius,style)
// (TaskComplexDriveToPoint.h:23), CTaskComplexGoToPointAndStandStill(ms,pt,r,r2,false,false)
// (TaskComplexGoToPointAndStandStill.h:25), CTaskComplexFollowNodeRoute(ms,pt,r,3,2,false,-1,true)
// (TaskComplexFollowNodeRoute.h:12), CPed::bInVehicle/IsInVehicle() (Ped.h:139/589),
// CCarEnterExit::IsVehicleStealable(const CVehicle*, const CPed*) (CarEnterExit.h:55),
// CEntity::RegisterReference(CEntity**) (Entity.h:254),
// CPedIntelligence::m_vehicleScanner (PedIntelligence.h:55) — сыр-вызов 0x5FFEE0
// (геттер CVehicleScanner+0x4C) оставлен по VERIFY-wf5a: в repo-лейауте сканера
// поле на +0x48, поэтому сыр-вызов корректен.
// ============================================================================

#include "StdInc.h"

#include "TaskComplexGoToPointAnyMeans.h"

#include "TaskSimpleCreateCarAndGetIn.h"
#include "TaskComplexEnterCarAsDriver.h"
#include "TaskComplexLeaveCar.h"
#include "TaskComplexDriveToPoint.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskComplexFollowNodeRoute.h"
#include "CarEnterExit.h"

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
    case TASK_SIMPLE_CREATE_CAR_AND_GET_IN: { // 0x340
        // В оригинале — просто GetPosition() педа (инлайн ветки m_matrix/m_transform), не позиция машины
        return new CTaskSimpleCreateCarAndGetIn(ped->GetPosition(), m_nModelId);
    }
    case TASK_COMPLEX_ENTER_CAR_AS_DRIVER: // 0x2BD
        return new CTaskComplexEnterCarAsDriver(m_Vehicle);

    case TASK_COMPLEX_LEAVE_CAR: // 0x2C0
        return new CTaskComplexLeaveCar(ped->m_pVehicle, 0, 0, true, false);

    case TASK_COMPLEX_CAR_DRIVE_TO_POINT: // 0x2C6
        // 0x2C6: последний аргумент = DRIVING_STYLE_STOP_FOR_CARS (0), не -1 (декомпил 0x6705D0.c:58-59)
        return new CTaskComplexDriveToPoint(m_Vehicle, m_Pos, -1.0f, 0, (eModelID)-1, -1.0f, DRIVING_STYLE_STOP_FOR_CARS);

    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL: // 0x387
        return new CTaskComplexGoToPointAndStandStill(m_MoveState, m_Pos, m_fRadius, 2.0f /* DAT_0086FC88 */, false, false);

    case TASK_COMPLEX_FOLLOW_NODE_ROUTE: // 0x38A
        return new CTaskComplexFollowNodeRoute(m_MoveState, m_Pos, m_fRadius, 3.0f /* DAT_0086FCA4 */, 2.0f /* DAT_0086FCA8 */, false, -1, true);

    default:
        return nullptr;
    }
}

// 0x6728A0
CTask* CTaskComplexGoToPointAnyMeans::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_CREATE_CAR_AND_GET_IN: // 0x340
        // В оригинале проверяется ТОЛЬКО bInVehicle (без m_pVehicle); в 0x2BD — полная проверка (IsInVehicle)
        if (ped->bInVehicle)
            return CreateSubTask(TASK_COMPLEX_CAR_DRIVE_TO_POINT, ped);   // 0x2C6
        return CreateSubTask(TASK_COMPLEX_FOLLOW_NODE_ROUTE, ped);        // 0x38A

    case TASK_COMPLEX_ENTER_CAR_AS_DRIVER: // 0x2BD
        if (ped->IsInVehicle())
            return CreateSubTask(TASK_COMPLEX_CAR_DRIVE_TO_POINT, ped);   // 0x2C6
        return CreateSubTask(TASK_COMPLEX_FOLLOW_NODE_ROUTE, ped);        // 0x38A

    case TASK_COMPLEX_LEAVE_CAR: // 0x2C0
        return CreateSubTask(TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL, ped); // 0x387

    case TASK_COMPLEX_CAR_DRIVE_TO_POINT: // 0x2C6
        return CreateSubTask(TASK_COMPLEX_LEAVE_CAR, ped);                // 0x2C0

    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL: // 0x387
    case TASK_COMPLEX_FOLLOW_NODE_ROUTE:           // 0x38A
        return CreateSubTask(TASK_FINISHED, ped);                         // 0x516

    case TASK_FINISHED: // 0x516
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
    if (m_pSubTask->GetTaskType() != TASK_COMPLEX_FOLLOW_NODE_ROUTE) { // 0x38A
        return m_pSubTask;
    }

    if (m_nModelId != -1) {
        if (!m_bRefreshTime) {
            m_nStartTimeInMs  = CTimer::GetTimeInMS();
            m_nTimeOffsetInMs = 3000;
            m_bRefreshTime    = true;
        }
        if (m_bResetStartTime) {
            m_nStartTimeInMs  = CTimer::GetTimeInMS();
            m_bResetStartTime = false;
        }
        if (m_nStartTimeInMs + m_nTimeOffsetInMs <= CTimer::GetTimeInMS()) {
            const auto t      = CreateSubTask(TASK_SIMPLE_CREATE_CAR_AND_GET_IN, ped); // 0x340
            m_nTimeOffsetInMs = 3000;
            m_bRefreshTime    = true;
            m_nStartTimeInMs  = CTimer::GetTimeInMS();
            return t;
        }
    }

    // Ищем машину «на угон», только если до цели ещё далеко (0x86FD04 = 50.0f; сравниваются квадраты)
    const CVector dist = ped->GetPosition() - m_Pos;
    if (StaticRef<float>(0x86FD04) * StaticRef<float>(0x86FD04) /* 50.0f*50.0f */ < dist.SquaredMagnitude()) {
        // 0x5FFEE0: геттер CVehicleScanner+0x4C — ближайший кандидат машины из сканера педа
        if (const auto veh = plugin::CallMethodAndReturn<CVehicle*, 0x5FFEE0, CVehicleScanner*>(&ped->GetIntelligence()->m_vehicleScanner)) {
            if (veh != m_Vehicle && CCarEnterExit::IsVehicleStealable(veh, ped)) {
                m_Vehicle = veh;
                const auto t = CreateSubTask(TASK_COMPLEX_ENTER_CAR_AS_DRIVER, ped); // 0x2BD
                m_Vehicle->RegisterReference(reinterpret_cast<CEntity**>(&m_Vehicle)); // 0x571B70
                return t;
            }
        }
    }
    return m_pSubTask;
}
