/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "Train.h"

#include "Buoyancy.h"
#include "CarCtrl.h"

// train-1/train-2 batch: headers required by the reversed functions below and NOT
// already pulled in by StdInc.h (FileMgr.h / Replay.h / Population.h / TaskComplexSequence.h
// are already in StdInc.h - verified)
#include "Pipelines/CustomBuilding/CustomBuildingDNPipeline.h"                            // 0x6F6CC0: CCustomBuildingDNPipeline::m_fDNBalanceParam
#include "Tasks/TaskTypes/TaskComplexEnterCar.h"                                          // 0x6F6850 + 0x6F8170: enter-car task check (GetTargetCar)
#include "Tasks/TaskTypes/TaskComplexEnterCarAsDriver.h"                                  // 0x6F8170
#include "Tasks/TaskTypes/TaskComplexEnterCarAsPassenger.h"                               // 0x6F8170
#include "Tasks/TaskTypes/TaskComplexLeaveCarAndWander.h"                                 // 0x6F6850
#include "Tasks/TaskTypes/SeekEntity/TaskComplexSeekEntity.h"                             // 0x6F8170
#include "Tasks/TaskTypes/SeekEntity/PosCalculators/EntitySeekPosCalculatorXYOffset.h"    // 0x6F8170

CVector CTrain::aStationCoors[6] = { // 0x8D48F8
    CVector{ 1741.0f, -1954.0f, 15.0f },
    CVector{ 1297.0f, -1898.0f, 3.0f  },
    CVector{ -1945.0f, 128.0f,  29.0f },
    CVector{ 1434.0f,  2632.0f, 13.0f },
    CVector{ 2783.0f,  1758.0f, 12.0f },
    CVector{ 2865.0f,  1281.0f, 12.0  }
};

auto& pTrackNodes = StaticRef<CTrainNode*[4]>(0xC38024);
auto& NumTrackNodes = StaticRef<std::array<int32, 4>>(0xC38014);
auto& arrTotalTrackLength = StaticRef<std::array<float, 4>>(0xC37FEC);
auto& StationDist = StaticRef<std::array<float, 6>>(0xC38034);

// train-2, file-scope globals (s_aTrainConfigs used by 0x6F7550 + 0x6F7900; the other two by 0x6F7900)
// Train configuration table @ 0x8D44F8 - rows of 16 model ids (0 terminated)
static auto& s_aTrainConfigs = StaticRef<std::array<uint32, 0x100>>(0x8D44F8);
static auto& s_GenTrainRoundRobin    = StaticRef<uint32>(0xC38064); // last train config used (round robin)
static auto& s_bPlayerNearSpawnPoint = StaticRef<bool>(0xC38068);   // player near any random train spawn point

// train-2, 0x6F6CC0 only: floats from the original's .rdata (kept as our own storage so
// they compile to memory operands, like the original - it never keeps these in registers)
namespace {
    volatile const float FLT_1_DIV_3     = 0.33333334f;  // 0x859040 - train-node distance-from-start scale (stored * 3)
    volatile const float FLT_1_DIV_8     = 0.125f;       // 0x858C48 - train-node coordinate scale (stored * 8)
    volatile const float FLT_ZERO_858B50 = 0.0f;         // 0x858B50
    volatile const float FLT_ONE_858624  = 1.0f;         // 0x858624
    volatile const float FLT_HALF_858B8C = 0.5f;         // 0x858B8C
    volatile const float FLT_3_858B3C    = 3.0f;         // 0x858B3C - max distance from track segment
    volatile const float FLT_0858CDC     = 0.001f;       // 0x858CDC - min projection along segment
    volatile const float FLT_0872490     = 1.001f;       // 0x872490 - max projection along segment
    volatile const float FLT_1_DIV_30    = 0.033333335f; // 0x858F10 - lighting scale (nibble * 1/30 == GetCurrentLighting inlined)
}

void CTrain::InjectHooks() {
    RH_ScopedVirtualClass(CTrain, 0x872370, 66);
    RH_ScopedCategory("Vehicle");

    RH_ScopedInstall(Constructor, 0x6F6030);
    RH_ScopedInstall(InitTrains, 0x6F7440);
    RH_ScopedInstall(ReadAndInterpretTrackFile, 0x6F55D0);
    RH_ScopedInstall(Shutdown, 0x6F58D0);
    RH_ScopedInstall(UpdateTrains, 0x6F5900);
    RH_ScopedInstall(FindCoorsFromPositionOnTrack, 0x6F59E0);
    RH_ScopedInstall(FindMaximumSpeedToStopAtStations, 0x6F5BA0);
    RH_ScopedInstall(FindNumCarriagesPulled, 0x6F5CD0);
    RH_ScopedInstall(OpenTrainDoor, 0x6F5D80);
    RH_ScopedInstall(AddPassenger, 0x6F5D90);
    RH_ScopedInstall(RemovePassenger, 0x6F5DA0);
    RH_ScopedInstall(DisableRandomTrains, 0x6F5DB0);
    RH_ScopedInstall(RemoveOneMissionTrain, 0x6F5DC0);
    RH_ScopedInstall(ReleaseOneMissionTrain, 0x6F5DF0);
    RH_ScopedInstall(SetTrainSpeed, 0x6F5E20);
    RH_ScopedInstall(SetTrainCruiseSpeed, 0x6F5E50);
    RH_ScopedInstall(FindCaboose, 0x6F5E70);
    RH_ScopedInstall(FindEngine, 0x6F5E90);
    RH_ScopedInstall(FindCarriage, 0x6F5EB0);
    RH_ScopedInstall(FindSideStationIsOn, 0x6F5EF0);
    RH_ScopedInstall(FindNextStationPositionInDirection, 0x6F5F00);
    RH_ScopedInstall(IsInTunnel, 0x6F6320);
    RH_ScopedInstall(RemoveRandomPassenger, 0x6F6850);
    RH_ScopedInstall(RemoveMissionTrains, 0x6F6A20);
    RH_ScopedInstall(RemoveAllTrains, 0x6F6AA0);
    RH_ScopedInstall(ReleaseMissionTrains, 0x6F6B60);
    RH_ScopedInstall(FindClosestTrackNode, 0x6F6BD0);
    RH_ScopedInstall(FindPositionOnTrackFromCoors, 0x6F6CC0);
    RH_ScopedInstall(FindNearestTrain, 0x6F7090);
    RH_ScopedInstall(SetNewTrainPosition, 0x6F7140);
    RH_ScopedInstall(IsNextStationAllowed, 0x6F7260);
    RH_ScopedInstall(SkipToNextAllowedStation, 0x6F72F0);
    RH_ScopedInstall(CreateMissionTrain, 0x6F7550);
    RH_ScopedInstall(DoTrainGenerationAndRemoval, 0x6F7900);
    RH_ScopedInstall(AddNearbyPedAsRandomPassenger, 0x6F8170);
    RH_ScopedVMTInstall(ProcessControl, 0x6F86A0);

    RH_ScopedGlobalInstall(ProcessTrainAnnouncements, 0x6F5910);
    RH_ScopedGlobalInstall(PlayAnnouncement, 0x6F5920);
    RH_ScopedGlobalInstall(MarkSurroundingEntitiesForCollisionWithTrain, 0x6F6640);
    RH_ScopedGlobalInstall(TrainHitStuff<CPtrListSingleLink<CPhysical*>>, 0x6F5CF0, { .Reversed = false }); // NOT in batch - stub kept as-is
}

// 0x6F6030 (train-1)
// Original: byte 0x5B8 = (b & 0xD9) | 0x81, byte 0x5B9 = (b & 0xFA) | 0x02, i.e.
// bIsFrontCarriage/bMissionTrain/bClockwiseDirection/bIsStreakModel are kept as-is
// (CreateMissionTrain sets them); the rest is set/cleared below.
CTrain::CTrain(int32 modelIndex, eVehicleCreatedBy createdBy) : CVehicle(createdBy) {
    m_nVehicleSubType = VEHICLE_TYPE_TRAIN;
    m_nVehicleType = VEHICLE_TYPE_TRAIN;

    const auto mi = CModelInfo::GetModelInfo(modelIndex)->AsVehicleModelInfoPtr();
    m_pHandlingData = gHandlingDataMgr.GetVehiclePointer(mi->m_nHandlingId);
    m_nHandlingFlagsIntValue = m_pHandlingData->m_nHandlingFlags;

    CVehicle::SetModelIndex(modelIndex);
    SetupModelNodes();

    std::memset(&m_aDoors, 0, sizeof(m_aDoors)); // original zeroes 0x5D8..0x667 (0x90 bytes)
    if (m_nModelIndex == MODEL_STREAKC) {
        m_aDoors[DOOR_LEFT_FRONT].Init(1.25f, 0.25f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
        m_aDoors[DOOR_RIGHT_FRONT].Init(1.25f, 0.25f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
    } else {
        m_aDoors[DOOR_LEFT_FRONT].Init(TWO_PI / -5.0f, 0.0f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
        m_aDoors[DOOR_RIGHT_FRONT].Init(TWO_PI / +5.0f, 0.0f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
    }

    {
        trainFlags.b01 = true;                            // initialised with 1
        trainFlags.bStoppedAtStation = false;
        trainFlags.bPassengersCanEnterAndLeave = false;
        trainFlags.bIsLastCarriage = false;
        trainFlags.bStopsAtStations = true;
        trainFlags.bNotOnARailRoad = false;               // cleared by the `& 0xFA`
        trainFlags.bForceSlowDown = true;                 // bit 9
        // bIsFrontCarriage (bit 3), bMissionTrain (bit 5), bClockwiseDirection (bit 6)
        // and bIsStreakModel (bit 10) are kept as-is
    }

    m_nPassengersGenerationState = 0;
    m_nNumPassengersToEnter = CGeneral::GetRandomNumber() & 3;       // low nibble of 0x5CB
    m_nNumPassengersToLeave = (CGeneral::GetRandomNumber() & 3) + 1; // high nibble of 0x5CB
    m_pTemporaryPassenger = nullptr;
    m_nMaxPassengers = 5;
    physicalFlags.bDisableSimpleCollision = true;
    SetUsesCollision(true);
    m_nTimeWhenCreated = CTimer::GetTimeInMS();
    field_5C8 = 0;
    m_nTrackId = 0;
    m_fCurrentRailDistance = 0.0f;
    m_fTrainSpeed = 0.0f;
    m_nTimeWhenStoppedAtStation = 0;
    mi->ChooseVehicleColour(m_nPrimaryColor, m_nSecondaryColor, m_nTertiaryColor, m_nQuaternaryColor, 1);
    m_fMass = m_pHandlingData->m_fMass;
    m_fTurnMass = m_pHandlingData->m_fTurnMass;
    m_vecCentreOfMass = m_pHandlingData->m_vecCentreOfMass;
    m_fElasticity = 0.05f;
    m_fBuoyancyConstant = m_pHandlingData->m_fBuoyancyConstant;
    m_fAirResistance = GetDefaultAirResistance();

    physicalFlags.bAddMovingCollisionSpeed = true;
    m_bTunnelTransition = true;
    m_pPrevCarriage = nullptr;
    m_pNextCarriage = nullptr;
    SetStatus(STATUS_TRAIN_MOVING);
    m_autoPilot.m_speed = 0.0f;
    m_autoPilot.SetCruiseSpeed(0);
    m_vehicleAudio.Initialise(this);
}

void CTrain::SetupModelNodes() {
    std::ranges::fill(m_aTrainNodes, nullptr);
    CClumpModelInfo::FillFrameArray(GetRpClump(), m_aTrainNodes.data());
}

// 0x6F7440 (train-2)
void CTrain::InitTrains() {
    ZoneScoped;

    bDisableRandomTrains = false; // DAT_00c38010
    GenTrain_Status = 0;          // DAT_00c3800c

    constexpr const char* filenames[] {
        "data\\paths\\tracks.dat",
        "data\\paths\\tracks3.dat",
        "data\\paths\\tracks2.dat",
        "data\\paths\\tracks4.dat",
    };
    for (auto i = 0u; i < std::size(pTrackNodes); ++i) {
        if (!pTrackNodes[i]) {
            CTrain::ReadAndInterpretTrackFile(filenames[i], pTrackNodes, NumTrackNodes.data(), arrTotalTrackLength.data(), i);
        }
    }

    for (auto i = 0u; i < std::size(aStationCoors); ++i) {
        int32 trackId;
        CTrain::FindClosestTrackNode(aStationCoors[i], &trackId);
        auto distance = (float)pTrackNodes[trackId]->m_nDistanceFromStart;
        StationDist[i] = distance * StaticRef<float>(0x859040); // * 1/3 (0.33333334f), ftol-source is m_nDistanceFromStart (uint16)
    }
}

// 0x6F55D0 (train-1)
// Difference from the stash (per decompile): the written-stations counter is a local, not a
// static - the original starts writing at aStationCoors[0] on every call. The overflow guard
// on aStationCoors is kept (not in the original, but tracks.dat holds exactly 6 stations).
void CTrain::ReadAndInterpretTrackFile(const char* filename, CTrainNode** nodes, int32* lineCount, float* totalDist, int32 skipStations) {
    if (*nodes) {
        // Track already loaded - only (re)compute the node distances from start + the total track length
        if (*lineCount > 0) {
            float totalLength = 0.0f;
            for (int32 i = 0; i < *lineCount; i++) {
                auto&      node = (*nodes)[i];
                const auto next = (*nodes)[(i + 1) % *lineCount];

                node.m_nDistanceFromStart = int16(totalLength * 3.0f); // ftol'ed in the original (FUN_00821B40)

                const float dx = node.GetX() - next.GetX();
                const float dy = node.GetY() - next.GetY();
                totalLength += sqrtf(sq(dx) + sq(dy));
            }
            *totalDist = totalLength;
        } else {
            *totalDist = 0.0f;
        }
        return;
    }

    // Load the file (0xB530 bytes max) and parse it line by line
    auto fileData = std::make_unique<char[]>(0xB530);
    CFileMgr::LoadFile(filename, reinterpret_cast<uint8*>(fileData.get()), 0xB530, "rb"); // FUN_00538890

    char lineBuffer[256]; // scratch buffer of the original at 0xB71670

    // First line - optionally the "processed" marker, followed by the actual header
    {
        size_t numWrittenStations = 0; // original restarts writing at aStationCoors[0] on every call

        int32 len = 0;
        for (const char* p = fileData.get(); *p != '\n'; p++) {
            lineBuffer[len++] = *p;
        }
        lineBuffer[len] = '\0';
        int32 bytesRead = len + 1;

        if (strncmp(lineBuffer, "processed", 10) == 0) { // 10 bytes are compared in the original
            len = 0;
            for (const char* p = fileData.get() + bytesRead; *p != '\n'; p++) {
                lineBuffer[len++] = *p;
            }
            lineBuffer[len] = '\0';
            bytesRead += len + 1;
        }
        sscanf(lineBuffer, "%d", lineCount);

        auto* nodeArray = new CTrainNode[*lineCount]; // operator new + FUN_00403D60 in the original
        *nodes = nodeArray;

        // Node lines: "x y z dist"
        for (int32 i = 0; i < *lineCount; i++) {
            int32 len = 0;
            for (const char* p = fileData.get() + bytesRead; *p != '\n'; p++) {
                lineBuffer[len++] = *p;
            }
            lineBuffer[len] = '\0';
            bytesRead += len + 1;

            float x{}, y{}, z{};
            int32 dist{};
            sscanf(lineBuffer, "%f %f %f %d", &x, &y, &z, &dist);

            auto& node = (*nodes)[i];
            node.SetX(x); // int16(x * 8) - FUN_00821B40 (ftol) in the original
            node.SetY(y);
            node.SetZ(z);

            // Remember station positions for the first track (overwrites CTrain::aStationCoors)
            if (skipStations == 0 && dist != 0) {
                if (numWrittenStations < std::size(aStationCoors)) { // not in the original, but tracks.dat has exactly 6 stations
                    aStationCoors[numWrittenStations++] = CVector{ x, y, z };
                }
            }
        }
    }
}

// 0x6F58D0
void CTrain::Shutdown() {
    for (auto node : pTrackNodes) {
        delete node;
        node = nullptr;
    }
}

// 0x6F5900
void CTrain::UpdateTrains() {
    ZoneScoped;

    // NOP
}

// 0x6F5910
void ProcessTrainAnnouncements() {
    // NOP
}

// 0x6F5920
void PlayAnnouncement(uint8 arg0, uint8 arg1) {
    // NOP
}

// 0x6F59E0 (train-1)
// NOTE: port of the stash; the CTrainNode accessors are inlined in the original, so the
// fields are read directly here as well.
void CTrain::FindCoorsFromPositionOnTrack(float railDistance, int32 trackId, CVector* outCoors) {
    const auto numNodes = NumTrackNodes[trackId];
    if (numNodes <= 0) {
        return;
    }
    const auto* nodes = pTrackNodes[trackId];

    // Find the segment the given rail distance lies on
    int32 nodeIdx = 0;
    float distFromNode   = 0.0f;
    float distToNextNode = 0.0f;
    for (;; nodeIdx++) {
        distFromNode   = railDistance - (float)nodes[nodeIdx].m_nDistanceFromStart * StaticRef<float>(0x859040);          // * 1/3 (distances stored * 3)
        distToNextNode = (float)nodes[(nodeIdx + 1) % numNodes].m_nDistanceFromStart * StaticRef<float>(0x859040) - railDistance;
        if (0.0f <= distFromNode && 0.0f <= distToNextNode) { // 0x858B50 == 0.0f in the original
            break;
        }
        if (nodeIdx + 1 >= numNodes) {
            return;
        }
    }

    const CTrainNode& cur  = nodes[nodeIdx];
    const CTrainNode& next = nodes[(nodeIdx + 1) % numNodes];

    const float totalDist = distFromNode + distToNextNode;
    const float t = 1.0f / totalDist; // 1.0f from 0x858624

    // Coordinate scale: nodes are stored * 8 (0x858C48 == 0.125f)
    (*outCoors).x = ((float)cur.x * 0.125f * distToNextNode + (float)next.x * 0.125f * distFromNode) * t;
    (*outCoors).y = ((float)cur.y * 0.125f * distToNextNode + (float)next.y * 0.125f * distFromNode) * t;
    (*outCoors).z = ((float)cur.z * 0.125f * distToNextNode + (float)next.z * 0.125f * distFromNode) * t;
}

// 0x6F5BA0 (train-1)
bool CTrain::FindMaximumSpeedToStopAtStations(float* speed) {
    *speed = 50.0f;

    if (m_nTrackId != 0) { // only the main loop track has stations
        return false;
    }

    const bool  bClockwise  = trainFlags.bClockwiseDirection;
    const float totalLength = arrTotalTrackLength[0];
    float       closestStationDist = 10000.0f;

    for (const auto stationDist : StationDist) {
        // Distance to the station, offset by the stopping distance (~40m)
        float dist = stationDist - m_fCurrentRailDistance;
        dist += bClockwise ? StaticRef<float>(0x858A10) : -StaticRef<float>(0x858A10); // +/- 40.0f

        // Wrap into [-totalLength / 2, +totalLength / 2)
        for (; totalLength * StaticRef<float>(0x858B8C) < dist; dist -= totalLength) { // * 0.5f
        }
        for (; dist < totalLength * StaticRef<float>(0x858F40); dist += totalLength) { // * -0.5f
        }

        // Only stations ahead of us matter
        if (!bClockwise) {
            if (dist >= 0.0f) {
                continue;
            }
            dist = -dist;
        } else {
            if (dist <= 0.0f) {
                continue;
            }
        }

        if (dist < closestStationDist) {
            closestStationDist = dist;
        }
    }

    if (closestStationDist >= StaticRef<float>(0x858B58)) { // 500.0f - no station near enough to slow down for
        *speed = 100000.0f;
    } else {
        // Slow down the closer we get
        *speed = (1.0f - (StaticRef<float>(0x858B58) - closestStationDist) * StaticRef<float>(0x858F44)) // * 0.002f
               * StaticRef<float>(0x858B40);                                                            // * 50.0f
    }

    // Return true if we effectively have to stop at a station
    return closestStationDist < StaticRef<float>(0x858C80); // 5.0f
}

// 0x6F5CD0
uint32 CTrain::FindNumCarriagesPulled() {
    uint32 num;
    CTrain* carriage = m_pNextCarriage;
    for (num = 0; carriage; ++num) {
        carriage = carriage->m_pNextCarriage;
    }
    return num;
}

// 0x6F5D80
void CTrain::OpenTrainDoor(float state) {
    // NOP
}

// 0x6F5D90
void CTrain::AddPassenger(CPed* ped) {
    // NOP
}

// 0x6F5DA0
void CTrain::RemovePassenger(CPed* ped) {
    // NOP
}

// 0x6F5DB0
void CTrain::DisableRandomTrains(bool disable) {
    bDisableRandomTrains = disable;
}

// 0x6F5DC0
void CTrain::RemoveOneMissionTrain(CTrain* train) {
    if (!train)
        return;

    CTrain* next;
    CTrain* _train = train;
    do {
        next = _train->m_pNextCarriage;
        CWorld::Remove(_train);
        delete _train;
        _train = next;
    } while (next);
}

// 0x6F5DF0
void CTrain::ReleaseOneMissionTrain(CTrain* train) {
    for (auto* head = train; head; head = head->m_pNextCarriage) {
        head->trainFlags.bMissionTrain = false;
    }
}

// 0x6F5E20
void CTrain::SetTrainSpeed(CTrain* train, float speed) {
    train->m_fTrainSpeed = speed / 50.0f;
    if (!train->trainFlags.bClockwiseDirection) {
        train->m_fTrainSpeed = -train->m_fTrainSpeed;
    }
}

// 0x6F5E50
void CTrain::SetTrainCruiseSpeed(CTrain* train, float speed) {
    train->m_autoPilot.SetCruiseSpeed((uint8)speed);
}

// 0x6F5E70 (train-1)
// NOTE: the decompile loses the return value (void); signature per header is CTrain*(CTrain*);
// the m_pNextCarriage (+0x5D4) loop matches 1:1 - result is the last carriage.
CTrain* CTrain::FindCaboose(CTrain* train) {
    for (auto* caboose = train->m_pNextCarriage; caboose; caboose = caboose->m_pNextCarriage) {
        train = caboose;
    }
    return train;
}

// 0x6F5E90 (train-1)
// NOTE: same as FindCaboose - decompile loses the return; the m_pPrevCarriage (+0x5D0) loop
// matches - result is the head carriage.
CTrain* CTrain::FindEngine(CTrain* train) {
    for (auto* engine = train->m_pPrevCarriage; engine; engine = engine->m_pPrevCarriage) {
        train = engine;
    }
    return train;
}

// 0x6F5EB0 (train-1)
CTrain* CTrain::FindCarriage(CTrain* train, uint8 carriageIdx) {
    if (carriageIdx == 0) {
        return train;
    }
    for (uint8 i = 0;; ++i) {
        train = train->m_pNextCarriage;
        if (!train) {
            return nullptr;
        }
        if (carriageIdx <= i + 1) { // original increments then tests `carriageIdx <= counter`
            return train;
        }
    }
}

// 0x6F5EF0
bool CTrain::FindSideStationIsOn() const {
    return trainFlags.bClockwiseDirection; // ?
}

// 0x6F5F00 (train-1)
void CTrain::FindNextStationPositionInDirection(bool clockwiseDirection, float distance, float* distanceToStation, int32* numStations) {
    // Find the first station that lies ahead of the given distance
    int32 i = 0;
    for (; i < 6; ++i) {
        if (distance < StationDist[i]) {
            break;
        }
    }
    if (i >= 6) {
        i = 0;
    }

    int32 station = i;
    if (!clockwiseDirection) {
        station = i - 1;
        if (station < 0) {
            station = i + 5;
        }
    }

    if (std::abs(distance - StationDist[station]) < StaticRef<float>(0x858628) /* 5.0f */) {
        // Too close to this station - give the next/previous one instead
        if (!clockwiseDirection) {
            station--;
        } else {
            station++;
        }
        if (station < 0) {
            station += 6;
        }
        if (station > 5) {
            station = 0;
        }
    }

    *numStations       = station;
    *distanceToStation = StationDist[station];
}

// 0x6F6320
bool CTrain::IsInTunnel() const {
    const auto& pos = GetPosition();
    CColPoint colPoint{};
    CEntity* colEntity{};
    return CWorld::ProcessVerticalLine(pos, pos.z + 100.0f, colPoint, colEntity, true);
}

// 0x6F6640
void MarkSurroundingEntitiesForCollisionWithTrain(CVector pos, float radius, CEntity* entity, bool bOnlyVehicles) {
    int32 startSectorX = std::max(CWorld::GetSectorX(pos.x - radius), 0);
    int32 startSectorY = std::max(CWorld::GetSectorY(pos.y - radius), 0);
    int32 endSectorX   = std::min(CWorld::GetSectorX(pos.x + radius), MAX_SECTORS_X - 1);
    int32 endSectorY   = std::min(CWorld::GetSectorY(pos.y + radius), MAX_SECTORS_Y - 1);

    CWorld::AdvanceCurrentScanCode();

    for (int32 sectorY = startSectorY; sectorY <= endSectorY; ++sectorY) {
        for (int32 sectorX = startSectorX; sectorX <= endSectorX; ++sectorX) {
            auto& repeatSector = CWorld::GetRepeatSector(sectorX, sectorY);
            TrainHitStuff(repeatSector.Vehicles, entity);
            if (!bOnlyVehicles) {
                TrainHitStuff(repeatSector.Peds, entity);
                TrainHitStuff(repeatSector.Objects, entity);
            }
        }
    }
}

// 0x6F5CF0 - NOT in batch, stub kept as-is
template<typename PtrListType>
void TrainHitStuff(PtrListType& ptrList, CEntity* entity) {
    ((void(__cdecl*)(PtrListType&, CEntity*))0x6F5CF0)(ptrList, entity);
}

// 0x6F6850 (train-1)
// Integration note (per decompile): intelligence+0x10 is the player ped's primary task
// (CTaskManager::m_aPrimaryTasks[TASK_PRIMARY_PRIMARY]); the original checks that it is an
// enter-car task (types 700/701) whose target car (CTaskComplexEnterCar::m_Car, +0xC) is this
// train. `AsComplex()` only yields CTaskComplex*, which has no GetTargetCar(), hence the
// explicit cast to CTaskComplexEnterCar.
void CTrain::RemoveRandomPassenger() {
    if (CReplay::Mode == MODE_PLAYBACK) { // DAT_00A43088
        return;
    }

    const auto* playerPed = FindPlayerPed(-1);
    if (playerPed->m_pVehicle == this) {
        return;
    }

    // Don't remove a passenger if someone is (already) entering this train:
    // the player ped's primary task is an enter-car task targeting this train
    if (const auto* task = playerPed->GetTaskManager().GetTaskPrimary(TASK_PRIMARY_PRIMARY)) {
        const auto taskType = task->GetTaskType();
        if ((taskType == TASK_COMPLEX_ENTER_CAR_AS_DRIVER || taskType == TASK_COMPLEX_ENTER_CAR_AS_PASSENGER) // 0x2BD / 700
            && static_cast<const CTaskComplexEnterCar*>(task)->GetTargetCar() == this
        ) {
            return;
        }
    }

    const bool bClockwise = trainFlags.bClockwiseDirection; // byte 0x5B8 bit 6

    // If the player is in this train, only remove a passenger from the "correct" side
    // (dead in practice - we already returned if the player's m_pVehicle is this)
    if (playerPed->bInVehicle && playerPed->m_pVehicle == this) {
        if (bClockwise) {
            if (m_pDriver == playerPed) {
                return;
            }
        } else if (m_pDriver != playerPed) {
            return;
        }
    }

    if (m_nNumPassengersToLeave == 0) {
        return;
    }

    if (m_pTemporaryPassenger) {
        if (m_pTemporaryPassenger->bInVehicle) { // +0x46C & 0x100
            return;
        }
        m_pTemporaryPassenger->CleanUpOldReference(reinterpret_cast<CEntity**>(&m_pTemporaryPassenger)); // FUN_00571A00
        m_pTemporaryPassenger = nullptr;
    }

    // Pick a random passenger ped model
    auto model = CGeneral::GetRandomNumberInRange(0, 12, true); // FUN_00407180(0, 0xC) - inclusive [0, 12]
    if (model == 12) {
        model = 11;
    }
    if ((CGeneral::GetRandomNumber() & 0xFFFF) < 100) {
        model = 25; // 0x19
    }

    if (auto* ped = CPopulation::AddPedInCar(this, !bClockwise, model, 0, false, false)) { // FUN_00613A00
        ped->bJustGotOffTrain = true; // CPed +0x478 |= 0x40000

        m_nNumPassengersToLeave--; // low nibble of byte +0x5CB

        m_pTemporaryPassenger = ped;
        m_pTemporaryPassenger->RegisterReference(reinterpret_cast<CEntity**>(&m_pTemporaryPassenger)); // FUN_00571B70

        if (auto* leaveTask = new CTaskComplexLeaveCarAndWander(this, TARGET_DOOR_FRONT_LEFT, 0, true)) { // FUN_0063BDD0(this, 0, 0, 1)
            ped->GetTaskManager().SetTask(leaveTask, TASK_PRIMARY_PRIMARY, false); // FUN_00681AF0(task, 3, 0)
        }
    }
}

// 0x6F6A20
void CTrain::RemoveMissionTrains() {
    for (auto& vehicle : GetVehiclePool()->GetAllValid()) {
        if (vehicle.IsTrain() &&
            &vehicle != FindPlayerVehicle() &&
            vehicle.AsTrain()->trainFlags.bMissionTrain
        ) {
            CWorld::Remove(&vehicle);
            delete &vehicle;
        }
    }
}

// 0x6F6AA0 (train-1)
// Difference from the stash (per decompile): both do-while loops start checking with the
// carriage itself (puVar7/puVar6 = puVar9 before the first chain step), so the loops run
// from `train`, not from m_pPrevCarriage/m_pNextCarriage. Also: pool->GetAt() yields
// CVehicle*, and m_pPrevCarriage/m_pNextCarriage are CTrain members - hence AsTrain().
void CTrain::RemoveAllTrains() {
    auto* pool = GetVehiclePool();
    for (int32 i = (int32)pool->GetSize() - 1; i >= 0; --i) { // reverse-iterated in the original
        auto* veh = pool->GetAt(i);
        if (!veh || !veh->IsTrain()) {
            continue;
        }
        auto* train = veh->AsTrain();

        // Don't remove the train the player is in (the carriage itself + prev and next chains)
        bool bPlayerTrain = false;
        for (auto* carriage = train; carriage; carriage = carriage->m_pPrevCarriage) { // +0x5D0
            if (carriage == FindPlayerVehicle(-1, false)) { // FUN_0056E0D0(-1, 0) - called on every step
                bPlayerTrain = true;
            }
        }
        for (auto* carriage = train; carriage; carriage = carriage->m_pNextCarriage) { // +0x5D4
            if (carriage == FindPlayerVehicle(-1, false)) {
                bPlayerTrain = true;
            }
        }

        if (!bPlayerTrain) {
            CWorld::Remove(train); // FUN_00563280
            delete train;          // (**vtbl)(1) in the original
        }
    }
}

// 0x6F6B60
void CTrain::ReleaseMissionTrains() {
    for (auto& vehicle : GetVehiclePool()->GetAllValid()) {
        if (vehicle.IsTrain() && &vehicle != FindPlayerVehicle()) {
            vehicle.AsTrain()->trainFlags.bMissionTrain = false;
        }
    }
}

// 0x6F6BD0 (train-2)
int32 CTrain::FindClosestTrackNode(CVector posn, int32* outTrackId) {
    float closestDist = 99999.9f;
    int32 closestNode = 0;

    for (int32 trackId = 0; trackId < 4; ++trackId) {
        const auto numNodes = NumTrackNodes[trackId];
        if (numNodes <= 0) {
            continue;
        }
        for (int32 i = 0; i < numNodes; ++i) {
            const float dist = DistanceBetweenPoints(posn, pTrackNodes[trackId][i].GetPosn());
            if (dist < closestDist) {
                *outTrackId = trackId;
                closestNode = i;
                closestDist = dist;
            }
        }
    }
    return closestNode;
}

// 0x6F6CC0 (train-2)
// NOTE: Faithful port - the CTrainNode accessors and the colmodel lookup are
// inlined in the original, so they're done directly here as well. Speed
// magnitude is computed inline (the original has no call to CVector::Magnitude).
void CTrain::FindPositionOnTrackFromCoors() {
    if (NumTrackNodes[m_nTrackId] < 1) {
        return;
    }

    auto* const nodes    = pTrackNodes[m_nTrackId];
    const auto  numNodes = NumTrackNodes[m_nTrackId];

    for (int32 nodeIdx = 0;; nodeIdx++) {
        const int32 nextIdx = (nodeIdx + 1) % numNodes;
        CTrainNode& node = nodes[nodeIdx];
        CTrainNode& next = nodes[nextIdx];

        const CVector& pos = GetPosition(); // matrix+0x30 / placement+0x4 (inlined in the original)

        const float nodeX = (float)node.x * FLT_1_DIV_8;
        const float nodeY = (float)node.y * FLT_1_DIV_8;
        const float nextX = (float)next.x * FLT_1_DIV_8;
        const float nextY = (float)next.y * FLT_1_DIV_8;

        const float dx = nextX - nodeX;
        const float dy = nextY - nodeY;

        // NOTE: fsqrt inline (the original has no sqrtf calls)
        float segLen, distSq, dist;
        const float dxv = dx;
        const float dyv = dy;
        __asm {
            fld dword ptr dxv
            fmul dword ptr dxv
            fld dword ptr dyv
            fmul dword ptr dyv
            fadd
            fstp dword ptr distSq
        }
        __asm {
            fld dword ptr distSq
            fsqrt
            fstp dword ptr segLen
        }
        const float t = ((pos.x - nodeX) * (nextX - nodeX)
                       + (pos.y - nodeY) * (nextY - nodeY)) / (segLen * segLen);

        if (FLT_0858CDC < t && t < FLT_0872490) { // 0.001f < t < 1.001f
            const float projX = (nextX - nodeX) * t + nodeX - pos.x;
            const float projY = (nextY - nodeY) * t + nodeY - pos.y;

            // Close enough to the track segment
            distSq = projX * projX + projY * projY + t * FLT_ZERO_858B50 * (t * FLT_ZERO_858B50);
            __asm {
                fld dword ptr distSq
                fsqrt
                fstp dword ptr dist
            }
            if (dist < FLT_3_858B3C) { // 3.0f
                // Rail distance on this segment
                float railDist = (float)node.m_nDistanceFromStart * FLT_1_DIV_3;
                railDist = ((float)next.m_nDistanceFromStart * FLT_1_DIV_3 - railDist) * t + railDist;
                m_fCurrentRailDistance = railDist;

                // Subtract half the carriage length (model bounding box) - inlined in the original
                // NOTE: GetBoundingBox() is non-const (ColModel.h), so no const pointer here
                auto* colModel = CModelInfo::GetModelInfo(m_nModelIndex)->GetColModel();
                railDist -= (colModel->GetBoundingBox().m_vecMax.y - colModel->GetBoundingBox().m_vecMin.y) * FLT_HALF_858B8C;
                m_fCurrentRailDistance = railDist;

                if (railDist < FLT_ZERO_858B50) {
                    m_fCurrentRailDistance = railDist + arrTotalTrackLength[m_nTrackId];
                }

                {
                    const CVector& v = m_vecMoveSpeed;
                    distSq = v.x * v.x + v.y * v.y + v.z * v.z;
                }
                float speed;
                __asm {
                    fld dword ptr distSq
                    fsqrt
                    fstp dword ptr speed
                }
                m_fTrainSpeed = speed;

                const float dot = (nextX - nodeX) * m_vecMoveSpeed.x
                                + m_vecMoveSpeed.z * FLT_ZERO_858B50 // * 0.0f (train never leaves the rails)
                                + (nextY - nodeY) * m_vecMoveSpeed.y;
                if (trainFlags.bClockwiseDirection != (FLT_ZERO_858B50 < dot)) {
                    return;
                }
                m_fTrainSpeed = -m_fTrainSpeed;
                return;
            }

            // Not near the track - interpolate the node lighting into the carriage's
            // contact-surface brightness (the original inlines tColLighting::GetCurrentLighting
            // here: night * 1/30 * mix + day * 1/30 * (1 - mix))
            const auto& mixFactor = CCustomBuildingDNPipeline::m_fDNBalanceParam; // 0x8D12C0
            const auto lighting1 = node.GetLightingFromCollision().value;
            const float lightA = (float)(lighting1 >> 4) * FLT_1_DIV_30 * mixFactor
                               + (FLT_ONE_858624 - mixFactor) * (float)(lighting1 & 0xF) * FLT_1_DIV_30;
            const auto lighting2 = next.GetLightingFromCollision().value;
            m_fContactSurfaceBrightness = // float at CPhysical (CTrain + 0x12C)
                ((float)(lighting2 >> 4) * FLT_1_DIV_30 * mixFactor
              + (FLT_ONE_858624 - mixFactor) * (float)(lighting2 & 0xF) * FLT_1_DIV_30) * t + lightA;
        }

        if (nodeIdx + 1 >= numNodes) {
            return;
        }
    }
}

// 0x6F7090 (train-2)
CTrain* CTrain::FindNearestTrain(CVector posn, bool mustBeMainTrain) {
    CTrain* nearest     = nullptr;
    float   closestDist = 1e7f;

    auto* pool = GetVehiclePool();
    for (int32 i = (int32)pool->GetSize() - 1; i >= 0; --i) { // reverse-iterated in the original
        auto* veh = pool->GetAt(i);
        if (!veh || !veh->IsTrain()) { // original: m_nVehicleType (+0x590) == VEHICLE_TYPE_TRAIN
            continue;
        }
        auto* train = veh->AsTrain();

        const CVector& trainPos = train->GetPosition();
        const float dist = std::sqrt(sq(trainPos.x - posn.x) + sq(trainPos.y - posn.y));
        if (dist < closestDist && (!mustBeMainTrain || train->trainFlags.bIsFrontCarriage)) {
            nearest     = train;
            closestDist = dist;
        }
    }
    return nearest;
}

// 0x6F7140
void CTrain::SetNewTrainPosition(CTrain* train, CVector posn) {
    train->SetPosn(posn);
    train->FindPositionOnTrackFromCoors();
}

// 0x6F7260 (train-2)
bool CTrain::IsNextStationAllowed(CTrain* train) {
    const auto* engine = FindEngine(train); // walks the m_pPrevCarriage chain (+0x5D0) to the front

    float distanceToStation{};
    int32 stationIdx{};
    CTrain::FindNextStationPositionInDirection(engine->trainFlags.bClockwiseDirection, engine->m_fCurrentRailDistance, &distanceToStation, &stationIdx);

    const auto stationLevel = CTheZones::GetLevelFromPosition(CTrain::aStationCoors[stationIdx]);
    const auto statValue    = CStats::GetStatValue(STAT_CITY_UNLOCKED); // FUN_00558E40(0xB5)

    // Station is allowed as long as its level is unlocked (stat value + 1 >= level)
    return statValue + 1.0f >= (float)stationLevel;
}

// 0x6F72F0 (train-2)
void CTrain::SkipToNextAllowedStation(CTrain* train) {
    auto* engine = FindEngine(train); // walks the m_pPrevCarriage chain (+0x5D0) to the front

    float stationDist        = engine->m_fCurrentRailDistance;
    int32 stationIdx{};
    float distanceToStation{};
    do {
        CTrain::FindNextStationPositionInDirection(engine->trainFlags.bClockwiseDirection, stationDist, &distanceToStation, &stationIdx);
        stationDist = distanceToStation;
    } while (CStats::GetStatValue(STAT_CITY_UNLOCKED) + 1.0f < (float)CTheZones::GetLevelFromPosition(CTrain::aStationCoors[stationIdx])); // FUN_00572300

    engine->m_fCurrentRailDistance = stationDist;

    // Position the train a bit before the station and give it speed towards it
    if (!engine->trainFlags.bClockwiseDirection) {
        engine->m_fCurrentRailDistance = stationDist + StaticRef<float>(0x858BA4); // + 20.0f
        engine->m_fTrainSpeed = -0.1f; // 0xBDCCCCCD
    } else {
        engine->m_fCurrentRailDistance = stationDist - StaticRef<float>(0x858BA4);
        engine->m_fTrainSpeed = 0.1f;  // 0x3DCCCCCD
    }

    // Stream the station in
    CStreaming::LoadScene(CTrain::aStationCoors[stationIdx]); // FUN_0040EB70
    CStreaming::LoadAllRequestedModels(false);                // FUN_0040EA10

    // Pass the time it takes to drive the remaining distance to the station
    const CVector stationCoors = CTrain::aStationCoors[stationIdx];
    const CVector enginePos    = engine->GetPosition();
    const float   dist         = sqrtf(sq(stationCoors.x - enginePos.x) + sq(stationCoors.y - enginePos.y));
    CGameLogic::PassTime((uint32)(dist * StaticRef<float>(0x858C28) + StaticRef<float>(0x858F84))); // dist * 0.05f + 23.0f, ftol'ed in the original (FUN_00821B40)
}

// 0x6F7550 (train-2)
// Differences from the stash (per decompile): the final pass sets trainFlags.bIsStreakModel
// (byte 0x5B9 bit 2), NOT bStopsAtStations; SetStatus(STATUS_ABANDONED) instead of the raw
// static_cast<eEntityStatus>(0x20 >> 3) (original: byte +0x36 = (x & 7) | 0x20 -> status 4).
void CTrain::CreateMissionTrain(CVector posn, bool clockwiseDirection, uint32 trainType, CTrain** outFirstCarriage, CTrain** outLastCarriage, int32 nodeIndex, int32 trackId, bool isMissionTrain) {
    const uint32* models = &s_aTrainConfigs[trainType * 0x10];

    if (nodeIndex < 0) {
        nodeIndex = CTrain::FindClosestTrackNode(posn, &trackId);
    }

    // Start rail distance = distance-from-start of the node
    float railDistance = (float)pTrackNodes[trackId][nodeIndex].m_nDistanceFromStart * StaticRef<float>(0x859040); // * 1/3 (0.33333334f)

    CTrain* prev  = nullptr;
    CTrain* first = nullptr;
    CTrain* last  = nullptr;
    if (*models != 0) {
        const uint8 missionBit = (isMissionTrain & 1) << 5;     // byte 0x5B8 bit 5
        const uint8 clockBit   = (clockwiseDirection & 1) << 6; // byte 0x5B8 bit 6

        for (; *models != 0; ++models) {
            auto* carriage = new CTrain((int32)*models, PERMANENT_VEHICLE); // FUN_006F6030(model, 4)

            carriage->m_matrix->SetTranslate(CVector{ 0.0f, 0.0f, 0.0f }); // FUN_0059AF40(0, 0, 0)
            carriage->m_nNodeIndex = (int16)nodeIndex;
            carriage->SetStatus(STATUS_ABANDONED);    // byte +0x36 = (x & 7) | 0x20
            carriage->vehicleFlags.bIsLocked = true;  // byte +0x428 |= 8

            carriage->m_fCurrentRailDistance = railDistance;
            carriage->m_nTrackId = (int8)trackId;

            if (isMissionTrain) {
                carriage->m_nTrainFlags = (carriage->m_nTrainFlags & 0x1F) | missionBit | clockBit;
            } else {
                carriage->m_nTrainFlags = (carriage->m_nTrainFlags & 0x9F) | missionBit | clockBit;
            }

            if (prev) {
                // Offset this carriage behind the previous one along the track
                // NOTE: GetBoundingBox() is non-const (ColModel.h), so no const pointers here
                auto* prevColModel = CModelInfo::GetModelInfo(prev->m_nModelIndex)->GetColModel();
                auto* currColModel = CModelInfo::GetModelInfo(carriage->m_nModelIndex)->GetColModel();
                if (!clockwiseDirection) {
                    const float carriageLen = prevColModel->GetBoundingBox().m_vecMax.y - prevColModel->GetBoundingBox().m_vecMin.y;
                    carriage->m_fLength = carriageLen;
                    railDistance = carriageLen + railDistance;
                } else {
                    const float carriageLen = currColModel->GetBoundingBox().m_vecMax.y - currColModel->GetBoundingBox().m_vecMin.y;
                    carriage->m_fLength = -carriageLen;
                    railDistance = railDistance - carriageLen;
                }
            } else {
                // No predecessor - zero length (both mission & non-mission paths)
                carriage->m_fLength = 0.0f;
                if (!isMissionTrain) {
                    carriage->m_pNextCarriage = nullptr;
                } else {
                    // First carriage - place at the requested posn
                    carriage->GetPosition() = posn;
                    carriage->FindPositionOnTrackFromCoors(); // FUN_006F6CC0
                    carriage->m_pNextCarriage = nullptr;
                }
            }

            if (!prev) {
                carriage->m_nTrainFlags |= 0x8; // bIsFrontCarriage
                first = carriage;
            } else {
                carriage->m_nTrainFlags &= ~0x8;      // not front
                carriage->vehicleFlags.bIsBus = true; // byte +0x42A |= 2
                prev->m_nTrainFlags &= ~0x10;         // clear bIsLastCarriage on the predecessor
            }
            carriage->m_nTrainFlags |= 0x10; // bIsLastCarriage
            last = carriage;

            carriage->m_pPrevCarriage = prev;
            if (prev) {
                carriage->RegisterReference(reinterpret_cast<CEntity**>(&carriage->m_pPrevCarriage)); // FUN_00571B70
            }
            carriage->m_pNextCarriage = nullptr;
            if (prev) {
                prev->m_pNextCarriage = carriage;
                prev->RegisterReference(reinterpret_cast<CEntity**>(&prev->m_pNextCarriage));
            }

            carriage->ProcessControl(); // virtual vtbl+0x28 call in the original

            prev = carriage;
        }
    }

    // (Re-)add all created carriages to the world (reverse order)
    {
        CTrain* carriages[16]{};
        int32   numCarriages = 0;
        for (auto* carriage = first; carriage; carriage = carriage->m_pNextCarriage) {
            carriages[numCarriages++] = carriage;
        }
        for (auto i = numCarriages - 1; i >= 0; --i) {
            CWorld::Remove(carriages[i]); // FUN_00563280
            CWorld::Add(carriages[i]);    // FUN_00563220
        }
    }

    if (outFirstCarriage) {
        *outFirstCarriage = first;
    }
    if (outLastCarriage) {
        *outLastCarriage = last;
    }

    // Fire the "train spawned" stuff for the first carriage (unless it's the tram)
    if (first && first->m_nModelIndex != MODEL_TRAM) { // 0x1C1
        CPopulation::AddPedInCar(first, true, -1, 0, false, false); // FUN_00613A00(first, 1, -1, 0, 0, 0)
        if (first->m_pDriver) {
            first->m_pDriver->GetIntelligence()->SetPedDecisionMakerType(6); // FUN_00600B50(6)
        }
    }

    // If any carriage is a "streak" model, all carriages are marked as streak models
    // (original sets byte +0x5B9 bit 2 == trainFlags.bIsStreakModel, NOT bStopsAtStations)
    uint8 bIsStreak = 0;
    for (auto* carriage = first; carriage; carriage = carriage->m_pNextCarriage) {
        if (carriage->m_nModelIndex == MODEL_STREAK) { // 0x21A
            bIsStreak = 1;
            break;
        }
    }
    for (auto* carriage = first; carriage; carriage = carriage->m_pNextCarriage) {
        carriage->trainFlags.bIsStreakModel = bIsStreak != 0;
    }
}

// 0x6F7900 (train-2)
// Differences from the stash (all per decompile 0x6F7900):
//   1) NO return after the tunnel check - the original falls through to the track-0 spawn;
//   2) the walk direction is picked by a SINGLE rand roll (the same roll yields GenTrain_Direction),
//      the stash re-rolled on every walk step;
//   3) the node restored on d >= 220 is saved INSIDE the loop (the node before the last step);
//   4) model-loaded check == CStreaming::IsModelLoaded (original: m_LoadState byte == LOADSTATE_LOADED);
//   5) write at +0x3D0 == m_autoPilot.m_nCruiseSpeed (0x390 + 0x40);
//   6) player z check via FindPlayerCoors(-1).z (FUN_0056E010).
void CTrain::DoTrainGenerationAndRemoval() {
    const auto nowTimeMS = CTimer::GetTimeInMS(); // 0xB7CB84

    // Once per 3 seconds: check if the player is near any random train spawn point
    if (nowTimeMS / 3000 != StaticRef<uint32>(0xB7CB78) / 3000) {
        s_bPlayerNearSpawnPoint = false;
        for (const auto& stationCoors : CTrain::aStationCoors) {
            if (DistanceBetweenPoints2D(CVector2D{ FindPlayerCoors(-1) }, CVector2D{ stationCoors }) < StaticRef<float>(0x858B34)) { // 60.0f
                s_bPlayerNearSpawnPoint = true;
            }
        }
    }

    // Timing interval & removal radius depend on player proximity
    uint32 interval;
    float  removalRadius;
    if (!s_bPlayerNearSpawnPoint) {
        interval      = 950;
        removalRadius = 70.0f;
    } else {
        interval      = 1;
        removalRadius = 100.0f;
    }

    if (nowTimeMS / interval == StaticRef<uint32>(0xB7CB78) / interval) {
        return;
    }

    // Phase 0: remove trains that are far away from the player
    bool bRemovedAny = false;
    {
        auto* pool = GetVehiclePool();
        for (int32 i = (int32)pool->GetSize() - 1; i >= 0; --i) { // reverse-iterated in the original
            auto* veh = pool->GetAt(i);
            if (!veh || !veh->IsTrain()) {
                continue;
            }
            auto* train = veh->AsTrain();
            if (!train->trainFlags.bIsFrontCarriage || train->trainFlags.bMissionTrain) {
                continue;
            }

            bRemovedAny = true;

            // Walk the whole train: if any carriage is close to the player, keep it
            bool bKeep = true;
            for (auto* carriage = train; carriage; carriage = carriage->m_pNextCarriage) {
                const CVector playerPos = FindPlayerVehicle(-1, false)
                    ? FindPlayerVehicle(-1, false)->GetPosition()
                    : FindPlayerCoors(-1);
                const float dist = sqrtf(sq(playerPos.x - carriage->GetPosition().x) + sq(playerPos.y - carriage->GetPosition().y));
                if (carriage == FindPlayerVehicle(-1, false) || dist < StaticRef<float>(0x863994)) { // 220.0f
                    bKeep = false;
                }
            }

            if (bKeep) { // Original removes only if keep stayed true (all carriages far away)
                for (auto* carriage = train; carriage; ) {
                    auto* next = carriage->m_pNextCarriage;
                    CWorld::Remove(carriage);
                    delete carriage; // (**vtbl)(1) in the original
                    carriage = next;
                }
            }
        }
    }

    if (bDisableRandomTrains) {
        return;
    }

    // Phase 1: waiting for the chosen train's models to stream in
    if (GenTrain_Status != 0) {
        if (GenTrain_Status != 1) {
            return;
        }

        bool bAllLoaded = true;
        for (const uint32* m = &s_aTrainConfigs[GenTrain_TrainConfig * 0x10]; *m != 0; ++m) {
            if (!CStreaming::IsModelLoaded((int32)*m)) { // original: m_LoadState byte != LOADSTATE_LOADED
                CStreaming::RequestModel((int32)*m, STREAMING_KEEP_IN_MEMORY); // 8
                bAllLoaded = false;
            }
        }
        if (!bAllLoaded) {
            return;
        }

        // All models loaded - spawn the train if the chosen node is far enough from the player
        const CVector playerPos = FindPlayerVehicle(-1, false)
            ? FindPlayerVehicle(-1, false)->GetPosition()
            : FindPlayerCoors(-1);
        const auto& node = pTrackNodes[GenTrain_Track][GenTrain_GenerationNode];
        if (StaticRef<float>(0x858B34) < sqrtf(sq((float)node.x * 0.125f - playerPos.x) + sq((float)node.y * 0.125f - playerPos.y))) { // 60.0f
            CTrain* first = nullptr;
            CTrain::CreateMissionTrain(CVector{ 0.0f, 0.0f, 0.0f }, GenTrain_Direction != 0, GenTrain_TrainConfig, &first, nullptr, (int32)GenTrain_GenerationNode, (int32)GenTrain_Track, false);

            int32 maxCarriages;
            if (GenTrain_Track == 0) {
                maxCarriages = CGeneral::GetRandomNumber() % 30 + 15; // 15..44
                // If the spawn point is near the city centre - halve it
                // NOTE: original compares node.x against [0x8724F8] and node.y against [0x8724F4]
                const float centreCoorsX = StaticRef<float>(0x8724F8);
                const float centreCoorsY = StaticRef<float>(0x8724F4);
                if (sqrtf(sq((float)node.x * 0.125f - centreCoorsX) + sq((float)node.y * 0.125f - centreCoorsY)) < StaticRef<float>(0x858FD8)) { // 300.0f
                    maxCarriages /= 2;
                }
            } else {
                maxCarriages = CGeneral::GetRandomNumber() % 7 + 7; // 7..13
            }

            float maxSpeed{};
            first->FindMaximumSpeedToStopAtStations(&maxSpeed); // FUN_006F5BA0
            if ((float)maxCarriages < maxSpeed) {
                maxSpeed = (float)maxCarriages;
            }
            CTrain::SetTrainSpeed(first, maxSpeed);            // FUN_006F5E20
            first->m_autoPilot.m_nCruiseSpeed = (uint8)maxCarriages; // byte +0x3D0 (== m_autoPilot + 0x40)
        }

        // Make all models of the chosen train deletable again
        for (const uint32* m = &s_aTrainConfigs[GenTrain_TrainConfig * 0x10]; *m != 0; ++m) {
            CStreaming::SetModelIsDeletable((int32)*m);    // FUN_00409C10
            CStreaming::SetModelTxdIsDeletable((int32)*m); // FUN_00409C70
        }

        GenTrain_Status = 0;
        return;
    }

    // Phase 0.5: skip generation if we removed something this tick
    if (bRemovedAny) {
        return;
    }

    // Phase 2: pick a random spawn point on a random track
    const CVector playerPos = FindPlayerVehicle(-1, false)
        ? FindPlayerVehicle(-1, false)->GetPosition()
        : FindPlayerCoors(-1);

    GenTrain_Track          = 0;
    GenTrain_GenerationNode = CGeneral::GetRandomNumber() % (uint32)NumTrackNodes[0];

    const auto& node = pTrackNodes[0][GenTrain_GenerationNode];
    float dist = sqrtf(sq((float)node.x * 0.125f - playerPos.x) + sq((float)node.y * 0.125f - playerPos.y));

    if (dist >= removalRadius) {
        goto track1;
    }

    // Node is close to the player - if the player is high above it, spawn in the SF tunnel
    // instead (track 1). NOTE: otherwise the original FALLS THROUGH to the track-0 spawn below.
    if (node.GetZ() + StaticRef<float>(0x858B44) <= FindPlayerCoors(-1).z) { // + 6.0f
        const auto playerTunnelAttrs = CCullZones::FindTunnelAttributesForCoors(FindPlayerCoors(-1)); // FUN_0072D9F0
        if ((playerTunnelAttrs & 0x880) == 0) {
            const auto nodeTunnelAttrs = CCullZones::FindTunnelAttributesForCoors(CVector{ node.GetX(), node.GetY(), node.GetZ() });
            if ((nodeTunnelAttrs & 0x800) != 0) {
                goto track1;
            }
        }
    }

    // Direction picking & spawn (track 0)
    {
        const auto rnd = CGeneral::GetRandomNumber();
        GenTrain_Direction = rnd & 1;

        // Move the node along the track (in the rolled direction) until it's at least ~170 units away
        uint32 previousGenerationNode{};
        if (dist < StaticRef<float>(0x858F98)) { // 170.0f
            for (;;) {
                previousGenerationNode = GenTrain_GenerationNode;
                if ((rnd & 1) == 0) {
                    GenTrain_GenerationNode = (GenTrain_GenerationNode + 1) % (uint32)NumTrackNodes[GenTrain_Track];
                } else {
                    GenTrain_GenerationNode = GenTrain_GenerationNode - 1;
                    if ((int32)GenTrain_GenerationNode < 0) {
                        GenTrain_GenerationNode += (uint32)NumTrackNodes[GenTrain_Track];
                    }
                }
                const auto& n = pTrackNodes[GenTrain_Track][GenTrain_GenerationNode];
                dist = sqrtf(sq((float)n.x * 0.125f - playerPos.x) + sq((float)n.y * 0.125f - playerPos.y));
                if (dist >= StaticRef<float>(0x858F98)) {
                    break;
                }
            }
        }

        // Round-robin the train config (signed % 8 idiom)
        uint32 cfg = (s_GenTrainRoundRobin + 1) & 0x80000007;
        if ((int32)cfg < 0) {
            cfg = ((cfg - 1) | 0xFFFFFFF8) + 1;
        }
        GenTrain_TrainConfig = cfg;
        s_GenTrainRoundRobin = cfg;

        GenTrain_Status = 1;

        if (StaticRef<float>(0x863994) < dist) { // 220.0f - walked too far, restore & retry next time
            GenTrain_GenerationNode = previousGenerationNode;
            return;
        }
    }
    return;

track1:
    // Try track 1 (tunnel track), only in region 2 (SF)
    if (GenTrain_Status == 0 && CWeather::WeatherRegion == WEATHER_REGION_SF) { // DAT_00C81314 == 2
        GenTrain_Track          = 1;
        GenTrain_GenerationNode = CGeneral::GetRandomNumber() % (uint32)NumTrackNodes[1];

        const auto& n = pTrackNodes[1][GenTrain_GenerationNode];
        float d = sqrtf(sq((float)n.x * 0.125f - playerPos.x) + sq((float)n.y * 0.125f - playerPos.y));

        if (d < removalRadius) {
            const auto rnd = CGeneral::GetRandomNumber();
            GenTrain_Direction = rnd & 1;

            if (d < StaticRef<float>(0x858F98)) { // 170.0f
                for (;;) {
                    if ((rnd & 1) == 0) {
                        GenTrain_GenerationNode = (GenTrain_GenerationNode + 1) % (uint32)NumTrackNodes[GenTrain_Track];
                    } else {
                        GenTrain_GenerationNode = GenTrain_GenerationNode - 1;
                        if ((int32)GenTrain_GenerationNode < 0) {
                            GenTrain_GenerationNode += (uint32)NumTrackNodes[GenTrain_Track];
                        }
                    }
                    const auto& n2 = pTrackNodes[GenTrain_Track][GenTrain_GenerationNode];
                    d = sqrtf(sq((float)n2.x * 0.125f - playerPos.x) + sq((float)n2.y * 0.125f - playerPos.y));
                    if (d >= StaticRef<float>(0x858F98)) {
                        break;
                    }
                }
            }

            if (d < StaticRef<float>(0x863994)) { // 220.0f
                GenTrain_Status = 1;
                uint32 cfg = CGeneral::GetRandomNumber() & 0x80000001;
                if ((int32)cfg < 0) {
                    cfg = ((cfg - 1) | 0xFFFFFFFE) + 1;
                }
                GenTrain_TrainConfig = cfg + 8; // configs 8..15 for track 1
            }
        }
    }
}

// 0x6F8170 (train-2)
// Differences from the stash (all per decompile 0x6F8170):
//   1) intelligence+0x10 is CTaskManager::m_aPrimaryTasks[TASK_PRIMARY_PRIMARY] (slot 3);
//      the check is GetTaskType() == 700/701 and CTaskComplexEnterCar::m_Car (+0xC) == this;
//   2) bit 18 at +0x478 == CPed::bJustGotOffTrain (stash read it as a raw offset);
//   3) byte +0x484 == 1 -> m_nCreatedBy == PED_GAME (stash: m_nPedState == PEDSTATE_IDLE);
//   4) +0x530 (0x36/0x37/0x38) == m_nPedState == PEDSTATE_DIE/_DEAD/_DIE_BY_STEALTH;
//   5) +0x480 == m_pPlayerData (players skipped) - accessed via GetPlayerData();
//   6) sideOffset == -Dot(trainPos, trainRight).
// Integration note: `AsComplex()` yields CTaskComplex* which has no GetTargetCar(), so the
// task is cast to CTaskComplexEnterCar (same fix as in 0x6F6850).
void CTrain::AddNearbyPedAsRandomPassenger() {
    const auto* playerPed = FindPlayerPed(-1);
    if (playerPed->m_pVehicle == this) {
        return;
    }

    // Don't pick a passenger if the player is already entering this train
    if (const auto* task = playerPed->GetIntelligence()->m_TaskMgr.m_aPrimaryTasks[TASK_PRIMARY_PRIMARY]) {
        const auto taskType = task->GetTaskType();
        if ((taskType == TASK_COMPLEX_ENTER_CAR_AS_PASSENGER || taskType == TASK_COMPLEX_ENTER_CAR_AS_DRIVER)
            && static_cast<const CTaskComplexEnterCar*>(task)->GetTargetCar() == this
        ) {
            return;
        }
    }

    const bool bClockwise = trainFlags.bClockwiseDirection; // byte 0x5B8 bit 6

    if (m_nNumPassengersToLeave == m_nNumPassengersToEnter) {
        return;
    }

    // If there is already a temporary passenger - remove it
    if (m_pTemporaryPassenger) {
        const auto* tmp = m_pTemporaryPassenger;
        if (tmp->bJustGotOffTrain == tmp->bInVehicle) { // +0x478 bit 18 vs +0x46C bit 8 - either both or neither
            return;
        }
        if (tmp->GetIntelligence()->FindTaskByType(TASK_COMPLEX_LEAVE_CAR_AND_WANDER)) { // FUN_00600EE0(0x2C3)
            return;
        }
        m_pTemporaryPassenger->CleanUpOldReference(reinterpret_cast<CEntity**>(&m_pTemporaryPassenger)); // FUN_00571A00
        if (!m_pTemporaryPassenger->bJustGotOffTrain
            && m_pTemporaryPassenger->bInVehicle
            && m_pTemporaryPassenger->m_pVehicle == this
        ) {
            CPopulation::RemovePed(m_pTemporaryPassenger); // FUN_00610F20
        }
        m_pTemporaryPassenger = nullptr;
    }

    // Find the closest suitable ped to the train
    CMatrix& trainMat = *m_matrix; // non-const: passed to CMatrix::Invert(in, out) below
    const CVector  trainPos = GetPosition();

    float closestDist = 999999.0f;
    CPed* closestPed  = nullptr;
    for (auto& ped : GetPedPool()->GetAllValid()) {
        if (!ped.IsCreatedBy(PED_GAME)) { // byte +0x484 == 1
            continue;
        }
        if (ped.GetPlayerData() != nullptr) { // +0x480 - skip players
            continue;
        }
        if (ped.GetGroup() != nullptr) { // FUN_005F7E80
            continue;
        }
        if (ped.m_nPedType == PED_TYPE_COP) { // +0x598 == 6
            continue;
        }
        if (ped.bInVehicle || ped.bJustGotOffTrain) { // +0x46C bit 8 / +0x478 bit 18
            continue;
        }
        switch (ped.m_nPedState) { // +0x530 - skip dying/dead peds (0x36/0x37/0x38)
        case PEDSTATE_DIE:
        case PEDSTATE_DEAD:
        case PEDSTATE_DIE_BY_STEALTH:
            continue;
        }
        if (ped.GetIntelligence()->FindTaskByType(TASK_COMPLEX_ENTER_CAR_AS_PASSENGER)) { // FUN_00600EE0(700)
            continue;
        }

        const CVector pedToTrain = trainPos - ped.GetPosition(); // FUN_0040FE60
        const float   dist       = pedToTrain.Magnitude();       // FUN_00406DA0
        if (dist > StaticRef<float>(0x872368)) { // 625.0f
            continue;
        }

        // Side-of-train check: the original dots the ped pos against the train matrix' first row
        // (FUN_0040FDB0 == DotProduct(CVector*, CVector*) with the matrix pointer standing in for the
        // right vector), offset by -Dot(m_pos, m_right)
        const float sideOffset = -DotProduct(trainMat.GetPosition(), trainMat.GetRight());
        const float sideDot    = DotProduct(ped.GetPosition(), trainMat.GetRight());
        if (bClockwise) {
            if (sideDot + sideOffset >= 0.0f && dist < closestDist) {
                closestDist = dist;
                closestPed  = &ped;
            }
        } else {
            if (sideDot + sideOffset <= 0.0f && dist < closestDist) {
                closestDist = dist;
                closestPed  = &ped;
            }
        }
    }

    if (!closestPed) {
        return;
    }

    // Make the ped enter the train as a passenger
    CVector doorPos{};
    int32   doorId{};
    CCarEnterExit::GetNearestCarDoor(closestPed, this, doorPos, doorId); // FUN_006528F0

    // Target position: the door position in train-local space (FUN_0059B920 == CMatrix::Invert(in, out),
    // FUN_0059C890 == matrix * vector - repo equivalent: CMatrix::TransformPoint)
    CMatrix invertedTrainMatrix{};
    Invert(trainMat, invertedTrainMatrix);
    const CVector localDoorPos = invertedTrainMatrix.TransformPoint(doorPos);

    const auto seekTask = new CTaskComplexSeekEntity<CEntitySeekPosCalculatorXYOffset>( // FUN_00661DC0
        this,
        50'000,
        1'000,
        StaticRef<float>(0x86FC2C),
        StaticRef<float>(0x86FC28),
        StaticRef<float>(0x86FC30),
        true,
        true
    );
    seekTask->GetSeekPosCalculator().SetOffset(localDoorPos); // task + 0x44..0x4C
    *reinterpret_cast<int32*>(reinterpret_cast<uint8*>(seekTask) + 0x50) = 7; // unknown task member (as in the original)

    const auto seq = new CTaskComplexSequence(); // FUN_00632BD0
    seq->AddTask(seekTask);                      // FUN_00632D10

    const auto enterTask = bClockwise
        ? new CTaskComplexEnterCarAsPassenger(this, 0, false)          // FUN_00640340(this, 0, 0)
        : static_cast<CTask*>(new CTaskComplexEnterCarAsDriver(this)); // FUN_006402F0(this)
    seq->AddTask(enterTask);

    closestPed->GetTaskManager().SetTask(seq, TASK_PRIMARY_PRIMARY); // FUN_00681AF0(seq, 3, 0)

    // Increment the passengers-to-leave counter (low nibble of byte + 0x5CB)
    m_nNumPassengersToLeave++;

    m_pTemporaryPassenger = closestPed;
    m_pTemporaryPassenger->RegisterReference(reinterpret_cast<CEntity**>(&m_pTemporaryPassenger)); // FUN_00571B70
}

// 0x6F86A0
void CTrain::ProcessControl() {
    vehicleFlags.bWarnedPeds = 0;
    m_vehicleAudio.Service();
    if (gbModelViewer) {
        return;
    }

    CVector vecOldTrainPosition = GetPosition();
    float fOldTrainHeading = GetHeading();

    const float& fTotalTrackLength = arrTotalTrackLength[m_nTrackId];
    CTrainNode* trainNodes = pTrackNodes[m_nTrackId];
    auto numTrackNodes = NumTrackNodes[m_nTrackId];

    if (trainFlags.bNotOnARailRoad == 0) {
        if (!trainFlags.bIsFrontCarriage) {
            if (m_pPrevCarriage) {
                m_fTrainSpeed = m_pPrevCarriage->m_fTrainSpeed;
                m_fCurrentRailDistance = m_pPrevCarriage->m_fCurrentRailDistance + m_fLength;
            } else {
                m_fTrainSpeed *= std::pow(0.9900000095367432f, CTimer::GetTimeStep());
                m_fCurrentRailDistance += m_fTrainSpeed * CTimer::GetTimeStep();
            }

            if (trainFlags.b01 && trainFlags.bStoppedAtStation && m_nModelIndex == MODEL_STREAKC && !trainFlags.bMissionTrain) {
                CPlayerPed* localPlayer = FindPlayerPed();
                if (m_nPassengersGenerationState == TRAIN_PASSENGERS_QUERY_NUM_PASSENGERS_TO_LEAVE) {
                    if (localPlayer->m_pVehicle == this) {
                        m_nNumPassengersToLeave = 0;
                    } else {
                        m_nNumPassengersToLeave = (CGeneral::GetRandomNumber() & 3) + 1; // [1, 4]
                    }
                    m_nPassengersGenerationState = TRAIN_PASSENGERS_TELL_PASSENGERS_TO_LEAVE;
                }

                if (m_nPassengersGenerationState == TRAIN_PASSENGERS_TELL_PASSENGERS_TO_LEAVE) {
                    RemoveRandomPassenger();
                    if (m_nNumPassengersToLeave == 0) {
                        m_nPassengersGenerationState = TRAIN_PASSENGERS_QUERY_NUM_PASSENGERS_TO_ENTER;
                    }
                }

                if (m_nPassengersGenerationState == TRAIN_PASSENGERS_QUERY_NUM_PASSENGERS_TO_ENTER) {
                    if (localPlayer->m_pVehicle == this) {
                        m_nNumPassengersToEnter = 0;
                    } else {
                        m_nNumPassengersToEnter = CGeneral::GetRandomNumber() % 4 + 1; // rand(1, 4)
                    }
                    m_nPassengersGenerationState = TRAIN_PASSENGERS_TELL_PASSENGERS_TO_ENTER;
                }

                if (m_nPassengersGenerationState == TRAIN_PASSENGERS_TELL_PASSENGERS_TO_ENTER) {
                    if (trainFlags.bPassengersCanEnterAndLeave) {
                        AddNearbyPedAsRandomPassenger();
                        if (m_nNumPassengersToLeave == m_nNumPassengersToEnter) {
                            m_nPassengersGenerationState = TRAIN_PASSENGERS_GENERATION_FINISHED;
                        }
                    }
                }
            }
        } else {
            CPad* pad = CPad::GetPad();
            if (m_pDriver && m_pDriver->IsPlayer()) {
                pad = m_pDriver->AsPlayer()->GetPadFromPlayer();
            }

            uint32 numCarriagesPulled = FindNumCarriagesPulled();
            if (!trainFlags.bClockwiseDirection) {
                m_fTrainSpeed = -m_fTrainSpeed;
            }

            if (GetStatus()) {
                bool bIsStreakModel = trainFlags.bIsStreakModel;
                auto fStopAtStationSpeed = static_cast<float>(m_autoPilot.m_nCruiseSpeed);

                uint32 timeAtStation = CTimer::GetTimeInMS() - m_nTimeWhenStoppedAtStation;
                if (timeAtStation >= (bIsStreakModel ? 20'000u : 10'000u)) {
                    if (timeAtStation >= (bIsStreakModel ? 28'000u : 18'000u)) {
                        if (timeAtStation >= (bIsStreakModel ? 32'000u : 22'000u)) {
                            if (trainFlags.bStopsAtStations) {
                                float maxTrainSpeed = 0.0f;
                                if (FindMaximumSpeedToStopAtStations(&maxTrainSpeed)) {
                                    fStopAtStationSpeed = 0.0f;
                                    m_nTimeWhenStoppedAtStation = CTimer::GetTimeInMS();
                                } else {
                                    if (fStopAtStationSpeed >= maxTrainSpeed) {
                                        fStopAtStationSpeed = maxTrainSpeed;
                                    }
                                }
                            }
                        } else if (trainFlags.bStoppedAtStation) {
                            CTrain* trainCarriage = this;
                            do {
                                trainFlags.bStoppedAtStation = false;
                                trainCarriage->m_nPassengersGenerationState = TRAIN_PASSENGERS_GENERATION_FINISHED;
                                trainCarriage = trainCarriage->m_pNextCarriage;
                            } while (trainCarriage);
                        }
                    } else {
                        fStopAtStationSpeed = 0.0f;
                        if (trainFlags.bStoppedAtStation) {
                            CTrain* trainCarriage = this;
                            do {
                                trainFlags.bPassengersCanEnterAndLeave = false;
                                trainCarriage->m_nPassengersGenerationState = TRAIN_PASSENGERS_GENERATION_FINISHED;
                                trainCarriage = trainCarriage->m_pNextCarriage;
                            } while (trainCarriage);
                        }
                    }
                } else {
                    fStopAtStationSpeed = 0.0f;
                    if (!trainFlags.bStoppedAtStation) {
                        CTrain* trainCarriage = this;
                        do {
                            trainFlags.bStoppedAtStation = true;
                            trainFlags.bPassengersCanEnterAndLeave = true;
                            trainCarriage->m_nPassengersGenerationState = TRAIN_PASSENGERS_QUERY_NUM_PASSENGERS_TO_LEAVE;
                            trainCarriage = trainCarriage->m_pNextCarriage;
                        } while (trainCarriage);
                    }
                }

                fStopAtStationSpeed = fStopAtStationSpeed / 50.0f - m_fTrainSpeed;
                if (fStopAtStationSpeed > 0.0f) {
                    m_fTrainGas = fStopAtStationSpeed * 30.0f;
                    if (m_fTrainGas >= 1.0f) {
                        m_fTrainGas = 1.0f;
                    }

                    m_fTrainGas *= 255.0f;
                    m_fTrainBrake = 0.0f;
                } else {
                    float fTrainSpeed = fStopAtStationSpeed * -30.0f;
                    m_fTrainGas = 0.0f;
                    if (fTrainSpeed >= 1.0f) {
                        fTrainSpeed = 1.0f;
                    }
                    m_fTrainBrake = fTrainSpeed * 255.0f;
                }
            } else {
                float fTrainSpeed = m_fTrainSpeed;
                if (fTrainSpeed < 0.0f) {
                    fTrainSpeed = -fTrainSpeed;
                }

                if (fTrainSpeed < 0.001f) {
                    m_fTrainBrake = 0.0f;
                    m_fTrainGas = static_cast<float>(pad->GetAccelerate() - pad->GetBrake());
                } else {
                    if (m_fTrainSpeed > 0.0f) {
                        m_fTrainBrake = static_cast<float>(pad->GetBrake());
                        m_fTrainGas = static_cast<float>(pad->GetAccelerate());
                    } else {
                        m_fTrainGas = static_cast<float>(-pad->GetBrake());
                        m_fTrainBrake = static_cast<float>(pad->GetAccelerate());
                    }
                }
            }

            if (trainFlags.bForceSlowDown) {
                const CVector& vecPoint = GetPosition();
                CVector vecDistance{};
                if (CGameLogic::CalcDistanceToForbiddenTrainCrossing(vecPoint, m_vecMoveSpeed, true, vecDistance) < 230.0f) {
                    if (DotProduct(GetForwardVector(), vecDistance) <= 0.0f) {
                        m_fTrainGas = std::max(0.0f, m_fTrainGas);
                    } else {
                        m_fTrainGas = std::min(0.0f, m_fTrainGas);
                    }

                    if (CGameLogic::CalcDistanceToForbiddenTrainCrossing(vecPoint, m_vecMoveSpeed, false, vecDistance) < 230.0f) {
                        m_fTrainBrake = 512.0f;
                    }
                }
            }

            numCarriagesPulled += 3;

            m_fTrainSpeed += m_fTrainGas / 256.0f * CTimer::GetTimeStep() * 0.002f / float(numCarriagesPulled);

            if (m_fTrainBrake != 0.0f) {
                float fTrainSpeed = m_fTrainSpeed;
                if (m_fTrainSpeed < 0.0f) {
                    fTrainSpeed = -fTrainSpeed;
                }
                float fBreak = m_fTrainBrake / 256.0f * CTimer::GetTimeStep() * 0.006f / float(numCarriagesPulled);
                if (fTrainSpeed >= fBreak) {
                    if (m_fTrainSpeed < 0.0f) {
                        m_fTrainSpeed += fBreak;
                    } else {
                        m_fTrainSpeed -= fBreak;
                    }
                } else {
                    m_fTrainSpeed = 0.0f;
                }
            }

            m_fTrainSpeed *= pow(0.999750018119812f, CTimer::GetTimeStep());
            if (!trainFlags.bClockwiseDirection) {
                m_fTrainSpeed = -m_fTrainSpeed;
            }

            m_fCurrentRailDistance += CTimer::GetTimeStep() * m_fTrainSpeed;

            if (GetStatus() == STATUS_PLAYER) {

                float fTheTrainSpeed = m_fTrainSpeed;
                if (fTheTrainSpeed < 0.0f) {
                    fTheTrainSpeed = -fTheTrainSpeed;
                }
                if (fTheTrainSpeed > 1.0f * 0.95f) {
                    CPad::GetPad()->StartShake(300, 70, 0);
                    TheCamera.CamShake(0.1f, GetPosition());
                }

                fTheTrainSpeed = m_fTrainSpeed;
                if (fTheTrainSpeed < 0.0f) {
                    fTheTrainSpeed = -fTheTrainSpeed;
                }

                if (fTheTrainSpeed > 1.0f) {
                    int32 nNodeIndex = m_nNodeIndex;
                    int32 previousNodeIndex = nNodeIndex - 1;
                    if (previousNodeIndex < 0) {
                        previousNodeIndex = numTrackNodes;
                    }

                    int32 previousNodeIndex2 = previousNodeIndex - 1;
                    if (previousNodeIndex2 < 0) {
                        previousNodeIndex2 = numTrackNodes;
                    }

                    CTrainNode* pCurrentTrainNode = &trainNodes[m_nNodeIndex];
                    CTrainNode* pPreviousTrainNode = &trainNodes[previousNodeIndex];
                    CTrainNode* pPreviousTrainNode2 = &trainNodes[previousNodeIndex2];

                    CVector vecDifference1 = pCurrentTrainNode->GetPosn() - pPreviousTrainNode->GetPosn();
                    CVector vecDifference2 = pPreviousTrainNode->GetPosn() - pPreviousTrainNode2->GetPosn();
                    vecDifference1.Normalise();
                    vecDifference2.Normalise();

                    if (DotProduct(vecDifference1, vecDifference2) < 0.996f) {
                        CTrain* carriage = this;
                        bool bIsInTunnel = false;
                        while (!bIsInTunnel) {
                            bIsInTunnel = carriage->IsInTunnel();
                            carriage = carriage->m_pNextCarriage;
                            if (!carriage) {
                                if (!bIsInTunnel) {
                                    CTrain* theTrainCarriage = this;
                                    do {
                                        trainFlags.bNotOnARailRoad = true;
                                        theTrainCarriage->physicalFlags.bDisableCollisionForce = false;
                                        theTrainCarriage->physicalFlags.bDisableSimpleCollision = false;
                                        theTrainCarriage->SetIsStatic(false);
                                        theTrainCarriage = theTrainCarriage->m_pNextCarriage;
                                    } while (theTrainCarriage);

                                    CPhysical::ProcessControl();
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (m_fCurrentRailDistance < 0.0f) {
            do {
                m_fCurrentRailDistance += fTotalTrackLength;
            } while (m_fCurrentRailDistance < 0.0f);
        }

        if (m_fCurrentRailDistance >= fTotalTrackLength) {
            do {
                m_fCurrentRailDistance -= fTotalTrackLength;
            } while (m_fCurrentRailDistance >= fTotalTrackLength);
        }

        float fNextNodeTrackLength = 0.0f;
        int32 nextNodeIndex = m_nNodeIndex + 1;
        if (nextNodeIndex < numTrackNodes) {
            CTrainNode* nextTrainNode = &trainNodes[nextNodeIndex];
            fNextNodeTrackLength = nextTrainNode->GetDistanceFromStart();
        } else {
            fNextNodeTrackLength = fTotalTrackLength;
            nextNodeIndex = 0;
        }

        CTrainNode* theTrainNode = &trainNodes[m_nNodeIndex];
        float fCurrentNodeTrackLength = theTrainNode->GetDistanceFromStart();
        while (m_fCurrentRailDistance < fCurrentNodeTrackLength || fNextNodeTrackLength < m_fCurrentRailDistance) {
            int32 newNodeIndex = m_nNodeIndex - 1; // previous node
            if (fCurrentNodeTrackLength <= m_fCurrentRailDistance) {
                newNodeIndex = m_nNodeIndex + 1; // next node
            }
            m_nNodeIndex = newNodeIndex % numTrackNodes;
            m_vehicleAudio.AddAudioEvent(AE_TRAIN_CLACK, 0.0f);

            theTrainNode = &trainNodes[m_nNodeIndex];
            fCurrentNodeTrackLength = theTrainNode->GetDistanceFromStart();

            nextNodeIndex = m_nNodeIndex + 1;
            if (nextNodeIndex < numTrackNodes) {
                CTrainNode* nextTrainNode = &trainNodes[nextNodeIndex];
                fNextNodeTrackLength = nextTrainNode->GetDistanceFromStart();
            } else {
                fNextNodeTrackLength = fTotalTrackLength;
                nextNodeIndex = 0;
            }
        }

        CTrainNode* nextTrainNode = &trainNodes[nextNodeIndex];
        fNextNodeTrackLength = nextTrainNode->GetDistanceFromStart();

        float fTrackNodeDifference = fNextNodeTrackLength - fCurrentNodeTrackLength;
        if (fTrackNodeDifference < 0.0f) {
            fTrackNodeDifference += fTotalTrackLength;
        }

        float fTheDistance = (m_fCurrentRailDistance - fCurrentNodeTrackLength) / fTrackNodeDifference;
        CVector vecPosition1 = theTrainNode->GetPosn() * (1.0f - fTheDistance) + nextTrainNode->GetPosn() * fTheDistance;

        CColModel* vehicleColModel = CModelInfo::GetModelInfo(m_nModelIndex)->GetColModel();
        const CBoundingBox& bbox = vehicleColModel->GetBoundingBox();
        float fTotalCurrentRailDistance = bbox.GetLength() + m_fCurrentRailDistance;
        if (fTotalCurrentRailDistance > fTotalTrackLength) {
            fTotalCurrentRailDistance -= fTotalTrackLength;
        }

        nextNodeIndex = m_nNodeIndex + 1;
        if (nextNodeIndex < numTrackNodes) {
            fNextNodeTrackLength = trainNodes[nextNodeIndex].GetDistanceFromStart();
        } else {
            fNextNodeTrackLength = fTotalTrackLength;
            nextNodeIndex = 0;
        }

        int32 trainNodeIndex = m_nNodeIndex;
        while (fTotalCurrentRailDistance < fCurrentNodeTrackLength || fTotalCurrentRailDistance > fNextNodeTrackLength) {
            trainNodeIndex = (trainNodeIndex + 1) % numTrackNodes;

            theTrainNode = &trainNodes[trainNodeIndex];
            fCurrentNodeTrackLength = theTrainNode->GetDistanceFromStart();

            nextNodeIndex = trainNodeIndex + 1;
            if (nextNodeIndex < numTrackNodes) {
                fNextNodeTrackLength = trainNodes[nextNodeIndex].GetDistanceFromStart();
            } else {
                fNextNodeTrackLength = fTotalTrackLength;
                nextNodeIndex = 0;
            }
        }

        fNextNodeTrackLength = trainNodes[nextNodeIndex].GetDistanceFromStart();

        fTrackNodeDifference = fNextNodeTrackLength - fCurrentNodeTrackLength;
        if (fTrackNodeDifference < 0.0f) {
            fTrackNodeDifference += fTotalTrackLength;
        }

        fTheDistance = (fTotalCurrentRailDistance - fCurrentNodeTrackLength) / fTrackNodeDifference;
        CVector vecPosition2 = theTrainNode->GetPosn() * (1.0f - fTheDistance) + trainNodes[nextNodeIndex].GetPosn() * fTheDistance;

        {
            CVector& vecVehiclePosition = GetPosition();
            vecVehiclePosition = (vecPosition1 + vecPosition2) / 2.0f;
            vecVehiclePosition.z += m_pHandlingData->m_fSuspensionLowerLimit - bbox.m_vecMin.z;
        }

        GetForward() = vecPosition2 - vecPosition1;
        GetForward().Normalise();
        if (!trainFlags.bClockwiseDirection) {
            GetForward() *= -1.0f;
        }

        CVector vecTemp(0.0f, 0.0f, 1.0f);
        CrossProduct(&GetRight(), &GetForward(), &vecTemp);
        GetRight().Normalise();
        CrossProduct(&GetUp(), &GetRight(), &GetForward());

        auto fTrainNodeLighting     = theTrainNode->GetLightingFromCollision().GetCurrentLighting();
        auto fTrainNextNodeLighting = trainNodes[nextNodeIndex].GetLightingFromCollision().GetCurrentLighting();

        fTrainNodeLighting += (fTrainNextNodeLighting - fTrainNodeLighting) * fTheDistance;
        m_fContactSurfaceBrightness = fTrainNodeLighting;
        m_vecMoveSpeed = (1.0f / CTimer::GetTimeStep()) * (GetPosition() - vecOldTrainPosition);

        float fNewTrainHeading = GetHeading();
        float fHeading = fNewTrainHeading - fOldTrainHeading;
        if (fHeading <= PI) {
            if (fHeading < -PI) {
                fHeading += TWO_PI;
            }
        } else {
            fHeading -= TWO_PI;
        }

        m_vecTurnSpeed = CVector(0.0f, 0.0f, fHeading / CTimer::GetTimeStep());

        if (trainFlags.bNotOnARailRoad) {
            m_vecMoveSpeed *= -1.0f;
            m_vecTurnSpeed *= -1.0f;

            ApplyMoveSpeed();

            m_vecMoveSpeed *= -1.0f;
            m_vecTurnSpeed *= -1.0f;

            CPhysical::ProcessControl();
        } else {
            m_vecMoveSpeed.x = std::clamp(m_vecMoveSpeed.x, -2.0f, 2.0f);
            m_vecMoveSpeed.y = std::clamp(m_vecMoveSpeed.y, -2.0f, 2.0f);
            m_vecMoveSpeed.z = std::clamp(m_vecMoveSpeed.z, -2.0f, 2.0f);

            m_vecTurnSpeed.x = std::clamp(m_vecTurnSpeed.x, -0.1f, 0.1f);
            m_vecTurnSpeed.y = std::clamp(m_vecTurnSpeed.y, -0.1f, 0.1f);
            m_vecTurnSpeed.z = std::clamp(m_vecTurnSpeed.z, -0.1f, 0.1f);
        }

        UpdateRwMatrix();
        UpdateRwFrame();
        RemoveAndAdd();

        SetIsStuck(false);
        SetWasPostponed(false);
        SetIsInSafePosition(true);

        m_fMovingSpeed = DistanceBetweenPoints(GetPosition(), vecOldTrainPosition);

        if (trainFlags.bIsFrontCarriage || trainFlags.bIsLastCarriage) {
            CVector vecPoint = bbox.m_vecMax.y * GetForward();
            vecPoint += GetPosition();
            vecPoint += CTimer::GetTimeStep() * m_vecMoveSpeed;

            MarkSurroundingEntitiesForCollisionWithTrain(vecPoint, 3.0f, this, false);
        }

        if (!vehicleFlags.bWarnedPeds) {
            CCarCtrl::ScanForPedDanger(this);
        }
        return;
    } else {
        if (!GetIsStuck()) {
            float fMaxForce = 0.003f;
            float fMaxTorque = 0.0009f;
            float fMaxMovingSpeed = 0.005f;

            if (GetStatus() != STATUS_PLAYER) {
                fMaxForce = 0.006f;
                fMaxTorque = 0.0015f;
                fMaxMovingSpeed = 0.015f;
            }

            float fMaxForceTimeStep  = (fMaxForce  * CTimer::GetTimeStep()) * (fMaxForce * CTimer::GetTimeStep());
            float fMaxTorqueTimeStep = (fMaxTorque * CTimer::GetTimeStep()) * (fMaxTorque * CTimer::GetTimeStep());

            m_vecForce  = (m_vecForce  + m_vecMoveSpeed) / 2.0f;
            m_vecTorque = (m_vecTorque + m_vecTurnSpeed) / 2.0f;

            if (m_vecForce.SquaredMagnitude() > fMaxForceTimeStep ||
                m_vecTorque.SquaredMagnitude() > fMaxTorqueTimeStep ||
                m_fMovingSpeed >= fMaxMovingSpeed ||
                m_fDamageIntensity > 0.0f && m_pDamageEntity != nullptr && m_pDamageEntity->GetIsTypePed()
            ) {
                m_nFakePhysics = 0;
            } else {
                m_nFakePhysics += 1;
                if (m_nFakePhysics > 10 /*&& !plugin::Call<0x424100>()*/) {
                    // if (m_nFakePhysics > 10) { // OG redundant check
                        m_nFakePhysics = 10;
                    // }

                    ResetMoveSpeed();
                    ResetTurnSpeed();
                    SkipPhysics();
                    return;
                }
            }
        }

        CPhysical::ProcessControl();

        CVector vecMoveForce{}, vecTurnForce{};
        if (mod_Buoyancy.ProcessBuoyancy(this, m_fBuoyancyConstant, &vecMoveForce, &vecTurnForce)) {
            physicalFlags.bTouchingWater = true;

            float fTimeStep = 0.01f;
            if (CTimer::GetTimeStep() >= 0.01f) {
                fTimeStep = CTimer::GetTimeStep();
            }

            float fSpeedFactor = 1.0f - vecOldTrainPosition.z / (fTimeStep * m_fMass * 0.008f) * 0.05f;
            fSpeedFactor = std::pow(fSpeedFactor, CTimer::GetTimeStep());

            m_vecMoveSpeed *= fSpeedFactor;
            m_vecTurnSpeed *= fSpeedFactor;
            ApplyMoveForce(vecOldTrainPosition.x, vecOldTrainPosition.y, vecOldTrainPosition.z);
            ApplyTurnForce(vecTurnForce, vecMoveForce);
        }
    }
}
