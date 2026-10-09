#pragma once

#include "TaskComplex.h"
#include "Vector.h"
class CVehicle;

class NOTSA_EXPORT_VTABLE CTaskGoToVehicleAndLean : public CTaskComplex {
public:
    CVehicle* m_Vehicle;
    int32     m_LeanAnimDurationInMs;
    bool      m_LeanOnVehicle;
    bool      field_15; // CalcTargetPos: сторона захода
    uint8     field_16[2];
    CVector   m_vecTargetPos; // 0x18 — результат CalcTargetPos (0x664770); оригинальный класс 0x24 байта
                              // (operator new(0x24) в Clone 0x6621B0; пишется в CreateFirstSubTask 0x664D40,
                              //  читается в ControlSubTask 0x664E60)

public:
    static constexpr auto Type = TASK_COMPLEX_GOTO_VEHICLE_AND_LEAN;

    CTaskGoToVehicleAndLean(CVehicle* vehicle, int32 leanAnimDurationInMs);
    ~CTaskGoToVehicleAndLean() override;

    eTaskType GetTaskType() const override { return Type; } // 0x660ED0
    CTask* Clone() const override; // 0x6621B0 (defined in cpp)
    bool MakeAbortable(CPed* ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent* event = nullptr) override;
    CTask* CreateNextSubTask(CPed* ped) override;
    CTask* CreateFirstSubTask(CPed* ped) override;
    CTask* ControlSubTask(CPed* ped) override;

    void DoTidyUp(CPed* ped);
    // CalcTargetPos(CPed* ped) 0x664770

private:
    friend void InjectHooksMain();
    static void InjectHooks();
    CTaskGoToVehicleAndLean* Constructor(CVehicle* vehicle, int32 leanAnimDurationInMs) { this->CTaskGoToVehicleAndLean::CTaskGoToVehicleAndLean(vehicle, leanAnimDurationInMs); return this; }
    CTaskGoToVehicleAndLean* Destructor() { this->CTaskGoToVehicleAndLean::~CTaskGoToVehicleAndLean(); return this; }
};
VALIDATE_SIZE(CTaskGoToVehicleAndLean, 0x24);
