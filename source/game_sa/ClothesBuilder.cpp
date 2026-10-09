#include "StdInc.h"

#include <extensions/ci_string.hpp>

#include "ClothesBuilder.h"
#include "PedClothesDesc.h"

auto& playerImg = StaticRef<CDirectory>(0xBC12C0);
auto& playerImgEntries = StaticRef<CDirectory::DirectoryInfo>(0xBBCDC8);

auto& gBoneIndices = StaticRef<notsa::mdarray<int16, 10, 64>>(0xBBC8C8);

auto& ms_ratiosHaveChanged  = StaticRef<bool>(0x8D0AA4);
auto& ms_geometryHasChanged = StaticRef<bool>(0x8D0AA5);
auto& ms_textureHasChanged  = StaticRef<bool>(0x8D0AA6);

void CClothesBuilder::InjectHooks() {
    RH_ScopedClass(CClothesBuilder);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(LoadCdDirectory, 0x5A4190);
    RH_ScopedInstall(RequestGeometry, 0x5A41C0);
    RH_ScopedInstall(RequestTexture, 0x5A4220);
    //RH_ScopedInstall(nullptr, 0x5A42B0); 
    //RH_ScopedInstall(nullptr, 0x5A4380); AtomicInstanceCB
    //RH_ScopedInstall(nullptr, 0x5A43A0);
    //RH_ScopedInstall(nullptr, 0x5A44A0); DestroyTextureCB
    RH_ScopedInstall(PreprocessClothesDesc, 0x5A44C0);
    RH_ScopedInstall(ReleaseGeometry, 0x5A47B0);
    RH_ScopedGlobalInstall(GetAtomicWithName, 0x5A4810);
    RH_ScopedInstall(AddWeightToBoneVertex, 0x5A4840);
    RH_ScopedInstall(StoreBoneArray, 0x5A48B0);
    RH_ScopedOverloadedInstall(BlendGeometry, "3", 0x5A4940, RpGeometry * (*)(RpClump*, const char*, const char*, const char*, float, float, float));
    RH_ScopedOverloadedInstall(BlendGeometry, "2", 0x5A4F10, RpGeometry* (*)(RpClump*, const char*, const char*, float, float));
    RH_ScopedInstall(CopyGeometry, 0x5A5340);
    RH_ScopedInstall(ConstructGeometryArray, 0x5A55A0); // Makes the game crash - Probably a register is changed or smth
    RH_ScopedInstall(DestroySkinArrays, 0x5A56C0);
    RH_ScopedInstall(BuildBoneIndexConversionTable, 0x5A56E0);
    RH_ScopedInstall(CopyTexture, 0x5A5730);
    RH_ScopedInstall(PlaceTextureOnTopOfTexture, 0x5A57B0);
    RH_ScopedOverloadedInstall(BlendTextures, "Dst-Src", 0x5A5820, void (*)(RwTexture*, RwTexture*, float, float, int32));
    RH_ScopedOverloadedInstall(BlendTextures, "Dst-Src1-Src2", 0x5A59C0, void (*)(RwTexture*, RwTexture*, RwTexture*, float, float, float, int32));
    RH_ScopedOverloadedInstall(BlendTextures, "Dst-Src1-Src2-Tat", 0x5A5BC0, void (*)(RwTexture*, RwTexture*, RwTexture*, float, float, float, int32, RwTexture*));
    RH_ScopedGlobalInstall(GetTextureFromTxdAndLoadNextTxd, 0x5A5F70);
    RH_ScopedInstall(ConstructTextures, 0x5A6040);
    RH_ScopedInstall(ConstructGeometryAndSkinArrays, 0x5A6530);
    RH_ScopedInstall(CreateSkinnedClump, 0x5A69D0);
}

// inlined
// 0x5A4190
void CClothesBuilder::LoadCdDirectory() {
    playerImg.Init(550, &playerImgEntries);
    playerImg.ReadDirFile("MODELS\\PLAYER.IMG");
}

// 0x5A41C0
void CClothesBuilder::RequestGeometry(int32 modelId, uint32 modelNameKey) {
    CModelInfo::GetModelInfo(modelId)->bHasComplexHierarchy = true; // TODO/NOTE: Not sure

    uint32 size;
    CdStreamPos pos;
    VERIFY(playerImg.FindItem(CKeyGen::AppendStringToKey(modelNameKey, ".DFF"), pos, size));
    CStreaming::RequestFile(modelId, pos, size, CClothes::ms_clothesImageId, STREAMING_PRIORITY_REQUEST | STREAMING_GAME_REQUIRED);
}

// 0x5A4220
int32 CClothesBuilder::RequestTexture(uint32 txdNameKey) {
    if (txdNameKey == 0) {
        return -1;
    }

    auto& defaultTxdIdx = StaticRef<uint32>(0xBC12D0);
    const auto defaultTxd = CTxdStore::defaultTxds[defaultTxdIdx];
    defaultTxdIdx = (defaultTxdIdx + 1) % 4;

    uint32 size;
    CdStreamPos pos;
    VERIFY(playerImg.FindItem(CKeyGen::AppendStringToKey(txdNameKey, ".TXD"), pos, size));
    CStreaming::RequestFile(TXDToModelId(defaultTxd), pos, size, CClothes::ms_clothesImageId, STREAMING_PRIORITY_REQUEST | STREAMING_GAME_REQUIRED);

    return defaultTxd;
}

// 0x5A44C0
// Перевод с декомпила 0x5A44C0. Глобалы: DAT_00BC12FC = CClothes::ms_numRuleTags, DAT_00BC1300 = CClothes::ms_clothesRules
// (объявлены в Clothes.h:19-20). Опкоды правил = eClothRule (Enums/eClothesRules.h, подключается через PedClothesDesc.h),
// арг-каунты совпадают с лоадером CLOTHES.DAT (Clothes.cpp LoadClothesFile): CUTS=2, SETC=4, TEX=2, HIDE=2,
// ENDIGNORE/IGNORE/ENDEXCLUSIVE/EXCLUSIVE=1.
// Вызовы OG: FUN_005A7910 = CPedClothesDesc::SetModel(uint32, eClothesModelPart) (thiscall, this=desc в ECX - поэтому
// в декомпиле 2 арга), FUN_005A7F30 = CClothes::GetDependentTexture, FUN_005A8050 = CPedClothesDesc::SetTextureAndModel.
// desc+0x28 = m_anTextureKeys (10 моделей по 4 байта).
void CClothesBuilder::PreprocessClothesDesc(CPedClothesDesc& desc, bool a2) {
    // Rule state: up to 8 key conditions; a key of 0xFFFFFFFF means the slot is unused.
    // isIgnore == true  => rule is skipped while wearing `key` (IGNORE family)
    // isIgnore == false => rule applies only while wearing `key` (EXCLUSIVE family)
    uint32 stateKeys[8];
    bool   isIgnore[8];
    for (auto& k : stateKeys) {
        k = 0xFFFFFFFF;
    }
    rng::fill(isIgnore, false); // OG leaves these uninitialized (they are only read for active slots)

    for (uint32 ruleIdx = 0; ruleIdx < CClothes::ms_numRuleTags;) {
        const auto rule    = static_cast<eClothRule>(CClothes::ms_clothesRules[ruleIdx]);
        auto       nextIdx = ruleIdx + 1;

        uint32 a{}, c{}, d{};     // Rule arguments
        auto   b = 0xFFFFFFFFu;   // OG: local_38 = -1 before every rule (also used as the "explicit check slot" marker)
        switch (rule) {
        case eClothRule::TAG_CUTS:
            a = CClothes::ms_clothesRules[nextIdx];
            c = CClothes::ms_clothesRules[nextIdx + 1];
            nextIdx += 2;
            break;
        case eClothRule::TAG_SETC:
            a = CClothes::ms_clothesRules[nextIdx];
            b = CClothes::ms_clothesRules[nextIdx + 1];
            c = CClothes::ms_clothesRules[nextIdx + 2];
            d = CClothes::ms_clothesRules[nextIdx + 3];
            nextIdx += 4;
            break;
        case eClothRule::TAG_TEX:
            a = CClothes::ms_clothesRules[nextIdx];
            d = CClothes::ms_clothesRules[nextIdx + 1];
            nextIdx += 2;
            break;
        case eClothRule::TAG_HIDE:
            a = CClothes::ms_clothesRules[nextIdx];
            b = CClothes::ms_clothesRules[nextIdx + 1];
            nextIdx += 2;
            break;
        case eClothRule::TAG_END_IGNORE:
        case eClothRule::TAG_END_EXCLUSIVE: {
            const auto key = CClothes::ms_clothesRules[nextIdx];
            for (auto s = 0; s < 8; s++) {
                if (stateKeys[s] == key) {
                    stateKeys[s] = 0xFFFFFFFF;
                    isIgnore[s]  = rule == eClothRule::TAG_END_IGNORE;
                    break;
                }
            }
            nextIdx += 1;
            break;
        }
        case eClothRule::TAG_IGNORE:
        case eClothRule::TAG_EXCLUSIVE: {
            const auto key = CClothes::ms_clothesRules[nextIdx];
            for (auto s = 0; s < 8; s++) {
                if (stateKeys[s] == 0xFFFFFFFF) { // First unused slot
                    stateKeys[s] = key;
                    isIgnore[s]  = rule == eClothRule::TAG_IGNORE;
                    break;
                }
            }
            nextIdx += 1;
            break;
        }
        }

        // OG loops all 10 model slots; the rule is applied only if none of the active
        // state conditions holds for the checked slot.
        for (auto part = 0; part < 10; part++) {
            const auto checkPart = b != 0xFFFFFFFF ? (int32)b : part;

            bool skip{};
            for (auto s = 0; s < 8; s++) {
                if (stateKeys[s] != 0xFFFFFFFF && ((desc.m_anModelKeys[checkPart] == stateKeys[s]) == isIgnore[s])) {
                    skip = true;
                    break;
                }
            }
            if (skip) {
                continue;
            }

            switch (rule) {
            case eClothRule::TAG_CUTS: // Cutscene only: replace any model `a` with `c`
                if (a2 && desc.m_anModelKeys[part] == a) {
                    desc.SetModel(c, (eClothesModelPart)part);
                }
                break;
            case eClothRule::TAG_SETC:
                if (desc.m_anModelKeys[part] == a) {
                    if (b == (uint32)CLOTHES_MODEL_HANDS) { // Hands have no texture slot => model only
                        desc.SetModel(c, CLOTHES_MODEL_HANDS);
                    } else {
                        if (d == 0) {
                            d = desc.m_anTextureKeys[CClothes::GetDependentTexture((eClothesModelPart)b)];
                        }
                        if (c == 0) {
                            c = desc.m_anModelKeys[b];
                        }
                        desc.SetTextureAndModel(d, c, CClothes::GetDependentTexture((eClothesModelPart)b));
                    }
                }
                break;
            case eClothRule::TAG_TEX:
                if (desc.m_anModelKeys[part] == a) {
                    desc.SetTextureAndModel(d, a, CClothes::GetDependentTexture((eClothesModelPart)part));
                }
                break;
            case eClothRule::TAG_HIDE:
                if (desc.m_anModelKeys[part] == a) {
                    desc.SetModel(0u, (eClothesModelPart)b); // FIX(integration): 0u, not 0 - literal 0 is ambiguous between SetModel(uint32,...) and SetModel(const char*,...)
                }
                break;
            }
        }

        ruleIdx = nextIdx;
    }
}

// unused
// 0x5A47B0
// Тело сверено с декомпилом 0x5A47B0 (SetModelIsDeletable(383+i)); оно уже было в репо, снят только .Reversed.
void CClothesBuilder::ReleaseGeometry(int32 numToRelease) {
    for (auto i = numToRelease; i; CStreaming::SetModelIsDeletable(MODEL_CLOTHES01_ID384 + i))
        --i;
}

// 0x5A4810
RpAtomic* GetAtomicWithName(RpClump* clump, const char* name) {
    struct Context {
        notsa::ci_string_view name{};
        RpAtomic*             atomic{};
    } c{name};
    RpClumpForAllAtomics(clump, [](RpAtomic* a, void* data) { // 0x5A47E0
        auto& ctx = *static_cast<Context*>(data);
        if (ctx.name == GetFrameNodeName(RpAtomicGetFrame(a))) {
            ctx.atomic = a;
        }
        return a;
    }, &c);
    return c.atomic;
}

// 0x5A4840
void CClothesBuilder::AddWeightToBoneVertex(float (&weights)[8], uint8(&boneVertexIdxs)[8], float weightToAdd, RwUInt32 targetVertexIdx) { // Unknown OG name
    if (weightToAdd == 0.f) {
        return;
    }
    for (auto i = 0; i < 8; i++) {
        if (weights[i] == 0.f) { // Weight not yet used?
            boneVertexIdxs[i] = targetVertexIdx; // Add to list
            weights[i]        = weightToAdd;
            weights[i + 1]    = 0.f;             // Mark next as unused [Though this step in our case is not necessary as the whole weights array is already zero-inited]
            return;
        }
        if (boneVertexIdxs[i] == targetVertexIdx) { // Already in the list, use that
            weights[i] += weightToAdd;
            return;
        }
    }
    NOTSA_UNREACHABLE(); // OG code had UB in this case
}

// 0x5A48B0
void CClothesBuilder::StoreBoneArray(RpClump* clump, int32 idx) {
    const auto a = GetAtomicWithName(clump, "normal");
    assert(a);

    const auto h = RpSkinAtomicGetHAnimHierarchy(a);
    assert(h);
    
    rng::fill(gBoneIndices[idx], -1);
    for (auto i = h->numNodes; i-- > 0;) {
        gBoneIndices[idx][i] = static_cast<int16>(h->pNodeInfo[i].nodeID);
    }
}

/*
* @notsa
* 
* Based on 0x5A4940
* Blend any number of geometries together
* The result is stored in the 0th frame's geometry.
*
* @arg clump  The clump to which the frames belong to
* @arg frames A list of frames whose geometry should be blended together
*/
template<size_t N>
RpGeometry* BlendGeometry(RpClump* clump, std::pair<const char*, float> (&&frameNamesRatios)[N]) {
#ifdef NOTSA_DEBUG
    {
        float a{};
        for (auto&& v : frameNamesRatios) {
            a += v.second;
        }
        assert(a >= 0.f && a <= 1.f);
    }
#endif

    // Process data needed for blending
    struct GeoBlendData {
        RpAtomic*        a;
        RpGeometry*      g;
        const RwUInt8*   boneIdxs;
        RwMatrixWeights* boneWeights;
        RwTexCoords*     uvs;
        CVector*         verts;
        CVector*         nrmls;
        float            r; // Blend ratio
    } fds[N];
    auto& out = fds[0];
    for (auto&& [i, v] : rngv::enumerate(frameNamesRatios)) {
        const auto a  = GetAtomicWithName(clump, v.first);
        const auto g  = RpAtomicGetGeometry(a);
        const auto s  = RpSkinGeometryGetSkin(g);
        const auto mt = RpGeometryGetMorphTarget(g, 0);
        fds[i] = {
            a,
            g,
            (const RwUInt8*)RpSkinGetVertexBoneIndices(s), // NOTE: Stored as single bytes (4 per vertex)
            RpSkinGetVertexBoneWeights(s),
            RpGeometryGetVertexTexCoords(g, 1),
            (CVector*)RpMorphTargetGetVertices(mt),
            (CVector*)RpMorphTargetGetVertexNormals(mt),
            v.second
        };
    }

    RpGeometryLock(out.g, rpGEOMETRYLOCKALL); // OG: 0xFFF

    for (auto i = 0; i < RpGeometryGetNumVertices(out.g); i++) {
        const auto Blend = [&](auto&& Get) {
            return multiply_weighted(fds | rng::views::transform([&](auto&& fd) { return WeightedValue{ std::invoke(Get, &fd)[i], fd.r }; }));
        };
        out.verts[i] = Blend(&GeoBlendData::verts);
        out.nrmls[i] = Blend(&GeoBlendData::nrmls);
        RwV3dNormalize(reinterpret_cast<RwV3d*>(&out.nrmls[i]), reinterpret_cast<const RwV3d*>(&out.nrmls[i])); // OG: 0x7ED9B0
        out.uvs[i]   = Blend(&GeoBlendData::uvs);

        // Helper function to get the weight at a given weight index
        const auto RwMatrixWeightsGetWeight = [](RwMatrixWeights& mw, int32 wi) -> float& {
            switch (wi) {
            case 0:  return mw.w0;
            case 1:  return mw.w1;
            case 2:  return mw.w2;
            case 3:  return mw.w3;
            default: NOTSA_UNREACHABLE();
            }
        };

        // Calculate bone weights
        float weights[8]{};
        uint8 boneVertexIdxs[8]{};
        for (auto& fd : fds) {
            for (auto wi = 0; wi < 4; wi++) {
                CClothesBuilder::AddWeightToBoneVertex(
                    weights,
                    boneVertexIdxs,
                    RwMatrixWeightsGetWeight(fd.boneWeights[i], wi), // NOTE: One RwMatrixWeights per vertex (ptr += 16 bytes/vertex in OG)
                    fd.boneIdxs[i + wi]
                );
            }
        }
        for (auto b = 0; b < 4; b++) {
            const_cast<RwUInt8*>(out.boneIdxs)[i + b] = boneVertexIdxs[b];
        }
        const auto t = weights[4] != 0.f // OG: DAT_00858B50 (0.f) - didn't fit into 4 slots
            ? 1.f / std::accumulate(weights, weights + 4, 0.f) // OG: _DAT_00858624 (1.f) / sum
            : 1.f;
        for (auto wi = 0; wi < 4; wi++) {
            RwMatrixWeightsGetWeight(out.boneWeights[i], wi) = weights[wi] * t; // NOTE: See above - single RwMatrixWeights per vertex
        }
    }

    RpGeometryUnlock(out.g);
    out.g->refCount++; // TODO: Function missing (OG: 0x74CCB0)

    return out.g;
}

// 0x5A4940
RpGeometry* CClothesBuilder::BlendGeometry(RpClump* clump, const char* frameName0, const char* frameName1, const char* frameName2, float r0, float r1, float r2) {
    return ::BlendGeometry(clump, { {frameName0, r0}, {frameName1, r1}, {frameName2, r2} });
}

// 0x5A4F10
RpGeometry* CClothesBuilder::BlendGeometry(RpClump* clump, const char* frameName0, const char* frameName1, float r0, float r1) {
    return ::BlendGeometry(clump, { {frameName0, r0}, {frameName1, r1} });
}

// 0x5A5340
// Перевод с декомпила 0x5A5340: копия verts/normals/uvs/skin-индексов/skin-весов из геометрии атомика `srcFrameName`
// в геометрию атомика `dstFrameName` (она лочится RpGeometryLock(0xFFF) и анлочится).
// В конце OG вызывает 0x74CCB0 (в обёртках RW репо отсутствует) - по аналогии с ::BlendGeometry сделан refCount++.
RpGeometry* CClothesBuilder::CopyGeometry(RpClump* clump, const char* dstFrameName, const char* srcFrameName) {
    struct GeoData {
        RpGeometry*      g;
        CVector*         verts;
        CVector*         nrmls;
        RwTexCoords*     uvs;
        const RwUInt8*   boneIdxs;
        RwMatrixWeights* boneWeights;
    } gd[2]{}; // [0] = dst (frame `dstFrameName`), [1] = src (frame `srcFrameName`)

    const RpAtomic* atomics[2]{
        GetAtomicWithName(clump, dstFrameName),
        GetAtomicWithName(clump, srcFrameName)
    };
    for (auto i = 0; i < 2; i++) {
        const auto g  = RpAtomicGetGeometry(atomics[i]);
        const auto s  = RpSkinGeometryGetSkin(g);
        const auto mt = RpGeometryGetMorphTarget(g, 0);
        gd[i] = {
            g,
            (CVector*)RpMorphTargetGetVertices(mt),
            (CVector*)RpMorphTargetGetVertexNormals(mt),
            RpGeometryGetVertexTexCoords(g, 1),
            (const RwUInt8*)RpSkinGetVertexBoneIndices(s),
            RpSkinGetVertexBoneWeights(s)
        };
    }

    RpGeometryLock(gd[0].g, rpGEOMETRYLOCKALL); // OG: 0xFFF

    // OG iterates over the *destination* geometry's vertex count
    for (auto i = 0; i < RpGeometryGetNumVertices(gd[0].g); i++) {
        gd[0].verts[i] = gd[1].verts[i];
        gd[0].nrmls[i] = gd[1].nrmls[i];
        gd[0].uvs[i]   = gd[1].uvs[i];
        for (auto b = 0; b < 4; b++) {
            const_cast<RwUInt8*>(gd[0].boneIdxs)[i + b] = gd[1].boneIdxs[i + b];
        }
        gd[0].boneWeights[i] = gd[1].boneWeights[i];
    }

    RpGeometryUnlock(gd[0].g);

    gd[0].g->refCount++; // TODO: Function missing (OG: 0x74CCB0)

    return gd[0].g;
}

// 0x5A55A0
// Тело сверено с декомпилом 0x5A55A0 (bHasComplexHierarchy -> RequestGeometry -> LoadAllRequestedModels(true)
// -> prefetch next -> BlendGeometry("normal","fat","ripped") -> StoreBoneArray -> RemoveModel).
// Комментарий про краш в хуке - вероятно, следствие отсутствовавших тел 0x5A4940/0x5A5340 (теперь есть).
void CClothesBuilder::ConstructGeometryArray(RpGeometry** out, uint32* modelNameKeys, float normal, float fatness, float strength) {
    for (auto i = 0; i < 10; i++) {
        if (modelNameKeys[i] == 0) {
            *out = nullptr;
            continue;
        }
        const auto modelIdx = (eModelID)((int)MODEL_CLOTHES01_ID384 + i);
        const auto mi       = CModelInfo::GetModelInfo(modelIdx);

        CModelInfo::GetModelInfo(modelIdx)->bHasComplexHierarchy = true; // OG: *(uint8*)(modelInfo + 0x13) |= 2
        RequestGeometry(modelIdx, modelNameKeys[i]);
        CStreaming::LoadAllRequestedModels(true);

        if (i + 1 < 10 && modelNameKeys[i + 1]) { // Request next model to be loaded in advance
            RequestGeometry((eModelID)((int)MODEL_CLOTHES01_ID384 + i + 1), modelNameKeys[i + 1]);
            CStreaming::LoadRequestedModels();
        }

        *out = BlendGeometry(mi->GetRpClump(), "normal", "fat", "ripped", normal, fatness, strength);
        StoreBoneArray(mi->GetRpClump(), i);
        CStreaming::RemoveModel(modelIdx);
    }
}

// inlined, see 0x5A6CE1
// 0x5A56C0
void CClothesBuilder::DestroySkinArrays(RwMatrixWeights* weights, RwUInt32* bones) {
    // TODO: Should this be `delete[]` or `delete`?
    delete weights;
    delete bones;
}

// 0x5A56E0
void CClothesBuilder::BuildBoneIndexConversionTable(uint8* pTable, RpHAnimHierarchy* hier, int32 index) {
    for (const auto [tableIdx, boneId] : rngv::enumerate(gBoneIndices[index])) {
        if (boneId == -1) {
            break;
        }
        const auto idx = RpHAnimIDGetIndex(hier, boneId);
        pTable[tableIdx] = idx == 0xFF ? 0 : idx;
    }
}

void AssertTextureLayouts(std::initializer_list<RwTexture*> textures) {
    assert(textures.size() >= 2);
    for (auto i = 0u; i < textures.size() - 1; i++) {
        const auto r1 = RwTextureGetRaster(textures.begin()[i]), r2 = RwTextureGetRaster(textures.begin()[i + 1]);

        assert(RwRasterGetWidth(r1) == RwRasterGetWidth(r2));
        assert(RwRasterGetHeight(r1) == RwRasterGetHeight(r2));
        assert(RwRasterGetDepth(r1) == RwRasterGetDepth(r2));
        assert(RwRasterGetDepth(r1) == 32);
    }
}

// 0x5A5730
RwTexture* CClothesBuilder::CopyTexture(RwTexture* srcTex) {
    const auto srcRaster = RwTextureGetRaster(srcTex);
    
    // Create a new raster to which we're going to copy to
    const auto dstRaster = RwRasterCreate(
        RwRasterGetWidth(srcRaster),
        RwRasterGetHeight(srcRaster),
        RwRasterGetDepth(srcRaster),
        (RwRasterGetFormat(srcRaster) & rwRASTERFORMATPIXELFORMATMASK) | 4 // TODO
    );
    
    // Copy data from the src raster to this one
    const auto srcLck = RwRasterLock(srcRaster, 0, rwRASTERLOCKREAD);
    memcpy(
        RwRasterLock(dstRaster, 0, rwRASTERLOCKWRITE),
        srcLck,
        RwRasterGetHeight(srcRaster) * RwRasterGetStride(srcRaster)
    );
    RwRasterUnlock(srcRaster);
    RwRasterUnlock(dstRaster);

    // Create a texture from the copied raster
    const auto dstTex = RwTextureCreate(dstRaster);
    RwTextureSetFilterMode(dstTex, rwFILTERLINEAR);

    AssertTextureLayouts({ dstTex, srcTex });

    return dstTex;
}

// 0x5A57B0
void CClothesBuilder::PlaceTextureOnTopOfTexture(RwTexture* dstTex, RwTexture* srcTex) {
    ZoneScoped;

    AssertTextureLayouts({ dstTex, srcTex });

    const auto dstRaster = RwTextureGetRaster(dstTex);
    const auto srcRaster = RwTextureGetRaster(srcTex);

    auto dstIt = (RwUInt32*)RwRasterLock(dstRaster, 0, rwRASTERLOCKREADWRITE);
    auto srcIt = (RwUInt32*)RwRasterLock(srcRaster, 0, rwRASTERLOCKREADWRITE);

    // NOTE: They don't skip the stride, but it's fine [This way vectorization should be easier for the compiler]
    for (auto i = RwRasterGetHeight(dstRaster) * RwRasterGetWidth(dstRaster); i-- > 0; dstIt++, srcIt++) {
        if (*srcIt & 0xFF000000) { // Check alpha != 0
            *dstIt = *srcIt;
        }
    }

    RwRasterUnlock(dstRaster);
    RwRasterUnlock(srcRaster);
}

// 0x5A5820
void CClothesBuilder::BlendTextures(RwTexture* dst, RwTexture* src, float r1, float r2, int32 numColors) {
    ZoneScoped;

    AssertTextureLayouts({ dst, src });

    const auto dstRaster = RwTextureGetRaster(dst);
    const auto srcRaster = RwTextureGetRaster(src);

    CTimer::Suspend();

    auto srcIt = RwRasterLock(srcRaster, 0, rwRASTERLOCKREAD);
    auto dstIt = RwRasterLock(dstRaster, 0, rwRASTERLOCKREADWRITE);

    for (auto i = RwRasterGetHeight(dstRaster) * RwRasterGetWidth(dstRaster); i-- > 0; dstIt++, srcIt++) {
        for (auto c = 3; i-- > 0; dstIt++, srcIt++) { // Copy RGB, alpha stays the same
            *dstIt = multiply_weighted<RwUInt8>({ { *dstIt, r1 }, { *srcIt, r2 } });
        }
    }

    RwRasterUnlock(dstRaster);
    RwRasterUnlock(srcRaster);

    CTimer::Resume();
}

// 0x5A59C0
void CClothesBuilder::BlendTextures(RwTexture* dst, RwTexture* src1, RwTexture* src2, float r1, float r2, float r3, int32) {
    ZoneScoped;

    AssertTextureLayouts({ dst, src1, src2 });

    const auto dstRaster  = RwTextureGetRaster(dst);
    const auto src1Raster = RwTextureGetRaster(src1);
    const auto src2Raster = RwTextureGetRaster(src2);

    CTimer::Suspend();

    auto src1It = RwRasterLock(src1Raster, 0, rwRASTERLOCKREAD);
    auto src2It = RwRasterLock(src2Raster, 0, rwRASTERLOCKREAD);
    auto dstIt  = RwRasterLock(dstRaster, 0, rwRASTERLOCKREADWRITE);

    for (auto i = RwRasterGetHeight(dstRaster) * RwRasterGetWidth(dstRaster); i-- > 0; dstIt++, src1It++, src2It++) {
        for (auto c = 3; i-- > 0; dstIt++, src1It++, src2It++) { // Copy RGB, alpha doesn't change
            *dstIt = multiply_weighted<RwUInt8>({ { *dstIt, r1 }, { *src1It, r2 }, { *src2It, r3 } });
        }
    }

    RwRasterUnlock(dstRaster);
    RwRasterUnlock(src1Raster);
    RwRasterUnlock(src2Raster);

    CTimer::Resume();
}

// 0x5A5BC0
void CClothesBuilder::BlendTextures(RwTexture* dst, RwTexture* src1, RwTexture* src2, float r1, float r2, float r3, int32 numColors, RwTexture* tattoos) {
    ZoneScoped;

    AssertTextureLayouts({ dst, src1, src2, tattoos });

    const auto dstRaster  = RwTextureGetRaster(dst);
    const auto src1Raster = RwTextureGetRaster(src1);
    const auto src2Raster = RwTextureGetRaster(src2);
    const auto tatRaster  = RwTextureGetRaster(tattoos);

    CTimer::Suspend();

    auto src1It = RwRasterLock(src1Raster, 0, rwRASTERLOCKREAD);
    auto src2It = RwRasterLock(src2Raster, 0, rwRASTERLOCKREAD);
    auto tatIt  = RwRasterLock(tatRaster, 0, rwRASTERLOCKREAD);
    auto dstIt  = RwRasterLock(dstRaster, 0, rwRASTERLOCKREADWRITE);

    for (auto i = RwRasterGetHeight(dstRaster) * RwRasterGetWidth(dstRaster); i-- > 0; dstIt++, src1It++, src2It++, tatIt++) {
        const auto tatAlphaT = (float)tatIt[3] / 255.f;
        for (auto c = 3; i-- > 0; dstIt++, src1It++, src2It++, tatIt++) { // Copy RGB, alpha doesn't change
            *dstIt = (RwUInt8)lerp(multiply_weighted<RwUInt8>({ { *dstIt, r1 }, { *src1It, r2 }, { *src2It, r3 } }), *tatIt, tatAlphaT);
        }
    }

    RwRasterUnlock(dstRaster);
    RwRasterUnlock(src1Raster);
    RwRasterUnlock(src2Raster);
    RwRasterUnlock(tatRaster);

    CTimer::Resume();
}

// 0x5A5F70
RwTexture* GetTextureFromTxdAndLoadNextTxd(RwTexture* dstTex, int32 txdId_withTexture, int32 CRC_nextTxd, int32* nextTxdId) {
    if (txdId_withTexture == -1) {
        if (CRC_nextTxd) {
            *nextTxdId = CClothesBuilder::RequestTexture(CRC_nextTxd);
            CStreaming::LoadRequestedModels();
        } else {
            *nextTxdId = -1;
        }
        return dstTex;
    }

    CStreaming::LoadAllRequestedModels(true);
    if (CRC_nextTxd) {
        *nextTxdId = CClothesBuilder::RequestTexture(CRC_nextTxd);
        CStreaming::LoadRequestedModels();
    } else {
        *nextTxdId = -1;
    }
    const auto tex = GetFirstTexture(CTxdStore::GetTxd(txdId_withTexture));
    const auto res = dstTex
        ? CClothesBuilder::PlaceTextureOnTopOfTexture(dstTex, tex), dstTex
        : CClothesBuilder::CopyTexture(tex);
    CStreaming::RemoveModel(TXDToModelId(txdId_withTexture));
    return res;
}

// 0x5A6040
// Перевод с декомпила 0x5A6040. Схема OG:
//   1) тату-цепочка: RequestTexture(hashes[4] = CLOTHES_TEXTURE_LOWER_LEFT_ARM), затем GetTextureFromTxdAndLoadNextTxd
//      по слотам hashes[5..12] (тату) + финальный вызов с ключом "player_torso" => его txd используется дальше для торса;
//   2) TORSO: базовые "torso"/"torso_fat"/"torso_ripped" из dict(player_torso), CopyTexture + BlendTextures
//      (numColors=0x6C), тату-оверлей сверху + RwTextureDestroy;
//   3) GetTextureFromTxdAndLoadNextTxd кладёт ВЫБРАННУЮ текстуру торса (hashes[0]) поверх бленда и запрашивает
//      "player_legs" -> SetName(...,"torso") + RwTexDictionaryAddTexture;
//   4) LEGS: "legs"/"legs_fat"/"legs_ripped" из dict(player_legs), бленд, оверлей hashes[2],
//      запрос "player_face" (или hashes[1]) -> SetName(...,"legs");
//   5) FACE: dict запрошенного head-txd, "face"/"face_fat", бленд 2-текстурный, RemoveModel(headTxd) -> SetName(...,"head");
//   6) FEET: hashes[3] или "player_feet" -> первая текстура txd -> SetName(...,"feet");
//   7) аксессуары hashes[13..17]: текстура берётся из txd, запрошенного ПРЕДЫДУЩИМ вызовом, и именуется по своей части:
//      feet->"feet", necklace->"necklace", bracelet->"watch", glasses->"glasses", hats->"hat", special->"extra1".
// Имена итоговых текстур обязаны совпадать с BODY_PART_TEX_NAMES из CreateSkinnedClump, иначе он не найдёт материалы.
// Проверка "txd загружен" в OG: *(char*)(txdId + [0xC88010]) < 0 => dict==nullptr; наблюдаемо эквивалентно
// CTxdStore::GetTxd (тот же приём, что в GetTextureFromTxdAndLoadNextTxd).
void CClothesBuilder::ConstructTextures(RwTexDictionary* dict, uint32* hashes, float factorA, float factorB, float factorC) {
    // Build the tattoo overlay from all tattoo slots (the texture of the previous slot is placed on top of the previous one)
    auto txdId = RequestTexture(hashes[4]); // CLOTHES_TEXTURE_LOWER_LEFT_ARM
    CStreaming::LoadRequestedModels();

    RwTexture* tattoos{};
    for (auto i = 5; i <= 12; i++) { // Remaining tattoo slots: UPPER_LEFT_ARM..UPPER_BACK
        int32 nextTxdId;
        tattoos = GetTextureFromTxdAndLoadNextTxd(tattoos, txdId, hashes[i], &nextTxdId);
        txdId   = nextTxdId;
    }
    int32 playerTorsoTxd;
    tattoos = GetTextureFromTxdAndLoadNextTxd(tattoos, txdId, CKeyGen::GetUppercaseKey("player_torso"), &playerTorsoTxd);

    // Torso
    CStreaming::LoadAllRequestedModels(true);
    const auto torsoClothesTxd = RequestTexture(hashes[0]); // CLOTHES_TEXTURE_TORSO
    CStreaming::LoadRequestedModels();
    const auto torsoDict = CTxdStore::GetTxd(playerTorsoTxd); // OG: nullptr if the txd slot isn't loaded
    const auto torsoBlend = CopyTexture(RwTexDictionaryFindNamedTexture(torsoDict, "torso"));
    if (!tattoos) {
        BlendTextures(torsoBlend, RwTexDictionaryFindNamedTexture(torsoDict, "torso_fat"), RwTexDictionaryFindNamedTexture(torsoDict, "torso_ripped"), factorA, factorB, factorC, 0x6C /* 108 */);
    } else {
        BlendTextures(torsoBlend, RwTexDictionaryFindNamedTexture(torsoDict, "torso_fat"), RwTexDictionaryFindNamedTexture(torsoDict, "torso_ripped"), factorA, factorB, factorC, 0x6C /* 108 */, tattoos);
        RwTextureDestroy(tattoos);
    }
    CStreaming::RemoveModel(TXDToModelId(playerTorsoTxd));

    // Place the player's chosen torso texture on top; request the legs base txd
    int32 playerLegsTxd;
    GetTextureFromTxdAndLoadNextTxd(torsoBlend, torsoClothesTxd, CKeyGen::GetUppercaseKey("player_legs"), &playerLegsTxd);
    RwTextureSetName(torsoBlend, "torso");
    RwTexDictionaryAddTexture(dict, torsoBlend);

    // Legs
    CStreaming::LoadAllRequestedModels(true);
    const auto legsClothesTxd = RequestTexture(hashes[2]); // CLOTHES_TEXTURE_LEGS
    CStreaming::LoadRequestedModels();
    const auto legsDict = CTxdStore::GetTxd(playerLegsTxd);
    const auto legsBlend = CopyTexture(RwTexDictionaryFindNamedTexture(legsDict, "legs"));
    BlendTextures(legsBlend, RwTexDictionaryFindNamedTexture(legsDict, "legs_fat"), RwTexDictionaryFindNamedTexture(legsDict, "legs_ripped"), factorA, factorB, factorC, 0x6C /* 108 */);
    CStreaming::RemoveModel(TXDToModelId(playerLegsTxd));

    // Place the player's chosen legs texture on top; request the face txd (or the player's head clothing txd)
    int32 faceTxd;
    GetTextureFromTxdAndLoadNextTxd(legsBlend, legsClothesTxd, hashes[1] ? hashes[1] : CKeyGen::GetUppercaseKey("player_face"), &faceTxd); // 1 == CLOTHES_TEXTURE_HEAD
    RwTextureSetName(legsBlend, "legs");
    RwTexDictionaryAddTexture(dict, legsBlend);

    // Face / head
    CStreaming::LoadAllRequestedModels(true);
    int32 feetTxd;
    if (hashes[3]) { // CLOTHES_TEXTURE_SHOES
        feetTxd = RequestTexture(hashes[3]);
    } else {
        feetTxd = RequestTexture(CKeyGen::GetUppercaseKey("player_feet"));
    }
    CStreaming::LoadRequestedModels();
    const auto faceDict = CTxdStore::GetTxd(faceTxd);
    const auto face     = RwTexDictionaryFindNamedTexture(faceDict, "face");
    const auto faceFat  = RwTexDictionaryFindNamedTexture(faceDict, "face_fat");
    RwTexture* headTex;
    if (!face) {
        headTex = CopyTexture(GetFirstTexture(faceDict));
    } else {
        headTex = CopyTexture(face);
        if (faceFat) {
            const auto sum = factorA + factorB + factorC;
            BlendTextures(headTex, faceFat, (factorA + factorC) / sum, factorB / sum, 0x6C /* 108 */);
        }
    }
    CStreaming::RemoveModel(TXDToModelId(faceTxd));
    RwTextureSetName(headTex, "head");
    RwTexDictionaryAddTexture(dict, headTex);

    // Feet (they have no fat/ripped variants, the plain texture of the txd is used)
    const auto feetTex = GetTextureFromTxdAndLoadNextTxd(nullptr, feetTxd, hashes[13], &txdId); // 13 == CLOTHES_TEXTURE_NECKLACE
    RwTextureSetName(feetTex, "feet");
    RwTexDictionaryAddTexture(dict, feetTex);

    // Accessories: each call copies the first texture of the previously requested txd and names it after that part
    int32 nextTxdId;
    if (const auto t = GetTextureFromTxdAndLoadNextTxd(nullptr, txdId, hashes[14], &nextTxdId)) { // 14 == CLOTHES_TEXTURE_BRACELET
        RwTextureSetName(t, "necklace");
        RwTexDictionaryAddTexture(dict, t);
    }
    txdId = nextTxdId;
    if (const auto t = GetTextureFromTxdAndLoadNextTxd(nullptr, txdId, hashes[15], &nextTxdId)) { // 15 == CLOTHES_TEXTURE_GLASSES
        RwTextureSetName(t, "watch");
        RwTexDictionaryAddTexture(dict, t);
    }
    txdId = nextTxdId;
    if (const auto t = GetTextureFromTxdAndLoadNextTxd(nullptr, txdId, hashes[16], &nextTxdId)) { // 16 == CLOTHES_TEXTURE_HATS
        RwTextureSetName(t, "glasses");
        RwTexDictionaryAddTexture(dict, t);
    }
    txdId = nextTxdId;
    if (const auto t = GetTextureFromTxdAndLoadNextTxd(nullptr, txdId, hashes[17], &nextTxdId)) { // 17 == CLOTHES_TEXTURE_SPECIAL
        RwTextureSetName(t, "hat");
        RwTexDictionaryAddTexture(dict, t);
    }
    txdId = nextTxdId;

    // extra1 (no next txd to request => handled manually)
    if (txdId != -1) {
        CStreaming::LoadAllRequestedModels(true);
        const auto tex  = GetFirstTexture(CTxdStore::GetTxd(txdId)); // OG: nullptr if the txd slot isn't loaded
        const auto copy = CopyTexture(tex);
        CStreaming::RemoveModel(TXDToModelId(txdId));
        if (copy) {
            RwTextureSetName(copy, "extra1");
            RwTexDictionaryAddTexture(dict, copy);
        }
    }
}

// 0x5A6530
// Перевод с декомпила 0x5A6530: сумма вершин/треугольников непустых геометрий => RpGeometryCreate(nVerts, nTris, 0x35);
// копия verts/normals/uvs/треугольников (+материалы pMaterial[i]) со смещением vertBase;
// RpMorphTargetCalcBoundingSphere -> mt->boundingSphere; RpGeometryUnlock без парного Lock (в OG его нет);
// new RwMatrixWeights[nVerts] + new uint32[nVerts]; на каждую геометрию BuildBoneIndexConversionTable
// (стековая таблица 64 байта), indices[k]=convTable[skinIdxs[4v+k]], weights[v]=skinWeights[v].
void CClothesBuilder::ConstructGeometryAndSkinArrays(RpHAnimHierarchy* pBoneHier, RpGeometry** ppGeometry, RwMatrixWeights** ppWeights, uint32** ppIndices, uint32 numModels, RpGeometry** pGeometrys, RpMaterial** pMaterial) {
    // Sum up vertex & triangle counts of all non-null geometries
    auto numVerts = 0;
    auto numTris  = 0;
    for (auto i = 0u; i < numModels; i++) {
        if (const auto g = pGeometrys[i]) {
            numVerts += RpGeometryGetNumVertices(g);
            numTris  += RpGeometryGetNumTriangles(g);
        }
    }

    const auto geo = RpGeometryCreate(numVerts, numTris, 0x35 /* OG format: TRISTRIP|TEXTURED|NORMALS|LIGHT */);
    *ppGeometry = geo;

    const auto mt      = RpGeometryGetMorphTarget(geo, 0);
    auto       outVert = (CVector*)RpMorphTargetGetVertices(mt);
    auto       outNrml = (CVector*)RpMorphTargetGetVertexNormals(mt);
    auto       outUVs  = RpGeometryGetVertexTexCoords(geo, 1);
    auto       outTri  = RpGeometryGetTriangles(geo);

    uint32 vertBase{}; // Offset of the current geometry's vertices in the new geometry
    for (auto i = 0u; i < numModels; i++) {
        const auto g = pGeometrys[i];
        if (!g) {
            continue;
        }
        const auto numV     = RpGeometryGetNumVertices(g);
        const auto numT     = RpGeometryGetNumTriangles(g);
        const auto srcMt    = RpGeometryGetMorphTarget(g, 0);
        const auto srcVerts = (const CVector*)RpMorphTargetGetVertices(srcMt);
        const auto srcNrmls = (const CVector*)RpMorphTargetGetVertexNormals(srcMt);
        const auto srcUVs   = RpGeometryGetVertexTexCoords(g, 1);

        for (auto v = 0; v < numV; v++) { // Copy vertices, normals and uv coords
            outVert[vertBase + v] = srcVerts[v];
            outNrml[vertBase + v] = srcNrmls[v];
            outUVs[vertBase + v]  = srcUVs[v];
        }

        const auto srcTris = RpGeometryGetTriangles(g);
        for (auto t = 0; t < numT; t++, outTri++) { // Copy triangle vertex indices (offset by vertBase) and material
            RpGeometryTriangleSetVertexIndices(
                geo,
                outTri,
                static_cast<RwUInt16>(srcTris[t].vertIndex[0] + vertBase),
                static_cast<RwUInt16>(srcTris[t].vertIndex[1] + vertBase),
                static_cast<RwUInt16>(srcTris[t].vertIndex[2] + vertBase)
            );
            RpGeometryTriangleSetMaterial(geo, outTri, pMaterial[i]);
        }

        vertBase += numV;
    }

    // (Re)calculate the bounding sphere of the morph target
    RwSphere boundingSphere;
    RpMorphTargetCalcBoundingSphere(mt, &boundingSphere);
    mt->boundingSphere = boundingSphere;

    RpGeometryUnlock(geo); // NOTE: There's no matching RpGeometryLock in the OG

    // Build the combined skin arrays
    const auto allWeights = new RwMatrixWeights[numVerts]; // NOTE: OG pairs this with DestroySkinArrays
    *ppWeights = allWeights;
    const auto allIdxs = new uint32[numVerts];
    *ppIndices = allIdxs;

    uint32 out{};
    for (auto i = 0u; i < numModels; i++) {
        const auto g = pGeometrys[i];
        if (!g) {
            continue;
        }
        const auto skin = RpSkinGeometryGetSkin(g);
        const auto numV = RpGeometryGetNumVertices(g);

        uint8 convTable[64];
        CClothesBuilder::BuildBoneIndexConversionTable(convTable, pBoneHier, (int32)i);

        for (auto v = 0; v < numV; v++, out++) {
            const auto idxs    = RpSkinGetVertexBoneIndices(skin);  // NOTE: OG calls these per vertex
            const auto weights = RpSkinGetVertexBoneWeights(skin);  //
            for (auto b = 0; b < 4; b++) {
                allIdxs[out * 4 + b] = convTable[idxs[v * 4 + b]];
            }
            allWeights[out] = weights[v];
        }
    }
}

// 0x5A69D0
RpClump* CClothesBuilder::CreateSkinnedClump(RpClump* bones, RwTexDictionary* dict, CPedClothesDesc& ndscr, const CPedClothesDesc* odscr, bool bCutscenePlayer) {
    LoadCdDirectory();

    const struct {
        eClothesModelPart mp;
        const char*       name;
    } parts[]{
        {eClothesModelPart::CLOTHES_MODEL_TORSO, "torso"},
        {eClothesModelPart::CLOTHES_MODEL_HEAD, "head"},
        {eClothesModelPart::CLOTHES_MODEL_HANDS, "hands"},
        {eClothesModelPart::CLOTHES_MODEL_LEGS, "legs"},
        {eClothesModelPart::CLOTHES_MODEL_SHOES, "feet"}
    };
    for (const auto [mp, name] : parts) {
        if (!ndscr.m_anModelKeys[(int)mp]) {
            ndscr.SetModel(name, mp);
        }
    }

    if (odscr) {
        ms_geometryHasChanged = false;
        ms_ratiosHaveChanged  = false;
        if (odscr->m_fFatStat != ndscr.m_fFatStat || odscr->m_fMuscleStat != ndscr.m_fMuscleStat) {
            ms_textureHasChanged = true;
            ms_geometryHasChanged = true;
        } else {
            ms_textureHasChanged = true;
        }
        ms_geometryHasChanged = !rng::equal(ndscr.m_anModelKeys, odscr->m_anModelKeys);
        ms_ratiosHaveChanged  = !rng::equal(ndscr.m_anTextureKeys, odscr->m_anTextureKeys);
        if (!ms_ratiosHaveChanged && !ms_geometryHasChanged && !ms_textureHasChanged) {
            return nullptr;
        }
    } else {
        ms_ratiosHaveChanged = ms_geometryHasChanged = ms_textureHasChanged = true;
    }
    CPedClothesDesc dscr = ndscr;
    PreprocessClothesDesc(dscr, bCutscenePlayer);

    //> 0x5A42B0 - Calculate blend ratios
    float rNormal, rFatness, rMuscle;
    {
        rMuscle  = std::clamp(CStats::GetStatValue(STAT_MUSCLE) / 1000.f, 0.f, 1.f);
        rFatness = std::clamp((dscr.m_fFatStat - 200.f) / 800.f, 0.f, 1.f);
        rNormal  = 1.f - rMuscle - rFatness;
        if (rNormal <= 0.f) {
            const auto t = 1.f / (rFatness + rMuscle);
            rMuscle  *= t;
            rFatness *= t;
            rNormal   = 0.f;
        }
    }

    if ((ms_textureHasChanged || ms_ratiosHaveChanged) && !bCutscenePlayer) {
        RwTexDictionaryForAllTextures(dict, [](RwTexture* t, void* data) {
            RwTexDictionaryRemoveTexture(t);
            RwTextureDestroy(t);
            return t;
        }, nullptr);
        ConstructTextures(dict, dscr.m_anTextureKeys.data(), rNormal, rFatness, rMuscle);
    }

    constexpr auto NO_BODY_PARTS = 10;

    constexpr const char* BODY_PART_TEX_NAMES[NO_BODY_PARTS]{
        "torso",
        "head",
        "torso",
        "legs",
        "feet",
        "necklace",
        "watch",
        "glasses",
        "hat",
        "extra1"
    };

    //> 0x5A6B2C
    RpMaterial*       ms[NO_BODY_PARTS];
    for (uint32 i{}; const auto name : BODY_PART_TEX_NAMES) {
        ms[i++] = [&]() -> RpMaterial* {
            if (const auto tex = RwTexDictionaryFindNamedTexture(dict, name)) {
                const auto mat = RpMaterialCreate();
                RpMaterialSetTexture(mat, tex);
                const auto clr = RwRGBA(0xFF, 0xFF, 0xFF, 0xFF);
                RpMaterialSetColor(mat, &clr);
                return mat;
            }
            return nullptr;
        }();
    }

    //> 0x5A6C5D
    RpGeometry* gs[NO_BODY_PARTS]{};
    ConstructGeometryArray(gs, dscr.m_anModelKeys.data(), rNormal, rFatness, rMuscle);

    //> 0x5A6C6A
    const auto boneAtomic = GetFirstAtomic(bones);
    const auto boneSkin   = RpSkinGeometryGetSkin(RpAtomicGetGeometry(boneAtomic));
    const auto boneAnimHr = RpSkinAtomicGetHAnimHierarchy(boneAtomic);

    RwMatrixWeights* boneWeights;
    RwUInt32*        boneIdxs;
    RpGeometry*      tmpGeo;
    ConstructGeometryAndSkinArrays(
        boneAnimHr,
        &tmpGeo,
        &boneWeights,
        &boneIdxs,
        NO_BODY_PARTS,
        gs,
        ms
    );
    RpSkinGeometrySetSkin(
        tmpGeo,
        RpSkinCreate(
            RpGeometryGetNumVertices(tmpGeo),
            RpSkinGetNumBones(boneSkin),
            boneWeights,
            boneIdxs,
            const_cast<RwMatrix*>(RpSkinGetSkinToBoneMatrices(boneSkin)) // TODO
        )
    );
    DestroySkinArrays(boneWeights, boneIdxs);
    
    const auto hier = RpHAnimHierarchyCreateFromHierarchy(
        boneAnimHr,
        (RpHAnimHierarchyFlag)boneAnimHr->flags,       // TODO: Use function to access
        boneAnimHr->currentAnim->maxInterpKeyFrameSize // TODO: Use function to access
    );

    const auto childFrame = RwFrameCreate();
    RpHAnimFrameSetHierarchy(childFrame, hier);

    const auto atomic = RpAtomicCreate();
    RpAtomicSetGeometry(atomic, tmpGeo, NULL);
    RpSkinAtomicSetHAnimHierarchy(atomic, hier);
    RpAtomicSetFrame(atomic, childFrame);
    RpSkinAtomicSetType(atomic, rpSKINTYPEGENERIC);

    const auto rootFrame = RwFrameCreate();
    RwFrameAddChild(rootFrame, childFrame);

    const auto clump = RpClumpCreate();
    RpClumpSetFrame(clump, rootFrame);
    RpClumpAddAtomic(clump, atomic);

    // Free memory
    {
        RpGeometryDestroy(tmpGeo);

        RwTexDictionarySetCurrent(dict);
        for (auto i = 0; i < NO_BODY_PARTS; i++) {
            if (gs[i]) {
                RpGeometryDestroy(gs[i]);
            }
            if (ms[i]) {
                RpMaterialDestroy(ms[i]);
            }
        }
    }

    return clump;
}
