#include "PepperSnapInternal.h"

PepperSnapDaemon g_Daemon;

// ----------------------------------------------------------------------------
// Timestamp & Path Utilities
// ----------------------------------------------------------------------------

void ReplaceStrW(std::wstring& str, const std::wstring& from, const std::wstring& to) {
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::wstring::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
}

std::wstring GetModuleFilePathDynamic(HMODULE hMod) {
    std::vector<WCHAR> buf(MAX_PATH, L'\0');
    while (buf.size() <= 32768) {
        DWORD len = GetModuleFileNameW(hMod, buf.data(), (DWORD)buf.size());
        if (len == 0) return L"";
        if (len < buf.size() - 1) {
            return std::wstring(buf.data(), len);
        }
        buf.resize(buf.size() * 2, L'\0');
    }
    return L"";
}

std::wstring QueryProcessFullImagePathDynamic(HANDLE hProc) {
    if (!hProc) return L"";
    std::vector<WCHAR> buf(MAX_PATH, L'\0');
    while (buf.size() <= 32768) {
        DWORD cch = (DWORD)buf.size();
        if (QueryFullProcessImageNameW(hProc, 0, buf.data(), &cch)) {
            return std::wstring(buf.data(), cch);
        }
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) break;
        buf.resize(buf.size() * 2, L'\0');
    }
    return L"";
}

std::wstring GetWindowTextDynamic(HWND hWnd) {
    if (!hWnd || !IsWindow(hWnd)) return L"";
    int len = GetWindowTextLengthW(hWnd);
    if (len <= 0) return L"";
    std::vector<WCHAR> buf((size_t)len + 2, L'\0');
    int got = GetWindowTextW(hWnd, buf.data(), len + 1);
    if (got <= 0) return L"";
    return std::wstring(buf.data(), (size_t)got);
}





// ----------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR lpCmdLine, int) {
    SetProcessDPIAware();

    bool cliElevate = false;
    bool cliNoElevate = false;
    bool cliUninstall = false;
    UINT cliMsg = 0;
    WPARAM cliWParam = 0;

    std::wstring cliEditFilePath;
    std::wstring cliSearchFilePath;
    std::vector<std::wstring> cliPinFilePaths;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            std::wstring low = arg;
            for (wchar_t& c : low) c = (wchar_t)towlower(c);
            if (low == L"--noelevate" || low == L"/noelevate") {
                cliNoElevate = true;
                continue;
            }
            if (low == L"--elevate" || low == L"/elevate") {
                cliElevate = true;
                continue;
            }
            if (low == L"--uninstall" || low == L"/uninstall") {
                cliUninstall = true;
                continue;
            }
            if (low == L"--fullscreen" || low == L"/fullscreen") {
                cliMsg = WM_TRIGGER_FULL_SNAP;
                continue;
            }
            if (low == L"--prevregion" || low == L"/prevregion") {
                cliMsg = WM_TRIGGER_PREV_REGION;
                continue;
            }
            if (low == L"--options" || low == L"/options") {
                cliMsg = WM_COMMAND;
                cliWParam = IDM_TRAY_OPTIONS;
                continue;
            }
            if (low == L"--help" || low == L"/help" || low == L"/?") {
                cliMsg = WM_COMMAND;
                cliWParam = IDM_TRAY_SHORTCUTS;
                continue;
            }
            if (low == L"--exit" || low == L"/exit" || low == L"--quit") {
                cliMsg = WM_COMMAND;
                cliWParam = IDM_TRAY_EXIT;
                continue;
            }
            if (low == L"--region" || low == L"/region") {
                cliMsg = WM_TRIGGER_REGION_SNIP;
                continue;
            }
            if ((low == L"--pin" || low == L"/pin") && i + 1 < argc) {
                while (i + 1 < argc) {
                    std::wstring nextArg = argv[i + 1];
                    std::wstring nextLow = nextArg;
                    for (wchar_t& c : nextLow) c = (wchar_t)towlower(c);
                    if (nextLow == L"--noelevate" || nextLow == L"/noelevate") {
                        cliNoElevate = true;
                        ++i;
                        continue;
                    }
                    if (nextLow == L"--elevate" || nextLow == L"/elevate") {
                        cliElevate = true;
                        ++i;
                        continue;
                    }
                    if (!nextArg.empty() && (nextArg[0] == L'-' || nextArg[0] == L'/')) {
                        break;
                    }
                    cliPinFilePaths.push_back(nextArg);
                    ++i;
                }
                break;
            } else if ((low == L"--search" || low == L"/search") && i + 1 < argc) {
                std::wstring resolvedSearchPath;
                if (IsValidExistingImageFilePath(argv[i + 1], &resolvedSearchPath)) {
                    cliSearchFilePath = resolvedSearchPath;
                }
                ++i;
            } else if (low == L"--edit" || low == L"/edit" || low == L"--open" || low == L"/open") {
                if (i + 1 < argc) {
                    cliEditFilePath = argv[i + 1];
                    ++i;
                } else if (low == L"--open" || low == L"/open") {
                    cliMsg = WM_COMMAND;
                    cliWParam = IDM_TRAY_OPEN_IMAGE;
                }
            } else if (!arg.empty() && arg[0] != L'-' && arg[0] != L'/' && GetFileAttributesW(arg.c_str()) != INVALID_FILE_ATTRIBUTES) {
                cliEditFilePath = arg;
            }
        }
        LocalFree(argv);
    }

    if (cliUninstall) {
        PepperSnapDaemon::UnregisterImageContextMenu();
        return 0;
    }

    std::wstring currentDaemonTitle = L"PepperSnap v" + std::wstring(PepperSnapDaemon::APP_VERSION) +
        L" [" + Utf8ToWideStr(std::string(__DATE__) + " " + std::string(__TIME__)) + L"]";

    auto forwardToRunningDaemon = [&](HWND hExisting) {
        if (!cliPinFilePaths.empty()) {
            for (const auto& pinPath : cliPinFilePaths) {
                if (pinPath.empty()) continue;
                COPYDATASTRUCT cds = {0};
                cds.dwData = COPYDATA_PIN_IMAGE;
                cds.cbData = (DWORD)((pinPath.size() + 1) * sizeof(wchar_t));
                cds.lpData = (PVOID)pinPath.c_str();
                SendMessageTimeoutW(hExisting, WM_COPYDATA, 0, (LPARAM)&cds, SMTO_ABORTIFHUNG, 5000, nullptr);
            }
        } else if (!cliSearchFilePath.empty()) {
            COPYDATASTRUCT cds = {0};
            cds.dwData = COPYDATA_SEARCH_IMAGE;
            cds.cbData = (DWORD)((cliSearchFilePath.size() + 1) * sizeof(wchar_t));
            cds.lpData = (PVOID)cliSearchFilePath.c_str();
            SendMessageTimeoutW(hExisting, WM_COPYDATA, 0, (LPARAM)&cds, SMTO_ABORTIFHUNG, 5000, nullptr);
        } else if (!cliEditFilePath.empty()) {
            COPYDATASTRUCT cds = {0};
            cds.dwData = COPYDATA_OPEN_IMAGE;
            cds.cbData = (DWORD)((cliEditFilePath.size() + 1) * sizeof(wchar_t));
            cds.lpData = (PVOID)cliEditFilePath.c_str();
            SendMessageTimeoutW(hExisting, WM_COPYDATA, 0, (LPARAM)&cds, SMTO_ABORTIFHUNG, 5000, nullptr);
        } else if (cliMsg != 0) {
            PostMessageW(hExisting, cliMsg, cliWParam, 0);
        } else {
            PostMessageW(hExisting, WM_TRIGGER_REGION_SNIP, 0, 0);
        }
    };

    // Serialize multi-select Explorer CLI invocations so concurrent "--pin" processes forward cleanly in order
    HANDLE hCliForwardMutex = nullptr;
    if (!cliNoElevate) {
        hCliForwardMutex = CreateMutexW(nullptr, FALSE, L"Local\\PepperSnap_CLI_Forward_Mutex");
        if (hCliForwardMutex) {
            WaitForSingleObject(hCliForwardMutex, 10000);
        }
    }
    auto releaseCliForwardMutex = [&]() {
        if (hCliForwardMutex) {
            ReleaseMutex(hCliForwardMutex);
            CloseHandle(hCliForwardMutex);
            hCliForwardMutex = nullptr;
        }
    };

    // Run elevated by default unless --noelevate is passed or RunElevated=0 is configured in settings.ini
    std::wstring iniPreCheckPath = PepperSnapDaemon::GetAppDataSettingsFile();
    bool iniRunElevated = (GetPrivateProfileIntW(L"PepperSnap", L"RunElevated", 1, iniPreCheckPath.c_str()) != 0);
    bool shouldElevate = !cliNoElevate && (cliElevate || iniRunElevated);
    if (shouldElevate && !IsUserAnAdmin()) {
        // A daemon is already running elevated, so hand the request over instead
        // of re-launching and asking for admin permission again. This is not an
        // elevation bypass - nothing is elevated that wasn't already; it just
        // avoids requesting a new elevation that would only be wasted.
        if (HWND hRunning = FindWindowW(L"PepperSnapTrayDaemonClass", nullptr)) {
            forwardToRunningDaemon(hRunning);
            releaseCliForwardMutex();
            return 0;
        }

        std::wstring selfPath = GetModuleFilePathDynamic(nullptr);
        if (!selfPath.empty()) {
            std::wstring args = lpCmdLine ? lpCmdLine : L"";
            if (!args.empty()) args += L" ";
            args += L"--noelevate";
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.lpVerb = L"runas";
            sei.lpFile = selfPath.c_str();
            sei.lpParameters = args.c_str();
            sei.nShow = SW_SHOWNORMAL;
            if (ShellExecuteExW(&sei)) {
                for (int w = 0; w < 125; ++w) {
                    if (FindWindowW(L"PepperSnapTrayDaemonClass", nullptr) != nullptr) break;
                    Sleep(40);
                }
                releaseCliForwardMutex();
                return 0;
            }
        }
    }

    // Standard single-instance mutex check
    HANDLE hDaemonMutex = CreateMutexW(nullptr, TRUE, L"Global\\PepperSnap_TrayDaemon_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExisting = FindWindowW(L"PepperSnapTrayDaemonClass", nullptr);
        if (!hExisting) {
            for (int w = 0; w < 40; ++w) {
                hExisting = FindWindowW(L"PepperSnapTrayDaemonClass", nullptr);
                if (hExisting) break;
                Sleep(30);
            }
        }
        if (hExisting) {
            forwardToRunningDaemon(hExisting);
        }
        releaseCliForwardMutex();
        if (hDaemonMutex) CloseHandle(hDaemonMutex);
        return 0;
    }

    if (cliMsg == WM_COMMAND && cliWParam == IDM_TRAY_EXIT) {
        releaseCliForwardMutex();
        if (hDaemonMutex) CloseHandle(hDaemonMutex);
        return 0;
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    InitCommonControls();
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&g_Daemon.gdiplusToken, &gdiplusStartupInput, nullptr);

    g_Daemon.hInst = hInstance;
    g_Daemon.InitPaths();
    g_Daemon.InitCustomCursors();

    HICON hAppIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));
    if (!hAppIcon) hAppIcon = g_Daemon.CreateCustomTrayIcon();

    WNDCLASSEXW wcTray = { sizeof(WNDCLASSEXW) };
    wcTray.lpfnWndProc = TrayDaemonWndProc;
    wcTray.hInstance = hInstance;
    wcTray.hIcon = hAppIcon;
    wcTray.hIconSm = hAppIcon;
    wcTray.lpszClassName = L"PepperSnapTrayDaemonClass";
    RegisterClassExW(&wcTray);

    WNDCLASSEXW wcOv = { sizeof(WNDCLASSEXW) };
    wcOv.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcOv.lpfnWndProc = OverlayWndProc;
    wcOv.hInstance = hInstance;
    wcOv.hIcon = hAppIcon;
    wcOv.hIconSm = hAppIcon;
    wcOv.hCursor = nullptr; // Managed dynamically via WM_SETCURSOR & UpdateOverlayCursor
    wcOv.lpszClassName = L"PepperSnapOverlayWnd";
    RegisterClassExW(&wcOv);

    WNDCLASSEXW wcPin = { sizeof(WNDCLASSEXW) };
    wcPin.style = CS_DBLCLKS;
    wcPin.lpfnWndProc = PinWndProc;
    wcPin.hInstance = hInstance;
    wcPin.hIcon = hAppIcon;
    wcPin.hIconSm = hAppIcon;
    wcPin.hCursor = nullptr;
    wcPin.lpszClassName = L"PepperSnapPinWnd";
    RegisterClassExW(&wcPin);

    g_Daemon.hTrayWnd = CreateWindowExW(
        0, L"PepperSnapTrayDaemonClass", currentDaemonTitle.c_str(),
        0, 0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr
    );
    ChangeWindowMessageFilterEx(g_Daemon.hTrayWnd, WM_COPYDATA, MSGFLT_ALLOW, nullptr);
    ChangeWindowMessageFilterEx(g_Daemon.hTrayWnd, WM_TRIGGER_REGION_SNIP, MSGFLT_ALLOW, nullptr);
    ChangeWindowMessageFilterEx(g_Daemon.hTrayWnd, WM_TRIGGER_FULL_SNAP, MSGFLT_ALLOW, nullptr);
    ChangeWindowMessageFilterEx(g_Daemon.hTrayWnd, WM_TRIGGER_PREV_REGION, MSGFLT_ALLOW, nullptr);
    ChangeWindowMessageFilterEx(g_Daemon.hTrayWnd, WM_TRIGGER_PIN_CLIPBOARD, MSGFLT_ALLOW, nullptr);
    ChangeWindowMessageFilterEx(g_Daemon.hTrayWnd, WM_TRIGGER_EDIT_CLIPBOARD, MSGFLT_ALLOW, nullptr);
    ChangeWindowMessageFilterEx(g_Daemon.hTrayWnd, WM_TRIGGER_SEARCH_GOOGLE, MSGFLT_ALLOW, nullptr);
    ChangeWindowMessageFilterEx(g_Daemon.hTrayWnd, WM_COMMAND, MSGFLT_ALLOW, nullptr);

    if (!g_Daemon.disableRightClickIntegration) {
        PepperSnapDaemon::RegisterImageContextMenu();
    } else {
        PepperSnapDaemon::UnregisterImageContextMenu();
    }
    g_Daemon.InitTrayIcon();
    g_Daemon.ApplyGlobalHotkeys();

    HANDLE hInputThread = CreateThread(nullptr, 0, InputHookWorkerThread, nullptr, 0, &g_InputHookThreadId);

    // Periodic background timer (every 30 minutes) + initial startup check for scheduled GitHub Releases updates
    SetTimer(g_Daemon.hTrayWnd, TIMER_AUTO_UPDATE_CHECK, 30 * 60 * 1000, nullptr);
    g_Daemon.MaybeRunScheduledUpdateCheck();

    releaseCliForwardMutex();

    if (!cliPinFilePaths.empty()) {
        g_Daemon.OpenImageFilesToPinOnTop(cliPinFilePaths);
    } else if (!cliSearchFilePath.empty()) {
        g_Daemon.SearchImageFileOnGoogle(cliSearchFilePath);
    } else if (!cliEditFilePath.empty()) {
        g_Daemon.OpenImageFileIntoOverlay(cliEditFilePath);
    } else if (cliMsg != 0) {
        PostMessageW(g_Daemon.hTrayWnd, cliMsg, cliWParam, 0);
    } else {
        g_Daemon.ShowTrayToast(
            L"PepperSnap v" + std::wstring(PepperSnapDaemon::APP_VERSION) + L" active in system tray",
            L"• " + PepperSnapDaemon::FormatHotkeyString(g_Daemon.hkRegionSnip) + L": Custom area\n"
            L"• " + PepperSnapDaemon::FormatHotkeyString(g_Daemon.hkFullSnap) + L": Instant fullscreen\n"
            L"• " + PepperSnapDaemon::FormatHotkeyString(g_Daemon.hkPrevRegion) + L": Instant save previous custom area"
        );
        g_Daemon.CheckAndPromptClipboardHistory();
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_InputHookThreadId != 0) {
        PostThreadMessageW(g_InputHookThreadId, WM_QUIT, 0, 0);
    }
    if (hInputThread) {
        WaitForSingleObject(hInputThread, 500);
        CloseHandle(hInputThread);
    }
    if (g_Daemon.hKeyHook) UnhookWindowsHookEx(g_Daemon.hKeyHook);
    UnregisterHotKey(g_Daemon.hTrayWnd, HK_REGION_SNIP);
    UnregisterHotKey(g_Daemon.hTrayWnd, HK_FULL_SNAP);
    UnregisterHotKey(g_Daemon.hTrayWnd, HK_PREV_REGION);
    UnregisterHotKey(g_Daemon.hTrayWnd, HK_PIN_CLIPBOARD);
    UnregisterHotKey(g_Daemon.hTrayWnd, HK_EDIT_CLIPBOARD);
    UnregisterHotKey(g_Daemon.hTrayWnd, HK_SEARCH_GOOGLE);

    g_Daemon.DestroyCustomCursors();
    GdiplusShutdown(g_Daemon.gdiplusToken);
    CoUninitialize();
    if (hDaemonMutex) CloseHandle(hDaemonMutex);
    return (int)msg.wParam;
}

int main() {
    return wWinMain(GetModuleHandleW(nullptr), nullptr, GetCommandLineW(), SW_SHOWDEFAULT);
}
