/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "Hud.h"
#include "Garages.h"
#include "IdleCam.h"
#include "MenuSystem.h"
#include "Radar.h"
#include "Vehicle.h"
#include "EntryExitManager.h"
#include "TaskSimpleUseGun.h"
#include "eHud.h"

// --- headers added by int-hud (hud-1 batch integration) ---
// Each one verified to exist in the repo (source/game_sa/...):
#include "Audio/AudioEngine.h"   // CAudioEngine + global AudioEngine (DrawHelpText: ReportFrontendAudioEvent)
#include "Camera.h"              // CCamera / CCam (DrawCrossHairs, DrawHelpText, DrawSubtitles)
#include "Clock.h"               // CClock::CurrentDay (DrawVitalStats)
#include "CutsceneMgr.h"         // CCutsceneMgr::ms_running (DrawHelpText, DrawSubtitles)
#include "GameLogic.h"           // CGameLogic::IsCoopGameGoingOn (DrawSubtitles)
#include "HudColours.h"          // HudColour.GetRGB/GetRGBA (DrawMissionTimers, DrawHelpText, DrawSuccessFailedMessage, DrawWanted)
#include "MenuManager.h"         // FrontEndMenuManager (DrawVitalStats: DrawWindow; m_bWidescreenOn/m_bShowSubtitles)
#include "Messages.h"            // CMessages::StringCopy/StringCompare (DrawHelpText)
#include "OnscreenTimer.h"       // COnscreenTimer + entries + eOnscreenCounter (DrawMissionTimers)
#include "PedGroupMembership.h"  // CPedGroupMembership::CountMembersExcludingLeader (DrawHelpText)
#include "PedGroups.h"           // CPedGroups::ms_groups (DrawHelpText)
#include "PlayerPedData.h"       // CPlayerPedData (m_nPlayerGroup, m_bFreeAiming, m_fBreath)
#include "Scripts/TheScripts.h"  // CTheScripts (bUseMessageFormatting/MessageWidth/MessageCentre, bDrawCrossHair)
#include "Stats.h"               // CStats (DrawVitalStats, DrawHelpText, DrawPlayerInfo)
#include "Text/Text.h"           // TheText (DrawVitalStats, DrawMissionTimers, DrawHelpText)
#include "Timer.h"               // CTimer::m_FrameCounter/ms_fTimeStep/m_snTimeInMilliseconds
#include "UserDisplay.h"         // CUserDisplay::OnscnTimer (DrawMissionTimers)
#include "Wanted.h"              // CWanted (DrawWanted)
#include "WeaponInfo.h"          // CWeaponInfo (DrawVitalStats, DrawCrossHairs)
#include "World.h"               // FindPlayerPed/FindPlayerVehicle/FindPlayerWanted, CWorld::Players/PlayerInFocus

// --- file-scope globals from hud-1 fragments (addresses verified unclaimed in repo) ---
// From 0x589650.cpp (DrawVitalStats): float tables of skill stat display params, filled at runtime.
// NOTE: 0xB78F40 falls inside CStats::StatReactionValue (0xB78F10) range and 0xB78EB4 inside
// CStats::PedsKilledOfThisType (0xB78E90) range - the repo CStats layout is probably shifted; see notes_int-hud.md.
static auto& SkillStatDispTable1 = StaticRef<std::array<float, 100>>(0xB78EB4); // index = skill stat id (69..84)
static auto& SkillStatDispTable2 = StaticRef<std::array<float, 16>>(0xB78F40);  // index = skill stat id - 69

// From 0x58C250.cpp (DrawSubtitles): "subtitles shown in widescreen camera mode" flag.
static auto& bSubtitleShownInWidescreen = StaticRef<bool>(0xBAB214);

// From 0x58C6A0.cpp (DrawSuccessFailedMessage): one-time Y position of the message + init flag.
// NOTE: 0xBAB218 is probably shared with the original DrawBustedWastedMessage (0x58CA50) - in the repo
// that one uses a literal (maximumHeight/2 - SCREEN_SCALE_Y(30)), see comment "// OG: posY static allocated var".
static auto& SuccessFailedMessagePosY    = StaticRef<float>(0xBAB218);
static auto& bSuccessFailedMessagePosSet = StaticRef<bool>(0xBAB21C);

// From 0x58D9A0.cpp (DrawWanted): wanted-stars-visible-this-frame flag + flash star RGB.
static auto& bWantedDisplayed  = StaticRef<bool>(0xBAB228);  // stars visible on this frame
static auto& FlashStarColourR  = StaticRef<uint8>(0xBAB244); // RGB of flashing stars (x0.8 when drawn)
static auto& FlashStarColourG  = StaticRef<uint8>(0xBAB245);
static auto& FlashStarColourB  = StaticRef<uint8>(0xBAB246);

void CHud::InjectHooks() {
    RH_ScopedClass(CHud);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Initialise, 0x5BA850);
    RH_ScopedInstall(ReInitialise, 0x588880);
    RH_ScopedInstall(Shutdown, 0x588850);
    RH_ScopedInstall(Draw, 0x58FAE0);
    RH_ScopedInstall(GetRidOfAllHudMessages, 0x588A50);
    RH_ScopedInstall(GetYPosBasedOnHealth, 0x588B60);
    RH_ScopedInstall(HelpMessageDisplayed, 0x588B50);
    RH_ScopedInstall(ResetWastedText, 0x589070);
    RH_ScopedInstall(SetMessage, 0x588F60);
    RH_ScopedInstall(SetBigMessage, 0x588FC0);
    RH_ScopedInstall(SetHelpMessage, 0x588BE0);
    RH_ScopedInstall(SetHelpMessageStatUpdate, 0x588D40);
    RH_ScopedInstall(SetHelpMessageWithNumber, 0x588E30);
    RH_ScopedInstall(SetVehicleName, 0x588F50);
    RH_ScopedInstall(SetZoneName, 0x588BB0);
    RH_ScopedInstall(DrawAfterFade, 0x58D490);
    RH_ScopedInstall(DrawAreaName, 0x58AA50);
    RH_ScopedInstall(DrawBustedWastedMessage, 0x58CA50);
    RH_ScopedInstall(DrawCrossHairs, 0x58E020); // int-hud: removed
    RH_ScopedInstall(DrawFadeState, 0x58D580);
    RH_ScopedInstall(DrawHelpText, 0x58B6E0); // int-hud: removed
    RH_ScopedInstall(DrawMissionTimers, 0x58B180); // int-hud: removed
    RH_ScopedInstall(DrawMissionTitle, 0x58D240);
    RH_ScopedInstall(DrawOddJobMessage, 0x58CC80);
    RH_ScopedInstall(DrawRadar, 0x58A330);
    RH_ScopedInstall(DrawScriptText, 0x58C080);
    RH_ScopedInstall(DrawSubtitles, 0x58C250); // int-hud: removed
    RH_ScopedInstall(DrawSuccessFailedMessage, 0x58C6A0); // int-hud: removed
    RH_ScopedInstall(DrawVehicleName, 0x58AEA0);
    RH_ScopedInstall(DrawVitalStats, 0x589650); // int-hud: removed
    RH_ScopedInstall(DrawAmmo, 0x5893B0);
    RH_ScopedInstall(DrawPlayerInfo, 0x58EAF0); // int-hud: removed
    RH_ScopedInstall(DrawTripSkip, 0x58A160);
    RH_ScopedInstall(DrawWanted, 0x58D9A0); // int-hud: removed
    RH_ScopedInstall(DrawWeaponIcon, 0x58D7D0);
    RH_ScopedInstall(RenderArmorBar, 0x5890A0);
    RH_ScopedInstall(RenderBreathBar, 0x589190);
    RH_ScopedInstall(RenderHealthBar, 0x589270);
}

// 0x5BA850
void CHud::Initialise() {
    static constexpr SpriteFileName textures[]= { // 0x8D128C
        { "fist",           "fistm"           },
        { "siteM16",        "siteM16m"        },
        { "siterocket",     "siterocketm"     },
        { "radardisc",      "radardiscA"      },
        { "radarRingPlane", "radarRingPlaneA" },
        { "SkipIcon",       "SkipIconA"       },
    };

    auto txd = CTxdStore::AddTxdSlot("hud");
    CTxdStore::LoadTxd(txd, "MODELS\\HUD.TXD");
    CTxdStore::AddRef(txd);
    CTxdStore::PushCurrentTxd();
    CTxdStore::SetCurrentTxd(txd);

    for (auto i = 0u; i < std::size(Sprites); i++) {
        const auto& [texture, mask] = textures[i];
        Sprites[i].SetTexture(texture, mask);
    }
    CTxdStore::PopCurrentTxd();
    ReInitialise();
}

// 0x588880
void CHud::ReInitialise() {
    memset(m_pHelpMessageToPrint, 0, sizeof(m_pHelpMessageToPrint));
    memset(m_pLastHelpMessage,    0, sizeof(m_pLastHelpMessage));
    memset(m_pHelpMessage,        0, sizeof(m_pHelpMessage));
    memset(m_Message,             0, sizeof(m_Message));
    memset(m_BigMessage,          0, sizeof(m_BigMessage));
    memset(BigMessageX,           0, sizeof(BigMessageX));

    OddJob2On       = 0;
    OddJob2Timer    = 0;
    OddJob2XOffset  = 0.0f;
    OddJob2OffTimer = 0.0f;
    PagerXOffset    = 150.0f;

    std::ranges::fill(TimerCounterHideState, 0);
    std::ranges::fill(TimerCounterWasDisplayed, 0);
    TimerMainCounterWasDisplayed = false;
    TimerMainCounterHideState = 0;

    const CPlayerInfo& playerInfo   = FindPlayerInfo();
    m_LastTimeEnergyLost            = playerInfo.m_nLastTimeEnergyLost;
    m_LastDisplayScore              = playerInfo.m_nDisplayMoney;
    m_fHelpMessageStatUpdateValue   = 0.0f;
    m_Wants_To_Draw_Hud             = true;
    m_bDraw3dMarkers                = true;
    m_ZoneNameTimer                 = 0;
    m_pZoneName                     = nullptr;
    m_pLastZoneName                 = nullptr;
    m_ZoneState                     = NAME_DONT_SHOW;
    m_nHelpMessageTimer             = 0;
    m_nHelpMessageFadeTimer         = 0;
    m_nHelpMessageState             = 0;
    m_bHelpMessageQuick             = false;
    m_nHelpMessageStatId            = 0;
    m_nHelpMessageMaxStatValue      = 1000;
    m_bHelpMessagePermanent         = false;
    m_fHelpMessageTime              = 1.0f;
    m_fHelpMessageBoxWidth          = 200.0f;
    m_pVehicleName                  = nullptr;
    m_pLastVehicleName              = nullptr;
    m_pVehicleNameToPrint           = nullptr;
    m_VehicleNameTimer              = 0;
    m_VehicleFadeTimer              = 0;
    m_VehicleState                  = NAME_DONT_SHOW;
    bScriptDontDisplayRadar         = false;
    bScriptForceDisplayWithCounters = false;
    bScriptDontDisplayVehicleName   = false;
    bScriptDontDisplayAreaName      = false;
    m_ItemToFlash                   = ITEM_NONE;
    m_EnergyLostTimer               = 0;
    m_EnergyLostFadeTimer           = 0;
    m_EnergyLostState               = 5;
    m_DisplayScoreTimer             = 0;
    m_DisplayScoreFadeTimer         = 0;
    m_DisplayScoreState             = 5;
    m_LastWanted                    = 0;
    m_WantedTimer                   = 0;
    m_WantedFadeTimer               = 0;
    m_WantedState                   = 5;
    m_LastWeapon                    = 0;
    m_WeaponTimer                   = 0;
    m_WeaponFadeTimer               = 0;
    m_WeaponState                   = 5;
    bDrawClock                      = true;
    m_LastBreathTime                = 0;
}

// 0x588850
void CHud::Shutdown() {
    std::ranges::for_each(Sprites, [](auto& sprite) { sprite.Delete(); });
    CTxdStore::RemoveTxdSlot(CTxdStore::FindTxdSlot("hud"));
}

// 0x588B60
float CHud::GetYPosBasedOnHealth(uint8 playerId, float pos, int8 offset) {
    return (float)FindPlayerInfo(playerId).m_nMaxHealth < 101.0f
               ? pos - SCREEN_SCALE_Y((float)offset)
               : pos;
}

// 0x588B50
bool CHud::HelpMessageDisplayed() {
    return m_nHelpMessageState != 0;
}

// 0x588F60
void CHud::SetMessage(const GxtChar* message) {
    if (message) {
        strncpy_s((char*)m_Message, sizeof(m_Message), AsciiFromGxtChar(message), sizeof(m_Message));
    } else {
        m_Message[0] = '\0';
    }
}

// little bit different from OG
// 0x588FC0
void CHud::SetBigMessage(GxtChar* message, eMessageStyle style) {
    if (BigMessageX[style] != 0.0f) {
        return;
    }

    strncpy_s((char*)m_BigMessage[style], sizeof(m_BigMessage[style]), AsciiFromGxtChar(message), sizeof(m_BigMessage[style]));

    switch (style) {
    case STYLE_WHITE_MIDDLE_SMALLER: {
        if (strcmp(AsciiFromGxtChar(message), AsciiFromGxtChar(LastBigMessage[STYLE_WHITE_MIDDLE_SMALLER])) != 0) {
            OddJob2OffTimer = 0.0f;
            OddJob2On = 0;
        }
        strncpy_s((char*)LastBigMessage[style], sizeof(LastBigMessage[style]), AsciiFromGxtChar(message), sizeof(LastBigMessage[style]));
        break;
    }
    default: {
        message[0] = '\0';
    }
    }
}

// 0x588BE0
void CHud::SetHelpMessage(const GxtChar* text, bool quickMessage, bool permanent, bool addToBrief) {
    if (m_BigMessage[STYLE_MIDDLE_SMALLER_HIGHER][0] || CGarages::MessageIDString[0] || CReplay::Mode == MODE_PLAYBACK || CCutsceneMgr::IsRunning()) {
        return;
    }

    std::ranges::fill(m_pHelpMessageToPrint, '\0');
    std::ranges::fill(m_pLastHelpMessage, '\0');
    std::ranges::fill(m_pHelpMessage, '\0');

    CMessages::StringCopy(m_pHelpMessage, text, sizeof(m_pHelpMessage));
    CMessages::InsertPlayerControlKeysInString(m_pHelpMessage);
    if (m_nHelpMessageState && CMessages::StringCompare(m_pHelpMessage, m_pHelpMessageToPrint, sizeof(m_pHelpMessage)))
        return;

    std::ranges::fill(m_pLastHelpMessage, '\0');
    if (!text) {
        m_pHelpMessage[0] = '\0';
        m_pHelpMessageToPrint[0] = '\0';
    }

    if (permanent) {
        m_nHelpMessageState = 1;
        CMessages::StringCopy(m_pHelpMessageToPrint, m_pHelpMessage, sizeof(m_pHelpMessage));
        CMessages::StringCopy(m_pLastHelpMessage, m_pHelpMessage, sizeof(m_pHelpMessage));
    } else {
        m_nHelpMessageState = 0;
    }

    if (addToBrief)
        CMessages::AddToPreviousBriefArray(text);

    m_bHelpMessagePermanent = permanent;
    m_bHelpMessageQuick = quickMessage;
    m_nHelpMessageStatId = 0;
    m_nHelpMessageMaxStatValue = 1000;
    m_fHelpMessageStatUpdateValue = 0.0f;
}

// 0x588D40
void CHud::SetHelpMessageStatUpdate(eStatUpdateState state, uint16 statId, float diff, float max) {
    if (m_BigMessage[STYLE_MIDDLE_SMALLER_HIGHER][0] || CGarages::MessageIDString[0] || CReplay::Mode == MODE_PLAYBACK || CCutsceneMgr::IsCutsceneProcessing()) {
        return;
    }

    std::ranges::fill(m_pHelpMessageToPrint, '\0');
    std::ranges::fill(m_pLastHelpMessage, '\0');
    std::ranges::fill(m_pHelpMessage, '\0');

    if (m_nHelpMessageState && CMessages::StringCompare(m_pHelpMessage, m_pHelpMessageToPrint, sizeof(m_pHelpMessage)))
        return;

    std::ranges::fill(m_pLastHelpMessage, '\0');
    m_nHelpMessageState = 0;
    m_bHelpMessageQuick = false;
    m_bHelpMessagePermanent = false;
    m_nHelpMessageStatId = statId;
    m_fHelpMessageStatUpdateValue = diff;
    m_nHelpMessageMaxStatValue = (uint32)max;
    sprintf_s(gString, state == STAT_UPDATE_INCREASE ? "+" : "-");
    AsciiToGxtChar(gString, m_pHelpMessage);
}

// 0x588E30
void CHud::SetHelpMessageWithNumber(const GxtChar* text, int32 number, bool quickMessage, bool permanent) {
    if (m_BigMessage[STYLE_MIDDLE_SMALLER_HIGHER][0] || CGarages::MessageIDString[0] || CReplay::Mode == MODE_PLAYBACK || CCutsceneMgr::IsCutsceneProcessing()) {
        return;
    }

    GxtChar str[400];
    CMessages::InsertNumberInString(text, number, -1, -1, -1, -1, -1, str);
    CMessages::GetStringLength(str);
    CMessages::StringCopy(m_pHelpMessage, str, sizeof(m_pHelpMessage));
    CMessages::InsertPlayerControlKeysInString(m_pHelpMessage);

    std::ranges::fill(m_pLastHelpMessage, '\0');
    if (permanent) {
        m_nHelpMessageState = 1;
        CMessages::StringCopy(m_pHelpMessageToPrint, m_pHelpMessage, sizeof(m_pHelpMessage));
        CMessages::StringCopy(m_pLastHelpMessage, m_pHelpMessage, sizeof(m_pHelpMessage));
    } else {
        m_nHelpMessageState = 0;
    }

    m_bHelpMessagePermanent = permanent;
    m_bHelpMessageQuick = quickMessage;
    m_nHelpMessageStatId = 0;
    m_nHelpMessageMaxStatValue = 1000;
    m_fHelpMessageStatUpdateValue = 0.0f;
}

// 0x588F50
void CHud::SetVehicleName(const GxtChar* name) {
    m_pVehicleName = name;
}

// 0x588BB0
void CHud::SetZoneName(const GxtChar* name, bool displayImmediately) {
    if (displayImmediately) {
        m_pZoneName = name;
        return;
    }
    if (CGame::currArea || m_ZoneState != NAME_DONT_SHOW) {
        return;
    }
    m_pZoneName = name;
}

// called each frame from Render2dStuff()
// 0x58FAE0
void CHud::Draw() {
    if (CReplay::Mode == MODE_PLAYBACK || CWeapon::ms_bTakePhoto || FrontEndMenuManager.m_bActivateMenuNextFrame || gbCineyCamProcessedOnFrame == CTimer::GetFrameCounter())
        return;

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,        RWRSTATE(rwFILTERNEAREST));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,    RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,            RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,             RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,            RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,    RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,        RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS,       RWRSTATE(rwTEXTUREADDRESSCLAMP));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,        RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,            RWRSTATE(rwSHADEMODEFLAT));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION,    RWRSTATE(rwALPHATESTFUNCTIONGREATER));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(NULL));

    if (!TheCamera.m_bWideScreenOn) {
        DrawCrossHairs();
        if (FrontEndMenuManager.m_bHudOn && CTheScripts::bDisplayHud) {
            DrawPlayerInfo();
            DrawWanted();
        }
        if (!bScriptDontDisplayVehicleName) {
            DrawVehicleName();
        }
        DrawMissionTimers();
    }

    if (!bScriptDontDisplayRadar && !TheCamera.m_bWideScreenOn) {
        CPed* player = FindPlayerPed();
        CPad* pad = CPad::GetPad();
        if (!pad->GetDisplayVitalStats(player) || FindPlayerVehicle()) {
            bDrawingVitalStats = false;
            DrawRadar();
        } else {
            bDrawingVitalStats = true;
            DrawVitalStats();
        }
        if (!CGameLogic::SkipCanBeActivated() || bDrawingVitalStats) {
            HelpTripSkipShown = false;
        } else {
            DrawTripSkip();
            if (!HelpTripSkipShown) {
                SetHelpMessage(TheText.Get("SKIP_1"), true, false, false);
                HelpTripSkipShown = true;
            }
        }
    }

    if (m_bDraw3dMarkers && !TheCamera.m_bWideScreenOn) {
        CRadar::Draw3dMarkers();
    }

    if (!CTimer::GetIsUserPaused()) {
        if (!m_BigMessage[STYLE_MIDDLE][0]) {
            if (CMenuSystem::GetNumMenusInUse()) {
                CMenuSystem::Process(CMenuSystem::MENU_UNDEFINED);
            }
            DrawScriptText(true);
        }
        if (CTheScripts::bDrawSubtitlesBeforeFade) {
            DrawSubtitles();
        }
        DrawHelpText();
        DrawOddJobMessage(true);
        DrawSuccessFailedMessage();
        DrawBustedWastedMessage();
    }
}

// 0x58D490
void CHud::DrawAfterFade() {
    ZoneScoped;

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERNEAREST));
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS,    RWRSTATE(rwTEXTUREADDRESSCLAMP));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));

    if (CTimer::GetIsUserPaused() || CReplay::Mode == MODE_PLAYBACK || CWeapon::ms_bTakePhoto)
        return;

    auto vehicle = FindPlayerVehicle();
    if (!vehicle || (!vehicle->IsSubPlane() && !vehicle->IsSubHeli())) {
        if (!CCutsceneMgr::ms_cutsceneProcessing) {
            if (!FrontEndMenuManager.m_bMenuActive && !TheCamera.m_bWideScreenOn && !bScriptDontDisplayAreaName) {
                DrawAreaName();
            }
        }
    }

    if (!m_BigMessage[STYLE_MIDDLE][0]) {
        DrawScriptText(false);
    }

    if (!CTheScripts::bDrawSubtitlesBeforeFade) {
        DrawSubtitles();
    }

    DrawMissionTitle();
    DrawOddJobMessage(false);
}

// 0x58AA50
void CHud::DrawAreaName() {
    if (!m_pZoneName) {
        return;
    }

    if (m_pZoneName != m_pLastZoneName) {
        switch (m_ZoneState) {
        case NAME_DONT_SHOW:
            if (!CTheScripts::bPlayerIsOffTheMap && CTheScripts::bDisplayHud ||
                CEntryExitManager::ms_exitEnterState == EXIT_ENTER_STATE_1 ||
                CEntryExitManager::ms_exitEnterState == EXIT_ENTER_STATE_2
            ) {
                m_ZoneState = NAME_FADE_IN;
                m_ZoneNameTimer = 0;
                m_ZoneFadeTimer = 0;
                m_ZoneToPrint = m_pZoneName;
                if (m_VehicleState == NAME_SHOW || m_VehicleState == NAME_FADE_IN) {
                    m_VehicleState = NAME_FADE_OUT;
                }
            }
            break;
        case NAME_SHOW:
        case NAME_FADE_IN:
        case NAME_FADE_OUT:
            m_ZoneState = NAME_SWITCH;
            m_ZoneNameTimer = 0;
            break;
        case NAME_SWITCH:
            m_ZoneNameTimer = 0;
            break;
        default:
            break;
        }
        m_pLastZoneName = m_pZoneName;
    }

    if (!m_ZoneState)
        return;

    float alpha = 255.0f;
    switch (m_ZoneState) {
    case NAME_SHOW:
        m_ZoneFadeTimer = 1000;
        if (m_ZoneNameTimer > 3000) {
            m_ZoneState = NAME_FADE_OUT;
            m_ZoneFadeTimer = 1000;
        }
        break;

    case NAME_FADE_IN:
        if (!TheCamera.GetFading() && TheCamera.GetScreenFadeStatus() != NAME_FADE_IN) {
            m_ZoneFadeTimer += (int32)CTimer::GetTimeStepInMS();
        }

        if (m_ZoneFadeTimer > 1000) {
            m_ZoneFadeTimer = 1000;
            m_ZoneState = NAME_SHOW;
        }

        if (TheCamera.GetScreenFadeStatus() != NAME_FADE_IN) {
            alpha = (float)m_ZoneFadeTimer / 1000.0f * 255.0f;
            break;
        }
        m_ZoneState = NAME_FADE_OUT;
        m_ZoneFadeTimer = 1000;
        break;

    case NAME_FADE_OUT:
        if (!TheCamera.GetFading() && TheCamera.GetScreenFadeStatus() != NAME_FADE_IN) {
            m_ZoneFadeTimer -= (int32)CTimer::GetTimeStepInMS();
        }

        if (m_ZoneFadeTimer < 0) {
            m_ZoneFadeTimer = 0;
            m_ZoneState = NAME_DONT_SHOW;
        }

        if (TheCamera.GetScreenFadeStatus() != NAME_FADE_IN) {
            alpha = (float)m_ZoneFadeTimer / 1000.0f * 255.0f;
            break;
        }
        m_ZoneFadeTimer = 1000;
        break;

    case NAME_SWITCH:
        m_ZoneFadeTimer -= (int32)CTimer::GetTimeStepInMS();
        if (m_ZoneFadeTimer < 0) {
            m_ZoneFadeTimer = 0;
            m_ZoneState = NAME_FADE_IN;
            m_ZoneToPrint = m_pLastZoneName;
        }
        alpha = (float)m_ZoneFadeTimer / 1000.0f * 255.0f;
        break;

    default:
        break;
    }

    if (m_Message[0] || BigMessageX[STYLE_BOTTOM_RIGHT] != 0.0f || BigMessageX[STYLE_WHITE_MIDDLE] != 0.0f) {
        m_ZoneState = NAME_FADE_OUT;
        return;
    }

    m_ZoneNameTimer += (uint32)CTimer::GetTimeStepInMS();
    CFont::SetProportional(true);
    CFont::SetBackground(false, false);
    CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(1.2f), SCREEN_SCALE_Y(1.9f));
    CFont::SetEdge(2);
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
    CFont::SetRightJustifyWrap(SCREEN_STRETCH_X(180.0f));
    CFont::SetDropColor({ 0, 0, 0, (uint8)alpha });
    CFont::SetFontStyle(FONT_GOTHIC);

    const CZoneInfo* info = CPopCycle::m_pCurrZoneInfo;
    const auto& color = info->ZoneColor; // Cppcheck: (warning) nullPointerRedundantCheck: Either the condition 'info' is redundant or there is possible null pointer dereference: info.
    if (CGangWars::bGangWarsActive && info && color.r && color.g && color.b) {
        CFont::SetColor({ color.r, color.g, color.b, (uint8)alpha});
    } else {
        CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_LIGHT_BLUE, (uint8)alpha));
    }

    CFont::PrintStringFromBottom(SCREEN_STRETCH_FROM_RIGHT(32.0f), SCREEN_SCALE_FROM_BOTTOM(104.0f) + SCREEN_SCALE_Y(76.0f), m_ZoneToPrint);
    CFont::SetSlant(0.0f);
}

// 0x58CA50
void CHud::DrawBustedWastedMessage() {
    auto& message      = m_BigMessage[STYLE_WHITE_MIDDLE];
    auto& messageX     = BigMessageX[STYLE_WHITE_MIDDLE];
    auto& messageAlpha = BigMessageAlpha[STYLE_WHITE_MIDDLE];

    if (!message[0]) {
        messageX = '\0';
        return;
    }

    if (messageX == 0.0f) {
        messageX = 1.0f;
        messageAlpha = 0.0f;

        if (m_VehicleState) {
            m_VehicleState = NAME_DONT_SHOW;
        }
        if (m_ZoneState) {
            m_ZoneState = NAME_DONT_SHOW;
        }
        return;
    }

    messageAlpha += CTimer::GetTimeStepInMS() * 0.4f;
    messageAlpha = std::min(messageAlpha, 255.0f);

    CFont::SetBackground(false, false);
    CFont::SetScale(SCREEN_STRETCH_X(2.1f), SCREEN_SCALE_Y(2.1f));
    CFont::SetProportional(true);
    CFont::SetJustify(false);
    CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
    CFont::SetFontStyle(FONT_GOTHIC);
    CFont::SetEdge(3);
    CFont::SetDropColor({ 0, 0, 0, (uint8)messageAlpha });
    CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_LIGHT_GRAY, (uint8)messageAlpha));
    CFont::PrintStringFromBottom(SCREEN_WIDTH / 2.0f, static_cast<float>(RsGlobal.maximumHeight / 2) - SCREEN_SCALE_Y(30.0f), message); // OG: posY static allocated var
}

// 0x589070
void CHud::ResetWastedText() {
    BigMessageX[STYLE_WHITE_MIDDLE] = 0.0f;
    m_BigMessage[STYLE_WHITE_MIDDLE][0] = '\0';

    BigMessageX[STYLE_MIDDLE] = 0.0f;
    m_BigMessage[STYLE_MIDDLE][0] = '\0';
}

// 0x58E020
// int-hud: body from batch_out/hud-1/0x58E020.cpp (replaces redirect stub + fixed repo draft).
// Fragment fixes vs the old draft: white crosshair colour (draft had test red), MODE_SNIPER branch
// handled (draft excluded sniper), aim condition uses m_bFreeAiming (not bIsRestoringLook), circle reset
// uses m_bGarageFixedCamPositionSet (not m_bTransitionState), numeric ped state literals 0x39/0x3B,
// circle branch requires MODE_AIMWEAPON{,_FROMCAR,_ATTACHED}, render-state restore moved out of
// the destructor to explicit tail calls (early returns in OG do not restore).
void CHud::DrawCrossHairs() {
    const auto player = FindPlayerPed();
    const auto& camMode = CCamera::GetActiveCamera().m_nMode;

    bool bDrawCircleCrossHair = false;
    bool bDrawCustomCrossHair = false;

    if (camMode == eCamMode::MODE_SNIPER) {
        if (player && !player->GetActiveWeapon().IsTypeMelee()) {
            bDrawCustomCrossHair = true;
        }
    } else {
        if (camMode == eCamMode::MODE_1STPERSON) {
            if (const CVehicle* vehicle = FindPlayerVehicle();
                vehicle && (vehicle->m_nModelIndex == eModelID::MODEL_HYDRA || vehicle->m_nModelIndex == eModelID::MODEL_HUNTER)
            ) {
                bDrawCustomCrossHair = true;
            }
        }
        if (camMode == eCamMode::MODE_ROCKETLAUNCHER || camMode == eCamMode::MODE_ROCKETLAUNCHER_HS
            || camMode == eCamMode::MODE_M16_1STPERSON || camMode == eCamMode::MODE_HELICANNON_1STPERSON
            || camMode == eCamMode::MODE_CAMERA
        ) {
            if (player && !player->GetActiveWeapon().IsTypeMelee()) {
                bDrawCustomCrossHair = true;
            }
        }
    }

    if (camMode == eCamMode::MODE_M16_1STPERSON_RUNABOUT || camMode == eCamMode::MODE_ROCKETLAUNCHER_RUNABOUT
        || camMode == eCamMode::MODE_ROCKETLAUNCHER_RUNABOUT_HS || camMode == eCamMode::MODE_SNIPER_RUNABOUT
    ) {
        bDrawCircleCrossHair = true;
    }

    const auto& activeWeapon = player->GetActiveWeapon();
    CTaskSimpleUseGun* useGunTask = player->GetIntelligence()->GetTaskUseGun();
    if (   !player->m_pTargetedObject
        && player->GetPlayerData()->m_bFreeAiming
        && (!useGunTask || !useGunTask->m_SkipAim)
        && (camMode == eCamMode::MODE_AIMWEAPON || camMode == eCamMode::MODE_AIMWEAPON_FROMCAR
            || camMode == eCamMode::MODE_AIMWEAPON_ATTACHED)
        && player->m_nPedState != (ePedState)57 // OG: 0x39
        && player->m_nPedState != (ePedState)59 // OG: 0x3B
        && (   (activeWeapon.m_Type >= eWeaponType::WEAPON_PISTOL && activeWeapon.m_Type <= eWeaponType::WEAPON_COUNTRYRIFLE)
            || activeWeapon.m_Type == eWeaponType::WEAPON_FLAMETHROWER
            || activeWeapon.m_Type == eWeaponType::WEAPON_MINIGUN)
    ) {
        bDrawCircleCrossHair = !(camMode == eCamMode::MODE_AIMWEAPON && TheCamera.m_bGarageFixedCamPositionSet);
    }

    if (!bDrawCustomCrossHair && !bDrawCircleCrossHair && CTheScripts::bDrawCrossHair == eCrossHairType::NONE) {
        return;
    }

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,  RWRSTATE(FALSE));

    const CRGBA white = CRGBA(255, 255, 255, 255); // OG: 0xFF,0xFF,0xFF,0xFF

    if (bDrawCircleCrossHair
        && (camMode == eCamMode::MODE_AIMWEAPON || camMode == eCamMode::MODE_AIMWEAPON_FROMCAR
            || camMode == eCamMode::MODE_AIMWEAPON_ATTACHED)
    ) {
        const float cx = SCREEN_WIDTH * CCamera::m_f3rdPersonCHairMultX;
        const float cy = SCREEN_HEIGHT * CCamera::m_f3rdPersonCHairMultY;
        const float gunRadius = player->GetWeaponRadiusOnScreen();

        if (gunRadius == 0.2f) {
            CSprite2d::DrawRect({ cx - 1.0f, cy - 1.0f, cx + 1.0f, cy + 1.0f }, white);
        }

        const float rx = SCREEN_STRETCH_X(64.0f) * gunRadius;
        const float ry = SCREEN_SCALE_Y(64.0f) * gunRadius;
        const float left   = cx - rx / 2.0f;
        const float right  = cx + rx / 2.0f;
        const float top    = cy - ry / 2.0f;
        const float bottom = cy + ry / 2.0f;

        Sprites[SPRITE_SITE_M16].Draw({ left, bottom, right, top }, white);
        Sprites[SPRITE_SITE_M16].Draw({ left + rx, bottom, right, top }, white);
        Sprites[SPRITE_SITE_M16].Draw({ left, bottom, right, top + ry }, white);
        Sprites[SPRITE_SITE_M16].Draw({ left + rx, bottom, right, top + ry }, white);
    } else if (CTheScripts::bDrawCrossHair == eCrossHairType::FIXED_DRAW_1STPERSON_WEAPON
        || (camMode != eCamMode::MODE_M16_1STPERSON && camMode != eCamMode::MODE_M16_1STPERSON_RUNABOUT
            && camMode != eCamMode::MODE_1STPERSON_RUNABOUT && camMode != eCamMode::MODE_HELICANNON_1STPERSON)
    ) {
        RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));

        RwTexture* drawTexture = nullptr;
        float sizeX = 0.0f;
        float sizeY = 0.0f;
        float offsetX = 0.0f;
        float offsetY = 0.0f;

        if (   activeWeapon.m_Type == eWeaponType::WEAPON_CAMERA
            || activeWeapon.m_Type == eWeaponType::WEAPON_SNIPERRIFLE
            || CTheScripts::bDrawCrossHair == eCrossHairType::FIXED_DRAW_1STPERSON_WEAPON
        ) {
            if (activeWeapon.m_Type == eWeaponType::WEAPON_CAMERA
                || CTheScripts::bDrawCrossHair == eCrossHairType::FIXED_DRAW_1STPERSON_WEAPON
            ) {
                sizeX = SCREEN_STRETCH_X(256.0f);
                sizeY = SCREEN_SCALE_Y(192.0f);
            } else {
                sizeX = SCREEN_STRETCH_X(210.0f);
                sizeY = SCREEN_SCALE_Y(210.0f);
            }

            const CWeaponInfo& info = *CWeaponInfo::GetWeaponInfo(activeWeapon.m_Type, eWeaponSkill::STD);
            if (info.m_nModelId1 <= 0) {
                return;
            }

            const CBaseModelInfo* mi = CModelInfo::GetModelInfo(info.m_nModelId1);
            const TxdDef* txd = CTxdStore::ms_pTxdPool->GetAt(mi->m_nTxdIndex);
            if (!txd->m_pRwDictionary) {
                return;
            }
            drawTexture = RwTexDictionaryFindHashNamedTexture(txd->m_pRwDictionary, CKeyGen::AppendStringToKey(mi->m_nKey, "CROSSHAIR"));
        } else {
            if (camMode != eCamMode::MODE_ROCKETLAUNCHER && camMode != eCamMode::MODE_1STPERSON
                && camMode != eCamMode::MODE_ROCKETLAUNCHER_RUNABOUT && camMode != eCamMode::MODE_ROCKETLAUNCHER_HS
                && camMode != eCamMode::MODE_ROCKETLAUNCHER_RUNABOUT_HS
            ) {
                return;
            }
            drawTexture = Sprites[SPRITE_SITE_ROCKET].m_pTexture;
            sizeX   = SCREEN_STRETCH_X(24.0f);
            sizeY   = SCREEN_SCALE_Y(24.0f);
            offsetX = SCREEN_STRETCH_X(20.0f);
            offsetY = SCREEN_SCALE_Y(20.0f);
        }

        if (drawTexture) {
            RwRenderStateSet(rwRENDERSTATEZTESTENABLE,    RWRSTATE(FALSE));
            RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, RWRSTATE(RwTextureAddressMode::rwTEXTUREADDRESSCLAMP));
            RwRenderStateSet(rwRENDERSTATETEXTURERASTER,  RWRSTATE(drawTexture->raster));

            const auto RenderOneXLUSprite = [=](float x, float y, auto u, auto v) {
                CSprite::RenderOneXLUSprite(
                    { x, y, 1.0f },
                    { sizeX / 2.0f, sizeY / 2.0f },
                    255u, 255u, 255u, 255,
                    0.01f,
                    255,
                    u, v
                );
            };

            RenderOneXLUSprite((float)(RsGlobal.maximumWidth / 2) - sizeX / 2.0f - offsetX,
                               (float)(RsGlobal.maximumHeight / 2) - sizeY / 2.0f - offsetY, 0, 0);
            RenderOneXLUSprite((float)(RsGlobal.maximumWidth / 2) + sizeX / 2.0f + offsetX,
                               (float)(RsGlobal.maximumHeight / 2) - sizeY / 2.0f - offsetY, 1, 0);
            RenderOneXLUSprite((float)(RsGlobal.maximumWidth / 2) - sizeX / 2.0f - offsetX,
                               (float)(RsGlobal.maximumHeight / 2) + sizeY / 2.0f + offsetY, 0, 1);
            RenderOneXLUSprite((float)(RsGlobal.maximumWidth / 2) + sizeX / 2.0f + offsetX,
                               (float)(RsGlobal.maximumHeight / 2) + sizeY / 2.0f + offsetY, 1, 1);

            RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(FALSE));
        }
    } else {
        // M16/heli-cannon crosshair: 4 SITE_M16 tiles around the screen center
        RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));
        RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,  RWRSTATE(FALSE));

        const float rx = SCREEN_STRETCH_X(64.0f);
        const float ry = SCREEN_SCALE_Y(64.0f);

        CRect rect;
        rect.left   = (float)(RsGlobal.maximumWidth / 2) - rx / 2.0f;
        rect.bottom = (float)(RsGlobal.maximumHeight / 2) - ry / 2.0f;
        rect.right  = rect.left + rx / 2.0f;
        rect.top    = rect.bottom + ry / 2.0f;
        Sprites[SPRITE_SITE_M16].Draw(rect, white);

        rect.left = (float)(RsGlobal.maximumWidth / 2) + rx / 2.0f;
        Sprites[SPRITE_SITE_M16].Draw(rect, white);

        rect.left   = (float)(RsGlobal.maximumWidth / 2) - rx / 2.0f;
        rect.bottom = ry + ((float)(RsGlobal.maximumHeight / 2) - ry / 2.0f);
        Sprites[SPRITE_SITE_M16].Draw(rect, white);

        rect.left = (float)(RsGlobal.maximumWidth / 2) + rx / 2.0f;
        Sprites[SPRITE_SITE_M16].Draw(rect, white);
    }

    RwRenderStateSet(rwRENDERSTATESRCBLEND,     RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,    RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(TRUE));
}

// 0x58D580
float CHud::DrawFadeState(DRAW_FADE_STATE fadingElement, int32 forceFadingIn) {
    uint32 state, timer, fadeTimer;
    switch (fadingElement) {
    case WANTED_STATE:
        fadeTimer = m_WantedFadeTimer;
        state = m_WantedState;
        timer = m_WantedTimer;
        break;
    case ENERGY_LOST_STATE:
        fadeTimer = m_EnergyLostFadeTimer;
        state = m_EnergyLostState;
        timer = m_EnergyLostTimer;
        break;
    case DISPLAY_SCORE_STATE:
        fadeTimer = m_DisplayScoreFadeTimer;
        state = m_DisplayScoreState;
        timer = m_DisplayScoreTimer;
        break;
    case WEAPON_STATE:
        fadeTimer = m_WeaponFadeTimer;
        state = m_WeaponState;
        timer = m_WeaponTimer;
        break;
    default:
        state = fadingElement;
        timer = fadingElement;
        fadeTimer = fadingElement;
        break;
    }

    if (forceFadingIn) {
        switch (state) {
        case NAME_DONT_SHOW:
            fadeTimer = 0;
            break;
        case NAME_SWITCH:
        case NAME_FADE_OUT:
            timer = 5;
            state = NAME_FADE_IN;
            break;
        default:
            break;
        }
    }

    float alpha = 255.0f;
    if (state != NAME_DONT_SHOW) {
        switch (state) {
        case NAME_SHOW:
            fadeTimer = 1000;
            if (timer > 10'000) {
                fadeTimer = 3000;
                state = NAME_FADE_OUT;
            }
            break;
        case NAME_FADE_IN:
            fadeTimer += (uint32)CTimer::GetTimeStepInMS();
            if (fadeTimer > 1000) {
                state = NAME_SHOW;  
                fadeTimer = 1000;
            }
            alpha = float(fadeTimer) / 1000.0f * 255.0f;
            break;
        case NAME_FADE_OUT:
            fadeTimer -= (uint32)CTimer::GetTimeStepInMS();
            if (fadeTimer < 0) {
                fadeTimer = 0;
                state = NAME_DONT_SHOW;
            }
            alpha = float(fadeTimer) / 1000.0f * 255.0f;
            break;
        default:
            break;
        }
        timer += (uint32)CTimer::GetTimeStepInMS();
    }

    switch (fadingElement) {
    case WANTED_STATE:
        m_WantedFadeTimer = fadeTimer;
        m_WantedState = state;
        m_WantedTimer = timer;
        break;
    case ENERGY_LOST_STATE:
        m_EnergyLostFadeTimer = fadeTimer;
        m_EnergyLostState = state;
        m_EnergyLostTimer = timer;
        break;
    case DISPLAY_SCORE_STATE:
        m_DisplayScoreFadeTimer = fadeTimer;
        m_DisplayScoreState = state;
        m_DisplayScoreTimer = timer;
        break;
    case WEAPON_STATE:
        m_WeaponFadeTimer = fadeTimer;
        m_WeaponState = state;
        m_WeaponTimer = timer;
        break;
    default:
        break;
    }

    return std::clamp(alpha, 0.0f, 255.0f);
}

// 0x58B6E0
// int-hud: body from batch_out/hud-1/0x58B6E0.cpp (replaces redirect stub).
// int-hud fixes vs fragment: (1) CAEAudioEngine -> global AudioEngine (repo class is CAudioEngine,
// ReportFrontendAudioEvent is a non-static member, see Audio/AudioEngine.h:89,172);
// (2) explicit (eStats) cast for CStats::GetStatValue(m_nHelpMessageStatId) (uint16 -> enum).
void CHud::DrawHelpText() {
    if (m_pHelpMessage[0] == '\0') {
        m_nHelpMessageState = 0;
        return;
    }

    if (!CMessages::StringCompare(m_pHelpMessage, m_pLastHelpMessage, 400)) {
        switch (m_nHelpMessageState) {
        case 0: {
            m_nHelpMessageState = 2;
            m_nHelpMessageTimer = 0;
            m_nHelpMessageFadeTimer = 0;
            CMessages::StringCopy(m_pHelpMessageToPrint, m_pHelpMessage, 400);
            CFont::SetOrientation(eFontAlignment::ALIGN_LEFT); // 1
            CFont::SetJustify(0);
            CFont::SetWrapx(SCREEN_STRETCH_X(230.0f)); // OG: 34 + 200 - 4
            CFont::SetFontStyle(eFontStyle::FONT_SUBTITLES); // 1
            CFont::SetBackground(true, true);
            CFont::SetDropShadowPosition(0);
            m_fHelpMessageTime = (float)(CFont::GetNumberLines(SCREEN_STRETCH_X(34.0f), SCREEN_SCALE_Y(28.0f), m_pHelpMessageToPrint) + 3);
            CFont::SetWrapx(SCREEN_WIDTH);
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_DISPLAY_INFO, 0.0f, 1.0f); // int-hud: was CAEAudioEngine::...
            break;
        }
        case 1:
        case 2:
        case 3:
        case 4:
            m_nHelpMessageState = 4;
            m_nHelpMessageTimer = 5;
            break;
        }
        CMessages::StringCopy(m_pLastHelpMessage, m_pHelpMessage, 400);
    }

    float boxWidth = 200.0f;
    switch (m_nHelpMessageState) {
    case 1:
        boxWidth = 200.0f;
        m_nHelpMessageFadeTimer = 600;
        if (!m_bHelpMessagePermanent
            && (m_fHelpMessageTime * 1000.0f < (float)m_nHelpMessageTimer
                || (m_bHelpMessageQuick && 3000.0f < (float)m_nHelpMessageTimer))
        ) {
            m_nHelpMessageState = 3;
            m_nHelpMessageFadeTimer = 600;
        }
        break;
    case 2:
        if (!TheCamera.m_bWideScreenOn) {
            m_nHelpMessageFadeTimer += 2 * (int)CTimer::GetTimeStepInMS(); // OG: signed int32, fades-in towards zero
            if (0.0f < (float)(int32)m_nHelpMessageFadeTimer) {
                m_nHelpMessageFadeTimer = 0;
                m_nHelpMessageState = 1;
            }
            boxWidth = (float)(int32)m_nHelpMessageFadeTimer * 0.001f * 200.0f;
        }
        break;
    case 3:
        m_nHelpMessageFadeTimer -= 2 * (int)CTimer::GetTimeStepInMS();
        if ((float)(int32)m_nHelpMessageFadeTimer < 0.0f || TheCamera.m_bWideScreenOn) {
            m_nHelpMessageFadeTimer = 0;
            m_nHelpMessageState = 0;
        }
        boxWidth = (float)(int32)m_nHelpMessageFadeTimer * 0.001f * 200.0f;
        break;
    case 4:
        m_nHelpMessageFadeTimer -= 2 * (int)CTimer::GetTimeStepInMS();
        if ((float)(int32)m_nHelpMessageFadeTimer < 0.0f) {
            m_nHelpMessageFadeTimer = 0;
            m_nHelpMessageState = 2;
            CMessages::StringCopy(m_pHelpMessageToPrint, m_pLastHelpMessage, 400);
        }
        boxWidth = (float)(int32)m_nHelpMessageFadeTimer * 0.001f * 200.0f;
        break;
    }

    if (CCutsceneMgr::ms_running) {
        return;
    }

    m_nHelpMessageTimer += (int)CTimer::GetTimeStepInMS();

    CFont::SetAlphaFade(boxWidth);
    CFont::SetProportional(true);
    CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(0.52f), SCREEN_SCALE_Y(1.1f));

    float textX;
    float textY;
    if (m_nHelpMessageStatId == 0) {
        // Regular help message
        if (m_BigMessage[0][0] != '\0' || m_BigMessage[STYLE_MIDDLE_SMALLER_HIGHER][0] != '\0' || CGarages::MessageIDString[0] != '\0') {
            return;
        }

        CFont::SetOrientation(eFontAlignment::ALIGN_LEFT); // 1
        CFont::SetJustify(0);
        const float xScale = SCREEN_STRETCH_X(1.0f);
        float wrap;
        if (xScale * m_fHelpMessageBoxWidth == 200.0f * xScale) {
            wrap = (xScale * 34.0f + 200.0f * xScale) - xScale * 4.0f;
        } else {
            wrap = xScale * 34.0f + (m_fHelpMessageBoxWidth - 4.0f) * xScale;
        }
        CFont::SetWrapx(wrap);
        CFont::SetFontStyle(eFontStyle::FONT_SUBTITLES); // 1
        CFont::SetBackground(true, true);
        CFont::SetDropShadowPosition(0);
        CFont::SetBackgroundColor(CRGBA(0, 0, 0, (uint8)(int)boxWidth));
        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY)); // idx 4

        textY = SCREEN_SCALE_Y(28.0f)
            + ((float)((TheCamera.m_bWideScreenOn && !FrontEndMenuManager.m_bWidescreenOn ? 0x38 : 0) + 150) - PagerXOffset) * 0.6f * SCREEN_SCALE_Y(1.0f);
        textX = SCREEN_STRETCH_X(34.0f);
    } else {
        // Stat update message (generated by SetHelpMessageStatUpdate)
        if (TheCamera.m_bWideScreenOn) {
            return;
        }

        if (m_nHelpMessageStatId < 10) {
            sprintf_s(gString, "STAT00%d", m_nHelpMessageStatId);
        } else if (m_nHelpMessageStatId < 100) {
            sprintf_s(gString, "STAT0%d", m_nHelpMessageStatId);
        } else {
            sprintf_s(gString, "STAT%d", m_nHelpMessageStatId);
        }

        CFont::SetOrientation(eFontAlignment::ALIGN_LEFT); // 1
        CFont::SetJustify(0);
        CFont::SetWrapx(SCREEN_WIDTH);
        CFont::SetFontStyle(eFontStyle::FONT_SUBTITLES); // 1
        CFont::SetBackground(true, true);
        CFont::SetDropShadowPosition(0);
        CFont::SetBackgroundColor(CRGBA(0, 0, 0, (uint8)(int)boxWidth));
        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY)); // idx 4

        const float xScale = SCREEN_STRETCH_X(1.0f);
        const float statLabelWidth = CFont::GetStringWidth(TheText.Get(gString), true, false);
        const float barX = xScale * 34.0f + statLabelWidth + xScale * 10.0f;
        CFont::SetWrapx(xScale * 75.0f + barX);

        const float yScale = SCREEN_SCALE_Y(1.0f);
        CFont::PrintString(
            SCREEN_STRETCH_X(34.0f),
            yScale * 28.0f + (150.0f - PagerXOffset) * 0.6f * yScale,
            TheText.Get(gString)
        );

        AsciiToGxtChar("+", gGxtString);
        float statValue;
        if (m_nHelpMessageStatId == STAT_GANG_STRENGTH) { // 0x150 - gang strength = player group member count
            statValue = (float)CPedGroups::ms_groups[FindPlayerPed()->GetPlayerData()->m_nPlayerGroup].GetMembership().CountMembersExcludingLeader();
        } else {
            statValue = CStats::GetStatValue((eStats)m_nHelpMessageStatId); // int-hud: explicit cast added
        }

        const float alpha     = (float)(int)boxWidth;
        const float frac      = 1.0f / (float)m_nHelpMessageMaxStatValue;
        const bool  bIncrease = m_pHelpMessageToPrint[0] == gGxtString[0];
        const CRGBA colBar     = bIncrease
            ? HudColour.GetRGBA(HUD_COLOUR_GREEN, (uint8)alpha)      // idx 1
            : HudColour.GetRGBA(HUD_COLOUR_RED, (uint8)alpha);       // idx 0
        const CRGBA colBack    = HudColour.GetRGBA(HUD_COLOUR_LIGHT_GRAY, (uint8)alpha); // idx 4

        CSprite2d::DrawBarChart(
            barX,
            yScale * 28.0f + (155.0f - PagerXOffset) * 0.6f * yScale,
            (uint16)(int)SCREEN_STRETCH_X(62.0f),
            (uint8)(int)SCREEN_SCALE_Y(12.0f),
            std::max(frac * statValue * 100.0f, 2.0f),
            (int8)(int)std::max(frac * m_fHelpMessageStatUpdateValue * 100.0f, 3.0f),
            0,
            0,
            colBack,
            colBar
        );

        textY = yScale * 28.0f + (150.0f - PagerXOffset) * 0.6f * yScale;
        textX = barX + SCREEN_STRETCH_X(65.0f);
    }

    CFont::PrintString(textX, textY, m_pHelpMessageToPrint);
    CFont::SetWrapx(SCREEN_WIDTH);
    CFont::SetAlphaFade(255.0f);
}

// 0x58B180
// int-hud: body from batch_out/hud-1/0x58B180.cpp (replaces redirect stub).
// ABI note (from fragment): GetYPosBasedOnHealth is applied twice (player 1 and player 2);
// player 2's m_nMaxHealth drives the offset in the else-branch (0x58B466) too.
void CHud::DrawMissionTimers() {
    if ((m_BigMessage[STYLE_MIDDLE_SMALLER_HIGHER][0] == '\0' || bScriptForceDisplayWithCounters) // 0xBAAEC0 = m_BigMessage[4][0]
        && CGarages::MessageIDString[0] == '\0') {
        const auto& onscnTimer = CUserDisplay::OnscnTimer;
        auto& clock            = onscnTimer.m_Clock;

        float clockY    = GetYPosBasedOnHealth(CWorld::PlayerInFocus, SCREEN_SCALE_Y(148.0f), 12);
        clockY          = GetYPosBasedOnHealth(1, clockY, 12);
        float countersY = GetYPosBasedOnHealth(CWorld::PlayerInFocus, SCREEN_SCALE_Y(148.0f) + SCREEN_SCALE_Y(20.0f), 12);
        countersY       = GetYPosBasedOnHealth(1, countersY, 12);
        if (CWorld::Players[1].m_pPed != nullptr) { // 0xB7CF28
            clockY    += SCREEN_SCALE_Y(72.0f);
            countersY += SCREEN_SCALE_Y(72.0f);
        }

        CFont::SetProportional(true);
        CFont::SetBackground(false, false);
        CFont::SetScale(SCREEN_STRETCH_X(0.5f), SCREEN_SCALE_Y(1.0f));
        CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT); // 2
        CFont::SetRightJustifyWrap(0.0f);
        CFont::SetFontStyle(eFontStyle::FONT_MENU); // 2
        CFont::SetWrapx(SCREEN_WIDTH);
        CFont::SetEdge(2);
        CFont::SetDropColor(CRGBA(0, 0, 0, 255));

        // While player 2 exists and the clock hasn't been displayed yet - reset the flag (0x58B2EA)
        if (CWorld::Players[1].m_pPed != nullptr && !clock.m_bEnabled) {
            TimerMainCounterWasDisplayed = false;
        }
        for (auto i = 0u; i < onscnTimer.NUM_COUNTERS; i++) {
            if (!onscnTimer.m_aCounters[i].m_bEnabled) {
                TimerCounterWasDisplayed[i] = 0;
            }
        }

        if (onscnTimer.m_bDisplay == 1) {
            if (clock.m_bEnabled == 1) {
                if (!TimerMainCounterWasDisplayed) {
                    TimerMainCounterHideState = 1;
                }
                TimerMainCounterWasDisplayed = true;
                if (TimerMainCounterHideState != 0 && ++TimerMainCounterHideState > 50) {
                    TimerMainCounterHideState = 0;
                }

                CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY)); // idx 4
                if ((CTimer::m_FrameCounter & 4) || TimerMainCounterHideState == 0) {
                    GxtChar text[200];
                    AsciiToGxtChar(clock.m_szDisplayedText, text);
                    CFont::PrintString(SCREEN_WIDTH - SCREEN_STRETCH_X(32.0f), clockY, text);
                    if (clock.m_szDescriptionTextKey[0] != '\0') {
                        CFont::PrintString(
                            (SCREEN_WIDTH - SCREEN_STRETCH_X(32.0f)) - SCREEN_STRETCH_X(90.0f),
                            clockY,
                            TheText.Get(clock.m_szDescriptionTextKey)
                        );
                    }
                }
            } else {
                // Clock disabled - counters base one line higher and no +72 shift (0x58B42A)
                countersY = GetYPosBasedOnHealth(CWorld::PlayerInFocus, SCREEN_SCALE_Y(148.0f), 12);
                if ((float)CWorld::Players[1].m_nMaxHealth < 101.0f) {
                    countersY -= SCREEN_SCALE_Y(12.0f);
                }
                if (CWorld::Players[1].m_pPed != nullptr) {
                    countersY += SCREEN_SCALE_Y(72.0f);
                }
            }

            for (auto i = 0u; i < onscnTimer.NUM_COUNTERS; i++) {
                auto& counter = onscnTimer.m_aCounters[i];
                if (counter.m_bEnabled == 1) {
                    if (TimerCounterWasDisplayed[i] == 0 && counter.m_bFlashWhenFirstDisplayed == 1) {
                        TimerCounterHideState[i] = 1;
                    }
                    const auto hideState = TimerCounterHideState[i];
                    TimerCounterWasDisplayed[i] = 1;
                    if (hideState != 0) {
                        TimerCounterHideState[i] = hideState + 1;
                        if (TimerCounterHideState[i] > 50) {
                            TimerCounterHideState[i] = 0;
                        }
                    }

                    if ((CTimer::m_FrameCounter & 4) || TimerCounterHideState[i] == 0) {
                        CFont::SetColor(HudColour.GetRGB(counter.m_nColourId));

                        // OG: 20.0f * scaleY * i * scaleY (double scale - as in the original, 0x58B53F)
                        const float rowY = countersY + SCREEN_SCALE_Y(20.0f) * (float)i * SCREEN_SCALE_Y(1.0f);

                        if (counter.m_nType == eOnscreenCounter::LINE) { // 1 - numeric counter with a bar
                            const auto counterValue = atol(counter.m_szDisplayedText);
                            CSprite2d::DrawBarChart(
                                SCREEN_WIDTH - SCREEN_STRETCH_X(32.0f) - SCREEN_STRETCH_X(61.0f),
                                rowY + SCREEN_SCALE_Y(6.0f),
                                (uint16)(int)SCREEN_STRETCH_X(61.0f),
                                (uint8)(int)SCREEN_SCALE_Y(9.0f),
                                (float)counterValue * 0.01f * 100.0f,
                                0, 0, 1,
                                HudColour.GetRGB(counter.m_nColourId),
                                CRGBA(0, 0, 0, 0)
                            );
                        } else {
                            GxtChar text[200];
                            AsciiToGxtChar(counter.m_szDisplayedText, text);
                            CFont::PrintString(SCREEN_WIDTH - SCREEN_STRETCH_X(32.0f), rowY, text);
                        }
                        if (counter.m_szDescriptionTextKey[0] != '\0') {
                            CFont::PrintString(
                                (SCREEN_WIDTH - SCREEN_STRETCH_X(32.0f)) - SCREEN_STRETCH_X(90.0f),
                                rowY,
                                TheText.Get(counter.m_szDescriptionTextKey)
                            );
                        }
                    }
                }
            }
        }
    }
}

// 0x58D240
void CHud::DrawMissionTitle() {
    auto& message      = m_BigMessage[STYLE_BOTTOM_RIGHT];
    auto& messageX     = BigMessageX[STYLE_BOTTOM_RIGHT];
    auto& messageAlpha = BigMessageAlpha[STYLE_BOTTOM_RIGHT];
    auto& messageInUse = BigMessageInUse[STYLE_BOTTOM_RIGHT];

    if (!message[0]) {
        messageX = 0.0f;
        return;
    }

    if (messageX == 0.0f) {
        messageInUse = -60.0f;
        messageX = 1.0f;
        m_ZoneState = NAME_DONT_SHOW;
        m_ZoneFadeTimer = 0;
        SetHelpMessage(nullptr, true, false, false);
        return;
    }

    CFont::SetBackground(false, false);
    CFont::SetProportional(true);
    CFont::SetRightJustifyWrap(0.0f);
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
    CFont::SetFontStyle(FONT_PRICEDOWN);
    CFont::SetScale(SCREEN_STRETCH_X(1.0f), SCREEN_SCALE_Y(1.3f));

    if (messageInUse >= SCREEN_WIDTH - 20.0f) { // magic shit
        messageX += CTimer::GetTimeStep();
        if (messageX >= 120.0f) {
            messageX = 120.0f;
            messageAlpha -= CTimer::GetTimeStepInMS();
        }
        if (messageAlpha <= 0.0f) {
            messageAlpha = 0.0f;
            message[0] = '\0';
            messageX = 0.0f;
        }
    } else {
        messageAlpha = 255.0f;
        messageInUse += CTimer::GetTimeStepInMS() * 0.3f;
    }

    CFont::SetEdge(2);
    CFont::SetDropColor({ 0, 0, 0, uint8(messageAlpha) });
    CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_GOLD, (uint8)messageAlpha));
    CFont::PrintStringFromBottom(SCREEN_SCALE_FROM_RIGHT(20.0f), SCREEN_SCALE_FROM_BOTTOM(115.0f), message);
    CFont::SetEdge(0);
}

// It looks like the original, but needs to be recheck
// 0x58CC80
void CHud::DrawOddJobMessage(bool displayImmediately) {
    const auto& m1 = m_BigMessage[STYLE_BOTTOM_RIGHT];
    const auto& m4 = m_BigMessage[STYLE_MIDDLE_SMALLER_HIGHER];
    if (displayImmediately == CTheScripts::bDrawOddJobTitleBeforeFade && !m1[0] && m4[0]) {
        CFont::SetBackground(false, false);
        CFont::SetJustify(false);
        CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(0.6f), SCREEN_SCALE_Y(1.35f));
        CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
        CFont::SetProportional(true);
        CFont::SetCentreSize(SCREEN_STRETCH_X(350.0f));
        CFont::SetFontStyle(FONT_MENU);
        CFont::SetEdge(2);
        CFont::SetDropColor({ 0, 0, 0, 255 });
        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_GOLD));
        CFont::PrintStringFromBottom(static_cast<float>(RsGlobal.maximumWidth / 2), SCREEN_STRETCH_Y(140.0f), m4);
    }

    if (!displayImmediately)
        return;

    const auto& m6 = m_BigMessage[STYLE_LIGHT_BLUE_TOP];
    if (m6[0]) {
        CFont::SetBackground(false, false);
        CFont::SetJustify(false);
        CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(1.0f), SCREEN_SCALE_Y(1.8f));
        CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
        CFont::SetProportional(true);
        CFont::SetCentreSize(SCREEN_STRETCH_X(500.0f));
        CFont::SetFontStyle(FONT_PRICEDOWN);
        CFont::SetEdge(2);
        CFont::SetDropColor({ 0, 0, 0, 255 });
        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_BLUE));
        CFont::PrintString(static_cast<float>(RsGlobal.maximumWidth / 2), SCREEN_STRETCH_Y(60.0f), m6);
    }

    const auto& m3 = m_BigMessage[STYLE_MIDDLE_SMALLER];
    if (m3[0]) {
        CFont::SetBackground(false, false);
        CFont::SetJustify(false);
        CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(0.6f), SCREEN_SCALE_Y(1.35f));
        CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
        CFont::SetProportional(true);
        CFont::SetCentreSize(SCREEN_STRETCH_X(500.0f));
        CFont::SetFontStyle(FONT_MENU);
        CFont::SetEdge(2);
        CFont::SetDropColor({ 0, 0, 0, 255 });
        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_GOLD));
        CFont::PrintString(static_cast<float>(RsGlobal.maximumWidth / 2), SCREEN_STRETCH_Y(155.0f), m3);
    }

    if (OddJob2OffTimer > 0.0f) {
        OddJob2OffTimer -= CTimer::GetTimeStepInMS();
    }

    const auto& m5 = m_BigMessage[STYLE_WHITE_MIDDLE_SMALLER];
    if (!m5[0])
        return;

    if (OddJob2OffTimer > 0.0f)
        return;

    switch (OddJob2On) {
    case 0:
        OddJob2XOffset = 380.0f;
        OddJob2On = 1;
        break;
    case 1:
        if (OddJob2XOffset <= 2.0f) {
            OddJob2On = 2;
            OddJob2Timer = 0;
        } else {
            OddJob2XOffset -= std::min(OddJob2XOffset / 6.0f, 40.0f);
        }
        break;
    case 2:
        OddJob2Timer += (uint16)CTimer::GetTimeStepInMS();
        if (OddJob2Timer > 1500) {
            OddJob2On = 3;
        }
        break;
    case 3:
        OddJob2XOffset -= std::max(OddJob2XOffset / 5.0f, 30.0f);
        if (OddJob2XOffset < -380.0f) {
            OddJob2On = 0;
            OddJob2OffTimer = 5000.0f;
        }
        break;
    default:
        break;
    }

    if (!m1[0]) {
        CFont::SetBackground(false, false);
        CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(0.6f), SCREEN_SCALE_Y(1.35f));
        CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
        CFont::SetProportional(true);
        CFont::SetCentreSize(SCREEN_STRETCH_X(500.0f));
        CFont::SetFontStyle(FONT_MENU);
        CFont::SetEdge(2);
        CFont::SetDropColor({ 0, 0, 0, 255 });
        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY));
        CFont::PrintString(static_cast<float>(RsGlobal.maximumWidth / 2), SCREEN_STRETCH_Y(217.0f), m5);
    }
}

// 0x58A330
void CHud::DrawRadar() {
    if (CEntryExitManager::ms_exitEnterState == EXIT_ENTER_STATE_1 ||
        CEntryExitManager::ms_exitEnterState == EXIT_ENTER_STATE_2 ||
        FrontEndMenuManager.m_nRadarMode == eRadarMode::RADAR_MODE_OFF ||
        (m_ItemToFlash == ITEM_RADAR && EachFrames(8))
    ) {
        return;
    }

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,     RWRSTATE(rwFILTERNEAREST));

    CRadar::DrawMap();

    if (FrontEndMenuManager.m_nRadarMode == eRadarMode::RADAR_MODE_BLIPS_ONLY) {
        CRadar::DrawBlips();
        return;
    }

    CVehicle* vehicle = FindPlayerVehicle();
    CRect rect;
    if (vehicle && vehicle->IsSubPlane() && vehicle->m_nModelIndex != MODEL_VORTEX) {
        float angle = PI - std::atan2(-vehicle->m_matrix->GetRight().z, vehicle->m_matrix->GetUp().z);
        CRadar::DrawRotatingRadarSprite(
            Sprites[SPRITE_RADAR_RING_PLANE],
            SCREEN_STRETCH_X(87.0f),
            SCREEN_STRETCH_FROM_BOTTOM(66.0f),
            angle,
            (uint32)SCREEN_STRETCH_X(78.0f),
            (uint32)SCREEN_SCALE_Y(59.0f),
            CRGBA(255, 255, 255, 255)
        );
    }

    CPlayerPed* player = FindPlayerPed();
    // Draws Altimeter on Planes And Helis or when parachuting down
    if (vehicle && (vehicle->IsSubPlane() || vehicle->IsSubHeli() && vehicle->m_nModelIndex != MODEL_VORTEX)
        || player->GetActiveWeapon().m_Type == WEAPON_PARACHUTE
    ) {
        rect.left   = SCREEN_STRETCH_X(40.0f) - SCREEN_STRETCH_X(20.0f);
        rect.bottom    = SCREEN_STRETCH_FROM_BOTTOM(104.0f);
        rect.right  = SCREEN_STRETCH_X(40.0f) - SCREEN_STRETCH_X(10.0f);
        rect.top = SCREEN_STRETCH_Y(76.0f) + SCREEN_STRETCH_FROM_BOTTOM(104.0f);
        CSprite2d::DrawRect(rect, { 10, 10, 10, 100 }); // rectangle

        const CVector& pos = vehicle ? vehicle->GetPosition() : player->GetPosition();
        auto lineY = 950.0f;
        if (pos.z <= 200.0f) {
            lineY = 200.0f;
        };
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(NULL));

        rect.left   = SCREEN_STRETCH_X(40.0f) - SCREEN_STRETCH_X(25.0f);
        rect.bottom    = SCREEN_STRETCH_FROM_BOTTOM(104.0f) + SCREEN_STRETCH_Y(76.0f) - std::min(SCREEN_STRETCH_Y(76.0f), SCREEN_STRETCH_Y(76.0f) * pos.z / lineY);
        rect.right  = SCREEN_STRETCH_X(40.0f) - 5.0f;
        rect.top = rect.bottom + 2.0f;
        CSprite2d::DrawRect(rect, { 200, 200, 200, 200 }); // horizontal line (current height)
    }

    // NOTSA: rects are optimized
    const auto black = CRGBA(0, 0, 0, 255);

    rect.left   = SCREEN_STRETCH_X(36.0f);
    rect.bottom    = SCREEN_STRETCH_FROM_BOTTOM(108.0f);
    rect.right  = SCREEN_STRETCH_X(87.0f);
    rect.top = SCREEN_STRETCH_FROM_BOTTOM(66.0f);
    Sprites[SPRITE_RADAR_DISC].Draw(rect, black); // top left

    rect.bottom = SCREEN_STRETCH_FROM_BOTTOM(24.0f);
    Sprites[SPRITE_RADAR_DISC].Draw(rect, black); // bottom left

    rect.left = SCREEN_STRETCH_X(138.0f);
    rect.bottom  = SCREEN_STRETCH_FROM_BOTTOM(108.0f);
    Sprites[SPRITE_RADAR_DISC].Draw(rect, black); // top right

    rect.bottom = SCREEN_STRETCH_FROM_BOTTOM(24.0f);
    Sprites[SPRITE_RADAR_DISC].Draw(rect, black); // bottom right

    CRadar::DrawBlips();
}

// 0x58C080
void CHud::DrawScriptText(bool isBeforeFade) {
    CTheScripts::DrawScriptSpritesAndRectangles(isBeforeFade);

    for (auto& t : CTheScripts::IntroTextLines) {
        if (!t.GXTKey[0]) { /* empty key? */
            continue;
        }
        if (t.IsDrawBeforeFade != isBeforeFade) {
            continue;
        }

        CFont::SetScale(SCREEN_SCALE_X(t.Scale.x), SCREEN_SCALE_Y(t.Scale.y / 2.0f));
        CFont::SetColor(t.Color);
        CFont::SetJustify(t.Justify);
        if (t.HasRightJustify) {
            CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
        } else {
            CFont::SetOrientation(t.IsCentered ? eFontAlignment::ALIGN_CENTER : eFontAlignment::ALIGN_LEFT);
        }
        CFont::SetWrapx(SCREEN_SCALE_X(t.WrapX));
        CFont::SetCentreSize(SCREEN_SCALE_X(t.CentreSize));
        CFont::SetBackground(t.HasBg, false);
        CFont::SetBackgroundColor(t.BgColor);
        CFont::SetProportional(t.IsProportional);
        CFont::SetDropColor(t.DropShadowColor);
        if (t.TextEdge) {
            CFont::SetEdge(t.TextEdge);
        } else {
            CFont::SetDropShadowPosition(t.DropShadow);
        }
        CFont::SetFontStyle((eFontStyle)t.FontStyle);

        GxtChar text[400];
        CMessages::InsertNumberInString(
            TheText.Get(t.GXTKey),
            t.NumberToInsert1,
            t.NumberToInsert2,
            -1,
            -1,
            -1,
            -1,
            text
        );
        CMessages::InsertPlayerControlKeysInString(text);
        // todo: Replace DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT
        // The first letter doesn't look good in window mode, but it looks fine in full-screen mode
        CFont::PrintString(
            SCREEN_SCALE_FROM_RIGHT(DEFAULT_SCREEN_WIDTH - t.Pos.x),
            SCREEN_SCALE_FROM_BOTTOM(DEFAULT_SCREEN_HEIGHT - t.Pos.y),
            text
        );
        CFont::SetEdge(0);
    }
}

// 0x58C250
// int-hud: body from batch_out/hud-1/0x58C250.cpp (replaces redirect stub).
// Fragment-local `#include "eHud.h"` dropped - already included at the top of this file.
void CHud::DrawSubtitles() {
    if (m_Message[0] == '\0') {
        return;
    }

    if (m_BigMessage[STYLE_WHITE_MIDDLE][0] != '\0' && !CGameLogic::IsCoopGameGoingOn()) { // 0xBAADC0 = m_BigMessage[2][0]
        return;
    }

    if (m_VehicleState != eNameState::NAME_DONT_SHOW) {
        m_VehicleState = eNameState::NAME_FADE_OUT; // 3
    }
    if (m_ZoneState != eNameState::NAME_DONT_SHOW) {
        m_ZoneState = eNameState::NAME_FADE_OUT; // 3
    }

    CFont::SetBackground(false, false);
    CFont::SetBackgroundColor(CRGBA(0, 0, 0, 128));
    CFont::SetOrientation(eFontAlignment::ALIGN_CENTER); // 0
    CFont::SetProportional(true);
    CFont::SetDropShadowPosition(0);
    CFont::SetFontStyle(eFontStyle::FONT_SUBTITLES); // 1
    CFont::SetColor(CRGBA(225, 225, 225, 255));
    CFont::SetDropShadowPosition(2);

    float x;
    float y;
    if (!TheCamera.m_bWideScreenOn) {
        if (bSubtitleShownInWidescreen) {
            m_Message[0] = '\0';
        }
        bSubtitleShownInWidescreen = false;

        CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(0.58f), SCREEN_SCALE_Y(1.22f));

        y = (SCREEN_HEIGHT - SCREEN_SCALE_Y(105.0f)) - SCREEN_SCALE_Y(2.0f); // OG: (H - 105s) - (s + s)
        if (!CTheScripts::bUseMessageFormatting) {
            if (!bDrawingVitalStats) {
                CFont::SetCentreSize(SCREEN_WIDTH - SCREEN_STRETCH_X(176.0f)); // OG: W - 20x - 8x - (140x + 8x)
                x = (SCREEN_WIDTH - SCREEN_STRETCH_X(176.0f)) * 0.5f + SCREEN_STRETCH_X(148.0f);
            } else {
                // While the stat panels (TAB) are shown subtitles are smaller and shifted left
                CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(0.58f) * 0.8f, SCREEN_SCALE_Y(1.22f));
                CFont::SetCentreSize((SCREEN_WIDTH - SCREEN_STRETCH_X(176.0f)) * 0.8f);
                x = SCREEN_STRETCH_X(40.0f) + (SCREEN_WIDTH - SCREEN_STRETCH_X(176.0f)) * 0.5f + SCREEN_STRETCH_X(148.0f);
            }
        } else {
            // Position given by script text formatting
            CFont::SetCentreSize(SCREEN_STRETCH_X(1.0f) * (float)CTheScripts::MessageWidth);
            x = SCREEN_STRETCH_X(1.0f) * (float)CTheScripts::MessageCentre;
        }
    } else {
        bSubtitleShownInWidescreen = true;
        if (!FrontEndMenuManager.m_bShowSubtitles && CCutsceneMgr::ms_running) { // 0xBA678C = m_bShowSubtitles (+0x44)
            CFont::SetDropShadowPosition(0);
            return;
        }

        CFont::SetCentreSize(SCREEN_WIDTH - SCREEN_STRETCH_X(60.0f));
        CFont::SetScale(SCREEN_STRETCH_X(0.58f), SCREEN_SCALE_Y(1.2f));
        y = SCREEN_HEIGHT - SCREEN_SCALE_Y(80.0f);
        x = (float)(RsGlobal.maximumWidth / 2); // OG: integer division
    }

    CFont::PrintString(x, y, m_Message);
    CFont::SetDropShadowPosition(0);
}

// 0x58C6A0
// int-hud: body from batch_out/hud-1/0x58C6A0.cpp (replaces redirect stub).
// NOTE: SuccessFailedMessagePosY (0xBAB218) is probably shared with the original DrawBustedWastedMessage
// (0x58CA50) - in the repo that one is replaced with a literal, see "// OG: posY static allocated var".
void CHud::DrawSuccessFailedMessage() {
    if (!bSuccessFailedMessagePosSet) {
        bSuccessFailedMessagePosSet = true;
        SuccessFailedMessagePosY = (float)(RsGlobal.maximumHeight / 2) - SCREEN_SCALE_Y(10.0f);
    }

    auto& message      = m_BigMessage[STYLE_MIDDLE];
    auto& messageX     = BigMessageX[STYLE_MIDDLE];
    auto& messageAlpha = BigMessageAlpha[STYLE_MIDDLE];
    auto& messageInUse = BigMessageInUse[STYLE_MIDDLE];

    if (message[0] == '\0') {
        messageX = 0.0f;
        return;
    }

    if (messageX != 0.0f) {
        CFont::SetBackground(false, false);
        CFont::SetScale(SCREEN_STRETCH_X(1.3f), SCREEN_SCALE_Y(1.8f));
        CFont::SetProportional(true);
        CFont::SetJustify(0);
        CFont::SetOrientation(eFontAlignment::ALIGN_CENTER); // 0
        CFont::SetCentreSize(SCREEN_STRETCH_X(590.0f));
        CFont::SetFontStyle(eFontStyle::FONT_PRICEDOWN); // 3
        CFont::SetEdge(2);
        CFont::SetDropColor(CRGBA(0, 0, 0, (uint8)(int)messageAlpha));
        CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_GOLD, (uint8)(int)messageAlpha)); // idx 6

        if ((float)(RsGlobal.maximumWidth - 20) <= messageInUse) {
            // Move up + fade out
            messageX += CTimer::ms_fTimeStep;
            if (messageX >= 120.0f) {
                messageX = 120.0f;
                messageAlpha -= (int)CTimer::GetTimeStepInMS() * 0.3f;
            }
            if (messageAlpha <= 0.0f) {
                messageAlpha = 0.0f;
                message[0] = '\0';
            }
        } else {
            // Fade in
            messageInUse += (int)CTimer::GetTimeStepInMS() * 0.3f;
            messageAlpha += (int)CTimer::GetTimeStepInMS() * 0.3f;
            if (messageAlpha > 255.0f) {
                messageAlpha = 255.0f;
            }
        }

        CFont::PrintString((float)(RsGlobal.maximumWidth / 2), SuccessFailedMessagePosY, message);
        return;
    }

    // First frame of display: reset animation and pick Y position
    messageInUse = -60.0f;
    messageX     = 1.0f;
    messageAlpha = 0.0f;

    if (m_BigMessage[STYLE_MIDDLE_SMALLER_HIGHER][0] == '\0' && m_BigMessage[STYLE_WHITE_MIDDLE_SMALLER][0] == '\0') {
        if (m_BigMessage[STYLE_WHITE_MIDDLE][0] == '\0') {
            const float baseY = (float)(RsGlobal.maximumHeight / 2) - SCREEN_SCALE_Y(10.0f);
            if (CFont::GetNumberLines((float)(RsGlobal.maximumWidth / 2), baseY, message) > 1) {
                SuccessFailedMessagePosY = baseY - SCREEN_SCALE_Y(15.0f);
                return;
            }
        }
        SuccessFailedMessagePosY = (float)(RsGlobal.maximumHeight / 2) - SCREEN_SCALE_Y(10.0f);
        return;
    }

    // Other central messages present - shift lower
    SuccessFailedMessagePosY = SCREEN_SCALE_Y(25.0f) + ((float)(RsGlobal.maximumHeight / 2) - SCREEN_SCALE_Y(10.0f));
}

// 0x58AEA0
void CHud::DrawVehicleName() {
    if (!m_pVehicleName) {
        m_VehicleState = NAME_DONT_SHOW;
        m_VehicleNameTimer = 0;
        m_VehicleFadeTimer = 0;
        m_pLastVehicleName = nullptr;
        return;
    }

    if (m_pVehicleName != m_pLastVehicleName) {
        switch (m_VehicleState) {
        case NAME_DONT_SHOW:
            m_VehicleState = NAME_FADE_IN;
            m_VehicleNameTimer = 0;
            m_VehicleFadeTimer = 0;
            m_pVehicleNameToPrint = m_pVehicleName;
            if (m_ZoneState == NAME_SHOW || m_ZoneState == NAME_FADE_IN) {
                m_ZoneState = NAME_FADE_OUT;
            }
            break;
        case NAME_SHOW:
        case NAME_FADE_IN:
        case NAME_FADE_OUT:
        case NAME_SWITCH:
            m_VehicleState = NAME_SWITCH;
            m_VehicleNameTimer = 0;
            break;
        default:
            break;
        }
        m_pLastVehicleName = m_pVehicleName;
    }

    if (!m_VehicleState)
        return;

    float alpha = 0.0f;
    switch (m_VehicleState) {
    case NAME_SHOW:
        if (m_VehicleNameTimer > 3000) {
            m_VehicleState = NAME_FADE_OUT;
            m_VehicleFadeTimer = 1000;
        }
        alpha = 255.0f;
        break;
    case NAME_FADE_IN:
        m_VehicleFadeTimer += (int32)CTimer::GetTimeStepInMS();
        if (m_VehicleFadeTimer > 1000) {
            m_VehicleFadeTimer = 1000;
            m_VehicleState = NAME_SHOW;
        }
        alpha = float(m_VehicleFadeTimer) / 1000.0f * 255.0f;
        break;
    case NAME_FADE_OUT:
        m_VehicleFadeTimer -= (int32)CTimer::GetTimeStepInMS();
        if (m_VehicleFadeTimer < 0) {
            m_VehicleState = NAME_DONT_SHOW;
            m_VehicleFadeTimer = 0;
        }
        alpha = float(m_VehicleFadeTimer) / 1000.0f * 255.0f;
        break;
    case NAME_SWITCH:
        m_VehicleFadeTimer -= (int32)CTimer::GetTimeStepInMS();
        if (m_VehicleFadeTimer < 0) {
            m_VehicleNameTimer = 0;
            m_VehicleState = NAME_FADE_IN;
            m_VehicleFadeTimer = 0;
            m_pVehicleNameToPrint = m_pLastVehicleName;
        }
        alpha = float(m_VehicleFadeTimer) / 1000.0f * 255.0f;
        break;
    default:
        break;
    }

    if (!m_Message[0]) {
        m_VehicleNameTimer += (int32)CTimer::GetTimeStepInMS();
        CFont::SetProportional(true);
        CFont::SetBackground(false, false);
        CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(1.0f), SCREEN_SCALE_Y(1.5f));
        CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
        CFont::SetRightJustifyWrap(0.0f);
        CFont::SetFontStyle(eFontStyle::FONT_MENU);
        CFont::SetEdge(2);
        CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_GREEN, (uint8)alpha));
        CFont::SetDropColor({ 0, 0, 0, (uint8)alpha });
        if (CTheScripts::bDisplayHud) {
            CFont::PrintString(
                SCREEN_STRETCH_FROM_RIGHT(32.0f),
                SCREEN_STRETCH_FROM_BOTTOM(104.0f),
                m_pVehicleNameToPrint
            );
        }
        CFont::SetSlant(0.0f);
    }
}

// 0x589650
// int-hud: body from batch_out/hud-1/0x589650.cpp (replaces redirect stub).
// Uses file-scope SkillStatDispTable1/2 (see top of this file).
// ABI note (from fragment): the skill percent formula floorf((v2*0.1+v)/(v1*v2))*(v1*v2)*0.001 relies on the
// runtime tables 0xB78EB4/0xB78F40 which overlap CStats::PedsKilledOfThisType / CStats::StatReactionValue
// in the repo - CStats layout likely shifted; semantics of the tables not confirmed.
void CHud::DrawVitalStats() {
    if (CReplay::Mode == MODE_PLAYBACK || TheCamera.m_bWideScreenOn) {
        return;
    }

    const auto player = FindPlayerPed();
    eWeaponType weaponType = player->m_aWeapons[player->m_nActiveWeaponSlot].m_Type;
    if (weaponType == WEAPON_TEC9) { // 0x20 -> 0x1C: same skill stat, normalization for display
        weaponType = WEAPON_MICRO_UZI;
    }

    CFont::SetBackground(false, false);
    CFont::SetColor(CRGBA(225, 225, 225, 255));
    CFont::SetWrapx(SCREEN_WIDTH);
    CFont::SetRightJustifyWrap(0.0f);
    CFont::SetProportional(true);

    const auto labelX     = (int)(40.0f + SCREEN_STRETCH_X(10.0f));
    const auto barX       = (int)(40.0f + SCREEN_STRETCH_X(90.0f));
    const auto barYOffset = (int)SCREEN_SCALE_Y(5.0f);
    const auto barWidth   = (int)SCREEN_STRETCH_X(70.0f);
    const auto barHeight  = (int)SCREEN_SCALE_Y(10.0f);

    const bool bNoSkillRow = !FindPlayerPed()->GetIntelligence()->GetTaskSwim()
        && (weaponType < WEAPON_PISTOL || weaponType > WEAPON_TEC9); // 0x16..0x20

    // Stats window: bottom/right shared, top depends on presence of the extra row (CURWSKL/STAT225)
    const CRect rect{
        40.0f,
        SCREEN_HEIGHT - SCREEN_SCALE_Y(13.0f),
        40.0f + SCREEN_STRETCH_X(170.0f),
        bNoSkillRow ? SCREEN_HEIGHT - SCREEN_SCALE_Y(125.0f) : SCREEN_HEIGHT - SCREEN_SCALE_Y(140.0f)
    };
    FrontEndMenuManager.DrawWindow(rect, "FEH_STA", 0, CRGBA(0, 0, 0, 190), false, true);

    const CRGBA barFrontColor(200, 200, 200, 255);
    const CRGBA barBackColor(0, 0, 0, 0);

    CFont::SetFontStyle(eFontStyle::FONT_SUBTITLES); // 1
    CFont::SetOrientation(eFontAlignment::ALIGN_LEFT); // 1
    CFont::SetScale(SCREEN_STRETCH_X(0.35f), SCREEN_SCALE_Y(0.9f));
    CFont::SetColor(CRGBA(225, 225, 225, 255));
    CFont::SetEdge(0);

    int rowY = (int)((bNoSkillRow
        ? (SCREEN_HEIGHT - SCREEN_SCALE_Y(140.0f)) + SCREEN_SCALE_Y(30.0f)
        : (SCREEN_HEIGHT - SCREEN_SCALE_Y(140.0f)) + SCREEN_SCALE_Y(15.0f)));

    // Row 1: RESPECT (GXT label "STAT068", bar over total-respect 64)
    CFont::PrintString((float)labelX, (float)rowY, TheText.Get("STAT068"));
    CSprite2d::DrawBarChart(
        (float)barX,
        (float)(rowY + barYOffset),
        (uint16)barWidth,
        (uint8)barHeight,
        CStats::GetStatValue(STAT_TOTAL_RESPECT) * 0.001f * 100.0f,
        0, 0, 1,
        barFrontColor,
        barBackColor
    );
    rowY = (int)(SCREEN_SCALE_Y(15.0f) + (float)rowY);

    // Row 2: swimming -> LUNG CAPACITY, otherwise current weapon's skill (if any);
    // if the row is absent rowY already points at STAT022 (goto LAB_00589cbf in the decompile)
    if (FindPlayerPed()->GetIntelligence()->GetTaskSwim()) {
        CFont::PrintString((float)labelX, (float)rowY, TheText.Get("STAT225"));
        CSprite2d::DrawBarChart(
            (float)barX,
            (float)(rowY + barYOffset),
            (uint16)barWidth,
            (uint8)barHeight,
            CStats::GetStatValue(STAT_LUNG_CAPACITY) * 0.001f * 100.0f,
            0, 0, 1,
            barFrontColor,
            barBackColor
        );
        rowY = (int)(SCREEN_SCALE_Y(15.0f) + (float)rowY);
    } else if (weaponType >= WEAPON_PISTOL && weaponType <= WEAPON_TEC9) {
        CFont::PrintString((float)labelX, (float)rowY, TheText.Get("CURWSKL"));
        const eStats skillStat = CWeaponInfo::GetSkillStatIndex(weaponType);
        const float v1 = SkillStatDispTable1[(size_t)skillStat];
        const float v2 = SkillStatDispTable2[(size_t)skillStat - 69];
        const float skillValue = CStats::GetStatValue(skillStat);
        const float percent = skillValue <= 999.0f
            ? std::floorf((v2 * 0.1f + skillValue) / (v1 * v2)) * (v1 * v2) * 0.001f
            : 1.0f;
        CSprite2d::DrawBarChart(
            (float)barX,
            (float)(rowY + barYOffset),
            (uint16)barWidth,
            (uint8)barHeight,
            percent * 100.0f,
            0, 0, 1,
            barFrontColor,
            barBackColor
        );
        rowY = (int)(SCREEN_SCALE_Y(15.0f) + (float)rowY);
    }

    // Attribute rows (GXT labels = "STAT0" + stat id)
    CFont::PrintString((float)labelX, (float)rowY, TheText.Get("STAT022"));
    CSprite2d::DrawBarChart((float)barX, (float)(rowY + barYOffset), (uint16)barWidth, (uint8)barHeight,
        CStats::GetStatValue(STAT_STAMINA) * 0.001f * 100.0f, 0, 0, 1, barFrontColor, barBackColor);
    rowY = (int)(SCREEN_SCALE_Y(15.0f) + (float)rowY);

    CFont::PrintString((float)labelX, (float)rowY, TheText.Get("STAT023"));
    CSprite2d::DrawBarChart((float)barX, (float)(rowY + barYOffset), (uint16)barWidth, (uint8)barHeight,
        CStats::GetStatValue(STAT_MUSCLE) * 0.001f * 100.0f, 0, 0, 1, barFrontColor, barBackColor);
    rowY = (int)(SCREEN_SCALE_Y(15.0f) + (float)rowY);

    CFont::PrintString((float)labelX, (float)rowY, TheText.Get("STAT021"));
    CSprite2d::DrawBarChart((float)barX, (float)(rowY + barYOffset), (uint16)barWidth, (uint8)barHeight,
        CStats::GetStatValue(STAT_FAT) * 0.001f * 100.0f, 0, 0, 1, barFrontColor, barBackColor);
    rowY = (int)(SCREEN_SCALE_Y(15.0f) + (float)rowY);

    CFont::PrintString((float)labelX, (float)rowY, TheText.Get("STAT025"));
    CSprite2d::DrawBarChart((float)barX, (float)(rowY + barYOffset), (uint16)barWidth, (uint8)barHeight,
        CStats::GetStatValue(STAT_SEX_APPEAL) * 0.001f * 100.0f, 0, 0, 1, barFrontColor, barBackColor);
    // no +15 step after the last row - straight to the big step towards the day row (15 + 3)

    // Day-of-week row
    rowY = (int)(SCREEN_SCALE_Y(18.0f) + (float)rowY);

    CFont::SetFontStyle(eFontStyle::FONT_PRICEDOWN); // 3
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT); // 2
    CFont::SetScale(SCREEN_STRETCH_X(0.7f), SCREEN_SCALE_Y(0.7f));
    CFont::SetEdge(1);
    CFont::SetColor(barFrontColor);
    CFont::SetDropColor(CRGBA(0, 0, 0, 255));

    sprintf_s(gString, "DAY_%d", CClock::CurrentDay);
    CFont::PrintString(40.0f + SCREEN_STRETCH_X(160.0f), (float)rowY, TheText.Get(gString));
}

// 0x588A50
void CHud::GetRidOfAllHudMessages(bool arg0) {
    std::ranges::fill(m_pHelpMessageToPrint, '\0');
    std::ranges::fill(m_pLastHelpMessage, '\0');
    std::ranges::fill(m_pHelpMessage, '\0');
    std::ranges::fill(m_Message, '\0');

    m_ZoneNameTimer               = 0;
    m_pZoneName                   = nullptr;
    m_ZoneState                   = NAME_DONT_SHOW;
    m_nHelpMessageTimer           = 0;
    m_nHelpMessageFadeTimer       = 0;
    m_nHelpMessageState           = 0;
    m_bHelpMessageQuick           = false;
    m_nHelpMessageMaxStatValue    = 1000;
    m_nHelpMessageStatId          = 0;
    m_fHelpMessageStatUpdateValue = 0.0f;
    m_bHelpMessagePermanent       = false;
    m_fHelpMessageTime            = 1.0f;
    m_pVehicleName                = nullptr;
    m_pVehicleNameToPrint         = nullptr;
    m_VehicleNameTimer            = 0;
    m_VehicleFadeTimer            = 0;
    m_VehicleState                = NAME_DONT_SHOW;

    for (auto i = 0; i < NUM_MESSAGE_STYLES; ++i) {
        if (BigMessageX[i] != 0.0f)
            continue;

        if (arg0) {
            if (BigMessageX[i] == BigMessageX[STYLE_BOTTOM_RIGHT] ||
                BigMessageX[i] == BigMessageX[STYLE_MIDDLE_SMALLER_HIGHER]
            ) {
                continue;
            }
        }
        std::ranges::fill(m_BigMessage[i], '\0');
    }
}

// 0x5893B0
void CHud::DrawAmmo(CPed* ped, int32 x, int32 y, float alpha) {
    const auto MAX_CLIP = 9999;

    const auto& weapon = ped->GetActiveWeapon();
    const auto& totalAmmo = weapon.m_TotalAmmo;
    const auto& ammoInClip = weapon.m_AmmoInClip;
    const auto& ammoClip = CWeaponInfo::GetWeaponInfo(weapon.m_Type, ped->GetWeaponSkill())->m_nAmmoClip;

    if (ammoClip <= 1 || ammoClip >= 1000) {
        sprintf_s(gString, "%d", totalAmmo);
    } else {
        uint32 total, current;

        if (weapon.m_Type == WEAPON_FLAMETHROWER ) {
            uint32 out = MAX_CLIP;
            if ((totalAmmo - ammoInClip) / 10 <= MAX_CLIP) {
                out = (totalAmmo - ammoInClip) / 10u;
            }
            total = out;

            current = ammoInClip / 10;
        } else {
            auto out = totalAmmo - ammoInClip;
            if (totalAmmo - ammoInClip > MAX_CLIP) {
                out = MAX_CLIP;
            }
            total = out;

            current = ammoInClip;
        }
        sprintf_s(gString, "%d-%d", total, current);
    }
    AsciiToGxtChar(gString, gGxtString);

    CFont::SetBackground(false, false);
    CFont::SetScale(SCREEN_STRETCH_X(0.3f), SCREEN_STRETCH_Y(0.7f));
    CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
    CFont::SetCentreSize(SCREEN_STRETCH_Y(640.0f));
    CFont::SetProportional(true);
    CFont::SetEdge(1);
    CFont::SetDropColor({ 0, 0, 0, 255 });
    CFont::SetFontStyle(eFontStyle::FONT_SUBTITLES);

    if (   totalAmmo - weapon.m_AmmoInClip >= MAX_CLIP
        || CDarkel::FrenzyOnGoing()
        || weapon.m_Type == WEAPON_UNARMED
        || weapon.m_Type == WEAPON_DETONATOR
        || weapon.m_Type == WEAPON_DILDO1
        || weapon.m_Type == WEAPON_DILDO2
        || weapon.m_Type == WEAPON_VIBE1
        || weapon.m_Type == WEAPON_VIBE2
        || weapon.m_Type == WEAPON_FLOWERS
        || weapon.m_Type == WEAPON_CANE
        || weapon.m_Type == WEAPON_PARACHUTE
        || CWeaponInfo::GetWeaponInfo(weapon.m_Type)->m_nWeaponFire == WEAPON_FIRE_USE
        || CWeaponInfo::GetWeaponInfo(weapon.m_Type)->m_nSlot <= 1
    ) {
        CFont::SetEdge(0);
        return;
    }

    CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_LIGHT_BLUE, (uint8)alpha));
    CFont::PrintString((float)x, (float)y, gGxtString);
    CFont::SetEdge(0);
}

// 0x58EAF0
// int-hud: body from batch_out/hud-1/0x58EAF0.cpp (replaces redirect stub).
// Calls the existing inline helpers DrawClock/DrawMoney/DrawWeapon and RenderHealthBar/RenderArmorBar/
// RenderBreathBar - all declared in Hud.h, so keeping this definition at its original position is fine.
// ABI notes (from fragment):
// - The m_EnergyLost*/m_DisplayScore*/m_Weapon* state machines are inlined (not DrawFadeState calls) and
//   have restart-on-change logic that DrawFadeState lacks - duplicated verbatim here.
// - [veh+0x40]&0x100 and [veh+0x42B]&0x40 are CVehicle flags not mapped in the repo model
//   (0x100 looks like physicalFlags.bSubmergedInWater); kept as offset reads - integrator to refine.
void CHud::DrawPlayerInfo() {
    const CPlayerInfo& playerInfo = CWorld::Players[CWorld::PlayerInFocus];

    if (bDrawClock) {
        DrawClock();
    }

    // Breath/health/armor indicators (ENERGY_LOST)
    {
        const int lastEnergyLost = (int)playerInfo.m_nLastTimeEnergyLost;
        if (m_LastTimeEnergyLost == lastEnergyLost) {
            if (m_EnergyLostState != 0 && m_EnergyLostState != 5) {
                if (m_EnergyLostState == 1) {
                    m_EnergyLostFadeTimer = 1000;
                    if (10000.0f < (float)m_EnergyLostTimer) {
                        m_EnergyLostState = 3;
                        m_EnergyLostFadeTimer = 3000;
                    }
                } else if (m_EnergyLostState == 2) {
                    m_EnergyLostFadeTimer += (int)CTimer::GetTimeStepInMS();
                    if (1000.0f < (float)m_EnergyLostFadeTimer) {
                        m_EnergyLostFadeTimer = 1000;
                        m_EnergyLostState = 1;
                    }
                } else if (m_EnergyLostState == 3) {
                    m_EnergyLostFadeTimer -= (int)CTimer::GetTimeStepInMS();
                    if ((float)m_EnergyLostFadeTimer < 0.0f) {
                        m_EnergyLostFadeTimer = 0;
                        m_EnergyLostState = 0;
                    }
                }
                m_EnergyLostTimer += (int)CTimer::GetTimeStepInMS();
            }
        } else {
            // Energy level changed - restart fade-in
            if (m_EnergyLostState == 0) {
                m_EnergyLostFadeTimer = 0;
            }
            if (m_EnergyLostState == 0 || m_EnergyLostState == 1 || m_EnergyLostState == 3) {
                m_EnergyLostTimer = 5;
                m_EnergyLostState = 2;
            }
            if (m_EnergyLostState == 2) {
                m_EnergyLostFadeTimer += (int)CTimer::GetTimeStepInMS();
                if (1000.0f < (float)m_EnergyLostFadeTimer) {
                    m_EnergyLostFadeTimer = 1000;
                    m_EnergyLostState = 1;
                }
            } else if (m_EnergyLostState == 3) {
                m_EnergyLostFadeTimer -= (int)CTimer::GetTimeStepInMS();
                if ((float)m_EnergyLostFadeTimer < 0.0f) {
                    m_EnergyLostFadeTimer = 0;
                    m_EnergyLostState = 0;
                }
            }
            m_EnergyLostTimer += (int)CTimer::GetTimeStepInMS();
        }
        m_LastTimeEnergyLost = (uint32)lastEnergyLost;

        if (m_EnergyLostState != 0) {
            const int healthX = (int)(SCREEN_WIDTH - SCREEN_STRETCH_X(141.0f));
            const int armorX  = (int)(SCREEN_WIDTH - SCREEN_STRETCH_X(94.0f));

            RenderHealthBar(CWorld::PlayerInFocus, healthX, (int)GetYPosBasedOnHealth(CWorld::PlayerInFocus, SCREEN_SCALE_Y(77.0f), 10));
            if (CWorld::Players[1].m_pPed) {
                RenderHealthBar(1, healthX, (int)GetYPosBasedOnHealth(1,
                    GetYPosBasedOnHealth(CWorld::PlayerInFocus, SCREEN_SCALE_Y(194.0f), 12), 12));
            }

            RenderArmorBar(CWorld::PlayerInFocus, armorX, (int)GetYPosBasedOnHealth(CWorld::PlayerInFocus, SCREEN_SCALE_Y(48.0f), 3));
            if (CWorld::Players[1].m_pPed) {
                RenderArmorBar(1, armorX, (int)GetYPosBasedOnHealth(1,
                    GetYPosBasedOnHealth(CWorld::PlayerInFocus, SCREEN_SCALE_Y(164.0f), 12), 12));
            }

            // Breath bar: swimming / in a sinking boat / breath below norm
            CPlayerPed* playerPed      = playerInfo.m_pPed;
            CPlayerPed* playerPed2     = CWorld::Players[1].m_pPed;
            bool        bShowBreath    = false;
            bool        bShowBreath2   = false;

            const auto IsBreathLost = [](const CPed* ped) {
                if (ped->GetIntelligence()->GetTaskSwim()) {
                    return true;
                }
                if (ped->bInVehicle && ped->m_pVehicle) {
                    const auto veh = reinterpret_cast<const uint8*>(ped->m_pVehicle);
                    if (*reinterpret_cast<const uint32*>(veh + 0x40) & 0x100
                        && *reinterpret_cast<const uint8*>(veh + 0x42B) & 0x40
                    ) {
                        return true;
                    }
                }
                return false;
            };

            if (IsBreathLost(playerPed)) {
                bShowBreath = true;
            } else if (playerPed->GetPlayerData()->m_fBreath < CStats::GetFatAndMuscleModifier(STAT_MOD_AIR_IN_LUNG)
                && (uint32)CTimer::m_snTimeInMilliseconds < (uint32)m_LastBreathTime + 500u
            ) {
                bShowBreath = true;
            }
            if (bShowBreath) {
                m_LastBreathTime = CTimer::m_snTimeInMilliseconds;
            }

            if (playerPed2) {
                bShowBreath2 = IsBreathLost(playerPed2)
                    || playerPed2->GetPlayerData()->m_fBreath < CStats::GetFatAndMuscleModifier(STAT_MOD_AIR_IN_LUNG);
            }

            if (bShowBreath) {
                RenderBreathBar(CWorld::PlayerInFocus, armorX, (int)GetYPosBasedOnHealth(CWorld::PlayerInFocus, SCREEN_SCALE_Y(62.0f), 6));
            }
            if (bShowBreath2 && playerPed2) {
                RenderBreathBar(1, armorX, (int)GetYPosBasedOnHealth(1,
                    GetYPosBasedOnHealth(CWorld::PlayerInFocus, SCREEN_SCALE_Y(179.0f), 12), 12));
            }
        }
    }

    // Money (DISPLAY_SCORE)
    {
        const int displayMoney = playerInfo.m_nDisplayMoney;
        if (m_LastDisplayScore == displayMoney) {
            if (m_DisplayScoreState != 0 && m_DisplayScoreState != 5) {
                if (m_DisplayScoreState == 1) {
                    m_DisplayScoreFadeTimer = 1000;
                    if (10000.0f < (float)m_DisplayScoreTimer) {
                        m_DisplayScoreState = 3;
                        m_DisplayScoreFadeTimer = 3000;
                    }
                } else if (m_DisplayScoreState == 2) {
                    m_DisplayScoreFadeTimer += (int)CTimer::GetTimeStepInMS();
                    if (1000.0f < (float)m_DisplayScoreFadeTimer) {
                        m_DisplayScoreFadeTimer = 1000;
                        m_DisplayScoreState = 1;
                    }
                } else if (m_DisplayScoreState == 3) {
                    m_DisplayScoreFadeTimer -= (int)CTimer::GetTimeStepInMS();
                    if ((float)m_DisplayScoreFadeTimer < 0.0f) {
                        m_DisplayScoreFadeTimer = 0;
                        m_DisplayScoreState = 0;
                    }
                }
                m_DisplayScoreTimer += (int)CTimer::GetTimeStepInMS();
            }
        } else {
            if (m_DisplayScoreState == 0) {
                m_DisplayScoreFadeTimer = 0;
            }
            if (m_DisplayScoreState == 0 || m_DisplayScoreState == 1 || m_DisplayScoreState == 3) {
                m_DisplayScoreTimer = 5;
                m_DisplayScoreState = 2;
            }
            if (m_DisplayScoreState == 2) {
                m_DisplayScoreFadeTimer += (int)CTimer::GetTimeStepInMS();
                if (1000.0f < (float)m_DisplayScoreFadeTimer) {
                    m_DisplayScoreFadeTimer = 1000;
                    m_DisplayScoreState = 1;
                }
            } else if (m_DisplayScoreState == 3) {
                m_DisplayScoreFadeTimer -= (int)CTimer::GetTimeStepInMS();
                if ((float)m_DisplayScoreFadeTimer < 0.0f) {
                    m_DisplayScoreFadeTimer = 0;
                    m_DisplayScoreState = 0;
                }
            }
            m_DisplayScoreTimer += (int)CTimer::GetTimeStepInMS();
        }
        m_LastDisplayScore = (uint32)displayMoney;

        if (m_DisplayScoreState != 0) {
            const float moneyAlpha = std::clamp((float)m_DisplayScoreFadeTimer * 0.001f * 255.0f, 0.0f, 255.0f);
            DrawMoney(playerInfo, (uint8)(int)moneyAlpha);
        }
    }

    // Weapon (WEAPON)
    {
        const int currentWeaponType = (int)playerInfo.m_pPed->GetActiveWeapon().m_Type;
        if (m_LastWeapon == currentWeaponType) {
            if (m_WeaponState != 0 && m_WeaponState != 5) {
                if (m_WeaponState == 1) {
                    m_WeaponFadeTimer = 1000;
                    if (10000.0f < (float)m_WeaponTimer) {
                        m_WeaponState = 3;
                        m_WeaponFadeTimer = 3000;
                    }
                } else if (m_WeaponState == 2) {
                    m_WeaponFadeTimer += (int)CTimer::GetTimeStepInMS();
                    if (1000.0f < (float)m_WeaponFadeTimer) {
                        m_WeaponFadeTimer = 1000;
                        m_WeaponState = 1;
                    }
                } else if (m_WeaponState == 3) {
                    m_WeaponFadeTimer -= (int)CTimer::GetTimeStepInMS();
                    if ((float)m_WeaponFadeTimer < 0.0f) {
                        m_WeaponFadeTimer = 0;
                        m_WeaponState = 0;
                    }
                }
                m_WeaponTimer += (int)CTimer::GetTimeStepInMS();
            }
        } else {
            if (m_WeaponState == 0) {
                m_WeaponFadeTimer = 0;
            }
            if (m_WeaponState == 0 || m_WeaponState == 1 || m_WeaponState == 3) {
                m_WeaponTimer = 5;
                m_WeaponState = 2;
            }
            if (m_WeaponState == 2) {
                m_WeaponFadeTimer += (int)CTimer::GetTimeStepInMS();
                if (1000.0f < (float)m_WeaponFadeTimer) {
                    m_WeaponFadeTimer = 1000;
                    m_WeaponState = 1;
                }
            } else if (m_WeaponState == 3) {
                m_WeaponFadeTimer -= (int)CTimer::GetTimeStepInMS();
                if ((float)m_WeaponFadeTimer < 0.0f) {
                    m_WeaponFadeTimer = 0;
                    m_WeaponState = 0;
                }
            }
            m_WeaponTimer += (int)CTimer::GetTimeStepInMS();
            m_LastWeapon = (uint32)currentWeaponType;
        }

        if (m_WeaponState != 0) {
            DrawWeapon(playerInfo.m_pPed, CWorld::Players[1].m_pPed);
        }
    }
}

inline void CHud::DrawClock() {
    char ascii[16];
    GxtChar gxtText[16];
    CFont::SetBackground(false, false);
    CFont::SetScale(SCREEN_STRETCH_X(0.55f), SCREEN_STRETCH_Y(1.1f));
    CFont::SetProportional(false);
    CFont::SetFontStyle(FONT_PRICEDOWN);
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
    CFont::SetRightJustifyWrap(0.0f);
    CFont::SetEdge(2);
    CFont::SetDropColor({0, 0, 0, 255});
    sprintf_s(ascii, "%02d:%02d", CClock::ms_nGameClockHours, CClock::ms_nGameClockMinutes);
    AsciiToGxtChar(ascii, gxtText);
    CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY));
    CFont::PrintString(SCREEN_STRETCH_FROM_RIGHT(32.0f), SCREEN_STRETCH_Y(22.0f), gxtText);
    CFont::SetEdge(0);
}

inline void CHud::DrawMoney(const CPlayerInfo& playerInfo, uint8 alpha) {
    char ascii[16];
    GxtChar gxtText[16];

    if (playerInfo.m_nDisplayMoney < 0) {
        CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_RED, alpha));
        auto m_nDisplayMoney = playerInfo.m_nDisplayMoney;
        if (m_nDisplayMoney < 0) {
            m_nDisplayMoney = -m_nDisplayMoney;
        }
        sprintf_s(ascii, "-$%07d", m_nDisplayMoney);
    } else {
        CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_GREEN, alpha));
        sprintf_s(ascii, "$%08d", std::abs(playerInfo.m_nDisplayMoney));
    }
    AsciiToGxtChar(ascii, gxtText);
    CFont::SetProportional(false);
    CFont::SetBackground(false, false);
    CFont::SetScale(SCREEN_STRETCH_X(0.55f), SCREEN_STRETCH_Y(1.1f));
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
    CFont::SetRightJustifyWrap(0.0f);
    CFont::SetFontStyle(FONT_PRICEDOWN);
    CFont::SetDropShadowPosition(0);
    CFont::SetEdge(2);
    CFont::SetDropColor({ 0, 0, 0, uint8(alpha) });
    CFont::PrintString(SCREEN_STRETCH_FROM_RIGHT(32.0f), GetYPosBasedOnHealth(CWorld::PlayerInFocus, SCREEN_STRETCH_Y(89.0f), 12), gxtText);
    CFont::SetEdge(0);
}

inline void CHud::DrawWeapon(CPlayerPed* ped0, CPlayerPed* ped1) {
    const auto magic = SCREEN_WIDTH * 0.17343046f; // todo: magic
    if (m_WeaponState) {
        DrawWeaponIcon(ped0, (int32)(SCREEN_WIDTH - (SCREEN_STRETCH_X(32.0f) + magic)), (int32)SCREEN_STRETCH_Y(20.0f), (float)m_WeaponFadeTimer);
        if (ped1) {
            const auto posX = (int32)(SCREEN_WIDTH - (SCREEN_STRETCH_X(32.0f) + 111.0f));
            const auto posY = (int32)GetYPosBasedOnHealth(CWorld::PlayerInFocus, SCREEN_STRETCH_Y(138.0f), 12);
            DrawWeaponIcon(ped1, posX, posY, (float)m_WeaponFadeTimer);
        }

        const auto ammoPosX = (int32)(SCREEN_WIDTH - (magic + SCREEN_STRETCH_X(32.0f)) + SCREEN_STRETCH_X(47.0f / 2.0f));
        const auto ammoPosY = SCREEN_STRETCH_Y(43.0f);
        DrawAmmo(ped0, ammoPosX, (int32)ammoPosY + (int32)SCREEN_STRETCH_Y(20.0f), (float)m_WeaponFadeTimer);
        if (ped1) {
            const auto posY = (int32)GetYPosBasedOnHealth(CWorld::PlayerInFocus, ammoPosY + SCREEN_STRETCH_Y(138.0f), 12);
            DrawAmmo(ped1, ammoPosX, posY, (float)m_WeaponFadeTimer);
        }
    }
}

// 0x58A160
void CHud::DrawTripSkip() {
    CRect rect{
        SCREEN_STRETCH_X(54.0f),
        SCREEN_STRETCH_FROM_BOTTOM(189.0f),
        SCREEN_STRETCH_X(118.0f),
        SCREEN_STRETCH_FROM_BOTTOM(125.0f)
    };
    Sprites[SPRITE_SKIP_ICON].Draw(rect, CRGBA(255, 255, 255, 255));

    CFont::SetBackground(false, false);
    CFont::SetScale(SCREEN_STRETCH_X(0.3f), SCREEN_SCALE_Y(0.7f));
    CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
    CFont::SetCentreSize(SCREEN_WIDTH);
    CFont::SetProportional(true);
    CFont::SetEdge(1);
    CFont::SetDropColor({ 0, 0, 0, 255 });
    CFont::SetFontStyle(eFontStyle::FONT_MENU);
    CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY));
    CFont::PrintString(
        SCREEN_STRETCH_X(64.0f) / 2.0f + SCREEN_STRETCH_X(54.0f),
        SCREEN_STRETCH_FROM_BOTTOM(127.0f),
        TheText.Get("FEC_TSK") // TRIP SKIP
    );
}

// 0x58D9A0
// int-hud: body from batch_out/hud-1/0x58D9A0.cpp (replaces redirect stub).
// ABI note (from fragment): the original reads [CWanted+0x2C] (m_WantedLevel) and [CWanted+0x30]
// (m_WantedLevelBeforeParole). In the repo's Wanted.h m_TimeCounting sits after m_CurrentChaseTimeCounter;
// if the packing differs from the original the offsets shift - verify CWanted packing.
// The wanted state machine is inlined in the original (not a DrawFadeState call) - reproduced verbatim.
void CHud::DrawWanted() {
    const CWanted* wanted      = FindPlayerWanted();
    const int      wantedLevel = (int)wanted->m_WantedLevel;             // [CWanted+0x2C]
    const int      flashLevel  = (int)wanted->m_WantedLevelBeforeParole; // [CWanted+0x30]

    float alpha = 255.0f;
    if (m_LastWanted == wantedLevel) {
        // Level unchanged - regular state machine (like DrawFadeState(WANTED_STATE))
        if (m_WantedState != 0 && m_WantedState != 5) {
            if (m_WantedState == 1) {
                m_WantedFadeTimer = 1000;
                if (10000.0f < (float)m_WantedTimer) {
                    m_WantedState = 3;
                    m_WantedFadeTimer = 3000;
                }
            } else if (m_WantedState == 2) {
                m_WantedFadeTimer += (int)CTimer::GetTimeStepInMS();
                if (1000.0f < (float)m_WantedFadeTimer) {
                    m_WantedFadeTimer = 1000;
                    m_WantedState = 1;
                }
            } else if (m_WantedState == 3) {
                m_WantedFadeTimer -= (int)CTimer::GetTimeStepInMS();
                if ((float)m_WantedFadeTimer < 0.0f) {
                    m_WantedFadeTimer = 0;
                    m_WantedState = 0;
                }
            }
            m_WantedTimer += (int)CTimer::GetTimeStepInMS();
            alpha = std::clamp((float)m_WantedFadeTimer * 0.001f * 255.0f, 0.0f, 255.0f);
        }
        bWantedDisplayed = true;
    } else {
        // Level changed - restart fade-in (0 -> and 1/3 -> state 2, timer=5)
        if (m_WantedState == 0) {
            m_WantedFadeTimer = 0;
        }
        if (m_WantedState == 0 || m_WantedState == 1 || m_WantedState == 3) {
            m_WantedTimer = 5;
            m_WantedState = 2;
        }
        if (m_WantedState != 0 && m_WantedState != 5) {
            if (m_WantedState == 2) {
                m_WantedFadeTimer += (int)CTimer::GetTimeStepInMS();
                if (1000.0f < (float)m_WantedFadeTimer) {
                    m_WantedFadeTimer = 1000;
                    m_WantedState = 1;
                }
            } else if (m_WantedState == 3) {
                m_WantedFadeTimer -= (int)CTimer::GetTimeStepInMS();
                if ((float)m_WantedFadeTimer < 0.0f) {
                    m_WantedFadeTimer = 0;
                    m_WantedState = 0;
                }
            }
            m_WantedTimer += (int)CTimer::GetTimeStepInMS();
            alpha = std::clamp((float)m_WantedFadeTimer * 0.001f * 255.0f, 0.0f, 255.0f);
        }
        m_LastWanted    = (uint32)wantedLevel;
        bWantedDisplayed = alpha > 0.0f;
    }

    if (m_WantedState == 0) {
        return;
    }

    CFont::SetBackground(false, false);
    CFont::SetScale(SCREEN_STRETCH_X(0.605f), SCREEN_SCALE_Y(1.21f));
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT); // 2
    CFont::SetProportional(true);
    CFont::SetFontStyle(eFontStyle::FONT_PRICEDOWN); // 0

    GxtChar starText[20];
    AsciiToGxtChar("]", starText); // star - the ']' glyph in FONT_PRICEDOWN

    float starX = SCREEN_WIDTH - SCREEN_STRETCH_X(29.0f);
    if ((wantedLevel > 0 && bWantedDisplayed) || flashLevel > 0) {
        const auto starAlpha = (uint8)(int)alpha;
        for (auto i = 0; i < 6; i++) {
            CFont::SetEdge(1);
            CFont::SetDropColor(CRGBA(0, 0, 0, starAlpha));
            CFont::SetScale(SCREEN_STRETCH_X(0.605f), SCREEN_SCALE_Y(1.21f));

            if (i < wantedLevel) {
                // Lit star; blinks for 2s after the wanted level changed
                if (CTimer::m_snTimeInMilliseconds > wanted->m_LastTimeWantedDecreased + 2000 || (CTimer::m_FrameCounter & 4)) {
                    CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_GOLD, starAlpha)); // idx 6
                    float y = SCREEN_SCALE_Y(114.0f);
                    if ((float)CWorld::Players[CWorld::PlayerInFocus].m_nMaxHealth < 101.0f) {
                        y -= SCREEN_SCALE_Y(12.0f);
                    }
                    CFont::PrintString(starX, y, starText);
                }
            } else if (i < flashLevel && (CTimer::m_FrameCounter & 4)) {
                // Flashing "extra" stars (level before leaving the chase)
                CFont::SetColor(CRGBA(
                    (uint8)(int)(FlashStarColourR * 0.8f),
                    (uint8)(int)(FlashStarColourG * 0.8f),
                    (uint8)(int)(FlashStarColourB * 0.8f),
                    starAlpha
                ));
                CFont::PrintString(starX, GetYPosBasedOnHealth(CWorld::PlayerInFocus, SCREEN_SCALE_Y(114.0f), 12), starText);
            } else if (i >= wantedLevel) {
                // Empty star: darker and bigger, below the row
                CFont::SetEdge(0);
                CFont::SetColor(CRGBA(0, 0, 0, (uint8)(int)(alpha * 120.0f)));
                CFont::SetScale(SCREEN_STRETCH_X(0.605f) * 1.2f, SCREEN_SCALE_Y(1.21f) * 1.2f);
                CFont::PrintString(
                    starX,
                    GetYPosBasedOnHealth(CWorld::PlayerInFocus, SCREEN_SCALE_Y(114.0f), 12) - 2.0f * SCREEN_SCALE_Y(1.0f),
                    starText
                );
            }
            starX -= SCREEN_STRETCH_X(18.0f);
        }
        CFont::SetEdge(0);
    }
}

// 0x58D7D0
void CHud::DrawWeaponIcon(CPed* ped, int32 x, int32 y, float alpha) {
    const auto x0 = (float)x;
    const auto y0 = (float)y;
    const float width  = SCREEN_STRETCH_X(47.0f);
    const float height = SCREEN_STRETCH_Y(58.0f);
    const float halfWidth  = width / 2.0f;
    const float halfHeight = height / 2.0f;

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERLINEAR));

    auto modelId = ped->GetActiveWeapon().GetWeaponInfo().m_nModelId1;
    if (modelId <= 0) {
        Sprites[SPRITE_FIST].Draw({ x0, y0, width + x0, height + y0 }, CRGBA(255, 255, 255, (uint8)alpha));
        return;
    }

    auto mi = CModelInfo::GetModelInfo(modelId);
    auto txd = CTxdStore::ms_pTxdPool->GetAt(mi->m_nTxdIndex);
    if (!txd)
        return;

    auto texture = RwTexDictionaryFindHashNamedTexture(txd->m_pRwDictionary, CKeyGen::AppendStringToKey(mi->m_nKey, "ICON"));
    if (!texture)
        return;

    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,   RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(RwTextureGetRaster(texture)));
    CSprite::RenderOneXLUSprite(
        { x0 + halfWidth, y0 + halfHeight, 1.0f },
        { halfWidth, halfHeight },
        255u, 255u, 255u, 255,
        1.0f,
        255,
        0, 0
    );
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,  RWRSTATE(FALSE));
}

// 0x5890A0
void CHud::RenderArmorBar(int32 playerId, int32 x, int32 y) {
    auto* player = FindPlayerPed(playerId);
    if ((m_ItemToFlash == ITEM_ARMOUR && EachFrames(8)) || player->m_fArmour <= 1.0f)
        return;

    const auto info = player->GetPlayerInfoForThisPlayerPed();
    CSprite2d::DrawBarChart(
        (float)x,
        (float)y,
        (uint16)SCREEN_STRETCH_X(62.0f),
        (uint8)SCREEN_STRETCH_Y(9.0f),
        player->m_fArmour / (float)info->m_nMaxArmour * 100.0f,
        false,
        false,
        true,
        HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY),
        CRGBA(0, 0, 0, 0)
    );
}

// 0x589190
void CHud::RenderBreathBar(int32 playerId, int32 x, int32 y) {
    if (m_ItemToFlash == ITEM_BREATH && EachFrames(8))
        return;

    auto* player = FindPlayerPed(playerId);
    CSprite2d::DrawBarChart(
        (float)x,
        (float)y,
        (uint16)SCREEN_STRETCH_X(62.0f),
        (uint8)SCREEN_STRETCH_Y(9.0f),
        player->GetPlayerData()->m_fBreath / CStats::GetFatAndMuscleModifier(STAT_MOD_AIR_IN_LUNG) * 100.0f,
        false,
        false,
        true,
        HudColour.GetRGB(HUD_COLOUR_LIGHT_BLUE),
        CRGBA(0, 0, 0, 0)
    );
}

// 0x589270
void CHud::RenderHealthBar(int32 playerId, int32 x, int32 y) {
    if (m_ItemToFlash == ITEM_HEALTH && EachFrames(8))
        return;

    auto* player = FindPlayerPed(playerId);
    if ((int)player->m_fHealth < 10 && EachFrames(8))
        return;

    const float x109 = SCREEN_STRETCH_X(109.0f);
    const auto info = player->GetPlayerInfoForThisPlayerPed();
    const auto totalWidth = uint16(x109 * (float)info->m_nMaxHealth / CStats::GetFatAndMuscleModifier(STAT_MOD_10));

    CSprite2d::DrawBarChart(
        x109 - (float)totalWidth + (float)x,
        (float)y,
        totalWidth,
        (uint8)SCREEN_STRETCH_Y(9.0f),
        player->m_fHealth * 100.0f / (float)info->m_nMaxHealth,
        false,
        false,
        true,
        HudColour.GetRGB(HUD_COLOUR_RED),
        CRGBA(0, 0, 0, 0)
    );
}
