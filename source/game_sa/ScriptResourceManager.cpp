#include "StdInc.h"

#include "ScriptResourceManager.h"

void CScriptResourceManager::InjectHooks() {
    RH_ScopedClass(CScriptResourceManager);
    RH_ScopedCategory("Scripts");

    RH_ScopedInstall(Initialise, 0x470480);
    RH_ScopedInstall(AddToResourceManager, 0x4704B0);
    RH_ScopedInstall(RemoveFromResourceManager, 0x470510);
    RH_ScopedInstall(HasResourceBeenRequested, 0x470620);
    //RH_ScopedInstall(Load, 0x0, { .reversed = false });
    //RH_ScopedInstall(Save, 0x0, { .reversed = false });
}

// 0x470480
void CScriptResourceManager::Initialise() {
    m_aScriptResources.fill(tScriptResource());
}

// 0x4704B0
void CScriptResourceManager::AddToResourceManager(int32 modelId, eScriptResourceType type, CRunningScript* script) {
    int32 freeSlot = -1;

    for (auto i = 0u; i < m_aScriptResources.size(); i++) {
        const auto& res = m_aScriptResources[i];

        if (res.m_nModelId == modelId && res.m_nType == type && res.m_pThread == script)
            return; // Already added

        if (res.m_nType == RESOURCE_TYPE_DEFAULT) // Empty slot
            freeSlot = i;
    }

    if (freeSlot != -1) {
        auto& res      = m_aScriptResources[freeSlot];
        res.m_nModelId = modelId;
        res.m_nType    = type;
        res.m_pThread  = script;
    }
}

// 0x470510
bool CScriptResourceManager::RemoveFromResourceManager(int32 modelId, eScriptResourceType type, CRunningScript* script) {
    int32 foundSlot = -1;
    int32 others    = 0; // Number of other matching resources held by a different script

    for (auto i = 0u; i < m_aScriptResources.size(); i++) {
        const auto& res = m_aScriptResources[i];

        if (res.m_nModelId == modelId && res.m_nType == type) {
            if (res.m_pThread == script) {
                foundSlot = i;
            } else {
                others++;
            }
        }
    }

    if (foundSlot != -1) {
        auto& res      = m_aScriptResources[foundSlot];
        res.m_nModelId = -1;
        res.m_nType    = RESOURCE_TYPE_DEFAULT;
        res.m_pThread  = nullptr;
    }

    return others == 0; // True when no other script still holds this resource
}

// 0x470620
// NOTE: Written as a plain counter/pointer walk over the array (like the
// original - no std::array helper calls in the compiled code).
bool CScriptResourceManager::HasResourceBeenRequested(int32 modelId, eScriptResourceType type) {
    const auto* res  = reinterpret_cast<const tScriptResource*>(&m_aScriptResources);
    const auto  num  = sizeof(m_aScriptResources) / sizeof(tScriptResource);

    for (auto i = 0u; i < num; i++) {
        if (res->m_nModelId == modelId && res->m_nType == type)
            return true;
        ++res;
    }
    return false;
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
