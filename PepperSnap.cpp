#include "PepperSnap.h"

class PepperSnapDaemon {
public:
    HINSTANCE hInst = nullptr;
    HWND hTrayWnd = nullptr;
    HWND hOverlayWnd = nullptr;
    HHOOK hKeyHook = nullptr;
    HICON hTrayIcon = nullptr;
    HCURSOR hCurPen = nullptr;
    HCURSOR hCurStabilo = nullptr;
    NOTIFYICONDATAW nid = {0};
    ULONG_PTR gdiplusToken = 0;
    std::vector<HWND> pinnedWindows;

    // Configuration (Desktop default, JPEG default, unified Options modal)
    std::wstring saveFolder;
    std::wstring namingPattern = L"PepperSnap_{YYYY}-{MM}-{DD}_{HH}-{mm}-{ss}";
    static constexpr const wchar_t* DEFAULT_NAMING_PATTERN = L"PepperSnap_{YYYY}-{MM}-{DD}_{HH}-{mm}-{ss}";
    ImageFormat regionFormat = ImageFormat::JPEG;
    ImageFormat fullscreenFormat = ImageFormat::JPEG;
    ImageFormat copyFormat = ImageFormat::JPEG;
    int jpegQuality = 95;
    bool nonStackingHighlighter = true;
    bool penSmoothingEnabled = true;
    int penSmoothingStrength = 50; // 5% to 100%
    std::wstring lastSavedFilePath;
    int captureCounter = 1;
    bool autoSaveOnCopy = true;
    bool doNotCopyOnSave = false;

    // Virtual Screen Metrics
    int vScreenX = 0;
    int vScreenY = 0;
    int vScreenW = 1920;
    int vScreenH = 1080;

    // Active Overlay Session State
    Bitmap* frozenDesktopBmp = nullptr;
    bool hasSelection = false;
    RECT selRect{0, 0, 0, 0};
    RECT dimPillRect{0, 0, 0, 0};
    RECT customStrokeRect{0, 0, 0, 0};
    DragMode dragMode = DragMode::None;
    POINT dragStartPt{0, 0};
    RECT dragOrigRect{0, 0, 0, 0};
    POINT mousePt{0, 0};

    // Typeable Size Indicator State (With Caret & Partial Selection)
    bool isEditingSize = false;
    bool isDraggingSizeText = false;
    std::wstring editingSizeText;
    size_t sizeCaretPos = 0;
    size_t sizeSelAnchor = 0;

    // Typeable Custom Stroke / Text Size State (Right of [XL], With Caret & Partial Selection)
    bool isEditingStroke = false;
    bool isDraggingStrokeText = false;
    std::wstring editingStrokeText;
    size_t strokeCaretPos = 0;
    size_t strokeSelAnchor = 0;

    // Active Tool & Annotations
    OverlayTool activeTool = OverlayTool::SelectMove;
    Color activeColor = Color(255, 239, 68, 68);
    float activeStroke = 4.0f;
    int nextStepNum = 1;
    int nextAnnotationId = 1;
    int selectedAnnotationId = -1;
    PointF2D moveAnnOffset{0.0f, 0.0f};
    PointF2D penSnapAnchor{0.0f, 0.0f};
    bool isPenShiftSnapping = false;

    // In-Place Live Text Box Editing State (With Caret & Partial Selection)
    bool isEditingText = false;
    bool isDraggingTextBoxSel = false;
    Annotation editingTextAnn;
    size_t textCaretPos = 0;
    size_t textSelAnchor = 0;

    std::vector<Annotation> annotations;
    std::vector<std::vector<Annotation>> undoStack;
    std::vector<std::vector<Annotation>> redoStack;
    Annotation draftAnn;

    std::vector<Color> palette = {
        Color(255, 239, 68, 68),   // Red #EF4444
        Color(255, 34, 197, 94),   // Green #22C55E
        Color(255, 59, 130, 246),  // Blue #3B82F6
        Color(255, 255, 255, 255), // White #FFFFFF
        Color(255, 0, 0, 0)        // Black #000000
    };

    std::vector<float> strokeSizes = { 2.0f, 4.0f, 8.0f, 14.0f };
    std::vector<DockButton> dockButtons;
    int hoveredBtnId = -1;

    // Combined 3-Row Toolbar Dragging & Bounds State
    bool hasCustomHudPos = false;
    POINT customHudPos{0, 0}; // { rightX, topY }
    POINT dragHudStartMouse{0, 0};
    POINT dragHudOrigPos{0, 0};
    RECT hudBoundsRect{0, 0, 0, 0};

    // Persistent Native GDI Surface Cache (eliminates per-frame fullscreen GDI+ DrawImage & alpha-blend lag)
    HDC hBrightDesktopDC = nullptr;
    HBITMAP hBrightDesktopBmp = nullptr;
    HGDIOBJ hBrightOldBmp = nullptr;
    DWORD* brightDibPixels = nullptr;

    HDC hDimmedDesktopDC = nullptr;
    HBITMAP hDimmedDesktopBmp = nullptr;
    HGDIOBJ hDimmedOldBmp = nullptr;

    HDC hBackBufferDC = nullptr;
    HBITMAP hBackBufferBmp = nullptr;
    HGDIOBJ hBackOldBmp = nullptr;
    DWORD* backBufferPixels = nullptr;
    bool lastPillHover = false;
    bool lastStrokeHover = false;

    // Methods
    void InitPaths();
    static std::wstring GetDefaultDesktopFolder();
    static std::wstring GetAppDataSettingsDir();
    static std::wstring GetAppDataSettingsFile();
    void LoadSettings();
    void SaveSettings() const;
    static std::wstring GetFormatExtension(ImageFormat fmt);
    static const wchar_t* GetFormatLabel(ImageFormat fmt);
    HICON CreateCustomTrayIcon();
    void InitCustomCursors();
    void DestroyCustomCursors();
    void UpdateOverlayCursor(int mx, int my);
    void InitTrayIcon();
    void EnsureNotificationSoundEnabled();
    void ShowTrayToast(const std::wstring& title, const std::wstring& message);
    void RemoveTrayIcon();
    std::wstring FormatFilename(int seqNum) const;
    static std::wstring FormatFilenameWithPattern(const std::wstring& pattern, int seqNum);
    bool IsRunAtStartupEnabled() const;
    void ToggleRunAtStartup();

    Bitmap* CaptureVirtualDesktop();
    void BuildOverlaySurfaceCache();
    void FreeOverlaySurfaceCache();
    void InstantFullscreenCapture(bool delay3Sec = false);
    void StartRegionSnipOverlay(Bitmap* customBmp = nullptr);
    void CloseRegionSnipOverlay();
    void OpenImageIntoOverlay();

    void CommitActiveTextBox();
    void CommitActiveSizeInput();
    void CommitActiveStrokeInput();
    void ApplyStrokeToSelectedAnnotation(float newStroke, bool recordUndo);
    void RecalcNextStepNum();
    Bitmap* RenderCroppedRegionBitmap();
    bool CopyBitmapToClipboard(Bitmap* bmp);
    bool SaveBitmapToPath(Bitmap* bmp, const std::wstring& path);
    static bool SaveBitmapAsWebP(Bitmap* bmp, const std::wstring& path);
    static bool SaveBitmapAsMetadataFreeBMP(Bitmap* bmp, const std::wstring& path);
    static bool WriteMetadataFreeJPEG(IStream* stream, const std::wstring& path);
    static bool WriteMetadataFreePNG(IStream* stream, const std::wstring& path);
    void ActionCopyAndClose();
    void ActionCopyPixelColorAndClose();
    void ActionQuickSaveAndClose();
    void ActionSaveAsAndClose();
    void ActionPinToDesktop();
    void PushUndo();
    void Undo();
    void Redo();

    void ShowOptionsModal();
    void ShowShortcutsModal();

    static int GetMosaicBlockSize(float strokeWidth) {
        return std::max(2, (int)std::round((strokeWidth * 2.5f + 6.0f) * 0.25f));
    }
    static float MeasureMonoPrefixWidth(Graphics& g, const Font& monoFont, const std::wstring& s, size_t count);
    static size_t HitTestMonoIndex(int mouseX, int textLeftX, const std::wstring& s);
    static void DrawEditablePillText(Graphics& g, const Font& monoFont, int textLeftX, int textTopY, int boxTopY, int boxH, const std::wstring& text, const std::wstring& suffix, bool isEditing, size_t caretPos, size_t selAnchor);
    static void ApplyInlineEditKeyDown(WPARAM wParam, bool ctrl, bool shift, std::wstring& text, size_t& caret, size_t& anchor, size_t maxLen, bool isSizeField, HWND hWnd);
    static void ApplyInlineEditChar(wchar_t ch, std::wstring& text, size_t& caret, size_t& anchor, size_t maxLen);
    RECT GetEditingTextBoxRect() const;
    size_t HitTestTextBoxIndex(int mouseX, int mouseY) const;
    void ApplyTextBoxKeyDown(WPARAM wParam, bool ctrl, bool shift, HWND hWnd);
    static void ApplyPixelateFilter(Bitmap* bmp, int rx, int ry, int rw, int rh, int blockSize, bool elliptical = false);
    static void DrawVectorAnnotation(Graphics& g, const Annotation& ann, int offsetX, int offsetY, bool isSelected, bool isEditingCaret, bool forceOpaqueHighlighter = false);
    static void RenderHighlighterLayer(Graphics& destG, int regionX, int regionY, int regionW, int regionH, const std::vector<Annotation>& anns, const Annotation* draft, bool nonStacking);
    static void DrawDockButtonIcon(Graphics& g, const DockButton& b, const RectF& rf);
    static void DrawHoverBubbleTooltip(Graphics& g, const DockButton& b, int screenW, int screenH);
    void GetDimensionPillAnchor(bool& outRightAlign, bool& outBottomAlign) const;
    void BuildDockedHUD();
    DragMode HitTestSelectionHandles(int mx, int my) const;
    int HitTestAnnotation(float x, float y, DragMode* outHandleMode) const;
};

static PepperSnapDaemon g_Daemon;

// ----------------------------------------------------------------------------
// Timestamp & Folder Helpers (Ported from src/utils/timestamp.ts)
// ----------------------------------------------------------------------------

static void ReplaceStrW(std::wstring& str, const std::wstring& from, const std::wstring& to) {
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::wstring::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
}

std::wstring PepperSnapDaemon::GetDefaultDesktopFolder() {
    WCHAR deskPath[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, SHGFP_TYPE_CURRENT, deskPath)) && wcslen(deskPath) > 0) {
        return std::wstring(deskPath);
    }
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOP, nullptr, SHGFP_TYPE_CURRENT, deskPath)) && wcslen(deskPath) > 0) {
        return std::wstring(deskPath);
    }
    GetCurrentDirectoryW(MAX_PATH, deskPath);
    return std::wstring(deskPath);
}

std::wstring PepperSnapDaemon::GetAppDataSettingsDir() {
    WCHAR appData[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appData)) && wcslen(appData) > 0) {
        std::wstring dir = std::wstring(appData) + L"\\PepperSnap";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir;
    }
    return GetDefaultDesktopFolder();
}

std::wstring PepperSnapDaemon::GetAppDataSettingsFile() {
    return GetAppDataSettingsDir() + L"\\settings.ini";
}

void PepperSnapDaemon::SaveSettings() const {
    std::wstring iniPath = GetAppDataSettingsFile();
    // Ensure UTF-16LE BOM exists so Windows INI APIs preserve full Unicode paths
    if (GetFileAttributesW(iniPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        HANDLE hFile = CreateFileW(iniPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            WORD bom = 0xFEFF;
            DWORD written = 0;
            WriteFile(hFile, &bom, sizeof(bom), &written, nullptr);
            CloseHandle(hFile);
        }
    }
    WritePrivateProfileStringW(L"PepperSnap", L"SaveFolder", saveFolder.c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"NamingPattern", namingPattern.c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"RegionFormat", std::to_wstring((int)regionFormat).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"FullscreenFormat", std::to_wstring((int)fullscreenFormat).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"CopyFormat", std::to_wstring((int)copyFormat).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"JpegQuality", std::to_wstring(jpegQuality).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"NonStackingHighlighter", nonStackingHighlighter ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"AutoSaveOnCopy", autoSaveOnCopy ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"DoNotCopyOnSave", doNotCopyOnSave ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"PenSmoothingEnabled", penSmoothingEnabled ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"PenSmoothingStrength", std::to_wstring(penSmoothingStrength).c_str(), iniPath.c_str());
}

void PepperSnapDaemon::LoadSettings() {
    std::wstring iniPath = GetAppDataSettingsFile();
    if (GetFileAttributesW(iniPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        SaveSettings();
        return;
    }
    WCHAR buf[MAX_PATH] = {0};
    GetPrivateProfileStringW(L"PepperSnap", L"SaveFolder", saveFolder.c_str(), buf, MAX_PATH - 1, iniPath.c_str());
    if (wcslen(buf) > 0) saveFolder = buf;

    WCHAR nameBuf[512] = {0};
    GetPrivateProfileStringW(L"PepperSnap", L"NamingPattern", DEFAULT_NAMING_PATTERN, nameBuf, 511, iniPath.c_str());
    if (wcslen(nameBuf) > 0) namingPattern = nameBuf;

    int rFmt = (int)GetPrivateProfileIntW(L"PepperSnap", L"RegionFormat", (INT)regionFormat, iniPath.c_str());
    int fFmt = (int)GetPrivateProfileIntW(L"PepperSnap", L"FullscreenFormat", (INT)fullscreenFormat, iniPath.c_str());
    int cFmt = (int)GetPrivateProfileIntW(L"PepperSnap", L"CopyFormat", (INT)copyFormat, iniPath.c_str());
    if (rFmt >= 0 && rFmt <= 3) regionFormat = (ImageFormat)rFmt;
    if (fFmt >= 0 && fFmt <= 3) fullscreenFormat = (ImageFormat)fFmt;
    if (cFmt >= 0 && cFmt <= 3) copyFormat = (ImageFormat)cFmt;

    int q = (int)GetPrivateProfileIntW(L"PepperSnap", L"JpegQuality", jpegQuality, iniPath.c_str());
    jpegQuality = std::max(10, std::min(100, q));

    nonStackingHighlighter = (GetPrivateProfileIntW(L"PepperSnap", L"NonStackingHighlighter", 1, iniPath.c_str()) != 0);
    autoSaveOnCopy = (GetPrivateProfileIntW(L"PepperSnap", L"AutoSaveOnCopy", 1, iniPath.c_str()) != 0);
    doNotCopyOnSave = (GetPrivateProfileIntW(L"PepperSnap", L"DoNotCopyOnSave", 0, iniPath.c_str()) != 0);
    penSmoothingEnabled = (GetPrivateProfileIntW(L"PepperSnap", L"PenSmoothingEnabled", 1, iniPath.c_str()) != 0);
    int pSmooth = (int)GetPrivateProfileIntW(L"PepperSnap", L"PenSmoothingStrength", penSmoothingStrength, iniPath.c_str());
    penSmoothingStrength = std::max(5, std::min(100, pSmooth));
}

void PepperSnapDaemon::InitPaths() {
    // 1. Save screenshot to Desktop by default, then load persisted settings from %APPDATA%\PepperSnap
    saveFolder = GetDefaultDesktopFolder();
    LoadSettings();
}

std::wstring PepperSnapDaemon::GetFormatExtension(ImageFormat fmt) {
    switch (fmt) {
        case ImageFormat::JPEG: return L".jpg";
        case ImageFormat::PNG:  return L".png";
        case ImageFormat::WEBP: return L".webp";
        case ImageFormat::BMP:  return L".bmp";
        default:                return L".jpg";
    }
}

const wchar_t* PepperSnapDaemon::GetFormatLabel(ImageFormat fmt) {
    switch (fmt) {
        case ImageFormat::JPEG: return L"JPEG (.jpg)";
        case ImageFormat::PNG:  return L"PNG (.png)";
        case ImageFormat::WEBP: return L"WebP (.webp)";
        case ImageFormat::BMP:  return L"BMP (.bmp)";
        default:                return L"JPEG (.jpg)";
    }
}

std::wstring PepperSnapDaemon::FormatFilenameWithPattern(const std::wstring& pattern, int seqNum) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    auto pad2 = [](WORD v) {
        std::wstringstream ss;
        ss << std::setw(2) << std::setfill(L'0') << v;
        return ss.str();
    };
    auto pad3 = [](int v) {
        std::wstringstream ss;
        ss << std::setw(3) << std::setfill(L'0') << v;
        return ss.str();
    };

    std::wstring out = pattern.empty() ? DEFAULT_NAMING_PATTERN : pattern;
    ReplaceStrW(out, L"{YYYY}", std::to_wstring(st.wYear));
    ReplaceStrW(out, L"{YY}", pad2(st.wYear % 100));
    ReplaceStrW(out, L"{MM}", pad2(st.wMonth));
    ReplaceStrW(out, L"{DD}", pad2(st.wDay));
    ReplaceStrW(out, L"{HH}", pad2(st.wHour));
    ReplaceStrW(out, L"{mm}", pad2(st.wMinute));
    ReplaceStrW(out, L"{ss}", pad2(st.wSecond));
    ReplaceStrW(out, L"{ms}", pad3(st.wMilliseconds));
    ReplaceStrW(out, L"{NNN}", pad3(seqNum));

    const std::wstring illegal = L"\\/:*?\"<>|";
    for (wchar_t& c : out) {
        if (illegal.find(c) != std::wstring::npos) c = L'_';
    }
    return out.empty() ? L"PepperSnap_Capture" : out;
}

std::wstring PepperSnapDaemon::FormatFilename(int seqNum) const {
    return FormatFilenameWithPattern(namingPattern, seqNum);
}

// ----------------------------------------------------------------------------
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

void PepperSnapDaemon::EnsureNotificationSoundEnabled() {
    // 1. Check & enable global Windows 10/11 notification sound setting + per-app PlaySound setting
    HKEY hSettingsKey = nullptr;
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Notifications\\Settings",
            0, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &hSettingsKey, nullptr) == ERROR_SUCCESS) {
        DWORD allowSound = 0;
        DWORD cb = sizeof(allowSound);
        DWORD type = 0;
        if (RegQueryValueExW(hSettingsKey, L"NOC_GLOBAL_SETTING_ALLOW_NOTIFICATION_SOUND", nullptr, &type, (LPBYTE)&allowSound, &cb) != ERROR_SUCCESS || allowSound == 0) {
            allowSound = 1;
            RegSetValueExW(hSettingsKey, L"NOC_GLOBAL_SETTING_ALLOW_NOTIFICATION_SOUND", 0, REG_DWORD, (const BYTE*)&allowSound, sizeof(allowSound));
        }

        auto ensureAppSubKeySound = [&](const wchar_t* subKeyName) {
            HKEY hAppKey = nullptr;
            if (RegOpenKeyExW(hSettingsKey, subKeyName, 0, KEY_READ | KEY_WRITE, &hAppKey) == ERROR_SUCCESS) {
                DWORD val = 0, sz = sizeof(val), vType = 0;
                if (RegQueryValueExW(hAppKey, L"Enabled", nullptr, &vType, (LPBYTE)&val, &sz) != ERROR_SUCCESS || val == 0) {
                    val = 1;
                    RegSetValueExW(hAppKey, L"Enabled", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
                }
                sz = sizeof(val);
                if (RegQueryValueExW(hAppKey, L"PlaySound", nullptr, &vType, (LPBYTE)&val, &sz) != ERROR_SUCCESS || val == 0) {
                    val = 1;
                    RegSetValueExW(hAppKey, L"PlaySound", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
                }
                RegCloseKey(hAppKey);
            }
        };

        DWORD idx = 0;
        WCHAR subName[512];
        DWORD subLen = 512;
        while (RegEnumKeyExW(hSettingsKey, idx++, subName, &subLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            if (wcsstr(subName, L"PepperSnap") != nullptr || wcsstr(subName, L"peppersnap") != nullptr ||
                wcsstr(subName, L"Microsoft.Explorer.Notification") != nullptr) {
                ensureAppSubKeySound(subName);
            }
            subLen = 512;
        }
        RegCloseKey(hSettingsKey);
    }

    // 2. Check & enable HKCU\Control Panel\Sound -> Beep & ExtendedSounds
    HKEY hSoundKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Sound", 0, KEY_READ | KEY_WRITE, &hSoundKey) == ERROR_SUCCESS) {
        WCHAR buf[32] = {0};
        DWORD cb = sizeof(buf);
        if (RegQueryValueExW(hSoundKey, L"Beep", nullptr, nullptr, (LPBYTE)buf, &cb) == ERROR_SUCCESS && _wcsicmp(buf, L"yes") != 0) {
            const wchar_t* yesStr = L"yes";
            RegSetValueExW(hSoundKey, L"Beep", 0, REG_SZ, (const BYTE*)yesStr, (DWORD)((wcslen(yesStr) + 1) * sizeof(wchar_t)));
        }
        cb = sizeof(buf);
        if (RegQueryValueExW(hSoundKey, L"ExtendedSounds", nullptr, nullptr, (LPBYTE)buf, &cb) == ERROR_SUCCESS && _wcsicmp(buf, L"yes") != 0) {
            const wchar_t* yesStr = L"yes";
            RegSetValueExW(hSoundKey, L"ExtendedSounds", 0, REG_SZ, (const BYTE*)yesStr, (DWORD)((wcslen(yesStr) + 1) * sizeof(wchar_t)));
        }
        RegCloseKey(hSoundKey);
    }
}

void PepperSnapDaemon::InitTrayIcon() {
    EnsureNotificationSoundEnabled();
    hTrayIcon = CreateCustomTrayIcon();
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hTrayWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = hTrayIcon;
    wcsncpy_s(nid.szTip, L"PepperSnap v3.0.0.2 — Ctrl+PrtScn: Region Snip | Shift+PrtScn: Instant Fullscreen", _TRUNCATE);
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void PepperSnapDaemon::ShowTrayToast(const std::wstring& title, const std::wstring& message) {
    EnsureNotificationSoundEnabled();
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    nid.dwInfoFlags &= ~NIIF_NOSOUND;
    // Clear any currently visible balloon first so Windows always plays the notification sound on consecutive captures
    nid.szInfoTitle[0] = L'\0';
    nid.szInfo[0] = L'\0';
    Shell_NotifyIconW(NIM_MODIFY, &nid);

    wcsncpy_s(nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid.szInfo, message.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void PepperSnapDaemon::RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &nid);
    if (hTrayIcon) {
        DestroyIcon(hTrayIcon);
        hTrayIcon = nullptr;
    }
}

static HCURSOR CreateCursorFromBitmap(Bitmap& bmp, DWORD hotX, DWORD hotY) {
    HICON hIcon = nullptr;
    if (bmp.GetHICON(&hIcon) != Ok || !hIcon) return LoadCursorW(nullptr, IDC_ARROW);
    ICONINFO ii = {0};
    if (!GetIconInfo(hIcon, &ii)) {
        DestroyIcon(hIcon);
        return LoadCursorW(nullptr, IDC_ARROW);
    }
    ii.fIcon = FALSE;
    ii.xHotspot = hotX;
    ii.yHotspot = hotY;
    HCURSOR hCur = (HCURSOR)CreateIconIndirect(&ii);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    DestroyIcon(hIcon);
    return hCur ? hCur : LoadCursorW(nullptr, IDC_ARROW);
}

void PepperSnapDaemon::InitCustomCursors() {
    // 1. Custom Pen Cursor (nib hotspot at (3, 28))
    {
        Bitmap bmp(32, 32, PixelFormat32bppARGB);
        Graphics g(&bmp);
        g.SetSmoothingMode(SmoothingModeAntiAlias);

        // Pen barrel angled from top-right (26, 5) down-left to nib (3, 28)
        PointF barrel[4] = {
            PointF(9.0f, 18.0f),
            PointF(23.0f, 4.0f),
            PointF(27.0f, 8.0f),
            PointF(13.0f, 22.0f)
        };
        SolidBrush barrelFill(Color(255, 239, 68, 68));
        Pen darkOutline(Color(255, 15, 23, 42), 1.6f);
        g.FillPolygon(&barrelFill, barrel, 4);
        g.DrawPolygon(&darkOutline, barrel, 4);

        // White collar + metallic pen nib cone pointing to (3, 28)
        PointF nibCone[3] = {
            PointF(3.0f, 28.0f),
            PointF(9.0f, 18.0f),
            PointF(13.0f, 22.0f)
        };
        SolidBrush nibFill(Color(255, 248, 250, 252));
        g.FillPolygon(&nibFill, nibCone, 3);
        g.DrawPolygon(&darkOutline, nibCone, 3);

        // Ink tip dot at (3, 28)
        SolidBrush tipBrush(Color(255, 239, 68, 68));
        g.FillEllipse(&tipBrush, 1.5f, 26.5f, 3.5f, 3.5f);

        hCurPen = CreateCursorFromBitmap(bmp, 3, 28);
    }

    // 2. Custom Stabilo Boss Highlighter Cursor (chisel tip hotspot at (4, 27))
    {
        Bitmap bmp(32, 32, PixelFormat32bppARGB);
        Graphics g(&bmp);
        g.SetSmoothingMode(SmoothingModeAntiAlias);

        Pen darkOutline(Color(255, 15, 23, 42), 1.6f);

        // Broad Stabilo fluorescent yellow-orange body
        PointF body[4] = {
            PointF(8.0f, 17.0f),
            PointF(20.0f, 5.0f),
            PointF(27.0f, 12.0f),
            PointF(15.0f, 24.0f)
        };
        SolidBrush bodyFill(Color(255, 250, 204, 21)); // Classic Stabilo fluorescent yellow
        g.FillPolygon(&bodyFill, body, 4);
        g.DrawPolygon(&darkOutline, body, 4);

        // Stabilo side grip stripe
        Pen stripePen(Color(255, 254, 240, 138), 2.0f);
        g.DrawLine(&stripePen, 11.5f, 17.5f, 21.5f, 7.5f);

        // Black neck collar
        PointF collar[4] = {
            PointF(6.0f, 20.0f),
            PointF(8.0f, 17.0f),
            PointF(15.0f, 24.0f),
            PointF(12.0f, 26.0f)
        };
        SolidBrush collarFill(Color(255, 30, 41, 59));
        g.FillPolygon(&collarFill, collar, 4);

        // Angled chisel felt tip at (3, 28) .. (8, 28)
        PointF chisel[4] = {
            PointF(3.0f, 28.0f),
            PointF(6.0f, 20.0f),
            PointF(12.0f, 26.0f),
            PointF(8.0f, 29.0f)
        };
        SolidBrush chiselFill(Color(255, 163, 230, 53)); // Bright neon highlighter tip
        g.FillPolygon(&chiselFill, chisel, 4);
        g.DrawPolygon(&darkOutline, chisel, 4);

        hCurStabilo = CreateCursorFromBitmap(bmp, 4, 27);
    }
}

void PepperSnapDaemon::DestroyCustomCursors() {
    if (hCurPen) { DestroyCursor(hCurPen); hCurPen = nullptr; }
    if (hCurStabilo) { DestroyCursor(hCurStabilo); hCurStabilo = nullptr; }
}

void PepperSnapDaemon::UpdateOverlayCursor(int mx, int my) {
    if (dragMode == DragMode::DraggingHUD) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
        return;
    }
    POINT pt{ mx, my };
    for (const auto& b : dockButtons) {
        if (PtInRect(&b.rect, pt)) {
            if (b.id == DBTN_ACT_DRAG_HUD) {
                SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
            } else {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
            }
            return;
        }
    }

    if (hasSelection && (PtInRect(&dimPillRect, pt) || PtInRect(&customStrokeRect, pt))) {
        SetCursor(LoadCursorW(nullptr, IDC_IBEAM));
        return;
    }
    if (isEditingText) {
        RECT tbRc = GetEditingTextBoxRect();
        if (PtInRect(&tbRc, pt)) {
            SetCursor(LoadCursorW(nullptr, IDC_IBEAM));
            return;
        }
    }

    DragMode handleHit = (dragMode >= DragMode::ResizeTL && dragMode <= DragMode::ResizeL)
        ? dragMode
        : HitTestSelectionHandles(mx, my);
    switch (handleHit) {
        case DragMode::ResizeTL:
        case DragMode::ResizeBR:
            SetCursor(LoadCursorW(nullptr, IDC_SIZENWSE));
            return;
        case DragMode::ResizeTR:
        case DragMode::ResizeBL:
            SetCursor(LoadCursorW(nullptr, IDC_SIZENESW));
            return;
        case DragMode::ResizeT:
        case DragMode::ResizeB:
            SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
            return;
        case DragMode::ResizeL:
        case DragMode::ResizeR:
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return;
        default:
            break;
    }

    if (!hasSelection || dragMode == DragMode::CreatingSelection) {
        SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        return;
    }

    // Tool-specific cursors per user specification:
    // 1. Arrow cursor for Select mode
    // 2. Pen cursor for Pen
    // 3. Stabilo cursor for Highlighter
    // 4. (+ 'plus') cursor for: Line, Square, Circle, Mosaic Square, Mosaic Circle, Arrow, Numbering Arrow
    // 5. Arrow cursor for Text
    switch (activeTool) {
        case OverlayTool::SelectMove:
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
            break;
        case OverlayTool::Pen:
            SetCursor(hCurPen ? hCurPen : LoadCursorW(nullptr, IDC_ARROW));
            break;
        case OverlayTool::Highlighter:
            SetCursor(hCurStabilo ? hCurStabilo : LoadCursorW(nullptr, IDC_ARROW));
            break;
        case OverlayTool::TextBox:
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
            break;
        case OverlayTool::Line:
        case OverlayTool::Rectangle:
        case OverlayTool::Ellipse:
        case OverlayTool::MosaicSquare:
        case OverlayTool::MosaicCircle:
        case OverlayTool::Arrow:
        case OverlayTool::NumberArrow:
        default:
            SetCursor(LoadCursorW(nullptr, IDC_CROSS));
            break;
    }
}

bool PepperSnapDaemon::IsRunAtStartupEnabled() const {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    WCHAR buf[MAX_PATH] = {0};
    DWORD cb = sizeof(buf);
    LSTATUS st = RegQueryValueExW(hKey, L"PepperSnap", nullptr, nullptr, (LPBYTE)buf, &cb);
    RegCloseKey(hKey);
    return (st == ERROR_SUCCESS && wcslen(buf) > 0);
}

void PepperSnapDaemon::ToggleRunAtStartup() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
        return;
    }
    if (IsRunAtStartupEnabled()) {
        RegDeleteValueW(hKey, L"PepperSnap");
        ShowTrayToast(L"Startup Disabled", L"PepperSnap will no longer launch automatically at login.");
    } else {
        WCHAR exePath[MAX_PATH] = {0};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring quoted = L"\"" + std::wstring(exePath) + L"\"";
        RegSetValueExW(hKey, L"PepperSnap", 0, REG_SZ, (const BYTE*)quoted.c_str(), (DWORD)((quoted.size() + 1) * sizeof(wchar_t)));
        ShowTrayToast(L"Startup Enabled", L"PepperSnap will start in the system tray when Windows boots.");
    }
    RegCloseKey(hKey);
}

// ----------------------------------------------------------------------------
// Low-Level Keyboard Hook (Ctrl+PrtScn & Shift+PrtScn)
// ----------------------------------------------------------------------------

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;
        if (kb->vkCode == VK_SNAPSHOT) {
            bool ctrlDown  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            if (ctrlDown && !shiftDown) {
                PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_REGION_SNIP, 0, 0);
                return 1;
            }
            if (shiftDown && !ctrlDown) {
                PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_FULL_SNAP, 0, 0);
                return 1;
            }
        }
    }
    return CallNextHookEx(g_Daemon.hKeyHook, nCode, wParam, lParam);
}

// ----------------------------------------------------------------------------
// Screen Capture, Clipboard & Image Saving
// ----------------------------------------------------------------------------

Bitmap* PepperSnapDaemon::CaptureVirtualDesktop() {
    vScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    vScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    vScreenW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    vScreenH = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (vScreenW <= 0 || vScreenH <= 0) {
        vScreenW = GetSystemMetrics(SM_CXSCREEN);
        vScreenH = GetSystemMetrics(SM_CYSCREEN);
    }

    HDC hScreen = GetDC(nullptr);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = vScreenW;
    bmi.bmiHeader.biHeight = -vScreenH; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HDC hMem = CreateCompatibleDC(hScreen);
    HBITMAP hDIB = CreateDIBSection(hScreen, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    HGDIOBJ hOld = SelectObject(hMem, hDIB);

    BitBlt(hMem, 0, 0, vScreenW, vScreenH, hScreen, vScreenX, vScreenY, SRCCOPY | CAPTUREBLT);

    Bitmap* clone32 = new Bitmap(vScreenW, vScreenH, PixelFormat32bppARGB);
    if (clone32 && pBits) {
        Rect lockRc(0, 0, vScreenW, vScreenH);
        BitmapData bd = {};
        if (clone32->LockBits(&lockRc, ImageLockModeWrite, PixelFormat32bppARGB, &bd) == Ok) {
            if (bd.Stride == vScreenW * 4) {
                const DWORD* src = (const DWORD*)pBits;
                DWORD* dst = (DWORD*)bd.Scan0;
                size_t totalPx = (size_t)vScreenW * (size_t)vScreenH;
                for (size_t i = 0; i < totalPx; ++i) {
                    dst[i] = src[i] | 0xFF000000u;
                }
            } else {
                for (int y = 0; y < vScreenH; ++y) {
                    const DWORD* srcRow = (const DWORD*)pBits + (size_t)y * vScreenW;
                    DWORD* dstRow = (DWORD*)((BYTE*)bd.Scan0 + y * bd.Stride);
                    for (int x = 0; x < vScreenW; ++x) {
                        dstRow[x] = srcRow[x] | 0xFF000000u;
                    }
                }
            }
            clone32->UnlockBits(&bd);
        }
    }

    SelectObject(hMem, hOld);
    if (hDIB) DeleteObject(hDIB);
    DeleteDC(hMem);
    ReleaseDC(nullptr, hScreen);
    return clone32;
}

void PepperSnapDaemon::FreeOverlaySurfaceCache() {
    if (hBackBufferDC) {
        if (hBackOldBmp) SelectObject(hBackBufferDC, hBackOldBmp);
        if (hBackBufferBmp) DeleteObject(hBackBufferBmp);
        DeleteDC(hBackBufferDC);
        hBackBufferDC = nullptr;
        hBackBufferBmp = nullptr;
        hBackOldBmp = nullptr;
        backBufferPixels = nullptr;
    }
    if (hDimmedDesktopDC) {
        if (hDimmedOldBmp) SelectObject(hDimmedDesktopDC, hDimmedOldBmp);
        if (hDimmedDesktopBmp) DeleteObject(hDimmedDesktopBmp);
        DeleteDC(hDimmedDesktopDC);
        hDimmedDesktopDC = nullptr;
        hDimmedDesktopBmp = nullptr;
        hDimmedOldBmp = nullptr;
    }
    if (hBrightDesktopDC) {
        if (hBrightOldBmp) SelectObject(hBrightDesktopDC, hBrightOldBmp);
        if (hBrightDesktopBmp) DeleteObject(hBrightDesktopBmp);
        DeleteDC(hBrightDesktopDC);
        hBrightDesktopDC = nullptr;
        hBrightDesktopBmp = nullptr;
        hBrightOldBmp = nullptr;
        brightDibPixels = nullptr;
    }
}

void PepperSnapDaemon::BuildOverlaySurfaceCache() {
    FreeOverlaySurfaceCache();
    if (!frozenDesktopBmp || vScreenW <= 0 || vScreenH <= 0) return;

    HDC hScreen = GetDC(nullptr);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = vScreenW;
    bmi.bmiHeader.biHeight = -vScreenH; // top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBrightBits = nullptr;
    hBrightDesktopDC = CreateCompatibleDC(hScreen);
    hBrightDesktopBmp = CreateDIBSection(hScreen, &bmi, DIB_RGB_COLORS, &pBrightBits, nullptr, 0);
    if (hBrightDesktopBmp) hBrightOldBmp = SelectObject(hBrightDesktopDC, hBrightDesktopBmp);
    brightDibPixels = (DWORD*)pBrightBits;

    void* pDimBits = nullptr;
    hDimmedDesktopDC = CreateCompatibleDC(hScreen);
    hDimmedDesktopBmp = CreateDIBSection(hScreen, &bmi, DIB_RGB_COLORS, &pDimBits, nullptr, 0);
    if (hDimmedDesktopBmp) hDimmedOldBmp = SelectObject(hDimmedDesktopDC, hDimmedDesktopBmp);

    void* pBackBits = nullptr;
    hBackBufferDC = CreateCompatibleDC(hScreen);
    hBackBufferBmp = CreateDIBSection(hScreen, &bmi, DIB_RGB_COLORS, &pBackBits, nullptr, 0);
    if (hBackBufferBmp) hBackOldBmp = SelectObject(hBackBufferDC, hBackBufferBmp);
    backBufferPixels = (DWORD*)pBackBits;

    ReleaseDC(nullptr, hScreen);

    if (pBrightBits && pDimBits) {
        Rect lockRc(0, 0, vScreenW, vScreenH);
        BitmapData bd = {};
        if (frozenDesktopBmp->LockBits(&lockRc, ImageLockModeRead, PixelFormat32bppARGB, &bd) == Ok) {
            // Scrim Color(135, 9, 13, 22): exact division by 255 via ((x + 128) * 257) >> 16 (zero CPU division instructions)
            for (int y = 0; y < vScreenH; ++y) {
                const DWORD* srcRow = (const DWORD*)((const BYTE*)bd.Scan0 + y * bd.Stride);
                DWORD* brightRow = (DWORD*)pBrightBits + (size_t)y * vScreenW;
                DWORD* dimRow    = (DWORD*)pDimBits + (size_t)y * vScreenW;
                memcpy(brightRow, srcRow, (size_t)vScreenW * sizeof(DWORD));
                for (int x = 0; x < vScreenW; ++x) {
                    DWORD px = srcRow[x];
                    uint32_t b = (px & 0xFF);
                    uint32_t g = ((px >> 8) & 0xFF);
                    uint32_t r = ((px >> 16) & 0xFF);
                    uint32_t db = ((b * 120 + 3098) * 257) >> 16;
                    uint32_t dg = ((g * 120 + 1883) * 257) >> 16;
                    uint32_t dr = ((r * 120 + 1343) * 257) >> 16;
                    dimRow[x] = 0xFF000000u | (dr << 16) | (dg << 8) | db;
                }
            }
            frozenDesktopBmp->UnlockBits(&bd);
        }
    }
}

static bool GetGdiPlusEncoderClsid(const WCHAR* mimeType, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return false;
    std::vector<BYTE> buf(size);
    ImageCodecInfo* pInfo = (ImageCodecInfo*)buf.data();
    GetImageEncoders(num, size, pInfo);
    for (UINT i = 0; i < num; ++i) {
        if (wcscmp(pInfo[i].MimeType, mimeType) == 0) {
            *pClsid = pInfo[i].Clsid;
            return true;
        }
    }
    return false;
}

bool PepperSnapDaemon::SaveBitmapAsWebP(Bitmap* bmp, const std::wstring& path) {
    if (!bmp) return false;
    UINT w = bmp->GetWidth();
    UINT h = bmp->GetHeight();
    if (w == 0 || h == 0 || w > 16383 || h > 16383) return false;

    Rect lockRc(0, 0, (INT)w, (INT)h);
    BitmapData data;
    if (bmp->LockBits(&lockRc, ImageLockModeRead, PixelFormat32bppARGB, &data) != Ok) return false;

    struct VP8LBitWriter {
        std::vector<BYTE> bytes;
        uint64_t acc = 0;
        int bits = 0;
        void Write(uint32_t val, int n) {
            acc |= (uint64_t(val) & ((1ULL << n) - 1ULL)) << bits;
            bits += n;
            while (bits >= 8) {
                bytes.push_back((BYTE)(acc & 0xFF));
                acc >>= 8;
                bits -= 8;
            }
        }
        void Flush() {
            if (bits > 0) {
                bytes.push_back((BYTE)(acc & 0xFF));
                acc = 0;
                bits = 0;
            }
        }
    } bw;

    bw.bytes.reserve(w * h * 3 + 512);
    bw.Write(0x2F, 8);       // VP8L signature byte
    bw.Write(w - 1, 14);     // 14-bit width - 1
    bw.Write(h - 1, 14);     // 14-bit height - 1
    bw.Write(0, 1);          // alpha_is_used = 0
    bw.Write(0, 3);          // version = 0
    bw.Write(0, 1);          // no transforms
    bw.Write(0, 1);          // no color cache
    bw.Write(0, 1);          // no meta prefix codes

    auto writeIdentity8BitTree = [&](int alphabetSize) {
        bw.Write(0, 1);      // normal code length tree
        bw.Write(12 - 4, 4); // num_code_lengths = 12
        const int clLens[12] = { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
        for (int i = 0; i < 12; ++i) bw.Write(clLens[i], 3);
        bw.Write(0, 1);      // full alphabet
        for (int i = 0; i < alphabetSize; ++i) bw.Write(i < 256 ? 1 : 0, 1);
    };

    writeIdentity8BitTree(280); // Green + length codes
    writeIdentity8BitTree(256); // Red
    writeIdentity8BitTree(256); // Blue
    // Alpha: single-symbol tree for 255 (0 bits per pixel)
    bw.Write(1, 1); bw.Write(0, 1); bw.Write(1, 1); bw.Write(255, 8);
    // Distance: single-symbol tree for 0
    bw.Write(1, 1); bw.Write(0, 1); bw.Write(0, 1); bw.Write(0, 1);

    BYTE rev8[256];
    for (int i = 0; i < 256; ++i) {
        BYTE r = 0;
        for (int b = 0; b < 8; ++b) r = (BYTE)((r << 1) | ((i >> b) & 1));
        rev8[i] = r;
    }

    const BYTE* scan0 = (const BYTE*)data.Scan0;
    for (UINT y = 0; y < h; ++y) {
        const DWORD* row = (const DWORD*)(scan0 + y * data.Stride);
        for (UINT x = 0; x < w; ++x) {
            DWORD px = row[x];
            BYTE b = (BYTE)(px & 0xFF);
            BYTE g = (BYTE)((px >> 8) & 0xFF);
            BYTE r = (BYTE)((px >> 16) & 0xFF);
            bw.Write(rev8[g], 8);
            bw.Write(rev8[r], 8);
            bw.Write(rev8[b], 8);
        }
    }
    bmp->UnlockBits(&data);
    bw.Flush();

    uint32_t vp8lSize = (uint32_t)bw.bytes.size();
    uint32_t padByte = (vp8lSize & 1u);
    uint32_t riffSize = 4u + 8u + vp8lSize + padByte;

    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    BYTE header[20];
    memcpy(header + 0,  "RIFF", 4);
    memcpy(header + 4,  &riffSize, 4);
    memcpy(header + 8,  "WEBP", 4);
    memcpy(header + 12, "VP8L", 4);
    memcpy(header + 16, &vp8lSize, 4);

    DWORD written = 0;
    bool ok = WriteFile(hFile, header, 20, &written, nullptr) && (written == 20);
    if (ok && vp8lSize > 0) {
        ok = WriteFile(hFile, bw.bytes.data(), vp8lSize, &written, nullptr) && (written == vp8lSize);
    }
    if (ok && padByte) {
        BYTE zero = 0;
        WriteFile(hFile, &zero, 1, &written, nullptr);
    }
    CloseHandle(hFile);
    return ok;
}

static bool ExtractStreamBytes(IStream* stream, std::vector<BYTE>& outBytes) {
    if (!stream) return false;
    STATSTG stat = {0};
    if (FAILED(stream->Stat(&stat, STATFLAG_NONAME))) return false;
    ULONG sz = (ULONG)stat.cbSize.QuadPart;
    if (sz == 0) return false;
    outBytes.resize(sz);
    LARGE_INTEGER zero = {0};
    stream->Seek(zero, STREAM_SEEK_SET, nullptr);
    ULONG readBytes = 0;
    return SUCCEEDED(stream->Read(outBytes.data(), sz, &readBytes)) && (readBytes == sz);
}

// 3. Strip all metadata segments (EXIF, XMP, ICC Profile, IPTC, Photoshop, Comments) from JPEG stream
bool PepperSnapDaemon::WriteMetadataFreeJPEG(IStream* stream, const std::wstring& path) {
    std::vector<BYTE> src;
    if (!ExtractStreamBytes(stream, src) || src.size() < 4 || src[0] != 0xFF || src[1] != 0xD8) return false;

    std::vector<BYTE> clean;
    clean.reserve(src.size());
    clean.push_back(0xFF);
    clean.push_back(0xD8); // SOI

    size_t i = 2;
    while (i < src.size()) {
        if (src[i] != 0xFF) {
            clean.insert(clean.end(), src.begin() + i, src.end());
            break;
        }
        while (i < src.size() && src[i] == 0xFF) i++;
        if (i >= src.size()) break;
        BYTE marker = src[i++];

        if (marker == 0xD9) { // EOI
            clean.push_back(0xFF);
            clean.push_back(0xD9);
            break;
        }
        if (marker == 0x00 || (marker >= 0xD0 && marker <= 0xD7) || marker == 0x01) {
            clean.push_back(0xFF);
            clean.push_back(marker);
            continue;
        }
        if (i + 1 >= src.size()) break;
        uint16_t segLen = (uint16_t(src[i]) << 8) | uint16_t(src[i + 1]);
        if (segLen < 2 || i + segLen > src.size()) break;

        if (marker == 0xDA) { // SOS: copy SOS header + entropy-coded image scan to end of file
            clean.push_back(0xFF);
            clean.push_back(marker);
            clean.insert(clean.end(), src.begin() + i, src.end());
            break;
        }

        // Drop all APP1..APP15 (0xE1..0xEF: EXIF, XMP, ICC, IPTC, Adobe) and COM (0xFE: comments)
        // Drop APP0 (0xE0) as well unless it's the bare 16-byte JFIF pixel aspect ratio header
        bool isMetadataSegment = (marker >= 0xE1 && marker <= 0xEF) || (marker == 0xFE) || (marker == 0xE0);
        if (!isMetadataSegment) {
            clean.push_back(0xFF);
            clean.push_back(marker);
            clean.insert(clean.end(), src.begin() + i, src.begin() + i + segLen);
        }
        i += segLen;
    }

    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(hFile, clean.data(), (DWORD)clean.size(), &written, nullptr) && (written == clean.size());
    CloseHandle(hFile);
    return ok;
}

// 3. Strip all ancillary metadata chunks (tEXt, zTXt, iTXt, eXIf, tIME, pHYs, iCCP, sRGB, gAMA, cHRM) from PNG stream
bool PepperSnapDaemon::WriteMetadataFreePNG(IStream* stream, const std::wstring& path) {
    std::vector<BYTE> src;
    if (!ExtractStreamBytes(stream, src) || src.size() < 8) return false;
    const BYTE pngSig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if (memcmp(src.data(), pngSig, 8) != 0) return false;

    std::vector<BYTE> clean;
    clean.reserve(src.size());
    clean.insert(clean.end(), pngSig, pngSig + 8);

    size_t i = 8;
    while (i + 12 <= src.size()) {
        uint32_t len = (uint32_t(src[i]) << 24) | (uint32_t(src[i + 1]) << 16) | (uint32_t(src[i + 2]) << 8) | uint32_t(src[i + 3]);
        size_t totalChunk = 12ULL + len;
        if (i + totalChunk > src.size()) break;

        const char* type = (const char*)(src.data() + i + 4);
        bool keep = (memcmp(type, "IHDR", 4) == 0) ||
                    (memcmp(type, "PLTE", 4) == 0) ||
                    (memcmp(type, "tRNS", 4) == 0) ||
                    (memcmp(type, "IDAT", 4) == 0) ||
                    (memcmp(type, "IEND", 4) == 0);
        if (keep) {
            clean.insert(clean.end(), src.begin() + i, src.begin() + i + totalChunk);
        }
        i += totalChunk;
        if (memcmp(type, "IEND", 4) == 0) break;
    }

    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(hFile, clean.data(), (DWORD)clean.size(), &written, nullptr) && (written == clean.size());
    CloseHandle(hFile);
    return ok;
}

// 3. Pure metadata-free 24-bit BMP writer (54-byte header + BGR pixels only, zero DPI/profile metadata)
bool PepperSnapDaemon::SaveBitmapAsMetadataFreeBMP(Bitmap* bmp, const std::wstring& path) {
    if (!bmp) return false;
    UINT w = bmp->GetWidth(), h = bmp->GetHeight();
    if (w == 0 || h == 0) return false;

    Rect lockRc(0, 0, (INT)w, (INT)h);
    BitmapData data;
    if (bmp->LockBits(&lockRc, ImageLockModeRead, PixelFormat32bppARGB, &data) != Ok) return false;

    int rowStride = ((w * 3 + 3) / 4) * 4;
    DWORD imgSize = (DWORD)(rowStride * h);

    BITMAPFILEHEADER bfh = {0};
    bfh.bfType = 0x4D42; // 'BM'
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + imgSize;

    BITMAPINFOHEADER bih = {0};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = (LONG)w;
    bih.biHeight = (LONG)h;
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;

    std::vector<BYTE> pixels(imgSize, 0);
    const BYTE* scan0 = (const BYTE*)data.Scan0;
    for (UINT y = 0; y < h; ++y) {
        const DWORD* srcRow = (const DWORD*)(scan0 + (h - 1 - y) * data.Stride);
        BYTE* dstRow = pixels.data() + y * rowStride;
        for (UINT x = 0; x < w; ++x) {
            DWORD px = srcRow[x];
            dstRow[x * 3 + 0] = (BYTE)(px & 0xFF);
            dstRow[x * 3 + 1] = (BYTE)((px >> 8) & 0xFF);
            dstRow[x * 3 + 2] = (BYTE)((px >> 16) & 0xFF);
        }
    }
    bmp->UnlockBits(&data);

    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(hFile, &bfh, sizeof(bfh), &written, nullptr) &&
              WriteFile(hFile, &bih, sizeof(bih), &written, nullptr) &&
              WriteFile(hFile, pixels.data(), imgSize, &written, nullptr);
    CloseHandle(hFile);
    return ok;
}

bool PepperSnapDaemon::SaveBitmapToPath(Bitmap* bmp, const std::wstring& path) {
    if (!bmp) return false;
    std::wstring ext = PathFindExtensionW(path.c_str());
    for (wchar_t& c : ext) c = (wchar_t)towlower(c);

    // 3. Do NOT save any metadata into the file at all (for WebP, BMP, JPEG, and PNG)
    if (ext == L".webp") {
        return SaveBitmapAsWebP(bmp, path);
    }
    if (ext == L".bmp") {
        return SaveBitmapAsMetadataFreeBMP(bmp, path);
    }

    const WCHAR* mime = (ext == L".png") ? L"image/png" : L"image/jpeg";
    CLSID clsid;
    if (!GetGdiPlusEncoderClsid(mime, &clsid)) return false;

    IStream* memStream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &memStream)) || !memStream) return false;

    Status st = GenericError;
    if (wcscmp(mime, L"image/jpeg") == 0) {
        EncoderParameters encParams;
        encParams.Count = 1;
        encParams.Parameter[0].Guid = EncoderQuality;
        encParams.Parameter[0].Type = EncoderParameterValueTypeLong;
        encParams.Parameter[0].NumberOfValues = 1;
        ULONG quality = (ULONG)std::max(10, std::min(100, jpegQuality));
        encParams.Parameter[0].Value = &quality;
        st = bmp->Save(memStream, &clsid, &encParams);
    } else {
        st = bmp->Save(memStream, &clsid, nullptr);
    }

    bool ok = false;
    if (st == Ok) {
        if (wcscmp(mime, L"image/jpeg") == 0) {
            ok = WriteMetadataFreeJPEG(memStream, path);
        } else {
            ok = WriteMetadataFreePNG(memStream, path);
        }
    }
    memStream->Release();
    return ok;
}

bool PepperSnapDaemon::CopyBitmapToClipboard(Bitmap* bmp) {
    if (!bmp) return false;
    int bw = (int)bmp->GetWidth();
    int bh = (int)bmp->GetHeight();
    if (bw <= 0 || bh <= 0) return false;

    Rect lockRc(0, 0, bw, bh);
    BitmapData bd = {};
    if (bmp->LockBits(&lockRc, ImageLockModeRead, PixelFormat32bppARGB, &bd) != Ok) {
        return false;
    }

    DWORD rowBytes = (DWORD)bw * 4;
    DWORD dwBmpSize = rowBytes * (DWORD)bh;
    HGLOBAL hDIB = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + dwBmpSize);
    if (!hDIB) {
        bmp->UnlockBits(&bd);
        return false;
    }

    BYTE* lpbitmap = (BYTE*)GlobalLock(hDIB);
    if (!lpbitmap) {
        bmp->UnlockBits(&bd);
        GlobalFree(hDIB);
        return false;
    }

    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bw;
    bi.biHeight = bh; // bottom-up standard CF_DIB for maximum app compatibility
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = dwBmpSize;
    memcpy(lpbitmap, &bi, sizeof(BITMAPINFOHEADER));

    BYTE* dstBase = lpbitmap + sizeof(BITMAPINFOHEADER);
    for (int y = 0; y < bh; ++y) {
        const BYTE* srcRow = (const BYTE*)bd.Scan0 + (size_t)(bh - 1 - y) * bd.Stride;
        memcpy(dstBase + (size_t)y * rowBytes, srcRow, rowBytes);
    }

    GlobalUnlock(hDIB);
    bmp->UnlockBits(&bd);

    if (!OpenClipboard(hTrayWnd)) {
        GlobalFree(hDIB);
        return false;
    }
    EmptyClipboard();
    SetClipboardData(CF_DIB, hDIB);
    CloseClipboard();
    return true;
}

void PepperSnapDaemon::InstantFullscreenCapture(bool delay3Sec) {
    if (hOverlayWnd) CloseRegionSnipOverlay();
    if (delay3Sec) Sleep(3000);

    Bitmap* fullBmp = CaptureVirtualDesktop();
    if (!fullBmp) return;

    if (!doNotCopyOnSave) {
        CopyBitmapToClipboard(fullBmp);
    }

    CreateDirectoryW(saveFolder.c_str(), nullptr);
    int seq = captureCounter++;
    std::wstring fileName = FormatFilename(seq) + GetFormatExtension(fullscreenFormat);
    std::wstring fullPath = saveFolder + L"\\" + fileName;

    if (SaveBitmapToPath(fullBmp, fullPath)) {
        lastSavedFilePath = fullPath;
        ShowTrayToast(
            L"Instant Fullscreen Captured (" + std::to_wstring(vScreenW) + L"×" + std::to_wstring(vScreenH) + L")",
            (doNotCopyOnSave ? L"Saved to:\n" : L"Copied to Clipboard & saved to:\n") + fullPath + L"\n(Click notification to reveal in Explorer)"
        );
    }
    delete fullBmp;
}

// ----------------------------------------------------------------------------
// Pixelation & Shadow-Free Annotation Drawers (Ported from annotationDrawers.ts)
// ----------------------------------------------------------------------------

static void ApplyPixelateFilterRawBuffer(DWORD* bufPixels, int bw, int bh, int strideBytes, int rx, int ry, int rw, int rh, int blockSize, bool elliptical) {
    if (!bufPixels || rw <= 2 || rh <= 2) return;
    int x0 = std::max(0, std::min(bw - 1, rx));
    int y0 = std::max(0, std::min(bh - 1, ry));
    int x1 = std::max(0, std::min(bw, rx + rw));
    int y1 = std::max(0, std::min(bh, ry + rh));
    if (x1 <= x0 || y1 <= y0) return;

    blockSize = std::max(2, blockSize);
    int subW = x1 - x0;
    int subH = y1 - y0;
    BYTE* scan0 = (BYTE*)bufPixels + (size_t)y0 * strideBytes + (size_t)x0 * 4;

    float cx = rw * 0.5f;
    float cy = rh * 0.5f;
    float invRadX = 1.0f / std::max(1.0f, rw * 0.5f);
    float invRadY = 1.0f / std::max(1.0f, rh * 0.5f);
    float baseLocalX = (float)(x0 - rx) + 0.5f;
    float baseLocalY = (float)(y0 - ry) + 0.5f;

    for (int by = 0; by < subH; by += blockSize) {
        for (int bx = 0; bx < subW; bx += blockSize) {
            long sumR = 0, sumG = 0, sumB = 0, count = 0;
            int maxY = std::min(subH, by + blockSize);
            int maxX = std::min(subW, bx + blockSize);

            for (int py = by; py < maxY; ++py) {
                DWORD* row = (DWORD*)(scan0 + (size_t)py * strideBytes);
                for (int px = bx; px < maxX; ++px) {
                    DWORD argb = row[px];
                    sumB += (argb & 0xFF);
                    sumG += ((argb >> 8) & 0xFF);
                    sumR += ((argb >> 16) & 0xFF);
                    count++;
                }
            }
            if (count > 0) {
                DWORD outPx = (0xFFu << 24) | ((DWORD)(sumR / count) << 16) | ((DWORD)(sumG / count) << 8) | (DWORD)(sumB / count);
                if (!elliptical) {
                    for (int py = by; py < maxY; ++py) {
                        DWORD* row = (DWORD*)(scan0 + (size_t)py * strideBytes);
                        for (int px = bx; px < maxX; ++px) {
                            row[px] = outPx;
                        }
                    }
                } else {
                    for (int py = by; py < maxY; ++py) {
                        DWORD* row = (DWORD*)(scan0 + (size_t)py * strideBytes);
                        float ny = ((baseLocalY + (float)py) - cy) * invRadY;
                        float nySq = ny * ny;
                        if (nySq > 1.0f) continue;
                        for (int px = bx; px < maxX; ++px) {
                            float nx = ((baseLocalX + (float)px) - cx) * invRadX;
                            if (nx * nx + nySq <= 1.0f) {
                                row[px] = outPx;
                            }
                        }
                    }
                }
            }
        }
    }
}

void PepperSnapDaemon::ApplyPixelateFilter(Bitmap* bmp, int rx, int ry, int rw, int rh, int blockSize, bool elliptical) {
    if (!bmp || rw <= 2 || rh <= 2) return;
    int bw = (int)bmp->GetWidth();
    int bh = (int)bmp->GetHeight();
    Rect lockRect(0, 0, bw, bh);
    BitmapData data;
    if (bmp->LockBits(&lockRect, ImageLockModeRead | ImageLockModeWrite, PixelFormat32bppARGB, &data) != Ok) return;
    ApplyPixelateFilterRawBuffer((DWORD*)data.Scan0, bw, bh, data.Stride, rx, ry, rw, rh, blockSize, elliptical);
    bmp->UnlockBits(&data);
}

void PepperSnapDaemon::DrawVectorAnnotation(Graphics& g, const Annotation& ann, int offsetX, int offsetY, bool isSelected, bool isEditingCaret, bool forceOpaqueHighlighter) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    float sx = ann.startPt.x - offsetX;
    float sy = ann.startPt.y - offsetY;
    float ex = ann.endPt.x   - offsetX;
    float ey = ann.endPt.y   - offsetY;

    switch (ann.tool) {
        case OverlayTool::Pen:
        case OverlayTool::Highlighter: {
            if (ann.points.empty()) break;
            bool isHi = (ann.tool == OverlayTool::Highlighter);
            BYTE alpha = isHi ? (forceOpaqueHighlighter ? 255 : 95) : 255;
            Color c = isHi ? Color(alpha, ann.color.GetR(), ann.color.GetG(), ann.color.GetB()) : ann.color;
            Pen pen(c, isHi ? ann.strokeWidth * 4.5f : ann.strokeWidth);
            pen.SetStartCap(LineCapRound);
            pen.SetEndCap(LineCapRound);
            pen.SetLineJoin(LineJoinRound);

            static thread_local std::vector<PointF> tlsPts;
            static thread_local std::vector<PointF> tlsSmooth;
            static thread_local std::vector<bool> tlsSharp;
            tlsPts.clear();
            if (tlsPts.capacity() < ann.points.size() + 2) tlsPts.reserve(ann.points.size() + 2);
            for (const auto& p : ann.points) tlsPts.emplace_back(p.x - offsetX, p.y - offsetY);

            if (tlsPts.size() == 1) {
                tlsPts.emplace_back(tlsPts[0].X + 0.5f, tlsPts[0].Y);
            }

            GraphicsState gst = g.Save();
            g.SetSmoothingMode(SmoothingModeHighQuality);
            g.SetPixelOffsetMode(PixelOffsetModeHighQuality);

            if (tlsPts.size() == 2) {
                float dx = tlsPts[1].X - tlsPts[0].X;
                float dy = tlsPts[1].Y - tlsPts[0].Y;
                if (dx * dx + dy * dy < 0.25f) tlsPts[1].X += 0.5f;
                g.DrawLine(&pen, tlsPts[0], tlsPts[1]);
            } else if (!g_Daemon.penSmoothingEnabled || g_Daemon.penSmoothingStrength <= 0) {
                // Raw unsmoothed polyline when smoothing is disabled in Options
                g.DrawLines(&pen, tlsPts.data(), (INT)tlsPts.size());
            } else {
                float sNorm = std::max(0.05f, std::min(1.0f, g_Daemon.penSmoothingStrength / 100.0f));
                // Mark vertices belonging to long Shift-snapped straight segments (> 48px) so they stay sharp
                size_t n = tlsPts.size();
                tlsSharp.assign(n, false);
                tlsSharp[0] = true;
                tlsSharp[n - 1] = true;
                for (size_t i = 0; i + 1 < n; ++i) {
                    float dx = tlsPts[i + 1].X - tlsPts[i].X;
                    float dy = tlsPts[i + 1].Y - tlsPts[i].Y;
                    if (dx * dx + dy * dy > 2304.0f) {
                        tlsSharp[i] = true;
                        tlsSharp[i + 1] = true;
                    }
                }

                // Step 1: Continuous strength-scaled Gaussian centerline smoothing (0.25..4.5 passes)
                float totalGauss = sNorm * 4.5f;
                int fullPasses = (int)std::floor(totalGauss);
                float fracPass = totalGauss - (float)fullPasses;
                tlsSmooth.resize(n);
                auto runGaussianPass = [&](float weight) {
                    float sideW = 0.25f * weight;
                    float centerW = 1.0f - 2.0f * sideW;
                    tlsSmooth[0] = tlsPts[0];
                    tlsSmooth[n - 1] = tlsPts[n - 1];
                    for (size_t i = 1; i + 1 < n; ++i) {
                        if (tlsSharp[i]) {
                            tlsSmooth[i] = tlsPts[i];
                        } else {
                            tlsSmooth[i].X = sideW * tlsPts[i - 1].X + centerW * tlsPts[i].X + sideW * tlsPts[i + 1].X;
                            tlsSmooth[i].Y = sideW * tlsPts[i - 1].Y + centerW * tlsPts[i].Y + sideW * tlsPts[i + 1].Y;
                        }
                    }
                    tlsPts.swap(tlsSmooth);
                };
                for (int pass = 0; pass < fullPasses; ++pass) {
                    runGaussianPass(1.0f);
                }
                if (fracPass > 0.01f) {
                    runGaussianPass(fracPass);
                }

                // Step 2: Chaikin quadratic B-spline corner subdivision (1 pass for <= 35%, 2 passes for > 35%)
                int chaikinPasses = (g_Daemon.penSmoothingStrength <= 35) ? 1 : 2;
                for (int pass = 0; pass < chaikinPasses; ++pass) {
                    tlsSmooth.clear();
                    tlsSmooth.reserve(tlsPts.size() * 2);
                    tlsSmooth.push_back(tlsPts.front());
                    for (size_t i = 0; i + 1 < tlsPts.size(); ++i) {
                        const PointF& p0 = tlsPts[i];
                        const PointF& p1 = tlsPts[i + 1];
                        float dx = p1.X - p0.X;
                        float dy = p1.Y - p0.Y;
                        if (dx * dx + dy * dy > 2304.0f) {
                            if (i > 0) tlsSmooth.push_back(p0);
                            tlsSmooth.push_back(p1);
                        } else {
                            tlsSmooth.emplace_back(0.75f * p0.X + 0.25f * p1.X, 0.75f * p0.Y + 0.25f * p1.Y);
                            tlsSmooth.emplace_back(0.25f * p0.X + 0.75f * p1.X, 0.25f * p0.Y + 0.75f * p1.Y);
                        }
                    }
                    tlsSmooth.push_back(tlsPts.back());
                    tlsPts.swap(tlsSmooth);
                }

                g.DrawLines(&pen, tlsPts.data(), (INT)tlsPts.size());
            }
            g.Restore(gst);
            break;
        }

        case OverlayTool::Line: {
            // Pure shadow-free vector line
            Pen pen(ann.color, ann.strokeWidth);
            pen.SetStartCap(LineCapRound);
            pen.SetEndCap(LineCapRound);
            g.DrawLine(&pen, sx, sy, ex, ey);
            break;
        }

        case OverlayTool::Arrow: {
            // Pure shadow-free vector arrow matching annotationDrawers.ts
            float dx = ex - sx, dy = ey - sy;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 2.0f) break;
            float angle = std::atan2(dy, dx);
            float headLen = std::max(14.0f, ann.strokeWidth * 3.8f);
            float headAngle = 0.48f;
            float shaftLen = std::max(0.0f, len - headLen * 0.78f);

            if (shaftLen > 1.0f) {
                Pen pen(ann.color, ann.strokeWidth);
                pen.SetStartCap(LineCapRound);
                pen.SetEndCap(LineCapFlat);
                g.DrawLine(&pen, sx, sy, sx + shaftLen * std::cos(angle), sy + shaftLen * std::sin(angle));
            }

            PointF tri[3] = {
                PointF(ex, ey),
                PointF(ex - headLen * std::cos(angle - headAngle), ey - headLen * std::sin(angle - headAngle)),
                PointF(ex - headLen * std::cos(angle + headAngle), ey - headLen * std::sin(angle + headAngle))
            };
            SolidBrush headBrush(ann.color);
            g.FillPolygon(&headBrush, tri, 3);
            break;
        }

        case OverlayTool::NumberArrow: {
            // Numbered circle badge at startPt + shadow-free arrow pointing to endPt
            float r = std::max(14.0f, ann.strokeWidth * 2.0f + 8.0f);
            float dx = ex - sx, dy = ey - sy;
            float len = std::sqrt(dx * dx + dy * dy);

            if (len > r + 4.0f) {
                float angle = std::atan2(dy, dx);
                float headLen = std::max(14.0f, ann.strokeWidth * 3.8f);
                float headAngle = 0.48f;
                float lineStartX = sx + r * std::cos(angle);
                float lineStartY = sy + r * std::sin(angle);
                float shaftEndDist = std::max(r, len - headLen * 0.78f);

                if (shaftEndDist > r + 1.0f) {
                    Pen pen(ann.color, ann.strokeWidth);
                    pen.SetStartCap(LineCapFlat);
                    pen.SetEndCap(LineCapFlat);
                    g.DrawLine(&pen, lineStartX, lineStartY,
                               sx + shaftEndDist * std::cos(angle),
                               sy + shaftEndDist * std::sin(angle));
                }

                PointF tri[3] = {
                    PointF(ex, ey),
                    PointF(ex - headLen * std::cos(angle - headAngle), ey - headLen * std::sin(angle - headAngle)),
                    PointF(ex - headLen * std::cos(angle + headAngle), ey - headLen * std::sin(angle + headAngle))
                };
                SolidBrush headBrush(ann.color);
                g.FillPolygon(&headBrush, tri, 3);
            }

            // 6. If White color is selected, change Numbering Arrow number color and circle border to Black so it is clearly visible
            bool isWhiteSelected = (ann.color.GetR() > 235 && ann.color.GetG() > 235 && ann.color.GetB() > 235);
            Color contrastCol = isWhiteSelected ? Color(255, 0, 0, 0) : Color(255, 255, 255, 255);

            SolidBrush fill(ann.color);
            g.FillEllipse(&fill, sx - r, sy - r, r * 2.0f, r * 2.0f);
            Pen ring(contrastCol, 2.2f);
            g.DrawEllipse(&ring, sx - r, sy - r, r * 2.0f, r * 2.0f);

            FontFamily ff(L"Segoe UI");
            Font font(&ff, r * 0.95f, FontStyleBold, UnitPixel);
            StringFormat sf;
            sf.SetAlignment(StringAlignmentCenter);
            sf.SetLineAlignment(StringAlignmentCenter);
            SolidBrush fg(contrastCol);
            std::wstring s = std::to_wstring(ann.stepNumber);
            RectF rc(sx - r, sy - r, r * 2.0f, r * 2.0f);
            g.DrawString(s.c_str(), -1, &font, rc, &sf, &fg);
            break;
        }

        case OverlayTool::Rectangle: {
            float x = std::min(sx, ex), y = std::min(sy, ey);
            float w = std::abs(ex - sx), h = std::abs(ey - sy);
            Pen pen(ann.color, ann.strokeWidth);
            pen.SetLineJoin(LineJoinRound);
            g.DrawRectangle(&pen, x, y, w, h);
            break;
        }

        case OverlayTool::Ellipse: {
            float x = std::min(sx, ex), y = std::min(sy, ey);
            float w = std::abs(ex - sx), h = std::abs(ey - sy);
            Pen pen(ann.color, ann.strokeWidth);
            g.DrawEllipse(&pen, x, y, w, h);
            break;
        }

        case OverlayTool::TextBox: {
            float fSize = std::max(10.0f, ann.strokeWidth * 3.8f + 6.0f);
            float lineStep = fSize * 1.32f;
            FontFamily ff(L"Segoe UI");
            Font font(&ff, fSize, FontStyleBold, UnitPixel);
            const StringFormat* typoSf = StringFormat::GenericTypographic();

            if (ann.text.empty() && !isEditingCaret) break;

            std::vector<std::pair<size_t, std::wstring>> lines;
            {
                size_t start = 0;
                for (size_t i = 0; i < ann.text.size(); ++i) {
                    if (ann.text[i] == L'\n') {
                        lines.push_back({ start, ann.text.substr(start, i - start) });
                        start = i + 1;
                    }
                }
                lines.push_back({ start, ann.text.substr(start) });
            }

            float bx = std::min(sx, ex);
            float by = std::min(sy, ey);
            float maxLineW = 0.0f;
            if (ann.text.empty()) {
                maxLineW = MeasureMonoPrefixWidth(g, font, L"Type here...", 12);
            } else {
                for (const auto& ln : lines) {
                    float w = MeasureMonoPrefixWidth(g, font, ln.second, ln.second.size());
                    if (w > maxLineW) maxLineW = w;
                }
            }

            float boxW = std::max(maxLineW + 14.0f, 110.0f);
            float boxH = std::max((float)lines.size() * lineStep + 8.0f, fSize + 10.0f);

            if (isEditingCaret || isSelected) {
                SolidBrush editBg(Color(165, 15, 23, 42));
                g.FillRectangle(&editBg, bx - 5.0f, by - 4.0f, boxW, boxH);
                Pen dashPen(ann.color, 1.5f);
                dashPen.SetDashStyle(DashStyleDash);
                g.DrawRectangle(&dashPen, bx - 5.0f, by - 4.0f, boxW, boxH);
            }

            if (isEditingCaret && ann.text.empty()) {
                SolidBrush placeholderFg(Color(145, 203, 213, 225));
                g.DrawString(L"Type here...", -1, &font, PointF(bx, by), typoSf, &placeholderFg);
                bool blinkOn = ((GetTickCount() / 450) % 2) == 0;
                if (blinkOn) {
                    Pen caretPen(Color(255, 56, 189, 248), 2.0f);
                    g.DrawLine(&caretPen, bx, by, bx, by + fSize * 1.18f);
                }
                break;
            }

            if (isEditingCaret && g_Daemon.textCaretPos != g_Daemon.textSelAnchor && !ann.text.empty()) {
                size_t s0 = std::min(g_Daemon.textCaretPos, g_Daemon.textSelAnchor);
                size_t s1 = std::max(g_Daemon.textCaretPos, g_Daemon.textSelAnchor);
                SolidBrush selHighlight(Color(200, 2, 132, 199));
                for (size_t i = 0; i < lines.size(); ++i) {
                    size_t lStart = lines[i].first;
                    size_t lEnd = lStart + lines[i].second.size();
                    if (s1 > lStart && s0 < lEnd) {
                        size_t c0 = std::max(s0, lStart) - lStart;
                        size_t c1 = std::min(s1, lEnd) - lStart;
                        float x0 = bx + MeasureMonoPrefixWidth(g, font, lines[i].second, c0);
                        float x1 = bx + MeasureMonoPrefixWidth(g, font, lines[i].second, c1);
                        g.FillRectangle(&selHighlight, x0, by + (float)i * lineStep, std::max(2.0f, x1 - x0), lineStep);
                    }
                }
            }

            SolidBrush fg(ann.color);
            for (size_t i = 0; i < lines.size(); ++i) {
                if (!lines[i].second.empty()) {
                    g.DrawString(lines[i].second.c_str(), -1, &font, PointF(bx, by + (float)i * lineStep), typoSf, &fg);
                }
            }

            if (isEditingCaret) {
                bool blinkOn = ((GetTickCount() / 450) % 2) == 0;
                if (blinkOn) {
                    size_t cPos = std::min(g_Daemon.textCaretPos, ann.text.size());
                    size_t caretLine = 0, caretCol = 0;
                    for (size_t i = 0; i < lines.size(); ++i) {
                        size_t lStart = lines[i].first;
                        size_t lEnd = lStart + lines[i].second.size();
                        if (cPos >= lStart && cPos <= lEnd) {
                            caretLine = i;
                            caretCol = cPos - lStart;
                            break;
                        }
                    }
                    float cx = bx + MeasureMonoPrefixWidth(g, font, lines[caretLine].second, caretCol);
                    float cy = by + (float)caretLine * lineStep;
                    Pen caretPen(Color(255, 56, 189, 248), 2.0f);
                    g.DrawLine(&caretPen, cx, cy, cx, cy + fSize * 1.18f);
                }
            }
            break;
        }

        default:
            break;
    }

    if (isSelected && ann.tool != OverlayTool::TextBox) {
        Pen selPen(Color(255, 56, 189, 248), 1.5f);
        selPen.SetDashStyle(DashStyleDash);
        float x = std::min(sx, ex) - 6.0f;
        float y = std::min(sy, ey) - 6.0f;
        float w = std::max(16.0f, std::abs(ex - sx) + 12.0f);
        float h = std::max(16.0f, std::abs(ey - sy) + 12.0f);
        g.DrawRectangle(&selPen, x, y, w, h);

        // Endpoint handles for lines, arrows, and number-arrows
        SolidBrush handleFill(Color(255, 255, 255, 255));
        Pen handleBorder(Color(255, 56, 189, 248), 1.5f);
        g.FillEllipse(&handleFill, sx - 5.0f, sy - 5.0f, 10.0f, 10.0f);
        g.DrawEllipse(&handleBorder, sx - 5.0f, sy - 5.0f, 10.0f, 10.0f);
        g.FillEllipse(&handleFill, ex - 5.0f, ey - 5.0f, 10.0f, 10.0f);
        g.DrawEllipse(&handleBorder, ex - 5.0f, ey - 5.0f, 10.0f, 10.0f);
    }
}

void PepperSnapDaemon::RenderHighlighterLayer(Graphics& destG, int regionX, int regionY, int regionW, int regionH, const std::vector<Annotation>& anns, const Annotation* draft, bool nonStacking) {
    bool hasHi = (draft && draft->tool == OverlayTool::Highlighter);
    if (!hasHi) {
        for (const auto& a : anns) {
            if (a.tool == OverlayTool::Highlighter) { hasHi = true; break; }
        }
    }
    if (!hasHi || regionW <= 0 || regionH <= 0) return;

    if (!nonStacking) {
        for (const auto& a : anns) {
            if (a.tool == OverlayTool::Highlighter) {
                DrawVectorAnnotation(destG, a, regionX, regionY, false, false, false);
            }
        }
        if (draft && draft->tool == OverlayTool::Highlighter) {
            DrawVectorAnnotation(destG, *draft, regionX, regionY, false, false, false);
        }
        return;
    }

    // 9. Non-Stacking Highlighter: render all highlighter strokes at 100% opaque onto a tight-bounds offscreen layer,
    //    then composite the tight layer once at uniform 37% opacity so overlapping strokes never darken!
    float minX = (float)(regionX + regionW);
    float minY = (float)(regionY + regionH);
    float maxX = (float)regionX;
    float maxY = (float)regionY;

    auto expandBounds = [&](const Annotation& a) {
        float pad = std::max(16.0f, a.strokeWidth * 3.0f + 8.0f);
        if (!a.points.empty()) {
            for (const auto& pt : a.points) {
                minX = std::min(minX, pt.x - pad);
                minY = std::min(minY, pt.y - pad);
                maxX = std::max(maxX, pt.x + pad);
                maxY = std::max(maxY, pt.y + pad);
            }
        } else {
            minX = std::min(minX, std::min(a.startPt.x, a.endPt.x) - pad);
            minY = std::min(minY, std::min(a.startPt.y, a.endPt.y) - pad);
            maxX = std::max(maxX, std::max(a.startPt.x, a.endPt.x) + pad);
            maxY = std::max(maxY, std::max(a.startPt.y, a.endPt.y) + pad);
        }
    };

    for (const auto& a : anns) {
        if (a.tool == OverlayTool::Highlighter) expandBounds(a);
    }
    if (draft && draft->tool == OverlayTool::Highlighter) {
        expandBounds(*draft);
    }

    int bLeft   = std::max(regionX, (int)std::floor(minX));
    int bTop    = std::max(regionY, (int)std::floor(minY));
    int bRight  = std::min(regionX + regionW, (int)std::ceil(maxX));
    int bBottom = std::min(regionY + regionH, (int)std::ceil(maxY));
    int bW = bRight - bLeft;
    int bH = bBottom - bTop;
    if (bW <= 0 || bH <= 0) return;

    static Bitmap* s_hiLayer = nullptr;
    static int s_hiCapW = 0;
    static int s_hiCapH = 0;
    if (!s_hiLayer || bW > s_hiCapW || bH > s_hiCapH) {
        delete s_hiLayer;
        s_hiCapW = std::max(bW, 256);
        s_hiCapH = std::max(bH, 256);
        s_hiLayer = new Bitmap(s_hiCapW, s_hiCapH, PixelFormat32bppARGB);
    }
    Graphics hg(s_hiLayer);
    hg.SetCompositingMode(CompositingModeSourceCopy);
    SolidBrush clearBrush(Color(0, 0, 0, 0));
    hg.FillRectangle(&clearBrush, 0, 0, bW, bH);
    hg.SetCompositingMode(CompositingModeSourceOver);
    hg.SetSmoothingMode(SmoothingModeAntiAlias);

    for (const auto& a : anns) {
        if (a.tool == OverlayTool::Highlighter) {
            DrawVectorAnnotation(hg, a, bLeft, bTop, false, false, true);
        }
    }
    if (draft && draft->tool == OverlayTool::Highlighter) {
        DrawVectorAnnotation(hg, *draft, bLeft, bTop, false, false, true);
    }

    ColorMatrix cm = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 95.0f / 255.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f
    };
    ImageAttributes ia;
    ia.SetColorMatrix(&cm, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
    Rect destRc(bLeft - regionX, bTop - regionY, bW, bH);
    destG.DrawImage(s_hiLayer, destRc, 0, 0, bW, bH, UnitPixel, &ia);
}

void PepperSnapDaemon::RecalcNextStepNum() {
    int maxStep = 0;
    for (const auto& a : annotations) {
        if (a.tool == OverlayTool::NumberArrow && a.stepNumber > maxStep) {
            maxStep = a.stepNumber;
        }
    }
    nextStepNum = maxStep + 1;
}

void PepperSnapDaemon::CommitActiveTextBox() {
    if (!isEditingText) return;
    isEditingText = false;
    isDraggingTextBoxSel = false;
    textCaretPos = textSelAnchor = 0;
    if (!editingTextAnn.text.empty()) {
        PushUndo();
        float fSize = std::max(10.0f, editingTextAnn.strokeWidth * 3.8f + 6.0f);
        float lineStep = fSize * 1.32f;
        FontFamily ff(L"Segoe UI");
        Font font(&ff, fSize, FontStyleBold, UnitPixel);
        HDC hdc = GetDC(nullptr);
        Graphics g(hdc);
        float maxW = 0.0f;
        int lineCount = 1;
        size_t start = 0;
        for (size_t i = 0; i <= editingTextAnn.text.size(); ++i) {
            if (i == editingTextAnn.text.size() || editingTextAnn.text[i] == L'\n') {
                std::wstring ln = editingTextAnn.text.substr(start, i - start);
                float w = MeasureMonoPrefixWidth(g, font, ln, ln.size());
                if (w > maxW) maxW = w;
                if (i < editingTextAnn.text.size()) lineCount++;
                start = i + 1;
            }
        }
        ReleaseDC(nullptr, hdc);
        float bx = std::min(editingTextAnn.startPt.x, editingTextAnn.endPt.x);
        float by = std::min(editingTextAnn.startPt.y, editingTextAnn.endPt.y);
        editingTextAnn.startPt = { bx, by };
        editingTextAnn.endPt = { bx + std::max(maxW + 14.0f, 40.0f), by + lineCount * lineStep + 8.0f };
        annotations.push_back(editingTextAnn);
    }
    editingTextAnn = Annotation();
}

RECT PepperSnapDaemon::GetEditingTextBoxRect() const {
    float bx = std::min(editingTextAnn.startPt.x, editingTextAnn.endPt.x);
    float by = std::min(editingTextAnn.startPt.y, editingTextAnn.endPt.y);
    float fSize = std::max(10.0f, editingTextAnn.strokeWidth * 3.8f + 6.0f);
    float lineStep = fSize * 1.32f;
    FontFamily ff(L"Segoe UI");
    Font font(&ff, fSize, FontStyleBold, UnitPixel);
    HDC hdc = GetDC(nullptr);
    Graphics g(hdc);
    float maxW = 0.0f;
    int lineCount = 1;
    if (editingTextAnn.text.empty()) {
        maxW = MeasureMonoPrefixWidth(g, font, L"Type here...", 12);
    } else {
        size_t start = 0;
        for (size_t i = 0; i <= editingTextAnn.text.size(); ++i) {
            if (i == editingTextAnn.text.size() || editingTextAnn.text[i] == L'\n') {
                std::wstring ln = editingTextAnn.text.substr(start, i - start);
                float w = MeasureMonoPrefixWidth(g, font, ln, ln.size());
                if (w > maxW) maxW = w;
                if (i < editingTextAnn.text.size()) lineCount++;
                start = i + 1;
            }
        }
    }
    ReleaseDC(nullptr, hdc);
    float boxW = std::max(maxW + 14.0f, 110.0f);
    float boxH = std::max(lineCount * lineStep + 8.0f, fSize + 10.0f);
    return { (LONG)(bx - 6.0f), (LONG)(by - 5.0f), (LONG)(bx + boxW + 6.0f), (LONG)(by + boxH + 5.0f) };
}

size_t PepperSnapDaemon::HitTestTextBoxIndex(int mouseX, int mouseY) const {
    const std::wstring& s = editingTextAnn.text;
    if (s.empty()) return 0;
    float bx = std::min(editingTextAnn.startPt.x, editingTextAnn.endPt.x);
    float by = std::min(editingTextAnn.startPt.y, editingTextAnn.endPt.y);
    float fSize = std::max(10.0f, editingTextAnn.strokeWidth * 3.8f + 6.0f);
    float lineStep = fSize * 1.32f;

    std::vector<std::pair<size_t, std::wstring>> lines;
    size_t start = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\n') {
            lines.push_back({ start, s.substr(start, i - start) });
            start = i + 1;
        }
    }
    lines.push_back({ start, s.substr(start) });

    int lineIdx = (int)std::floor(((float)mouseY - by) / lineStep);
    lineIdx = std::max(0, std::min((int)lines.size() - 1, lineIdx));

    const std::wstring& ln = lines[lineIdx].second;
    float relX = (float)mouseX - bx;
    if (relX <= 0.0f || ln.empty()) return lines[lineIdx].first;

    FontFamily ff(L"Segoe UI");
    Font font(&ff, fSize, FontStyleBold, UnitPixel);
    HDC hdc = GetDC(nullptr);
    Graphics g(hdc);
    size_t col = ln.size();
    float prevX = 0.0f;
    for (size_t i = 0; i < ln.size(); ++i) {
        float nextX = MeasureMonoPrefixWidth(g, font, ln, i + 1);
        if (relX < (prevX + nextX) * 0.5f) {
            col = i;
            break;
        }
        prevX = nextX;
    }
    ReleaseDC(nullptr, hdc);
    return lines[lineIdx].first + col;
}

void PepperSnapDaemon::ApplyTextBoxKeyDown(WPARAM wParam, bool ctrl, bool shift, HWND hWnd) {
    std::wstring& text = editingTextAnn.text;
    textCaretPos = std::min(textCaretPos, text.size());
    textSelAnchor = std::min(textSelAnchor, text.size());

    auto deleteSelection = [&]() -> bool {
        if (textCaretPos == textSelAnchor) return false;
        size_t s0 = std::min(textCaretPos, textSelAnchor);
        size_t s1 = std::max(textCaretPos, textSelAnchor);
        text.erase(s0, s1 - s0);
        textCaretPos = textSelAnchor = s0;
        return true;
    };

    if (ctrl) {
        if (wParam == 'A') {
            textSelAnchor = 0;
            textCaretPos = text.size();
            return;
        }
        if ((wParam == 'C' || wParam == 'X') && textCaretPos != textSelAnchor) {
            size_t s0 = std::min(textCaretPos, textSelAnchor);
            size_t s1 = std::max(textCaretPos, textSelAnchor);
            std::wstring sub = text.substr(s0, s1 - s0);
            if (OpenClipboard(hWnd)) {
                EmptyClipboard();
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (sub.size() + 1) * sizeof(wchar_t));
                if (hMem) {
                    wchar_t* dst = (wchar_t*)GlobalLock(hMem);
                    if (dst) {
                        memcpy(dst, sub.c_str(), (sub.size() + 1) * sizeof(wchar_t));
                        GlobalUnlock(hMem);
                        SetClipboardData(CF_UNICODETEXT, hMem);
                    }
                }
                CloseClipboard();
            }
            if (wParam == 'X') deleteSelection();
            return;
        }
        if (wParam == 'V') {
            if (OpenClipboard(hWnd)) {
                HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                if (hData) {
                    const wchar_t* psz = (const wchar_t*)GlobalLock(hData);
                    if (psz) {
                        deleteSelection();
                        for (size_t i = 0; psz[i] != 0; ++i) {
                            wchar_t c = psz[i];
                            if (c == L'\r') continue;
                            if (c >= 32 || c == L'\n') {
                                text.insert(textCaretPos, 1, c);
                                textCaretPos++;
                            }
                        }
                        textSelAnchor = textCaretPos;
                        GlobalUnlock(hData);
                    }
                }
                CloseClipboard();
            }
            return;
        }
    }

    std::vector<std::pair<size_t, size_t>> lineSpans;
    {
        size_t start = 0;
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == L'\n') {
                lineSpans.push_back({ start, i - start });
                start = i + 1;
            }
        }
        lineSpans.push_back({ start, text.size() - start });
    }
    size_t curLine = 0, curCol = 0;
    for (size_t i = 0; i < lineSpans.size(); ++i) {
        if (textCaretPos >= lineSpans[i].first && textCaretPos <= lineSpans[i].first + lineSpans[i].second) {
            curLine = i;
            curCol = textCaretPos - lineSpans[i].first;
            break;
        }
    }

    switch (wParam) {
        case VK_LEFT:
            if (!shift && textCaretPos != textSelAnchor) {
                textCaretPos = textSelAnchor = std::min(textCaretPos, textSelAnchor);
            } else {
                if (textCaretPos > 0) textCaretPos--;
                if (!shift) textSelAnchor = textCaretPos;
            }
            break;
        case VK_RIGHT:
            if (!shift && textCaretPos != textSelAnchor) {
                textCaretPos = textSelAnchor = std::max(textCaretPos, textSelAnchor);
            } else {
                if (textCaretPos < text.size()) textCaretPos++;
                if (!shift) textSelAnchor = textCaretPos;
            }
            break;
        case VK_HOME:
            textCaretPos = lineSpans[curLine].first;
            if (!shift) textSelAnchor = textCaretPos;
            break;
        case VK_END:
            textCaretPos = lineSpans[curLine].first + lineSpans[curLine].second;
            if (!shift) textSelAnchor = textCaretPos;
            break;
        case VK_UP:
            if (curLine > 0) {
                textCaretPos = lineSpans[curLine - 1].first + std::min(curCol, lineSpans[curLine - 1].second);
            } else {
                textCaretPos = 0;
            }
            if (!shift) textSelAnchor = textCaretPos;
            break;
        case VK_DOWN:
            if (curLine + 1 < lineSpans.size()) {
                textCaretPos = lineSpans[curLine + 1].first + std::min(curCol, lineSpans[curLine + 1].second);
            } else {
                textCaretPos = text.size();
            }
            if (!shift) textSelAnchor = textCaretPos;
            break;
        case VK_BACK:
            if (!deleteSelection() && textCaretPos > 0) {
                text.erase(textCaretPos - 1, 1);
                textCaretPos--;
                textSelAnchor = textCaretPos;
            }
            break;
        case VK_DELETE:
            if (!deleteSelection() && textCaretPos < text.size()) {
                text.erase(textCaretPos, 1);
            }
            break;
        default:
            break;
    }
}

void PepperSnapDaemon::ApplyStrokeToSelectedAnnotation(float newStroke, bool recordUndo) {
    newStroke = std::max(1.0f, std::min(120.0f, newStroke));
    activeStroke = newStroke;
    if (isEditingText) {
        editingTextAnn.strokeWidth = newStroke;
    }
    if (selectedAnnotationId == -1) return;
    for (auto& a : annotations) {
        if (a.id == selectedAnnotationId) {
            if (recordUndo && std::abs(a.strokeWidth - newStroke) > 0.05f) {
                PushUndo();
            }
            a.strokeWidth = newStroke;
            if (a.tool == OverlayTool::TextBox) {
                FontFamily ff(L"Segoe UI");
                float fSize = std::max(10.0f, a.strokeWidth * 3.8f + 6.0f);
                Font font(&ff, fSize, FontStyleBold, UnitPixel);
                HDC hdc = GetDC(nullptr);
                Graphics g(hdc);
                RectF bounds;
                g.MeasureString(a.text.c_str(), -1, &font, PointF(a.startPt.x, a.startPt.y), &bounds);
                ReleaseDC(nullptr, hdc);
                a.endPt = { a.startPt.x + bounds.Width + 12.0f, a.startPt.y + bounds.Height + 8.0f };
            }
            break;
        }
    }
}

void PepperSnapDaemon::CommitActiveStrokeInput() {
    if (!isEditingStroke) return;
    isEditingStroke = false;
    isDraggingStrokeText = false;
    std::wstring s = editingStrokeText;
    editingStrokeText.clear();
    strokeCaretPos = strokeSelAnchor = 0;
    for (wchar_t& c : s) {
        if ((c < L'0' || c > L'9') && c != L'.') c = L' ';
    }
    std::wstringstream ss(s);
    float val = 0.0f;
    if ((ss >> val) && val >= 1.0f) {
        ApplyStrokeToSelectedAnnotation(std::max(1.0f, std::min(120.0f, val)), true);
    }
}

void PepperSnapDaemon::CommitActiveSizeInput() {
    if (!isEditingSize) return;
    isEditingSize = false;
    isDraggingSizeText = false;
    std::wstring s = editingSizeText;
    editingSizeText.clear();
    sizeCaretPos = sizeSelAnchor = 0;
    for (wchar_t& c : s) {
        if (c < L'0' || c > L'9') c = L' ';
    }
    std::wstringstream ss(s);
    int newW = 0, newH = 0;
    if ((ss >> newW) && newW >= 1) {
        if (!(ss >> newH) || newH < 1) {
            newH = newW; // 1:1 square if a single number is entered
        }
        int sx = std::min(selRect.left, selRect.right);
        int sy = std::min(selRect.top, selRect.bottom);
        newW = std::min(vScreenW - sx, std::max(1, newW));
        newH = std::min(vScreenH - sy, std::max(1, newH));
        selRect = { sx, sy, sx + newW, sy + newH };
        hasSelection = true;
        hasCustomHudPos = false;
    }
}

float PepperSnapDaemon::MeasureMonoPrefixWidth(Graphics& g, const Font& monoFont, const std::wstring& s, size_t count) {
    if (count == 0 || s.empty()) return 0.0f;
    size_t n = std::min(count, s.size());
    const StringFormat* sf = StringFormat::GenericTypographic();
    RectF r1, r0;
    std::wstring wrapped = L"|" + s.substr(0, n) + L"|";
    g.MeasureString(wrapped.c_str(), -1, &monoFont, PointF(0, 0), sf, &r1);
    g.MeasureString(L"||", -1, &monoFont, PointF(0, 0), sf, &r0);
    return std::max(0.0f, r1.Width - r0.Width);
}

size_t PepperSnapDaemon::HitTestMonoIndex(int mouseX, int textLeftX, const std::wstring& s) {
    if (s.empty() || mouseX <= textLeftX) return 0;
    HDC hdc = GetDC(nullptr);
    Graphics g(hdc);
    Font monoFont(L"Consolas", 11.5f, FontStyleBold, UnitPixel);
    float relX = (float)(mouseX - textLeftX);
    size_t result = s.size();
    for (size_t i = 0; i < s.size(); ++i) {
        float x0 = MeasureMonoPrefixWidth(g, monoFont, s, i);
        float x1 = MeasureMonoPrefixWidth(g, monoFont, s, i + 1);
        if (relX < (x0 + x1) * 0.5f) {
            result = i;
            break;
        }
    }
    ReleaseDC(nullptr, hdc);
    return result;
}

void PepperSnapDaemon::DrawEditablePillText(
    Graphics& g, const Font& monoFont,
    int textLeftX, int textTopY, int boxTopY, int boxH,
    const std::wstring& text, const std::wstring& suffix,
    bool isEditing, size_t caretPos, size_t selAnchor
) {
    const StringFormat* sf = StringFormat::GenericTypographic();
    SolidBrush whiteBrush(Color(255, 248, 250, 252));
    SolidBrush suffixBrush(isEditing ? Color(255, 148, 163, 184) : Color(255, 248, 250, 252));

    if (isEditing && caretPos != selAnchor && !text.empty()) {
        size_t s0 = std::min(caretPos, selAnchor);
        size_t s1 = std::max(caretPos, selAnchor);
        float x0 = (float)textLeftX + MeasureMonoPrefixWidth(g, monoFont, text, s0);
        float x1 = (float)textLeftX + MeasureMonoPrefixWidth(g, monoFont, text, s1);
        SolidBrush selHighlight(Color(200, 2, 132, 199)); // Sky-600 selection highlight
        g.FillRectangle(&selHighlight, x0, (float)(boxTopY + 3), std::max(2.0f, x1 - x0), (float)(boxH - 6));
    }

    g.DrawString(text.c_str(), -1, &monoFont, PointF((float)textLeftX, (float)textTopY), sf, &whiteBrush);
    float textW = MeasureMonoPrefixWidth(g, monoFont, text, text.size());
    g.DrawString(suffix.c_str(), -1, &monoFont, PointF((float)textLeftX + textW, (float)textTopY), sf, &suffixBrush);

    if (isEditing) {
        bool blinkOn = ((GetTickCount() / 450) % 2) == 0;
        if (blinkOn) {
            float cx = (float)textLeftX + MeasureMonoPrefixWidth(g, monoFont, text, caretPos);
            Pen caretPen(Color(255, 56, 189, 248), 1.8f);
            g.DrawLine(&caretPen, cx, (float)(boxTopY + 3), cx, (float)(boxTopY + boxH - 3));
        }
    }
}

void PepperSnapDaemon::ApplyInlineEditChar(wchar_t ch, std::wstring& text, size_t& caret, size_t& anchor, size_t maxLen) {
    caret = std::min(caret, text.size());
    anchor = std::min(anchor, text.size());
    if (caret != anchor) {
        size_t s0 = std::min(caret, anchor);
        size_t s1 = std::max(caret, anchor);
        text.erase(s0, s1 - s0);
        caret = anchor = s0;
    }
    if (text.size() < maxLen) {
        text.insert(caret, 1, ch);
        caret++;
        anchor = caret;
    }
}

void PepperSnapDaemon::ApplyInlineEditKeyDown(
    WPARAM wParam, bool ctrl, bool shift,
    std::wstring& text, size_t& caret, size_t& anchor,
    size_t maxLen, bool isSizeField, HWND hWnd
) {
    caret = std::min(caret, text.size());
    anchor = std::min(anchor, text.size());

    auto deleteSelection = [&]() -> bool {
        if (caret == anchor) return false;
        size_t s0 = std::min(caret, anchor);
        size_t s1 = std::max(caret, anchor);
        text.erase(s0, s1 - s0);
        caret = anchor = s0;
        return true;
    };

    if (ctrl) {
        if (wParam == 'A') {
            anchor = 0;
            caret = text.size();
            return;
        }
        if (wParam == 'C' && caret != anchor) {
            size_t s0 = std::min(caret, anchor);
            size_t s1 = std::max(caret, anchor);
            std::wstring sub = text.substr(s0, s1 - s0);
            if (OpenClipboard(hWnd)) {
                EmptyClipboard();
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (sub.size() + 1) * sizeof(wchar_t));
                if (hMem) {
                    wchar_t* dst = (wchar_t*)GlobalLock(hMem);
                    if (dst) {
                        memcpy(dst, sub.c_str(), (sub.size() + 1) * sizeof(wchar_t));
                        GlobalUnlock(hMem);
                        SetClipboardData(CF_UNICODETEXT, hMem);
                    }
                }
                CloseClipboard();
            }
            return;
        }
        if (wParam == 'V') {
            if (OpenClipboard(hWnd)) {
                HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                if (hData) {
                    const wchar_t* psz = (const wchar_t*)GlobalLock(hData);
                    if (psz) {
                        deleteSelection();
                        for (size_t i = 0; psz[i] != 0 && text.size() < maxLen; ++i) {
                            wchar_t c = psz[i];
                            bool valid = (c >= L'0' && c <= L'9') ||
                                         (isSizeField ? (c == L'x' || c == L'X' || c == L'*' || c == L',' || c == L' ') : (c == L'.'));
                            if (valid) {
                                if (isSizeField && c == L'X') c = L'x';
                                text.insert(caret, 1, c);
                                caret++;
                            }
                        }
                        anchor = caret;
                        GlobalUnlock(hData);
                    }
                }
                CloseClipboard();
            }
            return;
        }
    }

    switch (wParam) {
        case VK_LEFT:
            if (!shift && caret != anchor) {
                caret = anchor = std::min(caret, anchor);
            } else {
                if (caret > 0) caret--;
                if (!shift) anchor = caret;
            }
            break;
        case VK_RIGHT:
            if (!shift && caret != anchor) {
                caret = anchor = std::max(caret, anchor);
            } else {
                if (caret < text.size()) caret++;
                if (!shift) anchor = caret;
            }
            break;
        case VK_HOME:
        case VK_UP:
            caret = 0;
            if (!shift) anchor = caret;
            break;
        case VK_END:
        case VK_DOWN:
            caret = text.size();
            if (!shift) anchor = caret;
            break;
        case VK_BACK:
            if (!deleteSelection() && caret > 0) {
                text.erase(caret - 1, 1);
                caret--;
                anchor = caret;
            }
            break;
        case VK_DELETE:
            if (!deleteSelection() && caret < text.size()) {
                text.erase(caret, 1);
            }
            break;
        default:
            break;
    }
}

int PepperSnapDaemon::HitTestAnnotation(float x, float y, DragMode* outHandleMode) const {
    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
    for (int i = (int)annotations.size() - 1; i >= 0; --i) {
        const auto& a = annotations[i];
        if (a.tool != OverlayTool::Pen && a.tool != OverlayTool::Highlighter) {
            float dxs = x - a.startPt.x, dys = y - a.startPt.y;
            if (dxs * dxs + dys * dys <= 100.0f) {
                if (outHandleMode) *outHandleMode = DragMode::DraggingAnnotationStart;
                return a.id;
            }
            float dxe = x - a.endPt.x, dye = y - a.endPt.y;
            if (dxe * dxe + dye * dye <= 100.0f) {
                if (outHandleMode) *outHandleMode = DragMode::DraggingAnnotationEnd;
                return a.id;
            }
        } else {
            float hitR = std::max(10.0f, a.strokeWidth * 1.5f);
            float hitRSq = hitR * hitR;
            for (const auto& pt : a.points) {
                float dx = x - pt.x, dy = y - pt.y;
                if (dx * dx + dy * dy <= hitRSq) {
                    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                    return a.id;
                }
            }
        }
        float minX = std::min(a.startPt.x, a.endPt.x) - 12.0f;
        float maxX = std::max(a.startPt.x, a.endPt.x) + 12.0f;
        float minY = std::min(a.startPt.y, a.endPt.y) - 12.0f;
        float maxY = std::max(a.startPt.y, a.endPt.y) + 12.0f;
        if (x >= minX && x <= maxX && y >= minY && y <= maxY) {
            if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
            return a.id;
        }
    }
    return -1;
}

Bitmap* PepperSnapDaemon::RenderCroppedRegionBitmap() {
    CommitActiveTextBox();
    if (!frozenDesktopBmp || !hasSelection) return nullptr;
    int rx = std::max(0, (int)std::min(selRect.left, selRect.right));
    int ry = std::max(0, (int)std::min(selRect.top, selRect.bottom));
    int rw = std::min(vScreenW - rx, (int)std::abs(selRect.right - selRect.left));
    int rh = std::min(vScreenH - ry, (int)std::abs(selRect.bottom - selRect.top));
    if (rw < 1 || rh < 1) return nullptr;

    Bitmap* out = frozenDesktopBmp->Clone(rx, ry, rw, rh, PixelFormat32bppARGB);
    if (!out) return nullptr;

    // Pass 1: Real-time pixelation filters (Mosaic Square & Mosaic Circle) applied directly to cropped bitmap
    for (const auto& ann : annotations) {
        if (ann.tool == OverlayTool::MosaicSquare || ann.tool == OverlayTool::MosaicCircle) {
            int px = (int)std::min(ann.startPt.x, ann.endPt.x) - rx;
            int py = (int)std::min(ann.startPt.y, ann.endPt.y) - ry;
            int pw = (int)std::abs(ann.endPt.x - ann.startPt.x);
            int ph = (int)std::abs(ann.endPt.y - ann.startPt.y);
            int block = GetMosaicBlockSize(ann.strokeWidth);
            ApplyPixelateFilter(out, px, py, pw, ph, block, ann.tool == OverlayTool::MosaicCircle);
        }
    }

    // Pass 2: Highlighter layer (supports Non-Stacking or Stacking mode) + Vector & Text annotations
    Graphics g(out);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    RenderHighlighterLayer(g, rx, ry, rw, rh, annotations, nullptr, nonStackingHighlighter);
    for (const auto& ann : annotations) {
        if (ann.tool != OverlayTool::MosaicSquare &&
            ann.tool != OverlayTool::MosaicCircle &&
            ann.tool != OverlayTool::Highlighter) {
            DrawVectorAnnotation(g, ann, rx, ry, false, false);
        }
    }
    return out;
}

// ----------------------------------------------------------------------------
// Vector Tool Icons, Hover Speech-Bubble Tooltip & Docked HUD Builder
// ----------------------------------------------------------------------------

void PepperSnapDaemon::DrawDockButtonIcon(Graphics& g, const DockButton& b, const RectF& rf) {
    float cx = rf.X + rf.Width * 0.5f;
    float cy = rf.Y + rf.Height * 0.5f;
    Pen whitePen(Color(255, 248, 250, 252), 1.8f);
    whitePen.SetStartCap(LineCapRound);
    whitePen.SetEndCap(LineCapRound);
    whitePen.SetLineJoin(LineJoinRound);
    SolidBrush whiteBrush(Color(255, 248, 250, 252));

    if (b.isTool) {
        switch (b.tool) {
            case OverlayTool::SelectMove: {
                // Cursor arrow icon
                PointF pts[7] = {
                    PointF(cx - 5.0f, cy - 8.0f),
                    PointF(cx - 5.0f, cy + 6.0f),
                    PointF(cx - 1.5f, cy + 3.0f),
                    PointF(cx + 1.5f, cy + 8.5f),
                    PointF(cx + 3.8f, cy + 7.2f),
                    PointF(cx + 0.8f, cy + 1.8f),
                    PointF(cx + 5.5f, cy + 1.8f)
                };
                g.FillPolygon(&whiteBrush, pts, 7);
                break;
            }
            case OverlayTool::Pen: {
                // Pen icon angled down-left
                PointF body[4] = {
                    PointF(cx - 3.0f, cy + 1.0f),
                    PointF(cx + 4.5f, cy - 6.5f),
                    PointF(cx + 7.5f, cy - 3.5f),
                    PointF(cx + 0.0f, cy + 4.0f)
                };
                g.DrawPolygon(&whitePen, body, 4);
                PointF tip[3] = {
                    PointF(cx - 7.0f, cy + 7.5f),
                    PointF(cx - 3.0f, cy + 1.0f),
                    PointF(cx + 0.0f, cy + 4.0f)
                };
                g.FillPolygon(&whiteBrush, tip, 3);
                break;
            }
            case OverlayTool::Highlighter: {
                // Stabilo chisel-tip marker icon
                PointF body[4] = {
                    PointF(cx - 2.5f, cy + 0.5f),
                    PointF(cx + 3.5f, cy - 5.5f),
                    PointF(cx + 7.5f, cy - 1.5f),
                    PointF(cx + 1.5f, cy + 4.5f)
                };
                SolidBrush hiFill(Color(255, 250, 204, 21));
                g.FillPolygon(&hiFill, body, 4);
                g.DrawPolygon(&whitePen, body, 4);
                PointF chisel[4] = {
                    PointF(cx - 7.0f, cy + 6.5f),
                    PointF(cx - 3.5f, cy + 1.5f),
                    PointF(cx + 0.5f, cy + 5.5f),
                    PointF(cx - 3.5f, cy + 7.5f)
                };
                g.FillPolygon(&whiteBrush, chisel, 4);
                break;
            }
            case OverlayTool::Line: {
                g.DrawLine(&whitePen, cx - 6.5f, cy + 6.0f, cx + 6.5f, cy - 6.0f);
                g.FillEllipse(&whiteBrush, cx - 8.0f, cy + 4.5f, 3.5f, 3.5f);
                g.FillEllipse(&whiteBrush, cx + 4.5f, cy - 8.0f, 3.5f, 3.5f);
                break;
            }
            case OverlayTool::Arrow: {
                g.DrawLine(&whitePen, cx - 6.5f, cy + 6.0f, cx + 4.5f, cy - 5.0f);
                PointF head[3] = {
                    PointF(cx + 7.0f, cy - 7.0f),
                    PointF(cx + 0.5f, cy - 6.5f),
                    PointF(cx + 6.0f, cy - 0.5f)
                };
                g.FillPolygon(&whiteBrush, head, 3);
                break;
            }
            case OverlayTool::NumberArrow: {
                // Circle badge with '1' + small arrow
                g.DrawEllipse(&whitePen, cx - 8.5f, cy - 3.5f, 9.5f, 9.5f);
                FontFamily ff(L"Segoe UI");
                Font f(&ff, 8.5f, FontStyleBold, UnitPixel);
                g.DrawString(L"1", -1, &f, PointF(cx - 6.8f, cy - 4.2f), &whiteBrush);
                g.DrawLine(&whitePen, cx + 1.0f, cy - 0.5f, cx + 7.5f, cy - 6.0f);
                PointF head[3] = {
                    PointF(cx + 8.5f, cy - 7.0f),
                    PointF(cx + 3.5f, cy - 7.0f),
                    PointF(cx + 8.0f, cy - 2.0f)
                };
                g.FillPolygon(&whiteBrush, head, 3);
                break;
            }
            case OverlayTool::Rectangle: {
                g.DrawRectangle(&whitePen, cx - 7.0f, cy - 5.5f, 14.0f, 11.0f);
                break;
            }
            case OverlayTool::Ellipse: {
                g.DrawEllipse(&whitePen, cx - 7.5f, cy - 6.0f, 15.0f, 12.0f);
                break;
            }
            case OverlayTool::TextBox: {
                FontFamily ff(L"Georgia");
                Font f(&ff, 14.0f, FontStyleBold, UnitPixel);
                StringFormat sf;
                sf.SetAlignment(StringAlignmentCenter);
                sf.SetLineAlignment(StringAlignmentCenter);
                g.DrawString(L"T", -1, &f, rf, &sf, &whiteBrush);
                break;
            }
            case OverlayTool::MosaicSquare: {
                Pen thinPen(Color(230, 248, 250, 252), 1.2f);
                g.DrawRectangle(&thinPen, cx - 7.0f, cy - 6.0f, 14.0f, 12.0f);
                for (int r = 0; r < 3; ++r) {
                    for (int c = 0; c < 3; ++c) {
                        if ((r + c) % 2 == 0) {
                            g.FillRectangle(&whiteBrush, cx - 6.0f + c * 4.0f, cy - 5.0f + r * 3.5f, 4.0f, 3.5f);
                        }
                    }
                }
                break;
            }
            case OverlayTool::MosaicCircle: {
                Pen thinPen(Color(230, 248, 250, 252), 1.4f);
                g.DrawEllipse(&thinPen, cx - 7.0f, cy - 7.0f, 14.0f, 14.0f);
                g.FillRectangle(&whiteBrush, cx - 4.0f, cy - 4.0f, 3.8f, 3.8f);
                g.FillRectangle(&whiteBrush, cx + 0.2f, cy + 0.2f, 3.8f, 3.8f);
                SolidBrush dimB(Color(140, 248, 250, 252));
                g.FillRectangle(&dimB, cx + 0.2f, cy - 4.0f, 3.8f, 3.8f);
                g.FillRectangle(&dimB, cx - 4.0f, cy + 0.2f, 3.8f, 3.8f);
                break;
            }
        }
        return;
    }

    // Action Button Icons
    switch (b.id) {
        case DBTN_ACT_UNDO: {
            g.DrawArc(&whitePen, cx - 6.0f, cy - 4.0f, 12.0f, 10.0f, 180.0f, 180.0f);
            PointF head[3] = { PointF(cx - 7.5f, cy - 1.0f), PointF(cx - 3.5f, cy - 5.0f), PointF(cx - 2.5f, cy + 0.5f) };
            g.FillPolygon(&whiteBrush, head, 3);
            break;
        }
        case DBTN_ACT_REDO: {
            g.DrawArc(&whitePen, cx - 6.0f, cy - 4.0f, 12.0f, 10.0f, 180.0f, 180.0f);
            PointF head[3] = { PointF(cx + 7.5f, cy - 1.0f), PointF(cx + 3.5f, cy - 5.0f), PointF(cx + 2.5f, cy + 0.5f) };
            g.FillPolygon(&whiteBrush, head, 3);
            break;
        }
        case DBTN_ACT_CLEAR: {
            // Trash bin icon
            g.DrawRectangle(&whitePen, cx - 5.0f, cy - 3.0f, 10.0f, 10.0f);
            g.DrawLine(&whitePen, cx - 7.0f, cy - 3.5f, cx + 7.0f, cy - 3.5f);
            g.DrawLine(&whitePen, cx - 2.5f, cy - 6.0f, cx + 2.5f, cy - 6.0f);
            break;
        }
        case DBTN_ACT_RESET_NUM: {
            // Circular reset arrow + '1' icon below Numbering Arrow
            Pen amberPen(Color(255, 250, 204, 21), 1.7f);
            SolidBrush amberBrush(Color(255, 250, 204, 21));
            g.DrawArc(&amberPen, cx - 8.5f, cy - 5.5f, 10.5f, 10.5f, 35.0f, 275.0f);
            PointF head[3] = {
                PointF(cx - 8.5f, cy - 4.0f),
                PointF(cx - 4.5f, cy - 6.8f),
                PointF(cx - 4.5f, cy - 1.8f)
            };
            g.FillPolygon(&amberBrush, head, 3);
            FontFamily ff(L"Segoe UI");
            Font f(&ff, 10.5f, FontStyleBold, UnitPixel);
            g.DrawString(L"1", -1, &f, PointF(cx + 1.5f, cy - 6.5f), &amberBrush);
            break;
        }
        case DBTN_ACT_OPTIONS: {
            // Options Gear / Sliders icon
            g.DrawEllipse(&whitePen, cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);
            g.FillEllipse(&whiteBrush, cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
            for (int i = 0; i < 8; ++i) {
                float a = i * 3.14159265f * 0.25f;
                g.DrawLine(&whitePen,
                           cx + 5.0f * std::cos(a), cy + 5.0f * std::sin(a),
                           cx + 7.2f * std::cos(a), cy + 7.2f * std::sin(a));
            }
            break;
        }
        case DBTN_ACT_PIN: {
            // Pushpin icon
            g.FillEllipse(&whiteBrush, cx - 4.0f, cy - 6.5f, 8.0f, 5.0f);
            g.DrawLine(&whitePen, cx, cy - 2.0f, cx, cy + 7.0f);
            g.DrawLine(&whitePen, cx - 5.0f, cy + 1.0f, cx + 5.0f, cy + 1.0f);
            break;
        }
        case DBTN_ACT_SAVE_AS: {
            // Floppy + ellipsis
            g.DrawRectangle(&whitePen, cx - 6.5f, cy - 6.5f, 11.0f, 12.0f);
            g.FillRectangle(&whiteBrush, cx - 3.5f, cy - 6.5f, 5.0f, 4.0f);
            g.FillEllipse(&whiteBrush, cx + 6.0f, cy + 3.5f, 2.5f, 2.5f);
            break;
        }
        case DBTN_ACT_SAVE: {
            // Floppy disk icon
            g.DrawRectangle(&whitePen, cx - 6.0f, cy - 6.0f, 12.0f, 12.0f);
            g.FillRectangle(&whiteBrush, cx - 3.5f, cy - 6.0f, 6.0f, 4.5f);
            g.DrawRectangle(&whitePen, cx - 3.5f, cy + 1.0f, 7.0f, 5.0f);
            break;
        }
        case DBTN_ACT_COPY: {
            // Copy sheets + label
            g.DrawRectangle(&whitePen, cx - 18.0f, cy - 6.0f, 8.0f, 10.0f);
            g.DrawRectangle(&whitePen, cx - 15.0f, cy - 3.0f, 8.0f, 10.0f);
            FontFamily ff(L"Segoe UI");
            Font f(&ff, 11.0f, FontStyleBold, UnitPixel);
            g.DrawString(L"Copy", -1, &f, PointF(cx - 4.0f, cy - 7.0f), &whiteBrush);
            break;
        }
        case DBTN_ACT_CLOSE: {
            g.DrawLine(&whitePen, cx - 4.5f, cy - 4.5f, cx + 4.5f, cy + 4.5f);
            g.DrawLine(&whitePen, cx + 4.5f, cy - 4.5f, cx - 4.5f, cy + 4.5f);
            break;
        }
        case DBTN_ACT_DRAG_HUD: {
            // 4-way move cross icon for dragging the combined 3-row toolbar
            Pen movePen(Color(255, 203, 213, 225), 1.5f);
            SolidBrush moveBr(Color(255, 203, 213, 225));
            g.DrawLine(&movePen, cx - 6.5f, cy, cx + 6.5f, cy);
            g.DrawLine(&movePen, cx, cy - 6.5f, cx, cy + 6.5f);
            PointF upH[3]    = { PointF(cx, cy - 8.5f), PointF(cx - 2.6f, cy - 5.2f), PointF(cx + 2.6f, cy - 5.2f) };
            PointF downH[3]  = { PointF(cx, cy + 8.5f), PointF(cx - 2.6f, cy + 5.2f), PointF(cx + 2.6f, cy + 5.2f) };
            PointF leftH[3]  = { PointF(cx - 8.5f, cy), PointF(cx - 5.2f, cy - 2.6f), PointF(cx - 5.2f, cy + 2.6f) };
            PointF rightH[3] = { PointF(cx + 8.5f, cy), PointF(cx + 5.2f, cy - 2.6f), PointF(cx + 5.2f, cy + 2.6f) };
            g.FillPolygon(&moveBr, upH, 3);
            g.FillPolygon(&moveBr, downH, 3);
            g.FillPolygon(&moveBr, leftH, 3);
            g.FillPolygon(&moveBr, rightH, 3);
            break;
        }
    }
}

void PepperSnapDaemon::DrawHoverBubbleTooltip(Graphics& g, const DockButton& b, int screenW, int screenH) {
    if (b.tooltip.empty()) return;
    FontFamily ff(L"Segoe UI");
    Font tipFont(&ff, 11.5f, FontStyleBold, UnitPixel);
    RectF measured;
    g.MeasureString(b.tooltip.c_str(), -1, &tipFont, PointF(0, 0), &measured);

    float bw = measured.Width + 18.0f;
    float bh = 26.0f;
    float bx = 0.0f, by = 0.0f;
    PointF tail[3];

    // Place bubble above or below horizontal button in the 3-row toolbar
    bool placeAbove = (b.rect.top - bh - 10.0f > 8.0f);
    float midX = (b.rect.left + b.rect.right) * 0.5f;
    bx = std::max(8.0f, std::min((float)screenW - bw - 8.0f, midX - bw * 0.5f));
    by = placeAbove ? (b.rect.top - bh - 8.0f) : (b.rect.bottom + 8.0f);
    if (placeAbove) {
        tail[0] = PointF(midX, (float)b.rect.top - 2.0f);
        tail[1] = PointF(midX - 5.0f, by + bh);
        tail[2] = PointF(midX + 5.0f, by + bh);
    } else {
        tail[0] = PointF(midX, (float)b.rect.bottom + 2.0f);
        tail[1] = PointF(midX - 5.0f, by);
        tail[2] = PointF(midX + 5.0f, by);
    }

    SolidBrush bubbleBg(Color(248, 15, 23, 42));
    Pen bubbleBorder(Color(255, 239, 68, 68), 1.4f);
    g.FillPolygon(&bubbleBg, tail, 3);
    g.FillRectangle(&bubbleBg, bx, by, bw, bh);
    g.DrawRectangle(&bubbleBorder, bx, by, bw, bh);

    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    SolidBrush textBr(Color(255, 248, 250, 252));
    RectF textRc(bx, by, bw, bh);
    g.DrawString(b.tooltip.c_str(), -1, &tipFont, textRc, &sf, &textBr);
}

void PepperSnapDaemon::GetDimensionPillAnchor(bool& outRightAlign, bool& outBottomAlign) const {
    outRightAlign = false;
    outBottomAlign = false;
    if (!hasSelection) return;

    int sx = std::min(selRect.left, selRect.right);
    int sy = std::min(selRect.top, selRect.bottom);
    int sw = std::abs(selRect.right - selRect.left);
    int sh = std::abs(selRect.bottom - selRect.top);

    int anchorX = sx;
    int anchorY = sy;

    switch (dragMode) {
        case DragMode::CreatingSelection:
            anchorX = std::max(0, std::min(vScreenW, (int)dragStartPt.x));
            anchorY = std::max(0, std::min(vScreenH, (int)dragStartPt.y));
            break;
        case DragMode::ResizeBR:
        case DragMode::ResizeR:
        case DragMode::ResizeB:
            anchorX = dragOrigRect.left;
            anchorY = dragOrigRect.top;
            break;
        case DragMode::ResizeBL:
        case DragMode::ResizeL:
            anchorX = dragOrigRect.right;
            anchorY = dragOrigRect.top;
            break;
        case DragMode::ResizeTR:
        case DragMode::ResizeT:
            anchorX = dragOrigRect.left;
            anchorY = dragOrigRect.bottom;
            break;
        case DragMode::ResizeTL:
            anchorX = dragOrigRect.right;
            anchorY = dragOrigRect.bottom;
            break;
        default:
            // When not resizing, keep the size indicator at top-left
            outRightAlign = false;
            outBottomAlign = false;
            return;
    }

    if (sw > 0) {
        outRightAlign = (std::abs(anchorX - (sx + sw)) < std::abs(anchorX - sx));
    } else {
        outRightAlign = (dragMode == DragMode::ResizeBL || dragMode == DragMode::ResizeL || dragMode == DragMode::ResizeTL ||
                         (dragMode == DragMode::CreatingSelection && mousePt.x < dragStartPt.x));
    }

    if (sh > 0) {
        outBottomAlign = (std::abs(anchorY - (sy + sh)) < std::abs(anchorY - sy));
    } else {
        outBottomAlign = (dragMode == DragMode::ResizeTL || dragMode == DragMode::ResizeTR || dragMode == DragMode::ResizeT ||
                          (dragMode == DragMode::CreatingSelection && mousePt.y < dragStartPt.y));
    }
}

void PepperSnapDaemon::BuildDockedHUD() {
    dockButtons.clear();
    if (dockButtons.capacity() < 36) dockButtons.reserve(36);
    customStrokeRect = {0, 0, 0, 0};
    hudBoundsRect = {0, 0, 0, 0};
    bool isResizing = (dragMode >= DragMode::ResizeTL && dragMode <= DragMode::ResizeL);
    if (!hasSelection || dragMode == DragMode::CreatingSelection || isResizing) return;

    int sx = std::min(selRect.left, selRect.right);
    int sy = std::min(selRect.top, selRect.bottom);
    int sw = std::abs(selRect.right - selRect.left);
    int sh = std::abs(selRect.bottom - selRect.top);
    if (sw < 1 || sh < 1) return;

    // ------------------------------------------------------------------------
    // Combined 3-Row Right-Aligned Toolbar:
    //   Row 1: Horizontal Tool Bar (keeping exact tool order)
    //   Row 2: Colors and Sizes Tool Bar (+ Custom px Input Box)
    //   Row 3: Drag Button + Options, Saves, Copy & Close Tool Bar
    // ------------------------------------------------------------------------

    struct ToolEntry { OverlayTool t; const wchar_t* lbl; const wchar_t* tip; };
    ToolEntry tools[] = {
        { OverlayTool::SelectMove,   L"Sel", L"Select Mode (V)" },
        { OverlayTool::Pen,          L"Pen", L"Pen (P)" },
        { OverlayTool::Highlighter,  L"Hi",  L"Stabilo Highlighter (H)" },
        { OverlayTool::Line,         L"Lin", L"Line (L)" },
        { OverlayTool::Arrow,        L"Arr", L"Arrow (A)" },
        { OverlayTool::NumberArrow,  L"1→",  L"Numbering Arrow (N)" },
        { OverlayTool::Rectangle,    L"Box", L"Square / Rectangle (R)" },
        { OverlayTool::Ellipse,      L"Cir", L"Circle / Ellipse (E)" },
        { OverlayTool::TextBox,      L"Txt", L"Text Box (T)" },
        { OverlayTool::MosaicSquare, L"MSq", L"Mosaic Square (X) — Pixelation level is affected by size selection" },
        { OverlayTool::MosaicCircle, L"MCi", L"Mosaic Circle (M) — Pixelation level is affected by size selection" }
    };

    bool hasNumberArrow = false;
    for (const auto& a : annotations) {
        if (a.tool == OverlayTool::NumberArrow) {
            hasNumberArrow = true;
            break;
        }
    }

    const int gap = 3;
    const int rowGap = gap;

    // Row 1 metrics (Tools)
    const int toolBtnW = 32, toolBtnH = 28;
    int totalToolCount = 11 + (hasNumberArrow ? 1 : 0);
    int row1W = totalToolCount * toolBtnW + (totalToolCount - 1) * gap;

    // Row 2 metrics (5 Colors + 4 Stroke Sizes + Custom Size Box equal to 2 toolbar buttons)
    const int colorW = toolBtnW, colorH = toolBtnH;
    const int strokeW = toolBtnW, strokeH = toolBtnH;
    const int customBoxW = toolBtnW * 2 + gap, customBoxH = toolBtnH;
    const int groupGap = gap;
    int colorsTotalW = (int)palette.size() * colorW + ((int)palette.size() - 1) * gap;
    int strokesTotalW = (int)strokeSizes.size() * strokeW + ((int)strokeSizes.size() - 1) * gap;
    int row2W = colorsTotalW + groupGap + strokesTotalW + gap + customBoxW;
    const int row2H = toolBtnH;

    // Row 3 metrics (Drag Handle + Undo/Redo/Clear/Options/Pin/SaveAs/Save/Copy/Close)
    struct ActEntry { int id; int w; const wchar_t* lbl; const wchar_t* tip; bool primary; };
    ActEntry acts[] = {
        { DBTN_ACT_DRAG_HUD, toolBtnW,           L"",     L"Drag to Move Toolbar (Resets when selection moves)", false },
        { DBTN_ACT_UNDO,     toolBtnW,           L"",     L"Undo (Ctrl+Z)", false },
        { DBTN_ACT_REDO,     toolBtnW,           L"",     L"Redo (Ctrl+Y)", false },
        { DBTN_ACT_CLEAR,    toolBtnW,           L"",     L"Clear All Annotations", false },
        { DBTN_ACT_OPTIONS,  toolBtnW,           L"",     L"Options (Folder, Formats & Naming)", false },
        { DBTN_ACT_PIN,      toolBtnW,           L"",     L"Pin to Desktop (F)", false },
        { DBTN_ACT_SAVE_AS,  toolBtnW,           L"",     L"Save As JPG/PNG/WEBP/BMP", false },
        { DBTN_ACT_SAVE,     toolBtnW,           L"",     L"Quick Save (Ctrl+S)", false },
        { DBTN_ACT_COPY,     toolBtnW * 2 + gap, L"Copy", L"Copy to Clipboard (Ctrl+C)", true },
        { DBTN_ACT_CLOSE,    toolBtnW,           L"",     L"Close Overlay (Esc)", false }
    };
    const int actBtnH = 28;
    int row3W = 0;
    for (size_t i = 0; i < 10; ++i) {
        row3W += acts[i].w + (i > 0 ? gap : 0);
    }

    int totalHudW = std::max(row1W, std::max(row2W, row3W));
    int totalHudH = toolBtnH + rowGap + row2H + rowGap + actBtnH; // 28 + 4 + 22 + 4 + 28 = 86px

    int rightX = 0;
    int topY = 0;

    if (hasCustomHudPos) {
        rightX = std::max(totalHudW + 6, std::min(vScreenW - 6, (int)customHudPos.x));
        topY   = std::max(6, std::min(vScreenH - totalHudH - 6, (int)customHudPos.y));
    } else {
        // Align right to selection's right edge (clamped to screen bounds)
        rightX = std::max(totalHudW + 8, std::min(vScreenW - 8, sx + sw));

        bool pillRight = false, pillBottom = false;
        GetDimensionPillAnchor(pillRight, pillBottom);
        int bottomExtra = (pillRight && pillBottom) ? 28 : 0;
        int topExtra    = (pillRight && !pillBottom) ? 28 : 0;

        bool hasSpaceBelow = (sy + sh + 8 + bottomExtra + totalHudH <= vScreenH - 6);
        bool hasSpaceAbove = (sy - 8 - topExtra - totalHudH >= 6);

        if (hasSpaceBelow) {
            // 3. Snap below bottom-right corner
            topY = sy + sh + 8 + bottomExtra;
        } else if (hasSpaceAbove) {
            // 4a. No space below bottom side -> snap to top-right corner
            topY = sy - 8 - topExtra - totalHudH;
        } else {
            // 4b. No space on bottom and top -> put inside bottom-right of selection window
            rightX = std::max(totalHudW + 8, std::min(vScreenW - 8, sx + sw - 8));
            topY   = std::max(6, std::min(vScreenH - totalHudH - 6, sy + sh - totalHudH - 8));
        }
    }

    hudBoundsRect = { rightX - totalHudW, topY, rightX, topY + totalHudH };

    // --- Row 1: Horizontal Tools Strip (Right-aligned to rightX, preserving tool order) ---
    int r1X = rightX - row1W;
    int r1Y = topY;
    for (int i = 0; i < 11; ++i) {
        DockButton b;
        b.id = DBTN_TOOL_BASE + i;
        b.rect = { r1X, r1Y, r1X + toolBtnW, r1Y + toolBtnH };
        b.label = tools[i].lbl;
        if (tools[i].t == OverlayTool::NumberArrow) {
            b.tooltip = L"Numbering Arrow (N) — Next: " + std::to_wstring(nextStepNum);
        } else {
            b.tooltip = tools[i].tip;
        }
        b.isTool = true;
        b.tool = tools[i].t;
        dockButtons.push_back(b);
        r1X += toolBtnW + gap;

        if (tools[i].t == OverlayTool::NumberArrow && hasNumberArrow) {
            DockButton rb;
            rb.id = DBTN_ACT_RESET_NUM;
            rb.rect = { r1X, r1Y, r1X + toolBtnW, r1Y + toolBtnH };
            rb.label = L"↺1";
            rb.tooltip = L"Reset Numbering Arrow Counter to 1 (Next: " + std::to_wstring(nextStepNum) + L")";
            rb.isTool = false;
            dockButtons.push_back(rb);
            r1X += toolBtnW + gap;
        }
    }

    // --- Row 2: Colors and Sizes Toolbar (Right-aligned to rightX) ---
    int r2X = rightX - row2W;
    int r2Y = topY + toolBtnH + rowGap;
    const wchar_t* colorNames[5] = {
        L"Red (#EF4444)",
        L"Green (#22C55E)",
        L"Blue (#3B82F6)",
        L"White (#FFFFFF)",
        L"Black (#000000)"
    };
    for (size_t i = 0; i < palette.size(); ++i) {
        DockButton b;
        b.id = DBTN_COLOR_BASE + (int)i;
        b.rect = { r2X, r2Y, r2X + colorW, r2Y + colorH };
        b.isColor = true;
        b.swatchColor = palette[i];
        b.tooltip = colorNames[i];
        dockButtons.push_back(b);
        r2X += colorW + (i + 1 < palette.size() ? gap : 0);
    }

    r2X += groupGap;
    const wchar_t* strokeLabels[4] = { L"S", L"M", L"L", L"XL" };
    const wchar_t* strokeTips[4] = { L"Small Size (2px)", L"Medium Size (4px)", L"Large Size (8px)", L"Extra Large Size (14px)" };
    for (size_t i = 0; i < strokeSizes.size(); ++i) {
        DockButton b;
        b.id = DBTN_STROKE_BASE + (int)i;
        b.rect = { r2X, r2Y, r2X + strokeW, r2Y + strokeH };
        b.isStroke = true;
        b.strokeVal = strokeSizes[i];
        b.label = strokeLabels[i];
        b.tooltip = strokeTips[i];
        dockButtons.push_back(b);
        r2X += strokeW + (i + 1 < strokeSizes.size() ? gap : 0);
    }

    r2X += gap;
    customStrokeRect = { r2X, r2Y, r2X + customBoxW, r2Y + customBoxH };

    // --- Row 3: Drag Button + Options, Saves, Copy & Close Toolbar (Right-aligned to rightX) ---
    int r3X = rightX - row3W;
    int r3Y = r2Y + row2H + rowGap;
    for (size_t i = 0; i < 10; ++i) {
        DockButton b;
        b.id = acts[i].id;
        b.rect = { r3X, r3Y, r3X + acts[i].w, r3Y + actBtnH };
        b.label = acts[i].lbl;
        b.tooltip = acts[i].tip;
        b.isPrimaryAction = acts[i].primary;
        dockButtons.push_back(b);
        r3X += acts[i].w + gap;
    }
}

DragMode PepperSnapDaemon::HitTestSelectionHandles(int mx, int my) const {
    if (!hasSelection) return DragMode::None;
    int x1 = std::min(selRect.left, selRect.right);
    int y1 = std::min(selRect.top, selRect.bottom);
    int x2 = std::max(selRect.left, selRect.right);
    int y2 = std::max(selRect.top, selRect.bottom);
    int xm = (x1 + x2) / 2;
    int ym = (y1 + y2) / 2;
    const int r = 7;

    auto nearPt = [&](int px, int py) {
        return std::abs(mx - px) <= r && std::abs(my - py) <= r;
    };

    if (nearPt(x1, y1)) return DragMode::ResizeTL;
    if (nearPt(xm, y1)) return DragMode::ResizeT;
    if (nearPt(x2, y1)) return DragMode::ResizeTR;
    if (nearPt(x2, ym)) return DragMode::ResizeR;
    if (nearPt(x2, y2)) return DragMode::ResizeBR;
    if (nearPt(xm, y2)) return DragMode::ResizeB;
    if (nearPt(x1, y2)) return DragMode::ResizeBL;
    if (nearPt(x1, ym)) return DragMode::ResizeL;
    return DragMode::None;
}

// ----------------------------------------------------------------------------
// Pinned Desktop Window & Modals (Naming Pattern, Folder, Shortcuts)
// ----------------------------------------------------------------------------

static LRESULT CALLBACK PinWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PinnedWindowData* data = (PinnedWindowData*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    switch (msg) {
        case WM_LBUTTONDOWN:
            if (data) {
                data->dragging = true;
                GetCursorPos(&data->dragStartMouse);
                RECT rc;
                GetWindowRect(hWnd, &rc);
                data->dragStartWnd = { rc.left, rc.top };
                SetCapture(hWnd);
            }
            return 0;
        case WM_MOUSEMOVE:
            if (data && data->dragging) {
                POINT cur;
                GetCursorPos(&cur);
                SetWindowPos(
                    hWnd, HWND_TOPMOST,
                    data->dragStartWnd.x + (cur.x - data->dragStartMouse.x),
                    data->dragStartWnd.y + (cur.y - data->dragStartMouse.y),
                    0, 0, SWP_NOSIZE | SWP_NOACTIVATE
                );
            }
            return 0;
        case WM_LBUTTONUP:
            if (data && data->dragging) {
                data->dragging = false;
                ReleaseCapture();
            }
            return 0;
        case WM_MOUSEWHEEL:
            if (data) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                data->scale = std::max(0.25f, std::min(3.0f, data->scale * (delta > 0 ? 1.1f : 0.9f)));
                SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, (int)(data->origW * data->scale), (int)(data->origH * data->scale), SWP_NOMOVE | SWP_NOACTIVATE);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        case WM_RBUTTONUP:
        case WM_LBUTTONDBLCLK:
            DestroyWindow(hWnd);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);
            int w = rc.right - rc.left, h = rc.bottom - rc.top;
            Graphics g(hdc);
            g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            if (data && data->bmp) g.DrawImage(data->bmp, 0, 0, w, h);
            Pen border(Color(255, 239, 68, 68), 2.0f);
            g.DrawRectangle(&border, 1, 1, w - 2, h - 2);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            if (data) {
                delete data->bmp;
                delete data;
            }
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void PepperSnapDaemon::ActionPinToDesktop() {
    Bitmap* cropped = RenderCroppedRegionBitmap();
    if (!cropped) return;
    int sx = vScreenX + std::min(selRect.left, selRect.right);
    int sy = vScreenY + std::min(selRect.top, selRect.bottom);
    int sw = (int)cropped->GetWidth();
    int sh = (int)cropped->GetHeight();

    PinnedWindowData* data = new PinnedWindowData();
    data->bmp = cropped;
    data->origW = sw;
    data->origH = sh;
    data->scale = 1.0f;

    CloseRegionSnipOverlay();

    HWND hPin = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"PepperSnapPinWnd", L"PepperSnap Pin",
        WS_POPUP | WS_VISIBLE, sx, sy, sw, sh,
        nullptr, nullptr, hInst, nullptr
    );
    SetWindowLongPtrW(hPin, GWLP_USERDATA, (LONG_PTR)data);
    InvalidateRect(hPin, nullptr, FALSE);
    pinnedWindows.push_back(hPin);
    ShowTrayToast(L"Pinned to Desktop", L"Drag to move · Scroll wheel to scale · Right-click to close.");
}

// Unified Options Modal (Save Folder, Default Formats, JPEG Quality Slider, Naming Pattern + Restore Default, Non-Stacking Highlighter)
struct OptionsDlgState {
    std::wstring folder;
    ImageFormat regionFmt = ImageFormat::JPEG;
    ImageFormat fullFmt = ImageFormat::JPEG;
    ImageFormat copyFmt = ImageFormat::JPEG;
    int jpgQuality = 95;
    std::wstring naming;
    bool autoSaveCopy = true;
    bool doNotCopySave = false;
    bool nonStackingHi = true;
    bool penSmoothEnabled = true;
    int penSmoothStrength = 50;
    bool confirmed = false;
    bool openedAppData = false;

    HWND hFolderEdit = nullptr;
    HWND hComboRegion = nullptr;
    HWND hComboFull = nullptr;
    HWND hComboCopy = nullptr;
    HWND hQualitySlider = nullptr;
    HWND hQualityValLbl = nullptr;
    HWND hNamingEdit = nullptr;
    HWND hPreviewLbl = nullptr;
    HWND hAutoSaveChk = nullptr;
    HWND hNoCopySaveChk = nullptr;
    HWND hNonStackingChk = nullptr;
    HWND hPenSmoothChk = nullptr;
    HWND hPenSmoothSlider = nullptr;
    HWND hPenSmoothValLbl = nullptr;
    HWND hAppDataInfo = nullptr;
};

#define IDC_OPT_FOLDER_EDIT      1001
#define IDC_OPT_FOLDER_BROWSE    1002
#define IDC_OPT_FOLDER_DEFAULT   1003
#define IDC_OPT_COMBO_REGION     1004
#define IDC_OPT_COMBO_FULL       1005
#define IDC_OPT_NAMING_EDIT      1006
#define IDC_OPT_NAMING_RESTORE   1007
#define IDC_OPT_AUTOSAVE_CHK     1008
#define IDC_OPT_QUALITY_SLIDER   1009
#define IDC_OPT_NONSTACK_CHK     1010
#define IDC_OPT_COMBO_COPY       1011
#define IDC_OPT_OPEN_APPDATA     1012
#define IDC_OPT_PENSMOOTH_CHK    1013
#define IDC_OPT_PENSMOOTH_SLIDER 1014
#define IDC_OPT_NOCOPY_SAVE_CHK  1015

static std::wstring FormatPenSmoothLabel(bool enabled, int strength) {
    if (!enabled) return L"Smoothing Strength: Off";
    const wchar_t* desc = L"Balanced";
    if (strength <= 25) desc = L"Weak";
    else if (strength <= 60) desc = L"Balanced";
    else if (strength <= 85) desc = L"Strong";
    else desc = L"Ultra Smooth";
    return L"Smoothing Strength: " + std::to_wstring(strength) + L"% (" + desc + L")";
}

static void UpdateOptionsPreviewLabel(OptionsDlgState* st) {
    if (!st || !st->hNamingEdit || !st->hPreviewLbl) return;
    WCHAR buf[512] = {0};
    GetWindowTextW(st->hNamingEdit, buf, 511);
    int selFmt = st->hComboRegion ? (int)SendMessageW(st->hComboRegion, CB_GETCURSEL, 0, 0) : (int)st->regionFmt;
    if (selFmt < 0 || selFmt > 3) selFmt = 0;
    std::wstring preview = L"Live Preview:  " +
        PepperSnapDaemon::FormatFilenameWithPattern(buf, g_Daemon.captureCounter) +
        PepperSnapDaemon::GetFormatExtension((ImageFormat)selFmt) +
        L"   (Zero EXIF/Metadata)";
    SetWindowTextW(st->hPreviewLbl, preview.c_str());
}

static LRESULT CALLBACK OptionsDlgWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    OptionsDlgState* st = (OptionsDlgState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            st = (OptionsDlgState*)cs->lpCreateParams;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);

            HFONT hFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            HFONT hBoldFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

            // 1. Save Folder Location
            HWND hLblFolder = CreateWindowExW(0, L"STATIC", L"1. Default Save Folder Location (Default: Desktop):",
                WS_CHILD | WS_VISIBLE, 18, 14, 516, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblFolder, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            st->hFolderEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", st->folder.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 18, 40, 320, 26, hWnd, (HMENU)IDC_OPT_FOLDER_EDIT, nullptr, nullptr);
            SendMessageW(st->hFolderEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hBrowse = CreateWindowExW(0, L"BUTTON", L"Browse...",
                WS_CHILD | WS_VISIBLE, 344, 39, 84, 28, hWnd, (HMENU)IDC_OPT_FOLDER_BROWSE, nullptr, nullptr);
            SendMessageW(hBrowse, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hDeskDef = CreateWindowExW(0, L"BUTTON", L"Use Desktop",
                WS_CHILD | WS_VISIBLE, 434, 39, 100, 28, hWnd, (HMENU)IDC_OPT_FOLDER_DEFAULT, nullptr, nullptr);
            SendMessageW(hDeskDef, WM_SETFONT, (WPARAM)hFont, TRUE);

            // 2. Default Formats & JPEG Quality Slider
            HWND hLblFormats = CreateWindowExW(0, L"STATIC", L"2. Default Image Formats & JPEG Quality (Zero Metadata Saved):",
                WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 18, 80, 516, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblFormats, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            HWND hLblReg = CreateWindowExW(0, L"STATIC", L"Selected Area Format:",
                WS_CHILD | WS_VISIBLE, 18, 108, 155, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblReg, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hComboRegion = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 176, 104, 175, 140, hWnd, (HMENU)IDC_OPT_COMBO_REGION, nullptr, nullptr);
            SendMessageW(st->hComboRegion, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hLblFull = CreateWindowExW(0, L"STATIC", L"Instant Fullscreen Format:",
                WS_CHILD | WS_VISIBLE, 18, 140, 155, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblFull, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hComboFull = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 176, 136, 175, 140, hWnd, (HMENU)IDC_OPT_COMBO_FULL, nullptr, nullptr);
            SendMessageW(st->hComboFull, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hLblCopy = CreateWindowExW(0, L"STATIC", L"Copy Format:",
                WS_CHILD | WS_VISIBLE, 18, 172, 155, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblCopy, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hComboCopy = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 176, 168, 175, 140, hWnd, (HMENU)IDC_OPT_COMBO_COPY, nullptr, nullptr);
            SendMessageW(st->hComboCopy, WM_SETFONT, (WPARAM)hFont, TRUE);

            const wchar_t* fmtItems[4] = { L"JPEG (.jpg) — Default", L"PNG (.png)", L"WebP (.webp)", L"BMP (.bmp)" };
            for (int i = 0; i < 4; ++i) {
                SendMessageW(st->hComboRegion, CB_ADDSTRING, 0, (LPARAM)fmtItems[i]);
                SendMessageW(st->hComboFull,   CB_ADDSTRING, 0, (LPARAM)fmtItems[i]);
                SendMessageW(st->hComboCopy,   CB_ADDSTRING, 0, (LPARAM)fmtItems[i]);
            }
            SendMessageW(st->hComboRegion, CB_SETCURSEL, (WPARAM)st->regionFmt, 0);
            SendMessageW(st->hComboFull,   CB_SETCURSEL, (WPARAM)st->fullFmt, 0);
            SendMessageW(st->hComboCopy,   CB_SETCURSEL, (WPARAM)st->copyFmt, 0);

            // JPEG Quality Slider (10% - 100%)
            std::wstring qText = L"JPEG Quality: " + std::to_wstring(st->jpgQuality) + L"%";
            st->hQualityValLbl = CreateWindowExW(0, L"STATIC", qText.c_str(),
                WS_CHILD | WS_VISIBLE, 18, 206, 150, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(st->hQualityValLbl, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hQualitySlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS, 172, 202, 362, 32, hWnd, (HMENU)IDC_OPT_QUALITY_SLIDER, nullptr, nullptr);
            SendMessageW(st->hQualitySlider, TBM_SETRANGE, TRUE, MAKELONG(10, 100));
            SendMessageW(st->hQualitySlider, TBM_SETTICFREQ, 10, 0);
            SendMessageW(st->hQualitySlider, TBM_SETPOS, TRUE, st->jpgQuality);

            // 3. Timestamp Naming Pattern + Restore Default Button
            HWND hLblNaming = CreateWindowExW(0, L"STATIC",
                L"3. Filename Naming Pattern:",
                WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 18, 244, 516, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblNaming, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            st->hNamingEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", st->naming.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 18, 270, 386, 26, hWnd, (HMENU)IDC_OPT_NAMING_EDIT, nullptr, nullptr);
            SendMessageW(st->hNamingEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hRestoreNaming = CreateWindowExW(0, L"BUTTON", L"Restore Default",
                WS_CHILD | WS_VISIBLE, 412, 269, 122, 28, hWnd, (HMENU)IDC_OPT_NAMING_RESTORE, nullptr, nullptr);
            SendMessageW(hRestoreNaming, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hPreviewLbl = CreateWindowExW(0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 18, 302, 516, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(st->hPreviewLbl, WM_SETFONT, (WPARAM)hFont, TRUE);

            // 4. Behavior, Highlighter & Pen Freehand Smoothing Options
            st->hNonStackingChk = CreateWindowExW(0, L"BUTTON",
                L"Non-Stacking Highlighter (prevent overlapping highlighter strokes from darkening)",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 332, 516, 24, hWnd, (HMENU)IDC_OPT_NONSTACK_CHK, nullptr, nullptr);
            SendMessageW(st->hNonStackingChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hNonStackingChk, BM_SETCHECK, st->nonStackingHi ? BST_CHECKED : BST_UNCHECKED, 0);

            st->hAutoSaveChk = CreateWindowExW(0, L"BUTTON",
                L"Automatically save file to Save Folder when copying selected area (Ctrl+C)",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 360, 516, 24, hWnd, (HMENU)IDC_OPT_AUTOSAVE_CHK, nullptr, nullptr);
            SendMessageW(st->hAutoSaveChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hAutoSaveChk, BM_SETCHECK, st->autoSaveCopy ? BST_CHECKED : BST_UNCHECKED, 0);

            st->hNoCopySaveChk = CreateWindowExW(0, L"BUTTON",
                L"Do not automatically copy to clipboard saved Instant Fullscreen or Selected Area",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 388, 516, 24, hWnd, (HMENU)IDC_OPT_NOCOPY_SAVE_CHK, nullptr, nullptr);
            SendMessageW(st->hNoCopySaveChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hNoCopySaveChk, BM_SETCHECK, st->doNotCopySave ? BST_CHECKED : BST_UNCHECKED, 0);

            st->hPenSmoothChk = CreateWindowExW(0, L"BUTTON",
                L"Enable Pen & Highlighter Freehand Stroke Smoothing",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 416, 516, 24, hWnd, (HMENU)IDC_OPT_PENSMOOTH_CHK, nullptr, nullptr);
            SendMessageW(st->hPenSmoothChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hPenSmoothChk, BM_SETCHECK, st->penSmoothEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

            std::wstring psText = FormatPenSmoothLabel(st->penSmoothEnabled, st->penSmoothStrength);
            st->hPenSmoothValLbl = CreateWindowExW(0, L"STATIC", psText.c_str(),
                WS_CHILD | WS_VISIBLE, 18, 450, 224, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(st->hPenSmoothValLbl, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hPenSmoothSlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS, 244, 446, 290, 32, hWnd, (HMENU)IDC_OPT_PENSMOOTH_SLIDER, nullptr, nullptr);
            SendMessageW(st->hPenSmoothSlider, TBM_SETRANGE, TRUE, MAKELONG(5, 100));
            SendMessageW(st->hPenSmoothSlider, TBM_SETTICFREQ, 10, 0);
            SendMessageW(st->hPenSmoothSlider, TBM_SETPOS, TRUE, st->penSmoothStrength);
            EnableWindow(st->hPenSmoothSlider, st->penSmoothEnabled ? TRUE : FALSE);

            // Save & Cancel Buttons + clickable %appdata%\peppersnap persistence link on bottom
            HFONT hLinkFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, TRUE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            st->hAppDataInfo = CreateWindowExW(0, L"STATIC",
                L"\x24D8 Settings is saved to %appdata%\\peppersnap",
                WS_CHILD | WS_VISIBLE | SS_NOPREFIX | SS_NOTIFY, 18, 497, 294, 22, hWnd, (HMENU)IDC_OPT_OPEN_APPDATA, nullptr, nullptr);
            SendMessageW(st->hAppDataInfo, WM_SETFONT, (WPARAM)hLinkFont, TRUE);

            HWND hOk = CreateWindowExW(0, L"BUTTON", L"Save Options",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 316, 490, 110, 34, hWnd, (HMENU)IDOK, nullptr, nullptr);
            SendMessageW(hOk, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            HWND hCancel = CreateWindowExW(0, L"BUTTON", L"Cancel",
                WS_CHILD | WS_VISIBLE, 434, 490, 100, 34, hWnd, (HMENU)IDCANCEL, nullptr, nullptr);
            SendMessageW(hCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

            UpdateOptionsPreviewLabel(st);
            return 0;
        }
        case WM_SETCURSOR: {
            if (st && st->hAppDataInfo && (HWND)wParam == st->hAppDataInfo) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
            break;
        }
        case WM_CTLCOLORSTATIC: {
            if (st && st->hAppDataInfo && (HWND)lParam == st->hAppDataInfo) {
                HDC hdcStatic = (HDC)wParam;
                SetTextColor(hdcStatic, RGB(0, 102, 204));
                SetBkMode(hdcStatic, TRANSPARENT);
                return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
            }
            break;
        }
        case WM_HSCROLL: {
            if (st && (HWND)lParam == st->hQualitySlider) {
                int pos = (int)SendMessageW(st->hQualitySlider, TBM_GETPOS, 0, 0);
                st->jpgQuality = std::max(10, std::min(100, pos));
                std::wstring qText = L"JPEG Quality: " + std::to_wstring(st->jpgQuality) + L"%";
                SetWindowTextW(st->hQualityValLbl, qText.c_str());
                return 0;
            }
            if (st && (HWND)lParam == st->hPenSmoothSlider) {
                int pos = (int)SendMessageW(st->hPenSmoothSlider, TBM_GETPOS, 0, 0);
                st->penSmoothStrength = std::max(5, std::min(100, pos));
                std::wstring psText = FormatPenSmoothLabel(st->penSmoothEnabled, st->penSmoothStrength);
                SetWindowTextW(st->hPenSmoothValLbl, psText.c_str());
                return 0;
            }
            break;
        }
        case WM_COMMAND: {
            if (!st) break;
            WORD id = LOWORD(wParam);
            WORD code = HIWORD(wParam);

            if (id == IDC_OPT_PENSMOOTH_CHK) {
                st->penSmoothEnabled = (SendMessageW(st->hPenSmoothChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                EnableWindow(st->hPenSmoothSlider, st->penSmoothEnabled ? TRUE : FALSE);
                std::wstring psText = FormatPenSmoothLabel(st->penSmoothEnabled, st->penSmoothStrength);
                SetWindowTextW(st->hPenSmoothValLbl, psText.c_str());
                return 0;
            }

            if ((id == IDC_OPT_NAMING_EDIT && code == EN_CHANGE) ||
                (id == IDC_OPT_COMBO_REGION && code == CBN_SELCHANGE)) {
                UpdateOptionsPreviewLabel(st);
                return 0;
            }
            if (id == IDC_OPT_NAMING_RESTORE) {
                SetWindowTextW(st->hNamingEdit, PepperSnapDaemon::DEFAULT_NAMING_PATTERN);
                UpdateOptionsPreviewLabel(st);
                return 0;
            }
            if (id == IDC_OPT_OPEN_APPDATA) {
                st->openedAppData = true;
                DestroyWindow(hWnd);
                return 0;
            }
            if (id == IDC_OPT_FOLDER_DEFAULT) {
                std::wstring desk = PepperSnapDaemon::GetDefaultDesktopFolder();
                SetWindowTextW(st->hFolderEdit, desk.c_str());
                return 0;
            }
            if (id == IDC_OPT_FOLDER_BROWSE) {
                BROWSEINFOW bi = {0};
                bi.hwndOwner = hWnd;
                bi.lpszTitle = L"Select Default Screenshot Save Folder:";
                bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
                LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
                if (pidl) {
                    WCHAR path[MAX_PATH] = {0};
                    if (SHGetPathFromIDListW(pidl, path)) {
                        SetWindowTextW(st->hFolderEdit, path);
                    }
                    CoTaskMemFree(pidl);
                }
                return 0;
            }
            if (id == IDOK) {
                WCHAR fBuf[MAX_PATH] = {0};
                GetWindowTextW(st->hFolderEdit, fBuf, MAX_PATH - 1);
                if (wcslen(fBuf) > 0) st->folder = fBuf;

                WCHAR nBuf[512] = {0};
                GetWindowTextW(st->hNamingEdit, nBuf, 511);
                st->naming = (wcslen(nBuf) > 0) ? nBuf : PepperSnapDaemon::DEFAULT_NAMING_PATTERN;

                int rSel = (int)SendMessageW(st->hComboRegion, CB_GETCURSEL, 0, 0);
                int fSel = (int)SendMessageW(st->hComboFull,   CB_GETCURSEL, 0, 0);
                int cSel = (int)SendMessageW(st->hComboCopy,   CB_GETCURSEL, 0, 0);
                if (rSel >= 0 && rSel <= 3) st->regionFmt = (ImageFormat)rSel;
                if (fSel >= 0 && fSel <= 3) st->fullFmt   = (ImageFormat)fSel;
                if (cSel >= 0 && cSel <= 3) st->copyFmt   = (ImageFormat)cSel;

                int qPos = (int)SendMessageW(st->hQualitySlider, TBM_GETPOS, 0, 0);
                st->jpgQuality = std::max(10, std::min(100, qPos));

                int psPos = (int)SendMessageW(st->hPenSmoothSlider, TBM_GETPOS, 0, 0);
                st->penSmoothStrength = std::max(5, std::min(100, psPos));
                st->penSmoothEnabled  = (SendMessageW(st->hPenSmoothChk, BM_GETCHECK, 0, 0) == BST_CHECKED);

                st->nonStackingHi = (SendMessageW(st->hNonStackingChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->autoSaveCopy  = (SendMessageW(st->hAutoSaveChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->doNotCopySave = (SendMessageW(st->hNoCopySaveChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->confirmed = true;
                DestroyWindow(hWnd);
                return 0;
            }
            if (id == IDCANCEL) {
                DestroyWindow(hWnd);
                return 0;
            }
            break;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void PepperSnapDaemon::ShowOptionsModal() {
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = OptionsDlgWndProc;
        wc.hInstance = hInst;
        wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APPICON));
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"PepperSnapOptionsModal";
        RegisterClassExW(&wc);
        reg = true;
    }
    OptionsDlgState st;
    st.folder = saveFolder;
    st.regionFmt = regionFormat;
    st.fullFmt = fullscreenFormat;
    st.copyFmt = copyFormat;
    st.jpgQuality = jpegQuality;
    st.naming = namingPattern;
    st.autoSaveCopy = autoSaveOnCopy;
    st.doNotCopySave = doNotCopyOnSave;
    st.nonStackingHi = nonStackingHighlighter;
    st.penSmoothEnabled = penSmoothingEnabled;
    st.penSmoothStrength = penSmoothingStrength;

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    HWND hParent = hOverlayWnd ? hOverlayWnd : hTrayWnd;
    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"PepperSnapOptionsModal",
        L"PepperSnap v3.0.0.2 Options — Folder, Formats, Quality, Naming & Smoothing",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        (sw - 564) / 2, (sh - 578) / 2, 564, 578, hParent, nullptr, hInst, &st
    );
    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) { SendMessageW(hDlg, WM_COMMAND, IDOK, 0); continue; }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) { SendMessageW(hDlg, WM_COMMAND, IDCANCEL, 0); continue; }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (st.openedAppData) {
        CloseRegionSnipOverlay();
        std::wstring appDataDir = PepperSnapDaemon::GetAppDataSettingsDir();
        CreateDirectoryW(appDataDir.c_str(), nullptr);
        ShellExecuteW(nullptr, L"open", appDataDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
    if (st.confirmed) {
        saveFolder = st.folder;
        regionFormat = st.regionFmt;
        fullscreenFormat = st.fullFmt;
        copyFormat = st.copyFmt;
        jpegQuality = st.jpgQuality;
        namingPattern = st.naming;
        autoSaveOnCopy = st.autoSaveCopy;
        doNotCopyOnSave = st.doNotCopySave;
        nonStackingHighlighter = st.nonStackingHi;
        penSmoothingEnabled = st.penSmoothEnabled;
        penSmoothingStrength = st.penSmoothStrength;
        CreateDirectoryW(saveFolder.c_str(), nullptr);
        SaveSettings();
        if (hOverlayWnd) {
            InvalidateRect(hOverlayWnd, nullptr, FALSE);
        }
        ShowTrayToast(
            L"PepperSnap Options Saved",
            L"Folder: " + saveFolder +
            L"\nRegion: " + GetFormatLabel(regionFormat) +
            L" · Copy: " + GetFormatLabel(copyFormat)
        );
    }
}

void PepperSnapDaemon::ShowShortcutsModal() {
    MessageBoxW(
        hOverlayWnd ? hOverlayWnd : hTrayWnd,
        L"PepperSnap v3.0.0.2 Native C++17 — Complete Hotkeys Reference\n"
        L"────────────────────────────────────────────────────────\n\n"
        L"GLOBAL CAPTURE HOTKEYS:\n"
        L"  • Ctrl + PrintScreen     Interactive Region Snip & Annotate\n"
        L"  • Shift + PrintScreen    Instant Fullscreen Capture (Default: Desktop / JPEG)\n"
        L"  • Ctrl + Shift + S / F   Laptop Fallback Capture Hotkeys\n\n"
        L"IN-PLACE ANNOTATION TOOLS & CURSORS:\n"
        L"  • V   Select Mode (Arrow Cursor) — Move / Resize / Recolor\n"
        L"  • P   Freehand Pen Tool (Pen Cursor)\n"
        L"  • H   Stabilo Highlighter Tool (Stabilo Cursor)\n"
        L"  • L   Line Tool (+ Crosshair Cursor)\n"
        L"  • A   Arrow Tool (+ Crosshair Cursor, No Shadow)\n"
        L"  • N   Numbering Arrow Tool 1→, 2→ (+ Crosshair Cursor)\n"
        L"  • R   Square / Rectangle Tool (+ Crosshair Cursor)\n"
        L"  • E   Circle / Ellipse Tool (+ Crosshair Cursor)\n"
        L"  • T   In-Place Text Box Tool (Arrow Cursor)\n"
        L"  • X   Mosaic Square Tool (+ Crosshair + Live Dashed Guide)\n"
        L"  • M   Mosaic Circle Tool (+ Crosshair + Live Dashed Guide)\n\n"
        L"SIZE INPUT & ESCAPE BEHAVIOR:\n"
        L"  • Click Size Pill (W × H px) to type custom size (e.g. 1280x720 + Enter)\n"
        L"  • Esc (when using any drawing tool) -> Switches to Select Mode\n"
        L"  • Esc (when in Select Mode)         -> Exits Capture Overlay",
        L"PepperSnap Shortcuts & Guide",
        MB_OK | MB_ICONINFORMATION
    );
}

// ----------------------------------------------------------------------------
// Overlay Actions (Copy, Save, Save As, Undo/Redo)
// ----------------------------------------------------------------------------

void PepperSnapDaemon::PushUndo() {
    undoStack.push_back(annotations);
    if (undoStack.size() > 40) undoStack.erase(undoStack.begin());
    redoStack.clear();
}

void PepperSnapDaemon::Undo() {
    CommitActiveTextBox();
    if (undoStack.empty()) return;
    redoStack.push_back(annotations);
    annotations = undoStack.back();
    undoStack.pop_back();
    RecalcNextStepNum();
    selectedAnnotationId = -1;
    if (hOverlayWnd) InvalidateRect(hOverlayWnd, nullptr, FALSE);
}

void PepperSnapDaemon::Redo() {
    CommitActiveTextBox();
    if (redoStack.empty()) return;
    undoStack.push_back(annotations);
    annotations = redoStack.back();
    redoStack.pop_back();
    RecalcNextStepNum();
    selectedAnnotationId = -1;
    if (hOverlayWnd) InvalidateRect(hOverlayWnd, nullptr, FALSE);
}

void PepperSnapDaemon::ActionCopyAndClose() {
    Bitmap* bmp = RenderCroppedRegionBitmap();
    if (!bmp) return;
    int w = (int)bmp->GetWidth();
    int h = (int)bmp->GetHeight();
    CopyBitmapToClipboard(bmp);

    std::wstring savedNote;
    if (autoSaveOnCopy) {
        CreateDirectoryW(saveFolder.c_str(), nullptr);
        std::wstring fn = FormatFilename(captureCounter++) + GetFormatExtension(copyFormat);
        std::wstring full = saveFolder + L"\\" + fn;
        if (SaveBitmapToPath(bmp, full)) {
            lastSavedFilePath = full;
            savedNote = L"\nAuto-saved to: " + full;
        }
    }
    delete bmp;
    CloseRegionSnipOverlay();
    ShowTrayToast(
        L"Copied to Clipboard (" + std::to_wstring(w) + L"×" + std::to_wstring(h) + L" px)",
        L"Ready to paste (Ctrl+V)." + savedNote
    );
}

void PepperSnapDaemon::ActionCopyPixelColorAndClose() {
    if (!frozenDesktopBmp) return;
    int mx = std::max(0, std::min(vScreenW - 1, (int)mousePt.x));
    int my = std::max(0, std::min(vScreenH - 1, (int)mousePt.y));
    BYTE r = 0, g = 0, b = 0;
    if (brightDibPixels) {
        DWORD raw = brightDibPixels[(size_t)my * vScreenW + mx];
        b = (BYTE)(raw & 0xFF);
        g = (BYTE)((raw >> 8) & 0xFF);
        r = (BYTE)((raw >> 16) & 0xFF);
    } else {
        Color pxCol;
        frozenDesktopBmp->GetPixel(mx, my, &pxCol);
        r = pxCol.GetR();
        g = pxCol.GetG();
        b = pxCol.GetB();
    }

    wchar_t hexBuf[32];
    swprintf_s(hexBuf, L"#%02X%02X%02X", r, g, b);
    std::wstring rgbStr(hexBuf);

    HWND hClipOwner = hOverlayWnd ? hOverlayWnd : hTrayWnd;
    if (OpenClipboard(hClipOwner)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (rgbStr.size() + 1) * sizeof(wchar_t));
        if (hMem) {
            wchar_t* dst = (wchar_t*)GlobalLock(hMem);
            if (dst) {
                memcpy(dst, rgbStr.c_str(), (rgbStr.size() + 1) * sizeof(wchar_t));
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
        }
        CloseClipboard();
    }

    CloseRegionSnipOverlay();

    wchar_t msgBuf[128];
    swprintf_s(msgBuf, L"RGB: %s (%d, %d, %d) copied to clipboard.",
               hexBuf, r, g, b);
    ShowTrayToast(L"Copied RGB Code: " + rgbStr, msgBuf);
}

void PepperSnapDaemon::ActionQuickSaveAndClose() {
    Bitmap* bmp = RenderCroppedRegionBitmap();
    if (!bmp) return;
    if (!doNotCopyOnSave) {
        CopyBitmapToClipboard(bmp);
    }
    CreateDirectoryW(saveFolder.c_str(), nullptr);
    std::wstring fn = FormatFilename(captureCounter++) + GetFormatExtension(regionFormat);
    std::wstring full = saveFolder + L"\\" + fn;
    if (SaveBitmapToPath(bmp, full)) {
        lastSavedFilePath = full;
        ShowTrayToast(
            doNotCopyOnSave ? L"Capture Saved" : L"Capture Saved & Copied",
            L"Saved to: " + full + L"\n(Click to reveal in Explorer)"
        );
    }
    delete bmp;
    CloseRegionSnipOverlay();
}

void PepperSnapDaemon::ActionSaveAsAndClose() {
    Bitmap* bmp = RenderCroppedRegionBitmap();
    if (!bmp) return;
    ShowWindow(hOverlayWnd, SW_HIDE);

    WCHAR szFile[MAX_PATH] = {0};
    std::wstring defName = FormatFilename(captureCounter++) + GetFormatExtension(regionFormat);
    wcsncpy_s(szFile, defName.c_str(), _TRUNCATE);

    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hTrayWnd;
    ofn.lpstrInitialDir = saveFolder.c_str();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"JPEG Image (*.jpg)\0*.jpg\0PNG Image (*.png)\0*.png\0WebP Image (*.webp)\0*.webp\0BMP Bitmap (*.bmp)\0*.bmp\0";
    ofn.nFilterIndex = (DWORD)regionFormat + 1;
    const wchar_t* defExts[4] = { L"jpg", L"png", L"webp", L"bmp" };
    ofn.lpstrDefExt = defExts[(int)regionFormat & 3];
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (GetSaveFileNameW(&ofn)) {
        if (SaveBitmapToPath(bmp, szFile)) {
            lastSavedFilePath = szFile;
            if (!doNotCopyOnSave) {
                CopyBitmapToClipboard(bmp);
            }
            ShowTrayToast(L"Saved Capture", std::wstring(szFile) + L"\n(Click to open in Explorer)");
        }
    }
    delete bmp;
    CloseRegionSnipOverlay();
}

// ----------------------------------------------------------------------------
// Overlay Window Rendering & Procedure (With Live Pixelation & Live Text Box)
// ----------------------------------------------------------------------------

static void RenderOverlayWindow(HWND, HDC hdc) {
    int W = g_Daemon.vScreenW;
    int H = g_Daemon.vScreenH;
    if (W <= 0 || H <= 0) return;

    g_Daemon.BuildDockedHUD();

    bool tempBackBuffer = false;
    HDC memDC = g_Daemon.hBackBufferDC;
    HBITMAP memBmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    if (!memDC) {
        tempBackBuffer = true;
        memDC = CreateCompatibleDC(hdc);
        memBmp = CreateCompatibleBitmap(hdc, W, H);
        oldBmp = SelectObject(memDC, memBmp);
    }

    // 1. Perform all hardware GDI BitBlts + in-place DIB Mosaic pixelation BEFORE constructing GDI+ Graphics(memDC)
    if (g_Daemon.hDimmedDesktopDC) {
        BitBlt(memDC, 0, 0, W, H, g_Daemon.hDimmedDesktopDC, 0, 0, SRCCOPY);
    }
    if (g_Daemon.hasSelection && g_Daemon.hBrightDesktopDC) {
        int sx0 = std::max(0, std::min(W, (int)std::min(g_Daemon.selRect.left, g_Daemon.selRect.right)));
        int sy0 = std::max(0, std::min(H, (int)std::min(g_Daemon.selRect.top, g_Daemon.selRect.bottom)));
        int sw0 = std::min(W - sx0, (int)std::abs(g_Daemon.selRect.right - g_Daemon.selRect.left));
        int sh0 = std::min(H - sy0, (int)std::abs(g_Daemon.selRect.bottom - g_Daemon.selRect.top));
        if (sw0 >= 1 && sh0 >= 1) {
            BitBlt(memDC, sx0, sy0, sw0, sh0, g_Daemon.hBrightDesktopDC, sx0, sy0, SRCCOPY);
            if (g_Daemon.backBufferPixels) {
                GdiFlush();
                auto applyMosaicClipped = [&](const Annotation& ma) {
                    int px = (int)std::min(ma.startPt.x, ma.endPt.x);
                    int py = (int)std::min(ma.startPt.y, ma.endPt.y);
                    int pw = (int)std::abs(ma.endPt.x - ma.startPt.x);
                    int ph = (int)std::abs(ma.endPt.y - ma.startPt.y);
                    if (px + pw <= sx0 || px >= sx0 + sw0 || py + ph <= sy0 || py >= sy0 + sh0) return;
                    DWORD* subBase = g_Daemon.backBufferPixels + (size_t)sy0 * W + sx0;
                    ApplyPixelateFilterRawBuffer(
                        subBase, sw0, sh0, W * 4,
                        px - sx0, py - sy0, pw, ph,
                        PepperSnapDaemon::GetMosaicBlockSize(ma.strokeWidth),
                        ma.tool == OverlayTool::MosaicCircle
                    );
                };
                for (const auto& ann : g_Daemon.annotations) {
                    if (ann.tool == OverlayTool::MosaicSquare || ann.tool == OverlayTool::MosaicCircle) {
                        applyMosaicClipped(ann);
                    }
                }
                if (g_Daemon.dragMode == DragMode::DrawingAnnotation &&
                    (g_Daemon.draftAnn.tool == OverlayTool::MosaicSquare || g_Daemon.draftAnn.tool == OverlayTool::MosaicCircle)) {
                    applyMosaicClipped(g_Daemon.draftAnn);
                }
            }
        }
    }

    {
        Graphics g(memDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

        if (!g_Daemon.hDimmedDesktopDC) {
            if (g_Daemon.frozenDesktopBmp) {
                g.DrawImage(g_Daemon.frozenDesktopBmp, 0, 0, W, H);
            }
            SolidBrush scrimBrush(Color(135, 9, 13, 22));
            g.FillRectangle(&scrimBrush, 0, 0, W, H);
        }

        FontFamily ff(L"Segoe UI");
        Font hudFont(&ff, 11.5f, FontStyleBold, UnitPixel);
        Font smallFont(&ff, 11.0f, FontStyleRegular, UnitPixel);
        Font monoFont(L"Consolas", 11.5f, FontStyleBold, UnitPixel);
        SolidBrush whiteBrush(Color(255, 248, 250, 252));
        SolidBrush mutedBrush(Color(255, 148, 163, 184));

        if (g_Daemon.hasSelection) {
            int sx = std::max(0, std::min(W, (int)std::min(g_Daemon.selRect.left, g_Daemon.selRect.right)));
            int sy = std::max(0, std::min(H, (int)std::min(g_Daemon.selRect.top, g_Daemon.selRect.bottom)));
            int sw = std::min(W - sx, (int)std::abs(g_Daemon.selRect.right - g_Daemon.selRect.left));
            int sh = std::min(H - sy, (int)std::abs(g_Daemon.selRect.bottom - g_Daemon.selRect.top));

            if (sw >= 1 && sh >= 1 && g_Daemon.frozenDesktopBmp) {
                // Clip Highlighter & vector annotations to selection region in a single Rect clip
                g.SetClip(Rect(sx, sy, sw, sh), CombineModeReplace);
                const Annotation* draftHi = (g_Daemon.dragMode == DragMode::DrawingAnnotation && g_Daemon.draftAnn.tool == OverlayTool::Highlighter)
                    ? &g_Daemon.draftAnn
                    : nullptr;
                PepperSnapDaemon::RenderHighlighterLayer(
                    g, 0, 0, W, H,
                    g_Daemon.annotations, draftHi, g_Daemon.nonStackingHighlighter
                );

            for (const auto& ann : g_Daemon.annotations) {
                if (ann.tool == OverlayTool::Highlighter) {
                    if (ann.id == g_Daemon.selectedAnnotationId) {
                        Pen selPen(Color(255, 56, 189, 248), 1.5f);
                        selPen.SetDashStyle(DashStyleDash);
                        float x = std::min(ann.startPt.x, ann.endPt.x) - 6.0f;
                        float y = std::min(ann.startPt.y, ann.endPt.y) - 6.0f;
                        float w = std::max(16.0f, std::abs(ann.endPt.x - ann.startPt.x) + 12.0f);
                        float h = std::max(16.0f, std::abs(ann.endPt.y - ann.startPt.y) + 12.0f);
                        g.DrawRectangle(&selPen, x, y, w, h);
                    }
                } else if (ann.tool != OverlayTool::MosaicSquare && ann.tool != OverlayTool::MosaicCircle) {
                    PepperSnapDaemon::DrawVectorAnnotation(g, ann, 0, 0, ann.id == g_Daemon.selectedAnnotationId, false);
                } else {
                    // 1. Keep the temporary outline for Mosaic Rectangle & Mosaic Circle until save so it stays as an indicator!
                    float mx0 = std::min(ann.startPt.x, ann.endPt.x);
                    float my0 = std::min(ann.startPt.y, ann.endPt.y);
                    float mw  = std::abs(ann.endPt.x - ann.startPt.x);
                    float mh  = std::abs(ann.endPt.y - ann.startPt.y);
                    bool isSel = (ann.id == g_Daemon.selectedAnnotationId);
                    Pen whiteGuide(Color(230, 255, 255, 255), isSel ? 2.0f : 1.6f);
                    Pen dashGuide(isSel ? Color(255, 56, 189, 248) : Color(255, 239, 68, 68), isSel ? 2.0f : 1.6f);
                    dashGuide.SetDashStyle(DashStyleDash);
                    if (ann.tool == OverlayTool::MosaicCircle) {
                        g.DrawEllipse(&whiteGuide, mx0, my0, mw, mh);
                        g.DrawEllipse(&dashGuide, mx0, my0, mw, mh);
                    } else {
                        g.DrawRectangle(&whiteGuide, mx0, my0, mw, mh);
                        g.DrawRectangle(&dashGuide, mx0, my0, mw, mh);
                    }
                    if (isSel) {
                        SolidBrush hFill(Color(255, 255, 255, 255));
                        Pen hBorder(Color(255, 56, 189, 248), 1.5f);
                        g.FillRectangle(&hFill, ann.startPt.x - 4.0f, ann.startPt.y - 4.0f, 8.0f, 8.0f);
                        g.DrawRectangle(&hBorder, ann.startPt.x - 4.0f, ann.startPt.y - 4.0f, 8.0f, 8.0f);
                        g.FillRectangle(&hFill, ann.endPt.x - 4.0f, ann.endPt.y - 4.0f, 8.0f, 8.0f);
                        g.DrawRectangle(&hBorder, ann.endPt.x - 4.0f, ann.endPt.y - 4.0f, 8.0f, 8.0f);
                    }
                }
            }
            if (g_Daemon.dragMode == DragMode::DrawingAnnotation) {
                if (g_Daemon.draftAnn.tool != OverlayTool::MosaicSquare &&
                    g_Daemon.draftAnn.tool != OverlayTool::MosaicCircle &&
                    g_Daemon.draftAnn.tool != OverlayTool::Highlighter) {
                    PepperSnapDaemon::DrawVectorAnnotation(g, g_Daemon.draftAnn, 0, 0, false, false);
                } else if (g_Daemon.draftAnn.tool == OverlayTool::MosaicSquare ||
                           g_Daemon.draftAnn.tool == OverlayTool::MosaicCircle) {
                    // 4. Temporary outline for Mosaic Rectangle & Mosaic Circle while dragging so it's easy to see
                    float mx0 = std::min(g_Daemon.draftAnn.startPt.x, g_Daemon.draftAnn.endPt.x);
                    float my0 = std::min(g_Daemon.draftAnn.startPt.y, g_Daemon.draftAnn.endPt.y);
                    float mw  = std::abs(g_Daemon.draftAnn.endPt.x - g_Daemon.draftAnn.startPt.x);
                    float mh  = std::abs(g_Daemon.draftAnn.endPt.y - g_Daemon.draftAnn.startPt.y);
                    Pen outerWhite(Color(235, 255, 255, 255), 2.0f);
                    Pen innerDash(Color(255, 239, 68, 68), 2.0f);
                    innerDash.SetDashStyle(DashStyleDash);
                    if (g_Daemon.draftAnn.tool == OverlayTool::MosaicCircle) {
                        g.DrawEllipse(&outerWhite, mx0, my0, mw, mh);
                        g.DrawEllipse(&innerDash, mx0, my0, mw, mh);
                    } else {
                        g.DrawRectangle(&outerWhite, mx0, my0, mw, mh);
                        g.DrawRectangle(&innerDash, mx0, my0, mw, mh);
                    }
                }
            }
            if (g_Daemon.isEditingText) {
                PepperSnapDaemon::DrawVectorAnnotation(g, g_Daemon.editingTextAnn, 0, 0, true, true);
            }
            g.ResetClip();

            // Selection Frame + 8 Resize Handles (Clamped to screen boundaries so border hits screen sides cleanly)
            Pen selBorder(Color(255, 239, 68, 68), 1.8f);
            selBorder.SetAlignment(PenAlignmentInset);
            int drawX = std::max(0, sx);
            int drawY = std::max(0, sy);
            int drawR = std::min(W - 1, sx + sw);
            int drawB = std::min(H - 1, sy + sh);
            g.DrawRectangle(&selBorder, drawX, drawY, std::max(1, drawR - drawX), std::max(1, drawB - drawY));

            int xm = sx + sw / 2, ym = sy + sh / 2;
            POINT handles[8] = {
                {sx, sy}, {xm, sy}, {sx + sw, sy},
                {sx + sw, ym}, {sx + sw, sy + sh}, {xm, sy + sh},
                {sx, sy + sh}, {sx, ym}
            };
            SolidBrush handleFill(Color(255, 255, 255, 255));
            Pen handleOutline(Color(255, 239, 68, 68), 1.5f);
            for (const auto& pt : handles) {
                g.FillRectangle(&handleFill, pt.x - 4, pt.y - 4, 8, 8);
                g.DrawRectangle(&handleOutline, pt.x - 4, pt.y - 4, 8, 8);
            }

            // 5. Typeable Dimension Pill — anchored to the stationary (non-moving) corner when resizing!
            bool pillRight = false, pillBottom = false;
            g_Daemon.GetDimensionPillAnchor(pillRight, pillBottom);
            int pillW = g_Daemon.isEditingSize ? 146 : 140;
            int pillH = 23;
            int pillX = pillRight ? (sx + sw - pillW) : sx;
            pillX = std::max(4, std::min(W - pillW - 4, pillX));

            int pillY = 0;
            if (!pillBottom) {
                pillY = (sy > 32) ? (sy - 28) : (sy + 6);
            } else {
                pillY = (sy + sh + 29 < H) ? (sy + sh + 6) : (sy + sh - 29);
            }
            pillY = std::max(4, std::min(H - pillH - 4, pillY));

            RECT candidatePill = { pillX, pillY, pillX + pillW, pillY + pillH };
            RECT overlapRc;
            if (IntersectRect(&overlapRc, &candidatePill, &g_Daemon.hudBoundsRect)) {
                if (!pillBottom && pillY < sy) {
                    pillY = sy + 6;
                } else if (pillBottom && pillY > sy + sh) {
                    pillY = std::max(4, sy + sh - 29);
                }
            }
            g_Daemon.dimPillRect = { pillX, pillY, pillX + pillW, pillY + pillH };
            bool pillHovered = PtInRect(&g_Daemon.dimPillRect, g_Daemon.mousePt);

            SolidBrush pillBg(g_Daemon.isEditingSize ? Color(252, 30, 41, 59) : Color(235, 15, 23, 42));
            g.FillRectangle(&pillBg, pillX, pillY, pillW, pillH);
            Pen pillBorder(g_Daemon.isEditingSize ? Color(255, 56, 189, 248) : (pillHovered ? Color(255, 239, 68, 68) : Color(160, 71, 85, 105)), 1.4f);
            g.DrawRectangle(&pillBorder, pillX, pillY, pillW, pillH);

            std::wstring dimValStr = g_Daemon.isEditingSize
                ? g_Daemon.editingSizeText
                : (std::to_wstring(sw) + L" × " + std::to_wstring(sh));
            PepperSnapDaemon::DrawEditablePillText(
                g, monoFont, pillX + 7, pillY + 4, pillY, pillH,
                dimValStr, L" px",
                g_Daemon.isEditingSize, g_Daemon.sizeCaretPos, g_Daemon.sizeSelAnchor
            );
        }

        // Docked Toolbars
        StringFormat centerFmt;
        centerFmt.SetAlignment(StringAlignmentCenter);
        centerFmt.SetLineAlignment(StringAlignmentCenter);

        for (const auto& b : g_Daemon.dockButtons) {
            RectF rf((float)b.rect.left, (float)b.rect.top, (float)(b.rect.right - b.rect.left), (float)(b.rect.bottom - b.rect.top));
            bool hovered = (b.id == g_Daemon.hoveredBtnId);

            if (b.isColor) {
                SolidBrush swB(b.swatchColor);
                g.FillRectangle(&swB, rf);
                bool actCol = (b.swatchColor.GetValue() == g_Daemon.activeColor.GetValue());
                bool isWhiteSwatch = (b.swatchColor.GetR() > 235 && b.swatchColor.GetG() > 235 && b.swatchColor.GetB() > 235);
                Color borderCol = actCol
                    ? (isWhiteSwatch ? Color(255, 239, 68, 68) : Color(255, 255, 255, 255))
                    : (hovered ? Color(255, 203, 213, 225) : Color(220, 71, 85, 105));
                Pen swP(borderCol, actCol ? 2.2f : 1.0f);
                g.DrawRectangle(&swP, rf.X, rf.Y, rf.Width, rf.Height);
                continue;
            }

            bool activeState = (b.isTool && b.tool == g_Daemon.activeTool) ||
                               (b.isStroke && std::abs(b.strokeVal - g_Daemon.activeStroke) < 0.5f);
            Color bgCol = Color(240, 15, 23, 42);
            if (b.isPrimaryAction || activeState) {
                bgCol = hovered ? Color(255, 220, 38, 38) : Color(255, 239, 68, 68);
            } else if (hovered) {
                bgCol = Color(255, 51, 65, 85);
            }

            SolidBrush btnBg(bgCol);
            Pen btnBorder(activeState ? Color(255, 252, 165, 165) : Color(220, 51, 65, 85), 1.0f);
            g.FillRectangle(&btnBg, rf);
            g.DrawRectangle(&btnBorder, rf.X, rf.Y, rf.Width, rf.Height);
            if (b.isStroke) {
                g.DrawString(b.label.c_str(), -1, &hudFont, rf, &centerFmt, &whiteBrush);
            } else {
                PepperSnapDaemon::DrawDockButtonIcon(g, b, rf);
            }
        }

        // 5. Render Typeable Custom Size Input Box on the right side of [XL]
        if (g_Daemon.customStrokeRect.right > g_Daemon.customStrokeRect.left) {
            RectF crf(
                (float)g_Daemon.customStrokeRect.left,
                (float)g_Daemon.customStrokeRect.top,
                (float)(g_Daemon.customStrokeRect.right - g_Daemon.customStrokeRect.left),
                (float)(g_Daemon.customStrokeRect.bottom - g_Daemon.customStrokeRect.top)
            );
            bool cHovered = PtInRect(&g_Daemon.customStrokeRect, g_Daemon.mousePt);
            SolidBrush cBg(g_Daemon.isEditingStroke ? Color(252, 30, 41, 59) : Color(240, 15, 23, 42));
            Pen cBorder(
                g_Daemon.isEditingStroke ? Color(255, 56, 189, 248) : (cHovered ? Color(255, 239, 68, 68) : Color(220, 51, 65, 85)),
                1.3f
            );
            g.FillRectangle(&cBg, crf);
            g.DrawRectangle(&cBorder, crf.X, crf.Y, crf.Width, crf.Height);

            std::wstring cValStr = g_Daemon.isEditingStroke
                ? g_Daemon.editingStrokeText
                : std::to_wstring((int)std::round(g_Daemon.activeStroke));
            PepperSnapDaemon::DrawEditablePillText(
                g, monoFont,
                g_Daemon.customStrokeRect.left + 6,
                g_Daemon.customStrokeRect.top + 6,
                g_Daemon.customStrokeRect.top,
                g_Daemon.customStrokeRect.bottom - g_Daemon.customStrokeRect.top,
                cValStr, L" px",
                g_Daemon.isEditingStroke, g_Daemon.strokeCaretPos, g_Daemon.strokeSelAnchor
            );

            if (cHovered && !g_Daemon.isEditingStroke && g_Daemon.hoveredBtnId == -1) {
                DockButton fakeTip;
                fakeTip.rect = g_Daemon.customStrokeRect;
                fakeTip.tooltip = L"Custom Size — Click to type custom size (1–120px)";
                fakeTip.isTool = false;
                PepperSnapDaemon::DrawHoverBubbleTooltip(g, fakeTip, W, H);
            }
        }

        // Render Floating Speech-Bubble Tooltip when hovering any button
        if (g_Daemon.hoveredBtnId != -1) {
            for (const auto& b : g_Daemon.dockButtons) {
                if (b.id == g_Daemon.hoveredBtnId) {
                    PepperSnapDaemon::DrawHoverBubbleTooltip(g, b, W, H);
                    break;
                }
            }
        }
    } else {
        Pen crossPen(Color(150, 239, 68, 68), 1.0f);
        crossPen.SetDashStyle(DashStyleDash);
        g.DrawLine(&crossPen, g_Daemon.mousePt.x, 0, g_Daemon.mousePt.x, H);
        g.DrawLine(&crossPen, 0, g_Daemon.mousePt.y, W, g_Daemon.mousePt.y);
    }

    // 5x Magnifier Loupe when aiming
    if (!g_Daemon.hasSelection || g_Daemon.dragMode == DragMode::CreatingSelection) {
        int mx = g_Daemon.mousePt.x, my = g_Daemon.mousePt.y;
        int loupeW = 132, loupeH = 136;
        int lx = (mx + 24 + loupeW < W) ? (mx + 24) : (mx - loupeW - 24);
        int ly = (my + 24 + loupeH < H) ? (my + 24) : (my - loupeH - 24);

        SolidBrush loupeBg(Color(240, 15, 23, 42));
        Pen loupeBorder(Color(255, 239, 68, 68), 1.5f);
        g.FillRectangle(&loupeBg, lx, ly, loupeW, loupeH);

        if (g_Daemon.frozenDesktopBmp) {
            if (g_Daemon.hBrightDesktopDC) {
                g.Flush(FlushIntentionSync);
                int srcX = std::max(0, std::min(W - 22, mx - 11));
                int srcY = std::max(0, std::min(H - 16, my - 8));
                SetStretchBltMode(memDC, COLORONCOLOR);
                StretchBlt(memDC, lx + 6, ly + 6, 120, 90, g_Daemon.hBrightDesktopDC, srcX, srcY, 22, 16, SRCCOPY);
            } else {
                GraphicsState st = g.Save();
                g.SetInterpolationMode(InterpolationModeNearestNeighbor);
                g.SetPixelOffsetMode(PixelOffsetModeHalf);
                g.DrawImage(g_Daemon.frozenDesktopBmp, Rect(lx + 6, ly + 6, 120, 90), mx - 11, my - 8, 22, 16, UnitPixel);
                g.Restore(st);
            }

            Pen centerCross(Color(200, 239, 68, 68), 1.5f);
            g.DrawLine(&centerCross, lx + 66, ly + 6, lx + 66, ly + 96);
            g.DrawLine(&centerCross, lx + 6, ly + 51, lx + 126, ly + 51);

            int cMx = std::max(0, std::min(W - 1, mx));
            int cMy = std::max(0, std::min(H - 1, my));
            BYTE pr = 0, pg = 0, pb = 0;
            if (g_Daemon.brightDibPixels) {
                DWORD raw = g_Daemon.brightDibPixels[(size_t)cMy * W + cMx];
                pb = (BYTE)(raw & 0xFF);
                pg = (BYTE)((raw >> 8) & 0xFF);
                pr = (BYTE)((raw >> 16) & 0xFF);
            } else {
                Color pxCol;
                g_Daemon.frozenDesktopBmp->GetPixel(cMx, cMy, &pxCol);
                pr = pxCol.GetR();
                pg = pxCol.GetG();
                pb = pxCol.GetB();
            }
            wchar_t hexBuf[64];
            swprintf_s(hexBuf, L"(%d,%d) #%02X%02X%02X", mx, my, pr, pg, pb);
            g.DrawString(hexBuf, -1, &monoFont, PointF((float)(lx + 8), (float)(ly + 102)), &whiteBrush);
            g.DrawString(L"Ctrl+C copy RGB · Esc", -1, &smallFont, PointF((float)(lx + 8), (float)(ly + 118)), &mutedBrush);
        }
        g.DrawRectangle(&loupeBorder, lx, ly, loupeW, loupeH);
    }
    } // Graphics g(memDC) scope ends and flushes before BitBlt to screen

    BitBlt(hdc, 0, 0, W, H, memDC, 0, 0, SRCCOPY);
    if (tempBackBuffer) {
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);
    }
}

static LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            SetTimer(hWnd, 1, 450, nullptr); // Caret blink timer for in-place text box
            return 0;

        case WM_TIMER:
            if (g_Daemon.isEditingText || g_Daemon.isEditingSize || g_Daemon.isEditingStroke) InvalidateRect(hWnd, nullptr, FALSE);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                return TRUE;
            }
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RenderOverlayWindow(hWnd, hdc);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_RBUTTONUP:
            if (g_Daemon.isEditingText) {
                g_Daemon.CommitActiveTextBox();
                InvalidateRect(hWnd, nullptr, FALSE);
            } else {
                g_Daemon.CloseRegionSnipOverlay();
            }
            return 0;

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            float nextS = std::max(1.0f, std::min(120.0f, g_Daemon.activeStroke + (delta > 0 ? 1.5f : -1.5f)));
            g_Daemon.ApplyStrokeToSelectedAnnotation(nextS, false);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_LBUTTONDBLCLK: {
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);
            POINT pt{ mx, my };
            if (g_Daemon.hasSelection && PtInRect(&g_Daemon.customStrokeRect, pt)) {
                if (!g_Daemon.isEditingStroke) {
                    g_Daemon.isEditingStroke = true;
                    g_Daemon.editingStrokeText = std::to_wstring((int)std::round(g_Daemon.activeStroke));
                }
                g_Daemon.strokeSelAnchor = 0;
                g_Daemon.strokeCaretPos = g_Daemon.editingStrokeText.size();
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.hasSelection && PtInRect(&g_Daemon.dimPillRect, pt)) {
                if (!g_Daemon.isEditingSize) {
                    int sw = std::abs(g_Daemon.selRect.right - g_Daemon.selRect.left);
                    int sh = std::abs(g_Daemon.selRect.bottom - g_Daemon.selRect.top);
                    g_Daemon.isEditingSize = true;
                    g_Daemon.editingSizeText = std::to_wstring(sw) + L"x" + std::to_wstring(sh);
                }
                size_t idx = PepperSnapDaemon::HitTestMonoIndex(mx, g_Daemon.dimPillRect.left + 7, g_Daemon.editingSizeText);
                const std::wstring& s = g_Daemon.editingSizeText;
                if (idx < s.size() && (s[idx] >= L'0' && s[idx] <= L'9')) {
                    size_t l = idx, r = idx;
                    while (l > 0 && s[l - 1] >= L'0' && s[l - 1] <= L'9') l--;
                    while (r < s.size() && s[r] >= L'0' && s[r] <= L'9') r++;
                    g_Daemon.sizeSelAnchor = l;
                    g_Daemon.sizeCaretPos = r;
                } else {
                    g_Daemon.sizeSelAnchor = 0;
                    g_Daemon.sizeCaretPos = s.size();
                }
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.isEditingText) {
                RECT tbRc = g_Daemon.GetEditingTextBoxRect();
                if (PtInRect(&tbRc, pt)) {
                    size_t idx = g_Daemon.HitTestTextBoxIndex(mx, my);
                    const std::wstring& s = g_Daemon.editingTextAnn.text;
                    if (idx < s.size() && s[idx] != L' ' && s[idx] != L'\n') {
                        size_t l = idx, r = idx;
                        while (l > 0 && s[l - 1] != L' ' && s[l - 1] != L'\n') l--;
                        while (r < s.size() && s[r] != L' ' && s[r] != L'\n') r++;
                        g_Daemon.textSelAnchor = l;
                        g_Daemon.textCaretPos = r;
                    } else {
                        g_Daemon.textSelAnchor = 0;
                        g_Daemon.textCaretPos = s.size();
                    }
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            SetFocus(hWnd);
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);
            POINT pt{ mx, my };
            g_Daemon.mousePt = pt;
            bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

            // 0a. Check Typeable Custom Stroke / Text Size Input Box (Right of [XL])
            if (g_Daemon.hasSelection && PtInRect(&g_Daemon.customStrokeRect, pt)) {
                g_Daemon.CommitActiveTextBox();
                g_Daemon.CommitActiveSizeInput();
                if (!g_Daemon.isEditingStroke) {
                    g_Daemon.isEditingStroke = true;
                    g_Daemon.editingStrokeText = std::to_wstring((int)std::round(g_Daemon.activeStroke));
                }
                size_t idx = PepperSnapDaemon::HitTestMonoIndex(mx, g_Daemon.customStrokeRect.left + 6, g_Daemon.editingStrokeText);
                g_Daemon.strokeCaretPos = idx;
                if (!shiftHeld) g_Daemon.strokeSelAnchor = idx;
                g_Daemon.isDraggingStrokeText = true;
                SetCapture(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.isEditingStroke) {
                g_Daemon.CommitActiveStrokeInput();
                InvalidateRect(hWnd, nullptr, FALSE);
            }

            // 0b. Check Typeable Size Indicator Pill Click (unless grabbing a resize handle while not editing size)
            DragMode preHandleHit = g_Daemon.isEditingSize ? DragMode::None : g_Daemon.HitTestSelectionHandles(mx, my);
            if (preHandleHit == DragMode::None && g_Daemon.hasSelection && PtInRect(&g_Daemon.dimPillRect, pt)) {
                g_Daemon.CommitActiveTextBox();
                if (!g_Daemon.isEditingSize) {
                    int sw = std::abs(g_Daemon.selRect.right - g_Daemon.selRect.left);
                    int sh = std::abs(g_Daemon.selRect.bottom - g_Daemon.selRect.top);
                    g_Daemon.isEditingSize = true;
                    g_Daemon.editingSizeText = std::to_wstring(sw) + L"x" + std::to_wstring(sh);
                }
                size_t idx = PepperSnapDaemon::HitTestMonoIndex(mx, g_Daemon.dimPillRect.left + 7, g_Daemon.editingSizeText);
                g_Daemon.sizeCaretPos = idx;
                if (!shiftHeld) g_Daemon.sizeSelAnchor = idx;
                g_Daemon.isDraggingSizeText = true;
                SetCapture(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.isEditingSize) {
                g_Daemon.CommitActiveSizeInput();
                InvalidateRect(hWnd, nullptr, FALSE);
            }

            // 1. Check Docked Toolbar Buttons
            for (const auto& b : g_Daemon.dockButtons) {
                if (PtInRect(&b.rect, pt)) {
                    g_Daemon.CommitActiveTextBox();
                    if (b.isTool) {
                        g_Daemon.activeTool = b.tool;
                        InvalidateRect(hWnd, nullptr, FALSE);
                        return 0;
                    }
                    if (b.isColor) {
                        g_Daemon.activeColor = b.swatchColor;
                        if (g_Daemon.selectedAnnotationId != -1) {
                            g_Daemon.PushUndo();
                            for (auto& a : g_Daemon.annotations) {
                                if (a.id == g_Daemon.selectedAnnotationId) a.color = b.swatchColor;
                            }
                        }
                        if (g_Daemon.isEditingText) {
                            g_Daemon.editingTextAnn.color = b.swatchColor;
                        }
                        InvalidateRect(hWnd, nullptr, FALSE);
                        return 0;
                    }
                    if (b.isStroke) {
                        // 1. Apply stroke size change (S/M/L/XL) to selected annotation immediately!
                        g_Daemon.ApplyStrokeToSelectedAnnotation(b.strokeVal, true);
                        InvalidateRect(hWnd, nullptr, FALSE);
                        return 0;
                    }
                    switch (b.id) {
                        case DBTN_ACT_DRAG_HUD:
                            if (!g_Daemon.hasCustomHudPos) {
                                g_Daemon.customHudPos = { g_Daemon.hudBoundsRect.right, g_Daemon.hudBoundsRect.top };
                                g_Daemon.hasCustomHudPos = true;
                            }
                            g_Daemon.dragMode = DragMode::DraggingHUD;
                            g_Daemon.dragHudStartMouse = pt;
                            g_Daemon.dragHudOrigPos = g_Daemon.customHudPos;
                            SetCapture(hWnd);
                            g_Daemon.UpdateOverlayCursor(mx, my);
                            InvalidateRect(hWnd, nullptr, FALSE);
                            return 0;
                        case DBTN_ACT_COPY:      g_Daemon.ActionCopyAndClose(); return 0;
                        case DBTN_ACT_SAVE:      g_Daemon.ActionQuickSaveAndClose(); return 0;
                        case DBTN_ACT_SAVE_AS:   g_Daemon.ActionSaveAsAndClose(); return 0;
                        case DBTN_ACT_PIN:       g_Daemon.ActionPinToDesktop(); return 0;
                        case DBTN_ACT_OPTIONS:
                            g_Daemon.ShowOptionsModal();
                            if (g_Daemon.hOverlayWnd) InvalidateRect(hWnd, nullptr, FALSE);
                            return 0;
                        case DBTN_ACT_UNDO:      g_Daemon.Undo(); return 0;
                        case DBTN_ACT_REDO:      g_Daemon.Redo(); return 0;
                        case DBTN_ACT_RESET_NUM:
                            // 4. Reset Numbering Arrow counter back to 1
                            g_Daemon.nextStepNum = 1;
                            InvalidateRect(hWnd, nullptr, FALSE);
                            return 0;
                        case DBTN_ACT_CLEAR:
                            g_Daemon.PushUndo();
                            g_Daemon.annotations.clear();
                            g_Daemon.nextStepNum = 1;
                            g_Daemon.selectedAnnotationId = -1;
                            InvalidateRect(hWnd, nullptr, FALSE);
                            return 0;
                        case DBTN_ACT_CLOSE:     g_Daemon.CloseRegionSnipOverlay(); return 0;
                    }
                }
            }

            // If currently editing a text box, check if clicking inside it to move caret / drag-select!
            if (g_Daemon.isEditingText) {
                RECT tbRc = g_Daemon.GetEditingTextBoxRect();
                if (PtInRect(&tbRc, pt)) {
                    size_t idx = g_Daemon.HitTestTextBoxIndex(mx, my);
                    g_Daemon.textCaretPos = idx;
                    if (!shiftHeld) g_Daemon.textSelAnchor = idx;
                    g_Daemon.isDraggingTextBoxSel = true;
                    SetCapture(hWnd);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                g_Daemon.CommitActiveTextBox();
                InvalidateRect(hWnd, nullptr, FALSE);
            }

            // 2. Check 8 Selection Resize Handles
            DragMode handleHit = g_Daemon.HitTestSelectionHandles(mx, my);
            if (handleHit != DragMode::None) {
                g_Daemon.dragMode = handleHit;
                g_Daemon.dragStartPt = pt;
                g_Daemon.dragOrigRect = {
                    std::min(g_Daemon.selRect.left, g_Daemon.selRect.right),
                    std::min(g_Daemon.selRect.top, g_Daemon.selRect.bottom),
                    std::max(g_Daemon.selRect.left, g_Daemon.selRect.right),
                    std::max(g_Daemon.selRect.top, g_Daemon.selRect.bottom)
                };
                SetCapture(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }

            // 3. Inside Selection Box
            if (g_Daemon.hasSelection) {
                RECT normSel = {
                    std::min(g_Daemon.selRect.left, g_Daemon.selRect.right),
                    std::min(g_Daemon.selRect.top, g_Daemon.selRect.bottom),
                    std::max(g_Daemon.selRect.left, g_Daemon.selRect.right),
                    std::max(g_Daemon.selRect.top, g_Daemon.selRect.bottom)
                };
                if (PtInRect(&normSel, pt)) {
                    if (g_Daemon.activeTool == OverlayTool::SelectMove) {
                        DragMode annDragMode = DragMode::MovingAnnotation;
                        int hitId = g_Daemon.HitTestAnnotation((float)mx, (float)my, &annDragMode);
                        g_Daemon.selectedAnnotationId = hitId;
                        if (hitId != -1) {
                            g_Daemon.PushUndo();
                            g_Daemon.dragMode = annDragMode;
                            for (const auto& a : g_Daemon.annotations) {
                                if (a.id == hitId) {
                                    g_Daemon.activeStroke = a.strokeWidth;
                                    g_Daemon.activeColor = a.color;
                                    g_Daemon.moveAnnOffset = { (float)mx - a.startPt.x, (float)my - a.startPt.y };
                                    break;
                                }
                            }
                        } else {
                            g_Daemon.dragMode = DragMode::MovingSelection;
                            g_Daemon.dragStartPt = pt;
                            g_Daemon.dragOrigRect = normSel;
                        }
                        SetCapture(hWnd);
                        InvalidateRect(hWnd, nullptr, FALSE);
                        return 0;
                    }

                    // Interactive In-Place Text Box Tool (Click existing text to re-edit, or click to create new)
                    if (g_Daemon.activeTool == OverlayTool::TextBox) {
                        for (auto it = g_Daemon.annotations.rbegin(); it != g_Daemon.annotations.rend(); ++it) {
                            if (it->tool == OverlayTool::TextBox) {
                                float minX = std::min(it->startPt.x, it->endPt.x) - 8.0f;
                                float maxX = std::max(it->startPt.x, it->endPt.x) + 8.0f;
                                float minY = std::min(it->startPt.y, it->endPt.y) - 8.0f;
                                float maxY = std::max(it->startPt.y, it->endPt.y) + 8.0f;
                                if (mx >= minX && mx <= maxX && my >= minY && my <= maxY) {
                                    g_Daemon.editingTextAnn = *it;
                                    g_Daemon.annotations.erase(std::next(it).base());
                                    g_Daemon.isEditingText = true;
                                    size_t idx = g_Daemon.HitTestTextBoxIndex(mx, my);
                                    g_Daemon.textCaretPos = idx;
                                    g_Daemon.textSelAnchor = idx;
                                    g_Daemon.isDraggingTextBoxSel = true;
                                    SetCapture(hWnd);
                                    InvalidateRect(hWnd, nullptr, FALSE);
                                    return 0;
                                }
                            }
                        }
                        g_Daemon.isEditingText = true;
                        g_Daemon.isDraggingTextBoxSel = false;
                        g_Daemon.textCaretPos = 0;
                        g_Daemon.textSelAnchor = 0;
                        g_Daemon.editingTextAnn = Annotation();
                        g_Daemon.editingTextAnn.id = g_Daemon.nextAnnotationId++;
                        g_Daemon.editingTextAnn.tool = OverlayTool::TextBox;
                        g_Daemon.editingTextAnn.color = g_Daemon.activeColor;
                        g_Daemon.editingTextAnn.strokeWidth = g_Daemon.activeStroke;
                        g_Daemon.editingTextAnn.startPt = { (float)mx, (float)my };
                        g_Daemon.editingTextAnn.endPt   = { (float)(mx + 160), (float)(my + 34) };
                        g_Daemon.editingTextAnn.text = L"";
                        InvalidateRect(hWnd, nullptr, FALSE);
                        return 0;
                    }

                    // Start drawing shape / Pen / Arrow / NumberArrow / Pixelate
                    g_Daemon.selectedAnnotationId = -1;
                    g_Daemon.dragMode = DragMode::DrawingAnnotation;
                    g_Daemon.draftAnn = Annotation();
                    g_Daemon.draftAnn.id = g_Daemon.nextAnnotationId++;
                    g_Daemon.draftAnn.tool = g_Daemon.activeTool;
                    g_Daemon.draftAnn.color = g_Daemon.activeColor;
                    g_Daemon.draftAnn.strokeWidth = g_Daemon.activeStroke;
                    g_Daemon.draftAnn.startPt = { (float)mx, (float)my };
                    g_Daemon.draftAnn.endPt   = { (float)mx, (float)my };
                    g_Daemon.draftAnn.points.push_back({ (float)mx, (float)my });
                    g_Daemon.penSnapAnchor    = { (float)mx, (float)my };
                    g_Daemon.isPenShiftSnapping = false;
                    if (g_Daemon.activeTool == OverlayTool::NumberArrow) {
                        g_Daemon.draftAnn.stepNumber = g_Daemon.nextStepNum;
                    }
                    SetCapture(hWnd);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
            }

            // 4. Outside Selection Box -> Do NOT reset selection on simple click or click-and-hold!
            // Only enter PendingOutsideSelection; actual reset happens in WM_MOUSEMOVE only if dragged >= 5px.
            g_Daemon.selectedAnnotationId = -1;
            g_Daemon.dragMode = DragMode::PendingOutsideSelection;
            g_Daemon.dragStartPt = pt;
            SetCapture(hWnd);
            g_Daemon.UpdateOverlayCursor(mx, my);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_MOUSEMOVE: {
            bool isFreehandDrawing = (g_Daemon.dragMode == DragMode::DrawingAnnotation &&
                                     (g_Daemon.draftAnn.tool == OverlayTool::Pen || g_Daemon.draftAnn.tool == OverlayTool::Highlighter));
            if (!isFreehandDrawing) {
                // Coalesce queued WM_MOUSEMOVE events for non-freehand drags so high-polling-rate mice never lag
                MSG nextMove;
                while (PeekMessageW(&nextMove, hWnd, WM_MOUSEMOVE, WM_MOUSEMOVE, PM_REMOVE)) {
                    lParam = nextMove.lParam;
                    wParam = nextMove.wParam;
                }
            }
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);
            POINT pt{ mx, my };
            g_Daemon.mousePt = pt;

            if (g_Daemon.isDraggingTextBoxSel && g_Daemon.isEditingText) {
                g_Daemon.textCaretPos = g_Daemon.HitTestTextBoxIndex(mx, my);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.isDraggingStrokeText) {
                g_Daemon.strokeCaretPos = PepperSnapDaemon::HitTestMonoIndex(
                    mx, g_Daemon.customStrokeRect.left + 6, g_Daemon.editingStrokeText
                );
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.isDraggingSizeText) {
                g_Daemon.sizeCaretPos = PepperSnapDaemon::HitTestMonoIndex(
                    mx, g_Daemon.dimPillRect.left + 7, g_Daemon.editingSizeText
                );
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }

            if (g_Daemon.dragMode == DragMode::None) {
                int newHover = -1;
                for (const auto& b : g_Daemon.dockButtons) {
                    if (PtInRect(&b.rect, pt)) {
                        newHover = b.id;
                        break;
                    }
                }
                bool pillHover = g_Daemon.hasSelection && ( PtInRect(&g_Daemon.dimPillRect, pt) != FALSE );
                bool strokeHover = g_Daemon.hasSelection && ( PtInRect(&g_Daemon.customStrokeRect, pt) != FALSE );
                bool hoverChanged = (newHover != g_Daemon.hoveredBtnId) ||
                                    (pillHover != g_Daemon.lastPillHover) ||
                                    (strokeHover != g_Daemon.lastStrokeHover);
                g_Daemon.hoveredBtnId = newHover;
                g_Daemon.lastPillHover = pillHover;
                g_Daemon.lastStrokeHover = strokeHover;
                g_Daemon.UpdateOverlayCursor(mx, my);
                if (!g_Daemon.hasSelection || hoverChanged) {
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }

            int clampedMx = std::max(0, std::min(g_Daemon.vScreenW, mx));
            int clampedMy = std::max(0, std::min(g_Daemon.vScreenH, my));
            int dx = mx - g_Daemon.dragStartPt.x;
            int dy = my - g_Daemon.dragStartPt.y;
            bool selShiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

            auto applySquareCorner = [&](int anchorX, int anchorY) {
                int sdx = clampedMx - anchorX;
                int sdy = clampedMy - anchorY;
                int side = std::max(std::abs(sdx), std::abs(sdy));
                int maxSideX = (sdx >= 0) ? (g_Daemon.vScreenW - anchorX) : anchorX;
                int maxSideY = (sdy >= 0) ? (g_Daemon.vScreenH - anchorY) : anchorY;
                side = std::min(side, std::min(maxSideX, maxSideY));
                int tx = anchorX + (sdx >= 0 ? side : -side);
                int ty = anchorY + (sdy >= 0 ? side : -side);
                g_Daemon.selRect = { std::min(anchorX, tx), std::min(anchorY, ty), std::max(anchorX, tx), std::max(anchorY, ty) };
            };

            switch (g_Daemon.dragMode) {
                case DragMode::DraggingHUD: {
                    int totalW = std::max(100, (int)(g_Daemon.hudBoundsRect.right - g_Daemon.hudBoundsRect.left));
                    int totalH = std::max(40, (int)(g_Daemon.hudBoundsRect.bottom - g_Daemon.hudBoundsRect.top));
                    int newRightX = g_Daemon.dragHudOrigPos.x + (mx - g_Daemon.dragHudStartMouse.x);
                    int newTopY   = g_Daemon.dragHudOrigPos.y + (my - g_Daemon.dragHudStartMouse.y);
                    newRightX = std::max(totalW + 6, std::min(g_Daemon.vScreenW - 6, newRightX));
                    newTopY   = std::max(6, std::min(g_Daemon.vScreenH - totalH - 6, newTopY));
                    g_Daemon.customHudPos = { newRightX, newTopY };
                    g_Daemon.hasCustomHudPos = true;
                    break;
                }
                case DragMode::PendingOutsideSelection:
                    if (std::abs(dx) >= 5 || std::abs(dy) >= 5) {
                        g_Daemon.hasSelection = true;
                        g_Daemon.hasCustomHudPos = false;
                        g_Daemon.activeTool = OverlayTool::SelectMove;
                        g_Daemon.annotations.clear();
                        g_Daemon.undoStack.clear();
                        g_Daemon.redoStack.clear();
                        g_Daemon.nextStepNum = 1;
                        g_Daemon.selectedAnnotationId = -1;
                        g_Daemon.dragMode = DragMode::CreatingSelection;
                        int startX = std::max(0, std::min(g_Daemon.vScreenW, (int)g_Daemon.dragStartPt.x));
                        int startY = std::max(0, std::min(g_Daemon.vScreenH, (int)g_Daemon.dragStartPt.y));
                        g_Daemon.dragOrigRect = { startX, startY, startX, startY };
                        if (selShiftHeld) {
                            applySquareCorner(startX, startY);
                        } else {
                            g_Daemon.selRect = { startX, startY, clampedMx, clampedMy };
                        }
                    } else {
                        return 0;
                    }
                    break;
                case DragMode::CreatingSelection:
                    g_Daemon.hasCustomHudPos = false;
                    if (selShiftHeld) {
                        int startX = std::max(0, std::min(g_Daemon.vScreenW, (int)g_Daemon.dragStartPt.x));
                        int startY = std::max(0, std::min(g_Daemon.vScreenH, (int)g_Daemon.dragStartPt.y));
                        applySquareCorner(startX, startY);
                    } else {
                        int startX = std::max(0, std::min(g_Daemon.vScreenW, (int)g_Daemon.dragStartPt.x));
                        int startY = std::max(0, std::min(g_Daemon.vScreenH, (int)g_Daemon.dragStartPt.y));
                        g_Daemon.selRect = { startX, startY, clampedMx, clampedMy };
                    }
                    break;
                case DragMode::MovingSelection: {
                    g_Daemon.hasCustomHudPos = false;
                    int cDx = std::max((int)(-g_Daemon.dragOrigRect.left), std::min((int)(g_Daemon.vScreenW - g_Daemon.dragOrigRect.right), dx));
                    int cDy = std::max((int)(-g_Daemon.dragOrigRect.top),  std::min((int)(g_Daemon.vScreenH - g_Daemon.dragOrigRect.bottom), dy));
                    g_Daemon.selRect.left   = g_Daemon.dragOrigRect.left + cDx;
                    g_Daemon.selRect.top    = g_Daemon.dragOrigRect.top + cDy;
                    g_Daemon.selRect.right  = g_Daemon.dragOrigRect.right + cDx;
                    g_Daemon.selRect.bottom = g_Daemon.dragOrigRect.bottom + cDy;
                    break;
                }
                case DragMode::ResizeTL:
                    g_Daemon.hasCustomHudPos = false;
                    if (selShiftHeld) applySquareCorner(g_Daemon.dragOrigRect.right, g_Daemon.dragOrigRect.bottom);
                    else g_Daemon.selRect = { clampedMx, clampedMy, g_Daemon.dragOrigRect.right, g_Daemon.dragOrigRect.bottom };
                    break;
                case DragMode::ResizeT:
                    g_Daemon.hasCustomHudPos = false;
                    if (selShiftHeld) {
                        int anchorX = g_Daemon.dragOrigRect.left, anchorY = g_Daemon.dragOrigRect.bottom;
                        int sdy = clampedMy - anchorY;
                        int side = std::min(std::abs(sdy), std::min(g_Daemon.vScreenW - anchorX, sdy >= 0 ? (g_Daemon.vScreenH - anchorY) : anchorY));
                        int ty = anchorY + (sdy >= 0 ? side : -side);
                        g_Daemon.selRect = { anchorX, std::min(anchorY, ty), anchorX + side, std::max(anchorY, ty) };
                    } else {
                        g_Daemon.selRect = { g_Daemon.dragOrigRect.left, clampedMy, g_Daemon.dragOrigRect.right, g_Daemon.dragOrigRect.bottom };
                    }
                    break;
                case DragMode::ResizeTR:
                    g_Daemon.hasCustomHudPos = false;
                    if (selShiftHeld) applySquareCorner(g_Daemon.dragOrigRect.left, g_Daemon.dragOrigRect.bottom);
                    else g_Daemon.selRect = { g_Daemon.dragOrigRect.left, clampedMy, clampedMx, g_Daemon.dragOrigRect.bottom };
                    break;
                case DragMode::ResizeR:
                    g_Daemon.hasCustomHudPos = false;
                    if (selShiftHeld) {
                        int anchorX = g_Daemon.dragOrigRect.left, anchorY = g_Daemon.dragOrigRect.top;
                        int sdx = clampedMx - anchorX;
                        int side = std::min(std::abs(sdx), std::min(g_Daemon.vScreenH - anchorY, sdx >= 0 ? (g_Daemon.vScreenW - anchorX) : anchorX));
                        int tx = anchorX + (sdx >= 0 ? side : -side);
                        g_Daemon.selRect = { std::min(anchorX, tx), anchorY, std::max(anchorX, tx), anchorY + side };
                    } else {
                        g_Daemon.selRect = { g_Daemon.dragOrigRect.left, g_Daemon.dragOrigRect.top, clampedMx, g_Daemon.dragOrigRect.bottom };
                    }
                    break;
                case DragMode::ResizeBR:
                    g_Daemon.hasCustomHudPos = false;
                    if (selShiftHeld) applySquareCorner(g_Daemon.dragOrigRect.left, g_Daemon.dragOrigRect.top);
                    else g_Daemon.selRect = { g_Daemon.dragOrigRect.left, g_Daemon.dragOrigRect.top, clampedMx, clampedMy };
                    break;
                case DragMode::ResizeB:
                    g_Daemon.hasCustomHudPos = false;
                    if (selShiftHeld) {
                        int anchorX = g_Daemon.dragOrigRect.left, anchorY = g_Daemon.dragOrigRect.top;
                        int sdy = clampedMy - anchorY;
                        int side = std::min(std::abs(sdy), std::min(g_Daemon.vScreenW - anchorX, sdy >= 0 ? (g_Daemon.vScreenH - anchorY) : anchorY));
                        int ty = anchorY + (sdy >= 0 ? side : -side);
                        g_Daemon.selRect = { anchorX, std::min(anchorY, ty), anchorX + side, std::max(anchorY, ty) };
                    } else {
                        g_Daemon.selRect = { g_Daemon.dragOrigRect.left, g_Daemon.dragOrigRect.top, g_Daemon.dragOrigRect.right, clampedMy };
                    }
                    break;
                case DragMode::ResizeBL:
                    g_Daemon.hasCustomHudPos = false;
                    if (selShiftHeld) applySquareCorner(g_Daemon.dragOrigRect.right, g_Daemon.dragOrigRect.top);
                    else g_Daemon.selRect = { clampedMx, g_Daemon.dragOrigRect.top, g_Daemon.dragOrigRect.right, clampedMy };
                    break;
                case DragMode::ResizeL:
                    g_Daemon.hasCustomHudPos = false;
                    if (selShiftHeld) {
                        int anchorX = g_Daemon.dragOrigRect.right, anchorY = g_Daemon.dragOrigRect.top;
                        int sdx = clampedMx - anchorX;
                        int side = std::min(std::abs(sdx), std::min(g_Daemon.vScreenH - anchorY, sdx >= 0 ? (g_Daemon.vScreenW - anchorX) : anchorX));
                        int tx = anchorX + (sdx >= 0 ? side : -side);
                        g_Daemon.selRect = { std::min(anchorX, tx), anchorY, std::max(anchorX, tx), anchorY + side };
                    } else {
                        g_Daemon.selRect = { clampedMx, g_Daemon.dragOrigRect.top, g_Daemon.dragOrigRect.right, g_Daemon.dragOrigRect.bottom };
                    }
                    break;
                case DragMode::DrawingAnnotation: {
                    bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    float targetX = (float)mx;
                    float targetY = (float)my;
                    OverlayTool t = g_Daemon.draftAnn.tool;

                    if (t == OverlayTool::Pen || t == OverlayTool::Highlighter) {
                        // 4. Snapping in Pen & Highlighter uses the LAST point (not the first point) as anchor!
                        if (shiftHeld) {
                            if (!g_Daemon.isPenShiftSnapping) {
                                g_Daemon.penSnapAnchor = g_Daemon.draftAnn.points.empty()
                                    ? g_Daemon.draftAnn.startPt
                                    : g_Daemon.draftAnn.points.back();
                                g_Daemon.isPenShiftSnapping = true;
                                g_Daemon.draftAnn.points.push_back(g_Daemon.penSnapAnchor);
                            }
                            float sdx = targetX - g_Daemon.penSnapAnchor.x;
                            float sdy = targetY - g_Daemon.penSnapAnchor.y;
                            float dist = std::hypot(sdx, sdy);
                            float angle = std::atan2(sdy, sdx);
                            const float step15 = 3.1415926535f / 12.0f; // 15 degrees in radians
                            float snappedAngle = std::round(angle / step15) * step15;
                            targetX = g_Daemon.penSnapAnchor.x + dist * std::cos(snappedAngle);
                            targetY = g_Daemon.penSnapAnchor.y + dist * std::sin(snappedAngle);
                            g_Daemon.draftAnn.points.back() = { targetX, targetY };
                        } else {
                            g_Daemon.isPenShiftSnapping = false;
                            auto recordFreehandPt = [&](float rx, float ry) {
                                if (g_Daemon.draftAnn.points.empty()) {
                                    g_Daemon.draftAnn.points.push_back({ rx, ry });
                                    g_Daemon.penSnapAnchor = { rx, ry };
                                } else {
                                    const auto& prev = g_Daemon.draftAnn.points.back();
                                    float pdx = rx - prev.x, pdy = ry - prev.y;
                                    if (!g_Daemon.penSmoothingEnabled || g_Daemon.penSmoothingStrength <= 0) {
                                        if (pdx * pdx + pdy * pdy >= 2.25f) {
                                            g_Daemon.draftAnn.points.push_back({ rx, ry });
                                            g_Daemon.penSnapAnchor = { rx, ry };
                                        }
                                    } else {
                                        float sNorm = std::max(0.05f, std::min(1.0f, g_Daemon.penSmoothingStrength / 100.0f));
                                        float minDist = 1.6f + 2.8f * sNorm;
                                        if (pdx * pdx + pdy * pdy >= minDist * minDist) {
                                            float prevW = 0.08f + 0.44f * sNorm;
                                            float curW  = 1.0f - prevW;
                                            PointF2D smoothed = {
                                                prev.x * prevW + rx * curW,
                                                prev.y * prevW + ry * curW
                                            };
                                            g_Daemon.draftAnn.points.push_back(smoothed);
                                            g_Daemon.penSnapAnchor = smoothed;
                                        }
                                    }
                                }
                            };
                            recordFreehandPt(targetX, targetY);
                            // Also drain any queued WM_MOUSEMOVE points into the freehand curve before repainting once
                            MSG nextMove;
                            while (PeekMessageW(&nextMove, hWnd, WM_MOUSEMOVE, WM_MOUSEMOVE, PM_REMOVE)) {
                                int nmx = GET_X_LPARAM(nextMove.lParam);
                                int nmy = GET_Y_LPARAM(nextMove.lParam);
                                g_Daemon.mousePt = { nmx, nmy };
                                float ntx = (float)nmx;
                                float nty = (float)nmy;
                                recordFreehandPt(ntx, nty);
                                targetX = ntx;
                                targetY = nty;
                            }
                        }
                        g_Daemon.draftAnn.endPt = { targetX, targetY };
                        break;
                    }

                    if (shiftHeld) {
                        float sdx = targetX - g_Daemon.draftAnn.startPt.x;
                        float sdy = targetY - g_Daemon.draftAnn.startPt.y;
                        if (t == OverlayTool::Line || t == OverlayTool::Arrow || t == OverlayTool::NumberArrow) {
                            // 7. 15-degree angle snapping for Line, Arrow, and Numbering Arrow on Shift-drag
                            float dist = std::hypot(sdx, sdy);
                            float angle = std::atan2(sdy, sdx);
                            const float step15 = 3.1415926535f / 12.0f; // 15 degrees in radians
                            float snappedAngle = std::round(angle / step15) * step15;
                            targetX = g_Daemon.draftAnn.startPt.x + dist * std::cos(snappedAngle);
                            targetY = g_Daemon.draftAnn.startPt.y + dist * std::sin(snappedAngle);
                        } else if (t == OverlayTool::Rectangle || t == OverlayTool::Ellipse ||
                                   t == OverlayTool::MosaicSquare || t == OverlayTool::MosaicCircle) {
                            // 8. 1:1 aspect ratio snapping for Rectangle, Circle, Mosaic Rectangle, and Mosaic Circle on Shift-drag
                            float side = std::max(std::abs(sdx), std::abs(sdy));
                            targetX = g_Daemon.draftAnn.startPt.x + (sdx >= 0.0f ? side : -side);
                            targetY = g_Daemon.draftAnn.startPt.y + (sdy >= 0.0f ? side : -side);
                        }
                    }

                    g_Daemon.draftAnn.endPt = { targetX, targetY };
                    break;
                }
                case DragMode::MovingAnnotation:
                case DragMode::DraggingAnnotationStart:
                case DragMode::DraggingAnnotationEnd: {
                    bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    for (auto& a : g_Daemon.annotations) {
                        if (a.id == g_Daemon.selectedAnnotationId) {
                            if (g_Daemon.dragMode == DragMode::DraggingAnnotationStart ||
                                g_Daemon.dragMode == DragMode::DraggingAnnotationEnd) {
                                PointF2D anchor = (g_Daemon.dragMode == DragMode::DraggingAnnotationStart) ? a.endPt : a.startPt;
                                float tx = (float)mx, ty = (float)my;
                                if (shiftHeld) {
                                    float sdx = tx - anchor.x, sdy = ty - anchor.y;
                                    if (a.tool == OverlayTool::Line || a.tool == OverlayTool::Arrow || a.tool == OverlayTool::NumberArrow) {
                                        float dist = std::hypot(sdx, sdy);
                                        const float step15 = 3.1415926535f / 12.0f;
                                        float snappedAngle = std::round(std::atan2(sdy, sdx) / step15) * step15;
                                        tx = anchor.x + dist * std::cos(snappedAngle);
                                        ty = anchor.y + dist * std::sin(snappedAngle);
                                    } else if (a.tool == OverlayTool::Rectangle || a.tool == OverlayTool::Ellipse ||
                                               a.tool == OverlayTool::MosaicSquare || a.tool == OverlayTool::MosaicCircle) {
                                        float side = std::max(std::abs(sdx), std::abs(sdy));
                                        tx = anchor.x + (sdx >= 0.0f ? side : -side);
                                        ty = anchor.y + (sdy >= 0.0f ? side : -side);
                                    }
                                }
                                if (g_Daemon.dragMode == DragMode::DraggingAnnotationStart) a.startPt = { tx, ty };
                                else a.endPt = { tx, ty };
                            } else {
                                float adx = ((float)mx - g_Daemon.moveAnnOffset.x) - a.startPt.x;
                                float ady = ((float)my - g_Daemon.moveAnnOffset.y) - a.startPt.y;
                                a.startPt.x += adx;
                                a.startPt.y += ady;
                                a.endPt.x   += adx;
                                a.endPt.y   += ady;
                                for (auto& p : a.points) { p.x += adx; p.y += ady; }
                            }
                            break;
                        }
                    }
                    break;
                }
                default:
                    break;
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_LBUTTONUP: {
            if (g_Daemon.isDraggingStrokeText || g_Daemon.isDraggingSizeText || g_Daemon.isDraggingTextBoxSel) {
                g_Daemon.isDraggingStrokeText = false;
                g_Daemon.isDraggingSizeText = false;
                g_Daemon.isDraggingTextBoxSel = false;
                ReleaseCapture();
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.dragMode != DragMode::None) {
                if (g_Daemon.dragMode == DragMode::DrawingAnnotation) {
                    g_Daemon.PushUndo();
                    if ((g_Daemon.draftAnn.tool == OverlayTool::Pen || g_Daemon.draftAnn.tool == OverlayTool::Highlighter) &&
                        !g_Daemon.draftAnn.points.empty()) {
                        float minX = g_Daemon.draftAnn.points[0].x, maxX = minX;
                        float minY = g_Daemon.draftAnn.points[0].y, maxY = minY;
                        for (const auto& p : g_Daemon.draftAnn.points) {
                            minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
                            minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
                        }
                        g_Daemon.draftAnn.startPt = { minX, minY };
                        g_Daemon.draftAnn.endPt   = { maxX, maxY };
                    }
                    g_Daemon.isPenShiftSnapping = false;
                    if (g_Daemon.draftAnn.tool == OverlayTool::NumberArrow) {
                        g_Daemon.nextStepNum++;
                    }
                    g_Daemon.annotations.push_back(g_Daemon.draftAnn);
                } else if (g_Daemon.dragMode == DragMode::CreatingSelection) {
                    int w = std::abs(g_Daemon.selRect.right - g_Daemon.selRect.left);
                    int h = std::abs(g_Daemon.selRect.bottom - g_Daemon.selRect.top);
                    if (w < 1 || h < 1) g_Daemon.hasSelection = false;
                }
                g_Daemon.dragMode = DragMode::None;
                ReleaseCapture();
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_CHAR: {
            if (g_Daemon.isEditingStroke) {
                wchar_t ch = (wchar_t)wParam;
                if (ch == L'\r') {
                    g_Daemon.CommitActiveStrokeInput();
                } else if ((ch >= L'0' && ch <= L'9') || ch == L'.') {
                    PepperSnapDaemon::ApplyInlineEditChar(
                        ch, g_Daemon.editingStrokeText, g_Daemon.strokeCaretPos, g_Daemon.strokeSelAnchor, 5
                    );
                }
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.isEditingSize) {
                wchar_t ch = (wchar_t)wParam;
                if (ch == L'\r') {
                    g_Daemon.CommitActiveSizeInput();
                } else if ((ch >= L'0' && ch <= L'9') || ch == L'x' || ch == L'X' || ch == L'*' || ch == L',' || ch == L' ') {
                    PepperSnapDaemon::ApplyInlineEditChar(
                        ch == L'X' ? L'x' : ch,
                        g_Daemon.editingSizeText, g_Daemon.sizeCaretPos, g_Daemon.sizeSelAnchor, 15
                    );
                }
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.isEditingText) {
                wchar_t ch = (wchar_t)wParam;
                if (ch == L'\r') {
                    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
                        PepperSnapDaemon::ApplyInlineEditChar(
                            L'\n', g_Daemon.editingTextAnn.text, g_Daemon.textCaretPos, g_Daemon.textSelAnchor, 4096
                        );
                    } else {
                        g_Daemon.CommitActiveTextBox();
                        g_Daemon.activeTool = OverlayTool::SelectMove;
                        g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                    }
                } else if (ch >= 32) {
                    PepperSnapDaemon::ApplyInlineEditChar(
                        ch, g_Daemon.editingTextAnn.text, g_Daemon.textCaretPos, g_Daemon.textSelAnchor, 4096
                    );
                }
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            break;
        }

        case WM_KEYDOWN: {
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

            if (g_Daemon.isEditingStroke) {
                if (wParam == VK_ESCAPE) {
                    g_Daemon.isEditingStroke = false;
                    g_Daemon.isDraggingStrokeText = false;
                    g_Daemon.editingStrokeText.clear();
                    g_Daemon.strokeCaretPos = g_Daemon.strokeSelAnchor = 0;
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                if (wParam == VK_RETURN) {
                    g_Daemon.CommitActiveStrokeInput();
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                PepperSnapDaemon::ApplyInlineEditKeyDown(
                    wParam, ctrl, shift,
                    g_Daemon.editingStrokeText, g_Daemon.strokeCaretPos, g_Daemon.strokeSelAnchor,
                    5, false, hWnd
                );
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }

            if (g_Daemon.isEditingSize) {
                if (wParam == VK_ESCAPE) {
                    g_Daemon.isEditingSize = false;
                    g_Daemon.isDraggingSizeText = false;
                    g_Daemon.editingSizeText.clear();
                    g_Daemon.sizeCaretPos = g_Daemon.sizeSelAnchor = 0;
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                if (wParam == VK_RETURN) {
                    g_Daemon.CommitActiveSizeInput();
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                PepperSnapDaemon::ApplyInlineEditKeyDown(
                    wParam, ctrl, shift,
                    g_Daemon.editingSizeText, g_Daemon.sizeCaretPos, g_Daemon.sizeSelAnchor,
                    15, true, hWnd
                );
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }

            if (g_Daemon.isEditingText) {
                if (wParam == VK_ESCAPE) {
                    g_Daemon.CommitActiveTextBox();
                    g_Daemon.activeTool = OverlayTool::SelectMove;
                    g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                if (wParam == VK_RETURN && !shift) {
                    g_Daemon.CommitActiveTextBox();
                    g_Daemon.activeTool = OverlayTool::SelectMove;
                    g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                g_Daemon.ApplyTextBoxKeyDown(wParam, ctrl, shift, hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }

            // 9. Pressing Esc when using any tool other than 'Select Mode' switches to 'Select Mode';
            //    only exit overlay when 'Select Mode' is already selected!
            if (wParam == VK_ESCAPE) {
                if (g_Daemon.activeTool != OverlayTool::SelectMove) {
                    g_Daemon.activeTool = OverlayTool::SelectMove;
                    g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                g_Daemon.CloseRegionSnipOverlay();
                return 0;
            }
            if ((wParam == VK_DELETE || wParam == VK_BACK) && g_Daemon.selectedAnnotationId != -1) {
                g_Daemon.PushUndo();
                g_Daemon.annotations.erase(
                    std::remove_if(g_Daemon.annotations.begin(), g_Daemon.annotations.end(),
                                   [](const Annotation& a) { return a.id == g_Daemon.selectedAnnotationId; }),
                    g_Daemon.annotations.end()
                );
                g_Daemon.RecalcNextStepNum();
                g_Daemon.selectedAnnotationId = -1;
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (ctrl) {
                if (wParam == 'C') {
                    if (!g_Daemon.hasSelection ||
                        g_Daemon.dragMode == DragMode::CreatingSelection ||
                        g_Daemon.dragMode == DragMode::PendingOutsideSelection) {
                        g_Daemon.ActionCopyPixelColorAndClose();
                    } else {
                        g_Daemon.ActionCopyAndClose();
                    }
                    return 0;
                }
                if (wParam == 'S') { g_Daemon.ActionQuickSaveAndClose(); return 0; }
                if (wParam == 'Z') { g_Daemon.Undo(); return 0; }
                if (wParam == 'Y') { g_Daemon.Redo(); return 0; }
            }
            switch (wParam) {
                case 'V': g_Daemon.activeTool = OverlayTool::SelectMove; break;
                case 'P': g_Daemon.activeTool = OverlayTool::Pen; break;
                case 'H': g_Daemon.activeTool = OverlayTool::Highlighter; break;
                case 'L': g_Daemon.activeTool = OverlayTool::Line; break;
                case 'A': g_Daemon.activeTool = OverlayTool::Arrow; break;
                case 'N': g_Daemon.activeTool = OverlayTool::NumberArrow; break;
                case 'R': g_Daemon.activeTool = OverlayTool::Rectangle; break;
                case 'E': g_Daemon.activeTool = OverlayTool::Ellipse; break;
                case 'T': g_Daemon.activeTool = OverlayTool::TextBox; break;
                case 'X': g_Daemon.activeTool = OverlayTool::MosaicSquare; break;
                case 'M': g_Daemon.activeTool = OverlayTool::MosaicCircle; break;
                case 'F': g_Daemon.ActionPinToDesktop(); return 0;
                default: break;
            }
            g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hWnd, 1);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void PepperSnapDaemon::StartRegionSnipOverlay(Bitmap* customBmp) {
    if (hOverlayWnd) {
        SetForegroundWindow(hOverlayWnd);
        return;
    }
    if (frozenDesktopBmp) {
        delete frozenDesktopBmp;
        frozenDesktopBmp = nullptr;
    }

    if (customBmp) {
        vScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
        vScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
        vScreenW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        vScreenH = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        frozenDesktopBmp = customBmp;
        hasSelection = true;
        int cw = std::min(vScreenW - 80, (int)customBmp->GetWidth());
        int ch = std::min(vScreenH - 80, (int)customBmp->GetHeight());
        selRect = { 40, 40, 40 + cw, 40 + ch };
    } else {
        frozenDesktopBmp = CaptureVirtualDesktop();
        hasSelection = false;
    }
    if (!frozenDesktopBmp) return;
    BuildOverlaySurfaceCache();

    dragMode = DragMode::None;
    hasCustomHudPos = false;
    lastPillHover = false;
    lastStrokeHover = false;
    activeTool = OverlayTool::SelectMove;
    isEditingText = false;
    isEditingSize = false;
    isDraggingSizeText = false;
    editingSizeText.clear();
    sizeCaretPos = sizeSelAnchor = 0;
    isEditingStroke = false;
    isDraggingStrokeText = false;
    editingStrokeText.clear();
    strokeCaretPos = strokeSelAnchor = 0;
    selectedAnnotationId = -1;
    annotations.clear();
    undoStack.clear();
    redoStack.clear();
    nextStepNum = 1;
    GetCursorPos(&mousePt);
    mousePt.x -= vScreenX;
    mousePt.y -= vScreenY;

    hOverlayWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"PepperSnapOverlayWnd", L"PepperSnap Region Snip",
        WS_POPUP | WS_VISIBLE,
        vScreenX, vScreenY, vScreenW, vScreenH,
        nullptr, nullptr, hInst, nullptr
    );
    UpdateOverlayCursor(mousePt.x, mousePt.y);
    HWND hFore = GetForegroundWindow();
    DWORD foreThread = hFore ? GetWindowThreadProcessId(hFore, nullptr) : 0;
    DWORD curThread = GetCurrentThreadId();
    if (foreThread && foreThread != curThread) {
        AttachThreadInput(foreThread, curThread, TRUE);
        BringWindowToTop(hOverlayWnd);
        SetForegroundWindow(hOverlayWnd);
        SetFocus(hOverlayWnd);
        AttachThreadInput(foreThread, curThread, FALSE);
    } else {
        SetForegroundWindow(hOverlayWnd);
        SetFocus(hOverlayWnd);
    }
}

void PepperSnapDaemon::CloseRegionSnipOverlay() {
    hasCustomHudPos = false;
    activeTool = OverlayTool::SelectMove;
    isEditingText = false;
    isEditingSize = false;
    isDraggingSizeText = false;
    editingSizeText.clear();
    sizeCaretPos = sizeSelAnchor = 0;
    isEditingStroke = false;
    isDraggingStrokeText = false;
    editingStrokeText.clear();
    strokeCaretPos = strokeSelAnchor = 0;
    if (hOverlayWnd) {
        DestroyWindow(hOverlayWnd);
        hOverlayWnd = nullptr;
    }
    FreeOverlaySurfaceCache();
    if (frozenDesktopBmp) {
        delete frozenDesktopBmp;
        frozenDesktopBmp = nullptr;
    }
    hasSelection = false;
    dragMode = DragMode::None;
}

void PepperSnapDaemon::OpenImageIntoOverlay() {
    WCHAR szFile[MAX_PATH] = {0};
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hTrayWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Image Files (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        Bitmap loaded(szFile);
        if (loaded.GetLastStatus() == Ok) {
            int sw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
            int sh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
            Bitmap* canvasBmp = new Bitmap(sw, sh, PixelFormat32bppARGB);
            Graphics g(canvasBmp);
            SolidBrush bg(Color(255, 15, 23, 42));
            g.FillRectangle(&bg, 0, 0, sw, sh);
            g.DrawImage(&loaded, 40, 40, (int)loaded.GetWidth(), (int)loaded.GetHeight());
            StartRegionSnipOverlay(canvasBmp);
        }
    }
}

// ----------------------------------------------------------------------------
// System Tray Daemon Window Procedure
// ----------------------------------------------------------------------------

static void ShowTrayContextMenu(HWND hWnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();

    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_REGION,      L"Capture Region\tCtrl + PrintScreen");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_FULL,        L"Instant Fullscreen\tShift + PrintScreen");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_DELAY,       L"Delayed Fullscreen (3s Timer)");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN_IMAGE,  L"Open Image File to Annotate...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN_FOLDER,   L"Open Captures Folder");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPTIONS,       L"Options (Save Folder, Default Formats & Naming)...");
    AppendMenuW(hMenu, MF_STRING | (g_Daemon.autoSaveOnCopy ? MF_CHECKED : MF_UNCHECKED),
                IDM_TRAY_AUTOSAVE_COPY, L"Auto-Save on Region Copy");
    AppendMenuW(hMenu, MF_STRING | (g_Daemon.IsRunAtStartupEnabled() ? MF_CHECKED : MF_UNCHECKED),
                IDM_TRAY_STARTUP_RUN, L"Launch PepperSnap at Windows Startup");
    if (!g_Daemon.pinnedWindows.empty()) {
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_CLOSE_PINS, L"Close All Pinned Captures");
    }
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SHORTCUTS, L"Keyboard Shortcuts & Help...");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT,      L"Exit PepperSnap");

    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
    PostMessageW(hWnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

static LRESULT CALLBACK TrayDaemonWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TRIGGER_REGION_SNIP:
            g_Daemon.StartRegionSnipOverlay();
            return 0;
        case WM_TRIGGER_FULL_SNAP:
            g_Daemon.InstantFullscreenCapture(false);
            return 0;
        case WM_TRIGGER_DELAY_SNAP:
            g_Daemon.InstantFullscreenCapture(true);
            return 0;
        case WM_HOTKEY:
            if (wParam == HK_CTRL_PRTSCN || wParam == HK_FALLBACK_REGION) g_Daemon.StartRegionSnipOverlay();
            else if (wParam == HK_SHIFT_PRTSCN || wParam == HK_FALLBACK_FULL) g_Daemon.InstantFullscreenCapture(false);
            return 0;
        case WM_TRAYICON:
            if (lParam == WM_LBUTTONUP) g_Daemon.StartRegionSnipOverlay();
            else if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) ShowTrayContextMenu(hWnd);
            else if (lParam == NIN_BALLOONUSERCLICK) {
                if (!g_Daemon.lastSavedFilePath.empty()) {
                    std::wstring param = L"/select,\"" + g_Daemon.lastSavedFilePath + L"\"";
                    ShellExecuteW(nullptr, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);
                } else {
                    ShellExecuteW(nullptr, L"open", g_Daemon.saveFolder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }
            }
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_TRAY_REGION:         g_Daemon.StartRegionSnipOverlay(); break;
                case IDM_TRAY_FULL:           g_Daemon.InstantFullscreenCapture(false); break;
                case IDM_TRAY_DELAY:          g_Daemon.InstantFullscreenCapture(true); break;
                case IDM_TRAY_OPEN_IMAGE:     g_Daemon.OpenImageIntoOverlay(); break;
                case IDM_TRAY_OPEN_FOLDER:
                    CreateDirectoryW(g_Daemon.saveFolder.c_str(), nullptr);
                    ShellExecuteW(nullptr, L"open", g_Daemon.saveFolder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    break;
                case IDM_TRAY_OPTIONS:        g_Daemon.ShowOptionsModal(); break;
                case IDM_TRAY_AUTOSAVE_COPY:
                    g_Daemon.autoSaveOnCopy = !g_Daemon.autoSaveOnCopy;
                    g_Daemon.SaveSettings();
                    break;
                case IDM_TRAY_STARTUP_RUN:    g_Daemon.ToggleRunAtStartup(); break;
                case IDM_TRAY_CLOSE_PINS:
                    for (HWND hp : g_Daemon.pinnedWindows) if (IsWindow(hp)) DestroyWindow(hp);
                    g_Daemon.pinnedWindows.clear();
                    break;
                case IDM_TRAY_SHORTCUTS:      g_Daemon.ShowShortcutsModal(); break;
                case IDM_TRAY_EXIT:
                    g_Daemon.SaveSettings();
                    g_Daemon.CloseRegionSnipOverlay();
                    g_Daemon.RemoveTrayIcon();
                    PostQuitMessage(0);
                    break;
            }
            return 0;
        case WM_DESTROY:
            g_Daemon.RemoveTrayIcon();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR lpCmdLine, int) {
    SetProcessDPIAware();

    std::wstring cmdArgs = lpCmdLine ? lpCmdLine : L"";
    for (wchar_t& c : cmdArgs) c = (wchar_t)towlower(c);

    UINT cliMsg = 0;
    WPARAM cliWParam = 0;
    if (cmdArgs.find(L"--fullscreen") != std::wstring::npos || cmdArgs.find(L"/fullscreen") != std::wstring::npos) {
        cliMsg = WM_TRIGGER_FULL_SNAP;
    } else if (cmdArgs.find(L"--delay") != std::wstring::npos || cmdArgs.find(L"/delay") != std::wstring::npos) {
        cliMsg = WM_TRIGGER_DELAY_SNAP;
    } else if (cmdArgs.find(L"--options") != std::wstring::npos || cmdArgs.find(L"/options") != std::wstring::npos) {
        cliMsg = WM_COMMAND;
        cliWParam = IDM_TRAY_OPTIONS;
    } else if (cmdArgs.find(L"--open") != std::wstring::npos || cmdArgs.find(L"/open") != std::wstring::npos) {
        cliMsg = WM_COMMAND;
        cliWParam = IDM_TRAY_OPEN_IMAGE;
    } else if (cmdArgs.find(L"--help") != std::wstring::npos || cmdArgs.find(L"/help") != std::wstring::npos || cmdArgs.find(L"/?") != std::wstring::npos) {
        cliMsg = WM_COMMAND;
        cliWParam = IDM_TRAY_SHORTCUTS;
    } else if (cmdArgs.find(L"--exit") != std::wstring::npos || cmdArgs.find(L"/exit") != std::wstring::npos || cmdArgs.find(L"--quit") != std::wstring::npos) {
        cliMsg = WM_COMMAND;
        cliWParam = IDM_TRAY_EXIT;
    } else if (cmdArgs.find(L"--region") != std::wstring::npos || cmdArgs.find(L"/region") != std::wstring::npos) {
        cliMsg = WM_TRIGGER_REGION_SNIP;
    }

    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Global\\PepperSnap_TrayDaemon_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExisting = FindWindowW(L"PepperSnapTrayDaemonClass", nullptr);
        if (hExisting) {
            if (cliMsg != 0) {
                PostMessageW(hExisting, cliMsg, cliWParam, 0);
            } else {
                PostMessageW(hExisting, WM_TRIGGER_REGION_SNIP, 0, 0);
            }
        }
        return 0;
    }
    if (cliMsg == WM_COMMAND && cliWParam == IDM_TRAY_EXIT) {
        if (hMutex) CloseHandle(hMutex);
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
    wcPin.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcPin.lpfnWndProc = PinWndProc;
    wcPin.hInstance = hInstance;
    wcPin.hIcon = hAppIcon;
    wcPin.hIconSm = hAppIcon;
    wcPin.hCursor = LoadCursorW(nullptr, IDC_SIZEALL);
    wcPin.lpszClassName = L"PepperSnapPinWnd";
    RegisterClassExW(&wcPin);

    g_Daemon.hTrayWnd = CreateWindowExW(
        0, L"PepperSnapTrayDaemonClass", L"PepperSnap",
        0, 0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr
    );

    g_Daemon.InitTrayIcon();

    RegisterHotKey(g_Daemon.hTrayWnd, HK_CTRL_PRTSCN,     MOD_CONTROL, VK_SNAPSHOT);
    RegisterHotKey(g_Daemon.hTrayWnd, HK_SHIFT_PRTSCN,    MOD_SHIFT,   VK_SNAPSHOT);
    RegisterHotKey(g_Daemon.hTrayWnd, HK_FALLBACK_REGION, MOD_CONTROL | MOD_SHIFT, 'S');
    RegisterHotKey(g_Daemon.hTrayWnd, HK_FALLBACK_FULL,   MOD_CONTROL | MOD_SHIFT, 'F');

    g_Daemon.hKeyHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);

    if (cliMsg != 0) {
        PostMessageW(g_Daemon.hTrayWnd, cliMsg, cliWParam, 0);
    } else {
        g_Daemon.ShowTrayToast(
            L"PepperSnap v3.0.0.2 Active in System Tray",
            L"• Ctrl + PrintScreen: Region Snip & Annotate\n"
            L"• Shift + PrintScreen: Instant Fullscreen Capture"
        );
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_Daemon.hKeyHook) UnhookWindowsHookEx(g_Daemon.hKeyHook);
    UnregisterHotKey(g_Daemon.hTrayWnd, HK_CTRL_PRTSCN);
    UnregisterHotKey(g_Daemon.hTrayWnd, HK_SHIFT_PRTSCN);
    UnregisterHotKey(g_Daemon.hTrayWnd, HK_FALLBACK_REGION);
    UnregisterHotKey(g_Daemon.hTrayWnd, HK_FALLBACK_FULL);

    g_Daemon.DestroyCustomCursors();
    GdiplusShutdown(g_Daemon.gdiplusToken);
    CoUninitialize();
    if (hMutex) CloseHandle(hMutex);
    return (int)msg.wParam;
}

int main() {
    return wWinMain(GetModuleHandleW(nullptr), nullptr, GetCommandLineW(), SW_SHOWDEFAULT);
}
