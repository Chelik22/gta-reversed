// HandlePedSwing 0x4E1A40 — полный разбор для следующей итерации (готов к переводу)
//
// Сигнатура: CAEPedAudioEntity::HandlePedSwing(eAudioEvents event, int32 a7, uint32 maxVol)
//   (репо-стаб уже имеет эту сигнатуру; Ghidra видел this + event; a7/maxVol в хвосте)
//
// Разбор (проверено дизасмом):
// 1. CPed* ped = m_pPed (this+0x94); if (!ped) return;
//    CWeapon* w = &ped->m_aWeapons[ped->m_nActiveWeaponSlot];   // +0x5A0 + slot(+0x718,sbyte)*0x1C
// 2. if (!AEAudioHardware::IsSoundBankLoaded(0x8F /*bank 143*/, 5)) {
//        if (CAudioEngine::IsLoadingTuneActive()) return;       // 0x506D90
//        AEAudioHardware::LoadSoundBank(0x8F, 5);               // 0x4D88A0
//        return;                                                // звук сыграет со след. раза
//    }
// 3. int16 soundId = -1; float speed = ?; float volAdd = ?;     // ebx=-1 init в prologue
//    switch (w->m_Type) {   // eWeaponType
//      UNARMED(0), BRASS(1), 9,0xE,0x10-0x12,0x16-0x22,0x25-0x2D: soundId=0x57; speed=0x841f(0x3F570A3D); volAdd=[0x85A5C0];
//      2(PIPE?),0xF:                        soundId=0x2F; break;                     // speed/volAdd не трогает (=?)
//      3,7:                                  soundId=0x56; speed=0.841f; break;
//      4,8:                                  soundId=0x58; volAdd=[0x859024]; goto LAB;
//      10,0xB,0xC,0xD:                       soundId=0x56; speed=0.841f; volAdd=[0x85A5C0];
//      // default: soundId остаётся -1
//    }
// 4. switch (event) {   // 0x4E1AF1: читает СТАК-АРГ = event (Ghidra назвал unaff_retaddr)
//      AE_PED_HIT_GROUND_KICK(0x40), AE_PED_HIT_HIGH_UNARMED(0x41), AE_PED_HIT_LOW_UNARMED(0x42):
//          soundId=0x57; speed=0.841f; volAdd=[0x85A5C0];
//      AE_PED_HIT_MARTIAL_PUNCH(0x43), AE_PED_HIT_MARTIAL_KICK(0x44):
//          soundId=0x57; speed=0.841f; volAdd=[0x858B18];
//      default: if ((int16)soundId < 0) return;   // оружие не попало в кейсы и не хит-ивент
//    }
//    !!! ЗНАЧИТ оригинальный AddAudioEvent маршрутизирует 0x40-0x44 сюда (репо-диспетчер
//    шлёт их в HandlePedHit — СВЕРИТЬ при реверсе HandlePedHit 0x4E1CC0!)
//
// 5. m_sSound.Initialise(                                  // CAESound at this+8; 0x4EFE50
//        5 /*bankSlot*/, soundId, this, ped->GetPosition(), // позиция: matrix?+0x30 : +4
//        (float)(int)*0xBD00F8 + volAdd,                    // volume: глобал-int + константа
//        1.0f, speed, 1.0f, 0, 0x80 /*flags*/, 0.04f, 0);
//    m_sSound.RegisterWithPhysicalEntity(ped);              // 0x4EF820
//    if (a7 != 0) {
//        field_28 = 0; field_24 = 0;                        // this+0x28/+0x24 (сверить имена в .h)
//        field_18 = (float)(uint32)(CTimer::m_snTimeInMilliseconds + a7);  // wrap: +2^32 если <0
//        field_14 = 0;
//        m_sSound.SetFlags(4 /*eSoundEnvironment*/, 1);     // 0x4EF2B0
//    }
//    AESoundManager::RequestNewSound(&m_sSound);            // 0x4EFB10, this+8
//
// Флоаты-константы: 0x3F570A3D=0.841f, 0x3F2B851F≈0.6677f, 0x3D23D70A=0.04f.
// Прочитать из exe: [0x85A5C0], [0x859024], [0x858B18] (fld-константы громкости).
// Неизвестное: глобал 0xBD00F8 (int → float; кандидат CTimer/CWeather-related — проверить в Ghidra xrefs).
//
// Идиома перевода: современные PlaySound({...}) как в HandleSwimSplash (0x4E26A0),
// НО условный SetFlags(4,1) требует прямой m_sSound.Initialise-путь (не PlaySound-хелпер).
// Хук: source/game_sa/Audio/Entities/AEPedAudioEntity.cpp:26 — снять { .Reversed = false }.
