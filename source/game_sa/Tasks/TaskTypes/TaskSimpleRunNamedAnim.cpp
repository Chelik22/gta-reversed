// NOTE(integration, батч w2-11-mixed): GetTaskType (0x61AA90) и Clone (0x61B770) вынесены из
// TaskSimpleRunNamedAnim.h в этот .cpp. Требуется сопутствующее изменение хедера — убрать
// инлайн-определения GetTaskType/Clone (оставить объявления), см. STATUS.md п.2 батча и
// _integration/new_Tasks_TaskTypes_TaskSimpleRunNamedAnim.h. Без него — redefinition.
#include "StdInc.h"

#include "TaskSimpleRunNamedAnim.h"

void CTaskSimpleRunNamedAnim::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleRunNamedAnim, 0x86D54C, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedOverloadedInstall(Constructor, "Default", 0x6674B0, CTaskSimpleRunNamedAnim*(CTaskSimpleRunNamedAnim::*)());
    RH_ScopedOverloadedInstall(Constructor, "Anim", 0x61A990, CTaskSimpleRunNamedAnim*(CTaskSimpleRunNamedAnim::*)(char const*, char const*, int32, float, int32, bool, bool, bool, bool));
    RH_ScopedInstall(Destructor, 0x61BF10);

    //RH_ScopedInstall(FinishRunAnimMovePedCB, 0x61AAA0, { .Reversed = false }); // не из батча, не объявлен в хедере — не трогаем
    //RH_ScopedInstall(StartAnim, 0x61BB10, { .Reversed = false }); // не из батча, не объявлен в хедере — не трогаем
    RH_ScopedVMTInstall(Clone, 0x61B770);
    RH_ScopedVMTInstall(GetTaskType, 0x61AA90);
    RH_ScopedVMTInstall(ProcessPed, 0x61BF20, { .Reversed = false }); // не из батча — redirect-стаб остаётся
    RH_ScopedInstall(OffsetPedPosition, 0x61AB00);
}

// 0x6674B0
CTaskSimpleRunNamedAnim::CTaskSimpleRunNamedAnim() :
    CTaskSimpleAnim{ false }
{
    // Rest done in header
}

// todo: check m_vecOffsetAtEnd initialization
// 0x61A990
CTaskSimpleRunNamedAnim::CTaskSimpleRunNamedAnim(
    const char* animName,
    const char* animGroupName,
    uint32 animFlags,
    float blendDelta,
    uint32 endTime,
    bool bDontInterrupt,
    bool bRunInSequence,
    bool bOffsetPed,
    bool bHoldLastFrame
) :
    CTaskSimpleAnim(bHoldLastFrame),
    m_Time{ endTime },
    m_animFlags{ animFlags }
{
    m_bDontInterrupt = bDontInterrupt;
    m_bRunInSequence = bRunInSequence;
    m_bOffsetAtEnd = bOffsetPed;
    m_fBlendDelta = blendDelta;
    strcpy_s(m_animName, animName);
    strcpy_s(m_animGroupName, animGroupName);
    if (const auto block = CAnimManager::GetAnimationBlock(m_animGroupName)) {
        m_pAnimHierarchy = CAnimManager::GetAnimation(m_animName, block);
    }
}

// 0x61AA90
eTaskType CTaskSimpleRunNamedAnim::GetTaskType() const {
    return Type; // TASK_SIMPLE_NAMED_ANIM (eTaskType.h:153; TASK_SIMPLE_ANIM = 400 => 401 = 0x191)
}

// 0x61B770
CTask* CTaskSimpleRunNamedAnim::Clone() const {
    // Оригинал: operator new(0x64 == sizeof, VALIDATE_SIZE) + ктор; булевы флаги из байта m_nFlags
    // (CTaskSimpleAnim), бит-раскладка подтверждена декомпилом (w2-11/0x61B770.cpp).
    return new CTaskSimpleRunNamedAnim(m_animName, m_animGroupName,
                                       m_animFlags, m_fBlendDelta, m_Time,
                                       (m_nFlags & 2) >> 1, (m_nFlags & 0x10) >> 4, (m_nFlags & 0x20) >> 5, (m_nFlags & 4) >> 2);
}

// 0x61BF20
bool CTaskSimpleRunNamedAnim::ProcessPed(CPed* ped) {
    return plugin::CallMethodAndReturn<bool, 0x61BF20>(this, ped);
}

// 0x61AB00
void CTaskSimpleRunNamedAnim::OffsetPedPosition(CPed* ped) {
    ped->UpdateRpHAnim();
    ped->m_bDontUpdateHierarchy = true;
    auto& pos = ped->GetPosition();
    pos += ped->m_matrix->TransformVector(m_vecOffsetAtEnd);
    m_bOffsetAvailable = false;
}
