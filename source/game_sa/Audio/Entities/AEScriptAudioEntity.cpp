#include "StdInc.h"

#include "AEScriptAudioEntity.h"
#include "AESoundManager.h"
#include "AEAudioUtility.h"
#include "AEAudioHardware.h"
#include "AEAmbienceTrackManager.h"
#include "AudioEngine.h"
#include "Timer.h"
#include "AEVehicleAudioEntity.h"
#include "Localisation.h"
#include "Ped.h"
#include "AEPedAudioEntity.h"
#include "Enums/eWeaponType.h"

// 0x5074D0
CAEScriptAudioEntity::CAEScriptAudioEntity() : CAEAudioEntity() {
    m_nLastTimeHornPlayed = 0;
    field_7E = 0;
    field_7C = 0;
    m_Volume = 0.0f;
    m_Speed = 1.0f;
    field_7D = 0;
    field_8C = 0.0f;
}

// 0x5B9B60
void CAEScriptAudioEntity::Initialise() {
    for (auto& link : wavLinks) {
        link.Init();
    }
}

// 0x4EC150
void CAEScriptAudioEntity::Reset() {
    for (auto i = 0; i < MISSION_AUDIO_COUNT; i++) {
        ClearMissionAudio(i);
    }
    field_7C = 0;
    m_Entity = nullptr;
    field_7D = 0;
    field_8C = 2.0f;
}

// 0x0
void CAEScriptAudioEntity::AddAudioEvent(int32) {
    /* Android NOP */
}

// 0x4EC100
void CAEScriptAudioEntity::AttachMissionAudioToPhysical(uint8 sampleId, CPhysical* physical) {
    // NOTE: No bounds check on `sampleId` in the original.
    auto& link       = wavLinks[sampleId];
    link.m_pEntity   = physical;
    link.m_vPosition = CVector(-1000.0f, -1000.0f, -1000.0f);
}
// 0x4EC040
void CAEScriptAudioEntity::ClearMissionAudio(uint8 sampleId) {
    if (sampleId >= 4) {
        return;
    }
    AESoundManager.CancelSoundsInBankSlot((eSoundBankSlot)(sampleId + 0x1A), true);
    auto* block = (float*)((char*)this + sampleId * 0x20 + 0x90);
    block[3] = 0.0f;    // +0x9C
    block[0] = -1000.0f; // +0x90
    *(uint32*)((char*)this + (sampleId + 5) * 0x20) = 0; // second interleaved array slot
    block[1] = -1000.0f; // +0x94
    block[2] = -1000.0f; // +0x98
}

// 0x4EBFE0
bool CAEScriptAudioEntity::IsMissionAudioSampleFinished(uint8 sampleId) {
    if (sampleId >= MISSION_AUDIO_COUNT) {
        return true;
    }

    // Slots 2/3 have no tracked sound object, query the bank slot instead
    if (sampleId > 1) {
        return !AESoundManager.AreSoundsPlayingInBankSlot((int16)(SND_BANK_SLOT_MISSION1 + sampleId));
    }
    return wavLinks[sampleId].m_Sound == nullptr;
}
// 0x4EBF60
int8 CAEScriptAudioEntity::GetMissionAudioLoadingStatus(uint8 sampleId) {
    if (sampleId < MISSION_AUDIO_COUNT) {
        auto& link = wavLinks[sampleId];
        if (link.m_nBankId >= 0) {
            if (link.m_nBankSlotId >= 0) {
                return AEAudioHardware.GetSoundLoadingStatus(
                    (eSoundBank)link.m_nBankId,
                    (eSoundID)link.m_nBankSlotId,
                    (eSoundBankSlot)(SND_BANK_SLOT_MISSION1 + sampleId)
                );
            }
            return AEAudioHardware.GetSoundBankLoadingStatus(
                (eSoundBank)link.m_nBankId,
                (eSoundBankSlot)(SND_BANK_SLOT_MISSION1 + sampleId)
            );
        }
    }
    return 1;
}
// 0x4EC020
int32 CAEScriptAudioEntity::GetMissionAudioEvent(uint8 sampleId) {
    return wavLinks[sampleId].m_nAudioEvent;
}

// 0x4EC0C0
void CAEScriptAudioEntity::SetMissionAudioPosition(uint8 sampleId, CVector& posn) {
    // NOTE: No bounds check on `sampleId` in the original.
    auto& link       = wavLinks[sampleId];
    link.m_vPosition = posn;
    link.m_pEntity   = nullptr;
}
// 0x4EC4D0
CVector* CAEScriptAudioEntity::GetMissionAudioPosition(uint8 sampleId) {
    // NOTE: No bounds check on `sampleId` in the original.
    auto& link = wavLinks[sampleId];
    if (link.m_pEntity) {
        return &link.m_pEntity->GetPosition();
    }
    // Position not set (still the -1000 sentinel, or exactly zero) => no position
    if (link.m_vPosition == CVector(-1000.0f, -1000.0f, -1000.0f) || link.m_vPosition.IsZero()) {
        return nullptr;
    }
    return &link.m_vPosition;
}
// 0x4EC6D0
// `eventId` - the mission audio event id (used for the default volume and the "already playing" check),
// `sfxId`   - sound within the bank, `linkId` - mission audio slot (only 2 or 3)
void CAEScriptAudioEntity::PlayMissionBankSound(eAudioEvents eventId, CVector& posn, CPhysical* physical, int16 sfxId, uint8 linkId, uint8 a7, float volume, float maxDistance, float speed) {
    // Mission bank sounds only ever use slots 2/3
    if (linkId < 2 || linkId > 3) {
        return;
    }
    if (a7 != 0 && AESoundManager.AreSoundsOfThisEventPlayingForThisEntity((int16)eventId, this)) {
        return;
    }

    const auto bankSlot = (eSoundBankSlot)(SND_BANK_SLOT_MISSION1 + linkId);
    if (!AEAudioHardware.IsSoundBankLoaded((eSoundBank)wavLinks[linkId].m_nBankId, bankSlot)) {
        return;
    }

    // Original: m_tempSound.Initialise(bankSlot, sfxId, this, pos, volume, maxDistance, speed, 1.0f),
    // m_tempSound.m_Flags = 6 (SOUND_IS_CANCELLABLE|SOUND_REQUEST_UPDATES),
    // SetFlags(SOUND_FRONT_END, bFrontEnd), and for a physical:
    // SetFlags(SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY) + RegisterWithPhysicalEntity(physical),
    // then RequestNewSound(&m_tempSound).
    const auto Play = [&](CVector pos, bool bFrontEnd) {
        AESoundManager.PlaySound({
            .BankSlotID         = bankSlot,
            .SoundID            = (eSoundID)sfxId,
            .AudioEntity        = this,
            .Pos                = pos,
            .Volume             = GetDefaultVolume(eventId) + volume,
            .RollOffFactor      = maxDistance,
            .Speed              = speed,
            .Doppler            = 1.0f,
            .Flags              = SOUND_IS_CANCELLABLE | SOUND_REQUEST_UPDATES
                                 | (bFrontEnd ? SOUND_FRONT_END : 0u)
                                 | (physical ? SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY : 0u),
            .FrequencyVariance  = 0.0f,
            .RegisterWithEntity = physical,
            .EventID            = eventId,
        });
    };

    if (physical) {
        Play(physical->GetPosition(), false);
    } else if (posn == CVector(-1000.0f, -1000.0f, -1000.0f) || posn.IsZero()) {
        Play(CVector(0.0f, 1.0f, 0.0f), true);
    } else if (posn == CVector(-1.0f, 0.0f, 0.0f) || posn == CVector(1.0f, 0.0f, 0.0f)) { // Stereo pair events (see AE_SCRIPT_AIR_HORN)
        Play(posn, true);
    } else {
        Play(posn, false);
    }
}
// 0x4EC550
void CAEScriptAudioEntity::PlayResidentSoundEvent(eSoundBankSlot slot, eSoundBank bank, eSoundID sfx, eAudioEvents event, CVector& posn, CPhysical* physical, float vol, float speed, int16 playPosn, float maxDistance) {
    if (!AEAudioHardware.IsSoundBankLoaded(bank, slot)) {
        return;
    }

    bool bFrontend = false;
    const auto volume = GetDefaultVolume(static_cast<eAudioEvents>(event)) + vol;
    CVector pos = [&] {
        if (physical) {
            return physical->GetPosition();
        } else if (posn == -1000.0f || posn.IsZero()) {
            bFrontend = true;
            return CVector{0.0f, 1.0f, 0.0f};
        } else {
            return posn;
        }
    }();

    AESoundManager.PlaySound({
        .BankSlotID         = slot,
        .SoundID            = sfx,
        .AudioEntity        = this,
        .Pos                = pos,
        .Volume             = volume,
        .RollOffFactor      = maxDistance,
        .Speed              = speed,
        .Doppler            = 1.0f,
        .FrameDelay         = 0,
        .Flags              = SOUND_START_PERCENTAGE | SOUND_REQUEST_UPDATES | SOUND_IS_CANCELLABLE | (bFrontend ? SOUND_FRONT_END : 0u),
        .FrequencyVariance  = 0.0f,
        .PlayTime           = playPosn,
        .RegisterWithEntity = physical,
        .EventID            = (eAudioEvents)(event),
    });
}

// 0x4EC270
void CAEScriptAudioEntity::PlayLoadedMissionAudio(uint8 sampleId) {
    if (sampleId >= MISSION_AUDIO_COUNT) {
        return;
    }
    auto& link = wavLinks[sampleId];
    if (link.m_nBankId < 0 || link.m_nBankSlotId < 0) {
        return;
    }
    if (GetMissionAudioLoadingStatus(sampleId) != 1) { // Not fully loaded yet
        return;
    }

    // Work out the volume
    float volume;
    bool  bDucking{}; // Slots 0/1 are the player slots, they get the ducking flags
    if (link.m_nAudioEvent == -1) { // 0xFFFF - no event (TODO: originally 65535, not AE_UNDEFINED)
        volume = -100.0f;            // VOLUME_SILENCE
    } else {
        volume   = GetDefaultVolume((eAudioEvents)link.m_nAudioEvent);
        bDucking = sampleId < 2;
        if (bDucking && volume == -128.0f) { // StaticRef<float>(0x85F3C4)
            volume = 6.0f;
        }
    }

    // Work out the position
    CVector pos;
    bool    bFrontEnd{};
    if (link.m_pEntity) {
        pos = link.m_pEntity->GetPosition();
    } else if (link.m_vPosition == CVector(-1000.0f, -1000.0f, -1000.0f) || link.m_vPosition.IsZero()) {
        pos        = CVector(0.0f, 1.0f, 0.0f);
        bFrontEnd  = true;
    } else {
        pos = link.m_vPosition;
    }

    // Original: local CAESound, Initialise(slot, soundId, this, pos, volume, 2.0f, 1.0f, 1.0f),
    // m_Flags = 0xE (SOUND_IS_CANCELLABLE|SOUND_REQUEST_UPDATES|SOUND_PLAY_PHYSICALLY),
    // SetFlags(SOUND_FRONT_END, bFrontEnd), SetFlags(SOUND_IS_DUCKABLE/SOUND_IS_COMPRESSABLE/SOUND_SMOOTH_DUCKING, bDucking),
    // then link.m_Sound = RequestNewSound(&sound). The sound's event is left AE_UNDEFINED.
    link.m_Sound = AESoundManager.PlaySound({
        .BankSlotID    = (eSoundBankSlot)(SND_BANK_SLOT_MISSION1 + sampleId),
        .SoundID       = (eSoundID)link.m_nBankSlotId, // sound id within the bank (m_nBankSlotId is dual-purpose)
        .AudioEntity   = this,
        .Pos           = pos,
        .Volume        = volume,
        .RollOffFactor = 2.0f,
        .Speed         = 1.0f,
        .Doppler       = 1.0f,
        .Flags         = SOUND_IS_CANCELLABLE | SOUND_REQUEST_UPDATES | SOUND_PLAY_PHYSICALLY
                       | (bFrontEnd ? SOUND_FRONT_END : 0u)
                       | (bDucking ? SOUND_IS_DUCKABLE | SOUND_IS_COMPRESSABLE | SOUND_SMOOTH_DUCKING : 0u),
    });
}
// 0x4EC190
void CAEScriptAudioEntity::PreloadMissionAudio(uint8 slotId, int32 sampleId) {
    if (slotId < MISSION_AUDIO_COUNT && IsMissionAudioSampleFinished(slotId)) {
        auto& link = wavLinks[slotId];
        // NOTE: `GetBankAndSoundFromScriptSlotAudioEvent` writes the sound id into an i32,
        // but only the low 16 bits are ever used later (original reads it as u16 too).
        eSoundBankS32 bankId{};
        if (CAEAudioUtility::GetBankAndSoundFromScriptSlotAudioEvent((eAudioEvents)sampleId, bankId, link.m_nBankSlotId, slotId)) {
            link.m_nBankId = bankId;
            if (link.m_nBankSlotId < 0) {
                AEAudioHardware.LoadSoundBank((eSoundBank)link.m_nBankId, (eSoundBankSlot)(SND_BANK_SLOT_MISSION1 + slotId));
            } else {
                AEAudioHardware.LoadSound((eSoundBank)link.m_nBankId, (eSoundID)link.m_nBankSlotId, (eSoundBankSlot)(SND_BANK_SLOT_MISSION1 + slotId));
            }
            link.m_nAudioEvent = sampleId;
            link.m_pEntity     = nullptr;
            link.m_vPosition   = CVector(-1000.0f, -1000.0f, -1000.0f);
        }
    }
}
// 0x4ECCF0
void CAEScriptAudioEntity::ProcessMissionAudioEvent(eAudioEvents eventId, CVector& posn, CPhysical* physical, float volume, float speed) {
    switch (eventId) {
    case AE_CRANE_WINCH_MOVE: // 104 (0x68)
        if (!field_7D) {
            break;
        }
        if (!AESoundManager.AreSoundsOfThisEventPlayingForThisEntityAndPhysical((int16)eventId, this, physical)) {
            PlayResidentSoundEvent(SND_BANK_SLOT_PLAYER_ENGINE_P, (eSoundBank)44, 1, eventId, posn, physical, volume, speed, 0, 2.5f);
        }
        m_Speed               = speed;
        m_Volume              = GetDefaultVolume(AE_CRANE_WINCH_MOVE) + volume;
        m_nLastTimeHornPlayed = CTimer::GetTimeInMS();
        break;

    case AE_SCRIPT_DISABLE_HELI_AUDIO: // 1000
        CAEVehicleAudioEntity::DisableHelicoptors();
        break;
    case AE_SCRIPT_ENABLE_HELI_AUDIO: // 1001
        CAEVehicleAudioEntity::EnableHelicoptors();
        break;
    case AE_SCRIPT_CEILING_VENT_LAND: // 1002
        PlayResidentSoundEvent((eSoundBankSlot)2, (eSoundBank)39, 64, eventId, posn, physical, 0.0f, 0.79f, 35, 1.0f);
        break;
    case AE_SCRIPT_CLAXON_START: // 1005
        PlayMissionBankSound(eventId, posn, physical, 1, 2);
        break;
    case AE_SCRIPT_CLAXON_STOP: // 1006
        AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity((int16)AE_SCRIPT_CLAXON_START, this);
        break;
    case AE_SCRIPT_BLAST_DOOR_SLIDE_START: // 1007
        // NOTE: shared tail with AE_SCRIPT_KEYPAD_BEEP's first branch (sfx 0 of the bank in mission slot 2;
        // Ghidra drops the sfx/a7/speed assignments on this path - restored from the keypad branch).
        PlayMissionBankSound(eventId, posn, physical, 0, 2);
        break;
    case AE_SCRIPT_BLAST_DOOR_SLIDE_STOP: // 1008
        AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity((int16)AE_SCRIPT_BLAST_DOOR_SLIDE_START, this);
        break;
    case AE_SCRIPT_BONNET_DENT: // 1009
        PlayResidentSoundEvent((eSoundBankSlot)2, (eSoundBank)39, (int16)CAEAudioUtility::GetRandomNumberInRange(20, 28), eventId, posn, physical, 0.0f, 1.0f, 0, 1.0f);
        break;
    case AE_SCRIPT_BASKETBALL_BOUNCE: // 1010
        PlayMissionBankSound(eventId, posn, physical, (int16)CAEAudioUtility::GetRandomNumberInRange(0, 2), 3);
        break;
    case AE_SCRIPT_BASKETBALL_HIT_HOOP: // 1011
        PlayMissionBankSound(eventId, posn, physical, 3, 3);
        break;
    case AE_SCRIPT_BASKETBALL_SCORE: // 1012
        if (AESoundManager.AreSoundsOfThisEventPlayingForThisEntity((int16)eventId, this)) {
            break;
        }
        PlayMissionBankSound(eventId, posn, physical, 4, 3);
        break;
    case AE_SCRIPT_POOL_HIT_WHITE: // 1014
        PlayMissionBankSound(eventId, posn, physical, 10, 3);
        break;
    case AE_SCRIPT_POOL_HIT_CUSHION: // 1016
        PlayMissionBankSound(eventId, posn, physical, 8, 3);
        break;
    case AE_SCRIPT_POOL_BALL_POT: // 1017
        PlayMissionBankSound(eventId, posn, physical, (int16)CAEAudioUtility::GetRandomNumberInRange(3, 5), 3);
        break;
    case AE_SCRIPT_POOL_CHALK_CUE: // 1018
        PlayMissionBankSound(eventId, posn, physical, 7, 3);
        break;
    case AE_SCRIPT_CRANE_ENTER: // 1019
        if (!AEAudioHardware.IsSoundBankLoaded((eSoundBank)44, SND_BANK_SLOT_PLAYER_ENGINE_P)) {
            if (AESoundManager.AreSoundsPlayingInBankSlot(SND_BANK_SLOT_PLAYER_ENGINE_P)) {
                AESoundManager.CancelSoundsInBankSlot(SND_BANK_SLOT_PLAYER_ENGINE_P, false);
            }
            AEAudioHardware.LoadSoundBank((eSoundBank)44, SND_BANK_SLOT_PLAYER_ENGINE_P);
        }
        m_Physical = physical;
        field_7D   = 1;
        break;
    case AE_SCRIPT_CRANE_MOVE_START: // 1020
        if (!field_7D) {
            break;
        }
        if (!AESoundManager.AreSoundsOfThisEventPlayingForThisEntityAndPhysical((int16)eventId, this, physical)) {
            PlayResidentSoundEvent(SND_BANK_SLOT_PLAYER_ENGINE_P, (eSoundBank)44, 1, eventId, posn, physical, 0.0f, 1.0f, 0, 2.5f);
        }
        break;
    case AE_SCRIPT_CRANE_MOVE_STOP: // 1021
        if (!field_7D) {
            break;
        }
        if (physical) {
            AESoundManager.CancelSoundsOfThisEventPlayingForThisEntityAndPhysical((int16)AE_SCRIPT_CRANE_MOVE_START, this, physical);
        } else {
            AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity((int16)AE_SCRIPT_CRANE_MOVE_START, this);
        }
        PlayResidentSoundEvent(SND_BANK_SLOT_PLAYER_ENGINE_P, (eSoundBank)44, 2, eventId, posn, physical, 0.0f, 1.0f, 0, 2.5f);
        break;
    case AE_SCRIPT_CRANE_EXIT: // 1022
        if (!field_7D) {
            break;
        }
        AESoundManager.CancelSoundsInBankSlot(SND_BANK_SLOT_PLAYER_ENGINE_P, true);
        PlayResidentSoundEvent(SND_BANK_SLOT_PLAYER_ENGINE_P, (eSoundBank)44, 3, eventId, posn, physical, 0.0f, 1.0f, 0, 2.5f);
        m_Physical = nullptr;
        field_7D   = 0;
        break;
    case AE_SCRIPT_VIDEO_POKER_PAYOUT: // 1025
        PlayMissionBankSound(eventId, posn, physical, 1, 3);
        break;
    case AE_SCRIPT_VIDEO_POKER_BUTTON: // 1026
        PlayMissionBankSound(eventId, posn, physical, 0, 3);
        break;
    case AE_SCRIPT_WHEEL_OF_FORTUNE_CLACKER: // 1027
        PlayResidentSoundEvent((eSoundBankSlot)2, (eSoundBank)39, 19, eventId, posn, physical, 0.0f, 1.0f, 70, 1.0f);
        break;
    case AE_SCRIPT_KEYPAD_BEEP: // 1028
        if (AEAudioHardware.IsSoundBankLoaded(SND_BANK_SCRIPT_KEYPAD, SND_BANK_SLOT_MISSION3)) {
            PlayMissionBankSound(eventId, posn, physical, 0, 2);
        } else if (AEAudioHardware.IsSoundBankLoaded(SND_BANK_SCRIPT_UNCLE_SAM, SND_BANK_SLOT_MISSION3)) {
            PlayMissionBankSound(eventId, posn, physical, 3, 2);
        }
        break;
    case AE_SCRIPT_KEYPAD_PASS: // 1029
        PlayMissionBankSound(eventId, posn, physical, 2, 2);
        break;
    case AE_SCRIPT_KEYPAD_FAIL: // 1030
        PlayMissionBankSound(eventId, posn, physical, 1, 2);
        break;
    case AE_SCRIPT_SHOOTING_RANGE_TARGET_SHATTER: // 1031
        PlayMissionBankSound(eventId, posn, physical, (int16)CAEAudioUtility::GetRandomNumberInRange(2, 6), 3);
        break;
    case AE_SCRIPT_SHOOTING_RANGE_TARGET_DROP: // 1032
        PlayMissionBankSound(eventId, posn, physical, 1, 3);
        break;
    case AE_SCRIPT_SHOOTING_RANGE_TARGET_MOVE_START: // 1033
        PlayMissionBankSound(eventId, posn, physical, 0, 3);
        break;
    case AE_SCRIPT_SHOOTING_RANGE_TARGET_MOVE_STOP: // 1034
        if (physical) {
            AESoundManager.CancelSoundsOfThisEventPlayingForThisEntityAndPhysical((int16)AE_SCRIPT_SHOOTING_RANGE_TARGET_MOVE_START, this, physical);
        } else {
            AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity((int16)AE_SCRIPT_SHOOTING_RANGE_TARGET_MOVE_START, this);
        }
        break;
    case AE_SCRIPT_SHUTTER_DOOR_START: // 1035
        m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENING, physical ? physical->GetPosition() : posn, 0.0f, 1.0f);
        break;
    case AE_SCRIPT_SHUTTER_DOOR_STOP: // 1036
        m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENED, physical ? physical->GetPosition() : posn, 0.0f, 1.0f);
        break;
    case AE_SCRIPT_PARACHUTE_OPEN: // 1039
        PlayResidentSoundEvent((eSoundBankSlot)5, (eSoundBank)143, 65, eventId, posn, physical, 0.0f, 1.0f, 0, 1.0f);
        break;
    case AE_SCRIPT_DUAL_SHOOT: // 1041
        PlayMissionBankSound(eventId, posn, physical, 8, 3);
        break;
    case AE_SCRIPT_DUAL_THRUST: // 1042
        PlayMissionBankSound(eventId, posn, physical, 0, 3, 1);
        m_nLastTimeHornPlayed = CTimer::GetTimeInMS();
        break;
    case AE_SCRIPT_DUAL_EXPLOSION_SHORT: // 1043
        PlayMissionBankSound(eventId, posn, physical, 2, 3, 1);
        break;
    case AE_SCRIPT_DUAL_EXPLOSION_LONG: // 1044
        PlayMissionBankSound(eventId, posn, physical, 1, 3, 1);
        break;
    case AE_SCRIPT_DUAL_MENU_SELECT: // 1045
        PlayMissionBankSound(eventId, posn, physical, 5, 3);
        break;
    case AE_SCRIPT_DUAL_MENU_DESELECT: // 1046
        PlayMissionBankSound(eventId, posn, physical, 4, 3);
        break;
    case AE_SCRIPT_DUAL_GAME_OVER: // 1047
        PlayMissionBankSound(eventId, posn, physical, 3, 3, 1);
        break;
    case AE_SCRIPT_DUAL_PICKUP_LIGHT: // 1048
        PlayMissionBankSound(eventId, posn, physical, 7, 3, 1);
        break;
    case AE_SCRIPT_DUAL_PICKUP_DARK: // 1049
        PlayMissionBankSound(eventId, posn, physical, 6, 3, 1);
        break;
    case AE_SCRIPT_DUAL_TOUCH_DARK: // 1050
        PlayMissionBankSound(eventId, posn, physical, 9, 3, 1);
        break;
    case AE_SCRIPT_DUAL_TOUCH_LIGHT: // 1051
        PlayMissionBankSound(eventId, posn, physical, 10, 3, 1);
        break;
    case AE_SCRIPT_AMMUNATION_BUY_WEAPON: // 1052
    case AE_SCRIPT_SHOP_BUY: // 1054
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PURCHASE_WEAPON);
        break;
    case AE_SCRIPT_AMMUNATION_BUY_WEAPON_DENIED: // 1053
    case AE_SCRIPT_SHOP_BUY_DENIED: // 1055
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_CAR_NO_CASH);
        break;
    case AE_SCRIPT_RACE_321: // 1056
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RACE_321);
        break;
    case AE_SCRIPT_RACE_GO: // 1057
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RACE_GO);
        break;
    case AE_SCRIPT_PART_MISSION_COMPLETE: // 1058
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PART_MISSION_COMPLETE);
        break;
    case AE_SCRIPT_GOGO_PLAYER_FIRE: // 1059
        PlayMissionBankSound(eventId, posn, physical, 5, 3);
        break;
    case AE_SCRIPT_GOGO_ENEMY_FIRE: // 1060
        PlayMissionBankSound(eventId, posn, physical, 5, 3, 0, 0.0f, 2.0f, 0.79f);
        break;
    case AE_SCRIPT_GOGO_EXPLOSION: // 1061
    case AE_SCRIPT_BANDIT_INSERT_COIN: // 1087
        PlayMissionBankSound(eventId, posn, physical, 3, 3);
        break;
    case AE_SCRIPT_GOGO_TRACK_START: // 1062
        AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack(AE_WEAPON_CHAINSAW_IDLE);
        break;
    case AE_SCRIPT_GOGO_TRACK_STOP: // 1063
    case AE_SCRIPT_DUAL_TRACK_STOP: // 1069
    case AE_SCRIPT_BEE_TRACK_STOP: // 1077
    case AE_SCRIPT_AWARD_TRACK_STOP: // 1098
        AEAmbienceTrackManager.StopSpecialMissionAmbienceTrack();
        break;
    case AE_SCRIPT_GOGO_SELECT: // 1064
        PlayMissionBankSound(eventId, posn, physical, 6, 3);
        break;
    case AE_SCRIPT_GOGO_ACCEPT: // 1065
        PlayMissionBankSound(eventId, posn, physical, 0, 3);
        break;
    case AE_SCRIPT_GOGO_DECLINE: // 1066
    case AE_SCRIPT_BEE_ACCEPT: // 1074
        PlayMissionBankSound(eventId, posn, physical, 1, 3);
        break;
    case AE_SCRIPT_GOGO_GAME_OVER: // 1067
        PlayMissionBankSound(eventId, posn, physical, 4, 3, 1);
        break;
    case AE_SCRIPT_DUAL_TRACK_START: // 1068
        AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack(AE_WEAPON_FIRE_MINIGUN_AMMO);
        break;
    case AE_SCRIPT_BEE_ZAP: // 1070
        PlayMissionBankSound(eventId, posn, physical, 6, 3);
        break;
    case AE_SCRIPT_BEE_PICKUP: // 1071
        PlayMissionBankSound(eventId, posn, physical, 4, 3);
        break;
    case AE_SCRIPT_BEE_DROP: // 1072
        PlayMissionBankSound(eventId, posn, physical, 2, 3);
        break;
    case AE_SCRIPT_BEE_SELECT: // 1073
        PlayMissionBankSound(eventId, posn, physical, 5, 3);
        break;
    case AE_SCRIPT_BEE_DECLINE: // 1075
        PlayMissionBankSound(eventId, posn, physical, 1, 3, 0, 0.0f, 2.0f, 0.79f);
        break;
    case AE_SCRIPT_BEE_TRACK_START: // 1076
        AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack(AE_THUNDER);
        break;
    case AE_SCRIPT_BEE_GAME_OVER: // 1078
        PlayMissionBankSound(eventId, posn, physical, 3, 3, 1);
        break;
    case AE_SCRIPT_FREEZER_OPEN: // 1079
        PlayMissionBankSound(eventId, posn, physical, 2, 2, 1, 0.0f, 3.0f);
        break;
    case AE_SCRIPT_FREEZER_CLOSE: // 1080
        PlayMissionBankSound(eventId, posn, physical, 1, 2, 1, 0.0f, 3.0f);
        break;
    case AE_SCRIPT_MEAT_TRACK_START: // 1081
        PlayMissionBankSound(eventId, posn, physical, 3, 2, 1, 0.0f, 3.0f);
        PlayMissionBankSound(eventId, posn, physical, 0, 2, 0, 0.0f, 3.0f, 1.0f);
        break;
    case AE_SCRIPT_MEAT_TRACK_STOP: // 1082
        AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity((int16)AE_SCRIPT_MEAT_TRACK_START, this);
        PlayMissionBankSound(eventId, posn, physical, 4, 2, 1, 0.0f, 3.0f);
        break;
    case AE_SCRIPT_ROULETTE_ADD_CASH: // 1083
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_SELECT);
        break;
    case AE_SCRIPT_ROULETTE_REMOVE_CASH: // 1084
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_BACK);
        break;
    case AE_SCRIPT_ROULETTE_NO_CASH: // 1085
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_ERROR);
        break;
    case AE_SCRIPT_ROULETTE_SPIN: // 1086
        PlayMissionBankSound(eventId, posn, physical, 0, 3);
        m_nLastTimeHornPlayed = CTimer::GetTimeInMS();
        field_7C              = 0;
        break;
    case AE_SCRIPT_BANDIT_WHEEL_STOP: // 1088
        if (physical) {
            AESoundManager.CancelSoundsOfThisEventPlayingForThisEntityAndPhysical((int16)AE_SCRIPT_BANDIT_WHEEL_START, this, physical);
        } else {
            AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity((int16)AE_SCRIPT_BANDIT_WHEEL_START, this);
        }
        PlayMissionBankSound(eventId, posn, physical, 2, 3);
        break;
    case AE_SCRIPT_BANDIT_WHEEL_START: // 1089
        PlayMissionBankSound(eventId, posn, physical, 0, 3);
        break;
    case AE_SCRIPT_BANDIT_PAYOUT: // 1090
        PlayMissionBankSound(eventId, posn, physical, 1, 3);
        break;
    case AE_SCRIPT_BIKE_PACKER_CLUNK: // 1095
        PlayResidentSoundEvent((eSoundBankSlot)2, (eSoundBank)39, 65, eventId, posn, physical, 0.0f, 1.0f, 16, 1.0f);
        break;
    case AE_SCRIPT_AWARD_TRACK_START: // 1097
        AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack(AE_WEAPON_FIRE);
        break;
    case AE_SCRIPT_MESH_GATE_OPEN_START: // 1100
        if (field_7E) {
            PlayResidentSoundEvent((eSoundBankSlot)2, (eSoundBank)39, 69, eventId, posn, physical, 0.0f, 1.0f, 63, 1.0f);
        } else {
            PlayResidentSoundEvent((eSoundBankSlot)2, (eSoundBank)39, 70, eventId, posn, physical, 0.0f, 1.0f, 62, 1.0f);
        }
        field_7E = (field_7E + 1) % 2;
        break;
    case AE_SCRIPT_MESH_GATE_OPEN_STOP: // 1101
        if (field_7E) {
            PlayResidentSoundEvent((eSoundBankSlot)2, (eSoundBank)39, 69, eventId, posn, physical, 0.0f, 0.94f, 33, 1.0f);
        } else {
            PlayResidentSoundEvent((eSoundBankSlot)2, (eSoundBank)39, 70, eventId, posn, physical, 0.0f, 1.0f, 25, 1.0f);
        }
        field_7E = (field_7E + 1) % 2;
        break;
    case AE_SCRIPT_OGLOC_DOORBELL: // 1102
        PlayMissionBankSound(eventId, posn, physical, 0, 2);
        break;
    case AE_SCRIPT_OGLOC_WINDOW_RATTLE_BANG: // 1103
        PlayMissionBankSound(eventId, posn, physical, 1, 2);
        break;
    case AE_SCRIPT_STINGER_RELOAD: // 1104
    case AE_SCRIPT_MECHANIC_SLIDE_OUT: // 1151
        PlayMissionBankSound(eventId, posn, physical, 1, 2);
        break;
    case AE_SCRIPT_HEAVY_DOOR_START: // 1105
        PlayMissionBankSound(eventId, posn, physical, 2, 2);
        break;
    case AE_SCRIPT_SHOOT_CONTROLS: // 1107
        if (AEAudioHardware.IsSoundBankLoaded(SND_BANK_SCRIPT_BLACK_PROJECT, SND_BANK_SLOT_MISSION3)) {
            PlayMissionBankSound(eventId, posn, physical, 3, 2);
        } else if (AEAudioHardware.IsSoundBankLoaded(SND_BANK_SCRIPT_UNCLE_SAM, SND_BANK_SLOT_MISSION3)) {
            PlayMissionBankSound(eventId, posn, physical, 4, 2);
        }
        break;
    case AE_SCRIPT_CARGO_PLANE_DOOR_START: // 1108
        PlayMissionBankSound(eventId, posn, physical, 1, 2);
        PlayMissionBankSound(eventId, posn, physical, 0, 2);
        break;
    case AE_SCRIPT_CARGO_PLANE_DOOR_STOP: // 1109
        AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity((int16)AE_SCRIPT_CARGO_PLANE_DOOR_START, this);
        PlayMissionBankSound(eventId, posn, physical, 2, 2);
        break;
    case AE_SCRIPT_DA_NANG_CONTAINER_OPEN: // 1110
    case AE_SCRIPT_MECHANIC_ATTACH_CAR_BOMB: // 1152
    case AE_SCRIPT_CAT2_SECURITY_ALARM: // 1155
        PlayMissionBankSound(eventId, posn, physical, 0, 2);
        break;
    case AE_SCRIPT_DA_NANG_HEAVY_DOOR_OPEN: // 1111
    case AE_SCRIPT_CAT2_WOODEN_DOOR_BREACH: // 1156
        PlayMissionBankSound(eventId, posn, physical, 1, 2);
        break;
    case AE_SCRIPT_GYM_BIKE_START: // 1113
        PlayMissionBankSound(eventId, posn, physical, 0, 3, 1, -18.0f, 2.0f, 1.0f);
        field_8C = 1.0f;
        break;
    case AE_SCRIPT_GYM_BIKE_STOP: // 1114
    case AE_SCRIPT_GYM_RUNNING_MACHINE_STOP: // 1119
        field_8C = 2.0f;
        break;
    case AE_SCRIPT_GYM_BOXING_BELL: // 1115
    case AE_SCRIPT_TEMPEST_EXPLOSION: // 1172
        PlayMissionBankSound(eventId, posn, physical, 2, 3);
        break;
    case AE_SCRIPT_GYM_INCREASE_DIFFICULTY: // 1116
        PlayMissionBankSound(eventId, posn, physical, 3, 3);
        break;
    case AE_SCRIPT_GYM_REST_WEIGHTS: // 1117
        PlayMissionBankSound(eventId, posn, physical, 7, 3);
        break;
    case AE_SCRIPT_GYM_RUNNING_MACHINE_START: // 1118
        PlayMissionBankSound(eventId, posn, physical, 1, 3, 1, -18.0f, 2.0f, 1.0f);
        field_8C = 1.0f;
        break;
    case AE_SCRIPT_OTB_BET_ZERO: // 1120
        PlayMissionBankSound(eventId, posn, physical, 0, 3);
        break;
    case AE_SCRIPT_OTB_INCREASE_BET: // 1121
    case AE_SCRIPT_TEMPEST_ENEMY_SHOOT: // 1171
        PlayMissionBankSound(eventId, posn, physical, 1, 3);
        break;
    case AE_SCRIPT_OTB_LOSE: // 1122
        PlayMissionBankSound(eventId, posn, physical, 2, 3, 1);
        break;
    case AE_SCRIPT_OTB_PLACE_BET: // 1123
        PlayMissionBankSound(eventId, posn, physical, 4, 3);
        break;
    case AE_SCRIPT_OTB_WIN: // 1124
        PlayMissionBankSound(eventId, posn, physical, 5, 3, 1);
        break;
    case AE_SCRIPT_STINGER_FIRE: // 1125
        PlayMissionBankSound(eventId, posn, physical, 0, 2);
        break;
    case AE_SCRIPT_HEAVY_GATE_START: // 1126
        PlayMissionBankSound(eventId, posn, physical, 1, 2);
        PlayMissionBankSound(eventId, posn, physical, 0, 2);
        break;
    case AE_SCRIPT_HEAVY_GATE_STOP: // 1127
        AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity((int16)AE_SCRIPT_HEAVY_GATE_START, this);
        PlayMissionBankSound(eventId, posn, physical, 2, 2);
        break;
    case AE_SCRIPT_VERTICAL_BIRD_LIFT_START: // 1128
        PlayMissionBankSound(eventId, posn, physical, 2, 2);
        PlayMissionBankSound(eventId, posn, physical, 0, 2);
        break;
    case AE_SCRIPT_VERTICAL_BIRD_LIFT_STOP: // 1129
        if (physical) {
            AESoundManager.CancelSoundsOfThisEventPlayingForThisEntityAndPhysical((int16)AE_SCRIPT_VERTICAL_BIRD_LIFT_START, this, physical);
        } else {
            AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity((int16)AE_SCRIPT_VERTICAL_BIRD_LIFT_START, this);
        }
        PlayMissionBankSound(eventId, posn, physical, 3, 2);
        break;
    case AE_SCRIPT_PUNCH_PED: // 1130
        PlayResidentSoundEvent((eSoundBankSlot)5, (eSoundBank)143, 58, eventId, posn, physical, 0.0f, 1.0f, 0, 1.0f);
        PlayResidentSoundEvent((eSoundBankSlot)5, (eSoundBank)143, 40, eventId, posn, physical, 0.0f, 1.0f, 0, 1.0f);
        break;
    case AE_SCRIPT_AMMUNATION_GUN_COLLISION: // 1131
        PlayResidentSoundEvent((eSoundBankSlot)2, (eSoundBank)39, 33, eventId, posn, physical, 0.0f, 1.0f, 0, 1.0f);
        PlayResidentSoundEvent((eSoundBankSlot)2, (eSoundBank)39, 50, eventId, posn, physical, 0.0f, 0.79f, 22, 1.0f);
        break;
    case AE_SCRIPT_CAMERA_SHOT: // 1132
        if (physical) {
            AudioEngine.ReportWeaponEvent(AE_WEAPON_FIRE, WEAPON_CAMERA, physical);
        } else {
            PlayResidentSoundEvent((eSoundBankSlot)5, (eSoundBank)143, 45, eventId, posn, nullptr, 0.0f);
        }
        break;
    case AE_SCRIPT_BUY_CAR_MOD: // 1133
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_BUY_CAR_MOD);
        break;
    case AE_SCRIPT_BUY_CAR_RESPRAY: // 1134
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_CAR_RESPRAY);
        break;
    case AE_SCRIPT_BASEBALL_BAT_HIT_PED: // 1135
        PlayResidentSoundEvent((eSoundBankSlot)5, (eSoundBank)143, 34, eventId, posn, physical, 5.0f, 1.0f, 0, 1.0f);
        PlayResidentSoundEvent((eSoundBankSlot)5, (eSoundBank)143, 40, eventId, posn, physical, 0.0f, 1.0f, 0, 1.0f);
        break;
    case AE_SCRIPT_STAMP_PED: // 1136
        PlayResidentSoundEvent((eSoundBankSlot)5, (eSoundBank)143, 82, eventId, posn, physical, -3.0f, 1.0f, 0, 1.0f);
        break;
    case AE_SCRIPT_CHECKPOINT_AMBER: // 1137
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PART_MISSION_COMPLETE, 0.0f, 1.12f);
        break;
    case AE_SCRIPT_CHECKPOINT_GREEN: // 1138
    case AE_SCRIPT_PROPERTY_PURCHASED: // 1149
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PART_MISSION_COMPLETE);
        break;
    case AE_SCRIPT_CHECKPOINT_RED: // 1139
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PART_MISSION_COMPLETE, 0.0f, 1.26f);
        break;
    case AE_SCRIPT_CAR_SMASH_CAR: // 1140
    case AE_SCRIPT_MAGNET_VEHICLE_COLLISION: // 1148
        PlayResidentSoundEvent((eSoundBankSlot)2, (eSoundBank)39, (int16)CAEAudioUtility::GetRandomNumberInRange(20, 28), eventId, posn, physical, 0.0f, 1.0f, 0, 1.0f);
        break;
    case AE_SCRIPT_CAR_SMASH_GATE: // 1141
        PlayResidentSoundEvent((eSoundBankSlot)2, (eSoundBank)39, (int16)CAEAudioUtility::GetRandomNumberInRange(20, 28), eventId, posn, physical, 0.0f, 1.0f, 0, 1.0f);
        PlayResidentSoundEvent((eSoundBankSlot)2, (eSoundBank)39, 65, eventId, posn, physical, 0.0f, 1.0f, 0, 1.0f);
        break;
    case AE_SCRIPT_OTB_TRACK_START: // 1142
        AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack(AE_GARAGE_DOOR_OPENED);
        break;
    case AE_SCRIPT_OTB_TRACK_STOP: // 1143
    case AE_SCRIPT_TEMPEST_TRACK_STOP: // 1182
    case AE_SCRIPT_DRIVING_AWARD_TRACK_STOP: // 1184
    case AE_SCRIPT_BIKE_AWARD_TRACK_STOP: // 1186
    case AE_SCRIPT_PILOT_AWARD_TRACK_STOP: // 1188
        AEAmbienceTrackManager.StopSpecialMissionAmbienceTrack();
        break;
    case AE_SCRIPT_PED_HIT_WATER_SPLASH: // 1144
        if (physical) {
            AudioEngine.ReportWaterSplash(physical, -6.0f, false);
        } else {
            AudioEngine.ReportWaterSplash(posn, -6.0f);
        }
        break;
    case AE_SCRIPT_RESTAURANT_TRAY_COLLISION: // 1145
        PlayResidentSoundEvent((eSoundBankSlot)2, (eSoundBank)39, 19, eventId, posn, physical, 0.0f, 1.0f, 65, 1.0f);
        break;
    case AE_SCRIPT_SWEETS_HORN: // 1147
        if (!AESoundManager.AreSoundsOfThisEventPlayingForThisEntity((int16)eventId, this)) {
            PlayResidentSoundEvent((eSoundBankSlot)17, (eSoundBank)74, 7, eventId, posn, physical, 0.0f, 1.0f, 0, 1.0f);
            m_nLastTimeHornPlayed = CTimer::GetTimeInMS();
        }
        break;
    case AE_SCRIPT_PICKUP_STANDARD: // 1150
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_MONEY);
        break;
    case AE_SCRIPT_GARAGE_DOOR_START: // 1153
        m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENING, physical ? physical->GetPosition() : posn, 0.0f, 1.0f);
        break;
    case AE_SCRIPT_GARAGE_DOOR_STOP: // 1154
        m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENED, physical ? physical->GetPosition() : posn, 0.0f, 1.0f);
        break;
    case AE_SCRIPT_MINITANK_FIRE: // 1157
        if (physical) {
            AudioEngine.ReportWeaponEvent(AE_WEAPON_FIRE, WEAPON_M4, physical);
        }
        break;
    case AE_SCRIPT_OTB_NO_CASH: // 1158
        PlayMissionBankSound(eventId, posn, physical, 3, 3);
        break;
    case AE_SCRIPT_EXPLOSION: // 1159
        m_ExplosionAudio.AddAudioEvent(AE_EXPLOSION, physical ? physical->GetPosition() : posn, 0.0f);
        break;
    case AE_SCRIPT_VERTICAL_BIRD_ALARM_START: // 1161
        PlayMissionBankSound(eventId, posn, physical, 1, 2);
        break;
    case AE_SCRIPT_VERTICAL_BIRD_ALARM_STOP: // 1162
        if (physical) {
            AESoundManager.CancelSoundsOfThisEventPlayingForThisEntityAndPhysical((int16)AE_SCRIPT_VERTICAL_BIRD_ALARM_START, this, physical);
        } else {
            AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity((int16)AE_SCRIPT_VERTICAL_BIRD_ALARM_START, this);
        }
        break;
    case AE_SCRIPT_PED_COLLAPSE: // 1163
        PlayResidentSoundEvent((eSoundBankSlot)41, (eSoundBank)0, 0, eventId, posn, physical, 0.0f, 1.0f, 0, 1.0f);
        break;
    case AE_SCRIPT_AIR_HORN: { // 1164
        CVector vec0 = { -1.0f, 0.0f, 0.0f };
        PlayMissionBankSound(eventId, vec0, nullptr, 0, 3);
        CVector vec1 = { +1.0f, 0.0f, 0.0f };
        PlayMissionBankSound(eventId, vec1, nullptr, 1, 3);
        break;
    }
    case AE_SCRIPT_SHUTTER_DOOR_SLOW_START: // 1165
        m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENING, physical ? physical->GetPosition() : posn, 0.0f, 0.79f);
        break;
    case AE_SCRIPT_SHUTTER_DOOR_SLOW_STOP: // 1166
        m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENED, physical ? physical->GetPosition() : posn, 0.0f, 1.0f);
        break;
    case AE_SCRIPT_BEE_BUZZ: // 1167
        PlayMissionBankSound(eventId, posn, physical, 0, 3, 1);
        m_nLastTimeHornPlayed = CTimer::GetTimeInMS();
        break;
    case AE_SCRIPT_RESTAURANT_CJ_EAT: // 1168
        PlayMissionBankSound(eventId, posn, physical, 0, 3);
        break;
    case AE_SCRIPT_RESTAURANT_CJ_PUKE: // 1169
        PlayMissionBankSound(eventId, posn, physical, 1, 3);
        break;
    case AE_SCRIPT_TEMPEST_PLAYER_SHOOT: // 1170
        PlayMissionBankSound(eventId, posn, physical, 6, 3);
        break;
    case AE_SCRIPT_TEMPEST_PICKUP1: // 1173
        PlayMissionBankSound(eventId, posn, physical, 5, 3, 0, 0.0f, 2.0f, 0.67f);
        break;
    case AE_SCRIPT_TEMPEST_PICKUP2: // 1174
        PlayMissionBankSound(eventId, posn, physical, 5, 3, 0, 0.0f, 2.0f, 0.79f);
        break;
    case AE_SCRIPT_TEMPEST_PICKUP3: // 1175
        PlayMissionBankSound(eventId, posn, physical, 5, 3);
        break;
    case AE_SCRIPT_TEMPEST_WARP: // 1176
        PlayMissionBankSound(eventId, posn, physical, 8, 3, 1, 0.0f, 2.0f, 0.38f);
        break;
    case AE_SCRIPT_TEMPEST_SHIELD_GLOW: // 1177
        PlayMissionBankSound(eventId, posn, physical, 0, 3, 1);
        m_nLastTimeHornPlayed = CTimer::GetTimeInMS();
        break;
    case AE_SCRIPT_TEMPEST_GAME_OVER: // 1178
        PlayMissionBankSound(eventId, posn, physical, 3, 3, 1);
        break;
    case AE_SCRIPT_TEMPEST_HIGHLIGHT: // 1179
        PlayMissionBankSound(eventId, posn, physical, 4, 3);
        break;
    case AE_SCRIPT_TEMPEST_SELECT: // 1180
        PlayMissionBankSound(eventId, posn, physical, 7, 3);
        break;
    case AE_SCRIPT_TEMPEST_TRACK_START: // 1181
        AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack((eAudioEvents)172); // 172 - старое имя AE_BLAST_DOOR_SLIDE_START (в текущем eAudioEvents нет)
        break;
    case AE_SCRIPT_DRIVING_AWARD_TRACK_START: // 1183
        AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack(AE_WEAPON_FIRE_PLANE);
        break;
    case AE_SCRIPT_BIKE_AWARD_TRACK_START: // 1185
        AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack(AE_RAIN_COLLISION);
        break;
    case AE_SCRIPT_PILOT_AWARD_TRACK_START: // 1187
        AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack(AE_GARAGE_DOOR_CLOSING);
        break;
    case AE_SCRIPT_PED_DEATH_CRUNCH: // 1189
        if (CLocalisation::Blood() && physical && physical->GetIsTypePed()) {
            physical->AsPed()->GetAE().AddAudioEvent(AE_PED_CRUNCH, 0.0f, 1.0f, physical, (eSurfaceType)0, 0, 0);
        }
        break;
    case AE_SCRIPT_SPANK: // 1190
        PlayResidentSoundEvent((eSoundBankSlot)5, (eSoundBank)143, (int16)CAEAudioUtility::GetRandomNumberInRange(78, 80), eventId, posn, physical, 0.0f, 1.0f, 0, 1.0f);
        break;
    default:
        break;
    }
}
// 0x4EE960
void CAEScriptAudioEntity::ReportMissionAudioEvent(eAudioEvents eventId, CPhysical* physical, float volume, float speed) {
    CVector posn{ -1000.0f, -1000.0f, -1000.0f };
    ProcessMissionAudioEvent(eventId, posn, physical, volume, speed);
}

// 0x4EE940
void CAEScriptAudioEntity::ReportMissionAudioEvent(eAudioEvents eventId, CVector& posn) {
    ProcessMissionAudioEvent(eventId, posn, nullptr);
}

// 0x4EC970
// `playTime` == -1 => the sound has finished
void CAEScriptAudioEntity::UpdateParameters(CAESound* sound, int16 playTime) {
    if (!sound) {
        return;
    }

    // NOTE (faithful): the original walks all 4 wav links; the event-based handling below runs
    // for every link whose `m_Sound` isn't `sound` - i.e. up to 4 times for sounds that aren't
    // mission-audio link sounds. Kept as-is.
    for (auto& link : wavLinks) {
        if (link.m_Sound == sound) {
            if (playTime == -1) {
                link.m_Sound = nullptr;
                return;
            }
            if (link.m_pEntity) {
                sound->SetPosition(link.m_pEntity->GetPosition());
            }
            continue;
        }

        switch ((eAudioEvents)sound->m_Event) {
        case AE_SCRIPT_GYM_BIKE_START:
        case AE_SCRIPT_GYM_RUNNING_MACHINE_START: {
            const auto targetVolume = GetDefaultVolume((eAudioEvents)sound->m_Event);
            if (field_8C == 1.0f) { // Ramp up
                if (sound->m_Volume < targetVolume) {
                    sound->m_Volume = std::min(sound->m_Volume + StaticRef<float>(0x858B1C), targetVolume);
                }
            } else if (field_8C == 2.0f) { // Ramp down, then stop
                const auto stopVolume = targetVolume - StaticRef<float>(0x859008);
                if (sound->m_Volume <= stopVolume) {
                    sound->StopSoundAndForget();
                } else {
                    sound->m_Volume = std::max(sound->m_Volume - StaticRef<float>(0x858B1C), stopVolume);
                }
            }
            break;
        }
        case AE_SCRIPT_ROULETTE_SPIN:
            if (m_nLastTimeHornPlayed + 4500U < CTimer::m_snTimeInMilliseconds) {
                if (sound->m_Volume <= StaticRef<float>(0x85A708)) {
                    sound->StopSoundAndForget();
                    m_nLastTimeHornPlayed = 0;
                } else {
                    sound->m_Volume -= StaticRef<float>(0x858B1C);
                }
                if (0.0f < sound->m_Speed) { // vs 0x858B50
                    sound->m_Speed = std::max(sound->m_Speed - StaticRef<float>(0x858CDC), 0.0f);
                }
            }
            if (m_nLastTimeHornPlayed + 4800U < CTimer::m_snTimeInMilliseconds && !field_7C) {
                CVector posn(-1000.0f, -1000.0f, -1000.0f);
                PlayMissionBankSound(AE_SCRIPT_AIR_HORN, posn, nullptr, (int16)CAEAudioUtility::GetRandomNumberInRange(1, 3), 3);
                field_7C = 1;
            }
            break;
        case AE_CRANE_WINCH_MOVE:
            if (m_nLastTimeHornPlayed + 300U < CTimer::m_snTimeInMilliseconds) {
                sound->StopSoundAndForget();
                m_nLastTimeHornPlayed = 0;
                PlayResidentSoundEvent(
                    (eSoundBankSlot)40, (eSoundBank)44, 2, AE_SCRIPT_CRANE_MOVE_START,
                    sound->m_CurrPos,
                    static_cast<CPhysical*>((CEntity*)sound->m_PhysicalEntity),
                    -12.0f, 1.0f, 0, 2.5f
                );
            } else {
                sound->m_Volume = m_Volume;
                sound->m_Speed  = m_Speed;
            }
            break;
        case AE_SCRIPT_SWEETS_HORN:
            if (m_nLastTimeHornPlayed + 500U < CTimer::m_snTimeInMilliseconds) {
                sound->StopSoundAndForget();
                m_nLastTimeHornPlayed = 0;
            }
            break;
        case AE_SCRIPT_DUAL_THRUST:
        case AE_SCRIPT_BEE_BUZZ:
        case AE_SCRIPT_TEMPEST_SHIELD_GLOW:
            if (m_nLastTimeHornPlayed + 300U < CTimer::m_snTimeInMilliseconds) {
                sound->StopSoundAndForget();
                m_nLastTimeHornPlayed = 0;
            }
            break;
        default:
            break;
        }
    }
}
// 0x4EC900
void CAEScriptAudioEntity::Service() {
    CVector posn = {-1000.0f, -1000.0f, -1000.0f};
    if (!m_Physical)
        return;
    if (AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(AE_SCRIPT_CRANE_ENTER, this) != 0)
        return;

    PlayResidentSoundEvent(SND_BANK_SLOT_PLAYER_ENGINE_P, SND_BANK_GENRL_CRANE_P, 0, AE_SCRIPT_CRANE_ENTER, posn, m_Physical, 0.0f, 1.0f, 0, 2.5f);
}

void CAEScriptAudioEntity::InjectHooks() {
    RH_ScopedVirtualClass(CAEScriptAudioEntity, 0x862E58, 1);
    RH_ScopedCategory("Audio/Entities");

    RH_ScopedInstall(Constructor, 0x5074D0);
    RH_ScopedInstall(Initialise, 0x5B9B60);
    RH_ScopedInstall(Service, 0x4EC900);
    RH_ScopedInstall(Reset, 0x4EC150);
    RH_ScopedInstall(GetMissionAudioLoadingStatus, 0x4EBF60);
    RH_ScopedInstall(IsMissionAudioSampleFinished, 0x4EBFE0);
    RH_ScopedInstall(GetMissionAudioEvent, 0x4EC020);
    RH_ScopedInstall(ClearMissionAudio, 0x4EC040);
    RH_ScopedInstall(SetMissionAudioPosition, 0x4EC0C0);
    RH_ScopedInstall(AttachMissionAudioToPhysical, 0x4EC100);
    RH_ScopedInstall(PreloadMissionAudio, 0x4EC190);
    RH_ScopedInstall(PlayLoadedMissionAudio, 0x4EC270);
    RH_ScopedInstall(GetMissionAudioPosition, 0x4EC4D0);
    RH_ScopedInstall(PlayResidentSoundEvent, 0x4EC550);
    RH_ScopedInstall(PlayMissionBankSound, 0x4EC6D0);
    RH_ScopedInstall(ProcessMissionAudioEvent, 0x4ECCF0);
    RH_ScopedOverloadedInstall(ReportMissionAudioEvent, "1", 0x4EE960, void (CAEScriptAudioEntity::*)(eAudioEvents, CPhysical*, float, float));
    RH_ScopedOverloadedInstall(ReportMissionAudioEvent, "2", 0x4EE940, void (CAEScriptAudioEntity::*)(eAudioEvents, CVector&));
    RH_ScopedVMTInstall(UpdateParameters, 0x4EC970);
}

CAEScriptAudioEntity* CAEScriptAudioEntity::Constructor() {
    this->CAEScriptAudioEntity::CAEScriptAudioEntity();
    return this;
}
