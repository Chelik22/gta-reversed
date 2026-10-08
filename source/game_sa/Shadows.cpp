/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"
#include <imgui.h>
#include "Shadows.h"
#include "FireManager.h"
#include <CustomBuildingDNPipeline.h>
#include "Collision/ColHelpers.h"

void CShadows::InjectHooks() {
    RH_ScopedClass(CShadows);
    RH_ScopedCategory("Shadows");

    RH_ScopedInstall(Init, 0x706CD0);
    RH_ScopedInstall(Shutdown, 0x706ED0);
    RH_ScopedInstall(TidyUpShadows, 0x707770);
    RH_ScopedInstall(AddPermanentShadow, 0x706F60);
    RH_ScopedInstall(UpdatePermanentShadows, 0x70C950);
    RH_ScopedOverloadedInstall(StoreShadowToBeRendered, "Texture", 0x707390, void(*)(uint8, RwTexture*, const CVector&, float, float, float, float, int16, uint8, uint8, uint8, float, bool, float, CRealTimeShadow*, bool));
    RH_ScopedOverloadedInstall(StoreShadowToBeRendered, "Type", 0x707930, void(*)(uint8, const CVector&, float, float, float, float, int16, uint8, uint8, uint8));
    RH_ScopedInstall(SetRenderModeForShadowType, 0x707460);
    RH_ScopedInstall(RemoveOilInArea, 0x7074F0);
    RH_ScopedInstall(GunShotSetsOilOnFire, 0x707550);
    RH_ScopedInstall(PrintDebugPoly, 0x7076B0);
    RH_ScopedInstall(CalcPedShadowValues, 0x7076C0);
    RH_ScopedInstall(AffectColourWithLighting, 0x707850);
    RH_ScopedInstall(StoreShadowForPedObject, 0x707B40);
    RH_ScopedInstall(StoreRealTimeShadow, 0x707CA0);
    RH_ScopedInstall(UpdateStaticShadows, 0x707F40);
    RH_ScopedInstall(RenderExtraPlayerShadows, 0x707FA0);
    RH_ScopedInstall(RenderStaticShadows, 0x708300);
    RH_ScopedInstall(CastShadowEntityXY, 0x7086B0);
    RH_ScopedInstall(CastShadowEntityXYZ, 0x70A040);
    RH_ScopedInstall(CastPlayerShadowSectorList<CPtrListSingleLink<CPhysical*>>, 0x70A470);
    RH_ScopedInstall(CastShadowSectorList<CPtrListSingleLink<CPhysical*>>, 0x70A630, { .Reversed = false });
    RH_ScopedInstall(CastRealTimeShadowSectorList<CPtrListSingleLink<CPhysical*>>, 0x70A7E0, { .Reversed = false });
    RH_ScopedInstall(RenderStoredShadows, 0x70A960);
    RH_ScopedInstall(GeneratePolysForStaticShadow, 0x70B730);
    RH_ScopedInstall(StoreStaticShadow, 0x70BA00);
    RH_ScopedInstall(StoreShadowForVehicle, 0x70BDA0);
    RH_ScopedInstall(StoreCarLightShadow, 0x70C500);
    RH_ScopedInstall(StoreShadowForPole, 0x70C750);
    RH_ScopedInstall(RenderIndicatorShadow, 0x70CCB0);
    RH_ScopedGlobalInstall(ShadowRenderTriangleCB, 0x709CF0, { .Reversed = false });
}

void CStaticShadow::InjectHooks() {
    RH_ScopedClass(CStaticShadow);
    RH_ScopedCategory("Shadows");

    RH_ScopedInstall(Free, 0x707670);
    //RH_ScopedInstall(Init, 0x0);
}

// 0x707670 
void CStaticShadow::Free() {
    if (m_pPolyBunch) {
        const auto prevHead = CShadows::pEmptyBunchList;
        CShadows::pEmptyBunchList = m_pPolyBunch;

        // Find last in the list and make it point to the previous head
        auto it{ m_pPolyBunch };
        while (it->m_pNext) {
            it = static_cast<CPolyBunch*>(it->m_pNext);
        }
        it->m_pNext = prevHead;

        m_pPolyBunch = nullptr;
        m_nId = 0;
    }
}

// 0x706CD0
void CShadows::Init() {
    CTxdStore::PushCurrentTxd();
    CTxdStore::SetCurrentTxd(CTxdStore::FindTxdSlot("particle"));

    gpShadowCarTex         = RwTextureRead("shad_car",     nullptr);
    gpShadowPedTex         = RwTextureRead("shad_ped",     nullptr);
    gpShadowHeliTex        = RwTextureRead("shad_heli",    nullptr);
    gpShadowBikeTex        = RwTextureRead("shad_bike",    nullptr);
    gpShadowBaronTex       = RwTextureRead("shad_rcbaron", nullptr);
    gpShadowExplosionTex   = RwTextureRead("shad_exp",     nullptr);
    gpShadowHeadLightsTex  = RwTextureRead("headlight",    nullptr);
    gpShadowHeadLightsTex2 = RwTextureRead("headlight1",   nullptr);
    gpBloodPoolTex         = RwTextureRead("bloodpool_64", nullptr);
    gpHandManTex           = RwTextureRead("handman",      nullptr);
    gpCrackedGlassTex      = RwTextureRead("wincrack_32",  nullptr);
    gpPostShadowTex        = RwTextureRead("lamp_shad_64", nullptr);

    CTxdStore::PopCurrentTxd();

    g_ShadowVertices = { 0, 2, 1, 0, 3, 2, 0, 4, 3, 0, 5, 4, 0, 6, 5, 0, 7, 6, 0, 8, 7, 0, 9, 8 };

    std::ranges::for_each(aStaticShadows, [&](auto& shadow) { shadow.Init(); });

    pEmptyBunchList = aPolyBunches.data();

    for (auto i = 0u; i < aPolyBunches.size() - 1; i++) {
        aPolyBunches[i].m_pNext = &aPolyBunches[i + 1];
    }
    aPolyBunches.back().m_pNext = nullptr;

    std::ranges::for_each(aPermanentShadows, [&](auto& shadow) { shadow.Init(); });
}

// 0x706ED0
void CShadows::Shutdown() {
    RwTextureDestroy(gpShadowCarTex);
    RwTextureDestroy(gpShadowPedTex);
    RwTextureDestroy(gpShadowHeliTex);
    RwTextureDestroy(gpShadowBikeTex);
    RwTextureDestroy(gpShadowBaronTex);
    RwTextureDestroy(gpShadowExplosionTex);
    RwTextureDestroy(gpShadowHeadLightsTex);
    RwTextureDestroy(gpShadowHeadLightsTex2);
    RwTextureDestroy(gpBloodPoolTex);
    RwTextureDestroy(gpHandManTex);
    RwTextureDestroy(gpCrackedGlassTex);
    RwTextureDestroy(gpPostShadowTex);
}

// 0x707770
void CShadows::TidyUpShadows() {
    std::ranges::for_each(
        aPermanentShadows,
        [&](auto& shadow) { shadow.m_nType = SHADOW_NONE; }
    );
}

// 0x706F60
void CShadows::AddPermanentShadow(uint8 type, RwTexture* texture, CVector* posn, float topX, float topY, float rightX, float rightY, int16 intensity, uint8 red, uint8 greeb, uint8 blue, float drawDistance, uint32 time, float upDistance) {
    // Find a free slot
    auto slotID = 0;
    while (slotID < MAX_PERMANENT_SHADOWS && aPermanentShadows[slotID].m_nType != SHADOW_NONE) {
        slotID++;
    }

    if (slotID == MAX_PERMANENT_SHADOWS) {
        // No free slot - Steal the slot of the oldest (and smallest) shadow
        auto oldestTimeCreated = (uint32)-1;
        for (auto i = 0u; i < MAX_PERMANENT_SHADOWS; i++) {
            const auto& shdw = aPermanentShadows[i];
            if (sq(shdw.m_fFrontX) + sq(shdw.m_fFrontY) < 0.25f // 0x858C84
                && sq(shdw.m_fSideX) + sq(shdw.m_fSideY) < 0.25f
                && shdw.m_nTimeCreated < oldestTimeCreated
            ) {
                slotID = i;
                oldestTimeCreated = shdw.m_nTimeCreated;
            }
        }
    }

    // ABI: оригинальный порядок аргументов (arg12=drawDistance -> m_fZDistance, arg13=time -> m_nTimeDuration, arg14=upDistance -> m_fScale)
    if (slotID != MAX_PERMANENT_SHADOWS) {
        auto& shdw = aPermanentShadows[slotID];
        shdw.m_nType         = (eShadowType)type;
        shdw.m_pTexture      = texture;
        shdw.m_vecPosn       = *posn;
        shdw.m_fFrontX       = topX;
        shdw.m_fFrontY       = topY;
        shdw.m_fSideX        = rightX;
        shdw.m_fSideY        = rightY;
        shdw.m_nIntensity    = intensity;
        shdw.m_nRed          = red;
        shdw.m_nGreen        = greeb;
        shdw.m_nBlue         = blue;
        shdw.m_fZDistance    = drawDistance;
        shdw.m_nTimeDuration = time;
        shdw.m_fScale        = upDistance;
        shdw.m_nTimeCreated  = CTimer::GetTimeInMS();
    }
}
// 0x70C950
void CShadows::UpdatePermanentShadows() {
    ZoneScoped;

    for (auto& shdw : aPermanentShadows) {
        if (shdw.m_nType == SHADOW_NONE) {
            continue;
        }

        const auto timeAlive = CTimer::GetTimeInMS() - shdw.m_nTimeCreated;
        if (timeAlive < shdw.m_nTimeDuration) {
            int16 intensity;
            uint8 r, g, b;
            if (timeAlive < (shdw.m_nTimeDuration * 3) >> 2) { // Not yet in the final quarter of it's lifetime
                intensity = shdw.m_nIntensity;
                r = shdw.m_nRed;
                g = shdw.m_nGreen;
                b = shdw.m_nBlue;
            } else { // Fade out in the final quarter of the lifetime
                const auto fade = 1.0f - (float)(timeAlive - (((shdw.m_nTimeDuration * 3) >> 2))) / (float)(shdw.m_nTimeDuration >> 2);
                b        = (uint8)((float)shdw.m_nBlue  * fade);
                g        = (uint8)((float)shdw.m_nGreen * fade);
                r        = (uint8)((float)shdw.m_nRed   * fade);
                intensity = (int16)((float)shdw.m_nIntensity * fade);
            }

            // Re-store it as a static shadow
            if (!StoreStaticShadow(
                (uint32)(uintptr)&shdw, // The shadow's address is used as it's id
                shdw.m_nType,
                shdw.m_pTexture,
                shdw.m_vecPosn,
                shdw.m_fFrontX, shdw.m_fFrontY,
                shdw.m_fSideX, shdw.m_fSideY,
                (uint8)intensity,
                r, g, b,
                shdw.m_fZDistance,
                1.0f,
                40.0f,
                false,
                0.0f
            ) && shdw.m_nType != SHADOW_OIL_5) {
                shdw.m_nType = SHADOW_NONE;
            }
        } else { // Lifetime expired
            shdw.m_nType = SHADOW_NONE;
        }
    }

    // Every 4th frame handle burning oil setting other oil splats on fire
    if ((CTimer::GetFrameCounter() & 3) == 0) {
        for (auto i = 0u; i < MAX_PERMANENT_SHADOWS; i++) {
            auto& shdw = aPermanentShadows[i];
            if (shdw.m_nType != SHADOW_OIL_2) { // Oil on fire?
                continue;
            }

            // It burns out in 2 seconds
            shdw.m_nType         = SHADOW_OIL_3;
            shdw.m_nTimeCreated  = CTimer::GetTimeInMS();
            shdw.m_nTimeDuration = 2000;

            // Find the closest oil splat (not on fire) to ignite
            auto closestID = -1;
            auto closestDist = 3.0f;
            for (auto j = 0u; j < MAX_PERMANENT_SHADOWS; j++) {
                const auto& other = aPermanentShadows[j];
                if (other.m_nType != SHADOW_OIL_1 && other.m_nType != SHADOW_OIL_5) {
                    continue;
                }
                if (const auto dist = (other.m_vecPosn - shdw.m_vecPosn).Magnitude(); dist < closestDist) {
                    closestID = j;
                    closestDist = dist;
                }
            }

            if (closestID != -1) {
                auto& closest = aPermanentShadows[closestID];

                // Mark it as "to be ignited" (see loop at the bottom)
                closest.m_nType = SHADOW_OIL_4;

                gFireManager.StartFire(closest.m_vecPosn, 1.8f, 0, nullptr, 2000u, 0, 1);

                // Find another oil splat (not on fire) that lies in the opposite direction
                // of the one set on fire above, and ignite it too (fire spreads in 2 directions)
                auto oppositeID = -1;
                auto oppositeDist = 3.0f; // 0x858B3C
                const auto toClosest = CVector2D{ closest.m_vecPosn - shdw.m_vecPosn };
                for (auto j = 0u; j < MAX_PERMANENT_SHADOWS; j++) {
                    const auto& other = aPermanentShadows[j];
                    if (other.m_nType != SHADOW_OIL_1 && other.m_nType != SHADOW_OIL_5) {
                        continue;
                    }
                    if (const auto dist = (other.m_vecPosn - shdw.m_vecPosn).Magnitude(); dist < oppositeDist) {
                        const auto toOther = CVector2D{ other.m_vecPosn - shdw.m_vecPosn };
                        if (toOther.Dot(toClosest) < 0.0f) {
                            oppositeID = j;
                            oppositeDist = dist;
                        }
                    }
                }

                if (oppositeID != -1) {
                    auto& opposite = aPermanentShadows[oppositeID];
                    opposite.m_nType = SHADOW_OIL_4;
                    gFireManager.StartFire(opposite.m_vecPosn, 1.8f, 0, nullptr, 2000u, 0, 1);
                }
            }
        }

        // All oil splats marked above are now burning (type 7 -> 5)
        for (auto& shdw : aPermanentShadows) {
            if (shdw.m_nType == SHADOW_OIL_4) {
                shdw.m_nType = SHADOW_OIL_2;
            }
        }
    }
}
// 0x707930
void CShadows::StoreShadowToBeRendered(uint8 type, const CVector& posn, float frontX, float frontY, float sideX, float sideY, int16 intensity, uint8 red, uint8 green, uint8 blue) {
    const auto Store = [=](auto mtype, auto texture) {
        StoreShadowToBeRendered(mtype, texture, posn, frontX, frontY, sideX, sideY, (uint8)intensity, red, green, blue, 15.0f, 0, 1.0f, nullptr, 0);
    };

    switch (type) {
    case SHADOW_DEFAULT:
        Store(SHADOW_TEX_CAR, gpShadowCarTex);
        break;
    case SHADOW_ADDITIVE:
        Store(SHADOW_TEX_CAR, gpShadowPedTex);
        break;
    case SHADOW_INVCOLOR:
        Store(SHADOW_TEX_PED, gpShadowExplosionTex);
        break;
    case SHADOW_OIL_1:
        Store(SHADOW_TEX_CAR, gpShadowHeliTex);
        break;
    case SHADOW_OIL_2:
        Store(SHADOW_TEX_PED, gpShadowHeadLightsTex);
        break;
    case SHADOW_OIL_3:
        Store(SHADOW_TEX_CAR, gpBloodPoolTex);
        break;
    default:
        return;
    }
}

// 0x707390
void CShadows::StoreShadowToBeRendered(uint8 type, RwTexture* texture, const CVector& posn, float topX, float topY, float rightX, float rightY, int16 intensity, uint8 red, uint8 green, uint8 blue, float zDistance, bool drawOnWater, float scale, CRealTimeShadow* realTimeShadow, bool drawOnBuildings) {
    if (ShadowsStoredToBeRendered >= asShadowsStored.size())
        return;

    auto& shadow = asShadowsStored[ShadowsStoredToBeRendered];

    shadow.m_nType      = (eShadowType)type;
    shadow.m_pTexture   = texture;
    shadow.m_vecPosn    = posn;
    shadow.m_Front.x    = topX;
    shadow.m_Front.y    = topY;
    shadow.m_Side.x     = rightX;
    shadow.m_Side.y     = rightY;
    shadow.m_nIntensity = intensity;
    shadow.m_nRed       = red;
    shadow.m_nGreen     = green;
    shadow.m_nBlue      = blue;
    shadow.m_fZDistance = zDistance;
    shadow.m_bDrawOnWater     = drawOnWater;
    shadow.m_bDrawOnBuildings = drawOnBuildings;
    shadow.m_fScale     = scale;
    shadow.m_pRTShadow  = realTimeShadow;

    ShadowsStoredToBeRendered++;
}

void CShadows::StoreShadowToBeRendered(eShadowType type, RwTexture* tex, const CVector& posn, CVector2D top, CVector2D right, int16 intensity, uint8 red, uint8 green, uint8 blue, float zDistance, bool drawOnWater, float scale, CRealTimeShadow* realTimeShadow, bool drawOnBuildings) {
    StoreShadowToBeRendered(
        type,
        tex,
        posn,
        top.x, top.y,
        right.x, right.y,
        (uint8)intensity,
        red, green, blue,
        zDistance,
        drawOnWater,
        scale,
        realTimeShadow,
        drawOnBuildings
    );
}

// 0x707460
void CShadows::SetRenderModeForShadowType(eShadowType type) {
    switch (type) {
    case SHADOW_DEFAULT:
    case SHADOW_OIL_1:
    case SHADOW_OIL_2:
    case SHADOW_OIL_3:
    /* case SHADOW_OIL_4: */ // missing
    case SHADOW_OIL_5:
        RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDSRCALPHA));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCALPHA));
        break;
    case SHADOW_ADDITIVE:
        RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDONE));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDONE));
        break;
    case SHADOW_INVCOLOR:
        RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDZERO));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCCOLOR));
        break;
    default:
        return;
    }
}

// 0x7074F0
void CShadows::RemoveOilInArea(float minX, float maxX, float minY, float maxY) {
    CRect rect{ {minX, minY}, {maxX, maxY} };
    for (auto& shadow : aPermanentShadows) {
        switch (shadow.m_nType) {
        case SHADOW_OIL_1:
        case SHADOW_OIL_5:
            break;
        default:
            continue;
        }
        if (rect.IsPointInside(shadow.m_vecPosn)) {
            shadow.m_nType = SHADOW_NONE;
        }
    }
}

// 0x707550
void CShadows::GunShotSetsOilOnFire(const CVector& shotOrigin, const CVector& shotTarget) {
    const auto V3DChangeZ = [] (const CVector& pos) {
        return CVector{ pos.x, pos.y, pos.z * 0.27f };
    };

    const CColLine line{V3DChangeZ(shotOrigin), V3DChangeZ(shotTarget)};

    // Find closest shadow (but not further than 1 unit)
    CPermanentShadow* closest{};
    auto              closestDist{1.f};
    for (auto& shdw : aPermanentShadows) {
        switch (shdw.m_nType) {
        case SHADOW_OIL_1:
        case SHADOW_OIL_5:
            break;
        default:
            continue;
        }

        const auto dist = line.DistTo(V3DChangeZ(shdw.m_vecPosn));
        if (dist < closestDist) {
            closestDist = dist;
            closest     = &shdw;
        }
    }

    if (closest) {
        closest->m_nType = SHADOW_OIL_2;
        gFireManager.StartFire(closest->m_vecPosn, 1.8f, 0, nullptr, 2000u, 0, 1);
    }
}

// 0x7076B0
void CShadows::PrintDebugPoly(CVector* a, CVector* b, CVector* c) {
    // NOP
}

// 0x7076C0
void CShadows::CalcPedShadowValues(
    CVector sunPosn,
    float& frontX,        float& frontY,
    float& sideX,         float& sideY,
    float& displacementX, float& displacementY
) {
    const auto sunDist = sunPosn.Magnitude2D();
    const auto recip = 1.0f / sunDist;

    const auto mult = (sunDist + 1.0f) * recip;
    frontX = -sunPosn.x * mult / 2.0f;
    frontY = -sunPosn.y * mult / 2.0f;

    sideX = -sunPosn.y * recip / 2.0f;
    sideY = +sunPosn.x * recip / 2.0f;

    displacementX = -sunPosn.x / 2.0f;
    displacementY = -sunPosn.y / 2.0f;
}

// 0x707850
void CShadows::AffectColourWithLighting(
    eShadowType shadowType,
    uint8 dayNightIntensity, // packed 2x4 bits for day/night
    uint8 r, uint8 g, uint8 b,
    uint8& outR, uint8& outG, uint8& outB
) {
    if (shadowType != SHADOW_ADDITIVE) {
        const auto mult = std::min(
            0.4f + 0.6f * (1.f - CCustomBuildingDNPipeline::m_fDNBalanceParam),
            0.3f + 0.7f * lerp(
                (float)(dayNightIntensity >> 0 & 0b1111) / 30.f,
                (float)(dayNightIntensity >> 4 & 0b1111) / 30.f,
                CCustomBuildingDNPipeline::m_fDNBalanceParam
            )
        );
        outR = (uint8)((float)r * mult);
        outG = (uint8)((float)g * mult);
        outB = (uint8)((float)b * mult);
    } else {
        outR = r;
        outG = g;
        outB = b;
    }
}

// NOTSA
uint16 CalculateShadowStrength(float currDist, float maxDist, uint16 maxStrength) {
    assert(maxDist >= currDist); // Otherwise integer underflow will occur

    const auto halfMaxDist = maxDist / 2.f;
    if (currDist >= halfMaxDist) { // Anything further than half the distance is faded out
        return (uint16)((1.f - (currDist - halfMaxDist) / halfMaxDist) * maxStrength);
    } else { // Anything closer than half the max distance is full strength
        return (uint16)maxStrength;
    }
}

// 0x707B40
void CShadows::StoreShadowForPedObject(CPed* ped, float displacementX, float displacementY, float frontX, float frontY, float sideX, float sideY) {
    // Okay, so.
    // This function is called from `CCutsceneObject::PreRender`
    // And you might ask "what the fuck, an object is not a ped!!"
    // well, in R* world it is.
    // it has bones (as anything can have bones actually, that's a known fact)
    // the `GetBonePosition` below *just works* because it doesn't access any
    // `CPed` specific member variables.
    // But we really should fix this in a sensible way in the future.
    assert(ped->GetIsTypePed() || ped->GetIsTypeObject());

    const auto  bonePos           = ped->GetBonePosition(BONE_ROOT);
    const auto& camPos            = TheCamera.GetPosition();
    const auto  boneToCamDist2DSq = (bonePos - camPos).SquaredMagnitude2D();

    // Check if ped is close enough
    if (boneToCamDist2DSq >= MAX_DISTANCE_PED_SHADOWS_SQR) {
        return;
    }

    const auto isPlayerPed = FindPlayerPed() == ped;

    // Check if ped is visible to the camera
    if (!isPlayerPed) {  // Optimization: Assume player ped is always visible
        if (!TheCamera.IsSphereVisible(ped->GetPosition(), 2.f)) {
            return;
        }
    }

    // Now store a shadow to be rendered
    const auto strength = (uint8)CalculateShadowStrength(std::sqrt(boneToCamDist2DSq), MAX_DISTANCE_PED_SHADOWS, CTimeCycle::m_CurrentColours.m_nShadowStrength);
    StoreShadowToBeRendered(
        SHADOW_DEFAULT,
        gpShadowPedTex,
        bonePos + CVector{displacementX, displacementY, 0.f},
        frontX, frontY,
        sideX, sideY,
        strength,
        strength, strength, strength,
        4.f,
        false,
        1.f,
        nullptr,
        isPlayerPed || g_fx.GetFxQuality() >= FX_QUALITY_VERY_HIGH // NOTSA: At higher FX quality draw all ped's shadows
    );
}

// 0x707CA0
void CShadows::StoreRealTimeShadow(CPhysical* physical, float displacementX, float displacementY, float frontX, float frontY, float sideX, float sideY) {
    const auto rtshdw = physical->m_pShadowData;
    if (!rtshdw) {
        return;
    }
    const auto& camPos = TheCamera.GetPosition();
    const auto  shdwPos = physical->GetIsTypePed()
        ? physical->AsPed()->GetBonePosition(BONE_ROOT)
        : physical->GetPosition();
    const auto shdwToCamDist2DSq = (shdwPos - camPos).SquaredMagnitude2D();

    // Check distance to camera
    if (shdwToCamDist2DSq > MAX_DISTANCE_PED_SHADOWS_SQR) {
        return;
    }

    // Check if the object is visible to the camera
    if (FindPlayerPed() != physical) {  // Optimization: Assume player ped is always visible
        if (!TheCamera.IsSphereVisible(shdwPos, 2.f)) {
            return;
        }
    }

    const auto strength = (float)CalculateShadowStrength(std::sqrt(shdwToCamDist2DSq), MAX_DISTANCE_PED_SHADOWS, CTimeCycle::m_CurrentColours.m_nShadowStrength);
    const auto cc       = (uint8)((float)rtshdw->m_nIntensity / 100.f * strength);

    const auto& vecToSun = CTimeCycle::m_VectorToSun[CTimeCycle::m_CurrentStoredValue];
    const auto lightFrame = rtshdw->SetLightProperties(
        RWRAD2DEG(+std::atan2(-vecToSun.x, -vecToSun.y)),
        RWRAD2DEG(-std::atan2(+vecToSun.x, -vecToSun.z)),
        true
    );
    CalcPedShadowValues(
        *RwMatrixGetAt(RwFrameGetMatrix(lightFrame)),
        displacementX, displacementY,
        frontX, frontY,
        sideX, sideY
    );
    StoreShadowToBeRendered(
        SHADOW_INVCOLOR,
        rtshdw->GetShadowRwTexture(),
        shdwPos - CVector{ displacementX, displacementY, 0.f } * 2.5f,
        frontX * 1.5f, frontY * 1.5f,
        sideX * 1.5f, sideY * 1.5f,
        cc,
        cc, cc, cc,
        4.f,
        false,
        1.f,
        rtshdw,
        g_fx.GetFxQuality() >= FX_QUALITY_VERY_HIGH // NOTSA: At higher FX quality draw all shadows on buildings too
    );
}

// 0x707F40
void CShadows::UpdateStaticShadows() {
    // Remove shadows that have no polies/are temporary and have expired
    for (auto& sshdw : aStaticShadows) {
        if (!sshdw.m_pPolyBunch || sshdw.m_bJustCreated) {
            goto skip; // Not even created fully
        }

        if (sshdw.m_bTemporaryShadow && CTimer::GetTimeInMS() <= sshdw.m_nTimeCreated + 5000u) {
            goto skip; // Not expired yet
        }

        sshdw.Free();

    skip:
        sshdw.m_bJustCreated = false;
    }
}

// 0x707FA0
void CShadows::RenderExtraPlayerShadows() {
    if (!CTimeCycle::m_CurrentColours.m_nShadowStrength) {
        return;
    }

    const auto plyrVeh = FindPlayerVehicle();
    if (!plyrVeh) {
        return;
    }

    if (plyrVeh->m_nModelIndex == eModelID::MODEL_RCBANDIT) {
        return;
    }

    if (plyrVeh->GetVehicleAppearance() == VEHICLE_APPEARANCE_BIKE) {
        return;
    }

    switch (plyrVeh->m_nVehicleType) {
    case VEHICLE_TYPE_BIKE:
    case VEHICLE_TYPE_FPLANE:
    case VEHICLE_TYPE_BOAT:
        return;
    }

    const auto  plyrPos    = FindPlayerCoors();
    const auto& plyrVehPos = plyrVeh->GetPosition();
    const auto& plyVehMat  = plyrVeh->GetMatrix();
    for (auto& ptl : CPointLights::GetActiveLights()) {
        if (ptl.m_nType != ePointLightType::PLTYPE_POINTLIGHT) {
            continue;
        }
        if (!ptl.m_bGenerateShadows) {
            continue;
        }
        if (ptl.m_fColorRed == 0.f && ptl.m_fColorGreen == 0.f && ptl.m_fColorBlue == 0.f) { // If it's black => ignore
            continue;
        }

        const auto lightToPlyr{ ptl.m_vecPosn - plyrPos };
        const auto lightToPlyrDist = lightToPlyr.Magnitude();
        if (lightToPlyrDist >= ptl.m_fRadius || lightToPlyrDist == 0.f) { // NOTSA: Zero check to prevent (possible) division-by-zero
            continue;
        }

        const auto lightToPlyrDir  = lightToPlyr / lightToPlyrDist; // Normalize vector

        const auto& plyrVehBB = plyrVeh->GetColModel()->GetBoundingBox();
        const auto  shdwSize  = CVector2D{ plyrVehBB.GetSize() } / 2.f;

        StoreShadowToBeRendered(
            SHADOW_DEFAULT,
            gpShadowCarTex,
            plyrVehPos - CVector{
                  CVector2D{ lightToPlyr / lightToPlyrDist } * 1.2f // Compensate for elvation of the light
                - CVector2D{ plyVehMat.GetForward() } * (shdwSize.y - plyrVehBB.m_vecMax.y) // Move point to center (as the shadow's position is it's center)
            },
            CVector2D{ plyVehMat.GetForward() } * shdwSize.y,
            CVector2D{ plyVehMat.GetRight() } * (plyVehMat.GetUp().z >= 0.f ? shdwSize.x : -shdwSize.x), // If vehicle is flipped, we gotta flip the `right` vector back
            CalculateShadowStrength( // 0x70809A
                lightToPlyrDist,
                ptl.m_fRadius,
                5 * CTimeCycle::m_CurrentColours.m_nShadowStrength / 8u // Same as mult by `0.625` and then casting to int
            ),
            0, 0, 0,
            4.5f,
            false,
            1.f,
            nullptr,
            false
        );
    }
}

// 0x708300
void CShadows::RenderStaticShadows() {
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,          RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,    RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,            RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,        RWRSTATE(rwFILTERLINEAR));

    RenderBuffer::ClearRenderBuffer();

    // Mark all as not-yet-rendered
    for (auto& shdw : aStaticShadows) {
        shdw.m_bRendered = false;
    }

    // Render all in batches
    for (auto& oshdw : aStaticShadows) {
        if (!oshdw.m_pPolyBunch || oshdw.m_bRendered) {
            continue;
        }

        // Setup additional render states for this shadow
        SetRenderModeForShadowType(oshdw.m_nType);
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(RwTextureGetRaster(oshdw.m_pTexture)));
         
        // Batch all other shadows with the same texture and type into the buffer
        for (auto& ishdw : aStaticShadows) {
            if (!ishdw.m_pPolyBunch) {
                continue;
            }
            if (ishdw.m_nType != oshdw.m_nType) {
                continue;
            }
            if (ishdw.m_pTexture != oshdw.m_pTexture) {
                continue;
            }
            // No need to check if this one was rendered (because of the batching)
            
            // Render polies of this shadow
            for (auto poly = ishdw.m_pPolyBunch; poly; poly = poly->m_pNext) {
                // 0x70841F: Calculate color
                uint8 r, g, b;
                CShadows::AffectColourWithLighting(
                    ishdw.m_nType,
                    ishdw.m_nDayNightIntensity,
                    ishdw.m_nRed, ishdw.m_nGreen, ishdw.m_nBlue,
                    r, g, b
                );

                const auto totalNoIdx = 3 * (poly->m_wNumVerts - 2); // Total no. of indices we'll use

                // 0x708432: Begin render buffer store
                RwIm3DVertex*    vtxIt{};
                RwImVertexIndex* vtxIdxIt{};
                RenderBuffer::StartStoring(
                    totalNoIdx,
                    poly->m_wNumVerts,
                    vtxIdxIt, vtxIt
                );

                // 0x70851D: Write vertices (`if` not necessary, it's part of the loop condition)
                const auto a = (uint8)((float)ishdw.m_nIntensity * (1.f - CWeather::Foggyness * 0.5f));
                for (auto i{ 0 }; i < poly->m_wNumVerts; i++, vtxIt++) {
                    const auto& pos = poly->m_avecPosn[i];
                    RwIm3DVertexSetPos(vtxIt, pos.x, pos.y, pos.z + 0.06f);
                    RwIm3DVertexSetRGBA(vtxIt, r, g, b, a);
                    RwIm3DVertexSetU(vtxIt, (float)poly->m_aU[i] / 200.f);
                    RwIm3DVertexSetV(vtxIt, (float)poly->m_aV[i] / 200.f);
                }

                // 0x7085BC: Write indices  (`if` not necessary, it's part of the loop condition)
                for (auto i = 0; i < totalNoIdx; i++) {
                    *vtxIdxIt++ = g_ShadowVertices[i];
                }
                
                // Finish storing 
                RenderBuffer::StopStoring();

                // Mark this as rendered
                ishdw.m_bRendered = true;
            }
        }

        // Render out this batch
        RenderBuffer::RenderStuffInBuffer();
    }

    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
}

static auto& aShadowPolyUVs    = StaticRef<std::array<CVector, 16>>(0xC4B6B8); // UVs of the polygon being clipped (ping-pong buffer 1)
static auto& aClippedPolyUVs   = StaticRef<std::array<CVector, 16>>(0xC4B730); // UVs of the clipped polygon       (ping-pong buffer 2)
static auto& aShadowPolyPosns  = StaticRef<std::array<CVector, 16>>(0xC4B7A8); // Positions of the polygon being clipped (ping-pong buffer 1)
static auto& aClippedPolyPosns = StaticRef<std::array<CVector, 16>>(0xC4B820); // Positions of the clipped polygon       (ping-pong buffer 2)
static auto& aShadowQuadCorners= StaticRef<std::array<CVector, 4>> (0xC4B898); // The shadow quad (in entity space)
static auto& nShadowScratchFlags = StaticRef<uint8>(0xC4B8C8);                 // Set to 0b111 on every call (unused elsewhere)

// 0x7086B0
void CShadows::CastShadowEntityXY(CEntity* entity, float conrerAX, float cornerAY, float cornerBX, float cornerBY, CVector* posn, float frontX, float frontY, float sideX, float sideY, int16 intensity, uint8 red, uint8 green, uint8 blue, float zDistance, float scale, CPolyBunch** ppPolyBunch, uint8* pDayNightIntensity, int32 shadowType) {
    nShadowScratchFlags |= 0b111;

    const auto colModel = CModelInfo::GetModelInfo(entity->m_nModelIndex)->m_pColModel;
    const auto colData = colModel->m_pColData;
    if (!colData || colData->m_nNumTriangles == 0) {
        return;
    }

    // Make sure the triangle planes are calculated
    CCollision::CalculateTrianglePlanes(colModel);

    const auto& m = entity->GetMatrix();

    // Transform the shadow (front/side) vectors and position into entity space
    // (by multiplying with the transpose of the rotation matrix)
    const auto front = CVector2D{
        frontX * m.GetRight().x   + frontY * m.GetRight().y,
        frontX * m.GetForward().x + frontY * m.GetForward().y
    };
    const auto side = CVector2D{
        sideX * m.GetRight().x   + sideY * m.GetRight().y,
        sideX * m.GetForward().x + sideY * m.GetForward().y
    };
    const auto worldDelta = *posn - m.GetPosition();
    // Rotate into entity space (dot with right/forward — verified by disasm 0x7088BC-0x7088CD)
    const auto localPos = CVector2D{
        worldDelta.x * m.GetRight().x + worldDelta.y * m.GetRight().y,
        worldDelta.x * m.GetForward().x + worldDelta.y * m.GetForward().y
    };

    // The 4 corners of the shadow quad (in entity space)
    aShadowQuadCorners = {
        CVector{ localPos + front - side },
        CVector{ localPos + front + side },
        CVector{ localPos - front + side },
        CVector{ localPos - front - side }
    };

    // Bounds of the quad (used to early-out triangles)
    const auto minX = std::min({ aShadowQuadCorners[0].x, aShadowQuadCorners[1].x, aShadowQuadCorners[2].x, aShadowQuadCorners[3].x });
    const auto maxX = std::max({ aShadowQuadCorners[0].x, aShadowQuadCorners[1].x, aShadowQuadCorners[2].x, aShadowQuadCorners[3].x });
    const auto minY = std::min({ aShadowQuadCorners[0].y, aShadowQuadCorners[1].y, aShadowQuadCorners[2].y, aShadowQuadCorners[3].y });
    const auto maxY = std::max({ aShadowQuadCorners[0].y, aShadowQuadCorners[1].y, aShadowQuadCorners[2].y, aShadowQuadCorners[3].y });

    // Light position (in entity space)
    const auto lightZ = posn->z - m.GetPosition().z;

    /*
     * Clip the shadow quad against the triangle (Sutherland-Hodgman-ish, 3 passes, one per edge).
     * This is done so that the resulting polygon is the part of the shadow that lies on the triangle.
     */
    const auto ClipAgainstEdge = [](
        const CVector* inPosns, const CVector* inUVs, int32 nIn,
        CVector* outPosns, CVector* outUVs,
        const CVector& P, const CVector& Q
    ) {
        const auto edge = CVector2D{ Q - P };

        // Returns the signed "distance" (2D cross product) of the point from the edge line P->Q.
        // <= 0 means the point is on the outside of the edge.
        const auto GetEdgeDist = [&](const CVector& pt) {
            return (pt.x - P.x) * edge.y - (pt.y - P.y) * edge.x;
        };
        // Emit the intersection of the edge line with the line (in[prev] -> in[curr])
        const auto EmitIntersection = [&](int32 prev, int32 curr, int32 nOut) {
            const auto tPrev = std::abs(GetEdgeDist(inPosns[prev]));
            const auto tCurr = std::abs(GetEdgeDist(inPosns[curr]));
            const auto s     = tPrev / (tCurr + tPrev); // Ratio of how close `curr` is to the edge line
            outPosns[nOut] = inPosns[prev] + (inPosns[curr] - inPosns[prev]) * s;
            outUVs[nOut]   = inUVs[prev]   + (inUVs[curr] - inUVs[prev]) * s;
        };

        enum class State { START, KEPT, DROPPED };
        auto state = State::START;

        auto nOut = 0;
        for (auto i = 0; i < nIn; i++) {
            const auto t = GetEdgeDist(inPosns[i]);
            if (t <= 0.0f) { // Outside this edge
                if (state == State::KEPT) { // Was inside -> emit the crossing point
                    EmitIntersection(i - 1, i, nOut++);
                }
                state = State::DROPPED;
            } else { // Inside
                if (state == State::START || state == State::KEPT) {
                    outPosns[nOut] = inPosns[i];
                    outUVs[nOut]   = inUVs[i];
                    nOut++;
                } else if (state == State::DROPPED) { // Was outside -> emit the crossing point and this vertex
                    EmitIntersection(i - 1, i, nOut++);
                    outPosns[nOut] = inPosns[i];
                    outUVs[nOut]   = inUVs[i];
                    nOut++;
                }
                state = State::KEPT;
            }
        }

        // Handle the closing edge (last -> first)
        if (const auto t0 = GetEdgeDist(inPosns[0]); (0.0f < t0 && state == State::DROPPED) || (t0 <= 0.0f && state == State::KEPT)) {
            EmitIntersection(nIn - 1, 0, nOut++);
        }

        return nOut;
    };

    const auto ProcessTriangle = [&](int32 triIdx) {
        const auto& triPlane = colData->m_pTrianglePlanes[triIdx];
        const auto normal = triPlane.GetNormal(); // Compressed with 1/4096 - 0x858BFC

        // Ignore near-vertical triangles (can't receive a shadow)
        if (!(0.1f < std::abs(normal.z))) { // 0x858B1C
            return;
        }

        const auto& tri = colData->m_pTriangles[triIdx];

        CVector vA, vB, vC;
        colData->GetTrianglePoint(vA, tri.vA);
        colData->GetTrianglePoint(vB, tri.vB);
        colData->GetTrianglePoint(vC, tri.vC);

        // Check if the triangle overlaps the shadow quad's bounds (in XY),
        // and it's Z is within (lightZ - zDistance, lightZ)
        if (!((minX < vA.x || minX < vB.x || minX < vC.x)
            && (vA.x < maxX || vB.x < maxX || vC.x < maxX)
            && (minY < vA.y || minY < vB.y || minY < vC.y)
            && (vA.y < maxY || vB.y < maxY || vC.y < maxY)
            && (vA.z < lightZ || vB.z < lightZ || vC.z < lightZ)
            && (lightZ - zDistance < vA.z || lightZ - zDistance < vB.z || lightZ - zDistance < vC.z))
        ) {
            return;
        }

        // Initialize the polygon to be clipped with the shadow quad's corners + their UVs
        aShadowPolyPosns[0] = aShadowQuadCorners[0];
        aShadowPolyPosns[1] = aShadowQuadCorners[1];
        aShadowPolyPosns[2] = aShadowQuadCorners[2];
        aShadowPolyPosns[3] = aShadowQuadCorners[3];
        aShadowPolyUVs = {
            CVector{ 0.0f, 0.0f, 0.0f },
            CVector{ 1.0f, 0.0f, 0.0f },
            CVector{ 1.0f, 1.0f, 0.0f },
            CVector{ 0.0f, 1.0f, 0.0f }
        };

        // Clip it against all 3 edges of the triangle
        const auto n1 = ClipAgainstEdge(aShadowPolyPosns.data(), aShadowPolyUVs.data(), 4, aClippedPolyPosns.data(), aClippedPolyUVs.data(), vA, vB);
        const auto n2 = ClipAgainstEdge(aClippedPolyPosns.data(), aClippedPolyUVs.data(), n1, aShadowPolyPosns.data(), aShadowPolyUVs.data(), vB, vC);
        const auto n3 = ClipAgainstEdge(aShadowPolyPosns.data(), aShadowPolyUVs.data(), n2, aClippedPolyPosns.data(), aClippedPolyUVs.data(), vC, vA);

        if (n3 < 3) { // Not a polygon
            return;
        }

        // Project the clipped polygon onto the triangle's plane (set the Z of each vertex)
        const auto planeDist = normal.Dot(vA);
        const auto zScale = -1.0f / normal.z; // 0x858C1C
        for (auto i = 0; i < n3; i++) {
            aClippedPolyPosns[i].z = ((normal.x * aClippedPolyPosns[i].x + normal.y * aClippedPolyPosns[i].y) - planeDist) * zScale;
        }

        // Transform the vertices back into world space
        for (auto i = 0; i < n3; i++) {
            const auto& local = aClippedPolyPosns[i];
            aClippedPolyPosns[i] = CVector{
                m.GetRight().x   * local.x + m.GetForward().x * local.y + m.GetPosition().x,
                m.GetRight().y   * local.x + m.GetForward().y * local.y + m.GetPosition().y,
                m.GetUp().z      * local.z + m.GetPosition().z
            };
        }

        // Save the (pre-baked) lighting of the triangle
        *pDayNightIntensity = tri.m_nLight.value;

        if (!ppPolyBunch) { // Store into the render buffer, and draw it right away
            const auto nIndices = 3 * n3 - 6; // Triangle fan indices

            RwImVertexIndex* idxIt{};
            RwIm3DVertex*    vtxIt{};
            RenderBuffer::StartStoring(nIndices, n3, idxIt, vtxIt); // Flushes the buffer if there's not enough space (0x708290)

            uint8 r, g, b;
            AffectColourWithLighting((eShadowType)shadowType, *pDayNightIntensity, red, green, blue, r, g, b); // 0x707850

            for (auto i = 0; i < n3; i++, vtxIt++) {
                const auto& pos = aClippedPolyPosns[i];
                const auto& uv  = aClippedPolyUVs[i];
                RwIm3DVertexSetPos(vtxIt, pos.x, pos.y, pos.z + 0.06f); // 0x859934
                RwIm3DVertexSetRGBA(vtxIt, r, g, b, (uint8)intensity);
                RwIm3DVertexSetU(vtxIt, scale * uv.x);
                RwIm3DVertexSetV(vtxIt, scale * uv.y);
            }

            // Copy the triangle fan indices (0, 1, 2, 0, 2, 3, ...)
            std::copy_n(g_ShadowVertices.begin(), nIndices, idxIt);

            RenderBuffer::StopStoring(); // 0x7077A0
        } else if (pEmptyBunchList) { // Store as a static shadow's poly bunch
            auto* const bunch = pEmptyBunchList;
            pEmptyBunchList = bunch->m_pNext;

            // Push it to the front of the list
            bunch->m_pNext = *ppPolyBunch;
            *ppPolyBunch = bunch;

            bunch->m_wNumVerts = (int16)n3;
            for (auto i = 0; i < n3; i++) {
                bunch->m_avecPosn[i] = aClippedPolyPosns[i];
                bunch->m_aU[i] = (uint8)(aClippedPolyUVs[i].x * 200.0f); // 0x858A48
                bunch->m_aV[i] = (uint8)(aClippedPolyUVs[i].y * 200.0f);
            }
        }
    };

    if (colData->bHasFaceGroups) { // The triangles are split into groups (with a bounding box each)
        for (const auto& group : colData->GetFaceGroups()) {
            // Check if the group's bounding box overlaps the shadow quad
            if (group.bb.m_vecMin.x < maxX && minX < group.bb.m_vecMax.x
                && group.bb.m_vecMin.y < maxY && minY < group.bb.m_vecMax.y
            ) {
                for (auto t = (int32)group.first; t <= (int32)group.last; t++) {
                    ProcessTriangle(t);
                }
            }
        }
    } else { // Process all triangles
        for (auto t = 0; t < colData->m_nNumTriangles; t++) {
            ProcessTriangle(t);
        }
    }
}
// 0x70A040
void CShadows::CastShadowEntityXYZ(CEntity* entity, CVector* posn, float frontX, float frontY, float sideX, float sideY, int16 intensity, uint8 red, uint8 green, uint8 blue, float zDistance, float scale, CPolyBunch** ppPolyBunch, CRealTimeShadow* realTimeShadow) {
    if (!realTimeShadow) {
        return;
    }

    RwCamera* const rwCam = realTimeShadow->m_camera.m_pRwCamera; // 0x7059E0 returns `&realTimeShadow->m_camera`

    auto* const colModel = CModelInfo::GetModelInfo(entity->m_nModelIndex)->m_pColModel;
    auto* const colData = colModel->m_pColData;
    if (!colData) {
        return;
    }

    // Make sure the triangle planes are calculated
    CCollision::CalculateTrianglePlanes(colModel);

    // Take the camera's frame matrix, and rotate it by -45 degrees around the X axis
    RwMatrix matCamFrame = RwCameraGetFrame(rwCam)->modelling;
    const RwV3d xAxis = { 1.0f, 0.0f, 0.0f };
    RwMatrixRotate(&matCamFrame, &xAxis, -45.0f, rwCOMBINEREPLACE); // 0x7F1FD0

    // Set up the projection parameters (used by `ShadowRenderTriangleCB`)
    _ProjectionParam param{};
    param.at = matCamFrame.at;
    RwMatrixCopy(&param.invMatrix, &matCamFrame); // 0x7F2070
    const RwV3d projScale = {
        -0.5f / (0.9f * rwCam->farPlane), // 0x858F40, 0x858C20
        1.0f / (rwCam->farPlane * 0.8f),  // 0x858C98
        -0.5f / (0.9f * rwCam->farPlane)
    };
    RwMatrixScale(&param.invMatrix, &projScale, rwCOMBINEPOSTCONCAT); // 0x7F22C0
    const RwV3d projTranslate = { 0.5f, 0.0f, 0.0f };
    RwMatrixTranslate(&param.invMatrix, &projTranslate, rwCOMBINEPOSTCONCAT); // 0x7F2450
    param.shadowValue  = (uint8)intensity;
    param.fade         = false;
    param.numIm3DBatch = 0;
    entity->GetMatrix().CopyToRwMatrix(&param.entityMatrix); // 0x59B8B0

    // Light position (in entity space)
    CVector lightPosEntity;
    {
        const auto lightPos = realTimeShadow->m_baseSphere.m_vecCenter
            + CVector{ frontX * -1.1f, frontY * -1.1f, -0.5f }; // 0x871654, 0x858B8C
        RwV3dTransformPoints(&lightPosEntity, &lightPos, 1, &param.entityMatrix); // 0x7EDD90
    }

    // Test which triangles are hit by the light (a small sphere at the light's position)
    CColSphere lightSphere;
    lightSphere.Set(2.0f, lightPosEntity, (eSurfaceType)0, 0, tColLighting{ 255 }); // 0x40FD10

    for (auto t = 0; t < colData->m_nNumTriangles; t++) {
        const auto& tri = colData->m_pTriangles[t];

        CVector verts[3];
        colData->GetTrianglePoint(verts[0], tri.vA);
        colData->GetTrianglePoint(verts[1], tri.vB);
        colData->GetTrianglePoint(verts[2], tri.vC);

        if (!CCollision::TestSphereTriangle(lightSphere, colData->m_pVertices, tri, colData->m_pTrianglePlanes[t])) { // 0x4165B0
            continue;
        }

        // Push the vertices out a little bit along the triangle's normal (to avoid z-fighting)
        const auto normal = colData->m_pTrianglePlanes[t].GetNormal();
        const auto offset = normal * 0.028f; // 0x872720
        verts[0] += offset;
        verts[1] += offset;
        verts[2] += offset;

        if (!ShadowRenderTriangleCB(const_cast<CVector*>(&normal), verts, &param)) { // 0x709CF0
            return;
        }
    }
}
// 0x70A470
template<typename PtrListType>
void CShadows::CastPlayerShadowSectorList(
    PtrListType& ptrList,
    float cornerAX,
    float cornerAY,
    float cornerBX,
    float cornerBY,
    CVector* posn,
    float frontX,
    float frontY,
    float sideX,
    float sideY,
    int16 intensity,
    uint8 red,
    uint8 green,
    uint8 blue,
    float zDistance,
    float scale,
    CPolyBunch** ppPolyBunch,
    uint8* pDayNightIntensity,
    int32 shadowType
) {
    const CRect shadowRect{
        cornerAX, cornerAY,
        cornerBX, cornerBY
    };
    for (auto* const entity : ptrList) {
        if (entity->IsScanCodeCurrent()) {
            continue;
        }
        entity->SetCurrentScanCode();

        if (!entity->m_bUsesCollision || entity->m_bDontCastShadowsOn) {
            continue;
        }

        if (!entity->IsInCurrentArea()) {
            continue;
        }

        // If slightly tilted, ignore
        if (entity->GetMatrix().GetUp().z <= 0.97f) {
            continue;
        }

        // 0x70A526
        if (!entity->GetBoundRect().OverlapsWith(shadowRect)) {
            continue;
        }

        // Quick Z height check of the bounding box
        const auto& cm         = entity->GetColModel();
        const auto  entityPosZ = entity->GetPosition().z;
        if (cm->m_boundBox.m_vecMax.z + entityPosZ <= posn->z - zDistance) {
            continue;
        }
        if (cm->m_boundBox.m_vecMin.z + entityPosZ >= posn->z) {
            continue;
        }

        CastShadowEntityXY(
            entity,
            cornerAX,
            cornerAY,
            cornerBX,
            cornerBY,
            posn,
            frontX,
            frontY,
            sideX,
            sideY,
            (uint8)intensity,
            red,
            green,
            blue,
            zDistance,
            scale,
            ppPolyBunch,
            pDayNightIntensity,
            shadowType
        );
    }
}

// 0x70A630
template<typename PtrListType>
void CShadows::CastShadowSectorList(PtrListType& ptrList, float conrerAX, float cornerAY, float cornerBX, float cornerBY, CVector* posn, float frontX, float frontY, float sideX, float sideY, int16 intensity, uint8 red, uint8 green, uint8 blue, float zDistance, float scale, CPolyBunch** ppPolyBunch, uint8* pDayNightIntensity, int32 shadowType) {
    // Nearly identical to `CastPlayerShadowSectorList`, the difference is 1 check is missing... :D
    ((void(__cdecl*)(PtrListType&, float, float, float, float, CVector*, float, float, float, float, int16, uint8, uint8, uint8, float, float, CPolyBunch**, uint8*, int32))0x70A630)(ptrList, conrerAX, cornerAY, cornerBX, cornerBY, posn, frontX, frontY, sideX, sideY, (uint8)intensity, red, green, blue, zDistance, scale, ppPolyBunch, pDayNightIntensity, shadowType);
}

// 0x70A7E0
template<typename PtrListType>
void CShadows::CastRealTimeShadowSectorList(PtrListType& ptrList, float conrerAX, float cornerAY, float cornerBX, float cornerBY, CVector* posn, float frontX, float frontY, float sideX, float sideY, int16 intensity, uint8 red, uint8 green, uint8 blue, float zDistance, float scale, CPolyBunch** ppPolyBunch, CRealTimeShadow* realTimeShadow, uint8* pDayNightIntensity) {
    ((void(__cdecl*)(PtrListType&, float, float, float, float, CVector*, float, float, float, float, int16, uint8, uint8, uint8, float, float, CPolyBunch**, CRealTimeShadow*, uint8*))0x70A7E0)(ptrList, conrerAX, cornerAY, cornerBX, cornerBY, posn, frontX, frontY, sideX, sideY, (uint8)intensity, red, green, blue, zDistance, scale, ppPolyBunch, realTimeShadow, pDayNightIntensity);
}

// 0x70A960
void CShadows::RenderStoredShadows() {
    // Originally renderstates are still set even though there are no shadows to be rendered
    // I don't think this is necessary, so we early-out here
    if (!CShadows::ShadowsStoredToBeRendered) {
        return;
    }

    RenderBuffer::ClearRenderBuffer();

    for (auto i = 0; i < ShadowsStoredToBeRendered; i++) {
        asShadowsStored[i].m_bAlreadyRenderedInBatch = false;
    }

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,         RWRSTATE(rwRENDERSTATENARENDERSTATE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,          RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,    RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,            RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS,       RWRSTATE(rwTEXTUREADDRESSCLAMP));
    RwRenderStateSet(rwRENDERSTATECULLMODE,             RWRSTATE(rwCULLMODECULLNONE));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,        RWRSTATE(rwFILTERLINEAR));

    const auto GetShadowRect = [](CRegisteredShadow& shdw) {
        return CRect{
            shdw.m_vecPosn,
            shdw.m_vecPosn + abs(shdw.m_Side) + abs(shdw.m_Front)
        };
    };

    for (auto o = 0; o < ShadowsStoredToBeRendered; o++) {
        auto& oshdw = asShadowsStored[o];

        // Setup additional render states for this shadow (and others in the batch below)
        SetRenderModeForShadowType(oshdw.m_nType);
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(RwTextureGetRaster(oshdw.m_pTexture)));

        //
        // Render onto ground (If not already)
        //
        if (!oshdw.m_bAlreadyRenderedInBatch) {
            // We do a batched rendering here:
            // All shadows of the same type and texture are rendered together to save on drawcalls

            for (auto i = o; i < ShadowsStoredToBeRendered; i++) {
                auto& ishdw = asShadowsStored[i];

                if (&ishdw != &oshdw) {
                    if (ishdw.m_nType != oshdw.m_nType) {
                        continue;
                    }
                    if (ishdw.m_pTexture != oshdw.m_pTexture) {
                        continue;
                    }
                }

                CWorld::AdvanceCurrentScanCode();

                const auto ishdwRect = GetShadowRect(ishdw);

                // Do shadow casting
                CWorld::IterateSectorsOverlappedByRect(
                    ishdwRect,
                    [&] (int32 x, int32 y) -> bool {
                        auto& sector = CWorld::GetSector(x, y);

                        if (const auto rtshdw = ishdw.m_pRTShadow) {
                            CastRealTimeShadowSectorList(
                                sector.Buildings,
                                ishdwRect.left, ishdwRect.bottom,
                                ishdwRect.right, ishdwRect.top,
                                &ishdw.m_vecPosn,
                                ishdw.m_Front.x, ishdw.m_Front.y,
                                ishdw.m_Side.x, ishdw.m_Side.y,
                                ishdw.m_nIntensity,
                                ishdw.m_nRed, ishdw.m_nGreen, ishdw.m_nBlue,
                                ishdw.m_fZDistance,
                                ishdw.m_fScale,
                                nullptr,
                                rtshdw,
                                nullptr // Unused
                            );
                        } else {
                            uint8 unused{};
                            if (ishdw.m_bDrawOnBuildings) {
                                CastShadowSectorList(
                                    sector.Buildings,
                                    ishdwRect.left, ishdwRect.bottom,
                                    ishdwRect.right, ishdwRect.top,
                                    &ishdw.m_vecPosn,
                                    ishdw.m_Front.x, ishdw.m_Front.y,
                                    ishdw.m_Side.x, ishdw.m_Side.y,
                                    ishdw.m_nIntensity,
                                    ishdw.m_nRed, ishdw.m_nGreen, ishdw.m_nBlue,
                                    ishdw.m_fZDistance,
                                    ishdw.m_fScale,
                                    nullptr,
                                    &unused,
                                    ishdw.m_nType
                                );
                            } else {
                                CastPlayerShadowSectorList(
                                    sector.Buildings,
                                    ishdwRect.left, ishdwRect.bottom,
                                    ishdwRect.right, ishdwRect.top,
                                    &ishdw.m_vecPosn,
                                    ishdw.m_Front.x, ishdw.m_Front.y,
                                    ishdw.m_Side.x, ishdw.m_Side.y,
                                    ishdw.m_nIntensity,
                                    ishdw.m_nRed, ishdw.m_nGreen, ishdw.m_nBlue,
                                    ishdw.m_fZDistance,
                                    ishdw.m_fScale,
                                    nullptr,
                                    &unused,
                                    ishdw.m_nType
                                );
                            }
                        }
                        return true; // Inisde lambda -> Continue sector loop
                    }
                );

                // Mark this as rendered
                ishdw.m_bAlreadyRenderedInBatch = true;
            }

            // Render out shadows (Can't batch together with next iteration, as renderstates change)
            RenderBuffer::RenderStuffInBuffer();
        }
        
        // Render onto water (If needed)
        if (oshdw.m_bDrawOnWater) {  
            float waterLevelZ{}, bigWavesZ{}, smallWavesZ{};
            if (!CWaterLevel::GetWaterLevelNoWaves(oshdw.m_vecPosn, &waterLevelZ, &bigWavesZ, &smallWavesZ)) {
                continue;
            }

            // Shadow under water?
            if (waterLevelZ >= oshdw.m_vecPosn.z) {
                continue;
            }

            // Clear the buffer as this is a new render pass (separate from the other)
            RenderBuffer::ClearRenderBuffer();

            // The smaller this value the detailed the shadows, but also (exponentially?) slower
            // This value basically represents the side of a square that is projected onto the 
            const auto STEP_SIZE = 2;

            const auto shdwSideMagSq = oshdw.m_Side.SquaredMagnitude();
            const auto shdwFrontMagSq = oshdw.m_Front.SquaredMagnitude();

            const auto oshdwRect = GetShadowRect(oshdw);

            // Iterate shadow sectors (Using `CWorld::IterateSectors` for convenience)
            CWorld::IterateSectors(
                (int32)(std::floor(oshdwRect.left / (float)STEP_SIZE)),
                (int32)(std::floor(oshdwRect.top / (float)STEP_SIZE)),
                (int32)(std::ceil(oshdwRect.right / (float)STEP_SIZE)),
                (int32)(std::ceil(oshdwRect.bottom / (float)STEP_SIZE)),
                [&](int32 ox, int32 oy) {
                    // 0x70B000 - 0x70B0DA: Start storing 
                    RwIm3DVertex* vtxIt{};
                    RwImVertexIndex* vtxIdxIt{};
                    RenderBuffer::StartStoring(6, 4, vtxIdxIt, vtxIt);

                    // Function to process 1 vertex (out of the 4)
                    const auto ProcessOneVertex = [&] (int32 x, int32 y) {
                        // The x, y passed in are sector coords, so make them into world coords
                        x *= STEP_SIZE;
                        y *= STEP_SIZE;

                        const auto currPos = CVector2D{ (float)x, (float)y };

                        // Set color
                        RwIm3DVertexSetRGBA(vtxIt, oshdw.m_nRed, oshdw.m_nGreen, oshdw.m_nBlue, (uint8)((float)oshdw.m_nIntensity * 0.6f));

                        // Set texture coords (UV)
                        const auto CalcTexCoord  = [
                            currPosToShdw = CVector2D{ currPos - oshdw.m_vecPosn }
                        ](CVector2D v, float vmagsq) {
                            return (currPosToShdw.Dot(v) / vmagsq + 1.f) * 0.5f;
                        };
                        RwIm3DVertexSetU(vtxIt, CalcTexCoord(oshdw.m_Side, shdwSideMagSq));
                        RwIm3DVertexSetV(vtxIt, CalcTexCoord(oshdw.m_Front, shdwFrontMagSq));

                        // Set position
                        RwIm3DVertexSetPos(
                            vtxIt,
                            currPos.x,
                            currPos.y,
                            CWaterLevel::CalculateWavesOnlyForCoordinate2_Direct(x, y, waterLevelZ, bigWavesZ, smallWavesZ) + 0.06f // Z pos of the wave at the given coord
                        );
                    };

                    // Process 4 vertices of this square
                    ProcessOneVertex(ox,     oy    ); // top left
                    ProcessOneVertex(ox + 1, oy    ); // top right
                    ProcessOneVertex(ox,     oy + 1); // bottom left
                    ProcessOneVertex(ox + 1, oy + 1); // bottom right

                    // Copy vertices into index buffer
                    rng::copy(std::to_array({ 0, 1, 2, 1, 3, 2 }), vtxIdxIt);

                    // And we're done with this one... onto the next!
                    RenderBuffer::StopStoring();

                    // Continue iteration...
                    return true;
                }
            );

            // Render it all
            RenderBuffer::RenderStuffInBuffer();
        }
    }

    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS,    RWRSTATE(rwTEXTUREADDRESSWRAP));
    RwRenderStateSet(rwRENDERSTATECULLMODE,          RWRSTATE(rwCULLMODECULLBACK));

    ShadowsStoredToBeRendered = 0;
}

// 0x70B730
void CShadows::GeneratePolysForStaticShadow(int16 staticShadowIndex) {
    auto& shdw = aStaticShadows[staticShadowIndex];

    // Bounds of the shadow (in world space)
    const auto minX = shdw.m_vecPosn.x - std::abs(shdw.m_fFrontX) - std::abs(shdw.m_fSideX);
    const auto maxX = shdw.m_vecPosn.x + std::abs(shdw.m_fFrontX) + std::abs(shdw.m_fSideX);
    const auto minY = shdw.m_vecPosn.y - std::abs(shdw.m_fFrontY) - std::abs(shdw.m_fSideY);
    const auto maxY = shdw.m_vecPosn.y + std::abs(shdw.m_fFrontY) + std::abs(shdw.m_fSideY);

    // World is divided into 120 x 120 sectors of 50 units (0.02 = 1/50, +60 centers -3000..3000)
    const auto startSectorX = std::max(0, (int32)std::floor(minX * 0.02f + 60.0f));
    const auto startSectorY = std::max(0, (int32)std::floor(minY * 0.02f + 60.0f));
    const auto endSectorX   = std::min(119, (int32)std::floor(maxX * 0.02f + 60.0f));
    const auto endSectorY   = std::min(119, (int32)std::floor(maxY * 0.02f + 60.0f));

    CWorld::AdvanceCurrentScanCode();

    for (auto sectorY = startSectorY; sectorY <= endSectorY; sectorY++) {
        for (auto sectorX = startSectorX; sectorX <= endSectorX; sectorX++) {
            CShadows::CastPlayerShadowSectorList(
                CWorld::GetSector(sectorX, sectorY).Buildings,
                minX, minY,
                maxX, maxY,
                &shdw.m_vecPosn,
                shdw.m_fFrontX, shdw.m_fFrontY,
                shdw.m_fSideX, shdw.m_fSideY,
                0,
                0, 0, 0,
                shdw.m_fZDistance,
                shdw.m_fScale,
                &shdw.m_pPolyBunch,
                &shdw.m_nDayNightIntensity,
                0
            );
        }
    }
}
// 0x70BA00
bool CShadows::StoreStaticShadow(uint32 id, eShadowType type, RwTexture* texture, const CVector& posn, float frontX, float frontY, float sideX, float sideY, int16 intensity, uint8 red, uint8 green, uint8 blue, float zDistane, float scale, float drawDistance, bool temporaryShadow, float upDistance) {
    const auto& camPos = TheCamera.GetPosition();
    const auto shdwToCamDist2DSq = sq(posn.x - camPos.x) + sq(posn.y - camPos.y);

    const auto bInfiniteDrawDistance = drawDistance == 0.0f; // 0x858B50
    if (shdwToCamDist2DSq < sq(drawDistance)) {
        // If in the outer quarter of the draw distance, fade the shadow out
        if (!bInfiniteDrawDistance && drawDistance * 0.75f <= std::sqrt(shdwToCamDist2DSq)) {
            const auto fade = 1.0f - (std::sqrt(shdwToCamDist2DSq) - drawDistance * 0.75f) * (4.0f / drawDistance);
            intensity = (int16)((float)intensity * fade);
            red       = (uint8)((float)red   * fade);
            green     = (uint8)((float)green * fade);
            blue      = (uint8)((float)blue  * fade);
        }
    } else if (!bInfiniteDrawDistance) { // Out of draw distance
        return true;
    }

    int32 slotID = -1;

    // Check if this shadow is already stored (search by id)
    for (auto i = 0u; i < MAX_STATIC_SHADOWS; i++) {
        auto& shdw = aStaticShadows[i];

        if (shdw.m_nId != id || !shdw.m_pPolyBunch) {
            continue;
        }

        // Check if it's basically the same shadow (=> just update it in-place)
        if ((std::abs(posn.x - shdw.m_vecPosn.x) < upDistance && std::abs(posn.y - shdw.m_vecPosn.y) < upDistance)
            || (std::abs(posn.x - shdw.m_vecPosn.x) < 0.05f // 0x858C28
                && std::abs(posn.y - shdw.m_vecPosn.y) < 0.05f
                && std::abs(posn.z - shdw.m_vecPosn.z) < 2.0f // 0x858CA0
                && zDistane == shdw.m_fZDistance
                && frontX == shdw.m_fFrontX && frontY == shdw.m_fFrontY
                && sideX == shdw.m_fSideX && sideY == shdw.m_fSideY)
        ) {
            shdw.m_pTexture         = texture;
            shdw.m_nType            = type;
            shdw.m_nRed             = red;
            shdw.m_nIntensity       = intensity;
            shdw.m_nBlue            = blue;
            shdw.m_nGreen           = green;
            shdw.m_fScale           = scale;
            shdw.m_fZDistance       = zDistane;
            shdw.m_nTimeCreated     = CTimer::GetTimeInMS();
            shdw.m_bJustCreated     = true;
            shdw.m_bTemporaryShadow = temporaryShadow;
            return true;
        }

        // Otherwise free the old polies, and overwrite this slot
        shdw.Free();
        slotID = i;
        break;
    }

    if (slotID == -1) {
        // Find a slot with no polies in it
        for (auto i = 0u; i < MAX_STATIC_SHADOWS; i++) {
            if (!aStaticShadows[i].m_pPolyBunch) {
                slotID = i;
                break;
            }
        }

        if (slotID == -1) { // No free slot
            return true;
        }
    }

    auto& shdw = aStaticShadows[slotID];
    shdw.m_nId             = id;
    shdw.m_nType           = type;
    shdw.m_pTexture        = texture;
    shdw.m_nIntensity      = intensity;
    shdw.m_nRed            = red;
    shdw.m_nGreen          = green;
    shdw.m_nBlue           = blue;
    shdw.m_fScale          = scale;
    shdw.m_fZDistance      = zDistane;
    shdw.m_nTimeCreated    = CTimer::GetTimeInMS();
    shdw.m_vecPosn         = posn;
    shdw.m_fFrontX         = frontX;
    shdw.m_fFrontY         = frontY;
    shdw.m_fSideX          = sideX;
    shdw.m_fSideY          = sideY;
    shdw.m_bJustCreated    = true;
    shdw.m_bTemporaryShadow = temporaryShadow;

    GeneratePolysForStaticShadow((int16)slotID);

    // If no polies were generated, the shadow failed to be stored
    return shdw.m_pPolyBunch != nullptr;
}
// 0x70BDA0
void CShadows::StoreShadowForVehicle(CVehicle* vehicle, VEH_SHD_TYPE vehShadowType) {
    if (CStencilShadows::GraphicsHighQuality() || CTimeCycle::m_CurrentColours.m_nShadowStrength == 0) {
        return;
    }

    auto zDistance = 4.5f;

    const auto& vehPos = vehicle->GetPosition();
    const auto& camPos = TheCamera.GetPosition();

    auto camToVehDist2DSq = sq(vehPos.x - camPos.x) + sq(vehPos.y - camPos.y);
    if (CCutsceneMgr::ms_running) {
        camToVehDist2DSq /= sq(TheCamera.m_fLODDistMultiplier) * 4.0f; // 0x858B90
    }

    float maxDist;
    switch (vehShadowType) {
    case VEH_SHD_HELI:
    case VEH_SHD_PLANE:
    case VEH_SHD_RC:
        if (camToVehDist2DSq >= 20736.0f) { // 0x87273C (144^2)
            return;
        }
        maxDist = 144.0f; // 0x86C824
        break;
    case VEH_SHD_BIG_PLANE:
        if (camToVehDist2DSq >= 82944.0f) { // 0x872738 (288^2)
            return;
        }
        maxDist = 288.0f; // 0x872734
        break;
    default:
        if (camToVehDist2DSq >= 324.0f) { // 0x872730 (18^2)
            return;
        }
        maxDist = 18.0f; // 0x859008
        break;
    }

    // Fade out if far away
    const auto camToVehDist2D = std::sqrt(camToVehDist2DSq);
    auto intensity = (uint16)CTimeCycle::m_CurrentColours.m_nShadowStrength;
    if (maxDist * 0.75f <= camToVehDist2D) {
        intensity = (uint16)((1.0f - (camToVehDist2D - maxDist * 0.75f) / (maxDist * 0.25f)) * (float)CTimeCycle::m_CurrentColours.m_nShadowStrength);
    }

    const auto& bb = vehicle->GetColModel()->GetBoundingBox();

    // Shadow size (from the vehicle's bounding box)
    auto width  = bb.m_vecMax.x - bb.m_vecMin.x;
    auto length = bb.m_vecMax.y - bb.m_vecMin.y;
    auto sizeMultY = 1.0f;

    // Some vehicles need adjustments to their shadow size
    switch ((eModelID)vehicle->m_nModelIndex) {
    case MODEL_LEVIATHN:
    case MODEL_HUNTER:
    case MODEL_SEASPAR:
    case MODEL_SPARROW:
    case MODEL_MAVERICK:
    case MODEL_POLMAV:
        sizeMultY = 0.5f;
        width *= 3.0f;   // 0x858B3C
        length *= 1.4f;  // 0x859994
        break;
    case MODEL_PIZZABOY:
    case MODEL_PCJ600:
    case MODEL_FAGGIO:
    case MODEL_BMX:
    case MODEL_BIKE:
    case MODEL_MTBIKE:
    case MODEL_FCR900:
    case MODEL_NRG500:
        // All bikes use a thinner (in Y) shadow, that's stretched out
        sizeMultY = 0.05f;
        length *= 1.2f; // 0x858F08
        break;
    case MODEL_FREEWAY:
    case MODEL_SANCHEZ:
    case MODEL_COPBIKE:
        sizeMultY = 0.03f;
        length *= 1.5f; // 0x858CE8
        break;
    case MODEL_RCRAIDER:
    case MODEL_RCGOBLIN:
        sizeMultY = 0.2f;
        length *= 1.5f;
        width *= 2.0f;
        break;
    case MODEL_VORTEX:
        return; // Hovercraft - casts no shadow
    case UNLOAD_MODEL: // -2
        width *= 0.4f;  // 0x858EE8
        length *= 0.9f; // 0x858C20
        break;
    default:
        break;
    }

    // Move the shadow a little behind the vehicle's center
    const auto& m = vehicle->GetMatrix();
    auto shdwPos = vehPos - CVector{CVector2D{m.GetForward()} * ((length * 0.5f - bb.m_vecMax.y) * sizeMultY)};

    // Texture + final per-type adjustments
    RwTexture* texture;
    switch (vehShadowType) {
    case VEH_SHD_CAR:
        texture = gpShadowCarTex;
        break;
    case VEH_SHD_BIKE: {
        texture = gpShadowBikeTex;

        // The more the bike is leaned, the wider the shadow
        auto leanMult = std::abs(vehicle->GetRideAnimData()->LeanAngle) * 5.092958f + 1.0f; // CBike+0x648 == m_RideAnimData.LeanAngle; 0x8722D8
        if (vehicle->GetStatus() != STATUS_ABANDONED || std::abs(m.GetRight().z) <= 0.6f) { // 0x858CC8
            width *= leanMult;
        } else { // Abandoned (fallen over) - even wider
            width *= std::abs(m.GetRight().z) * 4.0f + leanMult;
        }
        break;
    }
    case VEH_SHD_HELI:
        texture = gpShadowHeliTex;
        if (vehicle == FindPlayerVehicle()) {
            zDistance = 50.0f;
        }
        break;
    case VEH_SHD_PLANE:
    case VEH_SHD_BIG_PLANE:
        texture = gpShadowBaronTex;
        intensity = (uint16)CTimeCycle::m_CurrentColours.m_nShadowStrength;
        if (vehicle == FindPlayerVehicle()) {
            zDistance = 50.0f;
        }
        break;
    case VEH_SHD_RC:
        texture = gpShadowBaronTex;
        length *= 1.5f;
        width *= 2.2f; // 0x87272C
        break;
    default:
        texture = gpShadowCarTex;
        break;
    }

    // The direction of the sun light relative to the vehicle
    auto front = CVector2D{m.GetForward()};
    const auto fwd3D = CVector{m.GetForward()};
    const auto zAxis = CVector{0.0f, 0.0f, 1.0f};
    CVector side3d;
    side3d = CrossProduct(fwd3D, zAxis); // 0x59C730 - equivalent to (fwd.y, -fwd.x, 0)
    auto side = CVector2D{side3d};
    if (const auto mag = side.Magnitude(); mag < 0.5f) { // 0x858B8C
        side = side / mag * 0.5f;
    }
    if (m.GetUp().z < 0.0f) {
        side = -side;
    }

    if (vehShadowType == VEH_SHD_BIKE) {
        if (0.6f < std::abs(m.GetRight().z)) { // 0x858CC8
            side = CVector2D{m.GetUp()};
        }
    } else if (vehShadowType == VEH_SHD_HELI) {
        if (0.57f < std::abs(m.GetRight().z)) { // 0x872728
            side = CVector2D{m.GetUp()};
        }
        if (0.57f < std::abs(m.GetForward().z)) {
            front = CVector2D{m.GetUp()};
        }
    }

    const auto bDrawOnWaterAndBuildings = vehicle->vehicleFlags.bIsRCVehicle // 0x70C26E - sign test of the flags byte at 0x42C (bit 7)
        || vehicle == FindPlayerVehicle();

    if (vehicle->GetMoveSpeed().Magnitude() * CTimer::ms_fTimeStep <= 0.1f && !bDrawOnWaterAndBuildings) { // 0x858B1C
        // Standing still (and not the player's vehicle) - store it as a static shadow
        width *= 0.5f;
        length *= 0.5f;
        if (m.GetUp().z <= 0.0f) {
            StoreStaticShadow(
                (uint32)(uintptr)vehicle + 1,
                SHADOW_DEFAULT,
                texture,
                shdwPos,
                length * front.x, front.y * length,
                -(width * side.x), -(width * side.y),
                (uint8)intensity,
                (uint8)intensity, (uint8)intensity, (uint8)intensity,
                4.5f,
                1.0f,
                0.0f,
                false,
                0.1f // 0x3DCCCCD - the "same shadow" tolerance used by `StoreStaticShadow`
            );
        } else {
            StoreStaticShadow(
                (uint32)(uintptr)vehicle + 1,
                SHADOW_DEFAULT,
                texture,
                shdwPos,
                length * front.x, front.y * length,
                width * side.x, width * side.y,
                (uint8)intensity,
                (uint8)intensity, (uint8)intensity, (uint8)intensity,
                4.5f,
                1.0f,
                0.0f,
                false,
                0.1f
            );
        }
        return;
    }

    width *= 0.5f;
    length *= 0.5f;
    if (m.GetUp().z <= 0.0f) {
        StoreShadowToBeRendered(
            SHADOW_DEFAULT,
            texture,
            shdwPos,
            length * front.x, front.y * length,
            -(width * side.x), -(width * side.y),
            (uint8)intensity,
            (uint8)intensity, (uint8)intensity, (uint8)intensity,
            zDistance,
            bDrawOnWaterAndBuildings,
            1.0f,
            nullptr,
            bDrawOnWaterAndBuildings
        );
    } else {
        StoreShadowToBeRendered(
            SHADOW_DEFAULT,
            texture,
            shdwPos,
            length * front.x, front.y * length,
            width * side.x, width * side.y,
            (uint8)intensity,
            (uint8)intensity, (uint8)intensity, (uint8)intensity,
            zDistance,
            bDrawOnWaterAndBuildings,
            1.0f,
            nullptr,
            bDrawOnWaterAndBuildings
        );
    }
}

// 0x70C500
void CShadows::StoreCarLightShadow(CVehicle* vehicle, int32 id, RwTexture* texture, const CVector& posn, float frontX, float frontY, float sideX, float sideY, uint8 red, uint8 green, uint8 blue, float maxViewAngleCosine) {
    // Maximum distance (from camera to `posn`) after which shadows aren't stored (and rendered)
    constexpr auto MAX_CAM_TO_LIGHT_DIST = 27.f;

    if ([] { // Maybe ignore camera distance?
        switch (CCamera::GetActiveCamera().m_nMode) {
        case MODE_TOPDOWN:
        case MODE_TOP_DOWN_PED:
            return false;
        }

        return !CCutsceneMgr::IsRunning();
    }()) {
        const auto shdwToCam2D       = CVector2D{ TheCamera.GetPosition() - posn };
        const auto shdwToCamDist2DSq = shdwToCam2D.SquaredMagnitude();

        if (shdwToCamDist2DSq >= sq(MAX_CAM_TO_LIGHT_DIST)) {
            return;
        }

        // Check if the camera is facing the lights closely (in which case the camera can't see the shadow)
        if (shdwToCam2D.Dot(TheCamera.GetFrontNormal2D()) > maxViewAngleCosine) {
            return;
        }

        // If far enough from the camera, start fading out
        if (const auto dist = std::sqrt(shdwToCamDist2DSq); dist >= MAX_CAM_TO_LIGHT_DIST * 0.75f) {
            const auto t = 1.f - invLerp(MAX_CAM_TO_LIGHT_DIST * (2.f / 3.f), MAX_CAM_TO_LIGHT_DIST, dist);
            red   = (uint8)((float)red * t);
            green = (uint8)((float)green * t);
            blue  = (uint8)((float)blue * t);
        }
    }

    const auto isPlyrVeh = FindPlayerVehicle() == vehicle;
    if (isPlyrVeh || vehicle->GetMoveSpeed().Magnitude() * CTimer::GetTimeStep() >= 0.4f) {
        StoreShadowToBeRendered(
            SHADOW_ADDITIVE,
            texture,
            posn,
            frontX, frontY,
            sideX, sideY,
            128,
            red, green, blue,
            6.f,
            false,
            1.f,
            nullptr,
            isPlyrVeh || g_fx.GetFxQuality() >= FX_QUALITY_VERY_HIGH // NOTSA: At higher FX quality draw all vehicles's shadows on buildings too
        );
    } else {
        StoreStaticShadow(
            reinterpret_cast<uint32>(vehicle) + id,
            SHADOW_ADDITIVE,
            texture,
            posn,
            frontX, frontY,
            sideX, sideY,
            128,
            red, green, blue,
            6.f,
            1.f,
            0.f,
            false,
            0.4f
        );
    }
}

// 0x70C750
void CShadows::StoreShadowForPole(CEntity* entity, float offsetX, float offsetY, float offsetZ, float poleHeight, float poleWidth, uint32 localId) {
    if (GraphicsHighQuality() || !CTimeCycle::m_CurrentColours.m_nPoleShadowStrength) {
        return;
    }

    const auto& mat = entity->GetMatrix();

    if (mat.GetUp().z < .5f) { // More than 45 deg tilted
        return;
    }

    const auto intensity = 2.f * (mat.GetUp().z - 0.5f) * CTimeCycle::m_CurrentColours.m_nPoleShadowStrength;

    const auto front     = CVector2D{ CTimeCycle::GetVectorToSun() } * (-poleHeight / 2.f);
    const auto right     = CVector2D{ CTimeCycle::GetShadowSide() } * poleWidth;

    StoreStaticShadow(
        reinterpret_cast<uint32>(entity->GetLod()) + localId + 3,
        SHADOW_DEFAULT,
        gpPostShadowTex,
        mat.GetPosition() + CVector{ front, 0.f } + CVector{
            offsetX * mat.GetRight().x + offsetY * mat.GetForward().x, // Simplified matrix transform (Ignoring the Z axis)
            offsetX * mat.GetRight().y + offsetY * mat.GetForward().y, // >^^^
            offsetZ
        },
        front.x, front.y,
        right.x, right.y,
        2 * (int16)intensity / 3,
        0, 0, 0,
        9.f,
        1.f,
        40.f,
        false,
        0.f
    );
}

// 0x70CCB0
void CShadows::RenderIndicatorShadow(
    uint32 id,
    eShadowType,
    RwTexture*,
    const CVector& posn,
    float frontX, float frontY,
    float sideX, float sideY,
    int16 /*intensity*/
) {
    const auto size = std::max(frontX, -sideX);
    for (auto mult : { 0.8f, 0.9f, 1.0f }) {
        CVector markerPos = posn;
        C3dMarkers::PlaceMarkerSet(id, MARKER3D_CYLINDER, markerPos, size * mult, 255, 0, 0, 255u, 2048, 0.2f, 0);
    }
}

// 0x709CF0
#ifdef _MSC_VER
CVector* ShadowRenderTriangleCB(CVector* normal, CVector* trianglePos, _ProjectionParam* param) {
    CVector* result = nullptr;
    __asm mov eax, normal
    __asm mov ebx, trianglePos
    __asm mov edi, param
    __asm mov ecx, 0x709CF0
    __asm call ecx
    __asm mov result, eax
    return result;
}
#endif
