#include "StdInc.h"

#include "TaskComplexFollowPointRoute.h"
#include "TaskComplexWalkRoundCar.h"

// Интеграция фрагментов батча w2-16-mixed (0x654370, 0x6544F0, 0x656BB0, 0x658200).
// Все пути проверены по source/game_sa/:
#include "CarEnterExit.h"                              // CCarEnterExit::IsPlayerToQuitCarEnter
#include "PedGeometryAnalyser.h"                       // CPedGeometryAnalyser::*, CPointRoute
#include "Entity/Ped/Ped.h"                            // CPed::IsPlayer/GetIntelligence/GetTaskManager/GetPosition
#include "Entity/Vehicle/Vehicle.h"                    // CVehicle::vehicleFlags/m_pTowingVehicle/m_pVehicleBeingTowed/m_matrix
#include "PedIntelligence.h"                           // CPedIntelligence::m_TaskMgr
#include "Tasks/TaskManager.h"                         // CTaskManager::FindTaskByType/m_aPrimaryTasks, ePrimaryTasks
#include "Tasks/TaskTypes/TaskSimpleAchieveHeading.h"  // CTaskSimpleAchieveHeading
#include "Tasks/TaskTypes/TaskSimpleStandStill.h"      // CTaskSimpleStandStill
#include "Tasks/TaskTypes/TaskComplexEnterCar.h"       // CTaskComplexEnterCar (m_EnterCarStartTime, protected, +0x4C)
#include "Enums/eTaskType.h"                           // TASK_COMPLEX_*
#include "General.h"                                   // CGeneral::LimitRadianAngle/GetRadianAngleBetweenPoints
#include "Timer.h"                                     // CTimer::GetTimeInMS
// DotProduct/SquaredMagnitude/Normalized/NormaliseAndMag - Core/Vector.h (уже покрыт StdInc.h)

// FILE-SCOPE (из фрагмента 0x656BB0): глобальный float MARGIN бокс-анализатора (0x8D22B0).
// Это тот же глобал, что читается в PedGeometryAnalyser.cpp:270 (StaticRef<float>(0x8D22B0));
// ComputeRouteRoundBigCar временно выставляет его в 0.7f (0x3F333333) на время вызовов
// CPedGeometryAnalyser::ComputeClosestSurfacePoint.
static auto& g_BoundingBoxMargin = StaticRef<float>(0x8D22B0);

void CTaskComplexWalkRoundCar::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexWalkRoundCar, 0x86f308, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x6541B0);
    RH_ScopedInstall(Destructor, 0x656B00);

    RH_ScopedInstall(SetNewVehicle, 0x654290);
    RH_ScopedInstall(CreateRouteTask, 0x6542E0);
    RH_ScopedInstall(ComputeRouteRoundSmallCar, 0x6544F0);
    RH_ScopedInstall(GoingForDoor, 0x654720);
    RH_ScopedInstall(ComputeRouteRoundBigCar, 0x656BB0);
    RH_ScopedInstall(ComputeRoute, 0x657B80);

    RH_ScopedVMTInstall(Clone, 0x655B00);
    RH_ScopedVMTInstall(GetTaskType, 0x654280);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x656B70);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x658200);
    RH_ScopedVMTInstall(ControlSubTask, 0x654370);
}

// 0x6541B0
CTaskComplexWalkRoundCar::CTaskComplexWalkRoundCar(eMoveState moveState, CVector const& targetPt, CVehicle* vehicle, bool isPedGoingForCarDoor, uint8 forceThisDirectionRoundCar) :
    m_MoveState{(uint8)moveState},
    m_bIsPedGoingForCarDoor{isPedGoingForCarDoor},
    m_ForceThisDirectionRoundCar{forceThisDirectionRoundCar},
    m_TargetPt{targetPt},
    m_Veh{vehicle},
    m_Route{new CPointRoute{}}
{
    CEntity::SafeRegisterRef(m_Veh);
}

CTaskComplexWalkRoundCar::CTaskComplexWalkRoundCar(const CTaskComplexWalkRoundCar& o) :
    CTaskComplexWalkRoundCar{
        (eMoveState)o.m_MoveState,
        o.m_TargetPt,
        o.m_Veh,
        o.m_bIsPedGoingForCarDoor,
        o.m_ForceThisDirectionRoundCar
    }
{
}

// 0x656B00
CTaskComplexWalkRoundCar::~CTaskComplexWalkRoundCar() {
    CEntity::SafeCleanUpRef(m_Veh);
    delete m_Route;
}


// 0x654290
void CTaskComplexWalkRoundCar::SetNewVehicle(CVehicle * vehicle, uint8 forceThisDirectionRoundCar) {
    if (notsa::IsFixBugs()) {
        if (vehicle == m_Veh) {
            return;
        }
    }
    CEntity::ChangeEntityReference(m_Veh, vehicle);
    m_ForceThisDirectionRoundCar    = forceThisDirectionRoundCar;
    m_bFirstSubTaskNeedsToBeCreated = true;
    m_Route->Clear();
}

// 0x6542E0
CTask* CTaskComplexWalkRoundCar::CreateRouteTask(CPed*) const {
    if (m_Route->IsEmpty()) {
        return nullptr;
    }
    return new CTaskComplexFollowPointRoute{ (eMoveState)m_MoveState, *m_Route };
}

// 0x6544F0
float CTaskComplexWalkRoundCar::ComputeRouteRoundSmallCar(CPed* ped) {
    CVector corners[4];
    CVector planes[4];
    float   planeDots[4];

    const auto zPos = ped->GetPosition().z;
    CPedGeometryAnalyser::ComputeEntityBoundingBoxCornersUncached(zPos, *m_Veh, corners);
    CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanes(zPos, *m_Veh, &planes, planeDots);

    // Расстояние (со знаком) от педа до плоскости стороны, с которой он подошёл к машине
    const auto hitSide = CPedGeometryAnalyser::ComputeEntityHitSide(*ped, *m_Veh);
    const auto sideDist = hitSide == -1
        ? 0.f
        : DotProduct(planes[hitSide], ped->GetPosition()) + planeDots[hitSide];

    if (m_bIsPedGoingForCarDoor) {
        // Если цель внутри бокса машины - выталкиваем её наружу вдоль ближайшей плоскости
        unsigned i = 0;
        for (; i < 4; i++) {
            if (DotProduct(planes[i], m_TargetPt) + planeDots[i] >= 0.f) {
                break;
            }
        }
        if (i == 4) {
            if (const auto side = CPedGeometryAnalyser::ComputeEntityHitSide(m_TargetPt, *m_Veh); side != -1) {
                const auto push = 0.05f - (DotProduct(planes[side], m_TargetPt) + planeDots[side]); // 0x858C28 = 0.05f
                m_TargetPt += planes[side] * push;
            }
        }
    }

    // В оригинале младший ниббл знакорасширяется (декомпил 0x6544F0:102: (int)(char)(x << 4) >> 4),
    // поэтому простой каст (int8_t)m_ForceThisDirectionRoundCar не эквивалентен для значений 8..15.
    const auto forceDir = ((int8_t)(m_ForceThisDirectionRoundCar << 4)) >> 4;

    m_DirectionGoingRoundCar = (uint8)CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox(
        *ped,
        *m_Veh,
        m_TargetPt,
        *m_Route,
        forceDir
    ); // биты 2..5 байта 0xD (маска 0x3C в декомпиле)

    return sideDist;
}

// 0x656BB0
float CTaskComplexWalkRoundCar::ComputeRouteRoundBigCar(CPed* ped) {
    const auto pedSide    = CPedGeometryAnalyser::ComputeEntityHitSide(*ped, *m_Veh);
    const auto targetSide = CPedGeometryAnalyser::ComputeEntityHitSide(m_TargetPt, *m_Veh);

    const auto savedMargin = g_BoundingBoxMargin;

    CVector pedClosestPt{}, targetClosestPt{};

    if (pedSide == targetSide) {
        // Пед и цель с одной стороны машины - просто огибаем обе точки
        g_BoundingBoxMargin = 0.7f;
        CPedGeometryAnalyser::ComputeClosestSurfacePoint(*ped, *m_Veh, pedClosestPt);
        CPedGeometryAnalyser::ComputeClosestSurfacePoint(m_TargetPt, *m_Veh, targetClosestPt);
        g_BoundingBoxMargin = savedMargin;

        m_Route->AddUnlessFull(pedClosestPt);
    } else {
        g_BoundingBoxMargin = 0.7f;
        CPedGeometryAnalyser::ComputeClosestSurfacePoint(*ped, *m_Veh, pedClosestPt);
        CPedGeometryAnalyser::ComputeClosestSurfacePoint(m_TargetPt, *m_Veh, targetClosestPt);
        g_BoundingBoxMargin = savedMargin;

        // Временно ставим педа на поверхность бокса, а цель - на ближайшую точку бокса,
        // чтобы пересчитать маршрут по "малой" схеме
        const auto savedPedPos   = ped->GetPosition();
        const auto savedTargetPt = m_TargetPt;
        ped->GetPosition() = pedClosestPt;
        m_TargetPt         = targetClosestPt;

        ComputeRouteRoundSmallCar(ped);

        ped->GetPosition() = savedPedPos;
        m_TargetPt         = savedTargetPt;

        // 0x63A040: снять копию текущего маршрута и очистить его
        CVector savedPts[8];
        const auto savedCount = m_Route->GetSize();
        for (auto i = 0u; i < savedCount; i++) {
            savedPts[i] = (*m_Route)[i];
        }
        m_Route->Clear();

        m_Route->AddUnlessFull(pedClosestPt);
        for (auto i = 0u; i < savedCount; i++) {
            m_Route->AddUnlessFull(savedPts[i]);
        }

        g_BoundingBoxMargin = 0.7f;
        CPedGeometryAnalyser::ComputeClosestSurfacePoint(m_TargetPt, *m_Veh, targetClosestPt);
        g_BoundingBoxMargin = savedMargin;
    }

    m_Route->AddUnlessFull(targetClosestPt);

    return 0.f; // DAT_00858B50
}

// 0x657B80
float CTaskComplexWalkRoundCar::ComputeRoute(CPed* ped) {
    return m_Veh->vehicleFlags.bIsBig
        ? ComputeRouteRoundBigCar(ped)
        : ComputeRouteRoundSmallCar(ped);
}

// 0x656B70
CTask* CTaskComplexWalkRoundCar::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_ACHIEVE_HEADING:
        return CreateRouteTask(ped);
    }
    return nullptr;
}

// 0x658200
CTask* CTaskComplexWalkRoundCar::CreateFirstSubTask(CPed* ped) {
    m_Route->Clear();

    const auto distSide = m_Veh->vehicleFlags.bIsBig
        ? ComputeRouteRoundBigCar(ped)
        : ComputeRouteRoundSmallCar(ped);

    if (m_Route->IsEmpty()) {
        return nullptr;
    }

    // Таймаут на обход машины: 15000 (20000 для PEDMOVE_WALK), x4 если машина кого-то буксирует (0x858B90 = 4.f)
    auto timeLimit = 15000;
    if (m_MoveState == PEDMOVE_WALK) {
        timeLimit = 20000;
    }
    if (m_Veh->m_pTowingVehicle || m_Veh->m_pVehicleBeingTowed) {
        timeLimit = (int32)((float)timeLimit * 4.f);
    }
    m_Timer.m_nStartTime = CTimer::GetTimeInMS();
    m_Timer.m_nInterval  = timeLimit;
    m_Timer.m_bStarted   = true;

    // Запоминаем позицию и ориентацию машины
    m_VehPos      = m_Veh->GetPosition();
    m_VehMatFwd   = m_Veh->m_matrix->GetForward();
    m_VehMatRight = m_Veh->m_matrix->GetRight();

    const auto& routeStart       = (*m_Route)[0];
    const auto  distToRouteStart = (routeStart - ped->GetPosition()).NormaliseAndMag();

    const auto& pedFwd = ped->m_matrix->GetForward();

    const auto maxWanderDist = [this] {
        switch ((eMoveState)m_MoveState) {
        case PEDMOVE_WALK: return 2.f;
        case PEDMOVE_RUN:  return 4.f;
        default:           return 6.f;
        }
    }();

    if (ped->IsPlayer()) {
        // В оригинале: CTaskManager::FindTaskByType(TASK_PRIMARY_PRIMARY, TASK_COMPLEX_ENTER_CAR_AS_DRIVER)
        if (const auto enterCarTask = ped->GetTaskManager().FindTaskByType(TASK_PRIMARY_PRIMARY, TASK_COMPLEX_ENTER_CAR_AS_DRIVER)) {
            // CTaskComplexEnterCar::m_EnterCarStartTime - protected-член (TaskComplexEnterCar.h:65),
            // прямого доступа и публичного геттера нет; читаем по смещению, как в
            // TaskComplexGangFollower.cpp:288. Оффсет 0x4C подтверждён декомпилом 0x658200:104
            // (task+0x4C при sizeof(CTaskComplexEnterCar) == 0x50).
            static_assert(sizeof(CTaskComplexEnterCar) == 0x50);
            m_EnterCarStartTime = *reinterpret_cast<const int32*>(reinterpret_cast<const char*>(enterCarTask) + 0x4C);
        }
    }

    // Если маршрут далеко, пед идёт прямо к машине и целевая точка впереди - просто идём по маршруту
    if (!m_Veh->vehicleFlags.bIsBig
        && maxWanderDist < distToRouteStart
        && maxWanderDist < distSide
        && DotProduct((routeStart - ped->GetPosition()).Normalized(), pedFwd) > 0.f // 0x86F424 = 0.f
    ) {
        return CreateRouteTask(ped);
    }

    // Иначе: кадр stand-still + разворот в сторону первой точки маршрута
    CTaskSimpleStandStill standStill{ 0, false, false, 8.f };
    standStill.ProcessPed(ped);

    // ВНИМАНИЕ: в оригинале (декомпил 0x658200:124) вызов имеет вид
    //   CGeneral::GetRadianAngleBetweenPoints(routeStart.x - pos.x, routeStart.y - pos.y, 0.f, 0.f),
    // т.е. это угол вектора (ped - routeStart), а НЕ (routeStart - ped) - как ни странно.
    const auto heading = CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(
        routeStart.x - ped->GetPosition().x,
        routeStart.y - ped->GetPosition().y,
        0.f,
        0.f
    ));

    return new CTaskSimpleAchieveHeading{ heading, 1.0f, 0.1f };
}

// 0x654370
CTask* CTaskComplexWalkRoundCar::ControlSubTask(CPed* ped) {
    if (m_bFirstSubTaskNeedsToBeCreated) {
        m_bFirstSubTaskNeedsToBeCreated = false;
        return CreateFirstSubTask(ped);
    }

    bool quitEnteringCar = false;

    if (ped->IsPlayer() && m_Veh && m_EnterCarStartTime != -1) {
        if (CCarEnterExit::IsPlayerToQuitCarEnter(ped, m_Veh, m_EnterCarStartTime, m_pSubTask)
            && m_pSubTask->GetTaskType() == TASK_COMPLEX_FOLLOW_POINT_ROUTE
        ) {
            quitEnteringCar = true;
        }
    }

    if (m_pSubTask->GetTaskType() == TASK_COMPLEX_FOLLOW_POINT_ROUTE) {
        if (m_Timer.IsOutOfTime()) {
            quitEnteringCar = false;
        } else if (!quitEnteringCar) {
            // Маршрут ещё валиден: машина не сдвинулась и не повернулась => продолжаем идти по маршруту
            // (0x86F140 = 0.0625 = 0.25^2, 0x86F144 = 0.9)
            if (m_Veh) {
                if ((m_VehPos - m_Veh->GetPosition()).SquaredMagnitude() <= 0.0625f
                    && DotProduct(m_Veh->m_matrix->GetForward(), m_VehMatFwd) >= 0.9f
                    && DotProduct(m_Veh->m_matrix->GetRight(), m_VehMatRight) >= 0.9f
                ) {
                    return m_pSubTask;
                }
            }
        }
    }

    if (!m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
        return m_pSubTask;
    }

    if (quitEnteringCar) {
        // Прерываем посадку за руль: задача TASK_COMPLEX_ENTER_CAR_AS_DRIVER в слоте TASK_PRIMARY_PRIMARY
        // (декомпил 0x654370:68: intelligence+0x10 == m_TaskMgr.m_aPrimaryTasks[3], т.к. m_TaskMgr @ +0x4)
        if (const auto task = ped->GetIntelligence()->m_TaskMgr.m_aPrimaryTasks[TASK_PRIMARY_PRIMARY]) {
            if (task->GetTaskType() == TASK_COMPLEX_ENTER_CAR_AS_DRIVER) {
                task->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nullptr);
            }
        }
    }
    return nullptr;
}
