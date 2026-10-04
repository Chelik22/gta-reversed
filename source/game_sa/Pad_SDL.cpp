#include <StdInc.h>

#ifdef NOTSA_USE_SDL3
#include <SDL3/SDL.h>

bool CPad::ProcessMouseEvent(const SDL_Event& e, CMouseControllerState& ms) {
    switch (e.type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        switch (e.button.button) {
        case SDL_BUTTON_LEFT:   ms.isMouseLeftButtonPressed = e.button.down;  break;
        case SDL_BUTTON_RIGHT:  ms.isMouseRightButtonPressed = e.button.down;  break;
        case SDL_BUTTON_MIDDLE: ms.isMouseMiddleButtonPressed = e.button.down;  break;
        case SDL_BUTTON_X1:     ms.isMouseFirstXPressed = e.button.down; break;
        case SDL_BUTTON_X2:     ms.isMouseSecondXPressed = e.button.down; break;
        }
        return true;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
        ms.isMouseWheelMovedUp   = e.wheel.y > 0.f;
        ms.isMouseWheelMovedDown = e.wheel.y < 0.f;
        return true;
    }
    case SDL_EVENT_MOUSE_MOTION: {
        ms.m_AmountMoved.x += e.motion.xrel;
        ms.m_AmountMoved.y += e.motion.yrel;
        return true;
    }
    case SDL_EVENT_MOUSE_ADDED:
    case SDL_EVENT_MOUSE_REMOVED: {
        NOTSA_LOG_DEBUG(
            "SDL: Mouse ({}) {}",
            e.mdevice.which,
            e.mdevice.type == SDL_EVENT_MOUSE_ADDED ? "added" : "removed"
        );
        return true;
    }
    }
    return false;
}

bool CPad::ProcessKeyboardEvent(const SDL_Event& e, CKeyboardState& ks) {
    switch (e.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        // NOTE / NOTSA(fix): Auto-repeat key events must NOT be dropped here.
        // The original (Win32) `WndProc` -> `HandleKeyDown` (0x743DF0) never checked
        // the repeat flag - every `WM_KEYDOWN` (repeats included) refreshed `TempKeyState`.
        // After the window regains keyboard focus, auto-repeats are the only events
        // a still-held key produces, so filtering them out permanently lost held keys
        // (eg. `W` held -> `NewState.LeftStickY` stuck at 0 -> player frozen).
        // See also `RestoreKeyboardStateFromOS()`.

        //NOTSA_LOG_DEBUG("SDL: Key ({}): {}", (char)(e.key.key), e.key.down);

        const auto s = e.key.down ? 255 : 0;

        // Keypad digits (with numlock on) are reported by SDL3 with plain digit
        // keycodes, so we have to use the scancode to reach the `numX` slots.
        // Only do this when the keycode actually is a digit - with numlock off
        // the keypad sends navigation keycodes (like the original `WM_KEYDOWN`s),
        // which are handled by the switch below (eg. `SDLK_END` -> `ks.end`).
        if (e.key.key >= SDLK_0 && e.key.key <= SDLK_9) {
            switch (e.key.scancode) {
            case SDL_SCANCODE_KP_0: ks.num0 = s; return true;
            case SDL_SCANCODE_KP_1: ks.num1 = s; return true;
            case SDL_SCANCODE_KP_2: ks.num2 = s; return true;
            case SDL_SCANCODE_KP_3: ks.num3 = s; return true;
            case SDL_SCANCODE_KP_4: ks.num4 = s; return true;
            case SDL_SCANCODE_KP_5: ks.num5 = s; return true;
            case SDL_SCANCODE_KP_6: ks.num6 = s; return true;
            case SDL_SCANCODE_KP_7: ks.num7 = s; return true;
            case SDL_SCANCODE_KP_8: ks.num8 = s; return true;
            case SDL_SCANCODE_KP_9: ks.num9 = s; return true;
            }
        }

        switch (e.key.key) {
        case SDLK_F1:           ks.FKeys[0] = s;                break;                     
        case SDLK_F2:           ks.FKeys[1] = s;                break;                     
        case SDLK_F3:           ks.FKeys[2] = s;                break;                     
        case SDLK_F4:           ks.FKeys[3] = s;                break;                     
        case SDLK_F5:           ks.FKeys[4] = s;                break;                     
        case SDLK_F6:           ks.FKeys[5] = s;                break;                     
        case SDLK_F7:           ks.FKeys[6] = s;                break;                     
        case SDLK_F8:           ks.FKeys[7] = s;                break;                     
        case SDLK_F9:           ks.FKeys[8] = s;                break;                     
        case SDLK_F10:          ks.FKeys[9] = s;                break;                    
        case SDLK_F11:          ks.FKeys[10] = s;               break;                    
        case SDLK_F12:          ks.FKeys[11] = s;               break;
        case SDLK_ESCAPE:       ks.esc = s;                     break;
        case SDLK_INSERT:       ks.insert = s;                  break;
        case SDLK_DELETE:       ks.del = s;                     break;
        case SDLK_HOME:         ks.home = s;                    break;
        case SDLK_END:          ks.end = s;                     break;
        case SDLK_PAGEUP:       ks.pgup = s;                    break;
        case SDLK_PAGEDOWN:     ks.pgdn = s;                    break;
        case SDLK_UP:           ks.up = s;                      break; // Arrow up
        case SDLK_DOWN:         ks.down = s;                    break; // Arrow down
        case SDLK_LEFT:         ks.left = s;                    break; // Arrow left
        case SDLK_RIGHT:        ks.right = s;                   break; // Arrow right
        case SDLK_SCROLLLOCK:   ks.scroll = s;                  break;
        case SDLK_PAUSE:        ks.pause = s;                   break;
        case SDLK_NUMLOCKCLEAR: ks.numlock = s;                 break; // (?)
        // NOTSA(fix): The keypad operators have their own keycodes in SDL3.
        // The plain `SDLK_SLASH` / `SDLK_MINUS` / ... are main-row keys and must
        // end up in `standardKeys` (via `default:` below), like the original
        // `GTATranslateKey` -> `HandleKeyDown` does (VK_DIVIDE, VK_SUBTRACT, ...).
        case SDLK_KP_DIVIDE:    ks.div = s;                     break;
        case SDLK_KP_MULTIPLY:  ks.mul = s;                     break;
        case SDLK_KP_MINUS:     ks.sub = s;                     break;
        case SDLK_KP_PLUS:      ks.add = s;                     break;
        case SDLK_KP_PERIOD:    ks.decimal = s;                 break;
        // NOTSA(fix): These two were swapped. Original mapping (Win32):
        //   main  Return (VK_RETURN, no KF_EXTENDED) -> rsENTER  -> `extenter`
        //   numpad Enter (VK_RETURN + KF_EXTENDED)   -> rsPADENTER -> `enter`
        case SDLK_RETURN:       ks.extenter = s;                break; // rsENTER
        case SDLK_KP_ENTER:     ks.enter = s;                   break; // rsPADENTER
        case SDLK_TAB:          ks.tab = s;                     break;
        case SDLK_CAPSLOCK:     ks.capslock = s;                break;
        case SDLK_LSHIFT:       ks.lshift = s;                  break;
        case SDLK_RSHIFT:       ks.rshift = s;                  break;
        case SDLK_LCTRL:        ks.lctrl = s;                   break;
        case SDLK_RCTRL:        ks.rctrl = s;                   break;
        case SDLK_LALT:         ks.lalt = s;                    break;
        case SDLK_RALT:         ks.ralt = s;                    break;
        case SDLK_LGUI:         ks.lwin = s;                    break;
        case SDLK_RGUI:         ks.rwin = s;                    break;
        case SDLK_APPLICATION:  ks.apps = s;                    break; // (?)
        default: { // Other keys
            const auto key = std::toupper(e.key.key);
            if (key <= 0xFF) {
                ks.standardKeys[key] = s;
            }
            return true;
        }
        }
        ks.shift = (ks.rshift || ks.lshift) ? 255 : 0;
        return true;
    }
    case SDL_EVENT_KEYBOARD_ADDED:
    case SDL_EVENT_KEYBOARD_REMOVED: {
        NOTSA_LOG_DEBUG(
            "SDL: Keyboard ({}) {}",
            e.kdevice.which,
            e.kdevice.type == SDL_EVENT_KEYBOARD_ADDED ? "added" : "removed"
        );
        return true;
    }
    }
    return false;
}

bool CPad::ProcessGamepadEvent(const SDL_Event & e, CControllerState& cs) {
    //switch (e.type) {
    //case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
    //    const auto UpdateAxis = [&](int16& outA, int16& outB, bool isInverted, bool isSwapped) {
    //        int16 pos = e.gaxis.value;
    //        if (fabs(pos) > 0.3f) {
    //            pos = isInverted ? -pos : pos;
    //            pos /= 128; // SDL values are [-32768 to 32767], need to map to [-128, 127] (Actually should be [-128, 128] (?))
    //            (isSwapped ? outA : outB) = pos;
    //        }
    //    };
    //    switch (e.gaxis.axis) {
    //    case SDL_GAMEPAD_AXIS_LEFTX:
    //    case SDL_GAMEPAD_AXIS_LEFTY:
    //    case SDL_GAMEPAD_AXIS_RIGHTX:
    //    case SDL_GAMEPAD_AXIS_RIGHTY:
    //    }
    //        
    //    UpdateAxis(cs.LeftStickY, cs.LeftStickX, FrontEndMenuManager.m_bInvertPadX1, FrontEndMenuManager.m_bSwapPadAxis1);
    //    UpdateAxis(cs.LeftStickX, cs.LeftStickY, FrontEndMenuManager.m_bInvertPadY1, FrontEndMenuManager.m_bSwapPadAxis2);
    //
    //    UpdateAxis(cs.LeftStickY, cs.LeftStickX, FrontEndMenuManager.m_bInvertPadX2, FrontEndMenuManager.m_bSwapPadAxis1);
    //    UpdateAxis(cs.LeftStickX, cs.LeftStickY, FrontEndMenuManager.m_bInvertPadY2, FrontEndMenuManager.m_bSwapPadAxis2);
    //    return true;
    //}
    //}
    return false;
}

bool CPad::ProcessJoyStickEvent(const SDL_Event& e, CControllerState& cs) {
    return false;
}

bool CPad::ProcessEvent(const SDL_Event& e, bool ignoreMouseEvents, bool ignoreKeyboardEvents) {
    // Mouse/keyboard events are meant to be handled by Pad 0
    if (!ignoreMouseEvents && GetPad(0)->ProcessMouseEvent(e, TempMouseControllerState)) {
        return true;
    }
    if (!ignoreKeyboardEvents && GetPad(0)->ProcessKeyboardEvent(e, TempKeyState)) {
        return true;
    }

    // Other events (I guess) can be handled by any pad
    for (auto& pad : Pads) {
        if (pad.ProcessJoyStickEvent(e, pad.PCTempJoyState)) {
            return true;
        }
    }

    // Event not processed
    return false;
}

// NOTSA(fix): SDL3, unlike Win32, releases every pressed key when the window
// loses keyboard focus (`SDL_ResetKeyboard()` inside `SDL_SetKeyboardFocus()`),
// synthesizing `SDL_EVENT_KEY_UP`s that zero out our `TempKeyState`. The original
// (Win32) game never does this - there a key held across a focus change simply
// stays held (and `WM_KEYDOWN` auto-repeats, which the original never filtered,
// keep refreshing it). Without compensation a key that was held during a focus
// blip is lost forever -> `NewState.LeftStickY` stays 0 -> the player hard-freezes
// until the key is physically released and pressed again.
// We therefore rebuild the whole keyboard state from the OS whenever the window
// (re)gains keyboard focus. Slot mapping mirrors the original
// `GTATranslateKey` (0x747820) / `HandleKeyDown` (0x743DF0) pair.
void CPad::RestoreKeyboardStateFromOS() {
    auto& ks = TempKeyState;

    const auto Down = [](unsigned vk) -> int16 {
        return (GetAsyncKeyState(static_cast<int>(vk)) & 0x8000) ? 255 : 0;
    };

    for (auto i = 0; i < 12; i++) {
        ks.FKeys[i] = Down(VK_F1 + i);
    }

    ks.esc      = Down(VK_ESCAPE);
    ks.insert   = Down(VK_INSERT);
    ks.del      = Down(VK_DELETE);
    ks.home     = Down(VK_HOME);
    ks.end      = Down(VK_END);
    ks.pgup     = Down(VK_PRIOR);
    ks.pgdn     = Down(VK_NEXT);
    ks.up       = Down(VK_UP);
    ks.down     = Down(VK_DOWN);
    ks.left     = Down(VK_LEFT);
    ks.right    = Down(VK_RIGHT);
    ks.scroll   = Down(VK_SCROLL);
    ks.pause    = Down(VK_PAUSE);
    ks.numlock  = Down(VK_NUMLOCK);
    ks.div      = Down(VK_DIVIDE);
    ks.mul      = Down(VK_MULTIPLY);
    ks.sub      = Down(VK_SUBTRACT);
    ks.add      = Down(VK_ADD);
    ks.decimal  = Down(VK_DECIMAL);
    ks.num0     = Down(VK_NUMPAD0);
    ks.num1     = Down(VK_NUMPAD1);
    ks.num2     = Down(VK_NUMPAD2);
    ks.num3     = Down(VK_NUMPAD3);
    ks.num4     = Down(VK_NUMPAD4);
    ks.num5     = Down(VK_NUMPAD5);
    ks.num6     = Down(VK_NUMPAD6);
    ks.num7     = Down(VK_NUMPAD7);
    ks.num8     = Down(VK_NUMPAD8);
    ks.num9     = Down(VK_NUMPAD9);
    ks.back     = Down(VK_BACK);
    ks.tab      = Down(VK_TAB);
    ks.capslock = Down(VK_CAPITAL);
    ks.extenter = Down(VK_RETURN); // Main enter -> rsENTER -> `extenter` (original quirk).
    // `ks.enter` (rsPADENTER) is left alone: the OS state of VK_RETURN cannot tell
    // the main and the numpad Enter apart, and a false positive here would trigger
    // bindings made to numpad Enter. SDL events themselves maintain this slot.
    ks.lshift   = Down(VK_LSHIFT);
    ks.rshift   = Down(VK_RSHIFT);
    ks.shift    = (ks.lshift || ks.rshift) ? 255 : 0;
    ks.lctrl    = Down(VK_LCONTROL);
    ks.rctrl    = Down(VK_RCONTROL);
    ks.lalt     = Down(VK_LMENU);
    ks.ralt     = Down(VK_RMENU);
    ks.lwin     = Down(VK_LWIN);
    ks.rwin     = Down(VK_RWIN);
    ks.apps     = Down(VK_APPS);

    // Standard (character) keys - same rule as the original `GTATranslateKey`:
    // every VK that isn't one of the named keys above and whose character
    // translation fits into a byte lands in `standardKeys`.
    for (auto vk = 0x01u; vk < 0xFFu; vk++) {
        switch (vk) { // Skip VKs that belong to the named slots handled above
        case VK_ESCAPE:  case VK_PRIOR:   case VK_NEXT:    case VK_END:      case VK_HOME:
        case VK_LEFT:    case VK_UP:      case VK_RIGHT:   case VK_DOWN:     case VK_INSERT:
        case VK_DELETE:  case VK_BACK:    case VK_TAB:     case VK_CAPITAL:  case VK_NUMLOCK:
        case VK_SCROLL:  case VK_PAUSE:   case VK_RETURN:  case VK_SHIFT:    case VK_CONTROL:
        case VK_MENU:    case VK_LWIN:    case VK_RWIN:    case VK_APPS:
        case VK_DIVIDE:  case VK_MULTIPLY: case VK_SUBTRACT: case VK_ADD:   case VK_DECIMAL:
            continue;
        default:
            if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) { // Numpad digits
                continue;
            }
            if (vk >= VK_F1 && vk <= VK_F12) { // Function keys
                continue;
            }
            break;
        }

        const auto chr = MapVirtualKey(vk, MAPVK_VK_TO_CHAR); // Dead keys have the high bit set - excluded by the check below
        if (chr > 0 && chr <= 0xFE) {
            ks.standardKeys[chr] = Down(vk);
        }
    }
}
#endif
