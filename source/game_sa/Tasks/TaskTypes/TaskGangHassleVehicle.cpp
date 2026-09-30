#include "StdInc.h"

#include "TaskGangHassleVehicle.h"

#include "TaskComplexLeaveCar.h"
#include "TaskComplexSmartFleeEntity.h"
#include "TaskComplexTrackEntity.h"

void CTaskGangHassleVehicle::InjectHooks() {
    RH_ScopedVirtualClass(CTaskGangHassleVehicle, 0x86F9D4, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x65FAC0);
    RH_ScopedInstall(Destructor, 0x65FB60);
    RH_ScopedInstall(GetTargetHeading, 0x65FDD0);
    RH_ScopedInstall(CalcTargetOffset, 0x6641A0);
    RH_ScopedInstall(Clone, 0x65FC00);
    RH_ScopedInstall(CreateNextSubTask, 0x65FC80);
    RH_ScopedInstall(CreateFirstSubTask, 0x664BA0);
    RH_ScopedInstall(ControlSubTask, 0x6637C0, { .reversed = false });
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
    const auto& mat = m_Vehicle->m_matrix->GetRight(); // +0x14 (RwMatrix)
    CVector2D dir;
    switch (m_nHasslePosId) {
    case 0:
    case 2:
        dir = CVector2D{ mat.x, mat.y };
        break;
    case 1:
    case 3:
        dir = CVector2D{ -mat.x, -mat.y };
        break;
    case 4:
        dir = CVector2D{ m_Vehicle->m_matrix->GetUp().x, m_Vehicle->m_matrix->GetUp().y };
        break;
    case 5:
        dir = CVector2D{ -m_Vehicle->m_matrix->GetUp().x, -m_Vehicle->m_matrix->GetUp().y };
        break;
    }
    return CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(dir.x, dir.y, 0.f, 0.f));
}

// 0x6641A0
// NOTE: Instruction-level port of the original (x87 FPU code, hence the
// inline assembly). The col-model lookup is done through the model-info pool
// directly, exactly like the original (fully inlined).
// 0x858B8C in the original's .rdata
static const float ORIG_HALF_858B8C = 0.5f;

__declspec(naked) void CTaskGangHassleVehicle::CalcTargetOffset() {
    __asm {
        sub esp, 0x18
        mov eax, dword ptr [ecx + 0x10]         ; m_Vehicle
        xor edx, edx
        mov dword ptr [ecx + 0x2C], edx         ; m_vecPosn.z = 0
        mov dword ptr [ecx + 0x28], edx         ; m_vecPosn.y = 0
        mov dword ptr [ecx + 0x24], edx         ; m_vecPosn.x = 0
        movsx eax, word ptr [eax + 0x22]        ; m_nModelIndex
        mov eax, dword ptr [eax*4 + 0xA9B0C8]   ; CModelInfo::ms_modelInfoPtrs[idx]
        mov eax, dword ptr [eax + 0x14]         ; -> m_pColModel
        push esi
        mov esi, eax
        push edi
        mov edi, dword ptr [esi]                ; bb.min.x
        mov dword ptr [esp + 8], edi
        mov edi, dword ptr [esi + 4]            ; bb.min.y
        mov esi, dword ptr [esi + 8]            ; bb.min.z
        add eax, 0xC                            ; &bb.max
        mov dword ptr [esp + 0x10], esi         ; bb.min.z
        mov esi, dword ptr [eax]                ; bb.max.x
        mov dword ptr [esp + 0x14], esi
        mov esi, dword ptr [eax + 4]            ; bb.max.y
        mov eax, dword ptr [eax + 8]            ; bb.max.z
        mov dword ptr [esp + 0x1C], eax
        mov eax, dword ptr [ecx + 0x14]         ; m_nHasslePosId
        cmp eax, edx
        mov dword ptr [esp + 0xC], edi          ; bb.min.y
        pop edi
        mov dword ptr [esp + 0x14], esi         ; bb.max.y
        pop esi
        jne case1
        fld dword ptr [esp]
        fsub dword ptr [ecx + 0x20]             ; - m_fOffsetX
        fstp dword ptr [ecx + 0x24]             ; m_vecPosn.x
        fld dword ptr [esp + 0x10]
        fmul ORIG_HALF_858B8C               ; * 0.5
        fstp dword ptr [ecx + 0x28]             ; m_vecPosn.y
        add esp, 0x18
        ret
case1:
        cmp eax, 1
        jne case2
        fld dword ptr [esp + 0xC]
        fadd dword ptr [ecx + 0x20]
        fstp dword ptr [ecx + 0x24]
        fld dword ptr [esp + 0x10]
        fmul ORIG_HALF_858B8C
        fstp dword ptr [ecx + 0x28]
        add esp, 0x18
        ret
case2:
        cmp eax, 2
        jne case3
        fld dword ptr [esp]
        fsub dword ptr [ecx + 0x20]
        fstp dword ptr [ecx + 0x24]
        fld dword ptr [esp + 4]
        fmul ORIG_HALF_858B8C
        fstp dword ptr [ecx + 0x28]
        add esp, 0x18
        ret
case3:
        cmp eax, 3
        jne case4
        fld dword ptr [esp + 0xC]
        fadd dword ptr [ecx + 0x20]
        fstp dword ptr [ecx + 0x24]
        fld dword ptr [esp + 4]
        fmul ORIG_HALF_858B8C
        fstp dword ptr [ecx + 0x28]
        add esp, 0x18
        ret
case4:
        cmp eax, 4
        jne case5
        fld dword ptr [esp + 4]
        fsub dword ptr [ecx + 0x20]
        fstp dword ptr [ecx + 0x28]
        add esp, 0x18
        ret
case5:
        cmp eax, 5
        jne done
        fld dword ptr [esp + 0x10]
        fadd dword ptr [ecx + 0x20]
        fstp dword ptr [ecx + 0x28]
done:
        add esp, 0x18
        ret
    }
}

// 0x65FC80
CTask* CTaskGangHassleVehicle::CreateNextSubTask(CPed* ped) {
    if (!m_Vehicle) {
        return nullptr;
    }

    if (m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_SMART_FLEE_ENTITY) { // 0x38F
        return nullptr;
    }

    // NOTE: The original checks vehicle + 0x4C0 (m_fWheelSpinForAudio) as a "is the car moving" proxy
    if (m_Vehicle->m_fWheelSpinForAudio >= 0.4f) {
        if (m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_GANG_HASSLE_PED) { // 0x4BC
            return nullptr;
        }
        if (m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_TRACK_ENTITY) { // 0x39D
            return nullptr;
        }
        return new CTaskComplexTrackEntity(m_Vehicle, m_vecPosn, 1, -1, 10.0f, 40.0f, 1); // 0x65F3B0
    } else {
        return new CTaskComplexSmartFleeEntity( // 0x65C430
            m_Vehicle,
            false,
            30.0f,
            StaticRef<int32>(0x86F674),
            StaticRef<int32>(0x86F678),
            StaticRef<float>(0xC18CF0)
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

    const auto& bbox = m_Vehicle->GetColModel()->GetBoundingBox();
    if (bbox.m_vecMax.x - bbox.m_vecMin.x > 0.05f || bbox.m_vecMax.y - bbox.m_vecMin.y > 0.0007f) {
        return nullptr;
    }

    m_nHasslePosId = m_Vehicle->GetSpareHasslePosId();
    if (m_nHasslePosId == -1) {
        return nullptr;
    }

    m_Vehicle->SetHasslePosId(m_nHasslePosId, true);
    CalcTargetOffset();
    m_b31 = 0;
    ped->DropEntityThatThisPedIsHolding(true); // 0x5E0360
    m_nTime = CTimer::GetTimeInMS();
    dword3C = CGeneral::GetRandomNumberInRange(150'000, 250'000);
    byte40 = 1;

    if (!ped->bInVehicle || !ped->m_pVehicle) {
        return CreateNextSubTask(ped);
    }

    return new CTaskComplexLeaveCar(ped->m_pVehicle, 0, 0, true, false); // 0x63B8C0
}

// 0x6637C0
CTask* CTaskGangHassleVehicle::ControlSubTask(CPed* ped) {
    return plugin::CallMethodAndReturn<CTask*, 0x6637C0, CTaskGangHassleVehicle*, CPed*>(this, ped);
}
