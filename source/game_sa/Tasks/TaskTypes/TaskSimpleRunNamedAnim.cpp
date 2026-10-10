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
    RH_ScopedVMTInstall(ProcessPed, 0x61BF20); // свежий реверс волны 5 (0x61BF20.cpp), сверено с дизасмом
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
    if (m_bOffsetAvailable) { // 0x40
        OffsetPedPosition(ped);
    }

    if (!m_bIsFinished) {
        // Иерархия не загружена (IFP выгрузлен стримингом) => задача завершена
        if (!m_pAnimHierarchy || !m_pAnimHierarchy->m_pSequences) {
            return true;
        }

        if (m_bDontInterrupt && (!(m_animFlags & ANIMATION_IS_FINISH_AUTO_REMOVE) || (m_animFlags & ANIMATION_IS_LOOPED))) {
            ped->GetIntelligence()->m_eventGroup.RemoveInvalidEvents(true);
            ped->GetIntelligence()->m_eventGroup.Reorganise();
        }

        if (m_Timer.m_bStarted && m_Timer.IsOutOfTime()) {
            if (!(m_animFlags & ANIMATION_IS_FINISH_AUTO_REMOVE)) { // 0x8
                if (m_pAnim) {
                    // Пустой колбек (0x4D1390) — анимацию не трогать при завершении задачи
                    m_pAnim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
                }
                m_bIsFinished = true;
                m_pAnim = nullptr;
                return true;
            }
            // ABI: оригинал зовёт vtable-слот 6 (0x61C05E: call [vtbl+0x18]) = MakeAbortable с аргументами (ped, 0, 0)
            MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nullptr);
        }

        if (!m_pAnim) {
            const auto assoc = RpAnimBlendClumpGetAssociation(ped->GetRpClump(), true, m_pAnimHierarchy);
            if (assoc && assoc->m_pCallbackData != nullptr) { // +0x38
                return true;
            }
            plugin::CallMethod<0x61BB10, CTaskSimpleRunNamedAnim*, CPed*>(this, ped); // StartAnim — не реверснут
        }
        return m_bIsFinished;
    }

    if (!m_bOffsetAtEnd) { // 0x20
        const auto assoc = RpAnimBlendClumpGetAssociation(ped->GetRpClump(), true, m_pAnimHierarchy);

        CVector bonePos;
        ped->GetBonePosition(&bonePos, BONE_SPINE1, false);

        if (m_fBlendDelta > 100.0f && assoc) { // 100.0f (0x858628)
            if ((ped->GetPosition() - bonePos).Magnitude() > 1.0f) { // 1.0f (0x858624); в репо CVector нет Norm() - это Magnitude()
                assoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE; // 0x4
                assoc->m_BlendDelta = m_fBlendDelta * -1.0f;      // * -1.0f (0x858C1C)
            }
        }
    }
    return true;
}

// 0x61AB00
void CTaskSimpleRunNamedAnim::OffsetPedPosition(CPed* ped) {
    ped->UpdateRpHAnim();
    ped->m_bDontUpdateHierarchy = true;
    auto& pos = ped->GetPosition();
    pos += ped->m_matrix->TransformVector(m_vecOffsetAtEnd);
    m_bOffsetAvailable = false;
}
