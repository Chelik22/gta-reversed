// ============================================================================
// INTEGRATED FILE — source/game_sa/Tasks/TaskTypes/TaskComplexGoToPointShooting.h
// Изменение ТОЛЬКО одно (VERIFY-wf5a, заметка интегратору по 0x66CE10):
//   inline-тело Clone() убрано из хедера и перенесено в .cpp.
//   Прежнее `return new CTaskComplexGoToPointAiming{ *this };` давало клону vtable
//   базового CTaskComplexGoToPointAiming (0x86FE18), тогда как оригинал 0x66CE10
//   строит именно CTaskComplexGoToPointShooting (vtable 0x86FE2c после базового
//   ctor 0x668790). GetTaskType остаётся inline (тело `return Type;` корректно,
//   прецедент — CTaskComplexGoToPointAiming с хуком 0x668860 по inline-методу).
// Поля/остальные методы не тронуты — раскладка совпадает с оригиналом.
// ============================================================================

#pragma once

#include "TaskComplexGoToPointAiming.h"

class CEntity;

class NOTSA_EXPORT_VTABLE CTaskComplexGoToPointShooting : public CTaskComplexGoToPointAiming {
public:
    static constexpr auto Type = TASK_COMPLEX_GO_TO_POINT_SHOOTING;

    CTaskComplexGoToPointShooting(
        eMoveState moveState,
        const CVector& movePos,
        CEntity* aimAtEntity,
        CVector aimPos,
        const float targetRadius,
        const float slowDownDist
    );
    CTaskComplexGoToPointShooting(const CTaskComplexGoToPointShooting&);
    ~CTaskComplexGoToPointShooting() override = default;

    eTaskType GetTaskType() const override { return Type; } // 0x668CC0
    CTask*    Clone() const override;                       // 0x66CE10 — тело в .cpp

private:
    friend void InjectHooksMain();
    static void InjectHooks();

    // 0x668C70
    CTaskComplexGoToPointShooting* Constructor(eMoveState moveState, CVector const& movePos, CEntity* aimAtEntity, CVector aimPos, float targetRadius, float slowDownDist) {
        this->CTaskComplexGoToPointShooting::CTaskComplexGoToPointShooting(moveState, movePos, aimAtEntity, aimPos, targetRadius, slowDownDist);
        return this;
    }
};
