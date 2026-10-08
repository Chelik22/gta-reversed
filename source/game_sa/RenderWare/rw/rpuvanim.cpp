#include "StdInc.h"

#include "rpuvanim.h"

static auto& g_uvAnimCustomDataHeap = StaticRef<void*>(0xC978F4);

_rpUVAnimCustomData* _rpUVAnimCustomDataStreamRead(RwStream* stream) {
    // Original allocates sizeof(_rpUVAnimCustomData) (0x44 bytes) through the engine memory funcs:
    // *(fn**)(RwEngineInstance + 0x144)(cookie@0xC978F4, 0x30135 = rwID_UVANIMPLUGIN | 0x30000)
    _rpUVAnimCustomData* customData = (_rpUVAnimCustomData*)(*(void*(__cdecl**)(void*, RwUInt32))((RwUInt8*)RwEngineInstance + 0x144))(g_uvAnimCustomDataHeap, 0x30135);
    if (customData == nullptr) {
        return nullptr;
    }
    if (RwStreamRead(stream, customData->name, RP_UVANIM_MAXNAME /* 0x20 */) == 0) {
        return nullptr;
    }
    if (RwStreamReadInt32(stream, (RwInt32*)customData->nodeToUVChannelMap, RP_UVANIM_MAXSLOTS * sizeof(RwUInt32) /* 0x20 */) == nullptr) {
        return nullptr;
    }
    customData->refCount = 1;
    return customData;
}


const _rpUVAnimCustomData* _rpUVAnimCustomDataStreamWrite(const _rpUVAnimCustomData* customData, RwStream* stream) {
    if (RwStreamWrite(stream, customData->name, RP_UVANIM_MAXNAME /* 0x20 */) == nullptr) {
        return nullptr;
    }
    if (RwStreamWriteInt32(stream, (const RwInt32*)customData->nodeToUVChannelMap, RP_UVANIM_MAXSLOTS * sizeof(RwUInt32) /* 0x20 */) == nullptr) {
        return nullptr;
    }
    return customData;
}


RwUInt32 _rpUVAnimCustomDataStreamGetSize(const _rpUVAnimCustomData* customData) {
    // streamed payload: name (0x20) + nodeToUVChannelMap (0x20); refCount is not streamed.
    // original ignores customData.
    (void)customData;
    return 0x40;
}


static auto& g_uvAnimMatGlobals = StaticRef<RpUVAnimMaterialGlobalVars>(0xC9B8D0);

RwBool RpUVAnimPluginAttach() {
    if (RwEngineRegisterPlugin(0, rwID_UVANIMPLUGIN /* 0x135 */,
                               (RwPluginObjectConstructor)0x7CB9B0,
                               (RwPluginObjectDestructor)0x7CBA20) >= 0) {
        g_uvAnimMatGlobals.engineOffset = RpMaterialRegisterPlugin(sizeof(RpUVAnimMaterialExtension) /* 0x28 */,
                                                                   rwID_UVANIMPLUGIN,
                                                                   (RwPluginObjectConstructor)0x7CBA80,
                                                                   (RwPluginObjectDestructor)0x7CBAA0,
                                                                   (RwPluginObjectCopy)0x7CBB30);
        if (RpMaterialRegisterPluginStream(rwID_UVANIMPLUGIN,
                                           (RwPluginDataChunkReadCallBack)0x7CBC20,
                                           (RwPluginDataChunkWriteCallBack)0x7CBB70,
                                           (RwPluginDataChunkGetSizeCallBack)0x7CBF20) >= 0
            && g_uvAnimMatGlobals.engineOffset >= 0) {
            return TRUE;
        }
    }
    return FALSE;
}


void RpUVAnimLinearKeyFrameApply(void* matrix, void* voidIFrame) {
    RwMatrix* mtx = (RwMatrix*)matrix;
    const RpUVAnimLinearKeyFrameData* lkf = &((RpUVAnimInterpFrame*)voidIFrame)->data.linear;

    mtx->right.x = lkf->uv[0];
    mtx->right.y = lkf->uv[1];
    mtx->right.z = 0.0f;
    mtx->up.x    = lkf->uv[2];
    mtx->up.y    = lkf->uv[3];
    mtx->up.z    = 0.0f;
    mtx->at.x    = 0.0f;
    mtx->at.y    = 0.0f;
    mtx->at.z    = 0.0f;
    mtx->pos.x   = lkf->uv[4];
    mtx->pos.y   = lkf->uv[5];
    mtx->pos.z   = 0.0f;
    RwMatrixUpdate(mtx);
}


void RpUVAnimLinearKeyFrameBlend(void* voidOut, void* voidIn1, void* voidIn2, RwReal alpha) {
    const RpUVAnimKeyFrame* k1 = (const RpUVAnimKeyFrame*)voidIn1;
    const RpUVAnimKeyFrame* k2 = (const RpUVAnimKeyFrame*)voidIn2;
    RpUVAnimLinearKeyFrameData* out = &((RpUVAnimInterpFrame*)voidOut)->data.linear;

    out->uv[0] = (k2->data.linear.uv[0] - k1->data.linear.uv[0]) * alpha + k1->data.linear.uv[0];
    out->uv[1] = (k2->data.linear.uv[1] - k1->data.linear.uv[1]) * alpha + k1->data.linear.uv[1];
    out->uv[2] = (k2->data.linear.uv[2] - k1->data.linear.uv[2]) * alpha + k1->data.linear.uv[2];
    out->uv[3] = (k2->data.linear.uv[3] - k1->data.linear.uv[3]) * alpha + k1->data.linear.uv[3];
    out->uv[4] = (k2->data.linear.uv[4] - k1->data.linear.uv[4]) * alpha + k1->data.linear.uv[4];
    out->uv[5] = (k2->data.linear.uv[5] - k1->data.linear.uv[5]) * alpha + k1->data.linear.uv[5];
}


void RpUVAnimLinearKeyFrameInterpolate(void* voidOut, void* voidIn1, void* voidIn2, RwReal time, void* customData) {
    const RpUVAnimKeyFrame* k1 = (const RpUVAnimKeyFrame*)voidIn1;
    const RpUVAnimKeyFrame* k2 = (const RpUVAnimKeyFrame*)voidIn2;
    RpUVAnimLinearKeyFrameData* out = &((RpUVAnimInterpFrame*)voidOut)->data.linear;

    const RwReal t = (time - k1->time) / (k2->time - k1->time);

    out->uv[0] = (k2->data.linear.uv[0] - k1->data.linear.uv[0]) * t + k1->data.linear.uv[0];
    out->uv[1] = (k2->data.linear.uv[1] - k1->data.linear.uv[1]) * t + k1->data.linear.uv[1];
    out->uv[2] = (k2->data.linear.uv[2] - k1->data.linear.uv[2]) * t + k1->data.linear.uv[2];
    out->uv[3] = (k2->data.linear.uv[3] - k1->data.linear.uv[3]) * t + k1->data.linear.uv[3];
    out->uv[4] = (k2->data.linear.uv[4] - k1->data.linear.uv[4]) * t + k1->data.linear.uv[4];
    out->uv[5] = (k2->data.linear.uv[5] - k1->data.linear.uv[5]) * t + k1->data.linear.uv[5];
}


void RpUVAnimLinearKeyFrameAdd(void* voidOut, void* voidIn1, void* voidIn2)
{
    RpUVAnimLinearKeyFrameData* out = &((RpUVAnimInterpFrame*)voidOut)->data.linear;
    RpUVAnimLinearKeyFrameData* in1 = &((RpUVAnimInterpFrame*)voidIn1)->data.linear;
    RpUVAnimLinearKeyFrameData* in2 = &((RpUVAnimInterpFrame*)voidIn2)->data.linear;

    /* operand/add order exactly as in decompile */
    out->uv[0] = in2->uv[2] * in1->uv[1] + in2->uv[0] * in1->uv[0];
    out->uv[1] = in2->uv[3] * in1->uv[1] + in2->uv[1] * in1->uv[0];
    out->uv[2] = in2->uv[0] * in1->uv[2] + in2->uv[2] * in1->uv[3];
    out->uv[3] = in2->uv[1] * in1->uv[2] + in2->uv[3] * in1->uv[3];
    out->uv[4] = in2->uv[4] + in1->uv[4];
    out->uv[5] = in2->uv[5] + in1->uv[5];
}


void RpUVAnimLinearKeyFrameMulRecip(void* voidFrame, void* voidStart)
{
    RpUVAnimLinearKeyFrameData* frame = &((RpUVAnimInterpFrame*)voidFrame)->data.linear;
    RpUVAnimLinearKeyFrameData* start = &((RpUVAnimInterpFrame*)voidStart)->data.linear;

    RwReal det = start->uv[0] * start->uv[3] - start->uv[1] * start->uv[2];

    if (det != 0.0f)
    {
        RwReal recip = 1.0f / det;
        RwReal a = frame->uv[0];
        RwReal b = frame->uv[1];
        RwReal invA = recip * start->uv[3];   /* decompile fVar4 */
        RwReal invC = -(recip * start->uv[1]); /* decompile fVar5 */
        RwReal invB = -(recip * start->uv[2]); /* decompile fVar3 (reassigned) */

        frame->uv[0] = invA * a + invC * frame->uv[2];
        frame->uv[1] = invA * b + invC * frame->uv[3];
        frame->uv[2] = (frame->uv[2] + a) * invB;
        frame->uv[3] = (frame->uv[3] + b) * invB;
    }

    frame->uv[4] -= start->uv[4];
    frame->uv[5] -= start->uv[5];
}


static auto& g_uvAnimPivot    = StaticRef<const RwV3d>(0x882ED4);
static auto& g_uvAnimInvPivot = StaticRef<const RwV3d>(0x882EE0);

void RpUVAnimParamKeyFrameApply(void* matrix, void* voidIFrame) {
    RwMatrix* mtx = (RwMatrix*)matrix;
    const RpUVAnimParamKeyFrameData* param = &((RpUVAnimInterpFrame*)voidIFrame)->data.param;
    const RwV3d zAxis = { 0.0f, 0.0f, 1.0f };

    mtx->right.x = param->s0;
    mtx->right.y = param->skew;
    mtx->right.z = 0.0f;
    mtx->up.x    = 0.0f;
    mtx->up.y    = param->s1;
    mtx->up.z    = 0.0f;
    mtx->at.x    = 0.0f;
    mtx->at.y    = 0.0f;
    mtx->at.z    = 0.0f;
    mtx->pos.x   = param->x;
    mtx->pos.y   = param->y;
    mtx->pos.z   = 0.0f;
    RwMatrixUpdate(mtx);
    RwMatrixTranslate(mtx, &g_uvAnimPivot, rwCOMBINEPOSTCONCAT);
    RwMatrixRotate(mtx, &zAxis, param->theta * 57.2957764f /* 0x42652EE0 = 180/pi, read from binary 0x00859878 */, rwCOMBINEPOSTCONCAT);
    RwMatrixTranslate(mtx, &g_uvAnimInvPivot, rwCOMBINEPOSTCONCAT);
}


void RpUVAnimParamKeyFrameBlend(void* voidOut, void* voidIn1, void* voidIn2, RwReal alpha) {
    const RpUVAnimKeyFrame* k1 = (const RpUVAnimKeyFrame*)voidIn1;
    const RpUVAnimKeyFrame* k2 = (const RpUVAnimKeyFrame*)voidIn2;
    const RpUVAnimParamKeyFrameData* p1 = &k1->data.param;
    const RpUVAnimParamKeyFrameData* p2 = &k2->data.param;
    RpUVAnimParamKeyFrameData* out = &((RpUVAnimInterpFrame*)voidOut)->data.param;

    RwReal dTheta = p2->theta - p1->theta;
    if (dTheta >= -3.14159274f) {
        if (dTheta > 3.14159274f) {
            dTheta -= 6.28318548f;
        }
    } else {
        dTheta += 6.28318548f;
    }

    out->theta = dTheta * alpha + p1->theta;
    out->s0    = (p2->s0 - p1->s0) * alpha + p1->s0;
    out->s1    = (p2->s1 - p1->s1) * alpha + p1->s1;
    out->skew  = (p2->skew - p1->skew) * alpha + p1->skew;
    out->x     = (p2->x - p1->x) * alpha + p1->x;
    out->y     = (p2->y - p1->y) * alpha + p1->y;
}


void RpUVAnimParamKeyFrameInterpolate(void* voidOut, void* voidIn1, void* voidIn2, RwReal time, void* customData) {
    const RpUVAnimKeyFrame* k1 = (const RpUVAnimKeyFrame*)voidIn1;
    const RpUVAnimKeyFrame* k2 = (const RpUVAnimKeyFrame*)voidIn2;
    const RpUVAnimParamKeyFrameData* p1 = &k1->data.param;
    const RpUVAnimParamKeyFrameData* p2 = &k2->data.param;
    RpUVAnimParamKeyFrameData* out = &((RpUVAnimInterpFrame*)voidOut)->data.param;

    const RwReal t = (time - k1->time) / (k2->time - k1->time);

    // wrap angular delta into (-pi, pi] so theta interpolates the short way around
    RwReal dTheta = p2->theta - p1->theta;
    if (dTheta >= -3.14159274f) {
        if (dTheta > 3.14159274f) {
            dTheta -= 6.28318548f;
        }
    } else {
        dTheta += 6.28318548f;
    }

    out->theta = dTheta * t + p1->theta;
    out->s0    = (p2->s0 - p1->s0) * t + p1->s0;
    out->s1    = (p2->s1 - p1->s1) * t + p1->s1;
    out->skew  = (p2->skew - p1->skew) * t + p1->skew;
    out->x     = (p2->x - p1->x) * t + p1->x;
    out->y     = (p2->y - p1->y) * t + p1->y;
}


void RpUVAnimParamKeyFrameAdd(void* voidOut, void* voidIn1, void* voidIn2) {
    (void)voidOut;
    (void)voidIn1;
    (void)voidIn2;
}


void RpUVAnimParamKeyFrameMulRecip(void* voidFrame, void* voidStart) {
    (void)voidFrame;
    (void)voidStart;
}


RtAnimAnimation* RpUVAnimKeyFrameStreamRead(RwStream* stream, RtAnimAnimation* animation) {
    RwInt32 reserved;
    if (RwStreamReadInt32(stream, &reserved, sizeof(RwInt32)) == nullptr) {
        return nullptr;
    }
    _rpUVAnimCustomData* customData = _rpUVAnimCustomDataStreamRead(stream);
    if (customData == nullptr) {
        return nullptr;
    }
    animation->customData = customData;

    RpUVAnimKeyFrame* frames = (RpUVAnimKeyFrame*)animation->pFrames;
    for (RwInt32 i = 0; i < animation->numFrames; i++) {
        RpUVAnimKeyFrame* frame = &frames[i];
        if (RwStreamReadReal(stream, &frame->time, 0x1C /* time + keyframe data (6 floats) */) == nullptr) {
            return nullptr;
        }
        RwInt32 prevIndex;
        if (RwStreamReadInt32(stream, &prevIndex, sizeof(RwInt32)) == nullptr) {
            return nullptr;
        }
        frame->prevFrame = &frames[prevIndex];
    }
    return animation;
}


RwBool RpUVAnimKeyFrameStreamWrite(const RtAnimAnimation* animation, RwStream* stream) {
    RwInt32 reserved = 0;
    if (RwStreamWriteInt32(stream, &reserved, sizeof(RwInt32)) == nullptr) {
        return FALSE;
    }
    if (_rpUVAnimCustomDataStreamWrite((const _rpUVAnimCustomData*)animation->customData, stream) == nullptr) {
        return FALSE;
    }
    const RpUVAnimKeyFrame* frames = (const RpUVAnimKeyFrame*)animation->pFrames;
    for (RwInt32 i = 0; i < animation->numFrames; i++) {
        const RpUVAnimKeyFrame* frame = &frames[i];
        if (RwStreamWriteReal(stream, &frame->time, 0x1C /* time + keyframe data (6 floats) */) == nullptr) {
            return FALSE;
        }
        RwInt32 prevIndex = (RwInt32)((const RwUInt8*)frame->prevFrame - (const RwUInt8*)frames) >> 5 /* / sizeof(RpUVAnimKeyFrame) = 0x20 */;
        if (RwStreamWriteInt32(stream, &prevIndex, sizeof(RwInt32)) == nullptr) {
            return FALSE;
        }
    }
    return TRUE;
}


RwInt32 RpUVAnimKeyFrameStreamGetSize(const RtAnimAnimation* animation) {
    return animation->numFrames * 0x20 /* sizeof(RpUVAnimKeyFrame) */
         + _rpUVAnimCustomDataStreamGetSize((const _rpUVAnimCustomData*)animation->customData)
         + 4 /* leading reserved RwInt32 written by RpUVAnimKeyFrameStreamWrite */;
}


static auto& g_uvAnimLinearInfo     = StaticRef<RtAnimInterpolatorInfo>(0x8DED84);
static auto& g_uvAnimParamInfo      = StaticRef<RtAnimInterpolatorInfo>(0x8DEDB4);

RpUVAnim* RpUVAnimCreate(const RwChar* name, RwUInt32 numNodes, RwUInt32 numFrames, RwReal duration, RwUInt32* nodeIndexToUVChannelMap, RpUVAnimKeyFrameType keyframeType) {
    RwInt32 typeID;
    if (keyframeType == rpUVANIMLINEARKEYFRAMES) {
        typeID = g_uvAnimLinearInfo.typeID;   // binary: 448 (rwID_UVANIMLINEAR, rpcriter.h:443)
    } else if (keyframeType == rpUVANIMPARAMKEYFRAMES) {
        typeID = g_uvAnimParamInfo.typeID;    // binary: 449 (rwID_UVANIMPARAM)
    } else {
        return nullptr;
    }

    RtAnimAnimation* anim = RtAnimAnimationCreate(typeID, numFrames, 0, duration);
    if (anim == nullptr) {
        return nullptr;
    }

    // alloc of sizeof(_rpUVAnimCustomData) (0x44), hint 0x30135: *(fn**)(RwEngineInstance + 0x144)(cookie@0xC978F4, 0x30135)
    _rpUVAnimCustomData* customData = (_rpUVAnimCustomData*)(*(void*(__cdecl**)(void*, RwUInt32))((RwUInt8*)RwEngineInstance + 0x144))(g_uvAnimCustomDataHeap, 0x30135);
    if (customData == nullptr) {
        return nullptr;   // NOTE: original leaks the created RtAnimAnimation on this path - mirrored as-is
    }

    strncpy(customData->name, name, RP_UVANIM_MAXNAME);
    customData->name[RP_UVANIM_MAXNAME - 1] = '\0';
    for (RwUInt32 i = 0; i < numNodes; i++) {
        customData->nodeToUVChannelMap[i] = nodeIndexToUVChannelMap[i];
    }
    customData->refCount = 1;
    anim->customData = customData;
    return (RpUVAnim*)anim;
}



RwBool RpUVAnimDestroy(RpUVAnim* anim) {
    _rpUVAnimCustomData* customData = (_rpUVAnimCustomData*)anim->customData;
    customData->refCount--;
    if (customData->refCount == 0) {
        // free through engine memory funcs: *(fn**)(RwEngineInstance + 0x148)(cookie@0xC978F4, ptr)
        (*(void(__cdecl**)(void*, void*))((RwUInt8*)RwEngineInstance + 0x148))(g_uvAnimCustomDataHeap, customData);
        RtAnimAnimationDestroy(anim);
    }
    return TRUE;
}


RpUVAnim* RpUVAnimAddRef(RpUVAnim* anim) {
    ((_rpUVAnimCustomData*)anim->customData)->refCount++;
    return anim;
}


const RwChar* RpUVAnimGetName(const RpUVAnim* anim) {
    return (const RwChar*)(const _rpUVAnimCustomData*)anim->customData;
}



RpMaterial* RpMaterialSetUVAnim(RpMaterial* material, RpUVAnim* anim, RwUInt32 slot) {
    RpUVAnimMaterialExtension* matExt = (RpUVAnimMaterialExtension*)((RwUInt8*)material + g_uvAnimMatGlobals.engineOffset);

    RtAnimInterpolator* interp = RtAnimInterpolatorCreate(RtAnimAnimationGetNumNodes(anim),
                                                          anim->interpInfo->interpKeyFrameSize /* 0x20 for both uvanim schemes */);

    matExt->interp[slot] = interp;   // store happens before the null check in the original
    if (interp == nullptr) {
        return nullptr;
    }

    RtAnimInterpolatorSetCurrentAnim(interp, anim);   // 0x7CD5A0, already implemented in rtanim.cpp

    // internal "apply update for this material extension" helper - not part of the 849-address floor, kept as raw call
    if (!((RwBool(__cdecl*)(void*))0x7CBD80)(matExt)) {
        RtAnimInterpolatorDestroy(matExt->interp[slot]);
        return nullptr;
    }

    ((_rpUVAnimCustomData*)anim->customData)->refCount++;
    return material;
}



RpUVAnimInterpolator* RpMaterialUVAnimGetInterpolator(RpMaterial* material, RwUInt32 slot) {
    RpUVAnimMaterialExtension* matExt = (RpUVAnimMaterialExtension*)((RwUInt8*)material + g_uvAnimMatGlobals.engineOffset);
    return matExt->interp[slot];
}



RpMaterial* RpMaterialUVAnimSetInterpolator(RpMaterial* material, RpUVAnimInterpolator* interp, RwUInt32 slot) {
    RpUVAnimMaterialExtension* matExt = (RpUVAnimMaterialExtension*)((RwUInt8*)material + g_uvAnimMatGlobals.engineOffset);
    matExt->interp[slot] = interp;
    return material;
}



RpMaterial* RpMaterialUVAnimSetCurrentTime(RpMaterial* material, RwReal time) {
    RpUVAnimMaterialExtension* matExt = (RpUVAnimMaterialExtension*)((RwUInt8*)material + g_uvAnimMatGlobals.engineOffset);
    for (RwInt32 i = 0; i < RP_UVANIM_MAXSLOTS; i++) {
        if (matExt->interp[i]) {
            RtAnimInterpolatorSetCurrentTime(matExt->interp[i], time);   // 0x7CDAB0 wrapper, result ignored by original
        }
    }
    return material;
}



RpMaterial* RpMaterialUVAnimAddAnimTime(RpMaterial* material, RwReal deltaTime) {
    RpUVAnimMaterialExtension* matExt = (RpUVAnimMaterialExtension*)((RwUInt8*)material + g_uvAnimMatGlobals.engineOffset);
    for (RwInt32 i = 0; i < RP_UVANIM_MAXSLOTS; i++) {
        if (matExt->interp[i]) {
            RtAnimInterpolatorAddAnimTime(matExt->interp[i], deltaTime);   // 0x7CD8D0 wrapper, result ignored by original
        }
    }
    return material;
}



RpMaterial* RpMaterialUVAnimSubAnimTime(RpMaterial* material, RwReal deltaTime) {
    RpUVAnimMaterialExtension* matExt = (RpUVAnimMaterialExtension*)((RwUInt8*)material + g_uvAnimMatGlobals.engineOffset);
    for (RwInt32 i = 0; i < RP_UVANIM_MAXSLOTS; i++) {
        if (matExt->interp[i]) {
            RtAnimInterpolatorSubAnimTime(matExt->interp[i], deltaTime);   // 0x7CD760 wrapper, result ignored by original
        }
    }
    return material;
}


RpMaterial* RpMaterialUVAnimApplyUpdate(RpMaterial* material)
{
    return plugin::CallAndReturn<RpMaterial*, 0x7CC110, RpMaterial*>(material);
}


RwBool RpMaterialUVAnimExists(const RpMaterial* material) {
    const RpUVAnimMaterialExtension* matExt = (const RpUVAnimMaterialExtension*)((const RwUInt8*)material + g_uvAnimMatGlobals.engineOffset);
    for (RwInt32 i = 0; i < RP_UVANIM_MAXSLOTS; i++) {
        if (matExt->interp[i]) {
            return TRUE;
        }
    }
    return FALSE;
}

