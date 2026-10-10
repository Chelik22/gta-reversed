#include "StdInc.h"
#include "StencilShadowObject.h"

static inline auto& s_ShadowTrianglePointsUnk         = StaticRef<RxVertexIndex*>(0xC6A170);
static inline auto& s_ShadowTrianglePoints            = StaticRef<CVector*>(0xC6A174);
static inline auto& s_TransformedShadowTrianglePoints = StaticRef<CVector*>(0xC6A178);
static inline auto& s_SunPosNrm                       = StaticRef<CVector>(0x8D5244); // CVector(1.0, 1.0, -2.0)

void CStencilShadows::InjectHooks() {
    RH_ScopedClass(CStencilShadows);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x70F9E0);
    RH_ScopedInstall(Shutdown, 0x711390);
    RH_ScopedInstall(Process, 0x711D90);
    RH_ScopedInstall(GraphicsHighQuality, 0x70F9B0);
    RH_ScopedInstall(UpdateHierarchy, 0x710BC0);
    RH_ScopedInstall(RegisterStencilShadows, 0x711760);
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
// Вершинный буфер объектов — m_ShadowFacesData (CVector*): m_SizeOfShadowFacesData — число ВЕРШИН
// (3 на грань-«крышку» + 6 на силуэтное ребро); здесь вершины группируются по 6 (TRILIST, индексы
// подряд) + хвостовой одиночный треугольник, если (число вершин / 3) нечётно. Хвост рендерится
// и при 0 полных пар (объём из одной «крышки» — реальный случай), поэтому проверка ВНЕ цикла пар.
// В RwIm3DVertex пишутся только objVertex и color (ARGB = CRGBA::ToIntARGB()); нормали/UV
// остаются от предыдущих кадров — как в оригинале.
void CStencilShadows::Render(const CRGBA& color) {
    uiTempBufferIndicesStored  = 0;
    uiTempBufferVerticesStored = 0;

    uint32 totalIndices  = 0;
    uint32 totalVertices = 0;
    const auto argb = color.ToIntARGB();

    for (auto* shadow = pFirstActiveStencilShadowObject; shadow; shadow = shadow->m_pNext) {
        const auto  numVertices  = shadow->m_SizeOfShadowFacesData;
        const auto* points       = shadow->m_ShadowFacesData;
        const auto  numVertPairs = numVertices / 6;

        auto* p = points;
        for (auto f = 0u; f < numVertPairs; f++, p += 6) {
            sub_710CC0(6, 6); // сброс буфера, если переполнен

            const auto nextIdx = uiTempBufferIndicesStored;
            for (auto i = 0; i < 6; i++) {
                aTempBufferIndices[nextIdx + i] = nextIdx + i;
            }

            const auto nextVert = uiTempBufferVerticesStored;
            for (auto i = 0; i < 6; i++) {
                auto& vert = TempBufferVertices.m_3d[nextVert + i];
                RwIm3DVertexSetPos(&vert, p[i].x, p[i].y, p[i].z);
                vert.color = argb;
            }

            uiTempBufferIndicesStored  = uiTempBufferIndicesStored + 6;
            uiTempBufferVerticesStored = uiTempBufferVerticesStored + 6;
            totalIndices  += 6;
            totalVertices += 6;
        }

        if ((numVertices / 3) & 1) { // хвостовой одиночный треугольник
            sub_710CC0(3, 3);

            const auto nextIdx = uiTempBufferIndicesStored;
            aTempBufferIndices[nextIdx + 0] = nextIdx + 0;
            aTempBufferIndices[nextIdx + 1] = nextIdx + 1;
            aTempBufferIndices[nextIdx + 2] = nextIdx + 2;

            const auto nextVert = uiTempBufferVerticesStored;
            for (auto i = 0; i < 3; i++) {
                auto& vert = TempBufferVertices.m_3d[nextVert + i];
                RwIm3DVertexSetPos(&vert, p[i].x, p[i].y, p[i].z);
                vert.color = argb;
            }

            uiTempBufferIndicesStored  = uiTempBufferIndicesStored + 3;
            uiTempBufferVerticesStored = uiTempBufferVerticesStored + 3;
            totalIndices  += 3;
            totalVertices += 3;
        }
    }

    if (totalIndices != 0 && totalVertices != 0) {
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(nullptr));
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

// 0x70FA70 — toggle силуэтного ребра: пара (i1,i2) уже есть в буфере (в любом порядке) -> убрать её
// (последняя пара ставится на её место, счётчик--), нет -> дописать (счётчик++).
// ВНИМАНИЕ: оригинальная конвенция несовместима с C-вызовом (ecx=буфер, edx=&uint16-счётчика на стеке,
// edi+стек-арг — индексы), хук на 0x70FA70 НЕ ставить. Ghidra потеряла запись *edx, из-за чего
// «второй цикл» (стенки объёма) в декомпилах 0x70FAE0/0x710310 выглядел мёртвым — по дизасму он живой.
namespace {
void sub_70FA70(uint16* edges, uint16& count, uint16 i1, uint16 i2) {
    for (uint16 i = 0; i < count; i++) {
        if ((edges[i * 2] == i1 && edges[i * 2 + 1] == i2)
         || (edges[i * 2] == i2 && edges[i * 2 + 1] == i1)) {
            if (count > 1) {
                edges[i * 2]       = edges[(count - 1) * 2];
                edges[i * 2 + 1]   = edges[(count - 1) * 2 + 1];
            }
            count--;
            return;
        }
    }
    edges[count * 2]     = i1;
    edges[count * 2 + 1] = i2;
    count++;
}
} // namespace

// 0x70FAE0
// Роли полей CStencilShadowObject (семантика по декомпилу+дизасму; имена в хедере условны):
//   m_FaceID              = ЁМКОСТЬ буфера m_ShadowFacesData в ВЕРШИНАХ
//   m_SizeOfShadowFacesData = число ЗАПИСАННЫХ вершин (3 на крышку, 6 на силуэтное ребро)
// ABI: оригинал возвращает bool в AL (0 = нет колмодели/рассинхрон числа граней, 1 = ок).
bool CStencilShadows::RenderForVehicle(CStencilShadowObject* object) {
    auto* const owner = object->m_pOwner;

    // Дистанция выдавливания: вертолёты/самолёты (по подтипу) -> 40, остальные -> 5
    float extrudeDist;
    if (owner->GetType() == ENTITY_TYPE_VEHICLE) { // m_info -- protected
        const auto veh = owner->AsVehicle();
        if (veh->m_nVehicleSubType == VEHICLE_TYPE_HELI || veh->m_nVehicleSubType == VEHICLE_TYPE_PLANE) {
            extrudeDist = 40.0f;
        } else {
            extrudeDist = 5.0f;
        }
    } else {
        extrudeDist = 5.0f;
    }

    auto* colModel = owner->GetColModel();
    auto* colData  = colModel->m_pColData; // GetShadTrianglePoint -- non-const
    if (!colData || static_cast<int32>(colData->m_nNumShadowTriangles) != object->m_NumShadowFaces) {
        return false; // теневой геометрии нет (или модель изменилась после регистрации объекта)
    }

    if (!owner->m_matrix) {
        owner->AllocateMatrix();
        owner->m_placement.UpdateMatrix(owner->m_matrix);
    }

    CMatrix* mat = owner->m_matrix; // м.б. заменён на &bike->m_mLeanMatrix (CMatrix*)
    if (owner->GetType() == ENTITY_TYPE_VEHICLE && owner->AsVehicle()->m_nVehicleType == VEHICLE_TYPE_BIKE) {
        auto* bike = owner->AsBike();
        bike->CalculateLeanMatrix();
        mat = &bike->m_mLeanMatrix;
    }

    // Направление на солнце в объектном пространстве: inv*(0,0,0) и inv*s_SunPosNrm, разность.
    // (sret-вызовы 0x59C890 == CMatrix::TransformPoint)
    CMatrix invMat;
    Invert(*mat, invMat);                                       // 0x59B920 (in, out)
    const auto vecOrigin = invMat.TransformPoint(CVector{ 0.0f, 0.0f, 0.0f });
    const auto vecSun    = invMat.TransformPoint(s_SunPosNrm);
    const auto sunDirObj = vecSun - vecOrigin;

    // Разжать теневые вершины колмодели (объектное пространство) и перевести в мировое
    auto* const objPts   = s_ShadowTrianglePoints;
    auto* const worldPts = s_TransformedShadowTrianglePoints;
    const auto numVerts  = colData->m_nNumShadowVertices;
    for (auto i = 0u; i < numVerts; i++) {
        colData->GetShadTrianglePoint(objPts[i], i);            // 0x40F640
    }
    TransformPoints(reinterpret_cast<RwV3d*>(worldPts), numVerts, *mat, reinterpret_cast<RwV3d*>(objPts)); // 0x54EEF0

    const auto extrusion = s_SunPosNrm * extrudeDist;

    object->m_SizeOfShadowFacesData = 0;
    uint16  numEdges = 0;
    auto* const edges = s_ShadowTrianglePointsUnk; // пары uint16

    // Крышки объёма: отвёрнутые от солнца грани пишутся выдвинутыми, лицевые - на месте
    // (обход при записи обратный: v0, v2, v1)
    for (auto f = 0; f < object->m_NumShadowFaces; f++) {
        const auto& tri = colData->m_pShadowTriangles[f];
        const auto i0 = tri.vA;
        const auto i1 = tri.vB;
        const auto i2 = tri.vC;
        const CVector& v0 = objPts[i0]; // объектные - для нормали
        const CVector& v1 = objPts[i1];
        const CVector& v2 = objPts[i2];

        if (DotProduct(CrossProduct(v1 - v0, v2 - v0), sunDirObj) < 0.0f) {
            // Грань отвёрнута от солнца -> верхняя (выдвинутая) крышка
            if (object->m_FaceID <= object->m_SizeOfShadowFacesData + 3) {
                break;
            }
            auto* out = &object->m_ShadowFacesData[object->m_SizeOfShadowFacesData];
            out[0] = worldPts[i0] + extrusion;
            out[1] = worldPts[i2] + extrusion;
            out[2] = worldPts[i1] + extrusion;
            object->m_SizeOfShadowFacesData += 3;
        } else {
            // Лицевая грань: тогглим рёбра (силуэт), затем нижняя крышка.
            // В оригинале тоггли выполняется ДО проверки ёмкости - сохранить порядок!
            sub_70FA70(edges, numEdges, i0, i1);
            sub_70FA70(edges, numEdges, i1, i2);
            sub_70FA70(edges, numEdges, i2, i0);
            if (object->m_FaceID <= object->m_SizeOfShadowFacesData + 3) {
                break;
            }
            auto* out = &object->m_ShadowFacesData[object->m_SizeOfShadowFacesData];
            out[0] = worldPts[i0];
            out[1] = worldPts[i2];
            out[2] = worldPts[i1];
            object->m_SizeOfShadowFacesData += 3;
        }
    }

    // Стенки объёма вдоль силуэтных рёбер (2 треугольника на ребро)
    for (uint16 e = 0; e < numEdges; e++) {
        const auto j0 = edges[e * 2];
        const auto j1 = edges[e * 2 + 1];
        if (object->m_FaceID <= object->m_SizeOfShadowFacesData + 6) {
            break;
        }
        auto* out = &object->m_ShadowFacesData[object->m_SizeOfShadowFacesData];
        out[0] = worldPts[j0];
        out[1] = worldPts[j1];
        out[2] = worldPts[j0] + extrusion;
        out[3] = worldPts[j1];
        out[4] = worldPts[j1] + extrusion;
        out[5] = worldPts[j0] + extrusion;
        object->m_SizeOfShadowFacesData += 6;
    }

    return true;
}

// 0x710310
// То же, что RenderForVehicle, но: дистанция выдавливания ВСЕГДА 60.0f (fmul [0x858B34], тот же
// глобал, что Train.cpp:1081) и НЕТ ветки байка — всегда owner->m_matrix.
bool CStencilShadows::RenderForObject(CStencilShadowObject* object) {
    auto* const owner = object->m_pOwner;

    constexpr float EXTRUDE_DIST = 60.0f;

    auto* colModel = owner->GetColModel();
    auto* colData  = colModel->m_pColData; // GetShadTrianglePoint -- non-const
    if (!colData || static_cast<int32>(colData->m_nNumShadowTriangles) != object->m_NumShadowFaces) {
        return false;
    }

    if (!owner->m_matrix) {
        owner->AllocateMatrix();
        owner->m_placement.UpdateMatrix(owner->m_matrix);
    }

    auto* const mat = owner->m_matrix;

    CMatrix invMat;
    Invert(*mat, invMat);
    const auto vecOrigin = invMat.TransformPoint(CVector{ 0.0f, 0.0f, 0.0f });
    const auto vecSun    = invMat.TransformPoint(s_SunPosNrm);
    const auto sunDirObj = vecSun - vecOrigin;

    auto* const objPts   = s_ShadowTrianglePoints;
    auto* const worldPts = s_TransformedShadowTrianglePoints;
    const auto numVerts  = colData->m_nNumShadowVertices;
    for (auto i = 0u; i < numVerts; i++) {
        colData->GetShadTrianglePoint(objPts[i], i);
    }
    TransformPoints(reinterpret_cast<RwV3d*>(worldPts), numVerts, *mat, reinterpret_cast<RwV3d*>(objPts));

    const auto extrusion = s_SunPosNrm * EXTRUDE_DIST;

    object->m_SizeOfShadowFacesData = 0;
    uint16  numEdges = 0;
    auto* const edges = s_ShadowTrianglePointsUnk;

    for (auto f = 0; f < object->m_NumShadowFaces; f++) {
        const auto& tri = colData->m_pShadowTriangles[f];
        const auto i0 = tri.vA;
        const auto i1 = tri.vB;
        const auto i2 = tri.vC;
        const CVector& v0 = objPts[i0];
        const CVector& v1 = objPts[i1];
        const CVector& v2 = objPts[i2];

        if (DotProduct(CrossProduct(v1 - v0, v2 - v0), sunDirObj) < 0.0f) {
            if (object->m_FaceID <= object->m_SizeOfShadowFacesData + 3) {
                break;
            }
            auto* out = &object->m_ShadowFacesData[object->m_SizeOfShadowFacesData];
            out[0] = worldPts[i0] + extrusion;
            out[1] = worldPts[i2] + extrusion;
            out[2] = worldPts[i1] + extrusion;
            object->m_SizeOfShadowFacesData += 3;
        } else {
            sub_70FA70(edges, numEdges, i0, i1);
            sub_70FA70(edges, numEdges, i1, i2);
            sub_70FA70(edges, numEdges, i2, i0);
            if (object->m_FaceID <= object->m_SizeOfShadowFacesData + 3) {
                break;
            }
            auto* out = &object->m_ShadowFacesData[object->m_SizeOfShadowFacesData];
            out[0] = worldPts[i0];
            out[1] = worldPts[i2];
            out[2] = worldPts[i1];
            object->m_SizeOfShadowFacesData += 3;
        }
    }

    for (uint16 e = 0; e < numEdges; e++) {
        const auto j0 = edges[e * 2];
        const auto j1 = edges[e * 2 + 1];
        if (object->m_FaceID <= object->m_SizeOfShadowFacesData + 6) {
            break;
        }
        auto* out = &object->m_ShadowFacesData[object->m_SizeOfShadowFacesData];
        out[0] = worldPts[j0];
        out[1] = worldPts[j1];
        out[2] = worldPts[j0] + extrusion;
        out[3] = worldPts[j1];
        out[4] = worldPts[j1] + extrusion;
        out[5] = worldPts[j0] + extrusion;
        object->m_SizeOfShadowFacesData += 6;
    }

    return true;
}

// 0x711D90
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

// 0x711760
void CStencilShadows::RegisterStencilShadows(CVector& cameraPos, bool doNotCreateNew) {
}
