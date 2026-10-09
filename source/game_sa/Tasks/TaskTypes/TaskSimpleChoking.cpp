#include "StdInc.h"

#include "TaskSimpleChoking.h"

// Batch w2-17-mixed: includes needed by the integrated bodies.
#include "Animation/AnimManager.h"         // CAnimManager::BlendAnimation
#include "Audio/Enums/PedSpeechContexts.h" // CTX_GLOBAL_PAIN_COUGH

// 0x6202C0
CTaskSimpleChoking::CTaskSimpleChoking(CPed* attacker, bool bIsTeargas) :
    m_pAttacker{attacker},
    m_bIsTeargas{bIsTeargas}
{
    CEntity::SafeRegisterRef(m_pAttacker);
    m_pAnim          = nullptr;
    m_bIsFinished    = false;
    m_nTimeRemaining = CGeneral::GetRandomNumberInRange(0u, 1000u);
    m_nTimeStarted   = CTimer::GetTimeInMS();
}

// 0x623220
CTaskSimpleChoking::CTaskSimpleChoking(const CTaskSimpleChoking& o) :
    CTaskSimpleChoking{o.m_pAttacker, o.m_bIsTeargas}
{
}

// Finish-callback of the choking anims (original static 0x620480 next to ProcessPed; name not in the repo)
static void ChokingFinishAnimCB(CAnimBlendAssociation* assoc, void* data) {
    const auto self = static_cast<CTaskSimpleChoking*>(data);
    self->m_bIsFinished = true;
    self->m_pAnim = nullptr;
}

// 0x6203F0 (batch w2-17-mixed, fragment 0x6203F0.cpp)
bool CTaskSimpleChoking::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    if (priority == ABORT_PRIORITY_URGENT || priority == ABORT_PRIORITY_IMMEDIATE) { // param_3 == 1 || == 2
        if (event) {
            if (event->GetEventPriority() < 57 /*0x39*/) {
                return false;
            }
        }
        if (m_pAnim) { // +0xC
            m_pAnim->m_BlendAmount = -4.0f;      // +0x1C = 0xC0800000
            m_pAnim->SetDefaultFinishCallback(); // 0x4CEBE0(&LAB_004D1390, 0)
            m_pAnim = nullptr;
        }
        m_bIsFinished = true; // +0x19
    } else if (m_pAnim) {     // ABORT_PRIORITY_LEISURE
        m_pAnim->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE; // *(byte*)(m_pAnim + 0x2E) |= 4
        m_pAnim->m_BlendAmount = -4.0f;
        m_pAnim->SetDefaultFinishCallback();
        m_pAnim = nullptr;
    }
    return true;
}

// 0x620490 (batch w2-17-mixed, fragment 0x620490.cpp)
bool CTaskSimpleChoking::ProcessPed(CPed* ped) {
    if (m_bIsFinished) { // +0x19
        return true;
    }

    // If the attacker is in front of us (and we're not the player and not coughing from teargas) - look at them
    if (m_pAttacker /*+0x8*/ && !ped->IsPlayer() /*0x5DF8F0*/ && !m_bIsTeargas /*+0x18*/) {
        const auto& attackerPos = m_pAttacker->GetPosition();
        const auto& pedPos      = ped->GetPosition();
        if (DotProduct(attackerPos - pedPos, ped->GetMatrix().GetForward()) > 0.0f /*0x858B50*/) {
            // fpatan(-(dx), dy) @ ped + 0x55C
            ped->m_fAimingRotation = static_cast<float>(std::atan2(-(attackerPos.x - pedPos.x), attackerPos.y - pedPos.y));
        }
    }

    // m_nTimeRemaining(+0x10) -= (uint)_ftol2(0x821B40) - decreased by the timestep, clamped to 0
    if (const auto dt = (uint32)CTimer::GetTimeStep(); dt < m_nTimeRemaining) {
        m_nTimeRemaining -= dt;
    } else {
        m_nTimeRemaining = 0;
    }

    if (!m_pAnim) {
        m_pAnim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_DEFAULT, ANIM_ID_GAS_CWR /*0x15*/, 4.0f /*0x40800000*/); // 0x4D4610
        m_pAnim->SetFinishCallback(ChokingFinishAnimCB, this); // 0x4CEBE0(&LAB_00620480, this)
        m_pAnim->SetSpeed(CGeneral::GetRandomNumberInRange(0.8f /*0x858F34*/, 1.1f /*0x858C84*/)); // rand()*0x858C7C*0x858C84 + 0x858F34
    } else if (m_nTimeRemaining == 0) {
        if (m_pAnim->m_AnimId == ANIM_ID_GAS_CWR) { // +0x2C
            m_pAnim->SetDefaultFinishCallback();    // 0x4CEBE0(&LAB_004D1390, 0)
            m_pAnim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_DEFAULT, ANIM_ID_IDLE_TIRED /*10*/, 4.0f);
            m_pAnim->SetFinishCallback(ChokingFinishAnimCB, this);
            const auto sinceStart = CTimer::GetTimeInMS() - m_nTimeStarted; // DAT_00B7CB84 - +0x14
            m_nTimeRemaining = sinceStart;
            if (const auto rnd = (uint32)CGeneral::GetRandomNumberInRange(8000, 12000); sinceStart >= rnd) {
                m_nTimeRemaining = rnd;
            }
        } else {
            m_pAnim->m_BlendAmount = -4.0f; // 0xC0800000
            m_pAnim->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE; // *(byte*)(m_pAnim + 0x2E) |= 4
        }
    }

    ped->Say(CTX_GLOBAL_PAIN_COUGH, 0, 1.0f, false, false, false); // 0x5EFFE0(0x154, 0, 0x3F800000, 0, 0, 0)

    return false;
}

// 0x620660
void CTaskSimpleChoking::UpdateChoke(CPed* victim, CPed* attacker, bool bIsTeargas) {
    m_bIsTeargas = bIsTeargas;
    if (m_pAttacker != attacker) {
        CEntity::ChangeEntityReference(m_pAttacker, attacker);
    }
    m_bIsFinished = false;
    if (m_pAnim && m_pAnim->m_AnimId != ANIM_ID_GAS_CWR) {
        m_pAnim->SetDefaultFinishCallback();
        m_pAnim = CAnimManager::BlendAnimation(victim->GetRpClump(), ANIM_GROUP_DEFAULT, ANIM_ID_GAS_CWR, 4.f);
        m_pAnim->SetFinishCallback([](CAnimBlendAssociation* a, void* data) {
            const auto self = static_cast<CTaskSimpleChoking*>(data);
            self->m_bIsFinished = true;
            self->m_pAnim = nullptr;
        });
        m_pAnim->SetSpeed(CGeneral::GetRandomNumberInRange(0.8f, 1.1f));
        m_nTimeStarted = CTimer::GetTimeInMS() - m_nTimeRemaining;
    }
    m_nTimeRemaining = CGeneral::GetRandomNumberInRange<uint32>(1'000, 2'000);
}

void CTaskSimpleChoking::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleChoking, 0x86d820, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x6202C0);
    RH_ScopedInstall(Destructor, 0x620370);

    RH_ScopedInstall(UpdateChoke, 0x620660);

    RH_ScopedVMTInstall(Clone, 0x623220);
    RH_ScopedVMTInstall(GetTaskType, 0x620360);
    RH_ScopedVMTInstall(MakeAbortable, 0x6203F0);
    RH_ScopedVMTInstall(ProcessPed, 0x620490);
}
