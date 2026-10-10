#include "StdInc.h"

#include "TaskLeanOnVehicle.h"

void CTaskLeanOnVehicle::InjectHooks() {
    RH_ScopedVirtualClass(CTaskLeanOnVehicle, 0x86FAF4, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x660F90);
    RH_ScopedInstall(Destructor, 0x661030);

    RH_ScopedInstall(FinishAnimCB, 0x661160);

    RH_ScopedVMTInstall(MakeAbortable, 0x661110);
    RH_ScopedVMTInstall(ProcessPed, 0x6648C0);

}

// 0x660F90
CTaskLeanOnVehicle::CTaskLeanOnVehicle(CEntity* vehicle, int32 leanAnimDurationInMs, uint8 a4) {
    m_LeanAnimDurationInMs = leanAnimDurationInMs;
    m_Vehicle = vehicle->AsVehicle(); // todo: strange
    field_10 = a4;
    m_LeanAnim = nullptr;
    m_LastAnimId = ANIM_ID_UNDEFINED;
    m_bFinished = 0;
    m_LeanAnimId = 0;
    m_StopLeaning = 0;
    CEntity::SafeRegisterRef(m_Vehicle);
}

// 0x661030
CTaskLeanOnVehicle::~CTaskLeanOnVehicle() {
    CEntity::SafeCleanUpRef(m_Vehicle);
    if (m_LeanAnim) {
        m_LeanAnim->SetDeleteCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
    }
}

// 0x661160
void CTaskLeanOnVehicle::FinishAnimCB(CAnimBlendAssociation* assoc, void* data) {
    const auto self = notsa::cast<CTaskLeanOnVehicle>(static_cast<CTask*>(data));

    self->m_LastAnimId = assoc->m_AnimId;

    if (assoc->m_AnimId == ANIM_ID_LEANOUT) {
        assoc->m_BlendDelta = -1000.0f;
        self->m_bFinished = true;
    }

    if (self->m_LeanAnimId) {
        if (assoc->m_AnimId == ANIM_ID_LEANIN) {
            assoc->m_BlendDelta = -1000.0f;
            self->m_bFinished = true;
        }
    }

    self->m_LeanAnim = nullptr;
}

// 0x661110
bool CTaskLeanOnVehicle::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (priority == ABORT_PRIORITY_IMMEDIATE) {
        if (m_LeanAnim) {
            m_LeanAnim->m_BlendDelta = -1000.0f;
            m_LeanAnim->SetDeleteCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
            m_LeanAnim = nullptr;
        }
        m_StopLeaning = true;
        ped->m_pEntityIgnoredCollision = nullptr;
        return true;
    }

    m_LeanAnimId = 1;
    return false;
}

// 0x6648C0
bool CTaskLeanOnVehicle::ProcessPed(CPed* ped) {
    if (!m_Vehicle) {
        m_StopLeaning = true;
    }
    ped->m_pEntityIgnoredCollision = m_Vehicle;

    const auto currAnimId = m_LeanAnim ? m_LeanAnim->m_AnimId.get() : ANIM_ID_UNDEFINED; // assoc+0x2C (WEnum -> AnimationId)

    if (m_StopLeaning) { // 0x664914
        ped->m_pEntityIgnoredCollision = nullptr;
        return true;
    }

    ped->SetMoveState(PEDMOVE_STILL); // 0x5DEC00(1)

    if (m_bFinished) {
        if (!RpAnimBlendClumpGetAssociation(ped->GetRpClump(), ANIM_ID_LEANOUT)) {
            ped->m_pEntityIgnoredCollision = nullptr; // 0x664914
            return true;
        }
    }

    // Ветка m_LeanAnimId != 0 (0x6649A5..0x6649E7) - в исходном фрагменте ПРОПУЩЕНА (FIX(verify)):
    if (m_LeanAnimId) {
        switch (currAnimId) {
        case ANIM_ID_LEANIN:
            m_LeanAnim->m_BlendDelta = -8.0f; // assoc+0x1C: быстро гасим LEANIN
            break;
        case ANIM_ID_LEANIDLE:
            m_LeanAnim->SetDeleteCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr); // 0x4CEBC0(0x4D1390,0)
            m_LeanAnim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_GANGS, ANIM_ID_LEANOUT, 1000.0f);
            m_LeanAnim->SetFinishCallback(CTaskLeanOnVehicle::FinishAnimCB, this); // 0x4CEBE0
            return false; // 0x664A23
        case ANIM_ID_LEANOUT:
            m_LeanAnim->m_TimeStep = 3.0f; // assoc+0x24: ускоряем LEANOUT
            break;
        default:
            break;
        }
        // (далее - общий хвост, как и при m_LeanAnimId == 0)
    }

    if (!m_LeanAnim) {
        if (m_LastAnimId == ANIM_ID_UNDEFINED && !field_10) {
            // Старт: сразу LEANIN (таймер не трогается)
            m_LeanAnim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_GANGS, ANIM_ID_LEANIN, 4.0f);
            m_LeanAnim->SetFinishCallback(CTaskLeanOnVehicle::FinishAnimCB, this);
            return false; // 0x664A23
        }
        if (m_LastAnimId != ANIM_ID_UNDEFINED && m_LastAnimId != ANIM_ID_LEANIN) {
            return false; // 0x664A23
        }
        // Таймер + LEANIDLE
        if (m_LeanAnimDurationInMs >= 0) {
            m_LeanTimer.m_nStartTime = CTimer::GetTimeInMS();
            m_LeanTimer.m_nInterval  = m_LeanAnimDurationInMs;
            m_LeanTimer.m_bStarted   = true;
        }
        m_LeanAnim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_GANGS, ANIM_ID_LEANIDLE, 1000.0f);
    } else {
        if (!m_LeanTimer.m_bStarted) {
            return false; // 0x664A23
        }
        if (!m_LeanTimer.IsOutOfTime() || currAnimId == ANIM_ID_LEANOUT) {
            return false; // 0x664A23
        }
        m_LeanAnim->SetDeleteCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
        m_LeanAnim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_GANGS, ANIM_ID_LEANOUT, 1000.0f);
    }
    m_LeanAnim->SetFinishCallback(CTaskLeanOnVehicle::FinishAnimCB, this); // 0x4CEBE0
    return false; // 0x664A23
}
