#include "StdInc.h"

#include "TaskInteriorUseInfo.h"
#include "Interior/Interior_c.h"
#include "Interior/InteriorInfo_t.h"
#include "Interior/InteriorManager_c.h"
#include "TaskInteriorGoToInfo.h"
#include "TaskInteriorLieInBed.h"
#include "TaskInteriorSitAtDesk.h"
#include "TaskInteriorShopKeeper.h"
#include "../TaskSimpleStandStill.h"
#include "../TaskSimpleRunAnim.h"
#include "../TaskSimpleRunTimedAnim.h"
#include "../../TaskComplexSequence.h"

// Не реверснут (vtable 0x870314): используется только как тип this для вызова
// оригинального конструктора 0x675C30 в CreateFirstSubTask (см. ниже).
class CTaskInteriorSitOnChair;

void CTaskInteriorUseInfo::InjectHooks() {
    RH_ScopedVirtualClass(CTaskInteriorUseInfo, 0x8702e8, 11);
    RH_ScopedCategory("Tasks/TaskTypes/Interior");

    RH_ScopedInstall(Constructor, 0x675A50);
    RH_ScopedInstall(Destructor, 0x675A90);

    RH_ScopedVMTInstall(Clone, 0x675AB0);
    RH_ScopedVMTInstall(GetTaskType, 0x675A80);
    RH_ScopedVMTInstall(MakeAbortable, 0x675B30);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x676880);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x675B60);
    RH_ScopedVMTInstall(ControlSubTask, 0x675C00);
}

// 0x675A50
CTaskInteriorUseInfo::CTaskInteriorUseInfo(InteriorInfo_t* interiorInfo, Interior_c* interior, int32 duration, bool bDoInstantly) :
    m_IntInfo{interiorInfo},
    m_Int{interior},
    m_Dur{duration},
    m_bDoInstantly{bDoInstantly}
{
}

// 0x675AB0
CTaskInteriorUseInfo::CTaskInteriorUseInfo(const CTaskInteriorUseInfo& o) :
    CTaskInteriorUseInfo{o.m_IntInfo, o.m_Int, o.m_Dur, o.m_bDoInstantly}
{
}

// 0x675A90
CTaskInteriorUseInfo::~CTaskInteriorUseInfo() {
    m_IntInfo->IsInUse = false;
}

// 0x675B30
bool CTaskInteriorUseInfo::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    if (m_IntInfo->EntityIgnoredCollision) {
        ped->m_pEntityIgnoredCollision = nullptr;
    }
    return CTaskComplex::MakeAbortable(ped, priority, event);
}

// 0x676880
CTask* CTaskInteriorUseInfo::CreateNextSubTask(CPed* ped) {
    if (!m_IntInfo) {
        return nullptr;
    }

    // decomp: (**(code**)(**(int**)(this+0x8) + 0x10))() == 0x579 (GetTaskType через vtable).
    // 0x579 = TASK_INTERIOR_GOTO_INFO — сабтаск из CreateFirstSubTask; т.е. это первый заход
    // после прибытия к инфо-точке: выбираем активность по типу InteriorInfo_t.
    if (m_pSubTask->GetTaskType() == TASK_INTERIOR_GOTO_INFO) {
        switch (m_IntInfo->Type) {
        case eInteriorInfoType::UNK_1: { // 1 — alloc 0x2C
            // Класс не реверснут: TASK_INTERIOR_SIT_ON_CHAIR (1407/0x57F — FIX(verify): было 1406);
            // vtable 0x870314 (слот между
            // UseInfo 0x8702e8 и LieInBed 0x870338), sizeof = 0x2C, конструктор (duration, interiorInfo, bDoInstantly)
            // @0x675C30 (аргументы как у CTaskInteriorSitAtDesk). Вызываем оригинальный конструктор напрямую.
            const auto mem = CTask::operator new(0x2C);
            // CallMethodAndReturn требует классовый this (std::is_class), поэтому кастуем к fwd-declared типу:
            return plugin::CallMethodAndReturn<CTask*, 0x675C30, CTaskInteriorSitOnChair*, int32, InteriorInfo_t*, bool>((CTaskInteriorSitOnChair*)mem, m_Dur, m_IntInfo, m_bDoInstantly);
        }
        case eInteriorInfoType::UNK_2: { // 2 — alloc 0x20
            // decomp: FUN_0062F310(&DAT_0098967F, 0, 0, 8.0f) — 0x0098967F это ЧИСЛО 9999999 (0x98967F),
            // а не адрес (Ghidra изобразила int-константу адресом; ~2.7 часов, «бесконечное» ожидание).
            return new CTaskSimpleStandStill(9999999, false, false, 8.0f);
        }
        case eInteriorInfoType::UNK_3: // 3 — alloc 0x30: FUN_00675E20(dur, info, 0, doInstantly) = LieInBed, левый край
            return new CTaskInteriorLieInBed(m_Dur, m_IntInfo, false, m_bDoInstantly);
        case eInteriorInfoType::UNK_4: // 4 — alloc 0x30: LieInBed, правый край
            return new CTaskInteriorLieInBed(m_Dur, m_IntInfo, true, m_bDoInstantly);
        case eInteriorInfoType::UNK_5: { // 5 — alloc 0x34
            if (!InteriorManager_c::AreAnimsLoaded(0)) { // 0 => "int_house"
                break;
            }
            const auto blendDelta = m_bDoInstantly ? 1000.0f : 4.0f; // 0x447A0000 / 0x40800000
            return new CTaskSimpleRunTimedAnim(ANIM_GROUP_INT_HOUSE /*0x54*/, ANIM_ID_WASH_UP /*0x15A*/, blendDelta, 4.0f, m_Dur, false);
        }
        case eInteriorInfoType::UNK_6: // 6 — alloc 0x38: FUN_00676010 = CTaskInteriorSitAtDesk::Constructor
            return new CTaskInteriorSitAtDesk(m_Dur, m_IntInfo, m_bDoInstantly);
        case eInteriorInfoType::UNK_7: { // 7 — alloc 0x20
            if (!InteriorManager_c::AreAnimsLoaded(2)) { // 2 => "int_office"
                break;
            }
            return new CTaskSimpleStandStill(5000, false, false, 8.0f);
        }
        case eInteriorInfoType::UNK_8: { // 8 — alloc 0x20 (shop browse)
            if (!InteriorManager_c::AreAnimsLoaded(1)) { // 1 => "int_shop"
                break;
            }
            // FUN_005EFFE0 = CPed::Say — this (ped) Ghidra потеряла, восстановлен семантически.
            // 0xBC = CTX_GLOBAL_SHOP_BROWSE (PedSpeechContexts.h:199); остальные арги = дефолты (0, 1.0f, false, false, false).
            ped->Say(CTX_GLOBAL_SHOP_BROWSE);
            return new CTaskSimpleRunAnim(ANIM_GROUP_INT_SHOP /*0x56*/, ANIM_ID_SHOP_SHELF /*0x145*/, 4.0f);
        }
        case eInteriorInfoType::UNK_9: { // 9 — alloc 0x40 + 0x20 (shop buy)
            if (!InteriorManager_c::AreAnimsLoaded(1)) { // 1 => "int_shop"
                break;
            }
            // decomp: FUN_00594B90(0) = InteriorGroup_c::GetPed(0) — this потерян Ghidra; единственный
            // доступный InteriorGroup_c* — m_Int->m_pGroup (Interior_c+0xC).
            const auto shopKeeperPed = m_Int->m_pGroup->GetPed(0);
            // decomp: ped+0x47C -> +0x14 = CPed::m_pIntelligence -> m_TaskMgr.m_aPrimaryTasks[TASK_PRIMARY_DEFAULT]
            // (CTaskManager без vtable, массив первичек с +0x0; [4] = TASK_PRIMARY_DEFAULT).
            const auto shopKeeperTask = shopKeeperPed->GetIntelligence()->m_TaskMgr.m_aPrimaryTasks[TASK_PRIMARY_DEFAULT];
            if (shopKeeperTask->GetTaskType() == TASK_INTERIOR_SHOPKEEPER) { // 0x57D = 1405
                // decomp: byte по смещению +0xC задачи = 1 -> CTaskInteriorShopKeeper::m_bStartServeAnim
                // (первое поле после базы 0xC, bool).
                // FIX(verify): поле ПРИВАТНОЕ (TaskInteriorShopKeeper.h:41, friend'а нет) — обращение
                // по имени не компилируется из TaskInteriorUseInfo.cpp; заменено на запись по смещению
                // (та же техника, что в 0x690220.cpp для приватных полей EnterCar/SeekEntity).
                *reinterpret_cast<uint8*>(reinterpret_cast<uintptr_t>(shopKeeperTask) + 0xC) = 1;

                ped->Say(CTX_GLOBAL_SHOP_BUY); // 0xBD (PedSpeechContexts.h:200)

                const auto seq = new CTaskComplexSequence{}; // 0x632BD0
                seq->AddTask(new CTaskSimpleRunAnim(ANIM_GROUP_INT_SHOP /*0x56*/, ANIM_ID_SHOP_PAY /*0x146*/, 4.0f));
                return seq; // null-ветки operator new в декомпиле — артефакт nothrow-new, не воспроизводим
            }
            break;
        }
        case eInteriorInfoType::UNK_10: { // 10 — alloc 0x20
            return new CTaskSimpleStandStill(0, true, false, 8.0f); // decomp: (0, 1, 0, 8.0f) — стоять до аборта
        }
        }
    }

    // decomp: if (*(int*)(m_IntInfo + 0x1C)) ped->(0x128) = 0;
    // InteriorInfo_t+0x1C = EntityIgnoredCollision; CPed+0x128 = m_pEntityIgnoredCollision (Physical.h:137,
    // следующий за m_pDamageEntity@0x124) — тот же паттерн, что в MakeAbortable/ControlSubTask.
    if (m_IntInfo->EntityIgnoredCollision) {
        ped->m_pEntityIgnoredCollision = nullptr;
    }
    return nullptr;
}

// 0x675B60
CTask* CTaskInteriorUseInfo::CreateFirstSubTask(CPed* ped) {
    if (!m_IntInfo) {
        return nullptr;
    }

    // decomp: *(byte*)(m_IntInfo + 0x1) = 1  ->  InteriorInfo_t::IsInUse (InteriorInfo_t.h: Type@0x0, IsInUse@0x1)
    m_IntInfo->IsInUse = true;

    // Оригинал: operator new (CTask::operator new, 0x61A5A0) на 0x18 байт, вызов базового
    // конструктора 0x61A3B0, затем инлайнутая инициализация: vtable 0x8702bc
    // (= CTaskInteriorGoToInfo, см. RH_ScopedVirtualClass в TaskInteriorGoToInfo.cpp:13),
    // поля: +0xC m_IntInfo, +0x10 m_Int, +0x14 (byte) m_bDoInstantly. sizeof(CTaskInteriorGoToInfo) = 0x18.
    // Это тот же паттерн, что задокументирован в TaskComplexPlayHandSignalAnim.cpp:105 (Clone).
    // ped не используется (оригинал тоже).
    return new CTaskInteriorGoToInfo(m_IntInfo, m_Int, m_bDoInstantly);
}

// 0x675C00
CTask* CTaskInteriorUseInfo::ControlSubTask(CPed* ped) {
    if (!m_IntInfo) {
        return nullptr;
    }
    if (m_IntInfo->EntityIgnoredCollision) {
        ped->m_pEntityIgnoredCollision = m_IntInfo->EntityIgnoredCollision;
    }
    return m_pSubTask;
}
