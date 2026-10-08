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

void CPedGeometryAnalyser::InjectHooks() {
    RH_ScopedClass(CPedGeometryAnalyser);
    RH_ScopedCategoryGlobal();

    RH_ScopedOverloadedInstall(CanPedJumpObstacle, "", 0x5F1B00, bool(*)(const CPed&,const CEntity&), { .Reversed = false });
    RH_ScopedOverloadedInstall(CanPedJumpObstacle, "contacted", 0x5F32D0, bool(*)(const CPed&,const CEntity&,const CVector&,const CVector&));
    RH_ScopedInstall(CanPedTargetPed, 0x5F1C40);
    RH_ScopedInstall(CanPedTargetPoint, 0x5F1B70);
    RH_ScopedInstall(ComputeBuildingHitPoints, 0x5F1E30);
    RH_ScopedInstall(ComputeClearTarget, 0x5F5D80, { .Reversed = false });
    RH_ScopedOverloadedInstall(ComputeClosestSurfacePoint, "ped", 0x5F3B70, bool (*)(const CPed& ped, CEntity& entity, CVector& point));
    RH_ScopedOverloadedInstall(ComputeClosestSurfacePoint, "posn", 0x5F36F0, bool(*)(const CVector&,CEntity&,CVector&), { .Reversed = false });
    RH_ScopedOverloadedInstall(ComputeClosestSurfacePoint, "rect", 0x5F2C10, bool(*)(const CVector&,const CVector*,CVector&));
    RH_ScopedInstall(ComputeEntityBoundingBoxCentreUncached, 0x5F1600);
    RH_ScopedInstall(ComputeEntityBoundingBoxCentreUncachedAll, 0x5F3B40);
    RH_ScopedInstall(ComputeEntityBoundingBoxCorners, 0x5F3650);
    RH_ScopedInstall(ComputeEntityBoundingBoxCornersUncached, 0x5F1FA0);
    RH_ScopedInstall(ComputeEntityBoundingBoxPlanes, 0x5F3660);
    RH_ScopedInstall(ComputeEntityBoundingBoxPlanesUncached, 0x5F1670);
    RH_ScopedInstall(ComputeEntityBoundingBoxPlanesUncachedAll, 0x5F2B80);
    RH_ScopedInstall(ComputeEntityBoundingBoxSegmentPlanes, 0x5F36A0);
    RH_ScopedInstall(ComputeEntityBoundingBoxSegmentPlanesUncached, 0x5F1750, { .Reversed = false });
    RH_ScopedInstall(ComputeEntityBoundingBoxSegmentPlanesUncachedAll, 0x5F2BC0);
    RH_ScopedInstall(ComputeEntityBoundingSphere, 0x5F3C20, { .Reversed = false });
    RH_ScopedInstall(ComputeMoveDirToAvoidEntity, 0x5F3730, { .Reversed = false });
    RH_ScopedInstall(ComputeEntityDirs, 0x5F1500, { .Reversed = false });
    RH_ScopedOverloadedInstall(ComputeEntityHitSide, "1", 0x5F3BC0, int32 (*)(const CPed& ped, CEntity& entity), {.Reversed = false});
    RH_ScopedOverloadedInstall(ComputeEntityHitSide, "2", 0x5F1450, int32 (*)(const CVector& point1, const CVector* point2, const float* x));
    RH_ScopedOverloadedInstall(ComputeEntityHitSide, "3", 0x5F3AC0, int32 (*)(const CVector& point, CEntity& entity), {.Reversed = false});
    RH_ScopedOverloadedInstall(ComputePedHitSide, "physical", 0x5F3640, int32(*)(const CPed&,const CPhysical&));
    RH_ScopedOverloadedInstall(ComputePedHitSide, "posn", 0x5F1E70, int32(*)(const CPed&,const CVector&));
    RH_ScopedInstall(ComputePedShotSide, 0x5F13F0, { .Reversed = false });
    RH_ScopedOverloadedInstall(ComputeRouteRoundEntityBoundingBox, "1", 0x5F6110, int32(*)(const CPed&,CEntity&,const CVector&,CPointRoute&,int32), { .Reversed = false });
    RH_ScopedOverloadedInstall(ComputeRouteRoundEntityBoundingBox, "2", 0x5F3DD0, int32(*)(const CPed&,const CVector&,CEntity&,const CVector&,CPointRoute&,int32), { .Reversed = false });
    RH_ScopedInstall(ComputeRouteRoundSphere, 0x5F1890, { .Reversed = false });
    RH_ScopedOverloadedInstall(GetIsLineOfSightClear, "ped", 0x5F5A30, bool(*)(const CPed&,const CVector&,CEntity&,float&), { .Reversed = false });
    RH_ScopedOverloadedInstall(GetIsLineOfSightClear, "v3d", 0x5F2F00, bool(*)(const CVector&,const CVector&,CEntity&));
    RH_ScopedInstall(GetNearestPed, 0x5F3590, { .Reversed = false });
    RH_ScopedInstall(IsEntityBlockingTarget, 0x5F3970, { .Reversed = false });
    RH_ScopedInstall(IsInAir, 0x5F1CB0, { .Reversed = false });
    RH_ScopedInstall(IsWanderPathClear, 0x5F2F70, { .Reversed = false });
    RH_ScopedInstall(LiesInsideBoundingBox, 0x5F3880, { .Reversed = false });
}

// 0x5F1B00
bool CPedGeometryAnalyser::CanPedJumpObstacle(const CPed& ped, const CEntity& entity) {
    return plugin::CallAndReturn<bool, 0x5F1B00, CPed const&, CEntity const&>(ped, entity);
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
void CPedGeometryAnalyser::ComputeClearTarget(const CPed& ped, const CVector& a2, CVector& a3) {
    return plugin::Call<0x5F5D80, const CPed&, const CVector&, CVector&>(ped, a2, a3);
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
    return plugin::CallAndReturn<bool, 0x5F36F0, const CVector&, CEntity&, CVector&>(posn, entity, point);
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
CVector* CPedGeometryAnalyser::ComputeEntityBoundingBoxSegmentPlanesUncached(const CVector* corners, CVector& center, CVector* a3, float* a4) {
    return plugin::CallAndReturn<CVector*, 0x5F1750, const CVector*, CVector&, CVector*, float*>(corners, center, a3, a4);
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
void CPedGeometryAnalyser::ComputeEntityBoundingSphere(const CPed& ped, CEntity& entity, CColSphere& a3) {
    return plugin::Call<0x5F3C20, const CPed&, CEntity&, CColSphere&>(ped, entity, a3);
}

// 0x5F3730
int32 CPedGeometryAnalyser::ComputeMoveDirToAvoidEntity(const CPed& ped, CEntity& entity, CVector& outDirToAvoidEntity) {
    return plugin::CallAndReturn<int32, 0x5F3730, const CPed&, CEntity&, CVector&>(ped, entity, outDirToAvoidEntity);
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
CVector* CPedGeometryAnalyser::ComputeEntityDirs(const CEntity& entity, CVector* posn) {
    return plugin::CallAndReturn<CVector*, 0x5F1500, const CEntity&, CVector*>(entity, posn);
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
    return plugin::CallAndReturn<int32, 0x5F3AC0, const CVector&, CEntity&>(point, entity);
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
    return plugin::CallAndReturn<int32, 0x5F13F0, const CPed&, const CVector&>(ped, posn);
}

// 0x5F6110
int32 CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox(const CPed& ped, CEntity& entity, const CVector& posn, CPointRoute& pointRoute, int32 a5) {
    return ComputeRouteRoundEntityBoundingBox(ped, ped.GetPosition(), entity, posn, pointRoute, a5);
}

// 0x5F3DD0
int32 CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox(const CPed& ped, const CVector& a2, CEntity& entity, const CVector& a4, CPointRoute& pointRoute, int32 a6){
    return plugin::CallAndReturn<int32, 0x5F3DD0, const CPed&, const CVector&, CEntity&, const CVector&, CPointRoute&, int32>(ped, a2, entity, a4, pointRoute, a6);
}

// 0x5F1890
bool CPedGeometryAnalyser::ComputeRouteRoundSphere(const CPed& ped, const CColSphere& sphere, const CVector& a3, const CVector& a4, CVector& a5, CVector& a6) {
    return plugin::CallAndReturn<bool, 0x5F1890, const CPed&, const CColSphere&, const CVector&, const CVector&, CVector&, CVector&>(ped, sphere, a3, a4, a5, a6);
}

// 0x5F5A30
bool CPedGeometryAnalyser::GetIsLineOfSightClear(const CPed& ped, const CVector& a2, CEntity& entity, float& a4) {
    return plugin::CallAndReturn<bool, 0x5F5A30, const CPed&, const CVector&, CEntity&, float&>(ped, a2, entity, a4);
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
    return plugin::CallAndReturn<CPed*, 0x5F3590, const CVector&>(point);
}

// 0x5F3970
bool CPedGeometryAnalyser::IsEntityBlockingTarget(CEntity* entity, const CVector& point, float distance) {
    return plugin::CallAndReturn<bool, 0x5F3970, CEntity*, const CVector&, float>(entity, point, distance);
}

// 0x5F1CB0
bool CPedGeometryAnalyser::IsInAir(const CPed& ped) {
    return plugin::CallAndReturn<bool, 0x5F1CB0, const CPed&>(ped);
}

// 0x5F2F70
CPedGeometryAnalyser::WanderPathClearness CPedGeometryAnalyser::IsWanderPathClear(const CVector& from, const CVector& to, float maxHeightChange, int32 maxSamples) {
    return plugin::CallAndReturn<WanderPathClearness, 0x5F2F70, const CVector&, const CVector&, float, int32>(from, to, maxHeightChange, maxSamples);
}

// 0x5F3880
bool CPedGeometryAnalyser::LiesInsideBoundingBox(const CPed& ped, const CVector& posn, CEntity& entity) {
    return plugin::CallAndReturn<bool, 0x5F3880, const CPed&, const CVector&, CEntity&>(ped, posn, entity);
}

// 0x41B7C0
void* CPointRoute::operator new(uint32 size) {
    return GetPointRoutePool()->New();
}

// 0x41B7D0
void CPointRoute::operator delete(void* ptr, size_t sz) {
    GetPointRoutePool()->Delete(reinterpret_cast<CPointRoute*>(ptr));
}
