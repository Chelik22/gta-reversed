struct CTask { virtual ~CTask(); };
struct PedTaskPair { void* ped; CTask* task; int slot; char _pad[8]; };
struct CPedAttractorU {
    void AbortPedTasks();
    char _pad[0x30];
    PedTaskPair* pFirst;
    PedTaskPair* pLast;
};
void CPedAttractorU::AbortPedTasks() {
    int n = !pFirst ? 0 : (int)(pLast - pFirst);
    if (n > 0) { int i = 0; do {{
        PedTaskPair* e = (PedTaskPair*)((char*)pFirst + i);
        CTask* t = e->task;
        e->ped = 0;
        if (t) delete t;
        i += 0x14;
        --n;
        e->task = 0;
        e->slot = -1;
    }} while (n); }
}
