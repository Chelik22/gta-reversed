#include "StdInc.h"

#include "TaskComplexFollowLeaderInFormation.h"

void CTaskComplexFollowLeaderInFormation::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexFollowLeaderInFormation, 0x870c3c, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x6949A0);
    RH_ScopedInstall(Destructor, 0x694A40);

    RH_ScopedVMTInstall(Clone, 0x695740, { .reversed = false });
    RH_ScopedVMTInstall(GetTaskType, 0x694A30, { .reversed = false });
    RH_ScopedVMTInstall(CreateNextSubTask, 0x696820, { .reversed = false });
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x6968E0, { .reversed = false });
    RH_ScopedVMTInstall(ControlSubTask, 0x696940, { .reversed = false });
}

// 0x6949A0
CTaskComplexFollowLeaderInFormation::CTaskComplexFollowLeaderInFormation(CPedGroup* pedGroup, CPed* ped, const CVector& posn, float a5) : CTaskComplex() {
    m_Group  = pedGroup;
    m_Leader = ped;
    m_Pos    = posn;
    m_Int    = 4;
    m_Dist   = a5;

    if (m_Leader) {
        CEntity::SafeRegisterRef(m_Leader);
    }
}

// For 0x695740
CTaskComplexFollowLeaderInFormation::CTaskComplexFollowLeaderInFormation(const CTaskComplexFollowLeaderInFormation& o) :
    CTaskComplexFollowLeaderInFormation{
        o.m_Group,
        o.m_Leader,
        o.m_Pos,
        o.m_Dist
    }
{
}

// 0x694A40
CTaskComplexFollowLeaderInFormation::~CTaskComplexFollowLeaderInFormation() {
    CEntity::SafeCleanUpRef(m_Leader);
}

// 0x696820
CTask* CTaskComplexFollowLeaderInFormation::CreateNextSubTask(CPed* ped) {
    return plugin::CallMethodAndReturn<CTask*, 0x696820, CTaskComplexFollowLeaderInFormation*, CPed*>(this, ped);
}

// 0x6968E0
CTask* CTaskComplexFollowLeaderInFormation::CreateFirstSubTask(CPed* ped) {
    return plugin::CallMethodAndReturn<CTask*, 0x6968E0, CTaskComplexFollowLeaderInFormation*, CPed*>(this, ped);
}

// 0x696940
CTask* CTaskComplexFollowLeaderInFormation::ControlSubTask(CPed* ped) {
    return plugin::CallMethodAndReturn<CTask*, 0x696940, CTaskComplexFollowLeaderInFormation*, CPed*>(this, ped);
}
