#include "StdInc.h"

#include "CarFXRenderer.h"
#include "CustomCarEnvMapPipeline.h"

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
// 16 уровней грязи: уровень N — копия текстуры "vehiclegrunge256", каждый RGB-канал:
//   px' = px * N / 16 + shade / 16   (shade идёт от 0xFF0 вниз с шагом 0xFF)
// (в оригинале функция возвращает bool/1, сигнатура хедера void — оставлена void)
void CCarFXRenderer::InitialiseDirtTexture() {
    const auto txdSlot = CTxdStore::FindTxdSlot("vehicle");  // 0x731850
    CTxdStore::PushCurrentTxd();                             // 0x7316A0
    CTxdStore::SetCurrentTxd(txdSlot);                       // 0x7319C0

    RwTexture* const texture = RwTextureRead("vehiclegrunge256", nullptr); // 0x7F3AC0
    RwTextureSetFilterMode(texture, rwFILTERLINEAR);         // texture->filterMode (+0x50) = 2

    const auto width  = RwTextureGetRaster(texture)->width;  // raster + 0xC
    const auto height = RwTextureGetRaster(texture)->height; // raster + 0x10

    auto shade = 0xFF0;
    for (auto level = 0; level < NUM_DIRT_TEXTURES; level++, shade -= 0xFF) {
        RwTexture* const dirtTex = CClothesBuilder::CopyTexture(texture); // 0x5A5730
        ms_aDirtTextures[level] = dirtTex;                                // 0xC02BD0
        RwTextureSetName(dirtTex, "vehiclegrunge256");                    // 0x7F38A0

        // lockFlags 3 = rwRASTERLOCKWRITE | rwRASTERLOCKREAD (null-check в оригинале отсутствует)
        auto* const pixels = RwRasterLock(RwTextureGetRaster(dirtTex), 0, 3); // 0x7FB2D0
        for (auto y = 0; y < height; y++) {
            auto* px = reinterpret_cast<uint8*>(pixels) + y * width * 4;
            for (auto x = 0; x < width; x++, px += 4) {
                const auto shadeDiv16 = (shade + (shade >> 31 & 0xF)) >> 4; // signed fixed point rounding shade / 16
                for (auto channel = 0; channel < 3; channel++) { // RGB; alpha (px[3]) не трогаем
                    const auto value = static_cast<uint32>(px[channel]) * level;
                    px[channel] = static_cast<uint8>((value + ((value >> 31) & 0xF)) >> 4) + shadeDiv16;
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
