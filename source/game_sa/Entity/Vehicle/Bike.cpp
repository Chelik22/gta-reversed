/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/

// ---------------------------------------------------------------------------
// INTEGRATION BUILD (w2-01-Bike + w2-02-Bike -> source/game_sa/Entity/Vehicle/Bike.cpp)
//
// Батч: 13 функций CBike (w2-01; w2-02 — подмножество тех же адресов).
// Источник тел: w2-01-Bike\0x*.cpp; для SetRemoveAnimFlags — тело w2-02 (см. PORT-ADAPT #1).
// 0x6B9250 ProcessControl в батч НЕ входит (не переведена, w2-01 STATUS) — редирект-стаб сохранён,
// {.Reversed = false} НЕ снят. Остальные стабы (вне батча) не тронуты.
//
// PORT-ADAPT фиксы (адаптация фрагментов к лейауту/именам этого репо):
//  #1  SetRemoveAnimFlags: w2-01 ставил ANIMATION_DONT_ADD_TO_PARTIAL_BLEND (0x400), а оригинал —
//      byte[assoc+0x2E] |= 0x4 => ANIMATION_IS_BLEND_AUTO_REMOVE (взято тело w2-02, AnimBlendAssociation.h:23).
//  #2  ProcessRiderAnims — static (Bike.h:151): m_GasPedal/m_BrakePedal без this не существуют;
//      это поля хвоста CVehicle (Vehicle.h:347-348), а не CBike => vehicle->m_GasPedal / vehicle->m_BrakePedal.
//  #3  ProcessRiderAnims: tBikeHandlingData в репо = 0x40, БЕЗ трансмиссии => handling->GetTransmission()
//      не существует. Декомпил читает vehicle+0x384+0x8C = m_pHandlingData (0x384) -> m_transmissionData
//      (0x2C) + 0x60 => m_MaxReverseVelocity (offset-точное имя репо; в месте использования множитель 0.0f).
//  #4  ProcessAI: m_BikeHandling->GetTransmission().CalculateGearForSimpleCar(...) — трансмиссии в
//      tBikeHandlingData нет (см. #3) => m_pHandlingData->GetTransmission(), как в CAutomobile::ProcessAI
//      (Automobile.cpp:1221-1222).
//  #5  PreRender: ABI-байт 0x42B&1 (коп-байк) => vehicleFlags.bCreatedAsPoliceVehicle (Vehicle.h:288;
//      семантика по w2-01 STATUS).
//  #6  PreRender: ABI-байт 0x42B&0x40 (выхлоп) => !vehicleFlags.bIsDrowning; маска 0x1000 =>
//      VEHICLE_HANDLING_MODEL_NO_EXHAUST; bss-float 0xC1C81C поднят в file-scope StaticRef.
//  #7  PreRender: StoreShadowForVehicle(*this,1) => (this, VEH_SHD_BIKE) (Shadows.h:60/236);
//      Multiply3x3(GetMatrix(),v) => GetMatrix().TransformVector(v) (Matrix.h:167);
//      m_nPhysicalFlags&0x20000000 => physicalFlags.bRenderScorched (bit 29);
//      имена енумов репо: CORREFL_NONE/LOSCHECK_ON/TRAIL_OFF; CPointLights::AddLight выровнен
//      по сигнатуре репо (radius=10.0f=0x41200000; параметры — приближение w2-01, отмечено в STATUS);
//      eSkidmarkType (enum class) -> static_cast<uint32>.
//  #8  ProcessControlInputs: *(int32_t*)&m_nVehicleType != VEHICLE_TYPE_BMX => именное сравнение.
//  #9  FixHandsToBars: ABI *(uint8**)((uint8*)mi+0x5C) + float'ы @+0x78/0x7C/0x80 =>
//      AsVehicleModelInfoPtr()->GetVehicleStruct()->m_avDummyPos[DUMMY_HAND_REST]
//      (0x78 = 10*0xC; VehicleModelInfo.h:36/277).
// ---------------------------------------------------------------------------

#include "StdInc.h"

#include "Bike.h"

#include "Buoyancy.h"

#include "AnimBlendAssociation.h"                  // CAnimBlendAssociation, eAnimationFlags
#include "AnimManager.h"                           // CAnimManager::AddAnimation/BlendAnimation
#include "Camera.h"                                // TheCamera::m_bUseMouse3rdPerson
#include "CarAI.h"                                 // CCarAI::UpdateCarAI
#include "CarCtrl.h"                               // CCarCtrl::UpdateCarOnRails/SteerAICarWithPhysics
#include "Coronas.h"                               // CCoronas::RegisterCorona, eCoronaType...
#include "EventKnockOffBike.h"                     // CEventKnockOffBike, eKnockOffType
#include "GameLogic.h"                             // CGameLogic::SkipState, eSkipState
#include "General.h"                               // CGeneral
#include "ModelInfo.h"                             // CModelInfo
#include "Pad.h"                                   // CPad, CMouseControllerState
#include "Ped.h"                                   // CPed
#include "PlayerPed.h"                             // CPlayerPed::KeepAreaAroundPlayerClear
#include "Plugins/RpAnimBlendPlugin/RpAnimBlend.h" // RpAnimBlendClumpGetAssociation и др.
#include "PointLights.h"                           // CPointLights::AddLight
#include "RwHelper.h"                              // GetAnimHierarchyFromSkinClump
#include "Shadows.h"                               // CShadows::StoreShadowForVehicle, VEH_SHD_BIKE
#include "Stats.h"                                 // CStats
#include "SurfaceInfo_c.h"                         // eAdhesionGroup
#include "SurfaceInfos_c.h"                        // g_surfaceInfos
#include "Timer.h"                                 // CTimer
#include "VehicleRecording.h"                      // CVehicleRecording::bUseCarAI
#include "Weather.h"                               // CWeather::WetRoads
#include "World.h"                                 // CWorld::Players, FindPlayerPed

// PORT-ADAPT #6: bss-float 0xC1C81C (не идентифицирован, см. w2-01 STATUS) — делитель в PreRender
static auto& s_fUnkBikeSpeedDivisor = StaticRef<float>(0xC1C81C);

void CBike::InjectHooks() {
    RH_ScopedVirtualClass(CBike, 0x871360, 67);
    RH_ScopedCategory("Vehicle");

    RH_ScopedInstall(Constructor, 0x6BF430);
    RH_ScopedInstall(Destructor, 0x6B57A0);
    RH_ScopedInstall(dmgDrawCarCollidingParticles, 0x6B5A00);
    RH_ScopedInstall(DamageKnockOffRider, 0x6B5A10);
    RH_ScopedInstall(KnockOffRider, 0x6B5F40);
    RH_ScopedInstall(SetRemoveAnimFlags, 0x6B5F50);
    RH_ScopedInstall(ReduceHornCounter, 0x6B5F90);
    RH_ScopedVMTInstall(ProcessAI, 0x6BC930);
    RH_ScopedInstall(ProcessBuoyancy, 0x6B5FB0);
    RH_ScopedInstall(ResetSuspension, 0x6B6740);
    RH_ScopedInstall(GetAllWheelsOffGround, 0x6B6790);
    RH_ScopedInstall(DebugCode, 0x6B67A0);
    RH_ScopedInstall(DoSoftGroundResistance, 0x6B6D40);
    RH_ScopedInstall(PlayHornIfNecessary, 0x6B7130);
    RH_ScopedInstall(CalculateLeanMatrix, 0x6B7150);
    RH_ScopedInstall(ProcessRiderAnims, 0x6B7280);
    RH_ScopedInstall(FixHandsToBars, 0x6B7F90);
    RH_ScopedInstall(PlaceOnRoadProperly, 0x6BEEB0);
    RH_ScopedInstall(GetCorrectedWorldDoorPosition, 0x6BF230);
    RH_ScopedVMTInstall(Fix, 0x6B7050);
    RH_ScopedVMTInstall(BlowUpCar, 0x6BEA10);
    RH_ScopedVMTInstall(ProcessDrivingAnims, 0x6BF400);
    RH_ScopedVMTInstall(BurstTyre, 0x6BEB20);
    RH_ScopedVMTInstall(ProcessControlInputs, 0x6BE310);
    RH_ScopedVMTInstall(ProcessEntityCollision, 0x6BDEA0);
    RH_ScopedVMTInstall(Render, 0x6BDE20);
    RH_ScopedVMTInstall(PreRender, 0x6BD090);
    RH_ScopedVMTInstall(Teleport, 0x6BCFC0);
    RH_ScopedVMTInstall(ProcessControl, 0x6B9250, { .Reversed = false }); // НЕ в батче (0x6B9250 не переведена) — стаб сохранён
    RH_ScopedVMTInstall(VehicleDamage, 0x6B8EC0);
    RH_ScopedVMTInstall(SetupSuspensionLines, 0x6B89B0);
    RH_ScopedVMTInstall(SetModelIndex, 0x6B8970);
    RH_ScopedVMTInstall(PlayCarHorn, 0x6B7080);
    RH_ScopedVMTInstall(SetupDamageAfterLoad, 0x6B7070);
    RH_ScopedVMTInstall(DoBurstAndSoftGroundRatios, 0x6B6950);
    RH_ScopedVMTInstall(SetUpWheelColModel, 0x6B67E0);
    RH_ScopedVMTInstall(RemoveRefsToVehicle, 0x6B67B0);
    RH_ScopedVMTInstall(ProcessControlCollisionCheck, 0x6B6620);
    RH_ScopedVMTInstall(GetComponentWorldPosition, 0x6B5990);
    RH_ScopedVMTInstall(ProcessOpenDoor, 0x6B58D0);
}

// 0x6BF430
CBike::CBike(int32 modelIndex, eVehicleCreatedBy createdBy) : CVehicle(createdBy) {
    auto mi = CModelInfo::GetModelInfo(modelIndex)->AsVehicleModelInfoPtr();
    if (mi->m_nVehicleType == VEHICLE_TYPE_BIKE) {
        const auto& animationStyle = CAnimManager::GetAnimBlocks()[mi->GetAnimFileIndex()].GroupId;
        m_RideAnimData.AnimGroup = animationStyle;
        if (animationStyle < ANIM_GROUP_BIKES || animationStyle > ANIM_GROUP_WAYFARER) {
            m_RideAnimData.AnimGroup = ANIM_GROUP_BIKES;
        }
    }

    m_nVehicleSubType = VEHICLE_TYPE_BIKE;
    m_nVehicleType = VEHICLE_TYPE_BIKE;

    m_BlowUpTimer = 0.0f;
    m_nBrakesOn = false;
    nBikeFlags = 0;
    SetModelIndex(modelIndex);

    m_pHandlingData = gHandlingDataMgr.GetVehiclePointer(mi->m_nHandlingId);
    m_BikeHandling = gHandlingDataMgr.GetBikeHandlingPointer(mi->m_nHandlingId);
    m_nHandlingFlagsIntValue = m_pHandlingData->m_nHandlingFlags;
    m_pFlyingHandlingData = gHandlingDataMgr.GetFlyingPointer(static_cast<uint8>(mi->m_nHandlingId));
    m_fBrakeCount = 20.0f;
    mi->ChooseVehicleColour(m_nPrimaryColor, m_nSecondaryColor, m_nTertiaryColor, m_nQuaternaryColor, 1);
    m_fSwingArmLength = 0.0f;
    m_fForkYOffset = 0.0f;
    m_fForkZOffset = 0.0f;
    m_nFixLeftHand = false;
    m_nFixRightHand = false;
    m_fSteerAngleTan = std::tan(DegreesToRadians(mi->m_fBikeSteerAngle));
    m_fMass = m_pHandlingData->m_fMass;
    m_fTurnMass = m_pHandlingData->m_fTurnMass;
    m_vecCentreOfMass = m_pHandlingData->m_vecCentreOfMass;
    m_vecCentreOfMass.z = 0.1f;
    m_fAirResistance = GetDefaultAirResistance();
    m_fElasticity = 0.05f;
    m_fBuoyancyConstant = m_pHandlingData->m_fBuoyancyConstant;
    m_fSteerAngle = 0.0f;
    m_GasPedal = 0.0f;
    m_BrakePedal = 0.0f;
    m_Damager = nullptr;
    m_pWhoInstalledBombOnMe = nullptr;
    m_GasPedalAudioRevs = 0.0f;
    m_fTyreTemp = 1.0f;
    m_fBrakingSlide = 0.0f;
    m_PrevSpeed = 0.0f;

    for (auto i = 0; i < 2; ++i) {
        m_nWheelStatus[i] = 0;
        m_aWheelSkidmarkType[i] = eSkidmarkType::DEFAULT;
        m_bWheelBloody[i] = false;
        m_bMoreSkidMarks[i] = false;
        m_aWheelPitchAngles[i] = 0.0f;
        m_aWheelAngularVelocity[i] = 0.0f;
        m_aWheelSuspensionHeights[i] = 0.0f;
        m_aWheelOrigHeights[i] = 0.0f;
        m_WheelStates[i] = WHEEL_STATE_NORMAL;
    }

    for (auto i = 0; i < 4; ++i) {
        m_aWheelColPoints[i] = {};
        m_aWheelRatios[i] = 1.0f;
        m_aRatioHistory[i] = 0.0f;
        m_WheelCounts[i] = 0.0f;
        m_fSuspensionLength[i] = 0.0f;
        m_fLineLength[i] = 0.0f;
        m_aGroundPhysicalPtrs[i] = nullptr;
        m_aGroundOffsets[i] = CVector{};
    }

    m_nNoOfContactWheels = 0;
    m_NumDriveWheelsOnGround = 0;
    m_NumDriveWheelsOnGroundLastFrame = 0;
    m_fHeightAboveRoad = 0.0f;
    m_fExtraTractionMult = 1.0f;

    if (!mi->m_pColModel->m_pColData->m_pLines) {
        mi->m_pColModel->m_pColData->m_nNumLines = 4;
        mi->m_pColModel->m_pColData->m_pLines = static_cast<CColLine*>(CMemoryMgr::Malloc(4 * sizeof(CColLine)));
        mi->m_pColModel->m_pColData->m_pLines[1].m_vecStart.x = 99'999.99f; // todo: explain this
    }
    mi->m_pColModel->m_pColData->m_pLines[0].m_vecStart.z = 99'999.99f;
    CBike::SetupSuspensionLines();

    m_autoPilot.m_nTempAction = TEMPACT_NONE;
    m_autoPilot.SetCarMission(MISSION_NONE, 0);
    m_autoPilot.carCtrlFlags.bAvoidLevelTransitions = false;

    SetStatus(STATUS_SIMPLE);
    m_nNumPassengers = 0;
    vehicleFlags.bLowVehicle = false;
    vehicleFlags.bIsBig = false;
    vehicleFlags.bIsVan = false;

    m_bLeanMatrixCalculated = false;
    m_mLeanMatrix = *m_matrix;
    m_vecOldSpeedForPlayback = CVector{};
    m_vehicleAudio.Initialise(this);
}

// 0x6B57A0
CBike::~CBike() {
    m_vehicleAudio.Terminate();
}

// 0x6B5A00
void CBike::dmgDrawCarCollidingParticles(const CVector& position, float power, eWeaponType weaponType) {
    // NOP
}

// 0x6B5A10
bool CBike::DamageKnockOffRider(CVehicle* vehicle, float damageIntensity, uint16 pieceType, CEntity* damager, const CVector& collisionPos, const CVector& collisionImpactVelocity) {
    const auto driver    = vehicle->m_pDriver;
    const auto passenger = vehicle->m_apPassengers[0];

    // Impact force relative to the bike's mass
    auto force = damageIntensity / vehicle->m_fMass * 800.0f;

    // A skilled rider resists being knocked off (unless flagged to always come off)
    if (vehicle->GetStatus() != STATUS_PLAYER) {
        if (driver && driver->CantBeKnockedOffBike != CANT_BE_KNOCKED_OFF_ALWAYS_NORMAL) {
            force *= 1.0f - driver->GetBikeRidingSkill() * 0.6f;
        }
    } else {
        force *= 0.75f;
        if (driver) {
            force *= 1.0f - driver->GetBikeRidingSkill() * 0.5f;
        }
    }

    // Only an actual driver gets knocked off
    if (!driver || !driver->IsStateDriving() || force <= 10.0f) {
        return false;
    }

    // A ped already reacting to a hit isn't also knocked off (cops are exempt)
    if (const auto task = driver->GetIntelligence()->GetTaskManager().GetActiveTask()) {
        if (task->GetTaskType() == TASK_SIMPLE_BE_HIT && !driver->IsCop()) {
            return false;
        }
    }

    const auto impactFwdMag   = vehicle->GetForward().Dot(collisionImpactVelocity);
    const auto impactUpMag    = vehicle->GetUp().Dot(collisionImpactVelocity);
    const auto impactRightMag = vehicle->GetRight().Dot(collisionImpactVelocity);

    // Per-axis weighting of the impact
    auto fwdWeight = 0.6f;
    if (std::abs(impactFwdMag) > 0.85f) {
        const auto vertical = collisionImpactVelocity.z < 0.85f ? 0.0f : collisionImpactVelocity.z;
        fwdWeight = 7.0f * sq(vertical) + 0.6f;
    }
    if (vehicle->GetUp().z < 0.0f) { // bike lying on its side / upside down
        fwdWeight = 5.0f;
    }

    auto backWeight = 1.5f;
    auto upWeight   = 0.05f;
    if (vehicle->m_nModelIndex == MODEL_SANCHEZ) {
        fwdWeight *= 0.65f;
        upWeight  *= 0.75f;
    } else if (vehicle->IsSubQuad()) {
        backWeight = 3.0f;
        fwdWeight *= 0.65f;
        upWeight  *= 0.75f;
    }

    if (impactFwdMag > 0.0f) {
        fwdWeight *= 1.0f - driver->GetBikeRidingSkill() * 0.6f;
    }

    force *= std::abs(impactFwdMag) * fwdWeight
           + std::max(impactUpMag, 0.0f) * upWeight
           + std::abs(impactRightMag) * 0.45f
           - std::min(impactUpMag, 0.0f) * backWeight;

    // Don't knock the player off while they're on stairs
    if (driver->IsPlayer() && CCullZones::CamStairsForPlayer() && CCullZones::FindZoneWithStairsAttributeForPlayer()) {
        force = 0.0f;
    }

    // ALWAYS_HARD peds come off at a much lower force threshold
    if (force <= (driver->CantBeKnockedOffBike == CANT_BE_KNOCKED_OFF_ALWAYS_HARD ? 20.0f : 75.0f)) {
        return false;
    }

    // NEVER peds are never knocked off
    if (driver->CantBeKnockedOffBike == CANT_BE_KNOCKED_OFF_NEVER) {
        return false;
    }
    if (passenger && passenger->CantBeKnockedOffBike == CANT_BE_KNOCKED_OFF_NEVER) {
        return false;
    }

    // The driver (guaranteed present here) is thrown off, and so is the passenger, both reacting with the driver's facing
    const auto knockOffDir = (uint8)driver->GetLocalDirection(-CVector2D{ collisionImpactVelocity });

    driver->GetEventGroup().Add(CEventKnockOffBike{
        vehicle, vehicle->m_vecMoveSpeed, collisionImpactVelocity, damageIntensity, 0.05f * force, KNOCK_OFF_TYPE_SKIDBACKFRONT, knockOffDir, 0, nullptr, true, false
    });
    if (passenger) {
        passenger->GetEventGroup().Add(CEventKnockOffBike{
            vehicle, vehicle->m_vecMoveSpeed, collisionImpactVelocity, damageIntensity, 0.05f * force, KNOCK_OFF_TYPE_SKIDBACKFRONT, knockOffDir, 0, nullptr, false, false
        });
    }
    return true;
}

// dummy function
// 0x6B5F40
CPed* CBike::KnockOffRider(eWeaponType arg0, uint8 arg1, CPed* ped, bool arg3) {
    return ped;
}

// 0x6B5F50
void CBike::SetRemoveAnimFlags(CPed* ped) {
    // NOTE ABI: Ghidra видит один аргумент; тело оперирует ТОЛЬКО аргументом-педом (this не читается):
    // ped+0x36 & 7 == ENTITY_TYPE_PED, ped+0x18 -> GetRpClump().
    if (ped->GetType() != ENTITY_TYPE_PED) {
        return;
    }

    // Помечаем все второстепенные (secondary task) анимации педа флагом авто-удаления.
    // [PORT-ADAPT #1] оригинал: byte[assoc+0x2E] |= 0x4 => ANIMATION_IS_BLEND_AUTO_REMOVE
    for (auto assoc = RpAnimBlendClumpGetFirstAssociation(ped->GetRpClump(), ANIMATION_SECONDARY_TASK_ANIM);
         assoc;
         assoc = RpAnimBlendGetNextAssociation(assoc, ANIMATION_SECONDARY_TASK_ANIM)) {
        assoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
    }
}

// 0x6B5F90
void CBike::ReduceHornCounter() {
    if (m_HornCounter)
        m_HornCounter -= 1;
}

// 0x6B5FB0
void CBike::ProcessBuoyancy() {
    CVector vecBuoyancyTurnPoint;
    CVector vecBuoyancyForce;
    if (!mod_Buoyancy.ProcessBuoyancy(this, m_fBuoyancyConstant, &vecBuoyancyTurnPoint, &vecBuoyancyForce)) {
        vehicleFlags.bIsDrowning = false;
        physicalFlags.bSubmergedInWater = false;
        physicalFlags.bTouchingWater = false;
        return;
    }

    physicalFlags.bTouchingWater = true;
    ApplyMoveForce(vecBuoyancyForce);
    ApplyTurnForce(vecBuoyancyForce, vecBuoyancyTurnPoint);

    auto fTimeStep = std::max(0.01F, CTimer::GetTimeStep());
    auto fUsedMass = m_fMass / 125.0F;
    auto fBuoyancyForceZ = vecBuoyancyForce.z / (fTimeStep * fUsedMass);

    if (fUsedMass > m_fBuoyancyConstant)
        fBuoyancyForceZ *= 1.05F * fUsedMass / m_fBuoyancyConstant;

    if (physicalFlags.bMakeMassTwiceAsBig)
        fBuoyancyForceZ *= 1.5F;

    auto fBuoyancyForceMult = std::max(0.5F, 1.0F - fBuoyancyForceZ / 20.0F);
    auto fSpeedMult = std::pow(fBuoyancyForceMult, CTimer::GetTimeStep());
    m_vecMoveSpeed *= fSpeedMult;
    m_vecTurnSpeed *= fSpeedMult;

    // 0x6B6443
    if (fBuoyancyForceZ > 0.8F || (fBuoyancyForceZ > 0.4F && IsAnyWheelNotMakingContactWithGround())) {
        vehicleFlags.bIsDrowning = true;
        physicalFlags.bSubmergedInWater = true;

        m_vecMoveSpeed.z = std::max(-0.1F, m_vecMoveSpeed.z);

        if (m_pDriver) {
            ProcessPedInVehicleBuoyancy(m_pDriver->AsPed(), true);
        }
        else {
            vehicleFlags.bEngineOn = false;
        }

        for (const auto passenger : GetPassengers()) {
            ProcessPedInVehicleBuoyancy(passenger, false);
        }
    }
    else {
        vehicleFlags.bIsDrowning = false;
        physicalFlags.bSubmergedInWater = false;
    }
}

inline void CBike::ProcessPedInVehicleBuoyancy(CPed* ped, bool bIsDriver) {
    if (!ped)
        return;

    ped->physicalFlags.bTouchingWater = true;
    if (!ped->IsPlayer() && bikeFlags.bWaterTight)
        return;

    if (ped->IsPlayer())
        ped->AsPlayer()->HandlePlayerBreath(true, 1.0F);

    if (IsAnyWheelMakingContactWithGround()) {
        if (!ped->IsPlayer()) {
            auto pedDamageResponseCalc = CPedDamageResponseCalculator(this, CTimer::GetTimeStep(), eWeaponType::WEAPON_DROWNING, PED_PIECE_TORSO, false);
            auto damageEvent = CEventDamage(this, CTimer::GetTimeInMS(), eWeaponType::WEAPON_DROWNING, PED_PIECE_TORSO, 0, false, true);
            if (damageEvent.AffectsPed(ped))
                pedDamageResponseCalc.ComputeDamageResponse(ped, damageEvent.m_damageResponse, true);
            else
                damageEvent.m_damageResponse.m_bDamageCalculated = true;

            ped->GetEventGroup().Add(&damageEvent, false);
        }
    } else {
        auto knockOffBikeEvent = CEventKnockOffBike(this, m_vecMoveSpeed, m_vecLastCollisionImpactVelocity, m_fDamageIntensity, 0.0F, KNOCK_OFF_TYPE_FALL, 0, 0, nullptr, bIsDriver, false);
        ped->GetEventGroup().Add(&knockOffBikeEvent);
        if (bIsDriver) {
            vehicleFlags.bEngineOn = false;
        }
    }
}

// 0x6BC930
bool CBike::ProcessAI(uint32& extraHandlingFlags) {
    const auto mi = CModelInfo::GetModelInfo(m_nModelIndex);

    m_autoPilot.carCtrlFlags.bHonkAtCar = false; // 0x3DB &= 0xFC
    m_autoPilot.carCtrlFlags.bHonkAtPed = false;

    // Плейбек записи без AI - ничего не делаем
    const auto recordingId = m_autoPilot.m_vehicleRecordingId;
    if (recordingId >= 0 && !CVehicleRecording::bUseCarAI[recordingId]) {
        extraHandlingFlags += 2;
        return false;
    }

    switch (GetStatus()) {
    case STATUS_PLAYER: {
        extraHandlingFlags += 2;
        bikeFlags.bGettingPickedUp = false;

        // Пассажир за рулём: обработка ввода от игрока (поддержка 2 игроков)
        if (FindPlayerPed()->m_nPedState != PEDSTATE_DEAD && FindPlayerPed()->m_nPedState != PEDSTATE_DRAGGED_FROM_CAR) {
            if (m_pDriver) {
                if (CWorld::Players[0].m_pPed == m_pDriver) {
                    ProcessControlInputs(0);
                } else if (CWorld::Players[1].m_pPed == m_pDriver) {
                    ProcessControlInputs(1);
                }
            }

            // Наклон байка (lean) => центр масс и прижимающая сила
            if (m_RideAnimData.LeanFwd >= 0.0f) {
                m_vecCentreOfMass.y = m_BikeHandling->m_fLeanFwdCOM * m_RideAnimData.LeanFwd + m_pHandlingData->m_vecCentreOfMass.y;
                if (m_BrakePedal < 0.0f || m_nNoOfContactWheels == 0) {
                    const auto speed = std::max(0.1f /* 0x8712FC */, m_vecMoveSpeed.Magnitude());
                    const auto k     = std::min(speed / 0.1f, m_BrakePedal);
                    const auto force = m_BikeHandling->m_fLeanFwdForce * m_fTurnMass * m_RideAnimData.LeanFwd * speed
                        * (k + m_BrakePedal) * 0.5f /* 0x858B8C */
                        * CStats::GetFatAndMuscleModifier(STAT_MOD_11); // 0xB - модификатор скилла байка
                    ApplyTurnForce(-CTimer::GetTimeStep() * force * GetUp(), m_vecCentreOfMass + GetForward());
                }
            } else {
                m_vecCentreOfMass.y = m_BikeHandling->m_fLeanBakCOM * m_RideAnimData.LeanFwd + m_pHandlingData->m_vecCentreOfMass.y;
                if ((m_BrakePedal == 0.0f && !vehicleFlags.bIsHandbrakeOn) || m_nNoOfContactWheels == 0) {
                    const auto speed = std::max(0.1f, m_vecMoveSpeed.Magnitude());
                    const auto k     = std::min(speed / 0.1f, m_GasPedal);
                    const auto force = m_BikeHandling->m_fLeanBakForce * m_fTurnMass * m_RideAnimData.LeanFwd * speed
                        * (k + m_GasPedal) * 0.5f
                        * CStats::GetFatAndMuscleModifier(STAT_MOD_11);
                    ApplyTurnForce(-CTimer::GetTimeStep() * force * GetUp(), m_vecCentreOfMass + GetForward());
                }
            }

            PruneReferences();
            if (GetStatus() == STATUS_PLAYER) {
                DoDriveByShootings();
            }
            DoSoftGroundResistance(extraHandlingFlags);
        }

        if (CPad::GetPad(0)->CarGunJustDown()) {
            ActivateBomb();
            return true;
        }
        break;
    }
    case STATUS_PLAYER_PLAYBACK_FROM_BUFFER:
        extraHandlingFlags += 2;
        return true;
    case STATUS_SIMPLE: {
        CCarAI::UpdateCarAI(this);
        CPhysical::ProcessControl();
        CCarCtrl::UpdateCarOnRails(this);
        m_NumDriveWheelsOnGroundLastFrame = m_NumDriveWheelsOnGround;
        m_nNoOfContactWheels              = 2;
        m_NumDriveWheelsOnGround          = 2;
        // [PORT-ADAPT #4] трансмиссии в tBikeHandlingData (0x40) нет - как в CAutomobile::ProcessAI
        m_pHandlingData->GetTransmission().CalculateGearForSimpleCar(m_autoPilot.m_speed / 50.0f, m_nCurrentGear);
        const auto vmi = mi->AsVehicleModelInfoPtr();
        m_aWheelPitchAngles[0] += CVehicle::ProcessWheelRotation(WHEEL_STATE_NORMAL, GetForward(), m_vecMoveSpeed, vmi->m_fWheelSizeFront * 0.5f) * CTimer::GetTimeStep();
        m_aWheelPitchAngles[1] += CVehicle::ProcessWheelRotation(WHEEL_STATE_NORMAL, GetForward(), m_vecMoveSpeed, vmi->m_fWheelSizeRear * 0.5f) * CTimer::GetTimeStep();
        if (m_autoPilot.carCtrlFlags.bHonkAtCar || m_autoPilot.carCtrlFlags.bHonkAtPed) {
            PlayCarHorn();
        }
        if (m_HornCounter != 0) {
            m_HornCounter--;
        }
        bikeFlags.bEngineOnFire         = false; // 0x614 &= 0x7F
        vehicleFlags.bRestingOnPhysical = false; // 0x42B &= 0xDE (вместе с bDriverLastFrame)
        vehicleFlags.bDriverLastFrame   = false;
        return true;
    }
    case STATUS_PHYSICS:
    case STATUS_GHOST: {
        CCarAI::UpdateCarAI(this);
        CCarCtrl::SteerAICarWithPhysics(this);
        if (m_autoPilot.carCtrlFlags.bHonkAtCar || m_autoPilot.carCtrlFlags.bHonkAtPed) {
            PlayCarHorn();
        }
        extraHandlingFlags += 2;
        bikeFlags.bEngineOnFire = false;
        if (vehicleFlags.bIsBeingCarJacked) {
            vehicleFlags.bIsHandbrakeOn = true;
            m_GasPedal   = 0.0f;
            m_BrakePedal = 1.0f;
            return true;
        }
        bikeFlags.bGettingPickedUp = false; // 0x614 &= 0x77
        return true;
    }
    case STATUS_ABANDONED: {
        m_BrakePedal = 0.0f;
        if (m_vecMoveSpeed.SquaredMagnitude() < 0.01f /* 0x858C58 */ || bikeFlags.bOnSideStand) {
            vehicleFlags.bIsHandbrakeOn = true;
        } else {
            vehicleFlags.bIsHandbrakeOn = false;
        }
        m_GasPedal    = 0.0f;
        m_HornCounter = 0;
        if ((m_pDriver || m_apPassengers[0] || vehicleFlags.bIsBeingCarJacked) && !bikeFlags.bOnSideStand) {
            extraHandlingFlags += 2;
        }
        m_RideAnimData.AnimLeanLeft = 0.0f;
        m_RideAnimData.AnimLeanFwd  = 0.0f;
        bikeFlags.bEngineOnFire     = false;
        if (vehicleFlags.bIsBeingCarJacked) {
            vehicleFlags.bIsHandbrakeOn = true;
            m_GasPedal   = 0.0f;
            m_BrakePedal = 1.0f;
            return true;
        }
        break;
    }
    case STATUS_WRECKED:
        m_BrakePedal = 0.05f;
        vehicleFlags.bIsHandbrakeOn = true;
        m_fSteerAngle = 0.0f;
        m_GasPedal    = 0.0f;
        m_HornCounter = 0;
        bikeFlags.bEngineOnFire = false;
        m_RideAnimData.AnimLeanLeft = 0.0f;
        m_RideAnimData.AnimLeanFwd  = 0.0f;
        break;
    case STATUS_FORCED_STOP:
        if (m_vecMoveSpeed.SquaredMagnitude() >= 0.01f) {
            m_BrakePedal = 0.0f;
            vehicleFlags.bIsHandbrakeOn = false;
        } else {
            vehicleFlags.bIsHandbrakeOn = true;
            m_BrakePedal = 1.0f;
        }
        m_fSteerAngle = 0.0f;
        m_GasPedal    = 0.0f;
        m_HornCounter = 0;
        extraHandlingFlags += 2;
        bikeFlags.bEngineOnFire = false;
        return true;
    }
    return true;
}

// 0x6BF400
void CBike::ProcessDrivingAnims(CPed* driver, bool blend) {
    if (m_bOffscreen && GetStatus() == STATUS_PLAYER)
        return;

    ProcessRiderAnims(driver, this, &m_RideAnimData, m_BikeHandling, 0);
}

// 0x6B7280
void CBike::ProcessRiderAnims(CPed* rider, CVehicle* vehicle, CRideAnimData* rideData, tBikeHandlingData* handling, int16 a5) {
    // NOTE: разделяется с автомобилями: для VEHICLE_TYPE_AUTOMOBILE контактные колёса читаются по
    //   оригинальному смещению veh+0x960 (поле CAutomobile, в репо не смэплено - оставлен ABI-доступ).
    const auto riderIsPlayer = rider->IsPlayer();

    // Контактные колёса (функция разделяется с автомобилями)
    char numContactWheels{};
    CBike*    bikePtr{};
    CVehicle* automobilePtr{};
    if (vehicle->m_nVehicleType == VEHICLE_TYPE_BIKE) {
        numContactWheels = (char)reinterpret_cast<CBike*>(vehicle)->m_nNoOfContactWheels;
        bikePtr = reinterpret_cast<CBike*>(vehicle);
    } else if (vehicle->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
        numContactWheels = *reinterpret_cast<char*>(reinterpret_cast<uint8*>(vehicle) + 0x960); // ABI: CAutomobile+0x960 в репо не смэплено
        automobilePtr = vehicle;
    }

    auto leanTarget  = 1.0f; // свободный блендинг: 1 - still - pushes - fwd
    auto wheelieWant = 0.0f;

    const auto clump     = rider->GetRpClump();
    const auto animGroup = rideData->AnimGroup;

    auto animStill = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_STILL);  // 195
    auto animLeft  = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_LEFT);   // 196
    auto animRight = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_RIGHT);  // 197
    CAnimBlendAssociation *animFwd{}, *animBack{};
    if (riderIsPlayer) {
        animFwd  = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_FWD);  // 199
        animBack = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_BACK); // 198
    }
    auto animPushes  = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_PUSHES); // 200
    auto animDriveBy = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_DRIVEBYLHS); // 202
    if (!animDriveBy) {
        animDriveBy = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_DRIVEBYRHS); // 203
        if (!animDriveBy) {
            animDriveBy = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_DRIVEBYFT); // 204
        }
    }

    const auto fwdSpeed = vehicle->m_vecMoveSpeed.Dot(vehicle->GetForward());

    // BMX: при drive-by / толкании руки не фиксируем
    if (vehicle->m_nVehicleSubType == VEHICLE_TYPE_BMX && (animPushes || animDriveBy)) {
        bikePtr->m_nFixRightHand = 0; // veh+0x7B5
    }

    const auto KnockOffRider = [&](bool isDriver) {
        // [PORT-ADAPT, смежный с #2] в репо CEventKnockOffBike принимает const CVector&, а не указатели
        CEventKnockOffBike event{ vehicle, vehicle->m_vecMoveSpeed, CVector{}, 0.0f, 0.0f, KNOCK_OFF_TYPE_SKIDBACK_FALLR /* 0x36 */, 2, 0, nullptr, isDriver, false };
        (isDriver ? rider : vehicle->m_pDriver)->GetEventGroup().Add(&event, false);
    };
    const auto FadeOutAnims = [&] {
        if (animStill && animStill->m_BlendDelta >= 0.0f) {
            animStill->m_BlendDelta = -4.0f;
        }
        if (animPushes && animPushes->m_BlendDelta >= 0.0f) {
            animPushes->m_BlendDelta = -4.0f;
        }
    };
    const auto NeedsBlend = [](CAnimBlendAssociation* anim) {
        return !anim || (anim->m_BlendAmount < 1.0f && (anim->m_BlendDelta < 0.0f != (anim->m_BlendDelta == 0.0f)));
    };

    //> Слетание с байка на скорости (по скиллу)
    const auto fallSpeed = (rider->GetBikeRidingSkill() + 1.0f) * 0.3f /* 0x871348 */;
    if (sq(fallSpeed) < vehicle->m_vecMoveSpeed.SquaredMagnitude()) {
        KnockOffRider(true);
        if (vehicle->m_pDriver && vehicle->m_pDriver != rider) {
            KnockOffRider(false);
        }
    }

    //> Выбор анимаций по движению
    if (animDriveBy || 0.02f /* 0x871334 */ <= std::abs(fwdSpeed)) {
        if (fwdSpeed >= 0.0f) {
            FadeOutAnims();
        } else {
            auto skillThresh = (rider->GetBikeRidingSkill() + 1.0f) * 3.5f /* 0x859028 */;
            if (vehicle->m_nVehicleSubType == VEHICLE_TYPE_QUAD) {
                skillThresh += skillThresh;
            }
            // [PORT-ADAPT #3] оригинал читает vehicle+0x384+0x8C = m_pHandlingData->m_transmissionData.m_MaxReverseVelocity;
            // у tBikeHandlingData (параметр handling) трансмиссии в этом репо нет (sizeof == 0x40)
            const auto topGearVel = vehicle->m_pHandlingData->GetTransmission().m_MaxReverseVelocity;
            if (skillThresh * topGearVel <= fwdSpeed
                || (!numContactWheels
                    && (vehicle->GetUp().z >= -0.5f /* 0x858F40 */
                        || CTimer::m_snTimeInMilliseconds - vehicle->m_nCreationTime > 99))) {
                // [PORT-ADAPT #2] m_GasPedal - поле CVehicle (Vehicle.h:347), в static-функции нет this
                if (vehicle->m_GasPedal >= 0.0f || fwdSpeed <= topGearVel * 0.0f /* 0x86C680 */) {
                    if (riderIsPlayer && fwdSpeed < topGearVel * 0.0f) {
                        wheelieWant = -1.0f;
                    }
                    FadeOutAnims();
                } else if (NeedsBlend(animPushes)) {
                    animPushes = CAnimManager::BlendAnimation(clump, animGroup, ANIM_ID_BIKE_PUSHES, 4.0f);
                }
            } else {
                KnockOffRider(true);
                if (vehicle->m_pDriver && vehicle->m_pDriver != rider) {
                    KnockOffRider(false);
                }
            }
        }
    } else if (NeedsBlend(animStill)) {
        animStill = CAnimManager::BlendAnimation(clump, animGroup, ANIM_ID_BIKE_STILL, 2.0f);
    }

    //> Свободный блендинг = 1 - still - pushes - fwd
    const auto AnimBlendNow = [](CAnimBlendAssociation* anim) {
        return CTimer::ms_fTimeStepNonClipped * 0.02f /* 0x858B38 */ * anim->m_BlendDelta + anim->m_BlendAmount;
    };
    if (animStill) {
        leanTarget -= std::min(AnimBlendNow(animStill), leanTarget);
    }
    if (animPushes) {
        leanTarget -= std::min(AnimBlendNow(animPushes), leanTarget);
    }
    if (animFwd) {
        leanTarget -= std::min(AnimBlendNow(animFwd), leanTarget);
    }

    //> Целевой наклон (wheelie/stoppie) -> AnimLeanLeft
    auto targetLean = rideData->LeanAngle / handling->m_fFullAnimLean;
    if (wheelieWant == -1.0f) {
        targetLean = 0.0f;
    } else if (targetLean <= 1.0f) {
        if (targetLean < -1.0f) {
            targetLean = -1.0f;
        }
    } else {
        targetLean = 1.0f;
    }
    const auto leanK = std::pow(0.89f /* 0x87134C */, CTimer::GetTimeStep());
    rideData->AnimLeanLeft = leanK * rideData->AnimLeanLeft + (1.0f - leanK) * targetLean;

    //> AnimLeanFwd: wheelie/stoppie (только для игрока без водителя)
    if (!riderIsPlayer || vehicle->m_pDriver) {
        wheelieWant = 0.0f;
        if (bikePtr) {
            bikePtr->bikeFlags.bEngineOnFire = false; // bike+0x614 &= 0x7F
        }
    } else {
        wheelieWant = rideData->LeanFwd;
        bool applyWheelieDelta{};

        if (!bikePtr) {
            // Не-байк (квад): завалка при опрокидывании (ABI: поля CAutomobile в репо не смэплены)
            const auto a7f4 = *reinterpret_cast<float*>(reinterpret_cast<uint8*>(automobilePtr) + 0x7F4);
            const auto a7f8 = *reinterpret_cast<float*>(reinterpret_cast<uint8*>(automobilePtr) + 0x7F8);
            const auto a7fc = *reinterpret_cast<float*>(reinterpret_cast<uint8*>(automobilePtr) + 0x7FC);
            const auto a800 = *reinterpret_cast<float*>(reinterpret_cast<uint8*>(automobilePtr) + 0x800);
            if (automobilePtr
                && (a7f4 < 0.0f != (a7f4 == 0.0f))
                && (a7f8 < 0.0f != (a7f8 == 0.0f))
                && vehicle->GetForward().z > 0.0f
                && (a7fc > 0.0f || a800 > 0.0f)) {
                wheelieWant = handling->m_fWheelieAng - vehicle->GetForward().z;
                if (wheelieWant < 0.15f /* 0x858FCC */) {
                    wheelieWant = -0.3f; // 0x871340
                }
                applyWheelieDelta = true;
            }
        } else {
            // Байк: контакты колёс решают - едем или опрокинулись
            bikePtr->bikeFlags.bEngineOnFire = false;
            const auto& wc = bikePtr->m_WheelCounts;
            const auto backTouching  = (wc[0] < 0.0f != (wc[0] == 0.0f)) || (wc[1] < 0.0f != (wc[1] == 0.0f));
            const auto frontTouching = (wc[2] < 0.0f != (wc[2] == 0.0f)) && (wc[3] < 0.0f != (wc[3] == 0.0f)) && vehicle->GetForward().z < 0.0f;
            if ((backTouching == frontTouching) || vehicle->GetForward().z <= 0.0f || (wc[2] <= 0.0f && wc[3] <= 0.0f)) {
                if (frontTouching && backTouching
                    && handling->m_fStoppieAng * 0.6f /* 0x858CC8 */ < handling->m_fStoppieAng - vehicle->GetForward().z) {
                    bikePtr->bikeFlags.bWheelieForCamera = true; // |= 0x80
                }
            } else {
                wheelieWant = handling->m_fWheelieAng - vehicle->GetForward().z;
                if (wheelieWant < handling->m_fWheelieAng * 0.5f) {
                    bikePtr->bikeFlags.bWheelieForCamera = true;
                }
                if (wheelieWant < 0.15f /* 0x858FCC */) {
                    wheelieWant = -0.3f; // 0x871340
                    applyWheelieDelta = true;
                }
            }
        }

        if (!applyWheelieDelta) {
            // LAB_006B7AC4: стоппи-порог по тормозу / wheelie по газу
            // [PORT-ADAPT #2] m_BrakePedal/m_GasPedal - поля CVehicle (Vehicle.h:347-348)
            if (vehicle->m_BrakePedal <= 0.5f || fwdSpeed <= 0.01f /* 0x858C58 */) {
                if (vehicle->m_GasPedal > 0.5f
                    && (wheelieWant < 0.0f != (wheelieWant == 0.0f))
                    && fwdSpeed < handling->m_fSpeedSteer * 0.3f /* 0x858C24 */
                    && wheelieWant >= -0.3f /* 0x871340 */) {
                    wheelieWant = -0.3f;
                }
            } else if (wheelieWant <= 0.1f /* 0x87133C */) {
                wheelieWant = 0.1f;
            }
        }

        // Гашение от целевого наклона: |target| > 0.3 => ослабляем
        if (0.3f /* 0x871358 */ < std::abs(targetLean)) {
            auto k = 1.0f - (std::abs(targetLean) - 0.3f) / (0.56f /* 0x871354 */ - 0.3f);
            wheelieWant *= std::max(k, 0.0f);
        }
    }

    rideData->AnimLeanFwd = riderIsPlayer ? leanK * rideData->AnimLeanFwd + (1.0f - leanK) * wheelieWant : 0.0f;

    //> Анимации влево/вправо по steer-осям
    auto blendSide = 0.0f; // вес fwd/back пары (из AnimLeanFwd), по умолчанию 0
    auto blendFwd  = 1.0f; // вес left/right пары (из AnimLeanLeft), по умолчанию 1
    if (std::abs(rideData->AnimLeanLeft) <= 0.56f /* 0x871354 */ && riderIsPlayer && std::abs(rideData->AnimLeanFwd) <= 0.56f) {
        blendFwd  = rideData->AnimLeanLeft;
        blendSide = rideData->AnimLeanFwd;
        const auto mag = std::sqrt(sq(blendSide) + sq(blendFwd));
        if (mag <= 0.01f /* 0x87135C */) {
            blendSide = std::abs(blendSide);
            blendFwd  = std::abs(blendFwd);
        } else {
            const auto k = 1.0f / mag;
            blendSide = std::abs(blendSide * k);
            blendFwd  = std::abs(blendFwd * k);
        }
    }
    if (riderIsPlayer) {
        if (!animFwd)  animFwd  = CAnimManager::AddAnimation(clump, animGroup, ANIM_ID_BIKE_FWD);
        if (!animBack) animBack = CAnimManager::AddAnimation(clump, animGroup, ANIM_ID_BIKE_BACK);
        if (rideData->AnimLeanFwd < 0.0f) {
            animBack->m_BlendAmount = blendSide * leanTarget;
            animBack->SetCurrentTime(-animBack->m_BlendHier->m_fTotalTime * rideData->AnimLeanFwd);
            animBack->m_Flags &= ~(uint16)ANIMATION_WALK; // byte[assoc+0x2E] &= 0xFE
            animFwd->m_BlendAmount = 0.0f;
        } else {
            animFwd->m_BlendAmount = blendSide * leanTarget;
            animFwd->SetCurrentTime(animFwd->m_BlendHier->m_fTotalTime * rideData->AnimLeanFwd);
            animFwd->m_Flags &= ~(uint16)ANIMATION_WALK;
            animBack->m_BlendAmount = 0.0f;
        }
        if (!animLeft)  animLeft  = CAnimManager::AddAnimation(clump, animGroup, ANIM_ID_BIKE_LEFT);
        if (!animRight) animRight = CAnimManager::AddAnimation(clump, animGroup, ANIM_ID_BIKE_RIGHT);
        if (rideData->AnimLeanLeft < 0.0f) {
            animLeft->m_BlendAmount = blendFwd * leanTarget;
            animLeft->SetCurrentTime(-animLeft->m_BlendHier->m_fTotalTime * rideData->AnimLeanLeft);
            animLeft->m_Flags &= ~(uint16)ANIMATION_WALK;
            animRight->m_BlendAmount = 0.0f;
        } else {
            animRight->m_BlendAmount = blendFwd * leanTarget;
            animRight->SetCurrentTime(animRight->m_BlendHier->m_fTotalTime * rideData->AnimLeanLeft);
            animRight->m_Flags &= ~(uint16)ANIMATION_WALK;
            animLeft->m_BlendAmount = 0.0f;
        }
    }

    //> Тряска торса на скорости (кватернион из rider+0x490; ABI: CPed+0x490/+0x470 в репо не смэплены)
    if (0.3f /* 0x858C24 */ < fwdSpeed) {
        const auto quat = reinterpret_cast<RtQuat*>(*reinterpret_cast<uint8**>(reinterpret_cast<uint8*>(rider) + 0x490) + 0x10); // ABI
        const auto angle  = 6.0f /* 0x871338 */ * fwdSpeed;
        const auto rndAng = angle * 2.0f * ((float)rand() * (1.0f / 32768.0f) /* 0x858C7C */) - angle;
        const RwV3d axisX{ 1.0f, 0.0f, 0.0f }; // 0x8D232C
        const RwV3d axisY{ 0.0f, 1.0f, 0.0f }; // 0x8D2338
        RtQuatRotate(quat, &axisX, rndAng, rwCOMBINEREPLACE);
        RtQuatRotate(quat, &axisY, rndAng, rwCOMBINEREPLACE);
        *reinterpret_cast<uint32*>(reinterpret_cast<uint8*>(rider) + 0x470) |= 0x4000; // ABI: флаг CPed
    }
}

bool CBike::BurstTyre(uint8 tyreComponentId, bool bPhysicalEffect) {
    if (vehicleFlags.bTyresDontBurst)
        return false;

    if (physicalFlags.bRenderScorched)
        return false;

    const auto wheel = [&]() -> eCarWheel {
        switch (tyreComponentId) {
        case CAR_PIECE_WHEEL_LF: return CAR_WHEEL_FRONT_LEFT;
        case CAR_PIECE_WHEEL_RL: return CAR_WHEEL_REAR_LEFT;
        default: NOTSA_UNREACHABLE();
        }
    }();

    const auto burst = m_nWheelStatus[wheel] == WHEEL_STATUS_OK;

    if (burst) {
        m_nWheelStatus[wheel] = WHEEL_STATUS_BURST;
        m_vehicleAudio.AddAudioEvent(AE_TYRE_BURST, 0.0f);

        if (GetStatus() == STATUS_SIMPLE) {
            CCarCtrl::SwitchVehicleToRealPhysics(this);
        }

        // 0x8D322C
        constexpr auto force = 0.02f;

        if (bPhysicalEffect) {
            ApplyMoveForce(m_matrix->GetRight() * CGeneral::GetRandomNumberInRange(-force, force) * m_fMass);
            ApplyTurnForce(
                m_matrix->GetRight() * CGeneral::GetRandomNumberInRange(-force, force) * m_fTurnMass,
                m_matrix->GetForward()
            );
        }
    }

    if ([&] {
        if (!m_pDriver) {
            return false;
        }

        const auto WereWheelsInAir = [&] (size_t wheelA, size_t wheelB) {
            return m_aRatioHistory[wheelA] >= 1.0 && m_aRatioHistory[wheelB] >= 1.0;
        };

        switch (tyreComponentId) {
            case CAR_PIECE_WHEEL_LF: {
                if (WereWheelsInAir(0, 1)) {
                    return false;
                }
                break;
            }
            case CAR_PIECE_WHEEL_RF: {
                if (WereWheelsInAir(2, 3)) {
                    return false;
                }
                break;
            }
            default: NOTSA_UNREACHABLE();
        }

        auto speed = m_vecMoveSpeed.Magnitude();

        if (speed <= 0.3f) {
            return false;
        }

        if (GetStatus() == STATUS_PLAYER && speed <= 0.55f) {
            return false;
        }

        return true;
    }()) {
        if (tyreComponentId == CAR_PIECE_WHEEL_LF) {
            const auto DoKnockOffEvent = [&] (CPed* ped, bool isDriver) {
                CEventKnockOffBike event{ this,
                    m_vecMoveSpeed,
                    m_vecLastCollisionImpactVelocity,
                    0.f,
                    0.f,
                    0x31u, // KNOCK_OFF_TYPE_SKIDBACKFRONT
                    0,
                    0,
                    nullptr,
                    isDriver,
                    false };
                ped->GetEventGroup().Add(&event);
            };

            DoKnockOffEvent(m_pDriver, true);

            if (m_apPassengers[0]) {
                DoKnockOffEvent(m_apPassengers[0], false);
            }
        }
        else {
            ApplyTurnForce(GetRight() * m_fTurnMass * 0.02f * 2.f, GetForward());
        }
    }

    return burst;
}

// ---------------------------------------------------------------------------
// 0x6BEA10 — replaces stub at Bike.cpp:583-585
// ---------------------------------------------------------------------------

// 0x6BE310
void CBike::ProcessControlInputs(uint8 playerNum) {
    const auto plyrpad = CPad::GetPad((int32)playerNum);
    const auto fwdVel  = GetMoveSpeed().Dot(GetForward());

    //> Ручник (0x6BE349): выход из тачки => ручник, иначе кнопка ручника
    vehicleFlags.bIsHandbrakeOn = plyrpad->GetExitVehicle() != 0 || plyrpad->GetHandBrake() != 0;

    //> Руль: клавиатура vs мышь (0x6BE3C4)
    {
        const auto KeyboardSteer = [&] {
            m_fRawSteerAngle = ((float)(-(int16)plyrpad->GetSteeringLeftRight()) / 128.0f /* 0x858B88 */ - m_fRawSteerAngle)
                * CTimer::GetTimeStep() * 0.2f /* 0x871300 */ + m_fRawSteerAngle;
            m_f2ndSteerAngle = ((float)(-(int16)plyrpad->GetSteeringUpDown()) / 128.0f - m_f2ndSteerAngle)
                * CTimer::GetTimeStep() * 0.2f + m_f2ndSteerAngle;
        };

        if (!TheCamera.m_bUseMouse3rdPerson || !m_bEnableMouseSteering) {
            KeyboardSteer();
        } else {
            const auto& mouse = CPad::NewMouseControllerState;
            if (mouse.m_AmountMoved.x == 0.0f && mouse.m_AmountMoved.y == 0.0f) {
                // Мышь не двигалась: если рулим мышью и клавиши не тронуты - продолжаем гасить руль
                if (std::abs(m_fRawSteerAngle) > 0.0f && m_nLastControlInput == eControllerType::MOUSE
                    && plyrpad->GetSteeringLeftRight() == 0 && plyrpad->GetSteeringUpDown() == 0) {
                    m_nLastControlInput = eControllerType::MOUSE; // 0x6BE4E8
                } else {
                    m_nLastControlInput = eControllerType::KEYBOARD;
                    KeyboardSteer();
                }
            } else {
                m_nLastControlInput = eControllerType::MOUSE;
            }

            if (m_nLastControlInput == eControllerType::MOUSE) {
                if (!plyrpad->NewState.m_bVehicleMouseLook) {
                    m_fRawSteerAngle += mouse.m_AmountMoved.x * -0.0035f /* 0x871304 */;
                    m_f2ndSteerAngle += mouse.m_AmountMoved.y * -0.0035f;
                }
                if (std::abs(m_fRawSteerAngle) < 0.35f /* 0x871308 */ || plyrpad->NewState.m_bVehicleMouseLook) {
                    m_fRawSteerAngle *= std::pow(0.98f /* 0x87130C */, CTimer::GetTimeStep());
                }
                if (std::abs(m_f2ndSteerAngle) < 0.35f || plyrpad->NewState.m_bVehicleMouseLook) {
                    m_f2ndSteerAngle *= std::pow(0.98f, CTimer::GetTimeStep());
                }
            }
        }
    }

    //> Кламп руля [-1, 1] (0x6BE655)
    m_fRawSteerAngle = std::clamp(m_fRawSteerAngle, -1.0f, 1.0f);
    m_f2ndSteerAngle = std::clamp(m_f2ndSteerAngle, -1.0f, 1.0f);

    //> Газ/тормоз (0x6BE872-0x6BE8xx)
    const auto pedalInput = (float)(plyrpad->GetAccelerate() - plyrpad->GetBrake()) * (1.0f / 255.0f) /* 0x859A3C */;
    if (std::abs(fwdVel) >= 0.01f /* 0x858C58 */) {
        if (fwdVel < 0.0f) {
            if (pedalInput >= 0.0f) { // стоим задом, жмём вперёд - тормозим
                m_BrakePedal = pedalInput;
            } else {
                m_GasPedal = pedalInput;
            }
        } else {
            if (pedalInput < 0.0f) { // едем вперёд, жмём назад - тормозим
                m_BrakePedal = -pedalInput;
                m_GasPedal   = 0.0f;
            } else {
                m_GasPedal = pedalInput;
            }
        }
    } else {
        // Стоим: burnout при газ+тормоз > 150 (кроме BMX)
        // [PORT-ADAPT #8] m_GasPedal/m_BrakePedal/m_nVehicleType - имена репо на хвосте CVehicle
        if (plyrpad->GetAccelerate() > 150.0f /* 0x858A28 */ && plyrpad->GetBrake() > 150.0f
            && m_nVehicleType != VEHICLE_TYPE_BMX
        ) {
            m_GasPedal   = (float)plyrpad->GetAccelerate() * (1.0f / 255.0f);
            m_nBrakesOn  = (uint16)true;
            m_BrakePedal = (float)plyrpad->GetBrake() * (1.0f / 255.0f);
        } else {
            m_GasPedal   = pedalInput;
            m_BrakePedal = 0.0f;
        }
    }

    //> Угол поворота (только при реальном управлении, не при плейбеке записи) (0x6BE872+)
    if (m_autoPilot.m_vehicleRecordingId < 0 || CVehicleRecording::bUseCarAI[m_autoPilot.m_vehicleRecordingId]) {
        m_fSteerAngle = m_pHandlingData->m_fSteeringLock * DegreesToRadians(1.0f) /* 0x8595EC */
            * (m_fRawSteerAngle * m_fRawSteerAngle) * (m_fRawSteerAngle < 0.0f ? -1.0f : 1.0f);
    }

    //> Комедийные контролы (0x6BE8xx)
    if (vehicleFlags.bComedyControls) {
        if ((CTimer::m_snTimeInMilliseconds & 0x3C00) < 0x3000) {
            m_BrakePedal = 1.0f;
        }
        if ((((CTimer::m_snTimeInMilliseconds >> 10) + 6) & 0xF) < 0xC) {
            m_GasPedal = 0.0f;
        }
        vehicleFlags.bIsHandbrakeOn = false;
        m_fSteerAngle += (CTimer::m_snTimeInMilliseconds & 0x800) == 0 ? -0.03f /* 0x858B10 */ : 0.08f /* 0x859018 */;
    }

    //> Скип катсцены: стоим, скорость гасится (0x6BEA20-...)
    const auto pad0 = CPad::GetPad(0);
    if (pad0->DisablePlayerControls && CGameLogic::SkipState != eSkipState::SKIP_IN_PROGRESS) {
        m_BrakePedal = 1.0f;
        m_GasPedal   = 0.0f;
        vehicleFlags.bIsHandbrakeOn = true;
        FindPlayerPed()->KeepAreaAroundPlayerClear();
        if (const auto speed = GetMoveSpeed().Magnitude(); speed > 0.28f /* 0x871254 */) {
            GetMoveSpeed() *= 0.28f / speed;
        }
    }
}

// 0x6BDEA0
int32 CBike::ProcessEntityCollision(CEntity* entity, CColPoint* outColPoints) {
    if (GetStatus() != STATUS_SIMPLE) {
        vehicleFlags.bVehicleColProcessed = true;
    }

    const auto tcd = GetColData(),
               ocd = entity->GetColData();

#ifdef FIX_BUGS // Text search for `FIX_BUGS@CAutomobile::ProcessEntityCollision:1`
    if (!tcd || !ocd) {
        return 0;
    }
#endif

    if (physicalFlags.bSkipLineCol || physicalFlags.bProcessingShift || entity->GetIsTypePed()) {
        tcd->m_nNumLines = 0; // Later reset back to original value
    }

    const auto ogWheelRatios = m_aWheelRatios;

    auto numColPts = CCollision::ProcessColModels(
        GetMatrix(), *GetColModel(),
        entity->GetMatrix(), *entity->GetColModel(),
        *(std::array<CColPoint, 32>*)(outColPoints),
        m_aWheelColPoints.data(),
        m_aWheelRatios.data(),
        false
    );

    // Possibly add driver & entity collisions to `outColPoints`
    if (m_pDriver && m_nTestPedCollision) {
        const auto pcd = m_pDriver->GetColData();
        if (!pcd->m_nNumLines) {
            std::array<CColPoint, 32> pedCPs{};

            CMatrix driverMat = GetMatrix();
            driverMat.GetPosition() += GetDriverSeatDummyPositionWS();

            std::array<CColPoint, 32> pedEntityColPts{};
            const auto numPedEntityColPts = CCollision::ProcessColModels(
                driverMat, *m_pDriver->GetColModel(),
                entity->GetMatrix(), *entity->GetColModel(),
                pedEntityColPts,
                nullptr,
                nullptr,
                false
            );

            if (numPedEntityColPts) {
                if (m_nTestPedCollision == 1) {
                    m_nTestPedCollision = 0;
                } else {
                    for (auto i = 0; i < numPedEntityColPts && numColPts < 32; i++) {
                        const auto& pedEntityCP = pedCPs[i];
                        if (pedEntityCP.m_nPieceTypeA == PED_COL_SPHERE_LEG) {
                            continue;
                        }
                        outColPoints[numColPts++] = pedEntityCP;
                    }
                }
            }
        }
    }

    size_t numProcessedLines{};
    if (tcd->m_nNumLines) {
        // Process the real wheels
        for (auto i = 0; i < NUM_SUSP_LINES; i++) {
            const auto& cp = m_aWheelColPoints[i];

            const auto wheelColPtsTouchDist = m_aWheelRatios[i];
            if (wheelColPtsTouchDist >= 1.f || wheelColPtsTouchDist >= ogWheelRatios[i]) {
                continue;
            }

            numProcessedLines++;

            m_anCollisionLighting[i] = cp.m_nLightingB;
            m_nContactSurface = cp.m_nSurfaceTypeB;

            switch (entity->GetType()) {
            case ENTITY_TYPE_VEHICLE:
            case ENTITY_TYPE_OBJECT: {
                CEntity::ChangeEntityReference(m_aGroundPhysicalPtrs[i], entity->AsPhysical());

                m_aGroundOffsets[i] = cp.m_vecPoint - entity->GetPosition();
                if (entity->GetIsTypeVehicle()) {
                    m_anCollisionLighting[i] = entity->AsVehicle()->m_anCollisionLighting[i];
                }
                break;
            }
            case ENTITY_TYPE_BUILDING: {
                m_pEntityWeAreOn = entity;
                m_bTunnel = entity->m_bTunnel;
                m_bTunnelTransition = entity->m_bTunnelTransition;
                break;
            }
            }
        }
    } else {
        tcd->m_nNumLines = NUM_SUSP_LINES;
    }

    if (numColPts > 0 || numProcessedLines > 0) {
        AddCollisionRecord(entity);
        if (!entity->GetIsTypeBuilding()) {
            entity->AsPhysical()->AddCollisionRecord(this);
        }
        if (numColPts > 0) {
            if (   entity->GetIsTypeBuilding()
                || (entity->GetIsTypeObject() && entity->AsPhysical()->physicalFlags.bDisableCollisionForce)
            ) {
                SetHasHitWall(true);
            }
        }
    }

    return numColPts;
}

// 0x6B9250
void CBike::ProcessControl() {
    plugin::CallMethod<0x6B9250, CBike*>(this);
}

// 0x6B6740
void CBike::ResetSuspension() {
    for (auto& pitchAngle : m_aWheelPitchAngles) {
        pitchAngle = 0.0f;
    }
    for (auto& ratio : m_aWheelRatios) {
        ratio = 1.0f;
    }
    for (auto& wheelCount : m_WheelCounts) {
        wheelCount = 0.0f;
    }
    for (auto& wheelState : m_WheelStates) {
        wheelState = WHEEL_STATE_NORMAL;
    }
}

// 0x6B6790
bool CBike::GetAllWheelsOffGround() const {
    return m_nNoOfContactWheels == 0;
}

// 0x6B67A0
void CBike::DebugCode() {
    // NOP
}

// 0x6B6D40
void CBike::DoSoftGroundResistance(uint32& arg0) {
    // Ищем колесо, стоящее на песке (ratio < 1 => колесо в контакте)
    const auto softGroundWheelIdx = [this]() -> int32 {
        for (auto i = 0; i < NUM_SUSP_LINES; i++) {
            if (m_aWheelRatios[i] < 1.0f && g_surfaceInfos.GetAdhesionGroup(m_aWheelColPoints[i].m_nSurfaceTypeB) == eAdhesionGroup::ADHESION_GROUP_SAND) {
                return i;
            }
        }
        return -1;
    }();

    if (softGroundWheelIdx != -1) {
        // Убираем вертикальную составляющую скорости
        const auto upComp = m_vecMoveSpeed.Dot(GetUp());
        const auto vel    = m_vecMoveSpeed - GetUp() * upComp;

        // [PORT-ADAPT, смежный с #2] m_GasPedal - поле CVehicle (ABI: оригинал читает 0x49C)
        if (m_GasPedal > 0.3f) { // 0x858C24
            if (vel.SquaredMagnitude() < sq(0.3f)) { // 0x871320
                arg0 += 4;
            }
            // Убираем ещё и продольную составляющую - газ на песке "закапывает" байк
            const auto fwdComp = vel.Dot(GetForward());
            ApplyMoveForce(-(CTimer::GetTimeStep() * m_fMass * 0.02f /* 0x871330 */) * (vel - GetForward() * fwdComp));
        } else {
            ApplyMoveForce(-(CTimer::GetTimeStep() * m_fMass * 0.02f /* 0x871330 */) * vel);
        }
        return;
    }

    // Не песок: проверяем рельсы (SURFACE_RAILTRACK = 178 / 0xB2)
    for (auto i = 0; i < NUM_SUSP_LINES; i++) {
        if (m_aWheelRatios[i] < 1.0f && m_aWheelColPoints[i].m_nSurfaceTypeB == SURFACE_RAILTRACK) {
            const auto upComp = m_vecMoveSpeed.Dot(GetUp());
            const auto vel    = m_vecMoveSpeed - GetUp() * upComp;
            ApplyMoveForce(-(CTimer::GetTimeStep() * m_fMass * CVehicle::ms_fRailTrackResistance) * vel);
            break; // оригинал применяет силу один раз
        }
    }
}

// 0x6B7130
void CBike::PlayHornIfNecessary() {
    if (m_autoPilot.carCtrlFlags.bHonkAtCar || m_autoPilot.carCtrlFlags.bHonkAtPed)
        PlayCarHorn();
}

// 0x6B7150
void CBike::CalculateLeanMatrix() {
    if (m_bLeanMatrixCalculated)
        return;

    CMatrix mat;
    mat.SetRotateX(fabs(m_RideAnimData.LeanAngle) * -0.05f);
    mat.RotateY(m_RideAnimData.LeanAngle);
    m_mLeanMatrix = GetMatrix();
    m_mLeanMatrix = m_mLeanMatrix * mat;
    // place wheel back on ground
    m_mLeanMatrix.GetPosition() += GetUp() * (1.0f - cos(m_RideAnimData.LeanAngle)) * GetColModel()->GetBoundingBox().m_vecMin.z;
    m_bLeanMatrixCalculated = true;
}

// 0x6B7F90
void CBike::FixHandsToBars(CPed* rider) {
    if (!m_nFixRightHand && !m_nFixLeftHand) {
        return;
    }

    const auto chassis = m_aBikeNodes[BIKE_CHASSIS];
    if (!chassis) {
        return;
    }

    // Матрица шасси (modelling) + мировая матрица байка
    CMatrix mat{};
    mat.Attach(RwFrameGetMatrix(chassis), false);
    mat = GetMatrix();

    const auto hierarchy = GetAnimHierarchyFromSkinClump(rider->GetRpClump()); // rider+0x18 = RwObject

    // [PORT-ADAPT #9] Позиция руля из структуры модели (ABI: mi+0x5C -> CVehicleModelInfo::CVehicleStructure,
    // float'ы @+0x78/0x7C/0x80 = m_avDummyPos[DUMMY_HAND_REST], т.к. 10*0xC = 0x78)
    const auto vehStruct = CModelInfo::GetModelInfo(m_nModelIndex)->AsVehicleModelInfoPtr()->GetVehicleStruct();
    auto barsOffset = vehStruct->m_avDummyPos[DUMMY_HAND_REST];

    if (barsOffset == CVector{}) {
        // Нет данных в модели - хардкод по индексу модели
        if (m_nModelIndex == 0x1FE /* 510 */) {
            barsOffset = CVector{ 0.25f, 0.29f, 0.525f };   // 0x8D3250
        } else if (m_nModelIndex == 0x1FD /* 509 */) {
            barsOffset = CVector{ 0.25f, 0.21f, 0.69f };    // 0x8D325C
        } else {
            barsOffset = CVector{ 0.25f, 0.29f, 0.525f };   // 0x8D3244
        }
    } else {
        // Офсет из кости 24: boneRot * (-0.075, -0.03, 0), поворот на матрицу байка
        const auto mats   = RpHAnimHierarchyGetMatrixArray(hierarchy);
        const auto bone24 = RpHAnimIDGetIndex(hierarchy, 0x18);
        CVector v{ -0.075f /* 0x8D3274 */, -0.03f /* 0x8D3278 */, 0.0f /* 0x8D327C */ };
        CMatrix boneMat{};
        boneMat.Attach(mats + bone24, false);
        v = boneMat.TransformVector(v);
        v = mat.TransformVector(v);
        barsOffset += v;
    }

    const auto MoveHands = [&](std::initializer_list<int> snapBones, int armNearBone, float armNearMult, int armFarBone, float armFarMult, int playerBone, float playerBoneMult, float xSign) {
        auto target = barsOffset;
        target.x *= xSign;
        target = mat.TransformVector(target); // ABI: вращение офсета (вероятно m_mLeanMatrix) - см. w2-01 STATUS
        const auto mats = RpHAnimHierarchyGetMatrixArray(hierarchy);
        auto ApplyDelta = [&](int boneId, float mult) {
            const auto idx = RpHAnimIDGetIndex(hierarchy, boneId);
            auto&     m    = mats[idx];
            const CVector delta{ (target - CVector{ m.right.x, m.right.y, m.right.z }) * mult };
            m.right.x += delta.x;
            m.right.y += delta.y;
            m.right.z += delta.z;
        };
        for (const auto b : snapBones) {
            ApplyDelta(b, 1.0f);
        }
        ApplyDelta(armNearBone, armNearMult);
        ApplyDelta(armFarBone, armFarMult);
        if (rider->IsPlayer()) {
            ApplyDelta(playerBone, playerBoneMult);
        }
    };

    if (m_nFixRightHand) {
        MoveHands({ 0x18, 0x19, 0x1A }, 0x17, 0.667f /* 0x871488 */, 0x16, 0.333f /* 0x864E30 */, 0x12D, 0.333f, 1.0f);
    }
    if (m_nFixLeftHand) {
        MoveHands({ 0x22, 0x23, 0x24 }, 0x21, 0.75f /* 0x858F34 */, 0x20, 0.4f /* 0x858EE8 */, 0x12E, 0.4f, -1.0f);
    }

    m_nFixLeftHand  = 0; // veh+0x7B4
    m_nFixRightHand = 0; // veh+0x7B5
}

// 0x6BEEB0
void CBike::PlaceOnRoadProperly() {
    const auto& cm      = *GetColModel();
    const auto  fFrontY = cm.m_boundBox.m_vecMax.y;
    const auto  fRearY  = -cm.m_boundBox.m_vecMin.y;

    const auto& vecPos = GetPosition();

    auto vecRearCheck  = vecPos - GetForward() * fRearY;
    vecRearCheck.z     = vecPos.z;

    auto vecFrontCheck = vecPos + GetForward() * fFrontY;
    vecFrontCheck.z    = vecPos.z;

    //> Фронт
    {
        CColPoint cp{};
        CEntity*  hitEnt{};
        if (CWorld::ProcessVerticalLine(vecFrontCheck, vecFrontCheck.z + 5.0f, cp, hitEnt, true)) {
            m_bTunnel           = hitEnt->m_bTunnel;
            m_bTunnelTransition = hitEnt->m_bTunnelTransition;
            m_pEntityWeAreOn    = hitEnt;
            m_FrontCollPoly.ligthing = cp.m_nLightingB; // оригинал пишет байт по poly+0x24 (в репо-порядке CStoredCollPoly это поле `ligthing`, см. STATUS w2-01/02)
            vecFrontCheck.z     = cp.m_vecPoint.z;
        } else {
            vecFrontCheck.z = vecPos.z;
        }
    }

    //> Тыл
    {
        CColPoint cp{};
        CEntity*  hitEnt{};
        if (CWorld::ProcessVerticalLine(vecRearCheck, vecRearCheck.z + 5.0f, cp, hitEnt, true)) {
            m_bTunnel           = hitEnt->m_bTunnel;
            m_bTunnelTransition = hitEnt->m_bTunnelTransition;
            m_pEntityWeAreOn    = hitEnt;
            m_RearCollPoly.ligthing  = cp.m_nLightingB; // см. выше
            vecRearCheck.z      = cp.m_vecPoint.z;
        } else {
            vecRearCheck.z = vecPos.z;
        }
    }

    const auto fLength = fFrontY + fRearY;

    // right = нормализованная поперечная проекция
    GetRight().Set((vecFrontCheck.y - vecRearCheck.y) / fLength, -((vecFrontCheck.x - vecRearCheck.x) / fLength), 0.0f);

    // Наклон дороги: pitch = atan(dz/len) -> forward = (-cos(pitch)*right.y, cos(pitch)*right.x, sin(pitch))
    const auto pitch = std::atan((vecFrontCheck.z - vecRearCheck.z) / fLength);
    GetForward().x   = -std::cos(pitch) * GetRight().y;
    GetForward().y   =  std::cos(pitch) * GetRight().x;
    GetForward().z   =  std::sin(pitch);

    GetUp() = CrossProduct(GetRight(), GetForward());

    // Позиция: середина между фронт/тыл-точками + высота над дорогой
    SetPosn((vecFrontCheck.x + vecRearCheck.x) * 0.5f,
            (vecFrontCheck.y + vecRearCheck.y) * 0.5f,
            GetHeightAboveRoad() + (vecFrontCheck.z + vecRearCheck.z) * 0.5f);
}

// 0x6BF230
void CBike::GetCorrectedWorldDoorPosition(CVector& out, CVector arg1, CVector arg2) {
    // "Правый"-вектор: CrossProduct(fwd, {0,0,1}) - горизонтальная проекция right
    const auto rightish = CrossProduct(GetForward(), CVector{ 0.0f, 0.0f, 1.0f });
    // "Верхний"-вектор: CrossProduct(rightish, fwd) = up
    const auto upish    = CrossProduct(rightish, GetForward());

    // Поправка на геометрию байка (оригинал: colModel+0xC/+0x14 => m_boundBox.m_vecMax.x/y)
    const auto& bb    = GetColModel()->m_boundBox;
    auto        width = 0.0f;
    if (bb.m_vecMax.x < bb.m_vecMax.y) {
        width = bb.m_vecMax.y - bb.m_vecMax.x;
    }

    out = CVector{};

    // y-смещение вдоль forward
    out += GetForward() * (arg2.y - arg1.y);

    // x-смещение: ширина * наклон байка (rightish . up)
    out += rightish * (width * rightish.Dot(GetUp()));

    // z-смещение вдоль upish
    out += upish * (arg2.z - arg1.z);

    out += GetPosition();
}

void CBike::BlowUpCar(CEntity* damager, bool bHideExplosion) {
    if (!vehicleFlags.bCanBeDamaged) {
        return;
    }

    m_vecMoveSpeed.z += 0.13f;
    SetStatus(STATUS_WRECKED);
    physicalFlags.bRenderScorched = true;
    // [PORT-ADAPT] PR writes `this->m_pRwClump` — member does not exist in this
    // repo (CEntity has m_pRwObject + GetRpClump(), Entity.h:51/309). Same call
    // as repo's own CAutomobile::BlowUpCar_Impl (Automobile.cpp:2338).
    CVisibilityPlugins::SetClumpForAllAtomicsFlag(GetRpClump(), eAtomicComponentFlag::ATOMIC_PIPE_NO_EXTRA_PASSES);
    m_fHealth = 0.0f;
    m_wBombTimer = 0;

    TheCamera.CamShake(0.4f, GetPosition());
    KillPedsInVehicle();

    m_nOverrideLights = eVehicleOverrideLightsState::NO_CAR_LIGHT_OVERRIDE;
    vehicleFlags.bEngineOn = false;
    vehicleFlags.bLightsOn = false;
    ChangeLawEnforcerState(false);

    CExplosion::AddExplosion(this, damager, eExplosionType::EXPLOSION_CAR, GetPosition(), 0, 1, -1.0F, bHideExplosion);
    CDarkel::RegisterCarBlownUpByPlayer(*this, 0);
}

// ---------------------------------------------------------------------------
// 0x6B8EC0 — replaces stub at Bike.cpp:633-635
// ---------------------------------------------------------------------------

// 0x6B7050
void CBike::Fix() {
    vehicleFlags.bIsDamaged = false;
    bikeFlags.bEngineOnFire = false;
    m_nWheelStatus[0] = 0;
    m_nWheelStatus[1] = 0;
}

// 0x6BD090
void CBike::PreRender() {
    CVehicle::PreRender();

    const auto cm  = GetColModel();
    const auto cd  = cm->m_pColData;
    const auto vmi = CModelInfo::GetModelInfo(m_nModelIndex)->AsVehicleModelInfoPtr();

    //> Коп-байк: обновляем высоты подвески по сжатию (визуал)
    // [PORT-ADAPT #5] ABI-байт 0x42B&1 => bCreatedAsPoliceVehicle (Vehicle.h:288; семантика по w2-01 STATUS)
    if (vehicleFlags.bCreatedAsPoliceVehicle) {
        DoBurstAndSoftGroundRatios();
        {
            const auto frontCompression = 1.0f - m_fSuspensionLength[0] / m_fLineLength[0];
            const auto frontRatio       = std::min(m_aRatioHistory[0], m_aRatioHistory[1]);
            auto       targetZ          = cd->m_pLines[0].m_vecStart.z;
            if (frontRatio > 0.0f) {
                targetZ -= frontRatio * frontCompression;
            }
            m_aWheelSuspensionHeights[0] += (targetZ - m_aWheelSuspensionHeights[0]) * 0.75f /* 0x858F34 */;

            const auto rearCompression = 1.0f - m_fSuspensionLength[1] / m_fLineLength[1];
            const auto rearRatio       = std::min(m_aRatioHistory[2], m_aRatioHistory[3]);
            targetZ = cd->m_pLines[2].m_vecStart.z;
            if (rearRatio > 0.0f) {
                targetZ -= rearRatio * rearCompression;
            }
            m_aWheelSuspensionHeights[1] += (targetZ - m_aWheelSuspensionHeights[1]) * 0.75f;
        }
    }

    //> Частицы колёс / скиды
    if (const auto status = GetStatus(); status == STATUS_PHYSICS || status == STATUS_PLAYER || status == STATUS_PLAYER_PLAYBACK_FROM_BUFFER || status == STATUS_SIMPLE) {
        const auto speed = m_vecMoveSpeed.Magnitude();
        for (auto i = 0; i < 2; i++) {
            auto wheelIdx = 0;
            if (i == 0) {
                if (m_aRatioHistory[0] >= 1.0f && m_aRatioHistory[1] < 1.0f) {
                    wheelIdx = 1;
                }
            } else {
                wheelIdx = 3;
                if (m_aRatioHistory[3] >= 1.0f && m_aRatioHistory[2] < 1.0f) {
                    wheelIdx = 2;
                }
            }

            auto extraBits = 0;
            if (i == 0 && m_WheelStates[1] != WHEEL_STATE_SKIDDING /* tWheelState(2) */) {
                extraBits = 4;
            }
            if (m_bWheelBloody[i]) {
                extraBits += 1;
            }
            if (m_bMoreSkidMarks[i]) {
                extraBits += 2;
            }

            const auto leanAngle = m_RideAnimData.LeanAngle;
            // AddSingleWheelParticles принимает CVector* (не const) - skidPos не const
            auto skidPos = m_aWheelColPoints[wheelIdx].m_vecPoint
                                 + GetRight() * (std::sin(leanAngle) * cm->m_boundBox.m_vecMin.z * 0.8f /* 0x858C98 */);

            AddSingleWheelParticles(
                m_WheelStates[i],
                m_nWheelStatus[i],
                m_aRatioHistory[wheelIdx],
                speed,
                &m_aWheelColPoints[wheelIdx],
                &skidPos,
                leanAngle <= 0.0f ? 1.0f : -1.0f,
                i,
                static_cast<uint32>(m_aWheelSkidmarkType[i]), // [PORT-ADAPT #7] eSkidmarkType (enum class) -> uint32
                &m_bWheelBloody[i],
                static_cast<uint32>(extraBits)
            );
        }
    }

    //> Матрица наклона, выхлоп, дым повреждений
    m_bLeanMatrixCalculated = false;
    CalculateLeanMatrix();

    // [PORT-ADAPT #6] ABI-байт 0x42B&0x40 => !bIsDrowning; 0x1000 => VEHICLE_HANDLING_MODEL_NO_EXHAUST;
    // bss 0xC1C81C поднят в file-scope StaticRef (см. шапку файла)
    if (vehicleFlags.bEngineOn
        && (m_pHandlingData->m_nModelFlags & VEHICLE_HANDLING_MODEL_NO_EXHAUST) == 0
        && GetForward().Dot(m_vecMoveSpeed) / s_fUnkBikeSpeedDivisor < 130.0f /* 0x871250 */
        && !vehicleFlags.bIsDrowning) {
        AddExhaustParticles();
    }

    AddDamagedVehicleParticles();

    //> Коп-байк: мигалка (корона + точечный свет)
    // [PORT-ADAPT #7] ABI-байт 0x42D<0 (не идентифицирован, w2-01 STATUS) оставлен офсетным чтением;
    // m_nPhysicalFlags&0x20000000 => physicalFlags.bRenderScorched (bit 29)
    if (m_nModelIndex == MODEL_COPBIKE /* 523 = 0x20B */
        && reinterpret_cast<const int8*>(&m_nVehicleLowerFlags)[1] < 0 /* byte 0x42D < 0 */
        && vehicleFlags.bEngineOn
        && !physicalFlags.bRenderScorched) {
        const auto  phase    = static_cast<uint8>(CTimer::m_snTimeInMilliseconds & 0x1FF);
        const bool  first    = phase < 0x100;
        const auto  id       = first ? (uint32)phase : (uint32)(uint8)-(int8)phase;
        const auto& m        = GetMatrix();
        const auto  lightPos = m.GetPosition() + m.GetUp() * (m.GetUp().z * 0.5f);
        CCoronas::RegisterCorona(id, this, 0, 0, 0, 255, lightPos, 0.28f, 40.0f /* 0x42200000 */,
            eCoronaType::CORONATYPE_HEADLIGHT, eCoronaFlareType::FLARETYPE_NONE,
            eCoronaReflType::CORREFL_NONE, eCoronaLOSCheck::LOSCHECK_ON, eCoronaTrail::TRAIL_OFF,
            0.4f /* 0x3ECCCCCD */, false, 1.5f /* 0x3FC00000 */, false, 30.0f /* 0x41F00000 */, false, false); // 0x6FC580
        // [PORT-ADAPT #7] аргументы выровнены по сигнатуре репо: (type, pos, dir, radius, r, g, b, fog, extraShadows)
        CPointLights::AddLight(0, lightPos, CVector{}, 10.0f /* 0x41200000 radius */, first ? 1.0f : 0.0f, 0.0f, first ? 0.0f : 1.0f,
            0, true); // 0x7000E0 (параметры - приближение w2-01, отмечено в STATUS)
    }

    DoVehicleLights(m_mLeanMatrix, VEHICLE_LIGHTS_IGNORE_DAMAGE /* (eVehicleLightsFlags)4 */);

    CShadows::StoreShadowForVehicle(this, VEH_SHD_BIKE); // [PORT-ADAPT #7] сигнатура репо: (CVehicle*, VEH_SHD_TYPE)

    //> Вращение колёс
    // [PORT-ADAPT #7] Multiply3x3(GetMatrix(), v) => CMatrix::TransformVector (Matrix.h:167)
    const auto wheelDir = GetMatrix().TransformVector(CVector{ -std::sin(m_fSteerAngle), std::cos(m_fSteerAngle), 0.0f });
    if (m_GasPedal > 0.0f || m_BrakePedal > 0.0f) {
        const auto& line0 = cd->m_pLines[0];
        const auto  vel   = GetSpeed(CVector{
            (line0.m_vecStart.y + line0.m_vecEnd.y) * 0.5f,
            (line0.m_vecEnd.z - std::min(m_aRatioHistory[0], m_aRatioHistory[1]) * m_fSuspensionLength[0]) - vmi->m_fWheelSizeFront * 0.5f,
            0.0f });
        const auto rot = CVehicle::ProcessWheelRotation(WHEEL_STATE_NORMAL, wheelDir, vel, vmi->m_fWheelSizeFront * 0.5f);
        m_aWheelAngularVelocity[0] = rot;
        m_aWheelPitchAngles[0] += rot * CTimer::GetTimeStep();
    }
    if (m_WheelCounts[2] > 0.0f || m_WheelCounts[3] > 0.0f) {
        const auto& line2 = cd->m_pLines[2];
        const auto  vel   = GetSpeed(CVector{
            (line2.m_vecStart.y + line2.m_vecEnd.y) * 0.5f,
            (line2.m_vecEnd.z - std::min(m_aRatioHistory[2], m_aRatioHistory[3]) * m_fSuspensionLength[1]) - vmi->m_fWheelSizeRear * 0.5f,
            0.0f });
        const auto rot = CVehicle::ProcessWheelRotation(m_WheelStates[1], wheelDir, vel, vmi->m_fWheelSizeRear * 0.5f);
        m_aWheelAngularVelocity[1] = rot;
        m_aWheelPitchAngles[1] += rot * CTimer::GetTimeStep();
    }

    //> Узлы: переднее/заднее колесо (сдвиг подвески + тряска пробитого), вилки, шасси
    const auto AttachAndKeep = [](RwFrame* node) {
        CMatrix mat{};
        mat.Attach(RwFrameGetMatrix(node), false);
        return mat;
    };

    // Переднее колесо: сдвиг по X/Y на середину линии, Z - текущая высота - вилка
    if (m_aBikeNodes[BIKE_WHEEL_FRONT]) {
        auto mat = AttachAndKeep(m_aBikeNodes[BIKE_WHEEL_FRONT]);
        const auto& l1 = cd->m_pLines[1];
        const auto  zOff = m_aWheelSuspensionHeights[0] /* 0x760 */ - m_fForkZOffset /* 0x7A0 */;
        const auto  yOff = (l1.m_vecStart.y + l1.m_vecEnd.y) * 0.5f /* 0x858B8C */ - m_fForkYOffset /* 0x79C */
            - (m_aWheelSuspensionHeights[0] - m_aWheelOrigHeights[0]) * m_fSteerAngleTan /* 0x7A4 */;
        if (m_nWheelStatus[0] == WHEEL_STATUS_BURST) { // byte 0x65C == 1: тряска пробитого колеса
            mat.GetPosition() += CVector{ yOff, zOff, std::sin(m_aWheelPitchAngles[0]) * 0.05f /* 0x858C28 */ };
        } else {
            mat.SetRotateX(m_aWheelPitchAngles[0]);
            mat.GetPosition() += CVector{ yOff, zOff, 0.0f };
        }
        mat.UpdateRW();
    }

    // Вилки: угол качания заднего маятника
    if (m_aBikeNodes[BIKE_FORKS_REAR]) {
        auto  mat    = AttachAndKeep(m_aBikeNodes[BIKE_FORKS_REAR]);
        const auto angle = std::asin((m_aWheelSuspensionHeights[1] /* 0x764 */ - m_aWheelOrigHeights[1] /* 0x76C */) / m_fSwingArmLength /* 0x798 */)
            * -1.0f /* 0x858C1C */;
        mat.SetRotate(angle, 0.0f, 0.0f);
        mat.UpdateRW();
    }

    // Заднее колесо: тот же сдвиг
    if (m_aBikeNodes[BIKE_WHEEL_REAR]) {
        auto mat = AttachAndKeep(m_aBikeNodes[BIKE_WHEEL_REAR]);
        mat.GetPosition() += CVector{ 0.0f, 0.0f, 0.0f }; // Z-сдвиг уже внесён вилками (см. оригинал 0x6BD1F9)
        if (m_nWheelStatus[1] == WHEEL_STATUS_BURST) { // byte 0x65D == 1
            mat.SetRotate(m_aWheelPitchAngles[1], 0.0f, std::sin(m_aWheelPitchAngles[1]) * 0.07f /* 0x858CA8 */);
        } else {
            mat.SetRotateX(m_aWheelPitchAngles[1]);
        }
        mat.UpdateRW();
    }

    // Шасси: визуальный наклон байка (аналог CalculateLeanMatrix на ноде)
    if (m_aBikeNodes[BIKE_CHASSIS]) {
        auto mat = AttachAndKeep(m_aBikeNodes[BIKE_CHASSIS]);
        const auto lean = m_RideAnimData.LeanAngle;
        mat.SetTranslate(CVector{ 0.0f, 0.0f, (1.0f - std::cos(lean)) * cm->m_boundSphere.m_vecCenter.x /* +8 */ * 0.9f /* 0x858C20 */ });
        mat.SetRotateX(std::abs(lean) * -0.05f /* 0x871290 */);
        mat.RotateY(lean);
        mat.UpdateRW();
    }
}

// 0x6BDE20
void CBike::Render() {
    auto savedRef = 0;
    RwRenderStateGet(rwRENDERSTATEALPHATESTFUNCTIONREF, &savedRef);
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(1));

    m_nTimeTillWeNeedThisCar = CTimer::GetTimeInMS() + 3000;
    CVehicle::Render();

    if (m_renderLights.m_bRightFront) {
        CalculateLeanMatrix();
        CVehicle::DoHeadLightBeam(eVehicleLightId::MAIN, m_mLeanMatrix, true);
    }

    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(savedRef));
}

// 0x6BCFC0
void CBike::Teleport(CVector destination, bool resetRotation) {
    CWorld::Remove(this);

    GetPosition() = destination;
    if (resetRotation)
        SetOrientation(0.0f, 0.0f, 0.0f);

    ResetMoveSpeed();
    ResetTurnSpeed();
    ResetSuspension();

    CWorld::Add(this);
}

void CBike::VehicleDamage(float damageIntensity, eVehicleCollisionComponent component, CEntity* damager, CVector* vecCollisionCoors, CVector* vecCollisionDirection, eWeaponType weapon) {
    // inverted
    if (damageIntensity > 0.f || m_fDamageIntensity < 1.0f || !vehicleFlags.bCanBeDamaged)
        return;

    auto playerDamageIntensity = m_fDamageIntensity;

    if (GetStatus() == STATUS_PLAYER && CStats::GetPercentageProgress() >= 100.0f) {
        playerDamageIntensity *= 0.5f;
    }

    if (bikeFlags.bOnSideStand && playerDamageIntensity > 20.f) {
        bikeFlags.bOnSideStand = false;
    }

    DamageKnockOffRider(this, m_fDamageIntensity, m_nPieceType, m_pDamageEntity, m_vecLastCollisionPosn, m_vecLastCollisionImpactVelocity);

    if (m_pDamageEntity && m_pDamageEntity->GetType() == ENTITY_TYPE_VEHICLE) {
        m_nLastWeaponDamageType = eWeaponType::WEAPON_RAMMEDBYCAR;
        m_pLastDamageEntity = m_pDamageEntity;
        RegisterReference(m_pDamageEntity);
    }

    // inverted
    if (physicalFlags.bCollisionProof)
        return;

    // inverted
    if (m_pDamageEntity &&
        GetType() == eEntityType::ENTITY_TYPE_BUILDING &&
        DotProduct(m_vecLastCollisionImpactVelocity, m_matrix->GetUp()) > 0.6f)
        return;

    // inverted
    if (playerDamageIntensity <= 25.0 || GetStatus() == STATUS_WRECKED)
        return;

    if (!vehicleFlags.bIsLawEnforcer) {
        auto playerVehicle = FindPlayerVehicle();

        if (playerVehicle &&
            (CVehicle*)m_pDamageEntity == playerVehicle &&
            GetStatus() != STATUS_ABANDONED &&
            playerVehicle->m_vecMoveSpeed.Magnitude() >= m_vecMoveSpeed.Magnitude() &&
            playerVehicle->m_vecMoveSpeed.Magnitude() > 0.1) {
            // [PORT-ADAPT] PR writes `SetWantedLevelNoDrop(1)` — this repo's
            // signature takes eWantedLevel (PlayerPed.h:59); int does not
            // implicitly convert. Same pattern as Automobile.cpp:2945.
            FindPlayerPed()->SetWantedLevelNoDrop(eWantedLevel::WANTED_LEVEL_1);
        }
    }

    auto collisionDamageForce = (playerDamageIntensity - 25.0f) * m_pHandlingData->m_fCollisionDamageMultiplier;

    if (collisionDamageForce > 0.f) {
        auto damageVehicle = (CVehicle*)m_pDamageEntity;
        if (collisionDamageForce > 5.f &&
            m_pDriver &&
            m_pDamageEntity &&
            m_pDamageEntity->GetType() == ENTITY_TYPE_VEHICLE &&
            (FindPlayerVehicle() != this || damageVehicle->m_nCreatedBy != MISSION_VEHICLE) &&
            damageVehicle->m_pDriver) {
            m_pDriver->Say(CTX_GLOBAL_CRASH_BIKE, 0, 1.0, false, false, false);
        }

        auto vehicleDamageForce = collisionDamageForce;

        if (this == FindPlayerVehicle()) {
            if (vehicleFlags.bTakeLessDamage) {
                vehicleDamageForce *= 0.16f;
            } else {
                vehicleDamageForce *= 0.5f;
            }
        } else if (vehicleFlags.bTakeLessDamage) {
            vehicleDamageForce *= 0.083f;
        } else if (damageVehicle && damageVehicle == FindPlayerVehicle()) {
            vehicleDamageForce *= 0.6f;
        } else {
            vehicleDamageForce *= 0.25f;
        }

        auto currentHealth = m_fHealth;
        m_fHealth -= vehicleDamageForce;

        if (m_fHealth <= 1.0 && currentHealth > 1.0) {
            m_fHealth = 1.0f;
        }

        if (m_fHealth >= 250.f) {
            return;
        }

        if (!bikeFlags.bEngineOnFire) {
            bikeFlags.bEngineOnFire = true;
            m_BlowUpTimer = 0.f;
            m_Damager = m_pDamageEntity;

            if (m_pDamageEntity) {
                RegisterReference(m_pDamageEntity);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 0x6B7080 — replaces stub at Bike.cpp:655-657
// ---------------------------------------------------------------------------

// 0x6B89B0
void CBike::SetupSuspensionLines() {
    const auto& mi = *CModelInfo::GetModelInfo(m_nModelIndex)->AsVehicleModelInfoPtr();
          auto& cm = *mi.GetColModel();
          auto& cd = *cm.m_pColData;

    const auto linesAlreadySetup = cd.m_pLines[0].m_vecStart.z != 99999.99f;
    const auto firstTimeSetup    = cd.m_pLines[1].m_vecStart.z == 99999.99f;

    // Позиция ноды в координатах клампа (моделирующая матрица * родители до корня клампа)
    const auto NodePosUpToClump = [&](RwFrame* node) {
        CMatrix mat{ RwFrameGetMatrix(node), false };
        const auto clumpRoot = RwFrameGetParent(reinterpret_cast<RwFrame*>(GetRwObject()));
        for (auto parent = RwFrameGetParent(node); parent && parent != clumpRoot; parent = RwFrameGetParent(parent)) {
            mat = CMatrix{ RwFrameGetMatrix(parent), false } * mat;
        }
        return mat.GetPosition();
    };

    for (auto i = 0; i < 4; i++) {
        auto& colLine = cd.m_pLines[i];
        const auto  isFront = i < 2;
        const auto  size    = isFront ? mi.m_fWheelSizeFront : mi.m_fWheelSizeRear;

        if (linesAlreadySetup) {
            // Линии уже рассчитаны: X/Y берём из линий, Z - из текущей подвески
            const CVector startPos{
                colLine.m_vecStart.x,
                colLine.m_vecStart.y,
                (isFront ? m_aWheelOrigHeights[0] : m_aWheelOrigHeights[1]) + m_pHandlingData->m_fSuspensionUpperLimit
            };
            colLine.m_vecStart = startPos;
        } else {
            const auto  node    = isFront ? m_aBikeNodes[BIKE_WHEEL_FRONT] : m_aBikeNodes[BIKE_WHEEL_REAR];
            const float yOffset = i % 2 == 0 ? size * 0.25f /* 0x858C84 */ : size * -0.25f /* 0x85C654 */;

            const auto pos = NodePosUpToClump(node);

            if (i == 0) {
                m_aWheelOrigHeights[0] = pos.z; // z переднего колеса
            } else if (i == 2) {
                m_aWheelOrigHeights[1] = pos.z; // z заднего колеса
                if (const auto rearFork = m_aBikeNodes[BIKE_FORKS_REAR]) {
                    const auto forkPos = NodePosUpToClump(rearFork);
                    m_fSwingArmLength  = std::sqrt(sq(forkPos.y - pos.y) + sq(forkPos.z - pos.z));
                } else {
                    m_fSwingArmLength = 0.0f;
                }
            }

            colLine.m_vecStart = CVector{ pos.x, pos.y + yOffset, pos.z + m_pHandlingData->m_fSuspensionUpperLimit };
        }

        // Конец линии: start.z + (lower - upper) - радиус колеса; X/Y те же
        colLine.m_vecEnd   = colLine.m_vecStart;
        colLine.m_vecEnd.z = colLine.m_vecStart.z + (m_pHandlingData->m_fSuspensionLowerLimit - m_pHandlingData->m_fSuspensionUpperLimit) - size * 0.5f;

        m_fSuspensionLength[i] = m_pHandlingData->m_fSuspensionUpperLimit - m_pHandlingData->m_fSuspensionLowerLimit;
        m_fLineLength[i]       = colLine.m_vecEnd.z - colLine.m_vecStart.z;
    }

    // Смещение передней вилки (только при первой установке)
    if (!linesAlreadySetup) {
        if (const auto fork = m_aBikeNodes[BIKE_FORKS_FRONT]) {
            const auto forkPos = NodePosUpToClump(fork);
            m_fForkYOffset     = forkPos.y;
            m_fForkZOffset     = forkPos.z;
        }
    }

    // Высота над дорогой
    m_fHeightAboveRoad = mi.m_fWheelSizeFront * 0.5f - cd.m_pLines[0].m_vecStart.z
        + (1.0f - 1.0f / (m_pHandlingData->m_fSuspensionForceLevel * 4.0f)) * m_fSuspensionLength[0];

    for (auto i = 0; i < 2; i++) {
        m_aWheelSuspensionHeights[i] = (i == 0 ? mi.m_fWheelSizeFront : mi.m_fWheelSizeRear) * 0.5f - m_fHeightAboveRoad;
    }

    // Поправка bbox/sphere (аналог CAutomobile::SetupSuspensionLines)
    cm.m_boundBox.m_vecMin.z   = std::min(cm.m_boundBox.m_vecMin.z, cd.m_pLines[0].m_vecEnd.z);
    cm.m_boundSphere.m_fRadius = std::max({
        cm.m_boundSphere.m_fRadius,
        cm.m_boundBox.m_vecMin.Magnitude(),
        cm.m_boundBox.m_vecMax.Magnitude(),
        cm.m_boundSphere.m_vecCenter.Magnitude()
    });

    // При первой установке приподнимаем сферы (модели с флагом m_bForceGroundClearance)
    if ((m_pHandlingData->m_nModelFlags & VEHICLE_HANDLING_MODEL_FORCE_GROUND_CLEARANCE /* 0x40000000 */) != 0 && firstTimeSetup) {
        const auto sphBottomMinZ = 0.25f /* 0x858C84 */ - m_fHeightAboveRoad;
        for (auto& sph : cd.GetSpheres()) {
            const auto sphBottomZ = sph.m_vecCenter.z - sph.m_fRadius;
            if (sphBottomZ < sphBottomMinZ) {
                if (sph.m_fRadius > 0.4f /* 0x858EE8 */) {
                    sph.m_fRadius = std::max(0.4f, sph.m_vecCenter.z - sphBottomMinZ);
                }
                sph.m_vecCenter.z = sphBottomMinZ + sph.m_fRadius;
            }
        }
    }
}

// 0x6B8970
void CBike::SetModelIndex(uint32 index) {
    CVehicle::SetModelIndex(index);
    SetupModelNodes();
}

// 0x6B5960
void CBike::SetupModelNodes() {
    std::ranges::fill(m_aBikeNodes, nullptr);
    CClumpModelInfo::FillFrameArray(GetRpClump(), m_aBikeNodes.data());
}

void CBike::PlayCarHorn() {
    if (m_nAlarmState && m_nAlarmState != -1 && GetStatus() != STATUS_WRECKED || m_HornCounter) {
        return;
    }

    if (m_nCarHornTimer) {
        m_nCarHornTimer -= 1;
        return;
    }

    m_nCarHornTimer = CGeneral::GetRandomNumber() % 128 - 106; // TODO: GetRandomNumberInRange
    const uint32 chance = m_nCarHornTimer % 8;

    if (chance < 4) {
        if (chance >= 2) {
            if (m_pDriver && m_autoPilot.carCtrlFlags.bHonkAtCar) {
                m_pDriver->Say(CTX_GLOBAL_BLOCKED);
            }
        }
        m_HornCounter = 45;
    } else {
        if (m_pDriver) {
            m_pDriver->Say(CTX_GLOBAL_BLOCKED);
        }
    }
}

// 0x6B7070
void CBike::SetupDamageAfterLoad() {
    // NOP
}

// 0x6B6950
void CBike::DoBurstAndSoftGroundRatios() {
    const auto mi        = CModelInfo::GetModelInfo(m_nModelIndex)->AsVehicleModelInfoPtr();
    const auto fwdSpeed  = std::abs(m_vecMoveSpeed.Dot(GetForward()));
    bool  active[4]      = { true, true, true, true };

    for (auto i = 0; i < 2; i++) {
        const auto wheelA = 2 * i;     // левое колесо пары (0 - перед, 2 - зад)
        const auto wheelB = 2 * i + 1; // правое колесо пары

        if (m_nWheelStatus[i] == WHEEL_STATUS_MISSING) {
            // Колеса нет - подвески нет, ratios в 1.0
            m_aWheelRatios[wheelA] = 1.0f;
            m_aWheelRatios[wheelB] = 1.0f;
            active[wheelA] = active[wheelB] = false;
        } else if (m_nWheelStatus[i] == WHEEL_STATUS_BURST) {
            // Пробитое колесо: случайная "тряска" подвески
            const auto n = (uint16)(int)(fwdSpeed * 40.0f /* 0x858A10 */) + 98;
            if ((int)((rand() & 0xFFFF) * (1.0f / 32768.0f) /* 0x858B14 */ * (float)n) < 100) {
                const auto ratioDelta = ((m_fLineLength[wheelA] - m_fSuspensionLength[wheelA]) / m_fLineLength[wheelA]) * 0.2f /* 0x871310 */;
                m_aWheelRatios[wheelA] = std::min(m_aWheelRatios[wheelA] + ratioDelta, 1.0f);
                m_aWheelRatios[wheelB] = std::min(m_aWheelRatios[wheelB] + ratioDelta, 1.0f);
            }
            active[wheelA] = active[wheelB] = false;
        } else {
            // Целое колесо на рельсах: пробуксовка при качении
            if ((m_aWheelRatios[wheelA] < 1.0f && m_aWheelColPoints[wheelA].m_nSurfaceTypeB == SURFACE_RAILTRACK)
                || (m_aWheelRatios[wheelB] < 1.0f && m_aWheelColPoints[wheelB].m_nSurfaceTypeB == SURFACE_RAILTRACK)) {
                const auto wheelSize = i == 0 ? mi->m_fWheelSizeFront : mi->m_fWheelSizeRear;
                auto       pitchK    = 1.5f /* 0x871328 */ / (wheelSize * 0.5f /* 0x858B8C */);
                if (0.3f /* 0x87132C */ < fwdSpeed) {
                    pitchK *= fwdSpeed / 0.3f;
                }
                pitchK = 1.0f / pitchK;

                const auto frac1 = m_aWheelPitchAngles[i] * pitchK - std::floor(m_aWheelPitchAngles[i] * pitchK);
                const auto rot   = (CTimer::GetTimeStep() * m_aWheelAngularVelocity[i] + m_aWheelPitchAngles[i]) * pitchK;
                const auto frac2 = rot - std::floor(rot);
                if ((m_aWheelAngularVelocity[i] > 0.0f && frac2 < frac1) || (m_aWheelAngularVelocity[i] < 0.0f && frac1 < frac2)) {
                    const auto ratioDelta = ((m_fLineLength[wheelA] - m_fSuspensionLength[wheelA]) / m_fLineLength[wheelA]) * 0.3f /* 0x871324 */;
                    m_aWheelRatios[wheelA] = std::max(m_aWheelRatios[wheelA] - ratioDelta, 0.2f /* 0x858CC4 */);
                    m_aWheelRatios[wheelB] = std::max(m_aWheelRatios[wheelB] - ratioDelta, 0.2f);
                }
            }
        }
    }

    // Финальный проход: песок замедляет колеса (кроме RHINO - он "байк" в SA)
    if (m_nModelIndex != MODEL_RHINO /* 432 */) {
        float sandK = 0.1f; /* 0x87131C */
        if (!handlingFlags.bOffroadAbility2) { // биты 0x100000/0x200000 m_nHandlingFlagsIntValue
            sandK = handlingFlags.bOffroadAbility ? 0.15f /* 0x871318 */ : 0.25f /* 0x871314 */;
        }
        for (auto i = 0; i < 4; i++) {
            if (active[i] && m_aWheelRatios[i] < 1.0f
                && g_surfaceInfos.GetAdhesionGroup(m_aWheelColPoints[i].m_nSurfaceTypeB) == eAdhesionGroup::ADHESION_GROUP_SAND) {
                auto grip = 1.0f - (fwdSpeed / 0.3f /* 0x871320 */) * 0.7f /* 0x858CB0 */ - CWeather::WetRoads * 0.7f;
                grip = std::max(grip, 0.4f /* 0x858EE8 */);
                m_aWheelRatios[i] = std::min(((m_fSuspensionLength[i] - m_fLineLength[i]) / m_fLineLength[i]) * grip * sandK + m_aWheelRatios[i], 1.0f);
            }
        }
    }
}

// 0x6B67E0
bool CBike::SetUpWheelColModel(CColModel* wheelCol) {
    const auto mi      = CModelInfo::GetModelInfo(m_nModelIndex)->AsVehicleModelInfoPtr();
    const auto chassis = m_aBikeNodes[BIKE_CHASSIS];
    auto&      cd      = *wheelCol->m_pColData;

    // Копируем bounding sphere + box из колмодели самого байка (оригинал: 10 dword'ов = 0x28 байт)
    const auto& ownCM      = *GetColModel();
    wheelCol->m_boundSphere = ownCM.m_boundSphere;
    wheelCol->m_boundBox    = ownCM.m_boundBox;

    // Позиция колесной ноды в координатах шасси:
    // моделирующая матрица ноды, домноженная на матрицы родителей до шасси (BIKE_CHASSIS)
    const auto WheelPosRelToChassis = [&](eBikeNodes node) {
        CMatrix mat{ RwFrameGetMatrix(m_aBikeNodes[node]), false };
        for (auto parent = RwFrameGetParent(m_aBikeNodes[node]); parent && parent != chassis; parent = RwFrameGetParent(parent)) {
            mat = CMatrix{ RwFrameGetMatrix(parent), false } * mat;
        }
        return mat.GetPosition();
    };

    // Переднее колесо
    auto& frontSphere = cd.m_pSpheres[0];
    frontSphere.Set(mi->m_fWheelSizeFront * 0.5f, WheelPosRelToChassis(BIKE_WHEEL_FRONT), SURFACE_RUBBER, CAR_PIECE_WHEEL_LF, tColLighting{ 0xFF });

    // Заднее колесо
    auto& rearSphere = cd.m_pSpheres[1];
    rearSphere.Set(mi->m_fWheelSizeRear * 0.5f, WheelPosRelToChassis(BIKE_WHEEL_REAR), SURFACE_RUBBER, CAR_PIECE_WHEEL_RL, tColLighting{ 0xFF });

    cd.m_nNumSpheres = 2;
    return true;
}

// 0x6B67B0
void CBike::RemoveRefsToVehicle(CEntity* entityToRemove) {
    for (auto& entity: m_aGroundPhysicalPtrs) {
        if (entity == entityToRemove)
            entity = nullptr;
    }
}

// 0x6B6620
void CBike::ProcessControlCollisionCheck(bool applySpeed) {
    const CMatrix oldMat = GetMatrix();
    SetIsStuck(false);
    SkipPhysics();
    physicalFlags.bSkipLineCol     = false;
    physicalFlags.bProcessingShift = false;
    m_fMovingSpeed                 = 0.0f;
    rng::fill(m_aWheelRatios, 1.0f);

    if (applySpeed) {
        ApplyMoveSpeed();
        ApplyTurnSpeed();

        for (auto i = 0; CheckCollision() && i < 5; i++) {
            GetMatrix() = oldMat;
            ApplyMoveSpeed();
            ApplyTurnSpeed();
        }
    } else {
        const auto usesCollision = GetUsesCollision();
        SetUsesCollision(false);
        CheckCollision();
        SetUsesCollision(usesCollision);
    }

    SetIsStuck(false);
    SetIsInSafePosition(true);
}

// 0x6B5990
void CBike::GetComponentWorldPosition(int32 componentId, CVector& outPos) {
    if (IsComponentPresent(componentId))
        outPos = RwFrameGetLTM(m_aBikeNodes[componentId])->pos;
    else
        NOTSA_LOG_DEBUG("BikeNode missing: model={}, nodeIdx={}", m_nModelIndex, componentId);
}

// 0x6B58D0
void CBike::ProcessOpenDoor(CPed* ped, uint32 doorComponentId, uint32 animGroup, uint32 animId, float fTime) {
    // NOP
}
