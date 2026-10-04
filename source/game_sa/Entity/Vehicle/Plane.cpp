#include "StdInc.h"

#include "Plane.h"
#include "CarCtrl.h"
#include "FireManager.h"
#include "Pools/Pools.h"

auto& HARRIER_NOZZLE_ROTATERATE = StaticRef<float>(0x8D33DC);       // 25.0f
auto& PLANE_DAMAGE_WAVE_COUNTER_VAR = StaticRef<float>(0x8D33E0);   // 0.75f
auto& PLANE_DAMAGE_THRESHHOLD = StaticRef<float>(0x8D33E4);         // 500.0f
auto& PLANE_DAMAGE_SCALE_MASS = StaticRef<float>(0x8D33E8);         // 10000.0f
auto& PLANE_DAMAGE_DESTROY_THRESHHOLD = StaticRef<float>(0x8D33EC); // 5000.0f
auto& vecRCBaronGunPos = StaticRef<CVector>(0x8D33F0);            // <0.0f, 0.45f, 0.0f>

void CPlane::InjectHooks() {
    RH_ScopedVirtualClass(CPlane, 0x871948, 71);
    RH_ScopedCategory("Vehicle");

    RH_ScopedInstall(Constructor, 0x6C8E20, { .reversed = false });
    RH_ScopedInstall(InitPlaneGenerationAndRemoval, 0x6CAD90);
    RH_ScopedVMTInstall(SetUpWheelColModel, 0x6C9140);
    RH_ScopedVMTInstall(BurstTyre, 0x6C9150);
    RH_ScopedVMTInstall(PreRender, 0x6C94A0, { .reversed = false });
    RH_ScopedVMTInstall(Render, 0x6CAB70);
    RH_ScopedInstall(IsAlreadyFlying, 0x6CAB90);
    RH_ScopedVMTInstall(Fix, 0x6CABB0);
    RH_ScopedVMTInstall(SetupDamageAfterLoad, 0x6CAC10);
    RH_ScopedInstall(SetGearUp, 0x6CAC20);
    RH_ScopedInstall(SetGearDown, 0x6CAC70);
    RH_ScopedVMTInstall(OpenDoor, 0x6CACB0);
    RH_ScopedVMTInstall(ProcessControl, 0x6C9260);
    RH_ScopedVMTInstall(ProcessControlInputs, 0x6CADD0, { .reversed = false });
    RH_ScopedVMTInstall(ProcessFlyingCarStuff, 0x6CB7C0, { .reversed = false });
    RH_ScopedVMTInstall(VehicleDamage, 0x6CC4B0, { .reversed = false });
    RH_ScopedInstall(CountPlanesAndHelis, 0x6CCA50);
    RH_ScopedInstall(AreWeInNoPlaneZone, 0x6CCAA0);
    RH_ScopedInstall(AreWeInNoBigPlaneZone, 0x6CCBB0);
    RH_ScopedInstall(SwitchAmbientPlanes, 0x6CCC50);
    RH_ScopedVMTInstall(BlowUpCar, 0x6CCCF0);
    // NB: `FindPlaneCreationCoors` is now reversed; it keeps two unreversed leaf helpers as
    // plugin::Call (0x40FD10 = CColSphere-like ctor (radius, center, 0, 0, 0xFF) and 0x41A820 =
    // sector-scan "is sector occupied" check - cdecl(int xSector, int ySector, CVector*, CVector*,
    // CVector*, CVector*), convention verified against its caller; the float->int conversions around
    // it are just MSVC `_ftol2` (0x821B40)). `DoPlaneGenerationAndRemoval` stays disabled (see note there).
    RH_ScopedInstall(FindPlaneCreationCoors, 0x6CD090);
    RH_ScopedInstall(DoPlaneGenerationAndRemoval, 0x6CD2F0, { .reversed = false });
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
    m_fLandingGearStatus           = 0.0f;
    field_9A0                      = 0;
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
    return plugin::CallMethod<0x6CCCF0, CPlane*, CEntity*, uint8>(this, damager, bHideExplosion);

    // untested \ wip
    if (!vehicleFlags.bCanBeDamaged)
        return;

    if (GetStatus() == STATUS_PLAYER || m_autoPilot.m_nCarMission == MISSION_PLANE_CRASH_AND_BURN || m_nModelIndex == MODEL_RCBARON) {
        if (damager == FindPlayerPed() || damager == FindPlayerVehicle()) {
            FindPlayerInfo().m_nHavocCaused += 20;
            FindPlayerInfo().m_fCurrentChaseValue += 10.0f;
            CStats::IncrementStat(STAT_COST_OF_PROPERTY_DAMAGED, (float)CGeneral::GetRandomNumberInRange(4000, 10'000));
        }

        if (GetStatus() == STATUS_PLAYER) { // strange
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

        // m_nType = m_nType & 7 | STATUS_WRECKED;
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

            // todo: shit
            if (auto node = m_aCarNodes[PLANE_WHEEL_LF]) {
                damager = 0;
                RwFrameForAllObjects(node, GetCurrentAtomicObjectCB, &damager);
                if (damager) {
                    rwObjectSetFlags(damager, 0); // RpAtomic* damager
                }
            }
        }
        // this->m_nBombLightsWinchFlags &= 0xF8u;
        m_fHealth = 0.0f;
        m_wBombTimer = 0;

        TheCamera.CamShake(0.4f, GetPosition());
        KillPedsInVehicle();
        // auto v23 = this->m_nFlags1 & 0xAF;
        // this->m_nBombLightsWinchFlags &= 0xE7u;
        // this->m_nFlags1 = v23;
        m_bSmokeEjectorEnabled = false;
        // this->m_nFlags6 = m_nFlags6 & 0x7F; // vehicleFlags.bSirenOrAlarm = false;
        // this->ucNPCVehicleFlags = this->ucNPCVehicleFlags & 0xFE;

        if (vehicleFlags.bIsAmbulanceOnDuty) {
            vehicleFlags.bIsAmbulanceOnDuty = false;
            --CCarCtrl::NumAmbulancesOnDuty;
        }

        if (vehicleFlags.bIsFireTruckOnDuty) {
            vehicleFlags.bIsFireTruckOnDuty = false;
            --CCarCtrl::NumFireTrucksOnDuty;
        }

        ChangeLawEnforcerState(0);
        gFireManager.StartFire(this, damager, 0.8f, 1, 7000, 0);
        CDarkel::RegisterCarBlownUpByPlayer(*this, 0);
        if (m_nModelIndex == MODEL_RCBARON) {
            CExplosion::AddExplosion(this, damager, EXPLOSION_RC_VEHICLE, GetPosition(), 0, 1, -1.0f, 0);
        } else {

            CExplosion::AddExplosion(this, damager, EXPLOSION_AIRCRAFT, GetPosition(), 0, 1, -1.0f, 0);
        }
    } else {
        m_autoPilot.SetCarMission(MISSION_PLANE_CRASH_AND_BURN);
        m_fHealth = 0.0f;
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
    plugin::CallMethod<0x6CC4B0, CPlane*, float, eVehicleCollisionComponent, CEntity*, CVector*, CVector*, eWeaponType>(this, damageIntensity, component, damager, vecCollisionCoors, vecCollisionDirection, weapon);
}

// 0x6CAB90
void CPlane::IsAlreadyFlying() {
    m_nStartedFlyingTime = CTimer::GetTimeInMS() - 20000;
}

// 0x6CAC20
void CPlane::SetGearUp() {
    m_fLandingGearStatus = 1.0f;
    m_fAirResistance = m_pHandlingData->m_fDragMult / 1000.0f / 2.0f * m_pFlyingHandlingData->m_fGearUpR;
    m_damageManager.SetWheelStatus(CAR_WHEEL_FRONT_LEFT,  WHEEL_STATUS_MISSING);
    m_damageManager.SetWheelStatus(CAR_WHEEL_REAR_LEFT,   WHEEL_STATUS_MISSING);
    m_damageManager.SetWheelStatus(CAR_WHEEL_FRONT_RIGHT, WHEEL_STATUS_MISSING);
    m_damageManager.SetWheelStatus(CAR_WHEEL_REAR_RIGHT,  WHEEL_STATUS_MISSING);
}

// 0x6CAC70
void CPlane::SetGearDown() {
    m_fLandingGearStatus = 0.0f;
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
    if (GenPlane_Active == false) {
        GenPlane_Active = enable;
        return;
    }

    // Planes are being switched off - remove all ambient helis/planes
    if (!enable) {
        for (auto& veh : GetVehiclePool()->GetAllValid()) {
            const auto subType = veh.m_nVehicleSubType;
            // CVehicle +0x4A4: plane AI status, 1 == ambient (AI) plane/heli
            const auto planeAIStatus = *reinterpret_cast<const uint8*>(reinterpret_cast<uintptr_t>(&veh) + 0x4A4);
            if ((subType == VEHICLE_TYPE_HELI || subType == VEHICLE_TYPE_PLANE) && planeAIStatus == 1) {
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
            plugin::Call<0x563470>(); // unreversed helper
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
// BLOCKER (not reversed yet): Runs every frame at game timer `(frame & 0x1F) == 0x1E` while ambient
// planes are enabled (0x8D33BC): decides via `CPlane::AreWeInNoPlaneZone`-style checks (0x6CCA50) and the
// streaming model-request state (0x8E4CD0 availability table + 0x4087E0 RequestModel) whether to create a
// new ambient plane at the `FindPlaneCreationCoors` position (creating it via CPopulation/CStreaming),
// tracks the existing ambient planes (table 0xC1CAD8/0xC1CADC state + model id), sets their flight height
// / autopilot, and removes planes that flew out of range. Not translated because the creation/removal
// helpers (0x6CCA50, 0x4087E0 and the plane-spawn path) and the ambient-plane state globals
// (0xC1CAD8/0xC1CADC, 0x8D33BC) are not yet mapped to repo headers.
void CPlane::DoPlaneGenerationAndRemoval() {
    plugin::Call<0x6CD2F0>();
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
    plugin::CallMethod<0x6C94A0, CPlane*>(this);
}

// 0x6CAB70
void CPlane::Render() {
    m_nTimeTillWeNeedThisCar = CTimer::GetTimeInMS() + 3000;
    CVehicle::Render();
}

// 0x6C9260
void CPlane::ProcessControl() {
    return plugin::CallMethod<0x6C9260, CPlane*>(this);

    // untested
    if (GetStatus() == STATUS_PLAYER) {
        if (m_nModelIndex == MODEL_CROPDUST || m_nModelIndex == MODEL_STUNT) {
            auto pad = CPad::GetPad(m_pDriver->GetPadNumber());
            if (pad->IsRightShockPressed()) {
                m_bSmokeEjectorEnabled = m_bSmokeEjectorEnabled == 0;
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

    m_vehicleAudio.m_DoCountStalls = static_cast<int16>(field_9A0);
    if (field_9A0) {
        field_9A0 = 0;
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
    plugin::CallMethod<0x6CADD0, CPlane*, uint8>(this, playerNum);
}

// 0x6CB7C0
void CPlane::ProcessFlyingCarStuff() {
    plugin::CallMethod<0x6CB7C0, CPlane*>(this);
}
