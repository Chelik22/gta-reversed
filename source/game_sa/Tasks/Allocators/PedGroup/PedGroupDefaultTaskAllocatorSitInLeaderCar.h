#pragma once

#include <Base.h>
#include <PluginBase.h>
#include <reversiblehooks/ReversibleHooks.h>
#include "CarEnterExit.h"

#include "./PedGroupDefaultTaskAllocator.h"
#include "TaskComplexEnterCarAsDriver.h"
#include "TaskComplexEnterCarAsPassenger.h"
#include "TaskSimpleCarDrive.h"

class NOTSA_EXPORT_VTABLE CPedGroupDefaultTaskAllocatorSitInLeaderCar final : public CPedGroupDefaultTaskAllocator {
public:
    /* no virtual destructor */

    // 0x5F6560
    ePedGroupDefaultTaskAllocatorType GetType() const override { return ePedGroupDefaultTaskAllocatorType::SIT_IN_LEADER_CAR; };

    // 0x5F6FC0
    // FIX по декомпилу 0x5F6FC0 (см. batch_out/fresh/0x5F6FC0.cpp):
    // 1) задача последователя писалась в ЛИДЕРСКУЮ пару (SetPedDefaultTask(leader, LEADER_MEM_ID, ...))
    //    - по декомпилу пары (group+0x21C, stride 0x14) и члены (group+0xC) шагают СИНХРОННО по слотам:
    //      задача слота i должна попадать в пару слота i того мембера.
    // 2) rngv::enumerate(GetFollowers()) даёт плотную нумерацию ненулевых мемберов, а декомпил
    //    индексирует пару СЛОТОМ мембера (слоты могут быть с дырками) - обход через GetMember(i).
    // 3) проверка числа мест в оригинале НЕ брейкает цикл, а пропускает слот (continue).
    void AllocateDefaultTasks(CPedGroup* pedGroup, CPed* ped) const override {
        const auto leader = pedGroup->GetMembership().GetLeader();
        if (!leader) {
            return;
        }
        const auto veh = leader->m_pVehicle;
        if (!veh) {
            return;
        }
        const auto SetPedDefaultTask = [&](CPed* p, int32 i, CTask* task) {
            auto* const tp = &pedGroup->GetIntelligence().GetDefaultPedTaskPairs()[i];
            if (tp->Ped && (!ped || tp->Ped == ped)) {
                VERIFY(std::exchange(tp->Task, task) == nullptr);
            } else {
                delete task;
            }
        };
        SetPedDefaultTask(leader, CPedGroupMembership::LEADER_MEM_ID, new CTaskComplexSequence{
            new CTaskComplexEnterCarAsDriver{veh}, // 0x5F703E
            new CTaskSimpleCarDrive{veh} // 0x5F7074
        });
        int32 seat{};
        for (auto i = 0; i < CPedGroupMembership::LEADER_MEM_ID; i++) { // слоты 0..6
            auto* const mem = pedGroup->GetMembership().GetMember(i);
            if (!mem || seat >= veh->m_nMaxPassengers) {
                continue; // как в оригинале: слот пропускается, цикл не прерывается
            }
            SetPedDefaultTask(mem, i, new CTaskComplexSequence{
                new CTaskComplexEnterCarAsPassenger{veh, CCarEnterExit::ComputeTargetDoorToEnterAsPassenger(veh, seat++)}, // 0x5F714B
                new CTaskSimpleCarDrive{veh} // 0x5F7184
            });
        }
    };

public:
    static inline void InjectHooks() {
        RH_ScopedVirtualClass(CPedGroupDefaultTaskAllocatorSitInLeaderCar, 0x86C784, 2);
        RH_ScopedCategory("Tasks/Allocators/PedGroup");

        RH_ScopedVMTInstall(AllocateDefaultTasks, 0x5F6FC0);
        RH_ScopedVMTInstall(GetType, 0x5F6560);
    }
};
VALIDATE_SIZE(CPedGroupDefaultTaskAllocatorSitInLeaderCar, sizeof(void*)); /* vtable only */
