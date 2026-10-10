#include "StdInc.h"

#include "TaskInteriorSitAtDesk.h"

#include "Interior/InteriorManager_c.h" // InteriorManager_c::AreAnimsLoaded (0x5980F0)
#include "Interior/InteriorInfo_t.h"    // InteriorInfo_t::Pos / ::Dir
#include "CarEnterExit.h"               // CCarEnterExit::ms_vecPedDeskAnimOffset (0xC18C6C)

void CTaskInteriorSitAtDesk::InjectHooks() {
    RH_ScopedVirtualClass(CTaskInteriorSitAtDesk, 0x87035c, 9);
    RH_ScopedCategory("Tasks/TaskTypes/Interior");

    RH_ScopedInstall(Constructor, 0x676010);
    RH_ScopedInstall(Destructor, 0x676080);

    RH_ScopedInstall(FinishAnimCB, 0x676190);
    RH_ScopedInstall(StartRandomLoopAnim, 0x677780);
    RH_ScopedInstall(StartRandomOneOffAnim, 0x677880);
    RH_ScopedVMTInstall(Clone, 0x6760E0);
    RH_ScopedVMTInstall(GetTaskType, 0x676070);
    RH_ScopedVMTInstall(MakeAbortable, 0x676150);
    RH_ScopedVMTInstall(ProcessPed, 0x677920);
}

// 0x676010
CTaskInteriorSitAtDesk::CTaskInteriorSitAtDesk(int32 duration, InteriorInfo_t* interiorInfo, bool bDoInstantly) :
    m_bDoInstantly{bDoInstantly},
    m_InteriorInfo{interiorInfo},
    m_Duration{duration}
{
}

// 0x6760E0
CTaskInteriorSitAtDesk::CTaskInteriorSitAtDesk(const CTaskInteriorSitAtDesk& o) :
    CTaskInteriorSitAtDesk{o.m_Duration, o.m_InteriorInfo, o.m_bDoInstantly}
{
}

// 0x676080
CTaskInteriorSitAtDesk::~CTaskInteriorSitAtDesk() {
    if (m_Anim) {
        m_Anim->SetDefaultDeleteCallback(); // оригинал: FUN_004cebc0(DefaultAnimCB, 0) == SetDeleteCallback
    }
}

// 0x676190
void CTaskInteriorSitAtDesk::FinishAnimCB(CAnimBlendAssociation* anim, void* data) {
    const auto self = notsa::cast<CTaskInteriorSitAtDesk>(static_cast<CTask*>(data));

    assert(self && anim);
    assert(anim == self->m_Anim);

    self->m_PrevAnimId = anim->m_AnimId; // 0x676190: task+0x18 = assoc+0x2C, ДО обеих проверок

    if (anim->m_AnimId == ANIM_ID_OFF_SIT_2IDLE_180 || self->m_bTaskAborting && self->m_PrevAnimId == ANIM_ID_OFF_SIT_IN) {
        anim->SetBlendDelta(-1000.f);
        self->m_bTaskFinished = true;
    }

    self->m_Anim = nullptr;
}

// 0x677780
void CTaskInteriorSitAtDesk::StartRandomLoopAnim(CPed* ped, float blendDelta) {
    using CGeneral::GetRandomNumberInRange;

    const auto chance = GetRandomNumberInRange(100u);
    StartAnim(
        ped,
        chance > 40
            ? ANIM_ID_OFF_SIT_TYPE_LOOP    // 0x14B (rand >= 41)
            : chance > 10
                ? ANIM_ID_OFF_SIT_IDLE_LOOP  // 0x14A (11..40)
                : ANIM_ID_OFF_SIT_BORED_LOOP, // 0x14C (0..10)
        blendDelta
    );
    m_AnimTimer.Start(chance > 40 ? GetRandomNumberInRange(7000, 12000) : GetRandomNumberInRange(2000, 5000)); // 0x677780: ESI=0x1B58-(-5000*frac) vs 0x7D0-(-3000*frac)
}

// 0x677880
void CTaskInteriorSitAtDesk::StartRandomOneOffAnim(CPed* ped) {
    const auto chance = CGeneral::GetRandomNumberInRange(100u);
    StartAnim(
        ped,
        chance > 60
            ? ANIM_ID_OFF_SIT_READ
            : chance > 35
                ? ANIM_ID_OFF_SIT_DRINK
                : chance > 10
                    ? ANIM_ID_OFF_SIT_WATCH
                    : ANIM_ID_OFF_SIT_CRASH,
        4.f
    );
}

// 0x676150
bool CTaskInteriorSitAtDesk::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    if (priority == ABORT_PRIORITY_IMMEDIATE) {
        if (m_Anim) {
            m_Anim->SetBlendDelta(-1000.f);
            m_Anim->SetDefaultDeleteCallback(); // 0x676150: FUN_004cebc0(DefaultAnimCB, 0)
            m_Anim = nullptr;
        }
        return true;
    }
    m_bTaskAborting = true;
    return false;
}

// 0x677920
bool CTaskInteriorSitAtDesk::ProcessPed(CPed* ped) {
    const auto currAnimId = m_Anim ? m_Anim->m_AnimId.get() : ANIM_ID_UNDEFINED; // assoc+0x2C
    ped->SetMoveState(PEDMOVE_STILL); // 0x5DEC00

    // 2IDLE_180 уже не играет => пед встал: разворот на 180 и конец задачи
    if (m_bTaskFinished && !RpAnimBlendClumpGetAssociation(ped->GetRpClump(), ANIM_ID_OFF_SIT_2IDLE_180 /* 0x4D68B0 */)) {
        const auto angle = CGeneral::LimitRadianAngle(ped->m_fCurrentRotation + 3.14159274f /* PI, 0x858CB8 */); // 0x53CB50
        ped->m_fAimingRotation  = angle; // +0x55C
        ped->m_fCurrentRotation = angle; // +0x558
        if (!ped->m_matrix) {
            ped->m_placement.m_fHeading = angle;
            return true;
        }
        ped->m_matrix->SetRotateZOnly(angle); // 0x59B020
        return true;
    }

    // Аборты: приглушить посадку / ускорить вставание / уйти в 2IDLE_180
    if (m_bTaskAborting) { // +0x36
        if (!InteriorManager_c::AreAnimsLoaded(2) /* 0x5980F0 */) {
            return true;
        }
        if (currAnimId == ANIM_ID_OFF_SIT_IN) { // 0x148
            m_Anim->SetBlendDelta(-8.0f); // assoc+0x1C
        } else if (currAnimId < ANIM_ID_OFF_SIT_IDLE_LOOP || currAnimId > ANIM_ID_OFF_SIT_WATCH) { // не 0x14A..0x150
            if (currAnimId == ANIM_ID_OFF_SIT_2IDLE_180) { // 0x149
                m_Anim->SetSpeed(2.0f); // assoc+0x24
            }
        } else if (!m_bUpdatePedPos) { // +0x34
            m_Anim->SetDefaultDeleteCallback(); // 0x4CEBC0(CDefaultAnimCallback::DefaultAnimCB, 0)
            m_Anim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_INT_OFFICE, ANIM_ID_OFF_SIT_2IDLE_180, 1000.0f); // 0x4D4610
            m_Anim->SetFinishCallback(FinishAnimCB, this); // 0x4CEBE0
            m_bUpdatePedPos = true;
            return false;
        }
    }

    if (!m_Anim) {
        if (!InteriorManager_c::AreAnimsLoaded(2) /* 0x5980F0 */) {
            return false;
        }
        if (m_PrevAnimId == ANIM_ID_UNDEFINED) {
            if (!m_bDoInstantly) {
                m_Anim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_INT_OFFICE, ANIM_ID_OFF_SIT_IN, 4.0f); // 0x4D4610
                m_Anim->SetFinishCallback(FinishAnimCB, this); // 0x4CEBE0
                return false;
            }
        } else if (m_PrevAnimId != ANIM_ID_OFF_SIT_IN) {
            if (m_PrevAnimId < ANIM_ID_OFF_SIT_CRASH || m_PrevAnimId > ANIM_ID_OFF_SIT_WATCH) { // не 0x14D..0x150 (one-off)
                return false;
            }
            StartRandomLoopAnim(ped, 4.0f); // 0x677780
            return false;
        }
        // prevAnim == UNDEFINED && doInstantly, либо prevAnim == OFF_SIT_IN:
        m_TaskTimer.Start(m_Duration); // инлайн оригинала: +0x1C=now, +0x20=m_Duration, m_bStarted(+0x24)=1; при m_Duration<0 - скип
        StartRandomLoopAnim(ped, 1000.0f); // 0x677780
        m_bUpdatePedPos = true;
        return false;
    }

    // Телепорт на стул
    if (m_bUpdatePedPos) { // +0x34
        const auto pedPos = ped->GetPosition(); // m_matrix->pos (+0x30) / m_placement.m_vPosn (+0x4)
        auto       newPos = pedPos;
        if (currAnimId >= ANIM_ID_OFF_SIT_IDLE_LOOP && currAnimId <= ANIM_ID_OFF_SIT_BORED_LOOP) { // 0x14A..0x14C (loop-анимы)
            const auto transformed = ped->m_matrix->TransformPoint(CCarEnterExit::ms_vecPedDeskAnimOffset /* 0xC18C6C */); // 0x59C890
            newPos = CVector{ transformed.x, transformed.y, pedPos.z };
        } else if (currAnimId == ANIM_ID_OFF_SIT_2IDLE_180) { // 0x149
            const auto transformed = ped->m_matrix->TransformPoint(-CCarEnterExit::ms_vecPedDeskAnimOffset);
            newPos = CVector{ transformed.x, transformed.y, pedPos.z };
        }
        ped->SetPosn(newPos); // 0x4241C0 (z всегда от исходной позиции педа)
        m_bUpdatePedPos = false;
    }

    if (!m_TaskTimer.m_bStarted || !m_TaskTimer.IsOutOfTime() /* 0x420E30 */) { // +0x24
        if (m_AnimTimer.m_bStarted && m_AnimTimer.IsOutOfTime() /* 0x420E30 */) { // +0x30
            m_AnimTimer.Stop();
            if (CGeneral::GetRandomNumber() < 0x3FFF) { // rand()
                StartRandomOneOffAnim(ped); // 0x677880
            } else {
                StartRandomLoopAnim(ped, 4.0f); // 0x677780
            }
        }
    } else if (m_Anim->m_AnimId != ANIM_ID_OFF_SIT_2IDLE_180) { // время вышло: запуск вставания
        m_Anim->SetDefaultDeleteCallback(); // 0x4CEBC0(CDefaultAnimCallback::DefaultAnimCB, 0)
        m_Anim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_INT_OFFICE, ANIM_ID_OFF_SIT_2IDLE_180, 1000.0f); // 0x4D4610
        m_Anim->SetFinishCallback(FinishAnimCB, this); // 0x4CEBE0
        m_bUpdatePedPos = true;
    }

    // OFF_SIT_IN: подтягивание к стулу
    if (m_Anim->m_AnimId == ANIM_ID_OFF_SIT_IN) {
        const auto& mat  = *ped->m_matrix;
        const auto  dir  = m_InteriorInfo->Pos - ped->GetPosition();
        const auto  dist = dir.Magnitude();
        const auto  step = dir * (1.0f /* 0x858624 */ / dist) * std::min(dist, 0.02f /* 0x858B38 */);
        ped->m_vecAnimMovingShiftLocal.x = step.Dot(mat.GetRight());   // +0x4D8
        ped->m_vecAnimMovingShiftLocal.y = step.Dot(mat.GetForward()); // +0x4DC
        ped->m_fAimingRotation = CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints( // 0x53CBE0 + 0x53CB50
            m_InteriorInfo->Dir.x, m_InteriorInfo->Dir.y, 0.0f, 0.0f
        )); // +0x55C
    }
    return false;
}

void CTaskInteriorSitAtDesk::StartAnim(CPed* ped, AnimationId animId, float blendDelta) {
    if (m_Anim) {
        m_Anim->SetDefaultDeleteCallback(); // 0x677780:12-13 / 0x677880:11-12: FUN_004cebc0(DefaultAnimCB, 0)
    }
    m_Anim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_INT_OFFICE, animId, blendDelta);
    m_Anim->SetFinishCallback(FinishAnimCB, this);
}
