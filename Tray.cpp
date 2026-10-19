#include "PepperSnapInternal.h"

// Embedded Application Icon & Shell_NotifyIconW
// ----------------------------------------------------------------------------

HICON PepperSnapDaemon::CreateCustomTrayIcon() {
    if (hInst) {
        HICON hResIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
        if (hResIcon) return hResIcon;
    }
    Bitmap bmp(32, 32, PixelFormat32bppARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    SolidBrush badgeBrush(Color(255, 239, 68, 68));
    g.FillEllipse(&badgeBrush, 2, 2, 28, 28);

    Pen whitePen(Color(255, 255, 255, 255), 2.4f);
    g.DrawRectangle(&whitePen, 9, 9, 14, 14);
    g.DrawLine(&whitePen, 6, 9, 12, 9);
    g.DrawLine(&whitePen, 9, 6, 9, 12);
    g.DrawLine(&whitePen, 20, 23, 26, 23);
    g.DrawLine(&whitePen, 23, 20, 23, 26);

    HICON hIcon = nullptr;
    bmp.GetHICON(&hIcon);
    return hIcon ? hIcon : LoadIconW(nullptr, IDI_APPLICATION);
}

void PepperSnapDaemon::UpdateTrayTooltip() {
    wcsncpy_s(nid.szTip, L"PepperSnap", _TRUNCATE);
    if (nid.hWnd) {
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        Shell_NotifyIconW(NIM_MODIFY, &nid);
    }
}

void PepperSnapDaemon::InitTrayIcon() {
    if (hTrayIcon) {
        DestroyIcon(hTrayIcon);
        hTrayIcon = nullptr;
    }
    hTrayIcon = CreateCustomTrayIcon();
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hTrayWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = hTrayIcon;
    wcsncpy_s(nid.szTip, L"PepperSnap", _TRUNCATE);
    Shell_NotifyIconW(NIM_ADD, &nid);
}

// GetMessageW for the windows that run their own message loop while they are
// open: Options, Customize keys, the naming-syntax window, the shortcuts window
// and the clipboard-history prompt. It behaves identically, except that a
// retrieved WM_QUIT is posted straight back to the thread queue.
//
// The tray's "Exit" posts WM_QUIT from a menu handler, and when one of those
// windows is open it is that window's loop which is pumping, so it is that loop
// which would consume the quit. The outer loop in PepperSnap.cpp would then
// block for ever: the app gone from the tray, its window still on screen, and no
// way out but Task Manager. Handing the message back lets every nested loop
// unwind and still leaves the outermost one something to end on.
BOOL PepperSnapDaemon::GetMessageKeepQuit(MSG* msg) {
    BOOL got = GetMessageW(msg, nullptr, 0, 0);
    if (got == 0) {
        PostQuitMessage((int)msg->wParam);
    }
    return got;
}

void PepperSnapDaemon::ShowTrayToast(const std::wstring& title, const std::wstring& message, const std::wstring& clickFolder) {
    lastBalloonClickFolder = clickFolder;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    nid.dwInfoFlags &= ~NIIF_NOSOUND;
    // Clear any currently visible balloon first so Windows always plays the notification sound on consecutive captures
    nid.szInfoTitle[0] = L'\0';
    nid.szInfo[0] = L'\0';
    Shell_NotifyIconW(NIM_MODIFY, &nid);

    wcsncpy_s(nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid.szInfo, message.empty() ? L" " : message.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void PepperSnapDaemon::RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &nid);
    if (hTrayIcon) {
        DestroyIcon(hTrayIcon);
        hTrayIcon = nullptr;
    }
}

// ----------------------------------------------------------------------------
// GitHub Releases Update Checker (WinInet HTTPS + Scheduled Auto-Check)
// ----------------------------------------------------------------------------

struct UpdateCheckPayload {
    bool manualTrigger = false;
    bool networkSuccess = false;
    std::wstring latestTag;
    std::wstring releaseUrl;
};

long long PepperSnapDaemon::GetUpdateIntervalSeconds(UpdateCheckInterval iv) {
    switch (iv) {
        case UpdateCheckInterval::EveryDay:    return 86400LL;        // 1 day
        case UpdateCheckInterval::Every3Days:  return 259200LL;       // 3 days
        case UpdateCheckInterval::EveryWeek:   return 604800LL;       // 7 days
        case UpdateCheckInterval::Every2Weeks: return 1209600LL;      // 14 days
        case UpdateCheckInterval::EveryMonth:  return 2592000LL;      // 30 days
        default:                               return 86400LL;
    }
}

static std::vector<int> ParseVersionNumbers(const std::wstring& verStr) {
    std::vector<int> parts;
    std::wstring clean;
    bool startedDigits = false;
    for (wchar_t c : verStr) {
        if (c >= L'0' && c <= L'9') {
            clean.push_back(c);
            startedDigits = true;
        } else if (c == L'.' && startedDigits) {
            clean.push_back(L' ');
        } else if (startedDigits) {
            break;
        }
    }
    std::wstringstream ss(clean);
    int v = 0;
    while (ss >> v) {
        parts.push_back(v);
    }
    while (parts.size() < 4) parts.push_back(0);
    return parts;
}

int PepperSnapDaemon::CompareVersionStrings(const std::wstring& v1, const std::wstring& v2) {
    std::vector<int> p1 = ParseVersionNumbers(v1);
    std::vector<int> p2 = ParseVersionNumbers(v2);
    size_t n = std::max(p1.size(), p2.size());
    for (size_t i = 0; i < n; ++i) {
        int a = (i < p1.size()) ? p1[i] : 0;
        int b = (i < p2.size()) ? p2[i] : 0;
        if (a > b) return 1;
        if (a < b) return -1;
    }
    return 0;
}

static bool ParseHex4(const std::string& s, size_t pos, unsigned int& outVal) {
    if (pos + 4 > s.size()) return false;
    unsigned int val = 0;
    for (size_t k = 0; k < 4; ++k) {
        char c = s[pos + k];
        val <<= 4;
        if (c >= '0' && c <= '9') val |= (unsigned int)(c - '0');
        else if (c >= 'a' && c <= 'f') val |= (unsigned int)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val |= (unsigned int)(c - 'A' + 10);
        else return false;
    }
    outVal = val;
    return true;
}

static void AppendUtf8Codepoint(std::string& out, unsigned int cp) {
    if (cp <= 0x7F) {
        out.push_back((char)cp);
    } else if (cp <= 0x7FF) {
        out.push_back((char)(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back((char)(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back((char)(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    }
}

static bool ParseJsonStringToken(const std::string& json, size_t& ioPos, std::string& outStr) {
    outStr.clear();
    if (ioPos >= json.size() || json[ioPos] != '"') return false;
    size_t i = ioPos + 1;
    const size_t n = json.size();
    while (i < n) {
        unsigned char ch = (unsigned char)json[i++];
        if (ch == '"') {
            ioPos = i;
            return true;
        }
        if (ch < 0x20) {
            return false; // Unescaped control character in JSON string is malformed
        }
        if (ch != '\\') {
            outStr.push_back((char)ch);
            continue;
        }
        if (i >= n) return false;
        char esc = json[i++];
        switch (esc) {
            case '"':  outStr.push_back('"');  break;
            case '\\': outStr.push_back('\\'); break;
            case '/':  outStr.push_back('/');  break;
            case 'b':  outStr.push_back('\b'); break;
            case 'f':  outStr.push_back('\f'); break;
            case 'n':  outStr.push_back('\n'); break;
            case 'r':  outStr.push_back('\r'); break;
            case 't':  outStr.push_back('\t'); break;
            case 'u': {
                unsigned int u1 = 0;
                if (!ParseHex4(json, i, u1)) return false;
                i += 4;
                if (u1 >= 0xD800 && u1 <= 0xDBFF) {
                    if (i + 6 <= n && json[i] == '\\' && json[i + 1] == 'u') {
                        unsigned int u2 = 0;
                        if (!ParseHex4(json, i + 2, u2) || u2 < 0xDC00 || u2 > 0xDFFF) {
                            return false;
                        }
                        i += 6;
                        unsigned int cp = 0x10000U + (((u1 - 0xD800U) << 10) | (u2 - 0xDC00U));
                        AppendUtf8Codepoint(outStr, cp);
                    } else {
                        return false; // Unpaired high surrogate
                    }
                } else if (u1 >= 0xDC00 && u1 <= 0xDFFF) {
                    return false; // Unpaired low surrogate
                } else {
                    AppendUtf8Codepoint(outStr, u1);
                }
                break;
            }
            default:
                return false; // Invalid escape sequence
        }
    }
    return false;
}

static std::string ExtractJsonFieldValue(const std::string& json, const std::string& key) {
    if (json.empty() || key.empty()) return "";
    auto isJsonWs = [](char c) -> bool {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    };
    const size_t n = json.size();
    size_t i = 0;
    int depth = 0;
    std::string fallbackMatch;
    bool hasFallback = false;

    while (i < n) {
        while (i < n && isJsonWs(json[i])) ++i;
        if (i >= n) break;
        char c = json[i];
        if (c == '{' || c == '[') {
            ++depth;
            ++i;
            continue;
        }
        if (c == '}' || c == ']') {
            if (depth <= 0) return "";
            --depth;
            ++i;
            continue;
        }
        if (c == '"') {
            std::string token;
            if (!ParseJsonStringToken(json, i, token)) return "";
            while (i < n && isJsonWs(json[i])) ++i;
            if (i < n && json[i] == ':') {
                ++i;
                while (i < n && isJsonWs(json[i])) ++i;
                if (i >= n) return "";
                if (token == key) {
                    if (json[i] != '"') {
                        continue;
                    }
                    std::string val;
                    if (!ParseJsonStringToken(json, i, val)) return "";
                    if (depth <= 1) return val;
                    if (!hasFallback) {
                        fallbackMatch = val;
                        hasFallback = true;
                    }
                    continue;
                }
            }
            continue;
        }
        ++i;
    }
    return hasFallback ? fallbackMatch : "";
}

std::wstring Utf8ToWideStr(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (len <= 0) return L"";
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len);
    return out;
}

static bool IsSafeGithubHttpsUrl(const std::wstring& url) {
    if (url.size() < 19 || url.size() > 2048) return false;
    for (wchar_t c : url) {
        if (c <= 0x20 || c == L'"' || c == L'\'' || c == L'<' || c == L'>' || c == L'\\') return false;
    }
    if (_wcsnicmp(url.c_str(), L"https://github.com/", 19) != 0) return false;
    WCHAR hostBuf[256] = {0};
    WCHAR userBuf[128] = {0};
    WCHAR passBuf[128] = {0};
    std::vector<WCHAR> pathBuf(url.size() + 2, L'\0');
    URL_COMPONENTSW uc = { sizeof(uc) };
    uc.lpszHostName = hostBuf;
    uc.dwHostNameLength = 255;
    uc.lpszUserName = userBuf;
    uc.dwUserNameLength = 127;
    uc.lpszPassword = passBuf;
    uc.dwPasswordLength = 127;
    uc.lpszUrlPath = pathBuf.data();
    uc.dwUrlPathLength = (DWORD)(pathBuf.size() - 1);
    if (!InternetCrackUrlW(url.c_str(), (DWORD)url.size(), 0, &uc)) return false;
    if (uc.nScheme != INTERNET_SCHEME_HTTPS) return false;
    if (uc.dwUserNameLength != 0 || uc.dwPasswordLength != 0) return false;
    if (uc.nPort != INTERNET_DEFAULT_HTTPS_PORT) return false;
    if (_wcsicmp(hostBuf, L"github.com") != 0) return false;
    if (pathBuf[0] != L'/') return false;
    return true;
}

struct ScopedWinInetHandle {
    HINTERNET h = nullptr;
    ScopedWinInetHandle() = default;
    explicit ScopedWinInetHandle(HINTERNET handle) : h(handle) {}
    ~ScopedWinInetHandle() { if (h) InternetCloseHandle(h); }
    ScopedWinInetHandle(const ScopedWinInetHandle&) = delete;
    ScopedWinInetHandle& operator=(const ScopedWinInetHandle&) = delete;
    operator HINTERNET() const { return h; }
};

static DWORD WINAPI UpdateCheckWorkerThread(LPVOID lpParam) {
    bool manual = (lpParam != nullptr);
    std::unique_ptr<UpdateCheckPayload> result(new UpdateCheckPayload());
    result->manualTrigger = manual;
    result->networkSuccess = false;

    std::wstring repo = IsValidGithubRepoSlug(g_Daemon.updateGithubRepo)
        ? g_Daemon.updateGithubRepo
        : std::wstring(PepperSnapDaemon::DEFAULT_GITHUB_REPO);
    std::wstring apiUrl = L"https://api.github.com/repos/" + repo + L"/releases/latest";
    std::wstring fallbackHtmlUrl = L"https://github.com/" + repo + L"/releases/latest";
    result->releaseUrl = L"https://github.com/" + repo + L"/releases";

    std::wstring uaApp = L"PepperSnap/" + std::wstring(PepperSnapDaemon::APP_VERSION) + L" (Win32; +https://github.com)";
    std::wstring uaHdr = L"Accept: application/vnd.github+json\r\nUser-Agent: PepperSnap/" + std::wstring(PepperSnapDaemon::APP_VERSION) + L"\r\n";
    std::wstring uaWeb = L"User-Agent: PepperSnap/" + std::wstring(PepperSnapDaemon::APP_VERSION) + L"\r\n";

    ScopedWinInetHandle hInet(InternetOpenW(
        uaApp.c_str(),
        INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0
    ));
    if (hInet) {
        {
            ScopedWinInetHandle hUrl(InternetOpenUrlW(
                hInet, apiUrl.c_str(), uaHdr.c_str(), (DWORD)-1L,
                INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION,
                0
            ));
            if (hUrl) {
                std::string body;
                char buf[4096];
                DWORD bytesRead = 0;
                while (InternetReadFile(hUrl, buf, sizeof(buf), &bytesRead) && bytesRead > 0) {
                    body.append(buf, bytesRead);
                    if (body.size() > 262144) break;
                }

                std::string tag = ExtractJsonFieldValue(body, "tag_name");
                std::string htmlUrl = ExtractJsonFieldValue(body, "html_url");
                if (!tag.empty()) {
                    result->networkSuccess = true;
                    result->latestTag = Utf8ToWideStr(tag);
                    std::wstring candidateUrl = Utf8ToWideStr(htmlUrl);
                    if (IsSafeGithubHttpsUrl(candidateUrl)) {
                        result->releaseUrl = candidateUrl;
                    } else {
                        result->releaseUrl = fallbackHtmlUrl;
                    }
                }
            }
        }

        // Fallback: Check GitHub web redirect /releases/latest -> /releases/tag/<version>
        if (!result->networkSuccess) {
            ScopedWinInetHandle hWeb(InternetOpenUrlW(
                hInet, fallbackHtmlUrl.c_str(), uaWeb.c_str(), (DWORD)-1L,
                INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE,
                0
            ));
            if (hWeb) {
                std::vector<WCHAR> finalUrl(1024, L'\0');
                DWORD urlBytes = (DWORD)(finalUrl.size() * sizeof(WCHAR));
                BOOL qOk = InternetQueryOptionW(hWeb, INTERNET_OPTION_URL, finalUrl.data(), &urlBytes);
                if (!qOk && GetLastError() == ERROR_INSUFFICIENT_BUFFER && urlBytes > 0 && urlBytes <= 65536) {
                    finalUrl.resize((urlBytes / sizeof(WCHAR)) + 2, L'\0');
                    urlBytes = (DWORD)(finalUrl.size() * sizeof(WCHAR));
                    qOk = InternetQueryOptionW(hWeb, INTERNET_OPTION_URL, finalUrl.data(), &urlBytes);
                }
                if (qOk) {
                    std::wstring fUrl(finalUrl.data());
                    size_t tagPos = fUrl.find(L"/releases/tag/");
                    if (tagPos != std::wstring::npos && IsSafeGithubHttpsUrl(fUrl)) {
                        result->latestTag = fUrl.substr(tagPos + 14);
                        result->releaseUrl = fUrl;
                        result->networkSuccess = !result->latestTag.empty();
                    }
                }
            }
        }
    }

    if (g_Daemon.hTrayWnd && IsWindow(g_Daemon.hTrayWnd) &&
        PostMessageW(g_Daemon.hTrayWnd, WM_UPDATE_CHECK_RESULT, 0, (LPARAM)result.get())) {
        result.release();
    } else {
        g_Daemon.isCheckingUpdate = false;
    }
    return 0;
}

void PepperSnapDaemon::CheckForUpdatesAsync(bool manualUserTrigger) {
    if (isCheckingUpdate) return;
    isCheckingUpdate = true;
    HANDLE hThread = CreateThread(nullptr, 0, UpdateCheckWorkerThread, manualUserTrigger ? (LPVOID)1 : nullptr, 0, nullptr);
    if (hThread) {
        CloseHandle(hThread);
    } else {
        isCheckingUpdate = false;
    }
}

void PepperSnapDaemon::MaybeRunScheduledUpdateCheck() {
    if (!autoCheckUpdates || isCheckingUpdate) return;
    long long nowEpoch = (long long)_time64(nullptr);
    long long intervalSec = GetUpdateIntervalSeconds(updateInterval);
    if (lastUpdateCheckTime <= 0 || (nowEpoch - lastUpdateCheckTime) >= intervalSec) {
        lastUpdateCheckTime = nowEpoch;
        SaveSettings();
        CheckForUpdatesAsync(false);
    }
}

// ----------------------------------------------------------------------------
// System Tray Daemon Window Procedure
// ----------------------------------------------------------------------------

static bool ShouldTrayMenuSmoothAllPins() {
    int pinCount = 0;
    bool anyUnsmoothed = false;
    auto inspectPin = [&](HWND hp) {
        if (!hp || !IsWindow(hp)) return;
        PinnedWindowData* pData = (PinnedWindowData*)GetWindowLongPtrW(hp, GWLP_USERDATA);
        if (!pData || !pData->bmp) return;
        pinCount++;
        if (!pData->smoothImage) {
            anyUnsmoothed = true;
        }
    };
    for (HWND hp : g_Daemon.pinnedWindows) {
        inspectPin(hp);
    }
    HWND hPinExtra = nullptr;
    while ((hPinExtra = FindWindowExW(nullptr, hPinExtra, L"PepperSnapPinWnd", nullptr)) != nullptr) {
        if (std::find(g_Daemon.pinnedWindows.begin(), g_Daemon.pinnedWindows.end(), hPinExtra) == g_Daemon.pinnedWindows.end()) {
            inspectPin(hPinExtra);
        }
    }
    if (pinCount > 0) {
        return anyUnsmoothed;
    }
    return !g_Daemon.smoothPinnedImage;
}

static bool ShouldTrayMenuShowToolbarOnAllPins() {
    int pinCount = 0;
    bool anyToolbarHidden = false;
    auto inspectPin = [&](HWND hp) {
        if (!hp || !IsWindow(hp)) return;
        PinnedWindowData* pData = (PinnedWindowData*)GetWindowLongPtrW(hp, GWLP_USERDATA);
        if (!pData || !pData->bmp) return;
        pinCount++;
        if (pData->hideToolbar) {
            anyToolbarHidden = true;
        }
    };
    for (HWND hp : g_Daemon.pinnedWindows) {
        inspectPin(hp);
    }
    HWND hPinExtra = nullptr;
    while ((hPinExtra = FindWindowExW(nullptr, hPinExtra, L"PepperSnapPinWnd", nullptr)) != nullptr) {
        if (std::find(g_Daemon.pinnedWindows.begin(), g_Daemon.pinnedWindows.end(), hPinExtra) == g_Daemon.pinnedWindows.end()) {
            inspectPin(hPinExtra);
        }
    }
    if (pinCount > 0) {
        return anyToolbarHidden;
    }
    return g_Daemon.hidePinnedToolbar;
}

static bool ShouldTrayMenuShowOutlineOnAllPins() {
    int pinCount = 0;
    bool anyOutlineHidden = false;
    auto inspectPin = [&](HWND hp) {
        if (!hp || !IsWindow(hp)) return;
        PinnedWindowData* pData = (PinnedWindowData*)GetWindowLongPtrW(hp, GWLP_USERDATA);
        if (!pData || !pData->bmp) return;
        pinCount++;
        if (pData->hideOutline) {
            anyOutlineHidden = true;
        }
    };
    for (HWND hp : g_Daemon.pinnedWindows) {
        inspectPin(hp);
    }
    HWND hPinExtra = nullptr;
    while ((hPinExtra = FindWindowExW(nullptr, hPinExtra, L"PepperSnapPinWnd", nullptr)) != nullptr) {
        if (std::find(g_Daemon.pinnedWindows.begin(), g_Daemon.pinnedWindows.end(), hPinExtra) == g_Daemon.pinnedWindows.end()) {
            inspectPin(hPinExtra);
        }
    }
    if (pinCount > 0) {
        return anyOutlineHidden;
    }
    return g_Daemon.hidePinnedOutline;
}

static void ShowTrayContextMenu(HWND hWnd) {
    POINT anchorPt;
    GetCursorPos(&anchorPt);
    bool isReopen = false;

    for (;;) {
        HMENU hMenu = CreatePopupMenu();

        std::wstring regMenu  = L"Custom area\t" + PepperSnapDaemon::FormatHotkeyString(g_Daemon.hkRegionSnip);
        std::wstring fullMenu = L"Instant fullscreen\t" + PepperSnapDaemon::FormatHotkeyString(g_Daemon.hkFullSnap);
        std::wstring prevMenu = L"Instant save previous custom area\t" + PepperSnapDaemon::FormatHotkeyString(g_Daemon.hkPrevRegion);
        std::wstring pinClipMenu = L"Pin the image in clipboard on top\t" + PepperSnapDaemon::FormatHotkeyString(g_Daemon.hkPinClipboard);
        std::wstring editClipMenu = L"Edit the image in clipboard\t" + PepperSnapDaemon::FormatHotkeyString(g_Daemon.hkEditClipboard);
        std::wstring searchGoogleMenu = L"Search on web the image in clipboard\t" + PepperSnapDaemon::FormatHotkeyString(g_Daemon.hkSearchGoogle);
        const wchar_t* smoothPinsMenu = ShouldTrayMenuSmoothAllPins()
            ? L"Smooth all pinned image"
            : L"Unsmooth all pinned image";
        const wchar_t* toolbarPinsMenu = ShouldTrayMenuShowToolbarOnAllPins()
            ? L"Show toolbar on all pinned image"
            : L"Hide toolbar on all pinned image";
        const wchar_t* outlinePinsMenu = ShouldTrayMenuShowOutlineOnAllPins()
            ? L"Show outline on all pinned image"
            : L"Hide outline on all pinned image";

        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_REGION,            regMenu.c_str());
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_FULL,              fullMenu.c_str());
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_PREV_REGION,       prevMenu.c_str());
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_PIN_CLIPBOARD,     pinClipMenu.c_str());
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EDIT_CLIPBOARD,    editClipMenu.c_str());
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SEARCH_GOOGLE,     searchGoogleMenu.c_str());
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN_IMAGE,        L"Open image to edit with PepperSnap...");
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN_PIN_IMAGE,    L"Open image to pin on top...");
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SMOOTH_PINS,       smoothPinsMenu);
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SHOW_TOOLBAR_PINS, toolbarPinsMenu);
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SHOW_OUTLINE_PINS, outlinePinsMenu);
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_CLOSE_PINS,        L"Close all pinned image");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN_FOLDER,   L"Open save folder...");
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPTIONS,       L"Options...");
        AppendMenuW(hMenu, MF_STRING | (g_Daemon.IsRunAtStartupEnabled() ? MF_CHECKED : MF_UNCHECKED),
                    IDM_TRAY_STARTUP_RUN, L"Launch PepperSnap at Windows startup");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_CHECK_UPDATE,  L"Check for updates (GitHub releases)...");
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SHORTCUTS,     L"Keyboard shortcuts and info...");
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT,          L"Exit PepperSnap");

        SetForegroundWindow(hWnd);
        if (isReopen) {
            POINT curPt;
            if (GetCursorPos(&curPt)) {
                SetCursorPos(curPt.x, curPt.y);
            }
        }
        UINT tpmFlags = TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD |
                        (isReopen ? TPM_NOANIMATION : 0);
        int cmd = (int)TrackPopupMenu(hMenu, tpmFlags, anchorPt.x, anchorPt.y, 0, hWnd, nullptr);
        DestroyMenu(hMenu);

        if (cmd == IDM_TRAY_SMOOTH_PINS ||
            cmd == IDM_TRAY_SHOW_TOOLBAR_PINS ||
            cmd == IDM_TRAY_SHOW_OUTLINE_PINS) {
            SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
            isReopen = true;
            continue;
        }

        PostMessageW(hWnd, WM_NULL, 0, 0);
        if (cmd != 0) {
            PostMessageW(hWnd, WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
        }
        break;
    }
}

LRESULT CALLBACK TrayDaemonWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TIMER:
            if (wParam == TIMER_AUTO_UPDATE_CHECK) {
                g_Daemon.MaybeRunScheduledUpdateCheck();
            } else if (wParam == TIMER_TRAY_DELAY_REGION) {
                KillTimer(hWnd, TIMER_TRAY_DELAY_REGION);
                DwmFlush();
                g_Daemon.StartRegionSnipOverlay();
            } else if (wParam == TIMER_TRAY_DELAY_FULL) {
                KillTimer(hWnd, TIMER_TRAY_DELAY_FULL);
                DwmFlush();
                g_Daemon.InstantFullscreenCapture();
            } else if (wParam == TIMER_TRAY_DELAY_PREV) {
                KillTimer(hWnd, TIMER_TRAY_DELAY_PREV);
                DwmFlush();
                g_Daemon.InstantPreviousRegionCapture();
            } else if (wParam == TIMER_PROCESS_PIN_QUEUE) {
                KillTimer(hWnd, TIMER_PROCESS_PIN_QUEUE);
                PostMessageW(hWnd, WM_PROCESS_PIN_QUEUE, 0, 0);
            }
            return 0;
        case WM_UPDATE_CHECK_RESULT: {
            g_Daemon.isCheckingUpdate = false;
            std::unique_ptr<UpdateCheckPayload> res((UpdateCheckPayload*)lParam);
            if (!res) return 0;

            g_Daemon.lastUpdateCheckTime = (long long)_time64(nullptr);
            g_Daemon.SaveSettings();

            HWND hOwner = (g_Daemon.hOptionsWnd && IsWindow(g_Daemon.hOptionsWnd))
                ? g_Daemon.hOptionsWnd
                : (g_Daemon.hOverlayWnd ? g_Daemon.hOverlayWnd : hWnd);
            if (res->networkSuccess && !res->latestTag.empty()) {
                if (PepperSnapDaemon::CompareVersionStrings(res->latestTag, PepperSnapDaemon::APP_VERSION) > 0) {
                    std::wstring msgText =
                        L"A new version of PepperSnap (" + res->latestTag + L") is available on GitHub releases!\n\n"
                        L"Current version: v" + std::wstring(PepperSnapDaemon::APP_VERSION) + L"\n"
                        L"Latest release:  " + res->latestTag + L"\n\n"
                        L"Would you like to open the GitHub releases page to download the update?";
                    int ans = MessageBoxW(
                        hOwner, msgText.c_str(), L"PepperSnap update available",
                        MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND
                    );
                    if (ans == IDYES && IsSafeGithubHttpsUrl(res->releaseUrl)) {
                        ShellExecuteW(nullptr, L"open", res->releaseUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    }
                } else if (res->manualTrigger) {
                    std::wstring msgText =
                        L"You are running the latest version of PepperSnap (v" + std::wstring(PepperSnapDaemon::APP_VERSION) + L").\n\n"
                        L"Latest GitHub release: " + res->latestTag;
                    MessageBoxW(
                        hOwner, msgText.c_str(), L"PepperSnap — up to date",
                        MB_OK | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND
                    );
                }
            } else if (res->manualTrigger) {
                std::wstring safeRepo = IsValidGithubRepoSlug(g_Daemon.updateGithubRepo)
                    ? g_Daemon.updateGithubRepo
                    : std::wstring(PepperSnapDaemon::DEFAULT_GITHUB_REPO);
                std::wstring msgText =
                    L"Could not find a newer release tag on GitHub releases (or no releases have been published yet).\n\n"
                    L"Current version: v" + std::wstring(PepperSnapDaemon::APP_VERSION) + L"\n"
                    L"Repository: " + safeRepo + L"\n\n"
                    L"Would you like to open the GitHub releases page in your browser?";
                int ans = MessageBoxW(
                    hOwner, msgText.c_str(), L"PepperSnap — check for updates",
                    MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND
                );
                if (ans == IDYES && IsSafeGithubHttpsUrl(res->releaseUrl)) {
                    ShellExecuteW(nullptr, L"open", res->releaseUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }
            }
            return 0;
        }
        case WM_COPYDATA: {
            COPYDATASTRUCT* cds = (COPYDATASTRUCT*)lParam;
            if (cds && (cds->dwData == COPYDATA_OPEN_IMAGE || cds->dwData == COPYDATA_PIN_IMAGE || cds->dwData == COPYDATA_SEARCH_IMAGE) &&
                cds->lpData && cds->cbData >= sizeof(wchar_t)) {
                const wchar_t* wpath = (const wchar_t*)cds->lpData;
                size_t maxChars = cds->cbData / sizeof(wchar_t);
                std::wstring path(wpath, wcsnlen(wpath, maxChars));
                if (!path.empty()) {
                    if (cds->dwData == COPYDATA_PIN_IMAGE) {
                        g_Daemon.pendingPinFiles.push_back(path);
                        SetTimer(hWnd, TIMER_PROCESS_PIN_QUEUE, 90, nullptr);
                    } else if (cds->dwData == COPYDATA_SEARCH_IMAGE) {
                        g_Daemon.pendingSearchFile = path;
                        PostMessageW(hWnd, WM_PROCESS_PIN_QUEUE, 0, 0);
                    } else {
                        g_Daemon.pendingEditFile = path;
                        PostMessageW(hWnd, WM_PROCESS_PIN_QUEUE, 0, 0);
                    }
                }
                return TRUE;
            }
            break;
        }
        case WM_PROCESS_PIN_QUEUE: {
            KillTimer(hWnd, TIMER_PROCESS_PIN_QUEUE);
            static DWORD s_LastCopyDataPinTick = 0;
            static HWND s_LastSingleCopyDataPinWnd = nullptr;
            std::vector<std::wstring> toPin;
            toPin.swap(g_Daemon.pendingPinFiles);
            if (!toPin.empty()) {
                DWORD now = GetTickCount();
                bool isMultiBurstContinuation = (toPin.size() == 1) && (now - s_LastCopyDataPinTick <= 600);
                size_t beforeCount = g_Daemon.pinnedWindows.size();
                g_Daemon.OpenImageFilesToPinOnTop(toPin);
                if (g_Daemon.pinnedWindows.size() > beforeCount) {
                    if (isMultiBurstContinuation) {
                        if (s_LastSingleCopyDataPinWnd && IsWindow(s_LastSingleCopyDataPinWnd)) {
                            PinnedWindowData* dPrev = (PinnedWindowData*)GetWindowLongPtrW(s_LastSingleCopyDataPinWnd, GWLP_USERDATA);
                            SetPinnedWindowSelected(s_LastSingleCopyDataPinWnd, dPrev, true);
                        }
                        for (size_t i = beforeCount; i < g_Daemon.pinnedWindows.size(); ++i) {
                            HWND hNew = g_Daemon.pinnedWindows[i];
                            if (hNew && IsWindow(hNew)) {
                                PinnedWindowData* dNew = (PinnedWindowData*)GetWindowLongPtrW(hNew, GWLP_USERDATA);
                                SetPinnedWindowSelected(hNew, dNew, true);
                            }
                        }
                        s_LastSingleCopyDataPinWnd = nullptr;
                    } else if (toPin.size() == 1) {
                        s_LastSingleCopyDataPinWnd = g_Daemon.pinnedWindows.back();
                    } else {
                        s_LastSingleCopyDataPinWnd = nullptr;
                    }
                    s_LastCopyDataPinTick = GetTickCount();
                }
            }
            if (!g_Daemon.pendingEditFile.empty()) {
                std::wstring editPath = g_Daemon.pendingEditFile;
                g_Daemon.pendingEditFile.clear();
                g_Daemon.OpenImageFileIntoOverlay(editPath);
            }
            if (!g_Daemon.pendingSearchFile.empty()) {
                std::wstring searchPath = g_Daemon.pendingSearchFile;
                g_Daemon.pendingSearchFile.clear();
                g_Daemon.SearchImageFileOnGoogle(searchPath);
            }
            return 0;
        }
        case WM_TRIGGER_REGION_SNIP: {
            DWORD now = GetTickCount();
            if (now - g_LastHotkeyTriggerTick >= 250) {
                g_LastHotkeyTriggerTick = now;
                g_Daemon.StartRegionSnipOverlay();
            }
            return 0;
        }
        case WM_TRIGGER_FULL_SNAP: {
            DWORD now = GetTickCount();
            if (now - g_LastHotkeyTriggerTick >= 250) {
                g_LastHotkeyTriggerTick = now;
                g_Daemon.InstantFullscreenCapture();
            }
            return 0;
        }
        case WM_TRIGGER_PREV_REGION: {
            DWORD now = GetTickCount();
            if (now - g_LastHotkeyTriggerTick >= 250) {
                g_LastHotkeyTriggerTick = now;
                g_Daemon.InstantPreviousRegionCapture();
            }
            return 0;
        }
        case WM_TRIGGER_PIN_CLIPBOARD: {
            DWORD now = GetTickCount();
            if (now - g_LastHotkeyTriggerTick >= 250) {
                g_LastHotkeyTriggerTick = now;
                g_Daemon.PinClipboardImageOnTop();
            }
            return 0;
        }
        case WM_TRIGGER_EDIT_CLIPBOARD: {
            DWORD now = GetTickCount();
            if (now - g_LastHotkeyTriggerTick >= 250) {
                g_LastHotkeyTriggerTick = now;
                g_Daemon.EditClipboardImage();
            }
            return 0;
        }
        case WM_TRIGGER_SEARCH_GOOGLE: {
            DWORD now = GetTickCount();
            if (now - g_LastHotkeyTriggerTick >= 250) {
                g_LastHotkeyTriggerTick = now;
                g_Daemon.SearchClipboardImageOnGoogle();
            }
            return 0;
        }
        case WM_HOTKEY:
            if (g_RecordingHotkeySlot != 0) return 0;
            if (wParam == HK_REGION_SNIP) PostMessageW(hWnd, WM_TRIGGER_REGION_SNIP, 0, 0);
            else if (wParam == HK_FULL_SNAP) PostMessageW(hWnd, WM_TRIGGER_FULL_SNAP, 0, 0);
            else if (wParam == HK_PREV_REGION) PostMessageW(hWnd, WM_TRIGGER_PREV_REGION, 0, 0);
            else if (wParam == HK_PIN_CLIPBOARD) PostMessageW(hWnd, WM_TRIGGER_PIN_CLIPBOARD, 0, 0);
            else if (wParam == HK_EDIT_CLIPBOARD) PostMessageW(hWnd, WM_TRIGGER_EDIT_CLIPBOARD, 0, 0);
            else if (wParam == HK_SEARCH_GOOGLE) PostMessageW(hWnd, WM_TRIGGER_SEARCH_GOOGLE, 0, 0);
            return 0;
        case WM_TRAYICON:
            if (lParam == WM_LBUTTONUP) SetTimer(hWnd, TIMER_TRAY_DELAY_REGION, 400, nullptr);
            else if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) ShowTrayContextMenu(hWnd);
            else if (lParam == NIN_BALLOONUSERCLICK) {
                if (!g_Daemon.lastBalloonClickFolder.empty()) {
                    CreateDirectoryW(g_Daemon.lastBalloonClickFolder.c_str(), nullptr);
                    ShellExecuteW(nullptr, L"open", g_Daemon.lastBalloonClickFolder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                } else if (!g_Daemon.lastSavedFilePath.empty()) {
                    std::wstring param = L"/select,\"" + g_Daemon.lastSavedFilePath + L"\"";
                    ShellExecuteW(nullptr, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);
                } else {
                    ShellExecuteW(nullptr, L"open", g_Daemon.saveFolder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }
            }
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_TRAY_REGION:         SetTimer(hWnd, TIMER_TRAY_DELAY_REGION, 400, nullptr); break;
                case IDM_TRAY_FULL:           SetTimer(hWnd, TIMER_TRAY_DELAY_FULL, 400, nullptr); break;
                case IDM_TRAY_PREV_REGION:    SetTimer(hWnd, TIMER_TRAY_DELAY_PREV, 400, nullptr); break;
                case IDM_TRAY_OPEN_IMAGE:     g_Daemon.OpenImageIntoOverlay(); break;
                case IDM_TRAY_OPEN_PIN_IMAGE: g_Daemon.OpenImageToPinOnTop(); break;
                case IDM_TRAY_PIN_CLIPBOARD:  g_Daemon.PinClipboardImageOnTop(); break;
                case IDM_TRAY_EDIT_CLIPBOARD: g_Daemon.EditClipboardImage(); break;
                case IDM_TRAY_SEARCH_GOOGLE:  g_Daemon.SearchClipboardImageOnGoogle(); break;
                case IDM_TRAY_OPEN_FOLDER:
                    CreateDirectoryW(g_Daemon.saveFolder.c_str(), nullptr);
                    ShellExecuteW(nullptr, L"open", g_Daemon.saveFolder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    break;
                case IDM_TRAY_OPTIONS:        g_Daemon.ShowOptionsModal(); break;
                case IDM_TRAY_STARTUP_RUN:    g_Daemon.ToggleRunAtStartup(); break;
                case IDM_TRAY_SMOOTH_PINS: {
                    bool smoothNow = ShouldTrayMenuSmoothAllPins();
                    g_Daemon.smoothPinnedImage = smoothNow;
                    g_Daemon.SaveSettings();
                    auto setSmoothOnPin = [&](HWND hp) {
                        if (!hp || !IsWindow(hp)) return;
                        PinnedWindowData* pData = (PinnedWindowData*)GetWindowLongPtrW(hp, GWLP_USERDATA);
                        if (!pData || !pData->bmp) return;
                        if (pData->smoothImage != smoothNow) {
                            pData->smoothImage = smoothNow;
                            int imgW = 32, imgH = 32;
                            GetPinnedScaledDims(pData, imgW, imgH);
                            EnsurePinnedWindowCache(pData, imgW, imgH, true);
                            RenderPinnedLayeredWindow(hp, pData);
                        }
                    };
                    for (HWND hp : g_Daemon.pinnedWindows) {
                        setSmoothOnPin(hp);
                    }
                    HWND hPinExtra = nullptr;
                    while ((hPinExtra = FindWindowExW(nullptr, hPinExtra, L"PepperSnapPinWnd", nullptr)) != nullptr) {
                        setSmoothOnPin(hPinExtra);
                    }
                    break;
                }
                case IDM_TRAY_SHOW_TOOLBAR_PINS: {
                    bool showToolbarNow = ShouldTrayMenuShowToolbarOnAllPins();
                    bool targetHideToolbar = !showToolbarNow;
                    g_Daemon.hidePinnedToolbar = targetHideToolbar;
                    g_Daemon.SaveSettings();
                    if (targetHideToolbar) {
                        HidePinnedBubbleTooltip();
                    }
                    auto setToolbarOnPin = [&](HWND hp) {
                        if (!hp || !IsWindow(hp)) return;
                        PinnedWindowData* pData = (PinnedWindowData*)GetWindowLongPtrW(hp, GWLP_USERDATA);
                        if (!pData || !pData->bmp) return;
                        if (pData->hideToolbar != targetHideToolbar) {
                            pData->hideToolbar = targetHideToolbar;
                            UpdatePinnedWindowLayout(hp, pData, true);
                            RenderPinnedLayeredWindow(hp, pData);
                        }
                    };
                    for (HWND hp : g_Daemon.pinnedWindows) {
                        setToolbarOnPin(hp);
                    }
                    HWND hPinExtra = nullptr;
                    while ((hPinExtra = FindWindowExW(nullptr, hPinExtra, L"PepperSnapPinWnd", nullptr)) != nullptr) {
                        setToolbarOnPin(hPinExtra);
                    }
                    break;
                }
                case IDM_TRAY_SHOW_OUTLINE_PINS: {
                    bool showOutlineNow = ShouldTrayMenuShowOutlineOnAllPins();
                    bool targetHideOutline = !showOutlineNow;
                    g_Daemon.hidePinnedOutline = targetHideOutline;
                    g_Daemon.SaveSettings();
                    auto setOutlineOnPin = [&](HWND hp) {
                        if (!hp || !IsWindow(hp)) return;
                        PinnedWindowData* pData = (PinnedWindowData*)GetWindowLongPtrW(hp, GWLP_USERDATA);
                        if (!pData || !pData->bmp) return;
                        if (pData->hideOutline != targetHideOutline) {
                            pData->hideOutline = targetHideOutline;
                            int imgW = 32, imgH = 32;
                            GetPinnedScaledDims(pData, imgW, imgH);
                            EnsurePinnedWindowCache(pData, imgW, imgH, true);
                            UpdatePinnedWindowLayout(hp, pData, false);
                            RenderPinnedLayeredWindow(hp, pData);
                        }
                    };
                    for (HWND hp : g_Daemon.pinnedWindows) {
                        setOutlineOnPin(hp);
                    }
                    HWND hPinExtra = nullptr;
                    while ((hPinExtra = FindWindowExW(nullptr, hPinExtra, L"PepperSnapPinWnd", nullptr)) != nullptr) {
                        setOutlineOnPin(hPinExtra);
                    }
                    break;
                }
                case IDM_TRAY_CLOSE_PINS: {
                    HidePinnedBubbleTooltip();
                    std::vector<HWND> pinsToClose = g_Daemon.pinnedWindows;
                    g_Daemon.pinnedWindows.clear();
                    for (HWND hp : pinsToClose) {
                        if (hp && IsWindow(hp)) {
                            DestroyWindow(hp);
                        }
                    }
                    HWND hPinExtra = nullptr;
                    while ((hPinExtra = FindWindowExW(nullptr, nullptr, L"PepperSnapPinWnd", nullptr)) != nullptr) {
                        if (!DestroyWindow(hPinExtra)) {
                            SendMessageW(hPinExtra, WM_CLOSE, 0, 0);
                            break;
                        }
                    }
                    break;
                }
                case IDM_TRAY_CHECK_UPDATE:   g_Daemon.CheckForUpdatesAsync(true); break;
                case IDM_TRAY_SHORTCUTS:      g_Daemon.ShowShortcutsModal(); break;
                case IDM_TRAY_EXIT:
                    g_Daemon.SaveSettings();
                    g_Daemon.DiscardSavedEditSession();
                    g_Daemon.CloseRegionSnipOverlay();
                    g_Daemon.RemoveTrayIcon();
                    PostQuitMessage(0);
                    break;
            }
            return 0;
        case WM_DESTROY:
            KillTimer(hWnd, TIMER_AUTO_UPDATE_CHECK);
            g_Daemon.RemoveTrayIcon();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

