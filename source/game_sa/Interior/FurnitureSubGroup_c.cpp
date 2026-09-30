#include "StdInc.h"
#include "FurnitureSubGroup_c.h"

#include "Furniture_c.h"
#include "ModelInfo.h"

// Furniture item store shared by all sub groups (256 preallocated items, stride 0x14)
static auto& g_furnitureStore = StaticRef<std::array<Furniture_c, 0x100>>(0xBAE1F8); // g_aFurnitures
static auto& g_currFurnitureId = StaticRef<uint32>(0xBAB378); // FurnitureManager_c::g_currFurnitureId

// 0x858B8C
static constexpr float FURNITURE_SIZE_FUDGE = 0.5f;
// 0x858B38
static constexpr float FURNITURE_FRAC_EPSILON = 0.02f;

void FurnitureSubGroup_c::InjectHooks() {
    RH_ScopedClass(FurnitureSubGroup_c);
    RH_ScopedCategory("Interior");

    RH_ScopedInstall(GetFurniture, 0x590EE0);
    RH_ScopedInstall(GetRandomId, 0x590FD0);
    RH_ScopedInstall(AddFurniture, 0x5C00C0);
}

// 0x5C00C0
bool FurnitureSubGroup_c::AddFurniture(uint16 modelId, int16 id, uint8 wealthMin, uint8 wealthMax, uint8 maxAng) {
    if (g_currFurnitureId >= 0x100) { // 256 items max
        return false;
    }

    const auto idx  = static_cast<uint16>(g_currFurnitureId++);
    auto& item      = g_furnitureStore[idx];

    item.m_nModelId = modelId; // +0x8
    item.m_nId      = id;      // +0xA

    // Determine the furniture size (in tiles) based on the model's collision bounding box
    const auto colModel = CModelInfo::ms_modelInfoPtrs[modelId]->m_pColModel; // *(mi + 0x14)
    const float widthX = m_bCanSteal
        ? colModel->m_boundBox.m_vecMax.x - colModel->m_boundBox.m_vecMin.x
        : colModel->m_boundBox.m_vecMax.x + FURNITURE_SIZE_FUDGE;
    const float widthY = m_bCanSteal
        ? colModel->m_boundBox.m_vecMax.y - colModel->m_boundBox.m_vecMin.y
        : colModel->m_boundBox.m_vecMax.y + FURNITURE_SIZE_FUDGE;

    // Truncate, but nudge the value up when the fractional part is nearly zero (< 0.02f) -
    // the original code compensates float inaccuracy this way (see 0x5C01A3 / 0x5C01CE)
    const auto TruncateWithEpsilon = [](float value) {
        auto truncated = static_cast<int32>(value);
        if (value - static_cast<float>(truncated) < FURNITURE_FRAC_EPSILON) {
            truncated++;
        }
        return truncated;
    };

    item.m_nWidthX = static_cast<int8>(TruncateWithEpsilon(widthX));  // +0xC
    item.m_nWidthY = static_cast<int8>(TruncateWithEpsilon(widthY));  // +0xD
    item.m_nWealthMin = wealthMin;                                    // +0xE
    item.m_nWealthMax = wealthMax;                                    // +0xF
    item.m_bCanPlaceInFrontOfWindow = m_bCanPlaceInFrontOfWindow;     // +0x10
    item.m_bIsTall                  = m_bIsTall;                      // +0x11
    item.m_bCanSteal                = m_bCanSteal;                    // +0x12
    item.m_nMaxAng                  = maxAng;                         // +0x13

    m_Furnitures.AddItem(&item); // 0x4A8DF0
    return true;
}

// 0x590EE0
Furniture_c* FurnitureSubGroup_c::GetFurniture(int16 id, uint8 wealth) {
    if (id >= 0) { // Find by explicit id
        for (auto it = m_Furnitures.GetHead(); it; it = it->m_pNext) {
            if (it->m_nId == id) {
                return it;
            }
        }
        return nullptr;
    }

    if (wealth == 0xFF) { // Totally random pick over all furniture
        const auto index = static_cast<int32>(
            static_cast<float>(std::rand() & 0xFFFF) * (1.0f / 32768.0f) * static_cast<float>(m_Furnitures.GetNumItems()) // 0x858B14 = 1/32768
        );
        return m_Furnitures.GetItemOffset(true, index); // 0x4A9010
    }

    // Pick random furniture whose wealth range contains `wealth`
    const auto numMatching = static_cast<int32>(std::ranges::count_if(m_Furnitures, [wealth](const Furniture_c& furniture) {
        return furniture.m_nWealthMin <= wealth && wealth <= furniture.m_nWealthMax;
    }));
    const auto index = static_cast<int32>(
        static_cast<float>(std::rand() & 0xFFFF) * (1.0f / 32768.0f) * static_cast<float>(numMatching)
    );

    int32 matching{};
    for (auto it = m_Furnitures.GetHead(); it; it = it->m_pNext) {
        if (it->m_nWealthMin <= wealth && wealth <= it->m_nWealthMax) {
            if (matching == index) {
                return it;
            }
            matching++;
        }
    }
    return nullptr;
}

// 0x590FD0
int32 FurnitureSubGroup_c::GetRandomId(uint8 wealth) {
    if (wealth == 0xFF) { // Totally random pick over all furniture
        const auto index = static_cast<int32>(
            static_cast<float>(std::rand() & 0xFFFF) * (1.0f / 32768.0f) * static_cast<float>(m_Furnitures.GetNumItems()) // 0x858B14 = 1/32768
        );
        const auto item = m_Furnitures.GetItemOffset(true, index); // 0x4A9010
        return item->m_nId; // Not checked for null in the original - the list is never empty here
    }

    const auto numMatching = static_cast<int32>(std::ranges::count_if(m_Furnitures, [wealth](const Furniture_c& furniture) {
        return furniture.m_nWealthMin <= wealth && wealth <= furniture.m_nWealthMax;
    }));
    const auto index = static_cast<int32>(
        static_cast<float>(std::rand() & 0xFFFF) * (1.0f / 32768.0f) * static_cast<float>(numMatching)
    );

    int32 matching{};
    for (auto it = m_Furnitures.GetHead(); it; it = it->m_pNext) {
        if (it->m_nWealthMin <= wealth && wealth <= it->m_nWealthMax) {
            if (matching == index) {
                return it->m_nId;
            }
            matching++;
        }
    }
    return -1;
}

void FurnitureSubGroup_c::Exit() {
    m_Furnitures.RemoveAll();
}
