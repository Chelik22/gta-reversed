#include "StdInc.h"

#include "RoadBlocks.h"
#include "PedPlacement.h"
#include "TaskComplexWanderCop.h"
#include "TaskSimpleStandStill.h"
#include "CarCtrl.h"
#include "DamageManager.h"
#include "Collision/ColModel.h"
#include <extensions/File.hpp>

void CRoadBlocks::InjectHooks() {
    RH_ScopedClass(CRoadBlocks);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x461100);
    RH_ScopedInstall(ClearScriptRoadBlocks, 0x460EC0);
    RH_ScopedInstall(ClearSpaceForRoadBlockObject, 0x461020);
    RH_ScopedInstall(CreateRoadBlockBetween2Points, 0x4619C0);
    RH_ScopedInstall(GenerateRoadBlockPedsForCar, 0x461170);
    RH_ScopedInstall(GenerateRoadBlocks, 0x4629E0);
    RH_ScopedInstall(GetRoadBlockNodeInfo, 0x460EE0);
    RH_ScopedInstall(RegisterScriptRoadBlock, 0x460DF0);
}

// 0x461100
void CRoadBlocks::Init() {
    rng::fill(InOrOut, true);
    GenerateDynamicRoadBlocks = false;

    if (notsa::File rbx("data\\paths\\roadblox.dat", "rb"); rbx) {
        rbx.Read(&NumRoadBlocks, sizeof(int32));
        assert(NumRoadBlocks <= MAX_ROADBLOCKS);
        rbx.Read(RoadBlockNodes.data(), RoadBlockNodes.size() * sizeof(CNodeAddress));
    } else {
        NOTSA_UNREACHABLE("roadblox.dat couldn't be opened!");
    }
    ClearScriptRoadBlocks();
}

// 0x460EC0
void CRoadBlocks::ClearScriptRoadBlocks() {
    for (auto& srb : aScriptRoadBlocks) {
        srb.IsActive = false;
    }
}

// 0x461020
// Returns true if cleared successfully.
bool CRoadBlocks::ClearSpaceForRoadBlockObject(CVector cornerA, CVector cornerB){
    int16 numEntities{};
    CEntity* entities[2]{};
    CWorld::FindObjectsIntersectingCube(
        cornerA,
        cornerB,
        &numEntities,
        std::size(entities),
        entities,
        false,
        true,
        true,
        true,
        false
    );

    if (numEntities > std::size(entities) || numEntities <= 0) {
        return numEntities <= 0;
    }

    const auto Remove = [](CEntity* e) {
        CWorld::Remove(e);
        delete e;
    };

    for (auto* entity : entities | rngv::take(numEntities)) {
        switch (entity->GetType()) {
        case ENTITY_TYPE_VEHICLE:
            if (auto* v = entity->AsVehicle(); !v->CanBeDeleted()) {
                return false;
            } else if (!v->vehicleFlags.bCreateRoadBlockPeds) {
                Remove(v);
            }
            break;
        case ENTITY_TYPE_PED:
            if (auto* p = entity->AsPed(); p->CanBeDeleted()) {
                Remove(p);
            } else {
                return false;
            }
            break;
        case ENTITY_TYPE_OBJECT:
            if (auto* o = entity->AsObject(); o->CanBeDeleted() && o->m_nObjectType != OBJECT_GAME) {
                Remove(o);
            } else {
                return false;
            }
            break;
        default:
            NOTSA_UNREACHABLE();
        }
    }

    return true;
}

// 0x4619C0
void CRoadBlocks::CreateRoadBlockBetween2Points(CVector a, CVector b, bool isGangRoadBlock) {
    const auto  dir  = b - a; // dx/dy/dz are used below in their raw (non-normalized) form
    const float dist = dir.Magnitude();

    const auto mid = (a + b) * 0.5f;

    { // 0x59C910 - the original normalizes a copy of the direction, but the result is never used
        auto dirNorm = dir;
        dirNorm.Normalise();
    }

    auto perp = CVector{ dir.y, -dir.x, 0.0f }; // perpendicular to the line
    perp.Normalise();

    // Rotate the perpendicular away from the player (0x461AF0)
    const auto playerCoors = FindPlayerCoors();
    if ((playerCoors - mid).Dot(perp) < 0.0f) {
        perp *= -1.0f;
    }

    // Police car model choice (0x461B10)
    eModelID vehModel;
    if (!isGangRoadBlock) {
        const auto wanted = FindPlayerWanted();
        if      (wanted->AreArmyRequired()) vehModel = MODEL_BARRACKS; // 0x1B1
        else if (wanted->AreFbiRequired())  vehModel = MODEL_FBIRANCH; // 0x1EA
        else if (wanted->AreSwatRequired()) vehModel = MODEL_ENFORCER; // 0x1AB
        else                                vehModel = CStreaming::GetDefaultCopCarModel(false);
        if (!CStreaming::IsModelLoaded(vehModel)) {
            vehModel = CStreaming::GetDefaultCopCarModel(false);
        }
        if (vehModel == MODEL_COPBIKE) { // 0x20B - a bike is no good for a road block
            return;
        }
    } else {
        vehModel = CPopulation::PickRiotRoadBlockCar();
        if (vehModel == MODEL_INVALID) {
            return;
        }
    }

    const auto* vehColModel = CModelInfo::GetModelInfo(vehModel)->GetColModel();
    const auto& bbMin = vehColModel->m_boundBox.m_vecMin;
    const auto& bbMax = vehColModel->m_boundBox.m_vecMax;

    float spacingX = (bbMax.x - bbMin.x) + 2.0f; // 0x858CA0 - step along X (perpendicular cars)
    float spacingY = (bbMax.y - bbMin.y) + 0.2f; // 0x858CC4 - step along Y
    if (isGangRoadBlock) {
        spacingY += 0.5f;                        // 0x858B8C
    }

    // How many slots fit on the line (max. 5) (0x461CC0)
    bool slotIsPerp[5]{};
    unsigned numSlots{};
    auto remaining = dist;
    for (unsigned slot = 0; slot < 5; slot++) {
        slotIsPerp[slot] = (vehModel != MODEL_BARRACKS && !isGangRoadBlock) ? (rand() & 1) != 0 : false;
        const float spacing = slotIsPerp[slot] ? spacingX : spacingY;
        if (remaining < spacing) {
            break;
        }
        remaining -= spacing;
        numSlots++;
    }

    auto offset = remaining / float(numSlots + 1); // position of the next slot
    if (numSlots > 0) { // 0x461D53
        spacingY *= 0.5f;
        // fVar9 from the decompile (line 176): computed once before the loop
        const float advance = spacingY + offset;

        CMatrix mat; // stack matrix
        for (unsigned slot = 0; slot < numSlots; slot++) {
            auto spawnBurningCar = false;

            if (!slotIsPerp[slot]) { // police car along the road (0x461E33)
                offset += spacingY;
                if (isGangRoadBlock) {
                    spawnBurningCar = (rand() & 0xFF) < 0x40;
                }
                if (spawnBurningCar) {
                    mat.GetRight()   = CVector{ 0.f, 0.f, 1.f };
                    mat.GetUp()      = CVector{ -dir.y, dir.x, 0.f };
                } else {
                    mat.GetRight()   = CVector{ dir.y, -dir.x, 0.f }; // FIX (verify wf5b): was {dir.y, 0.f, -dir.x}; decompile/disasm: right=(dy,-dx,0)
                    mat.GetUp()      = CVector{ 0.f, 0.f, 1.f };
                }
                mat.GetForward() = dir;
                if (rand() & 1) {
                    if (spawnBurningCar) {
                        mat.RotateY(3.14159265f /* 0x40490FDB */);
                    } else {
                        mat.RotateZ(3.14159265f /* 0x40490FDB */);
                    }
                }
            } else { // perpendicular car (0x461D87)
                mat.GetRight()   = CVector{ perp.y, -perp.x, 0.f };
                mat.GetForward() = perp;
                mat.GetUp()      = CVector{ 0.f, 0.f, 1.f };
                offset += spacingX * 0.5f;
            }

            // Random rotation around Z
            float jitter;
            if (!slotIsPerp[slot]) {
                jitter = ((rand() & 0xFF) - 128.0f) * (isGangRoadBlock ? 0.006f : 0.003f); // 0x859CDC / 0x859CD8
            } else {
                jitter = ((rand() & 0xFF) - 128.0f) * (isGangRoadBlock ? 0.004f : 0.002f); // 0x859CE0 / 0x858F44
            }
            mat.RotateZ(jitter);

            mat.SetTranslateOnly(a + dir * offset);
            mat.GetPosition().z -= spawnBurningCar ? bbMin.x : bbMin.z;
            mat.GetPosition().z += 0.3f; // 0x858C24
            offset += advance;

            // Car bounds in world space
            const auto cornerA = mat.TransformPoint(bbMin);
            const auto cornerB = mat.TransformPoint(bbMax);

            if (!ClearSpaceForRoadBlockObject(cornerA, cornerB)) {
                continue;
            }

            if (!isGangRoadBlock || (!spawnBurningCar && (rand() & 0xFF) >= 0x40)) { // 0x4621EF - the car
                auto* car = new CAutomobile(vehModel, RANDOM_VEHICLE, true); // 0x988 (no null-check in the original)

                car->SetStatus(eEntityStatus::STATUS_ABANDONED); // byte+0x36 = (type & 7) | 0x20
                mat.GetPosition().z += car->GetHeightAboveRoad() - 0.6f; // vtable+0xD4; 0x858CC8 = 0.6f
                car->m_matrix->CMatrix::operator=(mat); // 0x59BBC0 CMatrix::operator= (CMatrixLink не наследует op=)
                car->PlaceOnRoadProperly();      // 0x6AF420
                car->SetIsStatic(false);         // vtable+0x10(0)
                if (car->GetRwObject()) {
                    car->UpdateRwMatrix();       // inlined CEntity::UpdateRwMatrix (0x446F90)
                }
                CCarCtrl::JoinCarWithRoadSystem(car); // 0x42F5A0 - stub, plugin::Call<0x42F5A0, CVehicle*>(car)
                car->m_nDoorLock = CARLOCK_UNLOCKED;  // +0x4F8 = 1

                car->vehicleFlags.bNeverUseSmallerRemovalRange = true; // byte+0x42C |= 0x40 (bit 38) - FIX (verify wf5b) per disasm 0x4622A4+ / SetEngineOn 0x41BDD0
                car->vehicleFlags.bIsLocked  = false; // byte+0x428 &= 0xE7: bit 3
                car->vehicleFlags.bEngineOn  = false; // byte+0x428 &= 0xE7: bit 4 (were incorrectly bLightsOn/bFreebies - those live in bits 6/7)
                car->m_autoPilot.m_nCurrentLane  = 0;             // +0x3B7
                car->m_autoPilot.m_nNextLane     = 0;             // +0x3B8
                car->m_autoPilot.m_nCarMission   = MISSION_NONE;  // +0x3BA
                car->m_autoPilot.m_nTempAction   = TEMPACT_NONE;  // +0x3BB
                car->m_autoPilot.m_speed         = 0.0f;          // +0x3C8
                car->m_autoPilot.m_nCruiseSpeed  = 0;             // +0x3D0

                if (isGangRoadBlock) { // 0x4622F4
                    // (rand() & 0xFFFF) * (1/32768 = 0x858B14) * 4.0f (0x858B90) -> 0..3
                    if (const auto roll = (int)((rand() & 0xFFFF) * (1.f / 32768.f) * 4.0f)) {
                        car->BlowUpCarCutSceneNoExtras(true, true, true, true); // vtable+0xA8
                        car->m_nTimeWhenBlowedUp += 1'000'000; // +0x4D8
                        spawnBurningCar = true;
                    } else {
                        car->SetTotalDamage(true);
                    }
                } else if (car->UsesSiren() && (rand() & 1)) { // 0x462342
                    car->vehicleFlags.bSirenOrAlarm = true; // byte+0x42D |= 0x80 (bit 47) - FIX (verify wf5b) (was bDoesProvideCover, bit 50 -> 0x42E,0x04)
                }

                if (car->m_matrix->GetUp().z <= 0.94f) { // 0x859CD4 - car on a slope: don't place it (vtable[0](1) = delete)
                    delete car;
                    continue;
                }

                CVisibilityPlugins::SetClumpAlpha(car->GetRpClump(), 0); // invisible until needed
                CWorld::Add(car);
                car->vehicleFlags.bCreateRoadBlockPeds = true; // byte+0x42A |= 0x10 (bit 20) - FIX (verify wf5b) (was bGunSwitchedOff, bit 23 -> 0x42A,0x80)
                car->m_nTimeTillWeNeedThisCar = CTimer::GetTimeInMS() + 7000; // +0x4D0
                car->m_nPedsPositionForRoadBlock = numSlots < 4 ? 2 : 1;      // +0x4AE

                if (!slotIsPerp[slot]) { // 0x46242A - roadblock side marker relative to the player
                    const auto dot = (car->GetPosition() - FindPlayerCoors()).Dot(car->m_matrix->GetRight());
                    car->m_nPacMansCollected = dot >= 0.0f ? 1 : 0; // +0x4AD (misnamed in the repo - not PacMans)
                } else { // 0x4623C6 - open the front doors
                    car->m_nPacMansCollected = 2;
                    if (const auto door = CDamageManager::GetCarNodeIndexFromDoor(DOOR_LEFT_FRONT); car->m_aCarNodes[door]) {
                        car->OpenDoor(nullptr, door, DOOR_LEFT_FRONT, 1.0f, true); // vtable+0x6C
                    }
                    if (car->m_nPedsPositionForRoadBlock > 1) {
                        if (const auto door = CDamageManager::GetCarNodeIndexFromDoor(DOOR_RIGHT_FRONT); car->m_aCarNodes[door]) {
                            car->OpenDoor(nullptr, door, DOOR_RIGHT_FRONT, 1.0f, true);
                        }
                    }
                }

                if (spawnBurningCar) { // 0x462490
                    gFireManager.StartFire(car, nullptr, 2.8f /* 0x40333333 */, 1, 60'000, 2);
                    // Оригинал: m_pFire->m_RemovalDist = 0x5C (92) — поле приватное (Fire.h), сеттера в репо нет.
                    // Остаётся дефолт 60 (Fire.cpp:44): пожар убирается при отдалении камеры чуть раньше.
                }
            } else { // 0x462110 - burning wrecked-car object
                auto* obj = new CObject(ModelIndices::MI_ROADBLOCKFUCKEDCAR1, true); // 0x17C (no null-check in the original)

                obj->m_matrix->CMatrix::operator=(mat); // 0x59BBC0 CMatrix::operator=
                obj->SetPosn(mat.GetPosition());
                obj->SetIsStatic(false); // vtable+0x10(0)
                CObject::nNoTempObjects++;
                obj->m_nObjectType  = OBJECT_TEMPORARY;               // +0x13C = 3
                obj->m_nRemovalTime = CTimer::GetTimeInMS() + 600'000; // +0x150
                CWorld::Add(obj);
                gFireManager.StartFire(obj, nullptr, 2.8f /* 0x40333333 */, 1, 60'000, 2);
                // Оригинал: m_pFire->m_RemovalDist = 0x5C (92) — поле приватное, сеттера нет; см. коммент. выше.
            }
        }
    }

    // 0x4624DF - roadwork barriers (non-gang only)
    if (!isGangRoadBlock) {
        const auto& barrierModel = ModelIndices::MI_ROADWORKBARRIER1;
        const auto* barrierColModel = CModelInfo::GetModelInfo(barrierModel)->GetColModel();
        const auto& bMin = barrierColModel->m_boundBox.m_vecMin;
        const auto& bMax = barrierColModel->m_boundBox.m_vecMax;

        const float barrierSpacing = (bMax.x - bMin.x) + 0.5f; // 0x858B8C
        const auto  barrierCount   = std::min(8, (int)(dist / barrierSpacing)); // _ftol2(dist / spacing), cap 8
        const float barrierGap     = (dist - float(barrierCount) * barrierSpacing) / float(barrierCount + 1);

        CObject::DeleteAllTempObjectsInArea(mid, dist * 0.5f);

        for (int i = 0; i < barrierCount; i++) { // 0x4625E0
            const float offset = (float(i) + 0.5f) * barrierSpacing + float(i + 1) * barrierGap;

            CMatrix mat;
            mat.SetUnity();
            mat.SetTranslate(CVector{ 0.f, 0.f, 0.f }); // 0x59AF40(0,0,0)
            mat.GetRight()   = dir;
            mat.GetForward() = CVector{ dir.y, -dir.x, 0.0f };
            mat.GetUp()      = CVector{ 0.f, 0.f, 1.f };
            mat.RotateZ(((rand() & 0xFF) - 128.0f) * 0.003f); // 0x859CD8
            mat.SetTranslateOnly(a + dir * offset + perp * 5.0f); // 0x858C80 = 5.0f - away from the player
            mat.GetPosition().x += (rand() & 0xF) * 0.05f; // 0x858C28
            mat.GetPosition().y += (rand() & 0xF) * 0.05f;

            bool found{};
            mat.GetPosition().z = CWorld::FindGroundZFor3DCoord(CVector{ mat.GetPosition().x, mat.GetPosition().y, mat.GetPosition().z + 2.0f }, &found);
            if (found) {
                mat.GetPosition().z -= bMin.z;
                const auto cornerA = mat.TransformPoint(bMin);
                const auto cornerB = mat.TransformPoint(bMax);
                if (ClearSpaceForRoadBlockObject(cornerA, cornerB)) {
                    auto* obj = new CObject(barrierModel, true);
                    obj->m_matrix->CMatrix::operator=(mat); // 0x59BBC0 CMatrix::operator=
                    obj->SetPosn(mat.GetPosition());
                    CObject::nNoTempObjects++;
                    obj->m_nObjectType  = OBJECT_TEMPORARY;
                    obj->m_nRemovalTime = CTimer::GetTimeInMS() + 600'000;
                    CWorld::Add(obj);
                }
            }
        }
    }
}

// 0x461170
void CRoadBlocks::GenerateRoadBlockPedsForCar(CVehicle* vehicle, int32 pedsPositionsType, ePedType pedType) {
    const auto Generate = [&](eModelID pedModel = MODEL_INVALID, eCopType copType = COP_TYPE_CITYCOP, bool isSpecialCop = false) {
        static constexpr auto PLACEMENTS = std::to_array<CVector>({
            { -1.5f, +1.9f, 0.0f },
            { -1.5f, -2.6f, 0.0f },
            { +1.5f, +1.9f, 0.0f },
            { +1.5f, -2.6f, 0.0f },
            { -1.5f,  0.0f, 0.0f },
            { +1.5f,  0.0f, 0.0f },
        });

        static constexpr auto SPECIAL_COP_PLACEMENTS = std::to_array<CVector>({
            {  0.0f, +3.2f, 0.0f },
            { +1.5f, -1.8f, 0.0f },
            {  0.0f, +3.2f, 0.0f },
            { -1.5f, -1.8f, 0.0f },
            { -1.5f,  0.0f, 0.0f },
            { +1.5f,  0.0f, 0.0f },
        });

        const auto placementIdx = 2 * pedsPositionsType;
        const auto radiusRatio  = vehicle->GetColModel()->GetBoundingSphere().m_fRadius
            / CModelInfo::GetModelInfo(CStreaming::GetDefaultCopCarModel(false))->GetColModel()->GetBoundingSphere().m_fRadius;

        for (auto i = 0u; i < vehicle->m_nNumPedsForRoadBlock; i++) {
            const auto offset = (isSpecialCop ? SPECIAL_COP_PLACEMENTS : PLACEMENTS)[placementIdx + i] * radiusRatio;
            const auto pos = vehicle->GetMatrix().TransformPoint(offset);

            auto* ped = [&]() -> CPed* {
                if (pedType != PED_TYPE_COP) { // 0x461560
                    return new CCivilianPed(pedType, pedModel);
                } else {
                    auto* p = new CCopPed(CStreaming::IsModelLoaded(pedModel) ? copType : COP_TYPE_CITYCOP);
                    if (copType == COP_TYPE_CITYCOP) {
                        p->SetCurrentWeapon(WEAPON_PISTOL);
                    }
                    return p;
                }
            }();

            ped->SetPosn(std::get<CVector>(CPedPlacement::FindZCoorForPed(pos)));
            ped->GetMatrix().SetRotateKeepPos({ 0.0f, 0.0f, -HALF_PI });

            if (pedType == PED_TYPE_COP) {
                auto* t = new CTaskComplexWanderCop(PEDMOVE_STILL, CGeneral::GetRandomNumberInRange(8ui8));
                t->m_nSubTaskCreatedTimer = {};
                t->m_nScanForStuffTimer   = {};
                ped->GetTaskManager().SetTask(t, TASK_PRIMARY_PRIMARY);
            }
            ped->GetTaskManager().SetTask(new CTaskSimpleStandStill(0, true), TASK_PRIMARY_DEFAULT);

            ped->bStayInSamePlace         = true;
            ped->bNotAllowedToDuck        = true;
            ped->m_nTimeTillWeNeedThisPed = CTimer::GetTimeInMS() + 10'000;
            ped->bCrouchWhenShooting      = !isSpecialCop || pedsPositionsType != 2;
            ped->bCullExtraFarAway        = true;
            CEntity::RegisterReference(ped->m_pVehicle = vehicle);
            CVisibilityPlugins::SetClumpAlpha(ped->GetRpClump(), 0);

            if (pedType != PED_TYPE_COP) {
                const auto weapon = CGangs::Gang[pedType - PED_TYPE_GANG1].GetRandomWeapon(false);
                if (weapon != WEAPON_UNARMED) {
                    ped->GiveDelayedWeapon(weapon, 25'001);
                    ped->SetCurrentWeapon(weapon);
                }
            }
            CWorld::Add(ped);
            ped->GetEventGroup().Add<CEventScriptCommand>({ TASK_PRIMARY_PRIMARY, new CTaskComplexKillPedOnFoot(FindPlayerPed()) });
        }
    };

    if (pedType == PED_TYPE_COP) {
        switch (vehicle->GetModelId()) {
        case MODEL_ENFORCER: Generate(MODEL_SWAT,    COP_TYPE_SWAT1,   true); break;
        case MODEL_BARRACKS: Generate(MODEL_ARMY,    COP_TYPE_ARMY,    true); break;
        case MODEL_FBIRANCH: Generate(MODEL_FBI,     COP_TYPE_FBI,     true); break;
        case MODEL_COPCARRU: Generate(MODEL_INVALID, COP_TYPE_CITYCOP, true); break;
        default:             Generate(MODEL_INVALID, COP_TYPE_CITYCOP, false); break;
        }
    } else if (IsPedTypeGang(pedType)) {
        for (auto i = 0; i < TOTAL_GANGS; i++) {
            if (!CPopCycle::m_pCurrZoneInfo->GangStrength[i]) {
                continue;
            }
            const auto pedModel = CGangs::ChooseGangPedModel((eGangID)i);
            if (pedModel == MODEL_INVALID) {
                continue;
            }
            Generate(pedModel);
            return;
        }
    } else {
        Generate();
    }
}

// 0x4629E0
void CRoadBlocks::GenerateRoadBlocks() {
    ZoneScoped;

    if (FindPlayerWanted()->m_ChanceOnRoadBlock && FindPlayerVehicle()) {
        if (!GenerateDynamicRoadBlocks) {
            rng::fill(InOrOut, true);
            GenerateDynamicRoadBlocks = true;
        }

        const auto counter1      = MAX_ROADBLOCKS * (CTimer::GetFrameCounter() % 16 + 1);
        const auto rbsToGenerate = std::min((uint32)NumRoadBlocks, ((counter1 % 16) + counter1) / 16);
        auto       counter2      = MAX_ROADBLOCKS * (CTimer::GetFrameCounter() % 16) / 16;

        for (; counter2 < rbsToGenerate; counter2++) {
            const auto& mrbNode = RoadBlockNodes[counter2];
            if (!ThePaths.IsAreaLoaded(mrbNode)) {
                continue;
            }
            const auto& mainNode = ThePaths.GetPathNode(mrbNode);
            const auto  playerPos = FindPlayerCoors();
            if (std::abs(playerPos.x - mainNode->GetPosition().x) >= 90.0f ||
                std::abs(playerPos.y - mainNode->GetPosition().y) >= 90.0f ||
                DistanceBetweenPoints2D(playerPos, mainNode->GetPosition()) >= 90.0f)
            {
                InOrOut[counter2] = false;
                continue;
            }

            if (InOrOut[counter2]) {
                continue;
            }
            InOrOut[counter2] = true;

            if (CGeneral::GetRandomNumberInRange(128u) >= FindPlayerWanted()->m_ChanceOnRoadBlock) {
                continue;
            }

            float mrbWidth{};
            CVector mrbDir{};
            if (!GetRoadBlockNodeInfo(mrbNode, mrbWidth, mrbDir)) {
                continue;
            }

            if (mainNode->m_nPathWidth) {
                const auto width = mainNode->m_nPathWidth / 16.0f;
                CreateRoadBlockBetween2Points(
                    mainNode->GetPosition() + mrbDir * (mrbWidth / 2.f + width),
                    mainNode->GetPosition() + mrbDir * width,
                    false
                );
                CreateRoadBlockBetween2Points(
                    mainNode->GetPosition() - mrbDir * width,
                    mainNode->GetPosition() - mrbDir * (mrbWidth / 2.f + width),
                    false
                );
                continue;
            }

            for (auto&& [i, nodeAddr] : rngv::enumerate(RoadBlockNodes)) {
                if (counter2 == i || InOrOut[i] || !ThePaths.IsAreaLoaded(nodeAddr.m_wAreaId)) {
                    continue;
                }
                const auto& node = ThePaths.GetPathNode(nodeAddr);

                if (std::abs(mainNode->GetPosition().x - node->GetPosition().x) >= 30.0f ||
                    std::abs(mainNode->GetPosition().y - node->GetPosition().y) >= 30.0f)
                {
                    continue;
                }

                float   width{};
                CVector dir{}; 
                if (!GetRoadBlockNodeInfo(nodeAddr, width, dir)) {
                    continue;
                }

                if (mrbWidth != width || dir.Dot(mrbDir) <= 0.7f) {
                    continue;
                }

                [[maybe_unused]] CColPoint col{};
                [[maybe_unused]] CEntity*  colEntity{};
                if (CWorld::ProcessLineOfSight(
                    mainNode->GetPosition() + CVector{0.0f, 0.0f, 1.0f},
                    node->GetPosition() + CVector{0.0f, 0.0f, 1.0f},
                    col,
                    colEntity,
                    true,
                    false,
                    false,
                    false,
                    false,
                    false,
                    false,
                    false
                )) {
                    continue;
                }

                const auto dirFromMain = (node->GetPosition() - mainNode->GetPosition()).Normalized();
                CreateRoadBlockBetween2Points(
                    node->GetPosition()     + dirFromMain * (mrbWidth / 2.0f),
                    mainNode->GetPosition() - dirFromMain * (mrbWidth / 2.0f),
                    false
                );
                InOrOut[i] = true;

                if (i == NumRoadBlocks) {
                    CreateRoadBlockBetween2Points(
                        mainNode->GetPosition() - dirFromMain * (mrbWidth / 2.0f),
                        mainNode->GetPosition() + dirFromMain * (mrbWidth / 2.0f),
                        false
                    );
                    break;
                }
            }
        }
    } else {
        GenerateDynamicRoadBlocks = false;
    }

    if (auto& srb = aScriptRoadBlocks[CTimer::GetFrameCounter() % MAX_SCRIPT_ROADBLOCKS]; srb.IsActive) {
        const auto c = CVector::Centroid({ srb.CornerA, srb.CornerB });

        if (DistanceBetweenPoints(FindPlayerCoors(), c) >= 90.0f) {
            srb.IsSafeToCreate = true;
        } else if (srb.IsSafeToCreate) {
            CreateRoadBlockBetween2Points(srb.CornerA, srb.CornerB, srb.IsGangRoadBlock);
            srb.IsSafeToCreate = false;
        }
    }
}

// 0x460EE0
bool CRoadBlocks::GetRoadBlockNodeInfo(CNodeAddress nodeAddress, float& outWidth, CVector& outDir) {
    auto* const node = ThePaths.GetPathNode(nodeAddress);
    assert(node);

    assert(node->m_nNumLinks >= 2);
    const auto naviLinkAddrA = ThePaths.GetNaviLink(nodeAddress.m_wAreaId, node->m_wBaseLinkId + 0),
               naviLinkAddrB = ThePaths.GetNaviLink(nodeAddress.m_wAreaId, node->m_wBaseLinkId + 1);
    if (!ThePaths.IsAreaLoaded(naviLinkAddrA.m_wAreaId) || !ThePaths.IsAreaLoaded(naviLinkAddrB.m_wAreaId)) {
        return false;
    }

    const auto &naviLinkA = ThePaths.GetCarPathLink(naviLinkAddrA),
               &naviLinkB = ThePaths.GetCarPathLink(naviLinkAddrB);

    const auto maxNumLanes = std::max(
        naviLinkA.m_numOppositeDirLanes + naviLinkA.m_numSameDirLanes,
        naviLinkB.m_numOppositeDirLanes + naviLinkB.m_numSameDirLanes
    );

    outWidth = ((float)maxNumLanes + 1.f) * 5.f;
    outDir   = CVector{ (naviLinkB.GetNodeCoors() - naviLinkA.GetNodeCoors()).GetPerpRight(), 0.f }.Normalized();

    return true;
}

// 0x460DF0
void CRoadBlocks::RegisterScriptRoadBlock(CVector cornerA, CVector cornerB, bool isGangRoadBlock) {
    auto free = rng::find_if(aScriptRoadBlocks, [](const auto& srb) { return !srb.IsActive; });
    if (free == aScriptRoadBlocks.end()) {
        // No free script roadblock found
        return;
    }

    free->CornerA         = cornerA;
    free->CornerB         = cornerB;
    free->IsActive        = true;
    free->IsSafeToCreate  = true;
    free->IsGangRoadBlock = isGangRoadBlock;
}
