// NOTE(integration, волна 5): сняты STUB_BLOCKED-редиректы 0x64FC10 и 0x6513A0
// (фрагменты batch_out/fresh/0x64FC10.cpp / 0x6513A0.cpp, верификация VERIFY-wf5a).
// Include-добавления: RenderWare/rw/rtslerp.h и PedPlacement.h (остальное — уже в StdInc.h).
#include "StdInc.h"

#include "TaskUtilityLineUpPedWithCar.h"
#include "RenderWare/rw/rtslerp.h"
#include "PedPlacement.h"

void CTaskUtilityLineUpPedWithCar::InjectHooks() {
    RH_ScopedClass(CTaskUtilityLineUpPedWithCar);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x64FBB0);
    RH_ScopedInstall(Destructor, 0x64FC00);

    RH_ScopedInstall(GetLocalPositionToOpenCarDoor, 0x64FC10); // STUB_BLOCKED снят (волна 5, 0x64FC10.cpp)
    RH_ScopedInstall(GetPositionToOpenCarDoor, 0x650A80);
    RH_ScopedInstall(ProcessPed, 0x6513A0); // STUB_BLOCKED снят (волна 5, 0x6513A0.cpp)
}

// 0x64FBB0
CTaskUtilityLineUpPedWithCar::CTaskUtilityLineUpPedWithCar(const CVector& offset, int32 time, int32 doorOpenPosType, int32 doorIdx) {
    m_Offset = offset;
    m_fDoorOpenPosZ = -999.99f;
    m_fTime = time;
    m_nDoorOpenPosType = doorOpenPosType;
    m_nDoorIdx = doorIdx;
}

// The following function seems to have copy elision on the returned CVector, that the compiled function
// took a vector ptr as their first arg. Now, hopefully our code will compile to the same stuff.
// If not, it might crash here, in that case a wrapper function should be used.

// 0x64FC10 (волна 5, фрагмент 0x64FC10.cpp; прежний STUB_BLOCKED снят — смещения handling+0xD4/+0xDE
// бьются с m_fSeatOffsetDistance/m_nAnimGroup репо)
CVector CTaskUtilityLineUpPedWithCar::GetLocalPositionToOpenCarDoor(CVehicle* vehicle, float animProgress, CAnimBlendAssociation* assoc) {
    const auto* modelInfo = CModelInfo::GetVehicleModelInfo(vehicle->m_nModelIndex); // 0xA9B0C8[veh+0x22]

    // Прогресс зануляется для задних дверей фургонов
    if (!vehicle->vehicleFlags.bIsVan || (m_nDoorOpenPosType != 11 && m_nDoorOpenPosType != 9)) {
        animProgress = animProgress * vehicle->m_pHandlingData->m_fSeatOffsetDistance;
    } else {
        animProgress = 0.0f;
    }

    // Тип смещения двери по ID анимации (маркер (int)vehicle == «решить по m_nDoorOpenPosType»):
    //   109 (CRAWLOUTRHS_1), 376 (GETOUT_RHS_1), 380-383 (CLOSE), 387/388 (FALLOUT) -> 3 (EXIT_FRONT)
    //   378/379 (JACKED) -> 5, 389/390 (DOORLOCKED) -> 0, прочее -> маркер
    int doorOffsetId = assoc ? (int)(uintptr_t)vehicle : 0;
    if (assoc) {
        const auto animId = (AnimationId)assoc->m_AnimId;
        if (animId >= ANIM_ID_CAR_CLOSE_LHS_0) { // >= 380
            switch (animId) {
            case ANIM_ID_CAR_CLOSE_LHS_0:
            case ANIM_ID_CAR_CLOSE_RHS_0:
            case ANIM_ID_CAR_CLOSE_LHS_1:
            case ANIM_ID_CAR_CLOSE_RHS_1:
            case ANIM_ID_CAR_FALLOUT_LHS:
            case ANIM_ID_CAR_FALLOUT_RHS:
                doorOffsetId = 3;
                break;
            case ANIM_ID_CAR_DOORLOCKED_LHS:
            case ANIM_ID_CAR_DOORLOCKED_RHS:
                doorOffsetId = 0;
                break;
            default:
                break;
            }
        } else if (animId >= ANIM_ID_CAR_JACKEDLHS) { // 378..379
            doorOffsetId = 5;
        } else if (animId <= ANIM_ID_KILL_KNIFE_PED_DIE) { // <= 350
            if (animId == ANIM_ID_DEFAULT_CAR_CRAWLOUTRHS_1) {
                doorOffsetId = 3;
            }
        } else if (animId == ANIM_ID_CAR_GETOUT_RHS_1) { // 373..377
            doorOffsetId = 3;
        }
    }

    // Уточнение по m_nDoorOpenPosType
    if (doorOffsetId == 0 || doorOffsetId == 3) {
        if (m_nDoorOpenPosType == 10 || m_nDoorOpenPosType == 8) {
            doorOffsetId = 0;                    // ENTER_FRONT
        } else if (m_nDoorOpenPosType == 11 || m_nDoorOpenPosType == 9) {
            doorOffsetId = 1;                    // ENTER_REAR
        } else if (m_nDoorOpenPosType == 0x12) { // 18
            doorOffsetId = 2;                    // ENTER_BIKE_FRONT
        }
    } else if (doorOffsetId == 5) {
        doorOffsetId = 0;
    }

    const auto& handling = *vehicle->m_pHandlingData;

    // Смещение двери из группы анимов; для DOORLOCKED-анимаций при bRunSpecialLockedDoor
    // оригинал дважды берёт оффсет и интерполирует по m_fBlendAmount (обо раза — одно значение,
    // оффсет мемоизирован; каллы сохранены как в оригинале)
    float doorOffX, doorOffY, doorOffZ;
    if (!assoc
        || ((AnimationId)assoc->m_AnimId != ANIM_ID_CAR_DOORLOCKED_LHS && (AnimationId)assoc->m_AnimId != ANIM_ID_CAR_DOORLOCKED_RHS)
        || !CVehicleAnimGroupData::m_vehicleAnimGroups[handling.m_nAnimGroup].m_specialFlags.bRunSpecialLockedDoor) {
        const auto off = CVehicleAnimGroupData::GetAnimDoorOffset(handling.GetAnimGroupId(), (eVehAnimDoorOffset)doorOffsetId);
        doorOffX = off.x;
        doorOffY = off.y;
        doorOffZ = off.z;
    } else {
        const auto t = assoc->m_BlendAmount;

        auto off = CVehicleAnimGroupData::GetAnimDoorOffset(handling.GetAnimGroupId(), (eVehAnimDoorOffset)doorOffsetId);
        doorOffX = off.x * (1.0f - t);
        doorOffY = off.y * (1.0f - t);
        doorOffZ = off.z * (1.0f - t);

        off = CVehicleAnimGroupData::GetAnimDoorOffset(handling.GetAnimGroupId(), (eVehAnimDoorOffset)doorOffsetId);
        doorOffX += off.x * t;
        doorOffY += off.y * t;
        doorOffZ += off.z * t;
    }

    // Позиция «сиденья» из дамми модели: SEAT_FRONT, для лодок — LIGHT_FRONT_MAIN
    const auto GetSeat = [&]() -> const CVector& {
        return modelInfo->m_pVehicleStruct->m_avDummyPos[
            modelInfo->m_nVehicleType != VEHICLE_TYPE_BOAT ? DUMMY_SEAT_FRONT : DUMMY_LIGHT_FRONT_MAIN
        ];
    };

    float x{}, y{}, z{};
    switch (m_nDoorOpenPosType) {
    case 8:
    case 9: { // 9 читает SEAT_REAR напрямую
        const auto& seat = (m_nDoorOpenPosType == 9)
            ? modelInfo->m_pVehicleStruct->m_avDummyPos[DUMMY_SEAT_REAR]
            : GetSeat();
        doorOffX = -doorOffX; // LAB_0064ff2b: local_30 = -local_30
        x = seat.x + animProgress + doorOffX; // (x + progress) - (-doorOffX)
        y = seat.y - doorOffY;
        z = seat.z - doorOffZ;
        break;
    }
    case 10: {
        const auto& seat = GetSeat();
        x = -(seat.x + animProgress) - doorOffX; // LAB_0064ff45 без инверсии X
        y = seat.y - doorOffY;
        z = seat.z - doorOffZ;
        break;
    }
    case 11: { // SEAT_REAR, X инвертирован
        const auto& seat = modelInfo->m_pVehicleStruct->m_avDummyPos[DUMMY_SEAT_REAR];
        x = -(seat.x + animProgress) - doorOffX;
        y = seat.y - doorOffY;
        z = seat.z - doorOffZ;
        break;
    }
    case 0x12: { // 18: байк — оффсеты ПРИБАВЛЯЮТСЯ
        const auto& seat = GetSeat();
        x = doorOffX + seat.x;
        y = seat.y + doorOffY;
        z = seat.z + doorOffZ;
        break;
    }
    default: { // без смещений двери
        const auto& seat = GetSeat();
        x = seat.x;
        y = seat.y;
        z = seat.z;
        doorOffX = doorOffY = doorOffZ = 0.0f;
        break;
    }
    }

    // Для байков (кроме типа 18) позиция корректируется по геометрии байка и переводится
    // из мировых координат в систему машины. Прочие типы возвращают результат как есть.
    CVector localPos{ x, y, z };
    if (vehicle->m_nVehicleType == VEHICLE_TYPE_BIKE && m_nDoorOpenPosType != 0x12) {
        CVector worldPos;
        // FIX(integration): GetCorrectedWorldDoorPosition — НЕстатический член CBike
        // (Bike.h:154, 0x6BF230 thiscall); в ветке выше машина уже проверена как байк.
        reinterpret_cast<CBike*>(vehicle)->GetCorrectedWorldDoorPosition(worldPos, CVector{ doorOffX, doorOffY, doorOffZ }, CVector{ x, y, z });
        localPos = vehicle->m_matrix->InverseTransformVector(worldPos - vehicle->GetPosition());
    }
    return localPos;
}

// 0x650A80 (batch w2-17-mixed, fragment 0x650A80.cpp)
// Original returns the CVector via sret (result ptr as 1st arg); helpers 0x59BCF0/0x59C790/0x59ACD0
// are matrix push/transform/pop == TransformPoint. Hopefully this compiles to the same stuff (see note above).
CVector CTaskUtilityLineUpPedWithCar::GetPositionToOpenCarDoor(CVehicle* vehicle, float animProgress, CAnimBlendAssociation* assoc) {
    return vehicle->GetMatrix().TransformPoint(GetLocalPositionToOpenCarDoor(vehicle, animProgress, assoc));
}

// 0x6513A0 (волна 5, фрагмент 0x6513A0.cpp; ~690 строк декомпила, спорные места сняты дизасмом exe)
bool CTaskUtilityLineUpPedWithCar::ProcessPed(CPed* ped, CVehicle* vehicle, CAnimBlendAssociation* assoc) {
    const auto GetAnimProgress = [&] {
        return assoc->m_CurrentTime / assoc->m_BlendHier->m_fTotalTime;
    };

    // 0x6513C0
    if (m_nDoorOpenPosType == 0) {
        ped->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f); // +0x44..0x4C
    }

    // 0x651407: целевой поворот педа (heading машины с поправкой на дверь)
    bool bSimplePath = false;
    const auto vehHeading = vehicle->GetHeading(); // в оригинале инлайн: atan2(-fwd.x, fwd.y) / placement.m_fHeading
    if (vehicle->m_matrix->GetUp().z > -0.8f) { // 0x858C9C == -0.8f: авто стоит на колёсах
        if (m_nDoorIdx == TARGET_DOOR_UNK /*18*/) {
            ped->m_fAimingRotation = vehHeading + 3.14159274f;  // PI (0x858CB8)
        } else if (m_nDoorOpenPosType != 2) {
            ped->m_fAimingRotation = vehHeading;
        }
        // m_nDoorOpenPosType == 2: поворот педа не трогаем (asm 0x651495: je 0x6514B2 — мимо записи +0x55C)
    } else { // перевёрнута / на боку
        bSimplePath = true;
        if (m_nDoorIdx == TARGET_DOOR_FRONT_RIGHT /*8*/ || m_nDoorIdx == TARGET_DOOR_REAR_RIGHT /*9*/) {
            ped->m_fAimingRotation = vehHeading - 3.14159274f;
        } else {
            ped->m_fAimingRotation = vehHeading;
        }
    }

    // 0x6514B2: прогресс открытия двери (doorOpenFactor) и фактор позиции (posFactor) — по анимации
    float doorOpenFactor = 0.0f;
    float posFactor      = 0.0f;

    // Нереверснутая: тайминг in/out-анимации для группы машины (cdecl, (uint8 группа, uint16 animId) -> float)
    const auto vehAnimGroupIdx = vehicle->m_pHandlingData->m_nAnimGroup; // tHandlingData+0xDE
    const auto InOutTiming = [&](AnimationId animId) {
        return plugin::CallAndReturn<float, 0x64E4B0, uint8, uint16>(vehAnimGroupIdx, static_cast<uint16>(animId));
    };

    const AnimationId animId = assoc ? assoc->GetAnimId() : static_cast<AnimationId>(0);

    if (assoc) {
        const auto progress = GetAnimProgress();

        switch (animId) {
        case ANIM_ID_DEFAULT_CAR_CRAWLOUTRHS_0: // 0x6C
        case ANIM_ID_DEFAULT_CAR_CRAWLOUTRHS_1: // 0x6D
            doorOpenFactor = progress;
            break;

        case ANIM_ID_CAR_ALIGN_LHS:   // 0x15F
        case ANIM_ID_CAR_ALIGN_RHS:   // 0x160
        case ANIM_ID_CAR_ALIGNHI_LHS: // 0x161
        case ANIM_ID_CAR_ALIGNHI_RHS: { // 0x162
            doorOpenFactor = 1.0f;
            const auto timing = InOutTiming(animId);
            if (std::abs(timing) >= 10.0f) { // 0x85862C
                const auto absTiming = std::abs(timing) - 11.0f; // 0x858FBC
                posFactor = progress;
                if (timing - 11.0f <= 0.0f) {
                    posFactor = (absTiming <= posFactor) ? (posFactor - absTiming) / (1.0f - absTiming) : 0.0f;
                } else {
                    posFactor = (absTiming <= posFactor) ? 1.0f : posFactor / absTiming;
                }
            } else {
                posFactor = 0.0f;
            }
            if (vehicle->m_nModelIndex == MODEL_AT400 /* 0x241 = 577; +0x22 */) {
                posFactor = 1.0f;
            }
            break;
        }
        // FIX(verify): 0x163-0x166 (CAR_OPEN_*) в декомпиле — ОТДЕЛЬНЫЙ кейс с простой
        // формулой (строки 198-210 декомпила), а не общая с CAR_ALIGN сложная.
        case ANIM_ID_CAR_OPEN_LHS:    // 0x163
        case ANIM_ID_CAR_OPEN_RHS:    // 0x164
        case ANIM_ID_CAR_OPEN_LHS_1:  // 0x165
        case ANIM_ID_CAR_OPEN_RHS_1: { // 0x166
            doorOpenFactor = 1.0f;
            posFactor = std::abs(InOutTiming(animId)) >= 10.0f ? 1.0f : 0.0f;
            if (vehicle->m_nModelIndex == MODEL_AT400 /* 0x241 = 577; +0x22 */) {
                posFactor = 1.0f;
            }
            break;
        }
        case ANIM_ID_CAR_GETIN_LHS_0: // 0x167
        case ANIM_ID_CAR_GETIN_RHS_0: // 0x168
        case ANIM_ID_CAR_GETIN_LHS_1: // 0x169
        case ANIM_ID_CAR_GETIN_RHS_1: { // 0x16A
            doorOpenFactor = 1.0f - progress;
            const auto timing   = InOutTiming(animId);
            const auto absTiming = std::abs(timing);
            if (absTiming >= 10.0f) {
                posFactor = 1.0f;
            } else {
                posFactor = progress;
                if (timing <= 0.0f) {
                    posFactor = (absTiming <= posFactor) ? (posFactor - absTiming) / (1.0f - absTiming) : 0.0f;
                } else {
                    posFactor = (absTiming <= posFactor) ? 1.0f : posFactor / absTiming;
                }
            }
            if (vehicle->m_nModelIndex == MODEL_AT400 /* 0x241 = 577; +0x22 */) {
                posFactor = 1.0f;
            }
            break;
        }
        case ANIM_ID_CAR_PULLOUT_LHS: // 0x16C
        case ANIM_ID_CAR_PULLOUT_RHS: // 0x16D
        case ANIM_ID_UNKNOWN_15:      // 0x16E
            doorOpenFactor = 1.0f;
            posFactor = 0.0f;
            break;

        case ANIM_ID_CAR_CLOSEDOOR_LHS_0: // 0x16F
        case ANIM_ID_CAR_CLOSEDOOR_RHS_0: // 0x170
        case ANIM_ID_CAR_CLOSEDOOR_LHS_1: // 0x171
        case ANIM_ID_CAR_CLOSEDOOR_RHS_1: // 0x172
        case ANIM_ID_CAR_SHUFFLE_RHS_0:   // 0x173
        case ANIM_ID_CAR_SHUFFLE_RHS_1:   // 0x174
            doorOpenFactor = 0.0f;
            posFactor = 1.0f;
            break;

        case ANIM_ID_CAR_JACKEDLHS:   // 0x17A
        case ANIM_ID_CAR_JACKEDRHS:   // 0x17B
        case ANIM_ID_CAR_ROLLOUT_LHS: // 0x180
        case ANIM_ID_CAR_ROLLOUT_RHS: // 0x181
        case ANIM_ID_CAR_FALLOUT_LHS: // 0x183
        case ANIM_ID_CAR_FALLOUT_RHS: { // 0x184
            doorOpenFactor = progress;
            // В оригинале перед этим есть ещё холостой вызов CVehicleAnimGroup::GetGroup(animId)
            // с тем же this (asm 0x651802/0x65180A, ECX один и тот же) — результат отброшен, опущен.
            const auto blendTime = CVehicleAnimGroupData::GetVehicleAnimGroup(vehAnimGroupIdx).ComputeCriticalBlendTime(animId);
            const auto absBlend  = std::abs(blendTime);
            if (blendTime <= 0.0f) {
                if (progress < absBlend) {
                    posFactor = 1.0f;              // ранний выход, без инверсии ниже
                } else {
                    posFactor = 1.0f - (progress - absBlend) / (1.0f - absBlend);
                }
            } else {
                if (progress >= absBlend) {
                    posFactor = 0.0f;              // ранний выход, без инверсии ниже
                } else {
                    posFactor = 1.0f - progress / absBlend;
                }
            }
            break;
        }
        case ANIM_ID_CAR_CLOSE_LHS_0: // 0x17C
        case ANIM_ID_CAR_CLOSE_RHS_0: // 0x17D
        case ANIM_ID_CAR_CLOSE_LHS_1: // 0x17E
        case ANIM_ID_CAR_CLOSE_RHS_1: // 0x17F
            doorOpenFactor = 1.0f;
            posFactor = 0.0f;
            break;

        case ANIM_ID_CAR_ROLLDOOR: // 0x182
            doorOpenFactor = 0.0f;
            posFactor = 1.0f;
            break;

        case ANIM_ID_CAR_DOORLOCKED_LHS: // 0x185
        case ANIM_ID_CAR_DOORLOCKED_RHS: { // 0x186
            doorOpenFactor = 1.0f;
            // asm 0x6517xx: dword по (0xC1CDC8 + группа*0x94) >> 5 & 1 == CVehicleAnimGroup::m_animFlags.bCanPulloutPed
            posFactor = CVehicleAnimGroupData::GetVehicleAnimGroup(vehAnimGroupIdx).m_animFlags.bCanPulloutPed ? 1.0f : 0.0f;
            break;
        }
        }
    }

    // 0x65151B: позиция цели
    CVector targetPos;
    if (m_nDoorOpenPosType == 2) {
        targetPos = ped->GetPosition();
    } else {
        targetPos = GetPositionToOpenCarDoor(vehicle, doorOpenFactor, assoc); // 0x650A80
    }

    // Финальная позиция (при полностью открытой двери)
    CVector finalPos = targetPos;
    if (vehicle->m_nVehicleType != VEHICLE_TYPE_BIKE && m_nDoorOpenPosType != 2) {
        finalPos = GetPositionToOpenCarDoor(vehicle, 1.0f, assoc);
    }

    // 0x651590-0x651A3D: не даём цели провалиться под землю
    if (!(vehicle->m_nPhysicalFlags & PHYSICAL_SUBMERGED_IN_WATER)) { // +0x40 & 0x100
        const auto vehPos = vehicle->GetPosition();
        // FIX(verify): в оригинале (строки 335-363 декомпила) при m_matrix == nullptr
        // right-вектор строится из heading плейсмента: (cos h, sin h, 0).
        const CVector vehRight = vehicle->m_matrix
            ? vehicle->m_matrix->GetRight()
            : CVector{ std::cos(vehHeading), std::sin(vehHeading), 0.0f };
        // порог = right.z * dot(target - vehPos, right) + vehPos.z - GetHeightAboveRoad() + 1.0f
        const auto threshold = vehRight.z * vehRight.Dot(finalPos - vehPos)
                             + vehPos.z - vehicle->GetHeightAboveRoad() + 1.0f; // 0x858624 == 1.0f
        const auto zBefore = finalPos.z;
        finalPos = CPedPlacement::FindZCoorForPed(finalPos).first; // 0x616920 (рефакторенная версия репо;
                                                                   // в оригинале при «не нашли» z := -100, что
                                                                   // эквивалентно откату через проверку ниже)
        if (finalPos.z < threshold - 0.5f) { // 0.5f == 0x858B8C
            finalPos.z = zBefore;
        }
    } else if (vehicle->m_nVehicleType == VEHICLE_TYPE_BOAT && vehicle->IsUpsideDown()) { // 0x651A1B
        finalPos.z += 1.0f;
    }
    m_fDoorOpenPosZ = finalPos.z; // 0x651A41: [this+0xC]

    // 0x651A48: вертикальная скорость педа
    float useZ = finalPos.z;
    if (m_nDoorOpenPosType == 1 || m_nDoorOpenPosType == 2) {
        const auto speedZ = ped->m_vecMoveSpeed.z - CTimer::ms_fTimeStep * 0.008f; // 0xB7CB5C * [0x863984]
        if (finalPos.z <= ped->GetPosition().z + speedZ) {
            ped->m_vecMoveSpeed.z = speedZ;
            useZ = ped->GetPosition().z;
        } else {
            useZ = finalPos.z;
            ped->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
        }
    }

    // 0x651B50: подъём/спуск к дверному проёму
    if (m_fDoorOpenPosZ > useZ) {
        float rise;
        if (vehicle->m_nVehicleType == VEHICLE_TYPE_BIKE && assoc && animId != ANIM_ID_CAR_GETIN_BIKE_FRONT /*0x16B*/) {
            if (animId == ANIM_ID_CAR_GETOUT_LHS_0 || animId == ANIM_ID_CAR_GETOUT_RHS_0
                || animId == ANIM_ID_CAR_GETOUT_LHS_1 || animId == ANIM_ID_CAR_GETOUT_RHS_1) { // 0x175..0x178
                rise = 1.0f - doorOpenFactor;
            } else if (animId == ANIM_ID_CAR_GETIN_LHS_0 || animId == ANIM_ID_CAR_GETIN_RHS_0
                    || animId == ANIM_ID_CAR_GETIN_LHS_1 || animId == ANIM_ID_CAR_GETIN_RHS_1) { // 0x167..0x16A
                const auto p = GetAnimProgress();
                rise = (p + p <= 1.0f) ? (p + p) : 1.0f;
            } else {
                rise = 0.0f;
            }
            rise = (vehicle->GetPosition().z - vehicle->GetHeightAboveRoad() + 1.0f - m_fDoorOpenPosZ) * rise;
        } else {
            rise = (useZ - m_fDoorOpenPosZ) * posFactor;
        }
        useZ = m_fDoorOpenPosZ + rise;
    } else if (m_nDoorOpenPosType == 0) {
        useZ = m_fDoorOpenPosZ + (useZ - m_fDoorOpenPosZ) * posFactor;
    }
    // (m_nDoorOpenPosType != 0 && m_fDoorOpenPosZ <= useZ): useZ не меняется

    // 0x651BC0: пока дверь открывается (m_fTime — дедлайн), сдвигаем цель на m_Offset
    // и доводим текущий поворот педа до целевого
    if (CTimer::GetTimeInMS() < static_cast<uint32>(m_fTime)) { // 0xB7CB84; беззнаковое сравнение
        auto       aim = CGeneral::LimitRadianAngle(ped->m_fAimingRotation);
        const auto cur = ped->m_fCurrentRotation;

        float t = static_cast<float>(static_cast<int32>(m_fTime) - static_cast<int32>(CTimer::GetTimeInMS()));
        if (t < 0.0f) {
            t += 4294967296.0f; // 2^32 (0x858C54)
        }
        t *= 0.00166667f; // 1/600 (0x865060)

        if (t <= 0.0f) {
            m_Offset.x = 0.0f;
            m_Offset.y = 0.0f;
        }
        m_Offset.z = 0.0f;

        targetPos.x -= t * m_Offset.x;
        targetPos.y -= t * m_Offset.y;
        targetPos.z -= t * m_Offset.z;

        if (aim <= cur + 3.14159274f) { // +PI
            if (aim < cur - 3.14159274f) {
                aim += 6.28318548f; // +2PI (0x858CBC)
            }
        } else {
            aim -= 6.28318548f;
        }
        ped->m_fCurrentRotation = cur - (1.0f - t) * (cur - aim);
    } else {
        ped->m_fCurrentRotation = ped->m_fAimingRotation;
    }

    // 0x651D20: анимации посадки/высадки — позиционируем педа матрицей, поворот — slerp
    // из ориентации машины к ориентации педа по прогрессу анимации
    if (assoc
        && ((animId >= ANIM_ID_CAR_GETIN_LHS_0 && animId <= ANIM_ID_CAR_GETIN_BIKE_FRONT)     // 0x167..0x16B
            || (animId >= ANIM_ID_CAR_GETOUT_LHS_0 && animId <= ANIM_ID_UNKNOWN_26)))         // 0x175..0x179
    {
        const auto bGetOut = animId >= ANIM_ID_CAR_GETOUT_LHS_0;

        CMatrix vehMat = *vehicle->m_matrix; // 0x59BCF0 (copy ctor)
        if (!bGetOut && animId == ANIM_ID_CAR_GETIN_BIKE_FRONT) { // 0x16B — на мотоцикл садимся «через» борт
            CMatrix rot;
            rot.SetRotateZ(3.14159274f); // 0x59B0E0(0x40490FDB)
            vehMat *= rot;               // 0x411A80
        }
        RwMatrix rwVeh{};
        vehMat.CopyToRwMatrix(&rwVeh); // 0x59B8B0

        CMatrix pedMat = *ped->m_matrix; // 0x59BCF0
        if (bGetOut) {
            pedMat.SetRotateZOnly(ped->m_fCurrentRotation); // 0x59B020 (asm 0x65226A: +0x558)
        }
        RwMatrix rwPed{};
        pedMat.CopyToRwMatrix(&rwPed);

        RtQuat qVeh{}, qPed{}, qRes{};
        RtQuatConvertFromMatrix(&qVeh, &rwVeh); // 0x7EB5C0
        RtQuatConvertFromMatrix(&qPed, &rwPed);

        const auto t = GetAnimProgress();
        RtQuatSlerpCache slerpCache;
        // FIX(verify): направление slerp различается: декомпил getin (строка 495) зовёт
        // SetupSlerpCache(qPed, qVeh) — t=0 даёт ориентацию педа; getout (строка 580) —
        // (qVeh, qPed) — t=0 даёт ориентацию машины. Прежний код всегда (qVeh, qPed).
        if (bGetOut) {
            RtQuatSetupSlerpCache(&qVeh, &qPed, &slerpCache); // 0x7EC220
        } else {
            RtQuatSetupSlerpCache(&qPed, &qVeh, &slerpCache);
        }

        // == инлайн RtQuatSlerpMacro (см. rtslerp.h) ==
        if (t <= 0.0f) {
            qRes = qVeh;
        } else if (t >= 1.0f) {
            qRes = qPed;
        } else {
            auto wFrom = 1.0f - t;
            auto wTo   = t;
            if (!slerpCache.nearlyZeroOm) {
                // RwSinMinusPiToPiMacro — ряд для синуса, коэффициенты из exe
                auto a  = wFrom * slerpCache.omega;
                auto a2 = a * a;
                wFrom = ((((a2 * 1.58969102e-10f /*0x86D2B0*/ - 2.50507597e-08f /*0x86D2AC*/) * a2 + 2.75573143e-06f /*0x86D2A8*/) * a2 - 1.98412701e-04f /*0x86D2A4*/) * a2 + 8.33333377e-03f /*0x864E0C*/) * a2 - 1.66666672e-01f /*0x85F0A0*/;
                wFrom = wFrom * a2 * a + a;
                auto b  = wTo * slerpCache.omega;
                auto b2 = b * b;
                wTo = ((((b2 * 1.58969102e-10f - 2.50507597e-08f) * b2 + 2.75573143e-06f) * b2 - 1.98412701e-04f) * b2 + 8.33333377e-03f) * b2 - 1.66666672e-01f;
                wTo = wTo * b2 * b + b;
            }
            qRes.imag.x = slerpCache.raTo.imag.x * wTo + slerpCache.raFrom.imag.x * wFrom;
            qRes.imag.y = slerpCache.raTo.imag.y * wTo + slerpCache.raFrom.imag.y * wFrom;
            qRes.imag.z = slerpCache.raTo.imag.z * wTo + slerpCache.raFrom.imag.z * wFrom;
            qRes.real   = slerpCache.raFrom.real * wFrom + slerpCache.raTo.real * wTo;
        }

        // Нормализация (scale = 2/|q|^2) и сборка матрицы поворота
        const auto s = 2.0f / (sq(qRes.imag.x) + sq(qRes.imag.y) + sq(qRes.imag.z) + sq(qRes.real));
        const auto xs = qRes.imag.x * s;
        const auto ys = qRes.imag.y * s;
        const auto zs = qRes.imag.z * s;
        const auto wx = xs * qRes.real;
        const auto wy = ys * qRes.real;
        const auto wz = zs * qRes.real;
        const auto yz = zs * qRes.imag.y;

        RwMatrix rwRes{};
        rwRes.right.x = 1.0f - (qRes.imag.z * zs + ys * qRes.imag.y); // 1 - 2(y^2+z^2)/|q|^2
        rwRes.right.y = wz + ys * qRes.imag.x;                        // 2(wz + xy)
        rwRes.right.z = xs * qRes.imag.z - wy;                        // 2(xz - wy)
        rwRes.up.x    = ys * qRes.imag.x - wz;                        // 2(xy - wz)
        rwRes.up.y    = 1.0f - (qRes.imag.z * zs + xs * qRes.imag.x); // 1 - 2(z^2+x^2)
        rwRes.up.z    = yz + wx;                                      // 2(yz + wx)
        rwRes.at.x    = xs * qRes.imag.z + wy;                        // 2(xz + wy)
        rwRes.at.y    = yz - wx;                                      // 2(yz - wx)
        rwRes.at.z    = 1.0f - (ys * qRes.imag.y + xs * qRes.imag.x); // 1 - 2(y^2+x^2)
        rwRes.pos.x   = 0.0f;
        rwRes.pos.y   = 0.0f;
        rwRes.pos.z   = 0.0f;

        CMatrix mat;
        mat.UpdateMatrix(&rwRes); // 0x59AD20
        mat.GetPosition() = CVector{ targetPos.x, targetPos.y, useZ }; // fStack_17C/178/174 декомпила
        ped->SetMatrix(mat);       // 0x54F610 (asm 0x652177/0x6525EF: ECX = ped)
        return true;
    }

    // 0x652640: обычный путь
    if (doorOpenFactor > 0.2f /*0x858CC4*/ || bSimplePath
        || vehicle->m_nVehicleType == VEHICLE_TYPE_BIKE || vehicle->m_nVehicleSubType == VEHICLE_TYPE_QUAD) { // +0x590/0x594
        // Просто ставим педа в точку у двери (поворот — только heading)
        if (ped->m_matrix) {
            ped->m_matrix->GetPosition() = CVector{ targetPos.x, targetPos.y, useZ }; // +0x30/0x34/0x38
        } else {
            ped->m_placement.m_vPosn = CVector{ targetPos.x, targetPos.y, useZ }; // +0x4/0x8/0xC
        }
        ped->SetOrientation(0.0f, 0.0f, ped->m_fCurrentRotation); // 0x439A80
    } else {
        // Дверь ещё почти закрыта и авто на колёсах: ставим педа «в систему координат» машины
        // у (закрытой) двери
        CMatrix mat = *vehicle->m_matrix; // 0x59BCF0
        const auto doorLocal = GetLocalPositionToOpenCarDoor(vehicle, 0.0f, assoc); // 0x64FC10 (теперь реверснута выше)
        mat.GetPosition() += mat.TransformVector(doorLocal); // Multiply3x3, 0x59C790
        ped->SetMatrix(mat); // 0x54F610
    }
    return true;
}
