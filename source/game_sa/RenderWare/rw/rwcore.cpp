#include "StdInc.h"

#include "rwcore.h"

RwBool _rwIm3DTransformPipeOpen(RxPipeline** pipeline);
void   _rwIm3DTransformPipeClose(RxPipeline** pipeline);
void   _rwIm3DRenderPipeClose(RxPipeline** pipelines);
RwBool _rwIm3DRenderPipeOpen(RxPipeline** pipelines);

void RwCoreInjectHooks() {
    RH_ScopedCategory("Renderware");
    RH_ScopedNamespaceName("RwCore");

    RH_ScopedGlobalInstall(RwCameraBeginUpdate, 0x7EE190);
    RH_ScopedGlobalInstall(RwCameraEndUpdate, 0x7EE180);
    RH_ScopedGlobalInstall(RwCameraClear, 0x7EE340);
    RH_ScopedGlobalInstall(RwCameraShowRaster, 0x7EE370);

    // NIGHT-DISABLED RxHeapCreate 0x809F90
    // NIGHT-DISABLED RxHeapDestroy 0x809F30
    // NIGHT-DISABLED _rxHeapReset 0x809EC0
    // NIGHT-DISABLED RxHeapAlloc 0x809AA0
    // NIGHT-DISABLED RxHeapFree 0x8098B0
    // NIGHT-DISABLED RxHeapRealloc 0x809D10
    // NIGHT-DISABLED _rxPacketDestroy 0x80F070
    // NIGHT-DISABLED RxRenderStateVectorSetDefaultRenderStateVector 0x80EA40
    // NIGHT-DISABLED RwImageResample 0x80C600
    // NIGHT-DISABLED RwImageCreateResample 0x80CD10
    // NIGHT-DISABLED _rwIm3DTransformPipeOpen 0x80A0A0
    // NIGHT-DISABLED _rwIm3DTransformPipeClose 0x80A110
    // NIGHT-DISABLED _rwIm3DRenderPipeClose 0x80A140
    // NIGHT-DISABLED _rwIm3DRenderPipeOpen 0x80A1A0
}

/* --- RxHeap internals (static helpers were left unreversed in the original binary; referenced by address) --- */

/* Formats an error message & reports it, same as the RWERROR() macro would.
 * The message formatter (0x8088D0) and RwErrorSet (0x808820) are the RW error helpers. */
static void RxHeapReportError(RwUInt32 code, RwUInt32 arg) {
    RwError err;
    err.pluginID  = 1; // rwcore plugin ID, as in the binary's error reporting pattern
    err.errorCode = ((RwInt32(__cdecl *)(RwUInt32, RwUInt32))0x8088D0)(code, arg);
    RwErrorSet(&err);
}

/* 0x8099F0: Grow the heap's free-block array by 32 entries if needed and return the next free entry */
static rxHeapFreeBlock* _rxHeapGetFreeBlock(RxHeap* heap) {
    if (heap->entriesUsed >= heap->entriesAlloced) {
        heap->entriesAlloced += 0x20;
        const auto newBlocks = (rxHeapFreeBlock*)(RWSRCGLOBAL(memoryFuncs).rwrealloc)(heap->freeBlocks, heap->entriesAlloced * sizeof(rxHeapFreeBlock), 0x1030409u);
        if (!newBlocks) {
            RxHeapReportError(0x80000013u, heap->entriesAlloced << 3);
            heap->entriesAlloced -= 0x20;
            return nullptr;
        }
        if (newBlocks != heap->freeBlocks) {
            // The array moved: fix up the freeEntry back pointers of all used entries
            auto blk = newBlocks;
            for (auto i = heap->entriesUsed; i != 0; i--, blk++) {
                blk->ptr->freeEntry = blk;
            }
        }
        heap->freeBlocks = newBlocks;
    }
    if (!heap->freeBlocks) {
        return nullptr;
    }
    return &heap->freeBlocks[heap->entriesUsed++];
}

/* 0x809BF0: Carve a block out of a super block and insert its free block into the heap's free list after `insertAfter`.
 * `insertAfter` may also be a rxHeapSuperBlockDescriptor (both share the {pointer, size} layout) */
static RwBool _rxHeapAddSuperBlock(rxHeapSuperBlockDescriptor* superBlock, rxHeapFreeBlock* insertAfter, RxHeap* heap) {
    const auto entry = _rxHeapGetFreeBlock(heap);
    if (!entry) {
        return FALSE;
    }

    auto head    = (rxHeapBlockHeader*)superBlock->start;                            /* Block header at the super block start */
    auto userBlk = (rxHeapBlockHeader*)((RwUInt8*)head + sizeof(rxHeapBlockHeader)); /* The (free) user area right after it */
    auto tail    = (rxHeapBlockHeader*)((RwUInt8*)head + superBlock->size - sizeof(rxHeapBlockHeader));

    memset(head, 0, sizeof(*head));
    *tail = *head; /* The tail block starts out as a copy of the (zeroed) head block */

    head->next         = userBlk;
    userBlk->prev      = head;
    userBlk->next      = tail;
    userBlk->freeEntry = entry;
    userBlk->size      = (RwUInt8*)tail - (RwUInt8*)userBlk;

    entry->ptr  = userBlk;
    entry->size = userBlk->size;

    if (insertAfter) {
        // Link the tail block of the previous super block to our head block.
        // NOTE: (insertAfter->ptr + insertAfter->size) is the tail block header of the previous super block.
        auto prevTail = (rxHeapBlockHeader*)((RwUInt8*)insertAfter->ptr + insertAfter->size);
        prevTail->next = head;
        head->prev     = prevTail;
    }
    return TRUE;
}

/* 0x809C70: Allocate `size` bytes from a free block, splitting it if the remainder is big enough */
static void* _rxHeapAllocFromFreeBlock(RxHeap* heap, rxHeapFreeBlock* freeBlock, RwUInt32 size) {
    if (freeBlock->size - size < 0x100) {
        // Take the whole block: remove this entry from the free list (swap with last)
        const auto userPtr = (RwUInt8*)freeBlock->ptr + sizeof(rxHeapBlockHeader);
        if (&heap->freeBlocks[heap->entriesUsed - 1] != freeBlock) {
            *freeBlock = heap->freeBlocks[heap->entriesUsed - 1];
            heap->freeBlocks[heap->entriesUsed - 1].ptr->freeEntry = freeBlock;
        }
        heap->entriesUsed--;
        freeBlock->ptr->freeEntry = nullptr;
        return userPtr;
    }

    // Split: block keeps `size`, the remainder becomes a new free block right after it
    auto hdr       = freeBlock->ptr;
    auto remainder = (rxHeapBlockHeader*)((RwUInt8*)hdr + sizeof(rxHeapBlockHeader) + size);

    remainder->size      = freeBlock->size - size - sizeof(rxHeapBlockHeader);
    remainder->prev      = hdr;
    remainder->next      = hdr->next;
    remainder->freeEntry = freeBlock;

    hdr->size = size;
    hdr->next = remainder;
    if (remainder->next) {
        remainder->next->prev = remainder;
    }

    freeBlock->ptr  = remainder;
    freeBlock->size = remainder->size;
    return (RwUInt8*)hdr + sizeof(rxHeapBlockHeader);
}

RxHeap* RxHeapCreate(RwUInt32 size) {
    ZoneScoped;
    if (size < 0x400) {
        size = 0x400;
    }

    auto heap = (RxHeap*)RwMalloc(sizeof(RxHeap), 0x40409u);
    if (!heap) {
        return nullptr;
    }

    RwUInt32 alignedSize = (size + 0x1F) & ~0x1F;
    if (alignedSize < 0x80) {
        alignedSize = 0x80;
    }

    auto superBlock = (rxHeapSuperBlockDescriptor*)RwMalloc(alignedSize + 0x8B, 0x1040409u);
    if (superBlock) {
        superBlock->size  = alignedSize;
        superBlock->next  = nullptr;
        superBlock->start = (void*)(((uintptr)superBlock + 0x8B) & ~0x7Fu);

        heap->superBlockSize = alignedSize;
        heap->head           = superBlock;
        heap->headBlock      = nullptr;
        heap->freeBlocks     = nullptr;
        heap->entriesAlloced = 0;
        heap->entriesUsed    = 0;
        heap->dirty          = TRUE;

        rxHeapFreeBlock* prev = nullptr;
        for (auto sb = superBlock->next; sb != nullptr; sb = sb->next) { /* Never taken for a fresh heap, kept for faithfulness */
            if (!_rxHeapAddSuperBlock(sb, prev, heap)) {
                goto fail;
            }
            if (!prev) {
                heap->headBlock = (rxHeapBlockHeader*)sb->start;
            }
            prev = (rxHeapFreeBlock*)sb;
        }

        if (_rxHeapAddSuperBlock(heap->head, prev, heap)) {
            if (!prev) {
                heap->headBlock = (rxHeapBlockHeader*)heap->head->start;
            }
            heap->dirty = FALSE;
            return heap;
        }
    fail:
        RwFree(superBlock);
    }
    RwFree(heap);
    return nullptr;
}

void RxHeapDestroy(RxHeap* heap) {
    ZoneScoped;
    if (!heap) {
        return;
    }
    if (heap->freeBlocks) {
        RwFree(heap->freeBlocks);
        heap->freeBlocks = nullptr;
    }
    for (auto sb = heap->head; sb != nullptr;) {
        const auto next = sb->next;
        RwFree(sb);
        sb = next;
    }
    RwFree(heap);
}

RwBool _rxHeapReset(RxHeap* heap) {
    ZoneScoped;
    heap->entriesUsed = 0;

    rxHeapFreeBlock* prev = nullptr;
    for (auto sb = heap->head->next; sb != nullptr; sb = sb->next) {
        if (!_rxHeapAddSuperBlock(sb, prev, heap)) {
            return FALSE;
        }
        if (!prev) {
            heap->headBlock = (rxHeapBlockHeader*)sb->start;
        }
        prev = (rxHeapFreeBlock*)sb;
    }

    if (!_rxHeapAddSuperBlock(heap->head, prev, heap)) {
        return FALSE;
    }
    if (!prev) {
        heap->headBlock = (rxHeapBlockHeader*)heap->head->start;
    }
    heap->dirty = FALSE;
    return TRUE;
}

void* RxHeapAlloc(RxHeap* heap, RwUInt32 size) {
    ZoneScoped;
    const RwUInt32 alignedSize = (size + 0x1F) & ~0x1F;

    // Try the free blocks, largest first
    for (RwUInt32 i = heap->entriesUsed; i != 0; i--) {
        const auto fb = &heap->freeBlocks[i - 1];
        if (alignedSize <= fb->size) {
            heap->dirty = TRUE;
            return _rxHeapAllocFromFreeBlock(heap, fb, alignedSize);
        }
    }

    // No fit: add a new super block
    RwUInt32 superBlockSize = alignedSize + 0x60;
    if (superBlockSize < heap->superBlockSize) {
        superBlockSize = heap->superBlockSize;
    }

    auto superBlock = (rxHeapSuperBlockDescriptor*)RwMalloc(superBlockSize + 0x8B, 0x1040409u);
    if (superBlock) {
        superBlock->size  = superBlockSize;
        superBlock->next  = nullptr;
        superBlock->start = (void*)(((uintptr)superBlock + 0x8B) & ~0x7Fu);

        if (_rxHeapAddSuperBlock(superBlock, (rxHeapFreeBlock*)heap->head, heap)) {
            RwUInt32 totalSize = 0;
            for (auto sb = superBlock; sb != nullptr; sb = sb->next) {
                totalSize += sb->size;
            }

            RwChar buffer[256];
            rwsprintf(buffer, "Heap resized from %d to %d bytes", totalSize - superBlockSize, totalSize);

            const auto headBlock = (rxHeapBlockHeader*)superBlock->start;
            if ((uintptr)headBlock != 0xFFFFFFE0u) {
                heap->dirty = TRUE;
                return _rxHeapAllocFromFreeBlock(heap, (headBlock + 1)->freeEntry, alignedSize);
            }
        } else {
            RwFree(superBlock);
        }
    }

    RxHeapReportError(0x80000013u, alignedSize);
    return nullptr;
}

void RxHeapFree(RxHeap* heap, void* block) {
    ZoneScoped;
    auto hdr = (rxHeapBlockHeader*)((RwUInt8*)block - sizeof(rxHeapBlockHeader));

    const bool prevFree = hdr->prev && hdr->prev->freeEntry;
    const bool nextFree = hdr->next && hdr->next->freeEntry;

    if (prevFree) {
        if (nextFree) {
            // Merge all three blocks (prev, this, next)
            const auto nextFreeBlk = hdr->next->freeEntry;
            if (&heap->freeBlocks[heap->entriesUsed - 1] != nextFreeBlk) {
                *nextFreeBlk = heap->freeBlocks[heap->entriesUsed - 1];
                heap->freeBlocks[heap->entriesUsed - 1].ptr->freeEntry = nextFreeBlk;
            }
            heap->entriesUsed--;

            hdr->prev->size = hdr->prev->size + hdr->size + 0x40 + hdr->next->size;
            hdr->prev->freeEntry->size = hdr->prev->size;
            hdr->prev->next = hdr->next->next;
            if (hdr->next->next) {
                hdr->next->next->prev = hdr->prev;
            }
            return;
        }
        // Merge with previous only
        hdr->prev->size += hdr->size + 0x20;
        hdr->prev->freeEntry->size = hdr->prev->size;
        hdr->prev->next = hdr->next;
        if (hdr->next) {
            hdr->next->prev = hdr->prev;
        }
        return;
    }

    if (nextFree) {
        // Merge with next only
        hdr->size += hdr->next->size + 0x20;
        hdr->freeEntry = hdr->next->freeEntry;
        hdr->freeEntry->ptr  = hdr;
        hdr->freeEntry->size = hdr->size;
        const auto nextNext = hdr->next->next;
        hdr->next = nextNext;
        if (nextNext) {
            nextNext->prev = hdr;
        }
        return;
    }

    // Neither neighbour is free: add a new free entry for this block
    if (const auto fb = _rxHeapGetFreeBlock(heap)) {
        fb->ptr  = hdr;
        fb->size = hdr->size;
        hdr->freeEntry = fb;
    }
}

void* RxHeapRealloc(RxHeap* heap, void* block, RwUInt32 newSize, RwBool allowCopy) {
    ZoneScoped;
    auto hdr = (rxHeapBlockHeader*)((RwUInt8*)block - sizeof(rxHeapBlockHeader));
    const RwUInt32 alignedSize = (newSize + 0x1F) & ~0x1F;
    const RwInt32  delta       = alignedSize - hdr->size;

    if (delta <= 0) {
        // Shrink (or no change)
        if (-delta > 0xFF) {
            if (const auto fb = _rxHeapGetFreeBlock(heap)) {
                auto remainder = (rxHeapBlockHeader*)((RwUInt8*)block + alignedSize);
                remainder->size      = -delta - 0x20;
                remainder->prev      = hdr;
                remainder->freeEntry = fb;
                remainder->next      = hdr->next;
                hdr->size = alignedSize;
                hdr->next = remainder;
                if (remainder->next) {
                    remainder->next->prev = remainder;
                }
                fb->ptr  = remainder;
                fb->size = remainder->size;
            }
        }
        return block;
    }

    // Grow: if the next block is free and big enough, absorb it
    if (hdr->next && hdr->next->freeEntry && delta <= (RwInt32)(hdr->next->size + 0x20)) {
        const RwUInt32 totalSize = hdr->next->size + 0x20 + hdr->size;
        if (totalSize - alignedSize < 0x100) {
            // Absorb the whole next block
            const auto nextFreeBlk = hdr->next->freeEntry;
            if (&heap->freeBlocks[heap->entriesUsed - 1] != nextFreeBlk) {
                *nextFreeBlk = heap->freeBlocks[heap->entriesUsed - 1];
                heap->freeBlocks[heap->entriesUsed - 1].ptr->freeEntry = nextFreeBlk;
            }
            heap->entriesUsed--;

            hdr->size = totalSize;
            const auto nextNext = hdr->next->next;
            hdr->next = nextNext;
            if (nextNext) {
                nextNext->prev = hdr;
            }
            return block;
        }

        // Absorb only part of the next block
        auto remainder = (rxHeapBlockHeader*)((RwUInt8*)block + alignedSize);
        remainder->freeEntry = hdr->next->freeEntry;
        remainder->prev      = hdr;
        remainder->size      = hdr->next->size - delta;
        const auto nextNext  = hdr->next->next;
        remainder->next      = nextNext;
        if (nextNext) {
            nextNext->prev = remainder;
        }
        hdr->size = alignedSize;
        hdr->next = remainder;
        remainder->freeEntry->ptr  = remainder;
        remainder->freeEntry->size = remainder->size;
        return block;
    }

    // Cannot grow in place: allocate a new block, copy and free the old one
    if (const auto newBlock = (RwUInt8*)RxHeapAlloc(heap, alignedSize)) {
        const RwUInt32 oldSize = hdr->size;
        RwUInt32 i;
        for (i = 0; i < (oldSize & ~0x3u); i += 4) {
            *(RwUInt32*)(newBlock + i) = *(RwUInt32*)((RwUInt8*)block + i);
        }
        for (; i < oldSize; i++) {
            newBlock[i] = ((const RwUInt8*)block)[i];
        }
        RxHeapFree(heap, block);
        return newBlock;
    }

    RxHeapReportError(0x80000013u, alignedSize);
    return nullptr;
}

void RxPipelineSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    ((void(__cdecl *)(RwInt32, RwInt32))0x804FC0)(blockSize, numBlocksToPrealloc);
}

RwBool _rxPipelineOpen() {
    return ((RwBool(__cdecl *)(void))0x804FE0)();
}

RwBool _rxPipelineClose() {
    return ((RwBool(__cdecl *)(void))0x804F60)();
}

RxPipeline* RxPipelineCreate() {
    return ((RxPipeline*(__cdecl *)(void))0x8057B0)();
}

void _rxPipelineDestroy(RxPipeline* Pipeline) {
    ((void(__cdecl *)(RxPipeline*))0x805820)(Pipeline);
}

RxHeap* RxHeapGetGlobalHeap() {
    return ((RxHeap*(__cdecl *)(void))0x8052F0)();
}

RxPipeline* RxPipelineExecute(RxPipeline* pipeline, void* data, RwBool heapReset) {
    return ((RxPipeline*(__cdecl *)(RxPipeline*, void*, RwBool))0x805710)(pipeline, data, heapReset);
}

RxPacket* RxPacketCreate(RxPipelineNode* node) {
    return ((RxPacket*(__cdecl *)(RxPipelineNode*))0x805300)(node);
}

RxCluster* RxClusterSetStride(RxCluster* cluster, RwInt32 stride) {
    return ((RxCluster*(__cdecl *)(RxCluster*, RwInt32))0x805330)(cluster, stride);
}

RxCluster* RxClusterSetExternalData(RxCluster* cluster, void* data, RwInt32 stride, RwInt32 numElements) {
    return ((RxCluster*(__cdecl *)(RxCluster*, void*, RwInt32, RwInt32))0x805340)(cluster, data, stride, numElements);
}

RxCluster* RxClusterSetData(RxCluster* cluster, void* data, RwInt32 stride, RwInt32 numElements) {
    return ((RxCluster*(__cdecl *)(RxCluster*, void*, RwInt32, RwInt32))0x8053A0)(cluster, data, stride, numElements);
}

/* 0xC9A6B0: Pointer to the global RxHeap used by the packet/cluster machinery */
static inline auto& rxPacketGlobalHeap = StaticRef<RxHeap*>(0xC9A6B0);

/* 0x80F070 */
void _rxPacketDestroy(RxPacket* Packet) {
    ZoneScoped;
    Packet->pipeline->embeddedPacketState = (rxEmbeddedPacketState)1; /* Mark the pipeline's embedded packet as free */

    auto cluster = Packet->clusters;
    for (RwUInt32 i = Packet->numClusters; i != 0; i--, cluster++) {
        if (cluster->clusterRef != nullptr) {
            if (cluster->data != nullptr && !(cluster->flags & 2)) {
                RxHeapFree(rxPacketGlobalHeap, cluster->data);
            }
            memset(cluster, 0, 8);                   /* flags, stride, data (currentData is left as-is!) */
            memset((RwUInt8*)cluster + 0xC, 0, 0xC); /* numAlloced, numUsed, clusterRef (attributes left as-is) */
        }
    }
    Packet->flags = 0;
}

RxCluster* RxClusterInitializeData(RxCluster* cluster, RwUInt32 numElements, RwUInt16 stride) {
    return ((RxCluster*(__cdecl *)(RxCluster*, RwUInt32, RwUInt16))0x805400)(cluster, numElements, stride);
}

RxCluster* RxClusterResizeData(RxCluster* CurrentCluster, RwUInt32 NumElements) {
    return ((RxCluster*(__cdecl *)(RxCluster*, RwUInt32))0x805470)(CurrentCluster, NumElements);
}

RxCluster* RxClusterDestroyData(RxCluster* CurrentCluster) {
    return ((RxCluster*(__cdecl *)(RxCluster*))0x8054C0)(CurrentCluster);
}

RxCluster* RxClusterLockWrite(RxPacket* packet, RwUInt32 clusterIndex, RxPipelineNode* node) {
    return ((RxCluster*(__cdecl *)(RxPacket*, RwUInt32, RxPipelineNode*))0x8054F0)(packet, clusterIndex, node);
}

void RxClusterUnlock(RxCluster* cluster) {
    ((void(__cdecl *)(RxCluster*))0x8055C0)(cluster);
}

RwUInt32 RxPipelineNodeSendConfigMsg(RxPipelineNode* dest, RwUInt32 msg, RwUInt32 intparam, void* ptrparam) {
    return ((RwUInt32(__cdecl *)(RxPipelineNode*, RwUInt32, RwUInt32, void*))0x8055D0)(dest, msg, intparam, ptrparam);
}

RxPipelineNode* RxPipelineNodeForAllConnectedOutputs(RxPipelineNode* node, RxPipeline* pipeline, RxPipelineNodeOutputCallBack callbackfn, void* callbackdata) {
    return ((RxPipelineNode*(__cdecl *)(RxPipelineNode*, RxPipeline*, RxPipelineNodeOutputCallBack, void*))0x8055F0)(node, pipeline, callbackfn, callbackdata);
}

RxPipelineCluster* RxPipelineNodeGetPipelineCluster(RxPipelineNode* node, RwUInt32 clustersOfInterestIndex) {
    return ((RxPipelineCluster*(__cdecl *)(RxPipelineNode*, RwUInt32))0x805680)(node, clustersOfInterestIndex);
}

RwUInt32 RxPipelineClusterGetCreationAttributes(RxPipelineCluster* cluster) {
    return ((RwUInt32(__cdecl *)(RxPipelineCluster*))0x8056B0)(cluster);
}

RxPipelineCluster* RxPipelineClusterSetCreationAttributes(RxPipelineCluster* cluster, RwUInt32 creationAttributes) {
    return ((RxPipelineCluster*(__cdecl *)(RxPipelineCluster*, RwUInt32))0x8056C0)(cluster, creationAttributes);
}

RwUInt32 RxClusterGetAttributes(RxCluster* cluster) {
    return ((RwUInt32(__cdecl *)(RxCluster*))0x8056E0)(cluster);
}

RxCluster* RxClusterSetAttributes(RxCluster* cluster, RwUInt32 attributes) {
    return ((RxCluster*(__cdecl *)(RxCluster*, RwUInt32))0x8056F0)(cluster, attributes);
}

RxNodeOutput RxPipelineNodeFindOutputByName(RxPipelineNode* node, const RwChar* outputname) {
    return ((RxNodeOutput(__cdecl *)(RxPipelineNode*, const RwChar*))0x805D10)(node, outputname);
}

RxNodeOutput RxPipelineNodeFindOutputByIndex(RxPipelineNode* node, RwUInt32 outputindex) {
    return ((RxNodeOutput(__cdecl *)(RxPipelineNode*, RwUInt32))0x805D70)(node, outputindex);
}

RxNodeInput RxPipelineNodeFindInput(RxPipelineNode* node) {
    return ((RxNodeInput(__cdecl *)(RxPipelineNode*))0x805DA0)(node);
}

RxNodeDefinition* RxPipelineNodeCloneDefinition(RxPipelineNode* node, RxClusterDefinition* cluster2add) {
    return ((RxNodeDefinition*(__cdecl *)(RxPipelineNode*, RxClusterDefinition*))0x8059C0)(node, cluster2add);
}

RxPipeline* RxPipelineNodeRequestCluster(RxPipeline* pipeline, RxPipelineNode* node, RxClusterDefinition* clusterDef) {
    return ((RxPipeline*(__cdecl *)(RxPipeline*, RxPipelineNode*, RxClusterDefinition*))0x805DB0)(pipeline, node, clusterDef);
}

RxPipeline* RxPipelineNodeReplaceCluster(RxPipeline* pipeline, RxPipelineNode* node, RxClusterDefinition* oldClusterDef, RxClusterDefinition* newClusterDef) {
    return ((RxPipeline*(__cdecl *)(RxPipeline*, RxPipelineNode*, RxClusterDefinition*, RxClusterDefinition*))0x805E20)(pipeline, node, oldClusterDef, newClusterDef);
}

void* RxPipelineNodeGetInitData(RxPipelineNode* node) {
    return ((void*(__cdecl *)(RxPipelineNode*))0x805F30)(node);
}

void* RxPipelineNodeCreateInitData(RxPipelineNode* node, RwUInt32 size) {
    return ((void*(__cdecl *)(RxPipelineNode*, RwUInt32))0x805EA0)(node, size);
}

RxPipeline* RxPipelineClone(RxPipeline* pipeline) {
    return ((RxPipeline*(__cdecl *)(RxPipeline*))0x806AC0)(pipeline);
}

RxPipelineNode* RxPipelineFindNodeByName(RxPipeline* pipeline, const RwChar* name, RxPipelineNode* start, RwInt32* nodeIndex) {
    return ((RxPipelineNode*(__cdecl *)(RxPipeline*, const RwChar*, RxPipelineNode*, RwInt32*))0x806B30)(pipeline, name, start, nodeIndex);
}

RxPipelineNode* RxPipelineFindNodeByIndex(RxPipeline* pipeline, RwUInt32 nodeindex) {
    return ((RxPipelineNode*(__cdecl *)(RxPipeline*, RwUInt32))0x806BC0)(pipeline, nodeindex);
}

RxLockedPipe* RxPipelineLock(RxPipeline* pipeline) {
    return ((RxLockedPipe*(__cdecl *)(RxPipeline*))0x806990)(pipeline);
}

RxPipeline* RxLockedPipeUnlock(RxLockedPipe* pipeline) {
    return ((RxPipeline*(__cdecl *)(RxLockedPipe*))0x805F40)(pipeline);
}

RxPipeline* RxLockedPipeReplaceNode(RxLockedPipe* pipeline, RxPipelineNode* node, RxNodeDefinition* nodeDef) {
    return ((RxPipeline*(__cdecl *)(RxLockedPipe*, RxPipelineNode*, RxNodeDefinition*))0x806F20)(pipeline, node, nodeDef);
}

RxPipeline* RxLockedPipeDeleteNode(RxLockedPipe* pipeline, RxPipelineNode* node) {
    return ((RxPipeline*(__cdecl *)(RxLockedPipe*, RxPipelineNode*))0x807040)(pipeline, node);
}

RxPipeline* RxLockedPipeSetEntryPoint(RxLockedPipe* pipeline, RxNodeInput in) {
    return ((RxPipeline*(__cdecl *)(RxLockedPipe*, RxNodeInput))0x807070)(pipeline, in);
}

RxLockedPipe* RxLockedPipeAddFragment(RxLockedPipe* pipeline, RwUInt32* firstIndex, RxNodeDefinition* nodeDef0, RxNodeDefinition* __va_end__) {
    return plugin::CallAndReturn<RxPipeline*, 0x806BE0, RxLockedPipe*, RwUInt32*, RxNodeDefinition*, RxNodeDefinition*>(pipeline, firstIndex, nodeDef0, __va_end__);
}

RxPipelineNode* RxLockedPipeGetEntryPoint(RxLockedPipe* pipeline) {
    return ((RxPipelineNode*(__cdecl *)(RxLockedPipe*))0x8070D0)(pipeline);
}

RxPipeline* RxLockedPipeAddPath(RxLockedPipe* pipeline, RxNodeOutput out, RxNodeInput in) {
    return ((RxPipeline*(__cdecl *)(RxLockedPipe*, RxNodeOutput, RxNodeInput))0x807100)(pipeline, out, in);
}

RxPipeline* RxLockedPipeDeletePath(RxLockedPipe* pipeline, RxNodeOutput out, RxNodeInput in) {
    return ((RxPipeline*(__cdecl *)(RxLockedPipe*, RxNodeOutput, RxNodeInput))0x807170)(pipeline, out, in);
}

RxPipeline* RxPipelineInsertDebugNode(RxPipeline* pipeline, RxPipelineNode* before, RxPipelineNode* after, RxNodeDefinition* debugNode) {
    return ((RxPipeline*(__cdecl *)(RxPipeline*, RxPipelineNode*, RxPipelineNode*, RxNodeDefinition*))0x8071B0)(pipeline, before, after, debugNode);
}

void RwD3D9VertexBufferManagerChangeDefaultSize(RwUInt32 defaultSize) {
    ((void(__cdecl *)(RwUInt32))0x7F57F0)(defaultSize);
}

RwBool RwD3D9CreateVertexBuffer(RwUInt32 stride, RwUInt32 size, void* vertexBuffer, RwUInt32* offset) {
    return ((RwBool(__cdecl *)(RwUInt32, RwUInt32, void*, RwUInt32*))0x7F5500)(stride, size, vertexBuffer, offset);
}

void RwD3D9DestroyVertexBuffer(RwUInt32 stride, RwUInt32 size, void* vertexBuffer, RwUInt32 offset) {
    ((void(__cdecl *)(RwUInt32, RwUInt32, void*, RwUInt32))0x7F56A0)(stride, size, vertexBuffer, offset);
}

RwBool RwD3D9DynamicVertexBufferCreate(RwUInt32 size, void* vertexBuffer) {
    return ((RwBool(__cdecl *)(RwUInt32, void*))0x7F5A00)(size, vertexBuffer);
}

// 0x7F5AE0: Resets the internal bookkeeping (cached base/offset) of a single dynamic vertex buffer.
// NOTE: the previous definition here was `RwD3D9DynamicVertexBufferDestroy(void)` which did not
// match the `void* vertexBuffer` signature declared in rwcore.h (identified 2026-09-27).
void RwD3D9DynamicVertexBufferDestroy(void* vertexBuffer) {
    ((void(__cdecl *)(void*))0x7F5AE0)(vertexBuffer);
}

RwBool RwD3D9DynamicVertexBufferLock(RwUInt32 vertexSize, RwUInt32 numVertex, void* vertexBufferOut, void* vertexDataOut, RwUInt32* baseIndexOut) {
    return ((RwBool(__cdecl *)(RwUInt32, RwUInt32, void*, void*, RwUInt32*))0x7F5B10)(vertexSize, numVertex, vertexBufferOut, vertexDataOut, baseIndexOut);
}

// 0x7F5C90: IDirect3DVertexBuffer9::Unlock() on the given dynamic vertex buffer.
// NOTE: the previous definition here was `RwD3D9DynamicVertexBufferUnlock(void)` which did not
// match the `void* vertexBuffer` signature declared in rwcore.h (identified 2026-09-27).
RwBool RwD3D9DynamicVertexBufferUnlock(void* vertexBuffer) {
    return ((RwBool(__cdecl *)(void*))0x7F5C90)(vertexBuffer);
}

/*
 * Dynamic vertex buffer module (driver/d3d9, identified from binary 2026-09-27).
 * Pool of either one or four D3DPOOL_DEFAULT vertex buffers (256 KiB each,
 * the 4-buffer round-robin is used when the driver-cap global has bit 0x10000 set).
 */

// 0x7F5940: Creates the dynamic vertex buffer pool (1 or 4 x 256 KiB buffers) and resets the slot index.
RwBool _rwD3D9DynamicVertexBufferCreateAll() {
    return ((RwBool(__cdecl *)(void))0x7F5940)();
}

// 0x7F5800: Clears the current slot's cached offsets and advances the round-robin slot index (mod 4).
void _rwD3D9DynamicVertexBufferAdvanceSlot() {
    ((void(__cdecl *)(void))0x7F5800)();
}

// 0x7F5840: Resets all slots' bookkeeping and, when the pool is the 4-buffer variant,
// releases each slot's D3D vertex buffer and unlinks its allocation record.
void _rwD3D9DynamicVertexBufferResetAll() {
    ((void(__cdecl *)(void))0x7F5840)();
}

// 0x7F5CB0: Opens the dynamic vertex buffer module: tears down any previous state,
// (re)creates the 4 bookkeeping freelists and the vertex buffer pool.
RwBool _rwD3D9DynamicVertexBufferOpen() {
    return ((RwBool(__cdecl *)(void))0x7F5CB0)();
}

// 0x7F5D20: Closes the dynamic vertex buffer module: releases every D3D vertex buffer,
// frees all allocation records and destroys the bookkeeping freelists.
void _rwD3D9DynamicVertexBufferClose() {
    ((void(__cdecl *)(void))0x7F5D20)();
}

/*
 * D3D9 device module (driver/d3d9, identified from binary 2026-09-27).
 */

// 0x7F6B00: Invalidates the cached D3D render states, sampler states and the 4 texture stage caches.
void _rwD3D9RenderStateInvalidateCache() {
    ((void(__cdecl *)(void))0x7F6B00)();
}

// 0x7F6B40: Opens the device resource cache: (re)creates the 0x40-byte-record freelist,
// clears the 65-slot resource table and allocates the default record.
void _rwD3D9DeviceResourceCacheOpen() {
    ((void(__cdecl *)(void))0x7F6B40)();
}

// 0x7F6BF0: Probes IDirect3D9 for the highest supported format value in the range [2, 16]
// and caches the maximum plus the derived per-format values in the driver globals.
void _rwD3D9DeviceProbeMaxFormatSupport() {
    ((void(__cdecl *)(void))0x7F6BF0)();
}

// 0x7F7100: Full device shutdown: frees the display-mode list and resource caches, unbinds all
// D3D streams/indices/textures/shaders, tears down the raster and dynamic vertex buffer modules
// and finally releases the IDirect3DDevice9.
RwBool _rwD3D9DeviceClose() {
    return ((RwBool(__cdecl *)(void))0x7F7100)();
}

// 0x7F72E0: Fills a std-function dispatch table (RwEngineInstance->stdFunc[]) of `count` entries
// with the NA stub, then installs the ~27 known driver/game handlers by message code
// (e.g. camera begin-update/clear, show-raster and the game-side overrides at 0x4C9F90-0x4CD820).
RwBool _rwD3D9StdFuncTableInit(void** table, void* param, RwInt32 count) {
    return ((RwBool(__cdecl *)(void**, void*, RwInt32))0x7F72E0)(table, param, count);
}

// 0x7F7540: Enumerates adapter display modes via IDirect3D9::EnumAdapterModes, deduplicates them
// per format keeping the highest refresh rate and classifies D3DFMT 21-25/35 as the supported
// (32/16 bpp) format set used for the video mode list.
void _rwD3D9BuildDisplayModeList() {
    ((void(__cdecl *)(void))0x7F7540)();
}

// 0x7F8300: Device-reset handler: waits via IDirect3DDevice9::TestCooperativeLevel, releases video
// memory, resets the device with the cached D3DPRESENT_PARAMETERS (re-probing the display format
// when necessary), reacquires the swap chain/back buffer and restores rasters, stream sources and Im3D.
RwBool _rwD3D9DeviceReset() {
    return ((RwBool(__cdecl *)(void))0x7F8300)();
}

void RwRasterSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    ((void(__cdecl *)(RwInt32, RwInt32))0x7FB350)(blockSize, numBlocksToPrealloc);
}

RwRaster* RwRasterCreate(RwInt32 width, RwInt32 height, RwInt32 depth, RwInt32 flags) {
    return ((RwRaster*(__cdecl *)(RwInt32, RwInt32, RwInt32, RwInt32))0x7FB230)(width, height, depth, flags);
}

RwBool RwRasterDestroy(RwRaster* raster) {
    return ((RwBool(__cdecl *)(RwRaster*))0x7FB020)(raster);
}

RwRaster* RwRasterGetOffset(RwRaster* raster, RwInt16* xOffset, RwInt16* yOffset) {
    return ((RwRaster*(__cdecl *)(RwRaster*, RwInt16*, RwInt16*))0x7FAEA0)(raster, xOffset, yOffset);
}

RwInt32 RwRasterGetNumLevels(RwRaster* raster) {
    return ((RwInt32(__cdecl *)(RwRaster*))0x7FB160)(raster);
}

RwRaster* RwRasterSubRaster(RwRaster* subRaster, RwRaster* raster, RwRect* rect) {
    return ((RwRaster*(__cdecl *)(RwRaster*, RwRaster*, RwRect*))0x7FB1D0)(subRaster, raster, rect);
}

RwRaster* RwRasterRenderFast(RwRaster* raster, RwInt32 x, RwInt32 y) {
    return ((RwRaster*(__cdecl *)(RwRaster*, RwInt32, RwInt32))0x7FAF50)(raster, x, y);
}

RwRaster* RwRasterRender(RwRaster* raster, RwInt32 x, RwInt32 y) {
    return ((RwRaster*(__cdecl *)(RwRaster*, RwInt32, RwInt32))0x7FAFB0)(raster, x, y);
}

RwRaster* RwRasterRenderScaled(RwRaster* raster, RwRect* rect) {
    return ((RwRaster*(__cdecl *)(RwRaster*, RwRect*))0x7FAE80)(raster, rect);
}

RwRaster* RwRasterPushContext(RwRaster* raster) {
    return ((RwRaster*(__cdecl *)(RwRaster*))0x7FB060)(raster);
}

RwRaster* RwRasterPopContext() {
    return ((RwRaster*(__cdecl *)(void))0x7FB110)();
}

RwRaster* RwRasterGetCurrentContext() {
    return ((RwRaster*(__cdecl *)(void))0x7FAE60)();
}

RwBool RwRasterClear(RwInt32 pixelValue) {
    return ((RwBool(__cdecl *)(RwInt32))0x7FAEE0)(pixelValue);
}

RwBool RwRasterClearRect(RwRect* rpRect, RwInt32 pixelValue) {
    return ((RwBool(__cdecl *)(RwRect*, RwInt32))0x7FAF90)(rpRect, pixelValue);
}

RwRaster* RwRasterShowRaster(RwRaster* raster, void* dev, RwUInt32 flags) {
    return ((RwRaster*(__cdecl *)(RwRaster*, void*, RwUInt32))0x7FB1A0)(raster, dev, flags);
}

//! NOTE: This function is responsible for calculating the `stride` in the `raster`
RwUInt8* RwRasterLock(RwRaster* raster, RwUInt8 level, RwInt32 lockMode) {
    return ((RwUInt8*(__cdecl *)(RwRaster*, RwUInt8, RwInt32))0x7FB2D0)(raster, level, lockMode);
}

RwRaster* RwRasterUnlock(RwRaster* raster) {
    return ((RwRaster*(__cdecl *)(RwRaster*))0x7FAEC0)(raster);
}

RwUInt8* RwRasterLockPalette(RwRaster* raster, RwInt32 lockMode) {
    return ((RwUInt8*(__cdecl *)(RwRaster*, RwInt32))0x7FB0E0)(raster, lockMode);
}

RwRaster* RwRasterUnlockPalette(RwRaster* raster) {
    return ((RwRaster*(__cdecl *)(RwRaster*))0x7FAFF0)(raster);
}

RwInt32 RwRasterRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return ((RwInt32(__cdecl *)(RwInt32, RwUInt32, RwPluginObjectConstructor, RwPluginObjectDestructor, RwPluginObjectCopy))0x7FB0B0)(size, pluginID, constructCB, destructCB, copyCB);
}

RwInt32 RwRasterGetPluginOffset(RwUInt32 pluginID) {
    return ((RwInt32(__cdecl *)(RwUInt32))0x7FAE40)(pluginID);
}

/* 0xC9BC60: RwGlobals offset of the rx pipeline globals (rwPipeGlobals) */
static inline auto& rxPipeGlobalsOffset = StaticRef<RwInt32>(0xC9BC60);

/* 0x80EA40 */
RxRenderStateVector* RxRenderStateVectorSetDefaultRenderStateVector(RxRenderStateVector* rsvp) {
    ZoneScoped;
    if (!rsvp) {
        RxHeapReportError(0x80000016u, 0);
        return nullptr;
    }

    auto& pipeGlobals = *(rwPipeGlobals*)((RwUInt8*)RwEngineInstance + rxPipeGlobalsOffset);
    if (RwEngineInstance->engineStatus == rwENGINESTATUSSTARTED) {
        // Engine is running: just copy the current default render state vector
        memcpy(rsvp, &pipeGlobals.defaultRenderState, sizeof(RxRenderStateVector));
        return rsvp;
    }

    if (rsvp != &pipeGlobals.defaultRenderState) {
        RxHeapReportError(0x80000018u, 0);
        return nullptr;
    }

    // Reset the default render state vector to sane defaults
    rsvp->Flags         = 7;
    rsvp->ShadeMode     = (RwShadeMode)2;
    rsvp->SrcBlend      = (RwBlendFunction)5;
    rsvp->DestBlend     = (RwBlendFunction)6;
    rsvp->TextureRaster = nullptr;
    rsvp->AddressModeU  = (RwTextureAddressMode)1;
    rsvp->AddressModeV  = (RwTextureAddressMode)1;
    rsvp->FilterMode    = (RwTextureFilterMode)2;
    memset(&rsvp->BorderColor, 0xFF, sizeof(rsvp->BorderColor));
    rsvp->FogType       = (RwFogType)0;
    memset(&rsvp->FogColor, 0xFF, sizeof(rsvp->FogColor));
    return rsvp;
}

RwImage* RwImageCreate(RwInt32 width, RwInt32 height, RwInt32 depth) {
    return ((RwImage*(__cdecl *)(RwInt32, RwInt32, RwInt32))0x8026E0)(width, height, depth);
}

RwBool RwImageDestroy(RwImage* image) {
    return ((RwBool(__cdecl *)(RwImage*))0x802740)(image);
}

RwImage* RwImageAllocatePixels(RwImage* image) {
    return ((RwImage*(__cdecl *)(RwImage*))0x8027A0)(image);
}

RwImage* RwImageFreePixels(RwImage* image) {
    return ((RwImage*(__cdecl *)(RwImage*))0x802860)(image);
}

RwImage* RwImageApplyMask(RwImage* image, const RwImage* mask) {
    return ((RwImage*(__cdecl *)(RwImage*, const RwImage*))0x802AF0)(image, mask);
}

RwImage* RwImageMakeMask(RwImage* image) {
    return ((RwImage*(__cdecl *)(RwImage*))0x802A20)(image);
}

RwImage* RwImageReadMaskedImage(const RwChar* imageName, const RwChar* maskname) {
    return ((RwImage*(__cdecl *)(const RwChar*, const RwChar*))0x8035C0)(imageName, maskname);
}

RwImage* RwImageRead(const RwChar* imageName) {
    return ((RwImage*(__cdecl *)(const RwChar*))0x802FD0)(imageName);
}

const RwChar* RwImageSetPath(const RwChar* path) {
    return ((const RwChar*(__cdecl *)(const RwChar*))0x802EA0)(path);
}

RwBool RwImageSetGamma(RwReal gammaValue) {
    return ((RwBool(__cdecl *)(RwReal))0x803FE0)(gammaValue);
}

RwImage* RwImageGammaCorrect(RwImage* image) {
    return ((RwImage*(__cdecl *)(RwImage*))0x803E30)(image);
}

RwBool RwPalQuantInit(RwPalQuant* pq) {
    return ((RwBool(__cdecl *)(RwPalQuant*))0x80C470)(pq);
}

void RwPalQuantTerm(RwPalQuant* pq) {
    ((void(__cdecl *)(RwPalQuant*))0x80C520)(pq);
}

void RwPalQuantAddImage(RwPalQuant* pq, RwImage* img, RwReal weight) {
    ((void(__cdecl *)(RwPalQuant*, RwImage*, RwReal))0x80AA80)(pq, img, weight);
}

RwInt32 RwPalQuantResolvePalette(RwRGBA* palette, RwInt32 maxcols, RwPalQuant* pq) {
    return ((RwInt32(__cdecl *)(RwRGBA*, RwInt32, RwPalQuant*))0x80AF60)(palette, maxcols, pq);
}

void RwPalQuantMatchImage(RwUInt8* dstpixels, RwInt32 dststride, RwInt32 dstdepth, RwBool dstPacked, RwPalQuant* pq, RwImage* img) {
    ((void(__cdecl *)(RwUInt8*, RwInt32, RwInt32, RwBool, RwPalQuant*, RwImage*))0x80BF20)(dstpixels, dststride, dstdepth, dstPacked, pq, img);
}

RwBool RwTextureSetMipmapping(RwBool enable) {
    return ((RwBool(__cdecl *)(RwBool))0x7F3530)(enable);
}

RwBool RwTextureGetMipmapping() {
    return ((RwBool(__cdecl *)(void))0x7F3550)();
}

RwBool RwTextureSetAutoMipmapping(RwBool enable) {
    return ((RwBool(__cdecl *)(RwBool))0x7F3560)(enable);
}

RwBool RwTextureGetAutoMipmapping() {
    return ((RwBool(__cdecl *)(void))0x7F3580)();
}

RwBool RwTextureSetMipmapGenerationCallBack(RwTextureCallBackMipmapGeneration callback) {
    return ((RwBool(__cdecl *)(RwTextureCallBackMipmapGeneration))0x7F3C70)(callback);
}

RwTextureCallBackMipmapGeneration RwTextureGetMipmapGenerationCallBack() {
    return ((RwTextureCallBackMipmapGeneration(__cdecl *)(void))0x7F3C90)();
}

RwBool RwTextureSetMipmapNameCallBack(RwTextureCallBackMipmapName callback) {
    return ((RwBool(__cdecl *)(RwTextureCallBackMipmapName))0x7F3CA0)(callback);
}

RwTextureCallBackMipmapName RwTextureGetMipmapNameCallBack() {
    return ((RwTextureCallBackMipmapName(__cdecl *)(void))0x7F3CC0)();
}

RwBool RwTextureGenerateMipmapName(RwChar* name, RwChar* maskName, RwUInt8 mipLevel, RwInt32 format) {
    return ((RwBool(__cdecl *)(RwChar*, RwChar*, RwUInt8, RwInt32))0x7F3AA0)(name, maskName, mipLevel, format);
}

RwBool RwTextureRasterGenerateMipmaps(RwRaster* raster, RwImage* image) {
    return ((RwBool(__cdecl *)(RwRaster*, RwImage*))0x7F3CD0)(raster, image);
}

/*
 * Texture/image loading internals (identified from binary 2026-09-27).
 */

// 0x7F44C0: Builds one palette shared by a set of images: if all images already reference the
// same cached palette (image->palette), copies it out; otherwise quantizes the images jointly via
// the RwPalQuant module (RwPalQuantAddImage/GetPalette), converts each image to the new palette,
// caches it in every image and copies it to `paletteOut`.
RwBool _rwImagePaletteUnify(RwRGBA* paletteOut, RwImage* imageToKeep, RwImage** images, RwInt32 numImages, RwInt32 paletteDepth) {
    return ((RwBool(__cdecl *)(RwRGBA*, RwImage*, RwImage**, RwInt32, RwInt32))0x7F44C0)(paletteOut, imageToKeep, images, numImages, paletteDepth);
}

// 0x7F4690: Reads an image (RwImageRead) and resamples it to *widthInOut x *heightInOut
// (both taken from the file when 0), preserving 4/8-bit paletted sources by quantizing the
// resized 32-bit image back down to a shared 16/256-color palette.
RwImage* _rwImageReadResample(const RwChar* name, const RwChar* maskName, RwUInt32 rasterFlags, RwInt32* widthInOut, RwInt32* heightInOut, void* param6, void* param7) {
    return ((RwImage * (__cdecl*)(const RwChar*, const RwChar*, RwUInt32, RwInt32*, RwInt32*, void*, void*))0x7F4690)(name, maskName, rasterFlags, widthInOut, heightInOut, param6, param7);
}

// 0x7F4AE0: The RwTextureRead raster pipeline: validates name/maskName, computes the raster
// format from the texture mip-map callbacks, reads + resamples the image
// (_rwImageReadResample), creates the D3D9 raster, applies mip-mapping and palettes
// (_rwImagePaletteUnify) and returns the finished raster.
RwRaster* _rwTextureReadRaster(const RwChar* name, const RwChar* maskName) {
    return ((RwRaster * (__cdecl*)(const RwChar*, const RwChar*))0x7F4AE0)(name, maskName);
}

RwBool _rwTextureSetAutoMipMapState(RwBool enable) {
    return ((RwBool(__cdecl *)(RwBool))0x7F3590)(enable);
}

RwBool _rwTextureGetAutoMipMapState() {
    return ((RwBool(__cdecl *)(void))0x7F35C0)();
}

RwTextureCallBackRead RwTextureGetReadCallBack() {
    return ((RwTextureCallBackRead(__cdecl *)(void))0x7F3520)();
}

RwBool RwTextureSetReadCallBack(RwTextureCallBackRead callBack) {
    return ((RwBool(__cdecl *)(RwTextureCallBackRead))0x7F3500)(callBack);
}

RwTextureCallBackFind RwTextureGetFindCallBack() {
    return ((RwTextureCallBackFind(__cdecl *)(void))0x7F34F0)();
}

RwBool RwTextureSetFindCallBack(RwTextureCallBackFind callBack) {
    return ((RwBool(__cdecl *)(RwTextureCallBackFind))0x7F34D0)(callBack);
}

RwTexture* RwTextureSetName(RwTexture* texture, const RwChar* name) {
    return ((RwTexture*(__cdecl *)(RwTexture*, const RwChar*))0x7F38A0)(texture, name);
}

RwTexture* RwTextureSetMaskName(RwTexture* texture, const RwChar* maskName) {
    return ((RwTexture*(__cdecl *)(RwTexture*, const RwChar*))0x7F3910)(texture, maskName);
}

void RwTexDictionarySetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    ((void(__cdecl *)(RwInt32, RwInt32))0x7F3E80)(blockSize, numBlocksToPrealloc);
}

RwTexDictionary* RwTexDictionaryCreate() {
    return ((RwTexDictionary*(__cdecl *)(void))0x7F3600)();
}

RwBool RwTexDictionaryDestroy(RwTexDictionary* dict) {
    return ((RwBool(__cdecl *)(RwTexDictionary*))0x7F36A0)(dict);
}

RwTexture* RwTextureCreate(RwRaster* raster) {
    return ((RwTexture*(__cdecl *)(RwRaster*))0x7F37C0)(raster);
}

RwTexture* RwTextureRead(const char* name, const char* maskName) {
    return plugin::CallAndReturn<RwTexture*, 0x7F3AC0, const char*, const char*>(name, maskName);
}

RwBool RwTextureDestroy(RwTexture* texture) {
    return ((RwBool(__cdecl *)(RwTexture*))0x7F3820)(texture);
}

RwTexture* RwTextureSetRaster(RwTexture* texture, RwRaster* raster) {
    return ((RwTexture*(__cdecl *)(RwTexture*, RwRaster*))0x7F35D0)(texture, raster);
}

RwTexture* RwTexDictionaryAddTexture(RwTexDictionary* dict, RwTexture* texture) {
    return ((RwTexture*(__cdecl *)(RwTexDictionary*, RwTexture*))0x7F3980)(dict, texture);
}

RwTexture* RwTexDictionaryRemoveTexture(RwTexture* texture) {
    return ((RwTexture*(__cdecl *)(RwTexture*))0x7F39C0)(texture);
}

RwTexture* RwTexDictionaryFindNamedTexture(RwTexDictionary* dict, const RwChar* name) {
    return ((RwTexture*(__cdecl *)(RwTexDictionary*, const RwChar*))0x7F39F0)(dict, name);
}

RwTexDictionary* RwTexDictionaryGetCurrent() {
    return ((RwTexDictionary*(__cdecl *)(void))0x7F3A90)();
}

RwTexDictionary* RwTexDictionarySetCurrent(RwTexDictionary* dict) {
    return ((RwTexDictionary*(__cdecl *)(RwTexDictionary*))0x7F3A70)(dict);
}

const RwTexDictionary* RwTexDictionaryForAllTextures(const RwTexDictionary* dict, RwTextureCallBack fpCallBack, void* data) {
    return ((const RwTexDictionary*(__cdecl *)(const RwTexDictionary*, RwTextureCallBack, void*))0x7F3730)(dict, fpCallBack, data);
}

RwBool RwTexDictionaryForAllTexDictionaries(RwTexDictionaryCallBack fpCallBack, void* data) {
    return ((RwBool(__cdecl *)(RwTexDictionaryCallBack, void*))0x7F3770)(fpCallBack, data);
}

RwInt32 RwTextureRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return ((RwInt32(__cdecl *)(RwInt32, RwUInt32, RwPluginObjectConstructor, RwPluginObjectDestructor, RwPluginObjectCopy))0x7F3BB0)(size, pluginID, constructCB, destructCB, copyCB);
}

RwInt32 RwTexDictionaryRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return ((RwInt32(__cdecl *)(RwInt32, RwUInt32, RwPluginObjectConstructor, RwPluginObjectDestructor, RwPluginObjectCopy))0x7F3C10)(size, pluginID, constructCB, destructCB, copyCB);
}

RwInt32 RwTextureGetPluginOffset(RwUInt32 pluginID) {
    return ((RwInt32(__cdecl *)(RwUInt32))0x7F3BE0)(pluginID);
}

RwInt32 RwTexDictionaryGetPluginOffset(RwUInt32 pluginID) {
    return ((RwInt32(__cdecl *)(RwUInt32))0x7F3C40)(pluginID);
}

RwBool RwTextureValidatePlugins(const RwTexture* texture) {
    return ((RwBool(__cdecl *)(const RwTexture*))0x7F3C00)(texture);
}

RwBool RwTexDictionaryValidatePlugins(const RwTexDictionary* dict) {
    return ((RwBool(__cdecl *)(const RwTexDictionary*))0x7F3C60)(dict);
}

rwIm3DPool* _rwIm3DGetPool() {
    return ((rwIm3DPool*(__cdecl *)(void))0x7EFDD0)();
}

void* RwIm3DTransform(RwIm3DVertex* pVerts, RwUInt32 numVerts, RwMatrix* ltm, RwUInt32 flags) {
    return ((void*(__cdecl *)(RwIm3DVertex*, RwUInt32, RwMatrix*, RwUInt32))0x7EF450)(pVerts, numVerts, ltm, flags);
}

RwBool RwIm3DEnd() {
    return ((RwBool(__cdecl *)(void))0x7EF520)();
}

RwBool RwIm3DRenderLine(RwInt32 vert1, RwInt32 vert2) {
    return ((RwBool(__cdecl *)(RwInt32, RwInt32))0x7EF900)(vert1, vert2);
}

RwBool RwIm3DRenderTriangle(RwInt32 vert1, RwInt32 vert2, RwInt32 vert3) {
    return ((RwBool(__cdecl *)(RwInt32, RwInt32, RwInt32))0x7EF810)(vert1, vert2, vert3);
}

RwBool RwIm3DRenderIndexedPrimitive(RwPrimitiveType primType, RwImVertexIndex* indices, RwInt32 numIndices) {
    return ((RwBool(__cdecl *)(RwPrimitiveType, RwImVertexIndex*, RwInt32))0x7EF550)(primType, indices, numIndices);
}

RwBool RwIm3DRenderPrimitive(RwPrimitiveType primType) {
    return ((RwBool(__cdecl *)(RwPrimitiveType))0x7EF6B0)(primType);
}

RxPipeline* RwIm3DGetTransformPipeline() {
    return ((RxPipeline*(__cdecl *)(void))0x7EF9D0)();
}

RxPipeline* RwIm3DGetRenderPipeline(RwPrimitiveType primType) {
    return ((RxPipeline*(__cdecl *)(RwPrimitiveType))0x7EF9E0)(primType);
}

RxPipeline* RwIm3DSetTransformPipeline(RxPipeline* pipeline) {
    return ((RxPipeline*(__cdecl *)(RxPipeline*))0x7EFAC0)(pipeline);
}

RxPipeline* RwIm3DSetRenderPipeline(RxPipeline* pipeline, RwPrimitiveType primType) {
    return ((RxPipeline*(__cdecl *)(RxPipeline*, RwPrimitiveType))0x7EFB10)(pipeline, primType);
}

RwRaster* RwD3D9RasterStreamReadDDS(RwStream* stream) {
    return ((RwRaster*(__cdecl *)(RwStream*))0x81F360)(stream);
}

RwTexture* RwD3D9DDSTextureRead(const RwChar* name, const RwChar* maskname) {
    return ((RwTexture*(__cdecl *)(const RwChar*, const RwChar*))0x820A10)(name, maskname);
}

RwBool RwD3D9RasterIsCompressed(const RwRaster* raster) {
    return ((RwBool(__cdecl *)(const RwRaster*))0x820C90)(raster);
}

RwBool RwD3D9DeviceSupportsDXTTexture() {
    return ((RwBool(__cdecl *)(void))0x7F9C30)();
}

void* RwD3D9GetCurrentD3DDevice() { // IDirect3DDevice9
    return ((void*(__cdecl *)(void))0x7F9D50)();
}

RwUInt32 RwD3D9EngineGetMaxMultiSamplingLevels() {
    return ((RwUInt32(__cdecl *)(void))0x7F84E0)();
}

void RwD3D9EngineSetMultiSamplingLevels(RwUInt32 numLevels) {
    ((void(__cdecl *)(RwUInt32))0x7F84F0)(numLevels);
}

void RwD3D9EngineSetRefreshRate(RwUInt32 refreshRate) {
    ((void(__cdecl *)(RwUInt32))0x7F8580)(refreshRate);
}

void RwD3D9EngineSetMultiThreadSafe(RwBool enable) {
    ((void(__cdecl *)(RwBool))0x7F8620)(enable);
}

void RwD3D9EngineSetSoftwareVertexProcessing(RwBool enable) {
    ((void(__cdecl *)(RwBool))0x7F8630)(enable);
}

void* RwD3D9GetCurrentD3DRenderTarget(RwUInt32 index) {
    return ((void*(__cdecl *)(RwUInt32))0x7F9E80)(index);
}

RwBool RwD3D9SetRenderTarget(RwUInt32 index, RwRaster* raster) {
    return ((RwBool(__cdecl *)(RwUInt32, RwRaster*))0x7F9E90)(index, raster);
}

RwBool RwD3D9ChangeVideoMode(RwInt32 modeIndex) {
    return ((RwBool(__cdecl *)(RwInt32))0x7F8640)(modeIndex);
}

RwBool RwD3D9ChangeMultiSamplingLevels(RwUInt32 numLevels) {
    return ((RwBool(__cdecl *)(RwUInt32))0x7F8A90)(numLevels);
}

RwBool RwD3D9CameraAttachWindow(void* camera, void* hwnd) {
    return ((RwBool(__cdecl *)(void*, void*))0x7F8D70)(camera, hwnd);
}

void RwD3D9SetStreamSource(RwUInt32 streamNumber, void* streamData, RwUInt32 offset, RwUInt32 stride) {
    ((void(__cdecl *)(RwUInt32, void*, RwUInt32, RwUInt32))0x7FA030)(streamNumber, streamData, offset, stride);
}

void _rwD3D9RenderStateFlushCache() {
    ((void(__cdecl *)(void))0x7FC200)();
}

void _rwD3D9DrawIndexedPrimitiveUP(RwUInt32 primitiveType, RwUInt32 minIndex, RwUInt32 numVertices, RwUInt32 primitiveCount, const void* indexData, const void* vertexStreamZeroData, RwUInt32 VertexStreamZeroStride) {
   ((void(__cdecl *)(RwUInt32, RwUInt32, RwUInt32, RwUInt32, const void*, const void*, RwUInt32))0x7FA1F0)(primitiveType, minIndex, numVertices, primitiveCount, indexData, vertexStreamZeroData, VertexStreamZeroStride);
}

void _rwD3D9DrawPrimitiveUP(RwUInt32 primitiveType, RwUInt32 primitiveCount, const void* vertexStreamZeroData, RwUInt32 VertexStreamZeroStride) {
   ((void(__cdecl *)(RwUInt32, RwUInt32, const void*, RwUInt32))0x7FA290)(primitiveType, primitiveCount, vertexStreamZeroData, VertexStreamZeroStride);
}

void _rwD3D9DrawIndexedPrimitive(RwUInt32 primitiveType, RwInt32 baseVertexIndex, RwUInt32 minIndex, RwUInt32 numVertices, RwUInt32 startIndex, RwUInt32 primitiveCount) {
   ((void(__cdecl *)(RwUInt32, RwInt32, RwUInt32, RwUInt32, RwUInt32, RwUInt32))0x7FA320)(primitiveType, baseVertexIndex, minIndex, numVertices, startIndex, primitiveCount);
}

void _rwD3D9SetVertexShaderConstant(RwUInt32 registerAddress, const void* constantData, RwUInt32 constantCount) {
   ((void(__cdecl *)(RwUInt32, const void*, RwUInt32))0x7FACA0)(registerAddress, constantData, constantCount);
}

void _rwD3D9SetPixelShaderConstant(RwUInt32 registerAddress, const void* constantData, RwUInt32 constantCount) {
   ((void(__cdecl *)(RwUInt32, const void*, RwUInt32))0x7FAD00)(registerAddress, constantData, constantCount);
}

void _rwD3D9SetFVF(RwUInt32 fvf) {
   ((void(__cdecl *)(RwUInt32))0x7F9F30)(fvf);
}

void _rwD3D9SetVertexShader(void *shader) {
   ((void(__cdecl *)(void *))0x7F9FB0)(shader);
}

void _rwD3D9SetPixelShader(void *shader) {
   ((void(__cdecl *)(void *))0x7F9FF0)(shader);
}

void RwD3D9SetRenderState(RwUInt32 state, RwUInt32 value) {
   ((void(__cdecl *)(RwUInt32, RwUInt32))0x7FC2D0)(state, value);
}

void RwD3D9GetRenderState(RwUInt32 state, void* value) {
   ((void(__cdecl *)(RwUInt32, void*))0x7FC320)(state, value);
}

void RwD3D9SetTextureStageState(RwUInt32 stage, RwUInt32 type, RwUInt32 value) {
   ((void(__cdecl *)(RwUInt32, RwUInt32, RwUInt32))0x7FC340)(stage, type, value);
}

void RwD3D9GetTextureStageState(RwUInt32 stage, RwUInt32 type, void* value) {
   ((void(__cdecl *)(RwUInt32, RwUInt32, void*))0x7FC3A0)(stage, type, value);
}

void RwD3D9SetSamplerState(RwUInt32 stage, RwUInt32 type, RwUInt32 value) {
   ((void(__cdecl *)(RwUInt32, RwUInt32, RwUInt32))0x7FC3C0)(stage, type, value);
}

void RwD3D9GetSamplerState(RwUInt32 stage, RwUInt32 type, void* value) {
   ((void(__cdecl *)(RwUInt32, RwUInt32, void*))0x7FC400)(stage, type, value);
}

void RwD3D9SetStencilClear(RwUInt32 stencilClear) {
   ((void(__cdecl *)(RwUInt32))0x7F9D30)(stencilClear);
}

RwUInt32 RwD3D9GetStencilClear() {
   return ((RwUInt32(__cdecl *)(void))0x7F9D40)();
}

RwBool RwD3D9SetTexture(RwTexture* texture, RwUInt32 stage) {
   return ((RwBool(__cdecl *)(RwTexture*, RwUInt32))0x7FDE70)(texture, stage);
}

RwBool RwD3D9SetTransform(RwUInt32 state, const void* matrix) {
   return ((RwBool(__cdecl *)(RwUInt32, const void*))0x7FA390)(state, matrix);
}

void RwD3D9GetTransform(RwUInt32 state, void* matrix) {
   ((void(__cdecl *)(RwUInt32, void*))0x7FA4F0)(state, matrix);
}

RwBool RwD3D9SetMaterial(const void* material) {
   return ((RwBool(__cdecl *)(const void*))0x7FC430)(material);
}

RwBool RwD3D9SetClipPlane(RwUInt32 index, const RwV4d* plane) {
   return ((RwBool(__cdecl *)(RwUInt32, const RwV4d*))0x7FC4A0)(index, plane);
}

RwBool RwD3D9SetTransformWorld(const RwMatrix* matrix) {
   return ((RwBool(__cdecl *)(const RwMatrix*))0x7FA520)(matrix);
}

RwBool RwD3D9SetSurfaceProperties(const RwSurfaceProperties* surfaceProps, const RwRGBA* color, RwUInt32 flags) {
   return ((RwBool(__cdecl *)(const RwSurfaceProperties*, const RwRGBA*, RwUInt32))0x7FC4D0)(surfaceProps, color, flags);
}

RwBool RwD3D9SetLight(RwInt32 index, const void* light) {
   return ((RwBool(__cdecl *)(RwInt32, const void*))0x7FA660)(index, light);
}

void RwD3D9GetLight(RwInt32 index, void* light) {
   ((void(__cdecl *)(RwInt32, void*))0x7FA820)(index, light);
}

RwBool RwD3D9EnableLight(RwInt32 index, RwBool enable) {
   return ((RwBool(__cdecl *)(RwInt32, RwBool))0x7FA860)(index, enable);
}

RwBool RwD3D9IndexBufferCreate(RwUInt32 numIndices, void* indexBuffer) {
   return ((RwBool(__cdecl *)(RwUInt32, void*))0x4C9970)(numIndices, indexBuffer);
}

RwBool RwD3D9CreateVertexDeclaration(const void* elements, void* vertexdeclaration) {
   return ((RwBool(__cdecl *)(const void*, void*))0x7FAA30)(elements, vertexdeclaration);
}

void RwD3D9DeleteVertexDeclaration() {
   ((void(__cdecl *)(void))0x7FAC10)();
}

void RwD3D9DeleteVertexShader() {
   ((void(__cdecl *)(void))0x7FAC90)();
}

RwBool RwD3D9CreatePixelShader(const RwUInt32* function, void* shader) {
   return ((RwBool(__cdecl *)(const RwUInt32*, void*))0x7FACC0)(function, shader);
}

void RwD3D9DeletePixelShader() {
   ((void(__cdecl *)(void))0x7FACF0)();
}

void _rwD3D9SetStreams(const RxD3D9VertexStream* streams, RwBool useOffsets) {
    plugin::CallAndReturn<void, 0x7FA090>(streams, useOffsets);
}

const void* RwD3D9GetCaps() { // D3DCAPS9*
   return ((const void*(__cdecl *)(void))0x7FAD20)();
}

RwBool RwD3D9CameraIsSphereFullyInsideFrustum(const void* camera, const void* sphere) {
   return ((RwBool(__cdecl *)(const void*, const void*))0x7FAD30)(camera, sphere);
}

RwBool RwD3D9CameraIsBBoxFullyInsideFrustum(const void* camera, const void* boundingBox) {
   return ((RwBool(__cdecl *)(const void*, const void*))0x7FAD90)(camera, boundingBox);
}

void _rwD3D9RasterConvertToNonPalettized(RwRaster* raster) {
   ((void(__cdecl *)(RwRaster*))0x4CD250)(raster);
}

RwBool _rwDeviceRegisterPlugin() {
   return ((RwBool(__cdecl *)(void))0x7F5F60)();
}

void _rwD3D9DeviceSetRestoreCallback(rwD3D9DeviceRestoreCallBack callback) {
   ((void(__cdecl *)(rwD3D9DeviceRestoreCallBack))0x7FAE20)(callback);
}

rwD3D9DeviceRestoreCallBack _rwD3D9DeviceGetRestoreCallback() {
   return ((rwD3D9DeviceRestoreCallBack(__cdecl *)(void))0x7FAE30)();
}

/* 0x80CA20: Static helper of RwImageResample -
 * bilinearly samples a horizontal span of `image` (16.16 fixed point coords) into a float RGBA quad (range 0..1) */
static void _rwImageResampleSpan(const RwImage* image, RwInt32 x1, RwInt32 x2, RwInt32 y, float* out) {
    constexpr float INV_65536 = 1.0f / 65536.0f;
    constexpr float INV_255   = 1.0f / 255.0f;

    const RwReal totalWeight = (RwReal)(x2 - x1) * INV_65536;
    auto px = (const RwUInt8*)(image->stride * (y >> 16) + ((x1 >> 16) * 4) + image->cpPixels);

    if ((x1 >> 16) == (x2 >> 16)) {
        // Single pixel span
        for (int i = 0; i < 4; i++) {
            out[i] = (RwReal)px[i] * INV_255;
            out[i] = out[i] * totalWeight;
        }
    } else {
        // First, partial pixel
        RwInt32 x = (x1 & 0xFFFF0000) + 0x10000;
        for (int i = 0; i < 4; i++) {
            out[i] = (RwReal)px[i] * INV_255;
        }
        const RwReal w1 = (RwReal)(x - x1) * INV_65536;
        for (int i = 0; i < 4; i++) {
            out[i] = out[i] * w1;
        }
        // Whole pixels in the middle
        for (; (x >> 16) != (x2 >> 16); x += 0x10000) {
            for (int i = 0; i < 4; i++) {
                out[i] = out[i] + (RwReal)px[4 + i] * INV_255;
            }
            px += 4;
        }
        // Last, partial pixel
        const RwReal w2 = (RwReal)(x2 - x) * INV_65536;
        for (int i = 0; i < 4; i++) {
            out[i] = out[i] + (RwReal)px[4 + i] * INV_255 * w2;
        }
    }

    const RwReal normalize = 1.0f / totalWeight;
    for (int i = 0; i < 4; i++) {
        out[i] = out[i] * normalize;
    }
}

/* 0x80C600 */
RwImage* RwImageResample(RwImage* dstImage, const RwImage* srcImage) {
    ZoneScoped;
    const RwInt32 dstWidth  = dstImage->width;
    const RwInt32 srcHeight = srcImage->height;
    const RwInt32 srcWidth  = srcImage->width;
    const RwInt32 dstHeight = dstImage->height;

    dstImage->flags = dstImage->flags | (srcImage->flags & 2);

    const RwInt32 xStep = (RwInt32)(((RwReal)srcWidth / (RwReal)dstWidth) * 65536.0f);
    const RwInt32 yStep = (RwInt32)(((RwReal)srcHeight / (RwReal)dstHeight) * 65536.0f);

    RwInt32 y = 0;
    for (RwInt32 row = 0; row < dstHeight; row++) {
        if (dstWidth > 0) {
            RwInt32 x = 0;
            const RwInt32 yFracWeight = yStep - 1 + y;
            const RwInt32 yPix        = yFracWeight >> 16;
            const RwReal  yWeight     = (RwReal)(yFracWeight - y) * (1.0f / 65536.0f);
            auto out = (RwUInt8*)(dstImage->stride * row + dstImage->cpPixels + 2);

            for (RwInt32 col = 0; col < dstWidth; col++) {
                float acc[4];
                RwInt32 xFracWeight = xStep - 1 + x;
                const RwInt32 xPix  = xFracWeight >> 16;

                if ((y >> 16) == yPix) {
                    _rwImageResampleSpan(srcImage, x, xFracWeight, y, acc);
                    for (int i = 0; i < 4; i++) {
                        acc[i] = acc[i] * yWeight;
                    }
                } else {
                    RwInt32 yy = (y & 0xFFFF0000) + 0x10000;
                    _rwImageResampleSpan(srcImage, x, xFracWeight, y, acc);
                    const RwReal w1 = (RwReal)(yy - y) * (1.0f / 65536.0f);
                    for (int i = 0; i < 4; i++) {
                        acc[i] = acc[i] * w1;
                    }
                    for (; (yy >> 16) != yPix; yy += 0x10000) {
                        float tmp[4];
                        _rwImageResampleSpan(srcImage, x, xFracWeight, yy, tmp);
                        for (int i = 0; i < 4; i++) {
                            acc[i] = acc[i] + tmp[i];
                        }
                    }
                    float tmp[4];
                    _rwImageResampleSpan(srcImage, x, xFracWeight, yy, tmp);
                    const RwReal w2 = (RwReal)(yFracWeight - yy) * (1.0f / 65536.0f);
                    for (int i = 0; i < 4; i++) {
                        acc[i] = acc[i] + tmp[i] * w2;
                    }
                }

                const RwReal invYWeight = 1.0f / yWeight;
                RwUInt8 outPx[4];
                for (int i = 0; i < 4; i++) {
                    acc[i] = acc[i] * invYWeight;
                    outPx[i] = (RwUInt8)(RwInt32)(acc[i] * 255.0f + 0.5f);
                }
                out[-2] = outPx[0];
                out[-1] = outPx[1];
                out[0]  = outPx[2];
                out[1]  = outPx[3];

                x += xStep;
                out += 4;
            }
        }
        y += yStep;
    }
    return dstImage;
}

/* 0x80CD10 */
RwImage* RwImageCreateResample(const RwImage* srcImage, RwInt32 width, RwInt32 height) {
    ZoneScoped;
    auto dstImage = RwImageCreate(width, height, 32);
    if (!dstImage) {
        return nullptr;
    }
    if (!RwImageAllocatePixels(dstImage)) {
        RwImageDestroy(dstImage);
        return nullptr;
    }

    if (srcImage->depth == 32) {
        if (RwImageResample(dstImage, srcImage)) {
            return dstImage;
        }
    } else {
        // Convert the source image to 32bpp first (via a helper image)
        auto tmpImage = RwImageCreate(srcImage->width, srcImage->height, 32);
        if (tmpImage) {
            if (RwImageAllocatePixels(tmpImage)) {
                ((RwImage * (__cdecl *)(RwImage*, const RwImage*))0x803950)(tmpImage, srcImage); /* _rwImageConvertTo32bpp (unreversed) */
                if (RwImageResample(dstImage, tmpImage)) {
                    RwImageFreePixels(tmpImage);
                    RwImageDestroy(tmpImage);
                    return dstImage;
                }
                RwImageFreePixels(tmpImage);
            }
            RwImageDestroy(tmpImage);
        }
    }
    RwImageFreePixels(dstImage);
    RwImageDestroy(dstImage);
    return nullptr;
}

RwImage* RwImageSetFromRaster(RwImage* image, RwRaster* raster) {
    return ((RwImage*(__cdecl *)(RwImage*, RwRaster*))0x804250)(image, raster);
}

RwRaster* RwRasterSetFromImage(RwRaster* raster, RwImage* image) {
    return ((RwRaster*(__cdecl *)(RwRaster*, RwImage*))0x804290)(raster, image);
}

RwRaster* RwRasterRead(const RwChar* filename) {
    return ((RwRaster*(__cdecl *)(const RwChar*))0x8043F0)(filename);
}

RwRaster* RwRasterReadMaskedRaster(const RwChar* filename, const RwChar* maskname) {
    return ((RwRaster*(__cdecl *)(const RwChar*, const RwChar*))0x8044E0)(filename, maskname);
}

RwImage* RwImageFindRasterFormat(RwImage* ipImage, RwInt32 nRasterType, RwInt32* npWidth, RwInt32* npHeight, RwInt32* npDepth, RwInt32* npFormat) {
    return ((RwImage*(__cdecl *)(RwImage*, RwInt32, RwInt32*, RwInt32*, RwInt32*, RwInt32*))0x8042C0)(ipImage, nRasterType, npWidth, npHeight, npDepth, npFormat);
}

RwFrame* RwFrameForAllObjects(RwFrame* frame, RwObjectCallBack callBack, void* data) {
    return ((RwFrame*(__cdecl *)(RwFrame*, RwObjectCallBack, void*))0x7F1200)(frame, callBack, data);
}

RwFrame* RwFrameTranslate(RwFrame* frame, const RwV3d* v, RwOpCombineType combine) {
    return ((RwFrame*(__cdecl *)(RwFrame*, const RwV3d*, RwOpCombineType))0x7F0E30)(frame, v, combine);
}

RwFrame* RwFrameRotate(RwFrame* frame, const RwV3d* axis, RwReal angle, RwOpCombineType combine) {
    return ((RwFrame*(__cdecl *)(RwFrame*, const RwV3d*, RwReal, RwOpCombineType))0x7F1010)(frame, axis, angle, combine);
}

RwFrame* RwFrameScale(RwFrame* frame, const RwV3d* v, RwOpCombineType combine) {
    return ((RwFrame*(__cdecl *)(RwFrame*, const RwV3d*, RwOpCombineType))0x7F0ED0)(frame, v, combine);
}

RwFrame* RwFrameTransform(RwFrame* frame, const RwMatrix* m, RwOpCombineType combine) {
    return ((RwFrame*(__cdecl *)(RwFrame*, const RwMatrix*, RwOpCombineType))0x7F0F70)(frame, m, combine);
}

RwFrame* RwFrameOrthoNormalize(RwFrame* frame) {
    return ((RwFrame*(__cdecl *)(RwFrame*))0x7F1170)(frame);
}

RwFrame* RwFrameSetIdentity(RwFrame* frame) {
    return ((RwFrame*(__cdecl *)(RwFrame*))0x7F10B0)(frame);
}

RwFrame* RwFrameCloneHierarchy(RwFrame* root) {
    return ((RwFrame*(__cdecl *)(RwFrame*))0x7F0250)(root);
}

RwBool RwFrameDestroyHierarchy(RwFrame* frame) {
    return ((RwBool(__cdecl *)(RwFrame*))0x7F08A0)(frame);
}

RwFrame* RwFrameForAllChildren(RwFrame* frame, RwFrameCallBack callBack, void* data) {
    return ((RwFrame*(__cdecl *)(RwFrame*, RwFrameCallBack, void*))0x7F0DC0)(frame, callBack, data);
}

RwFrame* RwFrameRemoveChild(RwFrame* child) {
    return ((RwFrame*(__cdecl *)(RwFrame*))0x7F0CD0)(child);
}

RwFrame* RwFrameAddChild(RwFrame* parent, RwFrame* child) {
    return ((RwFrame*(__cdecl *)(RwFrame*, RwFrame*))0x7F0B00)(parent, child);
}

RwFrame* RwFrameAddChildNoUpdate(RwFrame* parent, RwFrame* child) {
    return ((RwFrame*(__cdecl *)(RwFrame*, RwFrame*))0x7F09C0)(parent, child);
}

RwFrame* RwFrameGetRoot(const RwFrame* frame) {
    return ((RwFrame*(__cdecl *)(const RwFrame*))0x7F09B0)(frame);
}

RwMatrix* RwFrameGetLTM(RwFrame* frame) {
    return ((RwMatrix*(__cdecl *)(RwFrame*))0x7F0990)(frame);
}

RwFrame* RwFrameUpdateObjects(RwFrame* frame) {
    return ((RwFrame*(__cdecl *)(RwFrame*))0x7F0910)(frame);
}

void RwFrameSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    ((void(__cdecl *)(RwInt32, RwInt32))0x7EFED0)(blockSize, numBlocksToPrealloc);
}

RwFrame* RwFrameCreate() {
    return ((RwFrame*(__cdecl *)(void))0x7F0410)();
}

RwBool RwFrameDestroy(RwFrame* frame) {
    assert(frame);
    return ((RwBool(__cdecl *)(RwFrame*))0x7F05A0)(frame);
}

RwBool _rwFrameInit(RwFrame* frame) {
    return ((RwBool(__cdecl *)(RwFrame*))0x7F0450)(frame);
}

RwBool _rwFrameDeInit(RwFrame* frame) {
    return ((RwBool(__cdecl *)(RwFrame*))0x7F06F0)(frame);
}

RwBool RwFrameDirty(const RwFrame* frame) {
    return ((RwBool(__cdecl *)(const RwFrame*))0x7F0340)(frame);
}

RwInt32 RwFrameCount(RwFrame* frame) {
    return ((RwInt32(__cdecl *)(RwFrame*))0x7F0E00)(frame);
}

RwInt32 RwFrameRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return ((RwInt32(__cdecl *)(RwInt32, RwUInt32, RwPluginObjectConstructor, RwPluginObjectDestructor, RwPluginObjectCopy))0x7F1260)(size, pluginID, constructCB, destructCB, copyCB);
}

RwInt32 RwFrameGetPluginOffset(RwUInt32 pluginID) {
    return ((RwInt32(__cdecl *)(RwUInt32))0x7F1290)(pluginID);
}

RwBool RwFrameValidatePlugins(const RwFrame* frame) {
    return ((RwBool(__cdecl *)(const RwFrame*))0x7F12B0)(frame);
}

RwFrame* _rwFrameCloneAndLinkClones(RwFrame* root) {
    return ((RwFrame*(__cdecl *)(RwFrame*))0x7EFFB0)(root);
}

RwFrame* _rwFramePurgeClone(RwFrame* root) {
    return ((RwFrame*(__cdecl *)(RwFrame*))0x7F01A0)(root);
}

void _rwObjectHasFrameReleaseFrame() {
    ((void(__cdecl *)(void))0x804F40)();
}

RwBool _rwFrameSyncDirty() {
    return ((RwBool(__cdecl *)(void))0x809550)();
}

RwInt32 RwTextureRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return ((RwInt32(__cdecl *)(RwUInt32, RwPluginDataChunkReadCallBack, RwPluginDataChunkWriteCallBack, RwPluginDataChunkGetSizeCallBack))0x804550)(pluginID, readCB, writeCB, getSizeCB);
}

RwInt32 RwTextureSetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB) {
    return ((RwInt32(__cdecl *)(RwUInt32, RwPluginDataChunkAlwaysCallBack))0x804580)(pluginID, alwaysCB);
}

RwUInt32 RwTextureStreamGetSize(const RwTexture* texture) {
    return ((RwUInt32(__cdecl *)(const RwTexture*))0x8045A0)(texture);
}

RwTexture* RwTextureStreamRead(RwStream* stream) {
    return ((RwTexture*(__cdecl *)(RwStream*))0x8046E0)(stream);
}

const RwTexture* RwTextureStreamWrite(const RwTexture* texture, RwStream* stream) {
    return ((const RwTexture*(__cdecl *)(const RwTexture*, RwStream*))0x8045E0)(texture, stream);
}

RwInt32 RwTexDictionaryRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return ((RwInt32(__cdecl *)(RwUInt32, RwPluginDataChunkReadCallBack, RwPluginDataChunkWriteCallBack, RwPluginDataChunkGetSizeCallBack))0x8048E0)(pluginID, readCB, writeCB, getSizeCB);
}

RwInt32 RwTexDictionarySetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB) {
    return ((RwInt32(__cdecl *)(RwUInt32, RwPluginDataChunkAlwaysCallBack))0x804910)(pluginID, alwaysCB);
}

RwUInt32 RwTexDictionaryStreamGetSize(const RwTexDictionary* texDict) {
    return ((RwUInt32(__cdecl *)(const RwTexDictionary*))0x804930)(texDict);
}

RwTexDictionary* RwTexDictionaryStreamRead(RwStream* stream) {
    return ((RwTexDictionary*(__cdecl *)(RwStream*))0x804C30)(stream);
}

const RwTexDictionary* RwTexDictionaryStreamWrite(const RwTexDictionary* texDict, RwStream* stream) {
    return ((const RwTexDictionary*(__cdecl *)(const RwTexDictionary*, RwStream*))0x8049F0)(texDict, stream);
}

RwTextureChunkInfo* _rwTextureChunkInfoRead(RwStream* stream, RwTextureChunkInfo* textureChunkInfo, RwInt32* bytesRead) {
    return ((RwTextureChunkInfo*(__cdecl *)(RwStream*, RwTextureChunkInfo*, RwInt32*))0x804E60)(stream, textureChunkInfo, bytesRead);
}

void RwFrameListSetAutoUpdate(RwBool flag) {
    ((void(__cdecl *)(RwBool))0x807570)(flag);
}

RwInt32 RwFrameRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return ((RwInt32(__cdecl *)(RwUInt32, RwPluginDataChunkReadCallBack, RwPluginDataChunkWriteCallBack, RwPluginDataChunkGetSizeCallBack))0x807580)(pluginID, readCB, writeCB, getSizeCB);
}

RwInt32 RwFrameSetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB) {
    return ((RwInt32(__cdecl *)(RwUInt32, RwPluginDataChunkAlwaysCallBack))0x8075B0)(pluginID, alwaysCB);
}

rwFrameList* _rwFrameListInitialize(rwFrameList* frameList, RwFrame* frame) {
    return ((rwFrameList*(__cdecl *)(rwFrameList*, RwFrame*))0x8075D0)(frameList, frame);
}

RwBool _rwFrameListFindFrame(const rwFrameList* frameList, const RwFrame* frame, RwInt32* npIndex) {
    return ((RwBool(__cdecl *)(const rwFrameList*, const RwFrame*, RwInt32*))0x8076E0)(frameList, frame, npIndex);
}

rwFrameList* _rwFrameListDeinitialize(rwFrameList* frameList) {
    return ((rwFrameList*(__cdecl *)(rwFrameList*))0x807720)(frameList);
}

RwUInt32 _rwFrameListStreamGetSize(const rwFrameList* frameList) {
    return ((RwUInt32(__cdecl *)(const rwFrameList*))0x807750)(frameList);
}

rwFrameList* _rwFrameListStreamRead(RwStream* stream, rwFrameList* fl) {
    return ((rwFrameList*(__cdecl *)(RwStream*, rwFrameList*))0x807970)(stream, fl);
}

const rwFrameList* _rwFrameListStreamWrite(const rwFrameList* frameList, RwStream* stream) {
    return ((const rwFrameList*(__cdecl *)(const rwFrameList*, RwStream*))0x8077A0)(frameList, stream);
}

RwBBox* RwBBoxCalculate(RwBBox* boundBox, const RwV3d* verts, RwInt32 numVerts) {
    return ((RwBBox*(__cdecl *)(RwBBox*, const RwV3d*, RwInt32))0x808F60)(boundBox, verts, numVerts);
}

RwBBox* RwBBoxInitialize(RwBBox* boundBox, const RwV3d* vertex) {
    return ((RwBBox*(__cdecl *)(RwBBox*, const RwV3d*))0x809020)(boundBox, vertex);
}

RwBBox* RwBBoxAddPoint(RwBBox* boundBox, const RwV3d* vertex) {
    return ((RwBBox*(__cdecl *)(RwBBox*, const RwV3d*))0x809060)(boundBox, vertex);
}

RwBool RwBBoxContainsPoint(const RwBBox* boundBox, const RwV3d* vertex) {
    return ((RwBool(__cdecl *)(const RwBBox*, const RwV3d*))0x8090E0)(boundBox, vertex);
}

RwCamera* RwCameraBeginUpdate(RwCamera* camera) {
    ZoneScoped;
    return camera->beginUpdate(camera);
}

RwCamera* RwCameraEndUpdate(RwCamera* camera) {
    ZoneScoped;
    return camera->endUpdate(camera);
}

RwCamera* RwCameraClear(RwCamera* camera, RwRGBA* colour, RwInt32 clearMode) {
    ZoneScoped;
    return RwEngineInstance->stdFunc[rwSTANDARDCAMERACLEAR](camera, colour, clearMode) ? camera : nullptr;
}

RwCamera* RwCameraShowRaster(RwCamera* camera, void* pDev, RwUInt32 flags) {
    ZoneScoped;
    return RwRasterShowRaster(RwCameraGetRaster(camera), pDev, flags) != 0 ? camera : nullptr;
}

void RwCameraSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    ((void(__cdecl *)(RwInt32, RwInt32))0x7EE0F0)(blockSize, numBlocksToPrealloc);
}

RwBool RwCameraDestroy(RwCamera* camera) {
    return ((RwBool(__cdecl *)(RwCamera*))0x7EE4B0)(camera);
}

RwCamera* RwCameraCreate() {
    return ((RwCamera*(__cdecl *)(void))0x7EE4F0)();
}

RwCamera* RwCameraClone(RwCamera* camera) {
    return ((RwCamera*(__cdecl *)(RwCamera*))0x7EF3B0)(camera);
}

RwCamera* RwCameraSetViewOffset(RwCamera* camera, const RwV2d* offset) {
    return ((RwCamera*(__cdecl *)(RwCamera*, const RwV2d*))0x7EE1A0)(camera, offset);
}

RwCamera* RwCameraSetViewWindow(RwCamera* camera, const RwV2d* viewWindow) {
    return ((RwCamera*(__cdecl *)(RwCamera*, const RwV2d*))0x7EE410)(camera, viewWindow);
}

RwCamera* RwCameraSetProjection(RwCamera* camera, RwCameraProjection projection) {
    return ((RwCamera*(__cdecl *)(RwCamera*, RwCameraProjection))0x7EE3A0)(camera, projection);
}

RwCamera* RwCameraSetNearClipPlane(RwCamera* camera, RwReal nearClip) {
    return ((RwCamera*(__cdecl *)(RwCamera*, RwReal))0x7EE1D0)(camera, nearClip);
}

RwCamera* RwCameraSetFarClipPlane(RwCamera* camera, RwReal farClip) {
    return ((RwCamera*(__cdecl *)(RwCamera*, RwReal))0x7EE2A0)(camera, farClip);
}

RwInt32 RwCameraRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return ((RwInt32(__cdecl *)(RwInt32, RwUInt32, RwPluginObjectConstructor, RwPluginObjectDestructor, RwPluginObjectCopy))0x7EE450)(size, pluginID, constructCB, destructCB, copyCB);
}

RwInt32 RwCameraGetPluginOffset(RwUInt32 pluginID) {
    return ((RwInt32(__cdecl *)(RwUInt32))0x7EE480)(pluginID);
}

RwBool RwCameraValidatePlugins(const RwCamera* camera) {
    return ((RwBool(__cdecl *)(const RwCamera*))0x7EE4A0)(camera);
}

RwFrustumTestResult RwCameraFrustumTestSphere(const RwCamera* camera, const RwSphere* sphere) {
    return ((RwFrustumTestResult(__cdecl *)(const RwCamera*, const RwSphere*))0x7EE2D0)(camera, sphere);
}

RwInt32 RwCameraRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return ((RwInt32(__cdecl *)(RwUInt32, RwPluginDataChunkReadCallBack, RwPluginDataChunkWriteCallBack, RwPluginDataChunkGetSizeCallBack))0x808C90)(pluginID, readCB, writeCB, getSizeCB);
}

RwInt32 RwCameraSetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB) {
    return ((RwInt32(__cdecl *)(RwUInt32, RwPluginDataChunkAlwaysCallBack))0x808CC0)(pluginID, alwaysCB);
}

RwUInt32 RwCameraStreamGetSize(const RwCamera* camera) {
    return ((RwUInt32(__cdecl *)(const RwCamera*))0x808CE0)(camera);
}

RwCamera* RwCameraStreamRead(RwStream* stream) {
    return ((RwCamera*(__cdecl *)(RwStream*))0x808DE0)(stream);
}

const RwCamera* RwCameraStreamWrite(const RwCamera* camera, RwStream* stream) {
    return ((const RwCamera*(__cdecl *)(const RwCamera*, RwStream*))0x808D00)(camera, stream);
}

RwCameraChunkInfo* RwCameraChunkInfoRead(RwStream* stream, RwCameraChunkInfo* cameraChunkInfo, RwInt32* bytesRead) {
    return ((RwCameraChunkInfo*(__cdecl *)(RwStream*, RwCameraChunkInfo*, RwInt32*))0x808EF0)(stream, cameraChunkInfo, bytesRead);
}

void _rwObjectHasFrameSetFrame(void *object, RwFrame *frame) {
    plugin::Call<0x804EF0, void*, RwFrame*>(object, frame);
}

void _rwObjectHasFrameReleaseFrame(void *object) {
    plugin::Call<0x804F40, void*>(object);
}

RwUInt32 RwRGBAToPixel(RwRGBA* rgbIn, RwInt32 rasterFormat) {
    return plugin::CallAndReturn<RwUInt32, 0x803740, RwRGBA*, RwInt32>(rgbIn, rasterFormat);
}

/****************************************************************************
 * Frame internals (identified from binary; slice work, see night2 report)  *
 ****************************************************************************/

/* 0x809700: Recursively synchronizes LTM of a frame and its children,
 * invoked by _rwFrameSyncDirty() (0x809550) for every dirty frame.
 * Name taken from the RW SDK header (rwcore.h). */
void _rwFrameSyncHierarchyLTM(RwFrame* frame) {
    ((void(__cdecl*)(RwFrame*, RwUInt32))0x809700)(frame, 0);
}

/* 0x8076A0: Recursive helper of _rwFrameListInitialize() (0x8075D0);
 * appends frame and all its descendants to the linear frame list. */
RwBool _rwFrameListAddFrameRecursive(RwFrame** listCursor, RwFrame* frame) {
    return ((RwBool(__cdecl*)(RwFrame**, RwFrame*))0x8076A0)(listCursor, frame);
}

/* 0x802F60: Returns the current image file search path (set by RwImageSetPath).
 * Name taken from the RW SDK header (rwcore.h). */
RwChar* RwImageGetPath() {
    return ((RwChar*(__cdecl*)(void))0x802F60)();
}

/****************************************************************************
 * D3D9 driver raster<->image conversion (driver/d3d9 raster.c equivalents) *
 * Identified by decompilation: D3DFORMAT tables, _RwD3D9RasterExtOffset    *
 * usage, RwRasterLock/Unlock + RwPalQuant* calls, and the                  *
 * "Conversion to/from compressed rasters is not supported" error strings.  *
 ****************************************************************************/

/* 0x8001E0: Fills a D3D9 raster from an image (raster->image depth
 * conversion, palettized vs true color dispatch). Errors on compressed
 * (DXT) destination rasters. */
RwBool _rwD3D9RasterSetFromImage(RwRaster* raster, RwImage* image) {
    return ((RwBool(__cdecl*)(RwRaster*, RwImage*))0x8001E0)(raster, image);
}

/* 0x800C80: Palettized (4/8bpp) path of _rwD3D9RasterSetFromImage;
 * quantizes via RwPalQuantAddImage/ResolvePalette/MatchImage. */
void _rwD3D9RasterSetFromImagePalettized(RwRaster* raster, RwImage* image) {
    ((void(__cdecl*)(RwRaster*, RwImage*))0x800C80)(raster, image);
}

/* 0x801390: True-color (24/32bpp) path of _rwD3D9RasterSetFromImage. */
void _rwD3D9RasterSetFromImageNonPalettized(RwRaster* raster, RwImage* image) {
    ((void(__cdecl*)(RwRaster*, RwImage*))0x801390)(raster, image);
}

/* 0x7FF270: Reads a raster back into an image. Errors on compressed
 * rasters and on palettizing conversions; dispatches per image depth. */
RwBool _rwD3D9RasterGetImage(RwImage* image, RwRaster* raster) {
    return ((RwBool(__cdecl*)(RwImage*, RwRaster*))0x7FF270)(image, raster);
}

/* 0x7FF450: 32bpp destination path of _rwD3D9RasterGetImage
 * (expands 4/8bpp palettes to RGBA). */
void _rwD3D9RasterGetImageTrueColor(RwImage* image, RwRaster* raster) {
    ((void(__cdecl*)(RwImage*, RwRaster*))0x7FF450)(image, raster);
}

/* 0x7FF8C0: 16bpp destination path of _rwD3D9RasterGetImage
 * (per-D3DFMT unpack switch). */
void _rwD3D9RasterGetImage16bpp(RwImage* image, RwRaster* raster) {
    ((void(__cdecl*)(RwImage*, RwRaster*))0x7FF8C0)(image, raster);
}

/* 0x7FFFF0: Scans image alpha and checks D3D9 device format support
 * (via RwD3D9CheckDeviceFormat-ish helper at 0x4CBDE0) to pick the
 * rwRASTERFORMAT mask a raster of this image should use. */
RwUInt32 _rwD3D9ImageFindUsedFormat(RwImage* image) {
    return ((RwUInt32(__cdecl*)(RwImage*))0x7FFFF0)(image);
}

/* 0x7FF070: Converts one pixel from a rw raster format (0x100=555,
 * 0x200=565, 0x300=4444, 0x400=luminance, 0x500=888, 0x600=8888, ...)
 * into RGBA8888. */
RwBool _rwD3D9ConvertPixelFromRasterFormat(void* dstPixel, const void* srcPixel, RwUInt32 rasterFormat) {
    return ((RwBool(__cdecl*)(void*, const void*, RwUInt32))0x7FF070)(dstPixel, srcPixel, rasterFormat);
}

/* 0x7FEE20: Inverse of _rwD3D9ConvertPixelFromRasterFormat; packs an
 * RGBA8888 pixel into the target raster format (computes luminance for
 * L-format). */
RwBool _rwD3D9ConvertPixelToRasterFormat(RwUInt32* dstPixel, const RwUInt8* rgba, RwUInt32 rasterFormat) {
    return ((RwBool(__cdecl*)(RwUInt32*, const RwUInt8*, RwUInt32))0x7FEE20)(dstPixel, rgba, rasterFormat);
}

/* 0x7FED70: Clamps/rounds texture dimensions to device limits
 * (max texture size globals 0xC9BF58/0xC9BF5C, D3DPTEXTURECAPS_POW2 in
 * 0xC9BF3C) - rounds up to next power of two when the device needs it. */
void _rwD3D9AdjustTextureDimensions(RwInt32* width, RwInt32* height, RwUInt32 flags) {
    ((void(__cdecl*)(RwInt32*, RwInt32*, RwUInt32))0x7FED70)(width, height, flags);
}

/* 0x7FEC70: Cached shade-mode render state setter used by the driver's
 * RwRenderStateSet (0x7FE420); queues D3DRS_SHADEMODE (9) in the driver's
 * dirty state list (0xC98B40). */
RwBool _rwD3D9RenderStateSetShadeMode(RwInt32 shadeMode) {
    return ((RwBool(__cdecl*)(RwInt32))0x7FEC70)(shadeMode);
}

/* 0x7FED00: Cached cull-mode render state setter; queues D3DRS_CULLMODE
 * (22) in the driver's dirty state list. */
RwBool _rwD3D9RenderStateSetCullMode(RwInt32 cullMode) {
    return ((RwBool(__cdecl*)(RwInt32))0x7FED00)(cullMode);
}

/* 0x7FECC0: Cached mag-filter sampler setter; applies directly via
 * IDirect3DDevice9::SetSamplerState(0, D3DSAMP_MAGFILTER, filter). */
RwBool _rwD3D9TextureSetMagFilter(RwUInt32 d3dMagFilter) {
    return ((RwBool(__cdecl*)(RwUInt32))0x7FECC0)(d3dMagFilter);
}

/* ---------------------------------------------------------------------------
 * IM3D default pipelines (identified from binary 2026-09-27)
 * --------------------------------------------------------------------------- */

/* 0x80E990: Address of the IM3D transform node definition ("ImmInstance.csl") */
static RxNodeDefinition* _rwIm3DGetTransformNodeDefinition() {
    return &StaticRef<RxNodeDefinition>(0x8E2918);
}

/* 0x80E9A0: Address of the IM3D render node definition ("nodeD3D9SubmitNoLight.csl") */
static RxNodeDefinition* _rwIm3DGetRenderNodeDefinition() {
    return &StaticRef<RxNodeDefinition>(0x8E2978);
}

/* 0x80A0A0 */
RwBool _rwIm3DTransformPipeOpen(RxPipeline** pipeline) {
    ZoneScoped;
    if (auto pipe = RxPipelineCreate()) {
        pipe->pluginId = 1;
        if (auto locked = RxPipelineLock(pipe)) {
            locked = RxLockedPipeAddFragment(locked, nullptr, _rwIm3DGetTransformNodeDefinition());
            if (auto unlocked = RxLockedPipeUnlock(locked)) {
                *pipeline = unlocked;
                RwIm3DSetTransformPipeline(unlocked);
                return TRUE;
            }
        }
        _rxPipelineDestroy(pipe);
    }
    return FALSE;
}

/* 0x80A110 */
void _rwIm3DTransformPipeClose(RxPipeline** pipeline) {
    ZoneScoped;
    RwIm3DSetTransformPipeline(nullptr);
    if (*pipeline != nullptr) {
        _rxPipelineDestroy(*pipeline);
        *pipeline = nullptr;
    }
}

/* 0x80A140 */
void _rwIm3DRenderPipeClose(RxPipeline** pipelines) {
    ZoneScoped;
    ((void(__cdecl*)(void))0x80DFB0)(); /* _rwD3D8Im3DRenderPipeCloseDevice (driver side, unreversed) */
    RwIm3DSetRenderPipeline(nullptr, rwPRIMTYPETRIFAN);
    RwIm3DSetRenderPipeline(nullptr, rwPRIMTYPEPOLYLINE);
    RwIm3DSetRenderPipeline(nullptr, rwPRIMTYPELINELIST);
    RwIm3DSetRenderPipeline(nullptr, rwPRIMTYPETRILIST);
    RwIm3DSetRenderPipeline(nullptr, rwPRIMTYPETRISTRIP);
    if (pipelines[0] != nullptr) {
        _rxPipelineDestroy(pipelines[0]);
    }
    pipelines[0] = pipelines[1] = pipelines[2] = pipelines[3] = pipelines[4] = nullptr;
}

/* 0x80A1A0 */
RwBool _rwIm3DRenderPipeOpen(RxPipeline** pipelines) {
    ZoneScoped;
    if (auto pipe = RxPipelineCreate()) {
        pipe->pluginId = 1;
        if (auto locked = RxPipelineLock(pipe)) {
            locked = RxLockedPipeAddFragment(locked, nullptr, _rwIm3DGetRenderNodeDefinition());
            if (auto unlocked = RxLockedPipeUnlock(locked)) {
                pipelines[0] = pipelines[1] = pipelines[2] = pipelines[3] = pipelines[4] = unlocked;
                RwIm3DSetRenderPipeline(unlocked, rwPRIMTYPETRIFAN);
                RwIm3DSetRenderPipeline(unlocked, rwPRIMTYPEPOLYLINE);
                RwIm3DSetRenderPipeline(unlocked, rwPRIMTYPELINELIST);
                RwIm3DSetRenderPipeline(unlocked, rwPRIMTYPETRILIST);
                RwIm3DSetRenderPipeline(unlocked, rwPRIMTYPETRISTRIP);
                return ((RwBool(__cdecl*)(void))0x80E020)(); /* _rwD3D8Im3DRenderPipeOpenDevice (driver side, unreversed) */
            }
        }
        _rxPipelineDestroy(pipe);
    }
    return FALSE;
}
