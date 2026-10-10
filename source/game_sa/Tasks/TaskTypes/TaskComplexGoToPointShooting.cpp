// ============================================================================
// INTEGRATED FILE — source/game_sa/Tasks/TaskTypes/TaskComplexGoToPointShooting.cpp
// Функции: GetTaskType (0x668CC0), Clone (0x66CE10)
//
// ИСТОЧНИКИ: фрагменты batch_out/fresh/0x668CC0.cpp, 0x66CE10.cpp — оба OK по VERIFY-wf5a.
//
// GetTaskType: тело в хедере (inline, `return Type;` = TASK_COMPLEX_GO_TO_POINT_SHOOTING
// = 0x385 = 901, сверено по Enums/eTaskType.h); в .cpp тела нет, как у
// CTaskComplexGoToPointAiming (хук 0x668860 по inline-методу) — снимается только флаг.
//
// Clone (0x66CE10): оператор new(0x38) == sizeof(CTaskComplexGoToPointShooting);
// 0x668790 = CTaskComplexGoToPointAiming ctor; ПОСЛЕ ктора пишется vtable 0x86FE2C —
// vtable ИМЕННО CTaskComplexGoToPointShooting (RH_ScopedVirtualClass(..., 0x86fe2c, 11)),
// т.е. клон — объект CTaskComplexGoToPointShooting, а НЕ базового CTaskComplexGoToPointAiming.
// Аргументы из декомпила: m_moveState(0xC), &m_movePos(0x20), m_aimAtEntity(0x10),
// m_aimPos(0x14, by value), m_moveTargetRadius(0x2C), m_slowDownDistance(0x30).
// Бит m_newTargetSet (0x34) НЕ копируется (задаётся ктором) — как в оригинале.
//
// ХЕДЕР: inline-Clone удалён из хедера (см. final_Tasks_TaskTypes_TaskComplexGoToPointShooting.h) —
// иначе определение в .cpp конфликтовало бы с inline-телом, создававшим чужую vtable.
// ============================================================================

#include "StdInc.h"

#include "TaskComplexGoToPointShooting.h"

void CTaskComplexGoToPointShooting::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexGoToPointShooting, 0x86fe2c, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x668C70);

    RH_ScopedVMTInstall(Clone, 0x66CE10);
    RH_ScopedVMTInstall(GetTaskType, 0x668CC0);
}

// 0x668C70
CTaskComplexGoToPointShooting::CTaskComplexGoToPointShooting(
    eMoveState moveState,
    const CVector& movePos,
    CEntity* aimAtEntity,
    CVector aimPos,
    const float targetRadius,
    const float slowDownDist
) :
    CTaskComplexGoToPointAiming{
        moveState,
        movePos,
        aimAtEntity,
        aimPos,
        targetRadius,
        slowDownDist
    }
{
    // NOP
}

CTaskComplexGoToPointShooting::CTaskComplexGoToPointShooting(const CTaskComplexGoToPointShooting& o) :
    CTaskComplexGoToPointAiming{*this}
{
}

// 0x66CE10
CTask* CTaskComplexGoToPointShooting::Clone() const {
    return new CTaskComplexGoToPointShooting(m_moveState, m_movePos, m_aimAtEntity, m_aimPos, m_moveTargetRadius, m_slowDownDistance);
}
