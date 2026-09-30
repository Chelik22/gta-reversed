#include "StdInc.h"

#include "PedGroupPlacer.h"

#include "PedGroups.h"
#include "PedGroupMembership.h"
#include "PedGroupIntelligence.h"
#include "Tasks/Allocators/PedGroup/PedGroupDefaultTaskAllocators.h"
#include "Tasks/TaskTypes/TaskComplexFollowLeaderInFormation.h"
#include "Tasks/TaskTypes/TaskComplexBeInGroup.h"
#include "Tasks/TaskTypes/TaskComplexWanderGang.h"
#include "Gangs.h"
#include "Population.h"
#include "PedPlacement.h"
#include "World.h"
#include "Camera.h"
#include "VisibilityPlugins.h"
#include "Models/ModelInfo.h"
#include "Entity/Ped/Ped.h"

void CPedGroupPlacer::InjectHooks() {
    RH_ScopedClass(CPedGroupPlacer);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(PlaceFormationGroup, 0x5FC9B0, {.reversed = false});
    RH_ScopedInstall(PlaceChatGroup, 0x5FCE80, {.reversed = false});
    RH_ScopedInstall(PlaceRandomGroup, 0x5FD330, {.reversed = false});
    RH_ScopedInstall(PlaceGroup, 0x5FD810);
}

// NOTE: In the original binary the 3 functions below are near identical; the decompiler merged
// them (along with `PlaceGroup`) into one big blob, so the decompiled code had to be split apart
// again. All constant values have been verified against the binary.

namespace {
    static const float HALF               = StaticRef<float>(0x858B8C); // 0.5
    static const float RAND_SCALE         = StaticRef<float>(0x858C7C); // 1 / 32768
    static const float JITTER_AMOUNT      = StaticRef<float>(0x858EE8); // 0.4
    static const float JITTER_OFFSET      = StaticRef<float>(0x858CC4); // 0.2
    static const float WANDER_RADIUS      = StaticRef<float>(0x86FCB0); // 0.5
    static const float MIN_DIST_TO_PLAYER = StaticRef<float>(0x86C850); // 42.5

    //! Random jitter in [-0.2, 0.2] (times `JITTER_AMOUNT`)
    static float GetRandomJitter() {
        return ((float)CGeneral::GetRandomNumber() * RAND_SCALE * JITTER_AMOUNT) - JITTER_OFFSET;
    }

    //! Is the origin possibly visible but too close to the player?
    static bool IsTooCloseToPlayer(const CVector& origin, float radius) {
        if (!TheCamera.IsSphereVisible(origin, radius)) { // 0x420D40
            return false;
        }
        const auto* player = FindPlayerPed();
        const auto dist    = DistanceBetweenPoints2D(CVector2D{origin}, CVector2D{player->GetPosition()});
        return dist < CPopulation::PedCreationDistMultiplier() * MIN_DIST_TO_PLAYER;
    }

    //! Pick a gang ped model for the ped type and make sure it's loaded
    static eModelID ChooseLoadedGangModel(ePedType type) {
        const auto model = CGangs::ChooseGangPedModel((eGangID)(type - PED_TYPE_GANG1)); // 0x5DE5A0
        if (!CModelInfo::ms_modelInfoPtrs[model]->GetRwObject()) {
            return MODEL_INVALID;
        }
        return model;
    }

    //! Create a gang member ped at `pos` and setup it's rotation (facing away from `origin`)
    static CPed* PlaceGangPed(ePedType type, eModelID model, const CVector& pos, const CVector& origin) {
        auto* ped = CPopulation::AddPed(type, model, pos, false); // 0x612710
        if (!ped) {
            CPopulation::RemovePed(nullptr); // 0x610F20 - NB: The original literally calls this with a null ped
            return nullptr;
        }
        const auto heading = CGeneral::GetRadianAngleBetweenPoints(origin.x, origin.y, pos.x, pos.y); // 0x53CBE0
        ped->m_fCurrentRotation = ped->m_fAimingRotation = heading;
        CVisibilityPlugins::SetClumpAlpha(ped->GetRpClump(), 0); // 0x732B00 - the ped fades in
        return ped;
    }

    //! Create the group (leader, followers + tasks) - `peds[0]` must be the leader
    static bool SetupGroup(int32 groupId, const CPedGroupDefaultTaskAllocator& allocator, std::span<CPed* const> peds) {
        auto& group = CPedGroups::GetGroup(groupId);
        group.m_groupIntelligence.SetDefaultTaskAllocator(allocator); // 0x5FB280

        group.m_groupMembership.SetLeader(peds[0]);   // 0x5FB9C0
        group.m_groupMembership.Process();            // 0x5FBA60
        group.m_groupIntelligence.Process();          // 0x5FC4A0
        peds[0]->GetTaskManager().SetTask(new CTaskComplexBeInGroup{groupId, true}, TASK_PRIMARY_PRIMARY);

        for (auto i = 1u; i < peds.size(); i++) {
            group.m_groupMembership.AddFollower(peds[i]); // 0x5F8020
            group.m_groupMembership.Process();
            group.m_groupIntelligence.Process();
            peds[i]->GetTaskManager().SetTask(new CTaskComplexBeInGroup{groupId, false}, TASK_PRIMARY_PRIMARY);
        }
        return true;
    }

    //! [0x5FCE80 / 0x5FD330] Shared part of `PlaceChatGroup` and `PlaceRandomGroup` - places `numOfPeds` members
    //! randomly (with jitter) within a circle of `circleRadius` around `origin`.
    //! Returns the successfully placed peds (empty on failure).
    static std::vector<CPed*> PlaceCircleGroupMembers(ePedType type, uint32 numOfPeds, const CVector& origin, float circleRadius) {
        const auto angleStep = TWO_PI / (float)numOfPeds;

        std::vector<CPed*> peds{};
        CVector firstPos{};
        for (auto i = 0u; i < numOfPeds; i++) {
            // Random jitter applied to the angle and radius
            const auto angle  = (float)i * angleStep + angleStep * GetRandomJitter();
            const auto radius = circleRadius + circleRadius * GetRandomJitter();

            CVector pos{
                origin.x + cosf(angle) * radius,
                origin.y + sinf(angle) * radius,
                0.0f
            };

            bool foundGroundZ{};
            auto z = CWorld::FindGroundZFor3DCoord(CVector{pos.x, pos.y, origin.z + 1.0f}, &foundGroundZ); // 0x5696C0
            if (!foundGroundZ) {
                continue;
            }
            z += 1.0f;
            if (z < origin.z) {
                z = origin.z;
            }
            pos.z = z;

            if (i == 0) {
                firstPos = pos;
            }

            const auto model = ChooseLoadedGangModel(type);
            if (model == (eModelID)-1) {
                continue;
            }

            // Gather nearby entities, make sure none of them is a ped we've already placed
            CEntity* hits[9]{};
            (void)CPedPlacement::IsPositionClearForPed(pos, CModelInfo::ms_modelInfoPtrs[model]->m_pColModel->GetBoundRadius(), 9, hits); // NB: Original ignores the result
            const bool isDuplicate = std::any_of(std::begin(hits), std::end(hits), [&](auto* ent) {
                return ent && notsa::contains(peds, ent);
            });
            if (isDuplicate) {
                continue;
            }

            // The ped must be able to "see" the first one (and be at the same height)
            bool canPlace{};
            if (i == 0) {
                canPlace = true;
            } else {
                canPlace = CWorld::GetIsLineOfSightClear(pos, firstPos, true, false, false, false, false, false, false) // 0x56A490
                        && fabs(pos.z - firstPos.z) < 1.0f;
            }
            if (!canPlace) {
                continue;
            }

            if (auto* ped = PlaceGangPed(type, model, pos, origin)) {
                peds.push_back(ped);
            }
        }
        return peds;
    }
} // namespace

// 0x5FC9B0
bool CPedGroupPlacer::PlaceFormationGroup(ePedType type, uint32 numOfPeds, const CVector& origin, ePedGroupDefaultTaskAllocatorType) {
    ZoneScoped;

    const auto groupId = CPedGroups::AddGroup(); // 0x5FB800
    if (groupId < 0) {
        return false;
    }

    // Skip if the origin is (possibly) visible but too close to the player
    if (IsTooCloseToPlayer(origin, 3.0f)) {
        return false;
    }

    if (!CPedPlacement::IsPositionClearForPed(origin, 3.0f)) { // 0x616860
        return false;
    }

    // Find the ground z position
    bool foundGroundZ{};
    const auto groundZ = CWorld::FindGroundZFor3DCoord(origin, &foundGroundZ); // 0x5696C0 (original adds 1.0f to the z coordinate)
    if (!foundGroundZ) {
        return false;
    }
    const auto creationZ = std::max(origin.z, groundZ + 1.0f);

    const auto model = ChooseLoadedGangModel(type);
    if (model == (eModelID)-1) {
        return false;
    }

    auto& formationOffsets = CTaskComplexFollowLeaderInFormation::ms_offsets.Offsets;

    std::vector<CPed*> peds{};

    // The leader is placed right at the origin
    auto* leader = CPopulation::AddPed(type, model, CVector{origin.x, origin.y, creationZ}, false);
    if (!leader) {
        return false;
    }
    peds.push_back(leader);

    for (auto i = 1u; i < numOfPeds; i++) {
        const auto memberModel = ChooseLoadedGangModel(type);
        if (memberModel == (eModelID)-1) {
            continue;
        }

        auto* ped = CPopulation::AddPed(type, memberModel, CVector{origin.x, origin.y, creationZ}, false);
        if (!ped) {
            continue;
        }

        // Move the ped to it's formation position (relative to the leader)
        const CVector offset{ formationOffsets[i].x, formationOffsets[i].y, 0.0f };
        ped->SetPosn(leader->GetPosition() + offset); // 0x4241C0 (original adds the vectors manually - 0x40FE30)

        const auto pedPos = ped->GetPosition();

        bool foundGroundZ2{};
        const auto gz = CWorld::FindGroundZFor3DCoord(pedPos, &foundGroundZ2); // original adds 1.0f to the z coordinate
        if (!foundGroundZ2) {
            CPopulation::RemovePed(ped);
            continue;
        }
        const auto z = std::max(pedPos.z, gz + 1.0f);

        // Must be roughly at the same height as the leader and visible to it
        const auto& leaderPos = leader->GetPosition();
        if (fabs(z - leaderPos.z) > 1.0f) {
            CPopulation::RemovePed(ped);
            continue;
        }
        if (!CWorld::GetIsLineOfSightClear(CVector{pedPos.x, pedPos.y, z}, leaderPos, true, false, false, false, false, false, false)) { // 0x56A490
            CPopulation::RemovePed(ped);
            continue;
        }

        ped->SetPosn(pedPos.x, pedPos.y, z); // 0x420B80
        CVisibilityPlugins::SetClumpAlpha(ped->GetRpClump(), 0);

        peds.push_back(ped);
    }

    if (peds.size() < 2) { // Only the leader got placed => fail
        for (auto* ped : peds) {
            CPopulation::RemovePed(ped);
        }
        return false;
    }

    return SetupGroup(groupId, CPedGroupDefaultTaskAllocators::Get(ePedGroupDefaultTaskAllocatorType::FOLLOW_ANY_MEANS), peds);
}
// 0x5FCE80
bool CPedGroupPlacer::PlaceChatGroup(ePedType type, uint32 numOfPeds, const CVector& origin, ePedGroupDefaultTaskAllocatorType) {
    ZoneScoped;

    if (numOfPeds < 2) {
        return false;
    }

    const auto groupId = CPedGroups::AddGroup();
    if (groupId < 0) {
        return false;
    }

    // Members form a circle around the origin
    const auto angleStep    = TWO_PI / (float)numOfPeds;
    const auto circleRadius = sqrtf(HALF / (1.0f - cosf(angleStep)));

    if (IsTooCloseToPlayer(origin, circleRadius)) {
        return false;
    }

    if (!CPedPlacement::IsPositionClearForPed(origin, circleRadius)) {
        return false;
    }

    const auto peds = PlaceCircleGroupMembers(type, numOfPeds, origin, circleRadius);
    if (peds.empty()) {
        return false;
    }

    return SetupGroup(groupId, CPedGroupDefaultTaskAllocators::Get(ePedGroupDefaultTaskAllocatorType::STAND_STILL), peds);
}
// 0x5FD330
bool CPedGroupPlacer::PlaceRandomGroup(ePedType type, uint32 numOfPeds, const CVector& origin, ePedGroupDefaultTaskAllocatorType) {
    ZoneScoped;

    if (numOfPeds < 2) {
        return false;
    }

    const auto groupId = CPedGroups::AddGroup();
    if (groupId < 0) {
        return false;
    }

    // Members are placed randomly within a circle around the origin
    const auto angleStep    = TWO_PI / (float)numOfPeds;
    const auto circleRadius = sqrtf(HALF / (1.0f - cosf(angleStep)));

    if (IsTooCloseToPlayer(origin, circleRadius)) {
        return false;
    }

    if (!CPedPlacement::IsPositionClearForPed(origin, circleRadius)) {
        return false;
    }

    const auto peds = PlaceCircleGroupMembers(type, numOfPeds, origin, circleRadius);
    if (peds.empty()) {
        return false;
    }

    auto& group = CPedGroups::GetGroup(groupId);
    group.m_groupIntelligence.SetDefaultTaskAllocator(CPedGroupDefaultTaskAllocators::Get(ePedGroupDefaultTaskAllocatorType::RANDOM)); // 0x5FB280

    for (auto i = 0u; i < peds.size(); i++) {
        auto* ped = peds[i];
        if (i == 0) {
            group.m_groupMembership.SetLeader(ped); // 0x5FB9C0
        } else {
            group.m_groupMembership.AddFollower(ped); // 0x5F8020
        }
        group.m_groupMembership.Process();
        group.m_groupIntelligence.Process();

        // Random ped => gets a wander task + the "be in group" task
        ped->GetTaskManager().SetTask(
            new CTaskComplexWanderGang{PEDMOVE_WALK, (uint8)(CGeneral::GetRandomNumber() & 0xFFFF), 5000, true, WANDER_RADIUS}, // 0x66F5C0
            TASK_PRIMARY_DEFAULT
        );
        ped->GetTaskManager().SetTask(
            new CTaskComplexBeInGroup{groupId, false}, // 0x632E50
            TASK_PRIMARY_PRIMARY
        );
    }
    return true;
}
// 0x5FD810
bool CPedGroupPlacer::PlaceGroup(ePedType type, uint32 numOfPeds, const CVector& origin, ePedGroupDefaultTaskAllocatorType allocType) {
    using enum ePedGroupDefaultTaskAllocatorType;
    switch (allocType) {
    case FOLLOW_ANY_MEANS:
    case FOLLOW_LIMITED:   return PlaceFormationGroup(type, numOfPeds, origin, allocType);
    case STAND_STILL:
    case CHAT:             return PlaceChatGroup(type, numOfPeds, origin, allocType);
    case RANDOM:           return PlaceRandomGroup(type, numOfPeds, origin, allocType);
    default:               NOTSA_UNREACHABLE();
    }
}