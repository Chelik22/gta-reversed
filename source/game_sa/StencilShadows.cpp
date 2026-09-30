#include "StdInc.h"
#include "StencilShadows.h"
#include "StencilShadowObject.h"
#include "Collision/ColModel.h"
#include "Collision/CollisionData.h"
#include "Entity/Vehicle/Bike.h"

// TODO: Statically allocate after reversing RenderForVehicle&RenderForObject.
static inline auto& s_ShadowTrianglePointsUnk         = StaticRef<RxVertexIndex*>(0xC6A170);
static inline auto& s_ShadowTrianglePoints            = StaticRef<CVector*>(0xC6A174);
static inline auto& s_TransformedShadowTrianglePoints = StaticRef<CVector*>(0xC6A178);
static inline auto& s_SunPosNrm                       = StaticRef<CVector>(0x8D5244); // CVector(1.0, 1.0, -2.0)

void CStencilShadows::InjectHooks() {
    RH_ScopedClass(CStencilShadows);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x70F9E0);
    RH_ScopedInstall(Shutdown, 0x711390);
    RH_ScopedInstall(Process, 0x711D90, { .reversed = false }); // body redirects to the original (see Process) - installing it = infinite recursion
    RH_ScopedInstall(GraphicsHighQuality, 0x70F9B0);
    RH_ScopedInstall(UpdateHierarchy, 0x710BC0);
    RH_ScopedInstall(RegisterStencilShadows, 0x711760, { .reversed = false }); // TRIAGE: flicker — registration must be original too
    RH_ScopedInstall(RenderStencilShadows, 0x7113B0);
    RH_ScopedInstall(RenderForVehicle, 0x70FAE0);
    RH_ScopedInstall(RenderForObject, 0x710310);
    RH_ScopedInstall(Render, 0x710D50);
    RH_ScopedInstall(RenderBuffer, 0x710B50);
    RH_ScopedInstall(SunSetPositionFromEntity, 0x710AF0);
    RH_ScopedInstall(sub_710CC0, 0x710CC0);
}

// 0x70F9E0
void CStencilShadows::Init() {
    ZoneScoped;

    RwD3D9SetStencilClear(0);
    pFirstAvailableStencilShadowObject = m_StencilShadowObjects.data();
    pFirstActiveStencilShadowObject = nullptr;

    for (auto&& [i, obj] : rngv::enumerate(m_StencilShadowObjects)) {
        obj.m_pOwner                = nullptr;
        obj.m_NumShadowFaces        = 0;
        obj.m_Type                  = eStencilShadowObjType::NONE;
        obj.m_SizeOfShadowFacesData = 0;
        obj.m_FaceID                = 0;
        obj.m_ShadowFacesData       = 0;

        obj.m_pPrev = i ? &m_StencilShadowObjects[i - 1] : nullptr;
        obj.m_pNext = (i != m_StencilShadowObjects.size() - 1) ? &m_StencilShadowObjects[i + 1] : nullptr;
    }
}

// 0x711390
void CStencilShadows::Shutdown() {
    for (auto* obj = pFirstActiveStencilShadowObject; obj;) {
        auto* next = obj->m_pNext;
        obj->Destroy();
        obj = next;
    }
}

// 0x710D50
// Verified 1:1 against the original disassembly (2026-09-26):
// - The original takes the CRGBA by value and pre-packs it to a D3D ARGB dword by hand;
//   RxObjSpace3DVertexSetPreLitColor produces the exact same packing (a<<24|r<<16|g<<8|b).
// - Temp buffer vertex stride is sizeof(RxObjSpace3DVertex) == 36 (color at +0x18), which is
//   exactly what the original writes (pos dwords +0x00..0x08, color dword +0x18, normal +0x0C
//   never written, Im3D flags 0x18 == rwIM3D_VERTEXXYZ(8) | rwIM3D_VERTEXRGBA(16)).
// - The original writes indices as `aTempBufferIndices[idxCount + j] = idxCount + j`, ours uses
//   the vertex base -- both are identical because sub_710CC0 always flushes indices+vertices in
//   lockstep, so both counters are equal at any time.
void CStencilShadows::Render(CRGBA color) {
    ZoneScoped;

    uiTempBufferIndicesStored  = 0;
    uiTempBufferVerticesStored = 0;

    const auto rwColor = color.ToRwRGBA(); // Original packs the color to a D3D ARGB uint32 by hand

    for (auto* shadow = pFirstActiveStencilShadowObject; shadow; shadow = shadow->m_pNext) {
        const auto* pts = shadow->m_ShadowFacesData;
        assert(pts);

        // Quads (2 triangles == 6 vertices)
        const auto numQuadVerts = shadow->m_SizeOfShadowFacesData / 6;
        const auto* pt = pts;
        for (auto i = 0u; i < numQuadVerts; i++) {
            sub_710CC0(6, 6);

            const auto idxBase = uiTempBufferIndicesStored;
            const auto vtxBase = uiTempBufferVerticesStored;

            for (auto j = 0; j < 6; j++) {
                aTempBufferIndices[idxBase + j]  = (RxVertexIndex)(vtxBase + j);
                auto& v                          = TempBufferVertices.m_3d[vtxBase + j];
                const auto pos                   = pt[j];
                RxObjSpace3DVertexSetPos(&v, &pos);
                RxObjSpace3DVertexSetPreLitColor(&v, &rwColor);
            }
            uiTempBufferIndicesStored  += 6;
            uiTempBufferVerticesStored += 6;
            pt += 6;
        }

        // Leftover triangle (vertex count not divisible by 6 but by 3)
        if ((shadow->m_SizeOfShadowFacesData / 3) & 1) {
            sub_710CC0(3, 3);

            const auto idxBase = uiTempBufferIndicesStored;
            const auto vtxBase = uiTempBufferVerticesStored;

            for (auto j = 0; j < 3; j++) {
                aTempBufferIndices[idxBase + j]  = (RxVertexIndex)(vtxBase + j);
                auto& v                          = TempBufferVertices.m_3d[vtxBase + j];
                const auto pos                   = pt[j];
                RxObjSpace3DVertexSetPos(&v, &pos);
                RxObjSpace3DVertexSetPreLitColor(&v, &rwColor);
            }
            uiTempBufferIndicesStored  += 3;
            uiTempBufferVerticesStored += 3;
        }
    }

    if (uiTempBufferIndicesStored && uiTempBufferVerticesStored) {
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr); // Original calls the RW function table entry directly
        LittleTest();
        if (RwIm3DTransform(TempBufferVertices.m_3d, uiTempBufferVerticesStored, nullptr, rwIM3D_VERTEXXYZ | rwIM3D_VERTEXRGBA)) {
            RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, aTempBufferIndices, uiTempBufferIndicesStored);
            RwIm3DEnd();
        }
        uiTempBufferVerticesStored = 0;
        uiTempBufferIndicesStored  = 0;
    }
}

// unused
// 0x710AF0
void CStencilShadows::SunSetPositionFromEntity(const CEntity* entity) {
    if (!entity) {
        return;
    }
    s_SunPosNrm = entity->GetPosition().Normalized();
}

// 0x710B50
void CStencilShadows::RenderBuffer(const CVector& pos) {
    s_SunPosNrm = pos.Normalized(); 
}

// 0x710CC0
void CStencilShadows::sub_710CC0(int32 indices, int32 vertices) {
    if (uiTempBufferIndicesStored + indices < TOTAL_TEMP_BUFFER_INDICES
        && uiTempBufferVerticesStored + vertices < TOTAL_TEMP_BUFFER_3DVERTICES) {
        return;
    }

    if (!uiTempBufferIndicesStored || !uiTempBufferVerticesStored) {
        return;
    }

    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
    LittleTest();
    if (RwIm3DTransform(TempBufferVertices.m_3d, uiTempBufferVerticesStored, nullptr, rwIM3D_VERTEXXYZ | rwIM3D_VERTEXRGBA)) {
        RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, aTempBufferIndices, uiTempBufferIndicesStored);
        RwIm3DEnd();
    }
    uiTempBufferVerticesStored = uiTempBufferIndicesStored = 0;
}

// 0x7113B0
void CStencilShadows::RenderStencilShadows() {
    ZoneScoped;

    if (!GraphicsHighQuality()) {
        return;
    }

    // 0x7113C2: original additionally requires the scene camera's frame buffer raster to be
    // at least 32 bits deep (RwCamera + 0x60 == frameBuffer, RwRaster + 0x14 == depth).
    if (const auto* camera = StaticRef<RwCamera*>(0xC1703C); !camera || !camera->frameBuffer || camera->frameBuffer->depth < 32) {
        return;
    }

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,             RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESTENCILENABLE,            RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,                RWRSTATE(rwSHADEMODEFLAT));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,                RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,            RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,        RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,                 RWRSTATE(rwBLENDZERO));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,                RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATESTENCILFUNCTIONMASK,      RWRSTATE(uint32(-1)));
    RwRenderStateSet(rwRENDERSTATESTENCILFUNCTIONWRITEMASK, RWRSTATE(uint32(-1)));
    RwRenderStateSet(rwRENDERSTATESTENCILFUNCTION,          RWRSTATE(rwSTENCILFUNCTIONALWAYS));
    RwRenderStateSet(rwRENDERSTATESTENCILFAIL,              RWRSTATE(rwSTENCILOPERATIONKEEP));
    RwRenderStateSet(rwRENDERSTATESTENCILZFAIL,             RWRSTATE(rwSTENCILOPERATIONKEEP));
    RwRenderStateSet(rwRENDERSTATESTENCILPASS,              RWRSTATE(rwSTENCILOPERATIONKEEP));
    RwRenderStateSet(rwRENDERSTATESTENCILFUNCTIONREF,       RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,              RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESTENCILZFAIL,             RWRSTATE(rwSTENCILOPERATIONINCR));
    RwRenderStateSet(rwRENDERSTATECULLMODE,                 RWRSTATE(rwCULLMODECULLFRONT));

    Render(CRGBA{ 0, 0, 0, 255 });

    RwRenderStateSet(rwRENDERSTATESTENCILZFAIL,             RWRSTATE(rwSTENCILOPERATIONDECR));
    RwRenderStateSet(rwRENDERSTATECULLMODE,                 RWRSTATE(rwCULLMODECULLBACK));

    Render(CRGBA{ 0, 0, 0, 255 });

    // WTF is up with these states?
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,        RWRSTATE(FALSE)); // same state
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,             RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,              RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESTENCILENABLE,            RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,                RWRSTATE(rwSHADEMODEGOURAUD));
    RwRenderStateSet(rwRENDERSTATECULLMODE,                 RWRSTATE(rwCULLMODECULLBACK));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,             RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,              RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESTENCILENABLE,            RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,                RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,        RWRSTATE(TRUE)); // same state
    RwRenderStateSet(rwRENDERSTATESRCBLEND,                 RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,                RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,                RWRSTATE(rwSHADEMODEFLAT));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,            RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATECULLMODE,                 RWRSTATE(rwCULLMODECULLNONE));
    RwRenderStateSet(rwRENDERSTATESTENCILFUNCTIONREF,       RWRSTATE(1u));
    RwRenderStateSet(rwRENDERSTATESTENCILFUNCTION,          RWRSTATE(rwSTENCILFUNCTIONLESSEQUAL));
    RwRenderStateSet(rwRENDERSTATESTENCILFAIL,              RWRSTATE(rwSTENCILOPERATIONKEEP));
    RwRenderStateSet(rwRENDERSTATESTENCILZFAIL,             RWRSTATE(rwSTENCILOPERATIONKEEP));
    RwRenderStateSet(rwRENDERSTATESTENCILPASS,              RWRSTATE(rwSTENCILOPERATIONKEEP));

    CSprite2d::InitPerFrame();
    CSprite2d::DrawRect(
        CRect{ 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT },
        CRGBA{ 0, 0, 0, (uint8)(50u * CTimeCycle::m_CurrentColours.m_nShadowStrength / 256) }
    );

    RwRenderStateSet(rwRENDERSTATESTENCILENABLE,            RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,             RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,              RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,        RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,                RWRSTATE(rwSHADEMODEGOURAUD));
    RwRenderStateSet(rwRENDERSTATECULLMODE,                 RWRSTATE(rwCULLMODECULLBACK));
}

// Unreversed helper of the original binary (0x70FA70):
// Silhouette edge accumulator with TOGGLE semantics (verified against disassembly 2026-09-26).
// NOTE: The original is NOT a plain function -- it is a carved-out block of RenderForVehicle
// using a custom register calling convention: EDX -> uint16* numEdges, ECX -> RxVertexIndex*
// edge array, first index in EDI (caller-set), second index on the stack ([ESP+0xC] at entry).
// Semantics: if the (undirected) edge (a, b) is already in the list it is REMOVED (swap-erase
// with the last entry), otherwise it is appended. This leaves only edges seen an odd number of
// times -- the actual silhouette -- in the list.
static void RecordShadowEdgeIndices(uint16* numEdges, RxVertexIndex* edges, uint16 a, uint16 b) {
    for (auto i = 0u; i < *numEdges; i++) {
        if ((edges[2 * i] == a && edges[2 * i + 1] == b)
            || (edges[2 * i] == b && edges[2 * i + 1] == a)) {
            if (*numEdges > 1) { // Swap-erase: move the last edge into the removed slot
                edges[2 * i]     = edges[2 * (*numEdges - 1)];
                edges[2 * i + 1] = edges[2 * (*numEdges - 1) + 1];
            }
            (*numEdges)--;
            return;
        }
    }
    edges[2 * *numEdges]     = a;
    edges[2 * *numEdges + 1] = b;
    (*numEdges)++;
}

// Shared implementation of `RenderForVehicle` / `RenderForObject` (0x70FAE0 / 0x710310)
// NOTE: The original does not use a helper function, both are separate (almost identical) functions.
// Verified 1:1 against the disassembly (2026-09-26):
// - Backface test: cross product of the object-space triangle (v1-v0) x (v2-v0), dotted with the
//   sun direction transformed by the inverse world matrix (transform (0,0,0) and the sun vector,
//   subtract), threshold is 0.0f @ 0x858B50. dot < 0 -> extruded cap.
// - Cap winding is (vA, vC, vB) in both branches; the extruded branch adds the extrusion vector.
// - Edges of sun-facing caps are recorded as (vA,vB), (vB,vC), (vC,vA) in the original
//   (we record the same 3 edges, order is irrelevant since the recorder is parity-based).
// - Prism quad: (v0, v1, v0+e, v1, v1+e, v0+e); break conditions and counters identical.
static bool RenderStencilObject(CStencilShadowObject& object, const CMatrix& worldMat, const CVector& extrusion) {
    auto* owner = object.m_pOwner;

    // Get the collision data; the object is invalid if the number of shadow triangles changed (owner switched model)
    const auto* colModel = owner->GetColModel(); // 0x535300
    if (!colModel->m_pColData || colModel->m_pColData->m_nNumShadowTriangles != (uint32)(int16)object.m_NumShadowFaces) {
        return false;
    }
    const auto& colData = *colModel->m_pColData;

    // Cache the shadow triangle vertex positions (they're compressed in the col data)
    const auto numVertices = (int32)colData.m_nNumShadowVertices;
    for (auto i = 0; i < numVertices; i++) {
        colData.GetShadTrianglePoint(s_ShadowTrianglePoints[i], i); // 0x40F640
    }

    // Transform them to world space
    TransformPoints(s_TransformedShadowTrianglePoints, numVertices, worldMat, s_ShadowTrianglePoints); // 0x54EEF0

    // Transform the sun direction into object space
    const auto inv       = Invert(worldMat); // 0x59B920
    const auto invOrigin = inv.TransformPoint(CVector{ 0.0f, 0.0f, 0.0f });
    const auto sunObjDir = inv.TransformPoint(s_SunPosNrm) - invOrigin;

    object.m_SizeOfShadowFacesData = 0; // Now reused as the current vertex count

    auto numEdges = uint16{ 0 };
    for (auto i = 0; i < (int16)object.m_NumShadowFaces; i++) {
        const auto& tri = colData.m_pShadowTriangles[i];
        const auto& v0o = s_ShadowTrianglePoints[tri.vA];
        const auto& v1o = s_ShadowTrianglePoints[tri.vB];
        const auto& v2o = s_ShadowTrianglePoints[tri.vC];
        const auto& v0w = s_TransformedShadowTrianglePoints[tri.vA];
        const auto& v1w = s_TransformedShadowTrianglePoints[tri.vB];
        const auto& v2w = s_TransformedShadowTrianglePoints[tri.vC];

        // Is the triangle facing away from the sun?
        const auto normal    = CrossProduct(v1o - v0o, v2o - v0o);
        const bool backFacing = DotProduct(normal, sunObjDir) < 0.0f; // 0.0f @ 0x858B50

        auto* out = object.m_ShadowFacesData + object.m_SizeOfShadowFacesData;
        if (backFacing) {
            // Extruded cap (reversed winding)
            if (object.m_FaceID <= object.m_SizeOfShadowFacesData + 3) {
                break;
            }
            *out++ = v0w + extrusion;
            *out++ = v2w + extrusion;
            *out++ = v1w + extrusion;
        } else {
            // Cap in the original position (reversed winding) + record edges for the prism sides
            RecordShadowEdgeIndices(&numEdges, s_ShadowTrianglePointsUnk, tri.vB, tri.vC); // 0x70FA70
            RecordShadowEdgeIndices(&numEdges, s_ShadowTrianglePointsUnk, tri.vC, tri.vA);
            RecordShadowEdgeIndices(&numEdges, s_ShadowTrianglePointsUnk, tri.vA, tri.vB);
            if (object.m_FaceID <= object.m_SizeOfShadowFacesData + 3) {
                break;
            }
            *out++ = v0w;
            *out++ = v2w;
            *out++ = v1w;
        }
        object.m_SizeOfShadowFacesData += 3;
    }

    // Generate the prism sides
    for (auto i = 0; i < numEdges; i++) {
        const auto& v0w = s_TransformedShadowTrianglePoints[s_ShadowTrianglePointsUnk[2 * i]];
        const auto& v1w = s_TransformedShadowTrianglePoints[s_ShadowTrianglePointsUnk[2 * i + 1]];

        if (object.m_FaceID <= object.m_SizeOfShadowFacesData + 6) {
            break;
        }

        auto* out = object.m_ShadowFacesData + object.m_SizeOfShadowFacesData;
        *out++ = v0w;
        *out++ = v1w;
        *out++ = v0w + extrusion;
        *out++ = v1w;
        *out++ = v1w + extrusion;
        *out++ = v0w + extrusion;
        object.m_SizeOfShadowFacesData += 6;
    }
    return true;
}

// 0x70FAE0
bool CStencilShadows::RenderForVehicle(CStencilShadowObject* object) {
    ZoneScoped;

    auto* owner = object->m_pOwner;
    assert(owner->GetIsTypeVehicle());

    auto* vehicle = owner->AsVehicle();
    const float extrusionLength = vehicle->m_nVehicleSubType == VEHICLE_TYPE_PLANE || vehicle->m_nVehicleSubType == VEHICLE_TYPE_HELI
        ? 40.0f
        : 5.0f;

    CMatrix worldMat;
    // The original allocates the entity matrix before branching to the bike path,
    // so a bike with a null matrix doesn't crash inside CalculateLeanMatrix
    if (!owner->m_matrix) {
        owner->AllocateMatrix();                          // 0x54F560
        owner->m_placement.UpdateMatrix(owner->m_matrix); // 0x54F1B0
    }
    if (vehicle->m_nVehicleType == VEHICLE_TYPE_BIKE) {
        static_cast<CBike*>(vehicle)->CalculateLeanMatrix(); // 0x6B7150
        worldMat = static_cast<CBike*>(vehicle)->m_mLeanMatrix; // lean matrix @ vehicle + 0x5CC
    } else {
        worldMat = *owner->m_matrix;
    }

    return RenderStencilObject(*object, worldMat, s_SunPosNrm * extrusionLength);
}

// 0x710310
bool CStencilShadows::RenderForObject(CStencilShadowObject* object) {
    ZoneScoped;

    auto* owner = object->m_pOwner;
    if (!owner->m_matrix) {
        owner->AllocateMatrix();                                // 0x54F560
        owner->m_placement.UpdateMatrix(owner->m_matrix);       // 0x54F1B0
    }
    const CMatrix worldMat = *owner->m_matrix;

    // Object shadows are always extruded by 60 units
    return RenderStencilObject(*object, worldMat, s_SunPosNrm * 60.0f); // 60.0f @ 0x858B34
}

// 0x711D90
void CStencilShadows::Process(CVector& cameraPos) {
    // TRIAGE: our rewritten register+render pipeline floods the screen with
    // shadow volumes (user screenshots). Redirect to the original routine -
    // NOTE: internal C++ callers (CGame::Process) bypass the hook table, so the
    // body itself must forward for the disable to be complete.
    plugin::Call<0x711D90, CVector*>(&cameraPos); // static fn: cdecl, push &cameraPos
}

#if 0 // original reversed body kept for reference until the rewrite is fixed
void CStencilShadows::Process(CVector& cameraPos) {
    ZoneScoped;

    if (!GraphicsHighQuality()) {
        return;
    }

    static uint8 s_RegisterShadowCounter{}, s_RenderForObjCounter{};

    RegisterStencilShadows(cameraPos, ++s_RegisterShadowCounter % 8);

    // why do we even do this?
    s_ShadowTrianglePointsUnk         = (RxVertexIndex*)CMemoryMgr::Malloc(12'288 * sizeof(RxVertexIndex));
    s_ShadowTrianglePoints            = (CVector*)CMemoryMgr::Malloc(2'048 * sizeof(CVector));
    s_TransformedShadowTrianglePoints = (CVector*)CMemoryMgr::Malloc(2'048 * sizeof(CVector));

    auto i{ 0 };
    for (auto* obj = pFirstActiveStencilShadowObject; obj; obj = obj->m_pNext) {
        switch (obj->m_Type) {
        case eStencilShadowObjType::OBJECT:
            if ((i++ % 4) == s_RenderForObjCounter) {
                RenderForObject(obj);
            }
            break;
        case eStencilShadowObjType::VEHICLE:
            RenderForVehicle(obj);
            break;
        default:
            break;
        }
    }
    s_RenderForObjCounter = (s_RenderForObjCounter + 1) % 4;

    CMemoryMgr::Free(std::exchange(s_ShadowTrianglePointsUnk, nullptr));
    CMemoryMgr::Free(std::exchange(s_ShadowTrianglePoints, nullptr));
    CMemoryMgr::Free(std::exchange(s_TransformedShadowTrianglePoints, nullptr));
}

#endif

// 0x70F9B0
bool CStencilShadows::GraphicsHighQuality() {
    return ::GraphicsHighQuality();
}

// 0x710BC0
void CStencilShadows::UpdateHierarchy(CStencilShadowObject*& firstAvailable, CStencilShadowObject*& firstActive, CStencilShadowObject* newOne) {
    if (auto* prev = newOne->m_pPrev) {
        auto* next = newOne->m_pNext;
        if (next) {
            next->m_pPrev = prev;
            newOne->m_pPrev->m_pNext = newOne->m_pNext;
        } else {
            prev->m_pNext = nullptr;
        }
    } else {
        auto* next     = newOne->m_pNext;
        firstAvailable = next;
        if (next) {
            next->m_pPrev = nullptr;
        }
    }
    newOne->m_pNext = firstActive;
    newOne->m_pPrev = nullptr;
    firstActive     = newOne;

    if (newOne->m_pNext) {
        newOne->m_pNext->m_pPrev = newOne;
    }
}

// Unreversed helpers of this module:
//
// NOTE: 0x711160 and 0x7111F0 are NOT real functions in the original binary - they are
// caller-inlined blocks ("functions" Ghidra carved out mid-function) that take NO arguments.
// They read:
//   ESI -> pointer to the camera position (CVector, 3 floats)
//   EBX -> the entity (passed to GetColModel 0x535300 and TransformFromObjectSpace 0x5334F0 via ECX)
// Calling them with a stack argument (as this file previously did) leaves EBX/ESI holding
// whatever the caller happened to have there -> GetColModel on garbage -> AV in CEntity::GetType.
// They are therefore reimplemented below in C++ (identical math, verified against disassembly).
static bool FindStencilShadowObjectForEntity_0x710BA0(const CEntity* entity) {
    return plugin::CallAndReturn<bool, 0x710BA0, const CEntity*>(entity); // cdecl, entity on the stack
}
static void CreateStencilShadowObject_0x711280(CStencilShadowObject* self, CEntity* entity, eStencilShadowObjType type) {
    // thiscall: ECX = the first available CStencilShadowObject, stack args: (entity, type)
    plugin::CallMethod<0x711280, CStencilShadowObject*, CEntity*, eStencilShadowObjType>(self, entity, type);
}

// Reimplementation of 0x711160 / 0x7111F0 (both bodies are identical):
// Distance metric from the camera to the entity's world-space bounding sphere,
// with a quadratic falloff once inside the sphere's radius.
static float GetStencilShadowDistForEntity(const CEntity& entity, const CVector& cameraPos) {
    static const float FALLBACK_FACTOR = StaticRef<float>(0x858624); // dist >= radius
    static const float INNER_FACTOR    = StaticRef<float>(0x858C1C); // dist <  radius

    const auto* colModel = entity.GetColModel(); // 0x535300
    if (!colModel) {
        return std::numeric_limits<float>::max();
    }

    const CVector center = entity.TransformFromObjectSpace(const_cast<CColModel*>(colModel)->GetBoundCenter()); // 0x5334F0
    const float radius   = colModel->GetBoundRadius();
    const float dist     = DistanceBetweenPoints(cameraPos, center);
    const float factor   = dist < radius ? INNER_FACTOR : FALLBACK_FACTOR;
    const float distFromSphere = dist - radius;
    return factor * distFromSphere * distFromSphere;
}

// Maximum distance at which stencil shadows are still (created and) kept
static const float STENCIL_SHADOW_MAX_DIST = StaticRef<float>(0x8598B0);
// World range around the camera that is scanned for shadow casters
static const float SHADOW_RANGE            = StaticRef<float>(0x858B40);
static const float SECTOR_INVERSE_SIZE     = StaticRef<float>(0x858B38); // 1/50
static const float SECTOR_OFFSET           = StaticRef<float>(0x858B34); // world->sector offset

// Helper: destroy the object if the entity shouldn't cast a stencil shadow anymore
// (original inlines this in 0x711760): entities of type OBJECT are pruned when
// invisible, exploded (0x40) or broken (0x400).
static bool StencilShadows_ShouldStopCasting(const CEntity* entity) {
    if (!entity) {
        return true;
    }
    if (entity->GetIsTypeObject()) {
        const auto* object = static_cast<const CObject*>(entity);
        if (!entity->m_bIsVisible
            || (object->m_nObjectFlags & 0x40)    // bIsExploded
            || (object->m_nObjectFlags & 0x400)) { // bIsBroken
            return true;
        }
    }
    return false;
}

// Does this entity's collision data say it casts a stencil shadow?
static bool CastsStencilShadow(const CEntity* entity) {
    const auto* colModel = entity->GetColModel();
    if (!colModel || !colModel->m_pColData) {
        return false;
    }
    // CCollisionData +0x07: shadow flags, bit 2 == casts stencil shadow
    const auto flags = *reinterpret_cast<const uint8*>(reinterpret_cast<uintptr_t>(colModel->m_pColData) + 0x07);
    return (flags & 0x4) != 0;
}

// 0x711760
void CStencilShadows::RegisterStencilShadows(CVector& cameraPos, bool doNotCreateNew) {
    ZoneScoped;

    if (doNotCreateNew) {
        // Only prune stale objects (no distance checks)
        for (auto* obj = pFirstActiveStencilShadowObject; obj;) {
            auto* next   = obj->m_pNext;
            auto* entity = obj->m_pOwner;
            if (StencilShadows_ShouldStopCasting(entity)) {
                obj->Destroy();
            }
            obj = next;
        }
        return;
    }

    // --- Prune pass over the currently active stencil shadow objects ---
    for (auto* obj = pFirstActiveStencilShadowObject; obj;) {
        auto* entity = obj->m_pOwner;
        auto* next   = obj->m_pNext;

        if (StencilShadows_ShouldStopCasting(entity)) {
            obj->Destroy();
            obj = next;
            continue;
        }

        switch (obj->m_Type) {
        case eStencilShadowObjType::OBJECT:
            // Too far away, or in another area?
            if (GetStencilShadowDistForEntity(*entity, cameraPos) > STENCIL_SHADOW_MAX_DIST
                || !entity->IsInCurrentArea()) {
                obj->Destroy();
            }
            break;
        case eStencilShadowObjType::VEHICLE:
            if (GetStencilShadowDistForEntity(*entity, cameraPos) > STENCIL_SHADOW_MAX_DIST) {
                obj->Destroy();
            }
            break;
        default:
            break;
        }
        obj = next;
    }

    // --- Register vehicles from the vehicle pool ---
    if (pFirstAvailableStencilShadowObject) {
        for (auto& vehicle : GetVehiclePool()->GetAllValid()) {
            if (CastsStencilShadow(&vehicle)
                && !FindStencilShadowObjectForEntity_0x710BA0(&vehicle)
                && GetStencilShadowDistForEntity(vehicle, cameraPos) < STENCIL_SHADOW_MAX_DIST) {
                if (!pFirstAvailableStencilShadowObject) {
                    return;
                }
                CreateStencilShadowObject_0x711280(pFirstAvailableStencilShadowObject, &vehicle, eStencilShadowObjType::VEHICLE);
                if (!pFirstAvailableStencilShadowObject) {
                    return;
                }
            }
        }

        // --- Register objects from the world sectors around the camera ---
        if (pFirstAvailableStencilShadowObject) {
            const auto minX = (int32)((cameraPos.x - SHADOW_RANGE) * SECTOR_INVERSE_SIZE + SECTOR_OFFSET);
            const auto maxX = (int32)((cameraPos.x + SHADOW_RANGE) * SECTOR_INVERSE_SIZE + SECTOR_OFFSET);
            const auto minY = (int32)((cameraPos.y - SHADOW_RANGE) * SECTOR_INVERSE_SIZE + SECTOR_OFFSET);
            const auto maxY = (int32)((cameraPos.y + SHADOW_RANGE) * SECTOR_INVERSE_SIZE + SECTOR_OFFSET);

            CWorld::AdvanceCurrentScanCode();
            const auto startScanCode = CWorld::GetCurrentScanCode();

            for (auto x = std::clamp(minX, 0, MAX_SECTORS_X - 1); x <= std::clamp(maxX, 0, MAX_SECTORS_X - 1); x++) {
                for (auto y = std::clamp(minY, 0, MAX_SECTORS_Y - 1); y <= std::clamp(maxY, 0, MAX_SECTORS_Y - 1); y++) {
                    // 1) Static objects from the world sector building list
                    for (auto* const building : CWorld::ms_aSectors[y][x].Buildings) {
                        if (!building || building->m_bOffscreen) {
                            continue;
                        }
                        if (StencilShadows_ShouldStopCasting(building)) {
                            continue;
                        }
                        if (!CastsStencilShadow(building) || building->IsScanCodeCurrent()) {
                            continue;
                        }
                        building->SetCurrentScanCode();

                        if (building->IsInCurrentArea()) {
                            bool alreadyRegistered = false;
                            for (auto* o = pFirstActiveStencilShadowObject; o; o = o->m_pNext) {
                                if (o->m_pOwner == building) {
                                    alreadyRegistered = true;
                                    break;
                                }
                            }
                            if (!alreadyRegistered
                                && GetStencilShadowDistForEntity(*building, cameraPos) < STENCIL_SHADOW_MAX_DIST) {
                                if (!pFirstAvailableStencilShadowObject) {
                                    return;
                                }
                                CreateStencilShadowObject_0x711280(pFirstAvailableStencilShadowObject, building, eStencilShadowObjType::OBJECT);
                                if (!pFirstAvailableStencilShadowObject) {
                                    return;
                                }
                            }
                        }
                    }

                    // 2) Entities from this frame's repeat sector object list
                    for (auto* const object : CWorld::GetRepeatSector(x, y).Objects) {
                        if (!object || object->m_bOffscreen) {
                            continue;
                        }
                        if (StencilShadows_ShouldStopCasting(object)) {
                            continue;
                        }
                        if (!CastsStencilShadow(object) || object->IsScanCodeCurrent()) {
                            continue;
                        }
                        object->SetCurrentScanCode();

                        if (object->IsInCurrentArea()) {
                            bool alreadyRegistered = false;
                            for (auto* o = pFirstActiveStencilShadowObject; o; o = o->m_pNext) {
                                if (o->m_pOwner == object) {
                                    alreadyRegistered = true;
                                    break;
                                }
                            }
                            if (!alreadyRegistered
                                && GetStencilShadowDistForEntity(*object, cameraPos) < STENCIL_SHADOW_MAX_DIST) {
                                if (!pFirstAvailableStencilShadowObject) {
                                    return;
                                }
                                CreateStencilShadowObject_0x711280(pFirstAvailableStencilShadowObject, object, eStencilShadowObjType::OBJECT);
                                if (!pFirstAvailableStencilShadowObject) {
                                    return;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
