#include "StdInc.h"

#include "TaskComplexGoToAttractor.h"
#include "PedAtmAttractor.h"
#include "TaskComplexSequence.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskSimpleSlideToCoord.h"
#include "TaskSimpleStandStill.h"
#include "PedPlacement.h"

void CTaskComplexGoToAttractor::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexGoToAttractor, 0x86FF3C, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x66B640);
    RH_ScopedInstall(Destructor, 0x66B6A0);

    // Clone body is inline in TaskComplexGoToAttractor.h:21 and is equivalent to the
    // decompile of 0x66D130 (operator new(0x2C) + field-by-field copy, see batch
    // w2-09 fragment 0x66D130.cpp) - the inline body is hooked in place.
    RH_ScopedVMTInstall(Clone, 0x66D130);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x66B6C0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x670420);
}

// 0x66B640
CTaskComplexGoToAttractor::CTaskComplexGoToAttractor(CPedAttractor* attractor, const CVector& pos, float heading, float attrTime, int32 queueNumber, eMoveState ms) : CTaskComplex() {
    m_Attractor = attractor;
    m_vecAttrPosn = pos;
    m_fAttrHeading = heading;
    m_MoveState = ms;
    m_fAttrTime = attrTime;
    m_nQueueNumber = queueNumber;
}

// 0x66B6B0
bool CTaskComplexGoToAttractor::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    return m_pSubTask->MakeAbortable(ped, priority, event);
}

// 0x66B6C0
// Fragment w2-09 0x66B6C0.cpp: body verified against decompile 1:1
// (0x5EE190 GetPedAttractorManager, 0x5EFE20 BroadcastArrival, 0x5EB590 GetHeadOfQueue;
// ped+0x474 & 0x200000 == bUseAttractorInstantly). Only the hook flag was removed.
CTask* CTaskComplexGoToAttractor::CreateNextSubTask(CPed* ped) {
    GetPedAttractorManager()->BroadcastArrival(ped, m_Attractor);
    if (ped->bUseAttractorInstantly && m_Attractor->GetHeadOfQueue() != ped) {
        ped->bUseAttractorInstantly = false;
    }
    return nullptr;
}

// 0x670420
// Fragment w2-09 0x670420.cpp. Decompile notes:
// - vtable[0] of the attractor == CPedAttractor::GetType(); == 4 (PED_ATTRACTOR_SHELTER) -> PEDMOVE_RUN.
// - 0x668120 CTaskComplexGoToPointAndStandStill ctor with DAT_0086FC84=0.5f, DAT_0086FC88=2.0f (read from exe).
// - 0x66C3E0 CTaskSimpleSlideToCoord ctor (m_vecAttrPosn, m_fAttrHeading, 0.5f).
// - 0x616920 CPedPlacement::FindZCoorForPed (original mutates CVector* in place; refactored by-value
//   version returns std::pair<CVector, bool> - take .first); ped+0x55C = m_fAimingRotation,
//   ped+0x558 = m_fCurrentRotation; 0x62F310 CTaskSimpleStandStill(0, false, false, 8.0f).
CTask* CTaskComplexGoToAttractor::CreateFirstSubTask(CPed* ped) {
    auto moveState = m_MoveState;
    if (m_Attractor->GetType() == PED_ATTRACTOR_SHELTER) {
        moveState = PEDMOVE_RUN;
    }

    if (ped->bUseAttractorInstantly) {
        // Attractor is used instantly: teleport to the attractor and stand still
        m_vecAttrPosn           = CPedPlacement::FindZCoorForPed(m_vecAttrPosn).first;
        ped->GetPosition()      = m_vecAttrPosn;
        ped->m_fAimingRotation  = m_fAttrHeading;
        ped->m_fCurrentRotation = m_fAttrHeading;
        return new CTaskSimpleStandStill(0, false, false, 8.0f);
    }

    const auto seq = new CTaskComplexSequence();
    seq->AddTask(new CTaskComplexGoToPointAndStandStill(moveState, m_vecAttrPosn, 0.5f, 2.0f, false, false));
    seq->AddTask(new CTaskSimpleSlideToCoord(m_vecAttrPosn, m_fAttrHeading, 0.5f));
    return seq;
}

// 0x66B710
CTask* CTaskComplexGoToAttractor::ControlSubTask(CPed* ped) {
    return m_pSubTask;
}
