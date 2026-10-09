#include "StdInc.h"

#include "TaskUtilityLineUpPedWithCar.h"

void CTaskUtilityLineUpPedWithCar::InjectHooks() {
    RH_ScopedClass(CTaskUtilityLineUpPedWithCar);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x64FBB0);
    RH_ScopedInstall(Destructor, 0x64FC00);

    RH_ScopedInstall(GetLocalPositionToOpenCarDoor, 0x64FC10, { .Reversed = false }); // STUB_BLOCKED (batch w2-17-mixed) - left as redirect-stub, do not remove
    RH_ScopedInstall(GetPositionToOpenCarDoor, 0x650A80);
    RH_ScopedInstall(ProcessPed, 0x6513A0, { .Reversed = false }); // STUB_BLOCKED (batch w2-17-mixed) - left as redirect-stub, do not remove
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

// 0x64FC10 - STUB_BLOCKED (batch w2-17-mixed): decompile reads a float at handling+0xD4 whose offset layout
// mismatches the repo's tHandlingData, plus a second ComputeAnimDoorOffsets call with lost group/index pair.
// Redirect-stub kept intentionally.
CVector CTaskUtilityLineUpPedWithCar::GetLocalPositionToOpenCarDoor(CVehicle* vehicle, float animProgress, CAnimBlendAssociation* assoc) {
    CVector out;
    plugin::CallMethodAndReturn<CVector, 0x64FC10, CTaskUtilityLineUpPedWithCar*, CVector*, CVehicle*, float, CAnimBlendAssociation*>(this, &out, vehicle, animProgress, assoc);
    return out;
}

// 0x650A80 (batch w2-17-mixed, fragment 0x650A80.cpp)
// Original returns the CVector via sret (result ptr as 1st arg); helpers 0x59BCF0/0x59C790/0x59ACD0
// are matrix push/transform/pop == TransformPoint. Hopefully this compiles to the same stuff (see note above).
CVector CTaskUtilityLineUpPedWithCar::GetPositionToOpenCarDoor(CVehicle* vehicle, float animProgress, CAnimBlendAssociation* assoc) {
    return vehicle->GetMatrix().TransformPoint(GetLocalPositionToOpenCarDoor(vehicle, animProgress, assoc));
}

// 0x6513A0 - STUB_BLOCKED (batch w2-17-mixed): ~690 lines, inlined RtQuatSlerp + several unnamed
// CMatrix helpers. Redirect-stub kept intentionally.
bool CTaskUtilityLineUpPedWithCar::ProcessPed(CPed* ped, CVehicle* vehicle, CAnimBlendAssociation* assoc) {
    return plugin::CallMethodAndReturn<bool, 0x6513A0, CTaskUtilityLineUpPedWithCar*, CPed*, CVehicle*, CAnimBlendAssociation*>(this, ped, vehicle, assoc);
}
