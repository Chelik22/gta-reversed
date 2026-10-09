#include "StdInc.h"

#include "TaskGoToVehicleAndLean.h"
// Batch w2-11-mixed: includes needed by the integrated bodies.
#include "TaskSimpleAchieveHeading.h"          // CTaskSimpleAchieveHeading (CreateNextSubTask)
#include "TaskSimpleStandStill.h"              // CTaskSimpleStandStill (CreateNextSubTask)
#include "TaskLeanOnVehicle.h"                 // CTaskLeanOnVehicle (CreateNextSubTask)
#include "TaskComplexGoToPointAndStandStill.h" // CTaskComplexGoToPointAndStandStill (CreateFirstSubTask)
#include "TaskComplexGangLeader.h"             // CTaskComplexGangLeader::ShouldLoadGangAnims (ControlSubTask)
#include "Animation/AnimManager.h"             // CAnimManager (gangs anim block refs)
#include "Streaming.h"                         // CStreaming::RequestModel, IFPToModelId, STREAMING_KEEP_IN_MEMORY

// File-scope: параметры CTaskSimpleAchieveHeading — глобалы .data оригинала (0.5f / 0.2f)
static auto& ACHIEVE_HEADING_RATE       = StaticRef<float>(0x86FC7C); // = 0.5f
static auto& ACHIEVE_HEADING_TOLERANCE = StaticRef<float>(0x86FC80); // = 0.2f

void CTaskGoToVehicleAndLean::InjectHooks() {
    RH_ScopedVirtualClass(CTaskGoToVehicleAndLean, 0x86FAC8, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x660E60);
    RH_ScopedInstall(Destructor, 0x660EE0);
    RH_ScopedVMTInstall(Clone, 0x6621B0);
    RH_ScopedVMTInstall(MakeAbortable, 0x664500);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x664590);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x664D40);
    RH_ScopedVMTInstall(ControlSubTask, 0x664E60);
}

// 0x660E60
CTaskGoToVehicleAndLean::CTaskGoToVehicleAndLean(CVehicle* vehicle, int32 leanAnimDurationInMs) : CTaskComplex() {
    m_Vehicle = vehicle;
    m_LeanAnimDurationInMs = leanAnimDurationInMs;
    m_LeanOnVehicle = false;
    CEntity::SafeRegisterRef(m_Vehicle);
}

// 0x660EE0
CTaskGoToVehicleAndLean::~CTaskGoToVehicleAndLean() {
    if (m_LeanOnVehicle) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
        m_LeanOnVehicle = false;
    }
    CEntity::SafeCleanUpRef(m_Vehicle);
}

// 0x6621B0
CTask* CTaskGoToVehicleAndLean::Clone() const {
    // Оригинал: operator new(0x24) + ctor(m_Vehicle, m_LeanAnimDurationInMs);
    // размер 0x24 = m_vecTargetPos добавлен в хедер (VALIDATE_SIZE 0x24).
    return new CTaskGoToVehicleAndLean(m_Vehicle, m_LeanAnimDurationInMs);
}

// 0x664500
bool CTaskGoToVehicleAndLean::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (priority == ABORT_PRIORITY_IMMEDIATE) { // param_3 == 2
        m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, event); // NB: в оригинале без null-чека
        if (m_Vehicle) {
            m_Vehicle->vehicleFlags.bHasGangLeaningOn = false;
        }
        if (m_pSubTask) {
            m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, nullptr);
        }
    } else {
        if (!m_pSubTask->MakeAbortable(ped, priority, event)) { // NB: в оригинале без null-чека
            return false;
        }
        if (m_Vehicle) {
            m_Vehicle->vehicleFlags.bHasGangLeaningOn = false;
        }
        if (m_pSubTask) {
            m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, nullptr);
        }
    }
    return true;
}

// 0x664590
CTask* CTaskGoToVehicleAndLean::CreateNextSubTask(CPed* ped) {
    if (!m_Vehicle) {
        if (m_pSubTask) {
            m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, nullptr); // 2 == IMMEDIATE
        }
        return nullptr;
    }

    const auto subTaskType = m_pSubTask->GetTaskType(); // NB: в оригинале без null-чека
    if (subTaskType == TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL) {
        // Дошли до точки: повернуться боком к машине
        auto dirX = m_Vehicle->m_matrix->GetRight().x;
        auto dirY = m_Vehicle->m_matrix->GetRight().y;
        if (field_15 == 0) { // сторона захода (заполняется CalcTargetPos 0x664770)
            dirX = -dirX;
            dirY = -dirY;
        }
        const auto heading = CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(dirX, dirY, 0.f, 0.f));
        return new CTaskSimpleAchieveHeading(heading, ACHIEVE_HEADING_RATE, ACHIEVE_HEADING_TOLERANCE); // 0x667E20
    }

    if (subTaskType != TASK_SIMPLE_ACHIEVE_HEADING && subTaskType != TASK_SIMPLE_STAND_STILL) {
        DoTidyUp(ped); // 0x660F60
        return nullptr;
    }

    if (!m_LeanOnVehicle) {
        return new CTaskSimpleStandStill(500, false, false, 8.0f); // 0x62F310
    }
    return new CTaskLeanOnVehicle(m_Vehicle, m_LeanAnimDurationInMs, 0); // 0x660F90
}

// 0x664D40
CTask* CTaskGoToVehicleAndLean::CreateFirstSubTask(CPed* ped) {
    if (m_Vehicle) {
        if (!m_Vehicle->vehicleFlags.bHasGangLeaningOn) {
            m_Vehicle->vehicleFlags.bHasGangLeaningOn = true;

            CVector targetPos;
            plugin::CallMethod<0x664770, CTaskGoToVehicleAndLean*, CVector*, CPed*>(this, &targetPos, ped); // CalcTargetPos (sret, нереверснута)
            m_vecTargetPos = targetPos;

            const auto pedPos = ped->m_matrix
                ? ped->m_matrix->GetPosition()
                : ped->m_placement.m_vPosn;

            if ((m_vecTargetPos - pedPos).SquaredMagnitude() >= 1.0f) { // 0x858624 = 1.0f
                return new CTaskComplexGoToPointAndStandStill(PEDMOVE_WALK, m_vecTargetPos, 0.05f, 2.0f, false, true);
                // 0x668120: (4, &pos, 0x3D4CCCCD, 0x40000000, 0, 1)
            }
            return nullptr; // уже на месте
        }
        m_Vehicle->vehicleFlags.bHasGangLeaningOn = false; // флаг уже стоял -> снять и аборт
    }

    if (m_pSubTask) {
        m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, nullptr); // 2 == IMMEDIATE
    }
    return nullptr;
}

// 0x664E60
CTask* CTaskGoToVehicleAndLean::ControlSubTask(CPed* ped) {
    if (!m_Vehicle) {
        if (m_pSubTask) {
            m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, nullptr); // 2 == IMMEDIATE
        }
        return nullptr;
    }

    // Пед слишком долго "статичен" => хватит облокачиваться
    if (30 < ped->GetIntelligence()->m_AnotherStaticCounter) { // 0x86C938 == 30 (PedIntelligence.h VALIDATE_OFFSET 0x274)
        m_Vehicle->vehicleFlags.bHasGangLeaningOn = false;
        if (m_pSubTask) {
            m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, nullptr);
        }
        return nullptr;
    }

    // Позиция машины (фолбэк-квоирк оригинала: чтение 12 байт CTask-базы (m_Parent) как CVector)
    const auto vehPos = m_Vehicle->m_matrix
        ? m_Vehicle->m_matrix->GetPosition()
        : *reinterpret_cast<CVector*>(reinterpret_cast<uint8*>(this) + 0x4); // sic: m_Parent как позиция
    const auto pedPos = ped->m_matrix
        ? ped->m_matrix->GetPosition()
        : ped->m_placement.m_vPosn;

    const auto speedAtPoint = ped->GetSpeed(pedPos - vehPos); // 0x542CE0 CPhysical::GetSpeed(CVector)
    if (m_Vehicle->m_pDriver || speedAtPoint.Magnitude() > 0.01f) { // 0x858C58
        if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) { // 1 == URGENT; NB: в оригинале без null-чека
            DoTidyUp(ped); // 0x660F60
            return nullptr;
        }
    }

    // Задача уползла от исходной цели => отмена
    CVector targetPos;
    plugin::CallMethod<0x664770, CTaskGoToVehicleAndLean*, CVector*, CPed*>(this, &targetPos, ped); // CalcTargetPos (sret)
    if ((targetPos - m_vecTargetPos).SquaredMagnitude() > 0.01f) { // 0x86CD7C
        if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
            DoTidyUp(ped);
            return nullptr;
        }
    }

    // Блок ссылок на анимблок "gangs"
    if (m_LeanOnVehicle) {
        if (!CTaskComplexGangLeader::ShouldLoadGangAnims()) { // 0x65E7F0
            CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs")); // 0x4D3FD0/0x4D3990
            m_LeanOnVehicle = false;
        }
    } else {
        if (CTaskComplexGangLeader::ShouldLoadGangAnims()) {
            const auto blockIdx = CAnimManager::GetAnimationBlockIndex("gangs"); // 0x4D3990
            if (!CAnimManager::GetAnimationBlock("gangs")->IsLoaded) { // = ms_aAnimBlocks[blockIdx].IsLoaded (ms_aAnimBlocks is private - public getter used)
                CStreaming::RequestModel(IFPToModelId(blockIdx), STREAMING_KEEP_IN_MEMORY); // FUN_004087E0(idx + 0x63E7, 8)
                return m_pSubTask;
            }
            CAnimManager::AddAnimBlockRef(blockIdx); // 0x4D3FB0
            m_LeanOnVehicle = true;
        }
    }
    return m_pSubTask;
}

// 0x660F60
void CTaskGoToVehicleAndLean::DoTidyUp(CPed* ped) {
    if (m_Vehicle)
        m_Vehicle->vehicleFlags.bHasGangLeaningOn = false;

    if (m_pSubTask)
        m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, nullptr);
}
