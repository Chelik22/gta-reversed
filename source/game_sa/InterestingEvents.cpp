#include "StdInc.h"

#include "InterestingEvents.h"
#include "Pools/Pools.h"
#include "Entity/Ped/Ped.h"
#include "Entity/Vehicle/Vehicle.h"
#include "Entity/Object/Object.h"
#include "Entity/Building.h"

auto& g_InterestingEvents = StaticRef<CInterestingEvents>(0xC0B058);

/*
 * Commented hooks aren't tested.
 * */
void CInterestingEvents::InjectHooks() {
    RH_ScopedClass(CInterestingEvents);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x6023A0);
    RH_ScopedInstall(Destructor, 0x856880);
    RH_ScopedInstall(Add, 0x602590);
    RH_ScopedInstall(ScanForNearbyEntities, 0x605A30);
    RH_ScopedInstall(GetInterestingEvent, 0x6028A0);
    RH_ScopedInstall(InvalidateEvent, 0x602960);
    RH_ScopedInstall(InvalidateNonVisibleEvents, 0x6029C0);
}

// 0x6023A0
CInterestingEvents::CInterestingEvents()
{
    m_nFlags = 0;
    m_b2 = true;
    m_b4 = true;
    m_b8 = true;

    /* Everything missing from here is initialized in the header */

    const auto SetOptions = [=](auto index, auto priority, auto delay, uint32 end = 0) {
        m_nPriorities[index] = priority;
        m_nDelays[index]     = delay;
        m_nEndsOfTime[index] = end;
    };

    SetOptions(INTERESTING_EVENT_0,     5,  2000);
    SetOptions(PEDS_CHATTING,           1,  5000);
    SetOptions(INTERESTING_EVENT_2,     1,  5000);
    SetOptions(INTERESTING_EVENT_3,     1,  5000);
    SetOptions(INTERESTING_EVENT_4,     2,  3000);
    SetOptions(INTERESTING_EVENT_5,     2,  3000);
    SetOptions(INTERESTING_EVENT_6,     2,  3000);
    SetOptions(INTERESTING_EVENT_7,     2,  3000);
    SetOptions(INTERESTING_EVENT_8,     4,  3000);
    SetOptions(INTERESTING_EVENT_9,     4,  3000);
    SetOptions(INTERESTING_EVENT_10,    5,  6000);
    SetOptions(INTERESTING_EVENT_11,    6,  6000);
    SetOptions(INTERESTING_EVENT_12,    6,  8000);
    SetOptions(INTERESTING_EVENT_13,    6,  5000);
    SetOptions(INTERESTING_EVENT_14,    5,  6000);
    SetOptions(INTERESTING_EVENT_15,    9,  6000);
    SetOptions(INTERESTING_EVENT_16,    9,  6000);
    SetOptions(VEHICLE_DAMAGE,          8,  6000);
    SetOptions(INTERESTING_EVENT_18,    7,  6000);
    SetOptions(INTERESTING_EVENT_19,    6,  5000);
    SetOptions(INTERESTING_EVENT_20,    7,  6000);
    SetOptions(INTERESTING_EVENT_21,    8,  8000);
    SetOptions(INTERESTING_EVENT_22,    9,  5000);
    SetOptions(GANG_ATTACKING_PED,      9,  6000);
    SetOptions(GANG_FIGHT,              9,  6000);
    SetOptions(INTERESTING_EVENT_25,    9,  6000);
    SetOptions(ZELDICK_OCCUPATION,      9,  8000);
    SetOptions(EVENT_ATTRACTOR,         10, 4000);
    SetOptions(INTERESTING_EVENT_28,    10, 4000);
}

CInterestingEvents* CInterestingEvents::Constructor() {
    this->CInterestingEvents::CInterestingEvents();
    return this;
}

// 0x856880
CInterestingEvents::~CInterestingEvents() {
    for (auto& event : m_Events) {
        CEntity::ClearReference(event.entity);
    }
}

CInterestingEvents* CInterestingEvents::Destructor() {
    this->CInterestingEvents::~CInterestingEvents();
    return this;
}

// 0x602590
void CInterestingEvents::Add(CInterestingEvents::EType type, CEntity* entity) {
    if (!m_b1 || !entity)
        return;

    const auto& camPos = CCamera::GetActiveCamera().m_vecSource;

    if (m_CurrentFrameCounter != CTimer::GetFrameCounter()) {
        m_CurrentFrameCounter = CTimer::GetFrameCounter();

        const auto player = FindPlayerPed();

        // Direction from the camera to the player (2D only)
        CVector dir = player->GetPosition() - camPos;
        dir.z = 0.f;
        vec148 = dir;

        if (vec148.NormaliseAndMag() == 0.f) {
            vec148 = player->GetMatrix().GetForward();
        }
        m_vecCenter = player->GetPosition() + vec148 * m_fRadius;
    }

    const auto evPos = entity->GetPosition();
    const CVector2D dist{ m_vecCenter.x - evPos.x, m_vecCenter.y - evPos.y };
    if (dist.SquaredMagnitude() > m_fRadius * m_fRadius) {
        return; // Entity outside the interesting-event circle
    }

    if (m_b2) {
        // Entity must be in front of the half space defined by `vec148`
        if (DotProduct(vec148, evPos - camPos) < 0.f) {
            return;
        }
    }

    if (!CWorld::GetIsLineOfSightClear(camPos, evPos, true, false, false, false, false, true, false)) {
        return;
    }

    const auto now = CTimer::GetTimeInMS();

    // Find a free / replaceable slot
    int32 slotIdx{};
    for (; slotIdx < MAX_INTERESTING_EVENTS; slotIdx++) {
        auto& ev = m_Events[slotIdx];
        if (!ev.entity) {
            break; // free slot
        }

        const bool replaceable =
            ev.type == INTERESTING_EVENT_0
            || (
                (m_nPriorities[ev.type] <= m_nPriorities[type]
                    || (uint32)(m_nDelays[ev.type] + ev.time) < now)
                && (uint32)m_nEndsOfTime[type] < now
                && m_nInterestingEvent != slotIdx
            );
        if (replaceable) {
            break;
        }
    }
    if (slotIdx >= MAX_INTERESTING_EVENTS) {
        return; // no free slot
    }

    auto& ev = m_Events[slotIdx];
    CEntity::SafeCleanUpRef(ev.entity);
    ev.type   = type;
    ev.entity = entity;
    ev.time   = now;
    entity->RegisterReference(&ev.entity);

    if (m_b8) {
        m_nEndsOfTime[type] = m_nDelays[type] / 2 + now;
    } else {
        m_nEndsOfTime[type] = now;
    }
}

// 0x605A30
void CInterestingEvents::ScanForNearbyEntities() {
    if (!m_b1)
        return;

    const auto now = CTimer::GetTimeInMS();
    if (now - m_nLastScanTimeUpdate <= 499)
        return;
    m_nLastScanTimeUpdate = now;

    const auto player = FindPlayerPed();

    if (m_CurrentFrameCounter != CTimer::GetFrameCounter()) {
        m_CurrentFrameCounter = CTimer::GetFrameCounter();

        // (2D) direction from the active camera to the player
        CVector dir = player->GetPosition() - TheCamera.GetActiveCam().m_vecSource;
        dir.z = 0.f;
        vec148 = dir;

        if (vec148.NormaliseAndMag() == 0.f) { // 0x59C970
            dir = player->GetMatrix().GetForward();
        }
        m_vecCenter = player->GetPosition() + dir * m_fRadius;
    }

    // Compute the range of (repeat) sectors that cover [center - radius, center + radius] on both axes
    const auto SectorOfLow = [](float v) {
        const auto sector = (int32)floorf(v * (1.0f / 50.0f) + 60.0f); // 1/50 @ 0x858B38, 60.0f @ 0x858B34
        return sector < 1 ? 0u : (uint32)sector;
    };
    const auto SectorOfHigh = [](float v) {
        const auto sector = (int32)floorf(v * (1.0f / 50.0f) + 60.0f);
        return sector < 0x77 ? (uint32)sector : 0x77u;
    };
    const auto startX = SectorOfLow(m_vecCenter.x - m_fRadius);
    const auto startY = SectorOfLow(m_vecCenter.y - m_fRadius);
    const auto endX   = SectorOfHigh(m_vecCenter.x + m_fRadius);
    const auto endY   = SectorOfHigh(m_vecCenter.y + m_fRadius);

    // Advance (or reset) the scan code used to mark already visited entities
    CWorld::AdvanceCurrentScanCode(); // Original: if == -1 { ClearScanCodes (0x563470); = 1 } else ++
    player->SetScanCode(CWorld::ms_nCurrentScanCode);

    for (auto sectorY = startY; sectorY <= endY; sectorY++) {
        if (startX > endX) {
            continue;
        }
        for (auto sectorX = startX; sectorX <= endX; sectorX++) {
            auto& sector = CWorld::GetRepeatSector(sectorX & 0xF, sectorY & 0xF);

            // Add interesting events for notable peds (cops, gang members, criminals, whores).
            // If the ped is in a vehicle the event is registered for the vehicle instead.
            for (auto* ped : sector.Peds) {
                if (!ped->IsScanCodeCurrent()) {
                    ped->SetCurrentScanCode();
                    if (ped->m_nPedState != PEDSTATE_PASSENGER) {
                        CEntity* subject;
                        if (ped->bInVehicle && ped->m_pVehicle) {
                            subject = ped->m_pVehicle;
                        } else {
                            subject = ped;
                        }
                        bool knownType{ true };
                        EType evType{};
                        switch (ped->m_nPedType) {
                        case PED_TYPE_COP:        evType = INTERESTING_EVENT_5; break;
                        case PED_TYPE_CRIMINAL:   evType = INTERESTING_EVENT_6; break;
                        case PED_TYPE_PROSTITUTE: evType = INTERESTING_EVENT_4; break;
                        default:
                            if (IsPedTypeGang(ped->m_nPedType)) {
                                evType = INTERESTING_EVENT_7;
                            } else {
                                knownType = false;
                            }
                            break;
                        }
                        if (knownType) {
                            Add(evType, subject);
                        }
                    }
                }
            }

            // Add interesting events for vehicles driven by an AI that is actively doing something
            for (auto* veh : sector.Vehicles) {
                if (!veh->IsScanCodeCurrent()) {
                    veh->SetCurrentScanCode();
                    if (!veh->physicalFlags.bRenderScorched
                        && veh->m_pDriver
                        && veh->m_autoPilot.m_nCarMission != MISSION_NONE
                        && veh->m_autoPilot.m_nCarMission != MISSION_BLOCKPLAYER_HANDBRAKESTOP) {
                        Add(INTERESTING_EVENT_14, veh);
                    }
                }
            }
        }
    }
}

// 0x6028A0
TInterestingEvent* CInterestingEvents::GetInterestingEvent() {
    const auto now = CTimer::GetTimeInMS();

    if (m_b4 && m_nInterestingEvent != -1) {
        auto& ev = m_Events[m_nInterestingEvent];
        if (ev.entity && (uint32)(m_nDelays[ev.type] + ev.time) > now) {
            return &ev; // Keep using the current event until it expires
        }
    }

    int32 best{};
    uint8 bestPrio{};
    for (auto i = 0; i < MAX_INTERESTING_EVENTS; i++) {
        auto& ev = m_Events[i];
        if (!ev.entity) {
            continue;
        }
        if ((uint32)(m_nDelays[ev.type] + ev.time) <= now) {
            continue; // expired
        }
        // Prefer the highest priority; sometimes randomly pick a lower one (1/512 chance)
        if (bestPrio < m_nPriorities[ev.type] || (CGeneral::GetRandomNumber() & 0xFFFF) < 0x80) {
            bestPrio = m_nPriorities[ev.type];
            best     = i;
        }
    }

    m_nInterestingEvent = (int8)best;
    return best == -1 ? nullptr : &m_Events[best];
}

// 0x602960
void CInterestingEvents::InvalidateEvent(const TInterestingEvent* event) {
    for (auto index = 0; index < MAX_INTERESTING_EVENTS; index++) {
        TInterestingEvent* tevent = &m_Events[index];
        if (tevent != event)
            continue;

        tevent->time = 0;
        CEntity::ClearReference(tevent->entity);
        if (m_nInterestingEvent == index) {
            m_nInterestingEvent = -1;
        }
    }
}

// 0x6029C0
void CInterestingEvents::InvalidateNonVisibleEvents() {
    const auto& camPos = CCamera::GetActiveCamera().m_vecSource;
    // NOTSA(stability): event entities are raw pointers kept without
    // references - the entity may be long gone. Validate against the pools
    // (pointer-arithmetic only, no deref) before touching it.
    const auto EntityStillInPools = [](CEntity* e) -> bool {
        return CPools::IsValidPointer<CPed>(static_cast<CPed*>(e))
            || CPools::IsValidPointer<CVehicle>(static_cast<CVehicle*>(e))
            || CPools::IsValidPointer<CObject>(static_cast<CObject*>(e))
            || CPools::IsValidPointer<CBuilding>(static_cast<CBuilding*>(e));
    };
    for (auto i = 0; i < MAX_INTERESTING_EVENTS; i++) {
        TInterestingEvent& event = m_Events[i];
        if (!event.entity)
            continue;

        if (!EntityStillInPools(event.entity)) {
            // Entity was deleted - just drop the event, do NOT touch it.
            event.entity = nullptr;
            event.time   = 0;
            if (m_nInterestingEvent == i) {
                m_nInterestingEvent = -1;
            }
            continue;
        }

        CVector pos = event.entity->GetPosition();
        if (CWorld::GetIsLineOfSightClear(camPos, pos, true, false, false, false, false, true, false))
            continue;

        event.time = 0;
        CEntity::SafeCleanUpRef(event.entity);
        if (m_nInterestingEvent == i) {
            m_nInterestingEvent = -1;
        }
    }
}
