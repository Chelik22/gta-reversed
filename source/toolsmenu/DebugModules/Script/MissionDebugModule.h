#pragma once

#include "../DebugModule.h"
#include "toolsmenu/CDebugMenuToolInput.h"

class MissionDebugModule final : public DebugModule {
public:
    MissionDebugModule();

    void RenderWindow() override final;
    void RenderMenuEntry() override final;

    NOTSA_IMPLEMENT_DEBUG_MODULE_SERIALIZATION(MissionDebugModule, m_IsOpen, m_MissionIDToStart, m_SelectedMissionIdx, m_MissionToolInput);

public:
    //! NOTSA(test): local-only hook for the headless test rig (bisection).
    bool TestStartMissionPublic(int32 missionId) { return StartMission(missionId, true); }
    void TestRaiseStartFlag(int32 missionId) { m_ShouldStartMission = true; m_MissionIDToStart = missionId; m_IsOpen = true; }
private:
    bool StartMission(int32 missionId, bool bDoMissionCleanUp);

private:
    bool                m_IsOpen{};
    bool                m_ShouldStartMission{};
    int32               m_MissionIDToStart{};
    int32               m_SelectedMissionIdx{ -1 };
    CDebugMenuToolInput m_MissionToolInput{};
};
