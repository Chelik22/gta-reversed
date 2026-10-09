/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
// ============================================================================
// INTEGRATED FILE — source/game_sa/Interior/FurnitureSubGroup_c.cpp
// Батч: w2-45-FurnitureSubGroup_c (3 функции)
//
// Источник тел (фрагменты):
//   D:\projects\keep\batch_out\fresh\0x590EE0.cpp  -> GetFurniture
//   D:\projects\keep\batch_out\fresh\0x590FD0.cpp  -> GetRandomId
//   D:\projects\keep\batch_out\fresh\0x5C00C0.cpp  -> AddFurniture
//
// ИЗМЕНЕНИЯ относительно репо-версии:
//   - InjectHooks: сняты {.Reversed = false} со ВСЕХ трёх функций
//     (GetFurniture 0x590EE0, GetRandomId 0x590FD0, AddFurniture 0x5C00C0) —
//     теперь хукается оригинал, redirect-стабы plugin::CallMethodAndReturn
//     заменены реальными телами.
//   - FurnitureSubGroup_c::Exit() — реальное тело из репо, не тронуто
//     (в хуках не участвует).
//
// РАЗРЕШЁННЫЕ ЗАВИСИМОСТИ (проверены по репо):
//   - Furniture_c.h            — полный тип (FurnitureSubGroup_c.h держит
//     только forward-декларацию `class Furniture_c`); поля
//     m_nModelId/m_nId/m_nWidthX/m_nWidthY/m_nWealthMin/m_nWealthMax/
//     m_bCanPlaceInFrontOfWindow/m_bIsTall/m_bCanSteal/m_nMaxAng — ок
//     (source/game_sa/Interior/Furniture_c.h:7-16).
//   - TList_c::AddItem/GetHead/GetNext/GetNumItems/GetItemOffset — ок
//     (source/game_sa/Core/List_c.h:86, 367-372, 251).
//   - CModelInfo::GetModelInfo(int32)  — Models/ModelInfo.h:92.
//   - CBaseModelInfo::m_pColModel      — Models/BaseModelInfo.h:135 (смещ. 0x14).
//   - CColModel::m_boundBox (CBoundingBox : CBox { m_vecMin, m_vecMax })
//     — Collision/ColModel.h:15, Collision/Box.h:12-14.
//   - StaticRef<T>(addr)               — source/Base.h:164 (возвращает T&,
//     работает и для массива Furniture_c[0x100]).
//
// ЗАМЕТКИ ПО ГЛОБАЛАМ (из декомпила 0x5C00C0):
//   - 0xBAB378 — счётчик пула фурнитуры; в FurnitureManager_c.h:14 он объявлен
//     как uint32 (g_currFurnitureId), но оригинальный код работает с ним как
//     uint16 (сравнение `>= 0x100`). Здесь локальный StaticRef<uint16> —
//     это алиас той же памяти, конфликта нет (у менеджера другой адрес,
//     0xBAB380, см. FurnitureManager_c.h:40).
//   - 0xBAE1F8 — общий пул Furniture_c[0x100] (stride 0x14 = sizeof(Furniture_c),
//     поля item'а с базой 0xBAE1F8: +0x8 model, +0xA id, ... +0x13 maxAng —
//     сходится с Furniture_c.h). В репо больше нигде не смаплен.
//   - 0x858B8C = 0.5f, 0x858B38 = 0.02f (.rdata-константы).
//   - 0x4A8DF0 — TList_c<T>::AddItem (вставка в голову) — заменён репо-методом.
//   - Кодоген ftol (FUN_00821B40 = _ftol2) воспроизведён дословно по фрагменту.
// ============================================================================

#include "StdInc.h"
#include "FurnitureSubGroup_c.h"

#include "Furniture_c.h"          // Furniture_c - полный тип (форвард в FurnitureSubGroup_c.h)
#include "Models/ModelInfo.h"     // CModelInfo::GetModelInfo
#include "Collision/ColModel.h"   // CColModel::m_boundBox

void FurnitureSubGroup_c::InjectHooks() {
    RH_ScopedClass(FurnitureSubGroup_c);
    RH_ScopedCategory("Interior");

    RH_ScopedInstall(GetFurniture, 0x590EE0);
    RH_ScopedInstall(GetRandomId, 0x590FD0);
    RH_ScopedInstall(AddFurniture, 0x5C00C0);
}

// 0x5C00C0
bool FurnitureSubGroup_c::AddFurniture(uint16 modelId, int16 id, uint8 wealthMin, uint8 wealthMax, uint8 maxAng) {
    static auto& g_nNumFurnitures = StaticRef<uint16>(0xBAB378); // счётчик пула; FurnitureManager_c.h объявляет его как uint32 (g_currFurnitureId) — оригинал работает с ним как с uint16!
    static auto& g_Furnitures     = StaticRef<Furniture_c[0x100]>(0xBAE1F8); // общий пул фурнитуры (сразу после g_subGroupStore)

    if (g_nNumFurnitures >= 0x100) {
        return false;
    }

    auto* const furniture = &g_Furnitures[g_nNumFurnitures++];

    furniture->m_nModelId = modelId;
    furniture->m_nId      = id;

    // Размеры из bbox кол-модели (CBaseModelInfo::m_pColModel @ +0x14):
    // если фурнитуру можно украсть - полный размер (max-min), иначе bbox от угла (max + 0.5f)
    const auto* const colModel = CModelInfo::GetModelInfo(modelId)->m_pColModel;
    const auto&       bb       = colModel->m_boundBox;
    float widthX, widthY;
    if (!m_bCanSteal) {
        widthX = bb.m_vecMax.x + 0.5f /* 0x858B8C */;
        widthY = bb.m_vecMax.y + 0.5f /* 0x858B8C */;
    } else {
        widthX = bb.m_vecMax.x - bb.m_vecMin.x;
        widthY = bb.m_vecMax.y - bb.m_vecMin.y;
    }
    // Кодоген оригинала: __ftol2 (trunc) + досборка с порогом 0.02f (0x858B38):
    //   widthX: iX + ((float)iX < 0.02f)   — сравнивается само fild-значение
    //   widthY: iY + (widthY - iY < 0.02f) — сравнивается остаток
    const auto iX = (int32)widthX;
    furniture->m_nWidthX = (int8)(iX + ((float)iX < 0.02f ? 1 : 0));
    const auto iY = (int32)widthY;
    furniture->m_nWidthY = (int8)(iY + (widthY - (float)iY < 0.02f ? 1 : 0));

    furniture->m_nWealthMin = wealthMin;
    furniture->m_nWealthMax = wealthMax;
    furniture->m_nMaxAng    = maxAng;

    furniture->m_bCanPlaceInFrontOfWindow = m_bCanPlaceInFrontOfWindow;
    furniture->m_bIsTall                  = m_bIsTall;
    furniture->m_bCanSteal                = m_bCanSteal;

    m_Furnitures.AddItem(furniture); // 0x4A8DF0 (вставка в голову), см. TList_c::AddItem
    return true;
}

// 0x590EE0
Furniture_c* FurnitureSubGroup_c::GetFurniture(int16 id, uint8 wealth) {
    if (id > -1) {
        for (auto* furniture = m_Furnitures.GetHead(); furniture; furniture = m_Furnitures.GetNext(furniture)) {
            if (furniture->m_nId == id) {
                return furniture;
            }
        }
        return nullptr;
    }

    if (wealth == 0xFF) { // случайный из всех
        const auto numItems = m_Furnitures.GetNumItems(); // 0x4A8EC0
        const auto rnd      = int32((rand() & 0xFFFF) * (1.0f / 32768.0f) /* 0x858B14 */ * (float)numItems); // __ftol2
        return m_Furnitures.GetItemOffset(true, rnd); // 0x4A9010; может вернуть nullptr, если rnd >= numItems (как в оригинале)
    }

    // Сколько фурнитуры попадает в диапазон богатства
    int32 numInWealthRange = 0;
    for (auto* furniture = m_Furnitures.GetHead(); furniture; furniture = m_Furnitures.GetNext(furniture)) {
        if (furniture->m_nWealthMin <= wealth && wealth <= furniture->m_nWealthMax) {
            numInWealthRange++;
        }
    }

    const auto rnd = int32((rand() & 0xFFFF) * (1.0f / 32768.0f) /* 0x858B14 */ * (float)numInWealthRange); // __ftol2

    int32 i = 0;
    for (auto* furniture = m_Furnitures.GetHead(); furniture; furniture = m_Furnitures.GetNext(furniture)) {
        if (furniture->m_nWealthMin <= wealth && wealth <= furniture->m_nWealthMax) {
            if (i == rnd) {
                return furniture;
            }
            i++;
        }
    }
    return nullptr;
}

// 0x590FD0
int32 FurnitureSubGroup_c::GetRandomId(uint8 wealth) {
    if (wealth == 0xFF) { // случайный из всех
        const auto numItems = m_Furnitures.GetNumItems(); // 0x4A8EC0
        const auto rnd      = int32((rand() & 0xFFFF) * (1.0f / 32768.0f) /* 0x858B14 */ * (float)numItems); // __ftol2
        return m_Furnitures.GetItemOffset(true, rnd)->m_nId; // 0x4A9010; разыменование без null-чека - как в оригинале
    }

    // Сколько фурнитуры попадает в диапазон богатства
    int32 numInWealthRange = 0;
    for (auto* furniture = m_Furnitures.GetHead(); furniture; furniture = m_Furnitures.GetNext(furniture)) {
        if (furniture->m_nWealthMin <= wealth && wealth <= furniture->m_nWealthMax) {
            numInWealthRange++;
        }
    }

    const auto rnd = int32((rand() & 0xFFFF) * (1.0f / 32768.0f) /* 0x858B14 */ * (float)numInWealthRange); // __ftol2

    int32 i = 0;
    for (auto* furniture = m_Furnitures.GetHead(); furniture; furniture = m_Furnitures.GetNext(furniture)) {
        if (furniture->m_nWealthMin <= wealth && wealth <= furniture->m_nWealthMax) {
            if (i == rnd) {
                return furniture->m_nId;
            }
            i++;
        }
    }
    return -1;
}

void FurnitureSubGroup_c::Exit() {
    m_Furnitures.RemoveAll();
}
