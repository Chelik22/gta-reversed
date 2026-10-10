// ============================================================================
// INTEGRATED FILE — source/game_sa/Tasks/TaskTypes/TaskComplexUseClosestFreeScriptedAttractor.cpp
// Функции: ComputeClosestFreeScriptedEffect (0x634740), CreateFirstSubTask (0x639530)
//
// ИСТОЧНИКИ: фрагменты batch_out/fresh/0x634740.cpp (VERIFY-wf5b: FIXED — слотов 64,
// не 96: декомпил выходит при record > 0xC3AAFF, дизасм `add esi,0x24; cmp esi,0xc3ab00;
// jl` => 0x900/0x24 = 64; массив inUse не тронут) и 0x639530.cpp (VERIFY-wf5b: OK).
//
// ПРАВКИ ИНТЕГРАТОРА против фрагментов:
//  - 0x639530: CInterestingEvents::Add — НЕ-статический метод (InterestingEvents.h:78);
//    вызов заменён на репо-идиому `g_InterestingEvents.Add(...)`
//    (extern в InterestingEvents.h:85; прецеденты EventHandler.cpp:884, Explosion.cpp:394).
//
// Сверено в репо: C2dEffectPedAttractor (Plugins/TwoDEffectPlugin/2dEffect.h:237,
// бинарно совместим с C2dEffect; C2dEffectBase начинается с m_Pos — 2dEffect.h),
// CPedAttractorManager::HasEmptySlot(const C2dEffectPedAttractor*, const CEntity*)
// (PedAttractorManager.h:131), GetPedAttractorManager() (:215),
// CInterestingEvents::INTERESTING_EVENT_3 = 3 / INTERESTING_EVENT_25 (InterestingEvents.h:19,41),
// CTaskComplexUseEffect/-Running/-Sprinting(C2dEffectPedAttractor*, CEntity*)
// (TaskComplexUseEffect.h:18 и др.), eMoveState RUN/SPRINT (Enums/eMoveState.h),
// CPed::m_nModelIndex (Entity.h:107) / m_nPedType (Ped.h:310),
// CVector::SquaredMagnitude (Core/Vector.h:128).
// NOTE: временная матрица оригинала (0x59AED0/0x59ACD0) на результат не влияет — опущена
// (задокументировано в шапке фрагмента).
// ============================================================================

#include "StdInc.h"
#include "TaskComplexUseClosestFreeScriptedAttractor.h"

#include "InterestingEvents.h"
#include "TaskComplexUseEffect.h"
#include "TaskComplexUseEffectRunning.h"
#include "TaskComplexUseEffectSprinting.h"

void CTaskComplexUseClosestFreeScriptedAttractor::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexUseClosestFreeScriptedAttractor, 0x86e428, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x6346F0);
    RH_ScopedInstall(Destructor, 0x634720);

    RH_ScopedGlobalInstall(ComputeClosestFreeScriptedEffect, 0x634740);

    RH_ScopedVMTInstall(Clone, 0x636F70);
    RH_ScopedVMTInstall(GetTaskType, 0x634710);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x634730);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x639530);
    RH_ScopedVMTInstall(ControlSubTask, 0x634890);
}

CTaskComplexUseClosestFreeScriptedAttractor::CTaskComplexUseClosestFreeScriptedAttractor(eMoveState ms) :
    m_MoveState{ms}
{
}

CTaskComplexUseClosestFreeScriptedAttractor::CTaskComplexUseClosestFreeScriptedAttractor(const CTaskComplexUseClosestFreeScriptedAttractor&) :
    CTaskComplexUseClosestFreeScriptedAttractor{}
{
}

// 0x634740
C2dEffect* CTaskComplexUseClosestFreeScriptedAttractor::ComputeClosestFreeScriptedEffect(CPed const& ped) {
    struct SScriptedEffectRecord { // 0x24 байта
        int32 models[4];             // ID моделей, либо -2 => "любая модель указанного типа педа"
        int32 pedTypes[4];           // типы педов для -2 записей
        uint8 _pad[3];
        uint8 hasModelRestrictions;  // +0x20
    };

    // FIX(verify wf5b): слотов 64, НЕ 96 (records физически занимают 0xC3A200..0xC3AB00)
    constexpr auto NUM_SCRIPTED_ATTRACTOR_SLOTS = 64;

    // Глобалы оригинала (не объявлены в репо):
    //   0xC3A1A0 - флаги "слот занят"; 0xC3A200 - записи ограничений (0x24 на слот);
    //   0xC3AB00 - сами C2dEffect'ы (0x40 на слот). Создаются скриптовыми опкодами аттракторов.
    auto& inUse      = StaticRef<uint8_t[96]>(0xC3A1A0);
    auto& records    = StaticRef<SScriptedEffectRecord[64]>(0xC3A200);
    auto& effectData = StaticRef<uint8_t[64 * 0x40]>(0xC3AB00);

    C2dEffect* closestFx = nullptr;
    auto       bestDistSq = FLT_MAX;

    for (auto i = 0; i < NUM_SCRIPTED_ATTRACTOR_SLOTS; i++) {
        if (!inUse[i]) {
            continue;
        }

        const auto& rec = records[i];
        if (rec.hasModelRestrictions) {
            // Пед должен подходить под ограничения (модель или тип педа)
            bool allowed = false;
            for (auto j = 0; j < 4; j++) {
                if ((int32)ped.m_nModelIndex == rec.models[j]) {
                    allowed = true;
                    break;
                }
            }
            for (auto j = 0; !allowed && j < 4; j++) {
                if (rec.models[j] == -2 && rec.pedTypes[j] == ped.m_nPedType) {
                    allowed = true;
                }
            }
            if (!allowed) {
                continue;
            }
        }

        auto*       fx    = reinterpret_cast<C2dEffectPedAttractor*>(&effectData[i * 0x40]);
        const auto& fxPos = *reinterpret_cast<const CVector*>(fx); // C2dEffect начинается с позиции

        const auto distSq = (fxPos - ped.GetPosition()).SquaredMagnitude();
        if (distSq >= bestDistSq) {
            continue;
        }

        if (GetPedAttractorManager()->HasEmptySlot(fx, nullptr)) {
            bestDistSq = distSq;
            closestFx  = reinterpret_cast<C2dEffect*>(fx);
        }
    }

    return closestFx;
}

// 0x639530
CTask* CTaskComplexUseClosestFreeScriptedAttractor::CreateFirstSubTask(CPed* ped) {
    const auto fx = ComputeClosestFreeScriptedEffect(*ped);
    if (!fx) {
        return nullptr;
    }

    g_InterestingEvents.Add(CInterestingEvents::INTERESTING_EVENT_3, ped); // 0x602590

    const auto fxAttractor = reinterpret_cast<C2dEffectPedAttractor*>(fx);

    switch (m_MoveState) {
    case PEDMOVE_RUN:    return new CTaskComplexUseEffectRunning{ fxAttractor, nullptr };    // 0x636780
    case PEDMOVE_SPRINT: return new CTaskComplexUseEffectSprinting{ fxAttractor, nullptr }; // 0x636860
    default:             return new CTaskComplexUseEffect{ fxAttractor, nullptr };           // 0x6321F0 (вкл. PEDMOVE_WALK)
    }
}
