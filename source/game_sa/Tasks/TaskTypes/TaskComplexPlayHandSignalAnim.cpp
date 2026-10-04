#include "StdInc.h"
#include "TaskComplexPlayHandSignalAnim.h"

#include "TaskSimplePlayHandSignalAnim.h"
#include "TaskSimpleStandStill.h"

void CTaskComplexPlayHandSignalAnim::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexPlayHandSignalAnim, 0x86d5dc, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x61B2B0);
    RH_ScopedInstall(Destructor, 0x61BDF0);

    RH_ScopedInstall(GetAnimIdForPed, 0x61B460);
    RH_ScopedInstall(CreateSubTask, 0x61B2F0);

    RH_ScopedVMTInstall(Clone, 0x61BA00, {.reversed = false});
    RH_ScopedVMTInstall(GetTaskType, 0x61B2E0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x61B570, {.reversed = false});
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x61B4F0, {.reversed = false});
    RH_ScopedVMTInstall(ControlSubTask, 0x61B580, {.reversed = false});
}

// 0x61B2B0
CTaskComplexPlayHandSignalAnim::CTaskComplexPlayHandSignalAnim(AnimationId animationId, float blendFactor) :
    m_animationId{animationId},
    m_AnimBlenDelta{blendFactor}
{
}

// 0x61BDF0
CTaskComplexPlayHandSignalAnim::~CTaskComplexPlayHandSignalAnim() {
    enum {
        RIGHT,
        LEFT,
    };
    enum {
        NONFAT,
        FAT
    };
    const eModelID handModels[2][2]{
        // nonfat           fat
        { MODEL_SHANDL, MODEL_FHANDL }, // left
        { MODEL_SHANDR, MODEL_FHANDR }  // right
    };

    // Remove hand model refs
    for (const auto i : { LEFT, RIGHT }) {
        CModelInfo::GetModelInfo(handModels[i][m_DoUseFatHands ? FAT : NONFAT])->RemoveRef();
    }

    // Deal with anim
    if (m_bAnimationLoaded) { // Remove anim ref
        CAnimManager::RemoveAnimBlockRef(ms_animBlock);
    } else if (ms_animBlock != -1 && !CAnimManager::GetAnimBlocks()[ms_animBlock].RefCnt) { 
        if (!rng::all_of(std::array{ LEFT, RIGHT }, [&, this](auto i) { // Unload anim block if not all of the models has refs
            return CModelInfo::GetModelInfo(handModels[i][m_DoUseFatHands ? FAT : NONFAT])->m_nRefCount != 0;
        })) {
            CStreaming::RemoveModel(IFPToModelId(ms_animBlock));        
        }
    }
}

// 0x61B460
AnimationId CTaskComplexPlayHandSignalAnim::GetAnimIdForPed(CPed* ped) {
    switch (ped->m_nPedType) {
    case PED_TYPE_COP:    return (AnimationId)0x140; // 320
    case PED_TYPE_GANG1:  return (AnimationId)0x141; // 321
    case PED_TYPE_GANG2:  return (AnimationId)0x142; // 322
    case PED_TYPE_GANG7:  return (AnimationId)0x144; // 324
    case PED_TYPE_GANG10: return (AnimationId)0x143; // 323
    default:              return ANIM_ID_UNDEFINED; // -1
    }
}

// 0x61B2F0
CTask* CTaskComplexPlayHandSignalAnim::CreateSubTask(eTaskType taskType) {
    switch (taskType) {
    case TASK_SIMPLE_STAND_STILL: { // 203
        // Simple stand still for 8.0 seconds
        return new CTaskSimpleStandStill(0, true, false, 8.0f);
    }
    case TASK_SIMPLE_HANDSIGNAL_ANIM: { // 425 - sub task creation
        auto* seq = new CTaskComplexSequence{};

        // Stand still for a random time in [0, 1500]
        const auto delay = CGeneral::GetRandomNumberInRange(0, 1500);
        seq->AddTask(new CTaskSimpleStandStill(delay, false, false, 8.0f));

        // Play the hand signal anim
        seq->AddTask(new CTaskSimplePlayHandSignalAnim(
            m_animationId,
            m_AnimBlenDelta,
            m_DoUseFatHands,
            false
        ));

        return seq;
    }
    default:
        return nullptr;
    }
}

// 0x61BA00
CTask* CTaskComplexPlayHandSignalAnim::Clone() const {
    return plugin::CallMethodAndReturn<CTask*, 0x61BA00, const CTaskComplexPlayHandSignalAnim*>(this);
}

// 0x61B570
CTask* CTaskComplexPlayHandSignalAnim::CreateNextSubTask(CPed* ped) {
    return plugin::CallMethodAndReturn<CTask*, 0x61B570, CTaskComplexPlayHandSignalAnim*, CPed*>(this, ped);
}

// 0x61B4F0
CTask* CTaskComplexPlayHandSignalAnim::CreateFirstSubTask(CPed* ped) {
    return plugin::CallMethodAndReturn<CTask*, 0x61B4F0, CTaskComplexPlayHandSignalAnim*, CPed*>(this, ped);
}

// 0x61B580
CTask* CTaskComplexPlayHandSignalAnim::ControlSubTask(CPed* ped) {
    return plugin::CallMethodAndReturn<CTask*, 0x61B580, CTaskComplexPlayHandSignalAnim*, CPed*>(this, ped);
}
