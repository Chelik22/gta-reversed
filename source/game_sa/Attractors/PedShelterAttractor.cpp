#include "StdInc.h"

#include "PedShelterAttractor.h"
#include "Tasks/TaskTypes/TaskComplexGoToAttractor.h"

// 0x5EF420
// ABI: оригинал возвращает CVector* (указатель на элемент SArray: ret = _First + pedId*0xC,
// см. дизасм 0x5EF55D-0x5EF568 и deref в 0x5EFC40), а НЕ CVector по значению, как раньше в хедере.
// Константы проверены по .data оригинала: 0x858C7C = 1/32768.f, 0x858CBC = TWO_PI, 0x858624 = 1.0f.
// 0x5EE370 = вставка в SArray<CVector> (this=0xC0986C) — в репо это SArray::emplace_back.
CVector* CPedShelterAttractor::GetDisplacement(int32 pedId) {
    if (ms_displacements.empty()) {
        for (auto numAdded = 0; numAdded < 5;) {
            const auto angle = (float)rand() * (1.f / 32768.f) * TWO_PI; // 0x858C7C, 0x858CBC
            const auto radius = 2.0f * ((float)rand() * (1.f / 32768.f)); // fadd st(0), st(0) => *2
            const auto pos = CVector{ cosf(angle) * radius, sinf(angle) * radius, 0.0f };

            // Не даём точкам пересекаться (в оригинале z НЕ вычитается — сравнение с нулевым z новой точки)
            const bool bCollided = rng::any_of(ms_displacements, [&](const CVector& v) {
                return sq(v.x - pos.x) + sq(v.y - pos.y) + sq(v.z) < 1.0f; // 1.0f (0x858624)
            });

            if (bCollided) {
                continue; // Ретрай без инкремента счётчика (0x5EF535: jne 0x5ef460)
            }

            ms_displacements.emplace_back(pos); // 0x5EE370
            numAdded++;
        }
    }
    // SArray::operator[] возвращает КОПИЮ элемента (Core/SArray.h:108), поэтому &ms_displacements[pedId] —
    // адрес временного. Декомпил: ret = _First + pedId*0xC.
    return ms_displacements.begin() + pedId;
}

// 0x5EFC40
void CPedShelterAttractor::ComputeAttractPos(int32 pedId, CVector& outPos) {
    if (m_Fx) {
        outPos = *GetDisplacement(pedId) + m_Pos;
    }
}

// 0x5E9690
void CPedShelterAttractor::ComputeAttractHeading(int32 bQueue, float& heading) {
    heading = CGeneral::GetRandomNumberInRange(0.0f, TWO_PI);
}

// 0x5EF570
// Отличия от CPedAttractor::BroadcastDeparture (репо, PedAttractor.cpp:230): поиск педа — в m_ArrivedPeds
// (не в m_AttractPeds), НЕТ цикла перетаскивания с CTaskComplexSequence/StandStill, erase пары из
// m_PedTaskPairs и erase педа из m_ArrivedPeds идут ДО пересчёта задач, цикл перетаскивания —
// только CTaskComplexGoToAttractor с m_AchieveQueueTime (0x44).
bool CPedShelterAttractor::BroadcastDeparture(CPed* ped) {
    const auto it = rng::find(m_ArrivedPeds, ped);
    if (it == m_ArrivedPeds.end()) {
        return false;
    }
    // (в декомпиле индекс поиска переиспользуется как счётчик, а в цикле раздачи задач
    //  берётся актуальный размер m_ArrivedPeds)

    // Убрать пару (ped, task) из m_PedTaskPairs (элемент 0x14 байт; 0x5EF5DE-0x5EF61B)
    m_PedTaskPairs.erase(rng::find(m_PedTaskPairs, ped, &CPedTaskPair::Ped));

    // Убрать педа из m_ArrivedPeds (0x5EF61B-0x5EF645)
    m_ArrivedPeds.erase(it);

    // Перераздать задачи всем педам, привлечённым аттрактором (0x5EF646-0x5EF6A9)
    for (auto* const attractedPed : m_AttractPeds) {
        const auto n = (int32)(m_ArrivedPeds.size()); // размер после erase

        CVector attractPos;
        ComputeAttractPos(n, attractPos); // vtable+0xC

        float attractHeading;
        ComputeAttractHeading(n, attractHeading); // vtable+0x10

        SetTaskForPed(attractedPed, new CTaskComplexGoToAttractor{
            this,
            attractPos,
            attractHeading,
            m_AchieveQueueTime,
            n,
            PEDMOVE_WALK
        });
    }
    return true;
}

void CPedShelterAttractor::InjectHooks() {
    RH_ScopedVirtualClass(CPedShelterAttractor, 0x86C5B4, 6);
    RH_ScopedCategory("Attractors");

    RH_ScopedInstall(GetDisplacement, 0x5EF420);
    RH_ScopedVMTInstall(ComputeAttractPos, 0x5EFC40);
    RH_ScopedVMTInstall(ComputeAttractHeading, 0x5E9690);
    RH_ScopedVMTInstall(BroadcastDeparture, 0x5EF570);
}
