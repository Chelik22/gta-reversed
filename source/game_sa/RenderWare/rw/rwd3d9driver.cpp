#include "StdInc.h"

#include "rwcore.h"

/*
    Reversed batch (slice 160-239 of uncovered_big.json):

    RenderWare D3D9 driver core: the deferred render-state layer, the cached
    device setters, and the light / stencil / caps / callback accessors that
    live in gta_sa.exe at 0x7F8580..0x7FC430 (between the windowed-mode
    helpers and the raster/texture internals).

    Every body below is a faithful reimplementation of the original code,
    using the very same driver globals (mapped at their original addresses).
    The thunks in rwcore.cpp are kept untouched: once these hooks are active
    the original code paths (and the repo's thunks, which call the original
    addresses) all converge onto these bodies.

    Driver globals referenced (original addresses):
        0xC97C1C  WindowHandle                     (unchanged here)
        0xC97C20  IDirect3D9*                      _RwDirect3DObject
        0xC97C24  adapter ordinal                  _RwD3DAdapterIndex
        0x8E2428  D3DDEVTYPE                       _RwD3DAdapterType
        0xC9BEF0  D3DFORMAT                        _RwD3DAdapterFormat
        0x884788  D3DFORMAT[3]                     adapter format list
        0x8E243C  full screen refresh rate         FullScreenRefreshRateInHz
        0xC97C44  stencil clear value              StencilClearValue
        0xC9BF00  D3DCAPS9                         _RwD3D9DeviceCaps
        0xC980B0  device restore callback          D3D9RestoreDeviceCallback
        0xC98084  light cache capacity             MaxNumLights
        0xC98088  RxD3D9Light[capacity]            (D3DLIGHT9 + RwBool enabled)
        0xC98B40  deferred RS dirty list           [210]
        0xC98E88  deferred RS current values       [210]
        0xC991D0  deferred RS {value, dirty}       [210][2]
        0xC9A5EC  deferred RS dirty count
        0xC980F8  deferred TSS {value, dirty}      [8*33][2]
        0xC99860  deferred TSS current values      [8*33]
        0xC99C80  deferred TSS dirty {stage, st}   [8*33][2]
        0xC9A5F0  deferred TSS dirty count
        0xC98938  sampler state cache              [8*14]
        0xC97C70  transform cache slots (slot for D3DTS_WORLD lands at
                  0xC98070 == 0xC97C70 + 4 * 0x100, by design)
        0x8847A0  identity D3DMATRIX
*/
namespace RwD3D9Driver {
// Deferred render state (D3DRS) cache ----------------------------------------
static auto& RenderStateDirtyList = StaticRef<RwUInt32[210]>(0xC98B40); // waiting-to-be-set states
static auto& RenderStateCurrent   = StaticRef<RwUInt32[210]>(0xC98E88); // value currently on the device
static auto& RenderStateValues    = StaticRef<RwUInt32[210][2]>(0xC991D0); // [state] = { new value, dirty }
static auto& RenderStateDirtyCount = StaticRef<RwUInt32>(0xC9A5EC);

// Deferred texture stage state (D3DTSS) cache --------------------------------
static constexpr unsigned TSS_PER_STAGE = 0x21; // 33 TSS slots per stage
static auto& TSSValues     = StaticRef<RwUInt32[8 * TSS_PER_STAGE][2]>(0xC980F8); // [stage * 33 + state] = { new value, dirty }
static auto& TSSCurrent    = StaticRef<RwUInt32[8 * TSS_PER_STAGE]>(0xC99860);
static auto& TSSDirtyPairs = StaticRef<RwUInt32[8 * TSS_PER_STAGE][2]>(0xC99C80); // [i] = { stage, state }
static auto& TSSDirtyCount = StaticRef<RwUInt32>(0xC9A5F0);

// Sampler state cache (applied directly, not deferred) -----------------------
static constexpr unsigned SAMPLER_STATES = 0xE; // 14 sampler states per stage
static auto& SamplerValues = StaticRef<RwUInt32[8 * SAMPLER_STATES]>(0xC98938); // [stage * 14 + state]

// Adapter / device ------------------------------------------------------------
static auto& AdapterFormats    = StaticRef<D3DFORMAT[3]>(0x884788);
static auto& AdapterFormat     = StaticRef<D3DFORMAT>(0xC9BEF0);
static auto& AdapterIndex      = StaticRef<RwUInt32>(0xC97C24);
static auto& AdapterType       = StaticRef<D3DDEVTYPE>(0x8E2428);
static auto& Direct3DObject    = StaticRef<IDirect3D9*>(0xC97C20);
static auto& RefreshRate       = StaticRef<RwUInt32>(0x8E243C);
static auto& StencilClearValue = StaticRef<RwUInt32>(0xC97C44);
static auto& DeviceCaps        = StaticRef<D3DCAPS9>(0xC9BF00);
static auto& RestoreCallback   = StaticRef<rwD3D9DeviceRestoreCallBack>(0xC980B0);

// Light cache ------------------------------------------------------------------
// RxD3D9Light: D3DLIGHT9 followed by the enabled flag (0x6C bytes total)
struct DriverLight {
    D3DLIGHT9 light;
    RwBool    enabled;
};
static_assert(sizeof(DriverLight) == 0x6C);
static auto& MaxLights = StaticRef<RwInt32>(0xC98084);
static auto& Lights    = StaticRef<DriverLight*>(0xC98088);

// Transform cache ---------------------------------------------------------------
// Pointer slots indexed directly by the D3DTRANSFORMSTATETYPE; the layout is
// sparse on purpose: the slot for D3DTS_WORLD (0x100) is 0xC98070.
static auto& TransformSlotsBase = StaticRef<RwUInt8>(0xC97C70);
static auto& IdentityMatrix     = StaticRef<D3DMATRIX>(0x8847A0);

static void** TransformSlot(RwUInt32 state) {
    return reinterpret_cast<void**>((&TransformSlotsBase) + 4 * state);
}

// Defined further below; forward declared as the draw wrappers flush the cache first.
static void RenderStateFlushCache();

// 0x7F8580
// Scans all adapter display modes and picks the highest refresh rate that is
// still <= the requested one (the requested rate itself wins if present).
static void EngineSetRefreshRate(RwUInt32 requestedRefreshRate) {
    RefreshRate = requestedRefreshRate;
    if (requestedRefreshRate == 0)
        return;

    RwUInt32 best = 0;
    for (const auto format : AdapterFormats) {
        const UINT modeCount = Direct3DObject->GetAdapterModeCount(AdapterIndex, format);
        for (INT i = (INT)modeCount - 1; i >= 0; i--) {
            D3DDISPLAYMODE mode{};
            Direct3DObject->EnumAdapterModes(AdapterIndex, format, (UINT)i, &mode);
            if (mode.RefreshRate == RefreshRate)
                best = RefreshRate;
            else if (mode.RefreshRate < RefreshRate && best < mode.RefreshRate)
                best = mode.RefreshRate;
        }
    }
    if (RefreshRate != best)
        RefreshRate = best;
}

// 0x7F9C30
// Reports whether the current device supports compressed (DXT) textures.
// The DXT1..DXT5 formats are probed in order, first hit wins.
static RwBool DeviceSupportsDXTTexture() {
    static constexpr D3DFORMAT dxtFormats[5] = {
        (D3DFORMAT)0x31545844, // MAKEFOURCC('D', 'X', 'T', '1')
        (D3DFORMAT)0x32545844, // MAKEFOURCC('D', 'X', 'T', '2')
        (D3DFORMAT)0x33545844, // MAKEFOURCC('D', 'X', 'T', '3')
        (D3DFORMAT)0x34545844, // MAKEFOURCC('D', 'X', 'T', '4')
        (D3DFORMAT)0x35545844, // MAKEFOURCC('D', 'X', 'T', '5')
    };
    for (const auto format : dxtFormats) {
        if (SUCCEEDED(Direct3DObject->CheckDeviceFormat(AdapterIndex, AdapterType, AdapterFormat, 0, D3DRTYPE_TEXTURE, format)))
            return TRUE;
    }
    return FALSE;
}

// 0x7F9D30
static void SetStencilClear(RwUInt32 stencilClear) {
    StencilClearValue = stencilClear;
}

// 0x7F9D40
static RwUInt32 GetStencilClear() {
    return StencilClearValue;
}

// 0x7F9F70
static void SetVertexDeclaration(void* vertexDeclaration) {
    if (_rwD3D9LastVertexDeclarationUsed != vertexDeclaration) {
        const HRESULT hr = _RwD3DDevice->SetVertexDeclaration((LPDIRECT3DVERTEXDECLARATION9)vertexDeclaration);
        _rwD3D9LastVertexDeclarationUsed = (void*)-1;
        if (SUCCEEDED(hr))
            _rwD3D9LastVertexDeclarationUsed = vertexDeclaration;
        _rwD3D9LastFVFUsed = 0xFFFFFFFF;
    }
}

// 0x7F9FB0
static void SetVertexShader(void* shader) {
    if (_rwD3D9LastVertexShaderUsed != shader) {
        const HRESULT hr = _RwD3DDevice->SetVertexShader((LPDIRECT3DVERTEXSHADER9)shader);
        _rwD3D9LastVertexShaderUsed = (void*)-1;
        if (SUCCEEDED(hr))
            _rwD3D9LastVertexShaderUsed = shader;
    }
}

// 0x7F9FF0
static void SetPixelShader(void* shader) {
    if (_rwD3D9LastPixelShaderUsed != shader) {
        const HRESULT hr = _RwD3DDevice->SetPixelShader((LPDIRECT3DPIXELSHADER9)shader);
        _rwD3D9LastPixelShaderUsed = (void*)-1;
        if (SUCCEEDED(hr))
            _rwD3D9LastPixelShaderUsed = shader;
    }
}

// 0x7FA1C0
static void SetIndices(void* indexBuffer) {
    if (_rwD3D9LastIndexBufferUsed != indexBuffer) {
        _rwD3D9LastIndexBufferUsed = indexBuffer;
        _RwD3DDevice->SetIndices((LPDIRECT3DINDEXBUFFER9)indexBuffer);
    }
}

// 0x7FA320
static void DrawIndexedPrimitive(RwUInt32 primitiveType, RwInt32 baseVertexIndex, RwUInt32 minIndex, RwUInt32 numVertices, RwUInt32 startIndex, RwUInt32 primitiveCount) {
    RenderStateFlushCache();
    _RwD3DDevice->DrawIndexedPrimitive((D3DPRIMITIVETYPE)primitiveType, baseVertexIndex, minIndex, numVertices, startIndex, primitiveCount);
}

// 0x7FA360
static void DrawPrimitive(RwUInt32 primitiveType, RwUInt32 startVertex, RwUInt32 primitiveCount) {
    RenderStateFlushCache();
    _RwD3DDevice->DrawPrimitive((D3DPRIMITIVETYPE)primitiveType, startVertex, primitiveCount);
}

// 0x7FA4F0
// Copies the cached transform matrix for `state` into `matrix`; if the state
// was never set, the identity matrix is returned.
static void GetTransform(RwUInt32 state, void* matrix) {
    if (matrix) {
        const void* src = *TransformSlot(state);
        if (!src)
            src = &IdentityMatrix;
        memcpy(matrix, src, sizeof(D3DMATRIX));
    }
}

// 0x7FA860
static RwBool EnableLight(RwInt32 index, RwBool enable) {
    if (index < MaxLights) {
        DriverLight& l = Lights[index];
        if (l.enabled != enable) {
            l.enabled = enable;
            return SUCCEEDED(_RwD3DDevice->LightEnable(index, enable));
        }
    }
    return TRUE;
}

// 0x7FAC60
static RwBool CreateVertexShader(const RwUInt32* function, void** shader) {
    const HRESULT hr = _RwD3DDevice->CreateVertexShader((const DWORD*)function, (LPDIRECT3DVERTEXSHADER9*)shader);
    if (SUCCEEDED(hr))
        _rwD3D9LastVertexShaderUsed = (void*)-1;
    return SUCCEEDED(hr);
}

// 0x7FAC90
static void DeleteVertexShader(void* shader) {
    ((LPDIRECT3DVERTEXSHADER9)shader)->Release();
}

// 0x7FACC0
static RwBool CreatePixelShader(const RwUInt32* function, void** shader) {
    return SUCCEEDED(_RwD3DDevice->CreatePixelShader((const DWORD*)function, (LPDIRECT3DPIXELSHADER9*)shader));
}

// 0x7FACF0
static void DeletePixelShader(void* shader) {
    ((LPDIRECT3DPIXELSHADER9)shader)->Release();
}

// 0x7FAD20
static const void* GetCaps() {
    return &DeviceCaps;
}

// 0x7FAE20
static void DeviceSetRestoreCallback(rwD3D9DeviceRestoreCallBack callback) {
    RestoreCallback = callback;
}

// 0x7FAE30
static rwD3D9DeviceRestoreCallBack DeviceGetRestoreCallback() {
    return RestoreCallback;
}

// 0x7FC200
// Pushes all deferred (cached) render / texture-stage states onto the device.
// Only states whose value actually changed are written.
static void RenderStateFlushCache() {
    if (RenderStateDirtyCount != 0) {
        for (RwUInt32 i = 0; i < RenderStateDirtyCount; i++) {
            const auto state = RenderStateDirtyList[i];
            const auto value = RenderStateValues[state][0];
            RenderStateValues[state][1] = 0;
            if (RenderStateCurrent[state] != value) {
                RenderStateCurrent[state] = value;
                _RwD3DDevice->SetRenderState((D3DRENDERSTATETYPE)state, value);
            }
        }
        RenderStateDirtyCount = 0;
    }
    if (TSSDirtyCount != 0) {
        for (RwUInt32 i = 0; i < TSSDirtyCount; i++) {
            const auto stage = TSSDirtyPairs[i][0];
            const auto state = TSSDirtyPairs[i][1];
            const auto idx   = state + stage * TSS_PER_STAGE;
            const auto value = TSSValues[idx][0];
            TSSValues[idx][1] = 0;
            if (TSSCurrent[idx] != value) {
                TSSCurrent[idx] = value;
                _RwD3DDevice->SetTextureStageState(stage, (D3DTEXTURESTAGESTATETYPE)state, value);
            }
        }
        TSSDirtyCount = 0;
    }
}

// 0x7FC2D0
// Defers a render state change: the value is cached and pushed to the device
// by RenderStateFlushCache() (usually right before a draw call).
static void SetRenderState(RwUInt32 state, RwUInt32 value) {
    if (RenderStateValues[state][0] != value) {
        RenderStateValues[state][0] = value;
        if (RenderStateValues[state][1] == 0) {
            RenderStateValues[state][1] = 1;
            RenderStateDirtyList[RenderStateDirtyCount++] = state;
        }
    }
}

// 0x7FC320
static void GetRenderState(RwUInt32 state, void* value) {
    *(RwUInt32*)value = RenderStateValues[state][0];
}

// 0x7FC340
static void SetTextureStageState(RwUInt32 stage, RwUInt32 type, RwUInt32 value) {
    const auto idx = type + stage * TSS_PER_STAGE;
    if (TSSValues[idx][0] != value) {
        TSSValues[idx][0] = value;
        if (TSSValues[idx][1] == 0) {
            TSSValues[idx][1] = 1;
            TSSDirtyPairs[TSSDirtyCount][0] = stage;
            TSSDirtyPairs[TSSDirtyCount][1] = type;
            TSSDirtyCount++;
        }
    }
}

// 0x7FC3C0
static void SetSamplerState(RwUInt32 stage, RwUInt32 type, RwUInt32 value) {
    const auto idx = type + stage * SAMPLER_STATES;
    if (SamplerValues[idx] != value) {
        SamplerValues[idx] = value;
        _RwD3DDevice->SetSamplerState(stage, (D3DSAMPLERSTATETYPE)type, value);
    }
}
}; // namespace RwD3D9Driver

void RwD3D9DriverInjectHooks() {
    RH_ScopedCategory("Renderware");
    RH_ScopedNamespaceName("RwD3D9Driver");

    RH_ScopedNamedGlobalInstall(RwD3D9Driver::EngineSetRefreshRate,        "RwD3D9EngineSetRefreshRate",         0x7F8580);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::DeviceSupportsDXTTexture,    "RwD3D9DeviceSupportsDXTTexture",     0x7F9C30);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::SetStencilClear,             "RwD3D9SetStencilClear",              0x7F9D30);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::GetStencilClear,             "RwD3D9GetStencilClear",              0x7F9D40);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::SetVertexDeclaration,        "_rwD3D9SetVertexDeclaration",        0x7F9F70);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::SetVertexShader,             "_rwD3D9SetVertexShader",             0x7F9FB0);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::SetPixelShader,              "_rwD3D9SetPixelShader",              0x7F9FF0);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::SetIndices,                  "_rwD3D9SetIndices",                  0x7FA1C0);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::DrawIndexedPrimitive,        "_rwD3D9DrawIndexedPrimitive",        0x7FA320);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::DrawPrimitive,               "_rwD3D9DrawPrimitive",               0x7FA360);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::GetTransform,                "RwD3D9GetTransform",                 0x7FA4F0);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::EnableLight,                 "RwD3D9EnableLight",                  0x7FA860);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::CreateVertexShader,          "RwD3D9CreateVertexShader",           0x7FAC60);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::DeleteVertexShader,          "RwD3D9DeleteVertexShader",           0x7FAC90);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::CreatePixelShader,           "RwD3D9CreatePixelShader",            0x7FACC0);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::DeletePixelShader,           "RwD3D9DeletePixelShader",            0x7FACF0);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::GetCaps,                     "RwD3D9GetCaps",                      0x7FAD20);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::DeviceSetRestoreCallback,    "_rwD3D9DeviceSetRestoreCallback",    0x7FAE20);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::DeviceGetRestoreCallback,    "_rwD3D9DeviceGetRestoreCallback",    0x7FAE30);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::RenderStateFlushCache,       "_rwD3D9RenderStateFlushCache",       0x7FC200);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::SetRenderState,              "RwD3D9SetRenderState",               0x7FC2D0);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::GetRenderState,              "RwD3D9GetRenderState",               0x7FC320);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::SetTextureStageState,        "RwD3D9SetTextureStageState",         0x7FC340);
    RH_ScopedNamedGlobalInstall(RwD3D9Driver::SetSamplerState,             "RwD3D9SetSamplerState",              0x7FC3C0);
}
