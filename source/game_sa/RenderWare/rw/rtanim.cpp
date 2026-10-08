#include "StdInc.h"

#include "rpdbgerr.h"
#include "rtanim.h"

void RtAnimAnimationFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    StaticRef<RwInt32>(0x8DEDEC) = blockSize;            // DAT_008dedec
    StaticRef<RwInt32>(0x8DEDF0) = numBlocksToPrealloc;  // DAT_008dedf0
}


RwBool RtAnimInitialize() {
    // Registers the rtanim module plugin (ID 0x1B7, data size 0) in the core plugin registry;
    // returns registry result > 0.
    return ((int(__cdecl*)(RwInt32, RwInt32, void*, void*))0x7F2BB0)(0, 0x1B7, (void*)0x7CCCD0, (void*)0x7CCD10) > 0;
}


RwBool RtAnimRegisterInterpolationScheme(RtAnimInterpolatorInfo* interpolatorInfo) {
    auto& schemeCount = StaticRef<RwInt32>(0xC97900);                       // DAT_00c97900
    auto* schemeTable = reinterpret_cast<RtAnimInterpolatorInfo*>(0xC9B920); // scheme table

    if (schemeCount > 0x10) {
        RwError error;
        error.pluginID  = 0x1B7;   // rtanim module plugin ID
        error.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(1); // 0x8088D0 = passthrough thunk (mov eax,[esp+4]; ret)
        RwErrorSet(&error);
        return FALSE;
    }

    RwInt32 i = 0;
    if (schemeCount > 0) {
        for (i = 0; i < schemeCount; i++) {
            if (schemeTable[i].typeID == interpolatorInfo->typeID) {
                RwError error;
                error.pluginID  = 0x1B7;
                error.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0);
                RwErrorSet(&error);
                return FALSE;
            }
        }
    }

    // Copy the 12 dwords of the info struct (sizeof(RtAnimInterpolatorInfo) / 4 = 0x30 / 4) into the table
    RwInt32* dst = reinterpret_cast<RwInt32*>(&schemeTable[schemeCount]);
    RwInt32* src = reinterpret_cast<RwInt32*>(interpolatorInfo);
    for (i = 0x0C; i != 0; i--) {
        *dst++ = *src++;
    }
    schemeCount++;
    return TRUE;
}


RtAnimInterpolatorInfo* RtAnimGetInterpolatorInfo(RwInt32 typeID) {
    auto& schemeCount = StaticRef<RwInt32>(0xC97900);                        // DAT_00c97900
    auto* schemeTable = reinterpret_cast<RtAnimInterpolatorInfo*>(0xC9B920); // scheme table

    RwInt32 i = 0;
    if (schemeCount > 0) {
        for (i = 0; i < schemeCount; i++) {
            if (schemeTable[i].typeID == typeID) {
                return &schemeTable[i];
            }
        }
    }

    RwError error;
    error.pluginID  = 0x1B7;
    error.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(2);
    RwErrorSet(&error);
    return NULL;
}


typedef void* (__cdecl* RwAnimMemAllocFn_t)(RwUInt32 size, RwUInt32 hint);
typedef void  (__cdecl* RwAnimMemFreeFn_t)(void* mem);

RtAnimAnimation* RtAnimAnimationCreate(RwInt32 typeID, RwInt32 numFrames, RwInt32 flags, RwReal duration) {
    auto& schemeCount = StaticRef<RwInt32>(0xC97900);                        // DAT_00c97900
    auto* schemeTable = reinterpret_cast<RtAnimInterpolatorInfo*>(0xC9B920); // scheme table

    RwInt32 i = 0;
    if (schemeCount > 0) {
        for (i = 0; i < schemeCount; i++) {
            if (schemeTable[i].typeID == typeID) {
                RtAnimInterpolatorInfo* interpInfo = &schemeTable[i];
                if (!interpInfo) {  // redundant check present in the binary
                    return NULL;
                }
                // alloc(sizeof(RtAnimAnimation) + numFrames * animKeyFrameSize + customDataSize)
                RtAnimAnimation* animation = (RtAnimAnimation*)(*(RwAnimMemAllocFn_t*)((RwUInt8*)RwEngineInstance + 0x134))(
                    0x18 + interpInfo->animKeyFrameSize * numFrames + interpInfo->customDataSize,
                    rwMEMHINTDUR_EVENT | rwID_ANIMANIMATION);
                animation->flags      = flags;   // binary write order: [2], [1], [3], [0]
                animation->numFrames  = numFrames;
                animation->duration   = duration;
                animation->interpInfo = interpInfo;
                animation->pFrames    = animation + 1;  // keyframes follow the struct (puVar2 + 6)
                if (interpInfo->customDataSize > 0) {
                    animation->customData = (RwUInt8*)animation->pFrames + interpInfo->animKeyFrameSize * numFrames;
                } else {
                    animation->customData = NULL;
                }
                return animation;
            }
        }
    }

    RwError error;
    error.pluginID  = 0x1B7;
    error.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(2);
    RwErrorSet(&error);
    return NULL;
}


typedef void* (__cdecl* RwAnimMemAllocFn_t)(RwUInt32 size, RwUInt32 hint);
typedef void  (__cdecl* RwAnimMemFreeFn_t)(void* mem);

RwBool RtAnimAnimationDestroy(RtAnimAnimation* animation) {
    (*(RwAnimMemFreeFn_t*)((RwUInt8*)RwEngineInstance + 0x138))(animation);
    return TRUE;
}


typedef void* (__cdecl* RwAnimMemAllocFn_t)(RwUInt32 size, RwUInt32 hint);
typedef void  (__cdecl* RwAnimMemFreeFn_t)(void* mem);

RtAnimAnimation* RtAnimAnimationRead(const RwChar* filename) {
    RtAnimAnimation* result = NULL;
    RwStream* stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, filename);
    if (!stream) {
        return NULL;
    }
    if (RwStreamFindChunk(stream, rwID_ANIMANIMATION, NULL, NULL)) {
        RwInt32 version;
        if (RwStreamReadInt32(stream, &version, 4)) {
            if (version == rtANIMSTREAMCURRENTVERSION) {
                RwInt32 typeID;
                if (RwStreamReadInt32(stream, &typeID, 4)) {
                    auto& schemeCount = StaticRef<RwInt32>(0xC97900);                        // DAT_00c97900
                    auto* schemeTable = reinterpret_cast<RtAnimInterpolatorInfo*>(0xC9B920); // scheme table
                    RwInt32 i = 0;
                    // first search for the interpolation scheme by typeID
                    RtAnimInterpolatorInfo* interpInfo = NULL;
                    if (schemeCount > 0) {
                        for (i = 0; i < schemeCount; i++) {
                            if (schemeTable[i].typeID == typeID) {
                                interpInfo = &schemeTable[i];
                                break;
                            }
                        }
                    }
                    if (interpInfo) {
                        RwInt32 numFrames;
                        RwInt32 flags;
                        RwReal duration;
                        if (RwStreamReadInt32(stream, &numFrames, 4)) {
                            if (RwStreamReadInt32(stream, &flags, 4)) {
                                if (RwStreamReadReal(stream, &duration, 4)) {
                                    // the binary searches the scheme table a second time before allocating
                                    interpInfo = NULL;
                                    i = 0;
                                    if (schemeCount > 0) {
                                        for (i = 0; i < schemeCount; i++) {
                                            if (schemeTable[i].typeID == typeID) {
                                                interpInfo = &schemeTable[i];
                                                break;
                                            }
                                        }
                                    }
                                    if (interpInfo) {
                                        RtAnimAnimation* animation = (RtAnimAnimation*)(*(RwAnimMemAllocFn_t*)((RwUInt8*)RwEngineInstance + 0x134))(
                                            0x18 + interpInfo->animKeyFrameSize * numFrames + interpInfo->customDataSize,
                                            rwMEMHINTDUR_EVENT | rwID_ANIMANIMATION);
                                        animation->numFrames  = numFrames;
                                        animation->duration   = duration;
                                        animation->flags      = flags;
                                        animation->interpInfo = interpInfo;
                                        animation->pFrames    = animation + 1;
                                        if (interpInfo->customDataSize > 0) {
                                            animation->customData = (RwUInt8*)animation->pFrames + interpInfo->animKeyFrameSize * numFrames;
                                        } else {
                                            animation->customData = NULL;
                                        }
                                        interpInfo->keyFrameStreamReadCB(stream, animation);
                                        result = animation;
                                    } else {
                                        RwError error;
                                        error.pluginID  = 0x1B7;
                                        error.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(2);
                                        RwErrorSet(&error);
                                        result = NULL;
                                    }
                                }
                            }
                        }
                    } else {
                        RwError error;
                        error.pluginID  = 0x1B7;
                        error.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(2);
                        RwErrorSet(&error);
                        result = NULL;
                    }
                }
            }
        }
    }
    RwStreamClose(stream, NULL);
    return result;
}


RwBool RtAnimAnimationWrite(const RtAnimAnimation* animation, const RwChar* filename) {
    RwStream* stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMWRITE, filename);
    if (!stream) {
        return FALSE;
    }
    // chunk payload: keyframes (via scheme's get-size CB) + 0x14 bytes header (version, typeID, numFrames, flags, duration)
    RwInt32 size = animation->interpInfo->keyFrameStreamGetSizeCB(animation);
    _rwStreamWriteVersionedChunkHeader(stream, rwID_ANIMANIMATION, size + 0x14, 0x36003 /* rw 3.6 stream version */, 0xFFFF /* build num */);
    RwInt32 version = rtANIMSTREAMCURRENTVERSION;
    if (RwStreamWriteInt32(stream, (RwInt32*)&version, 4)) {
        if (RwStreamWriteInt32(stream, (RwInt32*)animation->interpInfo, 4)) {  // writes interpInfo->typeID (first dword)
            if (RwStreamWriteInt32(stream, (RwInt32*)&animation->numFrames, 4)) {
                if (RwStreamWriteInt32(stream, (RwInt32*)&animation->flags, 4)) {
                    if (RwStreamWriteReal(stream, (RwReal*)&animation->duration, 4)) {
                        animation->interpInfo->keyFrameStreamWriteCB(animation, stream);
                    }
                }
            }
        }
    }
    RwStreamClose(stream, NULL);
    return TRUE;
}


typedef void* (__cdecl* RwAnimMemAllocFn_t)(RwUInt32 size, RwUInt32 hint);
typedef void  (__cdecl* RwAnimMemFreeFn_t)(void* mem);

RtAnimAnimation* RtAnimAnimationStreamRead(RwStream* stream) {
    RwInt32 version;
    if (!RwStreamReadInt32(stream, &version, 4)) {
        return NULL;
    }
    if (version != rtANIMSTREAMCURRENTVERSION) {
        return NULL;
    }
    RwInt32 typeID;
    if (!RwStreamReadInt32(stream, &typeID, 4)) {
        return NULL;
    }

    auto& schemeCount = StaticRef<RwInt32>(0xC97900);                        // DAT_00c97900
    auto* schemeTable = reinterpret_cast<RtAnimInterpolatorInfo*>(0xC9B920); // scheme table
    RwInt32 i = 0;
    // first search
    RtAnimInterpolatorInfo* interpInfo = NULL;
    if (schemeCount > 0) {
        for (i = 0; i < schemeCount; i++) {
            if (schemeTable[i].typeID == typeID) {
                interpInfo = &schemeTable[i];
                break;
            }
        }
    }
    if (!interpInfo) {
        RwError error;
        error.pluginID  = 0x1B7;
        error.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(2);
        RwErrorSet(&error);
        return NULL;
    }

    RwInt32 numFrames;
    RwInt32 flags;
    RwReal  duration;
    if (!RwStreamReadInt32(stream, &numFrames, 4)) {
        return NULL;
    }
    if (!RwStreamReadInt32(stream, &flags, 4)) {
        return NULL;
    }
    if (!RwStreamReadReal(stream, &duration, 4)) {
        return NULL;
    }

    // second search (as in the binary)
    interpInfo = NULL;
    i = 0;
    if (schemeCount > 0) {
        for (i = 0; i < schemeCount; i++) {
            if (schemeTable[i].typeID == typeID) {
                interpInfo = &schemeTable[i];
                break;
            }
        }
    }
    if (!interpInfo) {
        RwError error;
        error.pluginID  = 0x1B7;
        error.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(2);
        RwErrorSet(&error);
        return NULL;
    }

    RtAnimAnimation* animation = (RtAnimAnimation*)(*(RwAnimMemAllocFn_t*)((RwUInt8*)RwEngineInstance + 0x134))(
        0x18 + interpInfo->animKeyFrameSize * numFrames + interpInfo->customDataSize,
        rwMEMHINTDUR_EVENT | rwID_ANIMANIMATION);
    animation->duration   = duration;
    animation->numFrames  = numFrames;
    animation->flags      = flags;
    animation->interpInfo = interpInfo;
    animation->pFrames    = animation + 1;
    if (interpInfo->customDataSize > 0) {
        animation->customData = (RwUInt8*)animation->pFrames + interpInfo->animKeyFrameSize * numFrames;
    } else {
        animation->customData = NULL;
    }
    interpInfo->keyFrameStreamReadCB(stream, animation);
    return animation;
}


RwBool RtAnimAnimationStreamWrite(const RtAnimAnimation* animation, RwStream* stream) {
    RwInt32 size = animation->interpInfo->keyFrameStreamGetSizeCB(animation);
    _rwStreamWriteVersionedChunkHeader(stream, rwID_ANIMANIMATION, size + 0x14, 0x36003 /* rw 3.6 stream version */, 0xFFFF /* build num */);
    RwInt32 version = rtANIMSTREAMCURRENTVERSION;
    if (!RwStreamWriteInt32(stream, (RwInt32*)&version, 4)) {
        return FALSE;
    }
    if (!RwStreamWriteInt32(stream, (RwInt32*)animation->interpInfo, 4)) {  // writes interpInfo->typeID (first dword)
        return FALSE;
    }
    if (!RwStreamWriteInt32(stream, (RwInt32*)&animation->numFrames, 4)) {
        return FALSE;
    }
    if (!RwStreamWriteInt32(stream, (RwInt32*)&animation->flags, 4)) {
        return FALSE;
    }
    if (!RwStreamWriteReal(stream, (RwReal*)&animation->duration, 4)) {
        return FALSE;
    }
    animation->interpInfo->keyFrameStreamWriteCB(animation, stream);
    return TRUE;
}


RwInt32 RtAnimAnimationStreamGetSize(const RtAnimAnimation* animation) {
    // keyframe data size (scheme CB) + 0x14 bytes stream header (version, typeID, numFrames, flags, duration)
    return animation->interpInfo->keyFrameStreamGetSizeCB(animation) + 0x14;
}


RwUInt32 RtAnimAnimationGetNumNodes(const RtAnimAnimation* animation) {
    RwUInt32 numNodes = 0;
    RwInt32 kfSize = animation->interpInfo->animKeyFrameSize;
    RtAnimKeyFrameHeader* first = (RtAnimKeyFrameHeader*)animation->pFrames;
    if (first->prevFrame == first) {
        return 0;
    }
    RtAnimKeyFrameHeader* key = first;
    while (true) {
        key = (RtAnimKeyFrameHeader*)((RwUInt8*)key + kfSize);
        numNodes++;
        if (key->prevFrame == first) {
            break;
        }
    }
    return numNodes;
}


typedef void* (__cdecl* RwAnimMemAllocFn_t)(RwUInt32 size, RwUInt32 hint);
typedef void  (__cdecl* RwAnimMemFreeFn_t)(void* mem);

RtAnimInterpolator* RtAnimInterpolatorCreate(RwInt32 numNodes, RwInt32 maxInterpKeyFrameSize) {
    RtAnimInterpolator* animI = (RtAnimInterpolator*)(*(RwAnimMemAllocFn_t*)((RwUInt8*)RwEngineInstance + 0x134))(
        numNodes * maxInterpKeyFrameSize + 0x4C,
        rwMEMHINTDUR_EVENT | rwID_ANIMANIMATION);
    animI->numNodes                  = numNodes;                  // [0x0B]
    animI->currentInterpKeyFrameSize = maxInterpKeyFrameSize;     // [0x09]
    animI->maxInterpKeyFrameSize     = maxInterpKeyFrameSize;     // [0x08]
    animI->pCurrentAnim              = NULL;                      // [0x00]
    animI->pNextFrame                = NULL;                      // [0x02]
    animI->currentTime               = 0.0f;                      // [0x01] (integer 0 store in binary)
    animI->pAnimCallBack             = NULL;                      // [0x03]
    animI->animCallBackTime          = -1.0f;                     // [0x05] = 0xBF800000
    animI->pAnimCallBackData         = NULL;                      // [0x04]
    animI->pAnimLoopCallBack         = NULL;                      // [0x06]
    animI->pAnimLoopCallBackData     = NULL;                      // [0x07]
    animI->currentAnimKeyFrameSize   = -1;                        // [0x0A] = 0xFFFFFFFF (invalid until SetCurrentAnim)
    animI->isSubInterpolator         = FALSE;                     // [0x0C]
    animI->offsetInParent            = 0;                         // [0x0D]
    animI->parentAnimation           = animI;                     // [0x0E] = self
    animI->keyFrameApplyCB           = NULL;                      // [0x0F]
    animI->keyFrameInterpolateCB     = NULL;                      // [0x11]
    animI->keyFrameBlendCB           = NULL;                      // [0x10]
    animI->keyFrameAddCB             = NULL;                      // [0x12]
    return animI;
}


typedef void* (__cdecl* RwAnimMemAllocFn_t)(RwUInt32 size, RwUInt32 hint);
typedef void  (__cdecl* RwAnimMemFreeFn_t)(void* mem);

void RtAnimInterpolatorDestroy(RtAnimInterpolator* anim) {
    (*(RwAnimMemFreeFn_t*)((RwUInt8*)RwEngineInstance + 0x138))(anim);
}

 
/**
 * \ingroup rtanim
 * \ref RtAnimInterpolatorSetCurrentAnim sets the current animation on the
 * animation interpolator. It is assumed that the animation is designed for the
 * animation interpolator it is being set on since no animation interpolator structure
 * comparisons are made. The animation defines the interpolation schemes used
 * and they will be setup on the animation interpolator at this call. The maximum
 * keyframe size of the animation interpolator must be sufficient to support the
 * keyframe size required by the interpolation scheme. The animation is
 * initialized to time zero at this call.
 *
 * \param animI Pointer to the \ref RtAnimInterpolator.
 * \param anim Pointer to the \ref RtAnimAnimation.
 *
 * \return \ref RwBool, TRUE on success, FALSE if an error occurs.
 *
 */
RwBool
RtAnimInterpolatorSetCurrentAnim(RtAnimInterpolator *animI,
                               RtAnimAnimation *anim)
{
    RtAnimInterpolatorInfo     *interpInfo;
    RwInt32                     i;
    void                       *kf1, *kf2, *interpFrame;

    RWAPIFUNCTION(RWSTRING("RtAnimInterpolatorSetCurrentAnim"));
    RWASSERT(animI);
    RWASSERT(anim);
    RWASSERT(anim->numFrames >= animI->numNodes * 2);

    animI->pCurrentAnim = anim;
    animI->currentTime = 0.0f;

    /* Get interpolator functions */
    interpInfo = anim->interpInfo;
    animI->currentInterpKeyFrameSize = interpInfo->interpKeyFrameSize;
    RWASSERT(animI->currentInterpKeyFrameSize <= animI->maxInterpKeyFrameSize);
    animI->currentAnimKeyFrameSize = interpInfo->animKeyFrameSize;

    animI->keyFrameApplyCB = interpInfo->keyFrameApplyCB;
    animI->keyFrameBlendCB = interpInfo->keyFrameBlendCB;
    animI->keyFrameInterpolateCB = interpInfo->keyFrameInterpolateCB;
    animI->keyFrameAddCB = interpInfo->keyFrameAddCB;

    /*
     * Set up initial interpolation frames for time=0
     * First copy the time=0 keyframes to the interpolated frame array
     */
    //TODO: convert these don't copy
    for (i=0; i < animI->numNodes; i++)
    {
        animI->keyFrameInterpolateCB(rtANIMGETINTERPFRAME(animI, i),
                                     ((RwUInt8 *)anim->pFrames) + (i * animI->currentAnimKeyFrameSize),
                                     ((RwUInt8 *)anim->pFrames) + ((i + animI->numNodes) * animI->currentAnimKeyFrameSize),
                                     0.0f, anim->customData);
    }

    /*
     * Now initialize the interpolated frame headers to point to the initial
     * keyframe pairs.
     */
    if (!notsa::IsFixBugs() || anim->interpInfo->typeID != rwID_RPANIMBLENDPLUGIN) {
        /*
         * In RW there is an RtAnimAnimation, which holds the raw keyframes of an animation.
         * To play an animation, an RtAnimAnimation is given to an RtAnimInterpolator which
         * then interpolates the keyframes into interpolation frames which are then used to
         * animate e.g. a hierarchy by generating matrices from them.
         * But R* hasn't really used RW's animation system since III.
         * In III and VC R* wrote their own interpolated animation data into a dummy
         * RtAnimAnimation and practically didn't use the RtAnimInterpolator.
         * In SA they went a step further and now write into the interpolation frames of
         * the RtAnimInterpolator instead, almost completely avoid RtAnimAnimation.
         * Almost! They're still attaching an RtAnimAnimation to an RtAnimInterpolator
         * and this is where stuff breaks.
         *
         * So, instead of corrupting our lovely `RpHAnimBlendInterpFrame`'s, we leave them alone ;)
         *
         * For more info see: https://gtaforums.com/topic/669045-silentpatch/page/154/#comment-1068971599
         */

        interpFrame = rtANIMGETINTERPFRAME(animI, 0);
        kf1 = anim->pFrames;
        kf2 = (RwInt8 *)anim->pFrames + animI->numNodes * animI->currentAnimKeyFrameSize;

        for (i=0; i < animI->numNodes; i++)
        {
            RtAnimInterpFrameHeader  *hdr = (RtAnimInterpFrameHeader *)interpFrame;

            hdr->keyFrame1 = (RtAnimKeyFrameHeader*)kf1;
            hdr->keyFrame2 = (RtAnimKeyFrameHeader*)kf2;
           
            interpFrame = (RwInt8 *)interpFrame + animI->currentInterpKeyFrameSize;
            kf1 = (RwInt8 *)kf1 + animI->currentAnimKeyFrameSize;
            kf2 = (RwInt8 *)kf2 + animI->currentAnimKeyFrameSize;
        }
    }
    animI->pNextFrame = (RwUInt8 *)anim->pFrames + (animI->numNodes * animI->currentAnimKeyFrameSize * 2);

    RWRETURN(TRUE);
}

RwBool RtAnimInterpolatorSetKeyFrameCallBacks(RtAnimInterpolator* anim, RwInt32 keyFrameTypeID) {
    auto& schemeCount = StaticRef<RwInt32>(0xC97900);                        // DAT_00c97900
    auto* schemeTable = reinterpret_cast<RtAnimInterpolatorInfo*>(0xC9B920); // scheme table

    RwInt32 i = 0;
    if (schemeCount > 0) {
        for (i = 0; i < schemeCount; i++) {
            if (schemeTable[i].typeID == keyFrameTypeID) {
                RtAnimInterpolatorInfo* interpInfo = &schemeTable[i];
                if (!interpInfo) {  // redundant check present in the binary
                    return FALSE;
                }
                anim->keyFrameApplyCB       = interpInfo->keyFrameApplyCB;        // anim+0x3C
                anim->keyFrameBlendCB       = interpInfo->keyFrameBlendCB;        // anim+0x40
                anim->keyFrameInterpolateCB = interpInfo->keyFrameInterpolateCB;  // anim+0x44
                anim->keyFrameAddCB         = interpInfo->keyFrameAddCB;          // anim+0x48
                return TRUE;
            }
        }
    }

    RwError error;
    error.pluginID  = 0x1B7;
    error.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(2);
    RwErrorSet(&error);
    return FALSE;
}


void RtAnimInterpolatorSetAnimLoopCallBack(RtAnimInterpolator* anim, RtAnimCallBack callBack, void* data) {
    anim->pAnimLoopCallBack     = callBack;  // anim+0x18
    anim->pAnimLoopCallBackData = data;      // anim+0x1C
}


void RtAnimInterpolatorSetAnimCallBack(RtAnimInterpolator* anim, RtAnimCallBack callBack, RwReal time, void* data) {
    anim->pAnimCallBack     = callBack;  // anim+0x0C
    anim->animCallBackTime  = time;      // anim+0x14
    anim->pAnimCallBackData = data;      // anim+0x10
}


RwBool RtAnimInterpolatorCopy(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim) {
    // copies inAnim's interpolated-frames block (after the 0x4C-byte struct) into outAnim
    RwUInt32 size = outAnim->numNodes * outAnim->currentInterpKeyFrameSize;  // [0x2C] * [0x24]
    memcpy((RwUInt8*)outAnim + 0x4C, (RwUInt8*)inAnim + 0x4C, size);
    return TRUE;
}


RwBool RtAnimInterpolatorSubAnimTime(RtAnimInterpolator* anim, RwReal time) {
    RwBool callAnimCB = FALSE;
    RwBool callLoopCB = FALSE;

    // crossed the callback time with this subtraction?
    // (binary: bVar3 = currentTime > animCallBackTime && currentTime - time < animCallBackTime)
    if (anim->currentTime > anim->animCallBackTime
        && anim->currentTime - time < anim->animCallBackTime) {
        callAnimCB = TRUE;
    }

    RwReal newTime = anim->currentTime - time;
    anim->currentTime = newTime;

    if (newTime >= 0.0f) {  // DAT_00858B50 == 0.0f
        // update interpolated frames for each node at the new time
        void* interpFrame = (RwUInt8*)anim + 0x4C;  // == rtANIMGETINTERPFRAME(anim, 0)
        for (RwInt32 i = anim->numNodes; i != 0; i--) {
            RtAnimInterpFrameHeader* hdr = (RtAnimInterpFrameHeader*)interpFrame;
            if (anim->currentTime < ((RtAnimKeyFrameHeader*)hdr->keyFrame1)->time) {
                do {
                    hdr->keyFrame2 = hdr->keyFrame1;
                    hdr->keyFrame1 = static_cast<RtAnimKeyFrameHeader*>(((RtAnimKeyFrameHeader*)hdr->keyFrame1)->prevFrame);
                } while (anim->currentTime < ((RtAnimKeyFrameHeader*)hdr->keyFrame1)->time);
            }
            anim->keyFrameInterpolateCB(hdr, hdr->keyFrame1, hdr->keyFrame2, anim->currentTime, anim->pCurrentAnim->customData);
            interpFrame = (RwUInt8*)interpFrame + anim->currentInterpKeyFrameSize;
        }
        anim->pNextFrame = NULL;
    } else {
        // wrapped below 0: add duration until in range, re-init the interpolator, then advance forward
        RwReal t = newTime;
        do {
            t += anim->pCurrentAnim->duration;
        } while (t < 0.0f);
        anim->currentTime = t;
        RtAnimInterpolatorSetCurrentAnim(anim, anim->pCurrentAnim);
        RtAnimInterpolatorAddAnimTime(anim, t);
        if (anim->animCallBackTime <= anim->currentTime) {
            callLoopCB = TRUE;
        } else {
            callAnimCB = TRUE;
            callLoopCB = TRUE;
        }
    }

    if (callAnimCB && anim->pAnimCallBack) {
        if (!anim->pAnimCallBack(anim, anim->pAnimCallBackData)) {
            anim->pAnimCallBack = NULL;
        }
    }
    if (callLoopCB && anim->pAnimLoopCallBack) {
        if (!anim->pAnimLoopCallBack(anim, anim->pAnimLoopCallBackData)) {
            anim->pAnimLoopCallBack = NULL;
            return TRUE;
        }
    }
    return TRUE;
}


RwBool RtAnimInterpolatorAddAnimTime(RtAnimInterpolator* animI, RwReal time) {
    if (time > 0.0f) {  // DAT_00858B50 == 0.0f
        RtAnimAnimation* anim = animI->pCurrentAnim;
        RwReal  duration = anim->duration;
        RwInt32 numNodes = animI->numNodes;
        RwReal  animCallBackTime = animI->animCallBackTime;

        RwBool callAnimCB = FALSE;
        RwBool callLoopCB = FALSE;
        // will we cross animCallBackTime in this step?
        if (!(animCallBackTime <= animI->currentTime)
            && !(animI->currentTime + time < animCallBackTime)) {
            callAnimCB = TRUE;
        }

        RwReal newTime = time + animI->currentTime;
        animI->currentTime = newTime;

        if (duration < newTime) {
            callLoopCB = TRUE;
            // wrap around the animation duration
            do {
                newTime -= duration;
                if (animCallBackTime <= newTime) {
                    callAnimCB = TRUE;
                }
            } while (duration < newTime);
            animI->currentTime = newTime;
            // reset interpolated frames to time 0, then restore the (wrapped) time
            // (binary keeps newTime on the FPU stack across the call and stores it back)
            RtAnimInterpolatorSetCurrentAnim(animI, anim);
            animI->currentTime = newTime;
        }

        if (animI->pNextFrame == NULL) {
            // re-sync interpolated frames, keeping currentTime as-is (same FPU-stack restore as above)
            RwReal t = animI->currentTime;
            RtAnimInterpolatorSetCurrentAnim(animI, anim);
            animI->currentTime = t;
        }

        // advance keyframe pairs whose next keyframe has time <= currentTime
        while ((RwUInt32)animI->pNextFrame < (RwUInt32)((RwUInt8*)anim->pFrames + anim->numFrames * animI->currentAnimKeyFrameSize)
            && ((RtAnimKeyFrameHeader*)animI->pNextFrame)->time <= animI->currentTime) {
            // find the interpolated frame whose keyFrame2 is pNextFrame
            RtAnimInterpFrameHeader* hdr;
            RwInt32 i = 0;
            do {
                hdr = (RtAnimInterpFrameHeader*)((RwUInt8*)animI + 0x4C + animI->currentInterpKeyFrameSize * i);
                i++;
                if (numNodes <= i) {
                    i = 0;
                }
            } while (hdr->keyFrame2 != (RtAnimKeyFrameHeader*)animI->pNextFrame);
            hdr->keyFrame1 = hdr->keyFrame2;
            hdr->keyFrame2 = (RtAnimKeyFrameHeader*)animI->pNextFrame;
            animI->pNextFrame = (RwUInt8*)animI->pNextFrame + animI->currentAnimKeyFrameSize;
        }

        // update all interpolated frames at the new time
        void* interpFrame = (RwUInt8*)animI + 0x4C;  // == rtANIMGETINTERPFRAME(animI, 0)
        for (RwInt32 i = numNodes; i != 0; i--) {
            RtAnimInterpFrameHeader* hdr = (RtAnimInterpFrameHeader*)interpFrame;
            animI->keyFrameInterpolateCB(hdr, hdr->keyFrame1, hdr->keyFrame2, animI->currentTime, anim->customData);
            interpFrame = (RwUInt8*)interpFrame + animI->currentInterpKeyFrameSize;
        }

        if (callAnimCB && animI->pAnimCallBack) {
            if (!animI->pAnimCallBack(animI, animI->pAnimCallBackData)) {
                animI->pAnimCallBack = NULL;
            }
        }
        if (callLoopCB && animI->pAnimLoopCallBack) {
            if (!animI->pAnimLoopCallBack(animI, animI->pAnimLoopCallBackData)) {
                animI->pAnimLoopCallBack = NULL;
                return TRUE;
            }
        }
    }
    return TRUE;
}


RwBool RtAnimInterpolatorSetCurrentTime(RtAnimInterpolator* anim, RwReal time) {
    time = time - anim->currentTime;
    if (time < 0.0f) {  // DAT_00858B50 == 0.0f
        RtAnimInterpolatorSubAnimTime(anim, -time);
        return TRUE;
    }
    RtAnimInterpolatorAddAnimTime(anim, time);
    return TRUE;
}


typedef void* (__cdecl* RwAnimMemAllocFn_t)(RwUInt32 size, RwUInt32 hint);
typedef void  (__cdecl* RwAnimMemFreeFn_t)(void* mem);

RwBool RtAnimAnimationMakeDelta(RtAnimAnimation* animation, RwInt32 numNodes, RwReal time) {
    RtAnimInterpolator* interp = RtAnimInterpolatorCreate(numNodes, animation->interpInfo->interpKeyFrameSize);
    if (!interp) {
        return FALSE;
    }
    RtAnimInterpolatorSetCurrentAnim(interp, animation);
    time = time - interp->currentTime;  // currentTime is 0 here, so time passes through
    if (time >= 0.0f) {                 // DAT_00858B50 == 0.0f
        RtAnimInterpolatorAddAnimTime(interp, time);
    } else {
        RtAnimInterpolatorSubAnimTime(interp, -time);
    }

    // For each node: multiply the interpolated frame by the reciprocal of every one of the
    // node's keyframes from the current time up to the animation duration.
    RwUInt8* key = (RwUInt8*)animation->pFrames;          // node i's first keyframe is at frames[i]
    RwUInt8* interpFrame = (RwUInt8*)interp + 0x4C;       // == rtANIMGETINTERPFRAME(interp, 0)
    RwInt32 kfAnim   = interp->currentAnimKeyFrameSize;   // interp+0x28
    RwInt32 kfInterp = interp->currentInterpKeyFrameSize; // interp+0x24
    for (RwInt32 i = 0; i < numNodes; i++) {
        RtAnimKeyFrameHeader* cur = (RtAnimKeyFrameHeader*)key;
        while (true) {
            animation->interpInfo->keyFrameMulRecipCB(cur, interpFrame);
            if (cur->time == animation->duration) {
                break;
            }
            // find the next keyframe of this node: the one whose prevFrame links back to cur
            if (cur->prevFrame != cur) {
                RtAnimKeyFrameHeader* probe = cur;
                RtAnimKeyFrameHeader* candidate;
                do {
                    candidate = (RtAnimKeyFrameHeader*)((RwUInt8*)probe + kfAnim);
                    probe = candidate;
                } while (candidate->prevFrame != cur);
                cur = candidate;
            }
        }
        interpFrame += kfInterp;
        key += kfAnim;
    }

    (*(RwAnimMemFreeFn_t*)((RwUInt8*)RwEngineInstance + 0x138))(interp);
    return TRUE;
}


RwBool RtAnimInterpolatorBlend(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim1, RtAnimInterpolator* inAnim2, RwReal alpha) {
    for (RwInt32 i = 0; i < outAnim->numNodes; i++) {
        outAnim->keyFrameBlendCB(
            (RwUInt8*)outAnim + 0x4C + outAnim->currentInterpKeyFrameSize * i,   // rtANIMGETINTERPFRAME(outAnim, i)
            (RwUInt8*)inAnim1 + 0x4C + inAnim1->currentInterpKeyFrameSize * i,
            (RwUInt8*)inAnim2 + 0x4C + inAnim2->currentInterpKeyFrameSize * i,
            alpha);
    }
    return TRUE;
}


RwBool RtAnimInterpolatorAddTogether(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim1, RtAnimInterpolator* inAnim2) {
    for (RwInt32 i = 0; i < outAnim->numNodes; i++) {
        outAnim->keyFrameAddCB(
            (RwUInt8*)outAnim + 0x4C + outAnim->currentInterpKeyFrameSize * i,   // rtANIMGETINTERPFRAME(outAnim, i)
            (RwUInt8*)inAnim1 + 0x4C + inAnim1->currentInterpKeyFrameSize * i,
            (RwUInt8*)inAnim2 + 0x4C + inAnim2->currentInterpKeyFrameSize * i);
    }
    return TRUE;
}


RtAnimInterpolator* RtAnimInterpolatorCreateSubInterpolator(RtAnimInterpolator* parentAnim, RwInt32 startNode, RwInt32 numNodes, RwInt32 maxInterpKeyFrameSize) {
    if (maxInterpKeyFrameSize == -1) {
        maxInterpKeyFrameSize = parentAnim->maxInterpKeyFrameSize;  // parentAnim+0x20
    }
    RtAnimInterpolator* sub = RtAnimInterpolatorCreate(numNodes, maxInterpKeyFrameSize);
    sub->parentAnimation   = parentAnim;  // sub+0x38
    sub->offsetInParent    = startNode;   // sub+0x34
    sub->isSubInterpolator = TRUE;        // sub+0x30
    return sub;
}


RwBool RtAnimInterpolatorBlendSubInterpolator(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim1, RtAnimInterpolator* inAnim2, RwReal alpha) {
    if (outAnim->isSubInterpolator == 1) {
        RwInt32 outStart = outAnim->offsetInParent;   // [0x34]
        RwInt32 in1Start = inAnim1->offsetInParent;
        if (outStart < outAnim->numNodes + outStart) {
            RwInt32 in2Start = inAnim2->offsetInParent;
            RwInt32 in1Idx = outStart - in1Start;
            RwInt32 j = in1Start - outStart;  // == -in1Idx, so (j + in1Idx) = 0, 1, 2, ...
            do {
                outAnim->keyFrameBlendCB(
                    ((RwUInt8*)outAnim + 0x4C) + (j + in1Idx) * outAnim->currentInterpKeyFrameSize,
                    ((RwUInt8*)inAnim1 + 0x4C) + in1Idx * inAnim1->currentInterpKeyFrameSize,
                    ((RwUInt8*)inAnim2 + 0x4C) + (in1Start - in2Start + in1Idx) * inAnim2->currentInterpKeyFrameSize,
                    alpha);
                outStart++;
                in1Idx++;
            } while (outStart < outAnim->numNodes + outAnim->offsetInParent);
            return TRUE;
        }
    } else {
        if (inAnim1->isSubInterpolator == 1) {
            // head: fill out frames before in1's range from in2
            memcpy((RwUInt8*)outAnim + 0x4C, (RwUInt8*)inAnim2 + 0x4C,
                   inAnim1->offsetInParent * outAnim->currentInterpKeyFrameSize);
            RwInt32 in1Start = inAnim1->offsetInParent;
            RwInt32 j = in1Start;
            if (in1Start < inAnim1->numNodes + in1Start) {
                do {
                    outAnim->keyFrameBlendCB(
                        ((RwUInt8*)outAnim + 0x4C) + j * outAnim->currentInterpKeyFrameSize,
                        ((RwUInt8*)inAnim1 + 0x4C) + (j - in1Start) * inAnim1->currentInterpKeyFrameSize,
                        ((RwUInt8*)inAnim2 + 0x4C) + j * inAnim2->currentInterpKeyFrameSize,
                        alpha);
                    in1Start = inAnim1->offsetInParent;  // re-read, as in the binary
                    j++;
                } while (j < inAnim1->numNodes + in1Start);
            }
            // tail: fill out frames after in1's range from in2
            RwInt32 in1End = inAnim1->offsetInParent + inAnim1->numNodes;
            memcpy(((RwUInt8*)outAnim + 0x4C) + in1End * outAnim->currentInterpKeyFrameSize,
                   ((RwUInt8*)inAnim2 + 0x4C) + in1End * inAnim2->currentInterpKeyFrameSize,
                   ((outAnim->numNodes - inAnim1->numNodes) - inAnim1->offsetInParent) * outAnim->currentInterpKeyFrameSize);
            return TRUE;
        }

        // in2 is a sub-interpolator (or plain blend, offsetInParent == 0)
        // head: fill out frames before in2's range from in1
        memcpy((RwUInt8*)outAnim + 0x4C, (RwUInt8*)inAnim1 + 0x4C,
               outAnim->currentInterpKeyFrameSize * inAnim2->offsetInParent);
        RwInt32 in2Start = inAnim2->offsetInParent;
        RwInt32 j = in2Start;
        if (in2Start < inAnim2->numNodes + in2Start) {
            do {
                outAnim->keyFrameBlendCB(
                    ((RwUInt8*)outAnim + 0x4C) + j * outAnim->currentInterpKeyFrameSize,
                    ((RwUInt8*)inAnim1 + 0x4C) + j * inAnim1->currentInterpKeyFrameSize,
                    ((RwUInt8*)inAnim2 + 0x4C) + (j - in2Start) * inAnim2->currentInterpKeyFrameSize,
                    alpha);
                j++;
                in2Start = inAnim2->offsetInParent;  // re-read, as in the binary
            } while (j < inAnim2->numNodes + in2Start);
        }
        // tail: fill out frames after in2's range from in1
        in2Start = inAnim2->numNodes + inAnim2->offsetInParent;
        memcpy(((RwUInt8*)outAnim + 0x4C) + in2Start * outAnim->currentInterpKeyFrameSize,
               ((RwUInt8*)inAnim1 + 0x4C) + in2Start * inAnim1->currentInterpKeyFrameSize,
               ((outAnim->numNodes - inAnim2->numNodes) - inAnim2->offsetInParent) * outAnim->currentInterpKeyFrameSize);
    }
    return TRUE;
}


RwBool RtAnimInterpolatorAddSubInterpolator(RtAnimInterpolator* outAnim,
                                            RtAnimInterpolator* mainAnim,
                                            RtAnimInterpolator* subAnim)
{
    RwInt32 offset = 0;
    RwInt32 i;

    if (outAnim->isSubInterpolator == 0)
    {
        offset = subAnim->offsetInParent;
    }

    for (i = 0; i < outAnim->numNodes; i++)
    {
        outAnim->keyFrameAddCB(rtANIMGETINTERPFRAME(outAnim, offset + i),
                               rtANIMGETINTERPFRAME(mainAnim, subAnim->offsetInParent + i),
                               rtANIMGETINTERPFRAME(subAnim, i));
    }

    return TRUE;
}


void RtAnim::InjectHooks() {
    RH_ScopedNamespaceName("RtAnim");
    RH_ScopedCategory("Plugins");

    RH_ScopedGlobalInstall(RtAnimInterpolatorSetCurrentAnim, 0x7CD5A0);
}
