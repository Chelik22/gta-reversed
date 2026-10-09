#include "StdInc.h"

#include "TaskComplexGangFollower.h"

// Added for batch w2-07 (7 reversed functions). Already covered by StdInc.h:
// General.h, Streaming.h/StreamingInfo.h (IFPToModelId, STREAMING_KEEP_IN_MEMORY), Animation/AnimManager.h,
// Animation/AnimBlendAssociation.h, ModelIndices.h (MI_GANG_*), Fx/FxManager.h (g_fxMan), Entity/Ped/Ped.h,
// PedGroup.h, PedGroupMembership.h, PedIntelligence.h, Tasks/TaskManager.h, Tasks/TaskTimer.h,
// Enums/{eTaskType,ePedState,eMoveState,AnimationEnums,eBoneTag}.h, Events/EventGroup.h,
// Core/Vector.h (DotProduct), common.h (sq, approxEqual), extensions/utility.hpp (rng::, notsa::NotIsNull)
#include "TaskComplexGangLeader.h"                                   // CTaskComplexGangLeader (ControlSubTask)
#include "TaskComplexWanderGang.h"                                   // CTaskComplexWanderGang (CreateNextSubTask)
#include "TaskComplexSignalAtPed.h"                                  // CTaskComplexSignalAtPed (CreateNextSubTask)
#include "TaskComplexTurnToFaceEntityOrCoord.h"                      // CTaskComplexTurnToFaceEntityOrCoord (CreateNextSubTask)
#include "TaskComplexLeaveCar.h"                                     // CTaskComplexLeaveCar (CreateFirstSubTask)
#include "TaskComplexPassObject.h"                                   // CTaskComplexPassObject (ControlSubTask)
#include "TaskSimpleCarDrive.h"                                      // CTaskSimpleCarDrive (CreateFirstSubTask)
#include "TaskSimpleStandStill.h"                                    // CTaskSimpleStandStill (CreateFirst/NextSubTask, ControlSubTask)
#include "TaskSimplePause.h"                                         // CTaskSimplePause (CreateNextSubTask)
#include "TaskSimpleRunAnim.h"                                       // CTaskSimpleRunAnim (ControlSubTask)
#include "TaskSimpleGoToPoint.h"                                     // CTaskSimpleGoToPoint (ControlSubTask)
#include "TaskSimpleHoldEntity.h"                                    // CTaskSimpleHoldEntity (ControlSubTask)
#include "TaskComplexPlayHandSignalAnim.h"                           // CTaskComplexPlayHandSignalAnim (ControlSubTask)
#include "SeekEntity/TaskComplexSeekEntity.h"                        // CTaskComplexSeekEntity<> (CreateFirst/NextSubTask, ControlSubTask)
#include "SeekEntity/PosCalculators/EntitySeekPosCalculatorXYOffset.h" // CEntitySeekPosCalculatorXYOffset
#include "Events/EventPassObject.h"                                  // CEventPassObject (ControlSubTask)
#include "Audio/Enums/PedSpeechContexts.h"                           // CTX_GLOBAL_* (ControlSubTask)
#include "Ragdoll/IKChainManager.h"                                  // g_ikChainMan (ControlSubTask)

// Глобалы оригинала (.data/.rdata, base 0x400000) - значения сняты с оригинального exe
static auto& int_86C938 = StaticRef<int32>(0x86C938); // 30 - порог m_AnotherStaticCounter ("слишком много педов рядом")
static auto& flt_86FC2C = StaticRef<float>(0x86FC2C); // 1.0f - max dist (в оригинале умножается на flt_858B8C = 0.5f)
static auto& flt_86F830 = StaticRef<float>(0x86F830); // 5.0f
static auto& flt_86FC30 = StaticRef<float>(0x86FC30); // 2.0f
static auto& flt_86F82C = StaticRef<float>(0x86F82C); // 64.0f - квадрат дистанции, при которой догоняем лидера (в толпе)
static auto& flt_86FC78 = StaticRef<float>(0x86FC78); // 0.5f - радиус go-to-point
static auto& flt_86FC7C = StaticRef<float>(0x86FC7C); // 0.5f - change rate mult (turn-to-face)
static auto& flt_86FC80 = StaticRef<float>(0x86FC80); // 0.2f - max heading (turn-to-face)
static auto& flt_86FCB0 = StaticRef<float>(0x86FCB0); // 0.5f - target radius (WanderGang)
static auto& flt_858624 = StaticRef<float>(0x858624); // 1.0f
static auto& flt_85A5C4 = StaticRef<float>(0x85A5C4); // 0.0125f - шаг скорости walk-анимации
static auto& flt_858B8C = StaticRef<float>(0x858B8C); // 0.5f
static auto& flt_858B50 = StaticRef<float>(0x858B50); // 0.0f

void CTaskComplexGangFollower::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexGangFollower, 0x86F938, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x65EAA0);
    RH_ScopedInstall(Destructor, 0x65EBB0);
    //RH_ScopedInstall(CalculateOffsetPosition, 0x65ED40, { .Reversed = false }); // not hooked because i want to keep CVector return, but original function took a CVector&
    RH_ScopedInstall(Clone, 0x65ECB0);
    RH_ScopedInstall(MakeAbortable, 0x65EC30);
    RH_ScopedInstall(CreateNextSubTask, 0x665E00);
    RH_ScopedInstall(CreateFirstSubTask, 0x666160);
    RH_ScopedInstall(ControlSubTask, 0x662A10);
}

// 0x65EAA0
// NOTE(fix vs stash): сентябрьский стэш не копировал `dword2C = pos;` (в декомпиле
// параметр pos пишется и в +0x2C, и в +0x14/+0x20) - восстановлено по декомпилу.
CTaskComplexGangFollower::CTaskComplexGangFollower(CPedGroup* pedGroup, CPed* ped, uint8 a4, CVector pos, float a6) : CTaskComplex() {
    dword40 = 0;
    dword44 = 0;
    byte48  = 0;
    byte49  = 0;
    m_PedGroup = pedGroup;
    byte3C     = a4;
    dword2C    = pos;
    m_PedPosn  = pos;
    dword20    = pos;
    dword38    = a6;
    m_Leader   = ped;
    m_Flags = (m_Flags | 0x14) & ~0x03; // set bits 2|4, clear bits 0|1
    if (m_Leader) {
        CEntity::SafeRegisterRef(m_Leader);
        m_PedPosn = m_Leader->GetPosition();
    }
    m_Flags &= ~0x03;
    if (m_Leader == FindPlayerPed()) {
        m_Flags |= 0x08; // leader is the player
    } else {
        m_Flags &= ~0x08;
    }
}

// 0x65EBB0
CTaskComplexGangFollower::~CTaskComplexGangFollower() {
    if (m_Leader) {
        CEntity::SafeCleanUpRef(m_Leader);
    }
    if (m_Flags & 1) { // anims were referenced
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
        m_Flags &= ~1;
    }
}

// 0x65ED40
// CalculateOffsetPosition в батч w2-07 не входит - остаётся redirect-стабом
// (см. комментарий у RH_ScopedInstall в InjectHooks).
CVector CTaskComplexGangFollower::CalculateOffsetPosition() {
    CVector ret;
    plugin::CallMethod<0x65ED40, CTaskComplexGangFollower*>(this, &ret);
    return ret;
}

// 0x65ECB0
CTask* CTaskComplexGangFollower::Clone() const {
    if (const auto clone = new CTaskComplexGangFollower(m_PedGroup, m_Leader, byte3C, dword20, dword38)) {
        // Copy only the "do stuff" flag (bit 2)
        clone->m_Flags = (uint8)((clone->m_Flags & ~4) | (m_Flags & 4));
        return clone;
    }
    return nullptr;
}

// 0x65EC30
// NOTE(fix vs stash): стэш писал `ped->bCanExitCar = false`, но сырой паттерн декомпила
// `*(uint*)(ped+0x474) &= ~0x400000` - это бит 22 по смещению 0x474, т.е. bDontAcceptIKLookAts
// в текущей раскладке Ped.h (проверено по влитому CTaskComplexGangLeader::MakeAbortable 0x65DFA0,
// TaskComplexGangLeader.cpp - тот же паттерн -> bDontAcceptIKLookAts).
// `*(uint*)(ped+0x478) &= ~0x10000` = bMoveAnimSpeedHasBeenSetByTask (структурный бит 112 = +0x47A.0).
bool CTaskComplexGangFollower::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (!m_pSubTask) {
        ped->bMoveAnimSpeedHasBeenSetByTask = false;
        ped->bDontAcceptIKLookAts           = false;
        return true;
    }
    if (m_pSubTask->MakeAbortable(ped, priority, event)) {
        ped->bDontAcceptIKLookAts           = false;
        ped->bMoveAnimSpeedHasBeenSetByTask = false;
        return true;
    }
    return false;
}

// 0x665E00
CTask* CTaskComplexGangFollower::CreateNextSubTask(CPed* ped) {
    if (!m_Leader) {
        ped->bMoveAnimSpeedHasBeenSetByTask = false;
        return nullptr;
    }

    const auto subTaskType = m_pSubTask->GetTaskType();

    // Flag 2 is set and we were told to signal at the leader: leave the group and wander on our own
    if ((m_Flags & 2) && subTaskType == eTaskType::TASK_COMPLEX_SIGNAL_AT_PED /* 0x4BA */) {
        m_PedGroup->GetMembership().RemoveMember(ped); // 0x5FB210
        ped->GetTaskManager().SetTask(
            new CTaskComplexWanderGang(PEDMOVE_WALK, (uint8)CGeneral::GetRandomNumberInRange(0, 8), 30'000, true, flt_86FCB0 /* 0.5f */),
            TASK_PRIMARY_DEFAULT,
            false
        ); // 0x681AF0
        ped->bMoveAnimSpeedHasBeenSetByTask = false;
        return nullptr;
    }

    const bool bTooFarFromLeader = int_86C938 /* 30 */ < ped->GetIntelligence()->m_AnotherStaticCounter;
    if (bTooFarFromLeader
     || (subTaskType == eTaskType::TASK_COMPLEX_SEEK_ENTITY /* 0x38B */ && m_Leader->m_nPedState < PEDSTATE_WANDER_RANGE)) {
        // Reached the leader (or he is idle-ish)
        if (m_Leader && m_Leader->IsPlayer()) {
            ped->Say(CTX_GLOBAL_FOLLOW_ARRIVE, 0, 0.3f); // 0x5EFFE0
        }
        return new CTaskSimpleStandStill(500, false, false, 8.0f); // 0x62F310
    }

    switch (subTaskType) {
    case eTaskType::TASK_SIMPLE_STAND_STILL:      /* 0xCB */
    case eTaskType::TASK_COMPLEX_HANDSIGNAL_ANIM: /* 0x1AA */ {
        if (m_Flags & 2) {
            return new CTaskComplexSignalAtPed(m_Leader, -1, false); // 0x660A30
        }
        if (CGeneral::GetRandomNumberInRange(0, 30) == 20) {
            const auto mem = m_PedGroup->GetMembership().GetMember(CGeneral::GetRandomNumberInRange(0, (int32)m_PedGroup->GetMembership().CountMembers()));
            const auto who = mem == ped ? m_PedGroup->GetMembership().GetLeader() : mem;
            if (who) {
                return new CTaskComplexTurnToFaceEntityOrCoord(who, flt_86FC7C /* 0.5f */, flt_86FC80 /* 0.2f */); // 0x66B890
            }
        }
        return new CTaskSimplePause(50); // 0x48E750
    }
    case eTaskType::TASK_SIMPLE_CAR_DRIVE: /* 0x2C5 */
        // Оригинал делает виртуальный вызов vtbl+0x24 (ControlSubTask) этого же таска
        return ControlSubTask(ped);
    }

    if (m_Flags & 4) {
        const auto task = new CTaskComplexSeekEntity<CEntitySeekPosCalculatorXYOffset>( // 0x661DC0
            m_Leader,
            50'000,
            1'000,
            flt_86FC2C * flt_858B8C,        // 1.0f * 0.5f
            flt_86F830,                     // 5.0f
            flt_86FC30,                     // 2.0f
            false,
            false
        );
        task->GetSeekPosCalculator().SetOffset(dword20); // 0x65DE60 = CEntitySeekPosCalculatorXYOffset::ctor(vtable 0x86F8F8) с нашим офсетом
        task->SetMoveState(PEDMOVE_SPRINT);              // *(task+0x50) = 7
        task->SetIsTrackingEntity(true);                 // *(task+0x54) |= 8
        return task;
    }

    return new CTaskSimpleStandStill(500, false, false, 8.0f); // 0x62F310
}

// 0x666160
// NOTE(vs stash): записи *(task+0x50)=7 и *(task+0x54)|=8 выражаются публичными акцессорами
// CTaskComplexSeekEntity: SetMoveState(PEDMOVE_SPRINT) / SetIsTrackingEntity(true).
CTask* CTaskComplexGangFollower::CreateFirstSubTask(CPed* ped) {
    if (!m_Leader) {
        ped->bMoveAnimSpeedHasBeenSetByTask = false;
        return nullptr;
    }

    if (!ped->bInVehicle || !ped->m_pVehicle || m_Leader->m_pVehicle != ped->m_pVehicle) {
        if (!ped->bInVehicle || !ped->m_pVehicle) {
            if ((m_Flags & 4) && ped->GetIntelligence()->m_AnotherStaticCounter <= int_86C938 /* 30 */) {
                // Follow the leader
                const auto task = new CTaskComplexSeekEntity<CEntitySeekPosCalculatorXYOffset>( // 0x661DC0
                    m_Leader,
                    50'000,
                    1'000,
                    flt_86FC2C * flt_858B8C,        // 1.0f * 0.5f
                    flt_86F830,                     // 5.0f
                    flt_86FC30,                     // 2.0f
                    false,
                    true
                );
                task->GetSeekPosCalculator().SetOffset(dword20);
                task->SetMoveState(PEDMOVE_SPRINT);   // *(task+0x50) = 7
                task->SetIsTrackingEntity(true);      // *(task+0x54) |= 8
                return task;
            }
            // Just stand still
            return new CTaskSimpleStandStill(500, false, false, 8.0f); // 0x62F310
        }
        // In a vehicle, but not the leader's one (or leader has none) - get out
        return new CTaskComplexLeaveCar(ped->m_pVehicle, 0, 0, true, false); // 0x63B8C0
    }

    // Both peds are in the same vehicle
    return new CTaskSimpleCarDrive(ped->m_pVehicle, nullptr, false); // 0x63C340
}

// 0x662A10
// Структура m_Flags (uint8 @0x3D): 0x01=anim block "gangs" referenced, 0x02=leave group later,
//   0x04="do stuff" (active seek), 0x08=leader is the player.
// dword40/dword44/byte48(+byte49) = CTaskTimer (StartTime/Interval/m_bStarted/m_bStopped) - доступ через
//   reinterpret_cast, т.к. в хедере класса поля названы dword40/dword44/byte48/byte49.
// NOTE: CTaskComplexSeekEntity::m_maxEntityDist2D (+0x18) и m_minEntityDist2D (+0x20) приватные в хедере -
//   доступ через offset-read/write (можно заменить акцессорами).
CTask* CTaskComplexGangFollower::ControlSubTask(CPed* ped) {
    ped->bDontAcceptIKLookAts = false; // *(ped+0x474) &= ~0x400000

    if (!m_Leader) {
        if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
            ped->bMoveAnimSpeedHasBeenSetByTask = false; // *(ped+0x478) &= ~0x10000
            return nullptr;
        }
        return m_pSubTask;
    }

    if (m_pSubTask->GetTaskType() == eTaskType::TASK_SIMPLE_CAR_DRIVE /* 0x2C5 */) {
        return m_pSubTask; // keep driving
    }

    const auto walkAnim = RpAnimBlendClumpGetAssociation(ped->GetRpClump(), ANIM_ID_WALK); // 0x4D68B0
    const auto leaderMoveState = m_Leader->GetIntelligence()->GetMoveStateFromGoToTask();  // 0x601D70
    const bool bLeaderIsMoving = leaderMoveState == PEDMOVE_WALK || leaderMoveState == PEDMOVE_RUN || leaderMoveState == PEDMOVE_SPRINT;

    dword20 = CalculateOffsetPosition(); // 0x65ED40 (остаётся redirect'ом в репо - см. комментарий в InjectHooks)

    const auto leaderPos = m_Leader->GetPosition();
    const auto pedPos    = ped->GetPosition();
    const CVector diff   = leaderPos - pedPos;

    bool bSpeedSetByUs{};

    if (m_pSubTask->GetTaskType() == eTaskType::TASK_COMPLEX_SEEK_ENTITY /* 0x38B */) {
        const bool bTooManyPedsAround = ped->GetIntelligence()->m_AnotherStaticCounter > 8;

        if (m_Flags & 4) {
            // Оригинал работает с сабтаском как с XYOffset-специализацией (только её и создаёт)
            const auto seekTask = static_cast<CTaskComplexSeekEntity<CEntitySeekPosCalculatorXYOffset>*>(m_pSubTask);
            seekTask->GetSeekPosCalculator().SetOffset(dword20); // +0x44..+0x4C
            *reinterpret_cast<float*>(reinterpret_cast<char*>(seekTask) + 0x20) = 2.0f; // m_minEntityDist2D = 2.0f (приватное, +0x20)

            const auto goToPoint = static_cast<CTaskSimpleGoToPoint*>(ped->GetTaskManager().FindActiveTaskByType(TASK_SIMPLE_GO_TO_POINT));            // 0x681740
            const auto nodeRoute = ped->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_FOLLOW_NODE_ROUTE); // 0x681740

            if (!goToPoint || nodeRoute || !bLeaderIsMoving) {
                bSpeedSetByUs = bTooManyPedsAround;
            } else {
                auto target = leaderPos + dword20; // Sum, 0x40FE30 (мутируется ниже - target += fwd * 2.0f)
                auto delta        = target - pedPos;     // Difference, 0x40FE60
                delta.z = 0.f;                           // *(delta+8) = 0

                const auto& fwd = m_Leader->GetForward(); // CMatrix::m_forward (в декомпиле - m_matrix->forward)
                if (DotProduct(pedPos, fwd) - DotProduct(delta, fwd) >= flt_858B50 /* 0.0f */) {
                    // Цель не ниже по forward-оси лидера - скорость не трогаем
                    bSpeedSetByUs = bTooManyPedsAround;
                } else {
                    const auto maxDistPlus1 = *reinterpret_cast<const float*>(reinterpret_cast<const char*>(seekTask) + 0x18) + flt_858624 /* 1.0f */; // m_maxEntityDist2D (приватное, +0x18)
                    if (delta.SquaredMagnitude() /* 0x406DA0 */ > sq(maxDistPlus1)) {
                        // Цель далеко: ускоряемся в её сторону (сдвинув точку на 2*forward), скорость ограничиваем 0.85
                        target += fwd * 2.0f; // Scale 0x40FEC0 + operator+= 0x411A00
                        if (!walkAnim) {
                            bSpeedSetByUs = bTooManyPedsAround;
                        } else {
                            // 0x420800 - нереверснутый 2-арговый float хелпер (min/max-подобный); см. аналогичное использование в stash Cam.cpp:2774
                            walkAnim->m_Speed = plugin::CallAndReturn<float, 0x420800>(0.85f /* 0x3F59999A */, walkAnim->m_Speed - flt_85A5C4 /* 0.0125f */);
                            bSpeedSetByUs = true;
                            goToPoint->UpdatePoint(target, flt_86FC78 /* 0.5f */, false); // 0x645700
                        }
                    } else if (!walkAnim) {
                        bSpeedSetByUs = bTooManyPedsAround;
                    } else {
                        // Подгоняем скорость walk-анимации
                        const auto oldSpeed = walkAnim->m_Speed;
                        ped->SetMoveAnimSpeed(walkAnim); // 0x5DEC10
                        const auto newSpeed = walkAnim->m_Speed;
                        if (approxEqual(oldSpeed, newSpeed, 0.013f) /* 0x3C54FDF4, 0x4EEA80 */) {
                            walkAnim->m_Speed = oldSpeed;
                            bSpeedSetByUs = true;
                            goToPoint->UpdatePoint(target, flt_86FC78 /* 0.5f */, false); // 0x645700
                        } else if (newSpeed < oldSpeed) {
                            walkAnim->m_Speed = oldSpeed - flt_85A5C4; // 0.0125f
                            bSpeedSetByUs = true;
                            goToPoint->UpdatePoint(target, flt_86FC78 /* 0.5f */, false); // 0x645700
                        } else {
                            walkAnim->m_Speed = oldSpeed + flt_85A5C4; // 0.0125f
                            bSpeedSetByUs = true;
                            goToPoint->UpdatePoint(target, flt_86FC78 /* 0.5f */, false); // 0x645700
                        }
                    }
                }
            }

            // Догнали лидера - встаём
            if (bTooManyPedsAround
             && sq(diff.x) + sq(diff.y) < flt_86F82C /* 64.0f */
             && m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
                dword20 = CVector{ diff.x, diff.y, 0.f };
                return new CTaskSimpleStandStill(500, false, false, 8.0f); // 0x62F310
            }
        }
        // (flags&4 сброшен -> bSpeedSetByUs остаётся false, как в декомпиле)
    }

    ped->bMoveAnimSpeedHasBeenSetByTask = bSpeedSetByUs; // *(ped+0x478) ^= bVar7 << 16

    // Подгрузка/освобождение аним-блока "gangs" (static из CTaskComplexGangLeader)
    if (m_Flags & 1) {
        if (!CTaskComplexGangLeader::ShouldLoadGangAnims()) { // 0x65E7F0
            CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs")); // 0x4D3FD0 / 0x4D3990
            m_Flags &= ~1;
        }
    } else if (CTaskComplexGangLeader::ShouldLoadGangAnims()) {
        const auto blk = CAnimManager::GetAnimationBlockIndex("gangs"); // 0x4D3990
        if (CAnimManager::GetAnimBlocks()[blk].IsLoaded) {              // ms_animBlocks @0xB5D4B0, stride 0x20
            CAnimManager::AddAnimBlockRef(blk);                         // 0x4D3FB0
            m_Flags |= 1;
        } else {
            CStreaming::RequestModel(IFPToModelId(blk), STREAMING_KEEP_IN_MEMORY); // 0x4087E0 (flags = 8)
        }
    }

    // Иногда решаем покинуть группу (приводит к RemoveMember + WanderGang в CreateNextSubTask)
    if (!m_PedGroup->m_bIsMissionGroup /* +0x2D0 */
     && !(m_Flags & 8)
     && m_PedGroup->GetMembership().CountMembers() > 3      // 0x5F6A50
     && CGeneral::GetRandomNumberInRange(0, 2000) == 500) { // 0x407180
        m_Flags |= 2;
    }

    // Курение/питьё: выдох (exhale FX), когда вышло время таймера
    auto& exhaleTimer = *reinterpret_cast<CTaskTimer*>(&dword40); // 0x40: StartTime, 0x44: Interval, 0x48: m_bStarted, 0x49: m_bStopped
    if (exhaleTimer.m_bStarted && exhaleTimer.IsOutOfTime() /* 0x420E30 */) {
        if (ped->GetRpClump() && RwFrameGetMatrix(RpClumpGetFrame(ped->GetRpClump()))) {
            if (const auto fx = g_fxMan.CreateFxSystem("exhale", CVector{ 0.f, 0.1f, 0.f }, RwFrameGetMatrix(RpClumpGetFrame(ped->GetRpClump())), false)) { // 0x4A9BE0
                fx->AttachToBone(ped, eBoneTag::BONE_HEAD); // 0x4AA400
                fx->PlayAndKill();                          // 0x4AA3D0
            }
        }
        exhaleTimer.m_bStarted = false; // byte48 = 0
    }

    if (!ped->IsVisible()) { // 0x536BC0
        return m_pSubTask;
    }

    // Изредка смотрим на случайного члена группы
    if (!g_ikChainMan.IsLooking(ped) /* 0x6181A0 */
     && CGeneral::GetRandomNumberInRange(0, 100) > 95 /* 0x407180 */) {
        // Оригинал: GetMember(rand(0,8)) (0x821B40 = MSVC _ftol2 в декомпиле)
        auto who = m_PedGroup->GetMembership().GetMember(CGeneral::GetRandomNumberInRange(0, 8)); // 0x5F69B0
        if (who == ped) {
            who = m_Leader;
        }
        if (who) {
            g_ikChainMan.LookAt("TaskGangFollower", ped, who, CGeneral::GetRandomNumberInRange(3000, 5000), eBoneTag::BONE_HEAD, nullptr, true, 0.15f, 500, 3, false); // 0x618970
        }
    }

    if (!(m_Flags & 1) || ped->m_nPedState > PEDSTATE_WANDER_PATH /* 5 */) {
        return m_pSubTask;
    }

    const auto heldEntity = ped->GetEntityThatThisPedIsHolding(); // 0x5E02E0

    if (!heldEntity) {
        if (ped->IsPlayingHandSignal() /* 0x5E0460 */
         || ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_PARTIAL_ANIM) /* 0x681810 */) {
            return m_pSubTask;
        }

        const auto rnd = CGeneral::GetRandomNumberInRange(0, 500);
        if (rnd > 50 && rnd < 56) { // ~1%
            ped->GetTaskManager().SetTaskSecondary(
                new CTaskSimpleRunAnim(ANIM_GROUP_GANGS /* 52 */, static_cast<AnimationId>(ANIM_ID_PRTIAL_GNGTLKA + CGeneral::GetRandomNumberInRange(0, 8)) /* +0x117 = GNGTLKA..GNGTLKH */, 4.0f, false), // 0x61A8B0
                TASK_SECONDARY_PARTIAL_ANIM
            ); // 0x681B60
            if (ped->m_nPedState == PEDSTATE_IDLE /* 1 */) {
                switch (CGeneral::GetRandomNumberInRange(0, 10)) {
                case 0: case 1: case 2:      ped->Say(CTX_GLOBAL_CHAT, 0, 1.0f); break;             /* 45  */
                case 3: case 4: case 5:
                case 6: case 7:              ped->Say(CTX_GLOBAL_PCONV_GREET_MALE, 0, 1.0f); break; /* 166 */
                case 8:                      ped->Say(CTX_GLOBAL_BOOZE_REQUEST, 0, 1.0f); break;    /* 24  */
                case 9:                      ped->Say(CTX_GLOBAL_SPLIFF_REQUEST, 0, 1.0f); break;   /* 201 */
                }
            }
            return m_pSubTask;
        }
        if (rnd != 100 || ped->m_nPedState != PEDSTATE_IDLE) {
            return m_pSubTask;
        }
        ped->GetTaskManager().SetTaskSecondary(
            new CTaskComplexPlayHandSignalAnim(ANIM_ID_UNDEFINED, 4.0f), // 0x61B2B0
            TASK_SECONDARY_PARTIAL_ANIM
        );
        switch (CGeneral::GetRandomNumberInRange(0, 10)) {
        case 0: case 1: case 2:      ped->Say(CTX_GLOBAL_CHAT, 0, 1.0f); break;             /* 45  */
        case 3: case 4: case 5:
        case 6: case 7:              ped->Say(CTX_GLOBAL_PCONV_GREET_MALE, 0, 1.0f); break; /* 166 */
        case 8:                      ped->Say(CTX_GLOBAL_BOOZE_REQUEST, 0, 1.0f); break;    /* 24  */
        case 9:                      ped->Say(CTX_GLOBAL_SPLIFF_REQUEST, 0, 1.0f); break;   /* 201 */
        }
        return m_pSubTask;
    }

    // Держит предмет (бутылку/косяк): анимации курения/питья + передача предмета
    const std::array assocs{
        RpAnimBlendClumpGetAssociation(ped->GetRpClump(), ANIM_ID_DRNKBR_PRTL),   /* 299 */
        RpAnimBlendClumpGetAssociation(ped->GetRpClump(), ANIM_ID_SMKCIG_PRTL),   /* 300 */
        RpAnimBlendClumpGetAssociation(ped->GetRpClump(), ANIM_ID_DRNKBR_PRTL_F), /* 301 */
        RpAnimBlendClumpGetAssociation(ped->GetRpClump(), ANIM_ID_SMKCIG_PRTL_F), /* 302 */
    }; // 0x4D68B0
    const bool bAnySmokingDrinkingAnim = rng::any_of(assocs, notsa::NotIsNull{});

    if (bAnySmokingDrinkingAnim) {
        if (g_ikChainMan.IsLooking(ped)) { // 0x6181A0
            g_ikChainMan.AbortLookAt(ped, 250); // 0x618280
        }
        ped->bDontAcceptIKLookAts = true; // *(ped+0x474) |= 0x400000

        // Таймер выдоха для SMKCIG анимаций
        if (!exhaleTimer.m_bStarted
         && ((assocs[1] && assocs[1]->m_CurrentTime < flt_858B8C /* 0.5f */)
          || (assocs[3] && assocs[3]->m_CurrentTime < flt_858B8C /* 0.5f */))) {
            exhaleTimer.Start(2700); // 0xA8C; StartTime = CTimer::m_snTimeInMilliseconds (0xB7CB84)
        }
    }

    if (ped->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_PASS_OBJECT) /* 0x4BB */) { // 0x681740
        return m_pSubTask;
    }

    if (CGeneral::GetRandomNumberInRange(0, 500) != 200) {
        if (CGeneral::GetRandomNumberInRange(0, 100) == 50) {
            if (const auto holdTask = static_cast<CTaskSimpleHoldEntity*>(ped->GetTaskManager().FindActiveTaskByType(TASK_SIMPLE_HOLD_ENTITY /* 0x133 */))) { // 0x681740
                holdTask->PlayAnim(CTaskComplexGangLeader::GetRandomGangAmbientAnim(ped, heldEntity) /* 0x65E730 */, ANIM_GROUP_GANGS); // 0x691720
            }
        }
        return m_pSubTask;
    }

    // 0.2%: передать предмет
    if (bAnySmokingDrinkingAnim) {
        return m_pSubTask;
    }
    // 0x65EA50 = CTaskComplexGangLeader::TryToPassObject (this в оригинале не используется - только ped+group)
    const auto passTo = plugin::CallAndReturn<CPed*, 0x65EA50, CPed*, CPedGroup*>(ped, m_PedGroup);
    if (!passTo) {
        return m_pSubTask;
    }
    if (passTo->GetEntityThatThisPedIsHolding() /* 0x5E02E0 */ || !passTo->IsCurrentlyUnarmed()) {
        return m_pSubTask;
    }

    if (heldEntity->m_nModelIndex == ModelIndices::MI_GANG_DRINK) { // DAT_008CD728
        if (CGeneral::GetRandomNumberInRange(0, 500) < 250) {
            passTo->Say(CTX_GLOBAL_BOOZE_REQUEST, 0, 1.0f);    /* 24 */
        } else {
            ped->Say(CTX_GLOBAL_BOOZE_RECEIVE, 1500, 1.0f);    /* 23 */ // 0x5DC
        }
    } else if (heldEntity->m_nModelIndex == ModelIndices::MI_GANG_SMOKE) { // DAT_008CD72C
        if (CGeneral::GetRandomNumberInRange(0, 500) > 250) {
            ped->Say(CTX_GLOBAL_SPLIFF_RECEIVE, 1500, 1.0f);   /* 200 */ // 0x5DC
        } else {
            passTo->Say(CTX_GLOBAL_SPLIFF_REQUEST, 0, 1.0f);   /* 201 */
        }
    }

    passTo->GetEventGroup().Add(CEventPassObject{ ped, false }); // 0x4AB420 (ctor ивента 0x65DC70; 0x65DCF0 - dtor временного объекта)
    return new CTaskComplexPassObject{ passTo, true };           // 0x6611B0
}
