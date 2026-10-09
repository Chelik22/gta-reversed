// ============================================================================
// INTEGRATED FILE — source/game_sa/Tasks/TaskTypes/TaskComplexArrestPed.cpp
// Батч: w2-41-TaskComplexArrestPed (манифест batches2/w2-07-mixed.json, 3 функции)
//
// ИСТОЧНИКИ:
//   - Фрагменты batch_out: ОТСУТСТВУЮТ. Каталоги w2-41-TaskComplexArrestPed/ и
//     w2-07-mixed/ пусты; grep по адресам 0x690220/0x68D350/0x68CF80 и по имени
//     "ArrestPed" по всем подкаталогам D:\projects\keep\batch_out — 0 попаданий.
//     По брифу применён fallback: «тела уже в текущем .cpp — снять {.Reversed = false}».
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
// НЕ РЕВЕРСИРОВАНО (1 из 3):
//   - CreateNextSubTask (0x690220): тела НЕТ нигде — ни в batch_out, ни в стише
//     (stash_wt), ни в апстриме gta-reversed (проверена вся история файла: коммиты
//     4d1f630b#232 … c4f0776c#1439 — всегда только call-заглушка «See #gists in
//     discord» и {.Reversed = false}). Декомпил D:\projects\keep\decompiles\0x690220.c
//     существует, но содержит артефакты декомпилятора (испорченный свитч по типам
//     подзадач, битые bit-тесты) — восстановление из него требует отдельной полной
//     выверки состояния машины ареста, за пределами этого слияния. Флаг
//     {.Reversed = false} и call-заглушка СОХРАНЕНЫ сознательно: функция остаётся
//     нативной (тело-заглушка просто вызывает оригинальный адрес 0x690220).
//   - Красным по декомпилу 0x690220 сверять было нечего; ключевые ветки живого
//     ControlSubTask сверены с 0x68D350 (0x516=TASK_FINISHED, 0x44c=TASK_SIMPLE_ARREST_PED,
//     0x38b=TASK_COMPLEX_SEEK_ENTITY, 0x33d=TASK_SIMPLE_WAIT_UNTIL_PED_OUT_CAR,
//     SetDownTime(100000), |dz|<=2.0f(0x858CA0), dist²<=3.0f²(0x87097C)) — совпадает.
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
    RH_ScopedVMTInstall(CreateNextSubTask, 0x690220, { .Reversed = false }); // тела нет нигде (см. шапку) — хук оставлен выключенным
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x6907A0);
    RH_ScopedVMTInstall(ControlSubTask, 0x68D350); // {.Reversed = false} снято — тело было мёртвым кодом в репо
    RH_ScopedInstall(CreateSubTask, 0x68CF80);     // {.Reversed = false} снято — тело было мёртвым кодом в репо
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

// 0x690220 See #gists in discord
// НЕ РЕВЕРСИРОВАНО: тела нет ни в репо, ни в стише, ни в апстриме (вся история файла),
// фрагментов batch_out нет. Заглушка-редирект сохранена, хук выключен.
CTask* CTaskComplexArrestPed::CreateNextSubTask(CPed* ped) {
    return plugin::CallMethodAndReturn<CTask*, 0x690220, CTaskComplexArrestPed*, CPed*>(this, ped);
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

// 0x68D350

// 0x0
CTask* CTaskComplexArrestPed::ControlSubTask(CPed* ped) {
    // Automatically make ped say something on function return
    const notsa::ScopeGuard Have_A_Nice_Day_Sir{
        [this, ped] {
            if (m_PedToArrest && m_PedToArrest->IsPlayer()) {
                if (FindPlayerWanted()->m_NumCopsInPursuit == 1) {
                    ped->Say(CTX_GLOBAL_SOLO);
                }
            }
        }
    };

    // Tries to abort current sub-task and replace it with `taskType`.
    const auto TryReplaceSubTask = [this, ped](auto taskType) {
        // Inverted `if` and got rid of `taskType == TASK_NONE` (in which case `m_pSubTask` was returned always)
        if (m_pSubTask->MakeAbortable(ped)) {
            return CreateSubTask(taskType, ped);
        } else {
            return m_pSubTask;
        }
    };

    const auto DoDestroyCarTask = [&] {
        MakeSurePedHasWeaponInHand(ped);
        return TryReplaceSubTask(TASK_COMPLEX_DESTROY_CAR);
    };

    // 0x68D370, 0x68D37F
    if (!m_PedToArrest || m_PedToArrest->m_fHealth <= 0.f) {
        return TryReplaceSubTask(TASK_FINISHED);
    }

    // 0x68D39F
    if (m_bSubTaskNeedsToBeCreated) {
        if (m_pSubTask->MakeAbortable(ped)) {
            m_pSubTask->AsComplex()->CreateFirstSubTask(ped);
        }
        return m_pSubTask;
    }

    // 0x68D3CB
    if (m_PedToArrest->bIsBeingArrested) {
        switch (m_pSubTask->GetTaskType()) {
        case TASK_SIMPLE_ARREST_PED:
        case TASK_COMPLEX_SEEK_ENTITY:
            break;
        default:
            return TryReplaceSubTask(TASK_COMPLEX_SEEK_ENTITY);
        }
    }

    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_DRAG_PED_FROM_CAR: // 0x68D49D
    case TASK_COMPLEX_DESTROY_CAR: { // 0x68D5F6
        if (!m_PedToArrest->bInVehicle) { // If not in vehicle anymore, try to kill them
            return TryReplaceSubTask(TASK_COMPLEX_KILL_PED_ON_FOOT);
        }
        break;
    }
    case TASK_COMPLEX_KILL_PED_ON_FOOT: { // 0x68D626
        // See if ped is falling, and is close enough
        if (const auto task = static_cast<CTaskComplexFallAndGetUp*>(m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_FALL_AND_GET_UP))) {
            if (task->IsFalling()) {
                const auto dir = ped->GetPosition() - m_PedToArrest->GetPosition();
                if (std::abs(dir.z) <= 2.f && dir.SquaredMagnitude() <= sq(3.0f)) {
                    task->SetDownTime(100'000);
                    return TryReplaceSubTask(TASK_SIMPLE_ARREST_PED);
                }
            }

            // Getting up already/not close enough
            return TryReplaceSubTask(TASK_COMPLEX_SEEK_ENTITY);
        }

        if (!m_PedToArrest->bInVehicle || !m_PedToArrest->m_pVehicle) {
            break;
        }

        // Ped has gotten into a vehicle, we need a different task!

        if (   m_PedToArrest->m_pVehicle->IsBoat()
            || m_PedToArrest->m_pVehicle->IsSubPlane()
            || m_PedToArrest->m_pVehicle->IsSubHeli()
        ) {
            return DoDestroyCarTask();
        }

        if (!ped->GetActiveWeapon().IsTypeMelee()) {
            if (!FindPlayerWanted()->IsClosestCop(ped->AsCop(), 2)) {
                return DoDestroyCarTask();
            }
        }

        if (m_PedToArrest->m_pVehicle != m_Vehicle) {
            if (m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_LEAVE_CAR)) {
                if (m_PedToArrest->m_pVehicle->IsBike() || m_PedToArrest->m_pVehicle->IsSubQuad()) {
                    return TryReplaceSubTask(TASK_COMPLEX_DRAG_PED_FROM_CAR);
                }

                if (!m_PedToArrest->m_pVehicle->IsUpsideDown() && !m_PedToArrest->m_pVehicle->IsOnItsSide()) {
                    return TryReplaceSubTask(TASK_COMPLEX_CAR_OPEN_DRIVER_DOOR);
                }

                return DoDestroyCarTask();
            }
        }

        break;
    }
    case TASK_COMPLEX_CAR_OPEN_DRIVER_DOOR: { // 0x68D424
        if (m_PedToArrest->bInVehicle) {
            // Maybe wait until ped gets out of the car..
            if (const auto task = m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_LEAVE_CAR)) {
                if (ped->IsEntityInRange(m_PedToArrest, 5.f)) {
                    return TryReplaceSubTask(TASK_SIMPLE_WAIT_UNTIL_PED_OUT_CAR);
                }
            }

            // Ped can't open driver door, but we can open front right door?
            if (!m_PedToArrest->m_pVehicle->IsRoomForPedToLeaveCar(TARGET_DOOR_DRIVER, nullptr)) {
                if (m_PedToArrest->m_pVehicle->IsRoomForPedToLeaveCar(TARGET_DOOR_FRONT_RIGHT, nullptr)) {
                    return TryReplaceSubTask(TASK_COMPLEX_CAR_OPEN_PASSENGER_DOOR);
                }
            }
        }

        // Ped not in vehicle any more vehicle, just kill 'em
        return TryReplaceSubTask(TASK_COMPLEX_KILL_PED_ON_FOOT);
    }
    case TASK_COMPLEX_CAR_OPEN_PASSENGER_DOOR: { // 0x68D510
        // Pretty much the copy of the above, with minor changes (See change 1,2)

        if (m_PedToArrest->bInVehicle) {
            // Maybe wait until ped gets out of the car..
            if (const auto task = m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_LEAVE_CAR)) {
                if (ped->IsEntityInRange(m_PedToArrest, 5.f)) {
                    return TryReplaceSubTask(TASK_SIMPLE_WAIT_UNTIL_PED_OUT_CAR);
                }
            }

            // Ped can't open passenger door, but driver door can be opened by us? - Change 1
            if (!m_PedToArrest->m_pVehicle->IsRoomForPedToLeaveCar(TARGET_DOOR_FRONT_RIGHT, nullptr)) {
                if (m_PedToArrest->m_pVehicle->IsRoomForPedToLeaveCar(TARGET_DOOR_DRIVER, nullptr)) {
                    return TryReplaceSubTask(TASK_COMPLEX_CAR_OPEN_DRIVER_DOOR);
                }
            }
        }

        // Change 2
        return DoDestroyCarTask();
    }
    }

    // Just continue with current sub-task
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
