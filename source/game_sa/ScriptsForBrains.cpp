#include "StdInc.h"

#include "ScriptsForBrains.h"

#include "Entity.h"     // CEntity::AsPed / AsObject
#include "Object.h"     // CObject (m_nStreamedScriptBrainToLoad, objectFlags)
#include "Ped.h"        // CPed (bHasAScriptBrain, bWaitingForScriptBrainToLoad, m_StreamedScriptBrainToLoad)
#include "Streaming.h"  // CStreaming::IsModelLoaded / RequestModel, SCMToModelId, STREAMING_MISSION_REQUIRED
#include "TheScripts.h" // CTheScripts::AddToWaitingForScriptBrainArray

void CScriptsForBrains::InjectHooks() {
    RH_ScopedClass(CScriptsForBrains);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Init, 0x46A8C0);
    //RH_ScopedInstall(SwitchAllObjectBrainsWithThisID, 0x46A900, {.Reversed = false});
    //RH_ScopedInstall(AddNewScriptBrain, 0x46A930, {.Reversed = false});
    //RH_ScopedInstall(AddNewStreamedScriptBrainForCodeUse, 0x46A9C0, {.Reversed = false});
    RH_ScopedInstall(GetIndexOfScriptBrainWithThisName, 0x46AA30);
    RH_ScopedInstall(HasAttractorScriptBrainWithThisNameLoaded, 0x46AB20);
    //RH_ScopedInstall(StartNewStreamedScriptBrain, 0x46B270, {.Reversed = false});
    RH_ScopedInstall(StartAttractorScriptBrainWithThisName, 0x46B390);
    RH_ScopedInstall(StartOrRequestNewStreamedScriptBrain, 0x46CD80);
    //RH_ScopedInstall(StartOrRequestNewStreamedScriptBrainWithThisName, 0x46CED0, {.Reversed = false});
    RH_ScopedInstall(IsObjectWithinBrainActivationRange, 0x46B3D0);
}


// 0x46A8C0
void CScriptsForBrains::Init() {
    for (auto& script : m_aScriptForBrains) {
        script = tScriptForBrains();
    }
}

void CScriptsForBrains::CheckIfNewEntityNeedsScript(CEntity* entity, int8 attachType, void* unused) {
    plugin::CallMethod<0x46FF20, CScriptsForBrains*, CEntity*, int8, void*>(this, entity, attachType, unused);
}

void CScriptsForBrains::MarkAttractorScriptBrainWithThisNameAsNoLongerNeeded(const char* name) {
    plugin::CallMethod<0x46AAE0, CScriptsForBrains*, const char*>(this, name);
}

void CScriptsForBrains::RequestAttractorScriptBrainWithThisName(const char* name) {
    plugin::CallMethod<0x46AA80, CScriptsForBrains*, const char*>(this, name);
}

void CScriptsForBrains::StartNewStreamedScriptBrain(uint8 index, CEntity* entity, bool bHasAScriptBrain) {
    plugin::CallMethodAndReturn<void, 0x46B270, CScriptsForBrains*, uint8, CEntity*, uint8>(this, index, entity, bHasAScriptBrain);
}

// 0x46CD80
void CScriptsForBrains::StartOrRequestNewStreamedScriptBrain(uint8 index, CEntity* entity, int8 attachType, bool bAddToWaitingArray) {
    const auto& brain = m_aScriptForBrains[index];

    if (bAddToWaitingArray) {
        if (!brain.m_bBrainActive) {
            return;
        }
        switch (attachType) {
        case 0:
        case 3: { // Peds
            const auto ped = entity->AsPed();
            if (ped->bHasAScriptBrain || ped->bWaitingForScriptBrainToLoad) {
                return;
            }
            break;
        }
        case 1:
        case 4: { // Objects
            const auto obj = entity->AsObject();
            if (obj->objectFlags.b0x100000_0x200000) {
                return;
            }
            break;
        }
        }
    }

    if (attachType == 1) { // Objects
        const auto obj = entity->AsObject();
        if (!bAddToWaitingArray) {
            obj->objectFlags.b0x100000_0x200000 = 0b10; // (m_nObjectFlags & 0xFFEFFFFF) | 0x200000
        } else {
            obj->m_nStreamedScriptBrainToLoad = index;
            obj->objectFlags.b0x100000_0x200000 = 0b01; // (m_nObjectFlags & 0xFFDFFFFF) | 0x100000
            CTheScripts::AddToWaitingForScriptBrainArray(entity, index);
        }
    }

    const auto modelId = SCMToModelId(brain.m_StreamedScriptIndex);
    if (!CStreaming::IsModelLoaded(modelId)) {
        CStreaming::RequestModel(modelId, STREAMING_MISSION_REQUIRED);
        if ((attachType == 0 || (attachType > 2 && attachType < 5)) && bAddToWaitingArray) { // Peds
            const auto ped = entity->AsPed();
            ped->m_StreamedScriptBrainToLoad = index;
            ped->bWaitingForScriptBrainToLoad = true;
            CTheScripts::AddToWaitingForScriptBrainArray(entity, index);
        }
        return;
    }

    StartNewStreamedScriptBrain(index, entity, false);
}

// 0x46B3D0
bool CScriptsForBrains::IsObjectWithinBrainActivationRange(CObject* entity, const CVector& point) {
    const auto& brain = m_aScriptForBrains[entity->m_nStreamedScriptBrainToLoad];
    if (brain.m_TypeOfBrain != 1) { // Radius-based activation only
        return false;
    }
    return DistanceBetweenPoints(entity->GetPosition(), point) < brain.m_ObjectBrainActivationRadius;
}

int16 CScriptsForBrains::GetIndexOfScriptBrainWithThisName(const char* name, int8 type) {
    const auto it = rng::find_if(m_aScriptForBrains, [=](tScriptForBrains& script) {
        return script.m_TypeOfBrain == type && !_stricmp(script.m_ScriptName, name);
    });
    return it != m_aScriptForBrains.end()
        ? rng::distance(m_aScriptForBrains.begin(), it)
        : -1;
}

void CScriptsForBrains::StartAttractorScriptBrainWithThisName(const char* name, CPed* ped, bool bHasAScriptBrain) {
    if (!ped->bWaitingForScriptBrainToLoad && !ped->bHasAScriptBrain) {
        if (const auto idx = GetIndexOfScriptBrainWithThisName(name, 5); idx >= 0) {
            StartNewStreamedScriptBrain(static_cast<uint8>(idx), ped, bHasAScriptBrain);
        }
    }
}

// 0x46A5F0 — redirect stub (not in wave-3 batch, preserved)
bool CScriptsForBrains::HasAttractorScriptBrainWithThisNameLoaded(const char* name) {
    return plugin::CallMethodAndReturn<bool, 0x46A5F0, CScriptsForBrains*, const char*>(this, name);
}
