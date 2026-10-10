// ============================================================================
// INTEGRATED FILE — source/game_sa/Tasks/TaskTypes/TaskComplexArrestPed.cpp
// Батчи: w2-41-TaskComplexArrestPed + волна 5 (фрагменты batch_out/fresh/
// 0x68CF80.cpp, 0x68D350.cpp, 0x690220.cpp; верификация VERIFY-wf5a)
//
// СОСТОЯНИЕ ПОСЛЕ ВОЛНЫ 5 (все 3 функции реверснуты):
//   - CreateSubTask (0x68CF80): тело из предыдущего слияния сохранено — сверено с
//     фрагментом 0x68CF80.cpp, эквивалентно (CTaskComplexSeekEntityStandard ==
//     CTaskComplexSeekEntity<CEntitySeekPosCalculatorStandard>, TaskComplexSeekEntityStandard.h).
//   - ControlSubTask (0x68D350): тело ЗАМЕНЕНО переводом декомпила (фрагмент 0x68D350.cpp).
//     Прежнее тело (из апстрима) расходилось с бинарником в 4 местах — см. список в шапке фрагмента:
//     (1) m_bSubTaskNeedsToBeCreated: CreateFirstSubTask зовётся У ЭТОЙ задачи и её результат
//         возвращается (asm 0x68D3BA: call [vtbl+0x24]); (2) в CAR_OPEN_* нет безусловного
//         destroy/kill в хвосте — «ничего не подошло» = KEEP (сторож TASK_NONE=200); в
//         CAR_OPEN_PASSENGER_DOOR !bInVehicle => KILL, «обе двери заняты» => EquipWeapon+DESTROY;
//         (3) KILL: найденная, но не-IsFalling задача FALL_AND_GET_UP проваливается в «машинную»
//         ветку; leave-car-проверка: оригинал KEEPит при (m_Vehicle == arrestedVehicle || есть
//         LEAVE_CAR-таск); (4) смена оружия (EquipWeapon, инлайн 0x68D7CD) только на ветках
//         boat/plane/heli и «обе двери заняты»; без GiveWeapon.
//   - CreateNextSubTask (0x690220): тело восстановлено из декомпила+дизасма (фрагмент
//     0x690220.cpp); флаги подзадач читаются по смещениям (поля protected/private в хедерах репо):
//     CTaskComplexEnterCar+0x10 (bit1=m_bQuitAfterOpeningDoor, bit2=m_bQuitAfterDraggingPedOut),
//     CTaskComplexSeekEntityStandard+0x48 (bit2=m_bAchievedSeekEntity; VALIDATE_SIZE 0x4C).
//
// Константы сверены по exe (VA -> float): 0x858B50=0.0, 0x858FE8=25.0, 0x858CA0=2.0,
//   0x87097C=3.0, 0x870980=4.0, 0x859E30=2.0, 0x859E34=2.0
//
// ИСТОРИЯ (предыдущее слияние):
//   - Тела ControlSubTask/CreateSubTask были в репо МЁРТВЫМ КОДОМ под call-заглушками
//     (TaskComplexArrestPed.cpp:108-264 и 267-306) — поднимены, заглушки удалены.
//   - CreateSubTask: случай TASK_COMPLEX_SEEK_ENTITY в репо был NOTSA_UNREACHABLE
//     (крэш в рантайме). Восстановлено тело из декомпила 0x68CF80 (случай 0x38b) —
//     см. «SEEK_ENTITY» ниже.
//
// КОНСТАНТЫ ПРОВЕРЕНЫ ПО БИНАРНИКУ (D:\projects\keep\ghidra\gta_sa.exe, .rdata):
//   0x87097C = 3.0f, 0x870980 = 4.0f, 0x859E30 = 2.0f, 0x859E34 = 2.0f, 0x858CA0 = 2.0f
//
// SEEK_ENTITY (0x68D014, случай 0x38b декомпила 0x68CF80):
//   CTaskComplexSeekEntityStandard(m_PedToArrest, 50'000, 1000,
//       bIsBeingArrested ? 4.0f : 3.0f,   // maxEntityDist2D: DAT_00870980/DAT_0087097C
//       2.0f,                              // moveStateRadius:  DAT_00859E30
//       2.0f,                              // followNodeThresholdHeightChange: DAT_00859E34
//       true, true)
//   (стиш-вариант stash_wt\...\TaskComplexArrestPed.cpp:290 с maxDist=2.0f —
//    ОШИБКА: противоречит и декомпилу, и бинарнику; закомментированный код репо
//    (radius = bIsBeingArrested ? 4.0f : 3.0f) декомпилу соответствует)
//
// НЕ РЕВЕРСИРОВАНО (1 из 3) — снято волной 5: CreateNextSubTask (0x690220) восстановлен
//   из декомпила+дизасма (фрагмент 0x690220.cpp, VERIFY-wf5a), хук включён.
//
// ЗАВИСИМОСТИ ПРОВЕРЕНЫ В РЕПО:
//   - CTaskComplexSeekEntityStandard: SeekEntity/TaskComplexSeekEntityStandard.h/.cpp
//     существуют, входят в билд (CMake GLOB_RECURSE), уже используются
//     (TaskGangHasslePed.cpp:60 — тот же ctor FUN_0046AC10, TaskComplexRoadRage.cpp:58);
//     include-стиль как в TaskGangHasslePed.cpp:1.
//   - Конструкторы всех создаваемых задач сверены с хедерами репо:
//     CTaskSimpleArrestPed(CPed*), CTaskComplexKillPedOnFoot(CPed*,-1,0,0,0,1),
//     CTaskComplexDestroyCar(CVehicle*,0,0,0), CTaskComplexDragPedFromCar(CPed*,100'000),
//     CTaskComplexOpenDriverDoor(CVehicle*), CTaskComplexOpenPassengerDoor(CVehicle*,8),
//     CTaskSimpleWaitUntilPedIsOutCar(CPed*,CVector).
//   - CWanted::m_NumCopsInPursuit (Wanted.h:28), CWanted::IsClosestCop(CPed*,int32)
//     (Wanted.h:110), CTX_GLOBAL_SOLO=199 (PedSpeechContexts.h:210),
//     CTaskComplexFallAndGetUp::IsFalling/SetDownTime (TaskComplexFallAndGetUp.h:26-27),
//     CTaskManager::FindActiveTaskByType (TaskManager.h:75).
// ============================================================================

#include "StdInc.h"

#include "extensions/utility.hpp"

#include "TaskComplexArrestPed.h"
#include "TaskComplexFallAndGetUp.h"
#include "TaskSimpleWaitUntilPedIsOutCar.h"
#include "TaskSimpleArrestPed.h"
#include "TaskComplexKillPedOnFoot.h"
#include "TaskComplexDestroyCar.h"
#include "SeekEntity/TaskComplexSeekEntityStandard.h" // CreateSubTask(TASK_COMPLEX_SEEK_ENTITY) — было закомментировано
#include "TaskComplexDragPedFromCar.h"
#include "TaskComplexOpenDriverDoor.h"
#include "TaskComplexOpenPassengerDoor.h"

#include "eTargetDoor.h"

void CTaskComplexArrestPed::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexArrestPed, 0x8709A8, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x68B990);
    RH_ScopedInstall(Destructor, 0x68BA00);
    RH_ScopedVMTInstall(MakeAbortable, 0x68BA60);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x690220); // тело восстановлено волной 5 (0x690220.cpp)
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x6907A0);
    RH_ScopedVMTInstall(ControlSubTask, 0x68D350); // тело заменено переводом декомпила волной 5 (0x68D350.cpp)
    RH_ScopedInstall(CreateSubTask, 0x68CF80);     // флаг снят — тело было мёртвым кодом в репо
}

// 0x68B990
CTaskComplexArrestPed::CTaskComplexArrestPed(CPed* ped) : CTaskComplex() {
    m_PedToArrest = ped;
    m_Vehicle = nullptr;
    CEntity::SafeRegisterRef(m_PedToArrest);
}

// 0x68BA00
CTaskComplexArrestPed::~CTaskComplexArrestPed() {
    CEntity::SafeCleanUpRef(m_PedToArrest);
}

// 0x68BA60


bool CTaskComplexArrestPed::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    return m_pSubTask->MakeAbortable(ped, priority, event);
}

// 0x690220 (волна 5, фрагмент 0x690220.cpp; декомпил + сверка asm 0x690220-0x690620)
CTask* CTaskComplexArrestPed::CreateNextSubTask(CPed* ped) {
    if (!m_PedToArrest) {
        return CreateSubTask(TASK_FINISHED, ped); // 0x516
    }

    // 0x690240: цель уже задерживается — довести дело до наручников
    if (m_PedToArrest->bIsBeingArrested && m_pSubTask->GetTaskType() != TASK_SIMPLE_ARREST_PED) {
        if (m_pSubTask->GetTaskType() != TASK_COMPLEX_SEEK_ENTITY) {
            return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
        }
        // Коп ещё не добрался до цели?
        if ((*reinterpret_cast<const uint8*>(reinterpret_cast<uintptr_t>(m_pSubTask) + 0x48) & 4) == 0) { // !m_bAchievedSeekEntity
            return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
        }
        return CreateSubTask(TASK_SIMPLE_ARREST_PED, ped);
    }

    const auto subTaskType   = m_pSubTask->GetTaskType();
    // Байт бит-флагов CTaskComplexEnterCar (для CAR_OPEN_*/DRAG_PED_FROM_CAR подзадач)
    const auto subTaskFlags10 = *reinterpret_cast<const uint8*>(reinterpret_cast<uintptr_t>(m_pSubTask) + 0x10);

    switch (subTaskType) {
    case TASK_COMPLEX_SEEK_ENTITY: { // 0x38B
        // FIX(verify): каст к CTaskComplexFallAndGetUp — IsFalling/SetDownTime не на CTask
        const auto fallTask = static_cast<CTaskComplexFallAndGetUp*>(
            m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_FALL_AND_GET_UP));
        if (fallTask && fallTask->IsFalling()
            && (*reinterpret_cast<const uint8*>(reinterpret_cast<uintptr_t>(m_pSubTask) + 0x48) & 4) != 0) { // m_bAchievedSeekEntity
            // Пед упал и мы рядом — сбить и арестовать
            const auto dir = m_PedToArrest->GetPosition() - ped->GetPosition();
            if (std::abs(dir.z) <= 2.0f && dir.SquaredMagnitude() <= sq(3.0f)) { // 2.0f == 0x858CA0, 3.0f == 0x87097C
                fallTask->SetDownTime(100'000);
                return CreateSubTask(TASK_SIMPLE_ARREST_PED, ped);
            }
        }
        break; // → TASK_COMPLEX_KILL_PED_ON_FOOT
    }
    case TASK_COMPLEX_DRAG_PED_FROM_CAR: { // 0x2BF
        // FIX(verify): каст к CTaskComplexFallAndGetUp — IsFalling/SetDownTime не на CTask
        const auto fallTask = static_cast<CTaskComplexFallAndGetUp*>(
            m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_FALL_AND_GET_UP));
        if (fallTask && fallTask->IsFalling()
            && (subTaskFlags10 & 4) == 0) { // !m_bQuitAfterDraggingPedOut (не бросаем после вытаскивания)
            const auto dir = m_PedToArrest->GetPosition() - ped->GetPosition();
            if (std::abs(dir.z) <= 2.0f && dir.SquaredMagnitude() <= sq(3.0f)) {
                fallTask->SetDownTime(100'000);
                return CreateSubTask(TASK_SIMPLE_ARREST_PED, ped);
            }
        }
        break; // → TASK_COMPLEX_KILL_PED_ON_FOOT
    }
    case TASK_COMPLEX_CAR_OPEN_DRIVER_DOOR:     // 0x2D2
    case TASK_COMPLEX_CAR_OPEN_PASSENGER_DOOR: { // 0x2D8 — тела обоих кейсов идентичны в оригинале
        // Машину заперли? Запоминаем её, чтобы дальше ломать именно её
        if ((subTaskFlags10 & 2) != 0 // m_bQuitAfterOpeningDoor
            && m_PedToArrest->m_pVehicle
            && !m_PedToArrest->m_pVehicle->CanPedOpenLocks(ped)
        ) {
            m_Vehicle = m_PedToArrest->m_pVehicle; // +0x20
        }

        if (!m_PedToArrest->IsAlive()) {
            // Цель умерла — надеваем наручники на труп
            return CreateSubTask(TASK_SIMPLE_ARREST_PED, ped);
        }

        if (m_PedToArrest->bInVehicle && (subTaskFlags10 & 2) == 0) {
            if (m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_LEAVE_CAR)) {
                return CreateSubTask(TASK_COMPLEX_KILL_PED_ON_FOOT, ped);
            }
            return CreateSubTask(TASK_SIMPLE_ARREST_PED, ped);
        }
        break; // → TASK_COMPLEX_KILL_PED_ON_FOOT
    }
    case TASK_SIMPLE_WAIT_UNTIL_PED_OUT_CAR: // 0x33D
        return CreateSubTask(TASK_COMPLEX_KILL_PED_ON_FOOT, ped);
    case TASK_COMPLEX_KILL_PED_ON_FOOT: { // 0x3E8
        // Цель умерла — арестовываем (труп)
        if (m_PedToArrest->m_fHealth <= 0.0f) {
            return CreateSubTask(TASK_SIMPLE_ARREST_PED, ped);
        }

        // Цель падает: рядом → арест; далеко → переходим на seek
        // FIX(verify): каст к CTaskComplexFallAndGetUp — IsFalling/SetDownTime не на CTask
        const auto fallTask = static_cast<CTaskComplexFallAndGetUp*>(
            m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_FALL_AND_GET_UP));
        if (fallTask && fallTask->IsFalling()) {
            const auto dir = m_PedToArrest->GetPosition() - ped->GetPosition();
            if (std::abs(dir.z) > 2.0f || dir.SquaredMagnitude() > sq(3.0f)) {
                return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
            }
            fallTask->SetDownTime(100'000);
            return CreateSubTask(TASK_SIMPLE_ARREST_PED, ped);
        }

        // Некоп (армия/охрана) арестовывает игрока, за которым уже идёт полицейская погоня —
        // отпускаем: это дело копов
        if (ped->m_nPedType != PED_TYPE_COP && m_PedToArrest->IsPlayer()) {
            if (m_PedToArrest->GetPlayerWanted()->m_NumCopsInPursuit != 0) { // CWanted+0x18 (asm 0x6906xx)
                return CreateSubTask(TASK_FINISHED, ped);
            }
        }
        break; // → TASK_COMPLEX_KILL_PED_ON_FOOT (пересоздаём)
    }
    case TASK_COMPLEX_DESTROY_CAR: // 0x3EB
    case TASK_SIMPLE_ARREST_PED:   // 0x44C
        return CreateSubTask(TASK_FINISHED, ped);
    default:
        // FIX(verify): декомпил возвращает nullptr для ЛЮБЫХ необработанных типов:
        //  - >= 0x38C: явный `return 0` (декомпил, строка 134);
        //  - < 0x38C: default внутреннего свитча делает goto на самый конец функции,
        //    ПЕРЕСКАКИВАЯ вызов CreateSubTask(1000) (строки 183-185 декомпила),
        //    uVar6 остаётся 0 => nullptr. Прежнее `break` (→ KILL) противоречило декомпилу.
        return nullptr;
    }
    return CreateSubTask(TASK_COMPLEX_KILL_PED_ON_FOOT, ped);
}

// NOTSA
void MakeSurePedHasWeaponInHand(CPed* ped) {
    // Make sure ped has an actual weapon in their hand
    if (!ped->GetActiveWeapon().IsTypeMelee())
        return;

    if (ped->DoWeHaveWeaponAvailable(WEAPON_SHOTGUN)) { // Use shotgun (if available)
        ped->SetCurrentWeapon(WEAPON_SHOTGUN);
        return;
    }

    // Otherwise a pistol
    if (!ped->DoWeHaveWeaponAvailable(WEAPON_PISTOL)) { // Make sure they have one
        ped->GiveWeapon(WEAPON_PISTOL, 10, false);
    }
    ped->SetCurrentWeapon(WEAPON_PISTOL);
}

// 0x6907A0


CTask* CTaskComplexArrestPed::CreateFirstSubTask(CPed* ped) {
    if (!m_PedToArrest) {
        return nullptr;
    }

    m_bSubTaskNeedsToBeCreated = false;

    if (!m_PedToArrest->bInVehicle) {
        return CreateSubTask(TASK_COMPLEX_KILL_PED_ON_FOOT, ped);
    }

    if (m_PedToArrest->m_pVehicle->IsBike() || m_PedToArrest->m_pVehicle->IsSubQuad()) { // Just drag ped from a bike/quad
        return CreateSubTask(TASK_COMPLEX_DRAG_PED_FROM_CAR, ped);
    }

    if (m_PedToArrest->m_pVehicle->IsSubBoat()) { // If they're in a boat, just destroy it
        MakeSurePedHasWeaponInHand(ped);
        return CreateSubTask(TASK_COMPLEX_DESTROY_CAR, ped);
    } else {
        if (m_PedToArrest->m_pVehicle->IsUpsideDown() || m_PedToArrest->m_pVehicle->IsOnItsSide()) {
            return CreateSubTask(TASK_COMPLEX_DESTROY_CAR, ped);
        }
        return CreateSubTask(TASK_COMPLEX_CAR_OPEN_DRIVER_DOOR, ped);
    }
}

// 0x68D350 (волна 5, фрагмент 0x68D350.cpp — перевод декомпила; прежнее апстримное тело расходилось
// с бинарником в 4 местах, см. шапку файла)
CTask* CTaskComplexArrestPed::ControlSubTask(CPed* ped) {
    // Сторож оригинала: TASK_NONE (200) == «подзадачу не менять» (asm 0x68D844: cmp edi, 0xC8)
    eTaskType newTaskType = TASK_NONE;

    // Инлайн-блок 0x68D7CD: взять оружие в руки (дробовик, если есть, иначе пистолет).
    // В оригинале НЕ выдаёт оружие.
    const auto EquipWeapon = [ped] {
        if (ped->GetActiveWeapon().IsTypeMelee()) {
            ped->SetCurrentWeapon(ped->DoWeHaveWeaponAvailable(WEAPON_SHOTGUN) ? WEAPON_SHOTGUN : WEAPON_PISTOL);
        }
    };

    // 0x68D360
    if (!m_PedToArrest || m_PedToArrest->m_fHealth <= 0.0f) { // 0.0f == DAT_00858B50
        newTaskType = TASK_FINISHED;
    } else if (m_bSubTaskNeedsToBeCreated) {
        // asm 0x68D3A6: MakeAbortable у подзадачи, затем CreateFirstSubTask У ЭТОЙ задачи
        // (vtable+0x24, ECX = this) и возврат именно её результата.
        if (m_pSubTask->MakeAbortable(ped)) {
            return CreateFirstSubTask(ped);
        }
        return m_pSubTask;
    } else {
        const auto subTaskType = m_pSubTask->GetTaskType();

        // 0x68D3CB
        if (m_PedToArrest->bIsBeingArrested
            && subTaskType != TASK_SIMPLE_ARREST_PED
            && subTaskType != TASK_COMPLEX_SEEK_ENTITY
        ) {
            newTaskType = TASK_COMPLEX_SEEK_ENTITY;
        } else {
            switch (subTaskType) {
            case TASK_COMPLEX_CAR_OPEN_DRIVER_DOOR: { // 0x68D49D... // тип 722 (0x2D2)
                // Оригинал считает (GetPositionToArrest - copPos).SquaredMagnitude() безусловно до
                // проверок bInVehicle — вынесено внутрь ветки (без сайд-эффектов, семантика та же).
                const auto leaveCarTask = m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_LEAVE_CAR);
                if (leaveCarTask && m_PedToArrest->bInVehicle
                    && (m_PedToArrest->GetPosition() - ped->GetPosition()).SquaredMagnitude() < 25.0f /* 0x858FE8 */
                ) {
                    newTaskType = TASK_SIMPLE_WAIT_UNTIL_PED_OUT_CAR;
                } else if (!m_PedToArrest->bInVehicle) {
                    newTaskType = TASK_COMPLEX_KILL_PED_ON_FOOT;
                } else if (m_PedToArrest->m_pVehicle->IsRoomForPedToLeaveCar(TARGET_DOOR_DRIVER /*10*/, nullptr)) {
                    // Есть место у водительской — подзадачу не меняем
                } else if (m_PedToArrest->m_pVehicle->IsRoomForPedToLeaveCar(TARGET_DOOR_FRONT_RIGHT /*8*/, nullptr)) {
                    newTaskType = TASK_COMPLEX_CAR_OPEN_PASSENGER_DOOR;
                }
                // FIX(verify): ветка DRIVER_DOOR — в оригинале при «обе двери заняты»
                // подзадача ОСТАЁТСЯ как есть: декомпил 0x2d2 (строки 116-121) после
                // неудачных room(10)/room(8) НЕ ставит новый тип (iStack_2c остаётся 200),
                // никакого EquipWeapon/DESTROY_CAR здесь нет; destroy со сменой оружия —
                // только в ветке PASSENGER_DOOR и в KILL (boat/plane/heli).
                break;
            }
            case TASK_COMPLEX_CAR_OPEN_PASSENGER_DOOR: { // 0x68D510 // тип 728 (0x2D8)
                const auto leaveCarTask = m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_LEAVE_CAR);
                if (leaveCarTask && m_PedToArrest->bInVehicle
                    && (m_PedToArrest->GetPosition() - ped->GetPosition()).SquaredMagnitude() < 25.0f
                ) {
                    newTaskType = TASK_SIMPLE_WAIT_UNTIL_PED_OUT_CAR;
                } else if (!m_PedToArrest->bInVehicle) {
                    newTaskType = TASK_COMPLEX_KILL_PED_ON_FOOT;
                } else if (m_PedToArrest->m_pVehicle->IsRoomForPedToLeaveCar(TARGET_DOOR_FRONT_RIGHT /*8*/, nullptr)) {
                    // Место справа есть — подзадачу не меняем
                } else if (m_PedToArrest->m_pVehicle->IsRoomForPedToLeaveCar(TARGET_DOOR_DRIVER /*10*/, nullptr)) {
                    newTaskType = TASK_COMPLEX_CAR_OPEN_DRIVER_DOOR;
                } else {
                    EquipWeapon();
                    newTaskType = TASK_COMPLEX_DESTROY_CAR;
                }
                break;
            }
            case TASK_COMPLEX_DRAG_PED_FROM_CAR: // 0x2BF
            case TASK_COMPLEX_DESTROY_CAR: {     // 0x3EB — общий хвост 0x651A1B-аналог (строка 185 декомпила)
                if (!m_PedToArrest->bInVehicle) {
                    newTaskType = TASK_COMPLEX_KILL_PED_ON_FOOT;
                }
                break;
            }
            case TASK_COMPLEX_KILL_PED_ON_FOOT: { // 0x68D626 // тип 1000
                // FIX(verify): FindActiveTaskByType возвращает CTask* — IsFalling/SetDownTime
                // объявлены на CTaskComplexFallAndGetUp (репо: TaskComplexFallAndGetUp.h:26-27);
                // без каста фрагмент не компилируется.
                const auto fallTask = static_cast<CTaskComplexFallAndGetUp*>(
                    m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_FALL_AND_GET_UP));
                if (fallTask && fallTask->IsFalling()) {
                    // Пед падает: подбежать и арестовать, если достаточно близко
                    const auto dir = m_PedToArrest->GetPosition() - ped->GetPosition();
                    if (std::abs(dir.z) <= 2.0f && dir.SquaredMagnitude() <= sq(3.0f)) { // 2.0f == 0x858CA0, 3.0f == 0x87097C
                        fallTask->SetDownTime(100'000);
                        newTaskType = TASK_SIMPLE_ARREST_PED;
                    } else {
                        newTaskType = TASK_COMPLEX_SEEK_ENTITY;
                    }
                    break;
                }

                // Не падает: если не в машине — просто продолжаем
                if (!m_PedToArrest->bInVehicle || !m_PedToArrest->m_pVehicle) {
                    break;
                }
                const auto targetVehicle = m_PedToArrest->m_pVehicle;

                if (targetVehicle->IsBoat() || targetVehicle->IsSubPlane() || targetVehicle->IsSubHeli()) {
                    // +0x590 == 5 (BOAT) / +0x594 == 4 (PLANE) / +0x594 == 3 (HELI)
                    // → в воду не лезем, уничтожаем машину (0x68D7CD: со сменой оружия)
                    EquipWeapon();
                    newTaskType = TASK_COMPLEX_DESTROY_CAR;
                    break;
                }

                // Не melee в руках и не ближайший коп → лучше уничтожить машину
                if (!ped->GetActiveWeapon().IsTypeMelee()) {
                    if (!FindPlayerWanted()->IsClosestCop(ped, 2)) {
                        newTaskType = TASK_COMPLEX_DESTROY_CAR; // asm: jmp 0x68D7F0 — БЕЗ смены оружия
                        break;
                    }
                }

                // Машина уже «наша», либо пед сам выходит — не мешаем
                // (В РЕПО ЭТА ПРОВЕРКА ИНВЕРТИРОВАНА — см. шапку, п.3)
                if (m_Vehicle == targetVehicle || m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_LEAVE_CAR)) {
                    break;
                }

                if (targetVehicle->IsBike() || targetVehicle->IsSubQuad()) { // +0x590 == 9 / +0x594 == 2
                    newTaskType = TASK_COMPLEX_DRAG_PED_FROM_CAR;
                    break;
                }

                if (!targetVehicle->IsUpsideDown() && !targetVehicle->IsOnItsSide()) {
                    // FIX(verify): декомпил (строка 155) ставит 0x2d2 = DRIVER_DOOR,
                    // а не PASSENGER_DOOR (copy-paste из соседнего кейса).
                    newTaskType = TASK_COMPLEX_CAR_OPEN_DRIVER_DOOR;
                    break;
                }

                // Перевёрнута / на боку — уничтожаем (без смены оружия)
                newTaskType = TASK_COMPLEX_DESTROY_CAR;
                break;
            }
            }
        }
    }

    // 0x68D7F9: арестовываемый — игрок, за ним гонится ровно один коп → реплика
    if (m_PedToArrest && m_PedToArrest->IsPlayer()) {
        if (FindPlayerWanted()->m_NumCopsInPursuit == 1) {
            ped->Say(CTX_GLOBAL_SOLO, 0, 1.0f, false, false, false); // Say(199, 0, 1.0f, 0, 0, 0)
        }
        // В оригинале в else-ветке есть ещё один «холостой» вызов FindPlayerWanted() (asm 0x68D836,
        // результат не используется) — опущен как не имеющий наблюдаемого эффекта.
    }

    // 0x68D840: применить смену подзадачи
    if (newTaskType != TASK_NONE && m_pSubTask->MakeAbortable(ped)) {
        return CreateSubTask(newTaskType, ped);
    }
    return m_pSubTask;
}

// 0x68CF80
CTask* CTaskComplexArrestPed::CreateSubTask(eTaskType taskType, CPed* ped) {
    switch (taskType) {
    case TASK_SIMPLE_ARREST_PED:
        if (m_PedToArrest->m_pVehicle) {
            if (m_PedToArrest->m_pVehicle->IsDriver(m_PedToArrest)) {
                m_PedToArrest->m_pVehicle->vehicleFlags.bIsHandbrakeOn = true;
                m_PedToArrest->m_pVehicle->SetStatus(STATUS_FORCED_STOP);
            }
        }
        return new CTaskSimpleArrestPed(m_PedToArrest);

    case TASK_COMPLEX_KILL_PED_ON_FOOT:
        return new CTaskComplexKillPedOnFoot(m_PedToArrest, -1, 0, 0, 0, 1);

    case TASK_COMPLEX_DESTROY_CAR:
        return new CTaskComplexDestroyCar(m_PedToArrest->m_pVehicle, 0, 0, 0);

    case TASK_COMPLEX_SEEK_ENTITY: { // 0x68D014 (декомпил 0x68CF80, случай 0x38b)
        // maxEntityDist2D: 0x870980 (4.0f) если цель уже задерживается, иначе 0x87097C (3.0f);
        // moveStateRadius = 0x859E30 (2.0f); followNodeThresholdHeightChange = 0x859E34 (2.0f)
        const auto radius = m_PedToArrest->bIsBeingArrested ? 4.0f : 3.0f;
        return new CTaskComplexSeekEntityStandard(m_PedToArrest, 50'000, 1000, radius, 2.0f, 2.0f, true, true);
    }
    case TASK_COMPLEX_DRAG_PED_FROM_CAR:
        return new CTaskComplexDragPedFromCar(m_PedToArrest, 100'000);

    case TASK_COMPLEX_CAR_OPEN_DRIVER_DOOR:
        return new CTaskComplexOpenDriverDoor(m_PedToArrest->m_pVehicle);

    case TASK_COMPLEX_CAR_OPEN_PASSENGER_DOOR:
        return new CTaskComplexOpenPassengerDoor(m_PedToArrest->m_pVehicle, 8); // todo: magic number

    case TASK_SIMPLE_WAIT_UNTIL_PED_OUT_CAR: {
        return new CTaskSimpleWaitUntilPedIsOutCar{m_PedToArrest, m_PedToArrest->GetPosition() - ped->GetPosition()};
    }
    default:
        return nullptr;
    }
}
