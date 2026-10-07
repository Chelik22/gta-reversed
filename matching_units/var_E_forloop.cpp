struct CTask { virtual ~CTask(); };
struct PedTaskPair {
    void*  ped; CTask* task; int slot; char _pad[8];
};
struct CPedAttractorU {
    void AbortPedTasks();
    char _pad[0x30];
    PedTaskPair* pFirst;
    PedTaskPair* pLast;
};
void CPedAttractorU::AbortPedTasks() {
    int n = pFirst ? (int)(pLast - pFirst) : 0;
    for (int i = 0; n > 0; i += 0x14, --n) {{
            PedTaskPair* e = (PedTaskPair*)((char*)pFirst + i);
            CTask* t = e->task;
            e->ped = 0;
            if (t) delete t;
            
            e->task = 0;
            e->slot = -1;
        }}
}
