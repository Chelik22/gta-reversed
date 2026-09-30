#include "StdInc.h"

#include "InteriorGroup_c.h"

#include "Interior_c.h"
#include "InteriorManager_c.h"

#include <TaskTypes/Interior/TaskInteriorBeInHouse.h>
#include <TaskTypes/Interior/TaskInteriorBeInOffice.h>
#include <TaskTypes/Interior/TaskInteriorBeInShop.h>
#include <TaskTypes/Interior/TaskInteriorShopKeeper.h>

//! Door info entry (Interior_c+0x512, stride 0x20, one per door side of m_box):
//!   block 0 (m_box->m_door)      @ 0x512
//!   block 1 (m_box->m_lDoorStart)@ 0x532
//!   block 2 (m_box->m_tDoorStart)@ 0x552
//!   block 3 (m_box->m_rDoorStart)@ 0x572
//!   +0x00 int8    neighborInteriorDoorIdx0
//!   +0x01 int8    neighborInteriorDoorIdx1
//!   +0x02 CVector posIn           (door position)
//!   +0x12 CVector posOut          (opposite position)
#pragma pack(push, 1)
struct DoorInfo_t { // Interior_c+0x512, stride 0x20
    int8    m_neighbor0; // 0x00 - adjacent interior's door idx (or -1)
    int8    m_neighbor1; // 0x01 - adjacent interior's door idx (or -1)
    CVector m_posIn;     // 0x02
    uint32  _pad0;       // 0x0E
    CVector m_posOut;    // 0x12
    uint16  _pad1;       // 0x1E
};
static_assert(sizeof(DoorInfo_t) == 0x20);
#pragma pack(pop)

//! Goto point entry (Interior_c+0x410, stride 0x10, count: Interior_c::field_40C)
struct GrpGotoPt_t {
    int8    m_tileX;
    int8    m_tileY;
    uint16  _pad;
    CVector m_pos;
};
static_assert(sizeof(GrpGotoPt_t) == 0x10);

// Data constants (kept as addresses):
//   0x858C48 : float, path node int16 coordinate -> world scale
//   0x85EED4 : float, max dist^2 for linking interior node to exterior node
//   0x858FEC : float, initial "best distance" (large) for exit pairing
//   0x858B3C : float, max dist^2 at which two exits still get paired
static auto& NODE_POS_SCALE      = StaticRef<float>(0x858C48);
static auto& MAX_LINK_DIST_SQ    = StaticRef<float>(0x85EED4);
static auto& EXIT_PAIR_INIT_DIST = StaticRef<float>(0x858FEC);
static auto& EXIT_PAIR_MAX_DIST  = StaticRef<float>(0x858B3C);

// 0xBB3D9C - some global flag array indexed by ped slot (TODO: Figure out type/name)
static auto& g_pedAllowedFlags = StaticRef<std::array<uint8, 16>>(0xBB3D9C);
// DAT_00BB3DC2 - interiors peds enabled flag
static auto& g_interiorPedsEnabled = StaticRef<int8>(0xBB3DC2);

void InteriorGroup_c::InjectHooks() {
    RH_ScopedClass(InteriorGroup_c);
    RH_ScopedCategory("Interior");

    //RH_ScopedInstall(Constructor, 0x597FE0, { .reversed = false });
    //RH_ScopedInstall(Destructor, 0x597FF0, { .reversed = false });

    RH_ScopedInstall(Init, 0x5947E0);
    RH_ScopedInstall(Update, 0x5968E0);
    RH_ScopedInstall(SetupPeds, 0x596890);
    RH_ScopedInstall(UpdatePeds, 0x596830);
    RH_ScopedInstall(SetupHousePeds, 0x5965E0);
    RH_ScopedInstall(SetupPaths, 0x595590);
    RH_ScopedInstall(ArePathsLoaded, 0x595380);
    RH_ScopedInstall(Setup, 0x595320);
    RH_ScopedInstall(Exit, 0x595290);
    RH_ScopedInstall(ContainsInteriorType, 0x595250);
    RH_ScopedInstall(CalcIsVisible, 0x595200);
    RH_ScopedInstall(DereferenceAnims, 0x595160);
    RH_ScopedInstall(ReferenceAnims, 0x5950D0);
    RH_ScopedInstall(UpdateOfficePeds, 0x594E90);
    RH_ScopedInstall(RemovePed, 0x594E30);
    RH_ScopedInstall(SetupShopPeds, 0x594C10);
    RH_ScopedInstall(SetupOfficePeds, 0x594BF0);
    RH_ScopedInstall(GetEntity, 0x594BD0);
    RH_ScopedInstall(GetPed, 0x594B90);
    RH_ScopedInstall(FindClosestInteriorInfo, 0x594A50);
    RH_ScopedInstall(FindInteriorInfo, 0x594970);
    RH_ScopedInstall(GetNumInteriorInfos, 0x594920);
    RH_ScopedInstall(GetRandomInterior, 0x5948C0);
    RH_ScopedInstall(AddInterior, 0x594840);
}

// 0x5947E0
void InteriorGroup_c::Init(CEntity* entity, int32 id) {
    for (auto& interior : m_interiors) {
        interior = nullptr;
    }
    for (auto& ped : m_peds) {
        ped = nullptr;
    }
    for (auto& ped : m_pedsToRemove) {
        ped = nullptr;
    }
    m_numInteriors        = 0;
    m_pathSetupComplete   = 0;
    m_updatePeds          = 0;
    m_isVisible           = false;
    m_lastIsVisible       = false;
    m_animBlockReferenced = 0;
    m_pEntity = entity;
    m_groupId = (uint8)id;
}

// 0x5968E0
void InteriorGroup_c::Update() {
    CalcIsVisible();

    if (!m_pathSetupComplete) {
        SetupPaths();
    }

    if (m_pathSetupComplete) {
        if (!m_updatePeds) {
            SetupPeds();
        }
    } else if (!m_updatePeds) {
        ReferenceAnims();
        return;
    }

    UpdatePeds();
    ReferenceAnims();
}

// 0x594840
int32 InteriorGroup_c::AddInterior(Interior_c* interior) {
    for (auto i = 0; i < 8; i++) {
        if (!m_interiors[i]) {
            m_interiors[i] = interior;
            m_numInteriors++;
            break;
        }
    }
    // NOTE: Decompile shows no return value; header declares int32.
    return 1; // TODO: return value not observable in decompilation (EAX left over)
}

// 0x596890
void InteriorGroup_c::SetupPeds() {
    if (m_EnEx && g_interiorPedsEnabled) {
        switch (m_groupType) {
        case 0: SetupHousePeds();  break;
        case 1: SetupShopPeds();   break;
        case 2: SetupOfficePeds(); break;
        }
        m_updatePeds = 1;
    }
}

// 0x596830
void InteriorGroup_c::UpdatePeds() {
    if (m_EnEx && g_interiorPedsEnabled) {
        for (auto& ped : m_pedsToRemove) {
            if (ped) {
                if (ped->IsPointerValid()) {
                    CPopulation::RemovePed(ped);
                }
                ped = nullptr;
            }
        }
        if (m_groupType == 2) { // office
            UpdateOfficePeds();
        }
    }
}

// 0x5965E0
// NOTES:
// - Header declared `int32 SetupHousePeds()`, decomp shows void in original.
// - gangPedType - 7: PED_TYPE_GANG1 == 7, so this is the gang index (eGangID).
void InteriorGroup_c::SetupHousePeds() {
    CStreaming::StreamPedsForInterior(0);
    CStreaming::LoadAllRequestedModels(false);

    const auto entity = m_pEntity;

    m_numPeds = 0;

    CVector pos;
    if (!entity->m_matrix) {
        entity->AllocateMatrix();
        entity->m_placement.UpdateMatrix(entity->m_matrix);
    }
    pos = *entity->m_matrix * pos; // == matrix position (multiplied with a zero vector)

    auto numPedsToCreate = (CGeneral::GetRandomNumber() < 0x33) + 1;
    auto useGangModel    = false;
    auto gangPedType     = (ePedType)0;

    if (CPopCycle::m_pCurrZoneInfo
        && (gangPedType = CPopCycle::PickGangToCreateMembersOf()) != (ePedType)0
        && (int32)CGangs::ChooseGangPedModel((eGangID)(gangPedType - 7)) >= 0) {
        useGangModel = true;
        if (CGeneral::GetRandomNumber() < 0x10) {
            numPedsToCreate = (rand() > 0x3ffe) + 3;
        } else {
            useGangModel = false;
        }
    }

    for (auto i = 0; i < numPedsToCreate; i++) {
        eModelID model;
        if (useGangModel) {
            model = CGangs::ChooseGangPedModel((eGangID)(gangPedType - 7));
        } else {
            auto randFactor = i;
            if (numPedsToCreate == 1) {
                randFactor = rand() < 0x3fff ? 0 : 1;
            }
            model = (eModelID)CStreaming::FindMIPedSlotForInterior(randFactor);
        }

        const auto slot = m_numPeds;
        const auto ped  = CPopulation::AddPed(
            CModelInfo::GetPedModelInfo(model)->GetPedType(),
            model,
            pos,
            false
        );
        m_peds[slot] = ped;
        if (ped) {
            m_numPeds++;
            // NOTE: Not reversed in the repo yet - call the original (0x46CED0)
            plugin::CallMethod<0x46CED0, CScriptsForBrains*>(&CTheScripts::ScriptsForBrains, "house", ped, 3);
            ped->SetCharCreatedBy(PED_MISSION); // 2
            ped->GetIntelligence()->SetPedDecisionMakerType(7);
            const auto task = new CTaskInteriorBeInHouse(this);
            ped->GetTaskManager().SetTask(task, TASK_PRIMARY_DEFAULT, false);
        }
        if (!g_pedAllowedFlags[i]) {
            RemovePed(m_peds[slot]);
        }
    }
}

// 0x595590
int8 InteriorGroup_c::SetupPaths() {
    if (!ArePathsLoaded()) {
        return 1;
    }

    ThePaths.StartNewInterior(m_id);

    // Pass 1: Build the path nodes of the goto points ("rooms") of each interior.
    // baseOffsets[i] = node index of interior #i's first goto point node.
    int32 baseOffsets[8];
    CVector exitPts[16];
    int32  exitNodeIdx[16];
    bool   exitPaired[16];
    int32  numExits = 0;

    int32 nodeCounter = 0; // total number of interior nodes created so far
    int32 base        = 0; // running base node offset

    for (auto i = 0u; i < std::size(m_interiors); i++) {
        const auto interior = m_interiors[i];
        baseOffsets[i] = base;
        if (interior) {
            const auto gotoPts = reinterpret_cast<GrpGotoPt_t*>(reinterpret_cast<uintptr_t>(interior) + 0x410);
            if (interior->field_40C > 2) {
                ThePaths.AddNodeToNewInterior(gotoPts[0].m_pos.x, gotoPts[0].m_pos.y, gotoPts[0].m_pos.z, false, -1, -1, -1, -1, -1, -1);
                ThePaths.AddNodeToNewInterior(gotoPts[1].m_pos.x, gotoPts[1].m_pos.y, gotoPts[1].m_pos.z, false, -1, -1, -1, -1, -1, -1);
                ThePaths.AddInteriorLink(nodeCounter + 1, base);
                nodeCounter += 2;
                for (auto j = 2; j < interior->field_40C; j++) {
                    ThePaths.AddNodeToNewInterior(gotoPts[j].m_pos.x, gotoPts[j].m_pos.y, gotoPts[j].m_pos.z, false, -1, -1, -1, -1, -1, -1);
                    if (j % 2 != 0) {
                        ThePaths.AddInteriorLink(nodeCounter, base + (j - 2));
                        ThePaths.AddInteriorLink(nodeCounter, base + (j - 1));
                    }
                    nodeCounter++;
                }
            }
            base += interior->field_40C;
        }
    }

    // Pass 2: Add the door nodes of every interior, link them to their interior
    // and (for houses) to the closest exterior node. Collect unconnected exits.
    int32 interiorNodeBase = base; // per-interior base node index

    // Saved per-interior base node index at the moment each door block was processed.
    // Used for the direct door-to-door cross-links of interiors without goto points.
    int32 doorBlockBase{}, lDoorBlockBase{}, tDoorBlockBase{}, rDoorBlockBase{};
    bool  interiorPosSet = false;
    int32 curInteriorIdx = 0;

    const auto doors = [](Interior_c* interior) -> DoorInfo_t* {
        return reinterpret_cast<DoorInfo_t*>(reinterpret_cast<uintptr_t>(interior) + 0x512);
    };

    // Processes one of the 4 door blocks of an interior.
    // `storeInteriorPos` - blocks 1..3 set Interior_c::m_position/m_nodeAddress, block 4 doesn't (matches original).
    const auto ProcessDoor = [&](Interior_c* interior, const DoorInfo_t& door, bool storeInteriorPos) {
        const auto nodeInAddr = ThePaths.AddNodeToNewInterior(door.m_posIn.x, door.m_posIn.y, door.m_posIn.z, false, -1, -1, -1, -1, -1, -1);

        if (door.m_neighbor0 != -1) {
            ThePaths.AddInteriorLink(interiorNodeBase, door.m_neighbor0 + baseOffsets[curInteriorIdx]);
        }
        if (door.m_neighbor1 != -1) {
            ThePaths.AddInteriorLink(interiorNodeBase, door.m_neighbor1 + baseOffsets[curInteriorIdx]);
        }

        if (storeInteriorPos && !interiorPosSet) {
            interior->m_position    = door.m_posIn;
            interior->m_nodeAddress = nodeInAddr;
            interiorPosSet = true;
        }

        const auto nodeBIdx    = interiorNodeBase + 1;
        const auto nodeOutAddr = ThePaths.AddNodeToNewInterior(door.m_posOut.x, door.m_posOut.y, door.m_posOut.z, m_groupType != 0, -1, -1, -1, -1, -1, -1);
        ThePaths.AddInteriorLink(nodeBIdx, nodeCounter);

        const auto extNodeAddr = ThePaths.FindNearestExteriorNodeToInteriorNode(nodeBIdx);
        if (extNodeAddr.m_wAreaId == (uint16)-1) {
            // No exterior node nearby (yet) - remember as a pending exit
            exitPts[numExits]     = door.m_posOut;
            exitNodeIdx[numExits] = nodeBIdx;
            exitPaired[numExits]  = false;
            numExits++;
        } else {
            reinterpret_cast<int32&>(interior->field_3EC) = (extNodeAddr.m_wNodeId << 16) | extNodeAddr.m_wAreaId;
            const auto& pathNode = ThePaths.m_pPathNodes[extNodeAddr.m_wAreaId][extNodeAddr.m_wNodeId];
            const auto  nodePos  = pathNode.GetPosition(); // CompressedLargeVector -> CVector (uses NODE_POS_SCALE scale)
            reinterpret_cast<float&>(interior->field_3F4) = nodePos.x; // field_3F4..field_3F6
            reinterpret_cast<float&>(interior->field_3F8) = nodePos.y;
            reinterpret_cast<float&>(interior->field_3FC) = nodePos.z;
            if (m_groupType == 0) { // house - link the door to the exterior node if it's close enough
                const auto distSq = (nodePos - door.m_posOut).SquaredMagnitude();
                if (distSq < MAX_LINK_DIST_SQ) {
                    ThePaths.AddInteriorLinkToExternalNode(nodeBIdx, extNodeAddr);
                }
            }
        }

        interiorNodeBase += 2;
        nodeCounter++;
    };

    // Door block traversal order: m_door, m_lDoorStart, m_tDoorStart, m_rDoorStart
    for (auto i = 0u; i < std::size(m_interiors); i++) {
        const auto interior = m_interiors[i];
        if (!interior) {
            continue;
        }
        curInteriorIdx = i;
        interiorPosSet = false;

        const auto box   = interior->m_box;
        const auto block = doors(interior);

        if (box->m_door >= 0) {
            doorBlockBase = interiorNodeBase;
            ProcessDoor(interior, block[0], true);
        }
        if (box->m_lDoorStart >= 0) {
            lDoorBlockBase = interiorNodeBase;
            ProcessDoor(interior, block[1], true);
        }
        if (box->m_tDoorStart >= 0) {
            tDoorBlockBase = interiorNodeBase;
            ProcessDoor(interior, block[2], true);
        }
        if (box->m_rDoorStart >= 0) {
            rDoorBlockBase = interiorNodeBase;
            ProcessDoor(interior, block[3], false);
        }

        // Interior has no goto points: cross-link the door nodes directly.
        if (interior->field_40C == 0) {
            if (box->m_door >= 0) {
                if (box->m_lDoorStart >= 0) ThePaths.AddInteriorLink(doorBlockBase, lDoorBlockBase);
                if (box->m_tDoorStart >= 0) ThePaths.AddInteriorLink(doorBlockBase, tDoorBlockBase);
                if (box->m_rDoorStart >= 0) ThePaths.AddInteriorLink(doorBlockBase, rDoorBlockBase);
            }
            if (box->m_tDoorStart >= 0) {
                if (box->m_lDoorStart >= 0) ThePaths.AddInteriorLink(tDoorBlockBase, lDoorBlockBase);
                if (box->m_rDoorStart >= 0) ThePaths.AddInteriorLink(tDoorBlockBase, rDoorBlockBase);
            }
        }
    }

    // Pass 3: Pair up pending exits with each other (closest first).
    for (auto i = 0; i < numExits; i++) {
        if (exitPaired[i]) {
            continue;
        }
        auto best       = -1;
        auto bestDistSq = EXIT_PAIR_INIT_DIST;
        for (auto j = i + 1; j < numExits; j++) {
            if (exitPaired[j]) {
                continue;
            }
            const auto distSq = (exitPts[i] - exitPts[j]).SquaredMagnitude();
            if (distSq < bestDistSq) {
                best       = j;
                bestDistSq = distSq;
            }
        }
        if (bestDistSq >= EXIT_PAIR_MAX_DIST) {
            // No partner close enough - drop this exit's links entirely
            // NOTE: Not reversed in the repo yet - call the original (0x44DF60)
            plugin::CallMethod<0x44DF60, CPathFind*>(&ThePaths, (uint32)exitNodeIdx[i]);
        } else {
            exitPaired[i]    = true;
            exitPaired[best] = true;
            ThePaths.AddInteriorLink(exitNodeIdx[i], exitNodeIdx[best]);
        }
    }

    // NOTE: Not reversed in the repo yet - call the original (0x452270)
    plugin::CallMethod<0x452270, CPathFind*>(&ThePaths, nullptr);

    m_pathSetupComplete = 1;
    return 1;
}

// 0x595380
// NOTE: Decompile returns void, but the last call (CPathFind::AreNodesLoadedForArea)
// is the tail call, so its return value is the function's return value.
int8 InteriorGroup_c::ArePathsLoaded() {
    auto* entity = m_pEntity;

    // Bounding box of the entity's collision model
    const auto& bbox = entity->GetColModel()->GetBoundingBox();
    const CVector& boxMin = bbox.m_vecMin;
    const CVector& boxMax = bbox.m_vecMax;

    // The 8 corners of the box, in the exact order the binary builds them
    const CVector corners[8] = {
        { boxMin.x, boxMin.y, boxMin.z },
        { boxMin.x, boxMax.y, boxMin.z },
        { boxMax.x, boxMax.y, boxMin.z },
        { boxMax.x, boxMin.y, boxMin.z },
        { boxMin.x, boxMin.y, boxMax.z },
        { boxMin.x, boxMax.y, boxMax.z },
        { boxMax.x, boxMax.y, boxMax.z },
        { boxMax.x, boxMin.y, boxMax.z },
    };

    CVector min{ 999999.0f, 999999.0f, 999999.0f };
    CVector max{ -999999.0f, -999999.0f, -999999.0f };

    for (auto corner : corners) {
        if (!entity->m_matrix) {
            entity->AllocateMatrix();
            entity->m_placement.UpdateMatrix(entity->m_matrix);
        }
        corner = *entity->m_matrix * corner;
        min.x = std::min(min.x, corner.x);
        min.y = std::min(min.y, corner.y);
        min.z = std::min(min.z, corner.z);
        max.x = std::max(max.x, corner.x);
        max.y = std::max(max.y, corner.y);
        max.z = std::max(max.z, corner.z);
    }

    return ThePaths.AreNodesLoadedForArea(min.x, max.x, min.y, max.y);
}

// 0x595320
void InteriorGroup_c::Setup() {
    if (ContainsInteriorType(2)) {
        m_groupType = 0; // HOUSE
        ReferenceAnims();
        return;
    }

    if (ContainsInteriorType(0) || ContainsInteriorType(6)) {
        m_groupType = 1; // SHOP
    } else {
        m_groupType = (uint8)(ContainsInteriorType(1) ? 2 : -1); // OFFICE / none
    }
    ReferenceAnims();
}

// 0x595290
// NOTE: 0xBB3D9C is an unknown global byte array indexed by ped slot; not found in repo.
int8 InteriorGroup_c::Exit() {
    for (auto i = 0; i < 8; i++) {
        auto* interior = m_interiors[i];
        if (interior) {
            interior->Exit();
            g_interiorMan.ReturnInteriorToPool(interior);
        }
    }

    for (auto i = 0; i < 16; i++) {
        auto*& ped = m_peds[i];
        if (ped) {
            if (!ped->IsPointerValid()) {
                g_pedAllowedFlags[i] = 0; // (&DAT_00bb3d9c)[i] = 0; unknown global, see note above
            } else {
                if (!ped->IsAlive()) {
                    g_pedAllowedFlags[i] = 0; // (&DAT_00bb3d9c)[i] = 0; unknown global, see note above
                }
                CPopulation::RemovePed(ped);
            }
            ped = nullptr;
        }
    }

    m_numPeds = 0;
    ThePaths.RemoveInterior(m_id);

    if (m_animBlockReferenced) {
        switch (m_groupType) {
        case 0: // HOUSE
            CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("int_house"));
            break;
        case 1: // SHOP
            CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("int_shop"));
            break;
        case 2: // OFFICE
            CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("int_office"));
            break;
        default:
            // Decompiled as RemoveAnimBlockRef(param_1) - i.e. `this` passed as index (garbage in binary)
            CAnimManager::RemoveAnimBlockRef((int32)(uintptr)this);
            break;
        }
        m_animBlockReferenced = false;
    }
    return 1;
}

// 0x595250
int8 InteriorGroup_c::ContainsInteriorType(int32 a2) {
    // Decompiled check: first byte of Interior_c::m_box (tEffectInterior, offset 0x14) == a2
    for (auto i = 0; i < 8; i++) {
        const auto* interior = m_interiors[i];
        if (interior && interior->m_box->m_type == a2) {
            return 1;
        }
    }
    return 0;
}

// 0x595200
int8 InteriorGroup_c::CalcIsVisible() {
    m_lastIsVisible = m_isVisible;
    m_isVisible = false;

    if (m_numInteriors > 0) {
        for (auto i = 0; i < m_numInteriors; i++) {
            if (m_interiors[i]->IsVisible()) {
                m_isVisible = true;
                break;
            }
        }
    }
    return m_isVisible;
}

// 0x595160
void InteriorGroup_c::DereferenceAnims() {
    if (!m_animBlockReferenced) {
        return;
    }
    CAnimManager::AddAnimBlockRef(CAnimManager::GetAnimationBlockIndex(GetAnimBlockName()));
    m_animBlockReferenced = false;
}

// 0x5950D0
void InteriorGroup_c::ReferenceAnims() {
    if (m_animBlockReferenced) {
        return;
    }
    const auto animBlkIdx = CAnimManager::GetAnimationBlockIndex(GetAnimBlockName());
    if (CStreaming::IsModelLoaded(IFPToModelId(animBlkIdx))) {
        CAnimManager::AddAnimBlockRef(animBlkIdx);
        m_animBlockReferenced = true;
    } else {
        CStreaming::RequestModel(IFPToModelId(animBlkIdx), STREAMING_KEEP_IN_MEMORY);
    }
}

// 0x594E90
void InteriorGroup_c::UpdateOfficePeds() {
    const auto RemoveAllPeds = [&]() {
        if (m_lastIsVisible) {
            for (auto& ped : m_peds) {
                if (ped) {
                    if (ped->IsPointerValid()) {
                        CPopulation::RemovePed(ped);
                    }
                    ped = nullptr;
                }
            }
            m_numPeds = 0;
        }
    };

    if (!m_isVisible) {
        RemoveAllPeds();
        return;
    }
    if (m_lastIsVisible) { // already visible, peds were set up previously
        return;
    }

    // Became visible - (re)spawn the office peds

    CVector pos;
    if (!m_pEntity->m_matrix) {
        m_pEntity->AllocateMatrix();
        m_pEntity->m_placement.UpdateMatrix(m_pEntity->m_matrix);
    }
    pos = m_pEntity->m_matrix->TransformPoint(CVector{ 0.f, 0.f, 0.f }); // == entity position

    const auto maxPeds = GetNumInteriorInfos(6);

    int32 numPeds;
    if (CClock::GetIsTimeInRange(9, 18)) {
        numPeds = CGeneral::GetRandomNumberInRange(maxPeds / 2, maxPeds);
    } else if (CClock::GetIsTimeInRange(18, 22) || CClock::GetIsTimeInRange(6, 9)) {
        numPeds = CGeneral::GetRandomNumberInRange(0, maxPeds / 2);
    } else {
        numPeds = 0;
    }
    if (numPeds > 15) {
        numPeds = 16;
    }
    if (numPeds < 1) {
        return;
    }

    for (auto i = 0; i < numPeds; i++) {
        const auto modelIdx = CStreaming::FindMIPedSlotForInterior(CGeneral::GetRandomNumber());
        const auto pedType  = *reinterpret_cast<ePedType*>(reinterpret_cast<uintptr_t>(CModelInfo::ms_modelInfoPtrs[modelIdx]) + 0x28); // CPedModelInfo::m_nPedType

        const auto ped = CPopulation::AddPed(pedType, (eModelID)modelIdx, pos, false);
        m_peds[m_numPeds] = ped;
        if (ped) {
            m_numPeds++;
            ped->SetCharCreatedBy(PED_MISSION);
            ped->GetIntelligence()->SetPedDecisionMakerType(7);
            ped->GetTaskManager().SetTask(new CTaskInteriorBeInOffice{ this }, TASK_PRIMARY_DEFAULT);
        }
    }
}

// 0x594E30
// NOTE: Header declared `int8 RemovePed(CPed*)`, but the decompilation shows
// no return value - it is void in the original binary.
void InteriorGroup_c::RemovePed(CPed* ped) {
    for (auto i = 0; i < 16; i++) {
        if (m_peds[i] == ped) {
            CPopulation::RemovePed(ped);
            m_peds[i] = nullptr;
            m_numPeds--;
            return;
        }
    }
}

// 0x594C10
// NOTE: Header declared `int32 SetupShopPeds()`, decomp shows void in original.
void InteriorGroup_c::SetupShopPeds() {
    CStreaming::StreamPedsForInterior(1);
    CStreaming::LoadAllRequestedModels(false);

    m_numPeds = 0;

    // FUN_00821B40 is the binary's rand() thunk == CGeneral::GetRandomNumber()
    const auto numPedsToCreate = (2 - CGeneral::GetRandomNumber()) * m_numInteriors + 1;

    for (auto i = 0; i < numPedsToCreate; i++) {
        const auto randFactor = i == 0 ? 0 : 1 - CGeneral::GetRandomNumber();
        const auto modelIdx   = CStreaming::FindMIPedSlotForInterior(randFactor);
        const auto pedType    = *reinterpret_cast<ePedType*>(reinterpret_cast<uintptr_t>(CModelInfo::ms_modelInfoPtrs[modelIdx]) + 0x28); // CPedModelInfo::m_nPedType

        CVector pos;
        const auto interior = GetRandomInterior();
        int32 tileX, tileY;
        interior->GetRandomTile(3, &tileX, &tileY);
        interior->GetTileCentre((float)tileX, (float)tileY, &pos);
        pos.z += StaticRef<float>(0x858624); // z offset above the floor

        const auto ped = CPopulation::AddPed(pedType, (eModelID)modelIdx, pos, false);
        m_peds[m_numPeds] = ped;
        if (ped) {
            m_numPeds++;
            ped->SetCharCreatedBy(PED_MISSION);
            ped->GetIntelligence()->SetPedDecisionMakerType(7);
            if (i == 0) {
                ped->GetTaskManager().SetTask(new CTaskInteriorShopKeeper{ this, false }, TASK_PRIMARY_DEFAULT);
            } else {
                ped->GetTaskManager().SetTask(new CTaskInteriorBeInShop{ this }, TASK_PRIMARY_DEFAULT);
            }
        }
    }
}

// 0x594BF0
void InteriorGroup_c::SetupOfficePeds() {
    CStreaming::StreamPedsForInterior(2);
    CStreaming::LoadAllRequestedModels(false);
    m_numPeds = 0;
}

// 0x594BD0
CEntity* InteriorGroup_c::GetEntity() {
    return m_pEntity;
}

// 0x594B90
CPed* InteriorGroup_c::GetPed(int32 idx) {
    return m_peds[idx];
}

// 0x594A50
bool InteriorGroup_c::FindClosestInteriorInfo(int32 a, CVector point, float b, InteriorInfo_t** interiorInfo, Interior_c** interior, float* pSome) {
    const auto maxDistSq = b * b;
    auto  bestDistSq   = 999999.0f;
    auto* bestInfo     = (InteriorInfo_t*)nullptr;
    auto* bestInterior = (Interior_c*)nullptr;

    for (auto i = 0; i < 8; i++) {
        auto* int_ = m_interiors[i];
        if (int_ && int_->IsPtInside(point)) {
            for (auto& info : std::span{ int_->m_interiorInfos, (size_t)int_->m_interiorInfosCount }) {
                if ((a == -1 || info.Type == (eInteriorInfoType)a) && !info.IsInUse) {
                    const auto distSq =
                        (point.x - info.Pos.x) * (point.x - info.Pos.x) +
                        (point.y - info.Pos.y) * (point.y - info.Pos.y) +
                        (point.z - info.Pos.z) * (point.z - info.Pos.z);
                    if (distSq < maxDistSq && distSq < bestDistSq) {
                        bestInfo     = &info;
                        bestInterior = int_;
                        bestDistSq   = distSq;
                    }
                }
            }
        }
    }

    if (!bestInfo) {
        return false;
    }

    *interiorInfo = bestInfo;
    *interior     = bestInterior;
    *pSome        = bestDistSq;
    return true;
}

// 0x594970
bool InteriorGroup_c::FindInteriorInfo(eInteriorInfoType infoType, InteriorInfo_t** a3, Interior_c** a4) {
    InteriorInfo_t* matchingInfos[64]{};
    Interior_c*     matchingInteriors[64]{};
    auto            numMatching = 0;

    for (auto* interior : m_interiors) {
        if (!interior) {
            continue;
        }
        for (auto& info : std::span{ interior->m_interiorInfos, (size_t)interior->m_interiorInfosCount }) {
            if (info.Type == infoType && !info.IsInUse) {
                matchingInfos[numMatching]     = &info;
                matchingInteriors[numMatching] = interior;
                numMatching++;
            }
        }
    }

    if (numMatching <= 0) {
        *a3 = nullptr;
        *a4 = nullptr;
        return false;
    }

    const auto idx = CGeneral::GetRandomNumber() % numMatching;
    *a3 = matchingInfos[idx];
    *a4 = matchingInteriors[idx];
    return true;
}

// 0x594920
int32 InteriorGroup_c::GetNumInteriorInfos(int32 a2) {
    int32 count = 0;
    for (auto interior : m_interiors) {
        if (interior && interior->m_interiorInfosCount > 0) {
            for (auto i = 0; i < interior->m_interiorInfosCount; i++) {
                if (interior->m_interiorInfos[i].Type == (eInteriorInfoType)a2) {
                    count++;
                }
            }
        }
    }
    return count;
}

// 0x5948C0
Interior_c* InteriorGroup_c::GetRandomInterior() {
    const auto rand = CGeneral::GetRandomNumber();
    auto nth = 0;
    for (auto i = 0; i < 8; i++) {
        if (m_interiors[i]) {
            if (nth == rand) {
                return m_interiors[i];
            }
            nth++;
        }
    }
    return nullptr;
}

//! @notsa
const char* InteriorGroup_c::GetAnimBlockName() {
    switch ((eInteriorGroupType)m_groupType) {
    case eInteriorGroupType::HOUSE:  return "int_house";
    case eInteriorGroupType::SHOP:   return "int_shop";
    case eInteriorGroupType::OFFICE: return "int_office";
    default:                         NOTSA_UNREACHABLE();
    }
}
