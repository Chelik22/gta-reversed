#include "StdInc.h"

#include "TaskComplexDragPedFromCar.h"
#include "CarEnterExit.h"     // CCarEnterExit::ComputeTargetDoorToExit (0x64F110)
#include "tHandlingData.h"    // m_nModelFlags @+0xCC (VEHICLE_HANDLING_MODEL_TANDEM_SEATS)

void CTaskComplexDragPedFromCar__InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexDragPedFromCar, 0x86EB6C, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedVMTInstall(ControlSubTask, 0x640530);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x643D00);
}

// 0x640430
CTaskComplexDragPedFromCar::CTaskComplexDragPedFromCar(CPed* ped, int32 draggedPedDownTime) : CTaskComplexEnterCar(nullptr, false, false, true, false) {
    m_Ped = ped;
    CEntity::SafeRegisterRef(m_Ped);
    m_DraggedPedDownTime = draggedPedDownTime;
}

// 0x6404D0
CTaskComplexDragPedFromCar::~CTaskComplexDragPedFromCar() {
    CEntity::SafeCleanUpRef(m_Ped);
}

// 0x640530


CTask* CTaskComplexDragPedFromCar::ControlSubTask(CPed* ped) {
    if (m_NumGettingInSet)
        return CTaskComplexEnterCar::ControlSubTask(ped);

    if (!m_Ped || m_Ped->bInVehicle || !m_pSubTask->MakeAbortable(ped))
        return CTaskComplexEnterCar::ControlSubTask(ped);

    return CTaskComplexEnterCar::CreateSubTask(TASK_NONE, nullptr);
}

// 0x643D00
CTask* CTaskComplexDragPedFromCar::CreateFirstSubTask(CPed* ped) {
    if (const auto draggedPed = m_Ped; draggedPed && draggedPed->m_pVehicle && draggedPed->bInVehicle) { // m_Ped @0x50 (derived); bInVehicle = +0x46C bit 8
        const auto veh = draggedPed->m_pVehicle;
        if (veh->IsPassenger(draggedPed) /*0x6D1BD0*/ || veh->IsDriver(draggedPed) /*0x6D1C40*/) {
            if (m_Car) {
                m_Car->CleanUpOldReference(reinterpret_cast<CEntity**>(&m_Car)); // 0x571A00
            }
            m_Car = draggedPed->m_pVehicle;
            m_Car->RegisterReference(reinterpret_cast<CEntity**>(&m_Car));       // 0x571B70

            m_bAsDriver                = m_Car->m_pDriver == draggedPed; // m_Car+0x460
            m_bQuitAfterOpeningDoor    = false; // & 0xFD
            m_bQuitAfterDraggingPedOut = true;  // | 0x04

            if (m_Car) { // не байк/квад (без TANDEM_SEATS): вычисляем дверь, из которой тащим
                if (   !(m_Car->m_pHandlingData->m_nModelFlags & VEHICLE_HANDLING_MODEL_TANDEM_SEATS) // *(veh+0x384)+0xCC & 0x200
                    && draggedPed->m_pVehicle->m_nVehicleType   != VEHICLE_TYPE_BIKE                   // +0x590 != 9
                    && draggedPed->m_pVehicle->m_nVehicleSubType != VEHICLE_TYPE_QUAD                  // +0x594 != 2
                ) {
                    m_TargetSeat = CCarEnterExit::ComputeTargetDoorToExit(m_Car, draggedPed); // 0x64F110
                    return CTaskComplexEnterCar::CreateFirstSubTask(ped); // 0x643A60
                }
                m_TargetSeat = 0;
            }
            return CTaskComplexEnterCar::CreateFirstSubTask(ped); // 0x643A60
        }
    }
    // Пед не в машине / не пассажир-водитель - фолбэк в родительский CreateSubTask(TASK_NONE=200)
    return CTaskComplexEnterCar::CreateSubTask(TASK_NONE, ped); // 0x63E040
}
