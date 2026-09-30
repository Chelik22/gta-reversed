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
    RH_ScopedInstall(RegisterRope, 0x556B40, { .reversed = false });
    RH_ScopedInstall(FindPickupHeight, 0x556760);
    RH_ScopedInstall(FindRope, 0x556000);
    RH_ScopedInstall(FindCoorsAlongRope, 0x555E40, { .reversed = false });
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
// 0x556B40
bool CRopes::RegisterRope(uint32 ropeID, uint32 ropeType, CVector startPos, bool bExpires, uint8 segmentCount, uint8 flags, CPhysical* holder, uint32 timeExpire) {
    // 1) If a rope with this id already exists - just re-position it
    for (auto& rope : aRopes) {
        if (rope.m_nType != eRopeType::NONE && rope.m_nId == ropeID) {
            rope.m_aSegments[0] = startPos;
            rope.m_aSpeed[0]    = CVector{};
            for (int32 i = 0; i <= rope.m_nSegments; i++) {
                rope.m_aSegments[i] = startPos;
                rope.m_aSpeed[i]    = CVector{};
            }
            rope.m_nSegments = segmentCount;
            rope.m_nFlags2 |= 1;
            rope.CreateHookObjectForRope();
            return true;
        }
    }

    // 2) Find a free slot
    CRope* rope = nullptr;
    for (auto& r : aRopes) {
        if (r.m_nType == eRopeType::NONE) {
            rope = &r;
            break;
        }
    }
    if (!rope) {
        return false;
    }

    // Initialize it
    rope->m_nId               = ropeID;
    rope->m_aSegments[0]      = startPos;
    rope->m_aSpeed[0]         = CVector{};
    rope->m_nSegments         = segmentCount;
    rope->m_nFlags2           = (rope->m_nFlags2 & 0xF9) | ((flags & 1) << 2) | 1;
    rope->m_fGroundZ          = 0.0f;
    rope->m_fMass             = 0.0f;
    rope->m_pRopeHolder       = holder;
    rope->m_pAttachedEntity   = nullptr;
    rope->m_pRopeAttachObject = nullptr;

    // Segment length depends on the holder type
    if (!holder || holder->GetType() != ENTITY_TYPE_VEHICLE) {
        rope->m_fSegmentLength = 0.5f;
    } else {
        rope->m_fSegmentLength = 0.9f;
    }

    rope->m_nTime = bExpires ? CTimer::m_snTimeInMilliseconds + timeExpire : 0;

    // Mass/total length per rope type
    switch (static_cast<eRopeType>(ropeType)) {
    case eRopeType::MAGNET:
        rope->m_fMass        = 10.0f;
        rope->m_fTotalLength = 0.32f;
        break;
    case eRopeType::CRANE_MAGNO:
        rope->m_fMass        = 50.0f;
        rope->m_fTotalLength = 1.62f;
        break;
    case eRopeType::WRECKING_BALL:
    case eRopeType::QUARRY_CRANE_ARM:
    case eRopeType::CRANE_TROLLEY:
        rope->m_fMass        = 68.0f;
        rope->m_fTotalLength = 2.19f;
        break;
    default:
        rope->m_fMass        = 20.0f;
        rope->m_fTotalLength = 0.64f;
        break;
    }

    // Lay out the initial segment chain
    if (ropeType < 4 || ropeType > 7) {
        // Alternating left/right chain (long ropes)
        for (int32 i = 1; i < NUM_ROPE_SEGMENTS; i++) {
            rope->m_aSegments[i].x = rope->m_aSegments[i - 1].x;
            rope->m_aSegments[i].y = rope->m_aSegments[i - 1].y + ((i & 1) ? rope->m_fTotalLength : -rope->m_fTotalLength);
            rope->m_aSegments[i].z = rope->m_aSegments[i - 1].z;
            rope->m_aSpeed[i]      = CVector{};
        }
    } else {
        // Crane types: hang straight down
        for (int32 i = 1; i < NUM_ROPE_SEGMENTS; i++) {
            rope->m_aSegments[i].x = rope->m_aSegments[i - 1].x;
            rope->m_aSegments[i].y = rope->m_aSegments[i - 1].y;
            rope->m_aSegments[i].z = rope->m_aSegments[i - 1].z - rope->m_fTotalLength;
        }
    }

    if (holder) {
        holder->RegisterReference(reinterpret_cast<CEntity**>(&rope->m_pRopeHolder));
    }

    rope->CreateHookObjectForRope();
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
// 0x555E40
// NOTE: Faithful instruction-level port of the original (x87 + inline rope search).
// The distance is normalized progress along the rope: clamped to [0, 0.999], scaled
// by 31.0 (segment count) and split into integer segment index + fractional alpha.
// Original .rdata constants: 0.0 0x858B50, 0.999 0x863E2C, 31.0 0x863E28, 1.0 0x858624.
static auto& s_aRopes = CRopes::aRopes; // unqualified alias so the asm block can load the array base directly

namespace {
    const float ROPE_ALPHA_ZERO = 0.0f;   // 0x858B50
    const float ROPE_ALPHA_MAX  = 0.999f; // 0x863E2C
    const float ROPE_SCALE_31   = 31.0f;  // 0x863E28
    const float ROPE_ONE        = 1.0f;   // 0x858624
}

// Mimics the original's CRT float->int rounder (0x821B40): pops the value off the
// x87 stack and returns it, rounded to nearest, in eax. Naked so it can be tail-jumped/called.
static __declspec(naked) int32 FpuRoundToInt() {
    __asm {
        sub esp, 4
        fistp dword ptr [esp]
        pop eax
        ret
    }
}

static_assert(offsetof(CRope, m_nId) == 0x300);
static_assert(offsetof(CRope, m_nType) == 0x325);
static_assert(offsetof(CRope, m_aSpeed) == 0x180);
static_assert(sizeof(CRope) == 0x328);
bool CRopes::FindCoorsAlongRope(uint32 ropeId, float fDistAlongRope, CVector* outPosn, CVector* outSpeed) {
    bool result;
    __asm {
        sub esp, 0x28                      ; scratch space for the float temps (like the original)

        ; ---- find the first rope whose type isn't NONE and id matches (original inlines the search) ----
        mov ecx, dword ptr [ropeId]
        mov edx, dword ptr [s_aRopes]      ; edx = aRopes (std::array<CRope, 8>)
        lea eax, [edx + 0x300]             ; &aRopes[0].m_nId
        xor esi, esi
search_rope:
        mov dl, byte ptr [eax + 0x25]      ; rope.m_nType != eRopeType::NONE
        test dl, dl
        je next_rope
        cmp dword ptr [eax], ecx
        je found_rope
next_rope:
        add eax, 0x328                     ; sizeof(CRope)
        inc esi
        cmp esi, 8                         ; MAX_NUM_ROPES
        jl search_rope
        xor al, al
        mov result, al
        add esp, 0x28
        jmp done

found_rope:
        ; ---- clamp fDistAlongRope into [0, 0.999] ----
        fld dword ptr [fDistAlongRope]
        fld ROPE_ALPHA_ZERO
        fcomp st(1)
        fnstsw ax
        test ah, 0x41
        jne check_upper
        fstp st(0)
        fld ROPE_ALPHA_ZERO
        jmp scale
check_upper:
        fld ROPE_ALPHA_MAX
        fcomp st(1)
        fnstsw ax
        test ah, 5
        jp scale
        fstp st(0)
        fld ROPE_ALPHA_MAX
scale:
        ; ---- split into integer segment index ([esp+4]) + fractional alpha (st0) ----
        fmul ROPE_SCALE_31
        fld st(0)
        call FpuRoundToInt                 ; original calls its CRT rounder (0x821B40) here
        mov dword ptr [esp + 4], eax
        fisub dword ptr [esp + 4]

        mov ecx, esi                       ; ecx = rope offset in the array
        lea ebx, [eax + eax*2 + 3]         ; (segIdx + 1) * 3
        imul ecx, ecx, 0x328               ; sizeof(CRope)
        fld st(0)                          ; a, a
        lea ebx, [ecx + ebx*4]             ; ebx = offset of m_aSegments[segIdx + 1]
        lea eax, [eax + eax*2]             ; segIdx * 3
        lea eax, [ecx + eax*4]             ; eax = offset of m_aSegments[segIdx]
        mov ecx, dword ptr [outPosn]

        ; ---- lerp: outPosn = p0 * (1 - alpha) + p1 * alpha (x component lives on the x87 stack) ----
        fmul dword ptr [edx + ebx]         ; a*p1.x
        fld st(1)                          ; a, a*p1.x, a
        fmul dword ptr [edx + ebx + 4]     ; a*p1.y
        fstp dword ptr [esp + 0x24]
        fld st(1)                          ; a, a*p1.x, a
        fmul dword ptr [edx + ebx + 8]     ; a*p1.z
        fstp dword ptr [esp + 0x28]
        fld ROPE_ONE                       ; 1
        fsub st(0), st(2)                  ; 1 - a
        fld st(0)                          ; 1-a, 1-a
        fmul dword ptr [edx + eax]         ; (1-a)*p0.x
        fstp dword ptr [esp + 8]
        fld st(0)
        fmul dword ptr [edx + eax + 4]     ; (1-a)*p0.y
        fstp dword ptr [esp + 0xC]
        fmul dword ptr [edx + eax + 8]     ; (1-a)*p0.z
        fstp dword ptr [esp + 0x10]
        fld dword ptr [esp + 8]
        fadd st(0), st(1)                  ; x
        fstp dword ptr [esp + 0x14]
        mov eax, dword ptr [esp + 0x14]
        mov [ecx], eax
        fstp st(0)                         ; pop a*p1.x
        fstp st(0)                         ; pop 1-a
        fld dword ptr [esp + 0xC]
        fadd dword ptr [esp + 0x24]        ; y
        fstp dword ptr [esp + 0x18]
        mov eax, dword ptr [esp + 0x18]
        mov [ecx + 4], eax
        fld dword ptr [esp + 0x10]
        fadd dword ptr [esp + 0x28]        ; z
        fstp dword ptr [esp + 0x1C]
        mov eax, dword ptr [esp + 0x1C]
        mov [ecx + 8], eax

        ; ---- optional out speed (segment segIdx + 1) ----
        mov eax, dword ptr [outSpeed]
        test eax, eax
        je no_speed
        lea ecx, [edx + ebx + 0x180]       ; &m_aSpeed[segIdx + 1]
        mov edx, dword ptr [ecx]
        mov dword ptr [eax], edx
        mov edx, dword ptr [ecx + 4]
        mov dword ptr [eax + 4], edx
        mov edx, dword ptr [ecx + 8]
        mov dword ptr [eax + 8], edx
no_speed:
        mov al, 1
        mov result, al
        add esp, 0x28
done:
    }
    return result;
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