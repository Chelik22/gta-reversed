#include "StdInc.h"

#include "AERadioTrackManager.h"
#include "RadioStreamsPC.h"

#include "AEAudioHardware.h"
#include "AEUserRadioTrackManager.h"
#include "AEAudioUtility.h"
#include "AEAudioEnvironment.h"
#include "AEPedSpeechAudioEntity.h" // (int-radio) NEW: CAEPedSpeechAudioEntity::s_bForceAudible used by UpdateRadioVolumes (was only in commented-out code before)

// (int-radio) Integration of batch w2-03-AERadioTrackManager (14 functions, full replacing file).
// Sources: D:\projects\keep\batch_out\w2-03-AERadioTrackManager\0x*.cpp (+STATUS.md), each fragment
// cross-checked against decompiles by the batch author; see notes_int-radio.md for the change list.

auto& AERadioTrackManager = StaticRef<CAERadioTrackManager>(0x8CB6F8);

// (int-radio) FILE-SCOPE globals from the batch fragments (.rdata/.bss addresses of the original).
static auto& s_IdentsIndexRange    = StaticRef<std::array<std::array<int32, 2>, 12>>(0x8C8FB0);                          // ChooseIdentIndex: {first, last} диапазон идентов на станцию (последняя запись - RADIO_TALK)
static auto& s_AdvertIdMin         = StaticRef<int32>(0x8C8B88);                                                         // ChooseAdvertIndex: минимальный ID рекламы
static auto& s_AdvertIdMax         = StaticRef<int32>(0x8C8B8C);                                                         // ChooseAdvertIndex: максимальный ID рекламы
static auto& s_BannedAdverts       = StaticRef<int32[RADIO_COUNT][23]>(0x8CADD0);                                        // ChooseAdvertIndex: запрещённые рекламы по станциям (23 записи на станцию)
static auto& s_DJBanterIndexRange  = StaticRef<std::array<std::array<int32, 2>, RADIO_COUNT>>(0x8C8CB0);                 // ChooseDJBanterIndexFromList: {first, last} диапазон диджейских бантеров на станцию
static auto& s_TalkShowOutroIds    = StaticRef<std::array<int32, 32>>(0x8CAD50);                                         // StopRadio: ID аутро для шоу WCTR, индекс по music track index
static auto& s_bWideScreenOn       = StaticRef<bool>(0xB6F065);                                                          // UpdateRadioVolumes: == TheCamera::m_bWideScreenOn (реальный адрес, чтобы не зависеть от раскладки CCamera)
static auto& s_fVolZero            = StaticRef<float>(0x858B50);                                                         // UpdateRadioVolumes: 0.0f
static auto& s_fMissionAudioDist   = StaticRef<float>(0x858B48);                                                         // UpdateRadioVolumes: 15.0f - дистанция, на которой миссийный звук приглушает радио
static auto& s_fRadioBaseVolumeSub = StaticRef<float>(0x858B90);                                                         // UpdateRadioVolumes: 4.0f
static auto& s_fAmbienceRadioCut   = StaticRef<float>(0x858BA4);                                                         // UpdateRadioVolumes: 20.0f - вычитается при активном ambience-радио (и множитель duck-формулы)
static auto& s_fDuckingBias        = StaticRef<float>(0x85EED4);                                                         // UpdateRadioVolumes: 9.0f
static auto& s_fDuckingFadeFactor  = StaticRef<float>(0x85904C);                                                         // UpdateRadioVolumes: -0.02f
static auto& s_fBassSetting1Cut    = StaticRef<float>(0x858CA0);                                                         // UpdateRadioVolumes: 2.0f - вычитается при eBassSetting(1)
static auto& s_fBassSetting2Boost  = StaticRef<float>(0x858CE8);                                                         // UpdateRadioVolumes: 1.5f - прибавляется при eBassSetting(2)
static auto& s_MusicTracksCount    = StaticRef<std::array<int32, RADIO_COUNT>>(0x8C9010);                                // ChooseMusicTrackIndex: количество музыкальных треков на станцию
static auto& s_SpecialBanter0      = StaticRef<std::array<std::array<int32, 2>, RADIO_COUNT>>(0x8C8BF0);                 // ChooseDJBanterIndex: используется при m_nSpecialDJBanterPending == 0
static auto& s_SpecialBanter1      = StaticRef<std::array<std::array<std::array<int32, 2>, 2>, RADIO_COUNT>>(0x8C8C50);  // ChooseDJBanterIndex: используется при m_nSpecialDJBanterPending == 1 (индекс - m_nSpecialDJBanterIndex)
static auto& s_SpecialBanter2      = StaticRef<std::array<std::array<int32, 0x16>, RADIO_COUNT>>(0x8CB280);              // ChooseDJBanterIndex: используется при m_nSpecialDJBanterPending == 2
static auto& s_BanterList_Riot     = StaticRef<std::array<std::array<int32, 2>, RADIO_COUNT>>(0x8C8E30);                 // ChooseDJBanterIndex: бантры во время бунта
static auto& s_BanterList_Desert   = StaticRef<std::array<std::array<int32, 2>, RADIO_COUNT>>(0x8C8D70);                 // ChooseDJBanterIndex: бантры в пустыне (18:00-20:00)
static auto& s_BanterList_Country  = StaticRef<std::array<std::array<int32, 2>, RADIO_COUNT>>(0x8C8DD0);                 // ChooseDJBanterIndex: бантры в сельской местности (06:00-08:00)
static auto& s_BanterList_House    = StaticRef<std::array<std::array<int32, 2>, RADIO_COUNT>>(0x8C8E90);                 // ChooseDJBanterIndex: бантры перед дождём
static auto& s_BanterList_DrumBass = StaticRef<std::array<std::array<int32, 2>, RADIO_COUNT>>(0x8C8EF0);                 // ChooseDJBanterIndex: недостижимо в оригинале
static auto& s_BanterList_Reggae   = StaticRef<std::array<std::array<int32, 2>, RADIO_COUNT>>(0x8C8F50);                 // ChooseDJBanterIndex: бантры в пасмурную погоду
static constexpr auto MUSIC_TRACKS_PER_STATION = 0x1F;                                                                   // QueueUpTracksForStation: 31 запись на станцию в таблицах треков
static auto& s_MusicIntroRanges    = StaticRef<std::array<std::array<int32, 2>, RADIO_COUNT * MUSIC_TRACKS_PER_STATION>>(0x8C9610); // QueueUpTracksForStation: {first, last} диапазон интро-ID на трек
static auto& s_MusicIntroTracks    = StaticRef<std::array<int32, RADIO_COUNT * MUSIC_TRACKS_PER_STATION>>(0x8C9040);                // QueueUpTracksForStation: основной track-ID каждого интро
static auto& s_MusicOutroRanges    = StaticRef<std::array<std::array<int32, 2>, RADIO_COUNT * MUSIC_TRACKS_PER_STATION>>(0x8CA1B0); // QueueUpTracksForStation: {first, last} диапазон аутро-ID на трек
static auto& s_UserTracksPlaying   = StaticRef<bool>(0xBA67F8);                                                          // TrackRadioStation + CheckForTrackConcatenation: true, пока реально играет user track (dedup - был объявлен в обоих фрагментах)
static auto& s_RetuneDelayThresh   = StaticRef<float>(0xB6F14C);                                                         // CheckForStationRetune: порог задержки ввода перетюна (сравнивается с m_fCameraAverageSpeed)
static auto& s_fRetuneDelayCmp     = StaticRef<float>(0x858C20);                                                         // CheckForStationRetune: 0.9f
static auto& s_PrevPlayTime        = StaticRef<int32>(0x8CBA68);                                                         // Service: значение m_ActiveSettings.PlayTime на предыдущем вызове

void CAERadioTrackManager::InjectHooks() {
    RH_ScopedClass(CAERadioTrackManager);
    RH_ScopedCategory("Audio/Managers");

    RH_ScopedInstall(Load, 0x5D40E0);
    RH_ScopedInstall(Save, 0x5D3EE0);
    RH_ScopedInstall(Initialise, 0x5B9390);
    RH_ScopedInstall(Service, 0x4EB9A0);                                     // (int-radio) снят
    RH_ScopedInstall(DisplayRadioStationName, 0x4E9E50);
    RH_ScopedInstall(CheckForStationRetune, 0x4EB660);
    RH_ScopedInstall(CheckForPause, 0x4EA590);
    RH_ScopedInstall(IsVehicleRadioActive, 0x4E9800);                        // (int-radio) снят
    RH_ScopedInstall(AddDJBanterIndexToHistory, 0x4E97B0);
    RH_ScopedInstall(AddAdvertIndexToHistory, 0x4E9760);
    RH_ScopedInstall(AddIdentIndexToHistory, 0x4E9720);
    RH_ScopedInstall(AddMusicTrackIndexToHistory, 0x4E96C0);
    RH_ScopedOverloadedInstall(StartRadio, "manual", 0x4EB3C0, void (CAERadioTrackManager::*)(eRadioID, eBassSetting, float, bool));
    RH_ScopedOverloadedInstall(StartRadio, "with-settings", 0x4EB550, void (CAERadioTrackManager::*)(const tVehicleAudioSettings&));
    RH_ScopedInstall(CheckForStationRetuneDuringPause, 0x4EB890);
    RH_ScopedInstall(TrackRadioStation, 0x4EAC30);                           // (int-radio) снят
    RH_ScopedInstall(ChooseTracksForStation, 0x4EB180);
    RH_ScopedInstall(CheckForTrackConcatenation, 0x4EA930);                  // (int-radio) снят
    RH_ScopedInstall(QueueUpTracksForStation, 0x4EA670);                     // (int-radio) снят
    RH_ScopedInstall(ChooseDJBanterIndex, 0x4EA2D0);                         // (int-radio) снят
    RH_ScopedInstall(ChooseDJBanterIndexFromList, 0x4E95E0);                 // (int-radio) снят
    RH_ScopedInstall(ChooseAdvertIndex, 0x4E9570);                           // (int-radio) снят
    RH_ScopedInstall(ChooseIdentIndex, 0x4E94C0);                            // (int-radio) снят
    RH_ScopedInstall(ChooseMusicTrackIndex, 0x4EA270);                       // (int-radio) снят
    RH_ScopedInstall(ChooseTalkRadioShow, 0x4E8E40);                         // (int-radio) снят
    RH_ScopedInstall(CheckForMissionStatsChanges, 0x4E8410);
    RH_ScopedInstall(StartTrackPlayback, 0x4EA640);
    RH_ScopedInstall(UpdateRadioVolumes, 0x4EA010);                          // (int-radio) снят
    RH_ScopedInstall(PlayRadioAnnouncement, 0x4E8400);
    RH_ScopedInstall(GetCurrentRadioStationID, 0x4E83F0);
    RH_ScopedInstall(GetRadioStationListenTimes, 0x4E83E0);
    RH_ScopedInstall(GetRadioStationName, 0x4E9E10);
    RH_ScopedInstall(GetRadioStationNameKey, 0x4E8380);
    RH_ScopedInstall(HasRadioRetuneJustStarted, 0x4E8370);
    RH_ScopedInstall(StopRadio, 0x4E9820);                                   // (int-radio) снят
    RH_ScopedInstall(IsRadioOn, 0x4E8350, { .Reversed = true });
    RH_ScopedInstall(InitialiseRadioStationID, 0x4E8330);
    RH_ScopedInstall(SetBassEnhanceOnOff, 0x4E9DB0);
    RH_ScopedInstall(SetBassSetting, 0x4E82F0);
    RH_ScopedInstall(SetRadioAutoRetuneOnOff, 0x4E82E0);
    RH_ScopedInstall(RetuneRadio, 0x4E8290, { .Reversed = true });
    RH_ScopedInstall(ResetStatistics, 0x4E8200);
    RH_ScopedInstall(Reset, 0x4E7F80, { .Reversed = true });
}

// Code from 0x5B9390
CAERadioTrackManager::CAERadioTrackManager(int32 hwClientHandle) :
    m_HwClientHandle{ hwClientHandle },
    m_nUserTrackPlayMode{ AEUserRadioTrackManager.GetUserTrackPlayMode() }
{
    // All constant value inits are done using member init lists

    rng::copy(CStats::GetFullFavoriteRadioStationList(), m_aListenTimes.begin());

    for (auto i = 0u; i < RADIO_COUNT; i++) {
        m_nMusicTrackIndexHistory[i].Reset();
        m_nDJBanterIndexHistory[i].Reset();
        m_nAdvertIndexHistory[i].Reset();
        m_nIdentIndexHistory[i].Reset();
    }

    // [1st radio, off]
    m_RequestedSettings = m_ActiveSettings = tRadioSettings{CAEAudioUtility::GetRandomRadioStation()};
}

// 0x5B9390
bool CAERadioTrackManager::Initialise(int32 channelId) {
    *this = CAERadioTrackManager{};
    return true;
}

// 0x4E8330
void CAERadioTrackManager::InitialiseRadioStationID(eRadioID id) {
    m_RequestedSettings.StationID = m_ActiveSettings.StationID = id;
}

// 0x4E7F80
void CAERadioTrackManager::Reset() {
    m_bInitialised = false;
    m_bDisplayStationName = false;
    rng::copy(CStats::GetFullFavoriteRadioStationList(), m_aListenTimes.begin());

    rng::for_each(m_nDJBanterIndexHistory, &DJBanterIndexHistory::Reset);
    rng::for_each(m_nAdvertIndexHistory, &AdvertIndexHistory::Reset);
    rng::for_each(m_nIdentIndexHistory, &IdentIndexHistory::Reset);
    rng::for_each(m_nMusicTrackIndexHistory, &MusicTrackHistory::Reset);
    rng::for_each(m_aRadioState, [](auto& s) { s.Reset(); });

    m_RequestedSettings = m_ActiveSettings = tRadioSettings{CAEAudioUtility::GetRandomRadioStation()};
    m_nStationsListed = m_nStationsListDown = 0;
    m_nTimeRadioStationRetuned = m_nTimeToDisplayRadioName = 0;
    m_prev = field_60 = 0;
    m_nRetuneStartedTime = 0;
    m_bEnabledInPauseMode = false;
    m_nSavedGameClockDays = m_nSavedGameClockHours = -1;
    m_bRadioAutoSelect = m_bBassEnhance = true;
    m_nSavedRadioStationId = m_iRadioStationMenuRequest = m_iRadioStationScriptRequest = RADIO_INVALID;
    m_nSpecialDJBanterPending = 3; // todo: enum
    m_nSpecialDJBanterIndex = -1;
    m_bPauseMode = m_bRetuneJustStarted = false;
    m_f80 = m_f84 = 0.0f;
    ResetStatistics();
}

// 0x4E8200
void CAERadioTrackManager::ResetStatistics() {
    m_nStatsCitiesPassed = 0;
    m_nStatsLastHitGameClockDays = -1;
    m_nStatsLastHitGameClockHours = -1;
    m_nStatsLastHitTimeOutHours = -1;
    m_nStatsPassedCasino3 = false;
    m_nStatsPassedCasino6 = false;
    m_nStatsPassedCasino10 = false;
    m_nStatsPassedCat1 = false;
    m_nStatsPassedDesert1 = false;
    m_nStatsPassedDesert3 = false;
    m_nStatsPassedDesert5 = false;
    m_nStatsPassedDesert8 = false;
    m_nStatsPassedDesert10 = false;
    m_nStatsPassedFarlie3 = false;
    m_nStatsPassedLAFin2 = false;
    m_nStatsPassedMansion2 = false;
    m_nStatsPassedRyder2 = false;
    m_nStatsPassedRiot1 = false;
    m_nStatsPassedSCrash1 = false;
    m_nStatsPassedStrap4 = false;
    m_nStatsPassedSweet2 = false;
    m_nStatsPassedTruth2 = false;
    m_nStatsPassedVCrash2 = false;
    m_nStatsStartedBadlands = false;
    m_nStatsStartedCat2 = false;
    m_nStatsStartedCrash1 = false;
}

// 0x4E8350
bool CAERadioTrackManager::IsRadioOn() const {
    return m_nMode != eRadioTrackMode::RADIO_STOPPED || m_bInitialised || m_nStationsListed || m_nStationsListDown;
}

// 0x4E8370
bool CAERadioTrackManager::HasRadioRetuneJustStarted() const {
    return m_bRetuneJustStarted;
}

// 0x4E83E0
int32* CAERadioTrackManager::GetRadioStationListenTimes() {
    return m_aListenTimes.data();
}

// 0x4E83F0
eRadioID CAERadioTrackManager::GetCurrentRadioStationID() const {
    return m_RequestedSettings.StationID == RADIO_INVALID ? RADIO_OFF : m_RequestedSettings.StationID;
}

// 0x4E82E0
void CAERadioTrackManager::SetRadioAutoRetuneOnOff(bool enable) {
    m_bRadioAutoSelect = enable;
}

// 0x4E82F0
void CAERadioTrackManager::SetBassSetting(eBassSetting bassSetting, float bassGrain) {
    m_RequestedSettings.BassGain = m_ActiveSettings.BassGain = bassGrain;
    m_RequestedSettings.BassSetting = m_ActiveSettings.BassSetting = bassSetting;
    AEAudioHardware.SetBassSetting(m_bBassEnhance ? bassSetting : eBassSetting::NORMAL, bassGrain);
}

// 0x4E9DB0
void CAERadioTrackManager::SetBassEnhanceOnOff(bool enable) {
    m_bBassEnhance = enable;
    if (m_nMode == eRadioTrackMode::RADIO_PLAYING) {
        m_RequestedSettings.BassSetting = m_ActiveSettings.BassSetting;
        m_RequestedSettings.BassGain = m_ActiveSettings.BassGain;
        if (enable) {
            AEAudioHardware.SetBassSetting(m_ActiveSettings.BassSetting, m_ActiveSettings.BassGain);
        } else {
            AEAudioHardware.SetBassSetting(eBassSetting::NORMAL, m_ActiveSettings.BassGain);
        }
    }
}

// 0x4E8290
void CAERadioTrackManager::RetuneRadio(eRadioID id) {
    const auto retunedStation = [id] {
        if (id == RADIO_USER_TRACKS && !AEUserRadioTrackManager.m_nUserTracksCount) {
            return RADIO_OFF;
        } else {
            return id;
        }
    }();

    if (CTimer::GetIsPaused()) {
        m_iRadioStationMenuRequest = retunedStation;
        m_nRetuneStartedTime = CTimer::GetTimeInMSPauseMode();
    } else {
        m_iRadioStationScriptRequest = retunedStation;
    }
}

// 0x4E9E50
void CAERadioTrackManager::DisplayRadioStationName() {
    if (CTimer::GetIsPaused())
        return;

    if (TheCamera.m_bWideScreenOn)
        return;

    if (!FindPlayerVehicle())
        return;

    if (CReplay::Mode == MODE_PLAYBACK)
        return;

    if (m_bDisplayStationName && IsVehicleRadioActive()) {
        m_nTimeToDisplayRadioName = CTimer::GetTimeInMS() + 2500;
        m_bDisplayStationName = false;
    }

    if (CTimer::GetTimeInMS() < m_nTimeToDisplayRadioName) {
        int station = m_nStationsListed + m_RequestedSettings.StationID;
        if (station) {
            if (station >= RADIO_COUNT) {
                station -= RADIO_COUNT - 1;
            } else if (station <= 0) {
                station += RADIO_COUNT - 1;
            }

            CFont::SetFontStyle(eFontStyle::FONT_MENU);
            CFont::SetJustify(false);
            CFont::SetBackground(false, false);
            CFont::SetScale(SCREEN_SCALE_X(0.6f), SCREEN_SCALE_Y(0.9f));
            CFont::SetProportional(true);
            CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
            CFont::SetRightJustifyWrap(0.0f);
            CFont::SetEdge(1);
            CFont::SetDropColor(CRGBA(0, 0, 0, 255));
            CFont::SetColor(HudColour.GetRGB(m_nStationsListed || m_nStationsListDown ? HUD_COLOUR_DARK_GRAY : HUD_COLOUR_GOLD));
            CFont::PrintString(SCREEN_WIDTH / 2, SCREEN_SCALE_Y(22.0f), GetRadioStationName((eRadioID)station));
            CFont::DrawFonts();
        }
    }
}

// 0x4E9E10
const GxtChar* CAERadioTrackManager::GetRadioStationName(eRadioID id) {
    if (id <= 0) {
        NOTSA_UNREACHABLE();
        return nullptr;
    }

    char key[8];
    GetRadioStationNameKey(id, key);
    return TheText.Get(key);
}

// 0x4E8380
void CAERadioTrackManager::GetRadioStationNameKey(eRadioID id, char* outStr) {
    switch (id) {
    case RADIO_OFF:
        *std::format_to_n(outStr, 7u, "FEA_NON").out = '\0';
        break;
    case RADIO_USER_TRACKS:
        *std::format_to_n(outStr, 7u, "FEA_MP3").out = '\0';
        break;
    default:
        assert(0 <= id && id < RADIO_USER_TRACKS);
        *std::format_to_n(outStr, 7u, "FEA_R{:d}", (int32)id - 1).out = '\0';
        break;
    }
}

// 0x4E9800
bool CAERadioTrackManager::IsVehicleRadioActive() {
    // (int-radio) тело из фрагмента 0x4E9800 (идентично прежнему телу репо, снят только .Reversed = false)
    if (const auto opts = CAEVehicleAudioEntity::StaticGetPlayerVehicleAudioSettingsForRadio()) {
        switch (opts->RadioType) {
        case AE_RT_CIVILIAN:
        case AE_RT_EMERGENCY:
        case AE_RT_UNKNOWN:
            return true;
        default:
            break;
        }
    }
    return false;
}

// 0x4E8410
void CAERadioTrackManager::CheckForMissionStatsChanges() {
    if (m_nSpecialDJBanterPending != 3) {
        int32 days = CClock::GetGameClockDays() - m_nStatsLastHitGameClockDays;
        if (days < 0) {
            auto month = CClock::GetGameClockMonth() - 1;
            if (month < 0) {
                month += 12;
            }
            days += CClock::daysInMonth[month];
        }
        if (CClock::GetGameClockHours() + 24 * days - m_nStatsLastHitGameClockHours >= m_nStatsLastHitTimeOutHours) {
            m_nSpecialDJBanterPending = 3;
        }
    }

    const auto statsCitiesPassed = CStats::GetStatValue<uint8>(STAT_CITY_UNLOCKED);
    if (m_nStatsCitiesPassed < statsCitiesPassed) {
        m_nStatsCitiesPassed = statsCitiesPassed;
        if (statsCitiesPassed == 1 || statsCitiesPassed == 2) {
            m_nStatsLastHitGameClockDays = CClock::GetGameClockDays();
            m_nStatsLastHitGameClockHours = CClock::GetGameClockHours();
            m_nStatsLastHitTimeOutHours = 24;
            m_nSpecialDJBanterPending = 1;
            m_nSpecialDJBanterIndex = m_nStatsCitiesPassed - 1;
        }
    }

    const auto Update = [](uint8& inputStat, const eStats stat, const auto specialDJBanterIndex) {
        const auto statValue = CStats::GetStatValue<uint8>(stat);
        if (inputStat < statValue) {
            inputStat = statValue;
            if (inputStat == 1) {
                m_nStatsLastHitGameClockDays = CClock::GetGameClockDays();
                m_nStatsLastHitGameClockHours = CClock::GetGameClockHours();
                m_nStatsLastHitTimeOutHours = 24 * 7;
                m_nSpecialDJBanterPending = 2;
                m_nSpecialDJBanterIndex = specialDJBanterIndex;
            }
        }
    };

    Update(m_nStatsPassedCasino3, STAT_LEAST_FAVORITE_RADIO_STATION, 0);
    Update(m_nStatsPassedCasino6, STAT_CURRENT_WEAPON_SKILL, 1);
    Update(m_nStatsPassedCasino10, STAT_WEAPON_SKILL_LEVELS, 2);
    Update(m_nStatsPassedCat1, STAT_LOCAL_LIQUOR_STORE_MISSION_ACCOMPLISHED, 3);
    Update(m_nStatsPassedDesert1, STAT_PLAYING_TIME, 4);
    Update(m_nStatsPassedDesert3, STAT_PILOT_RANKING, 5);
    Update(m_nStatsPassedDesert5, STAT_STRONGEST_GANG, 6);
    Update(m_nStatsPassedDesert8, STAT_2ND_STRONGEST_GANG, 7);
    Update(m_nStatsPassedDesert10, STAT_3RD_STRONGEST_GANG, 8);
    Update(m_nStatsPassedFarlie3, STAT_MIKE_TORENO_MISSION_ACCOMPLISHED, 9);
    Update(m_nStatsPassedLAFin2, STAT_LEAST_FAVORITE_GANG, 10);
    Update(m_nStatsPassedMansion2, STAT_A_HOME_IN_THE_HILLS_MISSION_ACCOMPLISHED, 11);
    Update(m_nStatsPassedRyder2, STAT_RYDERS_MISSION_ROBBING_UNCLE_SAM_ACCOMPLISHED, 12);
    Update(m_nStatsPassedRiot1, STAT_RIOT_MISSION_ACCOMPLISHED, 13);
    Update(m_nStatsPassedSCrash1, STAT_GANG_STRENGTH, 14);
    Update(m_nStatsPassedStrap4, STAT_TERRITORY_UNDER_CONTROL, 15);
    Update(m_nStatsPassedSweet2, STAT_DRIVE_THRU_MISSION_ACCOMPLISHED, 16);
    Update(m_nStatsPassedTruth2, STAT_ARE_YOU_GOING_TO_SAN_FIERRO_MISSION_ACCOMPLISHED, 17);
    Update(m_nStatsPassedVCrash2, STAT_HIGH_NOON_MISSION_ACCOMPLISHED, 18);
    Update(m_nStatsStartedBadlands, STAT_THE_GREEN_SABRE_MISSION_ACCOMPLISHED, 19);
    Update(m_nStatsStartedCat2, STAT_MAYBE_CATALINA_MEETING, 20);
    Update(m_nStatsStartedCrash1, STAT_MAYBE_WU_ZI_MEETING, 21);
}

// 0x4EA930
void CAERadioTrackManager::CheckForTrackConcatenation() {
    // (int-radio) тело из фрагмента 0x4EA930 (redirect-стаб заменён)
    int8 count = 1; // NOTE: изначально 1 (и 2, если смена play mode ниже что-то поставила)

    if (m_ActiveSettings.StationID == RADIO_USER_TRACKS) {
        const auto queuedPlayMode = m_nUserTrackPlayMode;
        const auto currPlayMode   = AEUserRadioTrackManager.GetUserTrackPlayMode();
        if (queuedPlayMode != currPlayMode) {
            if ((queuedPlayMode == 2 || currPlayMode == 2) && m_ActiveSettings.PlayTime != -4) {
                AEUserRadioTrackManager.SetUserTrackIndex(m_ActiveSettings.TrackQueue[0]);
                m_ActiveSettings.TrackQueue[1]   = AEUserRadioTrackManager.SelectUserTrackIndex();
                m_ActiveSettings.TrackTypes[1]   = TYPE_USER_TRACK;
                m_ActiveSettings.TrackIndices[1] = (int8)m_ActiveSettings.TrackQueue[1];
                count = 2;
                AEAudioHardware.PlayTrack(
                    m_ActiveSettings.TrackQueue[0],
                    m_ActiveSettings.TrackQueue[1],
                    0,
                    (uint8)m_ActiveSettings.TrackFlags,
                    m_ActiveSettings.TrackTypes[0] == TYPE_USER_TRACK,
                    m_ActiveSettings.TrackTypes[1] == TYPE_USER_TRACK
                );
            }
            m_nUserTrackPlayMode = AEUserRadioTrackManager.GetUserTrackPlayMode();
        }
    }

    if (AEAudioHardware.GetActiveTrackID() != m_ActiveSettings.TrackQueue[0]
        || m_ActiveSettings.TrackQueue[0] < 0) {
        return;
    }

    // Сдвигаем активную очередь на одну позицию вниз
    m_ActiveSettings.PrevTrackID   = m_ActiveSettings.TrackQueue[0];
    m_ActiveSettings.PrevTrackType = m_ActiveSettings.TrackTypes[0];
    m_ActiveSettings.PrevTrackIdx  = m_ActiveSettings.TrackIndices[0];
    for (auto i = 0; i < 4; i++) {
        m_ActiveSettings.TrackQueue[i]   = m_ActiveSettings.TrackQueue[i + 1];
        m_ActiveSettings.TrackTypes[i]   = m_ActiveSettings.TrackTypes[i + 1];
        m_ActiveSettings.TrackIndices[i] = m_ActiveSettings.TrackIndices[i + 1];
    }
    m_ActiveSettings.TrackQueue[4]   = -1;
    m_ActiveSettings.TrackTypes[4]   = TYPE_NONE;
    m_ActiveSettings.TrackIndices[4] = -1;

    if (m_ActiveSettings.TrackQueue[0] == -1) {
        if (m_ActiveSettings.StationID == RADIO_USER_TRACKS) {
            if (!s_UserTracksPlaying && CAEAudioUtility::ResolveProbability(0.17f)) {
                m_ActiveSettings.TrackQueue[count]   = ChooseAdvertIndex(RADIO_USER_TRACKS);
                m_ActiveSettings.TrackTypes[count]   = TYPE_ADVERT;
                count++;
            }
            m_ActiveSettings.TrackQueue[count]   = AEUserRadioTrackManager.SelectUserTrackIndex();
            m_ActiveSettings.TrackTypes[count]   = TYPE_USER_TRACK;
            m_ActiveSettings.TrackIndices[count] = (int8)m_ActiveSettings.TrackQueue[count];
            count++;
            m_ActiveSettings.TrackQueue[count]   = AEUserRadioTrackManager.SelectUserTrackIndex();
            m_ActiveSettings.TrackTypes[count]   = TYPE_USER_TRACK;
            m_ActiveSettings.TrackIndices[count] = (int8)m_ActiveSettings.TrackQueue[count];
        } else {
            int8 queueCount = 0; // NOTE: в оригинале здесь отдельный (нулевой) счётчик
            switch (m_ActiveSettings.TrackTypes[0]) {
            case TYPE_INTRO:
            case TYPE_TRACK:
            case TYPE_OUTRO: {
                if (m_ActiveSettings.StationID == RADIO_EMERGENCY_AA) {
                    QueueUpTracksForStation(RADIO_EMERGENCY_AA, &queueCount, TYPE_DJ_BANTER, m_ActiveSettings);
                } else if (m_nTracksInARow[m_ActiveSettings.StationID] < 2) {
                    if (CAEAudioUtility::ResolveProbability(0.5f)) {
                        QueueUpTracksForStation(m_ActiveSettings.StationID, &queueCount, TYPE_INDENT, m_ActiveSettings);
                    }
                    QueueUpTracksForStation(m_ActiveSettings.StationID, &queueCount, TYPE_INTRO, m_ActiveSettings);
                } else {
                    if (CAEAudioUtility::ResolveProbability(0.5f)) {
                        QueueUpTracksForStation(m_ActiveSettings.StationID, &queueCount, TYPE_INDENT, m_ActiveSettings);
                    }
                    if (QueueUpTracksForStation(m_ActiveSettings.StationID, &queueCount, TYPE_DJ_BANTER, m_ActiveSettings)) {
                        goto play; // очередь уже достаточно заполнена - играем сразу
                    }
                    QueueUpTracksForStation(m_ActiveSettings.StationID, &queueCount, TYPE_ADVERT, m_ActiveSettings);
                }
                break;
            }
            default:
                QueueUpTracksForStation(m_ActiveSettings.StationID, &queueCount, TYPE_INTRO, m_ActiveSettings);
                break;
            }
        }
    }

play:
    AEAudioHardware.PlayTrack(
        m_ActiveSettings.TrackQueue[0],
        m_ActiveSettings.TrackQueue[1],
        0,
        (uint8)m_ActiveSettings.TrackFlags,
        m_ActiveSettings.TrackTypes[0] == TYPE_USER_TRACK,
        m_ActiveSettings.TrackTypes[1] == TYPE_USER_TRACK
    );
}

// 0x4EB660
void CAERadioTrackManager::CheckForStationRetune() {
    // (int-radio) тело из фрагмента 0x4EB660 (заменило прежнее неполное тело репо - см. notes_int-radio.md)
    if (m_ActiveSettings.StationID == RADIO_EMERGENCY_AA /* 0 */) {
        return;
    }

    const auto mode = m_nMode;
    m_bRetuneJustStarted = false;

    if (   mode == eRadioTrackMode::RADIO_STARTING
        || mode == eRadioTrackMode::RADIO_WAITING_TO_PLAY
        || mode == eRadioTrackMode::RADIO_PLAYING
        || m_bInitialised
        || m_nStationsListed != 0
        || m_nStationsListDown != 0
        || m_ActiveSettings.StationID == RADIO_OFF) {
        if (!AudioEngine.GetCutsceneTrackStatus()) {
            const auto vehSettings = CAEVehicleAudioEntity::StaticGetPlayerVehicleAudioSettingsForRadio();
            if (vehSettings
                && (   vehSettings->RadioType == AE_RT_CIVILIAN
                    || vehSettings->RadioType == AE_RT_EMERGENCY
                    || vehSettings->RadioType == AE_RT_UNKNOWN)
                && CReplay::Mode != MODE_PLAYBACK
            ) {
                if (vehSettings->RadioType != AE_RT_CIVILIAN) {
                    return; // к ручному перетюну продолжаем только с гражданским радио
                }

                if (m_iRadioStationScriptRequest < 0) {
                    const auto pad = CPad::GetPad(0);
                    if (pad->NextStationJustUp()) {
                        m_nStationsListDown = m_nStationsListed;
                        m_nStationsListed++;
                        m_nTimeRadioStationRetuned = CTimer::m_snTimeInMilliseconds;
                    } else if (pad->LastStationJustUp()) {
                        m_nStationsListDown = m_nStationsListed;
                        m_nStationsListed--;
                        m_nTimeRadioStationRetuned = CTimer::m_snTimeInMilliseconds;
                    } else {
                        goto noRetune;
                    }
                } else {
                    m_nStationsListDown = m_nStationsListed;
                    m_nStationsListed   = m_iRadioStationScriptRequest - (int32)m_RequestedSettings.StationID; // NOTE: +0xAD (запрошенные настройки)
                    m_iRadioStationScriptRequest = -1;
                    m_nTimeRadioStationRetuned   = CTimer::m_snTimeInMilliseconds;
                }
                m_bDisplayStationName = true;
                m_bRetuneJustStarted  = true;
            }
        }
    }

noRetune:
    if (m_nStationsListed != 0 || m_nStationsListDown != 0) {
        auto newStation = (int8)((int8)m_RequestedSettings.StationID + (int8)m_nStationsListed); // NOTE: +0xAD (запрошенные настройки)
        if (newStation < 1) {
            newStation += 13;
        } else if (newStation > 13) {
            newStation -= 13;
        }

        if (newStation == (int8)RADIO_OFF || (newStation == (int8)RADIO_USER_TRACKS && AEUserRadioTrackManager.m_nUserTracksCount == 0)) {
            StopRadio(nullptr, false);
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_CLICK_OFF);
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_STOP);
        } else {
            if (m_ActiveSettings.StationID == RADIO_OFF) {
                AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_CLICK_ON);
                m_ActiveSettings.StationID = RADIO_INVALID;
            } else {
                StopRadio(nullptr, false);
            }
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_START);

            const auto delay = s_RetuneDelayThresh <= s_fRetuneDelayCmp /* 0.9f */ ? 2000u : 4000u;
            if (CTimer::m_snTimeInMilliseconds <= m_nTimeRadioStationRetuned + 1500u) {
                return;
            }
            if (CTimer::m_snTimeInMilliseconds <= field_60 + delay) {
                return;
            }
        }

        StartRadio((eRadioID)newStation, m_ActiveSettings.BassSetting, m_ActiveSettings.BassGain, false);
        m_nStationsListed   = 0;
        m_nStationsListDown = 0;
    }
}

// 0x4EB890
void CAERadioTrackManager::CheckForStationRetuneDuringPause() {
    if (m_ActiveSettings.StationID == RADIO_EMERGENCY_AA && IsRadioOn() || m_iRadioStationMenuRequest <= RADIO_INVALID)
        return;

    if (m_iRadioStationMenuRequest != RADIO_OFF) {
        if (m_ActiveSettings.StationID == RADIO_OFF) {
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_CLICK_ON);
            m_ActiveSettings.StationID = RADIO_INVALID;
        } else {
            AudioEngine.StopRadio(nullptr, true);
        }

        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_START);
        if (CTimer::GetTimeInMSPauseMode() > m_nRetuneStartedTime + 700u) {
            StartRadio((eRadioID)m_iRadioStationMenuRequest, m_ActiveSettings.BassSetting, m_ActiveSettings.BassGain, 0);
            m_iRadioStationMenuRequest = RADIO_INVALID;
        }
    } else {
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_CLICK_OFF);
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_STOP);
        StartRadio(RADIO_OFF, m_ActiveSettings.BassSetting, m_ActiveSettings.BassGain, 0);
        m_iRadioStationMenuRequest = RADIO_INVALID;
    }
}

// 0x4EA640
void CAERadioTrackManager::StartTrackPlayback() {
    AEAudioHardware.SetChannelFlags(m_HwClientHandle, 0, 55);
    AEAudioHardware.StartTrackPlayback();
    UpdateRadioVolumes();
}

// 0x4EA010
void CAERadioTrackManager::UpdateRadioVolumes() {
    // (int-radio) тело из фрагмента 0x4EA010 (redirect-стаб заменён)
    auto volume = -4.0f;

    const bool bPausedButEnabled = (CTimer::m_UserPause || CTimer::m_CodePause) && m_bEnabledInPauseMode;
    if (!bPausedButEnabled) {
        if (CTimer::GetIsSlowMotionActive()) {
            volume = -100.0f;
        } else if (s_bWideScreenOn /* TheCamera::m_bWideScreenOn */) {
            volume = -16.0f;
        } else {
            const auto fxScale  = AEAudioHardware.GetEffectsMasterScalingFactor();
            const auto musScale = AEAudioHardware.GetMusicMasterScalingFactor();
            if (fxScale > s_fVolZero && musScale > s_fVolZero) {
                // Приглушать радио, если рядом с камерой играет миссийный звук.
                // В оригинале force-audible-речь заставляет считать радио "приглушённым" безусловно.
                auto bNearMissionAudio = CAEPedSpeechAudioEntity::s_bForceAudible;
                for (auto i = 0; i < 2 && !bNearMissionAudio; i++) {
                    if (AudioEngine.IsMissionAudioSampleFinished(i) || AudioEngine.GetMissionAudioEvent(i) == 0xFFFF) {
                        continue;
                    }
                    if (const auto missionAudioPos = AudioEngine.GetMissionAudioPosition(i)) {
                        const auto relPos = CAEAudioEnvironment::GetPositionRelativeToCamera(*missionAudioPos);
                        if (relPos.Magnitude() < s_fMissionAudioDist) {
                            bNearMissionAudio = true;
                        }
                    } else {
                        bNearMissionAudio = true;
                    }
                }

                if (bNearMissionAudio) {
                    auto duckVolume = log2f(fxScale / musScale) * 0.3010299956639812f * s_fAmbienceRadioCut - s_fDuckingBias;
                    if (s_fVolZero <= duckVolume) { // оригинал зажимает только положительные значения в 0
                        duckVolume = 0.0f;
                    }
                    m_f80 = duckVolume;
                    m_f84 = s_fDuckingFadeFactor * duckVolume;
                    volume = duckVolume - s_fRadioBaseVolumeSub;
                } else if (m_f80 < s_fVolZero) {
                    // Плавно возвращаем громкость из ducking'а
                    m_f80 = std::min(m_f80 + m_f84, 0.0f);
                    volume = m_f80 - s_fRadioBaseVolumeSub;
                }
                // NOTE: если `m_f80 >= 0` оригинал ничего не делает (громкость остаётся дефолтной)
            }
        }

        if (CAudioEngine::IsAmbienceRadioActive()) {
            volume -= s_fAmbienceRadioCut;
        }
    }

    if (m_bBassEnhance && m_ActiveSettings.BassSetting != (eBassSetting)0) {
        switch (m_ActiveSettings.BassSetting) {
        case (eBassSetting)1:
            volume -= s_fBassSetting1Cut;
            break;
        case (eBassSetting)2:
            volume += s_fBassSetting2Boost;
            break;
        }
    }

    AEAudioHardware.SetChannelVolume((int16)m_HwClientHandle, 0, volume, 0);
}

// 0x4E8400
void CAERadioTrackManager::PlayRadioAnnouncement(uint32) {
    // NOP
}

// 0x4EB550
void CAERadioTrackManager::StartRadio(const tVehicleAudioSettings& settings) {
    // plugin::CallMethod<0x4EB550, CAERadioTrackManager*, tVehicleAudioSettings*>(this, settings);

    if (CReplay::Mode == MODE_PLAYBACK)
        return;

    if (settings.RadioType == AE_RT_EMERGENCY) {
        StartRadio(RADIO_EMERGENCY_AA, settings.BassSetting, settings.BassFactor, 0);
        return;
    }

    if (settings.RadioType != AE_RT_CIVILIAN)
        return;

    const bool needsRetune = [&] {
       if (!m_bRadioAutoSelect)
           return false;

       const auto savedId = m_nSavedRadioStationId;
       if (savedId < 0 || savedId == settings.RadioStation || savedId == RADIO_OFF || savedId == RADIO_EMERGENCY_AA)
           return false;

       if (CTimer::GetTimeInMS() > m_nSavedTimeMs + 60'000)
           return false;

       const auto savedHours = m_nSavedGameClockHours;
       auto savedDays = m_nSavedGameClockDays;
       if (savedHours < 0 || savedDays < 0)
           return false;

       if (savedDays > CClock::GetGameClockDays()) {
           const auto month = CClock::GetGameClockMonth();
           savedDays += CClock::daysInMonth[month == 0 ? 11 : month - 1]; // prev month
       }

       if (CClock::GetGameClockHours() + 24 * savedDays - savedHours > 5)
           return false;

       return true;
    }();

    if (needsRetune) {
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_START);
        StartRadio((eRadioID)m_nSavedRadioStationId, settings.BassSetting, settings.BassFactor, 0);
    } else {
        StartRadio(settings.RadioStation, settings.BassSetting, settings.BassFactor, 0);
    }
}

// 0x4EB3C0
void CAERadioTrackManager::StartRadio(eRadioID id, eBassSetting bassSetting, float bassGain, bool skipTrack) {
    id = std::min(id, RADIO_OFF);

    if (CTimer::GetIsPaused()) {
        m_bEnabledInPauseMode = true;

        if (IsRadioOn() && id == m_ActiveSettings.StationID) {
            m_aRadioState[id].m_iTimeInPauseModeInMs = CTimer::GetTimeInMSPauseMode();
            return;
        }
    }

    if (id != RADIO_OFF && CAudioEngine::IsAmbienceTrackActive()) {
        if (!CTimer::GetIsPaused() && CAudioEngine::DoesAmbienceTrackOverrideRadio()) {
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_STOP);
            return;
        }
        AudioEngine.StopAmbienceTrack(false);
    }

    auto* const rs = &m_RequestedSettings;
    rs->StationID = id;
    rs->BassSetting             = bassSetting;
    rs->BassGain            = bassGain;
    if (id == RADIO_OFF) {
        rs->Reset();
    } else if (m_aRadioState[id].m_iTimeInMs < 0 || !TrackRadioStation(id, skipTrack)) {
        ChooseTracksForStation(rs->StationID);
        rs->PlayTime = CAEAudioUtility::GetRandomNumberInRange(0, 300'000);
    }
    switch (m_nMode) {
    case eRadioTrackMode::RADIO_STARTING:
    case eRadioTrackMode::RADIO_WAITING_TO_PLAY:
    case eRadioTrackMode::RADIO_PLAYING:
        m_nMode = eRadioTrackMode::RADIO_STOPPING;
    }
    m_aRadioState[rs->StationID].m_iTimeInPauseModeInMs = -1;

    m_bInitialised = true;
}

// 0x4EAC30
bool CAERadioTrackManager::TrackRadioStation(eRadioID id, bool skipTrack) {
    // (int-radio) тело из фрагмента 0x4EAC30 (redirect-стаб заменён)
    const auto stationIdx = (int32)id;
    auto& state = m_aRadioState[stationIdx];

    // Продолжать можно, только если станцию не слушали последние 5 часов
    if (state.m_nGameClockHours >= 0 && state.m_nGameClockDays >= 0) {
        auto hoursPassed = (int32)(CClock::ms_nGameClockDays - state.m_nGameClockDays);
        if (hoursPassed < 0) {
            auto month = (int32)CClock::ms_nGameClockMonth - 1;
            if (month < 0) {
                month = (int32)CClock::ms_nGameClockMonth + 11;
            }
            hoursPassed += CClock::daysInMonth[month];
        }
        if (hoursPassed * 24 - state.m_nGameClockHours + CClock::ms_nGameClockHours > 5) {
            return false;
        }
    }

    auto& queue   = m_RequestedSettings.TrackQueue;
    auto& types   = m_RequestedSettings.TrackTypes;
    auto& indices = m_RequestedSettings.TrackIndices;

    // Позиция воспроизведения внутри сохранённого блока станции
    auto pos = CTimer::m_snTimeInMilliseconds - state.m_iTimeInMs;
    if (pos < 0x1B59 /* 7001 */) {
        pos = 7000;
    }
    if (skipTrack && (uint32)(state.m_aElapsed[0] + 1) >= (uint32)pos) {
        pos = state.m_aElapsed[0] + 1;
    }

    // Сброс запрошенных настроек
    for (auto i = 0u; i < tRadioSettings::NUM_TRACKS; i++) {
        queue[i]   = -1;
        types[i]   = TYPE_NONE;
        indices[i] = -1;
    }

    int8  count = 0;
    uint32 total = 0;
    for (auto i = 0; i < 3; i++) {
        total += state.m_aElapsed[i];
        if (pos <= total) {
            if (i == 0) {
                m_RequestedSettings.PlayTime = state.m_iTrackPlayTime + pos;
            } else {
                m_RequestedSettings.PlayTime = state.m_aElapsed[i] - total + pos;
            }

            switch (state.m_aTrackTypes[i]) {
            case TYPE_INDENT:
            case TYPE_ADVERT:
            case TYPE_DJ_BANTER: {
                queue[0] = state.m_aTrackQueue[i];
                types[0] = state.m_aTrackTypes[i];
                count    = 1;
                if (stationIdx == (int32)RADIO_USER_TRACKS) {
                    QueueUpTracksForStation(RADIO_USER_TRACKS, &count, TYPE_TRACK, m_RequestedSettings);
                } else {
                    QueueUpTracksForStation(id, &count, TYPE_INTRO, m_RequestedSettings);
                }
                return true;
            }
            case TYPE_INTRO: { // Интро -> трек -> аутро: копируем все 3 записи
                for (auto j = 0; j < 3; j++) {
                    queue[j] = state.m_aTrackQueue[i + j];
                    types[j] = state.m_aTrackTypes[i + j];
                }
                return true;
            }
            case TYPE_TRACK: { // Трек -> аутро: копируем обе записи
                for (auto j = 0; j < 2; j++) {
                    queue[j] = state.m_aTrackQueue[i + j];
                    types[j] = state.m_aTrackTypes[i + j];
                }
                return true;
            }
            case TYPE_OUTRO: {
                queue[0] = state.m_aTrackQueue[i];
                types[0] = state.m_aTrackTypes[i];
                count    = 1;
                if (stationIdx == (int32)RADIO_EMERGENCY_AA /* 0 */) {
                    QueueUpTracksForStation(RADIO_EMERGENCY_AA, &count, TYPE_DJ_BANTER, m_RequestedSettings);
                    return true;
                }
                if (CAEAudioUtility::ResolveProbability(0.5f)) {
                    QueueUpTracksForStation(id, &count, TYPE_INDENT, m_RequestedSettings);
                }
                if (!QueueUpTracksForStation(id, &count, TYPE_DJ_BANTER, m_RequestedSettings)) {
                    QueueUpTracksForStation(id, &count, TYPE_ADVERT, m_RequestedSettings);
                }
                return true;
            }
            case TYPE_USER_TRACK: {
                for (auto j = 0; j < 2; j++) {
                    queue[j] = state.m_aTrackQueue[i + j];
                    types[j] = state.m_aTrackTypes[i + j];
                }
                if (queue[1] == -1) {
                    queue[1]   = AEUserRadioTrackManager.SelectUserTrackIndex();
                    types[1]   = TYPE_USER_TRACK;
                    indices[1] = (int8)queue[1];
                }
                return true;
            }
            default:
                return false;
            }
        }
    }

    // Ничего из сохранённого блока не подходит под текущую позицию - ставим новый блок
    if (pos <= total + 7000) {
        if (stationIdx == (int32)RADIO_USER_TRACKS) {
            QueueUpTracksForStation(RADIO_USER_TRACKS, &count, TYPE_TRACK, m_RequestedSettings);
            QueueUpTracksForStation(RADIO_USER_TRACKS, &count, TYPE_TRACK, m_RequestedSettings);
        } else {
            QueueUpTracksForStation(id, &count, TYPE_INTRO, m_RequestedSettings);
        }
        pos -= total;
        m_RequestedSettings.PlayTime = std::min<uint32>(pos, 5000);
        return true;
    }
    if (pos <= total + 0x25D78 /* 155000 */) {
        QueueUpTracksForStation(id, &count, TYPE_TRACK, m_RequestedSettings);
        m_RequestedSettings.PlayTime = pos - total - 5000;
        return true;
    }
    if (total + 160000 < pos) {
        return false;
    }

    if (stationIdx == (int32)RADIO_USER_TRACKS) {
        QueueUpTracksForStation(RADIO_USER_TRACKS, &count, TYPE_TRACK, m_RequestedSettings);
        if (!s_UserTracksPlaying && CAEAudioUtility::ResolveProbability(0.17f)) {
            QueueUpTracksForStation(RADIO_USER_TRACKS, &count, TYPE_ADVERT, m_RequestedSettings);
        }
    } else {
        QueueUpTracksForStation(id, &count, TYPE_OUTRO, m_RequestedSettings);
        AddMusicTrackIndexToHistory(id, indices[count - 1]);
        if (stationIdx == (int32)RADIO_EMERGENCY_AA /* 0 */) {
            QueueUpTracksForStation(RADIO_EMERGENCY_AA, &count, TYPE_DJ_BANTER, m_RequestedSettings);
        } else {
            if (CAEAudioUtility::ResolveProbability(0.5f)) {
                if (CAEAudioUtility::ResolveProbability(0.5f)) {
                    QueueUpTracksForStation(id, &count, TYPE_INDENT, m_RequestedSettings);
                }
                QueueUpTracksForStation(id, &count, TYPE_INTRO, m_RequestedSettings);
            } else {
                if (CAEAudioUtility::ResolveProbability(0.5f)) {
                    QueueUpTracksForStation(id, &count, TYPE_INDENT, m_RequestedSettings);
                }
                if (!QueueUpTracksForStation(id, &count, TYPE_DJ_BANTER, m_RequestedSettings)) {
                    QueueUpTracksForStation(id, &count, TYPE_ADVERT, m_RequestedSettings);
                }
            }
        }
    }
    m_RequestedSettings.PlayTime = pos - total - 0x25D78;
    return true;
}

// 0x4EA670
bool CAERadioTrackManager::QueueUpTracksForStation(eRadioID id, int8* iTrackCount, int8 radioState, tRadioSettings& settings) {
    // (int-radio) тело из фрагмента 0x4EA670 (redirect-стаб заменён)
    const auto stationIdx = (int32)id;
    auto& count = *iTrackCount;

    switch (radioState) {
    case TYPE_INDENT: { // Идент
        if (id == RADIO_USER_TRACKS) {
            break;
        }
        settings.TrackQueue[count] = ChooseIdentIndex(id);
        if (settings.TrackQueue[count] != -1) {
            settings.TrackTypes[count] = TYPE_INDENT;
            count++;
            return true;
        }
        break;
    }
    case TYPE_ADVERT: { // Реклама
        settings.TrackQueue[count] = ChooseAdvertIndex(id);
        settings.TrackTypes[count] = TYPE_ADVERT;
        count++;
        return true;
    }
    case TYPE_DJ_BANTER: { // Диджейский бантер
        if (id == RADIO_USER_TRACKS) {
            break;
        }
        settings.TrackQueue[count] = ChooseDJBanterIndex(id);
        if (settings.TrackQueue[count] != -1) {
            settings.TrackTypes[count] = TYPE_DJ_BANTER;
            count++;
            return true;
        }
        break;
    }
    case TYPE_INTRO: { // Интро -> трек -> аутро
        if (id == RADIO_USER_TRACKS) {
            break;
        }
        const auto musicIdx = ChooseMusicTrackIndex(id);
        settings.TrackIndices[count] = musicIdx;
        const auto baseIdx = stationIdx * MUSIC_TRACKS_PER_STATION;

        settings.TrackQueue[count] = CAEAudioUtility::GetRandomNumberInRange(s_MusicIntroRanges[musicIdx + baseIdx][0], s_MusicIntroRanges[musicIdx + baseIdx][1]);
        settings.TrackTypes[count] = TYPE_INTRO;
        count++;

        settings.TrackIndices[count] = settings.TrackIndices[count - 1];
        settings.TrackQueue[count]   = s_MusicIntroTracks[settings.TrackIndices[count] + baseIdx];
        settings.TrackTypes[count]   = TYPE_TRACK;
        count++;

        settings.TrackIndices[count] = settings.TrackIndices[count - 1];
        const auto outroIdx = settings.TrackIndices[count] + baseIdx;
        settings.TrackQueue[count] = CAEAudioUtility::GetRandomNumberInRange(s_MusicOutroRanges[outroIdx][0], s_MusicOutroRanges[outroIdx][1]);
        settings.TrackTypes[count] = TYPE_OUTRO;
        count++;
        return true;
    }
    case TYPE_TRACK: { // Трек (+аутро)
        if (id == RADIO_USER_TRACKS) {
            settings.TrackQueue[count]   = AEUserRadioTrackManager.SelectUserTrackIndex();
            settings.TrackTypes[count]   = TYPE_USER_TRACK;
            settings.TrackIndices[count] = (int8)settings.TrackQueue[count];
            count++;
            return true;
        }
        const auto musicIdx = ChooseMusicTrackIndex(id);
        settings.TrackIndices[count] = musicIdx;
        const auto baseIdx = stationIdx * MUSIC_TRACKS_PER_STATION;

        settings.TrackQueue[count] = s_MusicIntroTracks[musicIdx + baseIdx];
        settings.TrackTypes[count] = TYPE_TRACK;
        count++;

        settings.TrackIndices[count] = settings.TrackIndices[count - 1];
        const auto outroIdx = settings.TrackIndices[count] + baseIdx;
        settings.TrackQueue[count] = CAEAudioUtility::GetRandomNumberInRange(s_MusicOutroRanges[outroIdx][0], s_MusicOutroRanges[outroIdx][1]);
        settings.TrackTypes[count] = TYPE_OUTRO;
        count++;
        return true;
    }
    case TYPE_OUTRO: { // Аутро
        if (id == RADIO_USER_TRACKS) {
            break;
        }
        const auto musicIdx = ChooseMusicTrackIndex(id);
        settings.TrackIndices[count] = musicIdx;
        const auto outroIdx = musicIdx + stationIdx * MUSIC_TRACKS_PER_STATION;
        settings.TrackQueue[count] = CAEAudioUtility::GetRandomNumberInRange(s_MusicOutroRanges[outroIdx][0], s_MusicOutroRanges[outroIdx][1]);
        settings.TrackTypes[count] = TYPE_OUTRO;
        count++;
        return true;
    }
    default:
        return true;
    }
    return false;
}

// 0x4E9820
void CAERadioTrackManager::StopRadio(tVehicleAudioSettings* settings, bool duringPause) {
    // (int-radio) тело из фрагмента 0x4E9820 (redirect-стаб заменён)
    const auto mode = m_nMode;
    if (mode == eRadioTrackMode::RADIO_STARTING
        || mode == eRadioTrackMode::RADIO_WAITING_TO_PLAY
        || mode == eRadioTrackMode::RADIO_PLAYING) {
        if ((!CTimer::m_UserPause && !CTimer::m_CodePause) || duringPause) {
            m_nMode = eRadioTrackMode::RADIO_STOPPING;
        }

        const auto stationIdx = (int32)m_ActiveSettings.StationID;
        auto& state = m_aRadioState[stationIdx]; // NOTE: оригинал также пишет в индекс `station + 6` (вылезает за конец массива) - bug compatible

        state.m_aElapsed[0]     = 0;
        state.m_iTrackPlayTime  = -1;
        state.m_aTrackQueue[0]  = -1;
        state.m_aTrackTypes[0]  = TYPE_NONE;

        state.m_aElapsed[1]     = 0;
        state.m_iTrackPlayTime  = -1;
        state.m_aTrackQueue[1]  = -1;
        state.m_aTrackTypes[1]  = TYPE_NONE;

        m_aRadioState[stationIdx + 6].m_aElapsed[0] = 0;
        state.m_iTrackPlayTime  = -1;
        state.m_aTrackQueue[2]  = -1;
        state.m_aTrackTypes[2]  = TYPE_NONE;

        state.m_iTimeInMs       = CTimer::m_snTimeInMilliseconds;
        state.m_nGameClockDays  = CClock::ms_nGameClockDays;
        state.m_nGameClockHours = CClock::ms_nGameClockHours;

        // Начисляем время прослушивания этой станции
        if (state.m_iTimeInPauseModeInMs > -1
            && stationIdx != (int32)RADIO_EMERGENCY_AA
            && stationIdx != (int32)RADIO_OFF) {
            m_aListenTimes[stationIdx] += CTimer::m_snTimeInMillisecondsPauseMode - state.m_iTimeInPauseModeInMs;
        }

        if (stationIdx == (int32)RADIO_OFF) {
            m_aRadioState[RADIO_OFF].m_aElapsed[0] = 0;
        } else {
            state.m_aElapsed[0] = (m_ActiveSettings.TrackLengthMs - m_ActiveSettings.PlayTime) - 100;

            switch (m_ActiveSettings.CurrTrackType) {
            case TYPE_INDENT:
            case TYPE_ADVERT:
            case TYPE_DJ_BANTER:
            case TYPE_OUTRO:
                state.m_iTrackPlayTime = m_ActiveSettings.PlayTime;
                state.m_aTrackQueue[0] = m_ActiveSettings.CurrTrackID;
                state.m_aTrackTypes[0] = m_ActiveSettings.CurrTrackType;
                break;
            case TYPE_INTRO: {
                if (stationIdx == (int32)RADIO_TALK) {
                    m_aRadioState[RADIO_TALK].m_aElapsed[1] = s_TalkShowOutroIds[m_ActiveSettings.TrackIndices[0]];
                } else {
                    state.m_aElapsed[1] = 150000;
                }
                m_aRadioState[stationIdx + 6].m_aElapsed[0] = 5000;
                state.m_iTrackPlayTime = m_ActiveSettings.PlayTime;
                if (m_ActiveSettings.TrackQueue[0] == m_ActiveSettings.CurrTrackID) {
                    state.m_aTrackQueue[0] = m_ActiveSettings.TrackQueue[0];
                    state.m_aTrackTypes[0] = m_ActiveSettings.TrackTypes[0];
                    state.m_aTrackQueue[1] = m_ActiveSettings.TrackQueue[1];
                    state.m_aTrackTypes[1] = m_ActiveSettings.TrackTypes[1];
                    state.m_aTrackQueue[2] = m_ActiveSettings.TrackQueue[2];
                    state.m_aTrackTypes[2] = m_ActiveSettings.TrackTypes[2];
                } else {
                    state.m_aTrackQueue[0] = m_ActiveSettings.CurrTrackID;
                    state.m_aTrackTypes[0] = m_ActiveSettings.CurrTrackType;
                    state.m_aTrackQueue[1] = m_ActiveSettings.TrackQueue[0];
                    state.m_aTrackTypes[1] = m_ActiveSettings.TrackTypes[0];
                    state.m_aTrackQueue[2] = m_ActiveSettings.TrackQueue[1];
                    state.m_aTrackTypes[2] = m_ActiveSettings.TrackTypes[1];
                }
                break;
            }
            case TYPE_TRACK:
            case TYPE_USER_TRACK: {
                m_aRadioState[stationIdx + 6].m_aElapsed[0] = 5000;
                state.m_iTrackPlayTime = m_ActiveSettings.PlayTime;
                if (m_ActiveSettings.TrackQueue[0] == m_ActiveSettings.CurrTrackID) {
                    state.m_aTrackQueue[0] = m_ActiveSettings.TrackQueue[0];
                    state.m_aTrackTypes[0] = m_ActiveSettings.TrackTypes[0];
                    state.m_aTrackQueue[1] = m_ActiveSettings.TrackQueue[1];
                    state.m_aTrackTypes[1] = m_ActiveSettings.TrackTypes[1];
                } else {
                    state.m_aTrackQueue[0] = m_ActiveSettings.CurrTrackID;
                    state.m_aTrackTypes[0] = m_ActiveSettings.CurrTrackType;
                    state.m_aTrackQueue[1] = m_ActiveSettings.TrackQueue[0];
                    state.m_aTrackTypes[1] = m_ActiveSettings.TrackTypes[0];
                }
                break;
            }
            }
        }
    }

    m_bInitialised        = false;
    m_bEnabledInPauseMode = false;

    if ((CTimer::m_UserPause || CTimer::m_CodePause) && !duringPause) {
        m_iRadioStationMenuRequest = -1;
        m_nRetuneStartedTime       = 0;
    }

    if (settings == nullptr) {
        if (duringPause) {
            m_nStationsListed    = 0;
            m_nStationsListDown  = 0;
            m_bRetuneJustStarted = false;
        }
    } else {
        m_nStationsListed            = 0;
        m_nStationsListDown          = 0;
        m_iRadioStationScriptRequest = -1;
        m_bDisplayStationName        = false;
        m_bRetuneJustStarted         = false;
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_STOP);
        if (m_ActiveSettings.StationID == RADIO_INVALID) {
            m_ActiveSettings.StationID = RADIO_OFF; // ('\r')
        }
        settings->RadioStation = m_ActiveSettings.StationID;
        settings->BassSetting  = m_ActiveSettings.BassSetting;
        if (   m_nMode != eRadioTrackMode::RADIO_STOPPED
            || m_bInitialised
            || m_nStationsListed != 0
            || m_nStationsListDown != 0) {
            m_nSavedTimeMs         = CTimer::m_snTimeInMilliseconds;
            m_nSavedGameClockDays  = CClock::ms_nGameClockDays;
            m_nSavedGameClockHours = CClock::ms_nGameClockHours;
            m_nSavedRadioStationId = (int32)m_ActiveSettings.StationID;
        }
        if (m_ActiveSettings.StationID == RADIO_EMERGENCY_AA /* 0 */) {
            m_ActiveSettings.StationID = (eRadioID)CAEAudioUtility::GetRandomNumberInRange(1, 13);
        }
    }
}

// 0x4E94C0
int32 CAERadioTrackManager::ChooseIdentIndex(eRadioID id) {
    // (int-radio) тело из фрагмента 0x4E94C0 (redirect-стаб заменён)
    const auto stationIdx = (int32)id;

    const auto& [first, last] = s_IdentsIndexRange[stationIdx];
    if (first == 0x782) { // NOTE: у станции нет идентов
        return -1;
    }

    for (;;) {
        const auto ident = CAEAudioUtility::GetRandomNumberInRange(first, last);

        // Идент 0x44C доступен только после завершения миссии "Are you going to San Fierro?"
        if (id == RADIO_MODERN_HIP_HOP && ident == 0x44C
            && CStats::GetStatValue(STAT_ARE_YOU_GOING_TO_SAN_FIERRO_MISSION_ACCOMPLISHED) == 0.0f) {
            continue;
        }

        const auto historyCheckCount = std::max(0, std::min(last - first - 1, (int32)IDENT_INDEX_HISTORY_COUNT));
        const auto historyEnd = m_nIdentIndexHistory[stationIdx].indices.begin() + historyCheckCount;
        if (std::find(m_nIdentIndexHistory[stationIdx].indices.begin(), historyEnd, ident) == historyEnd) {
            return ident;
        }
    }
}

// 0x4E9570
int32 CAERadioTrackManager::ChooseAdvertIndex(eRadioID id) {
    // (int-radio) тело из фрагмента 0x4E9570 (redirect-стаб заменён)
    const auto stationIdx = (int32)id;

    for (;;) {
        const auto advert = CAEAudioUtility::GetRandomNumberInRange(s_AdvertIdMin, s_AdvertIdMax);

        // Пропускаем рекламы, запрещённые для этой станции (23 записи на станцию)
        if (notsa::contains(s_BannedAdverts[stationIdx], advert)) {
            continue;
        }

        // Пропускаем то, что уже есть в истории рекламы этой станции
        const auto& history = m_nAdvertIndexHistory[stationIdx].indices;
        if (notsa::contains(history, advert)) {
            continue;
        }
        return advert;
    }
}

// 0x4EA270
int8 CAERadioTrackManager::ChooseMusicTrackIndex(eRadioID id) {
    // (int-radio) тело из фрагмента 0x4EA270 (redirect-стаб заменён)
    if (id == RADIO_TALK) {
        return ChooseTalkRadioShow();
    }

    const auto stationIdx = (int32)id;
    const auto numTracks  = s_MusicTracksCount[stationIdx];

    for (;;) {
        const auto track = (int8)CAEAudioUtility::GetRandomNumberInRange(0, numTracks - 1);

        const auto historyCheckCount = std::min(numTracks - 2, (int32)MUSIC_TRACK_HISTORY_COUNT);
        if (historyCheckCount <= 0) {
            return track;
        }
        const auto historyEnd = m_nMusicTrackIndexHistory[stationIdx].indices.begin() + historyCheckCount;
        if (std::find(m_nMusicTrackIndexHistory[stationIdx].indices.begin(), historyEnd, track) == historyEnd) {
            return track;
        }
    }
}

// 0x4EA2D0
int32 CAERadioTrackManager::ChooseDJBanterIndex(eRadioID id) {
    // (int-radio) тело из фрагмента 0x4EA2D0 (redirect-стаб заменён)
    const auto stationIdx = (int32)id;

    const auto PickSpecial = [&](int32 banterIdx) -> int32 {
        if (banterIdx == 0x782) { // У станции нет диджейских бантеров
            return -1;
        }
        if (banterIdx < 0) {
            return -1;
        }
        // Отклоняем, если уже есть в истории бантеров этой станции
        const auto& history = m_nDJBanterIndexHistory[stationIdx].indices;
        if (notsa::contains(history, banterIdx)) {
            return -1;
        }
        return banterIdx;
    };

    int32 banter = -1;
    if (m_nSpecialDJBanterPending == 0) {
        banter = s_SpecialBanter0[stationIdx][0];
    check_banter:
        banter = PickSpecial(banter);
        if (banter != -1) {
            return banter;
        }
    } else if (m_nSpecialDJBanterPending == 1) {
        if (m_nSpecialDJBanterIndex == 0
            || (m_nSpecialDJBanterIndex == 1 && s_SpecialBanter1[stationIdx][0] != s_SpecialBanter1[stationIdx][1])) {
            banter = s_SpecialBanter1[stationIdx][m_nSpecialDJBanterIndex][0]; // запись - пара {first, last}
            goto check_banter;
        }
    } else if (m_nSpecialDJBanterPending == 2) {
        banter = s_SpecialBanter2[stationIdx][m_nSpecialDJBanterIndex];
        goto check_banter;
    }
    // `m_nSpecialDJBanterPending == 3` проваливается дальше с `banter == -1`

    if (stationIdx == RADIO_EMERGENCY_AA) {
        if (CGameLogic::LaRiotsActiveHere()) {
            return ChooseDJBanterIndexFromList(RADIO_EMERGENCY_AA, reinterpret_cast<int32**>(s_BanterList_Riot.data()));
        }
        return ChooseDJBanterIndexFromList(RADIO_EMERGENCY_AA, reinterpret_cast<int32**>(s_DJBanterIndexRange.data()));
    }

    if (!CAEAudioUtility::ResolveProbability(0.6f)) {
        return banter;
    }
    if ((int32)CGame::currArea != 0) { // Не в обычном мире (интерьер)
        return banter;
    }

    const auto hour = CClock::ms_nGameClockHours;

    if (CWeather::ForecastWeather((eWeatherType)0x10, 3)
        || CWeather::ForecastWeather((eWeatherType)0x8, 3)) { // Дождливая погода - ветка House-бантеров
        if (CAEAudioUtility::ResolveProbability(0.5f)) {
            if (const auto idx = ChooseDJBanterIndexFromList(id, reinterpret_cast<int32**>(s_BanterList_House.data())); idx != -1) {
                return idx;
            }
        }
    } else if (CWeather::ForecastWeather((eWeatherType)0x0, 3)
        || CWeather::ForecastWeather((eWeatherType)0x2, 3)
        || CWeather::ForecastWeather((eWeatherType)0xD, 3)
        || CWeather::ForecastWeather((eWeatherType)0x6, 3)
        || CWeather::ForecastWeather((eWeatherType)0xB, 3)
        || CWeather::ForecastWeather((eWeatherType)0x11, 3)) {
        if (hour < 8 || 5 < hour) { // NOTE: дословно - условие истинно всегда, ветка Drum&Bass недостижима в оригинале
            if (CWeather::ForecastWeather((eWeatherType)0x9, 3) && CAEAudioUtility::ResolveProbability(0.5f)) {
                if (const auto idx = ChooseDJBanterIndexFromList(id, reinterpret_cast<int32**>(s_BanterList_Reggae.data())); idx != -1) {
                    return idx;
                }
            }
        } else {
            if (CAEAudioUtility::ResolveProbability(0.5f)) {
                if (const auto idx = ChooseDJBanterIndexFromList(id, reinterpret_cast<int32**>(s_BanterList_DrumBass.data())); idx != -1) {
                    return idx;
                }
            }
        }
    }

    const auto PickOrFallback = [&](auto& list) -> int32 {
        if (const auto idx = ChooseDJBanterIndexFromList(id, reinterpret_cast<int32**>(list.data())); idx != -1) {
            return idx;
        }
        return ChooseDJBanterIndexFromList(id, reinterpret_cast<int32**>(s_DJBanterIndexRange.data()));
    };

    if (hour < 6 || 8 < hour) {
        if (hour < 0x12 || 0x14 < hour) {
            if ((hour < 0x16 && 2 < hour) || !CAEAudioUtility::ResolveProbability(0.3f)) {
                return ChooseDJBanterIndexFromList(id, reinterpret_cast<int32**>(s_DJBanterIndexRange.data())); // fallback
            }
            return PickOrFallback(s_BanterList_Riot);   // бантры бунта (22:00-02:00)
        } else {
            if (!CAEAudioUtility::ResolveProbability(0.3f)) {
                return ChooseDJBanterIndexFromList(id, reinterpret_cast<int32**>(s_DJBanterIndexRange.data())); // fallback
            }
            return PickOrFallback(s_BanterList_Desert); // бантры пустыни (18:00-20:00)
        }
    } else {
        if (!CAEAudioUtility::ResolveProbability(0.3f)) {
            return ChooseDJBanterIndexFromList(id, reinterpret_cast<int32**>(s_DJBanterIndexRange.data())); // fallback
        }
        return PickOrFallback(s_BanterList_Country);    // бантры сельской местности (06:00-08:00)
    }
}

// 0x4E95E0
int32 CAERadioTrackManager::ChooseDJBanterIndexFromList(eRadioID id, int32** list) {
    // (int-radio) тело из фрагмента 0x4E95E0 (redirect-стаб заменён). Сигнатура оставлена как в хедере репо (int32** list);
    // рекомендация фрагмента: сменить объявление в AERadioTrackManager.h на `(eRadioID id, const std::array<int32, 2>* banterList)` - см. notes_int-radio.md
    const auto banterList = reinterpret_cast<const std::array<int32, 2>*>(list);

    const auto stationIdx = (int32)id;

    const auto first = banterList[stationIdx][0];
    if (first == 0x782) { // NOTE: у станции нет диджейских бантеров
        return -1;
    }

    const auto last  = banterList[stationIdx][1];
    const auto count = last - first + 1;
    if (count < 1) {
        return -1;
    }
    const auto randomOffset = CAEAudioUtility::GetRandomNumberInRange(0, last - first);

    const auto& scanRange = s_DJBanterIndexRange[stationIdx];
    const auto historyCheckCount = std::max(0, std::min(scanRange[1] - scanRange[0] - 1, (int32)DJBANTER_INDEX_HISTORY_COUNT));
    const auto& history = m_nDJBanterIndexHistory[stationIdx].indices;

    for (auto attempt = 0; attempt < count; attempt++) {
        const auto candidate = (attempt + randomOffset) % count + first;
        const auto historyEnd = history.begin() + historyCheckCount;
        if (std::find(history.begin(), historyEnd, candidate) == historyEnd) {
            return candidate;
        }
    }
    return -1;
}

// 0x4EB180
void CAERadioTrackManager::ChooseTracksForStation(eRadioID id) {
    int8 trackCount = 0;

    for (auto i = 0u; i < tRadioSettings::NUM_TRACKS; i++) {
        m_RequestedSettings.TrackTypes[i] = TYPE_NONE;
        m_RequestedSettings.TrackQueue[i] = -1;
        m_RequestedSettings.TrackIndices[i] = -1;
    }

    if (!CAEAudioUtility::ResolveProbability(0.95f)) {
        if (id) {
            if (CAEAudioUtility::ResolveProbability(0.5f))
                QueueUpTracksForStation(id, &trackCount, TYPE_INDENT, m_RequestedSettings);

            if (!QueueUpTracksForStation(id, &trackCount, TYPE_DJ_BANTER, m_RequestedSettings))
                QueueUpTracksForStation(id, &trackCount, TYPE_ADVERT, m_RequestedSettings);

            if (id == RADIO_USER_TRACKS) {
                QueueUpTracksForStation(RADIO_USER_TRACKS, &trackCount, TYPE_TRACK, m_RequestedSettings);
                return;
            }
        } else {
            QueueUpTracksForStation(RADIO_EMERGENCY_AA, &trackCount, TYPE_DJ_BANTER, m_RequestedSettings);
        }
        QueueUpTracksForStation(id, &trackCount, TYPE_INTRO, m_RequestedSettings);
        return;
    }

    if (id == RADIO_USER_TRACKS) {
        QueueUpTracksForStation(RADIO_USER_TRACKS, &trackCount, TYPE_TRACK, m_RequestedSettings);
        QueueUpTracksForStation(RADIO_USER_TRACKS, &trackCount, TYPE_TRACK, m_RequestedSettings);
        if (!FrontEndMenuManager.m_RadioMode && CAEAudioUtility::ResolveProbability(0.17f)) {
            QueueUpTracksForStation(RADIO_USER_TRACKS, &trackCount, TYPE_ADVERT, m_RequestedSettings);
        }
        return;
    }

    if (CAEAudioUtility::ResolveProbability(0.9f)) {
        QueueUpTracksForStation(id, &trackCount, TYPE_TRACK, m_RequestedSettings);
        return;
    }

    if (CAEAudioUtility::ResolveProbability(0.5f)) {
        if (CAEAudioUtility::ResolveProbability(0.5f)) {
            QueueUpTracksForStation(id, &trackCount, TYPE_INDENT, m_RequestedSettings);
        }
        QueueUpTracksForStation(id, &trackCount, TYPE_INTRO, m_RequestedSettings);
        return;
    }

    QueueUpTracksForStation(id, &trackCount, TYPE_OUTRO, m_RequestedSettings);
    AddMusicTrackIndexToHistory(id, m_RequestedSettings.TrackIndices[trackCount - 1]);

    if (id == RADIO_EMERGENCY_AA) {
        QueueUpTracksForStation(id, &trackCount, TYPE_DJ_BANTER, m_RequestedSettings);
        return;
    }

    if (CAEAudioUtility::ResolveProbability(0.5f)) {
        if (CAEAudioUtility::ResolveProbability(0.5f)) {
            QueueUpTracksForStation(id, &trackCount, TYPE_INDENT, m_RequestedSettings);
        }
        QueueUpTracksForStation(id, &trackCount, TYPE_INTRO, m_RequestedSettings);
        return;
    }

    if (CAEAudioUtility::ResolveProbability(0.5f))
        QueueUpTracksForStation(id, &trackCount, TYPE_INDENT, m_RequestedSettings);

    if (!QueueUpTracksForStation(id, &trackCount, TYPE_DJ_BANTER, m_RequestedSettings))
        QueueUpTracksForStation(id, &trackCount, TYPE_ADVERT, m_RequestedSettings);
}

// 0x4E8E40
int8 CAERadioTrackManager::ChooseTalkRadioShow() {
    // (int-radio) тело из фрагмента 0x4E8E40 (redirect-стаб заменён)
    auto& recentlyPlayedShows = m_nMusicTrackIndexHistory[RADIO_TALK].indices; // aliases 0xB62C1C

    const auto Stat = [](eStats stat) { return CStats::GetStatValue(stat); };
    constexpr auto STAT_ZERO = 0.0f;

    std::array<int8, 32> shows;
    shows.fill(-1);
    int8 count = 0;

    // NOTE: проверки статов/миссий ниже выбирают доступные шоу в зависимости
    // от прогресса игрока по сюжетным аркам.

    if (Stat((eStats)0x136) != STAT_ZERO && Stat((eStats)0x137) == STAT_ZERO) {
        shows[count++] = 0x0E;
    } else if (Stat((eStats)0x138) != STAT_ZERO) {
        shows[count++] = 0x0F;
    }

    if (Stat((eStats)0x139) == STAT_ZERO) {
        shows[count++] = 0x0C;
    } else if (Stat((eStats)0x138) == STAT_ZERO) {
        shows[count++] = 0x0D;
    }

    if (Stat((eStats)0x13B) != STAT_ZERO && Stat((eStats)0x13C) == STAT_ZERO) {
        shows[count++] = 0x06;
    }

    if (Stat((eStats)0x12E) != STAT_ZERO && Stat((eStats)0x13A) == STAT_ZERO) {
        if (Stat((eStats)0x13C) != STAT_ZERO && Stat((eStats)0x13D) == STAT_ZERO) {
            shows[count++] = 0x04;
        } else if (Stat((eStats)0x13D) != STAT_ZERO) {
            shows[count++] = 0x05;
        }
    } else {
        shows[count++] = 0x03;
    }

    if (Stat((eStats)0x13E) == STAT_ZERO) {
        shows[count++] = 0x07;
    } else {
        shows[count++] = 0x08;
    }

    if (Stat((eStats)0x13F) == STAT_ZERO) {
        shows[count++] = 0x09;
    } else if (Stat((eStats)0x130) == STAT_ZERO && Stat((eStats)0x140) == STAT_ZERO) {
        shows[count++] = 0x0A;
    } else if (Stat((eStats)0x140) != STAT_ZERO) {
        shows[count++] = 0x0B;
    }

    if (Stat((eStats)0x141) == STAT_ZERO) {
        shows[count++] = 0x1B;
    } else {
        shows[count++] = 0x1C;
    }

    if (Stat((eStats)0x142) == STAT_ZERO) {
        shows[count++] = 0x1D;
    } else {
        shows[count++] = 0x1E;
    }

    if (Stat((eStats)0x143) == STAT_ZERO) {
        shows[count++] = 0x00;
    } else if (Stat((eStats)0x144) != STAT_ZERO && Stat((eStats)0x145) == STAT_ZERO) {
        shows[count++] = 0x01;
    } else if (Stat((eStats)0x146) != STAT_ZERO) {
        shows[count++] = 0x02;
    }

    // Сюжетная цепочка (вложенные else-if в оригинале)
    if (Stat((eStats)0x12E) == STAT_ZERO) {
        shows[count++] = 0x10;
    } else if (Stat((eStats)0x12F) == STAT_ZERO) {
        shows[count++] = 0x11;
    } else if (Stat((eStats)0x143) == STAT_ZERO) {
        shows[count++] = 0x12;
    } else if (Stat((eStats)0x130) == STAT_ZERO) {
        shows[count++] = 0x13;
    } else if (Stat((eStats)0x131) == STAT_ZERO) {
        shows[count++] = 0x14;
    } else if (Stat((eStats)0x132) == STAT_ZERO) {
        shows[count++] = 0x15;
    } else if (Stat((eStats)0x133) == STAT_ZERO) {
        shows[count++] = 0x16;
    } else if (Stat((eStats)0x134) == STAT_ZERO) {
        shows[count++] = 0x17;
    } else if (Stat((eStats)0x135) == STAT_ZERO) {
        shows[count++] = 0x18;
    } else if (Stat((eStats)0xB5) == 4.0f /* 0x858B90 */) {
        shows[count++] = 0x1A;
    } else {
        shows[count++] = 0x19;
    }

    if (count < 1) {
        return -1;
    }

    // Выбираем случайное шоу, повторяя выбор, пока оно есть в истории "недавно игравших"
    for (;;) {
        const auto pick = CAEAudioUtility::GetRandomNumberInRange(0, count - 1);
        if (count < 2) {
            return shows[pick];
        }
        auto bInHistory = false;
        for (int8 i = 0; i < count - 1; i++) { // NOTE: оригинал сканирует только `count - 1` записей истории
            if (shows[pick] == recentlyPlayedShows[i]) {
                bInHistory = true;
                break;
            }
        }
        if (!bInHistory) {
            return shows[pick];
        }
    }
}

// 0x4E96C0
void CAERadioTrackManager::AddMusicTrackIndexToHistory(eRadioID id, int8 trackIndex) {
    if (trackIndex >= 0 && m_nMusicTrackIndexHistory[id].indices[0] != trackIndex) {
        m_nMusicTrackIndexHistory[id].PutAtFirst(trackIndex);
        m_nTracksInARow[id]++;
    }
}

// 0x4E9720
void CAERadioTrackManager::AddIdentIndexToHistory(eRadioID id, int8 trackIndex) {
    if (m_nIdentIndexHistory[id].indices[0] != trackIndex)
        m_nIdentIndexHistory[id].PutAtFirst(trackIndex);
}

// 0x4E9760
void CAERadioTrackManager::AddAdvertIndexToHistory(eRadioID id, int8 trackIndex) {
    if (m_nAdvertIndexHistory[id].indices[0] != trackIndex) {
        m_nAdvertIndexHistory[id].PutAtFirst(trackIndex);
        m_nTracksInARow[id] = 0;
    }
}

// 0x4E97B0
void CAERadioTrackManager::AddDJBanterIndexToHistory(eRadioID id, int8 trackIndex) {
    if (m_nDJBanterIndexHistory[id].indices[0] != trackIndex) {
        m_nDJBanterIndexHistory[id].PutAtFirst(trackIndex);
        m_nTracksInARow[id] = 0;
    }
}

// 0x4EA590
void CAERadioTrackManager::CheckForPause() {
    if (CTimer::GetIsPaused()) {
        m_bPauseMode = true;
        AEAudioHardware.SetChannelFrequencyScalingFactor(m_HwClientHandle, 0, m_bEnabledInPauseMode ? 1.0f : 0.0f);
    } else {
        const auto* settings = CAEVehicleAudioEntity::StaticGetPlayerVehicleAudioSettingsForRadio();
        
        if (settings && notsa::contains({
                AE_RT_CIVILIAN,
                AE_RT_EMERGENCY,
                AE_RT_UNKNOWN
            }, settings->RadioType)
            || CAudioEngine::IsAmbienceRadioActive()
        ) {
            m_bPauseMode = false;
            AEAudioHardware.SetChannelFrequencyScalingFactor(m_HwClientHandle, 0, 1.0f);
        } else {
            StopRadio(nullptr, false);
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_STOP);
            m_bPauseMode = false;
        }
    }
}

// 0x4EB9A0
void CAERadioTrackManager::Service(int32 playTime) {
    // (int-radio) тело из фрагмента 0x4EB9A0 (redirect-стаб заменён)
    s_PrevPlayTime                 = m_ActiveSettings.PlayTime;
    m_ActiveSettings.PlayTime      = playTime;
    m_ActiveSettings.TrackLengthMs = AEAudioHardware.GetTrackLengthMs();
    m_ActiveSettings.CurrTrackID   = AEAudioHardware.GetPlayingTrackID();

    if (!CTimer::m_UserPause && !CTimer::m_CodePause) {
        CheckForMissionStatsChanges();
        CheckForStationRetune();
    } else {
        CheckForStationRetuneDuringPause();
    }

    // Активируем запрошенную станцию
    if (m_bInitialised && m_nMode == eRadioTrackMode::RADIO_STOPPED) {
        if (m_RequestedSettings.StationID == RADIO_OFF /* 13 */) {
            m_ActiveSettings = m_RequestedSettings;
            if (const auto* settings = CAEVehicleAudioEntity::StaticGetPlayerVehicleAudioSettingsForRadio();
                settings && (   settings->RadioType == AE_RT_CIVILIAN
                             || settings->RadioType == AE_RT_EMERGENCY
                             || settings->RadioType == AE_RT_UNKNOWN)) {
                m_bDisplayStationName = true;
            }
        } else {
            if (CAudioEngine::IsAmbienceTrackActive()) {
                goto stateMachine; // NOTE: m_bInitialised сохраняется
            }

            m_ActiveSettings = m_RequestedSettings;
            m_nMode = eRadioTrackMode::RADIO_STARTING;
            if (IsVehicleRadioActive()) {
                m_bDisplayStationName = true;
            }
        }
        m_bInitialised = false;
    }

stateMachine:
    switch (m_nMode) {
    case eRadioTrackMode::RADIO_STARTING: {
        if (m_ActiveSettings.PlayTime < 0) {
            m_ActiveSettings.PlayTime = 0;
        }
        AEAudioHardware.SetBassSetting(
            m_bBassEnhance ? m_ActiveSettings.BassSetting : (eBassSetting)0,
            m_ActiveSettings.BassGain
        );
        AEAudioHardware.PlayTrack(
            m_ActiveSettings.TrackQueue[0],
            m_ActiveSettings.TrackQueue[1],
            m_ActiveSettings.PlayTime,
            (uint8)m_ActiveSettings.TrackFlags,
            m_ActiveSettings.TrackTypes[0] == TYPE_USER_TRACK,
            m_ActiveSettings.TrackTypes[1] == TYPE_USER_TRACK
        );
        m_nMode = eRadioTrackMode::RADIO_WAITING_TO_PLAY;
        m_nStationsListed   = 0;
        m_nStationsListDown = 0;
        return;
    }
    case eRadioTrackMode::RADIO_WAITING_TO_PLAY: {
        if (m_ActiveSettings.PlayTime == -2) {
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_STOP);
            StartTrackPlayback();
            field_60 = CTimer::m_snTimeInMilliseconds;
            m_aRadioState[m_ActiveSettings.StationID].m_iTimeInPauseModeInMs = CTimer::m_snTimeInMillisecondsPauseMode;
            m_nMode = eRadioTrackMode::RADIO_PLAYING;
            m_nStationsListed   = 0;
            m_nStationsListDown = 0;
            return;
        }
        if (m_ActiveSettings.PlayTime == -8) {
            if (   AEAudioHardware.GetPlayingTrackID() == m_ActiveSettings.TrackQueue[1]
                || (AEAudioHardware.GetPlayingTrackID() == m_ActiveSettings.TrackQueue[0] && m_ActiveSettings.TrackQueue[1] == -1)) {
                m_ActiveSettings.TrackQueue[0]   = ChooseAdvertIndex(RADIO_USER_TRACKS);
                m_ActiveSettings.TrackTypes[0]   = TYPE_ADVERT;
                m_ActiveSettings.TrackQueue[1]   = AEUserRadioTrackManager.SelectUserTrackIndex();
                m_ActiveSettings.TrackTypes[1]   = TYPE_USER_TRACK;
                m_ActiveSettings.TrackIndices[1] = (int8)m_ActiveSettings.TrackQueue[1];
                m_nMode = eRadioTrackMode::RADIO_STARTING;
                m_nStationsListed   = 0;
                m_nStationsListDown = 0;
                return;
            }
        }
        break;
    }
    case eRadioTrackMode::RADIO_PLAYING: {
        if (m_ActiveSettings.StationID == RADIO_USER_TRACKS && m_ActiveSettings.PlayTime == -6) {
            if (   AEAudioHardware.GetActiveTrackID() == m_ActiveSettings.TrackQueue[0]
                && m_ActiveSettings.TrackQueue[1] != -1) {
                m_ActiveSettings.TrackQueue[0]   = m_ActiveSettings.TrackQueue[1];
                m_ActiveSettings.TrackIndices[0] = m_ActiveSettings.TrackIndices[1];
                m_ActiveSettings.TrackTypes[0]   = m_ActiveSettings.TrackTypes[1];
                m_ActiveSettings.TrackQueue[1]   = AEUserRadioTrackManager.SelectUserTrackIndex();
                m_ActiveSettings.TrackTypes[1]   = TYPE_USER_TRACK;
                m_ActiveSettings.TrackIndices[1] = (int8)m_ActiveSettings.TrackQueue[1];
            } else {
                m_ActiveSettings.TrackQueue[0]   = AEUserRadioTrackManager.SelectUserTrackIndex();
                m_ActiveSettings.TrackTypes[0]   = TYPE_USER_TRACK;
                m_ActiveSettings.TrackIndices[0] = (int8)m_ActiveSettings.TrackQueue[0];
                m_ActiveSettings.TrackQueue[1]   = AEUserRadioTrackManager.SelectUserTrackIndex();
                m_ActiveSettings.TrackTypes[1]   = TYPE_USER_TRACK;
                m_ActiveSettings.TrackIndices[1] = (int8)m_ActiveSettings.TrackQueue[1];
            }
            m_nMode = eRadioTrackMode::RADIO_STARTING;
            m_nStationsListed   = 0;
            m_nStationsListDown = 0;
        }

        // Учёт истории закончившихся треков
        if (AEAudioHardware.GetPlayingTrackID() == m_ActiveSettings.TrackQueue[0]) {
            switch (m_ActiveSettings.TrackTypes[0]) {
            case TYPE_INTRO:
            case TYPE_TRACK:
            case TYPE_OUTRO:
            case TYPE_USER_TRACK:
                AddMusicTrackIndexToHistory(m_ActiveSettings.StationID, m_ActiveSettings.TrackIndices[0]);
                break;
            case TYPE_INDENT:    AddIdentIndexToHistory(m_ActiveSettings.StationID, (int8)m_ActiveSettings.TrackQueue[0]);    break;
            case TYPE_ADVERT:    AddAdvertIndexToHistory(m_ActiveSettings.StationID, (int8)m_ActiveSettings.TrackQueue[0]);   break;
            case TYPE_DJ_BANTER: AddDJBanterIndexToHistory(m_ActiveSettings.StationID, (int8)m_ActiveSettings.TrackQueue[0]); break;
            }
            m_ActiveSettings.CurrTrackType = m_ActiveSettings.TrackTypes[0];
            m_ActiveSettings.CurrTrackIdx  = m_ActiveSettings.TrackIndices[0];
        } else if (AEAudioHardware.GetPlayingTrackID() == m_ActiveSettings.PrevTrackID) {
            switch (m_ActiveSettings.PrevTrackType) {
            case TYPE_INTRO:
            case TYPE_TRACK:
            case TYPE_OUTRO:
            case TYPE_USER_TRACK:
                AddMusicTrackIndexToHistory(m_ActiveSettings.StationID, m_ActiveSettings.PrevTrackIdx);
                break;
            case TYPE_INDENT:    AddIdentIndexToHistory(m_ActiveSettings.StationID, (int8)m_ActiveSettings.PrevTrackID);    break;
            case TYPE_ADVERT:    AddAdvertIndexToHistory(m_ActiveSettings.StationID, (int8)m_ActiveSettings.PrevTrackID);   break;
            case TYPE_DJ_BANTER: AddDJBanterIndexToHistory(m_ActiveSettings.StationID, (int8)m_ActiveSettings.PrevTrackID); break;
            }
            m_ActiveSettings.CurrTrackType = m_ActiveSettings.PrevTrackType;
            m_ActiveSettings.CurrTrackIdx  = m_ActiveSettings.PrevTrackIdx;
        }

        // User tracks: перезапуск радио при скипе трека
        if (m_ActiveSettings.StationID == RADIO_USER_TRACKS
            && (m_ActiveSettings.TrackTypes[0] == TYPE_USER_TRACK || AEUserRadioTrackManager.GetUserTrackPlayMode() == 0)) {
            if (CPad::GetPad(0)->IsRadioTrackSkipPressed()) {
                StopRadio(nullptr, true);
                while (   m_nMode != eRadioTrackMode::RADIO_STOPPED
                       || m_bInitialised
                       || m_nStationsListed != 0
                       || m_nStationsListDown != 0) {
                    Service(AEAudioHardware.GetTrackPlayTime());
                    AEAudioHardware.Service();
                }
                StartRadio(m_ActiveSettings.StationID, m_ActiveSettings.BassSetting, m_ActiveSettings.BassGain, true);
            }
        }

        CheckForPause();
        UpdateRadioVolumes();
        CheckForTrackConcatenation();
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_STOP);
        return;
    }
    case eRadioTrackMode::RADIO_STOPPING:
    case eRadioTrackMode::RADIO_STOPPING_CHANNELS_STOPPED:
        AEAudioHardware.StopTrack();
        m_nMode = eRadioTrackMode::RADIO_WAITING_TO_STOP;
        m_nStationsListed   = 0;
        m_nStationsListDown = 0;
        return;
    case eRadioTrackMode::RADIO_STOPPING_SILENCED:
        m_nMode = eRadioTrackMode::RADIO_STOPPING_CHANNELS_STOPPED;
        m_nStationsListed   = 0;
        m_nStationsListDown = 0;
        return;
    case eRadioTrackMode::RADIO_WAITING_TO_STOP:
        if (m_ActiveSettings.PlayTime == -6 || m_ActiveSettings.PlayTime == -8) {
            m_nMode = eRadioTrackMode::RADIO_STOPPED;
            m_nStationsListed   = 0;
            m_nStationsListDown = 0;
        } else if (m_ActiveSettings.PlayTime == -7 || m_ActiveSettings.PlayTime == -2) {
            AEAudioHardware.StopTrack();
        }
        return;
    }
}

// 0x5D40E0
void CAERadioTrackManager::Load() {
    for (auto r = 0; r < RADIO_COUNT; r++) {
        for (auto& historyIndex : m_nMusicTrackIndexHistory[r].indices) {
            CGenericGameStorage::LoadDataFromWorkBuffer(historyIndex);
        }

        for (auto& identIndex : m_nIdentIndexHistory[r].indices) {
            CGenericGameStorage::LoadDataFromWorkBuffer(identIndex);
        }

        for (auto& advertIndex : m_nAdvertIndexHistory[r].indices) {
            CGenericGameStorage::LoadDataFromWorkBuffer(advertIndex);
        }

        for (auto& banterIndex : m_nDJBanterIndexHistory[r].indices) {
            CGenericGameStorage::LoadDataFromWorkBuffer(banterIndex);
        }
    }

    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsCitiesPassed);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedCasino3);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedCasino6);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedCasino10);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedCat1);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedDesert1);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedDesert3);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedDesert5);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedDesert8);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedDesert10);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedFarlie3);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedLAFin2);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedMansion2);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedRyder2);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedRiot1);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedSCrash1);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedStrap4);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedSweet2);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedTruth2);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedVCrash2);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsStartedBadlands);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsStartedCat2);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsStartedCrash1);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsLastHitGameClockDays);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsLastHitGameClockHours);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsLastHitTimeOutHours);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nSpecialDJBanterPending);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nSpecialDJBanterIndex);
}

// 0x5D3EE0
void CAERadioTrackManager::Save() {
    for (auto r = 0; r < RADIO_COUNT; r++) {
        for (auto& historyIndex : m_nMusicTrackIndexHistory[r].indices) {
            CGenericGameStorage::SaveDataToWorkBuffer(historyIndex);
        }

        for (auto& identIndex : m_nIdentIndexHistory[r].indices) {
            CGenericGameStorage::SaveDataToWorkBuffer(identIndex);
        }

        for (auto& advertIndex : m_nAdvertIndexHistory[r].indices) {
            CGenericGameStorage::SaveDataToWorkBuffer(advertIndex);
        }

        for (auto& banterIndex : m_nDJBanterIndexHistory[r].indices) {
            CGenericGameStorage::SaveDataToWorkBuffer(banterIndex);
        }
    }

    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsCitiesPassed);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedCasino3);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedCasino6);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedCasino10);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedCat1);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedDesert1);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedDesert3);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedDesert5);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedDesert8);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedDesert10);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedFarlie3);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedLAFin2);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedMansion2);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedRyder2);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedRiot1);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedSCrash1);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedStrap4);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedSweet2);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedTruth2);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedVCrash2);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsStartedBadlands);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsStartedCat2);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsStartedCrash1);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsLastHitGameClockDays);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsLastHitGameClockHours);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsLastHitTimeOutHours);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nSpecialDJBanterPending);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nSpecialDJBanterIndex);
}
