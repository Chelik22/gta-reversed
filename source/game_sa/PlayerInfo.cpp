#include "StdInc.h"

#include "PlayerInfo.h"
#include "FireManager.h"
#include "MenuSystem.h"
#include "Hud.h"
#include "Tasks/TaskTypes/TaskComplexGoPickUpEntity.h"
#include "Tasks/TaskTypes/TaskSimpleHoldEntity.h"
#include "Tasks/TaskTypes/TaskSimpleJetPack.h"
#include "Tasks/TaskTypes/TaskSimpleSwim.h"
#include "Tasks/TaskTypes/TaskComplexEnterCarAsPassenger.h"
#include "Tasks/TaskTypes/TaskComplexLeaveCar.h"

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
    RH_ScopedInstall(Process, 0x56F8D0); // полная реверсня (wf-final), 3 остаточных plugin::Call в теле
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

// Статик-переменные логики погони (оригинальные глобалы Process)
static auto& Process_ChasePosInitialized_   = StaticRef<uint8>(0xB9B9A0);    // bit0: кэш позиции инициализирован
static auto& Process_ChaseCachedPlayerPos_  = StaticRef<CVector>(0xB9B994);  // позиция игрока с прошлой проверки
static auto& Process_ChasePlayerMovedFar_   = StaticRef<uint8>(0x8CDF21);    // игрок уехал >= 10.0f от кэша
static auto& Process_ChaseNodeFound_        = StaticRef<uint8>(0x8CDF20);    // найдена роад-нода рядом

void CPlayerInfo::Process(uint32 playerIndex) {
    if (CReplay::Mode == MODE_PLAYBACK) {
        return;
    }

    const auto pad = CPad::GetPad(playerIndex);
    const auto ped = m_pPed;

    // Такси-тариф: +1$ за каждую секунду с пассажиром в TAXI/CABBIE
    if (m_bTaxiTimerScore && ped->bInVehicle) {
        const auto veh = ped->m_pVehicle;
        const auto now = CTimer::GetTimeInMS();
        if ((veh->m_nModelIndex == MODEL_TAXI || veh->m_nModelIndex == MODEL_CABBIE)
            && veh->m_pDriver == ped
            && veh->m_nNumPassengers != 0
        ) {
            if (const auto elapsed = now - m_nTaxiTimer; elapsed > 999) {
                const auto seconds = elapsed / 1000;
                m_nMoney += seconds;
                m_nTaxiTimer += seconds * 1000;
            }
        } else {
            m_nTaxiTimer = now;
        }
    } else {
        m_nTaxiTimer = CTimer::GetTimeInMS();
    }

    // Счётчики трюков: two wheels / wheelie / stoppie (+ меньше 3 колёс)
    const auto ResetStuntCounters = [&] {
        m_nCarLess3WheelCounter  = 0;
        m_nTempBufferCounter     = 0;
        m_nCarTwoWheelCounter    = 0;
        m_nBikeRearWheelCounter  = 0;
        m_nBikeFrontWheelCounter = 0;
    };
    if (!ped->bInVehicle || !ped->m_pVehicle) {
        ResetStuntCounters();
    } else {
        const auto veh = ped->m_pVehicle;

        // Служебное: финализация «двух колёс» автомобиля (рекорд с 2 секунд)
        const auto RecordCarTwoWheels = [&] {
            if (m_nCarTwoWheelCounter > 1999) {
                m_fBestCarTwoWheelsDistM  = m_fCarTwoWheelDist;
                m_nBestCarTwoWheelsTimeMs = m_nCarTwoWheelCounter;
                CStats::SetNewRecordStat(STAT_LONGEST_2_WHEELS_TIME, static_cast<float>(m_nCarTwoWheelCounter / 1000));
                CStats::SetNewRecordStat(STAT_LONGEST_2_WHEELS_DISTANCE, m_fCarTwoWheelDist);
            }
            m_fCarTwoWheelDist     = 0.0f;
            m_nTempBufferCounter   = 0;
            m_nCarTwoWheelCounter  = 0;
        };

        if (veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
            const auto automobile = veh->AsAutomobile();

            // Меньше 3 колёс на земле
            if (automobile->GetNumContactWheels() < 3) {
                m_nCarLess3WheelCounter += static_cast<uint32>(CTimer::GetTimeStepInMS());
            } else {
                m_nCarLess3WheelCounter = 0;
            }

            // Два колеса: m_fWheelsSuspensionCompressionPrev == 1.0f = колесо в покое
            const auto& susp = automobile->m_fWheelsSuspensionCompressionPrev;
            if (susp[2] != 1.0f || susp[3] != 1.0f) {
                if (susp[0] == 1.0f && susp[1] == 1.0f) {
                    if (susp[2] < 1.0f && susp[3] < 1.0f && veh->m_fDamageIntensity == 0.0f) {
                        // Едем на стороне {2,3}
                        m_nCarTwoWheelCounter += static_cast<uint32>(CTimer::GetTimeStepInMS());
                        m_fCarTwoWheelDist += veh->m_fMovingSpeed;
                        const auto newTemp = static_cast<float>(m_nTempBufferCounter) - CTimer::GetTimeStepInMS() * 0.5f;
                        m_nTempBufferCounter = newTemp > 0.0f ? static_cast<uint32>(newTemp) : 0;
                    } else if (m_nCarTwoWheelCounter != 0 && m_nTempBufferCounter < 500) {
                        m_nTempBufferCounter += static_cast<uint32>(CTimer::GetTimeStepInMS());
                    } else {
                        RecordCarTwoWheels();
                    }
                } else {
                    // Передняя сторона тоже не в покое
                    if (m_nCarTwoWheelCounter != 0) {
                        RecordCarTwoWheels();
                    }
                }
            } else {
                if (susp[0] >= 1.0f || susp[1] >= 1.0f || veh->m_fDamageIntensity != 0.0f) {
                    if (m_nCarTwoWheelCounter != 0 && m_nTempBufferCounter < 500) {
                        m_nTempBufferCounter += static_cast<uint32>(CTimer::GetTimeStepInMS());
                    } else {
                        RecordCarTwoWheels();
                    }
                } else {
                    // Едем на стороне {0,1}
                    m_nCarTwoWheelCounter += static_cast<uint32>(CTimer::GetTimeStepInMS());
                    m_fCarTwoWheelDist += veh->m_fMovingSpeed;
                    const auto newTemp = static_cast<float>(m_nTempBufferCounter) - CTimer::GetTimeStepInMS() * 0.5f;
                    m_nTempBufferCounter = newTemp > 0.0f ? static_cast<uint32>(newTemp) : 0;
                }
            }
            m_nBikeRearWheelCounter  = 0;
            m_nBikeFrontWheelCounter = 0;
        } else if (veh->m_nVehicleType == VEHICLE_TYPE_BIKE) {
            const auto bike = veh->AsBike();

            // Служебные: рекорды вилли (с 5 секунд) и стоппи (с 2 секунд)
            const auto RecordWheelie = [&] {
                if (m_nBikeRearWheelCounter > 4999) {
                    m_fBestBikeWheelieDistM  = m_fBikeRearWheelDist;
                    m_nBestBikeWheelieTimeMs = m_nBikeRearWheelCounter;
                    CStats::SetNewRecordStat(STAT_LONGEST_WHEELIE_TIME, static_cast<float>(m_nBikeRearWheelCounter / 1000));
                    CStats::SetNewRecordStat(STAT_LONGEST_WHEELIE_DISTANCE, m_fBikeRearWheelDist);
                }
                m_nBikeRearWheelCounter = 0;
                m_fBikeRearWheelDist    = 0.0f;
            };
            const auto RecordStoppie = [&] {
                if (m_nBikeFrontWheelCounter > 1999) {
                    m_fBestBikeStoppieDistM  = m_fBikeFrontWheelDist;
                    m_nBestBikeStoppieTimeMs = m_nBikeFrontWheelCounter;
                    CStats::SetNewRecordStat(STAT_LONGEST_STOPPIE_TIME, static_cast<float>(m_nBikeFrontWheelCounter / 1000));
                    CStats::SetNewRecordStat(STAT_LONGEST_STOPPIE_DISTANCE, m_fBikeFrontWheelDist);
                }
                m_nBikeFrontWheelCounter = 0;
                m_fBikeFrontWheelDist    = 0.0f;
            };
            const auto DecayTempBuffer = [&](float mult) {
                const auto newTemp = static_cast<float>(m_nTempBufferCounter) - CTimer::GetTimeStepInMS() * mult;
                m_nTempBufferCounter    = newTemp > 0.0f ? static_cast<uint32>(newTemp) : 0;
                m_nCarTwoWheelCounter   = 0;
                m_nCarLess3WheelCounter = 0;
            };

            const auto& hist = bike->m_aRatioHistory;
            if (hist[0] == 1.0f && hist[1] == 1.0f && m_nBikeFrontWheelCounter == 0) {
                if (hist[2] < 1.0f || (hist[3] < 1.0f && veh->m_fDamageIntensity == 0.0f)) {
                    // Вилли активен
                    m_nBikeRearWheelCounter += static_cast<uint32>(CTimer::GetTimeStepInMS());
                    m_fBikeRearWheelDist += veh->m_fMovingSpeed;
                    DecayTempBuffer(0.2f);
                } else if (m_nBikeRearWheelCounter == 0 || m_nTempBufferCounter > 499) {
                    // Вилли завершён: рекорд и полный сброс
                    RecordWheelie();
                    m_nTempBufferCounter     = 0;
                    m_nCarTwoWheelCounter    = 0;
                    m_nCarLess3WheelCounter  = 0;
                } else {
                    // Отсрочка: тихий период ещё не истёк
                    m_nTempBufferCounter += static_cast<uint32>(CTimer::GetTimeStepInMS());
                    m_nCarTwoWheelCounter   = 0;
                    m_nCarLess3WheelCounter = 0;
                }
            } else {
                if (m_nBikeRearWheelCounter != 0) {
                    // Вилли был в процессе — финализируем
                    RecordWheelie();
                    m_nTempBufferCounter     = 0;
                    m_nCarTwoWheelCounter    = 0;
                    m_nCarLess3WheelCounter  = 0;
                } else {
                    bool skipBufferReset = false;
                    if (hist[2] == 1.0f && hist[3] == 1.0f) {
                        if (hist[0] < 1.0f || (hist[1] < 1.0f && veh->m_fDamageIntensity == 0.0f)) {
                            // Стоппи активен
                            m_nBikeFrontWheelCounter += static_cast<uint32>(CTimer::GetTimeStepInMS());
                            m_fBikeFrontWheelDist += veh->m_fMovingSpeed;
                            DecayTempBuffer(0.2f);
                            skipBufferReset = true;
                        } else if (m_nBikeFrontWheelCounter != 0 && m_nTempBufferCounter < 500) {
                            m_nTempBufferCounter += static_cast<uint32>(CTimer::GetTimeStepInMS());
                            m_nCarTwoWheelCounter   = 0;
                            m_nCarLess3WheelCounter = 0;
                            skipBufferReset = true;
                        } else {
                            RecordStoppie();
                        }
                    } else {
                        RecordStoppie();
                    }
                    if (!skipBufferReset) {
                        m_nTempBufferCounter     = 0;
                        m_nCarTwoWheelCounter    = 0;
                        m_nCarLess3WheelCounter  = 0;
                    }
                }
            }
        } else {
            ResetStuntCounters();
        }
    }

    WorkOutEnergyFromHunger();

    // Отображаемые деньги догоняют реальные (шаг зависит от разницы)
    if (m_nDisplayMoney != m_nMoney) {
        const auto diff    = m_nMoney - m_nDisplayMoney;
        const auto absDiff = std::abs(diff);
        auto step = absDiff < 100'001
            ? (absDiff < 10'001
                ? (absDiff < 1'001
                    ? (absDiff < 51 ? 1 : 42)
                    : 123)
                : 1'234)
            : 12'345;
        if (diff < 0) {
            step = -step;
        }
        m_nDisplayMoney += step;
    }

    // Максимум здоровья из статов
    m_pPed->m_fMaxHealth = CStats::GetFatAndMuscleModifier(STAT_MOD_MAX_HEALTH);
    m_nMaxHealth = static_cast<uint8>(m_pPed->m_fMaxHealth);

    // Парашют: стримим при высоком навыке полёта в воздушном транспорте
    if (m_pPed->bInVehicle && CStats::GetStatValue(STAT_FLYING_SKILL) >= 400.0f) {
        StreamParachuteWeapon(true);
    } else if (m_bParachuteReferenced && CGameLogic::IsCoopGameGoingOn() && m_bParachuteReferenced) {
        CStreaming::SetModelIsDeletable(MODEL_GUN_PARA);
        m_bParachuteReferenced   = false;
        m_nRequireParachuteTimer = 0;
    }

    // Плотность дорог вокруг игрока (раз в 16 кадров), кламп [0.5, 1.45]
    if ((CTimer::m_FrameCounter & 0xF) == 0) {
        const auto& pos = m_pPed->bInVehicle
            ? m_pPed->m_pVehicle->GetPosition()
            : m_pPed->GetPosition();
        // 0x44EFC0: в репо не реверснуто (только объявление, хук закомменчен)
        m_fRoadDensityAroundPlayer = plugin::CallAndReturn<float, 0x44EFC0, float, float>(pos.x, pos.y);
    }
    m_fRoadDensityAroundPlayer = (m_fRoadDensityAroundPlayer - 1.0f) * 0.6f + 1.0f;
    m_fRoadDensityAroundPlayer = std::max(m_fRoadDensityAroundPlayer, 0.5f);
    m_fRoadDensityAroundPlayer = std::min(m_fRoadDensityAroundPlayer, 1.45f);

    // Вход/выход из машины по кнопкам
    // skipEnterExit == goto LAB_00570aa5: прервать вход/выход, продолжить со следующего блока
    auto skipEnterExit = false;
    if (!pad->GetTarget()
        && ped->bCanExitCar
        && !ped->bUsingMobilePhone
        && (pad->ExitVehicleJustDown()
            || (m_bTryingToExitCar && pad->GetExitVehicle() && ped->bInVehicle))
    ) {
        m_bTryingToExitCar = false;
        if (!ped->bInVehicle) {
            if (!ped->m_pAttachedTo) {
                const auto entity      = CPlayerInfo::FindObjectToSteal(ped);
                const auto activeTask  = ped->GetTaskManager().GetSimplestActiveTask();
                const auto holdTask    = ped->GetIntelligence()->GetTaskHold(false);
                const auto fightTask   = ped->GetIntelligence()->GetTaskFighting();
                if (activeTask->GetTaskType() != TASK_SIMPLE_CLIMB
                    && !fightTask
                    && (!holdTask || !holdTask->m_pEntityToHold)
                ) {
                    if (!entity) {
                        // Сканируем секторы в поисках машины
                        CVehicle* foundVehicle = nullptr;
                        float     foundVehDist = 0.0f;
                        const auto& pos  = ped->GetPosition();
                        const auto minX  = pos.x - 10.0f, maxX = pos.x + 10.0f;
                        const auto minY  = pos.y - 10.0f, maxY = pos.y + 10.0f;
                        const auto SectorOf = [](float c) { return static_cast<int32>(c * 0.02f + 60.0f); };
                        CWorld::AdvanceCurrentScanCode();
                        for (auto y = SectorOf(minY); y <= SectorOf(maxY); y++) {
                            for (auto x = SectorOf(minX); x <= SectorOf(maxX); x++) {
                                FindClosestCarSectorList(CWorld::GetRepeatSector(x, y).Vehicles, ped, minX, minY, maxX, maxY, &foundVehDist, &foundVehicle);
                            }
                        }

                        if (foundVehicle) {
                            // Прицеп — интересуемся тягачом
                            if (foundVehicle->m_nVehicleSubType == VEHICLE_TYPE_TRAILER && foundVehicle->m_pTowingVehicle) {
                                foundVehicle = foundVehicle->m_pTowingVehicle;
                            }
                            if (foundVehicle && foundVehicle->CanBeDriven()) {
                                if (const auto jetpack = ped->GetIntelligence()->GetTaskJetPack()) {
                                    jetpack->DropJetPack(ped);
                                }

                                // Добавить задачу входа (с учётом второго игрока). true = skipEnterExit.
                                const auto AddEnterCarTask = [&]() -> bool {
                                    if (!CWorld::Players[1].m_pPed || CGameLogic::bPlayersCanBeInSeparateCars) {
                                        // Одиночная игра — за руль
                                        ped->GetTaskManager().SetTask(new CTaskComplexEnterCarAsDriver{foundVehicle}, TASK_PRIMARY_PRIMARY, false);
                                        return false;
                                    }
                                    // 2 игрока: если второй уже (собирается) в этой машине — пассажиром
                                    const auto otherPed  = CWorld::Players[(playerIndex + 1) % 2].m_pPed;
                                    const auto& otherMgr = otherPed->GetTaskManager();
                                    // Раскладка CTaskManager (декомпил Flush 0x681850): 5 primary-слотов @+0x0, secondary-массив @+0x14.
                                    // Оригинал читает manager+0x10 = m_aPrimaryTasks[TASK_PRIMARY_DEFAULT]
                                    // и manager+0x14 = m_aSecondaryTasks[TASK_SECONDARY_ATTACK] (там живёт CAR_DRIVE/PLAYER_IN_CAR).
                                    const auto otherDefault = otherMgr.m_aPrimaryTasks[TASK_PRIMARY_DEFAULT];
                                    const auto otherAttack  = otherMgr.m_aSecondaryTasks[TASK_SECONDARY_ATTACK];
                                    const auto TaskTargetCar = [](CTask* t) {
                                        return *reinterpret_cast<CVehicle**>(reinterpret_cast<uintptr_t>(t) + 0xC); // CTaskComplexEnterCar::m_Car
                                    };
                                    auto otherCar        = static_cast<CVehicle*>(nullptr);
                                    auto otherPrimaryCar = static_cast<CVehicle*>(nullptr);
                                    const auto defaultType = otherDefault ? otherDefault->GetTaskType() : TASK_NONE;
                                    const auto attackType  = otherAttack  ? otherAttack->GetTaskType()  : TASK_NONE;
                                    switch (defaultType) {
                                    case TASK_COMPLEX_ENTER_CAR_AS_DRIVER:      // 701
                                    case TASK_COMPLEX_STEAL_CAR:                // 702
                                    case TASK_COMPLEX_DRAG_PED_FROM_CAR:        // 703
                                    case TASK_COMPLEX_ENTER_BOAT_AS_DRIVER:     // 715
                                        otherCar = TaskTargetCar(otherDefault);
                                        break;
                                    default:
                                        break;
                                    }
                                    if (attackType != TASK_SIMPLE_PLAYER_IN_CAR && defaultType != TASK_SIMPLE_PLAYER_IN_CAR // декомпил: тип 1
                                        && attackType != TASK_SIMPLE_CAR_DRIVE && defaultType != TASK_SIMPLE_CAR_DRIVE     // 709
                                    ) {
                                        // otherCar остаётся как есть, otherPrimaryCar = null
                                    } else if (otherPed->m_pVehicle && otherPed->m_pVehicle->m_pDriver == otherPed) {
                                        otherCar = otherPed->m_pVehicle;
                                    }
                                    if (defaultType == TASK_COMPLEX_ENTER_CAR_AS_PASSENGER) { // 700
                                        otherPrimaryCar = TaskTargetCar(otherDefault);
                                    }
                                    if (otherCar == foundVehicle) {
                                        ped->GetTaskManager().SetTask(new CTaskComplexEnterCarAsPassenger{foundVehicle, 0, false}, TASK_PRIMARY_PRIMARY, false);
                                        return false;
                                    }
                                    if (otherPrimaryCar != foundVehicle && (otherCar || otherPrimaryCar)) {
                                        return true;
                                    }
                                    ped->GetTaskManager().SetTask(new CTaskComplexEnterCarAsDriver{foundVehicle}, TASK_PRIMARY_PRIMARY, false);
                                    return false;
                                };

                                if (foundVehicle->m_nVehicleType == VEHICLE_TYPE_BOAT) {
                                    // Лодка с пустым местом за рулём — всегда за руль
                                    if (!foundVehicle->m_pDriver) {
                                        const auto task = ped->GetTaskManager().GetActiveTask();
                                        if (!task || task->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
                                            ped->GetTaskManager().SetTask(new CTaskComplexEnterCarAsDriver{foundVehicle}, TASK_PRIMARY_PRIMARY, false);
                                        }
                                    }
                                } else {
                                    const auto task = ped->GetTaskManager().GetActiveTask();
                                    if (task) {
                                        auto skipAbort = false;
                                        if (task->GetTaskType() == TASK_COMPLEX_IN_WATER) {
                                            if (const auto swim = ped->GetIntelligence()->GetTaskSwim()) {
                                                switch (foundVehicle->m_nModelIndex) {
                                                case MODEL_SKIMMER:
                                                case MODEL_VORTEX:
                                                case MODEL_SEASPAR:
                                                case MODEL_LEVIATHN: {
                                                    CVector doorPos;
                                                    int32   doorId = 0;
                                                    if (CCarEnterExit::GetNearestCarDoor(ped, foundVehicle, doorPos, doorId)) {
                                                        swim->m_vecPos    = doorPos;
                                                        swim->m_nTimeStep = 5000;
                                                        skipAbort = true;
                                                    }
                                                    break;
                                                }
                                                default:
                                                    break;
                                                }
                                            }
                                        }
                                        if (!skipAbort && !task->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
                                            skipEnterExit = true;
                                        } else {
                                            skipEnterExit = AddEnterCarTask();
                                        }
                                    } else {
                                        skipEnterExit = AddEnterCarTask();
                                    }
                                }
                            }
                        }
                    } else {
                        // Перед педом подбираемый объект — идём его подбирать
                        const auto curTask = ped->GetTaskManager().GetActiveTask();
                        if (!curTask || curTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
                            const auto mem = CTask::operator new(sizeof(CTaskComplexGoPickUpEntity));
                            // 0x6919C0: CTaskComplexGoPickUpEntity::ctor(CEntity*, int32 animGroup=0x51) — не реверснут.
                            // (Оригинал берёт указатель задачи из eax после ctor'а; здесь надёжнее — сам mem.)
                            if (mem) {
                                // plugin::CallMethod требует классовый this (requires std::is_class_v) — даём тип задачи:
                                plugin::CallMethod<0x6919C0, CTaskComplexGoPickUpEntity*, CEntity*, int32>(
                                    reinterpret_cast<CTaskComplexGoPickUpEntity*>(mem), entity, 0x51);
                            }
                            CTask* pickupTask = static_cast<CTask*>(mem);
                            CEventScriptCommand event{3, pickupTask, false};
                            CWorld::Players[CWorld::PlayerInFocus].m_pPed->GetEventGroup().Add(&event, false);
                            event.~CEventScriptCommand();
                        }
                    }
                }
            }
        } else if (!m_pRemoteVehicle
            && (!ped->m_pVehicle->m_pEntityWeAreOn
                || !CBridge::ThisIsABridgeObjectMovingUp(ped->m_pVehicle->m_pEntityWeAreOn->m_nModelIndex))
        ) {
            // Выход из машины
            const auto veh    = ped->m_pVehicle;
            const auto status = veh->GetStatus();
            if (status != STATUS_WRECKED
                && status != STATUS_TRAIN_MOVING
                && veh->m_nDoorLock != CARLOCK_LOCKED_PLAYER_INSIDE
            ) {
                const auto task = ped->GetTaskManager().GetActiveTask();
                if (!task || task->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
                    if (veh->m_nVehicleType == VEHICLE_TYPE_BOAT) {
                        ped->GetTaskManager().SetTask(new CTaskComplexLeaveCar{veh, 0, 0, true, false}, TASK_PRIMARY_PRIMARY, false);
                        ped->bTryingToReachDryLand = true;
                    } else if (veh->CanPedStepOutCar(false) || veh->CanPedJumpOutCar(ped)) {
                        ped->GetTaskManager().SetTask(new CTaskComplexLeaveCar{veh, 0, 0, true, false}, TASK_PRIMARY_PRIMARY, false);
                        m_bTryingToExitCar = true;
                        GivePlayerParachute();
                    } else if (status == STATUS_PLAYER) {
                        m_bTryingToExitCar = true;
                    }
                } else {
                    m_bTryingToExitCar = true;
                }
            }
        }
    }

    // (LAB_00570aa5: сюда приходит «прерывание» входа/выхода — просто продолжаем)
    (void)skipEnterExit;

    // Взрыв удалённо управляемой машины: затемнение + рестарт камеры/таймера
    if (m_bAfterRemoteVehicleExplosion) {
        const auto now = CTimer::GetTimeInMS();
        if (CTimer::m_snPreviousTimeInMilliseconds - m_nTimeOfRemoteVehicleExplosion < 1000
            && now - m_nTimeOfRemoteVehicleExplosion > 999
            && m_nPlayerState == PLAYERSTATE_PLAYING // декомпил: (char)param_1[0x37] == 0 → +0xDC = m_nPlayerState
            && m_bFadeAfterRemoteVehicleExplosion
        ) {
            TheCamera.SetFadeColour(0, 0, 0);
            TheCamera.Fade(1.0f, eFadeFlag::FADE_IN);
        }
        if (now - m_nTimeOfRemoteVehicleExplosion > 2000) {
            if (m_nPlayerState == PLAYERSTATE_PLAYING && m_bFadeAfterRemoteVehicleExplosion) { // декомпил: param_1[0x37] == 0
                TheCamera.RestoreWithJumpCut();
                TheCamera.SetFadeColour(0, 0, 0);
                TheCamera.Fade(1.0f, eFadeFlag::FADE_OUT);
                TheCamera.Process();
                CTimer::Stop();
                CRenderer::RequestObjectsInFrustum(nullptr, 0);
                CStreaming::LoadAllRequestedModels(false);
                CTimer::Update();
            }
            m_bAfterRemoteVehicleExplosion = false;

            auto& remoteVehicle = CWorld::Players[CWorld::PlayerInFocus].m_pRemoteVehicle;
            if (remoteVehicle) {
                remoteVehicle->m_bRemoveFromWorld = true;
            }
            remoteVehicle = nullptr;

            const auto focusPed = CWorld::Players[CWorld::PlayerInFocus].m_pPed;
            if (focusPed && focusPed->bInVehicle && focusPed->m_pVehicle) {
                if (focusPed->bInVehicle) {
                    focusPed->m_pVehicle->SetStatus(STATUS_PLAYER);
                } else {
                    focusPed->SetStatus(STATUS_PLAYER);
                }
            }
        }
    }

    // Счётчик «вверх ногами» сфокусированного игрока (раз в 32 кадра) → взрыв машины
    if ((CTimer::m_FrameCounter & 0x1F) == 0) {
        const auto focusPed = CWorld::Players[CWorld::PlayerInFocus].m_pPed;
        const auto focusVeh = (focusPed && focusPed->bInVehicle) ? focusPed->m_pVehicle : nullptr;
        if (!focusPed || !focusPed->bInVehicle || !focusVeh || !m_pPed->bInVehicle) {
            m_nTimesUpsideDownInARow = 0;
        } else if (focusVeh->m_matrix->GetUp().z >= 0.0f || focusVeh->m_vecMoveSpeed.Magnitude() >= 0.05f) {
            m_nTimesUpsideDownInARow = 0;
        } else if (focusVeh->m_nVehicleType != VEHICLE_TYPE_AUTOMOBILE && focusVeh->m_nVehicleType != VEHICLE_TYPE_BOAT) {
            m_nTimesUpsideDownInARow = 0;
        } else if (focusVeh->physicalFlags.bSubmergedInWater) {
            m_nTimesUpsideDownInARow = 0;
        } else {
            m_nTimesUpsideDownInARow += focusVeh->m_matrix->GetUp().z >= -0.5f ? 1 : 2;
        }

        if (m_nTimesUpsideDownInARow > 6) {
            const auto veh = (focusPed && focusPed->bInVehicle) ? focusPed->m_pVehicle : nullptr;
            if (veh && veh->vehicleFlags.bCanBeDamaged) {
                veh->m_fHealth = std::min(veh->m_fHealth, 249.0f);
                if (veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
                    veh->AsAutomobile()->GetDamageManager().SetEngineStatus(225); // 0xE1 — двигатель на грани
                    *reinterpret_cast<uint32*>(reinterpret_cast<uintptr_t>(veh) + 0x928) = 0; // CAutomobile::field_928 (не именовано в репо)
                }
            }
        }
    }

    // Статистика сфокусированного игрока: пешком/плавание/джетпак/транспорт
    {
        const auto focusPed = CWorld::Players[CWorld::PlayerInFocus].m_pPed;
        const auto focusVeh = (focusPed && focusPed->bInVehicle) ? focusPed->m_pVehicle : nullptr;
        if (!focusPed || !focusPed->bInVehicle || !focusVeh) {
            if (focusPed && focusPed->m_fMovingSpeed > 0.0f) {
                if (focusPed->GetIntelligence()->GetTaskSwim()) {
                    // В воде: только пока гребёт (кнопки)
                    if (CPad::GetPad(0)->GetPedWalkLeftRight() || CPad::GetPad(0)->GetAccelerate()) {
                        CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_SWIMMING, focusPed->m_fMovingSpeed);
                    }
                } else if (focusPed->GetIntelligence()->GetTaskJetPack()) {
                    CStats::IncrementStat(STAT_TIME_ON_JETPACK, CTimer::ms_fTimeStep * 16.0f);
                } else {
                    CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_ON_FOOT, focusPed->m_fMovingSpeed);
                }
            }
        } else {
            if (focusVeh->m_nModelIndex == MODEL_CADDY) {
                CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_GOLF_CART, focusVeh->m_fMovingSpeed);
            } else {
                if (focusVeh->m_nVehicleSubType == VEHICLE_TYPE_BMX) {
                    CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_BICYCLE, focusVeh->m_fMovingSpeed);
                }
                if (focusVeh->GetVehicleAppearance() == VEHICLE_APPEARANCE_HELI) {
                    CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_HELICOPTER, focusVeh->m_fMovingSpeed);
                }
                if (focusVeh->GetVehicleAppearance() == VEHICLE_APPEARANCE_PLANE) {
                    CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_PLANE, focusVeh->m_fMovingSpeed);
                }
                if (focusVeh->GetVehicleAppearance() == VEHICLE_APPEARANCE_AUTOMOBILE) {
                    CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_CAR, focusVeh->m_fMovingSpeed);
                }
                if (focusVeh->GetVehicleAppearance() == VEHICLE_APPEARANCE_BIKE) {
                    CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_MOTORBIKE, focusVeh->m_fMovingSpeed);
                }
                if (focusVeh->GetVehicleAppearance() == VEHICLE_APPEARANCE_BOAT) {
                    CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_BOAT, focusVeh->m_fMovingSpeed);
                }
                if ((focusVeh->GetVehicleAppearance() == VEHICLE_APPEARANCE_PLANE
                    || focusVeh->GetVehicleAppearance() == VEHICLE_APPEARANCE_HELI)
                    && focusVeh->m_vecMoveSpeed.Magnitude() > 0.2f
                ) {
                    CStats::IncrementStat(STAT_FLIGHT_TIME, CTimer::ms_fTimeStep * 16.0f);
                }
            }

            if (!FindPlayerTrain(-1)) {
                const auto& spd = focusVeh->m_vecMoveSpeed;
                if (spd.x * spd.x + spd.y * spd.y + spd.z * spd.z > 0.0f) {
                    switch (focusVeh->GetVehicleAppearance()) {
                    case VEHICLE_APPEARANCE_BIKE:
                        if (focusVeh->m_nVehicleSubType != VEHICLE_TYPE_BMX) {
                            CStats::UpdateStatsWhenDriving(focusVeh);
                        }
                        break;
                    case VEHICLE_APPEARANCE_HELI:
                    case VEHICLE_APPEARANCE_PLANE:
                        CStats::UpdateStatsWhenFlying(focusVeh);
                        break;
                    case VEHICLE_APPEARANCE_BOAT:
                        break;
                    default:
                        CStats::UpdateStatsWhenDriving(focusVeh);
                    }
                }
            }
        }
    }

    // Значение погони (m_fCurrentChaseValue)
    {
        const auto wanted = m_pPed->GetPlayerData() ? m_pPed->GetPlayerWanted() : nullptr; // ped+0x480 → m_pWanted (+0x0)
        if (!wanted || wanted->m_WantedLevel == eWantedLevel::WANTED_CLEAN || CTheScripts::IsPlayerOnAMission()) {
            m_fCurrentChaseValue = 0.0f;
        } else {
            if ((Process_ChasePosInitialized_ & 1) == 0) {
                Process_ChasePosInitialized_ |= 1;
            }
            if (CTimer::GetTimeInMS() / 20000 != CTimer::m_snPreviousTimeInMilliseconds / 20000) {
                const auto pos1 = FindPlayerCoors(-1);
                Process_ChasePlayerMovedFar_ =
                    (pos1 - static_cast<const CVector&>(Process_ChaseCachedPlayerPos_)).Magnitude() >= 10.0f;

                const auto pos2 = FindPlayerCoors(-1);
                Process_ChaseCachedPlayerPos_ = pos2;

                const auto pos3 = FindPlayerCoors(-1);
                const auto node = ThePaths.FindNodeClosestToCoors(pos3, PATH_TYPE_VEH, 60.0f, 1, 0, 0, 0, 0);
                // Оригинал проверяет только первое слово адреса (m_wAreaId != -1)
                Process_ChaseNodeFound_ = node.IsAreaValid();
            }

            auto maxChaseDist = 0.0f;
            switch (wanted->m_WantedLevel) {
            case eWantedLevel::WANTED_LEVEL_1: maxChaseDist = 31.0f;   break;
            case eWantedLevel::WANTED_LEVEL_2: maxChaseDist = 62.0f;   break;
            case eWantedLevel::WANTED_LEVEL_3: maxChaseDist = 125.0f;  break;
            case eWantedLevel::WANTED_LEVEL_4: maxChaseDist = 250.0f;  break;
            case eWantedLevel::WANTED_LEVEL_5: maxChaseDist = 500.0f;  break;
            case eWantedLevel::WANTED_LEVEL_6: maxChaseDist = 1000.0f; break;
            default:                           maxChaseDist = 0.0f;    break;
            }

            const auto delta = (maxChaseDist - m_fCurrentChaseValue) * CTimer::ms_fTimeStep * 0.0001f;
            if (delta < 0.0f
                || (Process_ChasePlayerMovedFar_
                    && Process_ChaseNodeFound_
                    && !CCullZones::NoPolice()
                    && !CCullZones::PoliceAbandonCars()
                    && CGame::currArea == eAreaCodes::AREA_CODE_NORMAL_WORLD)
            ) {
                m_fCurrentChaseValue = delta + m_fCurrentChaseValue;
            }
        }
    }

    // Процессинг прицела (вложенный подобъект this+0x158: m_nCrosshairActivated + m_vecCrosshairTarget).
    // 0x56EC80 — не реверснут, класса для подобъекта в репо нет. this = сырая память (&m_nCrosshairActivated),
    // поэтому plugin::CallMethod (требует класс) неприменим — прямой thiscall-каст.
    reinterpret_cast<void(__thiscall*)(void*, uint32, CPad*)>(0x56EC80)(&m_nCrosshairActivated, playerIndex, pad);

    m_nMoney        = std::min(m_nMoney, 999'999'999);
    m_nDisplayMoney = std::min(m_nDisplayMoney, 999'999'999);
}

// ============================================================================
// STATUS — финальный блок (что осталось на plugin::Call, нереверснуто в репо):
//
//  1) 0x56EC80 (plugin::CallMethod, this = &m_nCrosshairActivated == this+0x158)
//     Метод вложенного подобъекта «прицел» (m_nCrosshairActivated + m_vecCrosshairTarget, 12 байт).
//     Принимает (uint32 playerIndex, CPad* pad). В репо класса нет, вызов оставлен как есть.
//  2) 0x6919C0 (plugin::CallMethod, this = CTask::operator new(0x34))
//     CTaskComplexGoPickUpEntity::ctor(CEntity* entity, int32 animGroup = 0x51).
//     Класс и размер (0x34) подтверждены (vtable 0x870B98, GetTaskType → 310 = TASK_COMPLEX_GO_PICKUP_ENTITY),
//     конструктор не реверснут. 0x51 кладётся в поле m_nAnimGroupId (+0x2C).
//  3) 0x44EFC0 (plugin::CallAndReturn<float>)
//     CPathFind::CalcRoadDensity(float x, float y). Объявлена в PathFind.h:238, тела/хука нет
//     (закомментирован в PathFind.cpp:84).
//
//  Всё остальное (≈45 вызовов) резолвится в реверснутый код репо напрямую:
//  CPad::GetPad/GetTarget/ExitVehicleJustDown/GetExitVehicle/GetPedWalkLeftRight/GetAccelerate,
//  CTimer::GetTimeInMS/GetTimeStepInMS/Stop/Update/m_FrameCounter/ms_fTimeStep/m_snPreviousTimeInMilliseconds,
//  CStats::GetFatAndMuscleModifier/GetStatValue/IncrementStat/SetNewRecordStat/UpdateStatsWhenDriving/
//  UpdateStatsWhenFlying, CStreaming::SetModelIsDeletable/LoadAllRequestedModels, CGameLogic::IsCoopGameGoingOn/
//  bPlayersCanBeInSeparateCars, CReplay::Mode, CPlayerInfo::FindObjectToSteal/FindClosestCarSectorList/
//  WorkOutEnergyFromHunger/StreamParachuteWeapon/GivePlayerParachute, CWorld::GetRepeatSector/
//  AdvanceCurrentScanCode/Players/PlayerInFocus, FindPlayerCoors/FindPlayerTrain, CPathFind::FindNodeClosestToCoors,
//  CBridge::ThisIsABridgeObjectMovingUp, CTheScripts::IsPlayerOnAMission, CCullZones::NoPolice/PoliceAbandonCars,
//  TheCamera::Fade/RestoreWithJumpCut/SetFadeColour/Process, CRenderer::RequestObjectsInFrustum,
//  CTaskManager::GetSimplestActiveTask/GetActiveTask/SetTask, CPedIntelligence::GetTaskSwim/GetTaskJetPack/
//  GetTaskHold/GetTaskFighting, CTaskSimpleJetPack::DropJetPack, CCarEnterExit::GetNearestCarDoor,
//  CTaskComplexEnterCarAsDriver/AsPassenger/LeaveCar (ctor'ы), CEventScriptCommand + CEventGroup::Add,
//  CVehicle::CanBeDriven/CanPedStepOutCar/CanPedJumpOutCar/GetVehicleAppearance, CVector::Magnitude,
//  CDamageManager::SetEngineStatus, CGame::currArea.
// ============================================================================

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
