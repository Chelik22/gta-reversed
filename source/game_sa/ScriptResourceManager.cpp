#include "StdInc.h"

#include "ScriptResourceManager.h"

void CScriptResourceManager::InjectHooks() {
    RH_ScopedClass(CScriptResourceManager);
    RH_ScopedCategory("Scripts");

    RH_ScopedInstall(Initialise, 0x470480);
    RH_ScopedInstall(AddToResourceManager, 0x4704B0);
    RH_ScopedInstall(RemoveFromResourceManager, 0x470510);
    RH_ScopedInstall(HasResourceBeenRequested, 0x470620);
    //RH_ScopedInstall(Load, 0x0);
    //RH_ScopedInstall(Save, 0x0);
}

// 0x470480
void CScriptResourceManager::Initialise() {
    m_aScriptResources.fill(tScriptResource());
}

// 1) Duplicate (modelId, type, thread) -> early out.
// 2) Otherwise find a free slot - the original checks `type == 0` (== RESOURCE_TYPE_DEFAULT,
//    exactly how Initialise()/RemoveFromResourceManager() clear records).
// 3) If found - fill in all three fields.
// 0x4704B0
void CScriptResourceManager::AddToResourceManager(int32 modelId, eScriptResourceType type, CRunningScript* script) {
    std::optional<size_t> freeSlotIdx;
    for (auto i = 0u; i < m_aScriptResources.size(); i++) {
        const auto& res = m_aScriptResources[i];
        if (res.m_nModelId == modelId && res.m_nType == type && res.m_pThread == script) {
            return; // Already in the manager
        }
        if (res.m_nType == RESOURCE_TYPE_DEFAULT) {
            freeSlotIdx = i;
        }
    }
    if (freeSlotIdx) {
        auto& res      = m_aScriptResources[*freeSlotIdx];
        res.m_nModelId = modelId;
        res.m_pThread  = script;
        res.m_nType    = type;
    }
}

// Among the records with (m_nModelId == modelId && m_nType == type):
// - thread matched too -> remember index (record to remove);
// - otherwise          -> count of "other" holders.
// The found record is cleared: m_nModelId = -1, m_nType = 0, m_pThread = nullptr (== tScriptResource{}).
// Returns true if no other holders of the resource are left.
// 0x470510
bool CScriptResourceManager::RemoveFromResourceManager(int32 modelId, eScriptResourceType type, CRunningScript* script) {
    std::optional<size_t> foundIdx;
    size_t                othersCount = 0;
    for (auto i = 0u; i < m_aScriptResources.size(); i++) {
        const auto& res = m_aScriptResources[i];
        if (res.m_nModelId == modelId && res.m_nType == type) {
            if (res.m_pThread == script) {
                foundIdx = i;
            } else {
                othersCount++;
            }
        }
    }
    if (foundIdx) {
        m_aScriptResources[*foundIdx] = tScriptResource{}; // modelId=-1, type=DEFAULT, thread=nullptr
    }
    return othersCount == 0;
}

// Only (m_nModelId, m_nType) are checked, m_pThread is not.
// 0x470620
bool CScriptResourceManager::HasResourceBeenRequested(int32 modelId, eScriptResourceType type) {
    return std::ranges::any_of(m_aScriptResources, [&](const tScriptResource& res) {
        return res.m_nModelId == modelId && res.m_nType == type;
    });
}

// 0x0
bool CScriptResourceManager::Load() {
    assert(false);
    return true;
}

// 0x0
bool CScriptResourceManager::Save() {
    assert(false);
    return true;
}
