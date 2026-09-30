#include "StdInc.h"

#include "CarFXRenderer.h"
#include "CustomCarEnvMapPipeline.h"
#include "ClothesBuilder.h"

void CCarFXRenderer::InjectHooks() {
    RH_ScopedClass(CCarFXRenderer);
    RH_ScopedCategory("Fx");

    RH_ScopedInstall(RegisterPlugins, 0x5D5B00);
    RH_ScopedInstall(Initialise, 0x5D5AC0);
    RH_ScopedInstall(InitialiseDirtTexture, 0x5D5BC0);
    RH_ScopedInstall(Shutdown, 0x5D5AD0);
    RH_ScopedInstall(PreRenderUpdate, 0x5D5B10);
    RH_ScopedInstall(IsCCPCPipelineAttached, 0x5D5B80);
    RH_ScopedInstall(CustomCarPipeAtomicSetup, 0x5D5B20);
    RH_ScopedInstall(CustomCarPipeClumpSetup, 0x5D5B40);
    RH_ScopedInstall(SetCustomFXAtomicRenderPipelinesVMICB, 0x5D5B60);
    RH_ScopedInstall(SetFxEnvMapLightMult, 0x5D5BA0);
}

// 0x5D5B00
bool CCarFXRenderer::RegisterPlugins() {
    return CCustomCarEnvMapPipeline::RegisterPlugin() != 0;
}

// 0x5D5AC0
bool CCarFXRenderer::Initialise() {
    return CCustomCarEnvMapPipeline::CreatePipe() != 0;
}

// 0x5D5BC0
void CCarFXRenderer::InitialiseDirtTexture() {
    const auto txdSlot = CTxdStore::FindTxdSlot("vehicle"); // 0x731850
    CTxdStore::PushCurrentTxd();    // 0x7316A0
    CTxdStore::SetCurrentTxd(txdSlot); // 0x7319C0

    RwTexture* const texture = RwTextureRead("vehiclegrunge256", nullptr);
    RwTextureSetFilterMode(texture, rwFILTERLINEAR); // *(uint8*)(texture + 0x50) = 2

    const auto width  = RwTextureGetRaster(texture)->width;  // raster + 0xC
    const auto height = RwTextureGetRaster(texture)->height; // raster + 0x10

    // Generate the 16 dirt levels: level N is the base texture with every pixel
    // transformed into `px * N / 16 + shade / 16` (where shade runs from 0xFF0 down to 0)
    auto shade = 0xFF0;
    for (auto level = 0; level < NUM_DIRT_TEXTURES; level++, shade -= 0xFF) {
        RwTexture* const dirtTex = CClothesBuilder::CopyTexture(texture); // 0x5A5730
        ms_aDirtTextures[level] = dirtTex;
        RwTextureSetName(dirtTex, "vehiclegrunge256"); // 0x7F38A0

        // lockFlags 3 = rwRASTERLOCKWRITE | rwRASTERLOCKREAD
        auto* const pixels = RwRasterLock(RwTextureGetRaster(dirtTex), 0, 3);
        if (pixels && height > 0) {
            for (auto y = 0; y < height; y++) {
                auto* px = reinterpret_cast<uint8*>(pixels) + y * width * 4;
                for (auto x = 0; x < width; x++, px += 4) {
                    const auto shadeDiv16 = (shade + (shade >> 31 & 0xF)) >> 4; // signed fixed point rounding of shade / 16
                    for (auto channel = 0; channel < 3; channel++) { // RGB, alpha (px[3]) left as is
                        const auto value = static_cast<uint32>(px[channel]) * level;
                        px[channel] = static_cast<uint8>((value + ((value >> 31) & 0xF)) >> 4) + shadeDiv16;
                    }
                }
            }
        }
        RwRasterUnlock(RwTextureGetRaster(dirtTex)); // 0x7FAEC0
    }

    CTxdStore::PopCurrentTxd(); // 0x7316B0
}

// 0x5D5AD0
void CCarFXRenderer::Shutdown() {
    for (auto& texture : ms_aDirtTextures) {
        RwTextureDestroy(texture);
    }
    CCustomCarEnvMapPipeline::DestroyPipe();
}

// 0x5D5B10
void CCarFXRenderer::PreRenderUpdate() {
    CCustomCarEnvMapPipeline::PreRenderUpdate();
}

// 0x5D5B80
bool CCarFXRenderer::IsCCPCPipelineAttached(RpAtomic* atomic) {
    return GetPipelineID(atomic) == 0x53F2009A;
}

// 0x5D5B20
void CCarFXRenderer::CustomCarPipeAtomicSetup(RpAtomic* atomic) {
    CCustomCarEnvMapPipeline::CustomPipeAtomicSetup(atomic);
}

// 0x5D5B40
void CCarFXRenderer::CustomCarPipeClumpSetup(RpClump* clump) {
    RpClumpForAllAtomics(clump, SetCustomFXAtomicRenderPipelinesVMICB, nullptr);
}

// 0x5D5B60
RpAtomic* CCarFXRenderer::SetCustomFXAtomicRenderPipelinesVMICB(RpAtomic* atomic, void* data) {
    CCustomCarEnvMapPipeline::CustomPipeAtomicSetup(atomic);
    return atomic;
}

// 0x5D5BA0
void CCarFXRenderer::SetFxEnvMapLightMult(float multiplier) {
    gSpecIntensity = multiplier;
}
