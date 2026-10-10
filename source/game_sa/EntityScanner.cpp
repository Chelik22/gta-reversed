#include "StdInc.h"

#include "EntityScanner.h"

void CEntityScanner::InjectHooks() {
    RH_ScopedClass(CEntityScanner);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x5FF990);
    RH_ScopedInstall(Destructor, 0x603480);

    RH_ScopedInstall(Clear, 0x5FF9D0);
    RH_ScopedInstall(ScanForEntitiesInRange, 0x5FFA20);
}

// 0x5FF990
CEntityScanner::CEntityScanner() {
    m_timer = {};

    rng::fill(m_apEntities, nullptr);
    m_pClosestEntityInRange = nullptr;

    m_timer.SetPeriod(MAX_NUM_ENTITIES);
}

// 0x603480
CEntityScanner::~CEntityScanner() {
    Clear();
}

// 0x5FF9D0
void CEntityScanner::Clear() {
    for (auto& entity : m_apEntities) {
        CEntity::ClearReference(entity);
    }

    CEntity::ClearReference(m_pClosestEntityInRange);
}

// 0x5FFA20
// NOTE (0x5FFCEA): Dead peds are only skipped when the scanning ped is NOT the player
// (the flag is `FindPlayerPed() == &ped`, see 0x5FFA76 - 0x5FFA78; NOT a hearing/seeing comparison).
void CEntityScanner::ScanForEntitiesInRange(const eRepeatSectorList sectorList, const CPed& ped) {
    if (!m_timer.Tick()) {
        return;
    }

    Clear();

    const auto  playerPed    = FindPlayerPed();
    const auto  intel        = ped.GetIntelligence();
    const float scanRange    = std::max(intel->m_fHearingRange, intel->m_fSeeingRange);
    const bool  scanIsPlayer = playerPed == &ped;
    const auto& scanPos      = ped.GetPosition();

    std::array<float, MAX_NUM_ENTITIES> dists;
    dists.fill(std::numeric_limits<float>::max()); // 0x7F7FFFFF (FLT_MAX)

    const auto startX = std::max((int)((scanPos.x - scanRange) * 0.02f /* 0x858B38 */ + 60.0f /* 0x858B34 */), 0);
    const auto startY = std::max((int)((scanPos.y - scanRange) * 0.02f + 60.0f), 0);
    const auto endX   = std::min((int)((scanPos.x + scanRange) * 0.02f + 60.0f), 119); // 0x77
    const auto endY   = std::min((int)((scanPos.y + scanRange) * 0.02f + 60.0f), 119);

    if (CWorld::ms_nCurrentScanCode == 0xFFFFu) {
        plugin::Call<0x563470>(); // CWorld::ClearScanCodes (protected in World.h; precedent Plane.cpp:563)
        CWorld::ms_nCurrentScanCode = 1;
    } else {
        CWorld::ms_nCurrentScanCode++;
    }
    const_cast<CPed&>(ped).SetCurrentScanCode(); // m_ScanCode (CEntity+0x2C) = ms_nCurrentScanCode (OG writes it)

    auto numEntities = 0;

    const auto ScanList = [&](auto&& list) {
        for (auto* const entity : list) {
            if (entity->IsScanCodeCurrent()) {
                continue;
            }
            entity->SetCurrentScanCode();

            if (sectorList == REPEATSECTOR_PEDS
                && !scanIsPlayer
                && entity->AsPed()->m_nPedState == PEDSTATE_DEAD /* 0x37 */
            ) {
                continue;
            }

            const auto& entPos = entity->GetPosition();
            const auto  distSq = (entPos.y - scanPos.y) * (entPos.y - scanPos.y)
                               + (entPos.x - scanPos.x) * (entPos.x - scanPos.x)
                               + (entPos.z - scanPos.z) * (entPos.z - scanPos.z);
            if (distSq >= scanRange * scanRange) {
                continue;
            }

            // Insert sorted by distance (0x5FFD0E..0x5FFD6E)
            for (auto slot = 0u; slot < MAX_NUM_ENTITIES; slot++) {
                if (!m_apEntities[slot]) { // Empty slot - insert directly
                    dists[slot]       = distSq;
                    m_apEntities[slot] = entity;
                    numEntities++;
                    break;
                }
                if (distSq < dists[slot]) { // Insert here, shift the tail right
                    for (auto j = std::min<size_t>(numEntities, MAX_NUM_ENTITIES - 1); j > slot; j--) {
                        m_apEntities[j] = m_apEntities[j - 1];
                        dists[j]        = dists[j - 1];
                    }
                    dists[slot]        = distSq;
                    m_apEntities[slot] = entity;
                    if (numEntities < MAX_NUM_ENTITIES) {
                        numEntities++;
                    }
                    break;
                }
            }
        }
    };

    for (auto y = startY; y <= endY; y++) {
        if (startX > endX) {
            continue;
        }
        for (auto x = startX; x <= endX; x++) {
            const auto& sector = CWorld::ms_aRepeatSectors[y & 0xF][x & 0xF];
            switch (sectorList) {
            case REPEATSECTOR_VEHICLES: ScanList(sector.Vehicles); break;
            case REPEATSECTOR_PEDS:     ScanList(sector.Peds); break;
            default:                    ScanList(sector.Objects); break;
            }
        }
    }

    for (auto i = 0; i < numEntities; i++) {
        if (m_apEntities[i]) {
            CEntity::SafeRegisterRef(m_apEntities[i]); // 0x571B70
        }
    }
    if (m_apEntities[0]) {
        m_pClosestEntityInRange = m_apEntities[0];
        CEntity::SafeRegisterRef(m_pClosestEntityInRange);
    }
}
