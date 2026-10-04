#include "StdInc.h"

#include "Cam.h"
#include "TimeCycle.h"
#include "Camera.h"
#include "Shadows.h"
#include "IdleCam.h"
#include "InterestingEvents.h"
#include "ModelIndices.h"
#include "HandShaker.h"
#include "WeaponInfo.h"
#include "TaskSimpleClimb.h"
#include "TaskSimpleHoldEntity.h"
#include "TaskSimpleArrestPed.h"
#include "ControllerConfigManager.h"
#include "PedIntelligence.h"
#include "Messages.h"
#include "Text/Text.h"
#include "General.h"

auto& gbFirstPersonRunThisFrame = StaticRef<bool>(0xB6EC20);
auto& gLastFrameProcessedDWCineyCam = StaticRef<uint32>(0x8CCB9C);

static inline auto& gbExitCam = StaticRef<std::array<bool, 29>>(0xB6EC5C); // indexed by ciney cam id (0x00-0x1C)

// Shared DW ciney cam state
static inline auto& gbDWCineyCamProcessed = StaticRef<bool>(0xB6F059); // TheCamera+0x31
static inline auto& DWCineyCamCurrentId   = StaticRef<uint32>(0x8CC488);
static inline auto& DWCineyCamStartTime   = StaticRef<float>(0x8CCBA0);
static inline auto& DWCineyCamEndTime     = StaticRef<float>(0x8CCBA4);

static inline auto& DWCineyCamLastPos = StaticRef<CVector>(0xB6FE8C);
static inline auto& DWCineyCamLastUp = StaticRef<CVector>(0xB6FE98);
static inline auto& DWCineyCamLastRight = StaticRef<CVector>(0xB6FEA4);
static inline auto& DWCineyCamLastFwd = StaticRef<CVector>(0xB6FEB0);

static inline auto& DWCineyCamLastNearClip = StaticRef<float>(0xB6EC08);
static inline auto& DWCineyCamLastFov = StaticRef<float>(0xB6EC0C);

// 0x509AE0
static void WellBufferMe(float target, float& valueToChange, float& speedSoFar, float topSpeed, float speedStep, bool isAnAngle) {
    const auto valueToTargetDiff = [&] {
        auto d = target - valueToChange;
        if (isAnAngle) {
            for (; d >= DegreesToRadians(180.0f); d -= DegreesToRadians(360.0f)) {
                ;
            }
            for (; d < DegreesToRadians(-180.0f); d += DegreesToRadians(360.0f)) {
                ;
            }
        }
        return d;
    }();

    const auto fullSpeedStep = valueToTargetDiff * topSpeed;
    speedSoFar += std::abs(std::abs(fullSpeedStep - speedSoFar) * CTimer::GetTimeStep() * speedStep);

    if (fullSpeedStep >= 0.0f || fullSpeedStep <= speedSoFar) {
        if (fullSpeedStep > 0.0f && fullSpeedStep < speedSoFar) {
            speedSoFar = fullSpeedStep;
        }
    } else {
        speedSoFar = fullSpeedStep;
    }

    valueToChange += std::min(CTimer::GetTimeStep(), 10.0f) * speedSoFar;
}

void CCam::InjectHooks() {
    RH_ScopedClass(CCam);
    RH_ScopedCategory("Camera");

    RH_ScopedInstall(Constructor, 0x517730);
    RH_ScopedInstall(Init, 0x50E490);
    RH_ScopedInstall(CacheLastSettingsDWCineyCam, 0x50D7A0);
    RH_ScopedInstall(DoCamBump, 0x50CB30);
    RH_ScopedInstall(Finalise_DW_CineyCams, 0x50DD70);
    RH_ScopedInstall(GetCoreDataForDWCineyCamMode, 0x517130);
    RH_ScopedInstall(GetLookFromLampPostPos, 0x5161A0);
    RH_ScopedInstall(GetVectorsReadyForRW, 0x509CE0);
    RH_ScopedInstall(Get_TwoPlayer_AimVector, 0x513E40);
    RH_ScopedInstall(IsTimeToExitThisDWCineyCamMode, 0x517400, { .reversed = false });
    RH_ScopedInstall(KeepTrackOfTheSpeed, 0x509DF0);
    RH_ScopedInstall(LookBehind, 0x520690);
    RH_ScopedInstall(LookRight, 0x520E40);
    RH_ScopedInstall(RotCamIfInFrontCar, 0x50A4F0);
    RH_ScopedInstall(Using3rdPersonMouseCam, 0x50A850);
    // Disabled: rewrite crashes/hangs at load-time (null target / unverified statics).
    RH_ScopedInstall(Process, 0x526FC0, { .reversed = false });
    // NOTE: still disabled (fresh reversal not completed yet; decompilation is verified, see
    // D:\projects\gta_sa_tester\...\ev_ccam for the current decompiles + resolved constants):
    //   Process_AimWeapon (0x521500): ~800 lines; the per-weapon beta/alpha clamping
    //     (WellBufferMe + weapon-info zoom sections) still needs a careful pass.
    //   Process_Cam_TwoPlayer_InCarAndShooting (0x519810): depends on many unresolved helpers
    //     (2-player info at 0xB7CD98/0xB7CF28, aim-vector helper 0x73E240, the 0x40FEC0
    //     matrix chain, audio funcs 0x6217D0/0x6218C0, 0x627CC0) that need identification first.
    //   Process_FollowCar_SA (0x5245B0): largest car cam; model-specific tweak tables, wheelie
    //     cam blend and mouse/stick beta handling not yet re-translated.
    //   Process_FollowPed_SA (0x522D40): largest ped cam; history tracking, WellBufferMe
    //     damping and ducking beta nudges not yet re-translated.

    RH_ScopedInstall(ProcessArrestCamOne, 0x518500, { .reversed = false }); // TRIAGE: user camera cluster (V/look/aim broken, cam-in-car)
    RH_ScopedInstall(ProcessPedsDeadBaby, 0x519250);
    RH_ScopedInstall(Process_1rstPersonPedOnPC, 0x50EB70);
    RH_ScopedInstall(Process_1stPerson, 0x517EA0);
    RH_ScopedInstall(Process_AimWeapon, 0x521500, { .reversed = false });
    RH_ScopedInstall(Process_AttachedCam, 0x512B10);
    RH_ScopedInstall(Process_Cam_TwoPlayer, 0x525E50, { .reversed = false });
    RH_ScopedInstall(Process_Cam_TwoPlayer_InCarAndShooting, 0x519810, { .reversed = false });
    RH_ScopedInstall(Process_Cam_TwoPlayer_Separate_Cars, 0x513510, { .reversed = false });
    RH_ScopedInstall(Process_Cam_TwoPlayer_Separate_Cars_TopDown, 0x513BE0);
    RH_ScopedInstall(Process_DW_BirdyCam, 0x51B850, { .reversed = false });
    RH_ScopedInstall(Process_DW_CamManCam, 0x51B120, { .reversed = false });
    RH_ScopedInstall(Process_DW_HeliChaseCam, 0x51A740, { .reversed = false });
    RH_ScopedInstall(Process_DW_PlaneCam1, 0x51C760, { .reversed = false });
    RH_ScopedInstall(Process_DW_PlaneCam2, 0x51CC30, { .reversed = false });
    RH_ScopedInstall(Process_DW_PlaneCam3, 0x51D100, { .reversed = false });
    RH_ScopedInstall(Process_DW_PlaneSpotterCam, 0x51C250, { .reversed = false });
    RH_ScopedInstall(Process_Editor, 0x50F3F0);
    RH_ScopedInstall(Process_Fixed, 0x51D470);
    RH_ScopedInstall(Process_FlyBy, 0x5B25F0, { .reversed = false });
    RH_ScopedInstall(Process_FollowCar_SA, 0x5245B0, { .reversed = false });
    RH_ScopedInstall(Process_FollowPedWithMouse, 0x50F970, { .reversed = false }); // TRIAGE: user camera cluster (V/look/aim broken, cam-in-car)
    RH_ScopedInstall(Process_FollowPed_SA, 0x522D40, { .reversed = false });
    RH_ScopedInstall(Process_M16_1stPerson, 0x5105C0, { .reversed = false }); // TRIAGE: user camera cluster (V/look/aim broken, cam-in-car)
    RH_ScopedInstall(Process_Rocket, 0x511B50);
    RH_ScopedInstall(Process_SpecialFixedForSyphon, 0x517500);
    RH_ScopedInstall(Process_WheelCam, 0x512110, { .reversed = false });

    RH_ScopedGlobalInstall(WellBufferMe, 0x509AE0);
}

// 0x517730
CCam::CCam() {
    Init();
}

// 0x50E490
void CCam::Init() {
    m_vecFront = CVector(0, 0, -1);
    m_vecUp = CVector(0, 0, 1);
    m_nMode = eCamMode::MODE_FOLLOWPED;
    m_bRotating = false;
    m_nDoCollisionChecksOnFrameNum = 1;
    m_nDoCollisionCheckEveryNumOfFrames = 9;
    m_nFrameNumWereAt = 0;
    m_bCollisionChecksOn = true;
    m_fRealGroundDist = 0.0f;
    m_fBetaSpeed = 0.0f;
    m_fAlphaSpeed = 0.0f;
    m_fCameraHeightMultiplier = 0.75;
    m_fMaxRoleAngle = DegreesToRadians(2.0f); // Original value is 0x3D0EFA35 (= 2 degrees); VC used 20 degrees
    m_fDistance = 30.0f;
    m_fDistanceSpeed = 0.0f;
    m_pLastCarEntered = nullptr;
    m_pLastPedLookedAt = nullptr;
    m_bResetStatics = true;
    m_fHorizontalAngle = 0.0f;
    m_fTilt = 0.0f;
    m_fTiltSpeed = 0.0f;
    m_bFixingBeta = false;
    m_fCaMinDistance = 0.0f;
    m_fCaMaxDistance = 0.0f;
    m_bLookingBehind = false;
    m_bLookingLeft = false;
    m_bLookingRight = false;
    m_fPlayerInFrontSyphonAngleOffSet = DegreesToRadians(20.0f);
    m_fSyphonModeTargetZOffSet = 0.5f;
    m_fRadiusForDead = 1.5f;
    m_nDirectionWasLooking = 3; // TODO: enum
    m_bLookBehindCamWasInFront = 0;
    m_fRoll = 0.0f;
    m_fRollSpeed = 0.0f;
    m_fCloseInPedHeightOffset = 0.0f;
    m_fCloseInPedHeightOffsetSpeed = 0.0f;
    m_fCloseInCarHeightOffset = 0.0f;
    m_fCloseInCarHeightOffsetSpeed = 0.0f;
    m_fPedBetweenCameraHeightOffset = 0.0f;
    m_fTargetBeta = 0.0f;
    m_fBufferedTargetBeta = 0.0f;
    m_fBufferedTargetOrientation = 0.0f;
    m_fBufferedTargetOrientationSpeed = 0.0f;
    m_fDimensionOfHighestNearCar = 0.0;
    m_fBeta_Targeting = 0.0f;
    m_fX_Targetting = 0.0f;
    m_fY_Targetting = 0.0f;
    m_pCarWeAreFocussingOn = nullptr;
    m_pCarWeAreFocussingOnI = nullptr;
    m_fCamBumpedHorz = 1.0f;
    m_fCamBumpedVert = 0.0f;
    m_nCamBumpedTime = 0;
    for (int i = 0; i < 4; ++i) {
        m_anTargetHistoryTime[i] = 0;
        m_avecTargetHistoryPos[i] = CVector{};
    }
    m_nCurrentHistoryPoints = 0;
    gPlayerPedVisible = true;
    gbCineyCamMessageDisplayed = 2; // TODO: enum
    gCameraDirection = 3; // TODO: enum
    gCameraMode = (eCamMode)-1;
    gLastTime2PlayerCameraWasOK = 0;
    gLastTime2PlayerCameraCollided = 0;
    TheCamera.m_bCinemaCamera = false;
}

// 0x50D7A0
void CCam::CacheLastSettingsDWCineyCam() {
    DWCineyCamLastUp       = m_vecUp;
    DWCineyCamLastFwd      = m_vecFront;
    DWCineyCamLastRight    = CrossProduct(m_vecFront, m_vecUp);
    DWCineyCamLastFov      = m_fFOV;
    DWCineyCamLastNearClip = RwCameraGetNearClipPlane(Scene.m_pRwCamera);
    DWCineyCamLastPos      = m_vecSource;
}

// 0x50CB30
void CCam::DoCamBump(float horizontal, float vertical) {
    m_fCamBumpedHorz = horizontal;
    m_fCamBumpedVert = vertical;
    m_nCamBumpedTime = CTimer::GetTimeInMS();
}

// 0x50DD70
void CCam::Finalise_DW_CineyCams(const CVector& src, const CVector& dest, float roll, float fov, float nearClip, float shakeDegree) {
    m_vecFront  = (dest - src).Normalized();
    m_vecSource = src;

    // What is this thing?
    {
        auto rightDir = m_vecFront.Cross({ std::sin(roll), 0.0f, std::cos(roll) }).Normalized();
        m_vecUp       = rightDir.Cross(m_vecFront);
        if (m_vecFront.x == 0.0f && m_vecFront.y == 0.0f) {
            m_vecFront.x = m_vecFront.y = 0.0001f;
        }
        rightDir = CrossProduct(m_vecFront, m_vecUp).Normalized();
        m_vecUp  = CrossProduct(rightDir, m_vecFront);
    }

    m_fFOV = fov;
    RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.4f); // meant to use nearClip here?
    CacheLastSettingsDWCineyCam();
    gLastFrameProcessedDWCineyCam = CTimer::GetFrameCounter();

    gHandShaker[0].Process(shakeDegree);
    m_vecFront = gHandShaker[0].m_resultMat.TransformVector(m_vecFront);
    m_vecFront.Normalise();

    {
        auto rightDir = m_vecFront.Cross({ std::sin(roll), 0.0f, std::cos(roll) }).Normalized();
        m_vecUp       = rightDir.Cross(m_vecFront);
        if (m_vecFront.x == 0.0f && m_vecFront.y == 0.0f) {
            m_vecFront.x = m_vecFront.y = 0.0001f;
        }
        rightDir = m_vecFront.Cross(m_vecUp).Normalized();
        m_vecUp  = rightDir.Cross(m_vecFront);
    }
}

// 0x517130
void CCam::GetCoreDataForDWCineyCamMode(
    CEntity*& entity,
    CVehicle*& vehicle,
    CVector& dest,
    CVector& src,
    CVector& targetUp,
    CVector& targetRight,
    CVector& targetFwd,
    CVector& targetVel,
    float& targetSpeed,
    CVector& targetAngVel,
    float& targetAngSpeed,
    CColSphere& colSphere
) {
    entity         = m_pCamTargetEntity;
    vehicle        = entity->AsVehicle();
    dest           = entity->GetPosition();
    src            = DWCineyCamLastPos;
    targetUp       = entity->GetUpVector();
    targetRight    = entity->GetRightVector();
    targetFwd      = entity->GetForwardVector();
    targetVel      = entity->AsPhysical()->GetMoveSpeed();
    targetSpeed    = targetVel.Magnitude();
    targetAngVel   = entity->AsPhysical()->GetTurnSpeed();
    targetAngSpeed = targetAngVel.Magnitude();

    colSphere.Set(
        entity->GetModelInfo()->GetColModel()->GetBoundRadius(),
        entity->GetBoundCentre(),
        eSurfaceType::SURFACE_DEFAULT
    );
}

// 0x5161A0
bool CCam::GetLookFromLampPostPos(CEntity* /*target*/, CPed* /*cop*/, const CVector& vecTarget, CVector& outPos) {
    static auto& s_MinLampPostFwdZ       = StaticRef<float>(0x858C20); // minimum forward Z of the lamp post matrix
    static auto& s_MinLampPostTargetDist = StaticRef<float>(0x858C80); // minimum 2D distance to the target
    static auto& s_LampPostExpectedDist  = StaticRef<float>(0x8CC8D8); // expected 2D distance to the target

    CEntity* entities[15]{};
    int16    numEntities{};
    CWorld::FindObjectsInRange(vecTarget, 30.0f, true, &numEntities, 15, entities, false, false, false, true, true);

    CEntity* lampEntity{};
    float    bestDiff{ 10000.0f };

    for (int16 i = 0; i < numEntities; ++i) {
        CEntity* const ent = entities[i];
        if (!ent->GetIsStatic()) {
            continue;
        }
        if (!ent->m_matrix) {
            ent->AllocateMatrix();
            ent->UpdateRwMatrix();
        }
        if (s_MinLampPostFwdZ >= ent->m_matrix->GetForward().z || !IsLampPost(static_cast<eModelID>(ent->GetModelIndex()))) {
            continue;
        }

        const auto lampPos = ent->GetPosition();
        const auto dist2D  = (CVector2D{ lampPos } - CVector2D{ vecTarget }).Magnitude();
        if (dist2D <= s_MinLampPostTargetDist || std::abs(s_LampPostExpectedDist - dist2D) >= bestDiff) {
            continue;
        }

        // Use the top of the lamppost (bbox max transformed into world space) as the look-from position
        const auto* colModel = ent->GetColModel();
        const CVector lampTop = ent->m_matrix->TransformPoint(colModel->m_boundBox.m_vecMax);

        if (CWorld::GetIsLineOfSightClear(lampTop, (lampTop - vecTarget).Normalized() + vecTarget, true, false, false, false, false, true, true)) {
            lampEntity = ent;
            bestDiff   = std::abs(s_LampPostExpectedDist - dist2D);
            outPos     = lampTop;
        }
    }

    return lampEntity != nullptr;
}

// 0x509CE0
void CCam::GetVectorsReadyForRW() {
    m_vecFront.Normalise();
    if (m_vecFront.x == 0.0f && m_vecFront.y == 0.0f) {
        m_vecFront.x = m_vecFront.y = 0.0001f;
    }
    const auto a = CrossProduct(m_vecFront, { 0.0f, 0.0f, 1.0f }).Normalized();
    m_vecUp = CrossProduct(a, m_vecFront);
}

// 0x513E40 -- not tested
void CCam::Get_TwoPlayer_AimVector(CVector& out) {
    const auto player = [&] {
        auto* p1 = FindPlayerPed(PED_TYPE_PLAYER1);
        if (p1->m_pVehicle && !p1->m_pVehicle->IsDriver(p1)) {
            return FindPlayerPed(PED_TYPE_PLAYER2);
        }
        return p1;
    }();

    const auto weaponInfo = player->GetActiveWeapon().GetWeaponInfo(player);
    const auto nearestTargetEntityInScreen = CWeapon::FindNearestTargetEntityWithScreenCoors(
        m_fX_Targetting,
        m_fY_Targetting,
        2 * weaponInfo.m_fWeaponRange,
        player->GetPosition()
    );

    if (nearestTargetEntityInScreen) {
        out = nearestTargetEntityInScreen->GetPosition() - m_vecSource;
    } else {
        const auto right  = CrossProduct(m_vecFront, m_vecUp);
        const auto tanFov = std::tan(m_fFOV * PI / 360.0f);

        out = m_fX_Targetting * m_fY_Targetting * tanFov * right + m_vecFront - tanFov / CDraw::ms_fAspectRatio * m_vecUp;
    }
    out.Normalise();
}

// 0x517400
bool CCam::IsTimeToExitThisDWCineyCamMode(int32 camId, const CVector& src, const CVector& dst, float /*t*/, bool lineOfSightCheck) {
    static auto& s_MinDists = StaticRef<std::array<float, 9>>(0x8CCBCC); // indexed by camId - 0x14
    static auto& s_MaxDists = StaticRef<std::array<float, 9>>(0x8CCBF0); // indexed by camId - 0x14

    if (gbExitCam[camId]) {
        return true;
    }

    bool noHit = true;
    if (lineOfSightCheck) {
        CWorld::pIgnoreEntity = m_pCamTargetEntity;
        CColPoint cp{};
        CEntity*  hitEnt{};
        noHit = !CWorld::ProcessLineOfSight(src, dst, cp, hitEnt, true, true, false, false, false, false, false, false);
        CWorld::pIgnoreEntity = nullptr;
    }

    if (camId > 0x13 && camId < 0x1D) {
        const auto  idx     = camId - 0x14;
        const auto  dist    = (dst - src).Magnitude();
        const bool  bInRange = dist >= s_MinDists[idx] && dist <= s_MaxDists[idx];
        if (!bInRange || !noHit || DWCineyCamEndTime < static_cast<float>(CTimer::GetTimeInMS())) {
            return true;
        }
    }
    return false;
}

// 0x509DF0
// 0x509BE0
// Wraps a speed/angle value in place: clamps it into [-0x858CC0, 0x858CB8] by
// repeatedly adding/subtracting the step at 0x858CBC. Takes a single float* argument.
static void WrapSpeedValue(float* v) {
    static const float HI    = StaticRef<float>(0x858CB8);
    static const float STEP  = StaticRef<float>(0x858CBC);
    static const float LO    = StaticRef<float>(0x858CC0);

    if (HI <= *v) {
        float f = *v;
        do {
            f -= STEP;
        } while (HI <= f);
        *v = f;
    }
    if (*v < LO) {
        float f = *v;
        do {
            f += STEP;
        } while (f < LO);
        *v = f;
    }
}

void CCam::KeepTrackOfTheSpeed(const CVector& source, const CVector& target, const CVector& up, const float& alpha, const float& beta, const float& fov) {
    static auto& s_PrevSource = StaticRef<CVector>(0xB6FF80);
    static auto& s_PrevTarget = StaticRef<CVector>(0xB6FF74);
    static auto& s_PrevUp     = StaticRef<CVector>(0xB6FF68);
    static auto& s_PrevBeta   = StaticRef<float>(0xB6FF64);
    static auto& s_PrevAlpha  = StaticRef<float>(0xB6FF60);
    static auto& s_PrevFov    = StaticRef<float>(0xB6FF5C);
    static auto& s_InitFlags  = StaticRef<uint32>(0xB6FF8C); // bit 1=source 2=target 4=up 8=beta 0x10=alpha 0x20=fov initialized

    if (!(s_InitFlags & 0x1)) {
        s_PrevSource = source;
        s_InitFlags |= 0x1;
    }
    if (!(s_InitFlags & 0x2)) {
        s_PrevTarget = target;
        s_InitFlags |= 0x2;
    }
    if (!(s_InitFlags & 0x4)) {
        s_PrevUp = up;
        s_InitFlags |= 0x4;
    }

    const auto prevBeta = (s_InitFlags & 0x8)  != 0 ? s_PrevBeta : (s_InitFlags |= 0x8,  beta);
    const auto prevFov  = (s_InitFlags & 0x20) != 0 ? s_PrevFov  : (s_InitFlags |= 0x20, fov);
    if (!(s_InitFlags & 0x10)) {
        s_PrevAlpha = alpha;
        s_InitFlags |= 0x10;
    }

    if (TheCamera.m_bJust_Switched) { // 0xB6F052
        s_PrevSource = source;
        s_PrevTarget = target;
        s_PrevUp     = up;
    }

    m_vecSourceSpeedOverOneFrame = source - s_PrevSource;
    m_vecTargetSpeedOverOneFrame = target - s_PrevTarget;
    m_vecUpOverOneFrame          = up - s_PrevUp;

    m_fFovSpeedOverOneFrame   = fov - prevFov;
    m_fBetaSpeedOverOneFrame  = beta - prevBeta;
    m_fAlphaSpeedOverOneFrame = alpha - s_PrevAlpha;

    // 0x509BE0 -- wrap the speed value in place (single float* arg, NOT a thiscall)
    WrapSpeedValue(&m_fBetaSpeedOverOneFrame);
    WrapSpeedValue(&m_fAlphaSpeedOverOneFrame);

    s_PrevSource = source;
    s_PrevTarget = target;
    s_PrevUp     = up;
    s_PrevBeta   = beta;
    s_PrevAlpha  = alpha;
    s_PrevFov    = fov;
}

// 0x520690
void CCam::LookBehind() {
    static auto& g_ColEntityFlags  = StaticRef<bool>(0xB6FC70);      // also used as the collision-ignore list counter (list @ 0xB6FC74)
    static auto& s_ColEntityList   = StaticRef<std::array<CEntity*, 8>>(0xB6FC74); // registered collision-ignores
    static auto& s_BehindCamPos    = StaticRef<CVector>(0xB6F018);
    static auto& s_FirstPersonIdx  = StaticRef<int32>(0xB6F0F0);
    static auto& s_ZOff            = StaticRef<float>(0xB6F0FC);
    static auto& s_LookBehindPos1  = StaticRef<bool>(0x8A5B14); // collision-variable flags of SetColVarsPed
    static auto& s_LookBehindPos2  = StaticRef<bool>(0x8A5B15);
    static auto& s_LookBehindPos3  = StaticRef<bool>(0x8A5B17);
    static auto& LOOK_DIST_Z       = StaticRef<float>(0x858CC4);
    static auto& WATER_MARGIN      = StaticRef<float>(0x858C24);
    static auto& MIN_LOOK_DIST     = StaticRef<float>(0x858CC8);
    static auto& HALF              = StaticRef<float>(0x858C1C);
    static auto& SCALE             = StaticRef<float>(0x858B8C);
    static auto& HELI_FWD_K        = StaticRef<float>(0x858F54);
    static auto& FWD_K             = StaticRef<float>(0x858C84);
    static auto& ZOFF_BASE         = StaticRef<float>(0x8CCE4C);
    static auto& NEARCLIP_TH       = StaticRef<float>(0x858C28);
    static auto& SWIM_ZOFF_TAB     = StaticRef<std::array<float, 3>>(0x8CCE18); // indexed by s_FirstPersonIdx
    static auto& SWIM_LERP_TAB     = StaticRef<std::array<float, 3>>(0x8CCE24);
    static auto& PED_ZOFF_TAB      = StaticRef<std::array<float, 3>>(0x8CCE30);
    static auto& SWIM_ZOFF2_TAB    = StaticRef<std::array<float, 3>>(0x8CCE3C);

    const auto mode = m_nMode;
    const bool bCarLookBehind = (mode == static_cast<eCamMode>(0x12) || mode == static_cast<eCamMode>(0x16) || mode == MODE_FOLLOWPED)
        && m_pCamTargetEntity->GetIsTypeVehicle();
    const bool b1stPersonLookBehind = mode == static_cast<eCamMode>(0x10) && m_pCamTargetEntity->GetIsTypeVehicle();

    if (!bCarLookBehind && !b1stPersonLookBehind && !m_pCamTargetEntity->GetIsTypePed()) {
        return;
    }

    auto* target = m_pCamTargetEntity;
    CVector pos = target->GetPosition();

    m_vecFront = pos - m_vecSource;

    if (bCarLookBehind) {
        CVector src0 = s_BehindCamPos;
        m_bLookBehindCamWasInFront = true;

        const auto dist = mode == static_cast<eCamMode>(0x12) ? m_fCaMaxDistance : 15.5f;

        if (!target->m_matrix) {
            target->AllocateMatrix();
            target->UpdateRwMatrix();
        }
        m_vecSource = target->m_matrix->GetForward();
        m_vecSource.z += LOOK_DIST_Z;

        CVector want = src0 + m_vecSource * dist;
        want.z = src0.z + dist * m_vecSource.z;

        CWorld::pIgnoreEntity = target;
        g_ColEntityFlags = false;
        TheCamera.CameraVehicleModeSpecialCases(target->AsVehicle());
        TheCamera.CameraColDetAndReact(&want, &src0);

        m_vecFront = target->GetPosition() - want;
        GetVectorsReadyForRW();
        TheCamera.ImproveNearClip(target->AsVehicle(), nullptr, &want, &src0);
        CWorld::pIgnoreEntity = nullptr;
        m_vecSource = want;
    }

    if (b1stPersonLookBehind) {
        m_bLookBehindCamWasInFront = true;
        RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.05f); // 0x3D4CCCCD

        auto* veh = target->AsVehicle();
        if (!veh->m_matrix) {
            veh->AllocateMatrix();
            veh->UpdateRwMatrix();
        }
        m_vecFront = veh->m_matrix->GetForward();
        m_vecFront.Normalise();

        if (veh->m_nVehicleSubType == VEHICLE_TYPE_BOAT) {
            m_vecSource.z -= SCALE;
        }

        switch (veh->GetVehicleAppearance()) {
        case VEHICLE_APPEARANCE_BIKE:
            m_vecSource += m_vecFront * HELI_FWD_K;
            m_vecFront = -m_vecFront;
            GetVectorsReadyForRW();
            break;
        case VEHICLE_APPEARANCE_HELI:
            m_vecFront = veh->m_matrix->GetUp() * HALF;
            m_vecUp    = veh->m_matrix->GetForward();
            m_vecSource += m_vecFront * FWD_K;
            break;
        default:
            m_vecSource += m_vecFront * FWD_K;
            m_vecFront = -m_vecFront;
            break;
        }
    }

    if (target->GetIsTypePed()) {
        auto* ped = target->AsPed();
        m_vecFront = CVector{ -std::cos(m_fHorizontalAngle), -std::sin(m_fHorizontalAngle), 0.0f };
        const auto& animOffset = *reinterpret_cast<const CVector*>(reinterpret_cast<uintptr_t>(ped) + 0x578); // anim "lean" offset
        m_vecFront.z = 0.0f - DotProduct(animOffset, m_vecFront) + WATER_MARGIN;
        m_vecFront.Normalise();

        auto lookDist = ZOFF_BASE + s_ZOff;
        if (lookDist < MIN_LOOK_DIST) {
            lookDist = MIN_LOOK_DIST;
        }
        m_vecSource = pos + m_vecFront * lookDist;

        const auto idx = s_FirstPersonIdx;
        if (ped->GetIntelligence()->GetTaskSwim()) {
            const auto k = SWIM_LERP_TAB[idx];
            m_vecSource  = m_vecSource + (pos - m_vecSource) * k;
            m_vecSource.z += SWIM_ZOFF_TAB[idx];
        }
        m_vecSource.z += SWIM_ZOFF2_TAB[idx];
        pos.z += PED_ZOFF_TAB[idx];

        TheCamera.HandleCameraMotionForDucking(ped, &m_vecSource, &pos, false); // 0x50CFA0

        g_ColEntityFlags = false;
        if (m_pCamTargetEntity) {
            if (auto* task = m_pCamTargetEntity->AsPed()->GetIntelligence()->GetTaskHold(0); task) {
                if (auto* heldEntity = task->GetHeldEntity()) {
                    s_ColEntityList[0] = heldEntity; // original: s_ColEntityList[counter++] where counter is g_ColEntityFlags (== 0 here)
                }
            }
        }
        s_LookBehindPos1 = true;
        s_LookBehindPos2 = true;
        s_LookBehindPos3 = true;

        TheCamera.CameraColDetAndReact(&m_vecSource, &pos);

        m_vecFront = pos - m_vecSource;
        GetVectorsReadyForRW();
        TheCamera.ImproveNearClip(nullptr, target->AsPed(), &m_vecSource, &pos);

        if (s_FirstPersonIdx == 1 && RwCameraGetNearClipPlane(Scene.m_pRwCamera) > NEARCLIP_TH) {
            RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.05f);
        }
    }

    GetVectorsReadyForRW();
}

// 0x520E40
void CCam::LookRight(bool bLookRight) {
    static auto& gbFlag_00B6FC70  = StaticRef<bool>(0xB6FC70); // collision-flags/counter (see LookBehind)
    static auto& SCALE            = StaticRef<float>(0x858B8C);
    static auto& MIN_DOT          = StaticRef<float>(0x858B1C);
    static auto& HALF_PI          = StaticRef<float>(0x858FE4); // NOTE: decomp reads a global that is pi/2
    static auto& LOOK_SIDE_OFFS   = StaticRef<float>(0x858F9C);
    static auto& PLANE_LOOK_OFFS  = StaticRef<float>(0x863230);
    static auto& FRONT_Z_OFFS     = StaticRef<float>(0x858F14);
    static auto& FRONT_Z_OFFS2    = StaticRef<float>(0x858F08);

    const auto mode = m_nMode;
    const bool bIsLookBehindVehicleMode = (mode == static_cast<eCamMode>(0x12) || mode == static_cast<eCamMode>(0x16) || mode == MODE_FOLLOWPED)
        && m_pCamTargetEntity->GetIsTypeVehicle();
    const bool bIsMode10Vehicle = mode == static_cast<eCamMode>(0x10) && m_pCamTargetEntity->GetIsTypeVehicle();

    auto sign = 1.0f;
    if (!bLookRight) {
        m_bLookingLeft = true;
        sign = -1.0f;
    } else {
        m_bLookingRight = true;
    }

    auto* target = m_pCamTargetEntity;

    if (!bIsLookBehindVehicleMode) {
        if (!bIsMode10Vehicle) {
            return;
        }

        RwCameraSetNearClipPlane(Scene.m_pRwCamera, StaticRef<float>(0x8CCE50));

        auto* veh = target->AsVehicle();
        if (veh->m_nVehicleSubType == VEHICLE_TYPE_BOAT) {
            if (!veh->m_pDriver) {
                m_vecSource.z -= SCALE;
            } else {
                auto* driver = veh->m_pDriver;
                driver->PositionAttachedPed();
                driver->UpdateRwMatrix();
                driver->UpdateRwFrame();
                CVector headPos = driver->GetBonePosition(static_cast<eBoneTag>(4), true);

                const auto& sideOfs = bLookRight ? StaticRef<CVector>(0x8CC498) : StaticRef<CVector>(0x8CC49C);
                headPos += driver->GetMatrix().TransformVector(sideOfs);

                const auto upK = StaticRef<float>(0x8CC494);
                headPos += veh->GetMatrix().GetUp() * upK;

                m_vecSource = headPos;
            }
        }

        if (veh->m_nVehicleSubType != VEHICLE_TYPE_BIKE) {
            m_vecSource -= veh->GetMatrix().GetRight() * LOOK_SIDE_OFFS;
        }

        if (!veh->m_matrix) {
            veh->AllocateMatrix();
            veh->UpdateRwMatrix();
        }
        m_vecUp = veh->m_matrix->GetUp();
        m_vecUp.Normalise();

        m_vecFront = veh->m_matrix->GetForward();
        m_vecFront.Normalise();
        m_vecFront = bLookRight
            ? CrossProduct(m_vecFront, m_vecUp)
            : CrossProduct(m_vecUp, m_vecFront);
        m_vecFront.Normalise();

        if (veh->GetVehicleAppearance() == VEHICLE_APPEARANCE_BIKE) { // original checks the raw value 2 here
            m_vecSource -= m_vecFront * PLANE_LOOK_OFFS;
        }
        return;
    }

    // look-behind in a car (modes 0x12 / 0x16 / MODE_FOLLOWPED)
    CVector pos = target->GetPosition();

    float dist = 9.0f;
    if (mode == static_cast<eCamMode>(0x12)) {
        dist = m_fCaMaxDistance;
    } else if (mode == static_cast<eCamMode>(0x16)) {
        float height{};
        // 0x509CA0 -- unresolved CCam helper
        if (plugin::CallMethodAndReturn<bool, 0x509CA0>(this, &height) && !CCullZones::Cam1stPersonForPlayer()) {
            m_vecSource.z = pos.z + height;
        }
    }

    if (!target->m_matrix) {
        target->AllocateMatrix();
        target->UpdateRwMatrix();
    }
    auto fwd = target->m_matrix->GetForward();
    fwd.Normalise();

    auto beta = CGeneral::GetATanOfXY(fwd.x, fwd.y);
    beta = sign * HALF_PI + beta;

    m_vecSource.x = std::cos(beta) * dist + pos.x;
    m_vecSource.y = std::sin(beta) * dist + pos.y;

    const auto* colModel = target->GetColModel();
    {
        CWorld::pIgnoreEntity = m_pCamTargetEntity;
        gbFlag_00B6FC70 = false;
        TheCamera.CameraVehicleModeSpecialCases(target->AsVehicle());
        TheCamera.CameraColDetAndReact(&m_vecSource, &pos);
        CWorld::pIgnoreEntity = nullptr;
    }

    pos = target->GetPosition();

    // adjust z by the vehicle's collision bounding box
    if (!target->m_matrix) {
        target->AllocateMatrix();
        target->UpdateRwMatrix();
    }
    const auto rightZ = target->m_matrix->GetRight().z;
    const float zOfs  = bLookRight
        ? rightZ * colModel->m_boundSphere.m_fRadius
        : colModel->m_boundBox.m_vecMax.z * rightZ;
    float newZ = pos.z + zOfs;
    newZ += colModel->m_boundBox.m_vecMax.z * target->m_matrix->GetUp().z;

    float zWant = newZ;
    if (newZ < m_vecCamFixedModeUpOffSet.z) { // +0x150 -- lower z limit
        zWant = m_vecCamFixedModeUpOffSet.z;
    }
    float z = m_vecSource.z;
    if (z >= zWant + MIN_DOT) {
        if (newZ < m_vecCamFixedModeUpOffSet.z) {
            newZ = m_vecCamFixedModeUpOffSet.z;
        }
        z = newZ + MIN_DOT;
    }
    if (z <= m_vecSource.z) {
        z = m_vecSource.z;
    }
    m_vecSource.z = z;

    m_vecFront = target->GetPosition() - m_vecSource;
    m_vecFront.z += FRONT_Z_OFFS;
    if (mode != static_cast<eCamMode>(0x16)) {
        GetVectorsReadyForRW();
        return;
    }
    m_vecFront.z += FRONT_Z_OFFS2;
    GetVectorsReadyForRW();
}

// 0x50A4F0
void CCam::RotCamIfInFrontCar(const CVector& targetCoors, float targetHeading) {
    static auto& g_BlendBetaFlag1 = StaticRef<bool>(0xB6F042); // TheCamera blend-beta flags (look left/right/behind handling)
    static auto& g_BlendBetaFlag2 = StaticRef<bool>(0xB6F043);
    static auto& g_BlendBetaFlag3 = StaticRef<bool>(0xB6F05F);
    static auto& g_LookBehindCam  = StaticRef<bool>(0xB6F080);
    static auto& s_Pi             = StaticRef<float>(0x858CB8);
    static auto& s_TwoPi          = StaticRef<float>(0x858CBC);
    static auto& s_PiNeg          = StaticRef<float>(0x858CC0);
    static auto& s_MinDot         = StaticRef<float>(0x858B1C);
    static auto& s_MinSpeedSq     = StaticRef<float>(0x8630E4);
    static auto& s_AngleMax       = StaticRef<float>(0x8630E0);
    static auto& s_AngleDone      = StaticRef<float>(0x8630DC);

    if (!m_pCamTargetEntity->GetIsTypeVehicle()) {
        return;
    }
    auto* vehicle = m_pCamTargetEntity->AsVehicle();
    if (!vehicle->m_matrix) {
        vehicle->AllocateMatrix();
        vehicle->UpdateRwMatrix();
    }
    const auto& vehMat = *vehicle->m_matrix;

    if (sq(vehicle->m_vecMoveSpeed.x) + sq(vehicle->m_vecMoveSpeed.y) > s_MinSpeedSq) {
        targetHeading = CGeneral::GetATanOfXY(-vehicle->m_vecMoveSpeed.x, vehicle->m_vecMoveSpeed.y) - DegreesToRadians(90.0f);
    }

    const auto dist2D = (CVector2D{ m_vecSource } - CVector2D{ targetCoors }).Magnitude();

    auto betaDiff = targetHeading - m_fHorizontalAngle;
    for (; betaDiff > s_Pi; betaDiff -= s_TwoPi) {
        ;
    }
    for (; betaDiff < s_PiNeg; betaDiff += s_TwoPi) {
        ;
    }

    if (std::abs(betaDiff) > s_AngleMax
        && DotProduct(vehMat.GetForward(), vehicle->m_vecMoveSpeed) > s_MinDot
        && !g_LookBehindCam
    ) {
        m_bFixingBeta = true;
    }

    const auto* pad = CPad::GetPad(0);
    bool looking = pad->GetLookBehindForCar();
    if (!looking) {
        looking = pad->GetLookBehindForPed();
        if (!looking) {
            looking = pad->GetLookLeft();
            if (!looking) {
                looking = pad->GetLookRight();
            }
        }
    }
    if (!looking && m_nDirectionWasLooking != 3) { // TODO: enum
        g_BlendBetaFlag1 = true;
    }

    if (!m_bFixingBeta && !g_BlendBetaFlag3 && !g_BlendBetaFlag1 && !g_BlendBetaFlag2) {
        return;
    }

    const bool bIsActiveCam   = this == &TheCamera.GetActiveCam();
    const bool directionWasSet = g_BlendBetaFlag1 || g_BlendBetaFlag2 || g_BlendBetaFlag3;

    if (m_bFixingBeta || directionWasSet) {
        WellBufferMe(targetHeading, m_fHorizontalAngle, m_fBetaSpeed, 0.1f /*0x3DCCCCCD*/, 0.003f /*0x3B449BA6*/, true);

        if (g_BlendBetaFlag1 && bIsActiveCam) {
            m_fHorizontalAngle = targetHeading;
        }
        if (g_BlendBetaFlag2 && bIsActiveCam) {
            m_fHorizontalAngle = targetHeading + s_Pi;
        }
        if (g_BlendBetaFlag3 && bIsActiveCam) {
            m_fHorizontalAngle = m_fTransitionBeta;
        }

        m_vecSource.x = targetCoors.x + std::cos(m_fHorizontalAngle) * dist2D;
        m_vecSource.y = targetCoors.y + std::sin(m_fHorizontalAngle) * dist2D;

        auto remaining = targetHeading - m_fHorizontalAngle;
        for (; remaining > s_Pi; remaining -= s_TwoPi) {
            ;
        }
        for (; remaining < s_PiNeg; remaining += s_TwoPi) {
            ;
        }
        if (std::abs(remaining) < s_AngleDone) {
            m_bFixingBeta = false;
        }
    }

    g_BlendBetaFlag1 = false;
    g_BlendBetaFlag2 = false;
}

// 0x50A850
bool CCam::Using3rdPersonMouseCam() const {
    return CCamera::m_bUseMouse3rdPerson && m_nMode == MODE_FOLLOWPED;
}

// 0x509DC0
bool CCam::GetWeaponFirstPersonOn() {
    return m_pCamTargetEntity && m_pCamTargetEntity->GetIsTypePed() && m_pCamTargetEntity->AsPed()->GetActiveWeapon().m_IsFirstPersonWeaponModeSelected;
}

// inlined -- alpha = vertical angle
void CCam::ClipAlpha() {
    m_fVerticalAngle = std::clamp(
        m_fVerticalAngle,
        DegreesToRadians(-85.5f),
        DegreesToRadians(+60.0f)
    );
}

// 0x509C50 -- beta = horizontal angle
void CCam::ClipBeta() {
    if (m_fHorizontalAngle < DegreesToRadians(-180.0f)) {
        m_fHorizontalAngle += DegreesToRadians(360.0f);
    } else {
        m_fHorizontalAngle -= DegreesToRadians(360.0f);
    }
}

// 0x526FC0
void CCam::Process() {
    static auto& SCALE1_          = StaticRef<float>(0x858B8C);
    static auto& ZERO_            = StaticRef<float>(0x858B50);
    static auto& s_ScriptHudFlag  = StaticRef<uint32>(0xC0B184);
    static auto& s_ScriptFadeT    = StaticRef<float>(0xB6FE34);
    static auto& s_ScriptTimer    = StaticRef<float>(0xB6FDC8);
    static auto& s_InVehicleTest  = StaticRef<uint8>(0xC8A860);
    static auto& s_LastPlayerPos  = StaticRef<CVector>(0x8CCC3C);
    static auto& s_PlayerVel      = StaticRef<CVector>(0xB6EC7C);
    static auto& g_BlendBetaFlag1 = StaticRef<bool>(0xB6F042);
    static auto& g_BlendBetaFlag2 = StaticRef<bool>(0xB6F043);
    static auto& g_LookBehind     = StaticRef<bool>(0xB6F080);
    static auto& g_LookLeft       = StaticRef<bool>(0xB6F077);
    static auto& g_LookRight      = StaticRef<bool>(0xB6F062);
    static auto& g_CameraDirSwitch = TheCamera.m_bJust_Switched; // 0xB6F052
    static auto& g_LastDir        = StaticRef<int32>(0x8CC384);
    static auto& g_CurrDir        = StaticRef<int32>(0x8CC388);
    static auto& s_SyphonFlag     = StaticRef<uint8>(0x8D5109);
    static auto& s_FovOverrideFlag = StaticRef<bool>(0xB6FCDD);
    static auto& s_FovOverride     = StaticRef<float>(0xB6FCE0);
    static auto& s_PosOverrideFlag = StaticRef<bool>(0xB6FD14);
    static auto& s_PosOverride     = StaticRef<CVector>(0xB6FD08);
    static auto& s_FrontOverrideFlag = StaticRef<bool>(0xB6FCB4);
    static auto& s_FrontOverride   = StaticRef<CVector>(0xB6FCA8);
    static auto& s_f_8CCB84        = StaticRef<float>(0x8CCB84);
    static auto& s_DirWasFlag      = StaticRef<bool>(0xB6F999);
    static auto& PLAYER_DIST_TH    = StaticRef<float>(0x85EED4);
    static auto& TSTEP_TH          = StaticRef<float>(0x858CC4);
    static auto& SPEEDVAR_A        = StaticRef<float>(0x863250);
    static auto& SPEEDVAR_B        = StaticRef<float>(0x863254);
    static auto& SPEEDVAR_C        = StaticRef<float>(0x863258);
    static auto& SPEEDVAR_D        = StaticRef<float>(0x86325C);
    static auto& MOUSE_3RD_8CCF00  = StaticRef<uint8>(0x8CCF00);
    static auto& DEATH_FLAG1       = StaticRef<bool>(0xB6F04E);
    static auto& DEATH_FLAG2       = StaticRef<bool>(0xB6F050);

    if (s_ScriptTimer <= s_ScriptFadeT) {
        s_ScriptHudFlag &= ~1u;
    }
    if (TheCamera.GetActiveCam().m_nMode != MODE_FOLLOWPED) {
        s_ScriptHudFlag &= ~1u;
        s_ScriptFadeT = 0.0f;
    }

    if (!m_pCamTargetEntity) {
        m_pCamTargetEntity = TheCamera.m_pTargetEntity;
        m_pCamTargetEntity->RegisterReference(&m_pCamTargetEntity);
    }

    if (s_InVehicleTest == 1) {
        if (auto* playerPed = FindPlayerPed(-1); !playerPed || !playerPed->m_pVehicle || playerPed->m_pVehicle->GetModelIndex() != 0x208) {
            CWeaponEffects::ClearCrossHairImmediately(CrossHairId{ 0 }); // 0x742CA0
        }
    }

    ++m_nFrameNumWereAt;
    if (m_nFrameNumWereAt > m_nDoCollisionCheckEveryNumOfFrames) {
        m_nFrameNumWereAt = 1;
    }
    m_bCollisionChecksOn = m_nFrameNumWereAt == m_nDoCollisionChecksOnFrameNum;

    CVector targetPos{};
    float   orientation{};
    float   speedVar{};

    if (!m_bResetStatics) {
        if (m_pCamTargetEntity->GetIsTypeVehicle()) {
            auto* veh = m_pCamTargetEntity->AsVehicle();
            targetPos = veh->GetPosition();
            if (!veh->m_matrix) {
                veh->AllocateMatrix();
                veh->UpdateRwMatrix();
            }
            const auto& fwd = veh->m_matrix->GetForward();
            if (fwd.x == ZERO_) {
                orientation = fwd.y != ZERO_ ? CGeneral::GetATanOfXY(fwd.x, fwd.y) : 0.0f;
            } else {
                orientation = CGeneral::GetATanOfXY(fwd.x, fwd.y);
            }
            auto fwd2D = CVector2D{ fwd };
            if (fwd2D.Magnitude() != 0.0f) {
                fwd2D.Normalise();
            }
            const auto sp = std::abs(fwd2D.x * veh->m_vecMoveSpeed.x + fwd2D.y * veh->m_vecMoveSpeed.y);
            if (fwd2D.x * veh->m_vecMoveSpeed.x + fwd2D.y * veh->m_vecMoveSpeed.y <= ZERO_) {
                speedVar = sp * SPEEDVAR_C;
                if (speedVar > SCALE1_) {
                    speedVar = 1.0f;
                }
                speedVar = -speedVar;
            } else {
                speedVar = sp * SPEEDVAR_D;
                if (speedVar > 1.0f) {
                    speedVar = 1.0f;
                }
            }
            m_fSpeedVar = m_fSpeedVar * SPEEDVAR_A + speedVar * SPEEDVAR_B;

            if (m_nDirectionWasLooking != 3) {
                auto* pad = CPad::GetPad(0);
                if (pad->GetLookBehindForCar()) {
                    if (pad->GetLookLeft() || pad->GetLookRight()) {
                        g_BlendBetaFlag1 = true;
                    }
                }
            }
        } else {
            if (m_pCamTargetEntity == FindPlayerPed(-1)) {
                auto* playerPed = FindPlayerPed(-1);
                targetPos = playerPed->GetPosition();
                if (auto* climb = playerPed->GetIntelligence()->GetTaskClimb()) {
                    climb->GetCameraTargetPos(playerPed, targetPos); // 0x67A390
                }
                const auto ped = targetPos;
                // smooth / extrapolate the player position
                if (PLAYER_DIST_TH < sq(s_LastPlayerPos.x - ped.x) + sq(s_LastPlayerPos.y - ped.y) + sq(s_LastPlayerPos.z - ped.z)
                    || CTimer::GetTimeStep() > TSTEP_TH
                    || Using3rdPersonMouseCam()
                    || g_BlendBetaFlag1 || g_BlendBetaFlag2
                ) {
                    s_PlayerVel = CVector{};
                } else if (auto* fight = playerPed->GetIntelligence()->GetTaskFighting(); fight && m_nMode == static_cast<eCamMode>(0x35)) {
                    // Original: pow(0x8CC39C /* 0.9 */, CTimer::ms_fTimeStep) via the CRT pow
                    // dispatcher at 0x822130 (args passed on the FPU stack; asm 0x52744B-0x52745C).
                    const auto r = std::pow(StaticRef<float>(0x8CC39C), CTimer::GetTimeStep()); // 0.9
                    s_PlayerVel = CVector{};
                    targetPos = ped * r + s_LastPlayerPos * (1.0f - r);
                } else {
                    // Original (asm 0x5274B3-0x5274DF): 1 - pow(0x8CC394 /* 0.6 */, ts) and
                    // 1 - pow(0x8CC398 /* 0.8 */, ts) (same CRT pow dispatcher at 0x822130).
                    const auto a = 1.0f - std::pow(StaticRef<float>(0x8CC394), CTimer::GetTimeStep()); // 0.6
                    const auto b = 1.0f - std::pow(StaticRef<float>(0x8CC398), CTimer::GetTimeStep()); // 0.8
                    const auto vel = s_PlayerVel * CTimer::GetTimeStep();
                    targetPos = ped + vel;
                    targetPos = s_LastPlayerPos + (targetPos - s_LastPlayerPos) * b;
                    s_PlayerVel = (targetPos - ped) + s_PlayerVel * a;
                    s_PlayerVel.z = 0.0f;
                }
                s_LastPlayerPos = targetPos;
            } else {
                targetPos = m_pCamTargetEntity->GetPosition();
            }

            // orientation (same degenerate check as for vehicles)
            if (!m_pCamTargetEntity->m_matrix) {
                m_pCamTargetEntity->AllocateMatrix();
                m_pCamTargetEntity->UpdateRwMatrix();
            }
            const auto& fwd = m_pCamTargetEntity->m_matrix->GetForward();
            if (fwd.x == ZERO_) {
                orientation = fwd.y != ZERO_ ? CGeneral::GetATanOfXY(fwd.x, fwd.y) : 0.0f;
            } else {
                orientation = CGeneral::GetATanOfXY(fwd.x, fwd.y);
            }
            speedVar    = 0.0f;
            m_fSpeedVar = 0.0f;
        }
    } else {
        targetPos = m_vecCamFixedModeSource;
    }

    m_nDirectionWasLooking = g_LastDir;
    g_LastDir = 3;

    if (this == &TheCamera.GetActiveCam()) {
        const auto mode = m_nMode;
        bool bWasLooking  = g_LookBehind != 0;
        bool bSkipDirTail = false;
        if (((mode == static_cast<eCamMode>(0x12)) || (mode == static_cast<eCamMode>(0x10))
             || (mode == static_cast<eCamMode>(0x16)) || (mode == MODE_FOLLOWPED))
            && m_pCamTargetEntity->GetIsTypeVehicle()
        ) {
            const auto appearance = m_pCamTargetEntity->AsVehicle()->GetVehicleAppearance();
            const bool bBoatOrPlane = appearance == VEHICLE_APPEARANCE_BOAT || appearance == VEHICLE_APPEARANCE_PLANE;
            auto* pad = CPad::GetPad(0);
            if (pad->GetLookBehindForCar()) {
                g_LookBehind = false;
                g_LookLeft   = false;
                g_LookRight  = false;
                bWasLooking  = m_nDirectionWasLooking == 0;
                g_LastDir    = 0;
            } else if (pad->GetLookLeft() && !bBoatOrPlane) {
                g_LastDir    = 1;
                g_LookRight  = bBoatOrPlane;
                g_LookLeft   = bBoatOrPlane;
                g_LookBehind = bBoatOrPlane;
                if (m_nDirectionWasLooking != g_LastDir) {
                    g_CameraDirSwitch = true;
                }
                bWasLooking    = false;
                bSkipDirTail   = true;
            } else if (pad->GetLookRight() && !bBoatOrPlane) {
                g_LastDir    = 2;
                g_LookRight  = bBoatOrPlane;
                g_LookLeft   = bBoatOrPlane;
                g_LookBehind = bBoatOrPlane;
                if (m_nDirectionWasLooking != g_LastDir) {
                    g_CameraDirSwitch = true;
                }
                bWasLooking  = false;
                bSkipDirTail = true;
            } else {
                g_LastDir = 3;
            }
            if (!bSkipDirTail) {
                if (!bWasLooking) {
                    g_CameraDirSwitch = true;
                }
                g_LastDir = 0;
            }
        } else {
            if (mode == MODE_FOLLOWPED && m_pCamTargetEntity->GetIsTypePed()) {
                if (CPad::GetPad(0)->GetLookBehindForPed()) {
                    if (m_nDirectionWasLooking != 0) {
                        g_CameraDirSwitch = true;
                    }
                    g_LastDir = 0;
                }
            } else if (mode == static_cast<eCamMode>(0x35)) {
                g_LastDir = 3;
                if (m_nDirectionWasLooking != 3) {
                    s_f_8CCB84 = 1.0f;
                }
            }
        }
    }

    if (g_CameraDirSwitch) {
        s_f_8CCB84 = 1.0f;
        s_DirWasFlag = true;
    }

    if (m_nMode != MODE_FOLLOWPED && m_nMode != static_cast<eCamMode>(0x12)
        && m_nMode != static_cast<eCamMode>(0x16) && m_nMode != static_cast<eCamMode>(0x10)
        && m_nMode != static_cast<eCamMode>(0x31)) {
        s_SyphonFlag = 0;
    }

    gbFirstPersonRunThisFrame = false;

    switch (static_cast<uint16>(m_nMode)) {
    case 3:
    case 0x12:
    case 0x16:
        Process_FollowCar_SA(targetPos, orientation, m_fSpeedVar, speedVar, false);
        break;
    case 4:
        if (!CCamera::m_bUseMouse3rdPerson || MOUSE_3RD_8CCF00 != 0) {
            Process_FollowPed_SA(targetPos, orientation, m_fSpeedVar, speedVar, false);
        } else {
            Process_FollowPedWithMouse(targetPos, orientation, m_fSpeedVar, speedVar);
        }
        break;
    case 7:
    case 0x22:
    case 0x2D:
    case 0x2E:
        Process_M16_1stPerson(targetPos, orientation, m_fSpeedVar, speedVar);
        break;
    case 8:
        Process_Rocket(targetPos, orientation, m_fSpeedVar, speedVar, false);
        break;
    case 0xE:
        Process_WheelCam(targetPos, orientation, m_fSpeedVar, speedVar);
        break;
    case 0xF:
        Process_Fixed(targetPos, orientation, m_fSpeedVar, speedVar);
        break;
    case 0x10:
        Process_1stPerson(targetPos, orientation, m_fSpeedVar, speedVar);
        break;
    case 0x11:
        Process_FlyBy(targetPos, orientation, m_fSpeedVar, speedVar);
        break;
    case 0x1D:
        ProcessPedsDeadBaby();
        DEATH_FLAG1 = false;
        DEATH_FLAG2 = false;
        break;
    case 0x20:
        ProcessArrestCamOne();
        break;
    case 0x21:
        break;
    case 0x23:
        Process_SpecialFixedForSyphon(targetPos, orientation, m_fSpeedVar, speedVar);
        break;
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x34:
        Process_1rstPersonPedOnPC(targetPos, orientation, m_fSpeedVar, speedVar);
        break;
    case 0x2C:
        Process_Editor(targetPos, orientation, m_fSpeedVar, speedVar);
        break;
    case 0x2F:
        Process_AttachedCam();
        break;
    case 0x30:
        Process_Cam_TwoPlayer();
        break;
    case 0x31:
        Process_Cam_TwoPlayer_InCarAndShooting();
        break;
    case 0x32:
        Process_Cam_TwoPlayer_Separate_Cars();
        break;
    case 0x33:
        Process_Rocket(targetPos, orientation, m_fSpeedVar, speedVar, true);
        break;
    case 0x35:
    case 0x37:
    case 0x41:
        Process_AimWeapon(targetPos, orientation, m_fSpeedVar, speedVar);
        break;
    case 0x36:
        Process_Cam_TwoPlayer_Separate_Cars_TopDown();
        break;
    case 0x38:
        Process_DW_HeliChaseCam(false);
        break;
    case 0x39:
        Process_DW_CamManCam(false);
        break;
    case 0x3A:
        Process_DW_BirdyCam(false);
        break;
    case 0x3B:
        Process_DW_PlaneSpotterCam(false);
        break;
    case 0x3C:
    case 0x3D:
        gbDWCineyCamProcessed = false;
        break;
    case 0x3E:
        Process_DW_PlaneCam1(false);
        break;
    case 0x3F:
        Process_DW_PlaneCam2(false);
        break;
    case 0x40:
        Process_DW_PlaneCam3(false);
        break;
    default:
        m_vecSource = CVector{};
        m_vecFront  = CVector{ 0.0f, 1.0f, 0.0f };
        m_vecUp     = CVector{ 0.0f, 0.0f, 1.0f };
        break;
    }

    if (static_cast<uint16>(m_nMode) < 0x38 || 0x40 < static_cast<uint16>(m_nMode)) {
        DWCineyCamCurrentId = 0xFFFFFFFF;
    }
    g_CurrDir = static_cast<int16>(m_nMode);

    const auto toTarget = m_vecSource - m_vecTargetCoorsForFudgeInter;
    m_fTrueBeta  = CGeneral::GetATanOfXY(toTarget.x, toTarget.y);
    m_fTrueAlpha = CGeneral::GetATanOfXY(toTarget.Magnitude2D(), toTarget.z);

    if (!g_LookBehind) {
        KeepTrackOfTheSpeed(m_vecSource, m_vecTargetCoorsForFudgeInter, m_vecUp, m_fTrueAlpha, m_fTrueBeta, m_fFOV);
    }

    m_vecSourceBeforeLookBehind = m_vecSource;
    m_bLookingBehind = false;
    m_bLookingLeft   = false;
    m_bLookingRight  = false;

    if (this == &TheCamera.GetActiveCam()) {
        if (g_LastDir == 0) {
            LookBehind();
        } else if (g_LastDir == 1) {
            LookRight(false);
        } else if (g_LastDir == 2) {
            LookRight(true);
        }
        m_nDirectionWasLooking = g_LastDir;
    }

    if (s_FovOverrideFlag) {
        m_fFOV = s_FovOverride;
        s_FovOverrideFlag = false;
    }
    if (s_PosOverrideFlag) {
        m_vecSource = s_PosOverride;
        s_PosOverrideFlag = false;
    }
    if (s_FrontOverrideFlag) {
        m_vecFront = s_FrontOverride - m_vecSource;
        m_vecFront.Normalise();
        GetVectorsReadyForRW();
        s_FrontOverrideFlag = false;
    }
}

// 0x518500
void CCam::ProcessArrestCamOne() {
    // Unreversed helpers - they compute a view position relative to the busting cop:
    //   0x515D80: position at the side of the target, opposite of the cop (dir = normalize(cross(target - cop, up)))
    //   0x516010: position behind/above the cop along the (cop -> target) direction, ground snapped
    // Both are `__stdcall bool(CEntity* target, CPed* cop, CVector* targetPos, CVector* outPos)`.
    using GetLookFromCopPosFunc = bool(__stdcall*)(CEntity*, CPed*, CVector*, CVector*);
    static const auto GetLookFromCopSidePos  = reinterpret_cast<GetLookFromCopPosFunc>(0x515D80);
    static const auto GetLookFromCopAbovePos = reinterpret_cast<GetLookFromCopPosFunc>(0x516010);

    // Unreversed helper: CCamera::__thiscall(CVector* from, CVector* target, CVector* inOutSrc, float fov)
    // Moves the (in/out) source position away from the target until it isn't obstructed, doing a
    // line-of-sight test; the result source is written back into `inOutSrc`.
    using ProcessCamSrcFunc = void(__thiscall*)(CCamera*, CVector*, CVector*, CVector*, float);
    static const auto ProcessCamSrc = reinterpret_cast<ProcessCamSrcFunc>(0x514030);

    // Arrest-cam statics.
    // NOTE: 0xB6EC5C overlaps `gbExitCam[0]` - they are distinct statics in the original.
    static auto& s_Mode          = StaticRef<int32>(0xB6EC58); // Arrest cam variant in use (0 = none, 1..8)
    static auto& s_ArrestingCop  = StaticRef<CPed*>(0xB6EC5C);  // Cop we're currently looking from
    static auto& s_StartTime     = StaticRef<float>(0xB6EC60);  // (float)CTimer::m_snTimeInMilliseconds when the DW cop cam started
    static auto& gbSkipBustedMsg = StaticRef<bool>(0xBAADC0);   // If set, the "BUSTED" big message isn't shown
    static auto& s_TargetEntity  = StaticRef<CEntity*>(0xB6F980); // TheCamera.m_pTargetEntity

    static const auto& RAND_SCALE = StaticRef<float>(0x858C7C); // 1 / 32768
    static const auto& TWO_POW_32 = StaticRef<float>(0x858C54);

    const auto SetStartTime = [&] {
        s_StartTime = static_cast<float>(CTimer::m_snTimeInMilliseconds);
        if (static_cast<int32>(CTimer::m_snTimeInMilliseconds) < 0) {
            s_StartTime += TWO_POW_32;
        }
    };

    auto* target = s_TargetEntity;

    CVector targetPos{};
    // NOTE: In the original this is left uninitialized (garbage) when no variant succeeds.
    // We seed it with the current source so a total variant failure keeps the camera where
    // it was instead of snapping it to the origin.
    CVector outPos = m_vecSource;
    CPed*   cop    = nullptr; // The arresting cop (from player data), if any
    CPed*   anchor = nullptr; // Ped whose ped-scanner is searched for arresting cops

    const auto FinaliseSource = [&] {
        m_vecSource = outPos;
        CVector from = outPos;
        ProcessCamSrc(&TheCamera, &from, &targetPos, &m_vecSource, m_fFOV);

        m_vecFront = targetPos - m_vecSource;
        m_vecUp = CVector{ 0.0f, 0.0f, 1.0f };
        m_vecUp = CrossProduct(CrossProduct(m_vecFront, m_vecUp).Normalized(), m_vecFront);
    };

    // Recompute the target position (player's spine, or the driver's one when in a vehicle)
    const auto UpdateTargetPos = [&] {
        switch (target->GetType()) {
        case ENTITY_TYPE_PED: {
            targetPos = target->AsPed()->GetBonePosition(BONE_SPINE1, true);
            break;
        }
        case ENTITY_TYPE_VEHICLE: {
            if (const auto driver = target->AsVehicle()->m_pDriver; driver && driver->IsPlayer()) {
                targetPos = driver->GetBonePosition(BONE_SPINE1, true);
            } else {
                targetPos = target->GetPosition();
            }
            break;
        }
        default:
            return false;
        }
        return true;
    };

    m_fFOV = 45.0f;

    if (m_bResetStatics) {
        s_Mode = 0;

        std::array<int32, 6> variants{};

        switch (target->GetType()) {
        case ENTITY_TYPE_PED: {
            const auto tped = target->AsPed();
            targetPos = tped->GetBonePosition(BONE_SPINE1, true);
            anchor    = tped;
            variants  = { 1, 3, 2, -1, -1, -1 }; // FIX: was {1,3,2,8,...} - no 8 in the original default table
            if (auto* player = FindPlayerPed(-1)) {
                cop = player->GetPlayerData()->m_pArrestingCop;
                // FIX: comparison was inverted (original: rand/32768 > 0.5, asm 0x5185A6-0x5185C4)
                if (cop && static_cast<float>(CGeneral::GetRandomNumber()) * RAND_SCALE > 0.5f) {
                    variants = { 1, 2, 3, 2, 8, -1 };
                }
            }
            break;
        }
        case ENTITY_TYPE_VEHICLE: {
            const auto tveh = target->AsVehicle();
            if (const auto driver = tveh->m_pDriver; driver && driver->IsPlayer()) {
                targetPos = driver->GetBonePosition(BONE_SPINE1, true);
                anchor    = driver;
            } else {
                targetPos = target->GetPosition();
                anchor    = nullptr;
            }
            variants = { 8, 3, 2, -1, -1, -1 };
            if (auto* player = FindPlayerPed(-1)) {
                cop = player->GetPlayerData()->m_pArrestingCop;
                // FIX: comparison was inverted (original: rand/32768 > 0.65, asm 0x5186A5-0x5186C3)
                if (cop && static_cast<float>(CGeneral::GetRandomNumber()) * RAND_SCALE > 0.65f) {
                    variants = { 2, 8, 3, 2, -1, -1 };
                }
            }
            break;
        }
        default:
            s_Mode = 0;
            return;
        }

        if (!gbSkipBustedMsg) {
            CMessages::AddBigMessage(TheText.Get("BUSTED"), 5000, STYLE_WHITE_MIDDLE);
        }

        // Try each variant, until one of them succeeds (which sets `s_Mode`)
        for (int32 i = 0; s_Mode == 0 && i <= 5; i++) {
            const auto variant = variants[i];
            if (variant < 1) {
                break;
            }

            s_ArrestingCop = nullptr;

            bool found = false;
            switch (variant) {
            case 1: {
                // DW-style cop-face cam (handled completely by the helper)
                SetStartTime();
                if (plugin::CallMethodAndReturn<bool, 0x512EF0>(this, cop, true)) {
                    target->m_nFlags &= ~0x80; // m_bIsVisible = false
                    s_Mode          = 1;
                    m_bResetStatics = false;
                    return;
                }
                break;
            }
            case 2:
            case 3: {
                if (!cop) {
                    // Search the anchor's ped scanner for a cop that is arresting the player
                    if (anchor) {
                        for (auto* ent : anchor->GetIntelligence()->m_pedScanner.m_apEntities) {
                            if (!ent) {
                                continue;
                            }
                            const auto entry = ent->AsPed();
                            if (const auto task = entry->GetIntelligence()->m_TaskMgr.FindActiveTaskByType(TASK_SIMPLE_ARREST_PED)) {
                                if (notsa::cast<CTaskSimpleArrestPed>(task)->m_Ped == FindPlayerPed(-1)
                                 && GetLookFromCopSidePos(target, entry, &targetPos, &outPos)
                                ) {
                                    s_ArrestingCop = entry;
                                    found = true;
                                    break;
                                }
                            }
                        }
                    }
                } else {
                    // Already know the cop - use him
                    const auto res = variant == 2
                        ? GetLookFromCopSidePos(target, cop, &targetPos, &outPos)
                        : GetLookFromCopAbovePos(target, cop, &targetPos, &outPos);
                    s_ArrestingCop = cop;
                    cop = nullptr;
                    found = res;
                }
                break;
            }
            case 8: {
                // Lamp post cam (if possible)
                found = GetLookFromLampPostPos(target, cop, targetPos, outPos);
                break;
            }
            }

            if (found) {
                if (s_ArrestingCop) {
                    s_ArrestingCop->RegisterReference(reinterpret_cast<CEntity**>(&s_ArrestingCop));
                }

                s_Mode = variant;
                if (s_Mode == 3) {
                    // Pick a random one of the 3..7 variants
                    const auto r = static_cast<float>(CGeneral::GetRandomNumber()) * RAND_SCALE * 5.0f;
                    if      (r <= 1.0f) s_Mode = 3;
                    else if (r <= 2.0f) s_Mode = 4;
                    else if (r <= 3.0f) s_Mode = 5;
                    else if (r <= 4.0f) s_Mode = 6;
                    else                s_Mode = 7;
                }

                FinaliseSource();
                return;
            }
        }

        // NOTE: In the original `outPos` is garbage here - we just use whatever got computed (or zeros)
        FinaliseSource();
        return;
    }

    if (s_Mode == 1) {
        target->m_nFlags &= ~0x80; // m_bIsVisible = false
        // FIX: original calls 0x512EF0 unconditionally, passing a possibly-null cop
        // (asm 0x518B76-0x518BA0); we used to skip the call when there was no cop.
        CPed* arrestingCop = nullptr;
        if (auto* player = FindPlayerPed(-1)) {
            arrestingCop = player->GetPlayerData()->m_pArrestingCop;
        }
        plugin::CallMethodAndReturn<bool, 0x512EF0>(this, arrestingCop, false);
        return;
    }

    if (!UpdateTargetPos()) {
        return;
    }

    if (s_Mode == 2) {
        if (s_ArrestingCop) {
            if (GetLookFromCopSidePos(target, s_ArrestingCop, &targetPos, &outPos)) {
                outPos.z = std::min(outPos.z, CTimer::ms_fTimeStep * StaticRef<float>(0x8CC7EC) + m_vecSource.z);
                FinaliseSource();
                return;
            }
        }
    } else if (s_Mode == 8) {
        auto src = m_vecSource;
        auto dir = targetPos - m_vecSource;
        m_vecFront = dir;
        m_vecFront.z = 0.0f;
        m_vecFront.Normalise();

        m_vecUp = CVector{ 0.0f, 0.0f, 1.0f };
        const auto right = CrossProduct(m_vecFront, m_vecUp).Normalized();

        dir = targetPos - m_vecSource;
        m_vecFront = dir + right * StaticRef<float>(0x8CC7FC);
        m_vecFront.z = 0.0f;
        m_vecFront.Normalise();

        outPos = m_vecSource + m_vecFront * StaticRef<float>(0x858B8C);
        if (!CWorld::TestSphereAgainstWorld(outPos, 0.4f, target, true, true, false, true, false, true)) {
            outPos = src + m_vecFront * (StaticRef<float>(0x8CC800) * CTimer::ms_fTimeStep);
            FinaliseSource();
            return;
        }
    } else if (s_Mode >= 4 && s_Mode <= 7) {
        const auto src = m_vecSource;
        const auto dir = targetPos - m_vecSource;
        m_vecFront = dir;
        m_vecFront.Normalise();

        m_vecUp = CVector{ 0.0f, 0.0f, 1.0f };
        auto side = CrossProduct(m_vecFront, m_vecUp);
        if (s_Mode == 6 || s_Mode == 7) {
            side *= StaticRef<float>(0x858C1C); // -1
        }

        const auto cand = src + side * StaticRef<float>(0x858B8C);
        if (!CWorld::TestSphereAgainstWorld(cand, 0.4f, target, true, true, false, true, false, true)) {
            outPos = src + side * (StaticRef<float>(0x8CC7E4) * CTimer::ms_fTimeStep);
            if (s_Mode == 5 || s_Mode == 7) {
                outPos.z += CTimer::ms_fTimeStep * StaticRef<float>(0x8CC7E8);
            } else {
                bool foundGround = false;
                if (const auto groundZ = CWorld::FindGroundZFor3DCoord(outPos, &foundGround); foundGround) {
                    outPos.z = groundZ + StaticRef<float>(0x8CC7F8);
                }
            }
            FinaliseSource();
            return;
        }
    }

    // Fallback: just move the source away from the target so it isn't obstructed
    CVector from = m_vecSource;
    ProcessCamSrc(&TheCamera, &from, &targetPos, &m_vecSource, m_fFOV);
}

// 0x519250
void CCam::ProcessPedsDeadBaby() {
    static auto& s_DeathStartTime = StaticRef<uint32>(0xB6F094); // TheCamera+0x6C
    static auto& s_TimeAcc        = StaticRef<float>(0xB70054);
    static auto& s_TimeAcc2       = StaticRef<uint32>(0xB70050);
    static auto& gbFlag_55        = StaticRef<bool>(0xB6F055);
    static auto& ORBIT_RADIUS     = StaticRef<float>(0x858CC4); // 0.2
    static auto& TWO_POW_32       = StaticRef<float>(0x858C54); // 4294967296.0
    static auto& ORBIT_TIME_MAX   = StaticRef<float>(0x858C4C); // 1000.0 (ms)
    static auto& ORBIT_RAD_PER_MS = StaticRef<float>(0x858CDC); // 0.001 (2*pi / 1000 folded into one constant)
    static auto& Z_ABOVE_DEATH    = StaticRef<float>(0x8CC804); // 2.0
    static auto& RAISE_Z_STEP     = StaticRef<float>(0x8CC808); // 0.04
    static auto& ORBIT_AMP        = StaticRef<float>(0x8CC80C); // 2.0
    static auto& ORBIT_PERIOD     = StaticRef<float>(0x8CC810); // 600.0 (ms)
    static auto& SWING_AMP        = StaticRef<float>(0x8CC814); // -0.35
    static auto& SWING_TIME_MAX   = StaticRef<float>(0x8631F0); // 2000.0 (ms)
    static auto& SWING_SCALE      = StaticRef<float>(0x859978); // 0.0005
    static auto& WATER_MARGIN     = StaticRef<float>(0x858CE8); // 1.5

    auto* targetEntity = TheCamera.m_pTargetEntity;

    CVector deathPos{};
    switch (targetEntity->GetType()) {
    case ENTITY_TYPE_PED:
        deathPos = targetEntity->AsPed()->GetBonePosition(BONE_SPINE1, true);
        break;
    case ENTITY_TYPE_VEHICLE: {
        deathPos = targetEntity->GetPosition();
        deathPos.z += targetEntity->GetColModel()->m_boundBox.m_vecMax.z;
        break;
    }
    default:
        return;
    }

    const auto now = CTimer::GetTimeInMS();

    if (!m_bResetStatics) {
        // Orbit around the death position
        float srcX = m_vecSource.x;
        float srcY = m_vecSource.y;
        float srcZ = m_vecSource.z + ORBIT_RADIUS;

        // Raise above ground if needed
        if (!CWorld::TestSphereAgainstWorld(CVector{ srcX, srcY, srcZ }, 0.3f, targetEntity, true, true, false, true, false, true)) {
            srcZ += CTimer::GetTimeStep() * RAISE_Z_STEP;
        }

        if (!targetEntity->m_matrix) {
            targetEntity->AllocateMatrix();
            targetEntity->UpdateRwMatrix();
        }
        auto right = CrossProduct(targetEntity->m_matrix->GetUp(), CVector{ 0.0f, 0.0f, 1.0f }).Normalized();

        const auto elapsedRaw = float(now - s_DeathStartTime);
        const auto elapsed    = elapsedRaw < 0.0f ? elapsedRaw + TWO_POW_32 : elapsedRaw;
        const auto elapsedMax = std::min(elapsed, ORBIT_TIME_MAX);

        const auto orbit = elapsedMax * ORBIT_RAD_PER_MS * std::sin(elapsed / ORBIT_PERIOD);
        const CVector desired{
            deathPos.x + right.x * ORBIT_AMP * orbit,
            deathPos.y + right.y * ORBIT_AMP * orbit,
            srcZ
        };

        // Original: dir = normalise(desired.xy - src.xy); the test sphere is placed a
        // step of ORBIT_RADIUS away from the *current source*, not from the death pos.
        const auto dir = CVector2D{ desired.x - srcX, desired.y - srcY }.Normalized();
        const CVector candidate{
            srcX + dir.x * ORBIT_RADIUS,
            srcY + dir.y * ORBIT_RADIUS,
            srcZ
        };
        if (!CWorld::TestSphereAgainstWorld(candidate, 0.3f, targetEntity, true, true, false, true, false, true)) {
            srcX = desired.x;
            srcY = desired.y;
            srcZ = desired.z;
        }

        m_vecFront = CVector{ 0.0f, 0.0f, -1.0f };

        // Swing the front vector outwards over time
        const auto swing = SWING_AMP * std::min(elapsed, SWING_TIME_MAX) * SWING_SCALE * std::cos(elapsed / ORBIT_PERIOD);
        m_vecFront.x += right.x * swing;
        m_vecFront.y += right.y * swing;
        m_vecFront.Normalise();

        m_vecUp = CrossProduct(right, m_vecFront).Normalized();

        m_vecSource = CVector{ srcX, srcY, srcZ };
    } else {
        // First frame: place the camera above the death position
        {
            // Original: (float)(int)CTimer::m_snTimeInMilliseconds, +2^32 if negative
            const auto t = static_cast<float>(now);
            s_TimeAcc  = t < 0.0f ? t + TWO_POW_32 : t;
        }
        s_TimeAcc2 = 0;

        float targetZ = deathPos.z + Z_ABOVE_DEATH;
        s_DeathStartTime = now;

        if (float wl{}; CWaterLevel::GetWaterLevelNoWaves(deathPos, &wl) && targetZ < wl + WATER_MARGIN) {
            targetZ = wl + WATER_MARGIN;
        }

        if (!targetEntity->m_matrix) {
            targetEntity->AllocateMatrix();
            targetEntity->UpdateRwMatrix();
        }
        auto right = CrossProduct(targetEntity->m_matrix->GetUp(), CVector{ 0.0f, 0.0f, 1.0f }).Normalized();

        m_vecFront = deathPos - CVector{ deathPos.x, deathPos.y, targetZ };
        m_vecFront.Normalise();

        m_vecUp = CrossProduct(right, m_vecFront).Normalized();

        m_vecSource     = CVector{ deathPos.x, deathPos.y, targetZ };
        m_bResetStatics = false;
    }

    // 0x514030 -- unreversed helper: CCamera::__thiscall(from, target, inOutSrc, fov), this = TheCamera (0xB6F028)
    CVector srcCopy = m_vecSource;
    plugin::CallMethodAndReturn<void, 0x514030>(&TheCamera, &srcCopy, &deathPos, &m_vecSource, m_fFOV);

    gbFlag_55 = false;
}

// 0x50EB70
void CCam::Process_1rstPersonPedOnPC(const CVector& target, float orientation, float speedVar, float speedVarWanted) {
    static auto& v3d_8CCC54   = StaticRef<CVector>(0x8CCC54);
    static auto& byte_B6FFDC  = StaticRef<bool>(0xB6FFDC);
    static auto& v3d_B6FFC4   = StaticRef<CVector>(0xB6FFC4);
    static auto& v3d_B6FFD0   = StaticRef<CVector>(0xB6FFD0);

    if (m_nMode != MODE_SNIPER_RUNABOUT) {
        m_fFOV = 70.0f;
    }

    if (!m_pCamTargetEntity->GetRwObject()) {
        return;
    }

    if (!m_pCamTargetEntity->GetIsTypePed()) {
        m_bResetStatics = false;
        RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.05f);
        return;
    }

    const auto hier = GetAnimHierarchyFromSkinClump(m_pCamTargetEntity->GetRpClump());
    const auto aIdx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(2)); // todo: enum
    auto&      aMat = RpHAnimHierarchyGetMatrixArray(hier)[aIdx];
    auto*      targetPed = m_pCamTargetEntity->AsPed();

    CVector pointIn = v3d_8CCC54;
    RwV3dTransformPoint(&pointIn, &pointIn, &aMat);
    RwV3d v3dZero{ 0.0f };
    RwMatrixScale(&aMat, &v3dZero, rwCOMBINEPRECONCAT);

    if (m_bResetStatics) {
        // unnecessary entity ped check
        m_fVerticalAngle = 0.0f;
        byte_B6FFDC      = false;
        v3d_B6FFD0.Reset();
        m_fHorizontalAngle            = targetPed->m_fCurrentRotation + DegreesToRadians(90.0f);
        m_bCollisionChecksOn          = true;
        m_fInitialPlayerOrientation   = m_fHorizontalAngle;
        m_vecBufferedPlayerBodyOffset = v3d_B6FFC4 = pointIn;
    }
    m_vecBufferedPlayerBodyOffset.y = pointIn.y;

    if (TheCamera.m_bHeadBob) {
        m_vecBufferedPlayerBodyOffset.x = lerp(
            pointIn.x,
            m_vecBufferedPlayerBodyOffset.x,
            TheCamera.m_fScriptPercentageInterToCatchUp
        );

        m_vecBufferedPlayerBodyOffset.z = lerp(
            pointIn.z,
            m_vecBufferedPlayerBodyOffset.z,
            TheCamera.m_fScriptPercentageInterToCatchUp
        );

        m_vecSource = targetPed->GetMatrix().TransformPoint(m_vecBufferedPlayerBodyOffset);
    } else {
        const auto targetFwd = targetPed->GetForward().Normalized();
        const auto mag       = (pointIn - v3d_B6FFC4).Magnitude2D();

        m_vecSource = targetFwd * mag * 1.23f + targetPed->GetPosition() + CVector{ 0.0f, 0.0f, 0.59f };
    }

    CVector spinePos{};
    targetPed->GetTransformedBonePosition(spinePos, BONE_SPINE1, true);

    // TODO: Put in a function name e.g. 'HandleFreeMouseControl'?
    auto*      pad1   = CPad::GetPad(0);
    const auto fov    = m_fFOV / 80.0f;
    const auto amountMouseMoved = pad1->NewMouseControllerState.GetAmountMouseMoved();

    if (!amountMouseMoved.IsZero()) {
        m_fHorizontalAngle += -3.0f * amountMouseMoved.x * fov * CCamera::m_fMouseAccelHorzntl;
        m_fVerticalAngle += +4.0f * amountMouseMoved.y * fov * CCamera::m_fMouseAccelVertical;
    } else {
        const auto hv = (float)-pad1->LookAroundLeftRight(targetPed);
        const auto vv = (float)pad1->LookAroundUpDown(targetPed);

        m_fHorizontalAngle += sq(hv) / 10000.0f * fov / 17.5f * CTimer::GetTimeStep() * (hv < 0.0f ? -1.0f : 1.0f);
        m_fVerticalAngle += sq(vv) / 22500.0f * fov / 14.0f * CTimer::GetTimeStep() * (vv < 0.0f ? -1.0f : 1.0f);
    }
    ClipBeta();
    ClipAlpha();

    if (const auto* a = targetPed->m_pAttachedTo; targetPed->IsPlayer() && a) {
        // enum?
        switch (targetPed->m_fTurretAngleA) {
        case 0u:
            m_fHorizontalAngle -= a->GetHeading() + DegreesToRadians(90.0f);
            break;
        case 1u:
            m_fHorizontalAngle -= a->GetHeading() + DegreesToRadians(180.0f);
            break;
        case 2u:
            m_fHorizontalAngle -= a->GetHeading() + DegreesToRadians(-90.0f);
            break;
        case 3u:
            m_fHorizontalAngle -= a->GetHeading();
            break;
        default:
            // NOTE(yukani): If this is fired, gimme a call. 0x50F0ED
            NOTSA_UNREACHABLE();
            break;
        }

        // ...
    }
}

// 0x517EA0
void CCam::Process_1stPerson(const CVector& target, float orientation, float speedVar, float speedVarWanted) {
    static auto& s_LastWheelieTime = StaticRef<float>(0x8CCD14);
    // Making sure player doesn't see below ground when flipped.
    // Name is made up cuz I found it funny to name it like that.
    static auto& s_GroundFaultProtection = StaticRef<float>(0xB7004C);

    gbFirstPersonRunThisFrame = true;

    m_fFOV = 70.0f;
    if (!m_pCamTargetEntity->GetRwObject()) {
        return;
    }

    if (m_bResetStatics) {
        m_fVerticalAngle   = 0.0f;
        m_fHorizontalAngle = [&] {
            if (m_pCamTargetEntity->GetIsTypePed()) {
                return m_pCamTargetEntity->AsPed()->m_fCurrentRotation + DegreesToRadians(90.0f);
            } else {
                return orientation;
            }
        }();
        m_fInitialPlayerOrientation = m_fHorizontalAngle;

        s_GroundFaultProtection                 = 0.0f;
        TheCamera.m_fAvoidTheGeometryProbsTimer = 0.0f;
    }

    if (m_pCamTargetEntity->GetIsTypePed()) {
        m_bResetStatics = false;
        return;
    }

    const auto wheelieTime = static_cast<float>(CTimer::GetTimeInMS());
    if (s_LastWheelieTime > wheelieTime) {
        s_LastWheelieTime = 0.0f;
    }

    auto* targetVeh = m_pCamTargetEntity->AsVehicle();
    if (targetVeh->IsBike() && targetVeh->AsBike()->bikeFlags.bWheelieForCamera || TheCamera.m_fAvoidTheGeometryProbsTimer > 0.0f) {
        if (wheelieTime - s_LastWheelieTime >= 3000.0f) {
            s_LastWheelieTime = static_cast<float>(CTimer::GetTimeInMS());
        }

        const auto pad1 = CPad::GetPad();
        if (!pad1->NewState.LeftShoulder2 && !pad1->NewState.RightShoulder2) {
            auto* targetBike = targetVeh->AsBike();
            if (Process_WheelCam(target, orientation, speedVar, speedVarWanted)) {
                if (targetBike->bikeFlags.bWheelieForCamera) {
                    TheCamera.m_fAvoidTheGeometryProbsTimer = 50.0f;
                } else {
                    TheCamera.m_fAvoidTheGeometryProbsTimer -= CTimer::GetTimeStep();
                    targetBike->bikeFlags.bWheelieForCamera = true;
                }
                return;
            }
            TheCamera.m_fAvoidTheGeometryProbsTimer = 0.0f;
            targetBike->bikeFlags.bWheelieForCamera = false;

            s_LastWheelieTime = 0.0f;
        }
    }

    const auto& entityWorldMat = [&] {
        if (auto* t = targetVeh->AsBike(); t->IsBike()) {
            t->CalculateLeanMatrix();
            return t->m_mLeanMatrix;
        } else {
            return targetVeh->GetMatrix();
        }
    }();

    const auto dummyPos = [&] {
        const auto* vehStruct = targetVeh->GetVehicleModelInfo()->GetVehicleStruct();
        return vehStruct->m_avDummyPos[targetVeh->IsBoat() ? DUMMY_LIGHT_FRONT_MAIN : DUMMY_SEAT_FRONT] * CVector{0.0f, 1.0f, 1.0f}; // ignore x
    }() + CVector{ 0.0f, 0.08f, 0.62f };

    m_fFOV = 60.0f;
    m_vecSource = entityWorldMat.TransformVector(dummyPos);
    m_vecSource += targetVeh->GetPosition();

    if (targetVeh->IsBike() && targetVeh->m_pDriver) {
        auto*   targetBike = targetVeh->AsBike();
        CVector neckPos{};

        targetVeh->m_pDriver->GetTransformedBonePosition(neckPos, BONE_NECK, true);
        neckPos += targetBike->GetMoveSpeed() * CTimer::GetTimeStep();

        constexpr auto BIKE_1ST_PERSON_ZOFFSET = 0.15f; // 0x8CC7B4
        m_vecSource.z = neckPos.z + BIKE_1ST_PERSON_ZOFFSET;

        const auto right = CrossProduct(m_vecFront, m_vecUp);
        // right *= flt_8CCD0C; (=1.0f)

        if (!CWorld::GetIsLineOfSightClear(
            CrossProduct(m_vecSource, m_vecSource + right),
            CrossProduct(m_vecSource, m_vecSource - right),
            true,
            false,
            false,
            false
        )) {
            m_vecSource = targetBike->GetPosition();
            m_vecSource.z = neckPos.z + BIKE_1ST_PERSON_ZOFFSET + 0.62f;
        }
    } else if (targetVeh->IsBoat()) {
        m_vecSource.z += 0.5f;
    }

    // todo: refactor
    if (targetVeh->IsUpsideDown()) {
        if (s_GroundFaultProtection >= 0.5f) {
            s_GroundFaultProtection = 0.5f;
        } else {
            s_GroundFaultProtection += 0.03f;
        }
    } else if (s_GroundFaultProtection >= 0.0f) {
        s_GroundFaultProtection = 0.0f;
    } else {
        s_GroundFaultProtection -= 0.03f;
    }
    m_vecSource.z += s_GroundFaultProtection;

    m_vecFront = entityWorldMat.GetForward().Normalized();
    m_vecUp    = entityWorldMat.GetUp().Normalized();
    const auto a = CrossProduct(m_vecFront, m_vecUp).Normalized();
    m_vecUp = CrossProduct(a, m_vecFront).Normalized();

    if (float wl{}; CWaterLevel::GetWaterLevel(m_vecSource, wl, true) && m_vecSource.z < wl - 0.3f) {
        ApplyUnderwaterMotionBlur();
    }
    m_bResetStatics = false;
}

// 0x521500
// STILL DISABLED: the per-weapon beta/alpha clamping section (WellBufferMe on beta/alpha,
// weapon-table clamps 0x8CC4D4/0x8CC4D8, wi->flags.bAimWithArm handling) is mangled in the
// decompilation; the rest of the first-pass translation is unusable without it.
void CCam::Process_AimWeapon(const CVector&, float, float, float) {
    NOTSA_UNREACHABLE();
}

// 0x512B10
void CCam::Process_AttachedCam() {
    static auto& s_AttachedEntity  = StaticRef<CEntity*>(0xB6F984);  // entity the cam is attached to
    static auto& s_AttachedOffset  = StaticRef<CVector>(0xB6F960);   // offset on the attached entity
    static auto& s_AttachedOffset2 = StaticRef<CVector>(0xB6F96C);   // second (front) offset
    static auto& s_RotateAngle     = StaticRef<float>(0xB6F978);     // rotation angle
    static auto& s_TargetEntity    = StaticRef<CEntity*>(0xB6F980);
    static auto& s_bUseOffset2     = StaticRef<bool>(0xB6F054);
    static auto& TIME_SCALE        = StaticRef<float>(0x8595EC);
    static auto& WATER_MINDIST     = StaticRef<float>(0x858C24);

    m_fFOV = 70.0f;

    const auto angle = s_RotateAngle * TIME_SCALE;

    auto* attached = s_AttachedEntity;
    if (!attached->m_matrix) {
        attached->AllocateMatrix();
        attached->UpdateRwMatrix();
    }
    m_vecSource = attached->GetMatrix().TransformVector(s_AttachedOffset) + attached->GetPosition();

    if (!s_bUseOffset2) {
        m_vecFront = s_TargetEntity->GetPosition() - m_vecSource;
    } else {
        if (!attached->m_matrix) {
            attached->AllocateMatrix();
            attached->UpdateRwMatrix();
        }
        m_vecFront = attached->GetMatrix().TransformVector(s_AttachedOffset2) + attached->GetPosition() - m_vecSource;
    }
    m_vecFront.Normalise();

    auto right = CrossProduct(m_vecFront, CVector{ 0.0f, 0.0f, 1.0f }).Normalized();
    m_vecUp    = CrossProduct(right, m_vecFront).Normalized();

    if (float wl{}; CWaterLevel::GetWaterLevel(m_vecSource, wl, true) && m_vecSource.z < wl - WATER_MINDIST) {
        // Underwater -> set up motion blur
        TheCamera.m_nBlurRed    = static_cast<uint32>(CTimeCycle::GetWaterRed());
        TheCamera.m_nBlurGreen  = static_cast<uint32>(CTimeCycle::GetWaterGreen());
        TheCamera.m_nBlurBlue   = static_cast<uint32>(CTimeCycle::GetWaterBlue());
        TheCamera.m_nBlurType   = eMotionBlurType::LIGHT_SCENE;
        TheCamera.m_nMotionBlur = 20; // 0x8CC7A4
    }

    // Rotate the up vector around the front vector
    const auto s = std::sin(angle);
    const auto c = std::cos(angle);
    m_vecUp = m_vecUp * c + right * s;

    CWorld::pIgnoreEntity = nullptr;
}

// 0x525E50
void CCam::Process_Cam_TwoPlayer() {
    static auto& s_TargetPlayer    = StaticRef<uint32>(0x8A5E50);
    static auto& gLastTime2PCamOK  = StaticRef<uint32>(0xB6EC24);
    static auto& gLastTime2PMoved  = StaticRef<uint32>(0xB6EC28);
    static auto& s_LastHelpTime    = StaticRef<uint32>(0x96A8B4);
    static auto& s_HelpMsgCount    = StaticRef<uint32>(0x96A8B8);
    static auto& BETA_STEP         = StaticRef<float>(0x858FCC);
    static auto& PI                = StaticRef<float>(0x858CB8);
    static auto& TWO_PI            = StaticRef<float>(0x858CBC);
    static auto& PI_NEG            = StaticRef<float>(0x858CC0);
    static auto& HALF_PI           = StaticRef<float>(0x858FE4);
    static auto& ONE               = StaticRef<float>(0x858624);
    static auto& HALF              = StaticRef<float>(0x858B50);
    static auto& EPS               = StaticRef<float>(0x858C58);
    static auto& TURN_LERP         = StaticRef<float>(0x862F58);
    static auto& TURN_MAX          = StaticRef<float>(0x862F5C);
    static auto& TURN_SPD1         = StaticRef<float>(0x8CC5E8);
    static auto& TURN_SPD2         = StaticRef<float>(0x8CC5EC);
    static auto& TURN_MAX2         = StaticRef<float>(0x8CC5E4);
    static auto& FOV_SCALE         = StaticRef<float>(0x85A5C4);
    static auto& FOV_MUL           = StaticRef<float>(0x859B50);
    static auto& STICK_SCL1        = StaticRef<float>(0x8CC4A0);
    static auto& STICK_LIM         = StaticRef<float>(0x858BF4);
    static auto& STICK_LIM2        = StaticRef<float>(0x85F3C4);

    auto* p1 = CWorld::Players[0].m_pPed;
    auto* p2 = CWorld::Players[1].m_pPed;

    const auto switchState = plugin::CallMethodAndReturn<bool, 0x5404A0>(CPad::GetPad(0)); // 0x5404A0 -- unresolved pad helper
    if (!switchState) {
        if (plugin::CallMethodAndReturn<bool, 0x5404A0>(CPad::GetPad(1))) {
            s_TargetPlayer = (s_TargetPlayer == 1) + 1;
        }
    } else {
        s_TargetPlayer = ((s_TargetPlayer != 0) - 1) & 2;
    }

    if (s_TargetPlayer == 0) {
        if (p1->bInVehicle && p1->m_pVehicle) {
            m_pCamTargetEntity = p1->m_pVehicle;
            Process_FollowCar_SA(m_pCamTargetEntity->GetPosition(), 0.0f, 0.0f, 0.0f, false);
            m_pCamTargetEntity = p1;
            m_bResetStatics = false;
            return;
        }
        Process_FollowPed_SA(p1->GetPosition(), 0.0f, 0.0f, 0.0f, false);
        m_bResetStatics = false;
        return;
    }

    if (s_TargetPlayer == 1) {
        if (p2->bInVehicle && p2->m_pVehicle) {
            m_pCamTargetEntity = p2->m_pVehicle;
            Process_FollowCar_SA(m_pCamTargetEntity->GetPosition(), 0.0f, 0.0f, 0.0f, false);
            m_pCamTargetEntity = p1;
            m_bResetStatics = false;
            return;
        }
        m_pCamTargetEntity = p2;
        Process_FollowPed_SA(p2->GetPosition(), 0.0f, 0.0f, 0.0f, false);
        m_pCamTargetEntity = p1;
        m_bResetStatics = false;
        return;
    }

    // both players on screen
    const auto now      = CTimer::GetTimeInMS();
    const auto timeStep = CTimer::GetTimeStep();

    m_fVerticalAngle = -0.5238f; // 0xBF060A92
    m_fAlphaSpeed    = 0.0f;

    // find a beta that gives a valid camera position
    uint32 foundIdx{};
    float  bestBeta = m_fHorizontalAngle;
    for (uint32 i = 0; i < 0x15; ++i) {
        const auto beta = (i & 1) == 0
            ? m_fHorizontalAngle - (float)(i + 1) / 2.0f * BETA_STEP
            : (float)(i + 1) / 2.0f * BETA_STEP + m_fHorizontalAngle;

        CVector src{}, front{};
        plugin::CallMethod<0x5132D0>(this, beta, &src, &front, &m_vecTargetCoorsForFudgeInter); // 0x5132D0 -- unresolved
        // 0x513220 -- unresolved validity check
        if (plugin::CallMethodAndReturn<bool, 0x513220>(this, src, front, CVector{})) {
            gLastTime2PCamOK = now;
            foundIdx = i;
            bestBeta = beta;
            break;
        }
        foundIdx = i + 1;
    }
    if (foundIdx == 0x15) {
        bestBeta = m_fHorizontalAngle;
        gLastTime2PMoved = now;
    } else if ((int32)foundIdx > 0) {
        gLastTime2PMoved = now;
    }

    if (m_bResetStatics) {
        m_fHorizontalAngle = bestBeta;
    }

    auto betaDiff = bestBeta - m_fHorizontalAngle;
    if (betaDiff <= PI) {
        if (betaDiff < PI_NEG) {
            betaDiff += TWO_PI;
        }
    } else {
        betaDiff -= TWO_PI;
    }
    auto turn    = betaDiff * TURN_LERP;
    auto turnMax = TURN_MAX;
    if (turn <= TURN_MAX) {
        if (turn < -TURN_MAX) {
            turnMax = -TURN_MAX;
        }
    } else {
        turnMax = TURN_MAX;
    }
    turn = turnMax;
    auto beta = turn + m_fHorizontalAngle;

    float ofs{};
    if (foundIdx != 0 && now >= gLastTime2PMoved + 1000) {
        const auto speedSum = p1->m_vecMoveSpeed + p2->m_vecMoveSpeed;
        const auto speed2   = DotProduct(speedSum, speedSum);
        if (speed2 <= EPS) {
            beta -= BETA_STEP;
        } else {
            const auto k1 = TURN_SPD1 * timeStep;
            const auto k2 = TURN_SPD2 * timeStep;
            auto ang = CGeneral::GetATanOfXY(-speedSum.x, speedSum.y) - HALF_PI;
            auto rel = ang - beta;
            if (rel <= PI) {
                if (rel < PI_NEG) {
                    ang += TWO_PI;
                }
            } else {
                ang -= TWO_PI;
            }
            const auto kk = (speedSum.Magnitude() * k1 <= ONE) ? speedSum.Magnitude() * k1 : ONE;
            const auto delta = (ang - beta) * kk;
            ofs = (delta <= k2) ? ((delta < -k2) ? -k2 : delta) : k2;
            if (ofs > EPS) {
                beta += BETA_STEP;
            } else if (ofs < -EPS) {
                beta -= BETA_STEP;
            }
        }

        if (std::abs(ofs) > EPS) {
            CVector src{}, front{};
            plugin::CallMethod<0x5132D0>(this, beta, &src, &front, &m_vecTargetCoorsForFudgeInter);
            if (!plugin::CallMethodAndReturn<bool, 0x513220>(this, src, front, CVector{})) {
                if ((ofs > HALF && HALF < m_fBetaSpeed) || (ofs < HALF && m_fBetaSpeed < HALF)) {
                    m_fBetaSpeed = 0.0f;
                }
                gLastTime2PMoved = now;
                ofs = 0.0f;
            }
        }
    }

    beta = ofs + beta;
    if (beta <= m_fHorizontalAngle + PI) {
        if (beta < m_fHorizontalAngle - PI) {
            beta += TWO_PI;
        }
    } else {
        beta -= TWO_PI;
    }

    auto ts = timeStep;
    if (ts < ONE) {
        ts = ONE;
    }
    auto turnSpd = (beta - m_fHorizontalAngle) / ts;
    // Original: pow(0x8CC5E0 /* 0.8 */, CTimer::ms_fTimeStep) via the CRT pow dispatcher
    // at 0x822130 (args passed on the FPU stack; asm 0x5262F2-0x526303).
    const auto damp = std::pow(StaticRef<float>(0x8CC5E0), CTimer::GetTimeStep()); // 0.8

    if (foundIdx == 0 && gLastTime2PMoved + 1000 <= now) {
        const auto s1 = CPad::GetPad(0)->AimWeaponLeftRight(p1);
        const auto s2 = CPad::GetPad(1)->AimWeaponLeftRight(p2);
        auto stick = -(float)s1 - (float)s2;
        auto stickLim = STICK_LIM;
        if (stick <= STICK_LIM) {
            if (stick < STICK_LIM2) {
                stickLim = STICK_LIM2;
            }
        } else {
            stickLim = STICK_LIM;
        }
        stick = m_fFOV * FOV_SCALE * FOV_MUL * std::abs(stick) * STICK_SCL1 * STICK_SCL1 * stick;
        if (stick > EPS) {
            bestBeta += BETA_STEP;
        } else if (stick < -EPS) {
            bestBeta -= BETA_STEP;
        }
        if (std::abs(stick) > EPS) {
            CVector src{}, front{};
            plugin::CallMethod<0x5132D0>(this, bestBeta, &src, &front, &m_vecTargetCoorsForFudgeInter);
            if (!plugin::CallMethodAndReturn<bool, 0x513220>(this, src, front, CVector{})) {
                if ((stick > HALF && HALF < m_fBetaSpeed) || (stick < HALF && m_fBetaSpeed < HALF)) {
                    m_fBetaSpeed = 0.0f;
                }
                gLastTime2PMoved = now;
                stick = 0.0f;
            }
        }
        turnSpd = stick + turnSpd;
    }

    {
        auto lim = TURN_MAX2;
        if (turnSpd <= TURN_MAX2) {
            if (turnSpd < -TURN_MAX2) {
                lim = -TURN_MAX2;
            }
        } else {
            lim = TURN_MAX2;
        }
        turnSpd = lim;
    }

    m_fBetaSpeed       = damp * m_fBetaSpeed + (ONE - damp) * turnSpd;
    m_fHorizontalAngle = m_fBetaSpeed * timeStep + m_fHorizontalAngle;

    plugin::CallMethod<0x5132D0>(this, m_fHorizontalAngle, &m_vecSource, &m_vecFront, &m_vecTargetCoorsForFudgeInter);

    if (foundIdx == 0x15 && now - gLastTime2PCamOK > 500) {
        CColPoint cp{};
        CEntity*  hitEnt{};
        if (CWorld::ProcessLineOfSight(m_vecTargetCoorsForFudgeInter, m_vecSource, cp, hitEnt, true, false, false, false, false, true, true, false)) {
            m_vecSource = cp.m_vecPoint;
        }
        if (s_LastHelpTime < now && s_HelpMsgCount < 6) {
            CHud::SetHelpMessage(reinterpret_cast<const GxtChar*>("WRN2_2P"), false, false, false); // 0x588BE0
            s_LastHelpTime = now + 60000;
            s_HelpMsgCount++;
        }
    }

    m_vecUp = CVector{ 0.0f, 0.0f, 1.0f };
    m_vecUp.Normalise();
    auto right = CrossProduct(m_vecUp, m_vecFront);
    right.Normalise();
    m_vecUp = CrossProduct(m_vecFront, right);
    m_vecUp.Normalise();

    m_fFOV = 70.0f; // 0x428C0000
    m_bResetStatics = false;
}

// 0x519810
// STILL DISABLED: the camera-orbit and shooter lean-out sections are collapsed; several
// scratch locals of the first-pass translation are never assigned.
void CCam::Process_Cam_TwoPlayer_InCarAndShooting() {
    NOTSA_UNREACHABLE();
}

// 0x513510
void CCam::Process_Cam_TwoPlayer_Separate_Cars() {
    static auto& DIR_MULT   = StaticRef<float>(0x858B44);
    static auto& ONE        = StaticRef<float>(0x858624);
    static auto& SCALE      = StaticRef<float>(0x858B8C);
    static auto& ANGLE_MULT = StaticRef<float>(0x858CB8);
    static auto& SWING_MULT = StaticRef<float>(0x858F34);
    static auto& BLEND_MIN  = StaticRef<float>(0x858B50);
    static auto& BLEND_RATE = StaticRef<float>(0x8CCCB4);
    static auto& FOCUS_DOT  = StaticRef<float>(0x8631CC);

    m_fFOV = 80.0f;

    // NOTE: The original game abuses the two CVehicle* fields as scratch:
    // 0x110 - which player's car we look at (0/1), 0x114 - 0..1 blend factor
    auto& focusIdx = reinterpret_cast<int32&>(m_pCarWeAreFocussingOn);
    auto& blend    = reinterpret_cast<float&>(m_pCarWeAreFocussingOnI);

    CVehicle* vehicles[2] = { CWorld::Players[0].m_pPed->m_pVehicle, CWorld::Players[1].m_pPed->m_pVehicle };
    const CVector pos2 = vehicles[0]->GetPosition();
    const CVector pos1 = vehicles[1]->GetPosition();

    auto dir = (pos2 - pos1).Normalized();

    const CVector point1{
        pos1.x - dir.x * DIR_MULT,
        pos1.y - dir.y * DIR_MULT,
        (pos1.z + vehicles[1]->GetModelInfo()->GetColModel()->m_boundSphere.m_vecCenter.z + ONE) - dir.z * DIR_MULT
    };
    const CVector point2{
        dir.x * DIR_MULT + pos2.x,
        dir.y * DIR_MULT + pos2.y,
        dir.z * DIR_MULT + pos2.z + vehicles[0]->GetModelInfo()->GetColModel()->m_boundSphere.m_vecCenter.z + ONE
    };

    const auto dirFrom1 = (pos1 - point1).Normalized();
    const auto dirFrom2 = (pos2 - point2).Normalized();
    auto perp = CVector{ point2.x - point1.x, point2.y - point1.y, 0.0f }.Normalized();
    perp.z = -0.1f;
    perp.Normalise();

    const auto ang     = blend * ANGLE_MULT;
    const auto sine    = std::sin(ang);
    const auto cosk    = (std::cos(ang) + ONE) * SCALE;
    const auto dist    = (pos2 - pos1).Magnitude();
    const auto oneMcos = ONE - cosk;
    const auto oneMsin = ONE - sine;

    m_vecSource = point1 * cosk + point2 * oneMcos - perp * (sine * dist * SWING_MULT);

    m_vecFront = (dirFrom1 * cosk + dirFrom2 * oneMcos) * oneMsin + perp * sine;
    m_vecFront.Normalise();

    m_vecTargetCoorsForFudgeInter = focusIdx == 0 ? pos2 : pos1;

    m_vecUp = CVector{ 0.0f, 0.0f, 1.0f };
    m_vecUp.Normalise();
    const auto right = CrossProduct(m_vecUp, m_vecFront).Normalized();
    m_vecUp = CrossProduct(m_vecFront, right).Normalized();

    // Switch the focused car if we ended up looking at the other one
    {
        const auto* curVeh = vehicles[focusIdx];
        const auto* othVeh = vehicles[(uint32)(focusIdx - 1u) & 1u];
        const auto  fvec   = CVector{ m_vecFront.x, m_vecFront.y, 0.0f }.Normalized();
        const auto  dot    = DotProduct(curVeh->GetForward(), fvec);
        if (dot < FOCUS_DOT && dot < -DotProduct(othVeh->GetForward(), fvec)) {
            focusIdx = (uint32)(focusIdx - 1u) & 1u;
        }
    }

    if (focusIdx == 0) {
        blend -= CTimer::GetTimeStep() * BLEND_RATE;
        if (blend <= BLEND_MIN) {
            blend = BLEND_MIN;
        }
    } else {
        blend += CTimer::GetTimeStep() * BLEND_RATE;
        if (blend >= ONE) {
            blend = ONE;
        }
    }
}

// 0x513BE0
void CCam::Process_Cam_TwoPlayer_Separate_Cars_TopDown() {
    m_fFOV = 80.0f;
    const auto p1 = FindPlayerEntity(PED_TYPE_PLAYER1), p2 = FindPlayerEntity(PED_TYPE_PLAYER2);

    const auto p1p2Centroid = (p1->GetPosition() + p2->GetPosition()) / 2.0f;
    const auto p1p2Distance = DistanceBetweenPoints(p1->GetPosition(), p2->GetPosition());

    auto camHeightMult = std::abs([&] {
        if (FindPlayerVehicle(PED_TYPE_PLAYER1)) {
            return FindPlayerVehicle(PED_TYPE_PLAYER2) ? 1.0f : 0.75f;
        }
        return FindPlayerVehicle(PED_TYPE_PLAYER2) ? 0.75f : 0.45f;
    }() - m_fCameraHeightMultiplier);

    if (const auto s = CTimer::GetTimeStep() / 200.0f; camHeightMult >= s) {
        camHeightMult = (camHeightMult >= 0.0f ? s : -s) + m_fCameraHeightMultiplier;
    }
    m_fCameraHeightMultiplier = camHeightMult;

    const auto v21 = std::max(p1p2Distance + 10.0f, 30.0f);
    m_vecSource.Set(
        p1p2Centroid.x,
        p1p2Centroid.y - v21 * std::sin(0.4f),
        p1p2Centroid.z - v21 * -std::cos(0.4f)
    );
    m_vecFront.Set(0.0f, std::sin(0.4f), -std::cos(0.4f));
    m_vecUp.Set(0.0f, m_vecSource.y, std::sin(0.4f));
    m_vecTargetCoorsForFudgeInter = m_vecSource;
}

// 0x51B850
void CCam::Process_DW_BirdyCam(bool) {
    static auto& s_InitFlags    = StaticRef<uint32>(0xB70098);
    static auto& s_BlockedCount = StaticRef<int32>(0xB7007C);
    static auto& s_LampPos1     = StaticRef<CVector>(0xB70080);
    static auto& s_LampPos2     = StaticRef<CVector>(0xB7008C);
    static auto& DURATION       = StaticRef<float>(0x8CCBB0);  // duration of the cam 0x16 transition
    static auto& SEARCH_R1      = StaticRef<float>(0x8CCD70);
    static auto& SEARCH_R2      = StaticRef<float>(0x8CCD68);
    static auto& HEIGHT_K1      = StaticRef<float>(0x8CCD6C);
    static auto& HEIGHT_K2      = StaticRef<float>(0x8CCD64);
    static auto& CAM_K          = StaticRef<float>(0x8CCD5C);
    static auto& DOT_LIMIT      = StaticRef<float>(0x8CCD60);
    static auto& MAX_BLOCKED    = StaticRef<int32>(0x8CCD74);
    static auto& DOT_VALUE      = StaticRef<float>(0x859EF8);
    static auto& SCALE          = StaticRef<float>(0x858B8C);
    static auto& RAND_SCALE     = StaticRef<float>(0x858C7C);
    static auto& ONE            = StaticRef<float>(0x858624);
    static auto& MATRIX_FWD_Z   = StaticRef<float>(0x858C20);
    static auto& PI_2           = StaticRef<float>(0x859070);
    static auto& PI             = StaticRef<float>(0x85A994);
    static auto& TIME_SCALE     = StaticRef<float>(0x8595EC);

    gbDWCineyCamProcessed = false;

    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        gbDWCineyCamProcessed = false;
        return;
    }

    CEntity*  entity{};
    CVehicle* vehicle{};
    CVector   dest{}, src{}, targetUp{}, targetRight{}, targetFwd{}, targetVel{}, targetAngVel{};
    float     targetSpeed{}, targetAngSpeed{};
    CColSphere colSphere;
    GetCoreDataForDWCineyCamMode(entity, vehicle, dest, src, targetUp, targetRight, targetFwd, targetVel, targetSpeed, targetAngVel, targetAngSpeed, colSphere);

    const auto now = CTimer::GetTimeInMS();

    const auto FilterLampposts = [](CEntity** ents, int16 count, CEntity** out, int32& outCount) {
        outCount = 0;
        for (int16 i = 0; i < count; ++i) {
            auto* ent = ents[i];
            if (!ent->GetIsStatic()) {
                continue;
            }
            if (!ent->m_matrix) {
                ent->AllocateMatrix();
                ent->UpdateRwMatrix();
            }
            if (MATRIX_FWD_Z >= ent->m_matrix->GetForward().z) {
                continue;
            }
            if (IsLampPost(static_cast<eModelID>(ent->GetModelIndex()))) {
                out[outCount++] = ent;
            }
        }
    };

    // world position of a point somewhere on the lamppost (random height)
    const auto GetLampPoint = [](CEntity* lamp) {
        const auto* colModel = lamp->GetColModel();
        auto worldTop = lamp->m_matrix->TransformPoint(colModel->m_boundBox.m_vecMax);
        const auto zMin = colModel->m_boundBox.m_vecMax.z - colModel->m_boundBox.m_vecMin.z * SCALE;
        worldTop.z -= (zMin - ONE) * (float)rand() * RAND_SCALE + ONE;
        return worldTop;
    };

    auto& gbExitCamThis = gbExitCam[0x16];

    if (DWCineyCamCurrentId != 0x3A || gLastFrameProcessedDWCineyCam < CTimer::GetFrameCounter() - 1u) {
        DWCineyCamCurrentId = 0x3A;
        DWCineyCamEndTime   = DURATION + (float)now;
        gbExitCamThis       = false;
        DWCineyCamStartTime = (float)now;
        s_BlockedCount      = MAX_BLOCKED;
        gHandShaker[0].Reset(); // 0x50D860

        // find lampposts in front of and behind the vehicle
        const CVector searchPos1 = dest + targetFwd * SEARCH_R1 * HEIGHT_K1;
        const CVector searchPos2 = dest - targetFwd * SEARCH_R2 * HEIGHT_K2;

        CEntity* ents1[127]{};
        int16    n1{};
        CWorld::FindObjectsInRange(searchPos1, SEARCH_R1, true, &n1, 127, ents1, false, false, false, true, true);

        CEntity* ents2[127]{};
        int16    n2{};
        CWorld::FindObjectsInRange(searchPos2, SEARCH_R2, true, &n2, 127, ents2, false, false, false, true, true);

        CEntity* lamps1[127]{};
        int32    nlamps1{};
        FilterLampposts(ents1, n1, lamps1, nlamps1);

        CEntity* lamps2[127]{};
        int32    nlamps2{};
        FilterLampposts(ents2, n2, lamps2, nlamps2);

        if (nlamps1 < 1) {
            gbExitCamThis = true;
            return;
        }

        bool found = false;
        for (int32 i = 0; i < nlamps1 && !found; ++i) {
            const auto p1 = GetLampPoint(lamps1[i]);
            auto dir = p1 - dest;
            dir.Normalise();
            const auto unitPoint = dir + dest;

            if (std::abs(DOT_VALUE) < DOT_LIMIT) {
                for (int32 j = 0; j < nlamps2 && !found; ++j) {
                    const auto p2 = GetLampPoint(lamps2[j]);
                    if (CWorld::GetIsLineOfSightClear(p1, p2, true, false, false, false, false, true, true)
                        && CWorld::GetIsLineOfSightClear(p2, unitPoint, true, false, false, false, false, true, true)
                    ) {
                        src = p1; // camera position
                        s_LampPos1 = p1;
                        s_LampPos2 = p2;
                        found = true;
                    }
                }
            }
        }
        if (!found) {
            gbExitCamThis = true;
            return;
        }
    }

    const auto t = (float)(now - (uint32)DWCineyCamStartTime) / (DWCineyCamEndTime - DWCineyCamStartTime);

    // camera flies from lamp1 (+offset) to lamp2 (-offset)
    auto dir = (s_LampPos2 - s_LampPos1).Normalized();
    const auto ofs = dir * CAM_K;

    CVector camPos    = s_LampPos1 + ofs * 2.0f;
    CVector camTarget = s_LampPos2 - ofs;

    if (!gbExitCamThis) {
        const auto k = (ONE + std::sin((PI_2 - t * PI) * TIME_SCALE)) * SCALE;
        camPos = Lerp(camTarget, camPos, k);
    }

    if (IsTimeToExitThisDWCineyCamMode(0x16, camPos, dest, t, false)) {
        gbExitCamThis = true;
        return;
    }

    CWorld::pIgnoreEntity = entity;
    CColPoint cp{};
    CEntity*  hitEnt{};
    const auto hit = CWorld::ProcessLineOfSight(dest, camPos, cp, hitEnt, true, true, false, false, false, false, false, false);
    CWorld::pIgnoreEntity = nullptr;

    if (!hit) {
        const auto wasZero = s_BlockedCount == 0;
        s_BlockedCount--;
        if (wasZero) {
            gbExitCamThis = true;
            CWorld::pIgnoreEntity = nullptr;
            return;
        }
    } else {
        const auto capped = MAX_BLOCKED < s_BlockedCount;
        s_BlockedCount++;
        if (capped) {
            s_BlockedCount = MAX_BLOCKED;
        }
    }

    Finalise_DW_CineyCams(camPos, dest, 0.0f, 70.0f /*0x428C0000*/, 0.3f /*0x3E99999A*/, 0.0f);
}

// 0x51B120
void CCam::Process_DW_CamManCam(bool) {
    static auto& s_InitFlags     = StaticRef<uint32>(0xB70078);
    static auto& s_BlockedCount  = StaticRef<int32>(0xB70074);
    static auto& s_SavedCamPos   = StaticRef<CVector>(0xB70068);
    static auto& DURATION        = StaticRef<float>(0x8CCBAC);  // duration of the cam 0x15 transition
    static auto& SEARCH_RADIUS   = StaticRef<float>(0x8CCD54);
    static auto& FWD_MULT        = StaticRef<float>(0x8CCD54);  // NOTE: same address as SEARCH_RADIUS in the binary
    static auto& MIN_DIST_2D     = StaticRef<float>(0x8CCD50);
    static auto& DOT_LIMIT       = StaticRef<float>(0x8CCD4C);
    static auto& DOT_VALUE       = StaticRef<float>(0x859EF8);
    static auto& PULL_MULT       = StaticRef<float>(0x8CCD48);
    static auto& DIST_DIV        = StaticRef<float>(0x8CCD44);
    static auto& FOV_FAR         = StaticRef<float>(0x8CCD40);
    static auto& FOV_NEAR        = StaticRef<float>(0x8CCD3C);
    static auto& T_LIMIT         = StaticRef<float>(0x8CCD38);
    static auto& FOV_BASE        = StaticRef<float>(0x8CCD34);
    static auto& MAX_BLOCKED     = StaticRef<int32>(0x8CCD58);
    static auto& VEL_MULT        = StaticRef<float>(0x8CCD30);
    static auto& SHAKE_MIN       = StaticRef<float>(0x8CCD2C);
    static auto& SCALE           = StaticRef<float>(0x858B8C);
    static auto& MIN_CLAMP       = StaticRef<float>(0x858B50);
    static auto& ONE             = StaticRef<float>(0x858624);
    static auto& SIN_BASE        = StaticRef<float>(0x859070);
    static auto& SIN_MUL         = StaticRef<float>(0x85A994);
    static auto& TIME_SCALE      = StaticRef<float>(0x8595EC);
    static auto& NEAR_BASE       = StaticRef<float>(0x85862C);
    static auto& NEAR_MUL1       = StaticRef<float>(0x85B328);
    static auto& NEAR_MUL2       = StaticRef<float>(0x863200);
    static auto& MATRIX_FWD_Z    = StaticRef<float>(0x858C20);

    gbDWCineyCamProcessed = false;

    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        gbDWCineyCamProcessed = false;
        return;
    }

    CEntity*  entity{};
    CVehicle* vehicle{};
    CVector   dest{}, src{}, targetUp{}, targetRight{}, targetFwd{}, targetVel{}, targetAngVel{};
    float     targetSpeed{}, targetAngSpeed{};
    CColSphere colSphere;
    GetCoreDataForDWCineyCamMode(entity, vehicle, dest, src, targetUp, targetRight, targetFwd, targetVel, targetSpeed, targetAngVel, targetAngSpeed, colSphere);

    const auto now = CTimer::GetTimeInMS();

    if (!(s_InitFlags & 1)) {
        s_InitFlags |= 1;
    }
    if (!(s_InitFlags & 2)) {
        s_InitFlags |= 2;
        s_BlockedCount = MAX_BLOCKED;
    }

    auto& gbExitCamThis = gbExitCam[0x15];

    if (DWCineyCamCurrentId != 0x39 || gLastFrameProcessedDWCineyCam < CTimer::GetFrameCounter() - 1u) {
        DWCineyCamCurrentId = 0x39;
        DWCineyCamEndTime   = DURATION + (float)now;
        s_BlockedCount      = MAX_BLOCKED;
        gbExitCamThis       = false;
        DWCineyCamStartTime = (float)now;
        gHandShaker[0].Reset(); // 0x50D860

        // Search for a lamppost in front of the vehicle
        const CVector searchPos = dest + targetFwd * FWD_MULT;

        CEntity* entities[15]{};
        int16    numEntities{};
        CWorld::FindObjectsInRange(searchPos, SEARCH_RADIUS, true, &numEntities, 15, entities, false, false, false, true, true);

        CEntity* chosenLamp{};
        float    bestDist2D = 10000.0f;

        for (int16 i = 0; i < numEntities; ++i) {
            auto* ent = entities[i];
            if (!ent->GetIsStatic()) {
                continue;
            }
            if (!ent->m_matrix) {
                ent->AllocateMatrix();
                ent->UpdateRwMatrix();
            }
            if (MATRIX_FWD_Z >= ent->m_matrix->GetForward().z) {
                continue;
            }
            if (!IsLampPost(static_cast<eModelID>(ent->GetModelIndex()))) {
                continue;
            }

            const CVector lampPos = ent->GetPosition();
            const auto dist2D = (CVector2D{ lampPos } - CVector2D{ dest }).Magnitude();
            if (dist2D >= bestDist2D || dist2D <= MIN_DIST_2D) {
                continue;
            }

            const auto* colModel    = ent->GetColModel();
            const CVector worldMax  = ent->m_matrix->TransformPoint(colModel->m_boundBox.m_vecMax); // top of the lamppost
            CVector camPosCandidate = worldMax;
            camPosCandidate.z = ent->GetPosition().z + colModel->m_boundBox.m_vecMin.z * SCALE;

            auto dir = camPosCandidate - dest;
            dir.Normalise();
            const auto point = dest + dir;

            if (std::abs(DOT_VALUE) < DOT_LIMIT
                && CWorld::GetIsLineOfSightClear(camPosCandidate, point, true, false, false, false, false, true, true)
            ) {
                chosenLamp    = ent;
                bestDist2D    = dist2D;
                s_SavedCamPos = camPosCandidate;
                src           = camPosCandidate;
            }
        }

        if (!chosenLamp) {
            gbExitCamThis = true;
            return;
        }
    }

    const auto t = (float)(now - (uint32)DWCineyCamStartTime) / (DWCineyCamEndTime - DWCineyCamStartTime);

    CVector camPos = src;
    if (!gbExitCamThis) {
        auto dir = dest - s_SavedCamPos;
        dir.Normalise();
        camPos = s_SavedCamPos + dir * PULL_MULT;
    }

    // FOV blend (same pattern as the spotter cam)
    auto fov = 70.0f;
    {
        auto k = (dest - camPos).Magnitude() / DIST_DIV;
        k = (MIN_CLAMP <= k) ? ((ONE < k) ? ONE : k) : MIN_CLAMP;
        const auto s = std::sin((SIN_BASE - k * SIN_MUL) * TIME_SCALE);
        fov = (FOV_FAR - FOV_NEAR) * (s + ONE) * SCALE + FOV_NEAR;

        if (t < T_LIMIT) {
            auto k2 = t / T_LIMIT;
            k2 = (MIN_CLAMP <= k2) ? ((ONE < k2) ? ONE : k2) : MIN_CLAMP;
            const auto s2 = std::sin((SIN_BASE - k2 * SIN_MUL) * TIME_SCALE);
            fov = (fov - FOV_BASE) * (s2 + ONE) * SCALE + FOV_BASE;
        }
    }

    if (IsTimeToExitThisDWCineyCamMode(0x15, camPos, dest, t, false)) {
        gbExitCamThis = true;
        return;
    }

    CWorld::pIgnoreEntity = entity;
    CColPoint cp{};
    CEntity*  hitEnt{};
    const auto hit = CWorld::ProcessLineOfSight(dest, camPos, cp, hitEnt, true, true, false, false, false, false, false, false);
    CWorld::pIgnoreEntity = nullptr;

    if (hit) {
        const auto wasZero = s_BlockedCount == 0;
        s_BlockedCount--;
        if (wasZero) {
            gbExitCamThis = true;
            CWorld::pIgnoreEntity = nullptr;
            return;
        }
    } else {
        const auto capped = MAX_BLOCKED < s_BlockedCount;
        s_BlockedCount++;
        if (capped) {
            s_BlockedCount = MAX_BLOCKED;
        }
    }

    // shake amount based on the vehicle speed
    const auto velMult = targetVel.Magnitude() * VEL_MULT;
    float shake{};
    if (velMult <= SHAKE_MIN) {
        shake = SHAKE_MIN;
    }
    if (MIN_CLAMP <= shake) {
        if (shake <= ONE) {
            // 0x420800 -- unresolved 2-arg float helper (min/max-like)
            shake = plugin::CallAndReturn<float, 0x420800>(velMult, SHAKE_MIN);
        } else {
            shake = 1.0f;
        }
    } else {
        shake = 0.0f;
    }

    Finalise_DW_CineyCams(camPos, dest, 0.0f, fov, NEAR_BASE - fov * NEAR_MUL1 * NEAR_MUL2, shake);
}

// 0x51A740
// NOTE: each DW ciney cam owns a 0x9C byte state slot. Slots are addressed
// BACKWARDS from 0xB6FEC0: state = 0xB6FEC0 - 0x9C * GetDWStateIdx() (0x821B40, unresolved).
struct DWHeliCamState { // size 0x9C
    CVector m_vecCamPosB;         // +0x00 - position in front of the heli (lerp target)
    CVector m_vecCamPosA;         // +0x0C - position behind the heli (lerp start)
    float   m_fFwdMultB;          // +0x18
    float   m_fFwdMultA;          // +0x1C
    float   m_fZAdjust;           // +0x20
    float   m_fRightMult;         // +0x24
    float   m_fFovBlendT;         // +0x28
    float   m_fFovNear;           // +0x2C
    float   m_fFovFar;            // +0x30
    float   m_fRollMult;          // +0x34
    float   m_fFovToRoll;         // +0x38
    float   m_fNearClip;          // +0x3C
    bool    m_bTargetFrozen;      // +0x40
    int32   m_nFreezeCountdown;   // +0x44
    int32   m_nFreezeDuration;    // +0x48
    CVector m_vecFrozenTarget;    // +0x4C
    int32   m_nAttempts;          // +0x58
    bool    m_bSavedCamPos;       // +0x5C
    int32   m_nBlockedMax;        // +0x60
    int32   m_nBlockedCount;      // +0x64
    float   m_fRollStartDist;     // +0x68
    float   m_fRollEndDist;       // +0x6C
    float   m_fRollMax;           // +0x70
    float   m_fMinCamDist;        // +0x74
    float   m_fTestRadius;        // +0x78
    float   m_fHeightOff;         // +0x7C
    bool    m_bBlocked;           // +0x80
    bool    m_bRandFlag1;         // +0x81
    float   m_fSavedRoll;         // +0x84
    bool    m_bRollBlend;         // +0x88
    uint32  m_nRollBlendStart;    // +0x8C
    uint32  m_nRollBlendEnd;      // +0x90
    float   m_fRollBlendT;        // +0x94
    int32   m_nRollBlendDur;      // +0x98
};

void CCam::Process_DW_HeliChaseCam(bool) {
    static auto& s_SavedCamPos      = StaticRef<CVector>(0xB70058);
    static auto& s_SavedInitFlags   = StaticRef<uint32>(0xB70064);
    static auto& s_UnstickCountdown = StaticRef<int32>(0x8CCD24);
    static auto& UNSTICK_LERP       = StaticRef<float>(0x8CCD20);
    static auto& UNSTICK_RADIUS     = StaticRef<float>(0x8CCD28);
    static auto& DURATION           = StaticRef<float>(0x8CCBA8);   // duration of the cam 0x14 transition
    static auto& SCALE              = StaticRef<float>(0x858B8C);
    static auto& CLAMP_K            = StaticRef<float>(0x858B50);
    static auto& HALF               = StaticRef<float>(0x858C1C);
    static auto& ONE                = StaticRef<float>(0x858624);
    static auto& SIN_BASE           = StaticRef<float>(0x859070);
    static auto& SIN_MUL            = StaticRef<float>(0x85A994);
    static auto& TIME_SCALE         = StaticRef<float>(0x8595EC);

    gbDWCineyCamProcessed = false;

    auto& state = *reinterpret_cast<DWHeliCamState*>(0xB6FEC0 - 0x9C * plugin::CallAndReturn<int32, 0x821B40>()); // 0x821B40 -- unresolved (rand() thunk elsewhere in the binary)

    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        return;
    }

    CEntity*  entity{};
    CVehicle* vehicle{};
    CVector   dest{}, src{}, targetUp{}, targetRight{}, targetFwd{}, targetVel{}, targetAngVel{};
    float     targetSpeed{}, targetAngSpeed{};
    CColSphere colSphere;
    GetCoreDataForDWCineyCamMode(entity, vehicle, dest, src, targetUp, targetRight, targetFwd, targetVel, targetSpeed, targetAngVel, targetAngSpeed, colSphere);

    const auto now = CTimer::GetTimeInMS();

    auto& gbExitCamThis = gbExitCam[0x14];

    if (DWCineyCamCurrentId != 0x38 || gLastFrameProcessedDWCineyCam < CTimer::GetFrameCounter() - 1u) {
        DWCineyCamEndTime   = DURATION + (float)now;
        gbExitCamThis       = false;
        DWCineyCamCurrentId = 0x38;
        DWCineyCamStartTime = (float)now;
        plugin::Call<0x50E180>(); // 0x50E180 -- unresolved (resets DW ciney cam statics)
        state.m_nFreezeCountdown = state.m_nFreezeDuration;
        state.m_fHeightOff       = state.m_fFovNear - state.m_fFovFar;
        state.m_bTargetFrozen    = false;
        state.m_bSavedCamPos     = false;
        state.m_bBlocked         = false;
        state.m_bRandFlag1       = false;
        state.m_nBlockedCount    = state.m_nBlockedMax;
        state.m_bRollBlend       = false;
        plugin::Call<0x50E090>(); // 0x50E090 -- unresolved
        gHandShaker[0].Reset();   // 0x50D860

        bool found = false;
        if (state.m_nAttempts > 0) {
            for (int32 attempt = 0; attempt < state.m_nAttempts; ++attempt) {
                const auto sign = rand() < 0x3FFF ? -1.0f : 1.0f;
                const auto k    = rand() < 0x3FFF ? HALF : ONE;

                // position behind the heli
                state.m_vecCamPosA = dest - targetFwd * state.m_fFwdMultA;
                state.m_vecCamPosB = dest + targetFwd * state.m_fFwdMultB;
                state.m_vecCamPosB.z += state.m_fZAdjust;
                state.m_vecCamPosA.z += state.m_fZAdjust;

                const auto rightOfs = targetRight * state.m_fRightMult;
                state.m_vecCamPosA += rightOfs * sign;
                state.m_vecCamPosA.z += rightOfs.z * (SCALE - 1.0f) * sign; // z-offset gets extra SCALE factor
                state.m_vecCamPosB += rightOfs * k;
                state.m_vecCamPosB.z += rightOfs.z * k * SCALE - rightOfs.z * k;

                if (!CWorld::TestSphereAgainstWorld(state.m_vecCamPosA, state.m_fTestRadius, nullptr, false, true, true, false, false, false)) {
                    CWorld::pIgnoreEntity = entity;
                    CColPoint cp{};
                    CEntity*  hitEnt{};
                    const auto hit = CWorld::ProcessLineOfSight(dest, state.m_vecCamPosA, cp, hitEnt, true, true, false, false, false, false, false, false);
                    CWorld::pIgnoreEntity = nullptr;
                    if (!hit) {
                        state.m_bRandFlag1 = rand() < 0x3FFF;
                        state.m_bRollBlend = rand() < 0x3FFF;
                        found = true;
                        break;
                    }
                }
            }
        }
        if (!found) {
            state.m_bTargetFrozen    = false;
            state.m_bSavedCamPos     = false;
            state.m_fHeightOff       = state.m_fFovNear - state.m_fFovFar;
            state.m_bBlocked         = false;
            state.m_bRandFlag1       = false;
            state.m_nBlockedCount    = state.m_nBlockedMax;
            state.m_nFreezeCountdown = state.m_nFreezeDuration;
            state.m_bRollBlend       = false;
            gbExitCamThis = true;
            return;
        }
    }

    if (gbExitCamThis) {
        return;
    }

    const auto t = (float)(now - (uint32)DWCineyCamStartTime) / (DWCineyCamEndTime - DWCineyCamStartTime);

    CVector camPos = Lerp(state.m_vecCamPosA, state.m_vecCamPosB, t); // original lerps each component with t
    const auto fovMult = state.m_fRollMult;

    CVector target = dest + targetFwd * (targetSpeed * fovMult) + targetFwd;

    // enforce a minimum 2D distance from the heli
    auto  toTarget   = CVector2D{ target } - CVector2D{ camPos };
    const auto dist2D = toTarget.Magnitude();
    const auto inv    = ONE / dist2D;
    toTarget *= inv;
    if (dist2D < state.m_fMinCamDist) {
        camPos.x = target.x - toTarget.x * state.m_fMinCamDist;
        camPos.y = target.y - toTarget.y * state.m_fMinCamDist;
    }

    // base fov (blend in over m_fFovBlendT)
    auto fov = state.m_fFovFar;
    if (t < state.m_fFovBlendT && !state.m_bRandFlag1) {
        const auto k = std::sin((SIN_BASE - (ONE / state.m_fFovBlendT) * t * SIN_MUL) * TIME_SCALE);
        fov = (state.m_fFovFar - state.m_fFovNear) * (k + ONE) * SCALE + state.m_fFovNear;
    }

    // roll induced by distance to the heli
    auto rollOfs = CLAMP_K;
    {
        const auto dist3D = (camPos - target).Magnitude();
        if (state.m_fRollStartDist < dist3D) {
            auto k = (dist3D - state.m_fRollStartDist) / (state.m_fRollEndDist - state.m_fRollStartDist);
            k = (CLAMP_K <= k) ? ((ONE < k) ? ONE : k) : CLAMP_K;
            const auto s = std::sin((SIN_BASE - k * SIN_MUL) * TIME_SCALE);
            rollOfs = (s + ONE) * SCALE * state.m_fRollMax;
        }
    }
    auto roll = fov - rollOfs; // NOTE: despite the name, this is blended back into the FOV below
    const auto rollParam = t * state.m_fFovToRoll; // actual roll argument of Finalise_DW_CineyCams

    // if camera is inside geometry / below ground, pull it out towards the saved position
    if (state.m_bSavedCamPos || CWorld::TestSphereAgainstWorld(camPos, UNSTICK_RADIUS, nullptr, false, true, true, false, false, false)) {
        if (!(s_SavedInitFlags & 1)) {
            s_SavedInitFlags |= 1;
        }
        if (!state.m_bSavedCamPos) {
            s_SavedCamPos = camPos;
            state.m_bSavedCamPos = true;
            s_UnstickCountdown = 100;
        }
        if (s_UnstickCountdown < 0) {
            s_UnstickCountdown--;
            gbExitCamThis = true;
            return;
        }
        camPos = s_SavedCamPos + (camPos - s_SavedCamPos) * UNSTICK_LERP;
        s_UnstickCountdown--;
    }

    // free the look-at target if the line of sight gets blocked for long enough
    if (!state.m_bTargetFrozen) {
        CWorld::pIgnoreEntity = entity;
        CColPoint cp{};
        CEntity*  hitEnt{};
        const auto hit = CWorld::ProcessLineOfSight(target, camPos, cp, hitEnt, true, true, false, false, false, false, false, false);
        CWorld::pIgnoreEntity = nullptr;
        if (hit) {
            state.m_bBlocked = true;
            if (!state.m_bRollBlend
                && state.m_nBlockedCount < (state.m_nBlockedMax + ((int32(state.m_nBlockedMax) >> 31) & 3)) / 4
            ) {
                state.m_fSavedRoll      = roll;
                state.m_bRollBlend      = true;
                state.m_nRollBlendStart = now;
                state.m_nRollBlendEnd   = state.m_nRollBlendDur + now;
            }
            const auto wasZero = state.m_nBlockedCount == 0;
            state.m_nBlockedCount--;
            if (wasZero) {
                state.m_vecFrozenTarget = target;
                state.m_bTargetFrozen = true;
            }
        } else {
            state.m_nBlockedCount++;
            if (state.m_nBlockedMax < state.m_nBlockedCount) {
                state.m_nBlockedCount = state.m_nBlockedMax;
            }
        }
    } else {
        target = state.m_vecFrozenTarget;
        const auto wasZero = state.m_nFreezeCountdown == 0;
        state.m_nFreezeCountdown--;
        if (wasZero) {
            gbExitCamThis = true;
            return;
        }
    }

    // blend the roll back to its base value once the heli is far away again
    if (!state.m_bRollBlend) {
        if (state.m_fRollBlendT <= t) {
            state.m_fSavedRoll      = roll;
            state.m_bRollBlend      = true;
            state.m_nRollBlendStart = now;
            state.m_nRollBlendEnd   = state.m_nRollBlendDur + now;
        }
    }
    if (state.m_bRollBlend) {
        auto k = (float)(now - state.m_nRollBlendStart) / (float)(state.m_nRollBlendEnd - state.m_nRollBlendStart);
        k = (CLAMP_K <= k) ? ((ONE < k) ? ONE : k) : CLAMP_K;
        const auto s = std::sin((SIN_BASE - k * SIN_MUL) * TIME_SCALE);
        roll = (state.m_fFovNear - state.m_fSavedRoll) * (s + ONE) * SCALE + state.m_fSavedRoll;
    }

    if (IsTimeToExitThisDWCineyCamMode(0x14, camPos, target, t, false)) {
        gbExitCamThis = true;
        return;
    }

    Finalise_DW_CineyCams(camPos, target, rollParam, roll, state.m_fNearClip, 1.0f /*0x3F800000*/);
}

// 0x51C760
bool CCam::Process_DW_PlaneCam1(bool) {
    static auto& s_Duration     = StaticRef<float>(0x8CCBC0);   // duration of the cam 0x1A transition
    static auto& MIN_HEIGHT     = StaticRef<float>(0x8CCDBC);
    static auto& MIN_GROUND_DIST = StaticRef<float>(0x8CCDB8);
    static auto& s_XSign        = StaticRef<float>(0x8CCDB4);   // random +-1
    static auto& FWD_MULT       = StaticRef<float>(0x8CCDB0);
    static auto& RIGHT_MULT     = StaticRef<float>(0x8CCDAC);
    static auto& UP_MULT        = StaticRef<float>(0x8CCDA8);
    static auto& WOBBLE_FREQ    = StaticRef<float>(0x8CCDA0);
    static auto& WOBBLE_AMP     = StaticRef<float>(0x8CCD9C);
    static auto& ARC_MULT       = StaticRef<float>(0x8CCDA4);
    static auto& MAX_BLOCKED    = StaticRef<int32>(0x8CCD98);
    static auto& s_BlockedCount = StaticRef<int32>(0xB700B8);
    static auto& s_BlockedInit  = StaticRef<bool>(0xB700BC);
    static auto& s_Sign         = StaticRef<float>(0xB700C0);   // random +-1 / +-0.5
    static auto& HALF           = StaticRef<float>(0x858C1C);
    static auto& SCALE          = StaticRef<float>(0x858B8C);
    static auto& TWO_PI         = StaticRef<float>(0x859E2C);
    static auto& TIME_SCALE     = StaticRef<float>(0x8595EC);

    gbDWCineyCamProcessed = false;

    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        return false;
    }

    CEntity*  entity{};
    CVehicle* vehicle{};
    CVector   dest{}, src{}, targetUp{}, targetRight{}, targetFwd{}, targetVel{}, targetAngVel{};
    float     targetSpeed{}, targetAngSpeed{};
    CColSphere colSphere;
    GetCoreDataForDWCineyCamMode(entity, vehicle, dest, src, targetUp, targetRight, targetFwd, targetVel, targetSpeed, targetAngVel, targetAngSpeed, colSphere);

    const auto now = CTimer::GetTimeInMS();

    if (dest.z < MIN_HEIGHT) {
        gbExitCam[0x1A] = true;
        return false;
    }

    if (DWCineyCamCurrentId == 0x3E && gLastFrameProcessedDWCineyCam >= CTimer::GetFrameCounter() - 1u) {
        // Same transition still going: bail out if we got too close to the ground
        if (TheCamera.GetRoughDistanceToGround() < MIN_GROUND_DIST) {
            gbExitCam[0x1A] = true;
            return false;
        }
    } else {
        DWCineyCamCurrentId = 0x3E;
        DWCineyCamEndTime   = s_Duration + (float)now;
        gbExitCam[0x1A]     = false;
        DWCineyCamStartTime = (float)now;
        CWorld::pIgnoreEntity = entity;
        {
            CColPoint cp{};
            CEntity*  hitEnt{};
            const auto hit = CWorld::ProcessLineOfSight(dest, src, cp, hitEnt, true, true, false, false, false, false, false, false);
            CWorld::pIgnoreEntity = nullptr;
            if (hit) {
                gbExitCam[0x1A] = true;
                return false;
            }
        }
        s_Sign = 1.0f;
        if (rand() < 0x3FFF) { // RAND_MAX / 2
            s_Sign *= HALF;
        }
        if (rand() < 0x3FFF) {
            s_XSign = -1.0f;
        }
    }

    const auto t = (float)(now - (uint32)DWCineyCamStartTime) / (DWCineyCamEndTime - DWCineyCamStartTime);

    const auto fwd   = targetFwd.Normalized();
    const auto right = (targetRight * s_XSign).Normalized();

    const auto wobble = std::sin(WOBBLE_FREQ * t * TWO_PI * TIME_SCALE);
    const auto arc    = ARC_MULT * (t - SCALE) * s_Sign;

    const CVector camSrc = dest
        + fwd * FWD_MULT
        + right * RIGHT_MULT
        + targetUp * UP_MULT
        + targetUp * (WOBBLE_AMP * wobble + arc);

    CWorld::pIgnoreEntity = entity;
    CColPoint cp{};
    CEntity*  hitEnt{};
    const auto hit = CWorld::ProcessLineOfSight(dest, camSrc, cp, hitEnt, true, true, false, false, false, false, false, false);
    CWorld::pIgnoreEntity = nullptr;

    if (!s_BlockedInit) {
        s_BlockedInit  = true;
        s_BlockedCount = MAX_BLOCKED;
    }
    if (!hit) {
        s_BlockedCount = std::min(s_BlockedCount + 1, MAX_BLOCKED);
    } else {
        s_BlockedCount--;
        if (s_BlockedCount == 0) {
            gbExitCam[0x1A] = true;
            return false;
        }
    }

    if (!IsTimeToExitThisDWCineyCamMode(0x1A, camSrc, dest, t, false)) {
        Finalise_DW_CineyCams(camSrc, dest, 0.0f, 70.0f, 5.0f, 1.0f);
        return true;
    }

    gbExitCam[0x1A] = true;
    return false;
}

// 0x51CC30
bool CCam::Process_DW_PlaneCam2(bool) {
    static auto& s_Duration     = StaticRef<float>(0x8CCBC4);   // duration of the cam 0x1B transition
    static auto& MIN_HEIGHT     = StaticRef<float>(0x8CCDDC);
    static auto& s_XSign        = StaticRef<float>(0x8CCDD8);   // random +-1
    static auto& FWD_MULT       = StaticRef<float>(0x8CCDD4);
    static auto& RIGHT_MULT     = StaticRef<float>(0x8CCDD0);
    static auto& UP_MULT        = StaticRef<float>(0x8CCDCC);
    static auto& WOBBLE_FREQ    = StaticRef<float>(0x8CCDC8);
    static auto& WOBBLE_AMP     = StaticRef<float>(0x8CCDC4);
    static auto& MAX_BLOCKED    = StaticRef<int32>(0x8CCDC0);
    static auto& s_BlockedCount = StaticRef<int32>(0xB700C4);
    static auto& s_BlockedInit  = StaticRef<bool>(0xB700C8);
    static auto& s_FwdSign      = StaticRef<float>(0xB700D0);   // random +-1
    static auto& s_SideSign     = StaticRef<float>(0xB700CC);   // random +-1
    static auto& HALF           = StaticRef<float>(0x858C1C);
    static auto& ONE            = StaticRef<float>(0x858624);
    static auto& TWO_PI         = StaticRef<float>(0x859E2C);
    static auto& TIME_SCALE     = StaticRef<float>(0x8595EC);

    gbDWCineyCamProcessed = false;

    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        return false;
    }

    CEntity*  entity{};
    CVehicle* vehicle{};
    CVector   dest{}, src{}, targetUp{}, targetRight{}, targetFwd{}, targetVel{}, targetAngVel{};
    float     targetSpeed{}, targetAngSpeed{};
    CColSphere colSphere;
    GetCoreDataForDWCineyCamMode(entity, vehicle, dest, src, targetUp, targetRight, targetFwd, targetVel, targetSpeed, targetAngVel, targetAngSpeed, colSphere);

    const auto now = CTimer::GetTimeInMS();

    if (dest.z < MIN_HEIGHT) {
        gbExitCam[0x1B] = true;
        return false;
    }

    if (DWCineyCamCurrentId != 0x3F || gLastFrameProcessedDWCineyCam < CTimer::GetFrameCounter() - 1u) {
        DWCineyCamCurrentId = 0x3F;
        DWCineyCamEndTime   = s_Duration + (float)now;
        gbExitCam[0x1B]     = false;
        DWCineyCamStartTime = (float)now;
        CWorld::pIgnoreEntity = entity;
        {
            CColPoint cp{};
            CEntity*  hitEnt{};
            const auto hit = CWorld::ProcessLineOfSight(dest, src, cp, hitEnt, true, true, false, false, false, false, false, false);
            CWorld::pIgnoreEntity = nullptr;
            if (hit) {
                gbExitCam[0x1B] = true;
                return false;
            }
        }
        s_FwdSign = 1.0f;
        if (rand() < 0x3FFF) { // RAND_MAX / 2
            s_FwdSign *= HALF;
        }
        s_SideSign = 1.0f;
        if (rand() < 0x3FFF) {
            s_SideSign *= HALF;
        }
        if (rand() < 0x3FFF) {
            s_XSign = -1.0f;
        }
    }

    const auto t = (float)(now - (uint32)DWCineyCamStartTime) / (DWCineyCamEndTime - DWCineyCamStartTime);

    const auto fwd   = targetFwd.Normalized();
    const auto right = targetRight.Normalized();

    const auto fwdK   = (ONE - (t + t)) * s_SideSign;
    const auto rightK = (ONE - t) * s_XSign;

    const CVector camPos = dest + fwd * fwdK * FWD_MULT + right * rightK * RIGHT_MULT;

    const auto wobble = std::sin(WOBBLE_FREQ * t * TWO_PI * TIME_SCALE);
    const CVector camSrc = camPos + targetUp * (UP_MULT + WOBBLE_AMP * wobble);

    CWorld::pIgnoreEntity = entity;
    CColPoint cp{};
    CEntity*  hitEnt{};
    const auto hit = CWorld::ProcessLineOfSight(dest, camSrc, cp, hitEnt, true, true, false, false, false, false, false, false);
    CWorld::pIgnoreEntity = nullptr;

    if (!s_BlockedInit) {
        s_BlockedInit  = true;
        s_BlockedCount = MAX_BLOCKED;
    }
    if (!hit) {
        s_BlockedCount = std::min(s_BlockedCount + 1, MAX_BLOCKED);
    } else {
        s_BlockedCount--;
        if (s_BlockedCount == 0) {
            gbExitCam[0x1B] = true;
            return false;
        }
    }

    if (!IsTimeToExitThisDWCineyCamMode(0x1B, camSrc, dest, t, false)) {
        Finalise_DW_CineyCams(camSrc, dest, 0.0f, 70.0f, 5.0f, 1.0f);
        return true;
    }

    gbExitCam[0x1B] = true;
    return false;
}

// 0x51D100
bool CCam::Process_DW_PlaneCam3(bool) {
    static auto& s_Duration     = StaticRef<float>(0x8CCBC8);   // duration of the cam 0x1C transition
    static auto& MIN_HEIGHT     = StaticRef<float>(0x8CCDEC);
    static auto& UP_MULT        = StaticRef<float>(0x8CCDE4);
    static auto& MAX_BLOCKED    = StaticRef<int32>(0x8CCDE0);
    static auto& s_DistMult     = StaticRef<float>(0x8CCDE8);   // distance behind the plane
    static auto& s_BlockedCount = StaticRef<int32>(0xB700D4);
    static auto& s_BlockedInit  = StaticRef<bool>(0xB700D8);
    static auto& SCALE          = StaticRef<float>(0x858B8C);

    gbDWCineyCamProcessed = false;

    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        return false;
    }

    CEntity*  entity{};
    CVehicle* vehicle{};
    CVector   dest{}, src{}, targetUp{}, targetRight{}, targetFwd{}, targetVel{}, targetAngVel{};
    float     targetSpeed{}, targetAngSpeed{};
    CColSphere colSphere;
    GetCoreDataForDWCineyCamMode(entity, vehicle, dest, src, targetUp, targetRight, targetFwd, targetVel, targetSpeed, targetAngVel, targetAngSpeed, colSphere);

    const auto now = CTimer::GetTimeInMS();

    if (dest.z < MIN_HEIGHT) {
        gbExitCam[0x1C] = true;
        return false;
    }

    if (DWCineyCamCurrentId != 0x40 || gLastFrameProcessedDWCineyCam < CTimer::GetFrameCounter() - 1u) {
        DWCineyCamCurrentId = 0x40;
        DWCineyCamEndTime   = s_Duration + (float)now;
        gbExitCam[0x1C]     = false;
        DWCineyCamStartTime = (float)now;
        CWorld::pIgnoreEntity = entity;
        CColPoint cp{};
        CEntity*  hitEnt{};
        if (CWorld::ProcessLineOfSight(dest, src, cp, hitEnt, true, true, false, false, false, false, false, false)) {
            gbExitCam[0x1C] = true;
            CWorld::pIgnoreEntity = nullptr;
            return false;
        }
    }

    CWorld::pIgnoreEntity = entity;

    // (bbox max.y - bbox min.y) of the plane model * scale * 2 -> distance behind the plane
    const auto* colModel = entity->GetModelInfo()->GetColModel();
    s_DistMult  = (colModel->m_boundBox.m_vecMax.y - colModel->m_boundBox.m_vecMin.y) * SCALE;
    s_DistMult += s_DistMult;

    const CVector camTarget = dest + targetFwd * s_DistMult;
    const CVector camPos    = camTarget + targetUp * UP_MULT;
    const auto    t         = (float)(now - (uint32)DWCineyCamStartTime) / (DWCineyCamEndTime - DWCineyCamStartTime);

    CColPoint cp{};
    CEntity*  hitEnt{};
    const auto hit = CWorld::ProcessLineOfSight(dest, camPos, cp, hitEnt, true, true, false, false, false, false, false, false);
    CWorld::pIgnoreEntity = nullptr;

    if (!s_BlockedInit) {
        s_BlockedInit  = true;
        s_BlockedCount = MAX_BLOCKED;
    }
    if (!hit) {
        s_BlockedCount = std::min(s_BlockedCount + 1, MAX_BLOCKED);
    } else {
        s_BlockedCount--;
        if (s_BlockedCount == 0) {
            gbExitCam[0x1C] = true;
            return false;
        }
    }

    if (!IsTimeToExitThisDWCineyCamMode(0x1C, camPos, dest, t, false)) {
        Finalise_DW_CineyCams(camPos, dest, 0.0f, 70.0f, 5.0f, 1.0f);
        return true;
    }

    gbExitCam[0x1C] = true;
    return false;
}

// 0x51C250
bool CCam::Process_DW_PlaneSpotterCam(bool) {
    static auto& s_Duration     = StaticRef<float>(0x8CCBB4);   // duration of the cam 0x17 transition
    static auto& s_InitFlags    = StaticRef<uint32>(0xB700B4);  // bits: 1=init 2=fov-init 4=blocked-init
    static auto& s_SpotPoint    = StaticRef<CVector>(0xB700A8);
    static auto& s_RandomSpotter = StaticRef<bool>(0xB700A4);
    static auto& s_BaseFov      = StaticRef<float>(0xB700A0);
    static auto& s_BlockedCount = StaticRef<int32>(0xB7009C);
    static auto& NUM_TRIES      = StaticRef<int32>(0x8CCD94);
    static auto& SPOT_RADIUS    = StaticRef<float>(0x8CCD8C);
    static auto& MAX_BLOCKED    = StaticRef<int32>(0x8CCD78);
    static auto& DIST_DIV       = StaticRef<float>(0x8CCD88);
    static auto& FOV_FAR        = StaticRef<float>(0x8CCD84);
    static auto& FOV_NEAR       = StaticRef<float>(0x8CCD80);
    static auto& T_LIMIT        = StaticRef<float>(0x8CCD7C);
    static auto& SCALE          = StaticRef<float>(0x858B8C);
    static auto& SPOT_SCALE     = StaticRef<float>(0x858C7C);
    static auto& SPOT_ZOFF      = StaticRef<float>(0x858CA0);
    static auto& MIN_DIST_2D    = StaticRef<float>(0x858C80);
    static auto& CLAMP_MIN      = StaticRef<float>(0x858B50);
    static auto& ONE            = StaticRef<float>(0x858624);
    static auto& SIN_BASE       = StaticRef<float>(0x859070);
    static auto& SIN_MUL        = StaticRef<float>(0x85A994);
    static auto& TIME_SCALE     = StaticRef<float>(0x8595EC);
    static auto& NEAR_BASE      = StaticRef<float>(0x85862C);
    static auto& NEAR_MUL1      = StaticRef<float>(0x85B328);
    static auto& NEAR_MUL2      = StaticRef<float>(0x863200);

    gbDWCineyCamProcessed = false;

    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        return false;
    }

    CEntity*  entity{};
    CVehicle* vehicle{};
    CVector   dest{}, src{}, targetUp{}, targetRight{}, targetFwd{}, targetVel{}, targetAngVel{};
    float     targetSpeed{}, targetAngSpeed{};
    CColSphere colSphere;
    GetCoreDataForDWCineyCamMode(entity, vehicle, dest, src, targetUp, targetRight, targetFwd, targetVel, targetSpeed, targetAngVel, targetAngSpeed, colSphere);

    const auto now = CTimer::GetTimeInMS();

    if (!(s_InitFlags & 1)) {
        s_InitFlags |= 1;
    }

    if (DWCineyCamCurrentId != 0x3B || gLastFrameProcessedDWCineyCam < CTimer::GetFrameCounter() - 1u) {
        DWCineyCamCurrentId = 0x3B;
        DWCineyCamEndTime   = s_Duration + (float)now;
        gbExitCam[0x17]     = false;
        DWCineyCamStartTime = (float)now;

        // Try to find a spot for the camera (slightly offset from the old source position)
        bool found = false;
        for (int32 i = 0; i < NUM_TRIES; ++i) {
            const auto r = SPOT_RADIUS;
            CVector spot = src;
            spot.x += (r - r * SCALE) * (float)rand() * SPOT_SCALE + r * SCALE;
            spot.y += (r - r * SCALE) * (float)rand() * SPOT_SCALE + r * SCALE;

            CWorld::pIgnoreEntity = entity;
            CColPoint cp{};
            CEntity*  hitEnt{};
            const auto hit = CWorld::ProcessLineOfSight(dest, spot, cp, hitEnt, true, true, false, false, false, false, false, false);
            CWorld::pIgnoreEntity = nullptr;
            if (hit) {
                s_SpotPoint   = cp.m_vecPoint;
                s_SpotPoint.z += SPOT_ZOFF;
                found = true;
                break;
            }
        }
        if (!found) {
            gbExitCam[0x17] = true;
            return false;
        }

        s_RandomSpotter = rand() < 0x3FFF; // RAND_MAX / 2
    }

    const auto t = (float)(now - (uint32)DWCineyCamStartTime) / (DWCineyCamEndTime - DWCineyCamStartTime);

    float fov = 70.0f;
    if ((CVector2D{ s_SpotPoint } - CVector2D{ dest }).Magnitude() >= MIN_DIST_2D) {
        if (s_RandomSpotter) {
            auto k = (dest - s_SpotPoint).Magnitude() / DIST_DIV;
            k = (CLAMP_MIN <= k) ? ((ONE < k) ? ONE : k) : CLAMP_MIN;

            const auto s = std::sin((SIN_BASE - k * SIN_MUL) * TIME_SCALE);
            fov = (FOV_FAR - FOV_NEAR) * (s + ONE) * SCALE + FOV_NEAR;

            if (t < T_LIMIT) {
                if (!(s_InitFlags & 2)) {
                    s_InitFlags |= 2;
                    s_BaseFov = FOV_NEAR;
                }
                auto k2 = t / T_LIMIT;
                k2 = (CLAMP_MIN <= k2) ? ((ONE < k2) ? ONE : k2) : CLAMP_MIN;

                const auto s2 = std::sin((SIN_BASE - k2 * SIN_MUL) * TIME_SCALE);
                fov = (fov - s_BaseFov) * (s2 + ONE) * SCALE + s_BaseFov;
            }
        }

        CWorld::pIgnoreEntity = entity;
        CColPoint cp{};
        CEntity*  hitEnt{};
        const auto hit = CWorld::ProcessLineOfSight(dest, s_SpotPoint, cp, hitEnt, true, true, false, false, false, false, false, false);
        CWorld::pIgnoreEntity = nullptr;

        if (!(s_InitFlags & 4)) {
            s_InitFlags |= 4;
            s_BlockedCount = MAX_BLOCKED;
        }
        if (!hit) {
            s_BlockedCount = std::min(s_BlockedCount + 1, MAX_BLOCKED);
        } else {
            s_BlockedCount--;
            if (s_BlockedCount == 0) {
                gbExitCam[0x17] = true;
                return false;
            }
        }

        if (!IsTimeToExitThisDWCineyCamMode(0x17, s_SpotPoint, dest, t, false)) {
            Finalise_DW_CineyCams(s_SpotPoint, dest, 0.0f, fov, NEAR_BASE - fov * NEAR_MUL1 * NEAR_MUL2, 1.0f);
            return true;
        }
    }

    gbExitCam[0x17] = true;
    return false;
}

// 0x50F3F0 - debug
void CCam::Process_Editor(const CVector& target, float orientation, float speedVar, float speedVarWanted) {
    static auto& s_LookAtAngle     = StaticRef<float>(0xB6FFE4);
    static auto& s_DoRenderShadows = StaticRef<bool>(0xB7295A);

    if (m_bResetStatics) {
        m_vecSource.Set(796.0f, -937.0f, 40.0f);
        CEntity::SafeCleanUpRef(m_pCamTargetEntity);
        m_pCamTargetEntity = nullptr;
        m_bResetStatics    = false;
    }
    RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.3f);
    m_fFOV = 70.0f;

    static constexpr float _90DEG_PER_HOUR_IN_RAD_PER_MIN = 0.02617994f;
    const auto* pad = CPad::GetPad(1);
    m_fHorizontalAngle += pad->GetLeftStickX() * _90DEG_PER_HOUR_IN_RAD_PER_MIN / 19.0f;
    m_fVerticalAngle   += DegreesToRadians(static_cast<float>(pad->GetLeftStickY())) / 50.0f;

    m_fVerticalAngle = std::max(m_fVerticalAngle, DegreesToRadians(85.0f));
    if (m_fVerticalAngle >= DegreesToRadians(-85.0f)) {
        if (pad->IsSquareDown()) {
            s_LookAtAngle += 0.1f;
        } else if (pad->IsCrossDown()) {
            s_LookAtAngle -= 0.1f;
        } else {
            s_LookAtAngle = 0.0f;
        }
    } else {
        m_fVerticalAngle = DegreesToRadians(-85.0f);
    }
    s_LookAtAngle = std::clamp(s_LookAtAngle, -70.0f, 70.0f);

    m_vecFront = (m_pCamTargetEntity ? m_pCamTargetEntity->GetPosition() : m_vecSource - m_vecSource).Normalized();
    m_vecSource += s_LookAtAngle * m_vecFront;
    m_vecSource.z = std::min(m_vecSource.z, -450.0f);

    if (pad->IsRightShoulder2Pressed()) {
        FindPlayerEntity()->Teleport(m_vecSource, false);
    }

    const auto ClampByLoop = [](float& value, float min, float max) {
        while (value > max) {
            value -= 1.0f;
        }
        while (value < min) {
            value += 1.0f;
        }
    };
    ClampByLoop(m_vecSource.x, 5.0f, 115.0f);
    ClampByLoop(m_vecSource.y, 5.0f, 115.0f);

    GetVectorsReadyForRW();

    if (!pad->IsLeftShockPressed() && s_DoRenderShadows) {
        CShadows::StoreShadowToBeRendered(
            eShadowType::SHADOW_ADDITIVE,
            gpShadowExplosionTex,
            m_vecSource,
            {12.0f, 0.0f},
            {0.0f, -12.0f},
            128,
            128,
            128,
            128,
            1000.0f,
            false,
            1.0f,
            nullptr,
            false
        );
    }

    if (CHud::m_Wants_To_Draw_Hud) {
        NOTSA_LOG_DEBUG("CamX: {:0.3f} CamY: {:0.3f}  CamZ:  {:0.3f}", m_vecSource.x, m_vecSource.y, m_vecSource.z);
        NOTSA_LOG_DEBUG("Frontx: {:0.3f}, Fronty: {:0.3f}, Frontz: {:0.3f} ", m_vecFront.x, m_vecFront.y, m_vecFront.z);
        NOTSA_LOG_DEBUG("LookAT: {:0.3f}, LookAT: {:0.3f}, LookAT: {:0.3f} ", m_vecSource.x + m_vecFront.x, m_vecSource.y + m_vecFront.y, m_vecSource.z + m_vecFront.z);
    }
}

// 0x51D470
void CCam::Process_Fixed(const CVector& target, float orientation, float speedVar, float speedVarWanted) {
    if (m_nDirectionWasLooking != 3) {
        m_nDirectionWasLooking = 3; // todo: enum
    }

    m_vecSource = m_vecCamFixedModeSource;
    m_vecFront = (target - m_vecSource).Normalized();
    m_vecTargetCoorsForFudgeInter = target;
    GetVectorsReadyForRW();

    // inlined?
    const auto a = CrossProduct(
        m_vecFront,
        (m_vecCamFixedModeUpOffSet + CVector{ 0.0f, 0.0f, 1.0f }).Normalized()
    ).Normalized();
    m_vecUp = CrossProduct(
        a,
        m_vecFront
    );
    m_fFOV = 70.0f;

    if (float wl{}; CWaterLevel::GetWaterLevel(m_vecSource, wl, true) && m_vecSource.z < wl) {
        ApplyUnderwaterMotionBlur();
    }

    if (gAllowScriptedFixedCameraCollision) {
        const auto savedIgnoreEntity = CWorld::pIgnoreEntity;

        CWorld::pIgnoreEntity = FindPlayerVehicle();
        CVector out{};
        float   outDist{1.0f};
        if (TheCamera.ConeCastCollisionResolve(m_vecSource, target, out, 2.0f, 0.1f, outDist)) {
            m_vecSource.y = out.y;
            m_vecSource.z = out.z;
        }

        CWorld::pIgnoreEntity = savedIgnoreEntity;
    }
}

// 0x5B25F0
void CCam::Process_FlyBy(const CVector&, float, float, float) {
    static auto& gbFlyByCamPresent = StaticRef<bool>(0xB6F045);
    static auto& gbFlyByFinished   = StaticRef<bool>(0xB6F05B);
    static auto& s_FinishProgress  = StaticRef<float>(0xB6F168);
    // spline data (set up by CCamera::Process)
    static auto& s_PosSpline1 = StaticRef<void*>(0xB6F990);
    static auto& s_PosSpline2 = StaticRef<void*>(0xB6F994);
    static auto& s_RollSpline = StaticRef<void*>(0xB6F98C);
    static auto& s_FovSpline  = StaticRef<void*>(0xB6F988);
    static auto& s_Fov        = StaticRef<float>(0xBC4074);
    static auto& s_Roll       = StaticRef<float>(0xBC4078);
    static auto& s_Idx1       = StaticRef<int32>(0xBC4080);
    static auto& s_Idx2       = StaticRef<int32>(0xBC407C);
    static auto& s_Idx3       = StaticRef<int32>(0xBC4088);
    static auto& s_Idx4       = StaticRef<int32>(0xBC4084);
    static auto& TWO_POW_32   = StaticRef<float>(0x858C54);
    static auto& KNOT_SCALE   = StaticRef<float>(0x858C4C);
    static auto& TIME_SCALE   = StaticRef<float>(0x8595EC);
    static auto& ROLL_BASE    = StaticRef<float>(0x858FE4);
    static auto& TIME_STEP_K  = StaticRef<float>(0x858B38);

    if (!gbFlyByCamPresent) {
        return;
    }

    m_vecUp = CVector{ 0.0f, 0.0f, 1.0f };

    if (!gbFlyByFinished) {
        m_fTimeElapsedFloat = 0.0f;
        m_nFinishTime       = plugin::CallAndReturn<uint32, 0x821B40>(); // 0x821B40 -- unresolved (rand() thunk elsewhere in the binary)
        s_Idx3 = 5;
        s_Idx4 = 5;
        gbFlyByFinished = true;
        s_Idx1 = 0xB;
        s_Idx2 = 0xB;
        s_Fov  = *reinterpret_cast<float*>(0xB6F988 + 8);
        s_Roll = s_Fov;
    } else {
        m_fTimeElapsedFloat += CTimer::GetTimeStepNonClipped() * TIME_STEP_K * KNOT_SCALE;
    }

    const auto now    = (float)plugin::CallAndReturn<int32, 0x821B40>();
    const auto nowAdj = now < 0.0f ? now + TWO_POW_32 : now;
    const auto finish = (float)(int32)m_nFinishTime + ((int32)m_nFinishTime < 0 ? TWO_POW_32 : 0.0f);

    // 0x5B2090: evaluate a vector spline, 0x5B2330: evaluate a scalar spline
    if (finish <= nowAdj) {
        s_Idx1 = plugin::CallAndReturn<int32, 0x821B40>() - 1;
        s_Idx1 = s_Idx1 * 10 + 1;
        s_Idx2 = plugin::CallAndReturn<int32, 0x821B40>() - 1;
        s_Idx2 = s_Idx2 * 10 + 1;
        s_Idx3 = plugin::CallAndReturn<int32, 0x821B40>() * 4 - 3;
        s_Idx4 = plugin::CallAndReturn<int32, 0x821B40>() * 4 - 3;
        plugin::Call<0x5B2090, CVector*, void*, float, int32*>(&m_vecSource, s_PosSpline1, nowAdj, &s_Idx1);
        plugin::Call<0x5B2090, CVector*, void*, float, int32*>(&m_vecFront, s_PosSpline2, nowAdj, &s_Idx2);
        float roll{};
        plugin::Call<0x5B2330, float*, void*, float, int32*>(&roll, s_RollSpline, nowAdj, &s_Idx3);
        const auto ang = roll * TIME_SCALE + ROLL_BASE;
        m_vecUp.x = std::cos(ang);
        m_vecUp.z = std::sin(ang);
        plugin::Call<0x5B2330, float*, void*, float, int32*>(&s_Fov, s_FovSpline, nowAdj, &s_Idx4);
        s_FinishProgress = 1.0f;
        s_Idx1 = 0;
        s_Idx2 = 0;
        s_Idx3 = 0;
        s_Idx4 = 0;
    } else {
        s_FinishProgress = nowAdj / finish;

        // advance to the right spline segment
        {
            auto idx = s_Idx1;
            auto t   = (*reinterpret_cast<float*>(0xB6F990 + idx * 4) - *reinterpret_cast<float*>(0xB6F990 + 4)) * KNOT_SCALE;
            while (t < nowAdj) {
                idx += 10;
                t = (*reinterpret_cast<float*>(0xB6F990 + idx * 4) - *reinterpret_cast<float*>(0xB6F990 + 4)) * KNOT_SCALE;
                s_Idx1 = idx;
            }
        }
        plugin::Call<0x5B2090, CVector*, void*, float, int32*>(&m_vecSource, s_PosSpline1, nowAdj, &s_Idx1);

        {
            auto t = (*reinterpret_cast<float*>(0xB6F994 + s_Idx2 * 4) - *reinterpret_cast<float*>(0xB6F994 + 4)) * KNOT_SCALE;
            while (t < nowAdj) {
                s_Idx2 += 10;
                t = (*reinterpret_cast<float*>(0xB6F994 + s_Idx2 * 4) - *reinterpret_cast<float*>(0xB6F994 + 4)) * KNOT_SCALE;
            }
        }
        plugin::Call<0x5B2090, CVector*, void*, float, int32*>(&m_vecFront, s_PosSpline2, nowAdj, &s_Idx2);

        {
            auto t = (*reinterpret_cast<float*>(0xB6F98C + s_Idx3 * 4) - *reinterpret_cast<float*>(0xB6F98C + 4)) * KNOT_SCALE;
            while (t < nowAdj) {
                s_Idx3 += 4;
                t = (*reinterpret_cast<float*>(0xB6F98C + s_Idx3 * 4) - *reinterpret_cast<float*>(0xB6F98C + 4)) * KNOT_SCALE;
            }
        }
        float roll{};
        plugin::Call<0x5B2330, float*, void*, float, int32*>(&roll, s_RollSpline, nowAdj, &s_Idx3);
        const auto ang = roll * TIME_SCALE + ROLL_BASE;
        m_vecUp.x = std::cos(ang);
        m_vecUp.z = std::sin(ang);

        {
            auto t = (*reinterpret_cast<float*>(0xB6F988 + s_Idx4 * 4) - *reinterpret_cast<float*>(0xB6F988 + 4)) * KNOT_SCALE;
            while (t < nowAdj) {
                s_Idx4 += 4;
                t = (*reinterpret_cast<float*>(0xB6F988 + s_Idx4 * 4) - *reinterpret_cast<float*>(0xB6F988 + 4)) * KNOT_SCALE;
            }
        }
        plugin::Call<0x5B2330, float*, void*, float, int32*>(&s_Fov, s_FovSpline, nowAdj, &s_Idx4);
    }

    m_vecTargetCoorsForFudgeInter = m_vecFront;
    m_vecFront = m_vecFront - m_vecSource;
    m_vecFront.Normalise();

    const auto right = CrossProduct(m_vecUp, m_vecFront);
    m_vecUp = CrossProduct(m_vecFront, right);
    m_vecUp.Normalise();

    m_fFOV = s_Fov;
}

// 0x5245B0
// STILL DISABLED: model-specific camera tweak tables, the bike wheelie-cam blend and the
// mouse/stick beta handling are mangled in the decompilation.
void CCam::Process_FollowCar_SA(const CVector&, float, float, float, bool) {
    NOTSA_UNREACHABLE();
}

// 0x50F970
void CCam::Process_FollowPedWithMouse(const CVector& target, float orientation, float /*speedVar*/, float /*speedVarWanted*/) {
    static const auto& MOUSE_ABS_X      = StaticRef<float>(0xB73424); // CMouse: absolute mouse X position
    static const auto& MOUSE_ABS_Y      = StaticRef<float>(0xB73428); // CMouse: absolute mouse Y position
    static const auto& ALPHA_RECENTER_Z = StaticRef<float>(0x8CC7D0); // 0.8
    static const auto& ALPHA_RECENTER_A = StaticRef<float>(0x8CC7D8); // -0.22
    static const auto& ALPHA_STEP       = StaticRef<float>(0x858C28); // 0.05
    static const auto& ALPHA_STEP_NEG   = StaticRef<float>(0x85901C); // -0.05
    static const auto& ALPHA_MAX        = StaticRef<float>(0x8630F4); // -1.56207
    static constexpr float ALPHA_MAX_INL    = -1.56207f; // 0xBFC7F1E7 (inlined copy)
    static constexpr float ALPHA_PI_4       = 0.7853981f; // 0x3F490FDB (pi/4, inlined)
    static const auto& PI_4             = StaticRef<float>(0x859AB0);   // pi/4
    static const auto& DIST_MULT_SCALE  = StaticRef<float>(0x8CC7C8); // 3
    static const auto& DIST_MULT_MAX    = StaticRef<float>(0x858FE4); // pi/2
    static const auto& DIST_MULT_MUL    = StaticRef<float>(0x8CC7C4); // 2
    static const auto& DIST_MULT_BASE   = StaticRef<float>(0x8CC7C0); // 1.7
    static const auto& FOV_MULT         = StaticRef<float>(0x85A5C4); // 0.0125
    static const auto& BETA_MULT        = StaticRef<float>(0x859B50); // 1/14
    static const auto& ALPHA_MULT       = StaticRef<float>(0x8631AC); // 0.0428571
    static const auto& BETA_ABS_MULT    = StaticRef<float>(0x8631B0); // -2.5
    static const auto& ALPHA_ABS_MULT   = StaticRef<float>(0x858B90); // 4
    static const auto& INPUT_MULT       = StaticRef<float>(0x8CC7CC); // 0.01
    static const auto& TWO_PI           = StaticRef<float>(0x858CB8); // 2*pi
    static const auto& MIN_DIST         = StaticRef<float>(0x858EE8); // 0.4
    static const auto& NEARCLIP_SUB     = StaticRef<float>(0x858C24); // 0.3
    static const auto& NEARCLIP_FLOOR   = StaticRef<float>(0x858C28); // 0.05
    static const auto& NEARCLIP_SUB2    = StaticRef<float>(0x858F9C); // 0.35
    static const auto& NEARCLIP_MAX     = StaticRef<float>(0x858C20); // 0.9
    static constexpr float NEARCLIP_MAX_INL = 0.9f; // 0x3F666666 (inlined)
    static const auto& HIT_KEEP_DIST    = StaticRef<float>(0x858CC8); // 0.6
    static const auto& MIN_CLIP         = StaticRef<float>(0x858B1C); // 0.1
    static constexpr float NEARCLIP_FLOOR2  = 0.1f; // 0x3DCCCCCD (inlined)
    static const auto& ASPECT_MULT      = StaticRef<float>(0x862F10); // 1.1
    static const auto& FOV_TO_RAD       = StaticRef<float>(0x8595EC); // pi/180
    static const auto& HALF             = StaticRef<float>(0x858B8C); // 0.5
    static const auto& TAN_HALF_FOV_SUB = StaticRef<float>(0x8CC38C); // 0.5
    static const auto& LERP_CLAMP       = StaticRef<float>(0x858624); // 1
    static const auto& Z_OFF            = StaticRef<float>(0x858CAC); // -0.9

    auto& gaTempSphereColPoints = StaticRef<std::array<CColPoint, 32>>(0xB9B250); // see CWorld

    // Original calls CRT pow(0x862F14, CTimer::ms_fTimeStep) -- 0x862F14 = 0.92.
    // (0x822130 is the msvc pow dispatcher; it takes the base and exponent on the FPU stack.)
    const auto GetDampingFactor = [] {
        return std::pow(StaticRef<float>(0x862F14), CTimer::ms_fTimeStep); // 0.92
    };
    // CPad::GetMouseX/GetMouseY (unreversed): raw mouse deltas
    const auto GetMouseX = [] { return plugin::CallAndReturn<int16, 0x540E80>(); };
    const auto GetMouseY = [] { return plugin::CallAndReturn<int16, 0x540F80>(); };
    const auto SetNearClip = [](float v) { RwCameraSetNearClipPlane(Scene.m_pRwCamera, v); };
    const auto GetRwCameraNearClip = [] {
        return *reinterpret_cast<const float*>(reinterpret_cast<const uint8*>(Scene.m_pRwCamera) + 0x80); // RwCamera::nearClip
    };

    m_fFOV = 70.0f;

    if (!m_pCamTargetEntity->GetIsTypePed()) {
        return;
    }

    if (m_bResetStatics) {
        m_bRotating          = false;
        m_bCollisionChecksOn = true;
        CPad::ClearMouseHistory();
        m_bResetStatics      = false;
    }

    // If the player is driving a train, the camera always looks along his orientation
    const auto bDrivingTrain = [&] {
        if (const auto veh = FindPlayerVehicle(-1, false)) {
            return veh->m_nVehicleType == VEHICLE_TYPE_TRAIN;
        }
        return false;
    }();

    const CVector targetPos{ target.x, target.y, target.z + ALPHA_RECENTER_Z };

    auto* pad = CPad::GetPad();

    // Compute the angle changes over this frame
    float betaDiff{}, alphaDiff{};
    if (pad->bPlayerSafe) {
        // Center the camera behind the player.
        // FIX: the original branches `dir.z >= -0.9 -> orientation + pi, else atan2(dir.y, dir.x)`
        // (verified in asm at 0x50FA62-0x50FA8C); we had the two outcomes swapped.
        auto dir = m_vecSource - targetPos;
        dir.Normalise();
        const auto betaAngle = dir.z >= Z_OFF
            ? orientation + TWO_PI
            : std::atan2(dir.y, dir.x);
        alphaDiff = 0.0f;
        betaDiff  = betaAngle - m_fHorizontalAngle;
    } else {
        const auto fovMult = m_fFOV * FOV_MULT;
        if ((MOUSE_ABS_X == 0.0f && MOUSE_ABS_Y == 0.0f)
         || pad->DisablePlayerControls != 0
        ) {
            // Use the raw mouse deltas
            betaDiff  = BETA_MULT  * fovMult * CTimer::ms_fTimeStep * INPUT_MULT * static_cast<float>(-GetMouseX());
            alphaDiff = ALPHA_MULT * fovMult * CTimer::ms_fTimeStep * static_cast<float>(GetMouseY()) * INPUT_MULT;
        } else {
            // Use the absolute mouse position
            betaDiff  = TheCamera.m_fMouseAccelHorzntl * fovMult * MOUSE_ABS_X * BETA_ABS_MULT;
            alphaDiff = fovMult * MOUSE_ABS_Y * ALPHA_ABS_MULT * TheCamera.m_fMouseAccelVertical;
        }
    }

    // Re-center the camera vertically when the screen is faded out / controls disabled.
    // FIX: original compares 45 < FadeValue (const at 0x8CC7D4), we had `<`.
    if ((TheCamera.m_bUseNearClipScript && TheCamera.m_nFadeInOutFlag == eFadeFlag::FADE_OUT && CDraw::FadeValue > 45)
     || (CDraw::FadeValue > 200 || pad->bPlayerSafe)
    ) {
        alphaDiff = ALPHA_STEP;
        if (m_fVerticalAngle >= ALPHA_RECENTER_A - ALPHA_STEP) {
            if (ALPHA_RECENTER_A <= m_fVerticalAngle) {
                alphaDiff = ALPHA_STEP_NEG;
                if (m_fVerticalAngle <= ALPHA_RECENTER_A + ALPHA_STEP && ALPHA_RECENTER_A < m_fVerticalAngle) {
                    alphaDiff = ALPHA_RECENTER_A - m_fVerticalAngle;
                }
            }
        } else {
            alphaDiff = ALPHA_RECENTER_A - m_fVerticalAngle;
        }
    }

    m_fHorizontalAngle += betaDiff;
    m_fVerticalAngle   += alphaDiff;
    ClipBeta();

    // Clamp the vertical angle
    if (m_fVerticalAngle <= PI_4) {
        if (m_fVerticalAngle < ALPHA_MAX) {
            m_fVerticalAngle = ALPHA_MAX_INL;
        }
    } else {
        m_fVerticalAngle = ALPHA_PI_4;
    }

    // The higher we look, the bigger the distance
    const auto distMult = m_fVerticalAngle <= 0.0f
        ? std::cos(m_fVerticalAngle)
        : std::cos(std::min(m_fVerticalAngle * DIST_MULT_SCALE, DIST_MULT_MAX));
    const auto radius = distMult * DIST_MULT_MUL + DIST_MULT_BASE;

    // Forced orientation overrides
    if (TheCamera.m_bUseTransitionBeta)  m_fHorizontalAngle = m_fTransitionBeta;
    if (TheCamera.m_bCamDirectlyBehind)  m_fHorizontalAngle = TheCamera.m_fPedOrientForBehindOrInFront + TWO_PI;
    if (TheCamera.m_bCamDirectlyInFront) m_fHorizontalAngle = TheCamera.m_fPedOrientForBehindOrInFront;
    if (bDrivingTrain)                   m_fHorizontalAngle = orientation;

    // Compute the front vector and the source
    const auto cosA = std::cos(m_fVerticalAngle);
    const auto cosB = std::cos(m_fHorizontalAngle);
    const auto sinA = std::sin(m_fVerticalAngle);
    const auto sinB = std::sin(m_fHorizontalAngle);
    m_vecFront = CVector{ -(cosB * cosA), -(sinB * cosA), sinA };

    m_vecSource                   = targetPos - radius * m_vecFront;
    m_vecTargetCoorsForFudgeInter = targetPos;
    CWorld::pIgnoreEntity         = m_pCamTargetEntity;

    CColPoint cp;
    CEntity*  hitEnt{};
    if (CWorld::ProcessLineOfSight(targetPos, m_vecSource, cp, hitEnt, true, true, true, true, false, false, true, false)) {
        const auto hitDist = (targetPos - cp.m_vecPoint).Magnitude();
        const auto bHitEntIsPed = hitEnt && hitEnt->GetIsTypePed();
        if (!bHitEntIsPed || radius - hitDist <= MIN_DIST) {
            if (!(HIT_KEEP_DIST <= hitDist)) {
                auto newClip = hitDist - NEARCLIP_SUB;
                if (newClip < NEARCLIP_FLOOR) {
                    newClip = NEARCLIP_FLOOR;
                }
                SetNearClip(newClip);
            }
            m_vecSource = cp.m_vecPoint;
        } else {
            // Hit a ped - try to find an obstruction between the ped and our source instead
            CColPoint cp2;
            CEntity*  hitEnt2{};
            if (!CWorld::ProcessLineOfSight(cp.m_vecPoint, m_vecSource, cp2, hitEnt2, true, true, true, true, false, false, true, false)) {
                auto newClip = (radius - hitDist) - NEARCLIP_SUB2;
                if (newClip <= NEARCLIP_MAX) {
                    SetNearClip(newClip);
                } else {
                    SetNearClip(NEARCLIP_MAX_INL);
                }
            } else {
                const auto hitDist2 = (targetPos - cp2.m_vecPoint).Magnitude();
                m_vecSource = cp2.m_vecPoint;
                if (!(HIT_KEEP_DIST <= hitDist2)) {
                    auto newClip = hitDist2 - NEARCLIP_SUB;
                    if (newClip < NEARCLIP_FLOOR) {
                        newClip = NEARCLIP_FLOOR;
                    }
                    SetNearClip(newClip);
                }
            }
        }
    }

    CWorld::pIgnoreEntity = nullptr;

    // Shrink the near clip plane if it intersects something along the source ray
    for (int32 i = 0; i < 6; i++) {
        const auto nearClip = GetRwCameraNearClip();
        const auto radiusScale = CDraw::ms_fAspectRatio
            * std::tan(m_fFOV * FOV_TO_RAD * HALF)
            * ASPECT_MULT;

        const auto probe = m_vecSource + m_vecFront * nearClip;
        if (!CWorld::TestSphereAgainstWorld(probe, nearClip * radiusScale, nullptr, true, true, false, true, false, false)) {
            break;
        }

        // Distance of the (first) hit point from the source ray
        const auto& hitPos = gaTempSphereColPoints[0].m_vecPoint;
        const auto  along  = DotProduct(hitPos - m_vecSource, m_vecFront);
        const auto  perp   = (hitPos - m_vecSource) - m_vecFront * along;
        auto newClip = perp.Magnitude() * (1.0f / radiusScale);
        if (newClip < MIN_CLIP) {
            newClip = MIN_CLIP;
        }
        if (newClip < nearClip) {
            SetNearClip(newClip);
        }
        if (newClip == MIN_CLIP) {
            m_vecSource += (targetPos - m_vecSource) * NEARCLIP_SUB;
        }
    }

    const auto dist = (targetPos - m_vecSource).Magnitude();
    if (m_fDistance <= dist) {
        const auto t = GetDampingFactor();
        m_fDistance = t * m_fDistance + (LERP_CLAMP - t) * dist;
        if (NEARCLIP_FLOOR < dist) {
            m_vecSource = targetPos + (m_vecSource - targetPos) * (m_fDistance / dist);
        }

        auto newClip = t - TAN_HALF_FOV_SUB;
        if (newClip < GetRwCameraNearClip()) {
            SetNearClip(newClip <= MIN_CLIP ? NEARCLIP_FLOOR2 : newClip);
        }
    } else {
        m_fDistance = dist;
    }

    TheCamera.m_bCamDirectlyBehind  = false;
    TheCamera.m_bCamDirectlyInFront = false;
    GetVectorsReadyForRW();

    // Update the player's rotation to match the camera (when the screen is faded out).
    // FIX: original computes atan2(-front.x, front.y) (fld/fchs/fld/fpatan at 0x510533-0x510543);
    // we had the atan2 arguments swapped, which rotates the written heading arbitrarily.
    if (TheCamera.m_bUseNearClipScript && TheCamera.m_nFadeInOutFlag == eFadeFlag::FADE_OUT && CDraw::FadeValue > 0x80) {
        const auto angle = std::atan2(-m_vecFront.x, m_vecFront.y);
        auto* player = m_pCamTargetEntity->AsPed();
        player->m_fCurrentRotation = angle;
        player->m_fAimingRotation  = angle;

        if (!player->m_matrix) {
            player->m_placement.m_fHeading = angle;
        } else {
            player->m_matrix->SetRotateZOnly(angle);
        }

        if (player->GetRwObject()) {
            // NOTE: RwObject's parent + 0x10 = the frame's LTM
            const auto ltm = reinterpret_cast<RwMatrix*>(
                *reinterpret_cast<uintptr_t*>(reinterpret_cast<uint8*>(player->GetRwObject()) + 4) + 0x10);
            if (player->m_matrix) {
                player->m_matrix->UpdateRwMatrix(ltm);
            } else {
                player->m_placement.UpdateRwMatrix(ltm);
            }
        }
    }
}

// 0x522D40
// STILL DISABLED: history tracking, WellBufferMe damping and the ducking beta nudges are
// TODO(mangled) in the first-pass translation.
void CCam::Process_FollowPed_SA(const CVector&, float, float, float, bool) {
    NOTSA_UNREACHABLE();
}

// 0x5105C0
void CCam::Process_M16_1stPerson(const CVector& target, float /*orientation*/, float /*speedVar*/, float /*speedVarWanted*/) {
    // Camera-related statics
    static auto& MOUSE_WHEEL       = StaticRef<float>(0xB73420); // CMouse: mouse wheel delta (>0 up, <0 down)
    static auto& MOUSE_ABS_X       = StaticRef<float>(0xB73424); // CMouse: absolute mouse X position
    static auto& MOUSE_ABS_Y       = StaticRef<float>(0xB73428); // CMouse: absolute mouse Y position
    static auto& s_FovLerp         = StaticRef<float>(0xB6FFE8); // FOV we lerp to (scripted zoom)
    static auto& s_FovLerpUnused1  = StaticRef<float>(0xB6FFEC); // set to 0 on reset
    static auto& s_FovLerpUnused2  = StaticRef<float>(0xB6FFF0); // set to 0 on reset
    static auto& s_NearClipBlocked = StaticRef<bool>(0xB6FFF4);  // near clip was shrunk because of an obstruction
    static auto& s_FPFlag          = StaticRef<bool>(0xB6EC2E);  // unknown first-person flag
    // Attached-mode (turret) angles
    static auto& s_AttachedAlphaSnap = StaticRef<float>(0xA44498); // remaining angle to snap the beta by; zeroed when aligned
    static auto& s_AttachedBetaSnap  = StaticRef<float>(0xA4449C); // the horizontal angle we snap to
    // Consts
    static const auto& TWO_PI           = StaticRef<float>(0x858CBC); // 2*pi
    static const auto& PI_2             = StaticRef<float>(0x858FE4); // pi/2
    static const auto& FOV_MULT         = StaticRef<float>(0x85A5C4); // 0.0125
    static const auto& HALF             = StaticRef<float>(0x858B8C); // 0.5
    static const auto& THREE            = StaticRef<float>(0x858B3C); // 3
    static const auto& LERP             = StaticRef<float>(0x858624); // 1
    static const auto& TWO_POW_32       = StaticRef<float>(0x858C54); // 2^32
    static const auto& MIN_FOV_SNIPER   = StaticRef<float>(0x858B48); // 15
    static const auto& WHEEL_RATE       = StaticRef<float>(0x862F50); // 7
    static const auto& WHEEL_BASE       = StaticRef<float>(0x859AA4); // 10000
    static const auto& FOV_RATE         = StaticRef<float>(0x859AAC); // 255
    static const auto& VERY_SMALL       = StaticRef<float>(0x858FC4); // 0.0001
    static const auto& STICK_MULT       = StaticRef<float>(0x858B88); // 0.0078125 (1/128)
    static const auto& STICK_ALPHA_MUL  = StaticRef<float>(0x858CEC); // 0.04
    static const auto& STICK_BETA_MUL   = StaticRef<float>(0x863100); // 0.0571429
    static const auto& STICK_ALPHA_MUL2 = StaticRef<float>(0x8631C0); // 0.0342857
    static const auto& STICK_BETA_MUL2  = StaticRef<float>(0x859B50); // 1/14
    static const auto& STICK_TIME_MULT  = StaticRef<float>(0x8630FC); // 4.44444e-05
    static const auto& MOUSE_BETA_MULT  = StaticRef<float>(0x859024); // -3
    static const auto& BUMP_SIZE        = StaticRef<float>(0x8CC480); // 0.1
    static const auto& BUMP_PERIOD      = StaticRef<float>(0x8CC474); // 800
    static const auto& BUMP_DURATION    = StaticRef<float>(0x8CC478); // 600
    static const auto& TURRET_MIX       = StaticRef<float>(0x862F44); // 0.75
    static const auto& TURRET_STEP      = StaticRef<float>(0x862F48); // 0.05
    static const auto& TURRET_BIKE_ROLL = StaticRef<float>(0x862F4C); // 0.1
    static const auto& ALPHA_LIM_NEAR   = StaticRef<float>(0x8631B4); // 0.349066
    static const auto& BETA_LIM_NEAR    = StaticRef<float>(0x8631B8); // 0.610865
    static const auto& MIN_FOV_NEARCLIP = StaticRef<float>(0x862F54); // 0.15
    static const auto& ALPHA_MAX_2      = StaticRef<float>(0x8630F8); // 1.0472
    static const auto& ALPHA_MIN_2      = StaticRef<float>(0x8631BC); // -1.49226
    static constexpr float ALPHA_MIN_2_INL  = -1.49226f; // 0xBFBF0243 (inlined)
    static constexpr float ALPHA_MAX_2_INL  = 1.0472f; // 0x3F860A92 (inlined)

    // Original calls CRT pow(base, CTimer::ms_fTimeStep) (0x822130 is the msvc pow dispatcher):
    // - stick-input smoothing: base = 0.8 (0x8CCC98), or 0.5 (0x8CCC94) when both stick axes
    //   are pushed to >= 2.0 (0x858CA0) (asm 0x510CCD-0x510D13)
    // - camera-bump decay: base = 0.95 (0x8CC47C) (asm 0x510DEA-0x510E13)
    const auto GetDampingFactor = [](float base) { return std::pow(base, CTimer::ms_fTimeStep); };
    const auto SetNearClip = [](float v) { RwCameraSetNearClipPlane(Scene.m_pRwCamera, v); };

    if (!m_pCamTargetEntity->GetIsTypePed()) {
        return;
    }
    auto* ped = m_pCamTargetEntity->AsPed();

    // "Attached" == the player is attached to an entity (e.g. a turret)
    const bool bAttached = ped->IsPlayer() && ped->m_pAttachedTo != nullptr;

    const CVector targetPos{ target.x, target.y, target.z };

    if (m_bResetStatics) {
        if (!s_FPFlag || ped->m_pTargetedObject) {
            m_fHorizontalAngle = bAttached ? s_AttachedBetaSnap : ped->m_fCurrentRotation - PI_2;
            m_fVerticalAngle   = 0.0f;
        } else if (bAttached) {
            m_fHorizontalAngle = s_AttachedBetaSnap;
            m_fVerticalAngle   = 0.0f;
        }

        s_FovLerp                   = m_fFOV;
        m_fInitialPlayerOrientation = ped->m_fCurrentRotation - PI_2;
        m_bResetStatics             = false;
        s_NearClipBlocked           = false;
        s_FovLerpUnused1            = 0.0f; // FIX: original zeroes 0xB6FFEC too (asm 0x510725)
        s_FovLerpUnused2            = 0.0f;
        m_bCollisionChecksOn        = true;
        m_fFOVSpeed                 = 0.0f;
        m_fAlphaSpeed               = 0.0f;
        m_fBetaSpeed                = 0.0f;
    }

    if (m_nMode == eCamMode(7) || m_nMode == eCamMode(0x2E)) { // sniper / heat-active rocket modes
        bool bFovChanged = false;

        const auto zoomInBtn  = ControlsManager.GetMouseButtonAssociatedWithAction(eControllerAction::PED_SNIPER_ZOOM_IN);
        const auto zoomOutBtn = ControlsManager.GetMouseButtonAssociatedWithAction(eControllerAction::PED_SNIPER_ZOOM_OUT);
        if ((MOUSE_WHEEL > 0.0f && zoomOutBtn == 4 /* rsMOUSE_WHEEL_UP */)
         || (MOUSE_WHEEL < 0.0f && zoomOutBtn == 5 /* rsMOUSE_WHEEL_DOWN */)
        ) {
            s_FovLerp = (std::abs(MOUSE_WHEEL) * WHEEL_RATE + WHEEL_BASE) * VERY_SMALL * s_FovLerp;
            bFovChanged = true;
        } else if ((MOUSE_WHEEL > 0.0f && zoomInBtn == 4 /* rsMOUSE_WHEEL_UP */)
                || (MOUSE_WHEEL < 0.0f && zoomInBtn == 5 /* rsMOUSE_WHEEL_DOWN */)
        ) {
            s_FovLerp = s_FovLerp / ((std::abs(MOUSE_WHEEL) * WHEEL_RATE + WHEEL_BASE) * VERY_SMALL);
            bFovChanged = true;
        }

        auto* pad = CPad::GetPad();
        if (!pad->SniperZoomOut() || bFovChanged) {
            if (pad->SniperZoomIn() && !bFovChanged) {
                m_fFOV = m_fFOV / ((CTimer::ms_fTimeStep * FOV_RATE + WHEEL_BASE) * VERY_SMALL);
                s_FovLerp = m_fFOV;
            } else if (std::abs(s_FovLerp - m_fFOV) > HALF) {
                WellBufferMe(s_FovLerp, m_fFOV, m_fFOVSpeed, 0.5f, 0.25f, false);
            }
        } else {
            m_fFOV = (CTimer::ms_fTimeStep * FOV_RATE + WHEEL_BASE) * VERY_SMALL * m_fFOV;
            s_FovLerp = m_fFOV;
        }
        m_fFOVSpeed = 0.0f;

        if (m_fFOV > 70.0f) {
            m_fFOV = 70.0f;
        }
        if (s_FovLerp <= 70.0f) {
            if (m_nMode == eCamMode(0x2E)) {
                if (m_fFOV < THREE)    m_fFOV = 3.0f;
                if (s_FovLerp < THREE) s_FovLerp = 3.0f;
            } else {
                if (m_fFOV < MIN_FOV_SNIPER)    m_fFOV = 15.0f;
                if (s_FovLerp < MIN_FOV_SNIPER) s_FovLerp = 15.0f;
            }
        } else {
            s_FovLerp = 70.0f;
        }

        TheCamera.m_nBlurRed    = 180;
        TheCamera.m_nBlurGreen  = 255;
        TheCamera.m_nBlurBlue   = 180;
        TheCamera.m_nBlurType   = eMotionBlurType::SNIPER;
        TheCamera.m_nMotionBlur = 120;
    } else {
        m_fFOV = 70.0f;
    }

    // Snap the beta towards the attached entity's orientation
    if (bAttached && 0.0f < s_AttachedAlphaSnap) {
        auto diff = m_fHorizontalAngle - s_AttachedBetaSnap;
        if (diff < 0.0f) {
            diff += TWO_PI;
        }
        if (diff < s_AttachedAlphaSnap || TWO_PI - diff < s_AttachedAlphaSnap) {
            m_fHorizontalAngle  = s_AttachedBetaSnap;
            s_AttachedAlphaSnap = 0.0f;
        } else if (diff <= TWO_PI - diff) {
            m_fHorizontalAngle -= s_AttachedAlphaSnap;
        } else {
            m_fHorizontalAngle += s_AttachedAlphaSnap;
        }
    }

    // Compute the angle speeds
    float betaSpeed{}, alphaSpeed{};
    auto* pad = CPad::GetPad();
    if (MOUSE_ABS_X == 0.0f && MOUSE_ABS_Y == 0.0f) {
        // No mouse - use keyboard / stick
        const auto lookLr = -static_cast<float>(pad->LookAroundLeftRight(ped));
        const auto lookUd = static_cast<float>(pad->LookAroundUpDown(ped));
        if (bAttached) {
            betaSpeed  = m_fFOV * FOV_MULT * STICK_ALPHA_MUL * std::abs(lookLr * STICK_MULT) * CTimer::ms_fTimeStep * lookLr * STICK_MULT;
            alphaSpeed = m_fFOV * FOV_MULT * STICK_ALPHA_MUL2 * std::abs(lookUd * STICK_MULT) * CTimer::ms_fTimeStep * lookUd * STICK_MULT;
        } else {
            const auto lrSgn = lookLr < 0.0f ? -1.0f : 1.0f;
            const auto udSgn = lookUd < 0.0f ? -1.0f : 1.0f;
            betaSpeed  = m_fFOV * FOV_MULT * STICK_BETA_MUL  * lookLr * lookLr * VERY_SMALL * CTimer::ms_fTimeStep * lrSgn;
            alphaSpeed = m_fFOV * FOV_MULT * STICK_BETA_MUL2 * lookUd * lookUd * STICK_TIME_MULT * CTimer::ms_fTimeStep * udSgn;
        }

        // Smooth the speeds.
        // FIX: original passes the stick magnitudes to the pow-based damping: base is 0.5 when
        // both axes are pushed >= 2.0, else 0.8 (asm 0x510CCD-0x510D13). We used to call pow()
        // with no arguments (garbage base/exponent).
        const auto t = GetDampingFactor(
            std::abs(lookLr) >= StaticRef<float>(0x858CA0) && std::abs(lookUd) >= StaticRef<float>(0x858CA0) // 2.0
                ? StaticRef<float>(0x8CCC94) // 0.5
                : StaticRef<float>(0x8CCC98) // 0.8
        );
        m_fBetaSpeed  = t * m_fBetaSpeed + (LERP - t) * betaSpeed;
        m_fAlphaSpeed = (LERP - t) * alphaSpeed + t * m_fAlphaSpeed;
        betaSpeed     = m_fBetaSpeed;
        alphaSpeed    = m_fAlphaSpeed;
    } else {
        // Use the absolute mouse position
        auto mx = MOUSE_ABS_X * MOUSE_BETA_MULT;
        auto my = MOUSE_ABS_Y * THREE;
        if (pad->DisablePlayerControls != 0 || pad->JustOutOfFrontEnd || CTimer::ms_fTimeStep < 0.0f) {
            mx = 0.0f;
            my = 0.0f;
        }
        betaSpeed     = m_fFOV * FOV_MULT * TheCamera.m_fMouseAccelHorzntl * mx;
        alphaSpeed    = m_fFOV * FOV_MULT * TheCamera.m_fMouseAccelVertical * my;
        m_fAlphaSpeed = 0.0f;
        m_fBetaSpeed  = 0.0f;
    }

    m_fHorizontalAngle += betaSpeed;
    m_fVerticalAngle   += alphaSpeed;
    ClipBeta();

    // Camera bumping
    if (m_nCamBumpedTime > 0) {
        const auto elapsedRaw = static_cast<int32>(CTimer::m_snTimeInMilliseconds) - static_cast<int32>(m_nCamBumpedTime);
        float elapsed = static_cast<float>(elapsedRaw);
        if (elapsedRaw < 0) {
            elapsed += TWO_POW_32;
        }
        const auto f = std::cos((elapsed / BUMP_PERIOD) * TWO_PI);
        m_fHorizontalAngle += BUMP_SIZE * f * m_fCamBumpedHorz;
        m_fVerticalAngle   += f * m_fCamBumpedVert * BUMP_SIZE;
        m_fCamBumpedHorz *= GetDampingFactor(StaticRef<float>(0x8CC47C)); // 0.95 (fix: was pow with garbage args)
        m_fCamBumpedVert *= GetDampingFactor(StaticRef<float>(0x8CC47C));
        if (m_nCamBumpedTime + static_cast<uint32>(BUMP_DURATION) < CTimer::m_snTimeInMilliseconds) {
            m_nCamBumpedTime = 0;
        }
    }

    // If crouching, keep the camera further away from the body
    float sideOffset = 0.3f;
    if (ped->bCrouchWhenShooting) {
        sideOffset = 0.8f;
    }

    if (bAttached) {
        auto* attached = ped->m_pAttachedTo;

        float betaLim  = 1.0471976f;  // how fast we follow the attached entity's pitch
        float alphaLim = 1.4922565f;  // ... and its roll
        float fwdOff   = 0.1f;

        const auto GetHeadingOf = [](CEntity* e) -> float {
            if (e->m_matrix) {
                return std::atan2(-e->m_matrix->GetForward().x, e->m_matrix->GetForward().y);
            }
            return e->m_placement.m_fHeading;
        };
        // FIX: the original takes a *signed* clamped component and feeds it through CRT asin()
        // (clamp to [-1,1] at 0x510E7B-0x510EFE etc, then call 0x821E70 = asin); it is NOT
        // abs(). Which component and its sign depend on the turret mode.
        const auto GetPitchOf = [](CEntity* e, float sign) -> float { // asin(clamp(sign * forward.z))
            if (!e->m_matrix) {
                e->AllocateMatrix();
            }
            return std::asin(std::clamp(sign * e->m_matrix->GetForward().z, -1.0f, 1.0f));
        };
        const auto GetRollOf = [](CEntity* e, float sign) -> float { // asin(clamp(sign * right.z))
            if (!e->m_matrix) {
                e->AllocateMatrix();
            }
            return std::asin(std::clamp(sign * e->m_matrix->GetRight().z, -1.0f, 1.0f));
        };

        float alphaTarget{}, heading{};
        switch (*reinterpret_cast<const uint16*>(reinterpret_cast<const uint8*>(ped) + 0x778)) { // NOTE: read as u16 in the original
        case 1:
            alphaTarget = GetRollOf(attached, -1.0f);
            heading     = GetHeadingOf(attached);
            break;
        case 2:
            alphaTarget = GetPitchOf(attached, -1.0f);
            heading     = GetHeadingOf(attached) + PI_2;
            break;
        case 3: {
            if (!attached->m_matrix) {
                attached->AllocateMatrix();
            }
            alphaTarget = std::asin(std::clamp(attached->m_matrix->GetRight().z, -1.0f, 1.0f));
            heading     = GetHeadingOf(attached) - StaticRef<float>(0x858CB8); // FIX: -pi, was -2pi
            break;
        }
        default:
            alphaTarget = GetPitchOf(attached, 1.0f);
            heading     = GetHeadingOf(attached) - PI_2;
            break;
        }

        plugin::CallMethodAndReturn<void, 0x5DFDF0>(ped); // CPed::PositionAttachedPed (unreversed)
        if (ped->GetRwObject()) {
            const auto ltm = reinterpret_cast<RwMatrix*>(
                *reinterpret_cast<uintptr_t*>(reinterpret_cast<uint8*>(ped->GetRwObject()) + 4) + 0x10);
            if (ped->m_matrix) {
                ped->m_matrix->UpdateRwMatrix(ltm);
            } else {
                ped->m_placement.UpdateRwMatrix(ltm);
            }
        }
        ped->UpdateRwFrame();
        ped->UpdateRpHAnim();

        // Compute the source from the head position.
        // FIX: original checks attached->m_nVehicleType == VEHICLE_TYPE_BIKE (9) at +0x590
        // (asm 0x51122E-0x511244); we compared m_nVehicleSubType to 3.
        CVector src{};
        if (attached->GetIsTypeVehicle() && attached->AsVehicle()->m_nVehicleType == VEHICLE_TYPE_BIKE) {
            fwdOff     = 0.0f;
            sideOffset = 0.0f;
            src        = ped->GetBonePosition(BONE_HEAD, true);
            betaLim = alphaLim = *reinterpret_cast<const float*>(reinterpret_cast<const uint8*>(attached) + 0x780);
        } else {
            src = ped->GetBonePosition(BONE_HEAD, true);
        }
        m_vecSource = src;

        if (!ped->m_matrix) {
            ped->AllocateMatrix();
        }
        m_vecSource += ped->m_matrix->GetUp() * fwdOff;
        if (!ped->m_matrix) {
            ped->AllocateMatrix();
        }
        m_vecSource -= ped->m_matrix->GetForward() * sideOffset;

        // Wrap the heading difference.
        // FIX: original bounds are pi / -pi (0x858CB8 / 0x858CC0) with a +2pi / -pi
        // adjustment (decompile lines 399-407); we used +-2pi for everything.
        {
            const auto d0 = heading - m_fHorizontalAngle;
            if (d0 <= StaticRef<float>(0x858CB8)) { // pi
                if (d0 < StaticRef<float>(0x858CC0)) { // -pi
                    heading += TWO_PI;
                }
            } else {
                heading -= StaticRef<float>(0x858CB8); // pi
            }
        }

        // Limit how fast the beta follows the attached entity's heading.
        // FIX: original checks m_nVehicleSubType == VEHICLE_TYPE_HELI (3) at +0x594
        // (asm 0x5113CC-0x5113D4); we compared the wrong field/value.
        CEntity* subType3Veh = nullptr;
        if (attached->GetIsTypeVehicle() && attached->AsVehicle()->m_nVehicleSubType == VEHICLE_TYPE_HELI) {
            subType3Veh = attached;
        }
        const float tAngB   = *reinterpret_cast<const float*>(reinterpret_cast<const uint8*>(ped) + 0x77C); // m_fTurretAngleB
        const float lim     = TURRET_MIX * tAngB;
        const float rem     = (LERP - TURRET_MIX) * tAngB;
        auto        diff    = heading - m_fHorizontalAngle;
        if (diff <= lim) {
            if (diff < -lim) {
                diff += lim;
                const auto moveAbs = std::abs(diff);
                float step = CTimer::ms_fTimeStep * TURRET_STEP;
                if (rem + step < moveAbs) {
                    step = moveAbs - rem;
                }
                if (subType3Veh && *reinterpret_cast<const float*>(reinterpret_cast<const uint8*>(subType3Veh) + 0x954) > 0.0f) {
                    plugin::CallMethodAndReturn<void, 0x6A2450>(
                        subType3Veh,
                        *reinterpret_cast<const float*>(reinterpret_cast<const uint8*>(subType3Veh) + 0x954)
                            - CTimer::ms_fTimeStep * diff * TURRET_BIKE_ROLL);
                }
                if (diff != 0.0f) {
                    if (diff >= 0.0f) {
                        m_fHorizontalAngle += step;
                    } else {
                        m_fHorizontalAngle -= step;
                    }
                }
            }
        } else {
            diff -= lim;
            const auto moveAbs = std::abs(diff);
            float step = CTimer::ms_fTimeStep * TURRET_STEP;
            if (rem + step < moveAbs) {
                step = moveAbs - rem;
            }
            if (subType3Veh && *reinterpret_cast<const float*>(reinterpret_cast<const uint8*>(subType3Veh) + 0x954) > 0.0f) {
                plugin::CallMethodAndReturn<void, 0x6A2450>(
                    subType3Veh,
                    *reinterpret_cast<const float*>(reinterpret_cast<const uint8*>(subType3Veh) + 0x954)
                        - CTimer::ms_fTimeStep * diff * TURRET_BIKE_ROLL);
            }
            if (diff != 0.0f) {
                if (diff >= 0.0f) {
                    m_fHorizontalAngle += step;
                } else {
                    m_fHorizontalAngle -= step;
                }
            }
        }

        // Limit how fast the alpha follows the attached entity's pitch
        {
            const float l1 = TURRET_MIX * betaLim;
            const float r1 = (LERP - TURRET_MIX) * betaLim;
            if (alphaTarget + l1 < m_fVerticalAngle && (alphaTarget - m_fVerticalAngle) < -l1) {
                const auto d = std::abs((alphaTarget - m_fVerticalAngle) + l1);
                float step = CTimer::ms_fTimeStep * TURRET_STEP;
                if (r1 + step < d) {
                    step = d - r1;
                }
                m_fVerticalAngle -= step;
            }

            const float l2 = TURRET_MIX * alphaLim;
            const float r2 = (LERP - TURRET_MIX) * alphaLim;
            if (m_fVerticalAngle < alphaTarget - l2 && (alphaTarget - m_fVerticalAngle) > l2) {
                const auto d = std::abs((alphaTarget - m_fVerticalAngle) - l2);
                float step = CTimer::ms_fTimeStep * TURRET_STEP;
                if (r2 + step < d) {
                    step = d - r2;
                }
                m_fVerticalAngle += step;
            }
        }
    } else {
        // Not attached
        if (m_fVerticalAngle <= ALPHA_MAX_2) {
            if (m_fVerticalAngle < ALPHA_MIN_2) {
                m_fVerticalAngle = ALPHA_MIN_2_INL;
            }
        } else {
            m_fVerticalAngle = ALPHA_MAX_2_INL;
        }

        if (ped->GetRwObject()) {
            const auto ltm = reinterpret_cast<RwMatrix*>(
                *reinterpret_cast<uintptr_t*>(reinterpret_cast<uint8*>(ped->GetRwObject()) + 4) + 0x10);
            if (ped->m_matrix) {
                ped->m_matrix->UpdateRwMatrix(ltm);
            } else {
                ped->m_placement.UpdateRwMatrix(ltm);
            }
        }
        ped->UpdateRwFrame();
        ped->UpdateRpHAnim();

        m_vecSource = ped->GetBonePosition(BONE_HEAD, true);
        m_vecSource.z += StaticRef<float>(0x858B1C); // 0.1

        const auto EnsureMatrix = [&] {
            if (!ped->m_matrix) {
                ped->AllocateMatrix();
            }
        };
        if (!ped->bCrouchWhenShooting) {
            EnsureMatrix();
            m_vecSource -= ped->m_matrix->GetForward() * sideOffset;
        } else {
            EnsureMatrix();
            m_vecSource -= ped->m_matrix->GetForward() * StaticRef<float>(0x8CC7BC); // 0.5
            EnsureMatrix();
            m_vecSource -= ped->m_matrix->GetRight() * StaticRef<float>(0x8CC7B8); // 2
        }
    }

    // Clamp the alpha
    if (-StaticRef<float>(0x8CCC90) /* 1.2 */ > m_fVerticalAngle) {
        m_fVerticalAngle = -StaticRef<float>(0x8CCC90);
    } else if (m_fVerticalAngle > StaticRef<float>(0x8CCC90)) {
        m_fVerticalAngle = StaticRef<float>(0x8CCC90);
    }

    // Compute the front vector
    const auto cosA = std::cos(m_fVerticalAngle);
    const auto cosB = std::cos(m_fHorizontalAngle);
    const auto sinA = std::sin(m_fVerticalAngle);
    const auto sinB = std::sin(m_fHorizontalAngle);
    m_vecFront = CVector{ -(cosB * cosA), -(sinB * cosA), sinA };

    const CVector probe = m_vecSource + m_vecFront * THREE;
    m_vecSource += m_vecFront * StaticRef<float>(0x858EE8); // 0.4

    bool bSkipEnd = false;
    if (m_bCollisionChecksOn == 1) {
        auto IsClear = [](const CVector& from, const CVector& to) {
            return CWorld::GetIsLineOfSightClear(from, to, true, true, false, true, false, true, true);
        };
        auto ProbePos = [&](float beta, float alpha) {
            return m_vecSource + CVector{ std::cos(beta) * std::cos(alpha), std::sin(beta) * std::cos(alpha), std::sin(alpha) } * THREE;
        };
        if (!IsClear(probe, m_vecSource)) {
            SetNearClip(0.3f);
            s_NearClipBlocked = true;
            bSkipEnd = true;
        } else if (!IsClear(ProbePos(m_fHorizontalAngle + BETA_LIM_NEAR, m_fVerticalAngle - ALPHA_LIM_NEAR), m_vecSource)) {
            SetNearClip(0.3f);
            s_NearClipBlocked = true;
            bSkipEnd = true;
        } else if (!IsClear(ProbePos(m_fHorizontalAngle - BETA_LIM_NEAR, m_fVerticalAngle - ALPHA_LIM_NEAR), m_vecSource)) {
            SetNearClip(0.3f);
            s_NearClipBlocked = true;
            bSkipEnd = true;
        } else {
            s_NearClipBlocked = false;
        }
    } else if (s_NearClipBlocked) {
        bSkipEnd = true;
    }

    if (!bSkipEnd && m_nMode == eCamMode(0x2E)) {
        const auto f = std::min(MIN_FOV_SNIPER, m_fFOV);
        SetNearClip(((MIN_FOV_SNIPER - f) * MIN_FOV_NEARCLIP + LERP) * StaticRef<float>(0x858C24) /* 0.3 */);
    }

    m_vecSource -= m_vecFront * StaticRef<float>(0x858EE8); // 0.4
    GetVectorsReadyForRW();

    const auto rot = std::atan2(-m_vecFront.x, m_vecFront.y);
    if (auto* tped = TheCamera.m_pTargetEntity ? TheCamera.m_pTargetEntity->AsPed() : nullptr) {
        tped->m_fCurrentRotation = rot;
        tped->m_fAimingRotation  = rot;
    }
}

// 0x511B50
void CCam::Process_Rocket(const CVector& target, float orientation, float speedVar, float speedVarWanted, bool isHeatSeeking) {
    static auto& dword_B6FFF8 = StaticRef<uint32>(0xB6FFF8);
    static auto& dword_B6FFFC = StaticRef<uint32>(0xB6FFFC);
    static auto& byte_B70000  = StaticRef<bool>(0xB70000);

    if (!m_pCamTargetEntity->GetIsTypePed()) {
        return;
    }

    auto* targetPed = m_pCamTargetEntity->AsPed();
    m_fFOV = 70.0f;
    if (m_bResetStatics) {
        if (!CCamera::m_bUseMouse3rdPerson || targetPed->m_pTargetedObject) {
            m_fVerticalAngle = 0.0f;
            m_fHorizontalAngle = targetPed->m_fCurrentRotation - DegreesToRadians(90.0f);
        }
        m_fInitialPlayerOrientation = m_fHorizontalAngle;
        m_bResetStatics             = 0;
        m_bCollisionChecksOn        = true;
        byte_B70000                 = 0;
        dword_B6FFFC                = 0;
        dword_B6FFF8                = 0;
    }
    m_pCamTargetEntity->UpdateRwMatrix();
    m_pCamTargetEntity->UpdateRwFrame();
    CVector headPosition{};
    targetPed->GetTransformedBonePosition(headPosition, eBoneTag::BONE_HEAD, true);
    m_vecSource = headPosition + CVector{0.0f, 0.0f, 0.1f};

    auto*      pad1   = CPad::GetPad(0);
    const auto fov    = m_fFOV / 80.0f;
    const auto amountMouseMoved = pad1->NewMouseControllerState.GetAmountMouseMoved();
    
    if (!amountMouseMoved.IsZero()) {
        m_fHorizontalAngle += -3.0f * amountMouseMoved.x * fov * CCamera::m_fMouseAccelHorzntl;
        m_fVerticalAngle += +4.0f * amountMouseMoved.y * fov * CCamera::m_fMouseAccelVertical;
    } else {
        const auto hv  = (float)-pad1->LookAroundLeftRight(targetPed);
        const auto vv  = (float)pad1->LookAroundUpDown(targetPed);

        m_fHorizontalAngle += sq(hv) / 10000.0f * fov / 17.5f * CTimer::GetTimeStep() * (hv < 0.0f ? -1.0f : 1.0f);
        m_fVerticalAngle   += sq(vv) / 22500.0f * fov / 14.0f * CTimer::GetTimeStep() * (vv < 0.0f ? -1.0f : 1.0f);
    }
    ClipBeta();
    ClipAlpha();

    m_vecFront.Set(
        -(std::cos(m_fHorizontalAngle) * std::cos(m_fVerticalAngle)),
        -(std::sin(m_fHorizontalAngle) * std::cos(m_fVerticalAngle)),
        std::sin(m_fVerticalAngle)
    );
    GetVectorsReadyForRW();

    const auto heading = CGeneral::GetATanOfXY(m_vecFront.x, m_vecFront.y) - DegreesToRadians(90.0f);
    TheCamera.m_pTargetEntity->AsPed()->m_fCurrentRotation = heading;
    TheCamera.m_pTargetEntity->AsPed()->m_fAimingRotation  = heading;

    if (isHeatSeeking) {
        auto* player     = FindPlayerPed();
        auto* playerData = player->GetPlayerData();
        if (!playerData->m_nFireHSMissilePressedTime) {
            playerData->m_nFireHSMissilePressedTime = CTimer::GetTimeInMS();
        }

        const auto hsTarget = CWeapon::PickTargetForHeatSeekingMissile(
            m_vecSource,
            m_vecFront,
            1.2f,
            player,
            false,
            playerData->m_LastHSMissileTarget
        );

        // NOTE: not sure about the second one
        if (hsTarget && CTimer::GetTimeInMS() - playerData->m_nLastHSMissileLOSTime > 1'000) {
            playerData->m_nLastHSMissileLOSTime = CTimer::GetTimeInMS();

            const auto targetUsesCollision = hsTarget->GetUsesCollision();
            const auto playerUsesCollision = player->GetUsesCollision();
            hsTarget->SetUsesCollision(false);
            player->SetUsesCollision(false);

            const auto isClear = CWorld::GetIsLineOfSightClear(
                player->GetPosition(),
                hsTarget->GetPosition(),
                true,
                true,
                false,
                true,
                false,
                true
            );
            player->SetUsesCollision(playerUsesCollision);
            hsTarget->SetUsesCollision(targetUsesCollision);
            playerData->m_bLastHSMissileLOS = isClear;
        }

        if (!playerData->m_bLastHSMissileLOS || !hsTarget || hsTarget != playerData->m_LastHSMissileTarget) {
            playerData->m_nFireHSMissilePressedTime = CTimer::GetTimeInMS();
        }

        if (hsTarget) {
            CWeaponEffects::MarkTarget(
                CrossHairId(0),
                hsTarget->GetPosition(),
                255,
                255,
                255,
                100,
                1.3f,
                true
            );
        }

        auto& crosshair = gCrossHair[CrossHairId(0)];
        const auto time = CTimer::GetTimeInMS() - playerData->m_nFireHSMissilePressedTime;

        crosshair.m_nTimeWhenToDeactivate = 0;
        crosshair.m_color.Set(
            255,
            time <= 1'500 ? 255 : 0,
            time <= 1'500 ? 255 : 0
        );
        crosshair.m_fRotation = time <= 1'500 ? 0.0f : 1.0f;
        playerData->m_LastHSMissileTarget = hsTarget;
    }

    constexpr auto ROCKET_CAM_NEARCLIP_PLANE = 0.15f; // 0x8CCC9C
    RwCameraSetNearClipPlane(Scene.m_pRwCamera, ROCKET_CAM_NEARCLIP_PLANE);
}

// 0x517500
void CCam::Process_SpecialFixedForSyphon(const CVector& target, float /*orientation*/, float /*speedVar*/, float /*speedVarWanted*/) {
    m_vecSource = m_vecCamFixedModeSource;

    m_vecTargetCoorsForFudgeInter = target;
    m_vecTargetCoorsForFudgeInter.z += m_fSyphonModeTargetZOffSet;

    m_vecFront = target - m_vecSource;
    m_vecFront.z += m_fSyphonModeTargetZOffSet;

    // 0x514030 -- unreversed helper: CCamera::__thiscall(from, target, inOutSrc, fov), this = TheCamera (0xB6F028)
    CVector srcCopy = m_vecSource;
    plugin::CallMethodAndReturn<void, 0x514030>(&TheCamera, &srcCopy, &m_vecTargetCoorsForFudgeInter, &m_vecSource, m_fFOV);

    GetVectorsReadyForRW();

    m_vecUp += m_vecCamFixedModeUpOffSet;
    m_vecUp.Normalise();

    const auto right = CrossProduct(m_vecUp, m_vecFront).Normalized();
    m_vecFront = CrossProduct(right, m_vecUp).Normalized();

    m_fFOV = 70.0f;

    // Rotate the ped towards the target if they are firing a gun without aiming down the arm
    if (m_pCamTargetEntity && m_pCamTargetEntity->GetIsTypePed()) {
        auto* ped = m_pCamTargetEntity->AsPed();
        if (ped->m_pTargetedObject && ped->m_nActiveWeaponSlot != 0) {
            const auto& weapon = ped->GetWeaponInSlot(ped->m_nActiveWeaponSlot);
            const auto* wi     = CWeaponInfo::GetWeaponInfo(weapon.GetType(), ped->GetWeaponSkill());

            if (wi->m_nWeaponFire != eWeaponFire::WEAPON_FIRE_MELEE
                && (!wi->flags.bAimWithArm || ped->bIsDucking)
            ) {
                const auto delta   = ped->m_pTargetedObject->GetPosition() - ped->GetPosition();
                const auto heading = CGeneral::GetATanOfXY(delta.x, -delta.y); // original: fpatan(-(delta.x), delta.y)

                ped->m_fAimingRotation  = heading;
                ped->m_fCurrentRotation = heading;
                ped->SetHeading(heading);
                ped->UpdateRwMatrix();
            }
        }
    }
}

// 0x512110
bool CCam::Process_WheelCam(const CVector&, float, float, float) {
    static auto& WATER_MARGIN     = StaticRef<float>(0x858C24);
    static auto& BOAT_NO_DRV_Z    = StaticRef<float>(0x8CCC68);
    static auto& BOAT_DRV_K       = StaticRef<float>(0x8CCC60);
    static auto& BOAT_FWD_K       = StaticRef<float>(0x8CCC64);
    static auto& VORTEX_RIGHT_K   = StaticRef<float>(0x8CCC6C);
    static auto& VORTEX_SPEC_C    = StaticRef<float>(0x8CCCA0);
    static auto& VORTEX_SPEC_V    = StaticRef<CVector>(0x8CCC70);
    static auto& VORTEX_SPEC_R    = StaticRef<CVector>(0x8CCCA4);
    static auto& VORTEX_SPEC_SCL  = StaticRef<float>(0x8CCC74);
    static auto& OFFS_X_K         = StaticRef<float>(0x8CC7DC);
    static auto& OFFS_X_K2        = StaticRef<float>(0x8CC7E0);
    static auto& WHEEL_FREQ       = StaticRef<float>(0x8631C4);
    static auto& WHEEL_AMP        = StaticRef<float>(0x858EE8);
    static auto& TRAIN_DOT        = StaticRef<float>(0x858B50);

    auto* target = m_pCamTargetEntity;
    m_fFOV = 70.0f; // 0x428C0000

    if (target->GetIsTypePed()) {
        if (!target->m_matrix) {
            target->AllocateMatrix();
            target->UpdateRwMatrix();
        }
        m_vecSource = target->GetMatrix().TransformVector(CVector{ -0.3f, -0.5f, 0.1f });
        m_vecSource += target->GetPosition();
        m_vecSource.z += 1.0f; // TODO: exact origin of this +1.0 is mangled in the decomp
        m_vecFront = CVector{ 1.0f, 0.0f, 0.0f };
    } else {
        // vehicle: sit at the front of the model
        const auto* colModel = target->GetColModel();
        const CVector ofs{ colModel->m_boundBox.m_vecMin.x - OFFS_X_K, -2.3f, 0.3f };
        if (!target->m_matrix) {
            target->AllocateMatrix();
            target->UpdateRwMatrix();
        }
        m_vecSource = target->m_matrix->TransformPoint(ofs);
        m_vecFront  = target->m_matrix->GetForward();
    }

    CVector right{}, up2{};
    const bool bVehicle = target->GetIsTypeVehicle();
    if (bVehicle && (target->AsVehicle()->GetVehicleAppearance() == VEHICLE_APPEARANCE_HELI
                  || target->AsVehicle()->GetVehicleAppearance() == VEHICLE_APPEARANCE_PLANE)) {
        // helis / planes: fixed seat position
        auto* veh = target->AsVehicle();
        if (!veh->m_matrix) {
            veh->AllocateMatrix();
            veh->UpdateRwMatrix();
        }
        right       = veh->m_matrix->GetRight();
        up2         = veh->m_matrix->GetUp();
        m_vecSource = veh->m_matrix->TransformPoint(CVector{ -1.55f, 0.0f, 0.0f });
    } else if (bVehicle) {
        auto* veh = target->AsVehicle();
        if (veh->m_nVehicleSubType == VEHICLE_TYPE_BOAT) {
            right = CrossProduct(m_vecUp, CVector{ 0.0f, 0.0f, 1.0f });
            right.Normalise();
            up2 = CrossProduct(right, m_vecUp);
            up2.Normalise();

            if (!veh->m_pDriver) {
                m_vecSource.z += BOAT_NO_DRV_Z + BOAT_NO_DRV_Z;
            } else {
                auto* driver = veh->m_pDriver;
                CVector headPos = driver->GetBonePosition(static_cast<eBoneTag>(5), true);

                headPos += right * BOAT_DRV_K + veh->m_vecMoveSpeed * CTimer::GetTimeStep();
                headPos += driver->GetMatrix().GetForward() * BOAT_FWD_K;
                headPos.z += BOAT_NO_DRV_Z;

                if (veh->GetModelIndex() == 0x1AE) { // Vortex
                    headPos += right * VORTEX_RIGHT_K;
                    const auto v = driver->GetMatrix().TransformVector(VORTEX_SPEC_R);
                    headPos += VORTEX_SPEC_V + v;
                    headPos.z += VORTEX_SPEC_C * VORTEX_SPEC_SCL;
                }
                m_vecSource = headPos;
            }
        } else if (veh->m_nVehicleSubType == VEHICLE_TYPE_BIKE) {
            if (!veh->m_matrix) {
                veh->AllocateMatrix();
                veh->UpdateRwMatrix();
            }
            const auto vehRight = veh->m_matrix->GetRight();
            m_vecFront = CrossProduct(m_vecUp, vehRight);
            m_vecFront.Normalise();

            const auto k = (OFFS_X_K - OFFS_X_K2) + (veh->GetColModel()->m_boundBox.m_vecMin.x - OFFS_X_K);
            m_vecSource  = veh->GetPosition();
            m_vecSource += vehRight * k;
            m_vecSource += m_vecFront * -2.3f;
            m_vecSource += m_vecUp * 0.3f;
        } else {
            if (veh->m_nVehicleSubType == VEHICLE_TYPE_TRAIN
                && DotProduct(m_vecFront, veh->m_vecMoveSpeed) < TRAIN_DOT
            ) {
                m_vecFront = -m_vecFront;
            }
            right = CrossProduct(m_vecUp, CVector{ 0.0f, 0.0f, 1.0f });
            right.Normalise();
            up2 = CrossProduct(right, m_vecUp);
            up2.Normalise();
        }
    } else {
        right = CrossProduct(m_vecUp, CVector{ 0.0f, 0.0f, 1.0f });
        right.Normalise();
        up2 = CrossProduct(right, m_vecUp);
        up2.Normalise();
    }

    if (float wl{}; CWaterLevel::GetWaterLevel(m_vecSource, wl, true) && m_vecSource.z < wl - WATER_MARGIN) {
        // underwater -> set up motion blur (same as Process_AttachedCam)
        TheCamera.m_nBlurRed    = static_cast<uint32>(CTimeCycle::GetWaterRed());
        TheCamera.m_nBlurGreen  = static_cast<uint32>(CTimeCycle::GetWaterGreen());
        TheCamera.m_nBlurBlue   = static_cast<uint32>(CTimeCycle::GetWaterBlue());
        TheCamera.m_nBlurType   = eMotionBlurType::LIGHT_SCENE;
        TheCamera.m_nMotionBlur = 20; // 0x8CC7A4
    }

    // wobble the up vector
    const auto t = (float)(CTimer::GetTimeInMS() & 0x1FFFF);
    const auto a = std::cos(t * WHEEL_FREQ) * WHEEL_AMP;
    m_vecUp = up2 * std::cos(a) + right * std::sin(a);
    m_vecUp.Normalise();

    CWorld::pIgnoreEntity = target;
    CColPoint cp{};
    CEntity*  hitEnt{};
    const auto hit = CWorld::ProcessLineOfSight(m_vecSource, target->GetPosition(), cp, hitEnt, true, false, false, true, false, false, true, false);
    CWorld::pIgnoreEntity = nullptr;
    return !hit;
}

// based on 0x51847C - 0x5184EC
void CCam::ApplyUnderwaterMotionBlur() {
    static constexpr uint32 UNDERWATER_CAM_BLUR      = 20;    // 0x8CC7A4
    static constexpr float  UNDERWATER_CAM_MAG_LIMIT = 10.0f; // 0x8CC7A8

    const auto colorMag = std::sqrt(
        sq(CTimeCycle::GetWaterRed()) +
        sq(CTimeCycle::GetWaterGreen()) +
        sq(CTimeCycle::GetWaterBlue())
    );

    const auto factor = (colorMag <= UNDERWATER_CAM_MAG_LIMIT) ? 1.0f : UNDERWATER_CAM_MAG_LIMIT / colorMag;

    TheCamera.SetMotionBlur(
        static_cast<uint32>(factor * CTimeCycle::GetWaterRed()),
        static_cast<uint32>(factor * CTimeCycle::GetWaterGreen()),
        static_cast<uint32>(factor * CTimeCycle::GetWaterBlue()),
        UNDERWATER_CAM_BLUR,
        eMotionBlurType::LIGHT_SCENE
    );
}

// 0x4D58A0
int32 ConvertPedNode2BoneTag(int32 simpleId) {
    const auto map = notsa::make_mapping<int32, int32>({
        { 1,  3 },
        { 2,  5 },
        { 3,  32},
        { 4,  22},
        { 5,  34},
        { 6,  24},
        { 7,  41},
        { 8,  51},
        { 9,  43},
        { 10, 53},
        { 11, 52},
        { 12, 42},
        { 13, 33},
        { 14, 23},
        { 15, 31},
        { 16, 21},
        { 17, 4 },
        { 18, 8 },
    });
    return notsa::find_value_or(map, simpleId, -1);
}

// 0x509A30
bool IsLampPost(eModelID modelId) {
    using namespace ModelIndices;

    return notsa::contains<eModelID>(
        {
            MI_SINGLESTREETLIGHTS1,
            MI_SINGLESTREETLIGHTS2,
            MI_SINGLESTREETLIGHTS3,
            MI_BOLLARDLIGHT,
            MI_MLAMPPOST,
            MI_STREETLAMP1,
            MI_STREETLAMP2,
            MI_TELPOLE02,
            MI_TRAFFICLIGHTS_MIAMI,
            MI_TRAFFICLIGHTS_TWOVERTICAL,
            MI_TRAFFICLIGHTS_3,
            MI_TRAFFICLIGHTS_4,
            MI_TRAFFICLIGHTS_GAY,
            MI_TRAFFICLIGHTS_5,
        },
        modelId
    );
}
