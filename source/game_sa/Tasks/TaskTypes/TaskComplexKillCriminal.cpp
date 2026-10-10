// ============================================================================
// INTEGRATED FILE — source/game_sa/Tasks/TaskTypes/TaskComplexKillCriminal.cpp
// Функция: CreateFirstSubTask (0x68DC60) — все прежние функции файла сохранены.
//
// ИСТОЧНИК: фрагмент batch_out/fresh/0x68DC60.cpp — VERIFY-wf5a: FIXED(5) + 1 SUSPECT.
// Фиксы верификации уже в теле фрагмента:
//  (1) флаг машины = vehicleFlags.bIsBeingCarJacked (байт +0x42E & 8 = бит 19), не bCanPark;
//  (2) partner->m_fHealth < 0.f (строго);
//  (3) SetPartner/AddCriminalToKill — методы CCopPed (CopPed.h:40-41);
//  (4) ped->SetCurrentWeapon(WEAPON_PISTOL) после СОЗДАНИЯ нового
//      CTaskComplexKillPedOnFoot (общий хвост обеих пешеходных аллокаций, 0x68E29x);
//  (5) Finish: ветка else if (bIsBeingCarJacked) — пассажиры => все копы вооружаются
//      + AddCriminalToKill; преступник пешком => отдельная ветка rand&1.
//
// SUSPECT (из фрагмента, не восстановимо по декомпилу): в Finish-ветке «преступник
// пешком, rand&1» оригинал (0x68E337..0x68E355) создаёт стековый
// CEventAcquaintancePedHate (0x420E70, target = сам коп-param_2) и добавляет через
// CEventGroup::Add (0x4AB420) — this у Add в декомпиле потерян. НЕ включено.
//
// ПРАВКИ ИНТЕГРАТОРА против фрагмента (компиляционные):
//  - m_Cop в хедере репо — notsa::EntityRef<CCopPed> (TaskComplexKillCriminal.h);
//    `m_Cop = ped; CEntity::SafeRegisterRef(m_Cop);` заменено на
//    `m_Cop = static_cast<CCopPed*>(ped);` — operator=(T*) EntityRef сам делает
//    CleanUpOldReference+RegisterReference (EntityRef.hpp:43-51), SafeRegisterRef(T*&)
//    к EntityRef не привязывается;
//  - m_isTheDriver/m_pCopPartner объявлены на CCopPed (CopPed.h:28,30), а не на CPed —
//    доступы ped->m_isTheDriver / ped->m_pCopPartner заменены на копа-указатель
//    cop (static_cast<CCopPed*>(ped), тип проверен гейтом PED_TYPE_COP выше),
//    включая driver->m_isTheDriver -> static_cast<CCopPed*>(driver)->...;
//  - CInterestingEvents::Add — не-статический метод (InterestingEvents.h:78) —
//    заменено на g_InterestingEvents.Add(...) (extern в InterestingEvents.h:85,
//    прецеденты EventHandler.cpp:884, Explosion.cpp:394).
//
// Сверено в репо: notsa::EntityRef (extensions/EntityRef.hpp), CCopPed::SetPartner/
// AddCriminalToKill/m_pCopPartner/m_isTheDriver (CopPed.h:28,30,40,41), CPed::m_fHealth/
// bWantedByPolice/bDontDragMeOutCar/bCullExtraFarAway/m_nTimeTillWeNeedThisPed/
// m_fRemovalDistMultiplier/m_pVehicle/bInVehicle/m_pCopPartner нет — см. CopPed,
// GiveWeapon(eWeaponType,uint32,bool) (Ped.h:483), SetCurrentWeapon(eWeaponType)
// (Ped.h:489), GetPlayerData()->m_pWanted (PlayerPedData.h:21, Ped.h:533),
// eWantedLevel::WANTED_CLEAN (Enums/eWantedLevel.h:4), FindPlayerPed/FindPlayerWanted
// (World.h:446,457), CVehicle::m_pDriver/m_apPassengers/m_nMaxPassengers/m_nNumPassengers/
// m_nExtendedRemovalRange/vehicleFlags.bIsBeingCarJacked|bNeverUseSmallerRemovalRange|
// bSirenOrAlarm (Vehicle.h:273,294,304,332-338,350), CAutoPilot::m_nCarDrivingStyle/
// m_nCarMission/m_nCruiseSpeed (AutoPilot.h:66,67,76), CTaskSimpleCarDrive(veh,util,upd)
// (TaskSimpleCarDrive.h:43), CTaskComplexCarDriveMission(veh,target,mission,style,speed)
// (TaskComplexCarDriveMission.h:17), CTaskComplexLeaveCar(veh,0,0,true,false)
// (TaskComplexLeaveCar.h:34), CTaskComplexKillPedOnFoot(target,-1,0,0,0,1)
// (TaskComplexKillPedOnFoot.h:35), CEventAcquaintancePed(CPed*, eTaskType=TASK_NONE)
// (EventAcquaintancePed.h:15), CEventGroup::Add(CEvent*, bool=false) (EventGroup.h:19),
// CPedIntelligence::SetPedDecisionMakerType/FindTaskByType/m_eventGroup
// (PedIntelligence.h:43,81,97), notsa::cast_if_present (extensions/Casting.hpp:751),
// MISSION_KILLPED_CLOSE/MISSION_FOLLOWCAR_CLOSE/MISSION_BLOCKCAR_CLOSE (eCarMission),
// TASK_COMPLEX_KILL_CRIMINAL = 1105 (Enums/eTaskType.h).
// ============================================================================

#include "StdInc.h"

#include <extensions/utility.hpp>

#include "TaskComplexKillCriminal.h"
#include "TaskComplexKillPedOnFoot.h"
#include "TaskSimpleGangDriveBy.h"
#include "TaskComplexEnterCarAsPassenger.h"
#include "TaskComplexEnterCarAsDriver.h"
#include "TaskComplexLeaveCar.h"
#include "TaskSimpleCarDrive.h"
#include "TaskComplexCarDriveMission.h"
#include "Events/EventAcquaintancePedHate.h"
#include "InterestingEvents.h"


void CTaskComplexKillCriminal::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexKillCriminal, 0x870a00, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x68BE70);
    RH_ScopedInstall(Destructor, 0x68BF30);

    RH_ScopedInstall(CreateSubTask, 0x68C050);
    RH_ScopedInstall(FindNextCriminalToKill, 0x68C3C0);
    RH_ScopedInstall(ChangeTarget, 0x68C6E0);

    RH_ScopedVMTInstall(Clone, 0x68CE50);
    RH_ScopedVMTInstall(GetTaskType, 0x68BF20);
    RH_ScopedVMTInstall(MakeAbortable, 0x68DAD0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x68E4F0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x68DC60);
    RH_ScopedVMTInstall(ControlSubTask, 0x68E950);
}

bool NoPedOrNoHp(CPed* ped) {
    return !ped || ped->m_fHealth <= 0.f;
}

// 0x68BE70
CTaskComplexKillCriminal::CTaskComplexKillCriminal(CPed* criminal, bool randomize) :
    m_Randomize{randomize},
    m_Criminal{criminal}
{
    if (   !m_Criminal
        || m_Criminal->IsPlayer()
        || m_Criminal->IsCreatedByMission()
        || notsa::contains({ PED_TYPE_COP, PED_TYPE_MEDIC, PED_TYPE_FIREMAN, PED_TYPE_MISSION1 }, m_Criminal->m_nPedType)
    ) {
        m_Criminal = nullptr;
    }
}

// NOTSA (For 0x68CE50)
CTaskComplexKillCriminal::CTaskComplexKillCriminal(const CTaskComplexKillCriminal& o) :
    CTaskComplexKillCriminal{o.m_Criminal, false}
{
}

// 0x68BF30
CTaskComplexKillCriminal::~CTaskComplexKillCriminal() {
    if (m_Cop) {
        m_Cop->m_nTimeTillWeNeedThisPed = CTimer::GetTimeInMS();
        m_Cop->bCullExtraFarAway = false;
        m_Cop->m_fRemovalDistMultiplier = 1.f;
        if (const auto veh = m_Cop->m_pVehicle) {
            veh->m_nExtendedRemovalRange = false;
            veh->vehicleFlags.bNeverUseSmallerRemovalRange = false;
            if (veh->IsDriver(m_Cop)) {
                const auto ap = &veh->m_autoPilot;
                ap->SetCarMission(MISSION_CRUISE);
                ap->SetDrivingStyle(DRIVING_STYLE_AVOID_CARS);
                ap->SetCruiseSpeed(10);
                if (veh->GetStatus() != STATUS_SIMPLE) {
                    CCarCtrl::JoinCarWithRoadSystem(veh);
                }
                veh->vehicleFlags.bSirenOrAlarm = false;
            }
            veh->vehicleFlags.bSirenOrAlarm = false;
        }
    }
}

// 0x68C050
CTask* CTaskComplexKillCriminal::CreateSubTask(eTaskType tt, CPed* ped, bool force) {
    if (!force && m_pSubTask && m_pSubTask->GetTaskType() == tt) {
        return m_pSubTask;        
    }

    switch (tt) {
    case TASK_COMPLEX_KILL_PED_ON_FOOT: {
        ped->SetCurrentWeapon(WEAPON_PISTOL);
        return new CTaskComplexKillPedOnFoot{ m_Criminal };
    }
    case TASK_FINISHED: {
        if (m_Criminal) {
            m_Criminal->SetPedDefaultDecisionMaker();
        }
        return nullptr;
    }
    case TASK_SIMPLE_GANG_DRIVEBY:
        return new CTaskSimpleGangDriveBy{
            m_Criminal,
            nullptr,
            70.f,
            70,
            eDrivebyStyle::AI_ALL_DIRN,
            false
        };
    case TASK_COMPLEX_ENTER_CAR_AS_PASSENGER:
        return new CTaskComplexEnterCarAsPassenger{ ped->m_pVehicle };
    case TASK_COMPLEX_ENTER_CAR_AS_DRIVER:
        return new CTaskComplexEnterCarAsDriver{ ped->m_pVehicle };
    case TASK_COMPLEX_LEAVE_CAR:
        return new CTaskComplexLeaveCar{ ped->m_pVehicle, 0, 0, true, false };
    case TASK_SIMPLE_CAR_DRIVE:
        return new CTaskSimpleCarDrive{ ped->m_pVehicle };
    case TASK_COMPLEX_CAR_DRIVE_MISSION: {
        const auto oveh = ped->m_pVehicle; // (o)ur (veh)icle
        if (!oveh) {
            return nullptr;
        }

        const auto CreateDriveMission = [&, this](eCarMission mission, float cruiseSpeed, CEntity* traget) {
            return new CTaskComplexCarDriveMission{
                ped->m_pVehicle,
                traget,
                mission,
                DRIVING_STYLE_AVOID_CARS,
                cruiseSpeed
            };
        };

        if (const auto cveh = m_Criminal->GetVehicleIfInOne()) {
            return CreateDriveMission(
                oveh->IsBike()
                    ? MISSION_FOLLOWCAR_CLOSE
                    : MISSION_BLOCKCAR_CLOSE,
                (float)(m_Criminal->m_pVehicle->m_autoPilot.m_nCruiseSpeed) + 10.f,
                cveh
            );
        } else {
            return CreateDriveMission(MISSION_KILLPED_CLOSE, 20.f, m_Criminal);
        }
    }
    default:
        NOTSA_UNREACHABLE();
    }
}

// 0x68C3C0
CPed* CTaskComplexKillCriminal::FindNextCriminalToKill(CPed* ped, bool any) {
    const auto [closest, distSq] = notsa::SpatialQuery(
        m_Cop->m_apCriminalsToKill | rng::views::filter(notsa::Not(NoPedOrNoHp)),
        m_Cop->GetPosition(),
        m_Criminal.Get(),
        !any && NoPedOrNoHp(m_Criminal)
            ? m_Criminal.Get()
            : nullptr
    );
    return closest;
}

// 0x68C6E0
bool CTaskComplexKillCriminal::ChangeTarget(CPed* newTarget) { // TODO: Figure out if `newTarget` is actually the new target, or it's just he ped that is the owner of this task
    if (newTarget == m_Criminal) {
        return true;
    }

    if (NoPedOrNoHp(newTarget)) {
        return false;
    }

    if (m_Criminal && m_Criminal->bInVehicle) {
        return false;
    }

    if (notsa::isa_and_nonnull<CTaskComplexKillPedOnFoot>(m_pSubTask) && !m_pSubTask->MakeAbortable(newTarget, ABORT_PRIORITY_URGENT, nullptr)) {
        return false;
    }

    if (!notsa::contains(m_Cop->m_apCriminalsToKill, newTarget)) { // 0x68c760
        return false;
    }

    m_Criminal = newTarget;

    // Propagate change to partner
    if (const auto partner = m_Cop->m_pCopPartner) {
        if (partner->bInVehicle) {
            if (const auto partnersTask = partner->GetTaskManager().Find<CTaskComplexKillCriminal>(false)) {
                notsa::cast<CTaskComplexKillCriminal>(partnersTask)->ChangeTarget(newTarget);
            }
        }
    }

    m_HasFinished = false;

    return true;
}

// 0x68DAD0
bool CTaskComplexKillCriminal::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    if ([&, this]{
        if (!event) {
            return true;
        }

        // Code @ 0x68DB32 has been inlined into the stuff below

        switch (const auto evType = event->GetEventType()) {
        case EVENT_ACQUAINTANCE_PED_HATE:
        case EVENT_VEHICLE_DAMAGE_COLLISION: // Ignore these
            return false;

        case EVENT_DAMAGE:
        case EVENT_VEHICLE_DAMAGE_WEAPON:
        case EVENT_GUN_AIMED_AT:
        case EVENT_SHOT_FIRED: {
            const auto evSrc  = event->GetSourceEntity();
            if (m_Criminal && evSrc == m_Criminal) {
                return false; // As per 0x68DB32
            }
            if (!evSrc || !evSrc->GetIsTypePed() || evSrc->AsPed()->IsPlayer()) {
                return false;
            }
            const auto evSrcPed = evSrc->AsPed();
            if (!m_Cop || m_Cop->AddCriminalToKill(evSrcPed) == (notsa::IsFixBugs() ? -1 : 0)) {
                return false;
            }
            if (notsa::contains({ EVENT_DAMAGE, EVENT_VEHICLE_DAMAGE_WEAPON }, evType)) { // Change target immediately
                if (!m_Criminal || (m_Criminal->GetPosition() - ped->GetPosition()).SquaredMagnitude() <= sq(25.f)) {
                    ChangeTarget(evSrcPed);
                }
            }
            return false;
        }
        }
        return true;
    }()) {
        return m_pSubTask->MakeAbortable(ped, priority, event);
    } else {
        const_cast<CEvent*>(event)->m_nTimeActive++; // ???????
        return false;
    }
}

// 0x68E4F0
CTask* CTaskComplexKillCriminal::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_GANG_DRIVEBY:
        return CreateSubTask(
            m_Cop->m_isTheDriver
                ? TASK_COMPLEX_CAR_DRIVE_MISSION
                : TASK_SIMPLE_CAR_DRIVE,
            ped
        );
    case TASK_COMPLEX_KILL_PED_ON_FOOT: { // Try finding the next criminal, if none, set `m_finished` and get into *the* car (if possible)
        CPed* const nextCriminal = NoPedOrNoHp(m_Criminal)
            ? FindNextCriminalToKill(ped, true)
            : m_Criminal.Get();
        if (nextCriminal && ChangeTarget(nextCriminal)) {
            return CreateSubTask(TASK_COMPLEX_KILL_PED_ON_FOOT, ped, true);
        }

        // No criminal, or can't target it, so just bail, so try getting back into the car
        m_HasFinished = true;
        if (m_CantGetInCar || !ped->m_pVehicle) {
            return CreateSubTask(TASK_FINISHED, ped);
        }

        if (!m_Cop->m_isTheDriver) {
            if (NoPedOrNoHp(m_Cop->m_pCopPartner)) { // Partner is dead, we get in as the driver
                m_Cop->m_isTheDriver = true;
                m_Cop->SetPartner(nullptr);
            } else {
                return CreateSubTask(TASK_COMPLEX_ENTER_CAR_AS_PASSENGER, ped);
            }
        }

        return CreateSubTask(TASK_COMPLEX_ENTER_CAR_AS_DRIVER, ped);
    }
    case TASK_COMPLEX_ENTER_CAR_AS_DRIVER: { // 0x68E533
        if (!ped->bInVehicle) {
            return CreateSubTask(
                m_HasFinished || NoPedOrNoHp(m_Criminal)
                    ? TASK_FINISHED
                    : TASK_COMPLEX_KILL_PED_ON_FOOT,
                ped
            );
        }
        const auto copPartnerNoneOrInVeh = NoPedOrNoHp(m_Cop->m_pCopPartner) || m_Cop->m_pCopPartner->bInVehicle;
        if (!m_HasFinished && !NoPedOrNoHp(m_Criminal) && !m_Criminal->IsInVehicle()) {
            return CreateSubTask(
                !ped->m_pVehicle || m_Criminal->IsEntityInRange(ped->m_pVehicle, 25.f) // 0x68E5D8
                    ? TASK_COMPLEX_KILL_PED_ON_FOOT
                    : copPartnerNoneOrInVeh // otherwise if criminal is too far chase them with the car
                        ? TASK_COMPLEX_CAR_DRIVE_MISSION
                        : TASK_SIMPLE_CAR_DRIVE,
                ped
            );
        }
        if (const auto next = FindNextCriminalToKill(ped, true); next && ChangeTarget(next)) { // Try finding another criminal to kill
            return CreateSubTask(TASK_COMPLEX_KILL_PED_ON_FOOT, ped, true);
        }
        // No criminal to kill, so get into *the* vehicle and fuck off
        if (ped->IsInVehicle()) {
            ped->m_pVehicle->vehicleFlags.bSirenOrAlarm = false;
        }
        return CreateSubTask(
            copPartnerNoneOrInVeh
                ? TASK_FINISHED
                : TASK_SIMPLE_CAR_DRIVE,
            ped
        );
    }
    case TASK_COMPLEX_ENTER_CAR_AS_PASSENGER: { // 0x68E6FA
        if (ped->bInVehicle) { // (Inverted)
            return CreateSubTask(TASK_SIMPLE_CAR_DRIVE, ped);
        }
        m_CantGetInCar = true;
        if (m_HasFinished || NoPedOrNoHp(m_Criminal)) {
            return CreateSubTask(TASK_FINISHED, ped);
        } 
        return CreateSubTask(TASK_COMPLEX_KILL_PED_ON_FOOT, ped);
    }
    case TASK_COMPLEX_LEAVE_CAR: // 0x68E77F
        return CreateSubTask(
            !ped->bInVehicle || m_CantGetInCar || (!m_HasFinished && !NoPedOrNoHp(m_Criminal) && !m_Criminal->IsInVehicle() && m_Criminal->IsEntityInRange(ped, 25.f))
                ? TASK_COMPLEX_KILL_PED_ON_FOOT     // Criminal can be killed on foot
                : TASK_COMPLEX_ENTER_CAR_AS_DRIVER, // We have to chase the criminal with a vehicle
            ped
        );
    default:
        NOTSA_UNREACHABLE();
    }
}

// Глобал не реверснут в репо (ambient crime enabled)
static auto& g_AmbientCrimeEnabled = StaticRef<bool>(0xB72994);

// 0x68DC60
CTask* CTaskComplexKillCriminal::CreateFirstSubTask(CPed* ped) {
    if (!m_Criminal || m_Criminal->IsPlayer()) {
        return nullptr;
    }

    // Если игрок разыскивается - не нападаем
    if (const auto wanted = FindPlayerPed()->GetPlayerData()->m_pWanted) {
        if (wanted->m_WantedLevel != eWantedLevel::WANTED_CLEAN) {
            return nullptr;
        }
    }

    if (!g_AmbientCrimeEnabled) {
        return nullptr;
    }

    if (ped->m_nPedType != PED_TYPE_COP) {
        return nullptr;
    }

    // m_isTheDriver/m_pCopPartner - поля CCopPed (CopPed.h:28,30); тип проверен гейтом выше
    auto* const cop = static_cast<CCopPed*>(ped);

    // Если за рулём машины преступника или в ней пассажиром игрок - не трогаем
    if (const auto crimVeh = m_Criminal->m_pVehicle) {
        if (const auto driver = crimVeh->m_pDriver; driver && driver->IsPlayer()) {
            return nullptr;
        }
        for (auto i = 0u; i < crimVeh->m_nMaxPassengers; i++) {
            if (const auto passenger = crimVeh->m_apPassengers[i]; passenger && passenger->IsPlayer()) {
                return nullptr;
            }
        }
    }

    m_Cop = cop; // EntityRef<CCopPed>::operator=(T*) сам делает CleanUpOldReference + RegisterReference

    if (!m_Randomize || m_Criminal->bWantedByPolice) { // m_nFourthPedFlags бит 11 (0x478 & 0x800)
        // пропускаем рандомизированные проверки
    } else {
        if (!m_Criminal->bInVehicle) { // 0x46C & 0x100
            return nullptr;
        }
        const auto crimVeh = m_Criminal->m_pVehicle;
        if (!crimVeh) {
            return nullptr;
        }
        // Оригинал: *(byte*)(veh + 0x42E) & 8 == vehicleFlags bit 19 = bIsBeingCarJacked
        // (0x42F&8 было бы битом 35 bCanPark; в декомпиле читается байт 0x42E)
        if (!crimVeh->vehicleFlags.bIsBeingCarJacked) {
            return nullptr;
        }
        if (CGeneral::GetRandomNumberInRange(0, 3) != 0) {
            return nullptr;
        }
    }

    m_Cop->AddCriminalToKill(m_Criminal);

    if (m_Criminal->bInVehicle) {
        m_Criminal->GetIntelligence()->SetPedDecisionMakerType(6);
    }

    CTask* result{};

    if (!ped->bInVehicle || !ped->m_pVehicle) {
        // Коп пешком
        const auto partner = cop->m_pCopPartner;
        if (!partner || partner->m_fHealth < 0.f) { // оригинал: fHealth < DAT_00858B50 (0.0f), строго
            cop->m_isTheDriver = true;
        }

        if (m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_KILL_PED_ON_FOOT) {
            result = m_pSubTask;
            goto Finish;
        }
        result = new CTaskComplexKillPedOnFoot(m_Criminal, -1, 0, 0, 0, 1);
        ped->SetCurrentWeapon(WEAPON_PISTOL); // 0x5E6280(0x16) - в оригинале сразу после создания задачи
        goto Finish;
    }

    { // Коп в машине
        const auto copVeh = ped->m_pVehicle;
        if (copVeh->m_pDriver == ped) {
            // Коп сам водитель
            cop->m_isTheDriver = true;
            if (!cop->m_pCopPartner) {
                // Берём первого cop-пассажира в партнёры (SetPartner - метод CCopPed)
                for (auto i = 0u; i < copVeh->m_nMaxPassengers; i++) {
                    if (const auto passenger = copVeh->m_apPassengers[i]; passenger && passenger->m_nPedType == PED_TYPE_COP) {
                        cop->SetPartner(static_cast<CCopPed*>(passenger));
                        break;
                    }
                }
            }
            if (const auto partner = cop->m_pCopPartner) {
                // Сообщаем партнёру "ненавидеть" преступника
                CEventAcquaintancePedHate hate{ m_Criminal.Get() };
                partner->GetIntelligence()->m_eventGroup.Add(&hate, false);
            }
        } else {
            if (const auto driver = copVeh->m_pDriver; driver && driver->m_nPedType == PED_TYPE_COP) {
                cop->SetPartner(static_cast<CCopPed*>(driver));
                static_cast<CCopPed*>(driver)->m_isTheDriver = true;
            }

            if (const auto partner = cop->m_pCopPartner) {
                if (partner->m_isTheDriver) {
                    // Партнёр водитель: убедимся, что он преследует того же преступника
                    const auto chaseTask = partner->GetIntelligence()->FindTaskByType(TASK_COMPLEX_KILL_CRIMINAL);
                    if (!chaseTask || notsa::cast_if_present<CTaskComplexKillCriminal>(chaseTask)->m_Criminal != m_Criminal.Get()) {
                        CEventAcquaintancePedHate hate{ m_Criminal.Get() };
                        partner->GetIntelligence()->m_eventGroup.Add(&hate, false);
                        return nullptr;
                    }
                    cop->m_isTheDriver = false;
                    ped->bDontDragMeOutCar = true; // m_nPedFlags |= 0x20000000
                }
            } else {
                cop->m_isTheDriver = true;
                result = (m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_LEAVE_CAR)
                    ? m_pSubTask
                    : new CTaskComplexLeaveCar(ped->m_pVehicle, 0, 0, true, false);
                if (result) {
                    goto Finish;
                }
            }
        }

        if (ped->bInVehicle && ped->m_pVehicle) {
            if (!cop->m_isTheDriver) {
                // Пассажир: помогаем вести машину
                if (m_pSubTask && m_pSubTask->GetTaskType() == TASK_SIMPLE_CAR_DRIVE) {
                    result = m_pSubTask;
                    goto Finish;
                }
                result = new CTaskSimpleCarDrive(ped->m_pVehicle, nullptr, false);
                goto Finish;
            }

            // Водитель: преследуем преступника на машине
            if (m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_CAR_DRIVE_MISSION) {
                result = m_pSubTask;
                goto Finish;
            }
            if (!m_Criminal->bInVehicle || !m_Criminal->m_pVehicle) {
                // Преступник пешком - давим/убиваем
                result = new CTaskComplexCarDriveMission(
                    ped->m_pVehicle,
                    m_Criminal.Get(),
                    MISSION_KILLPED_CLOSE,
                    (eCarDrivingStyle)2,
                    20.f
                );
                goto Finish;
            }
            const auto crimVeh = m_Criminal->m_pVehicle;
            result = new CTaskComplexCarDriveMission(
                ped->m_pVehicle,
                crimVeh,
                ped->m_pVehicle->IsBike()
                    ? MISSION_FOLLOWCAR_CLOSE     // 0x35
                    : MISSION_BLOCKCAR_CLOSE,     // 0x12
                (eCarDrivingStyle)2,
                (float)crimVeh->m_autoPilot.m_nCruiseSpeed + 10.f
            );
            goto Finish;
        }

            if (m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_KILL_PED_ON_FOOT) {
                result = m_pSubTask;
                goto Finish;
            }
            result = new CTaskComplexKillPedOnFoot(m_Criminal, -1, 0, 0, 0, 1);
            ped->SetCurrentWeapon(WEAPON_PISTOL); // 0x5E6280(0x16) - как в пешей ветке (общий хвост 0x68E29x)
        }

Finish: // 0x68E2DE
    if (const auto copVeh = ped->m_pVehicle) {
        if (cop->m_isTheDriver) {
            // Запоминаем "родную" задачу водителя машины
            m_OrigDrivingMode = (int8)copVeh->m_autoPilot.m_nCarDrivingStyle;
            m_OrigMission     = (int8)copVeh->m_autoPilot.m_nCarMission;
            m_OrigCruiseSpeed = copVeh->m_autoPilot.m_nCruiseSpeed;
            m_IsSetUp         = true;
        }
    }

    // Оригинал (0x68E30C..): преступник пешком -> оружие с шансом 50%; иначе если у машины
    // преступника стоит vehicleFlags.bIsBeingCarJacked (байт +0x42E & 8 = бит 19) - вооружаемся;
    // при пассажирах - вооружаем и их (все копы), вызывая AddCriminalToKill. Если бит снят - ничего.
    if (const auto crimVeh = m_Criminal->m_pVehicle) {
        if (crimVeh->vehicleFlags.bIsBeingCarJacked) {
            if (crimVeh->m_nNumPassengers == 0) {
                if (CGeneral::GetRandomNumber() & 1) {
                    ped->GiveWeapon(WEAPON_PISTOL, 1000, true);
                    ped->SetCurrentWeapon(WEAPON_PISTOL);
                }
            } else {
                // Выдаём пистолеты всем копам в машине преступника (арест "штурмом")
                ped->GiveWeapon(WEAPON_PISTOL, 1000, true);
                ped->SetCurrentWeapon(WEAPON_PISTOL);
                for (auto i = 0u; i < crimVeh->m_nMaxPassengers; i++) {
                    if (const auto passenger = crimVeh->m_apPassengers[i]) {
                        passenger->GiveWeapon(WEAPON_PISTOL, 1000, true);
                        passenger->SetCurrentWeapon(WEAPON_PISTOL);
                        static_cast<CCopPed*>(passenger)->AddCriminalToKill(m_Criminal); // оригинал зовёт без проверки типа
                    }
                }
            }
        }
    } else {
        if (CGeneral::GetRandomNumber() & 1) {
            ped->GiveWeapon(WEAPON_PISTOL, 1000, true);
            ped->SetCurrentWeapon(WEAPON_PISTOL);
            // NOTE(asm) SUSPECT: после этого оригинал (0x68E337..0x68E355) создаёт стековый
            // CEventAcquaintancePedHate (0x420E70, target = этот коп!) и добавляет его через
            // CEventGroup::Add (0x4AB420) - в декомпиле this у Add потерян (чьё событие-группы -
            // преступника или партнёра - по декомпилу не определить). В перевод не включено.
        }
    }

    // Чтобы копа не удалили, пока идёт погоня
    ped->m_nTimeTillWeNeedThisPed = CTimer::GetTimeInMS() + 300000;
    ped->bCullExtraFarAway        = true;   // m_nSecondPedFlags |= 0x40
    ped->m_fRemovalDistMultiplier = 0.3f;

    if (const auto copVeh = ped->m_pVehicle) {
        copVeh->m_nExtendedRemovalRange       = 0xFF;  // 0x4A6 (word)
        copVeh->vehicleFlags.bNeverUseSmallerRemovalRange = true;  // 0x42C |= 0x40
        if (ped->bInVehicle) {
            copVeh->vehicleFlags.bSirenOrAlarm = true; // 0x42D |= 0x80
        }
    }

    g_InterestingEvents.Add(CInterestingEvents::INTERESTING_EVENT_25, ped); // 0x602590

    return result;
}

// 0x68E950
CTask* CTaskComplexKillCriminal::ControlSubTask(CPed* ped) {
    /*
    if (m_criminal && !m_criminal->CanBeCriminal()) {
        return nullptr;
    }

    if (const auto wanted = FindPlayerWanted(); wanted->m_nWantedLevel) {
        if (wanted->CanCopJoinPursuit(static_cast<CCopPed*>(ped)) && m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
            return nullptr;
        }
    }

    if (!g_LoadMonitor.m_bEnableAmbientCrime) {
        return nullptr;
    }

    auto taskToCreate = TASK_NONE;

    const auto cpartner = m_cop->m_pCopPartner;
    if (!m_cop->m_isTheDriver && IsPedNullOrLowHP(cpartner)) {
        m_cop->m_isTheDriver = true;
        m_cop->SetPartner(nullptr);
        if (m_cop->IsInVehicle()) {
            taskToCreate = TASK_COMPLEX_LEAVE_CAR;
        }
    }
    */
    return plugin::CallMethodAndReturn<CTask*, 0x68E950, CTaskComplexKillCriminal*, CPed*>(this, ped); // Good luck!
}
