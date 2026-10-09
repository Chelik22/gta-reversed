#include "StdInc.h"

#include "rpworld.h"
#include <rpdbgerr.h>

// ==== RW-CAMPAIGN wave-D canonical decls ====
// GTA SA RpWorldSector = tagged union (BSP node / leaf), 0x88 байт (rpworld-4/5/11).
struct RpWorldSectorGta
{
    RwInt32      type;                       // +0x00 (-1 leaf; 0..0x14 BSP axis)
    union {
        RpTriangle*     triangles;           // +0x04 (leaf)
        RwReal          value;               // +0x04 (node: plane position)
    };
    union {
        RwV3d*          vertices;            // +0x08 (leaf)
        void*           leftSubTree;         // +0x08 (node)
    };
    union {
        RpVertexNormal* normals;             // +0x0C (leaf)
        void*           rightSubTree;        // +0x0C (node)
    };
    void*        field_10[8];                // +0x10..0x2F
    void*        field_30;                   // +0x30
    void*        repEntry;                   // +0x34
    RwLinkList   collAtomicsInWorldSector;   // +0x38
    RwLinkList   lightsInWorldSector;        // +0x40
    RwBBox       boundingBox;                // +0x48
    RwBBox       tightBoundingBox;           // +0x60
    RpMeshHeader* mesh;                      // +0x78
    void*        pipeline;                   // +0x7C
    RwUInt16     field_80;                   // +0x80
    RwUInt16     numVertices;                // +0x82
    RwUInt16     numTriangles;               // +0x84
    RwUInt16     field_86;                   // +0x86
};
static_assert(sizeof(RpWorldSectorGta) == 0x88, "RpWorldSectorGta layout");
static_assert(offsetof(RpWorldSectorGta, repEntry)                 == 0x34, "RpWorldSectorGta layout");
static_assert(offsetof(RpWorldSectorGta, collAtomicsInWorldSector) == 0x38, "RpWorldSectorGta layout");
static_assert(offsetof(RpWorldSectorGta, lightsInWorldSector)      == 0x40, "RpWorldSectorGta layout");
static_assert(offsetof(RpWorldSectorGta, tightBoundingBox)         == 0x60, "RpWorldSectorGta layout");
static_assert(offsetof(RpWorldSectorGta, mesh)                     == 0x78, "RpWorldSectorGta layout");
static_assert(offsetof(RpWorldSectorGta, numTriangles)             == 0x84, "RpWorldSectorGta layout");

struct RpWorldCameraChunkGta
{
    RpWorldSector** frustumSectors;    /* +0x00 */
    RwUInt32        pad04;             /* +0x04 */
    RwInt32         numFrustumSectors; /* +0x08 */
    RpWorld*        world;             /* +0x0C */
};

struct RxD3D9AllInOneCallBacksSA
{
    RxD3D9AllInOneInstanceCallBack   instanceCB;   // +0x00
    RxD3D9AllInOneReinstanceCallBack reinstanceCB; // +0x04
    RxD3D9AllInOneLightingCallBack   lightingCB;   // +0x08
    RxD3D9AllInOneRenderCallBack     renderCB;     // +0x0C
};

// world-module offsets (rpworld-6)
static auto& gRpWorldGlobalsOffset  = StaticRef<RwInt32>(0xC9259C);
static auto& gRpWorldCameraChunkOff = StaticRef<RwInt32>(0xC925A4);
static auto& gRpWorldAtomicChunkOff = StaticRef<RwInt32>(0xC925A8);
static auto& gRpWorldClumpChunkOff  = StaticRef<RwInt32>(0xC925AC);
static auto& gRpWorldLightChunkOff  = StaticRef<RwInt32>(0xC925B0);
static inline auto& rwD3D9FindFormatCapsBits = StaticRef<RwUInt32>(0xC9BFEC);

// light-module globals 0xC925E0 (rpworld-8); D3DLIGHT9 stride 0x68 (D3DCOLORVALUE alpha)
typedef D3DLIGHT9 RwD3D9RpWorldLightTemplateSA;
struct RwD3D9RpWorldGlobalsSA
{
    RwD3D9RpWorldLightTemplateSA localLightTemplate;       // 0xC925E0
    RwD3D9RpWorldLightTemplateSA directionalLightTemplate; // 0xC92648
    RwInt32   lightPluginOffset;            // 0xC926B0
    RwInt32   maxLights;                    // 0xC926B4
    RwInt32   freeIndexCount;               // 0xC926B8
    RwInt32   field_BC;                     // 0xC926BC
    RwInt32   nextLightIndex;               // 0xC926C0
    RwInt32*  freeIndices;                  // 0xC926C4
    RwInt32*  curIndices;                   // 0xC926C8
    RwInt32   curCount;                     // 0xC926CC
    RwInt32   curCapacity;                  // 0xC926D0
    RwInt32*  prevIndices;                  // 0xC926D4
    RwInt32   prevCount;                    // 0xC926D8
    RwInt32   prevCapacity;                 // 0xC926DC
    RwInt32   field_100;                    // 0xC92600 abs (gap; unnamed)
    RwInt32   field_E4;                     // 0xC926E4
    RwInt32   geometryUsageChunkOffset;     // 0xC926E8
    RwInt32   worldSectorUsageChunkOffset;  // 0xC926EC
};
static_assert(offsetof(RwD3D9RpWorldGlobalsSA, directionalLightTemplate)    == 0x68,  "RwD3D9RpWorldGlobalsSA layout");
static_assert(offsetof(RwD3D9RpWorldGlobalsSA, lightPluginOffset)           == 0xD0,  "RwD3D9RpWorldGlobalsSA layout");
static_assert(offsetof(RwD3D9RpWorldGlobalsSA, curIndices)                  == 0xE8,  "RwD3D9RpWorldGlobalsSA layout");
static_assert(offsetof(RwD3D9RpWorldGlobalsSA, prevIndices)                 == 0xF4,  "RwD3D9RpWorldGlobalsSA layout");
static_assert(offsetof(RwD3D9RpWorldGlobalsSA, geometryUsageChunkOffset)    == 0x108, "RwD3D9RpWorldGlobalsSA layout");
static_assert(offsetof(RwD3D9RpWorldGlobalsSA, worldSectorUsageChunkOffset) == 0x10C, "RwD3D9RpWorldGlobalsSA layout");
static_assert(sizeof(RwD3D9RpWorldGlobalsSA) == 0x110, "RwD3D9RpWorldGlobalsSA layout");
static inline auto& rwD3D9RpWorldGlobals = StaticRef<RwD3D9RpWorldGlobalsSA>(0xC925E0);

// render-state module globals referenced by rpworld fragments (rwcore-7 layout)
static RwInt32&  rwD3D9ChangedRenderStateCount = StaticRef<RwInt32>(0xC980F4);
static RwInt32*& rwD3D9ChangedRenderStates     = StaticRef<RwInt32*>(0xC980F8);
static RwBool&   rwD3D9LightingEnabledState    = StaticRef<RwBool>(0xC9A5F4);
static RwBool&   rwD3D9LightingForceResend     = StaticRef<RwBool>(0xC9A5F8);

// ==== RW-CAMPAIGN rpworld-9 SHARED BLOCK — ИНТЕГРАТОРУ: вставить ОДИН раз в rpworld.cpp ====
// GTA SA RwGlobals, хвост с функциями памяти (офсеты сверены декомпилами; зеркало RwGlobalsSA из rwplcore.cpp):
//   +0x134 rwmalloc(size,hint)  +0x138 rwfree(ptr)  +0x13C rwrealloc(ptr,newSize,hint)
//   +0x140 rwcalloc  +0x144 memoryAlloc(RwFreeList*,hint)  +0x148 memoryFree(RwFreeList*,ptr)
struct RwGlobalsMemFuncsGta
{
    RwUInt8     pad[0x134];                                                // 0x000..0x133
    void*       (*rwmalloc)(size_t size, RwUInt32 hint);                   // +0x134
    void        (*rwfree)(void* mem);                                      // +0x138
    void*       (*rwrealloc)(void* mem, size_t newSize, RwUInt32 hint);    // +0x13C
    void*       (*rwcalloc)(size_t numObj, size_t sizeObj, RwUInt32 hint); // +0x140
    void*       (*memoryAlloc)(RwFreeList* fl, RwUInt32 hint);             // +0x144 (impl по умолчанию 0x801C30)
    RwFreeList* (*memoryFree)(RwFreeList* fl, void* pData);                // +0x148
};

// глобалы rpmesh-модуля (декомпилы 0x758970/0x758940/0x758EC0/0x75D530)
static RwFreeList*& rpMeshFreeListGta      = StaticRef<RwFreeList*>(0xC926F0); // DAT_00c926f0 (freelist модуля)
static RwInt32&     rpMeshGlobalsOffsetGta = StaticRef<RwInt32>(0xC9B8C0);     // DAT_00c9b8c0 (globalsOffset)
static RwInt32&     rpMeshOpenCountGta     = StaticRef<RwInt32>(0xC9B8C4);     // DAT_00c9b8c4 (refcount)

// глобалы tristrip-метода (0x75D4C0/0x75D500; используются и в _rpMeshOptimise 0x75D970 — батч rpworld-10!)
static RpTriStripMeshCallBack& rpMeshTriStripMethodGta     = StaticRef<RpTriStripMeshCallBack>(0x8D6388); // PTR_FUN_008d6388
static void*&                  rpMeshTriStripMethodDataGta = StaticRef<void*>(0x8D638C);                  // DAT_008d638c
// NOTE: 0x8D6390 = RwUInt32 счётчик testFrame туннельного стриппера; читается/пишется только
// внутренним хелпером 0x75BC80 (вне этажа, вызывается raw-кастом) — здесь не объявлен.
//
// ВНУТРЕННИЕ ХЕЛПЕРЫ tristrip-модуля ВНЕ ЭТАЖА (нет в rw_addr_map.json) — вызываются raw-кастами:
//   RwBool           __cdecl CheckStripWinding(RwUInt32* work)                                    @0x75BB80
//   void             __cdecl FlipStripEdges(RwUInt32* work)                                       @0x75BB20
//   RwInt32          __cdecl CountStripEdges(RpTriStripPolygon* poly)                             @0x75BB50
//   RwUInt32         __cdecl TunnelStripCost(RwUInt32* work, RpTriStripTunnelData* td)            @0x75BC80
//   RpTriStripEdge*  __cdecl FollowStrip(RpTriStripEdge*, RpTriStripPolygon*, RwUInt16, RwUInt32) @0x75C290
//   RwUInt32*        __cdecl BuildTriStrip(RpBuildMeshTriangle**, RwUInt32)                       @0x75C660
//   void             __cdecl FillMeshFromStrip(RwUInt32* strip, RpMesh* mesh, RpBuildMesh* bm)    @0x75C9D0
//   int              __cdecl CompareTriByMaterial(const void*, const void*)                       @0x759640
//   void             __cdecl crt_qsort(void*, RwUInt32 n, RwUInt32 sz, cmp)                       @0x8247E0 (msvcrt)
//   RwInt32          __cdecl RwErrorCodeMake(RwInt32 code, ...)                                   @0x8088D0 (identity 1-го арг.)
// ==== КОНЕЦ SHARED BLOCK ====

// ==== rpworld-11 COMMON BLOCK (D3D9 vertex-shader state) — вставить ОДИН раз в rpworld.cpp ====


// ==== RW-CAMPAIGN wave-D shared decls (consolidated) ====
// ==== rpworld-5 COMMON BLOCK — ВСТАВИТЬ ОДИН РАЗ в rpworld.cpp (например выше _rpWorldFindBBox) ====
// Все фрагменты батча rpworld-5 ссылаются на эти объявления. Защищено #ifndef, повторная вставка безопасна.
// Раскладки получены из декомпилов функций этого батча (0x74ED50/0x74EFA0/0x74F020/0x74F1A0/0x74F210/
// 0x74F4E0/0x74F610/0x74F760); SDK-раскладки repo rpworld.h/rwplcore.h бинару НЕ соответствуют.
#ifndef RPWORLD5_COMMON_BLOCK
#define RPWORLD5_COMMON_BLOCK

// Память RwGlobals (rwplcore.h SDK-раскладка бинару не соответствует; волна-B это уже подтвердила):
// memoryFuncs @+0x134 (rwmalloc/rwfree/rwrealloc/rwcalloc), memoryAlloc @+0x144, memoryFree @+0x148.
struct RwGlobalsMemGta
{
    RwUInt8  pad[0x134];
    void* (*rwmalloc)(size_t size, RwUInt32 hint);        // +0x134
    void  (*rwfree)(void* mem);                           // +0x138
    void* (*rwrealloc)(void* mem, size_t newSize, RwUInt32 hint); // +0x13C
    void* (*rwcalloc)(size_t numObj, size_t sizeObj, RwUInt32 hint); // +0x140
    void* (*memoryAlloc)(RwFreeList* fl, RwUInt32 hint);  // +0x144 (freelist-alloc, default 0x801C30)
    void  (*memoryFree)(RwFreeList* fl, void* pData);     // +0x148 (freelist-free)
};
static inline RwGlobalsMemGta* RwGlobalsMem() { return reinterpret_cast<RwGlobalsMemGta*>(RwEngineInstance); }

// rpworld module globals внутри RwEngineInstance. Значение *(0xC9254C) = globalsOffset модуля
// (присваивается при RpWorldPluginAttach). engine+off+0x00 = RwFreeList* узлов списка миров,
// engine+off+0x04 = RwLinkList сентинел списка зарегистрированных миров (_rpWorldRegisterWorld/GetWorld/Destroy).
static RwInt32& rpWorldGlobalsOffset = StaticRef<RwInt32>(0xC9254C);
// globalsOffset другого модуля (0xC9BC60): engine+off+0x40 = RxPipeline* дефолтный пайплайн
// сектора (fallback _rpSectorDefaultRenderCallBack, когда ни сектор, ни мир пайплайна не имеют).
static RwInt32& worldSectorPipelineGlobalsOffset = StaticRef<RwInt32>(0xC9BC60);

// Реестры плагинов (имена из plugin-sdk plugin_sa/game_sa/rw/rpworld.h:3507 `extern RwPluginRegistry worldTKList;`
// и его же worldSectorTKList; в репо ещё не объявлены). 0x8D62D0 = RpWorld, 0x8D736C = RpWorldSector.
static inline auto& worldTKList       = StaticRef<RwPluginRegistry>(0x8D62D0);
static inline auto& worldSectorTKList = StaticRef<RwPluginRegistry>(0x8D736C);

// Узел глобального списка миров (layout из _rpWorldRegisterWorld 0x74F0C0 / RpWorldSectorGetWorld 0x74F4E0):
// node+0x00 world, +0x04 memorySize, +0x08 RwLLLink link (узел вписывается звеном в список).
struct RwWorldListNodeGta
{
    RpWorld*  world;        // 0x00
    RwUInt32  memorySize;   // 0x04 (размер аллокации мира; для single-malloc миров - весь блок с секторами)
    RwLLLink  list;         // 0x08
};

// GTA SA RpMaterialList = 12 байт: ПОРЯДОК ПОЛЕЙ ИНВЕРТИРОВАН относительно repo rpworld.h
// (там {numMaterials; materials}), плюс третье поле, всегда обнуляемое (_rpMaterialListInitialize 0x74E1B0 /
// Deinitialize 0x74E150 трогают ровно 3 dword). Подтверждено использованием в RpWorldUnlock/ForAllMaterials.
struct RpMaterialListGta
{
    RpMaterial** materials;     // 0x00
    RwInt32      numMaterials;  // 0x04
    RwUInt32     field_08;      // 0x08 (обнуляется, семантика неизвестна)
};

// GTA SA RpWorld, 0x70 байт (из RpWorldCreate 0x74F760). Отличия от repo rpworld.h:
// matList = RpMaterialListGta (12 байт), из-за чего всё от rootSector смещено на +4:
// rootSector@0x1C, lightList@0x34, directionalLightList@0x3C, boundingBox@0x50, renderCallBack@0x68, pipeline@0x6C.
struct RpWorldGta
{
    RwObject        object;               // 0x00 (privateFlags bit0 = rpWORLDSINGLEMALLOC)
    RwUInt32        flags;                // 0x08 (bit0 = rpWORLDTRISTRIP, читает RpWorldUnlock)
    RpWorldRenderOrder renderOrder;       // 0x0C (Create пишет 2)
    RpMaterialListGta matList;            // 0x10
    RpSector*       rootSector;           // 0x1C
    RwInt32         numTexCoordSets;      // 0x20
    RwInt32         numClumpsInWorld;     // 0x24
    RwLLLink*       currentClumpLink;     // 0x28
    RwLinkList      clumpList;            // 0x2C (сентинел; RpWorldForAllClumps: clump = link-0x08!)
    RwLinkList      lightList;            // 0x34 (RpWorldForAllLights: light = link-0x34 = RpLight::inWorld)
    RwLinkList      directionalLightList; // 0x3C (_rpWorldForAllGlobalLights тоже идёт по этому списку)
    RwV3d           worldOrigin;          // 0x44
    RwBBox          boundingBox;          // 0x50
    RpWorldSectorCallBackRender renderCallBack; // 0x68
    RxPipeline*     pipeline;             // 0x6C
};
static_assert(sizeof(RpWorldGta) == 0x70, "RpWorldGta layout");
static_assert(offsetof(RpWorldGta, rootSector)  == 0x1C, "RpWorldGta layout");
static_assert(offsetof(RpWorldGta, lightList)   == 0x34, "RpWorldGta layout");
static_assert(offsetof(RpWorldGta, boundingBox) == 0x50, "RpWorldGta layout");
static_assert(offsetof(RpWorldGta, renderCallBack) == 0x68, "RpWorldGta layout");

// Узел BSP-дерева мира (плоскость; layout совпадает с repo rpworld.h RpPlaneSector):
// type == -2: пустой корень (уничтожается как есть); type == -1: лист RpWorldSector;
// type >= 0: плоскость; type используется КАК БАЙТОВОЕ СМЕЩЕНИЕ (0/4/8/0xC/0x10/0x14) в 6-float bbox
// (_rpWorldSetupSectorBoundingBoxes 0x74F020). leftSubTree/rightSubTree @0x08/0x0C.
struct RpSectorBSP
{
    RwInt32      type;         // 0x00
    RwReal       value;        // 0x04
    RpSectorBSP* leftSubTree;  // 0x08
    RpSectorBSP* rightSubTree; // 0x0C
    RwReal       leftValue;    // 0x10
    RwReal       rightValue;   // 0x14
};

// GTA SA RpWorldSector = tagged union (BSP node / leaf), 0x88 байт. Комбинированная
// раскладка из декомпилов батчей rpworld-4/5/11; type@0: -1 = лист, 0..0x14 = ось плоскости.
struct RpWorldSectorGta
{
    RwInt32      type;                       // +0x00
    union {
        RpTriangle*     triangles;           // +0x04 (leaf)
        RwReal          value;               // +0x04 (node: позиция плоскости)
    };
    union {
        RwV3d*          vertices;            // +0x08 (leaf)
        void*           leftSubTree;         // +0x08 (node)
    };
    union {
        RpVertexNormal* normals;             // +0x0C (leaf)
        void*           rightSubTree;        // +0x0C (node)
    };
    void*        field_10[8];                // +0x10..0x2F (9 вендорских слотов; Create обнуляет, Destroy фрит по отдельности)
    void*        field_30;                   // +0x30
    void*        repEntry;                   // +0x34 (RwResEntry*)
    RwLinkList   collAtomicsInWorldSector;   // +0x38
    RwLinkList   lightsInWorldSector;        // +0x40
    RwBBox       boundingBox;                // +0x48
    RwBBox       tightBoundingBox;           // +0x60
    RpMeshHeader* mesh;                      // +0x78
    void*        pipeline;                   // +0x7C
    RwUInt16     field_80;                   // +0x80
    RwUInt16     numVertices;                // +0x82
    RwUInt16     numTriangles;               // +0x84
    RwUInt16     field_86;                   // +0x86
};
static_assert(sizeof(RpWorldSectorGta) == 0x88, "RpWorldSectorGta layout");
static_assert(offsetof(RpWorldSectorGta, repEntry)                 == 0x34, "RpWorldSectorGta layout");
static_assert(offsetof(RpWorldSectorGta, collAtomicsInWorldSector) == 0x38, "RpWorldSectorGta layout");
static_assert(offsetof(RpWorldSectorGta, lightsInWorldSector)      == 0x40, "RpWorldSectorGta layout");
static_assert(offsetof(RpWorldSectorGta, tightBoundingBox)         == 0x60, "RpWorldSectorGta layout");
static_assert(offsetof(RpWorldSectorGta, mesh)                     == 0x78, "RpWorldSectorGta layout");
static_assert(offsetof(RpWorldSectorGta, numTriangles)             == 0x84, "RpWorldSectorGta layout");

// privateData D3D9 all-in-one нода (RxPipelineNode.privateData @+0x14): 4 колбэка подряд
struct RxD3D9AllInOneCallBacksSA
{
    RxD3D9AllInOneInstanceCallBack   instanceCB;   // +0x00
    RxD3D9AllInOneReinstanceCallBack reinstanceCB; // +0x04
    RxD3D9AllInOneLightingCallBack   lightingCB;   // +0x08
    RxD3D9AllInOneRenderCallBack     renderCB;     // +0x0C
};

// world-module offsets (rpworld-6): block/chunk offsets set by init 0x750050
// privateData D3D9 all-in-one нода (RxPipelineNode.privateData @+0x14): 4 колбэка подряд
struct RxD3D9AllInOneCallBacksSA
{
    RxD3D9AllInOneInstanceCallBack   instanceCB;   // +0x00
    RxD3D9AllInOneReinstanceCallBack reinstanceCB; // +0x04
    RxD3D9AllInOneLightingCallBack   lightingCB;   // +0x08
    RxD3D9AllInOneRenderCallBack     renderCB;     // +0x0C
};

// world-module offsets (rpworld-6): block/chunk offsets set by init 0x750050
// privateData D3D9 all-in-one нода (RxPipelineNode.privateData @+0x14): 4 колбэка подряд
struct RxD3D9AllInOneCallBacksSA
{
    RxD3D9AllInOneInstanceCallBack   instanceCB;   // +0x00
    RxD3D9AllInOneReinstanceCallBack reinstanceCB; // +0x04
    RxD3D9AllInOneLightingCallBack   lightingCB;   // +0x08
    RxD3D9AllInOneRenderCallBack     renderCB;     // +0x0C
};

// world-module offsets (rpworld-6): block/chunk offsets set by init 0x750050
// privateData D3D9 all-in-one нода (RxPipelineNode.privateData @+0x14): 4 колбэка подряд

// world-module offsets (rpworld-6): block/chunk offsets set by init 0x750050
// privateData D3D9 all-in-one нода (RxPipelineNode.privateData @+0x14): 4 колбэка подряд

// world-module offsets (rpworld-6): block/chunk offsets set by init 0x750050
// Сырые хелперы вне вендорского этажа (нет в rw_addr_map.json) - оставить кастами:
//   void      __cdecl sub_74FFF0(RwLLLink*) - teardown звена collAtomicsInWorldSector
//   void      __cdecl sub_74FFA0(RwLLLink*) - teardown узла lightsInWorldSector (освобождает {link,light})
//   RpWorldSector* __cdecl sub_74F590(RpWorldSector*, void*) - статический сектор-коллбек для RwCameraForAllSectorsInFrustum
//   RwInt32   __cdecl _rwerror(RwInt32 code, ...) @ 0x8088D0 (волна-B rwcore.cpp тоже кастует)

#endif // RPWORLD5_COMMON_BLOCK


// ==== rpworld-11 COMMON BLOCK (D3D9 vertex-shader state) — вставить ОДИН раз в rpworld.cpp ====
#ifndef RPWORLD11_D3D9VS_COMMON_BLOCK
#define RPWORLD11_D3D9VS_COMMON_BLOCK

// Стейт VS-модуля d3d9-драйвера (оффсеты из декомпилов 0x764650..0x764F60):
static RwMatrix&  g_rwD3D9VSActiveWorldCopy = StaticRef<RwMatrix>(0x8D7390);   // 4x3-копия активной мировой (right/up/at/pos)
static RwMatrix&  g_rwD3D9VSInverseMatrix   = StaticRef<RwMatrix>(0x8D73D0);   // буфер обратной мировой
static RwMatrix&  g_rwD3D9VSInvOrthoMatrix  = StaticRef<RwMatrix>(0x8D7410);   // буфер inverse+orthonormalize (для нормалей)
static RwMatrix&  g_rwD3D9VSNormalWorld     = StaticRef<RwMatrix>(0x8D7450);   // буфер нормализованной мировой
static RwMatrix&  g_rwD3D9VSComposedMatrix  = StaticRef<RwMatrix>(0xC94C30);   // буфер composed view*proj
static RwMatrix&  g_rwD3D9VSIdentityMatrix  = StaticRef<RwMatrix>(0x8753D0);   // константа identity-матрицы (rdata)
static RwMatrix*& g_rwD3D9VSActiveWorld    = StaticRef<RwMatrix*>(0xC94C70);   // активная мировая матрица (указатель извне)
static RwMatrix*& g_rwD3D9VSInvCache       = StaticRef<RwMatrix*>(0xC94C74);   // кэш обратной матрицы (0x8D73D0 | identity)
static RwMatrix*& g_rwD3D9VSInvOrthoCache  = StaticRef<RwMatrix*>(0xC94C78);   // кэш inverse-orthonormal (0x8D7410 | identity)
static RwMatrix*& g_rwD3D9VSNormCache      = StaticRef<RwMatrix*>(0xC94C7C);   // кэш нормализованной мировой (0x8D7450)
static RwMatrix*& g_rwD3D9VSComposedCache  = StaticRef<RwMatrix*>(0xC97C64);   // кэш composed (NULL, пока не собран)
static RwMatrix&  g_rwD3D9VSViewMatrix     = StaticRef<RwMatrix>(0xC9BC80);    // текущая видовая матрица (RW-тип, 64 байта; RenderWare.h знает её как GetD3DViewTransform)
static RwMatrix&  g_rwD3D9VSProjMatrix     = StaticRef<RwMatrix>(0x8E2458);    // матрица проекции (GetD3DProjTransform)

#endif // RPWORLD11_D3D9VS_COMMON_BLOCK


void _rwD3D9VSSetActiveWorldMatrix(const RwMatrix* worldMatrix) {
    g_rwD3D9VSActiveWorld = const_cast<RwMatrix*>(worldMatrix);
    if (worldMatrix != nullptr) {
        g_rwD3D9VSActiveWorldCopy.right = worldMatrix->right;
        g_rwD3D9VSActiveWorldCopy.up    = worldMatrix->up;
        g_rwD3D9VSActiveWorldCopy.at    = worldMatrix->at;
        g_rwD3D9VSActiveWorldCopy.pos   = worldMatrix->pos;
    }
    g_rwD3D9VSInvCache      = nullptr;      // DAT_00c94c74
    g_rwD3D9VSInvOrthoCache = nullptr;      // DAT_00c94c78
    g_rwD3D9VSNormCache     = nullptr;      // DAT_00c94c7c
}


void _rwD3D9VSGetComposedTransformMatrix(void* transformMatrix) {
    if (g_rwD3D9VSComposedCache == nullptr) {
        ((void(__cdecl*)(void*, const void*, const void*))0x7672E1)(&g_rwD3D9VSComposedMatrix,
                                                                    &g_rwD3D9VSViewMatrix,
                                                                    &g_rwD3D9VSProjMatrix);
        g_rwD3D9VSComposedCache = &g_rwD3D9VSComposedMatrix;
    }
    if (g_rwD3D9VSActiveWorld != nullptr) {
        ((void(__cdecl*)(void*, const RwMatrix*))0x7676C7)(transformMatrix, &g_rwD3D9VSActiveWorldCopy);
        return;
    }
    ((void(__cdecl*)(void*, const void*))0x7675EE)(transformMatrix, g_rwD3D9VSComposedCache);
}


void _rwD3D9VSGetWorldViewTransposedMatrix(void* worldViewMatrix) {
    if (g_rwD3D9VSActiveWorld != nullptr) {
        ((void(__cdecl*)(void*, const RwMatrix*))0x7676C7)(worldViewMatrix, &g_rwD3D9VSActiveWorldCopy);
        return;
    }
    ((void(__cdecl*)(void*, const void*))0x7675EE)(worldViewMatrix, &g_rwD3D9VSViewMatrix);
}


void _rwD3D9VSGetWorldViewMatrix(void* worldViewMatrix) {
    RwMatrix* dst = static_cast<RwMatrix*>(worldViewMatrix);
    if (g_rwD3D9VSActiveWorld != nullptr) {
        RwMatrixMultiply(dst, g_rwD3D9VSActiveWorld, &g_rwD3D9VSViewMatrix);
        dst->flags = 0;          // dword[3]
        dst->pad1  = 0;          // dword[7]
        dst->pad2  = 0;          // dword[0xB]
        dst->pad3  = 0x3F800000; // dword[0xF] = 1.0f
        return;
    }
    memcpy(dst, &g_rwD3D9VSViewMatrix, 0x40);
}


void _rwD3D9VSGetInverseWorldMatrix(void* inverseWorldMatrix) {
    RwMatrix* dst = static_cast<RwMatrix*>(inverseWorldMatrix);
    if (g_rwD3D9VSInvCache == nullptr) {
        if (g_rwD3D9VSActiveWorld != nullptr &&
            (g_rwD3D9VSActiveWorld->flags & rwMATRIXINTERNALIDENTITY /*0x20000*/) != rwMATRIXINTERNALIDENTITY) {
            if ((g_rwD3D9VSActiveWorld->flags & rwMATRIXTYPEMASK /*3*/) == rwMATRIXTYPEORTHONORMAL /*3*/) {
                ((void(__cdecl*)(RwMatrix*, const RwMatrix*))0x764890)(&g_rwD3D9VSInverseMatrix,
                                                                      g_rwD3D9VSActiveWorld);
                g_rwD3D9VSInvOrthoCache = &g_rwD3D9VSInverseMatrix;   // DAT_00c94c78
                g_rwD3D9VSInvCache      = &g_rwD3D9VSInverseMatrix;   // DAT_00c94c74
                memcpy(dst, &g_rwD3D9VSInverseMatrix, 0x40);
                return;
            }
            RwMatrixInvert(&g_rwD3D9VSInverseMatrix, g_rwD3D9VSActiveWorld);
            g_rwD3D9VSInverseMatrix.flags = 0;          // 0x8D73DC
            g_rwD3D9VSInverseMatrix.pad1  = 0;          // 0x8D73EC
            g_rwD3D9VSInverseMatrix.pad2  = 0;          // 0x8D73FC
            g_rwD3D9VSInverseMatrix.pad3  = 0x3F800000; // 0x8D740C = 1.0f
            g_rwD3D9VSInvCache = &g_rwD3D9VSInverseMatrix;
            memcpy(dst, &g_rwD3D9VSInverseMatrix, 0x40);
            return;
        }
        g_rwD3D9VSInvOrthoCache = &g_rwD3D9VSIdentityMatrix;   // DAT_00c94c78
        g_rwD3D9VSInvCache      = &g_rwD3D9VSIdentityMatrix;   // DAT_00c94c74
    }
    memcpy(dst, g_rwD3D9VSInvCache, 0x40);
}


void _rwD3D9VSGetWorldMultiplyMatrix(RwMatrix* worldMultiplyMatrix, const RwMatrix* matrix) {
    if (g_rwD3D9VSActiveWorld != nullptr) {
        RwMatrixMultiply(worldMultiplyMatrix, g_rwD3D9VSActiveWorld, matrix);
        return;
    }
    memcpy(worldMultiplyMatrix, matrix, 0x40);
}


void _rwD3D9VSGetWorldMultiplyTransposeMatrix(void* worldMultiplyMatrix, const RwMatrix* matrix) {
    RwMatrix tmp;
    const void* src = matrix;
    if (g_rwD3D9VSActiveWorld != nullptr &&
        (g_rwD3D9VSActiveWorld->flags & rwMATRIXINTERNALIDENTITY /*0x20000*/) != rwMATRIXINTERNALIDENTITY) {
        RwMatrixMultiply(&tmp, g_rwD3D9VSActiveWorld, matrix);
        src = &tmp;
    }
    ((void(__cdecl*)(void*, const void*))0x7675EE)(worldMultiplyMatrix, src);
    RwUInt32* dst = static_cast<RwUInt32*>(worldMultiplyMatrix);
    dst[0xC] = 0;
    dst[0xD] = 0;
    dst[0xE] = 0;
    dst[0xF] = 0x3F800000;   // 1.0f
}


void _rwD3D9VSGetWorldViewMultiplyTransposeMatrix(void* worldViewMultiplyMatrix, const RwMatrix* matrix) {
    RwMatrix tmpA;
    RwMatrix tmpB;
    if (g_rwD3D9VSActiveWorld == nullptr ||
        (g_rwD3D9VSActiveWorld->flags & rwMATRIXINTERNALIDENTITY /*0x20000*/) == rwMATRIXINTERNALIDENTITY) {
        RwMatrixMultiply(&tmpA, &g_rwD3D9VSViewMatrix, matrix);
    } else {
        RwMatrixMultiply(&tmpB, g_rwD3D9VSActiveWorld, &g_rwD3D9VSViewMatrix);
        RwMatrixMultiply(&tmpA, &tmpB, matrix);
    }
    tmpA.flags = 0;          // dword[3]
    tmpA.pad1  = 0;          // dword[7]
    tmpA.pad2  = 0;          // dword[0xB]
    tmpA.pad3  = 0x3F800000; // dword[0xF] = 1.0f
    ((void(__cdecl*)(void*, const void*))0x7675EE)(worldViewMultiplyMatrix, &tmpA);
}


void _rwD3D9VSGetWorldNormalizedMultiplyTransposeMatrix(void* worldMultiplyMatrix, const RwMatrix* matrix) {
    RwMatrix tmp;
    const void* src = matrix;
    if (g_rwD3D9VSActiveWorld != nullptr &&
        (g_rwD3D9VSActiveWorld->flags & rwMATRIXINTERNALIDENTITY /*0x20000*/) != rwMATRIXINTERNALIDENTITY) {
        if ((g_rwD3D9VSActiveWorld->flags & rwMATRIXTYPEMASK /*3*/) == rwMATRIXTYPEORTHONORMAL /*3*/) {
            RwMatrixMultiply(&tmp, g_rwD3D9VSActiveWorld, matrix);
            src = &tmp;
        } else {
            if (g_rwD3D9VSNormCache == nullptr) {
                RwMatrixOrthoNormalize(&g_rwD3D9VSNormalWorld, g_rwD3D9VSActiveWorld);
                g_rwD3D9VSNormalWorld.flags = 0;          // 0x8D745C
                g_rwD3D9VSNormalWorld.pad1  = 0;          // 0x8D746C
                g_rwD3D9VSNormalWorld.pad2  = 0;          // 0x8D747C
                g_rwD3D9VSNormalWorld.pad3  = 0x3F800000; // 0x8D748C = 1.0f
                g_rwD3D9VSNormCache = &g_rwD3D9VSNormalWorld;
            }
            RwMatrixMultiply(&tmp, g_rwD3D9VSNormCache, matrix);
            src = &tmp;
        }
    }
    ((void(__cdecl*)(void*, const void*))0x7675EE)(worldMultiplyMatrix, src);
    RwUInt32* dst = static_cast<RwUInt32*>(worldMultiplyMatrix);
    dst[0xC] = 0;
    dst[0xD] = 0;
    dst[0xE] = 0;
    dst[0xF] = 0x3F800000;   // 1.0f
}


void _rwD3D9VSGetWorldNormalizedViewMultiplyTransposeMatrix(void* worldViewMultiplyMatrix, const RwMatrix* matrix) {
    RwMatrix tmpA;
    RwMatrix tmpB;
    if (g_rwD3D9VSActiveWorld == nullptr ||
        (g_rwD3D9VSActiveWorld->flags & rwMATRIXINTERNALIDENTITY /*0x20000*/) == rwMATRIXINTERNALIDENTITY) {
        RwMatrixMultiply(&tmpA, &g_rwD3D9VSViewMatrix, matrix);
    } else {
        const RwMatrix* worldSrc = g_rwD3D9VSActiveWorld;
        if ((g_rwD3D9VSActiveWorld->flags & rwMATRIXTYPEMASK /*3*/) != rwMATRIXTYPEORTHONORMAL /*3*/) {
            if (g_rwD3D9VSNormCache == nullptr) {
                RwMatrixOrthoNormalize(&g_rwD3D9VSNormalWorld, g_rwD3D9VSActiveWorld);
                g_rwD3D9VSNormalWorld.flags = 0;          // 0x8D745C
                g_rwD3D9VSNormalWorld.pad1  = 0;          // 0x8D746C
                g_rwD3D9VSNormalWorld.pad2  = 0;          // 0x8D747C
                g_rwD3D9VSNormalWorld.pad3  = 0x3F800000; // 0x8D748C = 1.0f
                g_rwD3D9VSNormCache = &g_rwD3D9VSNormalWorld;
            }
            worldSrc = g_rwD3D9VSNormCache;
        }
        RwMatrixMultiply(&tmpB, worldSrc, &g_rwD3D9VSViewMatrix);
        RwMatrixMultiply(&tmpA, &tmpB, matrix);
    }
    tmpA.flags = 0;          // dword[3]
    tmpA.pad1  = 0;          // dword[7]
    tmpA.pad2  = 0;          // dword[0xB]
    tmpA.pad3  = 0x3F800000; // dword[0xF] = 1.0f
    ((void(__cdecl*)(void*, const void*))0x7675EE)(worldViewMultiplyMatrix, &tmpA);
}


void _rwD3D9VSGetWorldNormalizedTransposeMatrix(void* worldMatrix) {
    RwUInt32* dst = static_cast<RwUInt32*>(worldMatrix);
    if (g_rwD3D9VSActiveWorld != nullptr &&
        (g_rwD3D9VSActiveWorld->flags & rwMATRIXINTERNALIDENTITY /*0x20000*/) != rwMATRIXINTERNALIDENTITY) {
        if ((g_rwD3D9VSActiveWorld->flags & rwMATRIXTYPEMASK /*3*/) == rwMATRIXTYPEORTHONORMAL /*3*/) {
            ((void(__cdecl*)(void*, const void*))0x7675EE)(dst, &g_rwD3D9VSActiveWorldCopy);
            return;
        }
        if (g_rwD3D9VSNormCache == nullptr) {
            RwMatrixOrthoNormalize(&g_rwD3D9VSNormalWorld, g_rwD3D9VSActiveWorld);
            g_rwD3D9VSNormalWorld.flags = 0;          // 0x8D745C
            g_rwD3D9VSNormalWorld.pad1  = 0;          // 0x8D746C
            g_rwD3D9VSNormalWorld.pad2  = 0;          // 0x8D747C
            g_rwD3D9VSNormalWorld.pad3  = 0x3F800000; // 0x8D748C = 1.0f
            g_rwD3D9VSNormCache = &g_rwD3D9VSNormalWorld;
        }
        ((void(__cdecl*)(void*, const void*))0x7675EE)(dst, g_rwD3D9VSNormCache);
        return;
    }
    memcpy(dst, &g_rwD3D9VSIdentityMatrix, 0x40);
    dst[3]  = 0;
    dst[7]  = 0;
    dst[0xB] = 0;
    dst[0xF] = 0x3F800000;   // 1.0f
}


void _rwD3D9VSGetProjectionTransposedMatrix(void* projectionMatrix) {
    ((void(__cdecl*)(void*, const void*))0x7675EE)(projectionMatrix, &g_rwD3D9VSProjMatrix);
}


void _rwD3D9VSGetNormalInLocalSpace(const RwV3d* normalWorldSpace, RwV3d* normalLocalSpace) {
    RwMatrix* invOrtho = g_rwD3D9VSInvOrthoCache;
    if (invOrtho == nullptr) {
        if (g_rwD3D9VSActiveWorld == nullptr ||
            (g_rwD3D9VSActiveWorld->flags & rwMATRIXINTERNALIDENTITY /*0x20000*/) == rwMATRIXINTERNALIDENTITY) {
            g_rwD3D9VSInvCache      = &g_rwD3D9VSIdentityMatrix;
            g_rwD3D9VSInvOrthoCache = &g_rwD3D9VSIdentityMatrix;
        } else if ((g_rwD3D9VSActiveWorld->flags & rwMATRIXTYPEMASK /*3*/) != rwMATRIXTYPEORTHONORMAL /*3*/) {
            if (g_rwD3D9VSInvCache == nullptr) {
                RwMatrixInvert(&g_rwD3D9VSInverseMatrix, g_rwD3D9VSActiveWorld);
                g_rwD3D9VSInverseMatrix.flags = 0;          // 0x8D73DC
                g_rwD3D9VSInverseMatrix.pad1  = 0;          // 0x8D73EC
                g_rwD3D9VSInverseMatrix.pad2  = 0;          // 0x8D73FC
                g_rwD3D9VSInverseMatrix.pad3  = 0x3F800000; // 0x8D740C = 1.0f
                g_rwD3D9VSInvCache = &g_rwD3D9VSInverseMatrix;
            }
            RwMatrixOrthoNormalize(&g_rwD3D9VSInvOrthoMatrix, g_rwD3D9VSInvCache);
            g_rwD3D9VSInvOrthoMatrix.flags = 0;          // 0x8D741C
            g_rwD3D9VSInvOrthoMatrix.pad1  = 0;          // 0x8D742C
            g_rwD3D9VSInvOrthoMatrix.pad2  = 0;          // 0x8D743C
            g_rwD3D9VSInvOrthoMatrix.pad3  = 0x3F800000; // 0x8D744C = 1.0f
            g_rwD3D9VSInvOrthoCache = &g_rwD3D9VSInvOrthoMatrix;
            ((void(__cdecl*)(RwV3d*, const RwV3d*, const RwMatrix*))0x766AB7)(
                normalLocalSpace, normalWorldSpace, &g_rwD3D9VSInvOrthoMatrix);
            return;
        } else {
            invOrtho = g_rwD3D9VSInvCache;
            if (g_rwD3D9VSInvCache == nullptr) {
                ((void(__cdecl*)(RwMatrix*, const RwMatrix*))0x764890)(&g_rwD3D9VSInverseMatrix,
                                                                      g_rwD3D9VSActiveWorld);
                g_rwD3D9VSInvCache = &g_rwD3D9VSInverseMatrix;
                invOrtho = g_rwD3D9VSInvCache;
            }
            g_rwD3D9VSInvOrthoCache = invOrtho;
        }
    }
    if (g_rwD3D9VSInvOrthoCache != &g_rwD3D9VSIdentityMatrix) {
        ((void(__cdecl*)(RwV3d*, const RwV3d*, const RwMatrix*))0x766AB7)(
            normalLocalSpace, normalWorldSpace, g_rwD3D9VSInvOrthoCache);
        return;
    }
    // LAB_00764e4c: identity — нормаль не меняется
    normalLocalSpace->x = normalWorldSpace->x;
    normalLocalSpace->y = normalWorldSpace->y;
    normalLocalSpace->z = normalWorldSpace->z;
}


void _rwD3D9VSGetPointInLocalSpace(const RwV3d* pointWorldSpace, RwV3d* pointLocalSpace) {
    if (g_rwD3D9VSInvCache == nullptr) {
        if (g_rwD3D9VSActiveWorld != nullptr &&
            (g_rwD3D9VSActiveWorld->flags & rwMATRIXINTERNALIDENTITY /*0x20000*/) != rwMATRIXINTERNALIDENTITY) {
            if ((g_rwD3D9VSActiveWorld->flags & rwMATRIXTYPEMASK /*3*/) == rwMATRIXTYPEORTHONORMAL /*3*/) {
                ((void(__cdecl*)(RwMatrix*, const RwMatrix*))0x764890)(&g_rwD3D9VSInverseMatrix,
                                                                      g_rwD3D9VSActiveWorld);
                g_rwD3D9VSInvCache = &g_rwD3D9VSInverseMatrix;
            } else {
                RwMatrixInvert(&g_rwD3D9VSInverseMatrix, g_rwD3D9VSActiveWorld);
                g_rwD3D9VSInverseMatrix.flags = 0;          // 0x8D73DC
                g_rwD3D9VSInverseMatrix.pad1  = 0;          // 0x8D73EC
                g_rwD3D9VSInverseMatrix.pad2  = 0;          // 0x8D73FC
                g_rwD3D9VSInverseMatrix.pad3  = 0x3F800000; // 0x8D740C = 1.0f
                g_rwD3D9VSInvCache = &g_rwD3D9VSInverseMatrix;
            }
            ((void(__cdecl*)(RwV3d*, const RwV3d*, const RwMatrix*))0x766A32)(
                pointLocalSpace, pointWorldSpace, &g_rwD3D9VSInverseMatrix);
            return;
        }
        g_rwD3D9VSInvCache = &g_rwD3D9VSIdentityMatrix;
    } else if (g_rwD3D9VSInvCache != &g_rwD3D9VSIdentityMatrix) {
        ((void(__cdecl*)(RwV3d*, const RwV3d*, const RwMatrix*))0x766A32)(
            pointLocalSpace, pointWorldSpace, g_rwD3D9VSInvCache);
        return;
    }
    pointLocalSpace->x = pointWorldSpace->x;
    pointLocalSpace->y = pointWorldSpace->y;
    pointLocalSpace->z = pointWorldSpace->z;
}


void _rwD3D9VSGetRadiusInLocalSpace(RwReal radiusWorldSpace, RwReal* radiusLocalSpace) {
    if (g_rwD3D9VSActiveWorld != nullptr &&
        (g_rwD3D9VSActiveWorld->flags & rwMATRIXTYPEMASK /*3*/) != rwMATRIXTYPEORTHONORMAL /*3*/) {
        if (g_rwD3D9VSInvCache == nullptr) {
            RwMatrixInvert(&g_rwD3D9VSInverseMatrix, g_rwD3D9VSActiveWorld);
            g_rwD3D9VSInverseMatrix.flags = 0;          // 0x8D73DC
            g_rwD3D9VSInverseMatrix.pad1  = 0;          // 0x8D73EC
            g_rwD3D9VSInverseMatrix.pad2  = 0;          // 0x8D73FC
            g_rwD3D9VSInverseMatrix.pad3  = 0x3F800000; // 0x8D740C = 1.0f
            g_rwD3D9VSInvCache = &g_rwD3D9VSInverseMatrix;
        }
        RwV3d diag;
        diag.x = 0.57735026f;
        diag.y = 0.57735026f;
        diag.z = 0.57735026f;
        ((void(__cdecl*)(RwV3d*, const RwV3d*, const RwMatrix*))0x766AB7)(&diag, &diag, g_rwD3D9VSInvCache);
        const RwReal lengthSq = diag.z * diag.z + diag.x * diag.x + diag.y * diag.y;
        *radiusLocalSpace = _rwInvSqrt(lengthSq) * radiusWorldSpace;
        return;
    }
    *radiusLocalSpace = radiusWorldSpace;
}


void _rpD3D9VertexShaderCachePurge() {
    struct VSCacheEntryGta {            // 0x14 bytes
        RwUInt32 key;                   // descriptor nibble bytes as u32
        RwUInt32 renderFrame;           // RwGlobals->renderFrame stamp (eviction metric)
        void*    vertexShader;          // IDirect3DVertexShader9*
        RwUInt32 dispatchData[2];       // cached _rpD3D9VertexShaderDispatchDescriptor (8 bytes)
    };
    struct VSCacheGta {
        VSCacheEntryGta entries[256];
    };
    static auto&        numCached = StaticRef<RwInt32>(0xC94BF8);
    static auto&        cache     = StaticRef<VSCacheGta>(0xC926F8);
    static auto&        cursor    = StaticRef<VSCacheEntryGta*>(0xC94BFC);

    for (RwInt32 i = 0; i < numCached; i++) {
        VSCacheEntryGta* entry = &cache.entries[i];
        if (entry->vertexShader != nullptr) {
            RwD3D9DeleteVertexShader(entry->vertexShader);
            entry->vertexShader = nullptr;
        }
    }
    numCached = 0;

    cursor = &cache.entries[0];
    cache.entries[0].key           = 0xFFFFFFFF;
    cache.entries[0].renderFrame   = 0xFFFFFFFF;
    cache.entries[0].vertexShader  = reinterpret_cast<void*>(0xFFFFFFFF);
    cache.entries[0].dispatchData[0] = 0xFFFFFFFF;
    cache.entries[0].dispatchData[1] = 0xFFFFFFFF;
}


void* _rpD3D9GetVertexShader(const _rpD3D9VertexShaderDescriptor* desc, _rpD3D9VertexShaderDispatchDescriptor* dispatch) {
    struct VSCacheEntryGta {            // 0x14 bytes
        RwUInt32 key;
        RwUInt32 renderFrame;
        void*    vertexShader;
        RwUInt32 dispatchData[2];
    };
    struct VSCacheGta {
        VSCacheEntryGta entries[256];
    };
    static auto&        numCached = StaticRef<RwInt32>(0xC94BF8);
    static auto&        cache     = StaticRef<VSCacheGta>(0xC926F8);
    static auto&        cursor    = StaticRef<VSCacheEntryGta*>(0xC94BFC);

    RwGlobals* globals = RwEngineInstance;
    const RwUInt32 key = *reinterpret_cast<const RwUInt32*>(desc);

    if (cursor->key != key) {
        // binary search (lower bound)
        RwInt32 low = 0;
        RwInt32 high = numCached;
        RwInt32 found = -1;
        while (low < high) {
            const RwInt32 mid = low + (high - low) / 2;
            if (cache.entries[mid].key == key) {
                found = mid;
                break;
            }
            if (cache.entries[mid].key < key)
                low = mid + 1;
            else
                high = mid;
        }

        if (found < 0) {
            // miss: compile a new shader (also fills *dispatch)
            const RwUInt32 shader = ((RwUInt32(__cdecl*)(const _rpD3D9VertexShaderDescriptor*,
                                                          _rpD3D9VertexShaderDispatchDescriptor*))0x75F0B0)(desc, dispatch);
            if (numCached < 0x100) {
                // insert at the sorted position, shifting the tail right
                const RwInt32 pos = low;
                if (pos < numCached) {
                    for (RwInt32 i = numCached; i > pos; i--)
                        cache.entries[i] = cache.entries[i - 1];
                }
                cursor                 = &cache.entries[pos];
                cursor->key            = *reinterpret_cast<const RwUInt32*>(desc);
                cursor->renderFrame    = globals->renderFrame;
                cursor->vertexShader   = reinterpret_cast<void*>(shader);
                cursor->dispatchData[0] = reinterpret_cast<const RwUInt32*>(dispatch)[0];
                cursor->dispatchData[1] = reinterpret_cast<const RwUInt32*>(dispatch)[1];
                numCached++;
                return reinterpret_cast<void*>(shader);
            }
            // cache full: replace the entry with the smallest renderFrame stamp
            VSCacheEntryGta* victim = &cache.entries[0];
            for (RwInt32 i = 1; i < numCached; i++) {
                if (cache.entries[i].renderFrame < victim->renderFrame)
                    victim = &cache.entries[i];
            }
            victim->key            = *reinterpret_cast<const RwUInt32*>(desc);
            victim->renderFrame    = globals->renderFrame;
            if (victim->vertexShader != nullptr)
                RwD3D9DeleteVertexShader(victim->vertexShader);
            victim->vertexShader   = reinterpret_cast<void*>(shader);
            victim->dispatchData[0] = reinterpret_cast<const RwUInt32*>(dispatch)[0];
            victim->dispatchData[1] = reinterpret_cast<const RwUInt32*>(dispatch)[1];
            cursor = victim;
            return reinterpret_cast<void*>(shader);
        }

        cursor = &cache.entries[found];
    }

    cursor->renderFrame      = globals->renderFrame;
    reinterpret_cast<RwUInt32*>(dispatch)[0] = cursor->dispatchData[0];
    reinterpret_cast<RwUInt32*>(dispatch)[1] = cursor->dispatchData[1];
    return cursor->vertexShader;
}


RwUInt32 _rpD3D9GetNumConstantsUsed(const _rpD3D9VertexShaderDescriptor* desc) {
    struct EffectTableGta {
        RwUInt8 constants[64];      // indexed by desc->effect (6 bits)
    };
    static auto& effectTable = StaticRef<EffectTableGta>(0xC94AF8);

    RwUInt32 numConstants = 5;                                      // 4 matrix regs + ambient
    if (desc->numDirectionalLights != 0)
        numConstants = desc->numDirectionalLights * 2 + 5;          // {dir,0} + {color,0} per light
    if (desc->numPointLights != 0)
        numConstants += ((desc->numPointLights + 3) >> 2) + desc->numPointLights * 2;   // radius regs + {pos,color}
    if (desc->numSpotLights != 0)
        numConstants += ((desc->numSpotLights + 3) >> 2) + desc->numSpotLights * 4;     // radius regs + 4 regs/light
    if (desc->modulateMaterial != 0)
        numConstants += 1;
    if (desc->fogMode != 0)
        numConstants += 1;
    if (desc->effect != 0)
        numConstants += effectTable.constants[desc->effect];
    if (desc->morphing != 0)
        numConstants += 1;
    return numConstants;
}


void _rxD3D9VertexShaderDefaultBeginCallBack(void* object, RwUInt32 type, _rpD3D9VertexShaderDescriptor* desc) {
    *reinterpret_cast<RwUInt32*>(desc) = 0;     // clear all descriptor bits

    RwUInt32 flags;
    RwBool   modulateNotSet;
    if (type != 1 /*rpATOMIC*/) {
        RpWorld* world = reinterpret_cast<RpWorld*>(RwEngineInstance->curWorld);
        flags = world->flags;
        if (static_cast<RwInt8>(flags) < 0)                 // rpGEOMETRYTEXTURED2 (0x80)
            desc->numTexCoords = 2;
        else if ((flags & rpGEOMETRYTEXTURED /*0x4*/) != 0)
            desc->numTexCoords = 1;
        if ((flags & rpGEOMETRYPRELIT /*0x8*/) != 0)
            desc->prelit = 1;
        if ((flags & rpGEOMETRYNORMALS /*0x10*/) != 0)
            desc->normals = 1;
        modulateNotSet = (flags & rpGEOMETRYMODULATEMATERIALCOLOR /*0x40*/) == 0;
    } else {
        RpAtomic*  atomic  = static_cast<RpAtomic*>(object);
        RpGeometry* geometry = atomic->geometry;
        flags = geometry->flags;
        if (geometry->numMorphTargets != 1
            && atomic->interpolator.startMorphTarget != atomic->interpolator.endMorphTarget
            && atomic->interpolator.position * atomic->interpolator.recipTime != 0.0f /*DAT_00858B50 == 0.0f, exe-verified*/)
            desc->morphing = 1;
        desc->numTexCoords = static_cast<RwUInt8>(geometry->numTexCoordSets);
        if ((flags & rpGEOMETRYPRELIT /*0x8*/) != 0)
            desc->prelit = 1;
        if ((flags & rpGEOMETRYNORMALS /*0x10*/) != 0) {
            desc->normals = 1;
            const RwUInt32 usage = RpD3D9GeometryGetUsageFlags(geometry);
            if ((usage & 0xFFF /*rpD3D9GEOMETRYUSAGE_DYNAMICMASK*/) != 0) {
                desc->normalizeNormals = 1;     // dynamic geometry: never assume unit-length normals
            } else {
                const RwMatrix* ltm = RwFrameGetLTM(reinterpret_cast<RwFrame*>(atomic->object.object.parent));
                if ((ltm->flags & 0x20001) == 0) {  // 0x20000 = rwMATRIXINTERNALIDENTITY, bit0 = matrix type
                    RwReal len2 = ltm->right.x * ltm->right.x + ltm->right.y * ltm->right.y + ltm->right.z * ltm->right.z;
                    RwBool orthonormal = (len2 < 1.1f && 0.9f < len2);
                    if (orthonormal) {
                        len2 = ltm->up.x * ltm->up.x + ltm->up.y * ltm->up.y + ltm->up.z * ltm->up.z;
                        orthonormal = (len2 < 1.1f && 0.9f < len2);
                    }
                    if (orthonormal) {
                        len2 = ltm->at.x * ltm->at.x + ltm->at.y * ltm->at.y + ltm->at.z * ltm->at.z;
                        orthonormal = (len2 < 1.1f && 0.9f < len2);
                    }
                    if (!orthonormal)
                        desc->normalizeNormals = 1;
                }
            }
        }
        modulateNotSet = (flags & rpGEOMETRYMODULATEMATERIALCOLOR /*0x40*/) == 0;
    }

    if (!modulateNotSet)
        desc->modulateMaterial = 1;

    RwUInt32 fogEnabled;
    RwD3D9GetRenderState(D3DRS_FOGENABLE /*0x1C*/, &fogEnabled);
    if (fogEnabled != 0) {
        RwUInt32 fogType;
        RwEngineInstance->dOpenDevice.fpRenderStateGet(rwRENDERSTATEFOGTYPE /*0x10*/, &fogType);
        if (fogType == rwFOGTYPELINEAR)             // 1
            desc->fogMode = 1;
        else if (fogType == rwFOGTYPEEXPONENTIAL) { // 2
            desc->fogMode = 2;
            return;
        } else if (fogType == rwFOGTYPEEXPONENTIAL2) { // 3
            desc->fogMode = 3;
            return;
        }
    }
}


RwV4d* _rxD3D9VertexShaderDefaultLightingCallBack(void* object, RwUInt32 type, RwV4d* shaderConstantPtr,
                                                  _rpD3D9VertexShaderDescriptor* desc) {
    desc->numDirectionalLights = 0;
    desc->numSpotLights = 0;    // keeps the numTexCoords nibble

    RwUInt32 flags;
    if (type == 1 /*rpATOMIC*/)
        flags = static_cast<RpAtomic*>(object)->geometry->flags;
    else
        flags = reinterpret_cast<RpWorld*>(RwEngineInstance->curWorld)->flags;

    RwReal* constants = reinterpret_cast<RwReal*>(shaderConstantPtr);
    constants[0] = 0.0f;
    constants[1] = 0.0f;
    constants[2] = 0.0f;
    constants[3] = ((flags & rpGEOMETRYPRELIT /*0x8*/) != 0) ? 0.0f : 1.0f;

    RpWorld* world;
    if ((flags & rpGEOMETRYLIGHT /*0x20*/) == 0
        || (world = reinterpret_cast<RpWorld*>(RwEngineInstance->curWorld)) == nullptr)
        return shaderConstantPtr + 1;

    RpLight* dirLights[15];
    RpLight* pointLights[15];
    RpLight* spotLights[15];
    RwUInt32 numDir = 0, numPoint = 0, numSpot = 0;
    RwBool   anyLight = FALSE;

    if (type == 1 /*rpATOMIC*/) {
        RpAtomic* atomic = static_cast<RpAtomic*>(object);
        const RwUInt32 hasNormals = (flags >> 4) & 1;   // rpGEOMETRYNORMALS

        // world directional/ambient lights flagged for atomics
        for (RwLLLink* link = world->directionalLightList.link.next;
             link != &world->directionalLightList.link;
             link = link->next) {
            RpLight* light = rwLLLinkGetData(link, RpLight, inWorld);
            if (light != nullptr && (light->object.object.flags & rpLIGHTLIGHTATOMICS /*0x1*/) != 0) {
                if (light->object.object.subType == rpLIGHTDIRECTIONAL /*1*/) {
                    if (hasNormals != 0 && numDir < 15) {
                        dirLights[numDir++] = light;
                        anyLight = TRUE;
                    }
                } else if (light->object.object.subType == rpLIGHTAMBIENT /*2*/) {
                    constants[0] += light->color.red;
                    constants[1] += light->color.green;
                    constants[2] += light->color.blue;
                }
            }
        }

        if (hasNormals != 0) {
            // positioned lights: per-atomic, from the sectors the atomic lies in
            RwEngineInstance->lightFrame++;
            const RwUInt16 stamp = RwEngineInstance->lightFrame;
            const RwSphere* atomSphere = RpAtomicGetWorldBoundingSphere(atomic);
            for (RwLLLink* tieLink = atomic->llWorldSectorsInAtomic.link.next;
                 tieLink != &atomic->llWorldSectorsInAtomic.link;
                 tieLink = tieLink->next) {
                RpWorldSector* sector = rwLLLinkGetData(tieLink, RpTie, lWorldSectorInAtomic)->worldSector;
                for (RwLLLink* link = sector->lightsInWorldSector.link.next;
                     link != &sector->lightsInWorldSector.link;
                     link = link->next) {
                    RpLight* light = rwLLLinkGetData(link, RpLightTie, lightInWorldSector)->light;
                    if (light != nullptr
                        && (light->object.object.flags & rpLIGHTLIGHTATOMICS /*0x1*/) != 0
                        && light->lightFrame != stamp) {
                        light->lightFrame = stamp;
                        const RwMatrix* lightLTM = RwFrameGetLTM(reinterpret_cast<RwFrame*>(light->object.object.parent));
                        const RwReal dx = atomSphere->center.x - lightLTM->pos.x;
                        const RwReal dy = atomSphere->center.y - lightLTM->pos.y;
                        const RwReal dz = atomSphere->center.z - lightLTM->pos.z;
                        const RwReal range = light->radius + atomSphere->radius;
                        if (dx * dx + dy * dy + dz * dz < range * range) {
                            const RwUInt8 lightType = light->object.object.subType;
                            if (lightType == rpLIGHTPOINT /*0x80*/) {
                                if (numPoint < 15) {
                                    pointLights[numPoint++] = light;
                                    anyLight = TRUE;
                                }
                            } else if (lightType > rpLIGHTPOINT && lightType < rpLIGHTSPOTSOFT + 1 /*0x81..0x82*/) {
                                if (numSpot < 15) {
                                    spotLights[numSpot++] = light;
                                    anyLight = TRUE;
                                }
                            }
                        }
                    }
                }
            }
        }
    } else {
        // world sector: lights flagged for static geometry
        for (RwLLLink* link = world->directionalLightList.link.next;
             link != &world->directionalLightList.link;
             link = link->next) {
            RpLight* light = rwLLLinkGetData(link, RpLight, inWorld);
            if (light != nullptr && (light->object.object.flags & rpLIGHTLIGHTWORLD /*0x2*/) != 0) {
                if (light->object.object.subType == rpLIGHTDIRECTIONAL /*1*/) {
                    if (numDir < 15) {
                        dirLights[numDir++] = light;
                        anyLight = TRUE;
                    }
                } else if (light->object.object.subType == rpLIGHTAMBIENT /*2*/) {
                    constants[0] += light->color.red;
                    constants[1] += light->color.green;
                    constants[2] += light->color.blue;
                }
            }
        }

        RwEngineInstance->lightFrame++;
        const RwUInt16 stamp = RwEngineInstance->lightFrame;
        RpWorldSector* sector = static_cast<RpWorldSector*>(object);
        for (RwLLLink* link = sector->lightsInWorldSector.link.next;
             link != &sector->lightsInWorldSector.link;
             link = link->next) {
            RpLight* light = rwLLLinkGetData(link, RpLightTie, lightInWorldSector)->light;
            if (light != nullptr
                && (light->object.object.flags & rpLIGHTLIGHTWORLD /*0x2*/) != 0
                && light->lightFrame != stamp) {
                light->lightFrame = stamp;
                const RwUInt8 lightType = light->object.object.subType;
                if (lightType == rpLIGHTPOINT /*0x80*/) {
                    if (numPoint < 15) {
                        pointLights[numPoint++] = light;
                        anyLight = TRUE;
                    }
                } else if (lightType > rpLIGHTPOINT && lightType < rpLIGHTSPOTSOFT + 1 /*0x81..0x82*/) {
                    if (numSpot < 15) {
                        spotLights[numSpot++] = light;
                        anyLight = TRUE;
                    }
                }
            }
        }
    }

    constants += 4;     // past the ambient constant

    if (anyLight) {
        if (numDir != 0) {
            for (RwUInt32 i = 0; i < numDir; i++) {
                const RpLight*  light = dirLights[i];
                const RwMatrix* ltm = RwFrameGetLTM(reinterpret_cast<RwFrame*>(light->object.object.parent));
                _rwD3D9VSGetNormalInLocalSpace(&ltm->at, reinterpret_cast<RwV3d*>(constants));
                constants[3] = 0.0f;
                constants[4] = light->color.red;
                constants[5] = light->color.green;
                constants[6] = light->color.blue;
                constants[7] = 0.0f;
                constants += 8;
            }
            desc->numDirectionalLights = static_cast<RwUInt8>(numDir);
        }
        if (numPoint != 0) {
            RwReal* radiusSlot = constants;     // trailing write cursor, 4 radii per float4
            for (RwUInt32 i = 0; i < numPoint; i++) {
                const RpLight*  light = pointLights[i];
                const RwMatrix* ltm = RwFrameGetLTM(reinterpret_cast<RwFrame*>(light->object.object.parent));
                RwReal* block = constants;
                RwReal* radiusDst = radiusSlot;
                if ((i & 3) == 0) {
                    block = constants + 4;
                    radiusDst = constants;
                }
                _rwD3D9VSGetPointInLocalSpace(&ltm->pos, reinterpret_cast<RwV3d*>(block));
                block[3] = 0.0f;
                block[4] = light->color.red;
                block[5] = light->color.green;
                block[6] = light->color.blue;
                block[7] = 0.0f;
                constants = block + 8;
                _rwD3D9VSGetRadiusInLocalSpace(light->radius, radiusDst);
                radiusSlot = radiusDst + 1;
                *radiusDst = 1.0f / *radiusDst;     // DAT_00858624 == 1.0f (exe-verified)
            }
            desc->numPointLights = static_cast<RwUInt8>(numPoint);
        }
        if (numSpot != 0) {
            RwReal* radiusSlot = constants;
            for (RwUInt32 i = 0; i < numSpot; i++) {
                RpLight* light = spotLights[i];
                const RwMatrix* ltm = RwFrameGetLTM(reinterpret_cast<RwFrame*>(light->object.object.parent));
                RwReal* block = constants;
                RwReal* radiusDst = radiusSlot;
                if ((i & 3) == 0) {
                    block = constants + 4;
                    radiusDst = constants;
                }
                _rwD3D9VSGetPointInLocalSpace(&ltm->pos, reinterpret_cast<RwV3d*>(block));
                block[3] = 0.0f;
                _rwD3D9VSGetNormalInLocalSpace(&ltm->at, reinterpret_cast<RwV3d*>(block + 4));
                block[7] = 0.0f;
                const RwReal cosAngle = cosf(RpLightGetConeAngle(light));
                if (light->object.object.subType == static_cast<RwUInt8>(-0x7F) /*rpLIGHTSPOT (0x81)*/
                    || 0.999f /*DAT_00863E2C, exe-verified*/ <= cosAngle) {
                    block[8]  = 999.99994f;         // DAT_00875290 (0x4479FFFF), exe-verified
                    block[9]  = -cosAngle * 999.99994f;
                } else {
                    const RwReal falloff = 1.0f / (1.0f - cosAngle);
                    block[8]  = falloff;
                    block[9]  = -cosAngle * falloff;
                }
                block[10] = 0.5f;
                block[11] = 0.0f;
                block[12] = light->color.red;
                block[13] = light->color.green;
                block[14] = light->color.blue;
                block[15] = 0.0f;
                constants = block + 16;
                _rwD3D9VSGetRadiusInLocalSpace(light->radius, radiusDst);
                radiusSlot = radiusDst + 1;
                *radiusDst = 1.0f / *radiusDst;
            }
            desc->numSpotLights = static_cast<RwUInt8>(numSpot);
            return reinterpret_cast<RwV4d*>(constants);
        }
    }
    return reinterpret_cast<RwV4d*>(constants);
}


void* _rxD3D9VertexShaderDefaultGetMaterialShaderCallBack(const RpMaterial* material,
                                                          _rpD3D9VertexShaderDescriptor* desc,
                                                          _rpD3D9VertexShaderDispatchDescriptor* dispatch) {
    (void)material;
    return _rpD3D9GetVertexShader(desc, dispatch);
}


void _rxD3D9VertexShaderDefaultMeshRenderCallBack(RxD3D9ResEntryHeader* resEntryHeader,
                                                  RxD3D9InstanceData* instancedMesh,
                                                  const _rpD3D9VertexShaderDescriptor* desc,
                                                  const _rpD3D9VertexShaderDispatchDescriptor* dispatch) {
    (void)desc;
    (void)dispatch;

    if (_rwD3D9LastPixelShaderUsed != nullptr) {
        _rwD3D9LastPixelShaderUsed = nullptr;
        _RwD3DDevice->SetPixelShader(nullptr);
    }

    if (instancedMesh->material->texture == nullptr) {
        // untextured: take color/alpha from the diffuse argument
        RwD3D9SetTexture(nullptr, 0);
        RwD3D9SetTextureStageState(0, 1 /*D3DTSS_COLOROP*/, 3 /*D3DTOP_SELECTARG2*/);
        RwD3D9SetTextureStageState(0, 3 /*D3DTSS_COLORARG2*/, 0 /*D3DTA_DIFFUSE*/);
        RwD3D9SetTextureStageState(0, 4 /*D3DTSS_ALPHAOP*/, 3 /*D3DTOP_SELECTARG2*/);
        RwD3D9SetTextureStageState(0, 6 /*D3DTSS_ALPHAARG2*/, 0 /*D3DTA_DIFFUSE*/);
    } else {
        RwD3D9SetTexture(instancedMesh->material->texture, 0);
        RwD3D9SetTextureStageState(0, 1 /*D3DTSS_COLOROP*/, 4 /*D3DTOP_MODULATE*/);
        RwD3D9SetTextureStageState(0, 2 /*D3DTSS_COLORARG1*/, 2 /*D3DTA_TEXTURE*/);
        RwD3D9SetTextureStageState(0, 3 /*D3DTSS_COLORARG2*/, 0 /*D3DTA_DIFFUSE*/);
        RwD3D9SetTextureStageState(0, 4 /*D3DTSS_ALPHAOP*/, 4 /*D3DTOP_MODULATE*/);
        RwD3D9SetTextureStageState(0, 5 /*D3DTSS_ALPHAARG1*/, 2 /*D3DTA_TEXTURE*/);
        RwD3D9SetTextureStageState(0, 6 /*D3DTSS_ALPHAARG2*/, 0 /*D3DTA_DIFFUSE*/);
    }

    if (_rwD3D9LastVertexShaderUsed != instancedMesh->vertexShader) {
        _rwD3D9LastVertexShaderUsed = instancedMesh->vertexShader;
        _RwD3DDevice->SetVertexShader(instancedMesh->vertexShader);
    }

    _rwD3D9RenderStateFlushCache();

    if (resEntryHeader->indexBuffer != nullptr) {
        _RwD3DDevice->DrawIndexedPrimitive(resEntryHeader->primType,
                                           instancedMesh->baseIndex,     // BaseVertexIndex
                                           0,                            // MinIndex
                                           instancedMesh->numVertices,
                                           instancedMesh->startIndex,
                                           instancedMesh->numPrimitives);
    } else {
        _RwD3DDevice->DrawPrimitive(resEntryHeader->primType,
                                    instancedMesh->baseIndex,            // StartVertex
                                    instancedMesh->numPrimitives);
    }
}


void _rxD3D9VertexShaderDefaultEndCallBack(void* object, RwUInt32 type, _rpD3D9VertexShaderDescriptor* desc) {
    (void)object;
    (void)type;
    (void)desc;
}


void _rpD3D9VertexShaderUpdateLightsColors(RwV4d* shaderConstantPtr, const _rpD3D9VertexShaderDescriptor* desc,
                                           RwReal ambientCoef, RwReal diffuseCoef) {
    RwReal* constants = reinterpret_cast<RwReal*>(shaderConstantPtr);
    constants[0] *= ambientCoef;
    constants[1] *= ambientCoef;
    constants[2] *= ambientCoef;

    RwReal* cursor = constants + 4;
    for (RwUInt32 i = 0; i < desc->numDirectionalLights; i++) {     // color reg = block + 4
        cursor[4] *= diffuseCoef;
        cursor[5] *= diffuseCoef;
        cursor[6] *= diffuseCoef;
        cursor += 8;
    }
    for (RwUInt32 i = 0; i < desc->numPointLights; i++) {           // 4-point groups carry a radius float4 first
        RwReal* block = cursor;
        if ((i & 3) == 0)
            block = cursor + 4;
        cursor = block + 8;
        block[4] *= diffuseCoef;
        block[5] *= diffuseCoef;
        block[6] *= diffuseCoef;
    }
    for (RwUInt32 i = 0; i < desc->numSpotLights; i++) {            // 4-spot groups carry a radius float4 first
        RwReal* block = cursor;
        if ((i & 3) == 0)
            block = cursor + 4;
        cursor = block + 16;
        block[12] *= diffuseCoef;
        block[13] *= diffuseCoef;
        block[14] *= diffuseCoef;
    }

    _RwD3DDevice->SetVertexShaderConstantF(4, reinterpret_cast<const float*>(constants),
                                           static_cast<RwUInt32>(reinterpret_cast<RwUInt8*>(cursor)
                                                                 - reinterpret_cast<RwUInt8*>(constants)) >> 4);
}


void _rpD3D9VertexShaderUpdateMaterialColor(const RwRGBA* color, const _rpD3D9VertexShaderDispatchDescriptor* dispatch) {
    const RwReal byteToFloat = 0.003921569f;    // _DAT_00859A3C = 0x3B808081 = 1/255 (exe-verified)
    const float colorF[4] = {
        static_cast<float>(color->red)   * byteToFloat,
        static_cast<float>(color->green) * byteToFloat,
        static_cast<float>(color->blue)  * byteToFloat,
        static_cast<float>(color->alpha) * byteToFloat,
    };
    _RwD3DDevice->SetVertexShaderConstantF(dispatch->offsetMaterialColor, colorF, 1);
}


void _rpD3D9VertexShaderUpdateFogData(const _rpD3D9VertexShaderDescriptor* desc,
                                      const _rpD3D9VertexShaderDispatchDescriptor* dispatch) {
    float data[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

    switch (desc->fogMode) {
    case rwFOGTYPELINEAR /*1*/:
    {
        const RwCamera* camera = reinterpret_cast<const RwCamera*>(RwEngineInstance->curCamera);
        const RwReal fogPlane  = camera->fogPlane;      // +0x88 in the binary
        const RwReal scale     = 1.0f / (camera->farPlane - fogPlane);  // +0x84 in the binary
        data[0] = scale;
        data[1] = -(scale * fogPlane);
        break;
    }
    case rwFOGTYPEEXPONENTIAL:   /*2*/
    case rwFOGTYPEEXPONENTIAL2:  /*3*/
    {
        float density;
        RwD3D9GetRenderState(D3DRS_FOGDENSITY /*0x26*/, &density);
        data[0] = density * 1.442695f;                  // _DAT_00875294 = 0x3FB8AA3B = log2(e) (exe-verified)
        data[1] = 0.0f;
        break;
    }
    default:
        break;
    }

    _RwD3DDevice->SetVertexShaderConstantF(dispatch->offsetFogRange, data, 1);
}


void _rpD3D9VertexShaderUpdateMorphingCoef(RwReal morphingCoef, const _rpD3D9VertexShaderDispatchDescriptor* dispatch) {
    const float data[4] = {
        morphingCoef,
        1.0f - morphingCoef,    // _DAT_00858624 == 1.0f (exe-verified)
        1.0f,                   // 0x3F800000
        0.0f,
    };
    _RwD3DDevice->SetVertexShaderConstantF(dispatch->offsetMorphingCoef, data, 1);
}


void _rpD3DVertexShaderSetUVAnimMatrix(RwMatrix* matrix, const _rpD3D9VertexShaderDispatchDescriptor* dispatch) {
    const RwReal* src = reinterpret_cast<const RwReal*>(matrix);
    const float   transposed[8] = {
        static_cast<float>(src[0]),  static_cast<float>(src[4]),  static_cast<float>(src[8]),  static_cast<float>(src[12]),
        static_cast<float>(src[1]),  static_cast<float>(src[5]),  static_cast<float>(src[9]),  static_cast<float>(src[13]),
    };
    _RwD3DDevice->SetVertexShaderConstantF(dispatch->offsetEffect, transposed, 2);
}


void _rpD3D9VertexShaderSetEnvMatrix(RwFrame* frame, const _rpD3D9VertexShaderDescriptor* desc,
                                     const _rpD3D9VertexShaderDispatchDescriptor* dispatch) {
    struct EnvScaleMatrixGta {
        RwMatrix matrix;
    };
    static const EnvScaleMatrixGta envScale = [] {
        EnvScaleMatrixGta m;    // binary .rdata @0x875250
        m.matrix.right.x = 0.5f;  m.matrix.right.y = 0.0f;   m.matrix.right.z = 0.0f;
        m.matrix.up.x    = 0.0f;  m.matrix.up.y    = -0.5f;  m.matrix.up.z    = 0.0f;
        m.matrix.at.x    = 0.0f;  m.matrix.at.y    = 0.0f;   m.matrix.at.z    = 1.0f;
        m.matrix.pos.x   = 0.0f;  m.matrix.pos.y   = 0.0f;   m.matrix.pos.z   = 0.0f;
        m.matrix.flags   = 0;
        return m;
    }();

    const RwUInt32 effect = desc->effect;   // desc byte3 >> 2

    if (effect != 2 && effect != 3) {
        // plain environment mapping: camera position + world-normal matrix
        const RwMatrix* camLTM = RwFrameGetLTM(
            reinterpret_cast<RwFrame*>(reinterpret_cast<RwCamera*>(RwEngineInstance->curCamera)->object.parent));
        float camPos[4] = { camLTM->pos.x, camLTM->pos.y, camLTM->pos.z, 0.0f };
        _rwD3D9VSGetPointInLocalSpace(&camLTM->pos, reinterpret_cast<RwV3d*>(camPos));

        RwMatrix normalMat;
        _rwD3D9VSGetWorldNormalizedTransposeMatrix(&normalMat);
        // ABI note: the repo decl of _rwD3D9VSGetWorldNormalizedTransposeMatrix is parameterless;
        // the real 0x764C60 takes (void* out). The out-param call matches the real ABI.

        if (effect == 10) {
            _RwD3DDevice->SetVertexShaderConstantF(dispatch->offsetEffect, camPos, 1);
            _RwD3DDevice->SetVertexShaderConstantF(dispatch->offsetEffect + 1,
                                                   reinterpret_cast<const float*>(&normalMat), 3);
        } else {
            _RwD3DDevice->SetVertexShaderConstantF(dispatch->offsetEffect + 2, camPos, 1);
            _RwD3DDevice->SetVertexShaderConstantF(dispatch->offsetEffect + 3,
                                                   reinterpret_cast<const float*>(&normalMat), 3);
        }
        return;
    }

    // bump + environment: env matrix from the bumped frame (or the camera when frame == NULL)
    RwMatrix envMat;
    if (frame == nullptr) {
        _rwD3D9VSGetWorldNormalizedViewMultiplyTransposeMatrix(&envMat, &envScale.matrix);
    } else {
        RwMatrix inverse;
        RwMatrixInvert(&inverse, RwFrameGetLTM(frame));
        RwMatrix scaled;
        RwMatrixMultiply(&scaled, &inverse, &envScale.matrix);
        _rwD3D9VSGetWorldNormalizedMultiplyTransposeMatrix(&envMat, &scaled);
    }

    const float constants[8] = {
        envMat.right.x, envMat.right.y, envMat.right.z, 0.5f,
        envMat.up.x,    envMat.up.y,    envMat.up.z,    0.5f,
    };

    RwUInt8 reg = dispatch->offsetEffect;
    if (effect != 2)
        reg = static_cast<RwUInt8>(reg + 2);
    _RwD3DDevice->SetVertexShaderConstantF(reg, constants, 2);
}


void _rpD3D9VertexShaderSetBumpMatrix(RwFrame* frame, RwReal factor,
                                      const _rpD3D9VertexShaderDispatchDescriptor* dispatch) {
    if (frame == nullptr)
        frame = reinterpret_cast<RwFrame*>(reinterpret_cast<RwCamera*>(RwEngineInstance->curCamera)->object.parent);

    RwMatrix inverse;
    RwMatrixInvert(&inverse, RwFrameGetLTM(frame));

    RwMatrix transposed;
    _rwD3D9VSGetWorldMultiplyTransposeMatrix(&transposed, &inverse);

    const float constants[8] = {
        transposed.right.x * factor,
        transposed.right.y * factor,
        transposed.right.z * factor,
        0.0f,
        transposed.up.x * factor,
        transposed.up.y * factor,
        transposed.up.z * factor,
        0.0f,
    };
    _RwD3DDevice->SetVertexShaderConstantF(dispatch->offsetEffect, constants, 2);
}


RxNodeDefinition* RxNodeDefinitionGetD3D9WorldSectorAllInOne() {
    static auto& nodeD3D9WorldSectorAllInOne = StaticRef<RxNodeDefinition>(0x8D63B8);
    return &nodeD3D9WorldSectorAllInOne;
}


RxNodeDefinition* RxNodeDefinitionGetD3D9AtomicAllInOne()
{
    return &StaticRef<RxNodeDefinition>(0x8D6338);   // статический nodeD3D9AtomicAllInOne (CSL)
}


void RpMaterialSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    StaticRef<RwInt32>(0x8D62C8) = blockSize;              // DAT_008d62c8
    StaticRef<RwInt32>(0x8D62CC) = numBlocksToPrealloc;    // DAT_008d62cc
}


RpMaterial* RpMaterialCreate() {
    struct RwMemFuncsGta {                                 // GTA RwGlobals memory-function area
        RwUInt8  pad[0x134];
        void* (*rwmalloc)(size_t size, RwUInt32 hint);                    // +0x134
        void   (*rwfree)(void* mem);                                      // +0x138
        void* (*rwrealloc)(void* mem, size_t newSize, RwUInt32 hint);     // +0x13C
        void* (*rwcalloc)(size_t numObj, size_t sizeObj, RwUInt32 hint);  // +0x140
        void* (*memoryAlloc)(RwFreeList* fl, RwUInt32 hint);              // +0x144 (RWSRCGLOBAL(memoryAlloc))
        void   (*memoryFree)(RwFreeList* fl, void* mem);                  // +0x148 (RWSRCGLOBAL(memoryFree))
    };
    RwMemFuncsGta* mem = reinterpret_cast<RwMemFuncsGta*>(RwEngineInstance);

    // RpMaterial freelist lives at RwEngineInstance + *(RwInt32*)0xC92514
    RwFreeList* matFreeList =
        *reinterpret_cast<RwFreeList**>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + StaticRef<RwInt32>(0xC92514));
    RpMaterial* material = static_cast<RpMaterial*>(mem->memoryAlloc(matFreeList, 0x30007 /* FUNCTION|rwID_MATERIAL */));
    if (material == nullptr)
        return nullptr;

    material->refCount            = 1;                       // i16 @ +0x18
    material->texture             = nullptr;                 // +0x00
    *reinterpret_cast<RwUInt32*>(&material->color) = 0xFFFFFFFF;  // RGBA 255,255,255,255 (+0x04)
    material->pipeline            = nullptr;                 // +0x08
    material->surfaceProps.ambient  = _rpMaterialDefaultSurfaceProperties.ambient;   // DAT_008d62bc
    material->surfaceProps.specular = _rpMaterialDefaultSurfaceProperties.specular;  // DAT_008d62c0
    material->surfaceProps.diffuse  = _rpMaterialDefaultSurfaceProperties.diffuse;   // DAT_008d62c4
    _rwPluginRegistryInitObject(&StaticRef<RwPluginRegistry>(0x8D62A4), material);    // materialTKList
    return material;
}


RwBool RpMaterialDestroy(RpMaterial* material) {
    struct RwMemFuncsGta {                                 // GTA RwGlobals memory-function area
        RwUInt8  pad[0x134];
        void* (*rwmalloc)(size_t size, RwUInt32 hint);                    // +0x134
        void   (*rwfree)(void* mem);                                      // +0x138
        void* (*rwrealloc)(void* mem, size_t newSize, RwUInt32 hint);     // +0x13C
        void* (*rwcalloc)(size_t numObj, size_t sizeObj, RwUInt32 hint);  // +0x140
        void* (*memoryAlloc)(RwFreeList* fl, RwUInt32 hint);              // +0x144 (RWSRCGLOBAL(memoryAlloc))
        void   (*memoryFree)(RwFreeList* fl, void* mem);                  // +0x148 (RWSRCGLOBAL(memoryFree))
    };
    RwMemFuncsGta* mem = reinterpret_cast<RwMemFuncsGta*>(RwEngineInstance);

    if (material->refCount == 1) {                         // i16 @ +0x18 (decompile: (short)param_1[6])
        _rwPluginRegistryDeInitObject(&StaticRef<RwPluginRegistry>(0x8D62A4), material);   // materialTKList
        if (material->texture != nullptr) {
            RwTextureDestroy(material->texture);
        }
        material->texture = nullptr;

        RwFreeList* matFreeList =
            *reinterpret_cast<RwFreeList**>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + StaticRef<RwInt32>(0xC92514));
        mem->memoryFree(matFreeList, material);            // RwFreeListFree
        return TRUE;
    }
    material->refCount--;
    return TRUE;
}


static auto&   rpMaterialPluginRegistry = StaticRef<RwPluginRegistry>(0x8D62A4);
// DAT_00c92514 holds the material-module globalsOffset: the RpMaterial RwFreeList* lives at
// RwEngineInstance + offset (same module-globals-offset pattern as rwResourcesGlobalsOffset 0xC9BC58).
static RwInt32& g_rpMaterialGlobalsOffset = StaticRef<RwInt32>(0xC92514);
inline RwFreeList* rpMaterialGetFreeListGta() {
    return *(RwFreeList**)(reinterpret_cast<RwUInt8*>(RwEngineInstance) + g_rpMaterialGlobalsOffset);
}
// RwGlobals freelist-heap alloc/free dispatch slots (decompile: +0x144 alloc(fl,hint), +0x148 free(fl,mem));
// ABI = rwplcore.h RwMemoryAllocFn/RwMemoryFreeFn (RwFreeList* first arg).
inline RpMaterial* rpMaterialFreelistAllocGta() {
    return (RpMaterial*)(*(void* (__cdecl**)(RwFreeList*, RwUInt32))((RwUInt8*)RwEngineInstance + 0x144))
                         (rpMaterialGetFreeListGta(), 0x30007 /* rwID_MATERIAL | 0x30000 */);
}
inline void rpMaterialFreelistFreeGta(RpMaterial* material) {
    (*(RwFreeList* (__cdecl**)(RwFreeList*, void*))((RwUInt8*)RwEngineInstance + 0x148))
                         (rpMaterialGetFreeListGta(), material);
}
// ==== END SHARED ====

RpMaterial* RpMaterialClone(RpMaterial* material) {
    RpMaterial* ret = rpMaterialFreelistAllocGta();
    if (ret != nullptr) {
        ret->refCount = 1;                                    // +0x18 (RwInt16)
        *(RwUInt32*)&ret->color = 0xFFFFFFFF;                 // piVar2[1] = -1  -> RGBA(255,255,255,255)
        ret->texture  = nullptr;                              // +0x00
        ret->pipeline = nullptr;                              // +0x08
        ret->surfaceProps = _rpMaterialDefaultSurfaceProperties;  // defaults from DAT_008d62bc..c4
        _rwPluginRegistryInitObject(&rpMaterialPluginRegistry, ret);

        // Decompile reads material->texture/color/pipeline, then guards the tail with
        // `param_1 + 3 != NULL` — a compiler artifact of `material != NULL` (lea mat+0xC; test).
        if (material != nullptr) {
            if (material->texture != nullptr) {
                material->texture->refCount++;                // RwTexture.refCount @ +0x54
                if (ret->texture != nullptr) {
                    RwTextureDestroy(ret->texture);
                }
                ret->texture = material->texture;
            }
            ret->color       = material->color;
            ret->pipeline    = material->pipeline;
            ret->surfaceProps = material->surfaceProps;
            _rwPluginRegistryCopyObject(&rpMaterialPluginRegistry, ret, material);
            return ret;
        }

        // Dead path (material == NULL): dispose of the fresh material (retained for fidelity).
        if (ret->refCount == 1) {
            _rwPluginRegistryDeInitObject(&rpMaterialPluginRegistry, ret);
            if (ret->texture != nullptr) {
                RwTextureDestroy(ret->texture);
            }
            ret->texture = nullptr;
            rpMaterialFreelistFreeGta(ret);
            return nullptr;
        }
        ret->refCount--;
    }
    return nullptr;
}


RpMaterial* RpMaterialSetTexture(RpMaterial* material, RwTexture* texture) {
    if (texture != nullptr) {
        texture->refCount++;                             // RwTexture.refCount @ +0x54
    }
    if (material->texture != nullptr) {
        RwTextureDestroy(material->texture);
    }
    material->texture = texture;
    return material;
}


RwInt32 RpMaterialRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return _rwPluginRegistryAddPlugin(&rpMaterialPluginRegistry, size, pluginID, constructCB, destructCB, copyCB);
}


RwInt32 RpMaterialRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return _rwPluginRegistryAddPluginStream(&rpMaterialPluginRegistry, pluginID, readCB, writeCB, getSizeCB);
}


RwInt32 RpMaterialSetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB) {
    return _rwPluginRegistryAddPlgnStrmlwysCB(&rpMaterialPluginRegistry, pluginID, alwaysCB);
}


RwInt32 RpMaterialSetStreamRightsCallBack(RwUInt32 pluginID, RwPluginDataChunkRightsCallBack rightsCB) {
    return _rwPluginRegistryAddPlgnStrmRightsCB(&rpMaterialPluginRegistry, pluginID, rightsCB);
}


RwInt32 RpMaterialGetPluginOffset(RwUInt32 pluginID) {
    return _rwPluginRegistryGetPluginOffset(&rpMaterialPluginRegistry, pluginID);
}


RwBool RpMaterialValidatePlugins(const RpMaterial* material) {
    return TRUE;
}


RwUInt32 RpMaterialStreamGetSize(const RpMaterial* material) {
    RwUInt32 size = 0x28;                                              // 0x1C struct + 0xC chunk hdr, no texture
    if (material->texture != nullptr) {
        size = RwTextureStreamGetSize(material->texture) + 0x34;
    }
    return (RwUInt32)_rwPluginRegistryGetSize(&rpMaterialPluginRegistry, material) + 0xC + size;
}


static RwUInt32& g_rpMaterialStreamRightsId   = StaticRef<RwUInt32>(0xC92510);
static RwUInt32& g_rpMaterialStreamRightsData = StaticRef<RwUInt32>(0xC924E8);

RpMaterial* RpMaterialStreamRead(RwStream* stream) {
    RwUInt32 length;
    RwUInt32 version;
    if (!RwStreamFindChunk(stream, rwID_STRUCT, &length, &version)) {
        return nullptr;
    }
    if (version < 0x34000 || 0x36003 < version) {
        RwError rwError;
        rwError.pluginID   = 2;                                        // rpworld module id (decompile: local_28[1]=2)
        rwError.errorCode  = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000004);  // RWERROR((E_RW_BADVERSION))
        RwErrorSet(&rwError);
        return nullptr;
    }

    RpMaterialChunkInfo matInfo;
    memset(&matInfo, 0, sizeof(matInfo));                              // decompile: 7-dword zero loop
    if (RwStreamRead(stream, &matInfo, length) != length) {
        return nullptr;
    }

    RpMaterial* material = rpMaterialFreelistAllocGta();
    if (material == nullptr) {
        return nullptr;
    }
    material->refCount = 1;
    *(RwUInt32*)&material->color = 0xFFFFFFFF;
    material->texture  = nullptr;
    material->pipeline = nullptr;
    material->surfaceProps = _rpMaterialDefaultSurfaceProperties;
    _rwPluginRegistryInitObject(&rpMaterialPluginRegistry, material);

    material->color        = matInfo.color;                            // matInfo dword @ +0x04
    material->surfaceProps = matInfo.surfaceProps;                     // matInfo dwords @ +0x10..+0x18
    material->texture      = nullptr;
    if (matInfo.textured != 0) {                                       // matInfo dword @ +0x0C
        if (!RwStreamFindChunk(stream, rwID_TEXTURE, nullptr, &version)) {
            goto dispose;
        }
        if (version < 0x34000 || 0x36003 < version) {
            RwError rwError;
            rwError.pluginID  = 2;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000004);
            RwErrorSet(&rwError);
            goto dispose;
        }
        // NB: vendor does NOT check RwTextureStreamRead failure — texture stays NULL and flow continues
        material->texture = RwTextureStreamRead(stream);
    }

    g_rpMaterialStreamRightsId   = 0;
    g_rpMaterialStreamRightsData = 0;
    if (_rwPluginRegistryReadDataChunks(&rpMaterialPluginRegistry, stream, material) != nullptr) {
        if (g_rpMaterialStreamRightsId != 0) {
            _rwPluginRegistryInvokeRights(&rpMaterialPluginRegistry, g_rpMaterialStreamRightsId,
                                          material, g_rpMaterialStreamRightsData);
        }
        return material;
    }

dispose:
    if (material->refCount == 1) {
        _rwPluginRegistryDeInitObject(&rpMaterialPluginRegistry, material);
        if (material->texture != nullptr) {
            RwTextureDestroy(material->texture);
        }
        material->texture = nullptr;
        rpMaterialFreelistFreeGta(material);
    } else {
        material->refCount--;
    }
    return nullptr;
}


const RpMaterial* RpMaterialStreamWrite(const RpMaterial* material, RwStream* stream) {
    RwUInt32 size = 0x28;
    if (material->texture != nullptr) {
        size = RwTextureStreamGetSize(material->texture) + 0x34;
    }
    if (_rwStreamWriteVersionedChunkHeader(stream, rwID_MATERIAL,
            _rwPluginRegistryGetSize(&rpMaterialPluginRegistry, material) + 0xC + size,
            rwLIBRARYVERSION36003, 0xffff) == nullptr) {
        return nullptr;
    }
    if (_rwStreamWriteVersionedChunkHeader(stream, rwID_STRUCT, 0x1C,
            rwLIBRARYVERSION36003, 0xffff) == nullptr) {
        return nullptr;
    }

    RpMaterialChunkInfo matInfo;
    memset(&matInfo, 0, sizeof(matInfo));
    // decompile fills: flags=0, color=mat->color(+0x04), textured=(mat->texture!=NULL),
    // surfaceProps=mat->surfaceProps(+0x0C..); the `unused` field (+0x08) is left as
    // uninitialized stack garbage by the vendor — zeroed here for determinism.
    matInfo.color        = material->color;
    matInfo.textured     = (material->texture != nullptr);
    matInfo.surfaceProps = material->surfaceProps;
    if (RwStreamWrite(stream, &matInfo, 0x1C) == 0) {
        return nullptr;
    }
    if (material->texture != nullptr) {
        if (RwTextureStreamWrite(material->texture, stream) == nullptr) {
            return nullptr;
        }
    }
    if (_rwPluginRegistryWriteDataChunks(&rpMaterialPluginRegistry, stream, material) == nullptr) {
        return nullptr;
    }
    return material;
}


RpMaterialChunkInfo* _rpMaterialChunkInfoRead(RwStream* stream, RpMaterialChunkInfo* materialChunkInfo, RwInt32* bytesRead) {
    RwUInt32 length;
    // decompile: RwStreamFindChunk(stream, rwID_STRUCT(1), &length, NULL)
    if (!RwStreamFindChunk(stream, rwID_STRUCT, &length, nullptr)) {
        return nullptr;
    }
    // decompile: 7-dword zero loop = whole 0x1C struct
    memset(materialChunkInfo, 0, sizeof(RpMaterialChunkInfo));
    if (RwStreamRead(stream, materialChunkInfo, length) != length) {
        return nullptr;
    }
    *bytesRead = (RwInt32)(length + 0xC);   // struct + chunk header
    if (length < 0x11) {
        // short (pre-0x11) chunk carries no surfaceProps -> default 1.0f (0x3F800000)
        materialChunkInfo->surfaceProps.ambient  = 1.0f;
        materialChunkInfo->surfaceProps.specular = 1.0f;
        materialChunkInfo->surfaceProps.diffuse  = 1.0f;
    }
    return materialChunkInfo;
}


RpMaterialList* _rpMaterialListInitialize(RpMaterialList* matList) {
    matList->materials = nullptr;
    matList->numMaterials = 0;
    matList->space = 0;
    return matList;
}


RpMaterialList* _rpMaterialListDeinitialize(RpMaterialList* matList) {
    if (matList->materials != nullptr) {
        for (RwInt32 i = 0; i < matList->numMaterials; i++) {
            RpMaterialDestroy(matList->materials[i]);
            matList->materials[i] = nullptr;
        }
        // decompile: RwGlobals +0x138 = RwMemoryFunctions.rwfree(ptr)
        (*(void (__cdecl**)(void*))((RwUInt8*)RwEngineInstance + 0x138))(matList->materials);
        matList->materials = nullptr;
    }
    matList->numMaterials = 0;
    matList->space = 0;
    return matList;
}


RpMaterial** _rpMaterialListAlloc(RwUInt32 count) {
    // decompile: RwGlobals +0x134 = RwMemoryFunctions.rwmalloc(size, hint 0x1030008)
    return (RpMaterial**)(*(void* (__cdecl**)(size_t, RwUInt32))((RwUInt8*)RwEngineInstance + 0x134))
                         (count * 4, 0x1030008);
}


RpMaterial* _rpMaterialListGetMaterial(const RpMaterialList* matList, RwInt32 matIndex) {
    return matList->materials[matIndex];
}


RpMaterialList* _rpMaterialListSetSize(RpMaterialList* matList, RwInt32 size) {
    if (matList->space < size) {
        RpMaterial** materials;
        if (matList->materials == nullptr) {
            materials = (RpMaterial**)(*(void* (__cdecl**)(size_t, RwUInt32))
                      ((RwUInt8*)RwEngineInstance + 0x134))(size * 4, 0x1030008);
        } else {
            // decompile shows the +0x13C slot call with the pointer arg only (stack-arg recovery
            // artifact); the slot is RwMemoryFunctions.rwrealloc(mem, newSize, hint) — 3-arg ABI
            // per rwplcore.h RwMemoryFunctions / rwplcore.cpp RwGlobalsSA layout. See STATUS.md.
            materials = (RpMaterial**)(*(void* (__cdecl**)(void*, size_t, RwUInt32))
                      ((RwUInt8*)RwEngineInstance + 0x13C))(matList->materials, size * 4, 0x1030008);
        }
        if (materials == nullptr) {
            RwError rwError;
            rwError.pluginID  = 2;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, size * 4);
            RwErrorSet(&rwError);
            return nullptr;
        }
        matList->materials = materials;
        matList->space = size;
    }
    return matList;
}


RpMaterialList* _rpMaterialListCopy(RpMaterialList* matListOut, const RpMaterialList* matListIn) {
    matListOut->numMaterials = matListIn->numMaterials;
    matListOut->space        = matListIn->space;
    matListOut->materials    = nullptr;
    if (matListIn->materials != nullptr) {
        // decompile: RwGlobals +0x134 rwmalloc(space * 4, 0x1030008)
        RpMaterial** materials = (RpMaterial**)(*(void* (__cdecl**)(size_t, RwUInt32))
                               ((RwUInt8*)RwEngineInstance + 0x134))(matListOut->space * 4, 0x1030008);
        matListOut->materials = materials;
        if (materials == nullptr) {
            RwError rwError;
            rwError.pluginID  = 2;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, matListOut->space * 4);
            RwErrorSet(&rwError);
            return nullptr;
        }
        RwInt32 i = 0;
        for (; i < matListOut->numMaterials; i++) {
            materials[i] = matListIn->materials[i];
            materials[i]->refCount++;                    // RwInt16 @ +0x18
        }
        for (; i < matListOut->space; i++) {
            materials[i] = nullptr;
        }
    }
    return matListOut;
}


RwInt32 _rpMaterialListAppendMaterial(RpMaterialList* matList, RpMaterial* material) {
    if (matList->numMaterials < matList->space) {
        matList->materials[matList->numMaterials] = material;
        material->refCount++;                            // RwInt16 @ +0x18
        RwInt32 index = matList->numMaterials;
        matList->numMaterials++;
        return index;
    }

    // grow: space*4 + 0x50 bytes (= 20 extra entries)
    RwInt32 newSize = matList->space * 4 + 0x50;
    RpMaterial** materials;
    if (matList->materials == nullptr) {
        materials = (RpMaterial**)(*(void* (__cdecl**)(size_t, RwUInt32))
                  ((RwUInt8*)RwEngineInstance + 0x134))(newSize, 0x1030008);
    } else {
        // rwrealloc @+0x13C, 3-arg ABI — see note in 0x74E2C0.cpp / STATUS.md
        materials = (RpMaterial**)(*(void* (__cdecl**)(void*, size_t, RwUInt32))
                  ((RwUInt8*)RwEngineInstance + 0x13C))(matList->materials, newSize, 0x1030008);
    }
    if (materials == nullptr) {
        RwError rwError;
        rwError.pluginID  = 2;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, newSize);
        RwErrorSet(&rwError);
        return -1;
    }
    matList->materials = materials;
    matList->space += 0x14;                              // +20 entries
    matList->materials[matList->numMaterials] = material;
    material->refCount++;
    RwInt32 index = matList->numMaterials;
    matList->numMaterials++;
    return index;
}


RwInt32 _rpMaterialListFindMaterialIndex(const RpMaterialList* matList, const RpMaterial* material) {
    RwInt32 index = matList->numMaterials;
    if (0 < index) {
        do {
            index--;
            if (matList->materials[index] == material) {
                return index;
            }
        } while (0 < index);
    }
    return index - 1;                                    // not found -> -1
}


RwUInt32 _rpMaterialListStreamGetSize(const RpMaterialList* matList) {
    RwUInt32 size = matList->numMaterials * 4 + 0x10;
    for (RwInt32 i = 0; i < matList->numMaterials; i++) {
        // decompile: backward scan j = i-1..0 for the first earlier occurrence of materials[i]
        RwInt32 first = -1;
        if (i != 0) {
            RwInt32 j = i;
            do {
                j--;
                if (matList->materials[j] == matList->materials[i]) {
                    first = j;
                    break;
                }
            } while (j != 0);
        }
        if (first < 0) {
            size += 0xC + RpMaterialStreamGetSize(matList->materials[i]);   // chunk hdr + material
        }
    }
    return size;
}


RpMaterialList* _rpMaterialListStreamRead(RwStream* stream, RpMaterialList* matList) {
    RwUInt32 length;
    RwUInt32 version;
    if (!RwStreamFindChunk(stream, rwID_STRUCT, &length, &version)) {
        return nullptr;
    }
    if (version < 0x34000 || 0x36003 < version) {
        RwError rwError;
        rwError.pluginID  = 2;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000004);
        RwErrorSet(&rwError);
        return nullptr;
    }

    RwInt32 numMaterials;
    if (RwStreamReadInt32(stream, &numMaterials, 4) == nullptr) {
        return nullptr;
    }

    matList->materials    = nullptr;
    matList->numMaterials = 0;
    matList->space        = 0;
    if (numMaterials == 0) {
        return matList;
    }

    // preallocate the array (hint 0x1030008)
    matList->materials = (RpMaterial**)(*(void* (__cdecl**)(size_t, RwUInt32))
                       ((RwUInt8*)RwEngineInstance + 0x134))(numMaterials * 4, 0x1030008);
    if (matList->materials == nullptr) {
        RwError rwError;
        rwError.pluginID  = 2;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, numMaterials * 4);
        RwErrorSet(&rwError);
        return nullptr;
    }
    matList->space = numMaterials;

    // failure cleanup (vendor inlines this sequence at each failure site):
    // destroy appended materials, free the array, zero the list
    RpMaterial** materials = matList->materials;
    RwInt32 i;

    // raw index table (hint 0x10501)
    RwInt32* indices = (RwInt32*)(*(void* (__cdecl**)(size_t, RwUInt32))
                     ((RwUInt8*)RwEngineInstance + 0x134))(numMaterials * 4, 0x10501);
    if (RwStreamReadInt32(stream, indices, numMaterials * 4) == nullptr) {
        (*(void (__cdecl**)(void*))((RwUInt8*)RwEngineInstance + 0x138))(indices);
        goto fail;
    }

    for (i = 0; i < numMaterials; i++) {
        RpMaterial* material;
        RwInt32 index = indices[i];
        if (index < 0) {
            // new material chunk follows
            if (!RwStreamFindChunk(stream, rwID_MATERIAL, nullptr, &version)) {
                (*(void (__cdecl**)(void*))((RwUInt8*)RwEngineInstance + 0x138))(indices);
                goto fail;
            }
            if (version < 0x34000 || 0x36003 < version) {
                RwError rwError;
                rwError.pluginID  = 2;
                rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000004);
                RwErrorSet(&rwError);
                (*(void (__cdecl**)(void*))((RwUInt8*)RwEngineInstance + 0x138))(indices);
                goto fail;
            }
            material = RpMaterialStreamRead(stream);
            if (material == nullptr) {
                (*(void (__cdecl**)(void*))((RwUInt8*)RwEngineInstance + 0x138))(indices);
                goto fail;
            }
        } else {
            // reused material: extra ref (append below keeps it)
            material = matList->materials[index];
            material->refCount++;
        }
        _rpMaterialListAppendMaterial(matList, material);
        RpMaterialDestroy(material);      // balances the read/manual ref; append kept one
    }

    (*(void (__cdecl**)(void*))((RwUInt8*)RwEngineInstance + 0x138))(indices);
    return matList;

fail:
    if (materials != nullptr) {
        for (RwInt32 k = 0; k < matList->numMaterials; k++) {
            RpMaterialDestroy(materials[k]);
            materials[k] = nullptr;
        }
        (*(void (__cdecl**)(void*))((RwUInt8*)RwEngineInstance + 0x138))(materials);
    }
    matList->materials    = nullptr;
    matList->numMaterials = 0;
    matList->space        = 0;
    return nullptr;
}


const RpMaterialList* _rpMaterialListStreamWrite(const RpMaterialList* matList, RwStream* stream) {
    // total MATLIST size: struct chunk (numMaterials*4 + 4) + 0xC + first-occurrence materials
    RwUInt32 size = matList->numMaterials * 4 + 0x10;
    for (RwInt32 i = 0; i < matList->numMaterials; i++) {
        RwInt32 first = -1;
        if (i != 0) {
            RwInt32 j = i;
            do {
                j--;
                if (matList->materials[j] == matList->materials[i]) {
                    first = j;
                    break;
                }
            } while (j != 0);
        }
        if (first < 0) {
            size += 0xC + RpMaterialStreamGetSize(matList->materials[i]);
        }
    }

    if (_rwStreamWriteVersionedChunkHeader(stream, rwID_MATLIST, size,
            rwLIBRARYVERSION36003, 0xffff) == nullptr) {
        return nullptr;
    }
    if (_rwStreamWriteVersionedChunkHeader(stream, rwID_STRUCT, matList->numMaterials * 4 + 4,
            rwLIBRARYVERSION36003, 0xffff) == nullptr) {
        return nullptr;
    }
    if (RwStreamWriteInt32(stream, (const RwInt32*)&matList->numMaterials, 4) == nullptr) {
        return nullptr;
    }

    // index table: first-occurrence index (backward-scan match) or -1
    for (RwInt32 i = 0; i < matList->numMaterials; i++) {
        RwInt32 index = -1;
        if (i != 0) {
            RwInt32 j = i;
            do {
                j--;
                if (matList->materials[j] == matList->materials[i]) {
                    index = j;
                    break;
                }
            } while (j != 0);
        }
        if (RwStreamWriteInt32(stream, &index, 4) == nullptr) {
            return nullptr;
        }
    }

    // material chunks: only for first occurrences
    for (RwInt32 i = 0; i < matList->numMaterials; i++) {
        RwInt32 index = -1;
        if (i != 0) {
            RwInt32 j = i;
            do {
                j--;
                if (matList->materials[j] == matList->materials[i]) {
                    index = j;
                    break;
                }
            } while (j != 0);
        }
        if (index < 0 && RpMaterialStreamWrite(matList->materials[i], stream) == nullptr) {
            return nullptr;
        }
    }
    return matList;
}


void* _rpMeshOpen(void* instance, RwInt32 offset, RwInt32 size) {
    (void)size;
    rpMeshGlobalsOffsetGta = offset;
    if (rpMeshOpenCountGta == 0) {
        // декомпил: FUN_00801980(0xC, 0x32, 4, 0x40502) — бинарник зовёт 4 аргумента;
        // репо-обёртка _rwFreeListCreate (rwplcore.cpp) принимает ещё (файл, строку)
        rpMeshFreeListGta = ((RwFreeList*(__cdecl*)(RwInt32, RwInt32, RwInt32, RwUInt32))0x801980)(0xC, 0x32, 4, 0x40502);
        if (rpMeshFreeListGta == nullptr) {
            return nullptr;
        }
    }
    RwUInt8* globals = reinterpret_cast<RwUInt8*>(RwEngineInstance) + rpMeshGlobalsOffsetGta;
    *reinterpret_cast<RwUInt16*>(globals) = 1;   // стартовый serial number
    rpMeshOpenCountGta++;
    globals[0x08] = 3;
    globals[0x09] = 4;
    globals[0x0A] = 5;
    globals[0x0C] = 1;
    globals[0x10] = 2;
    globals[0x18] = 6;
    globals[0x28] = 4;
    globals[0x29] = 8;
    globals[0x2A] = 0;
    globals[0x2B] = 1;
    globals[0x2C] = 2;
    globals[0x2D] = 0x10;
    return instance;
}


void* _rpMeshClose(void* instance, RwInt32 offset, RwInt32 size) {
    (void)offset;
    (void)size;
    rpMeshOpenCountGta--;
    if ((rpMeshOpenCountGta == 0) && (rpMeshFreeListGta != nullptr)) {
        RwFreeListDestroy(rpMeshFreeListGta);   // репо-обёртка (rwplcore.cpp), в карте: 0x801B80
        rpMeshFreeListGta = nullptr;
    }
    return instance;
}


RwInt16 _rpMeshGetNextSerialNumber(void) {
    RwUInt16* serial = reinterpret_cast<RwUInt16*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + rpMeshGlobalsOffsetGta);
    *serial = static_cast<RwUInt16>(*serial + 1);
    return static_cast<RwInt16>(*serial);
}


RpBuildMesh* _rpBuildMeshCreate(RwUInt32 bufferSize) {
    RwGlobalsMemFuncsGta* mem = reinterpret_cast<RwGlobalsMemFuncsGta*>(RwEngineInstance);
    RpBuildMesh* mesh = reinterpret_cast<RpBuildMesh*>(mem->memoryAlloc(rpMeshFreeListGta, 0x30502));
    if (mesh == nullptr) {
        RwError rwError;
        rwError.pluginID = 2;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, 0xC);
        RwErrorSet(&rwError);
        return nullptr;
    }
    mesh->numTriangles = 0;
    if (bufferSize != 0) {
        mesh->meshTriangles = reinterpret_cast<RpBuildMeshTriangle*>(mem->rwmalloc(bufferSize * 0x14, 0x1030502));
        if (mesh->meshTriangles == nullptr) {
            mem->memoryFree(rpMeshFreeListGta, mesh);
            RwError rwError;
            rwError.pluginID = 2;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, bufferSize * 0x14);
            RwErrorSet(&rwError);
            return nullptr;
        }
        mesh->triangleBufferSize = bufferSize;
        return mesh;
    }
    mesh->meshTriangles = nullptr;
    mesh->triangleBufferSize = 0;
    return mesh;
}


RwBool _rpBuildMeshDestroy(RpBuildMesh* mesh) {
    RwGlobalsMemFuncsGta* mem = reinterpret_cast<RwGlobalsMemFuncsGta*>(RwEngineInstance);
    if (mesh->meshTriangles != nullptr) {
        mem->rwfree(mesh->meshTriangles);
        mesh->meshTriangles = nullptr;
    }
    mem->memoryFree(rpMeshFreeListGta, mesh);
    return TRUE;
}


RwBool _rpMeshDestroy(RpMeshHeader* mesh) {
    if ((mesh->flags != 0) || (mesh->numMeshes != 0) || (mesh->serialNum != 0) ||
        (mesh->totalIndicesInMesh != 0) || (mesh->firstMeshOffset != 0)) {
        reinterpret_cast<RwGlobalsMemFuncsGta*>(RwEngineInstance)->rwfree(mesh);
    }
    return TRUE;
}


RpBuildMesh* _rpBuildMeshAddTriangle(RpBuildMesh* mesh, RpMaterial* material, RwInt32 vert1, RwInt32 vert2, RwInt32 vert3,
                                     RwUInt16 matIndex, RwUInt16 textureIndex, RwUInt16 rasterIndex, RwUInt16 pipelineIndex) {
    RwGlobalsMemFuncsGta* mem = reinterpret_cast<RwGlobalsMemFuncsGta*>(RwEngineInstance);
    if (mesh->triangleBufferSize <= mesh->numTriangles) {
        RpBuildMeshTriangle* triangles;
        if (mesh->numTriangles == 0) {
            triangles = reinterpret_cast<RpBuildMeshTriangle*>(mem->rwmalloc(0x14, 0x1030502));
        } else {
            triangles = reinterpret_cast<RpBuildMeshTriangle*>(
                mem->rwrealloc(mesh->meshTriangles, (mesh->numTriangles * 5 + 5) * 4, 0x1030502));
        }
        if (triangles == nullptr) {
            RwError rwError;
            rwError.pluginID = 2;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, (mesh->numTriangles * 5 + 5) * 4);
            RwErrorSet(&rwError);
            return nullptr;
        }
        mesh->meshTriangles = triangles;
        mesh->triangleBufferSize = mesh->numTriangles + 1;
    }
    RpBuildMeshTriangle* tri = &mesh->meshTriangles[mesh->numTriangles];
    tri->material = material;                          // +0x08
    tri->vertIndex[0] = static_cast<RwUInt16>(vert1);  // +0x00
    tri->vertIndex[1] = static_cast<RwUInt16>(vert2);  // +0x02
    tri->vertIndex[2] = static_cast<RwUInt16>(vert3);  // +0x04
    tri->matIndex = matIndex;                          // +0x0C
    tri->textureIndex = textureIndex;                  // +0x0E
    tri->rasterIndex = rasterIndex;                    // +0x10
    tri->pipelineIndex = pipelineIndex;                // +0x12
    mesh->numTriangles++;
    return mesh;
}


RpMeshHeader* _rpMeshHeaderForAllMeshes(RpMeshHeader* meshHeader, RpMeshCallBack fpCallBack, void* data) {
    RwUInt16 remaining = meshHeader->numMeshes;
    RpMesh* mesh = reinterpret_cast<RpMesh*>(reinterpret_cast<RwUInt8*>(meshHeader) + meshHeader->firstMeshOffset + 0x10);
    while (remaining != 0) {
        remaining--;
        if (fpCallBack(mesh, meshHeader, data) == 0) {
            break;
        }
        mesh++;   // sizeof(RpMesh) == 0xC
    }
    return meshHeader;
}


RwStream* _rpMeshWrite(const RpMeshHeader* meshHeader, const void* object, RwStream* stream, const RpMaterialList* matList) {
    RwInt32 header[3];
    header[0] = static_cast<RwInt32>(meshHeader->flags);            // local_40c
    header[1] = static_cast<RwInt32>(meshHeader->numMeshes);        // local_408
    header[2] = static_cast<RwInt32>(meshHeader->totalIndicesInMesh); // local_404
    if (RwStreamWriteInt32(stream, header, 0xC) == nullptr) {
        return nullptr;
    }
    const RpMesh* mesh = reinterpret_cast<const RpMesh*>(reinterpret_cast<const RwUInt8*>(meshHeader) + meshHeader->firstMeshOffset + 0x10);
    RwUInt16 remaining = meshHeader->numMeshes;
    while (remaining != 0) {
        remaining--;
        RwInt32 entry[2];
        entry[0] = static_cast<RwInt32>(mesh->numIndices);
        entry[1] = _rpMaterialListFindMaterialIndex(matList, mesh->material);
        if (entry[1] < 0) {
            entry[1] = 0;
        }
        if (RwStreamWriteInt32(stream, entry, 8) == nullptr) {
            return nullptr;
        }
        // индексы пишутся на стрим как RwUInt32 только для НЕ natively-инстанцированных объектов
        // (тип 8 = rpGEOMETRY, 7 = rpWORLD; бит 0x01000000 = rpGEOMETRYNATIVE/rpWORLDNATIVE)
        const RwObject* obj = static_cast<const RwObject*>(object);
        if (((obj->type == rpGEOMETRY) || (obj->type == rpWORLD)) &&
            ((*reinterpret_cast<const RwUInt32*>(reinterpret_cast<const RwUInt8*>(object) + 8) & 0x01000000) == 0)) {
            RwUInt32 indices[256];
            RwUInt32 left = mesh->numIndices;
            const RwUInt16* src = mesh->indices;
            while (left != 0) {
                RwUInt32 chunk = (left < 256) ? left : 256;
                for (RwUInt32 i = 0; i < chunk; i++) {
                    indices[i] = src[i];
                }
                if (RwStreamWriteInt32(stream, reinterpret_cast<const RwInt32*>(indices), chunk * 4) == nullptr) {
                    return nullptr;
                }
                left -= chunk;
                src += chunk;
            }
        }
        mesh++;
    }
    return stream;
}


RpMeshHeader* _rpMeshRead(RwStream* stream, const void* object, const RpMaterialList* matList) {
    RwInt32 header[3];   // local_418, local_414, local_410
    if (RwStreamReadInt32(stream, header, 0xC) == nullptr) {
        return nullptr;
    }
    RwUInt32 numMeshes = static_cast<RwUInt32>(header[1]);
    RwUInt32 totalIndices = static_cast<RwUInt32>(header[2]);
    RwUInt32 size = (numMeshes + 1) * 0x10;
    const RwObject* obj = static_cast<const RwObject*>(object);
    if (((obj->type == rpGEOMETRY) || (obj->type == rpWORLD)) &&
        ((*reinterpret_cast<const RwUInt32*>(reinterpret_cast<const RwUInt8*>(object) + 8) & 0x01000000) == 0)) {
        size += totalIndices * 2;
    }
    RwGlobalsMemFuncsGta* mem = reinterpret_cast<RwGlobalsMemFuncsGta*>(RwEngineInstance);
    RpMeshHeader* meshHeader = reinterpret_cast<RpMeshHeader*>(mem->rwmalloc(size, 0x30502));
    if (meshHeader != nullptr) {
        RpMesh* mesh = reinterpret_cast<RpMesh*>(meshHeader + 1);
        RxVertexIndex* indices = reinterpret_cast<RxVertexIndex*>(mesh + numMeshes);
        meshHeader->flags = static_cast<RwUInt32>(header[0]);
        meshHeader->numMeshes = static_cast<RwUInt16>(numMeshes);
        RwUInt16* serial = reinterpret_cast<RwUInt16*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + rpMeshGlobalsOffsetGta);
        meshHeader->serialNum = *serial;
        *serial = static_cast<RwUInt16>(*serial + 1);
        meshHeader->totalIndicesInMesh = totalIndices;
        meshHeader->firstMeshOffset = 0;
        RwUInt16 remaining = meshHeader->numMeshes;
        while (remaining != 0) {
            remaining--;
            RwInt32 entry[2];   // iStack_408, uStack_404
            if (RwStreamReadInt32(stream, entry, 8) == nullptr) {
                return nullptr;   // утечка meshHeader — как в оригинале
            }
            mesh->numIndices = static_cast<RwUInt32>(entry[0]);
            mesh->material = _rpMaterialListGetMaterial(matList, entry[1]);
            mesh->indices = indices;
            if (((obj->type == rpGEOMETRY) || (obj->type == rpWORLD)) &&
                ((*reinterpret_cast<const RwUInt32*>(reinterpret_cast<const RwUInt8*>(object) + 8) & 0x01000000) == 0)) {
                RwUInt32 left = mesh->numIndices;
                while (left != 0) {
                    RwUInt16 buffer[512];   // auStack_400
                    RwUInt32 chunk = (left < 256) ? left : 256;
                    if (RwStreamReadInt32(stream, reinterpret_cast<RwInt32*>(buffer), chunk * 4) == nullptr) {
                        return nullptr;   // утечка meshHeader — как в оригинале
                    }
                    left -= chunk;
                    for (RwUInt32 i = 0; i < chunk; i++) {
                        *indices++ = buffer[i];
                    }
                }
            }
            mesh++;
        }
    }
    return meshHeader;
}


RwInt32 _rpMeshSize(const RpMeshHeader* meshHeader, const void* object) {
    const RwObject* obj = static_cast<const RwObject*>(object);
    RwUInt32 native = *reinterpret_cast<const RwUInt32*>(reinterpret_cast<const RwUInt8*>(object) + 8) & 0x01000000;
    if (((obj->type != rpGEOMETRY) || (native == 0)) && ((obj->type != rpWORLD) || (native == 0))) {
        // индексы на стриме 32-битные + 8 байт шапки на меш
        return static_cast<RwInt32>((meshHeader->totalIndicesInMesh + static_cast<RwUInt32>(meshHeader->numMeshes) * 2) * 4 + 0xC);
    }
    return static_cast<RwInt32>(static_cast<RwUInt32>(meshHeader->numMeshes) * 8 + 0xC);
}


void _rpMeshHeaderDestroy(RpMeshHeader* meshHeader)
{
    RwFree(meshHeader);
}


RpMeshHeader* _rpMeshHeaderCreate(RwUInt32 size)
{
    return (RpMeshHeader*)RwMalloc(size, 0x30502);
}


RpTriStripPolygon* RpTriStripPolygonFollowStrip(RpTriStripPolygon* curr, RpTriStripPolygon* prev) {
    if (curr->numEdges != 0) {
        for (RwUInt32 i = 0; i < curr->numEdges; i++) {
            RpTriStripEdge* edge = curr->edges[i];
            if ((edge->strip != 0) || (prev == edge->poly1) || (prev == edge->poly2)) {
                continue;
            }
            RpTriStripPolygon* next = edge->poly1;
            if (curr == next) {
                next = edge->poly2;
            }
            return next;
        }
    }
    return nullptr;
}


RwUInt32 RpTriStripDefaultCost(RpTriStripPolygon* startPolygon, RwUInt32 testFrame, void* data) {
    (void)data;
    RwUInt32 cleanStrips = 1;              // uStack_c
    RwUInt32 dirtyStrips = 0;              // uStack_20
    RwUInt32 cost = 40000;                 // iStack_14
    RpTriStripPolygon* windowBase = startPolygon;  // psStack_10
    RpTriStripPolygon* p1 = nullptr;       // iStack_1c
    RpTriStripPolygon* p2 = nullptr;       // iStack_18
    RpTriStripPolygon* p3 = nullptr;       // param_1
    RwUInt32 i;

    startPolygon->testFrame = testFrame;

    if (startPolygon->numEdges != 0) {
        for (i = 0; i < startPolygon->numEdges; i++) {
            RpTriStripEdge* edge = startPolygon->edges[i];
            if ((edge->strip != 0) && (edge->poly1 != nullptr) && (edge->poly2 != nullptr)) {
                p1 = edge->poly1;
                if (startPolygon == p1) {
                    p1 = edge->poly2;
                }
                break;
            }
        }
    }
    if (p1 != nullptr) {
        p1->testFrame = testFrame;
        if (p1->numEdges != 0) {
            for (i = 0; i < p1->numEdges; i++) {
                RpTriStripEdge* edge = p1->edges[i];
                if ((edge->strip != 0) && (startPolygon != edge->poly1) && (startPolygon != edge->poly2)) {
                    p2 = edge->poly1;
                    if (p1 == p2) {
                        p2 = edge->poly2;
                    }
                    break;
                }
            }
        }
        if (p2 != nullptr) {
            p2->testFrame = testFrame;
            if (p2->numEdges != 0) {
                for (i = 0; i < p2->numEdges; i++) {
                    RpTriStripEdge* edge = p2->edges[i];
                    if ((edge->strip != 0) && (p1 != edge->poly1) && (p1 != edge->poly2)) {
                        p3 = edge->poly1;
                        if (p2 == p3) {
                            p3 = edge->poly2;
                        }
                        break;
                    }
                }
            }
            if (p3 != nullptr) {
                RwBool restart;   // LAB_0075b6f1
                do {
                    RpTriStripPolygon* prevPoly = p2;   // iVar3
                    restart = FALSE;
                    p3->testFrame = testFrame;
                    // проверка: есть ли вершина, общая для windowBase, p1, p2 и p3
                    const RxVertexIndex* windowVerts = windowBase->vertIndex;
                    for (RwUInt32 v = 0; v < windowBase->numEdges; v++) {
                        RxVertexIndex vert = windowVerts[v];
                        RwBool found = FALSE;
                        for (i = 0; i < p1->numEdges; i++) {          // поиск в p1
                            if (vert == p1->vertIndex[i]) { found = TRUE; break; }
                        }
                        if (!found) {
                            continue;                                  // LAB_0075b6a7
                        }
                        found = FALSE;
                        for (i = 0; i < p2->numEdges; i++) {          // поиск в p2
                            if (vert == p2->vertIndex[i]) { found = TRUE; break; }
                        }
                        if (!found) {
                            continue;                                  // LAB_0075b6a7
                        }
                        found = FALSE;
                        RwUInt32 idx = p3->numEdges;
                        for (i = 0; i < p3->numEdges; i++) {          // поиск в p3
                            if (vert == p3->vertIndex[i]) { found = TRUE; idx = i; break; }
                        }
                        if (found && (idx < p2->numEdges)) {           // общая всем четырём
                            dirtyStrips = 1;                           // uStack_20 = 1
                            cost += 10000;
                            restart = TRUE;                            // goto LAB_0075b6f1
                            break;
                        }
                        // не найдена в p3 → следующая вершина (LAB_0075b6a7)
                    }
                    if (!restart) {
                        // LAB_0075b6c3
                        if (dirtyStrips == 0) {
                            cleanStrips++;
                        } else {
                            dirtyStrips++;
                        }
                    }
                    // LAB_0075b6f1: сдвиг окна вдоль полосы
                    windowBase = p1;
                    p1 = p2;
                    p2 = p3;
                    RpTriStripPolygon* next = nullptr;
                    if (p3->numEdges != 0) {
                        for (i = 0; i < p3->numEdges; i++) {
                            RpTriStripEdge* edge = p3->edges[i];
                            if ((edge->strip != 0) && (prevPoly != edge->poly1) && (prevPoly != edge->poly2)) {
                                next = edge->poly1;
                                if (p3 == next) {
                                    next = edge->poly2;
                                }
                                break;
                            }
                        }
                    }
                    p3 = next;
                } while (p3 != nullptr);
                if (cleanStrips < dirtyStrips) {
                    return cleanStrips + cost;
                }
            }
        }
    }
    return cost + dirtyStrips;
}


struct RpTriStripCandGta
{
    RpTriStripEdge*    edge;      // +0x00
    RpTriStripPolygon* polygon;   // +0x04
    RwUInt32           depth;     // +0x08
};

RpTriStripMesh* RpTriStripMeshTunnel(RpTriStripMesh* mesh, void* data) {
    RpTriStripTunnelData* tunnelData = static_cast<RpTriStripTunnelData*>(data);
    if (tunnelData->lengthLimit == 0) {
        return mesh;
    }
    RwGlobalsMemFuncsGta* mem = reinterpret_cast<RwGlobalsMemFuncsGta*>(RwEngineInstance);

    // workspace: rwmalloc(lengthLimit*8 + 0x14); [0]=глубина, [1]=число проходов,
    // [2]=база стека рёбер (доступ (base-4)+i*4), [3]=база стека полигонов (base+i*4)
    RwUInt32* work = reinterpret_cast<RwUInt32*>(mem->rwmalloc(tunnelData->lengthLimit * 8 + 0x14, 0x10502));
    work[2] = reinterpret_cast<RwUInt32>(work + 4);
    work[0] = 0;
    work[1] = 1;
    work[3] = reinterpret_cast<RwUInt32>(work + 4 + tunnelData->lengthLimit);
    RpTriStripCandGta* candidates = reinterpret_cast<RpTriStripCandGta*>(mem->rwmalloc(tunnelData->lengthLimit * 0x24, 0x10502));
    RwReal accum = 0.0f;                                                      // fStack_20
    double threshold = pow(0.001f, static_cast<double>(tunnelData->quality)); // dVar14
    RwUInt32 consumedCount = 0;                                               // uStack_2c

    // все объявления наверху — чтобы goto оставался легальным
    RpTriStripPolygon* polygon;
    RpTriStripPolygon* cur;
    RpTriStripCandGta* cand;
    RpTriStripCandGta* last;
    RwUInt32 depth;
    RpTriStripEdge* edge;
    RpTriStripPolygon* other;
    RwUInt32 i;
    RwUInt32 k;
    RpTriStripPolygon** polyStack;
    RwLLLink* node;
    RpTriStripPolygon* poly;
    RwInt32 stripped;
    RwBool windingOk;
    RwUInt32 costA;
    RwUInt32 costB;
    RwUInt32 keepOrig;
    RwBool doExtend;

    polygon = reinterpret_cast<RpTriStripPolygon*>(mesh->polygonEndList.link.next);
nextPolygon:   // LAB_0075b9cd / строка 58
    if (reinterpret_cast<RwLLLink*>(polygon) == &mesh->polygonEndList.link) {
        goto restartPass;   // LAB_0075b9fb: конец end-листа — проверка сходимости
    }
    *reinterpret_cast<RpTriStripPolygon**>(work[3]) = polygon;
    cur = *reinterpret_cast<RpTriStripPolygon**>(work[3]);
    if (cur->numEdges == 0) {
        polygon = reinterpret_cast<RpTriStripPolygon*>(cur->inEndLink.next);
        goto nextPolygon;
    }
    // кандидаты стартовых рёбер (строки 67-86)
    cand = candidates;
    for (i = 0; i < cur->numEdges; i++) {
        edge = cur->edges[i];
        if (edge->strip == 0) {
            other = edge->poly1;
            if (cur == other) {
                other = edge->poly2;
            }
            if (other != nullptr) {
                cand->edge = edge;
                cand->polygon = other;
                cand->depth = 1;
                cand++;
            }
        }
    }
joined:        // joined_r0x0075b890
    if (cand == candidates) {
        polygon = reinterpret_cast<RpTriStripPolygon*>(polygon->inEndLink.next);
        goto nextPolygon;
    }
    // последний кандидат задаёт старт полосы (строки 89-94);
    // стек рёбер живёт по адресам work+0xC+(i+1)*4 — декомпил: (work[2] - 4) + i*4
    last = cand - 1;
    work[0] = last->depth;
    *reinterpret_cast<RpTriStripEdge**>(work[2] - 4 + work[0] * 4) = last->edge;
    polyStack = reinterpret_cast<RpTriStripPolygon**>(work[3]);
    polyStack[work[0]] = last->polygon;
    cand = last;   // piVar9 = piVar13
    depth = work[0];
    doExtend = ((depth & 1) != 0) && (polyStack[depth]->inEndLink.next == nullptr);
    if (!doExtend) {
        // строки 136-143: инвертировать strip-флаги стека рёбер
        for (i = 0; i < depth; i++) {
            edge = *reinterpret_cast<RpTriStripEdge**>(work[2] + i * 4);
            edge->strip = static_cast<RwUInt32>(edge->strip == 0);
        }
        windingOk = reinterpret_cast<RwBool (__cdecl*)(RwUInt32*)>(0x75BB80)(work);
        if ((windingOk == 0) || (depth < 2)) {
            reinterpret_cast<void (__cdecl*)(RwUInt32*)>(0x75BB20)(work);
            keepOrig = 0;
        } else {
            costA = reinterpret_cast<RwUInt32 (__cdecl*)(RwUInt32*, RpTriStripTunnelData*)>(0x75BC80)(work, tunnelData);
            reinterpret_cast<void (__cdecl*)(RwUInt32*)>(0x75BB20)(work);
            costB = reinterpret_cast<RwUInt32 (__cdecl*)(RwUInt32*, RpTriStripTunnelData*)>(0x75BC80)(work, tunnelData);
            keepOrig = (costA < costB) ? 1u : 0u;
        }
        if (keepOrig == 0) {
            doExtend = TRUE;                        // строка 154 → LAB_0075b934
        } else if (reinterpret_cast<RwLLLink*>(polygon) == &mesh->polygonEndList.link) {
            goto restartPass;                        // строка 155 → LAB_0075b9fb
        } else {
            // строки 156-163: второй toggle — оставляем «перевёрнутую» ориентацию
            for (i = 0; i < depth; i++) {
                edge = *reinterpret_cast<RpTriStripEdge**>(work[2] + i * 4);
                edge->strip = static_cast<RwUInt32>(edge->strip == 0);
            }
        }
    }
    if (doExtend) {
        // LAB_0075b934 (строки 96-133): достроение кандидатов от полигона polyStack[depth]
        if (depth < work[1]) {
            cur = polyStack[depth];
            if (cur->numEdges != 0) {
                for (i = 0; i < cur->numEdges; i++) {
                    edge = cur->edges[i];
                    if (((depth & 1) == edge->strip) &&
                        (edge != *reinterpret_cast<RpTriStripEdge**>(work[2] - 4 + depth * 4))) {
                        other = edge->poly1;
                        if (cur == other) {
                            other = edge->poly2;
                        }
                        cand->polygon = other;
                        if (other != nullptr) {
                            polyStack = reinterpret_cast<RpTriStripPolygon**>(work[3]);
                            k = 0;
                            do {
                                if (other == polyStack[k]) {
                                    break;
                                }
                                k++;
                            } while (k <= depth);
                            if (depth < k) {         // нет в текущей полосе
                                cand->edge = edge;
                                cand->depth = depth + 1;
                                cand++;
                            }
                        }
                    }
                }
            }
        }
        goto joined;
    }
    // строки 164-186: переразметка end-листа по числу strip-рёбер (индексы 0..depth ВКЛЮЧИТЕЛЬНО)
    for (i = 0; i <= depth; i++) {
        poly = *reinterpret_cast<RpTriStripPolygon**>(work[3] + i * 4);
        stripped = reinterpret_cast<RwInt32 (__cdecl*)(RpTriStripPolygon*)>(0x75BB50)(poly);
        node = &poly->inEndLink;
        if (node->next == nullptr) {
            if (stripped < 2) {
                // вставить в начало polygonEndList
                node->next = mesh->polygonEndList.link.next;
                node->prev = reinterpret_cast<RwLLLink*>(&mesh->polygonEndList);
                node->next->prev = node;
                mesh->polygonEndList.link.next = node;
            }
        } else if (2 < stripped) {
            // исключить из end-листа
            *reinterpret_cast<RwLLLink**>(node->prev) = node->next;
            node->next->prev = node->prev;
            node->prev = nullptr;
            node->next = nullptr;
        }
    }
    work[0] = 0;
    consumedCount++;
    goto nextPolygon;

restartPass:   // LAB_0075b9fb: end-лист пуст — проверка сходимости
    {
        RwUInt32 pass = work[1];
        work[1] = pass + 1;
        accum = accum * 0.5f + static_cast<RwReal>(consumedCount) / static_cast<RwReal>(mesh->numPolygons);
        consumedCount = 0;
        if ((accum < threshold) || (static_cast<RwUInt32>(tunnelData->lengthLimit) <= pass + 1)) {
            mem->rwfree(candidates);
            mem->rwfree(work);
            return mesh;
        }
    }
    polygon = reinterpret_cast<RpTriStripPolygon*>(mesh->polygonEndList.link.next);
    goto nextPolygon;
}


RpTriStripMesh* RpTriStripMeshQuick(RpTriStripMesh* mesh, void* data) {
    (void)data;
    RwLLLink classLists[4];   // appppuStack_20 — 4 сигнала-класса по числу соседей 0..3
    classLists[0].next = &classLists[0];
    classLists[0].prev = &classLists[0].next;
    classLists[1].next = &classLists[1];
    classLists[1].prev = &classLists[1].next;
    classLists[2].next = &classLists[2];
    classLists[2].prev = &classLists[2].next;
    classLists[3].next = &classLists[3];
    classLists[3].prev = &classLists[3].next;

    RwLLLink* node;
    RpTriStripPolygon* polygon;
    RpTriStripPolygon* neighbor;
    RpTriStripPolygon* nextNeighbor;
    RpTriStripPolygon* prevPoly;
    RpTriStripEdge* edge;
    RpTriStripEdge* nextEdge;
    RwUInt32 cls;
    RwUInt32 start;
    RwUInt32 len;
    RwUInt32 bestLen;
    RwUInt32 bestStart;
    RwUInt32 testFrame = 0;
    RwUInt32 v;
    RwUInt32 i;
    RwUInt32 j;

    // фаза 1 (строки 44-69): раскидать used-полигоны по классам через inFreeLink
    for (node = mesh->polygonUsedList.link.next; node != &mesh->polygonUsedList.link; node = node->next) {
        polygon = reinterpret_cast<RpTriStripPolygon*>(reinterpret_cast<RwUInt8*>(node) - 0x8);   // inUsedLink @ +0x8
        j = 0;
        for (i = 0; i < polygon->numEdges; i++) {
            edge = polygon->edges[i];
            if (((edge->poly1 != nullptr) && (edge->poly1 != polygon)) ||
                ((edge->poly2 != nullptr) && (edge->poly2 != polygon))) {
                j++;
            }
        }
        polygon->inFreeLink.prev = &classLists[j];
        polygon->inFreeLink.next = classLists[j].next;
        classLists[j].next->prev = &polygon->inFreeLink;
        classLists[j].next = &polygon->inFreeLink;
    }

    cls = 0;
    do {
        // фаза 2 (строки 73-130): первый полигон непустого класса, перебор 6 стартов
        while (classLists[cls].next == &classLists[cls]) {
            cls++;
            if (cls > 3) {
                return mesh;
            }
        }
        polygon = reinterpret_cast<RpTriStripPolygon*>(reinterpret_cast<RwUInt8*>(classLists[cls].next) - 0x10);  // inFreeLink @ +0x10
        bestLen = 0;
        bestStart = 0;
        for (start = 0; start < 6; start++) {
            len = 1;
            edge = polygon->edges[start % 3];
            neighbor = (polygon == edge->poly1) ? edge->poly2 : edge->poly1;
            if ((neighbor != nullptr) && (neighbor->inFreeLink.next != nullptr)) {
                v = (start < 3) ? edge->vert1 : edge->vert2;
                testFrame++;
                if (testFrame == 0) {
                    testFrame = 1;
                }
                polygon->testFrame = testFrame;
                for (;;) {
                    if (neighbor->testFrame == testFrame) {
                        break;
                    }
                    neighbor->testFrame = testFrame;
                    len++;
                    nextEdge = reinterpret_cast<RpTriStripEdge* (__cdecl*)(RpTriStripEdge*, RpTriStripPolygon*, RwUInt16, RwUInt32)>(0x75C290)(edge, neighbor, static_cast<RwUInt16>(v), testFrame);
                    if (nextEdge == nullptr) {
                        break;
                    }
                    v = ((edge->vert1 == nextEdge->vert1) || (edge->vert1 == nextEdge->vert2)) ? edge->vert1 : edge->vert2;
                    nextNeighbor = (neighbor == nextEdge->poly1) ? nextEdge->poly2 : nextEdge->poly1;
                    edge = nextEdge;
                    neighbor = nextNeighbor;
                    if (neighbor == nullptr) {
                        break;
                    }
                }
            }
            if (bestLen < len) {
                bestStart = start;
                bestLen = len;
            }
        }
        // исключить первый полигон из класс-листа (строки 131-134)
        node = &polygon->inFreeLink;
        *reinterpret_cast<RwLLLink**>(node->prev) = node->next;
        node->next->prev = node->prev;
        node->prev = nullptr;
        node->next = nullptr;
        // построить полосу от лучшего старта (строки 135-172)
        edge = polygon->edges[bestStart % 3];
        neighbor = (polygon == edge->poly1) ? edge->poly2 : edge->poly1;
        if ((neighbor != nullptr) && (neighbor->inFreeLink.next != nullptr)) {
            v = (bestStart < 3) ? edge->vert1 : edge->vert2;
            for (;;) {
                if (neighbor->testFrame == 0) {
                    break;
                }
                edge->strip = 1;                       // строка 149
                neighbor->testFrame = 0;               // строка 150
                node = &neighbor->inFreeLink;          // unlink из класс-листа (151-154)
                *reinterpret_cast<RwLLLink**>(node->prev) = node->next;
                node->next->prev = node->prev;
                node->prev = nullptr;
                node->next = nullptr;
                nextEdge = reinterpret_cast<RpTriStripEdge* (__cdecl*)(RpTriStripEdge*, RpTriStripPolygon*, RwUInt16, RwUInt32)>(0x75C290)(edge, neighbor, static_cast<RwUInt16>(v), 0);
                if (nextEdge == nullptr) {
                    break;
                }
                v = ((edge->vert1 == nextEdge->vert1) || (edge->vert1 == nextEdge->vert2)) ? edge->vert1 : edge->vert2;
                node = &neighbor->inEndLink;           // unlink из end-листа (162-165)
                *reinterpret_cast<RwLLLink**>(node->prev) = node->next;
                node->next->prev = node->prev;
                node->prev = nullptr;
                node->next = nullptr;
                nextNeighbor = (neighbor == nextEdge->poly1) ? nextEdge->poly2 : nextEdge->poly1;
                neighbor = nextNeighbor;
                edge = nextEdge;
                if (neighbor == nullptr) {
                    break;
                }
            }
        }
        // фаза 3 (строки 174-192): от первого found-ребра дойти до конца полосы (joined_r0x0075c043)
        prevPoly = polygon;
        nextNeighbor = nullptr;
        for (i = 0; i < polygon->numEdges; i++) {
            edge = polygon->edges[i];
            if ((edge->strip != 0) && (edge->poly1 != nullptr) && (edge->poly2 != nullptr)) {
                nextNeighbor = (polygon == edge->poly1) ? edge->poly2 : edge->poly1;
                break;
            }
        }
        while (nextNeighbor != nullptr) {
            prevPoly = polygon;
            polygon = nextNeighbor;
            nextNeighbor = nullptr;
            for (i = 0; i < polygon->numEdges; i++) {
                edge = polygon->edges[i];
                if ((edge->strip != 0) && (prevPoly != edge->poly1) && (prevPoly != edge->poly2)) {
                    nextNeighbor = (polygon == edge->poly1) ? edge->poly2 : edge->poly1;
                    break;
                }
            }
        }
        // LAB_0075c093 (строки 221-314): перелинковка соседей конечного полигона по классам
        for (;;) {
            for (i = 0; i < 3; i++) {
                edge = polygon->edges[i];
                neighbor = (polygon == edge->poly1) ? edge->poly2 : edge->poly1;
                if ((neighbor != nullptr) && (neighbor->inFreeLink.next != nullptr)) {
                    node = &neighbor->inFreeLink;      // unlink из класс-листа
                    *reinterpret_cast<RwLLLink**>(node->prev) = node->next;
                    node->next->prev = node->prev;
                    node->prev = nullptr;
                    node->next = nullptr;
                    // число соседей, ещё висящих в класс-листах (в декомпиле цикл повторён 4 раза)
                    j = 0;
                    for (RwUInt32 e = 0; e < neighbor->numEdges; e++) {
                        RpTriStripEdge* e2 = neighbor->edges[e];
                        if (((e2->poly1 != nullptr) && (e2->poly1 != neighbor) && (e2->poly1->inFreeLink.next != nullptr)) ||
                            ((e2->poly2 != nullptr) && (e2->poly2 != neighbor) && (e2->poly2->inFreeLink.next != nullptr))) {
                            j++;
                        }
                    }
                    neighbor->inFreeLink.next = classLists[j].next;
                    neighbor->inFreeLink.prev = &classLists[j];
                    classLists[j].next->prev = &neighbor->inFreeLink;
                    classLists[j].next = &neighbor->inFreeLink;
                }
            }
            // сканирование ветвей (строки 318-336): strip-ребро конечного полигона, не ведущее к prevPoly
            nextNeighbor = nullptr;
            for (i = 0; i < polygon->numEdges; i++) {
                edge = polygon->edges[i];
                if ((edge->strip != 0) && (prevPoly != edge->poly1) && (prevPoly != edge->poly2)) {
                    nextNeighbor = (polygon == edge->poly1) ? edge->poly2 : edge->poly1;
                    break;
                }
            }
            if (nextNeighbor == nullptr) {
                break;
            }
            prevPoly = polygon;
            polygon = nextNeighbor;
        }
        // строка 317: pppppppppuStack_30 == 0 → повтор фазы 2 со следующим классом
    } while (true);
    // недостижимо (выход только через cls > 3)
    return mesh;
}


RpMeshHeader* RpBuildMeshGenerateTriStrip(RpBuildMesh* buildMesh, void* data) {
    RpTriStripData* tsData = static_cast<RpTriStripData*>(data);
    RwGlobalsMemFuncsGta* mem = reinterpret_cast<RwGlobalsMemFuncsGta*>(RwEngineInstance);

    // отфильтровать вырожденные треугольники (строки 31-46)
    RpBuildMeshTriangle** tris = reinterpret_cast<RpBuildMeshTriangle**>(mem->rwmalloc(buildMesh->numTriangles << 2, 0x10502));
    RwUInt32 count = 0;
    RwUInt32 i;
    for (i = 0; i < buildMesh->numTriangles; i++) {
        RpBuildMeshTriangle* tri = reinterpret_cast<RpBuildMeshTriangle*>(reinterpret_cast<RwUInt8*>(buildMesh->meshTriangles) + i * 0x14);
        if ((tri->vertIndex[0] != tri->vertIndex[1]) && (tri->vertIndex[1] != tri->vertIndex[2]) && (tri->vertIndex[2] != tri->vertIndex[0])) {
            tris[count] = tri;
            count++;
        }
    }
    // сортировка по материалу (строка 48)
    reinterpret_cast<void (__cdecl*)(void*, RwUInt32, RwUInt32, int (__cdecl*)(const void*, const void*))>(0x8247E0)(
        tris, count, 4, reinterpret_cast<int (__cdecl*)(const void*, const void*)>(0x759640));
    // число групп одинакового материала (строки 49-60)
    RwUInt32 numStrips = 1;
    if (1 < count) {
        for (i = 1; i < count; i++) {
            if (tris[i - 1]->material != tris[i]->material) {
                numStrips++;
            }
        }
    }
    RwUInt32** strips = reinterpret_cast<RwUInt32**>(mem->rwmalloc(numStrips * 4, 0x10502));
    RwUInt32 numStrips2 = 0;
    RwUInt32 runStart = 0;
    if (1 < count) {
        for (i = 1; i < count; i++) {
            if (tris[i - 1]->material != tris[i]->material) {
                strips[numStrips2] = reinterpret_cast<RwUInt32* (__cdecl*)(RpBuildMeshTriangle**, RwUInt32)>(0x75C660)(tris + runStart, i - runStart);
                tsData->stripCB(strips[numStrips2], tsData->data);
                numStrips2++;
                runStart = i;
            }
        }
    }
    strips[numStrips2] = reinterpret_cast<RwUInt32* (__cdecl*)(RpBuildMeshTriangle**, RwUInt32)>(0x75C660)(tris + runStart, i - runStart);
    tsData->stripCB(strips[numStrips2], tsData->data);

    // итоговый RpMeshHeader (строки 80-116)
    RpMeshHeader* meshHeader = reinterpret_cast<RpMeshHeader*>(
        mem->rwmalloc((count + numStrips) * 0xC + 0x10, 0x30502));
    meshHeader->numMeshes = static_cast<RwUInt16>(numStrips);
    RpMesh* mesh = reinterpret_cast<RpMesh*>(meshHeader + 1);
    RwUInt16* indices = reinterpret_cast<RwUInt16*>(mesh + numStrips);
    RwUInt32 meshIdx = 0;
    meshHeader->flags = 1;               // rpMESHHEADERTRISTRIP
    meshHeader->serialNum = 0;
    meshHeader->totalIndicesInMesh = 0;
    meshHeader->firstMeshOffset = 0;
    if (1 < count) {
        for (i = 0; i < count - 1; i++) {
            if (tris[i]->material != tris[i + 1]->material) {
                mesh->indices = indices;
                mesh->material = tris[i]->material;
                reinterpret_cast<void (__cdecl*)(RwUInt32*, RpMesh*, RpBuildMesh*)>(0x75C9D0)(strips[meshIdx], mesh, buildMesh);
                indices += mesh->numIndices * 2;
                meshHeader->totalIndicesInMesh += mesh->numIndices;
                mesh++;
                meshIdx++;
            }
        }
    }
    mesh->indices = indices;
    // декомпил: piVar3[uVar5 - 1], где uVar5 = count если count > 1, иначе 1
    mesh->material = tris[(1 < count ? count : 1) - 1]->material;
    reinterpret_cast<void (__cdecl*)(RwUInt32*, RpMesh*, RpBuildMesh*)>(0x75C9D0)(strips[meshIdx], mesh, buildMesh);
    meshHeader->totalIndicesInMesh += mesh->numIndices;
    // усадить буфер до фактического размера и починить указатели indices после realloc (строки 117-126)
    meshHeader = reinterpret_cast<RpMeshHeader*>(
        mem->rwrealloc(meshHeader,
                       reinterpret_cast<RwUInt32>(indices) + mesh->numIndices * 2 - reinterpret_cast<RwUInt32>(meshHeader),
                       0x30502));
    mesh = reinterpret_cast<RpMesh*>(meshHeader + 1);
    indices = reinterpret_cast<RwUInt16*>(mesh + numStrips);
    for (i = 0; i < numStrips; i++) {
        mesh[i].indices = indices;
        indices += mesh[i].numIndices * 2;
    }
    // очистка (строки 127-135)
    for (i = 0; i < numStrips; i++) {
        mem->rwfree(strips[i]);
    }
    mem->rwfree(strips);
    mem->rwfree(tris);
    return meshHeader;
}


RpMeshHeader* RpBuildMeshGenerateTrivialTriStrip(RpBuildMesh* buildMesh, void* data) {
    (void)data;
    RwUInt32 numTriangles = buildMesh->numTriangles;
    RwUInt32 size = numTriangles * 0x12 + 0x10;
    RpMeshHeader* meshHeader = _rpMeshHeaderCreate(size);   // rwmalloc(size, 0x30502)
    RwUInt32* fill32 = reinterpret_cast<RwUInt32*>(meshHeader);
    RwUInt32 dwordCount = size >> 2;
    while (dwordCount != 0) {           // debug-fill 0xCD, как в декомпиле
        *fill32++ = 0xCDCDCDCD;
        dwordCount--;
    }
    RwUInt8* fill8 = reinterpret_cast<RwUInt8*>(fill32);
    RwUInt32 byteCount = size & 3;
    while (byteCount != 0) {
        *fill8++ = 0xCD;
        byteCount--;
    }
    meshHeader->numMeshes = static_cast<RwUInt16>(numTriangles);
    meshHeader->totalIndicesInMesh = numTriangles * 3;
    meshHeader->flags = 1;              // rpMESHHEADERTRISTRIP
    meshHeader->serialNum = 0;
    meshHeader->firstMeshOffset = 0;
    RpMesh* mesh = reinterpret_cast<RpMesh*>(meshHeader + 1);
    RxVertexIndex* out = reinterpret_cast<RxVertexIndex*>(mesh + 1);
    const RpBuildMeshTriangle* tri = buildMesh->meshTriangles;
    for (RwUInt32 i = numTriangles; i != 0; i--) {
        mesh->indices = out;
        mesh->numIndices = 3;
        mesh->material = tri->material;
        out[0] = tri->vertIndex[0];
        out[1] = tri->vertIndex[1];
        out[2] = tri->vertIndex[2];
        tri++;      // sizeof(RpBuildMeshTriangle) == 0x14
        mesh++;     // sizeof(RpMesh) == 0xC
        out += 3;
    }
    return meshHeader;
}


RpMeshHeader* RpBuildMeshGenerateDefaultTriStrip(RpBuildMesh* buildmesh, void* data) {
    (void)data;
    return reinterpret_cast<RpMeshHeader* (__cdecl*)(RpBuildMesh*, void*, RwUInt32)>(0x7591D0)(buildmesh, nullptr, 1);
}


RpMeshHeader* RpBuildMeshGeneratePreprocessTriStrip(RpBuildMesh* buildmesh, void* data) {
    (void)data;
    return reinterpret_cast<RpMeshHeader* (__cdecl*)(RpBuildMesh*, RwUInt32, RwUInt32)>(0x7591D0)(buildmesh, 1, 1);
}


RpMeshHeader* RpBuildMeshGenerateExhaustiveTriStrip(RpBuildMesh* buildmesh, void* data) {
    return reinterpret_cast<RpMeshHeader* (__cdecl*)(RpBuildMesh*, void*, RwUInt32)>(0x75A960)(buildmesh, data, 1);
}


RpMeshHeader* RpBuildMeshGenerateDefaultIgnoreWindingTriStrip(RpBuildMesh* buildmesh, void* data) {
    (void)data;
    return reinterpret_cast<RpMeshHeader* (__cdecl*)(RpBuildMesh*, void*, RwUInt32)>(0x7591D0)(buildmesh, nullptr, 0);
}


RpMeshHeader* RpBuildMeshGeneratePreprocessIgnoreWindingTriStrip(RpBuildMesh* buildmesh, void* data) {
    (void)data;
    return reinterpret_cast<RpMeshHeader* (__cdecl*)(RpBuildMesh*, RwUInt32, RwUInt32)>(0x7591D0)(buildmesh, 1, 0);
}


RpMeshHeader* RpBuildMeshGenerateExhaustiveIgnoreWindingTriStrip(RpBuildMesh* buildmesh, void* data) {
    return reinterpret_cast<RpMeshHeader* (__cdecl*)(RpBuildMesh*, void*, RwUInt32)>(0x75A960)(buildmesh, data, 0);
}


RwBool RpMeshSetTriStripMethod(RpTriStripMeshCallBack callback, void* data) {
    if (callback != nullptr) {
        rpMeshTriStripMethodGta = callback;        // PTR_FUN_008d6388
        rpMeshTriStripMethodDataGta = data;        // DAT_008d638c
        return TRUE;
    }
    rpMeshTriStripMethodGta = RpBuildMeshGenerateDefaultTriStrip;   // FUN_007591b0 — репо-обёртка
    rpMeshTriStripMethodDataGta = nullptr;
    return TRUE;
}


RwBool RpMeshGetTriStripMethod(RpTriStripMeshCallBack* callback, void* data) {
    if (callback != nullptr) {
        *callback = rpMeshTriStripMethodGta;
    }
    if (data != nullptr) {
        *static_cast<void**>(data) = rpMeshTriStripMethodDataGta;
    }
    return TRUE;
}


RpMeshHeader* _rpTriListMeshGenerate(RpBuildMesh* buildMesh, void* data) {
    (void)data;
    RwGlobalsMemFuncsGta* mem = reinterpret_cast<RwGlobalsMemFuncsGta*>(RwEngineInstance);

    RpBuildMeshTriangle** tris = reinterpret_cast<RpBuildMeshTriangle**>(mem->rwmalloc(buildMesh->numTriangles << 2, 0x10502));
    if (tris == nullptr) {
        RwError rwError;
        rwError.pluginID = 2;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, buildMesh->numTriangles << 2);
        RwErrorSet(&rwError);
        return nullptr;
    }
    for (RwUInt32 i = 0; i < buildMesh->numTriangles; i++) {
        tris[i] = reinterpret_cast<RpBuildMeshTriangle*>(reinterpret_cast<RwUInt8*>(buildMesh->meshTriangles) + i * 0x14);
    }
    reinterpret_cast<void (__cdecl*)(void*, RwUInt32, RwUInt32, int (__cdecl*)(const void*, const void*))>(0x8247E0)(
        tris, buildMesh->numTriangles, 4, reinterpret_cast<int (__cdecl*)(const void*, const void*)>(0x759640));
    // число групп по материалу (строки 40-54)
    RwUInt32 numTriangles = buildMesh->numTriangles;
    RwUInt32 numMeshes = 1;
    if (1 < numTriangles) {
        RpMaterial* mat = tris[0]->material;
        for (RwUInt32 i = 1; i < numTriangles; i++) {
            if (tris[i]->material != mat) {
                numMeshes++;
                mat = tris[i]->material;
            }
        }
    }
    RwUInt32 size = (numTriangles + numMeshes * 2) * 6 + 0x10;
    RpMeshHeader* meshHeader = _rpMeshHeaderCreate(size);
    if (meshHeader == nullptr) {
        mem->rwfree(tris);
        RwError rwError;
        rwError.pluginID = 2;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, size);
        RwErrorSet(&rwError);
        return nullptr;
    }
    meshHeader->flags = 0;               // трилист (нет примитив-флагов)
    meshHeader->numMeshes = 1;
    RwUInt16* serial = reinterpret_cast<RwUInt16*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + rpMeshGlobalsOffsetGta);
    meshHeader->firstMeshOffset = 0;
    meshHeader->serialNum = *serial;
    meshHeader->totalIndicesInMesh = numTriangles * 3;
    *serial = static_cast<RwUInt16>(*serial + 1);
    RpMesh* mesh = reinterpret_cast<RpMesh*>(meshHeader + 1);
    mesh->numIndices = 0;
    mesh->material = tris[0]->material;   // numTriangles==0: чтение мусора — как в оригинале
    RwUInt16* indices = reinterpret_cast<RwUInt16*>(mesh + numMeshes);
    mesh->indices = indices;
    RwUInt32 i = 0;
    do {
        if (tris[i]->material != mesh->material) {
            RpMesh* next = mesh + 1;
            next->numIndices = 0;
            next->material = tris[i]->material;
            next->indices = indices;
            meshHeader->numMeshes = static_cast<RwUInt16>(meshHeader->numMeshes + 1);
            mesh = next;
        }
        indices[0] = tris[i]->vertIndex[0];
        indices[1] = tris[i]->vertIndex[1];
        indices[2] = tris[i]->vertIndex[2];
        i++;
        indices += 3;
        mesh->numIndices += 3;
    } while (i < numTriangles);
    _rpMeshHeaderForAllMeshes(meshHeader, reinterpret_cast<RpMeshCallBack>(0x75D730), nullptr);
    mem->rwfree(tris);
    return meshHeader;
}


RpMeshHeader* _rpMeshOptimise(RpBuildMesh* buildmesh, RwUInt32 flags) {
    static auto& emptyMeshHeader      = StaticRef<RpMeshHeader>(0x8D6378);      // {.flags=0,.numMeshes=0,...}
    static auto& tristripCallback     = StaticRef<RpTriStripMeshCallBack>(0x8D6388);
    static auto& tristripCallbackData = StaticRef<void*>(0x8D638C);

    if (buildmesh != nullptr) {
        if (buildmesh->numTriangles == 0) {
            // No triangles: hand back the shared empty mesh header
            _rpBuildMeshDestroy(buildmesh);
            return &emptyMeshHeader;
        }

        RpTriStripMeshCallBack callback;
        void*                   data;
        if ((flags & rpMESHHEADERTRISTRIP /*0x0001*/) == 0) {
            callback = _rpTriListMeshGenerate;
            data     = nullptr;
        } else {
            callback = tristripCallback;
            data     = tristripCallbackData;
        }

        RpMeshHeader* header = callback(buildmesh, data);
        if (header != nullptr) {
            _rpBuildMeshDestroy(buildmesh);
            return header;
        }
    }
    return nullptr;
}


RpGeometry* RpGeometryTransform(RpGeometry* geometry, const RwMatrix* matrix)
{
    geometry->lockedSinceLastInst |= (rpGEOMETRYLOCKVERTICES | rpGEOMETRYLOCKNORMALS);   // byte +0xC |= 6

    if (geometry->numMorphTargets > 0) {
        for (RwInt32 mtIndex = 0; mtIndex < geometry->numMorphTargets; mtIndex++) {
            // RpMorphTarget: { parentGeom, boundingSphere(+0x04), verts(+0x14), normals(+0x18) }
            RpMorphTarget *mt = &geometry->morphTarget[mtIndex];

            if (mt->verts != NULL)
                RwV3dTransformPoints(mt->verts, mt->verts, geometry->numVertices, matrix);
            if (mt->normals != NULL) {
                RwV3dTransformVectors(mt->normals, mt->normals, geometry->numVertices, matrix);
                for (RwInt32 v = 0; v < geometry->numVertices; v++)
                    _rwV3dNormalize(&mt->normals[v], &mt->normals[v]);
            }
            RpMorphTargetCalcBoundingSphere(mt, &mt->boundingSphere);
        }
    }

    if (RpGeometryUnlock(geometry) == NULL) {
        RpGeometryDestroy(geometry);   // inlined in the decompile (refCount-- + destroy at 0); identical
                                       // body verified against 0x74CCC0's own decompile
        return NULL;
    }
    return geometry;
}


RpGeometry* RpGeometryCreateSpace(RwReal radius)
{
    RpGeometry *geometry = RpGeometryCreate(0, 0, 0);

    if (geometry != NULL) {
        RpMorphTarget *mt = geometry->morphTarget;   // geometry+0x5C, morph target #0
        mt->boundingSphere.center.x = 0.0f;          // +0x04
        mt->boundingSphere.center.y = 0.0f;          // +0x08
        mt->boundingSphere.center.z = 0.0f;          // +0x0C
        mt->boundingSphere.radius   = radius;        // +0x10
    }

    if (RpGeometryUnlock(geometry) == NULL) {
        RpGeometryDestroy(geometry);   // inlined in the decompile; identical to 0x74CCC0's body
        return NULL;
    }
    return geometry;
}


const RpMorphTarget* RpMorphTargetCalcBoundingSphere(const RpMorphTarget* morphTarget, RwSphere* boundingSphere) {
    float       maxDistSq = 0.0f;
    const RwInt32 numVerts = morphTarget->parentGeom->numVertices;   // *(int*)(*morphTarget + 0x14)

    // RwBBoxCalculate(&bbox, verts, numVerts): bbox = {sup @+0, inf @+12}
    RwBBox bbox;
    RwBBoxCalculate(&bbox, morphTarget->verts, numVerts);

    const float centerX = (bbox.sup.x + bbox.inf.x) * StaticRef<RwReal>(0x858B8C) /* 0.5f */;
    const float centerY = (bbox.sup.y + bbox.inf.y) * StaticRef<RwReal>(0x858B8C) /* 0.5f */;
    const float centerZ = (bbox.sup.z + bbox.inf.z) * StaticRef<RwReal>(0x858B8C) /* 0.5f */;

    if (numVerts != 0) {
        const RwV3d* vtx = morphTarget->verts;
        for (RwInt32 i = 0; i < numVerts; i++) {
            const float dx = vtx[i].x - centerX;
            const float dz = vtx[i].z - centerZ;
            const float dy = vtx[i].y - centerY;
            const float distSq = dz * dz + dx * dx + dy * dy;
            if (maxDistSq < distSq)
                maxDistSq = distSq;
        }
        if (StaticRef<RwReal>(0x858B50) /* 0.0f */ < maxDistSq)
            maxDistSq = _rwSqrt(maxDistSq);          // squared -> radius
    }

    boundingSphere->center.x = centerX;
    boundingSphere->center.y = centerY;
    boundingSphere->center.z = centerZ;
    // radius slack factor baked at 0x872490 (official RW uses 1.05; value not recoverable from decomp)
    boundingSphere->radius = maxDistSq * StaticRef<RwReal>(0x872490);
    return morphTarget;
}


RwInt32 RpGeometryAddMorphTargets(RpGeometry* geometry, RwInt32 mtcount) {
    // Function-local view of the GTA RwGlobals memory-function area (offsets per rwplcore wave-B):
    struct RwMemFuncsGta {
        RwUInt8  pad[0x134];
        void* (*rwmalloc)(size_t size, RwUInt32 hint);                    // +0x134
        void   (*rwfree)(void* mem);                                      // +0x138
        void* (*rwrealloc)(void* mem, size_t newSize, RwUInt32 hint);     // +0x13C
        void* (*rwcalloc)(size_t numObj, size_t sizeObj, RwUInt32 hint);  // +0x140
        void* (*memoryAlloc)(RwFreeList* fl, RwUInt32 hint);              // +0x144
        void   (*memoryFree)(RwFreeList* fl, void* mem);                  // +0x148
    };
    RwMemFuncsGta* mem = reinterpret_cast<RwMemFuncsGta*>(RwEngineInstance);

    // Extra per-morph-target bytes (vertex data trails the RpMorphTarget array contiguously)
    RwUInt32 mtDataSize;                                   // per-MT vertex data size
    RwUInt32 mtStride;                                     // header + data
    if ((geometry->flags & rpGEOMETRYNATIVE) == 0) {
        mtDataSize = geometry->numVertices * 12;           // numVertices * sizeof(RwV3d)
        mtStride   = mtDataSize + 0x1C;                    // + sizeof(RpMorphTarget)
        if ((geometry->flags & rpGEOMETRYNORMALS) != 0)    // (flags & 0x10)
            mtStride = mtStride + mtDataSize;
    } else {
        mtDataSize = 0;
        mtStride   = 0x1C;
    }

    RwUInt32 totalSize = (geometry->numMorphTargets + mtcount) * mtStride;
    RwUInt8* block;
    if (geometry->morphTarget == nullptr) {               // *(geometry + 0x5C) == 0
        block = static_cast<RwUInt8*>(mem->rwmalloc(totalSize, 0x3000F /* rwMEMHINTDUR_FUNCTION|rwID_GEOMETRY */));
        if (block == nullptr) {
            RwError rwError;
            rwError.pluginID  = 2;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, totalSize);  // _rwerror
            RwErrorSet(&rwError);
            return -1;
        }
    } else {
        // NOTE: decompiler shows a 1-arg indirect call at +0x13C, but the callee is RwGlobals.rwrealloc
        // (mem, newSize, hint) as established from rwplcore decompiles; size is required for the move below.
        block = static_cast<RwUInt8*>(mem->rwrealloc(geometry->morphTarget, totalSize, 0x3000F));
        if (block == nullptr) {
            RwError rwError;
            rwError.pluginID  = 2;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, totalSize);  // _rwerror
            RwErrorSet(&rwError);
            return -1;
        }
        // Move existing vertex data up by mtcount * sizeof(RpMorphTarget) bytes (backwards, byte-wise)
        RwUInt8* src = block + geometry->numMorphTargets * mtStride - 1;
        RwUInt8* dst = src + mtcount * 0x1C;
        for (RwUInt32 len = mtDataSize * geometry->numMorphTargets; len != 0; len--) {
            *--dst = *--src;
        }
    }

    const RwInt32 oldCount = geometry->numMorphTargets;
    geometry->numMorphTargets = oldCount + mtcount;        // +0x18
    geometry->morphTarget     = reinterpret_cast<RpMorphTarget*>(block);  // +0x5C

    // Lay out vertex/normals pointers for every morph target
    RwV3d* dataCursor = reinterpret_cast<RwV3d*>(block + (oldCount + mtcount) * 0x1C);
    for (RwInt32 i = 0; i < geometry->numMorphTargets; i++) {
        RpMorphTarget* mt = reinterpret_cast<RpMorphTarget*>(block + i * 0x1C);
        mt->verts   = nullptr;                             // +0x14
        mt->normals = nullptr;                             // +0x18
        if ((geometry->flags & rpGEOMETRYNATIVE) == 0 && geometry->numVertices != 0) {
            mt->verts = dataCursor;
            dataCursor += geometry->numVertices;
            if ((geometry->flags & rpGEOMETRYNORMALS) != 0) {
                mt->normals = dataCursor;
                dataCursor += geometry->numVertices;
            }
        }
    }

    // Zero bounding sphere + set parent on the newly added morph targets only
    for (RwInt32 idx = oldCount; idx < geometry->numMorphTargets; idx++) {
        RpMorphTarget* mt = &geometry->morphTarget[idx];
        mt->boundingSphere.center.x = 0.0f;                // +0x04
        mt->boundingSphere.center.y = 0.0f;                // +0x08
        mt->boundingSphere.center.z = 0.0f;                // +0x0C
        mt->boundingSphere.radius   = 0.0f;                // +0x10
        mt->parentGeom = geometry;                         // +0x00
    }
    return geometry->numMorphTargets - mtcount;            // = oldCount (decompile-faithful)
}


RwInt32 RpGeometryAddMorphTarget(RpGeometry* geometry) {
    return RpGeometryAddMorphTargets(geometry, 1);
}


RpGeometry* RpGeometryRemoveMorphTarget(RpGeometry* geometry, RwInt32 morphTarget) {
    struct RwMemFuncsGta {                                 // GTA RwGlobals memory-function area
        RwUInt8  pad[0x134];
        void* (*rwmalloc)(size_t size, RwUInt32 hint);                    // +0x134
        void   (*rwfree)(void* mem);                                      // +0x138
        void* (*rwrealloc)(void* mem, size_t newSize, RwUInt32 hint);     // +0x13C
        void* (*rwcalloc)(size_t numObj, size_t sizeObj, RwUInt32 hint);  // +0x140
        void* (*memoryAlloc)(RwFreeList* fl, RwUInt32 hint);              // +0x144
        void   (*memoryFree)(RwFreeList* fl, void* mem);                  // +0x148
    };
    RwMemFuncsGta* mem = reinterpret_cast<RwMemFuncsGta*>(RwEngineInstance);

    // Per-MT stride; NOTE: unlike AddMorphTargets there is NO rpGEOMETRYNATIVE check here (decompile-faithful)
    RwUInt32 mtDataSize = geometry->numVertices * 12;      // numVertices * sizeof(RwV3d)
    RwUInt32 mtStride   = mtDataSize + 0x1C;               // + sizeof(RpMorphTarget)
    if ((geometry->flags & 0x10 /*rpGEOMETRYNORMALS*/) != 0)
        mtStride = mtStride + mtDataSize;

    RwUInt32 totalSize = (geometry->numMorphTargets - 1) * mtStride;
    RwUInt8* newBlock = static_cast<RwUInt8*>(mem->rwmalloc(totalSize, 0x3000F /* FUNCTION|rwID_GEOMETRY */));
    if (newBlock == nullptr) {
        RwError rwError;
        rwError.pluginID  = 2;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, totalSize);  // _rwerror
        RwErrorSet(&rwError);
        return nullptr;
    }

    // Pass 1: copy RpMorphTarget headers (0x1C bytes = 7 dwords) of every target except [morphTarget]
    RwInt32 kept = 0;
    RwUInt32 srcOff = 0;
    for (RwInt32 idx = 0; idx < geometry->numMorphTargets; idx++, srcOff += 0x1C) {
        if (idx != morphTarget) {
            RwUInt32* src = reinterpret_cast<RwUInt32*>(reinterpret_cast<RwUInt8*>(geometry->morphTarget) + srcOff);
            RwUInt32* dst = reinterpret_cast<RwUInt32*>(newBlock + kept * 0x1C);
            for (RwInt32 d = 0; d < 7; d++)
                dst[d] = src[d];
            kept++;
        }
    }

    // Pass 2: copy the trailing vertex/normals data of every kept target
    {
        RwUInt32 dataSize = mtStride - 0x1C;
        RwUInt8* srcVtx = reinterpret_cast<RwUInt8*>(geometry->morphTarget) + geometry->numMorphTargets * 0x1C;
        RwUInt8* dstVtx = newBlock + kept * 0x1C;
        for (RwInt32 idx = 0; idx < geometry->numMorphTargets; idx++, srcVtx += dataSize) {
            if (idx != morphTarget) {
                for (RwUInt32 b = 0; b < dataSize; b++)
                    dstVtx[b] = srcVtx[b];
                dstVtx += dataSize;
            }
        }
    }

    mem->rwfree(geometry->morphTarget);
    geometry->numMorphTargets--;
    geometry->morphTarget = reinterpret_cast<RpMorphTarget*>(newBlock);

    // Pass 3: rebuild verts/normals pointers for ALL remaining morph targets
    RwV3d* dataCursor = reinterpret_cast<RwV3d*>(newBlock + geometry->numMorphTargets * 0x1C);
    for (RwInt32 i = 0; i < geometry->numMorphTargets; i++) {
        RpMorphTarget* mt = reinterpret_cast<RpMorphTarget*>(newBlock + i * 0x1C);
        mt->verts   = nullptr;
        mt->normals = nullptr;
        if (geometry->numVertices != 0) {
            mt->verts = dataCursor;
            dataCursor += geometry->numVertices;
            if ((geometry->flags & 0x10 /*rpGEOMETRYNORMALS*/) != 0) {
                mt->normals = dataCursor;
                dataCursor += geometry->numVertices;
            }
        }
    }
    return geometry;
}


const RpGeometry* RpGeometryTriangleSetVertexIndices(const RpGeometry* geometry, RpTriangle* triangle, RwUInt16 vert1, RwUInt16 vert2, RwUInt16 vert3) {
    triangle->vertIndex[0] = vert1;
    triangle->vertIndex[1] = vert2;
    triangle->vertIndex[2] = vert3;
    return geometry;
}


RpGeometry* RpGeometryTriangleSetMaterial(RpGeometry* geometry, RpTriangle* triangle, RpMaterial* material) {
    if (material == nullptr) {
        triangle->matIndex = 0xFFFF;
        return geometry;
    }
    RwInt32 matIndex = _rpMaterialListFindMaterialIndex(&geometry->matList, material);
    if (matIndex < 0) {
        matIndex = _rpMaterialListAppendMaterial(&geometry->matList, material);
        if (matIndex < 0)
            return nullptr;
    }
    triangle->matIndex = static_cast<RwUInt16>(matIndex);
    return geometry;
}


const RpGeometry* RpGeometryTriangleGetVertexIndices(const RpGeometry* geometry, const RpTriangle* triangle, RwUInt16* vert1, RwUInt16* vert2, RwUInt16* vert3) {
    if (vert1 != nullptr)
        *vert1 = triangle->vertIndex[0];
    if (vert2 != nullptr)
        *vert2 = triangle->vertIndex[1];
    if (vert3 != nullptr)
        *vert3 = triangle->vertIndex[2];
    return geometry;
}


RpMaterial* RpGeometryTriangleGetMaterial(const RpGeometry* geometry, const RpTriangle* triangle) {
    if (triangle->matIndex == 0xFFFF)
        return nullptr;
    return _rpMaterialListGetMaterial(&geometry->matList, triangle->matIndex);
}


RpGeometry* RpGeometryForAllMaterials(RpGeometry* geometry, RpMaterialCallBack fpCallBack, void* data) {
    for (RwInt32 i = 0; i < geometry->matList.numMaterials; i++) {
        if (fpCallBack(geometry->matList.materials[i], data) == nullptr)
            return geometry;
    }
    return geometry;
}


RpGeometry* RpGeometryLock(RpGeometry* geometry, RwInt32 lockMode) {
    geometry->lockedSinceLastInst |= static_cast<RwUInt16>(lockMode);   // u16 @ +0x0C
    if ((lockMode & rpGEOMETRYLOCKPOLYGONS /*0x01*/) != 0 && geometry->mesh != nullptr) {
        _rpMeshDestroy(geometry->mesh);
        geometry->mesh = nullptr;                                       // +0x54
    }
    return geometry;
}


RpGeometry* RpGeometryUnlock(RpGeometry* geometry) {
    struct RwMemFuncsGta {                                 // GTA RwGlobals memory-function area
        RwUInt8  pad[0x134];
        void* (*rwmalloc)(size_t size, RwUInt32 hint);                    // +0x134
        void   (*rwfree)(void* mem);                                      // +0x138
        void* (*rwrealloc)(void* mem, size_t newSize, RwUInt32 hint);     // +0x13C
        void* (*rwcalloc)(size_t numObj, size_t sizeObj, RwUInt32 hint);  // +0x140
        void* (*memoryAlloc)(RwFreeList* fl, RwUInt32 hint);              // +0x144
        void   (*memoryFree)(RwFreeList* fl, void* mem);                  // +0x148
    };
    RwMemFuncsGta* mem = reinterpret_cast<RwMemFuncsGta*>(RwEngineInstance);

    if (geometry->mesh != nullptr)                         // +0x54
        return geometry;

    RpBuildMesh* buildMesh = _rpBuildMeshCreate(geometry->numTriangles);
    if (buildMesh == nullptr)
        return nullptr;

    if (geometry->matList.numMaterials > 0) {
        // Dedup tables keyed by material ptr / texture ptr / raster ptr / pipeline ptr (raw hints from decompile)
        void** matTable  = static_cast<void**>(mem->rwmalloc(geometry->matList.numMaterials * 4, 0x10006));
        void** texTable  = static_cast<void**>(mem->rwmalloc(geometry->matList.numMaterials * 4, 0x10503));
        void** pipeTable = static_cast<void**>(mem->rwmalloc(geometry->matList.numMaterials * 4, 0x10503));
        RwUInt16 numMats = 0, numTex = 0, numPipes = 0;

        for (RwInt32 i = 0; i < geometry->numTriangles; i++) {
            const RpTriangle* tri = &geometry->triangles[i];
            RpMaterial* mat = _rpMaterialListGetMaterial(&geometry->matList, tri->matIndex);
            RwTexture*  tex = mat->texture;                // decompile derefs unconditionally

            RwUInt16 matIdxInTable, rasterIdxInTable, pipeIdxInTable;
            RwUInt16 j;

            for (j = 0; j < numMats; j++)
                if (matTable[j] == mat)
                    break;
            if (j == numMats)
                matTable[numMats++] = mat;
            matIdxInTable = j;

            // raster key = tex ? tex->raster : NULL
            void* raster = (tex != nullptr) ? static_cast<void*>(tex->raster) : nullptr;
            for (j = 0; j < numTex; j++)
                if (texTable[j] == raster)
                    break;
            if (j == numTex)
                texTable[numTex++] = raster;
            rasterIdxInTable = j;

            void* pipeline = mat->pipeline;                // third dword of RpMaterial
            for (j = 0; j < numPipes; j++)
                if (pipeTable[j] == pipeline)
                    break;
            if (j == numPipes)
                pipeTable[numPipes++] = pipeline;
            pipeIdxInTable = j;

            _rpBuildMeshAddTriangle(buildMesh, mat,
                                    tri->vertIndex[0], tri->vertIndex[1], tri->vertIndex[2],
                                    tri->matIndex, matIdxInTable, rasterIdxInTable, pipeIdxInTable);
        }

        mem->rwfree(matTable);
        mem->rwfree(texTable);
        mem->rwfree(pipeTable);
    }

    RpMeshHeader* meshHeader = _rpMeshOptimise(buildMesh, (geometry->flags & rpGEOMETRYTRISTRIP) != 0);
    if (meshHeader == nullptr) {
        _rpBuildMeshDestroy(buildMesh);
        return nullptr;
    }
    geometry->mesh = meshHeader;                           // +0x54
    return geometry;
}


const RpGeometry* RpGeometryForAllMeshes(const RpGeometry* geometry, RpMeshCallBack fpCallBack, void* data) {
    if (geometry->mesh != nullptr) {
        if (_rpMeshHeaderForAllMeshes(geometry->mesh, fpCallBack, data) != nullptr)
            return geometry;
    }
    return nullptr;
}


RpGeometry* RpGeometryCreate(RwInt32 numVert, RwInt32 numTriangles, RwUInt32 format) {
    struct RwMemFuncsGta {                                 // GTA RwGlobals memory-function area
        RwUInt8  pad[0x134];
        void* (*rwmalloc)(size_t size, RwUInt32 hint);                    // +0x134
        void   (*rwfree)(void* mem);                                      // +0x138
        void* (*rwrealloc)(void* mem, size_t newSize, RwUInt32 hint);     // +0x13C
        void* (*rwcalloc)(size_t numObj, size_t sizeObj, RwUInt32 hint);  // +0x140
        void* (*memoryAlloc)(RwFreeList* fl, RwUInt32 hint);              // +0x144
        void   (*memoryFree)(RwFreeList* fl, void* mem);                  // +0x148
    };
    RwMemFuncsGta* mem = reinterpret_cast<RwMemFuncsGta*>(RwEngineInstance);

    if (numVert < 0)
        return nullptr;
    if (numVert > 0xFFFF) {
        RwError rwError;
        rwError.pluginID  = 2;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(6);   // _rwerror
        RwErrorSet(&rwError);
        return nullptr;
    }
    if (numTriangles < 0)
        return nullptr;

    // Resolve texture-coordinate set count from the format specifier
    RwUInt32 numTexSets;
    if ((format & 0xFF0000) != 0)
        numTexSets = (format >> 16) & 0xFF;                // rpGEOMETRYTEXCOORDSETS(n)
    else if ((format & 0x80 /*rpGEOMETRYTEXTURED2*/) != 0)
        numTexSets = 2;
    else if ((format & rpGEOMETRYTEXTURED /*0x04*/) != 0)
        numTexSets = 1;
    else
        numTexSets = 0;

    RwUInt32 texSetFlag = (numTexSets == 1) ? rpGEOMETRYTEXTURED /*0x04*/
                        : (numTexSets > 1)  ? rpGEOMETRYTEXTURED2 /*0x80*/
                                            : 0;

    // Total allocation: plugin-room + (non-native) prelit + texcoords + triangles, all contiguous
    RwUInt32 native = format & rpGEOMETRYNATIVE /*0x01000000*/;
    RwUInt32 allocSize = geometryTKList.sizeOfStruct;      // DAT_008d628c (RwPluginRegistry.sizeOfStruct)
    if (native == 0) {
        if ((format & rpGEOMETRYPRELIT /*0x08*/) != 0)
            allocSize = allocSize + numVert * 4;
        if (numTexSets != 0)
            allocSize = allocSize + numTexSets * numVert * 8;
        allocSize = allocSize + numTriangles * 8;
    }

    RpGeometry* geometry = static_cast<RpGeometry*>(mem->rwmalloc(allocSize, 0x3000F /* FUNCTION|rwID_GEOMETRY */));
    if (geometry == nullptr || _rpMaterialListInitialize(&geometry->matList) == nullptr)
        return nullptr;

    // Field init (order as in decompile)
    geometry->morphTarget         = nullptr;               // +0x5C
    geometry->numMorphTargets     = 0;                     // +0x18
    geometry->object.object.parent       = nullptr;               // +0x04
    geometry->repEntry            = nullptr;               // +0x58
    geometry->lockedSinceLastInst = 0;                     // +0x0C
    geometry->mesh                = nullptr;               // +0x54
    geometry->object.object.type         = rpGEOMETRY /*8*/;      // +0x00
    geometry->object.object.subType      = 0;
    geometry->object.object.flags        = 0;
    geometry->object.object.privateFlags = 0;
    geometry->refCount            = 1;                     // +0x0E (u16)
    geometry->numTexCoordSets     = numTexSets;            // +0x1C
    for (RwInt32 i = 0; i < rwMAXTEXTURECOORDS /*8*/; i++)
        geometry->texCoords[i] = nullptr;                  // +0x34..+0x53
    geometry->preLitLum           = nullptr;               // +0x30
    geometry->triangles           = nullptr;               // +0x2C
    geometry->numTriangles        = numTriangles;          // +0x10
    geometry->flags               = (format & rpGEOMETRYNATIVEFLAGSMASK /*0x0F000000*/)
                                  | (format & 0x7B)        // TRISTRIP|POSITIONS|PRELIT|NORMALS|LIGHT|MODULATE
                                  | texSetFlag;            // +0x08
    geometry->numVertices         = numVert;               // +0x14

    // Carve out the contiguous data area after the struct (non-native only)
    if (native == 0) {
        RwUInt8* cursor = reinterpret_cast<RwUInt8*>(geometry) + geometryTKList.sizeOfStruct;
        if ((format & rpGEOMETRYPRELIT /*0x08*/) != 0 && numVert != 0) {
            geometry->preLitLum = reinterpret_cast<RwRGBA*>(cursor);
            cursor += numVert * 4;
        }
        if (numTexSets != 0 && numVert != 0) {
            for (RwUInt32 i = 0; i < numTexSets; i++) {
                geometry->texCoords[i] = reinterpret_cast<RwTexCoords*>(cursor);
                cursor += numVert * 8;
            }
        }
        if (numTriangles != 0) {
            geometry->triangles = reinterpret_cast<RpTriangle*>(cursor);
            for (RwInt32 i = 0; i < numTriangles; i++)
                geometry->triangles[i].matIndex = 0xFFFF;
        }
    }

    if (RpGeometryAddMorphTargets(geometry, 1) < 0) {
        _rpMaterialListDeinitialize(&geometry->matList);
        mem->rwfree(geometry);
        return nullptr;
    }
    _rwPluginRegistryInitObject(&geometryTKList, geometry);
    return geometry;
}


RwBool RpGeometryDestroy(RpGeometry* geometry) {
    struct RwMemFuncsGta {                                 // GTA RwGlobals memory-function area
        RwUInt8  pad[0x134];
        void* (*rwmalloc)(size_t size, RwUInt32 hint);                    // +0x134
        void   (*rwfree)(void* mem);                                      // +0x138
        void* (*rwrealloc)(void* mem, size_t newSize, RwUInt32 hint);     // +0x13C
        void* (*rwcalloc)(size_t numObj, size_t sizeObj, RwUInt32 hint);  // +0x140
        void* (*memoryAlloc)(RwFreeList* fl, RwUInt32 hint);              // +0x144
        void   (*memoryFree)(RwFreeList* fl, void* mem);                  // +0x148
    };
    RwMemFuncsGta* mem = reinterpret_cast<RwMemFuncsGta*>(RwEngineInstance);

    const RwInt16 refCount = geometry->refCount;           // i16 @ +0x0E
    if (refCount == 1 || refCount - 1 < 0) {
        if (geometry->repEntry != nullptr) {               // +0x58
            RwResourcesFreeResEntry(geometry->repEntry);
        }
        geometry->lockedSinceLastInst |= 0xFFF;            // u16 @ +0x0C (decompile: direct OR, not RpGeometryLock)
        // decompile has a self-assignment no-op on refCount here (omitted)
        if (geometry->mesh != nullptr) {                   // +0x54
            _rpMeshDestroy(geometry->mesh);
            geometry->mesh = nullptr;
        }
        _rwPluginRegistryDeInitObject(&geometryTKList, geometry);
        if (geometry->morphTarget != nullptr) {            // +0x5C
            mem->rwfree(geometry->morphTarget);
            geometry->morphTarget = nullptr;
        }
        _rpMaterialListDeinitialize(&geometry->matList);
        geometry->refCount--;
        mem->rwfree(geometry);
        return TRUE;
    }
    geometry->refCount--;
    return TRUE;
}


RwInt32 RpGeometryRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return _rwPluginRegistryAddPlugin(&geometryTKList, size, pluginID, constructCB, destructCB, copyCB);
}


RwInt32 RpGeometryRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return _rwPluginRegistryAddPluginStream(&geometryTKList, pluginID, readCB, writeCB, getSizeCB);
}


RwInt32 RpGeometrySetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB) {
    return _rwPluginRegistryAddPlgnStrmlwysCB(&geometryTKList, pluginID, alwaysCB);
}


RwInt32 RpGeometryGetPluginOffset(RwUInt32 pluginID) {
    return _rwPluginRegistryGetPluginOffset(&geometryTKList, pluginID);
}


RwBool RpGeometryValidatePlugins(const RpGeometry* geometry) {
    (void)geometry;
    return TRUE;   // decompile: constant 1
}


RwUInt32 RpGeometryStreamGetSize(const RpGeometry* geometry) {
    // Internal body-size helper @0x74CE60 (outside vendor floor, not in rw_addr_map)
    RwUInt32 geomBodySize = ((RwUInt32(__cdecl*)(const RpGeometry*))0x74CE60)(geometry);
    RwUInt32 matListSize  = _rpMaterialListStreamGetSize(&geometry->matList);
    RwUInt32 pluginSize   = _rwPluginRegistryGetSize(&geometryTKList, geometry);
    return pluginSize + geomBodySize + 0x24 + matListSize;
}


const RpGeometry* RpGeometryStreamWrite(const RpGeometry* geometry, RwStream* stream) {
    // Internal body-size helper @0x74CE60 (outside vendor floor, not in rw_addr_map).
    // NOTE: decompile shows a 3-arg indirect call at one site (0x36003, 0xffff) - Ghidra trailing-arg
    // artifact; the callee is the same 1-arg helper used by RpGeometryStreamGetSize.
    RwUInt32 geomBodySize = ((RwUInt32(__cdecl*)(const RpGeometry*))0x74CE60)(geometry);
    RwUInt32 matListSize  = _rpMaterialListStreamGetSize(&geometry->matList);
    RwUInt32 pluginSize   = _rwPluginRegistryGetSize(&geometryTKList, geometry);

    if (_rwStreamWriteVersionedChunkHeader(stream, rwID_GEOMETRY /*0xF*/,
                                           geomBodySize + matListSize + 0x24 + pluginSize,
                                           0x36003, 0xFFFF) == nullptr)
        return nullptr;

    if (_rwStreamWriteVersionedChunkHeader(stream, rwID_STRUCT /*1*/,
                                           ((RwUInt32(__cdecl*)(const RpGeometry*))0x74CE60)(geometry),
                                           0x36003, 0xFFFF) == nullptr)
        return nullptr;

    {   // RpGeometryChunkInfo {format, numTriangles, numVertices, numMorphTargets}
        RpGeometryChunkInfo chunkInfo;
        chunkInfo.numVertices     = geometry->numVertices;
        chunkInfo.format          = ((geometry->numTexCoordSets & 0xFF) << 16) | geometry->flags;
        chunkInfo.numTriangles    = geometry->numTriangles;
        chunkInfo.numMorphTargets = geometry->numMorphTargets;
        if (RwStreamWrite(stream, &chunkInfo, 0x10) == nullptr)
            return nullptr;
    }

    if ((geometry->flags & rpGEOMETRYNATIVE) == 0 && geometry->numVertices != 0) {
        if ((geometry->flags & rpGEOMETRYPRELIT /*0x08*/) != 0 &&
            RwStreamWrite(stream, geometry->preLitLum, geometry->numVertices * 4) == nullptr)
            return nullptr;

        if (geometry->numTexCoordSets > 0) {
            for (RwInt32 i = 0; i < geometry->numTexCoordSets; i++) {
                if (RwStreamWriteReal(stream, reinterpret_cast<const RwReal*>(geometry->texCoords[i]),
                                      geometry->numVertices << 3) == nullptr)
                    return nullptr;
            }
        }

        for (RwInt32 i = 0; i < geometry->numTriangles; i++) {
            const RpTriangle* tri = &geometry->triangles[i];
            // Disk word order {v1,v0,matIndex,v2} (little-endian pairs of dwords);
            // mirrors the u16-swap done by RpGeometryStreamRead and librw's streamRead.
            RwUInt32 triBuf[2] = {
                (static_cast<RwUInt32>(tri->vertIndex[0]) << 16) | tri->vertIndex[1],
                (static_cast<RwUInt32>(tri->vertIndex[2]) << 16) | tri->matIndex
            };
            if (RwStreamWrite(stream, triBuf, 8) == nullptr)
                return nullptr;
        }
    }

    for (RwInt32 i = 0; i < geometry->numMorphTargets; i++) {
        const RpMorphTarget* mt = &geometry->morphTarget[i];
        // On-disk morph target header: {RwSphere (16 bytes), hasVerts, hasNormals} = 0x18 bytes
        struct MtChunk {
            RwV3d    center;
            RwReal   radius;
            RwUInt32 hasVerts;
            RwUInt32 hasNormals;
        } mtChunk;
        mtChunk.center = mt->boundingSphere.center;
        mtChunk.radius = mt->boundingSphere.radius;
        if ((geometry->flags & rpGEOMETRYNATIVE) == 0) {
            mtChunk.hasVerts   = (mt->verts != nullptr) ? 1u : 0u;
            mtChunk.hasNormals = (mt->normals != nullptr) ? 1u : 0u;
        } else {
            mtChunk.hasVerts   = 0;
            mtChunk.hasNormals = 0;
        }
        if (RwStreamWrite(stream, &mtChunk, 0x18) == nullptr)
            return nullptr;
        if (mtChunk.hasVerts != 0 &&
            RwStreamWriteReal(stream, reinterpret_cast<const RwReal*>(mt->verts),
                              geometry->numVertices * 12) == nullptr)
            return nullptr;
        if (mtChunk.hasNormals != 0 &&
            RwStreamWriteReal(stream, reinterpret_cast<const RwReal*>(mt->normals),
                              geometry->numVertices * 12) == nullptr)
            return nullptr;
    }

    if (_rpMaterialListStreamWrite(&geometry->matList, stream) == nullptr)
        return nullptr;
    if (_rwPluginRegistryWriteDataChunks(&geometryTKList, stream, geometry) == nullptr)
        return nullptr;
    return geometry;
}


RpGeometry* RpGeometryStreamRead(RwStream* stream) {
    struct RwMemFuncsGta {                                 // GTA RwGlobals memory-function area
        RwUInt8  pad[0x134];
        void* (*rwmalloc)(size_t size, RwUInt32 hint);                    // +0x134
        void   (*rwfree)(void* mem);                                      // +0x138
        void* (*rwrealloc)(void* mem, size_t newSize, RwUInt32 hint);     // +0x13C
        void* (*rwcalloc)(size_t numObj, size_t sizeObj, RwUInt32 hint);  // +0x140
        void* (*memoryAlloc)(RwFreeList* fl, RwUInt32 hint);              // +0x144
        void   (*memoryFree)(RwFreeList* fl, void* mem);                  // +0x148
    };
    RwMemFuncsGta* mem = reinterpret_cast<RwMemFuncsGta*>(RwEngineInstance);

    RwUInt32 version;
    if (RwStreamFindChunk(stream, rwID_STRUCT /*1*/, nullptr, &version) == 0)
        return nullptr;

    if (version < 0x34000 || 0x36003 < version) {
        RwError rwError;
        rwError.pluginID  = 2;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000004);   // _rwerror
        RwErrorSet(&rwError);
        return nullptr;
    }

    RwSurfaceProperties oldSurfProps;                      // only read for version < 0x34001
    RpGeometryChunkInfo chunkInfo;
    if (version < 0x34001) {
        if (RwStreamRead(stream, &chunkInfo, 0x10) != 0x10)
            return nullptr;
        if (RwStreamRead(stream, &oldSurfProps, 0xC) != 0xC)
            return nullptr;
    } else {
        if (RwStreamRead(stream, &chunkInfo, 0x10) != 0x10)
            return nullptr;
    }

    RpGeometry* geometry = RpGeometryCreate(chunkInfo.numVertices, chunkInfo.numTriangles, chunkInfo.format);
    if (geometry == nullptr)
        return nullptr;

    if (1 < chunkInfo.numMorphTargets &&
        RpGeometryAddMorphTargets(geometry, chunkInfo.numMorphTargets - 1) < 0) {
        goto failDestroyA;                                 // decompile LAB_0074d522
    }

    if ((geometry->flags & rpGEOMETRYNATIVE) == 0 && geometry->numVertices != 0) {
        if ((chunkInfo.format & rpGEOMETRYPRELIT /*0x08*/) != 0) {
            RwUInt32 numBytes = geometry->numVertices * 4;
            if (RwStreamRead(stream, geometry->preLitLum, numBytes) != static_cast<RwUInt32>(numBytes))
                goto failDestroyA;
        }
        if (geometry->numTexCoordSets > 0) {
            RwUInt32 numBytes = geometry->numVertices << 3;
            for (RwInt32 i = 0; i < geometry->numTexCoordSets; i++) {
                if (RwStreamReadReal(stream, reinterpret_cast<RwReal*>(geometry->texCoords[i]), numBytes) == nullptr)
                    goto failDestroyA;
            }
        }
        if (geometry->numTriangles != 0) {
            RwUInt32 numBytes = geometry->numTriangles * 8;
            if (RwStreamRead(stream, geometry->triangles, numBytes) != numBytes)
                goto failDestroyA;
            // Disk {v1,v0,matIndex,v2} -> memory {v0,v1,v2,matIndex}: swap u16 pairs inside each triangle
            RwUInt16* tri = reinterpret_cast<RwUInt16*>(geometry->triangles);
            for (RwInt32 i = 0; i < geometry->numTriangles; i++, tri += 4) {
                const RwUInt16 t0 = tri[0];
                tri[0] = tri[1];
                tri[1] = t0;
                const RwUInt16 t2 = tri[2];
                tri[2] = tri[3];
                tri[3] = t2;
            }
        }
    }

    for (RwInt32 i = 0; i < geometry->numMorphTargets; i++) {
        // On-disk morph target header: {RwSphere, hasVerts, hasNormals} = 0x18 bytes
        struct MtChunk {
            RwV3d    center;
            RwReal   radius;
            RwUInt32 hasVerts;
            RwUInt32 hasNormals;
        } mtChunk;
        RpMorphTarget* mt = &geometry->morphTarget[i];
        if (RwStreamRead(stream, &mtChunk, 0x18) != 0x18)
            goto failDestroyA;
        mt->boundingSphere.center = mtChunk.center;
        mt->boundingSphere.radius = mtChunk.radius;
        if (mtChunk.hasVerts == 0) {
            if (mtChunk.hasNormals != 0 &&
                RwStreamReadReal(stream, reinterpret_cast<RwReal*>(mt->normals),
                                 geometry->numVertices * 12) == nullptr)
                goto failDestroyA;
        } else if (mtChunk.hasNormals == 0) {
            if (RwStreamReadReal(stream, reinterpret_cast<RwReal*>(mt->verts),
                                 geometry->numVertices * 12) == nullptr)
                goto failDestroyA;
        } else {
            // verts+normals are contiguous: single read of numVertices * 24 bytes
            if (RwStreamReadReal(stream, reinterpret_cast<RwReal*>(mt->verts),
                                 geometry->numVertices * 24) == nullptr)
                goto failDestroyA;
        }
    }

    {
        RwUInt32 matListVersion;
        if (RwStreamFindChunk(stream, rwID_MATLIST /*8*/, nullptr, &matListVersion) == 0)
            return nullptr;
        if (matListVersion < 0x34000 || 0x36003 < matListVersion) {
            // decompile: destroy + RwErrorSet(2, _rwerror(0x80000004))
            RwInt16 refCount = geometry->refCount;
            if (refCount == 1 || refCount - 1 < 0) {
                if (geometry->repEntry != nullptr)
                    RwResourcesFreeResEntry(geometry->repEntry);
                RpGeometryLock(geometry, 0xFFF);
                _rwPluginRegistryDeInitObject(&geometryTKList, geometry);
                if (geometry->morphTarget != nullptr) {
                    mem->rwfree(geometry->morphTarget);
                    geometry->morphTarget = nullptr;
                }
                _rpMaterialListDeinitialize(&geometry->matList);
                geometry->refCount--;
                mem->rwfree(geometry);
            } else {
                geometry->refCount--;
            }
            RwError rwError;
            rwError.pluginID  = 2;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000004);   // _rwerror
            RwErrorSet(&rwError);
            return nullptr;
        }
        if (matListVersion < 0x34001)
            _rpMaterialSetDefaultSurfaceProperties(&oldSurfProps);

        if (_rpMaterialListStreamRead(stream, &geometry->matList) == nullptr)
            goto failDestroyC;
        if (matListVersion < 0x34001)
            _rpMaterialSetDefaultSurfaceProperties(nullptr);

        if (_rwPluginRegistryReadDataChunks(&geometryTKList, stream, geometry) != nullptr) {
            if (RpGeometryUnlock(geometry) != nullptr)
                return geometry;
            goto failDestroyC;
        }
        goto failDestroyC;
    }

failDestroyA:                                              // decompile LAB_0074d522 / LAB_0074d3ac
    {
        RwInt16 refCount = geometry->refCount;
        if (refCount == 1 || refCount - 1 < 0) {
            if (geometry->repEntry != nullptr)
                RwResourcesFreeResEntry(geometry->repEntry);
            RpGeometryLock(geometry, 0xFFF);
            _rwPluginRegistryDeInitObject(&geometryTKList, geometry);
            if (geometry->morphTarget != nullptr) {
                mem->rwfree(geometry->morphTarget);
                geometry->morphTarget = nullptr;
            }
            _rpMaterialListDeinitialize(&geometry->matList);
            geometry->refCount--;
            mem->rwfree(geometry);
            return nullptr;
        }
        geometry->refCount--;                              // decompile LAB_0074d5e7
        return nullptr;
    }

failDestroyC:                                              // decompile LAB_0074d5b4 (post-matlist failures)
    {
        RwInt16 refCount = geometry->refCount;
        if (refCount == 1 || refCount - 1 < 0) {
            if (geometry->repEntry != nullptr)
                RwResourcesFreeResEntry(geometry->repEntry);
            geometry->refCount--;
            ((void(__cdecl*)(RpGeometry*))0x74D6D0)(geometry);   // internal lighter destroy (not in rw_addr_map)
            return nullptr;
        }
        geometry->refCount--;
        return nullptr;
    }
}


RpGeometryChunkInfo* _rpGeometryChunkInfoRead(RwStream* stream, RpGeometryChunkInfo* geometryChunkInfo, RwInt32* bytesRead) {
    RwStream* savedStream = stream;
    RwUInt32 length;
    RwUInt32 version;

    if (RwStreamFindChunk(stream, rwID_STRUCT /*1*/, &length, &version) == 0)
        return nullptr;

    geometryChunkInfo->format          = 0;
    geometryChunkInfo->numTriangles    = 0;
    geometryChunkInfo->numVertices     = 0;
    geometryChunkInfo->numMorphTargets = 0;

    if (RwStreamRead(savedStream, geometryChunkInfo, 0x10) != 0x10)
        return nullptr;

    RwStreamSkip(savedStream, length - 0x10);              // skip any extra bytes beyond the 16 read
    if (bytesRead != nullptr)
        *bytesRead = length + 0xC;                         // STRUCT chunk header (12) + length
    return geometryChunkInfo;
}


RpWorldSector* RpWorldSectorRender(RpWorldSector* worldSector) {
    RpWorld* world = reinterpret_cast<RpWorld*>(RwEngineInstance->curWorld);
    return (reinterpret_cast<RpWorldSector*(__cdecl*)(RpWorldSector*)>(world->renderCallBack))(worldSector);
}


const RpWorldSector* RpWorldSectorForAllMeshes(const RpWorldSector* sector, RpMeshCallBack fpCallBack, void* data) {
    if (sector->mesh != nullptr) {
        if (_rpMeshHeaderForAllMeshes(sector->mesh, fpCallBack, data) != nullptr)
            return sector;
    }
    return nullptr;
}


RwInt32 RpWorldSectorRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB,
                                    RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    static auto& worldSectorTKList = StaticRef<RwPluginRegistry>(0x8D736C);
    // .data image bytes confirm the registry: { sizeOfStruct=0x88, origSizeOfStruct=0x88, ... } (RpWorldSector = 0x88)
    return _rwPluginRegistryAddPlugin(&worldSectorTKList, size, pluginID, constructCB, destructCB, copyCB);
}


RwInt32 RpWorldSectorRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB,
                                          RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB) {
    static auto& worldSectorTKList = StaticRef<RwPluginRegistry>(0x8D736C);
    return _rwPluginRegistryAddPluginStream(&worldSectorTKList, pluginID, readCB, writeCB, getSizeCB);
}


RwInt32 RpWorldSectorSetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB) {
    return _rwPluginRegistryAddPlgnStrmlwysCB(&worldSectorTKList, pluginID, alwaysCB);
}


RwInt32 RpWorldSectorSetStreamRightsCallBack(RwUInt32 pluginID, RwPluginDataChunkRightsCallBack rightsCB) {
    return _rwPluginRegistryAddPlgnStrmRightsCB(&worldSectorTKList, pluginID, rightsCB);
}


RwInt32 RpWorldSectorGetPluginOffset(RwUInt32 pluginID) {
    return _rwPluginRegistryGetPluginOffset(&worldSectorTKList, pluginID);
}


RwBool RpWorldSectorValidatePlugins(const RpWorldSector* sector) {
    (void)sector;
    return TRUE;
}


void RpLightSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    static auto& gRpLightFreeListBlockSize           = StaticRef<RwInt32>(0x8D6310);
    static auto& gRpLightFreeListNumBlocksToPrealloc = StaticRef<RwInt32>(0x8D6314);
    gRpLightFreeListBlockSize           = blockSize;            // DAT_008d6310
    gRpLightFreeListNumBlocksToPrealloc = numBlocksToPrealloc;  // DAT_008d6314
}


RpLight* RpLightCreate(RwInt32 type) {
    static auto& lightTKList                = StaticRef<RwPluginRegistry>(0x8D62F8);
    static auto& lightFreeListGlobalsOffset = StaticRef<RwInt32>(0xC925D8);
    RwGlobals*  engine        = RwEngineInstance;
    RwFreeList* lightFreeList = *(RwFreeList**)((RwUInt8*)engine + lightFreeListGlobalsOffset);
    // engine+0x144 = RwMemoryAllocFn (RwFreeList-shaped), hint 0x30012 — как в rpworld-1/0x74A290 (clump 0x30010)
    RpLight* light = (RpLight*)(*(void*(__cdecl**)(RwFreeList*, RwUInt32))((RwUInt8*)engine + 0x144))(lightFreeList, 0x30012);
    if (light == nullptr) {
        return nullptr;
    }
    light->object.object.type         = rpLIGHT;              // 3 (decompile: *light = 3; повторно ниже не пишется)
    light->object.object.subType      = (RwUInt8)type;        // light[1] = type
    light->color.red                  = 1.0f;                 // 0x3f800000 @ +0x18
    light->color.green                = 1.0f;                 // 0x3f800000 @ +0x1C
    light->color.blue                 = 1.0f;                 // 0x3f800000 @ +0x20
    light->color.alpha                = 1.0f;                 // 0x3f800000 @ +0x24
    light->object.object.flags        = 0;                    // light[2] = 0 (потом перезапишется 3)
    light->object.object.parent       = nullptr;              // *(light+4) = 0
    light->object.sync                = (RwObjectHasFrameSyncFunction)0x7521A0;  // internal light sync (LAB_7521A0, вне этажа)
    light->radius                     = 0.0f;                 // +0x14 = 0
    light->minusCosAngle              = 0.0f;                 // +0x28 = 0
    light->object.object.privateFlags = 1;                    // light[3] = 1 (белый свет -> NOCHROMA)
    rwLinkListInitialize(&light->WorldSectorsInLight);        // self-loop @ +0x2C/+0x30
    rwLLLinkInitialize(&light->inWorld);                      // next/prev = NULL @ +0x34/+0x38
    // RwGlobalsSA.lightFrame @ engine+0xA (см. rwplcore.cpp:53) — свет помечается протухшим
    RwUInt16 engineLightFrame = *(RwUInt16*)((RwUInt8*)engine + 0xA);
    light->object.object.flags = rpLIGHTLIGHTATOMICS | rpLIGHTLIGHTWORLD;   // light[2] = 3
    light->lightFrame          = (RwUInt16)(engineLightFrame - 1);          // +0x3C
    _rwPluginRegistryInitObject(&lightTKList, light);         // FUN_008086E0 (rwplcore.cpp:1079)
    return light;
}


RwBool RpLightDestroy(RpLight* light) {
    static auto& lightTKList                = StaticRef<RwPluginRegistry>(0x8D62F8);
    static auto& lightFreeListGlobalsOffset = StaticRef<RwInt32>(0xC925D8);
    RwGlobals*  engine        = RwEngineInstance;
    // световой freelist живёт в блоках модульных глобалов; паттерн тот же, что в RpClumpCreate/Destroy
    RwFreeList* lightFreeList = *(RwFreeList**)((RwUInt8*)engine + lightFreeListGlobalsOffset);

    _rwPluginRegistryDeInitObject(&lightTKList, light);        // FUN_00808740 (rwplcore.cpp:1094)
    _rwObjectHasFrameReleaseFrame(light);                      // FUN_00804F40 (rwcore.cpp:966)
    // engine+0x148 = RwMemoryFreeFn (RwFreeList-shaped) — как в rpworld-1/0x74A310
    (*(void(__cdecl**)(void*, void*))((RwUInt8*)engine + 0x148))(lightFreeList, light);
    return TRUE;
}


RpLight* RpLightSetRadius(RpLight* light, RwReal radius)
{
    light->radius = radius;
    if (light->object.object.parent != NULL) {
        RwFrameUpdateObjects((RwFrame*)light->object.object.parent);
    }
    return light;
}


RpLight* RpLightSetColor(RpLight* light, const RwRGBAReal* color)
{
    light->color = *color;

    // privateFlags bit0 = monochrome flag: set when r == g == b (decompile compares the stored values)
    if (light->color.red == light->color.green && light->color.red == light->color.blue) {
        light->object.object.privateFlags = 1;
    } else {
        light->object.object.privateFlags = 0;
    }
    return light;
}


RwReal RpLightGetConeAngle(const RpLight* light)
{
    RwReal   x      = -light->minusCosAngle;
    RwUInt32 xBits;
    memcpy(&xBits, &x, sizeof(xBits));
    RwUInt32 axBits = xBits & 0x7FFFFFFF;   // ABS

    if (axBits > 0x3F7FFFFF) {              // |x| >= 1.0
        if ((RwInt32)xBits > 0) {           // x > 0
            return 0.0f /*0x858B50*/;
        }
        return 3.14159274f /*0x858CB8 = pi*/;
    }

    if (axBits > 0x3EFFFFFF) {              // |x| >= 0.5
        RwReal z;
        RwReal s;
        if ((RwInt32)xBits < 0) {           // x < 0
            z = (x + 1.0f /*0x858624*/) * 0.5f /*0x858B8C*/;
            s = _rwSqrt(z);
            {
                double zd = (double)z;
                double sd = (double)s;
                double P  = ((((((zd * 3.479330916889012e-05f /*0x874E90*/ + 7.915350178384632e-04f /*0x874E8C*/) * zd
                                 - 4.005555361509323e-02f /*0x874E88*/) * zd
                                + 2.012125253677368e-01f /*0x874E84*/) * zd
                               - 3.255658149719238e-01f /*0x874E80*/) * zd
                              + 1.666666716337204e-01f /*0x85F0A0 = 1/6*/) * zd);
                double Q  = (((((zd * 7.703815400600433e-02f /*0x874E7C*/ - 6.882839798927307e-01f /*0x874E78*/) * zd
                                + 2.020945787429810e+00f /*0x874E74*/) * zd
                               - 2.403394937515259e+00f /*0x874E70*/) * zd
                              + 1.0f /*0x858624*/));
                double t  = (P / Q) * sd - 7.549789415861596e-08f /*0x874E6C = pi/2_lo*/;
                t = t + sd;
                return (RwReal)(3.141592502593994f /*0x874E64 = pi_hi*/ - (t + t));
            }
        } else {                            // x >= 0
            z = (1.0f /*0x858624*/ - x) * 0.5f /*0x858B8C*/;
            s = _rwSqrt(z);
            {
                // df = s with the low 12 mantissa bits cleared ((uint)(float)s & 0xFFFFF000)
                RwReal   sf = s;
                RwUInt32 b;
                memcpy(&b, &sf, sizeof(b));
                b &= 0xFFFFF000;
                RwReal df;
                memcpy(&df, &b, sizeof(df));

                double zd = (double)z;
                double sd = (double)s;
                double P  = ((((((zd * 3.479330916889012e-05f /*0x874E90*/ + 7.915350178384632e-04f /*0x874E8C*/) * zd
                                 - 4.005555361509323e-02f /*0x874E88*/) * zd
                                + 2.012125253677368e-01f /*0x874E84*/) * zd
                               - 3.255658149719238e-01f /*0x874E80*/) * zd
                              + 1.666666716337204e-01f /*0x85F0A0 = 1/6*/) * zd);
                double Q  = (((((zd * 7.703815400600433e-02f /*0x874E7C*/ - 6.882839798927307e-01f /*0x874E78*/) * zd
                                + 2.020945787429810e+00f /*0x874E74*/) * zd
                               - 2.403394937515259e+00f /*0x874E70*/) * zd
                              + 1.0f /*0x858624*/));
                double r  = (zd - (double)df * (double)df) / ((double)df + sd) + (P / Q) * sd + (double)df;
                return (RwReal)(r + r);
            }
        }
    }

    if (axBits < 0x23000001) {              // |x| < 2^-57
        return 1.57079637f /*0x858FE4 = pi/2*/;
    }

    {
        double x2 = (double)x * (double)x;
        double P  = ((((((3.479330916889012e-05f /*0x874E90*/ * x2 + 7.915350178384632e-04f /*0x874E8C*/) * x2
                          - 4.005555361509323e-02f /*0x874E88*/) * x2
                         + 2.012125253677368e-01f /*0x874E84*/) * x2
                        - 3.255658149719238e-01f /*0x874E80*/) * x2
                       + 1.666666716337204e-01f /*0x85F0A0 = 1/6*/) * x2);
        double Q  = (((((x2 * 7.703815400600433e-02f /*0x874E7C*/ - 6.882839798927307e-01f /*0x874E78*/) * x2
                        + 2.020945787429810e+00f /*0x874E74*/) * x2
                       - 2.403394937515259e+00f /*0x874E70*/) * x2
                      + 1.0f /*0x858624*/));
        return (RwReal)(1.570796251296997f /*0x874E68 = pi/2_hi*/
                        - ((double)x - (7.549789415861596e-08f /*0x874E6C = pi/2_lo*/ - (P / Q) * (double)x)));
    }
}


RpLight* RpLightSetConeAngle(RpLight* ight, RwReal angle) {
    // DAT_00858B50 = 0.0f (rpLIGHTMINCONEANGLE), _DAT_00858FE4 = 1.57079637f
    // (rpLIGHTMAXCONEANGLE = rwPIOVER2 для D3D9-сборки; .rdata проверена: 0x4577E4)
    if (0.0f /*0x858B50*/ <= angle && angle <= 1.57079637f /*0x858FE4 = rwPIOVER2*/) {
        // декомпил: x87 FCOS с float10-промежуточным -> CRT cos() в double
        ight->minusCosAngle = -(RwReal)cos((double)angle);
        return ight;
    }
    return nullptr;
}


RwUInt32 RpLightStreamGetSize(const RpLight* light) {
    static auto& lightTKList = StaticRef<RwPluginRegistry>(0x8D62F8);
    // декомпил: return _rwPluginRegistryGetSize(&DAT_008d62f8, light) + 0x30;
    // 0x30 = 12 (заголовок чанка LIGHT) + 12 (заголовок STRUCT) + 24 (RpLightChunkInfo)
    return (RwUInt32)(_rwPluginRegistryGetSize(&lightTKList, light) + 0x30);
}


RpLight* RpLightStreamRead(RwStream* stream) {
    static auto& lightTKList = StaticRef<RwPluginRegistry>(0x8D62F8);
    RwUInt32 length;
    RwUInt32 version;

    // декомпил: FUN_007ed2d0(stream, 1 /*rwID_STRUCT*/, &length, &version)
    if (RwStreamFindChunk(stream, rwID_STRUCT /*1*/, &length, &version) == 0)
        return nullptr;

    if (version < 0x34000 || 0x36003 < version) {
        RwError rwError;
        rwError.pluginID  = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000004);   // _rwerror
        RwErrorSet(&rwError);
        return nullptr;
    }

    // декомпил читает ЦЕЛИКОМ length байт (обычно 24 = sizeof(RpLightChunkInfo)) и
    // сравнивает с length; 6 dword'ов предварительно обнулены
    RpLightChunkInfo lightChunkInfo;
    memset(&lightChunkInfo, 0, sizeof(lightChunkInfo));
    if (RwStreamRead(stream, &lightChunkInfo, length) != length)
        return nullptr;

    // type = биты 16..23 typeAndFlags (decompile: local_2; librw: buf.type_flags>>16)
    RpLight* light = RpLightCreate((RwUInt8)(lightChunkInfo.typeAndFlags >> 16));
    if (light == nullptr)
        return nullptr;

    light->radius       = lightChunkInfo.radius;
    light->color.red    = lightChunkInfo.red;
    light->color.green  = lightChunkInfo.green;
    light->color.blue   = lightChunkInfo.blue;

    if (version < 0x30300) {
        // старый формат: в minusCosAngle лежал tan(coneAngle); перевод tan -> -cos
        // (decompile: -sqrt(0x858624 / (a*a + 0x858624)); .rdata 0x858624 = 1.0f)
        light->minusCosAngle = -(RwReal)_rwSqrt(1.0f /*0x858624*/
                                                / (lightChunkInfo.minusCosAngle * lightChunkInfo.minusCosAngle + 1.0f /*0x858624*/));
    } else {
        light->minusCosAngle = lightChunkInfo.minusCosAngle;
    }

    // grey light -> rpLIGHTPRIVATENOCHROMA (decompile: *(u8*)(light+3) = 1/0)
    light->object.object.privateFlags =
        (light->color.red == light->color.green && light->color.green == light->color.blue)
            ? rpLIGHTPRIVATENOCHROMA
            : 0;
    // flags = младший байт typeAndFlags (decompile: *(u8*)(light+2) = local_4; librw: object.flags = (uint8)type_flags)
    light->object.object.flags = (RwUInt8)lightChunkInfo.typeAndFlags;

    // декомпил: return -(uint)(ok != 0) & light; — при ошибке плагин-данных свет НЕ уничтожается
    // (утечка воспроизведена как в оригинале; librw делает destroy — расхождение с librw)
    if (_rwPluginRegistryReadDataChunks(&lightTKList, stream, light) != nullptr)
        return light;
    return nullptr;
}


RpLightChunkInfo* _rpLightChunkInfoRead(RwStream* stream, RpLightChunkInfo* lightChunkInfo, RwInt32* bytesRead) {
    RwStream* savedStream = stream;
    RwUInt32  length;

    if (RwStreamFindChunk(stream, rwID_STRUCT /*1*/, &length, nullptr) == 0)
        return nullptr;

    // декомпил: 6 dword'ов (весь RpLightChunkInfo, 0x18 байт) обнуляются
    lightChunkInfo->radius        = 0.0f;
    lightChunkInfo->red           = 0.0f;
    lightChunkInfo->green         = 0.0f;
    lightChunkInfo->blue          = 0.0f;
    lightChunkInfo->minusCosAngle = 0.0f;
    lightChunkInfo->typeAndFlags  = 0;

    if (RwStreamRead(savedStream, lightChunkInfo, 0x18) != 0x18)
        return nullptr;

    *bytesRead = (RwInt32)(length + 0xC);                  // STRUCT-заголовок (12) + length
    RwStreamSkip(savedStream, length - 0x18);              // лишние байты сверх 24 прочитанных
    return lightChunkInfo;
}


RwInt32 RpLightRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    static auto& lightTKList = StaticRef<RwPluginRegistry>(0x8D62F8);
    // декомпил: FUN_008084a0(&DAT_008d62f8, size, pluginID, constructCB, destructCB, copyCB)
    // -> _rwPluginRegistryAddPlugin (реализация уже в репо, rwplcore.cpp:988)
    return _rwPluginRegistryAddPlugin(&lightTKList, size, pluginID, constructCB, destructCB, copyCB);
}


RwInt32 RpLightRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB) {
    static auto& lightTKList = StaticRef<RwPluginRegistry>(0x8D62F8);
    // декомпил: FUN_008088e0(&DAT_008d62f8, pluginID, readCB, writeCB, getSizeCB)
    // -> _rwPluginRegistryAddPluginStream (реализация уже в репо, rwplcore.cpp:1108)
    return _rwPluginRegistryAddPluginStream(&lightTKList, pluginID, readCB, writeCB, getSizeCB);
}


RwInt32 RpLightSetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB) {
    static auto& lightTKList = StaticRef<RwPluginRegistry>(0x8D62F8);
    // декомпил: FUN_00808920(&DAT_008d62f8, pluginID, alwaysCB)
    // -> _rwPluginRegistryAddPlgnStrmlwysCB (реализация уже в репо, rwplcore.cpp:1121)
    return _rwPluginRegistryAddPlgnStrmlwysCB(&lightTKList, pluginID, alwaysCB);
}


RwInt32 RpLightGetPluginOffset(RwUInt32 pluginID) {
    static auto& lightTKList = StaticRef<RwPluginRegistry>(0x8D62F8);
    // декомпил: FUN_00808470(&DAT_008d62f8, pluginID)
    // -> _rwPluginRegistryGetPluginOffset (реализация уже в репо, rwplcore.cpp:1070)
    return _rwPluginRegistryGetPluginOffset(&lightTKList, pluginID);
}


RwBool RpLightValidatePlugins(const RpLight* light) {
    // декомпил 0x751E00: return 1; (сирота-заглушка вендора, light не читается)
    (void)light;
    return TRUE;
}


void RpD3D9LightSetAttenuationParams(RpLight* light, const RpD3D9AttenuationParams* params)
{
    // RpLight D3D9-чанк (смещение rwD3D9RpWorldGlobals.lightPluginOffset == DAT_00c926b0):
    //   { RwInt32 d3dLightIndex; RpD3D9AttenuationParams attenuation; } — параметры лежат с +0x04
    RpD3D9AttenuationParams* dst =
        (RpD3D9AttenuationParams*)((RwUInt8*)light + rwD3D9RpWorldGlobals.lightPluginOffset + 4);

    dst->constant  = params->constant;    // декомпил: копия 3 dword'ов
    dst->linear    = params->linear;
    dst->quadratic = params->quadratic;
}


void RpD3D9LightGetAttenuationParams(const RpLight* light, RpD3D9AttenuationParams* params)
{
    const RpD3D9AttenuationParams* src =
        (const RpD3D9AttenuationParams*)((RwUInt8*)light + rwD3D9RpWorldGlobals.lightPluginOffset + 4);

    params->constant  = src->constant;
    params->linear    = src->linear;
    params->quadratic = src->quadratic;
}


RwBool _rwD3D9LightsOpen(void)
{
    auto& g = rwD3D9RpWorldGlobals;

    // --- освободить и обнулить индексный стейт (3 блока: free-стек, current-массив, prev-массив)
    if (g.freeIndices != NULL) {
        RwFree(g.freeIndices);            // декомпил: (**(code**)(DAT_00c97b24 + 0x138))() = RwFree
        g.freeIndices = NULL;
    }
    g.freeIndexCount = 0;
    g.field_BC       = 0;
    g.nextLightIndex = 0;

    if (g.curIndices != NULL) {
        RwFree(g.curIndices);
        g.curIndices = NULL;
    }
    g.curCount    = 0;
    g.curCapacity = 0;

    if (g.prevIndices != NULL) {
        RwFree(g.prevIndices);
        g.prevIndices = NULL;
    }
    g.prevCount    = 0;
    g.prevCapacity = 0;

    // --- лимит D3D-светов: D3DCAPS9.MaxLights (caps+0xA0)
    g.maxLights = *(RwInt32*)((RwUInt8*)RwD3D9GetCaps() + 0xA0);   // RwD3D9GetCaps = 0x7FAD20
    if (g.maxLights == 0) {
        g.maxLights = -1;
    }

    // --- насыщенный ambient-аккумулятор (0x8E2418; в RenderWare.h уже есть AmbientSaturated)
    AmbientSaturated.red   = 0.0f;
    AmbientSaturated.green = 0.0f;
    AmbientSaturated.blue  = 0.0f;
    AmbientSaturated.alpha = 1.0f;    // 0x3F800000

    // --- направленный шаблон D3DLIGHT9 (0xC92648)
    g.directionalLightTemplate.Type         = D3DLIGHT_DIRECTIONAL;   // 3
    g.directionalLightTemplate.Diffuse      = D3DCOLORVALUE{0.0f, 0.0f, 0.0f, 1.0f};
    g.directionalLightTemplate.Specular     = D3DCOLORVALUE{0.0f, 0.0f, 0.0f, 1.0f};
    g.directionalLightTemplate.Ambient      = D3DCOLORVALUE{0.0f, 0.0f, 0.0f, 1.0f};
    g.directionalLightTemplate.Position     = D3DVECTOR{0.0f, 0.0f, 0.0f};
    g.directionalLightTemplate.Direction    = D3DVECTOR{0.0f, 0.0f, 0.0f};
    g.directionalLightTemplate.Range        = 0.0f;
    g.directionalLightTemplate.Falloff      = 0.0f;
    g.directionalLightTemplate.Attenuation0 = 0.0f;
    g.directionalLightTemplate.Attenuation1 = 0.0f;
    g.directionalLightTemplate.Attenuation2 = 0.0f;
    g.directionalLightTemplate.Theta        = 0.0f;
    g.directionalLightTemplate.Phi          = 0.0f;

    // --- локальный шаблон D3DLIGHT9 (0xC925E0): реинициализируются ТОЛЬКО
    //     цветовые четвёрки (alpha=1) и Falloff=1 (0x3F800000)
    g.localLightTemplate.Diffuse  = D3DCOLORVALUE{0.0f, 0.0f, 0.0f, 1.0f};   // +0x04
    g.localLightTemplate.Specular = D3DCOLORVALUE{0.0f, 0.0f, 0.0f, 1.0f};   // +0x14
    g.localLightTemplate.Ambient  = D3DCOLORVALUE{0.0f, 0.0f, 0.0f, 1.0f};   // +0x24
    g.localLightTemplate.Falloff  = 1.0f;                                    // +0x50 (0xC92630)

    return TRUE;
}


RwBool _rwD3D9LightsGlobalEnable(RpLightFlag flags)
{
    auto& g = rwD3D9RpWorldGlobals;

    // сброс насыщенного ambient-аккумулятора (0x8E2418 = {0,0,0,1})
    AmbientSaturated.red   = 0.0f;
    AmbientSaturated.green = 0.0f;
    AmbientSaturated.blue  = 0.0f;
    AmbientSaturated.alpha = 1.0f;

    // RwGlobals.curWorld @ RwEngineInstance+0x04; мировые направленные свети: world->directionalLightList
    // (голова списка @RpWorld+0x3C — подтверждено декомпилом и офсетами RpWorld)
    RpWorld* world = (RpWorld*)RwEngineInstance->curWorld;
    RwBool   foundAny = FALSE;

    RwLLLink* head = &world->directionalLightList.link;
    for (RwLLLink* link = head->next; link != head; link = link->next) {
        RpLight* light = rwLLLinkGetData(link, RpLight, inWorld);   // link - 0x34
        if (light != NULL && (flags & light->object.object.flags) != 0) {
            if (light->object.object.subType == rpLIGHTDIRECTIONAL /* 1 */) {
                RwInt32* lightIndex = (RwInt32*)((RwUInt8*)light + g.lightPluginOffset);
                if (*lightIndex < 0) {
                    // выдать D3D-индекс: из fresh-счётчика либо из стека освобождённых
                    if (g.freeIndexCount == 0) {
                        *lightIndex = g.nextLightIndex;
                        g.nextLightIndex++;
                    } else {
                        g.freeIndexCount--;
                        *lightIndex = g.freeIndices[g.freeIndexCount];
                    }
                }
                g.directionalLightTemplate.Diffuse.r = light->color.red;     // light+0x18
                g.directionalLightTemplate.Diffuse.g = light->color.green;   // light+0x1C
                g.directionalLightTemplate.Diffuse.b = light->color.blue;    // light+0x20
                RwMatrix* ltm = RwFrameGetLTM((RwFrame*)light->object.object.parent);  // light+0x04
                g.directionalLightTemplate.Direction.x = ltm->at.x;          // LTM+0x20
                g.directionalLightTemplate.Direction.y = ltm->at.y;
                g.directionalLightTemplate.Direction.z = ltm->at.z;
                RwD3D9SetLight(*lightIndex, &g.directionalLightTemplate);

                // --- добавить индекс в curIndices (инлайн 0x756070/0x756260/0x756390, ёмкость +8)
                {
                    RwInt32 oldCapacity = g.curCapacity;
                    if (g.curCapacity <= g.curCount) {
                        g.curCapacity += 8;
                        if (g.curIndices == NULL) {
                            g.curIndices = (RwInt32*)RwMalloc(g.curCapacity * 4, 0x103050d);
                        } else {
                            g.curIndices = (RwInt32*)RwRealloc(g.curIndices, g.curCapacity * 4, 0x103050d);
                        }
                        for (RwInt32 i = oldCapacity; i < g.curCapacity; i++) {
                            g.curIndices[i] = 0;
                        }
                    }
                    g.curIndices[g.curCount] = *lightIndex;
                    g.curCount++;
                }
            } else {
                // ambient/NA-свети копятся в насыщенный аккумулятор
                AmbientSaturated.red   += light->color.red;
                AmbientSaturated.green += light->color.green;
                AmbientSaturated.blue  += light->color.blue;
            }
            foundAny = TRUE;
        }
    }

    if (foundAny) {
        RwD3D9SetRenderState(D3DRS_AMBIENT /* 0x8B */, 0xFFFFFFFF);
        return TRUE;
    }
    RwD3D9SetRenderState(D3DRS_AMBIENT /* 0x8B */, 0);
    return FALSE;
}


RwBool _rwD3D9LightDirectionalEnable(RpLight* light)
{
    auto& g = rwD3D9RpWorldGlobals;

    RwInt32* lightIndex = (RwInt32*)((RwUInt8*)light + g.lightPluginOffset);
    if (*lightIndex < 0) {
        if (g.freeIndexCount == 0) {
            *lightIndex = g.nextLightIndex;
            g.nextLightIndex++;
        } else {
            g.freeIndexCount--;
            *lightIndex = g.freeIndices[g.freeIndexCount];
        }
    }

    g.directionalLightTemplate.Diffuse.r = light->color.red;     // light+0x18
    g.directionalLightTemplate.Diffuse.g = light->color.green;   // light+0x1C
    g.directionalLightTemplate.Diffuse.b = light->color.blue;    // light+0x20
    RwMatrix* ltm = RwFrameGetLTM((RwFrame*)light->object.object.parent);  // light+0x04
    g.directionalLightTemplate.Direction.x = ltm->at.x;          // LTM+0x20
    g.directionalLightTemplate.Direction.y = ltm->at.y;
    g.directionalLightTemplate.Direction.z = ltm->at.z;
    RwD3D9SetLight(*lightIndex, &g.directionalLightTemplate);

    // --- добавить индекс в curIndices (та же инлайн-вставка, что в 0x756070/0x756390)
    {
        RwInt32 oldCapacity = g.curCapacity;
        if (g.curCapacity <= g.curCount) {
            g.curCapacity += 8;
            if (g.curIndices == NULL) {
                g.curIndices = (RwInt32*)RwMalloc(g.curCapacity * 4, 0x103050d);
            } else {
                g.curIndices = (RwInt32*)RwRealloc(g.curIndices, g.curCapacity * 4, 0x103050d);
            }
            for (RwInt32 i = oldCapacity; i < g.curCapacity; i++) {
                g.curIndices[i] = 0;
            }
        }
        g.curIndices[g.curCount] = *lightIndex;
        g.curCount++;
    }
    return TRUE;
}


RwBool _rwD3D9LightLocalEnable(RpLight* light)
{
    auto& g = rwD3D9RpWorldGlobals;

    RwInt32* lightIndex = (RwInt32*)((RwUInt8*)light + g.lightPluginOffset);
    if (*lightIndex < 0) {
        if (g.freeIndexCount == 0) {
            *lightIndex = g.nextLightIndex;
            g.nextLightIndex++;
        } else {
            g.freeIndexCount--;
            *lightIndex = g.freeIndices[g.freeIndexCount];
        }
    }

    g.localLightTemplate.Diffuse.r = light->color.red;     // light+0x18 -> шаблон +0x04
    g.localLightTemplate.Diffuse.g = light->color.green;   // light+0x1C
    g.localLightTemplate.Diffuse.b = light->color.blue;    // light+0x20
    RwMatrix* ltm = RwFrameGetLTM((RwFrame*)light->object.object.parent);  // light+0x04
    g.localLightTemplate.Position.x = ltm->pos.x;          // LTM+0x30 -> шаблон Position (+0x34)
    g.localLightTemplate.Position.y = ltm->pos.y;
    g.localLightTemplate.Position.z = ltm->pos.z;
    g.localLightTemplate.Range = light->radius;            // light+0x14 -> шаблон Range (+0x4C)
    if (g.localLightTemplate.Range != 0.0f /*0x858B50*/) {
        // аттенюация из D3D9-чанка света ({RwInt32 index; RpD3D9AttenuationParams}, +0x04):
        // constant, linear/radius, quadratic/radius^2
        const RpD3D9AttenuationParams* att = (const RpD3D9AttenuationParams*)(lightIndex + 1);
        g.localLightTemplate.Attenuation0 = att->constant;                          // +0x54
        g.localLightTemplate.Attenuation1 = att->linear / g.localLightTemplate.Range;
        g.localLightTemplate.Attenuation2 =
            att->quadratic / (g.localLightTemplate.Range * g.localLightTemplate.Range);

        switch (light->object.object.subType) {                // light+0x01, знаковый char в декомпиле
        case (RwUInt8)rpLIGHTPOINT /* 0x80 */:
            g.localLightTemplate.Type      = D3DLIGHT_POINT;   // 1
            g.localLightTemplate.Direction = D3DVECTOR{0.0f, 0.0f, 0.0f};
            g.localLightTemplate.Theta     = 0.0f;
            g.localLightTemplate.Phi       = 0.0f;
            break;
        case (RwUInt8)rpLIGHTSPOT /* 0x81 */: {
            g.localLightTemplate.Type         = D3DLIGHT_SPOT; // 2
            g.localLightTemplate.Direction.x  = ltm->at.x;     // LTM+0x20
            g.localLightTemplate.Direction.y  = ltm->at.y;
            g.localLightTemplate.Direction.z  = ltm->at.z;
            RwReal coneAngle = RpLightGetConeAngle(light) * 2.0f;    // 0x751AE0
            if (coneAngle >= 3.14159274f /*rwPI, 0x858CB8*/) {
                g.localLightTemplate.Theta = coneAngle - 0.001f /*0x858CDC*/;
            } else {
                g.localLightTemplate.Theta = coneAngle;
            }
            g.localLightTemplate.Phi = coneAngle;
            break;
        }
        case (RwUInt8)rpLIGHTSPOTSOFT /* 0x82 */:
            g.localLightTemplate.Type         = D3DLIGHT_SPOT; // 2
            g.localLightTemplate.Direction.x  = ltm->at.x;
            g.localLightTemplate.Direction.y  = ltm->at.y;
            g.localLightTemplate.Direction.z  = ltm->at.z;
            g.localLightTemplate.Theta        = 0.0f;
            g.localLightTemplate.Phi          = RpLightGetConeAngle(light) * 2.0f;
            break;
        default:
            return FALSE;
        }

        RwD3D9SetLight(*lightIndex, &g.localLightTemplate);

        // --- добавить индекс в curIndices (та же инлайн-вставка, что в 0x756070/0x756260)
        {
            RwInt32 oldCapacity = g.curCapacity;
            if (g.curCapacity <= g.curCount) {
                g.curCapacity += 8;
                if (g.curIndices == NULL) {
                    g.curIndices = (RwInt32*)RwMalloc(g.curCapacity * 4, 0x103050d);
                } else {
                    g.curIndices = (RwInt32*)RwRealloc(g.curIndices, g.curCapacity * 4, 0x103050d);
                }
                for (RwInt32 i = oldCapacity; i < g.curCapacity; i++) {
                    g.curIndices[i] = 0;
                }
            }
            g.curIndices[g.curCount] = *lightIndex;
            g.curCount++;
        }
        return TRUE;
    }
    return FALSE;
}


void _rwD3D9LightsEnable(RwBool enable, RwUInt32 type)
{
    (void)type;   // декомпил: второй аргумент телом не читается (ABI-заметка в STATUS.md)
    auto& g = rwD3D9RpWorldGlobals;
    RwInt32 state;

    if (enable == FALSE) {
        state = 0;
    } else {
        if (g.maxLights < g.curCount) {
            g.curCount = g.maxLights;
        }
        RwInt32 count = g.curCount;
        // выключить свети прошлого кадра, которых нет в текущем
        if (g.prevCount != 0) {
            for (RwInt32 i = 0; i < g.prevCount; i++) {
                RwInt32 prevIdx = g.prevIndices[i];
                RwBool  disable = TRUE;
                RwInt32 j = 0;
                if (count != 0) {
                    do {
                        if (g.curIndices[j] == prevIdx) {
                            if (j < count) {
                                disable = FALSE;   // ещё активен — не трогать
                            }
                            break;                 // найден за пределами count -> выключить
                        }
                        j++;
                    } while (j < count);
                }
                if (disable) {
                    RwD3D9EnableLight(prevIdx, FALSE);
                    count = g.curCount;            // декомпил: перечитка после каждого вызова
                }
            }
        }
        if (count == 0) {
            g.prevCount = 0;
        } else {
            // включить свети текущего кадра
            RwInt32 newPrevCount = 0;
            for (RwInt32 j = 0; j < g.curCount; j++) {
                RwD3D9EnableLight(g.curIndices[j], TRUE);
                newPrevCount = g.curCount;
            }
            // обменять cur/prev массивы (указатели + ёмкости + счётчики)
            RwInt32* tmpArray   = g.prevIndices;
            RwInt32  tmpCapacity = g.prevCapacity;
            g.prevIndices = g.curIndices;
            g.curIndices  = tmpArray;
            g.prevCapacity = g.curCapacity;
            g.curCapacity  = tmpCapacity;
            g.prevCount    = newPrevCount;
        }
        g.curCount = 0;
        state = 1;
    }

    if (rwD3D9LightingEnabledState != state) {
        rwD3D9LightingEnabledState = state;
        if (rwD3D9LightingForceResend == 0) {
            rwD3D9LightingForceResend = 1;
            rwD3D9ChangedRenderStates[rwD3D9ChangedRenderStateCount] = D3DRS_LIGHTING;  // 0x89
            rwD3D9ChangedRenderStateCount++;
        }
    }
}


void _rwD3D9LightsClose(void)
{
    auto& g = rwD3D9RpWorldGlobals;

    if (g.freeIndices != NULL) {
        RwFree(g.freeIndices);
        g.freeIndices = NULL;
    }
    g.freeIndexCount = 0;
    g.field_BC       = 0;
    g.nextLightIndex = 0;

    if (g.curIndices != NULL) {
        RwFree(g.curIndices);
        g.curIndices = NULL;
    }
    g.curCount    = 0;
    g.curCapacity = 0;

    if (g.prevIndices != NULL) {
        RwFree(g.prevIndices);
        g.prevIndices = NULL;
    }
    g.prevCount    = 0;
    g.prevCapacity = 0;
}


void RpAtomicSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    static auto& gRpAtomicFreeListBlockSize           = StaticRef<RwInt32>(0x8D627C);
    static auto& gRpAtomicFreeListNumBlocksToPrealloc = StaticRef<RwInt32>(0x8D6280);
    gRpAtomicFreeListBlockSize           = blockSize;
    gRpAtomicFreeListNumBlocksToPrealloc = numBlocksToPrealloc;
}


void RpClumpSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    static auto& gRpClumpFreeListBlockSize           = StaticRef<RwInt32>(0x8D6284);
    static auto& gRpClumpFreeListNumBlocksToPrealloc = StaticRef<RwInt32>(0x8D6288);
    gRpClumpFreeListBlockSize           = blockSize;
    gRpClumpFreeListNumBlocksToPrealloc = numBlocksToPrealloc;
}


RpAtomic* AtomicDefaultRenderCallBack(RpAtomic* atomic) {
    static auto& rpAtomicDefaultPipelineGlobalsOffset = StaticRef<RwUInt32>(0xC9BC60);
    // atomic->pipeline — GTA offset 0x6C (repo rpworld.h RpAtomic has renderFrame+pad,
    // shifting llWorldSectorsInAtomic/pipeline to 0x68/0x70 — see STATUS.md ABI note).
    RxPipeline* pipeline = *(RxPipeline**)((RwUInt8*)atomic + 0x6C);
    if (pipeline == nullptr) {
        // default atomic pipeline: *(engine + *(u32*)0xC9BC60 + 0x3C)
        pipeline = *(RxPipeline**)(((RwUInt8*)RwEngineInstance) + rpAtomicDefaultPipelineGlobalsOffset + 0x3C);
    }
    if (RxPipelineExecute(pipeline, atomic, TRUE) == nullptr) {
        return nullptr;
    }
    return atomic;
}


void _rpAtomicResyncInterpolatedSphere(RpAtomic* atomic) {
    RpGeometry* geometry = atomic->geometry;
    if (geometry == nullptr) {
        return;
    }
    const RwInt32 start = atomic->interpolator.startMorphTarget; // +0x50
    const RwInt32 end   = atomic->interpolator.endMorphTarget;   // +0x52
    RpMorphTarget* const morphTargets = *(RpMorphTarget**)((RwUInt8*)geometry + 0x5C);
    if (start != end && start < geometry->numMorphTargets && end < geometry->numMorphTargets) {
        // lerp morph-target bounding spheres; alpha = position * recipTime (decompile: +0x5C * +0x58)
        const RwReal alpha = atomic->interpolator.position * atomic->interpolator.recipTime;
        const RpMorphTarget* mtA = &morphTargets[start];
        const RpMorphTarget* mtB = &morphTargets[end];
        atomic->boundingSphere.center.x =
            (mtB->boundingSphere.center.x - mtA->boundingSphere.center.x) * alpha + mtA->boundingSphere.center.x;
        atomic->boundingSphere.center.y =
            (mtB->boundingSphere.center.y - mtA->boundingSphere.center.y) * alpha + mtA->boundingSphere.center.y;
        atomic->boundingSphere.center.z =
            (mtB->boundingSphere.center.z - mtA->boundingSphere.center.z) * alpha + mtA->boundingSphere.center.z;
        atomic->boundingSphere.radius =
            (mtB->boundingSphere.radius - mtA->boundingSphere.radius) * alpha + mtA->boundingSphere.radius;
    } else if (start < geometry->numMorphTargets && end < geometry->numMorphTargets) {
        atomic->boundingSphere = morphTargets[start].boundingSphere;
    } else {
        atomic->boundingSphere = morphTargets[0].boundingSphere;
    }
    atomic->interpolator.flags &= ~rpINTERPOLATORDIRTYSPHERE;              // decompile: &= 0xFFFFFFFD
    atomic->object.object.privateFlags |= rpATOMICPRIVATEWORLDBOUNDDIRTY; // decompile: byte+3 |= 1
}


const RwSphere* RpAtomicGetWorldBoundingSphere(RpAtomic* atomic) {
    if (atomic->interpolator.flags & rpINTERPOLATORDIRTYSPHERE) { // decompile: byte@0x4C & 2
        _rpAtomicResyncInterpolatedSphere(atomic);
    }
    const RwBool frameDirty = RwFrameDirty((RwFrame*)atomic->object.object.parent);
    if (frameDirty || (atomic->object.object.privateFlags & rpATOMICPRIVATEWORLDBOUNDDIRTY)) {
        const RwMatrix* ltm = RwFrameGetLTM((RwFrame*)atomic->object.object.parent);
        RwV3dTransformPoint(&atomic->worldBoundingSphere.center, &atomic->boundingSphere.center, ltm);
        if ((ltm->flags & 3) != 3) { // matrix not orthonormal -> scale radius by max basis length
            const RwReal rightLenSq = ltm->right.x * ltm->right.x + ltm->right.y * ltm->right.y + ltm->right.z * ltm->right.z;
            const RwReal upLenSq    = ltm->up.x * ltm->up.x + ltm->up.y * ltm->up.y + ltm->up.z * ltm->up.z;
            const RwReal atLenSq    = ltm->at.x * ltm->at.x + ltm->at.y * ltm->at.y + ltm->at.z * ltm->at.z;
            RwReal maxLenSq = atLenSq;
            if (maxLenSq <= upLenSq) {
                maxLenSq = upLenSq;
            }
            if (rightLenSq > maxLenSq) {
                maxLenSq = rightLenSq;
            }
            atomic->object.object.privateFlags &= ~rpATOMICPRIVATEWORLDBOUNDDIRTY;
            atomic->worldBoundingSphere.radius = _rwSqrt(maxLenSq) * atomic->boundingSphere.radius;
        } else {
            atomic->worldBoundingSphere.radius = atomic->boundingSphere.radius;
            atomic->object.object.privateFlags &= ~rpATOMICPRIVATEWORLDBOUNDDIRTY;
        }
    }
    return &atomic->worldBoundingSphere;
}


void RpClumpGtaCancelStream() {
    plugin::Call<0x72E700>();
}

RpClump* RpClumpForAllAtomics(RpClump* clump, RpAtomicCallBack callback, void* data) {
    RwLLLink* link = clump->atomicList.link.next;
    while (link != &clump->atomicList.link) {
        RwLLLink* next = link->next; // saved before the callback (iteration may destroy)
        RpAtomic* atomic = (RpAtomic*)((RwUInt8*)link - offsetof(RpAtomic, inClumpLink));
        if (callback(atomic, data) == nullptr) {
            break;
        }
        link = next;
    }
    return clump;
}


RpClump* RpClumpForAllLights(RpClump* clump, RpLightCallBack callback, void* data) {
    static auto& lightClumpFieldOffset = StaticRef<RwInt32>(0xC924DC);
    RwLLLink* link = clump->lightList.link.next;
    while (link != &clump->lightList.link) {
        RwLLLink* next = link->next; // saved before the callback (iteration may destroy)
        RpLight* light = (RpLight*)((RwUInt8*)link - 4 - lightClumpFieldOffset);
        if (callback(light, data) == nullptr) {
            break;
        }
        link = next;
    }
    return clump;
}


RpClump* RpClumpForAllCameras(RpClump* clump, RwCameraCallBack callback, void* data) {
    static auto& cameraClumpFieldOffset = StaticRef<RwInt32>(0xC924D8);
    RwLLLink* link = clump->cameraList.link.next;
    while (link != &clump->cameraList.link) {
        RwLLLink* next = link->next; // saved before the callback (iteration may destroy)
        RwCamera* camera = (RwCamera*)((RwUInt8*)link - 4 - cameraClumpFieldOffset);
        if (callback(camera, data) == nullptr) {
            break;
        }
        link = next;
    }
    return clump;
}


RpAtomic* RpAtomicSetFrame(RpAtomic* atomic, RwFrame* frame)
{
    // 0x804EF0 = _rwObjectHasFrameSetFrame(object, frame) — rwcore internal, outside the vendor
    // floor (not in rw_addr_map.json), therefore left as a raw cast per campaign rules.
    ((void(__cdecl *)(void *, RwFrame *))0x804EF0)(atomic, frame);

    atomic->object.object.privateFlags |= rpATOMICPRIVATEWORLDBOUNDDIRTY;   // *(RwUInt8*)(atomic+3) |= 1

    return atomic;
}


RpClump* RpClumpCreateSpace(const RwV3d* position, RwReal radius) {
    static auto& atomicTKList                 = StaticRef<RwPluginRegistry>(0x8D624C);
    static auto& rpWorldFreeListGlobalsOffset = StaticRef<RwInt32>(0xC924AC);
    RpGeometry* geometry = RpGeometryCreateSpace(radius);
    if (geometry == nullptr) {
        return nullptr;
    }
    RwFrame* frame = RwFrameCreate();
    if (frame == nullptr) {
        RpGeometryDestroy(geometry);
        return nullptr;
    }
    *(RwV3d*)((RwUInt8*)frame + 0x40) = *position; // frame->modelling.pos (GTA offset)
    RpAtomic* atomic = RpAtomicCreate();
    if (atomic == nullptr) {
        RwFrameDestroy(frame);
        RpGeometryDestroy(geometry);
        return nullptr;
    }
    RpClump* clump = RpClumpCreate();
    if (clump == nullptr) {
        // full teardown of the just-created atomic, then frame and geometry (order as compiled)
        RwGlobals*  engine         = RwEngineInstance;
        RwFreeList* atomicFreeList = *(RwFreeList**)((RwUInt8*)engine + rpWorldFreeListGlobalsOffset);
        _rwPluginRegistryDeInitObject(&atomicTKList, atomic);
        if (atomic->repEntry != nullptr) {
            RwResourcesFreeResEntry(atomic->repEntry);
        }
        if (atomic->geometry != nullptr) {
            RpGeometryDestroy(atomic->geometry);
            RwFrame* atomicFrame = (RwFrame*)atomic->object.object.parent;
            atomic->geometry = nullptr;
            if (atomicFrame != nullptr && RpAtomicGetWorld(atomic) != nullptr) {
                RwFrameUpdateObjects(atomicFrame);
            }
        }
        _rwObjectHasFrameReleaseFrame(atomic);
        // engine+0x148 = RwMemoryFreeFn (RwFreeList-shaped), same convention as rwplcore wave-B
        (*(void(__cdecl**)(void*, void*))((RwUInt8*)engine + 0x148))(atomicFreeList, atomic);
        RwFrameDestroy(frame);
        RpGeometryDestroy(geometry);
        return nullptr;
    }
    _rwObjectHasFrameSetFrame(atomic, frame);
    atomic->object.object.privateFlags |= rpATOMICPRIVATEWORLDBOUNDDIRTY;
    if (geometry != atomic->geometry) {
        // geometry add-ref: 0x74CCB0 (outside floor, not in rw_addr_map -> raw cast; librw geo->addRef())
        ((RpGeometry * (__cdecl*)(RpGeometry*))0x74CCB0)(geometry);
        if (atomic->geometry != nullptr) {
            RpGeometryDestroy(atomic->geometry);
        }
        atomic->geometry = geometry;
        RpMorphTarget* morphTarget = *(RpMorphTarget**)((RwUInt8*)geometry + 0x5C); // GTA morphTarget offset
        atomic->boundingSphere = morphTarget->boundingSphere;
        RwFrame* atomicFrame = (RwFrame*)atomic->object.object.parent;
        if (atomicFrame != nullptr && RpAtomicGetWorld(atomic) != nullptr) {
            RwFrameUpdateObjects(atomicFrame);
        }
    }
    atomic->object.object.flags &= static_cast<RwUInt8>(~rpATOMICRENDER); // space clumps are not rendered
    RpGeometryDestroy(geometry);                                          // release our reference
    rwLinkListAddLLLink(&clump->atomicList, &atomic->inClumpLink);
    atomic->clump = clump;
    clump->object.parent = frame;
    return clump;
}


RpClump* RpClumpRender(RpClump* clump) {
    RpClump* result = clump;
    RwLLLink* link = clump->atomicList.link.next;
    while (link != &clump->atomicList.link) {
        RpAtomic* atomic = (RpAtomic*)((RwUInt8*)link - offsetof(RpAtomic, inClumpLink));
        if (atomic->object.object.flags & rpATOMICRENDER) {
            // GTA pulls the frame LTM (updates the hierarchy) before invoking the render callback
            RwFrameGetLTM((RwFrame*)atomic->object.object.parent);
            if (atomic->renderCallBack(atomic) == nullptr) {
                result = nullptr;
            }
        }
        link = link->next;
    }
    return result;
}


RpClump* RpClumpRemoveAtomic(RpClump* clump, RpAtomic* atomic) {
    // unlink atomic->inClumpLink; decompile writes prev->next / next->prev in this order
    atomic->inClumpLink.prev->next = atomic->inClumpLink.next;
    atomic->inClumpLink.next->prev = atomic->inClumpLink.prev;
    atomic->clump = nullptr;
    return clump;
}


RpClump* RpClumpAddAtomic(RpClump* clump, RpAtomic* atomic) {
    rwLinkListAddLLLink(&clump->atomicList, &atomic->inClumpLink); // exact instruction match
    atomic->clump = clump;
    return clump;
}


RpClump* RpClumpRemoveLight(RpClump* clump, RpLight* light)
{
    // Same clump-registered plugin payload on RpLight as RpClumpAddLight (see 0x74A4F0.cpp):
    // { RpClump *ownerClump; RwLLLink link; }, offset global DAT_00c924dc.
    struct ClumpObjLink
    {
        void    *ownerClump;   // +0x00
        RwLLLink link;         // +0x04
    };
    static auto& clumpLightListOffset = StaticRef<RwInt32>(0xC924DC);

    ClumpObjLink *lnk = reinterpret_cast<ClumpObjLink *>(reinterpret_cast<RwUInt8 *>(light) + clumpLightListOffset);

    lnk->link.prev->next = lnk->link.next;   // prev cell -> next
    lnk->link.next->prev = lnk->link.prev;   // next->prev = prev
    lnk->ownerClump      = NULL;             // zero all three dwords, as in the decompile
    lnk->link.next       = NULL;
    lnk->link.prev       = NULL;

    return clump;
}


RpClump* RpClumpAddLight(RpClump* clump, RpLight* light)
{
    // Plugin payload registered on every RpLight by RpWorldPluginAttach (offset global DAT_00c924dc):
    // { RpClump *ownerClump; RwLLLink link; } — link chained into clump->lightList.
    struct ClumpObjLink
    {
        void    *ownerClump;   // +0x00
        RwLLLink link;         // +0x04
    };
    static auto& clumpLightListOffset = StaticRef<RwInt32>(0xC924DC);

    ClumpObjLink *lnk  = reinterpret_cast<ClumpObjLink *>(reinterpret_cast<RwUInt8 *>(light) + clumpLightListOffset);
    RwLinkList   *list = &clump->lightList;

    lnk->link.prev           = &list->link;        // piVar3[2] = &clump->lightList.link
    lnk->link.next           = list->link.next;    // piVar3[1] = old head->next
    list->link.next->prev    = &lnk->link;         // old first element's prev = &lnk->link
    list->link.next          = &lnk->link;         // clump->lightList.link.next = &lnk->link
    lnk->ownerClump          = clump;              // piVar3[0] = clump

    return clump;   // ABI note: GTA binary ends the function without a visible return (decompile: void);
                    // repo header declares RpClump* — returning clump matches SDK docs and the value
                    // the binary happens to leave in eax.
}


RpClump* RpClumpRemoveCamera(RpClump* clump, RwCamera* camera)
{
    // Same clump-registered plugin payload on RwCamera as RpClumpAddCamera (see 0x74A550.cpp):
    // { RpClump *ownerClump; RwLLLink link; }, offset global DAT_00c924d8.
    struct ClumpObjLink
    {
        void    *ownerClump;   // +0x00
        RwLLLink link;         // +0x04
    };
    static auto& clumpCameraListOffset = StaticRef<RwInt32>(0xC924D8);

    ClumpObjLink *lnk = reinterpret_cast<ClumpObjLink *>(reinterpret_cast<RwUInt8 *>(camera) + clumpCameraListOffset);

    lnk->link.prev->next = lnk->link.next;
    lnk->link.next->prev = lnk->link.prev;
    lnk->ownerClump      = NULL;
    lnk->link.next       = NULL;
    lnk->link.prev       = NULL;

    return clump;
}


RpClump* RpClumpAddCamera(RpClump* clump, RwCamera* camera)
{
    // Plugin payload registered on every RwCamera by RpWorldPluginAttach (offset global DAT_00c924d8):
    // { RpClump *ownerClump; RwLLLink link; } — link chained into clump->cameraList.
    struct ClumpObjLink
    {
        void    *ownerClump;   // +0x00
        RwLLLink link;         // +0x04
    };
    static auto& clumpCameraListOffset = StaticRef<RwInt32>(0xC924D8);

    ClumpObjLink *lnk  = reinterpret_cast<ClumpObjLink *>(reinterpret_cast<RwUInt8 *>(camera) + clumpCameraListOffset);
    RwLinkList   *list = &clump->cameraList;

    lnk->link.prev           = &list->link;
    lnk->link.next           = list->link.next;
    list->link.next->prev    = &lnk->link;
    list->link.next          = &lnk->link;
    lnk->ownerClump          = clump;

    return clump;   // ABI note: same as RpClumpAddLight — binary has no visible return; header declares RpClump*.
}


RwBool RpClumpDestroy(RpClump* clump) {
    static auto& clumpTKList                  = StaticRef<RwPluginRegistry>(0x8D6264);
    static auto& atomicTKList                 = StaticRef<RwPluginRegistry>(0x8D624C);
    static auto& rpWorldFreeListGlobalsOffset = StaticRef<RwInt32>(0xC924AC);
    static auto& lightClumpFieldOffset        = StaticRef<RwInt32>(0xC924DC);
    static auto& cameraClumpFieldOffset       = StaticRef<RwInt32>(0xC924D8);
    RwGlobals*  engine         = RwEngineInstance;
    RwFreeList* atomicFreeList = *(RwFreeList**)((RwUInt8*)engine + rpWorldFreeListGlobalsOffset);
    RwFreeList* clumpFreeList  = *(RwFreeList**)((RwUInt8*)engine + rpWorldFreeListGlobalsOffset + 4);
    _rwPluginRegistryDeInitObject(&clumpTKList, clump);
    for (RwLLLink* link = clump->atomicList.link.next; link != &clump->atomicList.link;) {
        RwLLLink* next = link->next;
        RpAtomic* atomic = (RpAtomic*)((RwUInt8*)link - offsetof(RpAtomic, inClumpLink));
        _rwPluginRegistryDeInitObject(&atomicTKList, atomic);
        if (atomic->repEntry != nullptr) {
            RwResourcesFreeResEntry(atomic->repEntry);
        }
        if (atomic->geometry != nullptr) {
            RpGeometryDestroy(atomic->geometry);
            RwFrame* frame = (RwFrame*)atomic->object.object.parent;
            atomic->geometry = nullptr;
            if (frame != nullptr && RpAtomicGetWorld(atomic) != nullptr) {
                RwFrameUpdateObjects(frame);
            }
        }
        _rwObjectHasFrameReleaseFrame(atomic);
        (*(void(__cdecl**)(void*, void*))((RwUInt8*)engine + 0x148))(atomicFreeList, atomic);
        link = next;
    }
    for (RwLLLink* link = clump->lightList.link.next; link != &clump->lightList.link;) {
        RwLLLink* next = link->next;
        RpLight* light = (RpLight*)((RwUInt8*)link - 4 - lightClumpFieldOffset);
        RwLLLink* lightLink = (RwLLLink*)((RwUInt8*)light + 4 + lightClumpFieldOffset);
        // inline unlink (as compiled; identical to RpClumpRemoveLight) + clump = NULL
        lightLink->prev->next = lightLink->next;
        lightLink->next->prev = lightLink->prev;
        lightLink->prev = nullptr;
        lightLink->next = nullptr;
        *(RpClump**)((RwUInt8*)light + lightClumpFieldOffset) = nullptr;
        RpLightDestroy(light);
        link = next;
    }
    for (RwLLLink* link = clump->cameraList.link.next; link != &clump->cameraList.link;) {
        RwLLLink* next = link->next;
        RwCamera* camera = (RwCamera*)((RwUInt8*)link - 4 - cameraClumpFieldOffset);
        RwLLLink* cameraLink = (RwLLLink*)((RwUInt8*)camera + 4 + cameraClumpFieldOffset);
        // inline unlink (as compiled) + clump = NULL
        cameraLink->prev->next = cameraLink->next;
        cameraLink->next->prev = cameraLink->prev;
        cameraLink->prev = nullptr;
        cameraLink->next = nullptr;
        *(RpClump**)((RwUInt8*)camera + cameraClumpFieldOffset) = nullptr;
        RwCameraDestroy(camera);
        link = next;
    }
    if (clump->object.parent != nullptr) {
        RwFrameDestroyHierarchy((RwFrame*)clump->object.parent);
    }
    (*(void(__cdecl**)(void*, void*))((RwUInt8*)engine + 0x148))(clumpFreeList, clump);
    return TRUE;
}


RpClump* RpClumpCreate(void) {
    static auto& clumpTKList                 = StaticRef<RwPluginRegistry>(0x8D6264);
    static auto& rpWorldFreeListGlobalsOffset = StaticRef<RwInt32>(0xC924AC);
    RwGlobals*  engine        = RwEngineInstance;
    // clump freelist lives 4 bytes after the atomic freelist in the module globals block
    RwFreeList* clumpFreeList = *(RwFreeList**)((RwUInt8*)engine + rpWorldFreeListGlobalsOffset + 4);
    // engine+0x144 = RwMemoryAllocFn (RwFreeList-shaped), same convention as rwplcore wave-B
    RpClump* clump = (RpClump*)(*(void*(__cdecl**)(RwFreeList*, RwUInt32))((RwUInt8*)engine + 0x144))(clumpFreeList, 0x30010);
    if (clump == nullptr) {
        return nullptr;
    }
    rwLinkListInitialize(&clump->atomicList);
    rwLinkListInitialize(&clump->lightList);
    clump->object.type       = rpCLUMP; // *clump = 2
    rwLinkListInitialize(&clump->cameraList);
    clump->object.subType      = 0;
    clump->object.flags        = 0;
    clump->object.privateFlags = 0;
    clump->object.parent       = nullptr;
    rwLinkListInitialize(&clump->inWorldLink);
    clump->callback = reinterpret_cast<RpClumpCallBack>(0x74A260); // default clump callback (LAB_74A260, outside floor)
    _rwPluginRegistryInitObject(&clumpTKList, clump);
    return clump;
}


RpClump* RpClumpClone(RpClump* clump) {
    static auto& clumpTKList                 = StaticRef<RwPluginRegistry>(0x8D6264);
    static auto& atomicTKList                = StaticRef<RwPluginRegistry>(0x8D624C);
    static auto& rpWorldFreeListGlobalsOffset = StaticRef<RwInt32>(0xC924AC);
    static auto& lightClumpFieldOffset        = StaticRef<RwInt32>(0xC924DC);
    RpClump* newClump = RpClumpCreate();
    if (newClump == nullptr) {
        return nullptr;
    }
    RwFrame* clonedRoot = _rwFrameCloneAndLinkClones((RwFrame*)clump->object.parent);
    newClump->object.object.parent = clonedRoot;
    RwBool allCloned = FALSE;
    if (clonedRoot != nullptr) {
        allCloned = TRUE;
        for (RwLLLink* link = clump->atomicList.link.next; link != &clump->atomicList.link; link = link->next) {
            RpAtomic* srcAtomic = (RpAtomic*)((RwUInt8*)link - offsetof(RpAtomic, inClumpLink));
            RpAtomic* atomic = RpAtomicClone(srcAtomic);
            if (atomic == nullptr) {
                allCloned = FALSE;
                break;
            }
            if (srcAtomic->object.object.parent == nullptr) {
                // source atomic has no frame: discard the clone and report an error
                RwGlobals*  engine         = RwEngineInstance;
                RwFreeList* atomicFreeList = *(RwFreeList**)((RwUInt8*)engine + rpWorldFreeListGlobalsOffset);
                _rwPluginRegistryDeInitObject(&atomicTKList, atomic);
                if (atomic->repEntry != nullptr) {
                    RwResourcesFreeResEntry(atomic->repEntry);
                }
                RpAtomicSetGeometry(atomic, nullptr, 0);
                (*(void(__cdecl**)(void*, void*))((RwUInt8*)engine + 0x148))(atomicFreeList, atomic);
                RwError rwError;
                rwError.pluginID  = 2;
                rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(5);
                RwErrorSet(&rwError);
                allCloned = FALSE;
                break;
            }
            // pair the clone with the source frame's clone (RwFrame::root @ GTA +0xA0)
            _rwObjectHasFrameSetFrame(atomic, *(RwFrame**)((RwUInt8*)srcAtomic->object.object.parent + 0xA0));
            atomic->object.object.privateFlags |= rpATOMICPRIVATEWORLDBOUNDDIRTY;
            RpClumpAddAtomic(newClump, atomic);
        }
        _rwFramePurgeClone((RwFrame*)clump->object.parent);
        if (allCloned) {
            newClump->callback = clump->callback;
            _rwPluginRegistryCopyObject(&clumpTKList, newClump, clump);
            return newClump;
        }
        // failure B: tear down everything cloned so far
        RwGlobals*  engine         = RwEngineInstance;
        RwFreeList* clumpFreeList  = *(RwFreeList**)((RwUInt8*)engine + rpWorldFreeListGlobalsOffset + 4);
        _rwPluginRegistryDeInitObject(&clumpTKList, newClump);
        for (RwLLLink* link = newClump->atomicList.link.next; link != &newClump->atomicList.link;) {
            RwLLLink* next = link->next;
            RpAtomicDestroy((RpAtomic*)((RwUInt8*)link - offsetof(RpAtomic, inClumpLink)));
            link = next;
        }
        for (RwLLLink* link = newClump->lightList.link.next; link != &newClump->lightList.link;) {
            RwLLLink* next = link->next;
            RpLight* light = (RpLight*)((RwUInt8*)link - 4 - lightClumpFieldOffset);
            ((RpLight * (__cdecl*)(RpLight*, void*))0x74A1C0)(light, nullptr);
            link = next;
        }
        RpClumpForAllCameras(newClump, (RwCamera * (__cdecl*)(RwCamera*, void*))0x74A200, nullptr);
        if (newClump->object.object.parent != nullptr) {
            RwFrameDestroyHierarchy((RwFrame*)newClump->object.object.parent);
        }
        (*(void(__cdecl**)(void*, void*))((RwUInt8*)engine + 0x148))(clumpFreeList, newClump);
        return nullptr;
    }
    // failure A: frame hierarchy clone failed — newClump lists are empty but the binary still
    // runs the same teardown loops on them (guards break immediately).
    RwGlobals*  engine        = RwEngineInstance;
    RwFreeList* clumpFreeList = *(RwFreeList**)((RwUInt8*)engine + rpWorldFreeListGlobalsOffset + 4);
    _rwPluginRegistryDeInitObject(&clumpTKList, newClump);
    for (RwLLLink* link = newClump->atomicList.link.next; link != &newClump->atomicList.link;) {
        RwLLLink* next = link->next;
        RpAtomicDestroy((RpAtomic*)((RwUInt8*)link - offsetof(RpAtomic, inClumpLink)));
        link = next;
    }
    for (RwLLLink* link = newClump->lightList.link.next; link != &newClump->lightList.link;) {
        RwLLLink* next = link->next;
        RpLight* light = (RpLight*)((RwUInt8*)link - 4 - lightClumpFieldOffset);
        RpClumpRemoveLight(RpLightGetClump(light), light);
        RpLightDestroy(light);
        link = next;
    }
    RpClumpForAllCameras(newClump, (RwCamera * (__cdecl*)(RwCamera*, void*))0x74A200, nullptr);
    if (newClump->object.object.parent != nullptr) {
        RwFrameDestroyHierarchy((RwFrame*)newClump->object.object.parent);
    }
    (*(void(__cdecl**)(void*, void*))((RwUInt8*)engine + 0x148))(clumpFreeList, newClump);
    return nullptr;
}


RwBool RpAtomicDestroy(RpAtomic* atomic) {
    static auto& atomicTKList                 = StaticRef<RwPluginRegistry>(0x8D624C);
    static auto& rpWorldFreeListGlobalsOffset = StaticRef<RwInt32>(0xC924AC);
    _rwPluginRegistryDeInitObject(&atomicTKList, atomic);
    if (atomic->repEntry != nullptr) {
        RwResourcesFreeResEntry(atomic->repEntry);
    }
    if (atomic->geometry != nullptr) {
        RpGeometryDestroy(atomic->geometry);
        RwFrame* frame = (RwFrame*)atomic->object.object.parent;
        atomic->geometry = nullptr;
        if (frame != nullptr && RpAtomicGetWorld(atomic) != nullptr) {
            RwFrameUpdateObjects(frame);
        }
    }
    _rwObjectHasFrameReleaseFrame(atomic);
    RwGlobals*  engine         = RwEngineInstance;
    RwFreeList* atomicFreeList = *(RwFreeList**)((RwUInt8*)engine + rpWorldFreeListGlobalsOffset);
    (*(void(__cdecl**)(void*, void*))((RwUInt8*)engine + 0x148))(atomicFreeList, atomic);
    return TRUE;
}


RpAtomic* RpAtomicClone(RpAtomic* atomic) {
    static auto& atomicTKList = StaticRef<RwPluginRegistry>(0x8D624C);
    RpAtomic* clone = RpAtomicCreate();
    if (clone == nullptr) {
        return nullptr;
    }
    clone->object.object.type         = atomic->object.object.type;
    clone->object.object.subType      = atomic->object.object.subType;
    clone->object.object.flags        = atomic->object.object.flags;
    clone->object.object.privateFlags = atomic->object.object.privateFlags;
    clone->object.object.parent       = nullptr;
    clone->object.object.flags        = atomic->object.object.flags; // redundant re-write, as compiled
    _rwObjectHasFrameSetFrame(clone, nullptr);
    clone->object.object.privateFlags |= rpATOMICPRIVATEWORLDBOUNDDIRTY;
    if (atomic->geometry != clone->geometry) { // SetGeometry block, inlined by the compiler
        if (atomic->geometry != nullptr) {
            // geometry add-ref: 0x74CCB0 (outside floor, not in rw_addr_map -> raw cast)
            ((RpGeometry * (__cdecl*)(RpGeometry*))0x74CCB0)(atomic->geometry);
        }
        if (clone->geometry != nullptr) {
            RpGeometryDestroy(clone->geometry);
        }
        clone->geometry = atomic->geometry;
        if (atomic->geometry != nullptr) {
            RpMorphTarget* morphTarget = *(RpMorphTarget**)((RwUInt8*)atomic->geometry + 0x5C); // GTA morphTarget offset
            clone->boundingSphere = morphTarget->boundingSphere;
        }
        RwFrame* frame = (RwFrame*)clone->object.object.parent;
        if (frame != nullptr && RpAtomicGetWorld(clone) != nullptr) {
            RwFrameUpdateObjects(frame);
        }
    }
    clone->boundingSphere = atomic->boundingSphere;      // source sphere wins over morphTarget[0]
    clone->renderCallBack = atomic->renderCallBack;
    *(RxPipeline**)((RwUInt8*)clone + 0x6C)  = *(RxPipeline**)((RwUInt8*)atomic + 0x6C); // pipeline (GTA offsets)
    // 5 dwords from &interpolator: flags, start+end, time, recipTime — position is NOT copied
    RwUInt32* dst = reinterpret_cast<RwUInt32*>(&clone->interpolator);
    RwUInt32* src = reinterpret_cast<RwUInt32*>(&atomic->interpolator);
    for (RwInt32 i = 0; i < 5; ++i) {
        dst[i] = src[i];
    }
    _rwPluginRegistryCopyObject(&atomicTKList, clone, atomic);
    return clone;
}


RpAtomic* RpAtomicCreate(void) {
    static auto& atomicTKList                 = StaticRef<RwPluginRegistry>(0x8D624C);
    static auto& rpWorldFreeListGlobalsOffset = StaticRef<RwInt32>(0xC924AC);
    RwGlobals*  engine         = RwEngineInstance;
    RwFreeList* atomicFreeList = *(RwFreeList**)((RwUInt8*)engine + rpWorldFreeListGlobalsOffset);
    // engine+0x144 = RwMemoryAllocFn (RwFreeList-shaped), same convention as rwplcore wave-B / rpmatfx
    RpAtomic* atomic = (RpAtomic*)(*(void*(__cdecl**)(RwFreeList*, RwUInt32))((RwUInt8*)engine + 0x144))(atomicFreeList, 0x30014);
    if (atomic == nullptr) {
        return nullptr;
    }
    atomic->object.object.type       = rpATOMIC;   // *atomic = 1
    atomic->object.object.subType    = 0;
    atomic->object.object.parent     = nullptr;
    atomic->object.sync              = reinterpret_cast<RwObjectHasFrameSyncFunction>(0x749D20); // default sync (FUN_749D20, outside floor)
    atomic->repEntry                 = nullptr;
    atomic->object.object.flags        = rpATOMICCOLLISIONTEST | rpATOMICRENDER;            // 5
    atomic->object.object.privateFlags = rpATOMICPRIVATEWORLDBOUNDDIRTY;                    // 1
    _rwObjectHasFrameSetFrame(atomic, nullptr);
    atomic->geometry = nullptr;
    atomic->object.object.privateFlags |= rpATOMICPRIVATEWORLDBOUNDDIRTY;
    atomic->boundingSphere.radius      = 0.0f;
    atomic->boundingSphere.center.x    = 0.0f;
    atomic->boundingSphere.center.y    = 0.0f;
    atomic->boundingSphere.center.z    = 0.0f;
    atomic->worldBoundingSphere.radius = 0.0f;
    atomic->worldBoundingSphere.center.x = 0.0f;
    atomic->worldBoundingSphere.center.y = 0.0f;
    atomic->worldBoundingSphere.center.z = 0.0f;
    atomic->renderCallBack             = AtomicDefaultRenderCallBack;
    atomic->interpolator.time             = 1.0f; // 0x3F800000
    atomic->interpolator.recipTime        = 1.0f; // 0x3F800000
    atomic->interpolator.startMorphTarget = 0;
    atomic->interpolator.endMorphTarget   = 0;
    atomic->interpolator.position         = 0.0f;
    atomic->interpolator.flags            = rpINTERPOLATORDIRTYINSTANCE | rpINTERPOLATORDIRTYSPHERE; // 3
    atomic->inClumpLink.prev = nullptr;
    atomic->inClumpLink.next = nullptr;
    atomic->clump            = nullptr;
    *(RxPipeline**)((RwUInt8*)atomic + 0x6C) = nullptr; // atomic->pipeline (GTA offset; repo header shifts it to 0x70)
    RwLLLink* sectorsLink = (RwLLLink*)((RwUInt8*)atomic + 0x64); // llWorldSectorsInAtomic (GTA offset)
    sectorsLink->next = sectorsLink;
    sectorsLink->prev = sectorsLink;
    _rwPluginRegistryInitObject(&atomicTKList, atomic);
    return atomic;
}


RpAtomic* RpAtomicSetGeometry(RpAtomic* atomic, RpGeometry* geometry, RwUInt32 flags) {
    if (geometry != atomic->geometry) {
        if (geometry != nullptr) {
            // geometry add-ref: 0x74CCB0 (outside floor, not in rw_addr_map -> raw cast)
            ((RpGeometry * (__cdecl*)(RpGeometry*))0x74CCB0)(geometry);
        }
        if (atomic->geometry != nullptr) {
            RpGeometryDestroy(atomic->geometry);
        }
        atomic->geometry = geometry;
        if ((flags & rpATOMICSAMEBOUNDINGSPHERE) == 0) {
            if (geometry != nullptr) {
                RpMorphTarget* morphTarget = *(RpMorphTarget**)((RwUInt8*)geometry + 0x5C); // GTA morphTarget offset
                atomic->boundingSphere = morphTarget->boundingSphere;
            }
            RwFrame* frame = (RwFrame*)atomic->object.object.parent;
            if (frame != nullptr && RpAtomicGetWorld(atomic) != nullptr) {
                RwFrameUpdateObjects(frame);
            }
        }
    }
    return atomic;
}


RpClump* RpClumpSetCallBack(RpClump* clump, RpClumpCallBack callback) {
    if (callback != nullptr) {
        clump->callback = callback;
        return clump;
    }
    clump->callback = reinterpret_cast<RpClumpCallBack>(0x74A260); // default clump callback (LAB_74A260, outside floor)
    return clump;
}


RpClumpCallBack RpClumpGetCallBack(const RpClump* clump) {
    RpClumpCallBack callback = clump->callback;
    // decompile: return (callback != default) ? callback : NULL; default = LAB_74A260 (outside floor)
    if (callback == reinterpret_cast<RpClumpCallBack>(0x74A260)) {
        return nullptr;
    }
    return callback;
}


RwInt32 RpClumpGetNumAtomics(RpClump* clump) {
    RwInt32 numAtomics = 0;
    RwLLLink* link = clump->atomicList.link.next;
    while (link != &clump->atomicList.link) {
        // decompile safety stop: node != atomicBase+0x40 (i.e. atomic != NULL)
        if ((RpAtomic*)((RwUInt8*)link - offsetof(RpAtomic, inClumpLink)) == nullptr) {
            break;
        }
        ++numAtomics;
        link = link->next;
    }
    return numAtomics;
}


RwInt32 RpClumpGetNumLights(RpClump* clump) {
    static auto& lightClumpFieldOffset = StaticRef<RwInt32>(0xC924DC);
    RwInt32 numLights = 0;
    RwLLLink* link = clump->lightList.link.next;
    while (link != &clump->lightList.link) {
        // decompile safety stop: ((uintptr)link - offset) != 4  (i.e. light != NULL)
        if ((RpLight*)((RwUInt8*)link - 4 - lightClumpFieldOffset) == nullptr) {
            break;
        }
        ++numLights;
        link = link->next;
    }
    return numLights;
}


RwInt32 RpClumpGetNumCameras(RpClump* clump) {
    static auto& cameraClumpFieldOffset = StaticRef<RwInt32>(0xC924D8);
    RwInt32 numCameras = 0;
    RwLLLink* link = clump->cameraList.link.next;
    while (link != &clump->cameraList.link) {
        // decompile safety stop: ((uintptr)link - offset) != 4  (i.e. camera != NULL)
        if ((RwCamera*)((RwUInt8*)link - 4 - cameraClumpFieldOffset) == nullptr) {
            break;
        }
        ++numCameras;
        link = link->next;
    }
    return numCameras;
}


RpClump* RpLightGetClump(const RpLight* light) {
    static auto& lightClumpFieldOffset = StaticRef<RwInt32>(0xC924DC);
    return *(RpClump**)((RwUInt8*)light + lightClumpFieldOffset);
}


RpClump* RwCameraGetClump(const RwCamera* camera) {
    static auto& cameraClumpFieldOffset = StaticRef<RwInt32>(0xC924D8);
    return *(RpClump**)((RwUInt8*)camera + cameraClumpFieldOffset);
}


RwUInt32 RpAtomicStreamGetSize(RpAtomic* atomic)
{
    static auto& atomicTKList = StaticRef<RwPluginRegistry>(0x8D624C);

    // decompile: return _rwPluginRegistryGetSize(&atomicTK, atomic)
    //          + RpGeometryStreamGetSize(atomic->geometry) + 0x34;
    return _rwPluginRegistryGetSize(&atomicTKList, atomic)
         + RpGeometryStreamGetSize(atomic->geometry)
         + 0x34;
}


RpAtomic* RpAtomicStreamRead(RwStream* stream)
{
    struct GeomList
    {
        RpGeometry **geometries;
        RwInt32      numGeometries;
    };

    // decompile 0x74B030: two zeroed 8-byte structs {ptr, count} handed to the worker.
    rwFrameList frameList; frameList.frames = NULL; frameList.numFrames = 0;
    GeomList    geomList;   geomList.geometries = NULL; geomList.numGeometries = 0;

    // ABI note: binary wrapper has no visible return (decompile: void); propagating the worker's
    // result keeps the observable eax (the RpAtomic* the SDK documents) identical.
    return (RpAtomic *)((RpAtomic *(__cdecl *)(RwStream *, rwFrameList *, GeomList *))0x74B060)(stream, &frameList, &geomList);
}


RpAtomic* RpAtomicStreamWrite(RpAtomic* atomic, RwStream* stream)
{
    // 16-byte write context consumed by the internal writers 0x74A880 / 0x74AEA0 / 0x74AF20:
    // { RwStream *stream; void *unk08; void *unk0C; RwBool ok; } (decompile of 0x74A850:
    // {stream, 0, 0, 1}).
    struct StreamWriteCtx
    {
        RwStream *stream;
        void     *unk08;
        void     *unk0C;
        RwBool    ok;
    };

    StreamWriteCtx ctx;
    ctx.stream = stream;
    ctx.unk08  = NULL;
    ctx.unk0C  = NULL;
    ctx.ok     = TRUE;

    // ABI note: the GTA binary discards the worker's return (decompile: void), but its eax —
    // the worker's status — is what the SDK-documented RpAtomic* return value reads as; the
    // worker's status is propagated here to keep the observable return value identical.
    return (RpAtomic *)((RwInt32(__cdecl *)(RpAtomic *, StreamWriteCtx *))0x74A880)(atomic, &ctx);
}


RwUInt32 RpClumpStreamGetSize(RpClump* clump)
{
    struct GeomList
    {
        RpGeometry **geometries;
        RwInt32      numGeometries;
    };
    static auto& atomicTKList         = StaticRef<RwPluginRegistry>(0x8D624C);
    static auto& clumpTKList          = StaticRef<RwPluginRegistry>(0x8D6264);
    static auto& clumpLightListOffset  = StaticRef<RwInt32>(0xC924DC);
    static auto& clumpCameraListOffset = StaticRef<RwInt32>(0xC924D8);

    RwUInt32 size;
    rwFrameList frameList;

    if (_rwFrameListInitialize(&frameList, reinterpret_cast<RwFrame *>(clump->object.parent)) == NULL)
        return 0;
    size = _rwFrameListStreamGetSize(&frameList);
    _rwFrameListDeinitialize(&frameList);

    GeomList geomList;
    if (((RwBool(__cdecl *)(GeomList *, RpClump *))0x74A770)(&geomList, clump) == 0)
        return 0;

    RwUInt32 geomListSize = 0x10;
    for (RwInt32 i = 0; i < geomList.numGeometries; i++)
        geomListSize += 0xC + RpGeometryStreamGetSize(geomList.geometries[i]);
    size = geomListSize + size + 0x30;

    // vendor behavior (see file comment): destroys every geometry of the list
    for (RwInt32 i = 0; i < geomList.numGeometries; i++)
        RpGeometryDestroy(geomList.geometries[i]);
    if (geomList.geometries != NULL)
        ((void(__cdecl *)(void *))(*(void **)((RwUInt8 *)RwEngineInstance + 0x138)))(geomList.geometries); // RwMemoryFunctions.rwfree

    // atomics: 0x34 + plugin chunks each
    for (RwLLLink *lnk = clump->atomicList.link.next; lnk != &clump->atomicList.link; lnk = lnk->next) {
        RpAtomic *atomic = rwLLLinkGetData(lnk, RpAtomic, inClumpLink);
        size += 0x34 + _rwPluginRegistryGetSize(&atomicTKList, atomic);
    }
    // lights: 0x1C + light chunk each (link lives in the clump plugin payload at light+offset)
    for (RwLLLink *lnk = clump->lightList.link.next; lnk != &clump->lightList.link; lnk = lnk->next) {
        RpLight *light = reinterpret_cast<RpLight *>(reinterpret_cast<RwUInt8 *>(lnk) - 4 - clumpLightListOffset);
        size += 0x1C + RpLightStreamGetSize(light);
    }
    // cameras: 0x1C + camera chunk each
    for (RwLLLink *lnk = clump->cameraList.link.next; lnk != &clump->cameraList.link; lnk = lnk->next) {
        RwCamera *camera = reinterpret_cast<RwCamera *>(reinterpret_cast<RwUInt8 *>(lnk) - 4 - clumpCameraListOffset);
        size += 0x1C + RwCameraStreamGetSize(camera);
    }

    return size + 0xC + _rwPluginRegistryGetSize(&clumpTKList, clump);
}


RpClump* RpClumpStreamRead(RwStream* stream)
{
    struct GeomList
    {
        RpGeometry **geometries;
        RwInt32      numGeometries;
    };
    static auto& clumpTKList = StaticRef<RwPluginRegistry>(0x8D6264);

    RwUInt32     version;
    RwUInt32     length;
    RwInt32      counts[3];        // {numAtomics, numLights, numCameras}
    RpClump     *clump;
    rwFrameList  frameList;
    GeomList     geomList;
    RwInt32      numGeometries;
    RwInt32      readCount;        // local_28: geometries successfully read so far
    RwInt32      idx;              // iVar11
    RwInt32      i;                // iVar10 scratch
    RwInt32      frameIndex;
    RpAtomic    *atomic;
    RpLight     *light;
    RwCamera    *camera;
    RwError      rwError;

    if (RwStreamFindChunk(stream, 1 /* rwID_STRUCT */, &length, &version) == 0) {
        rwError.pluginID  = 2;   /* rpworld module */
        rwError.errorCode = ((RwInt32(__cdecl *)(RwInt32))0x8088D0)(0x8000001A);
        RwErrorSet(&rwError);
        return NULL;
    }
    if (version < 0x34000 || 0x36003 < version) {
        rwError.pluginID  = 2;
        rwError.errorCode = ((RwInt32(__cdecl *)(RwInt32))0x8088D0)(0x80000004);
        RwErrorSet(&rwError);
        return NULL;
    }
    if (RwStreamRead(stream, counts, 0xC) != 0xC) {
        rwError.pluginID  = 2;
        rwError.errorCode = ((RwInt32(__cdecl *)(RwInt32))0x8088D0)(0x8000001A);
        RwErrorSet(&rwError);
        return NULL;
    }

    clump = RpClumpCreate();
    if (clump == NULL)
        return NULL;

    if (RwStreamFindChunk(stream, 0x0E /* rwID_FRAMELIST */, NULL, &version) == 0) {
        _rwPluginRegistryDeInitObject(&clumpTKList, clump);
        for (RwLLLink *lnk = clump->atomicList.link.next; lnk != &clump->atomicList.link; ) {
            atomic = rwLLLinkGetData(lnk, RpAtomic, inClumpLink);
            lnk = lnk->next;
            RpAtomicDestroy(atomic);
        }
        return NULL;   // vendor flow: no RwErrorSet on this path
    }
    if (_rwFrameListStreamRead(stream, &frameList) == NULL) {
        _rwPluginRegistryDeInitObject(&clumpTKList, clump);
        for (RwLLLink *lnk = clump->atomicList.link.next; lnk != &clump->atomicList.link; ) {
            atomic = rwLLLinkGetData(lnk, RpAtomic, inClumpLink);
            lnk = lnk->next;
            RpAtomicDestroy(atomic);
        }
        RpClumpForAllLights(clump, (RpLightCallBack)0x74A1C0, NULL);
        RpClumpForAllCameras(clump, (RwCameraCallBack)0x74A200, NULL);
        if (clump->object.parent != NULL)
            RwFrameDestroyHierarchy(reinterpret_cast<RwFrame *>(clump->object.parent));
        ((void(__cdecl *)(RpWorld *, RpClump *))(*(void **)((RwUInt8 *)RwEngineInstance + 0x148)))(
            *(RpWorld **)((RwUInt8 *)RwEngineInstance + StaticRef<RwInt32>(0xC924AC) + 4), clump);
        rwError.pluginID  = 2;
        rwError.errorCode = ((RwInt32(__cdecl *)(RwInt32))0x8088D0)(0x8000001A);
        RwErrorSet(&rwError);
        return NULL;
    }
    clump->object.parent = frameList.frames[0];

    if (RwStreamFindChunk(stream, 0x1A /* rwID_GEOMETRYLIST */, NULL, &version) != 0) {
        if (RwStreamFindChunk(stream, 1 /* rwID_STRUCT */, &length, &version) != 0) {
            if (version < 0x34000 || 0x36003 < version) {
                rwError.pluginID  = 2;
                rwError.errorCode = ((RwInt32(__cdecl *)(RwInt32))0x8088D0)(0x80000004);
                RwErrorSet(&rwError);
                goto failAfterFrameList;
            }
            if (RwStreamRead(stream, &numGeometries, 4) == 4) {
                readCount = 0;
                if (numGeometries < 1) {
                    geomList.geometries = NULL;
                } else {
                    geomList.geometries = (RpGeometry **)
                        ((void *(*)(size_t, RwUInt32))(*(void **)((RwUInt8 *)RwEngineInstance + 0x134)))(
                            (size_t)(numGeometries * 4), 0x3000F);   // RwMemoryFunctions.rwmalloc
                    if (geomList.geometries == NULL) {
                        rwError.pluginID  = 2;
                        rwError.errorCode = ((RwInt32(__cdecl *)(RwInt32, RwInt32))0x8088D0)(0x80000013, numGeometries * 4);
                        RwErrorSet(&rwError);
                        goto failAfterFrameList;
                    }
                }
                geomList.numGeometries = 0;

                for (idx = 0; idx < numGeometries; idx++) {
                    if (RwStreamFindChunk(stream, 0x0F /* rwID_GEOMETRY */, NULL, &version) == 0) {
                        for (i = 0; i < readCount; i++)
                            RpGeometryDestroy(geomList.geometries[i]);
                        ((void(__cdecl *)(void *))(*(void **)((RwUInt8 *)RwEngineInstance + 0x138)))(geomList.geometries);
                        geomList.geometries = NULL;
                        goto failAfterFrameList;
                    }
                    if (version < 0x34000 || 0x36003 < version) {
                        ((void(__cdecl *)(GeomList *))0x74AE50)(&geomList);   // _rpGeometryListDeinitialize
                        goto failAfterFrameList;
                    }
                    geomList.geometries[readCount] = RpGeometryStreamRead(stream);
                    if (geomList.geometries[readCount] == NULL) {
                        ((void(__cdecl *)(GeomList *))0x74AE50)(&geomList);
                        goto failAfterFrameList;
                    }
                    readCount++;
                    geomList.numGeometries = readCount;
                }

                for (idx = 0; idx < counts[0]; idx++) {
                    if (RwStreamFindChunk(stream, 0x14 /* rwID_ATOMIC */, NULL, &version) == 0 ||
                        (atomic = (RpAtomic *)((RpAtomic *(__cdecl *)(RwStream *, rwFrameList *, GeomList *))0x74B060)(
                             stream, &frameList, &geomList)) == NULL) {
                        for (i = 0; i < readCount; i++)
                            RpGeometryDestroy(geomList.geometries[i]);
                        goto failWithGeometries;
                    }
                    RpClumpAddAtomic(clump, atomic);
                }

                for (idx = 0; idx < counts[1]; idx++) {
                    if (RwStreamFindChunk(stream, 1 /* rwID_STRUCT */, NULL, NULL) == 0 ||
                        RwStreamReadInt32(stream, &frameIndex, 4) == NULL ||
                        RwStreamFindChunk(stream, 0x12 /* rwID_LIGHT */, NULL, NULL) == 0 ||
                        (light = RpLightStreamRead(stream)) == NULL) {
                        for (i = 0; i < readCount; i++)
                            RpGeometryDestroy(geomList.geometries[i]);
                        goto failWithGeometries;
                    }
                    ((void(__cdecl *)(void *, RwFrame *))0x804EF0)(light, frameList.frames[frameIndex]);
                    RpClumpAddLight(clump, light);
                }

                for (idx = 0; idx < counts[2]; idx++) {
                    if (RwStreamFindChunk(stream, 1 /* rwID_STRUCT */, NULL, NULL) == 0 ||
                        RwStreamReadInt32(stream, &frameIndex, 4) == NULL ||
                        RwStreamFindChunk(stream, 5 /* rwID_CAMERA */, NULL, NULL) == 0 ||
                        (camera = RwCameraStreamRead(stream)) == NULL) {
                        for (i = 0; i < readCount; i++)
                            RpGeometryDestroy(geomList.geometries[i]);
                        goto failWithGeometries;
                    }
                    ((void(__cdecl *)(void *, RwFrame *))0x804EF0)(camera, frameList.frames[frameIndex]);
                    RpClumpAddCamera(clump, camera);
                }

                for (i = 0; i < readCount; i++)
                    RpGeometryDestroy(geomList.geometries[i]);
                if (geomList.geometries != NULL)
                    ((void(__cdecl *)(void *))(*(void **)((RwUInt8 *)RwEngineInstance + 0x138)))(geomList.geometries);
                _rwFrameListDeinitialize(&frameList);

                if (_rwPluginRegistryReadDataChunks(&clumpTKList, stream, clump) == NULL) {
                    RpClumpDestroy(clump);
                    rwError.pluginID  = 2;
                    rwError.errorCode = ((RwInt32(__cdecl *)(RwInt32))0x8088D0)(0x8000001A);
                    RwErrorSet(&rwError);
                    return NULL;
                }
                return clump;
            }
            // struct read != 4 bytes: falls into LAB_0074b7de in the decompile
            goto failAfterFrameList;
        }
        // inner rwID_STRUCT chunk missing: falls into LAB_0074b7de in the decompile
        goto failAfterFrameList;
    }
    // rwID_GEOMETRYLIST chunk missing: separate teardown variant (decompile lines 298-318):
    // like failAfterFrameList but destroys atomics with an inline walk + RpAtomicDestroy
    // instead of RpClumpForAllAtomics(0x74BCC0).
    _rwFrameListDeinitialize(&frameList);
    _rwPluginRegistryDeInitObject(&clumpTKList, clump);
    for (RwLLLink *lnk = clump->atomicList.link.next; lnk != &clump->atomicList.link; ) {
        atomic = rwLLLinkGetData(lnk, RpAtomic, inClumpLink);
        lnk = lnk->next;
        RpAtomicDestroy(atomic);
    }
    RpClumpForAllLights(clump, (RpLightCallBack)0x74A1C0, NULL);
    RpClumpForAllCameras(clump, (RwCameraCallBack)0x74A200, NULL);
    if (clump->object.parent != NULL)
        RwFrameDestroyHierarchy(reinterpret_cast<RwFrame *>(clump->object.parent));
    ((void(__cdecl *)(RpWorld *, RpClump *))(*(void **)((RwUInt8 *)RwEngineInstance + 0x148)))(
        *(RpWorld **)((RwUInt8 *)RwEngineInstance + StaticRef<RwInt32>(0xC924AC) + 4), clump);
    rwError.pluginID  = 2;
    rwError.errorCode = ((RwInt32(__cdecl *)(RwInt32))0x8088D0)(0x8000001A);
    RwErrorSet(&rwError);
    return NULL;

failWithGeometries:
    if (geomList.geometries != NULL)
        ((void(__cdecl *)(void *))(*(void **)((RwUInt8 *)RwEngineInstance + 0x138)))(geomList.geometries);
    geomList.geometries = NULL;
    // fall through
failAfterFrameList:
    _rwFrameListDeinitialize(&frameList);
    _rwPluginRegistryDeInitObject(&clumpTKList, clump);
    RpClumpForAllAtomics(clump, (RpAtomicCallBack)0x74BCC0, NULL);
    RpClumpForAllLights(clump, (RpLightCallBack)0x74A1C0, NULL);
    RpClumpForAllCameras(clump, (RwCameraCallBack)0x74A200, NULL);
    if (clump->object.parent != NULL)
        RwFrameDestroyHierarchy(reinterpret_cast<RwFrame *>(clump->object.parent));
    ((void(__cdecl *)(RpWorld *, RpClump *))(*(void **)((RwUInt8 *)RwEngineInstance + 0x148)))(
        *(RpWorld **)((RwUInt8 *)RwEngineInstance + StaticRef<RwInt32>(0xC924AC) + 4), clump);
    rwError.pluginID  = 2;
    rwError.errorCode = ((RwInt32(__cdecl *)(RwInt32))0x8088D0)(0x8000001A);
    RwErrorSet(&rwError);
    return NULL;
}


RpClump* RpClumpStreamWrite(RpClump* clump, RwStream* stream)
{
    struct GeomList
    {
        RpGeometry **geometries;
        RwInt32      numGeometries;
    };
    // Same 16-byte write context the internal workers consume (see 0x74A850.cpp).
    struct StreamWriteCtx
    {
        RwStream *stream;
        void     *unk08;
        void     *unk0C;
        RwBool    ok;
    };
    static auto& clumpTKList = StaticRef<RwPluginRegistry>(0x8D6264);

    RwUInt32    clumpSize  = 0x18;   // iStack_38
    RwUInt32    headerSize = 0;      // iVar6: size handed to the rwID_CLUMP chunk header
    rwFrameList frameList;
    GeomList    geomList;
    StreamWriteCtx ctx;
    RwInt32     counts[3];           // {numAtomics, numLights, numCameras} (iStack_24/-20/-1c)
    RwInt32     i;

    if (_rwFrameListInitialize(&frameList, reinterpret_cast<RwFrame *>(clump->object.parent)) != NULL) {
        clumpSize += 0xC + _rwFrameListStreamGetSize(&frameList);
        _rwFrameListDeinitialize(&frameList);

        if (((RwBool(__cdecl *)(GeomList *, RpClump *))0x74A770)(&geomList, clump) != 0) {
            RwUInt32 geomSize = 0x10;
            for (i = 0; i < geomList.numGeometries; i++)
                geomSize += 0xC + RpGeometryStreamGetSize(geomList.geometries[i]);
            clumpSize += 0xC + geomSize;
            ((void(__cdecl *)(GeomList *))0x74AE50)(&geomList);
            RpClumpForAllAtomics(clump, (RpAtomicCallBack)0x74B000, &clumpSize);
            RpClumpForAllLights(clump, (RpLightCallBack)0x74AFA0, &clumpSize);
            RpClumpForAllCameras(clump, (RwCameraCallBack)0x74AFD0, &clumpSize);
            headerSize = clumpSize + 0xC + _rwPluginRegistryGetSize(&clumpTKList, clump);
            clumpSize  = headerSize;
        }
    }

    if (_rwStreamWriteVersionedChunkHeader(stream, 0x10 /* rwID_CLUMP */, headerSize, 0x36003, 0xFFFF) == NULL)
        return NULL;
    if (_rwStreamWriteVersionedChunkHeader(stream, 1 /* rwID_STRUCT */, 0xC, 0x36003, 0xFFFF) == NULL)
        return NULL;

    counts[0] = 0;
    for (RwLLLink *lnk = clump->atomicList.link.next; lnk != &clump->atomicList.link; lnk = lnk->next)
        counts[0]++;
    counts[1] = 0;
    RpClumpForAllLights(clump, (RpLightCallBack)0x74AE40, &counts[1]);
    counts[2] = 0;
    for (RwLLLink *lnk = clump->cameraList.link.next; lnk != &clump->cameraList.link; lnk = lnk->next)
        counts[2]++;

    if (RwStreamWrite(stream, counts, 0xC) == NULL)
        return NULL;

    if (_rwFrameListInitialize(&frameList, reinterpret_cast<RwFrame *>(clump->object.parent)) == NULL)
        return NULL;
    if (_rwFrameListStreamWrite(&frameList, stream) == NULL) {
        _rwFrameListDeinitialize(&frameList);
        return NULL;
    }
    if (((RwBool(__cdecl *)(GeomList *, RpClump *))0x74A770)(&geomList, clump) == 0)
        return NULL;   // vendor-faithful: frameList is leaked here in the binary (no deinit on this path)

    RwUInt32 geomSize = 0x10;
    for (i = 0; i < geomList.numGeometries; i++)
        geomSize += 0xC + RpGeometryStreamGetSize(geomList.geometries[i]);

    if (_rwStreamWriteVersionedChunkHeader(stream, 0x1A /* rwID_GEOMETRYLIST */, geomSize, 0x36003, 0xFFFF) == NULL)
        goto failGeoms;
    if (_rwStreamWriteVersionedChunkHeader(stream, 1 /* rwID_STRUCT */, 4, 0x36003, 0xFFFF) == NULL)
        goto failGeoms;
    if (RwStreamWrite(stream, &geomList.numGeometries, 4) == NULL)
        goto failGeoms;
    for (i = 0; i < geomList.numGeometries; i++)
        if (RpGeometryStreamWrite(geomList.geometries[i], stream) == NULL)
            goto failGeoms;

    ctx.stream = stream;
    ctx.unk08  = NULL;
    ctx.unk0C  = NULL;
    ctx.ok     = TRUE;
    for (RwLLLink *lnk = clump->atomicList.link.next; lnk != &clump->atomicList.link; ) {
        RpAtomic *atomic = rwLLLinkGetData(lnk, RpAtomic, inClumpLink);
        lnk = lnk->next;
        if (((RwInt32(__cdecl *)(RpAtomic *, StreamWriteCtx *))0x74A880)(atomic, &ctx) == 0)
            break;
    }
    if (counts[1] > 0 && ctx.ok) {
        for (RwLLLink *lnk = clump->lightList.link.next; lnk != &clump->lightList.link; ) {
            RpLight *light = reinterpret_cast<RpLight *>(reinterpret_cast<RwUInt8 *>(lnk) - 4 - StaticRef<RwInt32>(0xC924DC));
            lnk = lnk->next;
            if (((RwInt32(__cdecl *)(RpLight *, StreamWriteCtx *))0x74AEA0)(light, &ctx) == 0)
                break;
        }
    }
    if (counts[2] > 0 && ctx.ok) {
        for (RwLLLink *lnk = clump->cameraList.link.next; lnk != &clump->cameraList.link; ) {
            RwCamera *camera = reinterpret_cast<RwCamera *>(reinterpret_cast<RwUInt8 *>(lnk) - 4 - StaticRef<RwInt32>(0xC924D8));
            lnk = lnk->next;
            if (((RwInt32(__cdecl *)(RwCamera *, StreamWriteCtx *))0x74AF20)(camera, &ctx) == 0)
                break;
        }
    }

    ((void(__cdecl *)(GeomList *))0x74AE50)(&geomList);
    _rwFrameListDeinitialize(&frameList);
    if (ctx.ok) {
        RwInt32 ok = _rwPluginRegistryWriteDataChunks(&clumpTKList, stream, clump);
        return ok ? clump : NULL;
    }
    return NULL;

failGeoms:
    for (i = 0; i < geomList.numGeometries; i++)
        RpGeometryDestroy(geomList.geometries[i]);
    if (geomList.geometries != NULL)
        ((void(__cdecl *)(void *))(*(void **)((RwUInt8 *)RwEngineInstance + 0x138)))(geomList.geometries); // RwMemoryFunctions.rwfree
    return NULL;
}


RpClumpChunkInfo* _rpClumpChunkInfoRead(RwStream* stream, RpClumpChunkInfo* clumpChunkInfo, RwInt32* bytesRead)
{
    RwUInt32 version;
    RwUInt32 length;

    if (RwStreamFindChunk(stream, 1 /* rwID_STRUCT */, &length, &version) == 0)
        return NULL;
    if (RwStreamRead(stream, clumpChunkInfo, 0xC /* sizeof(RpClumpChunkInfo) */) != 0xC)
        return NULL;
    *bytesRead = (RwInt32)(length + 0xC);
    return clumpChunkInfo;
}


RwInt32 RpAtomicRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB)
{
    static auto& atomicTKList = StaticRef<RwPluginRegistry>(0x8D624C);

    // ABI note: binary wrapper discards the helper result (decompile: void); repo header declares
    // RwInt32 — propagating the helper's return keeps the observable eax identical.
    return _rwPluginRegistryAddPlugin(&atomicTKList, size, pluginID, constructCB, destructCB, copyCB);
}


RwInt32 RpClumpRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB)
{
    static auto& clumpTKList = StaticRef<RwPluginRegistry>(0x8D6264);

    // ABI note: same as RpAtomicRegisterPlugin — binary discards the helper result; header declares RwInt32.
    return _rwPluginRegistryAddPlugin(&clumpTKList, size, pluginID, constructCB, destructCB, copyCB);
}


RwInt32 RpAtomicRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB)
{
    static auto& atomicTKList = StaticRef<RwPluginRegistry>(0x8D624C);

    // ABI note: binary discards the helper result; header declares RwInt32 — propagate eax.
    return _rwPluginRegistryAddPluginStream(&atomicTKList, pluginID, readCB, writeCB, getSizeCB);
}


RwInt32 RpAtomicSetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB)
{
    static auto& atomicTKList = StaticRef<RwPluginRegistry>(0x8D624C);

    // ABI note: binary discards the helper result; header declares RwInt32 — propagate eax.
    return _rwPluginRegistryAddPlgnStrmlwysCB(&atomicTKList, pluginID, alwaysCB);
}


RwInt32 RpAtomicSetStreamRightsCallBack(RwUInt32 pluginID, RwPluginDataChunkRightsCallBack rightsCB)
{
    static auto& atomicTKList = StaticRef<RwPluginRegistry>(0x8D624C);

    // ABI note: binary discards the helper result; header declares RwInt32 — propagate eax.
    return _rwPluginRegistryAddPlgnStrmRightsCB(&atomicTKList, pluginID, rightsCB);
}


RwInt32 RpClumpRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB)
{
    static auto& clumpTKList = StaticRef<RwPluginRegistry>(0x8D6264);

    // ABI note: binary discards the helper result; header declares RwInt32 — propagate eax.
    return _rwPluginRegistryAddPluginStream(&clumpTKList, pluginID, readCB, writeCB, getSizeCB);
}


RwInt32 RpClumpSetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB)
{
    static auto& clumpTKList = StaticRef<RwPluginRegistry>(0x8D6264);

    // ABI note: binary discards the helper result; header declares RwInt32 — propagate eax.
    return _rwPluginRegistryAddPlgnStrmlwysCB(&clumpTKList, pluginID, alwaysCB);
}


RwInt32 RpAtomicGetPluginOffset(RwUInt32 pluginID)
{
    static auto& atomicTKList = StaticRef<RwPluginRegistry>(0x8D624C);

    // ABI note: binary discards the helper result; header declares RwInt32 — propagate eax.
    return _rwPluginRegistryGetPluginOffset(&atomicTKList, pluginID);
}


RwInt32 RpClumpGetPluginOffset(RwUInt32 pluginID)
{
    static auto& clumpTKList = StaticRef<RwPluginRegistry>(0x8D6264);

    // ABI note: binary discards the helper result; header declares RwInt32 — propagate eax.
    return _rwPluginRegistryGetPluginOffset(&clumpTKList, pluginID);
}


RwBool RpAtomicValidatePlugins(const RpAtomic* atomic)
{
    // decompile: return 1 (parameters unused)
    (void)atomic;
    return TRUE;
}


RwBool RpClumpValidatePlugins(const RpClump* clump)
{
    // decompile: return 1 (parameters unused)
    (void)clump;
    return TRUE;
}


RwBool RpAtomicInstance(RpAtomic* atomic)
{
    RpGeometry *geometry = atomic->geometry;   // atomic+0x18

    if (geometry->numMorphTargets != 1)
        return FALSE;

    if ((geometry->flags & rpGEOMETRYNATIVE) == 0) {
        if (geometry->repEntry != NULL)
            RwResourcesFreeResEntry(geometry->repEntry);

        geometry->flags |= rpGEOMETRYNATIVEINSTANCE;                    // |= 0x02000000
        atomic->renderCallBack(atomic);                                 // (**(atomic+0x48))(atomic)
        geometry->flags = (geometry->flags & ~rpGEOMETRYNATIVEINSTANCE) | rpGEOMETRYNATIVE;
        // decompile: *(flags) = *(flags) & 0xfdffffff | 0x1000000
    }
    return TRUE;
}


RwBool _rpWorldFindBBox(RpWorld* world, RwBBox* boundingBox) {
    // Прямой обход BSP в глубину без рекурсии (стек указателей на правые поддеревья, глубина <= 64).
    // bbox строится по tightBoundingBox всех листьев: первый лист - RwBBoxInitialize(inf), дальше
    // RwBBoxAddPoint(inf); затем всегда RwBBoxAddPoint(sup).
    RpSectorBSP* stack[64];
    RwInt32      depth = 0;
    RwBool       first = FALSE;
    RpSectorBSP* cur = reinterpret_cast<RpSectorBSP*>(reinterpret_cast<RpWorldGta*>(world)->rootSector);
    do {
        if (cur->type < 0) {
            RpWorldSectorGta* ws = reinterpret_cast<RpWorldSectorGta*>(cur);
            if (first) {
                RwBBoxInitialize(boundingBox, &ws->tightBoundingBox.inf);
                first = TRUE;
            } else {
                RwBBoxAddPoint(boundingBox, &ws->tightBoundingBox.inf);
            }
            RwBBoxAddPoint(boundingBox, &ws->tightBoundingBox.sup);
            cur = stack[depth--];
        } else {
            depth++;
            stack[depth] = cur->rightSubTree;
            cur = cur->leftSubTree;
        }
    } while (depth >= 0);
    return TRUE;
}


RpWorld* _rpWorldSetupSectorBoundingBoxes(RpWorld* world) {
    // Рекурсивный алгоритм на явных стеках (декомпил 0x74F020):
    //   setup(узел, box): лист -> sector->boundingBox(0x48) = box;
    //   плоскость -> rightBox = box; rightBox[type] = node->rightValue(0x14);
    //                leftBox  = box; leftBox[type]  = node->leftValue(0x10);
    //                setup(left, leftBox); setup(right, rightBox).
    // type используется как БАЙТОВОЕ смещение (0/4/8/0xC/0x10/0x14) в 6 RwReal бокса.
    RpWorldGta* w = reinterpret_cast<RpWorldGta*>(world);
    RpSectorBSP* nodeStack[64];
    RwReal       boxStack[64][6];
    RwInt32      depth = 0;
    RpSectorBSP* cur = reinterpret_cast<RpSectorBSP*>(w->rootSector);
    RwReal       curBox[6];
    curBox[0] = w->boundingBox.sup.x;
    curBox[1] = w->boundingBox.sup.y;
    curBox[2] = w->boundingBox.sup.z;
    curBox[3] = w->boundingBox.inf.x;
    curBox[4] = w->boundingBox.inf.y;
    curBox[5] = w->boundingBox.inf.z;
    do {
        if (cur->type < 0) {
            RpWorldSectorGta* ws = reinterpret_cast<RpWorldSectorGta*>(cur);
            RwReal* dst = reinterpret_cast<RwReal*>(&ws->boundingBox);
            dst[0] = curBox[0];
            dst[1] = curBox[1];
            dst[2] = curBox[2];
            dst[3] = curBox[3];
            dst[4] = curBox[4];
            dst[5] = curBox[5];
            cur = nodeStack[depth];
            depth--;
            const RwReal* popped = boxStack[depth + 1];
            curBox[0] = popped[0];
            curBox[1] = popped[1];
            curBox[2] = popped[2];
            curBox[3] = popped[3];
            curBox[4] = popped[4];
            curBox[5] = popped[5];
        } else {
            RpSectorBSP* node = cur;
            depth++;
            nodeStack[depth] = node->rightSubTree;
            RwReal* pushed = boxStack[depth];
            pushed[0] = curBox[0];
            pushed[1] = curBox[1];
            pushed[2] = curBox[2];
            pushed[3] = curBox[3];
            pushed[4] = curBox[4];
            pushed[5] = curBox[5];
            const RwInt32 type = node->type;
            *(RwReal*)(reinterpret_cast<RwUInt8*>(pushed) + type) = node->rightValue; // бокс правого поддерева
            cur = node->leftSubTree;
            *(RwReal*)(reinterpret_cast<RwUInt8*>(curBox) + type) = node->leftValue;  // бокс левого поддерева
        }
    } while (depth >= 0);
    return world;
}


static auto& rpWorldSectorPluginRegistry = StaticRef<RwPluginRegistry>(0x8D736C);
// ==== END SHARED ====

void _rpWorldSectorDeinstanceAll(RpSector* sector) {
    RpWorldSectorGta* sect = (RpWorldSectorGta*)sector;
    RwInt32 type = sect->type;
    if (type != -2 /* rwSECTORTYPENULL */) {
        while (true) {
            if (type == -1 /* RpWorldSector leaf */) {
                if (sect->repEntry != nullptr) {
                    RwResourcesFreeResEntry(sect->repEntry);
                }
                // collAtomicsInWorldSector: walk + destroy nodes (vendor helper 0x74FFF0,
                // outside the translated floor -> raw call)
                RwLLLink* link = sect->collAtomicsInWorldSector.link.next;
                while ((void*)link != (void*)&sect->collAtomicsInWorldSector.link) {
                    RwLLLink* next = link->next;
                    ((void (__cdecl*)(RwLLLink*))0x74FFF0)(link);
                    link = next;
                }
                // lightsInWorldSector: walk + destroy nodes (vendor helper 0x74FFA0)
                link = sect->lightsInWorldSector.link.next;
                while ((void*)link != (void*)&sect->lightsInWorldSector.link) {
                    RwLLLink* next = link->next;
                    ((void (__cdecl*)(RwLLLink*))0x74FFA0)(link);
                    link = next;
                }
                _rwPluginRegistryDeInitObject(&rpWorldSectorPluginRegistry, sect);
                if (sect->mesh != nullptr) {
                    _rpMeshDestroy(sect->mesh);
                    sect->mesh = nullptr;
                }
                return;
            }
            // RpPlaneSector: recurse left, then continue with the right subtree
            if (sect->leftSubTree != nullptr) {
                _rpWorldSectorDeinstanceAll((RpSector*)sect->leftSubTree);
            }
            sect = (RpWorldSectorGta*)sect->rightSubTree;
            if (sect == nullptr) {
                break;
            }
            type = sect->type;
            if (type == -2) {
                return;
            }
        }
    }
}



// ============================================================================================
// ============================================================================================

void _rpWorldSectorDestroyRecurse(RpSector* sector) {
    RpSectorBSP* s = reinterpret_cast<RpSectorBSP*>(sector);
    if (s->type == -2) {
        // Пустой корень: просто освободить сам узел.
        RwGlobalsMem()->rwfree(sector);
        return;
    }
    if (s->type == -1) {
        // Лист (RpWorldSector): полная зачистка.
        RpWorldSectorGta* ws = reinterpret_cast<RpWorldSectorGta*>(sector);
        if (ws->repEntry != nullptr) {
            RwResourcesFreeResEntry(ws->repEntry);
        }
        for (RwLLLink* link = ws->collAtomicsInWorldSector.link.next;
             link != &ws->collAtomicsInWorldSector.link;) {
            RwLLLink* next = link->next;
            ((void(__cdecl*)(RwLLLink*))0x74FFF0)(link);
            link = next;
        }
        for (RwLLLink* link = ws->lightsInWorldSector.link.next;
             link != &ws->lightsInWorldSector.link;) {
            RwLLLink* next = link->next;
            ((void(__cdecl*)(RwLLLink*))0x74FFA0)(link);
            link = next;
        }
        _rwPluginRegistryDeInitObject(&worldSectorTKList, sector);
        if (ws->vertices != nullptr) {
            RwGlobalsMem()->rwfree(ws->vertices);
            ws->vertices = nullptr;
        }
        if (ws->normals != nullptr) {
            RwGlobalsMem()->rwfree(ws->normals);
            ws->normals = nullptr;
        }
        if (ws->field_30 != 0) {
            RwGlobalsMem()->rwfree(reinterpret_cast<void*>(ws->field_30));
            ws->field_30 = 0;
        }
        if (ws->triangles != nullptr) {
            RwGlobalsMem()->rwfree(ws->triangles);
            ws->triangles = nullptr;
        }
        for (RwInt32 i = 0; i < 8; i++) {
            if (ws->field_10[i] != 0) {
                RwGlobalsMem()->rwfree(reinterpret_cast<void*>(ws->field_10[i]));
                ws->field_10[i] = 0;
            }
        }
        if (ws->mesh != nullptr) {
            _rpMeshDestroy(ws->mesh);
            ws->mesh = nullptr;
        }
        RwGlobalsMem()->rwfree(sector);
        return;
    }
    // Плоскостной узел: рекурсивно в обе поддерева, затем освободить узел.
    if (s->leftSubTree != nullptr) {
        _rpWorldSectorDestroyRecurse(reinterpret_cast<RpSector*>(s->leftSubTree));
    }
    s->leftSubTree = nullptr;
    if (s->rightSubTree != nullptr) {
        _rpWorldSectorDestroyRecurse(reinterpret_cast<RpSector*>(s->rightSubTree));
    }
    s->rightSubTree = nullptr;
    RwGlobalsMem()->rwfree(sector);
}


RwBool _rpWorldForAllGlobalLights(RpLightCallBack callBack, void* data) {
    // "Глобальные" света вендорской сборки = directionalLightList текущего мира (engine+0x04 = curWorld).
    // Звено списка - RpLight::inWorld (@0x34, layout repo rpworld.h RpLight совпадает: light = link - 0x34).
    RpWorldGta* world = static_cast<RpWorldGta*>(RwEngineInstance->curWorld);
    for (RwLLLink* link = world->directionalLightList.link.next;
         link != &world->directionalLightList.link;
         link = link->next) {
        RpLight* light = reinterpret_cast<RpLight*>(reinterpret_cast<RwUInt8*>(link) - 0x34);
        if (light == nullptr) {
            break;
        }
        if (callBack(light, data) == nullptr) {
            break;
        }
    }
    return TRUE;
}


RpWorldSector* _rpWorldSectorForAllLocalLights(RpWorldSector* sector, RpLightCallBack callBack, void* data) {
    // Узел списка lightsInWorldSector = {RwLLLink; RpLight* по смещению +0x08} - указатель на свет
    // читается по link+0x08 (не как RwLight::inWorld!). Терминатор - сентинел сектора.
    RpWorldSectorGta* ws = reinterpret_cast<RpWorldSectorGta*>(sector);
    for (RwLLLink* link = ws->lightsInWorldSector.link.next;
         link != &ws->lightsInWorldSector.link;
         link = link->next) {
        RpLight* light = *reinterpret_cast<RpLight**>(reinterpret_cast<RwUInt8*>(link) + 0x08);
        if (light == nullptr) {
            break;
        }
        if (callBack(light, data) == nullptr) {
            break;
        }
    }
    return sector;
}


RpWorldSector* _rpSectorDefaultRenderCallBack(RpWorldSector* sector) {
    RpWorldSectorGta* ws = reinterpret_cast<RpWorldSectorGta*>(sector);
    if (ws->numTriangles == 0) {
        return sector;
    }
    RxPipeline* pipeline = ws->pipeline;
    if (pipeline == nullptr) {
        RpWorldGta* world = static_cast<RpWorldGta*>(RwEngineInstance->curWorld);
        pipeline = world->pipeline;
        if (pipeline == nullptr) {
            pipeline = *reinterpret_cast<RxPipeline**>(
                reinterpret_cast<RwUInt8*>(RwEngineInstance) + worldSectorPipelineGlobalsOffset + 0x40);
        }
    }
    if (RxPipelineExecute(pipeline, sector, TRUE) == nullptr) {
        return nullptr;
    }
    return sector;
}


RpWorld* RpWorldLock(RpWorld* world) {
    // Обход BSP без рекурсии: у каждого листа уничтожить сгенерированный меш (deinstance).
    RpSectorBSP* stack[64];
    RwInt32      depth = 0;
    RpSectorBSP* cur = reinterpret_cast<RpSectorBSP*>(reinterpret_cast<RpWorldGta*>(world)->rootSector);
    if (cur == nullptr) {
        return nullptr;
    }
    do {
        if (cur->type < 0) {
            RpWorldSectorGta* ws = reinterpret_cast<RpWorldSectorGta*>(cur);
            if (ws->mesh != nullptr) {
                _rpMeshDestroy(ws->mesh);
                ws->mesh = nullptr;
            }
            cur = stack[depth--];
        } else {
            depth++;
            stack[depth] = cur->rightSubTree;
            cur = cur->leftSubTree;
        }
    } while (depth >= 0);
    return world;
}


RpWorld* RpWorldUnlock(RpWorld* world) {
    RpWorldGta* w = reinterpret_cast<RpWorldGta*>(world);
    RpSectorBSP* stack[64];
    RwInt32      depth = 0;
    RpSectorBSP* cur = reinterpret_cast<RpSectorBSP*>(w->rootSector);
    if (cur == nullptr) {
        return nullptr;
    }
    do {
        if (cur->type < 0) {
            RpWorldSectorGta* ws = reinterpret_cast<RpWorldSectorGta*>(cur);
            RpMaterial** materials = w->matList.materials;
            const RwUInt16 matListWindowBase = ws->matListWindowBase;
            if (ws->mesh == nullptr) {
                RpBuildMesh* buildMesh = _rpBuildMeshCreate(
                    static_cast<RwUInt32>(static_cast<RwInt16>(ws->numTriangles)));
                if (buildMesh == nullptr) {
                    return nullptr;
                }
                RwUInt16 numTextures = 0;   // уникальные RpMaterial::texture
                RwUInt16 numRasters = 0;    // уникальные texture->raster (первое dword RwTexture)
                RwUInt16 numSurfaces = 0;   // уникальные material->surfaceProps (первое dword @+0x08)
                if (w->matList.numMaterials != 0) {
                    const size_t arraySize = static_cast<size_t>(w->matList.numMaterials) * 4;
                    RwUInt32* textureKeys = static_cast<RwUInt32*>(RwGlobalsMem()->rwmalloc(arraySize, 0x10006));
                    RwUInt32* rasterKeys  = static_cast<RwUInt32*>(RwGlobalsMem()->rwmalloc(arraySize, 0x10507));
                    RwUInt32* surfaceKeys = static_cast<RwUInt32*>(RwGlobalsMem()->rwmalloc(arraySize, 0x10507));
                    for (RwInt32 triIndex = 0; triIndex < static_cast<RwInt32>(ws->numTriangles); triIndex++) {
                        // Треугольник вендорской сборки = 8 байт: RwUInt16 v0, v1, v2, matId.
                        const RwUInt16* tri = reinterpret_cast<const RwUInt16*>(
                            reinterpret_cast<const RwUInt8*>(ws->triangles) + static_cast<size_t>(triIndex) * 8);
                        RpMaterial* material = *reinterpret_cast<RpMaterial**>(
                            reinterpret_cast<RwUInt8*>(materials)
                                + (static_cast<RwUInt32>(matListWindowBase) + static_cast<RwUInt32>(tri[3])) * 4);
                        const RwUInt32 textureKey = *reinterpret_cast<RwUInt32*>(material);
                        RwUInt32 textureIndex = 0;
                        while (static_cast<RwUInt16>(textureIndex) < numTextures) {
                            if (textureKeys[textureIndex] == textureKey) {
                                break;
                            }
                            textureIndex++;
                        }
                        if (static_cast<RwUInt16>(textureIndex) == numTextures) {
                            textureKeys[textureIndex] = textureKey;
                            numTextures++;
                        }
                        RwUInt32 rasterKey = 0;
                        if (textureKey != 0) {
                            rasterKey = *reinterpret_cast<RwUInt32*>(textureKey);
                        }
                        RwUInt32 rasterIndex = 0;
                        while (static_cast<RwUInt16>(rasterIndex) < numRasters) {
                            if (rasterKeys[rasterIndex] == rasterKey) {
                                break;
                            }
                            rasterIndex++;
                        }
                        if (static_cast<RwUInt16>(rasterIndex) == numRasters) {
                            rasterKeys[rasterIndex] = rasterKey;
                            numRasters++;
                        }
                        const RwUInt32 surfaceKey = reinterpret_cast<RwUInt32*>(material)[2];
                        RwUInt32 surfaceIndex = 0;
                        while (static_cast<RwUInt16>(surfaceIndex) < numSurfaces) {
                            if (surfaceKeys[surfaceIndex] == surfaceKey) {
                                break;
                            }
                            surfaceIndex++;
                        }
                        if (static_cast<RwUInt16>(surfaceIndex) == numSurfaces) {
                            surfaceKeys[surfaceIndex] = surfaceKey;
                            numSurfaces++;
                        }
                        _rpBuildMeshAddTriangle(buildMesh, material,
                                                tri[0], tri[1], tri[2],
                                                tri[3],
                                                static_cast<RwUInt16>(textureIndex),
                                                static_cast<RwUInt16>(rasterIndex),
                                                static_cast<RwUInt16>(surfaceIndex));
                    }
                    RwGlobalsMem()->rwfree(textureKeys);
                    RwGlobalsMem()->rwfree(rasterKeys);
                    RwGlobalsMem()->rwfree(surfaceKeys);
                }
                RpMeshHeader* mesh = _rpMeshOptimise(
                    buildMesh, static_cast<RwUInt32>((w->flags & rpWORLDTRISTRIP) != 0));
                if (mesh == nullptr) {
                    _rpBuildMeshDestroy(buildMesh);
                    return nullptr;
                }
                ws->mesh = mesh;
            }
            cur = stack[depth];
            depth--;
            if (depth < 0) {
                return world;
            }
        } else {
            depth++;
            stack[depth] = cur->rightSubTree;
            cur = cur->leftSubTree;
        }
    } while (true);
}


void _rpWorldRegisterWorld(RpWorld* world, RwUInt32 memorySize) {
    // engine+rpWorldGlobalsOffset+0x00 = RwFreeList* узлов, +0x04 = RwLinkList сентинел списка миров.
    // Узел аллоцируется через engine->memoryAlloc(+0x144) с hint 0x40507.
    RwUInt8* globals = reinterpret_cast<RwUInt8*>(RwEngineInstance) + rpWorldGlobalsOffset;
    RwWorldListNodeGta* node = static_cast<RwWorldListNodeGta*>(
        RwGlobalsMem()->memoryAlloc(*reinterpret_cast<RwFreeList**>(globals), 0x40507));
    if (node != nullptr) {
        RwLinkList* list = reinterpret_cast<RwLinkList*>(globals + 4);
        node->world = world;
        node->memorySize = memorySize;
        node->list.next = list->link.next;
        node->list.prev = &list->link;
        list->link.next->prev = &node->list;
        list->link.next = &node->list;
    }
}


void _rpWorldUnregisterWorld(RpWorld* world) {
    RwUInt8* globals = reinterpret_cast<RwUInt8*>(RwEngineInstance) + rpWorldGlobalsOffset;
    RwLinkList* list = reinterpret_cast<RwLinkList*>(globals + 4);
    for (RwLLLink* link = list->link.next; link != &list->link; link = link->next) {
        RwWorldListNodeGta* node = rwLLLinkGetData(link, RwWorldListNodeGta, list);
        if (node->world == world) {
            link->prev->next = link->next;
            link->next->prev = link->prev;
            RwGlobalsMem()->memoryFree(*reinterpret_cast<RwFreeList**>(globals), node);
            return;
        }
    }
}


RwBool RpWorldPluginAttach()
{
    // Engine-module registrations; pluginID = MAKECHUNKID(rwVENDORID_CRITERIONWORLD, n) == 0x5NN.
    // All 8 calls are always executed (decompile ORs the results afterwards).
    RwInt32 r1 = RwEngineRegisterPlugin(4, rwID_MATERIALMODULE,
                                        (RwPluginObjectConstructor)0x74D8E0, (RwPluginObjectDestructor)0x74D950);
    RwInt32 r2 = RwEngineRegisterPlugin(0x30, rwID_MESHMODULE,
                                        (RwPluginObjectConstructor)_rpMeshOpen, (RwPluginObjectDestructor)_rpMeshClose);
    RwInt32 r3 = RwEngineRegisterPlugin(4, rwID_GEOMETRYMODULE,
                                        (RwPluginObjectConstructor)0x74BFA0, (RwPluginObjectDestructor)0x74BFC0);
    RwInt32 r4 = RwEngineRegisterPlugin(8, rwID_CLUMPMODULE,
                                        (RwPluginObjectConstructor)0x749760, (RwPluginObjectDestructor)0x749480);
    RwInt32 r5 = RwEngineRegisterPlugin(4, rwID_LIGHTMODULE,
                                        (RwPluginObjectConstructor)0x752270, (RwPluginObjectDestructor)0x7521B0);
    RwInt32 r6 = RwEngineRegisterPlugin(0, rwID_SECTORMODULE,
                                        (RwPluginObjectConstructor)0x761C30, (RwPluginObjectDestructor)0x761C40);
    RwInt32 r7 = RwEngineRegisterPlugin(0xC, rwID_WORLDMODULE,
                                        (RwPluginObjectConstructor)0x74FEE0, (RwPluginObjectDestructor)0x74FE90);
    RwInt32 r8 = RwEngineRegisterPlugin(0, rwID_BINWORLDMODULE,
                                        (RwPluginObjectConstructor)0x7637E0, (RwPluginObjectDestructor)0x7637D0);

    if ((int)(r8 | r1 | r2 | r3 | r4 | r5 | r6 | r7) < 0) {
        return FALSE;
    }

    // Internal world-module init helpers (not part of the 849-function floor, no repo wrappers):
    // 0x750050: world global-data init (worldGlobalsDataOffset 0xC9259C + per-object chunk offsets 0xC925A4/A8/AC/B0)
    if (((RwInt32(__cdecl *)(void))0x750050)() == 0) {
        return FALSE;
    }
    // 0x749830: clump-module internals init
    if (((RwInt32(__cdecl *)(void))0x749830)() == 0) {
        return FALSE;
    }
    // 0x74EA40: world-object internals init
    return (((RwInt32(__cdecl *)(void))0x74EA40)() != 0) ? TRUE : FALSE;
}


RpWorld* RpWorldForAllClumps(RpWorld* world, RpClumpCallBack fpCallBack, void* data) {
    // ВАЖНО (ABI вендора): звено клампа в world->clumpList лежит по clump+0x08
    // (clump = link - 0x08), а НЕ по RpClump::inWorldLink из repo rpworld.h (SDK-смещение 0x20).
    RpWorldGta* w = reinterpret_cast<RpWorldGta*>(world);
    for (RwLLLink* link = w->clumpList.link.next;
         link != &w->clumpList.link;
         link = link->next) {
        RpClump* clump = reinterpret_cast<RpClump*>(reinterpret_cast<RwUInt8*>(link) - 0x08);
        if (fpCallBack(clump, data) == nullptr) {
            break;
        }
    }
    return world;
}


RpWorld* RpWorldForAllMaterials(RpWorld* world, RpMaterialCallBack fpCallBack, void* data) {
    RpWorldGta* w = reinterpret_cast<RpWorldGta*>(world);
    // GTA RpMaterialList: materials@+0x00, numMaterials@+0x04 (инвертирован относительно SDK).
    for (RwInt32 i = 0; i < w->matList.numMaterials; i++) {
        if (fpCallBack(w->matList.materials[i], data) == nullptr) {
            break;
        }
    }
    return world;
}


RpWorld* RpWorldForAllLights(RpWorld* world, RpLightCallBack fpCallBack, void* data) {
    RpWorldGta* w = reinterpret_cast<RpWorldGta*>(world);
    // Вендор идёт СНАЧАЛА по directionalLightList (глобальный свет), затем по lightList.
    // Звено в обоих списках - RpLight::inWorld @0x34 (light = link - 0x34).
    RwBool stop = FALSE;
    for (RwLLLink* link = w->directionalLightList.link.next;
         link != &w->directionalLightList.link && !stop;
         link = link->next) {
        RpLight* light = reinterpret_cast<RpLight*>(reinterpret_cast<RwUInt8*>(link) - 0x34);
        if (light == nullptr) {
            break;
        }
        if (fpCallBack(light, data) == nullptr) {
            stop = TRUE;
        }
    }
    if (!stop) {
        for (RwLLLink* link = w->lightList.link.next;
             link != &w->lightList.link;
             link = link->next) {
            RpLight* light = reinterpret_cast<RpLight*>(reinterpret_cast<RwUInt8*>(link) - 0x34);
            if (light == nullptr) {
                break;
            }
            if (fpCallBack(light, data) == nullptr) {
                break;
            }
        }
    }
    return world;
}


RpWorld* RpWorldForAllWorldSectors(RpWorld* world, RpWorldSectorCallBack fpCallBack, void* data) {
    RpSectorBSP* stack[64];
    RwInt32      depth = 0;
    RpSectorBSP* cur = reinterpret_cast<RpSectorBSP*>(reinterpret_cast<RpWorldGta*>(world)->rootSector);
    do {
        if (cur->type < 0) {
            if (fpCallBack(reinterpret_cast<RpWorldSector*>(cur), data) == nullptr) {
                return world;
            }
            cur = stack[depth--];
        } else {
            depth++;
            stack[depth] = cur->rightSubTree;
            cur = cur->leftSubTree;
        }
    } while (depth >= 0);
    return world;
}


RpWorld* RpWorldRender(RpWorld* world) {
    // ABI-расхождение с repo-хедером: вендорская RwCameraForAllSectorsInFrustum(0x751660) первым
    // аргументом получает RwGlobals* (RwEngineInstance), а не камеру (внутри берёт curCamera@+0).
    // Сигнатуру оставляем по repo-хедеру, движок передаём кастом. Второй аргумент - внутренний
    // статический сектор-коллбек вендора 0x74F590 (вне этажа, в rw_addr_map отсутствует).
    RwCameraForAllSectorsInFrustum(reinterpret_cast<RwCamera*>(RwEngineInstance),
                                   reinterpret_cast<RpWorldSectorCallBack>(0x74F590),
                                   world);
    return world;
}


RwBool RpWorldDestroy(RpWorld* world) {
    // 1) Выйти из глобального списка миров (в декомпиле тело _rpWorldUnregisterWorld инлайнено,
    //    поведение идентично вызову обёртки 0x74F140).
    _rpWorldUnregisterWorld(world);

    // 2) Деинстансить меши всех секторов (в декомпиле инлайнено тело обхода RpWorldLock 0x74F1A0).
    {
        RpSectorBSP* stack[64];
        RwInt32      depth = 0;
        RpSectorBSP* cur = reinterpret_cast<RpSectorBSP*>(reinterpret_cast<RpWorldGta*>(world)->rootSector);
        if (cur != nullptr) {
            do {
                if (cur->type < 0) {
                    RpWorldSectorGta* ws = reinterpret_cast<RpWorldSectorGta*>(cur);
                    if (ws->mesh != nullptr) {
                        _rpMeshDestroy(ws->mesh);
                        ws->mesh = nullptr;
                    }
                    cur = stack[depth--];
                } else {
                    depth++;
                    stack[depth] = cur->rightSubTree;
                    cur = cur->leftSubTree;
                }
            } while (depth >= 0);
        }
    }

    // 3) Уничтожить список материалов (world+0x10).
    RpWorldGta* w = reinterpret_cast<RpWorldGta*>(world);
    _rpMaterialListDeinitialize(reinterpret_cast<RpMaterialList*>(&w->matList));

    // 4) rpWORLDSINGLEMALLOC (object.privateFlags bit0): сектора вырезаны из единого блока
    //    аллокации мира - только деинстанс; иначе рекурсивно уничтожить каждый сектор.
    if ((w->object.object.privateFlags & rpWORLDSINGLEMALLOC) != 0) {
        if (w->rootSector != nullptr) {
            _rpWorldSectorDeinstanceAll(w->rootSector);
        }
        _rwPluginRegistryDeInitObject(&worldTKList, world);
        RwGlobalsMem()->rwfree(world);
        return TRUE;
    }
    if (w->rootSector != nullptr) {
        _rpWorldSectorDestroyRecurse(w->rootSector);
    }
    _rwPluginRegistryDeInitObject(&worldTKList, world);
    RwGlobalsMem()->rwfree(world);
    return TRUE;
}


RpWorld* RpWorldCreate(RwBBox* boundingBox) {
    RpWorldGta* world = static_cast<RpWorldGta*>(
        RwGlobalsMem()->rwmalloc(worldTKList.sizeOfStruct, 0x3000B));
    if (world == nullptr) {
        RwError rwError;
        rwError.pluginID = 2;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, &worldTKList);
        RwErrorSet(&rwError);
        return nullptr;
    }
    world->object.object.type = rpWORLD;
    world->object.object.subType = 0;
    world->object.object.flags = 0;
    world->object.object.privateFlags = 0;
    world->object.object.parent = nullptr;
    _rpMaterialListInitialize(reinterpret_cast<RpMaterialList*>(&world->matList));
    world->flags = 0;
    world->renderOrder = static_cast<RpWorldRenderOrder>(2);

    RpWorldSectorGta* rootSector = static_cast<RpWorldSectorGta*>(
        RwGlobalsMem()->rwmalloc(worldSectorTKList.sizeOfStruct, 0x3000B));
    if (rootSector == nullptr) {
        RwError rwError;
        rwError.pluginID = 2;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, 4);
        RwErrorSet(&rwError);
        RwGlobalsMem()->rwfree(world);
        return nullptr;
    }
    // Корневой сектор-лист (type == -1).
    rwLinkListInitialize(&rootSector->collAtomicsInWorldSector);
    rwLinkListInitialize(&rootSector->lightsInWorldSector);
    rootSector->type = -1;
    rootSector->repEntry = nullptr;
    rootSector->mesh = nullptr;
    rootSector->numVertices = 0;
    rootSector->numTriangles = 0;
    rootSector->vertices = nullptr;
    rootSector->triangles = nullptr;
    rootSector->normals = nullptr;
    for (RwInt32 i = 0; i < 8; i++) {
        rootSector->field_10[i] = 0;
    }
    rootSector->field_30 = 0;
    rootSector->boundingBox = *boundingBox;
    rootSector->tightBoundingBox = *boundingBox;
    rootSector->pipeline = nullptr;

    world->rootSector = reinterpret_cast<RpSector*>(rootSector);
    world->numTexCoordSets = 0;
    world->numClumpsInWorld = 0;
    world->currentClumpLink = &world->clumpList.link;
    rwLinkListInitialize(&world->clumpList);
    rwLinkListInitialize(&world->lightList);
    rwLinkListInitialize(&world->directionalLightList);
    world->worldOrigin.x = 0.0f;
    world->worldOrigin.y = 0.0f;
    world->worldOrigin.z = 0.0f;
    world->boundingBox = *boundingBox;
    world->renderCallBack = _rpSectorDefaultRenderCallBack;
    world->pipeline = nullptr;

    // Регистрация мира (в декомпиле тело _rpWorldRegisterWorld инлайнено; memorySize = sizeOfStruct реестра).
    _rpWorldRegisterWorld(world, worldTKList.sizeOfStruct);
    _rwPluginRegistryInitObject(&worldTKList, world);
    _rwPluginRegistryInitObject(&worldSectorTKList, rootSector);

    if (RpWorldUnlock(world) != nullptr) {
        return world;
    }
    // Неудача Unlock: unregister + зачистка (в декомпиле инлайнены тела _rpWorldUnregisterWorld,
    // обхода деинстанса из RpWorldLock и ветки RpWorldDestroy).
    _rpWorldUnregisterWorld(world);
    {
        RpSectorBSP* stack[64];
        RwInt32      depth = 0;
        RpSectorBSP* cur = reinterpret_cast<RpSectorBSP*>(world->rootSector);
        if (cur != nullptr) {
            do {
                if (cur->type < 0) {
                    RpWorldSectorGta* ws = reinterpret_cast<RpWorldSectorGta*>(cur);
                    if (ws->mesh != nullptr) {
                        _rpMeshDestroy(ws->mesh);
                        ws->mesh = nullptr;
                    }
                    cur = stack[depth--];
                } else {
                    depth++;
                    stack[depth] = cur->rightSubTree;
                    cur = cur->leftSubTree;
                }
            } while (depth >= 0);
        }
    }
    _rpMaterialListDeinitialize(reinterpret_cast<RpMaterialList*>(&world->matList));
    if ((world->object.object.privateFlags & rpWORLDSINGLEMALLOC) != 0) {
        if (world->rootSector != nullptr) {
            _rpWorldSectorDeinstanceAll(world->rootSector);
        }
        _rwPluginRegistryDeInitObject(&worldTKList, world);
        RwGlobalsMem()->rwfree(world);
        return nullptr;
    }
    if (world->rootSector != nullptr) {
        _rpWorldSectorDestroyRecurse(world->rootSector);
    }
    _rwPluginRegistryDeInitObject(&worldTKList, world);
    RwGlobalsMem()->rwfree(world);
    return nullptr;
}


RpWorld* RpWorldSetSectorRenderCallBack(RpWorld* world, RpWorldSectorCallBackRender fpCallBack) {
    // ABI-расхождение: вендорская функция 0x74F730 возвращает void (декомпил), repo-хедер
    // объявляет RpWorld* (как в SDK) - возвращаем world для совместимости с хедером.
    if (fpCallBack == nullptr) {
        fpCallBack = _rpSectorDefaultRenderCallBack;
    }
    reinterpret_cast<RpWorldGta*>(world)->renderCallBack = fpCallBack;
    return world;
}


RpWorldSectorCallBackRender RpWorldGetSectorRenderCallBack(const RpWorld* world) {
    return reinterpret_cast<RpWorldGta*>(const_cast<RpWorld*>(world))->renderCallBack;
}


RpWorld* RpWorldSectorGetWorld(const RpWorldSector* sector) {
    // Для каждого зарегистрированного мира:
    //  - не single-malloc (object.privateFlags bit0 = 0): точечный поиск сектора обходом BSP;
    //  - single-malloc: сектора вырезаны из одного блока, достаточно проверки диапазона
    //    [world, world + node->memorySize).
    RwUInt8* globals = reinterpret_cast<RwUInt8*>(RwEngineInstance) + rpWorldGlobalsOffset;
    RwLinkList* list = reinterpret_cast<RwLinkList*>(globals + 4);
    for (RwLLLink* link = list->link.next; link != &list->link; link = link->next) {
        RwWorldListNodeGta* node = rwLLLinkGetData(link, RwWorldListNodeGta, list);
        RpWorldGta* world = node->world;
        RpSectorBSP* root = reinterpret_cast<RpSectorBSP*>(world->rootSector);
        if ((*reinterpret_cast<RwUInt32*>(&world->object) & 0x1000000) == 0) {
            RpSectorBSP* stack[64];
            RwInt32      depth = 0;
            RpSectorBSP* cur = root;
            do {
                if (cur->type < 0) {
                    if (cur == reinterpret_cast<const RpSectorBSP*>(sector)) {
                        return world;
                    }
                    cur = stack[depth--];
                } else {
                    depth++;
                    stack[depth] = cur->rightSubTree;
                    cur = cur->leftSubTree;
                }
            } while (depth >= 0);
        } else if (root <= reinterpret_cast<const RpSectorBSP*>(sector)
                   && reinterpret_cast<const RpSectorBSP*>(sector)
                      < reinterpret_cast<RpSectorBSP*>(reinterpret_cast<RwUInt8*>(root) + node->memorySize)) {
            return world;
        }
    }
    return nullptr;
}


RwInt32 RpWorldRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return _rwPluginRegistryAddPlugin(&worldTKList, size, pluginID, constructCB, destructCB, copyCB);
}


RwInt32 RpWorldRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return _rwPluginRegistryAddPluginStream(&worldTKList, pluginID, readCB, writeCB, getSizeCB);
}


RwInt32 RpWorldSetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB) {
    return _rwPluginRegistryAddPlgnStrmlwysCB(&worldTKList, pluginID, alwaysCB);
}


RwInt32 RpWorldSetStreamRightsCallBack(RwUInt32 pluginID, RwPluginDataChunkRightsCallBack rightsCB) {
    return _rwPluginRegistryAddPlgnStrmRightsCB(&worldTKList, pluginID, rightsCB);
}


RwInt32 RpWorldGetPluginOffset(RwUInt32 pluginID) {
    return _rwPluginRegistryGetPluginOffset(&worldTKList, pluginID);
}


RwBool RpWorldValidatePlugins(RpWorld* world) {
    // ABI-расхождение: вендорская функция 0x74FD90 не имеет аргументов (декомпил FUN_0074fd90(void))
    // и всегда возвращает 1. Repo-сигнатура с (RpWorld*) сохранена - при cdecl лишний аргумент
    // безвреден; параметр world игнорируется, как и в оригинале.
    (void)(world);
    return TRUE;
}


void RpD3D9GeometrySetUsageFlags(RpGeometry* geometry, RpD3D9GeometryUsageFlag flags)
{
    *(RpD3D9GeometryUsageFlag*)((RwUInt8*)geometry + rwD3D9RpWorldGlobals.geometryUsageChunkOffset) = flags;
}


RpD3D9GeometryUsageFlag RpD3D9GeometryGetUsageFlags(const RpGeometry* geometry)
{
    return *(const RpD3D9GeometryUsageFlag*)((RwUInt8*)geometry + rwD3D9RpWorldGlobals.geometryUsageChunkOffset);
}


void RpD3D9WorldSectorSetUsageFlags(RpWorldSector* sector, RpD3D9WorldSectorUsageFlag flags)
{
    *(RpD3D9WorldSectorUsageFlag*)((RwUInt8*)sector + rwD3D9RpWorldGlobals.worldSectorUsageChunkOffset) = flags;
}


RpD3D9WorldSectorUsageFlag RpD3D9WorldSectorGetUsageFlags(const RpWorldSector* sector)
{
    return *(const RpD3D9WorldSectorUsageFlag*)((RwUInt8*)sector + rwD3D9RpWorldGlobals.worldSectorUsageChunkOffset);
}


void RxD3D9AllInOneSetInstanceCallBack(RxPipelineNode* node, RxD3D9AllInOneInstanceCallBack callback)
{
    ((RxD3D9AllInOneCallBacksSA*)node->privateData)->instanceCB = callback;   // *(node+0x14) -> +0x00
}


RxD3D9AllInOneInstanceCallBack RxD3D9AllInOneGetInstanceCallBack(RxPipelineNode* node)
{
    return ((RxD3D9AllInOneCallBacksSA*)node->privateData)->instanceCB;
}


void RxD3D9AllInOneSetReinstanceCallBack(RxPipelineNode* node, RxD3D9AllInOneReinstanceCallBack callback)
{
    ((RxD3D9AllInOneCallBacksSA*)node->privateData)->reinstanceCB = callback;   // *(node+0x14) -> +0x04
}


RxD3D9AllInOneReinstanceCallBack RxD3D9AllInOneGetReinstanceCallBack(RxPipelineNode* node)
{
    return ((RxD3D9AllInOneCallBacksSA*)node->privateData)->reinstanceCB;
}


void RxD3D9AllInOneSetLightingCallBack(RxPipelineNode* node, RxD3D9AllInOneLightingCallBack callback)
{
    ((RxD3D9AllInOneCallBacksSA*)node->privateData)->lightingCB = callback;   // *(node+0x14) -> +0x08
}


RxD3D9AllInOneLightingCallBack RxD3D9AllInOneGetLightingCallBack(RxPipelineNode* node)
{
    return ((RxD3D9AllInOneCallBacksSA*)node->privateData)->lightingCB;
}


void RxD3D9AllInOneSetRenderCallBack(RxPipelineNode* node, RxD3D9AllInOneRenderCallBack callback)
{
    ((RxD3D9AllInOneCallBacksSA*)node->privateData)->renderCB = callback;   // *(node+0x14) -> +0x0C
}


RxD3D9AllInOneRenderCallBack RxD3D9AllInOneGetRenderCallBack(RxPipelineNode* node)
{
    return ((RxD3D9AllInOneCallBacksSA*)node->privateData)->renderCB;
}


struct rpD3D9DynamicGeometryEntryGta
{
    RpGeometry*                  geometry;   // +0x00
    rpD3D9DynamicGeometryEntryGta* next;     // +0x04
};
static rpD3D9DynamicGeometryEntryGta*& g_rpD3D9DynamicGeometryList = StaticRef<rpD3D9DynamicGeometryEntryGta*>(0xC92520);
static RwFreeList*&                    g_rpD3D9DynamicGeometryFreeList = StaticRef<RwFreeList*>(0xC92524);
// ==== END SHARED ====

void _rpD3D9AddDynamicGeometry(RpGeometry* geometry) {
    rpD3D9DynamicGeometryEntryGta* entry = g_rpD3D9DynamicGeometryList;
    while (true) {
        if (entry == nullptr) {
            // create the entry freelist once: ((RwFreeList*(__cdecl*)(RwInt32, RwInt32, RwInt32, RwUInt32))0x801980)(8, 0x80, 4, 0x3050d)
            // (vendor 0x801980 is the 4-arg flavor == CreateAndPreallocateSpace(...,1,NULL,...);
            //  repo 6-arg wrapper ignores file/line and performs the identical call)
            if (g_rpD3D9DynamicGeometryFreeList == nullptr) {
                g_rpD3D9DynamicGeometryFreeList = ((RwFreeList*(__cdecl*)(RwInt32, RwInt32, RwInt32, RwUInt32))0x801980)(8, 0x80, 4, 0x3050d);
            }
            // decompile: RwGlobals +0x144 freelist-heap alloc(fl, 0x3050d)
            entry = (rpD3D9DynamicGeometryEntryGta*)
                    (*(void* (__cdecl**)(RwFreeList*, RwUInt32))((RwUInt8*)RwEngineInstance + 0x144))
                    (g_rpD3D9DynamicGeometryFreeList, 0x3050d);
            entry->geometry = geometry;
            entry->next     = g_rpD3D9DynamicGeometryList;
            g_rpD3D9DynamicGeometryList = entry;
            return;
        }
        if (entry->geometry == geometry) {
            return;                              // already registered
        }
        entry = entry->next;
    }
}


void _rpD3D9RemoveDynamicGeometry(RpGeometry* geometry) {
    rpD3D9DynamicGeometryEntryGta* entry = g_rpD3D9DynamicGeometryList;
    if (entry != nullptr) {
        rpD3D9DynamicGeometryEntryGta* prev = nullptr;
        while (entry->geometry != geometry) {
            prev  = entry;
            entry = entry->next;
            if (entry == nullptr) {
                return;                          // not registered
            }
        }
        if (entry == g_rpD3D9DynamicGeometryList) {
            g_rpD3D9DynamicGeometryList = entry->next;
        } else {
            prev->next = entry->next;
        }
        entry->geometry = nullptr;
        // decompile: RwGlobals +0x148 freelist-heap free(fl, entry)
        (*(RwFreeList* (__cdecl**)(RwFreeList*, void*))((RwUInt8*)RwEngineInstance + 0x148))
            (g_rpD3D9DynamicGeometryFreeList, entry);
    }
}


#ifndef RPWORLD7_D3D9_DECL_SHARED
#define RPWORLD7_D3D9_DECL_SHARED
// Таблица размеров элемента по типу (D3DDECLTYPE). Прочитана из .rdata 0x874EF8
// gta_sa_compact.exe (файл-офсет 0x47CCF8, проверено побайтово):
// 4,8,12,16,4,4,4,8,4,4,8,4,8,4,4,4,8
static const RwUInt32 gtaD3D9DeclTypeSizeTable[17] = {
    4,  8,  12, 16,   // 0..3   FLOAT1..FLOAT4
    4,  4,  4,  8,    // 4..7   D3DCOLOR, UBYTE4, SHORT2, SHORT4
    4,  4,  8,        // 8..10  UBYTE4N, SHORT2N, SHORT4N
    4,  8,  4,  4,    // 11..14 GTA-расширения: 2*i16, 4*i16, DEC-10-10-10, DEC-10-10-10(масштаб 1/511)
    4,  8             // 15..16 (в декомпилах случаев нет)
};
// Ghidra ROUND(x) = x87 FISTP при FPU control word по умолчанию = округление к ближайшему чётному
// (проверено по бинару: в 0x752AD0 49 inline-FISTP, вызовов _ftol нет). Точный C-эквивалент:
// (RwInt32)lrintf(x) — MSVC компилирует в cvtss2si с MXCSR RC=nearest.
#endif
// [rpworld-7 SHARED BLOCK END]

RwUInt32 _rpD3D9VertexDeclarationGetSize(RwUInt32 type) {
    // декомпил: return *(u32*)(&DAT_00874ef8 + type*4);  (проверки границ нет)
    return gtaD3D9DeclTypeSizeTable[type];
}


#ifndef RPWORLD7_D3D9_DECL_SHARED
#define RPWORLD7_D3D9_DECL_SHARED
static const RwUInt32 gtaD3D9DeclTypeSizeTable[17] = {
    4,  8,  12, 16,   // 0..3   FLOAT1..FLOAT4
    4,  4,  4,  8,    // 4..7   D3DCOLOR, UBYTE4, SHORT2, SHORT4
    4,  4,  8,        // 8..10  UBYTE4N, SHORT2N, SHORT4N
    4,  8,  4,  4,    // 11..14 GTA-расширения: 2*i16, 4*i16, DEC-10-10-10, DEC-10-10-10(1/511)
    4,  8             // 15..16
};
#endif
// [rpworld-7 SHARED BLOCK END]

RwUInt32 _rpD3D9VertexDeclarationGetStride(const void* vertexDeclaration) {
    // декомпил: сумма размеров типов по элементам D3DVERTEXELEMENT9 (8 байт на элемент,
    // байт Type @ +4), пока Type < 0x11 (D3DDECLTYPE_UNUSED)
    const D3DVERTEXELEMENT9* elements = (const D3DVERTEXELEMENT9*)vertexDeclaration;
    RwUInt32 stride = 0;
    RwUInt8  type   = elements[0].Type;
    while (type < 0x11 /*D3DDECLTYPE_UNUSED*/) {
        stride += gtaD3D9DeclTypeSizeTable[type];
        ++elements;
        type = elements->Type;
    }
    return stride;
}


#ifndef RPWORLD7_D3D9_DECL_SHARED
#define RPWORLD7_D3D9_DECL_SHARED
static const RwUInt32 gtaD3D9DeclTypeSizeTable[17] = {
    4,  8,  12, 16,   // 0..3   FLOAT1..FLOAT4
    4,  4,  4,  8,    // 4..7   D3DCOLOR, UBYTE4, SHORT2, SHORT4
    4,  4,  8,        // 8..10  UBYTE4N, SHORT2N, SHORT4N
    4,  8,  4,  4,    // 11..14 GTA-расширения
    4,  8             // 15..16
};
#endif
// [rpworld-7 SHARED BLOCK END]

RwUInt32 _rpD3D9VertexDeclarationInstV3d(RwUInt32 type, RwUInt8* mem, const RwV3d* src, RwInt32 numVerts, RwUInt32 stride) {
    RwUInt8*     dst = mem;
    const RwV3d* v   = src;
    switch (type) {
    case 0:  // D3DDECLTYPE_FLOAT1
        for (RwInt32 i = 0; i < numVerts; i++) {
            *(RwReal*)dst = v->x;
            dst += stride;
            v++;
        }
        break;
    case 1:  // D3DDECLTYPE_FLOAT2
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwReal*)dst)[0] = v->x;
            ((RwReal*)dst)[1] = v->y;
            dst += stride;
            v++;
        }
        break;
    case 2:  // D3DDECLTYPE_FLOAT3
        if (stride == 12) {
            memcpy(dst, v, (size_t)numVerts * 12);   // rep movsd + tail (см. шапку)
            break;
        }
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwReal*)dst)[0] = v->x;
            ((RwReal*)dst)[1] = v->y;
            ((RwReal*)dst)[2] = v->z;
            dst += stride;
            v++;
        }
        break;
    case 3:  // D3DDECLTYPE_FLOAT4 (+w=1)
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwReal*)dst)[0] = v->x;
            ((RwReal*)dst)[1] = v->y;
            ((RwReal*)dst)[2] = v->z;
            ((RwReal*)dst)[3] = 1.0f;
            dst += stride;
            v++;
        }
        break;
    case 4:  // D3DDECLTYPE_D3DCOLOR: [b=z,g=y,r=x,a=255] *255
        for (RwInt32 i = 0; i < numVerts; i++) {
            dst[0] = (RwUInt8)lrintf(v->z * 255.0f /*0x859AAC*/);
            dst[1] = (RwUInt8)lrintf(v->y * 255.0f);
            dst[2] = (RwUInt8)lrintf(v->x * 255.0f);
            dst[3] = 0xFF;
            dst += stride;
            v++;
        }
        break;
    case 5:  // D3DDECLTYPE_UBYTE4: [x,y,z,1] (усечение+округление к ближайшему)
        for (RwInt32 i = 0; i < numVerts; i++) {
            dst[0] = (RwUInt8)lrintf(v->x);
            dst[1] = (RwUInt8)lrintf(v->y);
            dst[2] = (RwUInt8)lrintf(v->z);
            dst[3] = 1;
            dst += stride;
            v++;
        }
        break;
    case 6:  // D3DDECLTYPE_SHORT2
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf(v->x);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf(v->y);
            dst += stride;
            v++;
        }
        break;
    case 7:  // D3DDECLTYPE_SHORT4 (+w=1)
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf(v->x);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf(v->y);
            ((RwUInt16*)dst)[2] = (RwUInt16)lrintf(v->z);
            ((RwUInt16*)dst)[3] = 1;
            dst += stride;
            v++;
        }
        break;
    case 8:  // D3DDECLTYPE_UBYTE4N: [x,y,z,255] *255
        for (RwInt32 i = 0; i < numVerts; i++) {
            dst[0] = (RwUInt8)lrintf(v->x * 255.0f);
            dst[1] = (RwUInt8)lrintf(v->y * 255.0f);
            dst[2] = (RwUInt8)lrintf(v->z * 255.0f);
            dst[3] = 0xFF;
            dst += stride;
            v++;
        }
        break;
    case 9:  // D3DDECLTYPE_SHORT2N *32767
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf(v->x * 32767.0f /*0x86B2C0*/);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf(v->y * 32767.0f);
            dst += stride;
            v++;
        }
        break;
    case 10: // D3DDECLTYPE_SHORT4N *32767 (+w=0x7FFF)
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf(v->x * 32767.0f);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf(v->y * 32767.0f);
            ((RwUInt16*)dst)[2] = (RwUInt16)lrintf(v->z * 32767.0f);
            ((RwUInt16*)dst)[3] = 0x7FFF;
            dst += stride;
            v++;
        }
        break;
    case 11: // GTA: 2*u16 *65535
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf(v->x * 65535.0f /*0x8729E8*/);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf(v->y * 65535.0f);
            dst += stride;
            v++;
        }
        break;
    case 12: // GTA: 4*u16 *65535 (+w=0xFFFF)
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf(v->x * 65535.0f);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf(v->y * 65535.0f);
            ((RwUInt16*)dst)[2] = (RwUInt16)lrintf(v->z * 65535.0f);
            ((RwUInt16*)dst)[3] = 0xFFFF;
            dst += stride;
            v++;
        }
        break;
    case 13: // GTA: DEC-10-10-10: (((z<<10)|y)<<10)|x = z<<20 | y<<10 | x, без масштаба
        for (RwInt32 i = 0; i < numVerts; i++) {
            *(RwUInt32*)dst = ((((RwUInt32)lrintf(v->z) & 0x3FF) << 10)
                              | ((RwUInt32)lrintf(v->y) & 0x3FF)) << 10
                              | ((RwUInt32)lrintf(v->x) & 0x3FF);
            dst += stride;
            v++;
        }
        break;
    case 14: // GTA: DEC-10-10-10 *511: z<<20 | y<<10 | x
        for (RwInt32 i = 0; i < numVerts; i++) {
            *(RwUInt32*)dst = ((((RwUInt32)lrintf(v->z * 511.0f /*0x874F3C*/) & 0x3FF) << 10)
                              | ((RwUInt32)lrintf(v->y * 511.0f) & 0x3FF)) << 10
                              | ((RwUInt32)lrintf(v->x * 511.0f) & 0x3FF);
            dst += stride;
            v++;
        }
        break;
    }
    return gtaD3D9DeclTypeSizeTable[type];
}


#ifndef RPWORLD7_D3D9_DECL_SHARED
#define RPWORLD7_D3D9_DECL_SHARED
static const RwUInt32 gtaD3D9DeclTypeSizeTable[17] = {
    4,  8,  12, 16,   // 0..3   FLOAT1..FLOAT4
    4,  4,  4,  8,    // 4..7   D3DCOLOR, UBYTE4, SHORT2, SHORT4
    4,  4,  8,        // 8..10  UBYTE4N, SHORT2N, SHORT4N
    4,  8,  4,  4,    // 11..14 GTA-расширения
    4,  8             // 15..16
};
#endif
// [rpworld-7 SHARED BLOCK END]

RwUInt32 _rpD3D9VertexDeclarationInstV3dComp(RwUInt32 type, RwUInt8* mem, const RpVertexNormal* src, RwInt32 numVerts, RwUInt32 stride) {
    RwUInt8*             dst = mem;
    const RpVertexNormal* n  = src;
    switch (type) {
    case 0:  // FLOAT1: только x
        for (RwInt32 i = 0; i < numVerts; i++) {
            *(RwReal*)dst = (RwReal)n->x * 0.0078125f /*0x858B88 = 1/128*/;
            dst += stride;
            n++;
        }
        break;
    case 1:  // FLOAT2: x,y
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwReal*)dst)[0] = (RwReal)n->x * 0.0078125f;
            ((RwReal*)dst)[1] = (RwReal)n->y * 0.0078125f;
            dst += stride;
            n++;
        }
        break;
    case 2:  // FLOAT3: x,y,z
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwReal*)dst)[0] = (RwReal)n->x * 0.0078125f;
            ((RwReal*)dst)[1] = (RwReal)n->y * 0.0078125f;
            ((RwReal*)dst)[2] = (RwReal)n->z * 0.0078125f;
            dst += stride;
            n++;
        }
        break;
    case 3:  // FLOAT4: x,y,z,1
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwReal*)dst)[0] = (RwReal)n->x * 0.0078125f;
            ((RwReal*)dst)[1] = (RwReal)n->y * 0.0078125f;
            ((RwReal*)dst)[2] = (RwReal)n->z * 0.0078125f;
            ((RwReal*)dst)[3] = 1.0f;
            dst += stride;
            n++;
        }
        break;
    case 4:  // D3DCOLOR: [z,y,x,255] *1/128
        for (RwInt32 i = 0; i < numVerts; i++) {
            dst[0] = (RwUInt8)lrintf((RwReal)n->z * 0.0078125f * 255.0f /*0x859AAC*/);
            dst[1] = (RwUInt8)lrintf((RwReal)n->y * 0.0078125f * 255.0f);
            dst[2] = (RwUInt8)lrintf((RwReal)n->x * 0.0078125f * 255.0f);
            dst[3] = 0xFF;
            dst += stride;
            n++;
        }
        break;
    case 5:  // UBYTE4: [x,y,z,1]
        for (RwInt32 i = 0; i < numVerts; i++) {
            dst[0] = (RwUInt8)lrintf((RwReal)n->x * 0.0078125f);
            dst[1] = (RwUInt8)lrintf((RwReal)n->y * 0.0078125f);
            dst[2] = (RwUInt8)lrintf((RwReal)n->z * 0.0078125f);
            dst[3] = 1;
            dst += stride;
            n++;
        }
        break;
    case 6:  // SHORT2: x,y
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf((RwReal)n->x * 0.0078125f);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf((RwReal)n->y * 0.0078125f);
            dst += stride;
            n++;
        }
        break;
    case 7:  // SHORT4: x,y,z,1
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf((RwReal)n->x * 0.0078125f);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf((RwReal)n->y * 0.0078125f);
            ((RwUInt16*)dst)[2] = (RwUInt16)lrintf((RwReal)n->z * 0.0078125f);
            ((RwUInt16*)dst)[3] = 1;
            dst += stride;
            n++;
        }
        break;
    case 8:  // UBYTE4N: [x,y,z,255] *1/128
        for (RwInt32 i = 0; i < numVerts; i++) {
            dst[0] = (RwUInt8)lrintf((RwReal)n->x * 0.0078125f * 255.0f);
            dst[1] = (RwUInt8)lrintf((RwReal)n->y * 0.0078125f * 255.0f);
            dst[2] = (RwUInt8)lrintf((RwReal)n->z * 0.0078125f * 255.0f);
            dst[3] = 0xFF;
            dst += stride;
            n++;
        }
        break;
    case 9:  // SHORT2N: x,y *1/128 *32767
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf((RwReal)n->x * 0.0078125f * 32767.0f /*0x86B2C0*/);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf((RwReal)n->y * 0.0078125f * 32767.0f);
            dst += stride;
            n++;
        }
        break;
    case 10: // SHORT4N: x,y,z *1/128 *32767 (+w=0x7FFF)
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf((RwReal)n->x * 0.0078125f * 32767.0f);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf((RwReal)n->y * 0.0078125f * 32767.0f);
            ((RwUInt16*)dst)[2] = (RwUInt16)lrintf((RwReal)n->z * 0.0078125f * 32767.0f);
            ((RwUInt16*)dst)[3] = 0x7FFF;
            dst += stride;
            n++;
        }
        break;
    case 11: // GTA 2*u16: x,y *1/128 *65535
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf((RwReal)n->x * 0.0078125f * 65535.0f /*0x8729E8*/);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf((RwReal)n->y * 0.0078125f * 65535.0f);
            dst += stride;
            n++;
        }
        break;
    case 12: // GTA 4*u16: x,y,z *1/128 *65535 (+w=0xFFFF)
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf((RwReal)n->x * 0.0078125f * 65535.0f);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf((RwReal)n->y * 0.0078125f * 65535.0f);
            ((RwUInt16*)dst)[2] = (RwUInt16)lrintf((RwReal)n->z * 0.0078125f * 65535.0f);
            ((RwUInt16*)dst)[3] = 0xFFFF;
            dst += stride;
            n++;
        }
        break;
    case 13: // GTA DEC-10-10-10: (((z<<10)|y)<<10)|x, *1/128 без доп. масштаба
        for (RwInt32 i = 0; i < numVerts; i++) {
            *(RwUInt32*)dst = ((((RwUInt32)lrintf((RwReal)n->z * 0.0078125f) & 0x3FF) << 10)
                              | ((RwUInt32)lrintf((RwReal)n->y * 0.0078125f) & 0x3FF)) << 10
                              | ((RwUInt32)lrintf((RwReal)n->x * 0.0078125f) & 0x3FF);
            dst += stride;
            n++;
        }
        break;
    case 14: // GTA DEC-10-10-10 *1/128 *511
        for (RwInt32 i = 0; i < numVerts; i++) {
            *(RwUInt32*)dst = ((((RwUInt32)lrintf((RwReal)n->z * 0.0078125f * 511.0f /*0x874F3C*/) & 0x3FF) << 10)
                              | ((RwUInt32)lrintf((RwReal)n->y * 0.0078125f * 511.0f) & 0x3FF)) << 10
                              | ((RwUInt32)lrintf((RwReal)n->x * 0.0078125f * 511.0f) & 0x3FF);
            dst += stride;
            n++;
        }
        break;
    }
    return gtaD3D9DeclTypeSizeTable[type];
}


#ifndef RPWORLD7_D3D9_DECL_SHARED
#define RPWORLD7_D3D9_DECL_SHARED
static const RwUInt32 gtaD3D9DeclTypeSizeTable[17] = {
    4,  8,  12, 16,   // 0..3   FLOAT1..FLOAT4
    4,  4,  4,  8,    // 4..7   D3DCOLOR, UBYTE4, SHORT2, SHORT4
    4,  4,  8,        // 8..10  UBYTE4N, SHORT2N, SHORT4N
    4,  8,  4,  4,    // 11..14 GTA-расширения
    4,  8             // 15..16
};
#endif
// [rpworld-7 SHARED BLOCK END]

RwUInt32 _rpD3D9VertexDeclarationInstV3dMorph(RwUInt32 type, RwUInt8* mem, const RwV3d* src1, const RwV3d* src2, RwReal scale, RwInt32 numVerts, RwUInt32 stride) {
    RwUInt8*     dst = mem;
    const RwV3d* v1  = src1;
    const RwV3d* v2  = src2;
    switch (type) {
    case 0:  // FLOAT1: только x
        for (RwInt32 i = 0; i < numVerts; i++) {
            *(RwReal*)dst = (v2->x - v1->x) * scale + v1->x;
            dst += stride;
            v1++; v2++;
        }
        break;
    case 1:  // FLOAT2
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwReal*)dst)[0] = (v2->x - v1->x) * scale + v1->x;
            ((RwReal*)dst)[1] = (v2->y - v1->y) * scale + v1->y;
            dst += stride;
            v1++; v2++;
        }
        break;
    case 2:  // FLOAT3
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwReal*)dst)[0] = (v2->x - v1->x) * scale + v1->x;
            ((RwReal*)dst)[1] = (v2->y - v1->y) * scale + v1->y;
            ((RwReal*)dst)[2] = (v2->z - v1->z) * scale + v1->z;
            dst += stride;
            v1++; v2++;
        }
        break;
    case 3:  // FLOAT4 (+w=1)
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwReal*)dst)[0] = (v2->x - v1->x) * scale + v1->x;
            ((RwReal*)dst)[1] = (v2->y - v1->y) * scale + v1->y;
            ((RwReal*)dst)[2] = (v2->z - v1->z) * scale + v1->z;
            ((RwReal*)dst)[3] = 1.0f;
            dst += stride;
            v1++; v2++;
        }
        break;
    case 4: { // D3DCOLOR: [b=z,g=y,r=x,a=255]; A=scale*255, B=(1-scale)*255
        RwReal a = scale * 255.0f /*0x859AAC*/;
        RwReal b = (1.0f /*0x858624*/ - scale) * 255.0f;
        for (RwInt32 i = 0; i < numVerts; i++) {
            dst[0] = (RwUInt8)lrintf(a * v2->z + b * v1->z);
            dst[1] = (RwUInt8)lrintf(a * v2->y + b * v1->y);
            dst[2] = (RwUInt8)lrintf(a * v2->x + b * v1->x);
            dst[3] = 0xFF;
            dst += stride;
            v1++; v2++;
        }
        break;
    }
    case 5:  // UBYTE4: [x,y,z,1]
        for (RwInt32 i = 0; i < numVerts; i++) {
            dst[0] = (RwUInt8)lrintf((v2->x - v1->x) * scale + v1->x);
            dst[1] = (RwUInt8)lrintf((v2->y - v1->y) * scale + v1->y);
            dst[2] = (RwUInt8)lrintf((v2->z - v1->z) * scale + v1->z);
            dst[3] = 1;
            dst += stride;
            v1++; v2++;
        }
        break;
    case 6:  // SHORT2
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf((v2->x - v1->x) * scale + v1->x);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf((v2->y - v1->y) * scale + v1->y);
            dst += stride;
            v1++; v2++;
        }
        break;
    case 7:  // SHORT4 (+w=1)
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf((v2->x - v1->x) * scale + v1->x);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf((v2->y - v1->y) * scale + v1->y);
            ((RwUInt16*)dst)[2] = (RwUInt16)lrintf((v2->z - v1->z) * scale + v1->z);
            ((RwUInt16*)dst)[3] = 1;
            dst += stride;
            v1++; v2++;
        }
        break;
    case 8: { // UBYTE4N: [x,y,z,255]; A=scale*255, B=(1-scale)*255
        RwReal a = scale * 255.0f;
        RwReal b = (1.0f - scale) * 255.0f;
        for (RwInt32 i = 0; i < numVerts; i++) {
            dst[0] = (RwUInt8)lrintf(a * v2->x + b * v1->x);
            dst[1] = (RwUInt8)lrintf(a * v2->y + b * v1->y);
            dst[2] = (RwUInt8)lrintf(a * v2->z + b * v1->z);
            dst[3] = 0xFF;
            dst += stride;
            v1++; v2++;
        }
        break;
    }
    case 9: { // SHORT2N; A=scale*32767, B=(1-scale)*32767
        RwReal a = scale * 32767.0f /*0x86B2C0*/;
        RwReal b = (1.0f - scale) * 32767.0f;
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf(a * v2->x + b * v1->x);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf(a * v2->y + b * v1->y);
            dst += stride;
            v1++; v2++;
        }
        break;
    }
    case 10: { // SHORT4N (+w=0x7FFF)
        RwReal a = scale * 32767.0f;
        RwReal b = (1.0f - scale) * 32767.0f;
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf(a * v2->x + b * v1->x);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf(a * v2->y + b * v1->y);
            ((RwUInt16*)dst)[2] = (RwUInt16)lrintf(a * v2->z + b * v1->z);
            ((RwUInt16*)dst)[3] = 0x7FFF;
            dst += stride;
            v1++; v2++;
        }
        break;
    }
    case 11: { // GTA 2*u16
        RwReal a = scale * 65535.0f /*0x8729E8*/;
        RwReal b = (1.0f - scale) * 65535.0f;
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf(a * v2->x + b * v1->x);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf(a * v2->y + b * v1->y);
            dst += stride;
            v1++; v2++;
        }
        break;
    }
    case 12: { // GTA 4*u16 (+w=0xFFFF)
        RwReal a = scale * 65535.0f;
        RwReal b = (1.0f - scale) * 65535.0f;
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf(a * v2->x + b * v1->x);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf(a * v2->y + b * v1->y);
            ((RwUInt16*)dst)[2] = (RwUInt16)lrintf(a * v2->z + b * v1->z);
            ((RwUInt16*)dst)[3] = 0xFFFF;
            dst += stride;
            v1++; v2++;
        }
        break;
    }
    case 13: // DEC-10-10-10 без масштаба: (((z<<10)|y)<<10)|x от морфнутого значения
        for (RwInt32 i = 0; i < numVerts; i++) {
            RwReal mx = (v2->x - v1->x) * scale + v1->x;
            RwReal my = (v2->y - v1->y) * scale + v1->y;
            RwReal mz = (v2->z - v1->z) * scale + v1->z;
            *(RwUInt32*)dst = ((((RwUInt32)lrintf(mz) & 0x3FF) << 10)
                              | ((RwUInt32)lrintf(my) & 0x3FF)) << 10
                              | ((RwUInt32)lrintf(mx) & 0x3FF);
            dst += stride;
            v1++; v2++;
        }
        break;
    case 14: { // DEC-10-10-10 *511
        RwReal a = scale * 511.0f /*0x874F3C*/;
        RwReal b = (1.0f - scale) * 511.0f;
        for (RwInt32 i = 0; i < numVerts; i++) {
            *(RwUInt32*)dst = ((((RwUInt32)lrintf(a * v2->z + b * v1->z) & 0x3FF) << 10)
                              | ((RwUInt32)lrintf(a * v2->y + b * v1->y) & 0x3FF)) << 10
                              | ((RwUInt32)lrintf(a * v2->x + b * v1->x) & 0x3FF);
            dst += stride;
            v1++; v2++;
        }
        break;
    }
    }
    return gtaD3D9DeclTypeSizeTable[type];
}


#ifndef RPWORLD7_D3D9_DECL_SHARED
#define RPWORLD7_D3D9_DECL_SHARED
static const RwUInt32 gtaD3D9DeclTypeSizeTable[17] = {
    4,  8,  12, 16,   // 0..3   FLOAT1..FLOAT4
    4,  4,  4,  8,    // 4..7   D3DCOLOR, UBYTE4, SHORT2, SHORT4
    4,  4,  8,        // 8..10  UBYTE4N, SHORT2N, SHORT4N
    4,  8,  4,  4,    // 11..14 GTA-расширения
    4,  8             // 15..16
};
#endif
// [rpworld-7 SHARED BLOCK END]

RwUInt32 _rpD3D9VertexDeclarationInstV2d(RwUInt32 type, RwUInt8* mem, const RwV2d* src, RwInt32 numVerts, RwUInt32 stride) {
    RwUInt8*     dst = mem;
    const RwV2d* t   = src;
    switch (type) {
    case 0:  // FLOAT1: только u
        for (RwInt32 i = 0; i < numVerts; i++) {
            *(RwReal*)dst = t->x;
            dst += stride;
            t++;
        }
        break;
    case 1:  // FLOAT2
        if (stride == 8) {
            // декомпил: размотанный поплавковый цикл numVerts*2 (эквивалент memcpy(numVerts*8))
            RwInt32 n = numVerts * 2;
            const RwReal* s = (const RwReal*)t;
            RwReal* d = (RwReal*)dst;
            while (n-- != 0) {
                *d++ = *s++;
            }
            break;
        }
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwReal*)dst)[0] = t->x;
            ((RwReal*)dst)[1] = t->y;
            dst += stride;
            t++;
        }
        break;
    case 2:  // FLOAT3: u,v,0
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwReal*)dst)[0] = t->x;
            ((RwReal*)dst)[1] = t->y;
            ((RwReal*)dst)[2] = 0.0f;
            dst += stride;
            t++;
        }
        break;
    case 3:  // FLOAT4: u,v,0,1
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwReal*)dst)[0] = t->x;
            ((RwReal*)dst)[1] = t->y;
            ((RwReal*)dst)[2] = 0.0f;
            ((RwReal*)dst)[3] = 1.0f;
            dst += stride;
            t++;
        }
        break;
    case 4:  // D3DCOLOR: [0, v, u, 255] *255
        for (RwInt32 i = 0; i < numVerts; i++) {
            dst[0] = 0;
            dst[1] = (RwUInt8)lrintf(t->y * 255.0f /*0x859AAC*/);
            dst[2] = (RwUInt8)lrintf(t->x * 255.0f);
            dst[3] = 0xFF;
            dst += stride;
            t++;
        }
        break;
    case 5:  // UBYTE4: [u,v,0,1]
        for (RwInt32 i = 0; i < numVerts; i++) {
            dst[0] = (RwUInt8)lrintf(t->x);
            dst[1] = (RwUInt8)lrintf(t->y);
            dst[2] = 0;
            dst[3] = 1;
            dst += stride;
            t++;
        }
        break;
    case 6:  // SHORT2
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf(t->x);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf(t->y);
            dst += stride;
            t++;
        }
        break;
    case 7:  // SHORT4: u,v,0,1
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf(t->x);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf(t->y);
            ((RwUInt16*)dst)[2] = 0;
            ((RwUInt16*)dst)[3] = 1;
            dst += stride;
            t++;
        }
        break;
    case 8:  // UBYTE4N: [u,v,0,255] *255
        for (RwInt32 i = 0; i < numVerts; i++) {
            dst[0] = (RwUInt8)lrintf(t->x * 255.0f);
            dst[1] = (RwUInt8)lrintf(t->y * 255.0f);
            dst[2] = 0;
            dst[3] = 0xFF;
            dst += stride;
            t++;
        }
        break;
    case 9:  // SHORT2N *32767
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf(t->x * 32767.0f /*0x86B2C0*/);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf(t->y * 32767.0f);
            dst += stride;
            t++;
        }
        break;
    case 10: // SHORT4N: u,v *32767 (+z=0, w=0x7FFF)
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf(t->x * 32767.0f);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf(t->y * 32767.0f);
            ((RwUInt16*)dst)[2] = 0;
            ((RwUInt16*)dst)[3] = 0x7FFF;
            dst += stride;
            t++;
        }
        break;
    case 11: // GTA 2*u16 *65535
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf(t->x * 65535.0f /*0x8729E8*/);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf(t->y * 65535.0f);
            dst += stride;
            t++;
        }
        break;
    case 12: // GTA 4*u16: u,v *65535 (+z=0, w=0xFFFF)
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf(t->x * 65535.0f);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf(t->y * 65535.0f);
            ((RwUInt16*)dst)[2] = 0;
            ((RwUInt16*)dst)[3] = 0xFFFF;
            dst += stride;
            t++;
        }
        break;
    case 13: // DEC-2x10: (((v<<10)|u)) без масштаба
        for (RwInt32 i = 0; i < numVerts; i++) {
            *(RwUInt32*)dst = ((RwUInt32)lrintf(t->y) & 0x3FF) << 10
                            | ((RwUInt32)lrintf(t->x) & 0x3FF);
            dst += stride;
            t++;
        }
        break;
    case 14: // DEC-2x10 *511
        for (RwInt32 i = 0; i < numVerts; i++) {
            *(RwUInt32*)dst = ((RwUInt32)lrintf(t->y * 511.0f /*0x874F3C*/) & 0x3FF) << 10
                            | ((RwUInt32)lrintf(t->x * 511.0f) & 0x3FF);
            dst += stride;
            t++;
        }
        break;
    }
    return gtaD3D9DeclTypeSizeTable[type];
}


RwBool _rpD3D9VertexDeclarationInstColor(RwUInt8* mem, const RwRGBA* color, RwInt32 numVerts, RwUInt32 stride) {
    RwUInt8*     dst = mem;
    const RwRGBA* c  = color;
    RwUInt8 alpha = 0xFF;
    for (RwInt32 i = 0; i < numVerts; i++) {
        // декомпил: *(u32*)dst = a<<24 | r<<16 | g<<8 | b  (D3DCOLOR ARGB; librw instColor: dst[0]=blue..dst[3]=alpha)
        dst[0] = c->blue;
        dst[1] = c->green;
        dst[2] = c->red;
        dst[3] = c->alpha;
        alpha  = (RwUInt8)(alpha & c->alpha);
        dst += stride;
        c++;
    }
    // декомпил: return alpha != 0xFF  — есть ли в прелайте хоть один полупрозрачный вертекс
    return alpha != 0xFF;
}


#ifndef RPWORLD7_D3D9_DECL_SHARED
#define RPWORLD7_D3D9_DECL_SHARED
static const RwUInt32 gtaD3D9DeclTypeSizeTable[17] = {
    4,  8,  12, 16,   // 0..3   FLOAT1..FLOAT4
    4,  4,  4,  8,    // 4..7   D3DCOLOR, UBYTE4, SHORT2, SHORT4
    4,  4,  8,        // 8..10  UBYTE4N, SHORT2N, SHORT4N
    4,  8,  4,  4,    // 11..14 GTA-расширения
    4,  8             // 15..16
};
#endif
// [rpworld-7 SHARED BLOCK END]

RwUInt32 _rpD3D9VertexDeclarationInstIndices(RwUInt32 type, RwUInt8* mem, const RwUInt32* indices, RwInt32 numVerts, RwUInt32 stride) {
    RwUInt8*     dst = mem;
    const RwUInt8* idx = (const RwUInt8*)indices;   // декомпил читает u32-массив побайтово
    switch (type) {
    case 4:  // D3DCOLOR: [i2, i1, i0, i3]
        for (RwInt32 i = 0; i < numVerts; i++) {
            dst[0] = idx[2];
            dst[1] = idx[1];
            dst[2] = idx[0];
            dst[3] = idx[3];
            dst  += stride;
            idx  += 4;
        }
        break;
    case 5:  // UBYTE4: [i0, i1, i2, i3]
        for (RwInt32 i = 0; i < numVerts; i++) {
            dst[0] = idx[0];
            dst[1] = idx[1];
            dst[2] = idx[2];
            dst[3] = idx[3];
            dst  += stride;
            idx  += 4;
        }
        break;
    case 6:  // SHORT2: 2*u16 из i0,i1
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)idx[0];
            ((RwUInt16*)dst)[1] = (RwUInt16)idx[1];
            dst += stride;
            idx += 4;
        }
        break;
    case 7:  // SHORT4: 4*u16 из i0..i3
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)idx[0];
            ((RwUInt16*)dst)[1] = (RwUInt16)idx[1];
            ((RwUInt16*)dst)[2] = (RwUInt16)idx[2];
            ((RwUInt16*)dst)[3] = (RwUInt16)idx[3];
            dst += stride;
            idx += 4;
        }
        break;
    }
    return gtaD3D9DeclTypeSizeTable[type];
}


#ifndef RPWORLD7_D3D9_DECL_SHARED
#define RPWORLD7_D3D9_DECL_SHARED
static const RwUInt32 gtaD3D9DeclTypeSizeTable[17] = {
    4,  8,  12, 16,   // 0..3   FLOAT1..FLOAT4
    4,  4,  4,  8,    // 4..7   D3DCOLOR, UBYTE4, SHORT2, SHORT4
    4,  4,  8,        // 8..10  UBYTE4N, SHORT2N, SHORT4N
    4,  8,  4,  4,    // 11..14 GTA-расширения
    4,  8             // 15..16
};
#endif
// [rpworld-7 SHARED BLOCK END]

RwUInt32 _rpD3D9VertexDeclarationInstIndicesRemap(RwUInt32 type, RwUInt8* mem, const RwUInt32* indices, const RwUInt8* remap, RwInt32 numVerts, RwUInt32 stride) {
    RwUInt8*       dst = mem;
    const RwUInt8* idx = (const RwUInt8*)indices;   // побайтовый взгляд, как в декомпиле
    switch (type) {
    case 4:  // D3DCOLOR: [r2, r1, r0, r3]
        for (RwInt32 i = 0; i < numVerts; i++) {
            dst[0] = remap[idx[2]];
            dst[1] = remap[idx[1]];
            dst[2] = remap[idx[0]];
            dst[3] = remap[idx[3]];
            dst  += stride;
            idx  += 4;
        }
        break;
    case 5:  // UBYTE4: [r0, r1, r2, r3]
        for (RwInt32 i = 0; i < numVerts; i++) {
            dst[0] = remap[idx[0]];
            dst[1] = remap[idx[1]];
            dst[2] = remap[idx[2]];
            dst[3] = remap[idx[3]];
            dst  += stride;
            idx  += 4;
        }
        break;
    case 6:  // SHORT2: 2*u16 из r0,r1
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)remap[idx[0]];
            ((RwUInt16*)dst)[1] = (RwUInt16)remap[idx[1]];
            dst += stride;
            idx += 4;
        }
        break;
    case 7:  // SHORT4: 4*u16 из r0..r3
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)remap[idx[0]];
            ((RwUInt16*)dst)[1] = (RwUInt16)remap[idx[1]];
            ((RwUInt16*)dst)[2] = (RwUInt16)remap[idx[2]];
            ((RwUInt16*)dst)[3] = (RwUInt16)remap[idx[3]];
            dst += stride;
            idx += 4;
        }
        break;
    }
    return gtaD3D9DeclTypeSizeTable[type];
}


#ifndef RPWORLD7_D3D9_DECL_SHARED
#define RPWORLD7_D3D9_DECL_SHARED
static const RwUInt32 gtaD3D9DeclTypeSizeTable[17] = {
    4,  8,  12, 16,   // 0..3   FLOAT1..FLOAT4
    4,  4,  4,  8,    // 4..7   D3DCOLOR, UBYTE4, SHORT2, SHORT4
    4,  4,  8,        // 8..10  UBYTE4N, SHORT2N, SHORT4N
    4,  8,  4,  4,    // 11..14 GTA-расширения
    4,  8             // 15..16
};
#endif
// [rpworld-7 SHARED BLOCK END]

RwUInt32 _rpD3D9VertexDeclarationInstWeights(RwUInt32 type, RwUInt8* mem, const RwV4d* src, RwInt32 numVerts, RwUInt32 stride) {
    RwUInt8*     dst = mem;
    const RwV4d* w   = src;
    switch (type) {
    case 0:  // FLOAT1: только w0
        for (RwInt32 i = 0; i < numVerts; i++) {
            *(RwReal*)dst = w->x;
            dst += stride;
            w++;
        }
        break;
    case 1:  // FLOAT2: w0,w1
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwReal*)dst)[0] = w->x;
            ((RwReal*)dst)[1] = w->y;
            dst += stride;
            w++;
        }
        break;
    case 2:  // FLOAT3: w0..w2
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwReal*)dst)[0] = w->x;
            ((RwReal*)dst)[1] = w->y;
            ((RwReal*)dst)[2] = w->z;
            dst += stride;
            w++;
        }
        break;
    case 3:  // FLOAT4: w0..w3
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwReal*)dst)[0] = w->x;
            ((RwReal*)dst)[1] = w->y;
            ((RwReal*)dst)[2] = w->z;
            ((RwReal*)dst)[3] = w->w;
            dst += stride;
            w++;
        }
        break;
    case 4:  // D3DCOLOR: [w2, w1, 255-w2-w1-w3, w3] *255; при w0>=1 -> [0,0,0,0xFF]
        for (RwInt32 i = 0; i < numVerts; i++) {
            if (w->x < 1.0f /*0x858624*/) {
                dst[1] = (RwUInt8)lrintf(w->y * 255.0f /*0x859AAC*/);
                dst[0] = (RwUInt8)lrintf(w->z * 255.0f);
                dst[3] = (RwUInt8)lrintf(w->w * 255.0f);
                dst[2] = (RwUInt8)(-1 - dst[3] - dst[1] - dst[0]);
            } else {
                dst[2] = 0xFF;
                dst[1] = 0;
                dst[0] = 0;
                dst[3] = 0;
            }
            dst += stride;
            w++;
        }
        break;
    case 5:  // UBYTE4: [w0,w1,w2,w3] (без масштаба)
        for (RwInt32 i = 0; i < numVerts; i++) {
            dst[0] = (RwUInt8)lrintf(w->x);
            dst[1] = (RwUInt8)lrintf(w->y);
            dst[2] = (RwUInt8)lrintf(w->z);
            dst[3] = (RwUInt8)lrintf(w->w);
            dst += stride;
            w++;
        }
        break;
    case 6:  // SHORT2: w0,w1
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf(w->x);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf(w->y);
            dst += stride;
            w++;
        }
        break;
    case 7:  // SHORT4: w0..w3
        for (RwInt32 i = 0; i < numVerts; i++) {
            ((RwUInt16*)dst)[0] = (RwUInt16)lrintf(w->x);
            ((RwUInt16*)dst)[1] = (RwUInt16)lrintf(w->y);
            ((RwUInt16*)dst)[2] = (RwUInt16)lrintf(w->z);
            ((RwUInt16*)dst)[3] = (RwUInt16)lrintf(w->w);
            dst += stride;
            w++;
        }
        break;
    case 8:  // UBYTE4N: [255-w1-w2-w3, w1, w2, w3] *255; при w0>=1 -> [0xFF,0,0,0]
        for (RwInt32 i = 0; i < numVerts; i++) {
            if (w->x < 1.0f /*0x858624*/) {
                dst[1] = (RwUInt8)lrintf(w->y * 255.0f /*0x859AAC*/);
                dst[2] = (RwUInt8)lrintf(w->z * 255.0f);
                dst[3] = (RwUInt8)lrintf(w->w * 255.0f);
                dst[0] = (RwUInt8)(-1 - dst[2] - dst[3] - dst[1]);
            } else {
                dst[0] = 0xFF;
                dst[1] = 0;
                dst[2] = 0;
                dst[3] = 0;
            }
            dst += stride;
            w++;
        }
        break;
    case 9: { // SHORT2N: [w0, 0x7FFF-w0] *32767
        for (RwInt32 i = 0; i < numVerts; i++) {
            RwUInt16 s0 = (RwUInt16)lrintf(w->x * 32767.0f /*0x86B2C0*/);
            ((RwUInt16*)dst)[0] = s0;
            ((RwUInt16*)dst)[1] = (RwUInt16)(0x7FFF - s0);
            dst += stride;
            w++;
        }
        break;
    }
    case 10: { // SHORT4N: [0x7FFF-w1-w2-w3, w1, w2, w3] *32767
        for (RwInt32 i = 0; i < numVerts; i++) {
            RwUInt16 s1 = (RwUInt16)lrintf(w->y * 32767.0f);
            RwUInt16 s2 = (RwUInt16)lrintf(w->z * 32767.0f);
            RwUInt16 s3 = (RwUInt16)lrintf(w->w * 32767.0f);
            ((RwUInt16*)dst)[2] = s2;
            ((RwUInt16*)dst)[1] = s1;
            ((RwUInt16*)dst)[3] = s3;
            ((RwUInt16*)dst)[0] = (RwUInt16)((0x7FFF - s3) - s2 - s1);
            dst += stride;
            w++;
        }
        break;
    }
    case 11: { // GTA 2*u16: [w0, -1-w0] *65535
        for (RwInt32 i = 0; i < numVerts; i++) {
            RwUInt16 s0 = (RwUInt16)lrintf(w->x * 65535.0f /*0x8729E8*/);
            ((RwUInt16*)dst)[0] = s0;
            ((RwUInt16*)dst)[1] = (RwUInt16)(-1 - s0);
            dst += stride;
            w++;
        }
        break;
    }
    case 12: { // GTA 4*u16: [-1-w1-w2-w3, w1, w2, w3] *65535
        for (RwInt32 i = 0; i < numVerts; i++) {
            RwUInt16 s1 = (RwUInt16)lrintf(w->y * 65535.0f);
            RwUInt16 s2 = (RwUInt16)lrintf(w->z * 65535.0f);
            RwUInt16 s3 = (RwUInt16)lrintf(w->w * 65535.0f);
            ((RwUInt16*)dst)[2] = s2;
            ((RwUInt16*)dst)[1] = s1;
            ((RwUInt16*)dst)[3] = s3;
            ((RwUInt16*)dst)[0] = (RwUInt16)((-1 - s3) - s2 - s1);
            dst += stride;
            w++;
        }
        break;
    }
    case 13: // DEC-3x10: w2<<20 | w1<<10 | w0 (без масштаба)
        for (RwInt32 i = 0; i < numVerts; i++) {
            *(RwUInt32*)dst = ((((RwUInt32)lrintf(w->z) & 0x3FF) << 10)
                              | ((RwUInt32)lrintf(w->y) & 0x3FF)) << 10
                              | ((RwUInt32)lrintf(w->x) & 0x3FF);
            dst += stride;
            w++;
        }
        break;
    case 14: // DEC-3x10 *511 с ветвлением по w2 (см. шапку)
        for (RwInt32 i = 0; i < numVerts; i++) {
            RwUInt32 packed;
            if (0.0019569471f /*0x874F40 = 1/511*/ <= w->z) {
                RwUInt32 p1 = (RwUInt32)lrintf(w->y * 511.0f /*0x874F3C*/) & 0x3FF;
                RwUInt32 p2 = (RwUInt32)lrintf(w->z * 511.0f) & 0x3FF;
                packed = ((p2 << 10) | p1) << 10 | ((0x1FF - p2) - p1);
            } else {
                RwUInt32 p0 = (RwUInt32)lrintf(w->x * 511.0f) & 0x3FF;
                packed = (p0 * -0x400u + 0x7FC00u) | p0;
            }
            *(RwUInt32*)dst = packed;
            dst += stride;
            w++;
        }
        break;
    }
    return gtaD3D9DeclTypeSizeTable[type];
}


RwUInt32 _rpD3D9VertexDeclarationInstTangent(RwUInt32 type, RwUInt8* mem, const RwV3d* pos, const RwTexCoords* texCoord, RxD3D9ResEntryHeader* meshHeader, RwUInt32 stride) {
    RwGlobals* engine = RwEngineInstance;
    RwMemoryFunctions* memFuncs = (RwMemoryFunctions*)((RwUInt8*)engine + 0x134);
    RwInt32 numVerts = (RwInt32)meshHeader->totalNumVertex;

    RwV3d* tangents = (RwV3d*)memFuncs->rwmalloc((size_t)((RwUInt32)numVerts * 12), 0x1050D);
    memset(tangents, 0, (size_t)((RwUInt32)numVerts * 12));   // rep stosd/stosb по total&3

    RwUInt16* indices = nullptr;
    meshHeader->indexBuffer->Lock(0, meshHeader->totalNumIndex * 2, (void**)&indices, 0);   // vtable+0x2C

    RxD3D9InstanceData* mesh = (RxD3D9InstanceData*)(meshHeader + 1);               // +0x40
    for (RwUInt32 m = 0; m < meshHeader->numMeshes; m++, mesh++) {
        const RwUInt16* idxBase = indices + mesh->startIndex;
        RwInt32 triCount = (RwInt32)mesh->numIndex;
        if (meshHeader->primType == 5 /*D3DPT_TRIANGLESTRIP*/) {
            triCount -= 2;
        }
        for (RwInt32 t = 0; t < triCount; t++) {
            RwUInt32 i0, i1, i2;
            if (meshHeader->primType == 4 /*D3DPT_TRIANGLELIST*/) {
                i0 = idxBase[t * 3 + 0];
                i1 = idxBase[t * 3 + 1];
                i2 = idxBase[t * 3 + 2];
            } else {
                // strip: чётные (i0,i1,i2), нечётные (i1,i2,i0); декомпил: if (t&1)==0 {...} else {...}
                if ((t & 1) == 0) {
                    i0 = idxBase[t + 0];
                    i1 = idxBase[t + 1];
                    i2 = idxBase[t + 2];
                } else {
                    i0 = idxBase[t + 1];
                    i1 = idxBase[t + 2];
                    i2 = idxBase[t + 0];
                }
                if (i0 == i1 || i0 == i2 || i1 == i2) {
                    continue;   // вырожденный треугольник — пропуск (в list-пути проверки НЕТ, как в декомпиле)
                }
            }
            i0 += mesh->minVert;
            i1 += mesh->minVert;
            i2 += mesh->minVert;

            const RwV3d&       p0  = pos[i0];
            const RwV3d&       p1  = pos[i1];
            const RwV3d&       p2  = pos[i2];
            const RwTexCoords& t0  = texCoord[i0];
            const RwTexCoords& t1  = texCoord[i1];
            const RwTexCoords& t2  = texCoord[i2];

            RwReal dx10 = p1.x - p0.x;
            RwReal dy20 = p2.y - p0.y;
            RwReal dx20 = p2.x - p0.x;
            RwReal dv20 = t2.v - t0.v;
            RwReal du10 = t1.u - t0.u;
            RwReal dv10 = t1.v - t0.v;
            RwReal du20 = t2.u - t0.u;

            RwReal det = dv20 * du10 - du20 * dv10;
            if (9.99999996e-13f /*0x874F44*/ < fabs(det)) {
                RwReal r = 1.0f /*0x858624*/ / det;
                RwReal tx = r * (dx20 * dv10 - dv20 * dx10);
                RwReal ty = r * (dy20 * dv10 - dv20 * (p1.y - p0.y));
                RwReal tz = r * ((p2.z - p0.z) * dv10 - dv20 * (p1.z - p0.z));
                tangents[i0].x -= tx; tangents[i1].x -= tx; tangents[i2].x -= tx;
                tangents[i0].y -= ty; tangents[i1].y -= ty; tangents[i2].y -= ty;
                tangents[i0].z -= tz; tangents[i1].z -= tz; tangents[i2].z -= tz;
            }
        }
    }
    meshHeader->indexBuffer->Unlock();   // vtable+0x30

    // нормализация: порядок слагаемых len = z*z + x*x + y*y — как в декомпиле
    for (RwInt32 i = 0; i < numVerts; i++) {
        RwV3d& v = tangents[i];
        RwReal len = v.z * v.z + v.x * v.x + v.y * v.y;
        RwReal r = _rwInvSqrt(len);
        v.x = r * v.x;
        v.y = r * v.y;
        v.z = r * v.z;
    }

    RwUInt32 result = _rpD3D9VertexDeclarationInstV3d(type, mem, tangents, numVerts, stride);
    memFuncs->rwfree(tangents);
    return result;
}


void _rpD3D9VertexDeclarationUnInstV3d(RwUInt32 type, RwV3d* dst, const RwUInt8* src) {
    switch (type) {
    case 0:  // FLOAT1: x, y=z=0
        dst->x = *(const RwReal*)src;
        dst->y = 0.0f;
        dst->z = 0.0f;
        return;
    case 1:  // FLOAT2: x,y, z=0
        dst->x = ((const RwReal*)src)[0];
        dst->y = ((const RwReal*)src)[1];
        dst->z = 0.0f;
        return;
    case 2:
    case 3:  // FLOAT3/FLOAT4: x,y,z
        dst->x = ((const RwReal*)src)[0];
        dst->y = ((const RwReal*)src)[1];
        dst->z = ((const RwReal*)src)[2];
        return;
    case 4:  // D3DCOLOR: [b,g,r]*1/255 -> x=r-байт, y=g, z=b (реверс)
        dst->x = (RwReal)src[2] * 0.0039215689f /*0x859A3C = 1/255*/;
        dst->y = (RwReal)src[1] * 0.0039215689f;
        dst->z = (RwReal)src[0] * 0.0039215689f;
        return;
    case 5:  // UBYTE4: [x,y,z]
        dst->x = (RwReal)src[0];
        dst->y = (RwReal)src[1];
        dst->z = (RwReal)src[2];
        return;
    case 6:  // SHORT2: x,y, z=0
        dst->x = (RwReal)(RwInt16)((const RwUInt16*)src)[0];
        dst->y = (RwReal)(RwInt16)((const RwUInt16*)src)[1];
        dst->z = 0.0f;
        return;
    case 7:  // SHORT4: x,y,z
        dst->x = (RwReal)(RwInt16)((const RwUInt16*)src)[0];
        dst->y = (RwReal)(RwInt16)((const RwUInt16*)src)[1];
        dst->z = (RwReal)(RwInt16)((const RwUInt16*)src)[2];
        return;
    case 8:  // UBYTE4N: *1/255
        dst->x = (RwReal)src[0] * 0.0039215689f;
        dst->y = (RwReal)src[1] * 0.0039215689f;
        dst->z = (RwReal)src[2] * 0.0039215689f;
        return;
    case 9:  // SHORT2N: *1/32767, z=0
        dst->x = (RwReal)(RwInt16)((const RwUInt16*)src)[0] * 3.0518509e-05f /*0x858C7C = 1/32767*/;
        dst->y = (RwReal)(RwInt16)((const RwUInt16*)src)[1] * 3.0518509e-05f;
        dst->z = 0.0f;
        return;
    case 10: // SHORT4N: *1/32767
        dst->x = (RwReal)(RwInt16)((const RwUInt16*)src)[0] * 3.0518509e-05f;
        dst->y = (RwReal)(RwInt16)((const RwUInt16*)src)[1] * 3.0518509e-05f;
        dst->z = (RwReal)(RwInt16)((const RwUInt16*)src)[2] * 3.0518509e-05f;
        return;
    case 11: // GTA 2*u16: *1/65535, z=0
        dst->x = (RwReal)(RwInt16)((const RwUInt16*)src)[0] * 1.5259022e-05f /*0x874F48 = 1/65535*/;
        dst->y = (RwReal)(RwInt16)((const RwUInt16*)src)[1] * 1.5259022e-05f;
        dst->z = 0.0f;
        return;
    case 12: // GTA 4*u16: *1/65535
        dst->x = (RwReal)(RwInt16)((const RwUInt16*)src)[0] * 1.5259022e-05f;
        dst->y = (RwReal)(RwInt16)((const RwUInt16*)src)[1] * 1.5259022e-05f;
        dst->z = (RwReal)(RwInt16)((const RwUInt16*)src)[2] * 1.5259022e-05f;
        return;
    case 13: { // DEC-10-10-10 без масштаба и без знака
        RwUInt32 v = *(const RwUInt32*)src;
        dst->x = (RwReal)(v & 0x3FF);
        dst->y = (RwReal)((v >> 10) & 0x3FF);
        dst->z = (RwReal)((v >> 20) & 0x3FF);
        return;
    }
    case 14: { // DEC-10-10-10 со знакорасширением *1/511
        RwUInt32 v   = *(const RwUInt32*)src;
        RwInt32  vx  = (RwInt32)(v & 0x3FF);
        RwInt32  vy  = ((RwInt32)v >> 10) & 0x3FF;
        RwInt32  vz  = ((RwInt32)v >> 20) & 0x3FF;
        if (vx & 0x200) vx |= (RwInt32)0xFFFFFC00;
        if (vy & 0x200) vy |= (RwInt32)0xFFFFFC00;
        if (vz & 0x200) vz |= (RwInt32)0xFFFFFC00;
        dst->x = (RwReal)vx * 0.0019569471f /*0x874F40 = 1/511*/;
        dst->y = (RwReal)vy * 0.0019569471f;
        dst->z = (RwReal)vz * 0.0019569471f;
        return;
    }
    default: // декомпил: 0,0,0
        dst->x = 0.0f;
        dst->y = 0.0f;
        dst->z = 0.0f;
        return;
    }
}


void _rpD3D9VertexDeclarationUnInstV2d(RwUInt32 type, RwV2d* dst, const RwUInt8* src) {
    switch (type) {
    case 0:  // FLOAT1: u, v=0
        dst->x = *(const RwReal*)src;
        dst->y = 0.0f;
        return;
    case 1:
    case 2:
    case 3:  // FLOAT2/3/4: u,v
        dst->x = ((const RwReal*)src)[0];
        dst->y = ((const RwReal*)src)[1];
        return;
    case 4:  // D3DCOLOR: [0, v, u, 255] -> x=r-байт, y=g-байт (реверс)
        dst->x = (RwReal)src[2] * 0.0039215689f /*0x859A3C = 1/255*/;
        dst->y = (RwReal)src[1] * 0.0039215689f;
        return;
    case 5:  // UBYTE4: [u,v,...]
        dst->x = (RwReal)src[0];
        dst->y = (RwReal)src[1];
        return;
    case 6:
    case 7:  // SHORT2/4: u,v
        dst->x = (RwReal)(RwInt16)((const RwUInt16*)src)[0];
        dst->y = (RwReal)(RwInt16)((const RwUInt16*)src)[1];
        return;
    case 8:  // UBYTE4N: *1/255
        dst->x = (RwReal)src[0] * 0.0039215689f;
        dst->y = (RwReal)src[1] * 0.0039215689f;
        return;
    case 9:
    case 10: // SHORT2N/4N: *1/32767
        dst->x = (RwReal)(RwInt16)((const RwUInt16*)src)[0] * 3.0518509e-05f /*0x858C7C = 1/32767*/;
        dst->y = (RwReal)(RwInt16)((const RwUInt16*)src)[1] * 3.0518509e-05f;
        return;
    case 11:
    case 12: // GTA 2/4*u16: *1/65535
        dst->x = (RwReal)(RwInt16)((const RwUInt16*)src)[0] * 1.5259022e-05f /*0x874F48 = 1/65535*/;
        dst->y = (RwReal)(RwInt16)((const RwUInt16*)src)[1] * 1.5259022e-05f;
        return;
    case 13: { // DEC-2x10 без знака
        RwUInt32 v = *(const RwUInt32*)src;
        dst->x = (RwReal)(v & 0x3FF);
        dst->y = (RwReal)((v >> 10) & 0x3FF);
        return;
    }
    case 14: { // DEC-2x10 со знакорасширением *1/511
        RwUInt32 v  = *(const RwUInt32*)src;
        RwInt32  vx = (RwInt32)(v & 0x3FF);
        RwInt32  vy = ((RwInt32)v >> 10) & 0x3FF;
        if (vx & 0x200) vx |= (RwInt32)0xFFFFFC00;
        if (vy & 0x200) vy |= (RwInt32)0xFFFFFC00;
        dst->x = (RwReal)vx * 0.0019569471f /*0x874F40 = 1/511*/;
        dst->y = (RwReal)vy * 0.0019569471f;
        return;
    }
    default: // декомпил: 0,0
        dst->x = 0.0f;
        dst->y = 0.0f;
        return;
    }
}


void _rpD3D9GetMinMaxValuesV3d(const RwV3d* src, RwInt32 numElements, RwV3d* min, RwV3d* max) {
    // декомпил: init из элемента 0 (безусловное чтение src[0], как в оригинале), затем
    // поточ компонентный min/max начиная с элемента 1
    min->x = src->x;
    min->y = src->y;
    min->z = src->z;
    max->x = src->x;
    max->y = src->y;
    max->z = src->z;
    if (1 < numElements) {
        const RwV3d* v = src + 1;
        for (RwInt32 i = 0; i < numElements - 1; i++, v++) {
            if (min->x <= v->x) {
                if (max->x < v->x) max->x = v->x;
            } else {
                min->x = v->x;
            }
            if (min->y <= v->y) {
                if (max->y < v->y) max->y = v->y;
            } else {
                min->y = v->y;
            }
            if (min->z <= v->z) {
                if (max->z < v->z) max->z = v->z;
            } else {
                min->z = v->z;
            }
        }
    }
}


void _rpD3D9GetMinMaxValuesV2d(const RwV2d* src, RwInt32 numElements, RwV2d* min, RwV2d* max)
{
    *min = *src;
    *max = *src;
    if (1 < numElements) {
        numElements--;
        do {
            if (min->x <= src->x) {
                if (max->x < src->x) {
                    max->x = src->x;
                }
            } else {
                min->x = src->x;
            }
            if (min->y <= src->y) {
                if (max->y < src->y) {
                    max->y = src->y;
                }
            } else {
                min->y = src->y;
            }
            src++;
            numElements--;
        } while (numElements != 0);
    }
}


RwUInt32 _rpD3D9FindFormatV3d(const RwV3d* src, RwInt32 numElements)
{
    RwV3d min, max;
    _rpD3D9GetMinMaxValuesV3d(src, numElements, &min, &max);   // 0x755830, репо-обёртка

    RwUInt32 format = D3DFVF_XYZ /* 0x2 */;
    if (max.x < 1.0f /*0x858624*/ && max.y < 1.0f && max.z < 1.0f) {
        if (min.x < 0.0f /*0x858B50*/ || min.y < 0.0f || min.z < 0.0f) {
            // отрицательные координаты допустимы только в диапазоне [-1, 1] (0x858C1C = -1.0f)
            if (min.x >= -1.0f && min.y >= -1.0f && min.z >= -1.0f) {
                if ((rwD3D9FindFormatCapsBits & 0x80) != 0) {  // декомпил: (char)caps < 0
                    return D3DFVF_XYZB5 /* 0xe */;
                }
                if ((rwD3D9FindFormatCapsBits & 0x08) != 0) {
                    format = D3DFVF_XYZB3 /* 0xa */;
                }
            }
        } else {
            if ((rwD3D9FindFormatCapsBits & 0x80) != 0) {
                return D3DFVF_XYZB5 /* 0xe */;
            }
            if ((rwD3D9FindFormatCapsBits & 0x20) != 0) {
                return D3DFVF_XYZB4 /* 0xc */;
            }
        }
    }
    return format;
}


RwUInt32 _rpD3D9FindFormatV2d(const RwV2d* src, RwInt32 numElements)
{
    RwV2d min, max;
    _rpD3D9GetMinMaxValuesV2d(src, numElements, &min, &max);   // 0x7558F0, этот же батч

    RwUInt32 format = 1u;   // декомпил: cVar1 = 1 (default)
    if (max.x < 1.0f /*0x858624*/ && max.y < 1.0f) {
        if (min.x >= 0.0f /*0x858B50*/ && min.y >= 0.0f) {
            if ((rwD3D9FindFormatCapsBits & 0x10) != 0) {
                return 0xBu;   // 11
            }
            // декомпил: (-((caps & 0x80) != 0) & 10u) + 4  -> 14 при бите, иначе 4
            return (((rwD3D9FindFormatCapsBits & 0x80) != 0 ? 0xFFFFFFFFu : 0u) & 10u) + 4u;
        }
        if (min.x >= -1.0f /*0x858C1C*/ && min.y >= -1.0f) {
            if ((rwD3D9FindFormatCapsBits & 0x04) != 0) {
                return 9u;
            }
            if ((rwD3D9FindFormatCapsBits & 0x80) != 0) {  // декомпил: (char)caps < 0
                format = 0xEu;   // 14
            }
        }
    }
    return format;
}


RxPipeline* RpWorldSetDefaultSectorPipeline(RxPipeline* pipeline) {
    static auto& worldGlobalsOffset = StaticRef<RwInt32>(0xC9BC60);

    RwUInt8* globals = reinterpret_cast<RwUInt8*>(RwEngineInstance) + worldGlobalsOffset;
    if (pipeline == nullptr)
        *reinterpret_cast<RxPipeline**>(globals + 0x40) = *reinterpret_cast<RxPipeline**>(globals + 0x58);
    else
        *reinterpret_cast<RxPipeline**>(globals + 0x40) = pipeline;
    return pipeline;    // binary wrapper discards the result (decompile: void); eax carries the pipeline
}


RxPipeline* RpAtomicSetDefaultPipeline(RxPipeline* pipeline) {
    static auto& worldGlobalsOffset = StaticRef<RwInt32>(0xC9BC60);

    RwUInt8* globals = reinterpret_cast<RwUInt8*>(RwEngineInstance) + worldGlobalsOffset;
    if (pipeline == nullptr)
        *reinterpret_cast<RxPipeline**>(globals + 0x3C) = *reinterpret_cast<RxPipeline**>(globals + 0x54);
    else
        *reinterpret_cast<RxPipeline**>(globals + 0x3C) = pipeline;
    return pipeline;
}


RxPipeline* RpMaterialSetDefaultPipeline(RxPipeline* pipeline) {
    static auto& worldGlobalsOffset = StaticRef<RwInt32>(0xC9BC60);

    RwUInt8* globals = reinterpret_cast<RwUInt8*>(RwEngineInstance) + worldGlobalsOffset;
    if (pipeline == nullptr)
        *reinterpret_cast<RxPipeline**>(globals + 0x44) = *reinterpret_cast<RxPipeline**>(globals + 0x5C);
    else
        *reinterpret_cast<RxPipeline**>(globals + 0x44) = pipeline;
    return pipeline;
}


const RpGeometry* RpGeometryIsCorrectlySorted(const RpGeometry* geometry, RwBool* result) {
    struct RwMemFuncsGta {                                              // GTA RwGlobals memory-function area
        RwUInt8  pad[0x134];
        void* (*rwmalloc)(RwUInt32 size, RwUInt32 hint);                // +0x134
        void   (*rwfree)(void* mem);                                    // +0x138
    };
    RwMemFuncsGta* mem = reinterpret_cast<RwMemFuncsGta*>(RwEngineInstance);

    struct MatSpanGta {                                                 // {min,max} u16 pair, 4 bytes per material
        RwUInt16 minVert;
        RwUInt16 maxVert;
    };

    RwUInt32 totalSpan = 0;
    if ((geometry->flags & rpGEOMETRYNATIVE /*0x01000000*/) == 0) {
        MatSpanGta* spans = static_cast<MatSpanGta*>(
            mem->rwmalloc(geometry->matList.numMaterials * sizeof(MatSpanGta), 0x1050D));
        if (spans == nullptr)
            return nullptr;

        for (RwInt32 m = 0; m < geometry->matList.numMaterials; m++) {
            spans[m].minVert = 0xFFFF;
            spans[m].maxVert = 0;
        }
        for (RwInt32 t = 0; t < geometry->numTriangles; t++) {
            const RpTriangle* tri = &geometry->triangles[t];
            for (RwInt32 c = 0; c < 3; c++) {
                const RwUInt16 v   = tri->vertIndex[c];
                MatSpanGta*     span = &spans[tri->matIndex];
                if (span->maxVert < v) span->maxVert = v;
                if (v < span->minVert) span->minVert = v;
            }
        }
        for (RwInt32 m = 0; m < geometry->matList.numMaterials; m++) {
            // unsigned u16 arithmetic exactly as in the binary (unused materials: 1 + 0 - 0xFFFF wraps down)
            totalSpan += 1u + (RwUInt32)(spans[m].maxVert - spans[m].minVert);
        }
        mem->rwfree(spans);
    }

    *result = (RwBool)(totalSpan <= (RwUInt32)geometry->numVertices);
    return geometry;
}


RpGeometry* RpGeometrySortByMaterial(const RpGeometry* geometry, RpGeometrySortByMaterialCallBack callback) {
    struct RwMemFuncsGta {                                              // GTA RwGlobals memory-function area
        RwUInt8  pad[0x134];
        void* (*rwmalloc)(RwUInt32 size, RwUInt32 hint);                // +0x134
        void   (*rwfree)(void* mem);                                    // +0x138
    };
    RwMemFuncsGta* mem = reinterpret_cast<RwMemFuncsGta*>(RwEngineInstance);

    struct SortElemGta {                                                // sort-array entry (8 bytes)
        RwUInt32 packed;                                                // (corner << 30) | triangleIndex
        RwUInt16 matIndex;                                              // copied from old triangle
        RwUInt16 vertIndex;                                             // corner vertex index
    };

    const RwUInt32 flags     = geometry->flags;
    const RwBool   hasNormals = (flags & rpGEOMETRYNORMALS /*0x10*/) != 0;

    // scratch (allocation order as in the binary)
    RwV3d**      oldNormalPtrs = static_cast<RwV3d**>(mem->rwmalloc(geometry->numMorphTargets * 4, 0x1050D));
    RwV3d**      newNormalPtrs = static_cast<RwV3d**>(mem->rwmalloc(geometry->numMorphTargets * 4, 0x1050D));
    RwV3d**      oldVertPtrs   = static_cast<RwV3d**>(mem->rwmalloc(geometry->numMorphTargets * 4, 0x1050D));
    RwV3d**      newVertPtrs   = static_cast<RwV3d**>(mem->rwmalloc(geometry->numMorphTargets * 4, 0x1050D));
    SortElemGta* sortElems     = static_cast<SortElemGta*>(
        mem->rwmalloc(geometry->numTriangles * 3 * sizeof(SortElemGta), 0x1050D));

    if ((hasNormals && (oldNormalPtrs == nullptr || newNormalPtrs == nullptr))
        || oldVertPtrs == nullptr || newVertPtrs == nullptr) {
        if (oldNormalPtrs != nullptr) mem->rwfree(oldNormalPtrs);
        if (newNormalPtrs != nullptr) mem->rwfree(newNormalPtrs);
        if (oldVertPtrs   != nullptr) mem->rwfree(oldVertPtrs);
        if (newVertPtrs   != nullptr) mem->rwfree(newVertPtrs);
        if (sortElems     != nullptr) mem->rwfree(sortElems);
        return nullptr;
    }

    // build sort elements: one per triangle corner
    for (RwInt32 t = 0; t < geometry->numTriangles; t++) {
        const RpTriangle* tri = &geometry->triangles[t];
        for (RwInt32 c = 0; c < 3; c++) {
            SortElemGta* e = &sortElems[t * 3 + c];
            e->packed    = ((RwUInt32)c << 30) | (RwUInt32)t;
            e->matIndex  = tri->matIndex;
            e->vertIndex = tri->vertIndex[c];
        }
    }
    ((void(__cdecl*)(void*, RwUInt32, RwUInt32, int(__cdecl*)(const void*, const void*)))0x8247E0)(
        sortElems, geometry->numTriangles * 3, sizeof(SortElemGta),
        (int(__cdecl*)(const void*, const void*))0x75E230);             // cmp s16 matIndex, then s16 vertIndex

    // count maximal runs of equal (matIndex, vertIndex)
    RwInt32 numSegments = 0;
    for (RwInt32 i = 0; i < geometry->numTriangles * 3; i++) {
        if (i == 0
            || sortElems[i].matIndex  != sortElems[i - 1].matIndex      // signed u16 inequality in the binary
            || sortElems[i].vertIndex != sortElems[i - 1].vertIndex) {
            numSegments++;
        }
    }

    RwUInt16*   remapTable = static_cast<RwUInt16*>(mem->rwmalloc(numSegments * sizeof(RwUInt16), 0x1050D));
    RpGeometry* newGeom    = RpGeometryCreate(numSegments, geometry->numTriangles,
                                              ((RwUInt32)(geometry->numTexCoordSets & 0xFF) << 16) | flags);
    newGeom = RpGeometryLock(newGeom, 0xFFF);
    // ABI note: the binary pushes 0xFFF once around the Create/Lock pair; ghidra showed it as a bogus
    // 4th RpGeometryCreate argument and dropped RpGeometryLock's 2nd argument. It is the lockMode.

    if (remapTable == nullptr || newGeom == nullptr) {
        mem->rwfree(sortElems);
        mem->rwfree(newVertPtrs);
        mem->rwfree(oldVertPtrs);
        if (remapTable != nullptr)
            mem->rwfree(remapTable);
        if (newGeom == nullptr)
            return nullptr;
        RpGeometryDestroy(newGeom);
        return nullptr;
    }

    // rebuild triangles: newTri[tri].vertIndex[corner] = segment; matIndex preserved from the old triangle
    RwInt32 segment = -1;
    for (RwInt32 i = 0; i < geometry->numTriangles * 3; i++) {
        if (i == 0
            || sortElems[i].matIndex  != sortElems[i - 1].matIndex
            || sortElems[i].vertIndex != sortElems[i - 1].vertIndex) {
            segment++;
        }
        // (corner << 30) vanishes when multiplied by 8 in 32 bits -> packed * 8 == triangleIndex * 8
        const RwUInt32  triIdx = sortElems[i].packed & 0x3FFFFFFF;
        const RwInt32   corner = (RwInt32)(sortElems[i].packed >> 30);
        RpTriangle*     newTri = &newGeom->triangles[triIdx];
        newTri->vertIndex[corner] = (RwUInt16)segment;
        newTri->matIndex          = geometry->triangles[triIdx].matIndex;
        remapTable[segment]       = sortElems[i].vertIndex;             // decompile reads the element's vert slot
    }
    mem->rwfree(sortElems);

    // the geometry created above already owns 1 morph target; add the missing ones
    if (geometry->numMorphTargets != 1) {
        for (RwInt32 i = 0; i < geometry->numMorphTargets - 1; i++)
            RpGeometryAddMorphTarget(newGeom);
    }
    if (newGeom->numMorphTargets != geometry->numMorphTargets) {
        RpGeometryDestroy(newGeom);
        mem->rwfree(newVertPtrs);
        mem->rwfree(oldVertPtrs);
        mem->rwfree(remapTable);
        return nullptr;
    }

    // harvest morph-target vertex/normal pointers (RpMorphTarget: +0x14 verts, +0x18 normals)
    for (RwInt32 i = 0; i < geometry->numMorphTargets; i++) {
        newVertPtrs[i] = newGeom->morphTarget[i].verts;
        oldVertPtrs[i] = geometry->morphTarget[i].verts;
    }
    if (hasNormals) {
        for (RwInt32 i = 0; i < geometry->numMorphTargets; i++) {
            newNormalPtrs[i] = newGeom->morphTarget[i].normals;
            oldNormalPtrs[i] = geometry->morphTarget[i].normals;
        }
    }

    RwRGBA* oldPreLit = ((flags & rpGEOMETRYPRELIT /*0x8*/) != 0) ? geometry->preLitLum : nullptr;
    RwRGBA* newPreLit = ((flags & rpGEOMETRYPRELIT /*0x8*/) != 0) ? newGeom->preLitLum : nullptr;

    // regather vertex data: new vertex v takes old vertex remapTable[v]
    for (RwInt32 v = 0; v < numSegments; v++) {
        const RwUInt32 oldVert = remapTable[v];
        for (RwInt32 mt = 0; mt < geometry->numMorphTargets; mt++) {
            const RwV3d* src = &oldVertPtrs[mt][oldVert];
            RwV3d*       dst = &newVertPtrs[mt][v];
            *dst = *src;
            if (hasNormals) {
                *reinterpret_cast<RwV3d*>(&newNormalPtrs[mt][v]) =
                    *reinterpret_cast<const RwV3d*>(&oldNormalPtrs[mt][oldVert]);
            }
        }
        if (newPreLit != nullptr)
            newPreLit[v] = oldPreLit[oldVert];
    }
    for (RwInt32 set = 0; set < geometry->numTexCoordSets; set++) {
        const RwTexCoords* src = geometry->texCoords[set];
        RwTexCoords*       dst = newGeom->texCoords[set];
        if (dst != nullptr && src != nullptr) {
            for (RwInt32 v = 0; v < numSegments; v++)
                dst[v] = src[remapTable[v]];
        }
    }

    // recompute bounding spheres of the new morph targets
    for (RwInt32 mt = 0; mt < newGeom->numMorphTargets; mt++) {
        RpMorphTarget* mtPtr = &newGeom->morphTarget[mt];
        RwSphere       sphere;
        RpMorphTargetCalcBoundingSphere(mtPtr, &sphere);
        mtPtr->boundingSphere = sphere;
    }

    mem->rwfree(newVertPtrs);
    mem->rwfree(oldVertPtrs);
    mem->rwfree(newNormalPtrs);
    mem->rwfree(oldNormalPtrs);

    if (_rpMaterialListCopy(&newGeom->matList, &geometry->matList) == nullptr) {
        mem->rwfree(remapTable);
        RpGeometryDestroy(newGeom);
        return nullptr;
    }

    // copy registered geometry-plugin data blocks (entry: +0x00 offset, +0x04 size, +0x30 next)
    for (RwUInt32* entry = ((RwUInt32*(__cdecl*)(void))0x74BFD0)();
         entry != nullptr;
         entry = reinterpret_cast<RwUInt32*>(entry[0xC])) {
        const RwUInt32 size = entry[1];
        if ((RwInt32)size > 0) {
            RwUInt8* src = reinterpret_cast<RwUInt8*>(const_cast<RpGeometry*>(geometry)) + entry[0];
            RwUInt8* dst = reinterpret_cast<RwUInt8*>(newGeom) + entry[0];
            for (RwUInt32 i = 0; i < size; i++)
                dst[i] = src[i];
        }
    }

    if (callback != nullptr)
        callback(geometry, newGeom, remapTable, (RwUInt16)numSegments);

    mem->rwfree(remapTable);

    if (RpGeometryUnlock(newGeom) != nullptr)   // unlock rebuilds the mesh header from the new triangles
        return newGeom;

    RpGeometryDestroy(newGeom);
    return nullptr;
}


void RpTieSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc)
{
    StaticRef<RwInt32>(0x8D62E8) = blockSize;
    StaticRef<RwInt32>(0x8D62EC) = numBlocksToPrealloc;
}


void RpLightTieSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc)
{
    StaticRef<RwInt32>(0x8D62F0) = blockSize;
    StaticRef<RwInt32>(0x8D62F4) = numBlocksToPrealloc;
}


RpWorld* RpWorldRemoveCamera(RpWorld* world, RwCamera* camera)
{
    RwUInt8* chunk = (RwUInt8*)camera + gRpWorldCameraChunkOff;

    if (*(RpWorld**)(chunk + 0x0C) != NULL) {
        *(RpWorld**)(chunk + 0x0C) = NULL;
        *(RwInt32*)(chunk + 0x08) = 0;   // numFrustumSectors reset
        return world;
    }
    return NULL;
}


RpWorld* RpWorldAddCamera(RpWorld* world, RwCamera* camera)
{
    RwUInt8* chunk = (RwUInt8*)camera + gRpWorldCameraChunkOff;

    if (camera->object.object.parent != NULL) {
        RwFrameUpdateObjects((RwFrame*)camera->object.object.parent);
    }
    *(RpWorld**)(chunk + 0x0C) = world;
    return world;   // header contract; binary body is void-returning (ABI note in STATUS.md)
}


RpWorld* RwCameraGetWorld(const RwCamera* camera)
{
    return *(RpWorld**)((RwUInt8*)camera + gRpWorldCameraChunkOff + 0x0C);
}


RpWorld* RpWorldRemoveAtomic(RpWorld* world, RpAtomic* atomic)
{
    if (atomic->repEntry != NULL) {
        RwResourcesFreeResEntry(atomic->repEntry);
    }

    RwLLLink* head = &atomic->llWorldSectorsInAtomic.link;
    RwLLLink* lnk  = head->next;
    while (lnk != head) {
        RpTie* tie = rwLLLinkGetData(lnk, RpTie, lWorldSectorInAtomic);
        RwLLLink* next = lnk->next;

        if (tie->apAtom != NULL && tie->worldSector != NULL) {
            // unlink from atomic's llWorldSectorsInAtomic
            lnk->prev->next = lnk->next;
            lnk->next->prev = lnk->prev;
            // unlink from sector's collAtomicsInWorldSector
            tie->lAtomicInWorldSector.prev->next = tie->lAtomicInWorldSector.next;
            tie->lAtomicInWorldSector.next->prev = tie->lAtomicInWorldSector.prev;
            // free the RpTie through the engine freelist-free (+0x148), cookie = *(RwEngineInstance + worldGlobalsOffset)
            (*(void(__cdecl**)(void*, void*))((RwUInt8*)RwEngineInstance + 0x148))
                (*(void**)((RwUInt8*)RwEngineInstance + gRpWorldGlobalsOffset), tie);
        }
        lnk = next;
    }

    *(RpWorld**)((RwUInt8*)atomic + gRpWorldAtomicChunkOff) = NULL;
    return world;
}


RpWorld* RpWorldAddAtomic(RpWorld* world, RpAtomic* atomic)
{
    RwUInt8* chunk = (RwUInt8*)atomic + gRpWorldAtomicChunkOff;

    if (atomic->object.object.parent != NULL) {
        RwFrameUpdateObjects((RwFrame*)atomic->object.object.parent);
    }
    *(RpWorld**)chunk = world;
    return world;   // header contract; binary body is void-returning (ABI note in STATUS.md)
}


RpWorld* RpAtomicGetWorld(const RpAtomic* atomic)
{
    return *(RpWorld**)((RwUInt8*)atomic + gRpWorldAtomicChunkOff);
}


RpWorld* RpWorldAddClump(RpWorld* world, RpClump* clump)
{
    void*     clumpFrame = clump->object.parent;
    RwLLLink* head       = &world->clumpList.link;
    RwLLLink* link       = &clump->inWorldLink;
    RwLLLink* first      = head->next;

    // insert at head of world->clumpList
    link->prev = head;
    link->next = first;
    first->prev = link;
    head->next  = link;
    world->numClumpsInWorld++;

    *(RpWorld**)((RwUInt8*)clump + gRpWorldClumpChunkOff) = world;

    // add the clump's atomics/lights/cameras to the world (static helpers outside the floor -> raw casts)
    RpClumpForAllAtomics(clump, (RpAtomicCallBack)0x751390, world);
    RpClumpForAllLights(clump, (RpLightCallBack)0x7513D0, world);
    RpClumpForAllCameras(clump, (RwCameraCallBack)0x751420, world);

    if (clumpFrame != NULL) {
        RwMatrixOptimize(&((RwFrame*)clumpFrame)->modelling, (const RwMatrixTolerance*)NULL);
        RwFrameUpdateObjects((RwFrame*)clumpFrame);
    }

    // snapshot the global frustum-stamp counter into clumpChunk+4
    *(RwInt32*)((RwUInt8*)clump + gRpWorldClumpChunkOff + 4) =
        *(RwInt32*)((RwUInt8*)RwEngineInstance + gRpWorldGlobalsOffset + 8);
    return world;
}


RpWorld* RpWorldRemoveClump(RpWorld* world, RpClump* clump)
{
    RpWorld*  clumpWorld = *(RpWorld**)((RwUInt8*)clump + gRpWorldClumpChunkOff);
    RwLLLink* link       = &clump->inWorldLink;

    if (clumpWorld != NULL) {
        clumpWorld->numClumpsInWorld--;
        if (link == clumpWorld->currentClumpLink) {
            clumpWorld->currentClumpLink = link->next;
        }
        link->prev->next = link->next;
        link->next->prev = link->prev;

        // remove the clump's atomics/lights/cameras from the world (static helpers outside the floor -> raw casts)
        RpClumpForAllAtomics(clump, (RpAtomicCallBack)0x751500, clumpWorld);
        RpClumpForAllLights(clump, (RpLightCallBack)0x7515A0, clumpWorld);
        RpClumpForAllCameras(clump, (RwCameraCallBack)0x751630, clumpWorld);

        *(RpWorld**)((RwUInt8*)clump + gRpWorldClumpChunkOff) = NULL;
        return world;
    }

    // not attached to any world: RwError{pluginID=2, errorCode=4} through the 0x8088D0 passthrough thunk
    RwError rwError;
    rwError.pluginID  = 2;
    rwError.errorCode = ((RwInt32(__cdecl *)(RwInt32))0x8088D0)(4);
    RwErrorSet(&rwError);
    return NULL;
}


RpWorld* RpClumpGetWorld(const RpClump* clump)
{
    return *(RpWorld**)((RwUInt8*)clump + gRpWorldClumpChunkOff);
}


RpWorld* RpWorldAddLight(RpWorld* world, RpLight* light)
{
    *(RpWorld**)((RwUInt8*)light + gRpWorldLightChunkOff) = world;

    RwLinkList* list;
    if (light->object.object.subType < rpLIGHTPOSITIONINGSTART /* 0x80: no tie mechanism (ambient/directional) */) {
        list = &world->directionalLightList;
    } else {
        if (light->object.object.parent != NULL) {
            RwFrameUpdateObjects((RwFrame*)light->object.object.parent);
        }
        list = &world->lightList;
    }

    RwLLLink* head  = &list->link;
    RwLLLink* link  = &light->inWorld;
    RwLLLink* first = head->next;

    // insert at head
    link->prev = head;
    link->next = first;
    first->prev = link;
    head->next  = link;
    return world;
}


RpWorld* RpWorldRemoveLight(RpWorld* world, RpLight* light)
{
    *(RpWorld**)((RwUInt8*)light + gRpWorldLightChunkOff) = NULL;

    // destroy all RpLightTies of this light (list of sectors holding the light)
    RwLLLink* head = &light->WorldSectorsInLight.link;
    RwLLLink* lnk  = head->next;
    while (lnk != head) {
        RpLightTie* tie = rwLLLinkGetData(lnk, RpLightTie, WorldSectorInLight);
        RwLLLink* next = lnk->next;

        // unlink from light's WorldSectorsInLight
        lnk->prev->next = lnk->next;
        lnk->next->prev = lnk->prev;
        // unlink from sector's lightsInWorldSector
        tie->lightInWorldSector.prev->next = tie->lightInWorldSector.next;
        tie->lightInWorldSector.next->prev = tie->lightInWorldSector.prev;
        // free the RpLightTie through the engine freelist-free (+0x148), cookie = *(RwEngineInstance + worldGlobalsOffset + 4)
        (*(void(__cdecl**)(void*, void*))((RwUInt8*)RwEngineInstance + 0x148))
            (*(void**)((RwUInt8*)RwEngineInstance + gRpWorldGlobalsOffset + 4), tie);

        lnk = next;
    }

    // unlink light from its world list (lightList or directionalLightList)
    light->inWorld.prev->next = light->inWorld.next;
    light->inWorld.next->prev = light->inWorld.prev;
    return world;
}


RpWorld* RpLightGetWorld(const RpLight* light)
{
    return *(RpWorld**)((RwUInt8*)light + gRpWorldLightChunkOff);
}


struct RpWorldCameraChunkGta
{
    RpWorldSector** frustumSectors;    /* +0x00 */
    RwUInt32        pad04;             /* +0x04 (not accessed here) */
    RwInt32         numFrustumSectors; /* +0x08 */
    RpWorld*        world;             /* +0x0C */
};

RwCamera* RwCameraForAllClumpsInFrustum(RwCamera* camera, void* data)
{
    // bump the global frustum stamp (world global-data block +8)
    RwInt32* stampSlot = (RwInt32*)((RwUInt8*)RwEngineInstance + gRpWorldGlobalsOffset + 8);
    ++(*stampSlot);
    const RwInt32 stamp = *stampSlot;

    RpWorldCameraChunkGta* chunk = (RpWorldCameraChunkGta*)((RwUInt8*)camera + gRpWorldCameraChunkOff);
    RpWorldSector**        it    = chunk->frustumSectors;
    RwInt32                count = chunk->numFrustumSectors;

    while (count != 0) {
        RpWorldSector* sector = *it;

        RwLLLink tmpLink;
        tmpLink.next = NULL;
        tmpLink.prev = NULL;

        RwLLLink* head = &sector->collAtomicsInWorldSector.link;
        RwLLLink* lnk  = head->next;
        while (lnk != head) {
            RpTie* tie = rwLLLinkGetData(lnk, RpTie, lAtomicInWorldSector);
            RwLLLink* next  = lnk->next;
            next->prev      = &tmpLink;
            lnk->next       = &tmpLink;
            RwLLLink* saved = lnk;

            RpAtomic* atomic = tie->apAtom;
            RpClump*  clump  = atomic->clump;
            lnk = next;

            if (clump != NULL && *(RwInt32*)((RwUInt8*)clump + gRpWorldClumpChunkOff + 4) != stamp) {
                if (RwCameraFrustumTestSphere(camera, RpAtomicGetWorldBoundingSphere(atomic)) != 0) {
                    *(RwInt32*)((RwUInt8*)clump + gRpWorldClumpChunkOff + 4) = stamp;
                    if (clump->callback(clump, data) == NULL) {
                        saved->next = lnk;
                        lnk->prev   = saved;
                        return camera;
                    }
                }
            }
            saved->next = lnk;
            lnk->prev   = saved;
            lnk = next;
        }
        count--;
        it++;
    }
    return camera;
}


struct RpWorldCameraChunkGta
{
    RpWorldSector** frustumSectors;    /* +0x00 */
    RwUInt32        pad04;             /* +0x04 (not accessed here) */
    RwInt32         numFrustumSectors; /* +0x08 */
    RpWorld*        world;             /* +0x0C */
};

RwCamera* RwCameraForAllAtomicsInFrustum(RwCamera* camera, RpAtomicCallBack callback, void* data)
{
    // bump the global frustum stamp (world global-data block +8)
    RwInt32* stampSlot = (RwInt32*)((RwUInt8*)RwEngineInstance + gRpWorldGlobalsOffset + 8);
    ++(*stampSlot);
    const RwUInt32 stamp = (RwUInt32)*stampSlot;

    RpWorldCameraChunkGta* chunk = (RpWorldCameraChunkGta*)((RwUInt8*)camera + gRpWorldCameraChunkOff);
    RpWorldSector**        it    = chunk->frustumSectors;
    RwInt32                count = chunk->numFrustumSectors;

    while (count != 0) {
        RpWorldSector* sector = *it;

        RwLLLink tmpLink;
        tmpLink.next = NULL;
        tmpLink.prev = NULL;

        RwLLLink* head = &sector->collAtomicsInWorldSector.link;
        RwLLLink* lnk  = head->next;
        while (lnk != head) {
            RpTie* tie = rwLLLinkGetData(lnk, RpTie, lAtomicInWorldSector);
            RwLLLink* next  = lnk->next;
            next->prev      = &tmpLink;
            lnk->next       = &tmpLink;
            RwLLLink* saved = lnk;

            RpAtomic* atomic = tie->apAtom;
            lnk = next;

            if (atomic != NULL && atomic->renderFrame != (RwUInt16)stamp) {
                if (RwCameraFrustumTestSphere(camera, RpAtomicGetWorldBoundingSphere(atomic)) != 0) {
                    atomic->renderFrame = (RwUInt16)stamp;
                    if (callback(atomic, data) == NULL) {
                        saved->next = lnk;
                        lnk->prev   = saved;
                        return camera;
                    }
                }
            }
            saved->next = lnk;
            lnk->prev   = saved;
            lnk = next;
        }
        count--;
        it++;
    }
    return camera;
}


struct RpWorldCameraChunkGta
{
    RpWorldSector** frustumSectors;    /* +0x00 */
    RwUInt32        pad04;             /* +0x04 (not accessed here) */
    RwInt32         numFrustumSectors; /* +0x08 */
    RpWorld*        world;             /* +0x0C */
};

RwCamera* RwCameraForAllSectorsInFrustum(RwCamera* camera, RpWorldSectorCallBack callBack, void* data)
{
    RpWorldCameraChunkGta* chunk = (RpWorldCameraChunkGta*)((RwUInt8*)camera + gRpWorldCameraChunkOff);

    RpWorldSector** it    = chunk->frustumSectors;
    RwInt32         count = chunk->numFrustumSectors;
    while (count != 0) {
        if (callBack(*it, data) == NULL) {
            break;
        }
        it++;
        count--;
    }
    return camera;
}


RpLight* RpLightForAllWorldSectors(RpLight* light, RpWorldSectorCallBack callback, void* data)
{
    if (light->object.object.subType < rpLIGHTPOSITIONINGSTART /* 0x80: ambient/directional */) {
        RpWorld* world = *(RpWorld**)((RwUInt8*)light + gRpWorldLightChunkOff);
        if (world != NULL && RpWorldForAllWorldSectors(world, callback, data) == NULL) {
            return NULL;
        }
    } else {
        // positioned light: walk its RpLightTie list (linked via WorldSectorInLight)
        RwLLLink* head = &light->WorldSectorsInLight.link;
        RwLLLink* lnk  = head->next;
        while (lnk != head) {
            RpLightTie* tie = rwLLLinkGetData(lnk, RpLightTie, WorldSectorInLight);
            lnk = lnk->next;
            if (callback(tie->sect, data) == NULL) {
                return light;
            }
        }
    }
    return light;
}


RpAtomic* RpAtomicForAllWorldSectors(RpAtomic* atomic, RpWorldSectorCallBack callback, void* data)
{
    RwLLLink* head = &atomic->llWorldSectorsInAtomic.link;
    RwLLLink* lnk  = head->next;
    while (lnk != head) {
        RpTie* tie = rwLLLinkGetData(lnk, RpTie, lWorldSectorInAtomic);
        lnk = lnk->next;
        if (callback(tie->worldSector, data) == NULL) {
            break;
        }
    }
    return atomic;
}


RpWorldSector* RpWorldSectorForAllAtomics(RpWorldSector* sector, RpAtomicCallBack callback, void* data)
{
    // Local temp link: the current entry is temporarily detached onto it while the
    // callback runs, so a callback that destroys the entry cannot corrupt the list
    // (decompile: uStack_10/uStack_4 = {0,0}).
    RwLLLink tmpLink;
    tmpLink.next = NULL;
    tmpLink.prev = NULL;

    RwLLLink* head = &sector->collAtomicsInWorldSector.link;
    RwLLLink* lnk  = head->next;
    if (lnk != head) {
        for (;;) {
            RwLLLink* next  = lnk->next;
            next->prev      = &tmpLink;
            lnk->next       = &tmpLink;
            RwLLLink* saved = lnk;

            RpAtomic* atomic = *(RpAtomic**)((RwUInt8*)lnk + 8);   // RpTie.apAtom (link = lAtomicInWorldSector)
            lnk = next;
            if (atomic != NULL && callback(atomic, data) == NULL) {
                saved->next = lnk;
                lnk->prev   = saved;
                return sector;
            }
            saved->next = lnk;
            lnk->prev   = saved;
            if (lnk == head) {
                return sector;
            }
        }
    }
    return sector;
}


RpWorldSector* RpWorldSectorForAllCollisionAtomics(RpWorldSector* sector, RpAtomicCallBack callback, void* data)
{
    RwLLLink tmpLink;
    tmpLink.next = NULL;
    tmpLink.prev = NULL;

    RwLLLink* head = &sector->collAtomicsInWorldSector.link;
    RwLLLink* lnk  = head->next;
    if (lnk != head) {
        for (;;) {
            RwLLLink* next  = lnk->next;
            next->prev      = &tmpLink;
            lnk->next       = &tmpLink;
            RwLLLink* saved = lnk;

            RpAtomic* atomic = *(RpAtomic**)((RwUInt8*)lnk + 8);   // RpTie.apAtom (link = lAtomicInWorldSector)
            lnk = next;
            if (atomic != NULL && callback(atomic, data) == NULL) {
                saved->next = lnk;
                lnk->prev   = saved;
                return sector;
            }
            saved->next = lnk;
            lnk->prev   = saved;
            if (lnk == head) {
                return sector;
            }
        }
    }
    return sector;
}


RpWorldSector* RpWorldSectorForAllLights(RpWorldSector* sector, RpLightCallBack callback, void* data)
{
    RwLLLink tmpLink;
    tmpLink.next = NULL;
    tmpLink.prev = NULL;

    // Phase 1: lights tied into this sector (RpLightTie linked via lightInWorldSector).
    RwLLLink* head = &sector->lightsInWorldSector.link;
    RwLLLink* lnk  = head->next;
    while (lnk != head) {
        RwLLLink* next  = lnk->next;
        next->prev      = &tmpLink;
        lnk->next       = &tmpLink;
        RwLLLink* saved = lnk;

        RpLight* light = *(RpLight**)((RwUInt8*)lnk + 8);   // RpLightTie.light
        lnk = next;
        if (light != NULL && callback(light, data) == NULL) {
            saved->next = lnk;
            lnk->prev   = saved;
            return sector;
        }
        saved->next = lnk;
        lnk->prev   = saved;
        lnk = next;
    }

    // Phase 2: world-global (non-positioned) lights, linked via RpLight.inWorld.
    RpWorld* world = RpWorldSectorGetWorld(sector);
    if (world != NULL) {
        RwLLLink* whead = &world->directionalLightList.link;
        RwLLLink* wlnk  = whead->next;
        if (wlnk != whead) {
            for (;;) {
                RwLLLink* next  = wlnk->next;
                next->prev      = &tmpLink;
                wlnk->next      = &tmpLink;
                RwLLLink* saved = wlnk;

                RpLight* light = rwLLLinkGetData(wlnk, RpLight, inWorld);
                wlnk = next;
                if (light != NULL && callback(light, data) == NULL) {
                    saved->next = wlnk;
                    wlnk->prev  = saved;
                    return sector;
                }
                saved->next = wlnk;
                wlnk->prev  = saved;
                if (wlnk == whead) {
                    return sector;
                }
            }
        }
    }
    return sector;
}


RwUInt32 RpWorldStreamGetSize(const RpWorld* world) {
    const RpWorldGta*       w    = reinterpret_cast<const RpWorldGta*>(world);
    const RpSectorBSP*      root = reinterpret_cast<const RpSectorBSP*>(w->rootSector);
    const RwUInt32          flags = w->flags;
    RwUInt32 result;

    if (root->type < 0) {
        // Корень — лист RpWorldSector: matListSize + 0x7C + pluginSize + sectorBody
        RwUInt32 sectorBody = 0x2C;
        if ((flags & rpWORLDNATIVE /*0x1000000*/) == 0) {
            const RwUInt32 numVerts = reinterpret_cast<const RpWorldSectorGta*>(root)->numVertices;
            sectorBody = numVerts * 0xC + 0x2C;
            if ((flags & rpWORLDNORMALS /*0x10*/) != 0) {
                sectorBody += numVerts * 4;
            }
            if ((flags & rpWORLDPRELIT /*0x08*/) != 0) {
                sectorBody += numVerts * 4;
            }
            sectorBody += (w->numTexCoordSets * numVerts
                           + reinterpret_cast<const RpWorldSectorGta*>(root)->numTriangles) * 8;
        }
        result = static_cast<RwUInt32>(_rpMaterialListStreamGetSize(reinterpret_cast<const RpMaterialList*>(&w->matList)))
               + 0x7C
               + static_cast<RwUInt32>(_rwPluginRegistryGetSize(&worldSectorTKList, root))
               + sectorBody;
    } else {
        // Корень — плоскостной BSP-узел: считаем левое и правое поддеревья (один уровень развёрнут)
        const RpSectorBSP* left = root->leftSubTree;
        RwUInt32 acc;
        if (left->type < 0) {
            RwUInt32 sectorBody = 0x2C;
            if ((flags & rpWORLDNATIVE /*0x1000000*/) == 0) {
                const RwUInt32 numVerts = reinterpret_cast<const RpWorldSectorGta*>(left)->numVertices;
                sectorBody = numVerts * 0xC + 0x2C;
                if ((flags & rpWORLDNORMALS /*0x10*/) != 0) {
                    sectorBody += numVerts * 4;
                }
                if ((flags & rpWORLDPRELIT /*0x08*/) != 0) {
                    sectorBody += numVerts * 4;
                }
                sectorBody += (w->numTexCoordSets * numVerts
                               + reinterpret_cast<const RpWorldSectorGta*>(left)->numTriangles) * 8;
            }
            acc = static_cast<RwUInt32>(_rwPluginRegistryGetSize(&worldSectorTKList, left))
                + 0x48 + sectorBody;
        } else {
            acc = ((RwUInt32(__cdecl*)(const RpSector*, const RpWorld*, RwUInt32))0x762040)(
                      reinterpret_cast<const RpSector*>(left), world, flags) + 0x30;
        }
        const RpSectorBSP* right = root->rightSubTree;
        if (right->type < 0) {
            RwUInt32 sectorBody = 0x2C;
            if ((flags & rpWORLDNATIVE /*0x1000000*/) == 0) {
                const RwUInt32 numVerts = reinterpret_cast<const RpWorldSectorGta*>(right)->numVertices;
                sectorBody = numVerts * 0xC + 0x2C;
                if ((flags & rpWORLDNORMALS /*0x10*/) != 0) {
                    sectorBody += numVerts * 4;
                }
                if ((flags & rpWORLDPRELIT /*0x08*/) != 0) {
                    sectorBody += numVerts * 4;
                }
                sectorBody += (w->numTexCoordSets * numVerts
                               + reinterpret_cast<const RpWorldSectorGta*>(right)->numTriangles) * 8;
            }
            acc += 0x24
                 + static_cast<RwUInt32>(_rwPluginRegistryGetSize(&worldSectorTKList, right))
                 + sectorBody;
        } else {
            acc += 0xC
                 + ((RwUInt32(__cdecl*)(const RpSector*, const RpWorld*, RwUInt32))0x762040)(
                       reinterpret_cast<const RpSector*>(right), world, flags);
        }
        result = static_cast<RwUInt32>(_rpMaterialListStreamGetSize(reinterpret_cast<const RpMaterialList*>(&w->matList))) + 100 + acc;
    }
    return static_cast<RwUInt32>(_rwPluginRegistryGetSize(&worldTKList, world)) + 0xC + result;
}


static RwUInt32& g_rpWorldStreamRightsId   = StaticRef<RwUInt32>(0xC94C18);
static RwUInt32& g_rpWorldStreamRightsData = StaticRef<RwUInt32>(0xC94C14);

RpWorld* RpWorldStreamRead(RwStream* stream) {
    RwUInt32 structLength;
    RwUInt32 version;
    if (!RwStreamFindChunk(stream, rwID_STRUCT /*0x01*/, &structLength, &version)) {
        return nullptr;
    }
    if (version < 0x34000 || 0x36003 < version) {
        RwError rwError;
        rwError.pluginID  = 2;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000004);   // _rwerror
        RwErrorSet(&rwError);
        return nullptr;
    }

    struct WorldChunkGta {                 // 0x40 байт (текущая версия)
        RwUInt32 rootIsWorldSector;        // 0x00 (1: корень — лист RpWorldSector)
        RwV3d    origin;                   // 0x04 (на диске инвертирован)
        RwUInt32 numTriangles;             // 0x10
        RwUInt32 numVertices;              // 0x14
        RwUInt32 numPlaneSectors;          // 0x18
        RwUInt32 numWorldSectors;          // 0x1C
        RwUInt32 collSector;               // 0x20 (>=1 -> отказ: коллизионные сектора не читаются)
        RwUInt32 format;                   // 0x24
        RwBBox   boundingBox;              // 0x28
    } chunk;
    memset(&chunk, 0, 0x40);               // декомпил: обнуление 16 dword
    if (version < 0x34001) {
        struct WorldChunk34000Gta {        // 0x34 байта (старая версия)
            RwUInt32 rootIsWorldSector;    // 0x00
            RwV3d    origin;               // 0x04
            RwSurfaceProperties surfaceProps; // 0x10..0x18 (читаются, НЕ копируются; ср. rpWorldChunkInfo34000)
            RwUInt32 numTriangles;         // 0x1C
            RwUInt32 numVertices;          // 0x20
            RwUInt32 numPlaneSectors;      // 0x24
            RwUInt32 numWorldSectors;      // 0x28
            RwUInt32 collSector;           // 0x2C
            RwUInt32 format;               // 0x30
        } oldChunk;
        memset(&oldChunk, 0, 0x34);        // декомпил: обнуление 13 dword
        if (RwStreamRead(stream, &oldChunk, structLength) != structLength) {
            return nullptr;                // декомпил: голый return без RwErrorSet
        }
        // декомпил: поразрядное перекладывание (порядок операндов сохранён)
        chunk.rootIsWorldSector = oldChunk.rootIsWorldSector;
        chunk.origin.x          = oldChunk.origin.x;
        chunk.origin.y          = oldChunk.origin.y;
        chunk.origin.z          = oldChunk.origin.z;
        chunk.numTriangles      = oldChunk.numTriangles;
        chunk.numVertices       = oldChunk.numVertices;
        chunk.numPlaneSectors   = oldChunk.numPlaneSectors;
        chunk.numWorldSectors   = oldChunk.numWorldSectors;
        chunk.collSector        = oldChunk.collSector;
        chunk.format            = oldChunk.format;
    } else {
        if (RwStreamRead(stream, &chunk, structLength) != structLength) {
            return nullptr;                // декомпил: голый return без RwErrorSet
        }
    }

    if (chunk.collSector < 1) {
        // полный размер блока: мир (+плагины) + листы + узлы BSP + данные вершин
        RwUInt32 totalSize = static_cast<RwUInt32>(worldTKList.sizeOfStruct)
                           + static_cast<RwUInt32>(worldSectorTKList.sizeOfStruct) * chunk.numWorldSectors
                           + chunk.numPlaneSectors * 0x18;
        RwUInt32 numTexCoords;
        if ((chunk.format & 0xFF0000) == 0) {              // старый формат: флаги в младшем байте
            if ((chunk.format & rpWORLDTEXTURED2 /*0x80*/) != 0) {
                numTexCoords = 2;
            } else {
                numTexCoords = (chunk.format >> 2) & 1;    // бит rpWORLDTEXTURED (0x04)
            }
        } else {
            numTexCoords = (chunk.format >> 16) & 0xFF;    // rpWORLDTEXCOORDSETS
        }
        if ((chunk.format & rpWORLDNATIVE /*0x1000000*/) == 0) {
            totalSize += chunk.numVertices * 0xC;          // вершины
            if ((chunk.format & rpWORLDNORMALS /*0x10*/) != 0) {
                totalSize += chunk.numVertices * 4;        // нормали
            }
            if ((chunk.format & rpWORLDPRELIT /*0x08*/) != 0) {
                totalSize += chunk.numVertices * 4;        // prelit
            }
            if (numTexCoords != 0) {
                totalSize += numTexCoords * chunk.numVertices * 8;
            }
            totalSize += chunk.numTriangles * 8;
        }

        RpWorldGta* world = reinterpret_cast<RpWorldGta*>(
            RwGlobalsMem()->rwmalloc(totalSize, 0x3000B));
        if (world == nullptr) {
            RwError rwError;
            rwError.pluginID  = 2;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000013);   // _rwerror
            RwErrorSet(&rwError);
            return nullptr;
        }
        memset(world, 0, totalSize);                       // декомпил: dword-цикл + хвост байтами

        // RW-объект: type=rpWORLD(7), privateFlags=rpWORLDSINGLEMALLOC(1), parent=NULL
        world->object.object.type         = rpWORLD /*7*/;
        world->object.object.subType      = 0;
        world->object.object.flags        = 0;
        world->object.object.privateFlags = rpWORLDSINGLEMALLOC /*0x01*/;
        world->object.object.parent       = nullptr;
        _rpWorldRegisterWorld(reinterpret_cast<RpWorld*>(world), totalSize);           // 0x74F0C0: узел в глобальный список миров
        world->renderOrder      = rpWORLDRENDERBACK2FRONT /*2*/;
        world->flags            = chunk.format;
        world->numTexCoordSets  = static_cast<RwInt32>(numTexCoords);
        // первый сектор пакуется сразу за миром (с его плагинными данными)
        RwUInt8* cursor = reinterpret_cast<RwUInt8*>(world) + worldTKList.sizeOfStruct;
        world->worldOrigin.x = chunk.origin.x * -1.0f;     // _DAT_00858c1c = -1.0f
        world->worldOrigin.y = chunk.origin.y * -1.0f;
        world->worldOrigin.z = chunk.origin.z * -1.0f;
        world->clumpList.link.next = &world->clumpList.link;               // сентинелы списков
        world->clumpList.link.prev = &world->clumpList.link;
        world->lightList.link.next = &world->lightList.link;
        world->lightList.link.prev = &world->lightList.link;
        world->directionalLightList.link.next = &world->directionalLightList.link;
        world->directionalLightList.link.prev = &world->directionalLightList.link;

        // сектора: корень-лист -> чанк rwID_ATOMICSECT(9), иначе дерево rwID_PLANESECT(0x0A)
        RwUInt32 sectorVersion;
        if (chunk.rootIsWorldSector == 0) {
            if (!RwStreamFindChunk(stream, rwID_PLANESECT /*0x0A*/, nullptr, &sectorVersion)) {
                RpWorldDestroy(reinterpret_cast<RpWorld*>(world));
                return nullptr;
            }
            if (sectorVersion < 0x34000 || 0x36003 < sectorVersion) {
                RwError rwError;
                rwError.pluginID  = 2;
                rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000004);
                RwErrorSet(&rwError);
                RpWorldDestroy(reinterpret_cast<RpWorld*>(world));
                return nullptr;
            }
            world->rootSector = ((RpSector*(__cdecl*)(RwStream*, RwUInt8**, RpWorldGta*, RwUInt32))0x763310)(
                stream, &cursor, world, chunk.format);
            if (world->rootSector == nullptr) {
                RpWorldDestroy(reinterpret_cast<RpWorld*>(world));
                return nullptr;
            }
        } else {
            if (!RwStreamFindChunk(stream, rwID_ATOMICSECT /*0x09*/, nullptr, &sectorVersion)) {
                RpWorldDestroy(reinterpret_cast<RpWorld*>(world));
                return nullptr;
            }
            if (sectorVersion < 0x34000 || 0x36003 < sectorVersion) {
                RwError rwError;
                rwError.pluginID  = 2;
                rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000004);
                RwErrorSet(&rwError);
                RpWorldDestroy(reinterpret_cast<RpWorld*>(world));
                return nullptr;
            }
            world->rootSector = ((RpSector*(__cdecl*)(RwStream*, RwUInt8**, RpWorldGta*, RwUInt32))0x762F30)(
                stream, &cursor, world, chunk.format);
            if (world->rootSector == nullptr) {
                RpWorldDestroy(reinterpret_cast<RpWorld*>(world));
                return nullptr;
            }
        }

        world->numClumpsInWorld = 0;
        world->currentClumpLink = &world->clumpList.link;

        if (version < 0x34003) {
            _rpWorldFindBBox(reinterpret_cast<RpWorld*>(world), &world->boundingBox);          // bbox по секторам
        } else {
            world->boundingBox = chunk.boundingBox;                // копия 6 dword из чанка
        }
        _rpWorldSetupSectorBoundingBoxes(reinterpret_cast<RpWorld*>(world));
        RpWorldSetSectorRenderCallBack(reinterpret_cast<RpWorld*>(world), nullptr);

        // материалы из списка держат текстуры: про-рефреш (RpMaterialSetTexture двигает refCount)
        for (RwInt32 i = 0; i < world->matList.numMaterials; i++) {
            RpMaterial* mat = world->matList.materials[i];
            if (mat->texture != nullptr) {
                RpMaterialSetTexture(mat, mat->texture);
            }
        }

        _rwPluginRegistryInitObject(&worldTKList, world);
        g_rpWorldStreamRightsId   = 0;
        g_rpWorldStreamRightsData = 0;
        if (_rwPluginRegistryReadDataChunks(&worldTKList, stream, world) == nullptr) {
            _rpWorldUnregisterWorld(reinterpret_cast<RpWorld*>(world));
            RwGlobalsMem()->rwfree(world);
            return nullptr;
        }
        if (g_rpWorldStreamRightsId != 0) {
            _rwPluginRegistryInvokeRights(&worldTKList, g_rpWorldStreamRightsId,
                                          world, g_rpWorldStreamRightsData);
        }
        if (RpWorldUnlock(reinterpret_cast<RpWorld*>(world)) == nullptr) {
            RpWorldDestroy(reinterpret_cast<RpWorld*>(world));
            return nullptr;
        }
        return reinterpret_cast<RpWorld*>(world);
    }

    // collSector >= 1: коллизионные сектора старых миров не поддерживаются
    RwError rwError;
    rwError.pluginID  = 2;
    rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000004);   // _rwerror
    RwErrorSet(&rwError);
    return nullptr;
}


const RpWorld* RpWorldStreamWrite(const RpWorld* world, RwStream* stream) {
    const RpWorldGta*       w     = reinterpret_cast<const RpWorldGta*>(world);
    const RpSectorBSP*      root  = reinterpret_cast<const RpSectorBSP*>(w->rootSector);
    const RwUInt32          flags = w->flags;
    RwUInt32 total;

    if (root->type < 0) {
        // matListSize + 0x7C + pluginSize + sectorBody
        RwUInt32 sectorBody = 0x2C;
        if ((flags & rpWORLDNATIVE /*0x1000000*/) == 0) {
            const RwUInt32 numVerts = reinterpret_cast<const RpWorldSectorGta*>(root)->numVertices;
            sectorBody = numVerts * 0xC + 0x2C;
            if ((flags & rpWORLDNORMALS /*0x10*/) != 0) {
                sectorBody += numVerts * 4;
            }
            if ((flags & rpWORLDPRELIT /*0x08*/) != 0) {
                sectorBody += numVerts * 4;
            }
            sectorBody += (w->numTexCoordSets * numVerts
                           + reinterpret_cast<const RpWorldSectorGta*>(root)->numTriangles) * 8;
        }
        total = static_cast<RwUInt32>(_rpMaterialListStreamGetSize(reinterpret_cast<const RpMaterialList*>(&w->matList)))
              + 0x7C
              + static_cast<RwUInt32>(_rwPluginRegistryGetSize(&worldSectorTKList, root))
              + sectorBody;
    } else {
        const RpSectorBSP* left = root->leftSubTree;
        RwUInt32 acc;
        if (left->type < 0) {
            // тело листа через внутренний хелпер 0x762440 (вне этажа)
            const RwUInt32 bodySize =
                ((RwUInt32(__cdecl*)(const RpSector*, const RpWorld*, RwUInt32))0x762440)(
                    reinterpret_cast<const RpSector*>(left), world, flags);
            acc = static_cast<RwUInt32>(_rwPluginRegistryGetSize(&worldSectorTKList, left))
                + bodySize + 0x48;
        } else {
            acc = ((RwUInt32(__cdecl*)(const RpSector*, const RpWorld*, RwUInt32))0x762040)(
                      reinterpret_cast<const RpSector*>(left), world, flags) + 0x30;
        }
        const RpSectorBSP* right = root->rightSubTree;
        if (right->type < 0) {
            const RwUInt32 bodySize =
                ((RwUInt32(__cdecl*)(const RpSector*, const RpWorld*, RwUInt32))0x762440)(
                    reinterpret_cast<const RpSector*>(right), world, flags);
            acc += static_cast<RwUInt32>(_rwPluginRegistryGetSize(&worldSectorTKList, right))
                 + bodySize + 0x24;
        } else {
            acc += 0xC
                 + ((RwUInt32(__cdecl*)(const RpSector*, const RpWorld*, RwUInt32))0x762040)(
                       reinterpret_cast<const RpSector*>(right), world, flags);
        }
        total = static_cast<RwUInt32>(_rpMaterialListStreamGetSize(reinterpret_cast<const RpMaterialList*>(&w->matList))) + 100 + acc;
    }

    const RwUInt32 worldPluginSize = static_cast<RwUInt32>(_rwPluginRegistryGetSize(&worldTKList, world));
    if (_rwStreamWriteVersionedChunkHeader(stream, rwID_WORLD /*0x0B*/,
                                           static_cast<RwInt32>(total + 0xC + worldPluginSize),
                                           0x36003, 0xFFFF) == nullptr) {
        return nullptr;
    }
    if (_rwStreamWriteVersionedChunkHeader(stream, rwID_STRUCT /*0x01*/, 0x40,
                                           0x36003, 0xFFFF) == nullptr) {
        return nullptr;
    }

    // Обход BSP: счётчики секторов (стек правых поддеревьев, глубина <= rpWORLDMAXBSPDEPTH=64)
    RwUInt32 numWorldSectors  = 0;
    RwUInt32 numTriangles     = 0;
    RwUInt32 numVertices      = 0;
    RwUInt32 numPlaneSectors  = 0;
    const RpSectorBSP* stack[rpWORLDMAXBSPDEPTH /*64*/];
    RwInt32            depth  = 0;
    const RpSectorBSP* cur    = root;
    do {
        if (cur->type < 0) {
            numWorldSectors++;
            numTriangles += reinterpret_cast<const RpWorldSectorGta*>(cur)->numTriangles;
            const RpSectorBSP* next = stack[depth];          // pop (decompile: aiStack_100[iVar2])
            numVertices += reinterpret_cast<const RpWorldSectorGta*>(cur)->numVertices;
            depth--;
            cur = next;
        } else {
            numPlaneSectors++;
            const RpSectorBSP* left = cur->leftSubTree;
            depth++;
            stack[depth] = cur->rightSubTree;                // push right
            cur = left;
        }
    } while (depth >= 0);

    {   // struct-чанк 0x40 байт
        struct WorldChunkGta {
            RwUInt32 rootIsWorldSector;   // 0x00
            RwV3d    origin;              // 0x04 (на диске инвертирован)
            RwUInt32 numTriangles;        // 0x10
            RwUInt32 numVertices;         // 0x14
            RwUInt32 numPlaneSectors;     // 0x18
            RwUInt32 numWorldSectors;     // 0x1C
            RwUInt32 collSector;          // 0x20
            RwUInt32 format;              // 0x24
            RwBBox   boundingBox;         // 0x28
        };
        // sizeof == 0x40: 4+12+4*5+4+24
        WorldChunkGta chunk;
        chunk.rootIsWorldSector = (root->type < 0) ? 1u : 0u;
        chunk.origin.x = w->worldOrigin.x * -1.0f;   // _DAT_00858c1c = -1.0f
        chunk.origin.y = w->worldOrigin.y * -1.0f;
        chunk.origin.z = w->worldOrigin.z * -1.0f;
        chunk.numTriangles    = numTriangles;
        chunk.numVertices     = numVertices;
        chunk.numPlaneSectors = numPlaneSectors;
        chunk.numWorldSectors = numWorldSectors;
        chunk.collSector      = 0;
        // (char)numTexCoordSets в CONCAT11(0x40,..) => 0x4000 | (u8)numTexCoordSets, затем <<16
        chunk.format = flags | static_cast<RwUInt32>(0x4000 | (w->numTexCoordSets & 0xFF)) << 16;
        chunk.boundingBox = w->boundingBox;          // 6 dword из world+0x50
        if (RwStreamWrite(stream, &chunk, 0x40) == nullptr) {
            return nullptr;
        }
    }

    if (_rpMaterialListStreamWrite(reinterpret_cast<const RpMaterialList*>(&w->matList), stream) == nullptr) {
        return nullptr;
    }

    if (root->type < 0) {
        // писатель листа-сектора (вне этажа; см. NOTE о сигнатуре в шапке)
        if (((RwBool(__cdecl*)(const RpSector*, RwStream*, const RpWorld*, RwUInt32))0x7624A0)(
                reinterpret_cast<const RpSector*>(root), stream, world, flags) == FALSE) {
            return nullptr;
        }
    } else {
        // писатель поддерева плоскостей (вне этажа)
        if (((RwBool(__cdecl*)(const RpSector*, RwStream*, const RpWorld*, RwUInt32))0x762740)(
                reinterpret_cast<const RpSector*>(root), stream, world, flags) == FALSE) {
            return nullptr;
        }
    }
    return (_rwPluginRegistryWriteDataChunks(&worldTKList, stream, world) != nullptr) ? world : nullptr;
}


RpWorldSectorChunkInfo* _rpWorldSectorChunkInfoRead(RwStream* stream, RpWorldSectorChunkInfo* worldSectorChunkInfo, RwInt32* bytesRead) {
    RwUInt32 length;
    if (!RwStreamFindChunk(stream, rwID_STRUCT /*0x01*/, &length, nullptr)) {
        return nullptr;
    }
    memset(worldSectorChunkInfo, 0, 0x2C);                       // декомпил: обнуление 11 dword
    if (RwStreamRead(stream, worldSectorChunkInfo, 0x2C) != 0x2C) {
        return nullptr;
    }
    *bytesRead = static_cast<RwInt32>(length + 0xC);
    RwStreamSkip(stream, length - 0x2C);                         // остаток struct-чанка
    return worldSectorChunkInfo;
}


RpPlaneSectorChunkInfo* _rpPlaneSectorChunkInfoRead(RwStream* stream, RpPlaneSectorChunkInfo* planeSectorChunkInfo, RwInt32* bytesRead) {
    RwUInt32 length;
    if (!RwStreamFindChunk(stream, rwID_STRUCT /*0x01*/, &length, nullptr)) {
        return nullptr;
    }
    memset(planeSectorChunkInfo, 0, 0x18);                       // декомпил: обнуление 6 dword
    if (RwStreamRead(stream, planeSectorChunkInfo, 0x18) != 0x18) {
        return nullptr;
    }
    *bytesRead = static_cast<RwInt32>(length + 0xC);
    RwStreamSkip(stream, length - 0x18);                         // остаток struct-чанка
    return planeSectorChunkInfo;
}


RpWorldChunkInfo* _rpWorldChunkInfoRead(RwStream* stream, RpWorldChunkInfo* worldChunkInfo, RwInt32* bytesRead) {
    RwUInt32 length;
    RwUInt32 version;
    if (!RwStreamFindChunk(stream, rwID_STRUCT /*0x01*/, &length, &version)) {
        return nullptr;
    }
    if (version < 0x34000 || 0x36003 < version) {
        RwError rwError;
        rwError.pluginID  = 2;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000004);   // _rwerror
        RwErrorSet(&rwError);
        return nullptr;
    }
    memset(worldChunkInfo, 0, 0x40);                             // декомпил: обнуление 16 dword
    if (version < 0x34001) {
        rpWorldChunkInfo34000 oldInfo;                           // repo rpworld.h:2329
        memset(&oldInfo, 0, 0x34);                               // декомпил: обнуление 13 dword
        if (RwStreamRead(stream, &oldInfo, length) != length) {
            return nullptr;
        }
        worldChunkInfo->rootIsWorldSector = oldInfo.rootIsWorldSector;
        worldChunkInfo->invWorldOrigin    = oldInfo.invWorldOrigin;
        worldChunkInfo->numTriangles      = oldInfo.numTriangles;
        worldChunkInfo->numVertices       = oldInfo.numVertices;
        worldChunkInfo->numPlaneSectors   = oldInfo.numPlaneSectors;
        worldChunkInfo->numWorldSectors   = oldInfo.numWorldSectors;
        worldChunkInfo->colSectorSize     = oldInfo.colSectorSize;
        worldChunkInfo->format            = oldInfo.format;
        // oldInfo.surfaceProps отбрасывается (декомпил: dword-копия перепрыгивает old[4..6])
    } else {
        if (RwStreamRead(stream, worldChunkInfo, length) != length) {
            return nullptr;
        }
    }
    *bytesRead = static_cast<RwInt32>(length + 0xC);
    return worldChunkInfo;
}



/****************************************************************************
_rwD3D9EnableClippingIfNeeded
Purpose:
On entry:
On exit :
*/
void
_rwD3D9EnableClippingIfNeeded(void *object,
    RwUInt32 type)
{
    RwCamera    *camera;
    RwBool      fullyInsideFrustum;

    RWFUNCTION(RWSTRING("_rwD3D9EnableClippingIfNeeded"));
    RWASSERT(object != NULL);

    camera = RwCameraGetCurrentCamera();
    RWASSERT(camera != NULL);

    if (type == rpATOMIC)
    {
        RpAtomic    *atomic;

        atomic = (RpAtomic *)object;

        fullyInsideFrustum =
            RwD3D9CameraIsSphereFullyInsideFrustum(camera,
                RpAtomicGetWorldBoundingSphere(atomic));
    }
    else
    {
        const RpWorldSector   *worldSector;

        RWASSERT(type == ((RwUInt8)rwSECTORATOMIC));

        worldSector = (const RpWorldSector *)object;

        fullyInsideFrustum =
            RwD3D9CameraIsBBoxFullyInsideFrustum(camera,
                RpWorldSectorGetTightBBox(worldSector));
    }

    RwD3D9SetRenderState(D3DRS_CLIPPING, (fullyInsideFrustum == FALSE));

    RWRETURNVOID();
}
