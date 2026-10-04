/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "RepeatSector.h"
#include "TickCounter.h"
#include <extensions/utility.hpp>

class CEntity;
class CPed;

static constexpr size_t MAX_NUM_ENTITIES = 16; // Mobile

// Just a theory..
// Since this is originally a virtual class..
// Maybe it was templated? Like: `template<class Entity_t, size_t MaxCount>` ?

class CEntityScanner {
protected:
    CTickCounter m_timer;

public: // NOTE: It's too much of a headache
    std::array<CEntity*, MAX_NUM_ENTITIES> m_apEntities; /// SEEMINGLY: The array might have "holes" in it, also it's sorted by distance (closer to further)

protected:
    CEntity* m_pClosestEntityInRange;

public:
    static const int32 ms_iScanPeriod = 10; // Mobile, unused

    friend class CPedIntelligence; // NOTE: There's no other way at all

public:
    CEntityScanner();
    ~CEntityScanner();

    // unused
    CTickCounter* GetTimer() {
        return &m_timer;
    }

protected:
    // NOTSA: Kept virtual (like in the original) so our class layout keeps matching
    // the original one (vptr at 0x0, m_timer at 0x4, ...) -- don't remove it.
    virtual void ScanForEntitiesInRange(const eRepeatSectorList sectorList, const CPed& ped);
    CEntity* GetClosestEntity() const {
        return m_pClosestEntityInRange;
    }

    void Clear();

private: // NOTSA:
    // Actual implementation of `ScanForEntitiesInRange`.
    // NOTE: The hook must be installed on THIS (non-virtual) function, NOT on the virtual one!
    // Hooking the virtual directly would install a jump to the compiler generated vcall thunk
    // (`mov eax, [ecx]; jmp [eax]`), and since all live scanners carry the original vtables
    // (0x86CD40/0x86CD84/0x86CD88/0x86CD8C -- their ctors aren't hooked), whose slot 0 points
    // back at 0x5FFA20, any call would spin forever between the hook jump and the thunk (freeze v16).
    void ScanForEntitiesInRangeImpl(const eRepeatSectorList sectorList, const CPed& ped);

private: // NOTSA:
    friend void InjectHooksMain();
    static void InjectHooks();

    CEntityScanner* Constructor() {
        this->CEntityScanner::CEntityScanner();
        return this;
    }

    CEntityScanner* Destructor() {
        this->CEntityScanner::~CEntityScanner();
        return this;
    }

public:
    /// View of all non-null entities as a view of `T&`
    template<typename T = CEntity>
    auto GetEntities() const {
        using namespace rng::views;
        using namespace notsa;

        return m_apEntities
             | filter([](auto&& e) { return e != nullptr; })  // Filter all null
             | transform([](CEntity* e) -> T& { return static_cast<T&>(*e); }); // Cast to required type and dereference
    }
};

VALIDATE_SIZE(CEntityScanner, 0x50);
