/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "WeaponEffects.h"

void CWeaponEffects::InjectHooks() {
    RH_ScopedClass(CWeaponEffects);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x742AB0);
    RH_ScopedInstall(Shutdown, 0x742B80);
    RH_ScopedInstall(IsLockedOn, 0x742BD0);
    RH_ScopedInstall(MarkTarget, 0x742BF0);
    RH_ScopedInstall(ClearCrossHair, 0x742C60);
    RH_ScopedInstall(ClearCrossHairs, 0x742C80);
    RH_ScopedInstall(ClearCrossHairImmediately, 0x742CA0);
    RH_ScopedInstall(ClearCrossHairsImmediately, 0x742CC0);
    RH_ScopedInstall(Render, 0x742CF0);
}

// 0x742AB0
void CWeaponEffects::Init() {
    for (auto& crossHair : gCrossHair) {
        crossHair.m_vecPosn = CVector();
        crossHair.m_bActive = false;
        crossHair.m_color = CRGBA(255, 0, 0, 127);
        crossHair.m_fSize = 1.0f;
        crossHair.field_1C = 0;
        crossHair.m_nTimeWhenToDeactivate = 0;
        crossHair.m_bClearImmediately = false;
        crossHair.m_fRotation = 0.0f;
    }

    CTxdStore::PushCurrentTxd();
    CTxdStore::SetCurrentTxd(CTxdStore::FindTxdSlot("particle"));
    gpCrossHairTex          = RwTextureRead("target256",  "target256m");
    gpCrossHairTexFlight[0] = RwTextureRead("lockon",     "lockonA");
    gpCrossHairTexFlight[1] = RwTextureRead("lockonFire", "lockonFireA");
    CTxdStore::PopCurrentTxd();
}

// 0x742B80
void CWeaponEffects::Shutdown() {
    for (auto& crossHair : gpCrossHairTexFlight) {
        RwTextureDestroy(crossHair);
        crossHair = nullptr;
    }

    RwTextureDestroy(gpCrossHairTex);
    gpCrossHairTex = nullptr;
}

// 0x742BD0
bool CWeaponEffects::IsLockedOn(CrossHairId id) {
    return gCrossHair[id].m_fRotation;
}

// 0x742BF0
void CWeaponEffects::MarkTarget(CrossHairId id, CVector posn, uint8 red, uint8 green, uint8 blue, uint8 alpha, float size, bool bClearImmediately) {
    auto& crossHair = gCrossHair[id];
    crossHair.m_vecPosn               = posn;
    crossHair.m_color.r               = red;
    crossHair.m_color.g               = green;
    crossHair.m_color.b               = blue;
    crossHair.m_color.a               = alpha;
    crossHair.m_bActive               = true;
    crossHair.m_fSize                 = size;
    crossHair.m_nTimeWhenToDeactivate = -1;
    crossHair.m_bClearImmediately     = bClearImmediately;
}

// 0x742C60
void CWeaponEffects::ClearCrossHair(CrossHairId id) {
    gCrossHair[id].m_nTimeWhenToDeactivate = static_cast<int32>(CTimer::GetTimeInMS() + 400);
}

// 0x742C80
void CWeaponEffects::ClearCrossHairs() {
    for (auto& crossHair : gCrossHair) {
        crossHair.m_bActive = false;
    }
}

// 0x742CA0
void CWeaponEffects::ClearCrossHairImmediately(CrossHairId id) {
    gCrossHair[id].m_nTimeWhenToDeactivate = static_cast<int32>(CTimer::GetTimeInMS() - 100);
    gCrossHair[id].m_bActive = false;
}

// 0x742CC0
void CWeaponEffects::ClearCrossHairsImmediately() {
    for (auto i = 0u; i < gCrossHair.size(); i++) {
        ClearCrossHair(i);
    }
}

// Float constants decoded from the original binary:
//   0x858BA4=20.0 0x858624=1.0 0x859AD8=28.0 0x859BB4=1.8 0x865034=0.006135923(2pi/1024)
//   0x858C98=0.8 0x858C80=5.0 0x858FE8=25.0 0x858FA0=2.5 0x8D6148=30.0 0x858BB0=120.0
//   0x8595EC=pi/180 0x858B48=15.0 0x859040=1/3 0x858F34=0.75 0x858C28=0.05 0x858CA0=2.0
//   0x858C20=0.9 0x858CBC=2pi 0x8D6140=15.0 0x858B8C=0.5 0x858B1C=0.1 0x858B3C=3.0 0x863A44=1/512
// Globals not named in the repo: 0xC8A890, 0xC8A894, 0xC8A898, 0xC8A89C, 0xC8A8A0, 0x8D6144.
// TheCamera fields used here (offsets verified against the repo layout):
//   m_bWideScreenOn @+0x3D (0xB6F065), m_nActiveCam @+0x59 (0xB6F081),
//   m_aCams @+0x174 (0xB6F19C), m_mCameraMatrix @+0x974 (0xB6F99C).
// 0x56EF90 is not reversed - called via plugin::Call (original loops twice over 0xB7CEF0 with 0x190 stride).
// 0x742CF0
void CWeaponEffects::Render() {
    ZoneScoped;

    // Original globals not declared in the repo
    static auto& s_ChTargetChangedMs = StaticRef<uint32>(0xC8A890);
    static auto& s_ChLastTarget      = StaticRef<CEntity*>(0xC8A894);
    static auto& s_ChWobbleCos       = StaticRef<float>(0xC8A898); // cos offset of the crosshair pulse
    static auto& s_ChWobbleSin       = StaticRef<float>(0xC8A89C); // sin offset of the crosshair pulse
    static auto& s_ChTriangleRadius  = StaticRef<float[2]>(0xC8A8A0); // triangle (2-player) crosshair radius
    static auto& s_ChTriangleGrowDir = StaticRef<uint8[2]>(0x8D6144); // radius pulsation phase

    const auto scrW = (float)RsGlobal.maximumWidth;   // 0xC17044
    const auto scrH = (float)RsGlobal.maximumHeight;  // 0xC17048

    if (!TheCamera.m_bWideScreenOn) {
        for (auto playerId = 0u; playerId < MAX_NUM_WEAPON_CROSSHAIRS; playerId++) {
            auto&        ch         = gCrossHair[playerId];
            float&       triRadius  = s_ChTriangleRadius[playerId];
            uint8&       triGrowDir = s_ChTriangleGrowDir[playerId];
            auto&        rotAccum   = ch.field_1C;
            auto&        pulseAmp   = ch.field_20;
            CPlayerPed*& playerPed  = CWorld::Players[playerId].m_pPed;

            if (ch.m_nTimeWhenToDeactivate != 0 && (uint32)ch.m_nTimeWhenToDeactivate < CTimer::GetTimeInMS()) {
                ch.m_bActive               = false;
                ch.m_nTimeWhenToDeactivate = 0;
                triRadius                  = 0.0f;
            }
            if (ch.m_nTimeWhenToDeactivate != -1) {
                triRadius = 0.0f;
            }
            if (!playerPed) {
                ch.m_bActive = false;
            }
            if (ch.m_bActive) {
                RwV3d scr{};
                float scrW2{}, scrH2{};
                const CVector posn = ch.m_vecPosn;

                if (ch.m_bClearImmediately) {
                    // Lock-on crosshair (textures lockon / lockonFire)
                    if (CSprite::CalcScreenCoors(posn, &scr, &scrW2, &scrH2, true, true)) {
                        if (const auto scale = 20.0f / scrH2; scale > 1.0f) {
                            scrW2 *= scale;
                            scrH2 *= scale;
                        }
                        RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
                        RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(FALSE));
                        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
                        RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
                        RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
                        RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(gpCrossHairTexFlight[0]->raster));

                        {
                            const float scales[2] = { 0.95f, 1.05f };
                            for (auto i = 0; i < 2; i++) {
                                CSprite::RenderOneXLUSprite_Rotate_Aspect(
                                    { scr.x, scr.y, scr.z },
                                    { std::min(scrW2, 28.0f) * scales[i] * 1.8f,
                                      std::min(scrH2, 20.0f) * scales[i] * 1.8f },
                                    0, 0, 0,
                                    (int16)0xFF,
                                    0.01f, 0.0f, (uint8)0xFF
                                );
                            }
                        }
                        CSprite::RenderOneXLUSprite_Rotate_Aspect(
                            { scr.x, scr.y, scr.z },
                            { std::min(scrW2, 28.0f) * 1.8f, std::min(scrH2, 20.0f) * 1.8f },
                            ch.m_color.r, ch.m_color.g, ch.m_color.b,
                            (int16)0xFF,
                            0.01f, 0.0f, ch.m_color.a
                        );

                        // Pulse: angle from (time & 1023), decaying amplitude field_20
                        const auto angle = (float)(CTimer::GetTimeInMS() & 0x3FF) * 0.006135923f; // 2pi/1024
                        pulseAmp -= 20.0f * angle;
                        if (pulseAmp < 0.0f) {
                            pulseAmp = 0.0f;
                        }
                        ch.m_fRotation = (pulseAmp == 0.0f) ? 1.0f : 0.0f; // IsLockedOn()
                        s_ChWobbleSin  = sin(angle) * pulseAmp;
                        s_ChWobbleCos  = cos(angle) * pulseAmp;

                        {
                            const float scales[2] = { 0.95f, 1.05f };
                            for (auto i = 0; i < 2; i++) {
                                CSprite::RenderOneXLUSprite_Rotate_Aspect(
                                    { scr.x - s_ChWobbleSin, scr.y - s_ChWobbleCos, scr.z },
                                    { std::min(scrW2, 28.0f) * scales[i] * 0.8f,
                                      std::min(scrH2, 20.0f) * scales[i] * 0.8f },
                                    0, 0, 0,
                                    (int16)0xFF,
                                    0.01f, angle, (uint8)0xFF
                                );
                            }
                        }
                        CSprite::RenderOneXLUSprite_Rotate_Aspect(
                            { scr.x - s_ChWobbleSin, scr.y - s_ChWobbleCos, scr.z },
                            { std::min(scrW2, 28.0f) * 0.8f, std::min(scrH2, 20.0f) * 0.8f },
                            ch.m_color.r, ch.m_color.g, ch.m_color.b,
                            (int16)ch.m_color.a, // original: intensity = color alpha
                            0.01f, angle, (uint8)0xFF
                        );
                    }
                    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(TRUE));
                    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,  RWRSTATE(TRUE));
                } else {
                    // Triangle crosshair (2 players)
                    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
                    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(FALSE));
                    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
                    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(nullptr));

                    CSprite::CalcScreenCoors(posn, &scr, &scrW2, &scrH2, false, false);
                    if (scr.x > scrW || scr.x < 0.0f || scr.y > scrH || scr.y < 0.0f) {
                        // Crosshair is off-screen - draw a direction pointer arrow
                        if (CGameLogic::IsCoopGameGoingOn()) {
                            const auto toTarget = posn - TheCamera.m_mCameraMatrix.GetPosition();
                            const auto camAngle = atan2(TheCamera.m_mCameraMatrix.GetUp().Dot(toTarget), TheCamera.m_mCameraMatrix.GetRight().Dot(toTarget));
                            const auto toPlayer = posn - playerPed->GetPosition();
                            const auto pedAngle = atan2(TheCamera.m_mCameraMatrix.GetUp().Dot(toPlayer), TheCamera.m_mCameraMatrix.GetRight().Dot(toPlayer));

                            float x1 = (1.0f - cos(camAngle)) * scrW * 0.5f;
                            float y1 = (1.0f - sin(camAngle)) * scrH * 0.5f;
                            const float sp  = sin(pedAngle) * 0.5f + cos(pedAngle);
                            const float sp2 = cos(pedAngle) * 0.5f + sin(pedAngle);
                            float x2 = sp * 15.0f + x1;
                            float y2 = (15.0f * sin(pedAngle) + y1) - cos(pedAngle) * 15.0f * 0.5f;
                            float x3 = (cos(pedAngle) * 15.0f + x1) - 15.0f * sin(pedAngle) * 0.5f;
                            float y3 = sp2 * 15.0f + y1;
                            CSprite::RenderOneXLUSprite_Triangle({ x1, y1 }, { x2, y2 }, { x3, y3 }, 8.0f, 0, 0, 0, (int16)0xFF, 1.0f, 0xFF);

                            x1 += cos(pedAngle) * 15.0f * 0.1f;
                            y1 += 0.1f * (15.0f * sin(pedAngle));
                            x2  = sp * 15.0f * 0.8f + x1;
                            y2  = (15.0f * sin(pedAngle) - 0.5f * cos(pedAngle) * 15.0f) * 0.8f + y1;
                            x3  = (cos(pedAngle) * 15.0f - 15.0f * sin(pedAngle) * 0.5f) * 0.8f + x1;
                            y3  = sp2 * 15.0f * 0.8f + y1;
                            CSprite::RenderOneXLUSprite_Triangle({ x1, y1 }, { x2, y2 }, { x3, y3 }, 8.0f, ch.m_color.r, ch.m_color.g, ch.m_color.b, (int16)0xFF, 1.0f, 0xFF);
                        }
                    } else {
                        // Original: FUN_00404330(m_fSize, 1.2f, 0.3f) then FUN_00420800(result).
                        // Per batch_out/w2-01-Bike: 0x404330=max(float,float), 0x420800=min(float,float);
                        // Ghidra shows a 3rd argument 0.3f for max and loses the 2nd one for min - calls kept as is.
                        ch.m_fSize = plugin::CallAndReturn<float, 0x404330, float, float, float>(ch.m_fSize, 1.2f, 0.3f);
                        ch.m_fSize = plugin::CallAndReturn<float, 0x420800, float>(ch.m_fSize);
                        float szA = 5.0f * ch.m_fSize;   // 0x858C80
                        float szB = ch.m_fSize * 25.0f;  // 0x858FE8
                        if (scr.z < 2.5f) {
                            scr.z = 2.5f;
                        }
                        // Ghidra loses this (ecx); radius is taken from the current player's ped
                        if (const auto radius = playerPed->GetWeaponRadiusOnScreen(); radius > 0.0f) {
                            szA = radius * 30.0f; // 0x8D6148
                            szB = ch.m_fSize * 20.0f + szA;
                        }

                        RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDZERO));
                        RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDZERO));

                        for (auto i = 0; i < 3; i++) {
                            const auto a = (float)i * 120.0f * DEG_TO_RAD + rotAccum;
                            const auto b = ((float)i * 120.0f + 15.0f) * DEG_TO_RAD + rotAccum;
                            const auto c = ((float)i * 120.0f - 15.0f) * DEG_TO_RAD + rotAccum;

                            const float x1 = -(sin(a) * (szA + triRadius)) + scr.x;
                            const float y1 = -(cos(a) * (szA + triRadius)) + scr.y;
                            const float x2 = -(sin(b) * (szB + triRadius)) + scr.x;
                            const float y2 = -(cos(b) * (szB + triRadius)) + scr.y;
                            const float x3 = -(sin(c) * (szB + triRadius)) + scr.x;
                            const float y3 = -(cos(c) * (szB + triRadius)) + scr.y;
                            CSprite::RenderOneXLUSprite_Triangle({ x1, y1 }, { x2, y2 }, { x3, y3 }, scr.z, 0, 0, 0, (int16)0xFF, 1.0f, ch.m_color.a);

                            // Inner triangle (shrunk towards the centroid)
                            const float cx = (x1 + x2 + x3) / 3.0f;
                            const float cy = (y1 + y2 + y3) / 3.0f;
                            const float j1x = (x1 - cx) * 0.75f + cx, j1y = (y1 - cy) * 0.75f + cy;
                            const float j2x = (x2 - cx) * 0.75f + cx, j2y = (y2 - cy) * 0.75f + cy;
                            const float j3x = (x3 - cx) * 0.75f + cx, j3y = (y3 - cy) * 0.75f + cy;

                            RwRenderStateSet(rwRENDERSTATESRCBLEND, RWRSTATE(rwBLENDSRCALPHA));
                            RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCALPHA));
                            RwRenderStateSet(rwRENDERSTATECULLMODE, RWRSTATE(rwCULLMODECULLNONE));
                            CSprite::RenderOneXLUSprite_Triangle({ j1x, j1y }, { j2x, j2y }, { j3x, j3y }, scr.z, ch.m_color.r, ch.m_color.g, ch.m_color.b, (int16)0xFF, 1.0f, ch.m_color.a);
                        }

                        // Crosshair rotation/size
                        if (ch.m_nTimeWhenToDeactivate == 0 || ch.m_nTimeWhenToDeactivate == -1) {
                            rotAccum += 0.05f; // 0x858C28
                        } else {
                            rotAccum  += 0.75f; // 0x858F34
                            triRadius += 2.0f;  // 0x858CA0
                            ch.m_fSize *= 0.9f; // 0x858C20
                        }
                        if (rotAccum > TWO_PI) {
                            rotAccum = 0.0f;
                        }
                        if (ch.m_nTimeWhenToDeactivate == 0) {
                            if (triGrowDir == 0) {
                                triRadius -= 2.0f;
                                if (triRadius < 0.0f) {
                                    triGrowDir = 1;
                                }
                            } else {
                                triRadius += ch.m_fSize + ch.m_fSize;
                                if (ch.m_fSize * 20.0f < triRadius) {
                                    triGrowDir = 0;
                                }
                            }
                        }
                    }
                    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
                    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
                    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
                }
            }
        }

        // 2-player drive-by shooting crosshair (texture target256)
        if (TheCamera.GetActiveCam().m_nMode == MODE_TWOPLAYER_IN_CAR_AND_SHOOTING) { // 0x31
            RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
            RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(FALSE));
            RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
            RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
            RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
            RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(gpCrossHairTex->raster));

            const auto& cam   = TheCamera.GetActiveCam();
            const float aimX  = (cam.m_fX_Targetting + 1.0f) * scrW * 0.5f;
            const float aimY  = (cam.m_fY_Targetting + 1.0f) * scrH * 0.5f;
            const float angle = (float)(CTimer::GetTimeInMS() & 0x3FF) * 0.006135923f;
            CSprite::RenderOneXLUSprite_Rotate_Aspect({ aimX, aimY, 100.0f }, { 10.0f, 10.0f }, 0xFF, 0x80, 0, (int16)0x80, 0.01f, angle, (uint8)0xFF);

            // Auto-aim target - player 1, if player 0 is in a car but not driving
            auto* ped = CWorld::Players[1].m_pPed;
            if (const auto* p0 = CWorld::Players[0].m_pPed;
                p0->m_pVehicle && p0->m_pVehicle->m_pDriver != p0) {
                ped = CWorld::Players[0].m_pPed;
            }

            CEntity* target{};
            if (ped) {
                const auto& weapon = ped->m_aWeapons[ped->m_nActiveWeaponSlot];
                const auto* wi     = CWeaponInfo::GetWeaponInfo(weapon.m_Type, ped->GetWeaponSkill());
                target = CWeapon::FindNearestTargetEntityWithScreenCoors(
                    cam.m_fX_Targetting, cam.m_fY_Targetting,
                    wi->m_fWeaponRange + wi->m_fWeaponRange,
                    ped->GetPosition(), nullptr, nullptr
                );
            }

            if (target != s_ChLastTarget) {
                s_ChTargetChangedMs = CTimer::GetTimeInMS();
                s_ChLastTarget      = target;
            }
            if (target) {
                RwV3d scr{};
                float tW{}, tH{};
                if (CSprite::CalcScreenCoors(target->GetPosition(), &scr, &tW, &tH, true, true)) {
                    if (const auto scale = 20.0f / tH; scale > 1.0f) {
                        tW *= scale;
                        tH *= scale;
                    }
                    const auto dt    = (int32)(CTimer::GetTimeInMS() - s_ChTargetChangedMs);
                    auto       scale = 3.0f - (float)dt * 0.001953125f; // 1/512
                    if (scale < 1.0f) {
                        scale = 1.0f;
                    }
                    const auto alpha = (uint8)std::min(((dt + ((dt >> 31) & 3)) >> 2) + 0x46, 0xFF);
                    CSprite::RenderOneXLUSprite_Rotate_Aspect(
                        { scr.x, scr.y, scr.z }, { tW * scale, tH * scale },
                        alpha, 0, 0,
                        (int16)alpha,
                        0.01f, (float)(CTimer::GetTimeInMS() & 0x3FF) * 0.006135923f, (uint8)0xFF
                    );
                }
            }
            RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(TRUE));
            RwRenderStateSet(rwRENDERSTATEZTESTENABLE,  RWRSTATE(TRUE));
        }

        // Final pass over the 2 players (0x56EF90 is not reversed; in the original - a loop
        // over a pointer from 0xB7CEF0 with 0x190 stride == 2 iterations)
        for (auto i = 0; i < 2; i++) {
            plugin::Call<0x56EF90, int32>(i);
        }
    }
}
