#include "StdInc.h"

#include "CustomBuildingRenderer.h"

#include "CustomBuildingDNPipeline.h"
#include "CustomBuildingPipeline.h"

// `CCustomBuildingPipeline::PLUGIN_ID` is file-private to CustomBuildingPipeline.cpp, so re-declare it here.
// Must match the one there: rpPDS_MAKEPIPEID(rwVENDORID_DEVELOPER, 0x9C) == 0x53F2009C
constexpr auto CBP_PLUGIN_ID = rpPDS_MAKEPIPEID(rwVENDORID_DEVELOPER, 0x9C);

// Frame throttling counters for CCustomBuildingDNPipeline::PreRenderUpdate - updated by CCustomBuildingRenderer::Update
// (aliased as `s_Magic1`/`s_Magic2` in CustomBuildingDNPipeline.cpp - same memory, different names)
static auto& s_DNFrameCounter = StaticRef<uint32>(0xC02C14);
static auto& s_DNForceUpdate  = StaticRef<uint32>(0xC02C18);

void CCustomBuildingRenderer::InjectHooks() {
    RH_ScopedClass(CCustomBuildingRenderer);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Initialise, 0x5D7EC0);
    RH_ScopedInstall(Shutdown, 0x5D7EE0);
    RH_ScopedInstall(PluginAttach, 0x5D7EF0);
    RH_ScopedInstall(AtomicSetup, 0x5D7F00);
    RH_ScopedInstall(IsCBPCPipelineAttached, 0x5D7F40);
    RH_ScopedInstall(UpdateDayNightBalanceParam, 0x5D7F80);
    RH_ScopedInstall(Update, 0x5D8050);
}

// 0x5D7EC0
bool CCustomBuildingRenderer::Initialise() {
    auto pipe = CCustomBuildingPipeline::CreatePipe();
    if (pipe)
        return CCustomBuildingDNPipeline::CreatePipe();

    return pipe;
}

// 0x5D7EE0
void CCustomBuildingRenderer::Shutdown() {
    CCustomBuildingPipeline::DestroyPipe();
    CCustomBuildingDNPipeline::DestroyPipe();
}

// 0x5D7EF0
bool CCustomBuildingRenderer::PluginAttach() {
    return CCustomBuildingDNPipeline::ExtraVertColourPluginAttach();
}

// 0x5D7F00
void CCustomBuildingRenderer::AtomicSetup(RpAtomic* atomic) {
    // Geometries with (day/night vertex colors && pre-lighting) get the DN pipeline,
    // everything else the generic custom building pipeline.
    if (CCustomBuildingDNPipeline::GetExtraVertColourPtr(atomic->geometry) && atomic->geometry->preLitLum) {
        CCustomBuildingDNPipeline::CustomPipeAtomicSetup(atomic);
    } else {
        CCustomBuildingPipeline::CustomPipeAtomicSetup(atomic);
    }
}

// 0x5D7F40
bool CCustomBuildingRenderer::IsCBPCPipelineAttached(RpAtomic* atomic) {
    const auto pipeId = GetPipelineID(atomic);
    if (pipeId == CBP_PLUGIN_ID || pipeId == CUSTOM_BUILDING_DN_PIPELINE_ID) {
        return true;
    }

    // Atomics with day/night colors + pre-lighting will be switched to one of the above pipelines
    // by AtomicSetup(), so count them as attached too.
    return CCustomBuildingDNPipeline::GetExtraVertColourPtr(atomic->geometry)
        && atomic->geometry->preLitLum;
}

// 0x5D7F80
void CCustomBuildingRenderer::UpdateDayNightBalanceParam() {
    const auto minutes = CClock::GetMinutesToday();

    if (minutes < 360.f /* 06:00 */) { // night
        CCustomBuildingDNPipeline::m_fDNBalanceParam = 1.f;
        return;
    }

    if (minutes < 420.f /* 07:00 */) { // sunrise (1.f -> 0.f)
        CCustomBuildingDNPipeline::m_fDNBalanceParam = (420.f - minutes) * (1.0f / 60.0f);
        return;
    }

    if (minutes < 1200.f /* 20:00 */) { // day
        CCustomBuildingDNPipeline::m_fDNBalanceParam = 0.f;
        return;
    }

    if (minutes < 1260.f /* 21:00 */) { // sunset (0.f -> 1.f)
        CCustomBuildingDNPipeline::m_fDNBalanceParam = 1.f - (1260.f - minutes) * (1.0f / 60.0f);
        return;
    }

    CCustomBuildingDNPipeline::m_fDNBalanceParam = 1.f;
}

// 0x5D8050
void CCustomBuildingRenderer::Update() {
    ZoneScoped;

    UpdateDayNightBalanceParam();

    // 0x5D6830 - advance the frame throttling counters
    s_DNFrameCounter = (s_DNFrameCounter + 1) & 0xF;
    s_DNForceUpdate  = 0;
}
