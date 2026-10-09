#include "StdInc.h"

#include "TaskComplexSignalAtPed.h"

// Added for batch w2-16-mixed (3 reversed functions: 0x660B30, 0x660CC0, 0x660D80).
// Already covered by StdInc.h:
// General.h (CGeneral), Streaming.h/StreamingInfo.h (IFPToModelId, STREAMING_KEEP_IN_MEMORY),
// Animation/AnimManager.h, Entity/Ped/Ped.h (StopPlayingHandSignal, m_fAimingRotation),
// Enums/{eTaskType,AnimationEnums,eBoneTag}.h (BONE_HEAD, AssocGroupId, AnimationId)
#include "TaskComplexGangLeader.h"                          // CTaskComplexGangLeader::ShouldLoadGangAnims (CreateFirst/ControlSubTask)
#include "TaskComplexTurnToFaceEntityOrCoord.h"             // CTaskComplexTurnToFaceEntityOrCoord (CreateFirstSubTask)
#include "TaskSimpleStandStill.h"                           // CTaskSimpleStandStill (CreateNextSubTask)
#include "TaskSimpleDoHandSignal.h"                         // CTaskSimpleDoHandSignal (CreateNextSubTask)
#include "TaskSimpleRunAnim.h"                              // CTaskSimpleRunAnim (CreateNextSubTask)
#include "Ragdoll/IKChainManager.h"                         // g_ikChainMan (CreateFirstSubTask)

void CTaskComplexSignalAtPed::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexSignalAtPed, 0x86fa8c, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x660A30);
    RH_ScopedInstall(Destructor, 0x660AB0);

    RH_ScopedVMTInstall(Clone, 0x662140);
    RH_ScopedVMTInstall(GetTaskType, 0x660AA0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x660B30);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x660CC0);
    RH_ScopedVMTInstall(ControlSubTask, 0x660D80);
}

// 0x660A30
CTaskComplexSignalAtPed::CTaskComplexSignalAtPed(CPed* pedToSignalAt, int32 unused1, bool playAnimAtEnd) :
    m_pedToSignalAt{pedToSignalAt},
    m_playAnimAtEnd{playAnimAtEnd}
{
    CEntity::SafeRegisterRef(m_pedToSignalAt);
}

CTaskComplexSignalAtPed::CTaskComplexSignalAtPed(const CTaskComplexSignalAtPed& o) :
    CTaskComplexSignalAtPed{o.m_pedToSignalAt, o.m_initialPause, o.m_playAnimAtEnd}
{
}

// 0x660AB0
CTaskComplexSignalAtPed::~CTaskComplexSignalAtPed() {
    CEntity::SafeCleanUpRef(m_pedToSignalAt);
}

// 0x660B30
CTask* CTaskComplexSignalAtPed::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_TURN_TO_FACE_ENTITY: { // 0x398
        // Оригинал (0x660B61): (int)((rand() & 0xFFFF) * [0x858B14]=1/32768 * 2000.f) -> [0, 4000)
        const auto pause = m_initialPause != -1
            ? m_initialPause
            : CGeneral::GetRandomNumberInRange(0, 4000);
        return new CTaskSimpleStandStill(pause, false, false, 8.f);
    }
    case TASK_SIMPLE_STAND_STILL: // 0xCB
        return new CTaskSimpleDoHandSignal();
    case TASK_SIMPLE_DO_HAND_SIGNAL: // 0x4BE
        if (m_areAnimsReferenced && m_playAnimAtEnd) {
            // Оригинал (0x660C5A..0x660C7F): and eax, 0xFFFF; fild; fmul [0x858B14]=1/32768;
            // fmul [0x85B320]=-8.0f (значение прочитано из .rdata); call ftol2 0x821B40;
            // mov ecx, 0x117; sub ecx, eax.
            // Множитель отрицательный => итог ANIM_ID_PRTIAL_GNGTLKA + [0..15]
            // (GNGTLKA..GNGTLKH + DEALER/HNDSH-гритингсы), а НЕ 279 - [0..15].
            return new CTaskSimpleRunAnim(
                ANIM_GROUP_GANGS, // push 0x34 == 52 == ANIM_GROUP_GANGS
                (AnimationId)(ANIM_ID_PRTIAL_GNGTLKA + CGeneral::GetRandomNumberInRange(0, 16)),
                4.f, // push 0x40800000
                false
            );
        }
        break;
    }
    return nullptr;
}

// 0x660CC0
CTask* CTaskComplexSignalAtPed::CreateFirstSubTask(CPed* ped) {
    if (m_pedToSignalAt && CTaskComplexGangLeader::ShouldLoadGangAnims()) {
        ped->StopPlayingHandSignal();
        g_ikChainMan.LookAt("TaskSignalAtPed", ped, m_pedToSignalAt, 5000, BONE_HEAD /*== (eBoneTag32)5*/, nullptr, true, 0.15f, 500, 3, false);
        // 0.5f = DAT_0086FC7C (changeRateMult), 0.2f = DAT_0086FC80 (maxHeading) - значения прочитаны из .rdata gta_sa.exe
        return new CTaskComplexTurnToFaceEntityOrCoord(m_pedToSignalAt, 0.5f, 0.2f);
    }
    return nullptr;
}

// 0x660D80
CTask* CTaskComplexSignalAtPed::ControlSubTask(CPed* ped) {
    if (!m_areAnimsReferenced) {
        if (CTaskComplexGangLeader::ShouldLoadGangAnims()) {
            const auto blockIdx = CAnimManager::GetAnimationBlockIndex("gangs");
            if (CStreaming::IsModelLoaded(IFPToModelId(blockIdx))) { // (&DAT_00B5D4B0)[idx * 0x20] == LOADSTATE_LOADED(1)
                CAnimManager::AddAnimBlockRef(blockIdx);
                m_areAnimsReferenced = true;
            } else {
                CStreaming::RequestModel(IFPToModelId(blockIdx), STREAMING_KEEP_IN_MEMORY); // оригинал: add eax, 0x63E7; push 8
            }
        }
    } else {
        if (!CTaskComplexGangLeader::ShouldLoadGangAnims()) {
            CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
            m_areAnimsReferenced = false;
        }
    }

    if (m_pedToSignalAt) {
        // Оригинал (0x660E18..0x660E39): fld [m_pedToSignalAt->pos.x]; fsub [ped->pos.x] => dx = target - ped;
        // аналогично dy; затем push dy; push dx поверх двух push 0 (скратч) =>
        // GetRadianAngleBetweenPoints(dx, dy, 0.f, 0.f); результат - LimitRadianAngle - в [edi+0x55C]=m_fAimingRotation.
        const auto dx = m_pedToSignalAt->GetPosition().x - ped->GetPosition().x;
        const auto dy = m_pedToSignalAt->GetPosition().y - ped->GetPosition().y;
        ped->m_fAimingRotation = CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(dx, dy, 0.f, 0.f));
    }

    return m_pSubTask;
}
