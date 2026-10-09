#include "StdInc.h"

#include "rwplcore.h"

// ==== RW-CAMPAIGN wave-B SHARED DECLS (consolidated; layouts decompile-verified) ====

// ================= [rwplcore-1] =================
// === SHARED-DECLS rwplcore-stream ============================================
// ОБЩИЙ БЛОК для фрагментов 0x7EC810, 0x7EC9D0, 0x7ECB30, 0x7ECD00, 0x7ECE20, 0x7ECEF0.
// ИНТЕГРАТОРУ: вставить блок ниже ОДИН раз в rwplcore.cpp (можно прямо перед
// RwStreamSetFreeListCreateParams), в остальных фрагментах он НЕ повторяется.
//
// RwFileFunctions - раскладка plugin-sdk (D:\projects\keep\plugin-sdk\plugin_sa\game_sa\rw\rwplcore.h:6153,
// тип RwFileFunctions; в репо-хедере rwplcore.h этого типа НЕТ - объявлен здесь).
// В бинаре GTA SA эта таблица встроена в RwGlobals по смещению 0xC4; смещения членов сверены
// с декомпилами: rwfopen=+0xC8, rwfclose=+0xCC, rwfread=+0xD0, rwfwrite=+0xD4, rwfeof=+0xE0,
// rwfseek=+0xE4, rwftell=+0xEC (0x7EC810/0x7EC9D0/0x7ECB30/0x7ECD00/0x7ECEF0).
struct RwFileFunctionsGta
{
    RwBool  (__cdecl *rwfexist)(const void* name);
    void*   (__cdecl *rwfopen)(const void* name, const void* access);
    int     (__cdecl *rwfclose)(void* fptr);
    size_t  (__cdecl *rwfread)(void* addr, size_t size, size_t count, void* fptr);
    size_t  (__cdecl *rwfwrite)(const void* addr, size_t size, size_t count, void* fptr);
    void*   (__cdecl *rwfgets)(void* buffer, int maxLen, void* fptr);
    int     (__cdecl *rwfputs)(const void* buffer, void* fptr);
    int     (__cdecl *rwfeof)(void* fptr);
    int     (__cdecl *rwfseek)(void* fptr, long offset, int origin);
    int     (__cdecl *rwfflush)(void* fptr);
    int     (__cdecl *rwftell)(void* fptr);
};

// Доступ к полям RwGlobals по ФАКТИЧЕСКИМ офсетам бинара GTA SA (раскладка RwGlobals из
// rwplcore.h:6280 бинару НЕ соответствует - официальная, у бинара.device-блок крупнее).
// Офсеты подтверждены декомпилом RwEngineInit 0x7F3170: memoryFuncs=+0x134, memoryAlloc=+0x144,
// memoryFree=+0x148, engineStatus=+0x150, resArenaInitSize=+0x154; RwFileFunctions=+0xC4.
#define RWGTA_FILEFUNCS(g)      (*reinterpret_cast<RwFileFunctionsGta*>(reinterpret_cast<RwUInt8*>(g) + 0xC4))
#define RWGTA_MEMFUNCS(g)       (*reinterpret_cast<RwMemoryFunctions*>(reinterpret_cast<RwUInt8*>(g) + 0x134))
#define RWGTA_FREELIST_ALLOC(g) (*reinterpret_cast<RwMemoryAllocFn*>(reinterpret_cast<RwUInt8*>(g) + 0x144))
#define RWGTA_FREELIST_FREE(g)  (*reinterpret_cast<RwMemoryFreeFn*>(reinterpret_cast<RwUInt8*>(g) + 0x148))

// ================= [rwplcore-3] =================
// ==== rwplcore-3 COMMON BLOCK ============================================
// Decompile evidence: RwOsGetMemoryInterface(0x802230) returns RwEngineInstance+0x134 and the
// FreeList/engine code calls rwmalloc@[+0x134], rwfree@[+0x138], memoryAlloc@[+0x144],
// memoryFree@[+0x148], reads metrics@[+0x14C], engineStatus@[+0x150], resArenaInitSize@[+0x154],
// static engine copy is 0x56 dwords = 0x158 bytes.
// The repo rwplcore.h RwGlobals does NOT match the compiled layout: the SA build inserts
// RwFileFunctions at +0xC4 (RwOsGetFileInterface 0x804130 returns RwEngineInstance+0xC4),
// which shifts stringFuncs/memoryFuncs/... by +44 bytes relative to the repo header.
// Hence this file-local RwGlobalsSA with the true offsets.
struct RwFileFunctions
{
    // RW SDK type; missing from repo rwplcore.h. Layout per plugin-sdk rw/rwplcore.h:6147
    // (11 fn ptrs = 44 bytes, fills 0xC4..0xF0 exactly).
    RwBool  (*rwfexist)(const RwChar* name);
    void*   (*rwfopen)(const RwChar* name, const RwChar* access);
    int     (*rwfclose)(void* fptr);
    size_t  (*rwfread)(void* addr, size_t size, size_t count, void* fptr);
    size_t  (*rwfwrite)(const void* addr, size_t size, size_t count, void* fptr);
    RwChar* (*rwfgets)(RwChar* buffer, int maxLen, void* fptr);
    int     (*rwfputs)(const RwChar* buffer, void* fptr);
    int     (*rwfeof)(void* fptr);
    int     (*rwfseek)(void* fptr, long offset, int origin);
    int     (*rwfflush)(void* fptr);
    int     (*rwftell)(void* fptr);
};

struct RwGlobalsSA
{
    void*               curCamera;          // 0x00
    void*               curWorld;           // 0x04
    RwUInt16            renderFrame;        // 0x08
    RwUInt16            lightFrame;         // 0x0A
    RwUInt16            pad[2];             // 0x0C
    RwDevice            dOpenDevice;        // 0x10 (0x38 bytes)
    RwStandardFunc      stdFunc[rwSTANDARDNUMOFSTANDARD]; // 0x48 (29 entries, EngineOpen passes 0x1d)
    RwLinkList          dirtyFrameList;     // 0xBC
    RwFileFunctions     fileFuncs;          // 0xC4  (not in repo header!)
    RwStringFunctions   stringFuncs;        // 0xF0
    RwMemoryFunctions   memoryFuncs;        // 0x134 (RwOsGetMemoryInterface -> engine+0x134)
    RwMemoryAllocFn     memoryAlloc;        // 0x144 (freelist-heap/malloc alloc, set by RwEngineInit)
    RwMemoryFreeFn      memoryFree;         // 0x148
    RwMetrics*          metrics;            // 0x14C
    RwEngineStatus      engineStatus;       // 0x150
    RwUInt32            resArenaInitSize;   // 0x154
};
static_assert(sizeof(RwGlobalsSA) == 0x158, "RwGlobalsSA must match compiled SA RwGlobals");
static_assert(offsetof(RwGlobalsSA, memoryFuncs)    == 0x134, "RwGlobalsSA layout");
static_assert(offsetof(RwGlobalsSA, memoryAlloc)    == 0x144, "RwGlobalsSA layout");
static_assert(offsetof(RwGlobalsSA, memoryFree)     == 0x148, "RwGlobalsSA layout");
static_assert(offsetof(RwGlobalsSA, metrics)        == 0x14C, "RwGlobalsSA layout");
static_assert(offsetof(RwGlobalsSA, engineStatus)   == 0x150, "RwGlobalsSA layout");
static_assert(offsetof(RwGlobalsSA, resArenaInitSize) == 0x154, "RwGlobalsSA layout");
static_assert(offsetof(RwGlobalsSA, fileFuncs)      == 0xC4,  "RwGlobalsSA layout");

// The static (default) RwGlobals block; RwEngineInstance points here unless RwEngineOpen
// swapped in a malloc'ed copy (decompile: DAT_00c97b24 = &DAT_00c979c8).
static inline auto& rwGlobalsStatic          = StaticRef<RwGlobalsSA>(0xC979C8);
// Engine open nesting counter, ++ in RwEngineOpen, -- in RwEngineClose (DAT_00c97b20).
static inline auto& rwEngineOpenCount        = StaticRef<RwInt32>(0xC97B20);
// Core plugin registry; its sizeOfStruct is the RwEngineOpen engine-block alloc size
// (decompile: rwmalloc(DAT_008e2298, 0x40000) + _rwPluginRegistry* on &DAT_008e2298).
static inline auto& rwCorePluginRegistry     = StaticRef<RwPluginRegistry>(0x8E2298);
// Gate checked by RwFreeListCreateAndPreallocateSpace: when 0, numBlocksToPreallocate is
// forced to 0 (DAT_008e266c; set up by the freelist module init called from RwEngineInit).
static inline auto& rwFreeListPreallocEnabled = StaticRef<RwInt32>(0x8E266C);
// Freelist-heap used as alloc source by RwFreeList* when present (DAT_00c9a634; fed to
// RwEngineInstance->memoryAlloc / memoryFree).
static inline auto& rwFreeListHeap           = StaticRef<RwFreeList*>(0xC9A634);
// Sentinel ring of all RwFreeList::link members (DAT_00c9a604; RwFreeListPurgeAllFreeLists
// walks it, terminates on &self).
static inline auto& rwFreeListListHead       = StaticRef<RwLinkList>(0xC9A604);
// Resources module globals offset inside RwGlobals (DAT_00c9bc58); +4 is
// rwResources::currentSize, decremented by entry->size in RwResourcesFreeResEntry.
static inline auto& rwResourcesGlobalsOffset = StaticRef<RwInt32>(0xC9BC58);
// Common raw helper of this batch (outside the vendor floor, not in rw_addr_map):
//   RwBool __cdecl rwEngineSystem(void* sysFnOrDevBlock, RwInt32 deviceSystemCode,
//                                 void* pOut, void* pInOut, RwInt32 nIn) @ 0x7F2AB0

// ================= [rwplcore-4] =================
// ======================= SHARED BLOCK (paste once above line 94) =======================
// GTA SA RwGlobals memory-function area. Offsets 0x134.. from the decompiles
// (rwplcore.h official RwGlobals layout does NOT match the binary here).
struct RwGlobalsMemFuncsGta
{
    RwUInt8  pad[0x134];                                                    // 0x000..0x133
    void    *(*rwmalloc)(size_t size, RwUInt32 hint);                       // +0x134 (RwMemoryFunctions.rwmalloc)
    void     (*rwfree)(void *mem);                                          // +0x138 (RwMemoryFunctions.rwfree)
    void    *(*rwrealloc)(void *mem, size_t newSize, RwUInt32 hint);        // +0x13C (RwMemoryFunctions.rwrealloc)
    void    *(*rwcalloc)(size_t numObj, size_t sizeObj, RwUInt32 hint);     // +0x140 (RwMemoryFunctions.rwcalloc)
    void    *(*memoryAlloc)(RwFreeList *fl, RwUInt32 hint);                 // +0x144 (RwGlobals.memoryAlloc, RwMemoryAllocFn; default impl = 0x801C30)
};
static RwGlobalsMemFuncsGta *RwGetMemFuncsGta() {
    return reinterpret_cast<RwGlobalsMemFuncsGta *>(RwEngineInstance);
}

// resources module: value at 0xC9BC58 is the baked module globalsOffset; rwResources sits
// at RwEngineInstance + offset + 0x24 (constant 0x24 straight from decompiles: DAT_00c9bc58+0x24+engine).
static RwInt32 &g_rwResourcesGlobalsOffset     = StaticRef<RwInt32>(0xC9BC58);
static RwInt32 &g_rwResourcesModuleNumInstances = StaticRef<RwInt32>(0xC9BC5C);
static rwResources *RwGetResourcesGta() {
    return reinterpret_cast<rwResources *>(reinterpret_cast<RwUInt8 *>(RwEngineInstance) + g_rwResourcesGlobalsOffset + 0x24);
}

// error module globalsOffset (0xC9A6F0); last RwError lives at RwEngineInstance + offset.
static RwInt32 &g_rwErrorGlobalsOffset = StaticRef<RwInt32>(0xC9A6F0);
static RwError *RwGetLastErrorGta() {
    return reinterpret_cast<RwError *>(reinterpret_cast<RwUInt8 *>(RwEngineInstance) + g_rwErrorGlobalsOffset);
}

// plugin-registry allocator globals (freelist of RwPluginRegEntry + registry tracking array)
static RwFreeList        *&g_rwPluginFreeList     = StaticRef<RwFreeList *>(0xC9A6E4);
static RwPluginRegistry **&g_rwPluginRegistryArray = StaticRef<RwPluginRegistry **>(0xC9A6E8);
static RwInt32           &g_rwPluginRegistryCount = StaticRef<RwInt32>(0xC9A6EC);

// ================= [rwplcore-5] =================
/* GTA SA (RW 3.6 custom) layout of the RwGlobals tail, verified against decompiles:
   - _rwStringOpen @0x80A240 writes 17 function pointers at +0xF0..+0x130 (this function);
   - _rwStringStreamGetSize/Write @0x80A470/0x80A4A0 call +0x120 as strlen;
   - _rwSListDestroy @0x809440 / _rwStringDestroy @0x80A450 / _rwStringStreamFindAndRead @0x80A510
     call +0x134 as malloc(size, hint) and +0x138 as free(ptr). */
struct RwGlobalsGta
{
    RwUInt8         pad[0xF0];      /* 0x000..0x0EF: head of RwGlobals, unused here        */
    vecSprintfFunc  vecSprintf;     /* 0x0F0                                              */
    vecVsprintfFunc vecVsprintf;    /* 0x0F4                                              */
    vecStrcpyFunc   vecStrcpy;      /* 0x0F8                                              */
    vecStrncpyFunc  vecStrncpy;     /* 0x0FC                                              */
    vecStrcatFunc   vecStrcat;      /* 0x100                                              */
    vecStrncatFunc  vecStrncat;     /* 0x104                                              */
    vecStrrchrFunc  vecStrrchr;     /* 0x108                                              */
    vecStrchrFunc   vecStrchr;      /* 0x10C                                              */
    vecStrstrFunc   vecStrstr;      /* 0x110                                              */
    vecStrcmpFunc   vecStrcmp;      /* 0x114                                              */
    vecStrncmpFunc  vecStrncmp;     /* 0x118                                              */
    vecStricmpFunc  vecStricmp;     /* 0x11C                                              */
    vecStrlenFunc   vecStrlen;      /* 0x120                                              */
    vecStruprFunc   vecStrupr;      /* 0x124                                              */
    vecStrlwrFunc   vecStrlwr;      /* 0x128                                              */
    vecStrtokFunc   vecStrtok;      /* 0x12C                                              */
    vecSscanfFunc   vecSscanf;      /* 0x130                                              */
    /* raw allocator slots, malloc-style in this build (see decompiles above).
       NB: rwplcore.h typedefs RwMemoryAllocFn/RwMemoryFreeFn are RwFreeList-shaped
       (RwFreeList* first arg) — they do NOT match this ABI, do not use them here.        */
    void* (*memoryAlloc)(RwUInt32 size, RwUInt32 hint); /* 0x134 */
    void  (*memoryFree)(void* ptr);                     /* 0x138 */
};

// ==== END SHARED DECLS ====


RwMemoryFunctions* RwOsGetMemoryInterface() {
    return &reinterpret_cast<RwGlobalsSA*>(RwEngineInstance)->memoryFuncs;
}


RwFreeList* _rwFreeListCreate(RwInt32 entrySize, RwInt32 entriesPerBlock, RwInt32 alignment, RwUInt32 hint, const RwChar* fileCreate, RwUInt32 lineCreate) {
    (void)fileCreate; (void)lineCreate;
    return RwFreeListCreateAndPreallocateSpace(entrySize, entriesPerBlock, alignment, 1, nullptr, hint);
}


RwFreeList* RwFreeListCreateAndPreallocateSpace(RwInt32 entrySize, RwInt32 entriesPerBlock, RwInt32 alignment, RwInt32 numBlocksToPreallocate, RwFreeList* inPlaceSpaceForFreeListStruct, RwUInt32 hint) {
    RwGlobalsSA* engine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);
    RwFreeList*  freelist = inPlaceSpaceForFreeListStruct;

    if (rwFreeListPreallocEnabled == 0) {
        numBlocksToPreallocate = 0;
    }
    if (alignment == 0) {
        alignment = 4;
    }
    if (freelist == nullptr) {
        if (rwFreeListHeap == nullptr) {
            freelist = static_cast<RwFreeList*>(engine->memoryFuncs.rwmalloc(sizeof(RwFreeList) /* 0x24 */, hint & 0xff0000));
        } else {
            freelist = static_cast<RwFreeList*>(engine->memoryAlloc(rwFreeListHeap, hint & 0xff0000));
        }
        if (freelist == nullptr) {
            return nullptr;
        }
        freelist->flags = 2; /* rwFREELISTFLAG_FREEBLOCKS */
    } else {
        freelist->flags = 3; /* rwFREELISTFLAG_STATIC | rwFREELISTFLAG_FREEBLOCKS */
    }

    const RwUInt32 alignedEntrySize = (static_cast<RwUInt32>(entrySize) - 1 + alignment) & ~static_cast<RwUInt32>(alignment - 1);
    const RwUInt32 bitmapSize = (static_cast<RwUInt32>(entriesPerBlock) + 7) >> 3;

    freelist->entrySize       = alignedEntrySize;
    freelist->entriesPerBlock = entriesPerBlock;
    freelist->heapSize        = bitmapSize;
    freelist->alignment       = alignment;
    rwLinkListInitialize(&freelist->blockList);

    for (RwInt32 i = 0; i < numBlocksToPreallocate; i++) {
        // block: RwLLLink header + bitmap + entriesPerBlock * alignedEntrySize
        RwLLLink* block = static_cast<RwLLLink*>(engine->memoryFuncs.rwmalloc(alignedEntrySize * entriesPerBlock + bitmapSize + 7 + alignment, hint));
        if (block == nullptr) {
            // unwind: free every block added so far, then the freelist struct itself
            while (!rwLinkListEmpty(&freelist->blockList)) {
                RwLLLink* blockLink = freelist->blockList.link.next;
                rwLinkListRemoveLLLink(blockLink);
                engine->memoryFuncs.rwfree(blockLink);
            }
            if ((freelist->flags & rwFREELISTFLAG_STATIC) == 0) {
                if (rwFreeListHeap != freelist && rwFreeListHeap != nullptr) {
                    engine->memoryFree(rwFreeListHeap, freelist);
                    return nullptr;
                }
                engine->memoryFuncs.rwfree(freelist);
            }
            return nullptr;
        }
        rwLinkListAddLLLink(&freelist->blockList, block);
        memset(block + 1, 0, bitmapSize); // clear usage bitmap right after the RwLLLink header
    }

    rwLinkListAddLLLink(&rwFreeListListHead, &freelist->link);
    return freelist;
}


RwBool RwFreeListDestroy(RwFreeList* freelist) {
    RwGlobalsSA* engine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);

    rwLinkListRemoveLLLink(&freelist->link);
    while (!rwLinkListEmpty(&freelist->blockList)) {
        RwLLLink* blockLink = freelist->blockList.link.next;
        rwLinkListRemoveLLLink(blockLink);
        engine->memoryFuncs.rwfree(blockLink);
    }
    if ((freelist->flags & rwFREELISTFLAG_STATIC) == 0) {
        if (rwFreeListHeap != freelist && rwFreeListHeap != nullptr) {
            engine->memoryFree(rwFreeListHeap, freelist);
            return TRUE;
        }
        engine->memoryFuncs.rwfree(freelist);
    }
    return TRUE;
}


void RwFreeListSetFlags(RwFreeList* freeList, RwUInt32 flags) {
    freeList->flags |= flags & 0xfffffffe;
}


RwUInt32 RwFreeListGetFlags(RwFreeList* freeList) {
    return freeList->flags;
}


RwInt32 RwFreeListPurge(RwFreeList* freelist) {
    RwGlobalsSA* engine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);
    RwInt32 freedBlocks = 0;
    const RwUInt32 bitmapSize = freelist->heapSize;

    RwLLLink* blockLink = freelist->blockList.link.next;
    while (blockLink != &freelist->blockList.link) {
        RwLLLink* nextLink = rwLLLinkGetNext(blockLink);
        rwLinkListRemoveLLLink(blockLink);

        const RwUInt8* bitmap = reinterpret_cast<const RwUInt8*>(blockLink + 1);
        RwUInt32 usedEntries = 0;
        for (RwUInt32 i = 0; i < bitmapSize; i++) {
            usedEntries += bitmap[i];
        }
        if (usedEntries != 0) {
            rwLinkListAddLLLink(&freelist->blockList, blockLink); // still has used entries
        } else {
            engine->memoryFuncs.rwfree(blockLink);
            freedBlocks++;
        }
        blockLink = nextLink;
    }
    return static_cast<RwInt32>(freelist->entrySize * static_cast<RwUInt32>(freedBlocks));
}


RwFreeList* RwFreeListForAllUsed(RwFreeList* freelist, RwFreeListCallBack fpCallBack, void* data) {
    RwGlobalsSA* engine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);
    const RwUInt32 bitmapSize = freelist->heapSize;

    RwLLLink* blockLink = freelist->blockList.link.next;
    if (blockLink == &freelist->blockList.link) {
        return freelist;
    }
    do {
        RwUInt8* bitmapCopy = static_cast<RwUInt8*>(engine->memoryFuncs.rwmalloc(bitmapSize, 0x10000));
        if (bitmapCopy == nullptr) {
            return nullptr;
        }
        memcpy(bitmapCopy, blockLink + 1, bitmapSize);

        RwLLLink* nextLink = rwLLLinkGetNext(blockLink);
        const RwUInt32 blockBase = reinterpret_cast<RwUInt32>(blockLink);
        const RwUInt32 entriesBase = (blockBase + bitmapSize + freelist->alignment + 7) & ~static_cast<RwUInt32>(freelist->alignment - 1);
        for (RwUInt32 i = 0; i < bitmapSize; i++) {
            const RwUInt8 byte = bitmapCopy[i];
            if (byte != 0) {
                for (RwUInt32 bit = 0; bit < 8; bit++) {
                    if (byte & (0x80 >> bit)) {
                        void* entry = reinterpret_cast<void*>(entriesBase + (bit + i * 8) * freelist->entrySize);
                        fpCallBack(entry, data);
                    }
                }
            }
        }
        engine->memoryFuncs.rwfree(bitmapCopy);
        blockLink = nextLink;
    } while (blockLink != &freelist->blockList.link);
    return freelist;
}


RwInt32 RwFreeListPurgeAllFreeLists() {
    RwInt32 total = 0;
    for (RwLLLink* link = rwFreeListListHead.link.next; link != &rwFreeListListHead.link; link = rwLLLinkGetNext(link)) {
        RwFreeList* freelist = rwLLLinkGetData(link, RwFreeList, link); // link is at RwFreeList+0x1C
        RwInt32 purged = RwFreeListPurge(freelist);
        if (purged > 0) {
            total += purged;
        }
    }
    return total;
}


void RwStreamSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc)
{
    StaticRef<RwInt32>(0x8E2218) = blockSize;
    StaticRef<RwInt32>(0x8E221C) = numBlocksToPrealloc;
}


RwStream* _rwStreamInitialize(RwStream* stream, RwBool rwOwned, RwStreamType type, RwStreamAccessType accessType, const void* data)
{
    if (stream == nullptr)
        return nullptr;

    stream->type = type;
    stream->accessType = accessType;
    stream->rwOwned = rwOwned;

    switch (type) {
    case rwSTREAMFILE:
    {
        // валидация FILE*: rwftell(data) == -1 => невалидный хэндл (декомпил: (EI+0xEC)(data) == -1)
        if (RWGTA_FILEFUNCS(RwEngineInstance).rwftell(const_cast<void*>(data)) == -1)
            return nullptr;
        stream->Type.file.fpFile = const_cast<void*>(data);
        return stream;
    }
    case rwSTREAMFILENAME:
    {
        const void* mode = nullptr;
        if (accessType == rwSTREAMREAD)
            mode = "rb";
        else if (accessType == rwSTREAMWRITE)
            mode = "wb";
        else if (accessType == rwSTREAMAPPEND)
            mode = "ab";
        else {
            RwError rwError;
            rwError.pluginID = 1;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0xD);
            RwErrorSet(&rwError);
        }
        if (mode) {
            void* fp = RWGTA_FILEFUNCS(RwEngineInstance).rwfopen(data, mode);
            if (fp != nullptr) {
                stream->Type.file.fpFile = fp;
                return stream;
            }
        }
        {
            RwError rwError;
            rwError.pluginID = 1;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000002, data);
            RwErrorSet(&rwError);
        }
        return nullptr;
    }
    case rwSTREAMMEMORY:
    {
        const RwMemory* mem = static_cast<const RwMemory*>(data);
        if (accessType == rwSTREAMREAD) {
            stream->Type.memory.position = 0;
            stream->Type.memory.nSize = mem->length;
            stream->Type.memory.memBlock = mem->start;
        } else if (accessType == rwSTREAMWRITE) {
            stream->Type.memory.position = 0;
            stream->Type.memory.nSize = 0;
            stream->Type.memory.memBlock = nullptr;
        } else if (accessType == rwSTREAMAPPEND) {
            stream->Type.memory.position = mem->length;
            stream->Type.memory.nSize = mem->length;
            stream->Type.memory.memBlock = mem->start;
        } else {
            RwError rwError;
            rwError.pluginID = 1;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0xD);
            RwErrorSet(&rwError);
            return nullptr;
        }
        return stream;
    }
    case rwSTREAMCUSTOM:
    {
        // декомпил: копия 5 dword'ов из data (цикл 5 итераций) = покомпонентное присваивание
        const RwStreamCustom* custom = static_cast<const RwStreamCustom*>(data);
        stream->Type.custom.sfnclose = custom->sfnclose;
        stream->Type.custom.sfnread = custom->sfnread;
        stream->Type.custom.sfnwrite = custom->sfnwrite;
        stream->Type.custom.sfnskip = custom->sfnskip;
        stream->Type.custom.data = custom->data;
        return stream;
    }
    default:
    {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0xE);
        RwErrorSet(&rwError);
        return nullptr;
    }
    }
}


RwStream* RwStreamOpen(RwStreamType type, RwStreamAccessType accessType, const void* data)
{
    RwFreeList* streamFreeList = *(RwFreeList**)(reinterpret_cast<RwUInt8*>(RwEngineInstance) + *(RwUInt32*)0xC9792C);
    RwStream* stream = static_cast<RwStream*>(RWGTA_FREELIST_ALLOC(RwEngineInstance)(streamFreeList, 0x30404));

    if (stream == nullptr) {
        // декомпил: при неудаче аллокации уходит на общий FAIL-путь, где зовёт FreeListFree(freelist, NULL)
        RWGTA_FREELIST_FREE(RwEngineInstance)(streamFreeList, stream);
        return nullptr;
    }

    stream->type = type;
    stream->accessType = accessType;
    stream->rwOwned = TRUE; // бинаре-выделенный стрим освобождается в RwStreamClose

    RwBool ok = TRUE;
    switch (type) {
    case rwSTREAMFILE:
    {
        // валидация FILE* через rwftell (декомпил: (EI+0xEC)(data) == -1 -> fail; ошибки НЕ ставится)
        if (RWGTA_FILEFUNCS(RwEngineInstance).rwftell(const_cast<void*>(data)) == -1)
            ok = FALSE;
        else
            stream->Type.file.fpFile = const_cast<void*>(data);
        break;
    }
    case rwSTREAMFILENAME:
    {
        const void* mode = nullptr;
        if (accessType == rwSTREAMREAD)
            mode = "rb";
        else if (accessType == rwSTREAMWRITE)
            mode = "wb";
        else if (accessType == rwSTREAMAPPEND)
            mode = "ab";
        else {
            RwError rwError;
            rwError.pluginID = 1;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0xD);
            RwErrorSet(&rwError);
        }
        if (mode) {
            void* fp = RWGTA_FILEFUNCS(RwEngineInstance).rwfopen(data, mode);
            if (fp != nullptr) {
                stream->Type.file.fpFile = fp;
                break;
            }
        }
        {
            RwError rwError;
            rwError.pluginID = 1;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000002, data);
            RwErrorSet(&rwError);
        }
        ok = FALSE;
        break;
    }
    case rwSTREAMMEMORY:
    {
        const RwMemory* mem = static_cast<const RwMemory*>(data);
        if (accessType == rwSTREAMREAD) {
            stream->Type.memory.position = 0;
            stream->Type.memory.nSize = mem->length;
            stream->Type.memory.memBlock = mem->start;
        } else if (accessType == rwSTREAMWRITE) {
            stream->Type.memory.position = 0;
            stream->Type.memory.nSize = 0;
            stream->Type.memory.memBlock = nullptr;
        } else if (accessType == rwSTREAMAPPEND) {
            stream->Type.memory.position = mem->length;
            stream->Type.memory.nSize = mem->length;
            stream->Type.memory.memBlock = mem->start;
        } else {
            RwError rwError;
            rwError.pluginID = 1;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0xD);
            RwErrorSet(&rwError);
            ok = FALSE;
        }
        break;
    }
    case rwSTREAMCUSTOM:
    {
        // декомпил: копия 5 dword'ов из data
        const RwStreamCustom* custom = static_cast<const RwStreamCustom*>(data);
        stream->Type.custom.sfnclose = custom->sfnclose;
        stream->Type.custom.sfnread = custom->sfnread;
        stream->Type.custom.sfnwrite = custom->sfnwrite;
        stream->Type.custom.sfnskip = custom->sfnskip;
        stream->Type.custom.data = custom->data;
        break;
    }
    default:
    {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0xE);
        RwErrorSet(&rwError);
        ok = FALSE;
        break;
    }
    }

    if (ok)
        return stream;

    RWGTA_FREELIST_FREE(RwEngineInstance)(streamFreeList, stream);
    return nullptr;
}


RwBool RwStreamClose(RwStream* stream, void* data)
{
    RwBool ok = TRUE;
    switch (stream->type) {
    case rwSTREAMFILENAME:
    {
        ok = RWGTA_FILEFUNCS(RwEngineInstance).rwfclose(stream->Type.file.fpFile) == 0;
        break;
    }
    case rwSTREAMMEMORY:
    {
        ok = TRUE;
        // декомпил: если accessType != rwSTREAMREAD и data != NULL - вернуть блок и позицию
        if (stream->accessType != rwSTREAMREAD && data != nullptr) {
            RwMemory* memOut = static_cast<RwMemory*>(data);
            memOut->start = stream->Type.memory.memBlock;   // data[0] = Type.memory.memBlock
            memOut->length = stream->Type.memory.position;  // data[1] = Type.memory.position
        }
        break;
    }
    case rwSTREAMCUSTOM:
    {
        if (stream->Type.custom.sfnclose != nullptr)
            stream->Type.custom.sfnclose(stream->Type.custom.data);
        // декомпил: провал в case rwSTREAMFILE (нет break)
        // fall through
    }
    case rwSTREAMFILE:
    {
        // FILE*, открытый вызывающим, не закрывается
        ok = TRUE;
        break;
    }
    default:
    {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0xE);
        RwErrorSet(&rwError);
        return FALSE; // декомпил: выход сразу, без FreeListFree
    }
    }

    if (stream->rwOwned != 0) {
        RwFreeList* streamFreeList = *(RwFreeList**)(reinterpret_cast<RwUInt8*>(RwEngineInstance) + *(RwUInt32*)0xC9792C);
        RWGTA_FREELIST_FREE(RwEngineInstance)(streamFreeList, stream);
    }
    return ok;
}


RwUInt32 RwStreamRead(RwStream* stream, void* buffer, RwUInt32 length)
{
    switch (stream->type) {
    case rwSTREAMFILE:
    case rwSTREAMFILENAME:
    {
        void* fp = stream->Type.file.fpFile;
        RwUInt32 result = static_cast<RwUInt32>(
            RWGTA_FILEFUNCS(RwEngineInstance).rwfread(buffer, 1, length, fp));
        if (result != length) {
            if (RWGTA_FILEFUNCS(RwEngineInstance).rwfeof(fp) != 0) {
                RwError rwError;
                rwError.pluginID = 1;
                rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(5);
                RwErrorSet(&rwError);
                return result;
            }
            RwError rwError;
            rwError.pluginID = 1;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x8000001A);
            RwErrorSet(&rwError);
        }
        return result;
    }
    case rwSTREAMMEMORY:
    {
        if (stream->Type.memory.nSize - stream->Type.memory.position < length) {
            RwError rwError;
            rwError.pluginID = 1;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(5);
            RwErrorSet(&rwError);
            length = stream->Type.memory.nSize - stream->Type.memory.position;
        }
        const RwUInt8* src = stream->Type.memory.memBlock + stream->Type.memory.position;
        // декомпил: копия 2 циклами (dword + байты) = побайтовая копия
        for (RwUInt32 i = 0; i < length; i++)
            static_cast<RwUInt8*>(buffer)[i] = src[i];
        stream->Type.memory.position += length;
        return length;
    }
    case rwSTREAMCUSTOM:
    {
        return stream->Type.custom.sfnread(stream->Type.custom.data, buffer, length);
    }
    default:
    {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0xE);
        RwErrorSet(&rwError);
        return 0;
    }
    }
}


RwStream* RwStreamWrite(RwStream* stream, const void* buffer, RwUInt32 length)
{
    switch (stream->type) {
    case rwSTREAMFILE:
    case rwSTREAMFILENAME:
    {
        RwUInt32 result = static_cast<RwUInt32>(
            RWGTA_FILEFUNCS(RwEngineInstance).rwfwrite(buffer, 1, length, stream->Type.file.fpFile));
        if (result == length)
            return stream;
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x8000001C);
        RwErrorSet(&rwError);
        return nullptr;
    }
    case rwSTREAMMEMORY:
    {
        if (stream->Type.memory.memBlock == nullptr) {
            stream->Type.memory.memBlock = static_cast<RwUInt8*>(
                RWGTA_MEMFUNCS(RwEngineInstance).rwmalloc(0x200, 0x30404));
            if (stream->Type.memory.memBlock == nullptr) {
                RwError rwError;
                rwError.pluginID = 1;
                rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, 0x200);
                RwErrorSet(&rwError);
                return nullptr;
            }
            stream->Type.memory.nSize = 0x200;
        }
        if (stream->Type.memory.nSize - stream->Type.memory.position < length) {
            RwUInt32 newSize = stream->Type.memory.nSize + 0x200;
            if (length > 0x1FF)
                newSize = stream->Type.memory.nSize + length;
            RwUInt8* memBlock = static_cast<RwUInt8*>(
                RWGTA_MEMFUNCS(RwEngineInstance).rwrealloc(stream->Type.memory.memBlock, newSize, 0x1030404));
            if (memBlock == nullptr) {
                RwError rwError;
                rwError.pluginID = 1;
                rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, newSize - stream->Type.memory.nSize);
                RwErrorSet(&rwError);
                return nullptr;
            }
            stream->Type.memory.memBlock = memBlock;
            stream->Type.memory.nSize = newSize;
        }
        RwUInt8* dst = stream->Type.memory.memBlock + stream->Type.memory.position;
        for (RwUInt32 i = 0; i < length; i++)
            dst[i] = static_cast<const RwUInt8*>(buffer)[i];
        stream->Type.memory.position += length;
        return stream;
    }
    case rwSTREAMCUSTOM:
    {
        if (stream->Type.custom.sfnwrite(stream->Type.custom.data, buffer, length))
            return stream;
        return nullptr;
    }
    default:
    {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0xE);
        RwErrorSet(&rwError);
        return nullptr;
    }
    }
}


RwStream* RwStreamSkip(RwStream* stream, RwUInt32 offset)
{
    if (offset == 0)
        return stream;

    switch (stream->type) {
    case rwSTREAMFILE:
    case rwSTREAMFILENAME:
    {
        void* fp = stream->Type.file.fpFile;
        if (RWGTA_FILEFUNCS(RwEngineInstance).rwfseek(fp, static_cast<long>(offset), 1 /*SEEK_CUR*/) == 0)
            return stream;
        if (RWGTA_FILEFUNCS(RwEngineInstance).rwfeof(fp) != 0) {
            RwError rwError;
            rwError.pluginID = 1;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(5);
            RwErrorSet(&rwError);
        }
        return nullptr;
    }
    case rwSTREAMMEMORY:
    {
        if (offset + stream->Type.memory.position <= stream->Type.memory.nSize) {
            stream->Type.memory.position += offset;
            return stream;
        }
        stream->Type.memory.position = stream->Type.memory.nSize;
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(5);
        RwErrorSet(&rwError);
        return nullptr;
    }
    case rwSTREAMCUSTOM:
    {
        if (stream->Type.custom.sfnskip(stream->Type.custom.data, offset))
            return stream;
        return nullptr;
    }
    default:
    {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0xE);
        RwErrorSet(&rwError);
        return nullptr;
    }
    }
}


RwBool _rwStringOpen() {
    RwGlobalsGta* eng = (RwGlobalsGta*)RwEngineInstance;

    eng->vecSprintf  = (vecSprintfFunc)0x821BB5;   /* CRT sprintf  */
    eng->vecVsprintf = (vecVsprintfFunc)0x8230B8;  /* CRT vsprintf */
    eng->vecStrcpy   = (vecStrcpyFunc)0x826590;    /* CRT strcpy   */
    eng->vecStrncpy  = (vecStrncpyFunc)0x821F40;   /* CRT strncpy  */
    eng->vecStrcat   = (vecStrcatFunc)0x8265A0;    /* CRT strcat   */
    eng->vecStrncat  = (vecStrncatFunc)0x826450;   /* strncat (internal helper) */
    eng->vecStrrchr  = (vecStrrchrFunc)0x80A420;   /* strrchr (internal helper) */
    eng->vecStrchr   = (vecStrchrFunc)0x80A400;    /* strchr  (internal helper) */
    eng->vecStrstr   = (vecStrstrFunc)0x822650;    /* CRT strstr   */
    eng->vecStrcmp   = (vecStrcmpFunc)0x8263C0;    /* CRT strcmp   */
    eng->vecStrncmp  = (vecStrncmpFunc)0x8214D0;   /* CRT strncmp  */
    eng->vecStricmp  = (vecStricmpFunc)0x80A350;   /* stricmp (internal helper) */
    eng->vecStrlen   = (vecStrlenFunc)0x826330;    /* CRT strlen   */
    eng->vecStrupr   = (vecStruprFunc)0x80A3A0;    /* strupr  (internal helper) */
    eng->vecStrlwr   = (vecStrlwrFunc)0x80A3D0;    /* strlwr  (internal helper) */
    eng->vecStrtok   = (vecStrtokFunc)0x82244B;    /* CRT strtok   */
    eng->vecSscanf   = (vecSscanfFunc)0x8220AD;    /* CRT sscanf   */
    return TRUE;
}


void _rwStringClose() {
    /* decompile @0x80A440: empty body */
}


RwBool _rwStringDestroy(RwChar* string) {
    /* decompile @0x80A450: engine->memoryFree(string) — raw free slot at RwGlobals+0x138 */
    ((RwGlobalsGta*)RwEngineInstance)->memoryFree(string);
    return TRUE;
}


RwUInt32 _rwStringStreamGetSize(const RwChar* string) {
    if (string == nullptr)
        string = "";
    /* decompile @0x80A470: strlen via engine string table slot +0x120 (NOT the rwstrlen macro —
       that macro reads wrong offsets on this binary), size rounded up to multiple of 4 */
    const RwGlobalsGta* eng = (const RwGlobalsGta*)RwEngineInstance;
    return (RwUInt32)(eng->vecStrlen(string) + 4u) & 0xFFFFFFFC;
}


const RwChar* _rwStringStreamWrite(const RwChar* string, RwStream* stream) {
    if (string == nullptr)
        string = "";

    const RwGlobalsGta* eng = (const RwGlobalsGta*)RwEngineInstance;
    const RwUInt32      size = (RwUInt32)(eng->vecStrlen(string) + 4u) & 0xFFFFFFFC;

    /* decompile @0x80A4A0: chunk header (rwID_STRING, size) with GTA RW version 3.6.0.3.
       NB: 0x36003 is the BINARY's rwLIBRARYCURRENTVERSION; rwversion.h in the repo defines
       rwLIBRARYCURRENTVERSION = 0x37002 — literals kept per decompile. */
    if (!_rwStreamWriteVersionedChunkHeader(stream, rwID_STRING, (RwInt32)size, 0x36003, 0xFFFF))
        return nullptr;

    /* write the string bytes (padded size incl. the NUL), return string on success */
    return RwStreamWrite(stream, string, size) ? string : nullptr;
}


RwChar* _rwStringStreamFindAndRead(RwChar* string, RwStream* stream) {
    RwGlobalsGta* eng = (RwGlobalsGta*)RwEngineInstance;

    RwInt32  type;
    RwUInt32 length;
    RwUInt32 version;
    if (!((RwBool(__cdecl*)(RwStream*, RwInt32*, RwUInt32*, RwUInt32*, RwInt32))0x7ED0F0)(stream, &type, &length, &version, 0))
        return nullptr;

    for (;;) {
        /* version window of the GTA build: [3.4.0.0 .. 3.6.0.3] (literals per decompile) */
        if (version < 0x34000 || 0x36003 < version) {
            RwError error;
            error.pluginID  = 1; /* plcore plugin id, per decompile */
            error.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000004);
            RwErrorSet(&error);
            return nullptr;
        }

        if (type == rwID_STRING)
            break;

        if (type == rwID_UNICODESTRING) {
            /* UTF-16LE chunk: down-convert to single-byte RwChar* (keep low byte of each pair) */
            RwBool allocated = FALSE;
            if (string == nullptr) {
                string = (RwChar*)eng->memoryAlloc(length, rwID_STRING | rwMEMHINTDUR_EVENT /* 0x30002 */);
                if (string == nullptr) {
                    RwError error;
                    error.pluginID  = 1;
                    error.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, length);
                    RwErrorSet(&error);
                    return nullptr;
                }
                allocated = TRUE;
            }

            if (length != 0) {
                RwChar   raw[128];
                RwChar*  out       = string;
                RwUInt32 remaining = length;
                do {
                    const RwUInt32 numBytes = (remaining < 0x81) ? remaining : 0x80;
                    if (RwStreamRead(stream, raw, numBytes) != numBytes) {
                        if (allocated)
                            eng->memoryFree(string);
                        return nullptr;
                    }
                    remaining -= numBytes;
                    const RwUInt32 numChars = numBytes >> 1;
                    for (RwUInt32 i = 0; i < numChars; i++)
                        out[i] = raw[i * 2];
                    out += numChars;
                } while (remaining != 0);
            }
            return string;
        }

        /* other chunk type: skip it and read the next header */
        if (!RwStreamSkip(stream, length))
            return nullptr;
        if (!((RwBool(__cdecl*)(RwStream*, RwInt32*, RwUInt32*, RwUInt32*, RwInt32))0x7ED0F0)(stream, &type, &length, &version, 0))
            return nullptr;
    }

    /* rwID_STRING chunk: plain single-byte string */
    if (string == nullptr) {
        string = (RwChar*)eng->memoryAlloc(length, rwID_STRING | rwMEMHINTDUR_EVENT /* 0x30002 */);
        if (string == nullptr) {
            RwError error;
            error.pluginID  = 1;
            error.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, length);
            RwErrorSet(&error);
            return nullptr;
        }
    }

    if (length != 0) {
        RwChar   raw[128];
        RwChar*  out       = string;
        RwUInt32 remaining = length;
        do {
            const RwUInt32 numBytes = (remaining < 0x41) ? remaining : 0x40;
            if (RwStreamRead(stream, raw, numBytes) != numBytes)
                return nullptr; /* NB: original does NOT free the buffer here — leak kept as-is */
            remaining -= numBytes;
            for (RwUInt32 i = 0; i < numBytes; i++)
                out[i] = raw[i];
            out += numBytes;
        } while (remaining != 0);
    }
    return string;
}


void RwPluginRegistrySetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    StaticRef<RwInt32>(0x8E26AC) = blockSize;         // DAT_008e26ac
    StaticRef<RwInt32>(0x8E26B0) = numBlocksToPrealloc; // DAT_008e26b0
}


RwBool _rwPluginRegistrySetStaticPluginsSize(RwPluginRegistry *reg, RwInt32 size) {
    if (reg->sizeOfStruct == reg->origSizeOfStruct) {
        if ((reg->maxSizeOfStruct < 1) || (size < reg->maxSizeOfStruct))
            reg->maxSizeOfStruct = reg->sizeOfStruct + size;
        reg->staticAlloc = TRUE;
        return TRUE;
    }
    return FALSE;
}


RwInt32 _rwPluginRegistryAddPlugin(RwPluginRegistry *reg, RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    if (g_rwPluginFreeList != nullptr) {
        if (((RwBool(__cdecl *)(void))0x7F2B90)() == FALSE) {
            RwGlobalsMemFuncsGta *mem = RwGetMemFuncsGta();
            // non-default freelist allocator -> track this registry in the global registry list
            if (mem->memoryAlloc != (RwMemoryAllocFn)0x801C30) {
                RwInt32 i = 0;
                if (g_rwPluginRegistryCount != 0) {
                    for (i = 0; i < g_rwPluginRegistryCount; i++) {
                        if (reg == g_rwPluginRegistryArray[i])
                            break;
                    }
                }
                if (g_rwPluginRegistryCount == i) { // not tracked yet -> append
                    RwPluginRegistry **newArray = (RwPluginRegistry **)mem->rwmalloc(g_rwPluginRegistryCount * 4 + 4, rwMEMHINTDUR_GLOBAL);
                    RwInt32 u = 0;
                    if (g_rwPluginRegistryArray != nullptr) {
                        if (g_rwPluginRegistryCount != 0) {
                            do {
                                u++;
                                newArray[u - 1] = g_rwPluginRegistryArray[u - 1];
                            } while (u < g_rwPluginRegistryCount);
                        }
                        mem->rwfree(g_rwPluginRegistryArray);
                    }
                    newArray[u] = reg;
                    g_rwPluginRegistryCount++;
                    g_rwPluginRegistryArray = newArray;
                }
            }
            // duplicate pluginID -> error out, return the existing offset
            RwPluginRegEntry *existing;
            for (existing = reg->firstRegEntry; existing != nullptr; existing = existing->nextRegEntry) {
                if (existing->pluginID == pluginID) {
                    RwError err;
                    err.pluginID = 1;
                    err.errorCode = _rwerror(0x80000017);
                    RwErrorSet(&err);
                    return existing->offset;
                }
            }
            RwInt32 newOffset = ((size + 3) & ~3) + reg->sizeOfStruct;
            RwPluginRegEntry *entry;
            if ((reg->maxSizeOfStruct == 0 || newOffset <= reg->maxSizeOfStruct) &&
                (entry = (RwPluginRegEntry *)mem->memoryAlloc(g_rwPluginFreeList, rwMEMHINTDUR_GLOBAL)) != nullptr) {
                entry->offset          = reg->sizeOfStruct;
                reg->sizeOfStruct      = newOffset;
                entry->size            = size;
                entry->pluginID        = pluginID;
                entry->readCB          = nullptr;
                entry->writeCB         = nullptr;
                entry->getSizeCB       = nullptr;
                entry->alwaysCB        = nullptr;
                entry->rightsCB        = nullptr;
                entry->constructCB     = (constructCB != nullptr) ? constructCB : (RwPluginObjectConstructor)0x8086B0;
                entry->destructCB      = (destructCB != nullptr) ? destructCB : (RwPluginObjectDestructor)0x8086C0;
                entry->copyCB          = (copyCB != nullptr) ? copyCB : (RwPluginObjectCopy)0x8086D0;
                entry->errStrCB        = nullptr;
                entry->nextRegEntry    = nullptr;
                entry->prevRegEntry    = nullptr;
                entry->parentRegistry  = reg;
                if (reg->firstRegEntry == nullptr) {
                    reg->firstRegEntry = entry;
                    reg->lastRegEntry  = entry;
                    return entry->offset;
                }
                reg->lastRegEntry->nextRegEntry = entry;
                entry->prevRegEntry             = reg->lastRegEntry;
                reg->lastRegEntry               = entry;
                return entry->offset;
            }
        } else {
            RwError err;
            err.pluginID = 1;
            err.errorCode = _rwerror(0x80000017);
            RwErrorSet(&err);
        }
    }
    return -1;
}


RwInt32 _rwPluginRegistryGetPluginOffset(const RwPluginRegistry *reg, RwUInt32 pluginID) {
    for (const RwPluginRegEntry *entry = reg->firstRegEntry; entry != nullptr; entry = entry->nextRegEntry) {
        if (entry->pluginID == pluginID)
            return entry->offset;
    }
    return -1;
}


const RwPluginRegistry *_rwPluginRegistryInitObject(const RwPluginRegistry *reg, void *object) {
    RwPluginRegEntry *entry = reg->firstRegEntry;
    while (entry != nullptr) {
        if (entry->constructCB(object, entry->offset, entry->size) == nullptr)
            break;
        entry = entry->nextRegEntry;
    }
    if (entry == nullptr)
        return reg;
    for (entry = entry->prevRegEntry; entry != nullptr; entry = entry->prevRegEntry)
        entry->destructCB(object, entry->offset, entry->size);
    return nullptr;
}


const RwPluginRegistry *_rwPluginRegistryDeInitObject(const RwPluginRegistry *reg, void *object) {
    for (RwPluginRegEntry *entry = reg->lastRegEntry; entry != nullptr; entry = entry->prevRegEntry)
        entry->destructCB(object, entry->offset, entry->size);
    return reg;
}


const RwPluginRegistry *_rwPluginRegistryCopyObject(const RwPluginRegistry *reg, void *dstObject, const void *srcObject) {
    for (RwPluginRegEntry *entry = reg->firstRegEntry; entry != nullptr; entry = entry->nextRegEntry)
        entry->copyCB(dstObject, srcObject, entry->offset, entry->size);
    return reg;
}


RwInt32 _rwPluginRegistryAddPluginStream(RwPluginRegistry *reg, RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB) {
    for (RwPluginRegEntry *entry = reg->firstRegEntry; entry != nullptr; entry = entry->nextRegEntry) {
        if (entry->pluginID == pluginID) {
            entry->readCB    = readCB;
            entry->writeCB   = writeCB;
            entry->getSizeCB = getSizeCB;
            return entry->offset;
        }
    }
    return -1;
}


RwInt32 _rwPluginRegistryAddPlgnStrmlwysCB(RwPluginRegistry *reg, RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB) {
    for (RwPluginRegEntry *entry = reg->firstRegEntry; entry != nullptr; entry = entry->nextRegEntry) {
        if (entry->pluginID == pluginID) {
            entry->alwaysCB = alwaysCB;
            return entry->offset;
        }
    }
    return -1;
}


RwInt32 _rwPluginRegistryAddPlgnStrmRightsCB(RwPluginRegistry *reg, RwUInt32 pluginID, RwPluginDataChunkRightsCallBack rightsCB) {
    for (RwPluginRegEntry *entry = reg->firstRegEntry; entry != nullptr; entry = entry->nextRegEntry) {
        if (entry->pluginID == pluginID) {
            entry->rightsCB = rightsCB;
            return entry->offset;
        }
    }
    return -1;
}


const RwPluginRegistry *_rwPluginRegistryReadDataChunks(const RwPluginRegistry *reg, RwStream *stream, void *object) {
    RwUInt32 extLength;
    RwUInt32 version;
    if (RwStreamFindChunk(stream, rwID_EXTENSION /* 3 */, &extLength, &version)) {
        if (version < 0x34000 || 0x36003 < version) {
            RwError err;
            err.pluginID = 1;
            err.errorCode = _rwerror(0x80000004);
            RwErrorSet(&err);
        } else {
            RwUInt32 chunkType;
            RwUInt32 chunkLength;
            for (; extLength != 0; extLength += -0xc - chunkLength) {
                RwBool ok;
                RwPluginRegEntry *entry;
                if (!((RwBool(__cdecl *)(RwStream *, RwUInt32 *, RwUInt32 *, RwUInt32 *, RwUInt32 *))0x7ED0F0)(stream, &chunkType, &chunkLength, nullptr, nullptr))
                    return nullptr;
                for (entry = reg->firstRegEntry; entry != nullptr; entry = entry->nextRegEntry) {
                    if (entry->pluginID == chunkType) {
                        if (entry->readCB != nullptr) {
                            ok = (entry->readCB(stream, (RwInt32)chunkLength, object, entry->offset, entry->size) != nullptr);
                            goto processed;
                        }
                        break; // plugin registered but no read CB -> fall through to raw skip
                    }
                }
                ok = (RwStreamSkip(stream, chunkLength) != nullptr);
            processed:
                if (!ok)
                    return nullptr;
            }
            // stream-always callbacks
            for (RwPluginRegEntry *entry = reg->firstRegEntry; entry != nullptr; entry = entry->nextRegEntry) {
                if (entry->alwaysCB != nullptr && !entry->alwaysCB(object, entry->offset, entry->size))
                    return nullptr;
            }
            return reg;
        }
    }
    return nullptr;
}


const RwPluginRegistry *_rwPluginRegistryWriteDataChunks(const RwPluginRegistry *reg, RwStream *stream, const void *object) {
    RwInt32 totalSize = 0;
    RwInt32 plgSize;
    for (RwPluginRegEntry *entry = reg->firstRegEntry; entry != nullptr; entry = entry->nextRegEntry) {
        if (entry->getSizeCB != nullptr) {
            plgSize = entry->getSizeCB(object, entry->offset, entry->size);
            if (0 < plgSize)
                totalSize += 0xc + plgSize;
        }
    }
    if (_rwStreamWriteVersionedChunkHeader(stream, rwID_EXTENSION, totalSize, rwLIBRARYVERSION36003, 0xffff) != nullptr) {
        for (RwPluginRegEntry *entry = reg->firstRegEntry; entry != nullptr; entry = entry->nextRegEntry) {
            if (entry->getSizeCB != nullptr && entry->writeCB != nullptr) {
                plgSize = entry->getSizeCB(object, entry->offset, entry->size);
                if (0 < plgSize) {
                    if (_rwStreamWriteVersionedChunkHeader(stream, (RwInt32)entry->pluginID, plgSize, rwLIBRARYVERSION36003, 0xffff) == nullptr ||
                        entry->writeCB(stream, plgSize, object, entry->offset, entry->size) == nullptr)
                        return nullptr;
                }
            }
        }
        return reg;
    }
    return nullptr;
}


const RwPluginRegistry *_rwPluginRegistrySkipDataChunks(const RwPluginRegistry *reg, RwStream *stream) {
    RwUInt32 extLength;
    if (RwStreamFindChunk(stream, rwID_EXTENSION /* 3 */, &extLength, nullptr)) {
        RwUInt32 chunkLength;
        for (; extLength != 0; extLength += -0xc - chunkLength) {
            if (!((RwBool(__cdecl *)(RwStream *, RwUInt32 *, RwUInt32 *, RwUInt32 *, RwUInt32 *))0x7ED0F0)(stream, nullptr, &chunkLength, nullptr, nullptr))
                return nullptr;
            if (RwStreamSkip(stream, chunkLength) == nullptr)
                return nullptr;
        }
        return reg;
    }
    return nullptr;
}


RwInt32 _rwPluginRegistryGetSize(const RwPluginRegistry *reg, const void *object) {
    RwInt32 totalSize = 0;
    for (RwPluginRegEntry *entry = reg->firstRegEntry; entry != nullptr; entry = entry->nextRegEntry) {
        if (entry->getSizeCB != nullptr) {
            RwInt32 plgSize = entry->getSizeCB(object, entry->offset, entry->size);
            if (0 < plgSize)
                totalSize += 0xc + plgSize; // 0xc = chunk header size
        }
    }
    return totalSize;
}


const RwPluginRegistry *_rwPluginRegistryInvokeRights(const RwPluginRegistry *reg, RwUInt32 id, void *obj, RwUInt32 extraData) {
    RwPluginRegEntry *entry = reg->firstRegEntry;
    while (entry != nullptr) {
        if (entry->pluginID == id)
            break;
        entry = entry->nextRegEntry;
    }
    if (entry == nullptr || entry->rightsCB == nullptr)
        return nullptr;
    RwBool res = entry->rightsCB(obj, entry->offset, entry->size, extraData);
    return (res != FALSE) ? reg : nullptr;
}


RwBool RwEngineGetMatrixTolerances(RwMatrixTolerance * const tolerance) {
    // copies the 3-dword default tolerance from the matrix module globals (offset +0xC)
    const RwMatrixTolerance* def = (const RwMatrixTolerance*)(((RwUInt8*)RwEngineInstance) + StaticRef<RwInt32>(0xC979BC) + 0x0C);
    tolerance->Normal     = def->Normal;
    tolerance->Orthogonal = def->Orthogonal;
    tolerance->Identity   = def->Identity;
    return TRUE;
}


RwBool RwEngineSetMatrixTolerances(const RwMatrixTolerance * const tolerance) {
    RwMatrixTolerance* def = (RwMatrixTolerance*)(((RwUInt8*)RwEngineInstance) + StaticRef<RwInt32>(0xC979BC) + 0x0C);
    def->Normal     = tolerance->Normal;
    def->Orthogonal = tolerance->Orthogonal;
    def->Identity   = tolerance->Identity;
    return TRUE;
}


void RwMatrixSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    StaticRef<RwInt32>(0x8E2290) = blockSize;      // consumed by engine init when creating the matrix RwFreeList
    StaticRef<RwInt32>(0x8E2294) = numBlocksToPrealloc;
}


RwBool RwMatrixDestroy(RwMatrix* mpMat) {
    using FreeListFreeFn = void(__cdecl*)(RwFreeList*, RwMatrix*);
    const FreeListFreeFn freeFn = *(FreeListFreeFn*)(((RwUInt8*)RwEngineInstance) + 0x148);
    RwFreeList* const fl = *(RwFreeList**)(((RwUInt8*)RwEngineInstance) + StaticRef<RwInt32>(0xC979BC));
    freeFn(fl, mpMat);
    return TRUE;
}


RwMatrix* RwMatrixCreate() {
    using FreeListAllocFn = RwMatrix*(__cdecl*)(RwFreeList*, RwUInt32);
    const FreeListAllocFn allocFn = *(FreeListAllocFn*)(((RwUInt8*)RwEngineInstance) + 0x144);
    RwMatrix* const matrix = allocFn(*(RwFreeList**)(((RwUInt8*)RwEngineInstance) + StaticRef<RwInt32>(0xC979BC)),
                                     0x3000D /* memory hint, literal from decompile */);
    if (matrix) {
        matrix->right.x = 1.0f;
        matrix->up.y    = 1.0f;
        matrix->at.z    = 1.0f;
        matrix->right.y = 0.0f;
        matrix->right.z = 0.0f;
        matrix->up.x    = 0.0f;
        matrix->up.z    = 0.0f;
        matrix->at.x    = 0.0f;
        matrix->at.y    = 0.0f;
        matrix->pos.x   = 0.0f;
        matrix->pos.y   = 0.0f;
        matrix->pos.z   = 0.0f;
        matrix->flags   = rwMATRIXINTERNALIDENTITY | rwMATRIXTYPEORTHONORMAL;   // 0x20003
    }
    return matrix;
}


RwMatrix* RwMatrixMultiply(RwMatrix* matrixOut, const RwMatrix* MatrixIn1, const RwMatrix* matrixIn2) {
    RwUInt8* const mgBase = ((RwUInt8*)RwEngineInstance) + StaticRef<RwInt32>(0xC979BC); // matrix module globals
    const RwUInt32 identityCheck = *(RwUInt32*)(mgBase + 4) & rwMATRIXINTERNALIDENTITY;
    const RwUInt32 flags1 = MatrixIn1->flags;
    const RwUInt32 flags2 = matrixIn2->flags;

    if (flags1 & identityCheck) {       // in1 is identity -> result = in2
        *matrixOut = *matrixIn2;
        return matrixOut;
    }
    if (flags2 & identityCheck) {       // in2 is identity -> result = in1
        *matrixOut = *MatrixIn1;
        return matrixOut;
    }

    const rwMatrixMultFn multFn = *(rwMatrixMultFn*)(mgBase + 8);
    multFn(matrixOut, MatrixIn1, matrixIn2);
    matrixOut->flags = flags2 & flags1;
    return matrixOut;
}


RwMatrix* RwMatrixTransform(RwMatrix* matrix, const RwMatrix* transform, RwOpCombineType combineOp) {
    if (combineOp == rwCOMBINEREPLACE) {
        *matrix = *transform;
        return matrix;
    }

    if (combineOp != rwCOMBINEPRECONCAT && combineOp != rwCOMBINEPOSTCONCAT) {
        RwError err;
        err.pluginID  = 1;
        err.errorCode = ((RwInt32(__cdecl*)(RwInt32, const RwChar*))0x8088D0)(0x80000003, "Invalid combination type");
        RwErrorSet(&err);
        return nullptr;
    }

    const RwUInt32 identityCheck = *(RwUInt32*)(((RwUInt8*)RwEngineInstance) + StaticRef<RwInt32>(0xC979BC) + 4) & rwMATRIXINTERNALIDENTITY;
    RwMatrix tmp;
    const rwMatrixMultFn multFn = *(rwMatrixMultFn*)(((RwUInt8*)RwEngineInstance) + StaticRef<RwInt32>(0xC979BC) + 8);

    if (combineOp == rwCOMBINEPRECONCAT) {
        if (transform->flags & identityCheck)   // transform == identity -> matrix unchanged
            return matrix;
        if (matrix->flags & identityCheck) {    // matrix == identity -> result = transform
            *matrix = *transform;
            return matrix;
        }
        multFn(&tmp, transform, matrix);        // tmp = transform * matrix
        tmp.flags = matrix->flags & transform->flags;
    } else { // rwCOMBINEPOSTCONCAT
        if (matrix->flags & identityCheck) {    // matrix == identity -> result = transform
            *matrix = *transform;
            return matrix;
        }
        if (transform->flags & identityCheck)   // transform == identity -> matrix unchanged
            return matrix;
        multFn(&tmp, matrix, transform);        // tmp = matrix * transform
        tmp.flags = matrix->flags & transform->flags;
    }

    *matrix = tmp;
    return matrix;
}


RwMatrix* RwMatrixOrthoNormalize(RwMatrix* matrixOut, const RwMatrix* matrixIn) {
    // scale each row to unit length (invSqrt of squared length)
    RwV3d r = matrixIn->right;
    RwV3d u = matrixIn->up;
    RwV3d a = matrixIn->at;
    const RwReal kR = _rwInvSqrt(r.z * r.z + r.x * r.x + r.y * r.y);
    r.x *= kR; r.y *= kR; r.z *= kR;
    const RwReal kU = _rwInvSqrt(u.z * u.z + u.x * u.x + u.y * u.y);
    u.x *= kU; u.y *= kU; u.z *= kU;
    const RwReal kA = _rwInvSqrt(a.z * a.z + a.x * a.x + a.y * a.y);
    a.x *= kA; a.y *= kA; a.z *= kA;

    // pivot selection (DAT_00858B50 == 0.0f): pick (pfa, pfb, pfc) so that |pfa.pfb| is minimal
    RwV3d *pfa, *pfb, *pfc;
    if (kR <= 0.0f) {
        pfa = &u; pfb = &a; pfc = &r;
    } else if (kU <= 0.0f) {
        pfa = &a; pfb = &r; pfc = &u;
        goto havePermutation;
    } else if (0.0f < kA) {
        RwReal dAU = a.z * u.z + a.x * u.x + a.y * u.y;
        if (dAU < 0.0f) dAU = -dAU;
        RwReal dAR = a.z * r.z + a.x * r.x + a.y * r.y;
        if (dAR < 0.0f) dAR = -dAR;
        RwReal dUR = u.z * r.z + u.x * r.x + u.y * r.y;
        if (dUR < 0.0f) dUR = -dUR;
        if (dAR <= dAU) {
            if (dAR < dUR) { pfa = &a; pfb = &r; pfc = &u; goto havePermutation; }
        } else if (dAU < dUR) {
            pfa = &u; pfb = &a; pfc = &r;
            goto havePermutation;
        }
    } else {
        pfa = &r; pfb = &u; pfc = &a;
        goto havePermutation;
    }
    pfa = &r; pfb = &u; pfc = &a;

havePermutation:;
    // c = normalize(pfa x pfb), written through pfc; one invSqrt shared with the first cross scale
    RwV3d& c = *pfc;
    c.x = pfb->z * pfa->y - pfb->y * pfa->z;
    c.y = pfa->z * pfb->x - pfb->z * pfa->x;
    const RwReal cz0 = pfb->y * pfa->x - pfa->y * pfb->x;
    c.z = cz0;
    const RwReal k1 = _rwInvSqrt(cz0 * cz0 + c.y * c.y + c.x * c.x);
    c.x = c.x * k1;
    const RwReal cy = c.y, cz = c.z;
    c.y = cy * k1;
    c.z = cz * k1;
    // b = (c x pfa) * k1, written through pfb, then re-normalized with a second invSqrt
    RwV3d& b = *pfb;
    b.x = (pfa->z * cy - pfa->y * cz) * k1;
    b.y = c.z * pfa->x - pfa->z * c.x;
    const RwReal bz = pfa->y * c.x - c.y * pfa->x;
    b.z = bz;
    const RwReal k2 = _rwInvSqrt(bz * bz + b.y * b.y + b.x * b.x);
    b.x *= k2;
    b.y *= k2;
    b.z *= k2;

    matrixOut->right = r;
    matrixOut->up    = u;
    matrixOut->at    = a;
    matrixOut->pos   = matrixIn->pos;
    matrixOut->flags = (matrixOut->flags & ~rwMATRIXINTERNALIDENTITY) | rwMATRIXTYPEORTHONORMAL; // (flags & 0xFFFDFFFF) | 3
    return matrixOut;
}


RwMatrix* RwMatrixInvert(RwMatrix* matrixOut, const RwMatrix* matrixIn) {
    const RwUInt32 identityCheck = *(RwUInt32*)(((RwUInt8*)RwEngineInstance) + StaticRef<RwInt32>(0xC979BC) + 4) & rwMATRIXINTERNALIDENTITY;

    if (matrixIn->flags & identityCheck) {          // in is identity
        *matrixOut = *matrixIn;
        return matrixOut;
    }

    if ((matrixIn->flags & rwMATRIXTYPEMASK) == rwMATRIXTYPEORTHONORMAL) {   // (flags & 3) == 3: fast transpose-invert
        matrixOut->right.x = matrixIn->right.x;
        matrixOut->right.y = matrixIn->up.x;
        matrixOut->right.z = matrixIn->at.x;
        matrixOut->up.x    = matrixIn->right.y;
        matrixOut->up.y    = matrixIn->up.y;
        matrixOut->up.z    = matrixIn->at.y;
        matrixOut->at.x    = matrixIn->right.z;
        matrixOut->at.y    = matrixIn->up.z;
        matrixOut->at.z    = matrixIn->at.z;
        matrixOut->pos.x   = -(matrixIn->at.z * matrixIn->pos.z + matrixIn->pos.x * matrixIn->right.x + matrixIn->pos.y * matrixIn->right.y);
        matrixOut->pos.y   = -(matrixIn->pos.z * matrixIn->up.z + matrixIn->pos.y * matrixIn->up.y + matrixIn->pos.x * matrixIn->up.x);
        matrixOut->flags   = rwMATRIXTYPEORTHONORMAL;   // decompile stores integer 3 (4.2039e-45f artifact)
        matrixOut->pos.z   = -(matrixIn->at.z * matrixIn->pos.z + matrixIn->pos.y * matrixIn->at.y + matrixIn->at.x * matrixIn->pos.x);
        return matrixOut;
    }

    ((void(__cdecl*)(RwMatrix*, const RwMatrix*))0x7F2160)(matrixOut, matrixIn);   // general inverse (internal helper)
    return matrixOut;
}


RwMatrix* RwMatrixScale(RwMatrix* matrix, const RwV3d* scale, RwOpCombineType combineOp) {
    if (combineOp == rwCOMBINEREPLACE) {
        // build identity first (exact original order)
        matrix->at.z = 1.0f;
        matrix->up.y = 1.0f;
        matrix->right.x = 1.0f;
        matrix->up.x = 0.0f;
        matrix->flags |= (rwMATRIXINTERNALIDENTITY | rwMATRIXTYPEORTHONORMAL);   // | 0x20003
        matrix->right.z = 0.0f;
        matrix->right.y = 0.0f;
        matrix->at.y = 0.0f;
        matrix->at.x = 0.0f;
        matrix->up.z = 0.0f;
        matrix->pos.z = 0.0f;
        matrix->pos.y = 0.0f;
        matrix->pos.x = 0.0f;
        matrix->right.x = scale->x;
        matrix->up.y = scale->y;
        matrix->at.z = scale->z;
        matrix->flags &= ~(rwMATRIXINTERNALIDENTITY | rwMATRIXTYPEMASK);         // & 0xFFFDFFFC
        return matrix;
    }

    if (combineOp != rwCOMBINEPRECONCAT && combineOp != rwCOMBINEPOSTCONCAT) {
        RwError err;
        err.pluginID  = 1;
        err.errorCode = ((RwInt32(__cdecl*)(RwInt32, const RwChar*))0x8088D0)(0x80000003, "Invalid combination type");
        RwErrorSet(&err);
        return nullptr;
    }

    if (combineOp == rwCOMBINEPRECONCAT) {
        // S * matrix (rows scaled) - exact original order
        matrix->right.x *= scale->x;
        matrix->right.y *= scale->x;
        matrix->right.z *= scale->x;
        matrix->up.x *= scale->y;
        matrix->up.y *= scale->y;
        matrix->up.z *= scale->y;
        matrix->at.x *= scale->z;
        matrix->at.y *= scale->z;
        matrix->at.z *= scale->z;
        matrix->flags &= ~(rwMATRIXINTERNALIDENTITY | rwMATRIXTYPEMASK);         // & 0xFFFDFFFC
        return matrix;
    }

    // rwCOMBINEPOSTCONCAT: matrix * S (columns scaled) - exact original order
    matrix->right.x *= scale->x;
    matrix->right.y *= scale->y;
    matrix->right.z *= scale->z;
    matrix->up.x *= scale->x;
    matrix->up.y *= scale->y;
    matrix->up.z *= scale->z;
    matrix->at.x *= scale->x;
    matrix->at.y *= scale->y;
    matrix->at.z *= scale->z;
    matrix->pos.x *= scale->x;
    matrix->pos.y *= scale->y;
    matrix->pos.z *= scale->z;
    matrix->flags &= ~(rwMATRIXINTERNALIDENTITY | rwMATRIXTYPEMASK);             // & 0xFFFDFFFC
    return matrix;
}


RwMatrix* RwMatrixTranslate(RwMatrix* matrix, const RwV3d* translation, RwOpCombineType combineOp) {
    if (combineOp == rwCOMBINEREPLACE) {
        // build identity first (exact original order)
        matrix->at.z = 1.0f;
        matrix->up.y = 1.0f;
        matrix->right.x = 1.0f;
        matrix->up.x = 0.0f;
        matrix->flags |= (rwMATRIXINTERNALIDENTITY | rwMATRIXTYPEORTHONORMAL);   // | 0x20003
        matrix->right.z = 0.0f;
        matrix->right.y = 0.0f;
        matrix->at.y = 0.0f;
        matrix->at.x = 0.0f;
        matrix->up.z = 0.0f;
        matrix->pos.z = 0.0f;
        matrix->pos.y = 0.0f;
        matrix->pos.x = 0.0f;
        matrix->pos.x = translation->x;
        matrix->pos.y = translation->y;
        matrix->pos.z = translation->z;
        matrix->flags &= ~rwMATRIXINTERNALIDENTITY;                              // & 0xFFFDFFFF (type bits kept)
        return matrix;
    }

    if (combineOp != rwCOMBINEPRECONCAT && combineOp != rwCOMBINEPOSTCONCAT) {
        RwError err;
        err.pluginID  = 1;
        err.errorCode = ((RwInt32(__cdecl*)(RwInt32, const RwChar*))0x8088D0)(0x80000003, "Invalid combination type");
        RwErrorSet(&err);
        return nullptr;
    }

    if (combineOp == rwCOMBINEPRECONCAT) {
        // matrix * T (pos column gets the dotted translation) - exact original order
        matrix->pos.x = matrix->right.x * translation->x + matrix->up.x * translation->y + matrix->at.x * translation->z + matrix->pos.x;
        matrix->pos.y = matrix->right.y * translation->x + matrix->up.y * translation->y + matrix->at.y * translation->z + matrix->pos.y;
        matrix->pos.z = matrix->right.z * translation->x + matrix->up.z * translation->y + matrix->at.z * translation->z + matrix->pos.z;
        matrix->flags &= ~rwMATRIXINTERNALIDENTITY;                              // & 0xFFFDFFFF
        return matrix;
    }

    // rwCOMBINEPOSTCONCAT: pos += t
    matrix->pos.x += translation->x;
    matrix->pos.y += translation->y;
    matrix->pos.z += translation->z;
    matrix->flags &= ~rwMATRIXINTERNALIDENTITY;                                  // & 0xFFFDFFFF
    return matrix;
}


RwMatrix* RwMatrixRotate(RwMatrix* matrix, const RwV3d* axis, RwReal angle, RwOpCombineType combineOp) {
    const RwReal rad = angle * 0.01745329238474369f;  // [0x8595EC] = pi/180
    const RwReal k = _rwInvSqrt(axis->z * axis->z + axis->y * axis->y + axis->x * axis->x);
    const RwV3d unitAxis = { axis->x * k, axis->y * k, axis->z * k };
    const RwReal sine = sinf(rad);
    const RwReal cosine = cosf(rad);
    RwMatrixRotateOneMinusCosineSine(matrix, &unitAxis, 1.0f - cosine, sine, combineOp);
    return matrix;
}


RwMatrix* RwMatrixRotateOneMinusCosineSine(RwMatrix* matrix, const RwV3d* unitAxis, RwReal oneMinusCosine, RwReal sine, RwOpCombineType combineOp) {
    const RwReal x = unitAxis->x, y = unitAxis->y, z = unitAxis->z;
    const RwReal omc = oneMinusCosine, s = sine;

    // Rodrigues build (row-vector convention), exactly as in the decompile
    const RwReal xz_omc = z * x * omc;               // fVar3
    const RwReal xy    = y * x;                      // local_90
    const RwReal yz_omc = y * z * omc;               // local_98
    const RwReal xs = s * x;                         // local_a4
    const RwReal ys = y * s;                         // local_a0

    RwMatrix rot;
    rot.right.x = 1.0f - (1.0f - x * x) * omc;       // local_80[0]
    rot.right.y = z * s + xy * omc;                  // local_80[1]
    rot.right.z = xz_omc - ys;                       // local_80[2]
    rot.flags   = rwMATRIXTYPEORTHONORMAL;           // local_80[3] = 3 (0x20000 identity bit not set)
    rot.up.x    = xy * omc - z * s;                  // local_80[4]
    rot.up.y    = 1.0f - (1.0f - y * y) * omc;       // local_6c
    rot.up.z    = xs + yz_omc;                       // local_68
    rot.at.x    = ys + xz_omc;                       // local_60
    rot.at.y    = yz_omc - xs;                       // local_5c
    rot.at.z    = 1.0f - (1.0f - z * z) * omc;       // local_58
    rot.pos.x   = 0.0f;
    rot.pos.y   = 0.0f;
    rot.pos.z   = 0.0f;

    if (combineOp == rwCOMBINEREPLACE) {
        *matrix = rot;
        return matrix;
    }

    if (combineOp != rwCOMBINEPRECONCAT && combineOp != rwCOMBINEPOSTCONCAT) {
        // error path: RwError{pluginID=1, code=_rwerror-ish(0x80000003, "Invalid combination type")}
        RwError err;
        err.pluginID  = 1;
        err.errorCode = ((RwInt32(__cdecl*)(RwInt32, const RwChar*))0x8088D0)(0x80000003, "Invalid combination type");
        RwErrorSet(&err);
        return nullptr;
    }

    RwUInt8* const mgBase = ((RwUInt8*)RwEngineInstance) + StaticRef<RwInt32>(0xC979BC); // matrix module globals
    const RwUInt32 optIdentity = *(RwUInt32*)(mgBase + 4) & rwMATRIXINTERNALIDENTITY;
    const RwUInt32 oldFlags = matrix->flags;

    RwMatrix tmp;
    if (oldFlags & optIdentity) {
        // target is flagged identity -> result is just the rotation (rot.flags = 3 lands in the matrix)
        tmp = rot;
    } else {
        const rwMatrixMultFn multFn = *(rwMatrixMultFn*)(mgBase + 8);
        if (combineOp == rwCOMBINEPRECONCAT)
            multFn(&tmp, &rot, matrix);              // tmp = rot * matrix
        else
            multFn(&tmp, matrix, &rot);              // tmp = matrix * rot
        tmp.flags = oldFlags & rwMATRIXTYPEMASK;     // uStack_34 = oldFlags & 3
    }
    *matrix = tmp;
    return matrix;
}


const RwMatrix* RwMatrixQueryRotate(const RwMatrix* matrix, RwV3d* unitAxis, RwReal* angle, RwV3d* center) {
    const RwReal traceMinus1 = (matrix->right.x + matrix->up.y + matrix->at.z) - 1.0f;

    // axis of rotation: (up.z - at.y, at.x - right.z, right.y - up.x)
    RwV3d d;
    d.x = matrix->up.z - matrix->at.y;
    d.y = matrix->at.x - matrix->right.z;
    d.z = matrix->right.y - matrix->up.x;
    const RwReal len = RwV3dLength(&d);
    if (len <= 0.0f) {                      // DAT_00858B50
        unitAxis->x = 0.0f;
        unitAxis->y = 0.0f;
        unitAxis->z = 0.0f;
    } else {
        const RwReal k = 1.0f / len;
        unitAxis->x = d.x * k;
        unitAxis->y = d.y * k;
        unitAxis->z = d.z * k;
    }

    *angle = atan2f(len, traceMinus1) * 57.295780181884766f;   // [0x884634] = 180/pi -> degrees

    if (len < 0.009999999776482582f && traceMinus1 < 0.0f) {   // [0x858C58], DAT_00858B50
        ((void(__cdecl*)(const RwMatrix*, RwV3d*))0x7F2930)(matrix, unitAxis);
    }

    // build (I - R) with zero translation, then general-invert it
    RwMatrix mInvI;
    mInvI.right.x = 1.0f - matrix->right.x;
    mInvI.right.y = -matrix->right.y;
    mInvI.right.z = -matrix->right.z;
    mInvI.flags   = 1;                       // rwMATRIXTYPENORMAL (original: 1 & ~0x20000)
    mInvI.up.x    = -matrix->up.x;
    mInvI.up.y    = 1.0f - matrix->up.y;
    mInvI.up.z    = -matrix->up.z;
    mInvI.at.x    = -matrix->at.x;
    mInvI.at.y    = -matrix->at.y;
    mInvI.at.z    = 1.0f - matrix->at.z;
    mInvI.pos.x   = 0.0f;
    mInvI.pos.y   = 0.0f;
    mInvI.pos.z   = 0.0f;

    RwMatrix inv;
    ((void(__cdecl*)(RwMatrix*, const RwMatrix*))0x7F2160)(&inv, &mInvI);

    // center = inv.pos + (pos * inv)   (row-vector transform of matrix->pos by inv)
    center->x = inv.pos.x;
    center->y = inv.pos.y;
    center->z = inv.pos.z;
    center->x += matrix->pos.x * inv.right.x + matrix->pos.y * inv.up.x + matrix->pos.z * inv.at.x;
    center->y += matrix->pos.x * inv.right.y + matrix->pos.y * inv.up.y + matrix->pos.z * inv.at.y;
    center->z += matrix->pos.x * inv.right.z + matrix->pos.y * inv.up.z + matrix->pos.z * inv.at.z;
    return matrix;
}


RwMatrix* RwMatrixUpdate(RwMatrix* matrix) {
    // flags &= 0xFFFDFFFC : clears rwMATRIXINTERNALIDENTITY and both type bits (optimization cache invalidation)
    matrix->flags &= ~(rwMATRIXINTERNALIDENTITY | rwMATRIXTYPEMASK);
    return matrix;
}


RwMatrix* RwMatrixOptimize(RwMatrix* matrix, const RwMatrixTolerance* tolerance) {
    if (!tolerance) {
        // default tolerance = matrix module globals +0xC
        tolerance = (const RwMatrixTolerance*)(((RwUInt8*)RwEngineInstance) + StaticRef<RwInt32>(0xC979BC) + 0x0C);
    }

    const bool bNormalFail = _rwMatrixNormalError(matrix)     > tolerance->Normal;
    const bool bOrthoFail  = _rwMatrixOrthogonalError(matrix) > tolerance->Orthogonal;
    // identity flag survives only if none of the three errors exceeds its tolerance
    const bool bIdFail     = bNormalFail || bOrthoFail || (_rwMatrixIdentityError(matrix) > tolerance->Identity);

    RwUInt32 flags = matrix->flags;
    if (bNormalFail) flags &= ~rwMATRIXTYPENORMAL;     // & 0xFFFFFFFE
    else             flags |=  rwMATRIXTYPENORMAL;     // | 0x1
    if (bOrthoFail)  flags &= ~rwMATRIXTYPEORTHOGONAL; // & 0xFFFFFFFD
    else             flags |=  rwMATRIXTYPEORTHOGONAL; // | 0x2
    if (!bIdFail)    flags |=  rwMATRIXINTERNALIDENTITY;   // | 0x20000
    else             flags &= ~rwMATRIXINTERNALIDENTITY;   // & 0xFFFDFFFF
    matrix->flags = flags;
    return matrix;
}


RwReal _rwMatrixDeterminant(const RwMatrix* matrix) {
    // Original computes in x87 80-bit (float10); double intermediates here.
    // Decompile terms: m[2]*(m[9]*m[4] - m[5]*m[8]) + (m[10]*m[5] - m[9]*m[6])*m[0] + m[1]*(m[6]*m[8] - m[10]*m[4])
    // (vendor RwMatrix float-indexing: right 0,1,2 / up 4,5,6 / at 8,9,10)
    const double rx = matrix->right.x, ry = matrix->right.y, rz = matrix->right.z;
    const double ux = matrix->up.x,   uy = matrix->up.y,   uz = matrix->up.z;
    const double ax = matrix->at.x,   ay = matrix->at.y,   az = matrix->at.z;
    return (RwReal)(rz * (ay * ux - uy * ax) + (az * uy - ay * uz) * rx + ry * (uz * ax - az * ux));
}


RwReal _rwMatrixNormalError(const RwMatrix* matrix) {
    // x87 80-bit in original -> double intermediates
    const double eR = (double)matrix->right.x * matrix->right.x
                    + (double)matrix->right.y * matrix->right.y
                    + (double)matrix->right.z * matrix->right.z - 1.0;
    const double eU = (double)matrix->up.x * matrix->up.x
                    + (double)matrix->up.y * matrix->up.y
                    + (double)matrix->up.z * matrix->up.z - 1.0;
    const double eA = (double)matrix->at.x * matrix->at.x
                    + (double)matrix->at.y * matrix->at.y
                    + (double)matrix->at.z * matrix->at.z - 1.0;
    return (RwReal)(eR * eR + eU * eU + eA * eA);
}


RwReal _rwMatrixOrthogonalError(const RwMatrix* matrix) {
    const double dUA = (double)matrix->up.x   * matrix->at.x + (double)matrix->up.y   * matrix->at.y + (double)matrix->up.z   * matrix->at.z;
    const double dRA = (double)matrix->right.x * matrix->at.x + (double)matrix->right.y * matrix->at.y + (double)matrix->right.z * matrix->at.z;
    const double dRU = (double)matrix->right.x * matrix->up.x + (double)matrix->right.y * matrix->up.y + (double)matrix->right.z * matrix->up.z;
    return (RwReal)(dUA * dUA + dRA * dRA + dRU * dRU);
}


RwReal _rwMatrixIdentityError(const RwMatrix* matrix) {
    // block 1: right row
    double e = (double)(matrix->right.y) * matrix->right.y
             + (double)(matrix->right.z) * matrix->right.z
             + (double)(matrix->right.x - 1.0f) * (matrix->right.x - 1.0f);
    // block 2: pos
    e += (double)matrix->pos.x * matrix->pos.x
       + (double)matrix->pos.y * matrix->pos.y
       + (double)matrix->pos.z * matrix->pos.z;
    // block 3: at row
    e += (double)matrix->at.x * matrix->at.x
       + (double)matrix->at.y * matrix->at.y
       + (double)(matrix->at.z - 1.0f) * (matrix->at.z - 1.0f);
    // block 4: up row
    e += (double)matrix->up.x * matrix->up.x
       + (double)matrix->up.z * matrix->up.z
       + (double)(matrix->up.y - 1.0f) * (matrix->up.y - 1.0f);
    return (RwReal)e;
}


RwReal RwV3dNormalize(RwV3d* out, const RwV3d* in)
{
    RwReal lenSq = in->z * in->z + in->y * in->y + in->x * in->x;
    RwReal len = 0.0f;
    RwReal recip = lenSq; // при lenSq == 0 масштаб = 0 (декомпил: param_2 = lenSq)
    if (lenSq != 0.0f) {
        len = _rwSqrt(lenSq);
        recip = _rwInvSqrt(lenSq);
    }
    out->x = recip * in->x;
    out->y = in->y * recip;
    out->z = in->z * recip;
    if (len < StaticRef<RwReal>(0x858B50) /* 0.0f */) {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x19);
        RwErrorSet(&rwError);
    }
    return len;
}


RwReal RwV3dLength(const RwV3d* in)
{
    RwReal lenSq = in->z * in->z + in->y * in->y + in->x * in->x;
    if (lenSq != 0.0f)
        return _rwSqrt(lenSq);
    return lenSq; // 0.0f
}


RwReal RwV2dLength(const RwV2d* in)
{
    RwReal lenSq = in->y * in->y + in->x * in->x;
    if (lenSq != 0.0f)
        return _rwSqrt(lenSq);
    return lenSq; // 0.0f
}


RwReal RwV2dNormalize(RwV2d* out, const RwV2d* in)
{
    RwReal lenSq = in->y * in->y + in->x * in->x;
    RwReal len = 0.0f;
    RwReal recip = 0.0f;
    if (lenSq != 0.0f) {
        len = _rwSqrt(lenSq);
        recip = _rwInvSqrt(lenSq);
    }
    out->x = recip * in->x;
    out->y = in->y * recip;
    if (len < StaticRef<RwReal>(0x858B50) /* 0.0f */) {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x19);
        RwErrorSet(&rwError);
    }
    return len;
}


RwV3d* RwV3dTransformPoint(RwV3d* pointOut, const RwV3d* pointIn, const RwMatrix* matrix)
{
    RwUInt8* mathGlobals = reinterpret_cast<RwUInt8*>(RwEngineInstance) + *(RwUInt32*)0xC97934;
    reinterpret_cast<RwV3d*(__cdecl*)(RwV3d*, const RwV3d*, const RwMatrix*)>(*reinterpret_cast<RwUInt32*>(mathGlobals + 0x08))(
        pointOut, pointIn, matrix);
    return pointOut;
}


RwV3d* RwV3dTransformPoints(RwV3d* pointsOut, const RwV3d* pointsIn, RwInt32 numPoints, const RwMatrix* matrix)
{
    RwUInt8* mathGlobals = reinterpret_cast<RwUInt8*>(RwEngineInstance) + *(RwUInt32*)0xC97934;
    reinterpret_cast<RwV3d*(__cdecl*)(RwV3d*, const RwV3d*, RwInt32, const RwMatrix*)>(*reinterpret_cast<RwUInt32*>(mathGlobals + 0x0C))(
        pointsOut, pointsIn, numPoints, matrix);
    return pointsOut;
}


RwV3d* RwV3dTransformVector(RwV3d* vectorOut, const RwV3d* vectorIn, const RwMatrix* matrix)
{
    RwUInt8* mathGlobals = reinterpret_cast<RwUInt8*>(RwEngineInstance) + *(RwUInt32*)0xC97934;
    reinterpret_cast<RwV3d*(__cdecl*)(RwV3d*, const RwV3d*, const RwMatrix*)>(*reinterpret_cast<RwUInt32*>(mathGlobals + 0x10))(
        vectorOut, vectorIn, matrix);
    return vectorOut;
}


RwV3d* RwV3dTransformVectors(RwV3d* vectorsOut, const RwV3d* vectorsIn, RwInt32 numPoints, const RwMatrix* matrix) {
    // Original: single indirect call through a function-pointer slot in the "vector" module globals:
    //   fn = *(code**)(RwEngineInstance + *(RwInt32*)0xC97934 + 0x14); fn(out, in, n, m);
    using TransformVectorsFn = RwV3d*(__cdecl*)(RwV3d*, const RwV3d*, RwInt32, const RwMatrix*);
    const TransformVectorsFn fn = *(TransformVectorsFn*)(((RwUInt8*)RwEngineInstance) + StaticRef<RwInt32>(0xC97934) + 0x14);
    fn(vectorsOut, vectorsIn, numPoints, matrix);
    return vectorsOut;
}


RwReal _rwSqrt(const RwReal num)
{
    RwReal result = num;
    if (num != 0.0f) {
        RwUInt32 f = *reinterpret_cast<const RwUInt32*>(&num);
        RwUInt8* mathGlobals = reinterpret_cast<RwUInt8*>(RwEngineInstance) + *(RwUInt32*)0xC97934;
        RwUInt32* sqrtTable = *reinterpret_cast<RwUInt32**>(mathGlobals + 0x00);
        RwUInt32 bits = sqrtTable[(f + 0x800) >> 0xC & 0xFFF] + ((f + 0x800) >> 1 & 0x3FC00000);
        result = *reinterpret_cast<RwReal*>(&bits);
    }
    return result;
}


RwReal _rwInvSqrt(const RwReal num)
{
    RwReal result = num;
    if (num != 0.0f) {
        RwUInt32 f = *reinterpret_cast<const RwUInt32*>(&num);
        RwUInt8* mathGlobals = reinterpret_cast<RwUInt8*>(RwEngineInstance) + *(RwUInt32*)0xC97934;
        RwUInt32* invSqrtTable = *reinterpret_cast<RwUInt32**>(mathGlobals + 0x04);
        RwUInt32 bits = invSqrtTable[(f + 0x800) >> 0xC & 0xFFF] + (~(f + 0x800) >> 1 & 0x3FC00000);
        result = *reinterpret_cast<RwReal*>(&bits);
    }
    return result;
}


RwReal _rwV3dNormalize(RwV3d* out, const RwV3d* in)
{
    RwReal lenSq = in->z * in->z + in->y * in->y + in->x * in->x;
    RwReal recip = lenSq; // при lenSq == 0 масштаб = 0 (декомпил: param_2 = lenSq)
    if (lenSq != 0.0f)
        recip = _rwInvSqrt(lenSq);
    out->x = recip * in->x;
    out->y = in->y * recip;
    out->z = in->z * recip;
    return recip;
}


RwSList *_rwSListCreate(RwInt32 size, RwUInt32 hint) {
    RwSList *sList = (RwSList *)RwGetMemFuncsGta()->rwmalloc(0x10 /* sizeof(RwSList) */, rwMEMHINTDUR_EVENT);
    if (sList == nullptr) {
        RwError err;
        err.pluginID = 1;
        err.errorCode = _rwerror(0x80000013, 0x10);
        RwErrorSet(&err);
        return nullptr;
    }
    sList->entrySize          = size;
    sList->numElementsFilled  = 0;
    sList->numElementsAlloced = 0x14;
    sList->listElements = (RwUInt8 *)RwGetMemFuncsGta()->rwmalloc((size_t)(size * 0x14), hint | rwMEMHINTFLAG_RESIZABLE);
    if (sList->listElements == nullptr) {
        RwError err;
        err.pluginID = 1;
        err.errorCode = _rwerror(0x80000013, sList->numElementsAlloced * sList->entrySize);
        RwErrorSet(&err);
        RwGetMemFuncsGta()->rwfree(sList);
        return nullptr;
    }
    return sList;
}


RwBool _rwSListDestroy(RwSList* sList) {
    /* decompile @0x809440: free the element block (RwGlobals+0x138 slot), then the list itself */
    RwGlobalsGta* eng = (RwGlobalsGta*)RwEngineInstance;

    if (sList->listElements != nullptr && sList->numElementsAlloced != 0) {
        eng->memoryFree(sList->listElements);
        sList->listElements       = nullptr;
        sList->numElementsAlloced = 0;
    }
    eng->memoryFree(sList);
    return TRUE;
}


void _rwSListDestroyEndEntries(RwSList *sList, RwInt32 amount) {
    sList->numElementsFilled -= amount;
}


void* _rwSListGetEntry(RwSList* sList, RwInt32 entry) {
    /* decompile @0x809510: listElements + entrySize * entry */
    return sList->listElements + sList->entrySize * entry;
}


void *_rwSListGetNewEntry(RwSList *sList, RwUInt32 hint) {
    if (sList->numElementsAlloced <= sList->numElementsFilled) {
        RwInt32 curAlloced = sList->numElementsAlloced;
        RwInt32 newAlloced = curAlloced + curAlloced / 4;
        RwUInt8 *newData = (RwUInt8 *)RwGetMemFuncsGta()->rwrealloc(sList->listElements, (size_t)(newAlloced * sList->entrySize), hint);
        if (newData == nullptr) {
            RwError err;
            err.pluginID = 1;
            err.errorCode = _rwerror(0x80000013, newAlloced * sList->entrySize);
            RwErrorSet(&err);
            return nullptr;
        }
        sList->listElements = newData;
        sList->numElementsAlloced = newAlloced;
    }
    RwInt32 index = sList->numElementsFilled++;
    return sList->listElements + sList->entrySize * index;
}


RwInt32 _rwSListGetNumEntries(const RwSList* sList) {
    /* decompile @0x8094B0: *(int*)(sList + 4) == RwSList::numElementsFilled (rwplcore.h:4968) */
    return sList->numElementsFilled;
}


void* _rwSListGetBegin(RwSList* sList) {
    /* decompile @0x809530: *(int*)(sList + 0) == RwSList::listElements */
    return sList->listElements;
}


void* _rwSListGetEnd(RwSList* sList) {
    /* decompile @0x809540: listElements + entrySize * numElementsFilled (one-past-last element) */
    return sList->listElements + sList->entrySize * sList->numElementsFilled;
}


RwUInt32 RwEngineGetVersion() {
    return 0x36003;   /* RW 3.6.0.3 binary/version stamp baked into the vendor build */
}


RwBool RwEngineInit(const RwMemoryFunctions* memFuncs, RwUInt32 initFlags, RwUInt32 resArenaSize) {
    RwBool useFreeLists = (initFlags & rwENGINEINITNOFREELISTS) == 0;

    RwEngineInstance = reinterpret_cast<RwGlobals*>(&rwGlobalsStatic);
    RwGlobalsSA* engine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);
    if (useFreeLists) {
        engine->memoryAlloc = (RwMemoryAllocFn)0x801C30;
        engine->memoryFree  = (RwMemoryFreeFn)0x801D50;
    } else {
        engine->memoryAlloc = (RwMemoryAllocFn)0x7F3490;
        engine->memoryFree  = (RwMemoryFreeFn)0x7F34B0;
    }
    ((void(__cdecl*)(RwBool))0x801970)(useFreeLists);
    engine->resArenaInitSize = resArenaSize;

    if (engine->engineStatus == rwENGINESTATUSIDLE) {
        if (_rwStringOpen()) {
            if (((RwBool(__cdecl*)(const RwMemoryFunctions*))0x801FD0)(memFuncs)) {
                if (((RwBool(__cdecl*)(void))0x804140)()) {
                    if (((RwBool(__cdecl*)(void))0x8082E0)()) {
                        RwPluginRegistry* coreReg = &rwCorePluginRegistry;
                        RwInt32 registered =
                            _rwPluginRegistryAddPlugin(coreReg, 0x008, 0x40f, (RwPluginObjectConstructor)0x8087D0, (RwPluginObjectDestructor)0x808810, nullptr);
                        registered |= _rwPluginRegistryAddPlugin(coreReg, 0x018, 0x401, (RwPluginObjectConstructor)0x7EDE90, (RwPluginObjectDestructor)0x7EDE20, nullptr);
                        registered |= _rwPluginRegistryAddPlugin(coreReg, 0x000, 0x40d, (RwPluginObjectConstructor)0x80AA40, (RwPluginObjectDestructor)0x80AA50, nullptr);
                        registered |= _rwPluginRegistryAddPlugin(coreReg, 0x018, 0x402, (RwPluginObjectConstructor)0x7F16C0, (RwPluginObjectDestructor)0x7F1660, nullptr);
                        registered |= _rwPluginRegistryAddPlugin(coreReg, 0x004, 0x403, (RwPluginObjectConstructor)0x7EFEF0, (RwPluginObjectDestructor)0x7EFF70, nullptr);
                        registered |= _rwPluginRegistryAddPlugin(coreReg, 0x004, 0x404, (RwPluginObjectConstructor)0x7EC780, (RwPluginObjectDestructor)0x7EC7E0, nullptr);
                        registered |= _rwPluginRegistryAddPlugin(coreReg, 0x004, 0x405, (RwPluginObjectConstructor)0x7EE110, (RwPluginObjectDestructor)0x7EE0B0, nullptr);
                        registered |= _rwPluginRegistryAddPlugin(coreReg, 0x220, 0x406, (RwPluginObjectConstructor)0x802280, (RwPluginObjectDestructor)0x8024E0, nullptr);
                        registered |= _rwPluginRegistryAddPlugin(coreReg, 0x064, 0x407, (RwPluginObjectConstructor)0x7FB370, (RwPluginObjectDestructor)0x7FB310, nullptr);
                        registered |= _rwPluginRegistryAddPlugin(coreReg, 0x034, 0x408, (RwPluginObjectConstructor)0x7F3EA0, (RwPluginObjectDestructor)0x7F3D00, nullptr);
                        registered |= _rwPluginRegistryAddPlugin(coreReg, 0x060, 0x409, (RwPluginObjectConstructor)0x807C40, (RwPluginObjectDestructor)0x807C60, nullptr);
                        registered |= _rwPluginRegistryAddPlugin(coreReg, 0x004, 0x412, (RwPluginObjectConstructor)0x80A780, (RwPluginObjectDestructor)0x80A7E0, nullptr);
                        registered |= ((RwInt32(__cdecl*)(void))0x807C70)();
                        registered |= _rwPluginRegistryAddPlugin(coreReg, 0x074, 0x40a, (RwPluginObjectConstructor)0x7EFE20, (RwPluginObjectDestructor)0x7EFDE0, nullptr);
                        registered |= _rwPluginRegistryAddPlugin(coreReg, 0x028, 0x40b, (RwPluginObjectConstructor)0x807C90, (RwPluginObjectDestructor)0x807D80, nullptr);

                        RwBool deviceRegistered = _rwDeviceRegisterPlugin();
                        if ((registered >= 0) && deviceRegistered) {
                            engine->engineStatus = rwENGINESTATUSINITED;
                            return deviceRegistered;
                        }
                        ((void(__cdecl*)(void))0x808320)();
                    }
                    ((void(__cdecl*)(void))0x804240)();
                }
                ((void(__cdecl*)(void))0x8020F0)();
            }
            _rwStringClose();
        }
    }
    return FALSE;
}


RwInt32 RwEngineRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor initCB, RwPluginObjectDestructor termCB) {
    // original: FUN_008084a0(&DAT_008E2298, size, pluginID, initCB, termCB, NULL)
    return _rwPluginRegistryAddPlugin(&StaticRef<RwPluginRegistry>(0x8E2298), size, pluginID, initCB, termCB, nullptr);
}


RwInt32 RwEngineGetPluginOffset(RwUInt32 pluginID) {
    // original: FUN_00808470(&DAT_008E2298, pluginID)
    return _rwPluginRegistryGetPluginOffset(&StaticRef<RwPluginRegistry>(0x8E2298), pluginID);
}


RwBool RwEngineOpen(RwEngineOpenParams* initParams) {
    RwBool(__cdecl* engineSystem)(void*, RwInt32, void*, void*, RwInt32) = (RwBool(__cdecl*)(void*, RwInt32, void*, void*, RwInt32))0x7F2AB0;
    RwError error;

    if (RwEngineInstance == nullptr) {
        RwEngineInstance = reinterpret_cast<RwGlobals*>(&rwGlobalsStatic);
    }
    RwGlobalsSA* engine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);
    if (engine->engineStatus != rwENGINESTATUSINITED) {
        error.pluginID = 1;
        error.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000001);
        RwErrorSet(&error);
        return FALSE;
    }
    if (initParams == nullptr) {
        error.pluginID = 1;
        error.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000016);
        RwErrorSet(&error);
        return FALSE;
    }

    RwSystemFunc systemFn = ((RwSystemFunc(__cdecl*)(void))0x7F9C20)();
    if (systemFn == nullptr) {
        return FALSE;
    }

    RwGlobalsSA* newEngine = static_cast<RwGlobalsSA*>(engine->memoryFuncs.rwmalloc(rwCorePluginRegistry.sizeOfStruct, 0x40000));
    if (newEngine == nullptr) {
        RwEngineInstance = reinterpret_cast<RwGlobals*>(&rwGlobalsStatic);
        error.pluginID = 1;
        error.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, rwCorePluginRegistry.sizeOfStruct);
        RwErrorSet(&error);
        return FALSE;
    }

    memcpy(newEngine, &rwGlobalsStatic, sizeof(RwGlobalsSA) /* 0x158 = 0x56 dwords */);
    RwEngineInstance = reinterpret_cast<RwGlobals*>(newEngine);

    // ORIGINAL 0x7F2F70: dispatch arg1 is the DEVICE STRUCT (0x7F9C20 -> &0x8E2498), NOT &engine->dOpenDevice.
    // engine->dOpenDevice (arg3) is merely the registration DESTINATION.
    void* rwDevice = reinterpret_cast<void*>(systemFn);
    engineSystem(rwDevice, rwDEVICESYSTEMREGISTER, &newEngine->dOpenDevice, &newEngine->memoryFuncs, 0);
    if (engineSystem(rwDevice, rwDEVICESYSTEMOPEN, nullptr, initParams, 0)) {
        engineSystem(rwDevice, rwDEVICESYSTEMSTANDARDS, &newEngine->stdFunc[0], nullptr, 0x1d);
        rwEngineOpenCount++;
        newEngine->engineStatus = rwENGINESTATUSOPENED;
        return TRUE;
    }

    RwGlobalsSA* failedEngine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);
    RwEngineInstance = reinterpret_cast<RwGlobals*>(&rwGlobalsStatic);
    memcpy(&rwGlobalsStatic, failedEngine, sizeof(RwGlobalsSA));
    rwGlobalsStatic.memoryFuncs.rwfree(failedEngine);
    return FALSE;
}


RwBool RwEngineStart() {
    RwGlobalsSA* engine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);
    RwBool(__cdecl* engineSystem)(void*, RwInt32, void*, void*, RwInt32) = (RwBool(__cdecl*)(void*, RwInt32, void*, void*, RwInt32))0x7F2AB0;
    void* rwDevice = ((void*(__cdecl*)(void))0x7F9C20)();   // device struct &0x8E2498 (original dispatch arg1)

    if (engineSystem(rwDevice, rwDEVICESYSTEMSTART, nullptr, nullptr, 0)) {
        if (_rwPluginRegistryInitObject(&rwCorePluginRegistry, RwEngineInstance)) {
            RwImageSetGamma(engine->dOpenDevice.gammaCorrection);
            engineSystem(rwDevice, rwDEVICESYSTEMFINALIZESTART, nullptr, nullptr, 0);
            engine->engineStatus = rwENGINESTATUSSTARTED;
            return TRUE;
        }
        engineSystem(rwDevice, rwDEVICESYSTEMSTOP, nullptr, nullptr, 0);
    }
    return FALSE;
}


RwBool RwEngineStop() {
    RwGlobalsSA* engine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);
    RwBool(__cdecl* engineSystem)(void*, RwInt32, void*, void*, RwInt32) = (RwBool(__cdecl*)(void*, RwInt32, void*, void*, RwInt32))0x7F2AB0;
    void* rwDevice = ((void*(__cdecl*)(void))0x7F9C20)();   // device struct &0x8E2498

    engineSystem(rwDevice, rwDEVICESYSTEMINITIATESTOP, nullptr, nullptr, 0);
    _rwPluginRegistryDeInitObject(&rwCorePluginRegistry, RwEngineInstance);
    RwBool stopped = engineSystem(rwDevice, rwDEVICESYSTEMSTOP, nullptr, nullptr, 0);
    if (stopped) {
        engine->engineStatus = rwENGINESTATUSOPENED;
    }
    return stopped;
}


RwBool RwEngineClose() {
    RwGlobalsSA* engine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);
    RwBool closed = ((RwBool(__cdecl*)(void*, RwInt32, void*, void*, RwInt32))0x7F2AB0)(((void*(__cdecl*)(void))0x7F9C20)(), rwDEVICESYSTEMCLOSE, nullptr, nullptr, 0);
    if (closed) {
        RwGlobalsSA* oldEngine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);
        RwEngineInstance = reinterpret_cast<RwGlobals*>(&rwGlobalsStatic);
        memcpy(&rwGlobalsStatic, oldEngine, sizeof(RwGlobalsSA) /* 0x158 = 0x56 dwords */);
        rwGlobalsStatic.memoryFuncs.rwfree(oldEngine);
        rwEngineOpenCount--;
        rwGlobalsStatic.engineStatus = rwENGINESTATUSINITED;
    }
    return closed;
}


RwBool RwEngineTerm() {
    RwBool terminated = (rwEngineOpenCount == 0);
    if (terminated) {
        ((void(__cdecl*)(void))0x808320)();
        ((void(__cdecl*)(void))0x804240)();
        ((void(__cdecl*)(void))0x8020F0)();
        reinterpret_cast<RwGlobalsSA*>(RwEngineInstance)->engineStatus = rwENGINESTATUSIDLE;
    }
    return terminated;
}


RwInt32 RwEngineGetNumSubSystems() {
    RwInt32 numSubSystems = 1;   // original default
    ((void(__cdecl*)(void*, RwInt32, RwInt32*, RwUInt32, RwInt32))0x7F2AB0)(
        ((void*(__cdecl*)(void))0x7F9C20)(),   // device struct &0x8E2498 (original dispatch arg1)
        0xD,
        &numSubSystems,
        0,
        0);
    return numSubSystems;
}


RwSubSystemInfo* RwEngineGetSubSystemInfo(RwSubSystemInfo* subSystemInfo, RwInt32 subSystemIndex) {
    const RwInt32 result = ((RwInt32(__cdecl*)(void*, RwInt32, RwSubSystemInfo*, RwUInt32, RwInt32))0x7F2AB0)(
        &RwEngineInstance->dOpenDevice,   // RwEngineInstance + 0x10
        0xE,
        subSystemInfo,
        0,
        subSystemIndex);
    return result ? subSystemInfo : nullptr;
}


RwInt32 RwEngineGetCurrentSubSystem() {
    RwGlobalsSA* engine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);
    RwInt32 curSubSystem = -1;
    if (((RwBool(__cdecl*)(void*, RwInt32, void*, void*, RwInt32))0x7F2AB0)(&engine->dOpenDevice, rwDEVICESYSTEMGETCURRENTSUBSYSTEM, &curSubSystem, nullptr, 0)) {
        return curSubSystem;
    }
    return -1;
}


RwBool RwEngineSetSubSystem(RwInt32 subSystemIndex) {
    RwGlobalsSA* engine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);
    return ((RwBool(__cdecl*)(void*, RwInt32, void*, void*, RwInt32))0x7F2AB0)(&engine->dOpenDevice, rwDEVICESYSTEMSETSUBSYSTEM, nullptr, nullptr, subSystemIndex);
}


RwInt32 RwEngineGetNumVideoModes() {
    RwGlobalsSA* engine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);
    RwInt32 numModes = -1;
    if (((RwBool(__cdecl*)(void*, RwInt32, void*, void*, RwInt32))0x7F2AB0)(&engine->dOpenDevice, rwDEVICESYSTEMGETNUMMODES, &numModes, nullptr, 0)) {
        return numModes;
    }
    return -1;
}


RwVideoMode* RwEngineGetVideoModeInfo(RwVideoMode* vmi, RwInt32 modeIndex) {
    return plugin::CallAndReturn<RwVideoMode*, 0x7F2CF0>(vmi, modeIndex);
}

RwInt32 RwEngineGetCurrentVideoMode() {
    RwGlobalsSA* engine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);
    RwInt32 curMode = -1;
    if (((RwBool(__cdecl*)(void*, RwInt32, void*, void*, RwInt32))0x7F2AB0)(&engine->dOpenDevice, rwDEVICESYSTEMGETMODE, &curMode, nullptr, 0)) {
        return curMode;
    }
    return -1;
}


RwBool RwEngineSetVideoMode(RwInt32 modeIndex) {
    RwGlobalsSA* engine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);
    return ((RwBool(__cdecl*)(void*, RwInt32, void*, void*, RwInt32))0x7F2AB0)(&engine->dOpenDevice, rwDEVICESYSTEMUSEMODE, nullptr, nullptr, modeIndex);
}


RwInt32 RwEngineGetTextureMemorySize() {
    RwGlobalsSA* engine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);
    RwInt32 texMemSize = -1;
    if (((RwBool(__cdecl*)(void*, RwInt32, void*, void*, RwInt32))0x7F2AB0)(&engine->dOpenDevice, rwDEVICESYSTEMGETTEXMEMSIZE, &texMemSize, nullptr, 0)) {
        return texMemSize;
    }
    return -1;
}


RwInt32 RwEngineGetMaxTextureSize() {
    RwGlobalsSA* engine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);
    RwInt32 maxTextureSize = -1;
    if (((RwBool(__cdecl*)(void*, RwInt32, void*, void*, RwInt32))0x7F2AB0)(&engine->dOpenDevice, rwDEVICESYSTEMGETMAXTEXTURESIZE, &maxTextureSize, nullptr, 0)) {
        return maxTextureSize;
    }
    return -1;
}


RwBool RwEngineSetFocus(RwBool enable) {
    RwGlobalsSA* engine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);
    return ((RwBool(__cdecl*)(void*, RwInt32, void*, void*, RwInt32))0x7F2AB0)(&engine->dOpenDevice, rwDEVICESYSTEMFOCUS, nullptr, nullptr, enable);
}


RwMetrics* RwEngineGetMetrics() {
    return reinterpret_cast<RwGlobalsSA*>(RwEngineInstance)->metrics;
}


//RwFileFunctions* RwOsGetFileInterface() {
//    return ((RwFileFunctions*(__cdecl *)(void))0x804130)();
//}
RwFileFunctions* RwOsGetFileInterface() {
    return &reinterpret_cast<RwGlobalsSA*>(RwEngineInstance)->fileFuncs;
}

RwError *RwErrorGet(RwError *code) {
    RwError *lastError = RwGetLastErrorGta();
    code->pluginID = lastError->pluginID;
    code->errorCode = lastError->errorCode;
    lastError->pluginID = 0;
    lastError->errorCode = (RwInt32)0x80000000; // reset to E_RW_NOERROR state
    return code;
}


RwError *RwErrorSet(RwError *code) {
    RwError *lastError = RwGetLastErrorGta();
    if ((lastError->pluginID == 0) && (lastError->errorCode == (RwInt32)0x80000000)) {
        if ((code->errorCode & 0x80000000) == 0)
            lastError->pluginID = code->pluginID;
        else
            lastError->pluginID = 0;
        lastError->errorCode = code->errorCode;
        return code;
    }
    return code;
}


RwInt32 _rwerror(RwInt32 code, ...) {
    return code;
}

RwBool RwResourcesSetArenaSize(RwUInt32 size) {
    if (g_rwResourcesModuleNumInstances == 0) {
        // engine not open yet: just remember the init size
        *(RwUInt32 *)((RwUInt8 *)RwEngineInstance + 0x154) = size;
    } else {
        rwResources *res = RwGetResourcesGta();
        res->maxSize = (RwInt32)size;
        RwResourcesEmptyArena();
        ((void(__cdecl *)(void *))0x810030)(res->memHeap); // destroy old arena block manager
        RwGetMemFuncsGta()->rwfree(res->memHeap);
        void *newHeap = RwGetMemFuncsGta()->rwmalloc((size_t)size, 0x3040b);
        res->memHeap = newHeap;
        if (newHeap == nullptr) {
            res->maxSize = 0;
            RwError err;
            err.pluginID = 1;
            err.errorCode = _rwerror(0x80000013);
            RwErrorSet(&err);
            return FALSE;
        }
        if (!((RwBool(__cdecl *)(void *, RwUInt32))0x80FFE0)(newHeap, size)) { // init new arena block manager
            RwGetMemFuncsGta()->rwfree(res->memHeap);
            RwError err;
            err.pluginID = 1;
            err.errorCode = _rwerror(0xc, 0);
            RwErrorSet(&err);
            return FALSE;
        }
    }
    return TRUE;
}


RwInt32 RwResourcesGetArenaSize() {
    return RwGetResourcesGta()->maxSize; // *(engine + resGlobalsOffset + 0x24 + 0x00)
}


RwInt32 RwResourcesGetArenaUsage() {
    rwResources *res = RwGetResourcesGta();
    RwInt32 usage = res->reusageSize;
    RwLinkList *head = res->usedEntries; // *(res + 0x24)
    for (RwLLLink *lnk = head->link.next; lnk != &head->link; lnk = lnk->next) {
        usage += rwLLLinkGetData(lnk, RwResEntry, link)->size;
    }
    return usage;
}


RwBool RwResourcesEmptyArena() {
    rwResources *res = RwGetResourcesGta();
    res->entriesA.link.prev->next = res->entriesB.link.next; // join A-tail to B-first
    RwLLLink *headB = &res->entriesB.link;
    for (RwLLLink *lnk = res->entriesA.link.next; lnk != headB;) {
        RwLLLink *next = lnk->next;
        RwResEntry *entry = rwLLLinkGetData(lnk, RwResEntry, link);
        if (entry->destroyNotify != nullptr)
            entry->destroyNotify(entry);
        if (entry->ownerRef != nullptr)
            *entry->ownerRef = nullptr;
        if (entry->link.next == nullptr) {
            RwGetMemFuncsGta()->rwfree(entry);
        } else {
            entry->link.prev->next = entry->link.next;
            entry->link.next->prev = entry->link.prev;
            res->currentSize -= entry->size;
            ((void(__cdecl *)(void))0x810040)();
        }
        lnk = next;
    }
    res->entriesA.link.next = &res->entriesA.link;
    res->entriesA.link.prev = &res->entriesA.link;
    res->entriesB.link.next = &res->entriesB.link;
    res->entriesB.link.prev = &res->entriesB.link;
    res->reusageSize = 0;
    return TRUE;
}


RwResEntry *RwResourcesAllocateResEntry(void *owner, RwResEntry **ownerRef, RwInt32 size, RwResEntryDestroyNotify destroyNotify) {
    rwResources *res = RwGetResourcesGta();
    while (true) {
        while (true) {
            RwResEntry *entry = ((RwResEntry *(__cdecl *)(void *, RwInt32))0x8100C0)(res->memHeap, size + 0x18 /* sizeof(RwResEntry) */);
            if (entry != nullptr) {
                RwLinkList *head = res->usedEntries; // *(res + 0x24)
                entry->link.next = head->link.next;
                entry->link.prev = &head->link;
                head->link.prev = &entry->link;
                head->link.next = &entry->link;
                entry->owner = owner;
                entry->size = size;
                entry->ownerRef = ownerRef;
                entry->destroyNotify = destroyNotify;
                res->currentSize += size;
                if (ownerRef != nullptr)
                    *ownerRef = entry;
                return entry;
            }
            // arena full: free entries off the *freeEntries(+0x20) list, one per retry (from the tail)
            RwLinkList *head = res->freeEntries;
            RwResEntry *victim = rwLLLinkGetData(head->link.prev, RwResEntry, link);
            if (&victim->link == &head->link)
                break;
            if (victim->destroyNotify != nullptr)
                victim->destroyNotify(victim);
            if (victim->ownerRef != nullptr)
                *victim->ownerRef = nullptr;
            if (victim->link.next == nullptr) {
                RwGetMemFuncsGta()->rwfree(victim);
            } else {
                victim->link.prev->next = victim->link.next;
                victim->link.next->prev = victim->link.prev;
                res->currentSize -= victim->size;
                ((void(__cdecl *)(void))0x810040)();
            }
        }
        // then drain the *usedEntries(+0x24) list, one per retry (from the tail), counting freed size
        RwLinkList *head = res->usedEntries;
        RwResEntry *victim = rwLLLinkGetData(head->link.prev, RwResEntry, link);
        if (&victim->link == &head->link)
            break;
        res->reusageSize += victim->size;
        if (victim->destroyNotify != nullptr)
            victim->destroyNotify(victim);
        if (victim->ownerRef != nullptr)
            *victim->ownerRef = nullptr;
        if (victim->link.next == nullptr) {
            RwGetMemFuncsGta()->rwfree(victim);
        } else {
            victim->link.prev->next = victim->link.next;
            victim->link.next->prev = victim->link.prev;
            res->currentSize -= victim->size;
            ((void(__cdecl *)(void))0x810040)();
        }
    }
    if (ownerRef != nullptr)
        *ownerRef = nullptr;
    RwError err;
    err.pluginID = 1;
    err.errorCode = _rwerror(0xc, size);
    RwErrorSet(&err);
    return nullptr;
}


RwBool RwResourcesFreeResEntry(RwResEntry* entry) {
    RwGlobalsSA* engine = reinterpret_cast<RwGlobalsSA*>(RwEngineInstance);

    if (entry->destroyNotify != nullptr) {
        entry->destroyNotify(entry);
    }
    if (entry->ownerRef != nullptr) {
        *entry->ownerRef = nullptr;
    }
    if (entry->link.next != nullptr) {
        rwLinkListRemoveLLLink(&entry->link);
        RwInt32* currentSize = reinterpret_cast<RwInt32*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + rwResourcesGlobalsOffset + 4);
        *currentSize -= entry->size;
        ((void(__cdecl*)(RwResEntry*))0x810040)(entry);
        return TRUE;
    }
    engine->memoryFuncs.rwfree(entry);
    return TRUE;
}


void _rwResourcesPurge() {
    rwResources *res = RwGetResourcesGta();
    RwLinkList *dstHead = res->usedEntries; // *(res + 0x24)
    RwLinkList *srcHead = res->freeEntries; // *(res + 0x20)
    RwLLLink *srcFirst = srcHead->link.next;
    if (srcFirst != &srcHead->link) {       // source list non-empty
        if (dstHead->link.next == &dstHead->link) { // destination empty
            dstHead->link.next = srcFirst;
            srcFirst->prev = &dstHead->link;
            RwLLLink *srcLast = srcHead->link.prev;
            dstHead->link.prev = srcLast;
            srcLast->next = &dstHead->link;
        } else {                                    // splice source between dst last and dst head
            RwLLLink *dstLast = dstHead->link.prev;
            dstLast->next = srcFirst;
            srcFirst->prev = dstLast;
            RwLLLink *srcLast = srcHead->link.prev;
            srcLast->next = &dstHead->link;
            dstHead->link.prev = srcLast;
        }
        srcHead->link.next = &srcHead->link;
        srcHead->link.prev = &srcHead->link;
    }
    res->usedEntries = srcHead; // swap the two head pointers
    res->freeEntries = dstHead;
    res->reusageSize = 0;
}


RwBool RwStreamFindChunk(RwStream* stream, RwUInt32 type, RwUInt32* lengthOut, RwUInt32* versionOut)
{
    RwStream* s = stream;
    RwUInt32 chunkType;
    RwUInt32 length;
    RwUInt32 version;

    if (!((RwBool(__cdecl*)(RwStream*, RwUInt32*, RwUInt32*, RwUInt32*, RwUInt32*))0x7ED0F0)(s, &chunkType, &length, &version, nullptr))
        return FALSE;

    while (chunkType != type) {
        if (RwStreamSkip(s, length) == nullptr)
            return FALSE;
        if (!((RwBool(__cdecl*)(RwStream*, RwUInt32*, RwUInt32*, RwUInt32*, RwUInt32*))0x7ED0F0)(s, &chunkType, &length, &version, nullptr))
            return FALSE;
    }

    if (0x33FFF < version) {
        if (0x36003 < version) {
            RwError rwError;
            rwError.pluginID = 1;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000004);
            RwErrorSet(&rwError);
            return FALSE;
        }
        if (lengthOut != nullptr)
            *lengthOut = length;
        if (versionOut != nullptr)
            *versionOut = version;
        return TRUE;
    }

    {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000004);
        RwErrorSet(&rwError);
    }
    return FALSE;
}


RwStream* _rwStreamWriteVersionedChunkHeader(RwStream* stream, RwInt32 type, RwInt32 size, RwUInt32 version, RwUInt32 buildNum)
{
    RwUInt32 header[3];
    header[0] = static_cast<RwUInt32>(type);
    header[1] = static_cast<RwUInt32>(size);
    header[2] = ((version + 0x10000) & 0x3FF00) << 0xE | (version & 0x3F) << 0x10 | (buildNum & 0xFFFF);
    return RwStreamWrite(stream, header, 0xC);
}


RwStream* RwStreamWriteReal(RwStream* stream, const RwReal* reals, RwUInt32 numBytes)
{
    RwUInt8 buffer[0x100];

    while (true) {
        if (numBytes == 0)
            return stream;

        RwUInt32 count = numBytes < 0x100 ? numBytes : 0x100;
        // декомпил: копия 2 циклами (dword + байты) = побайтовая копия
        for (RwUInt32 i = 0; i < count; i++)
            buffer[i] = reinterpret_cast<const RwUInt8*>(reals)[i];

        if (RwStreamWrite(stream, buffer, count) == nullptr)
            break;

        numBytes -= count;
        reals = reinterpret_cast<const RwReal*>(reinterpret_cast<const RwUInt8*>(reals) + count);
    }
    return nullptr;
}


RwStream* RwStreamWriteInt32(RwStream* stream, const RwInt32* ints, RwUInt32 numBytes)
{
    RwStreamWrite(stream, ints, numBytes);
    return stream;
}


RwStream* RwStreamWriteInt16(RwStream* stream, const RwInt16* ints, RwUInt32 numBytes)
{
    RwStreamWrite(stream, ints, numBytes);
    return stream;
}


RwStream* RwStreamReadReal(RwStream* stream, RwReal* reals, RwUInt32 numBytes)
{
    if (RwStreamRead(stream, reals, numBytes) == 0) {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x8000001A);
        RwErrorSet(&rwError);
        return nullptr;
    }
    return stream;
}


RwStream* RwStreamReadInt32(RwStream* stream, RwInt32* ints, RwUInt32 numBytes)
{
    if (RwStreamRead(stream, ints, numBytes) == 0) {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x8000001A);
        RwErrorSet(&rwError);
        return nullptr;
    }
    return stream;
}


RwStream* RwStreamReadInt16(RwStream* stream, RwInt16* ints, RwUInt32 numBytes)
{
    if (RwStreamRead(stream, ints, numBytes) == 0) {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x8000001A);
        RwErrorSet(&rwError);
        return nullptr;
    }
    return stream;
}


RwStream* RwStreamReadChunkHeaderInfo(RwStream* stream, RwChunkHeaderInfo* chunkHeaderInfo)
{
    RwUInt32 type;
    RwUInt32 length;
    RwUInt32 version;
    RwUInt32 buildNum;

    if (!((RwBool(__cdecl*)(RwStream*, RwUInt32*, RwUInt32*, RwUInt32*, RwUInt32*))0x7ED0F0)(stream, &type, &length, &version, &buildNum))
        return nullptr;

    chunkHeaderInfo->type = type;
    chunkHeaderInfo->length = length;
    chunkHeaderInfo->version = version;
    chunkHeaderInfo->buildNum = buildNum;
    chunkHeaderInfo->isComplex = static_cast<RwBool>(((RwUInt32(__cdecl*)(RwChunkHeaderInfo*))0x7ED1E0)(chunkHeaderInfo));
    return stream;
}

