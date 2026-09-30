#include "StdInc.h"
#include <set>
#include <fstream>

#include <extensions/CommandLine.h>

// Audio
// -- General
#include "AEAudioEnvironment.h"
#include "AEAudioUtility.h"
#include "AESmoothFadeThread.h"
#include "AESound.h"
#include "AEStreamThread.h"
#include "AudioEngine.h"
#include "AudioZones.h"
// -- Entities
#include "AEAudioEntity.h"
#include "AECollisionAudioEntity.h"
#include "AEDoorAudioEntity.h"
#include "AEExplosionAudioEntity.h"
#include "AEFireAudioEntity.h"
#include "AEFrontendAudioEntity.h"
#include "AEGlobalWeaponAudioEntity.h"
#include "AEPedAudioEntity.h"
#include "AEPedlessSpeechAudioEntity.h"
#include "AEPedSpeechAudioEntity.h"
#include "AEPedWeaponAudioEntity.h"
#include "AEPoliceScannerAudioEntity.h"
#include "AEScriptAudioEntity.h"
#include "AETwinLoopSoundEntity.h"
#include "AEVehicleAudioEntity.h"
#include "AEWaterCannonAudioEntity.h"
#include "AEWeaponAudioEntity.h"
#include "AEWeatherAudioEntity.h"
// -- Hardware
#include "AEAudioChannel.h"
#include "AEAudioHardware.h"
#include "AEStaticChannel.h"
#include "AEStreamingChannel.h"
// -- Loaders
#include "AEBankLoader.h"
#include "AEDataStream.h"
#include "AEMFDecoder.h"
#include "AEMP3BankLoader.h"
#include "AEMP3TrackLoader.h"
#include "AEStreamingDecoder.h"
#include "AEStreamTransformer.h"
#include "AETrackLoader.h"
#include "AEVorbisDecoder.h"
#include "AEWaveDecoder.h"
#include "AEWMADecoder.h"
// -- Managers
#include "AEAmbienceTrackManager.h"
#include "AECutsceneTrackManager.h"
#include "AERadioTrackManager.h"
#include "AESoundManager.h"
#include "AEUserRadioTrackManager.h"


#include "Garage.h"
#include "Garages.h"

// FX
#include "FxSystemBP.h"
#include "FxSystem.h"
#include "FxSphere.h"
#include "FxPrimBP.h"
#include "FxPrim.h"
#include "FxMemoryPool.h"
#include "FxManager.h"
#include "FxFrustumInfo.h"
#include "FxEmitterPrt.h"
#include "FxEmitterBP.h"
#include "FxEmitter.h"
#include "Fx.h"

#include "UIRenderer.h"

#include "ControllerConfigManager.h"
#include "CarGenerator.h"
#include "TheCarGenerators.h"
#include "Radar.h"
#include "WaterCannons.h"
#include "Lines.h"
#include "Escalators.h"
#include "MovingThings.h"
#include "PlaneTrail.h"
#include "PlaneTrails.h"
#include "Gamma.h"
#include "CustomBuildingPipeline.h"
#include "CustomBuildingDNPipeline.h"
#include "CustomCarEnvMapPipeline.h"
#include "CustomBuildingRenderer.h"
#include "PedType.h"
#include "Occlusion.h"
#include "Occluder.h"
#include "ActiveOccluder.h"
#include "MotionBlurStreaks.h"
#include "GroupEventHandler.h"
#include "SimpleVariablesSaveStructure.h"
#include "BreakManager_c.h"
#include "Buoyancy.h"
#include "CreepingFire.h"
#include "Restart.h"
#include "SetPiece.h"
#include "SetPieces.h"
#include "BulletInfo.h"
#include "Explosion.h"
#include "C_PcSave.h"
#include "FireManager.h"
#include "Skidmarks.h"
#include "CarCtrl.h"
#include "TagManager.h"
#include "Clouds.h"
#include "StuntJumpManager.h"
#include "CustomCarPlateMgr.h"
#include "WindModifiers.h"
#include "PedPlacement.h"
#include "UpsideDownCarCheck.h"
#include "PedClothesDesc.h"
#include "WaterCannon.h"
#include "EntryExitManager.h"
#include "ObjectSaveStructure.h"
#include "PedStats.h"
#include "TrafficLights.h"
#include "CurrentVehicle.h"
#include "PlaceName.h"
#include "UserDisplay.h"
#include "OnscreenTimer.h"
#include "OnscreenCounterEntry.h"
#include "PostEffects.h"
#include "Rope.h"
#include "Ropes.h"
#include "FallingGlassPane.h"
#include "Glass.h"
#include "FurnitureGroup_c.h"
#include "FurnitureManager_c.h"
#include "FurnitureSubGroup_c.h"
#include "Interior_c.h"
#include "InteriorGroup_c.h"
#include "InteriorManager_c.h"
#include "Checkpoint.h"
#include "Checkpoints.h"
#include "Ragdoll/IKChain.h"
#include "Ragdoll/IKChainManager.h"
#include "Ragdoll/BoneNode.h"
#include "Ragdoll/BoneNodeManager.h"
#include "TheScripts.h"
#include "RunningScript.h"
#include "Scripted2dEffects.h"
#include "ScriptResourceManager.h"
#include "PedAttractorManager.h"
#include "Attractors/PedAttractorPedPlacer.h"
#include "LoadingScreen.h"
#include "Securom.h"
#include "GridRef.h"
#include "MenuSystem.h"
#include "InterestingEvents.h"
#include "Shadows.h"
#include "ShadowCamera.h"
#include "VehicleRecording.h"
#include "Birds.h"
#include "Hud.h"
#include "CarFXRenderer.h"
#include "Curves.h"
#include "ProcObjectMan.h"
#include "ProcSurfaceInfo.h"
#include "Pickup.h"
#include "Pickups.h"
#include "PedIK.h"
#include "HandShaker.h"
#include "TempColModels.h"
#include "Conversations.h"
#include "DecisionMakers/DecisionMakerTypesFileLoader.h"

// Plant
#include "PlantMgr.h"
#include "PlantColEntEntry.h"
#include "PlantLocTri.h"
#include "GrassRenderer.h"
#include "PPTriPlantBuffer.h"
#include "PlantSurfPropMgr.h"

// Task ped group default allocators
#include <Tasks/Allocators/PedGroup/PedGroupDefaultTaskAllocatorFollowAnyMeans.h>
#include <Tasks/Allocators/PedGroup/PedGroupDefaultTaskAllocatorFollowLimited.h>
#include <Tasks/Allocators/PedGroup/PedGroupDefaultTaskAllocatorStandStill.h>
#include <Tasks/Allocators/PedGroup/PedGroupDefaultTaskAllocatorChat.h>
#include <Tasks/Allocators/PedGroup/PedGroupDefaultTaskAllocatorSitInLeaderCar.h>
#include <Tasks/Allocators/PedGroup/PedGroupDefaultTaskAllocatorRandom.h>
#include <Tasks/Allocators/PedGroup/PedGroupDefaultTaskAllocators.h>

#include <Tasks/Allocators/TaskAllocatorKillThreatsBasic.h>
#include <Tasks/Allocators/TaskAllocatorKillThreatsDriveby.h>
#include <Tasks/Allocators/TaskAllocatorKillThreatsBasicRandomGroup.h>
#include <Tasks/Allocators/TaskAllocatorKillOnFoot.h>
#include <Tasks/Allocators/TaskAllocatorAttack.h>
#include <Tasks/Allocators/TaskAllocatorPlayerCommandAttack.h>

// Tasks
#include "TaskSimpleKillPedWithCar.h"
#include "TaskComplexSitDownThenIdleThenStandUp.h"
#include "TaskComplexLeaveCarAsPassengerWait.h"
#include "TaskComplexInvestigateDeadPed.h"
#include "TaskComplexSignalAtPed.h"
#include "TaskSimpleCarCloseDoorFromOutside.h"
#include "TaskSimpleCarFallOut.h"
#include "TaskSimpleCarGetIn.h"
#include "TaskSimpleBeHit.h"
#include "TaskSimpleHitByGunFromFront.h"
#include "TaskSimpleHitByGunFromLeft.h"
#include "TaskSimpleHitByGunFromRear.h"
#include "TaskSimpleHitByGunFromRight.h"
#include "Tasks/TaskTypes/TaskComplexKillPedOnFootMelee.h"
#include "SeekEntity/PosCalculators/EntitySeekPosCalculator.h"
#include "SeekEntity/PosCalculators/EntitySeekPosCalculatorStandard.h"
#include "SeekEntity/PosCalculators/EntitySeekPosCalculatorRadiusAngleOffset.h"
#include "SeekEntity/PosCalculators/EntitySeekPosCalculatorXYOffset.h"
#include "SeekEntity/PosCalculators/EntitySeekPosCalculatorFixedPos.h"
#include "TaskComplexSeekEntityAnyMeansXYOffset.h"
#include "TaskComplexFleeEntity.h"
#include "TaskComplexSmartFleeEntity.h"
#include "TaskSimpleArrestPed.h"
#include "TaskComplexDragPedFromCar.h"
#include "TaskComplexDestroyCar.h"
#include "TaskComplexDestroyCarMelee.h"
#include "TaskComplexDestroyCarArmed.h"
#include "TaskSimpleAbseil.h"
#include "TaskComplexWanderCop.h"
#include "TaskComplexEnterCarAsPassengerTimed.h"
#include "TaskSimpleWaitUntilPedIsInCar.h"
#include "TaskComplexUseMobilePhone.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleCarDrive.h"
#include "TaskSimpleScratchHead.h"
#include "TaskSimpleChoking.h"
#include "TaskComplexPartnerChat.h"
#include "TaskSimpleUseGun.h"
#include "TaskSimpleGangDriveBy.h"
#include "TaskComplexObserveTrafficLightsAndAchieveHeading.h"
#include "Interior/TaskInteriorGoToInfo.h"
#include "TaskSimpleInAir.h"
#include "TaskComplexHitPedWithCar.h"
#include "TaskSimplePlayerOnFoot.h"
#include "TaskComplexStealCar.h"
#include "TaskComplexFollowPedFootsteps.h"
#include "Interior/TaskInteriorSitAtDesk.h"
#include "TaskComplexFollowLeaderAnyMeans.h"
#include "TaskSimpleFightingControl.h"
#include "TaskSimpleFinishBrain.h"
#include "TaskComplexGetUpAndStandStill.h"
#include "TaskComplexGoPickUpEntity.h"
#include "TaskSimpleDie.h"
#include "TaskSimpleDuck.h"
#include "TaskComplexPolicePursuit.h"
#include "TaskSimpleFacial.h"
#include "TaskComplexCopInCar.h"
#include "TaskComplexFacial.h"
#include "TaskComplexPlayHandSignalAnim.h"
#include "TaskComplexInWater.h"
#include "TaskComplexInAirAndLand.h"
#include "TaskSimpleGetUp.h"
#include "TaskSimpleUninterruptable.h"
#include "TaskComplexFallAndGetUp.h"
#include "TaskComplexFollowNodeRoute.h"
#include "TaskComplexFollowLeaderInFormation.h"
#include "TaskSimpleTriggerLookAt.h"
#include "TaskSimpleHitHead.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Tasks/TaskTypes/TaskComplexKillPedGroupOnFoot.h"
#include "TaskSimpleLand.h"
#include "TaskSimpleJetPack.h"
#include "TaskSimpleSitIdle.h"
#include "Interior/TaskInteriorUseInfo.h"
#include "TaskSimpleSetStayInSamePlace.h"
#include "TaskSimpleJump.h"
#include "TaskSimpleFall.h"
#include "TaskSimpleClimb.h"
#include "TaskComplexPlayHandSignalAnim.h"
#include "TaskComplexWaitForPed.h"
#include "TaskComplexJump.h"
#include "TaskComplexWanderStandard.h"
#include "TaskComplexSunbathe.h"
#include "TaskComplexEnterCar.h"
#include "Interior/TaskInteriorBeInShop.h"
#include "TaskSimpleTogglePedThreatScanner.h"
#include "TaskComplexUseGoggles.h"
#include "TaskComplexUseEffectRunning.h"
#include "TaskComplexUseEffectSprinting.h"
#include "TaskComplexCrossRoadLookAndAchieveHeading.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskSimpleAchieveHeading.h"
#include "TaskComplexWalkAlongsidePed.h"
#include "TaskSimpleCarGoToPointNearDoorUntilDoorNotInUse.h"
#include "Interior/TaskInteriorBeInOffice.h"
#include "TaskSimpleGiveCPR.h"
#include "TaskSimpleCarSetPedInAsPassenger.h"
#include "TaskComplexDriveFireTruck.h"
#include "Interior/TaskInteriorLieInBed.h"
#include "TaskSimpleSwim.h"
#include "TaskComplexWalkRoundObject.h"
#include "TaskSimplePause.h"
#include "TaskComplexShuffleSeats.h"
#include "TaskSimpleCarShuffle.h"
#include "TaskSimpleHurtPedWithCar.h"
#include "TaskComplexPassObject.h"
#include "TaskComplexEnterCarAsPassenger.h"
#include "TaskComplexEnterCarAsDriver.h"
#include "TaskSimpleCarShuffle.h"
#include "TaskSimpleCarSlowBeDraggedOut.h"
#include "TaskComplexReactToGunAimedAt.h"
#include "TaskSimpleNone.h"
#include "TaskComplexKillPedOnFoot.h"
#include "TaskSimpleThrowProjectile.h"
#include "TaskSimpleGoToPoint.h"
#include "TaskComplexWanderMedic.h"
#include "TaskComplexEvasiveDiveAndGetUp.h"
#include "TaskComplexMoveBackAndJump.h"
#include "TaskComplexSeekEntityAiming.h"
#include "TaskSimpleCarDriveTimed.h"
#include "TaskComplexKillPedOnFootStealth.h"
#include "TaskComplexDriveWander.h"
#include "TaskSimpleStealthKill.h"
#include "TaskSimpleCarSetPedOut.h"
#include "TaskSimpleAnim.h"
#include "TaskSimpleRunAnim.h"
#include "Tasks/TaskTypes/TaskComplexInvestigateDisturbance.h"
#include "TaskComplexWanderCriminal.h"
#include "TaskComplexWanderProstitute.h"
#include "TaskComplexWalkRoundBuildingAttempt.h"
#include "TaskSimpleLookAbout.h"
#include "TaskComplexEvasiveDiveAndGetUp.h"
#include "TaskComplexPartnerGreet.h"
#include "Interior/TaskInteriorShopKeeper.h"
#include "TaskComplexWaitForDryWeather.h"
#include "TaskSimpleShakeFist.h"
#include "TaskComplexFleeShooting.h"
#include "TaskComplexWalkRoundCar.h"
#include "TaskSimpleDuck.h"
#include "TaskComplexLeaveCarAndFlee.h"
#include "TaskComplexLeaveCarAndWander.h"
#include "TaskComplexMedicTreatInjuredPed.h"
#include "TaskComplexDiveFromAttachedEntityAndGetUp.h"
#include "TaskComplexKillCriminal.h"
#include "SeekEntity/TaskComplexSeekEntityRadiusAngleOffset.h"
#include "SeekEntity/TaskComplexSeekEntityXYOffset.h"
#include "SeekEntity/TaskComplexSeekEntityStandard.h"
#include "TaskComplexUseScriptedBrain.h"
#include "TaskSimplePlayHandSignalAnim.h"
#include "TaskComplexCarDrive.h"
#include "TaskComplexKillPedFromBoat.h"
#include "TaskSimpleHitWall.h"
#include "TaskComplexLeaveCar.h"
#include "TaskSimpleCarSetTempAction.h"
#include "TaskComplexWanderFlee.h"
#include "TaskComplexWaitAtAttractor.h"
#include "TaskComplexTreatAccident.h"
#include "TaskComplexGoToPointAndStandStillTimed.h"
#include "TaskComplexPartnerShove.h"
#include "TaskComplexFleeAnyMeans.h"
#include "TaskSimpleRunNamedAnim.h"
#include "TaskComplexProstituteSolicit.h"
#include "TaskComplexStuckInAir.h"
#include "TaskSimpleHoldEntity.h"
#include "TaskSimpleCower.h"
#include "TaskSimpleHitFromBack.h"
#include "TaskSimpleHitFromBehind.h"
#include "TaskSimpleHitFromFront.h"
#include "TaskSimpleHitFromLeft.h"
#include "TaskSimpleHitFromRight.h"
#include "TaskSimpleGoTo.h"
#include "TaskSimpleUseAtm.h"
#include "TaskSimpleCarOpenLockedDoorFromOutside.h"
#include "TaskSimpleCarSetPedSlowDraggedOut.h"
#include "TaskComplexPartner.h"
#include "TaskComplexFollowPointRoute.h"
#include "TaskComplexDrivePointRoute.h"
#include "TaskComplexStareAtPed.h"
#include "TaskSimpleCarSetPedInAsDriver.h"
#include "TaskComplexWaitForBus.h"
#include "TaskSimpleWaitForBus.h"
#include "TaskSimpleStandUp.h"
#include "TaskComplexWander.h"
#include "TaskSimplePutDownEntity.h"
#include "TaskComplexUseSwatRope.h"
#include "TaskComplexRoadRage.h"
#include "TaskSimpleGoToPointFine.h"
#include "TaskComplexExtinguishFires.h"
#include "TaskComplexDie.h"
#include "TaskComplexEnterBoatAsDriver.h"
#include "TaskComplexUseAttractor.h"
#include "TaskComplexUseAttractorPartner.h"
#include "TaskComplexAttractorPartnerWait.h"
#include "TaskSimpleFight.h"
#include "TaskComplexUseWaterCannon.h"
#include "TaskComplexDriveToPoint.h"
#include "TaskSimpleSetPedAsAutoDriver.h"
#include "TaskComplexWalkRoundFire.h"
#include "TaskSimpleSlideToCoord.h"
#include "TaskComplexPartnerDeal.h"
#include "TaskSimplePickUpEntity.h"
#include "TaskComplexEnterCarAsDriverTimed.h"
#include "TaskComplexBeInGroup.h"
#include "TaskComplexBeCop.h"
#include "TaskComplexAvoidOtherPedWhileWandering.h"
#include "TaskComplexArrestPed.h"
#include "TaskComplexEnterAnyCarAsDriver.h"
#include "TaskSimpleSetKindaStayInSamePlace.h"
#include "TaskSimpleSetCharIgnoreWeaponRangeFlag.h"
#include "TaskComplexCarDriveMission.h"
#include "TaskComplexLeaveAnyCar.h"
#include "TaskSimpleLeaveGroup.h"
#include "TaskSimpleCreateCarAndGetIn.h"
#include "TaskComplexWaitForBackup.h"
#include "TaskSimpleIKChain.h"
#include "TaskSimpleIKLookAt.h"
#include "TaskSimplePickUpBike.h"
#include "TaskSimpleIKManager.h"
#include "TaskSimpleGunControl.h"
#include "TaskSimpleIKPointArm.h"
#include "TaskSimpleCarSlowDragPedOut.h"
#include "TaskSimpleWaitUntilPedIsOutCar.h"
#include "TaskGangHasslePed.h"
#include "TaskGangHassleVehicle.h"
#include "TaskGoToVehicleAndLean.h"
#include "TaskSimpleSetCharDecisionMaker.h"
#include "TaskLeanOnVehicle.h"
#include "TaskComplexSeekCoverUntilTargetDead.h"
#include "TaskSimpleTurn180.h"
#include "TaskComplexCarSlowBeDraggedOut.h"
#include "TaskComplexCarSlowBeDraggedOutAndStandUp.h"
#include "TaskComplexGangFollower.h"
#include "TaskComplexGangJoinRespond.h"
#include "TaskComplexGangLeader.h"
#include "TaskComplexGetOnBoatSeat.h"
#include "TaskComplexGoToAttractor.h"
#include "TaskComplexGoToBoatSteeringWheel.h"
#include "TaskComplexUseScriptedAttractor.h"
#include "TaskSimpleSitDown.h"
#include "TaskComplexGoToCarDoorAndStandStill.h"
#include "TaskComplexGoToPointAiming.h"
#include "TaskComplexGoToPointAndStandStillAndAchieveHeading.h"
#include "TaskComplexGoToPointAnyMeans.h"
#include "TaskComplexGoToPointShooting.h"
#include "TaskComplexGotoDoorAndOpen.h"
#include "TaskComplexHitByGunResponse.h"
#include "TaskComplexHitResponse.h"
#include "TaskSimpleDrown.h"
#include "TaskComplexDieInCar.h"
#include "TaskComplexFallToDeath.h"
#include "TaskSimpleDrownInCar.h"
#include "TaskSimpleDuckToggle.h"
#include "TaskSimpleThrowControl.h"
#include "TaskSimpleDieInCar.h"
#include "TaskComplexTurnToFaceEntityOrCoord.h"
#include "TaskComplexTrackEntity.h"
#include "TaskSimpleTired.h"
#include "TaskSimpleAffectSecondaryBehaviour.h"
#include "TaskSimpleCarWaitForDoorNotToBeInUse.h"
#include "TaskSimpleCarWaitToSlowDown.h"
#include "TaskSimpleCarAlign.h"
#include "TaskComplexLeaveBoat.h"
#include "TaskSimpleCarGetOut.h"
#include "TaskSimpleTired.h"
#include "TaskSimpleCarOpenDoorFromOutside.h"
#include "TaskSimpleCarJumpOut.h"
#include "TaskSimpleCarCloseDoorFromInside.h"
#include "TaskSimpleClearLookAt.h"
#include "TaskSimpleCarWaitToSlowDown.h"    
#include "TaskComplexFleePoint.h"
#include "TaskSimpleWaitUntilAreaCodesMatch.h"
#include "TaskSimplePlayerOnFire.h"
#include "TaskComplexOnFire.h"
#include "TaskSimpleBikeJacked.h"
#include "TaskSimpleRunTimedAnim.h"
#include "TaskSimpleSay.h"
#include "TaskComplexEnterCar.h"
#include "TaskComplexLeaveCarAndDie.h"
#include "TaskSimpleHandsUp.h"
#include "TaskComplexEnterCar.h"
#include "TaskComplexSmartFleePoint.h"
#include "Interior/TaskInteriorBeInHouse.h"
#include "Tasks/TaskTypes/TaskComplexKillPedOnFootArmed.h"
#include "Tasks/TaskTypes/TaskSimpleWaitUntilLeaderAreaCodesMatch.h"
#include "Tasks/TaskTypes/TaskComplexUseClosestFreeScriptedAttractor.h"
#include "Tasks/TaskTypes/TaskComplexUseClosestFreeScriptedAttractorRun.h"
#include "Tasks/TaskTypes/TaskComplexUseClosestFreeScriptedAttractorSprint.h"

// Interior Tasks
#include "Tasks/TaskTypes/Interior/TaskInteriorUseInfo.h"
#include "TaskComplexSmartFleePoint.h"

#include "EventSeenPanickedPed.h"
#include "EventCarUpsideDown.h"
#include "EventPassObject.h"
#include "EventLeanOnVehicle.h"
#include "EventOnFire.h"
#include "EventEscalator.h"
#include "EventCopCarBeingStolen.h"
#include "EventDanger.h"
#include "EventSeenCop.h"
#include "EventScriptedAttractor.h"
#include "EventStuckInAir.h"
#include "EventDontJoinPlayerGroup.h"
#include "EventNewGangMember.h"
#include "EventAcquaintancePedHate.h"
#include "EventAcquaintancePedHateBadlyLit.h"
#include "EventPedCollisionWithPed.h"
#include "EventPedCollisionWithPlayer.h"
#include "EventPlayerCollisionWithPed.h"
#include "EventObjectCollision.h"
#include "EventBuildingCollision.h"
#include "EventLeaderEnteredCarAsDriver.h"
#include "EventLeaderExitedCarAsDriver.h"
#include "EventLeaderQuitEnteringCarAsDriver.h"
#include "EventAreaCodes.h"
#include "EventLeaderEntryExit.h"
#include "Formation.h"
#include "ProjectileInfo.h"

#include "Plugins/BreakablePlugin/BreakablePlugin.h"

#include "platform/win/VideoPlayer/VideoPlayer.h"
#include "platform/win/VideoMode.h"
#include "platform/win/WinPlatform.h"

#include "app/app.h"
#include <RealTimeShadowManager.h>

#include "extensions/utility.hpp"
#include "extensions/CommandLine.h"
#include <RenderBuffer.hpp>

#include "reversiblehooks/RootHookCategory.h"

#include "WindowedMode.hpp"


// NOTSA(bisection): classes listed in hooks_off.txt (one per line, next to the asi)
// are NOT hooked at all - the original exe code stays in place. Used to bisect
// reversals: disable half, test, narrow down.
static std::set<std::string>& HookOffList() {
    static std::set<std::string> s = [] {
        std::set<std::string> r;
        char cwdbuf[MAX_PATH]; GetModuleFileNameA(nullptr, cwdbuf, MAX_PATH);
        spdlog::warn("NOTSA(bisection) hooks_off.txt load: cwd={}", std::filesystem::current_path().string());
        if (std::ifstream f{ "hooks_off.txt" }) {
            for (std::string l; std::getline(f, l);) {
                while (!l.empty() && (l.back() == '' || l.back() == ' ')) l.pop_back();
                if (!l.empty()) r.insert(l);
            }
        }
        spdlog::warn("NOTSA(bisection) hooks_off entries: {}", r.size());
        return r;
    }();
    return s;
}
static bool IsHookDisabled(const char* cls) { return HookOffList().count(cls) != 0; }

void RwD3D9DriverInjectHooks(); // RenderWare D3D9 driver core (rwd3d9driver.cpp)

void InjectHooksMain() {
    /**
    * We have `NOTSA_STANDALONE` macro to be able to dump all hooks without actually writing to memory,
    * it is used by the CI to automatically update the docs.
    * So don't do anything game related here if that macro is set.
    **/

    #ifndef NOTSA_STANDALONE
        HookInstall(0x53E230, &Render2dStuff);   // [ImGui] This one shouldn't be reversible, it contains imgui debug menu logic, and makes game unplayable without
        HookInstall(0x541DD0, CPad::UpdatePads); // [ImGui] Changes logic of the function and shouldn't be toggled on/off
        HookInstall(0x459F70, CVehicleRecording::Render); // [ImGui] Debug stuff rendering

        #ifdef NOTSA_WINDOWED_MODE
            notsa::InjectWindowedModeHooks();
        #endif
    #endif

    if (!IsHookDisabled("CDoor")) CDoor::InjectHooks();
    if (!IsHookDisabled("CControllerConfigManager")) CControllerConfigManager::InjectHooks();
    if (!IsHookDisabled("CFormation")) CFormation::InjectHooks();
    if (!IsHookDisabled("CHandShaker")) CHandShaker::InjectHooks();
    if (!IsHookDisabled("CCutsceneMgr")) CCutsceneMgr::InjectHooks();
    if (!IsHookDisabled("CFileMgr")) CFileMgr::InjectHooks();
    if (!IsHookDisabled("CPedGroupPlacer")) CPedGroupPlacer::InjectHooks();
    if (!IsHookDisabled("CLoadedCarGroup")) CLoadedCarGroup::InjectHooks();
    if (!IsHookDisabled("RenderBuffer")) RenderBuffer::InjectHooks();
    if (!IsHookDisabled("CStaticShadow")) CStaticShadow::InjectHooks();
    if (!IsHookDisabled("CPedGroup")) CPedGroup::InjectHooks();
    if (!IsHookDisabled("CPedGroupMembership")) CPedGroupMembership::InjectHooks();
    if (!IsHookDisabled("CRealTimeShadowManager")) CRealTimeShadowManager::InjectHooks();
    if (!IsHookDisabled("CRealTimeShadow")) CRealTimeShadow::InjectHooks();
    if (!IsHookDisabled("CPopCycle")) CPopCycle::InjectHooks();
    if (!IsHookDisabled("ProcObjectMan_c")) ProcObjectMan_c::InjectHooks();
    if (!IsHookDisabled("ProcSurfaceInfo_c")) ProcSurfaceInfo_c::InjectHooks();
    RwHelperInjectHooks();
    RwCoreInjectHooks();
    // RwPlCoreInjectHooks(); // NIGHT-DISABLED (bisect)
    // RpMatFXInjectHooks(); // NIGHT-DISABLED (bisect)
    // RwD3D9DriverInjectHooks(); // NIGHT-DISABLED (bisect)
    if (!IsHookDisabled("CPad")) CPad::InjectHooks();
    InjectCommonHooks();
    if (!IsHookDisabled("CEscalator")) CEscalator::InjectHooks();
    if (!IsHookDisabled("CShadowCamera")) CShadowCamera::InjectHooks();
    if (!IsHookDisabled("CInterestingEvents")) CInterestingEvents::InjectHooks();
    if (!IsHookDisabled("CShadows")) CShadows::InjectHooks();
    if (!IsHookDisabled("CCam")) CCam::InjectHooks();
    if (!IsHookDisabled("CCarAI")) CCarAI::InjectHooks();
    if (!IsHookDisabled("CMenuSystem")) CMenuSystem::InjectHooks();
    if (!IsHookDisabled("CCarFXRenderer")) CCarFXRenderer::InjectHooks();
    if (!IsHookDisabled("CPedAttractorManager")) CPedAttractorManager::InjectHooks();
    if (!IsHookDisabled("CPedAttractorPedPlacer")) CPedAttractorPedPlacer::InjectHooks();
    if (!IsHookDisabled("BoneNode_c")) BoneNode_c::InjectHooks();
    if (!IsHookDisabled("BoneNodeManager_c")) BoneNodeManager_c::InjectHooks();
    if (!IsHookDisabled("IKChainManager_c")) IKChainManager_c::InjectHooks();
    if (!IsHookDisabled("IKChain_c")) IKChain_c::InjectHooks();
    if (!IsHookDisabled("CCheckpoint")) CCheckpoint::InjectHooks();
    if (!IsHookDisabled("CCheckpoints")) CCheckpoints::InjectHooks();
    if (!IsHookDisabled("CWeaponEffects")) CWeaponEffects::InjectHooks();
    if (!IsHookDisabled("CPedList")) CPedList::InjectHooks();
    if (!IsHookDisabled("CBouncingPanel")) CBouncingPanel::InjectHooks();
    if (!IsHookDisabled("CRope")) CRope::InjectHooks();
    if (!IsHookDisabled("CRopes")) CRopes::InjectHooks();
    if (!IsHookDisabled("CWeaponInfo")) CWeaponInfo::InjectHooks();
    if (!IsHookDisabled("CCurrentVehicle")) CCurrentVehicle::InjectHooks();
    if (!IsHookDisabled("CCurves")) CCurves::InjectHooks();
    if (!IsHookDisabled("CPlaceName")) CPlaceName::InjectHooks();
    if (!IsHookDisabled("CUserDisplay")) CUserDisplay::InjectHooks();
    if (!IsHookDisabled("COnscreenTimer")) COnscreenTimer::InjectHooks();
    if (!IsHookDisabled("COnscreenCounterEntry")) COnscreenCounterEntry::InjectHooks();
    if (!IsHookDisabled("CPedPlacement")) CPedPlacement::InjectHooks();
    if (!IsHookDisabled("CWindModifiers")) CWindModifiers::InjectHooks();
    if (!IsHookDisabled("CGenericGameStorage")) CGenericGameStorage::InjectHooks();
    if (!IsHookDisabled("C_PcSave")) C_PcSave::InjectHooks();
    if (!IsHookDisabled("CFileLoader")) CFileLoader::InjectHooks();
    if (!IsHookDisabled("CWorld")) CWorld::InjectHooks();
    if (!IsHookDisabled("CStreamingInfo")) CStreamingInfo::InjectHooks();
    if (!IsHookDisabled("CStreaming")) CStreaming::InjectHooks();
    InjectCdStreamHooks();
    if (!IsHookDisabled("CMirrors")) CMirrors::InjectHooks();
    if (!IsHookDisabled("CFire")) CFire::InjectHooks();
    if (!IsHookDisabled("CExplosion")) CExplosion::InjectHooks();
    if (!IsHookDisabled("CClothesBuilder")) CClothesBuilder::InjectHooks();
    if (!IsHookDisabled("CClothes")) CClothes::InjectHooks();
    if (!IsHookDisabled("CBulletInfo")) CBulletInfo::InjectHooks();
    if (!IsHookDisabled("CRestart")) CRestart::InjectHooks();
    if (!IsHookDisabled("CSetPiece")) CSetPiece::InjectHooks();
    if (!IsHookDisabled("CSetPieces")) CSetPieces::InjectHooks();
    if (!IsHookDisabled("CCopPed")) CCopPed::InjectHooks();
    if (!IsHookDisabled("CCivilianPed")) CCivilianPed::InjectHooks();
    if (!IsHookDisabled("CDamageManager")) CDamageManager::InjectHooks();
    if (!IsHookDisabled("CCreepingFire")) CCreepingFire::InjectHooks();
    if (!IsHookDisabled("BreakManager_c")) BreakManager_c::InjectHooks();
    if (!IsHookDisabled("BreakObject_c")) BreakObject_c::InjectHooks();
    if (!IsHookDisabled("CFireManager")) CFireManager::InjectHooks();
    if (!IsHookDisabled("CGroupEventHandler")) CGroupEventHandler::InjectHooks();
    if (!IsHookDisabled("CVehicleRecording")) CVehicleRecording::InjectHooks();
    if (!IsHookDisabled("CBrightLights")) CBrightLights::InjectHooks();
    if (!IsHookDisabled("CShinyTexts")) CShinyTexts::InjectHooks();
    if (!IsHookDisabled("CPedTaskPair")) CPedTaskPair::InjectHooks();
    if (!IsHookDisabled("CDirectory")) CDirectory::InjectHooks();
    if (!IsHookDisabled("CBulletTraces")) CBulletTraces::InjectHooks();
    if (!IsHookDisabled("CDebug")) CDebug::InjectHooks();
    if (!IsHookDisabled("CGame")) CGame::InjectHooks();
    if (!IsHookDisabled("CControllerState")) CControllerState::InjectHooks();
    if (!IsHookDisabled("CKeyboardState")) CKeyboardState::InjectHooks();
    if (!IsHookDisabled("CMouseControllerState")) CMouseControllerState::InjectHooks();
    if (!IsHookDisabled("CRect")) CRect::InjectHooks();

    /******* Don't hook these, performance is bad ********
    /* CVector::InjectHooks();
    /* CVector2D::InjectHooks();
    /* CQuaternion::InjectHooks();
    /* CMatrix::InjectHooks();
    /****************************************************/

    if (!IsHookDisabled("CMatrixLink")) CMatrixLink::InjectHooks();
    if (!IsHookDisabled("CMatrixLinkList")) CMatrixLinkList::InjectHooks();
    if (!IsHookDisabled("CEntryInfoNode")) CEntryInfoNode::InjectHooks();
    if (!IsHookDisabled("CEntryInfoList")) CEntryInfoList::InjectHooks();

    //CPtrList::InjectHooks();

    //CPtrNodeDoubleLink::InjectHooks();
    //CPtrListDoubleLink::InjectHooks();
    //
    //CPtrNodeSingleLink::InjectHooks();
    //CPtrListSingleLink::InjectHooks();

    if (!IsHookDisabled("CReferences")) CReferences::InjectHooks();
    if (!IsHookDisabled("CPopulation")) CPopulation::InjectHooks();
    if (!IsHookDisabled("CModelInfo")) CModelInfo::InjectHooks();
    if (!IsHookDisabled("CBaseModelInfo")) CBaseModelInfo::InjectHooks();
    if (!IsHookDisabled("CAtomicModelInfo")) CAtomicModelInfo::InjectHooks();
    if (!IsHookDisabled("CLodAtomicModelInfo")) CLodAtomicModelInfo::InjectHooks();
    if (!IsHookDisabled("CVehicleModelInfo")) CVehicleModelInfo::InjectHooks();
    if (!IsHookDisabled("CClumpModelInfo")) CClumpModelInfo::InjectHooks();
    if (!IsHookDisabled("CTimeModelInfo")) CTimeModelInfo::InjectHooks();
    if (!IsHookDisabled("CDamageAtomicModelInfo")) CDamageAtomicModelInfo::InjectHooks();
    if (!IsHookDisabled("CWeaponModelInfo")) CWeaponModelInfo::InjectHooks();
    if (!IsHookDisabled("CShotInfo")) CShotInfo::InjectHooks();
    if (!IsHookDisabled("CPedModelInfo")) CPedModelInfo::InjectHooks();
    if (!IsHookDisabled("CTimeInfo")) CTimeInfo::InjectHooks();
    if (!IsHookDisabled("SurfaceInfos_c")) SurfaceInfos_c::InjectHooks();
    if (!IsHookDisabled("CPlaceable")) CPlaceable::InjectHooks();
    if (!IsHookDisabled("CEntity")) CEntity::InjectHooks();
    if (!IsHookDisabled("CPhysical")) CPhysical::InjectHooks();
    if (!IsHookDisabled("CRenderer")) CRenderer::InjectHooks();
    if (!IsHookDisabled("CDummy")) CDummy::InjectHooks();
    if (!IsHookDisabled("CDummyObject")) CDummyObject::InjectHooks();
    if (!IsHookDisabled("CObject")) CObject::InjectHooks();
    if (!IsHookDisabled("CObjectData")) CObjectData::InjectHooks();
    if (!IsHookDisabled("CObjectSaveStructure")) CObjectSaveStructure::InjectHooks();
    if (!IsHookDisabled("CCutsceneObject")) CCutsceneObject::InjectHooks();
    if (!IsHookDisabled("CHandObject")) CHandObject::InjectHooks();
    if (!IsHookDisabled("CBuilding")) CBuilding::InjectHooks();
    if (!IsHookDisabled("CAnimatedBuilding")) CAnimatedBuilding::InjectHooks();
    if (!IsHookDisabled("CCompressedMatrixNotAligned")) CCompressedMatrixNotAligned::InjectHooks();
    if (!IsHookDisabled("CVisibilityPlugins")) CVisibilityPlugins::InjectHooks();
    if (!IsHookDisabled("CPed")) CPed::InjectHooks();
    if (!IsHookDisabled("CPedIntelligence")) CPedIntelligence::InjectHooks();
    if (!IsHookDisabled("CPedGroupIntelligence")) CPedGroupIntelligence::InjectHooks();
    if (!IsHookDisabled("CCollision")) CCollision::InjectHooks();
    if (!IsHookDisabled("CTempColModels")) CTempColModels::InjectHooks();
    if (!IsHookDisabled("CColSphere")) CColSphere::InjectHooks();
    if (!IsHookDisabled("CColLine")) CColLine::InjectHooks();
    if (!IsHookDisabled("CColTrianglePlane")) CColTrianglePlane::InjectHooks();
    if (!IsHookDisabled("CBox")) CBox::InjectHooks();
    if (!IsHookDisabled("CColModel")) CColModel::InjectHooks();
    if (!IsHookDisabled("CCollisionData")) CCollisionData::InjectHooks();
    CQuadTreeNode<void*>::InjectHooks();
    if (!IsHookDisabled("CColStore")) CColStore::InjectHooks();
    if (!IsHookDisabled("CColAccel")) CColAccel::InjectHooks();
    if (!IsHookDisabled("CTrafficLights")) CTrafficLights::InjectHooks();
    if (!IsHookDisabled("CMotionBlurStreaks")) CMotionBlurStreaks::InjectHooks();
    if (!IsHookDisabled("CRegisteredMotionBlurStreak")) CRegisteredMotionBlurStreak::InjectHooks();
    if (!IsHookDisabled("CCover")) CCover::InjectHooks();
    if (!IsHookDisabled("CVehicleAnimGroup")) CVehicleAnimGroup::InjectHooks();
    if (!IsHookDisabled("CVehicleAnimGroupData")) CVehicleAnimGroupData::InjectHooks();
    if (!IsHookDisabled("CPedDamageResponseCalculator")) CPedDamageResponseCalculator::InjectHooks();
    if (!IsHookDisabled("CPedScriptedTaskRecord")) CPedScriptedTaskRecord::InjectHooks();
    if (!IsHookDisabled("CPedScriptedTaskRecordData")) CPedScriptedTaskRecordData::InjectHooks();
    if (!IsHookDisabled("CPedClothesDesc")) CPedClothesDesc::InjectHooks();
    if (!IsHookDisabled("CScriptedBrainTaskStore")) CScriptedBrainTaskStore::InjectHooks();
    if (!IsHookDisabled("CTimer")) CTimer::InjectHooks();
    if (!IsHookDisabled("CTagManager")) CTagManager::InjectHooks();
    if (!IsHookDisabled("CWaterLevel")) CWaterLevel::InjectHooks();
    if (!IsHookDisabled("cBuoyancy")) cBuoyancy::InjectHooks();
    if (!IsHookDisabled("CWanted")) CWanted::InjectHooks();
    if (!IsHookDisabled("CCrime")) CCrime::InjectHooks();
    if (!IsHookDisabled("CEscalators")) CEscalators::InjectHooks();
    if (!IsHookDisabled("CWeapon")) CWeapon::InjectHooks();
    if (!IsHookDisabled("cTransmission")) cTransmission::InjectHooks();
    if (!IsHookDisabled("CPlayerPed")) CPlayerPed::InjectHooks();
    if (!IsHookDisabled("CStats")) CStats::InjectHooks();
    if (!IsHookDisabled("CCarCtrl")) CCarCtrl::InjectHooks();
    if (!IsHookDisabled("CTheZones")) CTheZones::InjectHooks();
    if (!IsHookDisabled("CMenuManager")) CMenuManager::InjectHooks();
    if (!IsHookDisabled("CSprite2d")) CSprite2d::InjectHooks();
    if (!IsHookDisabled("CCheat")) CCheat::InjectHooks();
    if (!IsHookDisabled("CBirds")) CBirds::InjectHooks();
    if (!IsHookDisabled("WaterCreature_c")) WaterCreature_c::InjectHooks();
    if (!IsHookDisabled("WaterCreatureManager_c")) WaterCreatureManager_c::InjectHooks();
    if (!IsHookDisabled("C2dEffect")) C2dEffect::InjectHooks();
    if (!IsHookDisabled("CCustomRoadsignMgr")) CCustomRoadsignMgr::InjectHooks();
    if (!IsHookDisabled("COcclusion")) COcclusion::InjectHooks();
    if (!IsHookDisabled("COccluder")) COccluder::InjectHooks();
    if (!IsHookDisabled("CActiveOccluder")) CActiveOccluder::InjectHooks();
    if (!IsHookDisabled("CGarage")) CGarage::InjectHooks();
    if (!IsHookDisabled("CGarages")) CGarages::InjectHooks();
    if (!IsHookDisabled("CPostEffects")) CPostEffects::InjectHooks();
    if (!IsHookDisabled("CIniFile")) CIniFile::InjectHooks();
    if (!IsHookDisabled("CStencilShadowObject")) CStencilShadowObject::InjectHooks();
    if (!IsHookDisabled("CStencilShadows")) CStencilShadows::InjectHooks();
    if (!IsHookDisabled("CFont")) CFont::InjectHooks();
    if (!IsHookDisabled("CGameLogic")) CGameLogic::InjectHooks();
    if (!IsHookDisabled("CStuckCarCheck")) CStuckCarCheck::InjectHooks();
    if (!IsHookDisabled("CUpsideDownCarCheck")) CUpsideDownCarCheck::InjectHooks();
    if (!IsHookDisabled("CCamera")) CCamera::InjectHooks();
    if (!IsHookDisabled("CMessages")) CMessages::InjectHooks();
    if (!IsHookDisabled("CGangWars")) CGangWars::InjectHooks();
    if (!IsHookDisabled("CPlayerPedData")) CPlayerPedData::InjectHooks();
    if (!IsHookDisabled("CTimeCycle")) CTimeCycle::InjectHooks();
    if (!IsHookDisabled("CColourSet")) CColourSet::InjectHooks();
    if (!IsHookDisabled("CSkidmarks")) CSkidmarks::InjectHooks();
    if (!IsHookDisabled("CMovingThings")) CMovingThings::InjectHooks();
    if (!IsHookDisabled("CRoadBlocks")) CRoadBlocks::InjectHooks();
    if (!IsHookDisabled("CBridge")) CBridge::InjectHooks();
    if (!IsHookDisabled("CCranes")) CCranes::InjectHooks();
    if (!IsHookDisabled("CGridRef")) CGridRef::InjectHooks();
    if (!IsHookDisabled("CShopping")) CShopping::InjectHooks();
    if (!IsHookDisabled("CInformFriendsEventQueue")) CInformFriendsEventQueue::InjectHooks();
    if (!IsHookDisabled("C3dMarkers")) C3dMarkers::InjectHooks();
    if (!IsHookDisabled("C3dMarker")) C3dMarker::InjectHooks();
    if (!IsHookDisabled("CSpecialFX")) CSpecialFX::InjectHooks();
    if (!IsHookDisabled("CFallingGlassPane")) CFallingGlassPane::InjectHooks();
    if (!IsHookDisabled("CGlass")) CGlass::InjectHooks();
    if (!IsHookDisabled("CPedGroups")) CPedGroups::InjectHooks();
    if (!IsHookDisabled("CClock")) CClock::InjectHooks();
    if (!IsHookDisabled("CClouds")) CClouds::InjectHooks();
    if (!IsHookDisabled("CCoronas")) CCoronas::InjectHooks();
    if (!IsHookDisabled("CRegisteredCorona")) CRegisteredCorona::InjectHooks();
    if (!IsHookDisabled("CCustomCarPlateMgr")) CCustomCarPlateMgr::InjectHooks();
    if (!IsHookDisabled("CDraw")) CDraw::InjectHooks();
    if (!IsHookDisabled("CEntryExitManager")) CEntryExitManager::InjectHooks();
    if (!IsHookDisabled("CEntryExit")) CEntryExit::InjectHooks();
    if (!IsHookDisabled("CInformGroupEvent")) CInformGroupEvent::InjectHooks();
    if (!IsHookDisabled("CInformGroupEventQueue")) CInformGroupEventQueue::InjectHooks();
    if (!IsHookDisabled("CGangs")) CGangs::InjectHooks();
    if (!IsHookDisabled("CPlayerInfo")) CPlayerInfo::InjectHooks();
    if (!IsHookDisabled("CReplay")) CReplay::InjectHooks();
    if (!IsHookDisabled("CDarkel")) CDarkel::InjectHooks();
    if (!IsHookDisabled("CGeneral")) CGeneral::InjectHooks();
    if (!IsHookDisabled("CGamma")) CGamma::InjectHooks();
    if (!IsHookDisabled("CHudColours")) CHudColours::InjectHooks();
    if (!IsHookDisabled("CTxdStore")) CTxdStore::InjectHooks();
    if (!IsHookDisabled("CCarEnterExit")) CCarEnterExit::InjectHooks();
    if (!IsHookDisabled("CEntityScanner")) CEntityScanner::InjectHooks();
    if (!IsHookDisabled("CPedType")) CPedType::InjectHooks();
    if (!IsHookDisabled("CAcquaintance")) CAcquaintance::InjectHooks();
    if (!IsHookDisabled("CWeather")) CWeather::InjectHooks();
    if (!IsHookDisabled("CPathFind")) CPathFind::InjectHooks();
    if (!IsHookDisabled("CPathNode")) CPathNode::InjectHooks();
    if (!IsHookDisabled("CNodeRoute")) CNodeRoute::InjectHooks();
    if (!IsHookDisabled("CLoadMonitor")) CLoadMonitor::InjectHooks();
    if (!IsHookDisabled("CDecisionMakerTypes")) CDecisionMakerTypes::InjectHooks();
    if (!IsHookDisabled("CDecisionMakerTypesFileLoader")) CDecisionMakerTypesFileLoader::InjectHooks();
    if (!IsHookDisabled("CPedStats")) CPedStats::InjectHooks();
    if (!IsHookDisabled("CCarGenerator")) CCarGenerator::InjectHooks();
    if (!IsHookDisabled("CTheCarGenerators")) CTheCarGenerators::InjectHooks();
    if (!IsHookDisabled("CCullZones")) CCullZones::InjectHooks();
    if (!IsHookDisabled("CSpecialPlateHandler")) CSpecialPlateHandler::InjectHooks();
    if (!IsHookDisabled("CKeyGen")) CKeyGen::InjectHooks();
    if (!IsHookDisabled("CAccidentManager")) CAccidentManager::InjectHooks();
    if (!IsHookDisabled("CStuntJumpManager")) CStuntJumpManager::InjectHooks();
    if (!IsHookDisabled("CLocalisation")) CLocalisation::InjectHooks();
    if (!IsHookDisabled("CSimpleVariablesSaveStructure")) CSimpleVariablesSaveStructure::InjectHooks();
    if (!IsHookDisabled("CPedGeometryAnalyser")) CPedGeometryAnalyser::InjectHooks();

    if (!IsHookDisabled("NodeNamePlugin")) NodeNamePlugin::InjectHooks();
    if (!IsHookDisabled("JPegPlugin")) JPegPlugin::InjectHooks();
    if (!IsHookDisabled("PipelinePlugin")) PipelinePlugin::InjectHooks();
    if (!IsHookDisabled("RpAnimBlendPlugin")) RpAnimBlendPlugin::InjectHooks();
    if (!IsHookDisabled("CCollisionPlugin")) CCollisionPlugin::InjectHooks();
    if (!IsHookDisabled("BreakablePlugin")) BreakablePlugin::InjectHooks();
    if (!IsHookDisabled("RtAnim")) RtAnim::InjectHooks();

    if (!IsHookDisabled("CIplStore")) CIplStore::InjectHooks();
    if (!IsHookDisabled("cHandlingDataMgr")) cHandlingDataMgr::InjectHooks();
    if (!IsHookDisabled("CLoadingScreen")) CLoadingScreen::InjectHooks();
    if (!IsHookDisabled("CRadar")) CRadar::InjectHooks();
    if (!IsHookDisabled("CText")) CText::InjectHooks();
    if (!IsHookDisabled("ModelIndices")) ModelIndices::InjectHooks();
    if (!IsHookDisabled("CWaterCannons")) CWaterCannons::InjectHooks();
    if (!IsHookDisabled("CWaterCannon")) CWaterCannon::InjectHooks();
    if (!IsHookDisabled("CSprite")) CSprite::InjectHooks();
    if (!IsHookDisabled("CPlaneTrail")) CPlaneTrail::InjectHooks();
    if (!IsHookDisabled("CPlaneTrails")) CPlaneTrails::InjectHooks();
    if (!IsHookDisabled("CPickup")) CPickup::InjectHooks();
    if (!IsHookDisabled("CPickups")) CPickups::InjectHooks();
    if (!IsHookDisabled("CPedIK")) CPedIK::InjectHooks();

    if (!IsHookDisabled("CCustomBuildingPipeline")) CCustomBuildingPipeline::InjectHooks();
    if (!IsHookDisabled("CCustomBuildingRenderer")) CCustomBuildingRenderer::InjectHooks();
    if (!IsHookDisabled("CCustomBuildingDNPipeline")) CCustomBuildingDNPipeline::InjectHooks();
    if (!IsHookDisabled("CCustomCarEnvMapPipeline")) CCustomCarEnvMapPipeline::InjectHooks();
    if (!IsHookDisabled("CConversations")) CConversations::InjectHooks();
    if (!IsHookDisabled("CProjectileInfo")) CProjectileInfo::InjectHooks();

    const auto Pools = [] {
        if (!IsHookDisabled("CPools")) CPools::InjectHooks();
        if (!IsHookDisabled("CIplDefPool")) CIplDefPool::InjectHooks();
        if (!IsHookDisabled("CPedPool")) CPedPool::InjectHooks();
        if (!IsHookDisabled("CVehiclePool")) CVehiclePool::InjectHooks();
        if (!IsHookDisabled("CBuildingPool")) CBuildingPool::InjectHooks();
        if (!IsHookDisabled("CObjectPool")) CObjectPool::InjectHooks();
        if (!IsHookDisabled("CDummyPool")) CDummyPool::InjectHooks();
        if (!IsHookDisabled("CColModelPool")) CColModelPool::InjectHooks();
        if (!IsHookDisabled("CTaskPool")) CTaskPool::InjectHooks();
        if (!IsHookDisabled("CPedIntelligencePool")) CPedIntelligencePool::InjectHooks();
        if (!IsHookDisabled("CPtrNodeSingleLinkPool")) CPtrNodeSingleLinkPool::InjectHooks();
        if (!IsHookDisabled("CPtrNodeDoubleLinkPool")) CPtrNodeDoubleLinkPool::InjectHooks();
        if (!IsHookDisabled("CEntryInfoNodePool")) CEntryInfoNodePool::InjectHooks();
        if (!IsHookDisabled("CPointRoutePool")) CPointRoutePool::InjectHooks();
        //CPatrolRoutePool::InjectHooks();
        if (!IsHookDisabled("CEventPool")) CEventPool::InjectHooks();
        if (!IsHookDisabled("CNodeRoutePool")) CNodeRoutePool::InjectHooks();
        if (!IsHookDisabled("CTaskAllocatorPool")) CTaskAllocatorPool::InjectHooks();
        if (!IsHookDisabled("CPedAttractorPool")) CPedAttractorPool::InjectHooks();
    };
    Pools();

    const auto Audio = []() {
        // General
        if (!IsHookDisabled("CAEAudioEnvironment")) CAEAudioEnvironment::InjectHooks();
        if (!IsHookDisabled("CAEAudioUtility")) CAEAudioUtility::InjectHooks();
        if (!IsHookDisabled("CAESmoothFadeThread")) CAESmoothFadeThread::InjectHooks();
        if (!IsHookDisabled("CAESound")) CAESound::InjectHooks();
        if (!IsHookDisabled("CAEStreamThread")) CAEStreamThread::InjectHooks();
        if (!IsHookDisabled("CAudioEngine")) CAudioEngine::InjectHooks();
        if (!IsHookDisabled("CAudioZones")) CAudioZones::InjectHooks();

        // Entities
        // CAEAudioEntity::InjectHooks(); -- inlined
        if (!IsHookDisabled("CAECollisionAudioEntity")) CAECollisionAudioEntity::InjectHooks();
        if (!IsHookDisabled("CAEDoorAudioEntity")) CAEDoorAudioEntity::InjectHooks();
        if (!IsHookDisabled("CAEExplosionAudioEntity")) CAEExplosionAudioEntity::InjectHooks();
        if (!IsHookDisabled("CAEFireAudioEntity")) CAEFireAudioEntity::InjectHooks();
        if (!IsHookDisabled("CAEFrontendAudioEntity")) CAEFrontendAudioEntity::InjectHooks();
        if (!IsHookDisabled("CAEGlobalWeaponAudioEntity")) CAEGlobalWeaponAudioEntity::InjectHooks();
        if (!IsHookDisabled("CAEPedAudioEntity")) CAEPedAudioEntity::InjectHooks();
        if (!IsHookDisabled("CAEPedlessSpeechAudioEntity")) CAEPedlessSpeechAudioEntity::InjectHooks();
        if (!IsHookDisabled("CAEPedSpeechAudioEntity")) CAEPedSpeechAudioEntity::InjectHooks();
        if (!IsHookDisabled("CAEPedWeaponAudioEntity")) CAEPedWeaponAudioEntity::InjectHooks();
        if (!IsHookDisabled("CAEPoliceScannerAudioEntity")) CAEPoliceScannerAudioEntity::InjectHooks();
        if (!IsHookDisabled("CAEScriptAudioEntity")) CAEScriptAudioEntity::InjectHooks();
        if (!IsHookDisabled("CAETwinLoopSoundEntity")) CAETwinLoopSoundEntity::InjectHooks();
        if (!IsHookDisabled("CAEVehicleAudioEntity")) CAEVehicleAudioEntity::InjectHooks();
        if (!IsHookDisabled("CAEWaterCannonAudioEntity")) CAEWaterCannonAudioEntity::InjectHooks();
        // CAEWeaponAudioEntity::InjectHooks();
        if (!IsHookDisabled("CAEWeatherAudioEntity")) CAEWeatherAudioEntity::InjectHooks();

        // Hardware
        if (!IsHookDisabled("CAEAudioChannel")) CAEAudioChannel::InjectHooks();
        if (!IsHookDisabled("CAEAudioHardware")) CAEAudioHardware::InjectHooks();
        if (!IsHookDisabled("CAEStaticChannel")) CAEStaticChannel::InjectHooks();
        if (!IsHookDisabled("CAEStreamingChannel")) CAEStreamingChannel::InjectHooks();

        // Loaders
        if (!IsHookDisabled("CAEBankLoader")) CAEBankLoader::InjectHooks();
        if (!IsHookDisabled("CAEDataStream")) CAEDataStream::InjectHooks();
        if (!IsHookDisabled("CAEMFDecoder")) CAEMFDecoder::InjectHooks();
        if (!IsHookDisabled("CAEMP3BankLoader")) CAEMP3BankLoader::InjectHooks();
        if (!IsHookDisabled("CAEMP3TrackLoader")) CAEMP3TrackLoader::InjectHooks();
        if (!IsHookDisabled("CAEStreamingDecoder")) CAEStreamingDecoder::InjectHooks();
        // CAEStreamTransformer::InjectHooks(); -- injected by AEDataStream::InjectHooks.
        if (!IsHookDisabled("CAETrackLoader")) CAETrackLoader::InjectHooks();
        if (!IsHookDisabled("CAEVorbisDecoder")) CAEVorbisDecoder::InjectHooks();
        if (!IsHookDisabled("CAEWaveDecoder")) CAEWaveDecoder::InjectHooks();
        if (!IsHookDisabled("CAEWMADecoder")) CAEWMADecoder::InjectHooks();

        // Managers
        if (!IsHookDisabled("CAEAmbienceTrackManager")) CAEAmbienceTrackManager::InjectHooks();
        if (!IsHookDisabled("CAECutsceneTrackManager")) CAECutsceneTrackManager::InjectHooks();
        if (!IsHookDisabled("CAERadioTrackManager")) CAERadioTrackManager::InjectHooks();
        if (!IsHookDisabled("CAESoundManager")) CAESoundManager::InjectHooks();
        if (!IsHookDisabled("CAEUserRadioTrackManager")) CAEUserRadioTrackManager::InjectHooks();
    };

    const auto Plant = []() {
        if (!IsHookDisabled("CPlantColEntEntry")) CPlantColEntEntry::InjectHooks();
        if (!IsHookDisabled("CPlantMgr")) CPlantMgr::InjectHooks();
        if (!IsHookDisabled("CPlantLocTri")) CPlantLocTri::InjectHooks();
        if (!IsHookDisabled("CGrassRenderer")) CGrassRenderer::InjectHooks();
        if (!IsHookDisabled("CPPTriPlantBuffer")) CPPTriPlantBuffer::InjectHooks();
        if (!IsHookDisabled("CPlantSurfPropMgr")) CPlantSurfPropMgr::InjectHooks();
    };
    Plant();

    const auto Tasks = []() {
        if (!IsHookDisabled("CTask")) CTask::InjectHooks();

        const auto Allocators = [] {
            const auto PedGroup = [] {
                if (!IsHookDisabled("CPedGroupDefaultTaskAllocatorFollowAnyMeans")) CPedGroupDefaultTaskAllocatorFollowAnyMeans::InjectHooks();
                if (!IsHookDisabled("CPedGroupDefaultTaskAllocatorFollowLimited")) CPedGroupDefaultTaskAllocatorFollowLimited::InjectHooks();
                if (!IsHookDisabled("CPedGroupDefaultTaskAllocatorStandStill")) CPedGroupDefaultTaskAllocatorStandStill::InjectHooks();
                if (!IsHookDisabled("CPedGroupDefaultTaskAllocatorChat")) CPedGroupDefaultTaskAllocatorChat::InjectHooks();
                if (!IsHookDisabled("CPedGroupDefaultTaskAllocatorSitInLeaderCar")) CPedGroupDefaultTaskAllocatorSitInLeaderCar::InjectHooks();
                if (!IsHookDisabled("CPedGroupDefaultTaskAllocatorRandom")) CPedGroupDefaultTaskAllocatorRandom::InjectHooks();
                if (!IsHookDisabled("CPedGroupDefaultTaskAllocators")) CPedGroupDefaultTaskAllocators::InjectHooks();
            };
            PedGroup();

            if (!IsHookDisabled("CTaskAllocator")) CTaskAllocator::InjectHooks();
            if (!IsHookDisabled("CTaskAllocatorKillThreatsBasic")) CTaskAllocatorKillThreatsBasic::InjectHooks();
            if (!IsHookDisabled("CTaskAllocatorKillThreatsDriveby")) CTaskAllocatorKillThreatsDriveby::InjectHooks();
            if (!IsHookDisabled("CTaskAllocatorKillThreatsBasicRandomGroup")) CTaskAllocatorKillThreatsBasicRandomGroup::InjectHooks();
            if (!IsHookDisabled("CTaskAllocatorKillOnFoot")) CTaskAllocatorKillOnFoot::InjectHooks();
            if (!IsHookDisabled("CTaskAllocatorAttack")) CTaskAllocatorAttack::InjectHooks();
            if (!IsHookDisabled("CTaskAllocatorPlayerCommandAttack")) CTaskAllocatorPlayerCommandAttack::InjectHooks();
        };
        Allocators();

        const auto Interior = [] {
            if (!IsHookDisabled("CTaskInteriorBeInHouse")) CTaskInteriorBeInHouse::InjectHooks();
            if (!IsHookDisabled("CTaskInteriorBeInOffice")) CTaskInteriorBeInOffice::InjectHooks();
            if (!IsHookDisabled("CTaskInteriorBeInShop")) CTaskInteriorBeInShop::InjectHooks();
            if (!IsHookDisabled("CTaskInteriorGoToInfo")) CTaskInteriorGoToInfo::InjectHooks();
            if (!IsHookDisabled("CTaskInteriorLieInBed")) CTaskInteriorLieInBed::InjectHooks();
            if (!IsHookDisabled("CTaskInteriorShopKeeper")) CTaskInteriorShopKeeper::InjectHooks();
            if (!IsHookDisabled("CTaskInteriorSitAtDesk")) CTaskInteriorSitAtDesk::InjectHooks();
        // CTaskInteriorSitInChair::InjectHooks();
            if (!IsHookDisabled("CTaskInteriorUseInfo")) CTaskInteriorUseInfo::InjectHooks();
        };

        if (!IsHookDisabled("CTaskComplexUseClosestFreeScriptedAttractorRun")) CTaskComplexUseClosestFreeScriptedAttractorRun::InjectHooks();
        if (!IsHookDisabled("CTaskComplexUseClosestFreeScriptedAttractorSprint")) CTaskComplexUseClosestFreeScriptedAttractorSprint::InjectHooks();
        if (!IsHookDisabled("CTaskComplexUseClosestFreeScriptedAttractor")) CTaskComplexUseClosestFreeScriptedAttractor::InjectHooks();

        if (!IsHookDisabled("CTaskSimpleChoking")) CTaskSimpleChoking::InjectHooks();
        if (!IsHookDisabled("CTaskComplexEvasiveDiveAndGetUp")) CTaskComplexEvasiveDiveAndGetUp::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleLeaveGroup")) CTaskSimpleLeaveGroup::InjectHooks();
        if (!IsHookDisabled("CTaskComplexDiveFromAttachedEntityAndGetUp")) CTaskComplexDiveFromAttachedEntityAndGetUp::InjectHooks();
        if (!IsHookDisabled("CTaskGangHasslePed")) CTaskGangHasslePed::InjectHooks();
        if (!IsHookDisabled("CTaskGangHassleVehicle")) CTaskGangHassleVehicle::InjectHooks();
        if (!IsHookDisabled("CTaskGoToVehicleAndLean")) CTaskGoToVehicleAndLean::InjectHooks();
        if (!IsHookDisabled("CTaskLeanOnVehicle")) CTaskLeanOnVehicle::InjectHooks();
        if (!IsHookDisabled("CTaskComplexCarSlowBeDraggedOut")) CTaskComplexCarSlowBeDraggedOut::InjectHooks();
        if (!IsHookDisabled("CTaskComplexCarSlowBeDraggedOutAndStandUp")) CTaskComplexCarSlowBeDraggedOutAndStandUp::InjectHooks();
        if (!IsHookDisabled("CTaskComplexDestroyCar")) CTaskComplexDestroyCar::InjectHooks();
        if (!IsHookDisabled("CTaskComplexDestroyCarArmed")) CTaskComplexDestroyCarArmed::InjectHooks();
        if (!IsHookDisabled("CTaskComplexDestroyCarMelee")) CTaskComplexDestroyCarMelee::InjectHooks();
        if (!IsHookDisabled("CTaskComplexDieInCar")) CTaskComplexDieInCar::InjectHooks();
        // CTaskComplexDiveFromAttachedEntityAndGetUp::InjectHooks();
        CTaskComplexDragPedFromCar__InjectHooks();
        if (!IsHookDisabled("CTaskComplexDrivePointRoute")) CTaskComplexDrivePointRoute::InjectHooks();
        if (!IsHookDisabled("CTaskComplexEnterCarAsDriverTimed")) CTaskComplexEnterCarAsDriverTimed::InjectHooks();
        if (!IsHookDisabled("CTaskComplexEnterCarAsPassengerTimed")) CTaskComplexEnterCarAsPassengerTimed::InjectHooks();
        // CTaskComplexEnterCarAsPassengerWait::InjectHooks();
        // CTaskComplexEvasiveCower::InjectHooks();
        // CTaskComplexEvasiveDiveAndGetUp::InjectHooks();
        // CTaskComplexEvasiveStep::InjectHooks();
        // CTaskComplexExtinguishFireOnFoot::InjectHooks();
        if (!IsHookDisabled("CTaskComplexExtinguishFires")) CTaskComplexExtinguishFires::InjectHooks();
        // CTaskComplexFallAndStayDown::InjectHooks();
        if (!IsHookDisabled("CTaskComplexFallToDeath")) CTaskComplexFallToDeath::InjectHooks();
        if (!IsHookDisabled("CTaskComplexFleeAnyMeans")) CTaskComplexFleeAnyMeans::InjectHooks();
        if (!IsHookDisabled("CTaskComplexFleeEntity")) CTaskComplexFleeEntity::InjectHooks();
        if (!IsHookDisabled("CTaskComplexFleePoint")) CTaskComplexFleePoint::InjectHooks();
        if (!IsHookDisabled("CTaskComplexFleeShooting")) CTaskComplexFleeShooting::InjectHooks();
        if (!IsHookDisabled("CTaskComplexFollowLeaderAnyMeans")) CTaskComplexFollowLeaderAnyMeans::InjectHooks();
        // CTaskComplexFollowNodeRouteShooting::InjectHooks();
        // CTaskComplexFollowPatrolRoute::InjectHooks();
        if (!IsHookDisabled("CTaskComplexFollowPointRoute")) CTaskComplexFollowPointRoute::InjectHooks();
        if (!IsHookDisabled("CTaskComplexGangFollower")) CTaskComplexGangFollower::InjectHooks();
        if (!IsHookDisabled("CTaskComplexGangJoinRespond")) CTaskComplexGangJoinRespond::InjectHooks();
        if (!IsHookDisabled("CTaskComplexGangLeader")) CTaskComplexGangLeader::InjectHooks();
        if (!IsHookDisabled("CTaskComplexGetOnBoatSeat")) CTaskComplexGetOnBoatSeat::InjectHooks();
        if (!IsHookDisabled("CTaskComplexGoToAttractor")) CTaskComplexGoToAttractor::InjectHooks();
        if (!IsHookDisabled("CTaskComplexGoToBoatSteeringWheel")) CTaskComplexGoToBoatSteeringWheel::InjectHooks();
        if (!IsHookDisabled("CTaskComplexGoToCarDoorAndStandStill")) CTaskComplexGoToCarDoorAndStandStill::InjectHooks();
        if (!IsHookDisabled("CTaskComplexGoToPointAiming")) CTaskComplexGoToPointAiming::InjectHooks();
        if (!IsHookDisabled("CTaskComplexGoToPointAndStandStillAndAchieveHeading")) CTaskComplexGoToPointAndStandStillAndAchieveHeading::InjectHooks();
        if (!IsHookDisabled("CTaskComplexGoToPointAnyMeans")) CTaskComplexGoToPointAnyMeans::InjectHooks();
        if (!IsHookDisabled("CTaskComplexGoToPointShooting")) CTaskComplexGoToPointShooting::InjectHooks();
        // CTaskComplexGoToPointUntilCollisionAndStandStill::InjectHooks();
        if (!IsHookDisabled("CTaskComplexGotoDoorAndOpen")) CTaskComplexGotoDoorAndOpen::InjectHooks();
        if (!IsHookDisabled("CTaskComplexHitByGunResponse")) CTaskComplexHitByGunResponse::InjectHooks();
        if (!IsHookDisabled("CTaskComplexHitResponse")) CTaskComplexHitResponse::InjectHooks();
        if (!IsHookDisabled("CTaskComplexInWater")) CTaskComplexInWater::InjectHooks();
        if (!IsHookDisabled("CTaskComplexInvestigateDeadPed")) CTaskComplexInvestigateDeadPed::InjectHooks();
        if (!IsHookDisabled("CTaskComplexInvestigateDisturbance")) CTaskComplexInvestigateDisturbance::InjectHooks();
        // CTaskComplexKillAllThreats::InjectHooks();
        if (!IsHookDisabled("CTaskComplexKillCriminal")) CTaskComplexKillCriminal::InjectHooks();

        if (!IsHookDisabled("CTaskComplexKillPedOnFootArmed")) CTaskComplexKillPedOnFootArmed::InjectHooks();
        if (!IsHookDisabled("CTaskComplexKillPedGroupOnFoot")) CTaskComplexKillPedGroupOnFoot::InjectHooks();
        if (!IsHookDisabled("CTaskComplexKillPedOnFootMelee")) CTaskComplexKillPedOnFootMelee::InjectHooks();
        if (!IsHookDisabled("CTaskComplexKillPedOnFootStealth")) CTaskComplexKillPedOnFootStealth::InjectHooks();

        if (!IsHookDisabled("CTaskComplexLeaveCarAndDie")) CTaskComplexLeaveCarAndDie::InjectHooks();
        if (!IsHookDisabled("CTaskComplexLeaveBoat")) CTaskComplexLeaveBoat::InjectHooks();
        if (!IsHookDisabled("CTaskComplexLeaveCarAndFlee")) CTaskComplexLeaveCarAndFlee::InjectHooks();
        if (!IsHookDisabled("CTaskComplexLeaveCarAsPassengerWait")) CTaskComplexLeaveCarAsPassengerWait::InjectHooks();
        if (!IsHookDisabled("CTaskComplexLeaveCarAndWander")) CTaskComplexLeaveCarAndWander::InjectHooks();
        if (!IsHookDisabled("CTaskComplexMoveBackAndJump")) CTaskComplexMoveBackAndJump::InjectHooks();
        if (!IsHookDisabled("CTaskComplexOnFire")) CTaskComplexOnFire::InjectHooks();
        // CTaskComplexOpenDriverDoor::InjectHooks();
        // CTaskComplexOpenPassengerDoor::InjectHooks();
        if (!IsHookDisabled("CTaskComplexPassObject")) CTaskComplexPassObject::InjectHooks();
        // CTaskComplexPresentIDToCop::InjectHooks();
        // CTaskComplexReactToAttack::InjectHooks();
        if (!IsHookDisabled("CTaskComplexReactToGunAimedAt")) CTaskComplexReactToGunAimedAt::InjectHooks();
        if (!IsHookDisabled("CTaskComplexRoadRage")) CTaskComplexRoadRage::InjectHooks();
        // CTaskComplexScreamInCarThenLeave::InjectHooks();
        if (!IsHookDisabled("CTaskComplexSeekCoverUntilTargetDead")) CTaskComplexSeekCoverUntilTargetDead::InjectHooks();

        // CTaskComplexSeekEntity<CEntitySeekPosCalculator>::InjectHooks();
        // CTaskComplexSeekEntity<CEntitySeekPosCalculatorDriving>::InjectHooks();
        // CTaskComplexSeekEntity<CEntitySeekPosCalculatorEntitySurface>::InjectHooks();
        // CTaskComplexSeekEntity<CEntitySeekPosCalculatorStandard>::InjectHooks();
        // CTaskComplexSeekEntity<CEntitySeekPosCalculatorVehicleId>::InjectHooks();

        if (!IsHookDisabled("CTaskComplexSeekEntityRadiusAngleOffset")) CTaskComplexSeekEntityRadiusAngleOffset::InjectHooks();
        if (!IsHookDisabled("CTaskComplexSeekEntityStandard")) CTaskComplexSeekEntityStandard::InjectHooks();
        if (!IsHookDisabled("CTaskComplexSeekEntityXYOffset")) CTaskComplexSeekEntityXYOffset::InjectHooks();
        if (!IsHookDisabled("CTaskComplexSeekEntityAiming")) CTaskComplexSeekEntityAiming::InjectHooks();
        //CTaskComplexSeekEntityXYOffset::InjectHooks();
        CTaskComplexSeekEntity<CEntitySeekPosCalculatorXYOffset>::InjectHooks();

        // CTaskComplexSeekEntityAiming::InjectHooks();
        if (!IsHookDisabled("CTaskComplexSeekEntityAnyMeansXYOffset")) CTaskComplexSeekEntityAnyMeansXYOffset::InjectHooks();
        if (!IsHookDisabled("CTaskComplexShuffleSeats")) CTaskComplexShuffleSeats::InjectHooks();
        if (!IsHookDisabled("CTaskComplexSignalAtPed")) CTaskComplexSignalAtPed::InjectHooks();
        if (!IsHookDisabled("CTaskComplexSitDownThenIdleThenStandUp")) CTaskComplexSitDownThenIdleThenStandUp::InjectHooks();
        if (!IsHookDisabled("CTaskComplexSmartFleeEntity")) CTaskComplexSmartFleeEntity::InjectHooks();
        if (!IsHookDisabled("CTaskComplexSmartFleePoint")) CTaskComplexSmartFleePoint::InjectHooks();
        if (!IsHookDisabled("CTaskComplexStareAtPed")) CTaskComplexStareAtPed::InjectHooks();
        if (!IsHookDisabled("CTaskComplexStealCar")) CTaskComplexStealCar::InjectHooks();
        if (!IsHookDisabled("CTaskComplexTrackEntity")) CTaskComplexTrackEntity::InjectHooks();
        if (!IsHookDisabled("CTaskComplexTurnToFaceEntityOrCoord")) CTaskComplexTurnToFaceEntityOrCoord::InjectHooks();
        if (!IsHookDisabled("CTaskComplexUseAttractor")) CTaskComplexUseAttractor::InjectHooks();
        if (!IsHookDisabled("CTaskComplexUseAttractorPartner")) CTaskComplexUseAttractorPartner::InjectHooks();
        if (!IsHookDisabled("CTaskComplexAttractorPartnerWait")) CTaskComplexAttractorPartnerWait::InjectHooks();
        // CTaskComplexUseClosestFreeScriptedAttractor::InjectHooks();
        // CTaskComplexUseClosestFreeScriptedAttractorRun::InjectHooks();
        // CTaskComplexUseClosestFreeScriptedAttractorSprint::InjectHooks();
        if (!IsHookDisabled("CTaskComplexUseEffect")) CTaskComplexUseEffect::InjectHooks();
        if (!IsHookDisabled("CTaskComplexUseEffectRunning")) CTaskComplexUseEffectRunning::InjectHooks();
        if (!IsHookDisabled("CTaskComplexUseEffectSprinting")) CTaskComplexUseEffectSprinting::InjectHooks();
        // CTaskComplexUseEntryExit::InjectHooks();
        // CTaskComplexUsePairedAttractor::InjectHooks();
        if (!IsHookDisabled("CTaskComplexUseScriptedBrain")) CTaskComplexUseScriptedBrain::InjectHooks();
        if (!IsHookDisabled("CTaskComplexUseScriptedAttractor")) CTaskComplexUseScriptedAttractor::InjectHooks();
        if (!IsHookDisabled("CTaskComplexWaitAtAttractor")) CTaskComplexWaitAtAttractor::InjectHooks();
        if (!IsHookDisabled("CTaskComplexWaitForBus")) CTaskComplexWaitForBus::InjectHooks();
        if (!IsHookDisabled("CTaskComplexWaitForBackup")) CTaskComplexWaitForBackup::InjectHooks();
        if (!IsHookDisabled("CTaskComplexWaitForDryWeather")) CTaskComplexWaitForDryWeather::InjectHooks();
        if (!IsHookDisabled("CTaskComplexWalkAlongsidePed")) CTaskComplexWalkAlongsidePed::InjectHooks();
        if (!IsHookDisabled("CTaskComplexWalkRoundBuildingAttempt")) CTaskComplexWalkRoundBuildingAttempt::InjectHooks();
        if (!IsHookDisabled("CTaskComplexWalkRoundFire")) CTaskComplexWalkRoundFire::InjectHooks();
        if (!IsHookDisabled("CTaskComplexWanderFlee")) CTaskComplexWanderFlee::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleAffectSecondaryBehaviour")) CTaskSimpleAffectSecondaryBehaviour::InjectHooks();
        CTaskSimpleArrestPed__InjectHooks();
        if (!IsHookDisabled("CTaskSimpleBeHit")) CTaskSimpleBeHit::InjectHooks();
        // CTaskSimpleBeHitWhileMoving::InjectHooks();
        // CTaskSimpleBeKickedOnGround::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleBikeJacked")) CTaskSimpleBikeJacked::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCarAlign")) CTaskSimpleCarAlign::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCarCloseDoorFromInside")) CTaskSimpleCarCloseDoorFromInside::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCarCloseDoorFromOutside")) CTaskSimpleCarCloseDoorFromOutside::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCarFallOut")) CTaskSimpleCarFallOut::InjectHooks();
        // CTaskSimpleCarForcePedOut::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCarGetOut")) CTaskSimpleCarGetOut::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCarGetIn")) CTaskSimpleCarGetIn::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse")) CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCarOpenDoorFromOutside")) CTaskSimpleCarOpenDoorFromOutside::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCarJumpOut")) CTaskSimpleCarJumpOut::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCarOpenLockedDoorFromOutside")) CTaskSimpleCarOpenLockedDoorFromOutside::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCarSetPedSlowDraggedOut")) CTaskSimpleCarSetPedSlowDraggedOut::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCarSetTempAction")) CTaskSimpleCarSetTempAction::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCarShuffle")) CTaskSimpleCarShuffle::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCarSlowBeDraggedOut")) CTaskSimpleCarSlowBeDraggedOut::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCarWaitToSlowDown")) CTaskSimpleCarWaitToSlowDown::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCarWaitForDoorNotToBeInUse")) CTaskSimpleCarWaitForDoorNotToBeInUse::InjectHooks();
        // CTaskSimpleChat::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCower")) CTaskSimpleCower::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleClearLookAt")) CTaskSimpleClearLookAt::InjectHooks();
        // CTaskSimpleDead::InjectHooks();
        // CTaskSimpleDetonate::InjectHooks();
        // + CTaskSimpleDieInCar
        // CTaskSimpleDoHandSignal::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleDrown")) CTaskSimpleDrown::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleDrownInCar")) CTaskSimpleDrownInCar::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleDuckToggle")) CTaskSimpleDuckToggle::InjectHooks();
        // CTaskSimpleDuckWhileShotsWhizzing::InjectHooks();
        // CTaskSimpleEvasiveDive::InjectHooks();
        // CTaskSimpleEvasiveStep::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleFightingControl")) CTaskSimpleFightingControl::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleFinishBrain")) CTaskSimpleFinishBrain::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleGunControl")) CTaskSimpleGunControl::InjectHooks();
        // CTaskSimpleHailTaxi::InjectHooks();
        // CTaskSimpleHailTaxiAndPause::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleHandsUp")) CTaskSimpleHandsUp::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleHitByGunFromFront")) CTaskSimpleHitByGunFromFront::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleHitByGunFromLeft")) CTaskSimpleHitByGunFromLeft::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleHitByGunFromRear")) CTaskSimpleHitByGunFromRear::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleHitByGunFromRight")) CTaskSimpleHitByGunFromRight::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleHitFromBack")) CTaskSimpleHitFromBack::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleHitFromBehind")) CTaskSimpleHitFromBehind::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleHitFromFront")) CTaskSimpleHitFromFront::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleHitFromLeft")) CTaskSimpleHitFromLeft::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleHitFromRight")) CTaskSimpleHitFromRight::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleHitWall")) CTaskSimpleHitWall::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleKillPedWithCar")) CTaskSimpleKillPedWithCar::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleHurtPedWithCar")) CTaskSimpleHurtPedWithCar::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleLookAbout")) CTaskSimpleLookAbout::InjectHooks();
        // CTaskSimpleOnEscalator::InjectHooks();
        if (!IsHookDisabled("CTaskSimplePickUpBike")) CTaskSimplePickUpBike::InjectHooks();
        if (!IsHookDisabled("CTaskSimplePlayerOnFire")) CTaskSimplePlayerOnFire::InjectHooks();
        // CTaskSimpleRunAnimLoopedMiddle::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleRunTimedAnim")) CTaskSimpleRunTimedAnim::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleSay")) CTaskSimpleSay::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleSetCharDecisionMaker")) CTaskSimpleSetCharDecisionMaker::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleSetCharIgnoreWeaponRangeFlag")) CTaskSimpleSetCharIgnoreWeaponRangeFlag::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleSetKindaStayInSamePlace")) CTaskSimpleSetKindaStayInSamePlace::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleSetPedAsAutoDriver")) CTaskSimpleSetPedAsAutoDriver::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleShakeFist")) CTaskSimpleShakeFist::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleSitDown")) CTaskSimpleSitDown::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleSitIdle")) CTaskSimpleSitIdle::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleStandUp")) CTaskSimpleStandUp::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleThrowControl")) CTaskSimpleThrowControl::InjectHooks();
        // CTaskSimpleTired::InjectHooks();
        // CTaskSimpleThrowControl::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleTired")) CTaskSimpleTired::InjectHooks();
        // CTaskSimpleTriggerEvent::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleUseAtm")) CTaskSimpleUseAtm::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleTurn180")) CTaskSimpleTurn180::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleWaitForBus")) CTaskSimpleWaitForBus::InjectHooks();
        // CTaskSimpleWaitForPizza::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleWaitUntilAreaCodesMatch")) CTaskSimpleWaitUntilAreaCodesMatch::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleWaitUntilLeaderAreaCodesMatch")) CTaskSimpleWaitUntilLeaderAreaCodesMatch::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleWaitUntilPedIsInCar")) CTaskSimpleWaitUntilPedIsInCar::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleWaitUntilPedIsOutCar")) CTaskSimpleWaitUntilPedIsOutCar::InjectHooks();
        if (!IsHookDisabled("CTaskComplexSequence")) CTaskComplexSequence::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCarSlowDragPedOut")) CTaskSimpleCarSlowDragPedOut::InjectHooks();
        if (!IsHookDisabled("CTaskManager")) CTaskManager::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCreateCarAndGetIn")) CTaskSimpleCreateCarAndGetIn::InjectHooks();
        if (!IsHookDisabled("CTaskComplexEnterAnyCarAsDriver")) CTaskComplexEnterAnyCarAsDriver::InjectHooks();
        if (!IsHookDisabled("CTaskComplexLeaveAnyCar")) CTaskComplexLeaveAnyCar::InjectHooks();
        // CTaskSimpleAbseil::InjectHooks();
        if (!IsHookDisabled("CTaskComplexWanderCop")) CTaskComplexWanderCop::InjectHooks();
        // CTaskComplexUseMobilePhone::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleStandStill")) CTaskSimpleStandStill::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleCarDrive")) CTaskSimpleCarDrive::InjectHooks();
        // CTaskSimpleScratchHead::InjectHooks();
        // CTaskSimpleChoking::InjectHooks();
        if (!IsHookDisabled("CTaskComplexPartnerChat")) CTaskComplexPartnerChat::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleUseGun")) CTaskSimpleUseGun::InjectHooks();
        // CTaskSimpleGangDriveBy::InjectHooks();
        if (!IsHookDisabled("CTaskComplexObserveTrafficLightsAndAchieveHeading")) CTaskComplexObserveTrafficLightsAndAchieveHeading::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleInAir")) CTaskSimpleInAir::InjectHooks();
        // CTaskComplexHitPedWithCar::InjectHooks();
        if (!IsHookDisabled("CTaskSimplePlayerOnFoot")) CTaskSimplePlayerOnFoot::InjectHooks();
        if (!IsHookDisabled("CTaskComplexFollowPedFootsteps")) CTaskComplexFollowPedFootsteps::InjectHooks();
        if (!IsHookDisabled("CTaskComplexGetUpAndStandStill")) CTaskComplexGetUpAndStandStill::InjectHooks();
        // CTaskComplexGoPickUpEntity::InjectHooks();
        CTaskSimpleDie__InjectHooks();
        if (!IsHookDisabled("CTaskSimpleDuck")) CTaskSimpleDuck::InjectHooks();
        if (!IsHookDisabled("CTaskComplexPolicePursuit")) CTaskComplexPolicePursuit::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleFacial")) CTaskSimpleFacial::InjectHooks();
        if (!IsHookDisabled("CTaskComplexCopInCar")) CTaskComplexCopInCar::InjectHooks();
        if (!IsHookDisabled("CTaskComplexFacial")) CTaskComplexFacial::InjectHooks();
        if (!IsHookDisabled("CTaskComplexInAirAndLand")) CTaskComplexInAirAndLand::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleGetUp")) CTaskSimpleGetUp::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleUninterruptable")) CTaskSimpleUninterruptable::InjectHooks();
        if (!IsHookDisabled("CTaskComplexFallAndGetUp")) CTaskComplexFallAndGetUp::InjectHooks();
        if (!IsHookDisabled("CTaskComplexFollowNodeRoute")) CTaskComplexFollowNodeRoute::InjectHooks();
        if (!IsHookDisabled("CTaskComplexFollowLeaderInFormation")) CTaskComplexFollowLeaderInFormation::InjectHooks();
        // CTaskSimpleTriggerLookAt::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleHitHead")) CTaskSimpleHitHead::InjectHooks();
        if (!IsHookDisabled("CTaskUtilityLineUpPedWithCar")) CTaskUtilityLineUpPedWithCar::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleLand")) CTaskSimpleLand::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleJetPack")) CTaskSimpleJetPack::InjectHooks();
        // + CTaskSimpleSetStayInSamePlace::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleJump")) CTaskSimpleJump::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleFall")) CTaskSimpleFall::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleClimb")) CTaskSimpleClimb::InjectHooks();
        if (!IsHookDisabled("CTaskComplexPlayHandSignalAnim")) CTaskComplexPlayHandSignalAnim::InjectHooks();
        // CTaskComplexWaitForPed::InjectHooks();
        if (!IsHookDisabled("CTaskComplexJump")) CTaskComplexJump::InjectHooks();
        // CTaskComplexWanderStandard::InjectHooks();
        if (!IsHookDisabled("CTaskComplexSunbathe")) CTaskComplexSunbathe::InjectHooks();
        if (!IsHookDisabled("CTaskComplexEnterCar")) CTaskComplexEnterCar::InjectHooks();
        // CTaskSimpleTogglePedThreatScanner::InjectHooks();
        if (!IsHookDisabled("CTaskComplexUseGoggles")) CTaskComplexUseGoggles::InjectHooks();
        // CTaskComplexCrossRoadLookAndAchieveHeading::InjectHooks();
        if (!IsHookDisabled("CTaskComplexGoToPointAndStandStill")) CTaskComplexGoToPointAndStandStill::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleAchieveHeading")) CTaskSimpleAchieveHeading::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleGiveCPR")) CTaskSimpleGiveCPR::InjectHooks();
        // CTaskSimpleCarSetPedInAsPassenger::InjectHooks();
        if (!IsHookDisabled("CTaskComplexDriveFireTruck")) CTaskComplexDriveFireTruck::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleSwim")) CTaskSimpleSwim::InjectHooks();
        if (!IsHookDisabled("CTaskComplexWalkRoundObject")) CTaskComplexWalkRoundObject::InjectHooks();
        if (!IsHookDisabled("CTaskSimplePause")) CTaskSimplePause::InjectHooks();
        // CTaskComplexEnterCarAsPassenger::InjectHooks();
        // + CTaskComplexEnterCarAsDriver::InjectHooks();
        // CTaskSimpleNone::InjectHooks();
        if (!IsHookDisabled("CTaskComplexKillPedOnFoot")) CTaskComplexKillPedOnFoot::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleThrowProjectile")) CTaskSimpleThrowProjectile::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleGoToPoint")) CTaskSimpleGoToPoint::InjectHooks();
        // CTaskComplexWanderMedic::InjectHooks();
        // CTaskSimpleCarDriveTimed::InjectHooks();
        // CTaskComplexDriveWander::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleStealthKill")) CTaskSimpleStealthKill::InjectHooks();
        // CTaskSimpleCarSetPedOut::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleAnim")) CTaskSimpleAnim::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleRunAnim")) CTaskSimpleRunAnim::InjectHooks();
        // CTaskComplexWanderCriminal::InjectHooks();
        // CTaskComplexWanderProstitute::InjectHooks();
        if (!IsHookDisabled("CTaskComplexPartnerGreet")) CTaskComplexPartnerGreet::InjectHooks();
        if (!IsHookDisabled("CTaskComplexWalkRoundCar")) CTaskComplexWalkRoundCar::InjectHooks();
        if (!IsHookDisabled("CTaskComplexMedicTreatInjuredPed")) CTaskComplexMedicTreatInjuredPed::InjectHooks();
        if (!IsHookDisabled("CTaskSimplePlayHandSignalAnim")) CTaskSimplePlayHandSignalAnim::InjectHooks();
        if (!IsHookDisabled("CTaskComplexCarDrive")) CTaskComplexCarDrive::InjectHooks();
        if (!IsHookDisabled("CTaskComplexCarDriveMission")) CTaskComplexCarDriveMission::InjectHooks();
        if (!IsHookDisabled("CTaskComplexKillPedFromBoat")) CTaskComplexKillPedFromBoat::InjectHooks();
        // CTaskComplexLeaveCar::InjectHooks();
        if (!IsHookDisabled("CTaskComplexTreatAccident")) CTaskComplexTreatAccident::InjectHooks();
        if (!IsHookDisabled("CTaskComplexGoToPointAndStandStillTimed")) CTaskComplexGoToPointAndStandStillTimed::InjectHooks();
        if (!IsHookDisabled("CTaskComplexPartnerShove")) CTaskComplexPartnerShove::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleRunNamedAnim")) CTaskSimpleRunNamedAnim::InjectHooks();
        // CTaskComplexProstituteSolicit::InjectHooks();
        if (!IsHookDisabled("CTaskComplexStuckInAir")) CTaskComplexStuckInAir::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleHoldEntity")) CTaskSimpleHoldEntity::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleGoTo")) CTaskSimpleGoTo::InjectHooks();
        if (!IsHookDisabled("CTaskComplexPartner")) CTaskComplexPartner::InjectHooks();
        // CTaskSimpleCarSetPedInAsDriver::InjectHooks();
        if (!IsHookDisabled("CTaskComplexWander")) CTaskComplexWander::InjectHooks();
        if (!IsHookDisabled("CTaskSimplePutDownEntity")) CTaskSimplePutDownEntity::InjectHooks();
        if (!IsHookDisabled("CTaskComplexUseSwatRope")) CTaskComplexUseSwatRope::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleGoToPointFine")) CTaskSimpleGoToPointFine::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleIKChain")) CTaskSimpleIKChain::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleIKManager")) CTaskSimpleIKManager::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleIKPointArm")) CTaskSimpleIKPointArm::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleIKLookAt")) CTaskSimpleIKLookAt::InjectHooks();
        if (!IsHookDisabled("CTaskComplexDie")) CTaskComplexDie::InjectHooks();
        if (!IsHookDisabled("CTaskComplexEnterBoatAsDriver")) CTaskComplexEnterBoatAsDriver::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleFight")) CTaskSimpleFight::InjectHooks();
        if (!IsHookDisabled("CTaskComplexUseWaterCannon")) CTaskComplexUseWaterCannon::InjectHooks();
        // CTaskComplexDriveToPoint::InjectHooks();
        if (!IsHookDisabled("CTaskSimpleSlideToCoord")) CTaskSimpleSlideToCoord::InjectHooks();
        // CTaskComplexPartnerDeal::InjectHooks();
        if (!IsHookDisabled("CTaskSimplePickUpEntity")) CTaskSimplePickUpEntity::InjectHooks();
        if (!IsHookDisabled("CTaskComplexBeInGroup")) CTaskComplexBeInGroup::InjectHooks();
        // CTaskComplexBeCop::InjectHooks();
        if (!IsHookDisabled("CTaskComplexAvoidOtherPedWhileWandering")) CTaskComplexAvoidOtherPedWhileWandering::InjectHooks();
        if (!IsHookDisabled("CTaskComplexArrestPed")) CTaskComplexArrestPed::InjectHooks();

        const auto EntitySeekPosCalculators = [] {
            if (!IsHookDisabled("CEntitySeekPosCalculator")) CEntitySeekPosCalculator::InjectHooks();
            if (!IsHookDisabled("CEntitySeekPosCalculatorStandard")) CEntitySeekPosCalculatorStandard::InjectHooks();
            if (!IsHookDisabled("CEntitySeekPosCalculatorRadiusAngleOffset")) CEntitySeekPosCalculatorRadiusAngleOffset::InjectHooks();
            if (!IsHookDisabled("CEntitySeekPosCalculatorXYOffset")) CEntitySeekPosCalculatorXYOffset::InjectHooks();
            if (!IsHookDisabled("CEntitySeekPosCalculatorFixedPos")) CEntitySeekPosCalculatorFixedPos::InjectHooks();
        };
        EntitySeekPosCalculators();
    };

    const auto Events = []() {
        if (!IsHookDisabled("CEvent")) CEvent::InjectHooks();
        if (!IsHookDisabled("CEventSeenPanickedPed")) CEventSeenPanickedPed::InjectHooks();
        if (!IsHookDisabled("CEventCarUpsideDown")) CEventCarUpsideDown::InjectHooks();
        if (!IsHookDisabled("CEventPassObject")) CEventPassObject::InjectHooks();
        if (!IsHookDisabled("CEventLeanOnVehicle")) CEventLeanOnVehicle::InjectHooks();
        if (!IsHookDisabled("CEventSeenCop")) CEventSeenCop::InjectHooks();
        if (!IsHookDisabled("CEventOnFire")) CEventOnFire::InjectHooks();
        // + CEventRevived::InjectHooks();
        if (!IsHookDisabled("CEventHandlerHistory")) CEventHandlerHistory::InjectHooks();
        if (!IsHookDisabled("CEventEditableResponse")) CEventEditableResponse::InjectHooks();
        if (!IsHookDisabled("CEventDamage")) CEventDamage::InjectHooks();
        if (!IsHookDisabled("CEventAttractor")) CEventAttractor::InjectHooks();
        if (!IsHookDisabled("CEventScriptedAttractor")) CEventScriptedAttractor::InjectHooks();
        if (!IsHookDisabled("CEventGunShot")) CEventGunShot::InjectHooks();
        if (!IsHookDisabled("CEventGunShotWhizzedBy")) CEventGunShotWhizzedBy::InjectHooks();
        // + CEventGunAimedAt::InjectHooks();
        // + CEventDeath::InjectHooks();
        // + CEventDeadPed::InjectHooks();
        // + CEventDraggedOutCar::InjectHooks();
        // + CEventGotKnockedOverByCar::InjectHooks();
        if (!IsHookDisabled("CEventKnockOffBike")) CEventKnockOffBike::InjectHooks();
        if (!IsHookDisabled("CEventScriptCommand")) CEventScriptCommand::InjectHooks();
        if (!IsHookDisabled("CEventSoundQuiet")) CEventSoundQuiet::InjectHooks();
        // + CEventPedToChase::InjectHooks();
        // + CEventPedToFlee::InjectHooks();
        if (!IsHookDisabled("CEventPotentialWalkIntoVehicle")) CEventPotentialWalkIntoVehicle::InjectHooks();
        if (!IsHookDisabled("CEventPotentialWalkIntoObject")) CEventPotentialWalkIntoObject::InjectHooks();
        if (!IsHookDisabled("CEventPotentialWalkIntoFire")) CEventPotentialWalkIntoFire::InjectHooks();
        if (!IsHookDisabled("CEventPotentialWalkIntoPed")) CEventPotentialWalkIntoPed::InjectHooks();
        if (!IsHookDisabled("CEventPedCollisionWithPed")) CEventPedCollisionWithPed::InjectHooks();
        if (!IsHookDisabled("CEventPedCollisionWithPlayer")) CEventPedCollisionWithPlayer::InjectHooks();
        if (!IsHookDisabled("CEventPlayerCollisionWithPed")) CEventPlayerCollisionWithPed::InjectHooks();
        if (!IsHookDisabled("CEventObjectCollision")) CEventObjectCollision::InjectHooks();
        if (!IsHookDisabled("CEventBuildingCollision")) CEventBuildingCollision::InjectHooks();
        if (!IsHookDisabled("CEventVehicleToSteal")) CEventVehicleToSteal::InjectHooks();
        if (!IsHookDisabled("CEventVehicleDamage")) CEventVehicleDamage::InjectHooks();
        if (!IsHookDisabled("CEventVehicleDamageWeapon")) CEventVehicleDamageWeapon::InjectHooks();
        if (!IsHookDisabled("CEventVehicleDied")) CEventVehicleDied::InjectHooks();
        if (!IsHookDisabled("CEventVehicleOnFire")) CEventVehicleOnFire::InjectHooks();
        if (!IsHookDisabled("CEventVehicleHitAndRun")) CEventVehicleHitAndRun::InjectHooks();
        if (!IsHookDisabled("CEventVehicleCollision")) CEventVehicleCollision::InjectHooks();
        if (!IsHookDisabled("CEventVehicleDamageCollision")) CEventVehicleDamageCollision::InjectHooks();
        if (!IsHookDisabled("CEventHitByWaterCannon")) CEventHitByWaterCannon::InjectHooks();
        // + CEventInWater::InjectHooks();
        // + CEventCreatePartnerTask::InjectHooks();
        if (!IsHookDisabled("CEventInAir")) CEventInAir::InjectHooks();
        if (!IsHookDisabled("CEventStuckInAir")) CEventStuckInAir::InjectHooks();
        if (!IsHookDisabled("CEventAcquaintancePed")) CEventAcquaintancePed::InjectHooks();
        if (!IsHookDisabled("CEventLeaderEnteredCarAsDriver")) CEventLeaderEnteredCarAsDriver::InjectHooks();
        if (!IsHookDisabled("CEventLeaderExitedCarAsDriver")) CEventLeaderExitedCarAsDriver::InjectHooks();
        if (!IsHookDisabled("CEventLeaderQuitEnteringCarAsDriver")) CEventLeaderQuitEnteringCarAsDriver::InjectHooks();
        if (!IsHookDisabled("CEventAreaCodes")) CEventAreaCodes::InjectHooks();
        if (!IsHookDisabled("CEventLeaderEntryExit")) CEventLeaderEntryExit::InjectHooks();
        // + CEventSpecial::InjectHooks();
        // + CEventFireNearby::InjectHooks();
        // + CEventGroupEvent::InjectHooks();
        if (!IsHookDisabled("CEventGroup")) CEventGroup::InjectHooks();
        if (!IsHookDisabled("CEventGlobalGroup")) CEventGlobalGroup::InjectHooks();
        if (!IsHookDisabled("CEventPlayerCommandToGroup")) CEventPlayerCommandToGroup::InjectHooks();
        if (!IsHookDisabled("CEventPlayerCommandToGroupAttack")) CEventPlayerCommandToGroupAttack::InjectHooks();
        if (!IsHookDisabled("CEventPlayerCommandToGroupGather")) CEventPlayerCommandToGroupGather::InjectHooks();
        if (!IsHookDisabled("CEventDontJoinPlayerGroup")) CEventDontJoinPlayerGroup::InjectHooks();
        if (!IsHookDisabled("CEventNewGangMember")) CEventNewGangMember::InjectHooks();
        if (!IsHookDisabled("CEventEscalator")) CEventEscalator::InjectHooks();
        if (!IsHookDisabled("CEventDanger")) CEventDanger::InjectHooks();
        // + CEventSexyVehicle::InjectHooks();
        // + CEventChatPartner::InjectHooks();
        if (!IsHookDisabled("CEventCopCarBeingStolen")) CEventCopCarBeingStolen::InjectHooks();
        if (!IsHookDisabled("CEventHandler")) CEventHandler::InjectHooks();
        if (!IsHookDisabled("CEventAcquaintancePedHate")) CEventAcquaintancePedHate::InjectHooks();
        if (!IsHookDisabled("CEventAcquaintancePedHateBadlyLit")) CEventAcquaintancePedHateBadlyLit::InjectHooks();
        // + CEventInteriorUseInfo
        // + CEventLowAngerAtPlayer
        // + CEventPedEnteredMyVehicle
        // + CEventPotentialGetRunOver
        // + CEventPotentialWalkIntoBuilding::InjectHooks();
        // + CEventSexyPed
        // + CEventSignalAtPed
        // + CEventSoundLoud
        // - CEventSource::InjectHooks();
        // - CEventScanner::InjectHooks();
    };

    const auto Fx = []() {
        if (!IsHookDisabled("FxSystemBP_c")) FxSystemBP_c::InjectHooks();
        if (!IsHookDisabled("FxSystem_c")) FxSystem_c::InjectHooks();
        if (!IsHookDisabled("FxSphere_c")) FxSphere_c::InjectHooks();
        if (!IsHookDisabled("FxPrimBP_c")) FxPrimBP_c::InjectHooks();
        if (!IsHookDisabled("FxMemoryPool_c")) FxMemoryPool_c::InjectHooks();
        if (!IsHookDisabled("FxInfoManager_c")) FxInfoManager_c::InjectHooks();
        if (!IsHookDisabled("FxManager_c")) FxManager_c::InjectHooks();
        // ReversibleHooks::Install("FxFrustumInfo_c", "IsCollision", 0x4AA030, &FxFrustumInfo_c::IsCollision);
        if (!IsHookDisabled("FxEmitterPrt_c")) FxEmitterPrt_c::InjectHooks();
        if (!IsHookDisabled("FxEmitterBP_c")) FxEmitterBP_c::InjectHooks();
        if (!IsHookDisabled("FxEmitter_c")) FxEmitter_c::InjectHooks();
        if (!IsHookDisabled("Fx_c")) Fx_c::InjectHooks();
    };

    const auto Vehicle = []() {
        if (!IsHookDisabled("CAutomobile")) CAutomobile::InjectHooks();
        if (!IsHookDisabled("CBike")) CBike::InjectHooks();
        if (!IsHookDisabled("CBmx")) CBmx::InjectHooks();
        if (!IsHookDisabled("CBoat")) CBoat::InjectHooks();
        if (!IsHookDisabled("CHeli")) CHeli::InjectHooks();
        if (!IsHookDisabled("CMonsterTruck")) CMonsterTruck::InjectHooks();
        if (!IsHookDisabled("CPlane")) CPlane::InjectHooks();
        if (!IsHookDisabled("CQuadBike")) CQuadBike::InjectHooks();
        if (!IsHookDisabled("CTrailer")) CTrailer::InjectHooks();
        if (!IsHookDisabled("CTrain")) CTrain::InjectHooks();
        if (!IsHookDisabled("CTrainNode")) CTrainNode::InjectHooks();
        if (!IsHookDisabled("CVehicle")) CVehicle::InjectHooks();
    };

    const auto Interior = []() {
        if (!IsHookDisabled("FurnitureGroup_c")) FurnitureGroup_c::InjectHooks();
        if (!IsHookDisabled("FurnitureManager_c")) FurnitureManager_c::InjectHooks();
        if (!IsHookDisabled("FurnitureSubGroup_c")) FurnitureSubGroup_c::InjectHooks();
        if (!IsHookDisabled("Interior_c")) Interior_c::InjectHooks();
        if (!IsHookDisabled("InteriorGroup_c")) InteriorGroup_c::InjectHooks();
        if (!IsHookDisabled("InteriorManager_c")) InteriorManager_c::InjectHooks();
    };

    const auto Scripts = []() {
        if (!IsHookDisabled("CScriptsForBrains")) CScriptsForBrains::InjectHooks();
        if (!IsHookDisabled("CRunningScript")) CRunningScript::InjectHooks();
        if (!IsHookDisabled("CTheScripts")) CTheScripts::InjectHooks();
        if (!IsHookDisabled("CStreamedScripts")) CStreamedScripts::InjectHooks();
        if (!IsHookDisabled("CScripted2dEffects")) CScripted2dEffects::InjectHooks();
        if (!IsHookDisabled("CScriptResourceManager")) CScriptResourceManager::InjectHooks();
    };

    const auto App = []() {
        if (!IsHookDisabled("VideoPlayer")) VideoPlayer::InjectHooks();
        if (!IsHookDisabled("Securom")) Securom::InjectHooks();
        AppInjectHooks();
        Win32InjectHooks();
        RsInjectHooks();
        VideoModeInjectHooks();
    };

    const auto Animation = []() {
        if (!IsHookDisabled("CAnimBlendAssocGroup")) CAnimBlendAssocGroup::InjectHooks();
        if (!IsHookDisabled("CAnimBlendAssociation")) CAnimBlendAssociation::InjectHooks();
        if (!IsHookDisabled("CAnimBlendClumpData")) CAnimBlendClumpData::InjectHooks();
        if (!IsHookDisabled("CAnimBlendHierarchy")) CAnimBlendHierarchy::InjectHooks();
        if (!IsHookDisabled("CAnimBlendNode")) CAnimBlendNode::InjectHooks();
        if (!IsHookDisabled("CAnimBlendSequence")) CAnimBlendSequence::InjectHooks();
        if (!IsHookDisabled("CAnimBlendStaticAssociation")) CAnimBlendStaticAssociation::InjectHooks();
        if (!IsHookDisabled("CAnimManager")) CAnimManager::InjectHooks();
    };

    Animation();
    App();
    Audio();
    Tasks();
    Events();
    Fx();
    Vehicle();
    Interior();
    Scripts();
#if _DEBUG
    CCurves::TestCurves();
#endif
}

void InjectHooksMain(HMODULE hThisDLL) {
    ReversibleHooks::OnInjectionBegin(hThisDLL);
    InjectHooksMain();
    ReversibleHooks::OnInjectionEnd();
}
