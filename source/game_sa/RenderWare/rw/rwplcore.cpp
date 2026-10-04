#include "StdInc.h"

#include "rwplcore.h"

static void RwPlCoreReportError(RwUInt32 code, RwUInt32 arg);

RwMemoryFunctions* RwOsGetMemoryInterface() {
    return ((RwMemoryFunctions*(__cdecl *)(void))0x802230)();
}

/* 0x801980: Legacy retail RwFreeListCreate (rwplcore.h); passes
 * prealloc=1 / in-place space=NULL to the shared implementation.
 * (Previously bound here with the 6-arg RWDEBUG signature, which the
 * retail binary does not have.) */
RwFreeList* RwFreeListCreate(RwInt32 entrySize, RwInt32 entriesPerBlock, RwInt32 alignment, RwUInt32 hint) {
    return ((RwFreeList*(__cdecl*)(RwInt32, RwInt32, RwInt32, RwInt32, RwFreeList*, RwUInt32))0x801980)(entrySize, entriesPerBlock, alignment, 1, nullptr, hint);
}

RwFreeList* RwFreeListCreateAndPreallocateSpace(RwInt32 entrySize, RwInt32 entriesPerBlock, RwInt32 alignment, RwInt32 numBlocksToPreallocate, RwFreeList* inPlaceSpaceForFreeListStruct, RwUInt32 hint) {
    return ((RwFreeList*(__cdecl *)(RwInt32, RwInt32, RwInt32, RwInt32, RwFreeList*, RwUInt32))0x801B70)(entrySize, entriesPerBlock, alignment, numBlocksToPreallocate, inPlaceSpaceForFreeListStruct, hint);
}

RwBool RwFreeListDestroy(RwFreeList* freelist) {
    return ((RwBool(__cdecl *)(RwFreeList*))0x801B80)(freelist);
}

void RwFreeListSetFlags(RwFreeList* freeList, RwUInt32 flags) {
    ((void(__cdecl *)(RwFreeList*, RwUInt32))0x801C10)(freeList, flags);
}

RwUInt32 RwFreeListGetFlags(RwFreeList* freeList) {
    return ((RwUInt32(__cdecl *)(RwFreeList*))0x801C20)(freeList);
}

RwInt32 RwFreeListPurge(RwFreeList* freelist) {
    return ((RwInt32(__cdecl *)(RwFreeList*))0x801E00)(freelist);
}

RwFreeList* RwFreeListForAllUsed(RwFreeList* freelist, RwFreeListCallBack fpCallBack, void* data) {
    return ((RwFreeList*(__cdecl *)(RwFreeList*, RwFreeListCallBack, void*))0x801E90)(freelist, fpCallBack, data);
}

RwInt32 RwFreeListPurgeAllFreeLists() {
    return ((RwInt32(__cdecl *)(void))0x801F90)();
}

void RwStreamSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    ((void(__cdecl *)(RwInt32, RwInt32))0x7EC760)(blockSize, numBlocksToPrealloc);
}

RwStream* _rwStreamInitialize(RwStream* stream, RwBool rwOwned, RwStreamType type, RwStreamAccessType accessType, const void* data) {
    return ((RwStream*(__cdecl *)(RwStream*, RwBool, RwStreamType, RwStreamAccessType, const void*))0x7EC810)(stream, rwOwned, type, accessType, data);
}

RwStream* RwStreamOpen(RwStreamType type, RwStreamAccessType accessType, const void* data) {
    return ((RwStream*(__cdecl *)(RwStreamType, RwStreamAccessType, const void*))0x7ECEF0)(type, accessType, data);
}

RwBool RwStreamClose(RwStream* stream, void* data) {
    return ((RwBool(__cdecl *)(RwStream*, void*))0x7ECE20)(stream, data);
}

RwUInt32 RwStreamRead(RwStream* stream, void* buffer, RwUInt32 length) {
    return ((RwUInt32(__cdecl *)(RwStream*, void*, RwUInt32))0x7EC9D0)(stream, buffer, length);
}

RwStream* RwStreamWrite(RwStream* stream, const void* buffer, RwUInt32 length) {
    return ((RwStream*(__cdecl *)(RwStream*, const void*, RwUInt32))0x7ECB30)(stream, buffer, length);
}

RwStream* RwStreamSkip(RwStream* stream, RwUInt32 offset) {
    return ((RwStream*(__cdecl *)(RwStream*, RwUInt32))0x7ECD00)(stream, offset);
}

/* 0x80A240: Registers the engine's default string functions (CRT / RW internals) */
RwBool _rwStringOpen() {
    ZoneScoped;
    auto& sf = RwEngineInstance->stringFuncs;
    sf.vecSprintf = (vecSprintfFunc)0x821BB5;   /* CRT */
    sf.vecVsprintf = (vecVsprintfFunc)0x8230B8; /* CRT */
    sf.vecStrcpy = (vecStrcpyFunc)0x826590;     /* CRT */
    sf.vecStrncpy = (vecStrncpyFunc)0x821F40;   /* CRT */
    sf.vecStrcat = (vecStrcatFunc)0x8265A0;     /* CRT */
    sf.vecStrncat = (vecStrncatFunc)0x826450;   /* CRT */
    sf.vecStrrchr = (vecStrrchrFunc)0x80A420;   /* RW's own */
    sf.vecStrchr = (vecStrchrFunc)0x80A400;     /* RW's own */
    sf.vecStrstr = (vecStrstrFunc)0x822650;     /* CRT */
    sf.vecStrcmp = (vecStrcmpFunc)0x8263C0;     /* CRT */
    sf.vecStrncmp = (vecStrncmpFunc)0x8214D0;   /* CRT */
    sf.vecStricmp = (vecStricmpFunc)0x80A350;   /* RW's own */
    sf.vecStrlen = (vecStrlenFunc)0x826330;     /* CRT */
    sf.vecStrupr = (vecStruprFunc)0x80A3A0;     /* RW's own */
    sf.vecStrlwr = (vecStrlwrFunc)0x80A3D0;     /* RW's own */
    sf.vecStrtok = (vecStrtokFunc)0x82244B;     /* CRT */
    sf.vecSscanf = (vecSscanfFunc)0x8220AD;     /* CRT */
    return TRUE;
}

/* 0x80A440: Nothing to do for the default (CRT) string functions */
void _rwStringClose() {
}

RwBool _rwStringDestroy(RwChar* string) {
    return ((RwBool(__cdecl *)(RwChar*))0x80A450)(string);
}

/* 0x80A470: Size of the string chunk as written to a stream (padded to 4 bytes) */
RwUInt32 _rwStringStreamGetSize(const RwChar* string) {
    ZoneScoped;
    if (string == nullptr) {
        string = "";
    }
    return (RWSRCGLOBAL(stringFuncs).vecStrlen(string) + 4) & ~3u;
}

/* 0x80A4A0 */
const RwChar* _rwStringStreamWrite(const RwChar* string, RwStream* stream) {
    ZoneScoped;
    if (string == nullptr) {
        string = "";
    }
    const RwUInt32 size = (RWSRCGLOBAL(stringFuncs).vecStrlen(string) + 4) & ~3u;
    if (!((RwBool(__cdecl*)(RwStream*, RwInt32, RwInt32, RwUInt32, RwUInt32))0x7ED270)(stream, 2 /* rwID_STRING */, (RwInt32)size, 0x36003u /* rwLIBRARYCURRENTVERSION */, 0xFFFFu /* RWBUILDNUMBER */)) {
        return nullptr;
    }
    return RwStreamWrite(stream, string, size) ? string : nullptr;
}

/* 0x80A510: Reads a string chunk from a stream. If `string` is non-null it must be
 * large enough, otherwise a buffer is allocated. Both rwID_STRING (ANSI, verbatim)
 * and rwID_UNICODESTRING (UTF-16, converted) chunks are accepted; unrelated chunks
 * in between are skipped. */
RwChar* _rwStringStreamFindAndRead(RwChar* string, RwStream* stream) {
    ZoneScoped;
    RwInt32 type;
    RwUInt32 length;
    RwUInt32 versionInfo[3];

    if (!((RwBool(__cdecl*)(RwStream*, RwInt32*, RwUInt32*, RwUInt32*, RwUInt32))0x7ED0F0)(stream, &type, &length, versionInfo, 0)) {
        return nullptr;
    }

    for (;;) {
        if (versionInfo[0] < 0x34000u || versionInfo[0] > 0x36003u) {
            RwPlCoreReportError(0x80000004u, 0);
            return nullptr;
        }

        if (type == 2 /* rwID_STRING */) {
            break; /* Handled after the loop (plain, verbatim read) */
        }

        if (type == 0x13 /* rwID_UNICODESTRING */) {
            bool allocated = false;
            if (string == nullptr) {
                string = (RwChar*)RwMalloc(length, 0x30002u);
                if (string == nullptr) {
                    RwPlCoreReportError(0x80000013u, length);
                    return nullptr;
                }
                allocated = true;
            }

            auto dst = string;
            auto remaining = length;
            if (remaining == 0) {
                return string;
            }
            RwChar buffer[128];
            do {
                const RwUInt32 chunk = remaining < 0x80 ? remaining : 0x80;
                if (RwStreamRead(stream, buffer, chunk) != chunk) {
                    if (allocated) {
                        RwFree(string);
                    }
                    return nullptr;
                }
                remaining -= chunk;
                /* UTF-16LE -> ANSI: keep every second byte */
                const RwUInt32 numChars = chunk / 2;
                for (RwUInt32 i = 0; i < numChars; i++) {
                    dst[i] = buffer[i * 2];
                }
                dst += numChars;
            } while (remaining != 0);
            return string;
        }

        /* Unrelated chunk: skip it and read the next header */
        if (!RwStreamSkip(stream, length)) {
            return nullptr;
        }
        if (!((RwBool(__cdecl*)(RwStream*, RwInt32*, RwUInt32*, RwUInt32*, RwUInt32))0x7ED0F0)(stream, &type, &length, versionInfo, 0)) {
            return nullptr;
        }
    }

    /* rwID_STRING: plain verbatim read */
    if (string == nullptr) {
        string = (RwChar*)RwMalloc(length, 0x30002u);
        if (string == nullptr) {
            RwPlCoreReportError(0x80000013u, length);
            return nullptr;
        }
    }

    {
        auto dst = string;
        auto remaining = length;
        if (remaining == 0) {
            return string;
        }
        RwChar buffer[128];
        do {
            const RwUInt32 chunk = remaining < 0x40 ? remaining : 0x40;
            if (RwStreamRead(stream, buffer, chunk) != chunk) {
                return nullptr; /* NOTE: leaks `string` if it was allocated here (original behavior) */
            }
            remaining -= chunk;
            memcpy(dst, buffer, chunk);
            dst += chunk;
        } while (remaining != 0);
    }
    return string;
}

/* ---------------------------------------------------------------------------
 * RwPlCore hooks
 * --------------------------------------------------------------------------- */

void RwPlCoreInjectHooks() {
    RH_ScopedCategory("Renderware");
    RH_ScopedNamespaceName("RwPlCore");

    RH_ScopedGlobalInstall(_rwStringOpen, 0x80A240);
    RH_ScopedGlobalInstall(_rwStringClose, 0x80A440);
    RH_ScopedGlobalInstall(_rwStringStreamGetSize, 0x80A470);
    RH_ScopedGlobalInstall(_rwStringStreamWrite, 0x80A4A0);
    RH_ScopedGlobalInstall(_rwStringStreamFindAndRead, 0x80A510);
}

void RwPluginRegistrySetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    ((void(__cdecl *)(RwInt32, RwInt32))0x8087B0)(blockSize, numBlocksToPrealloc);
}

RwBool _rwPluginRegistrySetStaticPluginsSize(RwPluginRegistry* reg, RwInt32 size) {
    return ((RwBool(__cdecl *)(RwPluginRegistry*, RwInt32))0x808430)(reg, size);
}

RwInt32 _rwPluginRegistryAddPlugin(RwPluginRegistry* reg, RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return ((RwInt32(__cdecl *)(RwPluginRegistry*, RwInt32, RwUInt32, RwPluginObjectConstructor, RwPluginObjectDestructor, RwPluginObjectCopy))0x8084A0)(reg, size, pluginID, constructCB, destructCB, copyCB);
}

RwInt32 _rwPluginRegistryGetPluginOffset(const RwPluginRegistry* reg, RwUInt32 pluginID) {
    return ((RwInt32(__cdecl *)(const RwPluginRegistry*, RwUInt32))0x808470)(reg, pluginID);
}

const RwPluginRegistry* _rwPluginRegistryInitObject(const RwPluginRegistry* reg, void* object) {
    return ((const RwPluginRegistry*(__cdecl *)(const RwPluginRegistry*, void*))0x8086E0)(reg, object);
}

const RwPluginRegistry* _rwPluginRegistryDeInitObject(const RwPluginRegistry* reg, void* object) {
    return ((const RwPluginRegistry*(__cdecl *)(const RwPluginRegistry*, void*))0x808740)(reg, object);
}

const RwPluginRegistry* _rwPluginRegistryCopyObject(const RwPluginRegistry* reg, void* dstObject, const void* srcObject) {
    return ((const RwPluginRegistry*(__cdecl *)(const RwPluginRegistry*, void*, const void*))0x808770)(reg, dstObject, srcObject);
}

RwInt32 _rwPluginRegistryAddPluginStream(RwPluginRegistry* reg, RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return ((RwInt32(__cdecl *)(RwPluginRegistry*, RwUInt32, RwPluginDataChunkReadCallBack, RwPluginDataChunkWriteCallBack, RwPluginDataChunkGetSizeCallBack))0x8088E0)(reg, pluginID, readCB, writeCB, getSizeCB);
}

RwInt32 _rwPluginRegistryAddPlgnStrmlwysCB(RwPluginRegistry* reg, RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB) {
    return ((RwInt32(__cdecl *)(RwPluginRegistry*, RwUInt32, RwPluginDataChunkAlwaysCallBack))0x808920)(reg, pluginID, alwaysCB);
}

RwInt32 _rwPluginRegistryAddPlgnStrmRightsCB(RwPluginRegistry* reg, RwUInt32 pluginID, RwPluginDataChunkRightsCallBack rightsCB) {
    return ((RwInt32(__cdecl *)(RwPluginRegistry*, RwUInt32, RwPluginDataChunkRightsCallBack))0x808950)(reg, pluginID, rightsCB);
}

const RwPluginRegistry* _rwPluginRegistryReadDataChunks(const RwPluginRegistry* reg, RwStream* stream, void* object) {
    return ((const RwPluginRegistry*(__cdecl *)(const RwPluginRegistry*, RwStream*, void*))0x808980)(reg, stream, object);
}

const RwPluginRegistry* _rwPluginRegistryWriteDataChunks(const RwPluginRegistry* reg, RwStream* stream, const void* object) {
    return ((const RwPluginRegistry*(__cdecl *)(const RwPluginRegistry*, RwStream*, const void*))0x808B40)(reg, stream, object);
}

const RwPluginRegistry* _rwPluginRegistrySkipDataChunks(const RwPluginRegistry* reg, RwStream* stream) {
    return ((const RwPluginRegistry*(__cdecl *)(const RwPluginRegistry*, RwStream*))0x808C10)(reg, stream);
}

RwInt32 _rwPluginRegistryGetSize(const RwPluginRegistry* reg, const void* object) {
    return ((RwInt32(__cdecl *)(const RwPluginRegistry*, const void*))0x808B00)(reg, object);
}

const RwPluginRegistry* _rwPluginRegistryInvokeRights(const RwPluginRegistry* reg, RwUInt32 id, void* obj, RwUInt32 extraData) {
    return ((const RwPluginRegistry*(__cdecl *)(const RwPluginRegistry*, RwUInt32, void*, RwUInt32))0x808AB0)(reg, id, obj, extraData);
}

RwBool RwEngineGetMatrixTolerances(RwMatrixTolerance * const tolerance) {
    return ((RwBool(__cdecl *)(RwMatrixTolerance * const))0x7F1780)(tolerance);
}

RwBool RwEngineSetMatrixTolerances(const RwMatrixTolerance * const tolerance) {
    return ((RwBool(__cdecl *)(const RwMatrixTolerance*))0x7F17B0)(tolerance);
}

void RwMatrixSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    ((void(__cdecl *)(RwInt32, RwInt32))0x7F16A0)(blockSize, numBlocksToPrealloc);
}

RwBool RwMatrixDestroy(RwMatrix* mpMat) {
    return ((RwBool(__cdecl *)(RwMatrix*))0x7F2A20)(mpMat);
}

RwMatrix* RwMatrixCreate() {
    return ((RwMatrix*(__cdecl *)(void))0x7F2A50)();
}

RwMatrix* RwMatrixMultiply(RwMatrix* matrixOut, const RwMatrix* MatrixIn1, const RwMatrix* matrixIn2) {
    return ((RwMatrix*(__cdecl *)(RwMatrix*, const RwMatrix*, const RwMatrix*))0x7F18B0)(matrixOut, MatrixIn1, matrixIn2);
}

RwMatrix* RwMatrixTransform(RwMatrix* matrix, const RwMatrix* transform, RwOpCombineType combineOp) {
    return ((RwMatrix*(__cdecl *)(RwMatrix*, const RwMatrix*, RwOpCombineType))0x7F25A0)(matrix, transform, combineOp);
}

RwMatrix* RwMatrixOrthoNormalize(RwMatrix* matrixOut, const RwMatrix* matrixIn) {
    return ((RwMatrix*(__cdecl *)(RwMatrix*, const RwMatrix*))0x7F1920)(matrixOut, matrixIn);
}

RwMatrix* RwMatrixInvert(RwMatrix* matrixOut, const RwMatrix* matrixIn) {
    return ((RwMatrix*(__cdecl *)(RwMatrix*, const RwMatrix*))0x7F2070)(matrixOut, matrixIn);
}

RwMatrix* RwMatrixScale(RwMatrix* matrix, const RwV3d* scale, RwOpCombineType combineOp) {
    return ((RwMatrix*(__cdecl *)(RwMatrix*, const RwV3d*, RwOpCombineType))0x7F22C0)(matrix, scale, combineOp);
}

RwMatrix* RwMatrixTranslate(RwMatrix* matrix, const RwV3d* translation, RwOpCombineType combineOp) {
    return ((RwMatrix*(__cdecl *)(RwMatrix*, const RwV3d*, RwOpCombineType))0x7F2450)(matrix, translation, combineOp);
}

RwMatrix* RwMatrixRotate(RwMatrix* matrix, const RwV3d* axis, RwReal angle, RwOpCombineType combineOp) {
    return ((RwMatrix*(__cdecl *)(RwMatrix*, const RwV3d*, RwReal, RwOpCombineType))0x7F1FD0)(matrix, axis, angle, combineOp);
}

RwMatrix* RwMatrixRotateOneMinusCosineSine(RwMatrix* matrix, const RwV3d* unitAxis, RwReal oneMinusCosine, RwReal sine, RwOpCombineType combineOp) {
    return ((RwMatrix*(__cdecl *)(RwMatrix*, const RwV3d*, RwReal, RwReal, RwOpCombineType))0x7F1D00)(matrix, unitAxis, oneMinusCosine, sine, combineOp);
}

const RwMatrix* RwMatrixQueryRotate(const RwMatrix* matrix, RwV3d* unitAxis, RwReal* angle, RwV3d* center) {
    return ((const RwMatrix*(__cdecl *)(const RwMatrix*, RwV3d*, RwReal*, RwV3d*))0x7F2720)(matrix, unitAxis, angle, center);
}

/*
 * Matrix library internals (identified from binary 2026-09-27).
 */

// 0x7F2160: Full affine inverse core: computes the inverse of the upper 3x3 via the adjugate
// (fails with an all-zero result when the determinant is 0) and applies -inv(rot) * pos.
// Shared by RwMatrixInvert (general case) and RwMatrixQueryRotate.
RwMatrix* _rwMatrixInvertInternal(RwMatrix* matrixOut, const RwMatrix* matrixIn) {
    return ((RwMatrix*(__cdecl *)(RwMatrix*, const RwMatrix*))0x7F2160)(matrixOut, matrixIn);
}

// 0x7F2930: RwMatrixQueryRotate helper: finds the dominant diagonal element of the rotation
// part, builds the (1 + mii, mij + mji) vector and normalizes it into the unit rotation axis.
const RwMatrix* _rwMatrixQueryRotateAxis(const RwMatrix* matrix, RwV3d* axisOut) {
    return ((const RwMatrix*(__cdecl *)(const RwMatrix*, RwV3d*))0x7F2930)(matrix, axisOut);
}

RwMatrix* RwMatrixUpdate(RwMatrix* matrix) {
    return ((RwMatrix*(__cdecl *)(RwMatrix*))0x7F18A0)(matrix);
}

RwMatrix* RwMatrixOptimize(RwMatrix* matrix, const RwMatrixTolerance* tolerance) {
    return ((RwMatrix*(__cdecl *)(RwMatrix*, const RwMatrixTolerance*))0x7F17E0)(matrix, tolerance);
}

RwReal _rwMatrixDeterminant(const RwMatrix* matrix) {
    return ((RwReal(__cdecl *)(const RwMatrix*))0x7F1450)(matrix);
}

RwReal _rwMatrixNormalError(const RwMatrix* matrix) {
    return ((RwReal(__cdecl *)(const RwMatrix*))0x7F1500)(matrix);
}

RwReal _rwMatrixOrthogonalError(const RwMatrix* matrix) {
    return ((RwReal(__cdecl *)(const RwMatrix*))0x7F14A0)(matrix);
}

RwReal _rwMatrixIdentityError(const RwMatrix* matrix) {
    return ((RwReal(__cdecl *)(const RwMatrix*))0x7F1590)(matrix);
}

RwReal RwV3dNormalize(RwV3d* out, const RwV3d* in) {
    return ((RwReal(__cdecl *)(RwV3d*, const RwV3d*))0x7ED9B0)(out, in);
}

RwReal RwV3dLength(const RwV3d* in) {
    return ((RwReal(__cdecl *)(const RwV3d*))0x7EDAC0)(in);
}

RwReal RwV2dLength(const RwV2d* in) {
    return ((RwReal(__cdecl *)(const RwV2d*))0x7EDBF0)(in);
}

RwReal RwV2dNormalize(RwV2d* out, const RwV2d* in) {
    return ((RwReal(__cdecl *)(RwV2d*, const RwV2d*))0x7EDC60)(out, in);
}

RwV3d* RwV3dTransformPoint(RwV3d* pointOut, const RwV3d* pointIn, const RwMatrix* matrix) {
    return ((RwV3d*(__cdecl *)(RwV3d*, const RwV3d*, const RwMatrix*))0x7EDD60)(pointOut, pointIn, matrix);
}

RwV3d* RwV3dTransformPoints(RwV3d* pointsOut, const RwV3d* pointsIn, RwInt32 numPoints, const RwMatrix* matrix) {
    return ((RwV3d*(__cdecl *)(RwV3d*, const RwV3d*, RwInt32, const RwMatrix*))0x7EDD90)(pointsOut, pointsIn, numPoints, matrix);
}

RwV3d* RwV3dTransformVector(RwV3d* vectorOut, const RwV3d* vectorIn, const RwMatrix* matrix) {
    return ((RwV3d*(__cdecl *)(RwV3d*, const RwV3d*, const RwMatrix*))0x7EDDC0)(vectorOut, vectorIn, matrix);
}

RwV3d* RwV3dTransformVectors(RwV3d* vectorsOut, const RwV3d* vectorsIn, RwInt32 numPoints, const RwMatrix* matrix) {
    return ((RwV3d*(__cdecl *)(RwV3d*, const RwV3d*, RwInt32, const RwMatrix*))0x7EDDF0)(vectorsOut, vectorsIn, numPoints, matrix);
}

RwReal _rwSqrt(const RwReal num) {
    return ((RwReal(__cdecl *)(const RwReal))0x7EDB30)(num);
}

RwReal _rwInvSqrt(const RwReal num) {
    return ((RwReal(__cdecl *)(const RwReal))0x7EDB90)(num);
}

RwReal _rwV3dNormalize(RwV3d* out, const RwV3d* in) {
    return ((RwReal(__cdecl *)(RwV3d*, const RwV3d*))0x7ED910)(out, in);
}

RwSList* _rwSListCreate(RwInt32 size, RwUInt32 hint) {
    return ((RwSList*(__cdecl *)(RwInt32, RwUInt32))0x809160)(size, hint);
}

RwBool _rwSListDestroy(RwSList* sList) {
    return ((RwBool(__cdecl *)(RwSList*))0x809440)(sList);
}

void _rwSListDestroyEndEntries(RwSList* sList, RwInt32 amount) {
    ((void(__cdecl *)(RwSList*, RwInt32))0x809400)(sList, amount);
}

void* _rwSListGetEntry(RwSList* sList, RwInt32 entry) {
    return ((void*(__cdecl *)(RwSList*, RwInt32))0x809510)(sList, entry);
}

void* _rwSListGetNewEntry(RwSList* sList, RwUInt32 hint) {
    return ((void*(__cdecl *)(RwSList*, RwUInt32))0x809240)(sList, hint);
}

RwInt32 _rwSListGetNumEntries(const RwSList* sList) {
    return ((RwInt32(__cdecl *)(const RwSList*))0x8094B0)(sList);
}

void* _rwSListGetBegin(RwSList* sList) {
    return ((void*(__cdecl *)(RwSList*))0x809530)(sList);
}

void* _rwSListGetEnd(RwSList* sList) {
    return ((void*(__cdecl *)(RwSList*))0x809540)(sList);
}

RwUInt32 RwEngineGetVersion() {
    return ((RwUInt32(__cdecl *)(void))0x7F2BA0)();
}

/*
 * Engine internals (identified from binary 2026-09-27).
 */

// 0x7F2AB0: The engine system-config dispatcher every RwEngine* subsystem/video-mode getter and
// RwEngineStart/Stop/Close/Open goes through. Forwards the message to the driver's handler
// (system table + 4) and otherwise applies the engine defaults for the sub-system/video-mode
// messages (e.g. numSubSystems == 1, currentSubSystem == 0, "Only rendering sub system").
RwBool _rwEngineSystemConfig(void* systemTable, RwInt32 message, void* dst, void* src, RwInt32 param) {
    return ((RwBool(__cdecl *)(void*, RwInt32, void*, void*, RwInt32))0x7F2AB0)(systemTable, message, dst, src, param);
}

// 0x7F2B90: Returns the engine open count (.data global next to the RwEngineInstance pointer);
// checked by _rwPluginRegistryAddPlugin to validate that the engine has been opened.
RwUInt32 _rwEngineGetOpenCount() {
    return ((RwUInt32(__cdecl *)(void))0x7F2B90)();
}

RwBool RwEngineInit(const RwMemoryFunctions* memFuncs, RwUInt32 initFlags, RwUInt32 resArenaSize) {
    return ((RwBool(__cdecl *)(const RwMemoryFunctions*, RwUInt32, RwUInt32))0x7F3170)(memFuncs, initFlags, resArenaSize);
}

RwInt32 RwEngineRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor initCB, RwPluginObjectDestructor termCB) {
    return ((RwInt32(__cdecl *)(RwInt32, RwUInt32, RwPluginObjectConstructor, RwPluginObjectDestructor))0x7F2BB0)(size, pluginID, initCB, termCB);
}

RwInt32 RwEngineGetPluginOffset(RwUInt32 pluginID) {
    return ((RwInt32(__cdecl *)(RwUInt32))0x7F2BE0)(pluginID);
}

RwBool RwEngineOpen(RwEngineOpenParams* initParams) {
    return ((RwBool(__cdecl *)(RwEngineOpenParams*))0x7F2F70)(initParams);
}

RwBool RwEngineStart() {
    return ((RwBool(__cdecl *)(void))0x7F2E70)();
}

RwBool RwEngineStop() {
    return ((RwBool(__cdecl *)(void))0x7F2E20)();
}

RwBool RwEngineClose() {
    return ((RwBool(__cdecl *)(void))0x7F2F00)();
}

RwBool RwEngineTerm() {
    return ((RwBool(__cdecl *)(void))0x7F3130)();
}

RwInt32 RwEngineGetNumSubSystems() {
    return ((RwInt32(__cdecl *)(void))0x7F2C00)();
}

RwSubSystemInfo* RwEngineGetSubSystemInfo(RwSubSystemInfo* subSystemInfo, RwInt32 subSystemIndex) {
    return ((RwSubSystemInfo*(__cdecl *)(RwSubSystemInfo*, RwInt32))0x7F2C30)(subSystemInfo, subSystemIndex);
}

RwInt32 RwEngineGetCurrentSubSystem() {
    return ((RwInt32(__cdecl *)(void))0x7F2C60)();
}

RwBool RwEngineSetSubSystem(RwInt32 subSystemIndex) {
    return ((RwBool(__cdecl *)(RwInt32))0x7F2C90)(subSystemIndex);
}

RwInt32 RwEngineGetNumVideoModes() {
    return ((RwInt32(__cdecl *)(void))0x7F2CC0)();
}

RwVideoMode* RwEngineGetVideoModeInfo(RwVideoMode* vmi, RwInt32 modeIndex) {
    return plugin::CallAndReturn<RwVideoMode*, 0x7F2CF0>(vmi, modeIndex);
}

RwInt32 RwEngineGetCurrentVideoMode() {
    return ((RwInt32(__cdecl *)(void))0x7F2D20)();
}

RwBool RwEngineSetVideoMode(RwInt32 modeIndex) {
    return ((RwBool(__cdecl *)(RwInt32))0x7F2D50)(modeIndex);
}

RwInt32 RwEngineGetTextureMemorySize() {
    return ((RwInt32(__cdecl *)(void))0x7F2D80)();
}

RwInt32 RwEngineGetMaxTextureSize() {
    return ((RwInt32(__cdecl *)(void))0x7F2DB0)();
}

RwBool RwEngineSetFocus(RwBool enable) {
    return ((RwBool(__cdecl *)(RwBool))0x7F2DE0)(enable);
}

RwMetrics* RwEngineGetMetrics() {
    return ((RwMetrics*(__cdecl *)(void))0x7F2E10)();
}

//RwFileFunctions* RwOsGetFileInterface() {
//    return ((RwFileFunctions*(__cdecl *)(void))0x804130)();
//}

RwError* RwErrorGet(RwError* code) {
    return ((RwError*(__cdecl *)(RwError*))0x808880)(code);
}

RwError* RwErrorSet(RwError* code) {
    return ((RwError*(__cdecl *)(RwError*))0x808820)(code);
}

/* Mirrors the RWERROR() macro pattern used throughout rwplcore:
 * the error text helper (0x8088D0, retail `_rwerror`) followed by RwErrorSet. */
static void RwPlCoreReportError(RwUInt32 code, RwUInt32 arg) {
    RwError err;
    err.pluginID  = 1;
    err.errorCode = (RwInt32)((RwUInt32(__cdecl*)(RwUInt32, ...))0x8088D0)(code, arg);
    RwErrorSet(&err);
}

RwBool RwResourcesSetArenaSize(RwUInt32 size) {
    return ((RwBool(__cdecl *)(RwUInt32))0x8080C0)(size);
}

RwInt32 RwResourcesGetArenaSize() {
    return ((RwInt32(__cdecl *)(void))0x8081B0)();
}

RwInt32 RwResourcesGetArenaUsage() {
    return ((RwInt32(__cdecl *)(void))0x8081C0)();
}

RwBool RwResourcesEmptyArena() {
    return ((RwBool(__cdecl *)(void))0x8081F0)();
}

RwResEntry* RwResourcesAllocateResEntry(void* owner, RwResEntry* ownerRef, RwInt32 size, RwResEntryDestroyNotify destroyNotify) {
    return ((RwResEntry*(__cdecl *)(void*, RwResEntry*, RwInt32, RwResEntryDestroyNotify))0x807ED0)(owner, ownerRef, size, destroyNotify);
}

RwBool RwResourcesFreeResEntry(RwResEntry* entry) {
    return ((RwBool(__cdecl *)(RwResEntry*))0x807DE0)(entry);
}

void _rwResourcesPurge() {
    ((void(__cdecl *)(void))0x807E50)();
}

RwBool RwStreamFindChunk(RwStream* stream, RwUInt32 type, RwUInt32* lengthOut, RwUInt32* versionOut) {
    return ((RwBool(__cdecl *)(RwStream*, RwUInt32, RwUInt32*, RwUInt32*))0x7ED2D0)(stream, type, lengthOut, versionOut);
}

RwStream* _rwStreamWriteVersionedChunkHeader(RwStream* stream, RwInt32 type, RwInt32 size, RwUInt32 version, RwUInt32 buildNum) {
    return ((RwStream*(__cdecl *)(RwStream*, RwInt32, RwInt32, RwUInt32, RwUInt32))0x7ED270)(stream, type, size, version, buildNum);
}

RwStream* RwStreamWriteReal(RwStream* stream, const RwReal* reals, RwUInt32 numBytes) {
    return ((RwStream*(__cdecl *)(RwStream*, const RwReal*, RwUInt32))0x7ED3D0)(stream, reals, numBytes);
}

RwStream* RwStreamWriteInt32(RwStream* stream, const RwInt32* ints, RwUInt32 numBytes) {
    return ((RwStream*(__cdecl *)(RwStream*, const RwInt32*, RwUInt32))0x7ED460)(stream, ints, numBytes);
}

RwStream* RwStreamWriteInt16(RwStream* stream, const RwInt16* ints, RwUInt32 numBytes) {
    return ((RwStream*(__cdecl *)(RwStream*, const RwInt16*, RwUInt32))0x7ED480)(stream, ints, numBytes);
}

RwStream* RwStreamReadReal(RwStream* stream, RwReal* reals, RwUInt32 numBytes) {
    return ((RwStream*(__cdecl *)(RwStream*, RwReal*, RwUInt32))0x7ED4F0)(stream, reals, numBytes);
}

RwStream* RwStreamReadInt32(RwStream* stream, RwInt32* ints, RwUInt32 numBytes) {
    return ((RwStream*(__cdecl *)(RwStream*, RwInt32*, RwUInt32))0x7ED540)(stream, ints, numBytes);
}

RwStream* RwStreamReadInt16(RwStream* stream, RwInt16* ints, RwUInt32 numBytes) {
    return ((RwStream*(__cdecl *)(RwStream*, RwInt16*, RwUInt32))0x7ED4A0)(stream, ints, numBytes);
}

RwStream* RwStreamReadChunkHeaderInfo(RwStream* stream, RwChunkHeaderInfo* chunkHeaderInfo) {
    return ((RwStream*(__cdecl *)(RwStream*, RwChunkHeaderInfo*))0x7ED590)(stream, chunkHeaderInfo);
}

/****************************************************************************
 * Memory / freelist / resources internals (identified from binary;         *
 * see night2 report). RwEngineInit (0x7F3170) / RwEngineTerm (0x7F3130)    *
 * drive all of these.                                                      *
 ****************************************************************************/

/* 0x801FD0: Installs the process-wide RwMemoryFunctions (defaults:
 * malloc/free/realloc/calloc) and creates the global RwFreeList used for
 * allocating RwFreeList structs themselves (in-place at 0xC9A60C). */
RwBool _rwMemoryOpen(RwMemoryFunctions* memFuncs) {
    return ((RwBool(__cdecl*)(RwMemoryFunctions*))0x801FD0)(memFuncs);
}

/* 0x8020F0: Tears down every live RwFreeList and resets the memory
 * subsystem (counterpart of _rwMemoryOpen). */
void _rwMemoryClose() {
    ((void(__cdecl*)(void))0x8020F0)();
}

/* 0x8019B0: Shared implementation of RwFreeListCreate /
 * RwFreeListCreateAndPreallocateSpace (both retail wrappers target it). */
RwFreeList* _rwFreeListCreateImpl(RwInt32 entrySize, RwInt32 entriesPerBlock, RwInt32 alignment, RwInt32 numBlocksToPreallocate, RwFreeList* inPlaceSpaceForFreeListStruct, RwUInt32 hint) {
    return ((RwFreeList*(__cdecl*)(RwInt32, RwInt32, RwInt32, RwInt32, RwFreeList*, RwUInt32))0x8019B0)(entrySize, entriesPerBlock, alignment, numBlocksToPreallocate, inPlaceSpaceForFreeListStruct, hint);
}

/* 0x801970: Global switch consulted by _rwFreeListCreateImpl - when false,
 * numBlocksToPreallocate is ignored. Set from RwEngineInit. */
void _rwFreeListSetGlobalPreallocate(RwBool enabled) {
    ((void(__cdecl*)(RwBool))0x801970)(enabled);
}

/* 0x8082E0: Opens the resource manager: creates the RwResEntry free list
 * (0x3C-byte entries) and zeroes the arena bookkeeping. */
RwBool _rwResourcesOpen() {
    return ((RwBool(__cdecl*)(void))0x8082E0)();
}

/* 0x808320: Closes the resource manager: destroys every live RwResEntry
 * (via RwFreeListForAllUsed + the per-owner entry lists), then the free
 * list itself. */
RwBool _rwResourcesClose() {
    return ((RwBool(__cdecl*)(void))0x808320)();
}

/* 0x8088D0: Retail _rwerror (rwplcore.h: varargs, returns the code);
 * identity passthrough - the actual error recording is RwErrorSet
 * (0x808820). 5-byte `mov eax, [esp+4]; ret`. */
RwInt32 _rwerror(RwInt32 code, ...) {
    return ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(code);
}
