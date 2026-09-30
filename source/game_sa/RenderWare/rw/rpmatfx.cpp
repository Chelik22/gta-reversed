#include "StdInc.h"

#include "rpmatfx.h"

void RpMatFXMaterialDataSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    ((void(__cdecl *)(RwInt32, RwInt32))0x810700)(blockSize, numBlocksToPrealloc);
}

/* --- MatFX internals (statics were left unreversed in the original binary; referenced by address) --- */

/* Helper used by the MatFX accessors, once per effect change. The original
 * (0x816280) unconditionally returns TRUE - kept as an address call, it is not
 * part of this slice. */
static inline RwBool MatFXNotifyUVTransform(MatFXEffectData* fx, RwInt32 code) {
    return ((RwBool(__cdecl *)(MatFXEffectData*, RwInt32))0x816280)(fx, code);
}

/* 0x810CA0: Frees all textures referenced by the effect data and zeroes it */
static rpMatFXMaterialData* MatFXDestroyData(rpMatFXMaterialData* matfx) {
    ZoneScoped;
    for (RwInt32 pass = rpSECONDPASS; pass < rpMAXPASS; pass++) {
        auto& fx = matfx->data[pass];
        RwTexture* extraTex = nullptr;
        switch (fx.flag) {
        case rpMATFXEFFECTBUMPMAP:
            if (fx.data.bumpMap.texture) {
                RwTextureDestroy(fx.data.bumpMap.texture);
            }
            extraTex = fx.data.bumpMap.bumpTexture;
            break;
        case rpMATFXEFFECTENVMAP:
            extraTex = fx.data.envMap.texture;
            break;
        case rpMATFXEFFECTDUAL:
            extraTex = fx.data.dual.texture;
            break;
        case rpMATFXEFFECTUVTRANSFORM:
            fx.data.uvAnim.baseTransform  = nullptr;
            fx.data.uvAnim.dualTransform = nullptr;
            break;
        default:
            continue;
        }
        if (extraTex) {
            RwTextureDestroy(extraTex);
        }
    }
    memset(matfx, 0, sizeof(rpMatFXMaterialData));
    return matfx;
}

/* Shared implementation of the two identical effect lookups (0x811150 / 0x8111A0) */
static MatFXEffectData* MatFXFindEffect(const RpMaterial* material, RpMatFXMaterialFlags flags) {
    auto matfx = *(rpMatFXMaterialData**)((RwUInt8*)material + MatFXMaterialDataOffset);
    for (RwInt32 pass = rpSECONDPASS; pass < rpMAXPASS; pass++) {
        if (matfx->data[pass].flag == flags) {
            return &matfx->data[pass];
        }
    }
    return nullptr;
}

/* 0x811150 */
static MatFXEffectData* MatFXFindEffectFirst(const RpMaterial* material, RpMatFXMaterialFlags flags) {
    return MatFXFindEffect(material, flags);
}

/* 0x8111A0: Identical twin of 0x811150 in the original binary */
static MatFXEffectData* MatFXFindEffectSecond(const RpMaterial* material, RpMatFXMaterialFlags flags) {
    return MatFXFindEffect(material, flags);
}

RwBool RpMatFXPluginAttach() {
    ZoneScoped;
    /* Engine-wide plugin data (0x120 = rwID_MATFX) */
    if (((RwInt32(__cdecl *)(RwInt32, RwUInt32, RwPluginObjectConstructor, RwPluginObjectDestructor))0x7F2BB0)(0, 0x120, (RwPluginObjectConstructor)0x810BF0, (RwPluginObjectDestructor)0x810BB0) < 0) {
        return false;
    }
    MatFXMaterialDataOffset = ((RwInt32(__cdecl *)(RwInt32, RwUInt32, RwPluginObjectConstructor, RwPluginObjectDestructor, RwPluginObjectCopy))0x74DBF0)(sizeof(rpMatFXMaterialData*), 0x120, (RwPluginObjectConstructor)0x810C40, (RwPluginObjectDestructor)0x810C60, (RwPluginObjectCopy)0x810D20);
    if (MatFXMaterialDataOffset < 0) {
        return false;
    }
    if (((RwInt32(__cdecl *)(RwUInt32, RwPluginDataChunkReadCallBack, RwPluginDataChunkWriteCallBack, RwPluginDataChunkGetSizeCallBack))0x74DC20)(0x120, (RwPluginDataChunkReadCallBack)0x811410, (RwPluginDataChunkWriteCallBack)0x8111F0, (RwPluginDataChunkGetSizeCallBack)0x811970) < 0) {
        return false;
    }
    MatFXAtomicDataOffset = ((RwInt32(__cdecl *)(RwInt32, RwUInt32, RwPluginObjectConstructor, RwPluginObjectDestructor, RwPluginObjectCopy))0x74BDA0)(sizeof(rpMatFXMaterialData*), 0x120, (RwPluginObjectConstructor)0x811A00, (RwPluginObjectDestructor)0x811A20, (RwPluginObjectCopy)0x811A40);
    if (MatFXAtomicDataOffset < 0) {
        return false;
    }
    if (((RwInt32(__cdecl *)(RwUInt32, RwPluginDataChunkReadCallBack, RwPluginDataChunkWriteCallBack, RwPluginDataChunkGetSizeCallBack))0x74BE00)(0x120, (RwPluginDataChunkReadCallBack)0x811A90, (RwPluginDataChunkWriteCallBack)0x811A60, (RwPluginDataChunkGetSizeCallBack)0x811AE0) < 0) {
        return false;
    }
    MatFXWorldSectorDataOffset = ((RwInt32(__cdecl *)(RwInt32, RwUInt32, RwPluginObjectConstructor, RwPluginObjectDestructor, RwPluginObjectCopy))0x761C90)(sizeof(rpMatFXMaterialData*), 0x120, (RwPluginObjectConstructor)0x811B00, (RwPluginObjectDestructor)0x811B20, (RwPluginObjectCopy)0x811B40);
    if (MatFXWorldSectorDataOffset < 0) {
        return false;
    }
    if (((RwInt32(__cdecl *)(RwUInt32, RwPluginDataChunkReadCallBack, RwPluginDataChunkWriteCallBack, RwPluginDataChunkGetSizeCallBack))0x761CC0)(0x120, (RwPluginDataChunkReadCallBack)0x811B90, (RwPluginDataChunkWriteCallBack)0x811B60, (RwPluginDataChunkGetSizeCallBack)0x811BE0) < 0) {
        return false;
    }
    return true;
}

RpAtomic* RpMatFXAtomicEnableEffects(RpAtomic* atomic) {
    return ((RpAtomic*(__cdecl *)(RpAtomic*))0x811C00)(atomic);
}

/* 0x811C30 */
RwBool RpMatFXAtomicQueryEffects(RpAtomic* atomic) {
    ZoneScoped;
    return *(RwBool*)((RwUInt8*)atomic + MatFXAtomicDataOffset);
}

RpWorldSector* RpMatFXWorldSectorEnableEffects(RpWorldSector* worldSector) {
    return ((RpWorldSector*(__cdecl *)(RpWorldSector*))0x811C40)(worldSector);
}

RwBool RpMatFXWorldSectorQueryEffects(RpWorldSector* worldSector) {
    return ((RwBool(__cdecl *)(RpWorldSector*))0x811C70)(worldSector);
}

/* 0x811C80 */
RpMaterial* RpMatFXMaterialSetEffects(RpMaterial* material, RpMatFXMaterialFlags flags) {
    ZoneScoped;
    auto& slot = *(rpMatFXMaterialData**)((RwUInt8*)material + MatFXMaterialDataOffset);
    if (slot == nullptr) {
        slot = (rpMatFXMaterialData*)RwFreeListAlloc(MatFXFreeList, 0x30120u);
        if (slot == nullptr) {
            return nullptr;
        }
        memset(slot, 0, sizeof(rpMatFXMaterialData));
    }

    // Destroy the old effect data if switching to another effect
    if (flags == rpMATFXEFFECTNULL || (slot->flags != rpMATFXEFFECTNULL && slot->flags != flags)) {
        MatFXDestroyData(slot);
    }
    slot->flags = flags;

    switch (flags) {
    case rpMATFXEFFECTBUMPMAP:
        slot->data[rpSECONDPASS].flag = rpMATFXEFFECTBUMPMAP;
        break;
    case rpMATFXEFFECTENVMAP:
        slot->data[rpSECONDPASS].flag = rpMATFXEFFECTENVMAP;
        break;
    case rpMATFXEFFECTBUMPENVMAP:
        slot->data[rpSECONDPASS].flag = rpMATFXEFFECTBUMPMAP;
        slot->data[rpTHIRDPASS].flag  = rpMATFXEFFECTENVMAP;
        break;
    case rpMATFXEFFECTDUAL: {
        slot->data[rpSECONDPASS].flag = rpMATFXEFFECTDUAL;
        if (auto fx = MatFXFindEffect(material, rpMATFXEFFECTDUAL)) {
            fx->data.dual.srcBlendMode = (RwBlendFunction)5;
            fx->data.dual.dstBlendMode = (RwBlendFunction)6;
            MatFXNotifyUVTransform(fx, 10);
            MatFXNotifyUVTransform(fx, 11);
        }
        break;
    }
    case rpMATFXEFFECTUVTRANSFORM:
        slot->data[rpSECONDPASS].flag = rpMATFXEFFECTUVTRANSFORM;
        break;
    case rpMATFXEFFECTDUALUVTRANSFORM: {
        slot->data[rpSECONDPASS].flag = rpMATFXEFFECTUVTRANSFORM;
        slot->data[rpTHIRDPASS].flag  = rpMATFXEFFECTDUAL;
        if (auto fx = MatFXFindEffect(material, rpMATFXEFFECTDUAL)) {
            fx->data.dual.srcBlendMode = (RwBlendFunction)5;
            fx->data.dual.dstBlendMode = (RwBlendFunction)6;
            MatFXNotifyUVTransform(fx, 10);
            MatFXNotifyUVTransform(fx, 11);
        }
        break;
    }
    default:
        break;
    }
    return material;
}

RpMaterial* RpMatFXMaterialSetupBumpMap(RpMaterial* material, RwTexture* texture, RwFrame* frame, RwReal coef) {
    return ((RpMaterial*(__cdecl *)(RpMaterial*, RwTexture*, RwFrame*, RwReal))0x811E00)(material, texture, frame, coef);
}

RpMaterial* RpMatFXMaterialSetupEnvMap(RpMaterial* material, RwTexture* texture, RwFrame* frame, RwBool useFrameBufferAlpha, RwReal coef) {
    return ((RpMaterial*(__cdecl *)(RpMaterial*, RwTexture*, RwFrame*, RwBool, RwReal))0x811ED0)(material, texture, frame, useFrameBufferAlpha, coef);
}

RpMaterial* RpMatFXMaterialSetupDualTexture(RpMaterial* material, RwTexture* texture, RwBlendFunction srcBlendMode, RwBlendFunction dstBlendMode) {
    return ((RpMaterial*(__cdecl *)(RpMaterial*, RwTexture*, RwBlendFunction, RwBlendFunction))0x812040)(material, texture, srcBlendMode, dstBlendMode);
}

/* 0x812140 */
RpMatFXMaterialFlags RpMatFXMaterialGetEffects(const RpMaterial* material) {
    ZoneScoped;
    auto matfx = *(rpMatFXMaterialData**)((RwUInt8*)material + MatFXMaterialDataOffset);
    if (matfx == nullptr) {
        return rpMATFXEFFECTNULL;
    }
    return matfx->flags;
}

RpMaterial* RpMatFXMaterialSetBumpMapTexture(RpMaterial* material, RwTexture* texture) {
    ZoneScoped;
    auto fx = MatFXFindEffect(material, rpMATFXEFFECTBUMPMAP);

    // Destroy the old textures first
    if (fx->data.bumpMap.bumpTexture) {
        RwTextureDestroy(fx->data.bumpMap.bumpTexture);
        fx->data.bumpMap.bumpTexture = nullptr;
    }
    if (fx->data.bumpMap.texture) {
        RwTextureDestroy(fx->data.bumpMap.texture);
        fx->data.bumpMap.texture    = nullptr;
        fx->data.bumpMap.invBumpWidth = 0;
    }

    RwTexture* baseTexture = material->texture;
    if (texture != nullptr) {
        fx->data.bumpMap.bumpTexture = texture;
        texture->refCount++;

        // Both textures must have a valid raster
        if (texture->raster == nullptr || texture->raster->width == 0) {
            return material;
        }
        if (baseTexture != nullptr && (baseTexture->raster == nullptr || baseTexture->raster->width == 0)) {
            return material;
        }

        // Build the combined name of the base and bump textures (interleaved chars)
        RwChar name[rwTEXTUREBASENAMELENGTH];
        memset(name, 0, sizeof(name));
        {
            const RwChar* names[2];
            names[1] = texture->name;
            names[0] = baseTexture != nullptr ? baseTexture->name : texture->name;
            RwInt32 numChars = 0;
            RwChar* out = name;
            do {
                if (names[0][0] == '\0' && names[1][0] == '\0') {
                    break;
                }
                for (RwInt32 i = 0; i < 2; i++) {
                    if (*names[i] != '\0') {
                        *out++ = *names[i]++;
                        numChars++;
                    }
                }
            } while (numChars < 30);
            *out = '\0';
        }

        // Look for an already combined texture in the current dictionary, create it if missing
        RwTexDictionary* dict = RwTexDictionaryGetCurrent();
        if (dict != nullptr) {
            fx->data.bumpMap.texture = RwTexDictionaryFindNamedTexture(dict, name);
        } else {
            fx->data.bumpMap.texture = nullptr;
        }

        if (fx->data.bumpMap.texture == nullptr) {
            // Create the combined (base + bump) texture (helper 0x816290 unreversed)
            auto newTex = ((RwTexture * (__cdecl*)(RwTexture*, RwTexture*))0x816290)(baseTexture, texture);
            fx->data.bumpMap.texture = newTex;
            if (newTex == nullptr) {
                return nullptr;
            }
            if (dict != nullptr) {
                RwTexDictionaryAddTexture(dict, newTex);
            }
        } else {
            fx->data.bumpMap.texture->refCount++;
        }
        baseTexture = fx->data.bumpMap.texture;
    }

    fx->data.bumpMap.invBumpWidth = 1.0f / (RwReal)baseTexture->raster->width;
    return material;
}

RpMaterial* RpMatFXMaterialSetBumpMapFrame(RpMaterial* material, RwFrame* frame) {
    return ((RpMaterial*(__cdecl *)(RpMaterial*, RwFrame*))0x812320)(material, frame);
}

RpMaterial* RpMatFXMaterialSetBumpMapCoefficient(RpMaterial* material, RwReal coef) {
    return ((RpMaterial*(__cdecl *)(RpMaterial*, RwReal))0x812380)(material, coef);
}

RwTexture* RpMatFXMaterialGetBumpMapTexture(const RpMaterial* material) {
    return ((RwTexture*(__cdecl *)(const RpMaterial*))0x812430)(material);
}

RwTexture* RpMatFXMaterialGetBumpMapBumpedTexture(const RpMaterial* material) {
    return ((RwTexture*(__cdecl *)(const RpMaterial*))0x8123E0)(material);
}

RwFrame* RpMatFXMaterialGetBumpMapFrame(const RpMaterial* material) {
    return ((RwFrame*(__cdecl *)(const RpMaterial*))0x812480)(material);
}

RwReal RpMatFXMaterialGetBumpMapCoefficient(const RpMaterial* material) {
    return ((RwReal(__cdecl *)(const RpMaterial*))0x8124D0)(material);
}

RpMaterial* RpMatFXMaterialSetEnvMapTexture(RpMaterial* material, RwTexture* texture) {
    return ((RpMaterial*(__cdecl *)(RpMaterial*, RwTexture*))0x812530)(material, texture);
}

/* 0x8125B0 */
RpMaterial* RpMatFXMaterialSetEnvMapFrame(RpMaterial* material, RwFrame* frame) {
    ZoneScoped;
    if (auto fx = MatFXFindEffect(material, rpMATFXEFFECTENVMAP)) {
        fx->data.envMap.frame = frame;
    }
    return material;
}

RpMaterial* RpMatFXMaterialSetEnvMapFrameBufferAlpha(RpMaterial* material, RwBool useFrameBufferAlpha) {
    return ((RpMaterial*(__cdecl *)(RpMaterial*, RwBool))0x812610)(material, useFrameBufferAlpha);
}

/* 0x812680 */
RpMaterial* RpMatFXMaterialSetEnvMapCoefficient(RpMaterial* material, RwReal coef) {
    ZoneScoped;
    if (auto fx = MatFXFindEffect(material, rpMATFXEFFECTENVMAP)) {
        fx->data.envMap.coef = coef;
    }
    return material;
}

RwTexture* RpMatFXMaterialGetEnvMapTexture(const RpMaterial* material) {
    return ((RwTexture*(__cdecl *)(const RpMaterial*))0x8126F0)(material);
}

RwFrame* RpMatFXMaterialGetEnvMapFrame(const RpMaterial* material) {
    return ((RwFrame*(__cdecl *)(const RpMaterial*))0x812740)(material);
}

RwBool RpMatFXMaterialGetEnvMapFrameBufferAlpha(const RpMaterial* material) {
    return ((RwBool(__cdecl *)(const RpMaterial*))0x812790)(material);
}

RwReal RpMatFXMaterialGetEnvMapCoefficient(const RpMaterial* material) {
    return ((RwReal(__cdecl *)(const RpMaterial*))0x8127E0)(material);
}

RpMaterial* RpMatFXMaterialSetDualTexture(RpMaterial* material, RwTexture* texture) {
    return ((RpMaterial*(__cdecl *)(RpMaterial*, RwTexture*))0x812830)(material, texture);
}

/* 0x8128C0 */
RpMaterial* RpMatFXMaterialSetDualBlendModes(RpMaterial* material, RwBlendFunction srcBlendMode, RwBlendFunction dstBlendMode) {
    ZoneScoped;
    if (auto fx = MatFXFindEffect(material, rpMATFXEFFECTDUAL)) {
        fx->data.dual.srcBlendMode = srcBlendMode;
        fx->data.dual.dstBlendMode = dstBlendMode;
        MatFXNotifyUVTransform(fx, 10);
        MatFXNotifyUVTransform(fx, 11);
    }
    return material;
}

RwTexture* RpMatFXMaterialGetDualTexture(const RpMaterial* material) {
    return ((RwTexture*(__cdecl *)(const RpMaterial*))0x812930)(material);
}

const RpMaterial* RpMatFXMaterialGetDualBlendModes(const RpMaterial* material, RwBlendFunction* srcBlendMode, RwBlendFunction* dstBlendMode) {
    return ((const RpMaterial*(__cdecl *)(const RpMaterial*, RwBlendFunction*, RwBlendFunction*))0x812980)(material, srcBlendMode, dstBlendMode);
}

RpMaterial* RpMatFXMaterialSetUVTransformMatrices(RpMaterial* material, RwMatrix* baseTransform, RwMatrix* dualTransform) {
    return ((RpMaterial*(__cdecl *)(RpMaterial*, RwMatrix*, RwMatrix*))0x8129E0)(material, baseTransform, dualTransform);
}

const RpMaterial* RpMatFXMaterialGetUVTransformMatrices(const RpMaterial* material, RwMatrix* baseTransform, RwMatrix* dualTransform) {
    return ((const RpMaterial*(__cdecl *)(const RpMaterial*, RwMatrix*, RwMatrix*))0x812A50)(material, baseTransform, dualTransform);
}

RxPipeline* RpMatFXGetD3D9Pipeline(RpMatFXD3D9Pipeline d3d9Pipeline) {
    return ((RxPipeline*(__cdecl *)(RpMatFXD3D9Pipeline))0x8162F0)(d3d9Pipeline);
}

/* ---------------------------------------------------------------------------
 * RpMatFX hooks
 * --------------------------------------------------------------------------- */

void RpMatFXInjectHooks() {
    RH_ScopedCategory("Renderware");
    RH_ScopedNamespaceName("RpMatFX");

    RH_ScopedGlobalInstall(RpMatFXPluginAttach, 0x810AA0);
    RH_ScopedGlobalInstall(RpMatFXAtomicQueryEffects, 0x811C30);
    RH_ScopedGlobalInstall(RpMatFXMaterialSetEffects, 0x811C80);
    RH_ScopedGlobalInstall(RpMatFXMaterialGetEffects, 0x812140);
    RH_ScopedGlobalInstall(RpMatFXMaterialSetBumpMapTexture, 0x812160);
    RH_ScopedGlobalInstall(RpMatFXMaterialSetEnvMapFrame, 0x8125B0);
    RH_ScopedGlobalInstall(RpMatFXMaterialSetEnvMapCoefficient, 0x812680);
    RH_ScopedGlobalInstall(RpMatFXMaterialSetDualBlendModes, 0x8128C0);
    RH_ScopedGlobalInstall(MatFXFindEffectFirst, 0x811150);
    RH_ScopedGlobalInstall(MatFXFindEffectSecond, 0x8111A0);
    RH_ScopedGlobalInstall(MatFXDestroyData, 0x810CA0);
}
