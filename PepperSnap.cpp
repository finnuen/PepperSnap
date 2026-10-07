#include "PepperSnap.h"

class PepperSnapDaemon {
public:
    HINSTANCE hInst = nullptr;
    HWND hTrayWnd = nullptr;
    HWND hOverlayWnd = nullptr;
    HWND hOptionsWnd = nullptr;
    HWND hShortcutsWnd = nullptr;
    HHOOK hKeyHook = nullptr;
    HICON hTrayIcon = nullptr;
    HCURSOR hCurPen = nullptr;
    HCURSOR hCurStabilo = nullptr;
    NOTIFYICONDATAW nid = {0};
    ULONG_PTR gdiplusToken = 0;
    std::vector<HWND> pinnedWindows;

    static constexpr const wchar_t* APP_VERSION = L"3.3.0.0";
    static constexpr const wchar_t* DEFAULT_GITHUB_REPO = L"finnuen/PepperSnap";

    // Configuration (Desktop default, JPEG default, unified Options modal)
    std::wstring saveFolder;
    std::wstring namingPattern = L"PepperSnap_{YYYY}-{MM}-{DD}_{HH}-{mm}-{ss}";
    static constexpr const wchar_t* DEFAULT_NAMING_PATTERN = L"PepperSnap_{YYYY}-{MM}-{DD}_{HH}-{mm}-{ss}";
    ImageFormat regionFormat = ImageFormat::JPEG;
    ImageFormat fullscreenFormat = ImageFormat::JPEG;
    ImageFormat copyFormat = ImageFormat::JPEG;
    int jpegQuality = 90;
    bool nonStackingHighlighter = true;
    bool penSmoothingEnabled = true;
    int penSmoothingStrength = 15; // 5% to 100% (default 15%)
    std::wstring lastSavedFilePath;
    std::wstring lastBalloonClickFolder;
    int captureCounter = 1;
    bool autoSaveOnCopy = true;
    bool alsoCopyFullscreen = true;
    bool alsoSavePinned = true;
    bool alsoSaveOcrText = true;
    bool autoCheckUpdates = true;
    UpdateCheckInterval updateInterval = UpdateCheckInterval::EveryDay;
    long long lastUpdateCheckTime = 0;
    std::wstring updateGithubRepo = DEFAULT_GITHUB_REPO;
    bool isCheckingUpdate = false;

    // Custom Global Hotkey Bindings (Defaults: Ctrl+PrtScn, Shift+PrtScn, Ctrl+Shift+PrtScn)
    HotkeyBinding hkRegionSnip{ MOD_CONTROL, VK_SNAPSHOT };
    HotkeyBinding hkFullSnap{ MOD_SHIFT, VK_SNAPSHOT };
    HotkeyBinding hkPrevRegion{ MOD_CONTROL | MOD_SHIFT, VK_SNAPSHOT };

    // Previously Selected Custom Window Size & Position
    bool hasLastCustomSel = false;
    RECT lastCustomSelRect{ 0, 0, 0, 0 };

    // Virtual Screen Metrics
    int vScreenX = 0;
    int vScreenY = 0;
    int vScreenW = 1920;
    int vScreenH = 1080;

    // Active Overlay Session State
    Bitmap* frozenDesktopBmp = nullptr;
    bool hasSelection = false;
    RECT selRect{0, 0, 0, 0};
    std::vector<RECT> desktopWindowRects;
    std::vector<HWND> suppressedAboveOverlayWindows;
    bool hasCtrlHoverWindow = false;
    RECT ctrlHoverWindowRect{0, 0, 0, 0};
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
    void UpdateTrayTooltip();
    void EnsureNotificationSoundEnabled();
    void ShowTrayToast(const std::wstring& title, const std::wstring& message, const std::wstring& clickFolder = L"");
    void RemoveTrayIcon();
    std::wstring FormatFilename(int seqNum) const;
    static std::wstring FormatFilenameWithPattern(const std::wstring& pattern, int seqNum);
    bool IsRunAtStartupEnabled() const;
    void ToggleRunAtStartup();
    void ApplyGlobalHotkeys();
    static std::wstring FormatHotkeyString(const HotkeyBinding& hk);
    static void RegisterImageContextMenu();

    Bitmap* CaptureVirtualDesktop();
    void BuildOverlaySurfaceCache();
    void FreeOverlaySurfaceCache();
    void InstantFullscreenCapture();
    void InstantPreviousRegionCapture();
    void RecordLastCustomSelection();
    void SnapshotDesktopWindows();
    void SuppressAboveOverlayWindows();
    void RestoreSuppressedAboveOverlayWindows();
    bool UpdateCtrlWindowHover();
    void StartRegionSnipOverlay(Bitmap* customBmp = nullptr, const RECT* customSelRect = nullptr);
    void CloseRegionSnipOverlay();
    void OpenImageIntoOverlay();
    void OpenImageFileIntoOverlay(const std::wstring& filePath);
    void OpenImageToPinOnTop();
    void OpenImageFileToPinOnTop(const std::wstring& filePath);
    HWND CreatePinnedWindow(Bitmap* bmp, int x, int y, float initialScale = 1.0f);

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
    void ActionOcrAndClose();
    static bool RecognizeBitmapTextNativeWinRT(Bitmap* srcBmp, std::wstring& outText);
    void PushUndo();
    void Undo();
    void Redo();

    void ShowOptionsModal();
    void ShowShortcutsModal();
    static long long GetUpdateIntervalSeconds(UpdateCheckInterval iv);
    static int CompareVersionStrings(const std::wstring& v1, const std::wstring& v2);
    void CheckForUpdatesAsync(bool manualUserTrigger);
    void MaybeRunScheduledUpdateCheck();

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
    WritePrivateProfileStringW(L"PepperSnap", L"AlsoCopyFullscreen", alsoCopyFullscreen ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"AlsoSavePinned", alsoSavePinned ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"AlsoSaveOcrText", alsoSaveOcrText ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"PenSmoothingEnabled", penSmoothingEnabled ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"PenSmoothingStrength", std::to_wstring(penSmoothingStrength).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"PenSmoothingDefaultV31", L"1", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"DefaultsV3107", L"1", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"AutoCheckUpdates", autoCheckUpdates ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"UpdateCheckInterval", std::to_wstring((int)updateInterval).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"LastUpdateCheckTime", std::to_wstring(lastUpdateCheckTime).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"UpdateGithubRepo", updateGithubRepo.c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"HkRegionMod", std::to_wstring(hkRegionSnip.modifiers).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"HkRegionVk", std::to_wstring(hkRegionSnip.vk).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"HkFullMod", std::to_wstring(hkFullSnap.modifiers).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"HkFullVk", std::to_wstring(hkFullSnap.vk).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"HkPrevMod", std::to_wstring(hkPrevRegion.modifiers).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"HkPrevVk", std::to_wstring(hkPrevRegion.vk).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"HasLastCustomSel", hasLastCustomSel ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"LastCustomSelLeft", std::to_wstring(lastCustomSelRect.left).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"LastCustomSelTop", std::to_wstring(lastCustomSelRect.top).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"LastCustomSelRight", std::to_wstring(lastCustomSelRect.right).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"LastCustomSelBottom", std::to_wstring(lastCustomSelRect.bottom).c_str(), iniPath.c_str());
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

    int q = (int)GetPrivateProfileIntW(L"PepperSnap", L"JpegQuality", 90, iniPath.c_str());
    jpegQuality = std::max(10, std::min(100, q));

    nonStackingHighlighter = (GetPrivateProfileIntW(L"PepperSnap", L"NonStackingHighlighter", 1, iniPath.c_str()) != 0);
    autoSaveOnCopy = (GetPrivateProfileIntW(L"PepperSnap", L"AutoSaveOnCopy", 1, iniPath.c_str()) != 0);
    alsoCopyFullscreen = (GetPrivateProfileIntW(L"PepperSnap", L"AlsoCopyFullscreen", 1, iniPath.c_str()) != 0);
    alsoSavePinned = (GetPrivateProfileIntW(L"PepperSnap", L"AlsoSavePinned", 1, iniPath.c_str()) != 0);
    alsoSaveOcrText = (GetPrivateProfileIntW(L"PepperSnap", L"AlsoSaveOcrText", 1, iniPath.c_str()) != 0);
    if (GetPrivateProfileIntW(L"PepperSnap", L"PenSmoothingDefaultV31", 0, iniPath.c_str()) == 0) {
        penSmoothingEnabled = true;
        penSmoothingStrength = 15;
        WritePrivateProfileStringW(L"PepperSnap", L"PenSmoothingEnabled", L"1", iniPath.c_str());
        WritePrivateProfileStringW(L"PepperSnap", L"PenSmoothingStrength", L"15", iniPath.c_str());
        WritePrivateProfileStringW(L"PepperSnap", L"PenSmoothingDefaultV31", L"1", iniPath.c_str());
    } else {
        penSmoothingEnabled = (GetPrivateProfileIntW(L"PepperSnap", L"PenSmoothingEnabled", 1, iniPath.c_str()) != 0);
        int pSmooth = (int)GetPrivateProfileIntW(L"PepperSnap", L"PenSmoothingStrength", 15, iniPath.c_str());
        penSmoothingStrength = std::max(5, std::min(100, pSmooth));
    }

    autoCheckUpdates = (GetPrivateProfileIntW(L"PepperSnap", L"AutoCheckUpdates", 1, iniPath.c_str()) != 0);
    if (GetPrivateProfileIntW(L"PepperSnap", L"DefaultsV3107", 0, iniPath.c_str()) == 0) {
        jpegQuality = 90;
        autoCheckUpdates = true;
        nonStackingHighlighter = true;
        autoSaveOnCopy = true;
        alsoCopyFullscreen = true;
        penSmoothingEnabled = true;
        WritePrivateProfileStringW(L"PepperSnap", L"JpegQuality", L"90", iniPath.c_str());
        WritePrivateProfileStringW(L"PepperSnap", L"AutoCheckUpdates", L"1", iniPath.c_str());
        WritePrivateProfileStringW(L"PepperSnap", L"NonStackingHighlighter", L"1", iniPath.c_str());
        WritePrivateProfileStringW(L"PepperSnap", L"AutoSaveOnCopy", L"1", iniPath.c_str());
        WritePrivateProfileStringW(L"PepperSnap", L"AlsoCopyFullscreen", L"1", iniPath.c_str());
        WritePrivateProfileStringW(L"PepperSnap", L"PenSmoothingEnabled", L"1", iniPath.c_str());
        WritePrivateProfileStringW(L"PepperSnap", L"DefaultsV3107", L"1", iniPath.c_str());
    }
    int uInt = (int)GetPrivateProfileIntW(L"PepperSnap", L"UpdateCheckInterval", (INT)UpdateCheckInterval::EveryDay, iniPath.c_str());
    if (uInt >= 0 && uInt <= 4) updateInterval = (UpdateCheckInterval)uInt;

    WCHAR timeBuf[64] = {0};
    GetPrivateProfileStringW(L"PepperSnap", L"LastUpdateCheckTime", L"0", timeBuf, 63, iniPath.c_str());
    lastUpdateCheckTime = _wtoi64(timeBuf);

    WCHAR repoBuf[256] = {0};
    GetPrivateProfileStringW(L"PepperSnap", L"UpdateGithubRepo", DEFAULT_GITHUB_REPO, repoBuf, 255, iniPath.c_str());
    if (wcslen(repoBuf) > 0) updateGithubRepo = repoBuf;

    hkRegionSnip.modifiers = (UINT)GetPrivateProfileIntW(L"PepperSnap", L"HkRegionMod", MOD_CONTROL, iniPath.c_str());
    hkRegionSnip.vk        = (UINT)GetPrivateProfileIntW(L"PepperSnap", L"HkRegionVk",  VK_SNAPSHOT, iniPath.c_str());
    hkFullSnap.modifiers   = (UINT)GetPrivateProfileIntW(L"PepperSnap", L"HkFullMod",   MOD_SHIFT,   iniPath.c_str());
    hkFullSnap.vk          = (UINT)GetPrivateProfileIntW(L"PepperSnap", L"HkFullVk",    VK_SNAPSHOT, iniPath.c_str());
    hkPrevRegion.modifiers = (UINT)GetPrivateProfileIntW(L"PepperSnap", L"HkPrevMod",   MOD_CONTROL | MOD_SHIFT, iniPath.c_str());
    hkPrevRegion.vk        = (UINT)GetPrivateProfileIntW(L"PepperSnap", L"HkPrevVk",    VK_SNAPSHOT, iniPath.c_str());

    hasLastCustomSel = (GetPrivateProfileIntW(L"PepperSnap", L"HasLastCustomSel", 0, iniPath.c_str()) != 0);
    lastCustomSelRect.left   = (LONG)GetPrivateProfileIntW(L"PepperSnap", L"LastCustomSelLeft",   0, iniPath.c_str());
    lastCustomSelRect.top    = (LONG)GetPrivateProfileIntW(L"PepperSnap", L"LastCustomSelTop",    0, iniPath.c_str());
    lastCustomSelRect.right  = (LONG)GetPrivateProfileIntW(L"PepperSnap", L"LastCustomSelRight",  0, iniPath.c_str());
    lastCustomSelRect.bottom = (LONG)GetPrivateProfileIntW(L"PepperSnap", L"LastCustomSelBottom", 0, iniPath.c_str());
    if (lastCustomSelRect.right <= lastCustomSelRect.left || lastCustomSelRect.bottom <= lastCustomSelRect.top) {
        hasLastCustomSel = false;
    }
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

void PepperSnapDaemon::UpdateTrayTooltip() {
    wcsncpy_s(nid.szTip, L"PepperSnap", _TRUNCATE);
    if (nid.hWnd) {
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        Shell_NotifyIconW(NIM_MODIFY, &nid);
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
    wcsncpy_s(nid.szTip, L"PepperSnap", _TRUNCATE);
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void PepperSnapDaemon::ShowTrayToast(const std::wstring& title, const std::wstring& message, const std::wstring& clickFolder) {
    lastBalloonClickFolder = clickFolder;
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
        if (!hasSelection && hasCtrlHoverWindow) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
        } else {
            SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        }
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
        ShowTrayToast(L"Startup disabled", L"PepperSnap will no longer launch automatically at login.");
    } else {
        WCHAR exePath[MAX_PATH] = {0};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring quoted = L"\"" + std::wstring(exePath) + L"\"";
        RegSetValueExW(hKey, L"PepperSnap", 0, REG_SZ, (const BYTE*)quoted.c_str(), (DWORD)((quoted.size() + 1) * sizeof(wchar_t)));
        ShowTrayToast(L"Startup enabled", L"PepperSnap will start in the system tray when Windows boots.");
    }
    RegCloseKey(hKey);
}

// ----------------------------------------------------------------------------
// Custom Hotkeys, Explorer Image Context Menu & Low-Level Keyboard Hook
// ----------------------------------------------------------------------------

static int g_RecordingHotkeySlot = 0; // 0 = None, 1 = Custom Window, 2 = Instant Fullscreen, 3 = Instant Save Previous Window
static DWORD g_LastHotkeyTriggerTick = 0;

std::wstring PepperSnapDaemon::FormatHotkeyString(const HotkeyBinding& hk) {
    if (hk.vk == 0) return L"None";
    std::wstring out;
    if (hk.modifiers & MOD_CONTROL) out += L"Ctrl + ";
    if (hk.modifiers & MOD_SHIFT)   out += L"Shift + ";
    if (hk.modifiers & MOD_ALT)     out += L"Alt + ";
    if (hk.modifiers & MOD_WIN)     out += L"Win + ";

    switch (hk.vk) {
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

void PepperSnapDaemon::ApplyGlobalHotkeys() {
    if (!hTrayWnd) return;
    UnregisterHotKey(hTrayWnd, HK_REGION_SNIP);
    UnregisterHotKey(hTrayWnd, HK_FULL_SNAP);
    UnregisterHotKey(hTrayWnd, HK_PREV_REGION);
    auto regHk = [&](int id, const HotkeyBinding& hk) {
        if (hk.vk == 0) return;
        if (!RegisterHotKey(hTrayWnd, id, hk.modifiers | 0x4000 /*MOD_NOREPEAT*/, hk.vk)) {
            RegisterHotKey(hTrayWnd, id, hk.modifiers, hk.vk);
        }
    };
    regHk(HK_REGION_SNIP, hkRegionSnip);
    regHk(HK_FULL_SNAP,   hkFullSnap);
    regHk(HK_PREV_REGION, hkPrevRegion);
    UpdateTrayTooltip();
}

void PepperSnapDaemon::RegisterImageContextMenu() {
    WCHAR exePath[MAX_PATH] = {0};
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) return;

    std::wstring label = L"Edit with PepperSnap";
    std::wstring iconVal = L"\"" + std::wstring(exePath) + L"\",0";
    std::wstring cmdVal = L"\"" + std::wstring(exePath) + L"\" --edit \"%1\"";

    auto writeShellVerb = [&](const std::wstring& baseKeyPath) {
        std::wstring verbKeyPath = baseKeyPath + L"\\shell\\PepperSnapEdit";
        HKEY hVerbKey = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, verbKeyPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hVerbKey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hVerbKey, nullptr, 0, REG_SZ, (const BYTE*)label.c_str(), (DWORD)((label.size() + 1) * sizeof(wchar_t)));
            RegSetValueExW(hVerbKey, L"Icon", 0, REG_SZ, (const BYTE*)iconVal.c_str(), (DWORD)((iconVal.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(hVerbKey);
        }
        std::wstring cmdKeyPath = verbKeyPath + L"\\command";
        HKEY hCmdKey = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, cmdKeyPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hCmdKey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hCmdKey, nullptr, 0, REG_SZ, (const BYTE*)cmdVal.c_str(), (DWORD)((cmdVal.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(hCmdKey);
        }
    };

    // Register ONLY for image files via SystemFileAssociations\image and common image extensions
    writeShellVerb(L"Software\\Classes\\SystemFileAssociations\\image");
    const wchar_t* imgExts[] = {
        L".png", L".jpg", L".jpeg", L".webp", L".bmp", L".gif", L".tif", L".tiff", L".ico"
    };
    for (const wchar_t* ext : imgExts) {
        writeShellVerb(std::wstring(L"Software\\Classes\\SystemFileAssociations\\") + ext);
    }
}

static bool IsModifierVk(DWORD vk) {
    return vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
           vk == VK_SHIFT   || vk == VK_LSHIFT   || vk == VK_RSHIFT   ||
           vk == VK_MENU    || vk == VK_LMENU    || vk == VK_RMENU    ||
           vk == VK_LWIN    || vk == VK_RWIN;
}

static UINT GetCurrentHardwareModifiers() {
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
        bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        bool isKeyUp   = (wParam == WM_KEYUP   || wParam == WM_SYSKEYUP);

        // Normalize SysRq (scanCode 0x54 when Shift/Alt + PrintScreen is pressed) to VK_SNAPSHOT
        DWORD vk = kb->vkCode;
        if (kb->scanCode == 0x54 || kb->scanCode == 0x37) {
            if (vk == VK_SNAPSHOT || vk == VK_EXECUTE || vk == 0xFF) {
                vk = VK_SNAPSHOT;
            }
        }

        // If the Options dialog is actively recording a custom shortcut key
        if (g_RecordingHotkeySlot != 0 && g_Daemon.hOptionsWnd && IsWindow(g_Daemon.hOptionsWnd)) {
            if (isKeyDown || (isKeyUp && vk == VK_SNAPSHOT)) {
                if (vk == VK_ESCAPE) {
                    PostMessageW(g_Daemon.hOptionsWnd, WM_SHORTCUT_RECORDED, 0, 0);
                    return 1;
                }
                if (!IsModifierVk(vk)) {
                    UINT mods = GetCurrentHardwareModifiers();
                    PostMessageW(g_Daemon.hOptionsWnd, WM_SHORTCUT_RECORDED, (WPARAM)mods, (LPARAM)vk);
                    return 1;
                }
            }
            return CallNextHookEx(g_Daemon.hKeyHook, nCode, wParam, lParam);
        }

        // If the Custom Area overlay is active (with no modal child window open), ensure Esc and overlay keys
        // reach hOverlayWnd immediately even if a higher-band shell flyout (Notification Sidebar) held focus
        if (g_Daemon.hOverlayWnd && IsWindow(g_Daemon.hOverlayWnd) &&
            (!g_Daemon.hOptionsWnd || !IsWindow(g_Daemon.hOptionsWnd)) &&
            (!g_Daemon.hShortcutsWnd || !IsWindow(g_Daemon.hShortcutsWnd))) {
            if (isKeyDown && vk == VK_ESCAPE) {
                PostMessageW(g_Daemon.hOverlayWnd, WM_KEYDOWN, VK_ESCAPE, 0);
                return 1;
            }
            if (GetForegroundWindow() != g_Daemon.hOverlayWnd) {
                if (isKeyDown && !IsModifierVk(vk)) {
                    switch (vk) {
                        case VK_RETURN:
                        case VK_DELETE:
                        case VK_BACK:
                        case 'V': case 'P': case 'H': case 'L': case 'A': case 'N':
                        case 'R': case 'E': case 'T': case 'X': case 'M': case 'F':
                        case 'O': case 'C': case 'S': case 'Z': case 'Y':
                            PostMessageW(g_Daemon.hOverlayWnd, WM_KEYDOWN, vk, 0);
                            return 1;
                        default:
                            break;
                    }
                } else if (isKeyDown && vk == VK_CONTROL) {
                    PostMessageW(g_Daemon.hOverlayWnd, WM_KEYDOWN, VK_CONTROL, 0);
                } else if (isKeyUp && vk == VK_CONTROL) {
                    PostMessageW(g_Daemon.hOverlayWnd, WM_KEYUP, VK_CONTROL, 0);
                }
            }
        }

        // Handle key-down for all hotkeys, plus key-up fallback for VK_SNAPSHOT
        // (Many keyboards/games swallow WM_KEYDOWN for Shift+PrintScreen and only emit WM_KEYUP)
        if ((isKeyDown || (isKeyUp && vk == VK_SNAPSHOT)) && !IsModifierVk(vk)) {
            UINT curMods = GetCurrentHardwareModifiers();
            if (kb->flags & LLKHF_ALTDOWN) curMods |= MOD_ALT;

            auto matchHk = [&](const HotkeyBinding& hk) {
                return hk.vk != 0 && vk == hk.vk && curMods == hk.modifiers;
            };

            if (matchHk(g_Daemon.hkPrevRegion)) {
                PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_PREV_REGION, 0, 0);
                return 1;
            }
            if (matchHk(g_Daemon.hkRegionSnip)) {
                PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_REGION_SNIP, 0, 0);
                return 1;
            }
            if (matchHk(g_Daemon.hkFullSnap)) {
                PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_FULL_SNAP, 0, 0);
                return 1;
            }
        }
    }
    return CallNextHookEx(g_Daemon.hKeyHook, nCode, wParam, lParam);
}

static DWORD g_InputHookThreadId = 0;

static DWORD WINAPI InputHookWorkerThread(LPVOID) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    g_Daemon.hKeyHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_Daemon.hInst, 0);

    // 15ms hardware key-state poller + foreground game hook-chain refresher
    UINT_PTR pollTimer = SetTimer(nullptr, 0, 15, nullptr);
    HWND lastFgWnd = nullptr;
    bool wasDownPrev = false;
    bool wasDownReg  = false;
    bool wasDownFull = false;

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_TIMER && msg.wParam == pollTimer) {
            // 1. If foreground window changed (e.g. borderless game just took focus and installed its own hook),
            // re-register our WH_KEYBOARD_LL hook so PepperSnap stays first in the hook chain.
            HWND curFg = GetForegroundWindow();
            if (curFg && curFg != lastFgWnd && curFg != g_Daemon.hOverlayWnd && curFg != g_Daemon.hOptionsWnd) {
                lastFgWnd = curFg;
                if (g_Daemon.hKeyHook) UnhookWindowsHookEx(g_Daemon.hKeyHook);
                g_Daemon.hKeyHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_Daemon.hInst, 0);
            }

            // 2. Direct GetAsyncKeyState polling fallback for borderless/fullscreen games that swallow hooks
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

                bool downPrev = isHkPressed(g_Daemon.hkPrevRegion);
                bool downReg  = isHkPressed(g_Daemon.hkRegionSnip);
                bool downFull = isHkPressed(g_Daemon.hkFullSnap);

                if (downPrev && !wasDownPrev) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_PREV_REGION, 0, 0);
                } else if (downReg && !wasDownReg) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_REGION_SNIP, 0, 0);
                } else if (downFull && !wasDownFull) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_FULL_SNAP, 0, 0);
                }

                wasDownPrev = downPrev;
                wasDownReg  = downReg;
                wasDownFull = downFull;
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

// ----------------------------------------------------------------------------
// Screen Capture, Clipboard & Image Saving
// ----------------------------------------------------------------------------

static bool IsSampledRegionMostlyBlack(const DWORD* pBits, int bufW, int bufH, const RECT& rc) {
    if (!pBits || bufW <= 0 || bufH <= 0) return true;
    int x0 = std::max(0, (int)rc.left);
    int y0 = std::max(0, (int)rc.top);
    int x1 = std::min(bufW, (int)rc.right);
    int y1 = std::min(bufH, (int)rc.bottom);
    int w = x1 - x0, h = y1 - y0;
    if (w < 16 || h < 16) return false;

    int nonBlackCount = 0;
    for (int gy = 1; gy <= 8; ++gy) {
        int sy = y0 + (h * gy) / 9;
        const DWORD* row = pBits + (size_t)sy * bufW;
        for (int gx = 1; gx <= 8; ++gx) {
            int sx = x0 + (w * gx) / 9;
            DWORD px = row[sx] & 0x00FFFFFFu;
            if (px != 0) {
                nonBlackCount++;
            }
        }
    }
    return nonBlackCount < 3;
}

static bool HasValidRenderedContent(const DWORD* pBits, int w, int h) {
    if (!pBits || w < 16 || h < 16) return false;
    int nonBlackCount = 0;
    DWORD firstNonZero = 0;
    bool hasVariance = false;
    for (int gy = 1; gy <= 8; ++gy) {
        int sy = (h * gy) / 9;
        const DWORD* row = pBits + (size_t)sy * w;
        for (int gx = 1; gx <= 8; ++gx) {
            int sx = (w * gx) / 9;
            DWORD px = row[sx] & 0x00FFFFFFu;
            if (px != 0) {
                nonBlackCount++;
                if (firstNonZero == 0) firstNonZero = px;
                else if (px != firstNonZero) hasVariance = true;
            }
        }
    }
    return nonBlackCount >= 4 && hasVariance;
}

Bitmap* PepperSnapDaemon::CaptureVirtualDesktop() {
    vScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    vScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    vScreenW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    vScreenH = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (vScreenW <= 0 || vScreenH <= 0) {
        vScreenW = GetSystemMetrics(SM_CXSCREEN);
        vScreenH = GetSystemMetrics(SM_CYSCREEN);
    }

    // Flush DWM composition so borderless DirectFlip / DXGI swapchains are up to date
    DwmFlush();

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

    if (!BitBlt(hMem, 0, 0, vScreenW, vScreenH, hScreen, vScreenX, vScreenY, SRCCOPY | CAPTUREBLT)) {
        BitBlt(hMem, 0, 0, vScreenW, vScreenH, hScreen, vScreenX, vScreenY, SRCCOPY);
    }

    // Borderless Window / Fullscreen Game Capture Fallback:
    // If a borderless fullscreen game or hardware-accelerated fullscreen app is in the foreground,
    // check if BitBlt missed its swapchain (or if it is a fullscreen borderless window) and capture via DWM PrintWindow(PW_RENDERFULLCONTENT).
    HWND hFg = GetForegroundWindow();
    if (hFg && IsWindowVisible(hFg) && !IsIconic(hFg) &&
        hFg != hOverlayWnd && hFg != hOptionsWnd && hFg != hShortcutsWnd && hFg != hTrayWnd) {
        WCHAR clsName[128] = {0};
        GetClassNameW(hFg, clsName, 127);
        if (wcscmp(clsName, L"Progman") != 0 &&
            wcscmp(clsName, L"WorkerW") != 0 &&
            wcscmp(clsName, L"Shell_TrayWnd") != 0 &&
            wcscmp(clsName, L"Windows.UI.Core.CoreWindow") != 0 &&
            wcscmp(clsName, L"XamlExplorerHostIslandWindow") != 0) {
            RECT rcFg = {0};
            if (GetWindowRect(hFg, &rcFg)) {
                int fgW = rcFg.right - rcFg.left;
                int fgH = rcFg.bottom - rcFg.top;
                HMONITOR hMon = MonitorFromWindow(hFg, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi = { sizeof(mi) };
                bool isFullscreenOrBorderless = false;
                if (hMon && GetMonitorInfoW(hMon, &mi)) {
                    int monW = mi.rcMonitor.right - mi.rcMonitor.left;
                    int monH = mi.rcMonitor.bottom - mi.rcMonitor.top;
                    LONG style = GetWindowLongW(hFg, GWL_STYLE);
                    bool borderlessStyle = ((style & WS_POPUP) != 0) || ((style & WS_CAPTION) == 0);
                    if (fgW >= monW && fgH >= monH &&
                        rcFg.left <= mi.rcMonitor.left && rcFg.top <= mi.rcMonitor.top) {
                        isFullscreenOrBorderless = true;
                    } else if (borderlessStyle && fgW >= (monW * 4) / 5 && fgH >= (monH * 4) / 5) {
                        isFullscreenOrBorderless = true;
                    }
                }

                RECT rcRel = {
                    rcFg.left - vScreenX,
                    rcFg.top - vScreenY,
                    rcFg.right - vScreenX,
                    rcFg.bottom - vScreenY
                };
                bool bitBltWasBlack = IsSampledRegionMostlyBlack((const DWORD*)pBits, vScreenW, vScreenH, rcRel);

                if ((isFullscreenOrBorderless || bitBltWasBlack) && fgW > 64 && fgH > 64 && fgW <= vScreenW * 2 && fgH <= vScreenH * 2) {
                    BITMAPINFO fgBmi = {};
                    fgBmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                    fgBmi.bmiHeader.biWidth = fgW;
                    fgBmi.bmiHeader.biHeight = -fgH;
                    fgBmi.bmiHeader.biPlanes = 1;
                    fgBmi.bmiHeader.biBitCount = 32;
                    fgBmi.bmiHeader.biCompression = BI_RGB;

                    void* pFgBits = nullptr;
                    HDC hFgMem = CreateCompatibleDC(hScreen);
                    HBITMAP hFgDIB = CreateDIBSection(hScreen, &fgBmi, DIB_RGB_COLORS, &pFgBits, nullptr, 0);
                    HGDIOBJ hFgOld = SelectObject(hFgMem, hFgDIB);

                    // PW_RENDERFULLCONTENT (0x00000002) instructs DWM to render DirectX/Vulkan/OpenGL swapchains
                    BOOL pwOk = PrintWindow(hFg, hFgMem, 0x00000002);
                    if (!pwOk || !HasValidRenderedContent((const DWORD*)pFgBits, fgW, fgH)) {
                        HDC hWndDC = GetWindowDC(hFg);
                        if (hWndDC) {
                            BitBlt(hFgMem, 0, 0, fgW, fgH, hWndDC, 0, 0, SRCCOPY);
                            ReleaseDC(hFg, hWndDC);
                        }
                    }

                    if (HasValidRenderedContent((const DWORD*)pFgBits, fgW, fgH)) {
                        BitBlt(hMem, rcRel.left, rcRel.top, fgW, fgH, hFgMem, 0, 0, SRCCOPY);
                    }

                    SelectObject(hFgMem, hFgOld);
                    if (hFgDIB) DeleteObject(hFgDIB);
                    DeleteDC(hFgMem);
                }
            }
        }
    }

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

void PepperSnapDaemon::InstantFullscreenCapture() {
    if (hOverlayWnd) CloseRegionSnipOverlay();

    Bitmap* fullBmp = CaptureVirtualDesktop();
    if (!fullBmp) return;

    bool copied = false;
    if (alsoCopyFullscreen) {
        copied = CopyBitmapToClipboard(fullBmp);
    }

    CreateDirectoryW(saveFolder.c_str(), nullptr);
    int seq = captureCounter++;
    std::wstring fileName = FormatFilename(seq) + GetFormatExtension(fullscreenFormat);
    std::wstring fullPath = saveFolder + L"\\" + fileName;

    if (SaveBitmapToPath(fullBmp, fullPath)) {
        lastSavedFilePath = fullPath;
        ShowTrayToast(
            L"Instant fullscreen captured (" + std::to_wstring(vScreenW) + L"×" + std::to_wstring(vScreenH) + L")",
            (copied ? L"Copied to clipboard & saved to:\n" : L"Saved to:\n") + fullPath + L"\n(click notification to reveal in Explorer)"
        );
    }
    delete fullBmp;
}

void PepperSnapDaemon::RecordLastCustomSelection() {
    if (!hasSelection) return;
    int sx0 = std::max(0, (int)std::min(selRect.left, selRect.right));
    int sy0 = std::max(0, (int)std::min(selRect.top, selRect.bottom));
    int sx1 = std::min(vScreenW, (int)std::max(selRect.left, selRect.right));
    int sy1 = std::min(vScreenH, (int)std::max(selRect.top, selRect.bottom));
    if (sx1 - sx0 >= 1 && sy1 - sy0 >= 1) {
        hasLastCustomSel = true;
        lastCustomSelRect = { sx0, sy0, sx1, sy1 };
        SaveSettings();
    }
}

void PepperSnapDaemon::InstantPreviousRegionCapture() {
    if (hOverlayWnd) {
        RecordLastCustomSelection();
        CloseRegionSnipOverlay();
    }
    if (!hasLastCustomSel ||
        lastCustomSelRect.right <= lastCustomSelRect.left ||
        lastCustomSelRect.bottom <= lastCustomSelRect.top) {
        ShowTrayToast(
            L"No previous custom area found",
            L"Select a custom area first (" + FormatHotkeyString(hkRegionSnip) + L"), then use " +
            FormatHotkeyString(hkPrevRegion) + L" to instantly capture & save it."
        );
        return;
    }

    Bitmap* fullBmp = CaptureVirtualDesktop();
    if (!fullBmp) return;

    int rx = std::max(0, std::min(vScreenW - 1, (int)std::min(lastCustomSelRect.left, lastCustomSelRect.right)));
    int ry = std::max(0, std::min(vScreenH - 1, (int)std::min(lastCustomSelRect.top, lastCustomSelRect.bottom)));
    int rw = std::min(vScreenW - rx, (int)std::abs(lastCustomSelRect.right - lastCustomSelRect.left));
    int rh = std::min(vScreenH - ry, (int)std::abs(lastCustomSelRect.bottom - lastCustomSelRect.top));
    if (rw < 1 || rh < 1) {
        delete fullBmp;
        return;
    }

    Bitmap* cropped = fullBmp->Clone(rx, ry, rw, rh, PixelFormat32bppARGB);
    delete fullBmp;
    if (!cropped) return;

    CopyBitmapToClipboard(cropped);

    CreateDirectoryW(saveFolder.c_str(), nullptr);
    int seq = captureCounter++;
    std::wstring fileName = FormatFilename(seq) + GetFormatExtension(regionFormat);
    std::wstring fullPath = saveFolder + L"\\" + fileName;

    if (SaveBitmapToPath(cropped, fullPath)) {
        lastSavedFilePath = fullPath;
        ShowTrayToast(
            L"Previous custom area saved (" + std::to_wstring(rw) + L"×" + std::to_wstring(rh) + L")",
            L"Copied to clipboard & saved to:\n" + fullPath + L"\n(click notification to reveal in Explorer)"
        );
    }
    delete cropped;
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
        case DBTN_ACT_OCR: {
            // Magnifying glass with 'T' inside the glass circle
            float gcx = cx - 1.8f;
            float gcy = cy - 1.8f;
            float gr  = 6.2f;
            g.DrawEllipse(&whitePen, gcx - gr, gcy - gr, gr * 2.0f, gr * 2.0f);

            Pen handlePen(Color(255, 248, 250, 252), 2.2f);
            handlePen.SetStartCap(LineCapRound);
            handlePen.SetEndCap(LineCapRound);
            g.DrawLine(&handlePen, gcx + 4.4f, gcy + 4.4f, cx + 7.2f, cy + 7.2f);

            Pen tPen(Color(255, 248, 250, 252), 1.6f);
            tPen.SetStartCap(LineCapRound);
            tPen.SetEndCap(LineCapRound);
            g.DrawLine(&tPen, gcx - 2.7f, gcy - 2.5f, gcx + 2.7f, gcy - 2.5f);
            g.DrawLine(&tPen, gcx,        gcy - 2.5f, gcx,        gcy + 3.1f);
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
            if (b.label.empty()) {
                // Centered copy sheets icon (1-square button)
                g.DrawRectangle(&whitePen, cx - 5.5f, cy - 6.5f, 8.0f, 10.0f);
                g.DrawRectangle(&whitePen, cx - 2.5f, cy - 3.5f, 8.0f, 10.0f);
            } else {
                // Copy sheets + label
                g.DrawRectangle(&whitePen, cx - 18.0f, cy - 6.0f, 8.0f, 10.0f);
                g.DrawRectangle(&whitePen, cx - 15.0f, cy - 3.0f, 8.0f, 10.0f);
                FontFamily ff(L"Segoe UI");
                Font f(&ff, 11.0f, FontStyleBold, UnitPixel);
                g.DrawString(b.label.c_str(), -1, &f, PointF(cx - 4.0f, cy - 7.0f), &whiteBrush);
            }
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
    if (!hasSelection || dragMode == DragMode::CreatingSelection || dragMode == DragMode::MovingSelection || isResizing) return;

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
        { OverlayTool::SelectMove,   L"Sel", L"Select mode (V)" },
        { OverlayTool::Pen,          L"Pen", L"Pen (P)" },
        { OverlayTool::Highlighter,  L"Hi",  L"Stabilo highlighter (H)" },
        { OverlayTool::Line,         L"Lin", L"Line (L)" },
        { OverlayTool::Arrow,        L"Arr", L"Arrow (A)" },
        { OverlayTool::NumberArrow,  L"1→",  L"Numbering arrow (N)" },
        { OverlayTool::Rectangle,    L"Box", L"Square / rectangle (R)" },
        { OverlayTool::Ellipse,      L"Cir", L"Circle / ellipse (E)" },
        { OverlayTool::TextBox,      L"Txt", L"Text box (T)" },
        { OverlayTool::MosaicSquare, L"MSq", L"Mosaic square (X) — pixelation level is affected by size selection" },
        { OverlayTool::MosaicCircle, L"MCi", L"Mosaic circle (M) — pixelation level is affected by size selection" }
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

    // Row 3 metrics (Drag Handle + Undo/Redo/Clear/Options/Pin/OCR/SaveAs/Save/Copy/Close)
    struct ActEntry { int id; int w; const wchar_t* lbl; const wchar_t* tip; bool primary; };
    ActEntry acts[] = {
        { DBTN_ACT_DRAG_HUD, toolBtnW, L"", L"Drag to move toolbar (resets when selection moves)", false },
        { DBTN_ACT_UNDO,     toolBtnW, L"", L"Undo (Ctrl+Z)", false },
        { DBTN_ACT_REDO,     toolBtnW, L"", L"Redo (Ctrl+Y)", false },
        { DBTN_ACT_CLEAR,    toolBtnW, L"", L"Clear all annotations", false },
        { DBTN_ACT_OPTIONS,  toolBtnW, L"", L"Options", false },
        { DBTN_ACT_PIN,      toolBtnW, L"", L"Pin on top (F)", false },
        { DBTN_ACT_OCR,      toolBtnW, L"", L"Extract text with OCR (O)", false },
        { DBTN_ACT_SAVE_AS,  toolBtnW, L"", L"Save as JPG/PNG/WEBP/BMP", false },
        { DBTN_ACT_SAVE,     toolBtnW, L"", L"Quick save (Ctrl+S)", false },
        { DBTN_ACT_COPY,     toolBtnW, L"", L"Copy to clipboard (Ctrl+C)", true },
        { DBTN_ACT_CLOSE,    toolBtnW, L"", L"Close overlay (Esc)", false }
    };
    const size_t actCount = sizeof(acts) / sizeof(acts[0]);
    const int actBtnH = 28;
    int row3W = 0;
    for (size_t i = 0; i < actCount; ++i) {
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
        int bottomExtra = (pillRight && pillBottom) ? 31 : 0;
        int topExtra    = (pillRight && !pillBottom) ? 31 : 0;

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
            b.tooltip = L"Numbering arrow (N) — next: " + std::to_wstring(nextStepNum);
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
            rb.tooltip = L"Reset numbering arrow counter to 1 (next: " + std::to_wstring(nextStepNum) + L")";
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
    const wchar_t* strokeTips[4] = { L"Small size (2px)", L"Medium size (4px)", L"Large size (8px)", L"Extra large size (14px)" };
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

    // --- Row 3: Drag Button + Options, Pin, OCR, Saves, Copy & Close Toolbar (Right-aligned to rightX) ---
    int r3X = rightX - row3W;
    int r3Y = r2Y + row2H + rowGap;
    for (size_t i = 0; i < actCount; ++i) {
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

static void UpdatePinnedWindowLayout(HWND hWnd, PinnedWindowData* data, bool resizeWindow) {
    if (!data || !data->bmp) return;
    int imgW = std::max(32, (int)std::round(data->origW * data->scale));
    int imgH = std::max(32, (int)std::round(data->origH * data->scale));

    const int toolBtnW = 32;
    const int actBtnH  = 28;
    const int gap      = 3;
    const int hudGap   = 8; // Same 8px gap between toolbar and custom area

    struct PinBtnSpec { int id; int w; const wchar_t* lbl; const wchar_t* tip; bool primary; };
    PinBtnSpec specs[4] = {
        { DBTN_ACT_SAVE_AS, toolBtnW,           L"",     L"Save as JPG/PNG/WEBP/BMP", false },
        { DBTN_ACT_SAVE,    toolBtnW,           L"",     L"Quick save",               false },
        { DBTN_ACT_COPY,    toolBtnW * 2 + gap, L"Copy", L"Copy to clipboard",        true  },
        { DBTN_ACT_CLOSE,   toolBtnW,           L"",     L"Close pinned image",       false }
    };

    int stripW = 0;
    for (int i = 0; i < 4; ++i) {
        stripW += specs[i].w + (i > 0 ? gap : 0);
    }

    int winW = std::max(imgW, stripW);
    int winH = imgH + hudGap + actBtnH;
    int imgLeft = winW - imgW;

    data->buttons.clear();
    int bx = winW - stripW;
    int by = imgH + hudGap;
    for (int i = 0; i < 4; ++i) {
        DockButton b;
        b.id = specs[i].id;
        b.rect = { bx, by, bx + specs[i].w, by + actBtnH };
        b.label = specs[i].lbl;
        b.tooltip = specs[i].tip;
        b.isPrimaryAction = specs[i].primary;
        data->buttons.push_back(b);
        bx += specs[i].w + gap;
    }

    if (resizeWindow && hWnd) {
        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, winW, winH, SWP_NOMOVE | SWP_NOACTIVATE);
    }

    if (hWnd) {
        HRGN hRgn = CreateRectRgn(imgLeft, 0, imgLeft + imgW, imgH);
        for (const auto& b : data->buttons) {
            HRGN hBtnRgn = CreateRectRgn(b.rect.left, b.rect.top, b.rect.right, b.rect.bottom);
            CombineRgn(hRgn, hRgn, hBtnRgn, RGN_OR);
            DeleteObject(hBtnRgn);
        }
        SetWindowRgn(hWnd, hRgn, TRUE);
    }
}

static LRESULT CALLBACK PinWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PinnedWindowData* data = (PinnedWindowData*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT && data) {
                SetCursor(LoadCursorW(nullptr, (data->hoveredBtnId != -1) ? IDC_HAND : IDC_SIZEALL));
                return TRUE;
            }
            break;
        case WM_LBUTTONDOWN:
            if (data) {
                POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                for (const auto& b : data->buttons) {
                    if (PtInRect(&b.rect, pt)) {
                        switch (b.id) {
                            case DBTN_ACT_CLOSE:
                                DestroyWindow(hWnd);
                                return 0;
                            case DBTN_ACT_COPY:
                                if (data->bmp) {
                                    g_Daemon.CopyBitmapToClipboard(data->bmp);
                                    g_Daemon.ShowTrayToast(
                                        L"Copied to clipboard (" + std::to_wstring(data->origW) + L"×" + std::to_wstring(data->origH) + L" px)",
                                        L"Ready to paste (Ctrl+V)."
                                    );
                                }
                                return 0;
                            case DBTN_ACT_SAVE:
                                if (data->bmp) {
                                    CreateDirectoryW(g_Daemon.saveFolder.c_str(), nullptr);
                                    std::wstring fn = g_Daemon.FormatFilename(g_Daemon.captureCounter++) +
                                                      PepperSnapDaemon::GetFormatExtension(g_Daemon.regionFormat);
                                    std::wstring full = g_Daemon.saveFolder + L"\\" + fn;
                                    if (g_Daemon.SaveBitmapToPath(data->bmp, full)) {
                                        g_Daemon.lastSavedFilePath = full;
                                        g_Daemon.ShowTrayToast(
                                            L"Capture saved",
                                            L"Saved to: " + full + L"\n(click to reveal in Explorer)"
                                        );
                                    }
                                }
                                return 0;
                            case DBTN_ACT_SAVE_AS:
                                if (data->bmp) {
                                    WCHAR szFile[MAX_PATH] = {0};
                                    std::wstring defName = g_Daemon.FormatFilename(g_Daemon.captureCounter++) +
                                                           PepperSnapDaemon::GetFormatExtension(g_Daemon.regionFormat);
                                    wcsncpy_s(szFile, defName.c_str(), _TRUNCATE);

                                    OPENFILENAMEW ofn = {0};
                                    ofn.lStructSize = sizeof(ofn);
                                    ofn.hwndOwner = hWnd;
                                    ofn.lpstrInitialDir = g_Daemon.saveFolder.c_str();
                                    ofn.lpstrFile = szFile;
                                    ofn.nMaxFile = MAX_PATH;
                                    ofn.lpstrFilter = L"JPEG image (*.jpg)\0*.jpg\0PNG image (*.png)\0*.png\0WebP image (*.webp)\0*.webp\0BMP bitmap (*.bmp)\0*.bmp\0";
                                    ofn.nFilterIndex = (DWORD)g_Daemon.regionFormat + 1;
                                    const wchar_t* defExts[4] = { L"jpg", L"png", L"webp", L"bmp" };
                                    ofn.lpstrDefExt = defExts[(int)g_Daemon.regionFormat & 3];
                                    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

                                    if (GetSaveFileNameW(&ofn)) {
                                        if (g_Daemon.SaveBitmapToPath(data->bmp, szFile)) {
                                            g_Daemon.lastSavedFilePath = szFile;
                                            g_Daemon.ShowTrayToast(
                                                L"Saved capture",
                                                std::wstring(szFile) + L"\n(click to open in Explorer)"
                                            );
                                        }
                                    }
                                }
                                return 0;
                        }
                    }
                }
                data->dragging = true;
                GetCursorPos(&data->dragStartMouse);
                RECT rc;
                GetWindowRect(hWnd, &rc);
                data->dragStartWnd = { rc.left, rc.top };
                SetCapture(hWnd);
            }
            return 0;
        case WM_MOUSEMOVE:
            if (data) {
                if (data->dragging) {
                    POINT cur;
                    GetCursorPos(&cur);
                    SetWindowPos(
                        hWnd, HWND_TOPMOST,
                        data->dragStartWnd.x + (cur.x - data->dragStartMouse.x),
                        data->dragStartWnd.y + (cur.y - data->dragStartMouse.y),
                        0, 0, SWP_NOSIZE | SWP_NOACTIVATE
                    );
                    return 0;
                }
                if (!data->trackingMouseLeave) {
                    TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
                    TrackMouseEvent(&tme);
                    data->trackingMouseLeave = true;
                }
                POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                int newHover = -1;
                for (const auto& b : data->buttons) {
                    if (PtInRect(&b.rect, pt)) {
                        newHover = b.id;
                        break;
                    }
                }
                if (newHover != data->hoveredBtnId) {
                    data->hoveredBtnId = newHover;
                    SetCursor(LoadCursorW(nullptr, (newHover != -1) ? IDC_HAND : IDC_SIZEALL));
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
            }
            return 0;
        case WM_MOUSELEAVE:
            if (data) {
                data->trackingMouseLeave = false;
                if (data->hoveredBtnId != -1) {
                    data->hoveredBtnId = -1;
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
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
                UpdatePinnedWindowLayout(hWnd, data, true);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);
            int w = rc.right - rc.left, h = rc.bottom - rc.top;
            if (w > 0 && h > 0) {
                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
                HGDIOBJ oldBmp = SelectObject(memDC, memBmp);
                {
                    Graphics g(memDC);
                    g.SetSmoothingMode(SmoothingModeAntiAlias);
                    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
                    SolidBrush clearBg(Color(255, 15, 23, 42));
                    g.FillRectangle(&clearBg, 0, 0, w, h);

                    if (data && data->bmp) {
                        int imgW = std::max(32, (int)std::round(data->origW * data->scale));
                        int imgH = std::max(32, (int)std::round(data->origH * data->scale));
                        int imgLeft = w - imgW;

                        GraphicsState st = g.Save();
                        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
                        g.DrawImage(data->bmp, imgLeft, 0, imgW, imgH);
                        g.Restore(st);

                        GraphicsState stBorder = g.Save();
                        g.SetSmoothingMode(SmoothingModeNone);
                        g.SetPixelOffsetMode(PixelOffsetModeNone);
                        Pen border1px(Color(255, 239, 68, 68), 1.0f);
                        g.DrawRectangle(&border1px, imgLeft, 0, imgW - 1, imgH - 1);
                        if (imgW >= 4 && imgH >= 4) {
                            g.DrawRectangle(&border1px, imgLeft + 1, 1, imgW - 3, imgH - 3);
                        }
                        g.Restore(stBorder);

                        for (const auto& b : data->buttons) {
                            int bw = (int)(b.rect.right - b.rect.left);
                            int bh = (int)(b.rect.bottom - b.rect.top);
                            RectF rf((float)b.rect.left, (float)b.rect.top, (float)bw, (float)bh);
                            bool hovered = (b.id == data->hoveredBtnId);
                            Color bgCol = Color(240, 15, 23, 42);
                            if (b.isPrimaryAction) {
                                bgCol = hovered ? Color(255, 220, 38, 38) : Color(255, 239, 68, 68);
                            } else if (hovered) {
                                bgCol = Color(255, 51, 65, 85);
                            }
                            SolidBrush btnBg(bgCol);
                            Pen btnBorder(Color(220, 51, 65, 85), 1.0f);

                            GraphicsState stBtn = g.Save();
                            g.SetSmoothingMode(SmoothingModeNone);
                            g.SetPixelOffsetMode(PixelOffsetModeNone);
                            g.FillRectangle(&btnBg, (INT)b.rect.left, (INT)b.rect.top, (INT)bw, (INT)bh);
                            g.Restore(stBtn);

                            PepperSnapDaemon::DrawDockButtonIcon(g, b, rf);

                            GraphicsState stOutline = g.Save();
                            g.SetSmoothingMode(SmoothingModeNone);
                            g.SetPixelOffsetMode(PixelOffsetModeNone);
                            g.DrawRectangle(&btnBorder, (INT)b.rect.left, (INT)b.rect.top, (INT)(bw - 1), (INT)(bh - 1));
                            g.Restore(stOutline);
                        }
                    }
                }
                BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
                SelectObject(memDC, oldBmp);
                DeleteObject(memBmp);
                DeleteDC(memDC);
            }
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            g_Daemon.pinnedWindows.erase(
                std::remove(g_Daemon.pinnedWindows.begin(), g_Daemon.pinnedWindows.end(), hWnd),
                g_Daemon.pinnedWindows.end()
            );
            if (data) {
                delete data->bmp;
                delete data;
            }
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

HWND PepperSnapDaemon::CreatePinnedWindow(Bitmap* bmp, int x, int y, float initialScale) {
    if (!bmp) return nullptr;
    int sw = (int)bmp->GetWidth();
    int sh = (int)bmp->GetHeight();

    PinnedWindowData* data = new PinnedWindowData();
    data->bmp = bmp;
    data->origW = sw;
    data->origH = sh;
    data->scale = initialScale;
    UpdatePinnedWindowLayout(nullptr, data, false);

    int imgW = std::max(32, (int)std::round(data->origW * data->scale));
    int imgH = std::max(32, (int)std::round(data->origH * data->scale));
    const int stripW = 32 + 3 + 32 + 3 + 67 + 3 + 32;
    int winW = std::max(imgW, stripW);
    int winH = imgH + 8 + 28;
    int winX = x - (winW - imgW);

    HWND hPin = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"PepperSnapPinWnd", L"PepperSnap Pin",
        WS_POPUP | WS_VISIBLE, winX, y, winW, winH,
        nullptr, nullptr, hInst, nullptr
    );
    SetWindowLongPtrW(hPin, GWLP_USERDATA, (LONG_PTR)data);
    UpdatePinnedWindowLayout(hPin, data, false);
    InvalidateRect(hPin, nullptr, FALSE);
    pinnedWindows.push_back(hPin);
    return hPin;
}

void PepperSnapDaemon::ActionPinToDesktop() {
    Bitmap* cropped = RenderCroppedRegionBitmap();
    if (!cropped) return;
    int sx = vScreenX + std::min(selRect.left, selRect.right);
    int sy = vScreenY + std::min(selRect.top, selRect.bottom);

    std::wstring savedNote;
    if (alsoSavePinned) {
        CreateDirectoryW(saveFolder.c_str(), nullptr);
        std::wstring fn = FormatFilename(captureCounter++) + GetFormatExtension(regionFormat);
        std::wstring full = saveFolder + L"\\" + fn;
        if (SaveBitmapToPath(cropped, full)) {
            lastSavedFilePath = full;
            savedNote = L"Saved to: " + full + L"\n";
        }
    }

    CloseRegionSnipOverlay();
    CreatePinnedWindow(cropped, sx, sy, 1.0f);
    ShowTrayToast(L"Pinned on top", savedNote + L"Drag to move · Scroll wheel to scale.");
}

// Unified Options Modal (Save Folder, Default Formats, JPEG Quality Slider, Naming Pattern + Restore Default, Non-Stacking Highlighter)
struct OptionsDlgState {
    std::wstring folder;
    ImageFormat regionFmt = ImageFormat::JPEG;
    ImageFormat fullFmt = ImageFormat::JPEG;
    ImageFormat copyFmt = ImageFormat::JPEG;
    int jpgQuality = 90;
    std::wstring naming;
    bool autoSaveCopy = true;
    bool alsoCopyFull = true;
    bool alsoSavePin = true;
    bool alsoSaveOcr = true;
    bool nonStackingHi = true;
    bool penSmoothEnabled = true;
    int penSmoothStrength = 15;
    bool autoUpdateEnabled = true;
    UpdateCheckInterval updateInterval = UpdateCheckInterval::EveryDay;
    HotkeyBinding hkRegion{ MOD_CONTROL, VK_SNAPSHOT };
    HotkeyBinding hkFull{ MOD_SHIFT, VK_SNAPSHOT };
    HotkeyBinding hkPrev{ MOD_CONTROL | MOD_SHIFT, VK_SNAPSHOT };
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
    HWND hCopyFullChk = nullptr;
    HWND hSavePinChk = nullptr;
    HWND hSaveOcrChk = nullptr;
    HWND hNonStackingChk = nullptr;
    HWND hPenSmoothChk = nullptr;
    HWND hPenSmoothSlider = nullptr;
    HWND hPenSmoothValLbl = nullptr;
    HWND hAutoUpdateChk = nullptr;
    HWND hComboUpdateInterval = nullptr;
    HWND hCheckUpdateNowBtn = nullptr;
    HWND hHkRegionBtn = nullptr;
    HWND hHkFullBtn = nullptr;
    HWND hHkPrevBtn = nullptr;
    HWND hHkRestoreBtn = nullptr;
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
#define IDC_OPT_AUTOUPDATE_CHK   1016
#define IDC_OPT_UPDATE_INTERVAL  1017
#define IDC_OPT_CHECK_UPDATE_NOW 1018
#define IDC_OPT_HK_REGION_BTN    1019
#define IDC_OPT_HK_FULL_BTN      1020
#define IDC_OPT_HK_PREV_BTN      1021
#define IDC_OPT_HK_RESTORE_BTN   1022
#define IDC_OPT_SAVE_PIN_CHK     1023
#define IDC_OPT_SAVE_OCR_CHK     1024

static void RefreshHotkeyButtonLabels(OptionsDlgState* st) {
    if (!st) return;
    if (st->hHkRegionBtn) {
        std::wstring s = (g_RecordingHotkeySlot == 1)
            ? L"Press shortcut keys... (Esc to cancel)"
            : PepperSnapDaemon::FormatHotkeyString(st->hkRegion);
        SetWindowTextW(st->hHkRegionBtn, s.c_str());
    }
    if (st->hHkFullBtn) {
        std::wstring s = (g_RecordingHotkeySlot == 2)
            ? L"Press shortcut keys... (Esc to cancel)"
            : PepperSnapDaemon::FormatHotkeyString(st->hkFull);
        SetWindowTextW(st->hHkFullBtn, s.c_str());
    }
    if (st->hHkPrevBtn) {
        std::wstring s = (g_RecordingHotkeySlot == 3)
            ? L"Press shortcut keys... (Esc to cancel)"
            : PepperSnapDaemon::FormatHotkeyString(st->hkPrev);
        SetWindowTextW(st->hHkPrevBtn, s.c_str());
    }
}

static std::wstring FormatPenSmoothLabel(bool enabled, int strength) {
    if (!enabled) return L"Smoothing strength: off";
    const wchar_t* desc = L"balanced";
    if (strength <= 25) desc = L"weak";
    else if (strength <= 60) desc = L"balanced";
    else if (strength <= 85) desc = L"strong";
    else desc = L"ultra smooth";
    return L"Smoothing strength: " + std::to_wstring(strength) + L"% (" + desc + L")";
}

static void UpdateOptionsPreviewLabel(OptionsDlgState* st) {
    if (!st || !st->hNamingEdit || !st->hPreviewLbl) return;
    WCHAR buf[512] = {0};
    GetWindowTextW(st->hNamingEdit, buf, 511);
    int selFmt = st->hComboRegion ? (int)SendMessageW(st->hComboRegion, CB_GETCURSEL, 0, 0) : (int)st->regionFmt;
    if (selFmt < 0 || selFmt > 3) selFmt = 0;
    std::wstring preview = L"Live preview:  " +
        PepperSnapDaemon::FormatFilenameWithPattern(buf, g_Daemon.captureCounter) +
        PepperSnapDaemon::GetFormatExtension((ImageFormat)selFmt) +
        L"   (zero EXIF/metadata)";
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

            // Save Folder Location
            HWND hLblFolder = CreateWindowExW(0, L"STATIC", L"Save screenshot to...",
                WS_CHILD | WS_VISIBLE, 18, 14, 516, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblFolder, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            st->hFolderEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", st->folder.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 18, 40, 320, 26, hWnd, (HMENU)IDC_OPT_FOLDER_EDIT, nullptr, nullptr);
            SendMessageW(st->hFolderEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hBrowse = CreateWindowExW(0, L"BUTTON", L"Browse...",
                WS_CHILD | WS_VISIBLE, 344, 39, 84, 28, hWnd, (HMENU)IDC_OPT_FOLDER_BROWSE, nullptr, nullptr);
            SendMessageW(hBrowse, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hDeskDef = CreateWindowExW(0, L"BUTTON", L"Use desktop",
                WS_CHILD | WS_VISIBLE, 434, 39, 100, 28, hWnd, (HMENU)IDC_OPT_FOLDER_DEFAULT, nullptr, nullptr);
            SendMessageW(hDeskDef, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Save Format & JPEG Quality Slider
            HWND hLblFormats = CreateWindowExW(0, L"STATIC", L"Save format",
                WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 18, 80, 516, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblFormats, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            HWND hLblReg = CreateWindowExW(0, L"STATIC", L"Selected area format:",
                WS_CHILD | WS_VISIBLE, 18, 108, 155, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblReg, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hComboRegion = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 176, 104, 175, 140, hWnd, (HMENU)IDC_OPT_COMBO_REGION, nullptr, nullptr);
            SendMessageW(st->hComboRegion, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hLblFull = CreateWindowExW(0, L"STATIC", L"Instant fullscreen format:",
                WS_CHILD | WS_VISIBLE, 18, 140, 155, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblFull, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hComboFull = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 176, 136, 175, 140, hWnd, (HMENU)IDC_OPT_COMBO_FULL, nullptr, nullptr);
            SendMessageW(st->hComboFull, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hLblCopy = CreateWindowExW(0, L"STATIC", L"Copy format:",
                WS_CHILD | WS_VISIBLE, 18, 172, 155, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblCopy, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hComboCopy = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 176, 168, 175, 140, hWnd, (HMENU)IDC_OPT_COMBO_COPY, nullptr, nullptr);
            SendMessageW(st->hComboCopy, WM_SETFONT, (WPARAM)hFont, TRUE);

            const wchar_t* fmtItems[4] = { L"JPEG (.jpg)", L"PNG (.png)", L"WebP (.webp)", L"BMP (.bmp)" };
            for (int i = 0; i < 4; ++i) {
                SendMessageW(st->hComboRegion, CB_ADDSTRING, 0, (LPARAM)fmtItems[i]);
                SendMessageW(st->hComboFull,   CB_ADDSTRING, 0, (LPARAM)fmtItems[i]);
                SendMessageW(st->hComboCopy,   CB_ADDSTRING, 0, (LPARAM)fmtItems[i]);
            }
            SendMessageW(st->hComboRegion, CB_SETCURSEL, (WPARAM)st->regionFmt, 0);
            SendMessageW(st->hComboFull,   CB_SETCURSEL, (WPARAM)st->fullFmt, 0);
            SendMessageW(st->hComboCopy,   CB_SETCURSEL, (WPARAM)st->copyFmt, 0);

            // JPEG Quality Slider (10% - 100%)
            std::wstring qText = L"JPEG quality: " + std::to_wstring(st->jpgQuality) + L"%";
            st->hQualityValLbl = CreateWindowExW(0, L"STATIC", qText.c_str(),
                WS_CHILD | WS_VISIBLE, 18, 206, 150, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(st->hQualityValLbl, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hQualitySlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS, 172, 202, 362, 32, hWnd, (HMENU)IDC_OPT_QUALITY_SLIDER, nullptr, nullptr);
            SendMessageW(st->hQualitySlider, TBM_SETRANGE, TRUE, MAKELONG(10, 100));
            SendMessageW(st->hQualitySlider, TBM_SETTICFREQ, 10, 0);
            SendMessageW(st->hQualitySlider, TBM_SETPOS, TRUE, st->jpgQuality);

            // Timestamp Naming Pattern + Restore Default Button
            HWND hLblNaming = CreateWindowExW(0, L"STATIC",
                L"Naming pattern",
                WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 18, 244, 516, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblNaming, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            st->hNamingEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", st->naming.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 18, 270, 386, 26, hWnd, (HMENU)IDC_OPT_NAMING_EDIT, nullptr, nullptr);
            SendMessageW(st->hNamingEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hRestoreNaming = CreateWindowExW(0, L"BUTTON", L"Restore default",
                WS_CHILD | WS_VISIBLE, 412, 269, 122, 28, hWnd, (HMENU)IDC_OPT_NAMING_RESTORE, nullptr, nullptr);
            SendMessageW(hRestoreNaming, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hPreviewLbl = CreateWindowExW(0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 18, 302, 516, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(st->hPreviewLbl, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Shortcut keys
            HWND hLblShortcuts = CreateWindowExW(0, L"STATIC",
                L"Shortcut keys",
                WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 18, 336, 380, 24, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblShortcuts, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            st->hHkRestoreBtn = CreateWindowExW(0, L"BUTTON", L"Restore default",
                WS_CHILD | WS_VISIBLE, 412, 332, 122, 28, hWnd, (HMENU)IDC_OPT_HK_RESTORE_BTN, nullptr, nullptr);
            SendMessageW(st->hHkRestoreBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hLblHkReg = CreateWindowExW(0, L"STATIC", L"Custom area:",
                WS_CHILD | WS_VISIBLE, 18, 370, 276, 24, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblHkReg, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hHkRegionBtn = CreateWindowExW(0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE, 300, 366, 234, 28, hWnd, (HMENU)IDC_OPT_HK_REGION_BTN, nullptr, nullptr);
            SendMessageW(st->hHkRegionBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hLblHkFull = CreateWindowExW(0, L"STATIC", L"Instant fullscreen:",
                WS_CHILD | WS_VISIBLE, 18, 404, 276, 24, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblHkFull, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hHkFullBtn = CreateWindowExW(0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE, 300, 400, 234, 28, hWnd, (HMENU)IDC_OPT_HK_FULL_BTN, nullptr, nullptr);
            SendMessageW(st->hHkFullBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hLblHkPrev = CreateWindowExW(0, L"STATIC", L"Instant save previous custom area:",
                WS_CHILD | WS_VISIBLE, 18, 438, 276, 24, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblHkPrev, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hHkPrevBtn = CreateWindowExW(0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE, 300, 434, 234, 28, hWnd, (HMENU)IDC_OPT_HK_PREV_BTN, nullptr, nullptr);
            SendMessageW(st->hHkPrevBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

            RefreshHotkeyButtonLabels(st);

            // Toggles at Bottom of Option Window (below Shortcut keys section):
            // 1. Check for updates (GitHub Releases)
            st->hAutoUpdateChk = CreateWindowExW(0, L"BUTTON",
                L"Automatically check for updates (GitHub releases):",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 476, 310, 24, hWnd, (HMENU)IDC_OPT_AUTOUPDATE_CHK, nullptr, nullptr);
            SendMessageW(st->hAutoUpdateChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hAutoUpdateChk, BM_SETCHECK, st->autoUpdateEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

            st->hComboUpdateInterval = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 332, 474, 98, 160, hWnd, (HMENU)IDC_OPT_UPDATE_INTERVAL, nullptr, nullptr);
            SendMessageW(st->hComboUpdateInterval, WM_SETFONT, (WPARAM)hFont, TRUE);
            const wchar_t* intervalItems[5] = {
                L"Every day",
                L"3-day",
                L"Every week",
                L"2-weekly",
                L"Every month"
            };
            for (int i = 0; i < 5; ++i) {
                SendMessageW(st->hComboUpdateInterval, CB_ADDSTRING, 0, (LPARAM)intervalItems[i]);
            }
            SendMessageW(st->hComboUpdateInterval, CB_SETCURSEL, (WPARAM)st->updateInterval, 0);
            EnableWindow(st->hComboUpdateInterval, st->autoUpdateEnabled ? TRUE : FALSE);

            st->hCheckUpdateNowBtn = CreateWindowExW(0, L"BUTTON", L"Check now",
                WS_CHILD | WS_VISIBLE, 436, 473, 98, 28, hWnd, (HMENU)IDC_OPT_CHECK_UPDATE_NOW, nullptr, nullptr);
            SendMessageW(st->hCheckUpdateNowBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

            // 2. Non-Stacking Highlighter
            st->hNonStackingChk = CreateWindowExW(0, L"BUTTON",
                L"Non-stacking highlighter (prevent overlapping highlighter strokes from darkening)",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 508, 516, 24, hWnd, (HMENU)IDC_OPT_NONSTACK_CHK, nullptr, nullptr);
            SendMessageW(st->hNonStackingChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hNonStackingChk, BM_SETCHECK, st->nonStackingHi ? BST_CHECKED : BST_UNCHECKED, 0);

            // 3. Also save screenshot when copying selected area
            st->hAutoSaveChk = CreateWindowExW(0, L"BUTTON",
                L"Also save screenshot when copying selected area",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 536, 516, 24, hWnd, (HMENU)IDC_OPT_AUTOSAVE_CHK, nullptr, nullptr);
            SendMessageW(st->hAutoSaveChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hAutoSaveChk, BM_SETCHECK, st->autoSaveCopy ? BST_CHECKED : BST_UNCHECKED, 0);

            // 4. Also copy instant fullscreen
            st->hCopyFullChk = CreateWindowExW(0, L"BUTTON",
                L"Also copy instant fullscreen",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 564, 516, 24, hWnd, (HMENU)IDC_OPT_NOCOPY_SAVE_CHK, nullptr, nullptr);
            SendMessageW(st->hCopyFullChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hCopyFullChk, BM_SETCHECK, st->alsoCopyFull ? BST_CHECKED : BST_UNCHECKED, 0);

            // 5. Also save pinned screenshot
            st->hSavePinChk = CreateWindowExW(0, L"BUTTON",
                L"Also save pinned screenshot",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 592, 516, 24, hWnd, (HMENU)IDC_OPT_SAVE_PIN_CHK, nullptr, nullptr);
            SendMessageW(st->hSavePinChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hSavePinChk, BM_SETCHECK, st->alsoSavePin ? BST_CHECKED : BST_UNCHECKED, 0);

            // 6. Also save copied text
            st->hSaveOcrChk = CreateWindowExW(0, L"BUTTON",
                L"Also save copied text",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 620, 516, 24, hWnd, (HMENU)IDC_OPT_SAVE_OCR_CHK, nullptr, nullptr);
            SendMessageW(st->hSaveOcrChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hSaveOcrChk, BM_SETCHECK, st->alsoSaveOcr ? BST_CHECKED : BST_UNCHECKED, 0);

            // 7. Enable pen and highlighter smoothing.
            st->hPenSmoothChk = CreateWindowExW(0, L"BUTTON",
                L"Enable pen and highlighter smoothing.",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 648, 516, 24, hWnd, (HMENU)IDC_OPT_PENSMOOTH_CHK, nullptr, nullptr);
            SendMessageW(st->hPenSmoothChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hPenSmoothChk, BM_SETCHECK, st->penSmoothEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

            std::wstring psText = FormatPenSmoothLabel(st->penSmoothEnabled, st->penSmoothStrength);
            st->hPenSmoothValLbl = CreateWindowExW(0, L"STATIC", psText.c_str(),
                WS_CHILD | WS_VISIBLE, 18, 680, 224, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(st->hPenSmoothValLbl, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hPenSmoothSlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS, 244, 676, 290, 32, hWnd, (HMENU)IDC_OPT_PENSMOOTH_SLIDER, nullptr, nullptr);
            SendMessageW(st->hPenSmoothSlider, TBM_SETRANGE, TRUE, MAKELONG(5, 100));
            SendMessageW(st->hPenSmoothSlider, TBM_SETTICFREQ, 10, 0);
            SendMessageW(st->hPenSmoothSlider, TBM_SETPOS, TRUE, st->penSmoothStrength);
            EnableWindow(st->hPenSmoothSlider, st->penSmoothEnabled ? TRUE : FALSE);

            // Save & Cancel Buttons + clickable %appdata%\PepperSnap persistence link on bottom
            HFONT hLinkFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, TRUE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            st->hAppDataInfo = CreateWindowExW(0, L"STATIC",
                L"\x24D8 settings.ini is saved in %appdata%\\PepperSnap",
                WS_CHILD | WS_VISIBLE | SS_NOPREFIX | SS_LEFTNOWORDWRAP | SS_NOTIFY, 18, 727, 296, 24, hWnd, (HMENU)IDC_OPT_OPEN_APPDATA, nullptr, nullptr);
            SendMessageW(st->hAppDataInfo, WM_SETFONT, (WPARAM)hLinkFont, TRUE);

            HWND hOk = CreateWindowExW(0, L"BUTTON", L"Save options",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 318, 720, 108, 34, hWnd, (HMENU)IDOK, nullptr, nullptr);
            SendMessageW(hOk, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            HWND hCancel = CreateWindowExW(0, L"BUTTON", L"Cancel",
                WS_CHILD | WS_VISIBLE, 434, 720, 100, 34, hWnd, (HMENU)IDCANCEL, nullptr, nullptr);
            SendMessageW(hCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

            UpdateOptionsPreviewLabel(st);
            return 0;
        }
        case WM_SHORTCUT_RECORDED: {
            if (!st) return 0;
            UINT mods = (UINT)wParam;
            UINT vk = (UINT)lParam;
            int slot = g_RecordingHotkeySlot;
            g_RecordingHotkeySlot = 0;
            if (vk != 0 && vk != VK_ESCAPE) {
                if (slot == 1) st->hkRegion = { mods, vk };
                else if (slot == 2) st->hkFull = { mods, vk };
                else if (slot == 3) st->hkPrev = { mods, vk };
            }
            RefreshHotkeyButtonLabels(st);
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
                std::wstring qText = L"JPEG quality: " + std::to_wstring(st->jpgQuality) + L"%";
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
            if (id == IDC_OPT_AUTOUPDATE_CHK) {
                st->autoUpdateEnabled = (SendMessageW(st->hAutoUpdateChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                EnableWindow(st->hComboUpdateInterval, st->autoUpdateEnabled ? TRUE : FALSE);
                return 0;
            }
            if (id == IDC_OPT_CHECK_UPDATE_NOW) {
                g_Daemon.CheckForUpdatesAsync(true);
                return 0;
            }
            if (id == IDC_OPT_HK_REGION_BTN) {
                g_RecordingHotkeySlot = (g_RecordingHotkeySlot == 1) ? 0 : 1;
                RefreshHotkeyButtonLabels(st);
                SetFocus(hWnd);
                return 0;
            }
            if (id == IDC_OPT_HK_FULL_BTN) {
                g_RecordingHotkeySlot = (g_RecordingHotkeySlot == 2) ? 0 : 2;
                RefreshHotkeyButtonLabels(st);
                SetFocus(hWnd);
                return 0;
            }
            if (id == IDC_OPT_HK_PREV_BTN) {
                g_RecordingHotkeySlot = (g_RecordingHotkeySlot == 3) ? 0 : 3;
                RefreshHotkeyButtonLabels(st);
                SetFocus(hWnd);
                return 0;
            }
            if (id == IDC_OPT_HK_RESTORE_BTN) {
                g_RecordingHotkeySlot = 0;
                st->hkRegion = { MOD_CONTROL, VK_SNAPSHOT };
                st->hkFull   = { MOD_SHIFT, VK_SNAPSHOT };
                st->hkPrev   = { MOD_CONTROL | MOD_SHIFT, VK_SNAPSHOT };
                RefreshHotkeyButtonLabels(st);
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
                bi.lpszTitle = L"Select default screenshot save folder:";
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
                st->alsoCopyFull  = (SendMessageW(st->hCopyFullChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->alsoSavePin   = (SendMessageW(st->hSavePinChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->alsoSaveOcr   = (SendMessageW(st->hSaveOcrChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->autoUpdateEnabled = (SendMessageW(st->hAutoUpdateChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                int uSel = (int)SendMessageW(st->hComboUpdateInterval, CB_GETCURSEL, 0, 0);
                if (uSel >= 0 && uSel <= 4) st->updateInterval = (UpdateCheckInterval)uSel;
                st->confirmed = true;
                g_RecordingHotkeySlot = 0;
                DestroyWindow(hWnd);
                return 0;
            }
            if (id == IDCANCEL) {
                g_RecordingHotkeySlot = 0;
                DestroyWindow(hWnd);
                return 0;
            }
            break;
        }
        case WM_DESTROY:
            g_RecordingHotkeySlot = 0;
            break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void PepperSnapDaemon::ShowOptionsModal() {
    if (hOptionsWnd && IsWindow(hOptionsWnd)) {
        if (IsIconic(hOptionsWnd)) {
            ShowWindow(hOptionsWnd, SW_RESTORE);
        }
        BringWindowToTop(hOptionsWnd);
        SetForegroundWindow(hOptionsWnd);
        SetActiveWindow(hOptionsWnd);
        SetFocus(hOptionsWnd);
        return;
    }

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
    g_RecordingHotkeySlot = 0;
    OptionsDlgState st;
    st.folder = saveFolder;
    st.regionFmt = regionFormat;
    st.fullFmt = fullscreenFormat;
    st.copyFmt = copyFormat;
    st.jpgQuality = jpegQuality;
    st.naming = namingPattern;
    st.autoSaveCopy = autoSaveOnCopy;
    st.alsoCopyFull = alsoCopyFullscreen;
    st.alsoSavePin  = alsoSavePinned;
    st.alsoSaveOcr  = alsoSaveOcrText;
    st.nonStackingHi = nonStackingHighlighter;
    st.penSmoothEnabled = penSmoothingEnabled;
    st.penSmoothStrength = penSmoothingStrength;
    st.autoUpdateEnabled = autoCheckUpdates;
    st.updateInterval = updateInterval;
    st.hkRegion = hkRegionSnip;
    st.hkFull = hkFullSnap;
    st.hkPrev = hkPrevRegion;

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    HWND hParent = hTrayWnd;
    std::wstring optTitle = L"PepperSnap v" + std::wstring(APP_VERSION) + L" — options";
    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"PepperSnapOptionsModal",
        optTitle.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        (sw - 564) / 2, (sh - 818) / 2, 564, 818, hParent, nullptr, hInst, &st
    );
    hOptionsWnd = hDlg;
    BringWindowToTop(hDlg);
    SetForegroundWindow(hDlg);
    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (g_RecordingHotkeySlot != 0 && (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)) {
            DWORD vk = (DWORD)msg.wParam;
            if (vk == VK_ESCAPE) {
                SendMessageW(hDlg, WM_SHORTCUT_RECORDED, 0, 0);
                continue;
            }
            if (!IsModifierVk(vk)) {
                UINT mods = 0;
                if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) mods |= MOD_CONTROL;
                if ((GetKeyState(VK_SHIFT)   & 0x8000) != 0) mods |= MOD_SHIFT;
                if ((GetKeyState(VK_MENU)    & 0x8000) != 0) mods |= MOD_ALT;
                if ((GetKeyState(VK_LWIN)    & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0) mods |= MOD_WIN;
                SendMessageW(hDlg, WM_SHORTCUT_RECORDED, (WPARAM)mods, (LPARAM)vk);
                continue;
            }
            continue;
        }
        if (!hOverlayWnd && (msg.hwnd == hDlg || IsChild(hDlg, msg.hwnd))) {
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) { SendMessageW(hDlg, WM_COMMAND, IDOK, 0); continue; }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) { SendMessageW(hDlg, WM_COMMAND, IDCANCEL, 0); continue; }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    g_RecordingHotkeySlot = 0;
    hOptionsWnd = nullptr;
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
        alsoCopyFullscreen = st.alsoCopyFull;
        alsoSavePinned = st.alsoSavePin;
        alsoSaveOcrText = st.alsoSaveOcr;
        nonStackingHighlighter = st.nonStackingHi;
        penSmoothingEnabled = st.penSmoothEnabled;
        penSmoothingStrength = st.penSmoothStrength;
        autoCheckUpdates = st.autoUpdateEnabled;
        updateInterval = st.updateInterval;
        hkRegionSnip = st.hkRegion;
        hkFullSnap = st.hkFull;
        hkPrevRegion = st.hkPrev;
        ApplyGlobalHotkeys();
        CreateDirectoryW(saveFolder.c_str(), nullptr);
        SaveSettings();
        if (hOverlayWnd) {
            InvalidateRect(hOverlayWnd, nullptr, FALSE);
        }
        std::wstring appDataDir = PepperSnapDaemon::GetAppDataSettingsDir();
        ShowTrayToast(
            L"PepperSnap options saved",
            L"settings.ini is saved in " + appDataDir,
            appDataDir
        );
    }
}

struct ShortcutsDlgParams {
    const wchar_t* text = nullptr;
    int textW = 460;
    int textH = 420;
};

static LRESULT CALLBACK ShortcutsDlgWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            const ShortcutsDlgParams* p = (const ShortcutsDlgParams*)cs->lpCreateParams;
            HFONT hFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            HFONT hBoldFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

            int tw = p ? p->textW : 460;
            int th = p ? p->textH : 420;
            HWND hBody = CreateWindowExW(0, L"STATIC", (p && p->text) ? p->text : L"",
                WS_CHILD | WS_VISIBLE | SS_NOPREFIX | SS_LEFTNOWORDWRAP, 16, 14, tw, th, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hBody, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hOk = CreateWindowExW(0, L"BUTTON", L"OK",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 16 + tw - 92, 14 + th + 12, 92, 30, hWnd, (HMENU)IDOK, nullptr, nullptr);
            SendMessageW(hOk, WM_SETFONT, (WPARAM)hBoldFont, TRUE);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hWnd);
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void PepperSnapDaemon::ShowShortcutsModal() {
    if (hShortcutsWnd && IsWindow(hShortcutsWnd)) {
        if (IsIconic(hShortcutsWnd)) {
            ShowWindow(hShortcutsWnd, SW_RESTORE);
        }
        BringWindowToTop(hShortcutsWnd);
        SetForegroundWindow(hShortcutsWnd);
        SetActiveWindow(hShortcutsWnd);
        SetFocus(hShortcutsWnd);
        return;
    }

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = ShortcutsDlgWndProc;
        wc.hInstance = hInst;
        wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APPICON));
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"PepperSnapShortcutsModal";
        RegisterClassExW(&wc);
        reg = true;
    }

    std::wstring text =
        L"PepperSnap v" + std::wstring(APP_VERSION) + L" — complete hotkeys reference\n"
        L"───────────────────────────────────────────────────\n\n"
        L"Global capture hotkeys (customizable in Options):\n"
        L"  • " + FormatHotkeyString(hkRegionSnip) + L"   Custom area snip & annotate\n"
        L"  • " + FormatHotkeyString(hkFullSnap) + L"   Instant fullscreen capture\n"
        L"  • " + FormatHotkeyString(hkPrevRegion) + L"   Instant save previous custom area\n\n"
        L"In-place annotation tools & cursors:\n"
        L"  • V   Select mode (arrow cursor) — move / resize / recolor\n"
        L"  • P   Freehand pen tool (pen cursor)\n"
        L"  • H   Stabilo highlighter tool (Stabilo cursor)\n"
        L"  • L   Line tool (+ crosshair cursor)\n"
        L"  • A   Arrow tool (+ crosshair cursor, no shadow)\n"
        L"  • N   Numbering arrow tool 1→, 2→ (+ crosshair cursor)\n"
        L"  • R   Square / rectangle tool (+ crosshair cursor)\n"
        L"  • E   Circle / ellipse tool (+ crosshair cursor)\n"
        L"  • T   In-place text box tool (arrow cursor)\n"
        L"  • X   Mosaic square tool (+ crosshair + live dashed guide)\n"
        L"  • M   Mosaic circle tool (+ crosshair + live dashed guide)\n"
        L"  • F   Pin on top\n"
        L"  • O   Extract text with OCR to clipboard\n\n"
        L"Size input & Escape behavior:\n"
        L"  • Click size indicator to type custom size, then press enter\n"
        L"  • Esc (when using any drawing tool) -> switches to select mode\n"
        L"  • Esc (when in select mode)         -> exits capture overlay";

    // Measure exact non-wrapping pixel bounds of text so the dialog fits snugly
    int measuredW = 450, measuredH = 416;
    HDC hdcScreen = GetDC(nullptr);
    if (hdcScreen) {
        HFONT hMeasureFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HGDIOBJ hOldF = SelectObject(hdcScreen, hMeasureFont);
        RECT rcCalc = { 0, 0, 0, 0 };
        DrawTextW(hdcScreen, text.c_str(), -1, &rcCalc, DT_CALCRECT | DT_NOPREFIX);
        measuredW = (rcCalc.right - rcCalc.left) + 6;
        measuredH = (rcCalc.bottom - rcCalc.top) + 4;
        SelectObject(hdcScreen, hOldF);
        DeleteObject(hMeasureFont);
        ReleaseDC(nullptr, hdcScreen);
    }

    ShortcutsDlgParams params;
    params.text = text.c_str();
    params.textW = measuredW;
    params.textH = measuredH;

    DWORD dwStyle = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
    DWORD dwExStyle = WS_EX_DLGMODALFRAME | WS_EX_TOPMOST;
    RECT rcWin = { 0, 0, 16 + measuredW + 16, 14 + measuredH + 12 + 30 + 14 };
    AdjustWindowRectEx(&rcWin, dwStyle, FALSE, dwExStyle);
    int winW = rcWin.right - rcWin.left;
    int winH = rcWin.bottom - rcWin.top;

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    HWND hOwner = (hOptionsWnd && IsWindow(hOptionsWnd))
        ? hOptionsWnd
        : hTrayWnd;

    HWND hDlg = CreateWindowExW(
        dwExStyle, L"PepperSnapShortcutsModal",
        L"PepperSnap — keyboard shortcuts and info",
        dwStyle,
        (sw - winW) / 2, (sh - winH) / 2, winW, winH, hOwner, nullptr, hInst, &params
    );
    hShortcutsWnd = hDlg;
    BringWindowToTop(hDlg);
    SetForegroundWindow(hDlg);
    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (!hOverlayWnd && (msg.hwnd == hDlg || IsChild(hDlg, msg.hwnd)) &&
            msg.message == WM_KEYDOWN && (msg.wParam == VK_RETURN || msg.wParam == VK_ESCAPE)) {
            DestroyWindow(hDlg);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    hShortcutsWnd = nullptr;
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
        L"Copied to clipboard (" + std::to_wstring(w) + L"×" + std::to_wstring(h) + L" px)",
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
    ShowTrayToast(L"Copied RGB code: " + rgbStr, msgBuf);
}

void PepperSnapDaemon::ActionQuickSaveAndClose() {
    Bitmap* bmp = RenderCroppedRegionBitmap();
    if (!bmp) return;
    CreateDirectoryW(saveFolder.c_str(), nullptr);
    std::wstring fn = FormatFilename(captureCounter++) + GetFormatExtension(regionFormat);
    std::wstring full = saveFolder + L"\\" + fn;
    if (SaveBitmapToPath(bmp, full)) {
        lastSavedFilePath = full;
        ShowTrayToast(
            L"Capture saved",
            L"Saved to: " + full + L"\n(click to reveal in Explorer)"
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
    ofn.lpstrFilter = L"JPEG image (*.jpg)\0*.jpg\0PNG image (*.png)\0*.png\0WebP image (*.webp)\0*.webp\0BMP bitmap (*.bmp)\0*.bmp\0";
    ofn.nFilterIndex = (DWORD)regionFormat + 1;
    const wchar_t* defExts[4] = { L"jpg", L"png", L"webp", L"bmp" };
    ofn.lpstrDefExt = defExts[(int)regionFormat & 3];
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (GetSaveFileNameW(&ofn)) {
        if (SaveBitmapToPath(bmp, szFile)) {
            lastSavedFilePath = szFile;
            ShowTrayToast(L"Saved capture", std::wstring(szFile) + L"\n(click to open in Explorer)");
        }
    }
    delete bmp;
    CloseRegionSnipOverlay();
}

// ----------------------------------------------------------------------------
// Zero-Dependency Windows Native OCR (Windows.Media.Ocr via WinRT COM ABI)
// ----------------------------------------------------------------------------

namespace WinRtOcrAbi {
    typedef void* HSTRING_ABI;

    typedef HRESULT (WINAPI *PFN_RoInitialize)(int initType);
    typedef void    (WINAPI *PFN_RoUninitialize)();
    typedef HRESULT (WINAPI *PFN_RoGetActivationFactory)(HSTRING_ABI activatableClassId, REFIID iid, void** factory);
    typedef HRESULT (WINAPI *PFN_WindowsCreateString)(LPCWSTR sourceString, UINT32 length, HSTRING_ABI* string);
    typedef HRESULT (WINAPI *PFN_WindowsDeleteString)(HSTRING_ABI string);
    typedef PCWSTR  (WINAPI *PFN_WindowsGetStringRawBuffer)(HSTRING_ABI string, UINT32* length);

    // {71AF914D-C10F-484B-BC50-14BC623B3A27} Windows.Storage.Streams.IBufferFactory
    static const GUID IID_IBufferFactory_ABI = { 0x71af914d, 0xc10f, 0x484b, { 0xbc, 0x50, 0x14, 0xbc, 0x62, 0x3b, 0x3a, 0x27 } };
    // {905A0FEF-BC53-11DF-8C49-001E4FC686DA} Windows.Storage.Streams.IBufferByteAccess (IUnknown-based)
    static const GUID IID_IBufferByteAccess_ABI = { 0x905a0fef, 0xbc53, 0x11df, { 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda } };
    // {DF0385DB-672F-4A9D-806E-C2442F343E86} Windows.Graphics.Imaging.ISoftwareBitmapStatics
    static const GUID IID_ISoftwareBitmapStatics_ABI = { 0xdf0385db, 0x672f, 0x4a9d, { 0x80, 0x6e, 0xc2, 0x44, 0x2f, 0x34, 0x3e, 0x86 } };
    // {5BFFA85A-3384-3540-9940-699120D428A8} Windows.Media.Ocr.IOcrEngineStatics
    static const GUID IID_IOcrEngineStatics_ABI = { 0x5bffa85a, 0x3384, 0x3540, { 0x99, 0x40, 0x69, 0x91, 0x20, 0xd4, 0x28, 0xa8 } };
    // {9B0252AC-0C27-44F8-B792-9793FB66C63E} Windows.Globalization.ILanguageFactory
    static const GUID IID_ILanguageFactory_ABI = { 0x9b0252ac, 0x0c27, 0x44f8, { 0xb7, 0x92, 0x97, 0x93, 0xfb, 0x66, 0xc6, 0x3e } };
    // {00000036-0000-0000-C000-000000000046} ABI::Windows::Foundation::IAsyncInfo
    static const GUID IID_IAsyncInfo_ABI = { 0x00000036, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

    static inline void** GetVtbl(void* p) {
        return *(void***)p;
    }
    static inline HRESULT CallQueryInterface(void* p, REFIID riid, void** ppv) {
        typedef HRESULT (STDMETHODCALLTYPE *Fn)(void*, REFIID, void**);
        return ((Fn)GetVtbl(p)[0])(p, riid, ppv);
    }
    static inline ULONG CallRelease(void* p) {
        if (!p) return 0;
        typedef ULONG (STDMETHODCALLTYPE *Fn)(void*);
        return ((Fn)GetVtbl(p)[2])(p);
    }

    struct OcrThreadWork {
        const BYTE* bgraPixels = nullptr;
        int width = 0;
        int height = 0;
        bool success = false;
        std::wstring recognizedText;
    };

    static DWORD WINAPI OcrMtaWorkerProc(LPVOID lpParam) {
        OcrThreadWork* work = (OcrThreadWork*)lpParam;
        if (!work || !work->bgraPixels || work->width <= 0 || work->height <= 0) return 0;

        HMODULE hComBase = GetModuleHandleW(L"combase.dll");
        if (!hComBase) hComBase = LoadLibraryW(L"combase.dll");
        if (!hComBase) return 0;

        auto fnRoInitialize          = (PFN_RoInitialize)GetProcAddress(hComBase, "RoInitialize");
        auto fnRoUninitialize        = (PFN_RoUninitialize)GetProcAddress(hComBase, "RoUninitialize");
        auto fnRoGetActivationFactory= (PFN_RoGetActivationFactory)GetProcAddress(hComBase, "RoGetActivationFactory");
        auto fnWindowsCreateString   = (PFN_WindowsCreateString)GetProcAddress(hComBase, "WindowsCreateString");
        auto fnWindowsDeleteString   = (PFN_WindowsDeleteString)GetProcAddress(hComBase, "WindowsDeleteString");
        auto fnWindowsGetStringRaw   = (PFN_WindowsGetStringRawBuffer)GetProcAddress(hComBase, "WindowsGetStringRawBuffer");

        if (!fnRoInitialize || !fnRoGetActivationFactory || !fnWindowsCreateString || !fnWindowsDeleteString || !fnWindowsGetStringRaw) {
            return 0;
        }

        HRESULT hrInit = fnRoInitialize(1 /* RO_INIT_MULTITHREADED */);
        bool didInit = SUCCEEDED(hrInit);

        auto activateFactory = [&](const wchar_t* className, REFIID iid, void** outFactory) -> bool {
            *outFactory = nullptr;
            HSTRING_ABI hCls = nullptr;
            if (FAILED(fnWindowsCreateString(className, (UINT32)wcslen(className), &hCls))) return false;
            HRESULT hr = fnRoGetActivationFactory(hCls, iid, outFactory);
            fnWindowsDeleteString(hCls);
            return SUCCEEDED(hr) && (*outFactory != nullptr);
        };

        UINT32 byteCount = (UINT32)work->width * (UINT32)work->height * 4u;
        void* pBufferFactory = nullptr;
        void* pBuffer = nullptr;
        void* pBufferByteAccess = nullptr;
        void* pSbStatics = nullptr;
        void* pSoftwareBitmap = nullptr;
        void* pOcrStatics = nullptr;
        void* pOcrEngine = nullptr;
        void* pAsyncOp = nullptr;
        void* pAsyncInfo = nullptr;
        void* pOcrResult = nullptr;

        do {
            // 1. Create Windows.Storage.Streams.Buffer and copy BGRA8 pixels
            if (!activateFactory(L"Windows.Storage.Streams.Buffer", IID_IBufferFactory_ABI, &pBufferFactory)) break;
            typedef HRESULT (STDMETHODCALLTYPE *FnBufferCreate)(void*, UINT32, void**);
            if (FAILED(((FnBufferCreate)GetVtbl(pBufferFactory)[6])(pBufferFactory, byteCount, &pBuffer)) || !pBuffer) break;

            if (FAILED(CallQueryInterface(pBuffer, IID_IBufferByteAccess_ABI, &pBufferByteAccess)) || !pBufferByteAccess) break;
            BYTE* dstBytes = nullptr;
            typedef HRESULT (STDMETHODCALLTYPE *FnGetBufferPtr)(void*, BYTE**);
            if (FAILED(((FnGetBufferPtr)GetVtbl(pBufferByteAccess)[3])(pBufferByteAccess, &dstBytes)) || !dstBytes) break;
            memcpy(dstBytes, work->bgraPixels, byteCount);

            typedef HRESULT (STDMETHODCALLTYPE *FnPutLength)(void*, UINT32);
            if (FAILED(((FnPutLength)GetVtbl(pBuffer)[8])(pBuffer, byteCount))) break;

            // 2. Create Windows.Graphics.Imaging.SoftwareBitmap from IBuffer (BitmapPixelFormat::Bgra8 = 87)
            if (!activateFactory(L"Windows.Graphics.Imaging.SoftwareBitmap", IID_ISoftwareBitmapStatics_ABI, &pSbStatics)) break;
            typedef HRESULT (STDMETHODCALLTYPE *FnCreateCopyFromBuffer)(void*, void*, INT32, INT32, INT32, void**);
            if (FAILED(((FnCreateCopyFromBuffer)GetVtbl(pSbStatics)[9])(
                    pSbStatics, pBuffer, 87 /* Bgra8 */, (INT32)work->width, (INT32)work->height, &pSoftwareBitmap)) || !pSoftwareBitmap) {
                break;
            }

            // 3. Create Windows.Media.Ocr.OcrEngine (UserProfileLanguages -> AvailableRecognizerLanguages[0] -> en-US)
            if (!activateFactory(L"Windows.Media.Ocr.OcrEngine", IID_IOcrEngineStatics_ABI, &pOcrStatics)) break;
            typedef HRESULT (STDMETHODCALLTYPE *FnTryCreateUserLang)(void*, void**);
            ((FnTryCreateUserLang)GetVtbl(pOcrStatics)[10])(pOcrStatics, &pOcrEngine);

            if (!pOcrEngine) {
                void* pLangVec = nullptr;
                typedef HRESULT (STDMETHODCALLTYPE *FnGetAvailLangs)(void*, void**);
                if (SUCCEEDED(((FnGetAvailLangs)GetVtbl(pOcrStatics)[7])(pOcrStatics, &pLangVec)) && pLangVec) {
                    UINT32 langCount = 0;
                    typedef HRESULT (STDMETHODCALLTYPE *FnGetSize)(void*, UINT32*);
                    if (SUCCEEDED(((FnGetSize)GetVtbl(pLangVec)[7])(pLangVec, &langCount)) && langCount > 0) {
                        void* pFirstLang = nullptr;
                        typedef HRESULT (STDMETHODCALLTYPE *FnGetAt)(void*, UINT32, void**);
                        if (SUCCEEDED(((FnGetAt)GetVtbl(pLangVec)[6])(pLangVec, 0, &pFirstLang)) && pFirstLang) {
                            typedef HRESULT (STDMETHODCALLTYPE *FnTryCreateFromLang)(void*, void*, void**);
                            ((FnTryCreateFromLang)GetVtbl(pOcrStatics)[9])(pOcrStatics, pFirstLang, &pOcrEngine);
                            CallRelease(pFirstLang);
                        }
                    }
                    CallRelease(pLangVec);
                }
            }
            if (!pOcrEngine) {
                void* pLangFactory = nullptr;
                if (activateFactory(L"Windows.Globalization.Language", IID_ILanguageFactory_ABI, &pLangFactory)) {
                    HSTRING_ABI hEn = nullptr;
                    if (SUCCEEDED(fnWindowsCreateString(L"en-US", 5, &hEn))) {
                        void* pEnLang = nullptr;
                        typedef HRESULT (STDMETHODCALLTYPE *FnCreateLang)(void*, HSTRING_ABI, void**);
                        if (SUCCEEDED(((FnCreateLang)GetVtbl(pLangFactory)[6])(pLangFactory, hEn, &pEnLang)) && pEnLang) {
                            typedef HRESULT (STDMETHODCALLTYPE *FnTryCreateFromLang)(void*, void*, void**);
                            ((FnTryCreateFromLang)GetVtbl(pOcrStatics)[9])(pOcrStatics, pEnLang, &pOcrEngine);
                            CallRelease(pEnLang);
                        }
                        fnWindowsDeleteString(hEn);
                    }
                    CallRelease(pLangFactory);
                }
            }
            if (!pOcrEngine) break;

            // 4. Run IOcrEngine::RecognizeAsync(pSoftwareBitmap, &pAsyncOp)
            typedef HRESULT (STDMETHODCALLTYPE *FnRecognizeAsync)(void*, void*, void**);
            if (FAILED(((FnRecognizeAsync)GetVtbl(pOcrEngine)[6])(pOcrEngine, pSoftwareBitmap, &pAsyncOp)) || !pAsyncOp) break;

            if (FAILED(CallQueryInterface(pAsyncOp, IID_IAsyncInfo_ABI, &pAsyncInfo)) || !pAsyncInfo) break;
            typedef HRESULT (STDMETHODCALLTYPE *FnGetStatus)(void*, INT32*);
            INT32 status = 0; // 0 = Started, 1 = Completed, 2 = Canceled, 3 = Error
            for (int waitIter = 0; waitIter < 1500; ++waitIter) {
                if (FAILED(((FnGetStatus)GetVtbl(pAsyncInfo)[7])(pAsyncInfo, &status))) break;
                if (status != 0) break;
                Sleep(10);
            }
            if (status != 1) break;

            typedef HRESULT (STDMETHODCALLTYPE *FnGetResults)(void*, void**);
            if (FAILED(((FnGetResults)GetVtbl(pAsyncOp)[8])(pAsyncOp, &pOcrResult)) || !pOcrResult) break;

            // 5. Extract multi-line text from IOcrResult::get_Lines (preserving line breaks), with fallback to get_Text
            std::wstring combinedLines;
            void* pLinesVec = nullptr;
            typedef HRESULT (STDMETHODCALLTYPE *FnGetLines)(void*, void**);
            if (SUCCEEDED(((FnGetLines)GetVtbl(pOcrResult)[6])(pOcrResult, &pLinesVec)) && pLinesVec) {
                UINT32 lineCount = 0;
                typedef HRESULT (STDMETHODCALLTYPE *FnGetSize)(void*, UINT32*);
                if (SUCCEEDED(((FnGetSize)GetVtbl(pLinesVec)[7])(pLinesVec, &lineCount)) && lineCount > 0) {
                    typedef HRESULT (STDMETHODCALLTYPE *FnGetAt)(void*, UINT32, void**);
                    typedef HRESULT (STDMETHODCALLTYPE *FnGetLineText)(void*, HSTRING_ABI*);
                    for (UINT32 i = 0; i < lineCount; ++i) {
                        void* pLine = nullptr;
                        if (SUCCEEDED(((FnGetAt)GetVtbl(pLinesVec)[6])(pLinesVec, i, &pLine)) && pLine) {
                            HSTRING_ABI hLineStr = nullptr;
                            if (SUCCEEDED(((FnGetLineText)GetVtbl(pLine)[7])(pLine, &hLineStr)) && hLineStr) {
                                UINT32 len = 0;
                                PCWSTR raw = fnWindowsGetStringRaw(hLineStr, &len);
                                if (raw && len > 0) {
                                    if (!combinedLines.empty()) combinedLines += L"\r\n";
                                    combinedLines.append(raw, len);
                                }
                                fnWindowsDeleteString(hLineStr);
                            }
                            CallRelease(pLine);
                        }
                    }
                }
                CallRelease(pLinesVec);
            }

            if (combinedLines.empty()) {
                HSTRING_ABI hFullStr = nullptr;
                typedef HRESULT (STDMETHODCALLTYPE *FnGetText)(void*, HSTRING_ABI*);
                if (SUCCEEDED(((FnGetText)GetVtbl(pOcrResult)[8])(pOcrResult, &hFullStr)) && hFullStr) {
                    UINT32 len = 0;
                    PCWSTR raw = fnWindowsGetStringRaw(hFullStr, &len);
                    if (raw && len > 0) {
                        combinedLines.assign(raw, len);
                    }
                    fnWindowsDeleteString(hFullStr);
                }
            }

            work->recognizedText = combinedLines;
            work->success = true;
        } while (false);

        CallRelease(pOcrResult);
        CallRelease(pAsyncInfo);
        CallRelease(pAsyncOp);
        CallRelease(pOcrEngine);
        CallRelease(pOcrStatics);
        CallRelease(pSoftwareBitmap);
        CallRelease(pSbStatics);
        CallRelease(pBufferByteAccess);
        CallRelease(pBuffer);
        CallRelease(pBufferFactory);

        if (didInit && fnRoUninitialize) {
            fnRoUninitialize();
        }
        return 0;
    }
}

bool PepperSnapDaemon::RecognizeBitmapTextNativeWinRT(Bitmap* srcBmp, std::wstring& outText) {
    outText.clear();
    if (!srcBmp) return false;
    int origW = (int)srcBmp->GetWidth();
    int origH = (int)srcBmp->GetHeight();
    if (origW <= 0 || origH <= 0) return false;

    // Upscale very small captures and add a clean margin so Windows.Media.Ocr reliably detects small single-line text
    int scale = (origW < 160 || origH < 64) ? 2 : 1;
    int scaledW = std::min(3800, origW * scale);
    int scaledH = std::min(3800, origH * scale);
    const int pad = 16;
    int prepW = std::max(64, scaledW + pad * 2);
    int prepH = std::max(64, scaledH + pad * 2);

    Color edgeCol(255, 255, 255, 255);
    srcBmp->GetPixel(0, 0, &edgeCol);

    Bitmap prepBmp(prepW, prepH, PixelFormat32bppARGB);
    {
        Graphics g(&prepBmp);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        SolidBrush padBrush( Color(255, edgeCol.GetR(), edgeCol.GetG(), edgeCol.GetB()) );
        g.FillRectangle(&padBrush, 0, 0, prepW, prepH);
        g.DrawImage(srcBmp, Rect(pad, pad, scaledW, scaledH), 0, 0, origW, origH, UnitPixel);
    }

    Rect lockRc(0, 0, prepW, prepH);
    BitmapData bmpData;
    if (prepBmp.LockBits(&lockRc, ImageLockModeRead, PixelFormat32bppARGB, &bmpData) != Ok) {
        return false;
    }

    std::vector<BYTE> tightBgra((size_t)prepW * (size_t)prepH * 4u);
    for (int y = 0; y < prepH; ++y) {
        const BYTE* srcRow = (const BYTE*)bmpData.Scan0 + (size_t)y * bmpData.Stride;
        BYTE* dstRow = tightBgra.data() + (size_t)y * (size_t)prepW * 4u;
        memcpy(dstRow, srcRow, (size_t)prepW * 4u);
    }
    prepBmp.UnlockBits(&bmpData);

    WinRtOcrAbi::OcrThreadWork work;
    work.bgraPixels = tightBgra.data();
    work.width = prepW;
    work.height = prepH;

    HANDLE hThread = CreateThread(nullptr, 0, WinRtOcrAbi::OcrMtaWorkerProc, &work, 0, nullptr);
    if (!hThread) return false;
    WaitForSingleObject(hThread, 16000);
    CloseHandle(hThread);

    outText = work.recognizedText;
    return work.success;
}

void PepperSnapDaemon::ActionOcrAndClose() {
    Bitmap* bmp = RenderCroppedRegionBitmap();
    if (!bmp) return;
    CloseRegionSnipOverlay();

    std::wstring recognized;
    bool ok = RecognizeBitmapTextNativeWinRT(bmp, recognized);
    delete bmp;

    if (!ok || recognized.empty()) {
        ShowTrayToast(
            L"No text detected",
            L"Windows OCR did not find any readable text in the selected area."
        );
        return;
    }

    if (OpenClipboard(hTrayWnd)) {
        EmptyClipboard();
        size_t bytes = (recognized.size() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (hMem) {
            wchar_t* dst = (wchar_t*)GlobalLock(hMem);
            if (dst) {
                memcpy(dst, recognized.c_str(), bytes);
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
        }
        CloseClipboard();
    }

    std::wstring savedNote;
    if (alsoSaveOcrText) {
        CreateDirectoryW(saveFolder.c_str(), nullptr);
        std::wstring fn = FormatFilename(captureCounter++) + L".txt";
        std::wstring full = saveFolder + L"\\" + fn;
        HANDLE hFile = CreateFileW(full.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, recognized.c_str(), (int)recognized.size(), nullptr, 0, nullptr, nullptr);
            if (utf8Len > 0) {
                std::string utf8Str(utf8Len, '\0');
                WideCharToMultiByte(CP_UTF8, 0, recognized.c_str(), (int)recognized.size(), &utf8Str[0], utf8Len, nullptr, nullptr);
                DWORD written = 0;
                WriteFile(hFile, utf8Str.data(), (DWORD)utf8Str.size(), &written, nullptr);
            }
            CloseHandle(hFile);
            lastSavedFilePath = full;
            savedNote = L"\nSaved to: " + full;
        }
    }

    std::wstring preview = recognized;
    for (wchar_t& c : preview) {
        if (c == L'\r' || c == L'\n') c = L' ';
    }
    if (preview.size() > 110) {
        preview = preview.substr(0, 110) + L"...";
    }
    ShowTrayToast(L"OCR text copied to clipboard", preview + savedNote);
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
    if (!g_Daemon.hasSelection && g_Daemon.hasCtrlHoverWindow && g_Daemon.hBrightDesktopDC) {
        int wx0 = std::max(0, std::min(W, (int)g_Daemon.ctrlHoverWindowRect.left));
        int wy0 = std::max(0, std::min(H, (int)g_Daemon.ctrlHoverWindowRect.top));
        int ww0 = std::min(W - wx0, (int)(g_Daemon.ctrlHoverWindowRect.right - g_Daemon.ctrlHoverWindowRect.left));
        int wh0 = std::min(H - wy0, (int)(g_Daemon.ctrlHoverWindowRect.bottom - g_Daemon.ctrlHoverWindowRect.top));
        if (ww0 >= 1 && wh0 >= 1) {
            BitBlt(memDC, wx0, wy0, ww0, wh0, g_Daemon.hBrightDesktopDC, wx0, wy0, SRCCOPY);
        }
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
            //    Uses the same 8px distance to the selection outline as the toolbar, and +5px horizontal offset
            //    so above-selection and inside-selection positions align identically.
            bool pillRight = false, pillBottom = false;
            g_Daemon.GetDimensionPillAnchor(pillRight, pillBottom);
            int pillW = g_Daemon.isEditingSize ? 146 : 140;
            int pillH = 23;
            const int outlineGap = 8; // Same distance to selected window outline as the toolbar (8px)
            int pillX = pillRight ? (sx + sw - pillW - 5) : (sx + 5);
            pillX = std::max(5, std::min(W - pillW - 5, pillX));

            int pillY = 0;
            if (!pillBottom) {
                pillY = (sy >= pillH + outlineGap + 4) ? (sy - outlineGap - pillH) : (sy + 6);
            } else {
                pillY = (sy + sh + outlineGap + pillH <= H - 4) ? (sy + sh + outlineGap) : (sy + sh - pillH - 6);
            }
            pillY = std::max(4, std::min(H - pillH - 4, pillY));

            RECT candidatePill = { pillX, pillY, pillX + pillW, pillY + pillH };
            RECT overlapRc;
            if (IntersectRect(&overlapRc, &candidatePill, &g_Daemon.hudBoundsRect)) {
                if (!pillBottom && pillY < sy) {
                    pillY = sy + 6;
                } else if (pillBottom && pillY > sy + sh) {
                    pillY = std::max(4, sy + sh - pillH - 6);
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

            if (cHovered && !g_Daemon.isEditingStroke && g_Daemon.hoveredBtnId == -1 && g_Daemon.dragMode == DragMode::None) {
                DockButton fakeTip;
                fakeTip.rect = g_Daemon.customStrokeRect;
                fakeTip.tooltip = L"Custom size — click to type custom size (1–120px)";
                fakeTip.isTool = false;
                PepperSnapDaemon::DrawHoverBubbleTooltip(g, fakeTip, W, H);
            }
        }

        // Render Floating Speech-Bubble Tooltip when hovering any button (hidden while dragging)
        if (g_Daemon.hoveredBtnId != -1 && g_Daemon.dragMode == DragMode::None) {
            for (const auto& b : g_Daemon.dockButtons) {
                if (b.id == g_Daemon.hoveredBtnId) {
                    PepperSnapDaemon::DrawHoverBubbleTooltip(g, b, W, H);
                    break;
                }
            }
        }
    } else {
        if (g_Daemon.hasCtrlHoverWindow) {
            int wx = std::max(0, std::min(W, (int)g_Daemon.ctrlHoverWindowRect.left));
            int wy = std::max(0, std::min(H, (int)g_Daemon.ctrlHoverWindowRect.top));
            int ww = std::min(W - wx, (int)(g_Daemon.ctrlHoverWindowRect.right - g_Daemon.ctrlHoverWindowRect.left));
            int wh = std::min(H - wy, (int)(g_Daemon.ctrlHoverWindowRect.bottom - g_Daemon.ctrlHoverWindowRect.top));
            if (ww >= 1 && wh >= 1) {
                Pen winBorder(Color(255, 239, 68, 68), 2.0f);
                winBorder.SetAlignment(PenAlignmentInset);
                int drawX = std::max(0, wx);
                int drawY = std::max(0, wy);
                int drawR = std::min(W - 1, wx + ww);
                int drawB = std::min(H - 1, wy + wh);
                g.DrawRectangle(&winBorder, drawX, drawY, std::max(1, drawR - drawX), std::max(1, drawB - drawY));

                int pillW = 140;
                int pillH = 23;
                const int outlineGap = 8;
                int pillX = std::max(5, std::min(W - pillW - 5, wx + 5));
                int pillY = (wy >= pillH + outlineGap + 4) ? (wy - outlineGap - pillH) : (wy + 6);
                pillY = std::max(4, std::min(H - pillH - 4, pillY));

                SolidBrush pillBg(Color(235, 15, 23, 42));
                g.FillRectangle(&pillBg, pillX, pillY, pillW, pillH);
                Pen pillBorder(Color(160, 71, 85, 105), 1.4f);
                g.DrawRectangle(&pillBorder, pillX, pillY, pillW, pillH);

                std::wstring dimValStr = std::to_wstring(ww) + L" × " + std::to_wstring(wh);
                PepperSnapDaemon::DrawEditablePillText(
                    g, monoFont, pillX + 7, pillY + 4, pillY, pillH,
                    dimValStr, L" px", false, 0, 0
                );
            }
        } else {
            Pen crossPen(Color(150, 239, 68, 68), 1.0f);
            crossPen.SetDashStyle(DashStyleDash);
            g.DrawLine(&crossPen, g_Daemon.mousePt.x, 0, g_Daemon.mousePt.x, H);
            g.DrawLine(&crossPen, 0, g_Daemon.mousePt.y, W, g_Daemon.mousePt.y);
        }
    }

    // 5x Magnifier Loupe when aiming
    if (!g_Daemon.hasSelection || g_Daemon.dragMode == DragMode::CreatingSelection) {
        int mx = g_Daemon.mousePt.x, my = g_Daemon.mousePt.y;
        const wchar_t* ctrlHintText = L"Hold Ctrl to select window";
        RectF hintBounds;
        g.MeasureString(ctrlHintText, -1, &smallFont, PointF(0.0f, 0.0f), &hintBounds);
        int hintPillW = (int)std::ceil(hintBounds.Width) + 6;
        int hintPillH = 18;
        int imgW = std::max(120, ((hintPillW + 2) + 1) & ~1);
        int imgH = 90;
        int loupeW = imgW + 12;
        int loupeH = 135 + hintPillH + 6;

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
                StretchBlt(memDC, lx + 6, ly + 6, imgW, imgH, g_Daemon.hBrightDesktopDC, srcX, srcY, 22, 16, SRCCOPY);
            } else {
                GraphicsState st = g.Save();
                g.SetInterpolationMode(InterpolationModeNearestNeighbor);
                g.SetPixelOffsetMode(PixelOffsetModeHalf);
                g.DrawImage(g_Daemon.frozenDesktopBmp, Rect(lx + 6, ly + 6, imgW, imgH), mx - 11, my - 8, 22, 16, UnitPixel);
                g.Restore(st);
            }

            Pen centerCross(Color(200, 239, 68, 68), 1.5f);
            int crossX = lx + 6 + imgW / 2;
            int crossY = ly + 6 + imgH / 2;
            g.DrawLine(&centerCross, crossX, ly + 6, crossX, ly + 6 + imgH);
            g.DrawLine(&centerCross, lx + 6, crossY, lx + 6 + imgW, crossY);

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
            g.DrawString(hexBuf, -1, &monoFont, PointF((float)(lx + 7), (float)(ly + 101)), &whiteBrush);
            g.DrawString(L"Ctrl+C copy RGB · Esc", -1, &smallFont, PointF((float)(lx + 7), (float)(ly + 117)), &mutedBrush);

            // White font with red highlighted background inside the magnifier box below 'Ctrl+C copy RGB · Esc'
            int hintX = lx + 7;
            int hintY = ly + 135;
            SolidBrush redHighlightBg(Color(255, 239, 68, 68));
            g.FillRectangle(&redHighlightBg, hintX, hintY, hintPillW, hintPillH);
            StringFormat hintSf;
            hintSf.SetAlignment(StringAlignmentCenter);
            hintSf.SetLineAlignment(StringAlignmentCenter);
            RectF hintRc((float)hintX, (float)hintY, (float)hintPillW, (float)hintPillH);
            g.DrawString(ctrlHintText, -1, &smallFont, hintRc, &hintSf, &whiteBrush);
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
            SetTimer(hWnd, 2, 40, nullptr);  // Ctrl-key state poll for automatic window hover selection
            SetFocus(hWnd);
            return 0;

        case WM_ACTIVATE:
            if (LOWORD(wParam) != WA_INACTIVE) {
                SetFocus(hWnd);
            }
            return 0;

        case WM_TIMER:
            if (wParam == 1) {
                if (g_Daemon.isEditingText || g_Daemon.isEditingSize || g_Daemon.isEditingStroke) {
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
            } else if (wParam == 2) {
                g_Daemon.SuppressAboveOverlayWindows();
                if ((!g_Daemon.hOptionsWnd || !IsWindow(g_Daemon.hOptionsWnd)) &&
                    (!g_Daemon.hShortcutsWnd || !IsWindow(g_Daemon.hShortcutsWnd))) {
                    SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                    if (GetForegroundWindow() != hWnd) {
                        LockSetForegroundWindow(LSFW_UNLOCK);
                        SetForegroundWindow(hWnd);
                        SetActiveWindow(hWnd);
                        SetFocus(hWnd);
                    }
                }
                if (!g_Daemon.hasSelection && g_Daemon.dragMode == DragMode::None) {
                    if (g_Daemon.UpdateCtrlWindowHover()) {
                        g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                        InvalidateRect(hWnd, nullptr, FALSE);
                    }
                }
            }
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
                            g_Daemon.hoveredBtnId = -1;
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
                        case DBTN_ACT_OCR:       g_Daemon.ActionOcrAndClose(); return 0;
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

            // 4. Outside Selection Box -> Check Ctrl + click automatic window selection first!
            bool ctrlHeld = ((GetKeyState(VK_CONTROL) & 0x8000) != 0) || ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
            if (!g_Daemon.hasSelection && ctrlHeld) {
                g_Daemon.UpdateCtrlWindowHover();
                if (g_Daemon.hasCtrlHoverWindow) {
                    g_Daemon.hasSelection = true;
                    g_Daemon.selRect = g_Daemon.ctrlHoverWindowRect;
                    g_Daemon.hasCtrlHoverWindow = false;
                    g_Daemon.hasCustomHudPos = false;
                    g_Daemon.activeTool = OverlayTool::SelectMove;
                    g_Daemon.annotations.clear();
                    g_Daemon.undoStack.clear();
                    g_Daemon.redoStack.clear();
                    g_Daemon.nextStepNum = 1;
                    g_Daemon.selectedAnnotationId = -1;
                    g_Daemon.dragMode = DragMode::None;
                    g_Daemon.UpdateOverlayCursor(mx, my);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
            }

            // Do NOT reset selection on simple click or click-and-hold!
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
                if (!g_Daemon.hasSelection) {
                    g_Daemon.UpdateCtrlWindowHover();
                } else {
                    g_Daemon.hasCtrlHoverWindow = false;
                }
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
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0 || (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0 || (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

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
            if (wParam == VK_CONTROL) {
                if (!g_Daemon.hasSelection && g_Daemon.dragMode == DragMode::None) {
                    if (g_Daemon.UpdateCtrlWindowHover()) {
                        g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                        InvalidateRect(hWnd, nullptr, FALSE);
                    }
                }
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
                case 'O': g_Daemon.ActionOcrAndClose(); return 0;
                default: break;
            }
            g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_KEYUP: {
            if (wParam == VK_CONTROL) {
                if (g_Daemon.UpdateCtrlWindowHover()) {
                    g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
            break;
        }

        case WM_DESTROY:
            KillTimer(hWnd, 1);
            KillTimer(hWnd, 2);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

struct EnumDesktopWindowsCtx {
    PepperSnapDaemon* daemon;
    std::vector<RECT>* rects;
};

struct CandidateWindowEntry {
    HWND hwnd = nullptr;
    int layerTier = 2; // 0 = frontmost notifications/sidebars/flyouts/menus, 1 = topmost/taskbars, 2 = standard windows
    int zOrderIdx = 0;
};

static bool IsSameOrNearlySameRect(const RECT& a, const RECT& b, LONG tol = 3) {
    return std::abs(a.left - b.left) <= tol &&
           std::abs(a.top - b.top) <= tol &&
           std::abs(a.right - b.right) <= tol &&
           std::abs(a.bottom - b.bottom) <= tol;
}

static void PushUniqueWindowRect(std::vector<RECT>& outRects, const RECT& rc) {
    if (rc.right - rc.left < 16 || rc.bottom - rc.top < 16) return;
    for (const auto& existing : outRects) {
        if (IsSameOrNearlySameRect(existing, rc, 3)) {
            return;
        }
    }
    outRects.push_back(rc);
}

static RECT ClampScreenRectToOverlay(const PepperSnapDaemon* d, RECT rc) {
    rc.left   -= d->vScreenX;
    rc.top    -= d->vScreenY;
    rc.right  -= d->vScreenX;
    rc.bottom -= d->vScreenY;

    rc.left   = std::max(0L, std::min((LONG)d->vScreenW, rc.left));
    rc.top    = std::max(0L, std::min((LONG)d->vScreenH, rc.top));
    rc.right  = std::max(0L, std::min((LONG)d->vScreenW, rc.right));
    rc.bottom = std::max(0L, std::min((LONG)d->vScreenH, rc.bottom));
    return rc;
}

// Extract visible sub-panels (such as Windows 11 Notification Center cards, Calendar flyout,
// Toast notification popups, Emoji/Clipboard popups, etc.) from large CoreWindow / XAML Island hosts
static void ExtractUiaSubWindowRects(
    IUIAutomation* pUia,
    IUIAutomationTreeWalker* pWalker,
    HWND hwnd,
    const RECT& hostOverlayRc,
    const PepperSnapDaemon* d,
    bool isFullscreenHost,
    std::vector<RECT>& outSubRects
) {
    if (!pUia || !pWalker || !hwnd) return;
    IUIAutomationElement* pRoot = nullptr;
    if (FAILED(pUia->ElementFromHandle((UIA_HWND)hwnd, &pRoot)) || !pRoot) return;

    LONG hostW = hostOverlayRc.right - hostOverlayRc.left;
    LONG hostH = hostOverlayRc.bottom - hostOverlayRc.top;
    long long hostArea = (long long)hostW * (long long)hostH;

    auto collectLevelPanels = [&](IUIAutomationElement* parentElem, auto& selfRef, int depth) -> void {
        if (!parentElem || depth > 3) return;
        IUIAutomationElement* pChild = nullptr;
        if (FAILED(pWalker->GetFirstChildElement(parentElem, &pChild)) || !pChild) return;

        while (pChild) {
            WINBOOL isOffscreen = FALSE;
            RECT childScreenRc = {0, 0, 0, 0};
            CONTROLTYPEID ctrlType = 0;
            bool okBounds = SUCCEEDED(pChild->get_CurrentBoundingRectangle(&childScreenRc));
            pChild->get_CurrentIsOffscreen(&isOffscreen);
            pChild->get_CurrentControlType(&ctrlType);

            if (okBounds && !isOffscreen) {
                RECT cRc = ClampScreenRectToOverlay(d, childScreenRc);
                LONG cw = cRc.right - cRc.left;
                LONG ch = cRc.bottom - cRc.top;
                long long cArea = (long long)cw * (long long)ch;

                if (cw >= 48 && ch >= 28) {
                    // If this child is a full-size wrapper around the host (>= 92% of host area), unwrap it
                    if (hostArea > 0 && cArea >= (hostArea * 92) / 100) {
                        selfRef(pChild, selfRef, depth + 1);
                    } else {
                        bool isContainerType =
                            ctrlType == UIA_WindowControlTypeId ||
                            ctrlType == UIA_PaneControlTypeId ||
                            ctrlType == UIA_GroupControlTypeId ||
                            ctrlType == UIA_CustomControlTypeId ||
                            ctrlType == UIA_MenuControlTypeId ||
                            ctrlType == UIA_ToolTipControlTypeId ||
                            ctrlType == UIA_ToolBarControlTypeId ||
                            ctrlType == UIA_ListControlTypeId;

                        bool validPanelSize = isFullscreenHost
                            ? (cw >= 80 && ch >= 32 && cArea < (hostArea * 85) / 100)
                            : (cw >= (hostW * 65) / 100 && ch >= 40 && ch < (hostH * 94) / 100);

                        if (isContainerType && validPanelSize) {
                            PushUniqueWindowRect(outSubRects, cRc);
                        } else if (depth < 2 && cArea >= (hostArea * 25) / 100) {
                            selfRef(pChild, selfRef, depth + 1);
                        }
                    }
                }
            }

            IUIAutomationElement* pNext = nullptr;
            if (FAILED(pWalker->GetNextSiblingElement(pChild, &pNext))) {
                pNext = nullptr;
            }
            pChild->Release();
            pChild = pNext;
        }
    };

    collectLevelPanels(pRoot, collectLevelPanels, 1);
    pRoot->Release();
}

static BOOL CALLBACK CollectEnumWindowsHwndProc(HWND hwnd, LPARAM lParam) {
    std::vector<HWND>* list = (std::vector<HWND>*)lParam;
    if (!list || !hwnd) return TRUE;
    if (std::find(list->begin(), list->end(), hwnd) == list->end()) {
        list->push_back(hwnd);
    }
    return TRUE;
}

static BOOL CALLBACK EnumMonitorWorkAreaProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) {
    EnumDesktopWindowsCtx* ctx = (EnumDesktopWindowsCtx*)lParam;
    if (!ctx || !ctx->daemon || !ctx->rects) return TRUE;
    PepperSnapDaemon* d = ctx->daemon;

    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (!GetMonitorInfoW(hMonitor, &mi)) return TRUE;

    // mi.rcWork is the monitor's desktop area excluding the Windows taskbar
    RECT rc = mi.rcWork;
    HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (hTaskbar && IsWindowVisible(hTaskbar)) {
        RECT tbRc = {0, 0, 0, 0};
        if (GetWindowRect(hTaskbar, &tbRc)) {
            SubtractRect(&rc, &rc, &tbRc);
        }
    }
    HWND hSecTaskbar = nullptr;
    while ((hSecTaskbar = FindWindowExW(nullptr, hSecTaskbar, L"Shell_SecondaryTrayWnd", nullptr)) != nullptr) {
        if (IsWindowVisible(hSecTaskbar)) {
            RECT tbRc = {0, 0, 0, 0};
            if (GetWindowRect(hSecTaskbar, &tbRc)) {
                SubtractRect(&rc, &rc, &tbRc);
            }
        }
    }

    rc = ClampScreenRectToOverlay(d, rc);
    PushUniqueWindowRect(*ctx->rects, rc);
    return TRUE;
}

void PepperSnapDaemon::SnapshotDesktopWindows() {
    desktopWindowRects.clear();
    hasCtrlHoverWindow = false;
    ctrlHoverWindowRect = {0, 0, 0, 0};

    // 1. Collect all top-level windows across ALL Z-order bands (ZBID_DESKTOP, ZBID_IMMERSIVE_NOTIFICATIONS,
    //    ZBID_IMMERSIVE_MOGO, ZBID_IMMERSIVE_SEARCH, ZBID_SYSTEM_TOOLS, ZBID_ABOVELOCK_UX, etc.)
    std::vector<HWND> rawHwnds;
    rawHwnds.reserve(256);

    // FindWindowExW(nullptr, prev, nullptr, nullptr) traverses the desktop's global child list across all ZBID bands
    HWND hCur = nullptr;
    for (int iter = 0; iter < 4096; ++iter) {
        hCur = FindWindowExW(nullptr, hCur, nullptr, nullptr);
        if (!hCur) break;
        if (std::find(rawHwnds.begin(), rawHwnds.end(), hCur) == rawHwnds.end()) {
            rawHwnds.push_back(hCur);
        }
    }

    // Also run EnumWindows & EnumDesktopWindows to ensure no desktop window is missed
    EnumWindows(CollectEnumWindowsHwndProc, (LPARAM)&rawHwnds);
    EnumDesktopWindows(GetThreadDesktop(GetCurrentThreadId()), CollectEnumWindowsHwndProc, (LPARAM)&rawHwnds);

    typedef BOOL (WINAPI *PFN_GetWindowBand)(HWND, PDWORD);
    static PFN_GetWindowBand s_fnGetWindowBand = nullptr;
    static bool s_resolvedBandFn = false;
    if (!s_resolvedBandFn) {
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32) {
            s_fnGetWindowBand = (PFN_GetWindowBand)GetProcAddress(hUser32, "GetWindowBand");
        }
        s_resolvedBandFn = true;
    }

    std::vector<CandidateWindowEntry> candidates;
    candidates.reserve(rawHwnds.size());

    for (size_t i = 0; i < rawHwnds.size(); ++i) {
        HWND hwnd = rawHwnds[i];
        if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd) || IsIconic(hwnd)) continue;
        if (hwnd == hOverlayWnd || hwnd == hTrayWnd) continue;

        HWND hRootOwner = GetAncestor(hwnd, GA_ROOTOWNER);
        if (hRootOwner && hRootOwner != hwnd && IsIconic(hRootOwner)) continue;

        // Skip cloaked Windows 10/11 UWP / background shell windows (DWMWA_CLOAKED = 14)
        DWORD cloaked = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(hwnd, 14, &cloaked, sizeof(cloaked))) && cloaked != 0) {
            continue;
        }

        LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        if (exStyle & WS_EX_LAYERED) {
            BYTE bAlpha = 255;
            DWORD dwFlags = 0;
            if (GetLayeredWindowAttributes(hwnd, nullptr, &bAlpha, &dwFlags) &&
                (dwFlags & LWA_ALPHA) && bAlpha < 15) {
                continue;
            }
        }

        WCHAR clsName[128] = {0};
        GetClassNameW(hwnd, clsName, 127);
        if (wcscmp(clsName, L"Progman") == 0 ||
            wcscmp(clsName, L"WorkerW") == 0 ||
            wcscmp(clsName, L"DummyDWMListenerWindow") == 0 ||
            wcscmp(clsName, L"EdgeUiInputTopWndClass") == 0 ||
            wcscmp(clsName, L"EdgeUiInputWndClass") == 0 ||
            wcscmp(clsName, L"IME") == 0 ||
            wcscmp(clsName, L"Default IME") == 0 ||
            wcscmp(clsName, L"MSCTFIME UI") == 0 ||
            wcscmp(clsName, L"SysShadow") == 0 ||
            wcscmp(clsName, L"PepperSnapOverlayWnd") == 0 ||
            wcscmp(clsName, L"PepperSnapTrayDaemonClass") == 0) {
            continue;
        }

        DWORD band = 1; // ZBID_DESKTOP = 1
        if (s_fnGetWindowBand) {
            s_fnGetWindowBand(hwnd, &band);
        }

        bool isTaskbar = (wcscmp(clsName, L"Shell_TrayWnd") == 0 ||
                          wcscmp(clsName, L"Shell_SecondaryTrayWnd") == 0);
        bool isCoreOrIsland = (wcscmp(clsName, L"Windows.UI.Core.CoreWindow") == 0 ||
                               wcscmp(clsName, L"XamlExplorerHostIslandWindow") == 0 ||
                               wcscmp(clsName, L"ControlCenterWindow") == 0 ||
                               wcscmp(clsName, L"TopLevelWindowForOverflowXamlIsland") == 0 ||
                               wcscmp(clsName, L"Xaml_WindowedPopupClass") == 0 ||
                               wcscmp(clsName, L"PopupHost") == 0 ||
                               wcscmp(clsName, L"NotifyIconOverflowWindow") == 0 ||
                               wcscmp(clsName, L"tooltips_class32") == 0 ||
                               wcscmp(clsName, L"#32768") == 0);

        int tier = 2;
        if (!isTaskbar && (isCoreOrIsland ||
            band == 2  /* ZBID_UIACCESS */ ||
            band == 3  /* ZBID_IMMERSIVE_IHM */ ||
            band == 4  /* ZBID_IMMERSIVE_NOTIFICATIONS */ ||
            band == 6  /* ZBID_IMMERSIVE_MOGO */ ||
            band == 13 /* ZBID_IMMERSIVE_SEARCH */ ||
            band == 16 /* ZBID_SYSTEM_TOOLS */ ||
            band == 18 /* ZBID_ABOVELOCK_UX */)) {
            tier = 0; // Frontmost notifications, notification sidebar, flyouts, popups, menus
        } else if (isTaskbar || (exStyle & WS_EX_TOPMOST) != 0) {
            tier = 1; // Topmost windows & taskbars
        }

        candidates.push_back({ hwnd, tier, (int)i });
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const CandidateWindowEntry& a, const CandidateWindowEntry& b) {
        if (a.layerTier != b.layerTier) return a.layerTier < b.layerTier;
        return a.zOrderIdx < b.zOrderIdx;
    });

    // Lazily initialize UI Automation only if we encounter a large CoreWindow / XAML Island host
    IUIAutomation* pUia = nullptr;
    IUIAutomationTreeWalker* pWalker = nullptr;
    bool triedUiaInit = false;

    auto ensureUia = [&]() -> bool {
        if (!triedUiaInit) {
            triedUiaInit = true;
            static const GUID CLSID_CUIAutomation_Local = { 0xff48dba4, 0x60ef, 0x4201, { 0xaa, 0x87, 0x54, 0x10, 0x3e, 0xef, 0x59, 0x4e } };
            static const GUID IID_IUIAutomation_Local   = { 0x30cbe57d, 0xd9d0, 0x452a, { 0xab, 0x13, 0x7a, 0xc5, 0xac, 0x48, 0x25, 0xee } };
            if (SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation_Local, nullptr, CLSCTX_INPROC_SERVER, IID_IUIAutomation_Local, (void**)&pUia)) && pUia) {
                pUia->get_ControlViewWalker(&pWalker);
            }
        }
        return (pUia != nullptr && pWalker != nullptr);
    };

    for (const auto& cand : candidates) {
        HWND hwnd = cand.hwnd;
        WCHAR clsName[128] = {0};
        GetClassNameW(hwnd, clsName, 127);
        WCHAR winTitle[256] = {0};
        GetWindowTextW(hwnd, winTitle, 255);

        RECT rawRc = {0, 0, 0, 0};
        if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rawRc, sizeof(rawRc))) ||
            (rawRc.right - rawRc.left) <= 0 || (rawRc.bottom - rawRc.top) <= 0) {
            if (!GetWindowRect(hwnd, &rawRc)) continue;
        }

        RECT rc = ClampScreenRectToOverlay(this, rawRc);
        LONG w = rc.right - rc.left;
        LONG h = rc.bottom - rc.top;
        if (w < 16 || h < 16) continue;

        HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(MONITORINFO) };
        LONG monW = vScreenW, monH = vScreenH;
        if (hMon && GetMonitorInfoW(hMon, &mi)) {
            monW = std::max(1L, mi.rcMonitor.right - mi.rcMonitor.left);
            monH = std::max(1L, mi.rcMonitor.bottom - mi.rcMonitor.top);
        }

        LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        bool isFullscreenMon = (w >= (monW * 85) / 100 && h >= (monH * 85) / 100);
        bool isTallColumn    = (h >= (monH * 75) / 100);

        // Skip fullscreen click-through transparent overlays (while keeping non-fullscreen notification balloons/popups!)
        if ((exStyle & WS_EX_TRANSPARENT) && isFullscreenMon) {
            continue;
        }

        // Skip untitled fullscreen background host windows on the desktop
        if (w >= vScreenW && h >= vScreenH && winTitle[0] == L'\0' &&
            wcscmp(clsName, L"Windows.UI.Core.CoreWindow") != 0 &&
            wcscmp(clsName, L"XamlExplorerHostIslandWindow") != 0) {
            continue;
        }

        bool isCoreOrXamlHost = (wcscmp(clsName, L"Windows.UI.Core.CoreWindow") == 0 ||
                                 wcscmp(clsName, L"XamlExplorerHostIslandWindow") == 0 ||
                                 wcscmp(clsName, L"TopLevelWindowForOverflowXamlIsland") == 0);

        if (isCoreOrXamlHost && (isFullscreenMon || isTallColumn)) {
            std::vector<RECT> subRects;
            if (ensureUia()) {
                ExtractUiaSubWindowRects(pUia, pWalker, hwnd, rc, this, isFullscreenMon, subRects);
            }

            if (!subRects.empty()) {
                RECT unionRc = subRects[0];
                for (const auto& sr : subRects) {
                    PushUniqueWindowRect(desktopWindowRects, sr);
                    unionRc.left   = std::min(unionRc.left, sr.left);
                    unionRc.top    = std::min(unionRc.top, sr.top);
                    unionRc.right  = std::max(unionRc.right, sr.right);
                    unionRc.bottom = std::max(unionRc.bottom, sr.bottom);
                }
                if (subRects.size() > 1) {
                    PushUniqueWindowRect(desktopWindowRects, unionRc);
                }
            }

            // If this is a fullscreen transparent shell canvas (like "Windows Input Experience" TextInputHost.exe),
            // do NOT push the fullscreen container rect itself—only its active child popup rects above.
            bool isInputExperienceCanvas =
                isFullscreenMon &&
                (wcsstr(winTitle, L"Input Experience") != nullptr ||
                 wcsstr(winTitle, L"Text Input") != nullptr ||
                 winTitle[0] == L'\0' ||
                 wcscmp(clsName, L"XamlExplorerHostIslandWindow") == 0);

            if (!isInputExperienceCanvas) {
                PushUniqueWindowRect(desktopWindowRects, rc);
            }
            continue;
        }

        PushUniqueWindowRect(desktopWindowRects, rc);
    }

    if (pWalker) pWalker->Release();
    if (pUia) pUia->Release();

    // Append each monitor's desktop work area (desktop screen without taskbar) at the back of Z-order
    EnumDesktopWindowsCtx ctx{ this, &desktopWindowRects };
    EnumDisplayMonitors(nullptr, nullptr, EnumMonitorWorkAreaProc, (LPARAM)&ctx);
}

void PepperSnapDaemon::SuppressAboveOverlayWindows() {
    typedef BOOL (WINAPI *PFN_GetWindowBand)(HWND, PDWORD);
    static PFN_GetWindowBand s_fnGetWindowBand = nullptr;
    static bool s_resolvedBandFn = false;
    if (!s_resolvedBandFn) {
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32) {
            s_fnGetWindowBand = (PFN_GetWindowBand)GetProcAddress(hUser32, "GetWindowBand");
        }
        s_resolvedBandFn = true;
    }

    HWND hCur = nullptr;
    for (int iter = 0; iter < 1024; ++iter) {
        hCur = FindWindowExW(nullptr, hCur, nullptr, nullptr);
        if (!hCur) break;
        if (!IsWindow(hCur) || !IsWindowVisible(hCur) || IsIconic(hCur)) continue;
        if (hCur == hOverlayWnd || hCur == hOptionsWnd || hCur == hShortcutsWnd || hCur == hTrayWnd) continue;
        if (std::find(pinnedWindows.begin(), pinnedWindows.end(), hCur) != pinnedWindows.end()) continue;

        DWORD cloaked = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(hCur, 14 /* DWMWA_CLOAKED */, &cloaked, sizeof(cloaked))) && cloaked != 0) {
            continue;
        }

        WCHAR clsName[128] = {0};
        GetClassNameW(hCur, clsName, 127);
        if (wcscmp(clsName, L"Progman") == 0 ||
            wcscmp(clsName, L"WorkerW") == 0 ||
            wcscmp(clsName, L"Shell_TrayWnd") == 0 ||
            wcscmp(clsName, L"Shell_SecondaryTrayWnd") == 0 ||
            wcscmp(clsName, L"PepperSnapOverlayWnd") == 0 ||
            wcscmp(clsName, L"PepperSnapOptionsModal") == 0 ||
            wcscmp(clsName, L"PepperSnapShortcutsModal") == 0 ||
            wcscmp(clsName, L"PepperSnapPinWnd") == 0 ||
            wcscmp(clsName, L"PepperSnapTrayDaemonClass") == 0) {
            continue;
        }

        DWORD band = 1; // ZBID_DESKTOP = 1
        if (s_fnGetWindowBand) {
            s_fnGetWindowBand(hCur, &band);
        }

        bool isHigherBand = (band > 1);
        bool isShellNotificationOrFlyout =
            wcscmp(clsName, L"Windows.UI.Core.CoreWindow") == 0 ||
            wcscmp(clsName, L"XamlExplorerHostIslandWindow") == 0 ||
            wcscmp(clsName, L"ControlCenterWindow") == 0 ||
            wcscmp(clsName, L"TopLevelWindowForOverflowXamlIsland") == 0 ||
            wcscmp(clsName, L"Xaml_WindowedPopupClass") == 0 ||
            wcscmp(clsName, L"PopupHost") == 0 ||
            wcscmp(clsName, L"NotifyIconOverflowWindow") == 0 ||
            wcscmp(clsName, L"tooltips_class32") == 0;

        if (!isHigherBand && !isShellNotificationOrFlyout) continue;

        if (std::find(suppressedAboveOverlayWindows.begin(), suppressedAboveOverlayWindows.end(), hCur) == suppressedAboveOverlayWindows.end()) {
            suppressedAboveOverlayWindows.push_back(hCur);
        }

        // Clip the live higher-band window to an empty 0x0 region and hide it so hOverlayWnd (which already captured
        // its frozen pixels and selectable rects) renders on the very top without obstruction
        HRGN hEmptyRgn = CreateRectRgn(0, 0, 0, 0);
        if (hEmptyRgn) {
            if (!SetWindowRgn(hCur, hEmptyRgn, TRUE)) {
                DeleteObject(hEmptyRgn);
            }
        }
        SetWindowPos(hCur, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_HIDEWINDOW);
        ShowWindow(hCur, SW_HIDE);
        PostMessageW(hCur, WM_ACTIVATE, WA_INACTIVE, 0);
        PostMessageW(hCur, WM_CANCELMODE, 0, 0);
    }
}

void PepperSnapDaemon::RestoreSuppressedAboveOverlayWindows() {
    for (HWND hwnd : suppressedAboveOverlayWindows) {
        if (hwnd && IsWindow(hwnd)) {
            SetWindowRgn(hwnd, nullptr, TRUE);
            ShowWindowAsync(hwnd, SW_SHOWNOACTIVATE);
        }
    }
    suppressedAboveOverlayWindows.clear();
}

bool PepperSnapDaemon::UpdateCtrlWindowHover() {
    bool prevHas = hasCtrlHoverWindow;
    RECT prevRc = ctrlHoverWindowRect;

    bool ctrlHeld = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    if (!ctrlHeld || hasSelection || dragMode != DragMode::None) {
        hasCtrlHoverWindow = false;
        return (prevHas != hasCtrlHoverWindow);
    }

    bool found = false;
    RECT matched = {0, 0, 0, 0};
    for (const auto& rc : desktopWindowRects) {
        if (PtInRect(&rc, mousePt)) {
            found = true;
            matched = rc;
            break;
        }
    }
    hasCtrlHoverWindow = found;
    if (found) ctrlHoverWindowRect = matched;

    return (prevHas != hasCtrlHoverWindow) ||
           (found && !EqualRect(&prevRc, &ctrlHoverWindowRect));
}

void PepperSnapDaemon::StartRegionSnipOverlay(Bitmap* customBmp, const RECT* customSelRect) {
    if (hOverlayWnd) {
        if (customBmp || (hOptionsWnd && IsWindow(hOptionsWnd)) || (hShortcutsWnd && IsWindow(hShortcutsWnd))) {
            CloseRegionSnipOverlay();
        } else {
            SetForegroundWindow(hOverlayWnd);
            return;
        }
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
        desktopWindowRects.clear();
        hasCtrlHoverWindow = false;
        frozenDesktopBmp = customBmp;
        hasSelection = true;
        if (customSelRect) {
            selRect = *customSelRect;
        } else {
            int cw = std::min(vScreenW, (int)customBmp->GetWidth());
            int ch = std::min(vScreenH, (int)customBmp->GetHeight());
            int cx = (vScreenW - cw) / 2;
            int cy = (vScreenH - ch) / 2;
            selRect = { cx, cy, cx + cw, cy + ch };
        }
    } else {
        frozenDesktopBmp = CaptureVirtualDesktop();
        hasSelection = false;
        SnapshotDesktopWindows();
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
    if (!hasSelection) {
        UpdateCtrlWindowHover();
    }

    typedef HWND (WINAPI *PFN_CreateWindowInBand)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID, DWORD);
    typedef BOOL (WINAPI *PFN_SetWindowBand)(HWND, HWND, DWORD);
    static PFN_CreateWindowInBand s_fnCreateWindowInBand = nullptr;
    static PFN_SetWindowBand s_fnSetWindowBand = nullptr;
    static bool s_resolvedWinBandApis = false;
    if (!s_resolvedWinBandApis) {
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32) {
            s_fnCreateWindowInBand = (PFN_CreateWindowInBand)GetProcAddress(hUser32, "CreateWindowInBand");
            s_fnSetWindowBand      = (PFN_SetWindowBand)GetProcAddress(hUser32, "SetWindowBand");
        }
        s_resolvedWinBandApis = true;
    }

    hOverlayWnd = nullptr;
    if (s_fnCreateWindowInBand) {
        const DWORD candidateBands[] = { 16 /* ZBID_SYSTEM_TOOLS */, 4 /* ZBID_IMMERSIVE_NOTIFICATIONS */, 2 /* ZBID_UIACCESS */ };
        for (DWORD b : candidateBands) {
            hOverlayWnd = s_fnCreateWindowInBand(
                WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                L"PepperSnapOverlayWnd", L"PepperSnap Region Snip",
                WS_POPUP | WS_VISIBLE,
                vScreenX, vScreenY, vScreenW, vScreenH,
                nullptr, nullptr, hInst, nullptr, b
            );
            if (hOverlayWnd) break;
        }
    }
    if (!hOverlayWnd) {
        hOverlayWnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            L"PepperSnapOverlayWnd", L"PepperSnap Region Snip",
            WS_POPUP | WS_VISIBLE,
            vScreenX, vScreenY, vScreenW, vScreenH,
            nullptr, nullptr, hInst, nullptr
        );
    }
    if (hOverlayWnd && s_fnSetWindowBand) {
        if (!s_fnSetWindowBand(hOverlayWnd, HWND_TOPMOST, 16 /* ZBID_SYSTEM_TOOLS */)) {
            if (!s_fnSetWindowBand(hOverlayWnd, HWND_TOPMOST, 4 /* ZBID_IMMERSIVE_NOTIFICATIONS */)) {
                s_fnSetWindowBand(hOverlayWnd, HWND_TOPMOST, 2 /* ZBID_UIACCESS */);
            }
        }
    }

    // Immediately suppress/clip any live higher-band notification sidebars or notification pop-ups
    // (their pixels and selectable window rects are already captured in frozenDesktopBmp & desktopWindowRects)
    SuppressAboveOverlayWindows();

    SetWindowPos(hOverlayWnd, HWND_TOPMOST, vScreenX, vScreenY, vScreenW, vScreenH, SWP_SHOWWINDOW);
    UpdateOverlayCursor(mousePt.x, mousePt.y);

    // Unlock foreground activation so SetForegroundWindow succeeds even when a UWP CoreWindow (Notification Center / Start) had focus
    LockSetForegroundWindow(LSFW_UNLOCK);
    keybd_event(0, 0, KEYEVENTF_KEYUP, 0);

    HWND hFore = GetForegroundWindow();
    DWORD foreThread = hFore ? GetWindowThreadProcessId(hFore, nullptr) : 0;
    DWORD curThread = GetCurrentThreadId();
    if (foreThread && foreThread != curThread) {
        AttachThreadInput(foreThread, curThread, TRUE);
        BringWindowToTop(hOverlayWnd);
        SetForegroundWindow(hOverlayWnd);
        SetActiveWindow(hOverlayWnd);
        SetFocus(hOverlayWnd);
        AttachThreadInput(foreThread, curThread, FALSE);
    } else {
        BringWindowToTop(hOverlayWnd);
        SetForegroundWindow(hOverlayWnd);
        SetActiveWindow(hOverlayWnd);
        SetFocus(hOverlayWnd);
    }
}

void PepperSnapDaemon::CloseRegionSnipOverlay() {
    RecordLastCustomSelection();
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
    RestoreSuppressedAboveOverlayWindows();
    FreeOverlaySurfaceCache();
    if (frozenDesktopBmp) {
        delete frozenDesktopBmp;
        frozenDesktopBmp = nullptr;
    }
    hasSelection = false;
    hasCtrlHoverWindow = false;
    desktopWindowRects.clear();
    dragMode = DragMode::None;
}

void PepperSnapDaemon::OpenImageFileIntoOverlay(const std::wstring& filePath) {
    if (filePath.empty()) return;
    Bitmap loaded(filePath.c_str());
    if (loaded.GetLastStatus() != Ok) return;

    int sw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int sh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (sw <= 0 || sh <= 0) {
        sw = GetSystemMetrics(SM_CXSCREEN);
        sh = GetSystemMetrics(SM_CYSCREEN);
    }

    int imgW = (int)loaded.GetWidth();
    int imgH = (int)loaded.GetHeight();
    if (imgW <= 0 || imgH <= 0) return;

    int drawW = imgW;
    int drawH = imgH;
    if (drawW > sw || drawH > sh) {
        double scale = std::min((double)sw / (double)drawW, (double)sh / (double)drawH);
        drawW = std::max(1, (int)std::round(drawW * scale));
        drawH = std::max(1, (int)std::round(drawH * scale));
    }

    int imgX = (sw - drawW) / 2;
    int imgY = (sh - drawH) / 2;

    Bitmap* canvasBmp = new Bitmap(sw, sh, PixelFormat32bppARGB);
    Graphics g(canvasBmp);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    SolidBrush bg(Color(255, 15, 23, 42));
    g.FillRectangle(&bg, 0, 0, sw, sh);
    g.DrawImage(&loaded, imgX, imgY, drawW, drawH);

    RECT centeredSel = { imgX, imgY, imgX + drawW, imgY + drawH };
    StartRegionSnipOverlay(canvasBmp, &centeredSel);
}

void PepperSnapDaemon::OpenImageIntoOverlay() {
    WCHAR szFile[MAX_PATH] = {0};
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hTrayWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Image files (*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.gif;*.tif;*.tiff;*.ico)\0*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.gif;*.tif;*.tiff;*.ico\0All files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        OpenImageFileIntoOverlay(szFile);
    }
}

void PepperSnapDaemon::OpenImageFileToPinOnTop(const std::wstring& filePath) {
    if (filePath.empty()) return;
    Bitmap loaded(filePath.c_str());
    if (loaded.GetLastStatus() != Ok) return;

    int imgW = (int)loaded.GetWidth();
    int imgH = (int)loaded.GetHeight();
    if (imgW <= 0 || imgH <= 0) return;

    Bitmap* memBmp = new Bitmap(imgW, imgH, PixelFormat32bppARGB);
    {
        Graphics g(memBmp);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.DrawImage(&loaded, 0, 0, imgW, imgH);
    }

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    float initScale = 1.0f;
    int maxW = (int)(sw * 0.85);
    int maxH = (int)(sh * 0.80);
    if (imgW > maxW || imgH > maxH) {
        initScale = (float)std::min((double)maxW / (double)imgW, (double)maxH / (double)imgH);
        initScale = std::max(0.25f, initScale);
    }

    int dispW = (int)std::round(imgW * initScale);
    int dispH = (int)std::round(imgH * initScale);
    int px = (sw - dispW) / 2;
    int py = std::max(20, (sh - (dispH + 36)) / 2);

    CreatePinnedWindow(memBmp, px, py, initScale);
}

void PepperSnapDaemon::OpenImageToPinOnTop() {
    WCHAR szFile[MAX_PATH] = {0};
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hTrayWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Image files (*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.gif;*.tif;*.tiff;*.ico)\0*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.gif;*.tif;*.tiff;*.ico\0All files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        OpenImageFileToPinOnTop(szFile);
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

static std::string ExtractJsonFieldValue(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + pattern.size());
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    size_t endPos = pos + 1;
    while (endPos < json.size()) {
        if (json[endPos] == '"' && json[endPos - 1] != '\\') break;
        endPos++;
    }
    if (endPos >= json.size()) return "";
    return json.substr(pos + 1, endPos - (pos + 1));
}

static std::wstring Utf8ToWideStr(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (len <= 0) return L"";
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len);
    return out;
}

static DWORD WINAPI UpdateCheckWorkerThread(LPVOID lpParam) {
    bool manual = (lpParam != nullptr);
    UpdateCheckPayload* result = new UpdateCheckPayload();
    result->manualTrigger = manual;
    result->networkSuccess = false;

    std::wstring repo = g_Daemon.updateGithubRepo.empty() ? PepperSnapDaemon::DEFAULT_GITHUB_REPO : g_Daemon.updateGithubRepo;
    std::wstring apiUrl = L"https://api.github.com/repos/" + repo + L"/releases/latest";
    std::wstring fallbackHtmlUrl = L"https://github.com/" + repo + L"/releases/latest";
    result->releaseUrl = L"https://github.com/" + repo + L"/releases";

    std::wstring uaApp = L"PepperSnap/" + std::wstring(PepperSnapDaemon::APP_VERSION) + L" (Win32; +https://github.com)";
    std::wstring uaHdr = L"Accept: application/vnd.github+json\r\nUser-Agent: PepperSnap/" + std::wstring(PepperSnapDaemon::APP_VERSION) + L"\r\n";
    std::wstring uaWeb = L"User-Agent: PepperSnap/" + std::wstring(PepperSnapDaemon::APP_VERSION) + L"\r\n";

    HINTERNET hInet = InternetOpenW(
        uaApp.c_str(),
        INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0
    );
    if (hInet) {
        HINTERNET hUrl = InternetOpenUrlW(
            hInet, apiUrl.c_str(), uaHdr.c_str(), (DWORD)-1L,
            INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION,
            0
        );
        if (hUrl) {
            std::string body;
            char buf[4096];
            DWORD bytesRead = 0;
            while (InternetReadFile(hUrl, buf, sizeof(buf), &bytesRead) && bytesRead > 0) {
                body.append(buf, bytesRead);
                if (body.size() > 262144) break;
            }
            InternetCloseHandle(hUrl);

            std::string tag = ExtractJsonFieldValue(body, "tag_name");
            std::string htmlUrl = ExtractJsonFieldValue(body, "html_url");
            if (!tag.empty()) {
                result->networkSuccess = true;
                result->latestTag = Utf8ToWideStr(tag);
                if (!htmlUrl.empty()) {
                    result->releaseUrl = Utf8ToWideStr(htmlUrl);
                } else {
                    result->releaseUrl = fallbackHtmlUrl;
                }
            }
        }

        // Fallback: Check GitHub web redirect /releases/latest -> /releases/tag/<version>
        if (!result->networkSuccess) {
            HINTERNET hWeb = InternetOpenUrlW(
                hInet, fallbackHtmlUrl.c_str(), uaWeb.c_str(), (DWORD)-1L,
                INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE,
                0
            );
            if (hWeb) {
                WCHAR finalUrl[1024] = {0};
                DWORD urlLen = sizeof(finalUrl);
                if (InternetQueryOptionW(hWeb, INTERNET_OPTION_URL, finalUrl, &urlLen)) {
                    std::wstring fUrl(finalUrl);
                    size_t tagPos = fUrl.find(L"/releases/tag/");
                    if (tagPos != std::wstring::npos) {
                        result->latestTag = fUrl.substr(tagPos + 14);
                        result->releaseUrl = fUrl;
                        result->networkSuccess = !result->latestTag.empty();
                    }
                }
                InternetCloseHandle(hWeb);
            }
        }
        InternetCloseHandle(hInet);
    }

    if (g_Daemon.hTrayWnd) {
        PostMessageW(g_Daemon.hTrayWnd, WM_UPDATE_CHECK_RESULT, 0, (LPARAM)result);
    } else {
        delete result;
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

static void ShowTrayContextMenu(HWND hWnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();

    std::wstring regMenu  = L"Custom area\t" + PepperSnapDaemon::FormatHotkeyString(g_Daemon.hkRegionSnip);
    std::wstring fullMenu = L"Instant fullscreen\t" + PepperSnapDaemon::FormatHotkeyString(g_Daemon.hkFullSnap);
    std::wstring prevMenu = L"Instant save previous custom area\t" + PepperSnapDaemon::FormatHotkeyString(g_Daemon.hkPrevRegion);

    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_REGION,         regMenu.c_str());
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_FULL,           fullMenu.c_str());
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_PREV_REGION,    prevMenu.c_str());
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN_IMAGE,     L"Open image to edit with PepperSnap...");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN_PIN_IMAGE, L"Open image to pin on top...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN_FOLDER,   L"Open save folder...");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPTIONS,       L"Options...");
    AppendMenuW(hMenu, MF_STRING | (g_Daemon.IsRunAtStartupEnabled() ? MF_CHECKED : MF_UNCHECKED),
                IDM_TRAY_STARTUP_RUN, L"Launch PepperSnap at Windows startup");
    if (!g_Daemon.pinnedWindows.empty()) {
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_CLOSE_PINS, L"Close all pinned captures");
    }
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_CHECK_UPDATE,  L"Check for updates (GitHub releases)...");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SHORTCUTS,     L"Keyboard shortcuts and info...");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT,          L"Exit PepperSnap");

    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
    PostMessageW(hWnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

static LRESULT CALLBACK TrayDaemonWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TIMER:
            if (wParam == TIMER_AUTO_UPDATE_CHECK) {
                g_Daemon.MaybeRunScheduledUpdateCheck();
            }
            return 0;
        case WM_UPDATE_CHECK_RESULT: {
            g_Daemon.isCheckingUpdate = false;
            UpdateCheckPayload* res = (UpdateCheckPayload*)lParam;
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
                    if (ans == IDYES) {
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
                std::wstring msgText =
                    L"Could not find a newer release tag on GitHub releases (or no releases have been published yet).\n\n"
                    L"Current version: v" + std::wstring(PepperSnapDaemon::APP_VERSION) + L"\n"
                    L"Repository: " + g_Daemon.updateGithubRepo + L"\n\n"
                    L"Would you like to open the GitHub releases page in your browser?";
                int ans = MessageBoxW(
                    hOwner, msgText.c_str(), L"PepperSnap — check for updates",
                    MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND
                );
                if (ans == IDYES) {
                    ShellExecuteW(nullptr, L"open", res->releaseUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }
            }
            delete res;
            return 0;
        }
        case WM_COPYDATA: {
            COPYDATASTRUCT* cds = (COPYDATASTRUCT*)lParam;
            if (cds && cds->dwData == COPYDATA_OPEN_IMAGE && cds->lpData && cds->cbData >= sizeof(wchar_t)) {
                const wchar_t* wpath = (const wchar_t*)cds->lpData;
                size_t maxChars = cds->cbData / sizeof(wchar_t);
                std::wstring path(wpath, wcsnlen(wpath, maxChars));
                if (!path.empty()) {
                    g_Daemon.OpenImageFileIntoOverlay(path);
                }
                return TRUE;
            }
            break;
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
        case WM_HOTKEY:
            if (g_RecordingHotkeySlot != 0) return 0;
            if (wParam == HK_REGION_SNIP) PostMessageW(hWnd, WM_TRIGGER_REGION_SNIP, 0, 0);
            else if (wParam == HK_FULL_SNAP) PostMessageW(hWnd, WM_TRIGGER_FULL_SNAP, 0, 0);
            else if (wParam == HK_PREV_REGION) PostMessageW(hWnd, WM_TRIGGER_PREV_REGION, 0, 0);
            return 0;
        case WM_TRAYICON:
            if (lParam == WM_LBUTTONUP) g_Daemon.StartRegionSnipOverlay();
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
                case IDM_TRAY_REGION:         g_Daemon.StartRegionSnipOverlay(); break;
                case IDM_TRAY_FULL:           g_Daemon.InstantFullscreenCapture(); break;
                case IDM_TRAY_PREV_REGION:    g_Daemon.InstantPreviousRegionCapture(); break;
                case IDM_TRAY_OPEN_IMAGE:     g_Daemon.OpenImageIntoOverlay(); break;
                case IDM_TRAY_OPEN_PIN_IMAGE: g_Daemon.OpenImageToPinOnTop(); break;
                case IDM_TRAY_OPEN_FOLDER:
                    CreateDirectoryW(g_Daemon.saveFolder.c_str(), nullptr);
                    ShellExecuteW(nullptr, L"open", g_Daemon.saveFolder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    break;
                case IDM_TRAY_OPTIONS:        g_Daemon.ShowOptionsModal(); break;
                case IDM_TRAY_STARTUP_RUN:    g_Daemon.ToggleRunAtStartup(); break;
                case IDM_TRAY_CLOSE_PINS:
                    for (HWND hp : g_Daemon.pinnedWindows) if (IsWindow(hp)) DestroyWindow(hp);
                    g_Daemon.pinnedWindows.clear();
                    break;
                case IDM_TRAY_CHECK_UPDATE:   g_Daemon.CheckForUpdatesAsync(true); break;
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
            KillTimer(hWnd, TIMER_AUTO_UPDATE_CHECK);
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

    std::wstring cliEditFilePath;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            std::wstring low = arg;
            for (wchar_t& c : low) c = (wchar_t)towlower(c);
            if ((low == L"--edit" || low == L"/edit" || low == L"--open" || low == L"/open") && i + 1 < argc) {
                cliEditFilePath = argv[i + 1];
                break;
            } else if (!arg.empty() && arg[0] != L'-' && arg[0] != L'/' && GetFileAttributesW(arg.c_str()) != INVALID_FILE_ATTRIBUTES) {
                cliEditFilePath = arg;
                break;
            }
        }
        LocalFree(argv);
    }

    UINT cliMsg = 0;
    WPARAM cliWParam = 0;
    if (cmdArgs.find(L"--fullscreen") != std::wstring::npos || cmdArgs.find(L"/fullscreen") != std::wstring::npos) {
        cliMsg = WM_TRIGGER_FULL_SNAP;
    } else if (cmdArgs.find(L"--prevregion") != std::wstring::npos || cmdArgs.find(L"/prevregion") != std::wstring::npos) {
        cliMsg = WM_TRIGGER_PREV_REGION;
    } else if (cmdArgs.find(L"--options") != std::wstring::npos || cmdArgs.find(L"/options") != std::wstring::npos) {
        cliMsg = WM_COMMAND;
        cliWParam = IDM_TRAY_OPTIONS;
    } else if (cliEditFilePath.empty() && (cmdArgs.find(L"--open") != std::wstring::npos || cmdArgs.find(L"/open") != std::wstring::npos)) {
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

    std::wstring currentDaemonTitle = L"PepperSnap v" + std::wstring(PepperSnapDaemon::APP_VERSION);

    auto isSameVersionDaemon = [&](HWND hExisting) -> bool {
        if (!hExisting || !IsWindow(hExisting)) return false;
        WCHAR existingTitle[128] = {0};
        GetWindowTextW(hExisting, existingTitle, 127);
        return (wcscmp(existingTitle, currentDaemonTitle.c_str()) == 0);
    };

    auto forwardToRunningDaemon = [&](HWND hExisting) {
        if (!cliEditFilePath.empty()) {
            COPYDATASTRUCT cds = {0};
            cds.dwData = COPYDATA_OPEN_IMAGE;
            cds.cbData = (DWORD)((cliEditFilePath.size() + 1) * sizeof(wchar_t));
            cds.lpData = (PVOID)cliEditFilePath.c_str();
            SendMessageTimeoutW(hExisting, WM_COPYDATA, 0, (LPARAM)&cds, SMTO_ABORTIFHUNG, 3000, nullptr);
        } else if (cliMsg != 0) {
            PostMessageW(hExisting, cliMsg, cliWParam, 0);
        } else {
            PostMessageW(hExisting, WM_TRIGGER_REGION_SNIP, 0, 0);
        }
    };

    auto shutdownOlderDaemon = [&](HWND hOld) {
        if (!hOld || !IsWindow(hOld)) return;
        DWORD oldPid = 0;
        GetWindowThreadProcessId(hOld, &oldPid);
        SendMessageTimeoutW(hOld, WM_COMMAND, IDM_TRAY_EXIT, 0, SMTO_ABORTIFHUNG, 1000, nullptr);
        PostMessageW(hOld, WM_CLOSE, 0, 0);
        for (int waitIter = 0; waitIter < 25; ++waitIter) {
            if (!FindWindowW(L"PepperSnapTrayDaemonClass", nullptr)) break;
            Sleep(40);
        }
        if (oldPid != 0 && FindWindowW(L"PepperSnapTrayDaemonClass", nullptr) != nullptr) {
            HANDLE hProc = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, oldPid);
            if (hProc) {
                TerminateProcess(hProc, 0);
                WaitForSingleObject(hProc, 500);
                CloseHandle(hProc);
            }
        }
    };

    // If an existing PepperSnap instance of the SAME version is already running, forward CLI commands immediately
    HWND hExistingPreCheck = FindWindowW(L"PepperSnapTrayDaemonClass", nullptr);
    if (hExistingPreCheck && isSameVersionDaemon(hExistingPreCheck)) {
        forwardToRunningDaemon(hExistingPreCheck);
        return 0;
    }

    // Request Administrator elevation so hotkeys and screen capture work inside elevated / borderless fullscreen games
    if (!IsUserAnAdmin() && cmdArgs.find(L"--noelevate") == std::wstring::npos) {
        WCHAR selfPath[MAX_PATH] = {0};
        if (GetModuleFileNameW(nullptr, selfPath, MAX_PATH) > 0) {
            std::wstring args = lpCmdLine ? lpCmdLine : L"";
            if (!args.empty()) args += L" ";
            args += L"--noelevate";
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.lpVerb = L"runas";
            sei.lpFile = selfPath;
            sei.lpParameters = args.c_str();
            sei.nShow = SW_SHOWNORMAL;
            if (ShellExecuteExW(&sei)) {
                return 0;
            }
        }
    }

    // If an older version of PepperSnap is running in the tray, cleanly replace it with this newer version
    HWND hExistingCheck = FindWindowW(L"PepperSnapTrayDaemonClass", nullptr);
    if (hExistingCheck && !isSameVersionDaemon(hExistingCheck)) {
        shutdownOlderDaemon(hExistingCheck);
    }

    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Global\\PepperSnap_TrayDaemon_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExisting = FindWindowW(L"PepperSnapTrayDaemonClass", nullptr);
        if (hExisting) {
            if (isSameVersionDaemon(hExisting)) {
                forwardToRunningDaemon(hExisting);
                return 0;
            }
            shutdownOlderDaemon(hExisting);
        }
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
    wcPin.style = CS_HREDRAW | CS_VREDRAW;
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
    ChangeWindowMessageFilterEx(g_Daemon.hTrayWnd, WM_COMMAND, MSGFLT_ALLOW, nullptr);

    PepperSnapDaemon::RegisterImageContextMenu();
    g_Daemon.InitTrayIcon();
    g_Daemon.ApplyGlobalHotkeys();

    HANDLE hInputThread = CreateThread(nullptr, 0, InputHookWorkerThread, nullptr, 0, &g_InputHookThreadId);

    // Periodic background timer (every 30 minutes) + initial startup check for scheduled GitHub Releases updates
    SetTimer(g_Daemon.hTrayWnd, TIMER_AUTO_UPDATE_CHECK, 30 * 60 * 1000, nullptr);
    g_Daemon.MaybeRunScheduledUpdateCheck();

    if (!cliEditFilePath.empty()) {
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

    g_Daemon.DestroyCustomCursors();
    GdiplusShutdown(g_Daemon.gdiplusToken);
    CoUninitialize();
    if (hMutex) CloseHandle(hMutex);
    return (int)msg.wParam;
}

int main() {
    return wWinMain(GetModuleHandleW(nullptr), nullptr, GetCommandLineW(), SW_SHOWDEFAULT);
}
