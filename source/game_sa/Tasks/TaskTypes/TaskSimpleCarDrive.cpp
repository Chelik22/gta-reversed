#include "StdInc.h"
#include "TaskSimpleCarDrive.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Ragdoll/IKChainManager.h"
// Batch w2-17-mixed: includes needed by the integrated bodies.
#include "TaskSimpleCarSetPedOut.h"                // CTaskSimpleCarSetPedOut (MakeAbortable)
#include "CarEnterExit.h"                          // CCarEnterExit::ComputeTargetDoorToExit / CarHasDoorToClose
#include "Animation/AnimManager.h"                 // CAnimManager::BlendAnimation
#include "Animation/AnimBlendFrameData.h"          // AnimBlendFrameData::KeyFrame
#include "Plugins/RpAnimBlendPlugin/RpAnimBlend.h" // RpAnimBlendClumpGetAssociation
#include "Weather.h"                               // CWeather::Rain
#include "Entity/Vehicle/Automobile.h"             // CAutomobile::GetDamageManager
#include "Models/ModelInfo.h"                      // CModelInfo::GetVehicleModelInfo
#include "DamageManager.h"                         // CDamageManager::GetDoorStatus_Component, DAMSTATE_OK, tComponent

// 0x86EBD4/0x86EBC8/0x86EBE0 - bopping rotation axes from .rdata (not named in the repo).
// Single file-scope instance shared by ProcessHeadBopping + ProcessArmBopping (see batch STATUS note).
static inline const auto& BoppingXAxis    = StaticRef<CVector>(0x86EBD4); // (1, 0, 0)
static inline const auto& BoppingZAxis    = StaticRef<CVector>(0x86EBC8); // (0, 0, 1)
static inline const auto& BoppingArmZAxis = StaticRef<CVector>(0x86EBE0); // (0, 0, 1) - used by ProcessArmBopping

// 0x8D2E9C..0x8D2EC8 - the 12 car-enter anim ids (not named in the repo, values not recovered).
static inline const auto& s_CarEnterAnimIds = StaticRef<AnimationId[12]>(0x8D2E9C);

void CTaskSimpleCarDrive::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleCarDrive, 0x86E904, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x63C340);
    RH_ScopedInstall(Destructor, 0x63C460);

    RH_ScopedInstall(TriggerIK, 0x63C500);
    RH_ScopedInstall(UpdateBopping, 0x63C900);
    RH_ScopedInstall(StartBopping, 0x642760);
    RH_ScopedInstall(ProcessHeadBopping, 0x6428C0);
    RH_ScopedInstall(ProcessArmBopping, 0x642AE0);
    RH_ScopedInstall(ProcessBopping, 0x642E70);
    RH_ScopedVMTInstall(Clone, 0x63DC20);
    RH_ScopedVMTInstall(GetTaskType, 0x63C450);
    RH_ScopedVMTInstall(MakeAbortable, 0x63C670);
    RH_ScopedVMTInstall(ProcessPed, 0x644470, { .Reversed = false }); // STUB_BLOCKED (batch w2-17-mixed) - left as redirect-stub, do not remove
    RH_ScopedVMTInstall(SetPedPosition, 0x63C770);
}

// 0x63C340
CTaskSimpleCarDrive::CTaskSimpleCarDrive(CVehicle* vehicle, CTaskUtilityLineUpPedWithCar* utilityTask, bool updateCurrentVehicle) : CTaskSimple() {
    m_pVehicle = vehicle;
    m_pAnimCloseDoorRolling = nullptr;
    m_pTaskUtilityLineUpPedWithCar = nullptr;
    m_TaskTimer = nullptr;
    field_18 = 0;
    field_1C = 0;
    field_1D = 0;
    m_nTimePassedSinceCarUpSideDown = 0;

    m_bUpdateCurrentVehicle = updateCurrentVehicle;
    m_b08 = true;

    CEntity::SafeRegisterRef(m_pVehicle);

    if (utilityTask) {
        m_pTaskUtilityLineUpPedWithCar = new CTaskUtilityLineUpPedWithCar(CVector{}, 0, utilityTask->m_nDoorOpenPosType, utilityTask->m_nDoorIdx);
    }

    m_fHeadBoppingFactor = 0.0f;
    m_fHeadBoppingOrientation = 0.0f;
    m_fRandomHeadBoppingMultiplier = 0.0f;
    m_nBoppingStartTime = -1;
}

// 0x63C460
CTaskSimpleCarDrive::~CTaskSimpleCarDrive() {
    CEntity::SafeCleanUpRef(m_pVehicle);

    if (m_pTaskUtilityLineUpPedWithCar) {
        delete m_pTaskUtilityLineUpPedWithCar;
        m_pTaskUtilityLineUpPedWithCar = nullptr;
    }

    if (m_b20 && m_pAnimCloseDoorRolling) {
        // TODO: FIX ME: Keeps triggering, annoying as fuck
        // Seemingly happens when getting of a motorbike (like cops getting off)
        //assert(m_pAnimCloseDoorRolling);
        m_pAnimCloseDoorRolling->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
        if (m_pVehicle) {
            m_pVehicle->ClearGettingOutFlags(1);
        }
    }
}

// 0x63C500
void CTaskSimpleCarDrive::TriggerIK(CPed* ped) const {
    if (!m_pVehicle) {
        return;
    }

    // Made an early-out of this, as this same code is used in both possible cases.
    if (g_ikChainMan.IsLooking(ped) || CGeneral::GetRandomNumberInRange(0, 100) >= 5) {
        return;
    }

    switch (m_pVehicle->m_autoPilot.m_nCarMission) {
    case MISSION_RAMPLAYER_FARAWAY:
    case MISSION_RAMPLAYER_CLOSE:
    case MISSION_BLOCKPLAYER_FARAWAY:
    case MISSION_BLOCKPLAYER_CLOSE:
    case MISSION_BLOCKPLAYER_HANDBRAKESTOP: { // Make ped look at player ped
        g_ikChainMan.LookAt("DriveCar", ped, FindPlayerPed(0), 3000, BONE_HEAD, nullptr, false, 0.25f, 500, 3, false);
        break;
    }
    case MISSION_RAMCAR_FARAWAY:
    case MISSION_RAMCAR_CLOSE: {
        if (const auto vehTargetCar = m_pVehicle->m_autoPilot.m_TargetEntity) {
            if (vehTargetCar->GetIsTypeVehicle()) {
                if (const auto driver = vehTargetCar->m_pDriver) { // Make ped look at target car or it's driver (if any)
                    g_ikChainMan.LookAt("DriveCar", ped, driver, 3000, BONE_HEAD, nullptr, false, 0.25f, 500, 3, false);
                } else {
                    g_ikChainMan.LookAt("DriveCar", ped, vehTargetCar, 3000, BONE_UNKNOWN, nullptr, false, 0.25f, 500, 3, false);
                }
            }
        }
        break;
    }
    }
}

// 0x63C900
void CTaskSimpleCarDrive::UpdateBopping() {
    const auto timeDelta = (int32)CTimer::GetTimeInMS() - m_nBoppingStartTime;
    m_fBoppingProgress = (float)(timeDelta % m_nBoppingEndTime) / (float)m_nBoppingEndTime;
    m_nBoppingCompletedTimes = timeDelta / m_nBoppingEndTime % 2;
}

// 0x642760
void CTaskSimpleCarDrive::StartBopping(CPed* ped) {
    // Try to sync our bopping to that of another occupant of the vehicle
    const auto FindOccupantBopping = [this, ped](CPed* occupant) {
        if (!occupant || occupant == ped) {
            return false;
        }
        if (const auto driveTask = static_cast<CTaskSimpleCarDrive*>(occupant->GetTaskManager().FindActiveTaskByType(TASK_SIMPLE_CAR_DRIVE) /*FUN_00681740(0x2C5)*/)) {
            if (driveTask->m_nBoppingStartTime != -1) { // +0x20
                const auto timeDelta     = (int32)(CTimer::m_snTimeInMilliseconds - driveTask->m_nBoppingStartTime);
                m_nBoppingStartTime      = driveTask->m_nBoppingStartTime;
                m_nBoppingEndTime        = driveTask->m_nBoppingEndTime;   // +0x28
                m_fBoppingProgress       = (float)(timeDelta % m_nBoppingEndTime) / (float)m_nBoppingEndTime; // +0x2C
                m_nBoppingCompletedTimes = timeDelta / m_nBoppingEndTime % 2;              // +0x30 (x & 0x80000001 со знаковой коррекцией)
                return true;
            }
        }
        return false;
    };

    if (FindOccupantBopping(m_pVehicle->m_pDriver)) { // veh + 0x460
        return;
    }
    for (auto i = 0; i < 3; i++) { // Then the first 3 passengers (veh + 0x464..0x46C)
        if (FindOccupantBopping(m_pVehicle->m_apPassengers[i])) {
            return;
        }
    }

    // Nobody else is bopping - start fresh
    m_nBoppingStartTime      = (int32)CTimer::m_snTimeInMilliseconds; // DAT_00B7CB84
    m_nBoppingEndTime        = rand(); // _rand + two _ftol2 (0x821B40) in the decompile
    m_fBoppingProgress       = 0.0f;
    m_nBoppingCompletedTimes = 0;
}

// 0x6428C0
void CTaskSimpleCarDrive::ProcessHeadBopping(CPed* ped, bool bDriving, float distSqToCamera) {
    auto headRotTarget = 0.0f;

    if (bDriving) {
        if (!m_b01) { // Not bopping yet (+0x5C & 1)
            if (m_fHeadBoppingFactor == 0.0f /*0x858B50*/ && CGeneral::GetRandomNumberInRange(0, 1000) > 995 /*0x3E3*/) {
                if (m_nBoppingStartTime == -1) {
                    StartBopping(ped); // 0x642760
                    m_nHeadBoppingStartTime = m_nBoppingStartTime;    // +0x34
                } else {
                    m_nHeadBoppingStartTime = (int32)CTimer::m_snTimeInMilliseconds; // DAT_00B7CB84
                }
                m_nHeadBoppingDirection        = CGeneral::GetRandomNumberInRange(1, 3) /*FUN_00407180*/;   // +0x38
                m_fRandomHeadBoppingMultiplier = CGeneral::GetRandomNumberInRange(3.0f /*0x40400000*/, 8.0f /*0x41000000*/); // FUN_0041BD90 → +0x40
                m_fHeadBoppingFactor           = 0.0f; // +0x44
                m_fHeadBoppingOrientation      = 0.0f; // +0x3C
                headRotTarget                  = 0.0f;
            } else {
                bDriving = false;
            }
        } else { // Bopping - compute the sine target
            headRotTarget = std::sin(m_fBoppingProgress * PI /*_DAT_00858CB8*/);
            if ((uint32)(CTimer::m_snTimeInMilliseconds - m_nHeadBoppingStartTime) >= 5001 /*0x1389*/
                && CGeneral::GetRandomNumberInRange(0, 1000) >= 996 /*0x3E4*/) {
                bDriving = false; // Stop bopping
            }
        }
    }

    // 0x64299E: m_b01 = bDriving (bit 0x1 @ +0x5C)
    m_b01 = bDriving;
    float delta;
    if (!m_b01) { // Fading out
        if (0.0f < m_fHeadBoppingFactor) {
            m_fHeadBoppingFactor -= 0.05f; // 0x858C28
        }
        if (m_fHeadBoppingFactor < 0.0f) {
            m_fHeadBoppingFactor = 0.0f;
        }
        delta = (1.0f /*0x858624*/ - m_fHeadBoppingFactor) * (headRotTarget - m_fHeadBoppingOrientation);
    } else { // Fading in
        if (m_fHeadBoppingFactor < 1.0f) {
            m_fHeadBoppingFactor += 0.05f;
        }
        if (m_fHeadBoppingFactor > 1.0f) {
            m_fHeadBoppingFactor = 1.0f;
        }
        delta = (headRotTarget - m_fHeadBoppingOrientation) * m_fHeadBoppingFactor;
    }
    m_fHeadBoppingOrientation += delta;

    // Apply to the head bone, but only when close enough to the camera and actually bopping
    if (distSqToCamera < 64.0f /*0x859A44*/ && m_fHeadBoppingOrientation > 0.0f) {
        const auto angle = m_fRandomHeadBoppingMultiplier * m_fHeadBoppingOrientation;
        const auto headQ = &ped->m_apBones[PED_NODE_HEAD]->KeyFrame->q; // ped + 0x490 → m_apBones[PED_NODE_HEAD]->KeyFrame(+0x10)

        if (m_nHeadBoppingDirection > 0) {
            auto a = angle;
            if (m_nBoppingCompletedTimes != 0) { // +0x30
                a = -a;
            }
            RtQuatRotate(headQ, &BoppingXAxis, a, rwCOMBINEPRECONCAT);   // FUN_007EB7C0(q, 0x86EBD4, a, 1)
        }
        if (m_nHeadBoppingDirection != 2) {
            RtQuatRotate(headQ, &BoppingZAxis, -angle, rwCOMBINEPRECONCAT); // FUN_007EB7C0(q, 0x86EBC8, -angle, 1)
        } else {
            RtQuatRotate(headQ, &BoppingZAxis, angle, rwCOMBINEPRECONCAT);
        }

        ped->bUpdateMatricesRequired = true; // ped flags (0x470) |= 0x4000
    }
}

// 0x642AE0
void CTaskSimpleCarDrive::ProcessArmBopping(CPed* ped, bool bArmOnDoor, float distSqToCamera) {
    if (ped->IsPlayer()) { // 0x5DF8F0
        return;
    }

    auto* veh = ped->m_pVehicle; // ped + 0x58C

    // If it's raining close all windows, otherwise open the window of the ped's seat
    if (CWeather::Rain /*DAT_00C81324*/ <= 0.0f) {
        if (veh->m_pDriver == ped) {           // veh + 0x460
            veh->SetWindowOpenFlag(10);
        } else if (veh->m_apPassengers[0] == ped) { // + 0x464
            veh->SetWindowOpenFlag(8);
        } else if (veh->m_apPassengers[1] == ped) { // + 0x468
            veh->SetWindowOpenFlag(11);
        } else {
            if (veh->m_apPassengers[2] != ped) { // + 0x46C
                goto windowsDone;
            }
            veh->SetWindowOpenFlag(9);
        }
    } else {
        veh->ClearWindowOpenFlag(10);
        veh->ClearWindowOpenFlag(8);
        veh->ClearWindowOpenFlag(11);
        veh->ClearWindowOpenFlag(9);
        bArmOnDoor = false;
    }

windowsDone: // 0x642B7B
    // Arm anim id: 0xA3 (163, ANIM_ID_TAP_HANDP) for front seats, 0xA2 (162, ANIM_ID_TAP_HAND) for the rest
    const auto isFrontSeat = veh->m_apPassengers[0] == ped || veh->m_apPassengers[2] == ped;
    const auto animId      = isFrontSeat ? ANIM_ID_TAP_HANDP : ANIM_ID_TAP_HAND;

    const auto anim = RpAnimBlendClumpGetAssociation(ped->GetRpClump(), animId); // FUN_004D68B0

    if (!bArmOnDoor) {
        if (anim) {
            anim->m_BlendAmount = -4.0f; // 0xC0800000
        }
        m_b02 = false; // +0x5C &= ~0x2
        return;
    }

    if (!m_b02) { // Not currently playing - maybe start it (0x642BE6)
        if (CGeneral::GetRandomNumberInRange(0, 1000) > 995 /*0x3E3*/) {
            const auto Play = [&]() {
                CAnimManager::BlendAnimation(ped->GetRpClump(), (AssocGroupId)0, animId, 4.0f); // FUN_004D4610
                if (m_nBoppingStartTime == -1) {
                    StartBopping(ped); // 0x642760
                    m_nArmBoppingStartTime = m_nBoppingStartTime;   // +0x48
                } else {
                    m_nArmBoppingStartTime = (int32)CTimer::m_snTimeInMilliseconds;
                }
                m_b02 = true;
            };

            if (veh->m_nVehicleSubType != VEHICLE_TYPE_AUTOMOBILE /* != 0 - veh + 0x594*/) {
                Play();
                return;
            }

            // Don't put the arm out if the door is blocked / missing / can't lean out
            const auto door = CCarEnterExit::ComputeTargetDoorToExit(veh, ped); // 0x64F110
            bool playNow = true;
            if (static_cast<CAutomobile*>(veh)->GetDamageManager().GetDoorStatus_Component((tComponent)door) != DAMSTATE_OK // FUN_006C2250
                || CCarEnterExit::CarHasDoorToClose(veh, door) // 0x64EE10
                || !veh->CanPedLeanOut(ped) /*0x6D5CF0*/
            ) {
                playNow = false;
            }

            // Speed threshold derived from the vehicle model's dummy positions
            const auto vehStruct = CModelInfo::GetVehicleModelInfo(veh->GetModelId())->m_pVehicleStruct; // mi + 0x5C
            if (   vehStruct->m_avDummyPos[DUMMY_HAND_REST].x != 0.0f /* +0x78 */
                || vehStruct->m_avDummyPos[DUMMY_HAND_REST].y != 0.0f /* +0x7C */
                || vehStruct->m_avDummyPos[DUMMY_HAND_REST].z != 0.0f /* +0x80 */
            ) {
                const auto speed = vehStruct->m_avDummyPos[DUMMY_HAND_REST].z - vehStruct->m_avDummyPos[DUMMY_SEAT_FRONT].z; // +0x38
                const auto speedFlags = (speed < 0.46f ? 0x100 : 0) | (speed == 0.46f /*0x86EBEC*/ ? 0x4000 : 0);
                if ((*reinterpret_cast<uint8*>(reinterpret_cast<uintptr_t>(veh) + 0x429) & 8) == 0) { // TODO: name the flag byte (veh + 0x429, bit 3)
                    if (speed < 0.39f /*0x86EBF0*/) {
                        return;
                    }
                    if (speedFlags != 0 && playNow) {
                        Play();
                    }
                } else {
                    const auto speedFlagsB = (speed < 0.44f ? 0x100 : 0) | (speed == 0.44f /*0x86EBF4*/ ? 0x4000 : 0);
                    if (speed < 0.4f /*0x858EE8*/) {
                        return;
                    }
                    if (speedFlagsB != 0 && playNow) {
                        Play();
                    }
                }
            }
            return; // 0x642E4E - none of the conditions to play were met
        }
        return;
    }

    // Currently playing (0x642D84)
    if (!anim) {
        m_b02 = false;
        return;
    }

    if (distSqToCamera < 64.0f /*0x859A44*/) {
        auto armAngle = 0.0f;
        if (0.75f /*0x858F34*/ <= m_fBoppingProgress) { // +0x2C
            armAngle = (m_fBoppingProgress - 0.75f) * 45.0f /*0x858CB4*/;
        }

        const auto handFrame = isFrontSeat
            ? ped->m_apBones[PED_NODE_RIGHT_HAND] /* ped + 0x4A0 */
            : ped->m_apBones[PED_NODE_LEFT_HAND]  /* ped + 0x49C */;
        RtQuatRotate(&handFrame->KeyFrame->q, &BoppingArmZAxis /*0x86EBE0*/, -armAngle, rwCOMBINEPRECONCAT); // FUN_007EB7C0
        ped->bUpdateMatricesRequired = true; // ped flags (0x470) |= 0x4000
    }

    // Stop after 5 seconds (with ~0.5% chance per frame)
    if ((uint32)(CTimer::m_snTimeInMilliseconds - m_nArmBoppingStartTime) > 5000
        && CGeneral::GetRandomNumberInRange(0, 1000) > 995 /*0x3E3*/
    ) {
        anim->m_BlendAmount = -4.0f; // 0xC0800000
        m_b02 = false;
    }
}

// 0x642E70
void CTaskSimpleCarDrive::ProcessBopping(CPed* ped, bool a3) {
    if (ped->m_pVehicle->m_pDriver == FindPlayerPed(0)
        || ped->m_nPedType == PED_TYPE_COP
        || ped->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT_AND_STAND_UP)
    ) {
        return;
    }

    auto* vehicle = ped->m_pVehicle;
    if (vehicle->IsAutomobile() && !vehicle->IsSubQuad() && !ped->IsCreatedByMission()) {
        if (m_nBoppingStartTime != -1) { // IsBopping
            UpdateBopping();
        }

        const auto dist = DistanceBetweenPointsSquared(TheCamera.GetPosition(), ped->GetPosition());
        ProcessHeadBopping(ped, a3, dist);
        ProcessArmBopping(ped, a3, dist);
        if (m_nBoppingStartTime != -1 && !m_b01 && !m_b02) {
            m_nBoppingStartTime = -1;
        }
    }
}

// 0x63DC20
CTask* CTaskSimpleCarDrive::Clone() const {
    auto task = new CTaskSimpleCarDrive(m_pVehicle);
    task->m_bUpdateCurrentVehicle = m_bUpdateCurrentVehicle;
    return task;
}

// 0x63C670
bool CTaskSimpleCarDrive::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    if (priority != ABORT_PRIORITY_IMMEDIATE) { // param_3 != 2 (1=URGENT тоже сюда)
        m_b08 = true;   // *(byte*)(this + 0x5C) |= 0x8
        return !m_b20;  // ~(*(byte*)(this + 0x5C) >> 5) & 1
    }

    // IMMEDIATE: пед выходит из машины
    if (ped->bInVehicle && ped->m_pVehicle) { // 0x46C & 0x100; +0x58C
        const auto door = CCarEnterExit::ComputeTargetDoorToExit(ped->m_pVehicle, ped); // 0x64F110 (репо: CCarEnterExit, CarEnterExit.h:41)
        const auto exitTask = new CTaskSimpleCarSetPedOut(m_pVehicle, (eTargetDoor)door, false); // ctor 0x6478B0(veh, door, 0)
        exitTask->ProcessPed(ped); // 0x647D10 — redirect-стаб в репо, позовёт оригинал
        delete exitTask;           // dtor 0x647950
    }

    if (g_ikChainMan.IsLooking(ped)) {      // 0x6181A0
        g_ikChainMan.AbortLookAt(ped, 250); // 0x618280, arg 0xFA
    }

    return true;
}

// 0x644470 - STUB_BLOCKED (batch w2-17-mixed): decompile needs dozens of unnamed CPed/CVehicle fields,
// vtable slots and tuning tables. Redirect-stub kept intentionally.
bool CTaskSimpleCarDrive::ProcessPed(CPed* ped) {
    return plugin::CallMethodAndReturn<bool, 0x644470, CTaskSimpleCarDrive*, CPed*>(this, ped);
}

// 0x63C770
bool CTaskSimpleCarDrive::SetPedPosition(CPed* ped) {
    if (m_b08) { // +0x5C & 0x8
        // Забираем utility-таск у другого CTaskSimpleCarDrive (угон машины с пассажиром/водителем)
        if (const auto task = ped->GetTaskManager().FindTaskByType(TASK_PRIMARY_DEFAULT, TASK_SIMPLE_CAR_DRIVE) /*FUN_006817D0(4, 0x2C5)*/) {
            if (task != this) {
                if (const auto other = static_cast<CTaskSimpleCarDrive*>(task); other->m_pTaskUtilityLineUpPedWithCar) { // +0x10
                    m_pTaskUtilityLineUpPedWithCar        = other->m_pTaskUtilityLineUpPedWithCar;
                    other->m_pTaskUtilityLineUpPedWithCar = nullptr;
                }
            }
        }
        m_b08 = false;
    }

    if (!m_pTaskUtilityLineUpPedWithCar) { // +0x10
        if (m_pVehicle && ped->m_pVehicle == m_pVehicle && ped->bInVehicle) { // +0x58C; 0x46C & 0x100
            ped->SetPedPositionInCar(); // 0x5DF910
        }
        return true;
    }

    // Ищем среди анимаций посадки ту, что сейчас играет у педа
    CAnimBlendAssociation* enterAnim{};
    for (const auto animId : s_CarEnterAnimIds) {
        if ((enterAnim = RpAnimBlendClumpGetAssociation(ped->GetRpClump(), animId)) /*FUN_004D68B0*/) {
            break;
        }
    }

    m_pTaskUtilityLineUpPedWithCar->ProcessPed(ped, m_pVehicle, enterAnim); // 0x6513A0 (redirect-стаб в репо)

    delete m_pTaskUtilityLineUpPedWithCar; // dtor 0x64FC00 + free
    m_pTaskUtilityLineUpPedWithCar = nullptr;

    return true;
}
