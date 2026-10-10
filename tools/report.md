# bin_diff report -- compiled code vs original binary

- original: D:\projects\keep\ghidra\gta_sa.exe (base 0x400000)
- ours: D:\projects\gta_sa_reversed\bin\Release\gta_reversed.asi (base 0x10000000)
- generated: 2026-10-08 02:13:17
- score = 0.45*mnemonic + 0.30*constants + 0.15*calls + 0.10*branches
- normalization: opcode classes (registers stripped), MSVC frame-pointer/GS-cookie boilerplate stripped

Compared: 1 functions | scored: 1 | unresolved: 0

- identical-ish (>=0.95): 0
- close (0.85-0.95): 0
- drifted (0.70-0.85): 0
- diverged (<0.70): 1

## Worst 50

| score | mnem | const | calls | branch | function | orig | bytes o/u |
|---|---|---|---|---|---|---|---|
| 0.364 | 0.27 | 0.59 | 0.00 | 0.64 | CGarage::FindDoorsWithGarage | 0x449FF0 | 384/356 |

## Top-20 detail (worst)

### CGarage::FindDoorsWithGarage (orig 0x449FF0, score 0.364)

- components: mnemonic 0.271, constants 0.593, calls 0.000, branches 0.643
- bytes: original 384, ours 356 (insns 128 vs 101)
- branches: orig jcc=10 back=1 jmp=4 | ours jcc=8 back=1 jmp=1
- missing constants (in original, absent in ours): i:32 x2, i:412 x2, i:5 x2, i:128 x1, i:48 x1, f:0.5 x1
- extra constants (in ours only): i:24 x1, i:4 x1
- callees only in ours: CPlaceable::GetPosition, CPool<CObject,CCutsceneObject>::GetAt, CPool<CObject,CCutsceneObject>::GetSize, GetObjectPool, sqrt