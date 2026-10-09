#include "StdInc.h"

#include "TaskGangHassleVehicle.h"
// Сабтаски, создаваемые в CreateNextSubTask / CreateFirstSubTask:
#include "TaskComplexTrackEntity.h"
#include "TaskComplexSmartFleeEntity.h"
#include "TaskComplexLeaveCar.h"

void CTaskGangHassleVehicle::InjectHooks() {
    RH_ScopedVirtualClass(CTaskGangHassleVehicle, 0x86F9D4, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x65FAC0);
    RH_ScopedInstall(Destructor, 0x65FB60);
    RH_ScopedInstall(GetTargetHeading, 0x65FDD0);
    RH_ScopedInstall(CalcTargetOffset, 0x6641A0);
    RH_ScopedInstall(Clone, 0x65FC00); // Тело - инлайн в хедере (TaskGangHassleVehicle.h:35), определения в .cpp нет
    RH_ScopedInstall(CreateNextSubTask, 0x65FC80);
    RH_ScopedInstall(CreateFirstSubTask, 0x664BA0);
    RH_ScopedInstall(ControlSubTask, 0x6637C0); // STUB_BLOCKED - редирект сохранён (вне батча)
}

// 0x65FAC0
CTaskGangHassleVehicle::CTaskGangHassleVehicle(CVehicle* vehicle, int32 a3, uint8 a4, float a5, float a6) : CTaskComplex() {
    m_nTime = 0;
    dword3C = 0;
    byte40 = 0;
    byte41 = 0;
    byte18 = a4;
    dword1C = a5;
    m_Vehicle = vehicle;
    m_nHasslePosId = -1;
    m_fOffsetX = a6;
    m_bRemoveAnim = 0;
    m_pEntity = nullptr;
    CEntity::SafeRegisterRef(m_Vehicle);
}

// 0x65FB60
CTaskGangHassleVehicle::~CTaskGangHassleVehicle() {
    if (m_Vehicle) {
        if (m_nHasslePosId > -1) {
            m_Vehicle->SetHasslePosId(m_nHasslePosId, false);
        }
        CEntity::SafeCleanUpRef(m_Vehicle);
    }

    CEntity::SafeCleanUpRef(m_pEntity);

    if (m_bRemoveAnim) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
        m_bRemoveAnim = false;
    }
}

// 0x65FDD0
float CTaskGangHassleVehicle::GetTargetHeading(CPed* ped) {
    assert(m_Vehicle);
    const auto* mat = m_Vehicle->m_matrix;
    CVector2D dir;
    switch (m_nHasslePosId) {
    case 0:
    case 2:
        dir = CVector2D{ mat->GetRight().x, mat->GetRight().y };
        break;
    case 1:
    case 3:
        dir = CVector2D{ -mat->GetRight().x, -mat->GetRight().y };
        break;
    case 4:
        dir = CVector2D{ mat->GetUp().x, mat->GetUp().y };
        break;
    case 5:
        dir = CVector2D{ -mat->GetUp().x, -mat->GetUp().y };
        break;
    }
    return CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(dir.x, dir.y, 0.f, 0.f));
}

// 0x6641A0
void CTaskGangHassleVehicle::CalcTargetOffset() {
    m_vecPosn = CVector{};
    const auto& bbox = CModelInfo::GetModelInfo(m_Vehicle->m_nModelIndex)->GetColModel()->GetBoundingBox();
    switch (m_nHasslePosId) {
    case 0:
        m_vecPosn.x = bbox.m_vecMin.x - m_fOffsetX;
        m_vecPosn.y = bbox.m_vecMax.y * 0.5f; // 0x858B8C
        break;
    case 1:
        m_vecPosn.x = bbox.m_vecMax.x + m_fOffsetX;
        m_vecPosn.y = bbox.m_vecMax.y * 0.5f; // 0x858B8C
        break;
    case 2:
        m_vecPosn.x = bbox.m_vecMin.x - m_fOffsetX;
        m_vecPosn.y = bbox.m_vecMin.y * 0.5f; // 0x858B8C
        break;
    case 3:
        m_vecPosn.x = bbox.m_vecMax.x + m_fOffsetX;
        m_vecPosn.y = bbox.m_vecMin.y * 0.5f; // 0x858B8C
        break;
    case 4:
        m_vecPosn.y = bbox.m_vecMin.y - m_fOffsetX;
        break;
    case 5:
        m_vecPosn.y = bbox.m_vecMax.y + m_fOffsetX;
        break;
    }
}

// 0x65FC80
CTask* CTaskGangHassleVehicle::CreateNextSubTask(CPed* ped) {
    if (!m_Vehicle) {
        return nullptr;
    }

    if (m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_SMART_FLEE_ENTITY) {
        return nullptr;
    }

    if (m_Vehicle->m_fHealth >= 250.0f) { // vehicle+0x4C0, порог 0x859F80
        if (m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_GANG_HASSLE_PED) {
            return nullptr;
        }
        if (m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_TRACK_ENTITY) {
            return nullptr;
        }
        return new CTaskComplexTrackEntity(m_Vehicle, m_vecPosn, 1, -1, 10.0f, 40.0f, 1); // 0x65F3B0
    } else {
        return new CTaskComplexSmartFleeEntity( // 0x65C430
            m_Vehicle,
            false,
            30.0f,                         // 0x41F00000
            StaticRef<int32>(0x86F674),    // fleeTime        = 1000000
            StaticRef<int32>(0x86F678),    // posCheckPeriod  = 1000
            StaticRef<float>(0xC18CF0)     // posChangeTolerance
        );
    }
}

// 0x664BA0
CTask* CTaskGangHassleVehicle::CreateFirstSubTask(CPed* ped) {
    if (!m_Vehicle) {
        return nullptr;
    }

    m_pEntity = m_Vehicle->m_pDriver;
    if (m_pEntity) {
        CEntity::SafeRegisterRef(m_pEntity);
    }

    const auto& bbox = CModelInfo::GetModelInfo(m_Vehicle->m_nModelIndex)->GetColModel()->GetBoundingBox(); // инлайн в оригинале
    if (bbox.m_vecMax.x - bbox.m_vecMin.x > 4.0f || bbox.m_vecMax.y - bbox.m_vecMin.y > 8.0f) { // 0x858B90, 0x859000
        return nullptr;
    }

    m_nHasslePosId = m_Vehicle->GetSpareHasslePosId();
    if (m_nHasslePosId == -1) {
        return nullptr;
    }

    m_Vehicle->SetHasslePosId(m_nHasslePosId, true);
    CalcTargetOffset();
    m_b31 = 0;
    ped->DropEntityThatThisPedIsHolding(true);
    m_nTime = CTimer::GetTimeInMS();
    dword3C = CGeneral::GetRandomNumberInRange(150'000, 250'000);
    byte40  = 1;

    if (!ped->bInVehicle || !ped->m_pVehicle) {
        return CreateNextSubTask(ped);
    }

    return new CTaskComplexLeaveCar(ped->m_pVehicle, 0, 0, true, false); // 0x63B8C0
}

// 0x6637C0
CTask* CTaskGangHassleVehicle::ControlSubTask(CPed* ped) {
    return plugin::CallMethodAndReturn<CTask*, 0x6637C0, CTaskGangHassleVehicle*, CPed*>(this, ped);
}
