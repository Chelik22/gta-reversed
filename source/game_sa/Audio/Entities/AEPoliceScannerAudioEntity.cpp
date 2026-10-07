#include "StdInc.h"

#include "AEPoliceScannerAudioEntity.h"

#include "AEAudioHardware.h"
#include "AEAudioUtility.h"
#include "TheZones.h"
#include "World.h"

// 0x4E6E00
CAEPoliceScannerAudioEntity::~CAEPoliceScannerAudioEntity() {
    if (s_pPSControlling == this && s_nScannerPlaybackState != STATE_INITIAL) {
        s_bStoppingScanner = true;
        if (s_pSound) {
            s_pSound->StopSoundAndForget();
            s_pSound = nullptr;
        }
        FinishedPlayingScannerDialogue();
    }
}

// 0x5B9C30
void CAEPoliceScannerAudioEntity::StaticInitialise() {
    s_NextNewScannerDialogueTime = 0;
    s_bScannerDisabled           = false;
    s_nScannerPlaybackState      = STATE_INITIAL;
    s_pPSControlling             = nullptr;
    s_pCurrentSlots              = nullptr;
    s_fVolumeOffset              = 0.0f;
}

// 0x4E6E90
void CAEPoliceScannerAudioEntity::Reset() {
    StopScanner(true);
    FinishedPlayingScannerDialogue();
}

// 0x4E71E0
// 0x4E71E0
void CAEPoliceScannerAudioEntity::AddAudioEvent(eAudioEvents event, eCrimeType crimeType, const CVector& point) {
    // Scanner phrase tables (contiguous blob in the original .data)
    static auto& s_ReportPhrases    = StaticRef<int16[4]>(0x8C8160);      // random report intros
    static auto& s_CrimePhrases     = StaticRef<int16[0x17]>(0x8C8168);   // by eCrimeType (2..0x16)
    static auto& s_ZonePhraseNames  = StaticRef<int32[0xC2][2]>(0x8C8198); // zone display-name, first 8 bytes as 2 dwords
    static auto& s_ZonePhrases      = StaticRef<int16[0xC2]>(0x8C87A8);
    static auto& s_ZoneHasDirInfo   = StaticRef<uint8[0xC2]>(0x8C8930);
    static auto& s_VehClassPhrases  = StaticRef<int16[0x2E]>(0x8C89F8);
    static auto& s_VehClassSayColor = StaticRef<uint8[0x2E]>(0x8C8A54);
    static auto& s_ColorPhrases     = StaticRef<int16[0x7F]>(0x8C8A88);

    tScannerSlot first[NUM_POLICE_SCANNER_SLOTS]{}, second[NUM_POLICE_SCANNER_SLOTS]{};

    if (event != AE_CRIME_COMMITTED /* 0xA4 */) {
        return;
    }
    if (crimeType < 2 || crimeType > 0x16) {
        return;
    }

    first[0] = { (eSoundBank)0x96, (eSoundID)s_ReportPhrases[CAEAudioUtility::GetRandomNumberInRange(0, 3)] };
    first[1] = { (eSoundBank)0x97, (eSoundID)s_CrimePhrases[crimeType] };

    const auto zone = CTheZones::FindSmallestZoneForPosition(point, true);
    if (!zone) {
        return;
    }
    // Locate the zone phrase by display name (first 8 bytes); -1 entries are skipped
    const auto nameLo = *(const int32*)&zone->m_TextLabel[0];
    const auto nameHi = *(const int32*)&zone->m_TextLabel[4];
    auto zi = 0;
    while (nameLo != s_ZonePhraseNames[zi][0] || nameHi != s_ZonePhraseNames[zi][1] || s_ZonePhrases[zi] < 0) {
        if (++zi > 0xC1) {
            return;
        }
    }

    if (s_ZoneHasDirInfo[zi]) {
        const auto midX = (zone->m_fX2 - zone->m_fX1) * 0.5f + zone->m_fX1;
        const auto midY = (zone->m_fY2 - zone->m_fY1) * 0.5f + zone->m_fY1;
        const auto extX = (zone->m_fX2 - zone->m_fX1) * 0.25f;
        const auto extY = (zone->m_fY2 - zone->m_fY1) * 0.25f;

        auto ns = (int16)-1; // north/south phrase
        auto haveNS = false;
        if (point.y <= midY + extY) {
            if (point.y < midY - extY) {
                ns = 3; // south
                haveNS = true;
            }
        } else {
            ns = 2; // north
            haveNS = true;
        }
        if (haveNS) {
            first[2] = { (eSoundBank)0x95, (eSoundID)ns };
        }

        if (point.x <= midX + extX) {
            if (midX - extX <= point.x) {
                if (haveNS) {
                    goto ZonePhrase; // center band on X while off-center on Y: no E/W phrase
                }
                first[3] = { (eSoundBank)0x95, (eSoundID)0 };
            } else {
                first[3] = { (eSoundBank)0x95, (eSoundID)4 }; // west
            }
        } else {
            first[3] = { (eSoundBank)0x95, (eSoundID)1 }; // east
        }
    }

ZonePhrase:
    first[4] = { (eSoundBank)0x93, (eSoundID)s_ZonePhrases[zi] };

    // Which player owns this scanner, and their ped?
    CPed* player = nullptr;
    for (auto i = 0; i < 2; i++) {
        if (&FindPlayerWanted(i)->m_PoliceScannerAudioEntity == this) {
            player = FindPlayerPed(i);
        }
    }
    if (player && !player->bInVehicle) {
        const auto intel = player->GetIntelligence();
        if (!intel->GetTaskSwim()) {
            if (intel->GetTaskJetPack()) {
                goto Play; // no second-part phrases
            }
            second[4] = { (eSoundBank)0x96, (eSoundID)4 }; // on foot
        } else {
            second[4] = { (eSoundBank)0x96, (eSoundID)2 }; // swimming
        }
        second[0] = { (eSoundBank)0x96, (eSoundID)7 };
        goto Play;
    }
    if (player && player->m_pVehicle) {
        const auto vehicle = player->m_pVehicle;
        const auto vc = *(int8_t*)((char*)vehicle + 0x1D4); // vehicle class byte (0..0x2D)
        if (vc >= 0 && vc < 0x2E && s_VehClassPhrases[vc] >= 0) {
            second[0] = { (eSoundBank)0x96, (eSoundID)7 };
            second[1] = { (eSoundBank)0x96, (eSoundID)(
                vc == 0x19 || vc == 0x1D || vc == 0x1E || vc == 0x25 || vc == 0x29 || vc == 0x22 ? 3 : 1
            ) };
            if (s_VehClassSayColor[vc]) {
                if (vehicle->GetRemapIndex() == -1) {
                    const auto color = *(uint8_t*)((char*)vehicle + 0x434); // paintjob/remap byte
                    if (color <= 0x7E && s_ColorPhrases[color] >= 0) {
                        second[3] = { (eSoundBank)0x94, (eSoundID)s_ColorPhrases[color] };
                    }
                } else {
                    second[3] = { (eSoundBank)0x94, (eSoundID)4 };
                }
            }
            second[4] = { (eSoundBank)0x98, (eSoundID)s_VehClassPhrases[vc] };
        }
    }

Play:
    PlayPoliceScannerDialogue(first, second);
}

// 0x4E6BC0
void CAEPoliceScannerAudioEntity::PrepSlots() {
    for (auto i = 0; i < NUM_POLICE_SCANNER_SLOTS; i++) {
        s_SlotState[i] = s_pCurrentSlots[i].IsActive();
    }
}

// 0x4E6CD0
void CAEPoliceScannerAudioEntity::LoadSlots() {
    if (!s_pCurrentSlots) {
        return;
    }

    bool canPlay = true;
    for (auto i = 0; i < NUM_POLICE_SCANNER_SLOTS; i++) {
        const auto slot = (eSoundBankSlot)(SND_BANK_SLOT_SCANNER_FIRST + i);
        auto& currentSlot = s_pCurrentSlots[i];

        if (s_SlotState[i]) {
            if (s_SlotState[i] == 2) {
                bool loaded = AEAudioHardware.IsSoundLoaded(currentSlot.Bank, currentSlot.SoundID, slot);
                if (loaded) {
                    s_SlotState[i] = 3;
                } else {
                    canPlay = false;
                }
            }
        } else if (currentSlot.IsActive()) {
            s_SlotState[i] = 1;
        } else {
            if (!CStreaming::IsVeryBusy()) {
                AEAudioHardware.LoadSound(currentSlot.Bank, currentSlot.SoundID, slot);
                s_SlotState[i] = 2;
            }
            canPlay = false;
        }
    }
    if (canPlay) {
        s_nScannerPlaybackState = FOUR;
    }
}

// 0x4E6DB0
void CAEPoliceScannerAudioEntity::EnableScanner() {
    s_bScannerDisabled = false;
}

// 0x4E71B0
void CAEPoliceScannerAudioEntity::DisableScanner(bool a1, bool bStopSound) {
    s_bScannerDisabled = true;
    if (a1 && s_nScannerPlaybackState != STATE_INITIAL) {
        if (s_pPSControlling) {
            StopScanner(bStopSound);
        }
    }
}

// 0x4E6DC0
void CAEPoliceScannerAudioEntity::StopScanner(bool bStopSound) {
    if (s_nScannerPlaybackState == STATE_INITIAL)
        return;

    s_bStoppingScanner = true;
    if (bStopSound) {
        if (s_pSound) {
            s_pSound->StopSoundAndForget();
            s_pSound = nullptr;
        }
        FinishedPlayingScannerDialogue();
    }
}

// 0x4E6C30
void CAEPoliceScannerAudioEntity::FinishedPlayingScannerDialogue() {
    AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_SCANNER_NOISE_STOP);
    s_nScannerPlaybackState      = STATE_INITIAL;
    s_pPSControlling             = nullptr;
    s_pCurrentSlots              = nullptr;
    s_bStoppingScanner           = false;
    s_NextNewScannerDialogueTime = s_NextNewScannerDialogueTime + CTimer::GetTimeInMS();
    s_fVolumeOffset              = 0.0f;

    std::ranges::fill(s_SlotState, -1);
    s_SlotState[4] = 1;

    rng::fill(s_ScannerSlotFirst, tScannerSlot{});
    rng::fill(s_ScannerSlotSecond, tScannerSlot{});
}

// 0x4E6F60
void CAEPoliceScannerAudioEntity::PlayLoadedDialogue() {
    int16 i;
    for (i = 0; i < NUM_POLICE_SCANNER_SLOTS; ++i) {
        if (s_SlotState[i] == FIVE) {
            break;
        }
    }

    if (i == NUM_POLICE_SCANNER_SLOTS) {
        i = 0;
        goto LABEL_7;
    }

    if (i >= NUM_POLICE_SCANNER_SLOTS) {
    LABEL_9:
        auto volumeChange = s_fVolumeOffset /* + flt_B61D54 */;
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_SCANNER_CLICK, volumeChange);
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_SCANNER_NOISE_STOP);
        if (s_nSectionPlaying) {
            FinishedPlayingScannerDialogue();
        } else {
            auto v2 = (int16*)s_ScannerSlotSecond;
            s_pCurrentSlots = s_ScannerSlotSecond;
            s_nSectionPlaying = 1;
            auto v3 = s_SlotState;
            auto v4 = 5;
            do {
                *v3 = *v2 < 0 || v2[1] < 0;
                v2 += 2;
                ++v3;
                --v4;
            } while (v4);
            s_nPlaybackStartTime = 0;
            // s_nAbortPlaybackTime = *(_DWORD*)&gSpeechContextLookup[366][0] + CTimer::GetTimeInMS();
            s_nScannerPlaybackState = TWO;
        }
    } else {
    LABEL_7:
        while (s_SlotState[i] != 3) {
            if (++i >= 5)
                goto LABEL_9;
        }

        auto volume = GetDefaultVolume(AE_CRIME_COMMITTED) + s_fVolumeOffset;
        CAESound sound;
        sound.Initialise((eSoundBankSlot)(SND_BANK_SLOT_SCANNER_FIRST + i), s_pCurrentSlots[i].SoundID, this, { 0.0, 1.0f, 0.0f }, volume, 1.0f, 1.0f, 1.0f, 0, SOUND_DEFAULT, 0.0f, 0);
        sound.m_ClientVariable = (float)i;
        sound.m_Flags = SOUND_FRONT_END | SOUND_IS_CANCELLABLE | SOUND_REQUEST_UPDATES | SOUND_IS_DUCKABLE;
        sound.m_Event = AE_CRIME_COMMITTED;

        s_pSound = AESoundManager.RequestNewSound(&sound);
        if (s_pSound) {
            s_SlotState[i] = 5;
            s_nScannerPlaybackState = SEVEN;
        }
    }
}

// 0x4E6B60
void CAEPoliceScannerAudioEntity::PopulateScannerDialogueLists(const tScannerSlot* first, const tScannerSlot* second) {
    assert(first && second);
    if (s_nScannerPlaybackState == STATE_INITIAL) {
        for (auto slotIndex = 0; slotIndex < NUM_POLICE_SCANNER_SLOTS; slotIndex++) {
            s_ScannerSlotFirst[slotIndex] = first[slotIndex];
            s_ScannerSlotSecond[slotIndex] = second[slotIndex];
        }
    }
}

// inlined
// 0x4E6C00
bool CAEPoliceScannerAudioEntity::CanWePlayNewScannerDialogue() {
    if (s_nScannerPlaybackState == STATE_INITIAL)
        return false;

    if (CTimer::GetTimeInMS() < s_NextNewScannerDialogueTime)
        return false;

    if (TheCamera.m_bWideScreenOn)
        return false;

    if (s_bScannerDisabled)
        return false;

    return true;
}

// 0x4E6ED0
void CAEPoliceScannerAudioEntity::PlayPoliceScannerDialogue(tScannerSlot* first, tScannerSlot* second) {
    assert(first && second);
    if (CanWePlayNewScannerDialogue()) { // todo: maybe little bit wrong
        PopulateScannerDialogueLists(first, second);
        s_pPSControlling = this;
        s_nSectionPlaying = 0;
        s_pCurrentSlots = s_ScannerSlotFirst;
        PrepSlots();
        s_nScannerPlaybackState = TWO;
        s_nPlaybackStartTime = CTimer::GetTimeInMS() + 2000; // todo: gSpeechContextLookup[365][6]
        s_nAbortPlaybackTime = CTimer::GetTimeInMS() + 5000; // todo: gSpeechContextLookup[366][0]
    }
}

// 0x4E7590
void CAEPoliceScannerAudioEntity::UpdateParameters(CAESound* sound, int16 curPlayPos) {
    if (curPlayPos == -1) {
        s_pSound = nullptr;
        if (s_bStoppingScanner) {
            if (s_nScannerPlaybackState != STATE_INITIAL) {
                s_bStoppingScanner = true; // V1048 [CWE-1164] The 's_bStoppingScanner' variable was assigned the same value
                FinishedPlayingScannerDialogue();
            }
            return;
        }
        PlayLoadedDialogue();
        return;
    }

    if (s_bStoppingScanner) {
        sound->m_Volume = sound->m_Volume - 6.0f; // todo: *(float*)&gSpeechContextLookup[366][4]
        return;
    }

    if (sound->m_Length > 0 && curPlayPos > sound->m_Length - 40 && sound->m_BankSlot != 37) { // todo: -40 should be replaced with by gSpeechContextLookup[366][2]
        sound->SetFlags(eSoundEnvironment::SOUND_REQUEST_UPDATES, false);
        s_pSound = nullptr;
        PlayLoadedDialogue();
        return;
    }
}

// 0x4E7630
void CAEPoliceScannerAudioEntity::Service() {
    static constexpr uint32 startDelay = 300;   // 0x8C8154
    static constexpr float noiseVolume = -6.0f; // 0x8C8158
    static constexpr float clickVolume = +0.0f; // 0xB61D54

    bool finishPlaying;
    if (TheCamera.m_bWideScreenOn && s_nScannerPlaybackState != STATE_INITIAL) {
        finishPlaying = true;
        s_bStoppingScanner = true;
    } else {
        finishPlaying = s_bStoppingScanner;
    }

    switch (s_nScannerPlaybackState) {
    case TWO:
        if (CTimer::GetTimeInMS() > s_nAbortPlaybackTime || finishPlaying) {
            FinishedPlayingScannerDialogue();
            break;
        }

        LoadSlots();
        break;
    case FOUR:
        if (finishPlaying) {
            FinishedPlayingScannerDialogue();
            break;
        }

        if (CTimer::GetTimeInMS() >= s_nPlaybackStartTime) {
            s_fVolumeOffset = CAEVehicleAudioEntity::s_pVehicleAudioSettingsForRadio ? 0.0f : -8.0f; // todo: -8 is gSpeechContextLookup[367][2]
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_SCANNER_CLICK,       s_fVolumeOffset + clickVolume, 1.0f);
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_SCANNER_NOISE_START, s_fVolumeOffset + noiseVolume, 1.0f); // todo: noiseVolume is gSpeechContextLookup[367][0]
            s_nPlaybackStartTime = startDelay + CTimer::GetTimeInMS(); // todo: startDelay is gSpeechContextLookup[366][6]
            s_nScannerPlaybackState = FIVE;
        }
        break;
    case FIVE:
        if (finishPlaying) {
            FinishedPlayingScannerDialogue();
            break;
        }

        if (CTimer::GetTimeInMS() >= s_nPlaybackStartTime && s_pPSControlling) {
            s_nScannerPlaybackState = SEVEN;
            s_pPSControlling->PlayLoadedDialogue();
        }
        break;
    default:
        return;
    }
}

void CAEPoliceScannerAudioEntity::InjectHooks() {
    RH_ScopedVirtualClass(CAEPoliceScannerAudioEntity, 0x85F368, 1);
    RH_ScopedCategory("Audio/Entities");

    RH_ScopedInstall(Constructor, 0x56DA00);
    RH_ScopedInstall(Destructor, 0x4E6E00);
    RH_ScopedInstall(StaticInitialise, 0x5B9C30);
    RH_ScopedInstall(Reset, 0x4E6E90);
    RH_ScopedInstall(AddAudioEvent, 0x4E71E0);
    RH_ScopedInstall(PrepSlots, 0x4E6BC0);
    RH_ScopedInstall(LoadSlots, 0x4E6CD0);
    RH_ScopedInstall(EnableScanner, 0x4E6DB0);
    RH_ScopedInstall(DisableScanner, 0x4E71B0);
    RH_ScopedInstall(StopScanner, 0x4E6DC0);
    RH_ScopedInstall(FinishedPlayingScannerDialogue, 0x4E6C30);
    RH_ScopedInstall(PlayLoadedDialogue, 0x4E6F60);
    RH_ScopedInstall(PopulateScannerDialogueLists, 0x4E6B60);
    RH_ScopedInstall(CanWePlayNewScannerDialogue, 0x4E6C00);
    RH_ScopedInstall(PlayPoliceScannerDialogue, 0x4E6ED0);
    RH_ScopedVMTInstall(UpdateParameters, 0x4E7590);
    RH_ScopedInstall(Service, 0x4E7630);
}

CAEPoliceScannerAudioEntity* CAEPoliceScannerAudioEntity::Constructor() {
    this->CAEPoliceScannerAudioEntity::CAEPoliceScannerAudioEntity();
    return this;
}

CAEPoliceScannerAudioEntity* CAEPoliceScannerAudioEntity::Destructor() {
    this->CAEPoliceScannerAudioEntity::~CAEPoliceScannerAudioEntity();
    return this;
}
