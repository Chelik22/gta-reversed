#include "StdInc.h"
#include "TaskComplexPlayHandSignalAnim.h"

// Added for batch w2-15 (6 reversed functions). Already covered by StdInc.h:
// General.h, Streaming.h/StreamingInfo.h, Animation/AnimManager.h, Models/ModelInfo.h,
// Models/BaseModelInfo.h, Tasks/TaskComplexSequence.h, Entity/Ped/Ped.h (-> Enums/ePedType.h),
// Enums/{AnimationEnums,eModelID,eTaskType}.h
#include "TaskSimpleStandStill.h"         // CTaskSimpleStandStill (CreateSubTask)
#include "TaskSimplePlayHandSignalAnim.h" // CTaskSimplePlayHandSignalAnim (CreateSubTask)
#include "Ragdoll/IKChainManager.h"       // g_ikChainMan, eIKArm (CreateFirstSubTask)

void CTaskComplexPlayHandSignalAnim::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexPlayHandSignalAnim, 0x86d5dc, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x61B2B0);
    RH_ScopedInstall(Destructor, 0x61BDF0);

    RH_ScopedInstall(GetAnimIdForPed, 0x61B460);
    RH_ScopedInstall(CreateSubTask, 0x61B2F0);

    RH_ScopedVMTInstall(Clone, 0x61BA00);
    RH_ScopedVMTInstall(GetTaskType, 0x61B2E0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x61B570);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x61B4F0);
    RH_ScopedVMTInstall(ControlSubTask, 0x61B580);
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
    case PED_TYPE_GANG1: return ANIM_ID_GSIGN1; // 0x140 - Ballas
    case PED_TYPE_GANG2: return ANIM_ID_GSIGN2; // 0x141 - Grove Street Families
    case PED_TYPE_GANG3: return ANIM_ID_GSIGN3; // 0x142 - Los Santos Vagos
    case PED_TYPE_GANG5: return ANIM_ID_GSIGN5; // 0x144 - Da Nang Boys
    case PED_TYPE_GANG8: return ANIM_ID_UNDEFINED; // 0xE -> -1 (Varrio Los Aztecas)
    default:             return ANIM_ID_GSIGN4; // 0x143 - все остальные типы педов
    }
}

// 0x61B2F0
CTask* CTaskComplexPlayHandSignalAnim::CreateSubTask(eTaskType taskType) {
    switch (taskType) {
    case TASK_SIMPLE_STAND_STILL: { // 0xCB / 203
        // Looped stand-still, no idle-anim stance, 8.0 blend
        return new CTaskSimpleStandStill(0, true, false, 8.0f);
    }
    case TASK_SIMPLE_HANDSIGNAL_ANIM: { // 0x1A9 / 425
        const auto seq = new CTaskComplexSequence{};

        // First stand still for a random amount of time in [0, 1500)
        seq->AddTask(new CTaskSimpleStandStill(CGeneral::GetRandomNumberInRange(0, 1500), false, false, 8.0f));

        // Then play the hand signal anim (do not hold the last frame)
        seq->AddTask(new CTaskSimplePlayHandSignalAnim(m_animationId, m_AnimBlenDelta, m_DoUseFatHands, false));

        return seq;
    }
    default:
        return nullptr;
    }
}

// 0x61BA00
// NOTE: оригинал аллоцирует 0x18 байт, вызывает базовый конструктор 0x61A3B0 и копирует только
// m_animationId (+0xC) и m_AnimBlenDelta (+0x10); флаги остаются сброшенными.
// Копи-конструктор объявлен в хедере, но нигде не определён - поэтому клоним вручную.
CTask* CTaskComplexPlayHandSignalAnim::Clone() const {
    const auto clone = new CTaskComplexPlayHandSignalAnim{};
    clone->m_animationId   = m_animationId;
    clone->m_AnimBlenDelta = m_AnimBlenDelta;
    return clone;
}

// 0x61B570
CTask* CTaskComplexPlayHandSignalAnim::CreateNextSubTask(CPed* ped) {
    return nullptr; // оригинал: xor eax, eax; ret 4 (ped не используется)
}

// 0x61B4F0
CTask* CTaskComplexPlayHandSignalAnim::CreateFirstSubTask(CPed* ped) {
    // Оригинал: push 1 -> eIKArm(1) == IK_ARM_LEFT
    if (g_ikChainMan.IsArmPointing(eIKArm::IK_ARM_LEFT, ped)) {
        return nullptr;
    }

    if (m_animationId == ANIM_ID_UNDEFINED) {
        m_animationId = GetAnimIdForPed(ped);
        if (m_animationId == ANIM_ID_UNDEFINED) {
            return nullptr; // этот тип педа не умеет показывать знаки руками
        }
    }

    // Жирные руки есть только у этих скинов (ped->m_nModelIndex == word [ped+0x22])
    switch (ped->m_nModelIndex) {
    case MODEL_BALLAS2:
    case MODEL_FAM1:
    case MODEL_FAM3:
        m_DoUseFatHands = true;
        break;
    default:
        m_DoUseFatHands = false;
        break;
    }

    return CreateSubTask(TASK_SIMPLE_STAND_STILL); // 0xCB
}

// 0x61B580
// NOTE: ped не используется оригиналом. Проверки загрузки в оригинале сделаны напрямую по
// CStreaming::ms_aInfoForModel[model].m_LoadState == LOADSTATE_LOADED -- CStreaming::IsModelLoaded() делает ровно это.
CTask* CTaskComplexPlayHandSignalAnim::ControlSubTask(CPed* ped) {
    const auto leftHandModel  = m_DoUseFatHands ? MODEL_FHANDL : MODEL_SHANDL;
    const auto rightHandModel = m_DoUseFatHands ? MODEL_FHANDR : MODEL_SHANDR;

    // Левая рука: если модель загружена - держим на неё ссылку, иначе запрашиваем
    if (CStreaming::IsModelLoaded(leftHandModel)) {
        if (!m_bLeftHandLoaded) {
            CModelInfo::GetModelInfo(leftHandModel)->AddRef(); // оригинал: CModelInfo::ms_modelInfoPtrs[model]->AddRef() (0xA9B6F0/0xA9B6F8)
            m_bLeftHandLoaded = true;
        }
    } else {
        CStreaming::RequestModel(leftHandModel, STREAMING_KEEP_IN_MEMORY); // оригинал: push 8
    }

    // Правая рука
    if (CStreaming::IsModelLoaded(rightHandModel)) {
        if (!m_bRightHandLoaded) {
            CModelInfo::GetModelInfo(rightHandModel)->AddRef(); // 0xA9B6F4/0xA9B6FC
            m_bRightHandLoaded = true;
        }
    } else {
        CStreaming::RequestModel(rightHandModel, STREAMING_KEEP_IN_MEMORY);
    }

    if (m_bLeftHandLoaded && m_bRightHandLoaded) {
        if (ms_animBlock == -1) { // StaticRef @ 0x8D2E38
            ms_animBlock = CAnimManager::GetAnimationBlockIndex("ghands");
        }
        if (CAnimManager::GetAnimBlocks()[ms_animBlock].RefCnt == 1) {
            if (!m_bAnimationLoaded) {
                CAnimManager::AddAnimBlockRef(ms_animBlock); // 0x4D3FB0
                m_bAnimationLoaded = true;
            }
        } else {
            // Оригинал: RequestModel(ms_animBlock + 0x63E7, 8) == IFPToModelId
            CStreaming::RequestModel(IFPToModelId(ms_animBlock), STREAMING_KEEP_IN_MEMORY);
        }
    }

    // Анимация загружена и текущий сабтаск ещё стандстилл -> заменяем его на последовательность hand signal
    if (m_bAnimationLoaded && m_pSubTask->GetTaskType() == TASK_SIMPLE_STAND_STILL) {
        return CreateSubTask(TASK_SIMPLE_HANDSIGNAL_ANIM); // 0x1A9
    }
    return m_pSubTask;
}
