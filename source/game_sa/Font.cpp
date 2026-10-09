/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
// ============================================================================
// INTEGRATED FILE — source/game_sa/Font.cpp
// Батч: PR gta-reversed#1414 «CFont done» (wave w2-26-Font, sprint-100)
//
// Источник тел:  D:\projects\keep\pr_patches\pr1414_functions.cpp
//                (извлечено из D:\projects\keep\pr_patches\pr1414.diff)
// Заметки:       D:\projects\keep\pr_patches\pr1414_notes.md
//
// БАТЧ (5 функций + клей):
//   1. CFont::PrintChar              0x718A10  (был redirect-стаб)
//   2. CFont::RenderFontBuffer       0x719840  (был redirect-стаб)
//   3. CFont::ProcessCurrentString   0x71A220  (был redirect-стаб)
//   4. CFont::PrintString (ranged)   0x719B40  (функции не было вовсе;
//        здесь — file-scope PrintString_Ranged, т.к. Font.h НЕ трогаем —
//        объявить static-член в классе без правки хедера нельзя)
//   5. GetScriptLetterSize           0x719670  (был redirect-стаб)
//   + клей RenderFontBuffer: file-scope класс RenderFontBuffer (буфер
//     0xC716B0, ровно 512 байт до gFontData@0xC718B0) и s_RenderEndPtr
//     (0xC716A8) — замена репо-глобалов FontRenderStateBuf/pEmptyChar.
//
// ИЗМЕНЕНИЯ ВНЕ 5 ФУНКЦИЙ (минимально-необходимые):
//   - InjectHooks: сняты {.Reversed = false} с PrintChar / RenderFontBuffer /
//     ProcessCurrentString (батч); добавлены хуки PrintString_Ranged (0x719B40)
//     и GetScriptLetterSize (0x719670, замена закомментированной строки).
//   - CFont::InitPerFrame: ОДНА строка — `pEmptyChar = &FontRenderStateBuf[0]`
//     заменена на `s_RenderEndPtr = s_RenderFontBuffer.GetTop()` (та же запись
//     0xC716B0 в 0xC716A8), т.к. старые глобалы удалены вместе с клеем.
//
// АДАПТАЦИИ ПОД ТЕКУЩИЙ РЕПО-ХЕДЕР (Font.h НЕ менялся! apply-пайплайн
// переносит только .cpp — см. tools/apply_integration.py):
//   - PR-типов eFontTextureStyle/eFontTextureName в репо НЕТ:
//       m_nFontStyle (uint8): 0=SUBTITLES_AND_GOTHIC, 1=PRICEDOWN, 2=MENU;
//       m_wFontTexture (uint16): 0=FONT2, 1=FONT1, >=2=FONT_UNK_2 (мусор).
//   - CFontChar: PR-поля Slope/SlopeRef -> репо m_fSlant/m_vSlanRefPoint;
//     PR-метод GetText() -> file-scope FontCharGetText(CFontChar*)
//     (= reinterpret_cast<GxtChar*>(fc + 1), тот же байтовый смысл).
//   - PR CFontDetails -> репо-статики ПО АДРЕСАМ (CFontDetails@0xC71A60):
//       Color=m_Color(0xC71A60) Scale=m_Scale(+0x04) Slope=m_fSlant(+0x0C)
//       SlopeRef=m_fSlantRefPoint(+0x10) Justify=m_bFontJustify(+0x18)
//       Centre=m_bFontCentreAlign(+0x19) RightJustify=m_bFontRightAlign(+0x1A)
//       Background=m_bFontBackground(+0x1B)
//       BackgroundOutline=m_bEnlargeBackgroundBox(+0x1C)
//       Proportional=m_bFontPropOn(+0x1D)
//       Shadow=m_bFontIsBlip(+0x1E)  <- PR переименовал m_bFontIsBlip
//       AlphaFade=m_fFontAlpha(+0x20) BackgroundColor=m_FontBackgroundColor(+0x24)
//       WrapEnd=m_fWrapx(+0x28) CentreX=m_fFontCentreSize(+0x2C)
//       RightJustifyWrap=m_fRightJustifyWrap(+0x30)
//       FontTextureName=m_FontTextureId(+0x34) FontTextureStyle=m_FontStyle(+0x35)
//       DropAmount=m_nFontShadow(+0x36) DropColor=m_FontDropColor(+0x37)
//       EdgeAmount=m_nFontOutlineSize(+0x3B) EdgeSpace=m_nFontOutline(+0x3C)
//     (в репо m_nFontOutline и m_nFontOutlineOrShadow — ОДИН адрес 0xC71A9C,
//     задвоение в хедере; EdgeSpace отображён на m_nFontOutline).
//   - CFont::ParseToken оставлен со СТАРОЙ сигнатурой char*()(char*,...) из
//     репо-хедера (сменить без правки Font.h нельзя — RH_ScopedInstall брал бы
//     адрес не той перегрузки). Константность восстановлена file-scope
//     обёрткой ParseTokenConst(const GxtChar*, ...) — единственное место с
//     cast'ами; PR менял сигнатуру на const GxtChar* — см. «Font.h upgrade» ниже.
//   - GetLetterIdPropValue(uint8) (0x718770) — тело уже есть в репо, НЕ тронуто.
//     Добавлен статический 3-арговый хелпер из PR (нужен GetScriptLetterSize).
//
// НЕ ИЗ БАТЧА — СОЗНАТЕЛЬНО НЕ ТРОНУТО:
//   - FindSubFontCharacter (0x7192C0): тело в репо есть; PR держал его хук
//     выключенным («hangs up the game for some reason», diff 1349) и менял
//     `letterId - 128` (Font.cpp:780) на `letterId + 128` (кандидат-фикс,
//     pr1414_notes.md). Здесь репо-вариант сохранён; хук уже включён коммитом
//     83c7c61c «MEGA FLAG SWEEP» — оставлен как есть.
//   - Все Set*/Get*/GetStringWidth/GetTextRect/PrintString(3-арг)/... —
//     ALREADY DONE, тела репо сохранены байт-в-байт.
//
// FONT.H UPGRADE (опционально, НЕ для этого пайплайна): PR реструктуризирует
// Font.h (eFontTextureStyle/eFontTextureName, CFontChar::GetText(),
// struct CFontDetails@0xC71A60/0x40, ParseToken->const GxtChar*,
// FindSubFontCharacter(uint8,eFontTextureStyle) file-global, ranged PrintString,
// GetTextRect(CRect&)) — см. pr1414.diff 1353-1676 и pr1414_notes.md разд. A/B.
// После такого апгрейда обёртки ParseTokenConst/PrintString_Ranged и
// числовые литералы стилей заменяются на PR-варианты напрямую.
//
// FINAL-FIX (final_Font.cpp, эта сессия) — единственная правка против
// new_Font.cpp: инсталл GetLetterIdPropValue переведён на
// RH_ScopedGlobalOverloadedInstall(..., float(*)(uint8)) — см. комментарий
// у строки инсталла в InjectHooks(). Остальное байт-в-байт из new_Font.cpp.
//
// Компиляция (эта сессия, MSVC 14.51 Hostx64/x86 -> x86, /W3 /WX, PCH репо
// cmake_pch.hxx, флаги из vcxopts.json; харнесс
// tmp_build_font/run_compile_check_font.py):
//   new_Font.cpp   -> rc=2, errors=1  (C2672 @ :212 — воспроизведена)
//   final_Font.cpp -> rc=0, errors=0  (obj собран)
// ============================================================================

#include "StdInc.h"

#include "Font.h"

#include "eLanguage.h"

// --- добавлено интеграцией (каждый путь существует в репо) ---
#include <cstddef>              // std::byte (буфер клея RenderFontBuffer)
#include <utility>              // std::exchange (PrintString ranged: outline/shadow)
#include "Scripts/TheScripts.h" // CTheScripts::IntroTextLines / NumberOfIntroTextLinesThisFrame (GetScriptLetterSize); также уже включён через StdInc.h:455
// RwRenderStateSet/rwRENDERSTATEVERTEXALPHAENABLE (RenderFontBuffer) — через StdInc.h:
// RenderWare/rw/rwplcore.h:5124 + RenderWare.h:48 (RWRSTATE); NOTSA_LOG_ERR — app_debug.h:69;
// NOTSA_UNREACHABLE — Base.h:120; StaticRef — Base.h:164; SCREEN_STRETCH_X/Y — common.h:41-42.

// ============================================================================
// FILE-SCOPE СОСТОЯНИЕ И КЛЕЙ (PR#1414)
// ============================================================================

auto& gFontData = StaticRef<std::array<tFontData, 2>>(0xC718B0);

// --- клей RenderFontBuffer (замена репо-глобалов
//     FontRenderStateBuf 0xC716B0 / pEmptyChar 0xC716A8; Font.cpp:13-14) ---
class RenderFontBuffer {
public:
    // Render font buffer is laid in memory like this:
    // [ CFontChar struct (config) ] 'H' 'e' 'l' 'l' 'o' '\0' [ PAD ] [ next CFontChar struct ] ...
    // ^~~ &s_RenderFontBuffer[0]    ^~ `->GetText()`    ^    ^~ Align for the next struct
    //                                                   |~ Terminator
    CFontChar* GetTop() { return reinterpret_cast<CFontChar*>(&m_UnderlyingBuffer[0]); }

    CFontChar* FindNext(const void* p) {
        if (*static_cast<const GxtChar*>(p) != '\0') {
            NOTSA_UNREACHABLE("expected null terminator to calculate the next fontchar");
        }
        const auto next = ((uintptr)p + 4) & ~3; // align to 4 bytes
        return reinterpret_cast<CFontChar*>(&m_UnderlyingBuffer[next - (uintptr)m_UnderlyingBuffer.data()]);
    }
private:
    std::array<std::byte, 512> m_UnderlyingBuffer{}; // Not an array of CFontChars, refer to RenderFontBuffer
};
static auto& s_RenderFontBuffer = StaticRef<RenderFontBuffer>(0xC716B0);
static auto& s_RenderEndPtr     = StaticRef<void*>(0xC716A8); // Points to the end of queued font renders at s_RenderFontBuffer

// NOTSA — замена PR-метода CFontChar::GetText() (в репо-хедере его нет).
// Текст идёт сразу после структуры: reinterpret_cast<GxtChar*>(this + 1).
static GxtChar* FontCharGetText(CFontChar* c) {
    return reinterpret_cast<GxtChar*>(c + 1);
}

// NOTSA — const-correct обёртка над CFont::ParseToken.
// PR#1414 сменил сигнатуру на const GxtChar*()(const GxtChar*, CRGBA&, bool, GxtChar*),
// репо-хедер оставляет char*()(char*, ...) — сменить без правки Font.h нельзя.
// Все новые вызовы (RenderFontBuffer / ProcessCurrentString / PrintString_Ranged)
// идут через эту обёртку; тело CFont::ParseToken не тронуто (ALREADY DONE).
static const GxtChar* ParseTokenConst(const GxtChar* text, CRGBA& color, bool isBlip, GxtChar* tag) {
    return reinterpret_cast<const GxtChar*>(
        CFont::ParseToken(
            reinterpret_cast<char*>(const_cast<GxtChar*>(text)),
            color,
            isBlip,
            reinterpret_cast<char*>(tag)
        )
    );
}

// NOTSA (PR#1414, diff 1258-1264) — статический хелпер; `style` — индекс текстуры
// (RenderState.m_wFontTexture). Нужен GetScriptLetterSize (ниже по файлу).
static float GetLetterIdPropValue(uint8 letterId, bool propCond, uint16 style) {
    if (propCond) {
        return gFontData[style].m_propValues[letterId];
    } else {
        return gFontData[style].m_unpropValue;
    }
}

// NOTSA — ranged-перегрузка CFont::PrintString (0x719B40) из PR#1414.
// В репо-хедере объявления нет (добавлять член в Font.h нельзя), поэтому
// функция объявлена на уровне файла; хук ставится через RH_ScopedGlobalInstall.
static void PrintString_Ranged(float x, float y, const GxtChar* start, const GxtChar* end, float wrap);

void CFont::InjectHooks() {
    RH_ScopedClass(CFont);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Initialise, 0x5BA690);
    RH_ScopedInstall(Shutdown, 0x7189B0);
    RH_ScopedInstall(PrintChar, 0x718A10); // PR#1414: реверснут, флаг снят
    RH_ScopedInstall(ParseToken, 0x718F00);

    // styling functions
    RH_ScopedInstall(SetScale, 0x719380);
    RH_ScopedInstall(SetScaleForCurrentLanguage, 0x7193A0);
    RH_ScopedInstall(SetSlantRefPoint, 0x719400);
    RH_ScopedInstall(SetSlant, 0x719420);
    RH_ScopedInstall(SetColor, 0x719430);
    RH_ScopedInstall(SetFontStyle, 0x719490);
    RH_ScopedInstall(SetWrapx, 0x7194D0);
    RH_ScopedInstall(SetCentreSize, 0x7194E0);
    RH_ScopedInstall(SetRightJustifyWrap, 0x7194F0);
    RH_ScopedInstall(SetAlphaFade, 0x719500);
    RH_ScopedInstall(SetDropColor, 0x719510);
    RH_ScopedInstall(SetDropShadowPosition, 0x719570);
    RH_ScopedInstall(SetEdge, 0x719590);
    RH_ScopedInstall(SetProportional, 0x7195B0);
    RH_ScopedInstall(SetBackground, 0x7195C0);
    RH_ScopedInstall(SetBackgroundColor, 0x7195E0);
    RH_ScopedInstall(SetJustify, 0x719600);
    RH_ScopedInstall(SetOrientation, 0x719610);

    RH_ScopedInstall(InitPerFrame, 0x719800);
    RH_ScopedInstall(RenderFontBuffer, 0x719840); // PR#1414: реверснут, флаг снят
    RH_ScopedInstall(GetStringWidth, 0x71A0E0);
    RH_ScopedInstall(DrawFonts, 0x71A210);
    RH_ScopedInstall(ProcessCurrentString, 0x71A220); // PR#1414: реверснут, флаг снят
    RH_ScopedInstall(GetNumberLines, 0x71A5E0);
    RH_ScopedInstall(ProcessStringToDisplay, 0x71A600);
    RH_ScopedInstall(GetTextRect, 0x71A620);
    RH_ScopedInstall(PrintString, 0x71A700);
    RH_ScopedGlobalInstall(PrintString_Ranged, 0x719B40); // PR#1414: ranged-перегрузка PrintString
    RH_ScopedInstall(PrintStringFromBottom, 0x71A820);
    RH_ScopedInstall(GetCharacterSize, 0x719750);
    RH_ScopedInstall(LoadFontValues, 0x7187C0);
    RH_ScopedGlobalInstall(GetScriptLetterSize, 0x719670); // PR#1414: реверснут (замена закомм. инсталла)
    RH_ScopedInstall(FindSubFontCharacter, 0x7192C0); // НЕ из батча: PR держал {.Reversed=false} ("hangs up the game"), репо включил в 83c7c61c — оставлено как в репо
    // FINAL-FIX (эта сессия): было RH_ScopedGlobalInstall(GetLetterIdPropValue, 0x718770)
    // -> C2672 "InstallStatic: no matching overloaded function found" (репродьюс
    //   MSVC 14.51 x86: 1 error @ new_Font.cpp:212). Причина: в этом TU у
    //   GetLetterIdPropValue ДВЕ перегрузки — 1-арговая (Font.h:168 + тело ниже)
    //   и 3-арговый хелпер PR (выше), поэтому &GetLetterIdPropValue без целевого
    //   типа неоднозначно. RH_ScopedInstall НЕ годится: он резолвит
    //   &RHCurrentNS::fn == &CFont::GetLetterIdPropValue, а такого члена в
    //   Font.h нет (C2039). Решение — как в самом PR#1414 (pr1414.diff:1351) и
    //   как принято в репо (common.cpp:26, MakeUpperCase ""):
    RH_ScopedGlobalOverloadedInstall(GetLetterIdPropValue, "", 0x718770, float(*)(uint8));
}

// 0x7187C0
void CFont::LoadFontValues() {
    CFileMgr::SetDir("");
    auto* file = CFileMgr::OpenFile("DATA\\FONTS.DAT", "rb");

    char attrib[32];

    uint32 totalFonts = 0;
    uint32 fontId = 0;

    for (auto line = CFileLoader::LoadLine(file); line; line = CFileLoader::LoadLine(file)) {
        if (*line == '\0' || *line == '#')
            continue;

        if (sscanf_s(line, "%s", SCANF_S_STR(attrib)) == EOF)
            continue;

        if (!memcmp(attrib, "[TOTAL_FONTS]", 14)) {
            auto nextLine = CFileLoader::LoadLine(file);

            VERIFY(sscanf_s(nextLine, "%d", &totalFonts) == 1);
        }
        else if (!memcmp(attrib, "[FONT_ID]", 10)) {
            auto nextLine = CFileLoader::LoadLine(file);

            VERIFY(sscanf_s(nextLine, "%d", &fontId) == 1);
        }
        else if (!memcmp(attrib, "[REPLACEMENT_SPACE_CHAR]", 25)) {
            auto nextLine = CFileLoader::LoadLine(file);
            uint8 spaceValue;

            VERIFY(sscanf_s(nextLine, "%hhu", &spaceValue) == 1);
            gFontData[fontId].m_spaceValue = spaceValue;
        }
        else if (!memcmp(attrib, "[PROP]", 7)) {
            for (int32 i = 0; i < 26; i++) {
                auto nextLine = CFileLoader::LoadLine(file);
                int32 propValues[8]{};

                VERIFY(sscanf_s(nextLine, "%d  %d  %d  %d  %d  %d  %d  %d",
                    &propValues[0], &propValues[1], &propValues[2], &propValues[3],
                    &propValues[4], &propValues[5], &propValues[6], &propValues[7]
                ) == 8);

                for (auto j = 0u; j < std::size(propValues); j++) {
                    gFontData[fontId].m_propValues[i * 8 + j] = propValues[j];
                }
            }
        }
        else if (!memcmp(attrib, "[UNPROP]", 9)) {
            auto nextLine = CFileLoader::LoadLine(file);
            uint32 unpropValue;

            VERIFY(sscanf_s(nextLine, "%d", &unpropValue) == 1);
            gFontData[fontId].m_unpropValue = unpropValue;
        }
    }

    CFileMgr::CloseFile(file);
}

// 0x5BA690
void CFont::Initialise() {
    int32 fontsTxd = CTxdStore::AddTxdSlot("fonts");
    CTxdStore::LoadTxd(fontsTxd, "MODELS\\FONTS.TXD");
    CTxdStore::AddRef(fontsTxd);
    CTxdStore::PushCurrentTxd();
    CTxdStore::SetCurrentTxd(fontsTxd);
    Sprite[0].SetTexture("font2", "font2m");
    Sprite[1].SetTexture("font1", "font1m");

    LoadFontValues();

    SetScale(1.0f, 1.0f);
    SetSlantRefPoint(SCREEN_WIDTH, 0.0f);
    SetSlant(0.0f);

    SetColor(CRGBA(255, 255, 255, 0));
    SetOrientation(eFontAlignment::ALIGN_LEFT);
    SetJustify(false);

    SetWrapx(SCREEN_WIDTH);

    SetCentreSize(SCREEN_WIDTH);
    SetBackground(false, false);

    SetBackgroundColor(CRGBA(128, 128, 128, 128));
    SetProportional(true);
    SetFontStyle(eFontStyle::FONT_GOTHIC);
    SetRightJustifyWrap(0.0f);
    SetAlphaFade(255.0f);
    SetDropShadowPosition(0);
    CTxdStore::PopCurrentTxd();

    int32 ps2btnsTxd = CTxdStore::AddTxdSlot("ps2btns");
    CTxdStore::LoadTxd(ps2btnsTxd, "MODELS\\PCBTNS.TXD");
    CTxdStore::AddRef(ps2btnsTxd);
    CTxdStore::PushCurrentTxd();
    CTxdStore::SetCurrentTxd(ps2btnsTxd);

    ButtonSprite[1].SetTexture("up", "upA");
    ButtonSprite[2].SetTexture("down", "downA");
    ButtonSprite[3].SetTexture("left", "leftA");
    ButtonSprite[4].SetTexture("right", "rightA");

    CTxdStore::PopCurrentTxd();
}

// 0x7189B0
void CFont::Shutdown() {
    std::ranges::for_each(Sprite, [](CSprite2d& sprite) { sprite.Delete(); });
    CTxdStore::SafeRemoveTxdSlot("fonts"); // FIX_BUGS: Added check for is slot exists
    std::ranges::for_each(ButtonSprite, [](CSprite2d& sprite) { sprite.Delete(); });
    CTxdStore::SafeRemoveTxdSlot("ps2btns"); // FIX_BUGS: Added check for is slot exists
}

// this adds a single character into rendering buffer
// 0x718A10
// PR#1414. Адаптация к репо-хедеру: eFontTextureStyle::PRICEDOWN -> 1,
// +eFontTextureName::FONT_UNK_2 -> 2, Slope -> m_fSlant.
void CFont::PrintChar(float x, float y, char character) {
    // out of screen
    if (x < 0.0f || x > SCREEN_WIDTH || y < 0.0f || y > SCREEN_HEIGHT) {
        return;
    }

    if (PS2Symbol) {
        // extra symbol to be drawn (e.g. PS2 buttons)
        const CRect rt{
            x,
            2.0f * RenderState.m_fHeight + y,
            17.0f * RenderState.m_fHeight + x,
            19.0f * RenderState.m_fHeight + y
        };

        ButtonSprite[PS2Symbol].Draw(rt, { 255, 255, 255, RenderState.m_color.a });
        return;
    }

    bool  zeroed{};
    uint8 letter = character;

    if (!letter || letter == '?') {
        letter    = 0;
        zeroed    = true;
        character = 0;
    }

    if (RenderState.m_nFontStyle == 1 /* eFontTextureStyle::PRICEDOWN */ && letter == 0xD0) {
        letter = 0;
        zeroed = true;
    }

    const auto u1     = (letter % 16) / 16.0f;
    const auto vBase  = (letter / 16);

    if (RenderState.m_wFontTexture >= 2 /* +eFontTextureName::FONT_UNK_2 */) {
        NOTSA_LOG_ERR("weird texture {}, for print char {}", RenderState.m_wFontTexture, character);
        float v18 = vBase / 16.0f;

        if (!zeroed) {
            const auto  propValue = GetLetterIdPropValue(character) / 32.0f;
            const CRect rt{
                x,
                y,
                32.0f * propValue * RenderState.m_fWidth + x,
                16.0f * RenderState.m_fHeight + y
            };
            const auto u2 = (propValue / 16.0f) + u1;
            const auto v2 = v18;
            const auto u3 = u1;
            const auto v3 = v18 + 0.0625f;
            const auto u4 = u2 - 0.0001f;
            const auto v4 = v3 - 0.0001f;
            CSprite2d::AddToBuffer(rt, RenderState.m_color, u1, v18, u2, v2, u3, v3, u4, v4);
        }
        return;
    }

    const auto v19 = vBase / 12.8f;
    if (!zeroed) {
        CRect rt{};
        rt.left = x;

        if (RenderState.m_fSlant == 0.0f) {
            rt.bottom = y;
            rt.right  = 32.0f * RenderState.m_fWidth + x;

            if (letter < 0xC0) {
                rt.top        = 20.0f * RenderState.m_fHeight + y;

                const auto v1 = v19 + 0.0021f;
                const auto u2 = u1 + 0.0615f; // u1 + 0.0625 - 0.001
                const auto v2 = v1;
                const auto u3 = u1;
                const auto v3 = v19 + 0.076025f; // v19 + 0.078125 - 0.0021
                const auto u4 = u2;
                const auto v4 = v3;
                CSprite2d::AddToBuffer(rt, RenderState.m_color, u1, v1, u2, v2, u3, v3, u4, v4);
            } else {
                rt.top        = 16.0f * RenderState.m_fHeight + y;

                const auto v7 = v19 + 0.078125f;
                const auto v1 = v19 + 0.0021f;
                const auto u2 = u1 + 0.0615f;
                const auto v2 = v1;
                const auto u3 = u1;
                const auto v3 = v7 - 0.016f;
                const auto u4 = u2;
                const auto v4 = v7 - 0.015f;
                CSprite2d::AddToBuffer(rt, RenderState.m_color, u1, v1, u2, v2, u3, v3, u4, v4);
            }
        } else { // Slant != 0.0f
            rt.bottom = y + 0.015f;
            rt.right  = 32.0f * RenderState.m_fWidth + x;
            rt.top    = 20.0f * RenderState.m_fHeight + y + 0.015f;

            float v8  = v19 + 0.078125f;

            float v1  = v19 + 0.00055f;
            float u2  = u1 + 0.0615f;
            float v2  = v19 + 0.0121f;
            float u3  = u1;
            float v3  = v8 - 0.009f;
            float u4  = u2;
            float v4  = v8 + 0.0079f; // v8 - 0.0021 + 0.01
            CSprite2d::AddToBuffer(rt, RenderState.m_color, u1, v1, u2, v2, u3, v3, u4, v4);
        }
    }
}

// Tags processing
// 0x718F00
char* CFont::ParseToken(char* text, CRGBA& color, bool isBlip, char* tag) {
    // info about tokens: https://gtamods.com/wiki/GXT#Tokens

    char* next = ++text;

    auto ApplyStyle = [&](eHudColours hudColor) {
        if (!isBlip)
            color = HudColour.GetRGBA(hudColor, color.a);

        if (tag)
            *tag = *next;
    };

    switch (*next) {
    case '<':
        PS2Symbol = EXSYMBOL_DPAD_LEFT;
        break;
    case '>':
        PS2Symbol = EXSYMBOL_DPAD_RIGHT;
        break;
    case 'A':
    case 'a':
        PS2Symbol = EXSYMBOL_L3;
        break;
    case 'B':
    case 'b':
        ApplyStyle(HUD_COLOUR_DARK_BLUE);
        break;
    case 'C':
    case 'c':
        PS2Symbol = EXSYMBOL_R3;
        break;
    case 'D':
    case 'd':
        PS2Symbol = EXSYMBOL_DPAD_DOWN;
        break;
    case 'G':
    case 'g':
        ApplyStyle(HUD_COLOUR_GREEN);
        break;
    case 'H':
    case 'h':
        if (!isBlip) {
            color = {
                (uint8)std::min((float)color.r * 1.5f, 255.0f),
                (uint8)std::min((float)color.g * 1.5f, 255.0f),
                (uint8)std::min((float)color.b * 1.5f, 255.0f),
                color.a
            };
        }

        if (tag)
            *tag = *next;
        break;
    case 'J':
    case 'j':
        PS2Symbol = EXSYMBOL_R1;
        break;
    case 'K':
    case 'k':
        PS2Symbol = EXSYMBOL_KEY;
        break;
    case 'M':
    case 'm':
        PS2Symbol = EXSYMBOL_L2;
        break;
    case 'N':
    case 'n':
        m_bNewLine = true;
        break;
    case 'O':
    case 'o':
        PS2Symbol = EXSYMBOL_CIRCLE;
        break;
    case 'P':
    case 'p':
        ApplyStyle(HUD_COLOUR_PURPLE);
        break;
    case 'Q':
    case 'q':
        PS2Symbol = EXSYMBOL_SQUARE;
        break;
    case 'R':
    case 'r':
        ApplyStyle(HUD_COLOUR_RED);
        break;
    case 'S':
    case 's':
        ApplyStyle(HUD_COLOUR_LIGHT_GRAY);
        break;
    case 'T':
    case 't':
        PS2Symbol = EXSYMBOL_TRIANGLE;
        break;
    case 'U':
    case 'u':
        PS2Symbol = EXSYMBOL_DPAD_UP;
        break;
    case 'V':
    case 'v':
        PS2Symbol = EXSYMBOL_R2;
        break;
    case 'W':
    case 'w':
        ApplyStyle(HUD_COLOUR_LIGHT_GRAY);
        break;
    case 'X':
    case 'x':
        PS2Symbol = EXSYMBOL_CROSS;
        break;
    case 'Y':
    case 'y':
        ApplyStyle(HUD_COLOUR_CREAM);
        break;
    case 'l':
        ApplyStyle(HUD_COLOUR_BLACK);
        break;
    default:
        break;
    }

    if (*next != '~') {
        // skip text to the next '~' character.
        for(; *next && *next != '~'; next++);
    }

    if (*next)
        return next + 1;

    return next + 2;
}

// Text scaling
// 0x719380
void CFont::SetScale(float w, float h) {
    m_Scale.Set(w, h);
}

// Text scaling depends on current language
// 0x7193A0
void CFont::SetScaleForCurrentLanguage(float w, float h) {
    switch (FrontEndMenuManager.m_nPrefsLanguage) {
    case eLanguage::FRENCH:
    case eLanguage::GERMAN:
    case eLanguage::ITALIAN:
    case eLanguage::SPANISH:
        m_Scale.Set(w * 0.8f, h);
        break;
    default:
        m_Scale.Set(w, h);
    }
}

// Set text rotation point
// 0x719400
void CFont::SetSlantRefPoint(float x, float y) {
    m_fSlantRefPoint.Set(x, y);
}

// Set text rotation angle
// 0x719420
void CFont::SetSlant(float value) {
    m_fSlant = value;
}

// Set text color
// 0x719430
void CFont::SetColor(CRGBA color) {
    m_Color = color;

    if (m_fFontAlpha < 255.0f) {
        m_Color.a = (uint8)(float(color.a) * m_fFontAlpha / 255.0f);
    }
}

// Set text style
// 0x719490
void CFont::SetFontStyle(eFontStyle style) {
    switch (style) {
    case eFontStyle::FONT_PRICEDOWN:
        m_FontTextureId = 1;
        m_FontStyle = 1;
        break;
    case eFontStyle::FONT_MENU:
        m_FontTextureId = 0;
        m_FontStyle = 2;
        break;
    default:
        m_FontTextureId = static_cast<uint8>(style);
        m_FontStyle = 0;
    }
}

// Set line width at right
// 0x7194D0
void CFont::SetWrapx(float value) {
    m_fWrapx = value;
}

// Set line width at center
// 0x7194E0
void CFont::SetCentreSize(float value) {
    m_fFontCentreSize = value;
}

// 0x7194F0
void CFont::SetRightJustifyWrap(float value) {
    m_fRightJustifyWrap = value;
}

// Like a 'global' font alpha, multiplied with each text alpha (from SetColor)
// 0x719500
void CFont::SetAlphaFade(float alpha) {
    m_fFontAlpha = alpha;
}

// Drop color is used for text shadow and text outline
// 0x719510
void CFont::SetDropColor(CRGBA color) {
    m_FontDropColor = color;

    if (m_fFontAlpha < 255.0f) {
        m_FontDropColor.a = (uint8)(float(m_Color.a) * m_fFontAlpha);
    }
}

// Set shadow size
// 0x719570
void CFont::SetDropShadowPosition(int16 value) {
    m_nFontOutlineSize = 0;
    m_nFontOutlineOrShadow = 0;
    m_nFontShadow = (uint8)value;
}

// Set outline size
// 0x719590
void CFont::SetEdge(int8 value) {
    m_nFontShadow = 0;
    m_nFontOutlineSize = value;
    m_nFontOutlineOrShadow = value;
}

// Toggles character proportions in text
// 0x7195B0
void CFont::SetProportional(bool on) {
    m_bFontPropOn = on;
}

// Setups text background
// 0x7195C0
void CFont::SetBackground(bool enable, bool includeWrap) {
    m_bFontBackground = enable;
    m_bEnlargeBackgroundBox = includeWrap;
}

// Sets background color
// 0x7195E0
void CFont::SetBackgroundColor(CRGBA color) {
    m_FontBackgroundColor = color;
}

// 0x719600
void CFont::SetJustify(bool on) {
    m_bFontJustify = on;
}

// 0x719610
void CFont::SetOrientation(eFontAlignment alignment) {
    m_bFontCentreAlign = alignment == eFontAlignment::ALIGN_CENTER;
    m_bFontRightAlign = alignment == eFontAlignment::ALIGN_RIGHT;
}

// Need to call this each frame
// 0x719800
// PR#1414: единственная правка — pEmptyChar/FontRenderStateBuf заменены клеем
// (s_RenderEndPtr = s_RenderFontBuffer.GetTop(); та же запись 0xC716B0 в 0xC716A8).
void CFont::InitPerFrame() {
    ZoneScoped;

    m_nFontOutline = 0;
    m_nFontOutlineOrShadow = 0;
    m_nFontShadow = 0;
    m_bNewLine = false;
    PS2Symbol = EXSYMBOL_NONE;
    RenderState.m_wFontTexture = 0; // todo: -1
    s_RenderEndPtr = s_RenderFontBuffer.GetTop();

    CSprite::InitSpriteBuffer();
}

// Draw text we have in buffer
// 0x719840
// PR#1414. Адаптация к репо-хедеру: GetText() -> FontCharGetText(),
// Set(const CFontChar*) -> Set(const CFontChar&), m_nFontStyle:
// eFontTextureStyle::SUBTITLES_AND_GOTHIC -> 0, Slope/SlopeRef ->
// m_fSlant/m_vSlanRefPoint, ParseToken -> ParseTokenConst (const-correct).
void CFont::RenderFontBuffer() {
    if (s_RenderFontBuffer.GetTop() == s_RenderEndPtr) {
        // nothing to render
        return;
    }

    Sprite[RenderState.m_wFontTexture].SetRenderState();
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));

    RenderState.Set(*s_RenderFontBuffer.GetTop());

    CRGBA col = RenderState.m_color;
    float x   = RenderState.m_vPosn.x;
    float y   = RenderState.m_vPosn.y;

    const auto* textPtr = FontCharGetText(s_RenderFontBuffer.GetTop());
    while (textPtr < s_RenderEndPtr) {
        if (*textPtr == '\0') {
            CFontChar* nextState = s_RenderFontBuffer.FindNext(textPtr);

            if (nextState >= s_RenderEndPtr) {
                break;
            }

            RenderState.Set(*nextState);
            y   = RenderState.m_vPosn.y;
            x   = RenderState.m_vPosn.x;
            col = RenderState.m_color;

            textPtr = FontCharGetText(nextState);
            continue;
        }

        PS2Symbol = EXSYMBOL_NONE;

        while (*textPtr == '~' && PS2Symbol == EXSYMBOL_NONE) {
            textPtr = ParseTokenConst(textPtr, col, RenderState.m_bContainImages, nullptr);
            if (!RenderState.m_bContainImages) {
                RenderState.m_color = col;
            }
        }

        // ASCII offset usually starts at space (32)
        uint8 letter = *textPtr - 32;
        auto charID = letter;
        if (RenderState.m_nFontStyle != 0 /* eFontTextureStyle::SUBTITLES_AND_GOTHIC */) {
            charID = FindSubFontCharacter(letter, RenderState.m_nFontStyle);
            letter = charID;
        } else {
            if (letter == 145) {
                letter = 64;
            } else if (letter > 155) {
                letter = 0;
            }
        }

        if (RenderState.m_fSlant != 0.0f) {
            y = (RenderState.m_vSlanRefPoint.x - x) * RenderState.m_fSlant + RenderState.m_vSlanRefPoint.y;
        }

        if (PS2Symbol == EXSYMBOL_NONE || !RenderState.m_bContainImages) {
            PrintChar(x, y, letter);
            charID = letter;
        }

        if (PS2Symbol != EXSYMBOL_NONE) {
            x += (RenderState.m_fHeight * 17.0f) + RenderState.m_nOutline;
        } else {
            const auto charWidth = GetLetterIdPropValue(charID);
            x += (charWidth + RenderState.m_nOutline) * RenderState.m_fWidth;
        }

        // apply wrap spacing if invalid
        if (!charID) {
            x += RenderState.m_fWrap;
        }

        if (*textPtr) {
            if (PS2Symbol != EXSYMBOL_NONE) {
                PS2Symbol = EXSYMBOL_NONE;
                Sprite[RenderState.m_wFontTexture].SetRenderState();
            } else {
                textPtr++;
            }
        } else if (PS2Symbol != EXSYMBOL_NONE) {
            PS2Symbol = EXSYMBOL_NONE;
            Sprite[RenderState.m_wFontTexture].SetRenderState();
        }
    }

    CSprite::FlushSpriteBuffer();
    CSprite2d::RenderVertexBuffer();

    // reset for the next frame
    s_RenderEndPtr = s_RenderFontBuffer.GetTop();
}

// 0x71A0E0
float CFont::GetStringWidth(const GxtChar* string, bool full, bool scriptText) {
    size_t len = CMessages::GetStringLength(string);
    GxtChar data[400] = { 0 };

    strncpy_s((char*)data, sizeof(data), AsciiFromGxtChar(string), len);
    CMessages::InsertPlayerControlKeysInString(data);

    float width = 0.0f;
    bool lastWasTag = false, lastWasLetter = false;
    auto* pStr = data;

    while (true) {
        if (*pStr == ' ' && !full)
            break;
        if (*pStr == '\0')
            break;

        if (*pStr == '~') {
            if (!full && (lastWasTag || lastWasLetter))
                return width;

            auto* next = pStr + 1;

            if (*next != '~') {
                for (; *next && *next != '~'; next++);
            }

            pStr = next + 1;

            if (lastWasLetter || *pStr == '~')
                lastWasTag = true;
        }
        else {
            if (!full && *pStr == ' ' && lastWasTag)
                return width;

            char upper = *pStr - 0x20;

            pStr++;
            if (scriptText) {
                width += GetScriptLetterSize(upper);
            }
            else {
                width += GetCharacterSize(upper);
            }

            lastWasLetter = true;
        }
    }

    return width;
}

// same as RenderFontBuffer() (0x71A210)
void CFont::DrawFonts() {
    ZoneScoped;

    RenderFontBuffer();
}

// 0x71A220
// PR#1414. Адаптация к репо-хедеру: поля CFontDetails -> репо-статики по адресам
// (см. таблицу в шапке файла), ParseToken -> ParseTokenConst,
// ranged PrintString -> PrintString_Ranged.
int16 CFont::ProcessCurrentString(bool print, float x, float y, const GxtChar* text) {
    // state variables per each line
    bool           isFirstWordInLine{}; // we check this so if even the first word doesn't fit to the line we don't break infinitely.
    GxtChar        tag{};               // this makes using old tags (colors etc) work even if we line breaked in middle
    float          width{};             // running total width of line, including the char currently being processed.
    float          processedWidth{};    // same as `width` except doesn't include the current one
    const GxtChar* lineStart{};         // points to the first char to be printed at the new line
    int16          spaceCount{};        // for font justifing

    GxtChar        savedTagBuffer[256]{};
    const auto ResetForNextLine = [&]() {
        width             = (m_bFontCentreAlign || m_bFontRightAlign) ? 0.0f : x; // PR: m_Details.Centre || m_Details.RightJustify
        processedWidth    = 0.0f;
        isFirstWordInLine = true;
        tag               = 0;
        lineStart         = text;
        spaceCount        = 0;
    };
    ResetForNextLine();

    auto numLines{ 0 };
    while (*text) {
        PS2Symbol              = EXSYMBOL_NONE;
        const auto stringWidth = GetStringWidth(text, false, false);

        if (*text == '~') {
            CRGBA outColor{};
            text = ParseTokenConst(text, outColor, true, &tag); // PR: ParseToken(text, outColor, true, &tag)
        }

        const auto wrapLimit = m_bFontCentreAlign ? m_fFontCentreSize : (m_bFontRightAlign ? x - m_fRightJustifyWrap : m_fWrapx); // PR: m_Details.Centre/CentreX/RightJustify/RightJustifyWrap/WrapEnd

        const auto totalWidth = stringWidth + width;
        if ((totalWidth <= wrapLimit || isFirstWordInLine) && !m_bNewLine) {
            width = totalWidth;

            for (GxtChar ch = *text; ch != ' '; ch = *++text) {
                if (ch == '\0' || ch == '~') {
                    break;
                }
            }

            if (*text) {
                if (!isFirstWordInLine) {
                    spaceCount++;
                }
                if (*text != '~') {
                    text++;
                    width += GetCharacterSize('\0');
                }
                processedWidth    = width;
                isFirstWordInLine = false;
                continue;
            }

            const auto drawX = m_bFontCentreAlign ? x - width / 2.0f : (m_bFontRightAlign ? x - width : x);

            numLines++;
            if (print) {
                PrintString_Ranged(drawX, y, lineStart, text, 0.0f); // PR: PrintString(drawX, y, lineStart, text, 0.0f)
            }
            continue;
        }

        if (PS2Symbol) {
            text -= 3;
        }

        const GxtChar* printEnd = m_bNewLine ? text - 3 : text;

        float drawX{ x }, wrap{};
        if (m_bFontJustify && !m_bFontCentreAlign) { // PR: m_Details.Justify && !m_Details.Centre
            if (spaceCount > 0) {
                wrap = (m_fWrapx - processedWidth) / (float)spaceCount; // PR: m_Details.WrapEnd
            }
        }

        if (m_bFontCentreAlign) { // PR: m_Details.Centre
            drawX = x - width / 2.0f;
        } else if (m_bFontRightAlign) { // PR: m_Details.RightJustify
            drawX = x - (width - GetCharacterSize('\0'));
        }

        numLines++;
        if (print) {
            PrintString_Ranged(drawX, y, lineStart, printEnd, wrap); // PR: PrintString(drawX, y, lineStart, printEnd, wrap)
        }

        if (tag) {
            // read the saved tag from previous line to not forget its formatting

            savedTagBuffer[0] = '~';
            savedTagBuffer[1] = tag;
            savedTagBuffer[2] = '~';
            CMessages::StringCopy(savedTagBuffer + 3, text, std::size(savedTagBuffer) - 3);
            text = savedTagBuffer;
        }

        m_bNewLine = false;
        y += m_Scale.y * 18.0f; // PR: m_Details.Scale.y
        ResetForNextLine();
    }

    if (print) {
        SetColor(m_Color); // PR: SetColor(m_Details.Color)
    }
    return numLines;
}

// 0x71A5E0
int16 CFont::GetNumberLines(float x, float y, const GxtChar* text) {
    return ProcessCurrentString(false, x, y, text);
}

// 0x71A600
int16 CFont::ProcessStringToDisplay(float x, float y, const GxtChar* text) {
    return ProcessCurrentString(true, x, y, text);
}

// 0x71A620
void CFont::GetTextRect(CRect* rect, float x, float y, const GxtChar* text) {
    if (m_bFontCentreAlign) {
        rect->left = x - (m_fFontCentreSize / 2.0f + 4.0f);
        rect->right = m_fFontCentreSize / 2.0f + x + 4.0f;
    }
    else if (m_bFontRightAlign) {
        rect->left = m_fRightJustifyWrap - 4.0f;
        rect->right = x;
    }
    else {
        rect->left = x - 4.0f;
        rect->right = m_fWrapx + 4.0f;
    }

    rect->top = y - 4.0f;
    rect->bottom = y + 4.0f + GetHeight() * (float)GetNumberLines(x, y, text);
}

// 0x71A700
void CFont::PrintString(float x, float y, const GxtChar* text) {
    if (*text == '\0' || *text == '*')
        return;

    if (m_bFontBackground) {
        CRect rt;

        RenderState.m_color = m_Color;
        GetTextRect(&rt, x, y, text);

        if (m_bEnlargeBackgroundBox) {
            rt.left -= 1.0f;
            rt.right += 1.0f;
            rt.bottom += 1.0f;
            rt.top -= 1.0f;

            FrontEndMenuManager.DrawWindow(rt, nullptr, 0, m_FontBackgroundColor, false, true);
        } else {
            CSprite2d::DrawRect(rt, m_FontBackgroundColor);
        }
        m_bFontBackground = false;
    }

    ProcessStringToDisplay(x, y, text);
}

// 0x719B40
// PR#1414: ranged-перегрузка CFont::PrintString. В репо-хедере объявления нет
// (Font.h не трогаем), поэтому функция file-scope — все статики CFont доступны,
// они public (Font.h:86-122), но обязаны быть квалифицированы CFont::.
// Адаптация полей — по адресам CFontDetails (см. шапку); особое соответствие:
// m_Details.Shadow == m_bFontIsBlip (0xC71A7E).
void PrintString_Ranged(float x, float y, const GxtChar* start, const GxtChar* end, float wrap) {
    auto savedColor = CFont::m_Color; // PR: m_Details.Color

    if (CFont::RenderState.m_wFontTexture != CFont::m_FontTextureId) { // PR: != +m_Details.FontTextureName
        // flush if texture's changed
        CFont::RenderFontBuffer();
        CFont::RenderState.m_wFontTexture = CFont::m_FontTextureId; // PR: = +m_Details.FontTextureName
    }

    // Handle outline
    if (!CFont::m_nFontShadow) { // PR: !m_Details.DropAmount
        if (CFont::m_nFontOutlineSize) { // PR: m_Details.EdgeAmount
            CFont::m_Color = CFont::m_FontDropColor; // PR: m_Details.Color = m_Details.DropColor
            CFont::m_bFontIsBlip = true;             // PR: m_Details.Shadow = true
            const auto savedOutline = std::exchange(CFont::m_nFontOutlineSize, uint8{ 0 }); // PR: std::exchange(m_Details.EdgeAmount, 0)
            const auto offsetX = SCREEN_STRETCH_X(savedOutline), offsetY = SCREEN_STRETCH_Y(savedOutline);

            if (CFont::m_fSlant != 0.0f) { // PR: m_Details.Slope
                CFont::m_fSlantRefPoint.x += offsetX; // PR: m_Details.SlopeRef.x
                CFont::m_fSlantRefPoint.y += offsetY;
            }

            PrintString_Ranged(x + offsetX, y - offsetY, start, end, wrap);
            PrintString_Ranged(x - offsetX, y - offsetY, start, end, wrap);
            PrintString_Ranged(x + offsetX, y + offsetY, start, end, wrap);
            PrintString_Ranged(x - offsetX, y + offsetY, start, end, wrap);
            PrintString_Ranged(x + offsetX, y, start, end, wrap);
            PrintString_Ranged(x - offsetX, y, start, end, wrap);
            PrintString_Ranged(x, y + offsetY, start, end, wrap);
            PrintString_Ranged(x, y - offsetY, start, end, wrap);

            CFont::m_Color.r          = savedColor.r;
            CFont::m_Color.g          = savedColor.g;
            CFont::m_Color.b          = savedColor.b;
            CFont::m_nFontOutlineSize = savedOutline; // PR: m_Details.EdgeAmount
            CFont::m_bFontIsBlip      = false;        // PR: m_Details.Shadow = false
        }
    } else {
        // Handle shadow
        CFont::m_Color = CFont::m_FontDropColor; // PR: m_Details.Color = m_Details.DropColor
        CFont::m_bFontIsBlip = true;             // PR: m_Details.Shadow = true
        const auto savedShadow = std::exchange(CFont::m_nFontShadow, uint8{ 0 }); // PR: std::exchange(m_Details.DropAmount, 0)
        const auto offsetX = SCREEN_STRETCH_X(savedShadow), offsetY = SCREEN_STRETCH_Y(savedShadow);

        if (CFont::m_fSlant != 0.0f) { // PR: m_Details.Slope
            CFont::m_fSlantRefPoint.x += offsetX;
            CFont::m_fSlantRefPoint.y += offsetY;
        }

        PrintString_Ranged(x + offsetX, y + offsetY, start, end, wrap);

        CFont::m_Color.r     = savedColor.r;
        CFont::m_Color.g     = savedColor.g;
        CFont::m_Color.b     = savedColor.b;
        CFont::m_nFontShadow = savedShadow; // PR: m_Details.DropAmount
        CFont::m_bFontIsBlip = false;       // PR: m_Details.Shadow = false
    }

    if (s_RenderEndPtr >= reinterpret_cast<uint8*>(&s_RenderFontBuffer) + 460 - (end - start)) {
        // if it's not gonna fit we gotta flush
        CFont::RenderFontBuffer();
    }

    // Set up the next character to be rendered
    auto* next             = reinterpret_cast<CFontChar*>(s_RenderEndPtr);
    next->m_fWidth         = CFont::m_Scale.x;      // PR: m_Details.Scale.x
    next->m_fHeight        = CFont::m_Scale.y;      // PR: m_Details.Scale.y
    next->m_color          = CFont::m_Color;        // PR: m_Details.Color
    next->m_fWrap          = wrap;
    next->m_fSlant         = CFont::m_fSlant;       // PR: next->Slope = m_Details.Slope
    next->m_vSlanRefPoint  = CFont::m_fSlantRefPoint; // PR: next->SlopeRef = m_Details.SlopeRef
    next->m_nFontStyle     = CFont::m_FontStyle;    // PR: m_Details.FontTextureStyle
    next->m_bPropOn        = CFont::m_bFontPropOn;  // PR: m_Details.Proportional
    next->m_wFontTexture   = CFont::m_FontTextureId; // PR: +m_Details.FontTextureName // TODO
    next->m_bContainImages = CFont::m_bFontIsBlip;   // PR: m_Details.Shadow
    next->m_nOutline       = CFont::m_nFontOutline; // PR: m_Details.EdgeSpace (0xC71A9C)
    next->m_vPosn.Set(x, y);

    auto& renderEnd = reinterpret_cast<uint8*&>(s_RenderEndPtr);
    renderEnd += sizeof(CFontChar);

    while (start < end) {
        if (*start == '~') {
            // saved color can change here
            auto* tokenEnd = ParseTokenConst(start, savedColor, CFont::RenderState.m_bContainImages, nullptr); // PR: ParseToken(start, savedColor, RenderState.m_bContainImages, nullptr)

            if (!CFont::m_bFontIsBlip && CFont::PS2Symbol == EXSYMBOL_NONE) { // PR: !m_Details.Shadow
                CFont::m_Color.r = savedColor.r;
                CFont::m_Color.g = savedColor.g;
                CFont::m_Color.b = savedColor.b;
            }
            CFont::PS2Symbol = EXSYMBOL_NONE;

            while (start != tokenEnd) {
                *renderEnd++ = *start++;
            }
        } else {
            *renderEnd++ = *start++;
        }
    }

    // null terminate the end
    *renderEnd = '\0';

    // align to 4 bytes
    auto* alignPtr = renderEnd + 1;
    while (((uintptr)alignPtr & 3) != 0) {
        alignPtr++;
    }
    s_RenderEndPtr = alignPtr;

    if (!CFont::m_bFontIsBlip) { // PR: !m_Details.Shadow
        CFont::m_Color = savedColor; // PR: m_Details.Color
    }
}

// 0x71A820
void CFont::PrintStringFromBottom(float x, float y, const GxtChar* text) {
    float drawY = y - GetHeight() * (float)GetNumberLines(x, y, text);

    if (m_fSlant != 0.0f)
        drawY -= (m_fSlantRefPoint.x - x) * m_fSlant + m_fSlantRefPoint.y;

    PrintString(x, drawY, text);
}

// 0x719750
float CFont::GetCharacterSize(uint8 letterId) {
    uint8 propValueIdx = letterId;

    if (letterId == '?') {
        letterId = 0;
        propValueIdx = 0;
    }

    if (m_FontStyle)
        propValueIdx = FindSubFontCharacter(letterId, m_FontStyle);
    else if (propValueIdx == 145)
        propValueIdx = '@';
    else if (propValueIdx > 155)
        propValueIdx = 0;

    if (m_bFontPropOn) {
        return ((float)gFontData[m_FontTextureId].m_propValues[propValueIdx] + (float)m_nFontOutlineSize) * m_Scale.x;
    } else {
        return ((float)gFontData[m_FontTextureId].m_unpropValue + (float)m_nFontOutlineSize) * m_Scale.x;
    }
}

// Android
float CFont::GetHeight(bool a1) {
    assert(a1 == false && "NOT IMPLEMENTED");
    const float y = a1 ? 0.0f : m_Scale.y;
    return y * 32.0f / 2.0f + y + y;
}

// 0x719670, original name unknown
// PR#1414. Адаптация к репо-хедеру: FindSubFontCharacter(uint8, uint8) —
// eFontTextureStyle::MENU -> 2, eFontTextureStyle::PRICEDOWN -> 1.
// ВАЖНО: switch по tScriptText::FontStyle (eFontStyleS32, NOTSA_WENUM_DEFS_FOR(eFontStyle),
// Font.h:84) идёт по ЗНАЧЕНИЯМ eFontStyle (FONT_MENU=2, FONT_PRICEDOWN=3),
// а FindSubFontCharacter принимает стиль текстуры (0/1/2) — маппинг из PR сохранён.
float GetScriptLetterSize(uint8 letterId) {
    letterId = (letterId != '?') ? letterId : 0;
    const auto i = CTheScripts::NumberOfIntroTextLinesThisFrame;

    const auto style = CTheScripts::IntroTextLines[i].FontStyle;
    const auto subFontChar = [letterId, style] {
        switch (style) {
        case FONT_MENU:
            return CFont::FindSubFontCharacter(letterId, 2 /* eFontTextureStyle::MENU */);
        case FONT_PRICEDOWN:
            return CFont::FindSubFontCharacter(letterId, 1 /* eFontTextureStyle::PRICEDOWN */);
        default:
            break;
        }

        if (letterId == 145) {
            return (uint8)'@';
        } else if (letterId > 155) {
            return (uint8)0;
        } else {
            return letterId;
        }
    }();

    const auto fontWidth = GetLetterIdPropValue(subFontChar, CTheScripts::IntroTextLines[i].IsProportional, style);
    return CTheScripts::IntroTextLines[i].Scale.x * (fontWidth + CTheScripts::IntroTextLines[i].TextEdge);
}

// 0x7192C0
// НЕ из батча: тело репо сохранено как есть. PR менял `letterId - 128` (строка
// ниже) на `letterId + 128` (кандидат-фикс, pr1414_notes.md «Однострочные фиксы»)
// и тип параметра на eFontTextureStyle — сюда НЕ перенесено.
uint8 CFont::FindSubFontCharacter(uint8 letterId, uint8 fontStyle) {
    if (fontStyle == 1) { // eFontStyle::FONT_PRICEDOWN
        switch (letterId) {
        case 1:  return 208;
        case 4:  return 93;
        case 7:  return 206;
        case 8:
        case 9:  return letterId + 86;
        case 14: return 207;
        case 26: return 154;
        }
    }

    if (letterId == 6)                      return 10;
    if (letterId >=  16 && letterId <=  25) return letterId - 128;
    if (letterId == 31)                     return 91;
    if (letterId >=  33 && letterId <=  58) return letterId + 122;
    if (letterId == 62)                     return 32;
    if (letterId >=  65 && letterId <=  90) return letterId + 90;
    if (letterId >=  96 && letterId <= 118) return letterId + 85;
    if (letterId >= 119 && letterId <= 140) return letterId + 62;
    if (letterId >= 141 && letterId <= 142) return 204;
    if (letterId == 143)                    return 205;

    return letterId;
}

// 0x718770
// НЕ из батча: тело репо (PR делегировал в 3-арговый хелпер — хелпер добавлен
// в file-scope блоке вверху файла, нужен GetScriptLetterSize).
float GetLetterIdPropValue(uint8 letterId) {
    uint8 id = letterId;

    if (letterId == '?')
        id = 0;

    if (CFont::RenderState.m_bPropOn)
        return gFontData[CFont::RenderState.m_wFontTexture].m_propValues[id];
    else
        return gFontData[CFont::RenderState.m_wFontTexture].m_unpropValue;
}
