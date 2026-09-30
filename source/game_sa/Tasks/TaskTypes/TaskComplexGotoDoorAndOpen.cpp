#include "StdInc.h"

#include "TaskComplexGotoDoorAndOpen.h"

#include "TaskSimpleGoToPoint.h"
#include "TaskSimpleRunAnim.h"
#include "Events/EventAreaCodes.h"
#include "../TaskManager.h"

void CTaskComplexGotoDoorAndOpen::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexGotoDoorAndOpen, 0x86FFC0, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedOverloadedInstall(Constructor, "0", 0x66BB20, CTaskComplexGotoDoorAndOpen*(CTaskComplexGotoDoorAndOpen::*)(CObject *));
    RH_ScopedOverloadedInstall(Constructor, "1", 0x66BBA0, CTaskComplexGotoDoorAndOpen*(CTaskComplexGotoDoorAndOpen::*)(CVector const&, CVector const&));
    RH_ScopedInstall(Destructor, 0x66BC00);
    RH_ScopedInstall(Clone, 0x66BCA0);
    RH_ScopedInstall(CreateNextSubTask, 0x66C0D0);
    RH_ScopedInstall(CreateFirstSubTask, 0x66BD40);
    RH_ScopedInstall(ControlSubTask, 0x66C1F0);
}

// 0x66BB20
CTaskComplexGotoDoorAndOpen::CTaskComplexGotoDoorAndOpen(CObject* door) : CTaskComplex() {
    m_Object = door;
    m_nStartTime = 0;
    m_nOffsetTime = 0;
    byte30 = 0;
    m_bRefreshTime = false;
    m_nFlags = m_nFlags & 0xF0 | 1; // todo: flags
    CEntity::SafeRegisterRef(m_Object);
}

// 0x66BBA0
CTaskComplexGotoDoorAndOpen::CTaskComplexGotoDoorAndOpen(const CVector& start, const CVector& end) : CTaskComplex() {
    m_Object = nullptr;
    m_Start = start;
    m_End = end;
    m_nStartTime = 0;
    m_nOffsetTime = 0;
    byte30 = 0;
    m_bRefreshTime = false;
    m_nFlags &= 244u; // todo: flags
}

// 0x66BC00
CTaskComplexGotoDoorAndOpen::~CTaskComplexGotoDoorAndOpen() {
    CEntity::SafeCleanUpRef(m_Object);
    if ((m_nFlags & 8) != 0) { // transition finished
        CPad::GetPad()->bPlayerOnInteriorTransition = false;
    }
}

// 0x66BCA0
CTask* CTaskComplexGotoDoorAndOpen::Clone() const {
    if ((m_nFlags & 1) != 0) { // todo: flags
        return new CTaskComplexGotoDoorAndOpen(m_Object);
    } else {
        return new CTaskComplexGotoDoorAndOpen(m_Start, m_End);
    }
}

// 0x66BC80
bool CTaskComplexGotoDoorAndOpen::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    return priority == ABORT_PRIORITY_IMMEDIATE && m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, event);
}

// 0x66C0D0
CTask* CTaskComplexGotoDoorAndOpen::CreateNextSubTask(CPed* ped) {
    if (m_pSubTask->GetTaskType() != TASK_SIMPLE_GO_TO_POINT) {
        return nullptr;
    }

    if ((m_nFlags & 2) != 0) {
        if (ped->IsPlayer()) {
            CPad::GetPad()->bPlayerOnInteriorTransition = true;
        }
        return nullptr;
    }

    if (!ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_PARTIAL_ANIM)) {
        const auto animTask = new CTaskSimpleRunAnim(ANIM_GROUP_DEFAULT, ANIM_ID_IDLE, 8.0f, false);
        ped->GetTaskManager().SetTaskSecondary(animTask, TASK_SECONDARY_PARTIAL_ANIM);
    }

    m_nFlags |= 2;
    return new CTaskSimpleGoToPoint(PEDMOVE_WALK, m_End, 0.2f, false, false);
}

// 0x66BD40
CTask* CTaskComplexGotoDoorAndOpen::CreateFirstSubTask(CPed* ped) {
    if (ped->IsPlayer()) {
        CPad::GetPad()->bPlayerOnInteriorTransition = true;
        m_nFlags |= 8; // "interior transition" flag
    }

    // Register an area codes event on the ped
    CEventAreaCodes areaCodesEvent{ ped };
    ped->GetEventGroup().Add(&areaCodesEvent, false);
    ped->GetEventGroup().Remove(&areaCodesEvent);

    m_nStartTime  = CTimer::m_snTimeInMilliseconds;
    m_nOffsetTime = 1000;
    byte30        = 1;

    if (!m_Object) {
        if ((m_nFlags & 1) != 0) {
            return nullptr;
        }
        m_nFlags |= 2;
        return new CTaskSimpleGoToPoint(PEDMOVE_RUN, m_End, 0.5f, false, false);
    }

    // Compute the two goto points on either side of the door
    const CVector  doorPos = m_Object->GetPosition();
    const CMatrix& doorMat = m_Object->GetMatrix();
    const auto     up      = doorMat.GetUp();
    const auto     right   = doorMat.GetRight();

    // Offsets along the door plane (original values at 0x8D2EE0 / 0x8D2EE4 / 0x8D2EE8)
    const auto  doorToPed = doorPos - ped->GetPosition();
    const float side      = DotProduct(doorToPed, up) <= 1.0f ? -1.0f : 1.0f;

    const CVector base = doorPos + right * StaticRef<float>(0x8D2EE0);
    m_Start = base - right * (StaticRef<float>(0x8D2EE4) * side);
    m_End   = base + right * (StaticRef<float>(0x8D2EE8) * side);

    // Clear door flags (m_nObjectFlags bits 0x8 / 0x4: door collision being processed)
    if (m_Object->m_nObjectFlags & 0x8) {
        m_nFlags |= 4;
        m_Object->m_nObjectFlags &= ~0x8u;
        m_Object->m_nObjectFlags &= ~0x4u;
    }

    // Keep the idle anim blended while walking to the door
    if (RpAnimBlendClumpGetAssociation(ped->GetRpClump(), {ANIM_ID_IDLE})) {
        CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_DEFAULT, ANIM_ID_IDLE, 1000.0f);
    }

    return new CTaskSimpleGoToPoint(PEDMOVE_RUN, m_Start, 0.3f, false, false);
}

// 0x66C1F0
CTask* CTaskComplexGotoDoorAndOpen::ControlSubTask(CPed* ped) {
    const auto TryAbortSubTask = [&]() -> CTask* {
        if (!m_Object && (m_nFlags & 1) != 0) {
            if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT)) {
                if (ped->IsPlayer()) {
                    CPad::GetPad()->bPlayerOnInteriorTransition = true;
                }
                return nullptr;
            }
        }
        return m_pSubTask;
    };

    if (byte30 == 0) {
        return TryAbortSubTask();
    }

    if (m_bRefreshTime) {
        m_nStartTime   = CTimer::m_snTimeInMilliseconds;
        m_bRefreshTime = false;
    }
    if (CTimer::m_snTimeInMilliseconds < m_nStartTime + m_nOffsetTime) {
        return TryAbortSubTask();
    }

    if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT)) {
        if (ped->IsPlayer()) {
            CPad::GetPad()->bPlayerOnInteriorTransition = true;
        }
        return nullptr;
    }
    return m_pSubTask;
}
