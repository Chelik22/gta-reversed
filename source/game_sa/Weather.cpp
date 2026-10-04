/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "eWeatherType.h"
#include "game_sa/Data/Weather.def"
#include "PostEffects.h"

// 0x8CCF30
std::array<float, 16> CWeather::saTreeWindOffsets = { 1.0f, 0.5f, 0.2f, 0.7f, 0.4f, 1.0f, 0.5f, 0.3f, 0.2f, 0.1f, 0.7f, 0.6f, 0.3f, 1.0f, 0.5f, 0.2f };

/// 0x8CCF70
std::array<float, 32> CWeather::saBannerWindOffsets = { 0.0f, 0.3f, 0.6f, 0.85f, 0.99f, 0.97f, 0.65f, 0.15f, -0.1f, 0.0f, 0.35f, 0.57f, 0.55f, 0.35f, 0.45f, 0.67f, 0.73f, 0.45f, 0.25f, 0.35f, 0.35f, 0.11f, 0.13f, 0.21f, 0.28f, 0.28f, 0.22f, 0.1f, 0.0f, -0.1f, -0.17f, -0.12f };

// NOTE: Globals used by `Update()` that are not `CWeather` class members (real addresses).
namespace {
    // 0x8D6078 - Minute of the last heat-haze fade step
    auto& HeatHazeLastMinute = StaticRef<int32>(0x8D6078);
    // 0xC81448 - Heat haze fade value (eased towards 0/1 based on time of day)
    auto& HeatHazeFade = StaticRef<float>(0xC81448);
    // 0xB7C540 - Underwater-fog animation counter (incremented elsewhere)
    auto& WaterFogCounter = StaticRef<int32>(0xB7C540);
    // 0xC81444 - Eased value towards `WaterFogCounter * 0.01f`
    auto& WaterFogTransitionValue = StaticRef<float>(0xC81444);
    // 0xC81440 - Whether the water fog transition reached its maximum
    auto& bWaterFogTransitionDone = StaticRef<bool>(0xC81440);
} // namespace

// 0x8D5E50 - Wind strength per weather type (indexed by `eWeatherType`, 20 entries; region extends to 0x8D5EA8)
static auto& WindStrengthPerWeatherType = StaticRef<std::array<float, 20>>(0x8D5E50);
// 0x8D5FF4 - Wind direction modulation table (17 entries; indexed 0..16)
static auto& WindDirTable2 = StaticRef<std::array<float, 17>>(0x8D5FF4);
// 0x8D6038 - Wind direction table (16 entries; always indexed `& 0xF`)
static auto& WindDirTable = StaticRef<std::array<float, 16>>(0x8D6038);


void CWeather::InjectHooks() {
    RH_ScopedClass(CWeather);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x72A480);
    RH_ScopedInstall(AddRain, 0x72A9A0, { .reversed = false }); // TRIAGE: user reports whole-screen flicker
    RH_ScopedInstall(AddSandStormParticles, 0x72A820);
    RH_ScopedInstall(FindWeatherTypesList, 0x72A520);
    RH_ScopedInstall(ForceWeather, 0x72A4E0);
    RH_ScopedInstall(ForceWeatherNow, 0x72A4F0);
    RH_ScopedInstall(ForecastWeather, 0x72A590);
    RH_ScopedInstall(ReleaseWeather, 0x72A510);
    RH_ScopedInstall(RenderRainStreaks, 0x72AF70);
    RH_ScopedInstall(SetWeatherToAppropriateTypeNow, 0x72A790);
    RH_ScopedInstall(Update, 0x72B850, { .reversed = false }); // TRIAGE: user reports whole-screen flicker
    RH_ScopedInstall(UpdateInTunnelness, 0x72B630);
    //RH_ScopedInstall(UpdateWeatherRegion, 0x72A640, true, { .reversed = false }); // bad
    RH_ScopedInstall(IsRainy, 0x4ABF50);
}

// 0x72A480
void CWeather::Init() {
    ZoneScoped;

    NewWeatherType = WEATHER_EXTRASUNNY_LA;
    OldWeatherType = WEATHER_EXTRASUNNY_LA;
    WeatherRegion  = WEATHER_REGION_DEFAULT;

    InterpolationValue = 0.0f;
    WeatherTypeInList = 0;
    ForcedWeatherType = WEATHER_UNDEFINED;
    WhenToPlayLightningSound = 0;
    bScriptsForceRain = false;
    Rain = 0.0f;
    Sandstorm = 0.0f;
    CurrentRainParticleStrength = 0;
    InTunnelness = 0.0f;
    LightningStartX = 0;
    LightningStartY = 0;
    StreamAfterRainTimer = 0;
}

// Globals not (yet) in `CWeather`
namespace {
    // 0xC81328 - Whether it rained recently (used to keep the rain sound streaming for a bit after it stopped raining)
    auto& bRainedRecently = StaticRef<int32>(0xC81328);
    // 0xC81410 - Eases towards `Rain * 0.2f`, used as the alpha of the rain splash particle
    auto& RainSplashStrength = StaticRef<float>(0xC81410);
    // 0xB6F178 - `TheCamera` + 0x150, base angle for rain drop scattering
    auto& CameraRainAngleBase = StaticRef<float>(0xB6F178);
    // 0xB6F9AC / 0xB6F9B0 - `TheCamera` + 0x984 / + 0x988, wind offset of the rain splash particles origin
    auto& CameraWindOffsetX = StaticRef<float>(0xB6F9AC);
    auto& CameraWindOffsetY = StaticRef<float>(0xB6F9B0);
}; // namespace

// 0x72A9A0
void CWeather::AddRain() {
    ZoneScoped;

    if (CCullZones::CamNoRain() || CCullZones::PlayerNoRain()) {
        return;
    }

    if (UnderWaterness > 0.0f || CGame::currArea != AREA_CODE_NORMAL_WORLD) {
        return;
    }

    if (const auto* const player = FindPlayerPed(-1)) {
        if (player->GetAreaCode() != AREA_CODE_NORMAL_WORLD) {
            return;
        }
    }

    const auto camPos = TheCamera.GetPosition();
    if (camPos.z > 900.0f) {
        return;
    }

    if (TheCamera.GetLookingLRBFirstPerson()) {
        if (auto* const veh = FindPlayerVehicle(-1, false); veh && veh->CarHasRoof()) {
            return;
        }
    }

    // Keep track of when it rained the last time
    bool skipStreamTimerReset{};
    if (Rain <= 0.0f) {
        if (bRainedRecently) {
            if (StreamAfterRainTimer > 0) {
                StreamAfterRainTimer--;
                skipStreamTimerReset = true;
            } else {
                bRainedRecently = false;
            }
        }
    } else {
        bRainedRecently = true;
    }
    if (!skipStreamTimerReset) {
        StreamAfterRainTimer = 800;
    }

    // Sandstorm particles
    if (Wind > 1.01f
        && !CCullZones::CamNoRain()
        && !CCullZones::PlayerNoRain()
        && UnderWaterness < 0.0f
    ) {
        AddSandStormParticles();
    }

    if (Rain <= 0.1f && RainSplashStrength == 0.0f) {
        return;
    }

    const auto maxDist = Rain * 2.5f; // 0x858C80 (5.0f) * 0.5f

    // Rain drops splashing on the ground
    for (auto i = (int32)(Rain * 5.0f); i > 0; i--) { // 5.0f - 0x858C80
        const FxPrtMult_c prtMult{ 1.0f, 1.0f, 1.0f, 0.25f, 0.02f, 0.0f, 0.98f };

        const auto dist = CGeneral::GetRandomNumberInRange(0.0f, maxDist);

        float angle;
        const auto rnd = CGeneral::GetRandomNumber();
        if (rnd & 1) {
            angle = (float)(CGeneral::GetRandomNumber() & 0xFF) * 0.02453125f;   // 0x859C44
        } else {
            angle = (float)((rnd & 0xFF) - 0x80) * 0.00625f + CameraRainAngleBase; // 0x859050
        }

        const auto x = sinf(angle) * dist + camPos.x;
        const auto y = cosf(angle) * dist + camPos.y;

        CColPoint colPoint;
        CEntity*  hitEntity{};
        if (CWorld::ProcessVerticalLine(CVector{ x, y, 0.0f }, -40.0f, colPoint, hitEntity, true, false, false, false, true, false, nullptr)) {
            const auto z = colPoint.m_vecPoint.z + 0.1f;

            // Spread of 30.0f (0x858CA4) around the hit point
            const auto numSplashes = 15 - (int32)(Rain * -2.0f);
            for (auto j = numSplashes; j > 0; j--) {
                const CVector pos{
                    (float)CGeneral::GetRandomNumber() * (1.0f / 32767.0f) * 30.0f + x - 15.0f,
                    (float)CGeneral::GetRandomNumber() * (1.0f / 32767.0f) * 30.0f + y - 15.0f,
                    z
                };
                CGeneral::GetRandomNumber(); // NOTE: Unused in the original
                g_fx.m_Splash->AddParticle(pos, CVector{}, 0.0f, prtMult, -1.0f, 1.2f, 0.6f, false);
            }
        }
    }

    // Ease the rain splash strength towards `Rain * 0.2f`
    if (RainSplashStrength < Rain * 0.2f) { // 0.2f - 0x858CC4
        RainSplashStrength += 0.0025f;      // 0x86C340
    }
    if (Rain * 0.2f < RainSplashStrength) {
        RainSplashStrength -= 0.0025f;
    }
    // Original: `if (RSS < 0) RSS = 0; RSS *= V(0x8D5FF0); if (RSS > 1) RSS = 1;` (V(0x8D5FF0) == 1.0f)
    RainSplashStrength = std::clamp(RainSplashStrength * 1.0f, 0.0f, 1.0f); // 1.0f - 0x8D5FF0

    // One big drop (splash) on the camera
    const FxPrtMult_c heavyDropPrtMult{ 0.7f, 0.7f, 1.0f, RainSplashStrength, 1.0f, 0.0f, 0.2f };
    const CVector pos{
        CameraWindOffsetX * 10.0f + camPos.x + (float)CGeneral::GetRandomNumber() * (1.0f / 32767.0f) * 40.0f - 20.0f,
        CameraWindOffsetY * 10.0f + camPos.y + (float)CGeneral::GetRandomNumber() * (1.0f / 32767.0f) * 40.0f - 20.0f,
                              camPos.z + (float)CGeneral::GetRandomNumber() * (1.0f / 32767.0f) * 7.0f - 2.0f
    };
    g_fx.m_Splash->AddParticle(pos, WindDir * 15.0f, 0.0f, heavyDropPrtMult, -1.0f, 1.2f, 0.6f, false);
}

// 0x72A820
void CWeather::AddSandStormParticles() {
    CVector position = TheCamera.GetPosition();
    position.x += TheCamera.m_mCameraMatrix.GetForward().x * 10.0f;
    position.y += TheCamera.m_mCameraMatrix.GetForward().y * 10.0f;

    position.x += CGeneral::GetRandomNumberInRange(0.0f, 40.0f) - 20.0f;
    position.y += CGeneral::GetRandomNumberInRange(0.0f, 40.0f) - 20.0f;
    position.z += CGeneral::GetRandomNumberInRange(0.0f, 7.00f) - 2.00f;

    g_fx.m_Sand2->AddParticle(position, CWeather::WindDir * 25.0f, 0.0f, FxPrtMult_c(0.67f, 0.65f, 0.55f, 0.25f, 1.0f, 0.0f, 0.2f));
}

// 0x72A520
const eWeatherType* CWeather::FindWeatherTypesList() {
    switch (WeatherRegion) {
    case WEATHER_REGION_LA:     return WeatherTypesListLA;
    case WEATHER_REGION_SF:     return WeatherTypesListSF;
    case WEATHER_REGION_LV:     return WeatherTypesListVegas;
    case WEATHER_REGION_DESERT: return WeatherTypesListDesert;
    default:                    return WeatherTypesListDefault;
    }
}

// 0x72A4E0
void CWeather::ForceWeather(eWeatherType weatherType) {
    ForcedWeatherType = weatherType;
}

// 0x72A4F0
void CWeather::ForceWeatherNow(eWeatherType weatherType) {
    ForcedWeatherType = weatherType;
    OldWeatherType = weatherType;
    NewWeatherType = weatherType;
}

// 0x72A590
bool CWeather::ForecastWeather(eWeatherType weatherType, int32 numSteps) {
    const auto* const list = FindWeatherTypesList();
    for (auto i = 0; i <= numSteps; i++) {
        const auto idx = (WeatherTypeInList + i) % 64;
        if (list[idx] == weatherType) {
            return true;
        }
    }
    return false;
}

// 0x72A510
void CWeather::ReleaseWeather() {
    ForcedWeatherType = WEATHER_UNDEFINED;
}

// 0x72AF70
void CWeather::RenderRainStreaks() {
    if (CTimer::GetIsCodePaused())
        return;

    {
        const auto strength = (uint32)((64.0f - (float)CTimeCycle::m_FogReduction) * (Rain * 110.0f) / 64.0f);
        if (CurrentRainParticleStrength < strength) {
            if (CurrentRainParticleStrength + 1 <= strength) {
                CurrentRainParticleStrength++;
            }
        } else {
            if (CurrentRainParticleStrength > 0) {
                CurrentRainParticleStrength--;
            }
        }
    }

    if (!CurrentRainParticleStrength)
        return;

    if (CCullZones::CamNoRain() || CCullZones::PlayerNoRain())
        return;

    if (UnderWaterness > 0.0f)
        return;

    if (CGame::currArea)
        return;

    const CVector camPos = TheCamera.GetPosition();
    if (camPos.z > 900.0f)
        return;

    uiTempBufferIndicesStored = 0;
    uiTempBufferVerticesStored = 0;

    // (Pirulax) TODO... (refactor)
    constexpr auto RAIN_STREAK_COUNT{ 32u };

    // These are arrays of size `RAIN_STREAK_COUNT`
    static auto& streakPosX = StaticRef<int32*>(0xC81420);
    static auto& streakPosY = StaticRef<int32*>(0xC8141C);
    static auto& streakPosZ = StaticRef<int32*>(0xC81418);
    static auto& streakStrength = StaticRef<uint8*>(0xC81414);

    if (!streakPosX) {
        // This stuff isn't even freed anywhere..
        streakPosX     = new int32[RAIN_STREAK_COUNT];
        streakPosY     = new int32[RAIN_STREAK_COUNT];
        streakPosZ     = new int32[RAIN_STREAK_COUNT];
        streakStrength = new uint8[RAIN_STREAK_COUNT];

        for (unsigned i = 0; i < RAIN_STREAK_COUNT; i++) {
            streakPosX[i] = 0;
            streakPosY[i] = 0;
            streakPosZ[i] = 0;
            streakStrength[i] = (uint8)((float)CurrentRainParticleStrength * 0.6f);
        }
    }

    const auto GetStreakPosition = [&](unsigned i) {
        return CVector{ (float)(streakPosX[i]), (float)(streakPosY[i]), (float)(streakPosZ[i]) };
    };

    const auto UpdateStreak = [&](unsigned i) {
        const CVector posn = GetStreakPosition(i);
        if (!streakStrength[i] || posn.z <= 0.0f || (camPos - posn).Magnitude() > 8.0f) {
            const CVector newPosn = CVector::Random(0.0f, 5.0f) + TheCamera.GetForward() * 6.0f + camPos - CVector{2.5f, 2.5f, 2.5f};
            streakPosX[i] = (int32)(newPosn.x);
            streakPosY[i] = (int32)(newPosn.y);
            streakPosZ[i] = (int32)(newPosn.z);

            streakStrength[i] = (uint8)((float)CurrentRainParticleStrength * 0.6f);
        }
    };

    const auto GetRealVertexIndex = [](unsigned i) {
        return uiTempBufferVerticesStored + i;
    };

    for (unsigned s = 0; s < RAIN_STREAK_COUNT; s++) {
        UpdateStreak(s);

        CVector offsets[2]{};
        offsets[0] = CVector{
            CGeneral::GetRandomNumberInRange(-0.2f, 0.2f),
            CGeneral::GetRandomNumberInRange(-0.2f, 0.2f),
            CGeneral::GetRandomNumberInRange(-0.1f, 0.1f),
        };

        const float posMul = (s % 2) ? Wind * 0.1f : Wind * Rain * 0.1f;
        offsets[1] = offsets[0] - WindDir * posMul + CVector{ 0.0f, 0.0f, CGeneral::GetRandomNumberInRange(0.1f, 0.5f) };

        const uint8 alphas[]{ streakStrength[s], static_cast<uint8>(streakStrength[s] / 2u) };
        for (auto v = 0u; v < std::size(alphas); v++) {
            RxObjSpace3DVertex* vertex = &TempBufferVertices.m_3d[GetRealVertexIndex(v)];

            const RwRGBA color{ 210, 210, 230, alphas[v] };
            // const RwRGBA color{ 255, 0, 0, 255 }; // For debug (makes it more visible)
            RxObjSpace3DVertexSetPreLitColor(vertex, &color);

            const CVector vertPosn = GetStreakPosition(s) + offsets[v];
            RxObjSpace3DVertexSetPos(vertex, &vertPosn);

            aTempBufferIndices[uiTempBufferIndicesStored + v] = GetRealVertexIndex(v);
        }

        streakPosZ[s] -= (int32)CGeneral::GetRandomNumberInRange(0.01f, 0.1f);
        streakStrength[s] = (uint8)std::max(0, (int32)streakStrength[s] - CGeneral::GetRandomNumberInRange(2, 5));

        uiTempBufferVerticesStored += 2;
        uiTempBufferIndicesStored += 2;
    }

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEFOGTYPE,           RWRSTATE(rwFOGTYPELINEAR));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(NULL));

    if (RwIm3DTransform(TempBufferVertices.m_3d, uiTempBufferVerticesStored, nullptr, rwIM3D_VERTEXXYZ)) {
        RwIm3DRenderIndexedPrimitive(rwPRIMTYPELINELIST, aTempBufferIndices, uiTempBufferIndicesStored);
        RwIm3DEnd();
    }

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
}

// 0x72A790
void CWeather::SetWeatherToAppropriateTypeNow() {
    CVector playerCoors = FindPlayerCoors();
    UpdateWeatherRegion(&playerCoors);

    auto weatherType = FindWeatherTypesList()[0];
    ForcedWeatherType = WEATHER_UNDEFINED;
    OldWeatherType = weatherType;
    NewWeatherType = weatherType;
}

// 0x72B850
void CWeather::Update() {
    ZoneScoped;

    const auto IsRainyWeather = [](eWeatherType t) {
        return t == WEATHER_RAINY_SF || t == WEATHER_RAINY_COUNTRYSIDE;
    };
    const auto IsSunnyWeather = [](eWeatherType t) {
        switch (t) { // All extra sunny + sunny weathers
        case WEATHER_EXTRASUNNY_LA:
        case WEATHER_SUNNY_LA:
        case WEATHER_EXTRASUNNY_SMOG_LA:
        case WEATHER_SUNNY_SMOG_LA:
        case WEATHER_SUNNY_SF:
        case WEATHER_EXTRASUNNY_SF:
        case WEATHER_SUNNY_VEGAS:
        case WEATHER_EXTRASUNNY_VEGAS:
        case WEATHER_EXTRASUNNY_COUNTRYSIDE:
        case WEATHER_SUNNY_COUNTRYSIDE:
        case WEATHER_EXTRASUNNY_DESERT:
        case WEATHER_SUNNY_DESERT:
            return true;
        default:
            return false;
        }
    };
    const auto IsCloudyWeather = [](eWeatherType t) {
        return t == WEATHER_CLOUDY_LA || t == WEATHER_CLOUDY_SF || t == WEATHER_CLOUDY_VEGAS || t == WEATHER_CLOUDY_COUNTRYSIDE;
    };
    const auto IsDesertHeatWeather = [](eWeatherType t) {
        return t == WEATHER_EXTRASUNNY_DESERT || t == WEATHER_SUNNY_DESERT || t == WEATHER_EXTRASUNNY_VEGAS || t == WEATHER_EXTRASUNNY_LA;
    };

    // Update the weather region every 16 frames
    if ((CTimer::m_FrameCounter & 0xF) == 0) {
        UpdateWeatherRegion(nullptr);
    }

    auto newWeather = OldWeatherType;
    if (CReplay::Mode != MODE_PLAYBACK) {
        const auto interp = ((float)CClock::ms_nGameClockSeconds * (1.0f / 60.0f) + (float)CClock::ms_nGameClockMinutes) * (1.0f / 60.0f);
        if (interp < InterpolationValue) {
            // Advance the forecast
            UpdateWeatherRegion(nullptr);
            newWeather = NewWeatherType;
            OldWeatherType = NewWeatherType;
            if (ForcedWeatherType < 0) {
                if (TheCamera.GetPosition().z < 950.0f) {
                    WeatherTypeInList = (WeatherTypeInList + 1) % 64;
                    NewWeatherType = FindWeatherTypesList()[WeatherTypeInList];
                    newWeather = NewWeatherType;
                }
            } else {
                OldWeatherType = ForcedWeatherType;
            }
        }
        InterpolationValue = interp;
    }

    // Lightning state machine
    if (IsRainyWeather(OldWeatherType) && IsRainyWeather(newWeather)
        && !CCullZones::CamNoRain()
        && !CCullZones::PlayerNoRain()
        && UnderWaterness < 0.0f
        && CGame::currArea == AREA_CODE_NORMAL_WORLD
    ) {
        if (!LightningBurst) {
            if ((CGeneral::GetRandomNumber() & 0xFFFF) < 200) {
                LightningStart = CTimer::m_FrameCounter;
                LightningFlashLastChange = CTimer::GetTimeInMS();
                LightningBurst = true;
                LightningFlash = true;
            } else {
                LightningFlash = false;
            }
        } else {
            if ((CGeneral::GetRandomNumber() & 0xFF) < 0x18) {
                LightningBurst = false;
                LightningDuration = 20;
                if (CTimer::m_FrameCounter - LightningStart < 0x15) {
                    LightningDuration = CTimer::m_FrameCounter - LightningStart;
                }
                WhenToPlayLightningSound = (20 - LightningDuration) * 150 + CTimer::GetTimeInMS();
                LightningFlash = false;
            } else if (CTimer::GetTimeInMS() - LightningFlashLastChange > 50) {
                const auto oldFlash = LightningFlash;
                LightningFlash = CGeneral::GetRandomNumber() & 1;
                if (oldFlash != LightningFlash) {
                    LightningFlashLastChange = CTimer::GetTimeInMS();
                }
            }
        }
    } else {
        LightningFlash = false;
        LightningBurst = false;
    }

    // Thunder + controller shake when the lightning timer runs out
    if (WhenToPlayLightningSound != 0 && (uint32)WhenToPlayLightningSound < CTimer::GetTimeInMS()) {
        m_WeatherAudioEntity.AddAudioEvent(AE_THUNDER);
        const auto shakeFreq = (uint8)((int8)LightningDuration + 40) * 2;
        const auto shakeTime = LightningDuration * 40 + 100;
        CPad::GetPad(0)->StartShake((int16)shakeTime, shakeFreq, 0);
        WhenToPlayLightningSound = 0;
    }

    // Wet roads
    if (IsRainyWeather(newWeather)) {
        WetRoads = IsRainyWeather(OldWeatherType) ? 1.0f : 1.0f - InterpolationValue;
    } else if (IsRainyWeather(OldWeatherType)) {
        WetRoads = InterpolationValue;
    } else {
        WetRoads = 0.0f;
    }

    // Rain
    float rainTarget{};
    if (IsRainyWeather(OldWeatherType)) {
        rainTarget = InterpolationValue;
    }
    if (IsRainyWeather(newWeather)) {
        rainTarget = (1.0f - InterpolationValue) + rainTarget;
    }
    {
        const auto target = ((float)((CTimer::GetTimeInMS() >> 13) & 3) * 0.1f + 0.7f) * rainTarget;
        const auto step   = CTimer::GetTimeStep() * 0.005f;
        const auto diff   = target - Rain;
        Rain = std::abs(diff) >= step
            ? (diff <= 0.0f ? Rain - step : Rain + step)
            : target;
    }

    // Sandstorm
    float sandTarget{};
    if (OldWeatherType == WEATHER_SANDSTORM_DESERT) {
        sandTarget = InterpolationValue;
    }
    if (newWeather == WEATHER_SANDSTORM_DESERT) {
        sandTarget = (1.0f - InterpolationValue) + sandTarget;
    }
    {
        const auto target = ((float)((CTimer::GetTimeInMS() >> 13) & 3) * 0.1f + 0.7f) * sandTarget;
        const auto step   = CTimer::GetTimeStep() * 0.005f;
        const auto diff   = target - Sandstorm;
        Sandstorm = std::abs(diff) >= step
            ? (diff <= 0.0f ? Sandstorm - step : Sandstorm + step)
            : target;
    }

    // Cloud coverage
    CloudCoverage = IsSunnyWeather(newWeather) ? 0.0f : 1.0f - InterpolationValue;
    if (!IsSunnyWeather(OldWeatherType)) {
        CloudCoverage += InterpolationValue;
    }

    // Fog
    Foggyness = newWeather == WEATHER_FOGGY_SF || newWeather == WEATHER_SANDSTORM_DESERT
        ? 1.0f - InterpolationValue
        : 0.0f;
    if (OldWeatherType == WEATHER_FOGGY_SF || OldWeatherType == WEATHER_SANDSTORM_DESERT) {
        Foggyness += InterpolationValue;
    }

    // SF fog
    Foggyness_SF = newWeather == WEATHER_FOGGY_SF ? 1.0f - InterpolationValue : 0.0f;
    if (OldWeatherType == WEATHER_FOGGY_SF) {
        Foggyness_SF += InterpolationValue;
    }

    // Extra sunnyness
    ExtraSunnyness = IsExtraSunny(newWeather) ? 1.0f - InterpolationValue : 0.0f;
    if (IsExtraSunny(OldWeatherType)) {
        ExtraSunnyness += InterpolationValue;
    }

    // Rainbow (only when transitioning from a cloudy to one of a few sunny weathers, in the morning / evening)
    const auto IsSunnyForRainbow = [](eWeatherType t) { // NOTE: Different (smaller) set than `IsSunnyWeather`
        switch (t) {
        case WEATHER_SUNNY_LA:
        case WEATHER_SUNNY_SMOG_LA:
        case WEATHER_SUNNY_SF:
        case WEATHER_SUNNY_VEGAS:
        case WEATHER_EXTRASUNNY_COUNTRYSIDE:
        case WEATHER_EXTRASUNNY_DESERT:
            return true;
        default:
            return false;
        }
    };
    if (IsCloudyWeather(OldWeatherType) && IsSunnyForRainbow(newWeather)
        && InterpolationValue < 0.5f
        && CClock::ms_nGameClockHours >= 7 && CClock::ms_nGameClockHours <= 20
    ) {
        Rainbow = 1.0f - std::abs(InterpolationValue - 0.25f) * 4.0f;
    } else {
        Rainbow = 0.0f;
    }

    // Sun glare
    SunGlare = IsSunnyWeather(newWeather) ? 1.0f - InterpolationValue : 0.0f;
    if (IsSunnyWeather(OldWeatherType)) {
        SunGlare += InterpolationValue;
    }
    if (SunGlare > 0.0f) {
        auto sunMult = CTimeCycle::m_VectorToSun[CTimeCycle::m_CurrentStoredValue].z * 7.0f;
        SunGlare = std::clamp(sunMult, 0.0f, 1.0f) * SunGlare;
        SunGlare = std::clamp(SunGlare, 0.0f, 1.0f);
        if (!CSpecialFX::bSnapShotActive) {
            SunGlare = (1.0f - (float)(CGeneral::GetRandomNumber() & 0x1F) * 0.007f) * SunGlare;
        }
    }

    // Heat haze
    HeatHaze = IsDesertHeatWeather(newWeather) ? 1.0f - InterpolationValue : 0.0f;
    if (IsDesertHeatWeather(OldWeatherType)) {
        HeatHaze += InterpolationValue;
    }

    // Heat haze fade in / out (based on time of day)
    if (HeatHaze > 0.0f) {
        const auto minute = CClock::ms_nGameClockMinutes;
        const auto hour   = CClock::ms_nGameClockHours;
        const bool minuteChanged  = minute != HeatHazeLastMinute;
        const bool inTimeRange    = hour >= CPostEffects::m_HeatHazeFXHourOfDayStart && hour < CPostEffects::m_HeatHazeFXHourOfDayEnd;
        const bool afterTimeRange = hour >= CPostEffects::m_HeatHazeFXHourOfDayEnd;
        HeatHazeFXControl = 0.0f; // Reset every frame, reassigned below
        auto fadeSpeed = CPostEffects::m_fHeatHazeFXFadeSpeed;

        const auto* const player = FindPlayerPed(-1);
        if (CGame::currArea == AREA_CODE_NORMAL_WORLD
            && player && player->GetAreaCode() == AREA_CODE_NORMAL_WORLD
            && !CCullZones::CamNoRain()
            && !CCullZones::PlayerNoRain()
        ) {
            if (inTimeRange) {
                if (minuteChanged) {
                    HeatHazeFade += fadeSpeed;
                }
                HeatHazeFXControl = HeatHazeFade;
                if (HeatHazeFade > 1.0f) {
                    HeatHazeFade = 1.0f;
                    HeatHazeFXControl = 1.0f;
                }
            }
            if (afterTimeRange) {
                if (minuteChanged) {
                    HeatHazeFade -= fadeSpeed;
                }
                HeatHazeFXControl = HeatHazeFade;
                if (HeatHazeFade < 0.0f) {
                    HeatHazeFade = 0.0f;
                    HeatHazeFXControl = 0.0f;
                }
            }
        } else {
            fadeSpeed = CPostEffects::m_fHeatHazeFXInsideBuildingFadeSpeed; // 0x8D50E0
            if (minuteChanged) {
                HeatHazeFade -= fadeSpeed;
            }
            HeatHazeFXControl = HeatHazeFade;
            if (HeatHazeFade < 0.0f) {
                HeatHazeFade = 0.0f;
                HeatHazeFXControl = 0.0f;
            }
        }
        HeatHazeFXControl *= HeatHaze;
        HeatHazeLastMinute = minute;
    }

    // Water fog (animates while the player is under water)
    {
        const auto target = (float)WaterFogCounter * 0.01f; // 0x858C58
        if (WaterFogCounter == 0) {
            WaterFogTransitionValue = 0.0f;
        } else if (WaterFogCounter >= 0x5F) {
            bWaterFogTransitionDone = true;
        }
        if (!bWaterFogTransitionDone && WaterFogTransitionValue < target) {
            WaterFogTransitionValue = target;
        }
        if (target < WaterFogTransitionValue) {
            WaterFogTransitionValue = target;
        }
        if (WaterFogTransitionValue < 0.0f) {
            bWaterFogTransitionDone = false;
        }

        WaterFogFXControl = WaterFogTransitionValue * 1.4f; // 0x859994
        WaterFogFXControl = WaterFogFXControl >= 0.0f
            ? std::min(WaterFogFXControl, 1.0f)
            : 0.0f;
    }

    // Wind strength + clipping
    Wind = WindStrengthPerWeatherType[OldWeatherType] * InterpolationValue
        + (1.0f - InterpolationValue) * WindStrengthPerWeatherType[newWeather];
    WindClipped = std::min(Wind, 1.0f);

    // Wind direction
    {
        const auto idx = (CTimer::GetTimeInMS() >> 10) & 0xF;
        const auto w   = 0.5f - cosf((float)(CTimer::GetTimeInMS() & 0x3FF) * 0.0009765625f * PI) * 0.5f; // 0x85C6D4, 0x858CB8
        const auto iw  = 1.0f - w;
        auto x = (iw * WindDirTable[idx] + w * WindDirTable[(idx + 1) & 0xF]) * WindClipped * 0.4f + WindClipped * 0.7f; // 0x858EE8, 0x858CB0
        auto y = (w * WindDirTable[(idx + 4) & 0xF] + iw * WindDirTable[(idx + 3) & 0xF]) * WindClipped * 0.4f + WindClipped * 0.7f;
        auto z = (w * WindDirTable[(idx + 7) & 0xF] + iw * WindDirTable[(idx + 6) & 0xF]) * WindClipped * 0.2f;         // 0x858CC4

        const auto extra = (WindClipped - 0.5f) * 0.4f;
        if (extra > 0.0f) {
            const auto idx2 = (CTimer::GetTimeInMS() >> 8) & 0xF;
            const auto w2   = (float)(CTimer::GetTimeInMS() & 0xFF) * 0.00390625f; // 0x859AA0
            const auto iw2  = 1.0f - w2;
            x += (iw2 * WindDirTable[idx2] + w2 * WindDirTable[(idx2 + 1) & 0xF]) * extra;
            y += (w2 * WindDirTable[(idx2 + 4) & 0xF] + iw2 * WindDirTable[(idx2 + 3) & 0xF]) * extra;
            z += (w2 * WindDirTable[(idx2 + 7) & 0xF] + iw2 * WindDirTable[(idx2 + 6) & 0xF]) * extra;
        }

        const auto idx3  = ((CTimer::GetTimeInMS() >> 11) & 0xF) + 1;
        const auto w3    = 0.5f - cosf((float)(CTimer::GetTimeInMS() & 0x7FF) * 0.00048828125f * PI) * 0.5f; // 0x863990
        const auto scale = w3 * WindDirTable2[(idx3 & 0xF) + 1] + (1.0f - w3) * WindDirTable2[idx3];
        WindDir.x = scale * x;
        WindDir.y = scale * y;
        WindDir.z = scale * z;
    }

    Wavyness = std::min(WindClipped + 0.3f, 1.0f); // 0x858C24

    // No rain under water
    if (1.0f - UnderWaterness <= Rain) {
        Rain = 1.0f - UnderWaterness;
    }

    // Traffic lights brightness
    if (CClock::ms_nGameClockHours < 21) {
        if (CClock::ms_nGameClockHours > 19) {
            TrafficLightsBrightness = (float)CClock::ms_nGameClockMinutes * (1.0f / 60.0f);
        } else if (CClock::ms_nGameClockHours > 6) {
            TrafficLightsBrightness = 0.0f;
        } else if (CClock::ms_nGameClockHours > 5) {
            TrafficLightsBrightness = 1.0f - (float)CClock::ms_nGameClockMinutes * (1.0f / 60.0f);
        } else {
            TrafficLightsBrightness = 1.0f;
        }
    } else {
        TrafficLightsBrightness = 1.0f;
    }

    // Head lights spectrum
    HeadLightsSpectrum = std::min(std::max(Rain, Foggyness), 1.0f);

    TrafficLightsBrightness = std::max(TrafficLightsBrightness, WetRoads);
    TrafficLightsBrightness = std::max(TrafficLightsBrightness, Foggyness);
    TrafficLightsBrightness = std::max(TrafficLightsBrightness, Rain);

    AddRain();

    // Check if the player is ragdolled (see original)
    const bool isRagdolled = [&] {
        if (const auto* const player = FindPlayerPed(-1)) {
            if (const auto* const task = player->GetTaskManager().GetSimplestActiveTask()) {
                return task->GetTaskType() == TASK_SIMPLE_RAGDOLL;
            }
        }
        return false;
    }();

    if (IsSunnyWeather(OldWeatherType)
        && CGame::currArea == AREA_CODE_NORMAL_WORLD
        && !CCutsceneMgr::ms_running
        && (CTimer::m_FrameCounter & 7) == 0
        && isRagdolled
    ) {
        FindPlayerPed(-1); // NOTE: Result unused in the original
    }

    UpdateInTunnelness();

    m_WeatherAudioEntity.Service();
}

// 0x72B630
void CWeather::UpdateInTunnelness() {
    ZoneScoped;

    static const CVector s_TunnelPoint1{ 85.0f, -1020.0f, 0.0f }; // 0xC81430
    static const CVector s_TunnelPoint2{ 1683.0f, -1956.0f, 0.0f }; // 0xC81424

    float target = 0.0f;
    if (CCullZones::CurrentFlags_Camera & 0x2000) { // TODO: Unnamed tunnel-related eZoneAttributes flag (bit 0x2000)
        const CVector from{ CVector2D{ TheCamera.GetPosition() } };
        const CVector to = from + CVector{ CVector2D{ TheCamera.GetForwardVector() }.Normalized() } * 100.0f;
        const auto dist = std::min({
            CCollision::DistToLine(from, to, s_TunnelPoint1),
            CCollision::DistToLine(from, to, s_TunnelPoint2),
            100.0f,
        });
        target = std::min(1.0f, dist / 100.0f);
    }

    InTunnelness = notsa::step_to(InTunnelness, target, CTimer::GetTimeStep() * 0.01f);
}

// Based on 0x72A640
eWeatherRegion CWeather::FindWeatherRegion(CVector2D pos) {
    if (pos.x > 1000.0f && pos.y > 910.0f) {
        return WEATHER_REGION_LV;
    }
    if (pos.x > -850.0f && pos.x < 1000.0f && pos.y > 1280.0f) {
        return WEATHER_REGION_DESERT;
    }
    if (pos.x < -1430.0f && pos.y > -580.0f && pos.y < 1430.0f) {
        return WEATHER_REGION_SF;
    }
    if (pos.x > 250.0f && pos.x < 3000.0f && pos.y > -3000.0f && pos.y < -850.0f) {
        return WEATHER_REGION_LA;
    }
    return WEATHER_REGION_DEFAULT;
}

// 0x72A640
void CWeather::UpdateWeatherRegion(CVector* posn) {
    WeatherRegion = FindWeatherRegion(posn ? *posn : TheCamera.GetPosition());
}

// 0x4ABF50
bool CWeather::IsRainy() {
    return Rain >= 0.2f;
}
