#include "PepperSnapInternal.h"

// ----------------------------------------------------------------------------
// Custom Hotkeys, Explorer Image Context Menu & Low-Level Keyboard Hook
// ----------------------------------------------------------------------------

int g_RecordingHotkeySlot = 0; // 0 = None, 1 = Custom Window, 2 = Instant Fullscreen, 3 = Instant Save Previous Window
DWORD g_LastHotkeyTriggerTick = 0;

std::wstring PepperSnapDaemon::FormatHotkeyString(const HotkeyBinding& hk) {
    if (hk.vk == 0) return L"";
    std::wstring out;
    if (hk.modifiers & MOD_CONTROL) out += L"Ctrl + ";
    if (hk.modifiers & MOD_SHIFT)   out += L"Shift + ";
    if (hk.modifiers & MOD_ALT)     out += L"Alt + ";
    if (hk.modifiers & MOD_WIN)     out += L"Win + ";

    switch (hk.vk) {
        case VK_ESCAPE:   out += L"Esc"; break;
        case VK_SNAPSHOT: out += L"PrintScreen"; break;
        case VK_SPACE:    out += L"Space"; break;
        case VK_TAB:      out += L"Tab"; break;
        case VK_RETURN:   out += L"Enter"; break;
        case VK_BACK:     out += L"Backspace"; break;
        case VK_INSERT:   out += L"Insert"; break;
        case VK_DELETE:   out += L"Delete"; break;
        case VK_HOME:     out += L"Home"; break;
        case VK_END:      out += L"End"; break;
        case VK_PRIOR:    out += L"PageUp"; break;
        case VK_NEXT:     out += L"PageDown"; break;
        case VK_PAUSE:    out += L"Pause"; break;
        case VK_UP:       out += L"Up"; break;
        case VK_DOWN:     out += L"Down"; break;
        case VK_LEFT:     out += L"Left"; break;
        case VK_RIGHT:    out += L"Right"; break;
        case VK_OEM_MINUS:
        case VK_SUBTRACT: out += L"-"; break;
        case VK_OEM_PLUS:
        case VK_ADD:      out += L"="; break;
        default:
            if (hk.vk >= 'A' && hk.vk <= 'Z') {
                out.push_back((wchar_t)hk.vk);
            } else if (hk.vk >= '0' && hk.vk <= '9') {
                out.push_back((wchar_t)hk.vk);
            } else if (hk.vk >= VK_F1 && hk.vk <= VK_F24) {
                out += L"F" + std::to_wstring(hk.vk - VK_F1 + 1);
            } else {
                UINT scan = MapVirtualKeyW(hk.vk, MAPVK_VK_TO_VSC);
                WCHAR keyName[64] = {0};
                if (GetKeyNameTextW((LONG)(scan << 16), keyName, 63) > 0) {
                    out += keyName;
                } else {
                    out += L"Key(" + std::to_wstring(hk.vk) + L")";
                }
            }
            break;
    }
    return out;
}

std::wstring FormatLabelWithShortcut(const std::wstring& label, const HotkeyBinding& hk, const std::wstring& suffix) {
    if (hk.vk == 0) return label + suffix;
    return label + L" (" + PepperSnapDaemon::FormatHotkeyString(hk) + L")" + suffix;
}

bool MatchesOverlayShortcut(const HotkeyBinding& hk, UINT vk, bool ctrl, bool shift, bool alt) {
    if (hk.vk == 0) return false;
    bool vkMatch = (vk == hk.vk) ||
                   (hk.vk == VK_OEM_MINUS && vk == VK_SUBTRACT) ||
                   (hk.vk == VK_OEM_PLUS  && vk == VK_ADD) ||
                   (hk.vk == '0'          && vk == VK_NUMPAD0);
    if (!vkMatch) return false;
    bool reqCtrl  = (hk.modifiers & MOD_CONTROL) != 0;
    bool reqShift = (hk.modifiers & MOD_SHIFT) != 0;
    bool reqAlt   = (hk.modifiers & MOD_ALT) != 0;
    return (ctrl == reqCtrl) && (shift == reqShift) && (alt == reqAlt);
}

bool MatchesOverlayShortcutWithOptionalShift(const HotkeyBinding& hk, UINT vk, bool ctrl, bool shift, bool alt) {
    if (hk.vk == 0 || vk == 0) return false;
    bool vkMatch = (vk == hk.vk) ||
                   (hk.vk == VK_OEM_MINUS && vk == VK_SUBTRACT) ||
                   (hk.vk == VK_OEM_PLUS  && vk == VK_ADD) ||
                   (hk.vk == '0'          && vk == VK_NUMPAD0);
    if (!vkMatch) return false;
    bool reqCtrl  = (hk.modifiers & MOD_CONTROL) != 0;
    bool reqShift = (hk.modifiers & MOD_SHIFT) != 0;
    bool reqAlt   = (hk.modifiers & MOD_ALT) != 0;
    if (ctrl != reqCtrl || alt != reqAlt) return false;
    return (shift == reqShift) || (shift && !reqShift);
}

bool MatchesAnyPinnedShortcut(UINT vk, bool ctrl, bool shift, bool alt) {
    return MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinZoomOut,     vk, ctrl, shift, alt) ||
           MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinZoomIn,      vk, ctrl, shift, alt) ||
           MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinZoomReset,   vk, ctrl, shift, alt) ||
           MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinPosReset,    vk, ctrl, shift, alt) ||
           MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinOutline,     vk, ctrl, shift, alt) ||
           MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinHideToolbar, vk, ctrl, shift, alt) ||
           MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinSmooth,      vk, ctrl, shift, alt) ||
           MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinSaveAs,      vk, ctrl, shift, alt) ||
           MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinSave,        vk, ctrl, shift, alt) ||
           MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinCopy,        vk, ctrl, shift, alt) ||
           MatchesOverlayShortcut(g_Daemon.hkPinSearchGoogle,                 vk, ctrl, shift, alt) ||
           MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinClose,       vk, ctrl, shift, alt);
}

void PepperSnapDaemon::ApplyGlobalHotkeys() {
    if (!hTrayWnd) return;
    UnregisterHotKey(hTrayWnd, HK_REGION_SNIP);
    UnregisterHotKey(hTrayWnd, HK_FULL_SNAP);
    UnregisterHotKey(hTrayWnd, HK_PREV_REGION);
    UnregisterHotKey(hTrayWnd, HK_PIN_CLIPBOARD);
    UnregisterHotKey(hTrayWnd, HK_EDIT_CLIPBOARD);
    UnregisterHotKey(hTrayWnd, HK_SEARCH_GOOGLE);
    auto regHk = [&](int id, const HotkeyBinding& hk) {
        if (hk.vk == 0) return;
        if (!RegisterHotKey(hTrayWnd, id, hk.modifiers | 0x4000 /*MOD_NOREPEAT*/, hk.vk)) {
            RegisterHotKey(hTrayWnd, id, hk.modifiers, hk.vk);
        }
    };
    regHk(HK_REGION_SNIP,   hkRegionSnip);
    regHk(HK_FULL_SNAP,     hkFullSnap);
    regHk(HK_PREV_REGION,   hkPrevRegion);
    regHk(HK_PIN_CLIPBOARD, hkPinClipboard);
    regHk(HK_EDIT_CLIPBOARD, hkEditClipboard);
    regHk(HK_SEARCH_GOOGLE, hkSearchGoogle);
    UpdateTrayTooltip();
}


bool IsModifierVk(DWORD vk) {
    return vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
           vk == VK_SHIFT   || vk == VK_LSHIFT   || vk == VK_RSHIFT   ||
           vk == VK_MENU    || vk == VK_LMENU    || vk == VK_RMENU    ||
           vk == VK_LWIN    || vk == VK_RWIN;
}

UINT GetCurrentHardwareModifiers() {
    UINT curMods = 0;
    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0) {
        curMods |= MOD_CONTROL;
    }
    if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0) {
        curMods |= MOD_SHIFT;
    }
    if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_LMENU) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_RMENU) & 0x8000) != 0) {
        curMods |= MOD_ALT;
    }
    if ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0) {
        curMods |= MOD_WIN;
    }
    return curMods;
}

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;
        if ((kb->flags & LLKHF_INJECTED) != 0) {
            return CallNextHookEx(g_Daemon.hKeyHook, nCode, wParam, lParam);
        }
        bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        bool isKeyUp   = (wParam == WM_KEYUP   || wParam == WM_SYSKEYUP);

        // Normalize SysRq (scanCode 0x54 when Shift/Alt + PrintScreen is pressed) to VK_SNAPSHOT
        DWORD vk = kb->vkCode;
        if (kb->scanCode == 0x54 || kb->scanCode == 0x37) {
            if (vk == VK_SNAPSHOT || vk == VK_EXECUTE || vk == 0xFF) {
                vk = VK_SNAPSHOT;
            }
        }

        // If the Customize Keys dialog is actively recording a custom shortcut key
        HWND hRecTarget = (g_Daemon.hCustomizeKeysWnd && IsWindow(g_Daemon.hCustomizeKeysWnd))
            ? g_Daemon.hCustomizeKeysWnd
            : ((g_Daemon.hOptionsWnd && IsWindow(g_Daemon.hOptionsWnd)) ? g_Daemon.hOptionsWnd : nullptr);
        if (g_RecordingHotkeySlot != 0 && hRecTarget) {
            if (isKeyDown || (isKeyUp && vk == VK_SNAPSHOT)) {
                if (isKeyDown && vk == VK_ESCAPE) {
                    PostMessageW(hRecTarget, WM_SHORTCUT_RECORDED, 0, VK_ESCAPE);
                    return 1;
                }
                if (!IsModifierVk(vk)) {
                    UINT mods = GetCurrentHardwareModifiers();
                    if (kb->flags & LLKHF_ALTDOWN) mods |= MOD_ALT;
                    PostMessageW(hRecTarget, WM_SHORTCUT_RECORDED, (WPARAM)mods, (LPARAM)vk);
                    return 1;
                }
            }
            if (vk == VK_SNAPSHOT) {
                return 1;
            }
            return CallNextHookEx(g_Daemon.hKeyHook, nCode, wParam, lParam);
        }

        bool noModalOpen =
            (!g_Daemon.hOptionsWnd || !IsWindow(g_Daemon.hOptionsWnd)) &&
            (!g_Daemon.hCustomizeKeysWnd || !IsWindow(g_Daemon.hCustomizeKeysWnd)) &&
            (!g_Daemon.hNamingSyntaxWnd || !IsWindow(g_Daemon.hNamingSyntaxWnd)) &&
            (!g_Daemon.hShortcutsWnd || !IsWindow(g_Daemon.hShortcutsWnd)) &&
            !g_Daemon.isSavingAsModalOpen;

        // Handle key-down for all global capture hotkeys, plus key-up fallback for VK_SNAPSHOT
        // (Many keyboards/games swallow WM_KEYDOWN for Shift+PrintScreen and only emit WM_KEYUP).
        // Evaluated before overlay key forwarding so capture shortcuts also execute in "Edit with PepperSnap" window!
        if (noModalOpen && (isKeyDown || (isKeyUp && vk == VK_SNAPSHOT)) && !IsModifierVk(vk)) {
            UINT curMods = GetCurrentHardwareModifiers();
            if (kb->flags & LLKHF_ALTDOWN) curMods |= MOD_ALT;

            auto matchHk = [&](const HotkeyBinding& hk) {
                return hk.vk != 0 && vk == hk.vk && curMods == hk.modifiers;
            };

            bool overlayActive = (g_Daemon.hOverlayWnd && IsWindow(g_Daemon.hOverlayWnd));
            bool canCaptureNow = !overlayActive || g_Daemon.IsEditingOpenedImage();
            bool inlineEditActive = overlayActive &&
                (g_Daemon.isEditingText || g_Daemon.isEditingSize || g_Daemon.isEditingRatio || g_Daemon.isEditingStroke);
            if (!inlineEditActive && !g_Daemon.pinnedWindows.empty()) {
                HWND hFg = GetForegroundWindow();
                if (g_Daemon.IsPinnedWindow(hFg)) {
                    PinnedWindowData* fgData = (PinnedWindowData*)GetWindowLongPtrW(hFg, GWLP_USERDATA);
                    if (fgData && (fgData->isEditingSize || fgData->isEditingPct)) {
                        inlineEditActive = true;
                    }
                }
            }

            if (matchHk(g_Daemon.hkPrevRegion)) {
                PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_PREV_REGION, 0, 0);
                return 1;
            }
            if (canCaptureNow && matchHk(g_Daemon.hkRegionSnip)) {
                PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_REGION_SNIP, 0, 0);
                return 1;
            }
            if (matchHk(g_Daemon.hkFullSnap)) {
                PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_FULL_SNAP, 0, 0);
                return 1;
            }
            if (canCaptureNow && !inlineEditActive && matchHk(g_Daemon.hkPinClipboard)) {
                PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_PIN_CLIPBOARD, 0, 0);
                return 1;
            }
            if (!inlineEditActive && matchHk(g_Daemon.hkEditClipboard)) {
                PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_EDIT_CLIPBOARD, 0, 0);
                return 1;
            }
            if (!inlineEditActive && matchHk(g_Daemon.hkSearchGoogle)) {
                PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_SEARCH_GOOGLE, 0, 0);
                return 1;
            }
        }

        // If the Custom Area overlay is active (with no modal child window open), ensure Esc and overlay keys
        // reach hOverlayWnd immediately even if a higher-band shell flyout (Notification Sidebar) held focus
        if (g_Daemon.hOverlayWnd && IsWindow(g_Daemon.hOverlayWnd) && noModalOpen) {
            // When "Edit with PepperSnap" is active and pinned images overlay it, let pinned windows handle
            // inline size/percentage typing or pinned shortcuts when the cursor is over a pinned image
            if (g_Daemon.IsEditingOpenedImage() && g_Daemon.pinnedImageAboveEdit && !g_Daemon.pinnedWindows.empty()) {
                HWND hFg = GetForegroundWindow();
                if (g_Daemon.IsPinnedWindow(hFg)) {
                    PinnedWindowData* fgData = (PinnedWindowData*)GetWindowLongPtrW(hFg, GWLP_USERDATA);
                    if (fgData && (fgData->isEditingSize || fgData->isEditingPct)) {
                        return CallNextHookEx(g_Daemon.hKeyHook, nCode, wParam, lParam);
                    }
                }
                POINT curPt = { 0, 0 };
                if (GetCursorPos(&curPt)) {
                    HWND hAtPt = WindowFromPoint(curPt);
                    if (g_Daemon.IsPinnedWindow(hAtPt) && isKeyDown && !IsModifierVk(vk)) {
                        UINT curMods = GetCurrentHardwareModifiers();
                        if (kb->flags & LLKHF_ALTDOWN) curMods |= MOD_ALT;
                        bool ctrl  = (curMods & MOD_CONTROL) != 0;
                        bool shift = (curMods & MOD_SHIFT) != 0;
                        bool alt   = (curMods & MOD_ALT) != 0;
                        if (MatchesAnyPinnedShortcut(vk, ctrl, shift, alt)) {
                            if (hFg != hAtPt) {
                                PostMessageW(hAtPt, WM_KEYDOWN, vk, 0);
                                return 1;
                            }
                            return CallNextHookEx(g_Daemon.hKeyHook, nCode, wParam, lParam);
                        }
                    }
                }
            }
            if (isKeyDown && vk == VK_ESCAPE) {
                PostMessageW(g_Daemon.hOverlayWnd, WM_KEYDOWN, VK_ESCAPE, 0);
                return 1;
            }
            if (GetForegroundWindow() != g_Daemon.hOverlayWnd) {
                if (isKeyDown && !IsModifierVk(vk)) {
                    PostMessageW(g_Daemon.hOverlayWnd, WM_KEYDOWN, vk, 0);
                    return 1;
                } else if (isKeyDown && (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL)) {
                    PostMessageW(g_Daemon.hOverlayWnd, WM_KEYDOWN, VK_CONTROL, 0);
                } else if (isKeyUp && (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL)) {
                    PostMessageW(g_Daemon.hOverlayWnd, WM_KEYUP, VK_CONTROL, 0);
                } else if (isKeyDown && (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT)) {
                    PostMessageW(g_Daemon.hOverlayWnd, WM_KEYDOWN, VK_SHIFT, 0);
                } else if (isKeyUp && (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT)) {
                    PostMessageW(g_Daemon.hOverlayWnd, WM_KEYUP, VK_SHIFT, 0);
                }
            }
        } else if ((!g_Daemon.hOverlayWnd || !IsWindow(g_Daemon.hOverlayWnd)) &&
                   noModalOpen &&
                   !g_Daemon.pinnedWindows.empty() && isKeyDown && !IsModifierVk(vk)) {
            HWND hFg = GetForegroundWindow();
            if (!g_Daemon.IsPinnedWindow(hFg)) {
                POINT curPt = {0, 0};
                if (GetCursorPos(&curPt)) {
                    HWND hAtPt = WindowFromPoint(curPt);
                    if (g_Daemon.IsPinnedWindow(hAtPt)) {
                        UINT curMods = GetCurrentHardwareModifiers();
                        if (kb->flags & LLKHF_ALTDOWN) curMods |= MOD_ALT;
                        bool ctrl  = (curMods & MOD_CONTROL) != 0;
                        bool shift = (curMods & MOD_SHIFT) != 0;
                        bool alt   = (curMods & MOD_ALT) != 0;
                        if (MatchesAnyPinnedShortcut(vk, ctrl, shift, alt)) {
                            PostMessageW(hAtPt, WM_KEYDOWN, vk, 0);
                            return 1;
                        }
                    }
                }
            }
        }
    }
    return CallNextHookEx(g_Daemon.hKeyHook, nCode, wParam, lParam);
}

DWORD g_InputHookThreadId = 0;

DWORD WINAPI InputHookWorkerThread(LPVOID) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    g_Daemon.hKeyHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_Daemon.hInst, 0);

    // 15ms hardware key-state poller + debounced hook health check
    UINT_PTR pollTimer = SetTimer(nullptr, 0, 15, nullptr);
    HWND lastFgWnd = GetForegroundWindow();
    DWORD lastHookInstallTick = GetTickCount();
    DWORD pendingFgRehookTick = 0;
    bool wasDownPrev   = false;
    bool wasDownReg    = false;
    bool wasDownFull   = false;
    bool wasDownPin    = false;
    bool wasDownEdit   = false;
    bool wasDownSearch = false;

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_TIMER && msg.wParam == pollTimer) {
            DWORD now = GetTickCount();
            HWND curFg = GetForegroundWindow();
            if (curFg && curFg != lastFgWnd &&
                curFg != g_Daemon.hOverlayWnd && curFg != g_Daemon.hOptionsWnd &&
                !g_Daemon.IsPinnedWindow(curFg)) {
                lastFgWnd = curFg;
                pendingFgRehookTick = (now == 0) ? 1 : now;
            }

            // Reinstall WH_KEYBOARD_LL only if the hook handle is invalid, or after a >=250ms debounce following a foreground change
            if (!g_Daemon.hKeyHook) {
                g_Daemon.hKeyHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_Daemon.hInst, 0);
                lastHookInstallTick = now;
                pendingFgRehookTick = 0;
            } else if (pendingFgRehookTick != 0 &&
                       (now - pendingFgRehookTick >= 250) &&
                       (now - lastHookInstallTick >= 250)) {
                HHOOK hNewHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_Daemon.hInst, 0);
                if (hNewHook) {
                    HHOOK hOldHook = g_Daemon.hKeyHook;
                    g_Daemon.hKeyHook = hNewHook;
                    if (hOldHook) UnhookWindowsHookEx(hOldHook);
                }
                lastHookInstallTick = now;
                pendingFgRehookTick = 0;
            }

            // Direct GetAsyncKeyState polling fallback for borderless/fullscreen games that swallow hooks
            if (g_RecordingHotkeySlot == 0 && g_Daemon.hTrayWnd) {
                UINT curMods = GetCurrentHardwareModifiers();
                auto isHkPressed = [&](const HotkeyBinding& hk) -> bool {
                    if (hk.vk == 0 || curMods != hk.modifiers) return false;
                    SHORT st = GetAsyncKeyState((int)hk.vk);
                    if (hk.vk == VK_SNAPSHOT) {
                        return (st & 0x8001) != 0;
                    }
                    return (st & 0x8000) != 0;
                };

                bool downPrev   = isHkPressed(g_Daemon.hkPrevRegion);
                bool downReg    = isHkPressed(g_Daemon.hkRegionSnip);
                bool downFull   = isHkPressed(g_Daemon.hkFullSnap);
                bool downPin    = isHkPressed(g_Daemon.hkPinClipboard);
                bool downEdit   = isHkPressed(g_Daemon.hkEditClipboard);
                bool downSearch = isHkPressed(g_Daemon.hkSearchGoogle);

                if (downPrev && !wasDownPrev) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_PREV_REGION, 0, 0);
                } else if (downReg && !wasDownReg) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_REGION_SNIP, 0, 0);
                } else if (downFull && !wasDownFull) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_FULL_SNAP, 0, 0);
                } else if (downPin && !wasDownPin) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_PIN_CLIPBOARD, 0, 0);
                } else if (downEdit && !wasDownEdit) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_EDIT_CLIPBOARD, 0, 0);
                } else if (downSearch && !wasDownSearch) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_SEARCH_GOOGLE, 0, 0);
                }

                wasDownPrev   = downPrev;
                wasDownReg    = downReg;
                wasDownFull   = downFull;
                wasDownPin    = downPin;
                wasDownEdit   = downEdit;
                wasDownSearch = downSearch;
            }
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    KillTimer(nullptr, pollTimer);
    if (g_Daemon.hKeyHook) {
        UnhookWindowsHookEx(g_Daemon.hKeyHook);
        g_Daemon.hKeyHook = nullptr;
    }
    return 0;
}

