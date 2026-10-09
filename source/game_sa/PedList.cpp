#include "StdInc.h"
#include "PedList.h"

#include "TaskComplexKillPedOnFoot.h"

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

    // Slots 0..6 only (the leader is slot 7 => LEADER_MEM_ID). The original
    // loops over GetMember(0..6) and skips nulls, so `GetMembers(false)` is equivalent.
    for (auto* const member : pedGroupMembership->GetMembers(false)) {
        if (!member->GetIntelligence()->IsInACarOrEnteringOne()) { // 0x601CC0
            if (m_count < 30u) { // 0x1E
                AddMember(member);
            }
        }
    }

    ClearUnused();
}

// 0x69A3B0
void CPedList::BuildListOfPedsOfPedType(int32 pedType) {
    const auto pool = GetPedPool();
    assert(pool);

    m_count = 0;

    // NOTE: The original iterates the ped pool (0xB74490) from the LAST slot down to 0,
    // so the list ends up ordered in reverse of a plain pool traversal.
    for (auto i = pool->GetSize(); i-- > 0u;) {
        if (const auto ped = pool->GetAt(i); ped && ped->m_nPedType == pedType) {
            if (m_count < 30u) { // 0x1E
                AddMember(ped);
            }
        }
    }

    ClearUnused();
}

// 0x69A450
void CPedList::RemovePedsAttackingPedType(int32 pedType) {
    const auto count = m_count;

    for (auto i = 0u; i < count; i++) {
        const auto ped = Get(i);

        // Original: CPedIntelligence::FindTaskByType(TASK_COMPLEX_KILL_PED_ON_FOOT) (0x600EE0)
        const auto killTask = ped->GetIntelligence()->FindTaskByType(TASK_COMPLEX_KILL_PED_ON_FOOT);
        const auto target   = killTask
            ? static_cast<CTaskComplexKillPedOnFoot*>(killTask)->m_target
            : nullptr;

        if (!target || target->m_nPedType != pedType) {
            RemoveMemberNoFill(i);
        }
    }

    FillUpHoles(); // 0x699E20
}

// 0x69A420
void CPedList::RemovePedsThatDontListenToPlayer() {
    const auto count = m_count;

    for (auto i = 0u; i < count; i++) {
        if (Get(i)->bDoesntListenToPlayerGroupCommands) { // ped + 0x474, bit 5 (mask 0x20)
            RemoveMemberNoFill(i);
        }
    }

    FillUpHoles(); // 0x699E20
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
