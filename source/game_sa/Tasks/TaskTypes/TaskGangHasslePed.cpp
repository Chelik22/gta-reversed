#include "SeekEntity/TaskComplexSeekEntityStandard.h"
#include "StdInc.h"

#include "TaskGangHasslePed.h"

// Batch w2-17-mixed: includes needed by the integrated bodies.
#include "TaskComplexGangLeader.h"         // CTaskComplexGangLeader::ShouldLoadGangAnims / DoGangAbuseSpeech (reversed in repo)
#include "TaskComplexKillPedOnFoot.h"      // CTaskComplexKillPedOnFoot (ControlSubTask)
#include "TaskSimpleRunAnim.h"             // CTaskSimpleRunAnim
#include "TaskComplexPlayHandSignalAnim.h" // CTaskComplexPlayHandSignalAnim
#include "Animation/AnimManager.h"         // CAnimManager (gangs anim block refs)
#include "Streaming.h"                     // CStreaming::RequestModel, IFPToModelId, STREAMING_KEEP_IN_MEMORY

void CTaskGangHasslePed::InjectHooks() {
    RH_ScopedVirtualClass(CTaskGangHasslePed, 0x86FA00, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x65FED0);
    RH_ScopedInstall(Destructor, 0x65FF60);

    RH_ScopedVMTInstall(CreateNextSubTask, 0x6642C0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x664380);
    RH_ScopedVMTInstall(ControlSubTask, 0x65FFE0);
}

// 0x65FED0
CTaskGangHasslePed::CTaskGangHasslePed(CPed* ped, int32 a3, int32 a4, int32 a5) : CTaskComplex() {
    m_nTime = 0;
    m_nSomeRandomShit = 0;
    m_bFirstSubTaskInitialised = 0;
    m_bRefreshTime = 0;
    dword10 = a3;
    m_RndMin = a4;
    m_Ped = ped;
    m_RndMax = a5;
    m_bAnimBlockRefAdded = false;
    CEntity::SafeRegisterRef(m_Ped);
}

// 0x65FF60
CTaskGangHasslePed::~CTaskGangHasslePed() {
    CEntity::SafeCleanUpRef(m_Ped);

    if (m_bAnimBlockRefAdded) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
        m_bAnimBlockRefAdded = false;
    }
}

// 0x6642C0 (batch w2-17-mixed, fragment 0x6642C0.cpp)
CTask* CTaskGangHasslePed::CreateNextSubTask(CPed* ped) {
    if (m_pSubTask->GetTaskType() == TASK_COMPLEX_KILL_PED_ON_FOOT) { // 1000
        return nullptr;
    }

    // rand()*0x858C7C + rand()*0x858C7C + 3.0f(0x858B3C) — inlined float `GetRandomNumberInRange`
    const auto radius = CGeneral::GetRandomNumberInRange(3.0f, 5.0f);

    // FUN_0046AC10 = CTaskComplexSeekEntityStandard ctor (operator new(0x4C))
    return new CTaskComplexSeekEntityStandard(
        m_Ped,
        999'999,
        1000,
        radius * StaticRef<float>(0x86FC2C), // множитель из .rdata — не именован в репо
        StaticRef<float>(0x86FC28),          // moveStateRadius
        StaticRef<float>(0x86FC30),          // followNodeThresholdHeightChange
        false,
        true
    );
}

// 0x664380 (batch w2-17-mixed, fragment 0x664380.cpp)
CTask* CTaskGangHasslePed::CreateFirstSubTask(CPed* ped) {
    if (!m_Ped) { // +0xC
        return nullptr;
    }

    // rand()*0x858C7C + rand()*0x858C7C + 3.0f(0x858B3C) — inlined float `GetRandomNumberInRange`
    const auto radius = CGeneral::GetRandomNumberInRange(3.0f, 5.0f);

    m_nTime = CTimer::GetTimeInMS(); // +0x20 = DAT_00B7CB84

    // Второй rand() из декомпила + CRT-округлитель 0x821B40 (_ftol2): та же пара констант 0x858C7C/0x858B3C,
    // т.е. ещё один GetRandomNumberInRange(3, 5); m_nSomeRandomShit(+0x24) = результат + m_RndMin(+0x14)
    m_nSomeRandomShit = CGeneral::GetRandomNumberInRange(3, 5) + m_RndMin;

    m_bFirstSubTaskInitialised = true; // +0x28

    return new CTaskComplexSeekEntityStandard(
        m_Ped,
        999'999,
        1000,
        radius * StaticRef<float>(0x86FC2C),
        StaticRef<float>(0x86FC28),
        StaticRef<float>(0x86FC30),
        false,
        true
    );
}

// 0x65FFE0 (batch w2-17-mixed, fragment 0x65FFE0.cpp)
CTask* CTaskGangHasslePed::ControlSubTask(CPed* ped) {
    if (!m_Ped) { // +0xC
        return nullptr;
    }

    if (m_bFirstSubTaskInitialised) { // +0x28
        if (m_bRefreshTime) {         // +0x29
            m_nTime = CTimer::GetTimeInMS(); // +0x20 = DAT_00B7CB84
            m_bRefreshTime = false;
        }

        if (static_cast<uint32>(m_nSomeRandomShit + m_nTime) <= CTimer::GetTimeInMS()) {
            if (dword10 == 2) { // +0x10
                if (m_pSubTask->GetTaskType() != TASK_COMPLEX_KILL_PED_ON_FOOT) {
                    return new CTaskComplexKillPedOnFoot(m_Ped, -1, 0, 0, 0, 1); // FUN_00620E30
                }
                // sub-task уже kill-ped-on-foot → проваливаемся к анимациям ниже
            } else if (dword10 != 1) {
                return nullptr;
            }
        }
    }

    // Держим аним-блок "gangs" загруженным (и зарефленным), пока пугаем пешехода
    if (!m_bAnimBlockRefAdded) { // +0x1C
        if (CTaskComplexGangLeader::ShouldLoadGangAnims()) { // 0x65E7F0
            const auto blkIdx = CAnimManager::GetAnimationBlockIndex("gangs"); // 0x4D3990
            if (CAnimManager::GetAnimationBlock("gangs")->IsLoaded) { // = ms_aAnimBlocks[blkIdx].IsLoaded (ms_aAnimBlocks is private - public getter used)
                CAnimManager::AddAnimBlockRef(blkIdx); // 0x4D3FB0
                m_bAnimBlockRefAdded = true;
            } else {
                CStreaming::RequestModel(IFPToModelId(blkIdx), STREAMING_KEEP_IN_MEMORY); // FUN_004087E0(idx + 0x63E7, 8)
            }
        }
    } else if (!CTaskComplexGangLeader::ShouldLoadGangAnims()) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs")); // 0x4D3FD0
        m_bAnimBlockRefAdded = false;
    }

    // Случайная ганг-ская анимация (разговор/жест) во вторичный слот partial-анимаций
    if (m_bAnimBlockRefAdded && !m_Ped->IsPlayingHandSignal() /*0x5E0460*/
        && !m_Ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_PARTIAL_ANIM) /*FUN_00681810(4)*/) {
        CTask* animTask = nullptr;
        switch (const auto rnd = CGeneral::GetRandomNumberInRange(0, 200) /*FUN_00407180*/) {
        case 51: // 0x33..0x37 — разговор (PRTIAL_GNGTLKA..H)
        case 52:
        case 53:
        case 54:
        case 55:
            animTask = new CTaskSimpleRunAnim(ANIM_GROUP_GANGS /*0x34*/, static_cast<AnimationId>(ANIM_ID_PRTIAL_GNGTLKA + CGeneral::GetRandomNumberInRange(0, 8)), 4.0f /*0x40800000*/, false); // FUN_0061A8B0
            break;
        case 100:
            animTask = new CTaskComplexPlayHandSignalAnim(ANIM_ID_UNDEFINED /*0xFFFFFFFF*/, 4.0f /*0x40800000*/); // FUN_0061B2B0
            break;
        default:
            break;
        }
        if (animTask) {
            m_Ped->GetTaskManager().SetTaskSecondary(animTask, TASK_SECONDARY_PARTIAL_ANIM); // FUN_00681B60(task, 4)
        }
    }

    if (dword10 == 0) {
        m_Ped->Say(CTX_GLOBAL_EYEING_PED, 0, 1.0f, false, false, false); // FUN_005EFFE0(0x56, 0, 0x3F800000, 0, 0, 0)
    } else if (dword10 == 1 || dword10 == 2) {
        CTaskComplexGangLeader::DoGangAbuseSpeech(ped, m_Ped); // 0x65E860
    }

    return m_pSubTask;
}
