/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "Pickups.h"
#include "Garages.h"
#include "tPickupMessage.h"
#include "Radar.h"
#include "Shadows.h"
#include "Coronas.h"
#include "TaskSimpleJetPack.h"
#include "PostEffects.h"
#include "Collision/ColModel.h"

using namespace ModelIndices;
void CPickups::InjectHooks() {
    RH_ScopedClass(CPickups);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x454A70);
    RH_ScopedInstall(ReInit, 0x456E60);
    RH_ScopedInstall(AddToCollectedPickupsArray, 0x455240);
    RH_ScopedOverloadedInstall(CreatePickupCoorsCloseToCoors, "", 0x458A80, void(*)(float, float, float, float&, float&, float&));
    RH_ScopedInstall(CreateSomeMoney, 0x458970);
    RH_ScopedInstall(DetonateMinesHitByGunShot, 0x4590C0);
    RH_ScopedInstall(DoCollectableEffects, 0x455E20);
    RH_ScopedInstall(DoMineEffects, 0x4560E0);
    RH_ScopedInstall(DoMoneyEffects, 0x454E80);
    RH_ScopedInstall(DoPickUpEffects, 0x455720);
    RH_ScopedInstall(FindPickUpForThisObject, 0x4551C0);

    // Cannot be hooked at all for now due to ABI fuckery, the return value is 32 bit, but causes the function to assume the calling convention of of T* Function(T*, ...)
    //RH_ScopedInstall(GenerateNewOne, 0x456F20, { .Reversed = false });

    RH_ScopedInstall(GenerateNewOne_WeaponType, 0x457380);
    RH_ScopedInstall(GetActualPickupIndex, 0x4552A0);
    RH_ScopedInstall(GetNewUniquePickupIndex, 0x456A30);
    RH_ScopedInstall(GetUniquePickupIndex, 0x455280);
    RH_ScopedInstall(GivePlayerGoodiesWithPickUpMI, 0x4564F0);
    RH_ScopedInstall(IsPickUpPickedUp, 0x454B40);
    RH_ScopedInstall(ModelForWeapon, 0x454AC0);
    RH_ScopedInstall(PassTime, 0x455200);
    RH_ScopedInstall(PickedUpHorseShoe, 0x455390);
    RH_ScopedInstall(PickedUpOyster, 0x4552D0);
    RH_ScopedInstall(PictureTaken, 0x456A70);
    RH_ScopedInstall(PlayerCanPickUpThisWeaponTypeAtThisMoment, 0x4554C0);
    RH_ScopedInstall(RemoveMissionPickUps, 0x456DE0);
    RH_ScopedInstall(RemovePickUp, 0x4573D0);
    RH_ScopedInstall(RemovePickUpsInArea, 0x456D30);
    RH_ScopedInstall(RemovePickupObjects, 0x455470);
    RH_ScopedInstall(RemoveUnnecessaryPickups, 0x4563A0);
    RH_ScopedInstall(RenderPickUpText, 0x455000);
    RH_ScopedInstall(TestForPickupsInBubble, 0x456450);
    RH_ScopedInstall(TryToMerge_WeaponType, 0x4555A0);
    RH_ScopedInstall(Update, 0x458DE0);
    RH_ScopedInstall(UpdateMoneyPerDay, 0x455680);
    RH_ScopedInstall(WeaponForModel, 0x454AE0);
    RH_ScopedInstall(Load, 0x5D35A0);
    RH_ScopedInstall(Save, 0x5D3540);

    RH_ScopedGlobalInstall(ModifyStringLabelForControlSetting, 0x454B70);
}

// 0x454A70
void CPickups::Init() {
    ZoneScoped;

    NumMessages = 0;
    for (auto& pickup : aPickUps) {
        pickup.m_nPickupType = PICKUP_NONE;
        pickup.m_nReferenceIndex = 1;
        pickup.m_pObject = nullptr;
    }
    aPickUpsCollected.fill(0);
    CollectedPickUpIndex = 0u;
    DisplayHelpMessage = 10u;
}

// 0x456E60
void CPickups::ReInit() {
    rng::for_each(GetAllActivePickups(), [](auto& pickup) { pickup.Remove(); });
    Init();
}

// 0x455240
void CPickups::AddToCollectedPickupsArray(int32 pickupIndex) {
    aPickUpsCollected[CollectedPickUpIndex++] = GetUniquePickupIndex(pickupIndex).num;
    CollectedPickUpIndex %= std::size(aPickUpsCollected);
}

/*!
 * @addr 0x458A80
 * @brief Created a pickup close to pos (inX, inY, inZ)
 *
 * @param [out] outX, outY, outZ Created pickup's position
 */
void CPickups::CreatePickupCoorsCloseToCoors(float inX, float inY, float inZ, float& outX, float& outY, float& outZ) {
    int32 attempts = 0;
    while (true) {
        const auto angle = (float)(rand() & 0xFF) * 0.024543693f; // 0x859BBC = 2*pi/256
        const float x = inX + std::sin(angle) * 1.5f;             // 0x858CE8
        const float y = inY + std::cos(angle) * 1.5f;

        bool foundGround = false;
        const float z = CWorld::FindGroundZFor3DCoord(CVector{ x, y, inZ }, &foundGround) + 0.5f; // 0x5696C0

        if (foundGround) {
            const CVector basePos{ inX, inY, inZ + 0.3f }; // 0x858C24
            const CVector dir{ x - basePos.x, y - basePos.y, z - basePos.z };
            const auto dirLen = dir.Magnitude();
            const CVector extendedEnd = basePos + dir * ((dirLen + 0.4f) / dirLen); // 0x858EE8

            const CVector playerCoors = FindPlayerCoors(-1); // 0x56E010 (original calls it 4 times)
            const bool bFarFromPlayer = 2.0f < (CVector{ x, y, z } - playerCoors).Magnitude2D(); // 0x858CA0

            const bool bTryPlace = bFarFromPlayer
                ? (attempts > 16 || !TestForPickupsInBubble(CVector{ x, y, z }, 1.3f))
                : attempts > 16;

            if (bTryPlace && CWorld::GetIsLineOfSightClear(extendedEnd, basePos, true, attempts < 16, false, attempts < 16)) {
                if (attempts > 16 || !CWorld::TestSphereAgainstWorld(CVector{ x, y, z }, 1.2f, nullptr, false, false, true, false, false, false)) {
                    outX = x;
                    outY = y;
                    outZ = z;
                    return;
                }
            }
        }

        attempts++;
        if (attempts > 31) {
            outX = inX;
            outY = inY;
            outZ = inZ + 0.4f;
            return;
        }
    }
}

/*!
 * @addr 0x458970
 * @brief Creates wads of money that is worth amount of money at the position coors.
 */
void CPickups::CreateSomeMoney(CVector coors, int32 amount) {
    const auto wads = std::min(amount / 20 + 1, 7);
    const auto perWad = amount / wads;

    for (auto i = 0; i < wads; i++) {
        bool result;
        coors.x += std::sinf(CGeneral::GetRandomNumberInRange(0.f, TWO_PI)) * 1.5f;
        coors.y += std::cosf(CGeneral::GetRandomNumberInRange(0.f, TWO_PI)) * 1.5f;
        coors.z = CWorld::FindGroundZFor3DCoord(coors, &result, nullptr) + 0.5f;

        if (result) {
            GenerateNewOne(coors, MI_MONEY, PICKUP_MONEY, perWad + (CGeneral::GetRandomNumber() % 4));
        }
    }
}

// 0x4590C0
void CPickups::DetonateMinesHitByGunShot(const CVector& shotOrigin, const CVector& shotTarget) {
    for (auto& pickup : aPickUps) {
        if (pickup.m_nPickupType == PICKUP_NAUTICAL_MINE_ARMED) {
            pickup.ProcessGunShot(shotOrigin, shotTarget);
        }
    }
}

// 0x455E20
void CPickups::DoCollectableEffects(CEntity* entity) {
    const auto& entityPos = entity->GetPosition();

    if (const auto d = DistanceBetweenPoints(TheCamera.GetPosition(), entityPos); d < 14.0f) {
        // shade of gray
        const auto t = (uint8)((std::sinf((float)(((uint16)std::bit_cast<uintptr_t>(entity) + (uint16)CTimer::GetTimeInMS()) % 2048) * 0.0030664064f) + 1.0f) / 2.0f * ((14.0f - d) / 14.0f) * 255.0f);

        CShadows::StoreStaticShadow(
            (uint32)entity,
            SHADOW_ADDITIVE,
            gpShadowExplosionTex,
            entityPos,
            2.0f,
            0.0f,
            0.0f,
            -2.0f,
            0,
            t,
            t,
            t,
            4.0f,
            1.0f,
            40.0f,
            false,
            0.0f
        );
        CCoronas::RegisterCorona(
            (uint32)entity,
            nullptr,
            t,
            t,
            t,
            255,
            entityPos,
            0.6f,
            40.0f,
            CORONATYPE_TORUS,
            FLARETYPE_NONE,
            eCoronaReflType::CORREFL_NONE,
            eCoronaLOSCheck::LOSCHECK_OFF,
            eCoronaTrail::TRAIL_OFF,
            0.0f,
            false,
            1.5f,
            0,
            15.0f,
            false,
            false
        );
    }

    entity->GetMatrix().SetRotateZOnly(static_cast<float>(CTimer::GetTimeInMS() % 4096) * 0.0015283204f);
}

// 0x4560E0
void CPickups::DoMineEffects(CEntity* entity) {
    const auto& entityPos = entity->GetPosition();

    if (const auto d = DistanceBetweenPoints(TheCamera.GetPosition(), entityPos); d < 20.0f) {
        // shade of red
        const auto t = (uint8)((std::sinf((float)(((uint16)std::bit_cast<uintptr_t>(entity) + (uint16)CTimer::GetTimeInMS()) % 512) * 0.012265625f) + 1.0f) / 2.0f * ((20.0f - d) / 20.0f) * 64.0f);

        CShadows::StoreStaticShadow(
            (uint32)entity,
            SHADOW_ADDITIVE,
            gpShadowExplosionTex,
            entityPos,
            2.0f,
            0.0f,
            0.0f,
            -2.0f,
            0,
            t,
            0,
            0,
            4.0f,
            1.0f,
            40.0f,
            false,
            0.0f
        );
        CCoronas::RegisterCorona(
            (uint32)entity,
            nullptr,
            t,
            0,
            0,
            255,
            entityPos,
            0.6f,
            40.0f,
            CORONATYPE_TORUS,
            FLARETYPE_NONE,
            eCoronaReflType::CORREFL_NONE,
            eCoronaLOSCheck::LOSCHECK_OFF,
            eCoronaTrail::TRAIL_OFF,
            0.0f,
            false,
            1.5f,
            0,
            15.0f,
            false,
            false
        );
    }

    entity->GetMatrix().SetRotateZOnly(static_cast<float>(CTimer::GetTimeInMS() % 1024) * 0.0061132815f);
}

// 0x454E80
void CPickups::DoMoneyEffects(CEntity* entity) {
    entity->GetMatrix().SetRotateZOnly(static_cast<float>(CTimer::GetTimeInMS() % 2048) * 0.0030566407f);
}

// —— Original globals not declared anywhere in the repo ——
static auto& CAMERA_PICKUP_INDEX     = StaticRef<uint8>(0xB6F081); // byte index into the array below (original uses it with stride 0x11C)
static auto& CAMERA_PICKUP_ARRAY     = StaticRef<uint8[64][0x11C]>(0xB6F1A8); // stride 0x11C (as in the original; [i][0] == 0x2E?)
static auto& PICKUP_MSG_MAX_DIST     = StaticRef<float>(0x859030); // max. 2D camera distance for the message to show up
static auto& PICKUP_MSG_Z_OFFSET     = StaticRef<float>(0x858CB0); // 0.7f above the pickup
static auto& PICKUP_SPIN_REF         = StaticRef<float>(0x858F08); // 1.2f
static auto& PICKUP_CORONA_R_PHASE   = StaticRef<float>(0x859B5C); // camera corona radius pulsation phase
static auto& PICKUP_CORONA_R_OFFSET  = StaticRef<float>(0x859B58);
static auto& PICKUP_CORONA_R_MULT    = StaticRef<float>(0x859B54);
static auto& PICKUP_SPIN_STEP        = StaticRef<float>(0x859B0C); // rad/ms of rotation (timeMS & 0x7FF)
// message color table: [colorId][0..2] = RGB (stride 8)
static auto& aPickupMessageColors    = StaticRef<uint8[54][8]>(0x8A5FB0);

// 0x455720
void CPickups::DoPickUpEffects(CEntity* entity) {
    // Implicitly used: `this` of PickUpShouldBeInvisible() below (Ghidra loses the thiscall's ecx)
    const auto pickup = FindPickUpForThisObject(static_cast<CObject*>(entity));
    // FIX (verify wf5b): objectFlags/m_nBonusValue/m_wCostValue are CObject fields (+0x140/+0x13D/+0x13E),
    //   while the header parameter is CEntity*; an explicit cast is required.
    auto* const obj   = static_cast<CObject*>(entity);
    const auto  model = entity->m_nModelIndex; // +0x22

    if (model == MI_PICKUP_CAMERA) {
        if (CAMERA_PICKUP_ARRAY[CAMERA_PICKUP_INDEX][0] == 0x2E /* ePickupType 46 - "camera" (not in the repo's enum) */) {
            obj->objectFlags.bDoNotRender = false; // bit 0x2000000
        } else {
            obj->objectFlags.bDoNotRender = true;
            if (CClock::ms_nGameClockHours < 5 || CPostEffects::IsVisionFXActive()) { // 0xB70153 / 0x7034F0
                // fp-expressions restored exactly from the disassembly 0x455793..0x45580E:
                //   randVal = _ftol(-50.0f * (float)(_rand() & 0xFFFF) * (1/32768.0f))  [0x858B14 = 1/32768, 0x859B60 = -50]
                //   coronaRed = 100 - randVal;  coronaGB = _ftol(0.7f * (float)(100 - randVal))  [0x858CB0 = 0.7]
                //   (the second color is derived, not a fresh rand)
                const auto randVal = static_cast<int32>(-50.0f * static_cast<float>(CGeneral::GetRandomNumber()) * (1.0f / 32768.0f)); // _ftol2
                const auto coronaRed = static_cast<uint8>(100 - randVal);
                const auto coronaGB  = static_cast<uint8>(static_cast<int32>(0.7f * static_cast<float>(100 - randVal))); // _ftol2

                const auto& entityPos    = entity->GetPosition();
                const auto  coronaRadius = (std::sin(float(CTimer::GetTimeInMS() & 0x1FFF) * PICKUP_CORONA_R_PHASE) + PICKUP_CORONA_R_OFFSET)
                                         * PICKUP_CORONA_R_MULT;

                CCoronas::RegisterCorona( // 0x6FC580
                    // NOTE (verify wf5b): the original passes id = address of a stack variable
                    //   (lea eax,[esp+0x85] @0x455844 - a per-frame "unique" id). Reproducing the stack address
                    //   1:1 in C++ is pointless, the pointer to entity is used as a stable unique id
                    //   (substitution is safe: id is only used as a key).
                    (uint32)(uintptr_t)entity,
                    nullptr,
                    coronaRed, coronaGB, coronaGB, 255,
                    entityPos,
                    coronaRadius,
                    100.0f,
                    CORONATYPE_HEADLIGHT,                 // 1
                    FLARETYPE_NONE,
                    eCoronaReflType::CORREFL_NONE,
                    eCoronaLOSCheck::LOSCHECK_OFF,
                    eCoronaTrail::TRAIL_OFF,
                    0.0f,
                    false,
                    1.5f,
                    false,
                    15.0f,
                    false,
                    false
                );
            }
        }
    } else {
        // 0x454D20: this = pickup (lost by Ghidra)
        obj->objectFlags.bDoNotRender = pickup->PickUpShouldBeInvisible();
    }

    if (obj->objectFlags.bDoNotRender) { // 0x2000000
        return;
    }

    // —— Message color - by the pickup's model (in the decompile - a reused argument slot;
    //    BRIBE/INFO/KILLFRENZY/PROPERTY/SAVEGAME keep their initial value - taken as 0) ——
    // FIX (verify wf5b): the first case used to compare with MI_PICKUP_CAMERA - a copy-paste of the neighboring case.
    //   According to the decompile, it compares with DAT_008cd5bc = MI_PICKUP_ADRENALINE
    //   (ModelIndices.cpp:56; MI_PICKUP_CAMERA = 0x8CD5D8 - the check at the start of the function).
    int16 colorId = 0;
    if (model == MI_PICKUP_ADRENALINE) {
        colorId = 0x2F;
    } else if (model == MI_PICKUP_BODYARMOUR) {
        colorId = 0x30;
    } else if (model != MI_PICKUP_BRIBE && model != MI_PICKUP_INFO && model != MI_PICKUP_KILLFRENZY) {
        if (model == MI_PICKUP_HEALTH || model == MI_PICKUP_BONUS) {
            colorId = 0x2F;
        } else if (model != MI_PICKUP_PROPERTY) {
            if (model == MI_PICKUP_PROPERTY_FORSALE) {
                colorId = 0x2F;
            } else if (model == MI_PICKUP_REVENUE) {
                colorId = 0x35;
            } else if (model != MI_PICKUP_SAVEGAME) {
                if (model == MI_PICKUP_CLOTHES) {
                    colorId = 0x2F;
                } else {
                    colorId = (int16)WeaponForModel(model); // weapons - color by weapon type; non-weapons return -1 (WEAPON_UNARMED = 0)
                }
            }
        }
    }

    const auto& entityPos = entity->GetPosition();

    // —— Purchase message (only for property/out-of-stock or objects with a bonus/price) ——
    if ((obj->m_nObjectFlags & 0xC) // bPickupPropertyForSale | bPickupInShopOutOfStock
     || obj->m_nBonusValue != 0                  // +0x13D
     || obj->m_wCostValue != 0) {                // +0x13E
        // 2D camera distance to the pickup (the decompile has a manual SQRT)
        const auto& camPos  = TheCamera.GetPosition(); // 0xB6F02C / 0xB6F03C
        const auto  dx      = camPos.x - entityPos.x;
        const auto  dy      = camPos.y - entityPos.y;

        if (PICKUP_MSG_MAX_DIST > std::sqrt(dx * dx + dy * dy) && NumMessages <= 15) {
            const CVector worldPos{ entityPos.x, entityPos.y, entityPos.z + PICKUP_MSG_Z_OFFSET };
            RwV3d screenPos{};
            float msgWidth{}, msgHeight{};

            if (CSprite::CalcScreenCoors(worldPos, &screenPos, &msgWidth, &msgHeight, true, true)) { // 0x70CE30
                auto& msg = aMessages[NumMessages];

                msg.pos.x  = screenPos.x;                              // +0x00
                msg.pos.y  = screenPos.y;                              // +0x04
                msg.width  = msgWidth;                                 // +0x0C
                msg.height = msgHeight;                                // +0x10
                msg.pos.z  = std::bit_cast<float>(WeaponForModel(model)); // +0x08 - the original copies the int bits (not used by the renderer)

                msg.color.r = aPickupMessageColors[colorId][0];        // 0x8A5FB0 + colorId*8
                msg.color.g = aPickupMessageColors[colorId][1];
                msg.color.b = aPickupMessageColors[colorId][2];
                msg.color.a = static_cast<uint8>((int)CGeneral::GetRandomNumberInRange(0.0f, 100.0f)); // _ftol2 - expression lost

                msg.flags = obj->objectFlags.bPickupInShopOutOfStock ? (msg.flags | 1) : (msg.flags & 0xFE);
                msg.field_19 = obj->m_nBonusValue;
                msg.price    = obj->m_wCostValue * 5;

                if (model == MI_PICKUP_PROPERTY || model == MI_PICKUP_PROPERTY_FORSALE) {
                    // CPickup+0x1D byte = m_nFlags; bits 4-6 = nPropertyTextIndex
                    const auto text = TheText.Get(CPickup::FindStringForTextIndex(
                        (ePickupPropertyText)pickup->m_nFlags.nPropertyTextIndex)); // 0x455540 + 0x6A0050
                    msg.text = const_cast<GxtChar*>(text);
                    if (model == MI_PICKUP_PROPERTY_FORSALE) {
                        msg.flags |= 2;
                    } else {
                        msg.flags &= 0xFD;
                    }
                } else {
                    msg.text      = nullptr;
                    msg.flags    &= 0xFD;
                }

                NumMessages++; // 0x978678
            }
        }
    }

    // —— Pickup model rotation/scale (always executed) ——
    const auto colModel = CModelInfo::ms_modelInfoPtrs[entity->m_nModelIndex]->m_pColModel; // +0x14
    const auto& bbox    = colModel->m_boundBox;

    const auto dxSize = bbox.m_vecMax.x - bbox.m_vecMin.x;
    const auto dySize = bbox.m_vecMax.y - bbox.m_vecMin.y;
    const auto dzSize = bbox.m_vecMax.z - bbox.m_vecMin.z;
    const auto maxDim = std::max({ dxSize, dySize, dzSize });

    auto spin = PICKUP_SPIN_REF / maxDim; // 1.2f / maxDim
    if (spin < 1.0f)                      // 0x858624 = 1.0f
        spin = 1.0f;
    spin = (spin - 1.0f) * 0.6f + 1.0f;   // 0x858CC8 = 0.6f
    if (entity->m_nModelIndex == MODEL_MINIGUN) // 0x16A
        spin = 1.2f;

    const auto angle = float(CTimer::GetTimeInMS() & 0x7FF) * PICKUP_SPIN_STEP;
    const auto cosA  = std::cos(angle);
    const auto sinA  = std::sin(angle);
    const auto x     = cosA * spin;
    const auto y     = sinA * spin;

    // In the decompile, the matrix-allocation check precedes EACH of the 9 writes.
    // CPlaceable::GetMatrix() does exactly that (allocates + UpdateMatrix when null), so it is
    // called before every write - accessors are used instead of touching CMatrix fields directly.
    entity->GetMatrix().GetRight().x   = x;    // +0x00
    entity->GetMatrix().GetRight().y   = y;    // +0x04
    entity->GetMatrix().GetRight().z   = 0.0f; // +0x08
    entity->GetMatrix().GetForward().x = -y;   // +0x10
    entity->GetMatrix().GetForward().y = x;    // +0x14
    entity->GetMatrix().GetForward().z = 0.0f; // +0x18
    entity->GetMatrix().GetUp().x      = 0.0f; // +0x20
    entity->GetMatrix().GetUp().y      = 0.0f; // +0x24
    entity->GetMatrix().GetUp().z      = spin; // +0x28
}

// 0x4551C0
CPickup* CPickups::FindPickUpForThisObject(CObject* object) {
    for (auto& pickup : GetAllActivePickups()) {
        if (pickup.m_pObject == object) {
            return &pickup;
        }
    }
    return aPickUps.data();
}

// returns pickup handle
// g
tPickupReference CPickups::GenerateNewOne(CVector coors, uint32 modelId, ePickupType pickupType, uint32 ammo, uint32 moneyPerDay, bool isEmpty, char* message) {
    auto retVal = plugin::CallAndReturn<int32, 0x456F20, CVector, uint32, ePickupType, uint32, uint32, bool, char*>(coors, modelId, pickupType, ammo, moneyPerDay, isEmpty, message);
    return tPickupReference(retVal);
}

/*!
 *
 * @param coors Position of new pickup
 * @param weaponType Weapon type
 * @param pickupType Pickup type
 * @param ammo
 * @param isEmpty
 * @param message
 * @return Pickup handle
 * @addr 0x457380
 */
tPickupReference CPickups::GenerateNewOne_WeaponType(CVector coors, eWeaponType weaponType, ePickupType pickupType, uint32 ammo, bool isEmpty, char* message) {
    return GenerateNewOne(coors, CWeaponInfo::GetWeaponInfo(weaponType)->m_nModelId1, pickupType, ammo, 0u, isEmpty, message);
}

/*!
 * @param pickupIndex Index of pickup
 * @return -1 if this index is not actual
 * @addr 0x4552A0
 */
int32 CPickups::GetActualPickupIndex(tPickupReference pickupRef) {
    if (pickupRef.num == -1)
        return -1;

    if (pickupRef.refIndex != aPickUps.at(pickupRef.index).m_nReferenceIndex)
        return -1;

    return pickupRef.index;
}

// 0x456A30
tPickupReference CPickups::GetNewUniquePickupIndex(int32 pickupIndex) {
    auto& refIdx = aPickUps[pickupIndex].m_nReferenceIndex;
    refIdx = (refIdx == -1) ? 1 : refIdx + 1;

    return GetUniquePickupIndex(pickupIndex);
}

// returns pickup handle
// 0x455280
tPickupReference CPickups::GetUniquePickupIndex(int32 pickupIndex) {
    return tPickupReference(pickupIndex, aPickUps.at(pickupIndex).m_nReferenceIndex);
}

// returns TRUE if player got goodies
// 0x4564F0
bool CPickups::GivePlayerGoodiesWithPickUpMI(uint16 modelId, int32 playerId) {
    auto* ped = FindPlayerPed(playerId);

    if (modelId == MI_PICKUP_ADRENALINE) {
        ped->GetPlayerData()->m_bAdrenaline = true;
        ped->GetPlayerData()->m_nAdrenalineEndTime = CTimer::GetTimeInMS() + 20'000;
        ped->ResetSprintEnergy();
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_ADRENALINE);
        return true;
    }

    if (modelId == MI_PICKUP_BODYARMOUR) {
        ped->m_fArmour = (float)FindPlayerInfo(playerId).m_nMaxArmour;
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_BODY_ARMOUR);
        return true;
    }

    if (modelId == MI_PICKUP_INFO) {
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_INFO);
        return true;
    }

    if (modelId == MI_PICKUP_HEALTH) {
        auto maxHealth = FindPlayerInfo(playerId).m_nMaxHealth;
        CStats::UpdateStatsAddToHealth((uint32)((float)maxHealth - ped->m_fHealth));
        ped->m_fHealth = (float)maxHealth;
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_HEALTH);
        return true;
    }

    if (modelId == MI_PICKUP_BONUS) {
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_INFO);
        return true;
    }

    if (modelId == MI_PICKUP_BRIBE) {
        auto wantedLevel = std::max(+eWantedLevel::WANTED_CLEAN, +FindPlayerPed()->GetWantedLevel() - +eWantedLevel::WANTED_LEVEL_1);
        FindPlayerPed(0)->SetWantedLevel((eWantedLevel)wantedLevel);
        CStats::IncrementStat(STAT_NUMBER_OF_POLICE_BRIBES, 1.0f);
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_INFO);
        return true;
    }

    if (modelId == MI_PICKUP_KILLFRENZY) {
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_INFO);
        return true;
    }

    if (modelId == MODEL_JETPACK) {
        auto* task = new CTaskSimpleJetPack(nullptr, 10.0f, 0, nullptr);
        CEventScriptCommand event(3, task, 0);
        ped->GetEventGroup().Add(&event, false);
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_INFO);
        return true;
    }

    if (modelId == MI_OYSTER) {
        PickedUpOyster();
        return true;
    }

    if (modelId == MI_HORSESHOE) {
        PickedUpHorseShoe();
        return true;
    }

    return false;
}

/*!
 * @brief Check if pickup was picked up, and then mark it as not picked up.
 * @addr 0x454B40
 */
bool CPickups::IsPickUpPickedUp(tPickupReference pickupRef) {
    if (const auto it = rng::find(aPickUpsCollected, pickupRef.num); it != aPickUpsCollected.end()) {
        *it = 0; // Reset
        return true;
    }
    return false;
}

/*!
 * @addr 0x454AC0
 * @returns The `nModelId1` of the given weapon type.
 */
int32 CPickups::ModelForWeapon(eWeaponType weaponType) {
    return CWeaponInfo::GetWeaponInfo(weaponType)->m_nModelId1;
}

/*!
 * @brief Update each pickup's (except if of type NONE or ASSET_REVENUE) `nRegenerationTime` field.
 * @addr 0x455200
 */
void CPickups::PassTime(uint32 time) {
    for (auto& pickup : aPickUps) {
        switch (pickup.m_nPickupType) {
        case PICKUP_NONE:
        case PICKUP_ASSET_REVENUE:
            continue;
        }

        if (pickup.m_nRegenerationTime <= time) {
            pickup.m_nRegenerationTime = 0;
        } else {
            pickup.m_nRegenerationTime -= time;
        }
    }
}

// 0x455390
void CPickups::PickedUpHorseShoe() {
    AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_COLLECTABLE1);

    CStats::IncrementStat(STAT_HORSESHOES_COLLECTED);
    CStats::IncrementStat(STAT_LUCK, 1000.f / CStats::GetStatValue(STAT_TOTAL_HORSESHOES)); // TODO: Is this some inlined stuff? (The division part)

    FindPlayerInfo().m_nMoney += 100; // originally rewarded to the player 1.

    const auto collected = CStats::GetStatValue(STAT_HORSESHOES_COLLECTED);
    const auto total = CStats::GetStatValue(STAT_TOTAL_HORSESHOES);
    if (collected == total) {
        CGarages::TriggerMessage("HO_ALL");
        FindPlayerInfo().m_nMoney += 100'000; // originally rewarded to the player 1.
    } else {
        CGarages::TriggerMessage("HO_ONE", static_cast<int16>(collected), 5000u, static_cast<int16>(total));
    }
}

// 0x4552D0
void CPickups::PickedUpOyster() {
    AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_COLLECTABLE1);

    CStats::IncrementStat(STAT_OYSTERS_COLLECTED);
    FindPlayerInfo().m_nMoney += 100; // originally rewarded to the player 1.

    const auto collected = CStats::GetStatValue(STAT_OYSTERS_COLLECTED);
    const auto total = CStats::GetStatValue(STAT_TOTAL_OYSTERS);
    if (collected == total) {
        CGarages::TriggerMessage("OY_ALL");
        FindPlayerInfo().m_nMoney += 100'000; // originally rewarded to the player 1.
    } else {
        CGarages::TriggerMessage("OY_ONE", static_cast<int16>(collected), 5000u, static_cast<int16>(total));
    }
}

// 0x456A70
void CPickups::PictureTaken() {
    std::optional<size_t> capturedPickup{};
    auto lastFoundDist = 999'999.88f; // maybe FLT_MAX

    for (auto&& [i, pickup] : rngv::enumerate(aPickUps)) {
        if (pickup.m_nPickupType != PICKUP_SNAPSHOT)
            continue;

        const auto pupPos = pickup.GetPosn();
        const auto dist = DistanceBetweenPoints(TheCamera.GetPosition(), pupPos);

        if (90.0f / TheCamera.FindCamFOV() * 20.0f > dist && dist < lastFoundDist) {
            CVector origin = pupPos;
            if (TheCamera.IsSphereVisible(pupPos, 0.2f) || TheCamera.IsSphereVisibleInMirror(pupPos, 0.2f)) {
                capturedPickup = i;
                lastFoundDist = dist;
            }
        }
    }

    if (!capturedPickup.has_value())
        return;

    aPickUps[*capturedPickup].Remove();

    FindPlayerInfo().m_nMoney += 100'000; // originally rewarded to the player 1.

    AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_COLLECTABLE1);
    CStats::IncrementStat(STAT_SNAPSHOTS_TAKEN, 1.0f);

    const auto taken = CStats::GetStatValue(STAT_SNAPSHOTS_TAKEN);
    const auto total = CStats::GetStatValue(STAT_TOTAL_SNAPSHOTS);
    if (taken == total) {
        CGarages::TriggerMessage("SN_ALL");
        FindPlayerInfo().m_nMoney += 100'000;
    } else {
        CGarages::TriggerMessage("SN_ONE", static_cast<int16>(taken), 5000u, static_cast<int16>(total));
    }
}

// 0x4554C0
bool CPickups::PlayerCanPickUpThisWeaponTypeAtThisMoment(eWeaponType weaponType) {
    if (!CWeaponInfo::GetWeaponInfo(weaponType)->flags.bAimWithArm) {
        if (FindPlayerPed()->GetIntelligence()->GetTaskJetPack()) {
            return false;
        }
    }
    return true;
}

// 0x456DE0
void CPickups::RemoveMissionPickUps() {
    for (auto&& [i, pickup] : rngv::enumerate(aPickUps)) {
        switch (pickup.m_nPickupType) {
        case PICKUP_ONCE_FOR_MISSION: {
            CRadar::ClearBlipForEntity(BLIP_PICKUP, GetUniquePickupIndex(i).num);
            pickup.GetRidOfObjects();

            pickup.m_nFlags.bDisabled = true;
            pickup.m_nPickupType = PICKUP_NONE;
            break;
        }
        }
    }
}

// 0x4573D0
void CPickups::RemovePickUp(tPickupReference pickupRef) {
    if (const auto i = GetActualPickupIndex(pickupRef); i != -1) {
        aPickUps[i].Remove();
    }
}

// 0x456D30
void CPickups::RemovePickUpsInArea(float minX, float maxX, float minY, float maxY, float minZ, float maxZ) {
    CBoundingBox bb{ { minX, minY, minZ }, { maxX, maxY, maxZ } }; // They didn't use a bounding box, but it's nicer to do so.

    for (auto& pickup : GetAllActivePickups()) {
        if (bb.IsPointWithin(pickup.GetPosn())) {
            pickup.Remove();
        }
    }
}

// 0x455470
void CPickups::RemovePickupObjects() {
    for (auto& pickup : GetAllActivePickups()) {
        if (pickup.m_pObject) {
            pickup.GetRidOfObjects();
            pickup.m_nFlags.bVisible = false;
        }
    }
}

// remove pickups with types PICKUP_ONCE_TIMEOUT and PICKUP_MONEY in area
// 0x4563A0
void CPickups::RemoveUnnecessaryPickups(const CVector& posn, float radius) {
    for (auto& pickup : aPickUps) {
        switch (pickup.m_nPickupType) {
        case PICKUP_ONCE_TIMEOUT:
        case PICKUP_MONEY: {
            if (IsPointInSphere(pickup.GetPosn(), posn, radius)) {
                pickup.Remove();
            }
            break;
        }
        }
    }
}

// 0x455000
void CPickups::RenderPickUpText() {
    GxtChar msgText[352]{};
    for (const auto& message : std::span{ aMessages.data(), NumMessages }) {
        if (message.price == 0u) {
            if (!message.text)
                continue;

            if (message.flags & 2) {
                CMessages::InsertNumberInString(message.text, 0, 0, 0, 0, 0, 0, msgText);
            }
        } else {
            AsciiToGxtChar(std::format("${:d}", message.price).c_str(), msgText);
        }

        // TODO: scaled wrong in windowed mode, but it's fine in fullscreen.
        auto scaleX = std::min(SCREEN_STRETCH_X(1.0f), message.width / 30.0f);
        auto scaleY = std::min(SCREEN_STRETCH_X(1.0f), message.height / 30.0f);

        CFont::SetProportional(true);
        CFont::SetBackground(false, false);
        CFont::SetScale(scaleX, scaleY);
        CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
        CFont::SetCentreSize(SCREEN_WIDTH);
        CFont::SetColor(message.color);
        CFont::SetFontStyle(eFontStyle::FONT_PRICEDOWN);
        CFont::PrintString(message.pos.x, message.pos.y, msgText);
    }
    NumMessages = 0;
}

// check for pickups in area
// 0x456450
bool CPickups::TestForPickupsInBubble(const CVector posn, float radius) {
    // NOTE: (Possible bug) - They dont check if the pickup is active (eg: type != NONE)
    return rng::any_of(aPickUps, [sp = CSphere{ posn, radius }](const CPickup& p) {
        return sp.IsPointWithin(p.GetPosn()); // They obviously didn't use `CSphere` here, but it's nicer.
    });
}

// search for pickup in area (radius = 5.5 units) with this weapon model and pickup type and add ammo to this pickup; returns TRUE if merged
// 0x4555A0
bool CPickups::TryToMerge_WeaponType(CVector posn, eWeaponType weaponType, ePickupType pickupType, uint32 ammo, bool arg4) {
    const auto mi = CWeaponInfo::GetWeaponInfo(weaponType)->m_nModelId1;

    for (auto& pickup : aPickUps) {
        if (mi == pickup.m_nModelIndex && pickup.m_nPickupType == pickupType && IsPointInSphere(pickup.GetPosn(), posn, 5.0f)) {
            pickup.m_nAmmo += ammo;

            return true;
        }
    }
    return false;
}

// 0x458DE0
void CPickups::Update() {
    ZoneScoped;

    if (CReplay::Mode == MODE_PLAYBACK)
        return;

    auto start = 620 * (CTimer::GetFrameCounter() % 32) / 32;
    auto end   = 620 * (CTimer::GetFrameCounter() % 32 + 1) / 32;
    for (auto i = start; i < end; i++) {
        auto& pickup = aPickUps[i];
        if (pickup.m_nPickupType == PICKUP_NONE)
            continue;

        if (pickup.m_nFlags.bVisible = pickup.IsVisible()) {
            if (!pickup.m_nFlags.bDisabled && !pickup.m_pObject) {
                pickup.GiveUsAPickUpObject(pickup.m_pObject);

                if (auto& obj = pickup.m_pObject; obj) {
                    CWorld::Add(obj);
                }
            }
        } else {
            pickup.GetRidOfObjects();
        }
    }

    const auto pad = CPad::GetPad();
    if (pad->CollectPickupJustDown()) {
        CollectPickupBuffer = 6;
    } else if (CollectPickupBuffer) {
        CollectPickupBuffer--;
    }

    if (PlayerOnWeaponPickup) {
        PlayerOnWeaponPickup--;
    }

    if (pad->GetTarget()) {
        CollectPickupBuffer = 0;
    }

    const auto player1 = FindPlayerPed(PED_TYPE_PLAYER1);
    const auto p1Busy = player1->GetIntelligence()->FindTaskByType(TASK_COMPLEX_ENTER_CAR_AS_DRIVER) || player1->GetIntelligence()->FindTaskByType(TASK_COMPLEX_USE_MOBILE_PHONE);

    start = 620 * (CTimer::GetFrameCounter() % 6) / 6;
    end   = 620 * (CTimer::GetFrameCounter() % 6 + 1) / 6;
    for (auto i = start; i < end; i++) {
        auto& pickup = aPickUps[i];

        if (pickup.m_nPickupType == PICKUP_NONE || !pickup.m_nFlags.bVisible)
            continue;

        if (!p1Busy) {
            if (pickup.Update(FindPlayerPed(), FindPlayerVehicle(), CWorld::PlayerInFocus)) {
                AddToCollectedPickupsArray(i);
            }
        } else if (FindPlayerPed(PED_TYPE_PLAYER2)) {
            if (pickup.Update(FindPlayerPed(1), FindPlayerVehicle(1), PED_TYPE_PLAYER2)) {
                AddToCollectedPickupsArray(i);
            }
        }
    }
}

// 0x455680
void CPickups::UpdateMoneyPerDay(tPickupReference pickupRef, uint16 money) {
    if (auto idx = GetActualPickupIndex(pickupRef); idx != -1) {
        aPickUps[idx].m_nMoneyPerDay = money;
    }
}

// 0x454AE0
eWeaponType CPickups::WeaponForModel(int32 modelId) {
    if (modelId == MI_PICKUP_BODYARMOUR) {
        return WEAPON_ARMOUR;
    }
    if (modelId == MI_PICKUP_HEALTH) {
        return WEAPON_LAST_WEAPON;
    }
    if (modelId == MI_PICKUP_ADRENALINE) {
        return WEAPON_ARMOUR;
    }

    switch (modelId) {
    case MODEL_JETPACK:
        return WEAPON_LAST_WEAPON;

    case MODEL_INVALID:
        return WEAPON_UNARMED;
    }

    if (auto mi = CModelInfo::GetModelInfo(modelId); mi->GetModelType() == MODEL_INFO_WEAPON) {
        return mi->AsWeaponModelInfoPtr()->GetWeaponInfo();
    }

    return WEAPON_UNARMED;
}

// 0x5D35A0
void CPickups::Load() {
    for (auto& pickup : aPickUps) {
        CGenericGameStorage::LoadDataFromWorkBuffer(pickup);
        if (pickup.m_nPickupType != PICKUP_NONE && pickup.m_pObject) {
            pickup.m_pObject = nullptr;
            pickup.m_nFlags.bVisible = false;
        }
    }
    NumMessages = 0u;
    CGenericGameStorage::LoadDataFromWorkBuffer(CPickups::CollectedPickUpIndex);
    CGenericGameStorage::LoadDataFromWorkBuffer(CPickups::DisplayHelpMessage);

    for (auto& collected : aPickUpsCollected) {
        CGenericGameStorage::LoadDataFromWorkBuffer(collected);
    }
}
// 0x5D3540
void CPickups::Save() {
    for (auto& pickup : aPickUps) {
        CGenericGameStorage::SaveDataToWorkBuffer(pickup);
    }
    CGenericGameStorage::SaveDataToWorkBuffer(CPickups::CollectedPickUpIndex);
    CGenericGameStorage::SaveDataToWorkBuffer(CPickups::DisplayHelpMessage);

    for (auto& collected : aPickUpsCollected) {
        CGenericGameStorage::SaveDataToWorkBuffer(collected);
    }
}

// 0x454B70
void ModifyStringLabelForControlSetting(char* stringLabel) {
    const auto len = strlen(stringLabel);

    if (len < 2 || stringLabel[len - 2] != '_')
        return;

    switch (CPad::GetPad(0)->Mode) {
    case 0:
    case 1:
        stringLabel[len - 1] = 'L';
        break;
    case 2:
        stringLabel[len - 1] = 'T';
        break;
    case 3:
        stringLabel[len - 1] = 'C';
        break;
    }
}
