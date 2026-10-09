// ============================================================================
// new_Entity_Vehicle_Plane.cpp - ПОЛНАЯ ЗАМЕНА для source/game_sa/Entity/Vehicle/Plane.cpp
// Интеграция батча plane-1 (D:\projects\keep\batch_out\plane-1\), 2026-10-09.
// Влито 9 функций: ProcessControl(0x6C9260), PreRender(0x6C94A0),
// ProcessControlInputs(0x6CADD0), ProcessFlyingCarStuff(0x6CB7C0),
// VehicleDamage(0x6CC4B0), SwitchAmbientPlanes(0x6CCC50), BlowUpCar(0x6CCCF0),
// FindPlaneCreationCoors(0x6CD090), DoPlaneGenerationAndRemoval(0x6CD2F0).
// Все redirect-стабы функций ВНЕ батча сохранены.
// Применённые фиксы помечены комментарием "FIX(int-plane):" - список в notes_int-plane.md.
// ============================================================================
#include "StdInc.h"

#include "Plane.h"
#include "CarCtrl.h"
#include "FireManager.h"
// FIX(int-plane) [I-1]: добавлены инклуды (в StdInc.h их нет):
#include "MotionBlurStreaks.h"   // CMotionBlurStreaks::RegisterStreak - PreRender (0x6C94A0)
#include "Events/EventDanger.h"  // CEventDanger - DoPlaneGenerationAndRemoval (0x6CD2F0)

auto& HARRIER_NOZZLE_ROTATERATE = StaticRef<float>(0x8D33DC);       // 25.0f
auto& PLANE_DAMAGE_WAVE_COUNTER_VAR = StaticRef<float>(0x8D33E0);   // 0.75f
auto& PLANE_DAMAGE_THRESHHOLD = StaticRef<float>(0x8D33E4);         // 500.0f
auto& PLANE_DAMAGE_SCALE_MASS = StaticRef<float>(0x8D33E8);         // 10000.0f
auto& PLANE_DAMAGE_DESTROY_THRESHHOLD = StaticRef<float>(0x8D33EC); // 5000.0f
auto& vecRCBaronGunPos = StaticRef<CVector>(0x8D33F0);            // <0.0f, 0.45f, 0.0f>

void CPlane::InjectHooks() {
    RH_ScopedVirtualClass(CPlane, 0x871948, 71);
    RH_ScopedCategory("Vehicle");

    RH_ScopedInstall(Constructor, 0x6C8E20);
    RH_ScopedInstall(InitPlaneGenerationAndRemoval, 0x6CAD90);
    RH_ScopedVMTInstall(SetUpWheelColModel, 0x6C9140);
    RH_ScopedVMTInstall(BurstTyre, 0x6C9150);
    RH_ScopedVMTInstall(PreRender, 0x6C94A0);                  // FIX(int-plane) [I-0]: снят {.Reversed = false}
    RH_ScopedVMTInstall(Render, 0x6CAB70);
    RH_ScopedInstall(IsAlreadyFlying, 0x6CAB90);
    RH_ScopedVMTInstall(Fix, 0x6CABB0);
    RH_ScopedVMTInstall(SetupDamageAfterLoad, 0x6CAC10);
    RH_ScopedInstall(SetGearUp, 0x6CAC20);
    RH_ScopedInstall(SetGearDown, 0x6CAC70);
    RH_ScopedVMTInstall(OpenDoor, 0x6CACB0);
    RH_ScopedVMTInstall(ProcessControl, 0x6C9260);             // FIX(int-plane) [I-0]: снят {.Reversed = false}
    RH_ScopedVMTInstall(ProcessControlInputs, 0x6CADD0);       // FIX(int-plane) [I-0]: снят {.Reversed = false}
    RH_ScopedVMTInstall(ProcessFlyingCarStuff, 0x6CB7C0);      // FIX(int-plane) [I-0]: снят {.Reversed = false}
    RH_ScopedVMTInstall(VehicleDamage, 0x6CC4B0);              // FIX(int-plane) [I-0]: снят {.Reversed = false}
    RH_ScopedInstall(CountPlanesAndHelis, 0x6CCA50);
    RH_ScopedInstall(AreWeInNoPlaneZone, 0x6CCAA0);
    RH_ScopedInstall(AreWeInNoBigPlaneZone, 0x6CCBB0);
    RH_ScopedInstall(SwitchAmbientPlanes, 0x6CCC50);           // FIX(int-plane) [I-0]: снят {.Reversed = false}
    RH_ScopedVMTInstall(BlowUpCar, 0x6CCCF0);                  // FIX(int-plane) [I-0]: снят {.Reversed = false}
    RH_ScopedInstall(FindPlaneCreationCoors, 0x6CD090);        // FIX(int-plane) [I-0]: снят {.Reversed = false}
    RH_ScopedInstall(DoPlaneGenerationAndRemoval, 0x6CD2F0);   // FIX(int-plane) [I-0]: снят {.Reversed = false}
}

// 0x6C8E20
CPlane::CPlane(int32 modelIndex, eVehicleCreatedBy createdBy) : CAutomobile(modelIndex, createdBy, true) {
    m_nVehicleSubType = VEHICLE_TYPE_PLANE;

    m_fLeftRightSkid               = 0.0f;
    m_fSteeringUpDown              = 0.0f;
    m_fSteeringLeftRight           = 0.0f;
    m_fAccelerationBreakStatus     = 0.0f;
    m_fAccelerationBreakStatusPrev = 1.0f;
    m_fPropSpeed                   = 0.0f;
    field_9C8                      = 0.0f;
    m_LandingGearAngle           = 0.0f;
    m_StallCounter                = 0;
    m_planeCreationHeading         = 0.0f;
    m_planeHeading                 = 0.0f;
    m_planeHeadingPrev             = 0.0f;
    m_maxAltitude                  = 15.0f;
    m_altitude                     = 25.0f;
    m_minAltitude                  = 20.0f;
    m_forwardZ                     = 0;
    m_nStartedFlyingTime           = 0;
    m_fSteeringFactor              = 0.0f;

    if (m_nModelIndex != MODEL_VORTEX)
        physicalFlags.bDontCollideWithFlyers = true;

    m_nExtendedRemovalRange = 255;
    vehicleFlags.bNeverUseSmallerRemovalRange = true;
    vehicleFlags.bIsBig = true;

    auto& leftDoor = m_doors[DOOR_LEFT_FRONT];
    switch (modelIndex) {
    case MODEL_HYDRA:
    case MODEL_RUSTLER:
    case MODEL_CROPDUST:
        m_damageManager.SetDoorStatus(DOOR_LEFT_FRONT, DAMSTATE_OK);
        leftDoor.Init((3.0f * PI) / 5.0f, 0.0f, DOOR_AXIS_NEG_X, DOOR_AXIS_Y, DOOR_EXTRA_BASED);
        break;
    case MODEL_SHAMAL:
        m_damageManager.SetDoorStatus(DOOR_LEFT_FRONT, DAMSTATE_OK);
        leftDoor.Init(-((3.0f * PI) / 4.0f), 0.0f, DOOR_AXIS_Z, DOOR_AXIS_Y, DOOR_EXTRA_BASED);
        rwObjectSetFlags(GetFirstObject(m_aCarNodes[PLANE_WHEEL_LF]), 0);
        break;
    case MODEL_NEVADA:
        m_damageManager.SetDoorStatus(DOOR_LEFT_FRONT, DAMSTATE_OK);
        leftDoor.Init(-TWO_PI / 5.0f, 0.0f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
        break;
    case MODEL_VORTEX:
        if (m_panels[FRONT_LEFT_PANEL].m_nFrameId == (uint16)-1)
            m_panels[FRONT_LEFT_PANEL].SetPanel(PLANE_GEAR_L, 1, -0.25f);
        break;
    case MODEL_STUNT:
        m_damageManager.SetDoorStatus(DOOR_LEFT_FRONT, DAMSTATE_OK);
        leftDoor.Init((3.0f * PI) / 5.0f, 0.0f, DOOR_AXIS_NEG_X, DOOR_AXIS_Y, DOOR_EXTRA_BASED);
        rwObjectSetFlags(GetFirstObject(m_aCarNodes[PLANE_WHEEL_LB]), 0);
        rwObjectSetFlags(GetFirstObject(m_aCarNodes[PLANE_WHEEL_RB]), 0);
        break;
    }

    CVector modelPos, localPos;
    for (auto wheelId = 0; wheelId < 4; wheelId++) {
        GetVehicleModelInfo()->GetWheelPosn(wheelId, modelPos, false);
        GetVehicleModelInfo()->GetWheelPosn(wheelId, localPos, true);
        m_wheelPosition[wheelId] = m_wheelPosition[wheelId] - modelPos.z + localPos.z;
    }

    m_planeDamageWave = 0;
    m_pGunParticles = nullptr;
    m_nFiringMultiplier = 16;
    field_9DC = 0;
    field_9E0 = 0;
    m_apJettrusParticles.fill(nullptr);

    m_pSmokeParticle = nullptr;

    if (m_nModelIndex == MODEL_HYDRA)
        m_wMiscComponentAngle = HARRIER_NOZZLE_ROTATE_LIMIT;

    m_bSmokeEjectorEnabled = false;
}

// 0x6C9160
CPlane::~CPlane() {
    if (m_pGunParticles) {
        for (auto i = 0; i < CVehicle::GetPlaneNumGuns(); i++) {
            if (auto& particle = m_pGunParticles[i]) {
                particle->Kill();
                g_fxMan.DestroyFxSystem(particle);
            }
        }
        delete[] m_pGunParticles;
        m_pGunParticles = nullptr;
    }

    for (auto particle : m_apJettrusParticles) {
        if (particle) {
            FxSystem_c::KillAndClear(particle);
        }
    }

    FxSystem_c::SafeKillAndClear(m_pSmokeParticle);

    m_vehicleAudio.Terminate();
}

// 0x6CAD90
void CPlane::InitPlaneGenerationAndRemoval() {
    GenPlane_Status = 0;
    GenPlane_LastTimeGenerated = 0;
    GenPlane_Active = true;
}

// 0x6CCCF0
void CPlane::BlowUpCar(CEntity* damager, bool bHideExplosion) {
    if (!vehicleFlags.bCanBeDamaged)
        return;

    if (GetStatus() != STATUS_PLAYER && m_autoPilot.m_nCarMission != MISSION_PLANE_CRASH_AND_BURN && m_nModelIndex != MODEL_RCBARON) {
        m_autoPilot.SetCarMission(MISSION_PLANE_CRASH_AND_BURN);
        m_fHealth = 0.0f;
        return;
    }

    if (damager == FindPlayerPed() || damager == FindPlayerVehicle()) {
        FindPlayerInfo().m_nHavocCaused += 20;
        FindPlayerInfo().m_fCurrentChaseValue += 10.0f;
        CStats::IncrementStat(STAT_COST_OF_PROPERTY_DAMAGED, (float)CGeneral::GetRandomNumberInRange(4000, 10'000));
    }

    if (GetStatus() == STATUS_PLAYER) {
        if (m_pDriver) {
            m_pDriver->bDontRender = true;
        }
        for (auto& passenger : m_apPassengers) {
            if (passenger) {
                passenger->bDontRender = true;
            }
        }
        m_nFlags &= 0xFFFFFF7E;
        ResetMoveSpeed();
        ResetTurnSpeed();
    }

    SetStatus(STATUS_WRECKED);
    physicalFlags.bRenderScorched = true;
    m_nTimeWhenBlowedUp = CTimer::GetTimeInMS();
    CVisibilityPlugins::SetClumpForAllAtomicsFlag(GetRpClump(), eAtomicComponentFlag::ATOMIC_PIPE_NO_EXTRA_PASSES_LOD);
    m_damageManager.FuckCarCompletely(false);
    if (m_nModelIndex != MODEL_RCBARON) {
        CAutomobile::SetBumperDamage(FRONT_BUMPER, false);
        CAutomobile::SetBumperDamage(REAR_BUMPER, false);
        CAutomobile::SetDoorDamage(DOOR_BONNET, false);
        CAutomobile::SetDoorDamage(DOOR_BOOT, false);
        CAutomobile::SetDoorDamage(DOOR_LEFT_FRONT, false);
        CAutomobile::SetDoorDamage(DOOR_RIGHT_FRONT, false);
        CAutomobile::SetDoorDamage(DOOR_LEFT_REAR, false);
        CAutomobile::SetDoorDamage(DOOR_RIGHT_REAR, false);
        CAutomobile::SpawnFlyingComponent(static_cast<eCarNodes>(PLANE_WHEEL_LF), 1);

        // убрать флаги у атомика колеса (damager при этом сохраняется для StartFire/AddExplosion ниже)
        if (auto node = m_aCarNodes[PLANE_WHEEL_LF]) {
            RwObject* wheelAtomic = nullptr;
            RwFrameForAllObjects(node, GetCurrentAtomicObjectCB, &wheelAtomic);
            if (wheelAtomic) {
                rwObjectSetFlags(wheelAtomic, 0);
            }
        }
    }
    m_nBombOnBoard = 0;
    m_fHealth = 0.0f;
    m_wBombTimer = 0;

    TheCamera.CamShake(0.4f, GetPosition());
    KillPedsInVehicle();
    m_nOverrideLights = NO_CAR_LIGHT_OVERRIDE;
    vehicleFlags.bCreateRoadBlockPeds = false;         // 0x428 &= 0xAF
    vehicleFlags.bOccupantsHaveBeenGenerated = false;  //
    m_bSmokeEjectorEnabled = false;
    vehicleFlags.bSirenOrAlarm = false;
    autoFlags.bTaxiLight = false;
    if (vehicleFlags.bIsAmbulanceOnDuty) {
        vehicleFlags.bIsAmbulanceOnDuty = false;
        --CCarCtrl::NumAmbulancesOnDuty;
    }

    if (vehicleFlags.bIsFireTruckOnDuty) {
        vehicleFlags.bIsFireTruckOnDuty = false;
        --CCarCtrl::NumFireTrucksOnDuty;
    }

    ChangeLawEnforcerState(false);
    gFireManager.StartFire(this, damager, 0.8f, 1, 7000, 0);
    CDarkel::RegisterCarBlownUpByPlayer(*this, 0);
    if (m_nModelIndex == MODEL_RCBARON) {
        CExplosion::AddExplosion(this, damager, EXPLOSION_RC_VEHICLE, GetPosition(), 0, 1, -1.0f, 0);
    } else {
        CExplosion::AddExplosion(this, damager, EXPLOSION_AIRCRAFT, GetPosition(), 0, 1, -1.0f, 0);
    }
}

// 0x6CABB0
void CPlane::Fix() {
    m_damageManager.ResetDamageStatus();
    if (m_pHandlingData->m_bNoDoors) {
        m_damageManager.SetDoorStatus(DOOR_LEFT_FRONT, DAMSTATE_NOTPRESENT);
        m_damageManager.SetDoorStatus(DOOR_RIGHT_FRONT, DAMSTATE_NOTPRESENT);
        m_damageManager.SetDoorStatus(DOOR_LEFT_REAR, DAMSTATE_NOTPRESENT);
        m_damageManager.SetDoorStatus(DOOR_RIGHT_REAR, DAMSTATE_NOTPRESENT);
    }
    SetupDamageAfterLoad();
}

// 0x6CACB0
void CPlane::OpenDoor(CPed* ped, int32 componentId, eDoors door, float doorOpenRatio, bool playSound) {
    CAutomobile::OpenDoor(ped, componentId, door, doorOpenRatio, playSound);

    if (m_nModelIndex != MODEL_STUNT)
        return;

    // Unfinished code R*, which removed in Android
    if (false) // byte_C1CAFC
    {
        CMatrix matrix(RwFrameGetMatrix(m_aCarNodes[componentId]), false);
        const auto y = m_doors[door].m_angle - m_doors[door].m_prevAngle + matrix.GetPosition().y;
        matrix.SetTranslate({matrix.GetPosition().x, y, matrix.GetPosition().z});
        matrix.UpdateRW();
    }
}

// 0x6CAC10
void CPlane::SetupDamageAfterLoad() {
    vehicleFlags.bIsDamaged = false;
}

// 0x6CC4B0
void CPlane::VehicleDamage(float damageIntensity, eVehicleCollisionComponent component, CEntity* damager, CVector* vecCollisionCoors, CVector* vecCollisionDirection, eWeaponType weapon) {
    // ABI: 5-й аргумент (vecCollisionDirection) в оригинале не используется - его стек-слот MSVC
    // переиспользовал под локальный bool CanVehicleBeDamaged (в декомпиле это param_6).
    float destroyThreshold = PLANE_DAMAGE_DESTROY_THRESHHOLD; // 5000.0f
    float damageThreshold  = PLANE_DAMAGE_THRESHHOLD;         // 500.0f
    bool  bWeaponDamage    = false;
    float collForceMult    = 0.333f;
    float minDmgIntensity  = PLANE_DAMAGE_THRESHHOLD;

    if (m_nModelIndex == MODEL_VORTEX) {
        CAutomobile::VehicleDamage(damageIntensity, component, damager, vecCollisionCoors, vecCollisionDirection, weapon);
        return;
    }

    if (!vehicleFlags.bCanBeDamaged)
        return;

    // урон разрешен: игрок / не неуязвим / последним бил игрок (0x400000 = bInvulnerable)
    if (GetStatus() != STATUS_PLAYER
        && physicalFlags.bInvulnerable
        && m_pDamageEntity != FindPlayerPed()
        && m_pDamageEntity != FindPlayerVehicle()) {
        return;
    }

    if (damageIntensity == 0.0f) {
        // коллизионный урон - взять данные последней коллизии
        damageIntensity   = m_fDamageIntensity;
        const float massScale = m_fMass / PLANE_DAMAGE_SCALE_MASS; // / 10000.0f
        collForceMult    = 1.0f;
        minDmgIntensity  = std::max(massScale, 1.0f) * damageThreshold;
        vecCollisionCoors = &m_vecLastCollisionPosn;
        destroyThreshold = massScale * PLANE_DAMAGE_DESTROY_THRESHHOLD;

        if (damageIntensity < 0.0f)
            return;
        if (physicalFlags.bCollisionProof)
            return;
        if (m_pDamageEntity && m_pDamageEntity->GetType() == ENTITY_TYPE_PED)
            return;
    } else {
        bool bDamagedDueToFireOrExplosionOrBullet;
        if (!CanVehicleBeDamaged(damager, weapon, bDamagedDueToFireOrExplosionOrBullet))
            return;
        // только взрывчатка
        if (weapon != WEAPON_ROCKET && weapon != WEAPON_ROCKET_HS && weapon != WEAPON_FREEFALL_BOMB && weapon != WEAPON_EXPLOSION)
            return;
        bWeaponDamage   = true;
        damageIntensity = 0.0f;
    }

    float dmg = damageIntensity * m_pHandlingData->m_fCollisionDamageMultiplier;
    const float prevHealth = m_fHealth;
    if (this == FindPlayerVehicle()) {
        dmg *= vehicleFlags.bTakeLessDamage ? (1.0f / 6.0f) : 0.5f; // 0x859F0A0 / 0x858B8C
    } else if (vehicleFlags.bTakeLessDamage) {
        dmg *= 1.0f / 12.0f;                                       // 0x858F0FC
    } else if (!damager || damager != FindPlayerVehicle()) {
        dmg *= 0.25f;                                              // 0x858C84
    } else {
        dmg *= 1.0f / 1.5f;                                        // 0x862D68
    }
    m_fHealth -= dmg;

    // не дать умереть в этом кадре - оставить 1 hp
    if (m_fHealth < 0.0f && prevHealth > 0.0f) {
        m_fHealth = 1.0f;
    }

    if (destroyThreshold < damageIntensity || (m_fHealth < 0.0f && prevHealth > 0.0f)) {
        BlowUpCar(nullptr, false);
    } else if (((damageThreshold < damageIntensity) || bWeaponDamage)
        && GetStatus() != STATUS_WRECKED
        && StaticRef<float>(0x858C4C) < m_fMass) { // неизвестная пороговая константа (масса самолета)

        // ближайший к точке удара компонент (пропеллеры/руль/элеваторы/элероны/шасси)
        CVector colLocal = *vecCollisionCoors - GetPosition();
        colLocal = GetMatrix().InverseTransformPoint(colLocal);

        float  minDist = 1000.0f;
        int32  nearestNode = -1;
        for (auto node = (int32)PLANE_STATIC_PROP; node < PLANE_NUM_NODES; node++) { // FIX(int-plane) [I-4]: int32 - у ePlaneNodes нет operator++ (C2676)
            if (auto frame = m_aCarNodes[node]) {
                const CVector d = RwFrameGetMatrix(frame)->pos - colLocal;
                const float distSq = d.SquaredMagnitude();
                if (distSq < minDist * minDist) {
                    minDist = sqrt(distSq);
                    nearestNode = node;
                }
            }
        }

        if (nearestNode != -1) {
            // при попадании в руль рандомно переносим урон на элеваторы
            if (nearestNode == PLANE_RUDDER && (rand() & 1) != 0) {
                if (!m_aCarNodes[PLANE_ELEVATOR_L] || (rand() & 1) == 0) {
                    if (m_aCarNodes[PLANE_ELEVATOR_R]) {
                        nearestNode = PLANE_ELEVATOR_R;
                    }
                } else {
                    nearestNode = PLANE_ELEVATOR_L;
                }
            }

            if (m_damageManager.ProgressAeroplaneDamage(nearestNode)) {
                if (m_damageManager.GetAeroplaneCompStatus(nearestNode) == DAMSTATE_OPENED
                    && nearestNode > PLANE_RUDDER - 1 && nearestNode < PLANE_MISC_A) { // [16, 22]
                    // оторванный компонент начинает болтаться - ищем свободную панель
                    // (4-й слот - m_swingingChassis, как в оригинале)
                    auto* panels = reinterpret_cast<CBouncingPanel*>(m_panels.data());
                    for (auto i = 0; i < 4; i++, panels++) {
                        if (panels->m_nFrameId == (uint16)-1) {
                            panels->SetPanel(nearestNode, AXIS_X, -0.02f); // 0xBCA3D70A
                            break;
                        }
                        if (panels->m_nFrameId == nearestNode)
                            break;
                    }
                } else if (m_damageManager.GetAeroplaneCompStatus(nearestNode) == DAMSTATE_DAMAGED) {
                    SetComponentVisibility(m_aCarNodes[nearestNode], 2); // скрыть
                }
            }

            dmgDrawCarCollidingParticles(*vecCollisionCoors, collForceMult * damageIntensity, weapon);

            // очаг огня на поврежденном компоненте (частица живет в m_pSmokeParticle/m_nSmokeTimer,
            // таймер тикает в ProcessControl)
            const CVector compPos = RwFrameGetMatrix(m_aCarNodes[nearestNode])->pos;

            if (m_pSmokeParticle) {
                m_pSmokeParticle->Kill();
                m_pSmokeParticle = nullptr;
            }
            if (m_pSmokeParticle = g_fxMan.CreateFxSystem("fire_med", compPos, GetModellingMatrix(), false)) {
                m_pSmokeParticle->Play();
                m_pSmokeParticle->SetVelAdd(-m_vecMoveSpeed * 5.0f);
                m_pSmokeParticle->SetLocalParticles(true);
                m_nSmokeTimer = CGeneral::GetRandomNumberInRange(2000, 4000);
                return;
            }
        }
    }
}

// 0x6CAB90
void CPlane::IsAlreadyFlying() {
    m_nStartedFlyingTime = CTimer::GetTimeInMS() - 20000;
}

// 0x6CAC20
void CPlane::SetGearUp() {
    m_LandingGearAngle = 1.0f;
    m_fAirResistance = m_pHandlingData->m_fDragMult / 1000.0f / 2.0f * m_pFlyingHandlingData->m_fGearUpR;
    m_damageManager.SetWheelStatus(CAR_WHEEL_FRONT_LEFT,  WHEEL_STATUS_MISSING);
    m_damageManager.SetWheelStatus(CAR_WHEEL_REAR_LEFT,   WHEEL_STATUS_MISSING);
    m_damageManager.SetWheelStatus(CAR_WHEEL_FRONT_RIGHT, WHEEL_STATUS_MISSING);
    m_damageManager.SetWheelStatus(CAR_WHEEL_REAR_RIGHT,  WHEEL_STATUS_MISSING);
}

// 0x6CAC70
void CPlane::SetGearDown() {
    m_LandingGearAngle = 0.0f;
    m_fAirResistance = m_pHandlingData->m_fDragMult / 1000.0f / 2.0f;
    m_damageManager.SetWheelStatus(CAR_WHEEL_FRONT_LEFT,  WHEEL_STATUS_OK);
    m_damageManager.SetWheelStatus(CAR_WHEEL_REAR_LEFT,   WHEEL_STATUS_OK);
    m_damageManager.SetWheelStatus(CAR_WHEEL_FRONT_RIGHT, WHEEL_STATUS_OK);
    m_damageManager.SetWheelStatus(CAR_WHEEL_REAR_RIGHT,  WHEEL_STATUS_OK);
}

// 0x6CCA50
uint32 CPlane::CountPlanesAndHelis() {
    uint32 counter = 0;
    for (auto& vehicle : GetVehiclePool()->GetAllValid()) {
        if (vehicle.IsSubHeli() || vehicle.IsSubPlane()) {
            counter++;
        }
    }
    return counter;
}

// 0x6CCAA0
bool CPlane::AreWeInNoPlaneZone() {
    const auto& camPos = TheCamera.GetPosition();
    constexpr CVector vec1 = { -1073.0f, -675.0f, 50.0f };

    return DistanceBetweenPoints(vec1, camPos) < 200.0f ||
           camPos.x > -2743.0f && camPos.x < -2626.0f && camPos.y > 1300.0f && camPos.y < 2200.0f || // todo: Is point inside
           camPos.x > -1668.0f && camPos.x < -1122.0f && camPos.y > 541.0f && camPos.y < 1118.0f;
}

// 0x6CCBB0
bool CPlane::AreWeInNoBigPlaneZone() {
    // untested
    const auto& camPos = TheCamera.GetPosition();
    return DistanceBetweenPoints2D({ +1522.0f, -1237.0f }, camPos) < 800.0f ||
           DistanceBetweenPoints2D({ -1836.0f, +659.0f }, camPos) < 800.0f;
}

// 0x6CCC50
void CPlane::SwitchAmbientPlanes(bool enable) {
    if (!GenPlane_Active) {
        GenPlane_Active = enable;
        return;
    }

    // Planes are being switched off - remove all ambient helis/planes
    if (!enable) {
        for (auto& veh : GetVehiclePool()->GetAllValid()) {
            // CVehicle +0x4A4 = m_nCreatedBy; RANDOM_VEHICLE (1) - "ambient" самолёт/вертолёт
            if ((veh.m_nVehicleSubType == VEHICLE_TYPE_HELI || veh.m_nVehicleSubType == VEHICLE_TYPE_PLANE)
                && veh.m_nCreatedBy == RANDOM_VEHICLE) {
                CWorld::Remove(&veh);
                delete &veh;
            }
        }
    }

    GenPlane_Active = enable;
}

// 0x6CD090
void CPlane::FindPlaneCreationCoors(CVector* outCoors, CVector* outTargetCoors, float* outPlaneOrientation, float* outFlightHeight, bool isBigPlane) {
    static auto& WIND_OFFSET_X = StaticRef<float>(0xB6F9AC); // TheCamera + 0x984 (see CWeather)
    static auto& WIND_OFFSET_Y = StaticRef<float>(0xB6F9B0); // TheCamera + 0x988
    static auto& WIND_OFFSET_Z = StaticRef<float>(0xB6F9B4);
    static auto& s_PlaneCreationCheckCounter = StaticRef<int32>(0xB7CD78);

    // 0x40FD10 - unreversed: builds a CColSphere{ radius, center, a, b, c } (probably in a static used by the checks below)
    const auto BuildRadiusSphere = [](float radius, CVector* center) {
        plugin::Call<0x40FD10, float, CVector*, uint8, uint8, uint8>(radius, center, 0, 0, 0xFF);
    };
    // 0x41A820 - unreversed: cdecl(int xSector, int ySector, CVector* a, CVector* b, CVector* c, CVector* d);
    // scans the sector entity table @0xB7D0B8 (120x120, stride 8) and returns true if the sector is "occupied".
    const auto IsSectorOccupied = [](int xSector, int ySector, CVector* a, CVector* b, CVector* c, CVector* d) -> bool {
        return plugin::CallAndReturn<bool, 0x41A820, int, int, CVector*, CVector*, CVector*, CVector*>(xSector, ySector, a, b, c, d);
    };

    const float creationDist = isBigPlane ? 340.0f : 140.0f;
    const float baseHeight   = isBigPlane ? 200.0f : 25.0f;

    for (int i = 0; i < 500; i += 10) {
        const float angle = (float)(rand() % 360) * DEG_TO_RAD; // float @ 0x8595EC

        const CVector playerCoors = FindPlayerCoors();
        *outCoors = playerCoors;

        *outFlightHeight = (float)(rand() & 0xF) + (float)i + baseHeight;

        outCoors->x += std::cos(angle) * creationDist;
        outCoors->y += std::sin(angle) * creationDist;
        outCoors->z += *outFlightHeight;

        CVector target = FindPlayerCoors();

        const float windDist = (float)(rand() & 0x1F) + 20.0f;
        target.x += WIND_OFFSET_X * windDist;
        target.y += WIND_OFFSET_Y * windDist;
        target.z += WIND_OFFSET_Z * windDist;
        target.z += *outFlightHeight;
        *outTargetCoors = target;

        *outPlaneOrientation = CGeneral::GetATanOfXY(target.x - outCoors->x, target.y - outCoors->y);

        CVector pointAhead{
            (target.x - outCoors->x) + target.x,
            (target.y - outCoors->y) + target.y,
            (target.z - outCoors->z) + target.z
        };

        if (s_PlaneCreationCheckCounter == -1) {
            // FIX(int-plane) [I-5]: CWorld::ClearScanCodes в репо protected (World.h:253);
            // вызов по адресу - реверснутая реализация установлена на 0x563470 (World.cpp:105)
            plugin::Call<0x563470>();
            s_PlaneCreationCheckCounter = 1;
        } else {
            s_PlaneCreationCheckCounter++;
        }

        BuildRadiusSphere(15.0f, outCoors);

        if (CWorld::GetIsLineOfSightClear(*outCoors, pointAhead, true, false, false, false, false, false, false)) {
            if (!IsSectorOccupied((int)target.x, (int)target.y, &pointAhead, &pointAhead, &pointAhead, &pointAhead)) {
                break;
            }
        }
    }

    *outFlightHeight  += 20.0f;
    outTargetCoors->z += 20.0f;
}

// 0x6CD2F0
void CPlane::DoPlaneGenerationAndRemoval() {
    // 0x6C4130 - нереверснутый сеттер heading для заспавненного транспорта (вызов прямо по адресу).
    if ((CTimer::GetFrameCounter() & 0x1F) != 0x1E)
        return;
    if (!GenPlane_Active)
        return;

    const auto numPlanesHelis = CountPlanesAndHelis();

    if (GenPlane_Status != 0) {
        if (GenPlane_Status != 1) // уже занят спавном (запрос модели в полёте)
            return;

        if (!CStreaming::IsModelLoaded(GenPlane_ModelIndex)) {
            CStreaming::RequestModel(GenPlane_ModelIndex, 8);
            return;
        }

        CVector creationCoors, targetCoors;
        float   heading, flightHeight;

        if (GenPlane_ModelIndex == MODEL_POLMAV) {
            // полицейский вертолёт
            FindPlaneCreationCoors(&creationCoors, &targetCoors, &heading, &flightHeight, false);
            auto* heli = new CHeli(GenPlane_ModelIndex, RANDOM_VEHICLE);
            heli->SetPosn(creationCoors);
            plugin::CallMethod<0x6C4130, CVehicle*, float>(heli, heading); // нереверснутый сеттер heading
            CCarCtrl::JoinCarWithRoadSystem(heli);
            heli->m_fHeliRotorSpeed = 0.22f; // FIX(int-plane) [S-1]: было 0.14f (0x3E14E978); бинарник: 0x3E6147AE = 0.22f
            heli->SetStatus(STATUS_PHYSICS);
            CWorld::Add(heli);
            heli->m_fMinAltitude = flightHeight;
            heli->m_autoPilot.m_nCarMission = MISSION_HELI_FLYINDIRECTION;
            heli->field_9B4 = heading; // heading-related поле CHeli
            heli->m_fMaxAltitude = flightHeight;
            heli->SetVelocity(CVector{ std::cos(heading) * StaticRef<float>(0x862D68),
                                       std::sin(heading) * StaticRef<float>(0x862D68),
                                       0.0f });
            heli->vehicleFlags.bNeverUseSmallerRemovalRange = true;
            heli->m_nExtendedRemovalRange = 255;
            heli->m_autoPilot.m_nCruiseSpeed = 50;
            heli->SetEngineOn(true);

            // цель для преследования - случайная незнающая машина или пед (50/50)
            if ((rand() & 1) == 0) {
                if (auto target = CWorld::FindUnsuspectingTargetCar(heli->GetPosition(), FindPlayerCoors())) {
                    heli->m_autoPilot.m_nCarMission = MISSION_HELI_FOLLOW_ENTITY;
                    // FIX(int-plane) [S-4]: регистрация ссылки через идиому AutoPilot.cpp:82
                    // (CEntity::RegisterReference - нестатический метод, статик-шаблон не матчится с CEntity**)
                    CEntity::ChangeEntityReference(heli->m_autoPilot.m_TargetEntity, target);
                    heli->m_nHeliFlags |= 2;
                    // FIX(int-plane) [S-2]: маска 0xA0 = bHeliFollowTarget (0x80) | bDoTargetCatchupCheck (0x20);
                    // bCantGoAgainstTraffic (0x40) тут не участвует (AutoPilot.h:91-93)
                    heli->m_autoPilot.carCtrlFlags.bDoTargetCatchupCheck = true;
                    heli->m_autoPilot.carCtrlFlags.bHeliFollowTarget = true;
                }
            } else {
                if (auto targetPed = CWorld::FindUnsuspectingTargetPed(heli->GetPosition(), FindPlayerCoors())) {
                    heli->m_autoPilot.m_nCarMission = MISSION_HELI_FOLLOW_ENTITY;
                    // FIX(int-plane) [S-4]: m_TargetEntity имеет тип CVehicle*, CPed* - сиблинг,
                    // поэтому идём через алиас CEntity* (сама ссылка регистрируется в тот же слот)
                    CEntity* pedEntity = targetPed;
                    CEntity::ChangeEntityReference(reinterpret_cast<CEntity*&>(heli->m_autoPilot.m_TargetEntity), pedEntity);
                    heli->m_nHeliFlags |= 2;
                    heli->m_autoPilot.carCtrlFlags.bDoTargetCatchupCheck = true; // FIX(int-plane) [S-2]: см. выше
                    heli->m_autoPilot.carCtrlFlags.bHeliFollowTarget = true;

                    CPedGroup* group = CPedGroups::GetPedsGroup(targetPed);
                    if (!group || group->m_bIsMissionGroup) {
                        // без группы - событие напрямую педу
                        if (targetPed->IsPlayer()) {
                            GenPlane_Status = 0;
                            GenPlane_LastTimeGenerated = CTimer::GetTimeInMS();
                            return;
                        }
                        // FIX(int-plane) [S-5]+[I-6]: CPed+0x484 = m_nCreatedBy (Ped.h:269); поле protected -
                        // используем публичный геттер GetCreatedBy() (Ped.h:545); 2 = PED_MISSION (Ped.h:83)
                        if (targetPed->GetCreatedBy() == PED_MISSION) {
                            GenPlane_Status = 0;
                            GenPlane_LastTimeGenerated = CTimer::GetTimeInMS();
                            return;
                        }
                        if (targetPed->m_nPedType == PED_TYPE_COP) {
                            GenPlane_Status = 0;
                            GenPlane_LastTimeGenerated = CTimer::GetTimeInMS();
                            return;
                        }
                        CEventDanger dangerEvent(heli, 200.0f);
                        targetPed->GetEventGroup().Add(&dangerEvent, false);
                    } else {
                        CEventDanger dangerEvent(heli, 200.0f);
                        group->m_groupIntelligence.AddEvent(&dangerEvent);
                    }
                }
            }
        } else {
            // обычный самолёт
            const bool isBigPlane = GenPlane_ModelIndex == MODEL_AT400 || GenPlane_ModelIndex == MODEL_ANDROM;
            FindPlaneCreationCoors(&creationCoors, &targetCoors, &heading, &flightHeight, isBigPlane);
            auto* plane = new CPlane(GenPlane_ModelIndex, RANDOM_VEHICLE);
            plane->SetPosn(creationCoors);
            plane->m_planeCreationHeading = heading;
            plugin::CallMethod<0x6C4130, CVehicle*, float>(plane, heading); // нереверснутый сеттер heading
            CCarCtrl::JoinCarWithRoadSystem(plane);
            plane->m_planeHeading      = heading;
            plane->m_minAltitude       = flightHeight;
            plane->m_autoPilot.m_nCarMission = MISSION_PLANE_FLYINDIRECTION;
            plane->m_planeHeadingPrev  = heading;
            plane->m_maxAltitude       = flightHeight;
            plane->SetVelocity(CVector{ std::cos(heading) * StaticRef<float>(0x862D68),
                                        std::sin(heading) * StaticRef<float>(0x862D68),
                                        0.0f });
            plane->vehicleFlags.bNeverUseSmallerRemovalRange = true;
            if (!isBigPlane && GenPlane_ModelIndex != MODEL_HYDRA) {
                plane->m_nExtendedRemovalRange = 150;
            } else {
                plane->m_nExtendedRemovalRange = 360;
                if (isBigPlane) {
                    plane->m_nFlags &= 0xFFFFFFFE; // bUsesCollision = false
                }
            }
            if (GenPlane_ModelIndex == MODEL_HYDRA) {
                plane->m_autoPilot.m_nCarMission = MISSION_PLANE_ATTACK_PLAYER_POLICE;
                plane->m_wMiscComponentAngle = 0;
            }
            plane->SetGearUp();
            plane->SetEngineOn(true);
            CStreaming::SetModelIsDeletable(GenPlane_ModelIndex);
            CStreaming::SetModelTxdIsDeletable(GenPlane_ModelIndex);
        }

        GenPlane_LastTimeGenerated = CTimer::GetTimeInMS();
        GenPlane_Status = 0;
        return;
    }

    // === Статус 0: решаем, что спавнить ===
    if (CGame::currArea != 0 || 4 < numPlanesHelis || AreWeInNoPlaneZone() || CCutsceneMgr::ms_cutsceneProcessing) {
        GenPlane_LastTimeGenerated = CTimer::GetTimeInMS();
        return;
    }

    // полицейский вертолёт (с лимитом 1-2 шт.)
    const auto SpawnPoliceHeli = [&] {
        GenPlane_ModelIndex = MODEL_POLMAV;
        GenPlane_Status = 1;
    };
    // FIX(int-plane) [S-3]: полярность гейта восстановлена по декомпилу 0x6CD2F0:
    // skip if (wanted<4 && veh && sub!=HELI && sub!=PLANE) || model==AT400 || model==ANDROM || time<=lastGen+60000,
    // т.е. спавним при (wanted>=4 || !veh || sub==HELI || sub==PLANE) && model!=AT400 && model!=ANDROM && time>lastGen+60000
    const auto playerVeh = FindPlayerVehicle();
    bool bSpawnPoliceHeli = true;
    if ((uint32)FindPlayerPed()->AsPlayer()->GetWantedLevel() < 4 && playerVeh
        && playerVeh->m_nVehicleSubType != VEHICLE_TYPE_PLANE && playerVeh->m_nVehicleSubType != VEHICLE_TYPE_HELI) {
        bSpawnPoliceHeli = false;
    }
    if (playerVeh && (playerVeh->m_nModelIndex == MODEL_AT400 || playerVeh->m_nModelIndex == MODEL_ANDROM)) {
        bSpawnPoliceHeli = false;
    }
    if (GenPlane_LastTimeGenerated + 60'000 >= CTimer::GetTimeInMS()) {
        bSpawnPoliceHeli = false;
    }
    if (bSpawnPoliceHeli) {
        int32 numPoliceHelis = 0;
        for (size_t i = 0; i < GetVehiclePool()->GetSize(); i++) { // FIX(int-plane) [I-7]: size_t против C4018
            if (auto veh2 = GetVehiclePool()->GetAt(i)) {
                if (veh2->m_autoPilot.m_nCarMission == MISSION_PLANE_ATTACK_PLAYER_POLICE)
                    numPoliceHelis++;
            }
        }
        if (numPoliceHelis < 1) {
            SpawnPoliceHeli();
            return;
        }
        if (numPoliceHelis < 2 && 4 < (uint32)FindPlayerPed()->AsPlayer()->GetWantedLevel()) {
            SpawnPoliceHeli();
            return;
        }
    }

    if (0 < numPlanesHelis)
        return;
    if (GenPlane_Status != 0)
        return;

    // обычные маленькие самолёты: город/пустыня, без песчаной бури
    const auto SpawnRandomSmallPlane = [&] {
        if (GenPlane_LastTimeGenerated + 120'000 < CTimer::GetTimeInMS()) {
            switch (rand() & 7) {
            case 0: case 1: case 2: case 7:
                GenPlane_ModelIndex = MODEL_RUSTLER;
                break;
            case 3: case 4:
                GenPlane_ModelIndex = MODEL_CROPDUST;
                break;
            case 5: case 6:
                GenPlane_ModelIndex = MODEL_BEAGLE;
                break;
            }
            GenPlane_Status = 1;
        }
    };
    if ((CWeather::WeatherRegion == WEATHER_REGION_DEFAULT || CWeather::WeatherRegion == WEATHER_REGION_DESERT)
        && CWeather::NewWeatherType != WEATHER_SANDSTORM_DESERT
        && CWeather::OldWeatherType != WEATHER_SANDSTORM_DESERT) {
        const auto veh = FindPlayerVehicle();
        if (!veh) {
            SpawnRandomSmallPlane();
        } else {
            const auto appearance = veh->GetVehicleAppearance();
            if (appearance != VEHICLE_APPEARANCE_HELI && appearance != VEHICLE_APPEARANCE_PLANE)
                SpawnRandomSmallPlane();
        }
        if (GenPlane_Status != 0)
            return;
    }

    // в ганг-зоне без миссии - полицейский вертолёт раз в ~350с
    if (CPopCycle::m_bCurrentZoneIsGangArea) {
        if (!CTheScripts::IsPlayerOnAMission() && GenPlane_LastTimeGenerated + 350'000 < CTimer::GetTimeInMS()) {
            SpawnPoliceHeli();
            return;
        }
        if (GenPlane_Status != 0)
            return;
    }

    // при бунте - полицейский вертолёт
    if (CGameLogic::LaRiotsActiveHere()) {
        SpawnPoliceHeli();
        return;
    }

    // большие самолёты: в городах (не LA-DEFAULT/DESERT), не в ганг-зоне, без бури, раз в ~200с
    if (GenPlane_Status == 0
        && CWeather::WeatherRegion != WEATHER_REGION_DEFAULT && CWeather::WeatherRegion != WEATHER_REGION_DESERT
        && !CPopCycle::m_bCurrentZoneIsGangArea
        && CWeather::NewWeatherType != WEATHER_SANDSTORM_DESERT
        && CWeather::OldWeatherType != WEATHER_SANDSTORM_DESERT
        && GenPlane_LastTimeGenerated + 200'000 < CTimer::GetTimeInMS()
        && !AreWeInNoBigPlaneZone()) {
        const auto SpawnBigPlane = [&] {
            GenPlane_ModelIndex = (rand() & 1) == 0 ? MODEL_AT400 : MODEL_ANDROM;
            GenPlane_Status = 1;
        };
        const auto veh = FindPlayerVehicle();
        if (!veh) {
            SpawnBigPlane();
            return;
        }
        const auto appearance = veh->GetVehicleAppearance();
        if (appearance != VEHICLE_APPEARANCE_HELI && appearance != VEHICLE_APPEARANCE_PLANE) {
            SpawnBigPlane();
            return;
        }
    }
}

// 0x6C9140
bool CPlane::SetUpWheelColModel(CColModel* wheelCol) {
    return false;
}

// 0x6C9150
bool CPlane::BurstTyre(uint8 tyreComponentId, bool bPhysicalEffect) {
    return false;
}

// 0x6C94A0
void CPlane::PreRender() {
    auto& mi = *GetVehicleModelInfo();
    CQuaternion quat; // переиспользуется блоками поворота рулей
    const auto colModel = GetColModel();
    CVector origPos;

    CVehicle::PreRender();

    if (m_nModelIndex == MODEL_VORTEX) {
        DoHoverSuspensionRatios(); // vtable+0x114
    }

    // === Колёса: сжатие подвески и положение ===
    if (vehicleFlags.bVehicleColProcessed) {
        DoBurstAndSoftGroundRatios(); // vtable+0xD0

        for (auto i = 0; i < 4; i++) {
            const float wheelRadius = 1.0f - m_aSuspensionSpringLength[i] / m_aSuspensionLineLength[i];
            const float compr = m_fWheelsSuspensionCompression[i] = (m_fWheelsSuspensionCompression[i] - wheelRadius) / (1.0f - wheelRadius);

            CVector wheelPos;
            mi.GetWheelPosn(i, wheelPos, true);
            float z = wheelPos.z + m_pHandlingData->m_fSuspensionUpperLimit;
            if (0.0f < compr) {
                z -= compr * m_aSuspensionSpringLength[i];
            }
            if (z <= m_wheelPosition[i]
                && (!physicalFlags.bAddMovingCollisionSpeed /* 0x4000000 */ || !(m_nHandlingFlagsIntValue & 0x20000))) {
                z = (z - m_wheelPosition[i]) * StaticRef<float>(0x858F34) + m_wheelPosition[i];
            }
            m_wheelPosition[i] = z;

            // вращение колёс Андомады
            if (m_nModelIndex == MODEL_ANDROM) {
                if (compr >= 1.0f) { // колесо в воздухе - затухание
                    m_wheelRotation[i] *= StaticRef<float>(0x858EF0);
                } else {
                    const float dot = DotProduct(GetForward(), m_wheelColPoint[i].m_vecNormal);
                    // FIX(int-plane) [S-6]: сайт 0x6C9618/0x6C962F/0x6C9640 - 0x821E70 = acos, ветви сняты
                    // с ассемблера: -acos(clamp(dot, -1, 1)); было std::pow(dot, 3.0f)
                    m_wheelRotation[i] = -std::acos(std::clamp(dot, -1.0f, 1.0f));
                }
            }
        }
    }

    // === Колёса/сопла: обновить матрицы ===
    UpdateWheelMatrix(PLANE_WHEEL_RB, 1); // 0x6AA290 (не реверснут)
    UpdateWheelMatrix(PLANE_WHEEL_LB, 1);
    UpdateWheelMatrix(PLANE_WHEEL_RF, 1);
    UpdateWheelMatrix(PLANE_WHEEL_LF, m_nModelIndex == MODEL_HYDRA ? 3 : 1); // у Хайдры это сопла

    // угол пропеллера (0x9C8)
    float propAngle = field_9C8 + CTimer::GetTimeStep() * m_fPropSpeed;
    field_9C8 = (float)(int)propAngle; // (int)-конверсия как в оригинале (FIX(int-plane) [I-8]: явный каст против C4244)
    if (TWO_PI < propAngle) {
        do {
            propAngle -= TWO_PI;
        } while (TWO_PI < propAngle);
        field_9C8 = (float)(int)propAngle;
    }

    // статичный пропеллер - непрозрачный, "смазанный" - невидимый
    const auto SetPropAtomicAlpha = [](RwFrame* frame, int32 alpha) {
        RwObject* atomic = nullptr;
        RwFrameForAllObjects(frame, GetCurrentAtomicObjectCB, &atomic);
        if (atomic) {
            SetComponentAtomicAlpha((RpAtomic*)atomic, alpha);
        }
    };
    SetComponentRotation(m_aCarNodes[PLANE_STATIC_PROP],  AXIS_Y, 2.0f * field_9C8, true);
    if (m_aCarNodes[PLANE_STATIC_PROP]) SetPropAtomicAlpha(m_aCarNodes[PLANE_STATIC_PROP], 0xFF);
    SetComponentRotation(m_aCarNodes[PLANE_MOVING_PROP],  AXIS_Y, -field_9C8, true);
    if (m_aCarNodes[PLANE_MOVING_PROP]) SetPropAtomicAlpha(m_aCarNodes[PLANE_MOVING_PROP], 0);
    SetComponentRotation(m_aCarNodes[PLANE_STATIC_PROP2], AXIS_Y, -2.0f * field_9C8, true);
    if (m_aCarNodes[PLANE_STATIC_PROP2]) SetPropAtomicAlpha(m_aCarNodes[PLANE_STATIC_PROP2], 0xFF);
    SetComponentRotation(m_aCarNodes[PLANE_MOVING_PROP2], AXIS_Y, field_9C8, true);
    if (m_aCarNodes[PLANE_MOVING_PROP2]) SetPropAtomicAlpha(m_aCarNodes[PLANE_MOVING_PROP2], 0);

    // === Шасси и створки (по моделям) ===
    const auto gearAngle = std::abs(m_LandingGearAngle);
    bool  hasGear = false;
    int32 gearAxis = 0, doorAxis = -1;
    float gearMultL = 0.0f, gearMultR = 0.0f, doorMult = 0.0f, doorMult2 = 0.0f;
    switch (m_nModelIndex) {
    case MODEL_RUSTLER:
        hasGear = true; gearAxis = 1;
        gearMultL = -StaticRef<float>(0x87192C); gearMultR = StaticRef<float>(0x87192C);
        break;
    case MODEL_SHAMAL:
    case MODEL_AT400:
        hasGear = true; gearAxis = 1;
        gearMultL = -StaticRef<float>(0x87192C); gearMultR = StaticRef<float>(0x87192C);
        doorAxis = 0; doorMult = StaticRef<float>(0x871938);
        break;
    case MODEL_HYDRA:
        hasGear = true; gearAxis = 0;
        gearMultL = gearMultR = StaticRef<float>(0x871930);
        doorAxis = 0; doorMult = StaticRef<float>(0x871934); doorMult2 = StaticRef<float>(0x871938);
        break;
    case MODEL_NEVADA:
        hasGear = true; gearAxis = 0;
        gearMultL = gearMultR = StaticRef<float>(0x871940);
        break;
    case MODEL_ANDROM:
        hasGear = true; gearAxis = 0;
        gearMultL = StaticRef<float>(0x87193C); gearMultR = StaticRef<float>(0x87193C);
        doorAxis = 0; doorMult = -StaticRef<float>(0x87193C);
        break;
    default:
        break;
    }
    if (hasGear) {
        SetComponentRotation(m_aCarNodes[PLANE_GEAR_L], (eRotationAxis)gearAxis, gearAngle * gearMultL, true);
        SetComponentRotation(m_aCarNodes[PLANE_GEAR_R], (eRotationAxis)gearAxis, gearAngle * gearMultR, true);
        if (doorAxis != -1) {
            SetComponentRotation(m_aCarNodes[PLANE_MISC_A], (eRotationAxis)doorAxis, gearAngle * doorMult, true);
            if (StaticRef<float>(0x859EF8) < doorMult2) {
                SetComponentRotation(m_aCarNodes[PLANE_MISC_B], (eRotationAxis)doorAxis, gearAngle * doorMult2, true);
            }
        }
    }

    if (m_nModelIndex == MODEL_ANDROM) {
        // задний трап Андомады
        SetComponentRotation(m_aCarNodes[PLANE_MISC_B], AXIS_X, (float)m_wMiscComponentAngle * ANDROM_COL_ANGLE_MULT, true);
    } else if (m_nModelIndex == MODEL_HYDRA) {
        const float nozzleAngle = (float)m_wMiscComponentAngle * StaticRef<float>(0x858FE4) / (float)(int)HARRIER_NOZZLE_ROTATE_LIMIT;
        SetComponentRotation(m_aCarNodes[PLANE_WHEEL_LM], AXIS_X, nozzleAngle, true);
        SetComponentRotation(m_aCarNodes[PLANE_WHEEL_RM], AXIS_X, nozzleAngle, true);

        if (!vehicleFlags.bEngineOn) {
            // движок заглушен - убрать струи и пыль
            for (auto& p : m_apJettrusParticles) {
                if (p) {
                    p->Kill();
                    p = nullptr;
                }
            }
            if (m_pDustParticle) {
                m_pDustParticle->Kill();
                m_pDustParticle = nullptr;
                m_heliDustFxTimeConst = 0.0f;
            }
        } else {
            const float thrustFX = (m_fAccelerationBreakStatus + 1.0f) * 0.5f;
            if ((int)m_wMiscComponentAngle < (int)HARRIER_NOZZLE_SWITCH_LIMIT) {
                // обычный полёт - пыль не нужна
                if (m_pDustParticle) {
                    m_pDustParticle->Kill();
                    m_pDustParticle = nullptr;
                    m_heliDustFxTimeConst = 0.0f;
                }
            } else {
                DoHeliDustEffect(thrustFX, 2.0f); // 0x40000000
            }

            // создать струи при необходимости (матрица корневого фрейма клампа: clump->object.parent->modelling)
            RwMatrix* clumpRootMatrix = nullptr;
            if (auto clump = GetRpClump()) {
                if (auto rootFrame = *reinterpret_cast<RwFrame**>(reinterpret_cast<char*>(clump) + 4)) {
                    clumpRootMatrix = RwFrameGetMatrix(rootFrame);
                }
            }
            for (auto& p : m_apJettrusParticles) {
                if (clumpRootMatrix && !p) {
                    const CVector zeroPos{ 0.0f, 0.0f, 0.0f };
                    if (p = g_fxMan.CreateFxSystem("jetthrust", zeroPos, clumpRootMatrix, false)) {
                        p->Play();
                        p->SetLocalParticles(true);
                        p->CopyParentMatrix();
                    }
                }
            }

            const auto UpdateThruster = [&](FxSystem_c* p, RwFrame* frame, const CVector& offset) {
                if (!p)
                    return;
                CMatrix mat = GetMatrix() * CMatrix{ *RwFrameGetMatrix(frame) }; // 0x59BE30 operator*
                p->SetMatrix(reinterpret_cast<RwMatrix*>(&mat));
                p->SetOffsetPos(offset);
                p->SetConstTime(true, thrustFX);
            };
            UpdateThruster(m_apJettrusParticles[0], m_aCarNodes[PLANE_WHEEL_LM], {  0.70f, -0.45f, 0.05f });
            UpdateThruster(m_apJettrusParticles[1], m_aCarNodes[PLANE_WHEEL_LM], { -0.82f, -0.45f, 0.05f });
            UpdateThruster(m_apJettrusParticles[2], m_aCarNodes[PLANE_WHEEL_RM], {  0.63f, -0.45f, 0.07f });
            UpdateThruster(m_apJettrusParticles[3], m_aCarNodes[PLANE_WHEEL_RM], { -0.75f, -0.45f, 0.07f });
        }
    }

    // === Динамика плавника Vortex (PLANE_MISC_A) ===
    if (m_nModelIndex == MODEL_VORTEX && m_aCarNodes[PLANE_MISC_A]) {
        CVector wheel0Pos;
        mi.GetWheelPosn(0, wheel0Pos, false);

        CMatrix mat{ RwFrameGetMatrix(m_aCarNodes[PLANE_MISC_A]), false };

        // тяга по направлению движения (зажим ±0x8D3414); в декомпиле далее не видна - вероятно, уходит в строки матрицы
        const float fwdSpeedDot   = DotProduct(GetForward(), m_vecMoveSpeed) * StaticRef<float>(0x8D3418);
        const float thrust        = std::clamp(fwdSpeedDot, -StaticRef<float>(0x8D3414), StaticRef<float>(0x8D3414));
        const float rightSpeedDot = DotProduct(GetRight(), m_vecMoveSpeed) * StaticRef<float>(0x8D3418);
        const float sideThrust    = std::clamp(rightSpeedDot, -StaticRef<float>(0x8D3414), StaticRef<float>(0x8D3414));
        (void)thrust; (void)sideThrust;

        // Аккумуляторы: в декомпиле неинициализированные слоты; смещения 0x948/0x94C/0x950 (CAutomobile).
        // 0x950 = m_fForcedOrientation (верифицировано); имена 0x948/0x94C в хедере на 4 байта
        // смещены относительно бинарника - оставлено сырыми оффсетами.
        float& finAccA = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(this) + 0x948);
        float& finAccB = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(this) + 0x94C);
        float& finAccC = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(this) + 0x950);
        // FIX(int-plane) [S-7]: сайт 0x6CA4CE - база = float @ 0x8D3410 = 0.9f (был литерал 0.99f)
        const float k = std::pow(StaticRef<float>(0x8D3410) /* 0.9f */, CTimer::GetTimeStep());

        // крен: разница положений колёс / колея (коллизионные линии/диски)
        float rollRate = ((m_wheelPosition[0] - m_wheelPosition[1]) + (m_wheelPosition[2] - m_wheelPosition[3]))
            * 0.5f
            / (*reinterpret_cast<const float*>(colModel->m_pColData->m_pLines) - *reinterpret_cast<const float*>(reinterpret_cast<char*>(colModel->m_pColData->m_pLines) + 0x24));
        rollRate = std::clamp(rollRate, -StaticRef<float>(0x8D340C), StaticRef<float>(0x8D340C));
        finAccA = (1.0f - k) * rollRate + finAccA * k;

        // тангаж: база колёс
        float pitchRate = ((m_wheelPosition[2] - m_wheelPosition[0]) + (m_wheelPosition[3] - m_wheelPosition[1]))
            * 0.5f
            / (*reinterpret_cast<const float*>(reinterpret_cast<char*>(colModel->m_pColData->m_pLines) + 0x60) - *reinterpret_cast<const float*>(reinterpret_cast<char*>(colModel->m_pColData->m_pLines) + 0x20));
        pitchRate = std::clamp(pitchRate, -StaticRef<float>(0x8D3408), StaticRef<float>(0x8D3408));
        m_fDoomHorizontalRotation = (1.0f - k) * pitchRate + m_fDoomHorizontalRotation * k;

        // вертикаль: среднее положение колёс против позиции колеса 0, делённое на половину CVehicleModelInfo+0x40
        float vertRate = 1.0f
            - ((m_wheelPosition[3] + m_wheelPosition[1] + m_wheelPosition[2] + m_wheelPosition[0]) * 0.25f - wheel0Pos.z)
              / (*reinterpret_cast<const float*>(reinterpret_cast<uintptr_t>(&mi) + 0x40) * 0.5f);
        vertRate = std::min(vertRate, StaticRef<float>(0x8D3404));
        m_fForcedOrientation = (1.0f - k) * vertRate + m_fForcedOrientation * k;

        mat.UpdateRW();
    }

    // === Рули: руль направления / элероны / элеваторы (только "активные" статусы) ===
    if (GetStatus() <= STATUS_PHYSICS) {
        const auto& dummies = mi.GetVehicleStruct()->m_avDummyPos;

        // руль направления
        if (m_aCarNodes[PLANE_RUDDER]) {
            CMatrix mat{ RwFrameGetMatrix(m_aCarNodes[PLANE_RUDDER]), false };
            CVector axis = dummies[DUMMY_EXHAUST_SECONDARY];
            if (axis == CVector{}) {
                axis.z = 1.0f;
            } else {
                axis.Normalise(); // NOTE: перед Normalise в декомпиле вычитание неинициализированного вектора - артефакт
            }
            quat.Set(reinterpret_cast<RwV3d*>(&axis), StaticRef<float>(0x871914) * DEG_TO_RAD * m_fLeftRightSkid);
            quat.Get(reinterpret_cast<RwMatrix*>(&mat));
            mat.UpdateRW();

            // у Vortex тот же поворот у второго плавника
            if (m_nModelIndex == MODEL_VORTEX && m_aCarNodes[PLANE_MISC_B]) {
                CMatrix mat2{ RwFrameGetMatrix(m_aCarNodes[PLANE_MISC_B]), false };
                quat.Get(reinterpret_cast<RwMatrix*>(&mat2));
                mat2.UpdateRW();
            }
        }

        // элероны (правый, затем зеркально левый)
        if (m_aCarNodes[PLANE_AILERON_R]) {
            // при фиксированном шасси - дополнительный "воздушный тормоз"
            float airbrakeScale = 1.0f, airbrakeRot = 0.0f;
            if (1.0f < m_pFlyingHandlingData->m_fGearDownL
                && std::abs(m_fSteeringLeftRight) < 1.0f
                && (m_aCarNodes[PLANE_GEAR_L] || m_aCarNodes[PLANE_GEAR_R])) {
                airbrakeScale = (1.0f - std::abs(m_fSteeringLeftRight)) * StaticRef<float>(0x871920) + 1.0f;
                airbrakeRot   = (1.0f - std::abs(m_fSteeringLeftRight)) * StaticRef<float>(0x871924);
            }

            CMatrix mat{ RwFrameGetMatrix(m_aCarNodes[PLANE_AILERON_R]), false };
            CVector axis = dummies[DUMMY_TRAILER_ATTACH];
            if (axis == CVector{}) {
                axis.z = 1.0f;
            } else {
                axis.Normalise();
            }
            quat.Set(reinterpret_cast<RwV3d*>(&axis), StaticRef<float>(0x87191C) * DEG_TO_RAD * m_fSteeringLeftRight + airbrakeRot);
            quat.Get(reinterpret_cast<RwMatrix*>(&mat));
            if (1.0f < airbrakeScale) {
                mat.Update();
                if (axis.x == 1.0f) {
                    // поворот вокруг X - растянуть строку Y матрицы (как в декомпиле)
                    // FIX(int-plane) [I-9]: m_forward private (Core/Matrix.h) - публичный реф-аксессор GetForward()
                    mat.GetForward() *= airbrakeScale;
                } else if (mi.GetOriginalCompPosition(origPos, PLANE_AILERON_R)) { // FIX(int-plane) [I-2]: порядок аргументов (CVector&, int32) - VehicleModelInfo.h:292
                    // сместить по Y пропорционально торможению
                    origPos.y = (1.0f - airbrakeScale) * colModel->m_boundBox.m_vecMax.y * StaticRef<float>(0x871928) + origPos.y;
                    mat.SetTranslateOnly(origPos);
                }
            }
            mat.UpdateRW();

            // левый элерон - зеркальная ось и знак
            if (m_aCarNodes[PLANE_AILERON_L]) {
                CMatrix matL{ RwFrameGetMatrix(m_aCarNodes[PLANE_AILERON_L]), false };
                axis.x = -axis.x;
                quat.Set(reinterpret_cast<RwV3d*>(&axis), StaticRef<float>(0x87191C) * DEG_TO_RAD * m_fSteeringLeftRight - airbrakeRot);
                quat.Get(reinterpret_cast<RwMatrix*>(&matL));
                if (1.0f < airbrakeScale) {
                    matL.Update();
                    if (axis.x == 1.0f) {
                        matL.GetForward() *= airbrakeScale; // FIX(int-plane) [I-9]: см. выше
                    } else if (mi.GetOriginalCompPosition(origPos, PLANE_AILERON_L)) { // FIX(int-plane) [I-2]: см. выше
                        origPos.y = (1.0f - airbrakeScale) * colModel->m_boundBox.m_vecMax.y * StaticRef<float>(0x871928) + origPos.y;
                        matL.SetTranslateOnly(origPos);
                    }
                }
                matL.UpdateRW();
            }
        }

        // элеваторы
        if (m_aCarNodes[PLANE_ELEVATOR_R]) {
            CMatrix mat{ RwFrameGetMatrix(m_aCarNodes[PLANE_ELEVATOR_R]), false };
            CVector axis = dummies[DUMMY_HAND_REST];
            if (axis == CVector{}) {
                axis.z = 1.0f;
            } else {
                axis.Normalise();
            }
            quat.Set(reinterpret_cast<RwV3d*>(&axis), -(StaticRef<float>(0x871918) * DEG_TO_RAD * m_fSteeringUpDown));
            quat.Get(reinterpret_cast<RwMatrix*>(&mat));
            mat.UpdateRW();

            if (m_aCarNodes[PLANE_ELEVATOR_L]) {
                CMatrix matL{ RwFrameGetMatrix(m_aCarNodes[PLANE_ELEVATOR_L]), false };
                axis.x = -axis.x;
                quat.Set(reinterpret_cast<RwV3d*>(&axis), StaticRef<float>(0x871918) * DEG_TO_RAD * m_fSteeringUpDown);
                quat.Get(reinterpret_cast<RwMatrix*>(&matL));
                matL.UpdateRW();
            }
        }
    }

    // === Следы-полосы от законцовок крыла ===
    if (!vehicleFlags.bIsDrowning && m_nModelIndex != MODEL_VORTEX && m_nModelIndex != MODEL_RCBARON) {
        // FIX(int-plane) [S-8]: сайт 0x6CA7AF - alpha снята с ассемблера:
        // (int)max(0, |moveSpeed . up| * m_fGearUpR (+0x2C) * 6400.0 (0x859AEC) - 32.0 (0x85950C)); было 255
        const auto streakAlpha = (uint8)std::max(
            0.0f,
            std::fabs(DotProduct(m_vecMoveSpeed, GetUpVector())) * m_pFlyingHandlingData->m_fGearUpR * StaticRef<float>(0x859AEC) /* 6400.0f */
                - StaticRef<float>(0x85950C) /* 32.0f */
        );
        const auto& wingDummy = mi.GetVehicleStruct()->m_avDummyPos[DUMMY_WING_AIR_TRAIL];
        const CVector localLeft{ wingDummy.x, wingDummy.y, wingDummy.z };
        const CVector localRight{ wingDummy.x - StaticRef<float>(0x858B1C), wingDummy.y, wingDummy.z };
        const auto& vehMat = GetMatrix();
        CMotionBlurStreaks::RegisterStreak(
            (uint32)this, 0xFF, 0xFF, 0xFF, streakAlpha,
            vehMat.TransformPoint(localLeft),
            vehMat.TransformPoint(localRight)
        );
        CMotionBlurStreaks::RegisterStreak(
            (uint32)this + 1, 0xFF, 0xFF, 0xFF, streakAlpha,
            vehMat.TransformPoint(-localLeft),
            vehMat.TransformPoint(-localRight)
        );
    }

    // === Дым опылителя / трюкового самолёта ===
    if (m_bSmokeEjectorEnabled) {
        if (m_nModelIndex == MODEL_CROPDUST) {
            auto particleData = FxPrtMult_c(1.0f, 1.0f, 1.0f, 0.2f, 1.0f, 1.0f, 0.3f);
            const CVector pos = GetMatrix().TransformPoint(CVector{ 0.0f, -0.5f, -0.5f });
            g_fx.m_SmokeHuge->AddParticle(
                pos,
                CVector{ m_vecMoveSpeed.x * 10.0f, m_vecMoveSpeed.y * 10.0f, m_vecMoveSpeed.z * 10.0f - StaticRef<float>(0x858B3C) },
                0.0f, particleData
            );
        } else if (m_nModelIndex == MODEL_STUNT) {
            auto particleData = FxPrtMult_c(1.0f, 0.0f, 0.0f, 0.2f, 1.0f, 1.0f, 0.3f);
            const CVector pos = GetMatrix().TransformPoint(CVector{ 0.0f, -5.0f, 0.0f });
            g_fx.m_SmokeHuge->AddParticle(
                pos,
                CVector{ m_vecMoveSpeed.x * 10.0f, m_vecMoveSpeed.y * 10.0f, m_vecMoveSpeed.z * 10.0f },
                0.0f, particleData
            );
        }
    }

    // Skimmer в воде - брызги
    if (m_nModelIndex == MODEL_SKIMMER && physicalFlags.bSubmergedInWater) { // 0x100
        DoBoatSplashes(m_fForcedOrientation);
    }
}

// 0x6CAB70
void CPlane::Render() {
    m_nTimeTillWeNeedThisCar = CTimer::GetTimeInMS() + 3000;
    CVehicle::Render();
}

// 0x6C9260
void CPlane::ProcessControl() {
    if (GetStatus() == STATUS_PLAYER) {
        if (m_nModelIndex == MODEL_CROPDUST || m_nModelIndex == MODEL_STUNT) {
            // Пад пилота: pad 1 только если водитель - PLAYER2, иначе pad 0
            uint8 padNumber = 0;
            if (m_pDriver && m_pDriver->m_nPedType == PED_TYPE_PLAYER2) {
                padNumber = 1;
            }
            if (CPad::GetPad(padNumber)->IsRightShockPressed()) {
                m_bSmokeEjectorEnabled = !m_bSmokeEjectorEnabled;
            }
        }
    }

    if (m_bSmokeEjectorEnabled) {
        if (!vehicleFlags.bEngineOn || vehicleFlags.bIsDrowning || !m_pDriver) {
            m_bSmokeEjectorEnabled = false;
        }
    }

    if (m_nModelIndex == MODEL_SKIMMER) {
        m_damageManager.SetAllWheelsState(WHEEL_STATUS_MISSING);
    }

    CAutomobile::ProcessControl();

    m_vehicleAudio.m_DoCountStalls = static_cast<int16>(m_StallCounter);
    if (m_StallCounter) {
        m_StallCounter = 0;
    }

    CVehicle::ProcessWeapons();

    if (m_nModelIndex == MODEL_VORTEX) {
        m_WheelStates[0] = WHEEL_STATE_NORMAL;
        m_WheelStates[1] = WHEEL_STATE_NORMAL;
        m_WheelStates[2] = WHEEL_STATE_NORMAL;
        m_WheelStates[3] = WHEEL_STATE_NORMAL;
    }

    if (m_pSmokeParticle) {
        RwMatrix out;
        m_nSmokeTimer += (uint32)(-CTimer::GetTimeStepInMS());
        m_pSmokeParticle->GetCompositeMatrix(&out);
        CVector velocity = -m_vecMoveSpeed * 5.0f;
        auto particleData = FxPrtMult_c(0.0f, 0.0f, 0.0f, 0.2f, 1.0f, 1.0f, 0.1f);
        g_fx.m_SmokeHuge->AddParticle(out.pos, velocity, 0.00f, particleData);
        g_fx.m_SmokeHuge->AddParticle(out.pos, velocity, 0.05f, particleData);
        if (m_nSmokeTimer <= 0 || vehicleFlags.bIsDrowning) {
            m_pSmokeParticle->Kill();
            m_pSmokeParticle = nullptr;
        }
    }
}

// 0x6CADD0
void CPlane::ProcessControlInputs(uint8 playerNum) {
    // Константы ввода сверены с реверсом CAutomobile::ProcessControlInputs (0x6AD690):
    // 0x858B88 = 1/128, 0x858CC4 = 0.2, 0x871908 = 0.0035, 0x859A3C = 1/255.
    const float timeStep = CTimer::GetTimeStep();
    const float fwdSpeed = m_vecMoveSpeed.Dot(GetForward());
    const auto  pad      = CPad::GetPad(playerNum);

    const auto KeyboardSteer = [&] {
        m_fSteeringLeftRight += ((float)pad->GetSteeringLeftRight() / 128.0f - m_fSteeringLeftRight) * timeStep * 0.2f;
        m_fSteeringUpDown    += ((float)pad->GetSteeringUpDown()    / 128.0f - m_fSteeringUpDown)    * timeStep * 0.2f;
    };
    const auto MouseSteer = [&] {
        m_nLastControlInput = eControllerType::MOUSE;
        if (!pad->NewState.m_bVehicleMouseLook) {
            m_fSteeringLeftRight += CPad::NewMouseControllerState.m_AmountMoved.x * 0.0035f;
            m_fSteeringUpDown    += CPad::NewMouseControllerState.m_AmountMoved.y * 0.0035f;
        }
        // возврат к нулю (аргументы pow(0x822130) в декомпиле потеряны; база/показатель - как в CAutomobile::ProcessControlInputs)
        if (std::abs(m_fSteeringLeftRight) < StaticRef<float>(0x87190C)) {
            m_fSteeringLeftRight *= std::pow(0.975f, timeStep);
        }
        if (std::abs(m_fSteeringUpDown) < StaticRef<float>(0x87190C)) {
            m_fSteeringUpDown *= std::pow(0.975f, timeStep);
        }
    };

    // === Руль направления / тангаж (0x990 / 0x98C): мышиный флайт или пад ===
    if (!(CCamera::m_bUseMouse3rdPerson && m_bEnableMouseFlying)) {
        m_nLastControlInput = eControllerType::KEYBOARD;
        KeyboardSteer();
    } else {
        bool useMouse = true;
        if (CPad::NewMouseControllerState.m_AmountMoved.x == 0.0f && CPad::NewMouseControllerState.m_AmountMoved.y == 0.0f) {
            useMouse = false;
            if ((std::abs(m_fSteeringLeftRight) > 0.0f || std::abs(m_fSteeringUpDown) > 0.0f) && m_nLastControlInput == eControllerType::MOUSE) {
                // мышь уже рулит: без ввода с пада продолжаем мышью (затухание к нулю)
                if (pad->GetSteeringLeftRight() == 0 && pad->GetSteeringUpDown() == 0)
                    useMouse = true;
            }
            if (!useMouse) {
                if (pad->GetSteeringLeftRight() == 0 && pad->GetSteeringUpDown() == 0 && m_nLastControlInput == eControllerType::MOUSE) {
                    // ввода нет - значения не меняем
                } else {
                    m_nLastControlInput = eControllerType::KEYBOARD;
                    KeyboardSteer();
                }
            }
        }
        if (useMouse) {
            MouseSteer();
        }
    }
    m_fSteeringLeftRight = std::clamp(m_fSteeringLeftRight, -1.0f, 1.0f);
    m_fSteeringUpDown    = std::clamp(m_fSteeringUpDown, -1.0f, 1.0f);

    // === Снос (0x988) ===
    float skidDelta;
    if (m_nModelIndex == MODEL_VORTEX) {
        skidDelta = ((float)pad->GetSteeringLeftRight() / 128.0f - m_fLeftRightSkid) * timeStep;
    } else {
        const auto lookLeft  = pad->GetLookLeft();
        const auto lookRight = pad->GetLookRight();
        skidDelta = (lookLeft ? (-1.0f - m_fLeftRightSkid) : ((float)lookRight - m_fLeftRightSkid)) * timeStep;
    }
    m_fLeftRightSkid = skidDelta * 0.2f + m_fLeftRightSkid;
    m_fLeftRightSkid = std::clamp(m_fLeftRightSkid, -1.0f, 1.0f);

    // === Угол переднего колеса (0x494) ===
    const float steerFactor = std::max(1.0f - fwdSpeed, StaticRef<float>(0x858B1C));
    m_fSteerAngle = -(m_pHandlingData->m_fSteeringLock * DEG_TO_RAD * m_fLeftRightSkid * steerFactor);
    if (m_nModelIndex == MODEL_VORTEX) {
        if (pad->GetHandBrake() != 0) {
            m_fSteerAngle    *= StaticRef<float>(0x858CE8);
            m_fLeftRightSkid *= StaticRef<float>(0x858CE8);
        }
    }

    // === Тяга/тормоз (0x994) ===
    m_fAccelerationBreakStatus = (float)(pad->GetAccelerate() - pad->GetBrake()) / 255.0f;

    // === Шасси (0x9CC): сравнения с 1.0f в оригинале побитовые (int == 0x3F800000) ===
    const bool anyWheelTouching = m_fWheelsSuspensionCompression[0] != 1.0f
        || m_fWheelsSuspensionCompression[1] != 1.0f
        || m_fWheelsSuspensionCompression[2] != 1.0f
        || m_fWheelsSuspensionCompression[3] != 1.0f;
    if (!pad->IsRightShockPressed() // шок не нажат "только что"
        || anyWheelTouching
        || 1.0f <= m_pFlyingHandlingData->m_fGearUpR
        || m_nModelIndex == MODEL_RCBARON) {
        // штатный переход состояния шасси
        if (m_LandingGearAngle >= 0.0f) {
            if (m_LandingGearAngle > 0.0f && m_LandingGearAngle < 1.0f) {
                m_LandingGearAngle += timeStep * StaticRef<float>(0x871904);
                if (m_LandingGearAngle >= 1.0f)
                    SetGearUp();
            }
        } else {
            // выпускаем
            m_LandingGearAngle += timeStep * StaticRef<float>(0x871904);
            if (m_LandingGearAngle >= 0.0f) {
                m_LandingGearAngle = 0.0f;
                m_damageManager.SetWheelStatus(CAR_WHEEL_FRONT_LEFT,  WHEEL_STATUS_OK);
                m_damageManager.SetWheelStatus(CAR_WHEEL_REAR_LEFT,   WHEEL_STATUS_OK);
                m_damageManager.SetWheelStatus(CAR_WHEEL_FRONT_RIGHT, WHEEL_STATUS_OK);
                m_damageManager.SetWheelStatus(CAR_WHEEL_REAR_RIGHT,  WHEEL_STATUS_OK);
                m_fAirResistance = m_pHandlingData->m_fDragMult / 1000.0f * 0.5f;
            }
        }
    } else if (m_LandingGearAngle == 0.0f) {
        // начинаем убирать (в оригинале - inline-часть SetGearUp)
        m_damageManager.SetWheelStatus(CAR_WHEEL_FRONT_LEFT,  WHEEL_STATUS_MISSING);
        m_damageManager.SetWheelStatus(CAR_WHEEL_REAR_LEFT,   WHEEL_STATUS_MISSING);
        m_damageManager.SetWheelStatus(CAR_WHEEL_FRONT_RIGHT, WHEEL_STATUS_MISSING);
        m_damageManager.SetWheelStatus(CAR_WHEEL_REAR_RIGHT,  WHEEL_STATUS_MISSING);
        m_LandingGearAngle = timeStep * StaticRef<float>(0x871904);
    } else if (m_LandingGearAngle == 1.0f) {
        m_LandingGearAngle = timeStep * StaticRef<float>(0x871904) - 1.0f;
    } else {
        m_LandingGearAngle *= -1.0f; // развернуть направление перехода
    }

    // === Сопла Хайдры (0x208) ===
    if (m_nModelIndex == MODEL_HYDRA) {
        if (10.0f < std::abs((float)pad->GetCarGunUpDown())) {
            m_wMiscComponentAnglePrev = m_wMiscComponentAngle;
            // FP-аргумент _ftol2 (0x821B40) в декомпиле потерян: масштаб = carGunUpDown * timeStep (проверить по ассемблеру)
            const auto nozzleDelta = (int16)((float)pad->GetCarGunUpDown() * timeStep);
            int32 newAngle = nozzleDelta + (int16)m_wMiscComponentAngle;
            if (newAngle < 0)
                newAngle = 0;
            m_wMiscComponentAngle = (uint16)std::min<int32>(newAngle, HARRIER_NOZZLE_ROTATE_LIMIT);
        }
    }

    // === Педали (0x49C/0x4A0) ===
    vehicleFlags.bIsHandbrakeOn = false;
    m_GasPedal = 0.0f;
    bool bPedalsSet = false;
    if (0.0f < fwdSpeed) {
        if (const auto brake = pad->GetBrake()) {
            m_BrakePedal = fwdSpeed <= StaticRef<float>(0x858F9C)
                ? (float)brake / 255.0f
                : (float)brake * StaticRef<float>(0x871A70);
            bPedalsSet = true;
        }
    }
    if (!bPedalsSet && m_vecMoveSpeed.Magnitude() < StaticRef<float>(0x858B1C)) {
        if (pad->GetBrake() < 10 && pad->GetAccelerate() < 10) {
            m_BrakePedal = 0.5f; // удержание тормозов на месте
            bPedalsSet = true;
        }
    }
    if (!bPedalsSet) {
        m_BrakePedal = 0.0f;
    }

    // катсцена: зажать тормоза и ограничить скорость
    if (pad->DisablePlayerControls != 0) {
        m_BrakePedal = 1.0f;
        vehicleFlags.bIsHandbrakeOn = true;
        m_GasPedal = 0.0f;
        FindPlayerPed()->AsPlayer()->KeepAreaAroundPlayerClear();
        const auto speed = m_vecMoveSpeed.Magnitude();
        if (StaticRef<float>(0x871254) < speed) {
            m_vecMoveSpeed *= StaticRef<float>(0x871254) / speed;
        }
    }
}

// 0x6CB7C0
void CPlane::ProcessFlyingCarStuff() {
    // NOTE: инкремент m_planeDamageWave в начале - аргументы _ftol2 (0x821B40) в декомпиле потеряны,
    // точное значение требует сверки с ассемблером.
    const float timeStep = CTimer::GetTimeStep();
    if (timeStep < 0.0f)
        return;

    // фаза волны турбулентности (используется в sin ниже)
    m_planeDamageWave += (int)((float)(rand() % 0x100) * timeStep); // TODO(asm): аргумент _ftol2 потерян

    const auto Rand01 = [] { return (float)rand() * StaticRef<float>(0x858C7C); }; // ~1/32768

    // масштаб скорости для силы турбулентности (0x858B3C ~ 1/макс.скорости), максимум 1.0
    const float speed = m_vecMoveSpeed.Magnitude();
    const float speedScale = std::min(speed * StaticRef<float>(0x858B3C), 1.0f);

    float skid    = m_fLeftRightSkid;        // 0x988
    float steerUD = m_fSteeringUpDown;       // 0x98C
    float steerLR = m_fSteeringLeftRight;    // 0x990

    // крушение игрока: рывки рулями и перевод пропеллеров в "повреждено"
    if (m_fHealth < StaticRef<float>(0x859F80) && GetStatus() == STATUS_PLAYER) {
        m_damageManager.SetAeroplaneCompStatus(PLANE_STATIC_PROP, DAMSTATE_DAMAGED);
        m_damageManager.SetAeroplaneCompStatus(PLANE_MOVING_PROP, DAMSTATE_DAMAGED);
        skid += 0.5f;
        if (std::abs(GetRoll()) < StaticRef<float>(0x86DAF4)) {
            steerLR += StaticRef<float>(0x858F34);
        }
        if (StaticRef<float>(0x858FE4) < std::abs(GetRoll())) {
            steerUD += 0.5f;
        }
    }

    if (m_nModelIndex != MODEL_RCBARON) {
        // турбулентность по поврежденным компонентам (узлы 12..24);
        // панели: m_panels[3] + m_swingingChassis как 4-й слот (как в оригинале)
        auto* panels = reinterpret_cast<CBouncingPanel*>(m_panels.data());
        for (auto node = (int32)PLANE_STATIC_PROP; node < PLANE_NUM_NODES; node++) { // FIX(int-plane) [I-4]: см. VehicleDamage
            const auto compStatus = (int)m_damageManager.GetAeroplaneCompStatus(node);
            if (!m_aCarNodes[node] || compStatus <= 0)
                continue;
            if (node > PLANE_AILERON_R) // шасси/прочее - турбулентности нет (default-ветка switch оригинала)
                continue;

            // индекс болтающейся панели этого узла (если есть)
            int32 panelIdx = -1;
            for (auto j = 0; j < 4; j++) {
                if (panels[j].m_nFrameId == node) { panelIdx = j; break; }
            }

            float force;
            if (node <= PLANE_MOVING_PROP2) {
                // пропеллеры: тряска тяги по волне
                if (0.0f < m_fAccelerationBreakStatus) {
                    const float lo = 1.0f - StaticRef<float>(0x8D344C);
                    const float hi = StaticRef<float>(0x8D344C) + 1.0f;
                    const float wavePhase = (float)(uint32)m_planeDamageWave * TWO_PI / StaticRef<float>(0x8D3448); // uint-конверсия как в оригинале
                    m_fAccelerationBreakStatus = ((hi - lo) * Rand01() + lo)
                        * (std::sin(wavePhase) - 1.0f)
                        * (float)(compStatus * compStatus)
                        * timeStep * StaticRef<float>(0x8D3444)
                        + m_fAccelerationBreakStatus;
                }
                force = (float)compStatus * 0.5f;
            } else if (node == PLANE_RUDDER) {
                // руль направления
                m_fLeftRightSkid = (1.0f - (float)compStatus * 0.2f) * m_fLeftRightSkid;
                const float amp = StaticRef<float>(0x8D3440);
                m_fLeftRightSkid = (2.0f * amp * Rand01() - amp)
                    * (float)(compStatus * compStatus) * timeStep * speedScale
                    + m_fLeftRightSkid;
                skid = m_fLeftRightSkid;
                if (panelIdx != -1) {
                    auto& bp = panels[panelIdx];
                    const float pAmp = StaticRef<float>(0x8D343C);
                    bp.m_vecPos.y = (2.0f * pAmp * Rand01() - pAmp)
                        * (float)(compStatus * compStatus) * timeStep * speedScale
                        + bp.m_vecPos.y;
                    skid -= StaticRef<float>(0x8D3438) * bp.m_vecRotation.y;
                }
                force = (float)compStatus * 0.2f;
            } else if (node <= PLANE_ELEVATOR_R) {
                // элеваторы (тангаж)
                m_fSteeringUpDown = (1.0f - (float)compStatus * 0.2f) * m_fSteeringUpDown;
                const float amp = StaticRef<float>(0x8D3434);
                m_fSteeringUpDown = (2.0f * amp * Rand01() - amp)
                    * (float)(compStatus * compStatus) * timeStep * speedScale
                    + m_fSteeringUpDown;
                steerUD = m_fSteeringUpDown;
                if (panelIdx != -1) {
                    auto& bp = panels[panelIdx];
                    const float pAmp = StaticRef<float>(0x8D3430);
                    bp.m_vecPos.y = (2.0f * pAmp * Rand01() - pAmp)
                        * (float)(compStatus * compStatus) * timeStep * speedScale
                        + bp.m_vecPos.y;
                    steerUD -= StaticRef<float>(0x8D342C) * bp.m_vecRotation.y;
                }
                force = (float)compStatus * StaticRef<float>(0x858FCC);
            } else {
                // элероны (крен)
                m_fSteeringLeftRight = (1.0f - (float)compStatus * 0.2f) * m_fSteeringLeftRight;
                const float amp = StaticRef<float>(0x8D3428);
                m_fSteeringLeftRight = (2.0f * amp * Rand01() - amp)
                    * (float)(compStatus * compStatus) * timeStep * speedScale
                    + m_fSteeringLeftRight;
                steerLR = m_fSteeringLeftRight;
                if (panelIdx != -1) {
                    auto& bp = panels[panelIdx];
                    const float pAmp = StaticRef<float>(0x8D3424);
                    bp.m_vecPos.y = (2.0f * pAmp * Rand01() - pAmp)
                        * (float)(compStatus * compStatus) * timeStep * speedScale
                        + bp.m_vecPos.y;
                    steerLR += (node == PLANE_AILERON_L ? 1.0f : -1.0f) * StaticRef<float>(0x8D3420) * bp.m_vecRotation.y;
                }
                force = (float)compStatus * 0.25f;
            }

            // частицы срыва потра с поврежденного узла
            if (0.0f < force && !vehicleFlags.bIsDrowning
                && (StaticRef<float>(0x858C24) < speedScale || (rand() & 7) == 0)) {

                const auto distToCamSq = (GetPosition() - TheCamera.GetPosition()).SquaredMagnitude();
                if (distToCamSq < StaticRef<float>(0x859AEC) || GetStatus() == STATUS_PLAYER) {
                    if (const auto frame = m_aCarNodes[node]) {
                        const CVector compPos = GetMatrix().TransformPoint(RwFrameGetMatrix(frame)->pos);
                        auto particleData = FxPrtMult_c(0.0f, 0.0f, 0.0f, 0.2f, 1.0f, 1.0f, force);

                        // скорость частиц = скорость самолета * 0.25 * коэф. (0x858B40), рандомизированная
                        const float mult = StaticRef<float>(0x858B40);
                        const float rndA = StaticRef<float>(0x871A74);
                        const float bias = StaticRef<float>(0x858C20);
                        CVector vel{
                            (Rand01() * rndA + bias) * (m_vecMoveSpeed.x * 0.25f * mult),
                            (Rand01() * rndA + bias) * (m_vecMoveSpeed.y * 0.25f * mult),
                            (Rand01() * rndA + bias) * (m_vecMoveSpeed.z * 0.25f * mult),
                        };
                        CVector pos{
                            2.0f * force * Rand01() - force + compPos.x,
                            2.0f * force * Rand01() - force + compPos.y,
                            2.0f * force * Rand01() - force + compPos.z,
                        };
                        g_fx.m_SmokeHuge->AddParticle(pos, vel, 0.0f, particleData);
                    }
                }
            }
        }
    }

    // === Обороты пропеллера (0x9C4) ===
    const auto status = GetStatus();
    if (status != STATUS_PLAYER && status != STATUS_REMOTE_CONTROLLED && status != STATUS_PHYSICS) {
        // NPC: глушим движок, пропеллер останавливается
        const float downRate = timeStep * StaticRef<float>(0x858CDC);
        if (m_fPropSpeed <= downRate) {
            m_fPropSpeed = 0.0f;
            vehicleFlags.bEngineOn = false;
            return;
        }
        if (PLANE_STD_PROP_SPEED < m_fPropSpeed) {
            m_nFakePhysics = 0;
            m_fPropSpeed -= timeStep * StaticRef<float>(0x859CD8);
            return;
        }
        m_nFakePhysics = 0;
        m_fPropSpeed -= downRate;
        return;
    }

    // игрок: целевые обороты от тяги
    float targetPropSpeed = PLANE_STD_PROP_SPEED;
    if (m_fAccelerationBreakStatus != 0.0f) {
        const float delta = (m_fAccelerationBreakStatus > 0.0f)
            ? PLANE_MAX_PROP_SPEED - PLANE_STD_PROP_SPEED
            : PLANE_STD_PROP_SPEED - PLANE_MIN_PROP_SPEED;
        targetPropSpeed = delta * m_fAccelerationBreakStatus + PLANE_STD_PROP_SPEED;
    }

    if (status == STATUS_PLAYER || status == STATUS_REMOTE_CONTROLLED) {
        const auto flightModel = m_nModelIndex != MODEL_RCBARON ? FLIGHT_MODEL_PLANE : FLIGHT_MODEL_RCPLANE;
        const auto aboveCeiling = HeightAboveCeiling(GetPosition().z, flightModel);
        if (0.0f < aboveCeiling) {
            // за потолком теряем обороты и тягу
            targetPropSpeed *= std::max(aboveCeiling * StaticRef<float>(0x858B38), 0.0f);
            m_fAccelerationBreakStatus = std::max(
                m_fAccelerationBreakStatus - aboveCeiling * StaticRef<float>(0x858CEC),
                -1.0f
            );
        }
    }

    m_fPropSpeed += (targetPropSpeed - m_fPropSpeed) * timeStep * PLANE_ROC_PROP_SPEED;

    const bool bDrowning = vehicleFlags.bIsDrowning;
    if (bDrowning) {
        m_fPropSpeed = 0.0f;
        m_fAccelerationBreakStatus = 0.0f;
        vehicleFlags.bEngineOn = false;
    }

    if (m_nModelIndex == MODEL_RCBARON) {
        if (!bDrowning && vehicleFlags.bEngineOn) {
            FlyingControl(FLIGHT_MODEL_RCPLANE, skid, steerUD, steerLR, m_fAccelerationBreakStatus);
        }
        return;
    }

    if (vehicleFlags.bEngineOn
        && (PLANE_MIN_PROP_SPEED < m_fPropSpeed || StaticRef<float>(0x859068) < m_vecMoveSpeed.SquaredMagnitude())) {
        if (m_nModelIndex == MODEL_HYDRA
            && GetStatus() == STATUS_PLAYER
            && (int)HARRIER_NOZZLE_SWITCH_LIMIT <= (int)m_wMiscComponentAngle) {
            // вертикальный режим: временно подменяем лётные данные на хантеровские
            const auto originalFlyingData = m_pFlyingHandlingData;
            // FIX(int-plane) [I-3]: AsVehicleModelInfo() в репо нет - есть AsVehicleModelInfoPtr() (BaseModelInfo.h:180)
            m_pFlyingHandlingData = gHandlingDataMgr.GetFlyingPointer(
                static_cast<uint8>(CModelInfo::GetModelInfo(MODEL_HUNTER)->AsVehicleModelInfoPtr()->m_nHandlingId)
            );
            if (StaticRef<float>(0x859EF8) < m_fAccelerationBreakStatus
                || (m_nNumContactWheels < 4 && !physicalFlags.bTouchingWater)) {
                FlyingControl(FLIGHT_MODEL_HELI, skid, steerUD, -steerLR, StaticRef<float>(0x8D341C) * m_fAccelerationBreakStatus);
            }
            m_pFlyingHandlingData = originalFlyingData;
            return;
        }
        FlyingControl(FLIGHT_MODEL_PLANE, skid, steerUD, steerLR, m_fAccelerationBreakStatus);
    }
}
