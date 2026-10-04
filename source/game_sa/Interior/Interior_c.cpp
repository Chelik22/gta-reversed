#include "StdInc.h"

#include "Interior_c.h"

#include "FurnitureManager_c.h"
#include "Furniture_c.h"
#include "InteriorManager_c.h"
#include "InteriorObject.h"

#include <TaskTypes/Interior/TaskInteriorBeInHouse.h>
#include <TaskTypes/Interior/TaskInteriorBeInOffice.h>
#include <TaskTypes/Interior/TaskInteriorBeInShop.h>
#include <TaskTypes/Interior/TaskInteriorShopKeeper.h>

// Helpers of the FurnitureEntity_c slot pool (not reversed in the repo yet)
static uint32 NumFreeFurnitureEntitySlots() {
    return plugin::CallAndReturn<uint32, 0x4A8EC0>();
}

static FurnitureEntity_c* TakeFurnitureEntitySlot() {
    return plugin::CallAndReturn<FurnitureEntity_c*, 0x4A8E70>();
}

static void CommitFurnitureEntitySlot(FurnitureEntity_c* slot) {
    plugin::Call<0x4A8DF0>(slot);
}

static void DiscardFurnitureEntitySlot(FurnitureEntity_c* slot) {
    plugin::Call<0x4A8E30>(slot);
}

//! Tile size of the interior grid (0x858B8C)
static constexpr float TILE_SIZE = 1.0f;
//! Half of the above (0x858CE8)
static constexpr float TILE_HALF = 0.5f;
//! Degrees to radians (0x8595EC)
static constexpr float INT_DEG_TO_RAD = 0.017453292f;
//! PI / 2 (0x858FE4)
static constexpr float PI_HALF = 1.5707963f;

//! Bounding box margin around an interior (0x8D22B0)
static auto& BBOX_MARGIN = StaticRef<float>(0x8D22B0);

//! Counter controlling how often shelf infos are added (0x8D0948)
static auto& g_ShelfInfoCounter = StaticRef<int32>(0x8D0948);
//! Random shop furniture-style value (0..99), picked per shop (0xBB3DE4)
static auto& g_ShopStyleRand = StaticRef<int32>(0xBB3DE4);
//! Time (ms) of the last pickup spawn (0xBB3DC4)
static auto& s_lastPickupTime = StaticRef<uint32>(0xBB3DC4);
//! Whether the office filler prop already got placed (0xBB3DC8)
static auto& bPlacedFiller = StaticRef<bool>(0xBB3DC8);

//! Goto point entry (Interior_c+0x410, stride 0x10, count: Interior_c::field_40C)
struct GotoPt_t {
    int8    m_tileX;
    int8    m_tileY;
    uint16  _pad;
    CVector m_pos;
};
static_assert(sizeof(GotoPt_t) == 0x10);

//! Exit point infos stored at Interior_c+0x512, stride 0x20, one per wall (L, R, T, B)
#pragma pack(push, 1)
struct ExitPt_t {
    int8_t  tileIdx1;
    int8_t  tileIdx2;
    CVector pos;
    CVector outPos;
    uint8_t _pad[6]; // stride 0x20, as in the original
};
static_assert(sizeof(ExitPt_t) == 0x20);
#pragma pack(pop)

void Interior_c::InjectHooks() {
    RH_ScopedClass(Interior_c);
    RH_ScopedCategory("Interior");

    //RH_ScopedInstall(Constructor, 0x5921D0, { .reversed = false });
    //RH_ScopedInstall(Destructor, 0x591360, { .reversed = false });

    RH_ScopedInstall(Bedroom_AddTableItem, 0x593F10);
    RH_ScopedInstall(FurnishBedroom, 0x593FC0);
    RH_ScopedInstall(Kitchen_FurnishEdges, 0x596930);
    RH_ScopedInstall(FurnishKitchen, 0x5970B0);
    RH_ScopedInstall(Lounge_AddTV, 0x597240);
    RH_ScopedInstall(Lounge_AddHifi, 0x597430);
    RH_ScopedInstall(Lounge_AddChairInfo, 0x5974E0);
    RH_ScopedInstall(Lounge_AddSofaInfo, 0x5975C0);
    RH_ScopedInstall(FurnishLounge, 0x597740);
    RH_ScopedInstall(Office_PlaceEdgeFillers, 0x599210);
    RH_ScopedInstall(Office_PlaceDesk, 0x5993E0);
    RH_ScopedInstall(Office_PlaceEdgeDesks, 0x5995B0);
    RH_ScopedInstall(Office_FurnishEdges, 0x599770);
    RH_ScopedInstall(Office_PlaceDeskQuad, 0x599960);
    RH_ScopedInstall(Office_FurnishCenter, 0x599A30);
    RH_ScopedInstall(FurnishOffice, 0x599AF0);
    RH_ScopedInstall(Shop_Place3PieceUnit, 0x599BB0);
    RH_ScopedInstall(Shop_PlaceEdgeUnits, 0x599DC0);
    RH_ScopedInstall(Shop_PlaceCounter, 0x599EF0);
    RH_ScopedInstall(Shop_PlaceFixedUnits, 0x59A030);
    RH_ScopedInstall(Shop_FurnishCeiling, 0x59A130);
    RH_ScopedInstall(Shop_AddShelfInfo, 0x59A140);
    RH_ScopedInstall(Shop_FurnishEdges, 0x59A1B0);
    RH_ScopedInstall(GetBoundingBox, 0x593DB0);
    RH_ScopedInstall(Init, 0x593BF0);
    RH_ScopedInstall(ResetTiles, 0x593910);
    RH_ScopedInstall(PlaceObject, 0x5934E0);
    RH_ScopedInstall(GetFurnitureEntity, 0x5913B0);
    RH_ScopedInstall(IsPtInside, 0x5913E0);
    RH_ScopedInstall(CalcMatrix, 0x5914D0);
    RH_ScopedInstall(Furnish, 0x591590);
    RH_ScopedInstall(Unfurnish, 0x5915D0);
    RH_ScopedInstall(CheckTilesEmpty, 0x591680);
    RH_ScopedInstall(SetTilesStatus, 0x591700);
    RH_ScopedInstall(SetCornerTiles, 0x5917C0);
    RH_ScopedInstall(GetTileStatus, 0x5918E0);
    RH_ScopedInstall(GetNumEmptyTiles, 0x591920);
    RH_ScopedInstall(GetRandomTile, 0x591B20);
    RH_ScopedInstall(Shop_FurnishAisles, 0x59A590);
    RH_ScopedInstall(GetTileCentre, 0x591BD0);
    RH_ScopedInstall(AddGotoPt, 0x591D20);
    RH_ScopedInstall(AddInteriorInfo, 0x591E40);
    RH_ScopedInstall(AddPickups, 0x591F90);
    RH_ScopedInstall(Exit, 0x592230);
    RH_ScopedInstall(FindBoundingBox, 0x5922C0);
    RH_ScopedInstall(CalcExitPts, 0x5924A0);
    RH_ScopedInstall(IsVisible, 0x5929F0);
    RH_ScopedInstall(PlaceFurniture, 0x592AA0);
    RH_ScopedInstall(PlaceFurnitureOnWall, 0x593120);
    RH_ScopedInstall(PlaceFurnitureInCorner, 0x593340);
    RH_ScopedInstall(FindEmptyTiles, 0x591C50);
    RH_ScopedInstall(FurnishShop, 0x59A790);
}

// 0x593BF0
int32 Interior_c::Init(const CVector& pos) {
    CalcMatrix(const_cast<CVector*>(&pos));
    ResetTiles();

    // Fetch the furniture "holder" and make sure its furniture (at +0x14) is loaded
    const auto holder = plugin::CallMethodAndReturn<uintptr_t, 0x594BD0>(this);
    auto*      furn   = *reinterpret_cast<Furniture_c**>(holder + 0x14);
    if (!furn) {
        plugin::Call<0x54F560>();
        plugin::Call<0x54F1B0>(furn);
    }
    plugin::Call<0x59BCF0>(furn);

    static auto& g_noRandomizeSeeds = StaticRef<int32>(0xBB3DAC);
    if (m_box->m_type != 99 /* 'c' */) {
        int32 seed;
        if (g_noRandomizeSeeds == 0) {
            seed = CGeneral::GetRandomNumber();
        } else {
            const auto a = CGeneral::GetRandomNumber();
            const auto b = CGeneral::GetRandomNumber();
            const auto c = CGeneral::GetRandomNumber();
            const auto d = CGeneral::GetRandomNumber();
            const auto e = CGeneral::GetRandomNumber();
            const auto f = CGeneral::GetRandomNumber();
            seed       = a * b * c + d * e * f + m_box->m_seed;
        }
        plugin::Call<0x821B11, int32>(seed); // srand(seed)
    }

    field_40C            = 0;
    m_interiorInfosCount = 0;

    switch (m_box->m_type) {
    case 0: FurnishShop(0); break;
    case 1: FurnishOffice(); break;
    case 2: FurnishLounge(); break;
    case 3: FurnishBedroom(); break;
    case 4: FurnishKitchen(); break;
    }

    CalcExitPts();

    static auto& g_numUsedInteriorIds = StaticRef<int32>(0xBB3914);
    static auto& g_usedInteriorIds    = StaticRef<std::array<int32, 0x40>>(0xBB3918);
    if (g_interiorMan.HasInteriorHadStealDataSetup(this) == false && g_numUsedInteriorIds < 0x40) {
        g_usedInteriorIds[g_numUsedInteriorIds] = m_interiorId;
        g_numUsedInteriorIds++;
    }

    if (m_box->m_type == 2 || m_box->m_type == 3) {
        AddPickups();
    }

    plugin::Call<0x59ACD0>(); // NOTE: not reversed yet
    return 1;
}

// 0x592230
void Interior_c::Exit() {
    static auto& kRemoveMargin = StaticRef<float>(0x858B40); // Bounding box margin around the interior

    CPickups::RemovePickUpsInArea(
        m_matrix.pos.x - kRemoveMargin, m_matrix.pos.x + kRemoveMargin,
        m_matrix.pos.y - kRemoveMargin, m_matrix.pos.y + kRemoveMargin,
        m_matrix.pos.z - kRemoveMargin, m_matrix.pos.z + kRemoveMargin
    );

    Unfurnish();
}

// 0x593F10
CObject* Interior_c::Bedroom_AddTableItem(int32 a2, int32 a3, int32 a4, int32 a5, int32 a6, int32 a7) {
    static auto& kTileSize = StaticRef<float>(0x858B8C); // Tile size
    static auto& kDegToRad = StaticRef<float>(0x85991C); // Degrees to radians (pi / 180)

    float x = static_cast<float>(a5);
    float y = static_cast<float>(a6);

    // Shift the item so it fits onto the table depending on which edge it is placed on
    if (a4 == 0 || a4 == 2) {
        x += kTileSize;
    } else if (a4 == 1 || a4 == 3) {
        y += kTileSize;
    }

    const auto furniture = g_furnitureMan.GetFurniture(a2, a3, -1, m_box->m_status); // 0x5911E0
    return PlaceObject(true, furniture, x + kTileSize, y + kTileSize, 0.5f, static_cast<float>(a7) * kDegToRad);
}

// 0x593FC0
void Interior_c::FurnishBedroom() {
    m_furnitureId = (int8)g_furnitureMan.GetRandomId(3, 1, m_box->m_seed);
    SetTilesStatus(m_box->m_door - 1, 0, 2, 2, 7, 0);

    // Bed against a wall
    int32 bedX, bedY;
    PlaceFurnitureOnWall(3, 0, -1, 0.f, 1, -1, -1, 0, &bedX, &bedY, nullptr, nullptr, nullptr, nullptr);

    // Nightstands beside the bed
    CEntity* nightstandEnt{};
    if (bedY > 0 && (nightstandEnt = PlaceFurnitureOnWall(3, 1, m_furnitureId, 0.f, 1, bedX, bedY - 1, 0,
                                                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr))) {
        int32 offX, offY, action, dir, tileX, tileY;
        switch (bedX) {
        case 1:
            offX = 1; offY = bedY - 1; action = 3; dir = 2; tileX = 2; tileY = bedY - 1;
            break;
        case 3:
            offX = m_box->m_width - 2; offY = bedY - 1; action = 4; dir = 2;
            tileX = m_box->m_width - 3; tileY = bedY - 1;
            break;
        case 0:
            offX = bedY - 1; offY = m_box->m_depth - 2; action = 3; dir = 1;
            tileX = bedY - 1; tileY = m_box->m_depth - 3;
            break;
        case 2:
            offX = bedY - 1; offY = 1; action = 4; dir = 1; tileX = bedY - 1; tileY = 2;
            break;
        }
        AddInteriorInfo(action, (float)offX, (float)offY, dir, nightstandEnt);
        SetTilesStatus(tileX, tileY, 1, 1, 2, 0);
    }

    if ((nightstandEnt = PlaceFurnitureOnWall(3, 1, m_furnitureId, 0.f, 1, bedX, bedY + 2, 0,
                                              nullptr, nullptr, nullptr, nullptr, nullptr, nullptr))) {
        int32 offX, offY, action, dir, tileX, tileY;
        switch (bedX) {
        case 1:
            offX = 1; offY = bedY + 2; action = 4; dir = 0; tileX = 2; tileY = bedY + 2;
            break;
        case 3:
            offX = m_box->m_width - 2; offY = bedY + 2; action = 3; dir = 0;
            tileX = m_box->m_width - 3; tileY = bedY + 2;
            break;
        case 0:
            offX = bedY + 2; offY = m_box->m_depth - 2; action = 4; dir = 3;
            tileX = bedY + 2; tileY = m_box->m_depth - 3;
            break;
        case 2:
            offX = bedY + 2; offY = 1; action = 3; dir = 3; tileX = bedY + 2; tileY = 2;
            break;
        }
        AddInteriorInfo(action, (float)offX, (float)offY, dir, nightstandEnt);
        SetTilesStatus(tileX, tileY, 1, 1, 2, 0);
    }

    // Wardrobe + dresser
    PlaceFurnitureOnWall(3, 3, m_furnitureId, 0.f, 1, -1, -1, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    PlaceFurnitureOnWall(3, 2, m_furnitureId, 0.f, 1, -1, -1, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    // Scatter probability threshold, based on the room's seed/wealth
    const auto seed = m_box->m_seed;
    int32 threshold;
    if (seed < 0x4B) {
        if (seed < 0x32) {
            threshold = CGeneral::GetRandomNumberInRange(0x32, 100);
        } else {
            threshold = 0x14 - CGeneral::GetRandomNumber();
        }
    } else {
        threshold = CGeneral::GetRandomNumber();
    }

    int32 tileX, tileY;
    if (CGeneral::GetRandomNumber() < threshold && FindEmptyTiles(2, 2, &tileX, &tileY)) {
        const auto furn = g_furnitureMan.GetFurniture(8, 2, -1, seed);
        PlaceObject(false, furn, (float)tileX + TILE_HALF, (float)tileY + TILE_HALF, 0.05f, 0.f);
        SetTilesStatus(tileX, tileY, 2, 2, 2, 0);
    }
    if (CGeneral::GetRandomNumber() < threshold && FindEmptyTiles(1, 1, &tileX, &tileY)) {
        const auto furn = g_furnitureMan.GetFurniture(8, 5, -1, seed);
        PlaceObject(false, furn, (float)tileX + TILE_HALF, (float)tileY + TILE_HALF, 0.05f, 0.f);
        SetTilesStatus(tileX, tileY, 1, 1, 2, 0);
    }
    if (CGeneral::GetRandomNumber() < threshold && FindEmptyTiles(1, 1, &tileX, &tileY)) {
        const auto furn = g_furnitureMan.GetFurniture(8, 4, -1, seed);
        PlaceObject(false, furn, (float)tileX + TILE_HALF, (float)tileY + TILE_HALF, 0.05f, 0.f);
        SetTilesStatus(tileX, tileY, 1, 1, 2, 0);
    }
    if (CGeneral::GetRandomNumber() < threshold && FindEmptyTiles(1, 1, &tileX, &tileY)) {
        const auto furn = g_furnitureMan.GetFurniture(8, 3, -1, seed);
        PlaceObject(false, furn, (float)tileX + TILE_HALF, (float)tileY + TILE_HALF, 0.05f, 0.f);
        SetTilesStatus(tileX, tileY, 1, 1, 2, 0);
    }
    if (CGeneral::GetRandomNumber() < threshold && FindEmptyTiles(2, 2, &tileX, &tileY)) {
        const auto furn = g_furnitureMan.GetFurniture(8, 6, -1, seed);
        PlaceObject(false, furn, (float)tileX + TILE_HALF, (float)tileY + TILE_HALF, 0.05f, 0.f);
        SetTilesStatus(tileX, tileY, 2, 2, 2, 0);
    }

    // Main furniture piece (bed) placed on a random free tile
    const auto furn = g_furnitureMan.GetFurniture(8, 1, -1, seed);
    int32 x, y;
    PlaceFurniture(furn, CGeneral::GetRandomNumber(), CGeneral::GetRandomNumber(), 0.f, 0, 0, &x, &y, 0);
}

// 0x596930
// NOTE: Header said `CObject*`, but the original returns nothing.
CObject* Interior_c::Kitchen_FurnishEdges() {
    constexpr float Z_OFFSET = 1.05f; // 0x3F866666

    const uint8  depth     = m_box->m_depth;
    const int32  lastTileX = m_box->m_width - 1;
    const int32  lastTileY = depth - 1;

    int32 dummyA, dummyB; // out params of PlaceFurniture we don't care about

    // Counter + shelves in the back-left / back-right corner
    auto* cornerFurn          = g_furnitureMan.GetFurniture(4, 7, m_furnitureId, m_box->m_status);
    const auto placedCounter  = PlaceFurniture(cornerFurn, 0, lastTileY, 0.0f, 1, 1, &dummyA, &dummyB, 0);
    const auto placedShelfR   = PlaceFurniture(cornerFurn, lastTileX, lastTileY, 0.0f, 1, 0, &dummyA, &dummyB, 0);

    // Amount of free tiles on the left/right wall (up to the first door/window)
    int32 freeL = std::max<int32>(m_box->m_lDoorEnd, m_box->m_lWindowEnd);
    freeL = freeL < 1 ? 0 : freeL;
    int32 freeR = std::max<int32>(m_box->m_rDoorEnd, m_box->m_rWindowEnd);
    freeR = freeR < 1 ? 0 : freeR;

    if (freeR < 1) {
        if (freeL < 1) {
            freeL = CGeneral::GetRandomNumberInRange(0, lastTileY);
            freeR = CGeneral::GetRandomNumberInRange(0, lastTileY);
        } else {
            freeR = 0;
        }
    }
    if (freeL > 0) {
        SetTilesStatus(0, 0, 1, freeL, 2, 0);
    }
    if (freeR > 0) {
        SetTilesStatus(lastTileX, 0, 1, freeR, 2, 0);
    }

    // Left and right counters
    PlaceFurniture(g_furnitureMan.GetFurniture(4, 0, m_furnitureId, m_box->m_status), 0, freeL, 0.0f, 1, 1, &dummyA, &dummyB, 0);
    PlaceFurniture(g_furnitureMan.GetFurniture(4, 2, m_furnitureId, m_box->m_status), lastTileX, freeR, 0.0f, 1, 3, &dummyA, &dummyB, 0);

    // Scratch buffers shared by all furniture placement below
    float xCoords[33]{};   // stored at [i + 1]!
    float yCoords[33]{};   // stored at [i + 1]!
    float rotations[32]{}; // float rotation angles
    int32 numItems = 0;

    // Wall units + a corresponding interior info entry (a goto point in front of them)
    int32 heightOut;
    if (PlaceFurnitureOnWall(4, 3, m_furnitureId, 0.0f, 1, 0, m_box->m_tWindowStart, 0, reinterpret_cast<int32*>(yCoords), &heightOut, nullptr, nullptr, nullptr, nullptr)) {
        heightOut++;
        AddInteriorInfo(5, (float)heightOut, (float)m_box->m_depth - TILE_HALF, 2, nullptr);
    }
    PlaceFurnitureOnWall(4, 5, m_furnitureId, 0.0f, 1, -1, -1, 0, reinterpret_cast<int32*>(yCoords), &heightOut, nullptr, nullptr, nullptr, nullptr);
    PlaceFurnitureOnWall(4, 4, m_furnitureId, 0.0f, 1, -1, -1, 0, reinterpret_cast<int32*>(yCoords), &heightOut, nullptr, nullptr, nullptr, nullptr);
    PlaceFurnitureOnWall(4, 6, m_furnitureId, 0.0f, 1, -1, -1, 0, reinterpret_cast<int32*>(yCoords), &heightOut, nullptr, nullptr, nullptr, nullptr);

    // Collect positions of all "aisle" furniture along the three free walls
    auto* aisleFurn = g_furnitureMan.GetFurniture(4, 1, m_furnitureId, m_box->m_status);

    // Left wall (between counter and last free tile)
    for (freeL++; freeL < lastTileY; freeL++) {
        if (PlaceFurniture(aisleFurn, 0, freeL, 0.0f, 1, 1, &dummyA, &dummyB, 0)) {
            xCoords[numItems + 1] = 0.5f;
            rotations[numItems]   = 90.0f; // 0x42B40000
            numItems++;
            yCoords[numItems] = (float)freeL + TILE_SIZE;
        }
    }

    // Back wall
    for (int32 x = 1; x < lastTileX; x++) {
        if (PlaceFurniture(aisleFurn, x, lastTileY, 0.0f, 1, 0, &dummyA, &dummyB, 0)) {
            rotations[numItems] = 0.0f;
            numItems++;
            xCoords[numItems] = (float)x + TILE_SIZE;
            yCoords[numItems] = (float)lastTileY + TILE_SIZE;
        }
    }

    // Right wall (up to 2 tiles before the end)
    for (freeR = freeR + 1; freeR <= depth - 2; freeR++) {
        if (PlaceFurniture(aisleFurn, lastTileX, freeR, 0.0f, 1, 3, &dummyA, &dummyB, 0)) {
            rotations[numItems] = 270.0f; // 0x43870000
            numItems++;
            xCoords[numItems] = (float)lastTileX + TILE_SIZE;
            yCoords[numItems] = (float)freeR + TILE_SIZE;
        }
    }

    // Fridge in one of the two back corners
    auto* fridgeFurn = g_furnitureMan.GetFurniture(4, 10, -1, m_box->m_status);
    const auto rnd = CGeneral::GetRandomNumber();
    if (rnd < 0x3FFF && placedCounter) {
        PlaceObject(1, fridgeFurn, 0.5f, (float)m_box->m_depth - TILE_SIZE, Z_OFFSET, 45.0f /* 0x42340000 */);
    } else if (placedShelfR) {
        PlaceObject(1, fridgeFurn, (float)m_box->m_width - TILE_SIZE, (float)m_box->m_depth - TILE_SIZE, Z_OFFSET, 315.0f /* 0x439D8000 */);
    }

    // Scatter random clutter / stealable items on top of the placed furniture
    if (numItems > 0) {
        float used[33]{}; // 0x858B50 == 1.0f

        const auto idx1 = CGeneral::GetRandomNumber();
        used[idx1] = 1.0f;

        auto idx2 = CGeneral::GetRandomNumber();
        int32 tries = 0;
        if (used[idx2] == 1.0f) {
            do {
                if (tries > 0x1D) {
                    break;
                }
                idx2 = CGeneral::GetRandomNumber();
                tries++;
            } while (used[idx2] == 1.0f);
            if (tries == 0x1E) {
                idx2 = -1;
            }
        }

        int32 stealChance;
        if (m_box->m_status < 0x4B) {
            if (m_box->m_status < 0x32) {
                stealChance = 0x32 - CGeneral::GetRandomNumber();
            } else {
                stealChance = 0x14 - CGeneral::GetRandomNumber();
            }
        } else {
            stealChance = CGeneral::GetRandomNumber();
        }

        for (int32 i = 0; i < numItems; i++) {
            if (used[i] == 1.0f) {
                if (CGeneral::GetRandomNumber() < stealChance) {
                    const auto subGroup = CGeneral::GetRandomNumber() < 0x3FFF ? 3 : 4;
                    if (auto* clutter = g_furnitureMan.GetFurniture(8, subGroup, -1, m_box->m_status)) {
                        PlaceObject(0, clutter, xCoords[i + 1], yCoords[i + 1], Z_OFFSET, rotations[i]);
                    }
                }
            }
        }

        if (idx1 != -1) {
            auto* hifi = g_furnitureMan.GetFurniture(4, 8, -1, m_box->m_status);
            PlaceObject(1, hifi, xCoords[idx1 + 1], yCoords[idx1 + 1], Z_OFFSET, rotations[idx1]);
        }
        if (idx2 != -1) {
            auto* tv = g_furnitureMan.GetFurniture(4, 9, -1, m_box->m_status);
            PlaceObject(1, tv, xCoords[idx2 + 1], yCoords[idx2 + 1], Z_OFFSET, rotations[idx2]);
        }
    }
    return nullptr;
}

// 0x5970B0
void Interior_c::FurnishKitchen() {
    SetTilesStatus(m_box->m_door - 1, 0, 2, 1, 7, 0);

    // Block out the interior border tiles (leaving the outermost ring)
    const auto width = m_box->m_width - 2;
    const auto depth = m_box->m_depth - 2;
    for (int32 x = 1; x <= (int32)width; x++) {
        SetTilesStatus(x, depth, 1, 1, 3, 0);
        SetTilesStatus(x, 0, 1, 1, 3, 0);
    }
    for (int32 y = 0; y <= (int32)depth; y++) {
        SetTilesStatus(1, y, 1, 1, 3, 0);
        SetTilesStatus(width, y, 1, 1, 3, 0);
    }

    AddGotoPt(1, 1, 0.f, 0.f);
    AddGotoPt(1, depth, 0.f, 0.f);
    AddGotoPt(width, 1, 0.f, 0.f);
    AddGotoPt(width, depth, 0.f, 0.f);

    m_furnitureId = (int8)g_furnitureMan.GetRandomId(4, 0, m_box->m_seed);
    Kitchen_FurnishEdges();

    const auto furn = g_furnitureMan.GetFurniture(8, 1, -1, m_box->m_seed);
    int32 x, y;
    PlaceFurniture(furn, CGeneral::GetRandomNumber(), CGeneral::GetRandomNumber(), 0.f, 0, 0, &x, &y, 0);
}

// 0x597240
// NOTE: decompilation shows only a2 is used (a3..a5 unused)
CObject* Interior_c::Lounge_AddTV(int32 a2, int32 a3, int32 a4, int32 a5) {
    float infoX, infoY; // AddInteriorInfo position
    float tvX, tvY;     // TV placement
    float hifiX, hifiY; // hifi/vcr placement

    if (a2 == 0) {
        infoX = 1.0f;
        infoY = (float)m_box->m_depth - 2.0f;
        tvX   = 0.5f;
        tvY   = (float)m_box->m_depth - 0.5f;
        hifiX = 1.5f;
        hifiY = tvY;
    } else if (a2 == 1) {
        infoX = (float)m_box->m_width - 2.0f;
        infoY = 1.0f;
        tvX   = (float)m_box->m_width - 0.5f;
        tvY   = 0.5f;
        hifiX = tvX - 1.0f;
        hifiY = 0.5f;
    } else if (a2 == 2) {
        infoX = 1.0f;
        infoY = 1.0f;
        tvX   = 0.5f;
        tvY   = 0.5f;
        hifiX = 0.5f;
        hifiY = 1.5f;
    } else {
        infoX = (float)m_box->m_width - 2.0f;
        infoY = (float)m_box->m_depth - 2.0f;
        tvX   = (float)m_box->m_width - 0.5f;
        tvY   = (float)m_box->m_depth - 0.5f;
        hifiX = tvX;
        hifiY = tvY - 1.0f;
    }

    // For directions 0..3 a "watch TV" info point is registered; the original
    // skips this (and only this) for any other value.
    if (a2 >= 0 && a2 <= 3) {
        AddInteriorInfo(0, infoX, infoY, -1, nullptr);
    }

    const float rot = (float)(a2 & 3) * 90.0f;

    const auto tvFurniture = g_furnitureMan.GetFurniture(2, 3, -1, m_box->m_seed);
    PlaceObject(1, tvFurniture, tvX, tvY, 0.5f, rot + 45.0f);

    // VCR (id 7) or game console (id 9), 50/50 (rand() range is 0..0x7FFF)
    const auto hifiFurniture = g_furnitureMan.GetFurniture(2, rand() < 0x3fff ? 7 : 9, -1, m_box->m_seed);
    PlaceObject(1, hifiFurniture, hifiX, hifiY, 0.5f, rot);

    return nullptr;
}

// 0x597430
CObject* Interior_c::Lounge_AddHifi(int32 a2, int32 a3, int32 a4, int32 a5) {
    // a2 = direction, a3 = tile coord (Y axis), a4 = tile coord (X axis), a5 unused
    float x = 0.0f;
    float y = 0.0f;
    if (a2 == 0 || a2 == 1) {
        y = (float)a3 + 0.5f;
        x = (float)a4;
    } else if (a2 == 2 || a2 == 3) {
        x = (float)a4 + 0.5f;
        y = (float)a3;
    }
    const auto furniture = g_furnitureMan.GetFurniture(2, 8, -1, m_box->m_seed);
    return PlaceObject(1, furniture, x + 0.5f, y + 0.5f, 0.5f, (float)(a2 & 3) * 90.0f);
}

// 0x5974E0
void Interior_c::Lounge_AddChairInfo(int32 a2, int32 a3, CEntity* entityIgnoredCollision) {
    // a2 = wall/direction, a3 = tile coord along that wall
    if (a2 == 0) {
        AddInteriorInfo(1, (float)a3 + 0.5f, (float)(m_box->m_depth - 1) - 1.0f, 2, entityIgnoredCollision);
    } else if (a2 == 2) {
        AddInteriorInfo(1, (float)a3 + 0.5f, 1.0f, 0, entityIgnoredCollision);
    } else if (a2 == 1) {
        AddInteriorInfo(1, 1.0f, (float)a3 + 0.5f, 3, entityIgnoredCollision);
    } else if (a2 == 3) {
        AddInteriorInfo(1, (float)(m_box->m_width - 1) - 1.0f, (float)a3 + 0.5f, 1, entityIgnoredCollision);
    }
}

// 0x5975C0
void Interior_c::Lounge_AddSofaInfo(int32 sitType, int32 offsetX, CEntity* entityIgnoredCollision) {
    // Two seat infos per call, spaced 1.0f apart (sofa width)
    if (sitType == 0) {
        const float x = (float)offsetX + 0.5f;
        const float y = (float)(m_box->m_depth - 1) - 1.0f;
        AddInteriorInfo(1, x, y, 2, entityIgnoredCollision);
        AddInteriorInfo(1, x + 1.0f, y, 2, entityIgnoredCollision);
    } else if (sitType == 2) {
        const float x = (float)offsetX + 0.5f;
        AddInteriorInfo(1, x, 1.0f, 0, entityIgnoredCollision);
        AddInteriorInfo(1, x + 1.0f, 1.0f, 0, entityIgnoredCollision);
    } else if (sitType == 1) {
        const float y = (float)offsetX + 0.5f;
        AddInteriorInfo(1, 1.0f, y, 3, entityIgnoredCollision);
        AddInteriorInfo(1, 1.0f, y + 1.0f, 3, entityIgnoredCollision);
    } else if (sitType == 3) {
        const float x = (float)(m_box->m_width - 1) - 1.0f;
        const float y = (float)offsetX + 0.5f;
        AddInteriorInfo(1, x, y, 1, entityIgnoredCollision);
        AddInteriorInfo(1, x, y + 1.0f, 1, entityIgnoredCollision);
    }
}

// 0x597740
void Interior_c::FurnishLounge() {
    const auto perimeter = (m_box->m_depth + m_box->m_width) * 2;

    SetTilesStatus(m_box->m_door - 1, 0, 2, 1, 7, 0);
    SetTilesStatus(m_box->m_door - 2, 0, 1, 1, 2, 0);
    SetTilesStatus(m_box->m_door + 1, 0, 1, 1, 2, 0);

    // Corner unit (returns nonzero when placed); remember which corner it took
    int32 cornerX, cornerY, cornerZ;
    int32 tvCorner = -1;
    if (PlaceFurnitureInCorner(2, 2, -1, 0.f, 1, -1, 0, &cornerX, &cornerY, &cornerZ, nullptr, nullptr)) {
        Lounge_AddTV(cornerX, cornerY, cornerZ, 0);
        tvCorner = cornerX;
    }
    SetCornerTiles(cornerX, 2, 2, 1);

    // Block out the border tiles
    const auto width = m_box->m_width - 2;
    const auto depth = m_box->m_depth - 2;
    for (int32 x = 1; x <= width; x++) {
        SetTilesStatus(x, depth, 1, 1, 3, 0);
        SetTilesStatus(x, 1, 1, 1, 3, 0);
    }
    for (int32 y = 1; y <= depth; y++) {
        SetTilesStatus(1, y, 1, 1, 3, 0);
        SetTilesStatus(width, y, 1, 1, 3, 0);
    }

    AddGotoPt(1, 1, 0.f, 0.f);
    AddGotoPt(1, depth, 0.f, 0.f);
    AddGotoPt(width, 1, 0.f, 0.f);
    AddGotoPt(width, depth, 0.f, 0.f);

    SetCornerTiles(0, 2, 2, 0);
    SetCornerTiles(2, 2, 2, 0);
    SetCornerTiles(1, 2, 2, 0);
    SetCornerTiles(3, 2, 2, 0);

    // Sofa against a wall
    m_furnitureId = (int8)g_furnitureMan.GetRandomId(2, 0, m_box->m_seed);
    int32 sofaX, sofaY;
    CEntity* sofaEnt{};
    if ((sofaEnt = PlaceFurnitureOnWall(2, 0, m_furnitureId, 0.f, 1, -1, -1, 0,
                                        &sofaX, &sofaY, nullptr, nullptr, nullptr, nullptr))) {
        Lounge_AddSofaInfo(sofaX, sofaY, sofaEnt);
        // Coffee table in front of the sofa (fall back to the other side orientation)
        if (!PlaceFurnitureOnWall(2, 4, -1, 0.f, 1, sofaX, sofaY, 2,
                                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)) {
            PlaceFurnitureOnWall(2, 4, -1, 0.f, 1, sofaX, sofaY, 3,
                                 nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        }
    }

    // Armchairs
    int32 chairX, chairY;
    CEntity* chairEnt{};
    if ((chairEnt = PlaceFurnitureOnWall(2, 1, m_furnitureId, 0.f, 1, -1, -1, 0,
                                         &chairX, &chairY, nullptr, nullptr, nullptr, nullptr))) {
        Lounge_AddChairInfo(chairX, chairY, chairEnt);
    }
    if ((chairEnt = PlaceFurnitureOnWall(2, 1, m_furnitureId, 0.f, 1, -1, -1, 0,
                                         &chairX, &chairY, nullptr, nullptr, nullptr, nullptr))) {
        Lounge_AddChairInfo(chairX, chairY, chairEnt);
    }
    if (perimeter > 0x1C
     && (chairEnt = PlaceFurnitureOnWall(2, 1, m_furnitureId, 0.f, 1, -1, -1, 0,
                                         &chairX, &chairY, nullptr, nullptr, nullptr, nullptr))) {
        Lounge_AddChairInfo(chairX, chairY, chairEnt);
    }

    // Free up the corner taken by the TV unit
    if (tvCorner != 0) SetCornerTiles(0, 2, 0, 0);
    if (tvCorner != 2) SetCornerTiles(2, 2, 0, 0);
    if (tvCorner != 1) SetCornerTiles(1, 2, 0, 0);
    if (tvCorner != 3) SetCornerTiles(3, 2, 0, 0);

    // Hifi
    int32 hifiX, hifiY, hifiZ;
    if (PlaceFurnitureOnWall(2, 6, -1, 0.f, 1, -1, -1, 0, &hifiX, nullptr, &hifiY, &hifiZ, nullptr, nullptr)) {
        Lounge_AddHifi(hifiX, hifiY, hifiZ, 0);
    }

    PlaceFurnitureOnWall(2, 5, -1, 0.f, 1, -1, -1, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    PlaceFurnitureOnWall(8, 0, -1, 0.f, 1, -1, -1, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    PlaceFurnitureOnWall(8, 0, -1, 0.f, 1, -1, -1, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    // Scatter probability threshold, based on the room's seed/wealth
    const auto seed = m_box->m_seed;
    int32 threshold;
    if (seed < 0x4B) {
        if (seed < 0x32) {
            threshold = 0x32 - CGeneral::GetRandomNumber();
        } else {
            threshold = 0x14 - CGeneral::GetRandomNumber();
        }
    } else {
        threshold = CGeneral::GetRandomNumber();
    }

    int32 tileX, tileY;
    if (CGeneral::GetRandomNumber() < threshold && FindEmptyTiles(2, 2, &tileX, &tileY)) {
        const auto furn = g_furnitureMan.GetFurniture(8, 2, -1, seed);
        PlaceObject(false, furn, (float)tileX + TILE_HALF, (float)tileY + TILE_HALF, 0.05f, 0.f);
        SetTilesStatus(tileX, tileY, 2, 2, 2, 0);
    }
    if (CGeneral::GetRandomNumber() < threshold && FindEmptyTiles(1, 1, &tileX, &tileY)) {
        const auto furn = g_furnitureMan.GetFurniture(8, 5, -1, seed);
        PlaceObject(false, furn, (float)tileX + TILE_HALF, (float)tileY + TILE_HALF, 0.05f, 0.f);
        SetTilesStatus(tileX, tileY, 1, 1, 2, 0);
    }
    if (CGeneral::GetRandomNumber() < threshold && FindEmptyTiles(1, 1, &tileX, &tileY)) {
        const auto furn = g_furnitureMan.GetFurniture(8, 4, -1, seed);
        PlaceObject(false, furn, (float)tileX + TILE_HALF, (float)tileY + TILE_HALF, 0.05f, 0.f);
        SetTilesStatus(tileX, tileY, 1, 1, 2, 0);
    }

    // Main furniture piece (sofa/table set) placed on a random free tile
    const auto furn = g_furnitureMan.GetFurniture(8, 1, -1, seed);
    int32 x, y;
    PlaceFurniture(furn, CGeneral::GetRandomNumber(), CGeneral::GetRandomNumber(), 0.f, 0, 0, &x, &y, 0);

    // Add goto-points along the free edges
    for (int32 x = 0; x < m_box->m_width - 1; x++) {
        const auto stBottom = GetTileStatus(x, 0);
        const auto stTop    = GetTileStatus(x, m_box->m_depth - 1);
        if (stBottom == 0 || stBottom == 2) {
            AddInteriorInfo(2, (float)x, 0.f, 2, nullptr);
        }
        if (stTop == 0 || stTop == 2) {
            AddInteriorInfo(2, (float)x, (float)(m_box->m_depth - 1), 0, nullptr);
        }
    }
    for (int32 y = 1; y < m_box->m_depth - 2; y++) {
        const auto stLeft  = GetTileStatus(0, y);
        const auto stRight = GetTileStatus(m_box->m_width - 1, y);
        if (stLeft == 0 || stLeft == 2) {
            AddInteriorInfo(2, 0.f, (float)y, 1, nullptr);
        }
        if (stRight == 0 || stRight == 2) {
            AddInteriorInfo(2, (float)(m_box->m_width - 1), (float)y, 3, nullptr);
        }
    }
}

// 0x599210
bool Interior_c::Office_PlaceEdgeFillers(int32 arg0, int32 a2, int32 x, int32 y, int32 dir) {
    rand(); // _rand() - discarded
    const auto rnd = CGeneral::GetRandomNumber();

    const int32 width = (dir == 2 || dir == 0) ? 1 : 2;
    if (GetNumEmptyTiles(x, y, width, 1) < 1) {
        return false;
    }

    // Step one tile inwards from the edge (dir == 2/0 step along Y, 3/1 along X)
    int32 fx = x;
    int32 fy = y;
    switch (dir) {
    case 2: fy = y + 1; break;
    case 0: fy = y - 1; break;
    case 3: fx = x - 1; break;
    case 1: fx = x + 1; break;
    }

    // Pick the furniture to place; all branches fall through to the single
    // PlaceFurniture call at the end.
    int32 furnitureGroupId = dir; // kept from original - doubles as PlaceFurniture's `dir`
    Furniture_c* furniture = nullptr;
    if (arg0 == -1) {
        if (0x5A < rnd && !bPlacedFiller) {
            furniture = g_furnitureMan.GetFurniture(1, 2, -1, m_box->m_status);
            if (furniture) {
                AddInteriorInfo(7, (float)fx, (float)fy, (dir - 2) & 3, nullptr);
            }
            bPlacedFiller = true;
        } else if (rnd < 0x4C) {
            if (rnd < 0x1A) {
                return false;
            }
            furniture = g_furnitureMan.GetFurniture(1, 3, -1, m_box->m_status);
            if (furniture) {
                AddInteriorInfo(7, (float)fx, (float)fy, (dir - 2) & 3, nullptr);
            }
        } else {
            furnitureGroupId = CGeneral::GetRandomNumberInRange(0, 5);
            furniture        = g_furnitureMan.GetFurniture(8, 0, -1, m_box->m_status);
        }
    } else {
        furniture = g_furnitureMan.GetFurniture(1, arg0, -1, m_box->m_status);
    }

    int32 outTileX, outTileY;
    PlaceFurniture(furniture, x, y, 0.f, 1, furnitureGroupId, &outTileX, &outTileY, false);
    return outTileX != 0;
}

// 0x5993E0
int32 Interior_c::Office_PlaceDesk(int32 a3, int32 arg4, int32 offsetY, int32 a5, uint8 a6, int32 b) {
    const int32  x   = a3;
    const int32  y   = arg4;
    const uint32 rot = ((int32)offsetY - 2U) & 3;

    int32 deskX = x;
    int32 deskY = y;
    if (offsetY == 1) {
        deskY = y + 1;
    } else if (offsetY == 2) {
        deskX = x + 1;
    }

    int32 out1, out2;

    // Desk
    const auto deskFurniture = g_furnitureMan.GetFurniture(1, 0, (int16)b, m_box->m_seed);
    if (PlaceFurniture(deskFurniture, deskX, deskY, 0, 1, rot, &out1, &out2, 0)) {
        // Chair, offset into the neighbouring tile depending on direction
        int32 chairX = x;
        int32 chairY = y;
        if (offsetY == 1) {
            chairX = x + 1;
        } else {
            if (offsetY == 0) {
                chairY = y + 1;
                deskX  = x + 1;
            } else if (offsetY == 3) {
                chairY = y + 1;
                deskX  = x + 1;
                chairX = deskX;
            } else if (offsetY == 2) {
                chairY = y + 1;
            }
        }

        const auto chairFurniture = g_furnitureMan.GetFurniture(1, 1, (int16)field_792, m_box->m_seed);
        CEntity*   placedChair    = PlaceFurniture(chairFurniture, chairX, chairY, 0, 1, rot, &out1, &out2, 1);

        // Sit info point, nudged by half a tile based on rotation
        float fx = (float)chairX;
        float fy = (float)chairY;
        if (rot == 2) {
            fy += 0.5f;
        } else if (rot == 0) {
            fy -= 0.5f;
        } else if (rot == 3) {
            fx -= 0.5f;
        } else if (rot == 1) {
            fx += 0.5f;
        }

        AddInteriorInfo(6, fx, fy, ((int32)rot - 2) & 3, placedChair);
        SetTilesStatus(deskX, chairY, 1, 1, 2, 1);
        return 2;
    }
    return 1;
}

// 0x5995B0
int32 Interior_c::Office_PlaceEdgeDesks(int32 a2, int32 x, int32 y, int32 dir, int32 a6) {
    rand(); // _rand() - discarded
    const auto rndA = CGeneral::GetRandomNumber();
    rand(); // _rand() - discarded
    const auto rndB = CGeneral::GetRandomNumber();

    // Desks occupy 1 tile along edges 0/2, 2 tiles along edges 1/3
    const int32 width = (dir == 2 || dir == 0) ? 1 : 2;

    int32 numDesks = GetNumEmptyTiles(x, y, width, 1);
    if (numDesks < 2) {
        return 1;
    }
    numDesks /= 2;

    rand(); // _rand() - discarded
    const auto rndC = CGeneral::GetRandomNumber();
    if (2 - rndC <= numDesks) {
        rand(); // _rand() - discarded
        numDesks = 2 - CGeneral::GetRandomNumber();
    }

    if (0x1E - rndB < rndA) {
        return 1;
    }

    int32 placed = 0;
    if (numDesks > 0) {
        const uint32 deskRot = dir - 2;
        auto count = numDesks;
        do {
            int32 fx, fy;
            switch (a6) {
            case 0: fx = placed + x; fy = y - 1;     break;
            case 3: fx = x - 1;      fy = placed + y; break;
            case 1: fx = x;          fy = placed + y; break;
            case 2: fx = placed + x; fy = y;         break;
            default: NOTSA_UNREACHABLE();
            }
            placed += Office_PlaceDesk(fx, fy, deskRot & 3, 0x46, 0, m_furnitureId);
        } while (--count != 0);
    }
    return placed + 1;
}

// 0x599770
void Interior_c::Office_FurnishEdges() {
    const int32 xEnd = m_box->m_width - 3;
    const int32 yEnd = m_box->m_depth - 3;

    // Mark inner perimeter tiles as no-go
    for (int32 i = 2; i <= xEnd; i++) {
        SetTilesStatus(i, yEnd, 1, 1, 3, 0);
        SetTilesStatus(i, 2, 1, 1, 3, 0);
    }
    for (int32 i = 2; i <= yEnd; i++) {
        SetTilesStatus(2, i, 1, 1, 3, 0);
        SetTilesStatus(xEnd, i, 1, 1, 3, 0);
    }

    // Corner goto points
    AddGotoPt(2, 2, 0.5f, 0.5f);
    AddGotoPt(2, yEnd, 0.5f, -0.5f);
    AddGotoPt(xEnd, 2, -0.5f, 0.5f);
    AddGotoPt(xEnd, yEnd, -0.5f, -0.5f);

    const int32  depth = m_box->m_depth;
    const int32  wM1   = m_box->m_width - 1;

    SetTilesStatus(m_box->m_door - 2, 0, 4, 2, 7, 0);

    // Edge desks (against walls)
    for (int32 i = 1; i < wM1;) {
        i += Office_PlaceEdgeDesks(-1, i, 0, 2, 2);
    }
    for (int32 i = 1; i < wM1;) {
        i += Office_PlaceEdgeDesks(-1, i, depth - 1, 0, 0);
    }
    for (int32 i = 1; i <= depth - 2;) {
        i += Office_PlaceEdgeDesks(-1, 0, i, 1, 1);
    }
    for (int32 i = 1; i <= depth - 2;) {
        i += Office_PlaceEdgeDesks(-1, wM1, i, 3, 3);
    }

    // Edge fillers
    for (int32 i = 1; i < wM1;) {
        i += Office_PlaceEdgeFillers(-1, i, 0, 2, 2);
    }
    for (int32 i = 1; i < wM1;) {
        i += Office_PlaceEdgeFillers(-1, i, depth - 1, 0, 0);
    }
    for (int32 i = 1; i <= depth - 2;) {
        i += Office_PlaceEdgeFillers(-1, 0, i, 1, 1);
    }
    for (int32 i = 1; i <= depth - 2;) {
        i += Office_PlaceEdgeFillers(-1, wM1, i, 3, 3);
    }
}

// 0x599960
int32 Interior_c::Office_PlaceDeskQuad(int32 a2, int32 a3, int32 a4, int32 a5) {
    const int32 y2 = a4 - 2;

    Office_PlaceDesk(a3, y2, 2, 0x46, 0, a5);
    Office_PlaceDesk(a3, a4, 0, 0x46, 0, a5);
    Office_PlaceDesk(a3 - 2, a4, 0, 0x46, 0, a5);
    Office_PlaceDesk(a3 - 2, y2, 2, 0x46, 0, a5);

    SetTilesStatus(a3 - 3, a4 - 3, 6, 1, 3, 0);
    SetTilesStatus(a3 - 3, a4 + 2, 6, 1, 3, 0);
    SetTilesStatus(a3 - 3, y2, 1, 4, 3, 0);
    SetTilesStatus(a3 + 2, y2, 1, 4, 3, 0);

    return 6;
}

// 0x599A30
int32 Interior_c::Office_FurnishCenter() {
    const int32 w    = m_box->m_width - 6;
    int32 numX = w / 6;
    const int32 d    = m_box->m_depth - 6;
    int32       x    = (w % 6) / 2;
    if (0 < w && 0 < d && 0 < numX) {
        do {
            x += 6;
            int32 numY = d / 6;
            int32 y    = (d % 6) / 2;
            if (0 < d / 6) {
                do {
                    Office_PlaceDeskQuad(-1, x, y + 6, m_furnitureId);
                    numY--;
                    y += 6;
                } while (numY != 0);
            }
            numX--;
        } while (numX != 0);
    }
    return 0;
}

// 0x599AF0
void Interior_c::FurnishOffice() {
    // Block out the four 2x2 corners
    SetTilesStatus(0, 0, 2, 2, 2, 0);
    SetTilesStatus(0, m_box->m_depth - 2, 2, 2, 2, 0);
    SetTilesStatus(m_box->m_width - 2, 0, 2, 2, 2, 0);
    SetTilesStatus(m_box->m_width - 2, m_box->m_depth - 2, 2, 2, 2, 0);

    m_furnitureId = (int8)g_furnitureMan.GetRandomId(1, 0, m_box->m_seed);
    field_792     = (int8)g_furnitureMan.GetRandomId(1, 1, m_box->m_seed);

    Office_FurnishEdges();
    Office_FurnishCenter();
}

// 0x599BB0
int8 Interior_c::Shop_Place3PieceUnit(int32 a2, int32 a3, int32 a4, int32 a5, int32 a6) {
    // a2=furniture subgroup, a3=x, a4=y, a5=direction, a6=length
    const uint8 wealth = m_box->m_status;

    Furniture_c* f1;
    Furniture_c* fMid;
    Furniture_c* fEnd;
    if (a5 == 2 || a5 == 3) {
        f1   = g_furnitureMan.GetFurniture(m_furnitureGroupId, a2 + 1, -1, wealth);
        fMid = g_furnitureMan.GetFurniture(m_furnitureGroupId, a2, f1->m_nId, wealth);
        fEnd = g_furnitureMan.GetFurniture(m_furnitureGroupId, a2 + 2, f1->m_nId, wealth);
    } else {
        f1   = g_furnitureMan.GetFurniture(m_furnitureGroupId, a2, -1, wealth);
        fMid = g_furnitureMan.GetFurniture(m_furnitureGroupId, a2 + 1, f1->m_nId, wealth);
        fEnd = g_furnitureMan.GetFurniture(m_furnitureGroupId, a2 + 2, f1->m_nId, wealth);
    }

    // a5/a2 are passed by address to PlaceFurniture and reused as its outputs
    int32 outA = a5;
    int32 outB = a2;

    if (a5 != 2 && a5 != 0) {
        // Vertical
        PlaceFurniture(f1, a3, a4, 0.0f, 1, a5, &outA, &outB, 0);
        int32 count = a6 - 2;
        int32 pos   = a3 + outA;
        for (; count > 0; count--) {
            PlaceFurniture(fEnd, pos, a4, 0.0f, 1, a5, &outA, &outB, 0);
            pos += outA;
        }
        PlaceFurniture(fMid, pos, a4, 0.0f, 1, a5, &outA, &outB, 0);
        return 1;
    } else {
        // Horizontal
        PlaceFurniture(f1, a3, a4, 0.0f, 1, a5, &outA, &outB, 0);
        int32 count = a6 - 2;
        int32 pos   = a4 + outB;
        for (; count > 0; count--) {
            PlaceFurniture(fEnd, a3, pos, 0.0f, 1, a5, &outA, &outB, 0);
            pos += outB;
        }
        PlaceFurniture(fMid, a3, pos, 0.0f, 1, a5, &outA, &outB, 0);
        return 1;
    }
}

// 0x599DC0
int32 Interior_c::Shop_PlaceEdgeUnits(int32 a2, int32 a3, int32 a4, int32 a5) {
    // a2=furniture subgroup, a3=x, a4=y, a5=direction
    const int32 width = (a5 == 2 || a5 == 0) ? 1 : 2;

    const int32 numEmpty = GetNumEmptyTiles(a3, a4, width, 1);
    if (numEmpty < 2)
        return 1;

    const int32 r = CGeneral::GetRandomNumberInRange(0, 100);
    int32 size    = numEmpty;
    if (numEmpty != 3) {
        const int32 diff = numEmpty - (2 - r);
        if (-1 < diff) {
            size = 2 - r;
            if (diff == 1)
                size = 1 - r;
        }
    }

    if (a2 != -1) {
        Shop_Place3PieceUnit(a2, a3, a4, a5, size);
        return size;
    }
    if (g_ShopStyleRand > 0x32) {
        Shop_Place3PieceUnit(0, a3, a4, a5, size);
        return size;
    }
    if (g_ShopStyleRand < 0x1A) {
        if (g_ShopStyleRand < 0xB) {
            Shop_Place3PieceUnit(9, a3, a4, a5, size);
            return size;
        }
        Shop_Place3PieceUnit(6, a3, a4, a5, size);
        return size;
    }
    Shop_Place3PieceUnit(3, a3, a4, a5, size);
    return size;
}

// 0x599EF0
int32 Interior_c::Shop_PlaceCounter(uint8 a2) {
    Furniture_c* f1 = g_furnitureMan.GetFurniture(0, 0xC, -1, m_box->m_status);
    Furniture_c* f2 = g_furnitureMan.GetFurniture(0, 0xD, -1, m_box->m_status);
    const int32 r    = CGeneral::GetRandomNumberInRange(0, 100);

    int32 outA;
    int32 outB;
    int32 x;
    int32 y;
    if (a2 == 0) {
        x = m_box->m_door + 2;
        PlaceFurniture(f1, x, 1, 0.0f, 1, 0, &outA, &outB, 0);
        SetTilesStatus(x, 0, outA + 1, 1, 2, 0);
        y = m_box->m_door - 2;
    } else {
        y = m_box->m_door;
        x = y - 5;
        PlaceFurniture(f1, x, 1, 0.0f, 1, 0, &outA, &outB, 0);
        SetTilesStatus(y - 6, 0, outA + 1, 1, 2, 0);
        y = m_box->m_door + 1;
    }
    PlaceFurniture(f2, y, 0, 0.0f, 1, r, &outA, &outB, 1);
    return x + 2;
}

// 0x59A030
void Interior_c::Shop_PlaceFixedUnits() {
    const int8 door = m_box->m_door;
    if (door == -1) {
        return;
    }

    SetTilesStatus(door - 1, 0, 2, 1, 7, 0);

    const int32 doorEnd = door - 2;

    int32 counterId  = static_cast<int32>(reinterpret_cast<intptr_t>(this)); // as in original: uninitialized if no counter is placed
    bool placeOnLeft = false;

    // (uint)m_width - door - 2 => space from door position to the right edge
    if (static_cast<int32>(static_cast<uint32>(m_box->m_width) - door - 2) < 6) {
        if (doorEnd >= 6) {
            placeOnLeft = true;
        } else {
            goto addInfos;
        }
    } else if (doorEnd < 6) {
        placeOnLeft = false;
    } else {
        placeOnLeft = rand() < 0x3FFF;
    }

    counterId = Shop_PlaceCounter(placeOnLeft);

addInfos:
    AddInteriorInfo(9, static_cast<float>(counterId), 2.0f, 0, nullptr);
    AddInteriorInfo(10, static_cast<float>(counterId), 0.0f, 2, nullptr);
}

// 0x59A130
// NOTE: The original decompilation for this address is actually the body of
// FurnishOffice (0x599AF0) - Ghidra mismatch in the batch data. Translated as-is.
void Interior_c::Shop_FurnishCeiling() {
    SetTilesStatus(0, 0, 2, 2, 2, 0);
    SetTilesStatus(0, m_box->m_depth - 2, 2, 2, 2, 0);
    SetTilesStatus(m_box->m_width - 2, 0, 2, 2, 2, 0);
    SetTilesStatus(m_box->m_width - 2, m_box->m_depth - 2, 2, 2, 2, 0);

    m_furnitureId = (int8)g_furnitureMan.GetRandomId(1, 0, m_box->m_status);
    field_792     = (int8)g_furnitureMan.GetRandomId(1, 1, m_box->m_status);

    Office_FurnishEdges();
    Office_FurnishCenter();
}

// 0x59A140
void Interior_c::Shop_AddShelfInfo(int32 a2, int32 a3, int32 a5) {
    if (g_ShelfInfoCounter > 1) {
        if (CGeneral::GetRandomNumberInRange(0, 100) > 60) {
            AddInteriorInfo(8, (float)a2, (float)a3, a5, nullptr);
            g_ShelfInfoCounter = 1;
            return;
        }
    }
    g_ShelfInfoCounter++;
}

// 0x59A1B0
void Interior_c::Shop_FurnishEdges() {
    const int32 width = m_box->m_width;
    const int32 depth = m_box->m_depth;

    // Bottom edge (row depth-1)
    int32 x = 1;
    int32 subgroup;
    {
        const int32 r = CGeneral::GetRandomNumberInRange(0, 100);
        if (r < 0x33) {
            if (r < 0x1A)
                subgroup = ((r < 0xB) - 1 & 0xFFFFFFFD) + 9; // 6 or 9
            else
                subgroup = 3;
        } else {
            subgroup = 0;
        }
    }
    if (1 < width - 1) {
        do {
            x += Shop_PlaceEdgeUnits(subgroup, x, depth - 1, 0);
        } while (x < width - 1);
    }

    // Left edge (column 0)
    const int32 maxY = depth - 2;
    int32 y = 1;
    {
        const int32 r = CGeneral::GetRandomNumberInRange(0, 100);
        if (r < 0x33) {
            if (r < 0x1A)
                subgroup = ((r < 0xB) - 1 & 0xFFFFFFFD) + 9;
            else
                subgroup = 3;
        } else {
            subgroup = 0;
        }
    }
    if (0 < maxY) {
        do {
            y += Shop_PlaceEdgeUnits(subgroup, 0, y, 1);
        } while (y <= maxY);
    }

    // Right edge (column width-1)
    y = 1;
    {
        const int32 r = CGeneral::GetRandomNumberInRange(0, 100);
        if (r < 0x33) {
            if (r < 0x1A)
                subgroup = ((r < 0xB) - 1 & 0xFFFFFFFD) + 9;
            else
                subgroup = 3;
        } else {
            subgroup = 0;
        }
    }
    if (0 < maxY) {
        do {
            y += Shop_PlaceEdgeUnits(subgroup, width - 1, y, 3);
        } while (y <= maxY);
    }

    // Bottom inner row: shelf infos above the bottom edge, skipping top door
    const int32 maxX = width - 2;
    for (int32 i = 1; i <= maxX; i++) {
        if (m_box->m_tDoorStart == -1 || i < m_box->m_tDoorStart || m_box->m_tDoorEnd < i) {
            if (g_ShelfInfoCounter > 1) {
                if (CGeneral::GetRandomNumberInRange(0, 100) > 60) {
                    AddInteriorInfo(8, (float)i, (float)maxY, 2, nullptr);
                    g_ShelfInfoCounter = 0;
                }
            }
            g_ShelfInfoCounter++;
        }
        if (GetTileStatus(i, 1) == 0) {
            SetTilesStatus(i, 1, 1, 1, 2, 0);
        }
    }

    // Side inner columns: shelf infos, skipping left/right doors
    const int32 maxY2 = depth - 3;
    int32 counter = g_ShelfInfoCounter;
    for (int32 i = 2; i <= maxY2; i++) {
        if (m_box->m_lDoorStart == -1 || i < m_box->m_lDoorStart || m_box->m_lDoorEnd < i) {
            if (1 < counter) {
                if (CGeneral::GetRandomNumberInRange(0, 100) > 60) {
                    AddInteriorInfo(8, 1.0f, (float)i, 3, nullptr);
                    counter = 0;
                }
            }
            counter++;
            g_ShelfInfoCounter = counter;
        }
        if (m_box->m_rDoorStart == -1 || i < m_box->m_rDoorStart || m_box->m_rDoorEnd < i) {
            if (1 < counter) {
                if (CGeneral::GetRandomNumberInRange(0, 100) > 60) {
                    AddInteriorInfo(8, (float)maxX, (float)i, 1, nullptr);
                    counter = 0;
                }
            }
            counter++;
            g_ShelfInfoCounter = counter;
        }
    }

    // Inner ring tile marking
    const int32 maxX2 = width - 3;
    if (1 < maxX2) {
        for (int32 i = 2; i <= maxX2; i++) {
            if (GetTileStatus(i, 2) == 0) {
                SetTilesStatus(i, 2, 1, 1, 2, 0);
            }
            SetTilesStatus(i, maxY2, 1, 1, 3, 0);
        }
    }
    if (2 < depth - 4) {
        for (int32 j = 3; j <= depth - 4; j++) {
            SetTilesStatus(2, j, 1, 1, 3, 0);
            SetTilesStatus(maxX2, j, 1, 1, 3, 0);
        }
    }
    SetTilesStatus(3, 3, width - 6, 1, 3, 0);
}

// 0x59A590
void Interior_c::Shop_FurnishAisles() {
    const int32 width = m_box->m_width;
    const int32 depth = m_box->m_depth;
    const int32 maxI  = width - 6;
    const int32 len   = depth - 7;
    if (maxI <= 0 || len <= 0)
        return;

    AddGotoPt(2, 3, -0.5f, -0.5f);
    const int32 y2 = depth - 3;
    AddGotoPt(2, y2, -0.5f, 0.5f);

    for (int32 i = 0; i < maxI; i++) {
        int32 subgroup;
        const int32 r = CGeneral::GetRandomNumberInRange(0, 100);
        if (r < 0x33) {
            if (r < 0x1A)
                subgroup = ((r < 0xB) - 1 & 0xFFFFFFFD) + 9; // 6 or 9
            else
                subgroup = 3;
        } else {
            subgroup = 0;
        }

        switch (i % 4) {
        case 0: {
            if (i != width - 7) {
                int32 y = 4;
                for (int32 n = len; n > 0; n--) {
                    y += Shop_PlaceEdgeUnits(subgroup, i + 3, y, 3);
                }
            }
            break;
        }
        case 1: {
            int32 y = 4;
            for (int32 n = len; n > 0; n--) {
                y += Shop_PlaceEdgeUnits(subgroup, i + 3, y, 1);
            }
            break;
        }
        case 2: {
            for (int32 j = 0; j < len; j++) {
                Shop_AddShelfInfo(i + 3, j + 4, 3);
            }
            break;
        }
        case 3: {
            SetTilesStatus(i + 3, 4, 1, len, 3, 0);
            AddGotoPt(i + 3, 3, -0.5f, -0.5f);
            AddGotoPt(i + 3, y2, -0.5f, 0.5f);
            break;
        }
        }
    }
    AddGotoPt(maxI + 3, 3, 0.5f, -0.5f);
    AddGotoPt(maxI + 3, y2, 0.5f, 0.5f);
}

// 0x593DB0
bool Interior_c::GetBoundingBox(FurnitureEntity_c* entity, CVector* a3) {
    switch (m_box->m_type) {
    case 0:
    case 1:
    case 6:
        break;
    default:
        return false;
    }

    const auto tileX = entity->m_tileX;
    const auto tileY = entity->m_tileY;

    // Flood-fill visited flags, 30x30 grid
    int32 visited[30 * 30];
    std::fill(std::begin(visited), std::end(visited), 0);
    visited[tileX * 0x1E + tileY] = 1;

    int32 x0, x1, y0, y1;
    FindBoundingBox(tileX, tileY, &x0, &x1, &y0, &y1, visited);

    GetTileCentre((float)x0 - TILE_HALF - BBOX_MARGIN, (float)y1 + BBOX_MARGIN + TILE_HALF, &a3[0]);
    GetTileCentre((float)x0 - TILE_HALF - BBOX_MARGIN, (float)y0 - TILE_HALF - BBOX_MARGIN, &a3[1]);
    GetTileCentre((float)x1 + BBOX_MARGIN + TILE_HALF, (float)y0 - TILE_HALF - BBOX_MARGIN, &a3[2]);
    GetTileCentre((float)x1 + BBOX_MARGIN + TILE_HALF, (float)y1 + BBOX_MARGIN + TILE_HALF, &a3[3]);
    return true;
}

// 0x593910
void Interior_c::ResetTiles() {
    memset(field_68, 0, sizeof(field_68));

    const auto& box = *m_box;

    const auto paintRun = [&](char* base, int32 count, int32 stride, int8 status) {
        for (auto i = 0; i < count; i++, base += stride) {
            if (*base != 3 && *base == 0) {
                *base = (char)status;
            }
        }
    };

    // Left wall doors (x = 0), status 8
    if (box.m_lDoorStart != -1) {
        const auto y = (int32)box.m_lDoorStart;
        const auto count = (int32)box.m_lDoorEnd - y;
        if (count > 0 && y >= 0 && box.m_width != 0 && y + count <= (int32)box.m_depth) {
            paintRun(&field_68[y], count, 1, 8);
        }
    }

    // Right wall doors (row = m_width - 1), status 8
    if (box.m_rDoorStart != -1) {
        const auto y = (int32)box.m_rDoorStart;
        const auto count = (int32)box.m_rDoorEnd - y;
        const auto row = (int32)box.m_width - 1;
        if (count > 0 && row >= 0 && y >= 0 && y + count <= (int32)box.m_depth) {
            paintRun(&field_68[row * 0x1E + y], count, 1, 8);
        }
    }

    // Top wall doors (column = m_depth - 1), status 8
    if (box.m_tDoorStart != -1) {
        const auto x = (int32)box.m_tDoorStart;
        const auto count = (int32)box.m_tDoorEnd - x;
        const auto col = (int32)box.m_depth - 1;
        if (count > 0 && col >= 0 && x >= 0 && x + count <= (int32)box.m_width) {
            paintRun(&field_68[x * 0x1E + col], count, 0x1E, 8);
        }
    }

    // Left wall windows, status 9
    if (box.m_lWindowStart != -1) {
        const auto y = (int32)box.m_lWindowStart;
        const auto count = (int32)box.m_lWindowEnd - y;
        if (count > 0 && y >= 0 && box.m_width != 0 && y + count <= (int32)box.m_depth) {
            paintRun(&field_68[y], count, 1, 9);
        }
    }

    // Right wall windows (row = m_width - 1), status 9
    if (box.m_rWindowStart != -1) {
        const auto y = (int32)box.m_rWindowStart;
        const auto count = (int32)box.m_rWindowEnd - y;
        const auto row = (int32)box.m_width - 1;
        if (count > 0 && row >= 0 && y >= 0 && y + count <= (int32)box.m_depth) {
            paintRun(&field_68[row * 0x1E + y], count, 1, 9);
        }
    }

    // Top wall windows (column = m_depth - 1), status 9
    if (box.m_tWindowStart != -1) {
        const auto x = (int32)box.m_tWindowStart;
        const auto count = (int32)box.m_tWindowEnd - x;
        const auto col = (int32)box.m_depth - 1;
        if (count > 0 && col >= 0 && x >= 0 && x + count <= (int32)box.m_width) {
            paintRun(&field_68[x * 0x1E + col], count, 0x1E, 9);
        }
    }

    // No-go zones (3 rectangles), status 11 (0xB)
    for (auto i = 0; i < 3; i++) {
        const auto left   = (int32)box.m_noGoLeft[i];
        const auto bottom = (int32)box.m_noGoBottom[i];
        if (left != -1 && bottom != -1) {
            int32 noGoWidth = (int32)box.m_noGoWidth[i];
            const auto depth = (int32)box.m_noGoDepth[i];
            if (left >= 0 && bottom >= 0 && left + noGoWidth <= (int32)box.m_width && bottom + depth <= (int32)box.m_depth && noGoWidth > 0) {
                auto p = &field_68[left * 0x1E + bottom];
                do {
                    for (auto j = 0; j < depth; j++) {
                        if (p[j] != 3 && p[j] == 0) {
                            p[j] = 0xB;
                        }
                    }
                    p += 0x1E;
                } while (--noGoWidth != 0);
            }
        }
    }
}

// 0x5934E0
CObject* Interior_c::PlaceObject(uint8 isStealable, Furniture_c* furniture, float offsetX, float offsetY, float offsetZ, float rotationZ) {
    const float tileSize = TILE_SIZE;
    auto& interiorMan = g_interiorMan;

    // Position relative to the interior's origin effect, lifted by the model's bbox min z
    const auto x = -(float)(uint8)m_box->m_width * tileSize + offsetX;
    const auto y = -(float)(uint8)m_box->m_depth * tileSize + offsetY;
    const auto z = (-(float)(uint8)m_box->m_height * tileSize + offsetZ)
                 - CModelInfo::ms_modelInfoPtrs[furniture->m_nModelId]->GetColModel()->m_boundBox.m_vecMin.z;

    if (NumFreeFurnitureEntitySlots() > 0) {
        // Interior rotation matrix
        CMatrix intMat;
        intMat.Attach(&m_matrix, false);
        intMat.RotateZ(rotationZ * INT_DEG_TO_RAD, false);
        intMat.GetPosition() += CVector{ x, y, z };

        auto* object = new CObject(furniture->m_nModelId, false);
        object->SetMatrix(intMat);
        object->SetAreaCode((eAreaCodes)m_areaCode);
        object->m_nFlags |= 0x10000;            // bDontCastShadowsOn
        object->m_nObjectType = OBJECT_TYPE_DECORATION;
        object->SetIsStatic(true);
        CWorld::Add(object);

        // Record tile coords on the FurnitureEntity_c slot (random in original!)
        FurnitureEntity_c* slot = nullptr;
        if ((slot = TakeFurnitureEntitySlot())) {
            slot->m_tileX = (uint16)CGeneral::GetRandomNumber();
            slot->m_tileY = (uint16)CGeneral::GetRandomNumber();
        }

        if (isStealable) {
            object->m_nFlags |= 0x2000; // bIsLiftable

            if (!interiorMan.HasInteriorHadStealDataSetup(this)) {
                // Not set up yet - append a new InteriorObject record
                // NOTE: m_Objects / m_ObjectCount of the manager (private, accessed by address)
                static auto& objectCount = StaticRef<int32>(0xBB3A18);
                static auto& objects     = StaticRef<std::array<InteriorObject, 32>>(0xBB3A1C);
                auto& rec = objects[objectCount];
                rec = InteriorObject{};
                rec.entity     = object;
                rec.modelId    = furniture->m_nModelId;
                rec.interiorId = m_interiorId;
                rec.pos        = CVector{ x, y, z };
                rec.wasStolen  = false;
                objectCount++;
            } else {
                static auto& objects = StaticRef<std::array<InteriorObject, 32>>(0xBB3A1C);
                auto idx = interiorMan.FindStealableObjectId(m_interiorId, furniture->m_nModelId, { x, y, z });
                if (idx < 0 || objects[idx].wasStolen) {
                    idx = interiorMan.FindStealableObjectId(m_interiorId, furniture->m_nModelId, { x, y, z });
                    if (idx >= 0) {
                        objects[idx].entity = object;
                    }
                } else {
                    // An un-stolen instance of this stealable already exists - drop this one
                    CWorld::Remove(object);
                    delete object;
                    DiscardFurnitureEntitySlot(slot);
                    CommitFurnitureEntitySlot(slot);
                    return nullptr;
                }
            }
        }

        CommitFurnitureEntitySlot(slot);
        return object;
    }
    return nullptr;
}

// 0x5913B0
FurnitureEntity_c* Interior_c::GetFurnitureEntity(CEntity* entity) {
    for (auto it = static_cast<FurnitureEntity_c*>(m_list.GetHead()); it; it = it->m_pNext) {
        if (it->m_entity == entity) {
            return it;
        }
    }
    return nullptr;
}

// 0x5913E0
bool Interior_c::IsPtInside(const CVector& pt, CVector bias) {
    const CVector diff = pt - CVector{ m_matrix.pos.x, m_matrix.pos.y, m_matrix.pos.z };

    const auto dx = fabs(diff.x * m_matrix.right.x + diff.y * m_matrix.right.y + diff.z * m_matrix.right.z);
    if (bias.x + (float)m_box->m_width * TILE_SIZE < dx) {
        return false;
    }

    const auto dy = fabs(diff.x * m_matrix.up.x + diff.y * m_matrix.up.y + diff.z * m_matrix.up.z);
    if (bias.y + (float)m_box->m_depth * TILE_SIZE < dy) {
        return false;
    }

    const auto dz = fabs(diff.x * m_matrix.at.x + diff.y * m_matrix.at.y + diff.z * m_matrix.at.z);
    if (bias.z + (float)m_box->m_height * TILE_SIZE < dz) {
        return false;
    }

    return true;
}

// 0x5914D0
void Interior_c::CalcMatrix(CVector* translation) {
    RwMatrix& m = m_matrix;

    // Build identity matrix (right.z is OR'ed with the RW matrix flag bits, as in the original)
    m.right = CVector(1.0f, 0.0f, 0.0f);
    m.up    = CVector(0.0f, 1.0f, 0.0f);
    m.at    = CVector(0.0f, 0.0f, 1.0f);
    m.pos   = CVector(0.0f, 0.0f, 0.0f);
    *reinterpret_cast<uint32*>(&m.right.z) |= 0x20003; // right.z = 1.0f + rwMatrix flags

    const CVector rotAxis{ 0.0f, 0.0f, 1.0f };
    RwMatrixRotate(&m, &rotAxis, m_box->m_rot, rwCOMBINEREPLACE);
    RwMatrixTranslate(&m, translation, rwCOMBINEPOSTCONCAT);

    // Concatenate the owning group entity's frame modelling matrix
    if (const auto* entity = m_pGroup->GetEntity()) {
        if (const auto* rwObject = entity->GetRwObject()) {
            RwMatrixMultiply(&m, &m, &reinterpret_cast<RwFrame*>(rwObject->parent)->modelling);
        }
    }
}

// 0x591590
void Interior_c::Furnish() {
    // The original compiler emitted inlined clones of the FurnishXxx() functions for cases 1-4;
    // their bodies are byte-for-byte identical to the standalone functions, so call them instead.
    switch (m_box->m_type) {
    case 0:
        // NOTE: the argument is not recoverable from the decompilation (IDA lost the stack slot).
        // FurnishShop() stores it into m_furnitureGroupId; the box's group id is the plausible value.
        FurnishShop(m_box->m_groupId);
        break;
    case 1:
        FurnishOffice();
        break;
    case 2:
        FurnishLounge();
        break;
    case 3:
        FurnishBedroom();
        break;
    case 4:
        FurnishKitchen();
        break;
    }
}

// 0x5915D0
void Interior_c::Unfurnish() {
    for (auto it = static_cast<FurnitureEntity_c*>(m_list.GetHead()); it; ) {
        const auto next = static_cast<FurnitureEntity_c*>(it->m_pNext);

        auto* entity = it->m_entity;

        // If the player isn't holding this entity, or it isn't a liftable object - remove and delete it.
        // Otherwise, turn it into a temporary object (so it eventually gets cleaned up).
        CEntity* heldEntity{};
        if (auto* player = FindPlayerPed()) {
            heldEntity = player->GetEntityThatThisPedIsHolding();
        }

        if (heldEntity != entity
            || !entity->GetIsTypeObject()
            || (entity->AsObject()->m_nObjectFlags & 0x2000) == 0 // bIsLiftable
        ) {
            CWorld::Remove(entity);
            if (entity) {
                delete entity; // scalar deleting destructor
            }
        } else {
            auto* object = entity->AsObject();
            CObject::nNoTempObjects++;
            object->m_nObjectType  = OBJECT_TEMPORARY;
            object->m_nRemovalTime = CTimer::m_snTimeInMilliseconds + 99999999;
        }

        it->m_entity = nullptr;

        // TList_c<void> can't be iterated/edited in C++ (void has no members) -
        // use the original's unlink+delete helper (FUN_004A8E30)
        plugin::Call<0x4A8E30>(it);

        it = next;
    }
}

// 0x591680
int8 Interior_c::CheckTilesEmpty(int32 x, int32 y, int32 width, int32 depth, uint8 allowTableTiles) {
    if (x < 0
        || y < 0
        || static_cast<int32>(static_cast<uint8_t>(m_box->m_width)) < x + width
        || static_cast<int32>(static_cast<uint8_t>(m_box->m_depth)) < y + depth
    ) {
        return false;
    }

    for (int32 iy = 0; iy < width; iy++) { // NOTE: rows are x, columns are y (stride 0x1E = 30)
        for (int32 ix = 0; ix < depth; ix++) {
            const auto tileStatus = field_68[(x + iy) * 0x1E + (y + ix)];
            if (tileStatus != 0) {
                if (!allowTableTiles) {
                    return false;
                }
                if (tileStatus != '\t') { // Tile status 9 is still considered empty when `allowTableTiles`
                    return false;
                }
            }
        }
    }
    return true;
}

// 0x591700
void Interior_c::SetTilesStatus(int32 a, int32 b, int32 a3, int32 a4, int32 a5, int8 a6) {
    // a=x, b=y, a3=width, a4=height, a5=new status, a6=mode
    if (a < 0 || b < 0)
        return;
    if (a + a3 > m_box->m_width || b + a4 > m_box->m_depth || a3 <= 0)
        return;

    int8* tiles = reinterpret_cast<int8*>(&field_68); // tile grid, stride 0x1E (30)
    int8* tile  = tiles + a * 0x1E + b;

    for (int32 row = 0; row < a3; row++) {
        if (a4 > 0) {
            for (int32 col = 0; col < a4; col++) {
                const int8 status = tile[col];
                if (status == 9 && a5 == 5) {
                    tile[col] = 10;
                } else if (a6 == 0) {
                    if (status == 3) {
                        if (a5 == 3)
                            return;
                        if (a5 == 4)
                            tile[col] = 4;
                    } else if (status == 0) {
                        tile[col] = (int8)a5;
                    }
                } else if (status != 5 && status != 7 && status != 8) {
                    tile[col] = (int8)a5;
                }
            }
        }
        tile += 0x1E;
    }
}

// 0x5917C0
void Interior_c::SetCornerTiles(int32 a4, int32 a3, int32 a5, uint8 a6) {
    const auto& box = *m_box;
    if (a4 == 1) {
        SetTilesStatus(0, 0, a3, 1, a5, a6);
        SetTilesStatus(0, 0, 1, a3, a5, a6);
    } else if (a4 == 0) {
        SetTilesStatus(0, box.m_depth - 1, a3, 1, a5, a6);
        SetTilesStatus(0, box.m_depth - a3, 1, a3, a5, a6);
    } else if (a4 == 2) {
        SetTilesStatus(box.m_width - a3, 0, a3, 1, a5, a6);
        SetTilesStatus(box.m_width - 1, 0, 1, a3, a5, a6);
    } else if (a4 == 3) {
        SetTilesStatus(box.m_width - a3, box.m_depth - 1, a3, 1, a5, a6);
        SetTilesStatus(box.m_width - 1, box.m_depth - a3, 1, a3, a5, a6);
    }
}

// 0x5918E0
int32 Interior_c::GetTileStatus(int32 x, int32 y) {
    if (x < m_box->m_width && y < m_box->m_depth && x >= 0 && y >= 0) {
        return field_68[x * 0x1E + y];
    }
    return 1;
}

// 0x591920
int32 Interior_c::GetNumEmptyTiles(int32 x, int32 y, int32 direction, int32 numTiles) {
    int32 numEmpty = 0;

    const int32 step = (direction == 3 || direction == 0) ? -1 : 1;

    if (direction == 3 || direction == 1) {
        // Walk along X, checking `numTiles` tiles in Y for each row
        for (;; x += step) {
            for (auto i = 0, ty = y; i < numTiles; i++, ty++) {
                if (m_box->m_width <= x || m_box->m_depth <= ty || x < 0 || ty < 0 || field_68[x * 0x1E + ty] != 0) {
                    return numEmpty;
                }
            }
            numEmpty++;
        }
    } else {
        // Walk along Y, checking `numTiles` tiles in X for each column
        for (;; y += step) {
            for (auto i = 0, tx = x; i < numTiles; i++, tx++) {
                if (m_box->m_width <= tx || m_box->m_depth <= y || tx < 0 || y < 0 || field_68[tx * 0x1E + y] != 0) {
                    return numEmpty;
                }
            }
            numEmpty++;
        }
    }
}

// 0x591B20
// NOTE: Header said `int32`, but the original returns nothing (void).
void Interior_c::GetRandomTile(int32 tileStatus, int32* outX, int32* outY) {
    int32 x, y;
    uint8 status;
    do {
        x = CGeneral::GetRandomNumber();
        y = CGeneral::GetRandomNumber();
        status = 1;
        if (x < m_box->m_width && y < m_box->m_depth && x >= 0 && y >= 0) {
            status = field_68[x * 0x1E + y];
        }
    } while (status != tileStatus);
    *outX = x;
    *outY = y;
}

// 0x591BD0
CVector* Interior_c::GetTileCentre(float offsetX, float offsetY, CVector* pointsIn) {
    pointsIn->x = -(float)m_box->m_width * TILE_SIZE + offsetX + TILE_SIZE;
    pointsIn->y = -(float)m_box->m_depth * TILE_SIZE + offsetY + TILE_SIZE;
    pointsIn->z = -(float)m_box->m_height * TILE_SIZE;

    RwV3dTransformPoints(pointsIn, pointsIn, 1, &m_matrix);
    return pointsIn;
}

// 0x591D20
void Interior_c::AddGotoPt(int32 x, int32 y, float offsetX, float offsetY) {
    const auto inBounds = x < (int32)m_box->m_width
                       && y < (int32)m_box->m_depth
                       && x >= 0
                       && y >= 0
                       && field_68[x * 0x1E + y] == 3;

    if (field_40C < 16 && (inBounds || GetTileStatus(x, y) == 7)) {
        CVector pos;
        GetTileCentre((float)x + offsetX, (float)y + offsetY, &pos);

        auto& pt = reinterpret_cast<GotoPt_t*>(reinterpret_cast<uintptr_t>(this) + 0x410)[field_40C];
        pt.m_pos   = pos;
        pt.m_tileX = (int8)x;
        pt.m_tileY = (int8)y;

        if (x >= 0 && y >= 0 && x + 1 <= (int32)m_box->m_width && y + 1 <= (int32)m_box->m_depth) {
            auto& tile = field_68[x * 0x1E + y];
            if (tile == 3 || tile == 0) {
                tile = 4;
            }
        }

        field_40C++;
    }
}

// 0x591E40
bool Interior_c::AddInteriorInfo(int32 actionType, float offsetX, float offsetY, int32 direction, CEntity* entityIgnoredCollision) {
    if (m_interiorInfosCount < 16) {
        CVector pos;
        GetTileCentre(offsetX, offsetY, &pos);
        pos.z += StaticRef<float>(0x858C98); // z offset above the floor

        CVector dir;
        if (direction != -1) {
            switch (direction) {
            case 3: dir = CVector{ -1.f,  0.f, 0.f }; break;
            case 1: dir = CVector{  1.f,  0.f, 0.f }; break;
            case 2: dir = CVector{  0.f,  1.f, 0.f }; break;
            case 0: dir = CVector{  0.f, -1.f, 0.f }; break;
            }
            RwV3dTransformPoints(reinterpret_cast<RwV3d*>(&dir), reinterpret_cast<const RwV3d*>(&dir), 1, &m_matrix);
        }

        auto& info = m_interiorInfos[m_interiorInfosCount];
        info.Type                   = (eInteriorInfoType)actionType;
        info.Pos                    = pos;
        info.Dir                    = dir;
        info.IsInUse                = false;
        info.EntityIgnoredCollision = entityIgnoredCollision;

        m_interiorInfosCount++;
        return true;
    }
    return false;
}

// 0x591F90
void Interior_c::AddPickups() {
    // Only try to (re-)spawn pickups every 180 seconds
    if (179999 < static_cast<uint32>(CTimer::m_snTimeInMilliseconds - s_lastPickupTime)) {
        for (int32 attempt = 0, placedCount = 0; attempt < 100; attempt++) {
            if (placedCount > 0) { // Max. 1 pickup per call
                return;
            }

            const auto tileX = CGeneral::GetRandomNumberInRange(0, static_cast<int32>(m_box->m_width));
            const auto tileY = CGeneral::GetRandomNumberInRange(0, static_cast<int32>(m_box->m_depth));

            const auto tileStatus = field_68[tileX * 0x1E + tileY];
            if (tileX < static_cast<int32>(m_box->m_width)
                && tileY < static_cast<int32>(m_box->m_depth)
                && tileX >= 0
                && tileY >= 0
                && (tileStatus == 0 || tileStatus == 3 || tileStatus == 4)
            ) {
                CVector pos;
                GetTileCentre(static_cast<float>(tileX), static_cast<float>(tileY), &pos);

                if (const auto roll = CGeneral::GetRandomNumberInRange(0, 100); roll < 0x4B) {
                    // 75% chance: money pickup with random amount
                    const auto ammo = CGeneral::GetRandomNumberInRange(0, 10);
                    CPickups::GenerateNewOne(pos, ModelIndices::MI_MONEY, PICKUP_MONEY, 10 - ammo);
                } else {
                    // Weapon pickup on top of the tile
                    pos.z += StaticRef<float>(0x858B8C); // Tile size

                    const auto weaponRoll = CGeneral::GetRandomNumberInRange(0, 100);
                    int32 weaponType;
                    if (weaponRoll < 0x28) {
                        weaponType = 5;
                    } else if (weaponRoll < 0x50) {
                        weaponType = 0x16;
                    } else {
                        weaponType = ((0x59 < weaponRoll) - 1 & 0xffffffeb) + 0x19;
                    }

                    const auto ammo = CGeneral::GetRandomNumberInRange(0, 3);
                    CPickups::GenerateNewOne_WeaponType(pos, static_cast<eWeaponType>(weaponType), PICKUP_ONCE, 3 - ammo, false, nullptr);
                }

                placedCount++;
            }
        }
    }
}

// 0x5922C0
// NOTE: Recursive flood fill over all tiles of value 5 starting at (x, y).
//       `visited` is a per-tile int32 flag array (30 x 30)
void Interior_c::FindBoundingBox(int32 x, int32 y, int32* minX, int32* maxX, int32* minY, int32* maxY, int32* visited) {
    int32 yNext = y + 1;
    while (true) {
        // Tile above: (x - 1, y)
        if (x > 0) {
            const auto xPrev = x - 1;
            if (xPrev < static_cast<int32>(static_cast<uint8_t>(m_box->m_width))
                && y < static_cast<int32>(static_cast<uint8_t>(m_box->m_depth))
                && xPrev >= 0
                && y >= 0
                && field_68[xPrev * 0x1E + y] == 5
                && visited[(x - 1) * 0x1E + y] == 0
            ) {
                visited[(x - 1) * 0x1E + y] = 1;
                if (xPrev < *minX) {
                    *minX = xPrev;
                }
                FindBoundingBox(xPrev, y, minX, maxX, minY, maxY, visited);
            }
        }

        // Tile to the right: (x, y + 1)
        if (y < 0x1D
            && x < static_cast<int32>(static_cast<uint8_t>(m_box->m_width))
            && yNext < static_cast<int32>(static_cast<uint8_t>(m_box->m_depth))
            && x >= 0
            && yNext >= 0
            && field_68[yNext + x * 0x1E] == 5
            && visited[x * 0x1E + y + 1] == 0
        ) {
            visited[x * 0x1E + y + 1] = 1;
            if (*maxY < yNext) {
                *maxY = yNext;
            }
            FindBoundingBox(x, yNext, minX, maxX, minY, maxY, visited);
        }

        // Tile below: (x + 1, y)
        if (x < 0x1D) {
            const auto xNext = x + 1;
            if (xNext < static_cast<int32>(static_cast<uint8_t>(m_box->m_width))
                && y < static_cast<int32>(static_cast<uint8_t>(m_box->m_depth))
                && xNext >= 0
            ) {
                if (y < 0) {
                    return;
                }
                if (field_68[xNext * 0x1E + y] == 5 && visited[xNext * 0x1E + y] == 0) {
                    visited[xNext * 0x1E + y] = 1;
                    if (*maxX < xNext) {
                        *maxX = xNext;
                    }
                    FindBoundingBox(xNext, y, minX, maxX, minY, maxY, visited);
                }
            }
        }

        // Tile to the left: (x, y - 1) -> continue the loop upwards
        if (y < 1) {
            return;
        }
        if (static_cast<int32>(static_cast<uint8_t>(m_box->m_width)) <= x) {
            return;
        }
        const auto yPrev = yNext - 2;
        if (static_cast<int32>(static_cast<uint8_t>(m_box->m_depth)) <= yPrev) {
            return;
        }
        if (x < 0) {
            return;
        }
        if (yPrev < 0) {
            return;
        }
        if (field_68[yPrev + x * 0x1E] != 5) {
            return;
        }
        if (visited[x * 0x1E + yPrev] != 0) {
            return;
        }
        visited[x * 0x1E + yPrev] = 1;
        if (yPrev < *minY) {
            *minY = yPrev;
        }
        y     = y - 1;
        yNext = yNext - 1;
    }
}

// 0x5924A0
void Interior_c::CalcExitPts() {
    static auto& kTileSize = StaticRef<float>(0x858B8C); // Tile size
    static auto& kTileHalf = StaticRef<float>(0x858C84); // Half tile size

    // Byte at absolute offset `off` of `this`
    // NOTE: Offsets 0x3F0-0x435 form an array of 8 records { int8 x; int8 y; <14 more bytes> }
    //       (they overlap the m_nodeAddress / field_3F4 / field_3F8 fields of the header),
    //       0x40C/0x40D is a record count.
    const auto i8 = [this](uint32 off) -> int8_t& {
        return *reinterpret_cast<int8_t*>(reinterpret_cast<char*>(this) + off);
    };

    // Exit point infos stored at 0x512, stride 0x20, one entry per wall (Left, Right, Top, Bottom)
    const auto exitPt = [this](uint32 i) -> ExitPt_t& {
        return *reinterpret_cast<ExitPt_t*>(reinterpret_cast<char*>(this) + 0x512 + i * 0x20);
    };

    const int8_t recordCount = i8(0x40C);

    { // Left wall door
        const auto door = m_box->m_door; // +0x5
        if (door >= 0) {
            float y;
            auto& L = exitPt(0);

            const float x = static_cast<float>(door) - kTileSize;
            if (recordCount < 3) {
                L.tileIdx1 = -1;
                L.tileIdx2 = -1;
                y = 0.0f;
            } else {
                // Find first (even) record whose x is past the door position
                int32 idx = 0;
                const int8_t* rec = &i8(0x410); // record #2
                if (recordCount >= 1) {
                    while (idx < recordCount) {
                        if (x < static_cast<float>(*rec)) {
                            break;
                        }
                        idx += 2;
                        rec += 0x20;
                    }
                }

                if (idx == 0) {
                    L.tileIdx1 = 0;
                    L.tileIdx2 = -1;
                } else if (idx == recordCount) {
                    L.tileIdx1 = recordCount - 2;
                    L.tileIdx2 = -1;
                } else {
                    L.tileIdx1 = static_cast<int8_t>(idx);
                    L.tileIdx2 = static_cast<int8_t>(idx - 2);
                }

                y = static_cast<float>(i8(0x411)) - kTileHalf;
            }

            GetTileCentre(x, y, &L.pos);
            GetTileCentre(x, -0.25f, &L.outPos);
        }
    }

    { // Left door end -> right side
        const auto doorStart = m_box->m_lDoorStart; // +0x6
        if (doorStart >= 0) {
            const float x = (static_cast<float>(m_box->m_lDoorEnd - doorStart) * kTileSize + static_cast<float>(doorStart)) - kTileSize;

            float y;
            auto& R = exitPt(1);

            if (recordCount < 3) {
                R.tileIdx1 = -1;
                R.tileIdx2 = -1;
                y = 0.0f;
            } else {
                if (static_cast<float>(i8(0x411)) <= x) {
                    if (x <= static_cast<float>(i8(0x421))) {
                        R.tileIdx2 = 1;
                        R.tileIdx1 = 0;
                    } else {
                        R.tileIdx1 = 1;
                        R.tileIdx2 = -1;
                    }
                } else {
                    R.tileIdx2 = -1;
                    R.tileIdx1 = 0;
                }
                y = static_cast<float>(i8(0x410)) - kTileHalf;
            }

            GetTileCentre(y, x, &R.pos);      // Note: (x, y) swapped
            GetTileCentre(-0.25f, x, &R.outPos);
        }
    }

    { // Top wall door
        const auto doorStart = m_box->m_tDoorStart; // +0xA
        if (doorStart >= 0) {
            const float x = (static_cast<float>(m_box->m_tDoorEnd - doorStart) * kTileSize + static_cast<float>(doorStart)) - kTileSize;

            float y;
            auto& T = exitPt(2);

            if (recordCount < 3) {
                T.tileIdx1 = -1;
                T.tileIdx2 = -1;
                y = static_cast<float>(static_cast<uint8_t>(m_box->m_depth) - 1);
            } else {
                // Find first (odd) record whose x is past the door position.
                // NOTE: Decomp has a `recordCount < 2` special case and an `idx != 0` check here,
                //       both are dead code (recordCount >= 3 and idx starts at 1).
                int32 idx = 1;
                const int8_t* rec = &i8(0x420); // record #3
                do {
                    if (x < static_cast<float>(*rec)) {
                        break;
                    }
                    idx += 2;
                    rec += 0x20;
                } while (idx < recordCount);

                if (idx == recordCount) {
                    T.tileIdx1 = recordCount - 1;
                    T.tileIdx2 = -1;
                } else {
                    T.tileIdx1 = static_cast<int8_t>(idx);
                    T.tileIdx2 = static_cast<int8_t>(idx - 2);
                }

                y = static_cast<float>(i8(0x421)) + kTileHalf;
            }

            GetTileCentre(x, y, &T.pos);
            GetTileCentre(x, static_cast<float>(static_cast<uint8_t>(m_box->m_depth) - 1) + kTileHalf, &T.outPos);
        }
    }

    { // Right wall door
        const auto doorStart = m_box->m_rDoorStart; // +0x8
        if (doorStart >= 0) {
            const float x = (static_cast<float>(m_box->m_rDoorEnd - doorStart) * kTileSize + static_cast<float>(doorStart)) - kTileSize;

            float y;
            auto& B = exitPt(3);

            if (recordCount < 3) {
                B.tileIdx1 = -1;
                B.tileIdx2 = -1;
                y = static_cast<float>(static_cast<uint8_t>(m_box->m_width) - 1);
            } else {
                const int8_t* lastRec = &i8(0x3F0 + recordCount * 0x10); // record[recordCount]
                if (static_cast<float>(lastRec[0x1]) <= x) {
                    if (x <= static_cast<float>(lastRec[0x11])) {
                        B.tileIdx1 = recordCount - 1;
                        B.tileIdx2 = recordCount - 2;
                    } else {
                        B.tileIdx1 = recordCount - 1;
                        B.tileIdx2 = -1;
                    }
                } else {
                    B.tileIdx1 = recordCount - 2;
                    B.tileIdx2 = -1;
                }
                y = static_cast<float>(lastRec[0x0]) + kTileHalf;
            }

            GetTileCentre(y, x, &B.pos); // Note: (x, y) swapped
            GetTileCentre(static_cast<float>(static_cast<uint8_t>(m_box->m_width) - 1) + kTileHalf, x, &B.outPos);
        }
    }
}

// 0x5929F0
bool Interior_c::IsVisible() {
    // Either the player entity's position (at +0x30) or a global fallback vector
    CVector posOfInterest;
    if (const auto entity = StaticRef<CEntity*>(0xB6F03C)) {
        posOfInterest = *reinterpret_cast<const CVector*>(reinterpret_cast<uintptr_t>(entity) + 0x30);
    } else {
        posOfInterest = StaticRef<CVector>(0xB6F02C);
    }

    if (IsPtInside(posOfInterest, CVector{ 5.0f, 5.0f, 0.0f })) {
        return true;
    }

    if (m_box->m_door > 0) {
        const float dx = posOfInterest.x - m_position.x;
        const float dy = posOfInterest.y - m_position.y;
        if (dx * dx + dy * dy < 900.0f /* 0x858628 */) {
            return true;
        }
    }
    return false;
}

// 0x592AA0
// NOTE: decomp shows this RETURNS the created entity (CEntity*), with the two out-params
// receiving the (possibly rotated) furniture footprint.
CEntity* Interior_c::PlaceFurniture(Furniture_c* furniture, int32 x, int32 y, float a4, int32 a5, int32 dir, int32* outA, int32* outB, uint8 a9) {
    const float tileSize = TILE_SIZE;

    if (NumFreeFurnitureEntitySlots() < 1) {
        if (outA) { *outA = 0; }
        if (outB) { *outB = 0; }
        return nullptr;
    }

    // Furniture footprint, rotated for dir 1/3 (swapped X/Y)
    uint32 dimX, dimY;
    if (dir == 1 || dir == 3) {
        dimX = (uint32)(uint8)furniture->m_nWidthY;
        dimY = (uint32)(uint8)furniture->m_nWidthX;
    } else {
        dimX = (uint32)(uint8)furniture->m_nWidthX;
        dimY = (uint32)(uint8)furniture->m_nWidthY;
    }

    if (a5 == 1 && !a9) {
        if (!CheckTilesEmpty(x, y, dimX, dimY, furniture->m_bCanPlaceInFrontOfWindow)) {
            if (outA) { *outA = 0; }
            if (outB) { *outB = 0; }
            return nullptr;
        }
    }

    // Rotation matrix for "tall" furniture: T(+off) * Rz(rand +-maxAng) * T(-off)
    CMatrix rotMat;
    if (furniture->m_nMaxAng == 0) {
        rotMat.SetUnity();
    } else {
        const auto offX = tileSize - (float)dimX * tileSize;
        const auto offY = tileSize - (float)dimY * tileSize;

        CMatrix m1; m1.SetUnity(); m1.SetTranslate(CVector{ offX, offY, 0.f });
        CMatrix m2; m2.SetUnity();
        m2.RotateZ((float)CGeneral::GetRandomNumberInRange(-(int32)(uint8)furniture->m_nMaxAng, (int32)(uint8)furniture->m_nMaxAng) * INT_DEG_TO_RAD, false);
        CMatrix m3; m3.SetUnity(); m3.SetTranslate(CVector{ -offX, -offY, 0.f });
        rotMat = m1 * m2 * m3;
    }

    // Interior rotation: quarter turns (dir * PI/2), attached to the interior matrix
    CMatrix intMat;
    intMat.Attach(&m_matrix, false);
    intMat.RotateZ((float)dir * PI_HALF, false);

    // Offset inside the footprint, depends on quarter turn
    uint32 offX = 0, offY = 0;
    if (dir == 1) {
        offX = dimX - 1;
    } else if (dir == 2) {
        offX = dimX - 1;
        offY = dimY - 1;
    } else if (dir == 3) {
        offY = dimY - 1;
    }

    // World position (relative to interior origin effect)
    CVector pos{};
    if (a5 == 1 || a5 == 2) {
        pos.x += -(float)(uint8)m_box->m_width * tileSize + (float)offX + (float)x + tileSize;
        pos.y += -(float)(uint8)m_box->m_depth * tileSize + (float)offY + (float)y + tileSize;
        pos.z += -(float)(uint8)m_box->m_height * tileSize + a4;
    } else if (a5 == 0) {
        pos.x += -(float)(uint8)m_box->m_width * tileSize + (float)offX + (float)x + tileSize;
        pos.y += -(float)(uint8)m_box->m_depth * tileSize + (float)offY + (float)y + tileSize;
        pos.z += (float)(uint8)m_box->m_height * tileSize - a4;
    }

    // final = rotMat * intMat
    CMatrix objMat = rotMat * intMat;

    if (const auto slot = TakeFurnitureEntitySlot()) {
        auto* entity = new CEntity();
        slot->m_entity = entity;
        entity->SetModelIndex(furniture->m_nModelId);
        entity->SetMatrix(objMat);
        entity->SetAreaCode((eAreaCodes)m_areaCode);
        entity->m_nFlags |= 0x10000;  // bDontCastShadowsOn
        entity->m_nFlags |= 0x400000; // bIsTempBuilding
        CWorld::Add(entity);
        slot->m_tileX = (uint16)x;
        slot->m_tileY = (uint16)y;
        CommitFurnitureEntitySlot(slot);

        if (a5 == 1) {
            // Mark tiles as occupied; tall furniture uses status 6, otherwise 5
            SetTilesStatus(x, y, dimX, dimY, furniture->m_bIsTall ? 6 : 5, 1);
        }

        if (outA) { *outA = (int32)dimX; }
        if (outB) { *outB = (int32)dimY; }
        return entity;
    }

    if (outA) { *outA = 0; }
    if (outB) { *outB = 0; }
    return nullptr;
}

// 0x593120
// NOTE: decomp returns the placed entity - signature corrected from `void` to `CEntity*`.
CEntity* Interior_c::PlaceFurnitureOnWall(int32 furnitureGroupId, int32 furnitureSubgroupId, int32 furnitureId, float a5, int32 a6, int32 dir, int32 a8, int32 offset, int32* outDir, int32* outA, int32* outX, int32* outY, int32* outTileX, int32* outTileY) {
    const auto* furniture = g_furnitureMan.GetFurniture(furnitureGroupId, furnitureSubgroupId, furnitureId, m_box->m_status);
    int32 attempts = 100;
    if (dir != -1 && a8 != -1) {
        attempts = 1;
    }

    if (!furniture || attempts == 0) {
        return nullptr;
    }

    for (auto i = 0; i < attempts; i++) {
        auto d = dir;
        if (d == -1) {
            rand(); // _rand() - discarded
            d = CGeneral::GetRandomNumber();
        }

        int32 x, y;
        if (d == 1 || d == 3) {
            auto off = a8;
            if (off == -1) {
                rand(); // _rand() - discarded
                off = CGeneral::GetRandomNumber();
            }
            y = off;
            x = (d != 1)
              ? (int32)((uint8)m_box->m_width - (uint8)furniture->m_nWidthY) - offset
              : offset;
        } else {
            auto off = a8;
            if (off == -1) {
                rand(); // _rand() - discarded
                off = CGeneral::GetRandomNumber();
            }
            x = off;
            y = (d != 2)
              ? (int32)((uint8)m_box->m_depth - (uint8)furniture->m_nWidthY) - offset
              : offset;
        }

        int32 outA_, outB_;
        if (auto* entity = PlaceFurniture(const_cast<Furniture_c*>(furniture), x, y, a5, a6, d, &outA_, &outB_, 0)) {
            if (outDir)   { *outDir = d; }
            if (outA)     { *outA = a8; }
            if (outX)     { *outX = x; }
            if (outY)     { *outY = y; }
            if (outTileX) { *outTileX = outA_; }
            if (outTileY) { *outTileY = outB_; }
            return entity;
        }
    }
    return nullptr;
}

// 0x593340
// NOTE: decomp returns the placed entity - signature corrected from `void` to `CEntity*`.
CEntity* Interior_c::PlaceFurnitureInCorner(int32 furnitureGroupId, int32 furnitureSubgroupId, int32 id, float a4, int32 a5, int32 a6, int32 dir, int32* outDir, int32* outX, int32* outY, int32* outA, int32* outB) {
    const auto* furniture = g_furnitureMan.GetFurniture(furnitureGroupId, furnitureSubgroupId, id, m_box->m_status);
    uint32 attempts = 0x14;
    if (dir != -1) {
        attempts = 1;
    }

    if (furniture && attempts != 0) {
        for (uint32 i = 0; i < attempts; i++) {
            auto d = dir;
            if (d == -1) {
                rand(); // _rand() - discarded
                d = CGeneral::GetRandomNumber();
            }

            // Corner position
            uint32 x, y;
            if (d == 1) {
                x = 0;
                y = (uint32)(uint8)a6;
            } else if (d == 3) {
                y = (uint32)((uint8)m_box->m_width - (uint8)furniture->m_nWidthY) - (uint32)(uint8)a6;
                x = (uint32)((uint8)m_box->m_depth - (uint8)furniture->m_nWidthX);
            } else if (d == 2) {
                y = (uint32)((uint8)m_box->m_width - (uint8)furniture->m_nWidthX);
                x = (uint32)(uint8)a6;
            } else { // d == 0
                y = 0;
                x = (uint32)((uint8)m_box->m_depth - (uint8)furniture->m_nWidthY) - (uint32)(uint8)a6;
            }

            int32 outTileX, outTileY;
            if (auto* entity = PlaceFurniture(const_cast<Furniture_c*>(furniture), (int32)y, (int32)x, a4, a5, d, &outTileX, &outTileY, 0)) {
                if (outDir) { *outDir = d; }
                if (outX)   { *outX = (int32)y; }
                if (outY)   { *outY = (int32)x; }
                if (outA)   { *outA = outTileX; }
                if (outB)   { *outB = outTileY; }
                return entity;
            }
        }
    }
    return nullptr;
}

// 0x591C50
bool Interior_c::FindEmptyTiles(int32 width, int32 depth, int32* outX, int32* outY) {
    for (int32 attempt = 0; attempt < 100; attempt++) {
        const auto x = CGeneral::GetRandomNumberInRange(0, width);
        const auto y = CGeneral::GetRandomNumberInRange(0, depth);

        if (CheckTilesEmpty(x, y, width, depth, 1)) {
            *outX = x;
            *outY = y;
            return true;
        }
    }
    return false;
}

// 0x59A790
void Interior_c::FurnishShop(int32 a2) {
    m_furnitureGroupId = (int8)a2;

    // Only furnish if the door isn't too close to either side wall
    const auto door = m_box->m_door;
    if (5 < door - 1 || 5 < m_box->m_width - door) {
        // Block out the four corner tiles
        SetTilesStatus(0, 0, 1, 1, 2, 0);
        SetTilesStatus(0, m_box->m_depth - 1, 1, 1, 2, 0);
        SetTilesStatus(m_box->m_width - 1, 0, 1, 1, 2, 0);
        SetTilesStatus(m_box->m_width - 1, m_box->m_depth - 1, 1, 1, 2, 0);

        Shop_PlaceFixedUnits();
        Shop_FurnishEdges();
        Shop_FurnishAisles();
    }
}
