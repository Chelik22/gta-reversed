#include "StdInc.h"

#include "Rope.h"
#include "Ropes.h"

void CRope::InjectHooks() {
    RH_ScopedClass(CRope);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(ReleasePickedUpObject, 0x556030);
    RH_ScopedInstall(CreateHookObjectForRope, 0x556070);
    RH_ScopedInstall(UpdateWeightInRope, 0x5561B0);
    RH_ScopedInstall(Remove, 0x556780);
    RH_ScopedInstall(Render, 0x556800);
    RH_ScopedInstall(PickUpObject, 0x5569C0);
    RH_ScopedInstall(Update, 0x557530);
}

// inlined see 0x557959
// use switch like in Android?
// 0x555FB0
bool CRope::DoControlsApply() const {
    return    m_nType == eRopeType::CRANE_MAGNO && CRopes::PlayerControlsCrane == eControlledCrane::MAGNO_CRANE
           || m_nType == eRopeType::WRECKING_BALL && CRopes::PlayerControlsCrane == eControlledCrane::WRECKING_BALL
           || m_nType == eRopeType::CRANE_TROLLEY && CRopes::PlayerControlsCrane == eControlledCrane::LAS_VEGAS_CRANE
           || m_nType == eRopeType::QUARRY_CRANE_ARM && CRopes::PlayerControlsCrane == eControlledCrane::QUARRY_CRANE
           || m_nType == eRopeType::CRANE_MAGNET1
           || m_nType == eRopeType::MAGNET
           || m_nType == eRopeType::CRANE_HARNESS;
}

// 0x556030
void CRope::ReleasePickedUpObject() {
    if (m_pRopeAttachObject) {
        m_pRopeAttachObject->AsPhysical()->physicalFlags.bAttachedToEntity = false;
        m_pRopeAttachObject->AsPhysical()->physicalFlags.bCarriedByRope = false;
        m_pRopeAttachObject = nullptr;
    }
    m_pAttachedEntity->SetUsesCollision(true);
    m_nFlags1 = 60; // 6th, 7th bits set
}

// 0x556070
void CRope::CreateHookObjectForRope() {
    if (m_pAttachedEntity)
        return;

    using namespace ModelIndices;

    const auto modelIndex = [&]() -> ModelIndex {
        switch (m_nType) {
        case eRopeType::CRANE_MAGNET1:
        case eRopeType::CRANE_MAGNO:
        case eRopeType::QUARRY_CRANE_ARM:
        case eRopeType::CRANE_TROLLEY:
            return MI_CRANE_MAGNET;
        case eRopeType::CRANE_HARNESS:
            return MI_CRANE_HARNESS;
        case eRopeType::MAGNET:
            return MI_MINI_MAGNET;
        case eRopeType::WRECKING_BALL:
            return MI_WRECKING_BALL;
        case eRopeType::SWAT:
            return MODEL_INVALID; // Just so the assert below wont be hit.
        default:
            NOTSA_UNREACHABLE(); //assert(0);
        }
    }();
    if (modelIndex == ModelIndex{ MODEL_INVALID }) { // Must do it like this because `ModelIndex` is u16, `MODEL_ID` is i32, and u16 -1 casted to int32 is 0xffff...
        return;
    }

    auto* obj = new CObject(modelIndex, true);
    m_pAttachedEntity = obj;

    obj->RegisterReference(reinterpret_cast<CEntity**>(&m_pAttachedEntity));
    obj->SetPosn(m_aSegments[NUM_ROPE_SEGMENTS - 1]);
    obj->m_nObjectType = OBJECT_TYPE_DECORATION;
    obj->SetIsStatic(false);
    obj->physicalFlags.bAttachedToEntity = true;

    CWorld::Add(m_pAttachedEntity);

    m_pRopeAttachObject = nullptr;
    m_nFlags1 = 0;
}

// Pulls the free end of the rope towards (a2, a3, a4).
// Returns true when the target is unreachable: the end gets clamped, *a6 receives the clamped
// position and the free segments are lined up towards it.
// Returns false when the target is reachable: 6 relaxation passes + a final bottom-up pass.
// ABI: signature from Rope.h (float a2..a4, int32 a5, float* a6). In the original a5 is never
// used (CRope::Update passes the bit pattern 0x3DCCCCCD == 0.1f there, so it used to be a float).
// 1.0f from 0x858624
// 0x5561B0
int8 CRope::UpdateWeightInRope(float a2, float a3, float a4, int32 a5 /* unused */, CVector* outPos) {
    const auto  segCount = m_nSegments;
    const auto& tipPos   = m_aSegments[segCount]; // free end

    m_aSegments[NUM_ROPE_SEGMENTS - 1] = CVector{ a2, a3, a4 }; // target point is written into the last segment

    const CVector dirToTarget{ a2 - tipPos.x, a3 - tipPos.y, a4 - tipPos.z };
    const auto    maxReach = (float)(NUM_ROPE_SEGMENTS - 1 - segCount) * m_fTotalLength;

    if (maxReach <= dirToTarget.Magnitude()) {
        // Target unreachable - clamp and line up the chain of segments up to the clamped point
        const auto scale = maxReach / dirToTarget.Magnitude();
        *outPos = tipPos + dirToTarget * scale;

        auto dir = dirToTarget;
        for (auto i = segCount + 1; i < (int32)NUM_ROPE_SEGMENTS; i++) {
            dir.Normalise();
            dir *= m_fTotalLength;
            m_aSegments[i] = tipPos + dir * (float)(i - segCount);
        }
        return true;
    }

    // Target reachable: 6 relaxation passes.
    // Pass A: top-down (pairs i,i-1 for i=31..m+2) - moves the upper segment of the pair;
    // breaks out on the first link that is short enough. Writes the segment's speed.
    // Pass B: from the tip upwards (pairs i,i+1 for i=m+1..30) - moves the lower segment of the pair.
    for (auto pass = 0; pass < 6; pass++) {
        for (auto i = (int32)NUM_ROPE_SEGMENTS - 1; i > segCount + 1; i--) {
            const auto delta = m_aSegments[i] - m_aSegments[i - 1];
            if (delta.Magnitude() <= m_fTotalLength)
                break;

            const auto oldPos = m_aSegments[i - 1];
            const auto scale  = m_fTotalLength / delta.Magnitude();
            m_aSegments[i - 1] = m_aSegments[i] + (m_aSegments[i - 1] - m_aSegments[i]) * scale;
            m_aSpeed[i - 1]    = (m_aSegments[i - 1] - oldPos) * (1.0f / CTimer::ms_fTimeStep);
        }

        for (auto i = segCount + 1; i < (int32)NUM_ROPE_SEGMENTS - 1; i++) {
            const auto delta = m_aSegments[i] - m_aSegments[i - 1];
            if (delta.Magnitude() > m_fTotalLength)
                m_aSegments[i] = m_aSegments[i - 1] + delta * (m_fTotalLength / delta.Magnitude());
        }
    }

    // Final pass: pairs (k,k+1) for k=30..m+2, while the link is longer than m_fTotalLength -
    // pull seg[k] towards seg[k+1]; stop at the first satisfied link.
    for (auto k = (int32)NUM_ROPE_SEGMENTS - 2; k > segCount + 1; k--) {
        const auto delta = m_aSegments[k + 1] - m_aSegments[k];
        if (delta.Magnitude() <= m_fTotalLength)
            break;

        const auto scale = m_fTotalLength / delta.Magnitude();
        m_aSegments[k] = m_aSegments[k + 1] + (m_aSegments[k] - m_aSegments[k + 1]) * scale;
    }
    return false;
}

// 0x556780
void CRope::Remove() {
    m_nType = eRopeType::NONE;
    if (m_pRopeAttachObject)
        ReleasePickedUpObject();

    if (m_pAttachedEntity) {
        CWorld::Remove(m_pAttachedEntity);
        delete m_pAttachedEntity;
        m_pAttachedEntity = nullptr;
    }
}

// 0x556800
void CRope::Render() {
    // Note: Probably needs adjustments if `NUM_ROPE_SEGMENTS` is changed
    if (!TheCamera.IsSphereVisible(m_aSegments[NUM_ROPE_SEGMENTS / 2], 20.0f))
        return;

    if ((TheCamera.GetPosition() - m_aSegments[0]).Magnitude2D() >= 120.0f)
        return;

    DefinedState();

    const auto GetVertex = [](unsigned i) {
        return &TempBufferVertices.m_3d[i];
    };

    const RwRGBA color = { 0, 0, 0, 128 };
    for (auto i = 0u; i < NUM_ROPE_SEGMENTS; i++) {
        RxObjSpace3DVertexSetPreLitColor(GetVertex(i), &color);
        RxObjSpace3DVertexSetPos(GetVertex(i), &m_aSegments[i]);
    }

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(FALSE));

    if (RwIm3DTransform(TempBufferVertices.m_3d, NUM_ROPE_SEGMENTS, nullptr, 0)) {
        RxVertexIndex indices[] = { // *(RxVertexIndex(*)[64])0x8CD818
            0,  1,  1,  2,  2,  3,  3,  4,
            4,  5,  5,  6,  6,  7,  7,  8,
            8,  9,  9,  10, 10, 11, 11, 12,
            12, 13, 13, 14, 14, 15, 15, 16,
            16, 17, 17, 18, 18, 19, 19, 20,
            20, 21, 21, 22, 22, 23, 23, 24,
            24, 25, 25, 26, 26, 27, 27, 28,
            28, 29, 29, 30, 30, 31, 31, 32
        };
        RwIm3DRenderIndexedPrimitive(rwPRIMTYPELINELIST, indices, std::size(indices) - 2); // the last two indexes are not used
        RwIm3DEnd();
    }

    if (m_nType == eRopeType::QUARRY_CRANE_ARM) {
        const CVector pos[] = { m_aSegments[0], { 709.32f, 916.20f, 53.0f } }; // Hunter Quarry
        for (auto i = 0u; i < std::size(pos); i++) {
            RxObjSpace3DVertexSetPreLitColor(GetVertex(i), &color);
            RxObjSpace3DVertexSetPos(GetVertex(i), &pos[i]);
        }
        if (RwIm3DTransform(TempBufferVertices.m_3d, std::size(pos), nullptr, 0)) {
            RxVertexIndex indices[] = { 0, 1 };
            RwIm3DRenderIndexedPrimitive(rwPRIMTYPELINELIST, indices, std::size(indices));
            RwIm3DEnd();
        }
    }
}

// 0x5569C0
void CRope::PickUpObject(CEntity* obj) {
    if (m_pRopeAttachObject == obj)
        return;

    if (m_pRopeAttachObject)
        ReleasePickedUpObject();

    obj->RegisterReference(&m_pAttachedEntity);
    m_pRopeAttachObject = obj;

    // TODO: Move model => world space translation into CEntity
    // MultiplyMatrixWithVector should be used here
    CVector height = { {}, {}, CRopes::FindPickupHeight(obj) };
    m_pAttachedEntity->SetPosn(obj->GetPosition() + obj->GetMatrix().TransformVector(height));
    m_pAttachedEntity->SetUsesCollision(false);

    obj->AsPhysical()->physicalFlags.bAttachedToEntity = true;
    if (obj->GetIsTypeVehicle()) {
        if (obj->GetStatus() == STATUS_SIMPLE)
        {
            obj->SetStatus(STATUS_PHYSICS);
        }
    } else if (obj->GetIsTypeObject()) {
        if (obj->GetIsStatic()) {
            obj->AsObject()->SetIsStatic(false);
            obj->AsObject()->AddToMovingList();
            obj->AsObject()->m_nFakePhysics = 0;
        }
    }
}

// Verlet physics of the free rope nodes, crane controls, magnet pickup logic.
// Speed retention factor: pow(0.8, ms_fTimeStep) (0x863E40 + pow 0x822130);
// wind jitter: (rand() & 0xF - 8) * 0.001f (0x858CDC); gravity: 0.15f (0x858FCC);
// rope elasticity: 0.06f (0x8CD898) / with pickup 0.13f * 1/1200 (0x863E30) * mass + 0.06f;
// mission audio event 104 (0x5073B0); release/drop thresholds - see inlined constants below.
// 0x557530
void CRope::Update() {
    // Speed retention factor: pow(0.8, ms_fTimeStep)
    const auto dtRetention = (float)std::pow(0.8, (double)CTimer::ms_fTimeStep);

    // 1) 2D distance to the camera (200.0f from 0x858A48)
    if ((TheCamera.GetPosition() - m_aSegments[0]).Magnitude2D() >= 200.0f)
        return;

    // 2) Expired rope: the top node falls
    if (!(m_nFlags2 & 1) && m_nTime < CTimer::GetTimeInMS()) {
        m_aSpeed[0].z -= CTimer::ms_fTimeStep * 0.0015f; // 0x863E38
        m_aSegments[0] += m_aSpeed[0] * CTimer::ms_fTimeStep;
    }

    // 3) Ground (every 8 frames) / keep magnet load above ground
    if ((m_nFlags2 & 4) && (CTimer::m_FrameCounter & 7) == 2)
        m_fGroundZ = CWorld::FindGroundZFor3DCoord(m_aSegments[0]);

    if ((m_nFlags2 & 4) && m_pRopeAttachObject
        && (m_pRopeAttachObject->GetIsTypeVehicle()
            || m_pRopeAttachObject->m_nModelIndex == ModelIndices::MI_OBJECTFORMAGNOCRANE1
            || m_pRopeAttachObject->m_nModelIndex == ModelIndices::MI_OBJECTFORMAGNOCRANE2
            || m_pRopeAttachObject->m_nModelIndex == ModelIndices::MI_OBJECTFORMAGNOCRANE3
            || m_pRopeAttachObject->m_nModelIndex == ModelIndices::MI_OBJECTFORMAGNOCRANE5)) {
        m_fGroundZ = m_pAttachedEntity->GetPosition().z - 0.5f; // 0x858B8C
    }

    // 4) Verlet physics of the free nodes (i > m_nSegments)
    for (auto i = m_nSegments + 1u; i < NUM_ROPE_SEGMENTS; i++) {
        const auto oldPos = m_aSegments[i];

        m_aSpeed[i].x += (float)((rand() & 0xF) - 8) * 0.001f; // 0x858CDC
        m_aSpeed[i].y += (float)((rand() & 0xF) - 8) * 0.001f;

        m_aSpeed[i] = m_aSpeed[i] * dtRetention + m_aSpeed[i - 1] * (1.0f - dtRetention);

        m_aSegments[i].z -= CTimer::ms_fTimeStep * 0.15f; // 0x858FCC
        if (m_nFlags2 & 4)
            m_aSegments[i].z = std::max(m_aSegments[i].z, m_fGroundZ + 0.3f); // 0x858C24

        const auto delta = m_aSegments[i] - m_aSegments[i - 1];
        m_aSegments[i] = m_aSegments[i - 1] + delta * (m_fTotalLength / delta.Magnitude());

        m_aSpeed[i] = (m_aSegments[i] - oldPos) * (1.0f / CTimer::ms_fTimeStep);
    }

    // 5) Crane controls
    const bool bMagnetish = m_nType == eRopeType::CRANE_MAGNET1
                         || m_nType == eRopeType::CRANE_HARNESS
                         || m_nType == eRopeType::MAGNET;
    const bool bCraneControlled =
           (m_nType == eRopeType::CRANE_MAGNO       && CRopes::PlayerControlsCrane == eControlledCrane::MAGNO_CRANE)
        || (m_nType == eRopeType::WRECKING_BALL     && CRopes::PlayerControlsCrane == eControlledCrane::WRECKING_BALL)
        || (m_nType == eRopeType::CRANE_TROLLEY     && CRopes::PlayerControlsCrane == eControlledCrane::LAS_VEGAS_CRANE)
        || (m_nType == eRopeType::QUARRY_CRANE_ARM  && CRopes::PlayerControlsCrane == eControlledCrane::QUARRY_CRANE);

    if (bMagnetish || bCraneControlled) {
        if (bMagnetish) {
            const auto stick = CPad::GetPad()->GetCarGunUpDown();
            if ((stick < 0 && CTheScripts::bEnableCraneRaise) || (stick > 0 && CTheScripts::bEnableCraneLower))
                m_fSegmentLength -= (float)stick * CTimer::ms_fTimeStep * 0.00001f; // 0x858C14
            m_fSegmentLength = std::min(m_fSegmentLength, 0.84f); // 0x863E34
        } else {
            const auto* pad = CPad::GetPad();
            if (CTheScripts::bEnableCraneRaise) {
                const auto delta = (float)pad->NewState.ButtonSquare * CTimer::ms_fTimeStep * 0.00001f;
                if (delta > 0.0f && m_fSegmentLength + delta < 0.9f)
                    AudioEngine.ReportMissionAudioEvent(104 /* 0x68 */, m_pRopeHolder->AsPhysical(), 0.0f, 1.0f);
                m_fSegmentLength += delta;
            }
            if (CTheScripts::bEnableCraneLower) {
                const auto delta = (float)pad->NewState.ButtonCross * CTimer::ms_fTimeStep * 0.00001f;
                if (delta > 0.0f && 0.01f < m_fSegmentLength - delta) // 0x858C58
                    AudioEngine.ReportMissionAudioEvent(104 /* 0x68 */, m_pRopeHolder->AsPhysical(), 0.0f, 1.0f);
                m_fSegmentLength -= delta;
            }
        }
    }
    m_fSegmentLength = std::clamp(m_fSegmentLength, 0.01f, 0.9f); // 0x858C58 / 0x858C20

    // 6) Rope elasticity
    auto tipEntity = m_pRopeAttachObject; // entity hanging on the rope end
    float elasticity = 0.06f;
    if (!tipEntity) {
        tipEntity = m_pAttachedEntity;
        if (tipEntity)
            tipEntity->SetUsesCollision(true);
    } else {
        auto mass = tipEntity->AsPhysical()->m_fMass;
        if (tipEntity->m_nModelIndex == MODEL_SECURICA)
            mass = 750.0f; // 0x859A98
        elasticity = std::min(0.13f * 0.00083333335f * mass + 0.06f, 0.5f); // 1/1200 (0x863E30), 0.5f (0x858B8C)
        tipEntity->AsPhysical()->m_nFakePhysics = 0;
    }

    // 7) The rope end is pulled towards tipEntity
    if (tipEntity) {
        CVector clampedPos;
        if (UpdateWeightInRope(tipEntity->GetPosition().x, tipEntity->GetPosition().y, tipEntity->GetPosition().z, 0x3DCCCCCD /* 0.1f, unused by ABI */, &clampedPos)) {
            tipEntity->GetPosition() = clampedPos;

            auto ropeTopSpeed = m_pRopeHolder->AsPhysical()->GetSpeed(m_aSegments[0] - m_pRopeHolder->GetPosition()); // 0x542CE0
            if (bCraneControlled)
                ropeTopSpeed.Set(0.0f, 0.0f, 0.0f);

            auto velocity = tipEntity->AsPhysical()->m_vecMoveSpeed - ropeTopSpeed;

            auto dir = tipEntity->GetPosition() - m_aSegments[0];
            dir.Normalise();

            if (const auto dot = DotProduct(dir, velocity); dot > 0.0f)
                velocity -= dir * dot;

            const auto speedDelta = (velocity + ropeTopSpeed) - tipEntity->AsPhysical()->m_vecMoveSpeed;
            tipEntity->AsPhysical()->m_vecMoveSpeed     += speedDelta * (1.0f - elasticity);
            m_pRopeHolder->AsPhysical()->m_vecMoveSpeed -= speedDelta * elasticity;

            // Orient tipEntity along the rope (0x59B7E0 - CMatrix::ForceUpVector)
            if (!m_pRopeAttachObject) {
                tipEntity->GetMatrix().ForceUpVector(-dir);
            } else {
                // Blend of old/new orientation 0.9f (0x858C20) / 0.1f (0x858B1C);
                // the CMatrix copy ctor copies only the matrix data, like the original 0x40-byte copy (0x59BCF0)
                const CMatrix matBefore = tipEntity->GetMatrix();
                tipEntity->GetMatrix().ForceUpVector(-dir);
                const CMatrix matAfter = tipEntity->GetMatrix();
                auto& mat = tipEntity->GetMatrix();
                mat.GetRight()   = matBefore.GetRight()   * 0.9f + matAfter.GetRight()   * 0.1f;
                mat.GetForward() = matBefore.GetForward() * 0.9f + matAfter.GetForward() * 0.1f;
                mat.GetUp()      = matBefore.GetUp()      * 0.9f + matAfter.GetUp()      * 0.1f;
            }
        }

        // 8) The hook follows the picked up object (Y-component of bbox as Z-offset - so in the original) (0x59BBC0/0x59C890/0x4241C0)
        if (m_pRopeAttachObject) {
            m_pAttachedEntity->GetMatrix() = m_pRopeAttachObject->GetMatrix();
            m_pAttachedEntity->AsPhysical()->m_vecMoveSpeed = m_pRopeAttachObject->AsPhysical()->m_vecMoveSpeed;

            const auto offZ = CModelInfo::ms_modelInfoPtrs[m_pRopeAttachObject->m_nModelIndex]
                                 ->GetColModel()->m_boundBox.m_vecMax.y;
            m_pAttachedEntity->SetPosn(m_pRopeAttachObject->GetMatrix().TransformPoint(CVector{ 0.0f, 0.0f, offZ }));
        }
    }

    // 9a) Something is picked up - release checks
    if (m_pRopeAttachObject) {
        if ((bMagnetish || bCraneControlled) && CTheScripts::bEnableCraneRelease) {
            if (CPad::GetPad()->CarGunJustDown()) // 0x53FFE0
                ReleasePickedUpObject();
        }

        if (m_pRopeAttachObject->AsPhysical()->physicalFlags.bRenderScorched /* PHYSICAL_DESTROYED 0x20000000 */) {
            m_pRopeAttachObject->AsPhysical()->physicalFlags.bAttachedToEntity = false;
            m_pRopeAttachObject->AsPhysical()->physicalFlags.bCarriedByRope    = false;
            m_pAttachedEntity->SetUsesCollision(true);
            m_pRopeAttachObject = nullptr;
            m_nFlags1 = 60; // pause before a new scan
        }

        if (m_pRopeAttachObject && m_pRopeAttachObject->GetIsTypeVehicle()) {
            const auto veh = m_pRopeAttachObject->AsVehicle();
            if (veh->m_nVehicleSubType == VEHICLE_TYPE_BIKE && veh->m_pDriver)
                ReleasePickedUpObject();
        }
    } else if (m_nFlags1) {
        m_nFlags1--;
    } else {
        // 9b) Scan for new pickup targets:
        // CRANE_HARNESS - peds only; NONE/WRECKING_BALL/SWAT - nothing;
        // CRANE_TROLLEY - objects only; the rest - vehicles + objects
        const CVector ropeTip = m_aSegments[NUM_ROPE_SEGMENTS - 1];

        if (m_nType == eRopeType::CRANE_HARNESS) {
            // Peds (pool iterated backwards)
            auto* pool = GetPedPool();
            for (int32 i = pool->GetSize() - 1; i >= 0; i--) {
                const auto ped = pool->GetAt(i);
                if (!ped || ped->m_nPedState == PEDSTATE_DEAD || ped->IsPlayer() || ped->bInVehicle)
                    continue;

                if ((ped->GetPosition() - ropeTip).Magnitude() >= 2.5f) // 0x858FA0
                    continue;

                m_pRopeAttachObject = ped;
                ped->RegisterReference(&m_pRopeAttachObject); // 0x571B70
                ped->physicalFlags.bCarriedByRope = true;

                const auto offZ = CModelInfo::ms_modelInfoPtrs[ped->m_nModelIndex]->GetColModel()->m_boundBox.m_vecMax.y;
                const auto pt   = ped->GetMatrix().TransformPoint(CVector{ 0.0f, 0.0f, offZ });
                m_pAttachedEntity->SetPosn(pt);
                m_pAttachedEntity->SetUsesCollision(false);

                if (ropeTip.z - pt.z > 0.0f)
                    m_fSegmentLength = std::max(m_fSegmentLength - (ropeTip.z - pt.z) / m_fMass - m_fTotalLength / m_fMass, 0.01f);
                break;
            }
        } else if (m_nType != eRopeType::NONE && m_nType != eRopeType::WRECKING_BALL && m_nType != eRopeType::SWAT) {
            const bool bWantMagno     = m_nType == eRopeType::CRANE_MAGNO;
            const bool bWantSiteCrane = m_nType == eRopeType::CRANE_TROLLEY;
            const bool bWantQuarry    = m_nType == eRopeType::QUARRY_CRANE_ARM;
            const bool bWantWongDish  = m_nType == eRopeType::MAGNET;
            const bool bWantKmbRock   = m_nType == eRopeType::MAGNET;
            const bool bWantKmbPlank  = m_nType == eRopeType::MAGNET;
            const bool bWantKmbBomb   = m_nType == eRopeType::MAGNET;
            const bool bMagnetOnlyRC  = m_nType == eRopeType::MAGNET; // From vehicles - MODEL_RCTIGER only

            // Vehicles (types 1,3,4,6; 7 goes straight to objects)
            if (m_nType != eRopeType::CRANE_TROLLEY) {
                auto* pool = GetVehiclePool();
                for (int32 i = pool->GetSize() - 1; i >= 0; i--) {
                    const auto veh = pool->GetAt(i);
                    if (!veh)
                        continue;

                    const auto subType = veh->m_nVehicleSubType;
                    const bool bEligible = subType == VEHICLE_TYPE_AUTOMOBILE
                                        || subType == VEHICLE_TYPE_MTRUCK
                                        || (subType == VEHICLE_TYPE_BIKE && !veh->m_pDriver)
                                        || veh->m_nModelIndex == MODEL_DINGHY
                                        || veh->m_nModelIndex == MODEL_VORTEX;
                    if (!bEligible)
                        continue;
                    if (veh->physicalFlags.bRenderScorched /* PHYSICAL_DESTROYED, 0x20000000 */)
                        continue;
                    if (!veh->vehicleFlags.bWinchCanPickMeUp)
                        continue;
                    if (bMagnetOnlyRC && veh->m_nModelIndex != MODEL_RCTIGER)
                        continue;
                    if (veh->m_ropeType != 0)
                        continue;

                    const auto offZ = CModelInfo::ms_modelInfoPtrs[veh->m_nModelIndex]->GetColModel()->m_boundBox.m_vecMax.y;
                    const auto pt   = veh->GetMatrix().TransformPoint(CVector{ 0.0f, 0.0f, offZ });
                    if ((pt - ropeTip).Magnitude() >= 2.5f)
                        continue;

                    m_pRopeAttachObject = veh;
                    veh->RegisterReference(&m_pRopeAttachObject);
                    veh->physicalFlags.bCarriedByRope = true;
                    if (veh->GetStatus() == STATUS_SIMPLE)
                        veh->SetStatus(STATUS_PHYSICS);
                    m_pAttachedEntity->SetPosn(pt); // TransformPoint repeated in the original, value is the same
                    m_pAttachedEntity->SetUsesCollision(false);

                    if (ropeTip.z - pt.z > 0.0f) {
                        m_fSegmentLength -= (ropeTip.z - pt.z) / m_fMass + m_fTotalLength / m_fMass;
                        m_fSegmentLength = std::max(m_fSegmentLength, 0.01f);
                    }
                    break;
                }
            }

            // Objects (types 3,4,6,7)
            if (bWantMagno || bWantSiteCrane || bWantQuarry || bWantWongDish || bWantKmbRock || bWantKmbPlank || bWantKmbBomb) {
                auto* pool = GetObjectPool();
                for (int32 i = pool->GetSize() - 1; i >= 0; i--) {
                    const auto obj = pool->GetAt(i);
                    if (!obj)
                        continue;
                    if (!obj->objectFlags.bCanBeAttachedToMagnet)
                        continue;

                    const auto model = obj->m_nModelIndex;
                    const bool bMatch =
                           (bWantMagno     && (model == ModelIndices::MI_OBJECTFORMAGNOCRANE1
                                            || model == ModelIndices::MI_OBJECTFORMAGNOCRANE2
                                            || model == ModelIndices::MI_OBJECTFORMAGNOCRANE3
                                            || model == ModelIndices::MI_OBJECTFORMAGNOCRANE4
                                            || model == ModelIndices::MI_OBJECTFORMAGNOCRANE5))
                        || (bWantSiteCrane && model == ModelIndices::MI_OBJECTFORBUILDINGSITECRANE1)
                        || (bWantQuarry    && (model == ModelIndices::MI_QUARY_ROCK1
                                            || model == ModelIndices::MI_QUARY_ROCK2
                                            || model == ModelIndices::MI_QUARY_ROCK3
                                            || model == ModelIndices::MI_DEAD_TIED_COP)
                                            && !obj->m_pAttachedTo)
                        || (bWantWongDish  && model == ModelIndices::MI_WONG_DISH)
                        || (bWantKmbRock   && model == ModelIndices::MI_KMB_ROCK)
                        || (bWantKmbPlank  && model == ModelIndices::MI_KMB_PLANK)
                        || (bWantKmbBomb   && model == ModelIndices::MI_KMB_BOMB);
                    if (!bMatch)
                        continue;

                    const auto offZ = CModelInfo::ms_modelInfoPtrs[obj->m_nModelIndex]->GetColModel()->m_boundBox.m_vecMax.y;
                    auto       best = obj->GetMatrix().TransformPoint(CVector{ 0.0f, 0.0f, offZ });
                    auto       flips = 0;
                    if (model == ModelIndices::MI_OBJECTFORMAGNOCRANE1
                     || model == ModelIndices::MI_OBJECTFORMAGNOCRANE2
                     || model == ModelIndices::MI_OBJECTFORMAGNOCRANE3
                     || model == ModelIndices::MI_OBJECTFORMAGNOCRANE5) { // note: the 4th model is NOT checked here (so in the original)
                        // Pick the candidate with highest z among points along the object's at axis (repeats - so in the original)
                        if (const auto c = obj->GetMatrix().TransformPoint(CVector{ 0.0f, 0.0f, -offZ }); c.z > best.z) { best = c; flips = 2; }
                        if (const auto c = obj->GetMatrix().TransformPoint(CVector{ 0.0f, 0.0f,  offZ }); c.z > best.z) { best = c; flips = 3; }
                        if (const auto c = obj->GetMatrix().TransformPoint(CVector{ 0.0f, 0.0f, -offZ }); c.z > best.z) { best = c; flips = 1; }
                    }

                    if ((best - ropeTip).Magnitude() >= 2.5f)
                        continue;

                    m_pRopeAttachObject = obj;
                    obj->RegisterReference(&m_pRopeAttachObject);
                    obj->physicalFlags.bCarriedByRope = true;

                    m_pAttachedEntity->SetPosn(obj->GetMatrix().TransformPoint(CVector{ 0.0f, 0.0f, offZ }));
                    m_pAttachedEntity->SetUsesCollision(false);

                    if (obj->m_bIsStatic || obj->m_bIsStaticWaitingForCollision) {
                        obj->SetIsStatic(false);
                        obj->AddToMovingList();
                    }
                    obj->physicalFlags.bAttachedToEntity = true;

                    if (ropeTip.z - best.z > 0.0f) {
                        m_fSegmentLength -= (ropeTip.z - best.z) / m_fMass + m_fTotalLength / m_fMass;
                        m_fSegmentLength = std::max(m_fSegmentLength, 0.01f);
                    }

                    // Flip the object (swap right/up with inversion)
                    for (; flips != 0; flips--) {
                        const auto r = obj->GetMatrix().GetRight();
                        obj->GetMatrix().GetRight() = obj->GetMatrix().GetUp();
                        obj->GetMatrix().GetUp()    = -r;
                    }
                    break;
                }
            }
        }
    }

    // 10) Tail
    if (!(m_nFlags2 & 1) && m_aSegments[0].z < -50.0f) // 0x859B60
        Remove();

    m_nFlags2 &= ~0x1u;
}
