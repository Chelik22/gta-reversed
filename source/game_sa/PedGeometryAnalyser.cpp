#include "StdInc.h"

#include "PedGeometryAnalyser.h"

#include "World.h"
#include "WaterLevel.h"
#include "PedGroups.h"
#include "SurfaceInfos_c.h"
#include "Models/ModelInfo.h"
#include "Entity/Ped/Ped.h"
#include "PedIntelligence.h"
#include "Tasks/TaskManager.h"
#include "Tasks/TaskTypes/TaskSimpleSwim.h"
#include "Tasks/TaskTypes/TaskSimpleJetPack.h"
#include "Collision/Collision.h"
#include "Interior/InteriorManager_c.h"
#include "Pools/PedPool.h"
#include "Pools/Pools.h"

// Constants used all over the analyser (reconstruction of the binary's float table)
namespace PedGeometryAnalyserConstants { // just for readability, all are file-local references
    // TODO: Give these proper names once their role is 100% clear
    // (0x86C6AC ~0.6, 0x858FCC 0.15f, 0x858C20 - used only by the disabled
    // CanPedJumpObstacle("contacted") overload, see the redirect above)
    static const auto& FLT_086C69C = StaticRef<float>(0x86C69C); // CanPedTargetPoint max distance
    static const auto& FLT_086C6A4 = StaticRef<float>(0x86C6A4); // ComputeClearTarget max entity distance
    static const auto& FLT_086C6A0 = StaticRef<float>(0x86C6A0); // IsInAir ground probe distance
    static const auto& FLT_0858B8C = StaticRef<float>(0x858B8C); // 0.5f
    static const auto& FLT_0858B3C = StaticRef<float>(0x858B3C); // 3.0f probe distance / z threshold
    static const auto& FLT_0858C1C = StaticRef<float>(0x858C1C); // -1.0f (ped forward is -Y)
    static const auto& FLT_0858624 = StaticRef<float>(0x858624); // ~0.1f small z tolerance / 1.0f
    static const auto& FLT_0858CDC = StaticRef<float>(0x858CDC); // den min. in LOS plane clipping
    static const auto& FLT_0858F14 = StaticRef<float>(0x858F14); // bounding sphere radius multiplier
    static const auto& FLT_0858CA0 = StaticRef<float>(0x858CA0); // 2.0f
    static const auto& FLT_0858F34 = StaticRef<float>(0x858F34); // CanPedTargetPoint LOS start z offset
    static const auto& FLT_0858B50 = StaticRef<float>(0x858B50); // 0.0f
    static const auto& FLT_086C6AC = StaticRef<float>(0x86C6AC); // ~0.17f - CanPedJumpObstacle("contacted") normal.z threshold
    static const auto& FLT_0858FCC = StaticRef<float>(0x858FCC); // 0.15f
    static const auto& FLT_0858C20 = StaticRef<float>(0x858C20); // 0.9f
    static const auto& FLT_0858B90 = StaticRef<float>(0x858B90); // 4.0f
    static const auto& FLT_0858CC4 = StaticRef<float>(0x858CC4); // +0.2f - route clipping upper margin
    static const auto& FLT_0858FDC = StaticRef<float>(0x858FDC); // -0.2f - route clipping lower margin
    static const auto& FLT_0858C84 = StaticRef<float>(0x858C84); // 0.25f
    static const auto& FLT_086C6A8 = StaticRef<float>(0x86C6A8); // 6.0f - entity bounding box height threshold
    static const auto& FLT_0872490 = StaticRef<float>(0x872490); // ~1.001f
    static auto&       FLT_08D22B0 = StaticRef<float>(0x8D22B0); // margin/slope global (float&, writable)
}

// volatile => the compiler uses a memory operand at *every* use (the original
// always reads these from memory - never keeps them in registers)
volatile const float FLT_M1_858C1C = -1.0f; // 0x858C1C (ped forward is -Y)
volatile const float FLT_0_858B50  = 0.0f;  // 0x858B50

void CPedGeometryAnalyser::InjectHooks() {
    RH_ScopedClass(CPedGeometryAnalyser);
    RH_ScopedCategoryGlobal();

    RH_ScopedOverloadedInstall(CanPedJumpObstacle, "", 0x5F1B00, bool(*)(const CPed&,const CEntity&));
    RH_ScopedOverloadedInstall(CanPedJumpObstacle, "contacted", 0x5F32D0, bool(*)(const CPed&,const CEntity&,const CVector&,const CVector&));
    RH_ScopedInstall(CanPedTargetPed, 0x5F1C40);
    RH_ScopedInstall(CanPedTargetPoint, 0x5F1B70);
    RH_ScopedInstall(ComputeBuildingHitPoints, 0x5F1E30);
    RH_ScopedInstall(ComputeClearTarget, 0x5F5D80);
    RH_ScopedOverloadedInstall(ComputeClosestSurfacePoint, "ped", 0x5F3B70, bool (*)(const CPed& ped, CEntity& entity, CVector& point));
    RH_ScopedOverloadedInstall(ComputeClosestSurfacePoint, "posn", 0x5F36F0, bool(*)(const CVector&,CEntity&,CVector&));
    RH_ScopedOverloadedInstall(ComputeClosestSurfacePoint, "rect", 0x5F2C10, bool(*)(const CVector&,const CVector*,CVector&));
    RH_ScopedInstall(ComputeEntityBoundingBoxCentreUncached, 0x5F1600);
    RH_ScopedInstall(ComputeEntityBoundingBoxCentreUncachedAll, 0x5F3B40);
    RH_ScopedInstall(ComputeEntityBoundingBoxCorners, 0x5F3650);
    RH_ScopedInstall(ComputeEntityBoundingBoxCornersUncached, 0x5F1FA0);
    RH_ScopedInstall(ComputeEntityBoundingBoxPlanes, 0x5F3660);
    RH_ScopedInstall(ComputeEntityBoundingBoxPlanesUncached, 0x5F1670);
    RH_ScopedInstall(ComputeEntityBoundingBoxPlanesUncachedAll, 0x5F2B80);
    RH_ScopedInstall(ComputeEntityBoundingBoxSegmentPlanes, 0x5F36A0);
    RH_ScopedInstall(ComputeEntityBoundingBoxSegmentPlanesUncached, 0x5F1750);
    RH_ScopedInstall(ComputeEntityBoundingBoxSegmentPlanesUncachedAll, 0x5F2BC0);
    RH_ScopedInstall(ComputeEntityBoundingSphere, 0x5F3C20);
    RH_ScopedInstall(ComputeMoveDirToAvoidEntity, 0x5F3730);
    RH_ScopedInstall(ComputeEntityDirs, 0x5F1500);
    RH_ScopedOverloadedInstall(ComputeEntityHitSide, "1", 0x5F3BC0, int32 (*)(const CPed& ped, CEntity& entity));
    RH_ScopedOverloadedInstall(ComputeEntityHitSide, "2", 0x5F1450, int32 (*)(const CVector& point1, const CVector* point2, const float* x));
    RH_ScopedOverloadedInstall(ComputeEntityHitSide, "3", 0x5F3AC0, int32 (*)(const CVector& point, CEntity& entity));
    RH_ScopedOverloadedInstall(ComputePedHitSide, "physical", 0x5F3640, int32(*)(const CPed&,const CPhysical&));
    RH_ScopedOverloadedInstall(ComputePedHitSide, "posn", 0x5F1E70, int32(*)(const CPed&,const CVector&));
    RH_ScopedInstall(ComputePedShotSide, 0x5F13F0);
    RH_ScopedOverloadedInstall(ComputeRouteRoundEntityBoundingBox, "1", 0x5F6110, int32(*)(const CPed&,CEntity&,const CVector&,CPointRoute&,int32));
    RH_ScopedOverloadedInstall(ComputeRouteRoundEntityBoundingBox, "2", 0x5F3DD0, int32(*)(const CPed&,const CVector&,CEntity&,const CVector&,CPointRoute&,int32));
    RH_ScopedInstall(ComputeRouteRoundSphere, 0x5F1890);
    RH_ScopedOverloadedInstall(GetIsLineOfSightClear, "ped", 0x5F5A30, bool(*)(const CPed&,const CVector&,CEntity&,float&));
    RH_ScopedOverloadedInstall(GetIsLineOfSightClear, "v3d", 0x5F2F00, bool(*)(const CVector&,const CVector&,CEntity&));
    RH_ScopedInstall(GetNearestPed, 0x5F3590);
    RH_ScopedInstall(IsEntityBlockingTarget, 0x5F3970);
    RH_ScopedInstall(IsInAir, 0x5F1CB0);
    RH_ScopedInstall(IsWanderPathClear, 0x5F2F70);
    RH_ScopedInstall(LiesInsideBoundingBox, 0x5F3880);
}

// 0x5F1B00
bool CPedGeometryAnalyser::CanPedJumpObstacle(const CPed& ped, const CEntity& entity) {
    if (entity.m_nFlags & 0x400000) { // entity can't be jumped over
        return false;
    }

    // Shoot a LOS ray from the ped's position along its forward vector (buildings + objects)
    const CVector startPos = ped.GetPosition();
    const CVector endPos   = startPos + ped.GetForward();

    return CWorld::GetIsLineOfSightClear(startPos, endPos, true, false, false, true, false, false, false);
}

// 0x5F32D0
bool CPedGeometryAnalyser::CanPedJumpObstacle(const CPed& ped, const CEntity& entity, const CVector& contactNormal, const CVector& /* contactPos - not used by the original (stack leftover) */) {
    namespace c = PedGeometryAnalyserConstants;

    if (entity.m_nFlags & 0x400000) { // entity can't be jumped over
        return false;
    }

    const CVector pedPos = ped.GetPosition();
    CVector jumpDir      = ped.GetForward();

    // FUN_0055E730 == SurfaceInfos_c::IsShallowWater -> in shallow water the ped can always jump
    if (g_surfaceInfos.IsShallowWater(ped.m_nContactSurface)) {
        return true;
    }

    if (contactNormal.z <= c::FLT_086C6AC) { // 0.17f - flat contact
        // FUN_005F7F10 == CPedGroups::IsInPlayersGroup - a player-group ped only skips the
        // jumpDir.z adjustment below, it still goes through the LOS / ground checks
        if (!CPedGroups::IsInPlayersGroup(const_cast<CPed*>(&ped))) {
            jumpDir.z -= c::FLT_0858FCC; // 0.15f
        }
    } else {
        if (contactNormal.z > c::FLT_0858C20) { // 0.9f - way too steep a wall
            return false;
        }

        // Stairs/slopes: adjust the jump offset using the ped's first collision sphere
        const auto* colData = CModelInfo::GetModelInfo(ped.m_nModelIndex)->GetColModel()->GetData();
        const auto& sphere  = colData->m_pSpheres[0]; // FUN_005F32D0 reads spheres[0].center.z / .radius directly

        jumpDir.z += sphere.m_vecCenter.z - sphere.m_fRadius * contactNormal.z;

        const float slope = sqrtf(sq(contactNormal.x) + sq(contactNormal.y));

        if (contactNormal.z <= c::FLT_0858B8C) { // 0.5f - shallow slope
            // FUN_0040FEC0 (v * f) x2 + FUN_00411A00 (+=): jumpDir += fwd * slope * radius
            jumpDir += ped.GetForward() * (slope * sphere.m_fRadius);
        } else {
            CVector n2{ -contactNormal.x, -contactNormal.y, 0.0f };
            n2 *= 1.0f / slope;                                   // FUN_00411A30 - scale by 1/slope
            jumpDir += n2 * (slope * sphere.m_fRadius);           // FUN_0040FEC0 x2 + FUN_00411A00
            jumpDir *= std::min(c::FLT_0858CA0 / slope, c::FLT_0858B90); // FUN_0040FEF0 - * min(2/slope, 4)
        }
    }

    const CVector target = pedPos + jumpDir;

    // FUN_0056A490 == CWorld::GetIsLineOfSightClear (buildings + objects)
    if (CWorld::GetIsLineOfSightClear(pedPos, target, true, false, false, true, false, false, false)) {
        // FUN_005696C0 == CWorld::FindGroundZFor3DCoord - probe 3m ahead, ground must not be more than 3m below
        const CVector probe = pedPos + jumpDir * c::FLT_0858B3C; // * 3.0f
        bool hasGround{};
        const float groundZ = CWorld::FindGroundZFor3DCoord(probe, &hasGround, nullptr);
        if (hasGround && probe.z - groundZ < c::FLT_0858B3C) {
            return true;
        }
    }
    return false;
}

// 0x5F1C40
bool CPedGeometryAnalyser::CanPedTargetPed(CPed& ped, CPed& targetPed, bool checkDirection) {
    return CanPedTargetPoint(
        ped,
        targetPed.GetPosition() + CVector{ 0.f, 0.f, targetPed.GetTaskManager().GetTaskSecondary(TASK_SECONDARY_DUCK) ? -0.25f : 0.75f }, // 0.75f - 1.f = -0.25f
        checkDirection
    );
}

// 0x5F1B70
bool CPedGeometryAnalyser::CanPedTargetPoint(const CPed& ped, const CVector& target, bool checkDirection) {
    const CVector pos  = ped.GetPosition();
    const CVector dist = target - pos;

    if (checkDirection) {
        // Reject points behind the ped (dot with forward < 0)
        if (DotProduct(dist, ped.GetForward()) < 0.0f) {
            return false;
        }
    }

    // Max distance check: squared distance vs MAX_DIST^2
    const auto MAX_DIST = PedGeometryAnalyserConstants::FLT_086C69C;
    if (MAX_DIST * MAX_DIST < dist.SquaredMagnitude()) {
        return false;
    }

    // Final LOS test from slightly above the ped's position; buildings & objects
    CVector losStart = ped.GetPosition();
    losStart.z += PedGeometryAnalyserConstants::FLT_0858F34;
    return CWorld::GetIsLineOfSightClear(losStart, target, true, false, false, true, false, true, false);
}

// 0x5F1E30
// unused
int32 CPedGeometryAnalyser::ComputeBuildingHitPoints(const CVector& start, const CVector& end) {
    CEntity* outEntity{};
    CColPoint colPoint{};

    // buildings only
    CWorld::ProcessLineOfSight(start, end, colPoint, outEntity, true, false, false, false, true, false, false, false);
    return CWorld::ms_iProcessLineNumCrossings;
}

// 0x5F5D80
void CPedGeometryAnalyser::ComputeClearTarget(const CPed& ped, const CVector& target, CVector& out) {
    out = target;

    const CVector pedPos = ped.GetPosition();

    const auto pushTargetAway = [&](CVector& targetRef, float losDist) {
        CVector dir    = targetRef - pedPos;
        dir.Normalise();
        const float amount = PedGeometryAnalyserConstants::FLT_08D22B0 + losDist;
        targetRef -= dir * amount;
    };

    // Two scanner entity lists of the ped's intelligence (16 entries each)
    for (CEntity** list : { ped.GetIntelligence()->GetPedEntities(),
                            ped.GetIntelligence()->GetVehicleEntities() }) {
        for (int i = 0; i < 16; i++) {
            CEntity* entity = list[i];
            if (!entity) {
                continue;
            }
            const CVector entityPos = entity->GetPosition();

            // Only entities near the current target matter
            if ((entityPos - out).SquaredMagnitude() >= PedGeometryAnalyserConstants::FLT_086C6A4 * PedGeometryAnalyserConstants::FLT_086C6A4) {
                continue;
            }
            if (!LiesInsideBoundingBox(ped, out, *entity)) {
                continue;
            }
            // Returns the blocked distance
            float losDist{};
            if (!GetIsLineOfSightClear(ped, out, *entity, losDist)) {
                continue;
            }
            pushTargetAway(out, losDist);
        }
    }

    // Finally push the target away from the ped itself until it is far enough,
    // using a line-crossing test as the stopping criterion
    CVector dir = pedPos - out;
    dir.Normalise();
    const CVector step = dir * PedGeometryAnalyserConstants::FLT_08D22B0;

    // 0x821B40 is the binary's rand() thunk (see ProjectileInfo.cpp); caps the number of iterations
    const int32 maxIter = plugin::CallAndReturn<int32, 0x821B40>() + 1;

    for (int32 iter = 0; iter < maxIter + 1; iter++) {
        if ((pedPos - out).SquaredMagnitude() >= PedGeometryAnalyserConstants::FLT_086C6A4 * PedGeometryAnalyserConstants::FLT_086C6A4) {
            return; // far enough
        }
        if (DotProduct(pedPos - out, step) < 0.0f) {
            return; // would move towards the ped
        }

        CColPoint colPoint{};
        CEntity*  hitEntity{};
        CWorld::ProcessLineOfSight(pedPos, out, colPoint, hitEntity, true, false, false, false, true, false, false, false);

        if ((CWorld::ms_iProcessLineNumCrossings & 0x80000001) != 1) { // even number of crossings
            break;
        }
        out += step; // still blocked by an odd number of obstacles -> step further away
    }
}

// 0x5F3B70
bool CPedGeometryAnalyser::ComputeClosestSurfacePoint(const CPed& ped, CEntity& entity, CVector& point) {
    CVector corners[4];
    const auto& posn = ped.GetPosition();
    ComputeEntityBoundingBoxCornersUncached(posn.z, entity, corners);
    return ComputeClosestSurfacePoint(posn, corners, point);
}

// 0x5F36F0
bool CPedGeometryAnalyser::ComputeClosestSurfacePoint(const CVector& posn, CEntity& entity, CVector& point) {
    CVector corners[4];
    ComputeEntityBoundingBoxCornersUncached(posn.z, entity, corners);
    return ComputeClosestSurfacePoint(posn, corners, point);
}

// 0x5F2C10
bool CPedGeometryAnalyser::ComputeClosestSurfacePoint(const CVector& posn, const CVector* corners, CVector& point) {
    bool  found      = false;
    float bestDistSq = StaticRef<float>(0x863A3C); // FLT_MAX-ish initial best

    // Check each of the 4 edges (corner[i] -> corner[(i+1) % 4]) for the closest point
    for (int i = 0; i < 4; i++) {
        const int j = (i + 1) % 4;
        const CVector& c1 = corners[i];
        const CVector& c2 = corners[j];

        const CVector edge = c2 - c1;
        const float   len  = edge.Magnitude();
        const float   inv  = PedGeometryAnalyserConstants::FLT_0858624 / len; // 1.0f / len - normalise

        // Project posn onto the edge: t in [0, len]
        const float t = DotProduct(posn - c1, edge) * inv;

        if (t >= 0.0f && t < len) {
            const CVector closest = c1 + edge * inv * t;
            const float   distSq  = (posn - closest).SquaredMagnitude();
            if (distSq < bestDistSq) {
                point      = closest;
                bestDistSq = distSq;
                found      = true;
            }
        }
    }

    // No edge was "hit" -> fall back to the closest corner vertex
    if (!found) {
        for (int i = 0; i < 4; i++) {
            const float distSq = (corners[i] - posn).SquaredMagnitude();
            if (distSq < bestDistSq) {
                point      = corners[i];
                bestDistSq = distSq;
                found      = true;
            }
        }
    }
    return found;
}

// inlined into CPedGeometryAnalyser::ComputeEntityBoundingSphere
void CPedGeometryAnalyser::ComputeEntityBoundingBoxCentre(float zPos, CEntity& entity, CVector& center) {
    ComputeEntityBoundingBoxCentreUncachedAll(zPos, entity, center);
}

// 0x5F1600
void CPedGeometryAnalyser::ComputeEntityBoundingBoxCentreUncached(float zPos, const CVector* corners, CVector& center) {
    center.Set(0.0f, 0.0f, zPos);

    center.x = corners[0].x;
    center.y = corners[0].y;

    center.x += corners[1].x;
    center.y += corners[1].y;

    center.x += corners[2].x;
    center.y += corners[2].y;

    center.x += corners[3].x;
    center.y += corners[3].y;

    center.x *= 0.25f;
    center.y *= 0.25f;
}

// 0x5F3B40
void CPedGeometryAnalyser::ComputeEntityBoundingBoxCentreUncachedAll(float zPos, CEntity& entity, CVector& center) {
    CVector corners[4];
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners);
    ComputeEntityBoundingBoxCentreUncached(zPos, corners, center);
}

// 0x5F3650
void CPedGeometryAnalyser::ComputeEntityBoundingBoxCorners(float zPos, CEntity& entity, CVector* corners) {
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners);
}

// 0x5F1FA0
void CPedGeometryAnalyser::ComputeEntityBoundingBoxCornersUncached(float zPos, CEntity& entity, CVector* corners) {
    namespace c = PedGeometryAnalyserConstants;
    constexpr float FLT_MAXV = std::numeric_limits<float>::max();

    // Fast path: interior buildings (flag 0x400000) -> use the interior manager's furniture bounding box.
    // FUN_00598090 == InteriorManager_c::GetBoundingBox
    if (entity.GetType() == ENTITY_TYPE_BUILDING
        && (entity.m_nFlags & 0x400000)
        && g_interiorMan.GetBoundingBox(reinterpret_cast<FurnitureEntity_c*>(&entity), corners)) {
        for (auto i = 0; i < 4; i++) {
            corners[i].z = zPos;
        }
        return;
    }

    // Make sure the entity has a world matrix (FUN_0054F560 / FUN_0054F1B0)
    if (!entity.m_matrix) {
        entity.AllocateMatrix();
        entity.m_placement.UpdateMatrix(entity.m_matrix);
    }

    auto* colModel = CModelInfo::GetModelInfo(entity.m_nModelIndex)->GetColModel();
    const auto* colData  = colModel ? colModel->GetData() : nullptr;

    CVector boxMin, boxMax;
    const bool bUseRawBox = entity.GetType() != ENTITY_TYPE_OBJECT
        || !colData
        || colModel->GetBoundingBox().m_vecMax.z - colModel->GetBoundingBox().m_vecMin.z <= c::FLT_086C6A8 // 6.0f
        || (colData->m_nNumSpheres == 0 && colData->m_nNumBoxes == 0);

    if (bUseRawBox) {
        boxMin = colModel->GetBoundingBox().m_vecMin;
        boxMax = colModel->GetBoundingBox().m_vecMax;
    } else {
        // Tight (local space) AABB of the entity's collision geometry that overlaps the ped's z slab
        const float zLo = zPos - c::FLT_0858624; // 1.0f
        const float zHi = zPos + c::FLT_0858624;

        boxMin = CVector{ FLT_MAXV, FLT_MAXV, FLT_MAXV };
        boxMax = CVector{ -FLT_MAXV, -FLT_MAXV, -FLT_MAXV };

        const auto& mat = *entity.m_matrix;

        for (const auto& box : std::span{ colData->m_pBoxes, (size_t)colData->m_nNumBoxes }) {
            CVector wMin, wMax;
            wMin.FromMultiply(mat, box.m_vecMin); // FUN_0059C670 - z only used for the overlap test
            wMax.FromMultiply(mat, box.m_vecMax);
            if ((zLo <= wMin.z || zLo <= wMax.z) && (wMin.z <= zHi || wMax.z <= zHi)) {
                boxMin.x = std::min(boxMin.x, box.m_vecMin.x);
                boxMin.y = std::min(boxMin.y, box.m_vecMin.y);
                boxMin.z = std::min(boxMin.z, box.m_vecMin.z);
                boxMax.x = std::max(boxMax.x, box.m_vecMax.x);
                boxMax.y = std::max(boxMax.y, box.m_vecMax.y);
                boxMax.z = std::max(boxMax.z, box.m_vecMax.z);
            }
        }

        for (const auto& sphere : std::span{ colData->m_pSpheres, (size_t)colData->m_nNumSpheres }) {
            const CVector locMin = sphere.m_vecCenter - CVector{ sphere.m_fRadius, sphere.m_fRadius, sphere.m_fRadius };
            const CVector locMax = sphere.m_vecCenter + CVector{ sphere.m_fRadius, sphere.m_fRadius, sphere.m_fRadius };
            CVector wMin, wMax;
            wMin.FromMultiply(mat, locMin);
            wMax.FromMultiply(mat, locMax);
            if ((zLo <= wMin.z || zLo <= wMax.z) && (wMin.z <= zHi || wMax.z <= zHi)) {
                boxMin.x = std::min(boxMin.x, locMin.x);
                boxMin.y = std::min(boxMin.y, locMin.y);
                boxMin.z = std::min(boxMin.z, locMin.z);
                boxMax.x = std::max(boxMax.x, locMax.x);
                boxMax.y = std::max(boxMax.y, locMax.y);
                boxMax.z = std::max(boxMax.z, locMax.z);
            }
        }
    }

    // Shrink the box by the global margin, then convert to centre + (negative) half extents
    const auto& mat    = *entity.m_matrix;
    const float margin = c::FLT_08D22B0;
    const float half   = c::FLT_0858B8C; // 0.5f

    const CVector negHalfExt{ // ((margin + min) - (max - margin)) * 0.5f == -(halfExtent - margin)
        ((margin + boxMin.x) - (boxMax.x - margin)) * half,
        ((margin + boxMin.y) - (boxMax.y - margin)) * half,
        ((margin + boxMin.z) - (boxMax.z - margin)) * half,
    };
    CVector centreLocal{ // ((margin + min) + (max - margin)) * 0.5f == (min + max) * 0.5f
        (margin + boxMin.x + (boxMax.x - margin)) * half,
        (margin + boxMin.y + (boxMax.y - margin)) * half,
        (margin + boxMin.z + (boxMax.z - margin)) * half,
    };
    CVector centre;
    centre.FromMultiply(mat, centreLocal); // FUN_0059C670

    // Squared xy lengths of the matrix basis vectors
    const float rightXYSq = sq(mat.GetRight().x) + sq(mat.GetRight().y);
    const float fwdXYSq   = sq(mat.GetForward().x) + sq(mat.GetForward().y);
    const float upXYSq    = sq(mat.GetUp().x) + sq(mat.GetUp().y);

    // Extent along each world axis pair
    const float extX = negHalfExt.x * rightXYSq * 2.0f;
    const float extY = negHalfExt.y * fwdXYSq * 2.0f;
    const float extZ = negHalfExt.z * upXYSq * 2.0f;

    // Pick the thinnest axis: its (flattened) direction becomes the base axis of the corner quad
    CVector axis;
    float   extSelf; // half extent along the chosen axis
    float   dotA1, crossA1, dotA2, crossA2;
    if (extX <= extY || extX <= extZ) {
        if (extY <= extZ) { // up is the thinnest -> axis = up, ext = z half extent
            axis    = CVector{ mat.GetUp().x, mat.GetUp().y, 0.0f };
            extSelf = negHalfExt.z;
            dotA1   = (axis.x * mat.GetRight().x + axis.y * mat.GetRight().y) * negHalfExt.x;
            crossA1 = (axis.x * mat.GetRight().y - axis.y * mat.GetRight().x) * negHalfExt.x;
            dotA2   = (axis.x * mat.GetForward().x + axis.y * mat.GetForward().y) * negHalfExt.y;
            crossA2 = (axis.x * mat.GetForward().y - axis.y * mat.GetForward().x) * negHalfExt.y;
        } else { // forward is the thinnest -> axis = forward, ext = y half extent
            axis    = CVector{ mat.GetForward().x, mat.GetForward().y, 0.0f };
            extSelf = negHalfExt.y;
            dotA1   = (axis.x * mat.GetRight().x + axis.y * mat.GetRight().y) * negHalfExt.x;
            crossA1 = (axis.x * mat.GetRight().y - axis.y * mat.GetRight().x) * negHalfExt.x;
            dotA2   = (axis.x * mat.GetUp().x + axis.y * mat.GetUp().y) * negHalfExt.z;
            crossA2 = (axis.x * mat.GetUp().y - axis.y * mat.GetUp().x) * negHalfExt.z;
        }
    } else { // right is the thinnest -> axis = right, ext = x half extent
        axis    = CVector{ mat.GetRight().x, mat.GetRight().y, 0.0f };
        extSelf = negHalfExt.x;
        dotA1   = (axis.x * mat.GetForward().x + axis.y * mat.GetForward().y) * negHalfExt.y;
        crossA1 = (axis.x * mat.GetForward().y - axis.y * mat.GetForward().x) * negHalfExt.y;
        dotA2   = (axis.x * mat.GetUp().x + axis.y * mat.GetUp().y) * negHalfExt.z;
        crossA2 = (axis.x * mat.GetUp().y - axis.y * mat.GetUp().x) * negHalfExt.z;
    }

    // The two inner corner points (centre +/- axis * extSelf), z offset by half the extent
    const float halfSelf = extSelf * c::FLT_0858B50; // * 0.0f in the original (DAT_00858B50 == 0.0f)
    const CVector p1{ centre.x + axis.x * extSelf, centre.y + axis.y * extSelf, centre.z + halfSelf };
    const CVector p2{ centre.x - axis.x * extSelf, centre.y - axis.y * extSelf, centre.z - halfSelf };

    axis.Normalise(); // FUN_0059C910 - normalise the chosen axis (the dots below use the normalised one)

    // Projected extents of the two remaining axes onto the (normalised) axis - abs'ed and summed
    if (dotA1 < c::FLT_0858B50) dotA1 = -dotA1;   // fabsf
    if (dotA2 < c::FLT_0858B50) dotA2 = -dotA2;
    const float aSum = dotA1 + dotA2;
    if (crossA1 < c::FLT_0858B50) crossA1 = -crossA1;
    if (crossA2 < c::FLT_0858B50) crossA2 = -crossA2;
    const float bSum = crossA1 + crossA2;

    // World space offset vectors A and B (B rotated 90 deg from the axis)
    const CVector vecA{ aSum * axis.x, aSum * axis.y, 0.0f };
    const CVector vecB{ bSum * axis.y, -bSum * axis.x, bSum * half };

    // The 4 corners (all flattened to the ped's height, as in the original)
    corners[0] = CVector{ (p1 + vecA - vecB).x, (p1 + vecA - vecB).y, zPos };
    corners[1] = CVector{ (p2 - vecA - vecB).x, (p2 - vecA - vecB).y, zPos };
    corners[2] = CVector{ (p2 - vecA + vecB).x, (p2 - vecA + vecB).y, zPos };
    corners[3] = CVector{ (p1 + vecA + vecB).x, (p1 + vecA + vecB).y, zPos };
}

// 0x5F3660
void CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanes(float zPos, CEntity& entity, CVector(*outPlanes)[4], float* outPlanesDot) {
    ComputeEntityBoundingBoxPlanesUncachedAll(zPos, entity, outPlanes, outPlanesDot);
}

// 0x5F1670
void CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanesUncached(float zPos, const CVector* corners, CVector(*outPlanes)[4], float* outPlanesDot) {
    const CVector* corner2 = &corners[3];
    for (auto i = 0; i < 4; i++) {
        const CVector& corner = corners[i];
        CVector& plane = (*outPlanes)[i];
        CVector direction = corner - *corner2;
        direction.Normalise();
        plane.x = direction.y;
        plane.y = -direction.x;
        plane.z = 0.0f;
        // point-normal plane equation:
        // ax + by + cz + d = 0
        // d = - n . P
        outPlanesDot[i] = -DotProduct(plane, *corner2);

        corner2 = &corner;
    }
}

// 0x5F2B80
void CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanesUncachedAll(float zPos, CEntity& entity, CVector (*outPlanes)[4], float* outPlanesDot) {
    CVector corners[4];
    CPedGeometryAnalyser::ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners);
    CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanesUncached(zPos, corners, outPlanes, outPlanesDot);
}

// 0x5F36A0
void CPedGeometryAnalyser::ComputeEntityBoundingBoxSegmentPlanes(float zPos, CEntity& entity, CVector* normals, float* dots) {
    ComputeEntityBoundingBoxSegmentPlanesUncachedAll(zPos, entity, normals, dots);
}

// 0x5F1750
CVector* CPedGeometryAnalyser::ComputeEntityBoundingBoxSegmentPlanesUncached(const CVector* corners, CVector& center, CVector* outNormals, float* outDots) {
    for (int i = 0; i < 4; i++) {
        const CVector& c = corners[i];
        // 2D left-normal of (corner - center), z = 0
        outNormals[i] = CVector{ -(c.y - center.y), c.x - center.x, 0.0f };
        // Point-normal plane: d = - n . P
        outDots[i] = -DotProduct(outNormals[i], c);
    }
    return outNormals;
}

// 0x5F2BC0
CVector* CPedGeometryAnalyser::ComputeEntityBoundingBoxSegmentPlanesUncachedAll(float zPos, CEntity& entity, CVector* a3, float* a4) {
    CVector corners[4];
    CVector center;

    ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners);
    ComputeEntityBoundingBoxCentreUncached(zPos, corners, center);
    return ComputeEntityBoundingBoxSegmentPlanesUncached(corners, center, a3, a4);
}

// 0x5F3C20
void CPedGeometryAnalyser::ComputeEntityBoundingSphere(const CPed& ped, CEntity& entity, CColSphere& out) {
    const CVector pedPos = ped.GetPosition();

    CVector corners[4];
    ComputeEntityBoundingBoxCornersUncached(pedPos.z, entity, corners);

    CVector cornersB[4];
    ComputeEntityBoundingBoxCornersUncached(pedPos.z, entity, cornersB);

    CVector centre;
    ComputeEntityBoundingBoxCentreUncached(pedPos.z, cornersB, centre);

    // Radius = furthest corner from the centre
    float maxDistSq = 0.0f;
    for (int i = 0; i < 4; i++) {
        maxDistSq = std::max(maxDistSq, (corners[i] - centre).SquaredMagnitude());
    }

    out.Set(sqrtf(maxDistSq) * PedGeometryAnalyserConstants::FLT_0858F14, centre, SURFACE_DEFAULT, 0, tColLighting{ 0xFF });
}

// 0x5F3730
// NOTE: Faithful port - returns void (like the original), the plane distances
// are computed inline (no DotProduct calls) and the direction is copied
// component-wise (no CVector::operator= calls).
void CPedGeometryAnalyser::ComputeMoveDirToAvoidEntity(const CPed& ped, CEntity& entity, CVector& outDir) {
    // NOTE: manual GetPosition (the accessor compiles to an out-of-line call)
    const CVector& pedPos = ped.m_matrix
        ? *reinterpret_cast<const CVector*>(reinterpret_cast<const uint8*>(ped.m_matrix) + 0x30)
        : ped.m_placement.m_vPosn;

    CVector corners[4];
    CVector planes[4];
    float   planeDots[4];

    ComputeEntityBoundingBoxCornersUncached(pedPos.z, entity, corners);
    ComputeEntityBoundingBoxPlanesUncached(pedPos.z, corners, &planes, planeDots);

    // Distances of the ped to planes 0 and 1
    const float d0 = planes[0].x * pedPos.x + planes[0].y * pedPos.y + planes[0].z * pedPos.z + planeDots[0];
    const float d1 = planes[1].x * pedPos.x + planes[1].y * pedPos.y + planes[1].z * pedPos.z + planeDots[1];

    // Pick the plane normal the ped is "less inside" of
    if (d0 > FLT_0_858B50) {
        outDir.x = planes[0].x;
        outDir.y = planes[0].y;
        outDir.z = planes[0].z;
        return;
    }
    if (d1 > FLT_0_858B50) {
        outDir.x = planes[1].x;
        outDir.y = planes[1].y;
        outDir.z = planes[1].z;
        return;
    }
    const auto& dir = d1 < d0 ? planes[0] : planes[1];
    outDir.x = dir.x;
    outDir.y = dir.y;
    outDir.z = dir.z;
}

//! @notsa
CVector CPedGeometryAnalyser::ComputeEntityDir(const CEntity& entity, eDirection dir) {
    switch (dir) {
    case eDirection::FORWARD:  return entity.GetForward();
    case eDirection::LEFT:     return -entity.GetRight();
    case eDirection::BACKWARD: return -entity.GetForward();
    case eDirection::RIGHT:    return entity.GetRight();
    default:                   NOTSA_UNREACHABLE();
    }
}

// 0x5F1500
// NOTE: Instruction-level port of the original (x87 FPU code with inline
// fsin/fcos - hence the inline assembly). The direction vectors are negated
// by multiplying with -1.0f (0x858C1C) and the outputs are written
// component-wise, exactly like the original.
__declspec(naked) CVector* CPedGeometryAnalyser::ComputeEntityDirs(const CEntity& /*entity*/, CVector* /*outDirs*/) {
    __asm {
        mov ecx, dword ptr [esp + 4]            ; &entity
        mov eax, dword ptr [ecx + 0x14]         ; entity.m_matrix
        sub esp, 0x18
        test eax, eax
        je no_matrix1
        push esi
        lea edx, [eax + 0x10]                   ; &m_forward
        mov esi, dword ptr [edx]
        mov dword ptr [esp + 4], esi
        mov esi, dword ptr [edx + 4]
        mov edx, dword ptr [edx + 8]
        mov dword ptr [esp + 8], esi
        mov dword ptr [esp + 0xC], edx
        pop esi
        jmp have_fwd
no_matrix1:
        fld dword ptr [ecx + 0x10]              ; heading
        mov dword ptr [esp + 8], 0
        fsin
        fchs
        fstp dword ptr [esp]                    ; -sin(heading)
        fld dword ptr [ecx + 0x10]
        fcos
        fstp dword ptr [esp + 4]                ; cos(heading)
have_fwd:
        test eax, eax
        je no_matrix2
        mov ecx, dword ptr [eax]                ; m_right
        mov edx, dword ptr [eax + 4]
        mov eax, dword ptr [eax + 8]
        mov dword ptr [esp + 0xC], ecx
        mov dword ptr [esp + 0x10], edx
        mov dword ptr [esp + 0x14], eax
        jmp have_right
no_matrix2:
        fld dword ptr [ecx + 0x10]              ; heading
        mov dword ptr [esp + 0x14], 0
        fcos
        fstp dword ptr [esp + 0xC]              ; cos(heading)
        fld dword ptr [ecx + 0x10]
        fsin
        fstp dword ptr [esp + 0x10]             ; sin(heading)
have_right:
        mov eax, dword ptr [esp + 0x20]         ; outDirs
        fld dword ptr [esp + 0xC]
        fmul FLT_M1_858C1C
        mov edx, dword ptr [esp]
        mov ecx, eax
        mov dword ptr [ecx], edx                ; out[0] = fwd.x
        mov edx, dword ptr [esp + 4]
        mov dword ptr [ecx + 4], edx            ; out[1] = fwd.y
        mov edx, dword ptr [esp + 8]
        mov dword ptr [ecx + 8], edx            ; out[2] = fwd.z
        fstp dword ptr [eax + 0xC]              ; out[3] = right.x * -1
        fld dword ptr [esp + 0x10]
        mov ecx, dword ptr [esp + 0xC]
        fmul FLT_M1_858C1C
        mov edx, dword ptr [esp + 0x10]
        add eax, 0x24
        fstp dword ptr [eax - 0x14]             ; out[4] = right.y * -1
        fld dword ptr [esp + 0x14]
        fmul FLT_M1_858C1C
        fstp dword ptr [eax - 0x10]             ; out[5] = right.z * -1
        fld dword ptr [esp]
        fmul FLT_M1_858C1C
        fstp dword ptr [eax - 0xC]              ; out[6] = fwd.x * -1
        fld dword ptr [esp + 4]
        fmul FLT_M1_858C1C
        fstp dword ptr [eax - 8]                ; out[7] = fwd.y * -1
        fld dword ptr [esp + 8]
        fmul FLT_M1_858C1C
        fstp dword ptr [eax - 4]                ; out[8] = fwd.z * -1
        mov dword ptr [eax], ecx                ; out[9]  = right.x
        mov ecx, dword ptr [esp + 0x14]
        mov dword ptr [eax + 4], edx            ; out[10] = right.y
        mov dword ptr [eax + 8], ecx            ; out[11] = right.z
        add esp, 0x18
        ret
    }
}

// 0x5F3BC0
int32 CPedGeometryAnalyser::ComputeEntityHitSide(const CPed& ped, CEntity& entity) {
    return ComputeEntityHitSide(ped.GetPosition(), entity);
}

// 0x5F1450
int32 CPedGeometryAnalyser::ComputeEntityHitSide(const CVector& point, const CVector* planes, const float* planeDots) {
    // Find a pair of consecutive planes (i, i+1 mod 4) whose signs differ:
    // d(i) >= 0 and d(i+1) < 0  ->  the point crossed plane i into i+1
    for (int i = 0; i < 4; i++) {
        const int j = (i + 1) % 4;

        const float d1 = DotProduct(point, planes[i]) + planeDots[i];
        const float d2 = DotProduct(point, planes[j]) + planeDots[j];

        if (d1 >= 0.0f && d2 < 0.0f) {
            return j;
        }
    }
    return 0;
}

// 0x5F3AC0
int32 CPedGeometryAnalyser::ComputeEntityHitSide(const CVector& point, CEntity& entity) {
    CVector corners[4];
    CVector centre;
    CVector planes[4];
    float   planeDots[4];

    ComputeEntityBoundingBoxCornersUncached(point.z, entity, corners);
    ComputeEntityBoundingBoxCentreUncached(point.z, corners, centre);
    ComputeEntityBoundingBoxSegmentPlanesUncached(corners, centre, planes, planeDots);

    return ComputeEntityHitSide(point, planes, planeDots);
}

// 0x5F3640
int32 CPedGeometryAnalyser::ComputePedHitSide(const CPed& ped, const CPhysical& physical) {
    return ComputePedHitSide(ped, physical.m_vecMoveSpeed);
}

// 0x5F1E70
// NOTE: Faithful port of the original max-dot scan (decompiled at 0x5F1E70).
// The original scans dirs[0..3] for the maximum dot with -posn (normalised):
//   best = raw bits of d0 (original quirk - only observable if every dot is < -1.0f),
//   bestDot = -1.0f;  if (-1.0f <= d) { best = i; bestDot = d; }  for i = 0..2;
//   return (d3 < bestDot) ? best : 3;
// The previous inline-asm port of this deviated from the original in 3 ways:
//   - it returned 4 (not 3) when direction #3 won the scan,
//   - its `no0` path reused EAX (the `dirs` base pointer) to hold the raw d0 bits,
//     so the next `fld [eax+0xC]` read through a wild pointer (the original keeps
//     addressing the local `dirs` array and only stores the bits into `best`),
//   - it returned with one value still on the x87 stack every call: after 8 calls
//     the x87 register stack is full and every subsequent FPU computation in the
//     process yields indeterminate values (a process-wide float corruptor).
int32 CPedGeometryAnalyser::ComputePedHitSide(const CPed& ped, const CVector& posn) {
    CVector v; // * -1.0f (0x858C1C), component-wise
    v.x = posn.x * FLT_M1_858C1C;
    v.y = posn.y * FLT_M1_858C1C;
    v.z = posn.z * FLT_M1_858C1C;
    v.Normalise(); // 0x59C910

    CVector dirs[4]; // forward, left, backward, right
    ComputeEntityDirs(ped, dirs); // 0x5F1500

    const float d0 = dirs[0].x * v.x + dirs[0].y * v.y + dirs[0].z * v.z;
    int32 best;
    std::memcpy(&best, &d0, sizeof(best)); // original quirk: `best = (int)d0` (raw bits)
    float bestDot = FLT_M1_858C1C;         // -1.0f
    if (FLT_M1_858C1C <= d0) {
        best = 0;
        bestDot = d0;
    }

    const float d1 = dirs[1].x * v.x + dirs[1].y * v.y + dirs[1].z * v.z;
    if (bestDot <= d1) {
        best = 1;
        bestDot = d1;
    }

    const float d2 = dirs[2].x * v.x + dirs[2].y * v.y + dirs[2].z * v.z;
    if (bestDot <= d2) {
        best = 2;
        bestDot = d2;
    }

    const float d3 = dirs[3].x * v.x + dirs[3].y * v.y + dirs[3].z * v.z;
    return d3 < bestDot ? best : 3;
}

// 0x5F13F0
// NOTE: Written as a faithful instruction-level port of the original (x87 FPU code),
// hence the inline assembly. Original .rdata constants: π/4 0x859AB0, 0.0 0x858B50,
// 2π 0x858CBC, 2/π 0x858FB8 (kept as our own storage so they compile to memory operands).
namespace {
    const float FLT_PI_4       = 0.7853982f; // 0x859AB0
    const float FLT_ZERO_858B5 = 0.0f;       // 0x858B50
    const float FLT_TWO_PI     = 6.283185f;  // 0x858CBC
    const float FLT_TWO_OVER_PI = 0.6366197f; // 0x858FB8
}

// Mimics the original's CRT float->int rounder (0x821B40) - see ComputePedShotSide.
static __declspec(naked) int32 FpuRoundToInt() {
    __asm {
        sub esp, 4
        fistp dword ptr [esp]
        pop eax
        ret
    }
}

int32 CPedGeometryAnalyser::ComputePedShotSide(const CPed& ped, const CVector& posn) {
    __asm {
        mov edx, ped                       ; edx = &ped
        mov eax, dword ptr [edx + 0x14]    ; ped.m_matrix
        test eax, eax
        push esi
        lea esi, [eax + 0x30]              ; esi = &m_matrix->pos
        jne have_pos
        lea esi, [edx + 4]                 ; esi = &m_placement.m_vPosn
have_pos:
        mov eax, posn                      ; eax = &posn
        fld dword ptr [eax]
        fsub dword ptr [esi]               ; posn.x - pedpos.x
        fchs                               ; -(dx)
        fld dword ptr [eax + 4]
        fsub dword ptr [esi + 4]           ; dy
        pop esi
        fpatan                             ; atan2(-(dx), dy) - cw-from-+Y heading convention
        fsub dword ptr [edx + 0x558]       ; - ped.m_fCurrentRotation
        fadd FLT_PI_4
        fcom FLT_ZERO_858B5
        fnstsw ax
        test ah, 5
        jp no_wrap
        fadd FLT_TWO_PI                    ; wrap into [0, 2π)
no_wrap:
        fmul FLT_TWO_OVER_PI               ; scale to quadrant count -> [0, 4)
        jmp FpuRoundToInt                  ; tail-jump into the rounder, like the original
    }
    return 0;
}

// 0x5F6110
int32 CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox(const CPed& ped, CEntity& entity, const CVector& posn, CPointRoute& pointRoute, int32 a5) {
    return ComputeRouteRoundEntityBoundingBox(ped, ped.GetPosition(), entity, posn, pointRoute, a5);
}

// 0x5F3DD0
int32 CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox(const CPed& ped, const CVector& start, CEntity& entity, const CVector& end, CPointRoute& route, int32 mode) {
    namespace c = PedGeometryAnalyserConstants;
    static const auto ZERO = c::FLT_0858B50; // 0.0f (DAT_00858B50) - static so lambdas can use it without capturing

    route.Clear(); // *route = 0

    // Temporarily set the global margin/slope factor to 0.17f (bit pattern 0x3E333333)
    float  savedMargin = c::FLT_08D22B0;
    c::FLT_08D22B0 = 0.17f;
    struct MarginRestore {
        float& r;
        float  v;
        ~MarginRestore() { r = v; }
    } restoreMargin{ c::FLT_08D22B0, savedMargin };

    const auto entityPos = entity.GetPosition();
    const float zPos     = entityPos.z; // the original uses the ped's z here (its matrix pos / placement pos)

    CVector cornersA[4];
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, cornersA); // FUN_005F1FA0 (x2 in the original)
    CVector cornersB[4];
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, cornersB);

    CVector planes[4];
    float   planeDots[4];
    ComputeEntityBoundingBoxPlanesUncached(zPos, cornersB, &planes, planeDots); // FUN_005F1670

    // If both endpoints are on the "outside" of the same plane -> no route needed
    for (int i = 0; i < 4; i++) {
        if (DotProduct(start, planes[i]) + planeDots[i] > ZERO
         && DotProduct(end, planes[i]) + planeDots[i] > ZERO) {
            return 0;
        }
    }

    // Compute the segment planes through the corner centre and make sure both
    // endpoints are on different sides of the box
    {
        CVector corners[4];
        ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners); // FUN_005F1FA0

        CVector centre; // average of the 4 corners (* 0.25f)
        centre.x = (corners[0].x + corners[1].x + corners[2].x + corners[3].x) * c::FLT_0858C84;
        centre.y = (corners[0].y + corners[1].y + corners[2].y + corners[3].y) * c::FLT_0858C84;
        centre.z = 0.0f;
        CVector normals[4];
        float   dots[4];
        ComputeEntityBoundingBoxSegmentPlanesUncached(corners, centre, normals, dots);       // FUN_005F1750
        if (ComputeEntityHitSide(start, normals, dots) == ComputeEntityHitSide(end, normals, dots)) { // FUN_005F1450
            return 0;
        }
    }

    // Redo the corners/planes computation (as the original does)
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, cornersA); // FUN_005F1FA0
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, cornersB);
    ComputeEntityBoundingBoxPlanesUncached(zPos, cornersB, &planes, planeDots); // FUN_005F1670

    // Clip both endpoints against the 4 planes, keeping them within the [-0.2f, +0.2f] margin band
    CVector pStart = start;
    CVector pEnd   = end;
    const CVector segDir = (end - start).Normalized(); // FUN_0059C910 - normalised start->end
    bool bTouchedPlane = false;

    const auto classify = [](float d) -> int { // -1: below the band, 0: within, 1: above
        if (d <= c::FLT_0858CC4) {             // +0.2f
            return d >= c::FLT_0858FDC ? 0 : -1; // -0.2f
        }
        return 1;
    };

    for (int i = 0; i < 4; i++) {
        const float dStart = DotProduct(pStart, planes[i]) + planeDots[i];
        const float dEnd   = DotProduct(pEnd, planes[i]) + planeDots[i];

        const int s = classify(dStart);
        const int e = classify(dEnd);

        if (s == -1) {
            if (e == 1) { // clip end back onto the plane (relative to the original start!)
                const float t = -DotProduct(start, planes[i]) / DotProduct(segDir, planes[i]);
                pEnd = start + segDir * t;
            }
        } else if (e == -1) {
            if (s == 1) { // clip start forward onto the plane
                const float t = -DotProduct(start, planes[i]) / DotProduct(segDir, planes[i]);
                pStart = start + segDir * t;
            }
        } else {
            bTouchedPlane = true;
            if (s == 0) { // push start deeper into the band
                pStart += planes[i] * (c::FLT_0858CC4 - dStart);
            }
            if (e == 0) { // push end deeper into the band
                pEnd += planes[i] * (c::FLT_0858CC4 - dEnd);
            }
        }
    }

    // No plane touched -> use the bounding sphere's ray intersections as the segment
    if (!bTouchedPlane) {
        CColSphere sphere;
        ComputeEntityBoundingSphere(ped, entity, sphere); // FUN_005F3C20

        CVector dir2D{ end.x - start.x, end.y - start.y, 0.0f };
        if (sqrtf(sq(dir2D.x) + sq(dir2D.y)) == ZERO) { // 2D zero-length segment
            return 0;
        }
        dir2D.Normalise(); // FUN_0059C910

        pStart = start;
        pEnd   = end;
        if (!sphere.IntersectRay(start, dir2D, pStart, pEnd)) { // FUN_0040FF20
            return 0;
        }
    }

    // Clip (pStart -> pEnd) against each plane, remembering the entry/exit plane indices
    const CVector seg      = pEnd - pStart;
    const CVector clipDir  = seg.Normalized(); // FUN_0059C910
    const CVector clipBase = pStart;

    int enterPlane = -1;
    int exitPlane  = -1;

    for (int i = 0; i < 4; i++) {
        const float dStart = DotProduct(pStart, planes[i]) + planeDots[i];
        const float dEnd   = DotProduct(pEnd, planes[i]) + planeDots[i];

        if (dStart == ZERO || dEnd == ZERO) {
            continue; // parallel & on plane
        }

        if (dStart < ZERO || ZERO <= dEnd) {
            if (dStart < ZERO && ZERO <= dEnd) { // start in, end out -> exit plane
                exitPlane = i;
                const float t    = -DotProduct(clipBase, planes[i]) / DotProduct(clipDir, planes[i]);
                pEnd = clipBase + clipDir * t;
            }
        } else { // start out, end in -> entry plane
            enterPlane = i;
            const float t    = -DotProduct(clipBase, planes[i]) / DotProduct(clipDir, planes[i]);
            pStart = clipBase + clipDir * t;
        }
    }

    if (exitPlane < 0 || enterPlane < 0) {
        return 0;
    }

    // Walk the box corners between the planes, building 2 candidate paths.
    // Path A walks backwards from (enterPlane + 3) % 4 until (exitPlane + 4) % 4,
    // path B walks forwards from (enterPlane + 4) % 4 until (exitPlane + 3) % 4.
    CVector pathA[9]; int numA = 0;
    CVector pathB[9]; int numB = 0;

    {
        int idx = (enterPlane + 3) % 4;
        const int endIdx = (exitPlane + 4) % 4;
        pathA[numA++] = start;
        pathA[numA++] = cornersA[idx];
        if (idx != endIdx) {
            do {
                idx = (idx + 3) % 4; // -1 mod 4
                pathA[numA++] = cornersA[idx];
            } while (idx != endIdx);
        }
        pathA[numA++] = end;
    }
    {
        int idx = (enterPlane + 4) % 4;
        const int endIdx = (exitPlane + 3) % 4;
        pathB[numB++] = start;
        pathB[numB++] = cornersA[idx];
        if (idx != endIdx) {
            do {
                idx = (idx + 5) % 4; // +1 mod 4
                pathB[numB++] = cornersA[idx];
            } while (idx != endIdx);
        }
        pathB[numB++] = end;
    }

    const CVector centre{ // centre of the (corner) box
        (cornersA[0].x + cornersA[1].x + cornersA[2].x + cornersA[3].x) * c::FLT_0858C84, // * 0.25f
        (cornersA[0].y + cornersA[1].y + cornersA[2].y + cornersA[3].y) * c::FLT_0858C84,
        (cornersA[0].z + cornersA[1].z + cornersA[2].z + cornersA[3].z) * c::FLT_0858C84,
    };

    // Validate a path with line-of-sight checks; returns the last blocking entity
    const auto validatePath = [&](const CVector* path, int num, CEntity*& outHit) -> bool {
        outHit = nullptr;

        CVector running = path[0];
        for (int i = 1; i < num; i++) {
            const CVector& cur = path[i];

            // 2D direction, expanded by half a tile, flipped when pointing away from the box centre
            CVector dir2D{ cur.x - running.x, cur.y - running.y, 0.0f };
            dir2D.Normalise(); // FUN_0059C910
            CVector off{ dir2D.y * c::FLT_0858B8C, -(dir2D.x * c::FLT_0858B8C), 0.0f };
            if ((cur.x - centre.x) * off.x + (cur.y - centre.y) * off.y + (cur.z - centre.z) * ZERO < ZERO) {
                off *= -1.0f; // * -1.0f (FUN_0059C910 scaling by DAT_00858C1C)
            }

            const CVector a = running + off;
            const CVector b = cur + off;

            CColPoint cp{};
            CEntity*  hit{};
            const bool blocked = CWorld::ProcessLineOfSight(running, cur, cp, hit, true, true, true, true, false, false, false, false) // FUN_0056BA00
                              || CWorld::ProcessLineOfSight(a, b, cp, hit, true, true, true, true, false, false, false, false);
            if (blocked) {
                if (hit != &entity && hit != &ped) {
                    // Peds walking in the same direction as us are ignored
                    if (hit->GetType() == ENTITY_TYPE_PED
                        && *reinterpret_cast<const int32*>(reinterpret_cast<const uint8*>(hit) + 0x534) != 1) {
                        if (!hit->m_matrix) {
                            hit->AllocateMatrix();                              // FUN_0054F560
                            hit->m_placement.UpdateMatrix(hit->m_matrix);       // FUN_0054F1B0
                        }
                        if (DotProduct(cur - running, hit->m_matrix->GetForward()) >= ZERO) {
                            running = cur; // ignore this hit
                            continue;
                        }
                    }
                    outHit = hit;
                    return false; // path blocked
                }
                outHit = nullptr;
            }
            running = cur;
        }
        return true;
    };

    const auto pathLength = [](const CVector* path, int num) {
        float len = ZERO;
        for (int i = 0; i < num - 1; i++) {
            len += (path[i + 1] - path[i]).Magnitude();
        }
        return len;
    };

    CEntity* hitA{};
    CEntity* hitB{};
    int32 result;
    if (mode == 0) {
        const bool aValid = validatePath(pathA, numA, hitA);
        const bool bValid = validatePath(pathB, numB, hitB);
        const float lenA = pathLength(pathA, numA);
        const float lenB = pathLength(pathB, numB);

        if (aValid) {
            result = bValid ? (lenA < lenB ? 1 : 2) : 1;
        } else if (bValid) {
            result = 2;
        } else {
            // Both blocked: if the blocking entities differ, prefer the one with the smaller collision radius,
            // otherwise (same entity) prefer the shorter path
            if (numA == 1) {
                if (numB >= 2) {
                    result = 2;
                } else if (hitA == hitB) {
                    result = lenA < lenB ? 1 : 2;
                } else if (CModelInfo::GetModelInfo(hitA->m_nModelIndex)->GetColModel()->m_boundSphere.m_fRadius
                        <  CModelInfo::GetModelInfo(hitB->m_nModelIndex)->GetColModel()->m_boundSphere.m_fRadius) {
                    result = 1;
                } else {
                    result = 2;
                }
            } else {
                if (numB == 1) {
                    result = 1;
                } else if (hitA == hitB) {
                    result = lenA < lenB ? 1 : 2;
                } else if (CModelInfo::GetModelInfo(hitA->m_nModelIndex)->GetColModel()->m_boundSphere.m_fRadius
                        <  CModelInfo::GetModelInfo(hitB->m_nModelIndex)->GetColModel()->m_boundSphere.m_fRadius) {
                    result = 1;
                } else {
                    result = 2;
                }
            }
        }
    } else {
        result = (mode != 1) + 1; // mode 1 -> 1, mode 2+ -> 2
    }

    // Copy the chosen path into the route (max 8 points), dropping the start point
    const CVector* chosen  = result == 1 ? pathA : pathB;
    const int      chosenN = result == 1 ? numA : numB;
    for (int i = 1; i < chosenN - 1; i++) {
        route.AddUnlessFull(chosen[i]);
    }
    route.AddUnlessFull(end); // LAB_005F5801

    // Deduplicate the first route point if it lies on the same side of the box as the start point
    if (route.GetSize() > 1) {
        const auto getSide = [&](const CVector& point) {
            CVector corners[4];
            ComputeEntityBoundingBoxCornersUncached(point.z, entity, corners); // FUN_005F1FA0

            CVector centre2;
            centre2.x = (corners[0].x + corners[1].x + corners[2].x + corners[3].x) * c::FLT_0858C84;
            centre2.y = (corners[0].y + corners[1].y + corners[2].y + corners[3].y) * c::FLT_0858C84;
            centre2.z = 0.0f;
            CVector normals[4];
            float   dots[4];
            ComputeEntityBoundingBoxSegmentPlanesUncached(corners, centre2, normals, dots); // FUN_005F1750
            return ComputeEntityHitSide(point, normals, dots);                              // FUN_005F1450
        };

        if (getSide(start) == getSide(route[1])) {
            for (size_t i = 0; i + 1 < route.GetSize(); i++) {
                route[i] = route[i + 1];
            }
            route.ResizeTo(route.GetSize() - 1);
        }
    }
    route.ResizeTo(route.GetSize() > 0 ? route.GetSize() - 1 : 0); // drop the last point

    return (result != 1) + 1; // returns 1 or 2
}

// 0x5F1890
bool CPedGeometryAnalyser::ComputeRouteRoundSphere(const CPed& ped, const CColSphere& sphere, const CVector& a3, const CVector& a4, CVector& out1, CVector& out2) {
    const CVector pedPos = ped.GetPosition();

    out1 = a4;

    if (sphere.IntersectPoint(a4)) {
        CVector dir = a4 - a3;
        dir.Normalise();

        CVector i1, i2;
        if (sphere.IntersectRay(pedPos, dir, i1, i2)) {
            out1 = i2;
        }
    }

    // Ray from out1 towards the ped; if it misses -> nothing to route around
    CVector dir = out1 - pedPos;
    dir.Normalise();

    CVector i1, i2;
    if (!sphere.IntersectRay(out1, dir, i1, i2)) {
        out2 = out1;
        return false;
    }

    // If out1 is already closer to the ped than the first hit -> nothing to do
    if ((out1 - pedPos).SquaredMagnitude() < (i1 - pedPos).SquaredMagnitude()) {
        out2 = out1;
        return false;
    }

    // Project the sphere centre onto the ray and push the result point onto
    // the sphere surface towards the closest approach point
    if (sphere.IntersectRay(pedPos, dir, i1, i2)) {
        const CVector toCentre = sphere.m_vecCenter - pedPos;
        const float   t        = DotProduct(toCentre, dir);

        CVector delta = pedPos + dir * t - sphere.m_vecCenter; // closest approach -> sphere centre
        delta.Normalise(); // (original scales the delta by 1/|delta| before multiplying with the radius)

        out2 = sphere.m_vecCenter + delta * sphere.m_fRadius;
    }
    return true;
}

// 0x5F5A30
bool CPedGeometryAnalyser::GetIsLineOfSightClear(const CPed& ped, const CVector& target, CEntity& entity, float& outDist) {
    CVector start = ped.GetPosition();
    CVector end   = target;

    CColSphere sphere;
    ComputeEntityBoundingSphere(ped, entity, sphere);

    CVector dir = end - start;
    dir.NormaliseAndMag();

    CVector i1, i2;
    if (!sphere.IntersectRay(start, dir, i1, i2)) {
        return true; // ray doesn't touch the sphere at all
    }

    // Clip the segment against the entity's 4 bounding box planes
    const CVector pedPos = ped.GetPosition();
    CVector corners[4];
    CVector planes[4];
    float   planeDots[4];
    ComputeEntityBoundingBoxCornersUncached(pedPos.z, entity, corners);
    ComputeEntityBoundingBoxPlanesUncached(pedPos.z, corners, &planes, planeDots);

    outDist = 0.0f;

    const auto margin = PedGeometryAnalyserConstants::FLT_08D22B0;
    for (int i = 0; i < 4; i++) {
        const float dStart = DotProduct(start, planes[i]) + planeDots[i];
        const float dEnd   = DotProduct(end,   planes[i]) + planeDots[i];

        const auto sign = [=](float d) -> int {
            if (d <= margin) {
                return d < -margin ? -1 : 0;
            }
            return 1;
        };

        const int s = sign(dStart);
        const int e = sign(dEnd);

        const float den = DotProduct(dir, planes[i]);

        if (s < 0) {
            if (e > 0) { // inside -> outside: pull the end back onto the plane
                if (den > PedGeometryAnalyserConstants::FLT_0858CDC) {
                    const float t = (PedGeometryAnalyserConstants::FLT_0858C1C / den) * dEnd; // -1/den * dEnd
                    end = start + dir * t;
                }
            }
        } else {
            if (e >= 0) {
                return true; // fully outside on this plane -> clear
            }
            if (s > 0) { // outside -> inside: push the start onto the plane
                if (den > PedGeometryAnalyserConstants::FLT_0858CDC) {
                    const float t = (PedGeometryAnalyserConstants::FLT_0858C1C / den) * dStart;
                    start = start + dir * t;
                }
            }
            // (s == 0 falls through: start lies on the margin band)
        }
    }

    // Fully clipped and still blocked on all planes
    outDist = (end - start).Magnitude();
    return false;
}

// 0x5F2F00
bool CPedGeometryAnalyser::GetIsLineOfSightClear(const CVector& from, const CVector& to, CEntity& entity) {
    const CColLine colLine{ from, to };

    if (!entity.m_matrix) {
        entity.AllocateMatrix();                              // 0x54F560
        entity.m_placement.UpdateMatrix(entity.m_matrix);     // 0x54F1B0
    }

    auto* colModel = CModelInfo::GetModelInfo(entity.m_nModelIndex)->GetColModel();
    if (!colModel) {
        return true;
    }

    // Clear when nothing is hit
    return !CCollision::TestLineOfSight(colLine, *entity.m_matrix, *colModel, false, false);
}

// 0x5F3590
CPed* CPedGeometryAnalyser::GetNearestPed(const CVector& point) {
    CPed* nearest        = nullptr;
    float nearestDistSq  = 3.4028235e38f;

    for (auto& ped : GetPedPool()->GetAllValid()) {
        const float distSq = (point - ped.GetPosition()).SquaredMagnitude();
        if (distSq < nearestDistSq) {
            nearestDistSq = distSq;
            nearest       = &ped;
        }
    }
    return nearest;
}

// 0x5F3970
bool CPedGeometryAnalyser::IsEntityBlockingTarget(CEntity* entity, const CVector& point, float distance) {
    const CVector entityPos = entity->GetPosition();
    const CVector diff      = entityPos - point;

    // Z difference must be small (0.5f) - only things at roughly the same height block
    if (std::abs(diff.z) < PedGeometryAnalyserConstants::FLT_0858B3C) {
        // 2D distance vs (distance^2 + collision radius^2)
        const float radius  = CModelInfo::GetModelInfo(entity->m_nModelIndex)->GetColModel()->m_boundSphere.m_fRadius;
        const float distXY  = sqrtf(diff.x * diff.x + diff.y * diff.y);

        if (distance * distance + radius * radius >= distXY) {
            // Inflate the box planes by distance * 0.5f and check the point is inside all of them
            CVector corners[4];
            CVector planes[4];
            float   planeDots[4];

            ComputeEntityBoundingBoxCornersUncached(entityPos.z, *entity, corners);
            ComputeEntityBoundingBoxPlanesUncached(entityPos.z, corners, &planes, planeDots);

            const float inflated = distance * PedGeometryAnalyserConstants::FLT_0858B8C; // * 0.5f

            for (int i = 0; i < 4; i++) {
                if (DotProduct(point, planes[i]) + planeDots[i] + inflated >= 0.0f) {
                    return false; // outside of this plane
                }
            }
            return true;
        }
    }
    return false;
}

// 0x5F1CB0
bool CPedGeometryAnalyser::IsInAir(const CPed& ped) {
    if (ped.bInVehicle) { // ped flags bit 0x100 -> definitely not in air
        return false;
    }

    auto& taskMgr = ped.GetTaskManager();
    if (taskMgr.GetActiveTask()) {
        if (taskMgr.GetActiveTaskAs<CTaskSimpleSwim>() || taskMgr.GetActiveTaskAs<CTaskSimpleJetPack>()) {
            return true;
        }
        // task type 0xFE == air-borne task
        if (const auto simplest = taskMgr.GetSimplestActiveTask();
            simplest && simplest->GetTaskType() == static_cast<eTaskType>(0xFE)) {
            return true;
        }
    }

    // Otherwise: test whether there is ground right below the ped
    const bool bInVehicleOrSpecial = taskMgr.GetActiveTask()
        && taskMgr.GetActiveTask()->GetTaskType() == static_cast<eTaskType>(0xD3);

    const CVector pedPos = ped.GetPosition();

    CColPoint colPoint{};
    CEntity*  hitEntity{};
    // buildings=true, vehicles=true, peds=false, objects=true
    const bool hitGround = CWorld::ProcessVerticalLine(pedPos, pedPos.z - PedGeometryAnalyserConstants::FLT_086C6A0,
                                                       colPoint, hitEntity, true, true, false, true, false, false, nullptr);

    if (!hitGround && !bInVehicleOrSpecial) {
        // Sphere (r = 0.15f) just under the feet
        if (CWorld::TestSphereAgainstWorld(CVector{ pedPos.x, pedPos.y, pedPos.z - PedGeometryAnalyserConstants::FLT_0858624 },
                                           0.15f,
                                           const_cast<CPed*>(&ped),
                                           true, false, false, false, false, false)) {
            return false; // sphere hit something -> standing on ground
        }
        return true; // nothing below -> in air
    }
    return false;
}

// 0x5F2F70
CPedGeometryAnalyser::WanderPathClearness CPedGeometryAnalyser::IsWanderPathClear(const CVector& from, const CVector& to, float maxHeightChange, int32 maxSamples) {
    if (maxHeightChange < std::abs(from.z - to.z)) {
        return WanderPathClearness::BLOCKED_HEIGHT;
    }

    // Flatten both points to the lower z and do a quick LOS check
    {
        CVector a = from;
        CVector b = to;
        a.z = b.z = std::min(from.z, to.z);

        // buildings only
        if (CWorld::GetIsLineOfSightClear(a, b, true, false, false, false, false, false, false)) {
            return WanderPathClearness::CLEAR;
        }
        /* otherwise fall through to the detailed checks */
    }

    const CVector dir = to - from;
    const int32 numSamples = std::min(maxSamples, static_cast<int32>(dir.Magnitude()));

    if (numSamples > 1) {
        // Check for water along the way
        for (int32 i = 1; i < numSamples; i++) {
            const CVector p{ from.x + dir.x * (float)i,
                             from.y + dir.y * (float)i,
                             from.z + dir.z * (float)i };

            float waterLevel{};
            if (CWaterLevel::GetWaterLevel(p.x, p.y, p.z, waterLevel, false, nullptr)) {
                CEntity*  waterEntity = nullptr;
                CColPoint colPoint{};
                const float vertDist = (to.z <= from.z) ? to.z : from.z;

                const CVector origin{ p.x, p.y, waterLevel };
                // buildings only
                if (!CWorld::ProcessVerticalLine(origin, vertDist, colPoint, waterEntity, true, false, false, false, false, false, nullptr)) {
                    return WanderPathClearness::BLOCKED_LOS;
                }
            }
        }

        // Follow the ground along the path; reject sharp height changes
        CColPoint colPoint{};
        CEntity*  hitEntity = nullptr;
        if (!CWorld::ProcessVerticalLine(from, from.z - StaticRef<float>(0x858C80),
                                         colPoint, hitEntity, true, false, false, false, false, false, nullptr)) {
            return WanderPathClearness::BLOCKED_SHARP_DROP;
        }

        float lastGroundZ = colPoint.m_vecPoint.z + PedGeometryAnalyserConstants::FLT_0858B8C; // + 0.5f

        for (int32 i = 1; i < numSamples; i++) {
            const CVector p{ from.x + dir.x * (float)i,
                             from.y + dir.y * (float)i,
                             lastGroundZ };

            if (!CWorld::ProcessVerticalLine(p, lastGroundZ - PedGeometryAnalyserConstants::FLT_0858CA0,
                                             colPoint, hitEntity, true, false, false, false, false, false, nullptr)) {
                return WanderPathClearness::BLOCKED_SHARP_DROP;
            }
            if (std::abs(colPoint.m_vecPoint.z - lastGroundZ) > PedGeometryAnalyserConstants::FLT_0858624) {
                return WanderPathClearness::BLOCKED_SHARP_DROP;
            }
            lastGroundZ = colPoint.m_vecPoint.z + PedGeometryAnalyserConstants::FLT_0858B8C;
        }
    }
    return WanderPathClearness::CLEAR;
}

// 0x5F3880
bool CPedGeometryAnalyser::LiesInsideBoundingBox(const CPed& ped, const CVector& posn, CEntity& entity) {
    // Entity's collision sphere radius
    const float radius = CModelInfo::GetModelInfo(entity.m_nModelIndex)->GetColModel()->m_boundSphere.m_fRadius;

    // Too far away -> can't be inside
    const float distSq = (posn - entity.GetPosition()).SquaredMagnitude();
    if (!(radius * radius > distSq)) {
        return false;
    }

    // Inflate the box planes by distance * 0.5f and check the point is inside all of them
    const CVector entityPos = entity.GetPosition();
    CVector corners[4];
    CVector planes[4];
    float   planeDots[4];

    ComputeEntityBoundingBoxCornersUncached(entityPos.z, entity, corners);
    ComputeEntityBoundingBoxPlanesUncached(entityPos.z, corners, &planes, planeDots);

    for (int i = 0; i < 4; i++) {
        const float d = DotProduct(posn, planes[i]) + planeDots[i];
        if (d < 0.0f) {
            return true; // inside this plane's half-space (original returns on the first hit)
        }
    }
    return false;
}

// 0x41B7C0
void* CPointRoute::operator new(uint32 size) {
    return GetPointRoutePool()->New();
}

// 0x41B7D0
void CPointRoute::operator delete(void* ptr, size_t sz) {
    GetPointRoutePool()->Delete(reinterpret_cast<CPointRoute*>(ptr));
}
