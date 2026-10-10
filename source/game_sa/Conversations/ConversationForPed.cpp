#include "ConversationForPed.h"
#include "Conversations.h"

void CConversationForPed::InjectHooks() {
    RH_ScopedClass(CConversationForPed);
    RH_ScopedCategory("Conversations");
    RH_ScopedInstall(Update, 0x43C190);
    RH_ScopedInstall(IsPlayerInPositionForConversation, 0x43AC40);
}

void CConversationForPed::Clear(bool dontClearNodes) {
    if (!dontClearNodes) {
        CConversations::m_Nodes[m_FirstNode].ClearRecursively();
    }
    m_FirstNode                 = -1;
    m_CurrentNode               = -1;
    m_pPed                      = nullptr;
    m_LastChange                = 0;
    m_LastTimeWeWereCloseEnough = 0;
}

// 0x43C190
// Dialogue state machine (question -> answer -> ped speaks -> player input).
// Node table: CConversations::m_Nodes (0x969570, stride 0x18).
// Buttons: 0x541200 = ConversationNoJustDown (left), 0x5411D0 = ConversationYesJustDown (right).
void CConversationForPed::Update() {
    if (!m_pPed || !m_Enabled || !IsPlayerInPositionForConversation(false)) { // 0x43AC40
        m_Status = eStatus::INACTIVE;
        return;
    }

    const auto now = CTimer::GetTimeInMS(); // 0xB7CB84

    if (now <= m_LastTimeWeWereCloseEnough + 10000) {
        if (m_Status != eStatus::PLAYER_SPEAKING) {
            // 0x43C218 - wait for the ped's answer / advance to the next question
            if (m_LastChange + 400 < now
                && (m_Status == eStatus::PED_SPEAKING || m_Status == eStatus::WAITINGFORINPUT || m_Status == eStatus::INACTIVE)) {
                const auto pad  = CPad::GetPad(0);
                auto&      node = CConversations::m_Nodes[m_CurrentNode];
                if (!pad->ConversationNoJustDown() || node.m_NodeNo < 0) {
                    if (!pad->ConversationYesJustDown() || node.m_NodeYes < 0) { // 0x5411D0
                        goto end;
                    }
                    // 0x43C243 - YES: answer subtitle + player line
                    if (!m_SuppressSubtitles) {
                        CMessages::ClearSmallMessagesOnly(); // 0x69DD30
                        strcpy(gString, node.m_Name);
                        *reinterpret_cast<uint16*>(&gString[strlen(gString)]) = StaticRef<uint16>(0x8594E0); // 2-byte "Y" suffix
                        CMessages::AddMessageJump(TheText.Get(gString), 4000, 3, true);
                    }
                    if (node.m_SpeechY > 0) {
                        FindPlayerPed(-1)->Say(static_cast<eGlobalSpeechContext>(node.m_SpeechY), 0, 1.0f, false, false, false); // 0x5EFFE0
                    }
                    if (node.m_SpeechY < 0) {
                        CConversations::AwkwardSay(-node.m_SpeechY, FindPlayerPed(-1)); // 0x43A810
                    }
                    m_CurrentNode = node.m_NodeYes;
                    m_LastChange  = now;
                } else {
                    // 0x43C285 - NO
                    if (!m_SuppressSubtitles) {
                        CMessages::ClearSmallMessagesOnly();
                        strcpy(gString, node.m_Name);
                        *reinterpret_cast<uint16*>(&gString[strlen(gString)]) = StaticRef<uint16>(0x8594E4); // 2-byte "N" suffix
                        CMessages::AddMessageJump(TheText.Get(gString), 4000, 3, true);
                    }
                    if (node.m_SpeechN > 0) {
                        FindPlayerPed(-1)->Say(static_cast<eGlobalSpeechContext>(node.m_SpeechN), 0, 1.0f, false, false, false);
                    }
                    if (node.m_SpeechN < 0) {
                        CConversations::AwkwardSay(-node.m_SpeechN, FindPlayerPed(-1));
                    }
                    m_CurrentNode = node.m_NodeNo;
                    m_LastChange  = now;
                }
                if (!m_SuppressSubtitles) {
                    CMessages::AddMessageQ(TheText.Get(CConversations::m_Nodes[m_CurrentNode].m_Name), 4000, 1, true); // 0x69F0B0
                }
                m_Status = eStatus::PLAYER_SPEAKING;
            }
            goto end;
        }

        // 0x43C1F0 - PLAYER_SPEAKING: wait up to 4s for input
        if (now <= m_LastChange + 4000) {
            const auto pad = CPad::GetPad(0);
            if (!pad->ConversationNoJustDown()) {
                if (!pad->ConversationYesJustDown()) {
                    goto end; // no input - keep waiting
                }
            }
        }
    }

    // 0x43C325 - (re-)speak the current node's question
    if (!m_SuppressSubtitles) {
        CMessages::ClearSmallMessagesOnly();
        CMessages::AddMessageJump(TheText.Get(CConversations::m_Nodes[m_CurrentNode].m_Name), 4000, 1, true); // 0x69F1E0
    }
    if (CConversations::m_Nodes[m_CurrentNode].m_Speech > 0) {
        FindPlayerPed(-1)->Say(static_cast<eGlobalSpeechContext>(CConversations::m_Nodes[m_CurrentNode].m_Speech), 0, 1.0f, false, false, false);
    }
    if (CConversations::m_Nodes[m_CurrentNode].m_Speech < 0) {
        CConversations::AwkwardSay(-CConversations::m_Nodes[m_CurrentNode].m_Speech, m_pPed); // OG: FindPlayerPed(-1) == param_1+8
    }
    m_Status     = eStatus::PED_SPEAKING;
    m_LastChange = now;

end:
    // 0x43C55D
    m_LastTimeWeWereCloseEnough = now;
    if (m_Status == eStatus::PED_SPEAKING && m_LastChange + 4000 < now) {
        m_Status = eStatus::WAITINGFORINPUT;
    }
}

// 0x43AC40
bool CConversationForPed::IsPlayerInPositionForConversation(bool randomConversation) {
    return plugin::CallMethodAndReturn<bool, 0x43AC40, CConversationForPed*, bool>(this, randomConversation);
}
