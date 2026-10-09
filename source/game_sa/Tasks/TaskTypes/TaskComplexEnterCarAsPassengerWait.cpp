#include "StdInc.h"
#include "TaskComplexEnterCarAsPassengerWait.h"

// Интеграция 4 реверснутых функций (0x6408D0 CreateSubTask, 0x643E10 CreateNextSubTask,
// 0x643F00 CreateFirstSubTask, 0x640730 ControlSubTask) из фрагментов keep/decompiles.
// Все имена/адреса проверены по source/game_sa/:
#include "TaskComplexEnterCarAsPassengerTimed.h"      // CTaskComplexEnterCarAsPassengerTimed (CreateSubTask)
#include "TaskComplexSequence.h"                      // CTaskComplexSequence (CreateSubTask, 0xF4)
#include "TaskComplexTurnToFaceEntityOrCoord.h"       // CTaskComplexTurnToFaceEntityOrCoord (CreateSubTask, 0x66B890)
#include "TaskComplexGoToPointAndStandStill.h"        // CTaskComplexGoToPointAndStandStill (CreateSubTask 0x668120, ControlSubTask 0x46FE60)
#include "TaskSimpleStandStill.h"                     // CTaskSimpleStandStill (CreateSubTask, 0x62F310)
#include "TaskSimpleCarDriveTimed.h"                  // CTaskSimpleCarDriveTimed (CreateSubTask, 0x5FF940)
#include "TaskSimpleWaitUntilPedIsInCar.h"            // CTaskSimpleWaitUntilPedIsInCar (CreateSubTask, 0x649620)
#include "CarEnterExit.h"                             // CCarEnterExit::ComputeTargetDoorToEnterAsPassenger (0x64F190)
#include "PedGeometryAnalyser.h"                      // CPedGeometryAnalyser::ComputeClosestSurfacePoint (0x5F3B70)
#include "PedGroups.h"                                // CPedGroups::GetPedsGroup (0x5F7E80)
#include "PedGroupMembership.h"                       // CPedGroupMembership::GetLeader (0x5F69A0)
#include "Tasks/TaskManager.h"                        // CTaskManager::FindActiveTaskByType (0x681740)
#include "Audio/Enums/PedSpeechContexts.h"            // CTX_GLOBAL_CAR_GET_IN / CTX_GLOBAL_CAR_WAIT_FOR_ME
// Ped::bInVehicle/m_pVehicle/IsPlayer/GetTaskManager, CVehicle::IsPassenger, CPlaceable::GetPosition,
// CModelInfo::ms_modelInfoPtrs, Core/Vector.h (SquaredMagnitude), eTaskType/eMoveState - уже покрыты StdInc.h

// Глобалы оригинала (.data, base 0x400000) - значения сняты с оригинального exe:
static auto& flt_86FC7C = StaticRef<float>(0x86FC7C); // 0.5f - change rate mult (turn-to-face); см. TaskComplexGangFollower.cpp:37
static auto& flt_86FC80 = StaticRef<float>(0x86FC80); // 0.2f - max heading (turn-to-face); см. TaskComplexGangFollower.cpp:38
static auto& flt_86FC88 = StaticRef<float>(0x86FC88); // 2.0f - fMoveStateRadius (GoToPointAndStandStill); см. TaskComplexGoToAttractor.cpp:56
static auto& flt_858CE8 = StaticRef<float>(0x858CE8); // 1.5f - запас к радиусу bounding sphere; см. Shadows.cpp:1655
static auto& flt_859038 = StaticRef<float>(0x859038); // 0.025f - порог квадрата расстояния перенацеливания (снято с exe)

void CTaskComplexEnterCarAsPassengerWait::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexEnterCarAsPassengerWait, 0x86E778, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x63B320);
    RH_ScopedInstall(Destructor, 0x63B3C0);

    RH_ScopedInstall(CreateSubTask, 0x6408D0);
    RH_ScopedVMTInstall(Clone, 0x63D850);
    RH_ScopedVMTInstall(GetTaskType, 0x63B3B0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x643E10);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x643F00);
    RH_ScopedVMTInstall(ControlSubTask, 0x640730);
}

// 0x63B320
CTaskComplexEnterCarAsPassengerWait::CTaskComplexEnterCarAsPassengerWait(CVehicle* target, CPed* waitFor, bool forceFrontSeat, eMoveState ms) :
    m_Car{target},
    m_WaitForPed{waitFor},
    m_bForceFrontSeat{forceFrontSeat},
    m_MoveState{ms}
{
    CEntity::SafeRegisterRef(m_Car);
    CEntity::SafeRegisterRef(m_WaitForPed);
}

// NOTSA (for 0x63D850)
CTaskComplexEnterCarAsPassengerWait::CTaskComplexEnterCarAsPassengerWait(const CTaskComplexEnterCarAsPassengerWait& o) :
    CTaskComplexEnterCarAsPassengerWait{
        o.m_Car,
        o.m_WaitForPed,
        o.m_bForceFrontSeat,
        o.m_MoveState
    }
{
}

// 0x63B3C0
CTaskComplexEnterCarAsPassengerWait::~CTaskComplexEnterCarAsPassengerWait() {
    CEntity::SafeCleanUpRef(m_Car);
    CEntity::SafeCleanUpRef(m_WaitForPed);
}

// 0x6408D0
// NOTE: в хедере репо сигнатура возвращает CTaskComplexEnterCarAsPassengerTimed*, хотя оригинал
// создаёт задачи разных типов; оригинал просто возвращает указатель из EAX, поэтому даункаст
// static_cast от CTask* для одиночного наследования - бит-в-бит тождественен.
CTaskComplexEnterCarAsPassengerTimed* CTaskComplexEnterCarAsPassengerWait::CreateSubTask(int32 taskType, CPed* ped) {
    CTask* task{};
    switch (taskType) {
    case eTaskType::TASK_SIMPLE_WAIT_UNTIL_PED_IN_CAR: /* 0x341 */
        task = new CTaskSimpleWaitUntilPedIsInCar(m_WaitForPed); // 0x649620
        break;

    case eTaskType::TASK_COMPLEX_SEQUENCE: { /* 0xF4 */
        const auto seq = new CTaskComplexSequence(); // 0x632BD0
        seq->AddTask(new CTaskComplexTurnToFaceEntityOrCoord(m_Car, flt_86FC7C /* 0.5f */, flt_86FC80 /* 0.2f */)); // 0x66B890 + 0x632D10
        seq->AddTask(new CTaskSimpleStandStill(1000, false, false, 8.0f)); // 0x62F310 + 0x632D10
        task = seq;
        break;
    }

    case eTaskType::TASK_COMPLEX_ENTER_CAR_AS_PASSENGER_TIMED: { /* 0x2C8 */
        const auto targetSeat = m_bForceFrontSeat
            ? CCarEnterExit::ComputeTargetDoorToEnterAsPassenger(m_Car, 0) // 0x64F190
            : 0;
        const auto timedTask = new CTaskComplexEnterCarAsPassengerTimed(m_Car, targetSeat, -1, true); // 0x63B030
        *reinterpret_cast<eMoveState*>(reinterpret_cast<char*>(timedTask) + 0x1C) = m_MoveState;      // +0x1C = m_MoveState (приватное, см. TaskComplexEnterCarAsPassengerTimed.h:46)
        task = timedTask;
        break;
    }

    case eTaskType::TASK_SIMPLE_CAR_DRIVE_TIMED: /* 0x2CF */
        task = new CTaskSimpleCarDriveTimed(m_Car, 0); // 0x5FF940
        break;

    case eTaskType::TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL: { /* 0x387 */
        CVector targetPoint{};
        if (CPedGeometryAnalyser::ComputeClosestSurfacePoint(*ped, *m_Car, targetPoint)) { // 0x5F3B70
            task = new CTaskComplexGoToPointAndStandStill(m_MoveState, targetPoint, 1.5f /* 0x3FC00000 */, flt_86FC88 /* 2.0f */, false, false); // 0x668120
        } else {
            // Ближайшую точку найти не удалось: цель - позиция машины, радиус = радиус bounding sphere + 1.5f
            const auto radius = m_Car->GetModelInfo()->GetColModel()->GetBoundRadius() + flt_858CE8; // ms_modelInfoPtrs[car->m_nModelIndex]->m_pColModel(+0x14)->m_boundSphere.m_fRadius(+0x24) + 0x858CE8
            task = new CTaskComplexGoToPointAndStandStill(m_MoveState, m_Car->GetPosition() /* m_matrix(+0x14) ? +0x30 : this+4 */, radius, flt_86FC88, false, false);
        }
        break;
    }

    default: /* 0x516 = TASK_FINISHED и всё прочее - подзадачи нет */
        return nullptr;
    }
    return static_cast<CTaskComplexEnterCarAsPassengerTimed*>(task);
}

// 0x643E10
CTask* CTaskComplexEnterCarAsPassengerWait::CreateNextSubTask(CPed* ped) {
    if (!m_Car) {
        return CreateSubTask(eTaskType::TASK_FINISHED, ped); /* 0x516 - всегда nullptr */
    }

    switch (m_pSubTask->GetTaskType()) {
    case eTaskType::TASK_SIMPLE_CAR_DRIVE_TIMED: /* 0x2CF - уже едем, конец */
        break;

    case eTaskType::TASK_COMPLEX_ENTER_CAR_AS_PASSENGER_TIMED: /* 0x2C8 - попытка сесть не удалась */
        if (!ped->bInVehicle /* *(ped+0x46C) & 0x100 */
         && ++m_EnterCarFails < 16 /* 0x10 */
         && m_Car) {
            return CreateSubTask(eTaskType::TASK_COMPLEX_SEQUENCE, ped);
        }
        break;

    case eTaskType::TASK_COMPLEX_SEQUENCE:              /* 0xF4 */
    case eTaskType::TASK_SIMPLE_WAIT_UNTIL_PED_IN_CAR:  /* 0x341 */
        if (m_Car) {
            return CreateSubTask(eTaskType::TASK_COMPLEX_ENTER_CAR_AS_PASSENGER_TIMED, ped);
        }
        break;

    case eTaskType::TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL: /* 0x387 */
        if (m_WaitForPed) {
            return CreateSubTask(eTaskType::TASK_SIMPLE_WAIT_UNTIL_PED_IN_CAR, ped);
        }
        break;

    default:
        return nullptr;
    }
    return CreateSubTask(eTaskType::TASK_FINISHED, ped); /* LAB_00643eeb - 0x516 */
}

// 0x643F00
CTask* CTaskComplexEnterCarAsPassengerWait::CreateFirstSubTask(CPed* ped) {
    // Мы уже пассажир в нашей машине - садимся за руль (ждать больше нечего)
    if (ped->bInVehicle && ped->m_pVehicle && ped->m_pVehicle == m_Car) {
        if (m_Car->IsPassenger(ped)) { // 0x6D1BD0
            return CreateSubTask(eTaskType::TASK_SIMPLE_CAR_DRIVE_TIMED, ped);
        }
    }
    if (!m_WaitForPed) {
        if (m_Car) {
            return CreateSubTask(eTaskType::TASK_COMPLEX_ENTER_CAR_AS_PASSENGER_TIMED, ped);
        }
    } else if (m_Car) {
        return CreateSubTask(eTaskType::TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL, ped);
    }
    return CreateSubTask(eTaskType::TASK_FINISHED, ped); /* 0x516 */
}

// 0x640730
CTask* CTaskComplexEnterCarAsPassengerWait::ControlSubTask(CPed* ped) {
    // Один раз произносим фразу, пока ждём
    if (!m_bPlayedSample) {
        if (m_WaitForPed) {
            if (ped->IsPlayer() /* 0x5DF8F0 */
             && ped->GetTaskManager().FindActiveTaskByType(eTaskType::TASK_SIMPLE_CAR_GET_IN /* 0x327 */) /* 0x681740 */) {
                m_bPlayedSample = true;
                ped->Say(CTX_GLOBAL_CAR_GET_IN /* 0x23 = 35 */, 0, 1.0f /* 0x3F800000 */, false, false, false); // 0x5EFFE0
            }
        } else {
            // Ждём не конкретного педа: голосуем, если мы в группе (как follower) и в ней есть лидер
            if (const auto group = CPedGroups::GetPedsGroup(ped) /* 0x5F7E80 */) {
                if (group->GetMembership().GetLeader() /* 0x5F69A0 */
                 && ped->IsPlayer()
                 && ped->GetTaskManager().FindActiveTaskByType(eTaskType::TASK_SIMPLE_CAR_GET_IN)) {
                    m_bPlayedSample = true;
                    ped->Say(CTX_GLOBAL_CAR_WAIT_FOR_ME /* 0x28 = 40 */, 0, 1.0f, false, false, false);
                }
            }
        }
    }

    // Держим цель go-to-point актуальной: цель - ближайшая точка на машине (или сама машина)
    if (m_Car && m_pSubTask->GetTaskType() == eTaskType::TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL /* 0x387 */) {
        const auto goToTask = static_cast<CTaskComplexGoToPointAndStandStill*>(m_pSubTask);

        CVector targetPoint{};
        // NOTE: при неудаче оригинал оставляет точку неинициализированной на стеке и всё равно
        // считает по ней дистанцию (артефакт декомпила); здесь - нулевая инициализация.
        const bool bFound = CPedGeometryAnalyser::ComputeClosestSurfacePoint(*ped, *m_Car, targetPoint); // 0x5F3B70

        const auto distSq = (targetPoint - goToTask->m_vecTargetPoint /* +0x10 */).SquaredMagnitude(); // 0x406DA0
        if (flt_859038 /* 0.025f */ < distSq) {
            if (bFound) {
                goToTask->GoToPoint(targetPoint, 1.5f /* 0x3FC00000 */, flt_86FC88 /* 2.0f */, false); // 0x46FE60
            } else {
                goToTask->GoToPoint(
                    m_Car->GetPosition(),
                    m_Car->GetModelInfo()->GetColModel()->GetBoundRadius() + flt_858CE8 /* 1.5f */,
                    flt_86FC88,
                    false
                );
            }
        }
    }
    return m_pSubTask;
}
