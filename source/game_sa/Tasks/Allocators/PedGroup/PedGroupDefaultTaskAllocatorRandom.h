#pragma once

#include <Base.h>
#include <PluginBase.h>
#include <reversiblehooks/ReversibleHooks.h>
#include <PedGroup.h>

#include "./PedGroupDefaultTaskAllocator.h"
#include "Tasks/TaskTypes/TaskComplexGangFollower.h"
#include "Tasks/TaskTypes/TaskComplexGangLeader.h"
#include "Tasks/TaskTypes/TaskComplexFollowLeaderInFormation.h"

class NOTSA_EXPORT_VTABLE CPedGroupDefaultTaskAllocatorRandom final : public CPedGroupDefaultTaskAllocator {
public:
    /* no virtual destructor */

    // 0x5F6E90
    // ABI: const-thiscall, ret 8 => this + 2 stack args. Per the disasm of 0x5F6E90:
    //   edi = pedGroup (arg1), ebx = ped (arg2), this (ecx) is not used.
    // Ghidra loses `this` in the decompile => FUN_005f6e90(param_1=pedGroup, param_2=ped).
    void AllocateDefaultTasks(CPedGroup* pedGroup, CPed* ped) const override {
        auto&       pairs   = pedGroup->GetIntelligence().GetDefaultPedTaskPairs(); // original: pedGroup+0x21C
        const auto& offsets = CTaskComplexFollowLeaderInFormation::ms_offsets.Offsets; // 0xC196E8 (bss, filled at runtime)

        // Followers: pairs [0..6] (7 iterations)
        for (auto i = 0; i < TOTAL_PED_GROUP_FOLLOWERS; i++) {
            if (const auto member = pairs[i].Ped; member && (!ped || member == ped)) {
                CTaskComplexGangFollower* task{}; // m_Flags lives in CTaskComplexGangFollower (+0x3D), the base CTask has no such field
                if (const auto t = new CTaskComplexGangFollower{
                        pedGroup,
                        pedGroup->GetMembership().GetLeader(), // original: *(pedGroup+0x28)
                        (uint8)i,
                        CVector{ offsets[i].x, offsets[i].y, 0.f }, // 3rd coordinate = 0
                        10.0f                                       // 0x41200000
                    }) {
                    task = t;
                }
                pairs[i].Task = task;
                if (task) {
                    // 0x65EE90 (inlined setter): m_Flags bit2 := m_bMembersEnterLeadersVehicle.
                    // The original calls it even when task == nullptr (would crash there; operator new
                    // doesn't return null in practice) - guarded here, documented deviation.
                    task->m_Flags = (task->m_Flags & ~0x04) | (pedGroup->m_bMembersEnterLeadersVehicle << 2);
                }
            }
        }

        // Leader: pair [7] (pedGroup+0x2A8 / .Task = +0x2AC)
        if (const auto leader = pairs[CPedGroupMembership::LEADER_MEM_ID].Ped; leader && (!ped || leader == ped)) {
            CTask* task{};
            if (const auto t = new CTaskComplexGangLeader{ pedGroup }) {
                task = t;
            }
            pairs[CPedGroupMembership::LEADER_MEM_ID].Task = task;
        }
    };
    ePedGroupDefaultTaskAllocatorType GetType() const override { return ePedGroupDefaultTaskAllocatorType::RANDOM; }; // 0x5F6530

public:
    static inline void InjectHooks() {
        RH_ScopedVirtualClass(CPedGroupDefaultTaskAllocatorRandom, 0x86C77C, 2);
        RH_ScopedCategory("Tasks/Allocators/PedGroup");

        RH_ScopedVMTInstall(AllocateDefaultTasks, 0x5F6E90);
        RH_ScopedVMTInstall(GetType, 0x5F6530);
    }
};
VALIDATE_SIZE(CPedGroupDefaultTaskAllocatorRandom, sizeof(void*)); /* vtable only */
