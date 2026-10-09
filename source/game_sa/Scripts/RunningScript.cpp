#include "StdInc.h"

#include "RunningScript.h"
#include "TheScripts.h"
#include "CarGenerator.h"
#include "Hud.h"
#include "spdlog/sinks/stdout_color_sinks.h"

// NOTSA (batch runningscript-1): headers needed by the newly reversed command bodies.
// All paths verified to exist in the repo; every one also has `#pragma once`.
#include "Animation/AnimBlendAssociation.h"                        // eAnimationFlags (ANIMATION_*)
#include "Enums/AnimationEnums.h"                                  // AnimationId, AssocGroupId (ANIM_ID_NO_ANIMATION_SET, ANIM_GROUP_DEFAULT)
#include "Timer.h"                                                 // CTimer::ms_fTimeStep / ms_fOldTimeStep
#include "General.h"                                               // CGeneral::GetRadianAngleBetweenPoints
#include "ShotInfo.h"                                              // CShotInfo::GetFlameThrowerShotPosn
#include "Tasks/TaskManager.h"                                     // eSecondaryTask (TASK_SECONDARY_*), CTaskManager::SetTaskSecondary
#include "Tasks/TaskSequences.h"                                   // CTaskSequences::AddTaskToActiveSequence / ms_iActiveSequence
#include "Tasks/PedScriptedTaskRecord.h"                           // CPedScriptedTaskRecord::GetVacantSlot / ms_scriptedTasks
#include "Tasks/ScriptedBrainTaskStore.h"                          // CScriptedBrainTaskStore::SetTask
#include "Tasks/TaskTypes/TaskSimpleFinishBrain.h"                 // CTaskSimpleFinishBrain
#include "Tasks/TaskTypes/TaskSimpleHoldEntity.h"                  // CTaskSimpleHoldEntity
#include "Tasks/TaskTypes/TaskSimpleAffectSecondaryBehaviour.h"    // CTaskSimpleAffectSecondaryBehaviour
#include "Tasks/TaskTypes/TaskSimpleRunNamedAnim.h"                // CTaskSimpleRunNamedAnim

static notsa::log_ptr logger;

//static auto logger = NOTSA_MAKE_LOGGER("script");

//! Define it to dump out all commands that don't have a custom handler (that is, they aren't reversed)
//! Makes compilation slow, so don't enable unless necessary!
//#define DUMP_CUSTOM_COMMAND_HANDLERS_TO_FILE

#ifdef DUMP_CUSTOM_COMMAND_HANDLERS_TO_FILE
#include <fstream>
#endif

#include "CommandParser/Parser.hpp"
#include "CommandParser/LUTGenerator.hpp"
#include "reversiblehooks/ReversibleHook/ScriptCommandHook.h"

#include "Commands/Commands.hpp"
#ifdef NOTSA_WITH_CLEO_SCRIPT_COMMANDS // TODO: Add premake/cmake option for this define
#include "Commands/CLEO/Commands.hpp"
#include "Commands/CLEO/Extensions/Commands.hpp"
#endif

// https://library.sannybuilder.com/#/sa

//! Holds all custom command handlers (or null for commands with no custom handler)
static inline std::array<notsa::script::CommandHandlerFunction, (size_t)(COMMAND_HIGHEST_ID_TO_HOOK) + 1> s_CustomCommandHandlerTable{};

std::array<std::array<char, COMMANDS_CHAR_BUFFER_SIZE>, COMMANDS_CHAR_BUFFERS_COUNT> CRunningScript::ScriptArgCharBuffers        = {};
uint8                                                                                CRunningScript::ScriptArgCharNextFreeBuffer = 0;

void CRunningScript::InjectHooks() {
    logger = NOTSA_MAKE_LOGGER("script");

    InjectCustomCommandHooks();

    RH_ScopedClass(CRunningScript);
    RH_ScopedCategory("Scripts");

    RH_ScopedInstall(Init, 0x4648E0);
    RH_ScopedInstall(GetCorrectPedModelIndexForEmergencyServiceType, 0x464F50);

    RH_ScopedInstall(PlayAnimScriptCommand, 0x470150);
    RH_ScopedInstall(LocateCarCommand, 0x487A20);
    RH_ScopedInstall(LocateCharCommand, 0x486D80);
    RH_ScopedInstall(LocateObjectCommand, 0x487D10);
    RH_ScopedInstall(LocateCharCarCommand, 0x487420);
    RH_ScopedInstall(LocateCharCharCommand, 0x4870F0);
    RH_ScopedInstall(LocateCharObjectCommand, 0x487720);
    RH_ScopedInstall(CarInAreaCheckCommand, 0x488EC0);
    RH_ScopedInstall(CharInAreaCheckCommand, 0x488B50);
    RH_ScopedInstall(ObjectInAreaCheckCommand, 0x489150);
    RH_ScopedInstall(CharInAngledAreaCheckCommand, 0x487F60);
    RH_ScopedInstall(FlameInAngledAreaCheckCommand, 0x488780);
    RH_ScopedInstall(ObjectInAngledAreaCheckCommand, 0x4883F0);
    RH_ScopedInstall(CollectParameters, 0x464080, { .StackArgumentsToPreserve = 1, .PreserveRegisters = true });
    RH_ScopedInstall(CollectNextParameterWithoutIncreasingPC, 0x464250, { .StackArgumentsToPreserve = 0, .PreserveRegisters = true });
    RH_ScopedInstall(StoreParameters, 0x464370, { .StackArgumentsToPreserve = 1, .PreserveRegisters = true });
    RH_ScopedInstall(ReadArrayInformation, 0x463CF0, { .StackArgumentsToPreserve = 3, .PreserveRegisters = true });
    RH_ScopedInstall(ReadParametersForNewlyStartedScript, 0x464500, { .StackArgumentsToPreserve = 1, .PreserveRegisters = true });
    RH_ScopedInstall(ReadTextLabelFromScript, 0x463D50, { .StackArgumentsToPreserve = 2, .PreserveRegisters = true });
    RH_ScopedInstall(GetIndexOfGlobalVariable, 0x464700, { .StackArgumentsToPreserve = 0, .PreserveRegisters = true });
    RH_ScopedInstall(GetPadState, 0x485B10);
    RH_ScopedInstall(GetPointerToLocalVariable, 0x463CA0, { .StackArgumentsToPreserve = 1, .PreserveRegisters = true });
    RH_ScopedInstall(GetPointerToLocalArrayElement, 0x463CC0, { .StackArgumentsToPreserve = 3, .PreserveRegisters = true });
    RH_ScopedInstall(GetPointerToScriptVariable, 0x464790, { .StackArgumentsToPreserve = 1, .PreserveRegisters = true });
    RH_ScopedInstall(DoDeathArrestCheck, 0x485A50);
    RH_ScopedInstall(SetCharCoordinates, 0x464DC0);
    RH_ScopedInstall(AddScriptToList, 0x464C00, { .StackArgumentsToPreserve = 1, .PreserveRegisters = true });
    RH_ScopedInstall(RemoveScriptFromList, 0x464BD0, { .StackArgumentsToPreserve = 1, .PreserveRegisters = true });
    RH_ScopedInstall(ShutdownThisScript, 0x465AA0);
    RH_ScopedInstall(IsPedDead, 0x464D70);
    RH_ScopedInstall(ThisIsAValidRandomPed, 0x489490);
    RH_ScopedInstall(ScriptTaskPickUpObject, 0x46AF50);
    RH_ScopedInstall(UpdateCompareFlag, 0x4859D0, { .StackArgumentsToPreserve = 1, .PreserveRegisters = true });
    RH_ScopedInstall(UpdatePC, 0x464DA0, { .StackArgumentsToPreserve = 1, .PreserveRegisters = true });
    RH_ScopedInstall(ProcessOneCommand, 0x469EB0);
    RH_ScopedInstall(Process, 0x469F00);
    RH_ScopedOverloadedInstall(GivePedScriptedTask, "OG", 0x465C20, void(CRunningScript::*)(int32, CTask*, int32));
}

//! Register our custom script command handlers
void CRunningScript::InjectCustomCommandHooks() {
    // Uncommenting any call will prevent it from being hooked, so
    // feel free to do so when debugging (Just don't forget to undo the changes!)

    namespace c = ::notsa::script::commands;
    c::basic::RegisterHandlers();
    c::camera::RegisterHandlers();
    c::character::RegisterHandlers();
    c::clock::RegisterHandlers();
    c::comparasion::RegisterHandlers();
    c::game::RegisterHandlers();
    c::generic::RegisterHandlers();
    c::math::RegisterHandlers();
    c::mission::RegisterHandlers();
    c::object::RegisterHandlers();
    c::pad::RegisterHandlers();
    c::ped::RegisterHandlers();
    c::player::RegisterHandlers();
    c::script::RegisterHandlers();
    c::sequence::RegisterHandlers();
    c::text::RegisterHandlers();
    c::unused::RegisterHandlers();
    c::utility::RegisterHandlers();
    c::vehicle::RegisterHandlers();
    c::zone::RegisterHandlers();
    c::stat::RegisterHandlers();
    c::conversation::RegisterHandlers();

#ifdef NOTSA_WITH_CLEO_SCRIPT_COMMANDS
    cleo::audiostream::RegisterHandlers();
    cleo::character::RegisterHandlers();
    cleo::dynamiclibrary::RegisterHandlers();
    cleo::fs::RegisterHandlers();
    cleo::game::RegisterHandlers();
    cleo::generic::RegisterHandlers();
    cleo::memory::RegisterHandlers();
    cleo::pad::RegisterHandlers();
    cleo::script::RegisterHandlers();
    cleo::vehicle::RegisterHandlers();
    cleo::world::RegisterHandlers();

    cleo::extensions::cleoplus::RegisterHandlers();
    cleo::extensions::clipboard::RegisterHandlers();
    cleo::extensions::fs::RegisterHandlers();
    cleo::extensions::imgui::RegisterHandlers();
    cleo::extensions::intoperations::RegisterHandlers();
#endif

#ifdef DUMP_CUSTOM_COMMAND_HANDLERS_TO_FILE
    auto reversed{0}, total{0};
    std::ofstream ofsrev{ "reversed_script_command_handlers.txt" }, ofsnotrev{ "NOT_reversed_script_command_handlers.txt" };
    for (auto&& [idx, handler] : rngv::enumerate(s_CustomCommandHandlerTable)) {
        const auto id = (eScriptCommands)(idx);
        ++total;
        if (handler) ++reversed;
        (handler ? ofsrev : ofsnotrev) << ::notsa::script::GetScriptCommandName(id) << '\n';
    }
    NOTSA_LOG_DEBUG("Script cmds dumped! Find them in `<GTA Directory>/Scripts`!");
    NOTSA_LOG_DEBUG("Script cmds reverse progress: {}/{} ({:.2f}% done)", reversed, total, 100.0f * ((float)reversed / (float)total));
#endif
}

// 0x4648E0
void CRunningScript::Init() {
    SetName("noname");
    rng::fill(m_IPStack, nullptr);
    rng::fill(m_LocalVars, tScriptParam{ 0 });
    m_BaseIP                          = nullptr;
    m_pPrev                           = nullptr;
    m_pNext                           = nullptr;
    m_IP                              = nullptr;
    m_StackDepth                      = 0;
    m_WakeTime                        = 0;
    m_IsActive                        = false;
    m_CondResult                      = false;
    m_UsesMissionCleanup              = false;
    m_IsExternal                      = false;
    m_IsTextBlockOverride             = false;
    m_ExternalType                    = -1;
    m_AndOrState                      = 0;
    m_NotFlag                         = false;
    m_DoneDeathArrest                 = false;
    m_SceneSkipIP                     = 0;
    m_ThisMustBeTheOnlyMissionRunning = false;
    m_IsDeathArrestCheckEnabled       = true;
}

/*!
 * Adds script to list
 * @addr 0x464C00
 */
void CRunningScript::AddScriptToList(CRunningScript** queueList) {
    m_pNext = *queueList;
    m_pPrev = nullptr;
    if (*queueList)
        (*queueList)->m_pPrev = this;
    *queueList = this;
}

/*!
 * Removes script from list
 * @addr 0x464BD0
 */
void CRunningScript::RemoveScriptFromList(CRunningScript** queueList) {
    if (m_pPrev)
        m_pPrev->m_pNext = m_pNext;
    else
        *queueList = m_pNext;

    if (m_pNext)
        m_pNext->m_pPrev = m_pPrev;
}

/*!
 * Terminates a script
 * @addr 0x465AA0
 */
void CRunningScript::ShutdownThisScript() {
    m_IsActive = false;

    if (m_IsExternal) {
        const auto idx = CTheScripts::StreamedScripts.GetStreamedScriptWithThisStartAddress(m_BaseIP);
        CTheScripts::StreamedScripts.m_aScripts[idx].m_NumberOfUsers--;
    }

    const auto entityRef = m_ThisMustBeTheOnlyMissionRunning
        ? CTheScripts::LocalVariablesForCurrentMission.front().iParam
        : m_LocalVars[0].iParam;

    switch (m_ExternalType) {
    case 0:
    case 2:
    case 3:
    case 5: {
        if (const auto ped = GetPedPool()->GetAtRef(entityRef)) {
            ped->bHasAScriptBrain = false;
            if (m_ExternalType == 5) {
                CScriptedBrainTaskStore::SetTask(ped, new CTaskSimpleFinishBrain{});
            }
        }
        break;
    }
    case 1:
    case 4: {
        if (const auto object = GetObjectPool()->GetAtRef(entityRef)) {
            // Mark object as no longer being used by a brain script
            object->m_nObjectFlags = (object->m_nObjectFlags & ~0x200000u) | 0x100000u;
        }
        break;
    }
    }
}

// 0x465C20
void CRunningScript::GivePedScriptedTask(int32 pedHandle, CTask* task, int32 opcode) {
    if (pedHandle == -1) {
        CTaskSequences::AddTaskToActiveSequence(task);
        return;
    }

    CPed* ped = GetPedPool()->GetAtRef(pedHandle);
    assert(ped);
    CPedGroup* pedGroup = CPedGroups::GetPedsGroup(ped);

    CPed* otherPed = nullptr;
    if (m_ExternalType == 5 || m_ExternalType == 2 || !m_ExternalType || m_ExternalType == 3) {
        auto* localVariable = reinterpret_cast<int32*>(GetPointerToLocalVariable(0));
        otherPed = GetPedPool()->GetAtRef(*localVariable);
    }

    if (ped->bHasAScriptBrain && otherPed != ped) {
        delete task;
    } else if (otherPed && m_ExternalType == 5) {
        if (CScriptedBrainTaskStore::SetTask(ped, task)) {
            const int32 slot = CPedScriptedTaskRecord::GetVacantSlot();
            CPedScriptedTaskRecord::ms_scriptedTasks[slot].SetAsAttractorScriptTask(ped, opcode, task);
        }
    } else if (!pedGroup || ped->IsPlayer()) {
        CEventScriptCommand eventScriptCommand(TASK_PRIMARY_PRIMARY, task, false);
        auto* event = static_cast<CEventScriptCommand*>(ped->GetEventGroup().Add(&eventScriptCommand, false));
        if (event) {
            const int32 slot = CPedScriptedTaskRecord::GetVacantSlot();
            CPedScriptedTaskRecord::ms_scriptedTasks[slot].Set(ped, opcode, event);
        }
    } else {
        pedGroup->GetIntelligence().SetScriptCommandTask(ped, *task);
        CTask* scriptedTask = pedGroup->GetIntelligence().GetTaskScriptCommand(ped);
        const int32 slot = CPedScriptedTaskRecord::GetVacantSlot();
        CPedScriptedTaskRecord::ms_scriptedTasks[slot].SetAsGroupTask(ped, opcode, scriptedTask);
        delete task;
    }
}

void CRunningScript::GivePedScriptedTask(CPed* ped, CTask* task, int32 opcode) {
    GivePedScriptedTask(GetPedPool()->GetRef(ped), task, opcode); // Must do it like this, otherwise unhooking of the original `GivePedScriptedTask` will do nothing
}

// 0x470150
// Handles COMMAND_TASK_PLAY_ANIM (0x0605), COMMAND_TASK_PLAY_ANIM_NON_INTERRUPTABLE (0x0812),
// COMMAND_TASK_PLAY_ANIM_WITH_FLAGS (0x088A) and COMMAND_TASK_PLAY_ANIM_SECONDARY (0x0A1A).
void CRunningScript::PlayAnimScriptCommand(int32 commandId) {
    CollectParameters(1);
    const auto pedHandle = ScriptParams[0].iParam; // saved: 2nd CollectParameters overwrites ScriptParams[0]

    char animName[24];
    char ifpName[16];
    ReadTextLabelFromScript(animName, 24);
    ReadTextLabelFromScript(ifpName, 16);

    auto bDontInterrupt = false;
    auto bOffsetPed     = false;
    switch (commandId) {
    case COMMAND_TASK_PLAY_ANIM_WITH_FLAGS:
        CollectParameters(8);
        bDontInterrupt = ScriptParams[6].iParam == 0;
        bOffsetPed     = ScriptParams[7].iParam != 0;
        break;
    case COMMAND_TASK_PLAY_ANIM_NON_INTERRUPTABLE:
        bDontInterrupt = true;
        [[fallthrough]];
    case COMMAND_TASK_PLAY_ANIM:
    case COMMAND_TASK_PLAY_ANIM_SECONDARY:
        CollectParameters(6);
        break;
    }

    uint32 animFlags = ANIMATION_IS_PARTIAL; // 0x10
    if (ScriptParams[1].iParam != 0 || (ScriptParams[5].iParam > 0 && ScriptParams[4].iParam == 0)) {
        animFlags = ANIMATION_IS_PARTIAL | ANIMATION_IS_LOOPED; // 0x12
    }
    if (ScriptParams[2].iParam != 0) {
        animFlags |= ANIMATION_CAN_EXTRACT_VELOCITY; // 0x40
    }
    if (ScriptParams[3].iParam != 0) {
        animFlags |= ANIMATION_CAN_EXTRACT_X_VELOCITY; // 0x80
    }
    if (ScriptParams[4].iParam == 0) {
        animFlags |= ANIMATION_IS_FINISH_AUTO_REMOVE; // 0x08
    }
    if (commandId == COMMAND_TASK_PLAY_ANIM_SECONDARY) {
        animFlags |= ANIMATION_DONT_ADD_TO_PARTIAL_BLEND; // 0x400
    }

    const auto bRunInSequence = CTaskSequences::ms_iActiveSequence > -1;

    CTask* task = new CTaskSimpleRunNamedAnim{
        animName,
        ifpName,
        animFlags,
        ScriptParams[0].fParam, // blend delta (param 1 of the 2nd CollectParameters)
        (uint32)ScriptParams[5].iParam, // end time
        bDontInterrupt,
        bRunInSequence,
        bOffsetPed,
        false
    };
    if (commandId == COMMAND_TASK_PLAY_ANIM_SECONDARY) {
        task = new CTaskSimpleAffectSecondaryBehaviour{ true, TASK_SECONDARY_PARTIAL_ANIM, task };
    }

    GivePedScriptedTask(pedHandle, task, commandId);
}

// 0x487A20
// Handles COMMAND_LOCATE_CAR_2D (0x01AD), COMMAND_LOCATE_STOPPED_CAR_2D (0x01AE),
// COMMAND_LOCATE_CAR_3D (0x01AF), COMMAND_LOCATE_STOPPED_CAR_3D (0x01B0).
void CRunningScript::LocateCarCommand(int32 commandId) {
    const auto is3D = commandId == COMMAND_LOCATE_CAR_3D || commandId == COMMAND_LOCATE_STOPPED_CAR_3D;
    CollectParameters(is3D ? 8 : 6);

    const auto veh = GetVehiclePool()->GetAtRef(ScriptParams[0].iParam);

    auto result = false;
    auto check  = true;
    switch (commandId) {
    case COMMAND_LOCATE_STOPPED_CAR_2D:
    case COMMAND_LOCATE_STOPPED_CAR_3D:
        // NOTE: inlined `CTheScripts::IsVehicleStopped` (0x4861F0) in the original.
        // Disassembly of both 0x4861F0 and this site: limit = min(ms_fTimeStep, ms_fOldTimeStep) * 0.01f (0x858C58),
        // i.e. `min`, not `max` as in the current repo's IsVehicleStopped (see STATUS.md).
        if (veh->m_fMovingSpeed > std::min(CTimer::ms_fTimeStep, CTimer::ms_fOldTimeStep) * 0.01f) {
            result = false;
            check  = false;
        }
        break;
    }

    const auto x         = ScriptParams[1].fParam;
    const auto y         = ScriptParams[2].fParam;
    const auto z         = is3D ? ScriptParams[3].fParam : 0.0f;
    const auto radiusX   = is3D ? ScriptParams[4].fParam : ScriptParams[3].fParam;
    const auto radiusY   = is3D ? ScriptParams[5].fParam : ScriptParams[4].fParam;
    const auto radiusZ   = is3D ? ScriptParams[6].fParam : 0.0f;
    const auto highlight = is3D ? ScriptParams[7].fParam : ScriptParams[5].fParam;

    if (check) {
        const CVector posn = veh->GetPosition();

        auto inArea = posn.x >= x - radiusX && posn.x <= x + radiusX
                   && posn.y >= y - radiusY && posn.y <= y + radiusY;
        if (is3D) {
            inArea = inArea && posn.z >= z - radiusZ && posn.z <= z + radiusZ;
        }
        result = inArea;
    }

    UpdateCompareFlag(result);

    if (highlight != 0.0f) {
        HighlightImportantArea(CVector2D{ x - radiusX, y - radiusY }, CVector2D{ x + radiusX, y + radiusY }, is3D ? z : -100.0f);
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugSquare(x - radiusX, y - radiusY, x + radiusX, y + radiusY);
    }
}

// 0x486D80
// Handles COMMAND_LOCATE_CHAR_ANY_MEANS/_ON_FOOT/_IN_CAR_2D (0x00EC-0x00EE),
// COMMAND_LOCATE_STOPPED_CHAR_ANY_MEANS/_ON_FOOT/_IN_CAR_2D (0x00EF-0x00F1)
// and their 3D versions (0x00FE-0x0103).
void CRunningScript::LocateCharCommand(int32 commandId) {
    const auto is3D = commandId >= COMMAND_LOCATE_CHAR_ANY_MEANS_3D && commandId <= COMMAND_LOCATE_STOPPED_CHAR_IN_CAR_3D;
    CollectParameters(is3D ? 8 : 6);

    const auto ped = GetPedPool()->GetAtRef(ScriptParams[0].iParam);

    auto result = false;
    auto check  = true;
    switch (commandId) {
    case COMMAND_LOCATE_STOPPED_CHAR_ANY_MEANS_2D:
    case COMMAND_LOCATE_STOPPED_CHAR_ON_FOOT_2D:
    case COMMAND_LOCATE_STOPPED_CHAR_IN_CAR_2D:
    case COMMAND_LOCATE_STOPPED_CHAR_ANY_MEANS_3D:
    case COMMAND_LOCATE_STOPPED_CHAR_ON_FOOT_3D:
    case COMMAND_LOCATE_STOPPED_CHAR_IN_CAR_3D:
        if (!CTheScripts::IsPedStopped(ped)) {
            result = false;
            check  = false;
        }
        break;
    }

    const auto x         = ScriptParams[1].fParam;
    const auto y         = ScriptParams[2].fParam;
    const auto z         = is3D ? ScriptParams[3].fParam : 0.0f;
    const auto radiusX   = is3D ? ScriptParams[4].fParam : ScriptParams[3].fParam;
    const auto radiusY   = is3D ? ScriptParams[5].fParam : ScriptParams[4].fParam;
    const auto radiusZ   = is3D ? ScriptParams[6].fParam : 0.0f;
    const auto highlight = is3D ? ScriptParams[7].fParam : ScriptParams[5].fParam;

    if (check) {
        const CVector posn = ped->GetRealPosition(); // in-vehicle -> vehicle posn (CPed::GetRealPosition)

        auto inArea = posn.x >= x - radiusX && posn.x <= x + radiusX
                   && posn.y >= y - radiusY && posn.y <= y + radiusY;
        if (is3D) {
            inArea = inArea && posn.z >= z - radiusZ && posn.z <= z + radiusZ;
        }

        if (inArea) {
            switch (commandId) {
            case COMMAND_LOCATE_CHAR_ANY_MEANS_2D:
            case COMMAND_LOCATE_STOPPED_CHAR_ANY_MEANS_2D:
            case COMMAND_LOCATE_CHAR_ANY_MEANS_3D:
            case COMMAND_LOCATE_STOPPED_CHAR_ANY_MEANS_3D:
                result = true;
                break;
            case COMMAND_LOCATE_CHAR_ON_FOOT_2D:
            case COMMAND_LOCATE_STOPPED_CHAR_ON_FOOT_2D:
            case COMMAND_LOCATE_CHAR_ON_FOOT_3D:
            case COMMAND_LOCATE_STOPPED_CHAR_ON_FOOT_3D:
                result = !ped->bInVehicle;
                break;
            case COMMAND_LOCATE_CHAR_IN_CAR_2D:
            case COMMAND_LOCATE_STOPPED_CHAR_IN_CAR_2D:
            case COMMAND_LOCATE_CHAR_IN_CAR_3D:
            case COMMAND_LOCATE_STOPPED_CHAR_IN_CAR_3D:
                result = ped->bInVehicle;
                break;
            }
        }
    }

    UpdateCompareFlag(result);

    if (highlight != 0.0f) {
        HighlightImportantArea(CVector2D{ x - radiusX, y - radiusY }, CVector2D{ x + radiusX, y + radiusY }, is3D ? z : -100.0f);
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugSquare(x - radiusX, y - radiusY, x + radiusX, y + radiusY);
    }
}

// 0x487D10
// Handles COMMAND_LOCATE_OBJECT_2D (0x04E5) and COMMAND_LOCATE_OBJECT_3D (0x04E6).
void CRunningScript::LocateObjectCommand(int32 commandId) {
    const auto is3D = commandId == COMMAND_LOCATE_OBJECT_3D;
    CollectParameters(is3D ? 8 : 6);

    const auto object = GetObjectPool()->GetAtRef(ScriptParams[0].iParam);

    const auto x         = ScriptParams[1].fParam;
    const auto y         = ScriptParams[2].fParam;
    const auto z         = is3D ? ScriptParams[3].fParam : 0.0f;
    const auto radiusX   = is3D ? ScriptParams[4].fParam : ScriptParams[3].fParam;
    const auto radiusY   = is3D ? ScriptParams[5].fParam : ScriptParams[4].fParam;
    const auto radiusZ   = is3D ? ScriptParams[6].fParam : 0.0f;
    const auto highlight = is3D ? ScriptParams[7].fParam : ScriptParams[5].fParam;

    const CVector posn = object->GetPosition();

    auto result = posn.x >= x - radiusX && posn.x <= x + radiusX
               && posn.y >= y - radiusY && posn.y <= y + radiusY;
    if (is3D) {
        result = result && posn.z >= z - radiusZ && posn.z <= z + radiusZ;
    }

    UpdateCompareFlag(result);

    if (highlight != 0.0f) {
        HighlightImportantArea(CVector2D{ x - radiusX, y - radiusY }, CVector2D{ x + radiusX, y + radiusY }, is3D ? z : -100.0f);
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugSquare(x - radiusX, y - radiusY, x + radiusX, y + radiusY);
    }
}

// 0x487420
// Handles COMMAND_LOCATE_CHAR_ANY_MEANS/_ON_FOOT/_IN_CAR_CAR_2D (0x0202-0x0204)
// and their 3D versions (0x0205-0x0207).
void CRunningScript::LocateCharCarCommand(int32 commandId) {
    const auto is3D = commandId >= COMMAND_LOCATE_CHAR_ANY_MEANS_CAR_3D && commandId <= COMMAND_LOCATE_CHAR_IN_CAR_CAR_3D;
    CollectParameters(is3D ? 6 : 5);

    const auto ped = GetPedPool()->GetAtRef(ScriptParams[0].iParam);
    const auto veh = GetVehiclePool()->GetAtRef(ScriptParams[1].iParam);

    const auto radiusX   = ScriptParams[2].fParam;
    const auto radiusY   = ScriptParams[3].fParam;
    const auto radiusZ   = is3D ? ScriptParams[4].fParam : 0.0f; // single z radius around the car
    const auto highlight = is3D ? ScriptParams[5].fParam : ScriptParams[4].fParam;

    const CVector pedPosn = ped->GetRealPosition(); // in-vehicle -> vehicle posn (CPed::GetRealPosition)
    const CVector vehPosn = veh->GetPosition();

    auto result = false;
    auto inArea = pedPosn.x >= vehPosn.x - radiusX && pedPosn.x <= vehPosn.x + radiusX
               && pedPosn.y >= vehPosn.y - radiusY && pedPosn.y <= vehPosn.y + radiusY;
    if (is3D) {
        inArea = inArea && pedPosn.z >= vehPosn.z - radiusZ && pedPosn.z <= vehPosn.z + radiusZ;
    }

    if (inArea) {
        switch (commandId) {
        case COMMAND_LOCATE_CHAR_ANY_MEANS_CAR_2D:
        case COMMAND_LOCATE_CHAR_ANY_MEANS_CAR_3D:
            result = true;
            break;
        case COMMAND_LOCATE_CHAR_ON_FOOT_CAR_2D:
        case COMMAND_LOCATE_CHAR_ON_FOOT_CAR_3D:
            result = !ped->bInVehicle;
            break;
        case COMMAND_LOCATE_CHAR_IN_CAR_CAR_2D:
        case COMMAND_LOCATE_CHAR_IN_CAR_CAR_3D:
            result = ped->bInVehicle;
            break;
        }
    }

    UpdateCompareFlag(result);

    if (highlight != 0.0f) {
        HighlightImportantArea(CVector2D{ vehPosn.x - radiusX, vehPosn.y - radiusY }, CVector2D{ vehPosn.x + radiusX, vehPosn.y + radiusY }, is3D ? vehPosn.z : -100.0f);
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugSquare(vehPosn.x - radiusX, vehPosn.y - radiusY, vehPosn.x + radiusX, vehPosn.y + radiusY);
    }
}

// 0x4870F0
// Handles COMMAND_LOCATE_CHAR_ANY_MEANS/_ON_FOOT/_IN_CAR_CHAR_2D (0x00F2-0x00F4)
// and their 3D versions (0x0104-0x0106).
void CRunningScript::LocateCharCharCommand(int32 commandId) {
    const auto is3D = commandId >= COMMAND_LOCATE_CHAR_ANY_MEANS_CHAR_3D && commandId <= COMMAND_LOCATE_CHAR_IN_CAR_CHAR_3D;
    CollectParameters(is3D ? 6 : 5);

    const auto ped1 = GetPedPool()->GetAtRef(ScriptParams[0].iParam);
    const auto ped2 = GetPedPool()->GetAtRef(ScriptParams[1].iParam);

    const auto radiusX   = ScriptParams[2].fParam;
    const auto radiusY   = ScriptParams[3].fParam;
    const auto radiusZ   = is3D ? ScriptParams[4].fParam : 0.0f; // single z radius around ped2
    const auto highlight = is3D ? ScriptParams[5].fParam : ScriptParams[4].fParam;

    const CVector pos1 = ped1->GetRealPosition(); // in-vehicle -> vehicle posn (CPed::GetRealPosition)
    const CVector pos2 = ped2->GetPosition();

    auto result = false;
    auto inArea = pos1.x >= pos2.x - radiusX && pos1.x <= pos2.x + radiusX
               && pos1.y >= pos2.y - radiusY && pos1.y <= pos2.y + radiusY;
    if (is3D) {
        inArea = inArea && pos1.z >= pos2.z - radiusZ && pos1.z <= pos2.z + radiusZ;
    }

    if (inArea) {
        switch (commandId) {
        case COMMAND_LOCATE_CHAR_ANY_MEANS_CHAR_2D:
        case COMMAND_LOCATE_CHAR_ANY_MEANS_CHAR_3D:
            result = true;
            break;
        case COMMAND_LOCATE_CHAR_ON_FOOT_CHAR_2D:
        case COMMAND_LOCATE_CHAR_ON_FOOT_CHAR_3D:
            result = !ped1->bInVehicle;
            break;
        case COMMAND_LOCATE_CHAR_IN_CAR_CHAR_2D:
        case COMMAND_LOCATE_CHAR_IN_CAR_CHAR_3D:
            result = ped1->bInVehicle;
            break;
        }
    }

    UpdateCompareFlag(result);

    if (highlight != 0.0f) {
        HighlightImportantArea(CVector2D{ pos2.x - radiusX, pos2.y - radiusY }, CVector2D{ pos2.x + radiusX, pos2.y + radiusY }, is3D ? pos2.z : -100.0f);
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugSquare(pos2.x - radiusX, pos2.y - radiusY, pos2.x + radiusX, pos2.y + radiusY);
    }
}

// 0x487720
// Handles COMMAND_LOCATE_CHAR_ANY_MEANS/_ON_FOOT/_IN_CAR_OBJECT_2D (0x0471-0x0473)
// and their 3D versions (0x0474-0x0476).
void CRunningScript::LocateCharObjectCommand(int32 commandId) {
    const auto is3D = commandId >= COMMAND_LOCATE_CHAR_ANY_MEANS_OBJECT_3D && commandId <= COMMAND_LOCATE_CHAR_IN_CAR_OBJECT_3D;
    CollectParameters(is3D ? 6 : 5);

    const auto ped    = GetPedPool()->GetAtRef(ScriptParams[0].iParam);
    const auto object = GetObjectPool()->GetAtRef(ScriptParams[1].iParam);

    const auto radiusX   = ScriptParams[2].fParam;
    const auto radiusY   = ScriptParams[3].fParam;
    const auto radiusZ   = is3D ? ScriptParams[4].fParam : 0.0f; // single z radius around the object
    const auto highlight = is3D ? ScriptParams[5].fParam : ScriptParams[4].fParam;

    const CVector pedPosn    = ped->GetRealPosition(); // in-vehicle -> vehicle posn (CPed::GetRealPosition)
    const CVector objectPosn = object->GetPosition();

    auto result = false;
    auto inArea = pedPosn.x >= objectPosn.x - radiusX && pedPosn.x <= objectPosn.x + radiusX
               && pedPosn.y >= objectPosn.y - radiusY && pedPosn.y <= objectPosn.y + radiusY;
    if (is3D) {
        inArea = inArea && pedPosn.z >= objectPosn.z - radiusZ && pedPosn.z <= objectPosn.z + radiusZ;
    }

    if (inArea) {
        switch (commandId) {
        case COMMAND_LOCATE_CHAR_ANY_MEANS_OBJECT_2D:
        case COMMAND_LOCATE_CHAR_ANY_MEANS_OBJECT_3D:
            result = true;
            break;
        case COMMAND_LOCATE_CHAR_ON_FOOT_OBJECT_2D:
        case COMMAND_LOCATE_CHAR_ON_FOOT_OBJECT_3D:
            result = !ped->bInVehicle;
            break;
        case COMMAND_LOCATE_CHAR_IN_CAR_OBJECT_2D:
        case COMMAND_LOCATE_CHAR_IN_CAR_OBJECT_3D:
            result = ped->bInVehicle;
            break;
        }
    }

    UpdateCompareFlag(result);

    if (highlight != 0.0f) {
        HighlightImportantArea(CVector2D{ objectPosn.x - radiusX, objectPosn.y - radiusY }, CVector2D{ objectPosn.x + radiusX, objectPosn.y + radiusY }, is3D ? objectPosn.z : -100.0f);
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugSquare(objectPosn.x - radiusX, objectPosn.y - radiusY, objectPosn.x + radiusX, objectPosn.y + radiusY);
    }
}

// 0x488EC0 - Not used in our code
// Handles COMMAND_IS_CAR_IN_AREA_2D (0x00B0) / _3D (0x00B1),
// COMMAND_IS_CAR_STOPPED_IN_AREA_2D (0x01AB) / _3D (0x01AC).
void CRunningScript::CarInAreaCheckCommand(int32 commandId) {
    const auto is3D = commandId == COMMAND_IS_CAR_IN_AREA_3D || commandId == COMMAND_IS_CAR_STOPPED_IN_AREA_3D;
    CollectParameters(is3D ? 8 : 6);

    const auto veh = GetVehiclePool()->GetAtRef(ScriptParams[0].iParam);

    auto result = false;
    auto check  = true;
    switch (commandId) {
    case COMMAND_IS_CAR_STOPPED_IN_AREA_2D:
    case COMMAND_IS_CAR_STOPPED_IN_AREA_3D:
        if (!CTheScripts::IsVehicleStopped(veh)) {
            result = false;
            check  = false;
        }
        break;
    }

    const auto x1 = ScriptParams[1].fParam;
    const auto y1 = ScriptParams[2].fParam;
    const auto x2 = is3D ? ScriptParams[4].fParam : ScriptParams[3].fParam;
    const auto y2 = is3D ? ScriptParams[5].fParam : ScriptParams[4].fParam;
    const float zmin    = is3D ? std::min(ScriptParams[3].fParam, ScriptParams[6].fParam) : 0.0f;
    const float zmax    = is3D ? std::max(ScriptParams[3].fParam, ScriptParams[6].fParam) : 0.0f;
    const auto  highlight = is3D ? ScriptParams[7].fParam : ScriptParams[5].fParam;
    const float xmin = std::min(x1, x2), xmax = std::max(x1, x2);
    const float ymin = std::min(y1, y2), ymax = std::max(y1, y2);

    if (check) {
        const CVector posn = veh->GetPosition();

        result = posn.x >= xmin && posn.x <= xmax
              && posn.y >= ymin && posn.y <= ymax;
        if (is3D) {
            result = result && posn.z >= zmin && posn.z <= zmax;
        }
    }

    UpdateCompareFlag(result);

    if (highlight != 0.0f) {
        HighlightImportantArea(CVector2D{ xmin, ymin }, CVector2D{ xmax, ymax }, is3D ? (zmax + zmin) * 0.5f : -100.0f); // 0x858B8C == 0.5f
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugSquare(xmin, ymin, xmax, ymax);
    }
}

// 0x488B50
// Handles COMMAND_IS_CHAR_IN_AREA_2D (0x00A3) / _3D (0x00A4),
// COMMAND_IS_CHAR_IN_AREA_ON_FOOT/_IN_CAR_2D (0x01A1-0x01A2), COMMAND_IS_CHAR_STOPPED_IN_AREA{,_ON_FOOT,_IN_CAR}_2D (0x01A3-0x01A5)
// and the 3D versions (0x01A6-0x01AA).
void CRunningScript::CharInAreaCheckCommand(int32 commandId) {
    const auto is3D = commandId == COMMAND_IS_CHAR_IN_AREA_3D
                   || (commandId >= COMMAND_IS_CHAR_IN_AREA_ON_FOOT_3D && commandId <= COMMAND_IS_CHAR_STOPPED_IN_AREA_IN_CAR_3D);
    CollectParameters(is3D ? 8 : 6);

    const auto ped = GetPedPool()->GetAtRef(ScriptParams[0].iParam);

    auto result = false;
    auto check  = true;
    switch (commandId) {
    case COMMAND_IS_CHAR_STOPPED_IN_AREA_2D:
    case COMMAND_IS_CHAR_STOPPED_IN_AREA_ON_FOOT_2D:
    case COMMAND_IS_CHAR_STOPPED_IN_AREA_IN_CAR_2D:
    case COMMAND_IS_CHAR_STOPPED_IN_AREA_3D:
    case COMMAND_IS_CHAR_STOPPED_IN_AREA_ON_FOOT_3D:
    case COMMAND_IS_CHAR_STOPPED_IN_AREA_IN_CAR_3D:
        if (!CTheScripts::IsPedStopped(ped)) {
            result = false;
            check  = false;
        }
        break;
    }

    const auto x1 = ScriptParams[1].fParam;
    const auto y1 = ScriptParams[2].fParam;
    const auto x2 = is3D ? ScriptParams[4].fParam : ScriptParams[3].fParam;
    const auto y2 = is3D ? ScriptParams[5].fParam : ScriptParams[4].fParam;
    const float zmin    = is3D ? std::min(ScriptParams[3].fParam, ScriptParams[6].fParam) : 0.0f;
    const float zmax    = is3D ? std::max(ScriptParams[3].fParam, ScriptParams[6].fParam) : 0.0f;
    const auto  highlight = is3D ? ScriptParams[7].fParam : ScriptParams[5].fParam;
    const float xmin = std::min(x1, x2), xmax = std::max(x1, x2);
    const float ymin = std::min(y1, y2), ymax = std::max(y1, y2);

    if (check) {
        const CVector posn = ped->GetRealPosition(); // in-vehicle -> vehicle posn (CPed::GetRealPosition)

        auto inArea = posn.x >= xmin && posn.x <= xmax
                   && posn.y >= ymin && posn.y <= ymax;
        if (is3D) {
            inArea = inArea && posn.z >= zmin && posn.z <= zmax;
        }

        if (inArea) {
            switch (commandId) {
            case COMMAND_IS_CHAR_IN_AREA_ON_FOOT_2D:
            case COMMAND_IS_CHAR_STOPPED_IN_AREA_ON_FOOT_2D:
            case COMMAND_IS_CHAR_IN_AREA_ON_FOOT_3D:
            case COMMAND_IS_CHAR_STOPPED_IN_AREA_ON_FOOT_3D:
                result = !ped->bInVehicle;
                break;
            case COMMAND_IS_CHAR_IN_AREA_IN_CAR_2D:
            case COMMAND_IS_CHAR_STOPPED_IN_AREA_IN_CAR_2D:
            case COMMAND_IS_CHAR_IN_AREA_IN_CAR_3D:
            case COMMAND_IS_CHAR_STOPPED_IN_AREA_IN_CAR_3D:
                result = ped->bInVehicle;
                break;
            case COMMAND_IS_CHAR_IN_AREA_2D:
            case COMMAND_IS_CHAR_STOPPED_IN_AREA_2D:
            case COMMAND_IS_CHAR_IN_AREA_3D:
            case COMMAND_IS_CHAR_STOPPED_IN_AREA_3D:
                result = true;
                break;
            }
        }
    }

    UpdateCompareFlag(result);

    if (highlight != 0.0f) {
        HighlightImportantArea(CVector2D{ xmin, ymin }, CVector2D{ xmax, ymax }, is3D ? (zmax + zmin) * 0.5f : -100.0f); // 0x858B8C == 0.5f
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugSquare(xmin, ymin, xmax, ymax);
    }
}

// 0x489150
// Handles COMMAND_IS_OBJECT_IN_AREA_2D (0x04E9) and COMMAND_IS_OBJECT_IN_AREA_3D (0x04EA).
void CRunningScript::ObjectInAreaCheckCommand(int32 commandId) {
    const auto is3D = commandId == COMMAND_IS_OBJECT_IN_AREA_3D;
    CollectParameters(is3D ? 8 : 6);

    const auto object = GetObjectPool()->GetAtRef(ScriptParams[0].iParam);

    const auto x1 = ScriptParams[1].fParam;
    const auto y1 = ScriptParams[2].fParam;
    const auto x2 = is3D ? ScriptParams[4].fParam : ScriptParams[3].fParam;
    const auto y2 = is3D ? ScriptParams[5].fParam : ScriptParams[4].fParam;
    const float zmin    = is3D ? std::min(ScriptParams[3].fParam, ScriptParams[6].fParam) : 0.0f;
    const float zmax    = is3D ? std::max(ScriptParams[3].fParam, ScriptParams[6].fParam) : 0.0f;
    const auto  highlight = is3D ? ScriptParams[7].fParam : ScriptParams[5].fParam;
    const float xmin = std::min(x1, x2), xmax = std::max(x1, x2);
    const float ymin = std::min(y1, y2), ymax = std::max(y1, y2);

    const CVector posn = object->GetPosition();

    auto result = posn.x >= xmin && posn.x <= xmax
               && posn.y >= ymin && posn.y <= ymax;
    if (is3D) {
        result = result && posn.z >= zmin && posn.z <= zmax;
    }

    UpdateCompareFlag(result);

    if (highlight != 0.0f) {
        HighlightImportantArea(CVector2D{ xmin, ymin }, CVector2D{ xmax, ymax }, is3D ? (zmax + zmin) * 0.5f : -100.0f); // 0x858B8C == 0.5f
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugSquare(xmin, ymin, xmax, ymax);
    }
}

// 0x487F60
// Handles COMMAND_IS_CHAR_IN_ANGLED_AREA_{2D,ON_FOOT_2D,IN_CAR_2D} (0x05F6-0x05F8),
// COMMAND_IS_CHAR_STOPPED_IN_ANGLED_AREA_{2D,ON_FOOT_2D,IN_CAR_2D} (0x05F9-0x05FB)
// and their 3D versions (0x05FC-0x0601).
void CRunningScript::CharInAngledAreaCheckCommand(int32 commandId) {
    const auto is3D = commandId >= COMMAND_IS_CHAR_IN_ANGLED_AREA_3D && commandId <= COMMAND_IS_CHAR_STOPPED_IN_ANGLED_AREA_IN_CAR_3D;
    CollectParameters(is3D ? 9 : 7);

    const auto ped = GetPedPool()->GetAtRef(ScriptParams[0].iParam);

    auto result = false;
    auto check  = true;
    switch (commandId) {
    case COMMAND_IS_CHAR_STOPPED_IN_ANGLED_AREA_2D:
    case COMMAND_IS_CHAR_STOPPED_IN_ANGLED_AREA_ON_FOOT_2D:
    case COMMAND_IS_CHAR_STOPPED_IN_ANGLED_AREA_IN_CAR_2D:
    case COMMAND_IS_CHAR_STOPPED_IN_ANGLED_AREA_3D:
    case COMMAND_IS_CHAR_STOPPED_IN_ANGLED_AREA_ON_FOOT_3D:
    case COMMAND_IS_CHAR_STOPPED_IN_ANGLED_AREA_IN_CAR_3D:
        if (!CTheScripts::IsPedStopped(ped)) {
            result = false;
            check  = false;
        }
        break;
    }

    const auto x1 = ScriptParams[1].fParam;
    const auto y1 = ScriptParams[2].fParam;
    const auto x2 = is3D ? ScriptParams[4].fParam : ScriptParams[3].fParam;
    const auto y2 = is3D ? ScriptParams[5].fParam : ScriptParams[4].fParam;
    const float zmin    = is3D ? std::min(ScriptParams[3].fParam, ScriptParams[6].fParam) : 0.0f;
    const float zmax    = is3D ? std::max(ScriptParams[3].fParam, ScriptParams[6].fParam) : 0.0f;
    const auto  width   = is3D ? ScriptParams[7].fParam : ScriptParams[5].fParam;
    const auto  highlight = is3D ? ScriptParams[8].fParam : ScriptParams[6].fParam;

    // Direction angle of the p1->p2 line, normalized to [0, TWO_PI] (computed in doubles in the original)
    double angle = (double)CGeneral::GetRadianAngleBetweenPoints(x1, y1, x2, y2) + (double)1.5707964f; // + HALF_PI (0x858FE4)
    while (angle < 0.0) {
        angle += (double)6.2831855f; // TWO_PI (0x858CBC)
    }
    while ((double)6.2831855f < angle) { // TWO_PI (0x858CBC)
        angle -= (double)6.2831855f;
    }

    const auto sinA = (float)sin(angle);
    const auto cosA = (float)cos(angle);
    const CVector2D rotTo{ x2 + sinA * width, y2 - cosA * width };     // "to" corner shifted by width
    const CVector2D rotFrom{ x1 + sinA * width, y1 - cosA * width };   // "from" corner shifted by width

    if (check) {
        const CVector posn = ped->GetRealPosition(); // in-vehicle -> vehicle posn (CPed::GetRealPosition)

        const auto diagX     = x2 - x1;
        const auto diagY     = y2 - y1;
        const auto sideX     = rotFrom.x - x1; //  sinA * width
        const auto sideY     = rotFrom.y - y1; // -cosA * width
        const auto diagLenSq = diagX * diagX + diagY * diagY;
        const auto sideLen   = sqrt(sideX * sideX + sideY * sideY);

        auto diagXn = 1.0f; // 0x858624
        auto diagYn = diagY;
        if (diagLenSq > 0.0f) {
            diagYn = (1.0f / sqrt(diagLenSq)) * diagY;
            diagXn = (1.0f / sqrt(diagLenSq)) * diagX;
        }

        const auto projDiag = (posn.x - x1) * diagXn + (posn.y - y1) * diagYn;
        if (projDiag >= 0.0f && projDiag < sqrt(diagLenSq)) {
            const auto projSide = (posn.x - x1) * sideX + (posn.y - y1) * sideY;
            if (projSide >= 0.0f && projSide < sideLen) {
                if (!is3D || (zmin <= posn.z && posn.z < zmax)) {
                    switch (commandId) {
                    case COMMAND_IS_CHAR_IN_ANGLED_AREA_ON_FOOT_2D:
                    case COMMAND_IS_CHAR_STOPPED_IN_ANGLED_AREA_ON_FOOT_2D:
                    case COMMAND_IS_CHAR_IN_ANGLED_AREA_ON_FOOT_3D:
                    case COMMAND_IS_CHAR_STOPPED_IN_ANGLED_AREA_ON_FOOT_3D:
                        result = !ped->bInVehicle;
                        break;
                    case COMMAND_IS_CHAR_IN_ANGLED_AREA_IN_CAR_2D:
                    case COMMAND_IS_CHAR_STOPPED_IN_ANGLED_AREA_IN_CAR_2D:
                    case COMMAND_IS_CHAR_IN_ANGLED_AREA_IN_CAR_3D:
                    case COMMAND_IS_CHAR_STOPPED_IN_ANGLED_AREA_IN_CAR_3D:
                        result = ped->bInVehicle;
                        break;
                    case COMMAND_IS_CHAR_IN_ANGLED_AREA_2D:
                    case COMMAND_IS_CHAR_STOPPED_IN_ANGLED_AREA_2D:
                    case COMMAND_IS_CHAR_IN_ANGLED_AREA_3D:
                    case COMMAND_IS_CHAR_STOPPED_IN_ANGLED_AREA_3D:
                        result = true;
                        break;
                    }
                }
            }
        }
    }

    UpdateCompareFlag(result);

    if (highlight != 0.0f) {
        CTheScripts::HighlightImportantAngledArea(
            reinterpret_cast<uint32>(this) + reinterpret_cast<uint32>(m_IP),
            x1, y1, x2, y2, rotTo.x, rotTo.y, rotFrom.x, rotFrom.y,
            is3D ? (zmax + zmin) * 0.5f : -100.0f // 0x858B8C == 0.5f
        );
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugAngledSquare(CVector2D{ x1, y1 }, CVector2D{ x2, y2 }, rotTo, rotFrom);
    }
}

// 0x488780
// Handles COMMAND_IS_FLAME_IN_ANGLED_AREA_2D (0x072D) and COMMAND_IS_FLAME_IN_ANGLED_AREA_3D (0x072E).
// NOTE: unlike the char/object variants there is no entity handle - params start with the corner coordinates.
void CRunningScript::FlameInAngledAreaCheckCommand(int32 commandId) {
    const auto is3D = commandId == COMMAND_IS_FLAME_IN_ANGLED_AREA_3D;
    CollectParameters(is3D ? 8 : 6);

    const auto x1 = ScriptParams[0].fParam;
    const auto y1 = ScriptParams[1].fParam;
    const auto x2 = is3D ? ScriptParams[3].fParam : ScriptParams[2].fParam;
    const auto y2 = is3D ? ScriptParams[4].fParam : ScriptParams[3].fParam;
    const float zmin    = is3D ? std::min(ScriptParams[2].fParam, ScriptParams[5].fParam) : 0.0f;
    const float zmax    = is3D ? std::max(ScriptParams[2].fParam, ScriptParams[5].fParam) : 0.0f;
    const auto  width   = is3D ? ScriptParams[6].fParam : ScriptParams[4].fParam;
    const auto  highlight = is3D ? ScriptParams[7].fParam : ScriptParams[5].fParam;

    // Direction angle of the p1->p2 line, normalized to [0, TWO_PI] (computed in doubles in the original)
    double angle = (double)CGeneral::GetRadianAngleBetweenPoints(x1, y1, x2, y2) + (double)1.5707964f; // + HALF_PI (0x858FE4)
    while (angle < 0.0) {
        angle += (double)6.2831855f; // TWO_PI (0x858CBC)
    }
    while ((double)6.2831855f < angle) { // TWO_PI (0x858CBC)
        angle -= (double)6.2831855f;
    }

    const auto sinA = (float)sin(angle);
    const auto cosA = (float)cos(angle);
    const CVector2D rotTo{ x2 + sinA * width, y2 - cosA * width };     // "to" corner shifted by width
    const CVector2D rotFrom{ x1 + sinA * width, y1 - cosA * width };   // "from" corner shifted by width

    const auto diagLen = sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
    const auto sideLen = sqrt((rotFrom.x - x1) * (rotFrom.x - x1) + (rotFrom.y - y1) * (rotFrom.y - y1));

    auto result = false;
    for (int32 i = 0; i < 100 && !result; i++) {
        CVector flamePosn;
        if (!CShotInfo::GetFlameThrowerShotPosn((uint8)i, flamePosn)) {
            continue;
        }

        auto diagX = x2 - x1;
        auto diagY = y2 - y1;
        const auto diagLenSq = diagX * diagX + diagY * diagY;
        if (diagLenSq > 0.0f) {
            diagX *= (1.0f / sqrt(diagLenSq)); // 0x858624 == 1.0f
            diagY *= (1.0f / sqrt(diagLenSq));
        } else {
            diagX = 1.0f;
        }

        const auto projDiag = (flamePosn.x - x1) * diagX + (flamePosn.y - y1) * diagY;
        if (projDiag >= 0.0f && projDiag < diagLen) {
            auto sideX = rotFrom.x - x1; //  sinA * width
            auto sideY = rotFrom.y - y1; // -cosA * width
            const auto sideLenSq = sideX * sideX + sideY * sideY;
            if (sideLenSq > 0.0f) {
                sideX *= (1.0f / sqrt(sideLenSq));
                sideY *= (1.0f / sqrt(sideLenSq));
            } else {
                sideX = 1.0f;
            }

            const auto projSide = (flamePosn.x - x1) * sideX + (flamePosn.y - y1) * sideY;
            if (projSide >= 0.0f && projSide < sideLen) {
                if (!is3D || (zmin <= flamePosn.z && flamePosn.z < zmax)) {
                    result = true;
                }
            }
        }
    }

    UpdateCompareFlag(result);

    if (highlight != 0.0f) {
        CTheScripts::HighlightImportantAngledArea(
            reinterpret_cast<uint32>(this) + reinterpret_cast<uint32>(m_IP),
            x1, y1, x2, y2, rotTo.x, rotTo.y, rotFrom.x, rotFrom.y,
            is3D ? (zmax + zmin) * 0.5f : -100.0f // 0x858B8C == 0.5f
        );
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugAngledSquare(CVector2D{ x1, y1 }, CVector2D{ x2, y2 }, rotTo, rotFrom);
    }
}

// 0x4883F0
// Handles COMMAND_IS_OBJECT_IN_ANGLED_AREA_2D (0x08E3) and COMMAND_IS_OBJECT_IN_ANGLED_AREA_3D (0x08E4).
void CRunningScript::ObjectInAngledAreaCheckCommand(int32 commandId) {
    const auto is3D = commandId == COMMAND_IS_OBJECT_IN_ANGLED_AREA_3D;
    CollectParameters(is3D ? 9 : 7);

    const auto object = GetObjectPool()->GetAtRef(ScriptParams[0].iParam);

    const auto x1 = ScriptParams[1].fParam;
    const auto y1 = ScriptParams[2].fParam;
    const auto x2 = is3D ? ScriptParams[4].fParam : ScriptParams[3].fParam;
    const auto y2 = is3D ? ScriptParams[5].fParam : ScriptParams[4].fParam;
    const float zmin    = is3D ? std::min(ScriptParams[3].fParam, ScriptParams[6].fParam) : 0.0f;
    const float zmax    = is3D ? std::max(ScriptParams[3].fParam, ScriptParams[6].fParam) : 0.0f;
    const auto  width   = is3D ? ScriptParams[7].fParam : ScriptParams[5].fParam;
    const auto  highlight = is3D ? ScriptParams[8].fParam : ScriptParams[6].fParam;

    // Direction angle of the p1->p2 line, normalized to [0, TWO_PI] (computed in doubles in the original)
    double angle = (double)CGeneral::GetRadianAngleBetweenPoints(x1, y1, x2, y2) + (double)1.5707964f; // + HALF_PI (0x858FE4)
    while (angle < 0.0) {
        angle += (double)6.2831855f; // TWO_PI (0x858CBC)
    }
    while ((double)6.2831855f < angle) { // TWO_PI (0x858CBC)
        angle -= (double)6.2831855f;
    }

    const auto sinA = (float)sin(angle);
    const auto cosA = (float)cos(angle);
    const CVector2D rotTo{ x2 + sinA * width, y2 - cosA * width };     // "to" corner shifted by width
    const CVector2D rotFrom{ x1 + sinA * width, y1 - cosA * width };   // "from" corner shifted by width

    const CVector posn = object->GetPosition();

    const auto diagX     = x2 - x1;
    const auto diagY     = y2 - y1;
    const auto sideX     = rotFrom.x - x1; //  sinA * width
    const auto sideY     = rotFrom.y - y1; // -cosA * width
    const auto diagLenSq = diagX * diagX + diagY * diagY;
    const auto sideLen   = sqrt(sideX * sideX + sideY * sideY);

    auto diagXn = 1.0f; // 0x858624
    auto diagYn = diagY;
    if (diagLenSq > 0.0f) {
        diagYn = (1.0f / sqrt(diagLenSq)) * diagY;
        diagXn = (1.0f / sqrt(diagLenSq)) * diagX;
    }

    auto result = false;
    const auto projDiag = (posn.x - x1) * diagXn + (posn.y - y1) * diagYn;
    if (projDiag >= 0.0f && projDiag < sqrt(diagLenSq)) {
        const auto projSide = (posn.x - x1) * sideX + (posn.y - y1) * sideY;
        if (projSide >= 0.0f && projSide < sideLen) {
            if (!is3D || (zmin <= posn.z && posn.z < zmax)) {
                result = true;
            }
        }
    }

    UpdateCompareFlag(result);

    if (highlight != 0.0f) {
        CTheScripts::HighlightImportantAngledArea(
            reinterpret_cast<uint32>(this) + reinterpret_cast<uint32>(m_IP),
            x1, y1, x2, y2, rotTo.x, rotTo.y, rotFrom.x, rotFrom.y,
            is3D ? (zmax + zmin) * 0.5f : -100.0f // 0x858B8C == 0.5f
        );
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugAngledSquare(CVector2D{ x1, y1 }, CVector2D{ x2, y2 }, rotTo, rotFrom);
    }
}

// 0x464D70
bool CRunningScript::IsPedDead(CPed* ped) const {
    ePedState pedState = ped->m_nPedState;
    return pedState == PEDSTATE_DEAD || pedState == PEDSTATE_DIE || pedState == PEDSTATE_DIE_BY_STEALTH;
}

// 0x489490
bool CRunningScript::ThisIsAValidRandomPed(ePedType pedType, bool civilian, bool gang, bool criminal) {
    switch (pedType) {
    case PED_TYPE_CIVMALE:
    case PED_TYPE_CIVFEMALE:
        return civilian;
    case PED_TYPE_GANG1:
    case PED_TYPE_GANG2:
    case PED_TYPE_GANG3:
    case PED_TYPE_GANG4:
    case PED_TYPE_GANG5:
    case PED_TYPE_GANG6:
    case PED_TYPE_GANG7:
    case PED_TYPE_GANG8:
    case PED_TYPE_GANG9:
        return gang;
    case PED_TYPE_CRIMINAL:
    case PED_TYPE_PROSTITUTE:
        return criminal;
    default:
        return false;
    }
}

// 0x485A50
void CRunningScript::DoDeathArrestCheck() {
    if (!m_IsDeathArrestCheckEnabled) {
        return;
    }

    if (!CTheScripts::IsPlayerOnAMission()) {
        return;
    }

    if (const auto& pi = FindPlayerInfo(); !pi.IsRestartingAfterDeath() && !pi.IsRestartingAfterArrest()) {
        return;
    }

    CMessages::ClearSmallMessagesOnly();
    memset(&CTheScripts::ScriptSpace[CTheScripts::OnAMissionFlag], 0, sizeof(uint32));
    ResetIP();

    m_DoneDeathArrest = true;
    m_WakeTime        = 0;
}

// 0x464F50
void CRunningScript::GetCorrectPedModelIndexForEmergencyServiceType(ePedType pedType, uint32* typeSpecificModelId) {
    switch (*typeSpecificModelId) {
    case MODEL_LAPD1:
    case MODEL_SFPD1:
    case MODEL_LVPD1:
    case MODEL_LAPDM1:
        if (pedType == PED_TYPE_COP) {
            *typeSpecificModelId = COP_TYPE_CITYCOP;
        }
        break;
    case MODEL_CSHER:
        if (pedType == PED_TYPE_COP) {
            *typeSpecificModelId = COP_TYPE_CSHER;
        }
        break;
    case MODEL_SWAT:
        if (pedType == PED_TYPE_COP) {
            *typeSpecificModelId = COP_TYPE_SWAT1;
        }
        break;
    case MODEL_FBI:
        if (pedType == PED_TYPE_COP) {
            *typeSpecificModelId = COP_TYPE_FBI;
        }
        break;
    case MODEL_ARMY:
        if (pedType == PED_TYPE_COP) {
            *typeSpecificModelId = COP_TYPE_ARMY;
        }
        break;
    default:
        return;
    }
}

// Returns state of pad button
// 0x485B10
int16 CRunningScript::GetPadState(uint16 playerIndex, eButtonId buttonId) {
    const auto* pad = CPad::GetPad(playerIndex);
    switch (buttonId) {
    case BUTTON_LEFT_STICK_X:    return pad->NewState.LeftStickX;
    case BUTTON_LEFT_STICK_Y:    return pad->NewState.LeftStickY;
    case BUTTON_RIGHT_STICK_X:   return pad->NewState.RightStickX;
    case BUTTON_RIGHT_STICK_Y:   return pad->NewState.RightStickY;
    case BUTTON_LEFT_SHOULDER1:  return pad->NewState.LeftShoulder1;
    case BUTTON_LEFT_SHOULDER2:  return pad->NewState.LeftShoulder2;
    case BUTTON_RIGHT_SHOULDER1: return pad->NewState.RightShoulder1;
    case BUTTON_RIGHT_SHOULDER2: return pad->NewState.RightShoulder2;
    case BUTTON_DPAD_UP:         return pad->NewState.DPadUp;
    case BUTTON_DPAD_DOWN:       return pad->NewState.DPadDown;
    case BUTTON_DPAD_LEFT:       return pad->NewState.DPadLeft;
    case BUTTON_DPAD_RIGHT:      return pad->NewState.DPadRight;
    case BUTTON_START:           return pad->NewState.Start;
    case BUTTON_SELECT:          return pad->NewState.Select;
    case BUTTON_SQUARE:          return pad->NewState.ButtonSquare;
    case BUTTON_TRIANGLE:        return pad->NewState.ButtonTriangle;
    case BUTTON_CROSS:           return pad->NewState.ButtonCross;
    case BUTTON_CIRCLE:          return pad->NewState.ButtonCircle;
    case BUTTON_LEFTSHOCK:       return pad->NewState.ShockButtonL;
    case BUTTON_RIGHTSHOCK:      return pad->NewState.ShockButtonR;
    default:                     return OR_CONTINUE;
    }
}

// 0x46AF50
// Handles COMMAND_TASK_PICK_UP_OBJECT (0x070A) and friends.
void CRunningScript::ScriptTaskPickUpObject(int32 commandId) {
    CollectParameters(7);
    const auto pedHandle   = ScriptParams[0].iParam; // saved: 2nd CollectParameters overwrites ScriptParams[0]
    const auto object      = GetObjectPool()->GetAtRef(ScriptParams[1].iParam);
    const CVector offset{ ScriptParams[2].fParam, ScriptParams[3].fParam, ScriptParams[4].fParam };
    const auto boneFrameId = (uint8)ScriptParams[5].iParam; // see ePedNode
    const auto boneFlags   = (uint8)ScriptParams[6].iParam; // see eHoldEntityBoneFlags

    char animName[24];
    char ifpName[16];
    ReadTextLabelFromScript(animName, 24);
    ReadTextLabelFromScript(ifpName, 16);

    const auto bUseDefaultAnim = strncmp(animName, "NULL", 5) == 0 || strncmp(ifpName, "NULL", 5) == 0;

    CollectParameters(1);
    // 0x10 == ANIMATION_IS_PARTIAL, 0x18 == ANIMATION_IS_PARTIAL | ANIMATION_IS_FINISH_AUTO_REMOVE
    const auto animFlags = static_cast<eAnimationFlags>(ScriptParams[0].iParam == 0
        ? (ANIMATION_IS_PARTIAL | ANIMATION_IS_FINISH_AUTO_REMOVE)
        : ANIMATION_IS_PARTIAL);

    CTask* task{};

    // NOTE: null-check of CTask::operator new (0x61A5A0) in the original omitted (never fails in practice)
    if (bUseDefaultAnim) {
        task = new CTaskSimpleHoldEntity(object, &offset, boneFrameId, boneFlags, ANIM_ID_NO_ANIMATION_SET, ANIM_GROUP_DEFAULT, false);
    } else {
        task = new CTaskSimpleHoldEntity(object, &offset, boneFrameId, boneFlags, animName, ifpName, animFlags);
    }

    if (pedHandle == -1) {
        if (commandId == COMMAND_TASK_PICK_UP_OBJECT) {
            task = new CTaskSimpleAffectSecondaryBehaviour{ true, TASK_SECONDARY_PARTIAL_ANIM, task };
            CTaskSequences::AddTaskToActiveSequence(task);
        }
    } else {
        const auto ped = GetPedPool()->GetAtRef(pedHandle);
        ped->GetTaskManager().SetTaskSecondary(task, commandId == COMMAND_TASK_PICK_UP_OBJECT ? TASK_SECONDARY_PARTIAL_ANIM : TASK_SECONDARY_ATTACK);

        const auto slot = CPedScriptedTaskRecord::GetVacantSlot();
        CPedScriptedTaskRecord::ms_scriptedTasks[slot].Set(ped, commandId, task);
    }
}

// 0x464DC0
void CRunningScript::SetCharCoordinates(CPed& ped, CVector posn, bool warpGang, bool offset) {
    CWorld::PutToGroundIfTooLow(posn);

    CVehicle* vehicle = ped.GetVehicleIfInOne();
    if (vehicle) {
        posn.z += vehicle->GetDistanceFromCentreOfMassToBaseOfModel();
        vehicle->Teleport(posn, false);
        CTheScripts::ClearSpaceForMissionEntity(posn, vehicle);
    } else {
        posn.z += offset ? ped.GetDistanceFromCentreOfMassToBaseOfModel() : 0.0f;
        CTheScripts::ClearSpaceForMissionEntity(posn, &ped);
        auto* group = CPedGroups::GetPedsGroup(&ped);
        if (group && group->GetMembership().IsLeader(&ped) && warpGang) {
            group->Teleport(posn);
        } else {
            ped.Teleport(posn, false);
        }
    }
}

// 0x463CA0
tScriptParam* CRunningScript::GetPointerToLocalVariable(int32 loc) {
    return &GetLocal<tScriptParam>(loc);
}

/*!
 * @addr 0x463CC0
 * @brief Returns pointer to a local script variable.
 *
 * @param arrayBaseOffset          The offset of the array
 * @param index                    Index of the variable inside the array
 * @param arrayEntriesSizeAsParams Size of 1 variable in the array (In terms of `tScriptParam`'s - So for a regular `int` (or float, etc) variable this will be `1`, for long strings it's `4` and for short one's it's `2`)
 */
tScriptParam* CRunningScript::GetPointerToLocalArrayElement(int32 arrayBaseOffset, uint16 index, uint8 arrayEntriesSizeAsParams) {
    return &GetArrayLocal<tScriptParam>(arrayBaseOffset, index, arrayEntriesSizeAsParams);
}

/*!
 * Returns pointer to script variable of any type.
 * @addr 0x464790
 */
tScriptParam* CRunningScript::GetPointerToScriptVariable(eScriptVariableType) {
    uint8  arrElemSize;
    uint16 arrVarOffset;
    int32  arrElemIdx;

    int8 type = CTheScripts::Read1ByteFromScript(m_IP);
    switch (type) {
    case SCRIPT_PARAM_GLOBAL_NUMBER_VARIABLE:
    case SCRIPT_PARAM_GLOBAL_SHORT_STRING_VARIABLE:
    case SCRIPT_PARAM_GLOBAL_LONG_STRING_VARIABLE:
    {
        uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
        return reinterpret_cast<tScriptParam*>(&CTheScripts::ScriptSpace[index]);
    }
    case SCRIPT_PARAM_LOCAL_NUMBER_VARIABLE:
    case SCRIPT_PARAM_LOCAL_SHORT_STRING_VARIABLE:
    case SCRIPT_PARAM_LOCAL_LONG_STRING_VARIABLE:
    {
        uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
        return GetPointerToLocalVariable(index);
    }

    case SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY:
    case SCRIPT_PARAM_GLOBAL_SHORT_STRING_ARRAY:
    case SCRIPT_PARAM_GLOBAL_LONG_STRING_ARRAY:
        ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
        if (type == SCRIPT_PARAM_GLOBAL_LONG_STRING_ARRAY)
            return reinterpret_cast<tScriptParam*>(&CTheScripts::ScriptSpace[LONG_STRING_SIZE * arrElemIdx + arrVarOffset]);
        else if (type == SCRIPT_PARAM_GLOBAL_SHORT_STRING_ARRAY)
            return reinterpret_cast<tScriptParam*>(&CTheScripts::ScriptSpace[SHORT_STRING_SIZE * arrElemIdx + arrVarOffset]);
        else // SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY
            return reinterpret_cast<tScriptParam*>(&CTheScripts::ScriptSpace[4 * arrElemIdx + arrVarOffset]);

    case SCRIPT_PARAM_LOCAL_NUMBER_ARRAY:
    case SCRIPT_PARAM_LOCAL_SHORT_STRING_ARRAY:
    case SCRIPT_PARAM_LOCAL_LONG_STRING_ARRAY:
        ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
        if (type == SCRIPT_PARAM_LOCAL_LONG_STRING_ARRAY)
            arrElemSize = 4;
        else if (type == SCRIPT_PARAM_LOCAL_SHORT_STRING_ARRAY)
            arrElemSize = 2;
        else // SCRIPT_PARAM_LOCAL_NUMBER_ARRAY
            arrElemSize = 1;
        return GetPointerToLocalArrayElement(arrVarOffset, arrElemIdx, arrElemSize);

    default:
        NOTSA_UNREACHABLE();
    }
}

/*!
 * Returns offset of global variable
 * @addr 0x464700
 */
uint16 CRunningScript::GetIndexOfGlobalVariable() {
    switch (const auto t = GetAtIPAs<uint8>()) {
    case SCRIPT_PARAM_GLOBAL_NUMBER_VARIABLE:
        return GetAtIPAs<uint16>();
    case SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY: {
        uint16 base;
        int32  idx;
        ReadArrayInformation(true, &base, &idx);
        return base + sizeof(tScriptParam) * idx;
    }
    default:
        NOTSA_UNREACHABLE();
    }
}

// 0x464080
void CRunningScript::CollectParameters(int16 count) {
    uint16 arrVarOffset;
    int32  arrElemIdx;

    for (auto i = 0; i < count; i++) {
        switch (CTheScripts::Read1ByteFromScript(m_IP)) {
        case SCRIPT_PARAM_STATIC_INT_32BITS:
            ScriptParams[i].iParam = CTheScripts::Read4BytesFromScript(m_IP);
            break;
        case SCRIPT_PARAM_GLOBAL_NUMBER_VARIABLE:
        {
            uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
            ScriptParams[i].iParam = *reinterpret_cast<int32*>(&CTheScripts::ScriptSpace[index]);
            break;
        }
        case SCRIPT_PARAM_LOCAL_NUMBER_VARIABLE:
        {
            uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
            ScriptParams[i] = *GetPointerToLocalVariable(index);
            break;
        }
        case SCRIPT_PARAM_STATIC_INT_8BITS:
            ScriptParams[i].iParam = CTheScripts::Read1ByteFromScript(m_IP);
            break;
        case SCRIPT_PARAM_STATIC_INT_16BITS:
            ScriptParams[i].iParam = CTheScripts::Read2BytesFromScript(m_IP);
            break;
        case SCRIPT_PARAM_STATIC_FLOAT:
            ScriptParams[i].fParam = CTheScripts::ReadFloatFromScript(m_IP);
            break;
        case SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY:
            ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
            ScriptParams[i].iParam = *reinterpret_cast<int32*>(&CTheScripts::ScriptSpace[arrVarOffset + 4 * arrElemIdx]);
            break;
        case SCRIPT_PARAM_LOCAL_NUMBER_ARRAY:
            ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
            ScriptParams[i] = *GetPointerToLocalArrayElement(arrVarOffset, arrElemIdx, 1);
            break;
        }
    }
}

/*!
 * Collects parameter and returns it.
 * @addr 0x464250
 */
int32 CRunningScript::CollectNextParameterWithoutIncreasingPC() {
    uint16 arrVarOffset;
    int32  arrElemIdx;
    uint8* ip = m_IP;
    int32  result = -1;

    switch (CTheScripts::Read1ByteFromScript(m_IP)) {
    case SCRIPT_PARAM_STATIC_INT_32BITS:
    case SCRIPT_PARAM_STATIC_FLOAT:
        result = CTheScripts::Read4BytesFromScript(m_IP);
        break;
    case SCRIPT_PARAM_GLOBAL_NUMBER_VARIABLE:
    {
        uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
        result = *reinterpret_cast<int32*>(&CTheScripts::ScriptSpace[index]);
        break;
    }
    case SCRIPT_PARAM_LOCAL_NUMBER_VARIABLE:
    {
        uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
        result = GetPointerToLocalVariable(index)->iParam;
        break;
    }
    case SCRIPT_PARAM_STATIC_INT_8BITS:
        result = CTheScripts::Read1ByteFromScript(m_IP);
        break;
    case SCRIPT_PARAM_STATIC_INT_16BITS:
        result = CTheScripts::Read2BytesFromScript(m_IP);
        break;
    case SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY:
        ReadArrayInformation(false, &arrVarOffset, &arrElemIdx);
        result = *reinterpret_cast<int32*>(&CTheScripts::ScriptSpace[arrVarOffset + 4 * arrElemIdx]);
        break;
    case SCRIPT_PARAM_LOCAL_NUMBER_ARRAY:
        ReadArrayInformation(false, &arrVarOffset, &arrElemIdx);
        result = GetPointerToLocalArrayElement(arrVarOffset, arrElemIdx, 1)->iParam;
        break;
    }

    m_IP = ip;
    return result;
}

/*!
 * @addr 0x464370
 */
void CRunningScript::StoreParameters(int16 count) {
    uint16 arrVarOffset;
    int32  arrElemIdx;

    for (auto i = 0; i < count; i++) {
        switch (CTheScripts::Read1ByteFromScript(m_IP)) {
        case SCRIPT_PARAM_GLOBAL_NUMBER_VARIABLE:
        {
            uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
            *reinterpret_cast<int32*>(&CTheScripts::ScriptSpace[index]) = ScriptParams[i].iParam;
            break;
        }
        case SCRIPT_PARAM_LOCAL_NUMBER_VARIABLE:
        {
            uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
            *GetPointerToLocalVariable(index) = ScriptParams[i];
            break;
        }
        case SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY:
            ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
            *reinterpret_cast<int32*>(&CTheScripts::ScriptSpace[arrVarOffset + 4 * arrElemIdx]) = ScriptParams[i].iParam;
            break;
        case SCRIPT_PARAM_LOCAL_NUMBER_ARRAY:
            ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
            *GetPointerToLocalArrayElement(arrVarOffset, arrElemIdx, 1) = ScriptParams[i];
            break;
        }
    }
}

// Reads array var base offset and element index from index variable.
// 0x463CF0
void CRunningScript::ReadArrayInformation(int32 updateIP, uint16* outArrayBase, int32* outArrayIndex) {
    const auto op = GetAtIPAs<scm::ArrayAccess>(updateIP);
    *outArrayIndex = op.IdxVarIsGlobal
        ? GetGlobal<int32>(op.IdxVarLoc)
        : GetLocal<int32>(op.IdxVarLoc);
    *outArrayBase = op.ArrayBase;
}

// Collects parameters and puts them to local variables of new script
// 0x464500
void CRunningScript::ReadParametersForNewlyStartedScript(CRunningScript* newScript) {
    uint16 arrVarOffset;
    int32  arrElemIdx;
    int8   type = CTheScripts::Read1ByteFromScript(m_IP);

    for (int i = 0; type != SCRIPT_PARAM_END_OF_ARGUMENTS; type = CTheScripts::Read1ByteFromScript(m_IP), i++) {
        switch (type) {
        case SCRIPT_PARAM_STATIC_INT_32BITS:
            newScript->m_LocalVars[i].iParam = CTheScripts::Read4BytesFromScript(m_IP);
            break;
        case SCRIPT_PARAM_GLOBAL_NUMBER_VARIABLE:
        {
            uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
            newScript->m_LocalVars[i].iParam = *reinterpret_cast<int32*>(&CTheScripts::ScriptSpace[index]);
            break;
        }
        case SCRIPT_PARAM_LOCAL_NUMBER_VARIABLE:
        {
            uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
            newScript->m_LocalVars[i] = *GetPointerToLocalVariable(index);
            break;
        }
        case SCRIPT_PARAM_STATIC_INT_8BITS:
            newScript->m_LocalVars[i].iParam = CTheScripts::Read1ByteFromScript(m_IP);
            break;
        case SCRIPT_PARAM_STATIC_INT_16BITS:
            newScript->m_LocalVars[i].iParam = CTheScripts::Read2BytesFromScript(m_IP);
            break;
        case SCRIPT_PARAM_STATIC_FLOAT:
            newScript->m_LocalVars[i].fParam = CTheScripts::ReadFloatFromScript(m_IP);
            break;
        case SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY:
            ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
            newScript->m_LocalVars[i].iParam = *reinterpret_cast<int32*>(&CTheScripts::ScriptSpace[arrVarOffset + 4 * arrElemIdx]);
            break;
        case SCRIPT_PARAM_LOCAL_NUMBER_ARRAY:
            ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
            newScript->m_LocalVars[i] = *GetPointerToLocalArrayElement(arrVarOffset, arrElemIdx, 1);
            break;
        default:
            break;
        }
    }
}

// Collects string parameter
// 0x463D50
void CRunningScript::ReadTextLabelFromScript(char* buffer, uint8 nBufferLength) {
    uint16 arrVarOffset;
    int32  arrElemIdx;

    int8 type = CTheScripts::Read1ByteFromScript(m_IP);
    switch (type) {
    case SCRIPT_PARAM_STATIC_SHORT_STRING:
        for (auto i = 0; i < SHORT_STRING_SIZE; i++)
            buffer[i] = CTheScripts::Read1ByteFromScript(m_IP);
        break;

    case SCRIPT_PARAM_GLOBAL_SHORT_STRING_VARIABLE:
    {
        uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
        strncpy_s(buffer, SHORT_STRING_SIZE, (char*)&CTheScripts::ScriptSpace[index], SHORT_STRING_SIZE);
        break;
    }

    case SCRIPT_PARAM_LOCAL_SHORT_STRING_VARIABLE:
    {
        uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
        strncpy_s(buffer, SHORT_STRING_SIZE, (char*) GetPointerToLocalVariable(index), SHORT_STRING_SIZE);
        break;
    }

    case SCRIPT_PARAM_GLOBAL_SHORT_STRING_ARRAY:
    case SCRIPT_PARAM_GLOBAL_LONG_STRING_ARRAY:
        ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
        if (type == SCRIPT_PARAM_GLOBAL_SHORT_STRING_ARRAY)
            strncpy_s(buffer, SHORT_STRING_SIZE, (char*) & CTheScripts::ScriptSpace[SHORT_STRING_SIZE * arrElemIdx + arrVarOffset], SHORT_STRING_SIZE);
        else
            strncpy_s(buffer, SHORT_STRING_SIZE, (char*) & CTheScripts::ScriptSpace[LONG_STRING_SIZE * arrElemIdx + arrVarOffset], std::min<uint8>(nBufferLength, LONG_STRING_SIZE));
        break;

    case SCRIPT_PARAM_LOCAL_SHORT_STRING_ARRAY:
    case SCRIPT_PARAM_LOCAL_LONG_STRING_ARRAY:
        ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
        if (type == SCRIPT_PARAM_LOCAL_SHORT_STRING_ARRAY)
            strncpy_s(buffer, SHORT_STRING_SIZE, (char*) GetPointerToLocalArrayElement(arrVarOffset, arrElemIdx, 2), SHORT_STRING_SIZE);
        else {
            const auto bufferLength = std::min<uint8>(nBufferLength, LONG_STRING_SIZE);
            strncpy_s(buffer, bufferLength, (char*)GetPointerToLocalArrayElement(arrVarOffset, arrElemIdx, 4), bufferLength);
        }
        break;

    case SCRIPT_PARAM_STATIC_PASCAL_STRING:
    {
        int16 nStringLen = CTheScripts::Read1ByteFromScript(m_IP); // sign extension. max size = 127, not 255
        for (auto i = 0; i < nStringLen; i++)
            buffer[i] = CTheScripts::Read1ByteFromScript(m_IP);

        if (nStringLen < nBufferLength)
            memset(&buffer[(uint8)nStringLen], 0, (uint8)(nBufferLength - nStringLen));
        break;
    }

    case SCRIPT_PARAM_STATIC_LONG_STRING:
        // slightly changed code: original code is a bit messy and calls Read1ByteFromScript
        // in a loop and does some additional checks to ensure that buffer can hold the data
        strncpy_s(buffer, LONG_STRING_SIZE, (char*)m_IP, std::min<uint8>(nBufferLength, LONG_STRING_SIZE));
        m_IP += LONG_STRING_SIZE;
        break;

    case SCRIPT_PARAM_GLOBAL_LONG_STRING_VARIABLE:
    {
        uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
        strncpy_s(buffer, LONG_STRING_SIZE, (char*) & CTheScripts::ScriptSpace[index], std::min<uint8>(nBufferLength, LONG_STRING_SIZE));
        break;
    }

    case SCRIPT_PARAM_LOCAL_LONG_STRING_VARIABLE:
    {
        uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
        strncpy_s(buffer, LONG_STRING_SIZE, (char*) GetPointerToLocalVariable(index), std::min<uint8>(nBufferLength, LONG_STRING_SIZE));
        break;
    }

    default:
        break;
    }
}

// Updates comparement flag, used in conditional commands
// 0x4859D0
void CRunningScript::UpdateCompareFlag(bool state) {
    if (m_NotFlag)
        state = !state;

    if (m_AndOrState == ANDOR_NONE) {
        m_CondResult = state;
        return;
    }

    if (m_AndOrState >= ANDS_1 && m_AndOrState <= ANDS_8) {
        m_CondResult &= state;
        if (m_AndOrState == ANDS_1)
            m_AndOrState = ANDOR_NONE;
        else
            m_AndOrState--;

        return;
    }

    if (m_AndOrState >= ORS_1 && m_AndOrState <= ORS_8) {
        m_CondResult |= state;
        if (m_AndOrState == ORS_1)
            m_AndOrState = ANDOR_NONE;
        else
            m_AndOrState--;

        return;
    }
}

// Sets instruction pointer, used in GOTO-like commands
// 0x464DA0
void CRunningScript::UpdatePC(int32 newIP) {
    m_IP = newIP >= 0
        ? &CTheScripts::ScriptSpace[newIP]
        : m_BaseIP + std::abs(newIP);
}

// 0x469EB0, inlined
OpcodeResult CRunningScript::ProcessOneCommand() {
    ++CTheScripts::CommandsExecuted;

    const auto op = GetAtIPAs<scm::Instruction>();

    // Check if IP is valid pre-return
    notsa::ScopeGuard guardIP{[this]() {
        const auto next{ GetAtIPAs<scm::Instruction>(false) };
        VERIFY(next.Command <= COMMAND_HIGHEST_VANILLA_ID);
    }};

#ifdef NOTSA_SCRIPT_TRACING
    // snprintf is faster (in debug at least) - Gotta stick to it for now
    char msg[4096];
    sprintf_s(msg, "[%s][IP: 0x%X + 0x%X]: %s [0x%X]", m_szName, LOG_PTR(m_pBaseIP), LOG_PTR(m_IP - m_pBaseIP), notsa::script::GetScriptCommandName((eScriptCommands)op.Command).data(), (size_t)op.Command);
    SPDLOG_LOGGER_TRACE(logger, msg);
    //SPDLOG_LOGGER_TRACE(logger, "[{}][IP: {:#x} + {:#x}]: {} [{:#x}]", BaseFilename, LOG_PTR(m_pBaseIP), LOG_PTR(m_IP - m_pBaseIP), notsa::script::GetScriptCommandName((eScriptCommands)op.command), (size_t)op.command);
#endif
    
    m_NotFlag = op.NotFlag;

    if (const auto handler = CustomCommandHandlerOf((eScriptCommands)(op.Command))) {
        return std::invoke(handler, this);
    } else {
        return std::invoke(s_OriginalCommandHandlerTable[(size_t)op.Command / 100], this, (eScriptCommands)(op.Command));
    }
}

// 0x469F00
OpcodeResult CRunningScript::Process() {
    if (m_SceneSkipIP && CCutsceneMgr::IsCutsceneSkipButtonBeingPressed()) {
        CHud::m_BigMessage[1][0] = 0;
        UpdatePC(std::exchange(m_SceneSkipIP, 0));
        m_WakeTime = 0;
    }

    if (m_UsesMissionCleanup) {
        DoDeathArrestCheck();
    }

    if (m_ThisMustBeTheOnlyMissionRunning && CTheScripts::FailCurrentMission == 1) {
        if (m_StackDepth > 0) {
            ResetIP();
        }
    }

    CTheScripts::ReinitialiseSwitchStatementData();

    if (CTimer::GetTimeInMS() >= (uint32)m_WakeTime) {
        while (ProcessOneCommand() == OR_CONTINUE)
            ; // Process commands
    }

    return OR_CONTINUE;
}

void CRunningScript::HighlightImportantArea(CVector2D from, CVector2D to, float z) {
    CTheScripts::HighlightImportantArea(reinterpret_cast<int32>(this) + reinterpret_cast<int32>(m_IP), from.x, from.y, to.x, to.y, z);
}

void CRunningScript::HighlightImportantArea(CRect area, float z) {
    HighlightImportantArea(area.GetTopLeft(), area.GetBottomRight(), z);
}

void CRunningScript::HighlightImportantArea(CVector from, CVector to) {
    HighlightImportantArea(CVector2D{ from }, CVector2D{ to }, (from.z + to.z) / 2.f);
}

void CRunningScript::HighlightImportantAngledArea(uint32 id, CVector2D a, CVector2D b, CVector2D c, CVector2D d) {
    NOTSA_UNREACHABLE(); // Fuck this, we dont need it!
}

notsa::script::CommandHandlerFunction& CRunningScript::CustomCommandHandlerOf(eScriptCommands command) {
    return s_CustomCommandHandlerTable[(size_t)(command)];
}

void CRunningScript::ResetIP() {
    assert(m_StackDepth > 0); // Original bug...
    do {
        m_IP = std::exchange(m_IPStack[m_StackDepth--], nullptr); // NOTSA: Also clear the stack, we don't need it anymore
    } while (m_StackDepth);
}
