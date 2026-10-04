#include "StdInc.h"

#include "Rope.h"
#include "Ropes.h"

void CRope::InjectHooks() {
    RH_ScopedClass(CRope);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(ReleasePickedUpObject, 0x556030);
    RH_ScopedInstall(CreateHookObjectForRope, 0x556070);
    RH_ScopedInstall(UpdateWeightInRope, 0x5561B0, { .reversed = false });
    RH_ScopedInstall(Remove, 0x556780);
    RH_ScopedInstall(Render, 0x556800);
    RH_ScopedInstall(PickUpObject, 0x5569C0);
    RH_ScopedInstall(Update, 0x557530, { .reversed = false });
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
        default:
            return ModelIndex{ MODEL_INVALID }; // Original: unknown type -> do nothing (0x556070 default label just returns)
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
// 0x5561B0
int8 CRope::UpdateWeightInRope(float newX, float newY, float newZ, int32, float* outPos) {
    const auto    heldSegIdx = m_nSegments;
    const CVector heldPos    = m_aSegments[heldSegIdx];

    // Move the free end of the rope
    m_aSegments[NUM_ROPE_SEGMENTS - 1] = CVector{ newX, newY, newZ };

    const CVector delta  = CVector{ newX, newY, newZ } - heldPos;
    const float   maxLen = (float)(NUM_ROPE_SEGMENTS - 1 - heldSegIdx) * m_fTotalLength; // this + 0x30C

    if (maxLen <= delta.Magnitude()) {
        // Rope is stretched: clamp the new point onto the sphere around the held segment
        const float scale = maxLen / delta.Magnitude();
        const auto  outVec = delta * scale + heldPos; // Original writes a full CVector at `outPos`
        outPos[0] = outVec.x;
        outPos[1] = outVec.y;
        outPos[2] = outVec.z;

        // And fill the rope linearly between the held segment and the new point
        auto dir = delta;
        dir.Normalise();
        for (auto i = heldSegIdx + 1u; i < NUM_ROPE_SEGMENTS; i++) {
            m_aSegments[i] = heldPos + dir * (m_fTotalLength * (float)(i - heldSegIdx));
        }
        return 1;
    }

    // Relax the rope (6 iterations of backward/forward constraint passes)
    for (auto rep = 0; rep < 6; rep++) {
        // Backward pass: from the free end down; record the correction as a velocity
        for (int32 i = NUM_ROPE_SEGMENTS - 1; i > heldSegIdx + 1; i--) {
            const float dist = (m_aSegments[i] - m_aSegments[i - 1]).Magnitude();
            if (dist <= m_fTotalLength) {
                break; // Constraint satisfied
            }
            const auto scale    = m_fTotalLength / dist;
            const auto oldPos  = m_aSegments[i - 1];
            m_aSegments[i - 1] = m_aSegments[i] + (oldPos - m_aSegments[i]) * scale;
            m_aSpeed[i - 1]    = (m_aSegments[i - 1] - oldPos) * (1.0f / CTimer::GetTimeStep()); // 0x858624 / 0xB7CB5C
        }

        // Forward pass: pairs (heldSeg, heldSeg+1) .. (29, 30) - segment 31 is never touched
        for (int32 j = heldSegIdx; j < NUM_ROPE_SEGMENTS - 2; j++) {
            const float dist = (m_aSegments[j + 1] - m_aSegments[j]).Magnitude();
            if (dist >= m_fTotalLength && dist != m_fTotalLength) {
                m_aSegments[j + 1] = m_aSegments[j] + (m_aSegments[j + 1] - m_aSegments[j]) * (m_fTotalLength / dist);
            }
        }
    }

    // Final backward scan: relax the first stretched segments from the free end down
    for (int32 i = NUM_ROPE_SEGMENTS - 1; i > heldSegIdx + 1; i--) {
        const float dist = (m_aSegments[i] - m_aSegments[i - 1]).Magnitude();
        if (dist <= m_fTotalLength) {
            break;
        }
        m_aSegments[i - 1] = m_aSegments[i] + (m_aSegments[i - 1] - m_aSegments[i]) * (m_fTotalLength / dist);
        if (i - 1 <= heldSegIdx + 1) {
            return 0;
        }
    }
    return 0;
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
// 0x557530
void CRope::Update() {
    plugin::CallMethod<0x557530, CRope*>(this);
}
