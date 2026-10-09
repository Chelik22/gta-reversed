#include "StdInc.h"
#include "TaskComplexSmartFleePoint.h"

#include "Ragdoll/IKChainManager.h"
#include "TaskComplexLeaveAnyCar.h"
#include "TaskComplexSequence.h"
#include "TaskComplexWander.h"
#include "TaskComplexWanderFlee.h"
#include "TaskSimpleRunAnim.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleTired.h"

void CTaskComplexSmartFleePoint::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexSmartFleePoint, 0x86f744, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x65BD20);
    RH_ScopedInstall(Destructor, 0x65BDB0);

    RH_ScopedInstall(SetDefaultTaskWanderDir, 0x65BE00);
    RH_ScopedInstall(ComputeFleeDir, 0x65BE40);
    RH_ScopedInstall(CreateSubTask, 0x65BE80);
    RH_ScopedInstall(SetFleePosition, 0x65C3C0);

    RH_ScopedVMTInstall(Clone, 0x65CED0);       // Body is inline in the header
    RH_ScopedVMTInstall(GetTaskType, 0x65BDA0); // Body is inline in the header
    RH_ScopedVMTInstall(MakeAbortable, 0x65BDC0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x65C0C0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x65C140);
    RH_ScopedVMTInstall(ControlSubTask, 0x65C1E0);
}

// 0x65BD20
CTaskComplexSmartFleePoint::CTaskComplexSmartFleePoint(CVector const& fleePos, bool doScream, float safeDist, int32 fleeTimeMs) :
    m_fleePoint{fleePos},
    m_doScream{doScream},
    m_safeDist{safeDist},
    m_fleeTimeMs{fleeTimeMs}
{
    m_timer.Start(m_fleeTimeMs);
}

// NOTSA
CTaskComplexSmartFleePoint::CTaskComplexSmartFleePoint(const CTaskComplexSmartFleePoint& o)
    : CTaskComplexSmartFleePoint{o.m_fleePoint, o.m_doScream, o.m_safeDist, o.m_fleeTimeMs}
{
}

// 0x65BE00
int8 CTaskComplexSmartFleePoint::SetDefaultTaskWanderDir(CPed* ped) {
    if (const auto task = ped->GetTaskManager().GetTaskPrimary(TASK_PRIMARY_DEFAULT)) {
        if (task->GetTaskType() == TASK_COMPLEX_WANDER) {
            plugin::Call<0x669D50, CTask*>(m_pSubTask); // Not reversed; applies the default wander direction to the subtask
        }
    }
    return 0;
}

// 0x65BE40
uint32 CTaskComplexSmartFleePoint::ComputeFleeDir(CPed* ped) {
    const auto& pedPos = ped->GetPosition();
    return CGeneral::GetNodeHeadingFromVector(pedPos.x - m_fleePoint.x, pedPos.y - m_fleePoint.y);
}

// 0x65BE80
CTask* CTaskComplexSmartFleePoint::CreateSubTask(eTaskType taskType, CPed* ped) {
    switch (taskType) {
    case TASK_COMPLEX_LEAVE_ANY_CAR: // 0x2CA
        return new CTaskComplexLeaveAnyCar{0, false, true};

    case TASK_SIMPLE_STAND_STILL: { // 0xCB
        SetDefaultTaskWanderDir(ped);
        return new CTaskSimpleStandStill{0, false, false, 8.0f};
    }

    case TASK_COMPLEX_SEQUENCE: { // 0xF4
        if (m_moveState != PEDMOVE_RUN) {
            return nullptr;
        }
        // Sequence: "look around" anim -> tired -> end
        const auto seq = new CTaskComplexSequence{};
        seq->AddTask(new CTaskSimpleRunAnim{ANIM_GROUP_DEFAULT, ANIM_ID_FLEE_LKAROUND_01, 4.0f, false});
        seq->AddTask(new CTaskSimpleTired{2000u});
        return seq;
    }

    case TASK_COMPLEX_WANDER: // 0x390
        return new CTaskComplexWanderFlee{m_moveState, m_fleeDir};

    default:
        return nullptr;
    }
}

// 0x65C3C0
int8 CTaskComplexSmartFleePoint::SetFleePosition(CVector const& pos, float safeDist, bool doScream) {
    if (m_fleePoint == pos && m_safeDist == safeDist) {
        m_doScream = doScream;
        return 0;
    }
    m_fleePoint           = pos;
    m_safeDist            = safeDist;
    m_hasFleePointChanged = true;
    m_doScream            = doScream;
    return 0;
}

// 0x65BDC0
bool CTaskComplexSmartFleePoint::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    if (priority == ABORT_PRIORITY_LEISURE) {
        m_fleeTimeMs          = -1;
        m_timer.m_nStartTime  = CTimer::m_snTimeInMilliseconds;
        m_timer.m_nInterval   = -1; // Infinite timer
        m_timer.m_bStarted    = true;
    }
    return m_pSubTask->MakeAbortable(ped, priority, event);
}

// 0x65C0C0
CTask* CTaskComplexSmartFleePoint::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_STAND_STILL:
        return CreateSubTask(TASK_COMPLEX_SEQUENCE, ped);

    case TASK_COMPLEX_SEQUENCE:
        return CreateSubTask(TASK_FINISHED, ped);

    case TASK_COMPLEX_LEAVE_ANY_CAR:
        m_fleeDir = (uint8)ComputeFleeDir(ped); // Original only keeps the low byte
        return CreateSubTask(TASK_COMPLEX_WANDER, ped);

    default:
        return nullptr;
    }
}

// 0x65C140
CTask* CTaskComplexSmartFleePoint::CreateFirstSubTask(CPed* ped) {
    m_initalPos = ped->GetPosition();

    if (ped->IsInVehicle()) {
        return CreateSubTask(TASK_COMPLEX_LEAVE_ANY_CAR, ped);
    }

    m_fleeDir = (uint8)ComputeFleeDir(ped); // Original only keeps the low byte
    return CreateSubTask(TASK_COMPLEX_WANDER, ped);
}

// 0x65C1E0
CTask* CTaskComplexSmartFleePoint::ControlSubTask(CPed* ped) {
    const auto subTaskType = m_pSubTask->GetTaskType();
    if (subTaskType != TASK_COMPLEX_WANDER) {
        if (g_ikChainMan.IsLooking(ped)) {
            g_ikChainMan.AbortLookAt(ped, 250);
        }
        return m_pSubTask;
    }

    if (m_doScream) {
        ped->Say(CTX_GLOBAL_PAIN_PANIC, 0, 0.1f);
    }

    static_cast<CTaskComplexWander*>(m_pSubTask)->m_nMoveState = m_moveState;

    if (m_hasFleePointChanged) {
        m_hasFleePointChanged = false;
        if (m_timer.m_bStarted) {
            m_timer.m_nStartTime = CTimer::m_snTimeInMilliseconds;
            m_timer.m_nInterval  = m_fleeTimeMs;
            m_timer.m_bStarted   = true;
        }
        if (const auto dir = (uint8)ComputeFleeDir(ped); m_fleeDir != dir) {
            m_fleeDir = dir;
            plugin::Call<0x669D30, uint8>(dir); // Not reversed; __cdecl, single argument = new wander direction
        }
    } else {
        if (m_timer.m_bStarted && m_timer.IsOutOfTime()) {
            return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
        }

        const auto& pedPos = ped->GetPosition();
        const auto  sqDist = m_safeDist * m_safeDist;
        if (sqDist < (m_fleePoint - pedPos).SquaredMagnitude()
            && sqDist < (m_initalPos - pedPos).SquaredMagnitude()) {
            return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
        }
    }

    if (m_moveState == PEDMOVE_RUN && !g_ikChainMan.IsLooking(ped)) {
        if (CGeneral::GetRandomNumberInRange(0, 100) < 6) {
            g_ikChainMan.LookAt("TaskSmartFleePoint", ped, nullptr, 2000, eBoneTag::BONE_UNKNOWN, &m_fleePoint, false, 0.25f, 500, 3, false);
        }
    }

    return m_pSubTask;
}
