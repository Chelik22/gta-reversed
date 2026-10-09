#include "StdInc.h"
#include "Tasks/TaskTypes/TaskSimpleFight.h"
#include "PedStats.h"

#include <reversiblebugfixes/Bugs.hpp>
#include "PedDamageResponseCalculator.h"

void CPedDamageResponseCalculator::InjectHooks() {
    RH_ScopedClass(CPedDamageResponseCalculator);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x4AD3F0);
    RH_ScopedInstall(AccountForPedDamageStats, 0x4AD430);
    RH_ScopedInstall(AccountForPedArmour, 0x4AD550);
    RH_ScopedInstall(ComputeWillForceDeath, 0x4AD610);
    RH_ScopedInstall(ComputeWillKillPed, 0x4B3210);
    // KEEP {.Reversed = false}: 0x4B5C2A is INSIDE ComputeDamageResponse (0x4B5AC0) in the original -
    // IsBleedingWeapon was inlined there. Enabling this hook would write a JMP into the middle of the
    // (still unreversed) ComputeDamageResponse and corrupt it. Body already present and correct.
    RH_ScopedInstall(IsBleedingWeapon, 0x4B5C2A);
    RH_ScopedInstall(ComputeDamageResponse, 0x4B5AC0); // not in batch w2-06 - kept unreversed
}

// 0x4AD3F0
CPedDamageResponseCalculator::CPedDamageResponseCalculator(const CEntity* entity, float fDamage, eWeaponType weaponType, ePedPieceTypes bodyPart, bool bSpeak) {
    m_pDamager      = entity;
    m_fDamageFactor = fDamage;
    m_bodyPart      = bodyPart;
    m_weaponType    = weaponType;
    m_bSpeak        = bSpeak;
}

CPedDamageResponseCalculator* CPedDamageResponseCalculator::Constructor(CEntity* entity, float fDamage, eWeaponType weaponType, ePedPieceTypes bodyPart, bool bSpeak) {
    this->CPedDamageResponseCalculator::CPedDamageResponseCalculator(entity, fDamage, weaponType, bodyPart, bSpeak);
    return this;
}

/*!
 *
 * @param ped
 * @param response
 * @addr 0x4AD430
 */
void CPedDamageResponseCalculator::AccountForPedDamageStats(CPed* ped, CPedDamageResponse& response) {
    const auto bSameAsStaticDamage = m_fDamageFactor == ms_damageFactor;

    if (ped->IsPlayer()) {
        m_fDamageFactor = m_fDamageFactor * 0.33f; // 0x85AD74
    } else {
        m_fDamageFactor = ped->m_pStats->m_fDefendWeakness * m_fDamageFactor; // CPedStat+0x2C
    }

    const auto damager = (m_pDamager && m_pDamager->GetIsTypePed())
        ? m_pDamager->AsPed()
        : nullptr;

    if (damager && damager->IsPlayer()) {
        // Damager is the player: always beef up the damage when killing friendlies/peds of own group
        if (CPedGroups::AreInSameGroup(ped, damager)) {
            m_fDamageFactor = std::max(12.5f, m_fDamageFactor * 0.1f);
        } else if (m_weaponType != WEAPON_EXPLOSION && m_weaponType != WEAPON_FLAMETHROWER && !bSameAsStaticDamage) {
            m_fDamageFactor = std::max(12.5f, m_fDamageFactor * 0.1f);
        }
        return;
    }

    // Friendly fire between friends also gets beefed up
    if (ped && ped->IsPlayer() && damager && CPedIntelligence::AreFriends(*ped, *damager)) {
        m_fDamageFactor = std::max(12.5f, m_fDamageFactor * 0.1f);
        return;
    }

    if (!m_pDamager || !damager || ped->GetCreatedBy() != PED_MISSION
        || !CPedGroups::AreInSameGroup(ped, damager)
        || m_weaponType == WEAPON_EXPLOSION || m_weaponType == WEAPON_FLAMETHROWER) {
        return;
    }

    m_fDamageFactor = std::max(12.5f, m_fDamageFactor * 0.1f);
}

/*!
 *
 * @param ped
 * @param response
 * @addr 0x4AD550
 */
void CPedDamageResponseCalculator::AccountForPedArmour(CPed* ped, CPedDamageResponse& response) {
    if (ped->m_fArmour == 0.0f)
        return;

    if (m_weaponType == WEAPON_DROWNING || m_weaponType == WEAPON_FALL)
        return;

    if (FindPlayerPed() == ped)
        CWorld::Players[CWorld::PlayerInFocus].m_nLastTimeArmourLost = CTimer::GetTimeInMS();

    if (m_fDamageFactor > ped->m_fArmour) {
        m_fDamageFactor = m_fDamageFactor - ped->m_fArmour;
        response.m_fDamageArmor = ped->m_fArmour;
        ped->m_fArmour = 0.0f;
    } else {
        response.m_fDamageArmor = m_fDamageFactor;
        ped->m_fArmour = ped->m_fArmour - m_fDamageFactor;
        m_fDamageFactor = 0.0f;
    }
}

/*!
 *
 * @param ped
 * @addr Added in Android
 */
void CPedDamageResponseCalculator::AdjustPedDamage(CPed* ped) {
    // TODO: Reverted function from GTA SA 2.10 version ABI : arm64-v8a (x64)
    // offset: 0x444370

    if (!ped) {
        return;
    }

    const auto plyr = FindPlayerPed(-1);
    if (!plyr) {
        return;
    }

    const bool isUnarmedBrassOrParachute = m_weaponType == WEAPON_UNARMED
        || m_weaponType == WEAPON_BRASSKNUCKLE
        || m_weaponType == WEAPON_PARACHUTE;

    if (CCheat::m_aCheatsActive[CHEAT_COUNTRY_TRAFFIC] && m_weaponType <= eWeaponType::WEAPON_PARACHUTE && isUnarmedBrassOrParachute) {
        m_fDamageFactor = ped->m_fHealth;
    }

    if (plyr == ped && CTheScripts::pActiveScripts && !strcmp(CTheScripts::pActiveScripts->m_szName, "intro1")) {
        m_fDamageFactor = m_fDamageFactor * 0.77f;
    }
}

/*!
 *
 * @param ped
 * @param response
 * @return
 * @addr 0x4AD610
 */
bool CPedDamageResponseCalculator::ComputeWillForceDeath(CPed* ped, CPedDamageResponse& response) {
    if (ped->bNoCriticalHits)
        return false;

    switch (m_weaponType) {
    case WEAPON_KATANA: {
        const auto damager = m_pDamager && m_pDamager->GetIsTypePed() ? m_pDamager->AsPed() : nullptr;
        if (!damager || !damager->IsPlayer())
            return false;

        const auto fight = damager->GetIntelligence()->GetTaskFighting();
        if (!fight)
            return false;

        if (fight->m_nComboSet != 11)
            return false;

        if (fight->m_nCurrentMove == FIGHT_ATTACK_HIT_3)
            return (rand() & 1) != 0; // 50%

        return (rand() & 7) == 0;     // 1/8
    }
    case WEAPON_GRENADE:
    case WEAPON_RLAUNCHER:
    case WEAPON_RLAUNCHER_HS:
    case WEAPON_REMOTE_SATCHEL_CHARGE:
    case WEAPON_EXPLOSION:
        if (!CLocalisation::KillPeds())
            return false;
        if (ped->IsPlayer())
            return false;
        if (ped->bInVehicle)
            return false;
        return ped->m_fHealth < m_fDamageFactor + 1.0f;
    case WEAPON_PISTOL:
    case WEAPON_PISTOL_SILENCED:
    case WEAPON_DESERT_EAGLE:
    case WEAPON_SHOTGUN:
    case WEAPON_SAWNOFF_SHOTGUN:
    case WEAPON_SPAS12_SHOTGUN:
    case WEAPON_MICRO_UZI:
    case WEAPON_MP5:
    case WEAPON_AK47:
    case WEAPON_M4:
    case WEAPON_TEC9:
    case WEAPON_COUNTRYRIFLE:
    case WEAPON_SNIPERRIFLE:
    case WEAPON_MINIGUN:
    case WEAPON_UZI_DRIVEBY: {
        // 0 == "death candidate"
        const auto deathRoll = (ped->IsPlayer() || ped->bNoCriticalHits)
            ? 1
            : (m_weaponType == WEAPON_COUNTRYRIFLE || m_weaponType == WEAPON_SNIPERRIFLE)
                ? 0
                : rand() & 7;

        const auto damager = m_pDamager && m_pDamager->GetIsTypePed() ? m_pDamager->AsPed() : nullptr;

        const auto dieInVehicle      = ped->bInVehicle && ped->m_pVehicle && !ped->IsPlayer() && !ped->bNoCriticalHits;
        const auto dieFromPlayerAim  = damager && damager->IsPlayer() && damager->GetPlayerData()
            && (damager->GetPlayerData()->m_nTargetBone == PED_NODE_LEFT_HAND
                || (damager->GetPlayerData()->m_nPlayerFlags & 8)); // m_bFreeAiming

        if (!dieInVehicle && !dieFromPlayerAim && deathRoll != 0)
            return false;

        return m_bodyPart == PED_PIECE_HEAD;
    }
    default:
        return false;
    }
}

/*!
 *
 * @param ped
 * @param response
 * @param bSpeak
 * @addr 0x4B3210
 */
void CPedDamageResponseCalculator::ComputeWillKillPed(CPed* ped, CPedDamageResponse& rsp, bool bSpeak) {
    if (ped->IsPlayer()) {
        if (CCheat::IsActive(CHEAT_NO_ONE_CAN_STOP_US) && m_weaponType < WEAPON_LAST_WEAPON) {
            return;
        }
    }

    rsp.m_bForceDeath = ComputeWillForceDeath(ped, rsp);

    if (CCheat::IsActive(CHEAT_MEGA_PUNCH)) {
        if (notsa::contains({ WEAPON_UNARMED, WEAPON_BRASSKNUCKLE, WEAPON_PARACHUTE }, m_weaponType)) {
            m_fDamageFactor = ped->m_fHealth;
        }
    }

    if (!rsp.m_bForceDeath && m_weaponType == WEAPON_FALL && m_bSpeak) {
        const auto prevHP   = ped->m_fHealth;
        ped->m_fHealth      = std::max(prevHP - m_fDamageFactor, 5.0f);
        rsp.m_bHealthZero   = false;
        rsp.m_fDamageHealth = prevHP - ped->m_fHealth;
        if (bSpeak) {
            ped->Say(CTX_GLOBAL_PAIN_LOW, 0, 1.0, 0, 0, 0);
        }
        return;
    }

    if (rsp.m_bForceDeath || ped->m_fHealth - m_fDamageFactor < 1.0f) {
        rsp.m_fDamageHealth = ped->m_fHealth;
        rsp.m_bHealthZero   = true;
        ped->m_fHealth      = 0.0f;
        return;
    }

    rsp.m_bHealthZero   = false;
    rsp.m_fDamageHealth = m_fDamageFactor;
    ped->m_fHealth      = ped->m_fHealth - m_fDamageFactor;

    if (bSpeak) {
        ped->Say(
            m_weaponType == WEAPON_DROWNING
                ? CTX_GLOBAL_PAIN_CJ_DROWNING
                : m_fDamageFactor < 5.0f && ped->m_fHealth > 10.0f
                    ? CTX_GLOBAL_PAIN_LOW
                    : CTX_GLOBAL_PAIN_HIGH
        );
    }
}

/*!
 *
 * @param ped
 * @return
 * @addr 0x4B5C2A inlined
 */
bool CPedDamageResponseCalculator::IsBleedingWeapon(CPed* ped) const {
    if (ped->IsPlayer())
        return false;

    if (m_weaponType == WEAPON_KNIFE || m_weaponType == WEAPON_KATANA || m_weaponType == WEAPON_CHAINSAW)
        return true;

    return false;
}

/*!
 *
 * @param ped
 * @param response
 * @param bSpeak
 * @addr 0x4B5AC0
 */
void CPedDamageResponseCalculator::ComputeDamageResponse(CPed* ped, CPedDamageResponse& response, bool bSpeak) {
    plugin::CallMethod<0x4B5AC0, CPedDamageResponseCalculator*, CPed*, CPedDamageResponse&, bool>(this, ped, response, bSpeak);
}

