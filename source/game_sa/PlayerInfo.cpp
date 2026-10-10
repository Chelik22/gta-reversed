#include "StdInc.h"

#include "PlayerInfo.h"
#include "FireManager.h"
#include "MenuSystem.h"
#include "Hud.h"

// 0x858B90 - порог z-гейта лодок в FindClosestCarSectorList (значение по байтам не перепроверено
// верификатором - читаем по адресу, как в оригинале)
static auto& boat_flt_858B90 = StaticRef<float>(0x858B90);

void CPlayerInfo::InjectHooks() {
    RH_ScopedClass(CPlayerInfo);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x571920, { .State = HS::RedirectToGTA, .Locked = true }); // hooking ctor will produce bugs with weapons, you will never give weapon through cheat or something
    RH_ScopedInstall(CancelPlayerEnteringCars, 0x56E860);
    RH_ScopedInstall(FindObjectToSteal, 0x56DBD0);
    RH_ScopedInstall(EvaluateCarPosition, 0x56DAD0);
    RH_ScopedInstall(Process, 0x56F8D0, { .Reversed = false }); // STUB_DEFERRED: тело не реверснуто (см. 0x56F8D0.cpp)
    RH_ScopedInstall(FindClosestCarSectorList, 0x56F4E0);
    RH_ScopedInstall(Clear, 0x56F330);
    RH_ScopedInstall(StreamParachuteWeapon, 0x56EB30);
    RH_ScopedInstall(AddHealth, 0x56EAB0);
    RH_ScopedInstall(ArrestPlayer, 0x56E5D0);
    RH_ScopedInstall(SetPlayerSkin, 0x5717F0);
    RH_ScopedInstall(LoadPlayerSkin, 0x56F7D0);
    RH_ScopedInstall(DeletePlayerSkin, 0x56EA80);
    RH_ScopedInstall(BlowUpRCBuggy, 0x56EA30);
    RH_ScopedInstall(MakePlayerSafe, 0x56E870);
    RH_ScopedInstall(PlayerFailedCriticalMission, 0x56E830);
    RH_ScopedInstall(WorkOutEnergyFromHunger, 0x56E610);
    RH_ScopedInstall(KillPlayer, 0x56E580);
    RH_ScopedInstall(IsRestartingAfterMissionFailed, 0x56E570);
    RH_ScopedInstall(IsRestartingAfterArrest, 0x56E560);
    RH_ScopedInstall(IsRestartingAfterDeath, 0x56E550);
    RH_ScopedInstall(IsPlayerInRemoteMode, 0x56DAB0);
    RH_ScopedInstall(GetPos_Hook, 0x56DFB0);
    RH_ScopedInstall(GetSpeed_Hook, 0x56DF50);
    RH_ScopedInstall(GivePlayerParachute, 0x56EC40);
    RH_ScopedInstall(SetLastTargetVehicle, 0x56DA80);
    RH_ScopedInstall(Load, 0x5D3B00);
    RH_ScopedInstall(Save, 0x5D3AC0);
}

// 0x571920
CPlayerInfo::CPlayerInfo() {
    plugin::CallMethod<0x571920, CPlayerInfo*>(this); // see hook
    return;

    m_PlayerData = CPlayerPedData();

    m_pSkinTexture = nullptr;
    m_bParachuteReferenced = false;
    m_nRequireParachuteTimer = 0;
}

// 0x571920
CPlayerInfo* CPlayerInfo::Constructor() {
    this->CPlayerInfo::CPlayerInfo();
    return this;
}

CVector* CPlayerInfo::GetSpeed_Hook(CVector* out) {
    *out = GetSpeed();
    return out;
}

CVector* CPlayerInfo::GetPos_Hook(CVector* outPos) {
    *outPos = GetPos();
    return outPos;
}

// 0x56E860
void CPlayerInfo::CancelPlayerEnteringCars(CVehicle* vehicle) {
    // NOP
}

// 0x56DBD0
// Константы декодированы из gta_sa.exe (1.0 US):
//   0x858B8C=0.5f 0x858B3C=3.0f 0x858B38=0.02f 0x858B34=60.0f 0x863214=4.5f
//   0x85862C=10.0f 0x858C4C=1000.0f 0xB7CD78=CWorld::ms_nCurrentScanCode
//   0xB992C0 = CWorld::ms_aRepeatSectors[].Objects (0xB992B8 + 0x8)
// Оффсеты: ped+0x14=m_matrix (fwd=+0x10, right=+0x00, pos=+0x30), ped+0x04=placement.pos;
//   obj+0x140=m_nObjectFlags (bit 0x2000 = bIsLiftable), obj+0x2C=m_ScanCode.
// Прочее: FUN_008219F0/FUN_00821B40 в декомпиле — пара float->int (_ftol/_ftol2) = обычный (int)каст;
//   лимиты секторов: min >= 0, max <= 0x77 (119).
CEntity* CPlayerInfo::FindObjectToSteal(CPed* ped) {
    CEntity* result = nullptr;

    // Точка поиска перед пэдом: pos + forward * (0.5f * 3.0f)
    const auto& pos = ped->GetPosition();      // matrix ? matrix->pos : placement.pos (оригинал: +0x14/+0x04)
    const auto& fwd = ped->GetForward();       // matrix->forward (RW-матрица, +0x10)
    const auto  searchX = pos.x + fwd.x * 1.5f;
    const auto  searchY = pos.y + fwd.y * 1.5f;
    const auto  searchZ = pos.z + fwd.z * 1.5f;

    // Координаты -> индексы 120 секторов (по 50 юнитов): c * 0.02f + 60.0f, окно ±3.0f
    const auto SectorOf = [](float c) { return (int32)(c * 0.02f + 60.0f); };

    const auto minX = std::max(0,   SectorOf(searchX - 3.0f));
    const auto minY = std::max(0,   SectorOf(searchY - 3.0f));
    const auto maxX = std::min(119, SectorOf(searchX + 3.0f));
    const auto maxY = std::min(119, SectorOf(searchY + 3.0f));
    // Оригинал (0x56DBD0): if (DAT_00b7cd78 == -1) { FUN_00563470(); DAT_00b7cd78 = 1; } else { DAT_00b7cd78++; }
    // FUN_00563470 = CWorld::ClearScanCodes; 0x4072E0 = CWorld::AdvanceCurrentScanCode - ровно эта логика.
    // Без инкремента сравнение obj->GetScanCode() != ms_nCurrentScanCode сравнивало бы с устаревшим значением.
    CWorld::AdvanceCurrentScanCode();
    if (maxY < minY) {
        return nullptr;
    }

    for (auto y = minY; y <= maxY; y++) {
        for (auto x = minX; x <= maxX; x++) {
            for (auto* const obj : CWorld::GetRepeatSector(x, y).Objects) { // (x & 0xF) + (y & 0xF) * 16
                const auto& objPosn = obj->GetPosition();
                const auto  dx = objPosn.x - searchX;
                const auto  dy = objPosn.y - searchY;
                const auto  dz = objPosn.z - searchZ;

                if (   obj->objectFlags.bIsLiftable          // +0x140 & 0x2000
                    && obj->GetScanCode() != CWorld::ms_nCurrentScanCode // +0x2C vs 0xB7CD78
                    && dx * dx + dy * dy + dz * dz < 4.5f
                ) {
                    const auto& mat    = *ped->m_matrix;
                    auto        dotFwd = DotProduct(CVector{dx, dy, dz}, mat.GetForward());
                    if (dotFwd < 0.0f) {
                        dotFwd = 10.0f - dotFwd;
                    }
                    if (dotFwd + std::abs(DotProduct(CVector{dx, dy, dz}, mat.GetRight())) * 3.0f < 1000.0f) {
                        result = obj;
                    }
                }
            }
        }
    }
    return result;
}

// 0x56DAD0
void CPlayerInfo::EvaluateCarPosition(CEntity* car, CPed* ped, float pedToVehDist, float* outDistance, CVehicle** outVehicle) {
    const auto& carPosn = car->GetPosition();
    const auto& pedPosn = ped->GetPosition();
    const auto& forward = ped->GetForward();

    // Find our rotation (so that is, at which angle the forward vector is)
    const auto angleFront = CGeneral::GetATanOfXY(forward.x, forward.y);

    // Find car angle, not relative to our rotation
    const auto carAngle = CGeneral::GetATanOfXY(carPosn.x - pedPosn.x, carPosn.y - pedPosn.y);

    // Make car's angle relative to our rotation (notice: it's an abs value)
    // Basically, we calculate how much the car is in our FOV.
    const auto carAngleFromFront = std::abs(CGeneral::LimitRadianAngle(angleFront - carAngle));

    // Calculate imaginary distance based on the car's angle: The higher the angle the greater the distance
    const auto distance = (1.0f - carAngleFromFront / TWO_PI) * (10.0f - pedToVehDist);
    if (distance >= *outDistance) {
        *outDistance = distance;
        *outVehicle = car->AsVehicle();
    }
}

// 0x56F7D0
void CPlayerInfo::LoadPlayerSkin() {
    DeletePlayerSkin();
    m_pSkinTexture = CPlayerSkin::GetSkinTexture(m_szSkinName);
}

// 0x56EA80
void CPlayerInfo::DeletePlayerSkin() {
    if (m_pSkinTexture) {
        RwTextureDestroy(m_pSkinTexture);
        m_pSkinTexture = nullptr;
    }
}

// 0x5717F0
void CPlayerInfo::SetPlayerSkin(const char* name) {
    strcpy_s(m_szSkinName, name); // NOTSA: They used `strcpy`, we use `_s` for safety
    LoadPlayerSkin();
}

// 0x56DA80
void CPlayerInfo::SetLastTargetVehicle(CVehicle* vehicle) {
    CEntity::SafeCleanUpRef(m_pLastTargetVehicle);
    m_pLastTargetVehicle = vehicle;
    CEntity::SafeRegisterRef(m_pLastTargetVehicle);
}

// 0x56F8D0
// STUB_DEFERRED — декомпил ~1030 строк, ~25 невыясненных вызовов, x87/_ftol2-паттерны, SEH.
// Тело оставлено редиректом; карта верифицированных фактов для будущего реверса — в шапке фрагмента
// D:\projects\keep\batch_out\fresh\0x56F8D0.cpp. Сигнатура свёрена: void Process(uint32 playerIndex).
void CPlayerInfo::Process(uint32 playerIndex) {
    plugin::CallMethod<0x56F8D0, CPlayerInfo*, uint32>(this, playerIndex);
}

// 0x56F4E0
// Константы из gta_sa.exe (1.0 US): 0x858C24=0.3f 0x858624=1.0f 0x858CA0=2.0f 0x85862C=10.0f
//   0x858B50=0.0f (множитель X/Y-слагаемых!) 0x41200000=10.0f 0xB7CD78=CWorld::ms_nCurrentScanCode
// Оффсеты: veh+0x1C bit0=m_bUsesCollision; veh+0x36=m_info (тип &7==2, статус >>3);
//   veh+0x42E & 0x20 = vehicleFlags.bCanBeDamaged; veh+0x590=m_nVehicleType (9=BIKE, 5=BOAT);
//   veh+0x22=m_nModelIndex (0x241=577=MODEL_AT400); veh+0x14=m_matrix, +0x04=placement.pos, +0x28=up.z.
// Прочее: vtbl+0xD4 = CVehicle::GetHeightAboveRoad(); вызов GetBoundCentre в оригинале затирается
//   нечитанной позицией (квирк R*) — опущен (чистая функция, без сайд-эффектов).
void CPlayerInfo::FindClosestCarSectorList(CPtrListDoubleLink<CVehicle*>& ptrList, CPed* ped, float minX, float minY, float maxX, float maxY, float* outVehDist, CVehicle** outVehicle) {
    // Оригинал: this и границы сектора (minX..maxY) не используются

    for (auto* const veh : ptrList) {
        if (veh->IsScanCodeCurrent()) {                       // +0x2C vs 0xB7CD78
            continue;
        }
        if (!veh->m_bUsesCollision) {                         // +0x1C bit0
            continue;
        }
        // +0x36 = CEntity::m_info (protected в хедере) — читаем байт по смещению, как в оригинале
        const auto infoByte = *reinterpret_cast<const uint8*>(uintptr_t(veh) + 0x36);
        if (eEntityType(infoByte & 7) != ENTITY_TYPE_VEHICLE) { // & 7 == 2
            continue;
        }
        const auto status = eEntityStatus(infoByte >> 3);       // >> 3
        veh->SetCurrentScanCode();
        // Оригинал: bVar3 == 5 || bVar3 == 6. По eEntityStatus ЭТОГО репо (PLAYER=0,..,ABANDONED=4,
        // WRECKED=5, TRAIN_MOVING=6) это STATUS_WRECKED и STATUS_TRAIN_MOVING - не ABANDONED/WRECKED.
        if (status == STATUS_WRECKED || status == STATUS_TRAIN_MOVING) {
            continue;
        }

        if (!veh->m_matrix) {
            veh->AllocateMatrix();                            // 0x54F560
            veh->m_placement.UpdateMatrix(veh->m_matrix);     // 0x54F1B0
        }

        // Перевёрнутая (up.z <= 0.3) машина — только байк подлежит обработке
        if (   veh->m_matrix->GetUp().z <= 0.3f
            && (eEntityType(infoByte & 7) != ENTITY_TYPE_VEHICLE || veh->m_nVehicleType != VEHICLE_TYPE_BIKE)
        ) {
            continue;
        }

        // (void)veh->GetBoundCentre(); — в оригинале вызывается (0x534250), но результат затирается позицией
        auto vehPos = veh->GetPosition();                     // matrix ? matrix->pos : placement.pos
        vehPos.z -= veh->GetHeightAboveRoad();                // vtbl+0xD4
        vehPos.z += 1.0f;

        // В оригинале дистанция = только разница Z: X/Y-слагаемые умножены на 0.0f (0x858B50)
        auto pedToVehDist = ped->GetPosition().z - vehPos.z;

        const auto ProcessVehicle = [&]() {
            if (!veh->vehicleFlags.bCanBeDamaged) {           // +0x42E & 0x20
                return;
            }
            CVector doorPos{};
            int32   doorId{};
            if (!CCarEnterExit::GetNearestCarDoor(ped, veh, doorPos, doorId)) {
                return;
            }
            const auto& carPosn = veh->GetPosition();
            const auto& pedPosn = ped->GetPosition();
            const auto  dist    = CVector{ pedPosn.x - carPosn.x, pedPosn.y - carPosn.y, 0.0f }.Magnitude(); // z=0 в оригинале
            auto        effectiveDist = dist;
            if (dist > 10.0f) { // оригинал: 0x85862C(10.0f) < dist - строго больше
                if (!CPedGeometryAnalyser::LiesInsideBoundingBox(*ped, pedPosn, *veh)) { // 0x5F3880
                    return;
                }
                effectiveDist = 10.0f;
            }
            if (!CCranes::IsThisCarBeingCarriedByAnyCrane(veh)) { // 0x6F3FF0
                EvaluateCarPosition(veh, ped, effectiveDist, outVehDist, outVehicle); // 0x56DAD0
            }
        };

        if (veh->m_nModelIndex == MODEL_AT400) {              // 0x241 = 577
            CVector doorPos{};
            int32   doorId{};
            if (CCarEnterExit::GetNearestCarDoor(ped, veh, doorPos, doorId)) {
                pedToVehDist = std::abs(ped->GetPosition().z - doorPos.z);
                if (pedToVehDist < 1.0f) {
                    ProcessVehicle();
                    continue;
                }
            }
        }
        if (std::abs(pedToVehDist) < 2.0f) {
            ProcessVehicle();
        } else if (veh->m_nVehicleType == VEHICLE_TYPE_BOAT) {
            // Оригинал (0x56F699..0x56F6DE): для не-лодок ветка `|dist|>=2` уходит на следующую машину,
            // а для BOAT (vehType==5) do-while проваливается в доп.гейт:
            //   if (vehPos.z < pedPos.z) { if (vehPos.z <= pedPos.z - DAT_00858b90) -> нужна контактная проверка; else -> Process }
            //   else (vehPos.z >= pedPos.z) -> нужна контактная проверка
            //   контактная проверка: ped->m_pContactEntity (+0x584) == veh, иначе следующая машина
            const auto pedZ = ped->GetPosition().z;
            if (vehPos.z >= pedZ || vehPos.z <= pedZ - boat_flt_858B90) {
                if (ped->m_pContactEntity != veh) {
                    continue;
                }
            }
            ProcessVehicle();
        }
    }
}

// 0x56F330
void CPlayerInfo::Clear() {
    // TODO: This should just use the constructor and `swap`,
    // like to just swap ourselves to a default constructed object
    // and do all the cleanup in the destructor

    m_pPed = nullptr;
    m_pRemoteVehicle = nullptr;
    if (m_pSpecCar) {
        m_pSpecCar->physicalFlags.bAddMovingCollisionSpeed = false;
        m_pSpecCar = nullptr;
    }
    m_nDisplayMoney = 0;
    m_nMoney = 0;
    m_nPlayerState = PLAYERSTATE_PLAYING;
    m_nCarDensityForCurrentZone = 0;
    m_fRoadDensityAroundPlayer = 1.0f;
    m_bAfterRemoteVehicleExplosion = false;
    m_bCreateRemoteVehicleExplosion = false;
    m_bFadeAfterRemoteVehicleExplosion = false;
    m_bTryingToExitCar = false;
    m_bTaxiTimerScore = false;
    m_nTaxiTimer = 0;
    m_nVehicleTimeCounter = CTimer::GetTimeInMS();
    m_nMaxArmour = 100;
    m_nMaxHealth = 100;
    m_bCanDoDriveBy = true;
    m_nCollectablesPickedUp = 0;
    m_nTotalNumCollectables = 3;
    m_nLastTimeEnergyLost = 0;
    m_nLastTimeArmourLost = 0;
    m_nLastTimeBigGunFired = 0;
    m_nTimesStuckInARow = 0;
    m_nTimesUpsideDownInARow = 0;
    m_nCarTwoWheelCounter = 0;
    m_fCarTwoWheelDist = 0.0f;
    m_nCarLess3WheelCounter = 0;
    m_nBikeRearWheelCounter = 0;
    m_fBikeRearWheelDist = 0.0f;
    m_nBikeFrontWheelCounter = 0;
    m_fBikeFrontWheelDist = 0.0f;
    m_nTempBufferCounter = 0;
    m_nBestCarTwoWheelsTimeMs = 0;
    m_fBestCarTwoWheelsDistM = 0.0f;
    m_nBestBikeWheelieTimeMs = 0;
    m_fBestBikeWheelieDistM = 0.0f;
    m_nBestBikeStoppieTimeMs = 0;
    m_fBestBikeStoppieDistM = 0.0f;
    m_bDoesNotGetTired = false;
    m_bFastReload = false;
    m_bFireProof = false;
    m_bGetOutOfJailFree = false;
    m_bFreeHealthCare = false;
    m_nTimeOfLastCarExplosionCaused = 0;
    m_nExplosionMultiplier = 0;
    m_nHavocCaused = 0;
    FindPlayerInfo().m_nNumHoursDidntEat = 0;
    m_nLastBustMessageNumber = 1;
    m_fCurrentChaseValue = 0.0f;
    m_nBustedAudioStatus = 0;
    m_nCrosshairActivated = 0;

    m_nRequireParachuteTimer = 0;

    if (m_bParachuteReferenced) {
        CGameLogic::IsCoopGameGoingOn();
        if (m_bParachuteReferenced) {
            CStreaming::SetModelIsDeletable(MODEL_GUN_PARA);
            m_bParachuteReferenced = false;
            m_nRequireParachuteTimer = 0;
        }
    }
}

// 0x56EC40
void CPlayerInfo::GivePlayerParachute() const {
    if (m_nRequireParachuteTimer) {
        if (CStreaming::IsModelLoaded(MODEL_GUN_PARA)) {
            m_pPed->GiveWeapon(WEAPON_PARACHUTE, 1, true);
            m_pPed->SetSavedWeapon(WEAPON_PARACHUTE);
        }
    }
}

// 0x56EB30
void CPlayerInfo::StreamParachuteWeapon(bool unk) {
    if (CGameLogic::IsCoopGameGoingOn()) {
        if (unk) {
            return;
        }
        if (m_bParachuteReferenced) {
            CStreaming::SetModelIsDeletable(MODEL_GUN_PARA);
            m_bParachuteReferenced = false;
            m_nRequireParachuteTimer = 0;
        }
        return;
    }

    if (m_pPed && m_pPed->IsInVehicle()) {
        if (m_pPed->m_pVehicle->IsSubPlane() || m_pPed->m_pVehicle->IsSubHeli()) {
            if (m_nRequireParachuteTimer <= (uint32)CTimer::GetTimeStepInMS()) {
                const auto groundHeight = TheCamera.CalculateGroundHeight(eGroundHeightType::ENTITY_BB_BOTTOM);
                const auto vehToGroundZDist = m_pPed->m_pVehicle->GetPosition().z - groundHeight;
                m_nRequireParachuteTimer = (vehToGroundZDist <= 50.f) ? 0 : 5000;
            } else {
                m_nRequireParachuteTimer -= (uint32)CTimer::GetTimeStepInMS();
            }
        }
    }

    if (m_nRequireParachuteTimer) {
        CStreaming::RequestModel(MODEL_GUN_PARA, STREAMING_MISSION_REQUIRED);
        m_bParachuteReferenced = true;
        return;
    }

    if (m_bParachuteReferenced) {
        CStreaming::SetModelIsDeletable(MODEL_GUN_PARA);
        m_bParachuteReferenced = false;
        m_nRequireParachuteTimer = 0;
    }
}

// 0x56EAB0
void CPlayerInfo::AddHealth(int32 amount) const {
    const auto newValue = std::min((float)m_nMaxHealth, m_pPed->m_fHealth + (float)amount); // Clamp to m_nMaxHealth
    m_pPed->m_fHealth = std::max(newValue, m_pPed->m_fHealth); // Don't change health to a lower value
}

// 0x56EA30
void CPlayerInfo::BlowUpRCBuggy(bool bExplode) const {
    if (m_pRemoteVehicle && !m_pRemoteVehicle->m_bRemoveFromWorld) {
        CRemote::TakeRemoteControlledCarFromPlayer(bExplode);
        if (bExplode)
            m_pRemoteVehicle->BlowUpCar(FindPlayerPed(), false);
    }
}

// 0x56E870
void CPlayerInfo::MakePlayerSafe(bool enable, float radius) {
    // Not quite SA, but this is the way to do it (instead of copy pasting it twice)
    auto& flags = m_pPed->physicalFlags;
    flags.bInvulnerable = enable;
    flags.bBulletProof = enable;
    flags.bFireProof = enable;
    flags.bExplosionProof = enable;
    flags.bCollisionProof = enable;
    flags.bMeleeProof = enable;
    m_PlayerData.m_bCanBeDamaged = !enable;
    m_PlayerData.m_pWanted->m_bEverybodyBackOff = enable;
    m_pPed->GetPadFromPlayer()->bPlayerSafe = enable;
    CWorld::SetAllCarsCanBeDamaged(!enable);

    if (enable) {
        CWorld::StopAllLawEnforcersInTheirTracks();
        CPad::StopPadsShaking();

        m_pPed->ClearAdrenaline();
        m_PlayerData.m_fTimeCanRun = std::max(m_PlayerData.m_fTimeCanRun, 0.f);
        m_pPed->GetIntelligence()->ClearTasks(true, false);

        gFireManager.ExtinguishPoint(GetPos(), radius);
        CExplosion::RemoveAllExplosionsInArea(GetPos(), 4000.f);
        CProjectileInfo::RemoveAllProjectiles();
        CWorld::ExtinguishAllCarFiresInArea(GetPos(), radius);
        CReplay::DisableReplays();
        m_pPed->ClearWeaponTarget();
    } else {
        CReplay::EnableReplays();
    }
}

// 0x56E830
void CPlayerInfo::PlayerFailedCriticalMission() {
    if (m_nPlayerState == PLAYERSTATE_PLAYING) {
        m_nPlayerState = PLAYERSTATE_FAILED_MISSION;
        CGameLogic::SetMissionFailed();
        CDarkel::ResetOnPlayerDeath();
    }
}

// 0x56E610
void CPlayerInfo::WorkOutEnergyFromHunger() {

    static auto& s_lastTimeHungryStateProcessedInitialized = StaticRef<bool>(0xB9B8F4); // false
    static auto& s_lastTimeHungryStateProcessed = StaticRef<uint8>(0xB9B8F2);
    static auto& s_LastHungryState = StaticRef<int8>(0xB9B8F1);
    static auto& s_bHungryMessageShown = StaticRef<bool>(0xB9B8F0);

    if (CCheat::IsActive(CHEAT_NEVER_GET_HUNGRY)) {
        return;
    }

    if (!s_lastTimeHungryStateProcessedInitialized) {
        s_lastTimeHungryStateProcessedInitialized = true;
        s_lastTimeHungryStateProcessed = CClock::GetGameClockHours();
    }

    auto pad = CPad::GetPad();
    if (   pad->ArePlayerControlsDisabled()
        || CMenuSystem::num_menus_in_use
        || TheCamera.m_bWideScreenOn
        || CCutsceneMgr::ms_running
        || CGameLogic::IsCoopGameGoingOn()
        || m_pRemoteVehicle
    ) {
        return;
    }

    if (!m_pPed)
        return;

    if (m_pPed->m_pAttachedTo)
        return;

    if (CClock::GetGameClockHours() != s_lastTimeHungryStateProcessed) {
        if (!m_nNumHoursDidntEat)
            s_LastHungryState = 0;
        m_nNumHoursDidntEat += 1;
    }

    if (m_nNumHoursDidntEat <= 48) {
        s_bHungryMessageShown = false;
    } else {
        if (CClock::GetGameClockHours() == s_lastTimeHungryStateProcessed)
            return;

        m_pPed->Say(CTX_GLOBAL_STOMACH_RUMBLE);
        pad->StartShake(400, 110u, 0);

        if (s_bHungryMessageShown) {
            bool bDecreaseHealth{};
            if (CStats::GetStatValue(STAT_FAT) > 0.0f) {
                CStats::DecrementStat(STAT_FAT, 25.0f);
                CStats::DisplayScriptStatUpdateMessage(STAT_UPDATE_DECREASE, STAT_FAT, 25.0f);
                bDecreaseHealth = true;
                if (!s_LastHungryState) {
                    s_LastHungryState = m_nNumHoursDidntEat + 24;
                }
            }

            if (CStats::GetStatValue(STAT_MUSCLE) <= 0.0f || m_nNumHoursDidntEat <= s_LastHungryState && s_LastHungryState) {
                if (!bDecreaseHealth) {
                    m_pPed->m_fHealth -= 2.0f;
                }
            } else {
                CStats::DecrementStat(STAT_MUSCLE, 25.0);
                CStats::DisplayScriptStatUpdateMessage(STAT_UPDATE_DECREASE, STAT_MUSCLE, 25.0f);
            }
        } else {
            CHud::SetHelpMessage(TheText.Get("NOTEAT"), true, false, true);
            s_bHungryMessageShown = true;
        }
    }

    if (CClock::GetGameClockHours() != s_lastTimeHungryStateProcessed) {
        s_lastTimeHungryStateProcessed = CClock::GetGameClockHours();
    }
}

// 0x56E5D0
void CPlayerInfo::ArrestPlayer() {
    if (m_nPlayerState == PLAYERSTATE_PLAYING) {
        m_nPlayerState = PLAYERSTATE_HAS_BEEN_ARRESTED;
        m_nBustedAudioStatus = 0;
        CDarkel::ResetOnPlayerDeath();
        CStats::IncrementStat(STAT_TIMES_BUSTED, 1.0f);
        CGangWars::EndGangWar(false);
    }
}

// 0x56E580
void CPlayerInfo::KillPlayer() {
    if (m_nPlayerState == PLAYERSTATE_PLAYING) {
        m_nPlayerState = PLAYERSTATE_HAS_DIED;
        CDarkel::ResetOnPlayerDeath();
        CMessages::AddBigMessage(TheText.Get("DEAD"), 4000, STYLE_WHITE_MIDDLE);
        CStats::IncrementStat(STAT_NUMBER_OF_HOSPITAL_VISITS, 1.0f);
        CGangWars::EndGangWar(false);
    }
}

// 0x56E570
bool CPlayerInfo::IsRestartingAfterMissionFailed() const {
    return m_nPlayerState == PLAYERSTATE_FAILED_MISSION;
}

// 0x56E560
bool CPlayerInfo::IsRestartingAfterArrest() const {
    return m_nPlayerState == PLAYERSTATE_HAS_BEEN_ARRESTED;
}

// 0x56E550
bool CPlayerInfo::IsRestartingAfterDeath() const {
    return m_nPlayerState == PLAYERSTATE_HAS_DIED;
}

// 0x56DAB0
bool CPlayerInfo::IsPlayerInRemoteMode() const {
    return m_pRemoteVehicle || m_bAfterRemoteVehicleExplosion;
}

// 0x56DFB0
// Return occupied vehicle's (if in any) or player's ped position
CVector CPlayerInfo::GetPos() const {
    return m_pPed->IsInVehicle() ? m_pPed->m_pVehicle->GetPosition() : m_pPed->GetPosition();
}

// 0x56DF50
// Return occupied vehicle's (if in any) or player's ped move speed
CVector CPlayerInfo::GetSpeed() const {
    return m_pPed->IsInVehicle() ? m_pPed->m_pVehicle->GetMoveSpeed() : m_pPed->GetMoveSpeed();
}

// 0x5D3B00
bool CPlayerInfo::Load() {
    CGenericGameStorage::LoadDataFromWorkBuffer<int32>(); // Discarded
    auto data = CGenericGameStorage::LoadDataFromWorkBuffer<CPlayerInfoSaveStructure>();
    data.Extract(this);
    return true;
}

// 0x5D3AC0
bool CPlayerInfo::Save() {
    CPlayerInfoSaveStructure data;
    data.Construct(this);
    CGenericGameStorage::SaveDataToWorkBuffer(sizeof(CPlayerInfoSaveStructure));
    CGenericGameStorage::SaveDataToWorkBuffer(data);
    return true;
}

// 0x45DEF0
CPlayerInfo& CPlayerInfo::operator=(const CPlayerInfo& rhs) {
    m_pPed                             = rhs.m_pPed;
    m_PlayerData                       = rhs.m_PlayerData;
    m_pRemoteVehicle                   = rhs.m_pRemoteVehicle;
    m_pSpecCar                         = rhs.m_pSpecCar;
    m_nMoney                           = rhs.m_nMoney;
    m_nDisplayMoney                    = rhs.m_nDisplayMoney;
    m_nCollectablesPickedUp            = rhs.m_nCollectablesPickedUp;
    m_nTotalNumCollectables            = rhs.m_nTotalNumCollectables;
    m_nLastBumpPlayerCarTimer          = rhs.m_nLastBumpPlayerCarTimer;
    m_nTaxiTimer                       = rhs.m_nTaxiTimer;
    m_nVehicleTimeCounter              = rhs.m_nVehicleTimeCounter;
    m_bTaxiTimerScore                  = rhs.m_bTaxiTimerScore;
    m_bTryingToExitCar                 = rhs.m_bTryingToExitCar;
    m_pLastTargetVehicle               = rhs.m_pLastTargetVehicle;
    m_nPlayerState                     = rhs.m_nPlayerState;
    m_bAfterRemoteVehicleExplosion     = rhs.m_bAfterRemoteVehicleExplosion;
    m_bCreateRemoteVehicleExplosion    = rhs.m_bCreateRemoteVehicleExplosion;
    m_bFadeAfterRemoteVehicleExplosion = rhs.m_bFadeAfterRemoteVehicleExplosion;
    m_nTimeOfRemoteVehicleExplosion    = rhs.m_nTimeOfRemoteVehicleExplosion;
    m_nLastTimeEnergyLost              = rhs.m_nLastTimeEnergyLost;
    m_nLastTimeArmourLost              = rhs.m_nLastTimeArmourLost;
    m_nLastTimeBigGunFired             = rhs.m_nLastTimeBigGunFired;
    m_nTimesUpsideDownInARow           = rhs.m_nTimesUpsideDownInARow;
    m_nTimesStuckInARow                = rhs.m_nTimesStuckInARow;
    m_nCarTwoWheelCounter              = rhs.m_nCarTwoWheelCounter;
    m_fCarTwoWheelDist                 = rhs.m_fCarTwoWheelDist;
    m_nCarLess3WheelCounter            = rhs.m_nCarLess3WheelCounter;
    m_nBikeRearWheelCounter            = rhs.m_nBikeRearWheelCounter;
    m_fBikeRearWheelDist               = rhs.m_fBikeRearWheelDist;
    m_nBikeFrontWheelCounter           = rhs.m_nBikeFrontWheelCounter;
    m_fBikeFrontWheelDist              = rhs.m_fBikeFrontWheelDist;
    m_nTempBufferCounter               = rhs.m_nTempBufferCounter;
    m_nBestCarTwoWheelsTimeMs          = rhs.m_nBestCarTwoWheelsTimeMs;
    m_fBestCarTwoWheelsDistM           = rhs.m_fBestCarTwoWheelsDistM;
    m_nBestBikeWheelieTimeMs           = rhs.m_nBestBikeWheelieTimeMs;
    m_fBestBikeWheelieDistM            = rhs.m_fBestBikeWheelieDistM;
    m_nBestBikeStoppieTimeMs           = rhs.m_nBestBikeStoppieTimeMs;
    m_fBestBikeStoppieDistM            = rhs.m_fBestBikeStoppieDistM;
    m_nCarDensityForCurrentZone        = rhs.m_nCarDensityForCurrentZone;
    m_fRoadDensityAroundPlayer         = rhs.m_fRoadDensityAroundPlayer;
    m_nTimeOfLastCarExplosionCaused    = rhs.m_nTimeOfLastCarExplosionCaused;
    m_nExplosionMultiplier             = rhs.m_nExplosionMultiplier;
    m_nHavocCaused                     = rhs.m_nHavocCaused;
    m_nNumHoursDidntEat                = rhs.m_nNumHoursDidntEat;
    m_fCurrentChaseValue               = rhs.m_fCurrentChaseValue;
    m_bDoesNotGetTired                 = rhs.m_bDoesNotGetTired;
    m_bFastReload                      = rhs.m_bFastReload;
    m_bFireProof                       = rhs.m_bFireProof;
    m_nMaxHealth                       = rhs.m_nMaxHealth;
    m_nMaxArmour                       = rhs.m_nMaxArmour;
    m_bGetOutOfJailFree                = rhs.m_bGetOutOfJailFree;
    m_bFreeHealthCare                  = rhs.m_bFreeHealthCare;
    m_bCanDoDriveBy                    = rhs.m_bCanDoDriveBy;
    m_nBustedAudioStatus               = rhs.m_nBustedAudioStatus;
    m_nLastBustMessageNumber           = rhs.m_nLastBustMessageNumber;
    m_nCrosshairActivated              = rhs.m_nCrosshairActivated;
    m_vecCrosshairTarget               = rhs.m_vecCrosshairTarget;
    m_pSkinTexture                     = rhs.m_pSkinTexture;
    m_bParachuteReferenced             = rhs.m_bParachuteReferenced;
    m_nRequireParachuteTimer           = rhs.m_nRequireParachuteTimer;
    strcpy_s(m_szSkinName, rhs.m_szSkinName);
    return *this;
}
