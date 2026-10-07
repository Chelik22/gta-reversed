// HandlePedHit 0x4E1CC0 (1393 байт) — разбор для перевода (полудоделан)
//
// РЕАЛЬНЫЙ порядок аргументов оригинала (по использованию, НЕ как в repo-стабе!):
//   HandlePedHit(this, int32 param_2 /*surfaceId2? см. ReportCollision surf2*/,
//                float volume /*param_3, добавляется к GetDefaultVolume(event)*/,
//                int32 duration /*param_4, tail: if != 0 → update-flags*/,
//                eAudioEvents event /*iStack_4, из m_pAudioEventVolumes[event]*/,
//                CPhysical* victim /*unaff_retaddr, [victim+0x36]&7==3 → второй путь*/)
//   repo-стаб: (event, physical, surfaceId, volume, maxVol) — ПЕРЕПРОВЕРИТЬ на соответствие
//   при замене вызова из AddAudioEvent-диспетчера!
//
// СТРУКТУРА:
// 1. ped = m_pPed; if (!ped) return;  weapon = ped->GetActiveWeapon();
// 2. if (!EnsureSoundBankIsLoaded(SND_BANK_GENRL_WEAPONS, SLOT_WEAPON_GEN, true)) return; // bank 0x8F slot 5
// 3. float baseVol = (float)GetDefaultVolume(event) + volume;
//    int16 soundId2 = -1; float speed2 = 1.0f /*local_14=0? НЕТ: local_14=банкСлот=0*/ ...
//    ВНИМАНИЕ: local_14 = bankSlot для ВТОРОГО звука (init 0 = FRONTEND_GAME? скорее slot0),
//              local_10 = speedVariance2 (init -3.0f?! 0xC0400000 = -3.0; case4/0xE → 0x3CF5C28F=0.03f, 0x3D8F5C29=0.07f)
//    ХМ: 0xC0400000 = -3.0 — вероятно это VOL смещение? Сверить по дизасму при переводе!
// 4. switch (weapon->m_Type) → param_3 (soundId!) ДЕНЬ-КЕЙСЫ (float-биты на деле int):
//    0,0x2C,0x2D → 3 (0x4E1CC0+); 1,0x10-0x12,0x16-0x22,0x25-0x2B → 5; 2 → 6; 3 → 7; 4 → 8 (var 0.03);
//    5 → 9; 6 → 10 (0x1A?); 7 → 11; 8 → 12; 10-0xD → 13; 0xE → 14 (var 0.07); 0xF → 15; default → -1
//    (тела — int-биты, Ghidra показал как float 8.1e-44 = int 3 и т.п. — МЛАДШИЕ ЗВУКИ 3..15 из банка?)
// 5. if (victim == NULL || (victim->flags[0x36] & 7) != 3) {   // НЕ ped-в-машине? статус?
//        switch (event) {
//        case 0x3D..0x3F: vol2 = FUN_004da510(param_2); // НЕ РЕВЕРСНУТ: 1 int-арг, float назад (surface-related?)
//                 → ReportCollision(m_pPed, victim, 0x3E, param_2, pedPos, null, 1,1, true, false); break;
//        case 0x40,0x44:   ReportCollision(...); return;      // сразу выход
//        case 0x41..0x43:  soundId = 3 /* unarmed */; vol2 = FUN_004da510(param_2);
//                 → ReportCollision(...); goto TAIL1;
//        default: goto AFTER;                                  // soundId2 = -1 → skip
//        }
//    } else switch (event) {  // victim == статус-3 (ped в машине/водителе?)
//        case 0x3D: switch (weapon) { 1: {id=0x2B, spd=1.0, vol=0x28} default: {-15.0, 0x28}
//                   4,8: {id=0x51, vol2=-9.0 → TAIL2} 5: {-10.0, 0x28} 9: {bank(0x1B,3), id=Rand(7,9) → TAIL2} 0xE: default }
//                   id=GetRandomNumberInRange(uVar7,uVar9); break;
//        case 0x3E: аналог 0x3D, id=0x24/0x27-range;
//        case 0x3F: switch: 4,8→0x51/-9; 5→0x52/-6; 9→rand; default→0x52/-6;
//        case 0x40: id=0x52, vol=-6 → TAIL2
//        case 0x41,0x43: soundId=3; id2=Rand(0x28,0x2B) → TAIL1
//        case 0x42: soundId=3; id2=Rand(0x24,0x27) → TAIL1
//        case 0x44: id2=Rand(0x3B,0x3E) → TAIL2
//        default: id2=-1
//    }
// 6. TAIL1 (soundId путь, if soundId>=0): m_tempSound.Initialise(5, soundId, this, pedPos,
//        baseVol + vol2, 1,1,1, 0, 0x80, local_10 /*variance*/, 0); Register(ped);
//        if (duration) { PhysicalEntity=0; RollOff=0; Speed=0; m_Event=wrap(ms+duration)→+0x18;
//                        m_Event(0x14)=EVENT(!!); SetFlags(4,1); }   // ← здесь +0x14 = iStack_4 (event), в Swing был 0!
//        RequestNewSound(&m_tempSound);
//    TAIL2 (soundId2 путь, if >=0): то же с (local_14 банкСлот, id2, baseVol+vol2, var 0x003D70D845≈0.0138f? = 0x3D70D845)
//
// НЕРАЗРЕШЁННОЕ: 0x4DA510 (int→float; кандидат CSurfaceInfos volume / CAEAudioUtility);
//                victim+0x36&7==3 — какой статус; local_10 init 0xC0400000(-3.0) — vol или variance (СВЕРИТЬ ДИЗАСМОМ).
// Калли: 0x4D9C10 = CAEAudioUtility::GetRandomNumberInRange<int32>; 0x506EB0 = CAudioEngine::ReportCollision ✓.
// Хук: AEPedAudioEntity.cpp:27 — снять .Reversed=false. ПОСЛЕ перевода сверить диспетчер AddAudioEvent
// (арг-порядок) и семантику 0x40-0x44: они идут И сюда, И в HandlePedSwing (оригинал дублирует?).
