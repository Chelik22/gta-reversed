/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "Font.h"

#include "eLanguage.h"

auto& FontRenderStateBuf = StaticRef<std::array<CFontChar, 9>>(0xC716B0);
auto& pEmptyChar = StaticRef<CFontChar*>(0xC716A8);

auto& gFontData = StaticRef<std::array<tFontData, 2>>(0xC718B0);

void CFont::InjectHooks() {
    RH_ScopedClass(CFont);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Initialise, 0x5BA690);
    RH_ScopedInstall(Shutdown, 0x7189B0);
    RH_ScopedInstall(PrintChar, 0x718A10);
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
    RH_ScopedInstall(RenderFontBuffer, 0x719840);
    RH_ScopedInstall(GetTextBuffer, 0x719B40);
    RH_ScopedInstall(GetStringWidth, 0x71A0E0);
    RH_ScopedInstall(DrawFonts, 0x71A210);
    RH_ScopedInstall(ProcessCurrentString, 0x71A220);
    RH_ScopedInstall(GetNumberLines, 0x71A5E0);
    RH_ScopedInstall(ProcessStringToDisplay, 0x71A600);
    RH_ScopedInstall(GetTextRect, 0x71A620);
    RH_ScopedInstall(PrintString, 0x71A700);
    RH_ScopedInstall(PrintStringFromBottom, 0x71A820);
    RH_ScopedInstall(GetCharacterSize, 0x719750);
    RH_ScopedInstall(LoadFontValues, 0x7187C0);
    // Install("", "GetScriptLetterSize", 0x719670, &GetScriptLetterSize);
    RH_ScopedInstall(FindSubFontCharacter, 0x7192C0);
    RH_ScopedGlobalInstall(GetLetterIdPropValue, 0x718770);
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
void CFont::PrintChar(float x, float y, char character) {
    // NOTE: All "magic" UV constants below come from data addresses in the original binary:
    //  1/16 = 0x858620, 0.0 = 0x872928, 0.0021 = 0x872920, 0.001 = 0x858CDC, 0.078125 = 0x8652C8,
    //  0.016 = 0x863C00, 0.015 = 0x859F00, 0.00055 = 0x8717B0, 0.0121 = 0x87291C, 0.009 = 0x859048,
    //  0.01 = 0x858C58, 0.0001 = 0x858FC4, 0.03125 = 0x864E10, 32.0 = 0x85950C, 40.0 = 0x858A10, 0.5 = 0x858B8C

    // NOTE: Original checks bounds against RsGlobal.maximumWidth/maximumHeight, which are
    // RwInt32's (original code converts them to float with `fild`). Reading them as floats
    // (reinterpreting the int bit pattern) yields a denormal ~0.0f and culled every glyph.
    const float screenHeight = (float)RsGlobal.maximumHeight;
    const float screenWidth  = (float)RsGlobal.maximumWidth;

    // out of screen
    if (y < 0.0f || y > screenHeight || x < 0.0f || x > screenWidth)
        return;

    if (PS2Symbol) {
        // extra symbol to be drawn (e.g. PS2 buttons)
        const CRect rt{
            x,
            2.0f * RenderState.m_fHeight + y,  // bottom
            17.0f * RenderState.m_fHeight + x, // right
            19.0f * RenderState.m_fHeight + y  // top
        };
        ButtonSprite[PS2Symbol].Draw(rt, { 255, 255, 255, RenderState.m_color.a });
        return;
    }

    uint8 ch = (uint8)character;
    bool  zeroed = false;
    if (ch == 0 || ch == '?') { // space / invalid character
        ch = 0;
        zeroed = true;
    }

    const float glyphProp = GetLetterIdPropValue(ch); // glyph width in font units (only used for japanese fonts)

    if (RenderState.m_nFontStyle == 1 && ch == 208) // "d0" glyph of FONT_PRICEDOWN is drawn empty
        ch = 0;

    const float u1 = (ch & 0xF) / 16.0f;

    if (RenderState.m_wFontTexture == 0 || RenderState.m_wFontTexture == 1) {
        // original: FMUL qword ptr [0x872928] - a 64-bit DOUBLE 0.078125 (the low dword at that
        // address is the float 0.0, which is why this was previously misread as `* 0.0f`).
        // 0.078125 is the atlas row V-stride for fonts 0/1 (20px glyph rows in a 256px texture).
        const float v = (ch >> 4) * 0.078125f;

        if (zeroed)
            return;

        const float u2 = (u1 + 1.0f / 16.0f) - 0.001f;

        if (RenderState.m_fSlant == 0.0f) {
            if (ch < 0xC0) {
                const CRect rt{
                    x,
                    y,                                              // top
                    32.0f * RenderState.m_fWidth + x,               // right
                    RenderState.m_fHeight * 40.0f * 0.5f + y        // bottom
                };
                const float vHigh = (v + 0.078125f) - 0.0021f;
                CSprite2d::AddToBuffer(rt, RenderState.m_color, u1, v + 0.0021f, u2, v + 0.0021f, u1, vHigh, u2, vHigh);
            } else {
                const CRect rt{
                    x,
                    y,                                              // top
                    32.0f * RenderState.m_fWidth + x,               // right
                    RenderState.m_fHeight * 32.0f * 0.5f + y        // bottom
                };
                CSprite2d::AddToBuffer(rt, RenderState.m_color, u1, v + 0.0021f, u2, v + 0.0021f, u1, (v + 0.078125f) - 0.016f, u2, (v + 0.078125f) - 0.015f);
            }
        } else { // slanted text
            const CRect rt{
                x,
                y + 0.015f,                                         // top
                32.0f * RenderState.m_fWidth + x,                   // right
                RenderState.m_fHeight * 40.0f * 0.5f + y + 0.015f   // bottom
            };
            const float vHigh = (v + 0.078125f) - 0.0021f;
            CSprite2d::AddToBuffer(rt, RenderState.m_color, u1, v + 0.00055f, u2, v + 0.0121f, u1, (v + 0.078125f) - 0.009f, u2, vHigh + 0.01f);
        }
    } else { // japanese (and other) fonts - glyph width taken from the font's prop values
        if (zeroed)
            return;

        const float v  = (ch >> 4) / 16.0f;
        const float u2 = glyphProp * 0.03125f * (1.0f / 16.0f) + u1;

        const CRect rt{
            x,
            y,                                                          // bottom
            32.0f * RenderState.m_fWidth * (glyphProp * 0.03125f) + x,  // right
            RenderState.m_fHeight * 32.0f * 0.5f + y                    // top (0x85950C = 32.0f, not 40.0f)
        };
        // NOTE: instruction-faithful - the original pushes y (not u1) as the 1st/5th float here.
        CSprite2d::AddToBuffer(rt, RenderState.m_color, y, v, u2, v, y, v + 1.0f / 16.0f, u2 - 0.0001f, (v + 1.0f / 16.0f) - 0.0001f);
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
void CFont::InitPerFrame() {
    ZoneScoped;

    m_nFontOutlineSize = 0;
    m_nFontOutline = 0;
    m_nFontOutlineOrShadow = 0;
    m_nFontShadow = 0;
    m_bNewLine = false;
    PS2Symbol = EXSYMBOL_NONE;
    RenderState.m_wFontTexture = 0xFFFF; // original sets the word at 0xC71ACC to -1 (sentinel "no font yet")
    pEmptyChar = &FontRenderStateBuf[0]; // FontRenderStatePointer.pRenderState

    CSprite::InitSpriteBuffer();
}

// Draw text we have in buffer
// 0x719840
// Walks `FontRenderStateBuf` and draws every buffered line: the buffer is a sequence of
// [CFontChar record (0x30 bytes)][line text][NUL][pad to 4] entries. Each record is copied
// into `RenderState` (0x718E50) and provides the line's start position, colour, scale,
// slant, style etc.; the text that follows is processed character by character, re-parsing
// '~' tokens via `ParseToken` (colours are restored into `RenderState.m_color` unless
// `RenderState.m_bContainImages`), resolving the letter id via `FindSubFontCharacter`
// (or the 145/'@' substitution when `m_nFontStyle == 0`), calling `PrintChar`, and
// advancing x by the glyph's prop value (+`m_fWrap` extra width for spaces, which is how
// justified text is stretched). After a button token ('~k~' etc.) `PrintChar` draws the
// button sprite instead of the glyph; the texture render state is then re-set and the same
// character is processed again so the glyph is drawn too (PS2Symbol is cleared first).
void CFont::RenderFontBuffer() {
    auto* const bufStart = reinterpret_cast<uint8_t*>(&FontRenderStateBuf[0]); // 0xC716B0
    auto* const bufEnd = reinterpret_cast<uint8_t*>(pEmptyChar);

    if (bufEnd == bufStart) // nothing buffered
        return;

    // Re-set the font texture (button sprites drawn by `PrintChar` changed it).
    // Original re-reads `RenderState.m_wFontTexture` (0xC71ACC) on every call
    // (0x719853 initial, 0x719AFE in-loop), not just once.
    const auto SetFontTexture = [&]() {
        const int16_t texId = static_cast<int16_t>(RenderState.m_wFontTexture);
        if (texId >= 0 && texId < MAX_FONT_SPRITES)
            Sprite[texId].SetRenderState();
    };
    SetFontTexture();

    // Original: RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, TRUE) via the engine
    // instance (0xC97B24): `PUSH 1; PUSH 0xC` at 0x71986C - state 0xC is
    // rwRENDERSTATEVERTEXALPHAENABLE, *not* rwRENDERSTATEDESTBLEND (11). This matters:
    // in the frontend `CSprite2d::DrawRect` leaves vertex alpha disabled after drawing
    // the menu background, so without this the buffered text renders unblended
    // (opaque glyph tiles) and effectively disappears.
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));

    RenderState.Set(*reinterpret_cast<CFontChar*>(bufStart)); // 0x718E50, record at 0xC716B0
    float drawX = RenderState.m_vPosn.x;
    float drawY = RenderState.m_vPosn.y;

    for (uint8_t* textPtr = bufStart + sizeof(CFontChar); textPtr < bufEnd;) { // 0xC716E0: text of record 0
        if (*textPtr == '\0') {
            // End of this line's text: skip the terminator padding to the next record.
            do { ++textPtr; } while (reinterpret_cast<uintptr_t>(textPtr) % 4 != 0);
            if (textPtr >= bufEnd)
                break;
            RenderState.Set(*reinterpret_cast<CFontChar*>(textPtr));
            drawX = RenderState.m_vPosn.x;
            drawY = RenderState.m_vPosn.y;
            textPtr += sizeof(CFontChar); // the record is followed by its text
            continue;
        }

        // Process the character at `textPtr`, re-parsing any '~' tokens first.
        PS2Symbol = EXSYMBOL_NONE;
        CRGBA color = RenderState.m_color;
        while (*textPtr == '~' && PS2Symbol == EXSYMBOL_NONE) {
            textPtr = reinterpret_cast<uint8_t*>(
                ParseToken(reinterpret_cast<char*>(textPtr), color, RenderState.m_bContainImages, nullptr));
            if (!RenderState.m_bContainImages)
                RenderState.m_color = color;
        }

        uint8_t letter = static_cast<uint8_t>(*textPtr - 0x20);
        if (RenderState.m_nFontStyle != 0) {
            letter = FindSubFontCharacter(letter, RenderState.m_nFontStyle);
        } else if (letter == 0x91) {
            letter = 0x40; // '@'
        } else if (letter > 0x9B) {
            letter = 0;
        }

        if (RenderState.m_fSlant != 0.0f)
            drawY = RenderState.m_vSlanRefPoint.y + (RenderState.m_vSlanRefPoint.x - drawX) * RenderState.m_fSlant;

        // With a pending button symbol and `m_bContainImages` set, `PrintChar` is skipped
        // entirely; otherwise it draws the button sprite instead of the glyph (PS2Symbol path).
        if (PS2Symbol == EXSYMBOL_NONE || !RenderState.m_bContainImages)
            PrintChar(drawX, drawY, static_cast<char>(letter));

        if (PS2Symbol != EXSYMBOL_NONE) {
            drawX += RenderState.m_fHeight * 17.0f + static_cast<float>(static_cast<int8_t>(RenderState.m_nOutline));
        } else {
            const uint8_t glyph = letter == '?' ? uint8_t{ 0 } : letter;
            const float w = RenderState.m_bPropOn
                ? static_cast<float>(gFontData[RenderState.m_wFontTexture].m_propValues[glyph])
                : static_cast<float>(gFontData[RenderState.m_wFontTexture].m_unpropValue);
            drawX += (w + static_cast<float>(static_cast<int8_t>(RenderState.m_nOutline))) * RenderState.m_fWidth;
        }
        if (letter == 0) // space: extra per-space width (justify stretch), stored per record
            drawX += RenderState.m_fWrap;

        if (*textPtr != '\0') {
            if (PS2Symbol == EXSYMBOL_NONE) {
                ++textPtr;
            } else {
                // Button was drawn for this position; clear the symbol, restore the font
                // texture and process the same character again (now as a glyph).
                PS2Symbol = EXSYMBOL_NONE;
                SetFontTexture();
            }
        } else if (PS2Symbol != EXSYMBOL_NONE) {
            PS2Symbol = EXSYMBOL_NONE;
            SetFontTexture();
        }
    }

    CSprite::FlushSpriteBuffer();
    CSprite2d::RenderVertexBuffer();
    pEmptyChar = &FontRenderStateBuf[0];
}

// 0x719B40
// Appends one already-wrapped line of text to the render buffer (see `RenderFontBuffer`
// for the buffer layout). Drops a drop-shadow / outline copy first by recursing with an
// offset position, the drop colour and tokens/colors suppressed (`m_bFontIsBlip`).
void CFont::GetTextBuffer(float x, float y, const char* lineStart, const char* lineEnd, float extraSpaceWidth) {
    // Flush the buffer when the selected font changed since it was filled.
    if (RenderState.m_wFontTexture != m_FontTextureId) {
        RenderFontBuffer();
        RenderState.m_wFontTexture = m_FontTextureId;
    }

    const float dxScale = static_cast<float>(RsGlobal.maximumWidth) * (1.0f / 640.0f);  // [0x859520]
    const float dyScale = static_cast<float>(RsGlobal.maximumHeight) * (1.0f / 448.0f); // [0x859524]

    // Drop shadow: single offset copy (0x719BFC).
    if (m_nFontShadow != 0) {
        const int8_t offset = static_cast<int8_t>(m_nFontShadow);
        const CRGBA color = m_Color;

        m_Color = m_FontDropColor;
        m_nFontShadow = 0;
        m_bFontIsBlip = true;
        if (m_fSlant == 0.0f) { // slant ref-point compensation (only relevant when slant is applied later)
            m_fSlantRefPoint.x += static_cast<float>(offset) * dxScale;
            m_fSlantRefPoint.y += static_cast<float>(offset) * dyScale;
        }
        GetTextBuffer(x + static_cast<float>(offset) * dxScale, y + static_cast<float>(offset) * dyScale, lineStart, lineEnd, extraSpaceWidth);

        m_Color = color;
        m_nFontShadow = static_cast<uint8_t>(offset);
        m_bFontIsBlip = false;
    }
    // Outline: eight offset copies (0x719CA3).
    else if (m_nFontOutlineSize != 0) {
        const int8_t offset = static_cast<int8_t>(m_nFontOutlineSize);
        const CRGBA color = m_Color;

        m_Color = m_FontDropColor;
        m_nFontOutlineSize = 0;
        m_bFontIsBlip = true;
        if (m_fSlant == 0.0f) {
            m_fSlantRefPoint.x += static_cast<float>(offset) * dxScale;
            m_fSlantRefPoint.y += static_cast<float>(offset) * dyScale;
        }
        const float dx = static_cast<float>(offset) * dxScale;
        const float dy = static_cast<float>(offset) * dyScale;
        GetTextBuffer(x + dx, y - dy, lineStart, lineEnd, extraSpaceWidth);
        GetTextBuffer(x - dx, y - dy, lineStart, lineEnd, extraSpaceWidth);
        GetTextBuffer(x + dx, y + dy, lineStart, lineEnd, extraSpaceWidth);
        GetTextBuffer(x - dx, y + dy, lineStart, lineEnd, extraSpaceWidth);
        GetTextBuffer(x + dx, y, lineStart, lineEnd, extraSpaceWidth);
        GetTextBuffer(x - dx, y, lineStart, lineEnd, extraSpaceWidth);
        GetTextBuffer(x, y + dy, lineStart, lineEnd, extraSpaceWidth);
        GetTextBuffer(x, y - dy, lineStart, lineEnd, extraSpaceWidth);

        m_Color = color;
        m_nFontOutlineSize = static_cast<uint8_t>(offset);
        m_bFontIsBlip = false;
    }

    // Append the line to the render buffer (0x719F30). Flush first when the remaining
    // buffer space does not cover the remaining text (plus record/terminator headroom).
    auto* appendPtr = reinterpret_cast<uint8_t*>(pEmptyChar);
    constexpr size_t BUFFER_CAPACITY = 0x1CC; // 0xC7187C - 0xC716B0
    if (appendPtr + (lineEnd - lineStart) >= reinterpret_cast<uint8_t*>(&FontRenderStateBuf[0]) + BUFFER_CAPACITY)
        RenderFontBuffer();

    CFontChar& record = *pEmptyChar;
    record.m_vPosn.Set(x, y);
    record.m_fWidth = m_Scale.x;
    record.m_fHeight = m_Scale.y;
    record.m_color = m_Color;
    record.m_fWrap = extraSpaceWidth;       // extra advance added per space by `RenderFontBuffer`
    record.m_fSlant = m_fSlant;
    record.m_vSlanRefPoint = m_fSlantRefPoint;
    record.m_bContainImages = m_bFontIsBlip;
    record.m_nFontStyle = m_FontStyle;
    record.m_bPropOn = m_bFontPropOn;
    record.m_wFontTexture = m_FontTextureId;
    record.m_nOutline = m_nFontOutlineOrShadow;
    ++pEmptyChar;

    // Copy the line, keeping '~' tokens verbatim so `RenderFontBuffer` re-parses them
    // (that is where token colours/button symbols are actually applied).
    char* out = reinterpret_cast<char*>(pEmptyChar);
    for (const char* s = lineStart; s < lineEnd;) {
        if (*s != '~') {
            *out++ = *s++;
        } else {
            CRGBA color = m_Color;
            const char* next = ParseToken(const_cast<char*>(s), color, RenderState.m_bContainImages, nullptr);
            if (!m_bFontIsBlip && PS2Symbol == EXSYMBOL_NONE)
                m_Color = color;
            PS2Symbol = EXSYMBOL_NONE; // original clears it unconditionally (0x71A068)
            while (s < next && s < lineEnd) // note: original is not bounded by `lineEnd` here
                *out++ = *s++;
        }
    }
    *out++ = '\0';
    pEmptyChar = reinterpret_cast<CFontChar*>((reinterpret_cast<uintptr_t>(out) + 3) & ~uintptr_t{ 3 });
}

// 0x71A0E0
float CFont::GetStringWidth(const GxtChar* string, bool full, bool scriptText) {
    size_t len = CMessages::GetStringLength(string);
    GxtChar data[400] = { 0 };

    // original caps the copy at 399 characters (strncpy_s fails & zeroes the buffer if count > dest size)
    len = std::min(len, sizeof(data) - 1);
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
// Splits `text` into lines and (when `print`) buffers each line via `GetTextBuffer`:
// - the running line position (`lineWidth`) starts at `x` for left-aligned text and 0
//   for centre/right (0x71A266); at each step the width of the upcoming word is measured
//   with `GetStringWidth` (which skips '~x~' tokens), and the line is broken when
//   `lineWidth + wordW` exceeds the wrap limit (m_fWrapx / m_fFontCentreSize /
//   x - m_fRightJustifyWrap) unless it is the first item on the line;
// - '~n~' forces a line break; a button token ('~k~' etc.) at a break position is
//   moved to the next line via the original's -3 rewind (all button tokens are 3 chars);
// - justified lines stretch by passing the extra per-space width
//   `(m_fWrapx - lineWidth) / spaces` (0x71A351) to `GetTextBuffer`;
// - the last token's colour character (`tag`, set by `ParseToken` for colour tokens) is
//   re-emitted as a "~x~" prefix on the next line so colours carry over the break;
// - lines are buffered at `lineY`, which starts at the passed `y` (0x71A27E) and grows
//   by `m_Scale.y * 18` per line (0x71A47F).
//
// NOTE (freeze fix): the original implementation could spin forever in the wrap/token
// cycle (the '-3' rewind gated by PS2Symbol/m_bNewLine feeding back into `ParseToken`,
// with lines re-buffered and flushed endlessly - samples landed inside the flush,
// `RenderFontBuffer` 0x719840 called from `GetTextBuffer` 0x719B40). This version
// guarantees progress: every path either advances `s` (ParseToken always advances past
// a token) or resets `firstItem`, which forbids breaking twice at the same position.
int16 CFont::ProcessCurrentString(bool print, float x, float y, const GxtChar* text) {
    const CRGBA colorAtEntry = m_Color; // original ends with SetColor(<m_Color at entry>) (0x71A231/0x71A5C0)

    if (*text == '\0') {
        if (print) // original runs the final SetColor even for empty strings (0x71A5B2)
            SetColor(colorAtEntry);
        return 0;
    }

    int16 lineCount = 0;
    float lineY = y;           // y of the line currently being built (starts at the passed y)
    float lineWidth = 0.0f;    // running position of the line (includes the `x` margin for left-aligned text)
    float centreOffset = 0.0f; // justify: extra width given to each space
    float justifyWidth = 0.0f; // lineWidth snapshot (incl. margin and spaces) used by the justify calculation
    int16 spaces = 0;          // spaces on the current line (justify divisor)
    char tag = 0;              // character of the last colour token (~x~)
    bool firstItem = true;     // nothing buffered on the current line yet
    const char* lineStart = reinterpret_cast<const char*>(text);
    const char* s = lineStart;
    char rebuilt[512]; // "~x~" + remainder, used when a colour token must carry over a break

    // Left-aligned text measures the line from `x` (0x71A266); centre/right start at 0.
    const auto ResetLineWidth = [&]() {
        lineWidth = !m_bFontCentreAlign && !m_bFontRightAlign ? x : 0.0f;
    };
    ResetLineWidth();

    while (*s) {
        PS2Symbol = EXSYMBOL_NONE;

        const float wordW = GetStringWidth(reinterpret_cast<const GxtChar*>(s), false, false);
        if (*s == '~') {
            CRGBA color = m_Color;
            s = ParseToken(const_cast<char*>(s), color, true, &tag); // isBlip: colour not applied here
        }

        const float limit = m_bFontCentreAlign ? m_fFontCentreSize
                          : m_bFontRightAlign  ? x - m_fRightJustifyWrap
                          : m_fWrapx;

        const bool mustBreak = m_bNewLine || (!firstItem && lineWidth + wordW > limit);
        if (!mustBreak) {
            // Consume the upcoming word (up to the next space, '~' or end of string).
            lineWidth += wordW;
            while (*s != '\0' && *s != ' ' && *s != '~')
                ++s;

            if (*s == '\0') {
                // Final line: flush it and stop.
                ++lineCount;
                if (print) {
                    float startX;
                    if (m_bFontCentreAlign)
                        startX = x - lineWidth * 0.5f;
                    else if (m_bFontRightAlign)
                        startX = x - lineWidth;
                    else
                        startX = x;
                    GetTextBuffer(startX, lineY, lineStart, s, 0.0f);
                }
                break;
            }
            if (*s == ' ') {
                if (!firstItem)
                    ++spaces;
                lineWidth += GetCharacterSize(0); // space glyph width
                ++s;
            }
            // For '~' the token is handled by the next iteration (ParseToken advances past it).
            justifyWidth = lineWidth; // 0x71A58E
            firstItem = false;
            continue;
        }

        // ----- break: flush the current line and start a new one -----
        const char* lineEnd = s;
        if (PS2Symbol != EXSYMBOL_NONE) {
            // Move the button token onto the next line: rewind over "~x~" (all button
            // tokens are exactly 3 characters).
            s -= 3;
            lineEnd = s;
        } else if (m_bNewLine) {
            lineEnd = s - 3; // exclude the "~n~" that caused the break
        }

        if (m_bFontJustify && !m_bFontCentreAlign && spaces != 0)
            centreOffset = (m_fWrapx - justifyWidth) / static_cast<float>(spaces); // 0x71A351

        ++lineCount;
        if (print) {
            float startX;
            if (m_bFontCentreAlign)
                startX = x - lineWidth * 0.5f;
            else if (m_bFontRightAlign)
                startX = x - lineWidth + GetCharacterSize(0); // 0x71A388
            else
                startX = x;
            GetTextBuffer(startX, lineY, lineStart, lineEnd, centreOffset);
        }

        if (tag) {
            // Re-emit the colour token at the start of the next line so the colour
            // carries over the break (original: sprintf into 0xB71670, 0x71A3E6).
            size_t n = 0;
            rebuilt[n++] = '~';
            rebuilt[n++] = tag;
            rebuilt[n++] = '~';
            // Original re-checks `m_bNewLine` here: `RenderFontBuffer` may have set it
            // while flushing a line whose text contained "~n~" (0x71A42F).
            const char* rest = m_bNewLine ? lineEnd + 3 : lineEnd;
            while (*rest) // manual forward copy: `rest` may point into `rebuilt` itself
                rebuilt[n++] = *rest++;
            rebuilt[n] = '\0';
            s = rebuilt;
            lineStart = s;
            tag = 0;
        }

        m_bNewLine = false;
        lineY += m_Scale.y * 18.0f; // line height: m_Scale.y * 32 * 0.5 + m_Scale.y * 2 (0x71A47F)
        centreOffset = 0.0f;
        spaces = 0;
        justifyWidth = 0.0f;
        ResetLineWidth();
        firstItem = true;
        lineStart = s; // points at the remainder, or at `rebuilt` after a tag rebuild
        PS2Symbol = EXSYMBOL_NONE;
    }

    // NOTE: the original ends with `SetColor(<m_Color at entry>)` (0x71A5C0).
    if (print)
        SetColor(colorAtEntry);

    return lineCount;
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
float GetScriptLetterSize(uint8 letterId) {
    return plugin::CallAndReturn<float, 0x719670, uint8>(letterId);
}

// 0x7192C0
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
float GetLetterIdPropValue(uint8 letterId) {
    uint8 id = letterId;

    if (letterId == '?')
        id = 0;

    if (CFont::RenderState.m_bPropOn)
        return gFontData[CFont::RenderState.m_wFontTexture].m_propValues[id];
    else
        return gFontData[CFont::RenderState.m_wFontTexture].m_unpropValue;
}
