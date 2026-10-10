// ============================================================================
// INTEGRATED FILE — source/game_sa/Tasks/TaskTypes/TaskComplexGotoDoorAndOpen.cpp
// Функции: CreateFirstSubTask (0x66BD40), CreateNextSubTask (0x66C0D0), ControlSubTask (0x66C1F0)
//
// ИСТОЧНИКИ: фрагменты batch_out/fresh/0x66BD40.cpp, 0x66C0D0.cpp, 0x66C1F0.cpp
// (основа — stash_wt, расхождения с декомпилами исправлены; все три верифицированы
// VERIFY-wf5a: 0x66BD40 — FIXED(2), 0x66C0D0/0x66C1F0 — OK).
//
// ПРАВКИ ИНТЕГРАТОРА: без изменений тел против (уже фикснутых) фрагментов;
// сняты {.Reversed = false} у трёх хуков.
//
// Ключевые фиксы верификации, вошедшие в тела:
//  - CEventAreaCodes добавляется в ГЛОБАЛЬНУЮ группу: GetEventGlobalGroup()->Add(&e, false)
//    (RwHelper.h:9, EventGroup.h:19), не в группу педа;
//  - PEDMOVE_WALK (4), не PEDMOVE_RUN; 0x99 = ANIM_ID_WALK_DOORPARTIAL = 153
//    (AnimationEnums.h:313), не ANIM_ID_IDLE;
//  - точки сторон двери: база = doorPos + m_right*0.75f (0x8D2EE0), смещения ±0.5f/2.0f
//    (0x8D2EE4/0x8D2EE8) вдоль m_forward (ось Y двери); знак по
//    (doorPos - pedPos)·forward <= 0.0f (0x858B50);
//  - флаги двери door+0x40 = CPhysical::m_nPhysicalFlags (Physical.h:106):
//    бит 0x8 = PHYSICAL_COLLIDABLE, бит 0x4 = PHYSICAL_DISABLE_COLLISION_FORCE;
//  - приватные поля CMatrix -> GetRight()/GetForward() (Core/Matrix.h:91,94);
//  - RpAnimBlendClumpGetAssociation(clump, {ANIM_ID_SPRINT}) — в репо сигнатура с
//    std::initializer_list<AnimationId> (common.h:267); animId 2 = ANIM_ID_SPRINT
//    (AnimationEnums.h:160).
//
// Сверено в репо: CTaskSimpleGoToPoint(eMoveState, CVector, float, bool, bool)
// (TaskSimpleGoToPoint.h:22), CTaskSimpleRunAnim(AssocGroupId, AnimationId, float, bool)
// (TaskSimpleRunAnim.h:22), CEventAreaCodes(CPed*) (EventAreaCodes.h:16),
// CAnimManager::BlendAnimation(RpClump*, AssocGroupId, AnimationId, float)
// (AnimManager.h:100), CPed::m_nAnimGroup (AssocGroupId, Ped.h:273),
// CPad::GetPad()->bPlayerOnInteriorTransition (Pad.h:72,206),
// CTask::MakeAbortable(ped, priority = URGENT, event = nullptr) (Task.h:68),
// CTaskManager::GetTaskSecondary/SetTaskSecondary (TaskManager.h:88,179).
// ============================================================================

#include "StdInc.h"

#include "TaskComplexGotoDoorAndOpen.h"

#include "TaskSimpleGoToPoint.h"
#include "TaskSimpleRunAnim.h"
#include "Events/EventAreaCodes.h"

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

    if ((m_nFlags & 2) != 0) { // уже идём от двери — переход завершён
        if (ped->IsPlayer()) {
            CPad::GetPad()->bPlayerOnInteriorTransition = true; // pad+0x10E |= 8
        }
        return nullptr;
    }

    if (!ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_PARTIAL_ANIM)) {
        const auto animTask = new CTaskSimpleRunAnim(ANIM_GROUP_DEFAULT, ANIM_ID_WALK_DOORPARTIAL, 8.0f, false);
        ped->GetTaskManager().SetTaskSecondary(animTask, TASK_SECONDARY_PARTIAL_ANIM);
    }

    m_nFlags |= 2;
    return new CTaskSimpleGoToPoint(PEDMOVE_WALK, m_End, 0.2f, false, false); // 0x3E4CCCCD
}

// 0x66BD40
CTask* CTaskComplexGotoDoorAndOpen::CreateFirstSubTask(CPed* ped) {
    if (ped->IsPlayer()) {
        CPad::GetPad()->bPlayerOnInteriorTransition = true; // pad+0x10E |= 8
        m_nFlags |= 8;
    }

    // Добавить событие area codes в глобальную группу (инициирует обработку зон у педов)
    CEventAreaCodes areaCodesEvent{ ped };
    GetEventGlobalGroup()->Add(&areaCodesEvent, false);

    m_nStartTime  = CTimer::GetTimeInMS();
    m_nOffsetTime = 1000;
    byte30        = 1;

    if (!m_Object) {
        if ((m_nFlags & 1) != 0) {
            return nullptr;
        }
        m_nFlags |= 2;
        return new CTaskSimpleGoToPoint(PEDMOVE_WALK, m_End, 0.5f, false, false);
    }

    // Две точки по разные стороны двери
    const CVector   doorPos = m_Object->GetPosition();
    const CMatrix*  doorMat = m_Object->m_matrix; // в оригинале null-чека нет
    const CVector&  right   = doorMat->GetRight();   // строка 0
    const CVector&  fwd     = doorMat->GetForward(); // строка 1, ось Y двери

    const CVector base = doorPos + right * StaticRef<float>(0x8D2EE0); // 0.75f

    // На какой стороне двери стоит пед: (doorPos - pedPos)·forward <= 0.0f (DAT_00858B50)
    const float side = DotProduct(doorPos - ped->GetPosition(), fwd) <= StaticRef<float>(0x858B50) ? -1.0f : 1.0f;
    m_Start = base + fwd * (side * StaticRef<float>(0x8D2EE4)); // -0.5f
    m_End   = base + fwd * (side * StaticRef<float>(0x8D2EE8)); // 2.0f

    // Подготовить дверь к открыванию
    if (m_Object->m_nPhysicalFlags & 0x8 /* bCollidable */) {
        m_nFlags |= 4;
        m_Object->m_nPhysicalFlags &= ~0x8u; // bCollidable = false
        m_Object->m_nPhysicalFlags &= ~0x4u; // bDisableCollisionForce = false
    }

    // Подмешать анимацию ходьбы, пока пед идёт к двери
    if (RpAnimBlendClumpGetAssociation(ped->GetRpClump(), {ANIM_ID_SPRINT} /* animId 2 */)) {
        CAnimManager::BlendAnimation(ped->GetRpClump(), ped->m_nAnimGroup, ANIM_ID_WALK, 1000.0f);
    }

    return new CTaskSimpleGoToPoint(PEDMOVE_WALK, m_Start, 0.35f, false, false); // 0x3EB33333
}

// 0x66C1F0
CTask* CTaskComplexGotoDoorAndOpen::ControlSubTask(CPed* ped) {
    const auto TryAbortSubTask = [&]() -> CTask* {
        if (!m_Object && (m_nFlags & 1) != 0) {
            if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT)) {
                if (ped->IsPlayer()) {
                    CPad::GetPad()->bPlayerOnInteriorTransition = true; // pad+0x10E |= 8
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
        m_nStartTime   = CTimer::GetTimeInMS();
        m_bRefreshTime = false;
    }
    if (CTimer::GetTimeInMS() < m_nOffsetTime + m_nStartTime) {
        return TryAbortSubTask();
    }

    if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT)) {
        if (ped->IsPlayer()) {
            CPad::GetPad()->bPlayerOnInteriorTransition = true; // pad+0x10E |= 8
        }
        return nullptr;
    }
    return m_pSubTask;
}
