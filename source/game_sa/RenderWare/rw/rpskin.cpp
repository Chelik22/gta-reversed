#include "StdInc.h"

#include "rpskin.h"

// ==== RW-CAMPAIGN wave-A shared decls (consolidated from agent fragments; layout decompile-verified) ====
static auto& gRpSkinEngineOffset   = StaticRef<RwInt32>(0xC978A0);
static auto& gRpSkinAtomicOffset   = StaticRef<RwInt32>(0xC978A4);
static auto& gRpSkinGeometryOffset = StaticRef<RwInt32>(0xC978A8);
static auto& gRpSkinFreeList       = StaticRef<RwFreeList*>(0xC978B8);
struct RpSkinGta {
    RwUInt32         numBones;              // +0x00 (RpSkinGetNumBones)
    RwUInt32         numUsedBones;          // +0x04 (byte count of the usedBones table)
    RwUInt8*         usedBones;             // +0x08 (remapped used-bone indices; == data block base)
    RwMatrix*        skinToBoneMatrices;    // +0x0C
    RwUInt32         numWeights;            // +0x10 (1..4, max weights used per vertex)
    RwUInt32*        vertexIndices;         // +0x14 (RpSkinGetVertexBoneIndices; 4 packed bytes per vertex)
    RwMatrixWeights* vertexWeights;         // +0x18 (RpSkinGetVertexBoneWeights)
    RwUInt32         pad1C;                 // +0x1C
    RwUInt32         pad20;                 // +0x20
    RwUInt32         boneLimit;             // +0x24 (split data)
    RwUInt32         numMeshes;             // +0x28 (0 == not split; RpSkinIsSplit)
    RwUInt32         numRLE;                // +0x2C
    RwUInt8*         meshBoneRemapIndices;  // +0x30 (_rpSkinGetMeshBoneRemapIndices)
    RwUInt8*         meshBoneRLECount;      // +0x34 (entries are RwUInt16)
    RwUInt8*         meshBoneRLE;           // +0x38 (entries are RwUInt16)
    RwUInt8*         data;                  // +0x3C (single malloc'd block; freed by RpSkinDestroy)
};


static auto& gRpSkinFreeListBlockSize = StaticRef<RwInt32>(0x8DECE0);
static auto& gRpSkinFreeListNumBlocksToPrealloc = StaticRef<RwInt32>(0x8DECE4);

void RpSkinSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    gRpSkinFreeListBlockSize = blockSize;
    gRpSkinFreeListNumBlocksToPrealloc = numBlocksToPrealloc;
}



RwBool RpSkinPluginAttach() {
    gRpSkinEngineOffset = RwEngineRegisterPlugin(0, rwID_SKINPLUGIN,
                                                 (RwPluginObjectConstructor)0x7C68E0,
                                                 (RwPluginObjectDestructor)0x7C69A0);
    gRpSkinAtomicOffset = RpAtomicRegisterPlugin(sizeof(RpHAnimHierarchy*), rwID_SKINPLUGIN,
                                                 (RwPluginObjectConstructor)0x7C6A80,
                                                 (RwPluginObjectDestructor)0x7C6AA0,
                                                 (RwPluginObjectCopy)0x7C6AC0);
    RpAtomicRegisterPluginStream(rwID_SKINPLUGIN,
                                 (RwPluginDataChunkReadCallBack)0x7C72A0,
                                 (RwPluginDataChunkWriteCallBack)0x7C7500,
                                 (RwPluginDataChunkGetSizeCallBack)0x7C7510);
    RpAtomicSetStreamAlwaysCallBack(rwID_SKINPLUGIN, (RwPluginDataChunkAlwaysCallBack)0x7C6AE0);
    RpAtomicSetStreamRightsCallBack(rwID_SKINPLUGIN, (RwPluginDataChunkRightsCallBack)0x7C6B70);
    gRpSkinGeometryOffset = RpGeometryRegisterPlugin(sizeof(RpSkin*), rwID_SKINPLUGIN,
                                                     (RwPluginObjectConstructor)0x7C69F0,
                                                     (RwPluginObjectDestructor)0x7C6A10,
                                                     (RwPluginObjectCopy)0x7C6A70);
    RpGeometryRegisterPluginStream(rwID_SKINPLUGIN,
                                   (RwPluginDataChunkReadCallBack)0x7C6D20,
                                   (RwPluginDataChunkWriteCallBack)0x7C6C30,
                                   (RwPluginDataChunkGetSizeCallBack)0x7C6BE0);
    return TRUE;
}



RpAtomic* RpSkinAtomicSetHAnimHierarchy(RpAtomic* atomic, RpHAnimHierarchy* hierarchy) {
    *(RpHAnimHierarchy**)((RwUInt8*)atomic + gRpSkinAtomicOffset) = hierarchy;
    return atomic;
}



RpHAnimHierarchy* RpSkinAtomicGetHAnimHierarchy(const RpAtomic* atomic) {
    return *(RpHAnimHierarchy**)((RwUInt8*)atomic + gRpSkinAtomicOffset);
}



RpGeometry* RpSkinGeometrySetSkin(RpGeometry* geometry, RpSkin* skin) {
    if (skin != *(RpSkin**)((RwUInt8*)geometry + gRpSkinGeometryOffset)) {
        if (*(RpSkin**)((RwUInt8*)geometry + gRpSkinGeometryOffset) != nullptr) {
            _rpSkinDeinitialize(geometry);
        }
        *(RpSkin**)((RwUInt8*)geometry + gRpSkinGeometryOffset) = skin;
        if (skin != nullptr) {
            if (_rpSkinInitialize(geometry) == nullptr) {
                return nullptr;
            }
        }
    }
    return geometry;
}



RpSkin* RpSkinGeometryGetSkin(RpGeometry* geometry) {
    return *(RpSkin**)((RwUInt8*)geometry + gRpSkinGeometryOffset);
}



RpSkin* RpSkinCreate(RwUInt32 numVertices, RwUInt32 numBones, RwMatrixWeights* vertexWeights, RwUInt32* vertexIndices, RwMatrix* inverseMatrices) {
    RpSkinGta* skin = (RpSkinGta*)RwFreeListAlloc(gRpSkinFreeList, rwID_SKINPLUGIN | rwMEMHINTDUR_EVENT);
    memset(skin, 0, sizeof(RpSkinGta)); // decompile zeroes 16 dwords (0x40)
    skin->numWeights = 1;

    // Find the max number of weights used per vertex (librw analog: Skin::findNumWeights)
    RwUInt32 remapStack[64]; // 256-byte stack buffer from the decompile
    RwUInt32 numUsedBones = 0;
    if (numVertices != 0) {
        RwUInt32 v = 0;
        do {
            RwUInt32 w = skin->numWeights;
            if (w < 4) {
                RwReal* pw = (RwReal*)(vertexWeights + v) + w;
                do {
                    if (*pw == 0.0f)
                        break;
                    skin->numWeights = skin->numWeights + 1;
                    if (skin->numWeights == 4)
                        goto done;
                    w++;
                    pw++;
                } while (w < 4);
            }
            v++;
        } while (v < numVertices);
    }
done:
    // Build the remapped used-bones table into remapStack (vendor helper 0x7C70C0)
    ((void(__cdecl*)(RpSkin*, RwUInt32*, RwMatrixWeights*, RwUInt32*, RwUInt32*, RwUInt32))0x7C70C0)
        ((RpSkin*)skin, vertexIndices, vertexWeights, remapStack, &numUsedBones, numVertices);

    RwUInt32 blockSize = numBones * sizeof(RwMatrix) + numVertices * 0x14 + 0xF + numUsedBones;
    RwUInt8* block = (RwUInt8*)RwMalloc(blockSize, rwID_SKINPLUGIN | rwMEMHINTDUR_EVENT);
    skin->data = block;
    if (block == nullptr) {
        RwFreeListFree(gRpSkinFreeList, skin);
        return nullptr;
    }
    memset(block, 0, blockSize);
    skin->numBones           = numBones;
    skin->numUsedBones       = numUsedBones;
    skin->skinToBoneMatrices = (RwMatrix*)(((uintptr)block + numUsedBones + 0xF) & ~(uintptr)0xF);
    skin->vertexIndices      = (RwUInt32*)(skin->skinToBoneMatrices + numBones);
    skin->vertexWeights      = (RwMatrixWeights*)(skin->vertexIndices + numVertices);
    skin->usedBones          = block;
    if (numUsedBones != 0) {
        memcpy(block, remapStack, numUsedBones);
    }
    if (inverseMatrices != nullptr && skin->numBones != 0) {
        // decompile copies the matrices backwards; forward copy is equivalent (non-overlapping)
        for (RwUInt32 i = 0; i < numBones; i++) {
            skin->skinToBoneMatrices[i] = inverseMatrices[i];
        }
    }
    if (vertexIndices != nullptr) {
        memcpy(skin->vertexIndices, vertexIndices, numVertices * sizeof(RwUInt32));
    }
    if (vertexWeights != nullptr) {
        memcpy(skin->vertexWeights, vertexWeights, numVertices * sizeof(RwMatrixWeights));
    }
    return (RpSkin*)skin;
}



RpSkin* RpSkinDestroy(RpSkin* skin) {
    if (((RpSkinGta*)skin)->data != nullptr) {
        RwFree(((RpSkinGta*)skin)->data);
    }
    _rpSkinSplitDataDestroy(skin);
    RwFreeListFree(gRpSkinFreeList, skin);
    return nullptr; // decompile returns 0
}



RwUInt32 RpSkinGetNumBones(RpSkin* skin) {
    return ((RpSkinGta*)skin)->numBones;
}



/*const*/ RwMatrixWeights* RpSkinGetVertexBoneWeights(RpSkin* skin) {
    return ((RpSkinGta*)skin)->vertexWeights;
}



const RwUInt32* RpSkinGetVertexBoneIndices(RpSkin* skin) {
    return ((RpSkinGta*)skin)->vertexIndices;
}



const RwMatrix* RpSkinGetSkinToBoneMatrices(RpSkin* skin) {
    return ((RpSkinGta*)skin)->skinToBoneMatrices;
}



RwBool RpSkinIsSplit(RpSkin* skin) {
    return ((RpSkinGta*)skin)->numMeshes != 0;
}


RpAtomic* RpSkinAtomicSetType(RpAtomic* atomic, RpSkinType type) {
    if ((RwEngineGetPluginOffset(rwID_MATERIALEFFECTSPLUGIN) == -1 && type == rpSKINTYPEMATFX)
        || (RwEngineGetPluginOffset(rwID_TOONPLUGIN) == -1 && type == rpSKINTYPETOON)) {
        type = rpSKINTYPEGENERIC;
    }
    ((void(__cdecl*)(RpAtomic*, RpSkinType))0x7C89B0)(atomic, type);
    return atomic;
}


RpSkinType RpSkinAtomicGetType(RpAtomic* atomic) {
    RxPipeline* pipeline = atomic->pipeline;
    if (pipeline->pluginId == rwID_SKINPLUGIN) {
        return (RpSkinType)pipeline->pluginData;
    }
    return rpNASKINTYPE;
}



RpGeometry* _rpSkinInitialize(RpGeometry* geometry) {
    RpSkinGta* skin = *(RpSkinGta**)((RwUInt8*)geometry + gRpSkinGeometryOffset);
    if (skin != nullptr) {
        RwMatrixWeights* weights = skin->vertexWeights;
        RwInt32 v = 0;
        if (0 < geometry->numVertices) {
            RwUInt8* indexBytes = (RwUInt8*)skin->vertexIndices + 2;
            do {
                if (weights->w0 < 1.0f) {
                    RwBool sorted;
                    do {
                        sorted = TRUE;
                        if (weights->w0 < weights->w1) {
                            RwReal  tw = weights->w0;  weights->w0 = weights->w1;  weights->w1 = tw;
                            RwUInt8 tb = indexBytes[-2]; indexBytes[-2] = indexBytes[-1]; indexBytes[-1] = tb;
                            sorted = FALSE;
                        }
                        if (weights->w1 < weights->w2) {
                            RwReal  tw = weights->w1;  weights->w1 = weights->w2;  weights->w2 = tw;
                            RwUInt8 tb = indexBytes[-1]; indexBytes[-1] = indexBytes[0]; indexBytes[0] = tb;
                            sorted = FALSE;
                        }
                        if (weights->w2 < weights->w3) {
                            RwReal  tw = weights->w2;  weights->w2 = weights->w3;  weights->w3 = tw;
                            RwUInt8 tb = indexBytes[0]; indexBytes[0] = indexBytes[1]; indexBytes[1] = tb;
                            sorted = FALSE;
                        }
                    } while (!sorted);
                }
                weights++;
                indexBytes += 4;
                v++;
            } while (v < geometry->numVertices);
        }
        if (skin->usedBones != nullptr) {
            ((void(__cdecl*)(void*, RwUInt32, RwUInt32, int(__cdecl*)(const void*, const void*)))0x8247E0)
                (skin->usedBones, skin->numUsedBones, 1,
                 (int(__cdecl*)(const void*, const void*))0x7C8720);
        }
    }
    return geometry;
}


RpGeometry* _rpSkinDeinitialize(RpGeometry* geometry) {
    return geometry;
}



RwUInt8* _rpSkinGetMeshBoneRemapIndices(RpSkin* skin) {
    return ((RpSkinGta*)skin)->meshBoneRemapIndices;
}



RwUInt8* _rpSkinGetMeshBoneRLECount(RpSkin* skin) {
    return ((RpSkinGta*)skin)->meshBoneRLECount;
}



RwUInt8* _rpSkinGetMeshBoneRLE(RpSkin* skin) {
    return ((RpSkinGta*)skin)->meshBoneRLE;
}



RpSkin* _rpSkinSplitDataCreate(RpSkin* skin, RwUInt32 boneLimit, RwUInt32 numMatrices, RwUInt32 numMeshes, RwUInt32 numRLE) {
    RpSkinGta* s = (RpSkinGta*)skin;
    if (s->meshBoneRemapIndices != nullptr) {
        RwFree(s->meshBoneRemapIndices);
    }
    s->boneLimit           = 0;
    s->numMeshes           = 0;
    s->numRLE              = 0;
    s->meshBoneRemapIndices = nullptr;
    s->meshBoneRLECount    = nullptr;
    s->meshBoneRLE         = nullptr;

    RwUInt32 blockSize = numMatrices + (numMeshes + numRLE) * 2;
    RwUInt8* block = (RwUInt8*)RwMalloc(blockSize, rwID_SKINPLUGIN | rwMEMHINTDUR_EVENT);
    s->meshBoneRemapIndices = block;
    if (block == nullptr) {
        return nullptr;
    }
    memset(block, 0, blockSize);
    s->boneLimit        = boneLimit;
    s->numRLE           = numRLE;
    s->meshBoneRLECount = block + numMatrices;
    s->numMeshes        = numMeshes;
    s->meshBoneRLE      = s->meshBoneRLECount + numMeshes * 2;
    return skin;
}


RwBool _rpSkinSplitDataDestroy(RpSkin* skin) {
    return ((RwBool(__cdecl *)(RpSkin*))0x7C8B10)(skin);
}

RxNodeDefinition* RxNodeDefinitionGetD3D9SkinAtomicAllInOne() {
    return &StaticRef<RxNodeDefinition>(0x8DED08);
}

