#include "StdInc.h"

#include "PedGeometryAnalyser.h"
#include <algorithm>
#include <cfloat>
#include <cstring>
#include "World.h"
#include "SurfaceInfos_c.h"
#include "PedGroups.h"
#include "Models/ModelInfo.h"
#include "Collision/ColLine.h"
#include "Collision/Collision.h"
#include "Interior/InteriorManager_c.h"

// sprint-100 wave-1 (pga-2, pga-3):
#include "Entity/Ped/Ped.h"
#include "PedIntelligence.h"
#include "Pools/PedPool.h"
#include "Pools/Pools.h"
#include "WaterLevel.h"

// File-scope globals (sprint-100 wave-1, auto-generated from fragments pga-2/pga-3).
// TU-local; the fragment bodies re-declare same-address locals where they need them
// (shadowing is intentional and benign).
static auto& MARGIN = StaticRef<float>(0x8D22B0); // from pga-3
static auto& MARGIN_BAND_HI = StaticRef<float>(0x858CC4); // from pga-2
static auto& MARGIN_BAND_LO = StaticRef<float>(0x858FDC); // from pga-2
static auto& MINDEN = StaticRef<float>(0x858CDC); // from pga-3
static auto& MINUS_ONE = StaticRef<float>(0x858C1C); // from pga-2
static auto& PROBE_DEPTH = StaticRef<float>(0x858C80); // from pga-3
static auto& PVL_DEPTH = StaticRef<float>(0x86C6A0); // from pga-3
static auto& RADIUS = StaticRef<float>(0x86C6A4); // from pga-3
static auto& RADIUS_MULT = StaticRef<float>(0x858F14); // from pga-3
static auto& THREE = StaticRef<float>(0x858B3C); // from pga-3
static auto& ZERO = StaticRef<float>(0x858B50); // from pga-3

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
    if (entity.m_bIsTempBuilding) { // m_nFlags & 0x400000
        return false;
    }

    // Shoot a LOS ray from the ped's position along its forward vector (buildings + objects) - 0x56A490
    const CVector& startPos = ped.GetPosition();
    const CVector  endPos   = startPos + ped.GetForward();

    return CWorld::GetIsLineOfSightClear(startPos, endPos, true, false, false, true, false, false, false);
}

// 0x5F32D0
bool CPedGeometryAnalyser::CanPedJumpObstacle(const CPed& ped, const CEntity& entity, const CVector& contactNormal, const CVector& /*contactPos - not used by the original (stack leftover)*/) {
    const auto& NORMAL_Z_LOW  = StaticRef<float>(0x86C6AC); // ~0.17f - flat contact threshold
    const auto& JUMP_Z_STEP   = StaticRef<float>(0x858FCC); // 0.15f
    const auto& NORMAL_Z_HIGH = StaticRef<float>(0x858C20); // 0.9f - too steep a wall
    const auto& HALF          = StaticRef<float>(0x858B8C); // 0.5f
    const auto& TWO           = StaticRef<float>(0x858CA0); // 2.0f
    const auto& FOUR          = StaticRef<float>(0x858B90); // 4.0f
    const auto& THREE         = StaticRef<float>(0x858B3C); // 3.0f

    if (entity.m_bIsTempBuilding) { // m_nFlags & 0x400000
        return false;
    }

    const CVector& posn = ped.GetPosition();
    CVector jumpDir = ped.GetForward();

    // In shallow water the ped can always jump (0x55E730)
    if (g_surfaceInfos.IsShallowWater(ped.m_nContactSurface)) {
        return true;
    }

    if (contactNormal.z <= NORMAL_Z_LOW) { // flat contact
        // A player-group ped skips the jumpDir.z adjustment (0x5F7F10)
        if (!CPedGroups::IsInPlayersGroup(const_cast<CPed*>(&ped))) {
            jumpDir.z -= JUMP_Z_STEP;
        }
    } else {
        if (contactNormal.z > NORMAL_Z_HIGH) { // way too steep a wall
            return false;
        }

        // Stairs/slopes: adjust the jump offset using the ped's first collision sphere
        const auto* colData = CModelInfo::GetModelInfo(ped.m_nModelIndex)->GetColModel()->GetData();
        const auto& sphere  = colData->m_pSpheres[0];

        jumpDir.z += sphere.m_vecCenter.z - sphere.m_fRadius * contactNormal.z;

        const float slope = sqrtf(sq(contactNormal.x) + sq(contactNormal.y));

        if (contactNormal.z <= HALF) { // shallow slope
            jumpDir += ped.GetForward() * (slope * sphere.m_fRadius);
        } else {
            CVector slopeDir{ -contactNormal.x, -contactNormal.y, 0.0f };
            slopeDir *= 1.0f / slope;                              // 0x411A30 - operator/=(float)
            jumpDir += slopeDir * (slope * sphere.m_fRadius);
            jumpDir *= std::min(TWO / slope, FOUR);                // 0x40FEF0 - * min(2/slope, 4)
        }
    }

    const CVector target = posn + jumpDir;

    // LOS to the target position (buildings + objects) - 0x56A490
    if (CWorld::GetIsLineOfSightClear(posn, target, true, false, false, true, false, false, false)) {
        // Probe the ground 3m ahead: it must not be more than 3m below - 0x5696C0
        const CVector probe = posn + jumpDir * THREE;
        bool          hasGround{};
        const float   groundZ = CWorld::FindGroundZFor3DCoord(probe, &hasGround, nullptr);
        if (hasGround && probe.z - groundZ < THREE) {
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
    const CVector& posn = ped.GetPosition();
    const CVector  dist = target - posn;

    if (checkDirection) {
        // The target has to be in front of the ped
        if (DotProduct(dist, ped.GetForward()) < StaticRef<float>(0x858B50)) { // 0.0f
            return false;
        }
    }

    // Has to be within the maximum targeting distance
    const auto& MAX_DIST = StaticRef<float>(0x86C69C);
    if (MAX_DIST * MAX_DIST < dist.SquaredMagnitude()) {
        return false;
    }

    // Final LOS test from slightly above the ped's position (buildings + objects, see-through)
    CVector losStart = posn;
    losStart.z += StaticRef<float>(0x858F34);
    return CWorld::GetIsLineOfSightClear(losStart, target, true, false, false, true, false, true, false);
}
// 0x5F1E30
// unused
int32 CPedGeometryAnalyser::ComputeBuildingHitPoints(const CVector& start, const CVector& end) {
    CEntity *outEntity;
    CColPoint colPoint;

    CWorld::ProcessLineOfSight(start, end, colPoint, outEntity, true, false, false, false, true, false, false, false);
    return CWorld::ms_iProcessLineNumCrossings;
}
// 0x5F5D80
void CPedGeometryAnalyser::ComputeClearTarget(const CPed& ped, const CVector& target, CVector& out) {
    const auto& ZERO   = StaticRef<float>(0x858B50);  // 0.0f
    const auto& RADIUS = StaticRef<float>(0x86C6A4);  // 5.0f - max distance to care about
    const auto& MARGIN = StaticRef<float>(0x8D22B0);  // 0.35f - nominal ped radius (writable global)

    out = target;

    const CVector& pedPos = ped.GetPosition();

    // Push the target out of any nearby scanned entity that blocks the ped->target line of sight
    const auto ProcessEntity = [&](CEntity& entity) {
        if (!((entity.GetPosition() - out).SquaredMagnitude() < RADIUS * RADIUS)) {
            return; // too far away to matter
        }
        if (!LiesInsideBoundingBox(ped, out, entity)) { // 0x5F3880
            return;
        }

        float depth{}; // blocked distance (written by the callee)
        if (GetIsLineOfSightClear(ped, out, entity, depth)) { // 0x5F5A30 - clear?
            return;
        }

        // Blocked: move the target away from the ped by margin + blocked distance
        CVector dir = out - pedPos;
        dir.Normalise(); // 0x59C910
        out -= dir * (MARGIN + depth);
    };

    // The ped's intelligence scanner entity lists (16 entries each): vehicles first, then peds
    for (auto& entity : ped.GetIntelligence()->GetVehicleScanner().GetEntities<>()) { // intel + 0xE0
        ProcessEntity(entity);
    }
    for (auto& entity : ped.GetIntelligence()->GetPedScanner().GetEntities<>()) { // intel + 0x130
        ProcessEntity(entity);
    }

    // Finally: step the target away from the ped until the ped->target ray is unblocked,
    // never stepping "towards" the ped
    CVector dir = pedPos - out;
    dir.Normalise(); // 0x59C910
    const CVector step = dir * MARGIN;

    // Original: (int)(5.0f / margin) via the 0x821B40 float->int helper, +1
    const int32 maxSteps = static_cast<int32>(RADIUS / MARGIN) + 1;

    for (int32 stepNo = 0; stepNo < maxSteps + 1; stepNo++) {
        const CVector diff = pedPos - out;
        if (diff.SquaredMagnitude() >= RADIUS * RADIUS) {
            return; // far enough
        }
        if (DotProduct(diff, step) < ZERO) {
            return; // would step towards the ped - direction changed
        }

        // ProcessLineOfSight (buildings+dummies) + crossing count; abs(crossings) != 1 -> clear
        if (ComputeBuildingHitPoints(pedPos, out) % 2 != 1) { // 0x5F1E30
            return; // ray is clear
        }
        out += step; // still blocked (odd number of crossings) - step further away
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
    ComputeEntityBoundingBoxCornersUncached(posn.z, entity, corners); // 0x5F1FA0
    return ComputeClosestSurfacePoint(posn, corners, point);          // 0x5F2C10
}

// 0x5F2C10
bool CPedGeometryAnalyser::ComputeClosestSurfacePoint(const CVector& posn, const CVector* corners, CVector& point) {
    const auto& ZERO = StaticRef<float>(0x858B50); // 0.0f
    const auto& ONE  = StaticRef<float>(0x858624); // ~1.0f

    bool  found      = false;
    float bestDistSq = StaticRef<float>(0x863A3C); // initial best (large .data constant)

    // Closest point on each of the 4 edges (corner[i] -> corner[(i + 1) % 4])
    for (int32 i = 0; i < 4; i++) {
        const int32   j = (i + 1) & 3;
        const CVector edge = corners[j] - corners[i];
        const float   len  = edge.Magnitude();
        const float   inv  = ONE / len;

        // Project posn onto the edge: t in [0, len)
        const float t = DotProduct(posn - corners[i], edge) * inv;
        if (t >= ZERO && t < len) {
            const CVector closest{
                edge.x * inv * t + corners[i].x,
                edge.y * inv * t + corners[i].y,
                edge.z * inv * t + corners[i].z,
            };
            const float distSq = (posn - closest).SquaredMagnitude();
            if (distSq < bestDistSq) {
                point      = closest;
                bestDistSq = distSq;
                found      = true;
            }
        }
    }

    // No edge was hit -> fall back to the closest corner vertex
    if (!found) {
        for (int32 i = 0; i < 4; i++) {
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
    const auto& ZERO       = StaticRef<float>(0x858B50); // 0.0f
    const auto& ONE        = StaticRef<float>(0x858624); // ~1.0f - z slab half width / normalisation scale
    const auto& HALF       = StaticRef<float>(0x858B8C); // 0.5f
    const auto& HEIGHT_TOL = StaticRef<float>(0x86C6A8); // 6.0f - bounding box z size threshold
    auto&       MARGIN     = StaticRef<float>(0x8D22B0); // global margin (writable, see ComputeRouteRoundEntityBoundingBox)

    // Interior furniture buildings -> use the interior manager's bounding box (0x598090)
    if (entity.GetType() == ENTITY_TYPE_BUILDING
        && entity.m_bIsTempBuilding // m_nFlags & 0x400000
        && g_interiorMan.GetBoundingBox(reinterpret_cast<FurnitureEntity_c*>(&entity), corners)) {
        for (auto i = 0; i < 4; i++) {
            corners[i].z = zPos;
        }
        return; // the original returns true here (kept void like the header)
    }

    // Make sure the entity has a matrix (0x54F560 / 0x54F1B0)
    if (!entity.m_matrix) {
        entity.AllocateMatrix();
        entity.m_placement.UpdateMatrix(entity.m_matrix);
    }

    auto*       colModel = CModelInfo::GetModelInfo(entity.m_nModelIndex)->GetColModel();
    const auto* colData  = colModel->GetData();
    const auto& bb       = colModel->GetBoundingBox();

    CVector boxMin, boxMax;
    const bool bUseRawBox = entity.GetType() != ENTITY_TYPE_OBJECT
        || !colData
        || bb.m_vecMax.z - bb.m_vecMin.z <= HEIGHT_TOL
        || (colData->m_nNumSpheres == 0 && colData->m_nNumBoxes == 0);

    if (bUseRawBox) {
        boxMin = bb.m_vecMin;
        boxMax = bb.m_vecMax;
    } else {
        // Tight (local space) AABB of the collision geometry overlapping the [zPos - 1, zPos + 1] slab
        const float zLo = zPos - ONE;
        const float zHi = zPos + ONE;

        boxMin = CVector{ FLT_MAX, FLT_MAX, FLT_MAX };
        boxMax = CVector{ -FLT_MAX, -FLT_MAX, -FLT_MAX };

        const auto& mat = *entity.m_matrix;
        CVector wMin, wMax;

        for (const auto& box : colData->GetBoxes()) { // boxes first, then spheres - like the original
            wMin.FromMultiply(mat, box.m_vecMin); // 0x59C670 - world z is only used for the overlap test
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

        for (const auto& sphere : colData->GetSpheres()) {
            const CVector locMin{
                sphere.m_vecCenter.x - sphere.m_fRadius,
                sphere.m_vecCenter.y - sphere.m_fRadius,
                sphere.m_vecCenter.z - sphere.m_fRadius,
            };
            const CVector locMax{
                sphere.m_vecCenter.x + sphere.m_fRadius,
                sphere.m_vecCenter.y + sphere.m_fRadius,
                sphere.m_vecCenter.z + sphere.m_fRadius,
            };
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

    // Expand the box by the global margin and convert to centre + half extents
    // (the margin terms cancel out for the centre - original expressions kept verbatim)
    const CVector halfExt{
        ((MARGIN + boxMax.x) - (boxMin.x - MARGIN)) * HALF, // == (boxMax.x - boxMin.x) * 0.5f + MARGIN
        ((MARGIN + boxMax.y) - (boxMin.y - MARGIN)) * HALF,
        ((MARGIN + boxMax.z) - (boxMin.z - MARGIN)) * HALF,
    };
    CVector centreLocal{
        (MARGIN + boxMax.x + (boxMin.x - MARGIN)) * HALF, // == (boxMin.x + boxMax.x) * 0.5f
        (MARGIN + boxMax.y + (boxMin.y - MARGIN)) * HALF,
        (MARGIN + boxMax.z + (boxMin.z - MARGIN)) * HALF,
    };
    CVector centre;
    centre.FromMultiply(*entity.m_matrix, centreLocal); // 0x59C670

    // Squared xy lengths of the matrix basis vectors
    const auto& mat      = *entity.m_matrix;
    const float rightXYSq = sq(mat.GetRight().x) + sq(mat.GetRight().y);
    const float fwdXYSq   = sq(mat.GetForward().x) + sq(mat.GetForward().y);
    const float upXYSq    = sq(mat.GetUp().x) + sq(mat.GetUp().y);

    // Flattened extent of the quad along each basis direction (halfExt * xyLen^2 * 2)
    const float extX = halfExt.x * rightXYSq + halfExt.x * rightXYSq;
    const float extY = halfExt.y * fwdXYSq + halfExt.y * fwdXYSq;
    const float extZ = halfExt.z * upXYSq + halfExt.z * upXYSq;

    // The thinnest direction becomes the base axis of the corner quad;
    // the projected extents of the two remaining axes are scaled by their half extents
    CVector axis;   // xy only, NOT normalised (see the note below)
    float   extSelf;
    float   dotA1, crossA1, dotA2, crossA2;
    if (extX <= extY || extX <= extZ) {
        if (extY <= extZ) { // up is the thinnest -> axis = up, extent = z half extent
            axis    = CVector{ mat.GetUp().x, mat.GetUp().y, 0.0f };
            extSelf = halfExt.z;
            dotA1   = (axis.x * mat.GetRight().x + axis.y * mat.GetRight().y) * halfExt.x;
            crossA1 = (axis.x * mat.GetRight().y - axis.y * mat.GetRight().x) * halfExt.x;
            dotA2   = (axis.x * mat.GetForward().x + axis.y * mat.GetForward().y) * halfExt.y;
            crossA2 = (axis.x * mat.GetForward().y - axis.y * mat.GetForward().x) * halfExt.y;
        } else { // forward is the thinnest -> axis = forward, extent = y half extent
            axis    = CVector{ mat.GetForward().x, mat.GetForward().y, 0.0f };
            extSelf = halfExt.y;
            dotA1   = (axis.x * mat.GetRight().x + axis.y * mat.GetRight().y) * halfExt.x;
            crossA1 = (axis.x * mat.GetRight().y - axis.y * mat.GetRight().x) * halfExt.x;
            dotA2   = (axis.x * mat.GetUp().x + axis.y * mat.GetUp().y) * halfExt.z;
            crossA2 = (axis.x * mat.GetUp().y - axis.y * mat.GetUp().x) * halfExt.z;
        }
    } else { // right is the thinnest -> axis = right, extent = x half extent
        axis    = CVector{ mat.GetRight().x, mat.GetRight().y, 0.0f };
        extSelf = halfExt.x;
        dotA1   = (axis.x * mat.GetForward().x + axis.y * mat.GetForward().y) * halfExt.y;
        crossA1 = (axis.x * mat.GetForward().y - axis.y * mat.GetForward().x) * halfExt.y;
        dotA2   = (axis.x * mat.GetUp().x + axis.y * mat.GetUp().y) * halfExt.z;
        crossA2 = (axis.x * mat.GetUp().y - axis.y * mat.GetUp().x) * halfExt.z;
    }

    // The two inner points (centre +/- axis * extSelf); the z offset is extSelf * 0.0f in the original
    const float   halfSelf = extSelf * ZERO;
    const CVector p1{ centre.x + axis.x * extSelf, centre.y + axis.y * extSelf, centre.z + halfSelf };
    const CVector p2{ centre.x - axis.x * extSelf, centre.y - axis.y * extSelf, centre.z - halfSelf };

    // 0x59C910 (CVector::Normalise) - NOTE: the original normalises a *copy* of the flattened
    // axis here, and the result is never used afterwards - everything below keeps using the
    // unnormalised axis (the small axis length is what shrinks the quad for thin obstacles).
    CVector axisNormalised{ axis.x, axis.y, 0.0f };
    axisNormalised.Normalise();

    // Projected extents of the two remaining axes - abs'ed and summed
    if (dotA1 < ZERO) dotA1 = -dotA1;
    if (dotA2 < ZERO) dotA2 = -dotA2;
    const float aSum = dotA1 + dotA2;
    if (crossA1 < ZERO) crossA1 = -crossA1;
    if (crossA2 < ZERO) crossA2 = -crossA2;
    const float bSum = crossA1 + crossA2;

    // Offset vectors: vecA along the axis, vecB perpendicular to it (both flattened, z = 0)
    const CVector vecA{ aSum * axis.x, aSum * axis.y, ZERO };
    const CVector vecB{ bSum * axis.y, -bSum * axis.x, ZERO };

    // The 4 corners - all flattened to zPos (the original writes centre.z first, then overwrites
    // every z component with zPos)
    corners[0] = CVector{ p1.x + vecA.x - vecB.x, p1.y + vecA.y - vecB.y, zPos };
    corners[1] = CVector{ p2.x - vecA.x - vecB.x, p2.y - vecA.y - vecB.y, zPos };
    corners[2] = CVector{ p2.x - vecA.x + vecB.x, p2.y - vecA.y + vecB.y, zPos };
    corners[3] = CVector{ p1.x + vecA.x + vecB.x, p1.y + vecA.y + vecB.y, zPos };
}
// 0x5F3660
void CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanes(float zPos, CEntity& entity, CVector(*outPlanes)[4], float* outPlanesDot) {
    CVector corners[4];
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners);                    // 0x5F1FA0
    ComputeEntityBoundingBoxPlanesUncached(zPos, corners, outPlanes, outPlanesDot);    // 0x5F1670
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
CVector* CPedGeometryAnalyser::ComputeEntityBoundingBoxSegmentPlanesUncached(const CVector* corners, CVector& center, CVector* outPlanes, float* outPlaneDots) {
    for (auto i = 0; i < 4; i++) {
        const CVector& corner = corners[i];
        CVector&       plane  = outPlanes[i];

        plane.x = -(corner.y - center.y);
        plane.y = corner.x - center.x;
        plane.z = 0.0f;

        // point-normal plane equation: d = - n . P
        outPlaneDots[i] = -DotProduct(plane, corner);
    }
    return outPlanes;
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
void CPedGeometryAnalyser::ComputeEntityBoundingSphere(const CPed& ped, CEntity& entity, CColSphere& outSphere) {
    const auto& ZERO        = StaticRef<float>(0x858B50); // 0.0f
    const auto& RADIUS_MULT = StaticRef<float>(0x858F14); // 1.1f

    const float zPos = ped.GetPosition().z;

    // The original computes the corners twice (inlined callee) - kept for fidelity
    CVector corners[4];
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners); // 0x5F1FA0

    CVector cornersB[4];
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, cornersB);

    CVector centre;
    ComputeEntityBoundingBoxCentreUncached(zPos, cornersB, centre); // 0x5F1600

    // Radius: furthest corner from the centre (z is zPos for all of them, so the dz terms are 0)
    float maxDistSq = ZERO;
    for (int32 i = 0; i < 4; i++) {
        maxDistSq = std::max(maxDistSq, (corners[i] - centre).SquaredMagnitude());
    }

    outSphere.Set(sqrtf(maxDistSq) * RADIUS_MULT, centre, SURFACE_DEFAULT); // 0x40FD10 (pieceType 0, lighting 0xFF)
}

// 0x5F3730
// ABI note: the original is void (EAX not written, see fragment pga-3/0x5F3730), but the repo header
// (PedGeometryAnalyser.h:55, read-only for this integration) declares `int32` - so the definition
// keeps `int32` and returns 0 (no in-repo callers; the return value is never consumed).
int32 CPedGeometryAnalyser::ComputeMoveDirToAvoidEntity(const CPed& ped, CEntity& entity, CVector& outDirToAvoidEntity) {
    const auto& ZERO = StaticRef<float>(0x858B50); // 0.0f

    const CVector& pedPos = ped.GetPosition();

    CVector corners[4];
    CVector planes[4];
    float   planeDots[4];
    ComputeEntityBoundingBoxCornersUncached(pedPos.z, entity, corners);                     // 0x5F1FA0
    ComputeEntityBoundingBoxPlanesUncached(pedPos.z, corners, &planes, planeDots);          // 0x5F1670

    // Signed distance of the ped to planes 1 and 3 (the pair whose normals face the ped)
    const float d1 = DotProduct(pedPos, planes[1]) + planeDots[1];
    const float d3 = DotProduct(pedPos, planes[3]) + planeDots[3];

    if (ZERO < d1) { // ped is outside of plane 1 - avoid along its normal
        outDirToAvoidEntity = planes[1];
        return 0; // int32 per repo header; original is void (see ABI note above)
    }
    if (ZERO < d3) {
        outDirToAvoidEntity = planes[3];
        return 0; // int32 per repo header; original is void (see ABI note above)
    }
    // Both behind: pick the plane we are further outside of
    outDirToAvoidEntity = d3 < d1 ? planes[1] : planes[3];
    return 0; // see ABI note above - the original never writes EAX
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
CVector* CPedGeometryAnalyser::ComputeEntityDirs(const CEntity& entity, CVector* outDirs) {
    CVector forward;
    CVector right;

    if (entity.m_matrix) {
        forward = entity.m_matrix->GetForward();
        right   = entity.m_matrix->GetRight();
    } else {
        const float heading = entity.m_placement.m_fHeading;
        forward = CVector{ -sinf(heading), cosf(heading), 0.0f };
        right   = CVector{  cosf(heading), sinf(heading), 0.0f };
    }

    const auto& MINUS_ONE = StaticRef<float>(0x858C1C); // -1.0f

    outDirs[0] = forward;
    outDirs[1] = right * MINUS_ONE;   // left
    outDirs[2] = forward * MINUS_ONE; // backward
    outDirs[3] = right;
    return outDirs;
}

// 0x5F3BC0
int32 CPedGeometryAnalyser::ComputeEntityHitSide(const CPed& ped, CEntity& entity) {
    return ComputeEntityHitSide(ped.GetPosition(), entity);
}

// 0x5F1450
int32 CPedGeometryAnalyser::ComputeEntityHitSide(const CVector& point, const CVector* planes, const float* planeDots) {
    // Find the first pair of consecutive planes whose signs differ - the point is in front
    // of plane i (d >= 0) and behind plane j = i + 1 (d < 0). The second plane of the pair
    // is the hit side. The original scans the pairs in the order (3,0), (0,1), (1,2), (2,3).
    for (int32 k = 0; k < 4; k++) {
        const int32 i = (k + 3) & 3; // 3, 0, 1, 2
        const int32 j = (k + 4) & 3; // 0, 1, 2, 3

        if (DotProduct(point, planes[i]) + planeDots[i] >= StaticRef<float>(0x858B50) // 0.0f
            && DotProduct(point, planes[j]) + planeDots[j] < StaticRef<float>(0x858B50)) {
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

    ComputeEntityBoundingBoxCornersUncached(point.z, entity, corners);                 // 0x5F1FA0
    ComputeEntityBoundingBoxCentreUncached(point.z, corners, centre);                  // 0x5F1600
    ComputeEntityBoundingBoxSegmentPlanesUncached(corners, centre, planes, planeDots); // 0x5F1750
    return ComputeEntityHitSide(point, planes, planeDots);                             // 0x5F1450
}

// 0x5F3640
int32 CPedGeometryAnalyser::ComputePedHitSide(const CPed& ped, const CPhysical& physical) {
    return ComputePedHitSide(ped, physical.m_vecMoveSpeed);
}
// 0x5F1E70
int32 CPedGeometryAnalyser::ComputePedHitSide(const CPed& ped, const CVector& posn) {
    const auto& MINUS_ONE = StaticRef<float>(0x858C1C); // -1.0f

    CVector dir = posn * MINUS_ONE; // negated posn (the original multiplies each component by -1.0f)
    dir.Normalise();                // 0x59C910

    CVector dirs[4]; // forward, left, backward, right
    ComputeEntityDirs(ped, dirs);   // 0x5F1500

    // Max-dot scan over the 4 direction vectors. Initial `best` is the raw bits of d0
    // (original quirk - only observable if every dot is < -1.0f, which can't happen
    // with normalised vectors), initial bestDot is -1.0f:
    //   if (-1.0f <= d0) { best = 0; bestDot = d0; }
    //   if (bestDot <= d1) { best = 1; bestDot = d1; }
    //   if (bestDot <= d2) { best = 2; bestDot = d2; }
    //   return d3 < bestDot ? best : 3;
    const float d0 = DotProduct(dirs[0], dir);
    int32 best;
    std::memcpy(&best, &d0, sizeof(int32)); // original quirk: `best = (int32)d0` (raw bits)
    float bestDot = MINUS_ONE;
    if (MINUS_ONE <= d0) {
        best    = 0;
        bestDot = d0;
    }

    const float d1 = DotProduct(dirs[1], dir);
    if (bestDot <= d1) {
        best    = 1;
        bestDot = d1;
    }

    const float d2 = DotProduct(dirs[2], dir);
    if (bestDot <= d2) {
        best    = 2;
        bestDot = d2;
    }

    const float d3 = DotProduct(dirs[3], dir);
    return d3 < bestDot ? best : 3;
}
// 0x5F13F0
int32 CPedGeometryAnalyser::ComputePedShotSide(const CPed& ped, const CVector& posn) {
    const CVector& pedPos = ped.GetPosition();

    const float angle = atan2f(pedPos.x - posn.x, posn.y - pedPos.y); // x87 fpatan(-(dx), dy) - по часовой от +Y
    float       v     = angle - ped.m_fCurrentRotation + StaticRef<float>(0x859AB0); // + pi/4
    if (v < StaticRef<float>(0x858B50)) { // 0.0f - отрицательные заворачиваются (fcom/fnstsw/test ah,5/jp)
        v += StaticRef<float>(0x858CBC);  // 2*pi
    }
    return (int32)(v * StaticRef<float>(0x858FB8)); // * 2/pi, усечение до int (0x821B40 = CRT _ftol2)
}

// 0x5F6110
int32 CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox(const CPed& ped, CEntity& entity, const CVector& posn, CPointRoute& pointRoute, int32 a5) {
    return ComputeRouteRoundEntityBoundingBox(ped, ped.GetPosition(), entity, posn, pointRoute, a5);
}

// 0x5F3DD0
int32 CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox(const CPed& ped, const CVector& start, CEntity& entity, const CVector& end, CPointRoute& route, int32 mode) {
    const auto& ZERO           = StaticRef<float>(0x858B50); // 0.0f
    const auto& HALF           = StaticRef<float>(0x858B8C); // 0.5f
    const auto& QUARTER        = StaticRef<float>(0x858C84); // 0.25f
    const auto& MARGIN_BAND_HI = StaticRef<float>(0x858CC4); // ~+0.2f - upper clipping band bound
    const auto& MARGIN_BAND_LO = StaticRef<float>(0x858FDC); // ~-0.2f - lower clipping band bound
    auto&       MARGIN         = StaticRef<float>(0x8D22B0); // global margin (writable)

    route.Clear(); // *route = 0

    // Temporarily replace the global margin with 0x3E333333 (~0.17f) for the corners/planes pass below
    const float savedMargin = MARGIN;
    MARGIN = 0.17f; // 0x3E333333

    // NOTE: zPos is the PED's z - the original re-reads it from the ped's matrix pos / placement pos,
    // not from `start` and not from the entity (fix vs stash)
    const float zPos = ped.GetPosition().z;

    CVector cornersA[4];
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, cornersA); // 0x5F1FA0
    CVector cornersB[4];
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, cornersB); // 0x5F1FA0 (called twice, like the original)

    CVector planes[4];
    float   planeDots[4];
    ComputeEntityBoundingBoxPlanesUncached(zPos, cornersB, &planes, planeDots); // 0x5F1670

    MARGIN = savedMargin; // restored right after the planes pass (0x5F3DD0+...)

    // Both endpoints on the "outside" of the same plane -> no route needed
    for (auto i = 0; i < 4; i++) {
        if (DotProduct(start, planes[i]) + planeDots[i] > ZERO
            && DotProduct(end, planes[i]) + planeDots[i] > ZERO) {
            return 0;
        }
    }

    // The endpoints must be on different sides of the box (segment planes through the corner centre)
    {
        CVector corners[4];
        ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners); // 0x5F1FA0 (3rd pass)

        CVector centre{ // average of the 4 corners (non-const: 0x5F1750 takes CVector& center)
            (corners[0].x + corners[1].x + corners[2].x + corners[3].x) * QUARTER,
            (corners[0].y + corners[1].y + corners[2].y + corners[3].y) * QUARTER,
            zPos,
        };
        CVector normals[4];
        float   dots[4];
        ComputeEntityBoundingBoxSegmentPlanesUncached(corners, centre, normals, dots); // 0x5F1750
        if (ComputeEntityHitSide(start, normals, dots) == ComputeEntityHitSide(end, normals, dots)) { // 0x5F1450
            return 0;
        }
    }

    // Redo the corners/planes computation (the original does the same)
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, cornersA);                            // 0x5F1FA0
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, cornersB);                            // 0x5F1FA0
    ComputeEntityBoundingBoxPlanesUncached(zPos, cornersB, &planes, planeDots);                 // 0x5F1670

    // Clip both endpoints against the 4 planes, keeping them within the [-0.2f, +0.2f] margin band
    CVector pStart = start;
    CVector pEnd   = end;
    CVector segDir = end - start;
    segDir.Normalise(); // 0x59C910
    bool bTouchedPlane = false;

    // Classify a plane distance: 1 - above the band, -1 - below it, 0 - inside the band
    const auto classify = [&](float d) -> int32 {
        if (d <= MARGIN_BAND_HI) {
            return d >= MARGIN_BAND_LO ? 0 : -1;
        }
        return 1;
    };

    for (auto i = 0; i < 4; i++) {
        const float dStart = DotProduct(pStart, planes[i]) + planeDots[i];
        const float dEnd   = DotProduct(pEnd, planes[i]) + planeDots[i];

        const int32 s = classify(dStart);
        const int32 e = classify(dEnd);

        if (s == -1) {
            if (e == 1) { // clip the end back onto the plane (parametrised from the original start!)
                const float t = -DotProduct(start, planes[i]) / DotProduct(segDir, planes[i]);
                pEnd = start + segDir * t;
            }
        } else if (e == -1) {
            if (s == 1) { // clip the start forward onto the plane
                const float t = -DotProduct(start, planes[i]) / DotProduct(segDir, planes[i]);
                pStart = start + segDir * t;
            }
        } else {
            bTouchedPlane = true;
            if (s == 0) { // push the start deeper into the band
                pStart += planes[i] * (MARGIN_BAND_HI - dStart);
            }
            if (e == 0) { // push the end deeper into the band
                pEnd += planes[i] * (MARGIN_BAND_HI - dEnd);
            }
        }
    }

    // No plane was touched -> use the entity's bounding sphere ray intersections as the segment
    if (!bTouchedPlane) {
        CColSphere sphere;
        ComputeEntityBoundingSphere(ped, entity, sphere); // 0x5F3C20

        CVector dir2D{ end.x - start.x, end.y - start.y, 0.0f };
        if (sqrtf(sq(dir2D.x) + sq(dir2D.y)) == ZERO) { // zero-length 2D segment
            return 0;
        }
        dir2D.Normalise(); // 0x59C910

        pStart = start;
        pEnd   = end;
        if (!sphere.IntersectRay(start, dir2D, pStart, pEnd)) { // 0x40FF20
            return 0;
        }
    }

    // Clip (pStart -> pEnd) against each plane, remembering the entry/exit plane indices
    CVector clipDir = pEnd - pStart;
    clipDir.Normalise(); // 0x59C910
    const CVector clipBase = pStart;

    int32 enterPlane = -1;
    int32 exitPlane  = -1;

    for (auto i = 0; i < 4; i++) {
        const float dStart = DotProduct(pStart, planes[i]) + planeDots[i];
        const float dEnd   = DotProduct(pEnd, planes[i]) + planeDots[i];

        if (dStart == ZERO || dEnd == ZERO) {
            continue; // point exactly on the plane - skipped in the original
        }

        if (dStart < ZERO || ZERO <= dEnd) {
            if (dStart < ZERO && ZERO <= dEnd) { // start inside, end outside -> exit plane
                exitPlane = i;
                const float t = -DotProduct(clipBase, planes[i]) / DotProduct(clipDir, planes[i]);
                pEnd = clipBase + clipDir * t;
            }
        } else { // start outside, end inside -> entry plane
            enterPlane = i;
            const float t = -DotProduct(clipBase, planes[i]) / DotProduct(clipDir, planes[i]);
            pStart = clipBase + clipDir * t;
        }
    }

    if (enterPlane < 0 || exitPlane < 0) {
        return 0;
    }

    // Walk the box corners between the entry and exit planes, building 2 candidate paths:
    // A walks backwards from (enterPlane - 1) up to exitPlane, B walks forwards from (enterPlane + 1)
    CVector pathA[9]; int32 numA = 0;
    CVector pathB[9]; int32 numB = 0;

    {
        auto       idx  = (enterPlane + 3) % 4;
        const auto last = (exitPlane + 4) % 4;
        pathA[numA++] = start;
        pathA[numA++] = cornersA[idx];
        while (idx != last) {
            idx = (idx + 3) % 4; // -1 mod 4
            pathA[numA++] = cornersA[idx];
        }
        pathA[numA++] = end;
    }
    {
        auto       idx  = (enterPlane + 4) % 4;
        const auto last = (exitPlane + 3) % 4;
        pathB[numB++] = start;
        pathB[numB++] = cornersA[idx];
        while (idx != last) {
            idx = (idx + 5) % 4; // +1 mod 4
            pathB[numB++] = cornersA[idx];
        }
        pathB[numB++] = end;
    }

    const CVector centre{ // centre of the (corner) box
        (cornersA[0].x + cornersA[1].x + cornersA[2].x + cornersA[3].x) * QUARTER,
        (cornersA[0].y + cornersA[1].y + cornersA[2].y + cornersA[3].y) * QUARTER,
        (cornersA[0].z + cornersA[1].z + cornersA[2].z + cornersA[3].z) * QUARTER,
    };

    // Validate a path segment by segment with LOS checks.
    // Returns false when blocked; then outBlockSeg is the endpoint index the walk was blocked at
    // (the original records the loop counter at the break) and outHit the blocking entity.
    const auto validatePath = [&](const CVector* path, int32 num, int32& outBlockSeg, CEntity*& outHit) -> bool {
        outBlockSeg = num; // stays the count when the path is valid

        CVector running = path[0];
        for (int32 i = 1; i < num; i++) {
            const CVector& cur = path[i];
            outBlockSeg = i;

            // 2D perpendicular offset, half a unit wide; flipped so it points towards the box centre.
            // NOTE: the z-term of the dot is multiplied by 0.0f in the original - kept verbatim.
            CVector dir2D{ cur.x - running.x, cur.y - running.y, 0.0f };
            dir2D.Normalise(); // 0x59C910
            CVector off{ dir2D.y * HALF, -(dir2D.x * HALF), 0.0f };
            if ((cur.x - centre.x) * off.x + (cur.y - centre.y) * off.y + (cur.z - centre.z) * ZERO < ZERO) {
                off *= -1.0f;
            }

            const CVector a = running + off;
            const CVector b = cur + off;

            CColPoint colPoint{};
            CEntity*  hitEntity{};
            const bool blocked = CWorld::ProcessLineOfSight(running, cur, colPoint, hitEntity, true, true, true, true, false, false, false, false) // 0x56BA00
                              || CWorld::ProcessLineOfSight(a, b, colPoint, hitEntity, true, true, true, true, false, false, false, false);
            if (blocked) {
                if (hitEntity != &entity && hitEntity != &ped) {
                    // A ped not standing still is ignored when it moves in the same direction as the segment
                    if (hitEntity->GetType() == ENTITY_TYPE_PED
                        && reinterpret_cast<const CPed*>(hitEntity)->m_nMoveState != PEDMOVE_STILL) { // *(int32*)(hit + 0x534) != 1
                        if (!hitEntity->m_matrix) {
                            hitEntity->AllocateMatrix();                              // 0x54F560
                            hitEntity->m_placement.UpdateMatrix(hitEntity->m_matrix); // 0x54F1B0
                        }
                        if (DotProduct(cur - running, hitEntity->m_matrix->GetForward()) >= ZERO) {
                            running = cur; // moving in the same direction -> not an obstacle
                            continue;
                        }
                    }
                    outHit = hitEntity;
                    return false; // path blocked
                }
                outHit = nullptr; // hit the ped itself or the target entity - not blocking
            }
            running = cur;
        }
        return true;
    };

    const auto pathLength = [&](const CVector* path, int32 num) { // [&]: ZERO is an enclosing const local (StaticRef)
        float len = ZERO;
        for (int32 i = 0; i < num - 1; i++) {
            len += (path[i + 1] - path[i]).Magnitude();
        }
        return len;
    };

    int32 result;
    if (mode == 0) {
        CEntity* hitA{};
        CEntity* hitB{};
        int32    blockSegA{};
        int32    blockSegB{};

        const bool  aValid = validatePath(pathA, numA, blockSegA, hitA);
        const bool  bValid = validatePath(pathB, numB, blockSegB, hitB);
        const float lenA   = pathLength(pathA, numA);
        const float lenB   = pathLength(pathB, numB);

        if (aValid) {
            result = bValid ? (lenA < lenB ? 1 : 2) : 1; // both valid -> the shorter path wins
        } else if (bValid) {
            result = 2;
        } else if (blockSegA == 1) {
            result = 2; // A is blocked on its very first segment
        } else if (blockSegB == 1) {
            result = 1; // B is blocked on its first segment (A only further along)
        } else if (hitA == hitB) {
            result = lenA < lenB ? 1 : 2; // same blocker -> the shorter path
        } else {
            // Different blockers -> the path blocked by the entity with the smaller collision radius
            const float radiusA = CModelInfo::GetModelInfo(hitA->m_nModelIndex)->GetColModel()->m_boundSphere.m_fRadius;
            const float radiusB = CModelInfo::GetModelInfo(hitB->m_nModelIndex)->GetColModel()->m_boundSphere.m_fRadius;
            result = radiusA < radiusB ? 1 : 2;
        }
    } else {
        result = (mode != 1) + 1; // mode 1 -> path A, any other -> path B
    }

    // Copy the chosen path into the route (skipping the start point), then the end point
    {
        const CVector* chosen  = result == 1 ? pathA : pathB;
        const int32    chosenN = result == 1 ? numA : numB;
        for (int32 i = 1; i < chosenN - 1; i++) {
            route.AddUnlessFull(chosen[i]); // capped at 8 points, like the original
        }
        route.AddUnlessFull(end); // LAB_005F5801
    }

    // Drop the first route point when it lies on the same side of the box as the start point
    if (route.GetSize() > 1) {
        const auto getSide = [&](const CVector& point) {
            CVector corners[4];
            ComputeEntityBoundingBoxCornersUncached(point.z, entity, corners); // 0x5F1FA0 (z from the tested point!)

            CVector centre2{ // non-const: 0x5F1750 takes CVector& center
                (corners[0].x + corners[1].x + corners[2].x + corners[3].x) * QUARTER,
                (corners[0].y + corners[1].y + corners[2].y + corners[3].y) * QUARTER,
                point.z,
            };
            CVector normals[4];
            float   dots[4];
            ComputeEntityBoundingBoxSegmentPlanesUncached(corners, centre2, normals, dots); // 0x5F1750
            return ComputeEntityHitSide(point, normals, dots);                              // 0x5F1450
        };

        if (getSide(start) == getSide(route[1])) {
            for (size_t i = 0; i + 1 < route.GetSize(); i++) {
                route[i] = route[i + 1];
            }
            route.ResizeTo(route.GetSize() - 1);
        }
    }

    // The original decrements the count once more at the very end (the end point never stays in the route)
    if (route.GetSize() > 0) {
        route.ResizeTo(route.GetSize() - 1);
    }

    return (result != 1) + 1; // 1 (path A) or 2 (path B)
}

// 0x5F1890
bool CPedGeometryAnalyser::ComputeRouteRoundSphere(const CPed& ped, const CColSphere& sphere, const CVector& from, const CVector& target, CVector& outTarget, CVector& outPoint) {
    // IntersectPoint/IntersectRay are not const-qualified in the repo (Collision/ColSphere.h:39,42)
    // although they never mutate the sphere (see ColSphere.cpp:26,75). The parameter must stay const
    // (header-declared), so all intersection calls below go through a non-const local copy.
    CColSphere sphereNC = sphere;

    const CVector& pedPos = ped.GetPosition();

    outTarget = target;

    // The target itself is inside the sphere -> push it to the sphere's far surface
    if (sphereNC.IntersectPoint(target)) { // 0x410040
        CVector dir = target - from;
        dir.Normalise(); // 0x59C910

        CVector i1, i2;
        if (sphereNC.IntersectRay(pedPos, dir, i1, i2)) { // 0x40FF20
            outTarget = i2; // second (far) intersection
        }
    }

    // Ray from the ped towards the (adjusted) target
    CVector dir = outTarget - pedPos;
    dir.Normalise(); // 0x59C910

    CVector i1, i2;
    if (!sphereNC.IntersectRay(outTarget, dir, i1, i2)) { // 0x40FF20
        outPoint = outTarget;
        return false;
    }

    // The target is already closer to the ped than the sphere's entry point -> nothing to do
    if ((outTarget - pedPos).SquaredMagnitude() < (i1 - pedPos).SquaredMagnitude()) {
        outPoint = outTarget;
        return false;
    }

    // Project the sphere centre onto the ray and push the output point onto the sphere surface
    if (sphereNC.IntersectRay(pedPos, dir, i1, i2)) { // 0x40FF20
        const float t = DotProduct(sphere.m_vecCenter - pedPos, dir);

        CVector delta = pedPos + dir * t - sphere.m_vecCenter; // closest approach minus sphere centre
        delta.Normalise(); // 0x59C910 (the original scales the delta by 1/|delta| before the radius)

        outPoint = sphere.m_vecCenter + delta * sphere.m_fRadius;
    }
    return true;
}

// 0x5F5A30
bool CPedGeometryAnalyser::GetIsLineOfSightClear(const CPed& ped, const CVector& target, CEntity& entity, float& outDist) {
    const auto& M1     = StaticRef<float>(0x858C1C); // -1.0f
    const auto& MINDEN = StaticRef<float>(0x858CDC); // 0.001f - min |dir . plane| for clipping
    const auto& MARGIN = StaticRef<float>(0x8D22B0); // 0.35f - nominal ped radius (writable global)

    const CVector& pedPos = ped.GetPosition();

    CColSphere sphere;
    ComputeEntityBoundingSphere(ped, entity, sphere); // 0x5F3C20

    CVector start = pedPos;
    CVector end   = target;
    CVector dir   = end - start;
    dir.NormaliseAndMag(); // 0x59C970

    // Ray doesn't touch the bounding sphere at all -> nothing can block
    {
        CVector i1, i2;
        if (!sphere.IntersectRay(start, dir, i1, i2)) { // 0x40FF20
            return true;
        }
    }

    CVector corners[4];
    CVector planes[4];
    float   planeDots[4];
    ComputeEntityBoundingBoxCornersUncached(pedPos.z, entity, corners);                     // 0x5F1FA0
    ComputeEntityBoundingBoxPlanesUncached(pedPos.z, corners, &planes, planeDots);          // 0x5F1670

    outDist = 0.0f;

    // Clip the segment against the 4 bounding box planes (margin band = +/- MARGIN around each plane)
    for (int32 i = 0; i < 4; i++) {
        const float dStart = DotProduct(start, planes[i]) + planeDots[i];
        const float dEnd   = DotProduct(end,   planes[i]) + planeDots[i];

        const auto sideOf = [&](float d) -> int32 { // -1: behind, 0: within the margin band, 1: in front
            if (d <= MARGIN) {
                return d < -MARGIN ? -1 : 0;
            }
            return 1;
        };

        const int32 sStart = sideOf(dStart);
        const int32 sEnd   = sideOf(dEnd);

        const float den = DotProduct(dir, planes[i]);

        if (sStart < 0) {
            if (sEnd > 0) { // inside -> outside: pull the end back onto the plane
                if (den > MINDEN) {
                    const float t = (M1 / den) * dStart;
                    end = start + dir * t;
                }
            }
            // (end within the band or behind: nothing to do for this plane)
        } else {
            if (sEnd >= 0) {
                return true; // segment fully in front of this plane -> clear
            }
            if (sStart > 0) { // outside -> inside: push the start onto the plane
                if (den > MINDEN) {
                    const float t = (M1 / den) * dStart;
                    start = start + dir * t;
                }
            }
            // (start within the band: fall through)
        }
    }

    outDist = (end - start).Magnitude();
    return false; // blocked
}

// 0x5F2F00
bool CPedGeometryAnalyser::GetIsLineOfSightClear(const CVector& from, const CVector& to, CEntity& entity) {
    const CColLine colLine{ from, to }; // 0x40EF50

    if (!entity.m_matrix) {
        entity.AllocateMatrix();                          // 0x54F560
        entity.m_placement.UpdateMatrix(entity.m_matrix); // 0x54F1B0
    }

    auto* colModel = CModelInfo::GetModelInfo(entity.m_nModelIndex)->GetColModel();

    // Clear when the line doesn't hit the entity's collision model
    return !CCollision::TestLineOfSight(colLine, *entity.m_matrix, *colModel, false, false); // 0x417730
}
// 0x5F3590
CPed* CPedGeometryAnalyser::GetNearestPed(const CVector& point) {
    CPed* nearest       = nullptr;
    float nearestDistSq = FLT_MAX; // 3.4028235e38f in the original

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
    const auto& ZERO  = StaticRef<float>(0x858B50); // 0.0f
    const auto& THREE = StaticRef<float>(0x858B3C); // 3.0f - max z difference
    const auto& HALF  = StaticRef<float>(0x858B8C); // 0.5f

    const CVector entityPos = entity->GetPosition();

    // Only things at roughly the same height block (|dz| <= 3.0f)
    if (const float dz = std::abs(entityPos.z - point.z); dz < THREE || dz == THREE) {
        // Original quirk (kept; see upstream PR 1434 bugfix flag): squared sum vs NOT-squared distance
        const float radius = entity->GetBoundRadius();
        const float xyDist = sqrtf(sq(entityPos.x - point.x) + sq(entityPos.y - point.y));

        if (distance * distance + radius * radius >= xyDist) {
            distance *= HALF;

            CVector corners[4];
            CVector planes[4];
            float   planeDots[4];
            ComputeEntityBoundingBoxCornersUncached(entityPos.z, *entity, corners);                     // 0x5F1FA0
            ComputeEntityBoundingBoxPlanesUncached(entityPos.z, corners, &planes, planeDots);           // 0x5F1670

            // Point must be inside all 4 planes, inflated by half the distance
            for (int32 i = 0; i < 4; i++) {
                if (DotProduct(point, planes[i]) + planeDots[i] + distance > ZERO) {
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
    const auto& ONE       = StaticRef<float>(0x858624); // 1.0f
    const auto& PVL_DEPTH = StaticRef<float>(0x86C6A0); // 1.5f - ground probe depth
    const auto& SPHERE_R  = StaticRef<float>(0x858FCC); // 0.15f

    if (ped.bInVehicle) { // m_nPedFlags bit 0x100 (TEST AH,0x1) - definitely not in the air
        return false;
    }

    auto& taskMgr = ped.GetTaskManager();
    if (taskMgr.GetActiveTask()) {
        // Swimming or jetpacking is not "in the air" (0x601070 / 0x601110)
        if (ped.GetIntelligence()->GetTaskSwim()
            || ped.GetIntelligence()->GetTaskJetPack()) {
            return false;
        }
        // Climbing is not "in the air" either; no null-check in the original
        // (GetSimplestActiveTask() can't be null when GetActiveTask() isn't)
        if (taskMgr.GetSimplestActiveTask()->GetTaskType() == TASK_SIMPLE_CLIMB) { // 0xFE
            return false;
        }
    }

    // Otherwise: is there any ground right below the ped?
    const CVector pedPos = ped.GetPosition();

    CColPoint colPoint;
    CEntity*  hitEntity{};
    // buildings=true, vehicles=true, peds=false, objects=true
    const bool hasGround = CWorld::ProcessVerticalLine(pedPos, pedPos.z - PVL_DEPTH,
                                                       colPoint, hitEntity,
                                                       true, true, false, true, false, false, nullptr);

    // A jump task skips the sphere test below (0xD3; the original calls GetActiveTask() twice here)
    const auto* const activeTask = taskMgr.GetActiveTask();
    const bool        bJumpTask  = activeTask && activeTask->GetTaskType() == TASK_COMPLEX_JUMP;

    if (!hasGround && !bJumpTask) {
        // Sphere (r = 0.15f) just below the feet - something to stand on?
        if (CWorld::TestSphereAgainstWorld(CVector{ pedPos.x, pedPos.y, pedPos.z - ONE },
                                           SPHERE_R,
                                           const_cast<CPed*>(&ped),
                                           true, false, false, false, false, false)) { // 0x569E20
            return false; // found something to stand on
        }
    }
    return !hasGround;
}

// 0x5F2F70
CPedGeometryAnalyser::WanderPathClearness CPedGeometryAnalyser::IsWanderPathClear(const CVector& from, const CVector& to, float maxHeightChange, int32 maxSamples) {
    const auto& ONE        = StaticRef<float>(0x858624); // 1.0f - max ground height change between samples
    const auto& HALF       = StaticRef<float>(0x858B8C); // 0.5f
    const auto& TWO        = StaticRef<float>(0x858CA0); // 2.0f - per-sample ground probe depth
    const auto& PROBE_DEPTH = StaticRef<float>(0x858C80); // 5.0f - initial ground probe depth

    if (maxHeightChange < std::abs(from.z - to.z)) {
        return WanderPathClearness::BLOCKED_HEIGHT;
    }

    // Quick LOS test with both points flattened to the lower z (buildings only)
    const float lowestZ = (to.z <= from.z) ? to.z : from.z;
    const CVector fromFlat{ from.x, from.y, lowestZ };
    const CVector toFlat{ to.x, to.y, lowestZ };

    if (!CWorld::GetIsLineOfSightClear(fromFlat, toFlat, true, false, false, false, false, false, false)) { // 0x56A490
        return WanderPathClearness::BLOCKED_LOS;
    }

    CVector dir = to - from; // non-const: Normalise() below (0x59C910 is an in-place, non-const method)
    // Number of samples: min(maxSamples, (int32)length) - 0x8219F0/0x821B40 is the binary's float->int pair
    const int32 numSamples = std::min(maxSamples, static_cast<int32>(dir.Magnitude()));

    if (numSamples != 0) {
        dir.Normalise(); // 0x59C910

        // Water check along the way
        for (int32 i = 1; i < numSamples; i++) {
            const CVector p = from + dir * static_cast<float>(i);

            float waterLevel{};
            if (CWaterLevel::GetWaterLevel(p.x, p.y, p.z, waterLevel, false, nullptr)) { // 0x6EB690
                CEntity*  hitEntity{};
                CColPoint colPoint{};
                // Vertical line from the water surface down; the "distance" is the lowest of the two z's (original quirk)
                if (!CWorld::ProcessVerticalLine(CVector{ p.x, p.y, waterLevel }, lowestZ,
                                                 colPoint, hitEntity,
                                                 true, false, false, false, false, false, nullptr)) { // 0x5674E0
                    return WanderPathClearness::BLOCKED_WATER;
                }
            }
        }

        // Follow the ground along the path; reject sharp height changes
        CEntity*  hitEntity{};
        CColPoint colPoint{};
        if (!CWorld::ProcessVerticalLine(from, from.z - PROBE_DEPTH,
                                         colPoint, hitEntity,
                                         true, false, false, false, false, false, nullptr)) {
            return WanderPathClearness::BLOCKED_SHARP_DROP;
        }

        float lastGroundZ = colPoint.m_vecPoint.z + HALF;

        for (int32 i = 1; i < numSamples; i++) {
            const CVector p{ from.x + dir.x * static_cast<float>(i),
                             from.y + dir.y * static_cast<float>(i),
                             lastGroundZ };

            // Note: `lastGroundZ - 2.0f` (an absolute z) is passed as the distance (original quirk)
            if (!CWorld::ProcessVerticalLine(p, lastGroundZ - TWO,
                                             colPoint, hitEntity,
                                             true, false, false, false, false, false, nullptr)) {
                return WanderPathClearness::BLOCKED_SHARP_DROP;
            }
            if (ONE < std::abs(colPoint.m_vecPoint.z - lastGroundZ)) {
                return WanderPathClearness::BLOCKED_SHARP_DROP;
            }
            lastGroundZ = colPoint.m_vecPoint.z + HALF;
        }
    }
    return WanderPathClearness::CLEAR;
}

// 0x5F3880
bool CPedGeometryAnalyser::LiesInsideBoundingBox(const CPed& ped, const CVector& posn, CEntity& entity) {
    const auto& ZERO = StaticRef<float>(0x858B50); // 0.0f

    // Bounding sphere radius of the entity's collision model
    // (original: *(float*)(*(int*)&ms_modelInfoPtrs[entity->m_nModelIndex][0x14] + 0x24) == CColModel::m_boundSphere.m_fRadius)
    const float radius   = entity.GetBoundRadius();
    const float radiusSq = radius * radius;

    // Too far away - can't be inside (strictly `radiusSq > distSq`)
    const float distSq = (posn - entity.GetPosition()).SquaredMagnitude();
    if (!(radiusSq > distSq)) {
        return false;
    }

    CVector corners[4];
    CVector planes[4];
    float   planeDots[4];
    ComputeEntityBoundingBoxCornersUncached(ped.GetPosition().z, entity, corners);            // 0x5F1FA0
    ComputeEntityBoundingBoxPlanesUncached(ped.GetPosition().z, corners, &planes, planeDots); // 0x5F1670

    // Point behind any of the 4 planes -> "inside" (the original returns on the first hit)
    for (int32 i = 0; i < 4; i++) {
        if (DotProduct(posn, planes[i]) + planeDots[i] < ZERO) {
            return true;
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
