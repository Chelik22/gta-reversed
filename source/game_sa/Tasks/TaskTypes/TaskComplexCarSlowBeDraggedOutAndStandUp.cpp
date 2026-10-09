#include "StdInc.h"

#include "TaskComplexCarSlowBeDraggedOutAndStandUp.h"

// w2-08: реверснуты 4 функции (0x648710 CreateSubTask, 0x6488F0 CreateNextSubTask,
// 0x648A10 CreateFirstSubTask, 0x648B80 ControlSubTask). Стабы Clone (0x64A190) и
// MakeAbortable (0x6486F0) НЕ из батча — оставлены как есть.
// Уже покрыто StdInc.h (отдельные include не нужны):
// World.h (FindPlayerPed), Events/EventDeath.h, Events/EventGroup.h,
// Enums/AnimationEnums.h, Enums/eTargetDoor.h, Enums/eTaskType.h,
// Entity/Ped/Ped.h (CPed::GetAE / m_fHealth / m_pVehicle / IsAlive / SetRadioStation).
#include "TaskComplexGetUpAndStandStill.h"  // CTaskComplexGetUpAndStandStill (CreateSubTask / CreateNextSubTask)
#include "TaskComplexCarSlowBeDraggedOut.h" // CTaskComplexCarSlowBeDraggedOut (CreateSubTask)
#include "TaskSimpleCarSetPedOut.h"         // CTaskSimpleCarSetPedOut (CreateSubTask)
#include "TaskComplexLeaveCar.h"            // CTaskComplexLeaveCar (CreateSubTask)

void CTaskComplexCarSlowBeDraggedOutAndStandUp::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexCarSlowBeDraggedOutAndStandUp, 0x86EF80, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x648620);
    RH_ScopedInstall(Destructor, 0x648690);
    RH_ScopedInstall(CreateSubTask, 0x648710);
    RH_ScopedInstall(Clone, 0x64A190, { .Reversed = false });         // НЕ из батча — стаб сохранён
    RH_ScopedInstall(GetTaskType, 0x648680);
    RH_ScopedInstall(MakeAbortable, 0x6486F0, { .Reversed = false }); // НЕ из батча — стаб сохранён
    RH_ScopedVMTInstall(CreateNextSubTask, 0x6488F0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x648A10);
    RH_ScopedVMTInstall(ControlSubTask, 0x648B80);
}

// 0x648620
CTaskComplexCarSlowBeDraggedOutAndStandUp::CTaskComplexCarSlowBeDraggedOutAndStandUp(CVehicle* vehicle, int32 a3) : CTaskComplex() {
    m_Vehicle = vehicle;
    dword10 = a3;
    CEntity::SafeRegisterRef(m_Vehicle);
}

// 0x648690
CTaskComplexCarSlowBeDraggedOutAndStandUp::~CTaskComplexCarSlowBeDraggedOutAndStandUp() {
    CEntity::SafeCleanUpRef(m_Vehicle);
}

// 0x648710
CTask* CTaskComplexCarSlowBeDraggedOutAndStandUp::CreateSubTask(eTaskType taskType, CPed* ped) {
    switch (taskType) {
    case TASK_SIMPLE_CAR_SET_PED_OUT: // 0x330
        return new CTaskSimpleCarSetPedOut(ped->m_pVehicle, static_cast<eTargetDoor>(dword10), true);

    case TASK_COMPLEX_GET_UP_AND_STAND_STILL: // 0xCE
        return new CTaskComplexGetUpAndStandStill();

    case TASK_COMPLEX_LEAVE_CAR: { // 0x2C0
        const auto animGroup = m_Vehicle->m_pHandlingData->GetAnimGroupId();
        if (animGroup == ANIM_GROUP_COACHCARANIMS) {
            return new CTaskComplexLeaveCar(ped->m_pVehicle, TARGET_DOOR_FRONT_RIGHT, 0, false, true);
        }
        if (animGroup == ANIM_GROUP_TANKCARANIMS) {
            return new CTaskComplexLeaveCar(ped->m_pVehicle, TARGET_DOOR_DRIVER, 0, false, true);
        }
        if (dword10 == static_cast<int32>(TARGET_DOOR_FRONT_RIGHT) && animGroup == ANIM_GROUP_BUSCARANIMS) {
            return new CTaskComplexLeaveCar(ped->m_pVehicle, TARGET_DOOR_FRONT_RIGHT, 0, false, true);
        }
        return nullptr;
    }

    case TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT: // 0x337
        return new CTaskComplexCarSlowBeDraggedOut(m_Vehicle, static_cast<eTargetDoor>(dword10), false);

    default: // включая TASK_FINISHED (0x516)
        return nullptr;
    }
}

// 0x6486F0 -- NOT REVERSED, НЕ из батча (стаб сохранён)
bool CTaskComplexCarSlowBeDraggedOutAndStandUp::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    return plugin::CallMethodAndReturn<bool, 0x6486F0, CTaskComplexCarSlowBeDraggedOutAndStandUp*, CPed*, int32, CEvent const*>(this, ped, priority, event);
}

// 0x6488F0
// - 0x5E0170 = CPed::IsAlive (по m_nPedState), 0x5DF8F0 = CPed::IsPlayer (0x598 == 0/1),
//   0x4ADDF0 = CEventDeath(bool), 0x4AB420 = CEventGroup::Add(CEvent*, bool), 0x4ADE90 = ~CEventDeath.
// - Событие смерти добавляется, когда педа вытащили мёртвым/умирающим:
//   asm: fld [ped+0x540] (m_fHealth) fcomp [0x858B50] (0.0f) -> ветка смерти при health <= 0.
// - В кейсе 0xCE оригинал вызывает ped->IsPlayer() и отбрасывает результат (сохранено как есть).
CTask* CTaskComplexCarSlowBeDraggedOutAndStandUp::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT: { // 0x337
        if (ped->IsAlive() && ped->m_fHealth > 0.0f) {
            return new CTaskComplexGetUpAndStandStill();
        }
        CEventDeath event(false); // not drowning
        ped->GetEventGroup().Add(&event, false);
        return nullptr;
    }
    case TASK_COMPLEX_GET_UP_AND_STAND_STILL: // 0xCE
        ped->IsPlayer(); // NOTE: оригинал вызывает, результат не используется
        return nullptr;

    case TASK_SIMPLE_CAR_SET_PED_OUT: // 0x330
    case TASK_COMPLEX_LEAVE_CAR:      // 0x2C0
    default:
        return nullptr;
    }
}

// 0x648A10
// - 0x56E210 = CWorld::FindPlayerPed(0); 0x4F5BA0 = CAEVehicleAudioEntity::PlayerAboutToExitVehicleAsDriver
//   (this = vehicle+0x138 = m_vehicleAudio); 0x5DFD90 = CPed::SetRadioStation;
//   0x4E2BB0 = CAEPedAudioEntity::AddAudioEvent (this = ped+0x138 = m_pedAudio, т.е. ped->GetAE()).
// - Оригинальный switch идёт по сырому байту m_pHandlingData->m_nAnimGroup (0..0x1C),
//   прыжковая таблица: {0,0xD,0xE,0x12,0x13,0x16,0x17,0x1C} -> punch/headbang, {1} -> kick, {0x11} -> dozer.
//   Здесь он переписан через GetAnimGroupId() (== ANIM_GROUP_STDCARAMIMS + m_nAnimGroup, tHandlingData.h:27)
//   с именованными группами: 0=STDCARAMIMS, 1=LOWCARAMIMS, 0xD=VANCARANIMS, 0xE=RUSTPLANEANIMS,
//   0x11=DOZERCARANIMS, 0x12=KARTCARANIMS, 0x13=CONVCARANIMS, 0x16=STDTALLCARAMIMS, 0x17=HOVERCARANIMS, 0x1C=STDCARUPRIGHT.
// - Первые проверки (coach/tank/bus) тоже сравнивают (m_nAnimGroup + 0x58) с 0x67/0x70/0x68.
// - Null-чеков m_pVehicle/m_pHandlingData в оригинале нет — чтение идёт напрямую (сохранено).
CTask* CTaskComplexCarSlowBeDraggedOutAndStandUp::CreateFirstSubTask(CPed* ped) {
    if (ped == FindPlayerPed(0)) {
        if (ped->m_pVehicle) {
            ped->m_pVehicle->m_vehicleAudio.PlayerAboutToExitVehicleAsDriver();
        }
    } else if (ped->m_pVehicle && ped->m_pVehicle->m_pDriver == ped) {
        ped->SetRadioStation();
    }

    const auto veh       = ped->m_pVehicle;
    const auto animGroup = veh->m_pHandlingData->GetAnimGroupId();

    // Автобус/коуч/танк вылезают через TASK_COMPLEX_LEAVE_CAR с фиксированной дверью
    if (animGroup == ANIM_GROUP_COACHCARANIMS
        || animGroup == ANIM_GROUP_TANKCARANIMS
        || (animGroup == ANIM_GROUP_BUSCARANIMS && dword10 == static_cast<int32>(TARGET_DOOR_FRONT_RIGHT))
    ) {
        return CreateSubTask(TASK_COMPLEX_LEAVE_CAR, ped);
    }

    // Звук "вытаскивания" зависит от аним-группы машины
    switch (animGroup) {
    case ANIM_GROUP_STDCARAMIMS:
    case ANIM_GROUP_VANCARANIMS:
    case ANIM_GROUP_RUSTPLANEANIMS:
    case ANIM_GROUP_KARTCARANIMS:
    case ANIM_GROUP_CONVCARANIMS:
    case ANIM_GROUP_STDTALLCARAMIMS:
    case ANIM_GROUP_HOVERCARANIMS:
    case ANIM_GROUP_STDCARUPRIGHT:
        if (dword10 == static_cast<int32>(TARGET_DOOR_DRIVER)) {
            ped->GetAE().AddAudioEvent(AE_PED_JACKED_CAR_PUNCH);
        } else if (dword10 == static_cast<int32>(TARGET_DOOR_FRONT_RIGHT)) {
            ped->GetAE().AddAudioEvent(AE_PED_JACKED_CAR_HEAD_BANG);
        }
        break;
    case ANIM_GROUP_LOWCARAMIMS:
        ped->GetAE().AddAudioEvent(AE_PED_JACKED_CAR_KICK);
        break;
    case ANIM_GROUP_DOZERCARANIMS:
        ped->GetAE().AddAudioEvent(AE_PED_JACKED_DOZER);
        break;
    }

    return CreateSubTask(TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT, ped);
}

// 0x64A190 Clone -- НЕ из батча: тела в .cpp нет (inline в хедере TaskComplexCarSlowBeDraggedOutAndStandUp.h:20),
// хук остаётся с {.Reversed = false}.

// 0x648B80
CTask* CTaskComplexCarSlowBeDraggedOutAndStandUp::ControlSubTask(CPed* ped) {
    return m_pSubTask;
}
