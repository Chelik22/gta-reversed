#include "StdInc.h"

#include "rwcore.h"

// ==== RW-CAMPAIGN wave-C shared decls (consolidated) ====
static RxHeap*& g_rxGlobalHeap = StaticRef<RxHeap*>(0xC9A6B0);
static RwInt32& g_rwRxRenderStateGlobalsOffset = StaticRef<RwInt32>(0xC9BC60); // renderstate/pipeline module
// ----------------------------------------------------------------------
// ==== rwcore-3 COMMON BLOCK (paste ONCE above all other rwcore-3 fragments) ====
// Texture module globals. In the SA build they live at RwEngineInstance + *(RwUInt32*)0xC97B4C
// (module globalsOffset static; same idiom as rwResourcesGlobalsOffset 0xC9BC58 / matrix 0xC979BC
// in the rwplcore batches — decompiles show the add: DAT_00c97b4c + offset + DAT_00c97b24).
// Field offsets below are straight from the decompiles (this struct is private in RW SDK).
struct RwTextureGlobalsGta
{
    RwLinkList                   dictList;           // 0x00 sentinel ring of RwTexDictionary::lInInstance
    RwFreeList*                  textureFreeList;    // 0x08 alloc source for RwTexture objects (engine->memoryAlloc/free)
    RwFreeList*                  texDictFreeList;    // 0x0C alloc source for RwTexDictionary objects
    RwTexDictionary*             currentTexDict;     // 0x10
    RwTextureCallBackRead        readCallBack;       // 0x14
    RwTextureCallBackFind        findCallBack;       // 0x18
    RwBool                       mipmapping;         // 0x1C
    RwBool                       automipmapping;     // 0x20
    RwUInt8                      pad[0xC];           // 0x24..0x2F unused in this batch
    RwTextureCallBackMipmapName  mipmapNameCallBack; // 0x30 tail-called by RwTextureGenerateMipmapName
};
static_assert(sizeof(RwTextureGlobalsGta) == 0x34, "RwTextureGlobalsGta layout");
static_assert(offsetof(RwTextureGlobalsGta, currentTexDict)      == 0x10, "RwTextureGlobalsGta layout");
static_assert(offsetof(RwTextureGlobalsGta, readCallBack)        == 0x14, "RwTextureGlobalsGta layout");
static_assert(offsetof(RwTextureGlobalsGta, findCallBack)        == 0x18, "RwTextureGlobalsGta layout");
static_assert(offsetof(RwTextureGlobalsGta, mipmapping)          == 0x1C, "RwTextureGlobalsGta layout");
static_assert(offsetof(RwTextureGlobalsGta, automipmapping)      == 0x20, "RwTextureGlobalsGta layout");
static_assert(offsetof(RwTextureGlobalsGta, mipmapNameCallBack)  == 0x30, "RwTextureGlobalsGta layout");

// Texture module globalsOffset static (a value, not a pointer).
static RwUInt32& g_rwTextureGlobalsOffset = StaticRef<RwUInt32>(0xC97B4C);
static RwTextureGlobalsGta* RwGetTextureGlobalsGta() {
    return reinterpret_cast<RwTextureGlobalsGta*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + g_rwTextureGlobalsOffset);
}

// GTA SA RwGlobals, head..0x14C — partial view, only what this batch uses. NB the repo rwplcore.h
// RwGlobals layout does NOT match the binary (SA inserts RwFileFunctions at +0xC4; see the
// rwplcore-3 batch notes), hence this file-local view.
struct RwStringFuncsViewSA
{
    RwUInt8           pad0[0x8];         // 0xF0 sprintf/vsprintf
    vecStrcpyFunc     vecStrcpy;         // 0xF8
    vecStrncpyFunc    vecStrncpy;        // 0xFC
    vecStrcatFunc     vecStrcat;         // 0x100
    RwUInt8           pad1[0x10];        // 0x104 strncat/strrchr/strchr/strstr
    vecStrcmpFunc     vecStrcmp;         // 0x114
    RwUInt8           pad2[0x8];         // 0x118 strncmp/stricmp
    vecStrlenFunc     vecStrlen;         // 0x120
    RwUInt8           pad3[0x10];        // 0x124..0x133 strupr/strlwr/strtok/sscanf
};
static_assert(sizeof(RwStringFuncsViewSA) == 0x44, "RwStringFuncsViewSA must cover 0xF0..0x134");
struct RwGlobalsRwcoreGta
{
    RwUInt8            pad0[0x68];           // 0x000..0x067
    void*              textureRasterFnTable; // 0x068 ptr to device fn table; table[0](texture, raster, 0) = set-raster-for-texture
    RwUInt8            pad1[0x84];           // 0x06C..0x0EF
    RwStringFuncsViewSA stringFuncs;         // 0x0F0 (17 used slots; repo RwStringFunctions is 28 ptrs — do not embed)
    RwUInt8            pad2[0x10];           // 0x134..0x143 (raw rwmalloc/rwrealloc/rwcalloc slots — ABI: see rwplcore-3 notes)
    RwMemoryAllocFn    memoryAlloc;          // 0x144 freelist alloc: void* (RwFreeList*, RwUInt32 hint)
    RwMemoryFreeFn     memoryFree;           // 0x148 freelist free:  RwFreeList* (RwFreeList*, void*)
};
static_assert(sizeof(RwGlobalsRwcoreGta) == 0x14C, "RwGlobalsRwcoreGta layout");
static_assert(offsetof(RwGlobalsRwcoreGta, memoryAlloc) == 0x144, "RwGlobalsRwcoreGta layout");
static_assert(offsetof(RwGlobalsRwcoreGta, memoryFree)  == 0x148, "RwGlobalsRwcoreGta layout");
static RwGlobalsRwcoreGta* RwGetGlobalsRwcoreGta() {
    return reinterpret_cast<RwGlobalsRwcoreGta*>(RwEngineInstance);
}

// Plugin registries of the texture module (statics baked in the binary; DAT_008e23cc/DAT_008e23e4).
static RwPluginRegistry& rwTexturePluginRegistry = StaticRef<RwPluginRegistry>(0x8E23CC); // TextureModule
static RwPluginRegistry& rwTexDictPluginRegistry = StaticRef<RwPluginRegistry>(0x8E23E4); // TexDictionaryModule
// ==== END rwcore-3 COMMON BLOCK ====

// ----------------------------------------------------------------------
// ==== rwcore-4 COMMON BLOCK (paste ONCE above all other rwcore-4 fragments) ====
// Partial SA RwGlobals view: only the freelist allocator slots. Binary offsets (rwcore-3 /
// rphanim-1 / rpskin-1 batches: SA inserts RwFileFunctions at +0xC4, which repo rwplcore.h
// RwGlobals lacks, so the RWSRCGLOBAL(memoryAlloc)-based RwFreeListAlloc macro would compute
// a wrong offset there - hence this explicit view).
struct RwGlobalsRwcore4Gta
{
    RwUInt8           pad[0x144];
    void*       (*memoryAlloc)(RwFreeList* fl, RwUInt32 hint);   // +0x144 = RwFreeListAlloc
    RwFreeList* (*memoryFree)(RwFreeList* fl, void* ptr);        // +0x148 = RwFreeListFree
};
static RwGlobalsRwcore4Gta* RwGetGlobalsRwcore4() {
    return reinterpret_cast<RwGlobalsRwcore4Gta*>(RwEngineInstance);
}

// Texture module globals base: RwEngineInstance + *(RwUInt32*)0xC97B4C (module globalsOffset
// static; decompiles show DAT_00c97b4c + off + DAT_00c97b24; same idiom as matrix 0xC979BC /
// resources 0xC9BC58). Slots used by this batch:
//   +0x2C RwTextureCallBackMipmapGeneration (0x7F3C70/0x7F3C90/0x7F3CD0)
//   +0x30 RwTextureCallBackMipmapName       (0x7F3CA0/0x7F3CC0)
// NB: the rwcore-3 batch declares its own view (RwTextureGlobalsGta) of the same block; these
// are independent file-local views and can coexist.
static RwUInt8* RwTextureModuleGlobalsRW4() {
    return reinterpret_cast<RwUInt8*>(RwEngineInstance) + StaticRef<RwUInt32>(0xC97B4C);
}

// ---- D3D9 vertex-buffer manager (driver/d3d9; private RW types, layouts from decompiles
//      0x7F5500/0x7F56A0/0x7F5A00) ----
struct RwD3D9VBFreedNodeR4          // freelist 0xC97B88, alloc hint 0x30411
{
    IDirect3DVertexBuffer9* vertexBuffer;   // +0x00
    RwD3D9VBFreedNodeR4*    next;           // +0x04
};
struct RwD3D9VBBufferNodeR4         // freelist 0xC97B84, alloc hint 0x30411
{
    RwUInt32                freeOffset;     // +0x00 write cursor of this D3D buffer
    RwUInt32                remaining;      // +0x04 free bytes left
    IDirect3DVertexBuffer9* vertexBuffer;   // +0x08
    RwD3D9VBBufferNodeR4*   prev;           // +0x0C (list head = newest; head->prev walks to older)
    RwD3D9VBBufferNodeR4*   next;           // +0x10
};
struct RwD3D9VBStrideNodeR4         // freelist 0xC97B80, alloc hint 0x30411
{
    RwUInt32                stride;         // +0x00
    RwD3D9VBBufferNodeR4*   bufferList;     // +0x04 head
    RwD3D9VBFreedNodeR4*    freedBufferList;// +0x08 records of raw D3D buffers (device-release cleanup elsewhere)
    RwD3D9VBStrideNodeR4*   next;           // +0x0C
};
struct RwD3D9DynVBNodeR4            // freelist 0xC97B90, alloc hint 0x40411
{
    RwUInt32                 size;              // +0x00
    RwUInt32                 used;              // +0x04
    IDirect3DVertexBuffer9*  vertexBuffer;      // +0x08
    void**                   vertexBufferOwner; // +0x0C the void** out-ptr the buffer was created for
    RwD3D9DynVBNodeR4*       next;              // +0x10
};
// module globals (no repo names; all StaticRef):
static RwD3D9VBStrideNodeR4*&     r4VbStrideNodeList = StaticRef<RwD3D9VBStrideNodeR4*>(0xC97B7C);
static RwFreeList*&               r4VbStrideNodeFl   = StaticRef<RwFreeList*>(0xC97B80);
static RwFreeList*&               r4VbBufferNodeFl   = StaticRef<RwFreeList*>(0xC97B84);
static RwFreeList*&               r4VbFreedNodeFl    = StaticRef<RwFreeList*>(0xC97B88);
static RwD3D9DynVBNodeR4*&        r4DynVbList        = StaticRef<RwD3D9DynVBNodeR4*>(0xC97B8C);
static RwFreeList*&               r4DynVbFl          = StaticRef<RwFreeList*>(0xC97B90);
static RwUInt32&                  r4DynVbCurrent     = StaticRef<RwUInt32>(0xC97B94);  // round-robin index
static RwUInt32* const            r4DynVbOffsets     = &StaticRef<RwUInt32>(0xC97B98); // [4] write cursors
static RwUInt32* const            r4DynVbSizes       = &StaticRef<RwUInt32>(0xC97BA8); // [4] buffer sizes
static IDirect3DVertexBuffer9** const r4DynVbBuffers =
    reinterpret_cast<IDirect3DVertexBuffer9**>(&StaticRef<void*>(0xC97BB8));            // [4] D3D buffers
static RwUInt32&                  r4D3D9DriverFlags  = StaticRef<RwUInt32>(0xC9BF1C);  // bit0x8000: dynVB pool select; bit0x10000: 4-buffer round-robin
static RwUInt32&                  r4VbDefaultSize    = StaticRef<RwUInt32>(0x8E2414);  // written here; consumed aligned by RwD3D9CreateVertexBuffer

// ---- D3D9 engine/device module (decompiles 0x7F84F0/0x7F8580/0x7F8640/0x7F8A90/0x7F5F60) ----
// Video-mode table entry (stride 0x14, table at 0xC97C48; fields from 0x7F8640 success path)
struct RwD3D9ModeEntryR4
{
    RwUInt32 width;             // +0x00 -> r4DisplayWidth
    RwUInt32 height;            // +0x04 -> r4DisplayHeight
    RwUInt32 field08;           // +0x08 -> r4ModeField0C (0xC9BEEC)
    D3DFORMAT format;           // +0x0C -> r4DisplayFormat (bpp derived: 21/22/35->32, 23/24/25->16)
    RwUInt32 windowed;          // +0x10 -> r4DisplayWindowed (passed to 0x7F6CB0)
};
// Per-light cache entry (array at 0xC98088, stride 0x6C = sizeof(D3DLIGHT9) + enable flag;
// re-applied by SetLight/LightEnable after device Reset in 0x7F8640/0x7F8A90)
struct RwD3D9LightCacheEntryR4
{
    D3DLIGHT9 light;            // +0x00 (0x68 bytes)
    RwUInt32  enabled;          // +0x68 (BOOL passed to LightEnable)
};
static RwInt32&                   r4DevicePluginOffset = StaticRef<RwInt32>(0xB4E9E0); // result of device plugin registration (0x7F5F60)
static RwInt32&                   r4CurrentModeIndex   = StaticRef<RwInt32>(0xC97C18); // active video mode
static HWND&                      r4DeviceWindow       = StaticRef<HWND>(0xC97C1C);    // game window
static IDirect3D9*&               r4D3D9               = StaticRef<IDirect3D9*>(0xC97C20);
static RwUInt32&                  r4D3D9Adapter        = StaticRef<RwUInt32>(0xC97C24);
static RwUInt32&                  r4DeviceOpen         = StaticRef<RwUInt32>(0xC97C38); // engine/device open gate
static RwUInt32&                  r4VideoModeCount     = StaticRef<RwUInt32>(0xC97C40);
static RwD3D9ModeEntryR4* const   r4VideoModes         = reinterpret_cast<RwD3D9ModeEntryR4*>(0xC97C48); // stride 0x14
static IDirect3DSurface9*&        r4BackBufferScratch  = StaticRef<IDirect3DSurface9*>(0xC97C2C);
static IDirect3DSurface9*&        r4RenderTargetScratch= StaticRef<IDirect3DSurface9*>(0xC97C30);
static RwUInt32&                  r4MultiThreadSafe    = StaticRef<RwUInt32>(0xC97C4C);
static RwUInt32&                  r4SoftwareVP         = StaticRef<RwUInt32>(0xC97C50);
static RwUInt32&                  r4MaxMultiSampling   = StaticRef<RwUInt32>(0x8E242C);
static RwUInt32&                  r4MultiSampleType    = StaticRef<RwUInt32>(0x8E2430);
static RwUInt32&                  r4MultiSampleQualityN= StaticRef<RwUInt32>(0x8E2434); // numerator of the quality formula
static RwUInt32&                  r4MultiSampleQuality = StaticRef<RwUInt32>(0x8E2438);
static RwUInt32&                  r4RefreshRate        = StaticRef<RwUInt32>(0x8E243C);
static RwUInt32&                  r4DisplayWidth       = StaticRef<RwUInt32>(0xC9BEE4);
static RwUInt32&                  r4DisplayHeight      = StaticRef<RwUInt32>(0xC9BEE8);
static RwUInt32&                  r4ModeField0C        = StaticRef<RwUInt32>(0xC9BEEC); // modeEntry[2] (unknown use)
static D3DFORMAT&                 r4DisplayFormat      = StaticRef<D3DFORMAT>(0xC9BEF0);
static RwUInt32&                  r4DisplayBpp         = StaticRef<RwUInt32>(0xC9BEF4);
static RwUInt32&                  r4DisplayWindowed    = StaticRef<RwUInt32>(0xC9BEF8); // bit0 consulted by multisampling checks
static RwUInt32&                  r4PresentWindowed    = StaticRef<RwUInt32>(0xC9C060); // window-style branch in ChangeVideoMode
static D3DPRESENT_PARAMETERS&     r4PresentParams      = StaticRef<D3DPRESENT_PARAMETERS>(0xC9C040);
static RwUInt32&                  r4LightCacheCount    = StaticRef<RwUInt32>(0xC98084);
static RwD3D9LightCacheEntryR4* const r4LightCache     = reinterpret_cast<RwD3D9LightCacheEntryR4*>(0xC98088); // stride 0x6C
static rwD3D9DeviceRestoreCallBack& r4DeviceRestoreCb = StaticRef<rwD3D9DeviceRestoreCallBack>(0xC980B0);
// ==== END rwcore-4 COMMON BLOCK ====

// ----------------------------------------------------------------------
// ==== [rwcore-6] SHARED DECLS (ИНТЕГРАТОРУ: вставить блок ниже ОДИН раз в
// rwcore.cpp выше первого фрагмента батча; в остальных фрагментах НЕ повторять) ====
// ============================================================================
// ============================================================================

// Raster-module globalsOffset (DAT_00c980d8): блок глобалов растерного модуля лежит
// по адресу RwEngineInstance + offset — тот же паттерн, что 0xC979BC (matrix) /
// 0xC9BC58 (resources) / 0xC9A6F0 (error) в rwplcore.cpp.
static RwInt32 &rwRasterGlobalsOffsetGta = StaticRef<RwInt32>(0xC980D8);

// Раскладка блока по декомпилам 0x7FB060/0x7FB110/0x7FAE60/0x7FAEE0/0x7FB230:
//   +0x00 contexts[rwRASTERCONTEXTSTACKSIZE=10], +0x28 curContextIndex, +0x60 raster RwFreeList.
struct RwRasterGlobalsGta
{
    RwRaster*   contexts[rwRASTERCONTEXTSTACKSIZE]; /* 0x00 */
    RwInt32     curContextIndex;                    /* 0x28 */
    RwUInt8     _pad[0x60 - 0x2C];                  /* 0x2C..0x5F */
    RwFreeList* rasterFreeList;                     /* 0x60 */
};
static RwRasterGlobalsGta *RwGetRasterGlobalsGta() {
    return reinterpret_cast<RwRasterGlobalsGta*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + rwRasterGlobalsOffsetGta);
}

// stdFunc[] лежит в RwGlobals на +0x48 (RwDevice = 0x38 байт на +0x10). Сверено декомпилом:
// RwRasterCreate зовёт +0x58 = stdFunc[rwSTANDARDRASTERCREATE=4] ... +0xB8 = stdFunc[28]
// (rwSTANDARDRASTERGETMIPLEVELS) — все 15 задействованных слотов совпадают с rwSTANDARD* из rwplcore.h.
static RwStandardFunc *RwGetStdFuncsGta() {
    return reinterpret_cast<RwStandardFunc*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x48);
}

// Freelist-аллокатор RwGlobals: +0x144 alloc / +0x148 free (декомпил RwRasterCreate/RwRasterDestroy;
// RwMemoryAllocFn/RwMemoryFreeFn из rwplcore.h имеют ту же форму — (RwFreeList*, ...)).
using RwRasterFreeListAllocFnGta = void*(__cdecl*)(RwFreeList*, RwUInt32);
using RwRasterFreeListFreeFnGta  = void (__cdecl*)(RwFreeList*, void*);
static RwRasterFreeListAllocFnGta RwGetFreelistAllocGta() {
    return *reinterpret_cast<RwRasterFreeListAllocFnGta*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x144);
}
static RwRasterFreeListFreeFnGta RwGetFreelistFreeGta() {
    return *reinterpret_cast<RwRasterFreeListFreeFnGta*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x148);
}

// Плагин-реестр растерного модуля (DAT_008e2518; в _rwPluginRegistry* передаётся &реестра).
static RwPluginRegistry &rwRasterPluginRegistryGta = StaticRef<RwPluginRegistry>(0x8E2518);

// D3D9 device-restore callback (DAT_00c980b0).
static rwD3D9DeviceRestoreCallBack &rwD3D9DeviceRestoreCBGta = StaticRef<rwD3D9DeviceRestoreCallBack>(0xC980B0);
// ==== КОНЕЦ [rwcore-6] SHARED DECLS ====


// ----------------------------------------------------------------------
// ===================== [rwcore-7 D3D9 SHARED BLOCK — paste once] =====================
// Кэш D3D9-состояний драйвера GTA SA. Раскладка выведена из декомпилов; размеры массивов
// замощены стыками секций .data (следующий глобал начинается ровно на границе):
//   0xC98B40 rsDirtyQueue[210] -> 0xC98E88 rsDeviceVals[210] -> 0xC991D0 rsCache[210 пар]
//   -> 0xC99860 tssDeviceVals[264] -> 0xC99C80 tssQueue[264 пар] -> (0xC9A0A0)
//   0xC980F8 tssCache[264 пар] -> 0xC98938 samplerCache[8][14] -> 0xC98AF8 matCache(D3DMATERIAL9,
//   68 байт) -> 0xC98B40. Индекс tss: stage*33+state (макс 7*33+32 = 263).
// device-таблица: DAT_00c97c28 — уже объявлена в rwcore.h:3635 как _RwD3DDevice
//   (LPDIRECT3DDEVICE9); смещения vtable декомпила (SetMaterial=0xC4, SetClipPlane=0xDC,
//   SetRenderState=0xE4, SetTexture=0x104, SetTextureStageState=0x10C, SetSamplerState=0x114)
//   совпадают со стандартным IDirect3DDevice9 из Windows SDK d3d9.h — проверено подсчётом
//   методов интерфейса (компиляция идёт с тем же d3d9.h).
struct RwD3D9StateCacheEntryGta
{
    RwUInt32 value; // текущее кэшированное значение (устанавливает RwD3D9SetRenderState и др.)
    RwUInt32 dirty; // 1 = состояние стоит в dirty-очереди, ещё не сброшено на девайс
};
struct RwD3D9StageStatePairGta { RwUInt32 stage; RwUInt32 state; };
static inline auto& g_d3d9RsDirtyQueue  = StaticRef<RwUInt32[210]>(0xC98B40);
static inline auto& g_d3d9RsDeviceVals  = StaticRef<RwUInt32[210]>(0xC98E88);
static inline auto& g_d3d9RsCache       = StaticRef<RwD3D9StateCacheEntryGta[210]>(0xC991D0);
static inline auto& g_d3d9RsFlushCount  = StaticRef<RwUInt32>(0xC9A5EC);
static inline auto& g_d3d9TssDeviceVals = StaticRef<RwUInt32[264]>(0xC99860);
static inline auto& g_d3d9TssQueue      = StaticRef<RwD3D9StageStatePairGta[264]>(0xC99C80);
static inline auto& g_d3d9TssCache      = StaticRef<RwD3D9StateCacheEntryGta[264]>(0xC980F8);
static inline auto& g_d3d9TssFlushCount = StaticRef<RwUInt32>(0xC9A5F0);
static inline auto& g_d3d9SamplerCache  = StaticRef<RwUInt32[8 * 14]>(0xC98938); // [stage*14 + D3DSAMP_]
static inline auto& g_d3d9MatCache      = StaticRef<D3DMATERIAL9>(0xC98AF8);     // зеркало последнего SetMaterial (сравнение/копирование dword-ами в декомпиле)
// --- RwD3D9SetSurfaceProperties / RwD3D9SetMaterial caches (0xC9A5D8..0xC9A600) ---
static inline auto& g_d3d9SurfaceAmbientCache   = StaticRef<RwReal>(0xC9A600);  // props->ambient последнего вызова
static inline auto& g_d3d9SurfaceDiffuseCache   = StaticRef<RwReal>(0xC9A5FC);  // props->diffuse последнего вызова
static inline auto& g_d3d9SurfaceColorCache     = StaticRef<RwUInt32>(0xC9A5F8);// *(RwUInt32*)color последнего вызова
static inline auto& g_d3d9SurfaceFlagsCache     = StaticRef<RwUInt32>(0xC9A5F4);// (flags&0x48); RwD3D9SetMaterial пишет сюда 0
static inline auto& g_d3d9LightAmbientCacheR    = StaticRef<RwReal>(0xC9A5D8);  // кэш AmbientSaturated.red  (0x8E2418)
static inline auto& g_d3d9LightAmbientCacheG    = StaticRef<RwReal>(0xC9A5DC);  // кэш AmbientSaturated.green (0x8E241C)
static inline auto& g_d3d9LightAmbientCacheB    = StaticRef<RwReal>(0xC9A5E0);  // кэш AmbientSaturated.blue  (0x8E2420)
// --- RwD3D9SetTexture: per-stage кэш привязки текстур (stride 6 dword, 8 stages) ---
struct RwD3D9TexStageCacheGta
{
    RwUInt32 bound;     // +0x00 0xC9A508: !=0 пока на stage привязана текстура
    RwUInt32 addrU;     // +0x04 0xC9A50C
    RwUInt32 addrV;     // +0x08 0xC9A510
    RwUInt32 filterIdx; // +0x0C 0xC9A514
    RwUInt32 unused10;  // +0x10 0xC9A518 (в декомпиле не используется)
    RwUInt32 aniso;     // +0x14 0xC9A51C
};
static inline auto& g_d3d9TexStageCache          = StaticRef<RwD3D9TexStageCacheGta[8]>(0xC9A508);
static inline auto& g_d3d9TexStage0AlphaActive   = StaticRef<RwUInt32>(0xC9A4EC); // stage-0 текстура держит ALPHABLEND/ALPHATEST включёнными
static inline auto& g_d3d9TexStage0AlphaFlushGate= StaticRef<RwUInt32>(0xC9A4E8); // !=0 => не ставить ALPHABLEND/ALPHATEST в 0
static inline auto& g_d3d9AnisoTexOffset         = StaticRef<RwInt32>(0xC9A5E8);  // байтовый офсет anisotropy-поля в RwTexture (ставит RpAnisotPluginAttach); <=0 = плагин не подключён
// --- .rdata таблицы трансляции RW -> D3D (значения вычитаны из gta_sa.exe) ---
static inline auto& g_d3d9TexAddrModeTable = StaticRef<const RwUInt32[4]>(0x88493C); // RwTextureAddress(0..3) -> D3DTADDRESS_ (0,1,2,3; индексы >3 в бинаре читают за таблицей)
struct RwD3D9FilterPairGta { RwUInt32 minMagFilter; RwUInt32 mipFilter; };
static inline auto& g_d3d9TexFilterTable   = StaticRef<const RwD3D9FilterPairGta[9]>(0x884950); // RwTextureFilterMode -> (min/mag, mip)
// --- D3DMATERIAL9, собираемый RwD3D9SetSurfaceProperties (передаётся в device->SetMaterial) ---
static inline auto& g_d3d9SurfaceMaterial = StaticRef<D3DMATERIAL9>(0x8E2538);
static inline auto& g_d3d9SceneAmbient    = StaticRef<RwRGBAReal>(0x8E2418); // тот же глобал, что RenderWare.h AmbientSaturated
// ===================== [rwcore-7 D3D9 SHARED BLOCK — конец] =====================

// ----------------------------------------------------------------------
// ===================== [rwcore-7 IMAGE SHARED BLOCK — paste once] =====================
// Модуль image держит свои глобалы ВНУТРИ RwGlobals-блока: RwEngineInstance + *(int*)0xC9A65C
// (паттерн тот же, что rwResourcesGlobalsOffset DAT_00c9bc58 в rwplcore.cpp; доказано
// дизассемблом gta_sa.exe 0x8030C0: mov eax,[0xC97B24]; mov ecx,[0xC9A65C]; esi,[ecx+eax+4]).
struct RwImageModuleGta
{
    RwFreeList* imageFreeList;   // +0x000 (источник аллокаций: memoryAlloc(fl, 0x30018))
    RwChar*     imagePath;       // +0x004 (буфер пути, RwImageSetPath)
    RwInt32     imagePathSize;   // +0x008 (ёмкость буфера)
    RwUInt8     gammaTable[256];     // +0x00C (r=g=b=gammaTable[v], alpha не трогаем)
    RwUInt8     invGammaTable[256];  // +0x10C (обратная таблица)
    RwReal      gamma;               // +0x20C (текущая гамма)
    RwUInt8     pad210[0x10];        // +0x210..0x21B (не используется в этом батче)
    void*       imageReaders;        // +0x21C (список форматов-ридеров, +0x30 = next)
};
struct RwImageReaderNodeGta
{
    RwChar*  ext1;              // +0x00 расширение ВКЛЮЧАЯ точку; vecStrcmp==0 => совпало
    RwUInt8  pad04[0x10];       // +0x04
    RwChar*  ext2;              // +0x14 второе расширение
    RwUInt8  pad18[0x10];       // +0x18
    RwImageCallBackRead read;   // +0x28 RwImageCallBackRead
    RwImageReaderNodeGta* next;// +0x30
};
inline RwImageModuleGta& RwGetImageModuleGta() {
    return *reinterpret_cast<RwImageModuleGta*>(
        reinterpret_cast<RwUInt8*>(RwEngineInstance) + StaticRef<RwInt32>(0xC9A65C));
}
// Реестр плагинов image-объекта (декомпил: DAT_008e2670, аргумент _rwPluginRegistryInitObject).
static inline auto& rwImagePluginRegistry = StaticRef<RwPluginRegistry>(0x8E2670);
// Доступ к областям RwGlobals по офсетам бинара (официальная раскладка rwplcore.h НЕ совпадает;
// сверено декомпилами RwEngineInit 0x7F3170 / RwOsGetFileInterface 0x804130):
inline RwStringFunctions& RwGtaStringFuncs() {   // +0xF0 (раскладка структуры — официальная)
    return *reinterpret_cast<RwStringFunctions*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0xF0);
}
inline RwMemoryFunctions& RwGtaMemFuncs() {      // +0x134: rwmalloc/rwfree/rwrealloc/rwcalloc
    return *reinterpret_cast<RwMemoryFunctions*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x134);
}
inline RwMemoryAllocFn& RwGtaFreelistAlloc() {   // +0x144 memoryAlloc(RwFreeList*, hint)
    return *reinterpret_cast<RwMemoryAllocFn*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x144);
}
inline RwMemoryFreeFn& RwGtaFreelistFree() {     // +0x148 memoryFree(RwFreeList*, void*)
    return *reinterpret_cast<RwMemoryFreeFn*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x148);
}
inline RwStandardFunc RwGtaStandardFunc(RwUInt32 idx) { // +0x48 stdFunc[29]
    return *reinterpret_cast<RwStandardFunc*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x48 + idx * 4);
}
// Хелперы вне этажа (нет декомпилов; сигнатуры сняты дизассемблированием gta_sa.exe):
//   0x803790: int __cdecl f(RwImage* dst, RwImage* src) — конверсия при равной глубине
//             (палитра + пиксели, esp+8/esp+0xC — 2 stack-аргумента)
//   0x803840: int __cdecl f(RwImage* dst, RwImage* src) — конверсия между глубинами
//   0x8088D0: RwInt32 __cdecl f(RwInt32 code, RwInt32 param) — identity (mov eax,[esp+4]; ret)
//   0x8030C0: RwImage* __cdecl f(const RwChar* name, RwInt32 mode, void* callback, void* ctx)
//   0x8032D0: int __cdecl cb(const RwChar* name, void* ctx); ctx = {RwImageCallBackRead fn@0; RwImage* result@4}
// ===================== [rwcore-7 IMAGE SHARED BLOCK — конец] =====================

// ----------------------------------------------------------------------
// ==== rwcore-10 COMMON BLOCK (paste ONCE above all other rwcore-10 fragments) ====
// Frame/Camera module plugin registries — private statics of the binary (DAT_008e2270/DAT_008e222c,
// decompiles of RwFrameRegisterPluginStream 0x807580 / RwCameraRegisterPluginStream 0x808C90).
static RwPluginRegistry& rwFramePluginRegistryGta  = StaticRef<RwPluginRegistry>(0x8E2270);
static RwPluginRegistry& rwCameraPluginRegistryGta = StaticRef<RwPluginRegistry>(0x8E222C);
// Frame module "stream-read attaches with update" flag (DAT_008e26a8; set by
// RwFrameListSetAutoUpdate 0x807570, read by _rwFrameListStreamRead 0x807970).
static RwUInt32& rwFrameListAutoUpdateGta = StaticRef<RwUInt32>(0x8E26A8);
// .rdata float thresholds used by the frame-matrix validation in _rwFrameListStreamRead
// (_DAT_00858c58 shared by the two error predicates, _DAT_00862cd0 by the determinant one).
// Exact literal values not decoded here — referenced in place, 1:1 with the binary.
static const RwReal& rwMatrixToleranceGta = StaticRef<const RwReal>(0x858C58);
static const RwReal& rwMatrixDetMinGta    = StaticRef<const RwReal>(0x862CD0);
// rwmalloc/rwfree live in RwMemoryFunctions at RwEngineInstance+0x134 (layout per rwplcore-3/4 notes;
// repo rwplcore.h RwGlobals does NOT match the binary offsets).
inline RwMemoryFunctions* RwGetMemoryFunctionsGta() {
    return reinterpret_cast<RwMemoryFunctions*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0x134);
}
// ==== END rwcore-10 COMMON BLOCK ====

// ----------------------------------------------------------------------
// ==== rwcore-11 COMMON BLOCK (paste ONCE above all other rwcore-11 fragments) ====
// GTA SA RwGlobals partial view. Offsets are decompile facts of this batch:
//   DAT_00c97b24+0xF0  = vecSprintf  (RxHeapAlloc "Heap resized from %d to %d bytes" trace)
//   DAT_00c97b24+0x134 = rwmalloc(size, hint)   (RxHeapCreate/Alloc/RwImageResample mallocs)
//   DAT_00c97b24+0x138 = rwfree(ptr)            (RxHeapDestroy/Alloc/Resample frees)
//   DAT_00c97b24+0x144 = memoryAlloc(freelist, hint) (RwPalQuantInit/AddImage node allocs)
//   DAT_00c97b24+0x148 = memoryFree(freelist, entry) (RwPalQuantTerm node free)
//   DAT_00c97b24+0x150 = engineStatus (RwEngineStatus; ==rwENGINESTATUSSTARTED(3) in
//                                        RxRenderStateVectorSetDefaultRenderStateVector)
struct RwGlobalsFnViewGta
{
    RwUInt8  pad[0xF0];                                            // 0x000..0x0EF
    RwStringFunctions stringFuncs;                                 // 0x0F0..0x133 (_rwStringOpen writes the 17 fn ptrs here; rwplcore.h type)
    void   *(*rwmalloc)(RwUInt32 size, RwUInt32 hint);             // 0x134 (RwMemoryFunctions.rwmalloc)
    void    (*rwfree)(void *mem);                                  // 0x138 (RwMemoryFunctions.rwfree)
    RwUInt8  pad3[0x8];                                            // 0x13C..0x143
    void   *(*memoryAlloc)(RwFreeList *fl, RwUInt32 hint);         // 0x144 (RwGlobals.memoryAlloc, freelist-shaped)
    void    (*memoryFree)(RwFreeList *fl, void *entry);            // 0x148 (RwGlobals.memoryFree)
    RwUInt8  pad4[0x4];                                            // 0x14C..0x14F
    RwInt32  engineStatus;                                         // 0x150 (RwGlobalsSA.engineStatus)
};
static_assert(sizeof(RwGlobalsFnViewGta) == 0x154, "RwGlobalsFnViewGta layout");
static_assert(offsetof(RwGlobalsFnViewGta, stringFuncs)   == 0x0F0, "RwGlobalsFnViewGta layout");
static_assert(offsetof(RwGlobalsFnViewGta, rwmalloc)      == 0x134, "RwGlobalsFnViewGta layout");
static_assert(offsetof(RwGlobalsFnViewGta, rwfree)        == 0x138, "RwGlobalsFnViewGta layout");
static_assert(offsetof(RwGlobalsFnViewGta, memoryAlloc)   == 0x144, "RwGlobalsFnViewGta layout");
static_assert(offsetof(RwGlobalsFnViewGta, memoryFree)    == 0x148, "RwGlobalsFnViewGta layout");
static_assert(offsetof(RwGlobalsFnViewGta, engineStatus)  == 0x150, "RwGlobalsFnViewGta layout");

static RwGlobalsFnViewGta *RwGetGlobalsFnViewGta() {
    return reinterpret_cast<RwGlobalsFnViewGta *>(RwEngineInstance);
}
// ==== END rwcore-11 COMMON BLOCK ====

// ----------------------------------------------------------------------
// ==== rwcore-11 PALQUANT COMMON BLOCK (paste ONCE above the other palquant fragments) ====
// Max quantization depth, set by RwPalQuantSetMaxDepth (decompile DAT_008e26e0).
static RwUInt32 &g_rwPalQuantMaxDepth = StaticRef<RwUInt32>(0x8E26E0);
// Bit-spread table indexed by (channel >> (8 - depth)); built by RwPalQuantInit
// (decompile &DAT_00c9a730, 1<<depth entries, 256 max).
static RwUInt32 (&g_rwPalQuantBitTable)[256] = StaticRef<RwUInt32[256]>(0xC9A730);
// ==== END rwcore-11 PALQUANT COMMON BLOCK ====

// ----------------------------------------------------------------------
// ==== rwcore-11 D3D9 COMMON BLOCK (paste ONCE above the other D3D9 fragments) ====
// R* D3D9 raster extension. Offsets verified against decompiles of 0x81F360 / 0x820C90:
//   +0x00 IDirect3DTexture9*/IDirect3DCubeTexture9* texture
//   +0x08 hasAlpha byte (0/1; = format table [+0] for non-DXT fourcc)
//   +0x09 flags: bit0 = D3D texture attached; 0xF0 nibble = cubemap faces loaded
//   +0x0A flags2: bit 0x10 = DXT1..DXT5 (compressed) — checked by RwD3D9RasterIsCompressed
//   +0x10 pitch (bytes per row; for DXT: bytes per block-row) — set by the raster
//         format-create helpers (0x4CBEE0/0x4CBDE0/0x4CD050)
//   +0x18 format (D3DFORMAT / DDS fourcc)
struct RwD3D9RasterExtGta
{
    void    *texture;   /* +0x00 */
    RwUInt8  pad04[4];  /* +0x04 (unused in this batch) */
    RwUInt8  hasAlpha;  /* +0x08 */
    RwUInt8  flags;     /* +0x09 */
    RwUInt8  flags2;    /* +0x0A */
    RwUInt8  pad0B;     /* +0x0B */
    RwUInt32 pad0C;     /* +0x0C (unused in this batch) */
    RwUInt32 pitch;     /* +0x10 */
    RwUInt32 pad14;     /* +0x14 (unused in this batch) */
    RwUInt32 format;    /* +0x18 */
};
static_assert(sizeof(RwD3D9RasterExtGta) == 0x1C, "RwD3D9RasterExtGta layout");
static_assert(offsetof(RwD3D9RasterExtGta, texture) == 0x00, "RwD3D9RasterExtGta layout");
static_assert(offsetof(RwD3D9RasterExtGta, hasAlpha) == 0x08, "RwD3D9RasterExtGta layout");
static_assert(offsetof(RwD3D9RasterExtGta, flags)    == 0x09, "RwD3D9RasterExtGta layout");
static_assert(offsetof(RwD3D9RasterExtGta, flags2)   == 0x0A, "RwD3D9RasterExtGta layout");
static_assert(offsetof(RwD3D9RasterExtGta, pitch)    == 0x10, "RwD3D9RasterExtGta layout");
static_assert(offsetof(RwD3D9RasterExtGta, format)   == 0x18, "RwD3D9RasterExtGta layout");

// Raster-extension offset static (decompile DAT_00b4e9e0).
static RwUInt32 &g_rwD3D9RasterExtOffset = StaticRef<RwUInt32>(0xB4E9E0);
// Device capability flag dword (decompile _DAT_00c9bf44; & 0x30000 == mip-level support).
static RwUInt32 &g_rwD3D9DeviceCapsFlags = StaticRef<RwUInt32>(0xC9BF44);
// D3DFORMAT property table, indexed [format * 4] (decompile &DAT_00b4e7e0):
//   [+0] alpha-presence byte, [+1] bits per pixel, [+3] alpha bits.
static RwUInt8 (&g_rwD3D9FormatPropTable)[1024] = StaticRef<RwUInt8[1024]>(0xB4E7E0);
// ==== END rwcore-11 D3D9 COMMON BLOCK ====
// ==== END wave-C shared decls ====

void RwCoreInjectHooks() {
    RH_ScopedCategory("Renderware");
    RH_ScopedNamespaceName("RwCore");

    RH_ScopedGlobalInstall(RwCameraBeginUpdate, 0x7EE190);
    RH_ScopedGlobalInstall(RwCameraEndUpdate, 0x7EE180);
    RH_ScopedGlobalInstall(RwCameraClear, 0x7EE340);
    RH_ScopedGlobalInstall(RwCameraShowRaster, 0x7EE370);
}

RxHeap *RxHeapCreate(RwUInt32 size)
{
    if (size < 0x400)
        size = 0x400;

    RxHeap *heap = (RxHeap *)RwGetGlobalsFnViewGta()->rwmalloc(0x1C, 0x40409);
    if (heap == NULL)
        return NULL;

    RwUInt32 sbSize = (size + 0x1F) & ~0x1F;
    if (sbSize < 0x80)
        sbSize = 0x80;

    rxHeapSuperBlockDescriptor *sbd =
        (rxHeapSuperBlockDescriptor *)RwGetGlobalsFnViewGta()->rwmalloc(sbSize + 0x8B, 0x1040409);
    if (sbd == NULL)
    {
        RwGetGlobalsFnViewGta()->rwfree(heap);
        return NULL;
    }

    sbd->size = sbSize;
    sbd->next = NULL;
    sbd->start = (void *)(((RwUInt32)sbd + 0x8B) & ~0x7F);

    heap->superBlockSize = sbSize;
    heap->head           = sbd;
    heap->headBlock      = NULL;
    heap->freeBlocks     = NULL;
    heap->entriesAlloced = 0;
    heap->entriesUsed    = 0;
    heap->dirty          = TRUE;

    // Re-init the pre-existing sbd chain (none here: sbd->next == NULL), then the new one
    {
        rxHeapSuperBlockDescriptor *prev = NULL;
        rxHeapSuperBlockDescriptor *sb;

        for (sb = sbd->next; sb != NULL; sb = sb->next)
        {
            if (((RwBool (__cdecl *)(rxHeapSuperBlockDescriptor *, rxHeapSuperBlockDescriptor *, RxHeap *))0x809BF0)
                    (sb, prev, heap) == FALSE)
                goto fail;
            if (prev == NULL)
                heap->headBlock = static_cast<rxHeapBlockHeader*>(sb->start);
            prev = sb;
        }

        sb = heap->head;
        if (((RwBool (__cdecl *)(rxHeapSuperBlockDescriptor *, rxHeapSuperBlockDescriptor *, RxHeap *))0x809BF0)
                (sb, prev, heap) != FALSE)
        {
            if (prev == NULL)
                heap->headBlock = static_cast<rxHeapBlockHeader*>(sb->start);
            heap->dirty = FALSE;
            return heap;
        }
    }

fail:
    RwGetGlobalsFnViewGta()->rwfree(sbd);
    RwGetGlobalsFnViewGta()->rwfree(heap);
    return NULL;
}


void RxHeapDestroy(RxHeap *heap)
{
    if (heap != NULL)
    {
        if (heap->freeBlocks != NULL)
        {
            RwGetGlobalsFnViewGta()->rwfree(heap->freeBlocks);
            heap->freeBlocks = NULL;
        }

        rxHeapSuperBlockDescriptor *sb = heap->head;
        while (sb != NULL)
        {
            rxHeapSuperBlockDescriptor *next = sb->next;
            RwGetGlobalsFnViewGta()->rwfree(sb);
            sb = next;
        }

        RwGetGlobalsFnViewGta()->rwfree(heap);
    }
}


RwBool _rxHeapReset(RxHeap *heap)
{
    rxHeapSuperBlockDescriptor *prev = NULL;
    rxHeapSuperBlockDescriptor *sb;

    heap->entriesUsed = 0;

    for (sb = heap->head->next; sb != NULL; sb = sb->next)
    {
        if (((RwBool (__cdecl *)(rxHeapSuperBlockDescriptor *, rxHeapSuperBlockDescriptor *, RxHeap *))0x809BF0)
                (sb, prev, heap) == FALSE)
        {
            return FALSE;
        }
        if (prev == NULL)
            heap->headBlock = static_cast<rxHeapBlockHeader*>(sb->start);
        prev = sb;
    }

    sb = heap->head;
    if (((RwBool (__cdecl *)(rxHeapSuperBlockDescriptor *, rxHeapSuperBlockDescriptor *, RxHeap *))0x809BF0)
            (sb, prev, heap) == FALSE)
    {
        return FALSE;
    }
    if (prev == NULL)
        heap->headBlock = static_cast<rxHeapBlockHeader*>(sb->start);

    heap->dirty = FALSE;
    return TRUE;
}


void *RxHeapAlloc(RxHeap *heap, RwUInt32 size)
{
    RwUInt32 alignedSize = (size + 0x1F) & ~0x1F;

    // Search the free-block stack from the most recent entry (top) downwards
    RwUInt32 idx = heap->entriesUsed;
    rxHeapFreeBlock *fb = (rxHeapFreeBlock *)((RwUInt32)heap->freeBlocks - 8 + idx * 8);
    do
    {
        if (alignedSize <= fb->size)
        {
            heap->dirty = TRUE;
            return ((void * (__cdecl *)(RxHeap *, rxHeapFreeBlock *, RwUInt32))0x809C70)(heap, fb, alignedSize);
        }
        fb--;
        idx--;
    } while (idx != 0);

    // No free block big enough: add a new superblock
    RwUInt32 newSBSize = alignedSize + 0x60;
    if (newSBSize < heap->superBlockSize)
        newSBSize = heap->superBlockSize;

    rxHeapSuperBlockDescriptor *sbd =
        (rxHeapSuperBlockDescriptor *)RwGetGlobalsFnViewGta()->rwmalloc(newSBSize + 0x8B, 0x1040409);
    if (sbd != NULL)
    {
        sbd->size = newSBSize;
        sbd->next = NULL;
        // sbd (12 bytes) + padding, first block header 128-byte aligned
        sbd->start = (void *)(((RwUInt32)sbd + 0x8B) & ~0x7F);

        if (((RwBool (__cdecl *)(rxHeapSuperBlockDescriptor *, rxHeapSuperBlockDescriptor *, RxHeap *))0x809BF0)
                (sbd, heap->head, heap) == FALSE)
        {
            RwGetGlobalsFnViewGta()->rwfree(sbd);
        }
        else
        {
            RwUInt32 totalSize;
            sbd->next = heap->head;
            heap->head = sbd;

            totalSize = 0;
            {
                rxHeapSuperBlockDescriptor *sb;
                for (sb = sbd; sb != NULL; sb = sb->next)
                    totalSize += sb->size;
            }
            // Debug trace through engine->stringFuncs.vecSprintf (+0xF0) into a throwaway
            // 256-byte stack buffer ("Heap resized from %d to %d bytes"); result discarded.
            {
                RwChar traceBuf[256];
                RwGetGlobalsFnViewGta()->stringFuncs.vecSprintf
                    (traceBuf, "Heap resized from %d to %d bytes", totalSize - newSBSize, totalSize);
            }

            RwUInt32 firstBlock = (RwUInt32)sbd->start;
            if (firstBlock != 0xFFFFFFE0) /* decompile literal: (uint)-32 */
            {
                heap->dirty = TRUE;
                return ((void * (__cdecl *)(RxHeap *, rxHeapFreeBlock *, RwUInt32))0x809C70)
                        (heap, *(rxHeapFreeBlock **)(firstBlock + 0x2C), alignedSize);
            }
        }
    }

    // Out of memory
    {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl *)(RwInt32, ...))0x8088D0)(0x80000013 /* E_RW_NOMEM */, alignedSize);
        RwErrorSet(&rwError);
    }
    return NULL;
}


void RxHeapFree(RxHeap *heap, void *block)
{
    rxHeapBlockHeader *hdr = (rxHeapBlockHeader *)((RwUInt8 *)block - 0x20);

    // bVar5: previous block exists and is free (has a freeEntry); bVar6: same for next
    RwBool prevFree = (hdr->prev != NULL) && (hdr->prev->freeEntry != NULL);
    RwBool nextFree = (hdr->next != NULL) && (hdr->next->freeEntry != NULL);

    if (prevFree)
    {
        if (nextFree)
        {
            // Merge prev + this + next: recycle next's freeEntry (it is not needed anymore)
            rxHeapFreeBlock *nextEntry = hdr->next->freeEntry;
            if ((rxHeapFreeBlock *)((RwUInt8 *)heap->freeBlocks - 8 + heap->entriesUsed * 8) != nextEntry)
            {
                // nextEntry is not the stack top: move the top entry into its slot
                rxHeapFreeBlock *top = (rxHeapFreeBlock *)((RwUInt8 *)heap->freeBlocks - 8 + heap->entriesUsed * 8);
                nextEntry->size = top->size;
                nextEntry->ptr  = top->ptr;
                top->ptr->freeEntry = nextEntry;
            }
            heap->entriesUsed--;

            // Absorb both headers (+0x40) and the next block into the previous one
            hdr->prev->size += hdr->size + 0x40 + hdr->next->size;
            hdr->prev->freeEntry->size = hdr->prev->size;
            hdr->prev->next = hdr->next;
            if (hdr->next != NULL)
                hdr->next->prev = hdr->prev;
        }
        else
        {
            // Merge into previous only (hdr keeps prev as its prev: the binary re-stores
            // the same value — self-assign no-op in the decompile)
            hdr->prev->size += hdr->size + 0x20;
            hdr->prev->freeEntry->size = hdr->prev->size;
            hdr->prev->next = hdr;
        }
    }
    else if (nextFree)
    {
        // Merge into next: this header becomes the free block header
        // (hdr->next keeps pointing at the same next block — no-op store in the decompile)
        hdr->size += hdr->next->size + 0x20;
        hdr->freeEntry = hdr->next->freeEntry;
        hdr->freeEntry->ptr  = hdr;
        hdr->freeEntry->size = hdr->size;
        if (hdr->next != NULL)
            hdr->next->prev = hdr;
    }
    else
    {
        // Neither neighbour free: push a new free-block stack entry
        rxHeapFreeBlock *entry = ((rxHeapFreeBlock * (__cdecl *)(RxHeap *))0x8099F0)(heap);
        if (entry != NULL)
        {
            entry->ptr  = hdr;
            entry->size = hdr->size;
            hdr->freeEntry = entry;
        }
    }
}


void *RxHeapRealloc(RxHeap *heap, void *block, RwUInt32 newSize, RwBool allowCopy)
{
    (void)allowCopy; /* ignored by the binary */
    rxHeapBlockHeader *hdr = (rxHeapBlockHeader *)((RwUInt8 *)block - 0x20);

    RwUInt32 alignedNew = (newSize + 0x1F) & ~0x1F;
    RwInt32 delta = (RwInt32)(alignedNew - hdr->size);

    if (delta < 1)
    {
        // Shrinking: release a split-off tail only if it frees > 0xFF bytes
        if (-delta > 0xFF)
        {
            rxHeapFreeBlock *entry = ((rxHeapFreeBlock * (__cdecl *)(RxHeap *))0x8099F0)(heap);
            if (entry != NULL)
            {
                rxHeapBlockHeader *tail = (rxHeapBlockHeader *)((RwUInt8 *)block + alignedNew);
                tail->size      = -delta - 0x20;
                tail->prev      = hdr;
                tail->next      = hdr->next;
                tail->freeEntry = entry;
                hdr->size = alignedNew;
                hdr->next = tail;
                if (tail->next != NULL)
                    tail->next->prev = tail;
                entry->ptr  = tail;
                entry->size = tail->size;
            }
        }
        return block;
    }

    // Growing: can we absorb the next block (free) in place?
    if (hdr->next != NULL && hdr->next->freeEntry != NULL &&
        (RwUInt32)delta <= hdr->next->size + 0x20)
    {
        RwInt32 total = hdr->next->size + 0x20 + hdr->size;
        if (total - (RwInt32)alignedNew < 0x100)
        {
            // Absorb the whole next block; recycle its freeEntry if it is not the stack top
            rxHeapFreeBlock *nextEntry = hdr->next->freeEntry;
            if ((rxHeapFreeBlock *)((RwUInt8 *)heap->freeBlocks - 8 + heap->entriesUsed * 8) != nextEntry)
            {
                rxHeapFreeBlock *top = (rxHeapFreeBlock *)((RwUInt8 *)heap->freeBlocks - 8 + heap->entriesUsed * 8);
                nextEntry->size = top->size;
                nextEntry->ptr  = top->ptr;
                top->ptr->freeEntry = nextEntry;
            }
            heap->entriesUsed--;
            hdr->size = total;
            hdr->next = hdr->next->next;
            if (hdr->next != NULL)
                hdr->next->prev = hdr;
        }
        else
        {
            // Split: keep the front, free the back part
            rxHeapBlockHeader *tail = (rxHeapBlockHeader *)((RwUInt8 *)block + alignedNew);
            tail->freeEntry = hdr->next->freeEntry;
            tail->prev      = hdr;
            tail->size      = hdr->next->size - delta;
            tail->next      = hdr->next->next;
            if (tail->next != NULL)
                tail->next->prev = tail;
            hdr->size = alignedNew;
            hdr->next = tail;
            tail->freeEntry->ptr  = tail;
            tail->freeEntry->size = tail->size;
        }
        return block;
    }

    // Cannot resize in place: alloc / copy / free
    {
        void *newBlock = RxHeapAlloc(heap, alignedNew);
        if (newBlock != NULL)
        {
            RwUInt32 oldSize = hdr->size; /* allocated size incl. rounding */
            {
                RwUInt32 *src = (RwUInt32 *)block;
                RwUInt32 *dst = (RwUInt32 *)newBlock;
                RwUInt32 i;
                for (i = oldSize >> 2; i != 0; i--)
                    *dst++ = *src++;
                RwUInt8 *srcB = (RwUInt8 *)src;
                RwUInt8 *dstB = (RwUInt8 *)dst;
                for (i = oldSize & 3; i != 0; i--)
                    *dstB++ = *srcB++;
            }
            RxHeapFree(heap, block);
            return newBlock;
        }
    }

    // Out of memory
    {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl *)(RwInt32, ...))0x8088D0)(0x80000013 /* E_RW_NOMEM */, alignedNew);
        RwErrorSet(&rwError);
    }
    return NULL;
}


void RxPipelineSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc)
{
    StaticRef<RwInt32>(0x8E26A0) = blockSize;
    StaticRef<RwInt32>(0x8E26A4) = numBlocksToPrealloc;
}


RwBool _rxPipelineOpen()
{
    RwBool& pipelineModuleOpen = StaticRef<RwBool>(0xC9A6AC);

    if (pipelineModuleOpen == FALSE) {
        RxHeap*& globalHeap = StaticRef<RxHeap*>(0xC9A6B0);
        globalHeap = RxHeapCreate(StaticRef<RwUInt32>(0x8E2698));
        if (globalHeap != NULL) {
            RwFreeList* pipelineFreeList = RwFreeListCreateAndPreallocateSpace(
                0x34,
                StaticRef<RwInt32>(0x8E26A0),
                4,
                StaticRef<RwInt32>(0x8E26A4),
                &StaticRef<RwFreeList>(0xC9A688),
                0x40409);

            RwUInt8* moduleBase = (RwUInt8*)RwEngineInstance + StaticRef<RwInt32>(0xC9BC60);
            *(RwFreeList**)moduleBase = pipelineFreeList;

            if (pipelineFreeList != NULL) {
                *(RwUInt32*)(moduleBase + 0x38) = StaticRef<RwUInt32>(0x8E269C);
                RxRenderStateVectorSetDefaultRenderStateVector((RxRenderStateVector*)(moduleBase + 0x04));
                *(RwUInt32*)(moduleBase + 0x34) = 0;
                *(RwUInt32*)(moduleBase + 0x30) = 0;

                pipelineModuleOpen = TRUE;
                return TRUE;
            }

            RxHeapDestroy(globalHeap);
            globalHeap = NULL;
        }
    }
    return FALSE;
}


RwBool _rxPipelineClose()
{
    RwBool& pipelineModuleOpen = StaticRef<RwBool>(0xC9A6AC);

    if (pipelineModuleOpen != FALSE) {
        RwFreeList*& pipelineFreeList = *(RwFreeList**)((RwUInt8*)RwEngineInstance + StaticRef<RwInt32>(0xC9BC60));
        RwFreeListDestroy(pipelineFreeList);
        pipelineFreeList = NULL;

        RxHeap*& globalHeap = StaticRef<RxHeap*>(0xC9A6B0);
        RxHeapDestroy(globalHeap);
        globalHeap = NULL;

        pipelineModuleOpen = FALSE;
    }
    return TRUE;
}


RxPipeline* RxPipelineCreate() {
    // decompile 0x8057B0: alloc from the pipeline free list (RwEngineInstance+globalsOffset cell,
    // freelist alloc via RwGlobals->memoryAlloc +0x144 == RwFreeListAlloc macro, hint 0x30409),
    // zero 13 dwords == sizeof(RxPipeline) == 0x34, NULL -> RwError{pluginID=1, nudge(0x80000013,0x34)}.
    RwFreeList*& pipelineFreeList = *(RwFreeList**)((RwUInt8*)RwEngineInstance + StaticRef<RwInt32>(0xC9BC60));
    RxPipeline*  pipeline         = static_cast<RxPipeline*>(RwFreeListAlloc(pipelineFreeList, 0x30409));
    if (pipeline == nullptr) {
        RwError rwError;
        rwError.pluginID  = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, RwInt32))0x8088D0)(0x80000013, 0x34);
        RwErrorSet(&rwError);
        return nullptr;
    }
    memset(pipeline, 0, sizeof(RxPipeline)); // 0x34 bytes / 13 dwords, matches decompile zero loop
    return pipeline;
}


void _rxPipelineDestroy(RxPipeline* Pipeline) {
    // decompile 0x805820: tear down every node (internal helper 0x8050C0, outside floor),
    // drop the superBlock (rwfree == RwFree, RwGlobals+0x138), free back to the pipeline free list
    // (RwGlobals->memoryFree +0x148 == RwFreeListFree macro).
    if (Pipeline != nullptr) {
        for (RwUInt32 i = 0; i < Pipeline->numNodes; i++) {
            ((void(__cdecl*)(RxPipelineNode*, RxPipeline*))0x8050C0)(&Pipeline->nodes[i], Pipeline);
        }
        Pipeline->nodes = nullptr;
        if (Pipeline->superBlock != nullptr) {
            RwFree(Pipeline->superBlock);
            Pipeline->superBlock    = nullptr;
            Pipeline->superBlockSize = 0;
        }
        RwFreeListFree(*(RwFreeList**)((RwUInt8*)RwEngineInstance + StaticRef<RwInt32>(0xC9BC60)), Pipeline);
    }
}


RxHeap* RxHeapGetGlobalHeap()
{
    return StaticRef<RxHeap*>(0xC9A6B0);
}


RxPipeline* RxPipelineExecute(RxPipeline* pipeline, void* data, RwBool heapReset) {
    // decompile 0x805710: optionally reset the dirty global heap, fill the global RxExecutionContext,
    // run nodes[0].nodeDef->nodeMethods.nodeBody on the first node, then destroy the embedded packet
    // if one was created; exitCode decides pipeline / NULL.
    RxHeap* heap = StaticRef<RxHeap*>(0xC9A6B0);
    if (heapReset != FALSE && heap->dirty != FALSE)
        _rxHeapReset(heap);

    RxExecutionContext& execCtx = StaticRef<RxExecutionContext>(0xC9BC40); // +0x00 pipeline, +0x08 exitCode, +0x10 params
    execCtx.exitCode      = TRUE;                                          // 0xC9BC48
    execCtx.pipeline      = pipeline;                                      // 0xC9BC40
    execCtx.params.dataParam = data;                                       // 0xC9BC50
    execCtx.params.heap   = heap;                                          // 0xC9BC54

    pipeline->embeddedPacketState = rxPKST_PACKETLESS;
    if (pipeline->nodes[0].nodeDef->nodeMethods.nodeBody(&pipeline->nodes[0], &execCtx.params) == FALSE)
        execCtx.exitCode = FALSE;

    if (pipeline->embeddedPacketState > rxPKST_UNUSED) {
        pipeline->embeddedPacketState = rxPKST_INUSE;
        _rxPacketDestroy(pipeline->embeddedPacket);
    }

    execCtx.pipeline        = nullptr;
    execCtx.params.dataParam = nullptr;
    execCtx.params.heap     = nullptr;
    return execCtx.exitCode != FALSE ? pipeline : static_cast<RxPipeline*>(nullptr);
}


RxPacket* RxPacketCreate(RxPipelineNode* node)
{
    RxPipeline* pipeline = StaticRef<RxPipeline*>(0xC9BC40);

    pipeline->embeddedPacketState = rxPKST_INUSE;   // decompile: *(pipeline+0x10) = 2

    RxPacket* packet = pipeline->embeddedPacket;    // decompile: *(pipeline+0x14)
    packet->inputToClusterSlot = node->inputToClusterSlot;
    packet->slotsContinue      = node->slotsContinue;
    packet->slotClusterRefs    = node->slotClusterRefs;
    return packet;   // ghidra shows void; header contract returns RxPacket* - see STATUS.md
}


RxCluster* RxClusterSetStride(RxCluster* cluster, RwInt32 stride)
{
    cluster->stride = (RwUInt16)stride;   // decompile: *(RwUInt16*)(cluster+2)
    return cluster;                       // ghidra shows void; header contract returns RxCluster*
}


RxCluster* RxClusterSetExternalData(RxCluster* cluster, void* data, RwInt32 stride, RwInt32 numElements)
{
    void* oldData = cluster->data;
    if (oldData != NULL && (cluster->flags & rxCLFLAGS_EXTERNAL) == 0 && oldData != data) {
        RxHeapFree(RxHeapGetGlobalHeap(), oldData);
        cluster->data = NULL;
    }

    cluster->flags       = (RwUInt16)(cluster->flags | rxCLFLAGS_CLUSTERVALID | rxCLFLAGS_EXTERNAL);  // |3
    cluster->data        = data;
    cluster->currentData = data;
    cluster->numAlloced  = (RwUInt32)numElements;
    cluster->numUsed     = (RwUInt32)numElements;
    cluster->stride      = (RwUInt16)stride;
    return cluster;
}


RxCluster* RxClusterSetData(RxCluster* cluster, void* data, RwInt32 stride, RwInt32 numElements)
{
    void* oldData = cluster->data;
    if (oldData != NULL && (cluster->flags & rxCLFLAGS_EXTERNAL) == 0 && oldData != data) {
        RxHeapFree(RxHeapGetGlobalHeap(), oldData);
        cluster->data = NULL;
    }

    cluster->flags       = (RwUInt16)(cluster->flags | rxCLFLAGS_CLUSTERVALID);   // |1
    cluster->data        = data;
    cluster->currentData = data;
    cluster->numAlloced  = (RwUInt32)numElements;
    cluster->numUsed     = (RwUInt32)numElements;
    cluster->stride      = (RwUInt16)stride;
    return cluster;
}


void _rxPacketDestroy(RxPacket *Packet)
{
    Packet->pipeline->embeddedPacketState = rxPKST_UNUSED; /* 1 (decompile: pipeline+0x10 = 1) */

    RxCluster *cluster = Packet->clusters; /* +0x14 */
    RwUInt32 n = (RwUInt32)Packet->numClusters; /* RwUInt16 at +2 */
    do
    {
        if (cluster->clusterRef != NULL) /* +0x14: cluster was locked/used */
        {
            if (cluster->data != NULL && (cluster->flags & 2) == 0)
            {
                /* bit1 of flags: data must not be freed from the pipeline heap */
                RxHeapFree(g_rxGlobalHeap, cluster->data);
            }
            cluster->flags = 0;
            cluster->stride = 0;
            cluster->data = NULL;
            /* currentData (+8) intentionally left untouched, like the binary */
            cluster->numAlloced = 0;
            cluster->numUsed = 0;
            cluster->clusterRef = NULL;
            cluster->attributes = 0;
        }
        cluster++; /* sizeof(RxCluster) == 0x1C */
        n--;
    } while (n != 0);

    Packet->flags = 0; /* decompile zeroes the RwUInt16 at +0 only */
}


RxCluster* RxClusterInitializeData(RxCluster* cluster, RwUInt32 numElements, RwUInt16 stride)
{
    if (cluster->data != NULL && (cluster->flags & rxCLFLAGS_EXTERNAL) == 0)
        RxHeapFree(RxHeapGetGlobalHeap(), cluster->data);

    void* newData = RxHeapAlloc(RxHeapGetGlobalHeap(), (RwUInt32)stride * numElements);

    cluster->flags       = (RwUInt16)(cluster->flags | rxCLFLAGS_CLUSTERVALID);
    cluster->data        = newData;
    cluster->currentData = newData;
    cluster->numAlloced  = numElements;
    cluster->numUsed     = 0;
    cluster->stride      = stride;
    return cluster;
}


RxCluster* RxClusterResizeData(RxCluster* CurrentCluster, RwUInt32 NumElements)
{
    void* newData = RxHeapRealloc(RxHeapGetGlobalHeap(), CurrentCluster->data,
                                  (RwUInt32)CurrentCluster->stride * NumElements, TRUE);

    CurrentCluster->flags       = (RwUInt16)(CurrentCluster->flags | rxCLFLAGS_CLUSTERVALID);
    CurrentCluster->data        = newData;
    CurrentCluster->currentData = newData;
    CurrentCluster->numAlloced  = NumElements;
    if (NumElements < CurrentCluster->numUsed)
        CurrentCluster->numUsed = NumElements;
    return CurrentCluster;
}


RxCluster* RxClusterDestroyData(RxCluster* CurrentCluster)
{
    if ((CurrentCluster->flags & rxCLFLAGS_EXTERNAL) == 0)
        RxHeapFree(RxHeapGetGlobalHeap(), CurrentCluster->data);

    CurrentCluster->data       = NULL;
    CurrentCluster->numAlloced = 0;
    CurrentCluster->numUsed    = 0;
    CurrentCluster->flags      = rxCLFLAGS_NULL;
    return CurrentCluster;   // ghidra shows void; header contract returns RxCluster*
}


RxCluster* RxClusterLockWrite(RxPacket* packet, RwUInt32 clusterIndex, RxPipelineNode* node)
{
    (void)node;   // in signature only (decompile never reads it)

    const RwInt32 slot = (RwInt32)packet->inputToClusterSlot[clusterIndex];
    if (slot == -1)
        return NULL;

    RxCluster* cluster = &packet->clusters[slot];

    if (cluster->clusterRef == NULL) {
        RxPipelineCluster* pipelineCluster = packet->slotClusterRefs[slot];
        cluster->clusterRef = pipelineCluster;
        cluster->attributes = pipelineCluster->creationAttributes;
    }

    cluster->flags = (RwUInt16)(cluster->flags | rxCLFLAGS_MODIFIED);

    if ((cluster->flags & (rxCLFLAGS_EXTERNALMODIFIABLE & rxCLFLAGS_EXTERNAL)) == rxCLFLAGS_EXTERNAL) {
        const RwUInt16 stride        = cluster->stride;
        const RwUInt32 numAlloced    = cluster->numAlloced;
        const RwUInt32 numUsed       = cluster->numUsed;
        void*          externalData  = cluster->data;

        cluster->flags = (RwUInt16)(cluster->flags & ~rxCLFLAGS_EXTERNAL);
        cluster->data  = NULL;

        void* heapData = RxHeapAlloc(RxHeapGetGlobalHeap(), (RwUInt32)stride * numAlloced);

        cluster->flags       = (RwUInt16)(cluster->flags | rxCLFLAGS_CLUSTERVALID);
        cluster->stride      = stride;
        const RwUInt32 copyBytes = (RwUInt32)stride * numUsed;   // vendor: dword-wise copy + byte tail
        cluster->numAlloced  = numAlloced;
        cluster->numUsed     = numUsed;
        cluster->data        = heapData;
        cluster->currentData = heapData;

        RwUInt32 count;
        RwUInt32* src = (RwUInt32*)externalData;
        RwUInt32* dst = (RwUInt32*)heapData;
        for (count = copyBytes >> 2; count != 0; count--)
            *dst++ = *src++;
        RwUInt8* srcB = (RwUInt8*)src;
        RwUInt8* dstB = (RwUInt8*)dst;
        for (count = copyBytes & 3; count != 0; count--)
            *dstB++ = *srcB++;
    }

    cluster->currentData = cluster->data;
    return cluster;
}


void RxClusterUnlock(RxCluster* cluster) {
    // decompile 0x8055C0 (size=1) is a bare `ret` (verified: disasm "008055C0 ret") —
    // in this RW 3.6 GTA build unlocking a write-locked cluster is a no-op.
    (void)cluster;
}


RwUInt32 RxPipelineNodeSendConfigMsg(RxPipelineNode* dest, RwUInt32 msg, RwUInt32 intparam, void* ptrparam) {
    // decompile 0x8055D0: NULL dest -> 0xFFFFFFFF; dispatch nodeDef->nodeMethods.configMsgHandler (+0x1C).
    // Ghidra shows the handler call argless — it is a tail-jump (disasm: mov [esp+4],ecx; jmp eax)
    // which forwards (dest, msg, intparam, ptrparam) unchanged; repo header signature kept.
    if (dest == nullptr)
        return 0xFFFFFFFF;
    RxConfigMsgHandlerFn handler = dest->nodeDef->nodeMethods.configMsgHandler;
    if (handler != nullptr)
        return handler(dest, msg, intparam, ptrparam);
    return 0;
}


RxPipelineNode* RxPipelineNodeForAllConnectedOutputs(RxPipelineNode* node, RxPipeline* pipeline, RxPipelineNodeOutputCallBack callbackfn, void* callbackdata) {
    // decompile 0x8055F0: node must point inside pipeline->nodes[0..numNodes); walk node->outputs,
    // skip indices >= pipeline->numNodes, call callbackfn(node, &nodes[outIdx], callbackdata);
    // callback returning NULL stops traversal (result is the input node), invalid index is skipped.
    if (node == nullptr || pipeline == nullptr || callbackfn == nullptr)
        return nullptr;
    if (node < pipeline->nodes || node >= pipeline->nodes + pipeline->numNodes)
        return nullptr;
    for (RwUInt32 i = 0; i < node->numOutputs; i++) {
        RwUInt32 outIdx = node->outputs[i];
        if (outIdx < pipeline->numNodes) {
            if (callbackfn(node, &pipeline->nodes[outIdx], callbackdata) == nullptr)
                return node;
        }
    }
    return node;
}


RxPipelineCluster* RxPipelineNodeGetPipelineCluster(RxPipelineNode* node, RwUInt32 clustersOfInterestIndex) {
    // decompile 0x805680: index within nodeDef->io.numClustersOfInterest (+0x20); map through
    // node->inputToClusterSlot (+0x18, -1 = no slot assigned); result = node->slotClusterRefs[slot] (+0xC).
    if (node == nullptr)
        return nullptr;
    if (clustersOfInterestIndex < node->nodeDef->io.numClustersOfInterest) {
        RwInt32 slot = (RwInt32)node->inputToClusterSlot[clustersOfInterestIndex];
        if (slot != -1)
            return node->slotClusterRefs[slot];
    }
    return nullptr;
}


RwUInt32 RxPipelineClusterGetCreationAttributes(RxPipelineCluster* cluster) {
    // decompile 0x8056B0: cluster->creationAttributes (+0x4), 0xFFFFFFFF when cluster is NULL.
    return cluster != nullptr ? cluster->creationAttributes : 0xFFFFFFFF;
}


RxPipelineCluster* RxPipelineClusterSetCreationAttributes(RxPipelineCluster* cluster, RwUInt32 creationAttributes) {
    // decompile 0x8056C0: set cluster->creationAttributes (+0x4); NULL in -> NULL out, else cluster.
    if (cluster == nullptr)
        return nullptr;
    cluster->creationAttributes = creationAttributes;
    return cluster;
}


RwUInt32 RxClusterGetAttributes(RxCluster* cluster) {
    // decompile 0x8056E0: cluster->attributes (+0x18), 0xFFFFFFFF when cluster is NULL.
    return cluster != nullptr ? cluster->attributes : 0xFFFFFFFF;
}


RxCluster* RxClusterSetAttributes(RxCluster* cluster, RwUInt32 attributes) {
    // decompile 0x8056F0: set cluster->attributes (+0x18); NULL in -> NULL out, else cluster.
    if (cluster == nullptr)
        return nullptr;
    cluster->attributes = attributes;
    return cluster;
}


RxNodeOutput RxPipelineNodeFindOutputByName(RxPipelineNode* node, const RwChar* outputname) {
    // decompile 0x805D10: scan nodeDef->io.outputs (+0x30, stride 12 = sizeof RxOutputSpec),
    // name compare through RwGlobals->stringFuncs.vecStrcmp (+0x114 == rwstrcmp macro,
    // confirmed as CRT strcmp by integrated _rwStringOpen 0x80A240).
    if (node != nullptr && node->nodeDef != nullptr && outputname != nullptr) {
        RxOutputSpec* out = node->nodeDef->io.outputs;
        for (RwUInt32 i = 0; i < node->nodeDef->io.numOutputs; i++) {
            if (rwstrcmp(out->name, outputname) == 0)
                return node->outputs + i;
            out++;
        }
    }
    return nullptr;
}


RxNodeOutput RxPipelineNodeFindOutputByIndex(RxPipelineNode* node, RwUInt32 outputindex) {
    // decompile 0x805D70: outputindex < node->numOutputs (+0x4) -> &node->outputs[index] (+0x8).
    if (node != nullptr && node->nodeDef != nullptr && outputindex < node->numOutputs)
        return node->outputs + outputindex;
    return nullptr;
}


RxNodeInput RxPipelineNodeFindInput(RxPipelineNode* node) {
    // decompile 0x805DA0: a node provides an input only when its nodeDef (+0x0) is set;
    // otherwise NULL. The node pointer itself doubles as the RxNodeInput handle.
    if (node == nullptr || node->nodeDef == nullptr)
        return nullptr;
    return node;
}


RxNodeDefinition* RxPipelineNodeCloneDefinition(RxPipelineNode* node, RxClusterDefinition* cluster2add) {
    // decompile 0x8059C0: refuse to clone-and-add when cluster2add is already among
    // nodeDef->io.clustersOfInterest (compare RxClusterRef.clusterDef pointers, stride 12);
    // otherwise delegate to the internal editable-copy helper 0x805A20 (outside floor).
    if (node != nullptr) {
        if (cluster2add != nullptr) {
            RwUInt32 numCOI = node->nodeDef->io.numClustersOfInterest;
            if (numCOI != 0) {
                RxClusterRef* clr      = node->nodeDef->io.clustersOfInterest;
                RwUInt32      i        = 0;
                while (clr->clusterDef != cluster2add) {
                    i++;
                    clr++;
                    if (numCOI <= i)
                        return ((RxNodeDefinition*(__cdecl*)(RxPipelineNode*, RxClusterDefinition*))0x805A20)(node, cluster2add);
                }
                return nullptr; // cluster already requested -> failure
            }
        }
        return ((RxNodeDefinition*(__cdecl*)(RxPipelineNode*, RxClusterDefinition*))0x805A20)(node, cluster2add);
    }
    return nullptr;
}


RxPipeline* RxPipelineNodeRequestCluster(RxPipeline* pipeline, RxPipelineNode* node, RxClusterDefinition* clusterDef) {
    // decompile 0x805DB0: pipeline must be locked; clusterDef must not already be among
    // nodeDef->io.clustersOfInterest; make the editable node-def copy + append the cluster via
    // internal helper 0x805A20 (outside floor), then mark the new entry rxREQ=2.
    if (pipeline == nullptr || pipeline->locked == FALSE || node == nullptr ||
        node->nodeDef == nullptr || clusterDef == nullptr)
        return nullptr;

    RwUInt32      numCOI = node->nodeDef->io.numClustersOfInterest;
    RxClusterRef* clr    = node->nodeDef->io.clustersOfInterest;
    for (RwUInt32 i = 0; i < numCOI; i++) {
        if (clr[i].clusterDef == clusterDef)
            return nullptr; // already requested
    }

    RxNodeDefinition* newDef = ((RxNodeDefinition*(__cdecl*)(RxPipelineNode*, RxClusterDefinition*))0x805A20)(node, clusterDef);
    if (newDef != nullptr) {
        // binary writes literal 2 (== rxCLREQ_OPTIONAL) at inputRequirements[numCOI_new - 1];
        // SDK prose suggests rxCLREQ_REQUIRED (=1) here — decompile wins, keep literal 2.
        newDef->io.inputRequirements[newDef->io.numClustersOfInterest - 1] = (RxClusterValidityReq)2;
        return pipeline;
    }
    return nullptr;
}


RxPipeline* RxPipelineNodeReplaceCluster(RxPipeline* pipeline, RxPipelineNode* node, RxClusterDefinition* oldClusterDef, RxClusterDefinition* newClusterDef) {
    // decompile 0x805E20: pipeline locked, both defs non-NULL; find oldClusterDef among
    // nodeDef->io.clustersOfInterest (stride 12); make editable copy WITHOUT adding a cluster
    // (internal helper 0x805A20, outside floor) and swap the found RxClusterRef.clusterDef.
    if (pipeline == nullptr || pipeline->locked == FALSE || node == nullptr ||
        node->nodeDef == nullptr || oldClusterDef == nullptr || newClusterDef == nullptr)
        return nullptr;

    RwUInt32      numCOI = node->nodeDef->io.numClustersOfInterest;
    RxClusterRef* clr    = node->nodeDef->io.clustersOfInterest;
    RwUInt32      i      = 0;
    if (numCOI != 0) {
        for (; i < numCOI && clr[i].clusterDef != oldClusterDef; i++)
            ;
    }
    if (i != numCOI) { // found
        RxNodeDefinition* newDef = ((RxNodeDefinition*(__cdecl*)(RxPipelineNode*, RxClusterDefinition*))0x805A20)(node, nullptr);
        if (newDef != nullptr) {
            newDef->io.clustersOfInterest[i].clusterDef = newClusterDef;
            return pipeline;
        }
    }
    return nullptr;
}


void* RxPipelineNodeGetInitData(RxPipelineNode* node) {
    // decompile 0x805F30 (size=8): plain read of node->initializationData (+0x20); the binary
    // performs NO NULL check — preserved as-is.
    return node->initializationData;
}


void* RxPipelineNodeCreateInitData(RxPipelineNode* node, RwUInt32 size) {
    // decompile 0x805EA0: free any previous initializationData (+0x20), rwmalloc(size, hint 0x30409)
    // (RwGlobals->memoryFuncs.rwmalloc +0x134 == RwMalloc macro); NULL -> RwError{1, nudge(0x80000013, size)};
    // success also stores initializationDataSize (+0x24).
    if (node->initializationData != nullptr) {
        RwFree(node->initializationData);
        node->initializationData      = nullptr;
        node->initializationDataSize = 0;
    }
    node->initializationData = RwMalloc(size, 0x30409);
    if (node->initializationData == nullptr) {
        RwError rwError;
        rwError.pluginID  = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, RwInt32))0x8088D0)(0x80000013, size);
        RwErrorSet(&rwError);
        return nullptr;
    }
    node->initializationDataSize = size;
    return node->initializationData;
}


RxPipeline* RxPipelineClone(RxPipeline* pipeline) {
    // decompile 0x806AC0: clone an UNLOCKED pipeline: create, copy pluginId/pluginData
    // (+0x2C/+0x30); node-less pipelines return immediately. Otherwise lock the clone, copy the
    // node array from the source (0x8065E0, outside floor) and unlock it; any failure destroys
    // the clone.
    if (pipeline != nullptr && pipeline->locked == FALSE) {
        RxPipeline* clone = RxPipelineCreate();
        if (clone != nullptr) {
            clone->pluginId   = pipeline->pluginId;
            clone->pluginData = pipeline->pluginData;
            if (pipeline->numNodes == 0)
                return clone;
            if (RxPipelineLock(clone) != nullptr &&
                ((RwBool(__cdecl*)(RxPipeline*, RxPipeline*))0x8065E0)(clone, pipeline) != FALSE &&
                RxLockedPipeUnlock(clone) != nullptr)
                return clone;
            _rxPipelineDestroy(clone);
        }
    }
    return nullptr;
}


RxPipelineNode* RxPipelineFindNodeByName(RxPipeline* pipeline, const RwChar* name, RxPipelineNode* start, RwInt32* nodeIndex) {
    // decompile 0x806B30: optionally skip up to `start` (search begins AFTER it), then scan nodes
    // comparing nodeDef->name via RwGlobals->stringFuncs.vecStrcmp (+0x114 == rwstrcmp macro);
    // *nodeIndex receives the found node's index or -1.
    if (pipeline != nullptr && name != nullptr && pipeline->numNodes != 0) {
        RxPipelineNode* node      = pipeline->nodes;
        RwInt32         remaining = (RwInt32)pipeline->numNodes;
        if (start != nullptr) {
            for (; node != start && remaining > 0; remaining--)
                node++;
            node++;
            remaining--;
        }
        for (; remaining > 0; remaining--) {
            if (node->nodeDef != nullptr && rwstrcmp(node->nodeDef->name, name) == 0) {
                if (nodeIndex != nullptr)
                    *nodeIndex = (RwInt32)pipeline->numNodes - remaining;
                return node;
            }
            node++;
        }
    }
    if (nodeIndex != nullptr)
        *nodeIndex = -1;
    return nullptr;
}


RxPipelineNode* RxPipelineFindNodeByIndex(RxPipeline* pipeline, RwUInt32 nodeindex) {
    // decompile 0x806BC0: nodeindex < pipeline->numNodes (+0x4) -> nodes + index*0x28.
    if (pipeline != nullptr && nodeindex < pipeline->numNodes)
        return &pipeline->nodes[nodeindex];
    return nullptr;
}


RxLockedPipe* RxPipelineLock(RxPipeline* pipeline) {
    // decompile 0x806990: already-locked pipes return unchanged. First lock allocates the
    // superBlock (blockSize*0xB4 bytes, rwmalloc hint 0x1030409); relock resizes it when the
    // block count grew (0x8064D0) and re-copies the node array (0x8065E0). Then locked = TRUE and
    // each node's pipelineNodeTerm runs, nodeDef->InputPipesCnt is decremented (nodeTerm on last
    // user), and slotClusterRefs is cleared.
    if (pipeline->locked == FALSE) {
        RwInt32  globalsOffset = StaticRef<RwInt32>(0xC9BC60);
        RwUInt32 need = (RwUInt32)(*(RwInt32*)((RwUInt8*)RwEngineInstance + globalsOffset + 0x38) * 0xB4);
        if (pipeline->nodes == nullptr) {
            pipeline->superBlock = RwMalloc(need, 0x1030409);
            if (pipeline->superBlock == nullptr) {
                RwError rwError;
                rwError.pluginID  = 1;
                rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, RwInt32))0x8088D0)(0x80000013, need);
                RwErrorSet(&rwError);
                return nullptr;
            }
            pipeline->superBlockSize = need;
            pipeline->nodes          = (RxPipelineNode*)pipeline->superBlock;
        } else {
            if (pipeline->superBlockSize < need &&
                ((RwBool(__cdecl*)(RxPipeline*, RwUInt32))0x8064D0)(pipeline, need) == FALSE)
                return nullptr;
            if (((RwBool(__cdecl*)(RxPipeline*, RxPipeline*))0x8065E0)(pipeline, pipeline) == FALSE)
                return nullptr;
        }
        pipeline->locked = TRUE;

        if (pipeline->nodes != nullptr && pipeline->numNodes != 0) {
            RxPipelineNode* node = pipeline->nodes;
            for (RwUInt32 i = 0; i < pipeline->numNodes; i++, node++) {
                RxNodeDefinition* nodeDef = node->nodeDef;
                if (nodeDef->nodeMethods.pipelineNodeTerm != nullptr)
                    nodeDef->nodeMethods.pipelineNodeTerm(node);
                nodeDef->InputPipesCnt--;
                if (nodeDef->InputPipesCnt == 0 && nodeDef->nodeMethods.nodeTerm != nullptr)
                    nodeDef->nodeMethods.nodeTerm(nodeDef);
                node->slotClusterRefs = nullptr;
            }
        }
    }
    return (RxLockedPipe*)pipeline;
}


RxPipeline* RxLockedPipeUnlock(RxLockedPipe* pipeline) {
    // decompile 0x805F40: full unlock pass — count distinct cluster definitions (sorted by POINTER
    // value), size/resize the superBlock, reset+propagate top-sort input counts, validate entry
    // point, run topsort helper 0x8067B0, compact per-node topSortData blocks to the superBlock
    // tail, rebuild outputs arrays, build the embedded packet (0x80F0D0, 0 == success), shrink the
    // superBlock, run nodeInit/pipelineNodeInit (reverse order) then pipelineNodeConfig (reverse
    // order) with full unwind on failure.
    if (pipeline == nullptr) {
        RwError rwError;
        rwError.pluginID  = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000016);
        RwErrorSet(&rwError);
        return nullptr;
    }

    RxPipeline* ppl = (RxPipeline*)pipeline;
    if (ppl->locked == FALSE) {
        RwError rwError;
        rwError.pluginID  = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x34);
        RwErrorSet(&rwError);
        return nullptr; // binary falls through to the common `return 0`
    }

    const RwUInt32 numNodes = ppl->numNodes;
    if (numNodes == 0) {
        ppl->locked = FALSE;
        return ppl; // LAB_008063b2
    }

    const RwUInt32 entryPoint = ppl->entryPoint;
    if (entryPoint >= numNodes || ppl->nodes[entryPoint].nodeDef == nullptr) {
        RwError rwError;
        rwError.pluginID  = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x24);
        RwErrorSet(&rwError);
        return nullptr;
    }

    // ---- Phase 1: number of DISTINCT cluster definitions in the pipeline.
    // The binary sorts them by POINTER value (pointer-order scan, not by name).
    RwUInt32 numDistinctClusterDefs = 0;
    {
        RxClusterDefinition* lastMin = nullptr;
        for (;;) {
            RxClusterDefinition* minDef = (RxClusterDefinition*)0xFFFFFFFF;
            for (RwUInt32 n = 0; n < numNodes; n++) {
                RxNodeDefinition* nodeDef = ppl->nodes[n].nodeDef;
                for (RwUInt32 c = 0; c < nodeDef->io.numClustersOfInterest; c++) {
                    RxClusterDefinition* cd = nodeDef->io.clustersOfInterest[c].clusterDef;
                    if (cd > lastMin && cd < minDef)
                        minDef = cd;
                }
            }
            if (minDef == (RxClusterDefinition*)0xFFFFFFFF)
                break;
            numDistinctClusterDefs++;
            lastMin = minDef;
        }
    }

    // ---- Phase 2: superBlock size computation (all terms literal from the decompile).
    RwInt32  globalsOffset = StaticRef<RwInt32>(0xC9BC60); // pipeline-module globalsOffset
    RwInt32  blockSize     = *(RwInt32*)((RwUInt8*)RwEngineInstance + globalsOffset + 0x38);
    RwUInt32 wantSize      = (RwUInt32)blockSize * 0xB4;
    RwUInt32 nodesBytes    = numNodes * 0x28;
    nodesBytes += ppl->nodes[0].numOutputs * numNodes * 4; // entry-node "width" times node count

    RwInt32 auxBytes = ((RwInt32)numNodes + 5) * (RwInt32)numDistinctClusterDefs
                     + ((RwInt32)numDistinctClusterDefs + 1) * (RwInt32)numNodes;
    auxBytes *= 4;
    for (RwUInt32 n = 0; n < numNodes; n++) {
        RxNodeDefinition* nodeDef = ppl->nodes[n].nodeDef;
        if (nodeDef->pipelineNodePrivateDataSize != 0)
            auxBytes += (RwInt32)nodeDef->pipelineNodePrivateDataSize;
        auxBytes += (RwInt32)nodeDef->io.numClustersOfInterest * 4;
    }
    RwUInt32 needed = (RwUInt32)auxBytes
                    + numDistinctClusterDefs * 0x1C + 0x14
                    + (RwUInt32)blockSize * 0xC
                    + (numDistinctClusterDefs * 0x34 + 0x14) * numNodes
                    + nodesBytes;
    if (wantSize < needed)
        wantSize = needed;

    if (wantSize > ppl->superBlockSize &&
        ((RwBool(__cdecl*)(RxPipeline*, RwUInt32))0x8064D0)(ppl, wantSize) == FALSE)
        return nullptr;

    StaticRef<void*>(0xC9A6B4) = (RwUInt8*)ppl->superBlock + wantSize; // superBlock tail cursor
    StaticRef<void*>(0xC9A6B8) = nullptr;

    // ---- reset per-node top-sort counters (only nodes with a nodeDef)
    for (RwUInt32 n = 0; n < numNodes; n++) {
        if (ppl->nodes[n].nodeDef != nullptr) {
            ppl->nodes[n].topSortData->numInsVisited = 0;
            ppl->nodes[n].topSortData->numIns        = 0;
        }
    }
    // ---- propagate input counts along outputs (output index 0xFFFFFFFF = unconnected)
    for (RwUInt32 n = 0; n < numNodes; n++) {
        RxPipelineNode* node = &ppl->nodes[n];
        if (node->nodeDef != nullptr && node->numOutputs != 0) {
            for (RwUInt32 o = 0; o < node->numOutputs; o++) {
                if (node->outputs[o] != 0xFFFFFFFF)
                    ppl->nodes[node->outputs[o]].topSortData->numIns++;
            }
        }
    }
    // ---- entry point must have no inputs; every other node must have >= 1 input
    if (ppl->nodes[entryPoint].topSortData->numIns != 0) {
        RwError rwError;
        rwError.pluginID  = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x24);
        RwErrorSet(&rwError);
        return nullptr;
    }
    for (RwUInt32 n = 0; n < numNodes; n++) {
        if (n != entryPoint && ppl->nodes[n].topSortData->numIns == 0) {
            RwError rwError;
            rwError.pluginID  = 1;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x22);
            RwErrorSet(&rwError);
            return nullptr;
        }
    }

    // ---- entry-node relocation (internal helper 0x8067B0, outside floor).
    // Decompile of 0x8067B0: param_1 = { RxPipeline* pipeline, RwUInt32 oldNodeIndex } PAIR,
    // param_2 = target index. It swaps nodes[old] <-> nodes[target] (0x28-byte nodes with a
    // 8-dword rotate) and their topSortData (3 dwords) when old != target.
    // Unlock relocates the entry node to slot 0 (ppl->entryPoint = 0 afterwards).
    void* rwTopSortCell[2] = { ppl, (void*)(uintptr_t)entryPoint };
    ((void(__cdecl*)(void**, RwUInt32))0x8067B0)(rwTopSortCell, 0);
    // ---- verify: every node fully visited
    for (RwUInt32 n = 0; n < numNodes; n++) {
        if (ppl->nodes[n].topSortData->numIns != ppl->nodes[n].topSortData->numInsVisited) {
            RwError rwError;
            rwError.pluginID  = 1;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x1C);
            RwErrorSet(&rwError);
            return nullptr;
        }
    }

    ppl->entryPoint = 0;

    // ---- compact the per-node RxPipelineNodeTopSortData blocks (3 dwords each) to the very tail
    // of the superBlock, in reverse node order; source blocks were laid out by 0x8067B0 starting
    // at nodes + blockSize*(0x28 + 0x80) and ending at + numNodes*12 (int*-arithmetic preserved).
    {
        RwInt32   blockSize2 = *(RwInt32*)((RwUInt8*)RwEngineInstance + StaticRef<RwInt32>(0xC9BC60) + 0x38);
        RwUInt8*  nodesBase  = (RwUInt8*)ppl->nodes;
        RwUInt32 numLeft     = numNodes;
        StaticRef<void*>(0xC9A6B4) =
            nodesBase + blockSize2 * 0x28 + blockSize2 * 0x80 + numLeft * 12 - 12;
        RwUInt32* dst = (RwUInt32*)((RwUInt8*)ppl->superBlock + wantSize - 0xC);
        if (numLeft > 0) {
            RwUInt32  nodeOff = (numLeft - 1) * 0x28;
            RwUInt32* src     = (RwUInt32*)StaticRef<void*>(0xC9A6B4);
            do {
                StaticRef<void*>(0xC9A6B4) = src - 3;
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                ppl->nodes[nodeOff / 0x28].topSortData = (RxPipelineNodeTopSortData*)dst;
                dst -= 3;
                numLeft--;
                nodeOff -= 0x28;
                src = (RwUInt32*)StaticRef<void*>(0xC9A6B4);
            } while (numLeft != 0);
        }

        // ---- rebuild the compact outputs arrays right behind the nodes array
        // (dest = nodes + numNodes*0x28; source = the helper's 32-dword-per-node aux area)
        RwUInt32* outDst     = (RwUInt32*)(nodesBase + numNodes * 0x28);
        RwUInt32* srcCursor  = (RwUInt32*)(nodesBase + blockSize2 * 0x28);
        RwUInt32  accumOut   = 0;
        for (RwUInt32 n = 0; n < numNodes; n++) {
            RxPipelineNode* node   = &ppl->nodes[n];
            RwUInt32        numOut = node->numOutputs;
            if (numOut == 0) {
                node->outputs = nullptr;
            } else {
                for (RwUInt32 k = 0; k < numOut; k++)
                    outDst[k] = srcCursor[k];
                node->outputs = outDst;
            }
            srcCursor += 0x20; // always advances by 32 dwords (RXNODEMAXOUTPUTS)
            accumOut  += numOut;
            outDst    += numOut;
        }
        StaticRef<void*>(0xC9A6B8) = srcCursor + accumOut;
    }

    // ---- embedded packet construction; 0 == success
    if (((int(__cdecl*)(RxPipeline*))0x80F0D0)(ppl) == 0) {
        // shrink superBlock to the actually used extent
        if (((RwBool(__cdecl*)(RxPipeline*, RwUInt32))0x8064D0)(
                ppl, (RwUInt32)((RwUInt8*)StaticRef<void*>(0xC9A6B8) - (RwUInt8*)ppl->superBlock)) == FALSE) {
            ((RwBool(__cdecl*)(RxPipeline*, RxPipeline*))0x8065E0)(ppl, ppl); // LAB_0080642f unlock-failure cleanup
            return nullptr;
        }

        // ---- nodeInit/pipelineNodeInit, reverse node order, unwind on failure
        for (RwInt32 idx = (RwInt32)numNodes - 1; idx >= 0; idx--) {
            RxPipelineNode*   node    = &ppl->nodes[idx];
            RxNodeDefinition* nodeDef = node->nodeDef;
            RwInt32           oldCnt  = nodeDef->InputPipesCnt;
            nodeDef->InputPipesCnt    = oldCnt + 1;
            if (oldCnt == 0 && nodeDef->nodeMethods.nodeInit != nullptr) {
                if (nodeDef->nodeMethods.nodeInit(nodeDef) == FALSE) {
                    for (RwUInt32 u = idx + 1; u < numNodes; u++) { // unwind already-initialised nodes
                        RxPipelineNode*   un   = &ppl->nodes[u];
                        RxNodeDefinition* udef = un->nodeDef;
                        if (udef->nodeMethods.pipelineNodeTerm != nullptr)
                            udef->nodeMethods.pipelineNodeTerm(un);
                        udef->InputPipesCnt--;
                        if (udef->InputPipesCnt == 0 && udef->nodeMethods.nodeTerm != nullptr)
                            udef->nodeMethods.nodeTerm(udef);
                    }
                    ((RwBool(__cdecl*)(RxPipeline*, RxPipeline*))0x8065E0)(ppl, ppl);
                    return nullptr;
                }
            }
            if (nodeDef->nodeMethods.pipelineNodeInit != nullptr &&
                nodeDef->nodeMethods.pipelineNodeInit(node) == FALSE) {
                nodeDef->InputPipesCnt--;
                if (nodeDef->InputPipesCnt == 0 && nodeDef->nodeMethods.nodeTerm != nullptr)
                    nodeDef->nodeMethods.nodeTerm(nodeDef);
                for (RwUInt32 u = idx + 1; u < numNodes; u++) {
                    RxPipelineNode*   un   = &ppl->nodes[u];
                    RxNodeDefinition* udef = un->nodeDef;
                    if (udef->nodeMethods.pipelineNodeTerm != nullptr)
                        udef->nodeMethods.pipelineNodeTerm(un);
                    udef->InputPipesCnt--;
                    if (udef->InputPipesCnt == 0 && udef->nodeMethods.nodeTerm != nullptr)
                        udef->nodeMethods.nodeTerm(udef);
                }
                ((RwBool(__cdecl*)(RxPipeline*, RxPipeline*))0x8065E0)(ppl, ppl);
                return nullptr;
            }
        }

        // ---- pipelineNodeConfig, reverse node order; failure unwinds ALL nodes
        for (RwInt32 idx = (RwInt32)numNodes - 1; idx >= 0; idx--) {
            RxPipelineNode*   node    = &ppl->nodes[idx];
            RxNodeDefinition* nodeDef = node->nodeDef;
            if (nodeDef->nodeMethods.pipelineNodeConfig != nullptr &&
                nodeDef->nodeMethods.pipelineNodeConfig(node, ppl) == FALSE) {
                for (RwUInt32 u = 0; u < numNodes; u++) {
                    RxPipelineNode*   un   = &ppl->nodes[u];
                    RxNodeDefinition* udef = un->nodeDef;
                    if (udef->nodeMethods.pipelineNodeTerm != nullptr)
                        udef->nodeMethods.pipelineNodeTerm(un);
                    udef->InputPipesCnt--;
                    if (udef->InputPipesCnt == 0 && udef->nodeMethods.nodeTerm != nullptr)
                        udef->nodeMethods.nodeTerm(udef);
                }
                ((RwBool(__cdecl*)(RxPipeline*, RxPipeline*))0x8065E0)(ppl, ppl);
                return nullptr;
            }
        }

        ppl->locked = FALSE;
        return ppl; // LAB_008063b2
    }
    return nullptr; // 0x80F0D0 nonzero == failure
}


RxPipeline* RxLockedPipeReplaceNode(RxLockedPipe* pipeline, RxPipelineNode* node, RxNodeDefinition* nodeDef) {
    // decompile 0x806F20: pipeline must be locked. The internal helper 0x806DE0 (outside floor)
    // appends the replacement node into the slots AT nodes[numNodes] (one past the live array).
    // Its outputs are seeded from the replaced node (min of both output counts), the replaced
    // node's surplus output slots are invalidated (0xFFFFFFFF), every reference to the replaced
    // node's index is remapped to the new index, and the replaced node is destroyed (0x8050C0).
    if (pipeline == nullptr || pipeline->locked == FALSE || node == nullptr || node->nodeDef == nullptr)
        return nullptr;

    RxPipelineNode* end = pipeline->nodes + pipeline->numNodes;
    if (((int(__cdecl*)(RxPipeline*, RxPipelineNode*, RxNodeDefinition*))0x806DE0)(pipeline, end, nodeDef) == 0)
        return nullptr;

    RwUInt32 minOutputs = node->numOutputs;
    if (end->numOutputs < minOutputs)
        minOutputs = end->numOutputs;

    for (RwUInt32 i = 0; i < minOutputs; i++) // copy common outputs to the appended node
        end->outputs[i] = node->outputs[i];

    if (minOutputs < end->numOutputs) {
        // invalidate the replaced node's surplus output slots (bounded by the appended node's
        // count — exact binary behaviour; the trailing byte-fill loop in the decompile is a dead
        // inlined-memset tail and never executes)
        for (RwUInt32 i = minOutputs; i < end->numOutputs; i++)
            node->outputs[i] = 0xFFFFFFFF;
    }

    // remap outputs[] entries pointing at the replaced node to the appended node's index
    RwUInt32 oldIdx = (RwUInt32)(node - pipeline->nodes);
    RwUInt32 newIdx = (RwUInt32)(end - pipeline->nodes);
    for (RwUInt32 i = 0; i < pipeline->numNodes; i++) {
        RxPipelineNode* n = &pipeline->nodes[i];
        for (RwUInt32 j = 0; j < n->numOutputs; j++) {
            if (n->outputs[j] == oldIdx)
                n->outputs[j] = newIdx;
        }
    }

    ((void(__cdecl*)(RxPipelineNode*, RxPipeline*))0x8050C0)(node, pipeline); // destroy replaced node
    return pipeline;
}


RxPipeline* RxLockedPipeDeleteNode(RxLockedPipe* pipeline, RxPipelineNode* node) {
    // decompile 0x807040: locked pipeline + valid node (nodeDef set) -> destroy the node via the
    // internal node destructor 0x8050C0 (outside floor) and return the pipeline.
    if (pipeline == nullptr || pipeline->locked == FALSE || node == nullptr || node->nodeDef == nullptr)
        return nullptr;
    ((void(__cdecl*)(RxPipelineNode*, RxPipeline*))0x8050C0)(node, pipeline);
    return pipeline;
}


static_assert(sizeof(RxPipelineNode) == 0x28, "RxPipelineNode must match binary stride 0x28");
static_assert(offsetof(RxPipeline, locked)                == 0x00, "RxPipeline layout");
static_assert(offsetof(RxPipeline, numNodes)              == 0x04, "RxPipeline layout");
static_assert(offsetof(RxPipeline, nodes)                 == 0x08, "RxPipeline layout");
static_assert(offsetof(RxPipeline, packetNumClusterSlots) == 0x0C, "RxPipeline layout");
static_assert(offsetof(RxPipeline, entryPoint)            == 0x28, "RxPipeline layout");

RxPipeline* RxLockedPipeSetEntryPoint(RxLockedPipe* pipeline, RxNodeInput in) {
    if (pipeline != nullptr && pipeline->locked != FALSE && in != nullptr &&
        *reinterpret_cast<RwUInt32*>(in) != 0) {
        RwUInt32 index = static_cast<RwUInt32>(
            (reinterpret_cast<RwUInt8*>(in) - reinterpret_cast<RwUInt8*>(pipeline->nodes)) / sizeof(RxPipelineNode));
        if (reinterpret_cast<RxPipelineNode*>(reinterpret_cast<RwUInt8*>(pipeline->nodes) + index * sizeof(RxPipelineNode)) == in &&
            index < pipeline->numNodes && index != 0xFFFFFFFF) {
            pipeline->entryPoint = index;
            return pipeline;
        }
    }
    return nullptr;
}


RxLockedPipe* RxLockedPipeAddFragment(RxLockedPipe* pipeline, RwUInt32* firstIndex, RxNodeDefinition* nodeDef0, RxNodeDefinition* __va_end__) {
    return plugin::CallAndReturn<RxPipeline*, 0x806BE0, RxLockedPipe*, RwUInt32*, RxNodeDefinition*, RxNodeDefinition*>(pipeline, firstIndex, nodeDef0, __va_end__);
}

RxPipelineNode* RxLockedPipeGetEntryPoint(RxLockedPipe* pipeline) {
    if (pipeline != nullptr && pipeline->locked != FALSE &&
        pipeline->entryPoint != 0xFFFFFFFF && pipeline->entryPoint < pipeline->numNodes) {
        return reinterpret_cast<RxPipelineNode*>(reinterpret_cast<RwUInt8*>(pipeline->nodes) +
                                                 pipeline->entryPoint * sizeof(RxPipelineNode));
    }
    return nullptr;
}


RxPipeline* RxLockedPipeAddPath(RxLockedPipe* pipeline, RxNodeOutput out, RxNodeInput in) {
    if (pipeline != nullptr && pipeline->locked != FALSE && out != nullptr &&
        *out == 0xFFFFFFFF && in != nullptr && *reinterpret_cast<RwUInt32*>(in) != 0) {
        RwUInt32 index = static_cast<RwUInt32>(
            (reinterpret_cast<RwUInt8*>(in) - reinterpret_cast<RwUInt8*>(pipeline->nodes)) / sizeof(RxPipelineNode));
        if (reinterpret_cast<RxPipelineNode*>(reinterpret_cast<RwUInt8*>(pipeline->nodes) + index * sizeof(RxPipelineNode)) == in &&
            index < pipeline->numNodes && index != 0xFFFFFFFF) {
            *out = index;
            return pipeline;
        }
    }
    return nullptr;
}


RxPipeline* RxLockedPipeDeletePath(RxLockedPipe* pipeline, RxNodeOutput out, RxNodeInput in) {
    if (pipeline != nullptr && pipeline->locked != FALSE && out != nullptr && in != nullptr &&
        pipeline->nodes + *out == in) {
        *out = 0xFFFFFFFF;
        return pipeline;
    }
    return nullptr;
}


RxPipeline* RxPipelineInsertDebugNode(RxPipeline* pipeline, RxPipelineNode* before, RxPipelineNode* after, RxNodeDefinition* debugNode) {
    if (pipeline == nullptr)
        return nullptr;
    if (before == nullptr && after == nullptr)
        return nullptr;
    if (debugNode == nullptr)
        return nullptr;

    RxPipelineNode* nodes = pipeline->nodes;
    RwUInt32 beforeIdx = 0xFFFFFFFF;
    RwUInt32 afterIdx  = 0xFFFFFFFF;
    RwUInt32 firstIndex = 0xFFFFFFFF;   // out-param of RxLockedPipeAddFragment
    RwUInt32 outSlot    = 0xFFFFFFFF;   // `before` output slot pointing at `after`

    if (before == nullptr) {
        if (after != nodes)          // with no `before`, `after` must be the entry node
            return nullptr;
    } else {
        RwUInt32 idx = static_cast<RwUInt32>(
            (reinterpret_cast<RwUInt8*>(before) - reinterpret_cast<RwUInt8*>(nodes)) / sizeof(RxPipelineNode));
        if (reinterpret_cast<RxPipelineNode*>(reinterpret_cast<RwUInt8*>(nodes) + idx * sizeof(RxPipelineNode)) != before ||
            pipeline->numNodes <= idx || idx == 0xFFFFFFFF)
            return nullptr;
        beforeIdx = idx;
    }
    if (after == nullptr) {
        if (before->numOutputs != 0) // with no `after`, `before` must have no outputs
            return nullptr;
    } else {
        RwUInt32 idx = static_cast<RwUInt32>(
            (reinterpret_cast<RwUInt8*>(after) - reinterpret_cast<RwUInt8*>(nodes)) / sizeof(RxPipelineNode));
        if (reinterpret_cast<RxPipelineNode*>(reinterpret_cast<RwUInt8*>(nodes) + idx * sizeof(RxPipelineNode)) != after ||
            pipeline->numNodes <= idx || idx == 0xFFFFFFFF)
            return nullptr;
        afterIdx = idx;
    }

    if (before != nullptr && after != nullptr) {
        // find which output slot of `before` leads to `after`
        if (before->numOutputs == 0)
            return nullptr;
        for (RwUInt32 i = 0; i < before->numOutputs; i++) {
            if (before->outputs[i] == afterIdx)
                outSlot = i;
        }
        if (outSlot == 0xFFFFFFFF)
            return nullptr;
    }

    // collect unique cluster definitions required by `before` and `after`
    // (decompile: rwmalloc(packetNumClusterSlots * 8, 0x10409); the *8 buys room for
    //  up to 2*slots unique entries stored as RwUInt32)
    RwUInt32 numClusterSlots = pipeline->packetNumClusterSlots;
    RwUInt32* clusterDefs = static_cast<RwUInt32*>(RwGetMemoryFunctionsGta()->rwmalloc(numClusterSlots << 3, 0x10409));
    if (clusterDefs == nullptr) {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, numClusterSlots << 3);
        RwErrorSet(&rwError);
        return nullptr;
    }

    RwUInt32 numClusterDefs = 0;
    if (before != nullptr && numClusterSlots != 0) {
        for (RwUInt32 slot = 0; slot < numClusterSlots; slot++) {
            RxPipelineCluster* pipeCluster = before->slotClusterRefs[slot];
            if (pipeCluster != nullptr) {
                RxClusterDefinition* def = pipeCluster->clusterRef;
                RwBool present = FALSE;
                for (RwUInt32 j = 0; j < numClusterDefs; j++) {
                    if (clusterDefs[j] == reinterpret_cast<RwUInt32>(def))
                        present = TRUE;
                }
                if (!present)
                    clusterDefs[numClusterDefs++] = reinterpret_cast<RwUInt32>(def);
            }
        }
    }
    if (after != nullptr && numClusterSlots != 0) {
        for (RwUInt32 slot = 0; slot < numClusterSlots; slot++) {
            RxPipelineCluster* pipeCluster = after->slotClusterRefs[slot];
            if (pipeCluster != nullptr) {
                RxClusterDefinition* def = pipeCluster->clusterRef;
                RwBool present = FALSE;
                for (RwUInt32 j = 0; j < numClusterDefs; j++) {
                    if (clusterDefs[j] == reinterpret_cast<RwUInt32>(def))
                        present = TRUE;
                }
                if (!present)
                    clusterDefs[numClusterDefs++] = reinterpret_cast<RwUInt32>(def);
            }
        }
    }

    RxLockedPipe* locked = RxPipelineLock(pipeline);
    if (locked == nullptr) {
        // decompile: bare `return 0` — clusterDefs leaks on this path (reproduced 1:1)
        return nullptr;
    }

    // NOTE decompile order: on the failure of RxPipelineLock the vendor leaks `clusterDefs`
    // (the free only happens on the success path below / after unlock). Reproduced 1:1.
    locked = RxLockedPipeAddFragment(locked, &firstIndex, debugNode, static_cast<RxNodeDefinition*>(nullptr));
    RxPipelineNode* newNode = reinterpret_cast<RxPipelineNode*>(
        reinterpret_cast<RwUInt8*>(locked->nodes) + firstIndex * sizeof(RxPipelineNode));

    for (RwUInt32 i = 0; i < numClusterDefs; i++) {
        RxClusterDefinition* def = reinterpret_cast<RxClusterDefinition*>(clusterDefs[i]);
        if (pipeline->locked != FALSE && newNode != nullptr && newNode->nodeDef != nullptr && def != nullptr) {
            RwBool present = FALSE;
            if (newNode->nodeDef->io.numClustersOfInterest != 0) {
                RxClusterRef* ref = newNode->nodeDef->io.clustersOfInterest;
                for (RwUInt32 k = 0; k < newNode->nodeDef->io.numClustersOfInterest; k++, ref++) {
                    if (ref->clusterDef == def) {
                        present = TRUE;
                        break;
                    }
                }
            }
            if (!present) {
                RxNodeDefinition* clonedDef = ((RxNodeDefinition*(__cdecl*)(RxPipelineNode*, RxClusterDefinition*))0x805A20)(newNode, def);
                if (clonedDef != nullptr) {
                    // mark the freshly appended input requirement as rxCLREQ_OPTIONAL (=2)
                    clonedDef->io.inputRequirements[clonedDef->io.numClustersOfInterest - 1] = rxCLREQ_OPTIONAL;
                }
            }
        }
    }

    if (before != nullptr) {
        RwUInt32* slot = (before->nodeDef != nullptr && before->numOutputs > outSlot)
                             ? &before->outputs[outSlot]
                             : static_cast<RwUInt32*>(nullptr);
        // disconnect before -> after
        if (locked->locked != FALSE && slot != nullptr && after != nullptr &&
            reinterpret_cast<RxPipelineNode*>(reinterpret_cast<RwUInt8*>(locked->nodes) + *slot * sizeof(RxPipelineNode)) == after) {
            *slot = 0xFFFFFFFF;
        }
        // point the freed slot at the new node
        slot = (before->nodeDef != nullptr && before->numOutputs > outSlot)
                   ? &before->outputs[outSlot]
                   : static_cast<RwUInt32*>(nullptr);
        if (locked->locked != FALSE && slot != nullptr && *slot == 0xFFFFFFFF &&
            newNode != nullptr && newNode->nodeDef != nullptr) {
            RwUInt32 idx = static_cast<RwUInt32>(
                (reinterpret_cast<RwUInt8*>(newNode) - reinterpret_cast<RwUInt8*>(locked->nodes)) / sizeof(RxPipelineNode));
            if (reinterpret_cast<RxPipelineNode*>(reinterpret_cast<RwUInt8*>(locked->nodes) + idx * sizeof(RxPipelineNode)) == newNode &&
                idx < locked->numNodes && idx != 0xFFFFFFFF) {
                *slot = idx;
            }
        }
    }
    if (after != nullptr) {
        // connect newNode's (only) output to `after`
        RwUInt32* slot = (newNode == nullptr || newNode->nodeDef == nullptr || newNode->numOutputs == 0)
                             ? static_cast<RwUInt32*>(nullptr)
                             : &newNode->outputs[0];
        if (locked->locked != FALSE && slot != nullptr && *slot == 0xFFFFFFFF && after->nodeDef != nullptr) {
            RwUInt32 idx = static_cast<RwUInt32>(
                (reinterpret_cast<RwUInt8*>(after) - reinterpret_cast<RwUInt8*>(locked->nodes)) / sizeof(RxPipelineNode));
            if (reinterpret_cast<RxPipelineNode*>(reinterpret_cast<RwUInt8*>(locked->nodes) + idx * sizeof(RxPipelineNode)) == after &&
                idx < locked->numNodes && idx != 0xFFFFFFFF) {
                *slot = idx;
            }
        }
    }

    RxPipeline* result = RxLockedPipeUnlock(locked);
    if (result == nullptr) {
        _rxPipelineDestroy(locked);
    }
    RwGetMemoryFunctionsGta()->rwfree(clusterDefs);
    return result;
}


void RwD3D9VertexBufferManagerChangeDefaultSize(RwUInt32 defaultSize) {
    r4VbDefaultSize = defaultSize;
}


RwBool RwD3D9CreateVertexBuffer(RwUInt32 stride, RwUInt32 size, void** vertexBuffer, RwUInt32* offset) {
    RwD3D9VBStrideNodeR4* strideNode;
    for (strideNode = r4VbStrideNodeList; strideNode != nullptr; strideNode = strideNode->next) {
        if (strideNode->stride == stride) {
            break;
        }
    }
    if (strideNode == nullptr) {
        strideNode = (RwD3D9VBStrideNodeR4*)RwGetGlobalsRwcore4()->memoryAlloc(r4VbStrideNodeFl, 0x30411);
        strideNode->stride          = stride;
        strideNode->bufferList      = nullptr;
        strideNode->freedBufferList = nullptr;
        strideNode->next            = r4VbStrideNodeList;
        r4VbStrideNodeList          = strideNode;
    }

    RwD3D9VBBufferNodeR4* buf;
    for (buf = strideNode->bufferList; buf != nullptr; buf = buf->prev) {
        if (size <= buf->remaining) {
            break;
        }
    }
    if (buf == nullptr) {
        buf = (RwD3D9VBBufferNodeR4*)RwGetGlobalsRwcore4()->memoryAlloc(r4VbBufferNodeFl, 0x30411);
        buf->freeOffset = 0;
        RwUInt32 bufSize = ((r4VbDefaultSize + stride - 1) / stride) * stride;   // align default size to stride
        buf->remaining = bufSize;
        if (bufSize <= size) {
            buf->remaining = size;                                               // at least the request
        }
        HRESULT hr = _RwD3DDevice->CreateVertexBuffer(buf->remaining,             // Length
                                                      D3DUSAGE_WRITEONLY,         // 0x8
                                                      0,                          // FVF
                                                      D3DPOOL_MANAGED,            // 1
                                                      &buf->vertexBuffer,
                                                      nullptr);
        if (FAILED(hr)) {
            RwGetGlobalsRwcore4()->memoryFree(r4VbBufferNodeFl, buf);
            return FALSE;
        }
        buf->next = nullptr;
        buf->prev = strideNode->bufferList;
        if (strideNode->bufferList != nullptr) {
            strideNode->bufferList->next = buf;
        }
        strideNode->bufferList = buf;
        RwD3D9VBFreedNodeR4* freed = (RwD3D9VBFreedNodeR4*)RwGetGlobalsRwcore4()->memoryAlloc(r4VbFreedNodeFl, 0x30411);
        freed->vertexBuffer = buf->vertexBuffer;
        freed->next         = strideNode->freedBufferList;
        strideNode->freedBufferList = freed;
    }

    *vertexBuffer = buf->vertexBuffer;
    RwUInt32 blockOffset = buf->freeOffset;
    *offset       = blockOffset;
    buf->freeOffset = blockOffset + size;
    buf->remaining  = buf->remaining - size;
    if (buf->remaining == 0) {
        if (buf->next != nullptr) {
            buf->next->prev = buf->prev;
        }
        if (buf->prev != nullptr) {
            buf->prev->next = buf->next;
        }
        if (strideNode->bufferList == buf) {
            strideNode->bufferList = buf->next;
        }
        RwGetGlobalsRwcore4()->memoryFree(r4VbBufferNodeFl, buf);
    }
    return TRUE;
}


void RwD3D9DestroyVertexBuffer(RwUInt32 stride, RwUInt32 size, void* vertexBuffer, RwUInt32 offset) {
    RwD3D9VBStrideNodeR4* strideNode = r4VbStrideNodeList;
    if (strideNode == nullptr) {
        return;
    }
    while (strideNode->stride != stride) {
        strideNode = strideNode->next;
        if (strideNode == nullptr) {
            return;
        }
    }

    RwD3D9VBBufferNodeR4* node = strideNode->bufferList;
    while (true) {
        if (node == nullptr) {
            // No adjacent node with this buffer: push a fresh free-range node at the head.
            node = (RwD3D9VBBufferNodeR4*)RwGetGlobalsRwcore4()->memoryAlloc(r4VbBufferNodeFl, 0x30411);
            node->freeOffset   = offset;
            node->remaining    = size;
            node->vertexBuffer = (IDirect3DVertexBuffer9*)vertexBuffer;
            node->next         = nullptr;
            node->prev         = strideNode->bufferList;
            if (strideNode->bufferList != nullptr) {
                strideNode->bufferList->next = node;
            }
            strideNode->bufferList = node;
            return;
        }
        RwD3D9VBBufferNodeR4* older = node->prev;
        if (node->vertexBuffer == (IDirect3DVertexBuffer9*)vertexBuffer &&
            ((node->freeOffset + node->remaining == offset) || (offset + size == node->freeOffset))) {
            break;
        }
        node = older;
    }

    do {
        if (node->freeOffset + node->remaining == offset) {
            // freed range lies right after this node -> grow it forward
            node->remaining = node->remaining + size;
        } else if (offset + size == node->freeOffset) {
            // freed range lies right before this node -> grow it backward
            node->freeOffset = offset;
            node->remaining  = node->remaining + size;
        }
        offset = node->freeOffset;
        size   = node->remaining;

        // look for another node adjacent to the accumulated [offset, offset+size) range
        RwD3D9VBBufferNodeR4* cur = strideNode->bufferList;
        if (cur == nullptr) {
            return;
        }
        while (cur->vertexBuffer != (IDirect3DVertexBuffer9*)vertexBuffer ||
               !((cur->freeOffset + cur->remaining == offset) || (offset + size == cur->freeOffset))) {
            cur = cur->prev;
            if (cur == nullptr) {
                return;
            }
        }

        // unlink the node we merged into and hand its range over to `cur`
        if (node->next != nullptr) {
            node->next->prev = node->prev;
        }
        if (node->prev != nullptr) {
            node->prev->next = node->next;
        }
        if (strideNode->bufferList == node) {
            strideNode->bufferList = node->next;
        }
        RwGetGlobalsRwcore4()->memoryFree(r4VbBufferNodeFl, node);
        node = cur;
    } while (true);
}


RwBool RwD3D9DynamicVertexBufferCreate(RwUInt32 size, void** vertexBuffer) {
    RwD3D9DynVBNodeR4* recycle = nullptr;
    RwD3D9DynVBNodeR4* node    = r4DynVbList;
    if (node != nullptr) {
        while (true) {
            RwD3D9DynVBNodeR4* next = node->next;
            if (node->used == 0) {
                if (node->size == size) {
                    node->used = 1;
                    *vertexBuffer = node->vertexBuffer;
                    node->vertexBufferOwner = vertexBuffer;
                    return TRUE;
                }
                if (recycle == nullptr || recycle->size < node->size) {
                    recycle = node;                    // largest free node so far
                }
            }
            if (next == nullptr) {
                break;
            }
            node = next;
        }
        if (recycle != nullptr) {
            recycle->vertexBuffer->Release();          // vtable +0x08
            recycle->vertexBuffer = nullptr;
        } else {
            recycle = (RwD3D9DynVBNodeR4*)RwGetGlobalsRwcore4()->memoryAlloc(r4DynVbFl, 0x40411);
            recycle->next = r4DynVbList;
            r4DynVbList   = recycle;
        }
    } else {
        recycle = (RwD3D9DynVBNodeR4*)RwGetGlobalsRwcore4()->memoryAlloc(r4DynVbFl, 0x40411);
        recycle->next = r4DynVbList;
        r4DynVbList   = recycle;
    }

    RwUInt32 pool = (~r4D3D9DriverFlags >> 0xF) & 2;   // 2 = D3DPOOL_SYSTEMMEM if bit15 clear, else 0 = D3DPOOL_DEFAULT
    IDirect3DVertexBuffer9* vb = nullptr;
    HRESULT hr = _RwD3DDevice->CreateVertexBuffer(size,                            // Length
                                                  D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, // 0x208
                                                  0,                           // FVF
                                                  (D3DPOOL)pool,
                                                  &vb,
                                                  nullptr);
    if (FAILED(hr)) {
        return FALSE;
    }
    recycle->size              = size;
    recycle->used              = 1;
    recycle->vertexBuffer      = vb;
    recycle->vertexBufferOwner = vertexBuffer;
    *vertexBuffer = vb;
    return TRUE;
}


void RwD3D9DynamicVertexBufferDestroy(void* vertexBuffer) {
    for (RwD3D9DynVBNodeR4* node = r4DynVbList; node != nullptr; node = node->next) {
        if (node->vertexBuffer == (IDirect3DVertexBuffer9*)vertexBuffer) {
            node->used = 0;
            node->vertexBufferOwner = nullptr;
            return;
        }
    }
}


RwBool RwD3D9DynamicVertexBufferLock(RwUInt32 vertexSize, RwUInt32 numVertex, void** vertexBufferOut, void** vertexDataOut, RwUInt32* baseIndexOut) {
    RwUInt32 size  = vertexSize * numVertex;
    RwUInt32 offset = 0;
    RwUInt32 total  = size;
    RwUInt32 cur    = r4DynVbCurrent;
    if ((r4D3D9DriverFlags & 0x10000) != 0) {
        offset = ((r4DynVbOffsets[cur] + vertexSize - 1) / vertexSize) * vertexSize;
        total  = offset + size;
        if (total > r4DynVbSizes[cur]) {
            bool found = false;
            for (RwUInt32 i = 0; i < 4 && !found; i++) {
                if (i == r4DynVbCurrent) {
                    continue;
                }
                offset = ((r4DynVbOffsets[i] + vertexSize - 1) / vertexSize) * vertexSize;
                total  = offset + size;
                if (total <= r4DynVbSizes[i]) {
                    r4DynVbCurrent = i;
                    cur   = i;
                    found = true;
                }
            }
            if (!found) {
                r4DynVbCurrent = r4DynVbCurrent + 1;
                if (r4DynVbCurrent > 3) {
                    r4DynVbCurrent = 0;
                }
                cur    = r4DynVbCurrent;
                offset = 0;
                total  = size;
            }
        }
    }

    if (r4DynVbSizes[cur] <= size && size - r4DynVbSizes[cur] != 0) {
        // request larger than the buffer: release its users and re-create it bigger
        for (RwD3D9DynVBNodeR4* node = r4DynVbList; node != nullptr; node = node->next) {
            if (node->vertexBuffer == r4DynVbBuffers[cur]) {
                node->used = 0;
                node->vertexBufferOwner = nullptr;
                break;
            }
        }
        r4DynVbSizes[cur] = size;
        RwD3D9DynamicVertexBufferCreate(size, reinterpret_cast<void**>(&r4DynVbBuffers[cur]));
    }

    IDirect3DVertexBuffer9* vb = r4DynVbBuffers[cur];
    HRESULT hr;
    if (offset == 0) {
        hr = vb->Lock(0, size, vertexDataOut, D3DLOCK_NOSYSLOCK | D3DLOCK_DISCARD);    // 0x2800
        r4DynVbOffsets[cur] = size;
        *baseIndexOut = 0;
    } else {
        hr = vb->Lock(offset, size, vertexDataOut, D3DLOCK_NOSYSLOCK | D3DLOCK_NOOVERWRITE); // 0x1800
        r4DynVbOffsets[cur] = total;
        *baseIndexOut = offset / vertexSize;
    }
    *vertexBufferOut = vb;
    return hr >= 0;
}


RwBool RwD3D9DynamicVertexBufferUnlock(void* vertexBuffer) {
    HRESULT hr = ((IDirect3DVertexBuffer9*)vertexBuffer)->Unlock();
    return hr >= 0;
}


void RwRasterSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    StaticRef<RwInt32>(0x8E2530) = blockSize;           // DAT_008e2530
    StaticRef<RwInt32>(0x8E2534) = numBlocksToPrealloc; // DAT_008e2534
}



RwRaster* RwRasterCreate(RwInt32 width, RwInt32 height, RwInt32 depth, RwInt32 flags) {
    /* Декомпил 0x7FB230: raster = memoryAlloc(+0x144)(rasterFreeList@+0x60, 0x30407);
       заполнение полей в точном порядке; stdFunc[rwSTANDARDRASTERCREATE](0, raster, flags);
       успех => InitObject(&реестр, raster) и возврат raster; провал => memoryFree(+0x148). */
    RwRaster* raster = static_cast<RwRaster*>(RwGetFreelistAllocGta()(RwGetRasterGlobalsGta()->rasterFreeList,
                                                                     0x30407 /* hint, литерал из декомпила */));
    if (raster == nullptr)
        return nullptr;

    raster->width        = width;    /* +0x0C */
    raster->privateFlags = 0;        /* +0x22 */
    raster->height       = height;   /* +0x10 */
    raster->depth        = depth;    /* +0x14 */
    raster->cFlags       = 0;        /* +0x21 */
    raster->nOffsetX     = 0;        /* +0x1C */
    raster->nOffsetY     = 0;        /* +0x1E */
    raster->parent       = raster;   /* +0x00: созданный растр — сам себе parent */
    raster->cpPixels     = nullptr;  /* +0x04 */
    raster->palette      = nullptr;  /* +0x08 */

    if (RwGetStdFuncsGta()[rwSTANDARDRASTERCREATE](nullptr, raster, flags)) {
        _rwPluginRegistryInitObject(&rwRasterPluginRegistryGta, raster);
        return raster;
    }
    RwGetFreelistFreeGta()(RwGetRasterGlobalsGta()->rasterFreeList, raster);
    return nullptr;
}



RwBool RwRasterDestroy(RwRaster* raster) {
    /* Декомпил 0x7FB020: DeInitObject(&DAT_008e2518, raster); stdFunc[+0x5C](0, raster, 0);
       memoryFree(+0x148)(freelist@+0x60, raster); безусловно TRUE. */
    _rwPluginRegistryDeInitObject(&rwRasterPluginRegistryGta, raster);
    RwGetStdFuncsGta()[rwSTANDARDRASTERDESTROY](nullptr, raster, 0);
    RwGetFreelistFreeGta()(RwGetRasterGlobalsGta()->rasterFreeList, raster);
    return TRUE;
}



RwRaster* RwRasterGetOffset(RwRaster* raster, RwInt16* xOffset, RwInt16* yOffset) {
    /* Декомпил: *xOffset = *(u16*)(raster+0x1C); *yOffset = *(u16*)(raster+0x1E); — без null-checks.
       ABI-заметка: в бинарнике возвращаемое значение не выставляется (Ghidra: void);
       возвращаем raster по сигнатуре репо-хедера (как в официальном SDK). */
    *xOffset = raster->nOffsetX;
    *yOffset = raster->nOffsetY;
    return raster;
}



RwInt32 RwRasterGetNumLevels(RwRaster* raster) {
    /* Декомпил: if (*(char*)(raster+0x23) >= 0) return 1; — знаковый байт cFormat:
       бит 0x80 в cFormat = rwRASTERFORMATMIPMAP>>8, т.е. без mipmap — ровно 1 уровень.
       Иначе stdFunc[rwSTANDARDRASTERGETMIPLEVELS](&слот, raster, 0): драйвер пишет
       число уровней В слот (инициализируется адресом raster, как в декомпиле);
       неудача => -1. */
    if ((RwInt8)raster->cFormat >= 0)
        return 1;
    RwUInt32 numLevels = (RwUInt32)(uintptr_t)raster;
    if (RwGetStdFuncsGta()[rwSTANDARDRASTERGETMIPLEVELS](&numLevels, raster, 0))
        return (RwInt32)numLevels;
    return -1;
}



RwRaster* RwRasterSubRaster(RwRaster* subRaster, RwRaster* raster, RwRect* rect) {
    /* Декомпил 0x7FB1D0: гейт (char)(subRaster->cFlags(+0x21)) < 0 — это флаг
       rwRASTERDONTALLOCATE (0x80), как в librw Raster::subRaster. */
    if ((RwInt8)subRaster->cFlags < 0) {
        subRaster->width    = rect->w;                                    /* +0x0C */
        subRaster->height   = rect->h;                                    /* +0x10 */
        subRaster->nOffsetX = (RwInt16)(raster->nOffsetX + rect->x);      /* +0x1C */
        subRaster->nOffsetY = (RwInt16)(raster->nOffsetY + rect->y);      /* +0x1E */
        if (RwGetStdFuncsGta()[rwSTANDARDRASTERSUBRASTER](subRaster, raster, 0)) {
            subRaster->parent = raster->parent; /* *subRaster = *raster (dword 0) — конечный родитель */
            return subRaster;
        }
    }
    return nullptr;
}



RwRaster* RwRasterRenderFast(RwRaster* raster, RwInt32 x, RwInt32 y) {
    /* Декомпил: {x,y} кладутся в локальную пару и адрес передаётся как pInOut.
       Результат диспатча игнорируется, возвращается raster. */
    RwInt32 pos[2] = { x, y };
    RwGetStdFuncsGta()[rwSTANDARDRASTERRENDERFAST](raster, pos, 0);
    return raster;
}



RwRaster* RwRasterRender(RwRaster* raster, RwInt32 x, RwInt32 y) {
    /* Декомпил: {x,y} в локальную пару, адрес — как pInOut; результат игнорируется. */
    RwInt32 pos[2] = { x, y };
    RwGetStdFuncsGta()[rwSTANDARDRASTERRENDER](raster, pos, 0);
    return raster;
}



RwRaster* RwRasterRenderScaled(RwRaster* raster, RwRect* rect) {
    /* Декомпил: stdFunc[+0x90](raster, rect, 0); результат игнорируется, возвращается raster. */
    RwGetStdFuncsGta()[rwSTANDARDRASTERRENDERSCALED](raster, rect, 0);
    return raster;
}



RwRaster* RwRasterPushContext(RwRaster* raster) {
    /* Декомпил 0x7FB060: stdFunc[rwSTANDARDSETRASTERCONTEXT](0, raster, 0);
       при успехе ++curContextIndex; contexts[curContextIndex] = raster (индекс инкрементится ДО записи). */
    if (RwGetStdFuncsGta()[rwSTANDARDSETRASTERCONTEXT](nullptr, raster, 0)) {
        RwRasterGlobalsGta* rasterGlobals = RwGetRasterGlobalsGta();
        rasterGlobals->curContextIndex++;
        rasterGlobals->contexts[rasterGlobals->curContextIndex] = raster;
        return raster;
    }
    return nullptr;
}



RwRaster* RwRasterPopContext() {
    /* Декомпил 0x7FB110: idx = curContextIndex; curContextIndex = idx-1; popped = contexts[idx];
       stdFunc[rwSTANDARDSETRASTERCONTEXT](0, contexts[curContextIndex после декремента], 0);
       при успехе — popped, иначе 0. (contexts[-1] при пустом стеке читается как в оригинале.) */
    RwRasterGlobalsGta* rasterGlobals = RwGetRasterGlobalsGta();
    const RwInt32  poppedIndex = rasterGlobals->curContextIndex;
    rasterGlobals->curContextIndex = poppedIndex - 1;
    const RwRaster* popped = rasterGlobals->contexts[poppedIndex];
    if (RwGetStdFuncsGta()[rwSTANDARDSETRASTERCONTEXT](nullptr, rasterGlobals->contexts[rasterGlobals->curContextIndex], 0))
        return const_cast<RwRaster*>(popped);
    return nullptr;
}



RwRaster* RwRasterGetCurrentContext() {
    /* Декомпил: *(EI + offset + curContextIndex*4) — contexts[curContextIndex], без проверок границ. */
    RwRasterGlobalsGta* rasterGlobals = RwGetRasterGlobalsGta();
    return rasterGlobals->contexts[rasterGlobals->curContextIndex];
}



RwBool RwRasterClear(RwInt32 pixelValue) {
    /* Декомпил: клир возможен только при curContextIndex > 0 (есть текущий растр-контекст). */
    if (0 < RwGetRasterGlobalsGta()->curContextIndex) {
        RwGetStdFuncsGta()[rwSTANDARDRASTERCLEAR](nullptr, nullptr, pixelValue);
        return TRUE;
    }
    RwError rwError;
    rwError.pluginID  = 1; /* plcore */
    rwError.errorCode = _rwerror(0x15); /* FUN_008088d0 */
    RwErrorSet(&rwError); /* FUN_00808820 */
    return FALSE;
}



RwBool RwRasterClearRect(RwRect* rpRect, RwInt32 pixelValue) {
    /* Декомпил: stdFunc[+0x7C](0, rpRect, pixelValue); безусловно TRUE. */
    RwGetStdFuncsGta()[rwSTANDARDRASTERCLEARRECT](nullptr, rpRect, pixelValue);
    return TRUE;
}



RwRaster* RwRasterShowRaster(RwRaster* raster, void* dev, RwUInt32 flags) {
    /* Декомпил 0x7FB1A0: указатель stdFunc[rwSTANDARDRASTERSHOWRASTER] читается ДО
       _rwResourcesPurge() (0x807E50), потом вызов (raster, dev, flags);
       при успехе — raster, иначе 0. */
    const RwStandardFunc showFn = RwGetStdFuncsGta()[rwSTANDARDRASTERSHOWRASTER];
    _rwResourcesPurge();
    if (showFn(raster, dev, (RwInt32)flags))
        return raster;
    return nullptr;
}


//! NOTE: This function is responsible for calculating the `stride` in the `raster`

RwUInt8* RwRasterLock(RwRaster* raster, RwUInt8 level, RwInt32 lockMode) {
    /* Декомпил 0x7FB2D0: stdFunc[rwSTANDARDRASTERLOCK](&слот, raster, (level&0xFF)*0x100 + lockMode)
       — драйвер пишет указатель на пиксели В слот; при успехе — содержимое слота, иначе 0. */
    RwUInt32 lockOut = level;
    if (RwGetStdFuncsGta()[rwSTANDARDRASTERLOCK](&lockOut, raster, ((RwUInt32)level & 0xFF) * 0x100 + (RwUInt32)lockMode))
        return reinterpret_cast<RwUInt8*>(lockOut);
    return nullptr;
}



RwRaster* RwRasterUnlock(RwRaster* raster) {
    /* Декомпил: stdFunc[+0x88](0, raster, 0); результат игнорируется, возвращается raster. */
    RwGetStdFuncsGta()[rwSTANDARDRASTERUNLOCK](nullptr, raster, 0);
    return raster;
}



RwUInt8* RwRasterLockPalette(RwRaster* raster, RwInt32 lockMode) {
    /* Декомпил 0x7FB0E0: stdFunc[rwSTANDARDRASTERLOCKPALETTE](&слот, raster, lockMode);
       драйвер пишет указатель палитры В слот (3-й аргумент дублирует lockMode);
       при успехе возвращается содержимое слота, иначе 0 (идиом -(ok) & slot). */
    RwUInt32 paletteOut = (RwUInt32)lockMode;
    if (RwGetStdFuncsGta()[rwSTANDARDRASTERLOCKPALETTE](&paletteOut, raster, (RwUInt32)lockMode))
        return reinterpret_cast<RwUInt8*>(paletteOut);
    return nullptr;
}



RwRaster* RwRasterUnlockPalette(RwRaster* raster) {
    /* Декомпил: stdFunc[+0xA8](0, raster, 0); затем raster->privateFlags(+0x22) &= 0xE7,
       т.е. сброс rwRASTERPALETTELOCKED (READ|WRITE = 0x08|0x10). */
    RwGetStdFuncsGta()[rwSTANDARDRASTERUNLOCKPALETTE](nullptr, raster, 0);
    raster->privateFlags &= (RwUInt8)~rwRASTERPALETTELOCKED; /* & 0xE7 */
    return raster;
}



RwInt32 RwRasterRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    /* Декомпил: FUN_008084a0(&DAT_008e2518, ...) = _rwPluginRegistryAddPlugin; eax пробрасывается. */
    return _rwPluginRegistryAddPlugin(&rwRasterPluginRegistryGta, size, pluginID, constructCB, destructCB, copyCB);
}



RwInt32 RwRasterGetPluginOffset(RwUInt32 pluginID) {
    /* Декомпил: FUN_00808470(&DAT_008e2518, pluginID) = _rwPluginRegistryGetPluginOffset */
    return _rwPluginRegistryGetPluginOffset(&rwRasterPluginRegistryGta, pluginID);
}


RxRenderStateVector *RxRenderStateVectorSetDefaultRenderStateVector(RxRenderStateVector *rsvp)
{
    if (rsvp == NULL)
    {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl *)(RwInt32))0x8088D0)(0x80000016 /* E_RW_NULLPOINTER */);
        RwErrorSet(&rwError);
        return NULL;
    }

    if (RwGetGlobalsFnViewGta()->engineStatus == rwENGINESTATUSSTARTED /* 3 */)
    {
        RwUInt32 *src = (RwUInt32 *)((RwUInt8 *)RwEngineInstance + g_rwRxRenderStateGlobalsOffset + 4);
        RwUInt32 *dst = (RwUInt32 *)rsvp;
        RwInt32 n = 0xB; /* sizeof(RxRenderStateVector) / 4 = 44 bytes */
        do
        {
            *dst = *src;
            src++;
            dst++;
            n--;
        } while (n != 0);
        return rsvp;
    }

    if (rsvp != (RxRenderStateVector *)((RwUInt8 *)RwEngineInstance + g_rwRxRenderStateGlobalsOffset + 4))
    {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl *)(RwInt32))0x8088D0)(0x80000018);
        RwErrorSet(&rwError);
        return NULL;
    }

    /* hard-coded defaults (decompile store order preserved) */
    rsvp->TextureRaster = NULL;                            /* +0x10 = 0 */
    rsvp->ShadeMode = (RwShadeMode)2;                      /* +0x04 = 2 */
    rsvp->FilterMode = (RwTextureFilterMode)2;             /* +0x1C = 2 */
    rsvp->FogType = (RwFogType)0;                          /* +0x24 = 0 */
    rsvp->Flags = 7;                                       /* +0x00 = 7 */
    rsvp->SrcBlend = (RwBlendFunction)5;                   /* +0x08 = 5 */
    rsvp->DestBlend = (RwBlendFunction)6;                  /* +0x0C = 6 */
    rsvp->AddressModeU = (RwTextureAddressMode)1;          /* +0x14 = 1 */
    rsvp->AddressModeV = (RwTextureAddressMode)1;          /* +0x18 = 1 */
    rsvp->BorderColor.red = 255;                           /* +0x20 = 0xFFFFFFFF */
    rsvp->BorderColor.green = 255;
    rsvp->BorderColor.blue = 255;
    rsvp->BorderColor.alpha = 255;
    rsvp->FogColor.red = 255;                              /* +0x28 = 0xFFFFFFFF */
    rsvp->FogColor.green = 255;
    rsvp->FogColor.blue = 255;
    rsvp->FogColor.alpha = 255;
    return rsvp;
}


RwImage* RwImageCreate(RwInt32 width, RwInt32 height, RwInt32 depth) {
    RwImage* image = static_cast<RwImage*>(RwGtaFreelistAlloc()(RwGetImageModuleGta().imageFreeList, 0x30018));
    if (image == nullptr)
        return nullptr;
    image->width    = width;
    image->height   = height;
    image->depth    = depth;
    image->cpPixels = nullptr;  // [5]
    image->palette  = nullptr;  // [6]
    image->flags    = 0;        // [0] (поле stride [4] декомпил НЕ обнуляет)
    _rwPluginRegistryInitObject(&rwImagePluginRegistry, image);
    return image;
}


RwBool RwImageDestroy(RwImage* image) {
    if ((image->flags & rwIMAGEALLOCATED) != 0) { // bit0
        RwGtaMemFuncs().rwfree(image->cpPixels);
        image->cpPixels = nullptr;
        image->palette  = nullptr;
        image->flags   &= ~rwIMAGEALLOCATED;
    }
    _rwPluginRegistryDeInitObject(&rwImagePluginRegistry, image);
    RwGtaFreelistFree()(RwGetImageModuleGta().imageFreeList, image);
    return TRUE;
}


RwImage* RwImageAllocatePixels(RwImage* image) {
    RwUInt32 paletteSize;
    bool hasPalette;
    if (image->depth == 4 || image->depth == 8) {
        hasPalette  = true;
        paletteSize = (1 << image->depth) << 2; // 2^depth записей RwRGBA
    } else {
        hasPalette  = false;
        paletteSize = 0;
    }

    const RwUInt32 stride  = (((image->depth + 7) >> 3) * image->width + 3) & ~3u;
    const RwUInt32 pixSize = image->height * stride;
    image->stride = static_cast<RwInt32>(stride);

    const RwUInt32 totalSize = pixSize + paletteSize;
    RwUInt8* pixels = static_cast<RwUInt8*>(RwGtaMemFuncs().rwmalloc(totalSize, 0x30018));
    image->cpPixels = pixels;
    if (pixels == nullptr) {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, RwInt32))0x8088D0)(0x80000013, static_cast<RwInt32>(totalSize));
        RwErrorSet(&rwError);
        return nullptr;
    }
    if (hasPalette) {
        image->palette = reinterpret_cast<RwRGBA*>(pixels + pixSize);
    } else {
        image->palette = nullptr;
    }
    image->flags |= rwIMAGEALLOCATED;
    return image;
}


RwImage* RwImageFreePixels(RwImage* image) {
    RwGtaMemFuncs().rwfree(image->cpPixels);
    image->cpPixels = nullptr;
    image->palette  = nullptr;
    image->flags   &= ~rwIMAGEALLOCATED;
    return image;
}


RwImage* RwImageApplyMask(RwImage* image, const RwImage* mask) {
    if (image->width != mask->width || image->height != mask->height) {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x8000000A);
        RwErrorSet(&rwError);
        return nullptr;
    }

    if (image->depth == 4 || image->depth == 8) {
        // Временная копия маски той же геометрии/глубины
        RwImage* maskCopy = static_cast<RwImage*>(
            RwGtaFreelistAlloc()(RwGetImageModuleGta().imageFreeList, 0x30018));
        if (maskCopy == nullptr)
            return nullptr;
        maskCopy->width    = image->width;
        maskCopy->height   = image->height;
        maskCopy->depth    = image->depth;
        maskCopy->cpPixels = nullptr;
        maskCopy->palette  = nullptr;
        maskCopy->flags    = 0;
        _rwPluginRegistryInitObject(&rwImagePluginRegistry, maskCopy);

        if (RwImageAllocatePixels(maskCopy) == nullptr) {
            if ((maskCopy->flags & rwIMAGEALLOCATED) != 0) {
                RwGtaMemFuncs().rwfree(maskCopy->cpPixels);
                maskCopy->cpPixels = nullptr;
                maskCopy->palette  = nullptr;
                maskCopy->flags   &= ~rwIMAGEALLOCATED;
            }
            _rwPluginRegistryDeInitObject(&rwImagePluginRegistry, maskCopy);
            RwGtaFreelistFree()(RwGetImageModuleGta().imageFreeList, maskCopy);
            return nullptr;
        }

        // Конверсия пикселей маски в глубину image (оригинальную)
        RwInt32 convertOk;
        if (maskCopy->depth == image->depth)
            convertOk = ((RwInt32(__cdecl*)(RwImage*, RwImage*))0x803790)(maskCopy, const_cast<RwImage*>(image));
        else
            convertOk = ((RwInt32(__cdecl*)(RwImage*, RwImage*))0x803840)(maskCopy, const_cast<RwImage*>(image));

        // (decompile 0x802B4x): puVar10 = maskCopy (NULL при неудаче); разыменование как в бинаре
        RwImage* flagTarget = maskCopy;
        if (convertOk == 0)
            flagTarget = nullptr;
        RwUInt32 fl = *reinterpret_cast<RwUInt32*>(flagTarget);
        *reinterpret_cast<RwUInt32*>(flagTarget) = fl & ~rwIMAGEGAMMACORRECTED;
        *reinterpret_cast<RwUInt32*>(flagTarget) = (image->flags & rwIMAGEGAMMACORRECTED) | (fl & ~rwIMAGEGAMMACORRECTED);

        // image -> 32 бита
        if ((image->flags & rwIMAGEALLOCATED) != 0) {
            RwGtaMemFuncs().rwfree(image->cpPixels);
            image->cpPixels = nullptr;
            image->palette  = nullptr;
            image->flags   &= ~rwIMAGEALLOCATED;
        }
        image->depth = 32;
        RwImageAllocatePixels(image);

        if (image->depth == maskCopy->depth) {
            // Копия палитры (обе есть, глубина маски < 9)
            if (image->palette != nullptr && maskCopy->palette != nullptr && maskCopy->depth < 9) {
                RwUInt32 count = (1u << maskCopy->depth) & 0x3fffffff;
                RwUInt32* dst = reinterpret_cast<RwUInt32*>(image->palette);
                RwUInt32* src = reinterpret_cast<RwUInt32*>(maskCopy->palette);
                for (; count != 0; count--) {
                    *dst = *src;
                    dst++;
                    src++;
                }
            }
            // Построчная копия пикселей (dwords + хвост), stride обеих сторон
            const RwUInt32 rowBytes = ((image->depth + 7) >> 3) * image->width;
            RwUInt8* src = maskCopy->cpPixels;
            RwUInt8* dst = image->cpPixels;
            for (RwInt32 y = 0; y < image->height; y++) {
                RwUInt8* s = src;
                RwUInt8* d = dst;
                for (RwUInt32 i = rowBytes >> 2; i != 0; i--) {
                    *reinterpret_cast<RwUInt32*>(d) = *reinterpret_cast<RwUInt32*>(s);
                    s += 4;
                    d += 4;
                }
                for (RwUInt32 i = rowBytes & 3; i != 0; i--) {
                    *d = *s;
                    s++;
                    d++;
                }
                src += maskCopy->stride;
                dst += image->stride;
            }
        } else {
            // Глубины разошлись (image уже 32): конверсия прямо в пиксели image
            convertOk = ((RwInt32(__cdecl*)(RwImage*, RwImage*))0x803840)(image, maskCopy);
            if (convertOk == 0)
                flagTarget = nullptr; // decompile: puVar10 = NULL (см. примечание выше)
            else
                flagTarget = image;
        }

        // image наследует флаг гамма-коррекции из маски (decompile 0x802D6x)
        fl = *reinterpret_cast<RwUInt32*>(flagTarget);
        *reinterpret_cast<RwUInt32*>(flagTarget) = fl & ~rwIMAGEGAMMACORRECTED;
        *reinterpret_cast<RwUInt32*>(flagTarget) = (maskCopy->flags & rwIMAGEGAMMACORRECTED) | (fl & ~rwIMAGEGAMMACORRECTED);

        // Уничтожение временной маски
        RwGtaMemFuncs().rwfree(maskCopy->cpPixels);
        maskCopy->cpPixels = nullptr;
        maskCopy->palette  = nullptr;
        maskCopy->flags   &= ~rwIMAGEALLOCATED;
        _rwPluginRegistryDeInitObject(&rwImagePluginRegistry, maskCopy);
        RwGtaFreelistFree()(RwGetImageModuleGta().imageFreeList, maskCopy);
    } else if (image->depth != 32) {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000009);
        RwErrorSet(&rwError);
        return nullptr;
    }

    // Альфа из маски-аргумента в 32-битные пиксели image
    {
        const RwRGBA* maskPalette = mask->palette;
        const RwUInt8* maskPix    = mask->cpPixels;
        RwUInt8* imagePix         = image->cpPixels;
        for (RwInt32 y = 0; y < image->height; y++) {
            const RwInt32 maskDepth = mask->depth;
            if (maskDepth == 4 || maskDepth == 8) {
                RwInt32 x = 0;
                if (0 < image->width) {
                    const RwUInt8* m = maskPix;
                    RwUInt8* d = imagePix + 3;
                    do {
                        const RwUInt8 idx = *m++;
                        *d = maskPalette[idx].alpha; // maskPalette[idx*4+3]
                        d += 4;
                        x++;
                    } while (x < image->width);
                }
            } else if (maskDepth == 32) {
                RwInt32 x = 0;
                if (0 < image->width) {
                    RwUInt8* d = imagePix + 3;
                    do {
                        *d = d[maskPix - imagePix]; // альфа соответствующего пикселя маски
                        d += 4;
                        x++;
                    } while (x < image->width);
                }
            }
            maskPix  += mask->stride;
            imagePix += image->stride;
        }
    }
    return image;
}


RwImage* RwImageMakeMask(RwImage* image) {
    if (image->depth == 4 || image->depth == 8) {
        // компактная палитра: count = 2^depth записей RwRGBA, stride не участвует
        RwInt32 count = 1 << image->depth;
        RwUInt8* entry = reinterpret_cast<RwUInt8*>(image->palette) + 2; // указатель на b-байт записи
        for (; 0 < count; count--) {
            RwUInt8 maxc = entry[-2];
            if (maxc < entry[-1])
                maxc = entry[-1];
            if (maxc < entry[0])
                maxc = entry[0];
            entry[1] = maxc; // alpha
            entry += 4;
        }
    } else if (image->depth == 32) {
        // по строкам с учётом stride
        RwUInt8* pixels = image->cpPixels;
        for (RwInt32 y = 0; y < image->height; y++) {
            RwUInt8* px = pixels + 2;
            for (RwInt32 x = 0; x < image->width; x++) {
                RwUInt8 maxc = px[-2];
                if (maxc < px[-1])
                    maxc = px[-1];
                if (maxc < px[0])
                    maxc = px[0];
                px[1] = maxc; // alpha
                px += 4;
            }
            pixels += image->stride;
        }
    }
    return image;
}


RwImage* RwImageReadMaskedImage(const RwChar* imageName, const RwChar* maskname) {
    RwImage* image = RwImageRead(imageName);
    if (image == nullptr)
        return nullptr;

    if (maskname == nullptr || *maskname == '\0')
        return image;

    RwImage* mask = RwImageRead(maskname);
    if (mask == nullptr) {
        // Маски нет: image остаётся, но должен быть уничтожен и перечитан как обычный
        if ((image->flags & rwIMAGEALLOCATED) != 0) {
            RwGtaMemFuncs().rwfree(image->cpPixels);
            image->cpPixels = nullptr;
            image->palette  = nullptr;
            image->flags   &= ~rwIMAGEALLOCATED;
        }
        _rwPluginRegistryDeInitObject(&rwImagePluginRegistry, image);
        mask = image;
    } else {
        if (RwImageMakeMask(mask) != nullptr
            && RwImageApplyMask(image, mask) != nullptr) {
            if ((mask->flags & rwIMAGEALLOCATED) != 0) {
                RwGtaMemFuncs().rwfree(mask->cpPixels);
                mask->cpPixels = nullptr;
                mask->palette  = nullptr;
                mask->flags   &= ~rwIMAGEALLOCATED;
            }
            _rwPluginRegistryDeInitObject(&rwImagePluginRegistry, mask);
            RwGtaFreelistFree()(RwGetImageModuleGta().imageFreeList, mask);
            return image;
        }
        // Неудача: уничтожить оба
        if ((image->flags & rwIMAGEALLOCATED) != 0) {
            RwGtaMemFuncs().rwfree(image->cpPixels);
            image->flags &= ~rwIMAGEALLOCATED;
            image->palette  = nullptr;
            image->cpPixels = nullptr;
        }
        _rwPluginRegistryDeInitObject(&rwImagePluginRegistry, image);
        RwGtaFreelistFree()(RwGetImageModuleGta().imageFreeList, image);
        if ((mask->flags & rwIMAGEALLOCATED) != 0) {
            RwGtaMemFuncs().rwfree(mask->cpPixels);
            mask->cpPixels = nullptr;
            mask->palette  = nullptr;
            mask->flags   &= ~rwIMAGEALLOCATED;
        }
        _rwPluginRegistryDeInitObject(&rwImagePluginRegistry, mask);
    }
    RwGtaFreelistFree()(RwGetImageModuleGta().imageFreeList, mask);
    return nullptr;
}


RwImage* RwImageRead(const RwChar* imageName) {
    RwChar* found = RwGtaStringFuncs().vecStrrchr(imageName, ':');
    const RwChar* searchBase = imageName;
    if (found != nullptr)
        searchBase = found;
    found = RwGtaStringFuncs().vecStrrchr(searchBase, '/');
    if (found != nullptr)
        searchBase = found;
    found = RwGtaStringFuncs().vecStrrchr(searchBase, '\\');
    if (found != nullptr)
        searchBase = found;
    RwChar* dot = RwGtaStringFuncs().vecStrrchr(searchBase, '.');
    if (dot != nullptr) {
        for (RwImageReaderNodeGta* reader = static_cast<RwImageReaderNodeGta*>(RwGetImageModuleGta().imageReaders);
             reader != nullptr;
             reader = reader->next) {
            if (RwGtaStringFuncs().vecStrcmp(reader->ext1, dot) == 0
                || RwGtaStringFuncs().vecStrcmp(reader->ext2, dot) == 0) {
                if (reader->read == nullptr)
                    return nullptr;
                struct ReadCtxGta
                {
                    RwImageCallBackRead readFn; // +0
                    RwImage* result;            // +4
                } ctx;
                ctx.readFn = reader->read;
                ctx.result = nullptr;
                ((void*(__cdecl*)(const RwChar*, RwInt32, void*, void*))0x8030C0)(imageName, 5, (void*)0x8032D0, &ctx);
                return ctx.result;
            }
        }
    }
    return nullptr;
}


const RwChar* RwImageSetPath(const RwChar* path) {
    RwImageModuleGta& mod = RwGetImageModuleGta();
    const RwUInt32 len = RwGtaStringFuncs().vecStrlen(path) + 1;
    if (mod.imagePathSize < static_cast<RwInt32>(len)) {
        const RwChar* newBuf = static_cast<const RwChar*>(
            RwGtaMemFuncs().rwrealloc(mod.imagePath, len, 0x1040406));
        if (newBuf == nullptr) {
            RwError rwError;
            rwError.pluginID = 1;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, RwInt32))0x8088D0)(0x80000013, static_cast<RwInt32>(len));
            RwErrorSet(&rwError);
            return nullptr;
        }
        mod.imagePath     = const_cast<RwChar*>(newBuf);
        mod.imagePathSize = static_cast<RwInt32>(len);
    }
    // копия len байт (dwords + хвост), как в декомпиле
    {
        const RwChar* src = path;
        RwChar* dst = mod.imagePath;
        for (RwUInt32 i = len >> 2; i != 0; i--) {
            *reinterpret_cast<RwUInt32*>(dst) = *reinterpret_cast<const RwUInt32*>(src);
            src += 4;
            dst += 4;
        }
        for (RwUInt32 i = len & 3; i != 0; i--) {
            *dst = *src;
            src++;
            dst++;
        }
    }
    return path;
}


RwBool RwImageSetGamma(RwReal gammaValue) {
    RwImageModuleGta& mod = RwGetImageModuleGta();
    mod.gamma = gammaValue;

    const float invGamma = 1.0f / gammaValue; // *(float*)0x858624 / gamma
    mod.gammaTable[0] = 0;
    mod.invGammaTable[0] = 0;
    for (RwInt32 i = 1; i < 0x100; i++) {
        const float v = static_cast<float>(i) * 0.003921569f; // 1/255
        const float scaledFwd = static_cast<float>(pow(static_cast<double>(v), static_cast<double>(invGamma))) * 255.0f + 0.5f;
        const RwUInt8 fwd = static_cast<RwUInt8>(static_cast<RwInt32>(floor(scaledFwd + 0.5f)));
        mod.gammaTable[i] = fwd;
        const float scaledInv = static_cast<float>(pow(static_cast<double>(v), static_cast<double>(gammaValue))) * 255.0f + 0.5f;
        const RwUInt8 inv = static_cast<RwUInt8>(static_cast<RwInt32>(floor(scaledInv + 0.5f)));
        mod.invGammaTable[i] = inv;
    }
    return TRUE;
}


RwImage* RwImageGammaCorrect(RwImage* image) {
    RwUInt8* const gammaTable = RwGetImageModuleGta().gammaTable; // engine+off+0xC

    if (image->depth == 4 || image->depth == 8) {
        if (image->palette == nullptr) {
            RwError rwError;
            rwError.pluginID = 1;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000016);
            RwErrorSet(&rwError);
            return nullptr;
        }
        RwInt32 count = 1 << image->depth;
        RwUInt8* entry = reinterpret_cast<RwUInt8*>(image->palette) + 2; // указатель на b-байт записи
        // корректируются r,g,b, альфа не трогается
        for (; count != 0; count--) {
            entry[-2] = gammaTable[entry[-2]];
            entry[-1] = gammaTable[entry[-1]];
            entry[0]  = gammaTable[entry[0]];
            entry    += 4; // pbVar4[1] = pbVar4[1]; — альфа без изменений
        }
    } else if (image->depth != 32) {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000008);
        RwErrorSet(&rwError);
        return nullptr;
    } else {
        if (image->cpPixels == nullptr) {
            RwError rwError;
            rwError.pluginID = 1;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000016);
            RwErrorSet(&rwError);
            return nullptr;
        }
        RwUInt8* pixels = image->cpPixels;
        for (RwInt32 y = 0; y < image->height; y++) {
            RwInt32 x = 0;
            if (image->width != 0) {
                RwUInt8* px = pixels + 2;
                do {
                    px[-2] = gammaTable[px[-2]];
                    px[-1] = gammaTable[px[-1]];
                    px[0]  = gammaTable[px[0]];
                    px    += 4; // px[1] = px[1]; — альфа без изменений
                    x++;
                } while (x < image->width);
            }
            pixels += image->stride;
        }
    }

    image->flags |= rwIMAGEGAMMACORRECTED;
    return image;
}


RwBool RwPalQuantInit(RwPalQuant *pq)
{
    RwUInt32 depth = g_rwPalQuantMaxDepth;
    RwUInt32 tableSize = 1 << (depth & 0x1F);

    RwUInt32 i = 0;
    if (tableSize != 0)
    {
        do
        {
            RwUInt32 spread = 0;
            if (depth != 0)
            {
                RwInt32 bitAcc = depth * 4;
                RwUInt32 j = 0;
                do
                {
                    bitAcc -= 4;
                    if ((i & (1 << (j & 0x1F))) != 0)
                        spread |= 1 << (bitAcc & 0x1F);
                    j++;
                } while (j < depth);
            }
            g_rwPalQuantBitTable[i] = spread;
            i++;
        } while (i < tableSize);
    }

    // Freelist of 0x5C-byte _rwPalQuantOctNodes: Leaf (0x1C) + Branch.dir[16] (0x40).
    // 0x801980 = _rwFreeListCreate per rw_addr_map (binary ABI: 4 args, no file/line);
    // raw-cast instead of the repo wrapper to match the 4-arg call in the decompile.
    pq->cubefreelist = ((RwFreeList * (__cdecl *)(RwInt32, RwInt32, RwInt32, RwUInt32))0x801980)
                       (0x5C, 0x400, 4, 0x30411);
    pq->root = (_rwPalQuantOctNode *)RwGetGlobalsFnViewGta()->memoryAlloc(pq->cubefreelist, 0x30411);
    {
        _rwPalQuantOctNode **slot = &pq->root->Branch.dir[0];
        RwInt32 n = 0x10;
        do
        {
            *slot = NULL;
            slot++;
            n--;
        } while (n != 0);
    }
    return TRUE;
}


void RwPalQuantTerm(RwPalQuant *pq)
{
    _rwPalQuantOctNode *root = pq->root;
    if (root != NULL)
    {
        if (0 < (RwInt32)g_rwPalQuantMaxDepth)
        {
            RwUInt32 depth = g_rwPalQuantMaxDepth - 1;
            _rwPalQuantOctNode **slot = &root->Branch.dir[0];
            RwInt32 n = 0x10;
            do
            {
                ((void (__cdecl *)(RwPalQuant *, void *, RwUInt32))0x80C5A0)(pq, *slot, depth);
                slot++;
                n--;
            } while (n != 0);
        }
        RwGetGlobalsFnViewGta()->memoryFree(pq->cubefreelist, root);
    }
    pq->root = NULL;
    ((void (__cdecl *)(void))0x801B80)();
}


void RwPalQuantAddImage(RwPalQuant *pq, RwImage *img, RwReal weight)
{
    RwInt32 stride = img->stride;
    RwRGBA *palette = img->palette;
    RwUInt8 *pixels = img->cpPixels;
    RwInt32 depth = img->depth;

    if (depth == 4 || depth == 8)
    {
        RwInt32 y = img->height;
        for (; y != 0; y--)
        {
            RwUInt8 *px = pixels;
            RwInt32 x = img->width;
            RwUInt32 treeDepth = g_rwPalQuantMaxDepth;
            for (; x != 0; x--)
            {
                RwRGBA *col = (RwRGBA *)((RwUInt8 *)palette + (RwUInt32)*px * 4);
                RwUInt32 shift = 8 - treeDepth;
                RwUInt32 key = (((g_rwPalQuantBitTable[col->red >> shift] * 2 |
                                  g_rwPalQuantBitTable[col->green >> shift]) * 2 |
                                 g_rwPalQuantBitTable[col->blue >> shift]) * 2 |
                                g_rwPalQuantBitTable[col->alpha >> shift]);

                void *leaf = pq->root; /* treeDepth == 0: the root itself is the leaf */
                if (treeDepth != 0)
                {
                    _rwPalQuantOctNode **slot = &pq->root->Branch.dir[key & 0xF];
                    if (*slot == NULL)
                    {
                        _rwPalQuantOctNode *node =
                            (_rwPalQuantOctNode *)RwGetGlobalsFnViewGta()->memoryAlloc(pq->cubefreelist, 0x30411);
                        *slot = node;
                        for (RwInt32 i = 0; i < 0x10; i++)
                            node->Branch.dir[i] = NULL;
                        if (treeDepth == 1)
                        {
                            /* last level: node is a leaf — clear its stat area */
                            node->Leaf.weight = 0.0f;
                            node->Leaf.ac.red = node->Leaf.ac.green =
                            node->Leaf.ac.blue = node->Leaf.ac.alpha = 0.0f;
                            node->Leaf.var = 0.0f;
                            node->Leaf.palIndex = 0;
                        }
                    }
                    leaf = ((void * (__cdecl *)(RwPalQuant *, void *, RwUInt32, RwUInt32))0x80AED0)
                           (pq, *slot, key >> 4, treeDepth - 1);
                }
                ((void (__cdecl *)(void *, void *, RwReal))0x80AD90)(leaf, col, weight);

                px++;
                treeDepth = g_rwPalQuantMaxDepth;
            }
            pixels += stride;
        }
    }
    else if (depth == 0x20 && img->height != 0)
    {
        RwUInt8 *row = pixels + 2;
        RwInt32 y = img->height;
        do
        {
            RwUInt8 *p = row;
            RwInt32 x = img->width;
            RwUInt32 treeDepth = g_rwPalQuantMaxDepth;
            for (; x != 0; x--)
            {
                RwUInt32 shift = 8 - treeDepth;
                RwUInt32 key = (((g_rwPalQuantBitTable[p[-2] >> shift] * 2 |
                                  g_rwPalQuantBitTable[p[-1] >> shift]) * 2 |
                                 g_rwPalQuantBitTable[p[0] >> shift]) * 2 |
                                g_rwPalQuantBitTable[p[1] >> shift]);

                void *leaf = pq->root;
                if (treeDepth != 0)
                {
                    _rwPalQuantOctNode **slot = &pq->root->Branch.dir[key & 0xF];
                    if (*slot == NULL)
                    {
                        _rwPalQuantOctNode *node =
                            (_rwPalQuantOctNode *)RwGetGlobalsFnViewGta()->memoryAlloc(pq->cubefreelist, 0x30411);
                        *slot = node;
                        for (RwInt32 i = 0; i < 0x10; i++)
                            node->Branch.dir[i] = NULL;
                        if (treeDepth == 1)
                        {
                            node->Leaf.weight = 0.0f;
                            node->Leaf.ac.red = node->Leaf.ac.green =
                            node->Leaf.ac.blue = node->Leaf.ac.alpha = 0.0f;
                            node->Leaf.var = 0.0f;
                            node->Leaf.palIndex = 0;
                        }
                    }
                    leaf = ((void * (__cdecl *)(RwPalQuant *, void *, RwUInt32, RwUInt32))0x80AED0)
                           (pq, *slot, key >> 4, treeDepth - 1);
                }
                ((void (__cdecl *)(void *, void *, RwReal))0x80AD90)(leaf, p - 2, weight);

                p += 4;
                treeDepth = g_rwPalQuantMaxDepth;
            }
            row += stride;
            y--;
        } while (y != 0);
    }
}


RwInt32 RwPalQuantResolvePalette(RwRGBA *palette, RwInt32 maxcols, RwPalQuant *pq)
{
    RwUInt32 depth = g_rwPalQuantMaxDepth;
    RwUInt8 walkRGBA[4];
    walkRGBA[0] = 0;
    walkRGBA[1] = 0;
    walkRGBA[2] = 0;
    walkRGBA[3] = 0;
    ((void (__cdecl *)(void *, RwUInt8 *, RwUInt32))0x80BCA0)(pq->root, walkRGBA, depth);

    {
        RwUInt8 *px = (RwUInt8 *)palette;
        RwInt32 n = maxcols;
        for (; n > 0; n--)
        {
            px[0] = 0;
            px[1] = 0;
            px[2] = 0;
            px[3] = 0;
            px += 4;
        }
    }

    RwInt32 numLeaves = 0;
    if (pq->root != NULL)
    {
        if (depth < 1)
        {
            numLeaves = 1;
        }
        else
        {
            RwUInt32 subDepth = depth - 1;
            _rwPalQuantOctNode **slot = &pq->root->Branch.dir[0];
            RwInt32 n = 0x10;
            do
            {
                numLeaves += ((RwInt32 (__cdecl *)(void *, RwUInt32))0x80BE60)(*slot, subDepth);
                slot++;
                n--;
            } while (n != 0);
        }
    }

    if (maxcols < numLeaves)
    {
        /* ---- median cut over Mcube/Mvv/Mvol ---- */
        pq->Mcube[0].col0[0] = 0;
        pq->Mcube[0].col0[1] = 0;
        pq->Mcube[0].col0[2] = 0;
        pq->Mcube[0].col0[3] = 0;
        pq->Mcube[0].col1[0] = 1 << (depth & 0x1F);
        pq->Mcube[0].col1[1] = 1 << (depth & 0x1F);
        pq->Mcube[0].col1[2] = 1 << (depth & 0x1F);
        pq->Mcube[0].col1[3] = 1 << (depth & 0x1F);
        pq->Mvol[0].palIndex = 0;
        pq->Mvol[0].weight = 0.0f;
        pq->Mvol[0].ac.red = pq->Mvol[0].ac.green =
        pq->Mvol[0].ac.blue = pq->Mvol[0].ac.alpha = 0.0f;
        pq->Mvol[0].var = 0.0f;

        {
            RwReal minmax[4];
            minmax[0] = 0.0f;
            minmax[1] = 0.0f;
            minmax[2] = 0.0f;
            minmax[3] = 0.0f;
            ((void (__cdecl *)(void *, RwReal *, RwUInt32, RwPalQuant *, _rwPalQuantLeafNode *))0x80B730)
                (pq->root, minmax, depth, pq, &pq->Mvol[0]);
        }
        pq->Mvv[0] = pq->Mvol[0].var;

        RwInt32 ncolors = 1;
        while (ncolors < maxcols)
        {
            /* pick the box with the largest score above 0.0f (DAT_00858b50 == 0.0f) */
            RwInt32 maxIdx = -1;
            {
                RwReal best = 0.0f; /* DAT_00858b50 */
                for (RwInt32 i = 0; i < ncolors; i++)
                {
                    if (best < pq->Mvv[i])
                    {
                        best = pq->Mvv[i];
                        maxIdx = i;
                    }
                }
            }
            if (maxIdx == -1)
                break;

            _rwPalQuantRGBABox *box = &pq->Mcube[maxIdx];
            _rwPalQuantLeafNode *leaf = &pq->Mvol[maxIdx];

            RwUInt8 splits[16];
            RwReal axisVar[4];
            axisVar[0] = ((RwReal (__cdecl *)(void *, _rwPalQuantRGBABox *, RwInt32, RwUInt8 *, _rwPalQuantLeafNode *))0x80BA30)
                         (pq->root, box, 0, splits, leaf);
            axisVar[1] = ((RwReal (__cdecl *)(void *, _rwPalQuantRGBABox *, RwInt32, RwUInt8 *, _rwPalQuantLeafNode *))0x80BA30)
                         (pq->root, box, 1, splits, leaf);
            axisVar[2] = ((RwReal (__cdecl *)(void *, _rwPalQuantRGBABox *, RwInt32, RwUInt8 *, _rwPalQuantLeafNode *))0x80BA30)
                         (pq->root, box, 2, splits, leaf);
            axisVar[3] = ((RwReal (__cdecl *)(void *, _rwPalQuantRGBABox *, RwInt32, RwUInt8 *, _rwPalQuantLeafNode *))0x80BA30)
                         (pq->root, box, 3, splits, leaf);

            /* pick the axis with the smallest split variance */
            RwInt32 minAxis = 0;
            {
                RwReal best = axisVar[0];
                for (RwInt32 i = 1; i < 4; i++)
                {
                    if (axisVar[i] < best)
                    {
                        best = axisVar[i];
                        minAxis = i;
                    }
                }
            }

            if (leaf->var <= axisVar[minAxis])
            {
                /* splitting gains nothing: retire this box */
                pq->Mvv[maxIdx] = 0;
            }
            else
            {
                /* split the box at the plane found by 0x80BA30 */
                pq->Mcube[ncolors] = *box;
                RwUInt8 splitByte = splits[minAxis * 4];
                pq->Mcube[ncolors].col0[minAxis] = splitByte;
                box->col1[minAxis] = splitByte;

                leaf->palIndex = 0;
                leaf->weight = 0.0f;
                leaf->ac.red = leaf->ac.green = leaf->ac.blue = leaf->ac.alpha = 0.0f;
                leaf->var = 0.0f;
                {
                    RwReal minmax[4];
                    minmax[0] = 0.0f; minmax[1] = 0.0f; minmax[2] = 0.0f; minmax[3] = 0.0f;
                    ((void (__cdecl *)(void *, RwReal *, RwUInt32, RwPalQuant *, _rwPalQuantLeafNode *))0x80B730)
                        (pq->root, minmax, depth, pq, leaf);
                }

                _rwPalQuantLeafNode *newLeaf = &pq->Mvol[ncolors];
                newLeaf->palIndex = 0;
                newLeaf->weight = 0.0f;
                newLeaf->ac.red = newLeaf->ac.green = newLeaf->ac.blue = newLeaf->ac.alpha = 0.0f;
                newLeaf->var = 0.0f;
                {
                    RwReal minmax[4];
                    minmax[0] = 0.0f; minmax[1] = 0.0f; minmax[2] = 0.0f; minmax[3] = 0.0f;
                    ((void (__cdecl *)(void *, RwReal *, RwUInt32, RwPalQuant *, _rwPalQuantLeafNode *))0x80B730)
                        (pq->root, minmax, depth, pq, newLeaf);
                }

                pq->Mvv[maxIdx]  = leaf->var;
                pq->Mvv[ncolors] = newLeaf->var;
                ncolors++;
            }
        }

        /* one palette entry per box */
        if (0 < maxcols)
        {
            RwInt32 idx = 0;
            do
            {
                RwRGBAReal col;
                ((void (__cdecl *)(void *, RwRGBAReal *, RwUInt32, _rwPalQuantRGBABox *, RwInt32))0x80B5D0)
                    (pq->root, &col, depth, &pq->Mcube[idx], idx);
                ((void (__cdecl *)(RwRGBA *, _rwPalQuantLeafNode *))0x80B490)(&palette[idx], &pq->Mvol[idx]);
                idx++;
            } while (idx < maxcols);
        }
        return maxcols;
    }
    else
    {
        /* ---- tree already small enough: assign leaves directly ---- */
        if (pq->root != NULL)
        {
            if (depth < 1)
            {
                ((void (__cdecl *)(RwRGBA *, _rwPalQuantLeafNode *))0x80B490)
                    (palette, (_rwPalQuantLeafNode *)pq->root);
                pq->root->Leaf.palIndex = 0;
                return numLeaves;
            }

            RwInt32 count = 0;
            RwUInt32 subDepth = depth - 1;
            _rwPalQuantOctNode **slot = &pq->root->Branch.dir[0];
            RwInt32 n = 0x10;
            do
            {
                count = ((RwInt32 (__cdecl *)(void *, RwRGBA *, RwInt32, RwUInt32))0x80BEB0)
                        (*slot, palette, count, subDepth);
                slot++;
                n--;
            } while (n != 0);
            return numLeaves;
        }
        return numLeaves;
    }
}


void RwPalQuantMatchImage(RwUInt8 *dstpixels, RwInt32 dststride, RwInt32 dstdepth,
                          RwBool dstPacked, RwPalQuant *pq, RwImage *img)
{
    RwInt32 srcStride = img->stride;
    RwUInt8 *srcPixels = img->cpPixels;

    if (dstdepth == 4 && dstPacked == 1)
    {
        RwInt32 srcDepth = img->depth;
        if (srcDepth == 4 || srcDepth == 8)
        {
            RwRGBA *palette = img->palette;
            RwInt32 y = img->height;
            if (y != 0)
            {
                do
                {
                    RwUInt8 *dst = dstpixels;
                    RwInt32 x = 0;
                    RwUInt32 width = (RwUInt32)img->width;
                    RwUInt8 *src = srcPixels;
                    if (width != 0)
                    {
                        do
                        {
                            RwUInt8 index = *src;
                            src++;
                            RwRGBA *col = (RwRGBA *)((RwUInt8 *)palette + (RwUInt32)index * 4);
                            RwUInt32 shift = 8 - (RwUInt8)g_rwPalQuantMaxDepth;
                            RwUInt32 key = (((g_rwPalQuantBitTable[col->green >> shift] |
                                              g_rwPalQuantBitTable[col->red >> shift] * 2) * 2 |
                                            g_rwPalQuantBitTable[col->blue >> shift]) * 2 |
                                           g_rwPalQuantBitTable[col->alpha >> shift]);
                            if (g_rwPalQuantMaxDepth == 0)
                                index = pq->root->Leaf.palIndex;
                            else
                                index = ((RwUInt8 (__cdecl *)(void *, RwUInt32, RwUInt32))0x80C430)
                                        (pq->root->Branch.dir[key & 0xF], key >> 4, g_rwPalQuantMaxDepth - 1);

                            if ((x & 1) == 0)
                                *dst = *dst ^ ((*dst ^ index) & 0xF);
                            else
                            {
                                *dst = (*dst & 0xF) | (index << 4);
                                dst++;
                            }
                            x++;
                        } while ((RwUInt32)x < width);
                    }
                    dstpixels += dststride;
                    srcPixels += srcStride;
                    y--;
                } while (y != 0);
            }
        }
        else if (srcDepth == 0x20)
        {
            RwInt32 y = img->height;
            if (y != 0)
            {
                do
                {
                    RwUInt32 width = (RwUInt32)img->width;
                    RwUInt8 *dst = dstpixels;
                    RwInt32 x = 0;
                    RwUInt8 *src = srcPixels;
                    if (width != 0)
                    {
                        do
                        {
                            RwUInt32 shift = 8 - (RwUInt8)g_rwPalQuantMaxDepth;
                            RwUInt32 key = (((g_rwPalQuantBitTable[src[0] >> shift] * 2 |
                                             g_rwPalQuantBitTable[src[1] >> shift]) * 2 |
                                            g_rwPalQuantBitTable[src[2] >> shift]) * 2 |
                                           g_rwPalQuantBitTable[src[3] >> shift]);
                            RwUInt8 index;
                            if (g_rwPalQuantMaxDepth == 0)
                                index = pq->root->Leaf.palIndex;
                            else
                                index = ((RwUInt8 (__cdecl *)(void *, RwUInt32, RwUInt32))0x80C430)
                                        (pq->root->Branch.dir[key & 0xF], key >> 4, g_rwPalQuantMaxDepth - 1);
                            if ((x & 1) == 0)
                                *dst = *dst ^ ((*dst ^ index) & 0xF);
                            else
                            {
                                *dst = (*dst & 0xF) | (index << 4);
                                dst++;
                            }
                            x++;
                            src += 4;
                        } while ((RwUInt32)x < width);
                    }
                    dstpixels += dststride;
                    srcPixels += srcStride;
                    y--;
                } while (y != 0);
            }
        }
    }
    else
    {
        RwInt32 srcDepth = img->depth;
        if (srcDepth == 4 || srcDepth == 8)
        {
            RwRGBA *palette = img->palette;
            RwInt32 y = img->height;
            for (; y != 0; y--)
            {
                RwUInt8 *src = srcPixels;
                RwUInt8 *dst = dstpixels;
                for (RwInt32 x = img->width; x != 0; x--)
                {
                    RwUInt8 index = *src;
                    src++;
                    RwRGBA *col = (RwRGBA *)((RwUInt8 *)palette + (RwUInt32)index * 4);
                    RwUInt32 shift = 8 - (RwUInt8)g_rwPalQuantMaxDepth;
                    RwUInt32 key = (((g_rwPalQuantBitTable[col->green >> shift] |
                                      g_rwPalQuantBitTable[col->red >> shift] * 2) * 2 |
                                    g_rwPalQuantBitTable[col->blue >> shift]) * 2 |
                                   g_rwPalQuantBitTable[col->alpha >> shift]);
                    if (g_rwPalQuantMaxDepth == 0)
                        index = pq->root->Leaf.palIndex;
                    else
                        index = ((RwUInt8 (__cdecl *)(void *, RwUInt32, RwUInt32))0x80C430)
                                (pq->root->Branch.dir[key & 0xF], key >> 4, g_rwPalQuantMaxDepth - 1);
                    *dst = index;
                    dst++;
                }
                dstpixels += dststride;
                srcPixels += srcStride;
            }
        }
        else if (srcDepth == 0x20)
        {
            RwInt32 y = img->height;
            if (y != 0)
            {
                do
                {
                    RwUInt8 *src = srcPixels;
                    RwUInt8 *dst = dstpixels;
                    for (RwInt32 x = img->width; x != 0; x--)
                    {
                        RwUInt32 shift = 8 - (RwUInt8)g_rwPalQuantMaxDepth;
                        RwUInt32 key = (((g_rwPalQuantBitTable[src[0] >> shift] * 2 |
                                         g_rwPalQuantBitTable[src[1] >> shift]) * 2 |
                                        g_rwPalQuantBitTable[src[2] >> shift]) * 2 |
                                       g_rwPalQuantBitTable[src[3] >> shift]);
                        RwUInt8 index;
                        if (g_rwPalQuantMaxDepth == 0)
                            index = pq->root->Leaf.palIndex;
                        else
                            index = ((RwUInt8 (__cdecl *)(void *, RwUInt32, RwUInt32))0x80C430)
                                    (pq->root->Branch.dir[key & 0xF], key >> 4, g_rwPalQuantMaxDepth - 1);
                        *dst = index;
                        dst++;
                        src += 4;
                    }
                    dstpixels += dststride;
                    srcPixels += srcStride;
                    y--;
                } while (y != 0);
            }
        }
    }
}


RwBool RwTextureSetMipmapping(RwBool enable) {
    RwGetTextureGlobalsGta()->mipmapping = enable;
    return TRUE;
}


RwBool RwTextureGetMipmapping() {
    return RwGetTextureGlobalsGta()->mipmapping;
}


RwBool RwTextureSetAutoMipmapping(RwBool enable) {
    RwGetTextureGlobalsGta()->automipmapping = enable;
    return TRUE;
}


RwBool RwTextureGetAutoMipmapping() {
    return RwGetTextureGlobalsGta()->automipmapping;
}


RwBool RwTextureSetMipmapGenerationCallBack(RwTextureCallBackMipmapGeneration callback) {
    *reinterpret_cast<RwTextureCallBackMipmapGeneration*>(RwTextureModuleGlobalsRW4() + 0x2C) = callback;
    return TRUE;
}


RwTextureCallBackMipmapGeneration RwTextureGetMipmapGenerationCallBack() {
    return *reinterpret_cast<RwTextureCallBackMipmapGeneration*>(RwTextureModuleGlobalsRW4() + 0x2C);
}


RwBool RwTextureSetMipmapNameCallBack(RwTextureCallBackMipmapName callback) {
    *reinterpret_cast<RwTextureCallBackMipmapName*>(RwTextureModuleGlobalsRW4() + 0x30) = callback;
    return TRUE;
}


RwTextureCallBackMipmapName RwTextureGetMipmapNameCallBack() {
    return *reinterpret_cast<RwTextureCallBackMipmapName*>(RwTextureModuleGlobalsRW4() + 0x30);
}


RwBool RwTextureGenerateMipmapName(RwChar* name, RwChar* maskName, RwUInt8 mipLevel, RwInt32 format) {
    RwTextureCallBackMipmapName callBack = RwGetTextureGlobalsGta()->mipmapNameCallBack;
    if (callBack != nullptr) {
        return callBack(name, maskName, mipLevel, format);
    }
    return FALSE;
}


RwBool RwTextureRasterGenerateMipmaps(RwRaster* raster, RwImage* image) {
    RwTextureCallBackMipmapGeneration callback =
        *reinterpret_cast<RwTextureCallBackMipmapGeneration*>(RwTextureModuleGlobalsRW4() + 0x2C);
    return callback(raster, image) != NULL;
}


RwBool _rwTextureSetAutoMipMapState(RwBool enable) {
    RwTextureGlobalsGta* tg = RwGetTextureGlobalsGta();
    tg->mipmapping = enable;
    tg->automipmapping = enable;
    return TRUE;
}


RwBool _rwTextureGetAutoMipMapState() {
    return RwGetTextureGlobalsGta()->automipmapping;
}


RwTextureCallBackRead RwTextureGetReadCallBack() {
    return RwGetTextureGlobalsGta()->readCallBack;
}


RwBool RwTextureSetReadCallBack(RwTextureCallBackRead callBack) {
    RwGetTextureGlobalsGta()->readCallBack = callBack;
    return TRUE;
}


RwTextureCallBackFind RwTextureGetFindCallBack() {
    return RwGetTextureGlobalsGta()->findCallBack;
}


RwBool RwTextureSetFindCallBack(RwTextureCallBackFind callBack) {
    RwGetTextureGlobalsGta()->findCallBack = callBack;
    return TRUE;
}


RwTexture* RwTextureSetName(RwTexture* texture, const RwChar* name) {
    RwGlobalsRwcoreGta* engine = RwGetGlobalsRwcoreGta();
    engine->stringFuncs.vecStrncpy(texture->name, name, rwTEXTUREBASENAMELENGTH);
    if (engine->stringFuncs.vecStrlen(name) > rwTEXTUREBASENAMELENGTH - 1) {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwUInt32, const RwChar*, RwUInt32, RwUInt32, RwChar))0x8088D0)(0x8000001Eu, name, rwTEXTUREBASENAMELENGTH, rwTEXTUREBASENAMELENGTH - 1, name[rwTEXTUREBASENAMELENGTH - 1]);
        RwErrorSet(&rwError);
        texture->name[rwTEXTUREBASENAMELENGTH - 1] = '\0';
    }
    return texture;
}


RwTexture* RwTextureSetMaskName(RwTexture* texture, const RwChar* maskName) {
    RwGlobalsRwcoreGta* engine = RwGetGlobalsRwcoreGta();
    engine->stringFuncs.vecStrncpy(texture->mask, maskName, rwTEXTUREBASENAMELENGTH);
    if (engine->stringFuncs.vecStrlen(maskName) > rwTEXTUREBASENAMELENGTH - 1) {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwUInt32, const RwChar*, RwUInt32, RwUInt32, RwChar))0x8088D0)(0x8000001Eu, maskName, rwTEXTUREBASENAMELENGTH, rwTEXTUREBASENAMELENGTH - 1, maskName[rwTEXTUREBASENAMELENGTH - 1]);
        RwErrorSet(&rwError);
        texture->mask[rwTEXTUREBASENAMELENGTH - 1] = '\0';
    }
    return texture;
}


void RwTexDictionarySetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    StaticRef<RwInt32>(0x8E2404) = blockSize;
    StaticRef<RwInt32>(0x8E2408) = numBlocksToPrealloc;
}


RwTexDictionary* RwTexDictionaryCreate() {
    RwTextureGlobalsGta* tg = RwGetTextureGlobalsGta();
    RwTexDictionary* dict = static_cast<RwTexDictionary*>(RwGetGlobalsRwcoreGta()->memoryAlloc(tg->texDictFreeList, 0x30016));
    if (dict == nullptr) {
        return nullptr;
    }
    dict->object.type          = 6;  // binary constant (decompile: *dict = 6)
    dict->object.subType       = 0;
    dict->object.flags         = 0;
    dict->object.privateFlags  = 0;
    dict->object.parent        = nullptr;

    // insert dict->lInInstance after the global dictList sentinel
    dict->lInInstance.next = tg->dictList.link.next;
    dict->lInInstance.prev = &tg->dictList.link;
    tg->dictList.link.next->prev = &dict->lInInstance;
    tg->dictList.link.next = &dict->lInInstance;

    // empty texture ring: texturesInDict -> self
    dict->texturesInDict.link.prev = &dict->texturesInDict.link;
    dict->texturesInDict.link.next = &dict->texturesInDict.link;
    NOTSA_LOG_DEBUG("[TXDCREATE] dict={} globals={:#x} texDictFreeList={:#x}", (void*)dict,
        (uintptr_t)tg, (uintptr_t)tg->texDictFreeList);

    _rwPluginRegistryInitObject(&rwTexDictPluginRegistry, dict);
    return dict;
}


RwBool RwTexDictionaryDestroy(RwTexDictionary* dict) {
    RwTextureGlobalsGta* tg = RwGetTextureGlobalsGta();
    if (tg->currentTexDict == dict) {
        tg->currentTexDict = nullptr;
    }
    RwBool ok = TRUE;
    RwLLLink* link = dict->texturesInDict.link.next;
    do {
        if (link == &dict->texturesInDict.link) {
            break;
        }
        RwLLLink* next = link->next;  // pre-fetched: RwTextureDestroy unlinks the current node
        RwTexture* texture = reinterpret_cast<RwTexture*>(reinterpret_cast<RwUInt8*>(link) - offsetof(RwTexture, lInDictionary));
        ok = RwTextureDestroy(texture);
        link = next;
    } while (ok);
    _rwPluginRegistryDeInitObject(&rwTexDictPluginRegistry, dict);
    dict->lInInstance.prev->next = dict->lInInstance.next;
    dict->lInInstance.next->prev = dict->lInInstance.prev;
    RwGetGlobalsRwcoreGta()->memoryFree(tg->texDictFreeList, dict);
    return TRUE;
}


RwTexture* RwTextureCreate(RwRaster* raster) {
    RwTextureGlobalsGta* tg = RwGetTextureGlobalsGta();
    RwTexture* texture = static_cast<RwTexture*>(RwGetGlobalsRwcoreGta()->memoryAlloc(tg->textureFreeList, 0x30006));
    if (texture != nullptr) {
        texture->filterAddressing = 0;
        texture->raster = raster;
        *(RwUInt8*)(reinterpret_cast<RwUInt8*>(texture) + 0x51) = 0x11;  // addressing U=1|V=1 (rwTEXTUREADDRESSWRAP)
        texture->dict = nullptr;
        texture->name[0] = '\0';
        texture->mask[0] = '\0';
        texture->refCount = 1;
        *(RwUInt8*)(&texture->filterAddressing) = 1;                     // filter = rwFILTERNEAREST
        _rwPluginRegistryInitObject(&rwTexturePluginRegistry, texture);
    }
    return texture;
}


RwTexture* RwTextureRead(const char* name, const char* maskName) {
    auto* tex = plugin::CallAndReturn<RwTexture*, 0x7F3AC0, const char*, const char*>(name, maskName);
    if (!tex)
        NOTSA_LOG_ERR("[TEXREAD] '{}' + '{}' -> NULL", name ? name : "?", maskName ? maskName : "-");
    return tex;
}

RwBool RwTextureDestroy(RwTexture* texture) {
    const RwInt32 oldRefCount = texture->refCount;
    texture->refCount = oldRefCount - 1;
    if (texture->refCount < 1) {
        texture->refCount = oldRefCount;
        _rwPluginRegistryDeInitObject(&rwTexturePluginRegistry, texture);
        if (texture->dict != nullptr) {
            texture->lInDictionary.prev->next = texture->lInDictionary.next;
            texture->lInDictionary.next->prev = texture->lInDictionary.prev;
        }
        if (texture->raster != nullptr) {
            RwRasterDestroy(texture->raster);
            texture->raster = nullptr;
        }
        texture->refCount = texture->refCount - 1;
        RwGetGlobalsRwcoreGta()->memoryFree(RwGetTextureGlobalsGta()->textureFreeList, texture);
    }
    return TRUE;
}


RwTexture* RwTextureSetRaster(RwTexture* texture, RwRaster* raster) {
    if (raster != nullptr) {
        RwGlobalsRwcoreGta* engine = RwGetGlobalsRwcoreGta();
        const RwBool ok = ((RwBool(__cdecl*)(RwTexture*, RwRaster*, RwInt32))((void**)engine->textureRasterFnTable)[0])(texture, raster, 0);
        return ok ? texture : nullptr;
    }
    texture->raster = nullptr;
    return texture;
}


RwTexture* RwTexDictionaryAddTexture(RwTexDictionary* dict, RwTexture* texture) {
    if (texture->dict != nullptr) {
        texture->lInDictionary.prev->next = texture->lInDictionary.next;
        texture->lInDictionary.next->prev = texture->lInDictionary.prev;
    }
    texture->dict = dict;
    RwLLLink* link = &texture->lInDictionary;
    RwLLLink* first = dict->texturesInDict.link.next;
    link->prev = &dict->texturesInDict.link;
    link->next = first;
    first->prev = link;
    dict->texturesInDict.link.next = link;
    return texture;
}


RwTexture* RwTexDictionaryRemoveTexture(RwTexture* texture) {
    if (texture->dict != nullptr) {
        texture->dict = nullptr;
        texture->lInDictionary.prev->next = texture->lInDictionary.next;
        texture->lInDictionary.next->prev = texture->lInDictionary.prev;
    }
    return texture;
}


RwTexture* RwTexDictionaryFindNamedTexture(RwTexDictionary* dict, const RwChar* name) {
    RwLLLink* link = dict->texturesInDict.link.next;
    do {
        if (link == &dict->texturesInDict.link) {
            return nullptr;
        }
        RwTexture* texture = reinterpret_cast<RwTexture*>(reinterpret_cast<RwUInt8*>(link) - offsetof(RwTexture, lInDictionary));
        const RwChar* texName = reinterpret_cast<const RwChar*>(reinterpret_cast<RwUInt8*>(link) + offsetof(RwTexture, name));
        if (texName != nullptr) {
            RwChar texChar = *texName;
            const RwChar* namePtr = name;
            RwBool mismatch = FALSE;
            while (texChar != '\0' && *namePtr != '\0') {
                RwChar nameChar = *namePtr;
                if (texChar > '`' && texChar < '{') {
                    texChar = texChar - 0x20;   // 'a'..'z' -> 'A'..'Z'
                }
                if (nameChar > '`' && nameChar < '{') {
                    nameChar = nameChar - 0x20;
                }
                if (texChar != nameChar) {
                    mismatch = TRUE;
                    break;
                }
                texChar = texName[1];           // pre-load next tex char, then advance both
                texName++;
                namePtr++;
            }
            if (!mismatch && *texName == *namePtr) {
                return texture;
            }
        }
        link = link->next;
    } while (true);
}


RwTexDictionary* RwTexDictionaryGetCurrent() {
    return RwGetTextureGlobalsGta()->currentTexDict;
}


RwTexDictionary* RwTexDictionarySetCurrent(RwTexDictionary* dict) {
    RwGetTextureGlobalsGta()->currentTexDict = dict;
    return dict;
}


const RwTexDictionary* RwTexDictionaryForAllTextures(const RwTexDictionary* dict, RwTextureCallBack fpCallBack, void* data) {
    RwTexDictionary* d = const_cast<RwTexDictionary*>(dict);  // binary treats it as mutable; no writes happen here
    RwLLLink* link = d->texturesInDict.link.next;
    if (link == &d->texturesInDict.link) {
        return dict;
    }
    do {
        RwLLLink* next = link->next;
        RwTexture* texture = reinterpret_cast<RwTexture*>(reinterpret_cast<RwUInt8*>(link) - offsetof(RwTexture, lInDictionary));
        if (fpCallBack(texture, data) == nullptr) {
            return dict;
        }
        link = next;
    } while (link != &d->texturesInDict.link);
    return dict;
}


RwBool RwTexDictionaryForAllTexDictionaries(RwTexDictionaryCallBack fpCallBack, void* data) {
    RwTextureGlobalsGta* tg = RwGetTextureGlobalsGta();
    RwLLLink* head = &tg->dictList.link;
    RwLLLink* link = head->next;
    do {
        if (link == head) {
            return TRUE;
        }
        RwLLLink* next = link->next;
        RwTexDictionary* dict = reinterpret_cast<RwTexDictionary*>(reinterpret_cast<RwUInt8*>(link) - offsetof(RwTexDictionary, lInInstance));
        if (fpCallBack(dict, data) == nullptr) {
            break;
        }
        link = next;
    } while (true);
    return TRUE;
}


RwInt32 RwTextureRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return _rwPluginRegistryAddPlugin(&rwTexturePluginRegistry, size, pluginID, constructCB, destructCB, copyCB);
}


RwInt32 RwTexDictionaryRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return _rwPluginRegistryAddPlugin(&StaticRef<RwPluginRegistry>(0x8E23E4), size, pluginID, constructCB, destructCB, copyCB);
}


RwInt32 RwTextureGetPluginOffset(RwUInt32 pluginID) {
    return _rwPluginRegistryGetPluginOffset(&StaticRef<RwPluginRegistry>(0x8E23CC), pluginID);
}


RwInt32 RwTexDictionaryGetPluginOffset(RwUInt32 pluginID) {
    return _rwPluginRegistryGetPluginOffset(&StaticRef<RwPluginRegistry>(0x8E23E4), pluginID);
}


RwBool RwTextureValidatePlugins(const RwTexture* texture) {
    return TRUE;
}


RwBool RwTexDictionaryValidatePlugins(const RwTexDictionary* dict) {
    return TRUE;
}


rwIm3DPool* _rwIm3DGetPool() {
    return reinterpret_cast<rwIm3DPool*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + *reinterpret_cast<RwUInt32*>(0xC9C078) + 0x38);
}


void* RwIm3DTransform(RwIm3DVertex* pVerts, RwUInt32 numVerts, RwMatrix* ltm, RwUInt32 flags) {
    RwUInt8* im3d = reinterpret_cast<RwUInt8*>(RwEngineInstance) + *reinterpret_cast<RwUInt32*>(0xC9C078);
    if (numVerts < 0x10001) {
        *reinterpret_cast<RwUInt16*>(im3d + 0x38) = static_cast<RwUInt16>(numVerts);
        *reinterpret_cast<void**>(im3d + 0x3C)    = pVerts;
        *reinterpret_cast<RwUInt32*>(im3d + 0x40) = 0x24;   // sizeof(RxObjSpace3DLitVertex)
        *reinterpret_cast<RwUInt32*>(im3d + 0x44) = flags | (rwIM3D_VERTEXXYZ | rwIM3D_VERTEXRGBA);
        *reinterpret_cast<RwMatrix**>(im3d + 0x48) = ltm;
        RxPipeline* transformPipeline = *reinterpret_cast<RxPipeline**>(im3d + 0x00);
        if (RxPipelineExecute(transformPipeline, im3d + 0x38, TRUE) != nullptr) {
            return pVerts;
        }
    } else {
        RwError rwError;
        rwError.pluginID  = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x32);
        RwErrorSet(&rwError);
    }
    return nullptr;
}


RwBool RwIm3DEnd() {
    RwUInt8* im3d = reinterpret_cast<RwUInt8*>(RwEngineInstance) + *reinterpret_cast<RwUInt32*>(0xC9C078);
    if (*reinterpret_cast<void**>(im3d + 0x3C) == nullptr) {
        return FALSE;
    }
    RwUInt32* p = reinterpret_cast<RwUInt32*>(im3d + 0x38);
    for (RwInt32 i = 15; i != 0; i--) {
        *p++ = 0;
    }
    return TRUE;
}


RwBool RwIm3DRenderLine(RwInt32 vert1, RwInt32 vert2) {
    RwUInt8* im3d = reinterpret_cast<RwUInt8*>(RwEngineInstance) + *reinterpret_cast<RwUInt32*>(0xC9C078);
    RxHeap*  heap = RxHeapGetGlobalHeap();
    if (*reinterpret_cast<void**>(im3d + 0x3C) == nullptr) {   // no RwIm3DTransform in progress
        RwError rwError;
        rwError.pluginID  = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x23);
        RwErrorSet(&rwError);
        return FALSE;
    }
    RwUInt32* packet = reinterpret_cast<RwUInt32*>(im3d + 0x44);
    packet[8]  = *reinterpret_cast<RwUInt32*>(im3d + 0x10);   // +0x20 LINELIST render pipe
    packet[9]  = rwPRIMTYPELINELIST;                          // +0x24
    packet[10] = 0;                                           // +0x28
    packet[11] = 2;                                           // +0x2C numIndices
    RxVertexIndex* indices = reinterpret_cast<RxVertexIndex*>(RxHeapAlloc(heap, 4));   // 2 * u16
    packet[10] = reinterpret_cast<RwUInt32>(indices);
    indices[0] = static_cast<RxVertexIndex>(vert1);
    indices[1] = static_cast<RxVertexIndex>(vert2);
    const RwBool ok = RxPipelineExecute(*reinterpret_cast<RxPipeline**>(&packet[8]), packet, FALSE) != nullptr;
    RxHeapFree(heap, indices);
    return ok;
}


RwBool RwIm3DRenderTriangle(RwInt32 vert1, RwInt32 vert2, RwInt32 vert3) {
    RwUInt8* im3d = reinterpret_cast<RwUInt8*>(RwEngineInstance) + *reinterpret_cast<RwUInt32*>(0xC9C078);
    RxHeap*  heap = RxHeapGetGlobalHeap();
    if (*reinterpret_cast<void**>(im3d + 0x3C) == nullptr) {   // no RwIm3DTransform in progress
        RwError rwError;
        rwError.pluginID  = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x23);
        RwErrorSet(&rwError);
        return FALSE;
    }
    RwUInt32* packet = reinterpret_cast<RwUInt32*>(im3d + 0x44);
    packet[8]  = *reinterpret_cast<RwUInt32*>(im3d + 0x04);   // +0x20 TRILIST render pipe
    packet[9]  = rwPRIMTYPETRILIST;                           // +0x24
    packet[10] = 0;                                           // +0x28
    packet[11] = 3;                                           // +0x2C numIndices
    RxVertexIndex* indices = reinterpret_cast<RxVertexIndex*>(RxHeapAlloc(heap, 6));   // 3 * u16
    packet[10] = reinterpret_cast<RwUInt32>(indices);
    indices[0] = static_cast<RxVertexIndex>(vert1);
    indices[1] = static_cast<RxVertexIndex>(vert2);
    indices[2] = static_cast<RxVertexIndex>(vert3);
    const RwBool ok = RxPipelineExecute(*reinterpret_cast<RxPipeline**>(&packet[8]), packet, FALSE) != nullptr;
    RxHeapFree(heap, indices);
    return ok;
}


RwBool RwIm3DRenderIndexedPrimitive(RwPrimitiveType primType, RwImVertexIndex* indices, RwInt32 numIndices) {
    RwUInt8* im3d = reinterpret_cast<RwUInt8*>(RwEngineInstance) + *reinterpret_cast<RwUInt32*>(0xC9C078);
    if (*reinterpret_cast<void**>(im3d + 0x3C) == nullptr) {   // no RwIm3DTransform in progress
        RwError rwError;
        rwError.pluginID  = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x23);
        RwErrorSet(&rwError);
        return FALSE;
    }
    RwUInt32* packet = reinterpret_cast<RwUInt32*>(im3d + 0x44);
    packet[10] = reinterpret_cast<RwUInt32>(indices);   // +0x28 indices
    packet[8]  = 0;                                     // +0x20 pipeline, filled below
    packet[9]  = primType;                              // +0x24 primType
    packet[11] = numIndices;                            // +0x2C numIndices
    switch (primType) {
    case rwPRIMTYPELINELIST:
        packet[8] = *reinterpret_cast<RwUInt32*>(im3d + 0x10);
        // decompile: param_3 - (param_3 % 2) via idiv sign dance == clear low bit
        packet[11] = numIndices & ~static_cast<RwUInt32>(1);
        break;
    case rwPRIMTYPEPOLYLINE:
        packet[8] = *reinterpret_cast<RwUInt32*>(im3d + 0x14);
        break;
    case rwPRIMTYPETRILIST:
        packet[8] = *reinterpret_cast<RwUInt32*>(im3d + 0x04);
        packet[11] = numIndices - numIndices % 3;
        break;
    case rwPRIMTYPETRISTRIP:
        packet[8] = *reinterpret_cast<RwUInt32*>(im3d + 0x0C);
        break;
    case rwPRIMTYPETRIFAN:
        packet[8] = *reinterpret_cast<RwUInt32*>(im3d + 0x08);
        break;
    default:
        RwError rwError;
        rwError.pluginID  = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x25, primType);
        RwErrorSet(&rwError);
        return FALSE;
    }
    if (RxPipelineExecute(*reinterpret_cast<RxPipeline**>(&packet[8]), packet, FALSE) != nullptr) {
        return TRUE;
    }
    return FALSE;
}


RwBool RwIm3DRenderPrimitive(RwPrimitiveType primType) {
    RwUInt8* im3d = reinterpret_cast<RwUInt8*>(RwEngineInstance) + *reinterpret_cast<RwUInt32*>(0xC9C078);
    const void* objVerts = *reinterpret_cast<void* const*>(im3d + 0x3C);
    RxHeapGetGlobalHeap();   // decompile calls it before the check; result unused
    if (objVerts == nullptr) {
        RwError rwError;
        rwError.pluginID  = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x23);
        RwErrorSet(&rwError);
        return FALSE;
    }
    *reinterpret_cast<RwUInt32*>(im3d + 0x64) = 0;                       // pipeline slot
    RwUInt32* packet = reinterpret_cast<RwUInt32*>(im3d + 0x44);
    packet[9]  = primType;                                               // +0x24 primType
    packet[10] = 0;                                                      // +0x28 indices = NULL
    packet[11] = *reinterpret_cast<RwUInt16*>(im3d + 0x38);              // +0x2C numIndices = numVerts
    switch (primType) {
    case rwPRIMTYPELINELIST:
        packet[8] = *reinterpret_cast<RwUInt32*>(im3d + 0x10);
        break;
    case rwPRIMTYPEPOLYLINE:
        packet[8] = *reinterpret_cast<RwUInt32*>(im3d + 0x14);
        break;
    case rwPRIMTYPETRILIST:
        packet[8] = *reinterpret_cast<RwUInt32*>(im3d + 0x04);
        break;
    case rwPRIMTYPETRISTRIP:
        packet[8] = *reinterpret_cast<RwUInt32*>(im3d + 0x0C);
        break;
    case rwPRIMTYPETRIFAN:
        packet[8] = *reinterpret_cast<RwUInt32*>(im3d + 0x08);
        break;
    default:
        RwError rwError;
        rwError.pluginID  = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x25, primType);
        RwErrorSet(&rwError);
        return FALSE;
    }
    if (RxPipelineExecute(*reinterpret_cast<RxPipeline**>(&packet[8]), packet, FALSE) != nullptr) {
        return TRUE;
    }
    return FALSE;
}


RxPipeline* RwIm3DGetTransformPipeline() {
    return *reinterpret_cast<RxPipeline**>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + *reinterpret_cast<RwUInt32*>(0xC9C078));
}


RxPipeline* RwIm3DGetRenderPipeline(RwPrimitiveType primType) {
    RwUInt8* im3d = reinterpret_cast<RwUInt8*>(RwEngineInstance) + *reinterpret_cast<RwUInt32*>(0xC9C078);
    switch (primType) {
    case rwPRIMTYPELINELIST:
        return *reinterpret_cast<RxPipeline**>(im3d + 0x10);
    case rwPRIMTYPEPOLYLINE:
        return *reinterpret_cast<RxPipeline**>(im3d + 0x14);
    case rwPRIMTYPETRILIST:
        return *reinterpret_cast<RxPipeline**>(im3d + 0x04);
    case rwPRIMTYPETRISTRIP:
        return *reinterpret_cast<RxPipeline**>(im3d + 0x0C);
    case rwPRIMTYPETRIFAN:
        return *reinterpret_cast<RxPipeline**>(im3d + 0x08);
    case rwPRIMTYPEPOINTLIST:
        return *reinterpret_cast<RxPipeline**>(im3d + 0x18);
    default:
        RwError rwError;
        rwError.pluginID  = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x25, primType);
        RwErrorSet(&rwError);
        return nullptr;
    }
}


RxPipeline* RwIm3DSetTransformPipeline(RxPipeline* pipeline) {
    RwUInt8* im3d = reinterpret_cast<RwUInt8*>(RwEngineInstance) + *reinterpret_cast<RwUInt32*>(0xC9C078);
    if (pipeline != nullptr) {
        *reinterpret_cast<RxPipeline**>(im3d + 0x00) = pipeline;
    } else {
        *reinterpret_cast<RxPipeline**>(im3d + 0x00) = *reinterpret_cast<RxPipeline**>(im3d + 0x1C);
    }
    return *reinterpret_cast<RxPipeline**>(im3d + 0x00);
}


RxPipeline* RwIm3DSetRenderPipeline(RxPipeline* pipeline, RwPrimitiveType primType) {
    RwUInt8* im3d = reinterpret_cast<RwUInt8*>(RwEngineInstance) + *reinterpret_cast<RwUInt32*>(0xC9C078);
    if (pipeline != nullptr) {
        switch (primType) {
        case rwPRIMTYPELINELIST:  *reinterpret_cast<RxPipeline**>(im3d + 0x10) = pipeline; return pipeline;
        case rwPRIMTYPEPOLYLINE:  *reinterpret_cast<RxPipeline**>(im3d + 0x14) = pipeline; return pipeline;
        case rwPRIMTYPETRILIST:   *reinterpret_cast<RxPipeline**>(im3d + 0x04) = pipeline; return pipeline;
        case rwPRIMTYPETRISTRIP:  *reinterpret_cast<RxPipeline**>(im3d + 0x0C) = pipeline; return pipeline;
        case rwPRIMTYPETRIFAN:    *reinterpret_cast<RxPipeline**>(im3d + 0x08) = pipeline; return pipeline;
        case rwPRIMTYPEPOINTLIST: *reinterpret_cast<RxPipeline**>(im3d + 0x18) = pipeline; return pipeline;
        default:
            RwError rwError;
            rwError.pluginID  = 1;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x25, primType);
            RwErrorSet(&rwError);
            return nullptr;
        }
    }
    switch (primType) {
    case rwPRIMTYPELINELIST: {
        const RxPipeline* backup = *reinterpret_cast<RxPipeline* const*>(im3d + 0x2C);
        if (backup == nullptr) {
            *reinterpret_cast<RxPipeline**>(im3d + 0x10) = nullptr;
        } else {
            *reinterpret_cast<RxPipeline**>(im3d + 0x10) = const_cast<RxPipeline*>(backup);
        }
        return *reinterpret_cast<RxPipeline**>(im3d + 0x2C);   // backup slot per decompile
    }
    case rwPRIMTYPEPOLYLINE: {
        const RxPipeline* backup = *reinterpret_cast<RxPipeline* const*>(im3d + 0x30);
        if (backup == nullptr) {
            *reinterpret_cast<RxPipeline**>(im3d + 0x14) = nullptr;
        } else {
            *reinterpret_cast<RxPipeline**>(im3d + 0x14) = const_cast<RxPipeline*>(backup);
        }
        return *reinterpret_cast<RxPipeline**>(im3d + 0x14);
    }
    case rwPRIMTYPETRILIST: {
        const RxPipeline* backup = *reinterpret_cast<RxPipeline* const*>(im3d + 0x20);
        if (backup == nullptr) {
            *reinterpret_cast<RxPipeline**>(im3d + 0x04) = nullptr;
        } else {
            *reinterpret_cast<RxPipeline**>(im3d + 0x04) = const_cast<RxPipeline*>(backup);
        }
        return *reinterpret_cast<RxPipeline**>(im3d + 0x04);
    }
    case rwPRIMTYPETRISTRIP: {
        const RxPipeline* backup = *reinterpret_cast<RxPipeline* const*>(im3d + 0x28);
        if (backup == nullptr) {
            *reinterpret_cast<RxPipeline**>(im3d + 0x0C) = nullptr;
            return *reinterpret_cast<RxPipeline**>(im3d + 0x0C);
        }
        *reinterpret_cast<RxPipeline**>(im3d + 0x0C) = const_cast<RxPipeline*>(backup);
        return *reinterpret_cast<RxPipeline**>(im3d + 0x28);
    }
    case rwPRIMTYPETRIFAN: {
        const RxPipeline* backup = *reinterpret_cast<RxPipeline* const*>(im3d + 0x24);
        if (backup == nullptr) {
            *reinterpret_cast<RxPipeline**>(im3d + 0x08) = nullptr;
            return *reinterpret_cast<RxPipeline**>(im3d + 0x08);
        }
        *reinterpret_cast<RxPipeline**>(im3d + 0x08) = const_cast<RxPipeline*>(backup);
        return *reinterpret_cast<RxPipeline**>(im3d + 0x24);
    }
    case rwPRIMTYPEPOINTLIST: {
        const RxPipeline* backup = *reinterpret_cast<RxPipeline* const*>(im3d + 0x34);
        if (backup == nullptr) {
            *reinterpret_cast<RxPipeline**>(im3d + 0x18) = nullptr;
            return *reinterpret_cast<RxPipeline**>(im3d + 0x18);
        }
        *reinterpret_cast<RxPipeline**>(im3d + 0x18) = const_cast<RxPipeline*>(backup);
        return *reinterpret_cast<RxPipeline**>(im3d + 0x34);
    }
    default:
        RwError rwError;
        rwError.pluginID  = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x25, primType);
        RwErrorSet(&rwError);
        return nullptr;
    }
}


struct RwDDSHeaderGta
{
    RwUInt32 dwSize;              /* +0x00 */
    RwUInt32 dwFlags;             /* +0x04 (0x1007 required caps/size/pixeldata; 0x20000 mipcount) */
    RwUInt32 dwHeight;            /* +0x08 */
    RwUInt32 dwWidth;             /* +0x0C */
    RwUInt32 dwPitchOrLinearSize; /* +0x10 */
    RwUInt32 dwDepth;             /* +0x14 */
    RwUInt32 dwMipMapCount;       /* +0x18 */
    RwUInt32 dwReserved1[11];     /* +0x1C */
    RwUInt32 ddpfSize;            /* +0x48 */
    RwUInt32 ddpfFlags;           /* +0x4C (4=FourCC 0x40=RGB 0x41=RGB|ALPHA 1=ALPHA RGBA) */
    RwUInt32 ddpfFourCC;          /* +0x50 */
    RwUInt32 ddpfRGBBitCount;     /* +0x54 */
    RwUInt32 ddpfRBitMask;        /* +0x58 */
    RwUInt32 ddpfGBitMask;        /* +0x5C */
    RwUInt32 ddpfBBitMask;        /* +0x60 */
    RwUInt32 ddpfABitMask;        /* +0x64 */
    RwUInt32 dwCaps;              /* +0x68 (bit 8 = DDSCAPS_MIPMAP) */
    RwUInt32 dwCaps2;             /* +0x6C (0xFE00 = cubemap faces) */
    RwUInt32 dwCaps3;             /* +0x70 */
    RwUInt32 dwCaps4;             /* +0x74 */
    RwUInt32 dwReserved2;         /* +0x78 */
};
static_assert(sizeof(RwDDSHeaderGta) == 0x7C, "RwDDSHeaderGta layout");
static_assert(offsetof(RwDDSHeaderGta, ddpfFourCC)  == 0x50, "RwDDSHeaderGta layout");
static_assert(offsetof(RwDDSHeaderGta, dwCaps2)     == 0x6C, "RwDDSHeaderGta layout");

/* DXT1..DXT5 fourcc range */
#define RWDDS_ISDXT(f) (((RwInt32)(f) >= (RwInt32)0x31545844) && ((RwInt32)(f) <= (RwInt32)0x35545844))

// RwD3D9RasterStreamReadDDS @ 0x81F360: read a DDS file from `stream` into a raster.
// Fast path: hand DXTn straight to the D3D9 device (CreateTexture/CreateCubeTexture) and
// stream mip levels into it. Fallback: decode to a 32bpp RwImage (DXT decoders or the
// per-format conversion loops) and build the raster from the image.
RwRaster *RwD3D9RasterStreamReadDDS(RwStream *stream)
{
    RwUInt32 magic;
    RwDDSHeaderGta hdr;

    if (RwStreamRead(stream, &magic, 4) != 4)
        return NULL;
    if (magic != 0x20534444 /* 'DDS ' */)
        return NULL;
    if (RwStreamRead(stream, &hdr, 0x7C) != 0x7C)
        return NULL;
    if ((hdr.ddpfFlags & 0x1007) == 0)
        return NULL;

    RwUInt32 mipFlag = (hdr.dwFlags >> 2) & 0x8000; /* DDSD_MIPMAPCOUNT -> raster flag 0x8000 */
    RwBool isCube = ((hdr.dwCaps & 8) != 0) && ((hdr.dwCaps2 & 0xFE00) != 0);
    RwUInt32 width = hdr.dwWidth;
    RwUInt32 height = hdr.dwHeight;

    RwRaster *raster = NULL;
    RwUInt32 d3dFormat = 0;
    RwUInt32 rasterFlags = 0;
    RwBool softwarePath = FALSE;

    if (hdr.ddpfFlags == 4)
    {
        /* ---- FourCC / DXT: hand to the device, else software-decode ---- */
        if (isCube)
        {
            raster = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBEE0)(hdr.ddpfFourCC);
            if (raster == NULL)
                return NULL;
        }
        else
        {
            raster = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(hdr.ddpfFourCC);
            if (raster == NULL)
                softwarePath = TRUE;
        }

        if (!softwarePath)
        {
            raster = RwRasterCreate((RwInt32)width, (RwInt32)height, 0, (mipFlag != 0) ? 0x80000084u : 0x84);
            if (raster == NULL)
                return NULL;
            raster->cFlags &= 0x7F;

            RwD3D9RasterExtGta *ext = (RwD3D9RasterExtGta *)((RwUInt8 *)raster + g_rwD3D9RasterExtOffset);
            ext->format = hdr.ddpfFourCC;
            ext->flags2 = (RwUInt8)(RWDDS_ISDXT(hdr.ddpfFourCC) ? ((ext->flags2 & 0x0F) | 0x10)
                                                                : (ext->flags2 & 0x0F));

            if (isCube)
            {
                RwInt32 levels = ((g_rwD3D9DeviceCapsFlags & 0x30000) != 0 && (hdr.dwFlags & 0x20000) != 0)
                                 ? (RwInt32)hdr.dwMipMapCount : 1;
                ext->flags = (RwUInt8)((ext->flags & 0xF1) | 1);
                if (_RwD3DDevice->CreateCubeTexture(width, levels, 0, (D3DFORMAT)ext->format,
                                                    D3DPOOL_MANAGED,
                                                    (IDirect3DCubeTexture9 **)&ext->texture, NULL) < 0)
                    return NULL;
            }
            else
            {
                RwInt32 levels = ((hdr.dwFlags & 0x20000) != 0) ? (RwInt32)hdr.dwMipMapCount : 1;
                if (_RwD3DDevice->CreateTexture(width, height, levels, 0, (D3DFORMAT)ext->format,
                                                D3DPOOL_MANAGED,
                                                (IDirect3DTexture9 **)&ext->texture, NULL) < 0)
                    return NULL;
            }
        }
    }
    else if (hdr.ddpfFlags == 0x40)
    {
        /* ---- RGB without alpha ---- */
        if (isCube)
        {
            if (hdr.ddpfRGBBitCount == 0x18)
            {
                d3dFormat = 0x14; /* R8G8B8 */
                raster = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBEE0)(0x14);
                if (raster == NULL)
                    return NULL;
                rasterFlags = 0x600;
            }
            else if (hdr.ddpfRBitMask == 0xff0000)
            {
                d3dFormat = 0x16; /* X8R8G8B8 */
                raster = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBEE0)(0x16);
                if (raster == NULL)
                    return NULL;
                rasterFlags = 0x600;
            }
            else if (hdr.ddpfRBitMask == 0xf800)
            {
                d3dFormat = 0x17; /* R5G6B5 */
                raster = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBEE0)(0x17);
                if (raster == NULL)
                    return NULL;
                rasterFlags = 0x200;
            }
            else
            {
                return NULL;
            }
            rasterFlags |= mipFlag;
            RwUInt32 fitW = width;
            RwUInt32 fitH = height;
            ((void (__cdecl *)(RwUInt32 *, RwUInt32 *, RwUInt32))0x7FED70)
                (&fitW, &fitH, ((rasterFlags >> 8) << 24) | 4);
            if (width != fitW)
                return NULL;
            if (height != fitH)
                return NULL;
            raster = RwRasterCreate((RwInt32)width, (RwInt32)height, 0, rasterFlags | 0x84);
            if (raster == NULL)
                return NULL;
            raster->cFlags &= 0x7F;
            RwD3D9RasterExtGta *ext = (RwD3D9RasterExtGta *)((RwUInt8 *)raster + g_rwD3D9RasterExtOffset);
            ext->flags = (RwUInt8)((ext->flags & 0xF1) | 1);
            RwInt32 levels = ((g_rwD3D9DeviceCapsFlags & 0x30000) != 0 && (hdr.dwFlags & 0x20000) != 0)
                             ? (RwInt32)hdr.dwMipMapCount : 1;
            if (_RwD3DDevice->CreateCubeTexture(width, levels, 0, (D3DFORMAT)d3dFormat,
                                                D3DPOOL_MANAGED,
                                                (IDirect3DCubeTexture9 **)&ext->texture, NULL) < 0)
                return NULL;
            goto loadLevels;
        }
        else
        {
            if (hdr.ddpfRGBBitCount == 0x18)
            {
                if (((RwRaster * (__cdecl *)(RwUInt32))0x4CBEE0)(0x14) == NULL)
                    softwarePath = TRUE;
                d3dFormat = 0x14;
            }
            else if (hdr.ddpfRBitMask == 0xff0000)
            {
                if (((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x16) == NULL)
                    softwarePath = TRUE;
                d3dFormat = 0x16;
            }
            else if (hdr.ddpfRBitMask == 0xf800)
            {
                d3dFormat = 0x17;
            }
            else
            {
                if (hdr.ddpfRBitMask != 0xe0)
                    return NULL;
                if (((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x1b) == NULL)
                    softwarePath = TRUE;
                d3dFormat = 0x1b;
            }
            if (!softwarePath)
            {
                RwUInt32 fitW = width;
                RwUInt32 fitH = height;
                ((void (__cdecl *)(RwUInt32 *, RwUInt32 *, RwUInt32))0x7FED70)(&fitW, &fitH, mipFlag | 4);
                if (width == fitW && height == fitH)
                {
                    raster = ((RwRaster * (__cdecl *)(RwUInt32, RwUInt32, RwUInt32, RwUInt32))0x4CD050)
                             (width, height, d3dFormat, mipFlag | 4);
                    if (raster == NULL)
                        return NULL;
                    goto loadLevels;
                }
                softwarePath = TRUE;
            }
        }
    }
    else if (hdr.ddpfFlags == 0x41)
    {
        /* ---- RGB with alpha ---- */
        if (isCube)
        {
            if (hdr.ddpfRBitMask == 0xff0000)
            {
                d3dFormat = 0x16;
                raster = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBEE0)(0x16);
                if (raster == NULL)
                    return NULL;
                rasterFlags = 0x600;
            }
            else if (hdr.ddpfRBitMask == 0x7c00)
            {
                d3dFormat = 0x19; /* A1R5G5B5 */
                raster = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBEE0)(0x19);
                if (raster == NULL)
                    return NULL;
                rasterFlags = 0x100;
            }
            else
            {
                if (hdr.ddpfRBitMask != 0xf00)
                    return NULL;
                d3dFormat = 0x1a; /* A4R4G4B4 */
                raster = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBEE0)(0x1a);
                if (raster == NULL)
                    return NULL;
                rasterFlags = 0x300;
            }
            rasterFlags |= mipFlag;
            RwUInt32 fitW = width;
            RwUInt32 fitH = height;
            ((void (__cdecl *)(RwUInt32 *, RwUInt32 *, RwUInt32))0x7FED70)
                (&fitW, &fitH, ((rasterFlags >> 8) << 24) | 4);
            if (width != fitW)
                return NULL;
            if (height != fitH)
                return NULL;
            raster = RwRasterCreate((RwInt32)width, (RwInt32)height, 0, rasterFlags | 0x84);
            if (raster == NULL)
                return NULL;
            raster->cFlags &= 0x7F;
            RwD3D9RasterExtGta *ext = (RwD3D9RasterExtGta *)((RwUInt8 *)raster + g_rwD3D9RasterExtOffset);
            ext->flags = (RwUInt8)((ext->flags & 0xF1) | 1);
            RwInt32 levels = ((g_rwD3D9DeviceCapsFlags & 0x30000) != 0 && (hdr.dwFlags & 0x20000) != 0)
                             ? (RwInt32)hdr.dwMipMapCount : 1;
            if (_RwD3DDevice->CreateCubeTexture(width, levels, 0, (D3DFORMAT)d3dFormat,
                                                D3DPOOL_MANAGED,
                                                (IDirect3DCubeTexture9 **)&ext->texture, NULL) < 0)
                return NULL;
            goto loadLevels;
        }
        else
        {
            if (hdr.ddpfRBitMask == 0xff0000)
            {
                if (((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x15) == NULL) /* A8R8G8B8 */
                    softwarePath = TRUE;
                d3dFormat = 0x15;
            }
            else if (hdr.ddpfRBitMask == 0x7c00)
            {
                d3dFormat = 0x19;
            }
            else if (hdr.ddpfRBitMask == 0xf00)
            {
                if (((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x1a) == NULL)
                    softwarePath = TRUE;
                d3dFormat = 0x1a;
            }
            else if (hdr.ddpfRBitMask == 0x3ff)
            {
                if (((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x23) == NULL)
                    softwarePath = TRUE;
                d3dFormat = 0x23;
            }
            else if (hdr.ddpfBBitMask == 0x3ff)
            {
                if (((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x1f) == NULL)
                    softwarePath = TRUE;
                d3dFormat = 0x1f;
            }
            else
            {
                if (hdr.ddpfRBitMask != 0xe0)
                    return NULL;
                if (((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x1d) == NULL)
                    softwarePath = TRUE;
                d3dFormat = 0x1d;
            }
            if (!softwarePath)
            {
                RwUInt32 fitW = width;
                RwUInt32 fitH = height;
                ((void (__cdecl *)(RwUInt32 *, RwUInt32 *, RwUInt32))0x7FED70)(&fitW, &fitH, mipFlag | 4);
                if (width == fitW && height == fitH)
                {
                    raster = ((RwRaster * (__cdecl *)(RwUInt32, RwUInt32, RwUInt32, RwUInt32))0x4CD050)
                             (width, height, d3dFormat, mipFlag | 4);
                    if (raster == NULL)
                        return NULL;
                    goto loadLevels;
                }
                softwarePath = TRUE;
            }
        }
    }
    else
    {
        /* ---- luminance / 16-8 bpp formats (2D only) ---- */
        if (isCube)
            return NULL;
        if (hdr.ddpfRGBBitCount == 0x10)
        {
            if (hdr.ddpfRBitMask == 0xffff)
            {
                if (((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x51) == NULL) /* L16 */
                    softwarePath = TRUE;
                d3dFormat = 0x51;
            }
            else
            {
                if (hdr.ddpfRBitMask != 0xff)
                    return NULL;
                if (hdr.ddpfABitMask != 0xff00)
                    return NULL;
                if (((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x33) == NULL) /* A8L8 */
                    softwarePath = TRUE;
                d3dFormat = 0x33;
            }
        }
        else
        {
            if (hdr.ddpfRGBBitCount != 8)
                return NULL;
            if (hdr.ddpfRBitMask == 0xff)
            {
                if (((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x32) == NULL) /* L8 */
                    softwarePath = TRUE;
                d3dFormat = 0x32;
            }
            else
            {
                if (hdr.ddpfABitMask != 0xff)
                    return NULL;
                if (((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x1c) == NULL) /* A8 */
                    softwarePath = TRUE;
                d3dFormat = 0x1c;
            }
        }
        if (!softwarePath)
        {
            RwUInt32 fitW = width;
            RwUInt32 fitH = height;
            ((void (__cdecl *)(RwUInt32 *, RwUInt32 *, RwUInt32))0x7FED70)(&fitW, &fitH, mipFlag | 4);
            if (width == fitW && height == fitH)
            {
                raster = ((RwRaster * (__cdecl *)(RwUInt32, RwUInt32, RwUInt32, RwUInt32))0x4CD050)
                         (width, height, d3dFormat, mipFlag | 4);
                if (raster == NULL)
                    return NULL;
                goto loadLevels;
            }
            softwarePath = TRUE;
        }
    }

    if (softwarePath)
    {
        /* ---- software conversion into a 32bpp image (LAB_0081ffd8) ---- */
        RwUInt32 readTotal; /* uVar20 */
        RwUInt32 rowBytes;  /* uStack_d8: source row stride (consumed by 0x24-swizzle and
                               the conversion loops; the binary leaves it uninitialised on
                               the FourCC/pitch!=0 sub-path, which no shipped format hits) */
        rasterFlags = 0;    /* uStack_e0 */
        if (hdr.ddpfFlags == 4)
        {
            rowBytes = 0;
            if (hdr.dwPitchOrLinearSize == 0)
            {
                if (!RWDDS_ISDXT(hdr.ddpfFourCC))
                {
                    rowBytes = (RwUInt32)(g_rwD3D9FormatPropTable[hdr.ddpfFourCC * 4 + 1] >> 3) * width;
                    readTotal = rowBytes * height;
                }
                else
                {
                    RwUInt32 blockShift;
                    if (hdr.ddpfFourCC == 0x31545844) /* DXT1 */
                        blockShift = 3;
                    else
                        blockShift = 4; /* DXT2..DXT5 */
                    RwUInt32 bw = width >> 2;
                    RwUInt32 bh = height >> 2;
                    if (bw == 0) bw = 1;
                    if (bh == 0) bh = 1;
                    readTotal = bh * bw << blockShift;
                }
            }
            else
            {
                readTotal = hdr.dwPitchOrLinearSize;
            }
        }
        else if ((hdr.dwFlags & 0x80000) == 0) /* no DDSD_LINEARSIZE */
        {
            rowBytes = hdr.dwPitchOrLinearSize;
            if (rowBytes == 0)
            {
                rowBytes = width;
                if (hdr.ddpfRGBBitCount != 8)
                {
                    if (hdr.ddpfRGBBitCount == 0x10)
                        rowBytes = width * 2;
                    else if (hdr.ddpfRGBBitCount == 0x18)
                        rowBytes = width * 3;
                    else if (hdr.ddpfRGBBitCount == 0x20)
                        rowBytes = width * 4;
                    else if (hdr.ddpfRGBBitCount == 0x40)
                        rowBytes = width * 8;
                    else
                    {
                        rowBytes = width * 4;
                        if (hdr.ddpfRBitMask != 0xff0000)
                            rowBytes = width * 2;
                    }
                }
            }
            readTotal = rowBytes * height;
        }
        else
        {
            rowBytes = width;
            if (hdr.ddpfRGBBitCount != 8)
            {
                if (hdr.ddpfRGBBitCount == 0x10)
                    rowBytes = width * 2;
                else if (hdr.ddpfRGBBitCount == 0x18)
                    rowBytes = width * 3;
                else if (hdr.ddpfRGBBitCount == 0x20)
                    rowBytes = width * 4;
                else if (hdr.ddpfRGBBitCount == 0x40)
                    rowBytes = width << 3;
                else if (hdr.ddpfRBitMask == 0xff0000)
                    rowBytes = width * 4;
                else
                    rowBytes = width * 2;
            }
            readTotal = hdr.dwPitchOrLinearSize;
        }

        RwUInt32 *buffer = (RwUInt32 *)RwGetGlobalsFnViewGta()->rwmalloc(readTotal, 0x10018);
        if (RwStreamRead(stream, buffer, readTotal) != readTotal)
        {
            RwGetGlobalsFnViewGta()->rwfree(buffer);
            return NULL;
        }
        RwImage *image = RwImageCreate((RwInt32)width, (RwInt32)height, 0x20);
        if (image == NULL)
        {
            RwGetGlobalsFnViewGta()->rwfree(buffer);
            return NULL;
        }
        RwImageAllocatePixels(image);

        if (hdr.ddpfFlags == 4)
        {
            if ((RwInt32)hdr.ddpfFourCC < (RwInt32)0x33545845)
            {
                if (hdr.ddpfFourCC != 0x33545844)
                {
                    if (hdr.ddpfFourCC == 0x24)
                    {
                        /* legacy fourcc 36: 8-byte pixels, byte-swap swizzle */
                        RwRaster *probe = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x15);
                        if (probe != NULL)
                            rasterFlags = 0x500;
                        else
                        {
                            probe = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x1a);
                            rasterFlags = 0x100 + ((probe != NULL) ? 0x200 : 0);
                        }
                        RwUInt32 row;
                        for (row = 0; row < height; row++)
                        {
                            RwUInt8 *dst = image->cpPixels + image->stride * row;
                            RwUInt8 *src = (RwUInt8 *)buffer + 3 + row * rowBytes;
                            RwUInt32 x;
                            for (x = 0; x < width; x++)
                            {
                                dst[0] = src[-2];
                                dst[1] = src[0];
                                dst[2] = src[2];
                                dst[3] = src[4];
                                dst += 4;
                                src += 8;
                            }
                        }
                    }
                    else if (hdr.ddpfFourCC == 0x31545844) /* DXT1 */
                    {
                        RwInt32 ok = ((RwInt32 (__cdecl *)(RwUInt32, RwUInt32, void *, void *))0x81E720)
                                     (width, height, image->cpPixels, buffer);
                        rasterFlags = (ok != 0) ? 0x100 : 0x200;
                    }
                    else if (hdr.ddpfFourCC == 0x32545844) /* DXT2 */
                    {
                        RwRaster *probe = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x1a);
                        rasterFlags = 0x100 + ((probe != NULL) ? 0x200 : 0);
                        ((void (__cdecl *)(RwUInt32, RwUInt32, void *, void *))0x81EA70)
                            (width, height, image->cpPixels, buffer);
                    }
                }
                else
                {
                    /* DXT3 */
                    RwRaster *probe = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x1a);
                    rasterFlags = 0x100 + ((probe != NULL) ? 0x200 : 0);
                    ((void (__cdecl *)(RwUInt32, RwUInt32, void *, void *))0x81EA70)
                        (width, height, image->cpPixels, buffer);
                }
            }
            else if (hdr.ddpfFourCC == 0x34545844 || hdr.ddpfFourCC == 0x35545844)
            {
                /* DXT4 / DXT5 */
                RwRaster *probe = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x1a);
                rasterFlags = 0x100 + ((probe != NULL) ? 0x200 : 0);
                ((void (__cdecl *)(RwUInt32, RwUInt32, void *, void *))0x81EDC0)
                    (width, height, image->cpPixels, buffer);
            }
        }
        else if (hdr.ddpfFlags == 0x40)
        {
            if (hdr.ddpfRGBBitCount == 0x18)
            {
                RwRaster *probe = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x16);
                rasterFlags = 0x200 + ((probe != NULL) ? 0x400 : 0);
                RwUInt32 row;
                for (row = 0; row < height; row++)
                {
                    RwUInt8 *dst = image->cpPixels + image->stride * row;
                    RwUInt8 *src = (RwUInt8 *)buffer + row * rowBytes;
                    RwUInt32 x;
                    for (x = 0; x < width; x++)
                    {
                        dst[0] = src[2];
                        dst[1] = src[1];
                        dst[2] = src[0];
                        dst[3] = 0xFF;
                        dst += 4;
                        src += 3;
                    }
                }
            }
            else if (hdr.ddpfRBitMask == 0xff0000)
            {
                RwRaster *probe = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x16);
                rasterFlags = 0x200 + ((probe != NULL) ? 0x400 : 0);
                RwUInt32 row;
                for (row = 0; row < height; row++)
                {
                    RwUInt8 *dst = image->cpPixels + image->stride * row;
                    RwUInt32 *src = (RwUInt32 *)((RwUInt8 *)buffer + row * rowBytes);
                    RwUInt32 x;
                    for (x = 0; x < width; x++)
                    {
                        RwUInt32 px = *src++;
                        dst[3] = 0xFF;
                        dst[2] = (RwUInt8)px;
                        dst[0] = (RwUInt8)(px >> 0x10);
                        dst[1] = (RwUInt8)(px >> 8);
                        dst += 4;
                    }
                }
            }
            else if (hdr.ddpfRBitMask == 0xf800)
            {
                rasterFlags = 0x200;
                RwUInt32 row;
                for (row = 0; row < height; row++)
                {
                    RwUInt8 *dst = image->cpPixels + image->stride * row;
                    RwUInt32 *src = (RwUInt32 *)((RwUInt8 *)buffer + row * rowBytes);
                    RwUInt32 x;
                    for (x = 0; x < width; x++)
                    {
                        RwUInt16 px = (RwUInt16)*src;
                        src = (RwUInt32 *)((RwUInt8 *)src + 2);
                        dst[3] = 0xFF;
                        dst[0] = (RwUInt8)(px >> 8) & 0xF8;
                        dst[1] = (RwUInt8)(px >> 3) & 0xFC;
                        dst[2] = (RwUInt8)(px << 3);
                        dst += 4;
                    }
                }
            }
            else if (hdr.ddpfRBitMask == 0xe0)
            {
                rasterFlags = 0x200;
                RwUInt32 row;
                for (row = 0; row < height; row++)
                {
                    RwUInt8 *dst = image->cpPixels + image->stride * row;
                    RwUInt8 *src = (RwUInt8 *)buffer + row * rowBytes;
                    RwUInt32 x;
                    for (x = 0; x < width; x++)
                    {
                        RwUInt8 b = src[x];
                        dst[3] = 0xFF;
                        dst[0] = b & 0xE0;
                        dst[1] = (b & 0xFC) << 3;
                        dst[2] = b << 6;
                        dst += 4;
                    }
                }
            }
        }
        else if (hdr.ddpfFlags == 0x41)
        {
            if (hdr.ddpfRBitMask == 0xff0000)
            {
                RwRaster *probe = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x15);
                if (probe != NULL)
                    rasterFlags = 0x500;
                else
                {
                    probe = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x1a);
                    rasterFlags = 0x100 + ((probe != NULL) ? 0x200 : 0);
                }
                RwUInt32 row;
                for (row = 0; row < height; row++)
                {
                    RwUInt8 *dst = image->cpPixels + image->stride * row;
                    RwUInt32 *src = (RwUInt32 *)((RwUInt8 *)buffer + row * rowBytes);
                    RwUInt32 x;
                    for (x = 0; x < width; x++)
                    {
                        RwUInt32 px = *src++;
                        dst[2] = (RwUInt8)px;
                        dst[0] = (RwUInt8)(px >> 0x10);
                        dst[1] = (RwUInt8)(px >> 8);
                        dst[3] = (RwUInt8)(px >> 0x18);
                        dst += 4;
                    }
                }
            }
            else if (hdr.ddpfRBitMask == 0x7c00)
            {
                rasterFlags = 0x100;
                RwUInt32 row;
                for (row = 0; row < height; row++)
                {
                    RwUInt8 *dst = image->cpPixels + image->stride * row;
                    RwUInt32 *src = (RwUInt32 *)((RwUInt8 *)buffer + row * rowBytes);
                    RwUInt32 x;
                    for (x = 0; x < width; x++)
                    {
                        RwUInt16 px = (RwUInt16)*src;
                        src = (RwUInt32 *)((RwUInt8 *)src + 2);
                        dst[3] = ((RwInt16)px < 0) ? 0xFF : 0;
                        dst[0] = (RwUInt8)(px >> 7) & 0xF8;
                        dst[1] = (RwUInt8)(px >> 2) & 0xF8;
                        dst[2] = (RwUInt8)(px << 3);
                        dst += 4;
                    }
                }
            }
            else if (hdr.ddpfRBitMask == 0xf00)
            {
                RwRaster *probe = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x1a);
                rasterFlags = 0x100 + ((probe != NULL) ? 0x200 : 0);
                RwUInt32 row;
                for (row = 0; row < height; row++)
                {
                    RwUInt8 *dst = image->cpPixels + image->stride * row;
                    RwUInt32 *src = (RwUInt32 *)((RwUInt8 *)buffer + row * rowBytes);
                    RwUInt32 x;
                    for (x = 0; x < width; x++)
                    {
                        RwUInt16 px = (RwUInt16)*src;
                        src = (RwUInt32 *)((RwUInt8 *)src + 2);
                        dst[3] = (RwUInt8)(px >> 8) & 0xF0;
                        dst[0] = (RwUInt8)(px >> 4) & 0xF0;
                        dst[1] = (RwUInt8)px & 0xF0;
                        dst[2] = (RwUInt8)(px << 4);
                        dst += 4;
                    }
                }
            }
            else if (hdr.ddpfRBitMask == 0x3ff)
            {
                RwRaster *probe = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x15);
                if (probe != NULL)
                    rasterFlags = 0x500;
                else
                {
                    probe = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x1a);
                    rasterFlags = 0x100 + ((probe != NULL) ? 0x200 : 0);
                }
                RwUInt32 row;
                for (row = 0; row < height; row++)
                {
                    RwUInt8 *dst = image->cpPixels + image->stride * row;
                    RwUInt32 *src = (RwUInt32 *)((RwUInt8 *)buffer + row * rowBytes);
                    RwUInt32 x;
                    for (x = 0; x < width; x++)
                    {
                        RwUInt32 px = *src++;
                        dst[0] = (RwUInt8)(px >> 0x16);
                        dst[1] = (RwUInt8)(px >> 0xC);
                        dst[2] = (RwUInt8)(px >> 2);
                        dst[3] = (RwUInt8)(px >> 0x1E) * 0x55;
                        dst += 4;
                    }
                }
            }
            else if (hdr.ddpfRBitMask == 0xe0)
            {
                rasterFlags = 0x300;
                RwUInt32 row;
                for (row = 0; row < height; row++)
                {
                    RwUInt8 *dst = image->cpPixels + image->stride * row;
                    RwUInt32 *src = (RwUInt32 *)((RwUInt8 *)buffer + row * rowBytes);
                    RwUInt32 x;
                    for (x = 0; x < width; x++)
                    {
                        RwUInt32 px = *src;
                        src = (RwUInt32 *)((RwUInt8 *)src + 2);
                        RwUInt8 b = (RwUInt8)(RwUInt16)px;
                        dst[0] = b & 0xE0;
                        dst[1] = (b & 0xFC) << 3;
                        dst[2] = b << 6;
                        dst[3] = (RwUInt8)((RwUInt16)px >> 8);
                        dst += 4;
                    }
                }
            }
            else if (hdr.ddpfBBitMask == 0x3ff)
            {
                RwRaster *probe = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x15);
                if (probe != NULL)
                    rasterFlags = 0x500;
                else
                {
                    probe = ((RwRaster * (__cdecl *)(RwUInt32))0x4CBDE0)(0x1a);
                    rasterFlags = 0x100 + ((probe != NULL) ? 0x200 : 0);
                }
                RwUInt32 row;
                for (row = 0; row < height; row++)
                {
                    RwUInt8 *dst = image->cpPixels + image->stride * row;
                    RwUInt32 *src = (RwUInt32 *)((RwUInt8 *)buffer + row * rowBytes);
                    RwUInt32 x;
                    for (x = 0; x < width; x++)
                    {
                        RwUInt32 px = *src++;
                        dst[0] = (RwUInt8)(px >> 2);
                        dst[1] = (RwUInt8)(px >> 0xC);
                        dst[2] = (RwUInt8)(px >> 0x16);
                        dst[3] = (RwUInt8)(px >> 0x1E) * 0x55;
                        dst += 4;
                    }
                }
            }
        }

        RwGetGlobalsFnViewGta()->rwfree(buffer);

        if (mipFlag != 0)
            rasterFlags |= mipFlag | 0x1000;
        raster = RwRasterCreate((RwInt32)width, (RwInt32)height, 0, rasterFlags | 4);
        if (raster != NULL)
        {
            if (image->width != raster->width || image->height != raster->height)
            {
                RwImage *scaled = RwImageCreateResample(image, raster->width, raster->height);
                RwImageDestroy(image);
                image = scaled;
            }
            RwRasterSetFromImage(raster, image);
            RwImageDestroy(image);
            return raster;
        }
        RwImageDestroy(image);
        return NULL;
    }

loadLevels:
    /* ---- stream mip levels into the device texture (LAB_0081f543) ---- */
    {
        RwD3D9RasterExtGta *ext = (RwD3D9RasterExtGta *)((RwUInt8 *)raster + g_rwD3D9RasterExtOffset);
        RwInt32 numLevels = RwRasterGetNumLevels(raster);
        RwUInt32 caps2 = hdr.dwCaps2;
        RwInt32 streamLevels = (mipFlag == 0) ? 1 : (RwInt32)hdr.dwMipMapCount;

        do
        {
            if (isCube)
            {
                /* mark the next cubemap face, clear its caps2 bits */
                if ((caps2 & 0x600) != 0)
                {
                    ext->flags = (RwUInt8)(ext->flags & 0x0F);
                    caps2 &= 0xFFFFF9FF;
                }
                else if ((caps2 & 0xA00) != 0)
                {
                    ext->flags = (RwUInt8)((ext->flags & 0x0F) | 0x10);
                    caps2 &= 0xFFFFF5FF;
                }
                else if ((caps2 & 0x1200) != 0)
                {
                    ext->flags = (RwUInt8)((ext->flags & 0x0F) | 0x20);
                    caps2 &= 0xFFFFEDFF;
                }
                else if ((caps2 & 0x2200) != 0)
                {
                    ext->flags = (RwUInt8)((ext->flags & 0x0F) | 0x30);
                    caps2 &= 0xFFFFDDFF;
                }
                else if ((caps2 & 0x4200) != 0)
                {
                    ext->flags = (RwUInt8)((ext->flags & 0x0F) | 0x40);
                    caps2 &= 0xFFFFBDFF;
                }
                else if ((caps2 & 0x8200) != 0)
                {
                    ext->flags = (RwUInt8)((ext->flags & 0x0F) | 0x50);
                    caps2 = 0;
                }
            }

            for (RwInt32 level = 0; level < streamLevels; level++)
            {
                if (level < numLevels)
                {
                    void *bits = RwRasterLock(raster, level, 1);
                    D3DSURFACE_DESC desc; /* auStack_20: vtbl+0x44 = GetLevelDesc (#17) */
                    ((IDirect3DTexture9 *)ext->texture)->GetLevelDesc(level, &desc);
                    RwUInt32 readSize;
                    if (hdr.ddpfFlags == 4 && RWDDS_ISDXT(ext->format))
                        readSize = (desc.Height >> 2) * ext->pitch;
                    else
                        readSize = ext->pitch * desc.Height;
                    if (RwStreamRead(stream, bits, readSize) != readSize)
                        return NULL;
                    RwRasterUnlock(raster);
                }
                else
                {
                    /* skip data of a mip level the raster does not store */
                    RwUInt32 w = (RwUInt32)raster->width >> level;
                    RwUInt32 h = (RwUInt32)raster->height >> level;
                    if (w == 0) w = 1;
                    if (h == 0) h = 1;
                    if ((ext->flags2 & 0xF0) == 0)
                    {
                        RwStreamSkip(stream, ((RwUInt32)raster->depth >> 3) * h * w);
                    }
                    else
                    {
                        w >>= 2;
                        h >>= 2;
                        if (w == 0) w = 1;
                        if (h == 0) h = 1;
                        RwStreamSkip(stream, h * w << (3 + (ext->format != 0x31545844)));
                    }
                }
            }
        } while (isCube && caps2 != 0);

        /* ---- finish (LAB_0081fd50) ---- */
        if (hdr.ddpfFlags != 4)
        {
            if (((hdr.ddpfFlags & 1) == 0) && (hdr.ddpfABitMask == 0))
                ext->hasAlpha = 0;
            else
                ext->hasAlpha = 1;
            return raster;
        }

        if (ext->format != 0x31545844) /* not DXT1 */
        {
            if (ext->format == 0x32545844 || ext->format == 0x33545844 ||
                ext->format == 0x34545844 || ext->format == 0x35545844)
            {
                /* DXT2..DXT5: explicit alpha */
                ext->hasAlpha = 1;
                raster->depth = 0x10;
                raster->cFormat = (RwUInt8)((raster->cFormat & 0xF3) | 3);
                return raster;
            }
            /* other fourcc: fetch properties from the format table */
            ext->hasAlpha = g_rwD3D9FormatPropTable[ext->format * 4 + 0];
            raster->depth = (RwInt32)g_rwD3D9FormatPropTable[ext->format * 4 + 1];
            raster->cFormat = (RwUInt8)(g_rwD3D9FormatPropTable[ext->format * 4 + 3] |
                                        (raster->cFormat & 0xF0));
            return raster;
        }

        /* DXT1: scan every block for a fully transparent pixel (index 3).
         * DXT1 blocks with color0 <= color1 carry the 1-bit alpha mode. */
        {
            RwBool hasAlpha = FALSE;
            RwInt32 numL = RwRasterGetNumLevels(raster);
            RwInt32 level = 0;
            if (0 < numL)
            {
                do
                {
                    RwUInt16 *block = (RwUInt16 *)RwRasterLock(raster, level, 2);
                    RwUInt32 w = (RwUInt32)raster->width;
                    RwUInt32 h = (RwUInt32)raster->height;
                    RwUInt32 idxPerRow; /* iVar17 */
                    RwUInt32 rows;      /* iStack_dc */
                    RwUInt32 blocks;    /* uStack_e0 */
                    if (3 < w)
                    {
                        idxPerRow = 4;
                        blocks = (w + (((RwInt32)w >> 31) & 3)) >> 2;
                    }
                    else
                    {
                        blocks = 1;
                        idxPerRow = w;
                    }
                    if (h < 4)
                    {
                        rows = h;
                    }
                    else
                    {
                        RwUInt32 rows4 = h + (((RwInt32)h >> 31) & 3);
                        rows = 4;
                        blocks = (rows4 >> 2) * blocks;
                    }

                    RwBool found = FALSE;
                    if (rows < 4 || w <= 3)
                    {
                        /* row-at-a-time scan (edge blocks) */
                        RwUInt32 b = blocks;
                        do
                        {
                            if (block[0] <= block[1])
                            {
                                RwUInt8 *idx = (RwUInt8 *)(block + 2);
                                for (RwUInt32 r = 0; r < rows; r++)
                                {
                                    RwUInt32 v = idx[r];
                                    for (RwUInt32 i = 0; i < idxPerRow; i++)
                                    {
                                        if ((v & 3) == 3)
                                        {
                                            found = TRUE;
                                            break;
                                        }
                                        v >>= 2;
                                    }
                                    if (found)
                                        break;
                                }
                            }
                            block += 4; /* 8 bytes per block */
                            b--;
                        } while (b != 0 && !found);
                    }
                    else
                    {
                        /* full 4x4 block scan */
                        RwUInt32 b = blocks;
                        do
                        {
                            if (block[0] <= block[1])
                            {
                                for (RwUInt32 r = 0; r < 4; r++)
                                {
                                    RwUInt32 v = ((RwUInt8 *)block)[4 + r];
                                    for (RwUInt32 i = 0; i < 4; i++)
                                    {
                                        if ((v & 3) == 3)
                                        {
                                            found = TRUE;
                                            break;
                                        }
                                        v >>= 2;
                                    }
                                    if (found)
                                        break;
                                }
                            }
                            block += 4;
                            b--;
                        } while (b != 0 && !found);
                    }
                    RwRasterUnlock(raster);
                    if (found)
                    {
                        hasAlpha = TRUE;
                        break;
                    }
                    level++;
                } while (level < numL);
            }

            ext->hasAlpha = hasAlpha;
            if (hasAlpha)
            {
                raster->depth = 0x10;
                raster->cFormat = (RwUInt8)((raster->cFormat & 0xF1) | 1);
                return raster;
            }
            raster->cFormat = (RwUInt8)((raster->cFormat & 0xF2) | 2);
            return raster;
        }
    }
}


RwTexture *RwD3D9DDSTextureRead(const RwChar *name, const RwChar *maskname)
{
    (void)maskname; /* unused by the binary */
    RwGlobalsFnViewGta *eng = RwGetGlobalsFnViewGta();
    RwBool (*rwfexist)(const RwChar *) =
        *reinterpret_cast<RwBool (__cdecl **)(const RwChar *)>(reinterpret_cast<RwUInt8 *>(RwEngineInstance) + 0xC4);
    RwChar pathBuf[256];
    RwTexture* texture = NULL;
    RwChar *searchPath = ((RwChar * (__cdecl *)(void))0x802F60)();

    if (((RwBool(__cdecl *)(const RwChar *))0x80E9B0)(name) != FALSE ||
        searchPath == NULL || *searchPath == '\0')
    {
        /* Direct path: scan back for the last '\\' (texture named after the bare
         * file name that follows it — or the whole name when there is none). */
        RwChar *p = (RwChar *)name + eng->stringFuncs.vecStrlen((RwChar *)name);
        RwChar *fileName = p;
        for (;;)
        {
            fileName = p;
            p--;
            if (p < name)
                break;
            if (*p == '\\')
            {
                fileName = p + 1;
                break;
            }
        }

        eng->stringFuncs.vecStrcpy(pathBuf, (RwChar *)name);
        eng->stringFuncs.vecStrcat(pathBuf, ".dds" /* 0x8E2B24 */);
        if (rwfexist(pathBuf) != 0)
        {
            RwStream *stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, pathBuf);
            if (stream == NULL)
                return NULL;
            RwRaster *raster = RwD3D9RasterStreamReadDDS(stream);
            RwStreamClose(stream, NULL);
            if (raster == NULL)
                return NULL;
            texture = RwTextureCreate(raster);
            if (texture == NULL)
                return NULL;
            if ((RwInt8)raster->cFormat > -1) /* raster+0x23 < 0x80 */
                *(RwUInt8 *)&texture->filterAddressing = 2;
            else
                *(RwUInt8 *)&texture->filterAddressing = 6;
            RwTextureSetName(texture, fileName);
            return texture;
        }
    }
    else
    {
        /* Walk the ';'-separated search path */
        RwChar *entry = searchPath;
        RwChar *nextEntry;
        do
        {
            if (*entry == '\0')
                break;
            RwChar *sep = eng->stringFuncs.vecStrchr(entry, 0x3b /* ';' */);
            RwUInt32 entryLen;
            if (sep == NULL)
            {
                entryLen = eng->stringFuncs.vecStrlen(entry);
                nextEntry = NULL;
            }
            else
            {
                entryLen = (RwUInt32)(sep - entry);
                nextEntry = sep + 1;
            }
            {
                /* copy the path entry, then append the name and the suffix */
                RwChar *dst = pathBuf;
                RwChar *src = entry;
                RwUInt32 i;
                for (i = entryLen >> 2; i != 0; i--)
                {
                    *(RwUInt32 *)dst = *(RwUInt32 *)src;
                    src += 4;
                    dst += 4;
                }
                for (i = entryLen & 3; i != 0; i--)
                    *dst++ = *src++;
            }
            eng->stringFuncs.vecStrcpy(pathBuf + entryLen, (RwChar *)name);
            eng->stringFuncs.vecStrcat(pathBuf, ".dds" /* 0x8E2B24 */);
            if (rwfexist(pathBuf) != 0)
            {
                RwStream *stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, pathBuf);
                if (stream == NULL)
                    return NULL;
                RwRaster *raster = RwD3D9RasterStreamReadDDS(stream);
                RwStreamClose(stream, NULL);
                if (raster == NULL)
                    return NULL;
                texture = RwTextureCreate(raster);
                if (texture == NULL)
                    return NULL;
                if ((RwInt8)raster->cFormat > -1) /* raster+0x23 < 0x80 */
                    *(RwUInt8 *)&texture->filterAddressing = 2;
                else
                    *(RwUInt8 *)&texture->filterAddressing = 6;
                RwTextureSetName(texture, (RwChar *)name);
                return texture;
            }
            entry = nextEntry;
        } while (nextEntry != NULL);
    }

    /* Not found */
    {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl *)(RwInt32, ...))0x8088D0)(0x16, name, "(null)" /* 0x8E240C */);
        RwErrorSet(&rwError);
    }
    return NULL;
}


RwBool RwD3D9RasterIsCompressed(const RwRaster *raster)
{
    return ((((const RwUInt8 *)raster)[g_rwD3D9RasterExtOffset + 10] & 0xF0) != 0);
}


struct _rwD3D9StreamCacheEntry {          // кэш RwD3D9SetStreamSource, 0x10 байт на стрим (декомпил 0x7FA030/0x7F9D50)
    void*    vertexBuffer;                // +0x00 как передал вызывающий (IDirect3DVertexBuffer9*)
    RwUInt32 offset;                      // +0x04
    RwUInt32 stride;                      // +0x08
};
struct _rwD3D9LightEntry {                // = RxD3D9Light (app/platform/win/WindowedMode.cpp:56), шаг массива 0x6C (декомпил 0x7FA660)
    D3DLIGHT9 light;                      // +0x00 (0x68)
    RwBool    enabled;                    // +0x68
};
struct _rwD3D9VertexDeclCacheEntry {      // 0x0C байт, кэш вершинных деклараций (декомпил 0x7FAA30)
    void*              decl;              // +0x00 IDirect3DVertexDeclaration9*
    RwUInt32           numElements;       // +0x04, включая элемент D3DDECL_END
    D3DVERTEXELEMENT9* elements;          // +0x08 копия в куче (numElements * 8 байт; sizeof(D3DVERTEXELEMENT9)==8)
};
struct _rwD3D9RasterExtView {             // read-only вид d3d9-расширения растра; ПОЛНАЯ раскладка:
                                          // struct _rwD3D9RasterExt @ app/platform/win/WindowedMode.cpp:66
    void*   texture;                      // +0x00 IDirect3DTexture9* / IDirect3DCubeTexture9*
    void*   palette;                      // +0x04
    RwUInt8 alpha;                        // +0x08
    RwUInt8 cubeFaceBits;                 // +0x09: cube:4 (младший ниббл) | face:4 (старший ниббл)
};

static inline auto& _rwD3D9StencilClearValue      = StaticRef<RwUInt32>(0xC97C44);   // дубль WindowedMode.cpp:106
static inline auto& _rwD3D9RenderTargetCacheExtra = StaticRef<void*>(0xC9808C);     // зануляется вместе с RT-кэшем (0x7F9D50); точное назначение не установлено
static inline auto& _rwD3D9RenderTargetCache      = StaticRef<void*[4]>(0xC98090);  // активные RT-поверхности по индексу (0x7F9E80/0x7F9E90)
static inline auto& _rwD3D9StreamCache            = StaticRef<_rwD3D9StreamCacheEntry[4]>(0xC97BD8);
static inline auto& _rwD3D9WorldTransformIsIdentity = StaticRef<RwBool>(0xC97C68);
static inline auto& _rwD3D9VertexDeclCacheCount   = StaticRef<RwUInt32>(0xC97C58);
static inline auto& _rwD3D9VertexDeclCacheCap     = StaticRef<RwUInt32>(0xC97C5C);
static inline auto& _rwD3D9VertexDeclCache        = StaticRef<_rwD3D9VertexDeclCacheEntry*>(0xC97C60);
static inline auto& _rwD3D9TransformFreeList      = StaticRef<RwFreeList*>(0xC98080);
static inline auto& _rwD3D9CurrentWorldTransform  = StaticRef<D3DMATRIX*>(0xC98070);
static inline auto& LightsCache                   = StaticRef<_rwD3D9LightEntry*>(0xC98088); // дубль WindowedMode.cpp:129
static inline auto& MaxNumLights                  = StaticRef<RwInt32>(0xC98084);            // дубль WindowedMode.cpp:130
static inline auto& _rwD3D9IdentityMatrix         = StaticRef<const D3DMATRIX>(0x8847A0);    // .rdata-константа (16 float'ов)
static inline auto& _rwD3D9LastWorldMatrixScratch = StaticRef<D3DMATRIX>(0x8E24D0);          // последний мировой, сконвертированный в D3D-раскладку
static inline auto& _RwDirect3DObject             = StaticRef<LPDIRECT3D9>(0xC97C20);        // дубль WindowedMode.cpp:114
static inline auto& _RwD3DAdapterIndex            = StaticRef<RwUInt32>(0xC97C24);           // дубль WindowedMode.cpp:115
static inline auto& _RwD3DAdapterType             = StaticRef<D3DDEVTYPE>(0x8E2428);         // дубль WindowedMode.cpp:116
static inline auto& _RwD3DAdapterFormat           = StaticRef<D3DFORMAT>(0xC9BEF0);          // формат адаптера, аргумент 3 CheckDeviceFormat
static inline auto& _RwD3D9RasterExtOffset        = StaticRef<RwInt32>(0xB4E9E0);            // дубль WindowedMode.cpp:105

// Доступ к памяти-диспетчу RwGlobals по ФАКТИЧЕСКИМ офсетам бинара GTA SA
// (stock-SDK RwGlobals из rwplcore.h:6280 бинару НЕ соответствует; сверено:
//  RwGlobalsSA с static_assert'ами @ rwplcore.cpp:69-95 и декомпил RwEngineInit 0x7F3170).
// Аналогичные макросы уже есть в rwplcore.cpp:38-40 (другая TU).
#define RWGTA_MEMFUNCS(g)       (*reinterpret_cast<RwMemoryFunctions*>(reinterpret_cast<RwUInt8*>(g) + 0x134))
#define RWGTA_FREELIST_ALLOC(g) (*reinterpret_cast<RwMemoryAllocFn*>(reinterpret_cast<RwUInt8*>(g) + 0x144))
#define RWGTA_FREELIST_FREE(g)  (*reinterpret_cast<RwMemoryFreeFn*>(reinterpret_cast<RwUInt8*>(g) + 0x148))
// [rwcore-5 SHARED BLOCK END]


RwBool RwD3D9DeviceSupportsDXTTexture() {
    // Декомпил 0x7F9C30: IDirect3D9::CheckDeviceFormat(vtable+0x28) с FOURCC 'DXT1'..'DXT5';
    // Usage=0, RType=3 (D3DRTYPE_TEXTURE); любой успех -> TRUE, все провалились -> FALSE.
    static const RwUInt32 dxtFourccs[5] = {
        0x31545844u, // 'DXT1'
        0x32545844u, // 'DXT2'
        0x33545844u, // 'DXT3'
        0x34545844u, // 'DXT4'
        0x35545844u, // 'DXT5'
    };
    for (const RwUInt32 fourcc : dxtFourccs) {
        if (SUCCEEDED(_RwDirect3DObject->CheckDeviceFormat(_RwD3DAdapterIndex, _RwD3DAdapterType, _RwD3DAdapterFormat, 0, D3DRTYPE_TEXTURE, (D3DFORMAT)fourcc))) {
            return TRUE;
        }
    }
    return FALSE;
}


// кэш трансформаций: массив из 260 указателей на D3DMATRIX (0x410 байт), слоты выдаются
// из freelist'а по требованию (декомпил: (&DAT_00c97c70)[state], зануление 0x104 dword'ов)
static inline auto& _rwD3D9TransformCache         = StaticRef<D3DMATRIX*[260]>(0xC97C70);

#define RWGTA_MEMFUNCS(g)        (*reinterpret_cast<RwMemoryFunctions*>(reinterpret_cast<RwUInt8*>(g) + 0x134))
#define RWGTA_FREELIST_ALLOC(g)  (*reinterpret_cast<RwMemoryAllocFn*>(reinterpret_cast<RwUInt8*>(g) + 0x144))
#define RWGTA_FREELIST_FREE(g)   (*reinterpret_cast<RwMemoryFreeFn*>(reinterpret_cast<RwUInt8*>(g) + 0x148))
// [rwcore-5 SHARED BLOCK END]


void* RwD3D9GetCurrentD3DDevice() {
    // Декомпил 0x7F9D50: сброс всех кэшей драйвера (вызывается при (re)создании девайса),
    // возврат IDirect3DDevice9* (DAT_00c97c28 = _RwD3DDevice, rwcore.h:3635).
    _rwD3D9RenderTargetCache[0] = nullptr;
    _rwD3D9RenderTargetCache[1] = nullptr;
    _rwD3D9RenderTargetCache[2] = nullptr;
    _rwD3D9RenderTargetCache[3] = nullptr;
    _rwD3D9RenderTargetCacheExtra = nullptr;
    // _rwD3D9Last* — готовые глобалы из rwcore.h:3636-3640
    _rwD3D9LastFVFUsed               = 0xFFFFFFFFu;
    _rwD3D9LastVertexDeclarationUsed = (void*)0xFFFFFFFF;
    _rwD3D9LastVertexShaderUsed      = (void*)0xFFFFFFFF;
    _rwD3D9LastPixelShaderUsed       = (void*)0xFFFFFFFF;
    _rwD3D9LastIndexBufferUsed       = (void*)0xFFFFFFFF;
    for (RwUInt32 i = 0; i < 4; i++) {
        _rwD3D9StreamCache[i].vertexBuffer = (void*)0xFFFFFFFF;
        _rwD3D9StreamCache[i].offset       = 0;
        _rwD3D9StreamCache[i].stride       = 0;
    }
    if (_rwD3D9TransformFreeList == nullptr) {
        // FREELIST ещё не создан: _rwFreeListCreate(0x40, 0xF, 0x10, hint=0x30411)
        // (обёртка rwplcore.cpp:198 имеет 2 доп. аргумента file/line — в бинаре их нет)
        // binary ABI of 0x801980: 4 args (no file/line) — raw cast, rwplcore wrapper has the RWDEBUG form
        _rwD3D9TransformFreeList = ((RwFreeList*(__cdecl*)(RwInt32, RwInt32, RwInt32, RwUInt32))0x801980)(0x40, 0xF, 0x10, 0x30411);
        for (auto& slot : _rwD3D9TransformCache) {
            slot = nullptr;
        }
    } else {
        // возврат всех выданных матриц во freelist (+0x148 = RwMemoryFreeFn) и purge (0x801E00)
        for (auto& slot : _rwD3D9TransformCache) {
            if (slot != nullptr) {
                RWGTA_FREELIST_FREE(RwEngineInstance)(_rwD3D9TransformFreeList, slot);
                slot = nullptr;
            }
        }
        RwFreeListPurge(_rwD3D9TransformFreeList);
    }
    // слот "текущая мировая матрица" (0xC98070): выделить и заполнить identity (16 dword'ов)
    _rwD3D9CurrentWorldTransform = static_cast<D3DMATRIX*>(RWGTA_FREELIST_ALLOC(RwEngineInstance)(_rwD3D9TransformFreeList, 0x30411));
    *_rwD3D9CurrentWorldTransform = _rwD3D9IdentityMatrix;
    // кэш источников света
    MaxNumLights = 0;
    if (LightsCache != nullptr) {
        RWGTA_MEMFUNCS(RwEngineInstance).rwfree(LightsCache);
        LightsCache = nullptr;
    }
    return _RwD3DDevice;
}


RwUInt32 RwD3D9EngineGetMaxMultiSamplingLevels() {
    return r4MaxMultiSampling;
}


void RwD3D9EngineSetMultiSamplingLevels(RwUInt32 numLevels) {
    if (r4MaxMultiSampling < numLevels) {
        numLevels = r4MaxMultiSampling;
    }
    if (numLevels < 2) {
        r4MultiSampleType    = 1;
        r4MultiSampleQuality = 1;
        return;
    }
    RwUInt32 windowed = ~r4DisplayWindowed & 1;
    r4MultiSampleType = numLevels;
    do {
        if (SUCCEEDED(r4D3D9->CheckDeviceMultiSampleType(r4D3D9Adapter,
                                                         (D3DDEVTYPE)StaticRef<DWORD>(0x8E2428),
                                                         r4DisplayFormat,
                                                         windowed & 1,
                                                         (D3DMULTISAMPLE_TYPE)r4MultiSampleType,
                                                         nullptr))) {
            break;
        }
        r4MultiSampleType = r4MultiSampleType - 1;
    } while (r4MultiSampleType > 1);
    r4MultiSampleQuality = (r4MultiSampleQualityN * numLevels) / r4MaxMultiSampling;
}


void RwD3D9EngineSetRefreshRate(RwUInt32 refreshRate) {
    r4RefreshRate = refreshRate;
    if (refreshRate != 0) {
        RwUInt32 result = 0;   // exact-match marker / chosen rate
        RwUInt32 best   = 0;   // highest rate below the request, carried across formats
        for (RwUInt32* fmt = &StaticRef<RwUInt32>(0x884788); reinterpret_cast<uintptr_t>(fmt) < 0x884794; fmt++) {
            RwUInt32 modeCount = r4D3D9->GetAdapterModeCount(r4D3D9Adapter, (D3DFORMAT)*fmt);
            best = result;
            while (true) {
                result = best;
                if (modeCount == 0) {
                    break;
                }
                modeCount--;
                D3DDISPLAYMODE dm;
                r4D3D9->EnumAdapterModes(r4D3D9Adapter, (D3DFORMAT)*fmt, modeCount, &dm);
                result = refreshRate;
                if (dm.RefreshRate == refreshRate) {
                    break;
                }
                if (dm.RefreshRate < refreshRate && best < dm.RefreshRate) {
                    best = dm.RefreshRate;
                }
            }
        }
        if (r4RefreshRate != result) {
            r4RefreshRate = result;
        }
    }
}


void RwD3D9EngineSetMultiThreadSafe(RwBool enable) {
    r4MultiThreadSafe = enable;
}


void RwD3D9EngineSetSoftwareVertexProcessing(RwBool enable) {
    r4SoftwareVP = enable;
}




void* RwD3D9GetCurrentD3DRenderTarget(RwUInt32 index) {
    // Декомпил 0x7F9E80 (12 байт): return (&DAT_00c98090)[param_1] — кэш активных RT-поверхностей.
    // Индекс не проверяется (4 simultaneously render targets).
    return _rwD3D9RenderTargetCache[index];
}




RwBool RwD3D9SetRenderTarget(RwUInt32 index, RwRaster* raster) {
    // Декомпил 0x7F9E90: поверхность берётся из d3d9-расширения РОДИТЕЛЬСКОГО растра
    // (raster->parent, первый член RwRaster, rwcore.h:1624; расширение лежит на
    // parent + _RwD3D9RasterExtOffset). cube==0 -> IDirect3DTexture9::GetSurfaceLevel(0),
    // иначе IDirect3DCubeTexture9::GetCubeMapSurface(face, 0) (оба на vtable+0x48,
    // сверено с d3d9.h). HRESULT от Get* игнорируется (как в декомпиле).
    IDirect3DSurface9* surface = nullptr;
    if (raster != nullptr) {
        const RwRaster*          parentRaster = raster->parent;
        const _rwD3D9RasterExtView* ext       = reinterpret_cast<const _rwD3D9RasterExtView*>(reinterpret_cast<const RwUInt8*>(parentRaster) + _RwD3D9RasterExtOffset);
        if ((ext->cubeFaceBits & 0xF) == 0) {
            reinterpret_cast<IDirect3DTexture9*>(ext->texture)->GetSurfaceLevel(0, &surface);
        } else {
            reinterpret_cast<IDirect3DCubeTexture9*>(ext->texture)->GetCubeMapSurface((D3DCUBEMAP_FACES)(ext->cubeFaceBits >> 4), 0, &surface);
        }
    }
    RwBool result;
    if (_rwD3D9RenderTargetCache[index] == surface) {
        result = TRUE;
    } else {
        _rwD3D9RenderTargetCache[index] = surface;
        result = SUCCEEDED(_RwD3DDevice->SetRenderTarget(index, surface)); // vtable+0x94
    }
    if (surface != nullptr) {
        surface->Release(); // отпустить ссылку от GetSurfaceLevel/GetCubeMapSurface
    }
    return result;
}


RwBool RwD3D9ChangeVideoMode(RwInt32 modeIndex) {
    if (r4DeviceOpen == 0) {
        return TRUE;
    }
    if (modeIndex < 0) {
        return TRUE;
    }
    if ((RwUInt32)modeIndex >= r4VideoModeCount) {
        return TRUE;
    }
    if (r4CurrentModeIndex == modeIndex) {
        return TRUE;
    }

    D3DPRESENT_PARAMETERS savedPP = r4PresentParams;   // 14 dwords @0xC9C040

    ((void(__cdecl*)(void))0x7F7F70)();                // release device resources
    _RwD3DDevice->TestCooperativeLevel();              // result discarded (decompile)

    RwD3D9ModeEntryR4& entry = r4VideoModes[modeIndex];
    ((void(__cdecl*)(const void*, RwUInt32, void*))0x7F6CB0)(&entry, entry.windowed,
                                                             StaticRef<void*>(0xC97BD4));

    if (SUCCEEDED(_RwD3DDevice->Reset(&r4PresentParams))) {
        r4RenderTargetScratch = nullptr;
        _RwD3DDevice->GetRenderTarget(0, &r4RenderTargetScratch);
        r4RenderTargetScratch->Release();
        r4BackBufferScratch = nullptr;
        _RwD3DDevice->GetDepthStencilSurface(&r4BackBufferScratch);
        r4BackBufferScratch->Release();

        bool ok = false;
        if (((RwBool(__cdecl*)(void))0x4CC970)() != 0 && ((RwBool(__cdecl*)(void))0x7F58D0)() != 0) {
            ((void(__cdecl*)(void))0x7FD100)();
            HRESULT hr = S_OK;
            for (RwUInt32 i = 0; i < r4LightCacheCount; i++) {
                _RwD3DDevice->SetLight(i, &r4LightCache[i].light);
                hr = _RwD3DDevice->LightEnable(i, r4LightCache[i].enabled);
            }
            if (SUCCEEDED(hr) && ((RwBool(__cdecl*)(void))0x7FB480)() != 0 && ((RwBool(__cdecl*)(void))0x80E020)() != 0) {
                ok = true;
            }
        }

        if (r4DeviceRestoreCb != nullptr) {
            r4DeviceRestoreCb();
        }

        if (ok) {
            r4CurrentModeIndex = modeIndex;
            RwUInt32 width     = entry.width;
            RwUInt32 height    = entry.height;
            r4ModeField0C      = entry.field08;
            r4DisplayFormat    = entry.format;
            r4DisplayWindowed  = entry.windowed;
            switch (entry.format) {
            case D3DFMT_A8R8G8B8:     // 21
            case D3DFMT_X8R8G8B8:     // 22
            case D3DFMT_A2R10G10B10:  // 35
                r4DisplayBpp = 0x20;
                break;
            case D3DFMT_R5G6B5:       // 23
            case D3DFMT_X1R5G5B5:     // 24
            case D3DFMT_A1R5G5B5:     // 25
                r4DisplayBpp = 0x10;
                break;
            default:
                r4DisplayBpp = 0;
                break;
            }
            r4DisplayWidth  = width;
            r4DisplayHeight = height;

            if (r4PresentWindowed != 0) {
                SetWindowLongA(r4DeviceWindow, GWL_STYLE, 0x10CF0000);
                height = r4PresentParams.BackBufferHeight;
                width  = r4PresentParams.BackBufferWidth;
            }
            {
                DWORD exStyle = (DWORD)GetWindowLongA(r4DeviceWindow, GWL_EXSTYLE);
                BOOL  hasMenu = (GetMenu(r4DeviceWindow) != NULL);
                DWORD style   = (DWORD)GetWindowLongA(r4DeviceWindow, GWL_STYLE);
                RECT  rect;
                rect.left   = 0;                       // NB: rect init not visible in the decompile
                rect.top    = 0;                       //     (see header note); standard idiom applied
                rect.right  = (LONG)width;
                rect.bottom = (LONG)height;
                AdjustWindowRectEx(&rect, style, hasMenu, exStyle);
                SetWindowPos(r4DeviceWindow, NULL, 0, 0, (int)width, (int)height,
                             (r4PresentWindowed != 0) ? (SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE)  // 0x26
                                                      : (SWP_NOMOVE | SWP_NOZORDER));                // 0x06
                if (r4PresentWindowed != 0) {
                    GetClientRect(r4DeviceWindow, &rect);
                }
                r4DisplayWidth  = width;
                r4DisplayHeight = height;
            }
            return TRUE;
        }

        ((void(__cdecl*)(void))0x7F7F70)();            // release again before the restore attempt
    }

    // failure: put the old present params back and Reset with them
    r4PresentParams = savedPP;
    if (FAILED(_RwD3DDevice->Reset(&r4PresentParams))) {
        return FALSE;
    }
    r4RenderTargetScratch = nullptr;
    _RwD3DDevice->GetRenderTarget(0, &r4RenderTargetScratch);
    r4RenderTargetScratch->Release();
    r4BackBufferScratch = nullptr;
    _RwD3DDevice->GetDepthStencilSurface(&r4BackBufferScratch);
    r4BackBufferScratch->Release();
    if (((RwBool(__cdecl*)(void))0x4CC970)() != 0 && ((RwBool(__cdecl*)(void))0x7F58D0)() != 0) {
        ((void(__cdecl*)(void))0x7FD100)();
        HRESULT hr = S_OK;
        for (RwUInt32 i = 0; i < r4LightCacheCount; i++) {
            _RwD3DDevice->SetLight(i, &r4LightCache[i].light);
            hr = _RwD3DDevice->LightEnable(i, r4LightCache[i].enabled);
        }
        if (SUCCEEDED(hr) && ((RwBool(__cdecl*)(void))0x7FB480)() != 0) {
            ((RwBool(__cdecl*)(void))0x80E020)();
        }
    }
    if (r4DeviceRestoreCb == nullptr) {
        return TRUE;
    }
    r4DeviceRestoreCb();
    return TRUE;
}


RwBool RwD3D9ChangeMultiSamplingLevels(RwUInt32 numLevels) {
    RwUInt32 savedType    = r4MultiSampleType;     // 0x8E2430
    RwUInt32 savedQuality = r4MultiSampleQuality;  // 0x8E2438
    if (r4DeviceOpen == 0) {
        return TRUE;
    }
    if (r4MultiSampleType == numLevels) {
        return TRUE;
    }

    D3DPRESENT_PARAMETERS savedPP = r4PresentParams;

    // pick the new multisample type/quality (same computation as 0x7F84F0)
    if (r4MaxMultiSampling < numLevels) {
        numLevels = r4MaxMultiSampling;
    }
    if (numLevels < 2) {
        r4MultiSampleQuality = 1;
        r4MultiSampleType    = 1;
    } else {
        RwUInt32 windowed = ~r4DisplayWindowed & 1;
        r4MultiSampleType = numLevels;
        do {
            if (SUCCEEDED(r4D3D9->CheckDeviceMultiSampleType(r4D3D9Adapter,
                                                             (D3DDEVTYPE)StaticRef<DWORD>(0x8E2428),
                                                             r4DisplayFormat,
                                                             windowed & 1,
                                                             (D3DMULTISAMPLE_TYPE)r4MultiSampleType,
                                                             nullptr))) {
                break;
            }
            r4MultiSampleType = r4MultiSampleType - 1;
        } while (r4MultiSampleType > 1);
        r4MultiSampleQuality = (r4MultiSampleQualityN * numLevels) / r4MaxMultiSampling;
    }

    ((void(__cdecl*)(void))0x7F7F70)();
    _RwD3DDevice->TestCooperativeLevel();              // result discarded (decompile)
    ((void(__cdecl*)(const void*, RwUInt32, void*))0x7F6CB0)(&r4DisplayWidth, r4DisplayWindowed,
                                                             StaticRef<void*>(0xC97BD4));

    if (SUCCEEDED(_RwD3DDevice->Reset(&r4PresentParams))) {
        r4RenderTargetScratch = nullptr;
        _RwD3DDevice->GetRenderTarget(0, &r4RenderTargetScratch);
        r4RenderTargetScratch->Release();
        r4BackBufferScratch = nullptr;
        _RwD3DDevice->GetDepthStencilSurface(&r4BackBufferScratch);
        r4BackBufferScratch->Release();

        bool ok = false;
        if (((RwBool(__cdecl*)(void))0x4CC970)() != 0 && ((RwBool(__cdecl*)(void))0x7F58D0)() != 0) {
            ((void(__cdecl*)(void))0x7FD100)();
            HRESULT hr = S_OK;
            for (RwUInt32 i = 0; i < r4LightCacheCount; i++) {
                _RwD3DDevice->SetLight(i, &r4LightCache[i].light);
                hr = _RwD3DDevice->LightEnable(i, r4LightCache[i].enabled);
            }
            if (SUCCEEDED(hr) && ((RwBool(__cdecl*)(void))0x7FB480)() != 0 && ((RwBool(__cdecl*)(void))0x80E020)() != 0) {
                ok = true;
            }
        }

        if (r4DeviceRestoreCb != nullptr) {
            r4DeviceRestoreCb();
        }

        if (ok) {
            return TRUE;
        }

        ((void(__cdecl*)(void))0x7F7F70)();
    }

    // failure: restore old present params and multisampling slots, Reset again
    r4PresentParams      = savedPP;
    r4MultiSampleType    = savedType;
    r4MultiSampleQuality = savedQuality;
    if (FAILED(_RwD3DDevice->Reset(&r4PresentParams))) {
        return FALSE;
    }
    r4RenderTargetScratch = nullptr;
    _RwD3DDevice->GetRenderTarget(0, &r4RenderTargetScratch);
    r4RenderTargetScratch->Release();
    r4BackBufferScratch = nullptr;
    _RwD3DDevice->GetDepthStencilSurface(&r4BackBufferScratch);
    r4BackBufferScratch->Release();
    if (((RwBool(__cdecl*)(void))0x4CC970)() != 0 && ((RwBool(__cdecl*)(void))0x7F58D0)() != 0) {
        ((void(__cdecl*)(void))0x7FD100)();
        HRESULT hr = S_OK;
        for (RwUInt32 i = 0; i < r4LightCacheCount; i++) {
            _RwD3DDevice->SetLight(i, &r4LightCache[i].light);
            hr = _RwD3DDevice->LightEnable(i, r4LightCache[i].enabled);
        }
        if (SUCCEEDED(hr) && ((RwBool(__cdecl*)(void))0x7FB480)() != 0) {
            ((RwBool(__cdecl*)(void))0x80E020)();
        }
    }
    if (r4DeviceRestoreCb != nullptr) {
        r4DeviceRestoreCb();
    }
    return TRUE;
}


// Verified from disassembly (stack-frame analysis; see keep/rw_campaign/0x7F8D70.disasm.txt)
RwBool RwD3D9CameraAttachWindow(void* camera, void* hwnd) {
    // 0x7F8D70 mov ecx,[0xC9C060]; 0x7F8D79 xor eax,eax (default FALSE);
    // 0x7F8D80-0x7F8D82 cmp/je -> return FALSE if template.Windowed == 0
    if (*(RwUInt32*)0xC9C060 == 0) // 0xC9C040+0x20 = present-params template .Windowed
        return FALSE;

    // 0x7F8D8F-0x7F8D97: if (current driver window == hwnd) return FALSE
    if (*(void**)0xC97C1C == hwnd)
        return FALSE;

    // 0x7F8D9D-0x7F8DA9: GetWindowRect(hwnd, &rect); rect lives at L+0x10.
    // Return value ignored. (ebx = hwnd for the rest of the function.)
    RECT rect;
    GetWindowRect((HWND)hwnd, &rect);

    RwUInt8* cam = (RwUInt8*)camera; // 0x7F8DA9 ebp = arg1 ([esp+0x7C])

    // 0x7F8DAD-0x7F8DBD: if (camera+0x60) { raster=it; camera+0x60=NULL; destroy(raster); }
    // 0x7F8DC0-0x7F8DD0: same for camera+0x64   (0x7FB020 cdecl, add esp,4)
    if (*(void**)(cam + 0x60) != nullptr) {
        void* old = *(void**)(cam + 0x60);
        *(void**)(cam + 0x60) = nullptr;
        ((void(__cdecl*)(void*))0x7FB020)(old);
    }
    if (*(void**)(cam + 0x64) != nullptr) {
        void* old = *(void**)(cam + 0x64);
        *(void**)(cam + 0x64) = nullptr;
        ((void(__cdecl*)(void*))0x7FB020)(old);
    }

    // 0x7F8DD3 edx = rect.left ([L+0x10], loaded BEFORE push edi);
    // 0x7F8DD7 push edi (esp := M); 0x7F8DD8-0x7F8DEB rep movsd: copy 14 dwords
    // from 0xC9C040 to L+0x40 ([M+0x44]); eax = *(void**)0xC97C28 (device).
    D3DPRESENT_PARAMETERS presentParams;
    memcpy(&presentParams, (const void*)0xC9C040, 14 * 4);

    // 0x7F8DED-0x7F8E01 + 0x7F8E06-0x7F8E0E, the neg/sbb/inc idiom:
    //   sub ecx,edx  -> d = rect.right - rect.left   (magnitude is DEAD)
    //   neg ecx      -> CF = (d != 0)
    //   sbb ecx,ecx  -> ecx = (d != 0) ? -1 : 0
    //   inc ecx      -> ecx = (d != 0) ? 0 : 1  ==  (rect.right == rect.left) ? 1 : 0
    // Same for edx over rect.bottom - rect.top. CONFIRMED: result is ONLY the
    // 0/1 boolean (a==b)?1:0 -- NOT the rect extent, NOT max(1,extent).
    presentParams.BackBufferWidth  = (rect.right == rect.left) ? 1 : 0;  // 0x7F8E0A [M+0x44]
    presentParams.BackBufferHeight = (rect.bottom == rect.top) ? 1 : 0;  // 0x7F8E17 [M+0x48]
    presentParams.hDeviceWindow    = (HWND)hwnd;                         // 0x7F8E02 [M+0x60]
    presentParams.EnableAutoDepthStencil = FALSE;                        // 0x7F8E0F [M+0x68]

    // 0x7F8E1B-0x7F8E2E: device->CreateAdditionalSwapChain (slot 13 = [v+0x34],
    // stdcall, callee cleans). Arg1 = &presentParams (lea [M-4+0x48] = L+0x40),
    // arg2 = &swapChain (lea [M+0x10] = L+0x0C). HRESULT != 0 -> pop edi; return FALSE.
    void* device = *(void**)0xC97C28; // _RwD3DDevice
    void* swapChain = nullptr;        // lands at L+0x0C
    HRESULT hr = ((HRESULT(__stdcall*)(void*, void*, void**))(*(void***)device)[0x34 / 4])(
        device, &presentParams, &swapChain);
    if (hr != 0)
        return FALSE; // 0x7F8F0F xor eax,eax; pops; ret

    // 0x7F8E34-0x7F8E4A: swapChain->GetBackBuffer (slot 5 = [v+0x14], 3 args).
    // Out slot is lea [L+0x80] -- the incoming ARG2 SLOT reused as scratch
    // (hwnd is already safe in ebx). eax = resulting IDirect3DSurface9*.
    IDirect3DSurface9* backBuffer = nullptr;
    ((HRESULT(__stdcall*)(void*, RwUInt32, RwUInt32, void**))(*(void***)swapChain)[0x14 / 4])(
        swapChain, 0 /*iBackBuffer*/, 0 /*D3DBACKBUFFER_TYPE_MONO*/, (void**)&backBuffer);

    // 0x7F8E51-0x7F8E59: backBuffer->GetDesc (slot 12 = [v+0x30], 1 arg).
    // Desc buffer at L+0x20: Format@L+0x20, Width@L+0x38, Height@L+0x3C.
    // Return value ignored.
    D3DSURFACE_DESC desc;
    ((HRESULT(__stdcall*)(void*, void*))(*(void***)backBuffer)[0x30 / 4])(backBuffer, &desc);

    // 0x7F8E5C-0x7F8E72: raster create #1, cdecl 4 args. Push order
    // 0x82, 0, eax=desc.Height, ecx=desc.Width  =>  (width, height, depth=0, flags=0x82).
    // 0x82 = rwRASTERDONTALLOCATE|rwRASTERTYPECAMERA -> this is the CAMERA raster.
    // (0x7FB230 = RwRasterCreate(width, height, depth, flags); arg3 is ALWAYS 0,
    //  arg4 is the type/flags dword: 0x82 here, 1 at the second call site.)
    RwRaster* rasterCam = (RwRaster*)((RwRaster * (__cdecl*)(RwInt32, RwInt32, RwInt32, RwInt32))0x7FB230)(
        (RwInt32)desc.Width, (RwInt32)desc.Height, 0, 0x82);
    if (rasterCam != nullptr) { // 0x7F8E75-0x7F8E77
        // 0x7F8E79-0x7F8E84: raster->width (+0x0C) = desc.Width; ->height (+0x10) = desc.Height
        *(RwInt32*)((RwUInt8*)rasterCam + 0x0C) = (RwInt32)desc.Width;
        *(RwInt32*)((RwUInt8*)rasterCam + 0x10) = (RwInt32)desc.Height;
        // 0x7F8E87-0x7F8E94: fmtIdx = [L+0x20] = desc.Format;
        // raster->depth (+0x14, dword store of byte) = fmtTable[fmtIdx].depthBits
        const RwUInt32 fmtIdx = (RwUInt32)desc.Format;
        *(RwInt32*)((RwUInt8*)rasterCam + 0x14) = *(RwUInt8*)(fmtIdx * 4 + 0xB4E7E1);
        // 0x7F8E97-0x7F8EA2: raster->cFormat (+0x23, byte store) = fmtTable[fmtIdx].alphaBits
        *(RwUInt8*)((RwUInt8*)rasterCam + 0x23) = *(RwUInt8*)(fmtIdx * 4 + 0xB4E7E3);
        // 0x7F8EA5-0x7F8EA B: ext = (RwUInt8*)rasterCam + *(RwUInt32*)0xB4E9E0 (rasterExt offset)
        RwUInt8* ext = (RwUInt8*)rasterCam + *(RwUInt32*)0xB4E9E0;
        *(void**)(ext + 0x00) = backBuffer;                       // 0x7F8EAE-0x7F8EB5 ([L+0x80])
        *(RwUInt8*)(ext + 0x08) = *(RwUInt8*)(fmtIdx * 4 + 0xB4E7E0); // 0x7F8EB7-0x7F8EC2 (byte)
        *(RwUInt32*)(ext + 0x18) = fmtIdx;                        // 0x7F8EC5-0x7F8EC9
        *(void**)(ext + 0x20) = hwnd;                             // 0x7F8ECC (ebx)
        *(void**)(ext + 0x1C) = swapChain;                        // 0x7F8ECF-0x7F8ED3 ([L+0x0C])
        *(void**)(cam + 0x60) = rasterCam;                        // 0x7F8ED6 camera+0x60
    }

    // 0x7F8ED9-0x7F8EEC: raster create #2: push 1, 0, eax=desc.Height, ecx=desc.Width
    // => (width, height, 0, rwRASTERTYPEZBUFFER). On success camera+0x64 = raster.
    RwRaster* rasterZ = (RwRaster*)((RwRaster * (__cdecl*)(RwInt32, RwInt32, RwInt32, RwInt32))0x7FB230)(
        (RwInt32)desc.Width, (RwInt32)desc.Height, 0, 0x01);
    if (rasterZ != nullptr) // 0x7F8EEF-0x7F8EF1
        *(void**)(cam + 0x64) = rasterZ; // 0x7F8EF3

    // 0x7F8EF6-0x7F8F00: backBuffer->Release() -- slot 2 = [v+8], UNCONDITIONAL
    // (no null/HRESULT guard in the binary; the surface was AddRef'd by
    //  GetBackBuffer and its pointer is retained in ext+0x00, so this drops the
    //  local reference). stdcall, callee cleans.
    ((ULONG(__stdcall*)(void*))(*(void***)backBuffer)[2])(backBuffer);

    return TRUE; // 0x7F8F05 mov eax,1; pops; ret
}



void RwD3D9SetStreamSource(RwUInt32 streamNumber, void* streamData, RwUInt32 offset, RwUInt32 stride) {
    // Декомпил 0x7FA030: кэш 4 стримов по 0x10 байт ({vb, offset, stride}); девайс дёргается
    // только при изменении. SetStreamSource = vtable+0x190 (сверено с d3d9.h).
    _rwD3D9StreamCacheEntry& cache = _rwD3D9StreamCache[streamNumber];
    if (cache.vertexBuffer != streamData || cache.offset != offset || cache.stride != stride) {
        cache.vertexBuffer = streamData;
        cache.offset       = offset;
        cache.stride       = stride;
        _RwD3DDevice->SetStreamSource(streamNumber, static_cast<IDirect3DVertexBuffer9*>(streamData), offset, stride);
    }
}


void _rwD3D9RenderStateFlushCache() {
    if (g_d3d9RsFlushCount != 0) {
        for (RwUInt32 i = 0; i < g_d3d9RsFlushCount; i++) {
            const RwUInt32 state = g_d3d9RsDirtyQueue[i];
            const RwUInt32 cached = g_d3d9RsCache[state].value;
            const RwUInt32 sent = g_d3d9RsDeviceVals[state];
            g_d3d9RsCache[state].dirty = 0;
            if (sent != cached) {
                g_d3d9RsDeviceVals[state] = cached;
                _RwD3DDevice->SetRenderState(static_cast<D3DRENDERSTATETYPE>(state), cached);
            }
        }
        g_d3d9RsFlushCount = 0;
    }
    if (g_d3d9TssFlushCount != 0) {
        for (RwUInt32 i = 0; i < g_d3d9TssFlushCount; i++) {
            const RwUInt32 stage = g_d3d9TssQueue[i].stage;
            const RwUInt32 state = g_d3d9TssQueue[i].state;
            const RwUInt32 idx = stage * 33 + state; // decompile: stage + state + stage*32
            const RwUInt32 cached = g_d3d9TssCache[idx].value;
            const RwUInt32 sent = g_d3d9TssDeviceVals[idx];
            g_d3d9TssCache[idx].dirty = 0;
            if (sent != cached) {
                g_d3d9TssDeviceVals[idx] = cached;
                _RwD3DDevice->SetTextureStageState(stage, static_cast<D3DTEXTURESTAGESTATETYPE>(state), cached);
            }
        }
        g_d3d9TssFlushCount = 0;
    }
}




void _rwD3D9DrawIndexedPrimitiveUP(RwUInt32 primitiveType, RwUInt32 minIndex, RwUInt32 numVertices, RwUInt32 primitiveCount, const void* indexData, const void* vertexStreamZeroData, RwUInt32 VertexStreamZeroStride) {
    // Декомпил 0x7FA1F0: Draw*UP конфликтует с активным стримом 0 и индекс-буфером —
    // те сбрасываются (кэш + COM), затем flush кэша render-state (обёртка 0x7FC200,
    // addr map: _rwD3D9RenderStateFlushCache) и DrawIndexedPrimitiveUP (vtable+0x150;
    // IndexDataFormat = 0x65 = D3DFMT_INDEX16, сверено).
    _rwD3D9StreamCacheEntry& streamZero = _rwD3D9StreamCache[0];
    if (streamZero.vertexBuffer != nullptr || streamZero.offset != 0 || streamZero.stride != 0) {
        streamZero.vertexBuffer = nullptr;
        streamZero.offset       = 0;
        streamZero.stride       = 0;
        _RwD3DDevice->SetStreamSource(0, nullptr, 0, 0);
    }
    if (_rwD3D9LastIndexBufferUsed != nullptr) { // rwcore.h:3640 (0x8E2450)
        _rwD3D9LastIndexBufferUsed = nullptr;
        _RwD3DDevice->SetIndices(nullptr);       // vtable+0x1A0
    }
    _rwD3D9RenderStateFlushCache();
    _RwD3DDevice->DrawIndexedPrimitiveUP(static_cast<D3DPRIMITIVETYPE>(primitiveType), minIndex, numVertices, primitiveCount, indexData, D3DFMT_INDEX16, vertexStreamZeroData, VertexStreamZeroStride);
}




void _rwD3D9DrawPrimitiveUP(RwUInt32 primitiveType, RwUInt32 primitiveCount, const void* vertexStreamZeroData, RwUInt32 VertexStreamZeroStride) {
    // Декомпил 0x7FA290: как DrawIndexedPrimitiveUP — сброс стрима 0 и индекс-буфера
    // (DrawPrimitiveUP тоже конфликтует с SetIndices), flush, DrawPrimitiveUP (vtable+0x14C).
    _rwD3D9StreamCacheEntry& streamZero = _rwD3D9StreamCache[0];
    if (streamZero.vertexBuffer != nullptr || streamZero.offset != 0 || streamZero.stride != 0) {
        streamZero.vertexBuffer = nullptr;
        streamZero.offset       = 0;
        streamZero.stride       = 0;
        _RwD3DDevice->SetStreamSource(0, nullptr, 0, 0);
    }
    if (_rwD3D9LastIndexBufferUsed != nullptr) { // rwcore.h:3640 (0x8E2450)
        _rwD3D9LastIndexBufferUsed = nullptr;
        _RwD3DDevice->SetIndices(nullptr);       // vtable+0x1A0
    }
    _rwD3D9RenderStateFlushCache();
    _RwD3DDevice->DrawPrimitiveUP(static_cast<D3DPRIMITIVETYPE>(primitiveType), primitiveCount, vertexStreamZeroData, VertexStreamZeroStride);
}


void _rwD3D9DrawIndexedPrimitive(RwUInt32 primitiveType, RwInt32 baseVertexIndex, RwUInt32 minIndex, RwUInt32 numVertices, RwUInt32 startIndex, RwUInt32 primitiveCount) {
    // Декомпил 0x7FA320 (50 байт): только flush кэша render-state (0x7FC200, addr map:
    // _rwD3D9RenderStateFlushCache) и DrawIndexedPrimitive (vtable+0x148).
    _rwD3D9RenderStateFlushCache();
    _RwD3DDevice->DrawIndexedPrimitive(static_cast<D3DPRIMITIVETYPE>(primitiveType), baseVertexIndex, minIndex, numVertices, startIndex, primitiveCount);
}


void _rwD3D9SetVertexShaderConstant(RwUInt32 registerAddress, const void* constantData, RwUInt32 constantCount) {
    // Декомпил 0x7FACA0 (30 байт): прямой COM-вызов SetVertexShaderConstantF (vtable+0x178,
    // сверено с d3d9.h), без кэша и без проверки HRESULT.
    _RwD3DDevice->SetVertexShaderConstantF(registerAddress, static_cast<const float*>(constantData), constantCount);
}





void _rwD3D9SetPixelShaderConstant(RwUInt32 registerAddress, const void* constantData, RwUInt32 constantCount) {
    /* Декомпил: vtable-вызов IDirect3DDevice9 +0x1B4 = SetPixelShaderConstantF (слот 109).
       Калибровка vtable по 7 якорям этого же бинарника (все точно по d3d9.h):
       SetFVF=89→0x164, SetVertexShader=92→0x170, SetVertexShaderConstantF=94→0x178,
       DrawIndexedPrimitive=82→0x148, CreatePixelShader=106→0x1A8, SetStreamSource=100→0x190,
       SetPixelShaderConstantF=109→0x1B4. */
    _RwD3DDevice->SetPixelShaderConstantF(registerAddress, static_cast<const float*>(constantData), constantCount);
}


void _rwD3D9SetFVF(RwUInt32 fvf) {
    // Декомпил 0x7F9F30: кэш обновляется ПОСЛЕ вызова с проверкой HRESULT —
    // в отличие от макроса _rwD3D9SetFVFMacro из rwcore.h:3739 (там кэш до вызова);
    // декомпил главнее. SetFVF = vtable+0x164 (сверено с d3d9.h).
    if (_rwD3D9LastFVFUsed != fvf) {
        const HRESULT hr = _RwD3DDevice->SetFVF(fvf);
        _rwD3D9LastFVFUsed = 0xFFFFFFFFu;
        if (hr >= 0) {
            _rwD3D9LastFVFUsed = fvf;
        }
        _rwD3D9LastVertexDeclarationUsed = (void*)0xFFFFFFFF; // инвалидирует кэш декларации
    }
}


void _rwD3D9SetVertexShader(void* shader) {
    // Декомпил 0x7F9FB0: SetVertexShader = vtable+0x170 (сверено с d3d9.h).
    if (_rwD3D9LastVertexShaderUsed != shader) {
        const HRESULT hr = _RwD3DDevice->SetVertexShader(static_cast<IDirect3DVertexShader9*>(shader));
        _rwD3D9LastVertexShaderUsed = (void*)0xFFFFFFFF;
        if (hr >= 0) {
            _rwD3D9LastVertexShaderUsed = shader;
        }
    }
}


void _rwD3D9SetPixelShader(void* shader) {
    // Декомпил 0x7F9FF0: SetPixelShader = vtable+0x1AC (сверено с d3d9.h).
    if (_rwD3D9LastPixelShaderUsed != shader) {
        const HRESULT hr = _RwD3DDevice->SetPixelShader(static_cast<IDirect3DPixelShader9*>(shader));
        _rwD3D9LastPixelShaderUsed = (void*)0xFFFFFFFF;
        if (hr >= 0) {
            _rwD3D9LastPixelShaderUsed = shader;
        }
    }
}


void RwD3D9SetRenderState(RwUInt32 state, RwUInt32 value) {
    if (g_d3d9RsCache[state].value != value) {
        g_d3d9RsCache[state].value = value;
        if (g_d3d9RsCache[state].dirty == 0) {
            g_d3d9RsCache[state].dirty = 1;
            g_d3d9RsDirtyQueue[g_d3d9RsFlushCount] = state;
            g_d3d9RsFlushCount++;
        }
    }
}


void RwD3D9GetRenderState(RwUInt32 state, void* value) {
    *static_cast<RwUInt32*>(value) = g_d3d9RsCache[state].value;
}


void RwD3D9SetTextureStageState(RwUInt32 stage, RwUInt32 type, RwUInt32 value) {
    const RwUInt32 idx = stage * 33 + type; // decompile: type + stage*32
    if (g_d3d9TssCache[idx].value != value) {
        g_d3d9TssCache[idx].value = value;
        if (g_d3d9TssCache[idx].dirty == 0) {
            g_d3d9TssCache[idx].dirty = 1;
            g_d3d9TssQueue[g_d3d9TssFlushCount].stage = stage;
            g_d3d9TssQueue[g_d3d9TssFlushCount].state = type;
            g_d3d9TssFlushCount++;
        }
    }
}


void RwD3D9GetTextureStageState(RwUInt32 stage, RwUInt32 type, void* value) {
    // decompile: *(u32*)(0xC980F8 + stage*0x42 + type*2) — dword-индексация пары {value,dirty}
    *static_cast<RwUInt32*>(value) = g_d3d9TssCache[stage * 33 + type].value;
}


void RwD3D9SetSamplerState(RwUInt32 stage, RwUInt32 type, RwUInt32 value) {
    const RwUInt32 idx = type + stage * 14; // плоский кэш 0xC98938, stride 14 dword на sampler
    if (g_d3d9SamplerCache[idx] != value) {
        g_d3d9SamplerCache[idx] = value;
        _RwD3DDevice->SetSamplerState(stage, static_cast<D3DSAMPLERSTATETYPE>(type), value);
    }
}


void RwD3D9GetSamplerState(RwUInt32 stage, RwUInt32 type, void* value) {
    *static_cast<RwUInt32*>(value) = g_d3d9SamplerCache[type + stage * 14];
}




void RwD3D9SetStencilClear(RwUInt32 stencilClear) {
    // Декомпил 0x7F9D30 (10 байт): DAT_00c97c44 = param_1.
    _rwD3D9StencilClearValue = stencilClear;
}




RwUInt32 RwD3D9GetStencilClear() {
    // Декомпил 0x7F9D40 (6 байт): return DAT_00c97c44.
    return _rwD3D9StencilClearValue;
}


RwBool RwD3D9SetTexture(RwTexture* texture, RwUInt32 stage) {
    if (texture != nullptr) {
        RwUInt32 filter = texture->filterAddressing & 0xFF;

        // U addressing
        const RwUInt32 addrU = (texture->filterAddressing >> 8) & 0xF;
        if (addrU != g_d3d9TexStageCache[stage].addrU) {
            g_d3d9TexStageCache[stage].addrU = addrU;
            const RwUInt32 d3dAddr = g_d3d9TexAddrModeTable[addrU];
            g_d3d9SamplerCache[stage * 14 + D3DSAMP_ADDRESSU] = d3dAddr;
            _RwD3DDevice->SetSamplerState(stage, D3DSAMP_ADDRESSU, d3dAddr);
        }

        // V addressing
        const RwUInt32 addrV = (texture->filterAddressing >> 12) & 0xF;
        if (addrV != g_d3d9TexStageCache[stage].addrV) {
            g_d3d9TexStageCache[stage].addrV = addrV;
            const RwUInt32 d3dAddr = g_d3d9TexAddrModeTable[addrV];
            g_d3d9SamplerCache[stage * 14 + D3DSAMP_ADDRESSV] = d3dAddr;
            _RwD3DDevice->SetSamplerState(stage, D3DSAMP_ADDRESSV, d3dAddr);
        }

        // Anisotropy (плагин RpAnisot)
        if (0 < g_d3d9AnisoTexOffset) {
            const RwUInt32 aniso = *reinterpret_cast<const RwUInt8*>(reinterpret_cast<const RwUInt8*>(texture) + g_d3d9AnisoTexOffset);
            if (g_d3d9TexStageCache[stage].aniso != aniso) {
                g_d3d9TexStageCache[stage].aniso = aniso;
                g_d3d9SamplerCache[0 * 14 + D3DSAMP_MAXANISOTROPY] = aniso; // декомпил: только stage 0
                _RwD3DDevice->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, aniso);
            }
            if (1 < aniso)
                filter = 7;
        }

        // Фильтр (min/mag + mip)
        if (g_d3d9TexStageCache[stage].filterIdx != filter) {
            const RwUInt32 minMag = g_d3d9TexFilterTable[filter].minMagFilter;
            g_d3d9TexStageCache[stage].filterIdx = filter;
            const RwUInt32 mip = g_d3d9TexFilterTable[filter].mipFilter;
            g_d3d9SamplerCache[stage * 14 + D3DSAMP_MINFILTER] = minMag;
            _RwD3DDevice->SetSamplerState(stage, D3DSAMP_MINFILTER, minMag);
            g_d3d9SamplerCache[stage * 14 + D3DSAMP_MAGFILTER] = minMag;
            _RwD3DDevice->SetSamplerState(stage, D3DSAMP_MAGFILTER, minMag);
            g_d3d9SamplerCache[stage * 14 + D3DSAMP_MIPFILTER] = mip;
            _RwD3DDevice->SetSamplerState(stage, D3DSAMP_MIPFILTER, mip);
        }

        // Привязка растера (вне этажа; сигнатура по decompile CALL: (RwRaster*, stage))
        ((RwBool(__cdecl*)(RwRaster*, RwUInt32))0x7FDCD0)(texture->raster, stage);
        return TRUE;
    }

    // texture == NULL: снять текстуру со stage
    if (stage == 0 && g_d3d9TexStage0AlphaActive != 0) {
        g_d3d9TexStage0AlphaActive = 0;
        if (g_d3d9TexStage0AlphaFlushGate == 0) {
            RwD3D9SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE); // state 0x1B
            RwD3D9SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);  // state 0x0F
        }
    }
    if (g_d3d9TexStageCache[stage].bound != 0) {
        g_d3d9TexStageCache[stage].bound = 0;
        _RwD3DDevice->SetTexture(stage, nullptr);
    }
    return TRUE;
}



#define RWGTA_FREELIST_ALLOC(g) (*reinterpret_cast<RwMemoryAllocFn*>(reinterpret_cast<RwUInt8*>(g) + 0x144))
// [rwcore-5 SHARED BLOCK END]


RwBool RwD3D9SetTransform(RwUInt32 state, const void* matrix) {
    // Декомпил 0x7FA390: per-state кэш матриц из freelist (260 слотов = весь диапазон
    // D3DTRANSFORMSTATETYPE, 0..259). Если слот свежевыделен — копия+SetTransform без
    // сравнения; иначе при равенстве 16 dword'ов — TRUE без COM-вызова.
    // SetTransform = vtable+0xB0. state==0x100 (D3DTS_WORLD) ведёт флаг
    // _rwD3D9WorldTransformIsIdentity (его читает RwD3D9SetTransformWorld 0x7FA520).
    D3DMATRIX* cached = _rwD3D9TransformCache[state];
    const bool fresh = cached == nullptr;
    if (fresh) {
        cached = static_cast<D3DMATRIX*>(RWGTA_FREELIST_ALLOC(RwEngineInstance)(_rwD3D9TransformFreeList, 0x30411));
        _rwD3D9TransformCache[state] = cached;
    }
    const D3DMATRIX& wanted = matrix != nullptr ? *static_cast<const D3DMATRIX*>(matrix) : _rwD3D9IdentityMatrix;
    if (!fresh) {
        const RwUInt32* dst = reinterpret_cast<const RwUInt32*>(cached);
        const RwUInt32* src = reinterpret_cast<const RwUInt32*>(&wanted);
        bool same = true;
        for (int i = 0; i < 16; i++) {
            if (dst[i] != src[i]) {
                same = false;
                break;
            }
        }
        if (same) {
            return TRUE;
        }
    }
    *cached = wanted;
    const HRESULT hr = _RwD3DDevice->SetTransform(static_cast<D3DTRANSFORMSTATETYPE>(state), &wanted);
    if (state == 0x100 /* D3DTS_WORLD */) {
        _rwD3D9WorldTransformIsIdentity = matrix != nullptr ? FALSE : TRUE;
    }
    return hr >= 0;
}




void RwD3D9GetTransform(RwUInt32 state, void* matrix) {
    // Декомпил 0x7FA4F0: копия 16 dword'ов из кэша; слот NULL -> identity (0x8847A0).
    if (matrix != nullptr) {
        const D3DMATRIX* src = _rwD3D9TransformCache[state];
        if (src == nullptr) {
            src = &_rwD3D9IdentityMatrix;
        }
        *static_cast<D3DMATRIX*>(matrix) = *src;
    }
}


RwBool RwD3D9SetMaterial(const void* material) {
    // Сравнение/копирование идёт 17 dword-ами (68 байт = sizeof(D3DMATERIAL9)) — как в декомпиле
    // (int-сравнение, NaN-безопасно).
    const RwUInt32* src = static_cast<const RwUInt32*>(material);
    for (RwInt32 i = 0; i < 17; i++) {
        if (reinterpret_cast<const RwUInt32*>(&g_d3d9MatCache)[i] != src[i]) {
            RwUInt32* dst = reinterpret_cast<RwUInt32*>(&g_d3d9MatCache);
            for (RwInt32 j = 0; j < 17; j++)
                dst[j] = src[j];
            g_d3d9SurfaceFlagsCache = 0; // сброс кэша RwD3D9SetSurfaceProperties (DAT_00c9a5f4)
            // декомпил передаёт девайсу param_1 (оригинальный указатель), не кэш
            return _RwD3DDevice->SetMaterial(static_cast<const D3DMATERIAL9*>(material)) >= 0;
        }
    }
    return TRUE;
}


RwBool RwD3D9SetClipPlane(RwUInt32 index, const RwV4d* plane) {
    return _RwD3DDevice->SetClipPlane(index, reinterpret_cast<const float*>(plane)) >= 0;
}




RwBool RwD3D9SetTransformWorld(const RwMatrix* matrix) {
    // Декомпил 0x7FA520: RwMatrix -> D3DMATRIX — только 3x4-часть (флаговые dword'ы
    // RwMatrix пропускаются), 4-й столбец скрэтча НЕ пишется (остаётся прежним — так в
    // декомпиле). Идентичная матрица в девайсе -> TRUE без COM-вызова.
    // matrix == NULL или rwMATRIXINTERNALIDENTITY (0x00020000, rwplcore.h:3882) в flags
    // -> считается identity. Слот текущей мировой — 0xC98070 (выделен в 0x7F9D50).
    if (matrix == nullptr || (matrix->flags & rwMATRIXINTERNALIDENTITY) != 0) {
        if (_rwD3D9WorldTransformIsIdentity == FALSE) {
            *_rwD3D9CurrentWorldTransform = _rwD3D9IdentityMatrix;
            const HRESULT hr = _RwD3DDevice->SetTransform(D3DTS_WORLD, &_rwD3D9IdentityMatrix); // vtable+0xB0
            _rwD3D9WorldTransformIsIdentity = TRUE;
            return hr >= 0;
        }
    } else {
        D3DMATRIX& scratch = _rwD3D9LastWorldMatrixScratch;
        scratch.m[0][0] = matrix->right.x; scratch.m[0][1] = matrix->right.y; scratch.m[0][2] = matrix->right.z;
        scratch.m[1][0] = matrix->up.x;    scratch.m[1][1] = matrix->up.y;    scratch.m[1][2] = matrix->up.z;
        scratch.m[2][0] = matrix->at.x;    scratch.m[2][1] = matrix->at.y;    scratch.m[2][2] = matrix->at.z;
        scratch.m[3][0] = matrix->pos.x;   scratch.m[3][1] = matrix->pos.y;   scratch.m[3][2] = matrix->pos.z;
        bool send = _rwD3D9WorldTransformIsIdentity != FALSE;
        if (!send) {
            const RwUInt32* slot = reinterpret_cast<const RwUInt32*>(_rwD3D9CurrentWorldTransform);
            const RwUInt32* cand = reinterpret_cast<const RwUInt32*>(&scratch);
            for (int i = 0; i < 16; i++) {
                if (slot[i] != cand[i]) {
                    send = true;
                    break;
                }
            }
        }
        if (send) {
            *_rwD3D9CurrentWorldTransform = scratch;
            const HRESULT hr = _RwD3DDevice->SetTransform(D3DTS_WORLD, &scratch);
            _rwD3D9WorldTransformIsIdentity = FALSE;
            return hr >= 0;
        }
    }
    return TRUE;
}


RwBool RwD3D9SetSurfaceProperties(const RwSurfaceProperties* surfaceProps, const RwRGBA* color, RwUInt32 flags) {
    const RwUInt32 flagsMask = flags & 0x48;

    // Ранний выход, если ничего не поменялось
    if (g_d3d9SurfaceDiffuseCache == surfaceProps->diffuse
        && g_d3d9SurfaceAmbientCache == surfaceProps->ambient
        && g_d3d9SurfaceColorCache == *reinterpret_cast<const RwUInt32*>(color)
        && g_d3d9SurfaceFlagsCache == flagsMask
        && g_d3d9LightAmbientCacheR == g_d3d9SceneAmbient.red
        && g_d3d9LightAmbientCacheG == g_d3d9SceneAmbient.green
        && g_d3d9LightAmbientCacheB == g_d3d9SceneAmbient.blue) {
        return TRUE;
    }

    g_d3d9SurfaceDiffuseCache = surfaceProps->diffuse;
    g_d3d9SurfaceAmbientCache = surfaceProps->ambient;
    g_d3d9SurfaceColorCache   = *reinterpret_cast<const RwUInt32*>(color);
    g_d3d9LightAmbientCacheR  = g_d3d9SceneAmbient.red;
    g_d3d9LightAmbientCacheG  = g_d3d9SceneAmbient.green;
    g_d3d9LightAmbientCacheB  = g_d3d9SceneAmbient.blue;
    g_d3d9SurfaceFlagsCache   = flagsMask;

    if ((flags & 0x40) == 0 || *reinterpret_cast<const RwUInt32*>(color) == 0xFFFFFFFF) {
        // Без модуляции материала (или белый цвет): Diffuse = (diffuse, diffuse, diffuse, 1.0)
        g_d3d9SurfaceMaterial.Diffuse.r = surfaceProps->diffuse;
        g_d3d9SurfaceMaterial.Diffuse.a = 1.0f;
        g_d3d9SurfaceMaterial.Diffuse.g = g_d3d9SurfaceMaterial.Diffuse.r;
        g_d3d9SurfaceMaterial.Diffuse.b = g_d3d9SurfaceMaterial.Diffuse.r;

        RwD3D9SetRenderState(D3DRS_AMBIENT, 0xFFFFFFFF);
        RwD3D9SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
        if ((flags & 0x08) == 0) {
            RwD3D9SetRenderState(D3DRS_COLORVERTEX, FALSE);
            RwD3D9SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
            g_d3d9SurfaceMaterial.Emissive.r = 0.0f;
            g_d3d9SurfaceMaterial.Emissive.g = 0.0f;
            g_d3d9SurfaceMaterial.Emissive.b = 0.0f;
        } else {
            RwD3D9SetRenderState(D3DRS_COLORVERTEX, TRUE);
            RwD3D9SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_COLOR1);
        }

        if (surfaceProps->ambient == 1.0f) {
            g_d3d9SurfaceMaterial.Ambient.r = g_d3d9SceneAmbient.red;
            g_d3d9SurfaceMaterial.Ambient.g = g_d3d9SceneAmbient.green;
            g_d3d9SurfaceMaterial.Ambient.b = g_d3d9SceneAmbient.blue;
        } else {
            g_d3d9SurfaceMaterial.Ambient.r = g_d3d9SceneAmbient.red * surfaceProps->ambient;
            g_d3d9SurfaceMaterial.Ambient.g = g_d3d9SceneAmbient.green * surfaceProps->ambient;
            g_d3d9SurfaceMaterial.Ambient.b = g_d3d9SceneAmbient.blue * surfaceProps->ambient;
        }
    } else {
        // Модуляция материалом цветом color (байты -> float, масштаб 1/255)
        const float diffuseScale = surfaceProps->diffuse * 0.003921569f; // 0x859A3C = 1/255
        g_d3d9SurfaceMaterial.Diffuse.r = static_cast<float>(color->red) * diffuseScale;
        g_d3d9SurfaceMaterial.Diffuse.g = static_cast<float>(color->green) * diffuseScale;
        g_d3d9SurfaceMaterial.Diffuse.b = static_cast<float>(color->blue) * diffuseScale;
        g_d3d9SurfaceMaterial.Diffuse.a = static_cast<float>(color->alpha) * 0.003921569f;

        const float ambientScale = surfaceProps->ambient * 0.003921569f;
        if ((flags & 0x08) == 0) {
            RwD3D9SetRenderState(D3DRS_AMBIENT, 0xFFFFFFFF);
            RwD3D9SetRenderState(D3DRS_COLORVERTEX, FALSE);
            RwD3D9SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
            RwD3D9SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
            g_d3d9SurfaceMaterial.Ambient.r = static_cast<float>(color->red) * g_d3d9SceneAmbient.red * ambientScale;
            g_d3d9SurfaceMaterial.Ambient.g = static_cast<float>(color->green) * g_d3d9SceneAmbient.green * ambientScale;
            g_d3d9SurfaceMaterial.Emissive.r = 0.0f;
            g_d3d9SurfaceMaterial.Emissive.g = 0.0f;
            g_d3d9SurfaceMaterial.Emissive.b = 0.0f;
            g_d3d9SurfaceMaterial.Ambient.b = static_cast<float>(color->blue) * g_d3d9SceneAmbient.blue * ambientScale;
        } else {
            // prelit: ambient светится из vertex color'а
            RwD3D9SetRenderState(D3DRS_AMBIENT, D3DCOLOR_ARGB(color->alpha, color->red, color->green, color->blue));
            RwD3D9SetRenderState(D3DRS_COLORVERTEX, TRUE);
            RwD3D9SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_COLOR1);
            RwD3D9SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
            g_d3d9SurfaceMaterial.Ambient.r = 0.0f;
            g_d3d9SurfaceMaterial.Ambient.g = 0.0f;
            g_d3d9SurfaceMaterial.Ambient.b = 0.0f;
            g_d3d9SurfaceMaterial.Emissive.r = static_cast<float>(color->red) * g_d3d9SceneAmbient.red * ambientScale;
            g_d3d9SurfaceMaterial.Emissive.g = static_cast<float>(color->green) * g_d3d9SceneAmbient.green * ambientScale;
            g_d3d9SurfaceMaterial.Emissive.b = static_cast<float>(color->blue) * g_d3d9SceneAmbient.blue * ambientScale;
        }
    }

    // matCache (0xC98AF8) := material (17 dword-ов, как в декомпиле)
    {
        const RwUInt32* src = reinterpret_cast<const RwUInt32*>(&g_d3d9SurfaceMaterial);
        RwUInt32* dst = reinterpret_cast<RwUInt32*>(&g_d3d9MatCache);
        for (RwInt32 i = 0; i < 17; i++)
            dst[i] = src[i];
    }
    return _RwD3DDevice->SetMaterial(&g_d3d9SurfaceMaterial) >= 0;
}



#define RWGTA_MEMFUNCS(g) (*reinterpret_cast<RwMemoryFunctions*>(reinterpret_cast<RwUInt8*>(g) + 0x134))
// [rwcore-5 SHARED BLOCK END]


RwBool RwD3D9SetLight(RwInt32 index, const void* light) {
    // Декомпил 0x7FA660: массив RxD3D9Light (0x6C = D3DLIGHT9 0x68 + RwBool enabled).
    // Если индекс уже кэширован и хелпер 0x7FA8B0 (не на этаже; возвращает !=0 при
    // совпадении) говорит "не изменился" -> TRUE без COM. Иначе массив растёт до
    // index+1 (malloc/realloc, hint=0x1030411), enabled новых слотов = 0; затем копия
    // 0x68 байт в кэш и SetLight = vtable+0xCC с ОРИГИНАЛЬНЫМ указателем.
    const auto LightUnchanged = reinterpret_cast<RwBool(__cdecl*)(const void* /*light*/, const void* /*cached*/)>(
        reinterpret_cast<void*>(0x7FA8B0));
    if (index < MaxNumLights) {
        if (LightUnchanged(light, &LightsCache[index])) {
            return TRUE;
        }
    } else {
        const RwInt32 oldCount = MaxNumLights;
        MaxNumLights = index + 1;
        if (LightsCache == nullptr) {
            LightsCache = static_cast<_rwD3D9LightEntry*>(RWGTA_MEMFUNCS(RwEngineInstance).rwmalloc(static_cast<size_t>(MaxNumLights) * sizeof(_rwD3D9LightEntry), 0x1030411));
        } else {
            LightsCache = static_cast<_rwD3D9LightEntry*>(RWGTA_MEMFUNCS(RwEngineInstance).rwrealloc(LightsCache, static_cast<size_t>(MaxNumLights) * sizeof(_rwD3D9LightEntry), 0x1030411));
        }
        for (RwInt32 i = oldCount; i < MaxNumLights; i++) {
            LightsCache[i].enabled = FALSE; // dword на +0x68, декомпил
        }
    }
    LightsCache[index].light = *static_cast<const D3DLIGHT9*>(light);
    const HRESULT hr = _RwD3DDevice->SetLight(index, static_cast<const D3DLIGHT9*>(light));
    return hr >= 0;
}




void RwD3D9GetLight(RwInt32 index, void* light) {
    // Декомпил 0x7FA820: копия 0x1A dword'ов (0x68 = sizeof(D3DLIGHT9)) из кэша;
    // индекс вне кэша -> 0x68 байт нулей. Проверки NULL на выходе в декомпиле НЕТ.
    if (index < MaxNumLights) {
        *static_cast<D3DLIGHT9*>(light) = LightsCache[index].light;
    } else {
        memset(light, 0, sizeof(D3DLIGHT9));
    }
}




RwBool RwD3D9EnableLight(RwInt32 index, RwBool enable) {
    // Декомпил 0x7FA860: dword enabled (+0x68 кэша); COM LightEnable (vtable+0xD4)
    // только при изменении; индекс вне кэша -> TRUE без вызова.
    if (index < MaxNumLights) {
        if (LightsCache[index].enabled != enable) {
            LightsCache[index].enabled = enable;
            const HRESULT hr = _RwD3DDevice->LightEnable(index, enable);
            return hr >= 0;
        }
    }
    return TRUE;
}


RwBool RwD3D9IndexBufferCreate(RwUInt32 numIndices, void* indexBuffer) {
   return ((RwBool(__cdecl *)(RwUInt32, void*))0x4C9970)(numIndices, indexBuffer);
}


#define RWGTA_MEMFUNCS(g) (*reinterpret_cast<RwMemoryFunctions*>(reinterpret_cast<RwUInt8*>(g) + 0x134))
// [rwcore-5 SHARED BLOCK END]


RwBool RwD3D9CreateVertexDeclaration(const void* elements, void** vertexdeclaration) {
    // Декомпил 0x7FAA30: sizeof(D3DVERTEXELEMENT9)==8 (WORD Stream, WORD Offset,
    // BYTE Type, BYTE Method, BYTE Usage, BYTE UsageIndex) — потому копия = numElements*8
    // байт. D3DDECL_END() опознаётся по Type == 0x11 (D3DDECLTYPE_UNUSED), младший байт
    // 2-го dword'а элемента в декомпиле. Кэш растёт блоками +0x10 записей.
    RwUInt32 numElements = 1;
    for (const D3DVERTEXELEMENT9* el = static_cast<const D3DVERTEXELEMENT9*>(elements); el->Type != 0x11 /* D3DDECLTYPE_UNUSED: маркер D3DDECL_END */; el++) {
        numElements++;
    }
    // поиск в кэше: тот же набор элементов -> реюз (декларация, потерянная через
    // RwD3D9DeleteVertexDeclaration при полном Release, создаётся заново; живая — AddRef)
    for (RwUInt32 i = 0; i < _rwD3D9VertexDeclCacheCount; i++) {
        _rwD3D9VertexDeclCacheEntry& entry = _rwD3D9VertexDeclCache[i];
        if (entry.numElements != numElements) {
            continue;
        }
        if (memcmp(entry.elements, elements, numElements * sizeof(D3DVERTEXELEMENT9)) != 0) {
            continue;
        }
        if (entry.decl == nullptr) {
            _RwD3DDevice->CreateVertexDeclaration(entry.elements, reinterpret_cast<IDirect3DVertexDeclaration9**>(&entry.decl)); // vtable+0x158
        } else {
            static_cast<IDirect3DVertexDeclaration9*>(entry.decl)->AddRef();
        }
        // NB: в декомпиле найденный decl пишется в *param_1 (буфер elements) — это
        // артефакт декомпилятора (перепутан стековый параметр); исход в *param_2
        // (vertexdeclaration), как в хвостовом и fail-путях того же декомпила.
        *vertexdeclaration = entry.decl;
        return *vertexdeclaration != nullptr;
    }
    if (_rwD3D9VertexDeclCacheCap <= _rwD3D9VertexDeclCacheCount) {
        _rwD3D9VertexDeclCacheCap += 0x10;
        if (_rwD3D9VertexDeclCache == nullptr) {
            // NB: декомпил показывает malloc без hint (аргумент потерян декомпилятором,
            // rwmalloc(size, hint) — hint взят от realloc этого же кэша)
            _rwD3D9VertexDeclCache = static_cast<_rwD3D9VertexDeclCacheEntry*>(
                RWGTA_MEMFUNCS(RwEngineInstance).rwmalloc(static_cast<size_t>(_rwD3D9VertexDeclCacheCap) * sizeof(_rwD3D9VertexDeclCacheEntry), 0x1040411));
        } else {
            _rwD3D9VertexDeclCache = static_cast<_rwD3D9VertexDeclCacheEntry*>(
                RWGTA_MEMFUNCS(RwEngineInstance).rwrealloc(_rwD3D9VertexDeclCache, static_cast<size_t>(_rwD3D9VertexDeclCacheCap) * sizeof(_rwD3D9VertexDeclCacheEntry), 0x1040411));
        }
    }
    const HRESULT hr = _RwD3DDevice->CreateVertexDeclaration(static_cast<const D3DVERTEXELEMENT9*>(elements), reinterpret_cast<IDirect3DVertexDeclaration9**>(vertexdeclaration)); // vtable+0x158
    if (hr >= 0) {
        _rwD3D9VertexDeclCacheEntry& entry = _rwD3D9VertexDeclCache[_rwD3D9VertexDeclCacheCount];
        entry.decl        = *vertexdeclaration;
        entry.numElements = numElements;
        entry.elements    = static_cast<D3DVERTEXELEMENT9*>(RWGTA_MEMFUNCS(RwEngineInstance).rwmalloc(numElements * sizeof(D3DVERTEXELEMENT9), 0x40411));
        memcpy(entry.elements, elements, numElements * sizeof(D3DVERTEXELEMENT9));
        _rwD3D9VertexDeclCacheCount++;
        return *vertexdeclaration != nullptr;
    }
    *vertexdeclaration = nullptr;
    return FALSE;
}




void RwD3D9DeleteVertexDeclaration(void* vertexdeclaration) {
    // Декомпил 0x7FAC10: поиск записи кэша по decl; decl->Release() (vtable+8);
    // если счётчик упал в 0 — слот кэша обнуляется (последующий
    // RwD3D9CreateVertexDeclaration пересоздаст декларацию по сохранённой копии элементов).
    for (RwUInt32 i = 0; i < _rwD3D9VertexDeclCacheCount; i++) {
        if (_rwD3D9VertexDeclCache[i].decl == vertexdeclaration) {
            if (static_cast<IDirect3DVertexDeclaration9*>(vertexdeclaration)->Release() == 0) {
                _rwD3D9VertexDeclCache[i].decl = nullptr;
            }
            return;
        }
    }
}


void RwD3D9DeleteVertexShader(void* shader) {
    // Декомпил 0x7FAC90 (11 байт): IDirect3DVertexShader9::Release() (vtable+8).
    static_cast<IDirect3DVertexShader9*>(shader)->Release();
}


RwBool RwD3D9CreatePixelShader(const RwUInt32* function, void** shader) {
    // Декомпил 0x7FACC0: CreatePixelShader (vtable+0x1A8, сверено с d3d9.h);
    // возвращается hr >= 0.
    const HRESULT hr = _RwD3DDevice->CreatePixelShader(reinterpret_cast<const DWORD*>(function), reinterpret_cast<IDirect3DPixelShader9**>(shader));
    return hr >= 0;
}


void RwD3D9DeletePixelShader(void* shader) {
    // Декомпил 0x7FACF0 (11 байт): IDirect3DPixelShader9::Release() (vtable+8).
    static_cast<IDirect3DPixelShader9*>(shader)->Release();
}


void _rwD3D9SetStreams(const RxD3D9VertexStream* streams, RwBool useOffsets) {
    plugin::CallAndReturn<void, 0x7FA090>(streams, useOffsets);
}


const void* RwD3D9GetCaps() {
    /* Декомпил: return &DAT_00c9bf00 — кэш D3DCAPS9, заполненный при инициализации D3D9-девайса. */
    return &StaticRef<D3DCAPS9>(0xC9BF00);
}



RwBool RwD3D9CameraIsSphereFullyInsideFrustum(const void* camera, const void* sphere) {
    const RwCamera* cam       = static_cast<const RwCamera*>(camera);
    const RwSphere* theSphere = static_cast<const RwSphere*>(sphere);
    for (int i = 0; i < 6; i++) {
        const RwFrustumPlane& fp = cam->frustumPlanes[i];
        /* Декомпил: если -radius < dot(n, center) - distance — сфера выходит за плоскость */
        if (-theSphere->radius < fp.plane.normal.z * theSphere->center.z
                              + fp.plane.normal.x * theSphere->center.x
                              + fp.plane.normal.y * theSphere->center.y
                              - fp.plane.distance)
            return FALSE;
    }
    return TRUE;
}



RwBool RwD3D9CameraIsBBoxFullyInsideFrustum(const void* camera, const void* boundingBox) {
    const RwCamera* cam = static_cast<const RwCamera*>(camera);
    const RwBBox*   bb  = static_cast<const RwBBox*>(boundingBox);
    for (int i = 0; i < 6; i++) {
        const RwFrustumPlane& fp = cam->frustumPlanes[i];
        /* closestX/Y/Z (байты RwFrustumPlane+0x10..0x12, «closestCorner»): 0 => inf, 1 => sup.
           Декомпил выбирает компоненты бокса по байту: idx = 3/4/5 - 3*sel по float-базе бокса. */
        const float x = fp.closestX ? bb->sup.x : bb->inf.x;
        const float y = fp.closestY ? bb->sup.y : bb->inf.y;
        const float z = fp.closestZ ? bb->sup.z : bb->inf.z;
        if (fp.plane.distance < fp.plane.normal.z * z
                              + fp.plane.normal.x * x
                              + fp.plane.normal.y * y)
            return FALSE;
    }
    return TRUE;
}


void _rwD3D9RasterConvertToNonPalettized(RwRaster* raster) {
   ((void(__cdecl *)(RwRaster*))0x4CD250)(raster);
}

RwBool _rwDeviceRegisterPlugin() {
    r4DevicePluginOffset = ((int(__cdecl*)(RwInt32, RwUInt32, void*, void*, void*))0x7FB0B0)(
        0x24, 0x40C, (void*)0x4C9A60, (void*)0x4C9A80, nullptr);
    if (r4DevicePluginOffset < 0) {
        return FALSE;
    }
    RwUInt32* block = &StaticRef<RwUInt32>(0xB4E7E0);
    for (RwInt32 i = 0; i < 0x80; i++) {   // 0x80 dwords = 0x200 bytes
        block[i] = 0;
    }
    StaticRef<RwUInt8>(0x00B4E830) = 0;
    StaticRef<RwUInt8>(0x00B4E831) = 0x18;
    StaticRef<RwUInt16>(0x00B4E832) = 0;
    StaticRef<RwUInt8>(0x00B4E834) = 1;
    StaticRef<RwUInt8>(0x00B4E835) = 0x20;
    StaticRef<RwUInt16>(0x00B4E836) = 0x500;
    StaticRef<RwUInt8>(0x00B4E838) = 0;
    StaticRef<RwUInt8>(0x00B4E839) = 0x20;
    StaticRef<RwUInt16>(0x00B4E83A) = 0x600;
    StaticRef<RwUInt8>(0x00B4E83C) = 0;
    StaticRef<RwUInt8>(0x00B4E83D) = 0x10;
    StaticRef<RwUInt16>(0x00B4E83E) = 0x200;
    StaticRef<RwUInt8>(0x00B4E840) = 0;
    StaticRef<RwUInt8>(0x00B4E841) = 0x10;
    StaticRef<RwUInt16>(0x00B4E842) = 0xa00;
    StaticRef<RwUInt8>(0x00B4E844) = 1;
    StaticRef<RwUInt8>(0x00B4E845) = 0x10;
    StaticRef<RwUInt16>(0x00B4E846) = 0x100;
    StaticRef<RwUInt8>(0x00B4E848) = 1;
    StaticRef<RwUInt8>(0x00B4E849) = 0x10;
    StaticRef<RwUInt16>(0x00B4E84A) = 0x300;
    StaticRef<RwUInt8>(0x00B4E84C) = 0;
    StaticRef<RwUInt8>(0x00B4E84D) = 8;
    StaticRef<RwUInt16>(0x00B4E84E) = 0;
    StaticRef<RwUInt8>(0x00B4E850) = 1;
    StaticRef<RwUInt8>(0x00B4E851) = 8;
    StaticRef<RwUInt16>(0x00B4E852) = 0;
    StaticRef<RwUInt8>(0x00B4E854) = 1;
    StaticRef<RwUInt8>(0x00B4E855) = 0x10;
    StaticRef<RwUInt16>(0x00B4E856) = 0;
    StaticRef<RwUInt8>(0x00B4E858) = 0;
    StaticRef<RwUInt8>(0x00B4E859) = 0x10;
    StaticRef<RwUInt16>(0x00B4E85A) = 0;
    StaticRef<RwUInt8>(0x00B4E85C) = 1;
    StaticRef<RwUInt8>(0x00B4E85D) = 0x20;
    StaticRef<RwUInt16>(0x00B4E85E) = 0;
    StaticRef<RwUInt8>(0x00B4E860) = 1;
    StaticRef<RwUInt8>(0x00B4E861) = 0x20;
    StaticRef<RwUInt16>(0x00B4E862) = 0;
    StaticRef<RwUInt8>(0x00B4E864) = 0;
    StaticRef<RwUInt8>(0x00B4E865) = 0x20;
    StaticRef<RwUInt16>(0x00B4E866) = 0;
    StaticRef<RwUInt8>(0x00B4E868) = 0;
    StaticRef<RwUInt8>(0x00B4E869) = 0x20;
    StaticRef<RwUInt16>(0x00B4E86A) = 0;
    StaticRef<RwUInt8>(0x00B4E86C) = 1;
    StaticRef<RwUInt8>(0x00B4E86D) = 0x20;
    StaticRef<RwUInt16>(0x00B4E86E) = 0;
    StaticRef<RwUInt8>(0x00B4E870) = 1;
    StaticRef<RwUInt8>(0x00B4E871) = 0x40;
    StaticRef<RwUInt16>(0x00B4E872) = 0;
    StaticRef<RwUInt8>(0x00B4E880) = 1;
    StaticRef<RwUInt8>(0x00B4E881) = 0x10;
    StaticRef<RwUInt16>(0x00B4E882) = 0;
    StaticRef<RwUInt8>(0x00B4E884) = 0;
    StaticRef<RwUInt8>(0x00B4E885) = 8;
    StaticRef<RwUInt16>(0x00B4E886) = 0x2000;
    StaticRef<RwUInt8>(0x00B4E8A8) = 0;
    StaticRef<RwUInt8>(0x00B4E8A9) = 8;
    StaticRef<RwUInt16>(0x00B4E8AA) = 0x400;
    StaticRef<RwUInt8>(0x00B4E8AC) = 1;
    StaticRef<RwUInt8>(0x00B4E8AD) = 0x10;
    StaticRef<RwUInt16>(0x00B4E8AE) = 0;
    StaticRef<RwUInt8>(0x00B4E8B0) = 1;
    StaticRef<RwUInt8>(0x00B4E8B1) = 8;
    StaticRef<RwUInt16>(0x00B4E8B2) = 0;
    StaticRef<RwUInt8>(0x00B4E8D0) = 0;
    StaticRef<RwUInt8>(0x00B4E8D1) = 0x10;
    StaticRef<RwUInt16>(0x00B4E8D2) = 0;
    StaticRef<RwUInt8>(0x00B4E8D4) = 0;
    StaticRef<RwUInt8>(0x00B4E8D5) = 0x10;
    StaticRef<RwUInt16>(0x00B4E8D6) = 0;
    StaticRef<RwUInt16>(0x00B4E8FA) = 0x700;
    StaticRef<RwUInt16>(0x00B4E906) = 0x700;
    StaticRef<RwUInt16>(0x00B4E922) = 0x700;
    StaticRef<RwUInt16>(0x00B4E8FE) = 0x900;
    StaticRef<RwUInt16>(0x00B4E90E) = 0x900;
    StaticRef<RwUInt16>(0x00B4E916) = 0x900;
    StaticRef<RwUInt16>(0x00B4E91E) = 0x900;
    StaticRef<RwUInt16>(0x00B4E92A) = 0x900;
    StaticRef<RwUInt16>(0x00B4E92E) = 0x900;
    StaticRef<RwUInt8>(0x00B4E8D8) = 0;
    StaticRef<RwUInt16>(0x00B4E8DA) = 0;
    StaticRef<RwUInt8>(0x00B4E8DC) = 0;
    StaticRef<RwUInt16>(0x00B4E8DE) = 0;
    StaticRef<RwUInt8>(0x00B4E8E0) = 0;
    StaticRef<RwUInt16>(0x00B4E8E2) = 0;
    StaticRef<RwUInt16>(0x00B4E8EE) = 0;
    StaticRef<RwUInt8>(0x00B4E8F8) = 0;
    StaticRef<RwUInt8>(0x00B4E8FC) = 0;
    StaticRef<RwUInt8>(0x00B4E904) = 0;
    StaticRef<RwUInt8>(0x00B4E90C) = 0;
    StaticRef<RwUInt8>(0x00B4E914) = 0;
    StaticRef<RwUInt8>(0x00B4E91C) = 0;
    StaticRef<RwUInt8>(0x00B4E920) = 0;
    StaticRef<RwUInt8>(0x00B4E928) = 0;
    StaticRef<RwUInt8>(0x00B4E92C) = 0;
    StaticRef<RwUInt8>(0x00B4E924) = 0;
    StaticRef<RwUInt16>(0x00B4E926) = 0;
    StaticRef<RwUInt8>(0x00B4E998) = 0;
    StaticRef<RwUInt16>(0x00B4E99A) = 0;
    StaticRef<RwUInt8>(0x00B4E99C) = 0;
    StaticRef<RwUInt16>(0x00B4E99E) = 0;
    StaticRef<RwUInt8>(0x00B4E9A0) = 0;
    StaticRef<RwUInt16>(0x00B4E9A2) = 0;
    StaticRef<RwUInt16>(0x00B4E9A6) = 0;
    StaticRef<RwUInt8>(0x00B4E9A8) = 0;
    StaticRef<RwUInt16>(0x00B4E9AA) = 0;
    StaticRef<RwUInt8>(0x00B4E9AC) = 0;
    StaticRef<RwUInt16>(0x00B4E9AE) = 0;
    StaticRef<RwUInt16>(0x00B4E9B2) = 0;
    StaticRef<RwUInt8>(0x00B4E9B4) = 0;
    StaticRef<RwUInt16>(0x00B4E9B6) = 0;
    StaticRef<RwUInt8>(0x00B4E8D9) = 0x20;
    StaticRef<RwUInt8>(0x00B4E8DD) = 0x20;
    StaticRef<RwUInt8>(0x00B4E8E1) = 0x20;
    StaticRef<RwUInt8>(0x00B4E8EC) = 1;
    StaticRef<RwUInt8>(0x00B4E8ED) = 0x20;
    StaticRef<RwUInt8>(0x00B4E8F9) = 0x10;
    StaticRef<RwUInt8>(0x00B4E8FD) = 0x20;
    StaticRef<RwUInt8>(0x00B4E905) = 0x10;
    StaticRef<RwUInt8>(0x00B4E90D) = 0x20;
    StaticRef<RwUInt8>(0x00B4E915) = 0x20;
    StaticRef<RwUInt8>(0x00B4E91D) = 0x20;
    StaticRef<RwUInt8>(0x00B4E921) = 0x10;
    StaticRef<RwUInt8>(0x00B4E929) = 0x20;
    StaticRef<RwUInt8>(0x00B4E92D) = 0x20;
    StaticRef<RwUInt8>(0x00B4E925) = 0x10;
    StaticRef<RwUInt8>(0x00B4E999) = 0x40;
    StaticRef<RwUInt8>(0x00B4E99D) = 0x10;
    StaticRef<RwUInt8>(0x00B4E9A1) = 0x20;
    StaticRef<RwUInt8>(0x00B4E9A4) = 1;
    StaticRef<RwUInt8>(0x00B4E9A5) = 0x40;
    StaticRef<RwUInt8>(0x00B4E9A9) = 0x20;
    StaticRef<RwUInt8>(0x00B4E9AD) = 0x40;
    StaticRef<RwUInt8>(0x00B4E9B0) = 1;
    StaticRef<RwUInt8>(0x00B4E9B1) = 0x80;
    StaticRef<RwUInt8>(0x00B4E9B5) = 0x10;
    return TRUE;
}



void _rwD3D9DeviceSetRestoreCallback(rwD3D9DeviceRestoreCallBack callback) {
    rwD3D9DeviceRestoreCBGta = callback;
}



rwD3D9DeviceRestoreCallBack _rwD3D9DeviceGetRestoreCallback() {
    return rwD3D9DeviceRestoreCBGta;
}


RwImage *RwImageResample(RwImage *dstImage, const RwImage *srcImage)
{
    RwInt32 dstWidth  = dstImage->width;      /* param_1[1] */
    RwInt32 srcWidth  = srcImage->width;      /* param_2[1] */
    RwInt32 srcHeight = srcImage->height;     /* param_2[2] */
    RwInt32 dstHeight = dstImage->height;     /* param_1[2] */

    dstImage->flags |= srcImage->flags & 2;

    RwInt32 xScale = (RwInt32)floorf(((RwReal)srcWidth / (RwReal)dstWidth) * 65536.0f + 0.5f);
    RwInt32 yScale = (RwInt32)floorf(((RwReal)srcHeight / (RwReal)dstHeight) * 65536.0f + 0.5f);

    RwInt32 yBand = 0;         /* uVar11: source row band start, 16.16 */
    RwInt32 row = 0;           /* local_3c */
    if (0 < dstHeight)
    {
        do
        {
            if (0 < dstWidth)
            {
                RwInt32 xBand = 0;                                    /* iVar14 */
                RwInt32 yBandEnd = yScale - 1 + yBand;                /* iVar1 */
                RwInt32 yBandLast = yBandEnd >> 0x10;                 /* iVar10 */
                RwReal bandWeight = (RwReal)(RwInt32)(yBandEnd - yBand) * (1.0f / 65536.0f); /* fVar7 */
                RwUInt8 *dstPx = (RwUInt8 *)(dstImage->stride * row + (RwUInt32)dstImage->cpPixels + 2);
                RwInt32 n = dstWidth;                                 /* local_48 */
                do
                {
                    RwReal acc[4];                                    /* local_20 */
                    RwInt32 yStart;                                   /* iVar13 */
                    if ((yBand >> 0x10) == yBandLast)
                    {
                        ((void (__cdecl *)(const RwImage *, RwInt32, RwInt32, RwUInt32, RwReal *))0x80CA20)
                            (srcImage, xBand, yBandEnd, (RwUInt32)yBand, acc);
                        acc[0] = bandWeight * acc[0];
                        acc[1] = acc[1] * bandWeight;
                        acc[2] = acc[2] * bandWeight;
                        acc[3] = acc[3] * bandWeight;
                    }
                    else
                    {
                        /* multiple source rows: partial first row */
                        yStart = (yBand & 0xffff0000) + 0x10000;
                        ((void (__cdecl *)(const RwImage *, RwInt32, RwInt32, RwUInt32, RwReal *))0x80CA20)
                            (srcImage, xBand, yBandEnd, (RwUInt32)yBand, acc);
                        {
                            RwReal w = (RwReal)(RwInt32)(yStart - yBand) * (1.0f / 65536.0f);
                            acc[0] = acc[0] * w;
                            acc[1] = acc[1] * w;
                            acc[2] = acc[2] * w;
                            acc[3] = acc[3] * w;
                        }
                        /* full rows */
                        for (; (yStart >> 0x10) != yBandLast; yStart += 0x10000)
                        {
                            RwReal add[4];                            /* local_10 */
                            ((void (__cdecl *)(const RwImage *, RwInt32, RwInt32, RwUInt32, RwReal *))0x80CA20)
                                (srcImage, xBand, yBandEnd, (RwUInt32)yStart, add);
                            acc[0] = add[0] + acc[0];
                            acc[1] = add[1] + acc[1];
                            acc[2] = add[2] + acc[2];
                            acc[3] = add[3] + acc[3];
                        }
                        /* partial last row */
                        {
                            RwReal add[4];
                            ((void (__cdecl *)(const RwImage *, RwInt32, RwInt32, RwUInt32, RwReal *))0x80CA20)
                                (srcImage, xBand, yBandEnd, (RwUInt32)yStart, add);
                            RwReal w = (RwReal)(RwInt32)((yScale - 1 + yBand) - yStart) * (1.0f / 65536.0f);
                            add[0] = add[0] * w;
                            add[1] = add[1] * w;
                            add[2] = add[2] * w;
                            add[3] = add[3] * w;
                            acc[0] = add[0] + acc[0];
                            acc[1] = add[1] + acc[1];
                            acc[2] = add[2] + acc[2];
                            acc[3] = add[3] + acc[3];
                        }
                    }

                    /* normalise by the band weight and store (decompile literal: 1.0f / bandWeight) */
                    {
                        RwReal inv = 1.0f / bandWeight;
                        acc[0] = inv * acc[0];
                        acc[1] = acc[1] * inv;
                        acc[2] = acc[2] * inv;
                        acc[3] = acc[3] * inv;
                    }
                    dstPx[-2] = (RwUInt8)(RwInt32)floorf(acc[0] * 255.0f + 0.5f);
                    dstPx[-1] = (RwUInt8)(RwInt32)floorf(acc[1] * 255.0f + 0.5f);
                    dstPx[0]  = (RwUInt8)(RwInt32)floorf(acc[2] * 255.0f + 0.5f);
                    dstPx[1]  = (RwUInt8)(RwInt32)floorf(acc[3] * 255.0f + 0.5f);

                    xBand += xScale;
                    dstPx += 4;
                    n--;
                } while (n != 0);
            }
            yBand += yScale;
            row++;
        } while (row < dstHeight);
    }
    return dstImage;
}


RwImage *RwImageCreateResample(const RwImage *srcImage, RwInt32 width, RwInt32 height)
{
    RwImage *newImage = RwImageCreate(width, height, 0x20);
    if (newImage != NULL)
    {
        if (RwImageAllocatePixels(newImage) == NULL)
        {
            /* allocate failed: destroy only — the binary never calls FreePixels here */
            RwImageDestroy(newImage);
            return NULL;
        }

        if (srcImage->depth == 0x20)
        {
            if (RwImageResample(newImage, srcImage) != NULL)
                return newImage;
        }
        else
        {
            RwImage *src32 = RwImageCreate(srcImage->width, srcImage->height, 0x20);
            if (src32 != NULL)
            {
                if (RwImageAllocatePixels(src32) == NULL)
                {
                    /* allocate failed: destroy only (no FreePixels), then the new image */
                    RwImageDestroy(src32);
                    RwImageFreePixels(newImage);
                    RwImageDestroy(newImage);
                    return NULL;
                }
                ((void (__cdecl *)(RwImage *, const RwImage *))0x803950)(src32, srcImage);
                if (RwImageResample(newImage, src32) != NULL)
                {
                    RwImageFreePixels(src32);
                    RwImageDestroy(src32);
                    return newImage;
                }
                RwImageFreePixels(src32);
                RwImageDestroy(src32);
            }
        }

        RwImageFreePixels(newImage);
        RwImageDestroy(newImage);
    }
    return NULL;
}


RwImage* RwImageSetFromRaster(RwImage* image, RwRaster* raster) {
    if (RwGtaStandardFunc(rwSTANDARDIMAGEGETRASTER)(image, raster, 0) != 0) {
        if ((raster->privateFlags & 1) != 0)
            image->flags |= rwIMAGEGAMMACORRECTED;
        return image;
    }
    return nullptr;
}


RwRaster* RwRasterSetFromImage(RwRaster* raster, RwImage* image) {
    if (RwGtaStandardFunc(rwSTANDARDRASTERSETIMAGE)(raster, image, 0) != 0) {
        if ((image->flags & rwIMAGEGAMMACORRECTED) != 0)
            raster->privateFlags |= 1;
        return raster;
    }
    return nullptr;
}


struct RwImageRasterFormatQuery
{
    RwInt32  width;      // +0x00
    RwInt32  height;     // +0x04
    RwInt32  depth;      // +0x08
    RwUInt32 rawFormat;  // +0x0C (pixel-format-id<<24 | low-byte flags)
};

RwRaster* RwRasterRead(const RwChar* filename)
{
    RwImage* image = RwImageRead(filename);
    if (image == NULL)
        return NULL;

    RwImageRasterFormatQuery query;
    const RwBool found = RwEngineInstance->stdFunc[rwSTANDARDIMAGEFINDRASTERFORMAT](&query, image, rwRASTERTYPENORMAL);

    // Vendor behaviour when the stdFunc fails: it consumes the uninitialized stack
    // buffer (ghidra shows the filename pointer surviving in the regs). We fall back
    // to the image's own dimensions + zero depth/format - see STATUS.md note.
    RwInt32  width  = image->width;
    RwInt32  height = image->height;
    RwInt32  depth  = 0;
    RwInt32  format = 0;
    if (found) {
        width  = query.width;
        height = query.height;
        depth  = query.depth;
        format = (RwInt32)(((query.rawFormat >> 24) << 8) | (query.rawFormat & 0xFF));
    }

    RwImage* usedImage = image;
    if (width != image->width || height != image->height) {
        usedImage = RwImageCreateResample(image, width, height);
        RwImageDestroy(image);
    }

    RwRaster* raster = RwRasterCreate(width, height, depth, format);
    if (raster != NULL) {
        if (RwEngineInstance->stdFunc[rwSTANDARDRASTERSETIMAGE](raster, usedImage, 0)) {
            if (usedImage->flags & rwIMAGEGAMMACORRECTED)
                raster->privateFlags |= rwRASTERGAMMACORRECTED;
        }
        RwImageDestroy(usedImage);
        return raster;
    }

    RwImageDestroy(usedImage);
    return NULL;
}


RwRaster* RwRasterReadMaskedRaster(const RwChar* filename, const RwChar* maskname)
{
    RwImage* image = RwImageReadMaskedImage(filename, maskname);
    if (image == NULL)
        return NULL;

    RwRaster* raster = RwRasterCreate(image->width, image->height, 0, 0);
    if (raster == NULL) {
        RwImageDestroy(image);
        return NULL;
    }

    if (RwEngineInstance->stdFunc[rwSTANDARDRASTERSETIMAGE](raster, image, 0) &&
        (image->flags & rwIMAGEGAMMACORRECTED)) {
        raster->privateFlags |= rwRASTERGAMMACORRECTED;
    }

    RwImageDestroy(image);
    return raster;
}


struct RwImageRasterFormatOutGta
{
    RwUInt8 pad00[0x0C]; // +0x00 (заполняется драйвером, не читается)
    RwInt32 width;       // +0x0C
    RwInt32 height;      // +0x10
    RwInt32 depth;       // +0x14
    RwInt32 format;      // +0x18
};
RwImage* RwImageFindRasterFormat(RwImage* ipImage, RwInt32 nRasterType,
                                 RwInt32* npWidth, RwInt32* npHeight,
                                 RwInt32* npDepth, RwInt32* npFormat) {
    RwImageRasterFormatOutGta out;
    if (RwGtaStandardFunc(rwSTANDARDIMAGEFINDRASTERFORMAT)(&out, ipImage, nRasterType) == 0)
        return nullptr;
    *npFormat = ((out.format >> 24) << 8) | (out.format & 0xFF);
    *npWidth  = out.width;
    *npHeight = out.height;
    *npDepth  = out.depth;
    return ipImage;
}


RwFrame* RwFrameForAllObjects(RwFrame* frame, RwObjectCallBack callBack, void* data) {
    // decompile 0x7F1200: walk frame->objectList (+0x90), callback gets link-0x8
    // == enclosing RwObject (RwObjectHasFrame::lFrame sits at +0x8 right after RwObject).
    for (RwLLLink* link = frame->objectList.link.next; link != &frame->objectList.link; link = link->next) {
        if (!callBack(&rwLLLinkGetData(link, RwObjectHasFrame, lFrame)->object, data)) {
            break;
        }
    }
    return frame;
}


RwFrame* RwFrameTranslate(RwFrame* frame, const RwV3d* v, RwOpCombineType combine) {
    // decompile 0x7F0E30: RwMatrixTranslate(frame+0x10 /* modelling */, v, combine) @0x7F2450
    RwMatrixTranslate(&frame->modelling, v, combine);
    // vendor-inline "markDirty": root not yet in sync list -> push root->inDirtyListLink
    // onto RwGlobals dirtyFrameList (RwEngineInstance+0xBC, decompile DAT_00c97b24+0xBC)
    RwFrame* root = frame->root;
    RwUInt8 rootPf = rwObjectGetPrivateFlags(root);
    if ((rootPf & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) == 0) {
        RwLLLink* head = reinterpret_cast<RwLLLink*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0xBC);
        root->inDirtyListLink.next = head->next;
        root->inDirtyListLink.prev = head;
        head->next->prev = &root->inDirtyListLink;
        head->next = &root->inDirtyListLink;
    }
    rwObjectSetPrivateFlags(root, rootPf | (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ));
    rwObjectSetPrivateFlags(frame, rwObjectGetPrivateFlags(frame) | (rwFRAMEPRIVATESUBTREESYNCLTM | rwFRAMEPRIVATESUBTREESYNCOBJ));
    return frame;
}


RwFrame* RwFrameRotate(RwFrame* frame, const RwV3d* axis, RwReal angle, RwOpCombineType combine) {
    // decompile 0x7F1010: RwMatrixRotate(frame+0x10, axis, angle, combine) @0x7F1FD0
    RwMatrixRotate(&frame->modelling, axis, angle, combine);
    RwFrame* root = frame->root;
    RwUInt8 rootPf = rwObjectGetPrivateFlags(root);
    if ((rootPf & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) == 0) {
        RwLLLink* head = reinterpret_cast<RwLLLink*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0xBC);
        root->inDirtyListLink.next = head->next;
        root->inDirtyListLink.prev = head;
        head->next->prev = &root->inDirtyListLink;
        head->next = &root->inDirtyListLink;
    }
    rwObjectSetPrivateFlags(root, rootPf | (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ));
    rwObjectSetPrivateFlags(frame, rwObjectGetPrivateFlags(frame) | (rwFRAMEPRIVATESUBTREESYNCLTM | rwFRAMEPRIVATESUBTREESYNCOBJ));
    return frame;
}


RwFrame* RwFrameScale(RwFrame* frame, const RwV3d* v, RwOpCombineType combine) {
    // decompile 0x7F0ED0: RwMatrixScale(frame+0x10, v, combine) @0x7F22C0
    RwMatrixScale(&frame->modelling, v, combine);
    RwFrame* root = frame->root;
    RwUInt8 rootPf = rwObjectGetPrivateFlags(root);
    if ((rootPf & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) == 0) {
        RwLLLink* head = reinterpret_cast<RwLLLink*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0xBC);
        root->inDirtyListLink.next = head->next;
        root->inDirtyListLink.prev = head;
        head->next->prev = &root->inDirtyListLink;
        head->next = &root->inDirtyListLink;
    }
    rwObjectSetPrivateFlags(root, rootPf | (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ));
    rwObjectSetPrivateFlags(frame, rwObjectGetPrivateFlags(frame) | (rwFRAMEPRIVATESUBTREESYNCLTM | rwFRAMEPRIVATESUBTREESYNCOBJ));
    return frame;
}


RwFrame* RwFrameTransform(RwFrame* frame, const RwMatrix* m, RwOpCombineType combine) {
    // decompile 0x7F0F70: RwMatrixTransform(frame+0x10, m, combine) @0x7F25A0
    RwMatrixTransform(&frame->modelling, m, combine);
    RwFrame* root = frame->root;
    RwUInt8 rootPf = rwObjectGetPrivateFlags(root);
    if ((rootPf & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) == 0) {
        RwLLLink* head = reinterpret_cast<RwLLLink*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0xBC);
        root->inDirtyListLink.next = head->next;
        root->inDirtyListLink.prev = head;
        head->next->prev = &root->inDirtyListLink;
        head->next = &root->inDirtyListLink;
    }
    rwObjectSetPrivateFlags(root, rootPf | (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ));
    rwObjectSetPrivateFlags(frame, rwObjectGetPrivateFlags(frame) | (rwFRAMEPRIVATESUBTREESYNCLTM | rwFRAMEPRIVATESUBTREESYNCOBJ));
    return frame;
}


RwFrame* RwFrameOrthoNormalize(RwFrame* frame) {
    // decompile 0x7F1170: RwMatrixOrthoNormalize(frame+0x10, frame+0x10) @0x7F1920(thunk 0x7F1930)
    RwMatrixOrthoNormalize(&frame->modelling, &frame->modelling);
    RwFrame* root = frame->root;
    RwUInt8 rootPf = rwObjectGetPrivateFlags(root);
    if ((rootPf & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) == 0) {
        RwLLLink* head = reinterpret_cast<RwLLLink*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0xBC);
        root->inDirtyListLink.next = head->next;
        root->inDirtyListLink.prev = head;
        head->next->prev = &root->inDirtyListLink;
        head->next = &root->inDirtyListLink;
    }
    rwObjectSetPrivateFlags(root, rootPf | (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ));
    rwObjectSetPrivateFlags(frame, rwObjectGetPrivateFlags(frame) | (rwFRAMEPRIVATESUBTREESYNCLTM | rwFRAMEPRIVATESUBTREESYNCOBJ));
    return frame;
}


RwFrame* RwFrameSetIdentity(RwFrame* frame) {
    // decompile 0x7F10B0: inline identity write to frame+0x10 (modelling):
    // right=(1,0,0) up=(0,1,0) at=(0,0,1) pos=0, flags |= 0x20003
    // (0x20003 = rwMATRIXINTERNALIDENTITY | rwMATRIXTYPEORTHONORMAL) == RwMatrixSetIdentityMacro
    RwMatrixSetIdentityMacro(&frame->modelling);
    RwFrame* root = frame->root;
    RwUInt8 rootPf = rwObjectGetPrivateFlags(root);
    if ((rootPf & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) == 0) {
        RwLLLink* head = reinterpret_cast<RwLLLink*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0xBC);
        root->inDirtyListLink.next = head->next;
        root->inDirtyListLink.prev = head;
        head->next->prev = &root->inDirtyListLink;
        head->next = &root->inDirtyListLink;
    }
    rwObjectSetPrivateFlags(root, rootPf | (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ));
    rwObjectSetPrivateFlags(frame, rwObjectGetPrivateFlags(frame) | (rwFRAMEPRIVATESUBTREESYNCLTM | rwFRAMEPRIVATESUBTREESYNCOBJ));
    return frame;
}


RwFrame* RwFrameCloneHierarchy(RwFrame* root) {
    // decompile 0x7F0250.
    // 0x7F0050: internal recursive clone-and-link helper (outside vendor floor, not in
    // rw_addr_map, no decompile) -> returns clone root or NULL.
    RwFrame* clone = ((RwFrame* (__cdecl*)(RwFrame*, RwUInt32))0x7F0050)(root, 0);
    if (clone) {
        // clone's own object: clear privateFlags bits 0-1 (not yet in sync list)
        rwObjectSetPrivateFlags(clone, rwObjectGetPrivateFlags(clone) & ~(rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ));
        // clone->root (set by the helper): make sure it is in the engine dirty sync list
        RwFrame* cloneRoot = clone->root;
        RwUInt8 cloneRootPf = rwObjectGetPrivateFlags(cloneRoot);
        if ((cloneRootPf & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) == 0) {
            RwLLLink* head = reinterpret_cast<RwLLLink*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0xBC);
            cloneRoot->inDirtyListLink.next = head->next;
            cloneRoot->inDirtyListLink.prev = head;
            head->next->prev = &cloneRoot->inDirtyListLink;
            head->next = &cloneRoot->inDirtyListLink;
        }
        rwObjectSetPrivateFlags(cloneRoot, cloneRootPf | (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ));
        rwObjectSetPrivateFlags(clone, rwObjectGetPrivateFlags(clone) | (rwFRAMEPRIVATESUBTREESYNCLTM | rwFRAMEPRIVATESUBTREESYNCOBJ));
    }
    // purge-tail, vendor-inlined copy of _rwFramePurgeClone(0x7F01A0) applied to the ORIGINAL
    // root (not the clone): restore original hierarchy root pointers.
    RwFrame* child = root->child;
    if (rwObjectGetParent(root) == nullptr) {
        root->root = root;
        for (; child; child = child->next) {
            // 0x7F0210: internal subtree setHierarchyRoot (no decompile, outside floor)
            ((void (__cdecl*)(RwFrame*, RwFrame*))0x7F0210)(child, root);
        }
    } else {
        RwFrame* newRoot = reinterpret_cast<RwFrame*>(rwObjectGetParent(root))->root;
        root->root = newRoot;
        for (; child; child = child->next) {
            ((void (__cdecl*)(RwFrame*, RwFrame*))0x7F0210)(child, newRoot);
        }
    }
    return clone;
}


static inline auto& frameTKList = StaticRef<RwPluginRegistry>(0x8E2270);

RwBool RwFrameDestroyHierarchy(RwFrame* frame) {
    // decompile 0x7F08A0
    if (frame) {
        RwFrame* child = frame->child;
        while (child) {
            RwFrame* next = child->next;
            // 0x7F0830: internal single-frame destroy incl. its subtree (outside floor,
            // not in rw_addr_map, no decompile)
            ((RwBool (__cdecl*)(RwFrame*))0x7F0830)(child);
            child = next;
        }
        _rwPluginRegistryDeInitObject(&frameTKList, frame);
        if ((rwObjectGetPrivateFlags(frame) & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) != 0) {
            // unlink frame->inDirtyListLink from the engine dirty list
            frame->inDirtyListLink.prev->next = frame->inDirtyListLink.next;
            frame->inDirtyListLink.next->prev = frame->inDirtyListLink.prev;
        }
        RwGlobals* engine = RwEngineInstance;
        // frame freeList = *(RwFreeList**)(engine + *(RwUInt32*)0xC9796C)  (decompile DAT_00c9796c offset global)
        RwFreeList* frameFreeList = *reinterpret_cast<RwFreeList**>(reinterpret_cast<RwUInt8*>(engine) + *reinterpret_cast<RwUInt32*>(0xC9796C));
        // free via engine->memoryFree @+0x148
        (*reinterpret_cast<RwMemoryFreeFn*>(reinterpret_cast<RwUInt8*>(engine) + 0x148))(frameFreeList, frame);
    }
    return TRUE;
}


RwFrame* RwFrameForAllChildren(RwFrame* frame, RwFrameCallBack callBack, void* data) {
    // decompile 0x7F0DC0: next sibling is fetched BEFORE the callback runs
    // (callback is allowed to destroy the frame it receives); stop when callback returns NULL.
    RwFrame* child = frame->child;
    while (child) {
        RwFrame* next = child->next;
        if (!callBack(child, data)) {
            break;
        }
        child = next;
    }
    return frame;
}


RwFrame* RwFrameRemoveChild(RwFrame* child) {
    // decompile 0x7F0CD0
    RwFrame* parent = reinterpret_cast<RwFrame*>(rwObjectGetParent(child));
    if (parent->child == child) {
        parent->child = child->next;
    } else {
        RwFrame* prev = parent->child;
        for (RwFrame* c = prev->next; c != child; c = c->next) {
            prev = c;
        }
        prev->next = child->next;
    }
    RwFrame* subTree = child->child;
    child->root = child;
    rwObjectSetParent(child, nullptr);
    child->next = nullptr;
    // 0x7F0210: internal subtree setHierarchyRoot (no decompile, outside floor):
    // retarget the detached subtree's root pointers to `child`
    for (RwFrame* c = subTree; c; c = c->next) {
        ((void (__cdecl*)(RwFrame*, RwFrame*))0x7F0210)(c, child);
    }
    // markDirty(child)
    RwFrame* root = child->root;
    RwUInt8 rootPf = rwObjectGetPrivateFlags(root);
    if ((rootPf & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) == 0) {
        RwLLLink* head = reinterpret_cast<RwLLLink*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0xBC);
        root->inDirtyListLink.next = head->next;
        root->inDirtyListLink.prev = head;
        head->next->prev = &root->inDirtyListLink;
        head->next = &root->inDirtyListLink;
    }
    rwObjectSetPrivateFlags(root, rootPf | (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ));
    rwObjectSetPrivateFlags(child, rwObjectGetPrivateFlags(child) | (rwFRAMEPRIVATESUBTREESYNCLTM | rwFRAMEPRIVATESUBTREESYNCOBJ));
    return child;
}


RwFrame* RwFrameAddChild(RwFrame* parent, RwFrame* child) {
    // decompile 0x7F0B00
    if (rwObjectGetParent(child)) {
        // detach child from its current parent (vendor-inline of RemoveChild head)
        RwFrame* oldParent = reinterpret_cast<RwFrame*>(rwObjectGetParent(child));
        if (oldParent->child == child) {
            oldParent->child = child->next;
        } else {
            RwFrame* prev = oldParent->child;
            for (RwFrame* c = prev->next; c != child; c = c->next) {
                prev = c;
            }
            prev->next = child->next;
        }
        RwFrame* subTree = child->child;
        child->root = child;
        rwObjectSetParent(child, nullptr);
        child->next = nullptr;
        // 0x7F0210: internal subtree setHierarchyRoot (no decompile, outside floor)
        for (RwFrame* c = subTree; c; c = c->next) {
            ((void (__cdecl*)(RwFrame*, RwFrame*))0x7F0210)(c, child);
        }
        // markDirty(child)
        RwFrame* root = child->root;
        RwUInt8 rootPf = rwObjectGetPrivateFlags(root);
        if ((rootPf & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) == 0) {
            RwLLLink* head = reinterpret_cast<RwLLLink*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0xBC);
            root->inDirtyListLink.next = head->next;
            root->inDirtyListLink.prev = head;
            head->next->prev = &root->inDirtyListLink;
            head->next = &root->inDirtyListLink;
        }
        rwObjectSetPrivateFlags(root, rootPf | (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ));
        rwObjectSetPrivateFlags(child, rwObjectGetPrivateFlags(child) | (rwFRAMEPRIVATESUBTREESYNCLTM | rwFRAMEPRIVATESUBTREESYNCOBJ));
    }
    // push-front into parent's child list
    child->next = parent->child;
    parent->child = child;
    rwObjectSetParent(child, parent);
    RwFrame* newRoot = parent->root;
    child->root = newRoot;
    // 0x7F0210: retarget child's subtrees to the new hierarchy root
    for (RwFrame* c = child->child; c; c = c->next) {
        ((void (__cdecl*)(RwFrame*, RwFrame*))0x7F0210)(c, newRoot);
    }
    // child was itself a hierarchy root sitting in the dirty list -> unlink it
    if ((rwObjectGetPrivateFlags(child) & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) != 0) {
        child->inDirtyListLink.prev->next = child->inDirtyListLink.next;
        child->inDirtyListLink.next->prev = child->inDirtyListLink.prev;
        rwObjectSetPrivateFlags(child, rwObjectGetPrivateFlags(child) & ~(rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ));
    }
    // markDirty(child)
    RwFrame* root = child->root;
    RwUInt8 rootPf = rwObjectGetPrivateFlags(root);
    if ((rootPf & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) == 0) {
        RwLLLink* head = reinterpret_cast<RwLLLink*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0xBC);
        root->inDirtyListLink.next = head->next;
        root->inDirtyListLink.prev = head;
        head->next->prev = &root->inDirtyListLink;
        head->next = &root->inDirtyListLink;
    }
    rwObjectSetPrivateFlags(root, rootPf | (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ));
    rwObjectSetPrivateFlags(child, rwObjectGetPrivateFlags(child) | (rwFRAMEPRIVATESUBTREESYNCLTM | rwFRAMEPRIVATESUBTREESYNCOBJ));
    return parent;
}


RwFrame* RwFrameAddChildNoUpdate(RwFrame* parent, RwFrame* child) {
    // decompile 0x7F09C0 — same as RwFrameAddChild but WITHOUT the final markDirty(child)
    if (rwObjectGetParent(child)) {
        // detach child from its current parent (vendor-inline of RemoveChild head)
        RwFrame* oldParent = reinterpret_cast<RwFrame*>(rwObjectGetParent(child));
        if (oldParent->child == child) {
            oldParent->child = child->next;
        } else {
            RwFrame* prev = oldParent->child;
            for (RwFrame* c = prev->next; c != child; c = c->next) {
                prev = c;
            }
            prev->next = child->next;
        }
        RwFrame* subTree = child->child;
        child->root = child;
        rwObjectSetParent(child, nullptr);
        child->next = nullptr;
        // 0x7F0210: internal subtree setHierarchyRoot (no decompile, outside floor)
        for (RwFrame* c = subTree; c; c = c->next) {
            ((void (__cdecl*)(RwFrame*, RwFrame*))0x7F0210)(c, child);
        }
        // markDirty(child)
        RwFrame* root = child->root;
        RwUInt8 rootPf = rwObjectGetPrivateFlags(root);
        if ((rootPf & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) == 0) {
            RwLLLink* head = reinterpret_cast<RwLLLink*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0xBC);
            root->inDirtyListLink.next = head->next;
            root->inDirtyListLink.prev = head;
            head->next->prev = &root->inDirtyListLink;
            head->next = &root->inDirtyListLink;
        }
        rwObjectSetPrivateFlags(root, rootPf | (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ));
        rwObjectSetPrivateFlags(child, rwObjectGetPrivateFlags(child) | (rwFRAMEPRIVATESUBTREESYNCLTM | rwFRAMEPRIVATESUBTREESYNCOBJ));
    }
    // push-front into parent's child list
    child->next = parent->child;
    parent->child = child;
    rwObjectSetParent(child, parent);
    RwFrame* newRoot = parent->root;
    child->root = newRoot;
    // 0x7F0210: retarget child's subtrees to the new hierarchy root.
    // NOTE: unlike RwFrameAddChild there is NO dirty-list unlink of the child and
    // NO trailing markDirty here (that is the "NoUpdate" part).
    for (RwFrame* c = child->child; c; c = c->next) {
        ((void (__cdecl*)(RwFrame*, RwFrame*))0x7F0210)(c, newRoot);
    }
    return parent;
}


RwFrame* RwFrameGetRoot(const RwFrame* frame) {
    // decompile 0x7F09B0: return *(frame + 0xA0)
    return frame->root;
}


RwMatrix* RwFrameGetLTM(RwFrame* frame) {
    // decompile 0x7F0990: if root privateFlags & rwFRAMEPRIVATEHIERARCHYSYNCLTM (0x01)
    // -> _rwFrameSyncHierarchyLTM(root) @0x8097D0 (repo wrapper, rwcore.cpp:974)
    if ((rwObjectGetPrivateFlags(frame->root) & rwFRAMEPRIVATEHIERARCHYSYNCLTM) != 0) {
        _rwFrameSyncHierarchyLTM(frame->root);
    }
    return &frame->ltm;
}


RwFrame* RwFrameUpdateObjects(RwFrame* frame) {
    // decompile 0x7F0910: only the vendor-inline "markDirty" (no object walking here —
    // the object updates happen in _rwFrameSyncDirty/_rwFrameSyncHierarchyLTM).
    RwFrame* root = frame->root;
    RwUInt8 rootPf = rwObjectGetPrivateFlags(root);
    if ((rootPf & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) == 0) {
        RwLLLink* head = reinterpret_cast<RwLLLink*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0xBC);
        root->inDirtyListLink.next = head->next;
        root->inDirtyListLink.prev = head;
        head->next->prev = &root->inDirtyListLink;
        head->next = &root->inDirtyListLink;
    }
    rwObjectSetPrivateFlags(root, rootPf | (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ));
    rwObjectSetPrivateFlags(frame, rwObjectGetPrivateFlags(frame) | (rwFRAMEPRIVATESUBTREESYNCLTM | rwFRAMEPRIVATESUBTREESYNCOBJ));
    return frame;
}


void RwFrameSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    *reinterpret_cast<RwInt32*>(0x8E2288) = blockSize;
    *reinterpret_cast<RwInt32*>(0x8E228C) = numBlocksToPrealloc;
}


RwFrame* RwFrameCreate() {
    // decompile 0x7F0410:
    //   frame = (*(void*(__cdecl**)(engine+0x144)))(*(RwFreeList**)(engine + *(RwUInt32*)0xC9796C), 0x3000E)
    // 0x3000E = rwMEMHINTDUR_FUNCTION(0x30000 in this build's encoding) | rwID_FRAME(0x0E)
    RwGlobals* engine = RwEngineInstance;
    RwFreeList* frameFreeList = *reinterpret_cast<RwFreeList**>(reinterpret_cast<RwUInt8*>(engine) + *reinterpret_cast<RwUInt32*>(0xC9796C));
    RwFrame* frame = reinterpret_cast<RwFrame*>(
        (*reinterpret_cast<RwMemoryAllocFn*>(reinterpret_cast<RwUInt8*>(engine) + 0x144))(frameFreeList, 0x3000E));
    if (frame == nullptr) {
        return nullptr;
    }
    // 0x7F0360: internal _rwObjectHasFrameInitialize (no decompile, outside floor,
    // not in rw_addr_map): rwObjectInitialize + objectList init + root/identity setup
    ((RwBool (__cdecl*)(RwFrame*))0x7F0360)(frame);
    return frame;
}



RwBool RwFrameDestroy(RwFrame* frame) {
    // decompile 0x7F05A0
    assert(frame);
    // run frame plugin destructors (0x808740 = _rwPluginRegistryDeInitObject, repo wrapper)
    _rwPluginRegistryDeInitObject(&frameTKList, frame);
    if (rwObjectGetParent(frame)) {
        // unlink from current parent's child list
        RwFrame* parent = reinterpret_cast<RwFrame*>(rwObjectGetParent(frame));
        if (parent->child == frame) {
            parent->child = frame->next;
        } else {
            RwFrame* prev = parent->child;
            for (RwFrame* c = prev->next; c != frame; c = c->next) {
                prev = c;
            }
            prev->next = frame->next;
        }
        // detach: frame becomes the root of its (still linked) subtree
        RwFrame* subTree = frame->child;
        rwObjectSetParent(frame, nullptr);
        frame->next = nullptr;
        frame->root = frame;
        // 0x7F0210: internal subtree setHierarchyRoot (no decompile, outside floor)
        for (RwFrame* c = subTree; c; c = c->next) {
            ((void (__cdecl*)(RwFrame*, RwFrame*))0x7F0210)(c, frame);
        }
        // markDirty(frame)
        RwFrame* root = frame->root;
        RwUInt8 rootPf = rwObjectGetPrivateFlags(root);
        if ((rootPf & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) == 0) {
            RwLLLink* head = reinterpret_cast<RwLLLink*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0xBC);
            root->inDirtyListLink.next = head->next;
            root->inDirtyListLink.prev = head;
            head->next->prev = &root->inDirtyListLink;
            head->next = &root->inDirtyListLink;
        }
        rwObjectSetPrivateFlags(root, rootPf | (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ));
        rwObjectSetPrivateFlags(frame, rwObjectGetPrivateFlags(frame) | (rwFRAMEPRIVATESUBTREESYNCLTM | rwFRAMEPRIVATESUBTREESYNCOBJ));
    }
    // if frame itself is in the engine dirty list (was a hierarchy root), unlink it
    if ((rwObjectGetPrivateFlags(frame) & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) != 0) {
        frame->inDirtyListLink.prev->next = frame->inDirtyListLink.next;
        frame->inDirtyListLink.next->prev = frame->inDirtyListLink.prev;
    }
    // orphan the children (callback-visible: their parent pointer is nulled, list stays)
    for (RwFrame* c = frame->child; c; c = c->next) {
        rwObjectSetParent(c, nullptr);
    }
    // free via frame freeList (engine + *(RwUInt32*)0xC9796C) and engine->memoryFree @+0x148
    RwGlobals* engine = RwEngineInstance;
    RwFreeList* frameFreeList = *reinterpret_cast<RwFreeList**>(reinterpret_cast<RwUInt8*>(engine) + *reinterpret_cast<RwUInt32*>(0xC9796C));
    (*reinterpret_cast<RwMemoryFreeFn*>(reinterpret_cast<RwUInt8*>(engine) + 0x148))(frameFreeList, frame);
    return TRUE;
}


RwBool _rwFrameInit(RwFrame* frame) {
    // decompile 0x7F0450:
    //   0x7F0360(frame);               // internal _rwObjectHasFrameInitialize (no decompile,
    //                                  // outside floor, not in rw_addr_map)
    //   frame->privateFlags |= rwFRAMEPRIVATESTATIC (0x10)
    ((RwBool (__cdecl*)(RwFrame*))0x7F0360)(frame);
    rwObjectSetPrivateFlags(frame, rwObjectGetPrivateFlags(frame) | rwFRAMEPRIVATESTATIC);
    return TRUE;
}



RwBool _rwFrameDeInit(RwFrame* frame) {
    // decompile 0x7F06F0 — byte-identical to RwFrameDestroy(0x7F05A0) except that the
    // trailing freeList free call is absent (destructor-only variant).
    _rwPluginRegistryDeInitObject(&frameTKList, frame);
    if (rwObjectGetParent(frame)) {
        RwFrame* parent = reinterpret_cast<RwFrame*>(rwObjectGetParent(frame));
        if (parent->child == frame) {
            parent->child = frame->next;
        } else {
            RwFrame* prev = parent->child;
            for (RwFrame* c = prev->next; c != frame; c = c->next) {
                prev = c;
            }
            prev->next = frame->next;
        }
        RwFrame* subTree = frame->child;
        rwObjectSetParent(frame, nullptr);
        frame->next = nullptr;
        frame->root = frame;
        // 0x7F0210: internal subtree setHierarchyRoot (no decompile, outside floor)
        for (RwFrame* c = subTree; c; c = c->next) {
            ((void (__cdecl*)(RwFrame*, RwFrame*))0x7F0210)(c, frame);
        }
        // markDirty(frame)
        RwFrame* root = frame->root;
        RwUInt8 rootPf = rwObjectGetPrivateFlags(root);
        if ((rootPf & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) == 0) {
            RwLLLink* head = reinterpret_cast<RwLLLink*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0xBC);
            root->inDirtyListLink.next = head->next;
            root->inDirtyListLink.prev = head;
            head->next->prev = &root->inDirtyListLink;
            head->next = &root->inDirtyListLink;
        }
        rwObjectSetPrivateFlags(root, rootPf | (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ));
        rwObjectSetPrivateFlags(frame, rwObjectGetPrivateFlags(frame) | (rwFRAMEPRIVATESUBTREESYNCLTM | rwFRAMEPRIVATESUBTREESYNCOBJ));
    }
    if ((rwObjectGetPrivateFlags(frame) & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) != 0) {
        frame->inDirtyListLink.prev->next = frame->inDirtyListLink.next;
        frame->inDirtyListLink.next->prev = frame->inDirtyListLink.prev;
    }
    for (RwFrame* c = frame->child; c; c = c->next) {
        rwObjectSetParent(c, nullptr);
    }
    return TRUE;
}


RwBool RwFrameDirty(const RwFrame* frame) {
    // decompile 0x7F0340: return root->object.privateFlags & 0x03
    return (rwObjectGetPrivateFlags(frame->root) & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) != 0;
}


RwInt32 RwFrameCount(RwFrame* frame) {
    // decompile 0x7F0E00: recursive; 1 + sum over the child sibling chain (calls itself @0x7F0E00)
    RwInt32 count = 1;
    for (RwFrame* child = frame->child; child; child = child->next) {
        count += RwFrameCount(child);
    }
    return count;
}



RwInt32 RwFrameRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    // decompile 0x7F1260: _rwPluginRegistryAddPlugin(&DAT_008e2270, ...) @0x8084A0 (repo wrapper)
    return _rwPluginRegistryAddPlugin(&frameTKList, size, pluginID, constructCB, destructCB, copyCB);
}



RwInt32 RwFrameGetPluginOffset(RwUInt32 pluginID) {
    // decompile 0x7F1290: _rwPluginRegistryGetPluginOffset(&DAT_008e2270, ...) @0x808470 (repo wrapper)
    return _rwPluginRegistryGetPluginOffset(&frameTKList, pluginID);
}


RwBool RwFrameValidatePlugins(const RwFrame* frame) {
    // decompile 0x7F12B0 (size 6): `mov eax,1; ret` — unconditional TRUE. The vendor build
    // short-circuits _rwPluginRegistryValidatePlugins(&frameTKList): with no per-frame
    // plugins registered the validate walk always succeeds.
    (void)frame;
    return TRUE;
}


RwFrame* _rwFrameCloneAndLinkClones(RwFrame* root) {
    RwFrame* clone = ((RwFrame*(__cdecl*)(RwFrame*, void*))0x7F0050)(root, nullptr);
    if (clone != nullptr) {
        clone->object.privateFlags &= static_cast<RwUInt8>(~(rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ));
        RwFrame* origRoot = clone->root;   // +0xA0: clone-of-root points at the original root
        const RwUInt8 pf = origRoot->object.privateFlags;
        if ((pf & (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ)) == 0) {
            rwLinkListAddLLLink(&RwEngineInstance->dirtyFrameList, &origRoot->inDirtyListLink);
        }
        origRoot->object.privateFlags = pf | (rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ);
        clone->object.privateFlags |= (rwFRAMEPRIVATESUBTREESYNCLTM | rwFRAMEPRIVATESUBTREESYNCOBJ);
    }
    return clone;
}


RwFrame* _rwFramePurgeClone(RwFrame* root) {
    // decompile 0x7F01A0: decouple a cloned hierarchy from the originals — restore the
    // root pointers of root's own hierarchy.
    RwFrame* child = root->child;
    if (rwObjectGetParent(root) == nullptr) {
        root->root = root;
        // 0x7F0210: internal subtree setHierarchyRoot (no decompile, outside floor)
        for (; child; child = child->next) {
            ((void (__cdecl*)(RwFrame*, RwFrame*))0x7F0210)(child, root);
        }
    } else {
        RwFrame* newRoot = reinterpret_cast<RwFrame*>(rwObjectGetParent(root))->root;
        root->root = newRoot;
        for (; child; child = child->next) {
            ((void (__cdecl*)(RwFrame*, RwFrame*))0x7F0210)(child, newRoot);
        }
    }
    return root;
}


void _rwObjectHasFrameReleaseFrame(void* object)
{
    RwObjectHasFrame* obj = (RwObjectHasFrame*)object;

    if (obj->object.parent != NULL) {
        RwLLLink* link = &obj->lFrame;
        *(RwLLLink**)link->prev = link->next;
        link->next->prev = link->prev;
    }
}


RwBool _rwFrameSyncDirty() {
    RwLLLink* head = reinterpret_cast<RwLLLink*>(reinterpret_cast<RwUInt8*>(RwEngineInstance) + 0xBC);
    for (RwLLLink* link = head->next; link != head; link = link->next) {
        RwFrame* rootFrame = reinterpret_cast<RwFrame*>(reinterpret_cast<RwUInt8*>(link) - 0x08);
        RwUInt8 rootFlags = rwObjectGetPrivateFlags(rootFrame);

        if ((rootFlags & rwFRAMEPRIVATEHIERARCHYSYNCLTM) == 0) {
            // hierarchy LTM is clean: only push the objects attached to this hierarchy
            for (RwLLLink* objLink = rootFrame->objectList.link.next;
                 objLink != &rootFrame->objectList.link;
                 objLink = objLink->next) {
                RwObjectHasFrame* object = reinterpret_cast<RwObjectHasFrame*>(reinterpret_cast<RwUInt8*>(objLink) - 0x08);
                object->sync(object);
            }
            for (RwFrame* child = rootFrame->child; child != nullptr; child = child->next) {
                for (RwLLLink* objLink = child->objectList.link.next;
                     objLink != &child->objectList.link;
                     objLink = objLink->next) {
                    RwObjectHasFrame* object = reinterpret_cast<RwObjectHasFrame*>(reinterpret_cast<RwUInt8*>(objLink) - 0x08);
                    object->sync(object);
                }
                rwObjectSetPrivateFlags(child, rwObjectGetPrivateFlags(child) & ~rwFRAMEPRIVATESUBTREESYNCOBJ);
                ((void(__cdecl*)(RwFrame*))0x809780)(child->child);
            }
        } else {
            if ((rootFlags & rwFRAMEPRIVATESUBTREESYNCLTM) != 0) {
                rootFrame->ltm = rootFrame->modelling;
            }
            for (RwLLLink* objLink = rootFrame->objectList.link.next;
                 objLink != &rootFrame->objectList.link;
                 objLink = objLink->next) {
                RwObjectHasFrame* object = reinterpret_cast<RwObjectHasFrame*>(reinterpret_cast<RwUInt8*>(objLink) - 0x08);
                object->sync(object);
            }
            for (RwFrame* child = rootFrame->child; child != nullptr; child = child->next) {
                RwUInt8 childFlags = rwObjectGetPrivateFlags(child);
                if (((childFlags & rwFRAMEPRIVATESUBTREESYNCLTM) != 0) ||
                    ((rootFlags & rwFRAMEPRIVATESUBTREESYNCLTM) != 0)) {
                    RwMatrixMultiply(&child->ltm, &child->modelling,
                                     &reinterpret_cast<RwFrame*>(rwObjectGetParent(child))->ltm);
                }
                for (RwLLLink* objLink = child->objectList.link.next;
                     objLink != &child->objectList.link;
                     objLink = objLink->next) {
                    RwObjectHasFrame* object = reinterpret_cast<RwObjectHasFrame*>(reinterpret_cast<RwUInt8*>(objLink) - 0x08);
                    object->sync(object);
                }
                rwObjectSetPrivateFlags(child, childFlags & ~(rwFRAMEPRIVATESUBTREESYNCLTM | rwFRAMEPRIVATESUBTREESYNCOBJ));
                ((void(__cdecl*)(RwFrame*, RwUInt8))0x809700)(child->child, childFlags | (rootFlags & rwFRAMEPRIVATESUBTREESYNCLTM));
            }
        }
        rwObjectSetPrivateFlags(rootFrame, rootFlags & 0xF0);
    }

    // empty the dirty list
    head->next = head;
    head->prev = head;
    return TRUE;
}


void _rwFrameSyncHierarchyLTM(RwFrame* frame) {
    RwUInt8 frameFlags = rwObjectGetPrivateFlags(frame);
    if ((frameFlags & rwFRAMEPRIVATESUBTREESYNCLTM) != 0) {
        frame->ltm = frame->modelling;
    }
    for (RwFrame* child = frame->child; child != nullptr; child = child->next) {
        RwUInt8 childFlags = rwObjectGetPrivateFlags(child);
        RwUInt8 inherited = childFlags | frameFlags;
        if ((inherited & rwFRAMEPRIVATESUBTREESYNCLTM) != 0) {
            RwMatrixMultiply(&child->ltm, &child->modelling,
                             &reinterpret_cast<RwFrame*>(rwObjectGetParent(child))->ltm);
            rwObjectSetPrivateFlags(child, rwObjectGetPrivateFlags(child) & ~rwFRAMEPRIVATESUBTREESYNCLTM);
        }
        ((void(__cdecl*)(RwFrame*, RwUInt8))0x809850)(child->child, inherited);
    }
    rwObjectSetPrivateFlags(frame, frameFlags & 0xFA);
}


RwInt32 RwTextureRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB)
{
    return _rwPluginRegistryAddPluginStream(&StaticRef<RwPluginRegistry>(0x8E23CC), pluginID, readCB, writeCB, getSizeCB);
}


RwInt32 RwTextureSetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB)
{
    return _rwPluginRegistryAddPlgnStrmlwysCB(&StaticRef<RwPluginRegistry>(0x8E23CC), pluginID, alwaysCB);
}


RwUInt32 RwTextureStreamGetSize(const RwTexture* texture)
{
    const RwUInt32  nameSize   = _rwStringStreamGetSize(texture->name);
    const RwUInt32  maskSize   = _rwStringStreamGetSize(texture->mask);
    const RwInt32   pluginSize = _rwPluginRegistryGetSize(&StaticRef<RwPluginRegistry>(0x8E23CC), texture);
    return (RwUInt32)pluginSize + nameSize + 0x34 + maskSize;
}


RwTexture* RwTextureStreamRead(RwStream* stream)
{
    RwUInt32 length;
    RwUInt32 version;
    if (!RwStreamFindChunk(stream, rwID_STRUCT, &length, &version))
        return NULL;

    if (version < 0x34000 || 0x36003 < version) {
        RwError error;
        error.pluginID   = 1;
        error.errorCode  = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000004);
        RwErrorSet(&error);
        return NULL;
    }

    RwUInt32 filterAddressing = 0;
    if (RwStreamRead(stream, &filterAddressing, length) != length)
        return NULL;

    const RwUInt32 filterMode = filterAddressing & 0xFF;
    if ((filterAddressing & 0xF000) == 0) {
        // Old (pre-3.6) streams carry U addressing only: mirror it into V.
        filterAddressing |= (filterAddressing & 0xF00) << 4;
    }

    const RwBool mipmappingWas     = RwTextureGetMipmapping();
    const RwBool autoMipmappingWas = RwTextureGetAutoMipmapping();
    if (filterMode == 3 || filterMode == 4 || filterMode == 5 || filterMode == 6) {
        RwTextureSetMipmapping(TRUE);
        RwTextureSetAutoMipmapping((filterAddressing & 0x10000) == 0);
    } else {
        RwTextureSetMipmapping(FALSE);
        RwTextureSetAutoMipmapping(FALSE);
    }

    RwChar name[128];   // vendor uses 128-byte stack buffers (rwTEXTUREBASENAMELENGTH is 32)
    RwChar mask[128];
    if (_rwStringStreamFindAndRead(name, stream) == NULL) {
        RwTextureSetMipmapping(mipmappingWas);
        RwTextureSetAutoMipmapping(autoMipmappingWas);
        return NULL;
    }
    if (_rwStringStreamFindAndRead(mask, stream) == NULL) {
        RwTextureSetMipmapping(mipmappingWas);
        RwTextureSetAutoMipmapping(autoMipmappingWas);
        return NULL;
    }

    RwTexture* texture = RwTextureRead(name, mask);
    if (texture == NULL) {
        _rwPluginRegistrySkipDataChunks(&StaticRef<RwPluginRegistry>(0x8E23CC), stream);
        RwTextureSetMipmapping(mipmappingWas);
        RwTextureSetAutoMipmapping(autoMipmappingWas);
        return NULL;
    }

    if (texture->refCount == 1)   // texture+0x54; only patch when solely referenced
        texture->filterAddressing = filterAddressing & 0xFFFF;

    RwTextureSetMipmapping(mipmappingWas);
    RwTextureSetAutoMipmapping(autoMipmappingWas);

    if (_rwPluginRegistryReadDataChunks(&StaticRef<RwPluginRegistry>(0x8E23CC), stream, texture) == NULL)
        return NULL;

    return texture;
}


const RwTexture* RwTextureStreamWrite(const RwTexture* texture, RwStream* stream)
{
    RwPluginRegistry& textureRegistry = StaticRef<RwPluginRegistry>(0x8E23CC);

    const RwUInt32 nameSize    = _rwStringStreamGetSize(texture->name);
    const RwUInt32 maskSize    = _rwStringStreamGetSize(texture->mask);
    const RwInt32  pluginSize  = _rwPluginRegistryGetSize(&textureRegistry, texture);

    if (_rwStreamWriteVersionedChunkHeader(stream, rwID_TEXTURE, (RwInt32)(nameSize + maskSize + 0x34 + (RwUInt32)pluginSize), 0x36003, 0xFFFF) == NULL)
        return NULL;
    if (_rwStreamWriteVersionedChunkHeader(stream, rwID_STRUCT, 4, 0x36003, 0xFFFF) == NULL)
        return NULL;

    // struct dword: automipmap flag<<16 | filter+addressing(16 bits).
    // automipmap read from raster->cFormat bit 0x10 (cFormat = rasterFormat >> 8; rwRASTERFORMATAUTOMIPMAP = 0x1000).
    RwUInt32 structData;
    if (texture->raster == NULL || (texture->raster->cFormat & (rwRASTERFORMATAUTOMIPMAP >> 8)))
        structData = 0;
    else
        structData = 1;
    structData = (structData << 0x10) | (texture->filterAddressing & 0xFFFF);

    if (RwStreamWrite(stream, &structData, 4) == NULL)
        return NULL;
    if (_rwStringStreamWrite(texture->name, stream) == NULL)
        return NULL;
    if (_rwStringStreamWrite(texture->mask, stream) == NULL)
        return NULL;
    if (_rwPluginRegistryWriteDataChunks(&textureRegistry, stream, texture) == NULL)
        return NULL;

    return texture;
}


RwInt32 RwTexDictionaryRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB)
{
    return _rwPluginRegistryAddPluginStream(&StaticRef<RwPluginRegistry>(0x8E23E4), pluginID, readCB, writeCB, getSizeCB);
}


RwInt32 RwTexDictionarySetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB)
{
    return _rwPluginRegistryAddPlgnStrmlwysCB(&StaticRef<RwPluginRegistry>(0x8E23E4), pluginID, alwaysCB);
}


struct RwCore8DictSizeAccum
{
    RwUInt32 size;   // +0x00 starts at 0x10 = STRUCT chunk hdr(12) + numTextures(4)
    RwBool   ok;     // +0x04 starts TRUE
};

RwUInt32 RwTexDictionaryStreamGetSize(const RwTexDictionary* texDict)
{
    RwCore8DictSizeAccum accum;
    accum.size = 0x10;
    accum.ok   = TRUE;

    RwTexDictionaryForAllTextures(texDict, (RwTextureCallBack)0x804990, &accum);
    if (!accum.ok)
        return 0;

    return accum.size + 0xC + (RwUInt32)_rwPluginRegistryGetSize(&StaticRef<RwPluginRegistry>(0x8E23E4), texDict);
}


RwTexDictionary* RwTexDictionaryStreamRead(RwStream* stream)
{
    RwUInt32 length;
    RwUInt32 version;
    if (!RwStreamFindChunk(stream, rwID_STRUCT, &length, &version)) {
        NOTSA_LOG_ERR("[TXD] STRUCT chunk NOT FOUND");
        return NULL;
    }
    NOTSA_LOG_DEBUG("[TXD] STRUCT len={} ver={:#x}", length, version);

    if (version < 0x34000 || 0x36003 < version) {
        RwError error;
        error.pluginID   = 1;
        error.errorCode  = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000004);
        RwErrorSet(&error);
        return NULL;
    }

    RwUInt32 structData = 0;
    if (RwStreamRead(stream, &structData, length) != length)
        return NULL;

    // Ask the device for its id (rwDEVICESYSTEMGETID) and compare against the streamed one.
    RwUInt32 deviceId = 0;
    ((RwBool(__cdecl*)(void*, RwInt32, void*, void*, RwInt32))0x7F2AB0)(&RwEngineInstance->dOpenDevice, rwDEVICESYSTEMGETID, &deviceId, NULL, 0);
    deviceId &= 0xFF;

    const RwUInt16 streamDeviceId = (RwUInt16)(structData >> 16);
    if (deviceId != 0 && streamDeviceId != 0 && (RwUInt16)deviceId != streamDeviceId)
        return NULL;

    RwTexDictionary* dict = RwTexDictionaryCreate();
    if (dict == NULL)
        return NULL;

    RwUInt32 remaining = structData & 0xFFFF;
    NOTSA_LOG_DEBUG("[TXD] deviceId={:#x} streamDevId={:#x} textures={}", deviceId, streamDeviceId, remaining);
    while (remaining != 0) {
        remaining--;

        if (!RwStreamFindChunk(stream, rwID_TEXTURENATIVE, &length, &version)) {
            RwTexDictionaryForAllTextures(dict, (RwTextureCallBack)0x804E40, NULL);
            RwTexDictionaryDestroy(dict);
            return NULL;
        }
        if (version < 0x34000 || 0x36003 < version) {
            RwTexDictionaryForAllTextures(dict, (RwTextureCallBack)0x804E40, NULL);
            RwTexDictionaryDestroy(dict);
            RwError error;
            error.pluginID   = 1;
            error.errorCode  = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000004);
            RwErrorSet(&error);
            return NULL;
        }

        RwTexture* texture = NULL;
        const RwBool readOk = RwEngineInstance->stdFunc[rwSTANDARDNATIVETEXTUREREAD](stream, &texture, (RwInt32)length);
        NOTSA_LOG_DEBUG("[TXD] NATIVETEXTUREREAD ok={} texture={}", readOk, (void*)texture);
        if (!readOk || texture == NULL ||
            _rwPluginRegistryReadDataChunks(&StaticRef<RwPluginRegistry>(0x8E23CC), stream, texture) == NULL) {
            RwTexDictionaryForAllTextures(dict, (RwTextureCallBack)0x804E40, NULL);
            RwTexDictionaryDestroy(dict);
            return NULL;
        }

        RwTexDictionaryAddTexture(dict, texture);
    }

    if (_rwPluginRegistryReadDataChunks(&StaticRef<RwPluginRegistry>(0x8E23E4), stream, dict) == NULL) {
        RwTexDictionaryForAllTextures(dict, (RwTextureCallBack)0x804E40, NULL);
        RwTexDictionaryDestroy(dict);
        return NULL;
    }

    return dict;
}


struct RwCore8DictWriteCtx
{
    RwStream* stream;   // +0x00
    RwBool    ok;       // +0x04
};

const RwTexDictionary* RwTexDictionaryStreamWrite(const RwTexDictionary* texDict, RwStream* stream)
{
    RwPluginRegistry& dictRegistry = StaticRef<RwPluginRegistry>(0x8E23E4);

    RwCore8DictSizeAccum accum;
    accum.size = 0x10;
    accum.ok   = TRUE;
    RwTexDictionaryForAllTextures(texDict, (RwTextureCallBack)0x804990, &accum);
    if (!accum.ok)
        return NULL;

    const RwUInt32 total = accum.size + 0xC + (RwUInt32)_rwPluginRegistryGetSize(&dictRegistry, texDict);
    if (total == 0)
        return NULL;

    if (_rwStreamWriteVersionedChunkHeader(stream, rwID_TEXDICTIONARY, (RwInt32)total, 0x36003, 0xFFFF) == NULL)
        return NULL;
    if (_rwStreamWriteVersionedChunkHeader(stream, rwID_STRUCT, 4, 0x36003, 0xFFFF) == NULL)
        return NULL;

    RwUInt32 numTextures = 0;
    RwTexDictionaryForAllTextures(texDict, (RwTextureCallBack)0x804B60, &numTextures);
    if (0xFFFF < numTextures)
        return NULL;

    // struct dword: low16 = numTextures, high16 = device id (vendor keeps only 24 bits).
    RwUInt32 structData = numTextures & 0xFFFF;
    ((RwBool(__cdecl*)(void*, RwInt32, void*, void*, RwInt32))0x7F2AB0)(&RwEngineInstance->dOpenDevice, rwDEVICESYSTEMGETID, (RwUInt16*)&structData + 1, NULL, 0);
    structData &= 0xFFFFFF;

    if (RwStreamWrite(stream, &structData, 4) == NULL)
        return NULL;

    RwCore8DictWriteCtx writeCtx;
    writeCtx.stream = stream;
    writeCtx.ok     = TRUE;
    RwTexDictionaryForAllTextures(texDict, (RwTextureCallBack)0x804B70, &writeCtx);

    if (_rwPluginRegistryWriteDataChunks(&dictRegistry, stream, texDict) != NULL && writeCtx.ok)
        return texDict;
    return NULL;
}


RwTextureChunkInfo* _rwTextureChunkInfoRead(RwStream* stream, RwTextureChunkInfo* textureChunkInfo, RwInt32* bytesRead)
{
    RwUInt32 length;
    if (!RwStreamFindChunk(stream, rwID_STRUCT, &length, NULL))
        return NULL;

    RwUInt32 filterAddressing = 0;
    if (RwStreamRead(stream, &filterAddressing, 4) != 4)
        return NULL;

    *bytesRead = (RwInt32)(length + 0xC);
    RwStreamSkip(stream, length - 4);

    textureChunkInfo->filtering   = (RwTextureFilterMode)((filterAddressing >> 0x00) & 0xFF);
    textureChunkInfo->addressingU = (RwTextureAddressMode)((filterAddressing >> 0x08) & 0xF);
    textureChunkInfo->addressingV = (RwTextureAddressMode)((filterAddressing >> 0x0C) & 0xF);

    return textureChunkInfo;
}


void RwFrameListSetAutoUpdate(RwBool flag) {
    rwFrameListAutoUpdateGta = static_cast<RwUInt32>(flag);
}


RwInt32 RwFrameRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return ((RwInt32(__cdecl*)(RwPluginRegistry*, RwUInt32, RwPluginDataChunkReadCallBack, RwPluginDataChunkWriteCallBack, RwPluginDataChunkGetSizeCallBack))0x8088E0)(
        &rwFramePluginRegistryGta, pluginID, readCB, writeCB, getSizeCB);
}


RwInt32 RwFrameSetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB) {
    return _rwPluginRegistryAddPlgnStrmlwysCB(&rwFramePluginRegistryGta, pluginID, alwaysCB);
}


rwFrameList* _rwFrameListInitialize(rwFrameList* frameList, RwFrame* frame) {
    RwFrame* root = frame;
    RwInt32 numFrames = RwFrameCount(frame);

    RwFrame** frames = static_cast<RwFrame**>(RwGetMemoryFunctionsGta()->rwmalloc(numFrames * 4, 0x4000E));
    if (frames == nullptr) {
        RwError rwError;
        rwError.pluginID = 1;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, numFrames * 4);
        RwErrorSet(&rwError);
        return nullptr;
    }

    if (root != nullptr) {
        frames[0] = root;
        RwFrame* child = root->child;   // +0x98
        if (child != nullptr) {
            frames[1] = child;
            RwFrame** cursor = &frames[2];
            ((void(__cdecl*)(RwFrame**, RwFrame*))0x8076A0)((RwFrame**)&cursor, child->next);   // +0x9C
            ((void(__cdecl*)(RwFrame**, RwFrame*))0x8076A0)((RwFrame**)&cursor, child->child);  // +0x98
        }
    }

    frameList->frames = frames;
    frameList->numFrames = numFrames;
    return frameList;
}


RwBool _rwFrameListFindFrame(const rwFrameList* frameList, const RwFrame* frame, RwInt32* npIndex) {
    RwInt32 index = 0;
    if (0 < frameList->numFrames) {
        RwFrame* const* frames = frameList->frames;
        do {
            if (frames[index] == const_cast<RwFrame*>(frame)) {
                *npIndex = index;
                return TRUE;
            }
            index++;
        } while (index < frameList->numFrames);
    }
    return FALSE;
}


rwFrameList* _rwFrameListDeinitialize(rwFrameList* frameList) {
    if (frameList->numFrames != 0) {
        RwGetMemoryFunctionsGta()->rwfree(frameList->frames);
    }
    return frameList;
}


RwUInt32 _rwFrameListStreamGetSize(const rwFrameList* frameList) {
    RwUInt32 size = frameList->numFrames * 0x38 + 0x10;
    if (0 < frameList->numFrames) {
        RwInt32 i = 0;
        do {
            size += 0xC + ((RwUInt32(__cdecl*)(const RwPluginRegistry*, const void*))0x808B00)(
                              &rwFramePluginRegistryGta, frameList->frames[i]);
            i++;
        } while (i < frameList->numFrames);
    }
    return size;
}


#ifndef RWFRAMELISTSTREAMRECORDGTA_DEFINED
#define RWFRAMELISTSTREAMRECORDGTA_DEFINED
struct RwFrameListStreamRecordGta
{
    RwV3d    right;
    RwV3d    up;
    RwV3d    at;
    RwV3d    pos;
    RwInt32  parentIndex;
    RwUInt32 matrixFlags;
};
static_assert(sizeof(RwFrameListStreamRecordGta) == 0x38, "frame-list stream record must be 0x38");
#endif

// Reverse of decompile 0x807970. Reads STRUCT chunk (version must be 0x34000..0x36003),
// frame count + 0x38-byte records; for each record creates a frame, copies the 12 matrix
// floats (the streamed matrixFlags dword is READ BUT IGNORED, exactly like the binary),
// validates modelling.flags with three out-of-floor matrix predicates and re-attaches the
// frame to its parent. Then reads plugin data chunks per frame.
// Out-of-floor internals (raw casts, absent from rw_addr_map.json):
//   0x7F1500 / 0x7F14A0 / 0x7F1450 — matrix predicates returning RwReal; fail conditions:
//     pred1(m) > rwMatrixToleranceGta || pred2(m) > rwMatrixToleranceGta || pred3(m) < rwMatrixDetMinGta
//     (likely the SDK _rwMatrixNormalError/_rwMatrixOrthogonalError/_rwMatrixDeterminant trio,
//     exact identity not established).
rwFrameList* _rwFrameListStreamRead(RwStream* stream, rwFrameList* fl) {
    RwUInt32 length = 0;
    RwUInt32 version = 0;
    if (RwStreamFindChunk(stream, rwID_STRUCT, &length, &version)) {
        if (version < 0x34000 || 0x36003 < version) {
            RwError rwError;
            rwError.pluginID = 1;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000004);
            RwErrorSet(&rwError);
        } else {
            RwInt32 numFrames = 0;
            if (RwStreamRead(stream, &numFrames, 4) == 4) {
                fl->numFrames = numFrames;
                fl->frames = static_cast<RwFrame**>(RwGetMemoryFunctionsGta()->rwmalloc(numFrames << 2, 0x3000E));
                if (fl->frames == nullptr) {
                    RwError rwError;
                    rwError.pluginID = 1;
                    rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000013, numFrames * 4);
                    RwErrorSet(&rwError);
                    return nullptr;
                }

                RwInt32 frameIdx = 0;
                if (0 < numFrames) {
                    do {
                        RwFrameListStreamRecordGta record;
                        if (RwStreamRead(stream, &record, 0x38) != 0x38) {
                            RwGetMemoryFunctionsGta()->rwfree(fl->frames);
                            return nullptr;
                        }

                        RwFrame* frame = RwFrameCreate();
                        if (frame == nullptr) {
                            RwGetMemoryFunctionsGta()->rwfree(fl->frames);
                            return nullptr;
                        }

                        // 12 matrix floats; +0x1C (modelling.flags) intentionally untouched
                        RwMatrix& matrix = frame->modelling;
                        matrix.right = record.right;
                        matrix.up    = record.up;
                        matrix.at    = record.at;
                        matrix.pos   = record.pos;

                        // validate the streamed matrix against the module tolerances
                        RwReal error = ((RwReal(__cdecl*)(const RwMatrix*))0x7F1500)(&matrix);
                        RwBool keepTypeFlags;
                        if (rwMatrixToleranceGta < error) {
                            keepTypeFlags = FALSE;
                        } else {
                            error = ((RwReal(__cdecl*)(const RwMatrix*))0x7F14A0)(&matrix);
                            if (rwMatrixToleranceGta < error) {
                                keepTypeFlags = FALSE;
                            } else {
                                error = ((RwReal(__cdecl*)(const RwMatrix*))0x7F1450)(&matrix);
                                keepTypeFlags = (rwMatrixDetMinGta <= error) ? TRUE : FALSE;
                            }
                        }
                        if (!keepTypeFlags)
                            matrix.flags &= ~(rwMATRIXTYPEMASK | rwMATRIXINTERNALIDENTITY);  // & 0xfffdfffc
                        else
                            matrix.flags &= ~0x00010000u;                                    // & 0xfffdffff

                        fl->frames[frameIdx] = frame;

                        if (record.parentIndex >= 0) {
                            if (rwFrameListAutoUpdateGta == 0) {
                                RwFrameAddChildNoUpdate(fl->frames[record.parentIndex], frame);
                            } else {
                                RwFrameAddChild(fl->frames[record.parentIndex], frame);
                            }
                        }
                        frameIdx++;
                    } while (frameIdx < numFrames);
                }

                for (RwInt32 i = 0; i < numFrames; i++) {
                    if (_rwPluginRegistryReadDataChunks(&rwFramePluginRegistryGta, stream, fl->frames[i]) == nullptr) {
                        RwFrameDestroyHierarchy(fl->frames[0]);   // decompile: destroys frames[0] only
                        RwGetMemoryFunctionsGta()->rwfree(fl->frames);
                        return nullptr;
                    }
                }
                return fl;
            }
        }
    }
    return nullptr;
}


#ifndef RWFRAMELISTSTREAMRECORDGTA_DEFINED
#define RWFRAMELISTSTREAMRECORDGTA_DEFINED
static_assert(sizeof(RwFrameListStreamRecordGta) == 0x38, "frame-list stream record must be 0x38");
#endif

// Reverse of decompile 0x8077A0: writes FRAMELIST chunk (rwID_FRAMELIST, version 0x36003),
// STRUCT chunk with count + per-frame records, then plugin data blocks per frame.
// 0x808B00/_rwStreamWriteVersionedChunkHeader(0x7ED270)/0x808B40: 0x808B00 and 0x808B40 are
// outside rw_addr_map.json => raw casts (SDK prototypes _rwPluginRegistryGetSize rwplcore.h:3749 /
// _rwPluginRegistryWriteDataChunks rwplcore.h:3744); 0x7ED270 is translated in rw/rwplcore.cpp:2590.
const rwFrameList* _rwFrameListStreamWrite(const rwFrameList* frameList, RwStream* stream) {
    RwUInt32 size = frameList->numFrames * 0x38 + 0x10;
    if (0 < frameList->numFrames) {
        RwInt32 i = 0;
        do {
            size += 0xC + ((RwUInt32(__cdecl*)(const RwPluginRegistry*, const void*))0x808B00)(
                              &rwFramePluginRegistryGta, frameList->frames[i]);
            i++;
        } while (i < frameList->numFrames);
    }

    if (_rwStreamWriteVersionedChunkHeader(stream, rwID_FRAMELIST, static_cast<RwInt32>(size), 0x36003, 0xFFFF) == nullptr)
        return nullptr;
    if (_rwStreamWriteVersionedChunkHeader(stream, rwID_STRUCT, frameList->numFrames * 0x38 + 4, 0x36003, 0xFFFF) == nullptr)
        return nullptr;

    RwInt32 numFrames = frameList->numFrames;
    if (RwStreamWrite(stream, &numFrames, 4) == nullptr)
        return nullptr;

    for (RwInt32 i = 0; i < numFrames; i++) {
        RwFrameListStreamRecordGta record;
        RwFrame* frame = frameList->frames[i];

        record.right.x = *reinterpret_cast<RwReal*>(reinterpret_cast<RwUInt8*>(frame) + 0x10);
        record.right.y = *reinterpret_cast<RwReal*>(reinterpret_cast<RwUInt8*>(frame) + 0x14);
        record.right.z = *reinterpret_cast<RwReal*>(reinterpret_cast<RwUInt8*>(frame) + 0x18);
        record.up.x    = *reinterpret_cast<RwReal*>(reinterpret_cast<RwUInt8*>(frame) + 0x20);
        record.up.y    = *reinterpret_cast<RwReal*>(reinterpret_cast<RwUInt8*>(frame) + 0x24);
        record.up.z    = *reinterpret_cast<RwReal*>(reinterpret_cast<RwUInt8*>(frame) + 0x28);
        record.at.x    = *reinterpret_cast<RwReal*>(reinterpret_cast<RwUInt8*>(frame) + 0x30);
        record.at.y    = *reinterpret_cast<RwReal*>(reinterpret_cast<RwUInt8*>(frame) + 0x34);
        record.at.z    = *reinterpret_cast<RwReal*>(reinterpret_cast<RwUInt8*>(frame) + 0x38);
        record.pos.x   = *reinterpret_cast<RwReal*>(reinterpret_cast<RwUInt8*>(frame) + 0x40);
        record.pos.y   = *reinterpret_cast<RwReal*>(reinterpret_cast<RwUInt8*>(frame) + 0x44);
        record.pos.z   = *reinterpret_cast<RwReal*>(reinterpret_cast<RwUInt8*>(frame) + 0x48);

        // parent index: scan this frame list for frame->object.parent (+0x04), else -1
        record.parentIndex = -1;
        RwFrame* parent = reinterpret_cast<RwFrame*>(rwObjectGetParent(frame));
        if (parent != nullptr) {
            for (RwInt32 k = 0; k < numFrames; k++) {
                if (frameList->frames[k] == parent) {
                    record.parentIndex = k;
                    break;
                }
            }
        }
        record.matrixFlags = *reinterpret_cast<RwUInt32*>(reinterpret_cast<RwUInt8*>(frame) + 0x1C); // modelling.flags

        if (RwStreamWrite(stream, &record, 0x38) == nullptr)
            return nullptr;
    }

    for (RwInt32 i = 0; i < numFrames; i++) {
        if (((const RwPluginRegistry*(__cdecl*)(const RwPluginRegistry*, RwStream*, const void*))0x808B40)(
                &rwFramePluginRegistryGta, stream, frameList->frames[i]) == nullptr)
            return nullptr;
    }
    return frameList;
}


RwBBox* RwBBoxCalculate(RwBBox* boundBox, const RwV3d* verts, RwInt32 numVerts) {
    boundBox->inf = verts[0];
    boundBox->sup = verts[0];

    const RwV3d* vertex = verts;
    RwInt32 remaining = numVerts;
    while (--remaining != 0) {
        vertex++;
        if (vertex->x < boundBox->inf.x) boundBox->inf.x = vertex->x;
        if (vertex->y < boundBox->inf.y) boundBox->inf.y = vertex->y;
        if (vertex->z < boundBox->inf.z) boundBox->inf.z = vertex->z;
        if (boundBox->sup.x < vertex->x) boundBox->sup.x = vertex->x;
        if (boundBox->sup.y < vertex->y) boundBox->sup.y = vertex->y;
        if (boundBox->sup.z < vertex->z) boundBox->sup.z = vertex->z;
    }
    return boundBox;
}


RwBBox* RwBBoxInitialize(RwBBox* boundBox, const RwV3d* vertex) {
    boundBox->inf = *vertex;
    boundBox->sup = *vertex;
    return boundBox;
}


RwBBox* RwBBoxAddPoint(RwBBox* boundBox, const RwV3d* vertex) {
    if (vertex->x < boundBox->inf.x) boundBox->inf.x = vertex->x;
    if (vertex->y < boundBox->inf.y) boundBox->inf.y = vertex->y;
    if (vertex->z < boundBox->inf.z) boundBox->inf.z = vertex->z;
    if (boundBox->sup.x < vertex->x) boundBox->sup.x = vertex->x;
    if (boundBox->sup.y < vertex->y) boundBox->sup.y = vertex->y;
    if (boundBox->sup.z < vertex->z) boundBox->sup.z = vertex->z;
    return boundBox;
}


RwBool RwBBoxContainsPoint(const RwBBox* boundBox, const RwV3d* vertex) {
    if (vertex->x < boundBox->inf.x) return FALSE;
    if (vertex->y < boundBox->inf.y) return FALSE;
    if (vertex->z < boundBox->inf.z) return FALSE;
    if (boundBox->sup.x < vertex->x) return FALSE;
    if (boundBox->sup.y < vertex->y) return FALSE;
    if (boundBox->sup.z < vertex->z) return FALSE;
    return TRUE;
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
    *reinterpret_cast<RwInt32*>(0x8E2244) = blockSize;
    *reinterpret_cast<RwInt32*>(0x8E2248) = numBlocksToPrealloc;
}


RwBool RwCameraDestroy(RwCamera* camera) {
    _rwPluginRegistryDeInitObject(reinterpret_cast<RwPluginRegistry*>(0x8E222C), camera);
    _rwObjectHasFrameReleaseFrame(camera);
    RwGlobals*         engine         = RwEngineInstance;
    RwFreeList*        cameraFreeList = *reinterpret_cast<RwFreeList**>(reinterpret_cast<RwUInt8*>(engine) + *reinterpret_cast<RwUInt32*>(0xC97960));
    (*reinterpret_cast<RwMemoryFreeFn*>(reinterpret_cast<RwUInt8*>(engine) + 0x148))(cameraFreeList, camera);
    return TRUE;
}


RwCamera* RwCameraCreate() {
    RwGlobals*  engine         = RwEngineInstance;
    RwFreeList* cameraFreeList = *reinterpret_cast<RwFreeList**>(reinterpret_cast<RwUInt8*>(engine) + *reinterpret_cast<RwUInt32*>(0xC97960));
    RwCamera*   camera         = reinterpret_cast<RwCamera*>(
        (*reinterpret_cast<RwMemoryAllocFn*>(reinterpret_cast<RwUInt8*>(engine) + 0x144))(cameraFreeList, 0x30005));
    if (camera == nullptr) {
        return nullptr;
    }
    camera->object.object.type         = 4;   // rwCAMERA object type id (literal per decompile; no named constant in repo headers)
    camera->object.object.subType      = 0;
    camera->object.object.flags        = 0;
    camera->object.object.privateFlags = 0;
    camera->object.object.parent       = nullptr;
    camera->object.sync                = (RwObjectHasFrameSyncFunction)0x7EE5A0;
    camera->beginUpdate                = (RwCameraBeginUpdateFunc)0x7EF370;
    camera->endUpdate                  = (RwCameraEndUpdateFunc)0x7EF340;
    camera->viewWindow.x      = 1.0f;    // 0x3F800000
    camera->viewWindow.y      = 1.0f;
    camera->recipViewWindow.x = 1.0f;
    camera->recipViewWindow.y = 1.0f;
    camera->viewOffset.x      = 0.0f;
    camera->viewOffset.y      = 0.0f;
    camera->nearPlane         = 0.05f;   // 0x3D4CCCCD
    camera->farPlane          = 10.0f;   // 0x41200000
    camera->fogPlane          = 5.0f;    // 0x40A00000
    camera->frameBuffer       = nullptr;
    camera->zBuffer           = nullptr;
    camera->projectionType    = rwPERSPECTIVE;
    ((void(__cdecl*)(RwCamera*))0x7EE200)(camera);   // zScale/zShift recalc (see 0x7EE1D0.cpp)
    camera->viewMatrix.flags = 0;
    _rwPluginRegistryInitObject(reinterpret_cast<RwPluginRegistry*>(0x8E222C), camera);
    return camera;
}


RwCamera* RwCameraClone(RwCamera* camera) {
    RwCamera* clone = RwCameraCreate();
    if (clone == nullptr) {
        return nullptr;
    }
    _rwObjectHasFrameSetFrame(clone, reinterpret_cast<RwFrame*>(rwObjectGetParent(camera)));   // 0x804EF0, repo rwcore.cpp:1169
    clone->beginUpdate     = camera->beginUpdate;    // +0x18
    clone->endUpdate       = camera->endUpdate;      // +0x1C
    clone->viewWindow      = camera->viewWindow;     // +0x68
    clone->recipViewWindow = camera->recipViewWindow; // +0x70
    clone->viewOffset      = camera->viewOffset;     // +0x78
    clone->nearPlane       = camera->nearPlane;      // +0x80
    clone->farPlane        = camera->farPlane;       // +0x84
    clone->fogPlane        = camera->fogPlane;       // +0x88
    clone->frameBuffer     = camera->frameBuffer;    // +0x60
    clone->zBuffer         = camera->zBuffer;        // +0x64
    clone->projectionType  = camera->projectionType; // +0x14
    ((void(__cdecl*)(RwCamera*))0x7EE200)(clone);    // zScale/zShift recalc (see 0x7EE1D0.cpp)
    _rwPluginRegistryCopyObject(reinterpret_cast<RwPluginRegistry*>(0x8E222C), clone, camera);
    return clone;
}


RwCamera* RwCameraSetViewOffset(RwCamera* camera, const RwV2d* offset) {
    camera->viewOffset = *offset;   // decompile: two dword copies to +0x78/+0x7C
    if (rwObjectGetParent(camera) != nullptr) {
        RwFrameUpdateObjects(reinterpret_cast<RwFrame*>(rwObjectGetParent(camera)));
    }
    return camera;
}


RwCamera* RwCameraSetViewWindow(RwCamera* camera, const RwV2d* viewWindow) {
    camera->viewWindow = *viewWindow;                     // +0x68
    camera->recipViewWindow.x = 1.0f / camera->viewWindow.x;   // +0x70
    camera->recipViewWindow.y = 1.0f / camera->viewWindow.y;   // +0x74
    if (rwObjectGetParent(camera) != nullptr) {
        RwFrameUpdateObjects(reinterpret_cast<RwFrame*>(rwObjectGetParent(camera)));
    }
    return camera;
}


RwCamera* RwCameraSetProjection(RwCamera* camera, RwCameraProjection projection) {
    if ((0 < projection) && (projection < 3)) {
        camera->projectionType = projection;   // +0x14
        if (rwObjectGetParent(camera) != nullptr) {
            RwFrameUpdateObjects(reinterpret_cast<RwFrame*>(rwObjectGetParent(camera)));
        }
        ((void(__cdecl*)(RwCamera*))0x7EE200)(camera);   // zScale/zShift recalc (see 0x7EE1D0.cpp)
        return camera;
    }
    // error path: _rwerror(0x80000003, "Invalid projection type specified") — string
    // @0x8E224C verified in binary; 0x80000003 = core error (MSB set), pluginID 1 = core
    RwError rwError;
    rwError.pluginID = 1;
    rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000003, "Invalid projection type specified");
    RwErrorSet(&rwError);
    return nullptr;
}


RwCamera* RwCameraSetNearClipPlane(RwCamera* camera, RwReal nearClip) {
    camera->nearPlane = nearClip;   // +0x80
    ((void(__cdecl*)(RwCamera*))0x7EE200)(camera);
    if (rwObjectGetParent(camera) != nullptr) {
        RwFrameUpdateObjects(reinterpret_cast<RwFrame*>(rwObjectGetParent(camera)));
    }
    return camera;
}


RwCamera* RwCameraSetFarClipPlane(RwCamera* camera, RwReal farClip) {
    camera->farPlane = farClip;   // +0x84
    ((void(__cdecl*)(RwCamera*))0x7EE200)(camera);
    if (rwObjectGetParent(camera) != nullptr) {
        RwFrameUpdateObjects(reinterpret_cast<RwFrame*>(rwObjectGetParent(camera)));
    }
    return camera;
}


RwInt32 RwCameraRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return _rwPluginRegistryAddPlugin(reinterpret_cast<RwPluginRegistry*>(0x8E222C), size, pluginID, constructCB, destructCB, copyCB);
}


RwInt32 RwCameraGetPluginOffset(RwUInt32 pluginID) {
    return _rwPluginRegistryGetPluginOffset(reinterpret_cast<const RwPluginRegistry*>(0x8E222C), pluginID);
}


RwBool RwCameraValidatePlugins(const RwCamera* camera) {
    (void)camera;
    return TRUE;
}


RwFrustumTestResult RwCameraFrustumTestSphere(const RwCamera* camera, const RwSphere* sphere) {
    RwFrustumTestResult result = rwSPHEREINSIDE;
    for (RwInt32 i = 0; i < 6; i++) {
        const RwFrustumPlane* fp = &camera->frustumPlanes[i];
        const RwReal dist = fp->plane.normal.x * sphere->center.x
                          + fp->plane.normal.y * sphere->center.y
                          + fp->plane.normal.z * sphere->center.z
                          - fp->plane.distance;
        if (sphere->radius < dist) {
            return rwSPHEREOUTSIDE;
        }
        if (-sphere->radius < dist) {
            result = rwSPHEREBOUNDARY;
        }
    }
    return result;
}


RwInt32 RwCameraRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return ((RwInt32(__cdecl*)(RwPluginRegistry*, RwUInt32, RwPluginDataChunkReadCallBack, RwPluginDataChunkWriteCallBack, RwPluginDataChunkGetSizeCallBack))0x8088E0)(
        &rwCameraPluginRegistryGta, pluginID, readCB, writeCB, getSizeCB);
}


RwInt32 RwCameraSetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB) {
    return _rwPluginRegistryAddPlgnStrmlwysCB(&rwCameraPluginRegistryGta, pluginID, alwaysCB);
}


RwUInt32 RwCameraStreamGetSize(const RwCamera* camera) {
    return ((RwUInt32(__cdecl*)(const RwPluginRegistry*, const void*))0x808B00)(&rwCameraPluginRegistryGta, camera) + 0x38;
}


RwCamera* RwCameraStreamRead(RwStream* stream) {
    RwUInt32 length = 0;
    RwUInt32 version = 0;
    if (RwStreamFindChunk(stream, rwID_STRUCT, &length, &version)) {
        if (version < 0x34000 || 0x36003 < version) {
            RwError rwError;
            rwError.pluginID = 1;
            rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000004);
            RwErrorSet(&rwError);
        } else {
            RwCameraChunkInfo chunkInfo;
            for (RwUInt32* p = reinterpret_cast<RwUInt32*>(&chunkInfo); p < reinterpret_cast<RwUInt32*>(&chunkInfo) + 8; p++)
                *p = 0;

            if (RwStreamRead(stream, &chunkInfo, length) == length) {
                RwCamera* camera = RwCameraCreate();
                if (camera != nullptr) {
                    if (_rwPluginRegistryReadDataChunks(&rwCameraPluginRegistryGta, stream, camera) != nullptr) {
                        RwCameraSetViewWindow(camera, &chunkInfo.viewWindow);
                        RwCameraSetViewOffset(camera, &chunkInfo.viewOffset);
                        RwCameraSetNearClipPlane(camera, chunkInfo.nearPlane);
                        RwCameraSetFarClipPlane(camera, chunkInfo.farPlane);
                        *reinterpret_cast<RwReal*>(reinterpret_cast<RwUInt8*>(camera) + 0x88) = chunkInfo.fogPlane;  // fogPlane
                        RwCameraSetProjection(camera, static_cast<RwCameraProjection>(chunkInfo.projection));
                        return camera;
                    }
                }
            }
        }
    }
    return nullptr;
}


const RwCamera* RwCameraStreamWrite(const RwCamera* camera, RwStream* stream) {
    RwUInt32 size = ((RwUInt32(__cdecl*)(const RwPluginRegistry*, const void*))0x808B00)(&rwCameraPluginRegistryGta, camera) + 0x38;
    if (_rwStreamWriteVersionedChunkHeader(stream, rwID_CAMERA, static_cast<RwInt32>(size), 0x36003, 0xFFFF) == nullptr)
        return nullptr;
    if (_rwStreamWriteVersionedChunkHeader(stream, rwID_STRUCT, 0x20, 0x36003, 0xFFFF) == nullptr)
        return nullptr;

    RwCameraChunkInfo chunkInfo;
    chunkInfo.viewWindow.x  = *reinterpret_cast<const RwReal*>(reinterpret_cast<const RwUInt8*>(camera) + 0x68);
    chunkInfo.viewWindow.y  = *reinterpret_cast<const RwReal*>(reinterpret_cast<const RwUInt8*>(camera) + 0x6C);
    chunkInfo.viewOffset.x  = *reinterpret_cast<const RwReal*>(reinterpret_cast<const RwUInt8*>(camera) + 0x78);
    chunkInfo.viewOffset.y  = *reinterpret_cast<const RwReal*>(reinterpret_cast<const RwUInt8*>(camera) + 0x7C);
    chunkInfo.nearPlane     = *reinterpret_cast<const RwReal*>(reinterpret_cast<const RwUInt8*>(camera) + 0x80);
    chunkInfo.farPlane      = *reinterpret_cast<const RwReal*>(reinterpret_cast<const RwUInt8*>(camera) + 0x84);
    chunkInfo.fogPlane      = *reinterpret_cast<const RwReal*>(reinterpret_cast<const RwUInt8*>(camera) + 0x88);
    chunkInfo.projection    = *reinterpret_cast<const RwUInt32*>(reinterpret_cast<const RwUInt8*>(camera) + 0x14);

    if (RwStreamWrite(stream, &chunkInfo, 0x20) == nullptr)
        return nullptr;

    if (((const RwPluginRegistry*(__cdecl*)(const RwPluginRegistry*, RwStream*, const void*))0x808B40)(
            &rwCameraPluginRegistryGta, stream, camera) == nullptr)
        return nullptr;
    return camera;
}


RwCameraChunkInfo* RwCameraChunkInfoRead(RwStream* stream, RwCameraChunkInfo* cameraChunkInfo, RwInt32* bytesRead) {
    RwUInt32 length = 0;
    if (RwStreamFindChunk(stream, rwID_STRUCT, &length, nullptr)) {
        for (RwUInt32* p = reinterpret_cast<RwUInt32*>(cameraChunkInfo); p < reinterpret_cast<RwUInt32*>(cameraChunkInfo) + 8; p++)
            *p = 0;

        if (RwStreamRead(stream, cameraChunkInfo, 0x20) != 0x20)
            return nullptr;

        *bytesRead = static_cast<RwInt32>(length) + 0xC;
        RwStreamSkip(stream, length - 0x20);
        return cameraChunkInfo;
    }
    return nullptr;
}


void _rwObjectHasFrameSetFrame(void *object, RwFrame *frame) {
    plugin::Call<0x804EF0, void*, RwFrame*>(object, frame);
}



RwUInt32 RwRGBAToPixel(RwRGBA* rgbIn, RwInt32 rasterFormat) {
    return plugin::CallAndReturn<RwUInt32, 0x803740, RwRGBA*, RwInt32>(rgbIn, rasterFormat);
}
