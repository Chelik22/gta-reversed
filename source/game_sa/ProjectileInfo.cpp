#include "StdInc.h"

#include "ProjectileInfo.h"

#include "Entity/Object/Projectile.h"
#include "Radar.h"
#include "World.h"
#include "Pools/Pools.h"
#include "Collision/Box.h"

// -- begin: includes required by the reversed bodies below (batch w2-05-ProjectileInfo)
#include "Timer.h"
#include "General.h"
#include "Camera.h"
#include "Streaming.h"
#include "Explosion.h"
#include "Replay.h"
#include "WeaponInfo.h"
#include "Weapon.h"
#include "Entity/Ped/Ped.h"
#include "Entity/Vehicle/Vehicle.h"
#include "Entity/Physical.h"
#include "Fx/Fx.h"
#include "Fx/FxManager.h"
#include "Fx/FxSystem.h"
#include "Fx/FxPrtMult.h"
#include "Audio/AudioEngine.h"
#include "ObjectData.h"
#include "MemoryMgr.h"
#include "Collision/ColModel.h"
#include "Collision/Sphere.h"
#include "Collision/ColSphere.h"
#include "Models/ModelInfo.h"
#include "Enums/eSurfaceType.h"
// -- end

// FILE-SCOPE: no new StaticRef globals required - `gaProjectileInfo` (0xC891A8) and
// `ms_apProjectile` (0xC89110) are already declared in ProjectileInfo.h:51 / ProjectileInfo.h:29.

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
    RH_ScopedInstall(IsProjectileInRange, 0x739860); // Not part of the batch - already reversed, kept as-is
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
        // NOTE: Original does NOT touch m_pVictim - reproduced here.
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

// 0x737C80
bool CProjectileInfo::AddProjectile(CEntity* creator, eWeaponType weaponType, CVector origin, float force, const CVector* direction, CEntity* victim) {
    CMatrix transform;           // Transform the projectile will be created with
    CVector velocity;            // Initial velocity
    float   elasticity{ 0.75f }; // 0x3F400000
    bool    bSetFxBrain{};       // Original: local_a6 == 5 -> m_pObjectInfo = &CObjectData::ms_aObjectInfo[4] (0xBB4BD0)
    bool    bApplyGravity{ true }; // Original: local_a5 (only rockets are created without gravity)
    int32   destroyTime{};

    const auto creatorType = creator ? creator->GetType() : ENTITY_TYPE_NOTHING;

    switch (weaponType) {
    case WEAPON_GRENADE:
    case WEAPON_REMOTE_SATCHEL_CHARGE: {
        destroyTime = CTimer::m_snTimeInMilliseconds + 2000;

        float speed = 0.0f;
        if (force != StaticRef<float>(0x858B50)) { // 0.0f
            speed = force * StaticRef<float>(0x8717AC) + StaticRef<float>(0x858FCC); // 0.22f + 0.15f
        }
        if (weaponType == WEAPON_REMOTE_SATCHEL_CHARGE) {
            speed *= StaticRef<float>(0x858B8C); // 0.5f
        }

        const float heading = creator
            ? (creatorType == ENTITY_TYPE_VEHICLE
                ? CGeneral::LimitRadianAngle(creator->GetHeading() + StaticRef<float>(0x858CB8)) // PI
                : creator->GetHeading())
            : 0.0f;

        // Original: SetTranslate(0, 0, 0) (0x59AF40) + RotateZ(heading) (0x59B390), then adds the origin
        transform.SetTranslate(CVector{});
        transform.RotateZ(heading);
        transform.GetPosition() += origin;

        velocity.x = sin(heading) * speed * StaticRef<float>(0x858C1C); // -1.0f (ped forward is -Y)
        velocity.y = cos(heading) * speed;
        velocity.z = (force + StaticRef<float>(0x858624)) * StaticRef<float>(0x858EE8) * speed; // (force + 1.0f) * 0.4f

        // Grenades thrown from a Sentinel inherit the car's velocity
        if (creator && creator->GetModelIndex() == MODEL_SENTINEL) { // 0x195
            velocity += static_cast<CPhysical*>(creator)->m_vecMoveSpeed;
        }

        bSetFxBrain  = true;
        bApplyGravity = true;
        elasticity   = 0.5f; // 0x3F000000
        if (weaponType == WEAPON_REMOTE_SATCHEL_CHARGE) {
            elasticity = 0.03f; // 0x3CF5C28F
        }
        break;
    }
    case WEAPON_TEARGAS: {
        destroyTime = CTimer::m_snTimeInMilliseconds + 20000;

        float speed = 0.0f;
        if (force != StaticRef<float>(0x858B50)) { // 0.0f
            speed = force * StaticRef<float>(0x8717AC) + StaticRef<float>(0x858FCC);
        }

        const float heading = creator ? creator->GetHeading() : 0.0f;

        transform.SetTranslate(CVector{});
        transform.RotateZ(heading);
        transform.GetPosition() += origin;

        velocity.x = sin(heading) * speed * StaticRef<float>(0x858C1C);
        velocity.y = cos(heading) * speed;
        velocity.z = (force + StaticRef<float>(0x858624)) * StaticRef<float>(0x858EE8) * speed;

        bSetFxBrain  = true;
        bApplyGravity = true;
        elasticity   = 0.5f;
        break;
    }
    case WEAPON_MOLOTOV: {
        destroyTime = CTimer::m_snTimeInMilliseconds + 2000;

        float speed = force * StaticRef<float>(0x8717AC) + StaticRef<float>(0x858FCC);
        if (speed < StaticRef<float>(0x858CC4)) { // 0.2f
            speed = 0.2f;
        }

        const float heading = creator ? creator->GetHeading() : 0.0f;

        transform.SetTranslate(CVector{});
        transform.RotateZ(heading);
        transform.GetPosition() += origin;

        velocity.x = sin(heading) * speed * StaticRef<float>(0x858C1C);
        velocity.y = cos(heading) * speed;
        velocity.z = (force * StaticRef<float>(0x858CC4) + StaticRef<float>(0x858EE8)) * speed; // (force * 0.2f + 0.4f)
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
            // Fired by the player on foot: use the active camera basis (0x738122).
            // Original reads 0xB6F32C + activeCam * 0x8E => &CCam::m_vecFront, +0x24 => CCam::m_vecUp.
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

        // Original: Multiply3x3 (0x59C790) (transform, CVector{0, speed, 0}) => speed * m_forward row (+0x10)
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
        CStreaming::RequestModel(ModelIndices::MI_FLARE, 0); // 0x8CD75C
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

    // Find a free slot (0x737F8E: m_bActive scan, 0xC89637 = end of the array's m_bActive fields)
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
    case WEAPON_REMOTE_SATCHEL_CHARGE:
    case WEAPON_ROCKET:
    case WEAPON_ROCKET_HS: {
        // Original: CObject::operator new(0x17C) (0x5A1EE0) + CProjectile ctor (0x5A4030)
        obj = new CProjectile{ CWeaponInfo::GetWeaponInfo(weaponType, eWeaponSkill::STD)->m_nModelId1 }; // +0xC
        break;
    }
    case WEAPON_FREEFALL_BOMB: {
        obj = new CProjectile{ CWeaponInfo::GetWeaponInfo(weaponType, eWeaponSkill::STD)->m_nModelId1 };
        break;
    }
    case WEAPON_FLARE: {
        // Original: CObject::operator new(0x17C) + CProjectile ctor with ModelIndices::MI_FLARE (0x8CD75C)
        obj = new CProjectile{ ModelIndices::MI_FLARE };
        if (obj) {
            obj->m_fAirResistance = 0.9f; // 0x3F666666
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

    // (0x738405) Grenade-family only: replace the weapon model's col model with a single collision sphere.
    // The original dereferences the col model without a null-check here.
    if (weaponType == WEAPON_GRENADE || weaponType == WEAPON_TEARGAS || weaponType == WEAPON_MOLOTOV || weaponType == WEAPON_REMOTE_SATCHEL_CHARGE) {
        auto& colModel = *CModelInfo::ms_modelInfoPtrs[obj->GetModelIndex()]->GetColModel(); // modelInfo->m_pColModel (+0x14)
        auto* colData  = colModel.m_pColData;                                               // +0x2C
        if (!colData) {
            colModel.AllocateData(1, 0, 0, 0, 0, false);
            colData = colModel.m_pColData; // Re-read after AllocateData (original: mov eax,[esi+2C] after the call)
        } else if (colData->m_nNumSpheres == 0 && !colData->m_pSpheres) {
            colData->m_nNumSpheres = 1;
            colData->m_pSpheres = static_cast<CColSphere*>(CMemoryMgr::Malloc(sizeof(CColSphere), 0)); // 0x14 bytes
        } else {
            colData = nullptr; // Already has collision volumes - left untouched (original: jmp 0x738562)
        }
        if (colData) {
            colData->m_pSpheres[0].Set( // 0x40FD10, ECX = &m_pSpheres[0]
                colModel.GetBoundRadius() * StaticRef<float>(0x858F34), // * 0.75f
                colModel.GetBoundCenter(),
                SURFACE_GIRDER, // Original passes 0x38
                0,
                tColLighting{ 0xFF }
            );
        }
    }

    info.m_nWeaponType = weaponType;
    info.m_pCreator    = creator;
    if (creator) {
        creator->RegisterReference(&info.m_pCreator); // Original calls this unconditionally (0x7385AD)
    }

    obj->SetMatrix(transform);
    obj->m_vecMoveSpeed = velocity;

    // Grenade-like projectiles are affected by gravity (rockets are not).
    // Original: (flags ^ ((gravity << 1) ^ flags)) & 2 - conditional set of bit 0x2
    obj->physicalFlags.bApplyGravity = bApplyGravity;

    info.m_nDestroyTime = destroyTime;
    obj->m_fElasticity  = elasticity; // +0x9C

    if (bSetFxBrain) {
        // Original: object+0x160 (m_pObjectInfo) = 0xBB4BD0 = &CObjectData::ms_aObjectInfo[4]
        // (the special projectile entry from OBJECT.DAT; ms_aObjectInfo @ 0xBB4A90, sizeof == 0x50)
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

    if (weaponType == WEAPON_TEARGAS) { // Original: (!(0x13) && !(0x14)) && (0x11)
        if (const auto fx = g_fxMan.CreateFxSystem("teargasAD", CVector{}, obj->GetModellingMatrix(), false)) { // 0x4A9BE0, "teargasAD" @ 0x872BF0
            info.m_pFxSystem = fx;
            fx->Play(); // 0x4AA2F0
        }
    }

    obj->m_pEntityIgnoredCollision = creator; // +0x128
    obj->physicalFlags.bCanBeCollidedWith = true; // Original: physicalFlags (+0x40) |= 0x10000000

    // Creator ignores itself (vehicle/ped/object only, and only if not set yet)
    if (creator
        && (creatorType == ENTITY_TYPE_VEHICLE || creatorType == ENTITY_TYPE_PED || creatorType == ENTITY_TYPE_OBJECT)
        && !static_cast<CPhysical*>(creator)->m_pEntityIgnoredCollision
    ) {
        static_cast<CPhysical*>(creator)->m_pEntityIgnoredCollision = creator;
    }

    if (weaponType == WEAPON_ROCKET_HS) {
        // Heat seeking missiles get a radar blip
        const auto  poolRef = GetObjectPool()->GetRef(obj); // (obj - pool->objects) / 0x19C + idx<<8
        const auto  blip    = CRadar::SetEntityBlip(BLIP_OBJECT, poolRef, 0xFF0000FF, BLIP_DISPLAY_BLIPONLY); // 0x5839A0; original passes an unused "CODEPRJ" 5th arg
        CRadar::ChangeBlipScale(blip, 1); // 0x583CC0
        const auto playerPed = FindPlayerPed(-1);       // 0x56E210
        const auto playerVeh = FindPlayerVehicle(-1, false); // 0x56E0D0
        CRadar::ChangeBlipColour(blip, creator == playerPed || creator == playerVeh ? (eBlipColour)0xFFFFFFFF : (eBlipColour)0xFF0000FF); // 0x583AB0
    }

    AudioEngine.ReportWeaponEvent(AE_PROJECTILE_FIRE, weaponType, obj); // 0x506F40 (0x94)

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
        projectile->m_bRemoveFromWorld = true; // Original: m_nFlags |= 0x800
    }
}

// 0x7388F0
void CProjectileInfo::RemoveProjectile(CProjectileInfo* info, CProjectile* projectile) {
    const auto  pos     = projectile->GetPosition();
    auto        creator = info->m_pCreator;
    const auto& type    = info->m_nWeaponType;

    eExplosionType explosionType{};
    bool bExplode = true;
    switch (type) {
    case WEAPON_GRENADE:
        explosionType = EXPLOSION_GRENADE;
        break;
    case WEAPON_MOLOTOV:
        explosionType = EXPLOSION_MOLOTOV;
        AudioEngine.ReportObjectDestruction(projectile); // 0x506ED0
        break;
    case WEAPON_ROCKET:
        explosionType = EXPLOSION_ROCKET;
        if (creator && creator->GetType() == ENTITY_TYPE_VEHICLE) {
            creator = static_cast<CVehicle*>(creator)->m_pDriver; // The driver takes the blame (+0x460)
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
        bExplode = false; // Unknown weapon type: no explosion, but still removed (original: 0x738AB6)
        break;
    }

    if (bExplode) {
        CExplosion::AddExplosion(nullptr, creator, explosionType, pos, 0, true, -1.0f, false); // 0x736A50
    }

    info->m_bActive = false;
    if (info->m_pFxSystem) {
        info->m_pFxSystem->Kill();
        info->m_pFxSystem = nullptr;
    }

    CRadar::ClearBlipForEntity(BLIP_OBJECT, GetObjectPool()->GetRef(projectile)); // 0x587C60
    CWorld::Remove(projectile); // 0x563280
    delete projectile;          // Original: vtable scalar-deleting-dtor(1)
}

// 0x738B20
void CProjectileInfo::Update() {
    if (CReplay::Mode == MODE_PLAYBACK) { // DAT_00A43088
        return;
    }

    for (auto i = 0u; i < MAX_PROJECTILE_INFOS; i++) {
        auto&       info       = gaProjectileInfo[i];
        auto* const projectile = ms_apProjectile[i];
        if (!info.m_bActive || !projectile) {
            continue;
        }

        // Kill the fx once the projectile is submerged (original checks physicalFlags bit 0x100)
        if (projectile->physicalFlags.bSubmergedInWater && info.m_pFxSystem) {
            info.m_pFxSystem->Kill();
            info.m_pFxSystem = nullptr;
        }

        // Get rid of dangling creators (original: type == PED && !CPed::IsPointerValid (0x5E4220))
        if (info.m_pCreator
            && info.m_pCreator->GetType() == ENTITY_TYPE_PED
            && !static_cast<CPed*>(info.m_pCreator)->IsPointerValid()
        ) {
            info.m_pCreator = nullptr;
        }

        const auto weaponType = info.m_nWeaponType;

        // Grenade-like projectiles harden once they come to a halt; tear gas chokes nearby peds
        if (weaponType == WEAPON_GRENADE || weaponType == WEAPON_TEARGAS || weaponType == WEAPON_REMOTE_SATCHEL_CHARGE) {
            if (projectile->m_fElasticity > StaticRef<float>(0x858B1C) // 0.1f
                && std::abs(projectile->m_vecMoveSpeed.x) < StaticRef<float>(0x858C28) // 0.05f
                && std::abs(projectile->m_vecMoveSpeed.y) < StaticRef<float>(0x858C28)
                && std::abs(projectile->m_vecMoveSpeed.z) < StaticRef<float>(0x858C28)
            ) {
                projectile->m_fElasticity = 0.03f; // 0x3CF5C28F
            }

            // Choking starts 0x445C (17500) ms before the tear gas is destroyed
            if (weaponType == WEAPON_TEARGAS
                && info.m_nDestroyTime != 0
                && info.m_nDestroyTime - 0x445C < CTimer::m_snTimeInMilliseconds
                && CGeneral::GetRandomNumberInRange(0, 100) < 10 // 0x407180
            ) {
                const auto pos = projectile->GetPosition();
                CWorld::SetPedsChoking(pos.x, pos.y, pos.z, 6.0f, info.m_pCreator); // 0x565800, radius 0x40C00000
            }
        }

        // Rocket / heat seeking missile smoke trail
        if (weaponType == WEAPON_ROCKET || weaponType == WEAPON_ROCKET_HS) {
            const FxPrtMult_c fxMults{ 0.3f, 0.3f, 0.3f, 0.3f, 0.5f, 1.0f, 0.08f }; // FxPrtMult_c ctor (0x4AB290)

            const CVector velPerFrame = projectile->m_vecMoveSpeed * CTimer::ms_fTimeStep; // 0xB7CB5C

            // Original: FUN_00821B40() - the binary's rand() thunk
            auto numPuffs = plugin::CallAndReturn<int32, 0x821B40>();
            if (numPuffs <= 0) {
                numPuffs = 1;
            }

            for (auto j = 0; j < numPuffs; j++) {
                auto mults{ fxMults };
                const auto shade = (float)CGeneral::GetRandomNumber() * StaticRef<float>(0x858C7C) * StaticRef<float>(0x858C84) + StaticRef<float>(0x858C84); // * 1/32768 * 0.25f + 0.25f
                mults.m_Color.red   = shade;
                mults.m_Color.green = shade;
                mults.m_Color.blue  = shade;
                mults.m_fLife       = (float)CGeneral::GetRandomNumber() * StaticRef<float>(0x858C7C) * StaticRef<float>(0x858CEC) + StaticRef<float>(0x859018); // * 1/32768 * 0.04f + 0.08f

                // Position trails behind the projectile
                const auto frac = 1.0f - (float)j / (float)numPuffs;
                const CVector particlePos = projectile->GetPosition() - velPerFrame * frac;

                // Random spread
                const CVector jitter{
                    ((float)CGeneral::GetRandomNumber() * StaticRef<float>(0x858C7C) + (float)CGeneral::GetRandomNumber() * StaticRef<float>(0x858C7C)) - 1.0f,
                    ((float)CGeneral::GetRandomNumber() * StaticRef<float>(0x858C7C) + (float)CGeneral::GetRandomNumber() * StaticRef<float>(0x858C7C)) - 1.0f,
                    ((float)CGeneral::GetRandomNumber() * StaticRef<float>(0x858C7C) + (float)CGeneral::GetRandomNumber() * StaticRef<float>(0x858C7C)) - 1.0f
                };

                CVector dirNorm = projectile->m_vecMoveSpeed;
                dirNorm.Normalise();
                const CVector particleVel = CrossProduct(dirNorm, jitter) * StaticRef<float>(0x858CE8); // * 1.5f

                g_fx.m_SmokeHuge->AddParticle(particlePos, particleVel, 0.0f, mults, -1.0f, 1.2f, 0.6f, false); // FxSystem_c::AddParticle (0x4AA440)
            }
        }

        // Life time expired? (original compares unsigned: (uint)destroyTime < CTimer::m_snTimeInMilliseconds)
        if (info.m_nDestroyTime != 0 && static_cast<uint32>(info.m_nDestroyTime) < CTimer::m_snTimeInMilliseconds) {
            if (weaponType == WEAPON_REMOTE_SATCHEL_CHARGE) {
                // Satchels are only deactivated when the player is out of detonators
                if (info.m_pCreator
                    && info.m_pCreator->GetType() == ENTITY_TYPE_PED
                    && static_cast<CPed*>(info.m_pCreator)->IsPlayer() // 0x5DF8F0
                ) {
                    auto* ped = static_cast<CPed*>(info.m_pCreator);
                    const auto& wep = ped->m_aWeapons[ped->GetWeaponSlot(WEAPON_DETONATOR)]; // 0x5DF200(0x28); m_aWeapons @ +0x5A0
                    if (wep.m_Type != WEAPON_DETONATOR || wep.m_TotalAmmo == 0) {
                        info.m_nDestroyTime = 0; // Deactivate - stays forever now
                    }
                }
            } else {
                RemoveProjectile(&info, projectile); // 0x7388F0
            }
        } else { // Not expired yet
            switch (weaponType) {
            case WEAPON_ROCKET: {
                // Accelerate along the forward row (original: matrix row +0x10)
                projectile->m_vecMoveSpeed += projectile->GetMatrix().GetForward() * (CTimer::ms_fTimeStep * StaticRef<float>(0x863984)); // * 0.008f

                if (const auto speed = projectile->m_vecMoveSpeed.Magnitude(); speed > StaticRef<float>(0x872C00)) { // 9.9f
                    projectile->m_vecMoveSpeed *= StaticRef<float>(0x872C00) / speed;
                }
                break; // -> collision check below (LAB_007396BB)
            }
            case WEAPON_FLARE: {
                projectile->m_bUsesCollision = false;
                CWorld::pIgnoreEntity = info.m_pCreator; // 0xB7CD68
                const bool bClear = CWorld::GetIsLineOfSightClear(info.m_vecLastPosn, projectile->GetPosition(), true, true, true, true, false, false, false); // 0x56A490
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

                // Only start colliding/exploding once far enough away from the creator (0x858CA0 == 2.0f)
                if (!info.m_pCreator
                    || CVector::DistSqr(info.m_vecLastPosn, info.m_pCreator->GetPosition()) >= StaticRef<float>(0x858CA0)
                ) { // LAB_007397C7
                    if (projectile->physicalFlags.bOnSolidSurface // Original: physicalFlags bit 0x200
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
                        AudioEngine.ReportFrontendAudioEvent(AE_MISSILE_LOCK, 0.0f, 1.0f); // 0x506EA0, 0x65
                    }

                    const auto aimingDir  = projectile->GetMatrix().GetForward(); // forward row (+0x10)
                    const auto origin     = projectile->GetPosition() + aimingDir;

                    // Score of the currently locked target (0x73E560)
                    const float victimScore = CWeapon::EvaluateTargetForHeatSeekingMissile(info.m_pVictim, origin, aimingDir, StaticRef<float>(0x858F08), true, nullptr); // 1.2f

                    // Flares are chased too - find the best one
                    CEntity* bestTarget{};
                    float    bestScore{};
                    for (auto j = 0u; j < MAX_PROJECTILE_INFOS; j++) {
                        const auto& otherInfo = gaProjectileInfo[j];
                        if (otherInfo.m_nWeaponType != WEAPON_FLARE || !otherInfo.m_bActive) {
                            continue;
                        }
                        if (const float score = CWeapon::EvaluateTargetForHeatSeekingMissile(ms_apProjectile[j], origin, aimingDir, StaticRef<float>(0x858F08), true, nullptr); score >= bestScore) {
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

                    CVector predictedPos = projectile->GetPosition() + projectile->m_vecMoveSpeed * StaticRef<float>(0x858628); // * 100.0f
                    if (bIsLocalPlayerPlane) {
                        predictedPos = targetPos;
                    }

                    // Desired direction & knock strength
                    float knock = std::min(StaticRef<float>(0x858B40), CVector::Dist(predictedPos, targetPos)); // 50.0f
                    if (bIsLocalPlayerPlane) {
                        knock = std::min(StaticRef<float>(0x858CE8), CVector::Dist(predictedPos, targetPos)); // 1.5f
                    }

                    CVector toTarget = (targetPos + static_cast<CPhysical*>(target)->m_vecMoveSpeed * knock) - predictedPos;
                    toTarget.Normalise();

                    const CVector oldVel = projectile->m_vecMoveSpeed;

                    // Remove the velocity component that goes away from the target
                    if (const float dot = DotProduct(toTarget, oldVel); dot < StaticRef<float>(0x858B50)) { // 0.0f
                        toTarget -= oldVel * dot;
                    }
                    toTarget.Normalise();

                    float damping = 1.0f;
                    float steer   = StaticRef<float>(0x872BFC); // 0.0117f
                    if (info.m_pCreator != FindPlayerPed(-1) && info.m_pCreator != FindPlayerVehicle(-1, false)) {
                        steer = 0.009f;
                    }
                    if (static_cast<CPhysical*>(target)->m_vecMoveSpeed.Magnitude() > StaticRef<float>(0x858C98)) { // 0.8f
                        steer *= StaticRef<float>(0x858F08); // 1.2f
                    }
                    if (bIsLocalPlayerPlane) {
                        damping = plugin::CallAndReturn<float, 0x822130>(); // Original calls FUN_00822130
                        steer   = StaticRef<float>(0x8D6108);               // 0.15f
                    }

                    projectile->m_vecMoveSpeed *= damping;
                    projectile->m_vecMoveSpeed += toTarget * (steer * CTimer::ms_fTimeStep);

                    if (const auto speed = projectile->m_vecMoveSpeed.Magnitude(); speed > StaticRef<float>(0x872C00)) {
                        projectile->m_vecMoveSpeed *= StaticRef<float>(0x872C00) / speed;
                    }

                    // Nose follows the previous velocity (original: matrix row +0x10)
                    projectile->GetMatrix().GetForward() = oldVel;
                }
                break; // -> collision check below (LAB_007396BB)
            }
            default: { // WEAPON_GRENADE / WEAPON_TEARGAS / WEAPON_REMOTE_SATCHEL_CHARGE
                if (weaponType == WEAPON_REMOTE_SATCHEL_CHARGE) {
                    // Stick to whatever it damaged (+0xD8 m_fDamageIntensity, +0xDC m_pDamageEntity, +0xFC m_pAttachedTo)
                    if (projectile->m_fDamageIntensity > StaticRef<float>(0x858B50) // 0.0f
                        && projectile->m_pDamageEntity
                        && !projectile->m_pAttachedTo
                    ) {
                        projectile->AttachEntityToEntity(static_cast<CPhysical*>(projectile->m_pDamageEntity), CVector{}, CVector{}); // 0x54D690
                        projectile->m_bUsesCollision = false; // Original: m_nFlags &= ~1
                    }
                }
                break;
            }
            }

            // Rockets & heat seeking missiles: explode on whatever they hit (LAB_007396BB).
            // bOnSolidSurface (bit 0x200) only skips the LOS test - the collided-entities check
            // below still runs and removes the projectile.
            if (weaponType == WEAPON_ROCKET || weaponType == WEAPON_ROCKET_HS) {
                bool bBlocked = true;
                if (!projectile->physicalFlags.bOnSolidSurface) {
                    CWorld::pIgnoreEntity = info.m_pCreator;
                    projectile->m_bUsesCollision = false;
                    bBlocked = !CWorld::GetIsLineOfSightClear(info.m_vecLastPosn, projectile->GetPosition(), true, true, true, true, false, false, false);
                    projectile->m_bUsesCollision = true;
                    CWorld::pIgnoreEntity = nullptr;
                    projectile->m_pEntityIgnoredCollision = info.m_pCreator;
                }

                if (bBlocked) {
                    if (CEntity* collided{}; // Original: +0xB9 = m_nNumEntitiesCollided, +0xBC = m_apCollidedEntities[0]
                        projectile->m_nNumEntitiesCollided == 0
                        || !(collided = projectile->m_apCollidedEntities[0])
                        || (collided != info.m_pCreator && collided->GetModelIndex() != MODEL_MISSILE) // 0x159
                    ) {
                        RemoveProjectile(&info, projectile);
                    }
                }
            }
        }

        // LAB_00739812 - note: the original reads the (possibly freed) projectile here too
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
        // NOTE: Original does NOT null out ms_apProjectile[i] here (dangling pointer kept) - reproduced.
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
