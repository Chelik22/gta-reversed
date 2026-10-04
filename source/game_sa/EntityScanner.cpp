#include "StdInc.h"

#include "EntityScanner.h"

void CEntityScanner::InjectHooks() {
    RH_ScopedClass(CEntityScanner);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x5FF990);
    RH_ScopedInstall(Destructor, 0x603480);

    RH_ScopedInstall(Clear, 0x5FF9D0);
    // NOTSA: Hook the non-virtual impl! See `ScanForEntitiesInRangeImpl` for why.
    RH_ScopedInstall(ScanForEntitiesInRangeImpl, 0x5FFA20);
}

// 0x5FFA20 (virtual entry)
void CEntityScanner::ScanForEntitiesInRange(const eRepeatSectorList sectorList, const CPed& ped) {
    ScanForEntitiesInRangeImpl(sectorList, ped);
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

// 0x5FFA20 (body)
void CEntityScanner::ScanForEntitiesInRangeImpl(const eRepeatSectorList sectorList, const CPed& ped) {
    if (!m_timer.Tick()) { // Only scan once every `Period` invocations
        return;
    }

    Clear();

    const auto range = std::max(ped.GetIntelligence()->m_fHearingRange, ped.GetIntelligence()->m_fSeeingRange);
    const auto& pedPos = ped.GetPosition();

    // Distances of the currently found entities (parallel array to `m_apEntities`, sorted by distance)
    float dists[MAX_NUM_ENTITIES];
    std::ranges::fill(dists, std::numeric_limits<float>::max());

    int32 numFound{};

    const auto SectorOfLow = [&](float v) {
        const auto sector = (int32)floorf((v - range) * (1.0f / 50.0f) + 60.0f);
        return sector < 1 ? 0u : (uint32)sector;
    };
    const auto SectorOfHigh = [&](float v) {
        const auto sector = (int32)floorf((v + range) * (1.0f / 50.0f) + 60.0f);
        return sector < 0x77 ? (uint32)sector : 0x77u;
    };
    const auto startX = SectorOfLow(pedPos.x);
    const auto startY = SectorOfLow(pedPos.y);
    const auto endX   = SectorOfHigh(pedPos.x);
    const auto endY   = SectorOfHigh(pedPos.y);

    // Advance (or reset) the scan code used to mark already visited entities
    CWorld::AdvanceCurrentScanCode();
    const auto scanCode = CWorld::ms_nCurrentScanCode;
    const_cast<CPed&>(ped).SetScanCode(scanCode);

    for (auto sectorY = startY; sectorY <= endY; sectorY++) {
        for (auto sectorX = startX; sectorX <= endX; sectorX++) {
            auto& sector = CWorld::GetRepeatSector(sectorX & 0xF, sectorY & 0xF);

            const auto ScanList = [&](auto& list) {
            for (auto entity : list) {
                if (entity->IsScanCodeCurrent()) {
                    continue;
                }
                entity->SetCurrentScanCode();
                // Skip dead peds (unless we are scanning for the player)
                if (sectorList == REPEATSECTOR_PEDS
                    && &ped != FindPlayerPed()
                    && entity->AsPed()->m_nPedState == PEDSTATE_DEAD
                ) {
                    continue;
                }

                const auto& entPos = entity->GetPosition();
                const auto distSq = (entPos - pedPos).SquaredMagnitude();
                if (distSq >= sq(range)) {
                    continue;
                }

                // Insert into the (sorted) entity array, shifting entities further back
                int32 i{};
                bool inserted{ false };
                for (; i < MAX_NUM_ENTITIES; i++) {
                    if (!m_apEntities[i]) { // Empty slot, just append
                        dists[i]         = distSq;
                        m_apEntities[i]  = entity;
                        numFound++;
                        inserted = true;
                        break;
                    }
                    if (distSq < dists[i]) { // Insert at `i`
                        for (int32 j = (int32)MAX_NUM_ENTITIES - 1; j > i; j--) {
                            m_apEntities[j] = m_apEntities[j - 1];
                            dists[j]        = dists[j - 1];
                        }
                        dists[i]        = distSq;
                        m_apEntities[i] = entity;
                        if (numFound < MAX_NUM_ENTITIES) {
                            numFound++;
                        }
                        inserted = true;
                        break;
                    }
                }
                (void)inserted;
            }
            };

            switch (sectorList) {
            case REPEATSECTOR_VEHICLES: ScanList(sector.Vehicles); break;
            case REPEATSECTOR_PEDS:     ScanList(sector.Peds);     break;
            case REPEATSECTOR_OBJECTS:  ScanList(sector.Objects);  break;
            }
        }
    }

    // Register references for all found entities
    if (numFound > 0) {
        for (auto& entity : m_apEntities) {
            if (entity) {
                CEntity::RegisterReference(entity);
            }
        }
    }

    // Store the closest entity
    if (m_apEntities[0]) {
        CEntity::SetEntityReference(m_pClosestEntityInRange, m_apEntities[0]);
    }
}
