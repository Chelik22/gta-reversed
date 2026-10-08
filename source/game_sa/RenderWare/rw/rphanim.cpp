#include "StdInc.h"

#include "rphanim.h"


// ==== RW-CAMPAIGN wave-A shared decls (reconstructed from agent notes; offsets decompile-verified) ====
// Binary RwGlobals layout (GTA RW 3.6): repo rwplcore.h is MISALIGNED (RwStringFunctions
// has more fn ptrs in this build), so memory section sits at +0x134, freelist fns +0x144/+0x148.
// 0x7C4C30 uses +0x134 (RwMalloc) and +0x144 (RwFreeListAlloc);
// 0x7C4D30 uses +0x138 (RwFree) and +0x148 (RwFreeListFree);
// 0x7C51D0 uses +0xBC (dirtyFrameList).
struct RwGlobalsGta
{
    RwUInt8     pad0[0xBC];                                                // +0x000
    RwLinkList  dirtyFrameList;                                            // +0x0BC
    RwUInt8     pad1[0x134 - 0xBC - sizeof(RwLinkList)];                   // +0x0C4..0x133
    void*       (*rwmalloc)(RwUInt32 size, RwUInt32 hint);                 // +0x134
    void        (*rwfree)(void* mem);                                      // +0x138
    RwUInt8     pad2[0x144 - 0x13C];                                       // +0x13C..0x143
    void*       (*freelistAlloc)(RwFreeList* freelist, RwUInt32 hint);     // +0x144
    void        (*freelistFree)(RwFreeList* freelist, void* mem);          // +0x148
};
static_assert(offsetof(RwGlobalsGta, dirtyFrameList) == 0xBC, "RwGlobalsGta layout");
static_assert(offsetof(RwGlobalsGta, rwmalloc)        == 0x134, "RwGlobalsGta layout");
static_assert(offsetof(RwGlobalsGta, rwfree)          == 0x138, "RwGlobalsGta layout");
static_assert(offsetof(RwGlobalsGta, freelistAlloc)   == 0x144, "RwGlobalsGta layout");
static_assert(offsetof(RwGlobalsGta, freelistFree)    == 0x148, "RwGlobalsGta layout");
static RwGlobalsGta& RwGlobalsGtaGet()
{
    return *reinterpret_cast<RwGlobalsGta*>(RwEngineInstance);
}

void RpHAnimHierarchySetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc)
{
    StaticRef<RwInt32>(0x8DECD8) = blockSize;
    StaticRef<RwInt32>(0x8DECDC) = numBlocksToPrealloc;
}


#include <cstddef>

// Binary layout of RwGlobals in the GTA RW 3.6 build. Offsets verified against decompiles:
// 0x7C4C30 uses +0x134 (RwMalloc) and +0x144 (RwFreeListAlloc);
// 0x7C4D30 uses +0x138 (RwFree) and +0x148 (RwFreeListFree);
// 0x7C51D0 uses +0xBC (dirtyFrameList head, RwLinkList insert).
// curCamera..stdFunc..dirtyFrameList match repo rwplcore.h RwGlobals; the delta is RwStringFunctions
// (28 pointers here vs 17 in the header), which shifts the memory section to +0x134.

RpHAnimHierarchy* RpHAnimHierarchyCreate(RwInt32 numNodes, RwUInt32* nodeFlags, RwInt32* nodeIDs, RpHAnimHierarchyFlag flags, RwInt32 maxInterpKeyFrameSize)
{
    RwGlobalsGta&            globals = RwGlobalsGtaGet();
    RpHAnimAtomicGlobalVars& atomics = StaticRef<RpHAnimAtomicGlobalVars>(0xC9B8C8);

    RpHAnimHierarchy* hierarchy = (RpHAnimHierarchy*)globals.freelistAlloc(atomics.HAnimFreeList, 0x3011E);

    hierarchy->currentAnim = RtAnimInterpolatorCreate(numNodes, maxInterpKeyFrameSize);
    hierarchy->flags       = flags;
    hierarchy->numNodes    = numNodes;
    hierarchy->parentFrame = NULL;

    if ((flags & rpHANIMHIERARCHYNOMATRICES) == 0)
    {
        hierarchy->pMatrixArrayUnaligned = globals.rwmalloc(numNodes * 0x40 + 0xF, 0x3011E);
        hierarchy->pMatrixArray = (RwMatrix*)(((uintptr_t)hierarchy->pMatrixArrayUnaligned + 0xF) & ~((uintptr_t)0xF));
    }
    else
    {
        hierarchy->pMatrixArray          = NULL;
        hierarchy->pMatrixArrayUnaligned = NULL;
    }

    hierarchy->pNodeInfo = (RpHAnimNodeInfo*)globals.rwmalloc(numNodes << 4, 0x3011E);

    for (RwInt32 i = 0; i < numNodes; i++)
    {
        hierarchy->pNodeInfo[i].pFrame    = NULL;
        hierarchy->pNodeInfo[i].nodeID    = nodeIDs ? nodeIDs[i] : 0;
        hierarchy->pNodeInfo[i].nodeIndex = i;
        hierarchy->pNodeInfo[i].flags     = nodeFlags ? nodeFlags[i] : 0;
    }

    hierarchy->parentHierarchy = hierarchy;
    return hierarchy;
}


RpHAnimHierarchy* RpHAnimHierarchyCreateFromHierarchy(RpHAnimHierarchy* hierarchy, RpHAnimHierarchyFlag flags, RwInt32 maxInterpKeyFrameSize)
{
    RpHAnimHierarchy* newHierarchy = RpHAnimHierarchyCreate(hierarchy->numNodes, NULL, NULL, flags, maxInterpKeyFrameSize);

    for (RwInt32 i = 0; i < newHierarchy->numNodes; i++)
    {
        newHierarchy->pNodeInfo[i].pFrame    = NULL;
        newHierarchy->pNodeInfo[i].flags     = hierarchy->pNodeInfo[i].flags;
        newHierarchy->pNodeInfo[i].nodeIndex = hierarchy->pNodeInfo[i].nodeIndex;
        newHierarchy->pNodeInfo[i].nodeID    = hierarchy->pNodeInfo[i].nodeID;
    }

    return newHierarchy;
}


RpHAnimHierarchy* RpHAnimHierarchyDestroy(RpHAnimHierarchy* hierarchy)
{
    RwGlobalsGta&            globals = RwGlobalsGtaGet();
    RpHAnimAtomicGlobalVars& atomics = StaticRef<RpHAnimAtomicGlobalVars>(0xC9B8C8);

    RwFrame* parentFrame = hierarchy->parentFrame;

    if ((hierarchy->flags & rpHANIMHIERARCHYSUBHIERARCHY) == 0)
    {
        if ((hierarchy->flags & rpHANIMHIERARCHYNOMATRICES) == 0)
        {
            globals.rwfree(hierarchy->pMatrixArrayUnaligned);
        }
        globals.rwfree(hierarchy->pNodeInfo);
    }

    hierarchy->pMatrixArrayUnaligned = NULL;
    hierarchy->pMatrixArray          = NULL;
    hierarchy->pNodeInfo             = NULL;

    RtAnimInterpolatorDestroy(hierarchy->currentAnim);

    globals.freelistFree(atomics.HAnimFreeList, hierarchy);

    if (parentFrame)
    {
        RWPLUGINOFFSET(RpHAnimFrameExtension, parentFrame, atomics.engineOffset)->hierarchy = NULL;
    }

    return NULL;
}


RpHAnimHierarchy* RpHAnimHierarchyCreateSubHierarchy(RpHAnimHierarchy* parentHierarchy, RwInt32 startNode, RpHAnimHierarchyFlag flags, RwInt32 maxInterpKeyFrameSize)
{
    RwGlobalsGta&            globals = RwGlobalsGtaGet();
    RpHAnimAtomicGlobalVars& atomics = StaticRef<RpHAnimAtomicGlobalVars>(0xC9B8C8);

    /* Count the nodes of the sub hierarchy (decompile iVar7/uVar8 walk) */
    RwUInt32 numNodes = 1;
    if ((parentHierarchy->pNodeInfo[startNode].flags & rpHANIMPOPPARENTMATRIX) == 0)
    {
        RwInt32 depth = 1;
        for (const RpHAnimNodeInfo* node = &parentHierarchy->pNodeInfo[startNode + 1]; ; node++)
        {
            if (node->flags & rpHANIMPUSHPARENTMATRIX)  depth++;
            if (node->flags & rpHANIMPOPPARENTMATRIX)   depth--;
            numNodes++;
            if (depth == 0)
                break;
        }
    }

    RpHAnimHierarchy* hierarchy = (RpHAnimHierarchy*)globals.freelistAlloc(atomics.HAnimFreeList, 0x3011E);
    hierarchy->flags       = flags | rpHANIMHIERARCHYSUBHIERARCHY;
    hierarchy->numNodes    = numNodes;
    hierarchy->currentAnim = RtAnimInterpolatorCreateSubInterpolator(parentHierarchy->currentAnim, startNode, numNodes, maxInterpKeyFrameSize);

    /* rootParentOffset: parent index inside the parent hierarchy */
    if (startNode == 0)
    {
        hierarchy->rootParentOffset = -1;
    }
    else
    {
        RwInt32   parentIdx = -1;
        RwInt32   i         = 0;
        RwInt32   stack[32]; /* decompile auStack_80[32] */
        RwInt32*  sp        = stack;

        for (const RpHAnimNodeInfo* node = &parentHierarchy->pNodeInfo[0]; i < startNode; i++, node++)
        {
            if (node->flags & rpHANIMPUSHPARENTMATRIX)
            {
                *sp++ = parentIdx;
            }
            parentIdx = i;
            if (node->flags & rpHANIMPOPPARENTMATRIX)
            {
                parentIdx = *--sp;
            }
        }
        hierarchy->rootParentOffset = parentIdx;
    }

    hierarchy->parentFrame          = parentHierarchy->pNodeInfo[startNode].pFrame;
    hierarchy->pMatrixArrayUnaligned = NULL;
    hierarchy->pMatrixArray          = (RwMatrix*)((RwUInt8*)parentHierarchy->pMatrixArray + startNode * 0x40);
    hierarchy->parentHierarchy      = parentHierarchy;
    hierarchy->pNodeInfo            = parentHierarchy->pNodeInfo + startNode;

    return hierarchy;
}


RpHAnimHierarchy* RpHAnimHierarchyAttach(RpHAnimHierarchy* hierarchy)
{
    RpHAnimAtomicGlobalVars& atomics = StaticRef<RpHAnimAtomicGlobalVars>(0xC9B8C8);

    RwFrame*      parentFrame = hierarchy->parentFrame;
    const RwInt32 frameID     = RWPLUGINOFFSET(RpHAnimFrameExtension, parentFrame, atomics.engineOffset)->id;

    for (RwInt32 i = 0; i < hierarchy->numNodes; i++)
    {
        if (hierarchy->pNodeInfo[i].nodeID == frameID)
        {
            hierarchy->pNodeInfo[i].pFrame = parentFrame;
        }
    }

    RwFrameUpdateObjects(parentFrame);
    RwFrameForAllChildren(parentFrame, (RwFrameCallBack)0x7C4F90, hierarchy);

    return hierarchy;
}


RpHAnimHierarchy* RpHAnimHierarchyDetach(RpHAnimHierarchy* hierarchy)
{
    for (RwInt32 i = 0; i < hierarchy->numNodes; i++)
    {
        hierarchy->pNodeInfo[i].pFrame = NULL;
    }

    return hierarchy;
}


RpHAnimHierarchy* RpHAnimHierarchyAttachFrameIndex(RpHAnimHierarchy* hierarchy, RwInt32 nodeIndex)
{
    RpHAnimAtomicGlobalVars& atomics = StaticRef<RpHAnimAtomicGlobalVars>(0xC9B8C8);

    RwFrame*      parentFrame = hierarchy->parentFrame;
    const RwInt32 nodeID      = hierarchy->pNodeInfo[nodeIndex].nodeID;

    if (RWPLUGINOFFSET(RpHAnimFrameExtension, parentFrame, atomics.engineOffset)->id == nodeID)
    {
        for (RwInt32 i = 0; i < hierarchy->numNodes; i++)
        {
            if (hierarchy->pNodeInfo[i].nodeID == nodeID)
            {
                hierarchy->pNodeInfo[i].pFrame = parentFrame;
            }
        }
    }

    RwFrameUpdateObjects(parentFrame);

    struct attachFrameIndexData_t   /* layout == decompile iStack_8/iStack_4 */
    {
        RpHAnimHierarchy* hierarchy;
        RwInt32           nodeID;
    } data = { hierarchy, nodeID };

    RwFrameForAllChildren(parentFrame, (RwFrameCallBack)0x7C50A0, &data);

    return hierarchy;
}


RpHAnimHierarchy* RpHAnimHierarchyDetachFrameIndex(RpHAnimHierarchy* hierarchy, RwInt32 nodeIndex)
{
    hierarchy->pNodeInfo[nodeIndex].pFrame = NULL;

    return hierarchy;
}


RwBool RpHAnimFrameSetHierarchy(RwFrame* frame, RpHAnimHierarchy* hierarchy)
{
    RpHAnimAtomicGlobalVars& atomics = StaticRef<RpHAnimAtomicGlobalVars>(0xC9B8C8);

    RpHAnimFrameExtension* extension = RWPLUGINOFFSET(RpHAnimFrameExtension, frame, atomics.engineOffset);

    if (extension->hierarchy)
    {
        extension->hierarchy->parentFrame = NULL;
    }

    extension->hierarchy = hierarchy;

    if (hierarchy)
    {
        hierarchy->parentFrame = frame;
    }

    return TRUE;
}


RpHAnimHierarchy* RpHAnimFrameGetHierarchy(RwFrame* frame)
{
    RpHAnimAtomicGlobalVars& atomics = StaticRef<RpHAnimAtomicGlobalVars>(0xC9B8C8);

    return RWPLUGINOFFSET(RpHAnimFrameExtension, frame, atomics.engineOffset)->hierarchy;
}


RwMatrix* RpHAnimHierarchyGetMatrixArray(RpHAnimHierarchy* hierarchy)
{
    return hierarchy->pMatrixArray;
}


// NOTSA
RwMatrix* RpHAnimHierarchyGetNodeMatrix(RpHAnimHierarchy* hierarchy, RwInt32 nodeID) {
    const auto i = RpHAnimIDGetIndex(hierarchy, nodeID);
    assert(i >= 0 && i < hierarchy->numNodes);
    return &RpHAnimHierarchyGetMatrixArray(hierarchy)[i];
}

RwBool RpHAnimHierarchyUpdateMatrices(RpHAnimHierarchy* hierarchy)
{
    RwGlobalsGta& globals = RwGlobalsGtaGet();

    const RwUInt32 hierFlags  = hierarchy->flags;
    const RwBool   updateLTMs = (hierFlags & rpHANIMHIERARCHYUPDATELTMS) != 0;

    if (hierFlags & rpHANIMHIERARCHYNOMATRICES)
    {
        /* ---- Path A: no local matrices ---- */
        RwMatrix  rootMat;                              /* decompile local_980 */
        RwMatrix  animMat;                              /* decompile local_9d0 */
        RwMatrix  combinedMat;                          /* decompile local_8c0 */
        RwMatrix  matrixStack[32];                      /* decompile local_7c0 area, whole matrices */
        RwMatrix* sp = matrixStack;

        /* Root matrix from parentFrame's parent (inlined block #1) */
        {
            RwFrame* parentFrame = hierarchy->parentFrame;
            if (parentFrame == NULL)
            {
                RwMatrixSetIdentityMacro(&rootMat);
            }
            else
            {
                RwFrame* parent = RwFrameGetParent(parentFrame);
                if (parent == NULL)
                {
                    RwMatrixSetIdentityMacro(&rootMat);
                }
                else if (!RwFrameDirty(parent))
                {
                    rootMat = *RwFrameGetLTM(parent);
                }
                else
                {
                    rootMat = parent->modelling;
                    for (RwFrame* frame = RwFrameGetParent(parent); frame; frame = RwFrameGetParent(frame))
                    {
                        RwMatrixMultiply(&rootMat, &rootMat, &frame->modelling);
                    }
                }
            }
        }

        if (hierarchy->numNodes < 1)
        {
            return TRUE;
        }

        RtAnimInterpolator* const interp     = hierarchy->currentAnim;
        const RwInt32             frameSize  = interp->currentInterpKeyFrameSize;
        RwUInt8*                  iFrame     = (RwUInt8*)rtANIMGETINTERPFRAME(interp, 0);
        const RpHAnimNodeInfo*    node       = hierarchy->pNodeInfo;

        for (RwInt32 i = 0; i < hierarchy->numNodes; i++, node++, iFrame += frameSize)
        {
            if (interp->keyFrameApplyCB == &RpHAnimKeyFrameApply)
            {
                RpHAnimKeyFrameApply(&animMat, iFrame);
            }
            else
            {
                interp->keyFrameApplyCB(&animMat, iFrame);
            }

            RwMatrixMultiply(&combinedMat, &animMat, &rootMat);

            if (node->pFrame)
            {
                if (hierFlags & rpHANIMHIERARCHYUPDATEMODELLINGMATRICES)
                {
                    node->pFrame->modelling = animMat;

                    if (!updateLTMs)
                    {
                        RwFrameUpdateObjects(node->pFrame);
                    }
                    else
                    {
                        node->pFrame->ltm = combinedMat;
                        node->pFrame->object.privateFlags = (node->pFrame->object.privateFlags & ~rwFRAMEPRIVATESUBTREESYNCLTM) | rwFRAMEPRIVATESUBTREESYNCOBJ;
                    }
                }
                else if (updateLTMs)
                {
                    node->pFrame->ltm = combinedMat;
                    node->pFrame->object.privateFlags = (node->pFrame->object.privateFlags & ~rwFRAMEPRIVATESUBTREESYNCLTM) | rwFRAMEPRIVATESUBTREESYNCOBJ;
                }
            }

            switch (node->flags & (rpHANIMPOPPARENTMATRIX | rpHANIMPUSHPARENTMATRIX))
            {
            case rpHANIMPOPPARENTMATRIX:                /* 0x01 POP */
                rootMat = *--sp;
                break;
            case rpHANIMPUSHPARENTMATRIX:               /* 0x02 PUSH */
                *sp++ = rootMat;
                rootMat = combinedMat;
                break;
            default:                                    /* 0x03 impossible, 0x00 plain */
                rootMat = combinedMat;
                break;
            }
        }

        return TRUE;
    }

    /* ---- Path B: matrices land in hierarchy->pMatrixArray ---- */
    RwMatrix  rootMat;                                  /* decompile local_940 - root LTM for LOCALSPACEMATRICES */
    RwMatrix  identityRoot;                             /* decompile local_900 */
    RwMatrix  animMat;                                  /* decompile local_9d0 */
    RwMatrix* matrixStackB[48];                         /* decompile local_880 - stack of RwMatrix* */
    RwMatrix* parentMat;                                /* decompile local_a14 */
    RwFrame*  rootFrame;                                /* decompile uVar14 (path-B reuse) */
    bool      buildRootLTM;

    if ((hierFlags & rpHANIMHIERARCHYSUBHIERARCHY) == 0 || hierarchy->rootParentOffset == -1)
    {
        if ((hierFlags & rpHANIMHIERARCHYLOCALSPACEMATRICES) == 0)
        {
            parentMat     = &rootMat;
            rootFrame     = hierarchy->parentFrame;
            buildRootLTM  = true;
        }
        else
        {
            parentMat = &identityRoot;
            RwMatrixSetIdentityMacro(&identityRoot);
            rootFrame    = hierarchy->parentFrame;
            buildRootLTM = updateLTMs;
        }
    }
    else
    {
        parentMat    = &hierarchy->parentHierarchy->pMatrixArray[hierarchy->rootParentOffset];
        rootFrame    = hierarchy->parentHierarchy->parentFrame;
        buildRootLTM = updateLTMs && (hierFlags & rpHANIMHIERARCHYLOCALSPACEMATRICES) != 0;
    }

    if (buildRootLTM)
    {
        /* Root matrix from rootFrame's parent (inlined block #2, same as #1) */
        RwFrame* parentFrame = rootFrame;
        if (parentFrame == NULL)
        {
            RwMatrixSetIdentityMacro(&rootMat);
        }
        else
        {
            RwFrame* parent = RwFrameGetParent(parentFrame);
            if (parent == NULL)
            {
                RwMatrixSetIdentityMacro(&rootMat);
            }
            else if (!RwFrameDirty(parent))
            {
                rootMat = *RwFrameGetLTM(parent);
            }
            else
            {
                rootMat = parent->modelling;
                for (RwFrame* frame = RwFrameGetParent(parent); frame; frame = RwFrameGetParent(frame))
                {
                    RwMatrixMultiply(&rootMat, &rootMat, &frame->modelling);
                }
            }
        }
    }

    if (updateLTMs)
    {
        /* Decompile LAB_007c5759: queue the root frame into the dirty frame list */
        RwFrame* root = hierarchy->parentFrame->root;
        if ((root->object.privateFlags & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) == 0)
        {
            rwLinkListAddLLLink(&globals.dirtyFrameList, &root->inDirtyListLink);
            root->object.privateFlags |= rwFRAMEPRIVATEHIERARCHYSYNCOBJ;
        }
    }

    RtAnimInterpolator* const interp    = hierarchy->currentAnim;
    const RwInt32             frameSize = interp->currentInterpKeyFrameSize;
    RwUInt8*                  iFrame    = (RwUInt8*)rtANIMGETINTERPFRAME(interp, 0);
    const RpHAnimNodeInfo*    node      = hierarchy->pNodeInfo;
    RwMatrix*                 mat       = hierarchy->pMatrixArray;
    RwMatrix**               sp        = &matrixStackB[1];   /* decompile local_a0c = local_880 + 1 */

    for (RwInt32 i = 0; i < hierarchy->numNodes; i++, node++, iFrame += frameSize, mat++)
    {
        if (interp->keyFrameApplyCB == &RpHAnimKeyFrameApply)
        {
            RpHAnimKeyFrameApply(&animMat, iFrame);
        }
        else
        {
            interp->keyFrameApplyCB(&animMat, iFrame);
        }

        RwMatrixMultiply(mat, &animMat, parentMat);

        if (node->pFrame)
        {
            if (hierFlags & rpHANIMHIERARCHYUPDATEMODELLINGMATRICES)
            {
                node->pFrame->modelling = animMat;

                if (!updateLTMs)
                {
                    RwFrameUpdateObjects(node->pFrame);
                }
                else
                {
                    /* LAB_007c5aba */
                    if ((hierFlags & rpHANIMHIERARCHYLOCALSPACEMATRICES) == 0)
                    {
                        node->pFrame->ltm = *mat;
                    }
                    else
                    {
                        RwMatrixMultiply(&node->pFrame->ltm, mat, &rootMat);
                    }
                    node->pFrame->object.privateFlags = (node->pFrame->object.privateFlags & ~rwFRAMEPRIVATESUBTREESYNCLTM) | rwFRAMEPRIVATESUBTREESYNCOBJ;
                }
            }
            else if (updateLTMs)
            {
                /* LAB_007c5aba */
                if ((hierFlags & rpHANIMHIERARCHYLOCALSPACEMATRICES) == 0)
                {
                    node->pFrame->ltm = *mat;
                }
                else
                {
                    RwMatrixMultiply(&node->pFrame->ltm, mat, &rootMat);
                }
                node->pFrame->object.privateFlags = (node->pFrame->object.privateFlags & ~rwFRAMEPRIVATESUBTREESYNCLTM) | rwFRAMEPRIVATESUBTREESYNCOBJ;
            }
        }

        switch (node->flags & (rpHANIMPOPPARENTMATRIX | rpHANIMPUSHPARENTMATRIX))
        {
        case rpHANIMPOPPARENTMATRIX:                /* 0x01 POP */
            parentMat = *--sp;
            break;
        case rpHANIMPUSHPARENTMATRIX:               /* 0x02 PUSH */
            *sp++ = parentMat;
            parentMat = mat;
            break;
        default:                                    /* 0x00 plain child */
            parentMat = mat;
            break;
        }
    }

    return TRUE;
}


RwInt32 RpHAnimIDGetIndex(RpHAnimHierarchy* hierarchy, RwInt32 ID)
{
    for (RwInt32 i = 0; i < hierarchy->numNodes; i++)
    {
        if (hierarchy->pNodeInfo[i].nodeID == ID)
        {
            return i;
        }
    }

    return -1;
}


RwBool RpHAnimPluginAttach()
{
    RpHAnimAtomicGlobalVars& atomics = StaticRef<RpHAnimAtomicGlobalVars>(0xC9B8C8);

    if (RwEngineRegisterPlugin(0, 0x11E /* rwID_HANIMPLUGIN (chunk id as in decompile) */,
                               (RwPluginObjectConstructor)0x7C4670,
                               (RwPluginObjectDestructor)0x7C4720) < 0)
    {
        return FALSE;
    }

    atomics.engineOffset = RwFrameRegisterPlugin(sizeof(RpHAnimFrameExtension), 0x11E,
                                                 (RwPluginObjectConstructor)0x7C4750,
                                                 (RwPluginObjectDestructor)0x7C4770,
                                                 (RwPluginObjectCopy)0x7C4830);

    if (RwFrameRegisterPluginStream(0x11E,
                                    (RwPluginDataChunkReadCallBack)0x7C4A00,
                                    (RwPluginDataChunkWriteCallBack)0x7C48D0,
                                    (RwPluginDataChunkGetSizeCallBack)0x7C4BF0) < 0)
    {
        return FALSE;
    }

    if (atomics.engineOffset < 0)
    {
        return FALSE;
    }

    return TRUE;
}


void RpHAnimKeyFrameApply(void* matrix, void* voidIFrame)
{
    RwMatrix* const          m      = (RwMatrix*)matrix;
    const RpHAnimInterpFrame* iFrame = (const RpHAnimInterpFrame*)voidIFrame;

    const float qx = iFrame->q.imag.x;
    const float qy = iFrame->q.imag.y;
    const float qz = iFrame->q.imag.z;
    const float qw = iFrame->q.real;
    float       t;

    t = qz * qz + qy * qy;
    m->right.x = 1.0f - (t + t);
    t = qy * qx + qw * qz;
    m->right.y = t + t;
    t = qz * qx - qw * qy;
    m->right.z = t + t;

    t = qy * qx - qw * qz;
    m->up.x = t + t;
    t = qz * qz + qx * qx;
    m->up.y = 1.0f - (t + t);
    t = qw * qx + qz * qy;
    m->up.z = t + t;

    t = qw * qy + qz * qx;
    m->at.x = t + t;
    t = qz * qy - qw * qx;
    m->at.y = t + t;
    t = qy * qy + qx * qx;
    m->at.z = 1.0f - (t + t);

    m->flags = rwMATRIXTYPEORTHONORMAL;   /* decompile writes raw 3 */

    m->pos.x = iFrame->t.x;
    m->pos.y = iFrame->t.y;
    m->pos.z = iFrame->t.z;
}


void RpHAnimKeyFrameBlend(void* voidOut, void* voidIn1, void* voidIn2, RwReal alpha)
{
    const RpHAnimKeyFrame* in1 = (const RpHAnimKeyFrame*)voidIn1;
    RpHAnimKeyFrame*       in2 = (RpHAnimKeyFrame*)voidIn2;
    RpHAnimInterpFrame*    out = (RpHAnimInterpFrame*)voidOut;

    out->t.x = (in2->t.x - in1->t.x) * alpha + in1->t.x;
    out->t.y = (in2->t.y - in1->t.y) * alpha + in1->t.y;
    out->t.z = (in2->t.z - in1->t.z) * alpha + in1->t.z;

    RwReal dp = in2->q.real * in1->q.real
              + in2->q.imag.z * in1->q.imag.z
              + in2->q.imag.x * in1->q.imag.x
              + in2->q.imag.y * in1->q.imag.y;

    if (dp < 0.0f)
    {
        dp = -dp;
        in2->q.imag.x = -in2->q.imag.x;   /* mutates the keyframe - as in the original */
        in2->q.imag.y = -in2->q.imag.y;
        in2->q.imag.z = -in2->q.imag.z;
        in2->q.real = -in2->q.real;
    }

    RwReal scale1 = 1.0f - alpha;
    RwReal scale2 = alpha;

    if (dp < 0.999f)
    {
        const RwReal theta    = acosf(dp);
        const RwReal sinTheta = sinf(theta);
        scale1 = sinf((1.0f - alpha) * theta) / sinTheta;
        scale2 = sinf(alpha * theta) / sinTheta;
    }

    out->q.imag.x = scale1 * in1->q.imag.x + scale2 * in2->q.imag.x;
    out->q.imag.y = in1->q.imag.y * scale1 + in2->q.imag.y * scale2;
    out->q.imag.z = scale1 * in1->q.imag.z + scale2 * in2->q.imag.z;
    out->q.real = scale1 * in1->q.real + scale2 * in2->q.real;
}


void RpHAnimKeyFrameInterpolate(void* voidOut, void* voidIn1, void* voidIn2, RwReal time, void* customData)
{
    const RpHAnimKeyFrame*  in1 = (const RpHAnimKeyFrame*)voidIn1;
    RpHAnimKeyFrame*        in2 = (RpHAnimKeyFrame*)voidIn2;
    RpHAnimInterpFrame*     out = (RpHAnimInterpFrame*)voidOut;

    const RwReal alpha = (time - in1->time) / (in2->time - in1->time);

    out->t.x = (in2->t.x - in1->t.x) * alpha + in1->t.x;
    out->t.y = (in2->t.y - in1->t.y) * alpha + in1->t.y;
    out->t.z = (in2->t.z - in1->t.z) * alpha + in1->t.z;

    RwReal dp = in2->q.real * in1->q.real
              + in2->q.imag.z * in1->q.imag.z
              + in1->q.imag.y * in2->q.imag.y
              + in2->q.imag.x * in1->q.imag.x;

    if (dp < 0.0f)
    {
        dp = -dp;
        in2->q.imag.x = -in2->q.imag.x;   /* mutates the keyframe - as in the original */
        in2->q.imag.y = -in2->q.imag.y;
        in2->q.imag.z = -in2->q.imag.z;
        in2->q.real = -in2->q.real;
    }

    RwReal scale1 = 1.0f - alpha;
    RwReal scale2 = alpha;

    if (dp < 0.999f)
    {
        const RwReal theta    = acosf(dp);
        const RwReal sinTheta = sinf(theta);
        scale1 = sinf((1.0f - alpha) * theta) / sinTheta;
        scale2 = sinf(alpha * theta) / sinTheta;
    }

    out->q.imag.x = scale1 * in1->q.imag.x + scale2 * in2->q.imag.x;   /* decompile: scale1*x1 + scale2*x2 */
    out->q.imag.y = scale2 * in2->q.imag.y + scale1 * in1->q.imag.y;   /* decompile: scale2*y2 + scale1*y1 */
    out->q.imag.z = scale1 * in1->q.imag.z + scale2 * in2->q.imag.z;
    out->q.real = scale2 * in2->q.real + scale1 * in1->q.real;
}


void RpHAnimKeyFrameAdd(void* voidOut, void* voidIn1, void* voidIn2)
{
    RpHAnimKeyFrame* const out = (RpHAnimKeyFrame*)voidOut;
    const RpHAnimKeyFrame* in1 = (const RpHAnimKeyFrame*)voidIn1;
    const RpHAnimKeyFrame* in2 = (const RpHAnimKeyFrame*)voidIn2;

    out->q.real = in2->q.real * in1->q.real
             - (in2->q.imag.z * in1->q.imag.z
             +  in2->q.imag.x * in1->q.imag.x
             +  in2->q.imag.y * in1->q.imag.y);

    out->q.imag.x = in1->q.imag.y * in2->q.imag.z - in2->q.imag.y * in1->q.imag.z;
    out->q.imag.y = in2->q.imag.x * in1->q.imag.z - in1->q.imag.x * in2->q.imag.z;
    out->q.imag.z = in2->q.imag.y * in1->q.imag.x - in2->q.imag.x * in1->q.imag.y;

    out->q.imag.x = in1->q.real * in2->q.imag.x + out->q.imag.x;
    out->q.imag.y = in1->q.real * in2->q.imag.y + out->q.imag.y;
    out->q.imag.z = in1->q.real * in2->q.imag.z + out->q.imag.z;

    out->q.imag.x = in2->q.real * in1->q.imag.x + out->q.imag.x;
    out->q.imag.y = in2->q.real * in1->q.imag.y + out->q.imag.y;
    out->q.imag.z = in2->q.real * in1->q.imag.z + out->q.imag.z;

    out->t.x = in1->t.x + in2->t.x;
    out->t.y = in1->t.y + in2->t.y;
    out->t.z = in1->t.z + in2->t.z;
}


void RpHAnimKeyFrameMulRecip(void* voidFrame, void* voidStart)
{
    RpHAnimKeyFrame* const frame = (RpHAnimKeyFrame*)voidFrame;
    RpHAnimKeyFrame* const start = (RpHAnimKeyFrame*)voidStart;

    const float norm = start->q.real * start->q.real
                     + start->q.imag.z * start->q.imag.z
                     + start->q.imag.y * start->q.imag.y
                     + start->q.imag.x * start->q.imag.x;

    float qw2, qx2, qy2, qz2;

    if (0.0f < norm)
    {
        float fInv = 1.0f / norm;
        qw2 = fInv * start->q.real;
        fInv = -fInv;
        qx2 = start->q.imag.x * fInv;
        qy2 = start->q.imag.y * fInv;
        qz2 = fInv * start->q.imag.z;
    }

    const float qx1 = frame->q.imag.x;
    const float qy1 = frame->q.imag.y;
    const float qz1 = frame->q.imag.z;
    const float qw1 = frame->q.real;

    frame->q.real = qw1 * qw2 - (qz1 * qz2 + qx1 * qx2 + qy1 * qy2);
    frame->q.imag.x = qw1 * qx2 + qw2 * qx1 + (qz1 * qy2 - qy1 * qz2);
    frame->q.imag.y = qy1 * qw2 + qw1 * qy2 + (qx1 * qz2 - qz1 * qx2);
    frame->q.imag.z = qz1 * qw2 + qw1 * qz2 + (qy1 * qx2 - qx1 * qy2);

    frame->t.x -= start->t.x;
    frame->t.y -= start->t.y;
    frame->t.z -= start->t.z;
}


RtAnimAnimation* RpHAnimKeyFrameStreamRead(RwStream* stream, RtAnimAnimation* animation)
{
    RpHAnimKeyFrame* const frames = (RpHAnimKeyFrame*)animation->pFrames;

    for (RwInt32 i = 0; i < animation->numFrames; i++)
    {
        RpHAnimKeyFrame* frame = &frames[i];

        if (RwStreamReadReal(stream, &frame->time, 0x20) == 0)
        {
            return NULL;
        }

        RwInt32 prevIndex;
        if (RwStreamReadInt32(stream, &prevIndex, 4) == 0)
        {
            return NULL;
        }

        frame->prevFrame = &frames[prevIndex / 0x24];   /* 0x24 == sizeof(RpHAnimKeyFrame) */
    }

    return animation;
}


RwBool RpHAnimKeyFrameStreamWrite(const RtAnimAnimation* animation, RwStream* stream)
{
    RpHAnimKeyFrame* const frames = (RpHAnimKeyFrame*)animation->pFrames;

    for (RwInt32 i = 0; i < animation->numFrames; i++)
    {
        const RpHAnimKeyFrame* frame = &frames[i];

        if (RwStreamWriteReal(stream, &frame->time, 0x20) == 0)
        {
            return FALSE;
        }

        RwInt32 prevOffset = (RwInt32)((RwUInt8*)frame->prevFrame - (RwUInt8*)frames);

        if (RwStreamWriteInt32(stream, &prevOffset, 4) == 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}


RwInt32 RpHAnimKeyFrameStreamGetSize(const RtAnimAnimation* animation)
{
    return animation->numFrames * 0x24;
}


RwBool RpHAnimFrameSetID(RwFrame* frame, RwInt32 id)
{
    RpHAnimAtomicGlobalVars& atomics = StaticRef<RpHAnimAtomicGlobalVars>(0xC9B8C8);

    RWPLUGINOFFSET(RpHAnimFrameExtension, frame, atomics.engineOffset)->id = id;

    return TRUE;
}


RwInt32 RpHAnimFrameGetID(RwFrame* frame)
{
    RpHAnimAtomicGlobalVars& atomics = StaticRef<RpHAnimAtomicGlobalVars>(0xC9B8C8);

    return RWPLUGINOFFSET(RpHAnimFrameExtension, frame, atomics.engineOffset)->id;
}

