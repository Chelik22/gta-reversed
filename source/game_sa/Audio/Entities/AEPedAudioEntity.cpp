#include "StdInc.h"

#include "AEPedAudioEntity.h"
#include "AEAudioHardware.h"
#include "AEAudioUtility.h"

void CAEPedAudioEntity::InjectHooks() {
    RH_ScopedClass(CAEPedAudioEntity);
    RH_ScopedCategory("Audio/Entities");

    //RH_ScopedInstall(Constructor, 0x5DE8D0, { .reversed = false });

    RH_ScopedInstall(Initialise, 0x4E0E80);
    RH_ScopedInstall(StaticInitialise, 0x5B98A0);
    RH_ScopedInstall(Terminate, 0x4E1360);
    RH_ScopedInstall(AddAudioEvent, 0x4E2BB0);
    RH_ScopedInstall(TurnOnJetPack, 0x4E28A0);
    RH_ScopedInstall(TurnOffJetPack, 0x4E2A70);
    RH_ScopedInstall(StopJetPackSound, 0x4E1120);
    RH_ScopedInstall(UpdateJetPack, 0x4E0EE0);
    RH_ScopedInstall(PlayWindRush, 0x4E1170);
    RH_ScopedInstall(UpdateParameters, 0x4E1180);
    RH_ScopedInstall(HandleFootstepEvent, 0x4E13A0);
    RH_ScopedInstall(HandleSkateEvent, 0x4E17E0);
    RH_ScopedInstall(HandleLandingEvent, 0x4E18E0);
    RH_ScopedInstall(HandlePedSwing, 0x4E1A40);
    RH_ScopedInstall(HandlePedHit, 0x4E1CC0);
    RH_ScopedInstall(HandlePedJacked, 0x4E2350);
    RH_ScopedInstall(HandleSwimSplash, 0x4E26A0);
    RH_ScopedInstall(HandleSwimWake, 0x4E2790);
    RH_ScopedInstall(PlayShirtFlap, 0x4E2A90);
    RH_ScopedInstall(Service, 0x4E2EE0, { .reversed = false });
}

// 0x5DE8D0
CAEPedAudioEntity::CAEPedAudioEntity() : CAEAudioEntity() {
    m_pPed = nullptr;
    m_bCanAddEvent = false;

    m_JetPackSound0 = nullptr;
    m_JetPackSound1 = nullptr;
    m_JetPackSound2 = nullptr;
}

// (CEntity* entity)
// 0x4E0E80
void CAEPedAudioEntity::Initialise(CPed* ped) {
    m_pPed = ped;
    m_nSfxId = 0;
    m_LastSwimWakeTriggerTimeMs = 0;

    m_bJetPackPlaying = false;
    m_JetPackSound0 = nullptr;
    m_JetPackSound1 = nullptr;
    m_fVolume1 = -100.0f;
    m_fVolume2 = -100.0f;

    field_150 = nullptr;
    field_154 = -100.0f;
    field_158 = -100.0f;
    m_bCanAddEvent = true;
}

// 0x5B98A0
void CAEPedAudioEntity::StaticInitialise() {
    AEAudioHardware.LoadSoundBank(SND_BANK_FEET_GENERIC, SND_BANK_SLOT_FOOTSTEPS_GENERIC);
    AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_SWIMMING, SND_BANK_SLOT_SWIMMING);
}

// 0x4E1360
void CAEPedAudioEntity::Terminate() {
    m_bCanAddEvent = false;
    m_pPed   = nullptr;
    StopJetPackSound();
    AESoundManager.CancelSoundsOwnedByAudioEntity(this, true);
    if (m_sTwinLoopSoundEntity.IsActive()) {
        m_sTwinLoopSoundEntity.StopSoundAndForget();
    }
}

// 0x4E2BB0
void CAEPedAudioEntity::AddAudioEvent(eAudioEvents event, float volume, float speed, CPhysical* ped, eSurfaceType surfaceId, int32 a7, uint32 maxVol) {
    if (!m_bCanAddEvent)
        return;

    if (!m_pPed)
        return;

    switch (event) {
    case AE_PED_FOOTSTEP_LEFT:
    case AE_PED_FOOTSTEP_RIGHT:
        HandleFootstepEvent(event, volume, speed, surfaceId);
        break;
    case AE_PED_SKATE_LEFT:
    case AE_PED_SKATE_RIGHT:
        HandleSkateEvent(event, volume, speed);
        break;
    case AE_PED_LAND_ON_FEET_AFTER_FALL:
    case AE_PED_COLLAPSE_AFTER_FALL:
        HandleLandingEvent(event);
        break;
    case AE_PED_SWING:
        HandlePedSwing(event, a7, maxVol);
        break;
    case AE_PED_HIT_HIGH:
    case AE_PED_HIT_LOW:
    case AE_PED_HIT_GROUND:
    case AE_PED_HIT_GROUND_KICK:
    case AE_PED_HIT_HIGH_UNARMED:
    case AE_PED_HIT_LOW_UNARMED:
    case AE_PED_HIT_MARTIAL_PUNCH:
    case AE_PED_HIT_MARTIAL_KICK:
        HandlePedHit(event, ped, surfaceId, volume, maxVol);
        break;
    case AE_PED_JACKED_CAR_PUNCH:
    case AE_PED_JACKED_CAR_HEAD_BANG:
    case AE_PED_JACKED_CAR_KICK:
    case AE_PED_JACKED_BIKE:
    case AE_PED_JACKED_DOZER:
        HandlePedJacked(event);
        break;
    case AE_PED_SWIM_STROKE_SPLASH:
    case AE_PED_SWIM_DIVE_SPLASH:
        HandleSwimSplash(event);
        break;
    case AE_PED_SWIM_WAKE:
        HandleSwimWake(event);
        break;
    case AE_PED_CRUNCH: {
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_COLLISIONS, SND_BANK_SLOT_COLLISIONS)) {
            break;
        }

        volume += GetDefaultVolume(AE_PED_CRUNCH);

        if (AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(AE_PED_CRUNCH, this)) {
            break;
        }
        AESoundManager.PlaySound({
            .BankSlotID        = SND_BANK_SLOT_COLLISIONS,
            .SoundID           = 29,
            .AudioEntity       = this,
            .Pos               = ped->GetPosition(),
            .Volume            = volume,
            .RollOffFactor     = 1.5f,
            .Speed             = speed,
            .Doppler           = 1.0f,
            .FrameDelay        = 0,
            .Flags             = SOUND_DEFAULT,
            .FrequencyVariance = 0.06f,
            .PlayTime          = 0,
            .EventID           = AE_PED_CRUNCH
        });

        if (AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(AE_PED_KNOCK_DOWN, this) != 0) {
            break;
        }
        AESoundManager.PlaySound({
            .BankSlotID        = SND_BANK_SLOT_COLLISIONS,
            .SoundID           = (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(47, 49)),
            .AudioEntity       = this,
            .Pos               = ped->GetPosition(),
            .Volume            = volume,
            .RollOffFactor     = 1.5f,
            .Speed             = speed,
            .Doppler           = 1.0f,
            .FrameDelay        = 0,
            .Flags             = SOUND_DEFAULT,
            .FrequencyVariance = 0.06f,
            .PlayTime          = 0,
            .EventID           = AE_PED_KNOCK_DOWN
        });
        break;
    }
    default:
        break;
    }
}

// 0x4E28A0
void CAEPedAudioEntity::TurnOnJetPack() {
    if (!m_pPed)
        return;

    if (m_bJetPackPlaying || m_JetPackSound1 || m_JetPackSound0 || m_JetPackSound2)
        return;

    m_fVolume1 = -100.0f;
    m_fVolume2 = -100.0f;
    m_fVolume3 = +2.000f;
    m_JetPackSoundSpeedMult = 0.400f;

    m_bJetPackPlaying = true;

    const auto PlayJetPackSound = [&](eSoundBankSlot slot, eSoundID soundID) {
        return AESoundManager.PlaySound({
            .BankSlotID  = slot,
            .SoundID     = soundID,
            .AudioEntity = this,
            .Pos         = m_pPed->GetPosition(),
            .Volume      = -100.f,
            .Flags       = SOUND_REQUEST_UPDATES,
        });
    };
    m_JetPackSound0 = PlayJetPackSound(SND_BANK_SLOT_VEHICLE_GEN, 26);
    m_JetPackSound1 = PlayJetPackSound(SND_BANK_SLOT_WEAPON_GEN, 10);
    m_JetPackSound2 = PlayJetPackSound(SND_BANK_SLOT_FRONTEND_GAME, 0);
}

// 0x4E2A70
void CAEPedAudioEntity::TurnOffJetPack() {
    StopJetPackSound();
    m_bJetPackPlaying = false;
}

// 0x4E1120
void CAEPedAudioEntity::StopJetPackSound() {
    if (m_JetPackSound0) {
        m_JetPackSound0->StopSoundAndForget();
        m_JetPackSound0 = nullptr;
    }

    if (m_JetPackSound1) {
        m_JetPackSound1->StopSoundAndForget();
        m_JetPackSound1 = nullptr;
    }

    if (m_JetPackSound2) {
        m_JetPackSound2->StopSoundAndForget();
        m_JetPackSound2 = nullptr;
    }
}

// 0x4E0EE0
void CAEPedAudioEntity::UpdateJetPack(float thrustFwd, float thrustAngle) {
    if (!m_bJetPackPlaying || !m_JetPackSound1 || !m_JetPackSound0 || !m_JetPackSound2) {
        return;
    }

    if (thrustFwd <= 0.5f) { // flying
        m_fVolume1 = std::max(m_fVolume1 - 5.0f, -100.0f);
        m_fVolume2 = std::min(m_fVolume2 + 6.0f, -17.0f);
        m_fVolume3 = std::max(m_fVolume3 - 0.3f, 2.0f);
        m_JetPackSoundSpeedMult = std::max(m_JetPackSoundSpeedMult - 0.031f, 0.4f);
    } else { // idle
        m_fVolume1 = std::min(m_fVolume1 + 15.0f, -15.0f);
        m_fVolume2 = std::max(m_fVolume2 - 7.1f, -100.0f);
        m_fVolume3 = std::min(m_fVolume3 + 0.3f, 11.0f);
        m_JetPackSoundSpeedMult = std::min(m_JetPackSoundSpeedMult + 0.031f, 0.71f);
    }

    const auto angle = std::sin(thrustAngle);
    const float speed = angle < 0.0f ? -angle : angle; // maybe wrong

    // 0.0f == 0xB61384 (uninitialized)
    m_JetPackSound0->m_Volume = m_fVolume1 + 0.0f;
    m_JetPackSound0->m_Speed = speed * -0.07f + 1.0f;

    m_JetPackSound1->m_Volume = m_fVolume2 + 0.0f;
    m_JetPackSound1->m_Speed = 0.56f;

    m_JetPackSound2->m_Volume = m_fVolume3 + 0.0f;
    m_JetPackSound2->m_Speed = (speed / 5.0f + 1.0f) * m_JetPackSoundSpeedMult;
}

// 0x4E1170
void CAEPedAudioEntity::PlayWindRush(float, float) {
    // NOP
}

// 0x4E1180
void CAEPedAudioEntity::UpdateParameters(CAESound* sound, int16 curPlayPos) {
    if (sound == m_JetPackSound1) {
        if (curPlayPos == -1) {
            m_JetPackSound1 = nullptr;
            return;
        }
        if (m_pPed) {
            sound->SetPosition(m_pPed->GetPosition());
        }
        return;
    }

    if (sound == m_JetPackSound0) {
        if (curPlayPos == -1) {
            m_JetPackSound0 = nullptr;
            return;
        }
        if (m_pPed) {
            sound->SetPosition(m_pPed->GetPosition());
        }
        return;
    }

    if (sound == m_JetPackSound2) {
        if (curPlayPos == -1) {
            m_JetPackSound2 = nullptr;
            return;
        }
        if (m_pPed) {
            sound->SetPosition(m_pPed->GetPosition());
        }
        return;
    }

    // shit
    if (sound == field_150) {
        if (curPlayPos == -1) {
            field_150 = nullptr;
        }
        return;
    }

    if (curPlayPos < 0)
        return;

    switch (sound->m_Event) {
    case AE_PED_SWING:
    case AE_PED_HIT_HIGH:
    case AE_PED_HIT_LOW:
    case AE_PED_HIT_GROUND:
    case AE_PED_HIT_GROUND_KICK:
    case AE_PED_HIT_HIGH_UNARMED:
    case AE_PED_HIT_LOW_UNARMED:
    case AE_PED_HIT_MARTIAL_PUNCH:
    case AE_PED_HIT_MARTIAL_KICK:
    case AE_PED_JACKED_CAR_PUNCH:
    case AE_PED_JACKED_CAR_HEAD_BANG:
    case AE_PED_JACKED_CAR_KICK:
    case AE_PED_JACKED_BIKE:
    case AE_PED_JACKED_DOZER:
        if (CTimer::GetTimeInMS() < (uint32)sound->m_ClientVariable)
            return;
        sound->m_Speed = 1.0f;
        return;
    case AE_PED_SWIM_WAKE: {
        const auto volume = GetDefaultVolume(AE_PED_SWIM_WAKE);

        if (CTimer::GetTimeInMS() <= m_LastSwimWakeTriggerTimeMs + 100) {
            if (sound->m_Volume >= volume) {
                return;
            }

            if (sound->m_Volume + 0.6f >= volume) {
                sound->m_Volume = volume;
                return;
            }
        } else {
            auto vol = volume - 20.0f;
            if (sound->m_Volume <= vol) {
                sound->StopSoundAndForget();
                m_LastSwimWakeTriggerTimeMs = 0;
                return;
            }
            sound->m_Volume = std::max(sound->m_Volume - 0.6f, vol);
        }

        return;
    }
    default:
        return;
    }
}

// 0x4E13A0
void CAEPedAudioEntity::HandleFootstepEvent(eAudioEvents event, float volume, float speed, eSurfaceType forcedSurfaceType) {
    volume += GetDefaultVolume(event);

    if (m_pPed->bIsInTheAir) {
        return;
    }

    const auto PlayFootstepSound = [&] (eSoundBank bank, eSoundBankSlot slot, eSoundID sfx, int16 playTime = 0) {
        if (AEAudioHardware.IsSoundBankLoaded(SND_BANK_FEET_GENERIC, slot)) {
            AESoundManager.PlaySound({
                .BankSlotID         = slot,
                .SoundID            = sfx,
                .AudioEntity        = this,
                .Pos                = m_pPed->GetPosition(),
                .Volume             = volume,
                .Speed              = speed,
                .Flags              = SOUND_START_PERCENTAGE,
                .FrequencyVariance  = 0.0588f,
                .PlayTime           = playTime,
                .RegisterWithEntity = m_pPed,
            });
        } else {
            AEAudioHardware.LoadSoundBank(bank, slot);
        }
    };

    const auto PlayRandomGenericFootstepSound = [&] () {
        PlayFootstepSound(SND_BANK_FEET_GENERIC, SND_BANK_SLOT_FOOTSTEPS_GENERIC, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(1, 5)));
    };

    if (FindPlayerPed(-1) != m_pPed) {
        PlayRandomGenericFootstepSound();
    } else {
        const auto TryPlayPlayerFootstepSound = [&] (eSoundBank bank, eSoundBankSlot slot, eSoundID sfx, bool needCancelSoundsInSlot = false, int16 playTime = 0) {
            if (AEAudioHardware.IsSoundBankLoaded(bank, slot)) {
                PlayFootstepSound(bank, slot, sfx);
                return true;
            } else {
                if (needCancelSoundsInSlot) {
                    if (AESoundManager.AreSoundsPlayingInBankSlot(slot) - 1 <= 1) {
                        AESoundManager.CancelSoundsInBankSlot(slot, false);
                    }
                }
                AEAudioHardware.LoadSoundBank(bank, slot);
                PlayRandomGenericFootstepSound();
                return false;
            }
        };
        if (g_surfaceInfos.IsAudioWater(forcedSurfaceType) || g_surfaceInfos.IsAudioWater(m_pPed->m_nContactSurface)) { // 0x4E1703
            TryPlayPlayerFootstepSound(SND_BANK_GENRL_SWIMMING, SND_BANK_SLOT_SWIMMING, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(0, 4)), false, 50);
        } else if (g_surfaceInfos.IsAudioConcrete(m_pPed->m_nContactSurface)) { // 0x4E145E
            PlayRandomGenericFootstepSound();
        } else if (g_surfaceInfos.IsAudioGrass(m_pPed->m_nContactSurface) || g_surfaceInfos.IsAudioLongGrass(m_pPed->m_nContactSurface)) { // 0x4E16BA
            TryPlayPlayerFootstepSound(SND_BANK_FEET_GRASS, SND_BANK_SLOT_FOOTSTEPS_PLAYER, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(0, 4)), true);
        } else if (g_surfaceInfos.IsAudioSand(m_pPed->m_nContactSurface)) { // 0x4E14B7
            TryPlayPlayerFootstepSound(SND_BANK_FEET_SAND, SND_BANK_SLOT_FOOTSTEPS_PLAYER, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(0, 3)), true);
        } else if (g_surfaceInfos.IsAudioGravel(m_pPed->m_nContactSurface)) { // 0x4E1531
            TryPlayPlayerFootstepSound(SND_BANK_FEET_GRAVEL, SND_BANK_SLOT_FOOTSTEPS_PLAYER, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(0, 4)), true);
        } else if (g_surfaceInfos.IsAudioWood(m_pPed->m_nContactSurface)) { // 0x4E157D
            TryPlayPlayerFootstepSound(SND_BANK_FEET_WOOD, SND_BANK_SLOT_FOOTSTEPS_PLAYER, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(0, 4)), true);
        } else if (g_surfaceInfos.IsAudioTile(m_pPed->m_nContactSurface)) { // 0x4E15DB
            TryPlayPlayerFootstepSound(SND_BANK_FEET_TILE, SND_BANK_SLOT_FOOTSTEPS_PLAYER, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(0, 4)), true);
        } else if (g_surfaceInfos.IsAudioMetal(m_pPed->m_nContactSurface)) { // 0x4E1636
            TryPlayPlayerFootstepSound(SND_BANK_FEET_METAL, SND_BANK_SLOT_FOOTSTEPS_PLAYER, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(0, 4)), true);
        } else { // 0x4E168F
            PlayRandomGenericFootstepSound();
        }
    }
}

// 0x4E17E0
void CAEPedAudioEntity::HandleSkateEvent(eAudioEvents event, float volume, float speed) {
    if (m_pPed->bIsInTheAir) {
        return;
    }
    if (!AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_FEET_GENERIC, SND_BANK_SLOT_FOOTSTEPS_GENERIC)) {
        return;
    }
    AESoundManager.PlaySound({
        .BankSlotID         = SND_BANK_SLOT_FOOTSTEPS_GENERIC,
        .SoundID            = (eSoundID)(event == AE_PED_SKATE_LEFT ? 7 : 8),
        .AudioEntity        = this,
        .Pos                = m_pPed->GetPosition(),
        .Volume             = GetDefaultVolume(event) + (float)(CAEAudioUtility::GetRandomNumberInRange(-3, 3)) + volume,
        .Speed              = speed,
        .RegisterWithEntity = m_pPed,
    });
}

// 0x4E18E0
void CAEPedAudioEntity::HandleLandingEvent(eAudioEvents event) {
    if (m_pPed->bIsInTheAir) {
        return;
    }

    const auto PlayLandingSound = [&](eSoundBankSlot slot, eSoundID soundID, float volume, int16 playPos) {
        AESoundManager.PlaySound({
            .BankSlotID = slot,
            .SoundID            = soundID,
            .AudioEntity        = this,
            .Pos                = m_pPed->GetPosition(),
            .Volume             = volume,
            .Flags              = SOUND_START_PERCENTAGE,
            .FrequencyVariance  = 0.0588f,
            .PlayTime           = playPos,
            .RegisterWithEntity = m_pPed
        });
    };
    const auto volume = GetDefaultVolume(event);
    if (g_surfaceInfos.IsAudioWater(m_pPed->m_nContactSurface)) {
        if (AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_GENRL_SWIMMING, SND_BANK_SLOT_SWIMMING)) {
            PlayLandingSound(
                SND_BANK_SLOT_SWIMMING,
                CAEAudioUtility::GetRandomNumberInRange<eSoundID>(SND_GENRL_SWIMMING_SWIM1, SND_GENRL_SWIMMING_SWIM5),
                std::max(0.f, volume),
                50
            );
        }
    } else {
        if (AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_FEET_GENERIC, SND_BANK_SLOT_FOOTSTEPS_GENERIC)) {
            PlayLandingSound(
                SND_BANK_SLOT_FOOTSTEPS_GENERIC,
                event == AE_PED_LAND_ON_FEET_AFTER_FALL ? 6 : 0,
                volume,
                0
            );
        }
    }
}

// 0x4E1A40
void CAEPedAudioEntity::HandlePedSwing(eAudioEvents event, int32 a3, uint32 volume) {
    const auto& weapon = m_pPed->m_aWeapons[m_pPed->m_nActiveWeaponSlot];

    if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_WEAPONS, SND_BANK_SLOT_WEAPON_GEN)) {
        if (AudioEngine.IsLoadingTuneActive()) {
            return;
        }
        AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_WEAPONS, SND_BANK_SLOT_WEAPON_GEN);
        return;
    }

    eSoundID sfxId{ (eSoundID)-1 };
    float    volAdd{ 0.0f };
    float    speed{ 1.0f };

    switch (weapon.m_Type) {
    case WEAPON_GOLFCLUB:
    case WEAPON_CANE:
        sfxId = 47; // SND_GENRL_WEAPONS(?): club/cane swing
        break;
    case WEAPON_NIGHTSTICK:
    case WEAPON_POOL_CUE:
        sfxId  = 86;
        volAdd = 0.84f;
        break;
    case WEAPON_KNIFE:
    case WEAPON_KATANA:
        sfxId  = 88;
        volAdd = -3.0f; // float @ 0x859024
        break;
    case WEAPON_BASEBALLBAT:
    case WEAPON_SHOVEL:
        sfxId = 86;
        speed = 0.67f;
        break;
    case WEAPON_DILDO1:
    case WEAPON_DILDO2:
    case WEAPON_VIBE1:
    case WEAPON_VIBE2:
        sfxId  = 86;
        volAdd = -6.0f; // float @ 0x85A5C0
        break;
    case WEAPON_ROCKET:
    case WEAPON_ROCKET_HS:
    case WEAPON_FREEFALL_BOMB:
    case WEAPON_SNIPERRIFLE:
    case WEAPON_RLAUNCHER:
        return; // no swing sound for these
    default:
        sfxId  = 87;
        volAdd = -6.0f; // float @ 0x85A5C0
        break;
    }

    float vol = GetDefaultVolume(event) + volAdd;

    switch (a3) {
    case 0x40:
    case 0x41:
    case 0x42:
        sfxId  = 87;
        vol    = -6.0f; // float @ 0x85A5C0 (default volume is NOT added here)
        speed  = 0.84f;
        break;
    case 0x43:
    case 0x44:
        sfxId  = 87;
        vol    = -2.0f; // float @ 0x858B18 (default volume is NOT added here)
        speed  = 0.84f;
        break;
    default:
        if ((int16)sfxId < 0)
            return;
        break;
    }

    m_tempSound.Initialise(
        SND_BANK_SLOT_WEAPON_GEN,
        sfxId,
        this,
        m_pPed->GetPosition(),
        vol,
        1.0f,
        speed,
        1.0f,
        0,
        SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY,
        0.04f,
        0
    );
    m_tempSound.RegisterWithPhysicalEntity(m_pPed);
    if (volume > 0) {
        m_tempSound.m_Event          = event;
        m_tempSound.m_ClientVariable = (float)(CTimer::GetTimeInMS() + volume);
        m_tempSound.m_Speed          = 0.0f;
        m_tempSound.m_SpeedVariance  = 0.0f;
        m_tempSound.SetFlags(SOUND_REQUEST_UPDATES, true);
    }
    AESoundManager.RequestNewSound(&m_tempSound);
}

// Per-surface ped-collision volumes (filled by `SurfaceInfos_c::LoadSurfaceAudioInfos`);
// stride 0x10, the first dword of each entry is the raw ped-collision volume value. Used by 0x4DA510.
static auto& s_PedCollisionSurfaceVolumes = StaticRef<int32*>(0x8AD914);

// 0x4DA510 - unreversed helper; converts a surface's ped-collision volume value to dB (20 * log10(value * 0.01))
static float SurfacePedCollisionVolumeToDB(uint8 surfaceId) {
    return std::log10f((float)s_PedCollisionSurfaceVolumes[surfaceId * 4] * 0.01f) * 20.0f;
}

// 0x4E1CC0
void CAEPedAudioEntity::HandlePedHit(eAudioEvents event, CPhysical* physical, uint8 surfaceId, float volume, uint32 maxVol) {
    const auto& weapon = m_pPed->m_aWeapons[m_pPed->m_nActiveWeaponSlot];

    if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_WEAPONS, SND_BANK_SLOT_WEAPON_GEN)) {
        if (AudioEngine.IsLoadingTuneActive()) {
            return;
        }
        AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_WEAPONS, SND_BANK_SLOT_WEAPON_GEN);
        return;
    }

    const float baseVol = GetDefaultVolume(event) + volume;

    // "Hit" sound (played with the weapon in hands), 1st sound
    eSoundID weaponSfx{};          // -1 = don't play
    float    hitVolAdd{ 0.0f };    // addend to `baseVol` for the hit sound
    float    speedVariance{ 0.06f };
    switch (weapon.m_Type) {
    case WEAPON_UNARMED:
    case WEAPON_NIGHTVISION:
    case WEAPON_INFRARED:
        weaponSfx = 58;
        break;
    case WEAPON_BRASSKNUCKLE:
    case WEAPON_GRENADE:
    case WEAPON_TEARGAS:
    case WEAPON_MOLOTOV:
    case WEAPON_PISTOL:
    case WEAPON_PISTOL_SILENCED:
    case WEAPON_DESERT_EAGLE:
    case WEAPON_SHOTGUN:
    case WEAPON_SAWNOFF_SHOTGUN:
    case WEAPON_SPAS12_SHOTGUN:
    case WEAPON_MICRO_UZI:
    case WEAPON_MP5:
    case WEAPON_AK47:
    case WEAPON_M4:
    case WEAPON_TEC9:
    case WEAPON_COUNTRYRIFLE:
    case WEAPON_SNIPERRIFLE:
    case WEAPON_FLAMETHROWER:
    case WEAPON_MINIGUN:
    case WEAPON_REMOTE_SATCHEL_CHARGE:
    case WEAPON_DETONATOR:
    case WEAPON_SPRAYCAN:
    case WEAPON_EXTINGUISHER:
    case WEAPON_CAMERA:
        weaponSfx = 44;
        break;
    case WEAPON_GOLFCLUB:
        weaponSfx = 48;
        break;
    case WEAPON_NIGHTSTICK:
        weaponSfx = 35;
        break;
    case WEAPON_KNIFE:
        weaponSfx      = 57;
        speedVariance  = 0.03f;
        break;
    case WEAPON_BASEBALLBAT:
        weaponSfx = 34;
        break;
    case WEAPON_SHOVEL:
        weaponSfx = 75;
        break;
    case WEAPON_POOL_CUE:
        weaponSfx = 67;
        break;
    case WEAPON_KATANA:
        weaponSfx = 56;
        break;
    case WEAPON_DILDO1:
    case WEAPON_DILDO2:
    case WEAPON_VIBE1:
    case WEAPON_VIBE2:
        weaponSfx = 50;
        break;
    case WEAPON_FLOWERS:
        weaponSfx     = 54;
        speedVariance = 0.07f;
        break;
    case WEAPON_CANE:
        weaponSfx = 46;
        break;
    default:
        weaponSfx = (eSoundID)-1;
        break;
    }

    // "Pain" sound, 2nd sound
    eSoundID painSfx{ (eSoundID)-1 }; // -1 = don't play
    float    painVolAdd{ -3.0f };     // addend to `baseVol` for the pain sound
    bool     playHitSound{ true };
    bool     playPainSound{ true };

    const auto PlayPainSoundOnly = [&] (eSoundID sfx, float volAdd) {
        playHitSound = false;
        painSfx      = sfx;
        painVolAdd   = volAdd;
    };

    if (physical && physical->GetType() == ENTITY_TYPE_PED) { // colliding entity is a ped
        switch (event) {
        case AE_PED_HIT_HIGH:
            switch (weapon.m_Type) {
            case WEAPON_BRASSKNUCKLE:                              painVolAdd = 1.0f;  painSfx = (eSoundID)CAEAudioUtility::GetRandomNumberInRange(40, 43); break;
            case WEAPON_GOLFCLUB:
            case WEAPON_NIGHTSTICK:                                painVolAdd = -15.0f; painSfx = (eSoundID)CAEAudioUtility::GetRandomNumberInRange(40, 43); break;
            case WEAPON_BASEBALLBAT:                               painVolAdd = -10.0f; painSfx = (eSoundID)CAEAudioUtility::GetRandomNumberInRange(40, 43); break;
            case WEAPON_KNIFE:
            case WEAPON_KATANA:                                    PlayPainSoundOnly(81, -9.0f); break;
            case WEAPON_CHAINSAW:
                if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_BULLET_HITS, SND_BANK_SLOT_BULLET_HITS)) {
                    return;
                }
                painSfx = (eSoundID)CAEAudioUtility::GetRandomNumberInRange(7, 9);
                painVolAdd = (float)(int8)painSfx - 3.0f - baseVol; // sic - the random sound id replaces `baseVol` in the volume calculation
                break;
            case WEAPON_FLOWERS:                                   playPainSound = false; break;
            default:                                               painVolAdd = -15.0f; painSfx = (eSoundID)CAEAudioUtility::GetRandomNumberInRange(40, 43); break;
            }
            break;
        case AE_PED_HIT_LOW:
            switch (weapon.m_Type) {
            case WEAPON_BRASSKNUCKLE:                              hitVolAdd = 1.0f;   painSfx = (eSoundID)CAEAudioUtility::GetRandomNumberInRange(36, 39); break;
            case WEAPON_GOLFCLUB:
            case WEAPON_NIGHTSTICK:                                hitVolAdd = -15.0f; painSfx = (eSoundID)CAEAudioUtility::GetRandomNumberInRange(36, 39); break;
            case WEAPON_BASEBALLBAT:                               hitVolAdd = -10.0f; painSfx = (eSoundID)CAEAudioUtility::GetRandomNumberInRange(36, 39); break;
            case WEAPON_KNIFE:
            case WEAPON_KATANA:                                    PlayPainSoundOnly(81, -9.0f); break;
            case WEAPON_CHAINSAW:
                if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_BULLET_HITS, SND_BANK_SLOT_BULLET_HITS)) {
                    return;
                }
                painSfx = (eSoundID)CAEAudioUtility::GetRandomNumberInRange(7, 9);
                painVolAdd = (float)(int8)painSfx - 3.0f - baseVol; // sic - the random sound id replaces `baseVol` in the volume calculation
                break;
            case WEAPON_FLOWERS:                                   playPainSound = false; break;
            default:                                               hitVolAdd = -15.0f; painSfx = (eSoundID)CAEAudioUtility::GetRandomNumberInRange(36, 39); break;
            }
            break;
        case AE_PED_HIT_GROUND:
            switch (weapon.m_Type) {
            case WEAPON_BRASSKNUCKLE:                              PlayPainSoundOnly(81, -9.0f); break;
            case WEAPON_GOLFCLUB:                                  hitVolAdd = -10.0f; painSfx = 82; painVolAdd = -6.0f; break;
            case WEAPON_NIGHTSTICK:
            case WEAPON_KNIFE:                                     hitVolAdd = -15.0f; painSfx = 82; painVolAdd = -6.0f; break;
            case WEAPON_BASEBALLBAT:                               PlayPainSoundOnly(81, -9.0f); break;
            case WEAPON_CHAINSAW:
                if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_BULLET_HITS, SND_BANK_SLOT_BULLET_HITS)) {
                    return;
                }
                painSfx = (eSoundID)CAEAudioUtility::GetRandomNumberInRange(7, 9);
                painVolAdd = (float)(int8)painSfx - 3.0f - baseVol; // sic - the random sound id replaces `baseVol` in the volume calculation
                break;
            case WEAPON_MOLOTOV:                                   playPainSound = false; break;
            default:                                               hitVolAdd = -15.0f; painSfx = 82; painVolAdd = -6.0f; break;
            }
            break;
        case AE_PED_HIT_GROUND_KICK:
            PlayPainSoundOnly(82, -6.0f);
            break;
        case AE_PED_HIT_HIGH_UNARMED:
        case AE_PED_HIT_MARTIAL_PUNCH:
            weaponSfx = 58;
            painSfx   = (eSoundID)CAEAudioUtility::GetRandomNumberInRange(40, 43);
            break;
        case AE_PED_HIT_LOW_UNARMED:
            weaponSfx = 58;
            painSfx   = (eSoundID)CAEAudioUtility::GetRandomNumberInRange(36, 39);
            break;
        case AE_PED_HIT_MARTIAL_KICK:
            playHitSound = false;
            painSfx      = (eSoundID)CAEAudioUtility::GetRandomNumberInRange(59, 62);
            break;
        default:
            playPainSound = false;
            break;
        }
    } else { // no physical entity
        switch (event) {
        case AE_PED_HIT_HIGH:
        case AE_PED_HIT_LOW:
        case AE_PED_HIT_GROUND:
            painVolAdd = SurfacePedCollisionVolumeToDB(surfaceId);
            AudioEngine.ReportCollision(m_pPed, physical, SURFACE_PED, (eSurfaceType)surfaceId, m_pPed->GetPosition(), nullptr, 1.0f, 1.0f, true, false);
            playPainSound = false;
            break;
        case AE_PED_HIT_GROUND_KICK:
        case AE_PED_HIT_MARTIAL_KICK:
            AudioEngine.ReportCollision(m_pPed, physical, SURFACE_PED, (eSurfaceType)surfaceId, m_pPed->GetPosition(), nullptr, 1.0f, 1.0f, true, false);
            return;
        case AE_PED_HIT_HIGH_UNARMED:
        case AE_PED_HIT_LOW_UNARMED:
        case AE_PED_HIT_MARTIAL_PUNCH:
            weaponSfx     = 58;
            painVolAdd    = SurfacePedCollisionVolumeToDB(surfaceId);
            AudioEngine.ReportCollision(m_pPed, physical, SURFACE_PED, (eSurfaceType)surfaceId, m_pPed->GetPosition(), nullptr, 1.0f, 1.0f, true, false);
            playPainSound = false;
            break;
        default:
            playPainSound = false;
            break;
        }
    }

    const auto PostNewSound = [&] (eSoundID sfx, float vol, float speedVariance_) {
        m_tempSound.Initialise(
            SND_BANK_SLOT_WEAPON_GEN,
            sfx,
            this,
            m_pPed->GetPosition(),
            vol,
            1.0f,
            1.0f,
            1.0f,
            0,
            SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY,
            speedVariance_,
            0
        );
        m_tempSound.RegisterWithPhysicalEntity(m_pPed);
        if (maxVol > 0) {
            m_tempSound.m_Event          = event;
            m_tempSound.m_ClientVariable = (float)(CTimer::GetTimeInMS() + maxVol);
            m_tempSound.m_Speed          = 0.0f;
            m_tempSound.m_SpeedVariance  = 0.0f;
            m_tempSound.SetFlags(SOUND_REQUEST_UPDATES, true);
        }
        AESoundManager.RequestNewSound(&m_tempSound);
    };

    // 1st (hit) sound
    if (playHitSound && (int16)weaponSfx >= 0) {
        PostNewSound(weaponSfx, baseVol + hitVolAdd, speedVariance);
    }

    // 2nd (pain) sound
    if (playPainSound && (int16)painSfx >= 0) {
        PostNewSound(painSfx, baseVol + painVolAdd, 0.06f);
    }
}

// 0x4E2350
void CAEPedAudioEntity::HandlePedJacked(eAudioEvents event) {
    if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_WEAPONS, SND_BANK_SLOT_WEAPON_GEN)) {
        if (AudioEngine.IsLoadingTuneActive()) {
            return;
        }
        AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_WEAPONS, SND_BANK_SLOT_WEAPON_GEN);
        return;
    }

    const float defVol = GetDefaultVolume(event);

    // 1st & 2nd sounds (punch/kick impact + random grunt) are only played for the events below
    bool     playFirstSound{};
    uint32   clientVarAdd{};   // addend for the 1st & 2nd sound's `m_ClientVariable`
    uint32   speechIdAdd{};    // addend for the 3rd sound's `m_ClientVariable`
    eSoundID firstSfx{};
    float    firstVol{};       // sic - the 1st sound's volume is a random value
    switch (event) {
    case AE_PED_JACKED_CAR_PUNCH:
        playFirstSound = true;
        firstSfx  = 58;
        firstVol  = (float)CAEAudioUtility::GetRandomNumberInRange(40, 43);
        clientVarAdd = 500;
        speechIdAdd  = 0x855;
        break;
    case AE_PED_JACKED_CAR_HEAD_BANG:
        playFirstSound = true;
        firstSfx  = 50;
        firstVol  = (float)CAEAudioUtility::GetRandomNumberInRange(36, 39);
        clientVarAdd = 0x3A5;
        speechIdAdd  = 0xA49;
        break;
    case AE_PED_JACKED_CAR_KICK:
        firstVol     = (float)CAEAudioUtility::GetRandomNumberInRange(40, 43);
        clientVarAdd = 900;
        speechIdAdd  = 0xAF0;
        break;
    case AE_PED_JACKED_BIKE:
        playFirstSound = true;
        firstSfx  = 58;
        firstVol  = (float)CAEAudioUtility::GetRandomNumberInRange(40, 43);
        clientVarAdd = 0x21;
        speechIdAdd  = 0x16E;
        break;
    case AE_PED_JACKED_DOZER:
        firstVol     = (float)CAEAudioUtility::GetRandomNumberInRange(36, 39);
        clientVarAdd = 0x10A;
        speechIdAdd  = 0x341;
        break;
    default: // unknown event - only the 3rd sound is played
        break;
    }

    const bool playSecondSound = playFirstSound || firstVol != 0.0f; // second sound is played for all 5 known events

    const auto PostNewSound = [&] (eSoundBankSlot bankSlot, eSoundID sfx, float vol, float clientVar) {
        m_tempSound.Initialise(bankSlot, sfx, this, m_pPed->GetPosition(), vol, 2.0f, 0.0f, 1.0f, 0, SOUND_REQUEST_UPDATES | SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY, 0.0f, 0);
        m_tempSound.RegisterWithPhysicalEntity(m_pPed);
        m_tempSound.m_Event          = event;
        m_tempSound.m_ClientVariable = clientVar;
        AESoundManager.RequestNewSound(&m_tempSound);
    };

    // 1st sound: punch / kick impact
    if (playFirstSound) {
        PostNewSound(SND_BANK_SLOT_WEAPON_GEN, firstSfx, firstVol, (float)(CTimer::GetTimeInMS() + clientVarAdd));
    }

    // 2nd sound: random grunt
    if (playSecondSound) {
        PostNewSound(SND_BANK_SLOT_WEAPON_GEN, (eSoundID)CAEAudioUtility::GetRandomNumberInRange(40, 43), defVol, (float)(CTimer::GetTimeInMS() + clientVarAdd));
    }

    // 3rd sound
    if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_FEET_GENERIC, SND_BANK_SLOT_FOOTSTEPS_GENERIC)) {
        AEAudioHardware.LoadSoundBank(SND_BANK_FEET_GENERIC, SND_BANK_SLOT_FOOTSTEPS_GENERIC);
        return;
    }
    PostNewSound(SND_BANK_SLOT_FOOTSTEPS_GENERIC, 0, defVol, (float)(int32)(event + speechIdAdd));
}

// 0x4E26A0
void CAEPedAudioEntity::HandleSwimSplash(eAudioEvents event) {
    if (!AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_GENRL_SWIMMING, SND_BANK_SLOT_SWIMMING)) {
        return;
    }
    m_nSfxId = std::max(0, m_nSfxId + 1);
    AESoundManager.PlaySound({
        .BankSlotID         = SND_BANK_SLOT_SWIMMING,
        .SoundID            = (eSoundID)(m_nSfxId),
        .AudioEntity        = this,
        .Pos                = m_pPed->GetPosition(),
        .Volume             = GetDefaultVolume(event),
        .Flags              = SOUND_PLAY_PHYSICALLY | SOUND_START_PERCENTAGE | SOUND_IS_DUCKABLE,
        .FrequencyVariance  = 0.0588f,
        .RegisterWithEntity = m_pPed,
    });
}

// 0x4E2790
void CAEPedAudioEntity::HandleSwimWake(eAudioEvents event) {
    if (!AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_GENRL_COLLISIONS, SND_BANK_SLOT_COLLISIONS, true)) {
        return;
    }
    if (!AESoundManager.AreSoundsOfThisEventPlayingForThisEntityAndPhysical(event, this, m_pPed)) {
        AESoundManager.PlaySound({
            .BankSlotID         = SND_BANK_SLOT_COLLISIONS,
            .SoundID            = 3,
            .AudioEntity        = this,
            .Pos                = m_pPed->GetPosition(),
            .Volume             = GetDefaultVolume(event) - 20.0f,
            .RollOffFactor      = 1.f,
            .Speed              = 0.75f,
            .Doppler            = 1.f,
            .FrameDelay         = 0,
            .Flags              = SOUND_REQUEST_UPDATES | SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY,
            .RegisterWithEntity = m_pPed,
            .EventID            = event,
        });
    }
    m_LastSwimWakeTriggerTimeMs = CTimer::GetTimeInMS();
}

// 0x4E2A90
void CAEPedAudioEntity::PlayShirtFlap(float volume, float speed) {
    if (m_sTwinLoopSoundEntity.IsActive()) {
        m_sTwinLoopSoundEntity.UpdateTwinLoopSound(m_pPed->GetPosition(), volume, speed);
    } else {
        m_sTwinLoopSoundEntity.Initialise(SND_BANK_SLOT_WEAPON_GEN, 19, 20, this, 200, 1000, -1, -1);
        m_sTwinLoopSoundEntity.PlayTwinLoopSound(m_pPed->GetPosition(), volume, speed, 2.0f, 1.0f, SOUND_DEFAULT);
    }
}

// 0x4E2EE0
// BLOCKER (not reversed yet): Updates the shirt-flap wind-rush twin loop sound (`PlayShirtFlap`).
// Outline of the original (see also sp2 decompilation):
//  1. Returns immediately if `m_pPed->IsPlayer()` (0x5DF8F0) is false.
//  2. bCanPlay = !(standing-on entity is a vehicle (type&7==2) or ped flags @+0x46C bit 0x100 set).
//  3. If `m_pPed->m_nMoveState? @+0x530 == 0x32` and ped's vehicle @+0x58C is of type 9 or 0
//     (or virtual [vtbl+0x9C]() returns true), uses the vehicle's move speed (@+0x44..0x4C magnitude,
//     negated if < float@0x858B50) to compute base volume
//     (`CAEAudioUtility::AudioLog10` * float@0x858BA4 + float@0x8AE650/0x8AE654 depending on veh+0x1B8==2).
//  4. If `m_pPed->m_pIntelligence->GetUsingParachute()` (0x6011B0), does the parachute/skydive
//     branch: anim-frame float lookups via 0x4D6870(ped frame, "FALL_SkyDive_accel"/"Para_decel").
//  5. Smooths `field_158` towards the computed volume and finally calls
//     `PlayShirtFlap(field_158, speedFactor)` (0x4E2A90) or `m_sTwinLoopSoundEntity.StopSoundAndForget()` (0x4F2960).
// Not translated because the data constants @0x8AE644-0x8AE6A4/0x858B50/0x858624 and several CPed/CVehicle
// offsets (+0x46C bit 0x100, +0x530, +0x58C, veh+0x1B8, veh+0x590 vtbl+0x9C) still need mapping to repo headers.
void CAEPedAudioEntity::Service() {
    plugin::CallMethod<0x4E2EE0, CAEPedAudioEntity*>(this);
}
