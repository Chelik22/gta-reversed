#include "PedToPlayerConversations.h"

#include "Ragdoll/IKChainManager.h"
#include "Conversations.h"
#include "Tasks/TaskComplexSequence.h"
#include "Tasks/TaskTypes/TaskSimpleStandStill.h"
#include "Tasks/TaskTypes/TaskComplexKillPedOnFoot.h"

void CPedToPlayerConversations::InjectHooks() {
    RH_ScopedClass(CPedToPlayerConversations);
    RH_ScopedCategory("Conversations");
    RH_ScopedInstall(Clear, 0x43AAE0);
    RH_ScopedInstall(Update, 0x43B0F0);
    RH_ScopedInstall(EndConversation, 0x43AB10);
}

// 0x43AAE0
void CPedToPlayerConversations::Clear() {
    ZoneScoped;

    if (m_State != eP2pState::INACTIVE) {
        CAEPedSpeechAudioEntity::ReleasePlayerConversation();
        m_State = eP2pState::INACTIVE;
    }

    m_pPed                         = nullptr;
    m_TimeOfLastPlayerConversation = 0;
}

// —— CPedToPlayerConversations globals not present in the repo's header ——
static auto& m_CandidateIndex  = StaticRef<int32>(0x969A3C);     // rotating index into the ped pool
static auto& m_pPlayerVehicle  = StaticRef<CVehicle*>(0x969A40); // cached FindPlayerVehicle (RegisterReference'd)

// —— Float constants (unnamed in the repo) ——
static auto& CONV_MIN_DOT            = StaticRef<float>(0x858B50); // min. dot(forward, position difference)
static auto& CONV_MAX_DIST           = StaticRef<float>(0x858F48); // max. ped-player distance
static auto& CONV_MIN_VEH_DIST       = StaticRef<float>(0x858BA4); // max. 2D ped-playerVehicle distance
static auto& CONV_STAT_THRESH        = StaticRef<float>(0x858C80); // GetStatValue(STAT_HAIRDRESSING_BUDGET) <
static auto& CONV_MIN_VEH_HEALTH     = StaticRef<float>(0x8594DC); // min. player vehicle m_fHealth
static auto& CONV_WIND_THRESH_LO     = StaticRef<float>(0x858B1C); // CWeather::Wind >= -> LIKE_WEATHER
static auto& CONV_WIND_THRESH_HI     = StaticRef<float>(0x858B8C); // CWeather::Wind >  -> DISLIKE_WEATHER

// 0x43B0F0
void CPedToPlayerConversations::Update() {
    const auto playerPed = FindPlayerPed(-1); // 0x56E210

    if (const auto playerVeh = FindPlayerVehicle(-1, false)) { // 0x56E0D0
        m_pPlayerVehicle = playerVeh;
        playerVeh->RegisterReference(reinterpret_cast<CEntity**>(&m_pPlayerVehicle)); // 0x571B70
    }

    // "Ped starts a conversation with the player" - the tail shared by all successful openings (LAB_0043b99b);
    // inlined at each opening site in the original.
    const auto openConversation = [&](CPed* ped) {
        m_State = eP2pState::PEDHASOPENED;
        m_pPed  = ped;
        ped->RegisterReference(reinterpret_cast<CEntity**>(&m_pPed)); // 0x571B70(&DAT_009691C0)
        m_TimeOfLastPlayerConversation = CTimer::GetTimeInMS();
        m_StartTime                    = CTimer::GetTimeInMS();
        ped->DisablePedSpeech(false); // 0x5EFF60(0)
        g_ikChainMan.LookAt("Ped2Pl_Conversation",  ped,       playerPed, 100'000, BONE_HEAD, nullptr, false, 0.25f, 500, 8, false); // 0x618970
        g_ikChainMan.LookAt("Ped2Pl_ConversationP", playerPed, ped,       100'000, BONE_HEAD, nullptr, false, 0.25f, 500, 8, false);
    };

    // LAB_0043ba40: the conversation is gone (m_pPed == nullptr) - shut everything down and bail
    const auto conversationGone = [&]() {
        m_State = eP2pState::INACTIVE;
        CAEPedSpeechAudioEntity::ReleasePlayerConversation();
        if (g_ikChainMan.IsLooking(playerPed))
            g_ikChainMan.AbortLookAt(playerPed, 250);
    };

    switch (m_State) {
    case eP2pState::INACTIVE: {
        if (m_TimeOfLastPlayerConversation + 30'000 < CTimer::GetTimeInMS() && !FindPlayerVehicle(-1, false)) {
            // (dead FindPlayerPed(-1) call from the decompile omitted)
            if (playerPed->GetWantedLevel() == eWantedLevel::WANTED_CLEAN // 0x41BE60
             && !CConversations::IsConversationGoingOn()                  // 0x43AAC0
             && playerPed->PedIsReadyForConversation(true)) {             // 0x43ABA0 (this lost by Ghidra: playerPed)

                auto& pool = *GetPedPool(); // ms_pPedPool 0xB74490; +8 = capacity (GetSize/GetAt -- non-const)

                // Up to 8 random candidates out of the ped pool
                for (int attempts = 0; attempts < 8; attempts++) {
                    m_CandidateIndex++;
                    if ((int32)pool.GetSize() <= m_CandidateIndex) // original: signed comparison against pool+8
                        m_CandidateIndex = 0;

                    const auto ped = pool.GetAt(m_CandidateIndex); // 0x4082A0
                    if (!ped || !ped->IsCreatedBy(PED_GAME))       // byte +0x484 == 1 (m_nCreatedBy -- protected, Ped.h:547)
                        continue;

                    if (!ped->PedIsReadyForConversation(true))     // 0x43ABA0 (this lost by Ghidra: ped)
                        continue;

                    if (ped->bHasAScriptBrain)                     // bit 23 of dword +0x474
                        continue;

                    if (!ped->GetIsOnScreen())                     // 0x534540 (this lost by Ghidra: ped)
                        continue;

                    // —— Position/direction checks ——
                    const auto& pedPos    = ped->GetPosition();
                    const auto& playerPos = playerPed->GetPosition();

                    const CVector diff1 = playerPos - pedPos; // 0x40FE60 (a - b)
                    // FIX (verify wf5b): 0x41CCB0 = CEntity::GetForwardVector() - thiscall, this in ECX
                    //   (disasm 0x41CCB0: [ecx+0x14] = m_matrix -> copy of forward(+0x10); call sites
                    //   0x43B278: mov ecx,esi = ped; 0x43B2CD: mov ecx,ebp = playerPed).
                    if (DotProduct(ped->GetForwardVector(), diff1) > CONV_MIN_DOT) { // 0x40FDB0
                        const CVector diff2 = pedPos - playerPos; // 0x40FE60
                        if (DotProduct(playerPed->GetForwardVector(), diff2) > CONV_MIN_DOT) {
                            const auto playerCoors = FindPlayerCoors(-1); // 0x56E010
                            const auto dist = (pedPos - playerCoors).Magnitude(); // 0x4082C0 (this lost: local_54)
                            if (dist < CONV_MAX_DIST) {
                                // —— Topic choice ——
                                const auto type   = ped->m_nPedType; // +0x598
                                const bool isGang = type == PED_TYPE_GANG1 || (type > PED_TYPE_GANG2 && type < 0x11 /* <= PED_TYPE_GANG10 */);
                                m_Topic = CGeneral::GetRandomNumberInRange(isGang ? 8 : 0, isGang ? 10 : 7); // 0x407180

                                if (ped->GetSpeechAE().WillPedChatAboutTopic((int16)m_Topic)) { // vtable+0x14
                                    switch (m_Topic) {
                                    case 0: { // player's vehicle
                                        if (m_pPlayerVehicle) {
                                            const auto& vehPos = m_pPlayerVehicle->GetPosition();
                                            const auto  vx     = pedPos.x - vehPos.x;
                                            const auto  vy     = pedPos.y - vehPos.y;
                                            if (std::sqrt(vx * vx + vy * vy) < CONV_MIN_VEH_DIST) {
                                                // CVehicleModelInfo+0x4D byte = m_nVehicleClass
                                                const auto vehClass = reinterpret_cast<CVehicleModelInfo*>(
                                                    CModelInfo::ms_modelInfoPtrs[m_pPlayerVehicle->m_nModelIndex])->m_nVehicleClass;
                                                switch (vehClass) {
                                                case VEHICLE_CLASS_POORFAMILY:
                                                case VEHICLE_CLASS_WORKER:
                                                case VEHICLE_CLASS_BIG:
                                                case VEHICLE_CLASS_TAXI:
                                                    if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) { // 0x4E38C0
                                                        m_bPositiveOpening = false;
                                                        ped->Say(CTX_GLOBAL_CONV_DISL_CAR, 0, 1.0f, true); // 0x5EFFE0
                                                        openConversation(ped);
                                                        return;
                                                    }
                                                    break;
                                                case VEHICLE_CLASS_RICHFAMILY:
                                                case VEHICLE_CLASS_EXECUTIVE:
                                                case VEHICLE_CLASS_LEISUREBOAT:
                                                    if (CONV_MIN_VEH_HEALTH < m_pPlayerVehicle->m_fHealth // +0x4C0
                                                     && CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                                        m_bPositiveOpening = true;
                                                        ped->Say(CTX_GLOBAL_CONV_LIKE_CAR, 0, 1.0f, true);
                                                        openConversation(ped);
                                                        return;
                                                    }
                                                    break;
                                                }
                                            }
                                        }
                                        break;
                                    }
                                    case 1: { // clothes - by the CLOTHES_RESPECT stat (0x42); (int) = _ftol2 0x821B40
                                        const auto stat = (int)CStats::GetStatValue(STAT_CLOTHES_RESPECT); // 0x558E40
                                        if (stat < 501 /*0x1F5*/) {
                                            if (stat < 150 /*0x96*/ && CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                                m_bPositiveOpening = false;
                                                ped->Say(CTX_GLOBAL_CONV_DISL_CLOTHES, 0, 1.0f, true);
                                                openConversation(ped);
                                                return;
                                            }
                                        } else if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                            m_bPositiveOpening = true;
                                            ped->Say(CTX_GLOBAL_CONV_LIKE_CLOTHES, 0, 1.0f, true);
                                            openConversation(ped);
                                            return;
                                        }
                                        break;
                                    }
                                    case 2: { // hair
                                        const auto stat = (int)CStats::GetStatValue(STAT_CLOTHES_RESPECT);
                                        if (FindPlayerPed(-1)->GetClothesDesc()->HasVisibleNewHairCut(0)) { // 0x5A7970 (this lost)
                                            if (stat < 501) {
                                                if ((stat < 150
                                                  || CStats::GetStatValue(STAT_HAIRDRESSING_BUDGET) < CONV_STAT_THRESH) // 0x1F
                                                 && CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                                    m_bPositiveOpening = false;
                                                    ped->Say(CTX_GLOBAL_CONV_DISL_HAIR, 0, 1.0f, true);
                                                    openConversation(ped);
                                                    return;
                                                }
                                            } else if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                                m_bPositiveOpening = true;
                                                ped->Say(CTX_GLOBAL_CONV_LIKE_HAIR, 0, 1.0f, true);
                                                openConversation(ped);
                                                return;
                                            }
                                        }
                                        break;
                                    }
                                    case 3: { // body shape - by MUSCLE (0x17) and FAT (0x15)
                                        const auto muscle = (int)CStats::GetStatValue(STAT_MUSCLE);
                                        const auto fat    = (int)CStats::GetStatValue(STAT_FAT);
                                        if (muscle < 401 /*0x191*/ || fat > 249 /*0xF9*/) {
                                            if (muscle - fat < 200 && fat > 500 && CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                                m_bPositiveOpening = false;
                                                ped->Say(CTX_GLOBAL_CONV_DISL_PHYS, 0, 1.0f, true);
                                                openConversation(ped);
                                                return;
                                            }
                                        } else if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                            m_bPositiveOpening = true;
                                            ped->Say(CTX_GLOBAL_CONV_LIKE_PHYS, 0, 1.0f, true);
                                            openConversation(ped);
                                            return;
                                        }
                                        break;
                                    }
                                    case 4: { // shoes - by the CLOTHES_RESPECT stat
                                        const auto stat = (int)CStats::GetStatValue(STAT_CLOTHES_RESPECT);
                                        if (stat < 701 /*0x2BD*/) {
                                            if (stat < 300 && CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                                m_bPositiveOpening = false;
                                                ped->Say(CTX_GLOBAL_CONV_DISL_SHOES, 0, 1.0f, true);
                                                openConversation(ped);
                                                return;
                                            }
                                        } else if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                            m_bPositiveOpening = true;
                                            ped->Say(CTX_GLOBAL_CONV_LIKE_SHOES, 0, 1.0f, true);
                                            openConversation(ped);
                                            return;
                                        }
                                        break;
                                    }
                                    case 5: { // smell - by SEX_APPEAL (0x19)
                                        const auto stat = (int)CStats::GetStatValue(STAT_SEX_APPEAL);
                                        if (stat < 701) {
                                            if (stat < 300 && CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                                m_bPositiveOpening = false;
                                                ped->Say(CTX_GLOBAL_CONV_DISL_SMELL, 0, 1.0f, true);
                                                openConversation(ped);
                                                return;
                                            }
                                        } else if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                            m_bPositiveOpening = true;
                                            ped->Say(CTX_GLOBAL_CONV_LIKE_SMELL, 0, 1.0f, true);
                                            openConversation(ped);
                                            return;
                                        }
                                        break;
                                    }
                                    case 6: { // tattoos
                                        if (FindPlayerPed(-1)->GetClothesDesc()->HasVisibleTattoo()) { // 0x5A79D0 (this lost)
                                            const auto stat = CGeneral::GetRandomNumberInRange(0, 1000);
                                            if (stat < 701) {
                                                if (stat < 300 && CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                                    m_bPositiveOpening = false;
                                                    ped->Say(CTX_GLOBAL_CONV_DISL_TATTOO, 0, 1.0f, true);
                                                    openConversation(ped);
                                                    return;
                                                }
                                            } else if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                                m_bPositiveOpening = true;
                                                ped->Say(CTX_GLOBAL_CONV_LIKE_TATTOO, 0, 1.0f, true);
                                                openConversation(ped);
                                                return;
                                            }
                                        }
                                        break;
                                    }
                                    case 7: { // weather - by CWeather::Wind and time of day
                                        if (CONV_WIND_THRESH_LO <= CWeather::Wind) { // 0xC812F0
                                            if (CONV_WIND_THRESH_HI < CWeather::Wind && CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                                m_bPositiveOpening = false;
                                                ped->Say(CTX_GLOBAL_CONV_DISL_WEATHER, 0, 1.0f, true);
                                                openConversation(ped);
                                                return;
                                            }
                                        } else if (CClock::ms_nGameClockHours < 20 && 6 < CClock::ms_nGameClockHours // 0xB70153
                                                && CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                            m_bPositiveOpening = true;
                                            ped->Say(CTX_GLOBAL_CONV_LIKE_WEATHER, 0, 1.0f, true);
                                            openConversation(ped);
                                            return;
                                        }
                                        break;
                                    }
                                    case 8: // gang: "Where you from?"
                                        if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                            m_bPositiveOpening = true;
                                            ped->Say(CTX_GLOBAL_WHERE_YOU_FROM /*0xFB*/, 0, 1.0f, true);
                                            openConversation(ped);
                                            return;
                                        }
                                        break;
                                    case 9: // gang: GANGBANG
                                        if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                            m_bPositiveOpening = true;
                                            ped->Say(CTX_GLOBAL_GANGBANG /*0xFC*/, 0, 1.0f, true);
                                            openConversation(ped);
                                            return;
                                        }
                                        break;
                                    }

                                    // Topic didn't work out - a rare "eyeing" line
                                    if ((rand() & 0xFFF) == 3) {
                                        ped->Say(CTX_GLOBAL_EYEING_PLAYER /*0x58*/, 0, 1.0f); // overrideSilence = false
                                        return;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        break;
    }
    case eP2pState::PEDHASOPENED: {
        // "Ped attacks the player" sequence - inlined at 3 places in the original
        // (No on a negative opening, topic 8, topic 9).
        // The CTask::operator new (0x61A5A0) null-check of the original is omitted (see RunningScript.cpp:1265).
        const auto attackPlayer = [&]() {
            auto* const seq = new CTaskComplexSequence{};                            // operator new(0x40) + ctor 0x632BD0
            seq->AddTask(new CTaskSimpleStandStill(2000, false, false, 8.0f));       // operator new(0x20) + 0x62F310
            seq->AddTask(new CTaskComplexKillPedOnFoot(playerPed, -1, 0, 0, 0, 0));  // operator new(0x38) + 0x620E30

            CEventScriptCommand event(3, seq, false);   // 0x4B0A00
            m_pPed->GetEventGroup().Add(&event, false); // 0x4AB420 - Add clones the event
            event.~CEventScriptCommand();               // 0x4B0A50 - the original destroys the stack instance
        };

        if (!m_pPed) {
            conversationGone(); // LAB_0043ba40
            return;
        }

        if (m_StartTime + 4'000 < CTimer::GetTimeInMS()) {
            m_State = eP2pState::INACTIVE;
            CAEPedSpeechAudioEntity::ReleasePlayerConversation(); // 0x4E3960
            playerPed->EnablePedSpeech();                         // 0x5EFF70
            if (g_ikChainMan.IsLooking(playerPed))                // 0x6181A0
                g_ikChainMan.AbortLookAt(playerPed, 250);         // 0x618280 (0xFA)
            if (g_ikChainMan.IsLooking(m_pPed)) {
                g_ikChainMan.AbortLookAt(m_pPed, 250);
                // FIX (verify wf5b): used to be `if (!m_bPositiveOpening)` - inverted. In the decompile
                //   (case 1, timeout): `if (DAT_009691b1 != 0) Say(0x38)` - the "ignored" line
                //   plays on a POSITIVE opening (ped complimented, player didn't reply).
                if (m_bPositiveOpening)
                    m_pPed->Say(CTX_GLOBAL_CONV_IGNORED /*0x38*/, 0, 1.0f); // overrideSilence = false
            }
            return;
        }

        const auto pad = CPad::GetPad(0); // 0x53FB70
        if (pad->ConversationYesJustDown()) { // 0x5411D0
            // === PLAYER REPLIED "YES" ===
            if (!m_bPositiveOpening) {
                // Negative opening -> polite farewell -> WAITINGTOFINISH
                // 0x43A6A0 not reversed: by ped type -> "female" (voice line choice)
                const auto isFemale = plugin::CallAndReturn<int32, 0x43A6A0>(m_pPed->m_nPedType);
                m_pPed->Say(isFemale ? CTX_GLOBAL_LIKE_DISMISS_FEMALE /*0x83*/ : CTX_GLOBAL_LIKE_DISMISS_MALE /*0x84*/, 0, 1.0f, true);
                m_State     = eP2pState::WAITINGTOFINISH;
                m_StartTime = CTimer::GetTimeInMS();
                return;
            }

            eGlobalSpeechContext reply{};
            switch (m_Topic) {
            case 0: reply = CTX_GLOBAL_LIKE_CAR_REPLY;     break; // 0x81
            case 1: reply = CTX_GLOBAL_LIKE_CLOTHES_REPLY; break; // 0x82
            case 2: reply = CTX_GLOBAL_LIKE_HAIR_REPLY;    break; // 0x86
            case 3: reply = CTX_GLOBAL_LIKE_PHYS_REPLY;    break; // 0x89
            case 4: reply = CTX_GLOBAL_LIKE_SHOES_REPLY;   break; // 0x8A
            case 5: reply = CTX_GLOBAL_LIKE_SMELL_REPLY;   break; // 0x8B
            case 6: reply = CTX_GLOBAL_LIKE_TATTOO_REPLY;  break; // 0x8C
            case 7: reply = CTX_GLOBAL_WEATHER_LIKE_REPLY; break; // 0xEA
            case 8:
                m_pPed->Say(CTX_GLOBAL_WHERE_YOU_FROM_POS /*0xEC*/, 0, 1.0f, true);
                if (CGeneral::GetRandomNumberInRange(0, 100) <= 74 /*0x4A*/)
                    attackPlayer();
                EndConversation(); // LAB_0043c0eb (0x43AB10)
                return;
            case 9:
                m_pPed->Say(CTX_GLOBAL_GANGBANG_YES /*0x6B*/, 0, 1.0f, true);
                if (CGeneral::GetRandomNumberInRange(0, 100) <= 74)
                    attackPlayer();
                EndConversation();
                return;
            default: // in the decompile - straight to state=3 without a line
                m_State     = eP2pState::WAITINGTOFINISH;
                m_StartTime = CTimer::GetTimeInMS();
                return;
            }
            m_pPed->Say(reply, 0, 1.0f, true);
            m_State     = eP2pState::WAITINGTOFINISH;
            m_StartTime = CTimer::GetTimeInMS();
            return;
        }

        if (!pad->ConversationNoJustDown()) // 0x541200
            return;

        // === PLAYER REPLIED "NO" ===
        if (!m_bPositiveOpening) {
            // Negative opening -> negative line, gang members may attack
            const auto isFemale = plugin::CallAndReturn<int32, 0x43A6A0>(m_pPed->m_nPedType);
            m_pPed->Say(isFemale ? CTX_GLOBAL_LIKE_NEGATIVE_FEMALE /*0x87*/ : CTX_GLOBAL_LIKE_NEGATIVE_MALE /*0x88*/, 0, 1.0f, true);
            if (m_pPed->m_nPedType == PED_TYPE_GANG2 /*8*/) {
                m_StartTime = CTimer::GetTimeInMS();
                m_State     = eP2pState::WAITINGTOFINISH;
                return;
            }
            if (CGeneral::GetRandomNumberInRange(0, 100) > 39 /*0x27*/) {
                m_StartTime = CTimer::GetTimeInMS();
                m_State     = eP2pState::WAITINGTOFINISH;
                return;
            }
            attackPlayer();
            m_StartTime = CTimer::GetTimeInMS();
            m_State     = eP2pState::WAITINGTOFINISH;
            return;
        }

        if (m_Topic == 8) {
            m_pPed->Say(CTX_GLOBAL_WHERE_YOU_FROM_NEG /*0xEB*/, 0, 1.0f, true);
        } else if (m_Topic != 9) {
            const auto isFemale = plugin::CallAndReturn<int32, 0x43A6A0>(m_pPed->m_nPedType);
            m_pPed->Say(isFemale ? CTX_GLOBAL_LIKE_DISMISS_FEMALE /*0x83*/ : CTX_GLOBAL_LIKE_DISMISS_MALE /*0x84*/, 0, 1.0f, true);
            m_StartTime = CTimer::GetTimeInMS();
            m_State     = eP2pState::WAITINGFORFINALWORD;
            return;
        } else {
            m_pPed->Say(CTX_GLOBAL_GANGBANG_NO /*0x6A*/, 0, 1.0f, true);
        }
        EndConversation(); // LAB_0043c0eb
        return;
    }
    case eP2pState::WAITINGFORFINALWORD:
        if (m_pPed) {
            if (CTimer::GetTimeInMS() <= m_StartTime + 3'000)
                return;
            if (!m_pPed->GetPedTalking()) { // 0x5EFF50 (this lost by Ghidra: m_pPed)
                m_pPed->Say(CTX_GLOBAL_LIKE_DISMISS_REPLY /*0x85*/, 0, 1.0f, true);
                m_StartTime = CTimer::GetTimeInMS();
                m_State     = eP2pState::WAITINGTOFINISH;
            }
            return;
        }
        conversationGone(); // LAB_0043ba40: the conversation is gone
        break;
    case eP2pState::WAITINGTOFINISH:
        if (CTimer::GetTimeInMS() <= m_StartTime + 2'500 /*0x9C4*/ && m_pPed)
            return;
        EndConversation(); // 0x43AB10
        break;
    }
}

// 0x43AB10
void CPedToPlayerConversations::EndConversation() {
    m_State           = eP2pState::INACTIVE;

    CAEPedSpeechAudioEntity::ReleasePlayerConversation();

    if (m_pPed) {
        m_pPed->EnablePedSpeech();
    }

    const auto player = FindPlayerPed(-1);
    if (g_ikChainMan.IsLooking(player)) {
        g_ikChainMan.AbortLookAt(player, 250);
    }

    if (m_pPed && g_ikChainMan.IsLooking(m_pPed)) {
        return g_ikChainMan.AbortLookAt(m_pPed, 250);
    }
}
