#include "StdInc.h"

#include "TaskComplexGangFollower.h"

#include "TaskComplexLeaveCar.h"
#include "TaskSimpleCarDrive.h"
#include "TaskSimpleStandStill.h"
#include "SeekEntity/TaskComplexSeekEntity.h"
#include "SeekEntity/PosCalculators/EntitySeekPosCalculatorXYOffset.h"

void CTaskComplexGangFollower::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexGangFollower, 0x86F938, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x65EAA0);
    RH_ScopedInstall(Destructor, 0x65EBB0);
    //RH_ScopedInstall(CalculateOffsetPosition, 0x65ED40, { .reversed = false }); // not hooked because i want to keep CVector return, but original function took a CVector&
    RH_ScopedInstall(Clone, 0x65ECB0);
    RH_ScopedInstall(MakeAbortable, 0x65EC30);
    RH_ScopedInstall(CreateNextSubTask, 0x665E00, { .reversed = false });
    RH_ScopedInstall(CreateFirstSubTask, 0x666160);
    RH_ScopedInstall(ControlSubTask, 0x662A10, { .reversed = false });
}

// 0x65EAA0
CTaskComplexGangFollower::CTaskComplexGangFollower(CPedGroup* pedGroup, CPed* ped, uint8 a4, CVector pos, float a6) : CTaskComplex() {
    dword40 = 0;
    dword44 = 0;
    byte48 = 0;
    byte49 = 0;
    m_PedGroup = pedGroup;
    byte3C = a4;
    m_PedPosn = pos;
    dword20 = pos;
    dword38 = a6;
    m_Leader = ped;
    m_Flags = (m_Flags | 0x14) & ~0x03;
    if (m_Leader) {
        CEntity::SafeRegisterRef(m_Leader);
        m_PedPosn = m_Leader->GetPosition();
    }
    m_Flags &= ~0x03;
    if (m_Leader == FindPlayerPed()) {
        m_Flags |= 0x08;
    } else {
        m_Flags &= ~0x08;
    }
}

// 0x65EBB0
CTaskComplexGangFollower::~CTaskComplexGangFollower() {
    if (m_Leader) {
        CEntity::SafeCleanUpRef(m_Leader);
    }
    if (m_Flags & 1) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
        m_Flags &= ~1;
    }
}

// 0x65ED40
CVector CTaskComplexGangFollower::CalculateOffsetPosition() {
    CVector ret;
    plugin::CallMethod<0x65ED40, CTaskComplexGangFollower*>(this, &ret);
    return ret;
}

// 0x65ECB0
CTask* CTaskComplexGangFollower::Clone() const {
    if (const auto clone = new CTaskComplexGangFollower(m_PedGroup, m_Leader, byte3C, dword20, dword38)) {
        // Copy the "do stuff" flag (bit 2)
        clone->m_Flags = (uint8)((clone->m_Flags & ~4) | (m_Flags & 4));
        return clone;
    }
    return nullptr;
}

// 0x65EC30
bool CTaskComplexGangFollower::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (!m_pSubTask) {
        ped->bMoveAnimSpeedHasBeenSetByTask = false;
        ped->bCanExitCar = false;
        return true;
    }
    if (m_pSubTask->MakeAbortable(ped, priority, event)) {
        ped->bCanExitCar = false;
        ped->bMoveAnimSpeedHasBeenSetByTask = false;
        return true;
    }
    return false;
}

// 0x665E00
CTask* CTaskComplexGangFollower::CreateNextSubTask(CPed* ped) {
    return plugin::CallMethodAndReturn<CTask*, 0x665E00, CTaskComplexGangFollower*, CPed*>(this, ped);
}

// 0x666160
CTask* CTaskComplexGangFollower::CreateFirstSubTask(CPed* ped) {
    if (!m_Leader) {
        ped->bMoveAnimSpeedHasBeenSetByTask = false;
        return nullptr;
    }

    if (!ped->bInVehicle || !ped->m_pVehicle || m_Leader->m_pVehicle != ped->m_pVehicle) {
        if (!ped->bInVehicle || !ped->m_pVehicle) {
            if ((m_Flags & 4) && ped->GetIntelligence()->m_AnotherStaticCounter <= StaticRef<int32>(0x86C938)) {
                // Follow the leader
                const auto task = new CTaskComplexSeekEntity<CEntitySeekPosCalculatorXYOffset>( // 0x661DC0
                    m_Leader,
                    50'000,
                    1'000,
                    StaticRef<float>(0x86FC2C) * 0.5f,
                    StaticRef<float>(0x86F830),
                    StaticRef<float>(0x86FC30),
                    false,
                    true
                );
                task->GetSeekPosCalculator().SetOffset(dword20);
                // NOTE: The original also sets `*(int32*)(task + 0x50) = 7` and `*(uint8*)(task + 0x54) |= 8`,
                //       fields that have no counterpart in the NOTSA task layout.
                return task;
            }
            // Just stand still
            return new CTaskSimpleStandStill(500, false, false, 8.0f); // 0x62F310
        }
        // Enter the same vehicle as the leader
        return new CTaskComplexLeaveCar(ped->m_pVehicle, 0, 0, true, false); // 0x63B8C0
    }

    // Both peds are in the same vehicle
    return new CTaskSimpleCarDrive(ped->m_pVehicle, nullptr, false); // 0x63C340
}

// 0x662A10
CTask* CTaskComplexGangFollower::ControlSubTask(CPed* ped) {
    return plugin::CallMethodAndReturn<CTask*, 0x662A10, CTaskComplexGangFollower*, CPed*>(this, ped);
}
