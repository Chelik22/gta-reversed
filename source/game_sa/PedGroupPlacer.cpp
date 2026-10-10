#include "StdInc.h"

#include "PedPlacement.h"
#include "Tasks/TaskTypes/TaskComplexBeInGroup.h"
#include "Tasks/TaskTypes/TaskComplexFollowLeaderInFormation.h"
#include "Tasks/Allocators/PedGroup/PedGroupDefaultTaskAllocators.h" // NOTE: тянет TaskComplexWanderGang.h - НЕ включать его здесь напрямую (MSVC #pragma once не дедуплицирует разные написания пути)

// Глобалы оригинала (.rdata, base 0x400000) - значения сняты с оригинального exe (байты проверены):
static auto& formation_flt_858624 = StaticRef<float>(0x858624); // 1.0f - допуск по z / +1.0 к z для пробы земли
static auto& formation_flt_86C850 = StaticRef<float>(0x86C850); // 42.5f - множитель min-дистанции до игрока (0x6116C0 * 42.5)
static auto& chat_flt_858CBC      = StaticRef<float>(0x858CBC); // 6.2831853f (2*pi)
static auto& chat_flt_858B8C      = StaticRef<float>(0x858B8C); // 0.5f - "сторона" полигона: R = sqrt(c / (1 - cos(2*pi/n)))
static auto& random_flt_858B14    = StaticRef<float>(0x858B14); // 1/32768 (0x38000000) - для направления wander
static auto& random_flt_859000    = StaticRef<float>(0x859000); // 8.0f - число направлений wander
static auto& random_flt_86FCB0    = StaticRef<float>(0x86FCB0); // 0.5f - fTargetRadius CTaskComplexWanderGang

void CPedGroupPlacer::InjectHooks() {
    RH_ScopedClass(CPedGroupPlacer);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(PlaceFormationGroup, 0x5FC9B0);
    RH_ScopedInstall(PlaceChatGroup, 0x5FCE80);
    RH_ScopedInstall(PlaceRandomGroup, 0x5FD330);
    RH_ScopedInstall(PlaceGroup, 0x5FD810);
}

// 0x5FC9B0
// ABI: thiscall, ret 0x10 (this + 4 арга: type, numOfPeds, &origin, unused).
// NOTE(asm): параметр unused (ePedGroupDefaultTaskAllocatorType) В ТЕЛЕ НЕ ЧИТАЕТСЯ: скан
// [0x5FC9B0,0x5FCE80) даёт ссылки только на 0xC09908 (FollowAnyMeans) и 0xC09920 (ms_groups);
// 0xC0990C (FollowLimited) не используется - для FOLLOW_ANY_MEANS и FOLLOW_LIMITED аллокатор один.
// (В декомпиле 0x5FC9B0.c тело - это case 0/1 инлайна FUN_005fd810 = PlaceGroup; проверено
// сравнением прологов: 0x5FC9B0 начинается с call 0x5FB800 ДО проверки аргументов.)
bool CPedGroupPlacer::PlaceFormationGroup(ePedType type, uint32 numOfPeds, const CVector& origin, ePedGroupDefaultTaskAllocatorType unused) {
    (void)unused;

    const auto groupId = CPedGroups::AddGroup(); // 0x5FB800, -1 = нет слота
    if (groupId < 0) {
        return false;
    }

    // Если точка в зоне видимости камеры - проверяем, что игрок достаточно далеко
    if (TheCamera.IsSphereVisible(origin, 3.0f)) { // 0x420D40, radius 3.0f (0x40400000)
        const auto& playerPos = FindPlayerPed()->GetPosition(); // 0x56E210(-1); if(m_matrix) m_matrix->pos else placement.pos
        if ((origin - playerPos).Magnitude2D() < CPopulation::PedCreationDistMultiplier() * formation_flt_86C850) { // 0x6116C0
            return false;
        }
    }

    if (!CPedPlacement::IsPositionClearForPed(origin, 3.0f, -1, nullptr, true, true, true)) { // 0x616860
        return false;
    }

    // Привязка z к земле: spawnZ = max(origin.z, groundZ + 1.0f)
    bool groundFound{};
    const auto groundZ = CWorld::FindGroundZFor3DCoord({ origin.x, origin.y, origin.z + formation_flt_858624 }, &groundFound, nullptr); // 0x5696C0
    if (!groundFound) {
        return false;
    }
    const auto spawnZ = std::max(origin.z, groundZ + formation_flt_858624);

    const auto model      = CGangs::ChooseGangPedModel((eGangID)(type - PED_TYPE_GANG1)); // 0x5DE5A0 (thunk), type-7 => eGangID 0..9
    const auto modelInfo  = CModelInfo::ms_modelInfoPtrs[(int32)model];
    if (!modelInfo || !modelInfo->GetRwObject()) { // +0x1C != 0 (модель загружена)
        return false;
    }

    const CVector spawnPos{ origin.x, origin.y, spawnZ };
    const auto    leader = CPopulation::AddPed(type, model, spawnPos, false); // 0x612710, makeWander=false
    if (!leader) {
        return false;
    }

    int32  numPlaced = 1;
    CPed*  peds[9]{}; // оригинал: local_30[9]; [0] не используется, лидер в [1]
    peds[1] = leader;

    // Последователи: по оффсетам формации от ПОЗИЦИИ ЛИДЕРА
    for (auto offsetIdx = 1; offsetIdx < (int32)numOfPeds; offsetIdx++) {
        const auto memberModel      = CGangs::ChooseGangPedModel((eGangID)(type - PED_TYPE_GANG1));
        const auto memberModelInfo  = CModelInfo::ms_modelInfoPtrs[(int32)memberModel];
        if (!memberModelInfo || !memberModelInfo->GetRwObject()) {
            continue;
        }

        // Пед создаётся в точке спавна, потом переносится к лидеру
        const auto member = CPopulation::AddPed(type, memberModel, spawnPos, false);
        if (!member) {
            continue;
        }

        const auto& offsets = CTaskComplexFollowLeaderInFormation::ms_offsets.Offsets; // 0xC196E8, CVector2D[8], stride 8 ([esi*8+0xC196E8]/[esi*8+0xC196EC])
        member->SetPosn(leader->GetPosition() + CVector{ offsets[offsetIdx].x, offsets[offsetIdx].y, 0.f }); // 0x40FE30 (operator+) + 0x4241C0 (SetPosn)

        // Проба земли под НОВОЙ позицией педа
        const auto memberPosX = member->GetPosition().x;
        const auto memberPosY = member->GetPosition().y;
        const auto memberZ0   = member->GetPosition().z;
        bool memberGroundFound{};
        const auto memberGroundZ = CWorld::FindGroundZFor3DCoord({ memberPosX, memberPosY, memberZ0 + formation_flt_858624 }, &memberGroundFound, nullptr);
        if (!memberGroundFound) {
            CPopulation::RemovePed(member); // 0x610F20
            continue;
        }
        const auto memberZ = std::max(memberZ0, memberGroundZ + formation_flt_858624);

        // |z_педа - z_лидера| <= 1.0f (fcomp 0x858624, test ah,0x41)
        const auto& leaderPos = leader->GetPosition();
        if (!(std::fabs(memberZ - leaderPos.z) <= formation_flt_858624)) {
            CPopulation::RemovePed(member);
            continue;
        }

        // LOS от педа к лидеру (только здания)
        if (!CWorld::GetIsLineOfSightClear(CVector{ memberPosX, memberPosY, memberZ }, leaderPos, true, false, false, false)) { // 0x56A490
            CPopulation::RemovePed(member);
            continue;
        }

        member->SetPosn(memberPosX, memberPosY, memberZ); // 0x420B80 (SetPosn(x,y,z))
        peds[numPlaced + 1] = member;
        numPlaced++;
        CVisibilityPlugins::SetClumpAlpha(member->GetRpClump(), 0); // 0x732B00 (ped+0x18, alpha 0 - фейд-ин населения)
    }

    if (numPlaced < 1) { // ветка оригинала (cmp edi,1 / jl) - на практике недостижима: лидер уже добавлен
        for (auto i = 0; i < numPlaced; i++) {
            CPopulation::RemovePed(peds[i + 1]);
        }
        return false;
    }

    auto& group = CPedGroups::GetGroup(groupId); // imul esi,0x2D4 + 0xC09920 в асме
    group.GetIntelligence().SetDefaultTaskAllocator(CPedGroupDefaultTaskAllocators::Get(ePedGroupDefaultTaskAllocatorType::FOLLOW_ANY_MEANS)); // 0x5FB280, this=group+0x30, 0xC09908
    group.GetMembership().SetLeader(peds[1]);   // 0x5FB9C0, this=group+0x08
    group.GetMembership().Process();            // 0x5FBA60
    group.GetIntelligence().Process();          // 0x5FC4A0
    {
        // Лидер: BeInGroup(groupId, isLeader=true), слот TASK_PRIMARY_PRIMARY (3)
        CTask* task{};
        if (const auto t = new CTaskComplexBeInGroup{ groupId, true }) { // 0x61A5A0(0x28) + 0x632E50
            task = t;
        }
        peds[1]->GetTaskManager().SetTask(task, TASK_PRIMARY_PRIMARY, false); // 0x681AF0, this=ped+0x47C+4
    }
    for (auto i = 1; i < numPlaced; i++) {
        const auto member = peds[i + 1];
        group.GetMembership().AddFollower(member); // 0x5F8020
        group.GetMembership().Process();
        group.GetIntelligence().Process();
        CTask* task{};
        if (const auto t = new CTaskComplexBeInGroup{ groupId, false }) {
            task = t;
        }
        member->GetTaskManager().SetTask(task, TASK_PRIMARY_PRIMARY, false);
    }
    return true;
}

// 0x5FCE80
// ABI: thiscall, ret 0x10 (this + 4 арга).
// NOTE(asm): дизасм 0x5FCE80 начинается с чтения аргументов (push edi; mov edi,[esp+0x98]) и
// проверки numOfPeds < 2 ДО AddGroup - соответствует case 2/3 инлайна PlaceGroup (тело после
// switch в декомпиле 0x5FC9B0.c). Ссылки функции: только 0xC09910 (StandStill) и 0xC09920.
// (Аллокатор 0xC09910 = STAND_STILL в сегодняшнем репо - имя внезапное, но адрес и поведение оригинальные.)
bool CPedGroupPlacer::PlaceChatGroup(ePedType type, uint32 numOfPeds, const CVector& origin, ePedGroupDefaultTaskAllocatorType unused) {
    (void)unused;

    if (numOfPeds < 2) {
        return false;
    }

    const auto groupId = CPedGroups::AddGroup(); // 0x5FB800
    if (groupId < 0) {
        return false;
    }

    // Правильный полигон: шаг угла и радиус описанной окружности
    const auto angleStep  = chat_flt_858CBC / (float)numOfPeds;                                   // 2*pi/n
    const auto circRadius = std::sqrt(chat_flt_858B8C / (formation_flt_858624 - std::cos(angleStep))); // sqrt(0.5 / (1 - cos(2*pi/n)))

    if (TheCamera.IsSphereVisible(origin, circRadius)) { // 0x420D40
        const auto& playerPos = FindPlayerPed()->GetPosition();
        if ((origin - playerPos).Magnitude2D() < CPopulation::PedCreationDistMultiplier() * formation_flt_86C850) {
            return false;
        }
    }

    if (!CPedPlacement::IsPositionClearForPed(origin, circRadius, -1, nullptr, true, true, true)) { // 0x616860
        return false;
    }

    int32  numPlaced{};
    CPed*  peds[9]{}; // оригинал: local_50[9], заполнение с [0]; [0] = будущий лидер
    CVector firstPos; // позиция первого размещённого педа (local_5c/58/54)

    for (auto i = 0; i < (int32)numOfPeds; i++) {
        // Джиттер: (rand * (1/32767) * 0.4 - 0.2) == GetRandomNumberInRange(-0.2, 0.2)
        // (константы 0x858C7C = 1/32767, 0x858EE8 = 0.4, 0x858CC4 = 0.2; репо использует
        // 1/32767 (RAND_MAX_FLOAT_RECIPROCAL) и rand() - санкционированное репо отклонение)
        const auto angleJitter  = CGeneral::GetRandomNumberInRange(-0.2f, 0.2f) * angleStep;  // 2-й множитель = angleStep (local_84)
        const auto radiusJitter = CGeneral::GetRandomNumberInRange(-0.2f, 0.2f) * circRadius; // множитель = circRadius (local_80)
        const auto angle        = (float)i * angleStep + angleJitter;
        const auto radius       = circRadius + radiusJitter;
        const auto x            = std::cos(angle) * radius;
        const auto y            = std::sin(angle) * radius;

        bool groundFound{};
        const auto groundZ = CWorld::FindGroundZFor3DCoord({ x + origin.x, y + origin.y, origin.z + formation_flt_858624 }, &groundFound, nullptr);
        if (!groundFound) {
            continue;
        }
        auto z = groundZ + formation_flt_858624;
        if (z < origin.z) {
            z = origin.z;
        }
        const CVector pos{ x + origin.x, y + origin.y, z };
        if (i == 0) {
            firstPos = pos;
        }

        const auto model     = CGangs::ChooseGangPedModel((eGangID)(type - PED_TYPE_GANG1));
        const auto modelInfo = CModelInfo::ms_modelInfoPtrs[(int32)model];
        if (!modelInfo || !modelInfo->GetRwObject()) {
            continue;
        }

        // Скан ближайших сущностей радиусом = радиусу колл-модели педа (+0x14 -> +0x24)
        CEntity* found[9]{}; // оригинал: local_30[9], обнуляется
        const auto searchRadius = modelInfo->GetColModel()->GetBoundRadius();
        CPedPlacement::IsPositionClearForPed(pos, searchRadius, 9, found, true, true, true); // 0x616860; ретёрн игнорируется

        // Все найденные рядом должны быть нашими уже размещёнными педами (иначе - "чужак" рядом)
        bool strangerNearby{};
        for (auto j = 0; j < 9; j++) {
            if (!found[j]) {
                continue;
            }
            bool ours{};
            for (auto k = 0; k < numPlaced; k++) {
                if (found[j] == peds[k]) {
                    ours = true;
                    break;
                }
            }
            if (!ours) {
                strangerNearby = true;
                break;
            }
        }

        const auto losOk = (numPlaced == 0) || CWorld::GetIsLineOfSightClear(pos, firstPos, true, false, false, false); // 0x56A490
        const auto zOk   = (numPlaced == 0) || std::fabs(pos.z - firstPos.z) < formation_flt_858624;

        if (strangerNearby || !losOk || !zOk) {
            continue;
        }

        const auto ped = CPopulation::AddPed(type, model, pos, false);
        if (!ped) {
            CPopulation::RemovePed(nullptr); // 0x610F20(0) - как в оригинале
            continue;
        }
        peds[numPlaced] = ped;
        numPlaced++;
        const auto heading = CGeneral::GetRadianAngleBetweenPoints(origin.x, origin.y, pos.x, pos.y); // 0x53CBE0 (x1,y1,x2,y2)
        ped->m_fCurrentRotation = heading; // ped+0x558
        ped->m_fAimingRotation  = heading; // ped+0x55C
        CVisibilityPlugins::SetClumpAlpha(ped->GetRpClump(), 0); // 0x732B00
    }

    if (numPlaced > 0) {
        auto& group = CPedGroups::GetGroup(groupId);
        group.GetIntelligence().SetDefaultTaskAllocator(CPedGroupDefaultTaskAllocators::Get(ePedGroupDefaultTaskAllocatorType::STAND_STILL)); // 0x5FB280, 0xC09910
        group.GetMembership().SetLeader(peds[0]); // 0x5FB9C0
        group.GetMembership().Process();          // 0x5FBA60
        group.GetIntelligence().Process();        // 0x5FC4A0
        {
            CTask* task{};
            if (const auto t = new CTaskComplexBeInGroup{ groupId, true }) {
                task = t;
            }
            peds[0]->GetTaskManager().SetTask(task, TASK_PRIMARY_PRIMARY, false); // 0x681AF0, слот 3
        }
        for (auto i = 1; i < numPlaced; i++) {
            group.GetMembership().AddFollower(peds[i]); // 0x5F8020
            group.GetMembership().Process();
            group.GetIntelligence().Process();
            CTask* task{};
            if (const auto t = new CTaskComplexBeInGroup{ groupId, false }) {
                task = t;
            }
            peds[i]->GetTaskManager().SetTask(task, TASK_PRIMARY_PRIMARY, false);
        }
        return true;
    }

    for (auto i = 0; i < numPlaced; i++) {
        CPopulation::RemovePed(peds[i]); // 0x610F20
    }
    return false;
}

// 0x5FD330
// ABI: thiscall, ret 0x10 (this + 4 арга).
// NOTE(asm): 0x5FD330 ссылается только на 0xC09918 (Random) и 0xC09920; тело = case 5 инлайна
// PlaceGroup в декомпиле 0x5FC9B0.c (прологи 0x5FCE80/0x5FD330 идентичны).
// Отличие от PlaceChatGroup: аллокатор RANDOM и на каждого педа выдаётся wander-таск
// (слот TASK_PRIMARY_DEFAULT=4) ПЕРЕД BeInGroup (слот 3); лидер получает BeInGroup(false).
bool CPedGroupPlacer::PlaceRandomGroup(ePedType type, uint32 numOfPeds, const CVector& origin, ePedGroupDefaultTaskAllocatorType unused) {
    (void)unused;

    if (numOfPeds < 2) {
        return false;
    }

    const auto groupId = CPedGroups::AddGroup(); // 0x5FB800
    if (groupId < 0) {
        return false;
    }

    const auto angleStep  = chat_flt_858CBC / (float)numOfPeds;                                    // 2*pi/n
    const auto circRadius = std::sqrt(chat_flt_858B8C / (formation_flt_858624 - std::cos(angleStep))); // sqrt(0.5 / (1 - cos(2*pi/n)))

    if (TheCamera.IsSphereVisible(origin, circRadius)) {
        const auto& playerPos = FindPlayerPed()->GetPosition();
        if ((origin - playerPos).Magnitude2D() < CPopulation::PedCreationDistMultiplier() * formation_flt_86C850) {
            return false;
        }
    }

    if (!CPedPlacement::IsPositionClearForPed(origin, circRadius, -1, nullptr, true, true, true)) {
        return false;
    }

    int32  numPlaced{};
    CPed*  peds[9]{};
    CVector firstPos;

    for (auto i = 0; i < (int32)numOfPeds; i++) {
        const auto angleJitter  = CGeneral::GetRandomNumberInRange(-0.2f, 0.2f) * angleStep;  // rand*(1/32767)*0.4-0.2 (0x858C7C/0x858EE8/0x858CC4)
        const auto radiusJitter = CGeneral::GetRandomNumberInRange(-0.2f, 0.2f) * circRadius;
        const auto angle        = (float)i * angleStep + angleJitter;
        const auto radius       = circRadius + radiusJitter;
        const auto x            = std::cos(angle) * radius;
        const auto y            = std::sin(angle) * radius;

        bool groundFound{};
        const auto groundZ = CWorld::FindGroundZFor3DCoord({ x + origin.x, y + origin.y, origin.z + formation_flt_858624 }, &groundFound, nullptr);
        if (!groundFound) {
            continue;
        }
        auto z = groundZ + formation_flt_858624;
        if (z < origin.z) {
            z = origin.z;
        }
        const CVector pos{ x + origin.x, y + origin.y, z };
        if (i == 0) {
            firstPos = pos;
        }

        const auto model     = CGangs::ChooseGangPedModel((eGangID)(type - PED_TYPE_GANG1));
        const auto modelInfo = CModelInfo::ms_modelInfoPtrs[(int32)model];
        if (!modelInfo || !modelInfo->GetRwObject()) {
            continue;
        }

        CEntity* found[9]{};
        const auto searchRadius = modelInfo->GetColModel()->GetBoundRadius();
        CPedPlacement::IsPositionClearForPed(pos, searchRadius, 9, found, true, true, true);

        bool strangerNearby{};
        for (auto j = 0; j < 9; j++) {
            if (!found[j]) {
                continue;
            }
            bool ours{};
            for (auto k = 0; k < numPlaced; k++) {
                if (found[j] == peds[k]) {
                    ours = true;
                    break;
                }
            }
            if (!ours) {
                strangerNearby = true;
                break;
            }
        }

        const auto losOk = (numPlaced == 0) || CWorld::GetIsLineOfSightClear(pos, firstPos, true, false, false, false);
        const auto zOk   = (numPlaced == 0) || std::fabs(pos.z - firstPos.z) < formation_flt_858624;

        if (strangerNearby || !losOk || !zOk) {
            continue;
        }

        const auto ped = CPopulation::AddPed(type, model, pos, false);
        if (!ped) {
            CPopulation::RemovePed(nullptr); // 0x610F20(0) - как в оригинале
            continue;
        }
        peds[numPlaced] = ped;
        numPlaced++;
        const auto heading = CGeneral::GetRadianAngleBetweenPoints(origin.x, origin.y, pos.x, pos.y);
        ped->m_fCurrentRotation = heading; // ped+0x558
        ped->m_fAimingRotation  = heading; // ped+0x55C
        CVisibilityPlugins::SetClumpAlpha(ped->GetRpClump(), 0);
    }

    if (numPlaced > 0) {
        auto& group = CPedGroups::GetGroup(groupId);
        group.GetIntelligence().SetDefaultTaskAllocator(CPedGroupDefaultTaskAllocators::Get(ePedGroupDefaultTaskAllocatorType::RANDOM)); // 0x5FB280, 0xC09918
        for (auto i = 0; i < numPlaced; i++) {
            const auto member = peds[i];
            if (i == 0) {
                group.GetMembership().SetLeader(member); // 0x5FB9C0
            } else {
                group.GetMembership().AddFollower(member); // 0x5F8020
            }
            group.GetMembership().Process();
            group.GetIntelligence().Process();

            // Wander-таск: dir = (uint8)((rand & 0xFFFF) * (1/32768) * 8.0f) (fild/fmul 0x858B14,
            // fmul 0x859000, ftol 0x821B40) => 0..7; moveState=4 (PEDMOVE_WALK); scanTime=5000 (0x1388)
            CTask* wanderTask{};
            if (const auto t = new CTaskComplexWanderGang{
                    PEDMOVE_WALK,                                                                                       // 4
                    (uint8)((float)CGeneral::GetRandomNumber() * random_flt_858B14 * random_flt_859000),                // 0x821B1E (rand)
                    5000u,
                    true,                                                                                               // bWanderSensibly
                    random_flt_86FCB0                                                                                   // fTargetRadius (0x86FCB0 = 0.5f)
                }) {
                wanderTask = t; // 0x61A5A0(0x38) + 0x66F5C0
            }
            member->GetTaskManager().SetTask(wanderTask, TASK_PRIMARY_DEFAULT, false); // 0x681AF0, слот 4

            CTask* beInGroupTask{};
            if (const auto t = new CTaskComplexBeInGroup{ groupId, false }) { // даже лидер получает isLeader=false - так в оригинале (0x5FD76x push 0)
                beInGroupTask = t;
            }
            member->GetTaskManager().SetTask(beInGroupTask, TASK_PRIMARY_PRIMARY, false); // слот 3
        }
        return true;
    }

    for (auto i = 0; i < numPlaced; i++) {
        CPopulation::RemovePed(peds[i]);
    }
    return false;
}

// 0x5FD810
bool CPedGroupPlacer::PlaceGroup(ePedType type, uint32 numOfPeds, const CVector& origin, ePedGroupDefaultTaskAllocatorType allocType) {
    using enum ePedGroupDefaultTaskAllocatorType;
    switch (allocType) {
    case FOLLOW_ANY_MEANS:
    case FOLLOW_LIMITED:   return PlaceFormationGroup(type, numOfPeds, origin, allocType);
    case STAND_STILL:
    case CHAT:             return PlaceChatGroup(type, numOfPeds, origin, allocType);
    case RANDOM:           return PlaceRandomGroup(type, numOfPeds, origin, allocType);
    default:               NOTSA_UNREACHABLE();
    }
}
