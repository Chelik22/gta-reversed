// Matching unit: CPedAttractor::AbortPedTasks @ 0x5EAF60 (98 bytes)
// v2: if/else with zero-case first (jne polarity), registers per original
// (ebx = count, edi = offset).
struct CTask { virtual ~CTask(); };

struct PedTaskPair {
    void*  ped;    // +0
    CTask* task;   // +4
    int    slot;   // +8
    char   _pad[8];// -> 0x14 stride
};

struct CPedAttractorU {
    void AbortPedTasks();
    char _pad[0x30];
    PedTaskPair* pFirst; // +0x30
    PedTaskPair* pLast;  // +0x34
};

void CPedAttractorU::AbortPedTasks() {
    int n;
    if (!pFirst)
        n = 0;
    else
        n = (int)(pLast - pFirst);
    if (n > 0) {
        int i = 0;
        do {
            PedTaskPair* e = (PedTaskPair*)((char*)pFirst + i);
            CTask* t = e->task;
            e->ped = 0;
            if (t)
                delete t;
            i += sizeof(PedTaskPair);
            --n;
            e->task = 0;
            e->slot = -1;
        } while (n);
    }
}
