#include "StdInc.h"

#include "ProjectileInfo.h"

#include "Entity/Object/Projectile.h"
#include "ObjectData.h"
#include "Radar.h"
#include "World.h"
#include "Pools/Pools.h"
#include "Collision/Box.h"

#include "Timer.h"
#include "General.h"
#include "Common.h"
#include "Camera.h"
#include "Streaming.h"
#include "Explosion.h"
#include "Replay.h"
#include "WeaponInfo.h"
#include "ModelIndices.h"
#include "Entity/Ped/Ped.h"
#include "Entity/Vehicle/Vehicle.h"
#include "Entity/Physical.h"
#include "Fx/Fx.h"
#include "Fx/FxSystem.h"
#include "Fx/FxPrtMult.h"
#include "Audio/AudioEngine.h"

// Constants (values read from the 1.0 US binary)
namespace {
    constexpr auto FORCE_SPEED_SCALE     = 0.22f; // 0x8717AC - speed = force * this + FORCE_SPEED_BIAS
    constexpr auto FORCE_SPEED_BIAS      = 0.15f; // 0x858FCC
    constexpr auto SATCHEL_SPEED_MULT    = 0.5f;  // 0x858B8C
    constexpr auto MOLOTOV_MIN_SPEED     = 0.2f;  // 0x858CC4
    constexpr auto GRENADE_Z_SPEED_MULT  = 0.4f;  // 0x858EE8
    constexpr auto FLARE_AIR_RESISTANCE  = 0.92f; // 0x3F666666
    constexpr auto TEARGAS_CHOKING_BEGIN = 17500u; // 0x445C - ms before destruction when choking starts
    constexpr auto MAX_SPEED_LIMIT       = 9.9f;  // 0x872C00
    constexpr auto ROCKET_ACCEL          = 0.008f;   // 0x863984 - forward acceleration per timestep
    constexpr auto HS_STEER_LOCAL_PLAYER = 0.0117f;  // 0x872BFC
    constexpr auto HS_STEER_OTHER        = 0.009f;   // (immediate constant)
    constexpr auto HS_STEER_PLANES       = 0.15f;    // 0x8D6108
    constexpr auto HS_KNOCK_MAX_DIST     = 50.0f;    // 0x858B40
    constexpr auto HS_KNOCK_MAX_DIST_PLANE = 1.5f;   // 0x858CE8
    constexpr auto HS_FAST_TARGET_SPEED  = 0.8f;     // 0x858C98
    constexpr auto HS_FAST_TARGET_MULT   = 1.2f;     // 0x858F08
    constexpr auto HS_PREDICT_TIME_MULT  = 100.0f;   // 0x858628
    constexpr auto MOLOTOV_CREATOR_DIST_SQ = 2.0f;   // 0x858CA0 - min squared distance from creator before it can explode
    constexpr auto RAND_MAX_F_RECIP      = 3.0518509447574615e-05f; // 0x858C7C - 1 / (RAND_MAX + 1)
} // namespace

void CProjectileInfo::InjectHooks() {
    RH_ScopedClass(CProjectileInfo);
    RH_ScopedCategoryGlobal();

    // Install("CProjectileInfo", "", , &CProjectileInfo::);
    RH_ScopedInstall(Initialise, 0x737B40);
    RH_ScopedInstall(Shutdown, 0x737BC0);
    RH_ScopedInstall(GetProjectileInfo, 0x737BF0);
    RH_ScopedInstall(RemoveNotAdd, 0x737C00);
    RH_ScopedInstall(AddProjectile, 0x737C80);
    RH_ScopedInstall(RemoveDetonatorProjectiles, 0x738860);
    RH_ScopedInstall(RemoveProjectile, 0x7388F0);
    RH_ScopedInstall(Update, 0x738B20);
    RH_ScopedInstall(IsProjectileInRange, 0x739860);
    RH_ScopedInstall(RemoveAllProjectiles, 0x7399B0);
    RH_ScopedInstall(RemoveIfThisIsAProjectile, 0x739A40);
    RH_ScopedInstall(RemoveFXSystem, 0x737B80);
}

// 0x737B40
void CProjectileInfo::Initialise() {
    for (auto& projectile : ms_apProjectile) {
        projectile = nullptr;
    }

    for (auto& info : gaProjectileInfo) {
        info.m_nWeaponType  = WEAPON_GRENADE; // 0x10
        info.m_pCreator     = nullptr;
        info.m_nDestroyTime = 0;
        info.m_bActive      = false;
        info.m_pFxSystem    = nullptr;
    }
}

// 0x737BC0
void CProjectileInfo::Shutdown() {
    for (auto& info : gaProjectileInfo) {
        if (info.m_pFxSystem) {
            g_fxMan.DestroyFxSystem(info.m_pFxSystem);
            info.m_pFxSystem = nullptr;
        }
    }
}

// 0x737BF0
CProjectileInfo* CProjectileInfo::GetProjectileInfo(int32 infoId) {
    return &gaProjectileInfo[infoId];
}

// 0x737C00
void CProjectileInfo::RemoveNotAdd(CEntity* creator, eWeaponType weaponType, CVector pos) {
    eExplosionType explosionType;
    switch (weaponType) {
    case WEAPON_GRENADE:
    case WEAPON_REMOTE_SATCHEL_CHARGE:
        explosionType = EXPLOSION_GRENADE;
        break;
    case WEAPON_MOLOTOV:
        explosionType = EXPLOSION_MOLOTOV;
        break;
    case WEAPON_ROCKET:
    case WEAPON_ROCKET_HS:
        explosionType = EXPLOSION_ROCKET;
        break;
    default:
        return;
    }
    CExplosion::AddExplosion(nullptr, creator, explosionType, pos, 0, true, -1.0f, false);
}

// 0x737B80
void CProjectileInfo::RemoveFXSystem(bool bInstantly) {
    if (!m_pFxSystem) {
        return;
    }

    if (bInstantly) {
        g_fxMan.DestroyFxSystem(m_pFxSystem);
    } else {
        m_pFxSystem->Kill();
    }
    m_pFxSystem = nullptr;
}

// 0x737C80
bool CProjectileInfo::AddProjectile(CEntity* creator, eWeaponType weaponType, CVector origin, float force, const CVector* direction, CEntity* victim) {
    CMatrix transform;           // Transform the projectile will be created with
    CVector velocity;            // Initial velocity
    float   elasticity{ 0.75f }; // 0x3F400000
    bool    bSetFxBrain{};       // Original: local_a6 == 5 -> sets m_pObjectInfo to the special object info entry at 0xBB4BD0
    bool    bApplyGravity{ true }; // Original: local_a5 (only rockets are created without gravity)
    int32   destroyTime{};

    const auto creatorType = creator ? creator->GetType() : ENTITY_TYPE_NOTHING;

    switch (weaponType) {
    case WEAPON_GRENADE:
    case WEAPON_REMOTE_SATCHEL_CHARGE: {
        destroyTime = CTimer::m_snTimeInMilliseconds + 2000;

        float speed = 0.0f;
        if (force != 0.0f) {
            speed = force * FORCE_SPEED_SCALE + FORCE_SPEED_BIAS;
        }
        if (weaponType == WEAPON_REMOTE_SATCHEL_CHARGE) {
            speed *= SATCHEL_SPEED_MULT;
        }

        const float heading = creator
            ? (creatorType == ENTITY_TYPE_VEHICLE
                ? CGeneral::LimitRadianAngle(creator->GetHeading() + PI) // 0x858CB8 == PI
                : creator->GetHeading())
            : 0.0f;

        // Original: SetTranslate(0, 0, 0) (0x59AF40) + RotateZ(heading) (0x59B390), then adds the origin
        transform.SetTranslate(CVector{});
        transform.RotateZ(heading);
        transform.GetPosition() += origin;

        velocity.x = sin(heading) * speed * -1.0f; // 0x858C1C == -1.0f (ped forward is -Y)
        velocity.y = cos(heading) * speed;
        velocity.z = (force + 1.0f) * GRENADE_Z_SPEED_MULT * speed;

        // Grenades thrown from a Sentinel inherit the car's velocity
        if (creator && creator->GetModelIndex() == MODEL_SENTINEL) { // 0x195
            velocity += static_cast<CPhysical*>(creator)->m_vecMoveSpeed;
        }

        bSetFxBrain = true;
        bApplyGravity = true;
        elasticity  = 0.5f; // 0x3F000000
        if (weaponType == WEAPON_REMOTE_SATCHEL_CHARGE) {
            elasticity = 0.03f; // 0x3CF5C28F
        }
        break;
    }
    case WEAPON_TEARGAS: {
        destroyTime = CTimer::m_snTimeInMilliseconds + 20000;

        float speed = 0.0f;
        if (force != 0.0f) {
            speed = force * FORCE_SPEED_SCALE + FORCE_SPEED_BIAS;
        }

        const float heading = creator ? creator->GetHeading() : 0.0f;

        transform.SetTranslate(CVector{});
        transform.RotateZ(heading);
        transform.GetPosition() += origin;

        velocity.x = sin(heading) * speed * -1.0f;
        velocity.y = cos(heading) * speed;
        velocity.z = (force + 1.0f) * GRENADE_Z_SPEED_MULT * speed;

        bSetFxBrain = true;
        bApplyGravity = true;
        elasticity  = 0.5f;
        break;
    }
    case WEAPON_MOLOTOV: {
        destroyTime = CTimer::m_snTimeInMilliseconds + 2000;

        float speed = force * FORCE_SPEED_SCALE + FORCE_SPEED_BIAS;
        if (speed < MOLOTOV_MIN_SPEED) {
            speed = 0.2f;
        }

        const float heading = creator ? creator->GetHeading() : 0.0f;

        transform.SetTranslate(CVector{});
        transform.RotateZ(heading);
        transform.GetPosition() += origin;

        velocity.x = sin(heading) * speed * -1.0f;
        velocity.y = cos(heading) * speed;
        velocity.z = (force * MOLOTOV_MIN_SPEED + GRENADE_Z_SPEED_MULT) * speed;
        break;
    }
    case WEAPON_ROCKET:
    case WEAPON_ROCKET_HS: {
        destroyTime = CTimer::m_snTimeInMilliseconds + (weaponType == WEAPON_ROCKET ? 3000 : 10000);
        bApplyGravity = false; // Original: local_a5 = 0 (only case where it is set to false)

        float speed = weaponType == WEAPON_ROCKET ? 0.4f : 0.2f;

        if (creatorType == ENTITY_TYPE_VEHICLE) {
            transform = creator->GetMatrix(); // 0x411990 + 0x59BBC0
            transform.GetPosition() = origin;
            // Original adds the magnitude of a unit row of the creator's matrix (0x4082C0) => +1.0f
            speed += transform.GetRight().Magnitude();
        } else if (creatorType == ENTITY_TYPE_PED && static_cast<CPed*>(creator)->IsPlayer()) {
            // Fired by the player on foot: use the active camera basis.
            // Original reads 0xB6F32C + activeCam * sizeof(CCam) => &CCam::m_vecFront,
            // and +0x24 from it => CCam::m_vecUp. (0x738122)
            const auto& cam = TheCamera.GetActiveCam();
            transform.GetRight()    = CrossProduct(cam.m_vecUp, cam.m_vecFront);
            transform.GetForward()  = cam.m_vecFront;
            transform.GetUp()       = cam.m_vecUp;
            transform.GetPosition() = origin;
        } else if (direction) {
            // Explicit direction given: forward = direction; right = creator's right; up = Cross(right, forward)
            transform.GetRight()    = creator->GetMatrix().GetRight();
            transform.GetForward()  = *direction;
            transform.GetUp()       = CrossProduct(transform.GetRight(), *direction);
            transform.GetPosition() = origin;
        } else {
            transform = creator ? creator->GetMatrix() : CMatrix{};

            transform.GetPosition() = origin;
            // See note above (0x4082C0) => +1.0f
            speed += transform.GetRight().Magnitude();
        }

        // Original: Multiply3x3(0x59C790)(transform, CVector{0, speed, 0}) => speed * forward row
        velocity = transform.GetForward() * speed;
        break;
    }
    case WEAPON_FREEFALL_BOMB: {
        destroyTime = CTimer::m_snTimeInMilliseconds + 2000000;

        if (creatorType >= ENTITY_TYPE_VEHICLE && creatorType <= ENTITY_TYPE_OBJECT) {
            velocity = static_cast<CPhysical*>(creator)->m_vecMoveSpeed;
        }
        transform = creator ? creator->GetMatrix() : CMatrix{};
        transform.GetPosition() = origin;
        break;
    }
    case WEAPON_FLARE: {
        CStreaming::RequestModel(ModelIndices::MI_FLARE, 0);
        destroyTime = CTimer::m_snTimeInMilliseconds + 10000;

        if (creatorType >= ENTITY_TYPE_VEHICLE && creatorType <= ENTITY_TYPE_OBJECT) {
            velocity = static_cast<CPhysical*>(creator)->m_vecMoveSpeed;
        }
        transform = creator ? creator->GetMatrix() : CMatrix{};
        transform.GetPosition() = origin;
        break;
    }
    default:
        break;
    }

    // Find a free slot
    uint32 slot = 0;
    while (slot < MAX_PROJECTILE_INFOS && gaProjectileInfo[slot].m_bActive) {
        slot++;
    }
    if (slot == MAX_PROJECTILE_INFOS) { // LAB_00738574 - all slots busy
        return false;
    }

    // (LAB_0073839B) Create the projectile object
    CProjectile* obj{};
    switch (weaponType) {
    case WEAPON_GRENADE:
    case WEAPON_TEARGAS:
    case WEAPON_MOLOTOV:
    case WEAPON_ROCKET:
    case WEAPON_ROCKET_HS:
    case WEAPON_FREEFALL_BOMB:
    case WEAPON_REMOTE_SATCHEL_CHARGE: {
        // Original: CObject::operator new(0x17C) (0x5A1EE0) + CProjectile ctor (0x5A4030) with the weapon's model
        obj = new CProjectile{ CWeaponInfo::GetWeaponInfo(weaponType, eWeaponSkill::STD)->m_nModelId1 };
        break;
    }
    case WEAPON_FLARE: {
        // Original: CObject::operator new(0x17C) + CProjectile ctor with ModelIndices::MI_FLARE (0x8CD75C)
        obj = new CProjectile{ ModelIndices::MI_FLARE };
        if (obj) {
            obj->m_fAirResistance = FLARE_AIR_RESISTANCE;
        }
        break;
    }
    default:
        break;
    }
    ms_apProjectile[slot] = obj;
    if (!obj) { // No free pool slot
        return false;
    }

    auto& info = gaProjectileInfo[slot];

    info.m_nWeaponType = weaponType;
    info.m_pCreator    = creator;
    if (creator) {
        creator->RegisterReference(&info.m_pCreator);
    }

    obj->SetMatrix(transform);
    obj->m_vecMoveSpeed = velocity;

    // Grenade-like projectiles are affected by gravity (rockets are not)
    obj->physicalFlags.bApplyGravity = bApplyGravity;

    info.m_nDestroyTime = destroyTime;
    obj->m_fElasticity  = elasticity;

    if (bSetFxBrain) {
        // Original: object+0x160 (m_pObjectInfo) = 0xBB4BD0, which is &CObjectData::ms_aObjectInfo[4]
        // (the special projectile entry from OBJECT.DAT).
        obj->m_pObjectInfo = &CObjectData::GetAtIndex(4);
    }

    info.m_pVictim = victim;
    if (victim) {
        victim->RegisterReference(&info.m_pVictim);
    }

    info.m_bActive = true;

    CWorld::Add(obj);
    obj->RegisterReference(reinterpret_cast<CEntity**>(&ms_apProjectile[slot]));

    info.m_vecLastPosn = obj->GetPosition();

    if (weaponType == WEAPON_TEARGAS) {
        if (const auto fx = g_fxMan.CreateFxSystem("teargasAD", CVector{}, obj->GetModellingMatrix(), false)) {
            info.m_pFxSystem = fx;
            fx->Play();
        }
    }

    obj->m_pEntityIgnoredCollision = creator;
    obj->physicalFlags.bCanBeCollidedWith = true; // Original: physicalFlags (+0x40) |= 0x10000000

    // Creator ignores itself (vehicle/ped/object only, and only if not set yet)
    if (creator
        && (creatorType == ENTITY_TYPE_VEHICLE || creatorType == ENTITY_TYPE_PED || creatorType == ENTITY_TYPE_OBJECT)
        && !static_cast<CPhysical*>(creator)->m_pEntityIgnoredCollision) {
        static_cast<CPhysical*>(creator)->m_pEntityIgnoredCollision = creator;
    }

    if (weaponType == WEAPON_ROCKET_HS) {
        // Heat seeking missiles get a radar blip ("CODEPRJ")
        const auto  poolRef = GetObjectPool()->GetRef(obj);
        const auto  blip    = CRadar::SetEntityBlip(BLIP_OBJECT, poolRef, 0xFF0000FF, (eBlipDisplay)2);
        CRadar::ChangeBlipScale(blip, 1);
        const auto playerPed = FindPlayerPed(-1);
        const auto playerVeh = FindPlayerVehicle(-1, false);
        CRadar::ChangeBlipColour(blip, creator == playerPed || creator == playerVeh ? (eBlipColour)0xFFFFFFFF : (eBlipColour)0xFF0000FF);
    }

    AudioEngine.ReportWeaponEvent(0x94, weaponType, obj);

    return true;
}

// 0x738860
void CProjectileInfo::RemoveDetonatorProjectiles() {
    for (auto i = 0u; i < MAX_PROJECTILE_INFOS; i++) {
        auto&       info       = gaProjectileInfo[i];
        auto* const projectile = ms_apProjectile[i];
        if (!info.m_bActive || info.m_nWeaponType != WEAPON_REMOTE_SATCHEL_CHARGE) {
            continue;
        }

        CExplosion::AddExplosion(nullptr, info.m_pCreator, EXPLOSION_GRENADE, projectile->GetPosition(), 0, true, -1.0f, false);

        info.m_bActive = false;
        if (info.m_pFxSystem) {
            info.m_pFxSystem->Kill();
            info.m_pFxSystem = nullptr;
        }
        projectile->m_nFlags |= 0x800; // Detonated satchels won't take further damage
    }
}

// 0x7399B0
void CProjectileInfo::RemoveAllProjectiles() {
    for (auto i = 0u; i < MAX_PROJECTILE_INFOS; i++) {
        auto&       info       = gaProjectileInfo[i];
        auto* const projectile = ms_apProjectile[i];
        if (!info.m_bActive) {
            continue;
        }

        info.m_bActive = false;
        if (info.m_pFxSystem) {
            g_fxMan.DestroyFxSystem(info.m_pFxSystem);
            info.m_pFxSystem = nullptr;
        }

        CRadar::ClearBlipForEntity(BLIP_OBJECT, GetObjectPool()->GetRef(projectile));
        CWorld::Remove(projectile);
        delete projectile;
    }
}

// 0x739A40
bool CProjectileInfo::RemoveIfThisIsAProjectile(CObject* object) {
    for (auto i = 0u; i < MAX_PROJECTILE_INFOS; i++) {
        auto& info = gaProjectileInfo[i];
        if (ms_apProjectile[i] != object || !info.m_bActive) {
            continue;
        }

        info.m_bActive = false;
        if (info.m_pFxSystem) {
            info.m_pFxSystem->Kill();
            info.m_pFxSystem = nullptr;
        }

        CRadar::ClearBlipForEntity(BLIP_OBJECT, GetObjectPool()->GetRef(object));
        CWorld::Remove(object);
        delete object;
        ms_apProjectile[i] = nullptr;
        return true;
    }
    return false;
}

// 0x7388F0
void CProjectileInfo::RemoveProjectile(CProjectileInfo* info, CProjectile* projectile) {
    const auto  pos     = projectile->GetPosition();
    auto        creator = info->m_pCreator;
    const auto& type    = info->m_nWeaponType;

    eExplosionType explosionType{};
    switch (type) {
    case WEAPON_GRENADE:
        explosionType = EXPLOSION_GRENADE;
        break;
    case WEAPON_MOLOTOV:
        explosionType = EXPLOSION_MOLOTOV;
        AudioEngine.ReportObjectDestruction(projectile);
        break;
    case WEAPON_ROCKET:
        explosionType = EXPLOSION_ROCKET;
        if (creator && creator->GetType() == ENTITY_TYPE_VEHICLE) {
            creator = static_cast<CVehicle*>(creator)->m_pDriver; // The driver takes the blame
        }
        break;
    case WEAPON_ROCKET_HS: {
        explosionType = creator == FindPlayerPed(-1) ? EXPLOSION_ROCKET : EXPLOSION_WEAK_ROCKET;
        break;
    }
    case WEAPON_FREEFALL_BOMB:
        explosionType = EXPLOSION_GRENADE;
        break;
    default:
        return;
    }

    CExplosion::AddExplosion(nullptr, creator, explosionType, pos, 0, true, -1.0f, false);

    info->m_bActive = false;
    if (info->m_pFxSystem) {
        info->m_pFxSystem->Kill();
        info->m_pFxSystem = nullptr;
    }

    CRadar::ClearBlipForEntity(BLIP_OBJECT, GetObjectPool()->GetRef(projectile));
    CWorld::Remove(projectile);
    delete projectile;
}

// 0x738B20
void CProjectileInfo::Update() {
    if (CReplay::Mode == MODE_PLAYBACK) {
        return;
    }

    for (auto i = 0u; i < MAX_PROJECTILE_INFOS; i++) {
        auto&       info       = gaProjectileInfo[i];
        auto* const projectile = ms_apProjectile[i];
        if (!info.m_bActive || !projectile) {
            continue;
        }

        // Kill the fx once the projectile has come to a halt / is submerged (original checks flag bit 0x100)
        if (projectile->physicalFlags.bSubmergedInWater && info.m_pFxSystem) {
            info.m_pFxSystem->Kill();
            info.m_pFxSystem = nullptr;
        }

        // Get rid of dangling creators
        if (info.m_pCreator
            && info.m_pCreator->GetType() == ENTITY_TYPE_PED
            && !static_cast<CPed*>(info.m_pCreator)->IsPointerValid()) {
            info.m_pCreator = nullptr;
        }

        const auto weaponType = info.m_nWeaponType;

        // Grenade-like projectiles slow down over time; tear gas chokes nearby peds
        if (weaponType == WEAPON_GRENADE || weaponType == WEAPON_TEARGAS || weaponType == WEAPON_REMOTE_SATCHEL_CHARGE) {
            if (projectile->m_fElasticity > 0.1f
                && std::abs(projectile->m_vecMoveSpeed.x) < 0.05f
                && std::abs(projectile->m_vecMoveSpeed.y) < 0.05f
                && std::abs(projectile->m_vecMoveSpeed.z) < 0.05f) {
                projectile->m_fElasticity = 0.03f;
            }

            if (weaponType == WEAPON_TEARGAS
                && info.m_nDestroyTime != 0
                && info.m_nDestroyTime - TEARGAS_CHOKING_BEGIN < CTimer::m_snTimeInMilliseconds
                && CGeneral::GetRandomNumberInRange(0, 100) < 10
            ) {
                const auto pos = projectile->GetPosition();
                CWorld::SetPedsChoking(pos.x, pos.y, pos.z, 6.0f, info.m_pCreator);
            }
        }

        // Rocket / heat seeking missile smoke trail
        if (weaponType == WEAPON_ROCKET || weaponType == WEAPON_ROCKET_HS) {
            const FxPrtMult_c fxMults{ 0.3f, 0.3f, 0.3f, 0.3f, 0.5f, 1.0f, 0.08f }; // 0x4AB290

            const CVector velPerFrame = projectile->m_vecMoveSpeed * CTimer::ms_fTimeStep;

            // Original: FUN_00821B40() - previously identified as the binary's rand() thunk
            auto numPuffs = plugin::CallAndReturn<int32, 0x821B40>();
            if (numPuffs <= 0) {
                numPuffs = 1;
            }

            for (auto j = 0; j < numPuffs; j++) {
                auto mults{ fxMults };
                const auto shade = (float)CGeneral::GetRandomNumber() * RAND_MAX_F_RECIP * 0.25f + 0.25f;
                mults.m_Color.red   = shade;
                mults.m_Color.green = shade;
                mults.m_Color.blue  = shade;
                mults.m_fLife       = (float)CGeneral::GetRandomNumber() * RAND_MAX_F_RECIP * 0.04f + 0.08f;

                // Position trails behind the projectile
                const auto frac = 1.0f - (float)j / (float)numPuffs;
                const CVector particlePos = projectile->GetPosition() - velPerFrame * frac;

                // Random spread
                const CVector jitter{
                    ((float)CGeneral::GetRandomNumber() * RAND_MAX_F_RECIP + (float)CGeneral::GetRandomNumber() * RAND_MAX_F_RECIP) - 1.0f,
                    ((float)CGeneral::GetRandomNumber() * RAND_MAX_F_RECIP + (float)CGeneral::GetRandomNumber() * RAND_MAX_F_RECIP) - 1.0f,
                    ((float)CGeneral::GetRandomNumber() * RAND_MAX_F_RECIP + (float)CGeneral::GetRandomNumber() * RAND_MAX_F_RECIP) - 1.0f
                };

                CVector dirNorm = projectile->m_vecMoveSpeed;
                dirNorm.Normalise();
                const CVector particleVel = CrossProduct(dirNorm, jitter) * 1.5f;

                g_fx.m_SmokeHuge->AddParticle(particlePos, particleVel, 0.0f, mults, -1.0f, 1.2f, 0.6f, false);
            }
        }

        // Life time expired?
        if (info.m_nDestroyTime != 0 && info.m_nDestroyTime < (int32)CTimer::m_snTimeInMilliseconds) {
            if (weaponType == WEAPON_REMOTE_SATCHEL_CHARGE) {
                // Satchels are only removed when the player is out of detonators
                if (info.m_pCreator
                    && info.m_pCreator->GetType() == ENTITY_TYPE_PED
                    && static_cast<CPed*>(info.m_pCreator)->IsPlayer()
                ) {
                    auto* ped = static_cast<CPed*>(info.m_pCreator);
                    const auto& wep = ped->m_aWeapons[ped->GetWeaponSlot(WEAPON_DETONATOR)];
                    if (wep.m_Type != WEAPON_DETONATOR || wep.m_TotalAmmo == 0) {
                        info.m_nDestroyTime = 0; // Deactivate - stays forever now
                    }
                }
            } else {
                RemoveProjectile(&info, projectile);
            }
        } else { // Not expired yet
            switch (weaponType) {
            case WEAPON_ROCKET: {
                // Accelerate along the forward vector (original: matrix row at +0x10)
                projectile->m_vecMoveSpeed += projectile->GetMatrix().GetForward() * (CTimer::ms_fTimeStep * ROCKET_ACCEL);

                if (const auto speed = projectile->m_vecMoveSpeed.Magnitude(); speed > MAX_SPEED_LIMIT) {
                    projectile->m_vecMoveSpeed *= MAX_SPEED_LIMIT / speed;
                }
                break; // -> collision check below
            }
            case WEAPON_FLARE: {
                projectile->m_bUsesCollision = false;
                CWorld::pIgnoreEntity = info.m_pCreator;
                const bool bClear = CWorld::GetIsLineOfSightClear(info.m_vecLastPosn, projectile->GetPosition(), true, true, true, true, false, false, false);
                projectile->m_bUsesCollision = true;
                CWorld::pIgnoreEntity = nullptr;

                if (!bClear) { // Hit something - stop moving
                    projectile->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
                    projectile->SetPosn(info.m_vecLastPosn);
                }
                break;
            }
            case WEAPON_MOLOTOV:
            case WEAPON_FREEFALL_BOMB: {
                const auto pos = projectile->GetPosition();
                CWorld::pIgnoreEntity = info.m_pCreator;
                projectile->m_bUsesCollision = false;

                // Only start colliding/exploding once far enough away from the creator
                if (!info.m_pCreator
                    || CVector::DistSqr(info.m_vecLastPosn, info.m_pCreator->GetPosition()) >= MOLOTOV_CREATOR_DIST_SQ
                ) {
                    if (projectile->physicalFlags.bOnSolidSurface // Original: flag bit 0x200
                        || !CWorld::GetIsLineOfSightClear(info.m_vecLastPosn, pos, true, true, true, true, false, false, false)
                    ) {
                        RemoveProjectile(&info, projectile);
                    }
                }

                CWorld::pIgnoreEntity = nullptr;
                projectile->m_bUsesCollision = true;
                break;
            }
            case WEAPON_ROCKET_HS: {
                // Heat seeking steering
                if (info.m_pVictim) {
                    if (info.m_pVictim == FindPlayerVehicle(-1, false)) {
                        AudioEngine.ReportFrontendAudioEvent((eAudioEvents)0x65, 0.0f, 1.0f);
                    }

                    const auto aimingDir  = projectile->GetMatrix().GetForward(); // forward (original: matrix row +0x10)
                    const auto origin     = projectile->GetPosition() + aimingDir;

                    // Score of the currently locked target
                    const float victimScore = CWeapon::EvaluateTargetForHeatSeekingMissile(info.m_pVictim, origin, aimingDir, 1.2f, true, nullptr);

                    // Flares are chased too - find the best one
                    CEntity* bestTarget{};
                    float    bestScore{};
                    for (auto j = 0u; j < MAX_PROJECTILE_INFOS; j++) {
                        const auto& otherInfo = gaProjectileInfo[j];
                        if (otherInfo.m_nWeaponType != WEAPON_FLARE || !otherInfo.m_bActive) {
                            continue;
                        }
                        if (const float score = CWeapon::EvaluateTargetForHeatSeekingMissile(ms_apProjectile[j], origin, aimingDir, 1.2f, true, nullptr); score >= bestScore) {
                            bestScore  = score;
                            bestTarget = ms_apProjectile[j];
                        }
                    }

                    CEntity* target;
                    if (!bestTarget || bestScore <= victimScore) {
                        target = info.m_pVictim;
                    } else {
                        target = bestTarget;
                    }

                    // Special handling: local player fired at their own plane
                    const bool bIsLocalPlayerPlane = target->GetType() == ENTITY_TYPE_VEHICLE
                        && (info.m_pCreator == FindPlayerPed(-1) || info.m_pCreator == FindPlayerVehicle(-1, false))
                        && static_cast<CVehicle*>(target)->m_nVehicleSubType == VEHICLE_TYPE_PLANE; // +0x594

                    const auto targetPos = target->GetPosition();

                    CVector predictedPos = projectile->GetPosition() + projectile->m_vecMoveSpeed * HS_PREDICT_TIME_MULT;
                    if (bIsLocalPlayerPlane) {
                        predictedPos = targetPos;
                    }

                    // Desired direction & knock strength
                    float knock = std::min(HS_KNOCK_MAX_DIST, CVector::Dist(predictedPos, targetPos));
                    if (bIsLocalPlayerPlane) {
                        knock = std::min(HS_KNOCK_MAX_DIST_PLANE, CVector::Dist(predictedPos, targetPos));
                    }

                    CVector toTarget = (targetPos + static_cast<CPhysical*>(target)->m_vecMoveSpeed * knock) - predictedPos;
                    toTarget.Normalise();

                    const CVector oldVel = projectile->m_vecMoveSpeed;

                    // Remove the velocity component that goes away from the target
                    if (const float dot = DotProduct(toTarget, oldVel); dot < 0.0f) {
                        toTarget -= oldVel * dot;
                    }
                    toTarget.Normalise();

                    float damping = 1.0f;
                    float steer   = HS_STEER_LOCAL_PLAYER;
                    if (info.m_pCreator != FindPlayerPed(-1) && info.m_pCreator != FindPlayerVehicle(-1, false)) {
                        steer = HS_STEER_OTHER;
                    }
                    if (static_cast<CPhysical*>(target)->m_vecMoveSpeed.Magnitude() > HS_FAST_TARGET_SPEED) {
                        steer *= HS_FAST_TARGET_MULT;
                    }
                    if (bIsLocalPlayerPlane) {
                        damping = plugin::CallAndReturn<float, 0x822130>(); // Original calls FUN_00822130
                        steer   = HS_STEER_PLANES;
                    }

                    projectile->m_vecMoveSpeed *= damping;
                    projectile->m_vecMoveSpeed += toTarget * (steer * CTimer::ms_fTimeStep);

                    if (const auto speed = projectile->m_vecMoveSpeed.Magnitude(); speed > MAX_SPEED_LIMIT) {
                        projectile->m_vecMoveSpeed *= MAX_SPEED_LIMIT / speed;
                    }

                    // Nose follows the previous velocity (original: matrix row +0x10)
                    projectile->GetMatrix().GetForward() = oldVel;
                }
                break; // -> collision check below
            }
            default: { // WEAPON_GRENADE / WEAPON_TEARGAS / WEAPON_REMOTE_SATCHEL_CHARGE
                if (weaponType == WEAPON_REMOTE_SATCHEL_CHARGE) {
                    // Stick to whatever it damaged
                    if (projectile->m_fDamageIntensity > 0.0f && projectile->m_pDamageEntity && !projectile->m_pAttachedTo) {
                        projectile->AttachEntityToEntity(static_cast<CPhysical*>(projectile->m_pDamageEntity), CVector{}, CVector{});
                        projectile->m_bUsesCollision = false;
                    }
                }
                break;
            }
            }

            // Rockets & heat seeking missiles: explode on whatever they hit
            if (weaponType == WEAPON_ROCKET || weaponType == WEAPON_ROCKET_HS) {
                if (!projectile->physicalFlags.bOnSolidSurface) { // Original: flag bit 0x200
                    CWorld::pIgnoreEntity = info.m_pCreator;
                    projectile->m_bUsesCollision = false;
                    const bool bClear = CWorld::GetIsLineOfSightClear(info.m_vecLastPosn, projectile->GetPosition(), true, true, true, true, false, false, false);
                    projectile->m_bUsesCollision = true;
                    CWorld::pIgnoreEntity = nullptr;
                    projectile->m_pEntityIgnoredCollision = info.m_pCreator;

                    if (!bClear) { // Hit something
                        if (CEntity* collided{}; // Original: 0xB9 = m_nNumEntitiesCollided, 0xBC = m_apCollidedEntities[0]
                            projectile->m_nNumEntitiesCollided == 0
                            || !(collided = projectile->m_apCollidedEntities[0])
                            || (collided != info.m_pCreator && collided->GetModelIndex() != MODEL_MISSILE)
                        ) {
                            RemoveProjectile(&info, projectile);
                        }
                    }
                }
            }
        }

        info.m_vecLastPosn = projectile->GetPosition();
    }
}

// 0x739860
bool CProjectileInfo::IsProjectileInRange(float x1, float x2, float y1, float y2, float z1, float z2, bool bDestroy) {
    const CBox bb{
        CVector{ x1, y1, z1 },
        CVector{ x2, y2, z2 }
    };
    bool found = false;
    for (auto&& [info, proj] : rngv::zip(gaProjectileInfo, ms_apProjectile)) {
        if (!info.m_bActive) {
            continue;
        }

        if (!IsWeaponTypeProjectile(static_cast<eWeaponType>(info.m_nWeaponType))) {
            continue;
        }

        if (!bb.IsPointInside(proj->GetPosition())) {
            continue;
        }

        found = true;
        if (bDestroy) {
            info.m_bActive = false;
            info.RemoveFXSystem(false);
            CRadar::ClearBlipForEntity(BLIP_OBJECT, GetObjectPool()->GetRef(proj));
            CWorld::Remove(proj);
            delete proj;
        }
    }
    return found;
}
