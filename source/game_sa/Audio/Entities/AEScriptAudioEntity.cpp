#include "StdInc.h"

#include "AEScriptAudioEntity.h"
#include "AESoundManager.h"
#include "AEAudioUtility.h"
#include "AEAudioHardware.h"
#include "AEAmbienceTrackManager.h"

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
    auto& link      = wavLinks[sampleId];
    link.m_pEntity   = physical;
    link.m_vPosition = CVector(-1000.0f, -1000.0f, -1000.0f);
}
// 0x4EC040
void CAEScriptAudioEntity::ClearMissionAudio(uint8 sampleId) {
    if (sampleId < MISSION_AUDIO_COUNT) {
        AESoundManager.CancelSoundsInBankSlot((int16)(SND_BANK_SLOT_MISSION1 + sampleId), true);
        auto& link       = wavLinks[sampleId];
        link.m_pEntity   = nullptr;
        link.m_vPosition = CVector(-1000.0f, -1000.0f, -1000.0f);
        link.m_Sound     = nullptr;
    }
}
// 0x4EBFE0
bool CAEScriptAudioEntity::IsMissionAudioSampleFinished(uint8 sampleId) {
    if (sampleId >= MISSION_AUDIO_COUNT) {
        return true;
    }
    if (sampleId > 1) {
        return AESoundManager.AreSoundsPlayingInBankSlot((int16)(SND_BANK_SLOT_MISSION1 + sampleId)) == eSoundPlayingStatus::SOUND_NOT_PLAYING;
    }
    return wavLinks[sampleId].m_Sound == nullptr;
}
// 0x4EBF60
int8 CAEScriptAudioEntity::GetMissionAudioLoadingStatus(uint8 sampleId) {
    if (sampleId < MISSION_AUDIO_COUNT) {
        auto& link = wavLinks[sampleId];
        if (link.m_nBankId >= 0) {
            if (link.m_nBankSlotId != -1) {
                return AEAudioHardware.GetSoundLoadingStatus((eSoundBank)link.m_nBankId, (eSoundID)link.m_nBankSlotId, (eSoundBankSlot)(SND_BANK_SLOT_MISSION1 + sampleId));
            }
            return AEAudioHardware.GetSoundBankLoadingStatus((eSoundBank)link.m_nBankId, (eSoundBankSlot)(SND_BANK_SLOT_MISSION1 + sampleId));
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
    auto& link       = wavLinks[sampleId];
    link.m_vPosition = posn;
    link.m_pEntity   = nullptr;
}
// 0x4EC4D0
// NOTE: Faithful port - position/entity are read directly and the special
// position checks are done component-wise (as in the original - no CVector
// temporaries / operator!= calls).
// NOTE: Instruction-level port of the original (hence the inline assembly).
// The link array/entity/matrix are accessed through the original's raw offsets
// and the "is position set" test replicates the original's integer compare
// against the -1000.0f bit pattern (0xC47A0000) followed by the three
// x87 > 0 checks (test ah, 0x44 / jp).
namespace { volatile const float FLT_ZERO_858B50 = 0.0f; } // 0x858B50

__declspec(naked) CVector* CAEScriptAudioEntity::GetMissionAudioPosition(uint8 /*sampleId*/) {
    __asm {
        movzx eax, byte ptr [esp + 4]       ; sampleId
        shl eax, 5                          ; * 0x20 (sizeof(CAudioLink))
        add ecx, eax                        ; &wavLinks[sampleId]
        mov eax, dword ptr [ecx + 0x9C]     ; link.m_pEntity
        test eax, eax
        je no_entity
        mov ecx, dword ptr [eax + 0x14]     ; entity->m_matrix
        test ecx, ecx
        je no_matrix
        lea eax, [ecx + 0x30]               ; &matrix->pos
        ret 4
no_matrix:
        add eax, 4                          ; &placement.pos
        ret 4
no_entity:
        mov edx, dword ptr [ecx + 0x90]     ; link.m_vPosition.x
        add ecx, 0x90                       ; &link.m_vPosition
        mov eax, 0xC47A0000                 ; -1000.0f
        cmp edx, eax
        jne check_zero
        cmp dword ptr [ecx + 4], eax
        jne check_zero
        cmp dword ptr [ecx + 8], eax
        je null_pos
check_zero:
        fld dword ptr [ecx]
        fcomp FLT_ZERO_858B50
        fnstsw ax
        test ah, 0x44
        jp valid
        fld dword ptr [ecx + 4]
        fcomp FLT_ZERO_858B50
        fnstsw ax
        test ah, 0x44
        jp valid
        fld dword ptr [ecx + 8]
        fcomp FLT_ZERO_858B50
        fnstsw ax
        test ah, 0x44
        jnp null_pos
valid:
        mov eax, ecx
        ret 4
null_pos:
        xor eax, eax
        ret 4
    }
}

// 0x4EC6D0
// `sampleId` - mission audio event id, `sfxId` - sound within the bank, `linkId` - mission audio slot (2 or 3)
void CAEScriptAudioEntity::PlayMissionBankSound(uint8 sampleId, CVector& posn, CPhysical* physical, int16 sfxId, uint8 linkId, uint8 a7, float volume, float maxDistance, float speed) {
    // Mission bank sounds only ever use slots 2/3
    if (linkId < 2 || linkId > 3) {
        return;
    }
    if (a7 != 0 && AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(sampleId, this)) {
        return;
    }

    const auto bankSlot = (eSoundBankSlot)(SND_BANK_SLOT_MISSION1 + linkId);
    if (!AEAudioHardware.IsSoundBankLoaded((eSoundBank)wavLinks[linkId].m_nBankId, bankSlot)) {
        return;
    }

    const auto finalVolume = GetDefaultVolume((eAudioEvents)sampleId) + volume;

    // Work out the position
    CVector pos;
    bool    bFrontEnd{};
    if (physical) {
        pos = physical->GetPosition();
    } else {
        constexpr CVector UNSET_POSN{ -1000.0f, -1000.0f, -1000.0f };
        if (posn == UNSET_POSN || posn.IsZero()) {
            pos       = CVector{ 0.0f, 1.0f, 0.0f };
            bFrontEnd = true;
        } else {
            pos = posn;
        }
    }

    CAESound sound;
    sound.Initialise(bankSlot, (eSoundID)sfxId, this, pos, finalVolume, maxDistance, speed, 1.0f, 0, 0, 0.0f, 0);
    sound.SetFlags(SOUND_FRONT_END, bFrontEnd);
    if (physical) {
        sound.SetFlags(SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY, true);
        sound.RegisterWithPhysicalEntity(physical);
    }
    // Original stores the created sound at `this + 0x8` and sets the uint16 at `this + 0x5E` to 6.
    // NOTE: `+0x8` overlaps the first fields of `CAEAudioEntity::m_tempSound` - reproduced faithfully.
    const auto result = AESoundManager.RequestNewSound(&sound);
    *reinterpret_cast<CAESound**>(reinterpret_cast<uint8*>(this) + 0x8) = result;
    *reinterpret_cast<uint16*>(reinterpret_cast<uint8*>(this) + 0x5E) = 6;
}
// event eAudioEvents
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
    if (GetMissionAudioLoadingStatus(sampleId) != 1) {
        return;
    }

    // Work out the volume
    float volume;
    bool  bPlayerDucking{}; // Slots 0/1 get the ducking flags
    if (link.m_nAudioEvent == 0xFFFF) {
        volume = -100.0f;
    } else {
        volume = GetDefaultVolume((eAudioEvents)link.m_nAudioEvent);
        if (sampleId < 2) {
            bPlayerDucking = true;
            if (volume == StaticRef<float>(0x85F3C4)) { // -128.0f
                volume = 6.0f;
            }
        }
    }

    // Work out the position
    CVector pos;
    bool    bFrontEnd{};
    if (link.m_pEntity) {
        pos = link.m_pEntity->GetPosition();
    } else {
        constexpr CVector UNSET_POSN{ -1000.0f, -1000.0f, -1000.0f };
        if (link.m_vPosition == UNSET_POSN || link.m_vPosition.IsZero()) {
            pos       = CVector{ 0.0f, 1.0f, 0.0f };
            bFrontEnd = true;
        } else {
            pos = link.m_vPosition;
        }
    }

    CAESound sound;
    sound.Initialise((eSoundBankSlot)(SND_BANK_SLOT_MISSION1 + sampleId), (eSoundID)link.m_nBankSlotId, this, pos, volume, 2.0f, 1.0f, 1.0f, 0, 0, 0.0f, 0);
    sound.SetFlags(SOUND_FRONT_END, bFrontEnd);
    sound.SetFlags(SOUND_IS_DUCKABLE, bPlayerDucking);
    sound.SetFlags(SOUND_IS_COMPRESSABLE, bPlayerDucking);
    sound.SetFlags(SOUND_SMOOTH_DUCKING, bPlayerDucking);
    link.m_Sound = AESoundManager.RequestNewSound(&sound);
}
// 0x4EC190
void CAEScriptAudioEntity::PreloadMissionAudio(uint8 slotId, int32 sampleId) {
    if (slotId < MISSION_AUDIO_COUNT && IsMissionAudioSampleFinished(slotId)) {
        auto&         link = wavLinks[slotId];
        eSoundBankS32 bankID{};
        if (CAEAudioUtility::GetBankAndSoundFromScriptSlotAudioEvent((eAudioEvents)sampleId, bankID, link.m_nBankSlotId, slotId)) {
            link.m_nBankId = bankID;
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
// TODO: The switch below is a faithful translation, but the event-id values (the original
// uses raw ids in [0x3EF..0x4A6], named "AE_CAS*" upstream) were never mapped to the
// repo's eAudioEvents - the repo defines no such names. Before enabling, every one of the
// ~163 case labels must be verified against the raw decompilation (Ghidra
// FUN_004eccf0 / all_decompiles.json "CAEScriptAudioEntity.ProcessMissionAudioEvent").
// Until then the original function is kept (see the hook below).
void CAEScriptAudioEntity::ProcessMissionAudioEvent(eAudioEvents eventId, CVector& posn, CPhysical* physical, float volume, float speed) {
    return plugin::CallMethod<0x4ECCF0, CAEScriptAudioEntity*, eAudioEvents, CVector&, CPhysical*, float, float>(this, eventId, posn, physical, volume, speed);

    /*
    * Untested
    * Need to fix case's
    switch (eventId) {
    case AE_CAS4_NE:
    case AE_CAS4_NG:
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PURCHASE_WEAPON);
        break;
    case AE_CAS4_NF:
    case AE_CAS4_NH:
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_CAR_NO_CASH);
        break;
    case AE_CAS4_NJ:
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RACE_321);
        break;
    case AE_CAS4_OA:
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RACE_GO);
        break;
    case AE_CAS4_OB:
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PART_MISSION_COMPLETE);
        break;
    case AE_CAS4_OC:
        PlayMissionBankSound(eventId, posn, physical, 5, 3u);
        break;
    case AE_CAS4_OD:
        PlayMissionBankSound(eventId, posn, physical, 2, 3);
        break;
    case AE_CAS4_OE:
    case AE_CAS4_TB:
        PlayMissionBankSound(eventId, posn, physical, 3, 3u);
        break;
    case AE_CAS4_OF:
        AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack(AE_WEAPON_CHAINSAW_IDLE);
        break;
    case AE_CAS4_OG:
    case AE_CAS4_PA:
    case AE_CAS4_PK:
        AEAmbienceTrackManager.StopSpecialMissionAmbienceTrack();
        break;
    case AE_CAS4_OH:
        PlayMissionBankSound(eventId, posn, physical, 6, 3u);
        break;
    case AE_CAS4_OJ:
        PlayMissionBankSound(eventId, posn, physical, 0, 3);
        break;
    case AE_CAS4_OK:
    case AE_CAS4_PG:
        PlayMissionBankSound(eventId, posn, physical, 1, 3u);
        break;
    case AE_CAS4_OL:
        PlayMissionBankSound(eventId, posn, physical, 4, 3u, 1u);
        break;
    case AE_CAS4_OM:
        AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack(AE_WEAPON_FIRE_MINIGUN_AMMO);
        break;
    case AE_CAS4_PB:
        PlayMissionBankSound(eventId, posn, physical, 6, 3);
        break;
    case AE_CAS4_PD:
        PlayMissionBankSound(eventId, posn, physical, 4, 3u);
        break;
    case AE_CAS4_PE:
        PlayMissionBankSound(eventId, posn, physical, 2, 3u);
        break;
    case AE_CAS4_PF:
        PlayMissionBankSound(eventId, posn, physical, 5, 3);
        break;
    case AE_CAS4_PH:
        PlayMissionBankSound(eventId, posn, physical, 1, 3u, 0, 0.0f, 2.0f, 0.79f);
        break;
    case AE_CAS4_PJ:
        AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack(AE_THUNDER);
        break;
    case AE_CAS4_QA:
        PlayMissionBankSound(eventId, posn, physical, 3, 3, 1);
        break;
    case AE_CAS4_QB:
        PlayMissionBankSound(eventId, posn, physical, 2, 2u, 1u, 0.0f, 3.0f);
        break;
    case AE_CAS4_RA:
        PlayMissionBankSound(eventId, posn, physical, 1, 2u, 1u, 0.0f, 3.0f);
        break;
    case AE_CAS4_RB:
        PlayMissionBankSound(eventId, posn, physical, 3, 2u, 1u, 0.0f, 3.0f);
        PlayMissionBankSound(eventId, posn, physical, 0, 2u, 0, 0.0f, 3.0f, 1.0f);
        break;
    case AE_CAS4_RD:
        AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity(AE_CAS4_RB, this);
        PlayMissionBankSound(eventId, posn, physical, 4, 2u, 1u, 0.0f, 3.0f);
        break;
    case AE_CAS4_RE:
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_SELECT);
        break;
    case AE_CAS4_SA:
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_BACK);
        break;
    case AE_CAS4_SB:
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_ERROR);
        break;
    case AE_CAS4_TA:
        PlayMissionBankSound(eventId, posn, physical, 0, 3u);
        m_nLastTimeHornPlayed = CTimer::GetTimeInMS();
        field_7C = 0;
        break;
    case AE_CAS4_TC:
        if (physical)
            AESoundManager.CancelSoundsOfThisEventPlayingForThisEntityAndPhysical(AE_CAS5_AA, this, physical);
        else
            AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity(AE_CAS5_AA, this);
        PlayMissionBankSound(eventId, posn, physical, 2, 3u);
        break;
    case AE_CAS4_ND:
        PlayMissionBankSound(eventId, posn, physical, 10, 3u, 1u);
        break;
    case AE_CAS4_HC:
        PlayResidentSoundEvent(2, 39, 19, eventId, posn, physical, 0.0f, 1.0f, 70, 1.0f);
        break;
    case AE_CAS4_HD:
        if (AEAudioHardware.IsSoundBankLoaded(242u, 28)) {
            PlayMissionBankSound(eventId, posn, physical, 0, 2);
            break;
        }
        if (AEAudioHardware.IsSoundBankLoaded(345u, 28)) {
            PlayMissionBankSound(eventId, posn, physical, 3, 2u);
            break;
        }
        break;
    case AE_CAS4_JA:
        PlayMissionBankSound(eventId, posn, physical, 2, 2u);
        break;
    case AE_CAS4_JB:
        PlayMissionBankSound(eventId, posn, physical, 1, 2u);
        break;
    case AE_CAS4_JC: {
        auto sfxId = (int16)CAEAudioUtility::GetRandomNumberInRange(2, 6);
        PlayMissionBankSound(eventId, posn, physical, sfxId, 3u);
        break;
    }
    case AE_CAS4_JD:
        PlayMissionBankSound(eventId, posn, physical, 1, 3u);
        break;
    case AE_CAS4_KA:
        PlayMissionBankSound(eventId, posn, physical, 0, 3u);
        break;
    case AE_CAS4_KB:
        if (physical)
            AESoundManager.CancelSoundsOfThisEventPlayingForThisEntityAndPhysical(AE_CAS4_KA, this, physical);
        else
            AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity(AE_CAS4_KA, this);
        break;
    case AE_CAS4_KC:
        if (physical) {
            m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENING, physical->GetPosition(), 0.0f, 1.0f);
        } else {
            m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENING, posn, 0.0f, 1.0f);
        }
        break;
    case AE_CAS4_KD:
        if (physical) {
            m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENED, physical->GetPosition(), 0.0f, 1.0f);
        } else {
            m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENED, posn, 0.0f, 1.0f);
        }
        break;
    case AE_CAS4_KG:
        PlayResidentSoundEvent(5, 143, 65, eventId, posn, physical, 0.0f);
        break;
    case AE_CAS4_KJ:
        PlayMissionBankSound(eventId, posn, physical, 8, 3);
        break;
    case AE_CAS4_LA:
        PlayMissionBankSound(eventId, posn, physical, 0, 3u, 1u);
        m_nLastTimeHornPlayed = CTimer::GetTimeInMS();
        break;
    case AE_CAS4_LB:
        PlayMissionBankSound(eventId, posn, physical, 2, 3u, 1u);
        break;
    case AE_CAS4_LC:
        PlayMissionBankSound(eventId, posn, physical, 1, 3u, 1u);
        break;
    case AE_CAS4_LD:
        PlayMissionBankSound(eventId, posn, physical, 5, 3);
        break;
    case AE_CAS4_LE:
        PlayMissionBankSound(eventId, posn, physical, 4, 3u);
        break;
    case AE_CAS4_MA:
        PlayMissionBankSound(eventId, posn, physical, 3, 3u, 1u);
        break;
    case AE_CAS4_NA:
        PlayMissionBankSound(eventId, posn, physical, 7, 3, 1);
        break;
    case AE_CAS4_NB:
        PlayMissionBankSound(eventId, posn, physical, 6, 3u, 1u);
        break;
    case AE_CAS4_NC:
        PlayMissionBankSound(eventId, posn, physical, 9, 3u, 1u);
        break;
    case AE_CAS4_HB:
        PlayMissionBankSound(eventId, posn, physical, 0, 3u);
        break;
    case AE_CAS4_FA:
        if (AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(eventId, this))
            break;

        PlayMissionBankSound(eventId, posn, physical, 4, 3);
        break;
    case AE_CAS4_FC:
        PlayMissionBankSound(eventId, posn, physical, 10, 3u);
        break;
    case AE_CAS4_FE:
        PlayMissionBankSound(eventId, posn, physical, 8, 3);
        break;
    case AE_CAS4_FF: {
        auto sfxId = (int16)CAEAudioUtility::GetRandomNumberInRange(3, 5);
        PlayMissionBankSound(eventId, posn, physical, sfxId, 3u);
        break;
    }
    case AE_CAS4_FG:
        PlayMissionBankSound(eventId, posn, physical, 7, 3u);
        break;
    case AE_CAS4_FH:
        if (!AEAudioHardware.IsSoundBankLoaded(44u, 40)) {
            if (AESoundManager.AreSoundsPlayingInBankSlot(40))
                AESoundManager.CancelSoundsInBankSlot(40, 0);
            AEAudioHardware.LoadSoundBank(44, 40);
        }
        m_Entity = physical;
        field_7D = 1;
        break;
    case AE_CAS4_FJ:
        if (field_7D) {
            if (!AESoundManager.AreSoundsOfThisEventPlayingForThisEntityAndPhysical(eventId, this, physical))
                PlayResidentSoundEvent(40, 44, 1, eventId, posn, physical, 0.0f, 1.0f, 0, 2.5f);
        }
        break;
    case AE_CAS4_FK:
        if (field_7D) {
            if (physical)
                AESoundManager.CancelSoundsOfThisEventPlayingForThisEntityAndPhysical(AE_CAS4_FJ, this, physical);
            else
                AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity(AE_CAS4_FJ, this);
            PlayResidentSoundEvent(40, 44, 2, eventId, posn, physical, 0.0f, 1.0f, 0, 2.5f);
        }
        break;
    case AE_CAS4_FL:
        if (field_7D) {
            AESoundManager.CancelSoundsInBankSlot(40, 1);
            PlayResidentSoundEvent(40, 44, 3, eventId, posn, physical, 0.0f, 1.0f, 0, 2.5f);
            m_Entity = nullptr;
            field_7D = 0;
        }
        break;
    case AE_CAS4_HA:
        PlayMissionBankSound(eventId, posn, physical, 1, 3u);
        break;
    case AE_CAS4_EH:
        PlayMissionBankSound(eventId, posn, physical, 3, 3u);
        break;
    case AE_CAS4_EC:
        AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity(AE_CAS4_EB, this);
        break;
    case AE_CAS4_DC:
        CAEVehicleAudioEntity::EnableHelicoptors();
        break;
    case AE_CAS4_DB:
        CAEVehicleAudioEntity::DisableHelicoptors();
        break;
    case AE_CAS4_DD:
        PlayResidentSoundEvent(2, 39, 64, eventId, posn, physical, 0.0f, 0.79f, 35, 1.0f);
        break;
    case AE_CAS4_ED:
        PlayMissionBankSound(eventId, posn, physical, 0, 2);
        break;
    case AE_CAS4_EE:
        AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity(AE_CAS4_ED, this);
        break;
    case AE_CAS4_EF: {
        auto sfxId = (int16)CAEAudioUtility::GetRandomNumberInRange(20, 28);
        PlayResidentSoundEvent(2, 39, sfxId, eventId, posn, physical, 0.0f);
        break;
    }
    case AE_CAS4_EG: {
        auto sfxId = (int16)CAEAudioUtility::GetRandomNumberInRange(0, 2);
        PlayMissionBankSound(eventId, posn, physical, sfxId, 3u);
        break;
    }
    case AE_CAS5_AA:
        PlayMissionBankSound(eventId, posn, physical, 0, 3u);
        break;
    case AE_CAS5_AB:
        PlayMissionBankSound(eventId, posn, physical, 1, 3u);
        break;
    case AE_CAS5_BE:
        PlayResidentSoundEvent(2, 39, 65, eventId, posn, physical, 0.0f, 1.0f, 16, 1.0f);
        break;
    case AE_CAS5_CB:
        AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack(AE_WEAPON_FIRE);
        break;
    case AE_CAS5_CF:
        if (field_7E)
            PlayResidentSoundEvent(2, 39, 69, eventId, posn, physical, 0.0f, 0.94f, 33, 1.0f);
        else
            PlayResidentSoundEvent(2, 39, 70, eventId, posn, physical, 0.0f, 1.0f, 25, 1.0f);
        field_7E = (field_7E + 1) % 2;
        break;
    case AE_CAS5_DB:
        PlayMissionBankSound(eventId, posn, physical, 1, 2u);
        break;
    case AE_CAS5_CG:
        PlayMissionBankSound(eventId, posn, physical, 0, 2u);
        break;
    case AE_CAS6_CB:
        PlayMissionBankSound(eventId, posn, physical, 0, 2u);
        break;
    case AE_CAS5_DA:
        PlayMissionBankSound(eventId, posn, physical, 1, 2u);
        break;
    case AE_CAS5_EA:
        PlayMissionBankSound(eventId, posn, physical, 2, 2);
        break;
    case AE_CAS5_CE:
        if (field_7E)
            PlayResidentSoundEvent(2, 39, 69, eventId, posn, physical, 0.0f, 1.0f, 63, 1.0f);
        else
            PlayResidentSoundEvent(2, 39, 70, eventId, posn, physical, 0.0f, 1.0f, 62, 1.0f);
        field_7E = (field_7E + 1) % 2;
        break;
    case AE_CAS5_CC:
        AEAmbienceTrackManager.StopSpecialMissionAmbienceTrack();
        break;
    case AE_CAS6_AE:
    case AE_CAS6_FK:
    case AE_CAS6_GB:
    case AE_CAS6_HB:
        PlayMissionBankSound(eventId, posn, physical, 1, 2u);
        break;
    case AE_CAS6_AA:
        if (AEAudioHardware.IsSoundBankLoaded(167u, 28)) {
            PlayMissionBankSound(eventId, posn, physical, 3, 2u);
        } else if (AEAudioHardware.IsSoundBankLoaded(345u, 28)) {
            PlayMissionBankSound(eventId, posn, physical, 4, 2u);
        }
        break;
    case AE_CAS6_AB:
        PlayMissionBankSound(eventId, posn, physical, 1, 2u);
        PlayMissionBankSound(eventId, posn, physical, 0, 2u);
        break;
    case AE_CAS6_AC:
        AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity(AE_CAS6_AB, this);
        PlayMissionBankSound(eventId, posn, physical, 2, 2u);
        break;
    case AE_CAS6_AD:
    case AE_CAS6_FM:
    case AE_CAS6_FP:
        PlayMissionBankSound(eventId, posn, physical, 0, 2u);
        break;
    case AE_CAS6_BA:
        PlayMissionBankSound(eventId, posn, physical, 0, 3u, 1u, -18.0f, 2.0f, 1.0f);
        field_8C = 1.0f;
        break;
    case AE_CAS6_BB:
    case AE_CAS6_BH:
        field_8C = 2.0f;
        break;
    case AE_CAS6_BC:
    case AE_CAS6_JK:
        PlayMissionBankSound(eventId, posn, physical, 2, 3u);
        break;
    case AE_CAS6_BD:
        PlayMissionBankSound(eventId, posn, physical, 3, 3u);
        break;
    case AE_CAS6_BE:
        PlayMissionBankSound(eventId, posn, physical, 7, 3);
        break;
    case AE_CAS6_BF:
        PlayMissionBankSound(eventId, posn, physical, 1, 3u, 1u, -18.0, 2.0f, 1.0f);
        field_8C = 1.0;
        break;
    case AE_CAS6_BK:
        PlayMissionBankSound(eventId, posn, physical, 0, 3u);
        break;
    case AE_CAS6_BL:
    case AE_CAS6_JJ:
        PlayMissionBankSound(eventId, posn, physical, 1, 3);
        break;
    case AE_CAS6_BM:
        PlayMissionBankSound(eventId, posn, physical, 2, 3u, 1u);
        break;
    case AE_CAS6_BN:
    case AE_CAS6_KC:
        PlayMissionBankSound(eventId, posn, physical, 4, 3u);
        break;
    case AE_CAS6_CA:
        PlayMissionBankSound(eventId, posn, physical, 5, 3, 1);
        break;
    case AE_CAS6_DA:
        PlayMissionBankSound(eventId, posn, physical, 1, 2u);
        PlayMissionBankSound(eventId, posn, physical, 0, 2u);
        break;
    case AE_CAS6_DB:
        AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity(1126, this);
        PlayMissionBankSound(eventId, posn, physical, 2, 2u);
        break;
    case AE_CAS6_EA:
        PlayMissionBankSound(eventId, posn, physical, 2, 2u);
        PlayMissionBankSound(eventId, posn, physical, 0, 2u);
        break;
    case AE_CAS6_EB:
        if (physical)
            AESoundManager.CancelSoundsOfThisEventPlayingForThisEntityAndPhysical(AE_CAS6_EA, this, physical);
        else
            AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity(AE_CAS6_EA, this);
        PlayMissionBankSound(eventId, posn, physical, 3, 2u);
        break;
    case AE_CAS6_EC:
        PlayResidentSoundEvent(5, 143, 58, eventId, posn, physical, 0.0f);
        PlayResidentSoundEvent(5, 143, 40, eventId, posn, physical, 0.0f);
        break;
    case AE_CAS6_ED:
        PlayResidentSoundEvent(2, 39, 33, eventId, posn, physical, 0.0f);
        PlayResidentSoundEvent(2, 39, 50, eventId, posn, physical, 0.0f, 0.79f, 22, 1.0f);
        break;
    case AE_CAS6_EE:
        if (physical)
            AudioEngine.ReportWeaponEvent(AE_WEAPON_FIRE, WEAPON_CAMERA, physical);
        else
            PlayResidentSoundEvent(5, 143, 45, eventId, posn, 0, 0.0f);
        break;
    case AE_CAS6_EF:
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_BUY_CAR_MOD);
        break;
    case AE_CAS6_EG:
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_CAR_RESPRAY);
        break;
    case AE_CAS6_EH:
        PlayResidentSoundEvent(5, 143, 34, eventId, posn, physical, 5.0f);
        PlayResidentSoundEvent(5, 143, 40, eventId, posn, physical, 0.0f);
        break;
    case AE_CAS6_EJ:
        PlayResidentSoundEvent(5, 143, 82, eventId, posn, physical, -3.0f);
        break;
    case AE_CAS6_EK:
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PART_MISSION_COMPLETE, 0.0f, 1.12f);
        break;
    case AE_CAS6_EL:
    case AE_CAS6_FC:
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PART_MISSION_COMPLETE);
        break;
    case AE_CAS6_EM:
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PART_MISSION_COMPLETE, 0.0f, 1.26f);
        break;
    case AE_CAS6_EN:
    case AE_CAS6_FA: {
        auto random = (int16)CAEAudioUtility::GetRandomNumberInRange(20, 28);
        PlayResidentSoundEvent(2, 39, random, eventId, posn, physical, 0.0f);
        break;
    }
    case AE_CAS6_EO: {
        auto random = (int16)CAEAudioUtility::GetRandomNumberInRange(20, 28);
        PlayResidentSoundEvent(2, 39, random, eventId, posn, physical, 0.0f);
        PlayResidentSoundEvent(2, 39, 65, eventId, posn, physical, 0.0f);
        break;
    }
    case AE_CAS6_EP:
        AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack(AE_GARAGE_DOOR_OPENED);
        break;
    case AE_CAS6_EQ:
    case AE_CAS6_LA:
    case AE_CAS6_LC:
        AEAmbienceTrackManager.StopSpecialMissionAmbienceTrack();
        break;
    case AE_CAS6_ER:
        if (physical)
            AudioEngine.ReportWaterSplash(physical, -6.0f, 0);
        else
            AudioEngine.ReportWaterSplash(posn, -6.0f);
        break;
    case AE_CAS6_ES:
        PlayResidentSoundEvent(2, 39, 19, eventId, posn, physical, 0.0f, 1.0f, 65, 1.0f);
        break;
    case AE_CAS6_EU:
        if (!AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(eventId, this)) {
            PlayResidentSoundEvent(17, 74, 7, eventId, posn, physical, 0.0f);
            m_nLastTimeHornPlayed = CTimer::GetTimeInMS();
        }
        break;
    case AE_CAS6_FF:
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_MONEY);
        break;
    case AE_CAS6_FN:
        if (physical) {
            m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENING, physical->GetPosition(), 0.0f, 1.0f);
        } else {
            m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENING, posn, 0.0f, 1.0f);
        }
        break;
    case AE_CAS6_FO:
        if (physical) {
            m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENED, physical->GetPosition(), 0.0f, 1.0f);
        } else {
            m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENED, posn, 0.0f, 1.0f);
        }
        break;
    case AE_CAS6_GH:
        if (physical)
            AudioEngine.ReportWeaponEvent(AE_WEAPON_FIRE, WEAPON_M4, physical);
        break;
    case AE_CAS6_GJ:
        PlayMissionBankSound(eventId, posn, physical, 3, 3u);
        break;
    case AE_CAS6_GK:
        if (physical) {
            m_ExplosionAudio.AddAudioEvent(AE_EXPLOSION, physical->GetPosition(), 0.0f);
        } else {
            m_ExplosionAudio.AddAudioEvent(AE_EXPLOSION, posn, 0.0f);
        }
        break;
    case AE_CAS6_HC:
        if (physical)
            AESoundManager.CancelSoundsOfThisEventPlayingForThisEntityAndPhysical(AE_CAS6_HB, this, physical);
        else
            AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity(AE_CAS6_HB, this);
        break;
    case AE_CAS6_HD:
        PlayResidentSoundEvent(AE_FRONTEND_FIRE_FAIL_SNIPERRIFFLE, 0, 0, eventId, posn, physical, 0.0f);
        break;
    case AE_CAS6_JA: {
        CVector vec0 = {-1.0f, 0.0f, 0.0f};
        PlayMissionBankSound(eventId, vec0, nullptr, 0, 3u);
        CVector vec1 = {+1.0f, 0.0f, 0.0f};
        PlayMissionBankSound(eventId, vec1, nullptr, 1, 3u);
        break;
    }
    case AE_CAS6_JB:
        if (physical) {
            m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENING, physical->GetPosition(), 0.0f, 0.79f);
        } else {
            m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENING, posn, 0.0f, 0.79f);
        }
        break;
    case AE_CAS6_JC:
        if (physical) {
            m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENED, physical->GetPosition(), 0.0f, 1.0f);
        } else {
            m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENED, posn, 0.0f, 1.0f);
        }
        break;
    case AE_CAS6_JD:
    case AE_CAS6_KA:
        PlayMissionBankSound(eventId, posn, physical, 0, 3u, 1u);
        m_nLastTimeHornPlayed = CTimer::GetTimeInMS();
        break;
    case AE_CAS6_JE:
        PlayMissionBankSound(eventId, posn, physical, 0, 3u);
        break;
    case AE_CAS6_JF:
        PlayMissionBankSound(eventId, posn, physical, 1, 3u);
        break;
    case AE_CAS6_JG:
        PlayMissionBankSound(eventId, posn, physical, 6, 3u);
        break;
    case AE_CAS6_JL:
        PlayMissionBankSound(eventId, posn, physical, 5, 3u, 0, 0.0f, 2.0f, 0.67f);
        break;
    case AE_CAS6_JM:
        PlayMissionBankSound(eventId, posn, physical, 5, 3, 0, 0.0f, 2.0f, 0.79f);
        break;
    case AE_CAS6_JN:
        PlayMissionBankSound(eventId, posn, physical, 5, 3u);
        break;
    case AE_CAS6_JO:
        PlayMissionBankSound(eventId, posn, physical, 8, 3u, 1u, 0.0f, 2.0f, 0.38f);
        break;
    case AE_CAS6_KB:
        PlayMissionBankSound(eventId, posn, physical, 3, 3, 1);
        break;
    case AE_CAS6_KD:
        PlayMissionBankSound(eventId, posn, physical, 7, 3u);
        break;
    case AE_CAS6_KE:
        AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack(AE_BLAST_DOOR_SLIDE_START);
        break;
    case AE_CAS6_LB:
        AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack(AE_WEAPON_FIRE_PLANE);
        break;
    case AE_CAS6_LD:
        AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack(AE_RAIN_COLLISION);
        break;
    case AE_CAS9_AA:
    case AE_CAS9_AC:
        AEAmbienceTrackManager.StopSpecialMissionAmbienceTrack();
        break;
    case AE_CAS9_AB:
        AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack(AE_GARAGE_DOOR_CLOSING);
        break;
    case AE_CAS9_AD:
        if (CLocalisation::Blood() && physical && physical->GetIsTypePed()) {
            physical->AsPed()->GetAE().AddAudioEvent(AE_PED_CRUNCH, 0.0f, 1.0f, physical, 0, 0, 0);
        }
        break;
    case AE_CAS9_BA: {
        auto sfxId = (int16)CAEAudioUtility::GetRandomNumberInRange(78, 80);
        PlayResidentSoundEvent(5, 143, sfxId, eventId, posn, physical, 0.0f);
        break;
    }
    case AE_CRANE_WINCH_MOVE:
        if (!field_7D) {
            break;
        }
        if (!AESoundManager.AreSoundsOfThisEventPlayingForThisEntityAndPhysical(eventId, this, physical)) {
            PlayResidentSoundEvent(40, 44, 1, eventId, posn, physical, volume, speed, 0, 2.5f);
        }
        m_Speed = speed;
        m_Volume = GetDefaultVolume(AE_CRANE_WINCH_MOVE) + volume;
        m_nLastTimeHornPlayed = CTimer::GetTimeInMS();
        break;
    default:
        return;
    }
    */
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
void CAEScriptAudioEntity::UpdateParameters(CAESound* sound, int16 playTime) {
    plugin::CallMethod<0x4EC970, CAEScriptAudioEntity*, CAESound*, int16>(this, sound, playTime);
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
    RH_ScopedInstall(GetMissionAudioLoadingStatus, 0x4EBF60, { .reversed = false });
    RH_ScopedInstall(IsMissionAudioSampleFinished, 0x4EBFE0, { .reversed = false });
    RH_ScopedInstall(GetMissionAudioEvent, 0x4EC020);
    RH_ScopedInstall(ClearMissionAudio, 0x4EC040, { .reversed = false });
    RH_ScopedInstall(SetMissionAudioPosition, 0x4EC0C0, { .reversed = false });
    RH_ScopedInstall(AttachMissionAudioToPhysical, 0x4EC100, { .reversed = false });
    RH_ScopedInstall(PreloadMissionAudio, 0x4EC190, { .reversed = false });
    RH_ScopedInstall(PlayLoadedMissionAudio, 0x4EC270, { .reversed = false });
    RH_ScopedInstall(GetMissionAudioPosition, 0x4EC4D0, { .reversed = false });
    RH_ScopedInstall(PlayResidentSoundEvent, 0x4EC550);
    RH_ScopedInstall(PlayMissionBankSound, 0x4EC6D0, { .reversed = false });
    RH_ScopedInstall(ProcessMissionAudioEvent, 0x4ECCF0, { .reversed = false });
    RH_ScopedOverloadedInstall(ReportMissionAudioEvent, "1", 0x4EE960, void (CAEScriptAudioEntity::*)(eAudioEvents, CPhysical*, float, float));
    RH_ScopedOverloadedInstall(ReportMissionAudioEvent, "2", 0x4EE940, void (CAEScriptAudioEntity::*)(eAudioEvents, CVector&));
    RH_ScopedVMTInstall(UpdateParameters, 0x4EC970, { .reversed = false });
}

CAEScriptAudioEntity* CAEScriptAudioEntity::Constructor() {
    this->CAEScriptAudioEntity::CAEScriptAudioEntity();
    return this;
}
