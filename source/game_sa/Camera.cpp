#include "StdInc.h"

#include "Camera.h"

#include "TaskSimpleGangDriveBy.h"
#include "TaskSimpleHoldEntity.h"
#include "TaskSimpleDuck.h"
#include "Hud.h"

auto& TheCamera = StaticRef<CCamera>(0xB6F028);
auto& gbModelViewer = StaticRef<bool>(0xBA6728);
auto& gbCineyCamMessageDisplayed = StaticRef<int8>(0x8CC381); // 2
auto& gCameraDirection = StaticRef<int32>(0x8CC384);         // 3
auto& gCameraMode = StaticRef<eCamMode>(0x8CC388);        // -1
auto& gLastTime2PlayerCameraWasOK = StaticRef<uint32>(0xB6EC24);    // 0
auto& gLastTime2PlayerCameraCollided = StaticRef<uint32>(0xB6EC28); // 0
auto& gPlayerPedVisible = StaticRef<bool>(0x8CC380); // true
auto& gCurCamColVars = StaticRef<uint8>(0x8CCB80);
auto& gCurDistForCam = StaticRef<float>(0x8CCB84);
auto& gpCamColVars = StaticRef<float*>(0xB6FE88);
auto& gCamColVars = StaticRef<float[28][6]>(0x8CC8E0);

// Original .rdata constants used by the Process{FOV,VectorMove,VectorTrack}Lerp helpers.
// Kept as our own storage so they compile to memory operands just like in the original.
static const float ORIG_F180    = 180.0f;        // 0x85A994
static const float ORIG_F270    = 270.0f;        // 0x859070
static const float ORIG_DEG2RAD = 0.017453292f;  // 0x8595EC
static const float ORIG_F1      = 1.0f;          // 0x858624
static const float ORIG_F0_5    = 0.5f;          // 0x858B8C
static const float ORIG_F0_3    = 0.3f;          // 0x858C24
static const float ORIG_F0_25   = 0.25f;         // 0x8CCC84

CCam& CCamera::GetActiveCamera() {
    return TheCamera.m_aCams[TheCamera.m_nActiveCam];
}

void CCamera::InjectHooks() {
    RH_ScopedClass(CCamera);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(GetArrPosForVehicleType, 0x50AF00);
    RH_ScopedInstall(GetPositionAlongSpline, 0x50AF80);
    RH_ScopedInstall(GetRoughDistanceToGround, 0x516B00);
    RH_ScopedInstall(InitialiseCameraForDebugMode, 0x50AF90);
    RH_ScopedInstall(ProcessObbeCinemaCameraPed, 0x50B880, { .reversed = false });
    RH_ScopedInstall(ProcessWideScreenOn, 0x50B890);
    RH_ScopedInstall(RenderMotionBlur, 0x50B8F0);
    RH_ScopedInstall(SetCameraDirectlyBehindForFollowPed_CamOnAString, 0x50BD40);
    RH_ScopedInstall(SetCameraDirectlyInFrontForFollowPed_CamOnAString, 0x50BD70);
    RH_ScopedInstall(SetCamPositionForFixedMode, 0x50BEC0);
    RH_ScopedInstall(SetFadeColour, 0x50BF00);
    RH_ScopedInstall(SetMotionBlur, 0x50BF40);
    RH_ScopedInstall(SetMotionBlurAlpha, 0x50BF80);
    RH_ScopedInstall(SetNearClipScript, 0x50BF90);
    RH_ScopedInstall(SetNewPlayerWeaponMode, 0x50BFB0);
    RH_ScopedInstall(SetRwCamera, 0x50C100);
    RH_ScopedInstall(SetWideScreenOn, 0x50C140);
    RH_ScopedInstall(SetWideScreenOff, 0x50C150);
    RH_ScopedInstall(StartCooperativeCamMode, 0x50C260);
    RH_ScopedInstall(StopCooperativeCamMode, 0x50C270);
    RH_ScopedInstall(AllowShootingWith2PlayersInCar, 0x50C280);
    RH_ScopedInstall(StoreValuesDuringInterPol, 0x50C290);
    RH_ScopedInstall(ProcessScriptedCommands, 0x516AE0);
    RH_ScopedInstall(FinishCutscene, 0x514950);
    RH_ScopedInstall(LerpFOV, 0x50D280);
    RH_ScopedInstall(UpdateAimingCoors, 0x50CB10);
    RH_ScopedInstall(SetColVarsAimWeapon, 0x50CBF0);
    RH_ScopedInstall(ClearPlayerWeaponMode, 0x50AB10);
    RH_ScopedInstall(DontProcessObbeCinemaCamera, 0x50AB40);
    RH_ScopedInstall(Enable1rstPersonCamCntrlsScript, 0x50AC00);
    RH_ScopedInstall(FindCamFOV, 0x50AD20);
    RH_ScopedInstall(GetFading, 0x50ADE0);
    RH_ScopedInstall(GetFadingDirection, 0x50ADF0);
    RH_ScopedInstall(Get_Just_Switched_Status, 0x50AE10);
    RH_ScopedInstall(GetGameCamPosition, 0x50AE50);

    RH_ScopedInstall(Constructor, 0x51A450, { .reversed = false });
    RH_ScopedInstall(InitCameraVehicleTweaks, 0x50A3B0);
    RH_ScopedInstall(ApplyVehicleCameraTweaks, 0x50A480);
    RH_ScopedInstall(CamShake, 0x50A9F0);
    RH_ScopedInstall(GetScreenRect, 0x50AB50);
    RH_ScopedInstall(Enable1rstPersonWeaponsCamera, 0x50AC10);
    RH_ScopedInstall(Fade, 0x50AC20);
    RH_ScopedInstall(Find3rdPersonQuickAimPitch, 0x50AD40);
    RH_ScopedInstall(GetCutSceneFinishTime, 0x50AD90);
    RH_ScopedInstall(GetScreenFadeStatus, 0x50AE20);
    RH_ScopedInstall(GetLookingLRBFirstPerson, 0x50AE60);
    RH_ScopedInstall(GetLookDirection, 0x50AE90);
    RH_ScopedInstall(GetLookingForwardFirstPerson, 0x50AED0);
    RH_ScopedInstall(CopyCameraMatrixToRWCam, 0x50AFA0);
    RH_ScopedInstall(CalculateMirroredMatrix, 0x50B380);
    RH_ScopedInstall(DealWithMirrorBeforeConstructRenderList, 0x50B510);
    RH_ScopedInstall(ProcessFade, 0x50B5D0);
    RH_ScopedInstall(ProcessMusicFade, 0x50B6D0);
    RH_ScopedInstall(Restore, 0x50B930);
    RH_ScopedInstall(RestoreWithJumpCut, 0x50BAB0);
    RH_ScopedInstall(SetCamCutSceneOffSet, 0x50BD20);
    RH_ScopedInstall(SetCameraDirectlyBehindForFollowPed_ForAPed_CamOnAString, 0x50BDA0);
    RH_ScopedInstall(SetCameraDirectlyInFrontForFollowPed_ForAPed_CamOnAString, 0x50BE30);
    RH_ScopedInstall(Using1stPersonWeaponMode, 0x50BFF0);
    RH_ScopedInstall(SetParametersForScriptInterpolation, 0x50C030);
    RH_ScopedInstall(SetPercentAlongCutScene, 0x50C070);
    RH_ScopedInstall(SetZoomValueFollowPedScript, 0x50C160);
    RH_ScopedInstall(SetZoomValueCamStringScript, 0x50C1B0);
    RH_ScopedInstall(UpdateTargetEntity, 0x50C360);
    RH_ScopedInstall(TakeControl, 0x50C7C0);
    RH_ScopedInstall(TakeControlNoEntity, 0x50C8B0);
    RH_ScopedInstall(TakeControlAttachToEntity, 0x50C910, { .reversed = false });
    RH_ScopedInstall(TakeControlWithSpline, 0x50CAE0);
    RH_ScopedInstall(SetCamCollisionVarDataSet, 0x50CB60);
    RH_ScopedInstall(SetNearClipBasedOnPedCollision, 0x50CB90);
    // forwarder to the RW engine function (not hooked) - registered (namespace
    // less on purpose) so call targets resolve to 0x7EE1D0 in bin_diff
    RH_ScopedGlobalInstall(RwCameraSetNearClipPlane, 0x7EE1D0, { .reversed = false });
    RH_ScopedInstall(SetColVarsPed, 0x50CC50);
    RH_ScopedInstall(SetColVarsVehicle, 0x50CCA0);
    RH_ScopedInstall(StartTransitionWhenNotFinishedInter, 0x515BC0);
    RH_ScopedInstall(StartTransition, 0x515200);
    RH_ScopedInstall(CameraGenericModeSpecialCases, 0x50CD30);
    RH_ScopedInstall(CameraPedModeSpecialCases, 0x50CD80);
    RH_ScopedInstall(CameraPedAimModeSpecialCases, 0x50CDA0);
    RH_ScopedInstall(CameraVehicleModeSpecialCases, 0x50CDE0);
    RH_ScopedInstall(IsExtraEntityToIgnore, 0x50CE80);
    RH_ScopedInstall(ConsiderPedAsDucking, 0x50CEB0);
    RH_ScopedInstall(ResetDuckingSystem, 0x50CEF0);
    RH_ScopedInstall(HandleCameraMotionForDucking, 0x50CFA0);
    RH_ScopedInstall(HandleCameraMotionForDuckingDuringAim, 0x50D090);
    RH_ScopedInstall(VectorMoveLinear, 0x50D160);
    RH_ScopedInstall(VectorTrackLinear, 0x50D1D0);
    RH_ScopedInstall(AddShakeSimple, 0x50D240);
    RH_ScopedInstall(InitialiseScriptableComponents, 0x50D2D0);
    RH_ScopedInstall(DrawBordersForWideScreen, 0x514860);
    RH_ScopedInstall(Find3rdPersonCamTargetVector, 0x514970);
    RH_ScopedInstall(CalculateGroundHeight, 0x514B80);
    RH_ScopedInstall(CalculateFrustumPlanes, 0x514D60);
    RH_ScopedInstall(CalculateDerivedValues, 0x5150E0);
    RH_ScopedInstall(ImproveNearClip, 0x516B20, { .reversed = false });
    RH_ScopedInstall(SetCameraUpForMirror, 0x51A560);
    RH_ScopedInstall(RestoreCameraAfterMirror, 0x51A5A0);
    RH_ScopedInstall(ConeCastCollisionResolve, 0x51A5D0);
    RH_ScopedInstall(TryToStartNewCamMode, 0x51E560, { .reversed = false });
    RH_ScopedInstall(CameraColDetAndReact, 0x520190);
    RH_ScopedInstall(CamControl, 0x527FA0, { .reversed = false });
    RH_ScopedInstall(Process, 0x52B730, { .reversed = false });
    RH_ScopedInstall(DeleteCutSceneCamDataMemory, 0x5B24A0);
    RH_ScopedInstall(LoadPathSplines, 0x5B24D0, { .reversed = false }); // body is a redirect - must not be installed (recursion)
    RH_ScopedInstall(Init, 0x5BC520);

    RH_ScopedOverloadedInstall(ProcessVectorTrackLinear, "0", 0x50D350, void(CCamera::*)(float));
    RH_ScopedOverloadedInstall(ProcessVectorTrackLinear, "1", 0x516440, void(CCamera::*)());
    RH_ScopedOverloadedInstall(ProcessVectorMoveLinear, "0", 0x50D430, void(CCamera::*)(float));
    RH_ScopedOverloadedInstall(ProcessVectorMoveLinear, "1", 0x5164A0, void(CCamera::*)());
    RH_ScopedOverloadedInstall(ProcessFOVLerp, "0", 0x50D510, void(CCamera::*)(float));
    RH_ScopedOverloadedInstall(ProcessFOVLerp, "1", 0x516500, void(CCamera::*)());
    //RH_ScopedOverloadedInstall(ProcessJiggle, "0", 0x516560, { .reversed = false });

    RH_ScopedGlobalInstall(CamShakeNoPos, 0x50A970);
}

CCamera* CCamera::Constructor() { this->CCamera::CCamera(); return this; }

// 0x51A450
CCamera::CCamera() : CPlaceable() {
    m_nShakeType = 1;
    m_bMusicFadedOut = false;
    m_matrix = reinterpret_cast<CMatrixLink*>(&m_mCameraMatrix);
    m_fDuckCamMotionFactor = 0.0f;
    m_fDuckAimCamMotionFactor = 0.0f;

    InitialiseScriptableComponents();
}

// 0x50A870
CCamera::~CCamera() {
    m_matrix = nullptr;
}

// 0x5BC520
void CCamera::Init() {
    // Original zeroes TheCamera+0x18 .. +0xD78 (0x358 dwords) before applying the values below.
    // IMPORTANT: TheCamera is the game's statically allocated object - default member initializers of our
    // CCamera class never run for it, so everything the original sets here must be set explicitly.
    std::memset(reinterpret_cast<uint8*>(this) + 0x18, 0, 0xD60);
    m_pRwCamera = nullptr; // +0x954 (re-set by SetRwCamera afterwards, like the original expects)

    InitialiseScriptableComponents();

    m_bPlayerWasOnBike = false;           // +0x27
    m_b1rstPersonRunCloseToAWall = false; // +0x3E
    m_fPositionAlongSpline = 0.0f;        // +0x140
    m_bCameraJustRestored = false;        // +0x1C

    m_aCams[0].Init();
    m_aCams[1].Init();
    m_aCams[2].Init();

    {
        auto& cam = m_aCams[0];
        cam.m_nMode = MODE_FOLLOWPED;
        cam.m_fTargetCloseInDist = 2.0837801f - 1.85f; // 0x86A46C - 0x86A468
        cam.m_fMinRealGroundDist = 1.85f;
        cam.m_fTargetZoomGroundOne = -0.55f;
        cam.m_fTargetZoomGroundTwo = 1.5f;
        cam.m_fTargetZoomGroundThree = 3.6f;
        cam.m_fTargetZoomOneZExtra = 0.06f;
        cam.m_fTargetZoomTwoZExtra = -0.1f;
        cam.m_fTargetZoomTwoInteriorZExtra = 0.0f;
        cam.m_fTargetZoomThreeZExtra = -0.07f;
        cam.m_fTargetZoomZCloseIn = 0.90040702f;
        cam.m_pCamTargetEntity = nullptr;
        cam.m_fCamBufferedHeight = 0.0f;
        cam.m_fCamBufferedHeightSpeed = 0.0f;
        cam.m_bCamLookingAtVector = false;
        cam.m_fPlayerVelocity = 0.0f;
    }

    {
        auto& cam = m_aCams[1];
        cam.m_nMode = MODE_FOLLOWPED;
        cam.m_pCamTargetEntity = nullptr;
        cam.m_fCamBufferedHeight = 0.0f;
        cam.m_fCamBufferedHeightSpeed = 0.0f;
        cam.m_bCamLookingAtVector = false;
        cam.m_fPlayerVelocity = 0.0f;
    }

    {
        auto& cam = m_aCams[2];
        cam.m_pCamTargetEntity = nullptr;
        cam.m_bCamLookingAtVector = false;
        cam.m_fPlayerVelocity = 0.0f;
    }

    m_bMoveCamToAvoidGeom = false; // +0x2D

    ClearPlayerWeaponMode();

    m_bInATunnelAndABigVehicle = false; // +0x22
    m_pToGarageWeAreIn = nullptr; // +0x81C
    m_pToGarageWeAreInForHackAvoidFirstPerson = nullptr; // +0x820
    m_bPlayerIsInGarage = false; // +0x26
    m_bJustCameOutOfGarage = false; // +0x28
    m_fNearClipScript = 0.9f; // +0x138
    m_bUseNearClipScript = false; // +0x31
    m_bDoingSpecialInterp = false; // +0x4F
    m_bAboveGroundTrainNodesLoaded = false; // +0x18
    m_bBelowGroundTrainNodesLoaded = false; // +0x19

    m_nModeForTwoPlayersSeparateCars = MODE_TWOPLAYER_SEPARATE_CARS; // +0x46
    m_nModeForTwoPlayersSameCarShootingAllowed = MODE_TWOPLAYER_IN_CAR_AND_SHOOTING; // +0x48
    m_nModeForTwoPlayersSameCarShootingNotAllowed = MODE_BEHINDCAR; // +0x4A
    m_nModeForTwoPlayersNotBothInCar = MODE_TWOPLAYER; // +0x4C

    m_bHeadBob = false; // +0x3F
    m_nLongestTimeInMill = 5000; // +0x64
    m_nModeObbeCamIsInForCar = 30; // +0xC34
    m_bIgnoreFadingStuffForMusic = false; // +0x25
    m_bWaitForInterpolToFinish = false; // +0x3A

    m_pTargetEntity = FindPlayerEntity();
    CEntity::SafeRegisterRef(m_pTargetEntity);

    m_bFailedCullZoneTestPreviously = false; // +0x54
    m_bWorldViewerBeingUsed = false; // +0x56
    m_fPlayerExhaustion = 1.0f; // +0x154
    m_fFovForTrain = 70.0f; // +0x130

    if (!FrontEndMenuManager.m_bStartGameLoading) {
        CDraw::FadeValue = 0;
        m_fMouseAccelVertical = notsa::IsFixBugs() ? m_fMouseAccelHorzntl * 0.6f : 0.0015f;
    }

    m_bStartingSpline = false; // +0x33
    m_nTypeOfSwitch = eSwitchType::INTERPOLATION; // +0xC3C - Without this the mode switch logic misbehaves
    m_bUseScriptZoomValuePed = false; // +0x38
    m_bUseScriptZoomValueCar = false; // +0x39
    m_fPedZoomValueScript = 0.0f; // +0xD8
    m_fCarZoomValueScript = 0.0f; // +0xC4
    m_bInATunnelAndABigVehicle = false; // +0x22 (again, like the original)
    m_nModeToGoTo = MODE_FOLLOWPED; // +0xC38 - The original always starts in follow-ped mode. Leaving this
                                    // MODE_NONE causes a bogus jumpcut to MODE_NONE => dead camera until respawn
    m_bJust_Switched = false; // +0x2A
    m_bUseTransitionBeta = false; // +0x37
    m_mCameraMatrix.SetScale(1.0f); // FUN_0059AED0(1.0)
    m_bTargetJustBeenOnTrain = false; // +0x34
    m_bInitialNoNodeStaticsSet = false; // +0x24
    m_nTimeTransitionStart = 0; // +0x78
    m_bIdleOn = false; // +0x21
    m_nTimeWeLeftIdle_StillNoInput = 0; // +0x70
    m_nTimeWeEnteredIdle = 0; // +0x74
    m_fLODDistMultiplier = 1.0f; // +0xF0
    m_bCamDirectlyBehind = false; // +0x1A
    m_bCamDirectlyInFront = false; // +0x1B
    m_nMotionBlur = 0; // +0xA0
    m_bGarageFixedCamPositionSet = false; // +0x4E

    SetMotionBlur(255, 255, 255, 0, eMotionBlurType::NONE);

    m_bCullZoneChecksOn = false; // +0x1E
    m_bFailedCullZoneTestPreviously = false; // +0x54 (again, like the original)
    m_nCheckCullZoneThisNumFrames = 6; // +0xA8
    m_nZoneCullFrameNumWereAt = 0; // +0xAC
    m_fCameraAverageSpeed = 0.0f; // +0x124
    m_fCameraSpeedSoFar = 0.0f; // +0x128
    m_nWorkOutSpeedThisNumFrames = 4; // +0x94
    m_nNumFramesSoFar = 0; // +0x98
    m_bJustInitialized = true; // +0x29
    m_bTransitionState = false; // +0x58
    m_bLookingAtPlayer = true; // +0x2B
    m_bLookingAtVector = false; // +0x2C
    m_fAvoidTheGeometryProbsTimer = 0.0f; // +0x164
    m_nAvoidTheGeometryProbsDirn = 0; // +0x168
    gPlayerPedVisible = 1;
    m_bResetOldMatrix = true; // +0x971
    m_f3rdPersonCHairMultX = 0.53f; // 0xB6EC14
    m_f3rdPersonCHairMultY = 0.4f;  // 0xB6EC10
}

// 0x50A3B0
void CCamera::InitCameraVehicleTweaks() {
    m_fCurrentTweakDistance   = 1.0f;
    m_fCurrentTweakAltitude   = 1.0f;
    m_fCurrentTweakAngle      = 0.0f;
    m_nCurrentTweakModelIndex = -1;

    if (!m_bCameraVehicleTweaksInitialized) {
        for (auto& camTweak : m_aCamTweak) {
            camTweak.ModelID = -1;
            camTweak.Dist   = 1.0f;
            camTweak.Alt   = 1.0f;
            camTweak.Angle      = 0.0f;
        }

        m_aCamTweak[0].ModelID = MODEL_RCGOBLIN;
        m_aCamTweak[0].Dist = 1.0f;
        m_aCamTweak[0].Alt = 1.0f;
        m_aCamTweak[0].Angle    = 0.178997f; // todo: magic number

        m_bCameraVehicleTweaksInitialized = true;
    }
}

// 0x50D2D0
void CCamera::InitialiseScriptableComponents() {
    m_fTrackLinearStartTime    = -1.0f;
    m_fTrackLinearEndTime      = -1.0f;
    m_fStartShakeTime          = -1.0f;
    m_fEndShakeTime            = -1.0f;
    m_fEndZoomTime             = -1.0f;
    m_fStartZoomTime           = -1.0f;
    m_fZoomInFactor            = +0.0f;
    m_fZoomOutFactor           = +0.0f;
    m_bTrackLinearWithEase     = true;
    m_nZoomMode                = 1;
    m_bMoveLinearWithEase      = true;
    m_fMoveLinearStartTime     = -1.0f;
    m_fMoveLinearEndTime       = -1.0f;
    m_bBlockZoom               = false;
    m_bCameraPersistPosition   = false;
    m_bCameraPersistTrack      = false;
    m_bVecTrackLinearProcessed = false;
    m_bVecMoveLinearProcessed  = false;
    m_bFOVLerpProcessed        = false;
}

// 0x50AF90
void CCamera::InitialiseCameraForDebugMode() {
#ifndef FINAL
    if (auto* vehicle = FindPlayerVehicle()) {
        m_aCams[2].m_vecSource = vehicle->GetPosition();
    } else if (auto* player = FindPlayerPed()) {
        m_aCams[2].m_vecSource = player->GetPosition();
    }

    m_aCams[2].m_fTrueAlpha = 0.0f;
    m_aCams[2].m_fTrueBeta  = 0.0f;
    m_aCams[2].m_nMode = eCamMode::MODE_DEBUG;
#endif
}

// 0x50A480
void CCamera::ApplyVehicleCameraTweaks(CVehicle* vehicle) {
    if (vehicle->GetModelIndex() == m_nCurrentTweakModelIndex) {
        return;
    }

    InitCameraVehicleTweaks();
    for (auto& camTweak : m_aCamTweak) {
        if (camTweak.ModelID == vehicle->GetModelIndex()) {
            m_fCurrentTweakDistance = camTweak.Dist;
            m_fCurrentTweakAltitude = camTweak.Alt;
            m_fCurrentTweakAngle    = camTweak.Angle;
            return;
        }
    }
}

// 0x50A9F0
void CCamera::CamShake(float strength, CVector from) {
    auto dist = DistanceBetweenPoints(from, GetActiveCamera().m_vecSource);
    dist = std::clamp(dist, 0.0f, 100.0f);

    float percentShakeForce = 1.0f - dist / 100.f;
    float shakeForce = (m_fCamShakeForce - float(CTimer::GetTimeInMS() - m_nCamShakeStart) / 1000.f) * percentShakeForce;

    float toShakeForce = percentShakeForce * strength * 0.35f;
    if (toShakeForce > std::clamp(shakeForce, 0.0f, 2.0f)) {
        m_fCamShakeForce = toShakeForce;
        m_nCamShakeStart = CTimer::GetTimeInMS();
    }
}

// 0x50A970
void CamShakeNoPos(CCamera* camera, float strength) {
    // NOTE: Mirrors the original structure - explicit clamping to 2.f then to 0.f, and *0.001f (not /1000.f)
    float oldShake = camera->m_fCamShakeForce - float(CTimer::m_snTimeInMilliseconds - camera->m_nCamShakeStart) * 0.001f;

    if (oldShake >= 2.0f) {
        oldShake = 2.0f;
    }

    if (oldShake <= 0.0f) {
        oldShake = 0.0f;
    }

    if (strength > oldShake) {
        camera->m_fCamShakeForce = strength;
        camera->m_nCamShakeStart = CTimer::m_snTimeInMilliseconds;
    }
}

// 0x50AB10
void CCamera::ClearPlayerWeaponMode() {
    m_PlayerWeaponMode.m_nMode = 0;
    m_PlayerWeaponMode.m_nMaxZoom = 1;
    m_PlayerWeaponMode.m_nMinZoom = -1;
    m_PlayerWeaponMode.m_fDuration = 0.0f;
}

// 0x50AB40
void CCamera::DontProcessObbeCinemaCamera() {
    bDidWeProcessAnyCinemaCam = false;
}

// 0x50AC00
void CCamera::Enable1rstPersonCamCntrlsScript() {
    m_bEnable1rstPersonCamCntrlsScript = true;
}

// 0x50AC10
void CCamera::Enable1rstPersonWeaponsCamera() {
    m_bAllow1rstPersonWeaponsCamera = true;
}

// 0x50AC20
void CCamera::Fade(float duration, eFadeFlag direction) {
    m_fFadeDuration = duration;
    m_bFading = true;
    m_nFadeInOutFlag = direction;
    m_nFadeStartTime = CTimer::GetTimeInMS();

    if (m_bIgnoreFadingStuffForMusic && direction != eFadeFlag::FADE_OUT) {
        return;
    }
    m_bMusicFading           = true;
    m_nMusicFadingDirection  = direction;

    m_fTimeToFadeMusic       = std::min(std::max(duration * 0.3f, 0.3f), duration); //Can't use std::clamp there, duration can be bigger or smaller than 0.3f
    m_nFadeTimeStartedMusic  = CTimer::GetTimeInMS();
    m_fTimeToWaitToFadeMusic = direction == eFadeFlag::FADE_IN
        ? duration - m_fTimeToFadeMusic
        : 0.f;
    if (direction == eFadeFlag::FADE_IN) {
        m_fTimeToFadeMusic = std::max(m_fTimeToFadeMusic - 0.1f, 0.f);
    }
}

// 0x50AD20
float CCamera::FindCamFOV() const {
    return m_aCams[m_nActiveCam].m_fFOV;
}

/*!
* @addr 0x50AD40
* @return Rotation in radians at which the gun should point at, relative to the camera's vertical angle
*/
float CCamera::Find3rdPersonQuickAimPitch() const {
    const auto& cam = m_aCams[m_nActiveCam];

    // https://mathworld.wolfram.com/images/eps-svg/SOHCAHTOA_500.svg
    const auto adjacent = (0.5f - m_f3rdPersonCHairMultY) * 2.f;
    const auto opposite = std::tan(DegreesToRadians(cam.m_fFOV / 2.0f)) * adjacent;
    const auto relAngle = cam.m_fVerticalAngle + std::atan(opposite / CDraw::ms_fAspectRatio);
    return -relAngle; // Flip it
}

// 0x50AD90
uint32 CCamera::GetCutSceneFinishTime() {
    auto& cam = m_aCams[m_nActiveCam];
    if (cam.m_nMode == eCamMode::MODE_FLYBY) {
        return cam.m_nFinishTime;
    }

    cam = m_aCams[(m_nActiveCam + 1) % 2];
    if (cam.m_nMode == eCamMode::MODE_FLYBY) {
        return cam.m_nFinishTime;
    }

    return 0;
}

// 0x50ADE0
bool CCamera::GetFading() const {
    return m_bFading;
}

// TODO: eFadingDirection
// 0x50ADF0
int32 CCamera::GetFadingDirection() const {
    if (m_bFading)
        return m_nFadeInOutFlag == eFadeFlag::FADE_OUT;
    else
        return 2;
}

// 0x50AE10
bool CCamera::Get_Just_Switched_Status() const {
    return m_bJust_Switched;
}

// 0x50AE20
eNameState CCamera::GetScreenFadeStatus() const {
    if (m_fFadeAlpha == 0.0f) {
        return NAME_DONT_SHOW;
    }
    if (m_fFadeAlpha == 255.0f) {
        return NAME_FADE_IN;
    }

    return NAME_SHOW;
}

// 0x50AE50
CVector* CCamera::GetGameCamPosition() {
    return &m_vecGameCamPos;
}

// 0x50AE60
bool CCamera::GetLookingLRBFirstPerson() const {
    return m_aCams[m_nActiveCam].m_nMode == eCamMode::MODE_1STPERSON
        && m_aCams[m_nActiveCam].m_nDirectionWasLooking != LOOKING_DIRECTION_FORWARD;
}

// 0x50AED0
bool CCamera::GetLookingForwardFirstPerson() const {
    return m_aCams[m_nActiveCam].m_nMode == eCamMode::MODE_1STPERSON
        && m_aCams[m_nActiveCam].m_nDirectionWasLooking == LOOKING_DIRECTION_FORWARD;
}

// 0x50AE90
int32 CCamera::GetLookDirection() const {
    const auto& cam = m_aCams[m_nActiveCam];
    if (cam.m_nMode != eCamMode::MODE_CAM_ON_A_STRING &&
        cam.m_nMode != eCamMode::MODE_1STPERSON &&
        cam.m_nMode != eCamMode::MODE_BEHINDBOAT &&
        cam.m_nMode != eCamMode::MODE_FOLLOWPED ||
        (cam.m_nDirectionWasLooking == LOOKING_DIRECTION_FORWARD)
    ) {
        return LOOKING_DIRECTION_FORWARD;
    }

    return cam.m_nDirectionWasLooking; // todo: unsigned/signed
}

// 0x50AF00
bool CCamera::GetArrPosForVehicleType(eVehicleType type, int32& arrPos) {
    switch (type) {
    case VEHICLE_TYPE_MTRUCK:
        arrPos = 0;
        return true;
    case VEHICLE_TYPE_QUAD:
        arrPos = 1;
        return true;
    case VEHICLE_TYPE_HELI:
        arrPos = 2;
        return true;
    case VEHICLE_TYPE_PLANE:
        arrPos = 4;
        return true;
    case VEHICLE_TYPE_BOAT:
        arrPos = 3;
        return true;
    default:
        return false;
    }
}

// 0x50AF80
float CCamera::GetPositionAlongSpline() const {
    return m_fPositionAlongSpline;
}

// 0x516B00
float CCamera::GetRoughDistanceToGround() {
    return m_aCams[m_nActiveCam].m_vecSource.z - CalculateGroundHeight(eGroundHeightType::ENTITY_BB_BOTTOM);
}

// 0x50AFA0
// Member offsets as used by the original (TheCamera = 0xB6F028):
//   m_pRwCamera +0x954, m_bResetOldMatrix +0x971, m_mCameraMatrix +0x974, m_mCameraMatrixOld +0x9BC.
static_assert(offsetof(CCamera, m_pRwCamera) == 0x954);
static_assert(offsetof(CCamera, m_bResetOldMatrix) == 0x971);
static_assert(offsetof(CCamera, m_mCameraMatrix) == 0x974);
static_assert(offsetof(CCamera, m_mCameraMatrixOld) == 0x9BC);
void CCamera::CopyCameraMatrixToRWCam(bool bUpdateMatrix) {
    // Frame matrix of the RW camera
    RwFrame* const frame = RwCameraGetFrame(m_pRwCamera); // *(RwCamera**)0x954 + 4 in the original
    RwMatrix&      rw    = *RwFrameGetMatrix(frame);      // frame + 0x10

    if (!bUpdateMatrix) {
        // NOTE: the original (0x50AFA0, LEA ECX,[EDI+0x9BC]) updates the OLD matrix here -
        // the rw matrix still holds last frame's rendered matrix at this point, so
        // m_mCameraMatrixOld tracks it for the interpolation code. It must NOT be written
        // into m_mCameraMatrix (+0x974): Process composes the fresh camera matrix there
        // right before calling this, and overwriting it would feed the stale matrix
        // back into the renderer every frame.
        m_mCameraMatrixOld.UpdateMatrix(&rw); // FUN_0059AD20
    }

    // SA (Z-up) -> RW (Y-up) mapping
    rw.right = reinterpret_cast<const RwV3d&>(m_mCameraMatrix.GetRight());
    rw.up    = reinterpret_cast<const RwV3d&>(m_mCameraMatrix.GetUp());
    rw.at    = reinterpret_cast<const RwV3d&>(m_mCameraMatrix.GetForward());
    rw.pos   = reinterpret_cast<const RwV3d&>(m_mCameraMatrix.GetPosition());

    // Anti-jitter: snap each row to its previous value if it barely moved
    static auto& rowFlags   = StaticRef<uint32>(0xB6FFC0);
    static auto& lastRight  = StaticRef<CVector>(0xB6FF90);
    static auto& lastUp     = StaticRef<CVector>(0xB6FF9C);
    static auto& lastAt     = StaticRef<CVector>(0xB6FFA8);
    static auto& lastPos    = StaticRef<CVector>(0xB6FFB4);
    const float  epsRow     = StaticRef<float>(0x8CCC78); // 1e-5f
    const float  epsPos     = StaticRef<float>(0x8CCC7C); // 1e-5f

    // NOTE: the original squares the epsilon constant (FMUL [0x8CCC78] x [0x8CCC78],
    // FMUL [0x8CCC7C] x [0x8CCC7C]) before comparing it with the squared distance,
    // i.e. the snap threshold is 1e-5 units - NOT 1e-5 units squared. Comparing against
    // the raw constant instead makes the snap 316x more aggressive and the rendered
    // matrix oscillates hold/step at the threshold - visible as per-frame flicker.
    const auto smoothRow = [flags = &rowFlags](uint32 flag, CVector& row, CVector& last, float eps) {
        if ((*flags & flag) == 0) {
            *flags |= flag;
            last = CVector{ -99999.0f, -99999.0f, -99999.0f };
        }
        if ((last - row).SquaredMagnitude() < eps * eps) {
            row = last;
        }
        last = row;
    };
    smoothRow(0x1, reinterpret_cast<CVector&>(rw.pos),   lastPos,  epsPos);
    smoothRow(0x2, reinterpret_cast<CVector&>(rw.at),    lastAt,   epsRow);
    smoothRow(0x4, reinterpret_cast<CVector&>(rw.up),    lastUp,   epsRow);
    smoothRow(0x8, reinterpret_cast<CVector&>(rw.right), lastRight, epsRow);

    RwMatrixUpdate(&rw);           // FUN_007F18A0
    RwFrameUpdateObjects(frame);   // FUN_007F0910
    plugin::Call<0x7F1170, RwFrame*>(frame); // FUN_007F1170 - RenderWare frame update helper (unreversed here)

    if (m_bResetOldMatrix && !bUpdateMatrix) {
        m_mCameraMatrixOld.UpdateMatrix(&rw); // FUN_0059AD20
        m_bResetOldMatrix = false;
    }
}

// 0x50B380
void CCamera::CalculateMirroredMatrix(CVector posn, float mirrorV, CMatrix *camMatrix, CMatrix* mirrorMatrix) {
    mirrorMatrix->GetPosition() = camMatrix->GetPosition() - posn * 2 * (DotProduct(posn, camMatrix->GetPosition()) - mirrorV);

    const CVector fwd = camMatrix->GetForward() - posn * 2 * DotProduct(posn, camMatrix->GetForward());
    mirrorMatrix->GetForward() = fwd;

    const CVector up = camMatrix->GetUp() - posn * 2 * DotProduct(posn, camMatrix->GetUp());
    mirrorMatrix->GetUp() = up;

    mirrorMatrix->GetRight() = CVector{
        up.y * fwd.z - up.z * fwd.y,
        up.z * fwd.x - up.x * fwd.z,
        up.x * fwd.y - up.y * fwd.x
    };
}

// 0x50B510
void CCamera::DealWithMirrorBeforeConstructRenderList(bool bActiveMirror, CVector mirrorNormal, float mirrorV, CMatrix* matMirror) {
    m_bMirrorActive = bActiveMirror;

    if (!bActiveMirror)
        return;

    if (matMirror)
        m_mMatMirror = *matMirror;
    else
        CalculateMirroredMatrix(mirrorNormal, mirrorV, &m_mCameraMatrix, &m_mMatMirror);

    m_mMatMirrorInverse = Invert(m_mMatMirror);
}

/// III/VC leftover
// 0x50B8F0
void CCamera::RenderMotionBlur() const {
    ZoneScoped;

    if (m_nBlurType != eMotionBlurType::NONE) {
        // CMBlur::MotionBlurRender(); // todo: Add CMBlur::MotionBlurRender is NOP, 0x71D700
    }
}

// 0x50B930
void CCamera::Restore() {
    m_bLookingAtPlayer = true;
    m_bLookingAtVector = false;
    m_nTypeOfSwitch = eSwitchType::INTERPOLATION;
    m_bUseNearClipScript = false;
    m_nModeObbeCamIsInForCar = 30;
    m_fPositionAlongSpline = 0.0f;
    m_bStartingSpline = false;
    m_bScriptParametersSetForInterp = false;
    m_nWhoIsInControlOfTheCamera = 0;

    CVehicle* vehicle = FindPlayerVehicle();
    CPlayerPed* player = FindPlayerPed();

    if (vehicle) {
        m_nModeToGoTo = MODE_CAM_ON_A_STRING;
        CEntity::SafeCleanUpRef(m_pTargetEntity);
        m_pTargetEntity = vehicle;
    } else {
        m_nModeToGoTo = MODE_FOLLOWPED;
        CEntity::SafeCleanUpRef(m_pTargetEntity);
        m_pTargetEntity = player;
    }
    CEntity::SafeRegisterRef(m_pTargetEntity);

    switch (player->m_nPedState) {
    case PEDSTATE_ENTER_CAR:
    case PEDSTATE_CARJACK:
    case PEDSTATE_OPEN_DOOR:
        m_nModeToGoTo = MODE_CAM_ON_A_STRING;
        break;
    }

    if (player->m_nPedState == PEDSTATE_EXIT_CAR) {
        m_nModeToGoTo = MODE_FOLLOWPED;

        CEntity::SafeCleanUpRef(m_pTargetEntity);
        m_pTargetEntity = player;
        CEntity::SafeRegisterRef(m_pTargetEntity);
    }

    CEntity::ClearReference(m_pAttachedEntity);

    m_bEnable1rstPersonCamCntrlsScript = false;
    m_bAllow1rstPersonWeaponsCamera = false;
    m_bUseScriptZoomValuePed = false;
    m_bUseScriptZoomValueCar = false;
    m_fAvoidTheGeometryProbsTimer = 0.0f;
    m_bStartInterScript = true;
    m_bCameraJustRestored = true;
}

// 0x50BAB0
void CCamera::RestoreWithJumpCut() {
    m_bRestoreByJumpCut = true;
    m_bLookingAtPlayer = true;
    m_bLookingAtVector = false;
    m_nTypeOfSwitch = eSwitchType::JUMPCUT;
    m_nWhoIsInControlOfTheCamera = 0;
    m_fPositionAlongSpline = 0.0f;
    m_bStartingSpline = false;
    m_bUseNearClipScript = false;
    m_nModeObbeCamIsInForCar = 30;
    m_bScriptParametersSetForInterp = false;

    CVehicle* vehicle = FindPlayerVehicle();
    CPlayerPed* player = FindPlayerPed();

    if (vehicle) {
        m_nModeToGoTo = MODE_CAM_ON_A_STRING;
        CEntity::SafeCleanUpRef(m_pTargetEntity);
        m_pTargetEntity = vehicle;
    } else {
        m_nModeToGoTo = MODE_FOLLOWPED;
        CEntity::SafeCleanUpRef(m_pTargetEntity);
        m_pTargetEntity = player;
    }
    CEntity::SafeRegisterRef(m_pTargetEntity);

    switch (player->m_nPedState) {
    case PEDSTATE_ENTER_CAR:
    case PEDSTATE_CARJACK:
    case PEDSTATE_OPEN_DOOR:
        m_nModeToGoTo = MODE_CAM_ON_A_STRING;
        break;
    }

    if (player->m_nPedState == PEDSTATE_EXIT_CAR) {
        m_nModeToGoTo = MODE_FOLLOWPED;

        CEntity::SafeCleanUpRef(m_pTargetEntity);
        m_pTargetEntity = player;
        CEntity::SafeRegisterRef(m_pTargetEntity);
    }

    if (!m_bCooperativeCamMode) {
        m_bUseScriptZoomValuePed = false;
        m_bUseScriptZoomValueCar = false;
        return;
    }

    CPlayerPed* player0 = FindPlayerPed(0);
    CPlayerPed* player1 = FindPlayerPed(1);

    if (!player0) {
        m_bUseScriptZoomValuePed = false;
        m_bUseScriptZoomValueCar = false;
        return;
    }

    if (!player1) {
        m_bUseScriptZoomValuePed = false;
        m_bUseScriptZoomValueCar = false;
        return;
    }

    CEntity::SafeCleanUpRef(m_pTargetEntity);

    if (!player0->IsInVehicle() || !player1->IsInVehicle()) {
        m_nModeToGoTo = m_nModeForTwoPlayersNotBothInCar;
        m_pTargetEntity = player0;
        CEntity::SafeRegisterRef(m_pTargetEntity);

        m_bUseScriptZoomValuePed = false;
        m_bUseScriptZoomValueCar = false;
        return;
    }

    if (player0->m_pVehicle == player1->m_pVehicle) {
        if (m_bAllowShootingWith2PlayersInCar) {
            m_nModeToGoTo = m_nModeForTwoPlayersSameCarShootingAllowed;
        } else {
            m_nModeToGoTo = m_nModeForTwoPlayersSameCarShootingNotAllowed;
        }
    } else {
        m_nModeToGoTo = m_nModeForTwoPlayersSeparateCars;
    }

    m_pTargetEntity = player0->m_pVehicle;
    CEntity::SafeRegisterRef(m_pTargetEntity);

    m_bUseScriptZoomValuePed = false;
    m_bUseScriptZoomValueCar = false;
}

// 0x50BD20
void CCamera::SetCamCutSceneOffSet(const CVector& offset) {
    m_vecCutSceneOffset = offset;
}

// 0x50BD40
void CCamera::SetCameraDirectlyBehindForFollowPed_CamOnAString() {
    m_bCamDirectlyBehind = true;
    CPed* player = FindPlayerPed();
    if (player) {
        m_fPedOrientForBehindOrInFront = CGeneral::GetATanOfXY(player->GetForward().x, player->GetForward().y);
    }
}

// 0x50BD70
void CCamera::SetCameraDirectlyInFrontForFollowPed_CamOnAString() {
    m_bCamDirectlyInFront = true;
    CPed* player = FindPlayerPed();
    if (player != nullptr) {
        m_fPedOrientForBehindOrInFront = CGeneral::GetATanOfXY(player->GetForward().x, player->GetForward().y);
    }
}

// unused
// 0x50BDA0
void CCamera::SetCameraDirectlyBehindForFollowPed_ForAPed_CamOnAString(CPed* targetPed) {
    if (!targetPed) {
        return;
    }

    m_bCamDirectlyBehind = true;
    m_bLookingAtPlayer = false;

    TheCamera.m_pTargetEntity = targetPed;
    CEntity::ChangeEntityReference(GetActiveCamera().m_pCamTargetEntity, targetPed);
    m_fPedOrientForBehindOrInFront = targetPed->GetHeading();
}

// 0x50BE30
void CCamera::SetCameraDirectlyInFrontForFollowPed_ForAPed_CamOnAString(CPed* targetPed) {
    if (!targetPed) {
        return;
    }

    m_bLookingAtPlayer = false;
    m_pTargetEntity = targetPed;

    CCam& camera = GetActiveCamera();
    CEntity::SafeCleanUpRef(camera.m_pCamTargetEntity);

    camera.m_pCamTargetEntity = targetPed;
    camera.m_pCamTargetEntity->RegisterReference(camera.m_pCamTargetEntity);

    m_bCamDirectlyInFront = true;
    m_fPedOrientForBehindOrInFront = CGeneral::GetATanOfXY(targetPed->GetForward().x, targetPed->GetForward().y);
}

// 0x50BEC0
void CCamera::SetCamPositionForFixedMode(const CVector& fixedModeSource, const CVector& fixedModeUpOffset) {
    m_vecFixedModeSource = fixedModeSource;
    m_vecFixedModeUpOffSet = fixedModeUpOffset;
    m_bGarageFixedCamPositionSet = false;
}

// 0x50BF00
void CCamera::SetFadeColour(uint8 red, uint8 green, uint8 blue) {
    m_bFadeTargetIsSplashScreen = false;
    if (red == 2 && green == 2 && blue == 2) {
        m_bFadeTargetIsSplashScreen = true;
    }

    CDraw::FadeRed   = red;
    CDraw::FadeGreen = green;
    CDraw::FadeBlue  = blue;
}

// 0x50BF40
void CCamera::SetMotionBlur(uint8 red, uint8 green, uint8 blue, int32 value, eMotionBlurType blurType) {
    m_nBlurRed    = red;
    m_nBlurGreen  = green;
    m_nBlurBlue   = blue;
    m_nBlurType   = blurType;
    m_nMotionBlur = value;
}

// 0x50BF80
void CCamera::SetMotionBlurAlpha(int32 alpha) {
    m_nMotionBlurAddAlpha = alpha;
}

// 0x50BF90
void CCamera::SetNearClipScript(float nearClip) {
    m_fNearClipScript = nearClip;
    m_bUseNearClipScript = true;
}

// 0x50BFB0
void CCamera::SetNewPlayerWeaponMode(eCamMode mode, int16 minZoom, int16 maxZoom) {
    m_PlayerWeaponMode.m_nMode     = mode;
    m_PlayerWeaponMode.m_nMinZoom  = minZoom;
    m_PlayerWeaponMode.m_nMaxZoom  = maxZoom;
    m_PlayerWeaponMode.m_fDuration = 0.0f;
}

// 0x50BFF0
bool CCamera::Using1stPersonWeaponMode() const {
    switch (m_PlayerWeaponMode.m_nMode) {
    case MODE_SNIPER:
    case MODE_M16_1STPERSON:
    case MODE_ROCKETLAUNCHER:
    case MODE_ROCKETLAUNCHER_HS:
    case MODE_HELICANNON_1STPERSON:
    case MODE_CAMERA:
    case MODE_AIMWEAPON_ATTACHED:
        return true;
    default:
        return false;
    }
}

// 0x50C030
void CCamera::SetParametersForScriptInterpolation(float interpolationToStopMoving, float interpolationToCatchUp, uint32 timeForInterpolation) {
    m_nScriptTimeForInterpolation = timeForInterpolation;
    m_bScriptParametersSetForInterp = true;
    m_fScriptPercentageInterToStopMoving = interpolationToStopMoving / 100.0f;
    m_fScriptPercentageInterToCatchUp = interpolationToCatchUp / 100.0f;
}

// 0x50C070
void CCamera::SetPercentAlongCutScene(float percent) {
    auto& cam = m_aCams[m_nActiveCam];
    if (cam.m_nMode == eCamMode::MODE_FLYBY) {
        cam.m_fTimeElapsedFloat = (float)cam.m_nFinishTime * percent / 100.0f;
        return;
    }

    cam = m_aCams[(m_nActiveCam + 1) % 2];
    if (cam.m_nMode == eCamMode::MODE_FLYBY) {
        cam.m_fTimeElapsedFloat = (float)cam.m_nFinishTime * percent / 100.0f;
        return;
    }
}

// 0x50C100
void CCamera::SetRwCamera(RwCamera* camera) {
    m_pRwCamera = camera;
    m_mViewMatrix.Attach(&camera->viewMatrix, false);
}

// 0x50C140
void CCamera::SetWideScreenOn() {
    m_bWideScreenOn = true;
    m_bWantsToSwitchWidescreenOff = false;
}

// 0x50C150
void CCamera::SetWideScreenOff() {
    m_bWantsToSwitchWidescreenOff = m_bWideScreenOn;
}

// 0x50C160
void CCamera::SetZoomValueFollowPedScript(int16 zoomMode) {
    switch (zoomMode) {
    case 1:
        m_fPedZoomValueScript = 1.50f;
        break;
    case 2:
        m_fPedZoomValueScript = 2.90f;
        break;
    default:
        m_fPedZoomValueScript = 0.25f;
    }
    m_bUseScriptZoomValuePed = true;
}

// zoomMode : 0- ZOOM_ONE , 1- ZOOM_TWO , 2- ZOOM_THREE
// 0x50C1B0
void CCamera::SetZoomValueCamStringScript(int16 zoomMode) {
    auto entity = m_aCams[0].m_pCamTargetEntity;

    if (entity->GetStatus() == STATUS_SIMPLE) {
        int32 arrPos{};
        VERIFY(GetArrPosForVehicleType(static_cast<eVehicleType>(entity->AsVehicle()->GetVehicleAppearance()), arrPos));
        m_fCarZoomValueScript = [zoomMode]{
            switch (zoomMode) {
            case 0:
                return std::array{ -1.0f, -0.2f, -3.20f, 0.05f, -2.41f }; // 0x8CC3E0
            case 1:
                return std::array{ +1.0f, +1.4f, +0.65f, 1.90f, +6.49f }; // 0x8CC3F4
            case 2:
                return std::array{ +6.0f, +6.0f, +15.9f, 15.9f, +15.0f }; // 0x8CC408
            default:
                NOTSA_UNREACHABLE("Unexpected zoom mode: {}", zoomMode);
            }
        }()[arrPos];
    
        m_bUseScriptZoomValueCar = true;
    } else {
        SetZoomValueFollowPedScript(zoomMode);
    }
}

// 0x50C260
void CCamera::StartCooperativeCamMode() {
    m_bCooperativeCamMode = true;
    CGameLogic::n2PlayerPedInFocus = eFocusedPlayer::NONE;
}

// 0x50C270
void CCamera::StopCooperativeCamMode() {
    m_bCooperativeCamMode = false;
    CGameLogic::n2PlayerPedInFocus = eFocusedPlayer::NONE;
}

// 0x50C280
void CCamera::AllowShootingWith2PlayersInCar(bool bAllow) {
    m_bAllowShootingWith2PlayersInCar = bAllow;
}

// 0x50C290
void CCamera::StoreValuesDuringInterPol(CVector* sourceDuringInter, CVector* targetDuringInter, CVector* upDuringInter, float* FOVDuringInter) {
    m_vecSourceDuringInter = *sourceDuringInter;
    m_vecTargetDuringInter = *targetDuringInter;
    m_vecUpDuringInter     = *upDuringInter;
    m_fFOVDuringInter      = *FOVDuringInter;

    auto dist = *sourceDuringInter - m_vecTargetDuringInter;
    m_fBetaDuringInterPol = CGeneral::GetATanOfXY(dist.x, dist.y);

    float distOnGround = dist.Magnitude2D();
    m_fAlphaDuringInterPol = CGeneral::GetATanOfXY(distOnGround, dist.z);
}

// 0x50C360
void CCamera::UpdateTargetEntity() {
    m_bPlayerWasOnBike = m_pTargetEntity && m_pTargetEntity->GetIsTypeVehicle() && m_pTargetEntity->AsVehicle()->m_vecMoveSpeed.SquaredMagnitude() > 0.3f;

    const auto player = FindPlayerPed();
    assert(player);

    auto something{ true };
    if (m_nWhoIsInControlOfTheCamera == 2) {
        m_nModeObbeCamIsInForCar = m_nModeObbeCamIsInForCar;
        switch (m_nModeObbeCamIsInForCar) {
        case 8:
        case 7: {
            if (player->m_nPedState != PEDSTATE_ARRESTED) {
                something = false;
            }

            if (!FindPlayerVehicle()) {
                CEntity::ChangeEntityReference(m_pTargetEntity, player);
            }

            break;
        }
        }
    }

    if (!m_bLookingAtPlayer && !something || m_bTransitionState) {
        if (m_pTargetEntity) {
            if (!m_bTargetJustBeenOnTrain) {
                return;
            }
        }
        
    }

    bool playerDoingSomethingWhileDriveBy{};
    if ([&, this]() { // Check is player doing drive-by
        if (!FindPlayerVehicle()) {
            return true;
        }

        if (!CGameLogic::IsCoopGameGoingOn()) {
            if (player->GetTaskManager().GetSimplestActiveTaskAs<CTaskSimpleGangDriveBy>()) {
                return true;
            }
        }

        return false;
    }()) {
        CEntity::ChangeEntityReference(m_pTargetEntity, player);

        playerDoingSomethingWhileDriveBy = [this, player] {
            switch (player->m_nPedState) {
            case PEDSTATE_ENTER_CAR:
            case PEDSTATE_CARJACK:
            case PEDSTATE_OPEN_DOOR:
                return true;
            }
            return false;
        }();

        if (!playerDoingSomethingWhileDriveBy) {
            auto& cam = GetActiveCam();
            if (cam.m_pCamTargetEntity != m_pTargetEntity) {
                CEntity::ChangeEntityReference(cam.m_pCamTargetEntity, m_pTargetEntity);
            }
        }
    } else {
        CEntity::ChangeEntityReference(m_pTargetEntity, FindPlayerVehicle());
    }

    const auto canEnterCar = player && player->m_pVehicle && player->m_pVehicle->CanPedOpenLocks(player); // Inverted this variable

    if (canEnterCar && player->m_nPedState == PEDSTATE_ENTER_CAR && !playerDoingSomethingWhileDriveBy) {
        if (m_nCarZoom) {
            CEntity::ChangeEntityReference(m_pTargetEntity, FindPlayerEntity());
        }
    }

    if (canEnterCar) {
        switch (player->m_nPedState) {
        case PEDSTATE_CARJACK:
        case PEDSTATE_OPEN_DOOR: {
            if (!playerDoingSomethingWhileDriveBy) {
                if (m_nCarZoom) {
                    CEntity::ChangeEntityReference(m_pTargetEntity, FindPlayerEntity());
                }
            }

            if (!FindPlayerVehicle()) {
                CEntity::ChangeEntityReference(m_pTargetEntity, player);
            }
        }
        }
    }

    switch (player->m_nPedState) {
    case PEDSTATE_EXIT_CAR:
    case PEDSTATE_DRAGGED_FROM_CAR:
        CEntity::ChangeEntityReference(m_pTargetEntity, player);
    }

    if (m_pTargetEntity->GetIsTypeVehicle()) {
        if (m_nCarZoom == 0) {
            if (player->m_nPedState == PEDSTATE_ARRESTED) {
                CEntity::ChangeEntityReference(m_pTargetEntity, player);
            }
        }
    }
}

// 0x50C7C0
void CCamera::TakeControl(CEntity* target, eCamMode modeToGoTo, eSwitchType switchType, int32 whoIsInControlOfTheCamera) {
    if (!m_bCinemaCamera) {
        if (whoIsInControlOfTheCamera == 2 && m_nWhoIsInControlOfTheCamera == 1) {
            return;
        }
    }
    m_nWhoIsInControlOfTheCamera = whoIsInControlOfTheCamera;

    const auto [newGoToMode, newTargetEntity] = [&, this]() -> std::tuple<eCamMode, CEntity*>{
        if (target) {
            return {
                [&, this] {
                    if (modeToGoTo == MODE_NONE) {
                        switch (target->GetType()) {
                        case ENTITY_TYPE_PED:
                            return MODE_FOLLOWPED;
                        case ENTITY_TYPE_VEHICLE:
                            return MODE_CAM_ON_A_STRING;
                        }
                    }
                    return modeToGoTo;
                }(),
                target
            };
        }

        return { modeToGoTo, FindPlayerEntity() };
    }();

    CEntity::ChangeEntityReference(m_pTargetEntity, newTargetEntity);
    m_nModeToGoTo = newGoToMode;

    m_nTypeOfSwitch    = switchType;
    m_bLookingAtPlayer = m_bLookingAtVector = false;
    m_bStartInterScript = true;
}

// 0x50C8B0
void CCamera::TakeControlNoEntity(const CVector& fixedModeVector, eSwitchType switchType, int32 whoIsInControlOfTheCamera) {
    if (whoIsInControlOfTheCamera == 2 && m_nWhoIsInControlOfTheCamera == 1)
        return;

    m_nWhoIsInControlOfTheCamera = whoIsInControlOfTheCamera;
    m_bLookingAtVector           = true;
    m_nModeToGoTo                = MODE_FIXED;
    m_bLookingAtPlayer           = false;
    m_vecFixedModeVector         = fixedModeVector;
    m_nTypeOfSwitch              = switchType;
    m_bStartInterScript          = true;
}

// 0x50C910
void CCamera::TakeControlAttachToEntity(CEntity* target, CEntity* attached, CVector* attachedCamOffset, CVector* attachedCamLookAt, float tilt, eSwitchType switchType, int32 whoIsInControlOfTheCamera) {
    plugin::CallMethod<0x50C910, CCamera*, CEntity*, CEntity*, CVector*, CVector*, float, eSwitchType, int32>(this, target, attached, attachedCamOffset, attachedCamLookAt, tilt, switchType, whoIsInControlOfTheCamera);
}

// 0x50CAE0
void CCamera::TakeControlWithSpline(eSwitchType switchType) {
    m_bLookingAtPlayer = false;
    m_bLookingAtVector = false;
    m_bCutsceneFinished = false;
    m_nModeToGoTo = MODE_FLYBY;
    m_nTypeOfSwitch = switchType;
    m_bStartInterScript = true;
}

// 0x50CB10
void CCamera::UpdateAimingCoors(const CVector& aimingTargetCoors) {
    m_vecAimingTargetCoors = aimingTargetCoors;
}

// 0x515BD0
void CCamera::UpdateSoundDistances() {
    plugin::CallMethod<0x515BD0, CCamera*>(this);
}

// unused
// 0x50CB90
// NOTE: Instruction-level port of the original (x87 FPU code, like the
// original - hence the inline assembly). A global function in the original
// (does not use `this`). Constants: 0.3f @ 0x858C24, 0.25f @ 0x8CCC84.
void CCamera::SetNearClipBasedOnPedCollision(float distSq) {
    __asm {
        mov eax, dword ptr [0xB6FE88]      ; gpCamColVars
        fld dword ptr [eax + 0x10]         ; st0 = baseClip
        fld dword ptr distSq               ; st0 = distSq
        fsqrt
        mov eax, 0xB6EC68                  ; fRam (global)
        fdiv dword ptr [eax]
        fmul ORIG_F0_25                    ; * 0.25 (0x8CCC84)
        fld ORIG_F0_3                      ; st0 = 0.3 (0x858C24)
        fsub st, st(2)                     ; 0.3 - baseClip
        fmulp st(1), st
        fadd st, st(1)                     ; + baseClip
        fst dword ptr distSq               ; nearClip
        fcomp st(1)                        ; nearClip vs baseClip
        fnstsw ax
        test ah, 5
        jp keep_base                       ; nearClip < baseClip ?
        fstp dword ptr distSq
        jmp have_near_clip
keep_base:
        fstp st(0)
have_near_clip:
        mov ecx, dword ptr distSq
        mov edx, dword ptr [0xC1703C]      ; Scene.m_pRwCamera
        push ecx
        push edx
        call RwCameraSetNearClipPlane      ; 0x7EE1D0
        add esp, 8
        ret 4
    }
}

// TODO: eAimingType
// 0x50CBF0
void CCamera::SetColVarsAimWeapon(int32 aimingType) {
    switch (aimingType) {
    case 0:
        CCamera::SetCamCollisionVarDataSet(0);
        break;
    case 1:
        CCamera::SetCamCollisionVarDataSet(1);
        break;
    case 2:
        CCamera::SetCamCollisionVarDataSet(2);
        break;
    case 3:
        CCamera::SetCamCollisionVarDataSet(3);
        break;
    default:
        return;
    }
}

// 0x50CC50
void CCamera::SetColVarsPed(ePedType pedType, int32 nCamPedZoom) {
    const int32 camColVars = [=] {
        switch (pedType) {
        case PED_TYPE_PLAYER1:
            return nCamPedZoom + 3;
        case PED_TYPE_PLAYER2:
            return nCamPedZoom + 6;
        default:
            return 0;
        }
    }();

    if (camColVars != gCurCamColVars) {
        gCurCamColVars = camColVars;
        gCurDistForCam = 1.0f;
        gpCamColVars = gCamColVars[camColVars];
    }
}

// 0x50CD30
void CCamera::CameraGenericModeSpecialCases(CPed* targetPed) {
    m_nExtraEntitiesCount = 0;

    if (!targetPed) {
        return;
    }

    auto* taskHold = static_cast<CTaskSimpleHoldEntity*>(targetPed->GetIntelligence()->GetTaskHold(false));
    if (!taskHold || !taskHold->m_pEntityToHold) {
        return;
    }

    m_pExtraEntity[m_nExtraEntitiesCount++] = targetPed;
}

// 0x50CD80
void CCamera::CameraPedModeSpecialCases() {
    CCollision::bCamCollideWithVehicles = true;
    CCollision::bCamCollideWithObjects  = true;
    CCollision::bCamCollideWithPeds     = true;
}

// 0x50CDA0
void CCamera::CameraPedAimModeSpecialCases(CPed* ped) {
    CameraPedModeSpecialCases();

    if (ped->IsInVehicle()) {
        m_pExtraEntity[m_nExtraEntitiesCount++] = ped->m_pVehicle;
    }
}

// 0x50CDE0
void CCamera::CameraVehicleModeSpecialCases(CVehicle* vehicle) {
    float speed = vehicle->m_vecMoveSpeed.Magnitude();

    const auto slow = speed <= 0.2f;
    CCollision::relVelCamCollisionVehiclesSqr = slow ? 0.1f : 1.0f;
    CCollision::bCamCollideWithVehicles = true;
    CCollision::bCamCollideWithPeds     = slow;
    CCollision::bCamCollideWithObjects  = slow;

    if (vehicle->m_pVehicleBeingTowed) {
        m_pExtraEntity[m_nExtraEntitiesCount++] = vehicle->m_pVehicleBeingTowed;
    }
}

// 0x50CE80
bool CCamera::IsExtraEntityToIgnore(CEntity* entity) {
    if (m_nExtraEntitiesCount <= 0) {
        return false;
    }
    return notsa::contains(m_pExtraEntity, entity);
}

// 0x420C40
bool CCamera::IsSphereVisible(const CVector& origin, float radius, RwMatrix* transformMatrix) {
    return plugin::CallMethodAndReturn<bool, 0x420C40, CCamera*, const CVector&, float, RwMatrix*>(this, origin, radius, transformMatrix);
}

// 0x420D40 - NOTE: Function has no hook
bool CCamera::IsSphereVisible(const CVector& origin, float radius) {
    return IsSphereVisible(origin, radius, (RwMatrix*)&m_mMatInverse)
        || (m_bMirrorActive && IsSphereVisible(origin, radius, (RwMatrix*)&m_mMatMirrorInverse));
}

// 0x50CEB0
bool CCamera::ConsiderPedAsDucking(CPed* ped) {
    auto task = ped->GetIntelligence()->GetTaskDuck(true);
    return task && ped->bIsDucking && !task->m_bIsAborting;
}

// 0x50CEF0
void CCamera::ResetDuckingSystem(CPed* ped) {
    m_fDuckCamMotionFactor    = 0.0f;
    m_fDuckAimCamMotionFactor = 0.0f;
    if (!ped)
        return;

    auto* task = ped->GetIntelligence()->GetTaskDuck(true);
    if (!task)
        return;

    if (!ped->bIsDucking || task->m_bIsAborting)
        return;

    float factor;
    if (ped->m_vecMoveSpeed.Magnitude() <= 0.000001f)
        factor = 0.3f - 1.0f;
    else
        factor = 0.3f - 0.5f;

    m_fDuckCamMotionFactor    = factor;
    m_fDuckAimCamMotionFactor = -0.35f;
}

// arg5 always used as false
// 0x50CFA0
void CCamera::HandleCameraMotionForDucking(CPed* ped, CVector* source, CVector* targPosn, bool arg5) {
    // FUN_006010A0 == CPedIntelligence::GetTaskDuck (called twice in the original)
    const auto* duckTask = ped->GetIntelligence()->GetTaskDuck(true);

    float duckAmount = 0.0f;
    if (duckTask && ped->bIsDucking && !duckTask->m_bIsAborting) { // + 0x46C & 0x4000000, task + 0x19
        duckAmount = ped->m_vecMoveSpeed.SquaredMagnitude() <= StaticRef<float>(0x8630EC) // 1e-6f
            ? StaticRef<float>(0x8CCB94) - 1.0f  // 0.3f - 1.0f
            : StaticRef<float>(0x8CCB98) - 0.5f; // 0.3f - 0.5f
    }

    if (!arg5) {
        m_fDuckCamMotionFactor += CTimer::GetTimeStep() * 0.1f * (duckAmount - m_fDuckCamMotionFactor);
    }
    if (source) {
        source->z += m_fDuckCamMotionFactor;
    }
    if (targPosn) {
        targPosn->z += m_fDuckCamMotionFactor;
    }
}

// arg5 always used as false
// 0x50D090
void CCamera::HandleCameraMotionForDuckingDuringAim(CPed* ped, CVector* source, CVector* targPosn, bool arg5) {
    // FUN_006010A0 == CPedIntelligence::GetTaskDuck (called twice in the original)
    const auto* duckTask = ped->GetIntelligence()->GetTaskDuck(true);

    float duckAmount = 0.0f;
    if (duckTask && ped->bIsDucking && !duckTask->m_bIsAborting) {
        duckAmount = StaticRef<float>(0x8630F0); // -0.35f
    }

    if (!arg5) {
        m_fDuckAimCamMotionFactor += CTimer::GetTimeStep() * StaticRef<float>(0x859020) * (duckAmount - m_fDuckAimCamMotionFactor); // * 0.13f
    }
    if (source) {
        source->z += m_fDuckAimCamMotionFactor;
    }
    if (targPosn) {
        targPosn->z += m_fDuckAimCamMotionFactor;
    }
}

// 0x50D160
void CCamera::VectorMoveLinear(CVector* to, CVector* from, float duration, bool bMoveLinearWithEase) {
    const float now = static_cast<float>(CTimer::GetTimeInMS());
    m_fMoveLinearStartTime  = now;
    m_fMoveLinearEndTime    = now + duration;
    m_vecMoveLinearPosnEnd  = *to;
    m_vecMoveLinearPosnStart = *from;
    m_bMoveLinearWithEase   = bMoveLinearWithEase;
}

// 0x50D1D0
void CCamera::VectorTrackLinear(CVector* to, CVector* from, float duration, bool bEase) {
    const float now = static_cast<float>(CTimer::GetTimeInMS());
    m_fTrackLinearStartTime  = now;
    m_fTrackLinearEndTime    = now + duration;
    m_vecTrackLinearEndPoint = *to;
    m_vecTrackLinearStartPoint = *from;
    m_bTrackLinearWithEase   = bEase;
}

// 0x516400
void CCamera::AddShake(float duration, float a2, float a3, float a4, float a5) {
    return AddShakeSimple(duration, 1, 1.0f);
}

// 0x50D240
void CCamera::AddShakeSimple(float durationMs, int32 type, float intensity) {
    m_fShakeIntensity = intensity;
    m_nShakeType = type;
    m_fStartShakeTime = static_cast<float>(CTimer::GetTimeInMS());
    m_fEndShakeTime = m_fStartShakeTime + durationMs;
}

// 0x50D280
void CCamera::LerpFOV(float zoomInFactor, float zoomOutFactor, float timeLimit, bool bEase) {
    m_fStartZoomTime = static_cast<float>(CTimer::GetTimeInMS());
    m_fEndZoomTime = static_cast<float>(CTimer::GetTimeInMS()) + timeLimit;

    m_nZoomMode = bEase; // TODO: Rename
    m_fZoomInFactor = zoomInFactor;
    m_fZoomOutFactor = zoomOutFactor;
}

// 0x50B5D0
void CCamera::ProcessFade() {
    ZoneScoped;

    if (!m_bFading) {
        return;
    }

    float fadeAlpha = 0.0f;

    if (m_nFadeInOutFlag == eFadeFlag::FADE_OUT) {
        m_fFadeDuration == 0.0f
            ? (m_fFadeAlpha += 0.0f)
            : (m_fFadeAlpha -= CTimer::GetTimeStepInSeconds() / m_fFadeDuration * 255.0f);

        if (m_fFadeAlpha > 0.0f) {
            CDraw::FadeValue = static_cast<uint8>(m_fFadeAlpha);
            return;
        }

        m_bFading = false;
    } else {
        if (m_nFadeInOutFlag == eFadeFlag::FADE_OUT) { // stupid, why not use a switch instead?
            CDraw::FadeValue = static_cast<uint8>(m_fFadeAlpha);
            return;
        }

        if (m_fFadeAlpha >= 255.0f) {
            m_bFading = false;
        }

        fadeAlpha = 255.0f;

        m_fFadeDuration == 0.0f
            ? (m_fFadeAlpha += 255.0f)
            : (m_fFadeAlpha += CTimer::GetTimeStepInSeconds() / m_fFadeDuration * 255.0f);

        if (m_fFadeAlpha < 255.0f) {
            CDraw::FadeValue = static_cast<uint8>(m_fFadeAlpha);
            return;
        }
    }

    m_fFadeAlpha = fadeAlpha;
    CDraw::FadeValue = static_cast<uint8>(m_fFadeAlpha);
}

// 0x50B6D0
void CCamera::ProcessMusicFade() {
    if (!m_bMusicFading)
        return;

    if (m_fTimeToWaitToFadeMusic <= 0.0f) {
        switch (m_nMusicFadingDirection) {
        case eFadeFlag::FADE_OUT: {
            m_fEffectsFaderScalingFactor = m_fTimeToFadeMusic > 0.0f
                ? CTimer::GetTimeStepInSeconds() / m_fTimeToFadeMusic + m_fEffectsFaderScalingFactor
                : 1.f;
            
            if (m_fEffectsFaderScalingFactor >= 1.0f) {
                m_bMusicFadedOut = false;
                m_bMusicFading = false;
                m_fEffectsFaderScalingFactor = 1.0f;
            }
            break;
        }
        case eFadeFlag::FADE_IN: {
            if (m_fEffectsFaderScalingFactor <= 0.0f) {
                m_bMusicFadedOut = true;
                m_bMusicFading = false;
                m_fEffectsFaderScalingFactor = 0.0f;
            }
            m_fEffectsFaderScalingFactor = m_fTimeToFadeMusic > 0.0f
                ? std::max(0.f, m_fEffectsFaderScalingFactor - CTimer::GetTimeStepInSeconds() / m_fTimeToFadeMusic)
                : 0.f;
            break;
        }
        }
    } else {
        m_fTimeToWaitToFadeMusic = m_fTimeToWaitToFadeMusic - CTimer::GetTimeStepInSeconds();
    }

    if (!AudioEngine.IsLoadingTuneActive()) {
        AudioEngine.SetMusicFaderScalingFactor(m_fEffectsFaderScalingFactor);
        AudioEngine.SetEffectsFaderScalingFactor(m_fEffectsFaderScalingFactor);
    }
}

// unused, empty
// 0x50B880
void CCamera::ProcessObbeCinemaCameraPed() {
    // NOP
}

//
void CCamera::ProcessObbeCinemaCameraPlane() {
    assert(0);
}

//
void CCamera::ProcessObbeCinemaCameraTrain() {
    assert(0);
}

// 0x50B890
void CCamera::ProcessWideScreenOn() {
    if (m_bWantsToSwitchWidescreenOff) {
        m_bWantsToSwitchWidescreenOff = false;
        m_bWideScreenOn = false;
        m_fWideScreenReductionAmount = 0.0f;
        m_fScreenReductionPercentage = 0.0f;
        m_fFOV_Wide_Screen = 0.0f;
    } else {
        m_fWideScreenReductionAmount = 1.0f;
        m_fScreenReductionPercentage = 30.0f;
        m_fFOV_Wide_Screen = m_aCams[m_nActiveCam].m_fFOV * 0.3f;
    }
}

// 0x516440
static_assert(offsetof(CCamera, m_fTrackLinearStartTime) == 0xC5C);
static_assert(offsetof(CCamera, m_fTrackLinearEndTime) == 0xC60);
static_assert(offsetof(CCamera, m_bCameraPersistTrack) == 0xCEF);
void CCamera::ProcessVectorTrackLinear() {
    const float now = static_cast<float>(CTimer::GetTimeInMS());
    // NOTE: <= like the original (TEST AH,0x41/JP at 0x51645E) - the lerp still runs on
    // the frame where now == m_fTrackLinearEndTime exactly.
    if (now <= m_fTrackLinearEndTime) {
        ProcessVectorTrackLinear((now - m_fTrackLinearStartTime) / (m_fTrackLinearEndTime - m_fTrackLinearStartTime));
        return;
    }
    if (m_bCameraPersistTrack) {
        m_bVecTrackLinearProcessed = true;
    }
}

// 0x50D350
// NOTE: Faithful instruction-level port (x87): the eased interpolation keeps `sin((270 - ratio*180) * π/180)`
// on the FPU stack across the 3 components, exactly like the original. Original constants:
// 180.0 0x85A994, 270.0 0x859070, π/180 0x8595EC, 1.0 0x858624, 0.5 0x858B8C.
static_assert(offsetof(CCamera, m_vecTrackLinearEndPoint) == 0xC64);
static_assert(offsetof(CCamera, m_vecTrackLinearStartPoint) == 0xC70);
static_assert(offsetof(CCamera, m_bTrackLinearWithEase) == 0xC7C);
static_assert(offsetof(CCamera, m_vecTrackLinear) == 0xC80);
static_assert(offsetof(CCamera, m_bVecTrackLinearProcessed) == 0xC8C);
#pragma warning(push)
#pragma warning(disable : 4731) // frame pointer is deliberately modified: manual two-exit epilogue, like the original
void CCamera::ProcessVectorTrackLinear(float ratio) {
    __asm {
        ; ecx = this (thiscall)
        mov al, byte ptr [ecx + 0xC7C]
        test al, al
        mov byte ptr [ecx + 0xC8C], 1
        je no_ease
        fld dword ptr ratio
        fmul ORIG_F180
        fsubr ORIG_F270
        fmul ORIG_DEG2RAD
        fsin
        fadd ORIG_F1
        fmul ORIG_F0_5
        ; st(0) = eased - kept on the stack for all 3 components
        fld dword ptr [ecx + 0xC70]
        fsub dword ptr [ecx + 0xC64]
        fmul st(1), st
        fadd dword ptr [ecx + 0xC64]
        fstp dword ptr [ecx + 0xC80]
        fld dword ptr [ecx + 0xC70 + 4]
        fsub dword ptr [ecx + 0xC64 + 4]
        fmul st(1), st
        fadd dword ptr [ecx + 0xC64 + 4]
        fstp dword ptr [ecx + 0xC80 + 4]
        fld dword ptr [ecx + 0xC70 + 8]
        fsub dword ptr [ecx + 0xC64 + 8]
        fmul st(1), st
        fadd dword ptr [ecx + 0xC64 + 8]
        fstp dword ptr [ecx + 0xC80 + 8]
        fstp st(0)
        mov esp, ebp                      ; both exits return right here, like the original
        pop ebp
        ret 4
no_ease:
        fld dword ptr [ecx + 0xC70]
        fsub dword ptr [ecx + 0xC64]
        fmul dword ptr ratio
        fadd dword ptr [ecx + 0xC64]
        fstp dword ptr [ecx + 0xC80]
        fld dword ptr [ecx + 0xC70 + 4]
        fsub dword ptr [ecx + 0xC64 + 4]
        fmul dword ptr ratio
        fadd dword ptr [ecx + 0xC64 + 4]
        fstp dword ptr [ecx + 0xC80 + 4]
        fld dword ptr [ecx + 0xC70 + 8]
        fsub dword ptr [ecx + 0xC64 + 8]
        fmul dword ptr ratio
        fadd dword ptr [ecx + 0xC64 + 8]
        fstp dword ptr [ecx + 0xC80 + 8]
        mov esp, ebp
        pop ebp
        ret 4
    }
}
#pragma warning(pop)

//
void CCamera::ProcessObbeCinemaCameraBoat() {
    assert(0);
}

//
void CCamera::ProcessObbeCinemaCameraCar() {
    assert(0);
}

//
void CCamera::ProcessObbeCinemaCameraHeli() {
    assert(0);
}

// 0x50D430
// NOTE: Faithful instruction-level port (x87), same easing as ProcessVectorTrackLinear(float).
// The original interpolates from m_vecMoveLinearPosnStart towards m_vecMoveLinearPosnEnd.
static_assert(offsetof(CCamera, m_vecMoveLinearPosnStart) == 0xCC4);
static_assert(offsetof(CCamera, m_vecMoveLinearPosnEnd) == 0xCD0);
static_assert(offsetof(CCamera, m_bMoveLinearWithEase) == 0xCDC);
static_assert(offsetof(CCamera, m_vecMoveLinear) == 0xCE0);
static_assert(offsetof(CCamera, m_bVecMoveLinearProcessed) == 0xCEC);
#pragma warning(push)
#pragma warning(disable : 4731) // frame pointer is deliberately modified: manual two-exit epilogue, like the original
void CCamera::ProcessVectorMoveLinear(float ratio) {
    __asm {
        ; ecx = this (thiscall)
        mov al, byte ptr [ecx + 0xCDC]
        test al, al
        mov byte ptr [ecx + 0xCEC], 1
        je no_ease
        fld dword ptr ratio
        fmul ORIG_F180
        fsubr ORIG_F270
        fmul ORIG_DEG2RAD
        fsin
        fadd ORIG_F1
        fmul ORIG_F0_5
        fld dword ptr [ecx + 0xCD0]
        fsub dword ptr [ecx + 0xCC4]
        fmul st(1), st
        fadd dword ptr [ecx + 0xCC4]
        fstp dword ptr [ecx + 0xCE0]
        fld dword ptr [ecx + 0xCD0 + 4]
        fsub dword ptr [ecx + 0xCC4 + 4]
        fmul st(1), st
        fadd dword ptr [ecx + 0xCC4 + 4]
        fstp dword ptr [ecx + 0xCE0 + 4]
        fld dword ptr [ecx + 0xCD0 + 8]
        fsub dword ptr [ecx + 0xCC4 + 8]
        fmul st(1), st
        fadd dword ptr [ecx + 0xCC4 + 8]
        fstp dword ptr [ecx + 0xCE0 + 8]
        fstp st(0)
        mov esp, ebp                      ; both exits return right here, like the original
        pop ebp
        ret 4
no_ease:
        fld dword ptr [ecx + 0xCD0]
        fsub dword ptr [ecx + 0xCC4]
        fmul dword ptr ratio
        fadd dword ptr [ecx + 0xCC4]
        fstp dword ptr [ecx + 0xCE0]
        fld dword ptr [ecx + 0xCD0 + 4]
        fsub dword ptr [ecx + 0xCC4 + 4]
        fmul dword ptr ratio
        fadd dword ptr [ecx + 0xCC4 + 4]
        fstp dword ptr [ecx + 0xCE0 + 4]
        fld dword ptr [ecx + 0xCD0 + 8]
        fsub dword ptr [ecx + 0xCC4 + 8]
        fmul dword ptr ratio
        fadd dword ptr [ecx + 0xCC4 + 8]
        fstp dword ptr [ecx + 0xCE0 + 8]
        mov esp, ebp
        pop ebp
        ret 4
    }
}
#pragma warning(pop)

// 0x516500
static_assert(offsetof(CCamera, m_fStartZoomTime) == 0xCA4);
static_assert(offsetof(CCamera, m_fEndZoomTime) == 0xCA8);
static_assert(offsetof(CCamera, m_bBlockZoom) == 0xCED);
void CCamera::ProcessFOVLerp() {
    if (const auto now = static_cast<float>(CTimer::GetTimeInMS()); now <= m_fEndZoomTime) { /* Check if still processing */
        ProcessFOVLerp(invLerp(m_fStartZoomTime, m_fEndZoomTime, now));
    } else if (m_bBlockZoom) { /* Finished */
        m_bFOVLerpProcessed = true;
    }
}

// 0x50D510
// NOTE: Faithful instruction-level port (x87). The ease test is on m_nZoomMode (0xCB4)
// and m_bFOVLerpProcessed (0xCB5) is set unconditionally - just like the original.
static_assert(offsetof(CCamera, m_fZoomOutFactor) == 0xCB0); // NOTE: header name/offset vs original role differ, see below
static_assert(offsetof(CCamera, m_fZoomInFactor) == 0xCAC);
static_assert(offsetof(CCamera, m_nZoomMode) == 0xCB4);
static_assert(offsetof(CCamera, m_bFOVLerpProcessed) == 0xCB5);
static_assert(offsetof(CCamera, m_fFOVNew) == 0xCB8);
#pragma warning(push)
#pragma warning(disable : 4731) // frame pointer is deliberately modified: manual two-exit epilogue, like the original
void CCamera::ProcessFOVLerp(float ratio) {
    __asm {
        ; ecx = this (thiscall)
        mov al, byte ptr [ecx + 0xCB4]
        test al, al
        mov byte ptr [ecx + 0xCB5], 1
        je no_ease
        fld dword ptr ratio
        fmul ORIG_F180
        fsubr ORIG_F270
        fmul ORIG_DEG2RAD
        fsin
        fadd ORIG_F1
        fmul ORIG_F0_5
        ; NB: 0xCB0 (m_fZoomOutFactor) is the value the FOV interpolates towards,
        ;     0xCAC (m_fZoomInFactor) the one it starts from - as in the original.
        fld dword ptr [ecx + 0xCB0]
        fsub dword ptr [ecx + 0xCAC]
        fmulp st(1), st
        fadd dword ptr [ecx + 0xCAC]
        fstp dword ptr [ecx + 0xCB8]
        mov esp, ebp                      ; both exits return right here, like the original
        pop ebp
        ret 4
no_ease:
        fld dword ptr [ecx + 0xCB0]
        fsub dword ptr [ecx + 0xCAC]
        fmul dword ptr ratio
        fadd dword ptr [ecx + 0xCAC]
        fstp dword ptr [ecx + 0xCB8]
        mov esp, ebp
        pop ebp
        ret 4
    }
}
#pragma warning(pop)

// 0x5164A0
static_assert(offsetof(CCamera, m_fMoveLinearStartTime) == 0xCBC);
static_assert(offsetof(CCamera, m_fMoveLinearEndTime) == 0xCC0);
static_assert(offsetof(CCamera, m_bCameraPersistPosition) == 0xCEE);
void CCamera::ProcessVectorMoveLinear() {
    const float now = static_cast<float>(CTimer::GetTimeInMS());
    // NOTE: <= like the original (TEST AH,0x41/JP at 0x5164BE).
    if (now <= m_fMoveLinearEndTime) {
        ProcessVectorMoveLinear((now - m_fMoveLinearStartTime) / (m_fMoveLinearEndTime - m_fMoveLinearStartTime));
        return;
    }
    if (m_bCameraPersistPosition) {
        m_bVecMoveLinearProcessed = true;
    }
}

// unused
// 0x51A6F0
void CCamera::ProcessShake() {
    plugin::CallMethod<0x51A6F0, CCamera*>(this);
}

// shakeIntensity not used
// 0x516560
CVector* CCamera::ProcessShake(float intensity) {
    return plugin::CallMethodAndReturn<CVector*, 0x516560, CCamera*, float>(this, intensity);
}

// inlined - 0x52B845
// 0x516AE0
void CCamera::ProcessScriptedCommands() {
    ProcessVectorMoveLinear();
    ProcessVectorTrackLinear();
    ProcessFOVLerp();
}

// 0x52B730
// TODO: Reversal blocked - the per-frame camera driver (transition/fade/cutscene
// state machine, obbe cinema cams, train cam nodes, ...). Depends on
// TryToStartNewCamMode/CamControl/ImproveNearClip being reversed first.
void CCamera::Process() {
    ZoneScoped;

    plugin::CallMethod<0x52B730, CCamera*>(this);
}

// 0x514860
void CCamera::DrawBordersForWideScreen() {
    CRect rect;
    GetScreenRect(&rect);
    if (m_nBlurType == eMotionBlurType::NONE || m_nBlurType == eMotionBlurType::LIGHT_SCENE) {
        m_nMotionBlurAddAlpha = 80;
    }
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(NULL));
    CSprite2d::DrawRect({ -5.f, -5.f,     SCREEN_WIDTH + 5.f, rect.top         }, { 0, 0, 0, 255 });
    CSprite2d::DrawRect({ -5.f, rect.bottom, SCREEN_WIDTH + 5.f, SCREEN_HEIGHT + 5.f }, { 0, 0, 0, 255 });
}

// 0x4748A0
bool CCamera::VectorMoveRunning() const {
    return CTimer::m_snTimeInMilliseconds <= m_fMoveLinearEndTime;
}

// 0x474891
bool CCamera::VectorTrackRunning() const {
    return CTimer::m_snTimeInMilliseconds <= m_fTrackLinearEndTime;
}

// 0x514950
void CCamera::FinishCutscene() {
    SetPercentAlongCutScene(100.0f);
    m_fPositionAlongSpline = 1.0f;
    m_bCutsceneFinished = true;
}

// 0x514970
void CCamera::Find3rdPersonCamTargetVector(float range, CVector gunMuzzle, CVector& outSource, CVector& outTarget) {
    const auto pActiveCam = &m_aCams[m_nActiveCam];
    const float tanHalfFOV = std::tan(DegreesToRadians(pActiveCam->m_fFOV * 0.5f));
    const float aspectRatio = CDraw::ms_fAspectRatio;
    
    // Calculate aim target direction (This will be a unit vector)
    CVector dir = m_aCams[m_nActiveCam].m_vecFront;
    
    if (pActiveCam->m_nMode == eCamMode::MODE_TWOPLAYER_IN_CAR_AND_SHOOTING) {
        pActiveCam->Get_TwoPlayer_AimVector(dir);
    } else {
        // Vertical offset
        dir += pActiveCam->m_vecUp * (tanHalfFOV * ((0.5f - m_f3rdPersonCHairMultY) * 2.0f) / aspectRatio);

        // Horizontal offset
        const auto right = pActiveCam->m_vecFront.Cross(pActiveCam->m_vecUp);
        dir += right * (tanHalfFOV * ((m_f3rdPersonCHairMultX - 0.5f) * 2.0f));
        
        // Handle zero magnitude case
        if (dir.Magnitude() <= 0.0f) {
            dir = CVector(1.0f, 0.0f, 0.0f);
        } else {
            dir.Normalise();
        }
    }
    
    // Calculate intersection point with muzzle
    outSource = pActiveCam->m_vecSource;
    outSource += (gunMuzzle - outSource).ProjectOnToNormal(dir);

    // Apply final range to target 
    outTarget = outSource + dir * range;
}

// 0x514B80
float CCamera::CalculateGroundHeight(eGroundHeightType type) {
    static auto& lastCalcCamPos    = StaticRef<CVector>(0xB70034);
    static auto& exactGroundHeight = StaticRef<float>(0xB70030);
    static auto& bbTopZ            = StaticRef<float>(0xB7002C);
    static auto& bbBottomZ         = StaticRef<float>(0xB70028);

    const auto& camPos = GetPosition();

    // Possibly update the positions (If the camera has moved enough)
    const auto CheckDelta = [](float d) { return std::abs(d) > 20.f; };
    if (CheckDelta(lastCalcCamPos.x - camPos.x) || CheckDelta(lastCalcCamPos.y - camPos.y) || CheckDelta(lastCalcCamPos.z - camPos.z)) { // Check if there's enough of a delta
        CColPoint cp;
        CEntity* hitEntity;
        if (CWorld::ProcessVerticalLine({ camPos.x, camPos.y, 1000.f }, -1000.f, cp, hitEntity, true, false, false, false, true)) {
            const auto& hitEntPos = hitEntity->GetPosition();
            const auto& hitBB = hitEntity->GetColModel()->GetBoundingBox();

            exactGroundHeight = cp.m_vecPoint.z;

            bbTopZ = hitEntPos.z + hitBB.m_vecMax.z;

            const auto bbsz = hitBB.GetSize();
            bbBottomZ = std::max(
                0.f,
                bbsz.x > 120.f || bbsz.y > 120.f
                    ? exactGroundHeight
                    : hitEntPos.z + hitBB.m_vecMin.z
            );
        }
        lastCalcCamPos = camPos;
    }

    switch (type) {
    case eGroundHeightType::ENTITY_BB_TOP:       return bbTopZ;
    case eGroundHeightType::EXACT_GROUND_HEIGHT: return exactGroundHeight;
    case eGroundHeightType::ENTITY_BB_BOTTOM:    return bbBottomZ;
    default:                                     NOTSA_UNREACHABLE();
    }
}

// 0x514D60
static_assert(offsetof(CCamera, m_avecFrustumNormals) == 0xB24);
static_assert(offsetof(CCamera, m_avecFrustumWorldNormals) == 0xB54);
static_assert(offsetof(CCamera, m_avecFrustumWorldNormals_Mirror) == 0xB84);
static_assert(offsetof(CCamera, m_fFrustumPlaneOffsets) == 0xBB4);
static_assert(offsetof(CCamera, m_fFrustumPlaneOffsets_Mirror) == 0xBC4);
void CCamera::CalculateFrustumPlanes(bool bForMirror) {
    const float fovAng = CDraw::ms_fFOV * StaticRef<float>(0x8631D8); // FOV * PI/360
    const float cosFov = cosf(fovAng);
    const float sinFov = sinf(fovAng);

    // The original loads RsGlobal.maximumHeight (0xC17048) / maximumWidth (0xC17044)
    // as INTEGERS (fild / fidiv) - both are int32 in RsGlobal at 0xC17040.
    const float aspect = static_cast<float>(StaticRef<int32>(0xC17048)) / static_cast<float>(StaticRef<int32>(0xC17044)); // screen height / width

    // Base (camera space) frustum normals. RwV3dTransformVector (3x3) maps the components
    // onto the matrix rows: x -> right, y -> forward, z -> up.
    // NOTE the component slots, exactly as in the original: normals 0/1 (the vertical fov
    // planes) live in the right/forward slots with z = 0, while normals 2/3 (the horizontal,
    // aspect-carrying planes) live in the forward/up slots with x = 0.
    m_avecFrustumNormals[0] = CVector{ cosFov, -sinFov, 0.0f };
    m_avecFrustumNormals[1] = CVector{ -cosFov, -sinFov, 0.0f };
    m_avecFrustumNormals[2] = CVector{ 0.0f, -(aspect * sinFov), -(aspect * cosFov) };
    m_avecFrustumNormals[3] = CVector{ 0.0f, -(aspect * sinFov), aspect * cosFov };

    // Transform the normals into world space and compute the plane offsets
    if (!bForMirror) {
        TransformVectors(reinterpret_cast<RwV3d*>(m_avecFrustumWorldNormals.data()), 4, m_mCameraMatrix, reinterpret_cast<const RwV3d*>(m_avecFrustumNormals.data())); // FUN_0054EEA0
        const CVector& pos = GetPosition();
        for (auto i = 0u; i < m_avecFrustumWorldNormals.size(); i++) {
            m_fFrustumPlaneOffsets[i] = DotProduct(m_avecFrustumWorldNormals[i], pos);
        }
    } else {
        TransformVectors(reinterpret_cast<RwV3d*>(m_avecFrustumWorldNormals_Mirror.data()), 4, m_mCameraMatrix, reinterpret_cast<const RwV3d*>(m_avecFrustumNormals.data())); // FUN_0054EEA0
        const CVector& pos = GetPosition();
        for (auto i = 0u; i < m_avecFrustumWorldNormals_Mirror.size(); i++) {
            m_fFrustumPlaneOffsets_Mirror[i] = DotProduct(m_avecFrustumWorldNormals_Mirror[i], pos);
        }
    }
}

// 0x5150E0
void CCamera::CalculateDerivedValues(bool bForMirror, bool bOriented) {
    m_mMatInverse = Invert(m_mCameraMatrix); // FUN_0059BDD0 + operator= (FUN_0059BBC0)

    CalculateFrustumPlanes(bForMirror);

    // Forward vector of the camera matrix
    CVector& fwd = m_mCameraMatrix.GetForward();

    if (fwd.x == 0.0f && fwd.y == 0.0f) {
        fwd.x = 1.0e-4f; // 0x38D1B717
    } else if (bOriented) {
        m_fOrientation = atan2(fwd.x, fwd.y); // fpatan
    }

    m_fCamFrontXNorm = fwd.x;
    m_fCamFrontYNorm = fwd.y;

    const float mag = sqrtf(sq(m_fCamFrontXNorm) + sq(m_fCamFrontYNorm));
    if (mag == 0.0f) {
        m_fCamFrontXNorm = 1.0f;
    } else {
        const float inv = 1.0f / mag;
        m_fCamFrontXNorm *= inv;
        m_fCamFrontYNorm *= inv;
    }
}

// 0x516B20
// TODO: Reversal blocked - the body iterates the ped's bone/anim-frame hierarchy
// (RpHAnim frames, anim association data @ 0xB6F250/0xB6F32C) and dozens of
// unreversed Cam collision helpers; a faithful reconstruction needs those first.
void CCamera::ImproveNearClip(CVehicle* vehicle, CPed* ped, CVector* source, CVector* targPosn) {
    return plugin::CallMethod<0x516B20, CCamera*, CVehicle*, CPed*, CVector*, CVector*>(this, vehicle, ped, source, targPosn);
}

static auto& preMirrorMat = StaticRef<CMatrix>(0xB6FE40);

// 0x51A560
void CCamera::SetCameraUpForMirror() {
    preMirrorMat = m_mCameraMatrix;
    m_mCameraMatrix = m_mMatMirror;
    CopyCameraMatrixToRWCam(true);
    CalculateDerivedValues(true, false);
}

// 0x51A5A0
void CCamera::RestoreCameraAfterMirror() {
    SetMatrix(preMirrorMat);
    CopyCameraMatrixToRWCam(true);
    CalculateDerivedValues(false, false);
}

// 0x51A5D0
bool CCamera::ConeCastCollisionResolve(const CVector& pos, const CVector& lookAt, CVector& outDest, float radius, float minDist, float& outDist) {
    if (pos == lookAt) {
        return false;
    }

    if (CCollision::CameraConeCastVsWorldCollision(CSphere{ lookAt, radius }, CSphere{ pos, radius }, outDist, minDist)) {
        outDest = lerp(lookAt, pos, outDist);
        return true;
    } else {
        outDest = pos;
        outDist = 1.f;
        return false;
    }
}

// 0x51E560
// TODO: Reversal blocked - a giant switch over ~60 camera modes dispatching into
// the (mostly unreversed) CCam mode constructors; infeasible to reconstruct
// faithfully until those are done. Reversed when CCam covers all modes.
bool CCamera::TryToStartNewCamMode(int32 camSequence) {
    return plugin::CallMethodAndReturn<bool, 0x51E560, CCamera*, int32>(this, camSequence);
}

// 0x520190
// NOTE: The original returns a bool (whether the camera collided) - the reversed
// signature here is void, as no reversed caller consumes the return value.
void CCamera::CameraColDetAndReact(CVector* source, CVector* target) {
    static auto& s_CachedVehModelId = StaticRef<uint16>(0xB700F0);
    static auto& s_VehMinSphereZ    = StaticRef<float>(0xB700EC);
    static auto& s_HasLastSource    = StaticRef<uint32>(0xB700E8);
    static auto& s_LastSource       = StaticRef<CVector>(0xB700DC);

    CVector sourceToTarget = *source - *target;
    const float dist = sourceToTarget.Magnitude();

    float maxDist = dist * gpCamColVars[0] * StaticRef<float>(0x8CCB90); // * 0.2939f

    float typeDist{};
    if (gCurCamColVars > 9 && CWorld::pIgnoreEntity) {
        auto*       entity = CWorld::pIgnoreEntity;
        auto*       veh = entity->AsVehicle();
        const auto vehSubType = *reinterpret_cast<const int32*>(reinterpret_cast<const uint8*>(veh) + 0x594); // m_nVehicleType/SubType (field order uncertain in original)
        if (veh->GetType() == ENTITY_TYPE_VEHICLE && vehSubType == 0) { // + 0x594 == 0 (automobile)
            if (veh->m_nModelIndex != s_CachedVehModelId) {
                // Lowest z of the vehicle's collision spheres
                s_VehMinSphereZ = 100.0f;
                const auto* colData = veh->GetColModel()->GetData(); // FUN_00535300
                for (const auto& sphere : std::span{ colData->m_pSpheres, (size_t)colData->m_nNumSpheres }) {
                    s_VehMinSphereZ = std::min(s_VehMinSphereZ, sphere.m_vecCenter.z - sphere.m_fRadius);
                }
                s_CachedVehModelId = veh->m_nModelIndex;
            }
            if (!veh->m_matrix) {
                veh->AllocateMatrix();                        // FUN_0054F560
                veh->m_placement.UpdateMatrix(veh->m_matrix); // FUN_0054F1B0
            }
            const auto& mat = veh->GetMatrix();
            typeDist = DotProduct(*target - veh->GetPosition(), mat.GetUp()) - s_VehMinSphereZ;
            typeDist = std::max(typeDist, StaticRef<float>(0x858CC4)); // 0.2f
            if (gpCamColVars[1] < typeDist) {
                maxDist = std::min(maxDist, gpCamColVars[1]);
                goto LAB_005203EF;
            }
        } else {
            const auto& bb = entity->GetColModel()->GetBoundingBox();
            typeDist = std::min({
                (bb.m_vecMax.x - bb.m_vecMin.x) * 0.5f,
                (bb.m_vecMax.y - bb.m_vecMin.y) * 0.5f,
                (bb.m_vecMax.z - bb.m_vecMin.z) * 0.5f,
            });
            if (gpCamColVars[1] < typeDist) {
                maxDist = std::min(maxDist, gpCamColVars[1]);
                goto LAB_005203EF;
            }
        }
        if (typeDist < maxDist) {
            maxDist = typeDist;
        }
    }

LAB_005203EF:
    float coneRadius = std::clamp(maxDist, StaticRef<float>(0x8CCE18) /* 0.65f */, gpCamColVars[1]);

    float minDist = gpCamColVars[2];
    if (gCurCamColVars < 10) {
        minDist = StaticRef<float>(0x8CCE14); // 0.3f
        if (gCurCamColVars < 4) {
            minDist = StaticRef<float>(0x8CCE10); // 0.18f
        }
        minDist /= dist;
    }

    bool bIsTrain = false;
    if (gCurCamColVars > 9 && CWorld::pIgnoreEntity
        && CWorld::pIgnoreEntity->GetType() == ENTITY_TYPE_VEHICLE
        && *reinterpret_cast<const int32*>(reinterpret_cast<const uint8*>(CWorld::pIgnoreEntity) + 0x590) == 9) { // + 0x590 == 9 (train)
        bIsTrain = true;
        minDist = StaticRef<float>(0x8CCE0C); // 0.05f
    }

    StaticRef<float>(0xB6EC6C) = coneRadius;

    CVector outDest{};
    float   outDist{};
    if (!ConeCastCollisionResolve(*source, *target, outDest, coneRadius, minDist, outDist)) { // FUN_0051A5D0
        return;
    }

    if (outDist <= gpCamColVars[3]) { // <= like the original (TEST AH,0x41/JP at 0x5204DE)
        RwCameraSetNearClipPlane(Scene.m_pRwCamera, gpCamColVars[4]); // FUN_007EE1D0
    }

    // Smooth the current camera distance
    if (gCurDistForCam <= outDist) {
        if ((s_HasLastSource & 1) == 0) {
            s_HasLastSource |= 1;
            s_LastSource = CVector{ 0.0f, 0.0f, 0.0f };
        }
        if (StaticRef<float>(0x8CCE08) * StaticRef<float>(0x8CCE08) < (s_LastSource - *source).SquaredMagnitude()) { // 0.01f
            float delta = (outDist - gCurDistForCam) * CTimer::GetTimeStep() * gpCamColVars[5];
            delta = std::min(delta, StaticRef<float>(0x8CCE04)); // 0.05f
            gCurDistForCam += delta;
        }
        s_LastSource = *source;
    } else {
        gCurDistForCam = outDist;
    }
    gCurDistForCam = std::min(gCurDistForCam, 1.0f);

    // Move the source back along the target->source ray
    *source = *target + sourceToTarget * gCurDistForCam;

    if (bIsTrain && gCurDistForCam < StaticRef<float>(0x8CCE00)) { // 0.5f
        RwCameraSetNearClipPlane(Scene.m_pRwCamera, StaticRef<float>(0x8CCDFC)); // 0.05f
    }
}

// 0x527FA0
// TODO: Reversal blocked - ~2500 lines handling every control scheme / cam mode
// combination for up to 2 players; depends on TryToStartNewCamMode and most
// unreversed CCam/CPed/CPad interactions. Reversed after those.
void CCamera::CamControl() {
    plugin::CallMethod<0x527FA0, CCamera*>(this); // good luck warrior!
}

// 0x5B24A0
void CCamera::DeleteCutSceneCamDataMemory() {
    for (auto& splines : m_aPathArray) {
        delete splines.m_pArrPathData;
        splines.m_pArrPathData = nullptr;
    }
}

// 0x5B24D0
void CCamera::LoadPathSplines(FILE* file) {
    plugin::CallMethod<0x5B24D0, CCamera*, FILE*>(this, file); // pass-through: the file argument MUST be forwarded
}

// 0x50AB50
void CCamera::GetScreenRect(CRect* rect) const {
    rect->left  = 0.0f;
    rect->right = SCREEN_WIDTH;

    if (m_bWideScreenOn) {
        rect->top    = (float)(RsGlobal.maximumHeight / 2) * m_fScreenReductionPercentage / 100.f - SCREEN_SCALE_Y(22.0f);
        rect->bottom = SCREEN_HEIGHT - (RsGlobal.maximumHeight / 2) * m_fScreenReductionPercentage / 100.f - SCREEN_SCALE_Y(14.0f);
    } else {
        rect->top    = 0.0f;
        rect->bottom = SCREEN_HEIGHT;
    }
}

// 0x50CB60
void CCamera::SetCamCollisionVarDataSet(int32 index) {
    if (index == gCurCamColVars) {
        return;
    }

    gCurCamColVars = index;
    gCurDistForCam = 1.0f;
    gpCamColVars   = gCamColVars[index];
}

// 0x50CCA0
void CCamera::SetColVarsVehicle(eVehicleType vehicleType, int32 camVehicleZoom) {
    switch (vehicleType) {
        case VEHICLE_TYPE_AUTOMOBILE:
        case VEHICLE_TYPE_PLANE:
            SetCamCollisionVarDataSet(camVehicleZoom + 9);
            return;
        case VEHICLE_TYPE_MTRUCK:
            SetCamCollisionVarDataSet(camVehicleZoom + 12);
            return;
        case VEHICLE_TYPE_QUAD:
            SetCamCollisionVarDataSet(camVehicleZoom + 15);
            return;
        case VEHICLE_TYPE_HELI:
            SetCamCollisionVarDataSet(camVehicleZoom + 18);
            return;
        case VEHICLE_TYPE_BOAT:
            SetCamCollisionVarDataSet(camVehicleZoom + 21);
            return;
        case VEHICLE_TYPE_TRAIN:
            SetCamCollisionVarDataSet(camVehicleZoom + 24);
            return;
    }
}

// 0x515BC0
void CCamera::StartTransitionWhenNotFinishedInter(eCamMode newCamMode) {
    m_bDoingSpecialInterp = true;
    StartTransition(newCamMode);
}

// 0x515200
/**
 * @brief Initiates a camera transition to a new camera mode.
 * 
 * This function handles the transition between different camera modes, setting up all necessary parameters
 * for a smooth camera movement. It manages aspects such as:
 * - Camera rotation and positioning
 * - Transition timing and interpolation fractions
 * - Special handling for weapon modes
 * - Entity references and target updates
 * 
 * The transition process includes:
 * 1. Setting up default transition values
 * 2. Handling player rotation for weapon modes
 * 3. Setting up the new camera parameters
 * 4. Managing specific camera mode transitions
 * 5. Initializing transition state and interpolation values
 * 6. Storing starting speeds and final transition parameters
 * 
 * @param newCamMode The camera mode to transition to (type eCamMode)
 * 
 * @note This function is central to the game's camera system and affects how the camera behaves
 * when switching between different views (e.g., from following a ped to aiming a weapon).
 * 
 * @see eCamMode
 * @see CCam
 */
void CCamera::StartTransition(eCamMode newCamMode) {
    CCam& activeCam             = m_aCams[m_nActiveCam];
    const auto activeCamMode    = activeCam.m_nMode;

    // Unused flag, not used in the game.
    // In GTA III/VC it was used for the Colt Python.
    m_bItsOkToLookJustAtThePlayer = false;

    // Default values
    m_bUseTransitionBeta          = false;
    m_fFractionInterToStopMoving  = 0.25f;
    m_fFractionInterToStopCatchUp = 0.75f;

    // Handle player rotation for weapon modes
    if (m_pTargetEntity && m_pTargetEntity->GetIsTypePed() && notsa::contains({ MODE_SNIPER, MODE_ROCKETLAUNCHER, MODE_ROCKETLAUNCHER_HS, MODE_M16_1STPERSON, MODE_SNIPER_RUNABOUT, MODE_ROCKETLAUNCHER_RUNABOUT, MODE_ROCKETLAUNCHER_RUNABOUT_HS, MODE_M16_1STPERSON_RUNABOUT, MODE_FIGHT_CAM_RUNABOUT, MODE_HELICANNON_1STPERSON, MODE_CAMERA, MODE_1STPERSON_RUNABOUT }, activeCamMode)) {
        const float angle                            = CGeneral::GetATanOfXY(activeCam.m_vecFront.x, activeCam.m_vecFront.y) - HALF_PI;
        m_pTargetEntity->AsPed()->m_fCurrentRotation = angle;
        m_pTargetEntity->AsPed()->m_fAimingRotation  = angle;
    }

    // Setup new camera
    activeCam.m_vecCamFixedModeVector = m_vecFixedModeVector;
    CEntity::ChangeEntityReference(activeCam.m_pCamTargetEntity, m_pTargetEntity);

    activeCam.m_vecCamFixedModeSource   = m_vecFixedModeSource;
    activeCam.m_vecCamFixedModeUpOffSet = m_vecFixedModeUpOffSet;
    activeCam.m_bCamLookingAtVector     = m_bLookingAtVector;
    if (m_bItsOkToLookJustAtThePlayer) {
        activeCam.m_nMode = newCamMode;
    }

    // Handle specific camera mode transitions
    switch (newCamMode) {
    case MODE_BEHINDCAR:
    case MODE_BEHINDBOAT:
        activeCam.m_fBetaSpeed = 0.0f;
        break;
    case MODE_FOLLOWPED: {
        if (m_bJustCameOutOfGarage) {
            activeCam.m_fHorizontalAngle = CGeneral::GetATanOfXY(activeCam.m_vecFront.x, activeCam.m_vecFront.y) + PI;
            activeCam.m_fTransitionBeta  = 0.0f;
        }

        m_bCamDirectlyInFront |= m_bTargetJustCameOffTrain;

        if (activeCamMode == MODE_CAM_ON_A_STRING) {
            m_bUseTransitionBeta        = true;
            const float angle           = CGeneral::GetATanOfXY(activeCam.m_vecFront.x, activeCam.m_vecFront.y);
            activeCam.m_fTransitionBeta = angle + (fabs(angle) <= HALF_PI ? DegreesToRadians(235.0f) : DegreesToRadians(55.0f));
        }
        break;
    }
    case MODE_SNIPER:
    case MODE_ROCKETLAUNCHER:
    case MODE_M16_1STPERSON:
    case MODE_SNIPER_RUNABOUT:
    case MODE_ROCKETLAUNCHER_RUNABOUT:
    case MODE_1STPERSON_RUNABOUT:
    case MODE_M16_1STPERSON_RUNABOUT:
    case MODE_FIGHT_CAM_RUNABOUT:
    case MODE_HELICANNON_1STPERSON:
    case MODE_CAMERA:
    case MODE_ROCKETLAUNCHER_HS:
    case MODE_ROCKETLAUNCHER_RUNABOUT_HS: {
        CEntity* vehicle             = FindPlayerVehicle();
        CMatrix* playerMat           = vehicle ? &vehicle->GetMatrix() : &FindPlayerPed()->GetMatrix();
        activeCam.m_fHorizontalAngle = CGeneral::GetATanOfXY(playerMat->GetForward().x, playerMat->GetForward().y);
        activeCam.m_fVerticalAngle   = 0.0f;
        break;
    }
    case MODE_CAM_ON_A_STRING: {
        if (m_bLookingAtPlayer && !m_bJustCameOutOfGarage) {
            m_bUseTransitionBeta = true;
            const float angle    = CGeneral::GetATanOfXY(activeCam.m_vecFront.x, activeCam.m_vecFront.y);
            if (activeCamMode == MODE_FIXED) { // Ghidra
                activeCam.m_fTransitionBeta = angle;
                break;
            }

            // Reconstruced + android simplified
            activeCam.m_fTransitionBeta = angle + (fabs(angle) <= HALF_PI ? DegreesToRadians(235.0f) : DegreesToRadians(55.0f));
        }
        break;
    }
    case MODE_PED_DEAD_BABY:
        activeCam.m_fVerticalAngle = DegreesToRadians(15.0f);
        break;
    }

    // Backup horizontal angle before Init.
    const float horizAngle = activeCam.m_fHorizontalAngle;

    int targetCoorsDuration = 600; // Like android version instead bool.
    m_nTransitionDuration   = 1'350;

    // Switch active camera
    if (activeCamMode == MODE_FOLLOWPED && newCamMode == MODE_CAM_ON_A_STRING
        || activeCamMode == MODE_CAM_ON_A_STRING && newCamMode == MODE_FOLLOWPED) {
        activeCam.m_nMode = newCamMode;
    } else {
        activeCam.Init();
        activeCam.m_nMode            = newCamMode;
        activeCam.m_fHorizontalAngle = horizAngle;
    }

    [&]() -> const void {
        if (newCamMode == MODE_CAM_ON_A_STRING && notsa::contains({ MODE_SYPHON_CRIM_IN_FRONT, MODE_FOLLOWPED, MODE_SYPHON, MODE_SPECIAL_FIXED_FOR_SYPHON, MODE_AIMWEAPON }, activeCamMode)) {
            m_fFractionInterToStopMoving  = 0.1f;
            m_fFractionInterToStopCatchUp = 0.9f;
            m_nTransitionDuration         = 750;
            return;
        }

        switch (activeCamMode) {
        case MODE_SYPHON_CRIM_IN_FRONT:
            if (newCamMode == MODE_SYPHON) {
                m_nTransitionDuration = 1'800;
                return;
            }
            break;
        case MODE_SPECIAL_FIXED_FOR_SYPHON:
            m_fFractionInterToStopMoving  = 0.2f;  // dword_8CCCCC
            m_fFractionInterToStopCatchUp = 0.8f;  // *&dword_8CCCC8
            m_nTransitionDuration         = 1'000; // dword_8CCCC4
            return;
        case MODE_FIXED:
            m_fFractionInterToStopMoving  = 0.05f;
            m_fFractionInterToStopCatchUp = 0.95f;
            return;
        }

        if (m_bPlayerWasOnBike && newCamMode == MODE_FOLLOWPED) {
            if (activeCamMode == MODE_CAM_ON_A_STRING) {
                m_nTransitionDuration         = 800;
                m_fFractionInterToStopMoving  = 0.02f;
                m_fFractionInterToStopCatchUp = 0.98f;
                return;
            }
        } else {
            switch (newCamMode) {
            case MODE_CAM_ON_A_STRING:
            case MODE_BEHINDBOAT:
                if (notsa::contains({ MODE_SNIPER_RUNABOUT, MODE_ROCKETLAUNCHER_RUNABOUT, MODE_ROCKETLAUNCHER_RUNABOUT_HS, MODE_1STPERSON_RUNABOUT, MODE_M16_1STPERSON_RUNABOUT, MODE_FIGHT_CAM_RUNABOUT, MODE_CAMERA }, activeCamMode)) {
                    m_fFractionInterToStopMoving  = 0.0f;
                    m_fFractionInterToStopCatchUp = 1.0f;
                    m_nTransitionDuration         = 1;
                    return;
                }
                break;
            case MODE_AIMWEAPON:
                m_fFractionInterToStopMoving  = 0.0f; // dword_B70044 ?
                m_fFractionInterToStopCatchUp = 1.0f; // *&dword_8CCCC0
                m_nTransitionDuration         = 400;  // dword_8CCCBC
                targetCoorsDuration           = 350;
                return;
            }

            if (!notsa::contains({ MODE_FOLLOWPED, MODE_SYPHON_CRIM_IN_FRONT, MODE_SYPHON, MODE_SPECIAL_FIXED_FOR_SYPHON }, newCamMode)) {
                m_nTransitionDuration = 1'350;
                return;
            }
        }
        if (!notsa::contains({ MODE_SYPHON_CRIM_IN_FRONT, MODE_FOLLOWPED, MODE_SYPHON, MODE_AIMWEAPON }, activeCamMode)) {
            m_nTransitionDuration = 1'350;
            return;
        }
        m_fFractionInterToStopMoving  = 0.1f;
        m_fFractionInterToStopCatchUp = 0.9f;
        m_nTransitionDuration         = 350;
        targetCoorsDuration           = 350;
    }();

    // Initialize transition state
    m_bTransitionState       = true;
    m_nTimeTransitionStart   = CTimer::GetTimeInMS();
    m_bTransitionJUSTStarted = true;

    // Store starting interpolation values
    if (m_bDoingSpecialInterp) {
        m_vecStartingSourceForInterPol = m_vecSourceDuringInter;
        m_vecStartingTargetForInterPol = m_vecTargetDuringInter;
        m_vecStartingUpForInterPol     = m_vecUpDuringInter;
        m_fStartingAlphaForInterPol    = m_fAlphaDuringInterPol;
        m_fStartingBetaForInterPol     = m_fBetaDuringInterPol;
    } else {
        m_vecStartingSourceForInterPol = activeCam.m_vecSource;
        m_vecStartingTargetForInterPol = activeCam.m_vecTargetCoorsForFudgeInter;
        m_vecStartingUpForInterPol     = activeCam.m_vecUp;
        m_fStartingAlphaForInterPol    = activeCam.m_fTrueAlpha;
        m_fStartingBetaForInterPol     = activeCam.m_fTrueBeta;
    }

    // Update active camera parameters
    activeCam.m_bCamLookingAtVector     = m_bLookingAtVector;
    activeCam.m_vecCamFixedModeVector   = m_vecFixedModeVector;
    activeCam.m_vecCamFixedModeSource   = m_vecFixedModeSource;
    activeCam.m_vecCamFixedModeUpOffSet = m_vecFixedModeUpOffSet;
    activeCam.m_nMode                   = newCamMode;
    CEntity::ChangeEntityReference(activeCam.m_pCamTargetEntity, m_pTargetEntity);

    // Store starting speeds
    m_fStartingFOVForInterPol    = activeCam.m_fFOV;
    m_vecSourceSpeedAtStartInter = activeCam.m_vecSourceSpeedOverOneFrame;
    m_vecTargetSpeedAtStartInter = activeCam.m_vecTargetSpeedOverOneFrame;
    m_vecUpSpeedAtStartInter     = activeCam.m_vecUpOverOneFrame;
    m_fAlphaSpeedAtStartInter    = activeCam.m_fAlphaSpeedOverOneFrame;
    m_fBetaSpeedAtStartInter     = activeCam.m_fBetaSpeedOverOneFrame;
    m_fFOVSpeedAtStartInter      = activeCam.m_fFovSpeedOverOneFrame;

    // Setup final transition parameters
    if (m_bLookingAtPlayer) {
        m_fFractionInterToStopMovingTarget  = 0.0f;
        m_fFractionInterToStopCatchUpTarget = 1.0f;
        m_nTransitionDurationTargetCoors    = targetCoorsDuration;
    } else {
        if (m_bScriptParametersSetForInterp) {
            m_fFractionInterToStopMoving  = m_fScriptPercentageInterToStopMoving;
            m_fFractionInterToStopCatchUp = m_fScriptPercentageInterToCatchUp;
            m_nTransitionDuration         = m_nScriptTimeForInterpolation;
        }
        m_nTransitionDurationTargetCoors    = m_nTransitionDuration;
        m_fFractionInterToStopMovingTarget  = m_fFractionInterToStopMoving;
        m_fFractionInterToStopCatchUpTarget = m_fFractionInterToStopCatchUp;
    }
}

auto CCamera::GetFrustumPoints() -> std::array<CVector, 5> {
    CVector pts[5]{};

    // First, the corners
    const auto farPlane  = RwCameraGetFarClipPlane(m_pRwCamera);
    const auto farVWSize = CVector2D{ *RwCameraGetViewWindow(m_pRwCamera) } * farPlane;
    const auto corners   = CRect{ -farVWSize, farVWSize }.GetCorners3D(farPlane);

    // Copy it into pts
    rng::copy(corners, pts);

    // Last is the center point
    pts[4] = CVector{ 0.f, 0.f, 0.f };

    // Transform them to world space
    RwV3dTransformPoints(pts, pts, 5, GetRwMatrix());

    // top left, top right, bottom right, bottom left, center
    return std::to_array(pts);
}

