#include "StdInc.h"

#include "RealTimeShadowManager.h"
#include "Shadows.h"
#include "World.h" // CWorld::Players, CWorld::PlayerInFocus

auto& g_realTimeShadowMan = StaticRef<CRealTimeShadowManager>(0xC40350);

void CRealTimeShadowManager::InjectHooks() {
    RH_ScopedClass(CRealTimeShadowManager);
    RH_ScopedCategory("Shadows");

    RH_ScopedInstall(Init, 0x7067C0);
    RH_ScopedInstall(ReInit, 0x706870);
    RH_ScopedInstall(ReturnRealTimeShadow, 0x705B30);
    RH_ScopedInstall(GetRealTimeShadow, 0x706970);
    RH_ScopedInstall(Update, 0x706AB0);
    RH_ScopedInstall(DoShadowThisFrame, 0x706BA0);
    RH_ScopedInstall(Exit, 0x706A60);
}

// 0x7067C0
void CRealTimeShadowManager::Init() {
    if (m_bInitialised) {
        return;
    }

    for (auto& shdw : m_apShadows) {
        shdw = new CRealTimeShadow();
        shdw->Create(true, 4, true);
    }

    m_BlurCamera.Create(6);

    m_GradientCamera.Create(6);
    m_GradientCamera.MakeGradientRaster();

    m_bInitialised = true;
}

// 0x706A60
void CRealTimeShadowManager::Exit() { // AKA `Shutdown`
    if (!m_bInitialised) {
        return;
    }

    for (auto& shdw : m_apShadows) {
        delete std::exchange(shdw, nullptr);
    }

    // Nice hack
    m_BlurCamera.Destroy();
    m_GradientCamera.Destroy();

    m_bInitialised = false;
}

// 0x705B30
void CRealTimeShadowManager::ReturnRealTimeShadow(CRealTimeShadow* shdw) {
    if (m_bInitialised) {
        shdw->m_pOwner->m_pShadowData = nullptr;
        shdw->m_pOwner = nullptr;
    }
}

// 0x706870
// Пересоздание raster'ов всех теневых камер: для каждой из 16 теней (камера + blur-камера) и
// для m_BlurCamera/m_GradientCamera менеджера уничтожить raster камеры, создать новый квадратный
// (w x w, depth=0, rwRASTERTYPECAMERATEXTURE) и привязать к render-текстуре; в конце —
// MakeGradientRaster() у m_GradientCamera. В оригинале тело пересоздания инлайнено 4 раза —
// здесь вынесено в лямбду (прецедент: EmitVertex в CPostEffects::UnderWaterRipple).
void CRealTimeShadowManager::ReInit() {
    const auto ReCreateShadowRaster = [](CShadowCamera& shadowCam) {
        auto* const oldRaster = shadowCam.m_pRwCamera->frameBuffer; // RwCamera+0x60
        const auto size       = oldRaster->width;                   // RwRaster+0xC

        shadowCam.m_pRwCamera->frameBuffer = nullptr;
        RwRasterDestroy(oldRaster);                                 // 0x7FB020
        shadowCam.m_pRwCamera->frameBuffer = RwRasterCreate(size, size, 0, rwRASTERTYPECAMERATEXTURE); // 0x7FB230
        RwTextureSetRaster(shadowCam.m_pRwRenderTexture, shadowCam.m_pRwCamera->frameBuffer);          // 0x7F35D0
    };

    for (const auto shdw : m_apShadows) {
        ReCreateShadowRaster(shdw->m_camera);     // shdw+0x08 / текстура shdw+0x0C
        ReCreateShadowRaster(shdw->m_blurCamera); // shdw+0x14 / текстура shdw+0x18
    }

    ReCreateShadowRaster(m_BlurCamera);     // this+0x44 / this+0x48
    ReCreateShadowRaster(m_GradientCamera); // this+0x4C / this+0x50

    m_GradientCamera.MakeGradientRaster(); // 0x705D20
}

// 0x706AB0
void CRealTimeShadowManager::Update() {
    ZoneScoped;

    if (m_bInitialised && m_bNeedsReinit) {
        ReInit();
        m_bNeedsReinit = false;
    }

    for (const auto shdw : m_apShadows) {
        if (!shdw->m_pOwner) {
            continue;
        }

        assert(shdw->m_pOwner->m_pShadowData == shdw);

        // 0x305eed - 0x305f0f: Update intensity
        constexpr auto INTENSITY_STEP = 3u;
        if (shdw->m_bKeepAlive) {
            shdw->m_nIntensity = std::min<uint8>(100u, shdw->m_nIntensity + INTENSITY_STEP);
        } else { // Fade out
            shdw->m_nIntensity = std::max<uint8>(shdw->m_nIntensity, INTENSITY_STEP) - INTENSITY_STEP; // Avoids underflow
        }

        if (shdw->m_nIntensity) {
            shdw->Update();
            CShadows::StoreRealTimeShadow(
                shdw->m_pOwner,

                CTimeCycle::m_fShadowDisplacementX[CTimeCycle::m_CurrentStoredValue],
                CTimeCycle::m_fShadowDisplacementY[CTimeCycle::m_CurrentStoredValue],

                CTimeCycle::m_fShadowFrontX[CTimeCycle::m_CurrentStoredValue],
                CTimeCycle::m_fShadowFrontY[CTimeCycle::m_CurrentStoredValue],

                CTimeCycle::m_fShadowSideX[CTimeCycle::m_CurrentStoredValue],
                CTimeCycle::m_fShadowSideY[CTimeCycle::m_CurrentStoredValue]
            );
        } else if (m_bInitialised) {
            shdw->m_pOwner->m_pShadowData = nullptr;
            shdw->m_pOwner = nullptr;
        }
    }

    // TODO: ??? - Perhaps debug code left accidentally in?
    for (const auto shdw : m_apShadows) {
        shdw->m_bKeepAlive = false;
    }
}

// 0x706970
// ABI: оригинал возвращает CRealTimeShadow* (может вернуть nullptr!), а не ссылку —
// сигнатура в RealTimeShadowManager.h поправлена. Слот 0 всегда за игроком 1 (PED_TYPE_PLAYER1),
// остальные 1..15 — берётся ПОСЛЕДНИЙ свободный (как в оригинале, без break).
// Если игрок в фокусе едет в машине быстрее 0.3 (кв. скорость 0.09 = [0x85B324]) — тень не выдаём.
CRealTimeShadow* CRealTimeShadowManager::GetRealTimeShadow(CPhysical* physical) {
    CRealTimeShadow* shadow   = nullptr;
    bool             doSearch  = true;
    bool             isPlayer1Ped = false;

    if (physical->GetIsTypePed()) {
        isPlayer1Ped = physical->AsPed()->m_nPedType == PED_TYPE_PLAYER1;
    }

    if (!isPlayer1Ped) {
        // null-чека playerPed в оригинале нет
        const auto* const playerPed = CWorld::Players[CWorld::PlayerInFocus].m_pPed;
        if (playerPed->bInVehicle && playerPed->m_pVehicle) {
            if (playerPed->m_pVehicle->m_vecMoveSpeed.SquaredMagnitude() > 0.09f) {
                doSearch = false;
            }
        }
    }

    if (m_bInitialised && doSearch) {
        if (isPlayer1Ped) {
            shadow = m_apShadows[0];
        } else {
            for (auto i = 1; i < NUM_REALTIME_SHADOWS; i++) {
                const auto shdw = m_apShadows[i];
                if (!shdw->m_pOwner) {
                    shadow = shdw;
                }
            }
        }

        if (shadow) {
            plugin::CallMethod<0x706520, CRealTimeShadow*, CPhysical*>(shadow, physical); // CRealTimeShadow::SetupForThisEntity (не реверснута)
            physical->m_pShadowData = shadow;
            shadow->m_bKeepAlive    = true;
            shadow->m_nIntensity    = 0;
        }
    }
    return shadow;
}

// 0x706BA0
void CRealTimeShadowManager::DoShadowThisFrame(CPhysical* physical) {
    switch (g_fx.GetFxQuality()) {
    case FX_QUALITY_VERY_HIGH: // Always render
        break;
    case FX_QUALITY_HIGH: { // Only draw for main player
        if (physical->GetIsTypePed()) {
            if (physical->AsPed()->m_nPedType == PED_TYPE_PLAYER1) {
                break;
            }
        }
        return;
    }
    default: // For any other quality: skip
        return;
    }

    if (const auto shdw = physical->m_pShadowData) {
        shdw->m_bKeepAlive = true;
    } else {
        (void)GetRealTimeShadow(physical); // ???
    }
}
