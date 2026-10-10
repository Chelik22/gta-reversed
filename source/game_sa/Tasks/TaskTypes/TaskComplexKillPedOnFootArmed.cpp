// ============================================================================
// INTEGRATED FILE — source/game_sa/Tasks/TaskTypes/TaskComplexKillPedOnFootArmed.cpp
// Функции: CreateSubTask (0x626FC0), CreateNextSubTask (0x62C190)
// (все прежние реверснутые функции файла сохранены без изменений)
//
// ИСТОЧНИКИ: фрагменты batch_out/fresh/0x626FC0.cpp и 0x62C190.cpp — оба
// верифицированы VERIFY-wf5a: 0x626FC0 — FIXED(3), 0x62C190 — FIXED(4). Правки
// верификации уже в телах фрагментов:
//  0x626FC0: (1) gun-command = LineOfSightClearForAttack(ped) ? FIREBURST : NONE;
//            (2) крик CTX_GLOBAL_SURROUNDED при группе после установки m_shootTimer;
//            (3) стороны обхода: sideA/B = target ± rot90(d)*1.5 (X <-> Y переставлены).
//  0x62C190: (1) m_fTargetRange вместо несуществующего m_fRange;
//            (2) кейс TASK_SIMPLE_THROW_CTRL (0x3FD) в общем обработчике;
//            (3) блок нового укрытия по !isWepMelee (не по LOS);
//            (4) стрейф-рандом: rand < 0x3FFF => RIGHT (не инверсия).
//
// ПРАВКИ ИНТЕГРАТОРА против фрагментов: тел не касались (кроме формата); сняты
// {.Reversed = false} у двух хуков. Литерал case (eTaskType)900 (0x384) сохранён как
// во фрагменте (== TASK_SIMPLE_GO_TO_POINT по Enums/eTaskType.h).
//
// Сверено в репо: поля класса (TaskComplexKillPedOnFootArmed.h:20-45 — m_shootTimer,
// m_lastAttackTime, m_losBlockedTime, m_needToMoveInCloserTime, m_lengthOfDuck,
// m_competence, m_strafeDir, m_bStrafeBack, m_aimImmediate и др.),
// CTaskSimpleGunControl ctor 6 аргов + m_isLOSBlocked/m_aimImmidiately
// (TaskSimpleGunControl.h:41-49), CTaskSimpleThrowControl(CEntity*, CVector const*)
// (TaskSimpleThrowControl.h:21), CTaskComplexSeekEntityStandard — унаследованный ctor
// CTaskComplexSeekEntity(8 аргов) (SeekEntity/TaskComplexSeekEntityStandard.h:10,
// TaskComplexSeekEntity.h:76,426), CTaskSimpleStandStill ctor + ProcessPed
// (TaskSimpleStandStill.h:23,30), CTaskSimpleDuck(eDuckControlType,uint16,int16)
// (TaskSimpleDuck.h:41, DUCK_STANDALONE=0), CTaskSimplePause(int32) (TaskSimplePause.h:10),
// CWorld::GetIsLineOfSightClear (World.h:111), CCover::* (Cover.h:38-41),
// CCoverPoint::eUsage/m_Usage (CoverPoint.h:30-60), CPedIntelligence::
// SetTaskDuckSecondary/ClearTaskDuckSecondary (PedIntelligence.h:109,110),
// CWanted::m_NumCopsInPursuit (Wanted.h:28), FindPlayerWanted (World.h:457),
// CPedGroups::GetPedsGroup (PedGroups.h:39), CTX_GLOBAL_* (PedSpeechContexts.h —
// SURROUNDED=207, SOLO=199, CHASED=44, CHASE_FOOT=43, MOVE_IN=144, DUCK=85, COVER_ME=65),
// CGeneral::GetRandomNumberInRange/GetRandomNumber (General.h:84,104),
// notsa::dyn_cast_if_present (extensions/Casting.hpp), plugin::Call<ADDR, Args...>
// (прецедент CarCtrl.cpp:171; 0x40FEC0 не реверснут — TransformFromMatrix-хелпер),
// CPlaceable::m_matrix публичен (Placeable.h:18).
// Константы: 0x858B3C=3.f 0x858C98=0.8f 0x858F84=23.f 0x858CC8=0.6f 0x858BA4=20.f
//            0x858CCC=12.f 0x858F34=0.75f 0x858B90=4.f 0x858C80=5.f 0x858624=1.f
//            0x85862C=10.f 0x858B44=6.f 0x86DB24=2000
// ============================================================================

#include "StdInc.h"

#include "TaskComplexKillPedOnFootArmed.h"
#include "TaskSimpleGoToPoint.h"
#include "TaskSimpleDuck.h"
#include "TaskSimpleGunControl.h"
#include "TaskSimpleUseGun.h"
#include "TaskSimpleThrowControl.h"
#include "TaskSimplePause.h"
#include "TaskSimpleStandStill.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "SeekEntity/TaskComplexSeekEntityStandard.h"
#include <extensions/utility.hpp>

void CTaskComplexKillPedOnFootArmed::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexKillPedOnFootArmed, 0x86d918, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x621190);
    RH_ScopedInstall(Destructor, 0x621250);

    RH_ScopedInstall(LineOfSightClearForAttack, 0x621500);
    RH_ScopedInstall(IsPedInLeaderFiringLine, 0x621300);
    RH_ScopedInstall(CreateSubTask, 0x626FC0);

    RH_ScopedVMTInstall(Clone, 0x6234C0);
    RH_ScopedVMTInstall(GetTaskType, 0x621240);
    RH_ScopedVMTInstall(MakeAbortable, 0x6212B0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x62C190);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x62BF00);
    RH_ScopedVMTInstall(ControlSubTask, 0x62CCE0);
}

// 0x621190
CTaskComplexKillPedOnFootArmed::CTaskComplexKillPedOnFootArmed(
    CPed*  target,
    uint32 duckingConditions,
    uint32 duckTime,
    uint32 duckChancePerc,
    int8   competence
) :
    m_target{ target },
    m_duckingConditions{ duckingConditions },
    m_lengthOfDuck{ duckTime },
    m_duckChancePerc{ duckChancePerc },
    m_competence{ competence }
{
    CEntity::SafeRegisterRef(m_target);
}

// notsa
CTaskComplexKillPedOnFootArmed::CTaskComplexKillPedOnFootArmed(const CTaskComplexKillPedOnFootArmed& o) :
    CTaskComplexKillPedOnFootArmed{
        o.m_target,
        o.m_duckingConditions,
        o.m_lengthOfDuck,
        o.m_duckChancePerc,
        o.m_competence
    }
{
    m_aimImmediate = o.m_aimImmediate;
}

// 0x621250
CTaskComplexKillPedOnFootArmed::~CTaskComplexKillPedOnFootArmed() {
    CEntity::SafeCleanUpRef(m_target);
}

// 0x621500
bool CTaskComplexKillPedOnFootArmed::LineOfSightClearForAttack(CPed* ped) { // ped is the task owner ped
    /*
    * TODO:
    * here's some code to calculate the values to use (based on some variables)
    * nothing special, but it's messy, and it's late, so I won't bother
    */
    const auto reqDistDeltaSq = sq(2.f);
    const auto reqTimeDelta   = 5000;

    //> If the LOS was recently clear, let's consider it still is
    if (CTimer::GetTimeInMS() - m_losClearTime < reqTimeDelta) {
        return true;
    }

    //> Perhaps check if the entities are still kinda around the same position as the last time the LOS was blocked...
    if (CTimer::GetTimeInMS() - m_losBlockedTime < reqTimeDelta) {
        if (reqDistDeltaSq >= (m_target->GetPosition() - m_losBlockedTargetPos).SquaredMagnitude()) {
            if (reqDistDeltaSq >= (ped->GetPosition() - m_losBlockedOurPos).SquaredMagnitude()) {
                return false;
            }
        }
    }

    //> 0x6216DD
    // Temporarily disable the target ped vehicle's collision (To ignore it)
    // Perhaps, `CWorld::pIgnoreEntity` could be used?
    const auto targetVeh = m_target->GetVehicleIfInOne();
    const notsa::ScopeGuard restore{[targetVeh, had = targetVeh && targetVeh->GetUsesCollision()] {
        if (targetVeh) {
            targetVeh->SetUsesCollision(had);
        }
    }};

    if (targetVeh) {
        targetVeh->SetUsesCollision(false);
    }

    const auto GetPedHeadPos = [](CPed* headOf) {
        CVector inout{ 0.1f, 0.f, 0.f };
        headOf->GetTransformedBonePosition(inout, BONE_HEAD);
        return inout;
    };

    if (CWorld::GetIsLineOfSightClear(
        ped->bIsDucking
            ? GetPedHeadPos(ped)
            : ped->GetPosition() + CVector{0.f, 0.f, 0.25f},
        GetPedHeadPos(m_target), // Always use head position
        true,
        true,
        false,
        true,
        false,
        true,
        false
    )) {
        m_losClearTime   = CTimer::GetTimeInMS();
        m_losBlockedTime = 0;
        return true;
    } else {
        m_losBlockedTime      = CTimer::GetTimeInMS();
        m_losClearTime        = 0;
        m_losBlockedOurPos    = ped->GetPosition();
        m_losBlockedTargetPos = m_target->GetPosition();
        return false;
    }
}

// 0x621300
bool CTaskComplexKillPedOnFootArmed::IsPedInLeaderFiringLine(CPed* ped) {
    const auto pedGrp = ped->GetGroup();
    if (!pedGrp) {
        return false;
    }

    const auto grpLeaderPlyr = pedGrp->GetMembership().GetLeader();
    if (!grpLeaderPlyr || !grpLeaderPlyr->IsPlayer()) {
        return false;
    }

    if (!grpLeaderPlyr->m_pTargetedObject || grpLeaderPlyr->GetActiveWeapon().IsTypeMelee()) {
        return false;
    }

    const auto &leaderPos2D      = grpLeaderPlyr->GetPosition2D();
    const auto &leaderPos        = grpLeaderPlyr->GetPosition();
    const auto leaderToPed       = ped->GetPosition() - leaderPos, // 0x6213BD
               leaderToTargetDir = (grpLeaderPlyr->m_pTargetedObject->GetPosition() - leaderPos).Normalized(); // 0x621394

    /* clang-format off
     * --[projPointOnLeaderToTargetRay2D]-->[leaderPos]---[leaderToTargetDir]-->[Target]
     *                                          /
     *                                         /
     *                                        /
     *                                       /
     *                                 [leaderToPed]
     *                                     /
     *                                    /
     *                                  \|/
     *                                [Ped]
     * clang-format on */

    //> 0x6213AE
    const auto projPointOnLeaderToTargetRay2D = leaderPos + CVector2D{ leaderToPed }.ProjectOnToNormal(leaderToTargetDir);
    if ((projPointOnLeaderToTargetRay2D - ped->GetPosition2D()).SquaredMagnitude() >= sq(2.f)) {
        return false;
    }

    //> 0x0621482
    if (leaderToTargetDir.Dot(leaderToPed) <= 0.f) { // Ped is "behind" leader (Like on the ASCII art above)
        return false;
    }

    //> 0x6214CD
    if (leaderToPed.SquaredMagnitude() >= sq(10.f)) {
        return false;
    }

    return true;
}

// 0x626FC0
CTask* CTaskComplexKillPedOnFootArmed::CreateSubTask(eTaskType taskType, CPed* ped) {
    const auto& wep     = ped->m_aWeapons[ped->m_nActiveWeaponSlot];
    const auto  wepInfo = CWeaponInfo::GetWeaponInfo(wep.m_Type, ped->GetWeaponSkill());

    switch (taskType) {
    case TASK_SIMPLE_GUN_CTRL: { // 0x3FC
        CTask* task;
        if (!wepInfo->flags.bThrow) { // бит 8 флагов (0x18)
            // ABI: в оригинале вместо пустых CVector передаются nullptr (см. Constructor-обёртку в TaskSimpleGunControl.h)
            // Оригинал: gunCommand = LineOfSightClearForAttack(ped) ? FIREBURST(3) : NONE(0) (FUN_00621500)
            const auto gunCmd = LineOfSightClearForAttack(ped) ? eGunCommand::FIREBURST : eGunCommand::NONE;
            const auto gctrl = new CTaskSimpleGunControl(m_target, CVector{}, CVector{}, gunCmd, 5, -1);
            gctrl->m_aimImmidiately = m_aimImmediate != false;
            m_aimImmediate  = false;
            m_shootTimer    = CTimer::GetTimeInMS() + CGeneral::GetRandomNumberInRange(4000, 8000);
            // Оригинал: if (CPedGroups::GetPedsGroup(ped)) ped->Say(0xCF = CTX_GLOBAL_SURROUNDED)
            if (CPedGroups::GetPedsGroup(ped)) {
                ped->Say(CTX_GLOBAL_SURROUNDED);
            }
            task = gctrl;
        } else {
            task = new CTaskSimpleThrowControl(m_target, nullptr); // метательное оружие
        }
        m_lastAttackTime = CTimer::GetTimeInMS();
        m_losBlockedTime = 0;
        return task;
    }
    case TASK_COMPLEX_SEEK_ENTITY: { // 0x38B
        const auto CreateSeek = [&](float maxEntityDist2D) {
            return new CTaskComplexSeekEntityStandard(m_target, 50000, 1000, maxEntityDist2D, 2.f, 2.f, true, true);
        };

        CTask* task{};
        if (m_needToMoveInCloserTime != 0) {
            const auto elapsed = (float)(CTimer::GetTimeInMS() - m_needToMoveInCloserTime); // беззнаковое
            if (elapsed >= 3000.f) {
                if (elapsed <= 8000.f) {
                    task = CreateSeek(6.f - (elapsed - 3000.f) * 0.001f);
                } else {
                    // Оригинал: sideA = targetPos + rot90(targetPos - pedPos)*1.5, где rot90(dx,dy)=(-dy,dx):
                    //   local_38 = -(target.y - ped.y)*1.5 -> в X; local_40 = (target.x - ped.x)*1.5 -> в Y
                    const auto& targetPos = m_target->GetPosition();
                    const auto  perpX     = (targetPos.x - ped->GetPosition().x) * 1.5f; // local_40 (-> Y)
                    const auto  perpY     = (ped->GetPosition().y - targetPos.y) * 1.5f; // local_38 (-> X)

                    const CVector sideA{ targetPos.x + perpY, targetPos.y + perpX, targetPos.z };
                    const CVector sideB{ targetPos.x - perpY, targetPos.y - perpX, targetPos.z };

                    if (CWorld::GetIsLineOfSightClear(targetPos, sideA, true, true, false, false, true, false, false)) {
                        task = new CTaskComplexGoToPointAndStandStill(PEDMOVE_RUN, sideA, 0.5f, 2.f, false, false);
                    } else if (CWorld::GetIsLineOfSightClear(targetPos, sideB, true, true, false, false, true, false, false)) {
                        task = new CTaskComplexGoToPointAndStandStill(PEDMOVE_RUN, sideB, 0.5f, 2.f, false, false);
                    } else {
                        task = CreateSeek(1.f);
                    }
                }
            } else {
                task = CreateSeek(6.f);
            }
        } else {
            task = CreateSeek(6.f);
        }

        // Аудио: коп кричит при атаке игрока
        if (m_target && m_target->IsPlayer()) {
            if (ped->m_nPedType == PED_TYPE_COP) {
                if (FindPlayerWanted()->m_NumCopsInPursuit < 2) {
                    if (ped->Say(CTX_GLOBAL_SOLO) > -1) {
                        ped->Say(CTX_GLOBAL_CHASED, 3500); // 0xDAC
                    }
                } else {
                    if (ped->Say(CTX_GLOBAL_CHASE_FOOT) > -1) {
                        ped->Say(CTX_GLOBAL_CHASED, 3500);
                    }
                }
            } else {
                if (CPedGroups::GetPedsGroup(ped)) {
                    ped->Say(CTX_GLOBAL_CHASE_FOOT);
                }
            }
        }
        return task;
    }
    case TASK_SIMPLE_PAUSE: { // 0xCA
        // Оригинал: временная CTaskSimpleStandStill -> ProcessPed(ped) -> уничтожение
        CTaskSimpleStandStill standStill{ 0, false, false, 8.f };
        standStill.ProcessPed(ped);
        return new CTaskSimplePause(100);
    }
    case TASK_SIMPLE_STAND_STILL: // 0xCB
        return new CTaskSimpleStandStill(2000, false, false, 8.f); // DAT_0086DB24 = 2000
    case TASK_SIMPLE_DUCK: // 0x19F
        return new CTaskSimpleDuck(DUCK_STANDALONE, (uint16)m_lengthOfDuck, -1);
    default:
        return nullptr;
    }
}

// 0x6212B0
bool CTaskComplexKillPedOnFootArmed::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    switch (priority) {
    case ABORT_PRIORITY_URGENT: {
        if (const auto aimedAtEvent = notsa::dyn_cast_if_present<const CEventGunAimedAt>(event)) {
            if (aimedAtEvent->m_AimedBy == m_target) {
                return false;
            }
        }
        break;
    }
    case ABORT_PRIORITY_IMMEDIATE:
        break;
    case ABORT_PRIORITY_LEISURE:
    default:
        return false;
    }
    return m_pSubTask->MakeAbortable(ped, priority, event);
}

// 0x62C190
CTask* CTaskComplexKillPedOnFootArmed::CreateNextSubTask(CPed* ped) {
    if (!m_target) {
        return nullptr;
    }

    auto& activeWep = ped->m_aWeapons[ped->m_nActiveWeaponSlot]; // IsTypeMelee() -- non-const
    const auto  wepInfo   = CWeaponInfo::GetWeaponInfo(activeWep.m_Type, ped->GetWeaponSkill());
    const auto  weaponRange    = wepInfo->m_fTargetRange; // оригинал: *(float*)(wepInfo+4) = m_fTargetRange (поля m_fRange в CWeaponInfo нет)
    const bool  isWepMelee     = activeWep.IsTypeMelee(); // 0x73B1C0
    const auto  dist           = (m_target->GetPosition() - ped->GetPosition()).Magnitude();

    const auto CreateSeek = [&](float maxEntityDist2D) {
        return new CTaskComplexSeekEntityStandard(m_target, 50000, 1000, maxEntityDist2D, 2.f, 2.f, true, true);
    };

    CTask* result{};

    const auto subTaskType = m_pSubTask->GetTaskType();

    // Подзадачи "идём/ищем" (903 - GO_TO_POINT_AND_STAND_STILL, 907 - SEEK_ENTITY)
    if (subTaskType == TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL || subTaskType == TASK_COMPLEX_SEEK_ENTITY) {
        if (LineOfSightClearForAttack(ped)) {
            m_needToMoveInCloserTime = 0;
            return CreateSubTask(TASK_SIMPLE_GUN_CTRL, ped);
        }
        result = CreateSubTask(TASK_SIMPLE_PAUSE, ped);
        if (m_needToMoveInCloserTime == 0) {
            m_needToMoveInCloserTime = CTimer::GetTimeInMS();
        }
        return result;
    }

    switch (subTaskType) {
    case (eTaskType)900: // 0x384
        if (LineOfSightClearForAttack(ped)) {
            m_needToMoveInCloserTime = 0;
            return CreateSubTask(TASK_SIMPLE_GUN_CTRL, ped);
        }
        m_needToMoveInCloserTime = 0;
        return CreateSubTask(TASK_SIMPLE_PAUSE, ped);

    case TASK_SIMPLE_PAUSE:
    case TASK_SIMPLE_STAND_STILL: // 0xCA / 0xCB
        if (ped->bStayInSamePlace || ped->bKindaStayInSamePlace) { // bStayInSamePlace | bKindaStayInSamePlace
            const auto distSq = (m_target->GetPosition() - ped->GetPosition()).SquaredMagnitude(); // 0x40FE60 + 0x406DA0
            const auto type   = (weaponRange * weaponRange <= distSq || !LineOfSightClearForAttack(ped))
                ? TASK_SIMPLE_PAUSE
                : TASK_SIMPLE_GUN_CTRL;
            result = CreateSubTask(type, ped);
            m_needToMoveInCloserTime = 0;
            return result;
        }
        ped->Say(CTX_GLOBAL_MOVE_IN);
        result = CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
        if (m_needToMoveInCloserTime == 0) {
            m_needToMoveInCloserTime = CTimer::GetTimeInMS();
        }
        return result;

    case TASK_SIMPLE_DUCK: { // 0x19F
        const auto los = LineOfSightClearForAttack(ped);
        result = CreateSubTask(los ? TASK_SIMPLE_GUN_CTRL : TASK_SIMPLE_PAUSE, ped);
        ped->bIsDucking = false;
        m_needToMoveInCloserTime = 0;
        return result;
    }

    case TASK_FINISHED: // 0x516
        m_needToMoveInCloserTime = 0;
        return nullptr;

    case TASK_COMPLEX_GO_TO_POINT_SHOOTING: // 0x385
    case TASK_SIMPLE_GUN_CTRL:              // 0x3FC
    case TASK_SIMPLE_THROW_CTRL:            // 0x3FD - оригинал пускает в общий обработчик и его (декомпил: пропускает только < 0x3FC и == 0x516)
        break;

    default:
        m_needToMoveInCloserTime = 0;
        return nullptr;
    }

    // Общий обработчик (стреляем на ходу / gun control)
    if (ped->bStayInSamePlace) {
        result = CreateSubTask(TASK_SIMPLE_PAUSE, ped);
        m_lastAttackTime = CTimer::GetTimeInMS();
    } else {
        if (dist < 3.f) {
            result = CreateSubTask(TASK_SIMPLE_GUN_CTRL, ped);
            if (!result) {
                result = CreateSubTask(TASK_SIMPLE_PAUSE, ped);
                m_lastAttackTime = CTimer::GetTimeInMS();
            }
        } else {
            const auto maxSeekDist = std::min(weaponRange * 0.8f, 23.f);
            if (maxSeekDist < dist) {
                // Слишком далеко
                if (ped->bKindaStayInSamePlace) { // bKindaStayInSamePlace
                    result = CreateSubTask(TASK_SIMPLE_PAUSE, ped);
                    m_lastAttackTime = CTimer::GetTimeInMS();
                } else {
                    auto seekDist = weaponRange * 0.6f;
                    if (seekDist >= 20.f) {
                        seekDist = 20.f;
                    }
                    result = CreateSeek(seekDist);
                    if (!result) {
                        result = CreateSubTask(TASK_SIMPLE_PAUSE, ped);
                        m_lastAttackTime = CTimer::GetTimeInMS();
                    }
                }
            } else {
                const auto gunCtrl = notsa::dyn_cast_if_present<CTaskSimpleGunControl>(m_pSubTask);
                if (gunCtrl && gunCtrl->m_isLOSBlocked && !(ped->bStayInSamePlace || ped->bKindaStayInSamePlace)) {
                    // Целься, но LOS заблокирован - двигаемся ближе
                    ped->Say(CTX_GLOBAL_MOVE_IN);
                    result = CreateSeek(2.f);
                    if (!result) {
                        goto NoTask; // 0x62C849
                    }
                    goto FinalCheck; // 0x62CC6C
                } else if (!isWepMelee && ped->m_pCoverPoint) {
                    // Укрывание
                    if (CCover::DoesCoverPointStillProvideCover(ped->m_pCoverPoint, m_target->GetPosition())) {
                        CVector coverPos{};
                        CCover::FindCoordinatesCoverPoint(*ped->m_pCoverPoint, ped, m_target->GetPosition(), coverPos);
                        if ((coverPos - ped->GetPosition()).Magnitude2D() < 0.75f) {
                            if (ped->m_pCoverPoint->GetUsage() == CCoverPoint::eUsage::LOWCOVER) {
                                // Низкое укрытие -> пригибаемся
                                if (dist < 12.f
                                    && !ped->bNotAllowedToDuck      // !bNotAllowedToDuck (байт 0x46F)
                                    && CGeneral::GetRandomNumber() % 4 == 0
                                    && !ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_DUCK)
                                ) {
                                    ped->Say(CTX_GLOBAL_DUCK);
                                    result = new CTaskSimpleDuck(DUCK_STANDALONE, 2500, -1);
                                    if (!result) {
                                        ped->ReleaseCoverPoint();
                                        goto NoTask;
                                    }
                                    goto FinalCheck;
                                }
                            } else if (CGeneral::GetRandomNumber() % 2 == 0 && !wepInfo->flags.bThrow) {
                                // Стреляем от укрытия
                                // ABI: в оригинале вместо пустых CVector передаются nullptr
                                const auto gctrl = new CTaskSimpleGunControl(m_target, CVector{}, CVector{}, eGunCommand::FIREBURST, 5, -1);
                                gctrl->m_aimImmidiately = m_aimImmediate != false;
                                m_aimImmediate     = false;
                                m_shootTimer       = CTimer::GetTimeInMS() + 5000;
                                m_lastAttackTime   = CTimer::GetTimeInMS();
                                m_lastStrafeTime   = CTimer::GetTimeInMS() + 2500;
                                m_strafeDir        = eStrafeDir::RIGHT;
                                m_bStrafeBack      = true;
                                if (ped->m_pCoverPoint->GetUsage() == CCoverPoint::eUsage::WALLTOLEFT) {
                                    m_strafeDir = eStrafeDir::LEFT;
                                }
                                ped->GetIntelligence()->ClearTaskDuckSecondary();
                                result = gctrl;
                                goto FinalCheck;
                            }
                        }
                        ped->ReleaseCoverPoint();
                    }
                }

            NoTask: // 0x62C849
                // Оригинал (0x62C849): условие cVar4 == 0 - это isWepMelee == false (cVar4 держит
                // результат IsTypeMelee с пролога, LOS тут не вызывался)
                if (m_competence > 0
                    && dist > 6.f
                    && !isWepMelee
                    && CGeneral::GetRandomNumber() % 2 == 0
                ) {
                    // Ищем новое укрытие
                    ped->ReleaseCoverPoint();
                    ped->m_pCoverPoint = CCover::FindAndReserveCoverPoint(ped, m_target->GetPosition(), m_competence == 2);
                    if (ped->m_pCoverPoint) {
                        CVector newCoverPos{};
                        CCover::FindCoordinatesCoverPoint(*ped->m_pCoverPoint, ped, m_target->GetPosition(), newCoverPos);
                        newCoverPos.z += 1.f;
                        if ((newCoverPos - m_target->GetPosition()).Magnitude2D() < weaponRange * 0.75f) {
                            if (CWorld::GetIsLineOfSightClear(newCoverPos, ped->GetPosition(), true, true, false, false, false, false, false)) {
                                if (CPedGroups::GetPedsGroup(ped)) {
                                    ped->Say(CTX_GLOBAL_COVER_ME);
                                }
                                ped->GetIntelligence()->SetTaskDuckSecondary(6000);
                                result = new CTaskSimpleGoToPoint(PEDMOVE_RUN, newCoverPos, 0.5f, true, false);
                                if (result) {
                                    goto FinalCheck;
                                }
                            }
                        }
                        ped->ReleaseCoverPoint();
                    }
                }

                // 0x62C9E5: стрейф
                if (dist > 10.f) {
                    switch (CGeneral::GetRandomNumber() % 4) {
                    case 0:
                        if (!(ped->bKindaStayInSamePlace)) {
                            ped->Say(CTX_GLOBAL_MOVE_IN);
                            result = CreateSeek(dist - 4.f);
                        }
                        break;
                    case 1:
                        if (!(ped->bKindaStayInSamePlace)) {
                            m_strafeDir      = eStrafeDir::FORWARD;
                            m_lastStrafeTime = CTimer::GetTimeInMS() + 2000;
                            m_bStrafeBack    = false;
                        }
                        break;
                    }
                }
                if (dist > 5.f && CGeneral::GetRandomNumber() % 4 == 0) {
                    m_strafeDir = CGeneral::GetRandomNumber() % 2 != 0
                        ? eStrafeDir::RIGHT
                        : eStrafeDir::LEFT;
                    const auto strafeBasePos = ped->GetPosition();
                    CVector    strafeOffset{};
                    plugin::Call<0x40FEC0, CVector*, CMatrix*, float>(&strafeOffset, ped->m_matrix, 2.5f); // нереверснутый хелпер
                    const auto strafeEndPos = m_strafeDir == eStrafeDir::RIGHT
                        ? strafeBasePos + strafeOffset
                        : strafeBasePos - strafeOffset;
                    if (!CWorld::GetIsLineOfSightClear(strafeBasePos, strafeEndPos, true, true, false, true, false, false, false)) {
                        m_strafeDir = m_strafeDir == eStrafeDir::RIGHT
                            ? eStrafeDir::LEFT
                            : eStrafeDir::RIGHT;
                    }
                    m_lastStrafeTime = CTimer::GetTimeInMS() + 2000;
                    m_bStrafeBack    = false;
                }
                if (!result) {
                    if (!LineOfSightClearForAttack(ped)) {
                        if (CGeneral::GetRandomNumber() % 4 == 0) {
                            // Оригинал: m_strafeDir = (rand < 0x3FFF) => 1 => RIGHT (eStrafeDir: LEFT=0, RIGHT=1)
                            m_strafeDir      = CGeneral::GetRandomNumber() < 0x3FFF
                                ? eStrafeDir::RIGHT
                                : eStrafeDir::LEFT;
                            m_lastStrafeTime = CTimer::GetTimeInMS() + 2000;
                            m_bStrafeBack    = false;
                        }
                        result = CreateSubTask(TASK_SIMPLE_PAUSE, ped);
                        m_lastAttackTime = CTimer::GetTimeInMS();
                    } else {
                        result = CreateSubTask(TASK_SIMPLE_GUN_CTRL, ped);
                        if (!result) {
                            result = CreateSubTask(TASK_SIMPLE_PAUSE, ped);
                            m_lastAttackTime = CTimer::GetTimeInMS();
                        }
                    }
                }
            }
        }
    }

FinalCheck: // 0x62CC6C
    if (CGeneral::GetRandomNumber() % 2 != 0
        && m_competence > 0
        && !ped->bNotAllowedToDuck // !bNotAllowedToDuck
        && (!ped->m_pCoverPoint
            || (ped->m_pCoverPoint->GetUsage() != CCoverPoint::eUsage::WALLTOLEFT
                && ped->m_pCoverPoint->GetUsage() != CCoverPoint::eUsage::WALLTORIGHT))
    ) {
        ped->GetIntelligence()->SetTaskDuckSecondary(6000);
        m_needToMoveInCloserTime = 0;
        return result;
    }
    m_needToMoveInCloserTime = 0;
    return result;
}

// 0x62BF00
CTask* CTaskComplexKillPedOnFootArmed::CreateFirstSubTask(CPed* ped) {
    if (!m_target) {
        return nullptr;
    }

    const auto &ourPos    = ped->GetPosition(),
               &targetPos = m_target->GetPosition();

    const auto targetToOurPedDistSq = (ourPos - targetPos).SquaredMagnitude();

    if (   !ped->bStayInSamePlace
        && CGeneral::RandomBool(50)
        && m_competence > 0
        && (targetToOurPedDistSq >= sq(30.f) || targetToOurPedDistSq >= sq(6.f) && !m_target->GetActiveWeapon().IsTypeMelee()) 
    ) {
        ped->ReleaseCoverPoint();
        if (ped->m_pCoverPoint = CCover::FindAndReserveCoverPoint(ped, targetPos, false)) {
            CVector coverPos{};
            if (!notsa::IsFixBugs() || CCover::FindCoordinatesCoverPoint(*ped->m_pCoverPoint, ped, targetPos, coverPos)) {
                if (CWorld::GetIsLineOfSightClear(coverPos, ourPos, true, true, false, false, false, false, false)) {
                    ped->GetIntelligence()->SetTaskDuckSecondary(6000);
                    if (const auto task = new CTaskSimpleGoToPoint{
                        PEDMOVE_RUN,
                        coverPos,
                        0.5f,
                        true
                    }) { // I can't believe my eyes.... error handling?
                        return task;
                    }
                } else {
                    ped->ReleaseCoverPoint();
                }
            } else {
                NOTSA_LOG_WARN("Can't find cover point's coordinates!"); // Originally the game has left `coverPos` uninitialized in this case
            }
        }
    }

    return CreateSubTask([&, this] {
        const auto& ourActiveWep = ped->GetActiveWeapon().GetWeaponInfo(ped);

        if (targetToOurPedDistSq <= sq(ourActiveWep.m_fTargetRange / 4.f) && LineOfSightClearForAttack(ped)) {
            return TASK_SIMPLE_GUN_CTRL;
        }

        if (!ped->bStayInSamePlace && !ped->bKindaStayInSamePlace) {
            return TASK_COMPLEX_SEEK_ENTITY;
        }

        if (LineOfSightClearForAttack(ped) || ourActiveWep.flags.bThrow) {
            return TASK_SIMPLE_PAUSE;
        }

        return TASK_SIMPLE_GUN_CTRL;
    }(), ped);
}

// 0x62CCE0
CTask* CTaskComplexKillPedOnFootArmed::ControlSubTask(CPed* ped) {
    if (m_newTarget) {
        return CreateFirstSubTask(ped);
    }

    if (!m_target || m_target->m_fHealth <= 0.f) {
        return nullptr;
    }
    const auto ogSubTask = m_pSubTask;

    const auto &ourPos    = ped->GetPosition(),
               &targetPos = m_target->GetPosition();

    const auto targetToOurPedDistSq = (ourPos - targetPos).SquaredMagnitude();

    //> 0x62CD55
    if (   (m_duckingConditions & 4) == 0
        || CTimer::GetTimeInMS() <= m_lastDuckTime + m_lengthOfDuck + 2000
        || ped->bIsDucking
    ) {
        if (   m_duckingConditions & 1
            && m_bShotFiredByPlayer
            && CGeneral::RandomBool((float)m_duckChancePerc)
        ) {
            if (const auto quack = ped->GetIntelligence()->GetTaskDuck()) {
                quack->SetDuckTimer((uint16)m_lengthOfDuck);
            } else {
                ped->GetIntelligence()->SetTaskDuckSecondary((uint16)m_lengthOfDuck);
#ifdef FIX_BUGS
                m_lastDuckTime = CTimer::GetTimeInMS(); // Not actually sure if this is necessary tbh
#endif
            }
        }
    } else {
        if (CGeneral::RandomBool((float)m_duckChancePerc)) {
            if (targetToOurPedDistSq <= sq(20.f)) {
                ped->GetIntelligence()->SetTaskDuckSecondary((uint16)m_lengthOfDuck);
            }
        }
        m_lastDuckTime = CTimer::GetTimeInMS();
    }

    //> 0x62CE56
    const auto quack = ped->GetIntelligence()->GetTaskDuck();
    if (quack) {
        quack->m_bIsInControl = true;
    }

    //> 0x62CE6E
    if (m_pSubTask != ogSubTask) { // ?????
        return ogSubTask;
    }

    const auto& ourwi = ped->GetActiveWeapon().GetWeaponInfo(ped);
    switch (m_pSubTask->GetTaskType()) { //> 0x62CE7E
    case TASK_COMPLEX_SEEK_ENTITY: {
        if ([&, this]{
            if (IsPedInLeaderFiringLine(ped)) {
                return true;
            }
            if (m_target->physicalFlags.bSubmergedInWater && LineOfSightClearForAttack(ped)) {
                return true;
            }
            if (ped->bStayInSamePlace) {
                return LineOfSightClearForAttack(ped);
            } else if (sq(ourwi.m_fTargetRange / 2.f) >= targetToOurPedDistSq) {
                if (CTimer::GetTimeInMS() - m_lastAttackTime >= 2000 || m_target->GetMoveSpeed().Dot(ped->GetForward()) < 0.f) {
                    return LineOfSightClearForAttack(ped);
                }
            }
            return false;
        }()) {
            return CreateSubTask(TASK_SIMPLE_GUN_CTRL, ped);
        } else {
            if (ped->GetGroup()) {
                ped->Say(CTX_GLOBAL_COVER_ME);
            }
        }
        break;
    }
    case TASK_SIMPLE_GUN_CTRL: { //> 0x62CE87
        if (   targetToOurPedDistSq >= sq(ourwi.m_fTargetRange)
            || targetToOurPedDistSq >= sq(ourwi.m_fTargetRange / 2.f) && !ped->bStayInSamePlace && CTimer::GetTimeInMS() - m_lastAttackTime >= 2000
            || targetToOurPedDistSq >= sq(4.f) && !ped->bStayInSamePlace && CTimer::GetTimeInMS() >= m_shootTimer
            || !LineOfSightClearForAttack(ped)
        ) {
            const auto gctrl = notsa::cast<CTaskSimpleGunControl>(m_pSubTask);
            if (gctrl->m_firingTask != eGunCommand::END_LEISURE) {
                gctrl->m_nextAtkTimeMs = 0;
                gctrl->m_firingTask    = eGunCommand::END_LEISURE;
            }
        }

        const auto ugun = ped->GetIntelligence()->GetTaskUseGun();
        if (!ugun) {
            break;
        }

        if (ped->GetGroup()) {
            ped->Say(CTX_GLOBAL_SURROUNDED);
        }

        //> 0x62CF88
        const auto actionDir = [&, this]() -> CVector2D {
            if (m_target->physicalFlags.bSubmergedInWater) {
                return { 0.f, 0.f };
            }
            if (CTimer::GetTimeInMS() >= m_lastStrafeTime || !ped->bStayInSamePlace) {
                if (targetToOurPedDistSq <= sq(4.f) && ped->bStayInSamePlace) {
                    return { 0.f, 1.f }; // Forwards
                }
                if (m_bStrafeBack) {
                    m_bStrafeBack    = false;
                    m_lastStrafeTime = CTimer::GetTimeInMS() + 2500;
                    m_strafeDir      = [&, this] { // Strafe back to where we came from the last time
                        switch (m_strafeDir)
                        {
                        case eStrafeDir::LEFT:    return eStrafeDir::RIGHT;
                        case eStrafeDir::RIGHT:   return eStrafeDir::LEFT;
                        case eStrafeDir::FORWARD: return eStrafeDir::BACK;
                        case eStrafeDir::BACK:    return eStrafeDir::FORWARD;
                        default:                  NOTSA_UNREACHABLE();
                        }
                    }();
                }
                return { 0.f, 0.f };
            } else {
                switch (m_strafeDir)
                {
                case eStrafeDir::LEFT:    return { -1.f,  0.f };
                case eStrafeDir::RIGHT:   return {  1.f,  0.f };
                case eStrafeDir::FORWARD: return {  0.f, -1.f };
                case eStrafeDir::BACK:    return {  0.f,  1.f };
                default:                  NOTSA_UNREACHABLE();
                }
            }
        }();

        //> 0x62D080
        if (quack) {
            quack->ControlDuckMove([&, this] {
                if (notsa::contains(std::to_array({ eStrafeDir::LEFT, eStrafeDir::RIGHT }), m_strafeDir)) {
                    if (m_lastRollTime + 3000 >= CTimer::GetTimeInMS()) {
                        return CVector2D{ 0.f, 0.f };
                    } else {
                        m_lastRollTime = CTimer::GetTimeInMS();
                    }
                }
                return actionDir;
            }());
        } else {
            ugun->ControlGunMove(actionDir);
        }

        break;
    }
    }

    return m_pSubTask;
}
