// Полная замена source/game_sa/Tasks/TaskTypes/TaskComplexGangJoinRespond.cpp
// Интеграция 5 VMT-функций: 0x662290 Clone, 0x661790 MakeAbortable, 0x6617A0 CreateNextSubTask,
// 0x6618D0 CreateFirstSubTask, 0x661950 ControlSubTask.
// Источники фрагментов: batch_out/w2-08-MonsterTruck/{0x661790,0x6617A0,0x662290}.cpp,
//                       batch_out/w2-09-TaskComplexCarSlowBeDraggedOutAnd/{0x6618D0,0x661950}.cpp
// Примечание: тела Clone (0x662290) и MakeAbortable (0x661790) УЖЕ инлайн в хедере
// (TaskComplexGangJoinRespond.h:20-21) — в .cpp их не дублируем (иначе redefinition);
// для них здесь только снят флаг {.Reversed = false} у хука.
// Хедер не меняется.

#include "StdInc.h"

#include "TaskComplexGangJoinRespond.h"
#include "TaskComplexGangLeader.h"                // CTaskComplexGangLeader::ShouldLoadGangAnims (ControlSubTask)
#include "TaskComplexTurnToFaceEntityOrCoord.h"   // CTaskComplexTurnToFaceEntityOrCoord (CreateFirstSubTask)
#include "TaskSimpleRunAnim.h"                    // CTaskSimpleRunAnim (CreateNextSubTask)
#include "TaskSimpleStandStill.h"                 // CTaskSimpleStandStill (CreateNextSubTask)

void CTaskComplexGangJoinRespond::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexGangJoinRespond, 0x86FB5C, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x6616F0);
    RH_ScopedInstall(Destructor, 0x661720);

    RH_ScopedVMTInstall(Clone, 0x662290);
    RH_ScopedVMTInstall(MakeAbortable, 0x661790);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x6617A0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x6618D0);
    RH_ScopedVMTInstall(ControlSubTask, 0x661950);
}

// 0x6616F0
CTaskComplexGangJoinRespond::CTaskComplexGangJoinRespond(bool response) :
    m_response{response}
{
}

// 0x661720
CTaskComplexGangJoinRespond::~CTaskComplexGangJoinRespond() {
    if (m_animsReferenced) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
        m_animsReferenced = false;
    }
}

// 0x6617A0
CTask* CTaskComplexGangJoinRespond::CreateNextSubTask(CPed* ped) {
    if (m_pSubTask->GetTaskType() != TASK_SIMPLE_ANIM) {
        if (!m_animsReferenced) {
            m_attempts++;
            if (m_attempts < 11) {
                return new CTaskSimpleStandStill(250, false, false, 8.0f);
            }
        } else if (m_response == 1) {
            return new CTaskSimpleRunAnim{ ANIM_GROUP_GANGS, ANIM_ID_INVITE_YES, 4.0f, false };
        } else {
            return new CTaskSimpleRunAnim{ ANIM_GROUP_GANGS, ANIM_ID_INVITE_NO, 4.0f, false };
        }
    }
    return nullptr;
}

// 0x6618D0
CTask* CTaskComplexGangJoinRespond::CreateFirstSubTask(CPed* ped) {
    m_attempts = 0;
    return new CTaskComplexTurnToFaceEntityOrCoord(FindPlayerPed(0), 0.5f /* DAT_0086FC7C */, 0.2f /* DAT_0086FC80 */);
}

// 0x661950
CTask* CTaskComplexGangJoinRespond::ControlSubTask(CPed* ped) {
    // Развернуть педа в сторону игрока
    const CVector diff = FindPlayerPed(-1)->GetPosition() - ped->GetPosition();
    // 0x59C910: оригинал нормализует копию (плоского) вектора; результат не используется
    CVector diffNormalised{ diff.x, diff.y, 0.0f };
    diffNormalised.Normalise();
    ped->m_fAimingRotation = CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(diff.x, diff.y, 0.0f, 0.0f));

    if (!m_animsReferenced) {
        if (CTaskComplexGangLeader::ShouldLoadGangAnims()) {
            const auto blockIdx = CAnimManager::GetAnimationBlockIndex("gangs");
            if (!CAnimManager::GetAnimBlocks()[blockIdx].IsLoaded) {
                CStreaming::RequestModel(IFPToModelId(blockIdx), STREAMING_KEEP_IN_MEMORY);
                return m_pSubTask;
            }
            CAnimManager::AddAnimBlockRef(blockIdx);
            m_animsReferenced = true;
            return m_pSubTask;
        }
    } else if (!CTaskComplexGangLeader::ShouldLoadGangAnims()) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
        m_animsReferenced = false;
    }
    return m_pSubTask;
}
