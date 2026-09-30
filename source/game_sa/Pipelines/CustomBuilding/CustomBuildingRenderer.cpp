#include "StdInc.h"

#include "CustomBuildingRenderer.h"

#include "CustomBuildingDNPipeline.h"
#include "CustomBuildingPipeline.h"

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
    auto* geometry = atomic->geometry;

    // If the geometry has the extra-vertex-colour (day/night) plugin data attached,
    // use the DN pipe, otherwise use the plain custom building pipe
    const auto* extVertColour = CCustomBuildingDNPipeline::GetExtraVertColourPtr(geometry);
    if (extVertColour && extVertColour->NightColors) {
        CCustomBuildingDNPipeline::CustomPipeAtomicSetup(atomic);
        return;
    }
    CCustomBuildingPipeline::CustomPipeAtomicSetup(atomic);
}

// 0x5D7F40
bool CCustomBuildingRenderer::IsCBPCPipelineAttached(RpAtomic* atomic) {
    const auto pipeline = atomic->pipeline;
    if (pipeline != CCustomBuildingDNPipeline::ObjPipeline && pipeline != CCustomBuildingPipeline::ObjPipeline) {
        // Not one of the two custom building pipelines - check the DN plugin data
        const auto* extVertColour = CCustomBuildingDNPipeline::GetExtraVertColourPtr(atomic->geometry);
        if (!extVertColour || !extVertColour->NightColors) {
            return false;
        }
    }
    return true;
}

// 0x5D7F80
void CCustomBuildingRenderer::UpdateDayNightBalanceParam() {
    const auto minutes = CClock::GetMinutesToday();

    if (minutes < 360.0f) { // before 6 AM
        CCustomBuildingDNPipeline::m_fDNBalanceParam = 1.0f;
    } else if (minutes < 420.0f) { // 6 AM - 7 AM : fade out
        CCustomBuildingDNPipeline::m_fDNBalanceParam = (420.0f - minutes) / 60.0f;
    } else if (minutes < 1200.0f) { // 7 AM - 8 PM
        CCustomBuildingDNPipeline::m_fDNBalanceParam = 0.0f;
    } else if (minutes < 1260.0f) { // 8 PM - 9 PM : fade in
        CCustomBuildingDNPipeline::m_fDNBalanceParam = 1.0f - (1260.0f - minutes) / 60.0f;
    } else {
        CCustomBuildingDNPipeline::m_fDNBalanceParam = 1.0f;
    }
}

// Unreversed helper (per-frame pipeline bookkeeping, camera/dir callback)
static void sub_5D6830(int32 a1) {
    plugin::Call<0x5D6830, int32>(a1);
}

// 0x5D8050
void CCustomBuildingRenderer::Update() {
    ZoneScoped;

    UpdateDayNightBalanceParam();
    sub_5D6830(0);
}
