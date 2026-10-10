/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/

#include "StdInc.h"

#include "Rope.h"
#include "Ropes.h"

void CRopes::InjectHooks() {
    RH_ScopedClass(CRopes);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x555DC0);
    RH_ScopedInstall(Shutdown, 0x556B10);
    RH_ScopedInstall(Update, 0x558D70);
    RH_ScopedInstall(Render, 0x556AE0);
    RH_ScopedInstall(RegisterRope, 0x556B40);
    RH_ScopedInstall(FindPickupHeight, 0x556760);
    RH_ScopedInstall(FindRope, 0x556000);
    RH_ScopedInstall(FindCoorsAlongRope, 0x555E40);
    RH_ScopedInstall(CreateRopeForSwatPed, 0x558D10);
    RH_ScopedInstall(IsCarriedByRope, 0x555F80);
    RH_ScopedInstall(SetSpeedOfTopNode, 0x555DF0);
}

// 0x555DC0
void CRopes::Init() {
    for (auto& rope : aRopes) {
        rope.m_nType = eRopeType::NONE;
    }
    PlayerControlsCrane = eControlledCrane::NONE;
}

// 0x556B10
void CRopes::Shutdown() {
    for (auto& rope : aRopes) {
        if (rope.m_nType == eRopeType::NONE)
            continue;

        rope.Remove();
    }
}

// 0x558D70
void CRopes::Update() {
    ZoneScoped;

    if (CReplay::Mode == MODE_PLAYBACK)
        return;

    for (auto& rope : aRopes) {
        if (rope.m_nType != eRopeType::NONE)
            rope.Update();
    }
}

// 0x556AE0
void CRopes::Render() {
    ZoneScoped;

    for (auto& rope : aRopes) {
        if (rope.m_nType != eRopeType::NONE)
            rope.Render();
    }
}

// Must be used in loop to make attached to holder
// Re-initializes an existing rope with the same id (positions 0..segmentCount in startPos, speeds
// zeroed, m_nFlags2 |= 1, then CreateHookObjectForRope), otherwise takes the first free slot.
// m_fMass/m_fTotalLength by type: MAGNET 10.0f/0.32258064f(=10/31), CRANE_MAGNO 50.0f/1.6129032f(=50/31),
// WRECKING_BALL/QUARRY_CRANE_ARM/CRANE_TROLLEY 68.0f/2.1935484f(=68/31), otherwise 20.0f/0.64516127f(=20/31).
// `flags` bit 0 -> m_nFlags2 bit 2 (ground collision of the segments in Update);
// bExpires -> m_nTime = CTimer::GetTimeInMS() + timeExpire (0xB7CB84).
// Nodes: crane types (4..7) hang vertically downwards (step m_fTotalLength by z), the others - zigzag
// along X (odd +m_fTotalLength, even -m_fTotalLength), speeds are zeroed.
// m_fSegmentLength: 0.9f (0x3F666666) if the holder is a vehicle (CEntity::GetType(), byte 0x36 & 7), otherwise 0.5f.
// 0x556B40
bool CRopes::RegisterRope(uint32 ropeID, uint32 ropeType, CVector startPos, bool bExpires, uint8 segmentCount, uint8 flags, CPhysical* holder, uint32 timeExpire) {
    // 1) Re-initialize an existing rope with the same id
    for (auto& rope : aRopes) {
        if (rope.m_nType == eRopeType::NONE || rope.m_nId != ropeID)
            continue;

        rope.m_aSegments[0] = startPos;
        rope.m_aSpeed[0]    = CVector{};
        rope.m_nFlags2     |= 1;
        rope.m_nSegments    = segmentCount;
        for (auto i = 0u; i <= segmentCount; i++) {
            rope.m_aSegments[i] = startPos;
            rope.m_aSpeed[i]    = CVector{};
        }
        rope.CreateHookObjectForRope();
        return true;
    }

    // 2) First free slot
    CRope* freeRope = nullptr;
    for (auto& rope : aRopes) {
        if (rope.m_nType == eRopeType::NONE) {
            freeRope = &rope;
            break;
        }
    }
    if (!freeRope)
        return false;

    const auto type = static_cast<eRopeType>(ropeType);
    auto& rope = *freeRope;

    rope.m_nId               = ropeID;
    rope.m_aSegments[0]      = startPos;
    rope.m_aSpeed[0]         = CVector{};
    rope.m_nSegments         = segmentCount;
    rope.m_nFlags2           = (rope.m_nFlags2 & ~0x6u) | 0x1u | ((flags & 1) << 2); // clear bits 1-2, bit0=1, bit2=flags&1
    rope.m_fGroundZ          = 0.0f;
    rope.m_pAttachedEntity   = nullptr;
    rope.m_pRopeAttachObject = nullptr;
    rope.m_fSegmentLength    = (!holder || holder->GetType() != ENTITY_TYPE_VEHICLE) ? 0.5f : 0.9f;
    rope.m_nFlags1           = 0;
    rope.m_pRopeHolder       = holder;
    rope.m_nType             = type;

    if (holder)
        holder->RegisterReference(reinterpret_cast<CEntity**>(&rope.m_pRopeHolder)); // 0x571B70

    rope.m_nTime = bExpires ? CTimer::GetTimeInMS() + timeExpire : 0;

    switch (type) {
    case eRopeType::MAGNET:
        rope.m_fMass        = 10.0f;
        rope.m_fTotalLength = 0.32258064f; // 10 / 31
        break;
    case eRopeType::CRANE_MAGNO:
        rope.m_fMass        = 50.0f;
        rope.m_fTotalLength = 1.6129032f; // 50 / 31
        break;
    case eRopeType::WRECKING_BALL:
    case eRopeType::QUARRY_CRANE_ARM:
    case eRopeType::CRANE_TROLLEY:
        rope.m_fMass        = 68.0f;
        rope.m_fTotalLength = 2.1935484f; // 68 / 31
        break;
    default:
        rope.m_fMass        = 20.0f;
        rope.m_fTotalLength = 0.64516127f; // 20 / 31
        break;
    }

    // Node placement
    if (type < eRopeType::CRANE_MAGNO || type > eRopeType::CRANE_TROLLEY) {
        // Zigzag along X (alternating +/-), y/z inherited from startPos; speeds are zeroed
        for (auto i = 1u; i < NUM_ROPE_SEGMENTS; i++) {
            rope.m_aSegments[i] = CVector{
                rope.m_aSegments[i - 1].x + ((i & 1) ? rope.m_fTotalLength : -rope.m_fTotalLength),
                rope.m_aSegments[i - 1].y,
                rope.m_aSegments[i - 1].z
            };
            rope.m_aSpeed[i] = CVector{};
        }
    } else {
        // Cranes (4..7): the chain hangs vertically downwards; speeds are NOT touched (so in the original)
        for (auto i = 1u; i < NUM_ROPE_SEGMENTS; i++) {
            rope.m_aSegments[i] = CVector{
                rope.m_aSegments[i - 1].x,
                rope.m_aSegments[i - 1].y,
                rope.m_aSegments[i - 1].z - rope.m_fTotalLength
            };
        }
    }

    rope.CreateHookObjectForRope(); // 0x556070
    return true;
}

// 0x556760
float CRopes::FindPickupHeight(CEntity* entity) {
    return CModelInfo::GetModelInfo(entity->m_nModelIndex)->GetColModel()->GetBoundingBox().m_vecMax.z;
}

// Returns id to array
// 0x556000
int32 CRopes::FindRope(uint32 id) {
    for (auto ropeId = 0; ropeId < MAX_NUM_ROPES; ropeId++) {
        if (aRopes[ropeId].m_nType != eRopeType::NONE && aRopes[ropeId].m_nId == id)
            return ropeId;
    }
    return -1;
}

// a4 always nullptr
// Constants: clamp of t in [0.0f (0x858B50), 0.999f (0x863E2C)], then t *= 31.0f (0x863E28 == NUM_ROPE_SEGMENTS-1).
// outSpeed = m_aSpeed[i + 1] (index i+1 - so in the original).
// 0x555E40
bool CRopes::FindCoorsAlongRope(uint32 ropeId, float fDistAlongRope, CVector* outPosn, CVector* outSpeed) {
    for (auto& rope : aRopes) {
        if (rope.m_nType == eRopeType::NONE || rope.m_nId != ropeId) {
            continue;
        }

        auto t = fDistAlongRope;
        if (t <= 0.0f) {
            t = 0.0f;
        } else if (t >= 0.999f) {
            t = 0.999f;
        }
        t *= 31.0f; // NUM_ROPE_SEGMENTS - 1

        const auto  segIdx = (int)(t);
        const auto  frac   = t - (float)(segIdx);
        const auto& segA   = rope.m_aSegments[segIdx];
        const auto& segB   = rope.m_aSegments[segIdx + 1];

        *outPosn = segA * (1.0f - frac) + segB * frac;
        if (outSpeed) {
            *outSpeed = rope.m_aSpeed[segIdx + 1];
        }
        return true;
    }
    return false;
}

// 0x558D10
int32 CRopes::CreateRopeForSwatPed(const CVector& startPos) {
    int32 newRopeId = m_nRopeIdCreationCounter + 100;
    if (RegisterRope(newRopeId, static_cast<uint32>(eRopeType::SWAT), startPos, true, 0, 0, nullptr, 4000)) {
        return -1;
    }

    m_nRopeIdCreationCounter += 1;
    return newRopeId;
}

// 0x555F80
bool CRopes::IsCarriedByRope(CPhysical* entity) {
    if (!entity)
        return false;

    for (auto& rope : aRopes) {
        if (rope.m_nType != eRopeType::NONE && rope.m_pRopeAttachObject == entity)
            return true;
    }
    return false;
}

// 0x555DF0
void CRopes::SetSpeedOfTopNode(uint32 ropeId, CVector dirSpeed) {
    for (auto& rope : aRopes) {
        if (rope.m_nType != eRopeType::NONE && rope.m_nId == ropeId) {
            rope.m_aSpeed[0] = dirSpeed;
            return;
        }
    }
}
