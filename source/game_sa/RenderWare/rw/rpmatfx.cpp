#include "StdInc.h"

#include "rpmatfx.h"

void RpMatFXMaterialDataSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    static auto& MatFXFreeListBlockSize           = StaticRef<RwInt32>(0x8E29B8);
    static auto& MatFXFreeListNumBlocksToPrealloc = StaticRef<RwInt32>(0x8E29BC);

    MatFXFreeListBlockSize           = blockSize;
    MatFXFreeListNumBlocksToPrealloc = numBlocksToPrealloc;
}


RwBool RpMatFXPluginAttach() {
    static auto& MatFXAtomicDataOffset      = StaticRef<RwInt32>(0xC9AB84);
    static auto& MatFXWorldSectorDataOffset = StaticRef<RwInt32>(0xC9AB88);

    /* engine-level plugin: stream open/close callbacks */
    if (RwEngineRegisterPlugin(0, 0x120 /* rwID_MATFX, GTA vendor ID */,
                               (RwPluginObjectConstructor)0x810BF0,
                               (RwPluginObjectDestructor)0x810BB0) < 0) {
        return FALSE;
    }

    /* material plugin: data slot = 4 (pointer to rpMatFXMaterialDataGta) */
    MatFXMaterialDataOffset = RpMaterialRegisterPlugin(4, 0x120,
                                                       (RwPluginObjectConstructor)0x810C40,
                                                       (RwPluginObjectDestructor)0x810C60,
                                                       (RwPluginObjectCopy)0x810D20);
    if (MatFXMaterialDataOffset < 0) {
        return FALSE;
    }
    if (RpMaterialRegisterPluginStream(0x120,
                                       (RwPluginDataChunkReadCallBack)0x811410,
                                       (RwPluginDataChunkWriteCallBack)0x8111F0,
                                       (RwPluginDataChunkGetSizeCallBack)0x811970) < 0) {
        return FALSE;
    }

    /* atomic plugin */
    MatFXAtomicDataOffset = RpAtomicRegisterPlugin(4, 0x120,
                                                   (RwPluginObjectConstructor)0x811A00,
                                                   (RwPluginObjectDestructor)0x811A20,
                                                   (RwPluginObjectCopy)0x811A40);
    if (MatFXAtomicDataOffset < 0) {
        return FALSE;
    }
    if (RpAtomicRegisterPluginStream(0x120,
                                     (RwPluginDataChunkReadCallBack)0x811A90,
                                     (RwPluginDataChunkWriteCallBack)0x811A60,
                                     (RwPluginDataChunkGetSizeCallBack)0x811AE0) < 0) {
        return FALSE;
    }

    /* world sector plugin */
    MatFXWorldSectorDataOffset = RpWorldSectorRegisterPlugin(4, 0x120,
                                                             (RwPluginObjectConstructor)0x811B00,
                                                             (RwPluginObjectDestructor)0x811B20,
                                                             (RwPluginObjectCopy)0x811B40);
    if (MatFXWorldSectorDataOffset < 0) {
        return FALSE;
    }
    return RpWorldSectorRegisterPluginStream(0x120,
                                             (RwPluginDataChunkReadCallBack)0x811B90,
                                             (RwPluginDataChunkWriteCallBack)0x811B60,
                                             (RwPluginDataChunkGetSizeCallBack)0x811BE0) >= 0;
}


RpAtomic* RpMatFXAtomicEnableEffects(RpAtomic* atomic) {
    static auto& MatFXAtomicDataOffset = StaticRef<RwInt32>(0xC9AB84);

    RwInt32* slot = (RwInt32*)((RwUInt8*)atomic + MatFXAtomicDataOffset);
    if (*slot == 0) {
        if (!((RwBool(__cdecl*)(RpAtomic*))0x816260)(atomic)) {
            return nullptr;
        }
        *slot = 1;
    }
    return atomic;
}


RwBool RpMatFXAtomicQueryEffects(RpAtomic* atomic) {
    static auto& MatFXAtomicDataOffset = StaticRef<RwInt32>(0xC9AB84);

    return *(RwInt32*)((RwUInt8*)atomic + MatFXAtomicDataOffset);
}


RpWorldSector* RpMatFXWorldSectorEnableEffects(RpWorldSector* worldSector) {
    static auto& MatFXWorldSectorDataOffset = StaticRef<RwInt32>(0xC9AB88);

    RwInt32* slot = (RwInt32*)((RwUInt8*)worldSector + MatFXWorldSectorDataOffset);
    if (*slot == 0) {
        if (!((RwBool(__cdecl*)(RpWorldSector*))0x816270)(worldSector)) {
            return nullptr;
        }
        *slot = 1;
    }
    return worldSector;
}


RwBool RpMatFXWorldSectorQueryEffects(RpWorldSector* worldSector) {
    static auto& MatFXWorldSectorDataOffset = StaticRef<RwInt32>(0xC9AB88);

    return *(RwInt32*)((RwUInt8*)worldSector + MatFXWorldSectorDataOffset);
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
/* GTA SA rpmatfx internal data layout — exact offsets from the binary decompiles.
   ABI divergence vs rpmatfx.h MatFXBumpMapData: binary has bumpedTexture@+4, texture@+8
   (swapped). envMap/dual/uvAnim match the repo header layouts. */
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 generated base+bump texture (GetBumpMapBumpedTexture) */
    RwTexture *texture;        /* +0x08 user bump map (SetBumpMapTexture arg, GetBumpMapTexture) */
    RwReal     coef;           /* +0x0C stored NEGATED */
    RwReal     invBumpWidth;   /* +0x10 = 1.0f / bumpedTexture->raster->width */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;     /* rpmatfx.h: frame@+0 texture@+4 coef@+8 useFrameBufferAlpha@+0xC */
    MatFXDualData       dual;       /* rpmatfx.h: texture@+0 srcBlendMode@+4 dstBlendMode@+8 */
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS]; /* +0x00 / +0x18 */
    RpMatFXMaterialFlags  flags;           /* +0x30 combined flags */
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RpMaterial* RpMatFXMaterialSetEffects(RpMaterial* material, RpMatFXMaterialFlags flags) {
    static auto& MatFXMaterialFreeList = StaticRef<RwFreeList*>(0xC9AB80);

    rpMatFXMaterialDataGta*& slot = *(rpMatFXMaterialDataGta**)((RwUInt8*)material + MatFXMaterialDataOffset);
    if (slot == nullptr) {
        /* *(void*(__cdecl**)(RwGlobals+0x144))(freelist, 0x30120) — freelist alloc dispatch */
        slot = (rpMatFXMaterialDataGta*)(*(void*(__cdecl**)(RwFreeList*, RwUInt32))((RwUInt8*)RwEngineInstance + 0x144))(MatFXMaterialFreeList, 0x30120);
        if (slot == nullptr) {
            return nullptr;
        }
        memset(slot, 0, sizeof(rpMatFXMaterialDataGta)); /* decompile: 13-dword zero loop = 0x34 bytes */
    }

    if (flags == rpMATFXEFFECTNULL || (slot->flags != rpMATFXEFFECTNULL && slot->flags != flags)) {
        ((void(__cdecl*)(rpMatFXMaterialDataGta*))0x810CA0)(slot); /* internal effect cleanup */
    }
    slot->flags = flags;

    switch (flags) {
    case rpMATFXEFFECTBUMPMAP:
        slot->data[0].flag = rpMATFXEFFECTBUMPMAP;
        return material;

    case rpMATFXEFFECTENVMAP:
        slot->data[0].flag = rpMATFXEFFECTENVMAP;
        return material;

    case rpMATFXEFFECTBUMPENVMAP:
        slot->data[0].flag = rpMATFXEFFECTBUMPMAP;
        slot->data[1].flag = rpMATFXEFFECTENVMAP;
        return material;

    case rpMATFXEFFECTDUAL:
    case rpMATFXEFFECTDUALUVTRANSFORM: {
        if (flags == rpMATFXEFFECTDUAL) {
            slot->data[0].flag = rpMATFXEFFECTDUAL;
        } else {
            slot->data[0].flag = rpMATFXEFFECTUVTRANSFORM;
            slot->data[1].flag = rpMATFXEFFECTDUAL;
        }
        /* default blend modes on the pass that carries the DUAL effect */
        for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
            if (slot->data[pass].flag == rpMATFXEFFECTDUAL) {
                MatFXDualData* dual = &slot->data[pass].data.dual;
                dual->srcBlendMode = rwBLENDSRCALPHA;     /* 5 */
                dual->dstBlendMode = rwBLENDINVSRCALPHA;  /* 6 */
                ((void(__cdecl*)(void*, RwInt32))0x816280)(dual, rwRENDERSTATESRCBLEND);   /* 10 */
                ((void(__cdecl*)(void*, RwInt32))0x816280)(dual, rwRENDERSTATEDESTBLEND);  /* 11 */
                break;
            }
        }
        return material;
    }

    case rpMATFXEFFECTUVTRANSFORM:
        slot->data[0].flag = rpMATFXEFFECTUVTRANSFORM;
        return material;

    default:
        return material;
    }
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 */
    RwTexture *texture;        /* +0x08 */
    RwReal     coef;           /* +0x0C stored NEGATED */
    RwReal     invBumpWidth;   /* +0x10 */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;
    MatFXDualData       dual;
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS];
    RpMatFXMaterialFlags  flags;
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RpMaterial* RpMatFXMaterialSetupBumpMap(RpMaterial* material, RwTexture* texture, RwFrame* frame, RwReal coef) {
    if (RpMatFXMaterialSetBumpMapTexture(material, texture) == nullptr) {
        return nullptr;
    }

    rpMatFXMaterialDataGta* data = *(rpMatFXMaterialDataGta**)((RwUInt8*)material + MatFXMaterialDataOffset);

    /* frame (inlined SetBumpMapFrame) */
    MatFXEffectDataGta* effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTBUMPMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    effect->data.bumpMap.frame = frame; /* not-found: NULL store, same as binary (dead path) */

    if (material == nullptr) {          /* dead check retained verbatim from decompile */
        return nullptr;
    }

    /* coefficient (inlined SetBumpMapCoefficient; stored NEGATED) */
    effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTBUMPMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    effect->data.bumpMap.coef = -coef; /* not-found: NULL store, same as binary (dead path) */
    return material;
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 */
    RwTexture *texture;        /* +0x08 */
    RwReal     coef;           /* +0x0C */
    RwReal     invBumpWidth;   /* +0x10 */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;
    MatFXDualData       dual;
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS];
    RpMatFXMaterialFlags  flags;
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RpMaterial* RpMatFXMaterialSetupEnvMap(RpMaterial* material, RwTexture* texture, RwFrame* frame, RwBool useFrameBufferAlpha, RwReal coef) {
    rpMatFXMaterialDataGta* data = *(rpMatFXMaterialDataGta**)((RwUInt8*)material + MatFXMaterialDataOffset);

    /* texture swap (inlined SetEnvMapTexture) */
    MatFXEffectDataGta* effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTENVMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    texture->refCount++; /* binary: unconditional (crashes on NULL texture, dead path) */
    if (effect->data.envMap.texture != nullptr) {
        RwTextureDestroy(effect->data.envMap.texture);
        effect->data.envMap.texture = nullptr;
    }
    effect->data.envMap.texture = texture;

    if (material == nullptr) { /* dead check retained verbatim from decompile */
        return nullptr;
    }

    /* frame (inlined SetEnvMapFrame) */
    effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTENVMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    effect->data.envMap.frame = frame;

    /* useFrameBufferAlpha (inlined SetEnvMapFrameBufferAlpha) */
    effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTENVMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    effect->data.envMap.useFrameBufferAlpha = useFrameBufferAlpha;

    /* coefficient (inlined SetEnvMapCoefficient) */
    effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTENVMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    effect->data.envMap.coef = coef;
    return material;
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 */
    RwTexture *texture;        /* +0x08 */
    RwReal     coef;           /* +0x0C */
    RwReal     invBumpWidth;   /* +0x10 */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;
    MatFXDualData       dual;
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS];
    RpMatFXMaterialFlags  flags;
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RpMaterial* RpMatFXMaterialSetupDualTexture(RpMaterial* material, RwTexture* texture, RwBlendFunction srcBlendMode, RwBlendFunction dstBlendMode) {
    rpMatFXMaterialDataGta* data = *(rpMatFXMaterialDataGta**)((RwUInt8*)material + MatFXMaterialDataOffset);

    /* texture swap (inlined SetDualTexture) */
    MatFXEffectDataGta* effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTDUAL) {
            effect = &data->data[pass];
            break;
        }
    }
    texture->refCount++; /* binary: unconditional (crashes on NULL texture, dead path) */
    if (effect->data.dual.texture != nullptr) {
        RwTextureDestroy(effect->data.dual.texture);
        effect->data.dual.texture = nullptr;
    }
    effect->data.dual.texture = texture;
    ((void(__cdecl*)(void*, RwInt32))0x816280)(effect, rwRENDERSTATETEXTUREADDRESS); /* 2 */
    ((void(__cdecl*)(void*, RwInt32))0x816280)(effect, rwRENDERSTATETEXTURERASTER);  /* 1 */

    if (material == nullptr) { /* dead check retained verbatim from decompile */
        return nullptr;
    }

    /* blend modes (inlined SetDualBlendModes) */
    effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTDUAL) {
            effect = &data->data[pass];
            break;
        }
    }
    effect->data.dual.srcBlendMode = srcBlendMode;
    effect->data.dual.dstBlendMode = dstBlendMode;
    ((void(__cdecl*)(void*, RwInt32))0x816280)(effect, rwRENDERSTATESRCBLEND);   /* 10 */
    ((void(__cdecl*)(void*, RwInt32))0x816280)(effect, rwRENDERSTATEDESTBLEND);  /* 11 */
    return material;
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 */
    RwTexture *texture;        /* +0x08 */
    RwReal     coef;           /* +0x0C */
    RwReal     invBumpWidth;   /* +0x10 */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;
    MatFXDualData       dual;
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS];
    RpMatFXMaterialFlags  flags;   /* +0x30 */
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RpMatFXMaterialFlags RpMatFXMaterialGetEffects(const RpMaterial* material) {
    const rpMatFXMaterialDataGta* data = *(const rpMatFXMaterialDataGta* const*)((const RwUInt8*)material + MatFXMaterialDataOffset);
    if (data == nullptr) {
        return rpMATFXEFFECTNULL;
    }
    return data->flags;
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 generated base+bump texture */
    RwTexture *texture;        /* +0x08 user bump map */
    RwReal     coef;           /* +0x0C stored NEGATED */
    RwReal     invBumpWidth;   /* +0x10 */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;
    MatFXDualData       dual;
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS];
    RpMatFXMaterialFlags  flags;
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RpMaterial* RpMatFXMaterialSetBumpMapTexture(RpMaterial* material, RwTexture* texture) {
    rpMatFXMaterialDataGta* data = *(rpMatFXMaterialDataGta**)((RwUInt8*)material + MatFXMaterialDataOffset);

    MatFXEffectDataGta* effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTBUMPMAP) {
            effect = &data->data[pass];
            break;
        }
    }

    MatFXBumpMapDataGta* bump = &effect->data.bumpMap; /* not-found: NULL deref, same as binary (dead path) */

    if (bump->texture != nullptr) {
        RwTextureDestroy(bump->texture);
        bump->texture = nullptr;
    }
    if (bump->bumpedTexture != nullptr) {
        RwTextureDestroy(bump->bumpedTexture);
        bump->bumpedTexture = nullptr;
        bump->invBumpWidth  = 0.0f;
    }

    RwTexture* base = material->texture;
    if (texture == nullptr) {
        /* no new bump map: invBumpWidth recomputed from the base texture only */
        bump->invBumpWidth = 1.0f / (RwReal)base->raster->width; /* binary: base deref unconditional */
        return material;
    }

    bump->texture = texture;
    texture->refCount++;

    if (texture->raster->width == 0) {                       /* binary: raster deref unconditional */
        return material;
    }
    if (base != nullptr && base->raster->width == 0) {
        return material;
    }

    /* interleaved name: alternate chars from base and bump texture names, max 30 */
    RwChar name[rwTEXTUREBASENAMELENGTH];
    memset(name, 0, sizeof(name)); /* binary zeroes 31 bytes; terminator written explicitly below */
    const RwChar* src[2];
    src[1] = texture->name;
    src[0] = (base != nullptr) ? base->name : texture->name;
    RwInt32  count = 0;
    RwChar*  dst   = name;
    do {
        if (src[0][0] == '\0' && src[1][0] == '\0') {
            break;
        }
        for (RwInt32 i = 0; i < 2; i++) {
            if (*src[i] != '\0') {
                *dst++ = *src[i]++;
                count++;
            }
        }
    } while (count < 30);
    *dst = '\0';

    RwTexDictionary* dict = RwTexDictionaryGetCurrent();
    bump->bumpedTexture = nullptr;
    if (dict != nullptr) {
        bump->bumpedTexture = RwTexDictionaryFindNamedTexture(dict, name);
    }
    if (bump->bumpedTexture == nullptr) {
        /* internal bumped-texture creator (copies base raster, applies bump) */
        bump->bumpedTexture = ((RwTexture*(__cdecl*)(RwTexture*, RwTexture*))0x816290)(base, texture);
        if (bump->bumpedTexture == nullptr) {
            return nullptr;
        }
        if (dict != nullptr) {
            RwTexDictionaryAddTexture(dict, bump->bumpedTexture);
        }
    } else {
        bump->bumpedTexture->refCount++;
    }

    bump->invBumpWidth = 1.0f / (RwReal)bump->bumpedTexture->raster->width;
    return material;
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 */
    RwTexture *texture;        /* +0x08 */
    RwReal     coef;           /* +0x0C */
    RwReal     invBumpWidth;   /* +0x10 */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;
    MatFXDualData       dual;
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS];
    RpMatFXMaterialFlags  flags;
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RpMaterial* RpMatFXMaterialSetBumpMapFrame(RpMaterial* material, RwFrame* frame) {
    rpMatFXMaterialDataGta* data = *(rpMatFXMaterialDataGta**)((RwUInt8*)material + MatFXMaterialDataOffset);

    MatFXEffectDataGta* effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTBUMPMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    effect->data.bumpMap.frame = frame; /* not-found: NULL store, same as binary (dead path) */
    return material;                    /* binary is void; header ABI requires RpMaterial* */
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 */
    RwTexture *texture;        /* +0x08 */
    RwReal     coef;           /* +0x0C stored NEGATED */
    RwReal     invBumpWidth;   /* +0x10 */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;
    MatFXDualData       dual;
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS];
    RpMatFXMaterialFlags  flags;
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RpMaterial* RpMatFXMaterialSetBumpMapCoefficient(RpMaterial* material, RwReal coef) {
    rpMatFXMaterialDataGta* data = *(rpMatFXMaterialDataGta**)((RwUInt8*)material + MatFXMaterialDataOffset);

    MatFXEffectDataGta* effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTBUMPMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    effect->data.bumpMap.coef = -coef; /* negated, FLD/FCHS/FSTP verified in binary; NULL store on not-found (dead path) */
    return material;                    /* binary is void; header ABI requires RpMaterial* */
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 generated base+bump texture */
    RwTexture *texture;        /* +0x08 user bump map */
    RwReal     coef;           /* +0x0C */
    RwReal     invBumpWidth;   /* +0x10 */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;
    MatFXDualData       dual;
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS];
    RpMatFXMaterialFlags  flags;
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RwTexture* RpMatFXMaterialGetBumpMapTexture(const RpMaterial* material) {
    const rpMatFXMaterialDataGta* data = *(const rpMatFXMaterialDataGta* const*)((const RwUInt8*)material + MatFXMaterialDataOffset);

    const MatFXEffectDataGta* effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTBUMPMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    return effect->data.bumpMap.texture; /* not-found: NULL read, same as binary (dead path) */
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 generated base+bump texture */
    RwTexture *texture;        /* +0x08 user bump map */
    RwReal     coef;           /* +0x0C */
    RwReal     invBumpWidth;   /* +0x10 */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;
    MatFXDualData       dual;
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS];
    RpMatFXMaterialFlags  flags;
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RwTexture* RpMatFXMaterialGetBumpMapBumpedTexture(const RpMaterial* material) {
    const rpMatFXMaterialDataGta* data = *(const rpMatFXMaterialDataGta* const*)((const RwUInt8*)material + MatFXMaterialDataOffset);

    const MatFXEffectDataGta* effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTBUMPMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    return effect->data.bumpMap.bumpedTexture; /* not-found: NULL read, same as binary (dead path) */
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 */
    RwTexture *texture;        /* +0x08 */
    RwReal     coef;           /* +0x0C */
    RwReal     invBumpWidth;   /* +0x10 */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;
    MatFXDualData       dual;
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS];
    RpMatFXMaterialFlags  flags;
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RwFrame* RpMatFXMaterialGetBumpMapFrame(const RpMaterial* material) {
    const rpMatFXMaterialDataGta* data = *(const rpMatFXMaterialDataGta* const*)((const RwUInt8*)material + MatFXMaterialDataOffset);

    const MatFXEffectDataGta* effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTBUMPMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    return effect->data.bumpMap.frame; /* not-found: NULL read, same as binary (dead path) */
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 */
    RwTexture *texture;        /* +0x08 */
    RwReal     coef;           /* +0x0C stored NEGATED */
    RwReal     invBumpWidth;   /* +0x10 */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;
    MatFXDualData       dual;
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS];
    RpMatFXMaterialFlags  flags;
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RwReal RpMatFXMaterialGetBumpMapCoefficient(const RpMaterial* material) {
    const rpMatFXMaterialDataGta* data = *(const rpMatFXMaterialDataGta* const*)((const RwUInt8*)material + MatFXMaterialDataOffset);

    const MatFXEffectDataGta* effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTBUMPMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    return -effect->data.bumpMap.coef; /* FLD coef + FCHS verified in binary; NULL read on not-found (dead path) */
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 */
    RwTexture *texture;        /* +0x08 */
    RwReal     coef;           /* +0x0C */
    RwReal     invBumpWidth;   /* +0x10 */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;     /* frame@+0 texture@+4 coef@+8 useFrameBufferAlpha@+0xC */
    MatFXDualData       dual;
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS];
    RpMatFXMaterialFlags  flags;
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RpMaterial* RpMatFXMaterialSetEnvMapTexture(RpMaterial* material, RwTexture* texture) {
    rpMatFXMaterialDataGta* data = *(rpMatFXMaterialDataGta**)((RwUInt8*)material + MatFXMaterialDataOffset);

    MatFXEffectDataGta* effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTENVMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    texture->refCount++; /* binary: unconditional (NULL texture = crash, dead path) */
    if (effect->data.envMap.texture != nullptr) { /* not-found: NULL deref, same as binary (dead path) */
        RwTextureDestroy(effect->data.envMap.texture);
        effect->data.envMap.texture = nullptr;
    }
    effect->data.envMap.texture = texture;
    return material;
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 */
    RwTexture *texture;        /* +0x08 */
    RwReal     coef;           /* +0x0C */
    RwReal     invBumpWidth;   /* +0x10 */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;
    MatFXDualData       dual;
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS];
    RpMatFXMaterialFlags  flags;
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RpMaterial* RpMatFXMaterialSetEnvMapFrame(RpMaterial* material, RwFrame* frame) {
    rpMatFXMaterialDataGta* data = *(rpMatFXMaterialDataGta**)((RwUInt8*)material + MatFXMaterialDataOffset);

    MatFXEffectDataGta* effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTENVMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    effect->data.envMap.frame = frame; /* not-found: NULL store, same as binary (dead path) */
    return material;
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 */
    RwTexture *texture;        /* +0x08 */
    RwReal     coef;           /* +0x0C */
    RwReal     invBumpWidth;   /* +0x10 */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;     /* useFrameBufferAlpha @ +0x0C */
    MatFXDualData       dual;
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS];
    RpMatFXMaterialFlags  flags;
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RpMaterial* RpMatFXMaterialSetEnvMapFrameBufferAlpha(RpMaterial* material, RwBool useFrameBufferAlpha) {
    rpMatFXMaterialDataGta* data = *(rpMatFXMaterialDataGta**)((RwUInt8*)material + MatFXMaterialDataOffset);

    MatFXEffectDataGta* effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTENVMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    effect->data.envMap.useFrameBufferAlpha = useFrameBufferAlpha; /* not-found: NULL store, same as binary (dead path) */
    return material;
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 */
    RwTexture *texture;        /* +0x08 */
    RwReal     coef;           /* +0x0C */
    RwReal     invBumpWidth;   /* +0x10 */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;     /* coef @ +0x08 */
    MatFXDualData       dual;
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS];
    RpMatFXMaterialFlags  flags;
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RpMaterial* RpMatFXMaterialSetEnvMapCoefficient(RpMaterial* material, RwReal coef) {
    rpMatFXMaterialDataGta* data = *(rpMatFXMaterialDataGta**)((RwUInt8*)material + MatFXMaterialDataOffset);

    MatFXEffectDataGta* effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTENVMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    effect->data.envMap.coef = coef; /* stored as-is (no negation); not-found: NULL store, same as binary (dead path) */
    return material;
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 */
    RwTexture *texture;        /* +0x08 */
    RwReal     coef;           /* +0x0C */
    RwReal     invBumpWidth;   /* +0x10 */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;     /* texture @ +0x04 */
    MatFXDualData       dual;
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS];
    RpMatFXMaterialFlags  flags;
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RwTexture* RpMatFXMaterialGetEnvMapTexture(const RpMaterial* material) {
    const rpMatFXMaterialDataGta* data = *(const rpMatFXMaterialDataGta* const*)((const RwUInt8*)material + MatFXMaterialDataOffset);

    const MatFXEffectDataGta* effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTENVMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    return effect->data.envMap.texture; /* not-found: NULL read, same as binary (dead path) */
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 */
    RwTexture *texture;        /* +0x08 */
    RwReal     coef;           /* +0x0C */
    RwReal     invBumpWidth;   /* +0x10 */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;     /* frame @ +0x00 */
    MatFXDualData       dual;
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS];
    RpMatFXMaterialFlags  flags;
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RwFrame* RpMatFXMaterialGetEnvMapFrame(const RpMaterial* material) {
    const rpMatFXMaterialDataGta* data = *(const rpMatFXMaterialDataGta* const*)((const RwUInt8*)material + MatFXMaterialDataOffset);

    const MatFXEffectDataGta* effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTENVMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    return effect->data.envMap.frame; /* not-found: NULL read, same as binary (dead path) */
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 */
    RwTexture *texture;        /* +0x08 */
    RwReal     coef;           /* +0x0C */
    RwReal     invBumpWidth;   /* +0x10 */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;     /* useFrameBufferAlpha @ +0x0C */
    MatFXDualData       dual;
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS];
    RpMatFXMaterialFlags  flags;
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RwBool RpMatFXMaterialGetEnvMapFrameBufferAlpha(const RpMaterial* material) {
    const rpMatFXMaterialDataGta* data = *(const rpMatFXMaterialDataGta* const*)((const RwUInt8*)material + MatFXMaterialDataOffset);

    const MatFXEffectDataGta* effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTENVMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    return effect->data.envMap.useFrameBufferAlpha; /* not-found: NULL read, same as binary (dead path) */
}


#ifndef RPMATFX_GTA_INTERNAL_DATA_LAYOUT
#define RPMATFX_GTA_INTERNAL_DATA_LAYOUT
struct MatFXBumpMapDataGta {
    RwFrame   *frame;          /* +0x00 */
    RwTexture *bumpedTexture;  /* +0x04 */
    RwTexture *texture;        /* +0x08 */
    RwReal     coef;           /* +0x0C */
    RwReal     invBumpWidth;   /* +0x10 */
};
union MatFXEffectUnionGta {        /* 0x14 bytes */
    MatFXBumpMapDataGta bumpMap;
    MatFXEnvMapData     envMap;     /* coef @ +0x08 */
    MatFXDualData       dual;
    MatFXUVAnimData     uvAnim;
};
struct MatFXEffectDataGta {        /* 0x18 bytes per pass */
    MatFXEffectUnionGta   data;     /* +0x00 */
    RpMatFXMaterialFlags  flag;    /* +0x14 */
};
struct rpMatFXMaterialDataGta {    /* 0x34 bytes total */
    MatFXEffectDataGta    data[rpMAXPASS];
    RpMatFXMaterialFlags  flags;
};
#endif /* RPMATFX_GTA_INTERNAL_DATA_LAYOUT */

RwReal RpMatFXMaterialGetEnvMapCoefficient(const RpMaterial* material) {
    const rpMatFXMaterialDataGta* data = *(const rpMatFXMaterialDataGta* const*)((const RwUInt8*)material + MatFXMaterialDataOffset);

    const MatFXEffectDataGta* effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (data->data[pass].flag == rpMATFXEFFECTENVMAP) {
            effect = &data->data[pass];
            break;
        }
    }
    return effect->data.envMap.coef; /* as stored, NO negation (FLD without FCHS verified in binary); NULL read on not-found (dead path) */
}


RpMaterial* RpMatFXMaterialSetDualTexture(RpMaterial* material, RwTexture* texture) {
    rpMatFXMaterialData *matfxData = *(rpMatFXMaterialData **)((RwUInt8 *)material + MatFXMaterialDataOffset);
    MatFXEffectData *effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (matfxData->data[pass].flag == rpMATFXEFFECTDUAL) {
            effect = &matfxData->data[pass];
            break;
        }
    }
    RwTextureAddRef(texture);
    if (effect->data.dual.texture != nullptr) {
        RwTextureDestroy(effect->data.dual.texture);
        effect->data.dual.texture = nullptr;
    }
    effect->data.dual.texture = texture;
    // внутренний хелпер matfx D3D9-драйвера вне этажа 849 (нет в rw_addr_map.json и в декомпилах):
    // семантика неизвестна, наблюдаемые пары аргументов: (2,1) после установки текстуры, (0xA,0xB) после blend-модов
    ((void(__cdecl *)(MatFXEffectData *, RwInt32))0x816280)(effect, 2);
    ((void(__cdecl *)(MatFXEffectData *, RwInt32))0x816280)(effect, 1);
    return material;
}


RpMaterial* RpMatFXMaterialSetDualBlendModes(RpMaterial* material, RwBlendFunction srcBlendMode, RwBlendFunction dstBlendMode) {
    rpMatFXMaterialData *matfxData = *(rpMatFXMaterialData **)((RwUInt8 *)material + MatFXMaterialDataOffset);
    MatFXEffectData *effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (matfxData->data[pass].flag == rpMATFXEFFECTDUAL) {
            effect = &matfxData->data[pass];
            break;
        }
    }
    effect->data.dual.srcBlendMode = srcBlendMode;
    effect->data.dual.dstBlendMode = dstBlendMode;
    ((void(__cdecl *)(MatFXEffectData *, RwInt32))0x816280)(effect, 0xA);
    ((void(__cdecl *)(MatFXEffectData *, RwInt32))0x816280)(effect, 0xB);
    return material;
}


RwTexture* RpMatFXMaterialGetDualTexture(const RpMaterial* material) {
    rpMatFXMaterialData *matfxData = *(rpMatFXMaterialData **)((RwUInt8 *)material + MatFXMaterialDataOffset);
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (matfxData->data[pass].flag == rpMATFXEFFECTDUAL) {
            return matfxData->data[pass].data.dual.texture;
        }
    }
    return *(RwTexture **)nullptr; // как в бинарнике: чтение [0]; не заменять на return nullptr
}


const RpMaterial* RpMatFXMaterialGetDualBlendModes(const RpMaterial* material, RwBlendFunction* srcBlendMode, RwBlendFunction* dstBlendMode) {
    rpMatFXMaterialData *matfxData = *(rpMatFXMaterialData **)((RwUInt8 *)material + MatFXMaterialDataOffset);
    MatFXEffectData *effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (matfxData->data[pass].flag == rpMATFXEFFECTDUAL) {
            effect = &matfxData->data[pass];
            break;
        }
    }
    *srcBlendMode = effect->data.dual.srcBlendMode;
    *dstBlendMode = effect->data.dual.dstBlendMode;
    return material;
}


RpMaterial* RpMatFXMaterialSetUVTransformMatrices(RpMaterial* material, RwMatrix* baseTransform, RwMatrix* dualTransform) {
    rpMatFXMaterialData *matfxData = *(rpMatFXMaterialData **)((RwUInt8 *)material + MatFXMaterialDataOffset);
    MatFXEffectData *effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (matfxData->data[pass].flag == rpMATFXEFFECTUVTRANSFORM) {
            effect = &matfxData->data[pass];
            break;
        }
    }
    effect->data.uvAnim.baseTransform = baseTransform;
    effect->data.uvAnim.dualTransform = dualTransform;
    return material;
}


const RpMaterial* RpMatFXMaterialGetUVTransformMatrices(const RpMaterial* material, RwMatrix* baseTransform, RwMatrix* dualTransform) {
    rpMatFXMaterialData *matfxData = *(rpMatFXMaterialData **)((RwUInt8 *)material + MatFXMaterialDataOffset);
    MatFXEffectData *effect = nullptr;
    for (RwInt32 pass = 0; pass < rpMAXPASS; pass++) {
        if (matfxData->data[pass].flag == rpMATFXEFFECTUVTRANSFORM) {
            effect = &matfxData->data[pass];
            break;
        }
    }
    if (baseTransform != nullptr) {
        *baseTransform = *effect->data.uvAnim.baseTransform;
    }
    if (dualTransform != nullptr) {
        *dualTransform = *effect->data.uvAnim.dualTransform;
    }
    return material;
}


static inline auto &MatFXD3D9AtomicPipeline      = StaticRef<RxPipeline *>(0xC9AB8C);
static inline auto &MatFXD3D9WorldSectorPipeline = StaticRef<RxPipeline *>(0xC9AB90);

RxPipeline* RpMatFXGetD3D9Pipeline(RpMatFXD3D9Pipeline d3d9Pipeline) {
    if (d3d9Pipeline == rpMATFXD3D9ATOMICPIPELINE) {
        return MatFXD3D9AtomicPipeline;
    }
    if (d3d9Pipeline != rpMATFXD3D9WORLDSECTORPIPELINE) {
        return nullptr;
    }
    return MatFXD3D9WorldSectorPipeline;
}

