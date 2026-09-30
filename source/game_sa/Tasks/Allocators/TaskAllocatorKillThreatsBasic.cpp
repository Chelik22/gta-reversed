#include "StdInc.h"

#include "TaskAllocatorKillThreatsBasic.h"
#include <InterestingEvents.h>
#include <TaskComplexKillPedGroupOnFoot.h>
#include <TaskComplexSequence.h>
#include <TaskSimpleLookAbout.h>

// 0x69C710
CTaskAllocatorKillThreatsBasic::CTaskAllocatorKillThreatsBasic(CPed* threat) :
    m_Threat{threat}
{
}

// 0x69D170
void CTaskAllocatorKillThreatsBasic::AllocateTasks(CPedGroupIntelligence* intel) {
    intel->FlushTasks(intel->GetPedTaskPairs(), nullptr);
    intel->FlushTasks(intel->GetSecondaryPedTaskPairs(), nullptr);

    if (!m_Threat) {
        return;
    }

    auto* const group = &intel->GetPedGroup();
    if (auto* const threatsGroup = m_Threat->GetGroup()) {
        if (threatsGroup == group) {
            NOTSA_LOG_DEBUG("ComputeKillThreatsBasicResponse() - threat ped already in group"); // vanilla
        } else {
            CPed* closest[TOTAL_PED_GROUP_MEMBERS]{};
            ComputeClosestPeds(*group, *threatsGroup, closest);
            for (int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++) {
                auto* mem = group->GetMembership().GetMember(i);
                if (!mem || mem->IsPlayer()) {
                    continue;
                }
                intel->SetEventResponseTask(
                    mem,
                    CTaskComplexKillPedGroupOnFoot{ CPedGroups::GetGroupId(threatsGroup), closest[i] }
                );
            }
            g_InterestingEvents.Add(CInterestingEvents::GANG_FIGHT, group->GetMembership().GetLeader()); // 0x69D436
        }
    } else { // 0x69D2EB
        for (auto* const mem : group->GetMembership().GetMembers()) {
            if (mem->IsPlayer()) {
                continue;
            }
            intel->SetEventResponseTask(
                mem,
                CTaskComplexSequence{
                    new CTaskComplexKillPedOnFoot{ m_Threat },
                    new CTaskSimpleLookAbout{ CGeneral::GetRandomNumberInRange(1'000u, 2'000u) },
                }
            );
        }
        g_InterestingEvents.Add(CInterestingEvents::GANG_ATTACKING_PED, group->GetMembership().GetLeader()); // 0x69D436
    }
}

// 0x69C7E0
CTaskAllocator* CTaskAllocatorKillThreatsBasic::ProcessGroup(CPedGroupIntelligence* intel) {
    m_Timer.StartIfNotAlready(0);
    if (m_Timer.IsOutOfTime()) {
        m_Timer.Start(5'000);
        AllocateTasks(intel);
    }
    return this;
}

// 0x69C850
void CTaskAllocatorKillThreatsBasic::ComputeClosestPeds(CPedGroup& group1, CPedGroup& group2, CPed** peds) {
    std::fill_n(peds, TOTAL_PED_GROUP_MEMBERS, nullptr);

    float dist[TOTAL_PED_GROUP_MEMBERS][TOTAL_PED_GROUP_MEMBERS];
    for (auto& row : dist) {
        rng::fill(row, FLT_MAX);
    }

    // Build the pairwise squared-distance matrix of alive, non-player members
    for (auto a = 0; a < TOTAL_PED_GROUP_MEMBERS; a++) {
        const auto* pedA = group1.GetMembership().GetMember(a);
        if (!pedA || !pedA->IsAlive() || pedA->IsPlayer()) {
            continue;
        }
        for (auto b = 0; b < TOTAL_PED_GROUP_MEMBERS; b++) {
            const auto* pedB = group2.GetMembership().GetMember(b);
            if (!pedB || !pedB->IsAlive()) {
                continue;
            }
            dist[a][b] = DistanceBetweenPointsSquared(pedA->GetPosition(), pedB->GetPosition());
        }
    }

    // Greedy minimum matching (8 rounds)
    for (auto round = 0; round < TOTAL_PED_GROUP_MEMBERS; round++) {
        auto bestA  = -1;
        auto bestB  = -1;
        auto best   = 5000.0f; // Max matching distance (0x863A3C)
        for (auto a = 0; a < TOTAL_PED_GROUP_MEMBERS; a++) {
            for (auto b = 0; b < TOTAL_PED_GROUP_MEMBERS; b++) {
                if (dist[a][b] < best) {
                    best  = dist[a][b];
                    bestA = a;
                    bestB = b;
                }
            }
        }
        if (bestA != -1 && bestB != -1) {
            rng::fill(dist[bestA], FLT_MAX);
            for (auto& row : dist) {
                row[bestB] = FLT_MAX;
            }
            peds[bestA] = group2.GetMembership().GetMember(bestB);
        }
    }

    // Members without a match: assign the leader of group2
    if (auto* leader = group2.GetMembership().GetLeader()) {
        if (leader->IsAlive()) {
            for (auto i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++) {
                if (group1.GetMembership().GetMember(i) && !peds[i]) {
                    peds[i] = leader;
                }
            }
            return;
        }
    }

    // Fallback: the first alive member of group2 covers all
    for (auto first = 0; first < TOTAL_PED_GROUP_MEMBERS; first++) {
        if (auto* member = group2.GetMembership().GetMember(first)) {
            if (member->IsAlive()) {
                for (auto i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++) {
                    const auto* m = group1.GetMembership().GetMember(i);
                    if (m && m->IsAlive() && !peds[i]) {
                        peds[i] = member;
                    }
                }
                return;
            }
        }
    }
}

void CTaskAllocatorKillThreatsBasic::InjectHooks() {
    RH_ScopedVirtualClass(CTaskAllocatorKillThreatsBasic, 0x870e90, 6);
    RH_ScopedCategory("Tasks/Allocators");

    RH_ScopedInstall(Constructor, 0x69C710);
    RH_ScopedInstall(Destructor, 0x69C780);

    RH_ScopedGlobalInstall(ComputeClosestPeds, 0x69C850);
    RH_ScopedVMTInstall(GetType, 0x69C770);
    RH_ScopedVMTInstall(AllocateTasks, 0x69D170);
    RH_ScopedVMTInstall(ProcessGroup, 0x69C7E0);
}
