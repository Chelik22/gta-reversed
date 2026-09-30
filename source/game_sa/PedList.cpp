#include "StdInc.h"
#include "PedList.h"

#include "PedGroupMembership.h"
#include "PedIntelligence.h"
#include "Pools/Pools.h"
#include "Tasks/TaskTypes/TaskComplexKillPedOnFoot.h"

void CPedList::InjectHooks() {
    RH_ScopedClass(CPedList);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Empty, 0x699DB0);
    RH_ScopedInstall(BuildListFromGroup_NoLeader, 0x699DD0);
    RH_ScopedInstall(ExtractPedsWithGuns, 0x69A4C0);
    RH_ScopedInstall(BuildListFromGroup_NotInCar_NoLeader, 0x69A340);
    RH_ScopedInstall(BuildListOfPedsOfPedType, 0x69A3B0);
    RH_ScopedInstall(RemovePedsAttackingPedType, 0x69A450);
    RH_ScopedInstall(RemovePedsThatDontListenToPlayer, 0x69A420);
}

// 0x699DB0
void CPedList::Empty() {
    *this = {};
}

// 0x699DD0
void CPedList::BuildListFromGroup_NoLeader(CPedGroupMembership& groupMembership) {
    m_count = 0;
    for (auto* const mem : groupMembership.GetMembers(false)) {
        AddMember(mem);
    }
    ClearUnused();
}

// 0x69A4C0
void CPedList::ExtractPedsWithGuns(CPedList& from) {
    for (auto i = 0u; i < from.m_count; i++) {
        if (!from.Get(i)->GetActiveWeapon().IsTypeMelee()) {
            AddMember(from.Get(i));
            from.RemoveMemberNoFill(i);
        }
    }
    from.FillUpHoles();
}


// After nulling out a field in the
// array there might be a hole, so it has to be filled
void CPedList::FillUpHoles() {
    rng::fill(rng::remove(m_peds, nullptr), nullptr);
}

// 0x69A340
void CPedList::BuildListFromGroup_NotInCar_NoLeader(CPedGroupMembership* pedGroupMembership) {
    m_count = 0;
    for (auto i = 0; i < 7; i++) { // 7 members max (leader + 6 followers)
        if (const auto member = pedGroupMembership->GetMember(i)) {
            if (!member->GetIntelligence()->IsInACarOrEnteringOne() && m_count < m_peds.size()) {
                m_peds[m_count++] = member;
            }
        }
    }
    ClearUnused();
}

// 0x69A3B0
void CPedList::BuildListOfPedsOfPedType(int32 pedType) {
    m_count = 0;

    const auto pool = GetPedPool();
    for (auto i = pool->GetSize() - 1; i >= 0; i--) {
        if (const auto ped = pool->GetAt(i)) {
            if (ped->m_nPedType == pedType && m_count < m_peds.size()) {
                m_peds[m_count++] = ped;
            }
        }
    }
    ClearUnused();
}

// 0x69A450
void CPedList::RemovePedsAttackingPedType(int32 pedType) {
    for (auto i = 0u; i < m_count; i++) {
        const auto task = m_peds[i]->GetIntelligence()->FindTaskByType(TASK_COMPLEX_KILL_PED_ON_FOOT);
        const auto target = task
            ? notsa::cast<CTaskComplexKillPedOnFoot>(task)->m_target
            : nullptr;

        // if we are not attacking a ped of the given type - remove from the list
        if (!target || target->m_nPedType != pedType) {
            RemoveMemberNoFill(i);
        }
    }
    FillUpHoles();
}

// 0x69A420
void CPedList::RemovePedsThatDontListenToPlayer() {
    for (auto i = 0u; i < m_count; i++) {
        if (m_peds[i]->bDoesntListenToPlayerGroupCommands) {
            RemoveMemberNoFill(i);
        }
    }
}

//
// NOTSA section
//

// nulls out everything after the first `m_count` elements
void CPedList::ClearUnused() {
    rng::fill(m_peds | std::views::drop(m_count), nullptr);
}

void CPedList::AddMember(CPed* ped) {
    m_peds[m_count++] = ped;
}

// Must call FillUpHoles afterwards!
void CPedList::RemoveMemberNoFill(int32 i) {
    m_peds[i] = nullptr;
    m_count--;
}

CPed* CPedList::Get(int32 i) {
    return m_peds[i];
}
