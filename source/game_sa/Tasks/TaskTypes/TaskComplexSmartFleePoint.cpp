#include "StdInc.h"
#include "TaskComplexSmartFleePoint.h"

#include "TaskSimpleCarSetPedOut.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleRunAnim.h"
#include "TaskSimpleTired.h"
#include "TaskComplexWander.h"
#include "TaskComplexWanderFlee.h"
#include "../TaskComplexSequence.h"
#include "General.h"

void CTaskComplexSmartFleePoint::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexSmartFleePoint, 0x86f744, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x65BD20);
    RH_ScopedInstall(Destructor, 0x65BDB0);

    RH_ScopedInstall(SetDefaultTaskWanderDir, 0x65BE00);
    RH_ScopedInstall(ComputeFleeDir, 0x65BE40);
    RH_ScopedInstall(CreateSubTask, 0x65BE80);
    RH_ScopedInstall(SetFleePosition, 0x65C3C0);

    RH_ScopedVMTInstall(Clone, 0x65CED0, {.reversed = false});
    RH_ScopedVMTInstall(GetTaskType, 0x65BDA0, {.reversed = false});
    RH_ScopedVMTInstall(MakeAbortable, 0x65BDC0, {.reversed = false});
    RH_ScopedVMTInstall(CreateNextSubTask, 0x65C0C0, {.reversed = false});
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x65C140, {.reversed = false});
    RH_ScopedVMTInstall(ControlSubTask, 0x65C1E0, {.reversed = false});
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

CTaskComplexSmartFleePoint::CTaskComplexSmartFleePoint(const CTaskComplexSmartFleePoint& o)
    : CTaskComplexSmartFleePoint{o.m_fleePoint, o.m_doScream, o.m_safeDist, o.m_fleeTimeMs}
{
}

// 0x65BE00
int8 CTaskComplexSmartFleePoint::SetDefaultTaskWanderDir(CPed* ped) {
    if (const auto task = ped->GetIntelligence()->GetActivePrimaryTask()) {
        if (task->GetTaskType() == TASK_COMPLEX_WANDER) {
            // 0x669D50 - applies the default wander direction to the sub task (unreversed here)
            plugin::Call<0x669D50, CTask*>(m_pSubTask);
        }
    }
    return 0;
}

// 0x65BE40
uint32 CTaskComplexSmartFleePoint::ComputeFleeDir(CPed* ped) {
    // NOTE: The original computes the node heading but discards the result
    // (CGeneral::GetNodeHeadingFromVector has no other side effect)
    const auto& pedPos = ped->GetPosition();
    return CGeneral::GetNodeHeadingFromVector(pedPos.x - m_fleePoint.x, pedPos.y - m_fleePoint.y);
}

// 0x65BE80
CTask* CTaskComplexSmartFleePoint::CreateSubTask(eTaskType taskType, CPed* ped) {
    switch (taskType) {
    case TASK_COMPLEX_LEAVE_ANY_CAR:
        return new CTaskSimpleCarSetPedOut(nullptr, TARGET_DOOR_FRONT_LEFT, false, true);

    case TASK_SIMPLE_STAND_STILL: {
        SetDefaultTaskWanderDir(ped);
        return new CTaskSimpleStandStill(0, false, false, 8.0f);
    }

    case TASK_COMPLEX_FLEE_POINT: {
        if (m_moveState != PEDMOVE_SPRINT) {
            return nullptr;
        }
        // Sequence: hit response anim -> tired -> done
        const auto seq = new CTaskComplexSequence();
        seq->AddTask(new CTaskSimpleRunAnim(ANIM_GROUP_DEFAULT, ANIM_ID_FLEE_LKAROUND_01, 4.0f, false));
        seq->AddTask(new CTaskSimpleTired(2000));
        return seq;
    }

    case TASK_COMPLEX_WANDER:
        return new CTaskComplexWanderFlee(m_moveState, m_fleeDir);

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
    return plugin::CallMethodAndReturn<bool, 0x65BDC0, CTaskComplexSmartFleePoint*, CPed*, eAbortPriority, CEvent const*>(this, ped, priority, event);
}

// 0x65C0C0
CTask* CTaskComplexSmartFleePoint::CreateNextSubTask(CPed* ped) {
    return plugin::CallMethodAndReturn<CTask*, 0x65C0C0, CTaskComplexSmartFleePoint*, CPed*>(this, ped);
}

// 0x65C140
CTask* CTaskComplexSmartFleePoint::CreateFirstSubTask(CPed* ped) {
    return plugin::CallMethodAndReturn<CTask*, 0x65C140, CTaskComplexSmartFleePoint*, CPed*>(this, ped);
}

// 0x65C1E0
CTask* CTaskComplexSmartFleePoint::ControlSubTask(CPed* ped) {
    return plugin::CallMethodAndReturn<CTask*, 0x65C1E0, CTaskComplexSmartFleePoint*, CPed*>(this, ped);
}
