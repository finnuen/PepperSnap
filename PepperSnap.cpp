#include "PepperSnap.h"

class PepperSnapDaemon {
public:
    HINSTANCE hInst = nullptr;
    HWND hTrayWnd = nullptr;
    HWND hOverlayWnd = nullptr;
    HWND hOptionsWnd = nullptr;
    HWND hCustomizeKeysWnd = nullptr;
    HWND hNamingSyntaxWnd = nullptr;
    HWND hShortcutsWnd = nullptr;
    HHOOK hKeyHook = nullptr;
    HICON hTrayIcon = nullptr;
    HCURSOR hCurPen = nullptr;
    HCURSOR hCurStabilo = nullptr;
    NOTIFYICONDATAW nid = {0};
    ULONG_PTR gdiplusToken = 0;
    std::vector<HWND> pinnedWindows;
    std::vector<std::wstring> pendingPinFiles;
    std::wstring pendingEditFile;

    static constexpr const wchar_t* APP_VERSION = L"3.9.1";
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
    bool keepCursorOnScreenshot = false;
    bool autoHideFrameList = true;
    bool hidePinnedToolbar = false;
    bool hidePinnedOutline = false;
    bool smoothPinnedImage = false;
    int optionsWindowHeight = 772;
    int customizeKeysWindowHeight = 560;
    int shortcutsWindowWidth = 880;
    int shortcutsWindowHeight = 560;
    int namingSyntaxWindowHeight = 706;
    bool autoCheckUpdates = true;
    UpdateCheckInterval updateInterval = UpdateCheckInterval::EveryDay;
    long long lastUpdateCheckTime = 0;
    std::wstring updateGithubRepo = DEFAULT_GITHUB_REPO;
    bool isCheckingUpdate = false;

    // Custom Global Hotkey Bindings (Defaults: Ctrl+PrtScn, Shift+PrtScn, Ctrl+Shift+PrtScn)
    HotkeyBinding hkRegionSnip{ MOD_CONTROL, VK_SNAPSHOT };
    HotkeyBinding hkFullSnap{ MOD_SHIFT, VK_SNAPSHOT };
    HotkeyBinding hkPrevRegion{ MOD_CONTROL | MOD_SHIFT, VK_SNAPSHOT };

    // Custom Area Tool Shortcut Bindings
    HotkeyBinding hkToolSelect{ 0, 'V' };
    HotkeyBinding hkToolPen{ 0, 'P' };
    HotkeyBinding hkToolStabilo{ 0, 'H' };
    HotkeyBinding hkToolLine{ 0, 'L' };
    HotkeyBinding hkToolArrow{ 0, 'A' };
    HotkeyBinding hkToolNumber{ 0, 'N' };
    HotkeyBinding hkToolResetNum{ 0, 0 };
    HotkeyBinding hkToolRect{ 0, 'R' };
    HotkeyBinding hkToolEllipse{ 0, 'E' };
    HotkeyBinding hkToolText{ 0, 'T' };
    HotkeyBinding hkToolMosaicSq{ 0, 'X' };
    HotkeyBinding hkToolMosaicCir{ 0, 'M' };

    // Custom Area Toolbar Action Shortcut Bindings
    HotkeyBinding hkActUndo{ MOD_CONTROL, 'Z' };
    HotkeyBinding hkActRedo{ MOD_CONTROL, 'Y' };
    HotkeyBinding hkActClear{ 0, 0 };
    HotkeyBinding hkActOptions{ 0, 0 };
    HotkeyBinding hkActPin{ 0, 'F' };
    HotkeyBinding hkActOcr{ 0, 'O' };
    HotkeyBinding hkActSaveAs{ MOD_CONTROL | MOD_SHIFT, 'S' };
    HotkeyBinding hkActSave{ MOD_CONTROL, 'S' };
    HotkeyBinding hkActCopy{ MOD_CONTROL, 'C' };

    // Multi-Frame GIF / WebP Editor Shortcut Bindings
    HotkeyBinding hkFramePlayPause{ 0, VK_SPACE };
    HotkeyBinding hkFrameToggleStrip{ 0, 0 };

    // Pinned Image Toolbar Shortcut Bindings
    HotkeyBinding hkPinZoomOut{ 0, VK_OEM_MINUS };
    HotkeyBinding hkPinZoomIn{ 0, VK_OEM_PLUS };
    HotkeyBinding hkPinZoomReset{ 0, '0' };
    HotkeyBinding hkPinOutline{ 0, 'O' };
    HotkeyBinding hkPinHideToolbar{ 0, 'H' };
    HotkeyBinding hkPinSmooth{ 0, 'S' };
    HotkeyBinding hkPinSaveAs{ 0, 0 };
    HotkeyBinding hkPinSave{ 0, 0 };
    HotkeyBinding hkPinCopy{ MOD_CONTROL, 'C' };
    HotkeyBinding hkPinClose{ 0, VK_ESCAPE };

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
    bool lastCtrlTempSelect = false;
    bool lastShiftColorPick = false;
    bool pendingShiftColorPick = false;
    bool pendingCtrlWindowSelect = false;
    RECT pendingCtrlWindowRect{0, 0, 0, 0};
    int loupeMagnification = 5;
    bool isRightClickSelectionDrag = false;
    POINT selectionTargetPt{0, 0};

    bool HasCreatedCustomArea() const {
        if (!hasSelection) return false;
        if (dragMode == DragMode::CreatingSelection || dragMode == DragMode::PendingOutsideSelection) return false;
        return (selRect.right != selRect.left) && (selRect.bottom != selRect.top);
    }

    bool IsCtrlTempSelectActive() const {
        if (!HasCreatedCustomArea() || isEditingText || isEditingSize || isEditingStroke) return false;
        if (dragMode == DragMode::DrawingAnnotation) return false;
        return ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) ||
               ((GetKeyState(VK_CONTROL) & 0x8000) != 0) ||
               ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0) ||
               ((GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0);
    }

    OverlayTool GetEffectiveTool() const {
        return IsCtrlTempSelectActive() ? OverlayTool::SelectMove : activeTool;
    }
    Color activeColor = Color(255, 239, 68, 68);
    float activeStroke = 4.0f;
    int nextStepNum = 1;
    int nextAnnotationId = 1;
    int selectedAnnotationId = -1;
    std::vector<int> selectedAnnotationIds;
    POINT marqueeStartPt{0, 0};
    RECT marqueeRect{0, 0, 0, 0};
    bool hasMarqueeBox = false;
    bool moveAnnUndoPushed = false;
    bool clickedAlreadySelectedAnn = false;
    int clickedAlreadySelectedAnnId = -1;
    PointF2D moveAnnOffset{0.0f, 0.0f};
    PointF2D penSnapAnchor{0.0f, 0.0f};
    bool isPenShiftSnapping = false;

    bool IsAnnotationSelected(int id) const {
        if (id == -1) return false;
        if (id == selectedAnnotationId) return true;
        return std::find(selectedAnnotationIds.begin(), selectedAnnotationIds.end(), id) != selectedAnnotationIds.end();
    }

    bool HasSelectedAnnotations() const {
        return selectedAnnotationId != -1 || !selectedAnnotationIds.empty();
    }

    void ClearAnnotationSelection() {
        selectedAnnotationId = -1;
        selectedAnnotationIds.clear();
    }

    void SelectSingleAnnotation(int id) {
        selectedAnnotationIds.clear();
        selectedAnnotationId = id;
        if (id != -1) {
            selectedAnnotationIds.push_back(id);
        }
    }

    void ToggleAnnotationSelection(int id) {
        if (id == -1) return;
        if (selectedAnnotationIds.empty() && selectedAnnotationId != -1) {
            selectedAnnotationIds.push_back(selectedAnnotationId);
        }
        auto it = std::find(selectedAnnotationIds.begin(), selectedAnnotationIds.end(), id);
        if (it != selectedAnnotationIds.end()) {
            selectedAnnotationIds.erase(it);
            if (selectedAnnotationId == id) {
                selectedAnnotationId = selectedAnnotationIds.empty() ? -1 : selectedAnnotationIds.back();
            }
        } else {
            selectedAnnotationIds.push_back(id);
            selectedAnnotationId = id;
        }
    }

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
    int pressedBtnId = -1;

    // Combined 3-Row Toolbar Dragging & Bounds State
    bool hasCustomHudPos = false;
    POINT customHudPos{0, 0}; // { rightX, topY }
    POINT dragHudStartMouse{0, 0};
    POINT dragHudOrigPos{0, 0};
    RECT hudBoundsRect{0, 0, 0, 0};

    // Multi-Frame GIF / WebP Selector Strip State (Bottom of Edit with PepperSnap)
    bool isSavingAsModalOpen = false;
    Bitmap* overlaySingleImageBmp = nullptr;
    std::vector<Bitmap*> overlayFrames;
    std::vector<int> overlayFrameDelaysMs;
    size_t overlayActiveFrameIdx = 0;
    RECT overlayImgDrawRect{0, 0, 0, 0};
    bool overlayFrameStripCollapsed = false;
    int overlayFrameStripHeight = 142;
    float overlayFrameScrollX = 0.0f;
    float overlayFrameMaxScrollX = 0.0f;
    int overlayHoveredFrameIdx = -1;
    bool overlayToggleBtnHovered = false;
    bool overlayPlayBtnHovered = false;
    bool overlayFramesPlaying = false;
    bool overlayTopResizeHovered = false;
    bool overlayScrollbarHovered = false;
    bool overlayScrollLeftBtnHovered = false;
    bool overlayScrollRightBtnHovered = false;
    RECT overlayFrameStripPanelRect{0, 0, 0, 0};
    RECT overlayFrameStripToggleBtnRect{0, 0, 0, 0};
    RECT overlayFrameStripPlayBtnRect{0, 0, 0, 0};
    RECT overlayFrameStripResizeRect{0, 0, 0, 0};
    RECT overlayFrameStripScrollTrackRect{0, 0, 0, 0};
    RECT overlayFrameStripScrollThumbRect{0, 0, 0, 0};
    RECT overlayFrameStripScrollLeftRect{0, 0, 0, 0};
    RECT overlayFrameStripScrollRightRect{0, 0, 0, 0};
    std::vector<RECT> overlayFrameCardRects;
    int dragFrameStripStartY = 0;
    int dragFrameStripOrigH = 142;
    int dragFrameScrollStartX = 0;
    float dragFrameScrollOrigX = 0.0f;

    enum class FrameStripPressedItem {
        None = 0,
        ToggleCollapse,
        PlayPause,
        ScrollLeft,
        ScrollRight,
        FrameCard
    };
    FrameStripPressedItem overlayFramePressedItem = FrameStripPressedItem::None;
    int overlayFramePressedCardIdx = -1;

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
    bool HasOverlayMultiFrames() const { return overlayFrames.size() > 1; }
    void ClearOverlayFrames();
    void SelectOverlayFrame(size_t frameIdx);
    void ToggleOverlayFramePlayback();
    void StopOverlayFramePlayback();
    void EnsureActiveFrameVisibleInStrip();
    void LayoutOverlayFrameStrip(int screenW, int screenH);
    void RenderOverlayFrameStrip(Graphics& g, int screenW, int screenH);
    void OpenImageToPinOnTop();
    void OpenImageFileToPinOnTop(const std::wstring& filePath);
    void OpenImageFilesToPinOnTop(const std::vector<std::wstring>& filePaths);
    HWND CreatePinnedWindow(Bitmap* bmp, int x, int y, float initialScale = 1.0f, std::vector<Bitmap*> animFrames = {}, std::vector<int> animDelaysMs = {});

    void CommitActiveTextBox();
    void CommitActiveSizeInput();
    void CommitActiveStrokeInput();
    void ApplyStrokeToSelectedAnnotation(float newStroke, bool recordUndo);
    void RecalcNextStepNum();
    Bitmap* RenderCroppedRegionBitmap();
    bool CopyBitmapToClipboard(Bitmap* bmp);
    bool CopyMultipleBitmapsToClipboard(const std::vector<Bitmap*>& bmps);
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
    static float GetFittedPillFontSize(Graphics& g, const FontFamily& ff, const std::wstring& fullText, int boxW, int boxH);
    static float GetCenteredPillTextLeftX(Graphics& g, const Font& font, const std::wstring& fullText, int boxLeft, int boxW);
    static size_t HitTestMonoIndex(int mouseX, const RECT& boxRect, const std::wstring& s, const std::wstring& suffix = L" px");
    static void DrawEditablePillText(Graphics& g, const RECT& boxRect, const std::wstring& text, const std::wstring& suffix, bool isEditing, size_t caretPos, size_t selAnchor);
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
    static bool AnnotationIntersectsRect(const Annotation& a, const RECT& rc);
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
    WritePrivateProfileStringW(L"PepperSnap", L"KeepCursorOnScreenshot", keepCursorOnScreenshot ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"AutoHideFrameList", autoHideFrameList ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"HidePinnedToolbar", hidePinnedToolbar ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"HidePinnedOutline", hidePinnedOutline ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"SmoothPinnedImage", smoothPinnedImage ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"OptionsWindowHeight", std::to_wstring(optionsWindowHeight).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"CustomizeKeysWindowHeight", std::to_wstring(customizeKeysWindowHeight).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"ShortcutsWindowWidth", std::to_wstring(shortcutsWindowWidth).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"ShortcutsWindowHeight", std::to_wstring(shortcutsWindowHeight).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"NamingSyntaxWindowHeight", std::to_wstring(namingSyntaxWindowHeight).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"FrameStripHeight", std::to_wstring(overlayFrameStripHeight).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"ActiveColorARGB", std::to_wstring(activeColor.GetValue()).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"ActiveStrokeWidth", std::to_wstring((int)std::round(activeStroke)).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"CaptureCounter", std::to_wstring(captureCounter).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"PenSmoothingEnabled", penSmoothingEnabled ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"PenSmoothingStrength", std::to_wstring(penSmoothingStrength).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"PenSmoothingDefaultV31", L"1", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"DefaultsV3107", L"1", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"AutoCheckUpdates", autoCheckUpdates ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"UpdateCheckInterval", std::to_wstring((int)updateInterval).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"LastUpdateCheckTime", std::to_wstring(lastUpdateCheckTime).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"UpdateGithubRepo", updateGithubRepo.c_str(), iniPath.c_str());
    auto writeHk = [&](const wchar_t* modKey, const wchar_t* vkKey, const HotkeyBinding& hk) {
        WritePrivateProfileStringW(L"PepperSnap", modKey, std::to_wstring(hk.modifiers).c_str(), iniPath.c_str());
        WritePrivateProfileStringW(L"PepperSnap", vkKey,  std::to_wstring(hk.vk).c_str(), iniPath.c_str());
    };
    writeHk(L"HkRegionMod",        L"HkRegionVk",        hkRegionSnip);
    writeHk(L"HkFullMod",          L"HkFullVk",          hkFullSnap);
    writeHk(L"HkPrevMod",          L"HkPrevVk",          hkPrevRegion);
    writeHk(L"HkToolSelectMod",    L"HkToolSelectVk",    hkToolSelect);
    writeHk(L"HkToolPenMod",       L"HkToolPenVk",       hkToolPen);
    writeHk(L"HkToolStabiloMod",   L"HkToolStabiloVk",   hkToolStabilo);
    writeHk(L"HkToolLineMod",      L"HkToolLineVk",      hkToolLine);
    writeHk(L"HkToolArrowMod",     L"HkToolArrowVk",     hkToolArrow);
    writeHk(L"HkToolNumberMod",    L"HkToolNumberVk",    hkToolNumber);
    writeHk(L"HkToolResetNumMod",  L"HkToolResetNumVk",  hkToolResetNum);
    writeHk(L"HkToolRectMod",      L"HkToolRectVk",      hkToolRect);
    writeHk(L"HkToolEllipseMod",   L"HkToolEllipseVk",   hkToolEllipse);
    writeHk(L"HkToolTextMod",      L"HkToolTextVk",      hkToolText);
    writeHk(L"HkToolMosaicSqMod",  L"HkToolMosaicSqVk",  hkToolMosaicSq);
    writeHk(L"HkToolMosaicCirMod", L"HkToolMosaicCirVk", hkToolMosaicCir);
    writeHk(L"HkActUndoMod",       L"HkActUndoVk",       hkActUndo);
    writeHk(L"HkActRedoMod",       L"HkActRedoVk",       hkActRedo);
    writeHk(L"HkActClearMod",      L"HkActClearVk",      hkActClear);
    writeHk(L"HkActOptionsMod",    L"HkActOptionsVk",    hkActOptions);
    writeHk(L"HkActPinMod",        L"HkActPinVk",        hkActPin);
    writeHk(L"HkActOcrMod",        L"HkActOcrVk",        hkActOcr);
    writeHk(L"HkActSaveAsMod",     L"HkActSaveAsVk",     hkActSaveAs);
    writeHk(L"HkActSaveMod",       L"HkActSaveVk",       hkActSave);
    writeHk(L"HkActCopyMod",       L"HkActCopyVk",       hkActCopy);
    writeHk(L"HkFramePlayMod",     L"HkFramePlayVk",     hkFramePlayPause);
    writeHk(L"HkFrameToggleMod",   L"HkFrameToggleVk",   hkFrameToggleStrip);
    writeHk(L"HkPinZoomOutMod",    L"HkPinZoomOutVk",    hkPinZoomOut);
    writeHk(L"HkPinZoomInMod",     L"HkPinZoomInVk",     hkPinZoomIn);
    writeHk(L"HkPinZoomResetMod",   L"HkPinZoomResetVk",   hkPinZoomReset);
    writeHk(L"HkPinOutlineMod",     L"HkPinOutlineVk",     hkPinOutline);
    writeHk(L"HkPinHideToolbarMod", L"HkPinHideToolbarVk", hkPinHideToolbar);
    writeHk(L"HkPinSmoothMod",      L"HkPinSmoothVk",      hkPinSmooth);
    writeHk(L"HkPinSaveAsMod",     L"HkPinSaveAsVk",     hkPinSaveAs);
    writeHk(L"HkPinSaveMod",       L"HkPinSaveVk",       hkPinSave);
    writeHk(L"HkPinCopyMod",       L"HkPinCopyVk",       hkPinCopy);
    writeHk(L"HkPinCloseMod",      L"HkPinCloseVk",      hkPinClose);
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
    keepCursorOnScreenshot = (GetPrivateProfileIntW(L"PepperSnap", L"KeepCursorOnScreenshot", 0, iniPath.c_str()) != 0);
    autoHideFrameList = (GetPrivateProfileIntW(L"PepperSnap", L"AutoHideFrameList", 1, iniPath.c_str()) != 0);
    hidePinnedToolbar = (GetPrivateProfileIntW(L"PepperSnap", L"HidePinnedToolbar", 0, iniPath.c_str()) != 0);
    hidePinnedOutline = (GetPrivateProfileIntW(L"PepperSnap", L"HidePinnedOutline", 0, iniPath.c_str()) != 0);
    smoothPinnedImage = (GetPrivateProfileIntW(L"PepperSnap", L"SmoothPinnedImage", 0, iniPath.c_str()) != 0);
    int optWinH = (int)GetPrivateProfileIntW(L"PepperSnap", L"OptionsWindowHeight", 772, iniPath.c_str());
    if (optWinH == 716 || optWinH == 744) optWinH = 772;
    optionsWindowHeight = std::max(320, std::min(2160, optWinH));
    int ckWinH = (int)GetPrivateProfileIntW(L"PepperSnap", L"CustomizeKeysWindowHeight", 560, iniPath.c_str());
    customizeKeysWindowHeight = std::max(280, std::min(2160, ckWinH));
    int scWinW = (int)GetPrivateProfileIntW(L"PepperSnap", L"ShortcutsWindowWidth", 880, iniPath.c_str());
    shortcutsWindowWidth = std::max(680, std::min(3840, scWinW));
    int scWinH = (int)GetPrivateProfileIntW(L"PepperSnap", L"ShortcutsWindowHeight", 560, iniPath.c_str());
    shortcutsWindowHeight = std::max(260, std::min(2160, scWinH));
    int nsWinH = (int)GetPrivateProfileIntW(L"PepperSnap", L"NamingSyntaxWindowHeight", 706, iniPath.c_str());
    namingSyntaxWindowHeight = std::max(260, std::min(2160, nsWinH));
    int fsH = (int)GetPrivateProfileIntW(L"PepperSnap", L"FrameStripHeight", 142, iniPath.c_str());
    overlayFrameStripHeight = std::max(92, std::min(600, fsH));

    WCHAR colBuf[64] = {0};
    GetPrivateProfileStringW(L"PepperSnap", L"ActiveColorARGB", L"", colBuf, 63, iniPath.c_str());
    if (wcslen(colBuf) > 0) {
        ARGB parsedCol = (ARGB)_wcstoui64(colBuf, nullptr, 10);
        if ((parsedCol >> 24) != 0) activeColor = Color(parsedCol);
    }
    int savedStroke = (int)GetPrivateProfileIntW(L"PepperSnap", L"ActiveStrokeWidth", 4, iniPath.c_str());
    activeStroke = (float)std::max(1, std::min(120, savedStroke));
    int savedCounter = (int)GetPrivateProfileIntW(L"PepperSnap", L"CaptureCounter", 1, iniPath.c_str());
    captureCounter = std::max(1, savedCounter);
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

    auto readHk = [&](const wchar_t* modKey, const wchar_t* vkKey, HotkeyBinding& hk, UINT defMod, UINT defVk) {
        int vkVal = (int)GetPrivateProfileIntW(L"PepperSnap", vkKey, -1, iniPath.c_str());
        if (vkVal < 0) {
            hk.modifiers = defMod;
            hk.vk = defVk;
        } else if (vkVal == 0) {
            hk.modifiers = 0;
            hk.vk = 0;
        } else {
            hk.modifiers = (UINT)GetPrivateProfileIntW(L"PepperSnap", modKey, defMod, iniPath.c_str());
            hk.vk = (UINT)vkVal;
        }
    };
    readHk(L"HkRegionMod",        L"HkRegionVk",        hkRegionSnip,    MOD_CONTROL,             VK_SNAPSHOT);
    readHk(L"HkFullMod",          L"HkFullVk",          hkFullSnap,      MOD_SHIFT,               VK_SNAPSHOT);
    readHk(L"HkPrevMod",          L"HkPrevVk",          hkPrevRegion,    MOD_CONTROL | MOD_SHIFT, VK_SNAPSHOT);
    readHk(L"HkToolSelectMod",    L"HkToolSelectVk",    hkToolSelect,    0,                       'V');
    readHk(L"HkToolPenMod",       L"HkToolPenVk",       hkToolPen,       0,                       'P');
    readHk(L"HkToolStabiloMod",   L"HkToolStabiloVk",   hkToolStabilo,   0,                       'H');
    readHk(L"HkToolLineMod",      L"HkToolLineVk",      hkToolLine,      0,                       'L');
    readHk(L"HkToolArrowMod",     L"HkToolArrowVk",     hkToolArrow,     0,                       'A');
    readHk(L"HkToolNumberMod",    L"HkToolNumberVk",    hkToolNumber,    0,                       'N');
    readHk(L"HkToolResetNumMod",  L"HkToolResetNumVk",  hkToolResetNum,  0,                       0);
    readHk(L"HkToolRectMod",      L"HkToolRectVk",      hkToolRect,      0,                       'R');
    readHk(L"HkToolEllipseMod",   L"HkToolEllipseVk",   hkToolEllipse,   0,                       'E');
    readHk(L"HkToolTextMod",      L"HkToolTextVk",      hkToolText,      0,                       'T');
    readHk(L"HkToolMosaicSqMod",  L"HkToolMosaicSqVk",  hkToolMosaicSq,  0,                       'X');
    readHk(L"HkToolMosaicCirMod", L"HkToolMosaicCirVk", hkToolMosaicCir, 0,                       'M');
    readHk(L"HkActUndoMod",       L"HkActUndoVk",       hkActUndo,          MOD_CONTROL,             'Z');
    readHk(L"HkActRedoMod",       L"HkActRedoVk",       hkActRedo,          MOD_CONTROL,             'Y');
    readHk(L"HkActClearMod",      L"HkActClearVk",      hkActClear,         0,                       0);
    readHk(L"HkActOptionsMod",    L"HkActOptionsVk",    hkActOptions,       0,                       0);
    readHk(L"HkActPinMod",        L"HkActPinVk",        hkActPin,           0,                       'F');
    readHk(L"HkActOcrMod",        L"HkActOcrVk",        hkActOcr,           0,                       'O');
    readHk(L"HkActSaveAsMod",     L"HkActSaveAsVk",     hkActSaveAs,        MOD_CONTROL | MOD_SHIFT, 'S');
    readHk(L"HkActSaveMod",       L"HkActSaveVk",       hkActSave,          MOD_CONTROL,             'S');
    readHk(L"HkActCopyMod",       L"HkActCopyVk",       hkActCopy,          MOD_CONTROL,             'C');
    readHk(L"HkFramePlayMod",     L"HkFramePlayVk",     hkFramePlayPause,   0,                       VK_SPACE);
    readHk(L"HkFrameToggleMod",   L"HkFrameToggleVk",   hkFrameToggleStrip, 0,                       0);
    readHk(L"HkPinZoomOutMod",    L"HkPinZoomOutVk",    hkPinZoomOut,       0,                       VK_OEM_MINUS);
    readHk(L"HkPinZoomInMod",     L"HkPinZoomInVk",     hkPinZoomIn,     0,                       VK_OEM_PLUS);
    readHk(L"HkPinZoomResetMod",   L"HkPinZoomResetVk",   hkPinZoomReset,   0,                       '0');
    readHk(L"HkPinOutlineMod",     L"HkPinOutlineVk",     hkPinOutline,     0,                       'O');
    readHk(L"HkPinHideToolbarMod", L"HkPinHideToolbarVk", hkPinHideToolbar, 0,                       'H');
    readHk(L"HkPinSmoothMod",      L"HkPinSmoothVk",      hkPinSmooth,      0,                       'S');
    readHk(L"HkPinSaveAsMod",     L"HkPinSaveAsVk",     hkPinSaveAs,     0,                       0);
    readHk(L"HkPinSaveMod",       L"HkPinSaveVk",       hkPinSave,       0,                       0);
    if (GetPrivateProfileIntW(L"PepperSnap", L"PinSaveShortcutsEmptyDefaultV1", 0, iniPath.c_str()) == 0) {
        if (hkPinSaveAs.modifiers == (MOD_CONTROL | MOD_SHIFT) && hkPinSaveAs.vk == 'S') {
            hkPinSaveAs = { 0, 0 };
            WritePrivateProfileStringW(L"PepperSnap", L"HkPinSaveAsMod", L"0", iniPath.c_str());
            WritePrivateProfileStringW(L"PepperSnap", L"HkPinSaveAsVk", L"0", iniPath.c_str());
        }
        if (hkPinSave.modifiers == MOD_CONTROL && hkPinSave.vk == 'S') {
            hkPinSave = { 0, 0 };
            WritePrivateProfileStringW(L"PepperSnap", L"HkPinSaveMod", L"0", iniPath.c_str());
            WritePrivateProfileStringW(L"PepperSnap", L"HkPinSaveVk", L"0", iniPath.c_str());
        }
        WritePrivateProfileStringW(L"PepperSnap", L"PinSaveShortcutsEmptyDefaultV1", L"1", iniPath.c_str());
    }
    readHk(L"HkPinCopyMod",       L"HkPinCopyVk",       hkPinCopy,       MOD_CONTROL,             'C');
    readHk(L"HkPinCloseMod",      L"HkPinCloseVk",      hkPinClose,      0,                       VK_ESCAPE);

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

    static const wchar_t* const kMonthFull[12] = {
        L"January", L"February", L"March", L"April", L"May", L"June",
        L"July", L"August", L"September", L"October", L"November", L"December"
    };
    static const wchar_t* const kMonthShort[12] = {
        L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun",
        L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"
    };
    static const wchar_t* const kDayFull[7] = {
        L"Sunday", L"Monday", L"Tuesday", L"Wednesday", L"Thursday", L"Friday", L"Saturday"
    };
    static const wchar_t* const kDayShort[7] = {
        L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat"
    };

    int mIdx = (st.wMonth >= 1 && st.wMonth <= 12) ? (st.wMonth - 1) : 0;
    int dIdx = (st.wDayOfWeek <= 6) ? (int)st.wDayOfWeek : 0;
    WORD hour12 = (st.wHour % 12 == 0) ? 12 : (st.wHour % 12);
    bool isPm = (st.wHour >= 12);
    long long unixSec = (long long)_time64(nullptr);

    std::wstring out = pattern.empty() ? DEFAULT_NAMING_PATTERN : pattern;
    ReplaceStrW(out, L"{YYYY}", std::to_wstring(st.wYear));
    ReplaceStrW(out, L"{YY}",   pad2(st.wYear % 100));
    ReplaceStrW(out, L"{MMMM}", kMonthFull[mIdx]);
    ReplaceStrW(out, L"{MMM}",  kMonthShort[mIdx]);
    ReplaceStrW(out, L"{MM}",   pad2(st.wMonth));
    ReplaceStrW(out, L"{M}",    std::to_wstring(st.wMonth));
    ReplaceStrW(out, L"{DDDD}", kDayFull[dIdx]);
    ReplaceStrW(out, L"{DDD}",  kDayShort[dIdx]);
    ReplaceStrW(out, L"{DD}",   pad2(st.wDay));
    ReplaceStrW(out, L"{D}",    std::to_wstring(st.wDay));
    ReplaceStrW(out, L"{HH}",   pad2(st.wHour));
    ReplaceStrW(out, L"{H}",    std::to_wstring(st.wHour));
    ReplaceStrW(out, L"{hh}",   pad2(hour12));
    ReplaceStrW(out, L"{h}",    std::to_wstring(hour12));
    ReplaceStrW(out, L"{AP}",   isPm ? L"PM" : L"AM");
    ReplaceStrW(out, L"{ap}",   isPm ? L"pm" : L"am");
    ReplaceStrW(out, L"{mm}",   pad2(st.wMinute));
    ReplaceStrW(out, L"{m}",    std::to_wstring(st.wMinute));
    ReplaceStrW(out, L"{ss}",   pad2(st.wSecond));
    ReplaceStrW(out, L"{s}",    std::to_wstring(st.wSecond));
    ReplaceStrW(out, L"{ms}",   pad3(st.wMilliseconds));
    ReplaceStrW(out, L"{UNIX}", std::to_wstring(unixSec));
    ReplaceStrW(out, L"{NNN}",  pad3(seqNum));
    ReplaceStrW(out, L"{N}",    std::to_wstring(seqNum));

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
    if (dragMode == DragMode::ResizingFrameStrip) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
        return;
    }
    if (dragMode == DragMode::ScrollingFrameStrip) {
        SetCursor(LoadCursorW(nullptr, IDC_HAND));
        return;
    }
    if (dragMode == DragMode::DraggingHUD) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
        return;
    }
    POINT pt{ mx, my };
    if (HasOverlayMultiFrames()) {
        if (PtInRect(&overlayFrameStripToggleBtnRect, pt)) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return;
        }
        if (!overlayFrameStripCollapsed) {
            if (PtInRect(&overlayFrameStripResizeRect, pt)) {
                SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
                return;
            }
            if (PtInRect(&overlayFrameStripPanelRect, pt)) {
                if (overlayHoveredFrameIdx >= 0 ||
                    PtInRect(&overlayFrameStripScrollTrackRect, pt) ||
                    PtInRect(&overlayFrameStripScrollLeftRect, pt) ||
                    PtInRect(&overlayFrameStripScrollRightRect, pt)) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                } else {
                    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                }
                return;
            }
        }
    }
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
    // 1. Arrow cursor for Select mode (or when holding Ctrl to temporarily use Select mode)
    // 2. Pen cursor for Pen
    // 3. Stabilo cursor for Highlighter
    // 4. (+ 'plus') cursor for: Line, Square, Circle, Mosaic Square, Mosaic Circle, Arrow, Numbering Arrow
    // 5. Arrow cursor for Text
    switch (GetEffectiveTool()) {
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

static std::wstring FormatLabelWithShortcut(const std::wstring& label, const HotkeyBinding& hk, const std::wstring& suffix = L"") {
    if (hk.vk == 0) return label + suffix;
    return label + L" (" + PepperSnapDaemon::FormatHotkeyString(hk) + L")" + suffix;
}

static bool MatchesOverlayShortcut(const HotkeyBinding& hk, UINT vk, bool ctrl, bool shift, bool alt) {
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

static bool MatchesOverlayShortcutWithOptionalShift(const HotkeyBinding& hk, UINT vk, bool ctrl, bool shift, bool alt) {
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

static bool MatchesAnyPinnedShortcut(UINT vk, bool ctrl, bool shift, bool alt) {
    return MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinZoomOut,     vk, ctrl, shift, alt) ||
           MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinZoomIn,      vk, ctrl, shift, alt) ||
           MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinZoomReset,   vk, ctrl, shift, alt) ||
           MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinOutline,     vk, ctrl, shift, alt) ||
           MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinHideToolbar, vk, ctrl, shift, alt) ||
           MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinSmooth,      vk, ctrl, shift, alt) ||
           MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinSaveAs,      vk, ctrl, shift, alt) ||
           MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinSave,        vk, ctrl, shift, alt) ||
           MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinCopy,        vk, ctrl, shift, alt) ||
           MatchesOverlayShortcutWithOptionalShift(g_Daemon.hkPinClose,       vk, ctrl, shift, alt);
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

    std::wstring editLabel = L"Edit with PepperSnap";
    std::wstring pinLabel  = L"Pin on top";
    std::wstring iconVal   = L"\"" + std::wstring(exePath) + L"\",0";
    std::wstring editCmd   = L"\"" + std::wstring(exePath) + L"\" --edit \"%1\"";
    std::wstring pinCmd    = L"\"" + std::wstring(exePath) + L"\" --pin \"%1\"";

    auto writeSingleVerb = [&](const std::wstring& verbKeyPath, const std::wstring& lbl, const std::wstring& cmd, const wchar_t* multiSelectModel) {
        HKEY hVerbKey = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, verbKeyPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hVerbKey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hVerbKey, nullptr, 0, REG_SZ, (const BYTE*)lbl.c_str(), (DWORD)((lbl.size() + 1) * sizeof(wchar_t)));
            RegSetValueExW(hVerbKey, L"Icon", 0, REG_SZ, (const BYTE*)iconVal.c_str(), (DWORD)((iconVal.size() + 1) * sizeof(wchar_t)));
            if (multiSelectModel && multiSelectModel[0] != L'\0') {
                RegSetValueExW(hVerbKey, L"MultiSelectModel", 0, REG_SZ, (const BYTE*)multiSelectModel, (DWORD)((wcslen(multiSelectModel) + 1) * sizeof(wchar_t)));
            }
            RegCloseKey(hVerbKey);
        }
        std::wstring cmdKeyPath = verbKeyPath + L"\\command";
        HKEY hCmdKey = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, cmdKeyPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hCmdKey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hCmdKey, nullptr, 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(hCmdKey);
        }
    };

    auto writeShellVerbs = [&](const std::wstring& baseKeyPath) {
        writeSingleVerb(baseKeyPath + L"\\shell\\PepperSnapEdit", editLabel, editCmd, L"Single");
        writeSingleVerb(baseKeyPath + L"\\shell\\PepperSnapPin",  pinLabel,  pinCmd,  L"Player");
    };

    // Register ONLY for image files via SystemFileAssociations\image and common image extensions
    writeShellVerbs(L"Software\\Classes\\SystemFileAssociations\\image");
    const wchar_t* imgExts[] = {
        L".png", L".jpg", L".jpeg", L".webp", L".bmp", L".gif", L".tif", L".tiff", L".ico"
    };
    const wchar_t* perceivedImg = L"image";
    for (const wchar_t* ext : imgExts) {
        writeShellVerbs(std::wstring(L"Software\\Classes\\SystemFileAssociations\\") + ext);
        HKEY hExtKey = nullptr;
        std::wstring extClassPath = std::wstring(L"Software\\Classes\\") + ext;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, extClassPath.c_str(), 0, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &hExtKey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hExtKey, L"PerceivedType", 0, REG_SZ, (const BYTE*)perceivedImg, (DWORD)((wcslen(perceivedImg) + 1) * sizeof(wchar_t)));
            WCHAR progIdBuf[256] = {0};
            DWORD cbProg = sizeof(progIdBuf);
            if (RegQueryValueExW(hExtKey, nullptr, nullptr, nullptr, (LPBYTE)progIdBuf, &cbProg) == ERROR_SUCCESS && progIdBuf[0] != L'\0') {
                writeShellVerbs(std::wstring(L"Software\\Classes\\") + progIdBuf);
            }
            RegCloseKey(hExtKey);
        }
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

        // If the Customize Keys dialog is actively recording a custom shortcut key
        HWND hRecTarget = (g_Daemon.hCustomizeKeysWnd && IsWindow(g_Daemon.hCustomizeKeysWnd))
            ? g_Daemon.hCustomizeKeysWnd
            : ((g_Daemon.hOptionsWnd && IsWindow(g_Daemon.hOptionsWnd)) ? g_Daemon.hOptionsWnd : nullptr);
        if (g_RecordingHotkeySlot != 0 && hRecTarget) {
            if (vk == VK_SNAPSHOT) {
                return CallNextHookEx(g_Daemon.hKeyHook, nCode, wParam, lParam);
            }
            if (isKeyDown) {
                if (vk == VK_ESCAPE) {
                    PostMessageW(hRecTarget, WM_SHORTCUT_RECORDED, 0, VK_ESCAPE);
                    return 1;
                }
                if (!IsModifierVk(vk)) {
                    UINT mods = GetCurrentHardwareModifiers();
                    PostMessageW(hRecTarget, WM_SHORTCUT_RECORDED, (WPARAM)mods, (LPARAM)vk);
                    return 1;
                }
            }
            return CallNextHookEx(g_Daemon.hKeyHook, nCode, wParam, lParam);
        }

        // If the Custom Area overlay is active (with no modal child window open), ensure Esc and overlay keys
        // reach hOverlayWnd immediately even if a higher-band shell flyout (Notification Sidebar) held focus
        if (g_Daemon.hOverlayWnd && IsWindow(g_Daemon.hOverlayWnd) &&
            (!g_Daemon.hOptionsWnd || !IsWindow(g_Daemon.hOptionsWnd)) &&
            (!g_Daemon.hCustomizeKeysWnd || !IsWindow(g_Daemon.hCustomizeKeysWnd)) &&
            (!g_Daemon.hNamingSyntaxWnd || !IsWindow(g_Daemon.hNamingSyntaxWnd)) &&
            (!g_Daemon.hShortcutsWnd || !IsWindow(g_Daemon.hShortcutsWnd))) {
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
                   (!g_Daemon.hOptionsWnd || !IsWindow(g_Daemon.hOptionsWnd)) &&
                   (!g_Daemon.hCustomizeKeysWnd || !IsWindow(g_Daemon.hCustomizeKeysWnd)) &&
                   (!g_Daemon.hNamingSyntaxWnd || !IsWindow(g_Daemon.hNamingSyntaxWnd)) &&
                   (!g_Daemon.hShortcutsWnd || !IsWindow(g_Daemon.hShortcutsWnd)) &&
                   !g_Daemon.pinnedWindows.empty() && isKeyDown && !IsModifierVk(vk)) {
            HWND hFg = GetForegroundWindow();
            bool fgIsPin = (std::find(g_Daemon.pinnedWindows.begin(), g_Daemon.pinnedWindows.end(), hFg) != g_Daemon.pinnedWindows.end());
            if (!fgIsPin) {
                POINT curPt = {0, 0};
                if (GetCursorPos(&curPt)) {
                    HWND hAtPt = WindowFromPoint(curPt);
                    if (std::find(g_Daemon.pinnedWindows.begin(), g_Daemon.pinnedWindows.end(), hAtPt) != g_Daemon.pinnedWindows.end()) {
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

    if (keepCursorOnScreenshot) {
        CURSORINFO ci = { sizeof(CURSORINFO) };
        if (GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING) != 0 && ci.hCursor) {
            HICON hIconCopy = CopyIcon(ci.hCursor);
            HICON hDrawCur = hIconCopy ? hIconCopy : (HICON)ci.hCursor;
            int drawX = (int)ci.ptScreenPos.x - vScreenX;
            int drawY = (int)ci.ptScreenPos.y - vScreenY;
            ICONINFO ii = {0};
            if (GetIconInfo(hDrawCur, &ii)) {
                drawX -= (int)ii.xHotspot;
                drawY -= (int)ii.yHotspot;
                if (ii.hbmMask) DeleteObject(ii.hbmMask);
                if (ii.hbmColor) DeleteObject(ii.hbmColor);
            }
            DrawIconEx(hMem, drawX, drawY, hDrawCur, 0, 0, 0, nullptr, DI_NORMAL);
            if (hIconCopy) DestroyIcon(hIconCopy);
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

    const BYTE* scan0 = (const BYTE*)data.Scan0;
    bool hasAlpha = false;
    for (UINT y = 0; y < h && !hasAlpha; ++y) {
        const DWORD* row = (const DWORD*)(scan0 + y * data.Stride);
        for (UINT x = 0; x < w; ++x) {
            if ((row[x] >> 24) < 255) {
                hasAlpha = true;
                break;
            }
        }
    }

    bw.bytes.reserve(w * h * (hasAlpha ? 4 : 3) + 512);
    bw.Write(0x2F, 8);               // VP8L signature byte
    bw.Write(w - 1, 14);             // 14-bit width - 1
    bw.Write(h - 1, 14);             // 14-bit height - 1
    bw.Write(hasAlpha ? 1 : 0, 1);   // alpha_is_used
    bw.Write(0, 3);                  // version = 0
    bw.Write(0, 1);                  // no transforms
    bw.Write(0, 1);                  // no color cache
    bw.Write(0, 1);                  // no meta prefix codes

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
    if (hasAlpha) {
        writeIdentity8BitTree(256); // Full 8-bit Alpha channel
    } else {
        // Alpha: single-symbol tree for 255 (0 bits per pixel)
        bw.Write(1, 1); bw.Write(0, 1); bw.Write(1, 1); bw.Write(255, 8);
    }
    // Distance: single-symbol tree for 0
    bw.Write(1, 1); bw.Write(0, 1); bw.Write(0, 1); bw.Write(0, 1);

    BYTE rev8[256];
    for (int i = 0; i < 256; ++i) {
        BYTE r = 0;
        for (int b = 0; b < 8; ++b) r = (BYTE)((r << 1) | ((i >> b) & 1));
        rev8[i] = r;
    }

    for (UINT y = 0; y < h; ++y) {
        const DWORD* row = (const DWORD*)(scan0 + y * data.Stride);
        for (UINT x = 0; x < w; ++x) {
            DWORD px = row[x];
            BYTE b = (BYTE)(px & 0xFF);
            BYTE g = (BYTE)((px >> 8) & 0xFF);
            BYTE r = (BYTE)((px >> 16) & 0xFF);
            BYTE a = (BYTE)((px >> 24) & 0xFF);
            bw.Write(rev8[g], 8);
            bw.Write(rev8[r], 8);
            bw.Write(rev8[b], 8);
            if (hasAlpha) {
                bw.Write(rev8[a], 8);
            }
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

static bool IsWindowsClipboardHistoryEnabled() {
    HKEY hKey = nullptr;
    DWORD enabled = 0;
    DWORD cbData = sizeof(enabled);
    DWORD dwType = 0;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Clipboard", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"EnableClipboardHistory", nullptr, &dwType, (LPBYTE)&enabled, &cbData) != ERROR_SUCCESS) {
            enabled = 0;
        }
        RegCloseKey(hKey);
    }
    return enabled != 0;
}

static std::wstring BuildUniqueSaveFilePath(const std::wstring& folder, const std::wstring& stem, const std::wstring& ext, int batchIdx = 0, int batchTotal = 1) {
    std::wstring baseStem = stem;
    if (batchTotal > 1 && batchIdx > 0) {
        baseStem += L"_" + std::to_wstring(batchIdx);
    }
    std::wstring candidate = folder + L"\\" + baseStem + ext;
    int suffix = 2;
    while (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) {
        candidate = folder + L"\\" + baseStem + L"_" + std::to_wstring(suffix++) + ext;
    }
    return candidate;
}

bool PepperSnapDaemon::CopyMultipleBitmapsToClipboard(const std::vector<Bitmap*>& bmps) {
    std::vector<Bitmap*> validBmps;
    for (Bitmap* b : bmps) {
        if (b && b->GetWidth() > 0 && b->GetHeight() > 0) {
            validBmps.push_back(b);
        }
    }
    if (validBmps.empty()) return false;
    bool anyOk = false;
    for (size_t i = 0; i < validBmps.size(); ++i) {
        if (CopyBitmapToClipboard(validBmps[i])) {
            anyOk = true;
        }
        if (i + 1 < validBmps.size()) {
            // Pump messages and pause briefly so Windows Clipboard History (Win+V / cbdhsvc) records each image separately
            DWORD t0 = GetTickCount();
            while (GetTickCount() - t0 < 220) {
                MSG msg;
                while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
                Sleep(15);
            }
        }
    }
    return anyOk;
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
    SaveSettings();
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
    SaveSettings();
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

            float boxW = isEditingCaret ? std::max(maxLineW + 14.0f, 110.0f) : std::max(12.0f, maxLineW + 10.0f);
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
        editingTextAnn.endPt = { bx + std::max(maxW + 10.0f, 12.0f), by + lineCount * lineStep + 6.0f };
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
    SaveSettings();
    if (isEditingText) {
        editingTextAnn.strokeWidth = newStroke;
    }
    if (!HasSelectedAnnotations()) return;
    bool pushed = false;
    for (auto& a : annotations) {
        if (IsAnnotationSelected(a.id)) {
            if (recordUndo && !pushed && std::abs(a.strokeWidth - newStroke) > 0.05f) {
                PushUndo();
                pushed = true;
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
    std::wstring raw = editingSizeText;
    editingSizeText.clear();
    sizeCaretPos = sizeSelAnchor = 0;

    int curW = std::max(1, (int)std::abs(selRect.right - selRect.left));
    int curH = std::max(1, (int)std::abs(selRect.bottom - selRect.top));
    int newW = 0, newH = 0;
    bool parsed = false;

    size_t sep = raw.find_first_of(L"xX\x00D7*,");
    auto hasDigit = [](const std::wstring& str) {
        for (wchar_t c : str) {
            if (c >= L'0' && c <= L'9') return true;
        }
        return false;
    };

    if (sep != std::wstring::npos) {
        std::wstring leftStr  = raw.substr(0, sep);
        std::wstring rightStr = raw.substr(sep + 1);
        bool hasLeft  = hasDigit(leftStr);
        bool hasRight = hasDigit(rightStr);
        if (hasLeft && !hasRight) {
            try {
                int wVal = std::stoi(leftStr);
                if (wVal >= 1) {
                    newW = wVal;
                    newH = hasSelection ? curH : wVal;
                    parsed = true;
                }
            } catch (...) {}
        } else if (!hasLeft && hasRight) {
            try {
                size_t lastSep = rightStr.find_last_of(L"xX\x00D7*, ");
                std::wstring hDigits = (lastSep != std::wstring::npos) ? rightStr.substr(lastSep + 1) : rightStr;
                int hVal = std::stoi(hDigits);
                if (hVal >= 1) {
                    newH = hVal;
                    newW = hasSelection ? curW : hVal;
                    parsed = true;
                }
            } catch (...) {}
        }
    }

    if (!parsed) {
        std::wstring s = raw;
        for (wchar_t& c : s) {
            if (c < L'0' || c > L'9') c = L' ';
        }
        std::wstringstream ss(s);
        if ((ss >> newW) && newW >= 1) {
            if (!(ss >> newH) || newH < 1) {
                newH = hasSelection ? curH : newW;
            }
            parsed = true;
        }
    }

    if (parsed && newW >= 1 && newH >= 1) {
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
    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces);
    CharacterRange cr(0, (INT)n);
    sf.SetMeasurableCharacterRanges(1, &cr);
    Region rgn;
    if (g.MeasureCharacterRanges(s.c_str(), (INT)s.size(), &monoFont, RectF(0.0f, 0.0f, 4000.0f, 1000.0f), &sf, 1, &rgn) == Ok) {
        RectF bounds;
        if (rgn.GetBounds(&bounds, &g) == Ok && bounds.Width > 0.0f) {
            return std::max(0.0f, bounds.X + bounds.Width);
        }
    }
    RectF r1, r0;
    std::wstring wrapped = L"|" + s.substr(0, n) + L"|";
    g.MeasureString(wrapped.c_str(), -1, &monoFont, PointF(0, 0), &sf, &r1);
    g.MeasureString(L"||", -1, &monoFont, PointF(0, 0), &sf, &r0);
    return std::max(0.0f, r1.Width - r0.Width);
}

float PepperSnapDaemon::GetFittedPillFontSize(Graphics& g, const FontFamily& ff, const std::wstring& fullText, int boxW, int boxH) {
    float fontSize = std::max(11.0f, (float)boxH * 0.60f); // 16.8px for 28px-tall toolbar box
    float maxTextW = std::max(12.0f, (float)(boxW - 10));
    if (!fullText.empty()) {
        Font testFont(&ff, fontSize, FontStyleRegular, UnitPixel);
        float measuredW = MeasureMonoPrefixWidth(g, testFont, fullText, fullText.size());
        if (measuredW > maxTextW && measuredW > 0.1f) {
            fontSize = std::max(9.5f, fontSize * (maxTextW / measuredW));
            for (int iter = 0; iter < 8; ++iter) {
                Font f2(&ff, fontSize, FontStyleRegular, UnitPixel);
                float w2 = MeasureMonoPrefixWidth(g, f2, fullText, fullText.size());
                if (w2 <= maxTextW || fontSize <= 9.5f) break;
                fontSize = std::max(9.5f, fontSize - 0.3f);
            }
        }
    }
    return fontSize;
}

float PepperSnapDaemon::GetCenteredPillTextLeftX(Graphics& g, const Font& font, const std::wstring& fullText, int boxLeft, int boxW) {
    if (fullText.empty()) return (float)boxLeft + (float)boxW * 0.5f;
    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces);
    CharacterRange cr(0, (INT)fullText.size());
    sf.SetMeasurableCharacterRanges(1, &cr);
    Region rgn;
    if (g.MeasureCharacterRanges(fullText.c_str(), (INT)fullText.size(), &font, RectF(0.0f, 0.0f, 4000.0f, 1000.0f), &sf, 1, &rgn) == Ok) {
        RectF bounds;
        if (rgn.GetBounds(&bounds, &g) == Ok && bounds.Width > 0.0f) {
            float midX = bounds.X + bounds.Width * 0.5f;
            return (float)boxLeft + (float)boxW * 0.5f - midX;
        }
    }
    float fullW = MeasureMonoPrefixWidth(g, font, fullText, fullText.size());
    return (float)boxLeft + ((float)boxW - fullW) * 0.5f;
}

size_t PepperSnapDaemon::HitTestMonoIndex(int mouseX, const RECT& boxRect, const std::wstring& s, const std::wstring& suffix) {
    if (s.empty()) return 0;
    int boxW = std::max(16, (int)(boxRect.right - boxRect.left));
    int boxH = std::max(16, (int)(boxRect.bottom - boxRect.top));
    HDC hdc = GetDC(nullptr);
    Graphics g(hdc);
    FontFamily ff(L"Segoe UI");
    std::wstring fullText = s + suffix;
    float fontSize = GetFittedPillFontSize(g, ff, fullText, boxW, boxH);
    Font pillFont(&ff, fontSize, FontStyleRegular, UnitPixel);
    float textLeftX = GetCenteredPillTextLeftX(g, pillFont, fullText, boxRect.left, boxW);
    if ((float)mouseX <= textLeftX) {
        ReleaseDC(nullptr, hdc);
        return 0;
    }
    float relX = (float)mouseX - textLeftX;
    size_t result = s.size();
    for (size_t i = 0; i < s.size(); ++i) {
        float x0 = MeasureMonoPrefixWidth(g, pillFont, s, i);
        float x1 = MeasureMonoPrefixWidth(g, pillFont, s, i + 1);
        if (relX < (x0 + x1) * 0.5f) {
            result = i;
            break;
        }
    }
    ReleaseDC(nullptr, hdc);
    return result;
}

void PepperSnapDaemon::DrawEditablePillText(
    Graphics& g, const RECT& boxRect,
    const std::wstring& text, const std::wstring& suffix,
    bool isEditing, size_t caretPos, size_t selAnchor
) {
    int boxLeft = boxRect.left;
    int boxTopY = boxRect.top;
    int boxW = std::max(16, (int)(boxRect.right - boxRect.left));
    int boxH = std::max(16, (int)(boxRect.bottom - boxRect.top));

    FontFamily ff(L"Segoe UI");
    std::wstring fullText = text + suffix;
    float fontSize = GetFittedPillFontSize(g, ff, fullText, boxW, boxH);
    Font pillFont(&ff, fontSize, FontStyleRegular, UnitPixel);

    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces);

    float textLeftX = GetCenteredPillTextLeftX(g, pillFont, fullText, boxLeft, boxW);

    GraphicsPath vPath;
    vPath.AddString(L"0123456789%", -1, &ff, FontStyleRegular, fontSize, PointF(0.0f, 0.0f), &sf);
    RectF vBounds;
    vPath.GetBounds(&vBounds);
    float digitsMidY = (vBounds.Height > 0.1f)
        ? (vBounds.Y + vBounds.Height * 0.5f)
        : (fontSize * 0.72f);
    float textTopY = (float)boxTopY + (float)boxH * 0.5f - digitsMidY;

    SolidBrush whiteBrush(Color(255, 248, 250, 252));
    SolidBrush suffixBrush(isEditing ? Color(255, 148, 163, 184) : Color(255, 248, 250, 252));

    GraphicsState st = g.Save();
    g.SetClip(Rect(boxLeft + 1, boxTopY + 1, std::max(1, boxW - 2), std::max(1, boxH - 2)), CombineModeIntersect);

    if (isEditing && caretPos != selAnchor && !text.empty()) {
        size_t s0 = std::min(caretPos, selAnchor);
        size_t s1 = std::max(caretPos, selAnchor);
        float x0 = textLeftX + MeasureMonoPrefixWidth(g, pillFont, text, s0);
        float x1 = textLeftX + MeasureMonoPrefixWidth(g, pillFont, text, s1);
        SolidBrush selHighlight(Color(200, 2, 132, 199)); // Sky-600 selection highlight
        g.FillRectangle(&selHighlight, x0, (float)(boxTopY + 3), std::max(2.0f, x1 - x0), (float)(boxH - 6));
    }

    if (!isEditing) {
        g.DrawString(fullText.c_str(), -1, &pillFont, PointF(textLeftX, textTopY), &sf, &whiteBrush);
    } else {
        g.DrawString(text.c_str(), -1, &pillFont, PointF(textLeftX, textTopY), &sf, &whiteBrush);
        float textW = MeasureMonoPrefixWidth(g, pillFont, fullText, text.size());
        g.DrawString(suffix.c_str(), -1, &pillFont, PointF(textLeftX + textW, textTopY), &sf, &suffixBrush);
    }

    if (isEditing) {
        bool blinkOn = ((GetTickCount() / 450) % 2) == 0;
        if (blinkOn) {
            float cx = textLeftX + MeasureMonoPrefixWidth(g, pillFont, text, caretPos);
            Pen caretPen(Color(255, 56, 189, 248), 1.8f);
            g.DrawLine(&caretPen, cx, (float)(boxTopY + 3), cx, (float)(boxTopY + boxH - 3));
        }
    }
    g.Restore(st);
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

bool PepperSnapDaemon::AnnotationIntersectsRect(const Annotation& a, const RECT& rc) {
    float rL = (float)std::min(rc.left, rc.right);
    float rT = (float)std::min(rc.top, rc.bottom);
    float rR = (float)std::max(rc.left, rc.right);
    float rB = (float)std::max(rc.top, rc.bottom);
    if (rR - rL < 1.0f && rB - rT < 1.0f) return false;

    auto ptInRc = [&](float x, float y, float pad) -> bool {
        return x >= rL - pad && x <= rR + pad && y >= rT - pad && y <= rB + pad;
    };
    auto distSeg = [](float px, float py, float x1, float y1, float x2, float y2) -> float {
        float dx = x2 - x1, dy = y2 - y1;
        float l2 = dx * dx + dy * dy;
        if (l2 == 0.0f) return std::hypot(px - x1, py - y1);
        float t = std::max(0.0f, std::min(1.0f, ((px - x1) * dx + (py - y1) * dy) / l2));
        return std::hypot(px - (x1 + t * dx), py - (y1 + t * dy));
    };
    auto segIntersectsRc = [&](float x1, float y1, float x2, float y2, float pad) -> bool {
        float sMinX = std::min(x1, x2) - pad, sMaxX = std::max(x1, x2) + pad;
        float sMinY = std::min(y1, y2) - pad, sMaxY = std::max(y1, y2) + pad;
        if (sMaxX < rL || sMinX > rR || sMaxY < rT || sMinY > rB) return false;
        if (ptInRc(x1, y1, pad) || ptInRc(x2, y2, pad)) return true;
        if (distSeg(rL, rT, x1, y1, x2, y2) <= pad ||
            distSeg(rR, rT, x1, y1, x2, y2) <= pad ||
            distSeg(rR, rB, x1, y1, x2, y2) <= pad ||
            distSeg(rL, rB, x1, y1, x2, y2) <= pad ||
            distSeg((rL + rR) * 0.5f, (rT + rB) * 0.5f, x1, y1, x2, y2) <= pad) {
            return true;
        }
        auto ccw = [](float ax, float ay, float bx, float by, float cx, float cy) -> bool {
            return (cy - ay) * (bx - ax) > (by - ay) * (cx - ax);
        };
        auto segCross = [&](float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy) -> bool {
            return (ccw(ax, ay, cx, cy, dx, dy) != ccw(bx, by, cx, cy, dx, dy)) &&
                   (ccw(ax, ay, bx, by, cx, cy) != ccw(ax, ay, bx, by, dx, dy));
        };
        return segCross(x1, y1, x2, y2, rL, rT, rR, rT) ||
               segCross(x1, y1, x2, y2, rR, rT, rR, rB) ||
               segCross(x1, y1, x2, y2, rR, rB, rL, rB) ||
               segCross(x1, y1, x2, y2, rL, rB, rL, rT);
    };

    float pad = std::max(3.0f, a.strokeWidth * 0.5f);
    if ((a.tool == OverlayTool::Pen || a.tool == OverlayTool::Highlighter) && !a.points.empty()) {
        if (a.points.size() == 1) {
            return ptInRc(a.points[0].x, a.points[0].y, pad);
        }
        for (size_t i = 0; i + 1 < a.points.size(); ++i) {
            if (segIntersectsRc(a.points[i].x, a.points[i].y, a.points[i + 1].x, a.points[i + 1].y, pad)) {
                return true;
            }
        }
        return false;
    }
    if (a.tool == OverlayTool::Line || a.tool == OverlayTool::Arrow || a.tool == OverlayTool::NumberArrow) {
        if (a.tool == OverlayTool::NumberArrow) {
            float br = std::max(14.0f, a.strokeWidth * 2.0f + 8.0f);
            float cx = std::max(rL, std::min(rR, a.startPt.x));
            float cy = std::max(rT, std::min(rB, a.startPt.y));
            if (std::hypot(a.startPt.x - cx, a.startPt.y - cy) <= br) return true;
        }
        return segIntersectsRc(a.startPt.x, a.startPt.y, a.endPt.x, a.endPt.y, pad);
    }

    if (a.tool == OverlayTool::Rectangle) {
        float x0 = std::min(a.startPt.x, a.endPt.x);
        float x1 = std::max(a.startPt.x, a.endPt.x);
        float y0 = std::min(a.startPt.y, a.endPt.y);
        float y1 = std::max(a.startPt.y, a.endPt.y);
        return segIntersectsRc(x0, y0, x1, y0, pad) ||
               segIntersectsRc(x1, y0, x1, y1, pad) ||
               segIntersectsRc(x1, y1, x0, y1, pad) ||
               segIntersectsRc(x0, y1, x0, y0, pad);
    }

    float minX = std::min(a.startPt.x, a.endPt.x) - pad;
    float maxX = std::max(a.startPt.x, a.endPt.x) + pad;
    float minY = std::min(a.startPt.y, a.endPt.y) - pad;
    float maxY = std::max(a.startPt.y, a.endPt.y) + pad;
    return !(maxX < rL || minX > rR || maxY < rT || minY > rB);
}

int PepperSnapDaemon::HitTestAnnotation(float x, float y, DragMode* outHandleMode) const {
    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;

    auto distToSeg = [](float px, float py, float x1, float y1, float x2, float y2) -> float {
        float dx = x2 - x1, dy = y2 - y1;
        float l2 = dx * dx + dy * dy;
        if (l2 < 1e-4f) return std::hypot(px - x1, py - y1);
        float t = std::max(0.0f, std::min(1.0f, ((px - x1) * dx + (py - y1) * dy) / l2));
        return std::hypot(px - (x1 + t * dx), py - (y1 + t * dy));
    };

    auto hitTriangle = [&](float px, float py, float ax, float ay, float bx, float by, float cx, float cy, float pad) -> bool {
        float d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by);
        float d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy);
        float d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay);
        bool hasNeg = (d1 < 0.0f) || (d2 < 0.0f) || (d3 < 0.0f);
        bool hasPos = (d1 > 0.0f) || (d2 > 0.0f) || (d3 > 0.0f);
        if (!(hasNeg && hasPos)) return true;
        return distToSeg(px, py, ax, ay, bx, by) <= pad ||
               distToSeg(px, py, bx, by, cx, cy) <= pad ||
               distToSeg(px, py, cx, cy, ax, ay) <= pad;
    };

    for (int i = (int)annotations.size() - 1; i >= 0; --i) {
        const auto& a = annotations[i];

        // 1. Endpoint resize handles only when the annotation is already selected
        if (IsAnnotationSelected(a.id) &&
            a.tool != OverlayTool::Pen &&
            a.tool != OverlayTool::Highlighter &&
            a.tool != OverlayTool::TextBox) {
            float dxs = x - a.startPt.x, dys = y - a.startPt.y;
            if (dxs * dxs + dys * dys <= 49.0f) {
                if (outHandleMode) *outHandleMode = DragMode::DraggingAnnotationStart;
                return a.id;
            }
            float dxe = x - a.endPt.x, dye = y - a.endPt.y;
            if (dxe * dxe + dye * dye <= 49.0f) {
                if (outHandleMode) *outHandleMode = DragMode::DraggingAnnotationEnd;
                return a.id;
            }
        }

        // 2. Direct geometry hit-testing per annotation tool (no bounding-box misselection)
        switch (a.tool) {
            case OverlayTool::Pen:
            case OverlayTool::Highlighter: {
                float halfW = (a.tool == OverlayTool::Highlighter)
                    ? std::max(5.0f, a.strokeWidth * 2.25f + 2.0f)
                    : std::max(4.5f, a.strokeWidth * 0.5f + 3.5f);
                if (a.points.size() == 1) {
                    if (std::hypot(x - a.points[0].x, y - a.points[0].y) <= halfW) {
                        if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                        return a.id;
                    }
                } else {
                    for (size_t j = 0; j + 1 < a.points.size(); ++j) {
                        if (distToSeg(x, y, a.points[j].x, a.points[j].y, a.points[j + 1].x, a.points[j + 1].y) <= halfW) {
                            if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                            return a.id;
                        }
                    }
                }
                break;
            }
            case OverlayTool::Line: {
                float halfW = std::max(4.5f, a.strokeWidth * 0.5f + 3.5f);
                if (distToSeg(x, y, a.startPt.x, a.startPt.y, a.endPt.x, a.endPt.y) <= halfW) {
                    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                    return a.id;
                }
                break;
            }
            case OverlayTool::Arrow: {
                float halfW = std::max(4.5f, a.strokeWidth * 0.5f + 3.5f);
                if (distToSeg(x, y, a.startPt.x, a.startPt.y, a.endPt.x, a.endPt.y) <= halfW) {
                    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                    return a.id;
                }
                float dx = a.endPt.x - a.startPt.x, dy = a.endPt.y - a.startPt.y;
                float len = std::hypot(dx, dy);
                if (len >= 2.0f) {
                    float angle = std::atan2(dy, dx);
                    float headLen = std::max(14.0f, a.strokeWidth * 3.8f);
                    float headAngle = 0.48f;
                    float ax = a.endPt.x, ay = a.endPt.y;
                    float bx = a.endPt.x - headLen * std::cos(angle - headAngle);
                    float by = a.endPt.y - headLen * std::sin(angle - headAngle);
                    float cx = a.endPt.x - headLen * std::cos(angle + headAngle);
                    float cy = a.endPt.y - headLen * std::sin(angle + headAngle);
                    if (hitTriangle(x, y, ax, ay, bx, by, cx, cy, 3.0f)) {
                        if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                        return a.id;
                    }
                }
                break;
            }
            case OverlayTool::NumberArrow: {
                float r = std::max(14.0f, a.strokeWidth * 2.0f + 8.0f);
                if (std::hypot(x - a.startPt.x, y - a.startPt.y) <= r + 2.5f) {
                    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                    return a.id;
                }
                float halfW = std::max(4.5f, a.strokeWidth * 0.5f + 3.5f);
                if (distToSeg(x, y, a.startPt.x, a.startPt.y, a.endPt.x, a.endPt.y) <= halfW) {
                    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                    return a.id;
                }
                float dx = a.endPt.x - a.startPt.x, dy = a.endPt.y - a.startPt.y;
                float len = std::hypot(dx, dy);
                if (len > r + 4.0f) {
                    float angle = std::atan2(dy, dx);
                    float headLen = std::max(14.0f, a.strokeWidth * 3.8f);
                    float headAngle = 0.48f;
                    float ax = a.endPt.x, ay = a.endPt.y;
                    float bx = a.endPt.x - headLen * std::cos(angle - headAngle);
                    float by = a.endPt.y - headLen * std::sin(angle - headAngle);
                    float cx = a.endPt.x - headLen * std::cos(angle + headAngle);
                    float cy = a.endPt.y - headLen * std::sin(angle + headAngle);
                    if (hitTriangle(x, y, ax, ay, bx, by, cx, cy, 3.0f)) {
                        if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                        return a.id;
                    }
                }
                break;
            }
            case OverlayTool::Rectangle: {
                float x0 = std::min(a.startPt.x, a.endPt.x);
                float x1 = std::max(a.startPt.x, a.endPt.x);
                float y0 = std::min(a.startPt.y, a.endPt.y);
                float y1 = std::max(a.startPt.y, a.endPt.y);
                float halfW = std::max(4.5f, a.strokeWidth * 0.5f + 3.5f);
                if (distToSeg(x, y, x0, y0, x1, y0) <= halfW ||
                    distToSeg(x, y, x1, y0, x1, y1) <= halfW ||
                    distToSeg(x, y, x1, y1, x0, y1) <= halfW ||
                    distToSeg(x, y, x0, y1, x0, y0) <= halfW) {
                    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                    return a.id;
                }
                break;
            }
            case OverlayTool::Ellipse: {
                float x0 = std::min(a.startPt.x, a.endPt.x);
                float x1 = std::max(a.startPt.x, a.endPt.x);
                float y0 = std::min(a.startPt.y, a.endPt.y);
                float y1 = std::max(a.startPt.y, a.endPt.y);
                float rx = (x1 - x0) * 0.5f;
                float ry = (y1 - y0) * 0.5f;
                float halfW = std::max(4.5f, a.strokeWidth * 0.5f + 3.5f);
                if (rx < 2.0f || ry < 2.0f) {
                    if (distToSeg(x, y, a.startPt.x, a.startPt.y, a.endPt.x, a.endPt.y) <= halfW) {
                        if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                        return a.id;
                    }
                } else {
                    float cx = (x0 + x1) * 0.5f;
                    float cy = (y0 + y1) * 0.5f;
                    float dx = x - cx;
                    float dy = y - cy;
                    float normLen = std::hypot(dx / rx, dy / ry);
                    if (normLen > 1e-4f) {
                        float radialDist = std::hypot(dx, dy) * std::abs(1.0f - 1.0f / normLen);
                        if (radialDist <= halfW) {
                            if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                            return a.id;
                        }
                    }
                }
                break;
            }
            case OverlayTool::TextBox: {
                float minX = std::min(a.startPt.x, a.endPt.x) - 2.0f;
                float maxX = std::max(a.startPt.x, a.endPt.x) + 2.0f;
                float minY = std::min(a.startPt.y, a.endPt.y) - 2.0f;
                float maxY = std::max(a.startPt.y, a.endPt.y) + 2.0f;
                if (x >= minX && x <= maxX && y >= minY && y <= maxY) {
                    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                    return a.id;
                }
                break;
            }
            case OverlayTool::MosaicSquare: {
                float minX = std::min(a.startPt.x, a.endPt.x) - 3.0f;
                float maxX = std::max(a.startPt.x, a.endPt.x) + 3.0f;
                float minY = std::min(a.startPt.y, a.endPt.y) - 3.0f;
                float maxY = std::max(a.startPt.y, a.endPt.y) + 3.0f;
                if (x >= minX && x <= maxX && y >= minY && y <= maxY) {
                    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                    return a.id;
                }
                break;
            }
            case OverlayTool::MosaicCircle: {
                float x0 = std::min(a.startPt.x, a.endPt.x);
                float x1 = std::max(a.startPt.x, a.endPt.x);
                float y0 = std::min(a.startPt.y, a.endPt.y);
                float y1 = std::max(a.startPt.y, a.endPt.y);
                float rx = std::max(2.0f, (x1 - x0) * 0.5f + 3.0f);
                float ry = std::max(2.0f, (y1 - y0) * 0.5f + 3.0f);
                float cx = (x0 + x1) * 0.5f;
                float cy = (y0 + y1) * 0.5f;
                float nx = (x - cx) / rx;
                float ny = (y - cy) / ry;
                if (nx * nx + ny * ny <= 1.0f) {
                    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                    return a.id;
                }
                break;
            }
            default:
                break;
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

    Bitmap* out = nullptr;
    Bitmap* activeImageSource = (!overlayFrames.empty() && overlayActiveFrameIdx < overlayFrames.size())
        ? overlayFrames[overlayActiveFrameIdx]
        : overlaySingleImageBmp;
    if (activeImageSource && overlayImgDrawRect.right > overlayImgDrawRect.left && overlayImgDrawRect.bottom > overlayImgDrawRect.top) {
        out = new Bitmap(rw, rh, PixelFormat32bppARGB);
        if (out) {
            Graphics gOut(out);
            gOut.SetCompositingMode(CompositingModeSourceCopy);
            gOut.Clear(Color(0, 0, 0, 0));
            gOut.SetCompositingMode(CompositingModeSourceOver);
            gOut.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            gOut.SetPixelOffsetMode(PixelOffsetModeHalf);
            int drawX = overlayImgDrawRect.left - rx;
            int drawY = overlayImgDrawRect.top - ry;
            int drawW = overlayImgDrawRect.right - overlayImgDrawRect.left;
            int drawH = overlayImgDrawRect.bottom - overlayImgDrawRect.top;
            ImageAttributes ia;
            ia.SetWrapMode(WrapModeTileFlipXY);
            gOut.DrawImage(activeImageSource, Rect(drawX, drawY, drawW, drawH),
                           0, 0, (INT)activeImageSource->GetWidth(), (INT)activeImageSource->GetHeight(), UnitPixel, &ia);
        }
    }
    if (!out) {
        out = frozenDesktopBmp->Clone(rx, ry, rw, rh, PixelFormat32bppARGB);
    }
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
        case DBTN_ACT_ZOOM_OUT: {
            // Magnifying glass with '-' inside the glass circle
            float gcx = cx - 1.8f;
            float gcy = cy - 1.8f;
            float gr  = 6.2f;
            g.DrawEllipse(&whitePen, gcx - gr, gcy - gr, gr * 2.0f, gr * 2.0f);

            Pen handlePen(Color(255, 248, 250, 252), 2.2f);
            handlePen.SetStartCap(LineCapRound);
            handlePen.SetEndCap(LineCapRound);
            g.DrawLine(&handlePen, gcx + 4.4f, gcy + 4.4f, cx + 7.2f, cy + 7.2f);

            Pen symPen(Color(255, 248, 250, 252), 1.7f);
            symPen.SetStartCap(LineCapRound);
            symPen.SetEndCap(LineCapRound);
            g.DrawLine(&symPen, gcx - 2.8f, gcy, gcx + 2.8f, gcy);
            break;
        }
        case DBTN_ACT_ZOOM_IN: {
            // Magnifying glass with '+' inside the glass circle
            float gcx = cx - 1.8f;
            float gcy = cy - 1.8f;
            float gr  = 6.2f;
            g.DrawEllipse(&whitePen, gcx - gr, gcy - gr, gr * 2.0f, gr * 2.0f);

            Pen handlePen(Color(255, 248, 250, 252), 2.2f);
            handlePen.SetStartCap(LineCapRound);
            handlePen.SetEndCap(LineCapRound);
            g.DrawLine(&handlePen, gcx + 4.4f, gcy + 4.4f, cx + 7.2f, cy + 7.2f);

            Pen symPen(Color(255, 248, 250, 252), 1.7f);
            symPen.SetStartCap(LineCapRound);
            symPen.SetEndCap(LineCapRound);
            g.DrawLine(&symPen, gcx - 2.8f, gcy,        gcx + 2.8f, gcy);
            g.DrawLine(&symPen, gcx,        gcy - 2.8f, gcx,        gcy + 2.8f);
            break;
        }
        case DBTN_ACT_ZOOM_RESET: {
            // Magnifying glass with '=' inside the glass circle (Reset to original size)
            float gcx = cx - 1.8f;
            float gcy = cy - 1.8f;
            float gr  = 6.2f;
            g.DrawEllipse(&whitePen, gcx - gr, gcy - gr, gr * 2.0f, gr * 2.0f);

            Pen handlePen(Color(255, 248, 250, 252), 2.2f);
            handlePen.SetStartCap(LineCapRound);
            handlePen.SetEndCap(LineCapRound);
            g.DrawLine(&handlePen, gcx + 4.4f, gcy + 4.4f, cx + 7.2f, cy + 7.2f);

            Pen symPen(Color(255, 248, 250, 252), 1.6f);
            symPen.SetStartCap(LineCapRound);
            symPen.SetEndCap(LineCapRound);
            g.DrawLine(&symPen, gcx - 2.6f, gcy - 1.6f, gcx + 2.6f, gcy - 1.6f);
            g.DrawLine(&symPen, gcx - 2.6f, gcy + 1.6f, gcx + 2.6f, gcy + 1.6f);
            break;
        }
        case DBTN_ACT_PIN_OUTLINE: {
            // Dotted-line outline icon (between Zoom Reset and Hide Toolbar)
            Pen dotPen(Color(255, 248, 250, 252), 1.7f);
            REAL dashVals[2] = { 1.6f, 1.8f };
            dotPen.SetDashPattern(dashVals, 2);
            dotPen.SetLineJoin(LineJoinMiter);
            g.DrawRectangle(&dotPen, cx - 6.5f, cy - 5.5f, 13.0f, 11.0f);
            break;
        }
        case DBTN_ACT_PIN_HIDE_TOOLBAR: {
            // Typical eye icon (Hide / Show toolbar)
            Pen eyePen(Color(255, 248, 250, 252), 1.65f);
            eyePen.SetStartCap(LineCapRound);
            eyePen.SetEndCap(LineCapRound);
            eyePen.SetLineJoin(LineJoinRound);
            GraphicsPath eyePath;
            PointF topPts[3] = {
                PointF(cx - 7.4f, cy),
                PointF(cx,        cy - 4.9f),
                PointF(cx + 7.4f, cy)
            };
            PointF botPts[3] = {
                PointF(cx + 7.4f, cy),
                PointF(cx,        cy + 4.9f),
                PointF(cx - 7.4f, cy)
            };
            eyePath.StartFigure();
            eyePath.AddCurve(topPts, 3, 0.55f);
            eyePath.AddCurve(botPts, 3, 0.55f);
            eyePath.CloseFigure();
            g.DrawPath(&eyePen, &eyePath);
            g.DrawEllipse(&eyePen, cx - 2.2f, cy - 2.2f, 4.4f, 4.4f);
            break;
        }
        case DBTN_ACT_PIN_UNFILTER: {
            // Water-drop blur / smooth icon (between Hide Outline and Save As)
            Pen dropPen(Color(255, 248, 250, 252), 1.75f);
            dropPen.SetLineJoin(LineJoinRound);
            dropPen.SetStartCap(LineCapRound);
            dropPen.SetEndCap(LineCapRound);

            float bulbCy = cy + 1.8f;
            float r      = 4.6f;
            GraphicsPath dropPath;
            dropPath.StartFigure();
            dropPath.AddLine(PointF(cx, cy - 6.8f), PointF(cx + 3.98f, bulbCy - 2.3f));
            dropPath.AddArc(cx - r, bulbCy - r, r * 2.0f, r * 2.0f, -30.0f, 240.0f);
            dropPath.CloseFigure();
            g.DrawPath(&dropPen, &dropPath);

            Pen shinePen(Color(220, 248, 250, 252), 1.3f);
            shinePen.SetStartCap(LineCapRound);
            shinePen.SetEndCap(LineCapRound);
            g.DrawArc(&shinePen, cx - 2.4f, bulbCy - 2.4f, 4.8f, 4.8f, 20.0f, 80.0f);
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
        case DBTN_ACT_FRAME_PLAY_PAUSE: {
            // Play triangle on left + Pause bars on right
            PointF tri[3] = {
                PointF(cx - 6.8f, cy - 5.5f),
                PointF(cx - 6.8f, cy + 5.5f),
                PointF(cx + 0.2f, cy)
            };
            g.FillPolygon(&whiteBrush, tri, 3);
            g.FillRectangle(&whiteBrush, cx + 2.0f, cy - 5.5f, 2.2f, 11.0f);
            g.FillRectangle(&whiteBrush, cx + 5.4f, cy - 5.5f, 2.2f, 11.0f);
            break;
        }
        case DBTN_ACT_FRAME_TOGGLE: {
            // Frame strip cards + dropdown chevron below
            Pen thinFramePen(Color(255, 248, 250, 252), 1.35f);
            g.DrawRectangle(&whitePen, cx - 7.0f, cy - 6.2f, 14.0f, 7.6f);
            g.DrawLine(&thinFramePen, cx - 2.3f, cy - 6.2f, cx - 2.3f, cy + 1.4f);
            g.DrawLine(&thinFramePen, cx + 2.3f, cy - 6.2f, cx + 2.3f, cy + 1.4f);
            Pen chevPen(Color(255, 248, 250, 252), 1.85f);
            chevPen.SetStartCap(LineCapRound);
            chevPen.SetEndCap(LineCapRound);
            g.DrawLine(&chevPen, cx - 4.0f, cy + 3.8f, cx, cy + 7.2f);
            g.DrawLine(&chevPen, cx, cy + 7.2f, cx + 4.0f, cy + 3.8f);
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

    struct ToolEntry { OverlayTool t; const wchar_t* lbl; std::wstring tip; };
    ToolEntry tools[] = {
        { OverlayTool::SelectMove,   L"Sel", FormatLabelWithShortcut(L"Select mode",         hkToolSelect) },
        { OverlayTool::Pen,          L"Pen", FormatLabelWithShortcut(L"Pen",                 hkToolPen) },
        { OverlayTool::Highlighter,  L"Hi",  FormatLabelWithShortcut(L"Stabilo highlighter", hkToolStabilo) },
        { OverlayTool::Line,         L"Lin", FormatLabelWithShortcut(L"Line",                hkToolLine) },
        { OverlayTool::Arrow,        L"Arr", FormatLabelWithShortcut(L"Arrow",               hkToolArrow) },
        { OverlayTool::NumberArrow,  L"1→",  FormatLabelWithShortcut(L"Numbering arrow",     hkToolNumber) },
        { OverlayTool::Rectangle,    L"Box", FormatLabelWithShortcut(L"Square / rectangle",  hkToolRect) },
        { OverlayTool::Ellipse,      L"Cir", FormatLabelWithShortcut(L"Circle / ellipse",    hkToolEllipse) },
        { OverlayTool::TextBox,      L"Txt", FormatLabelWithShortcut(L"Text box",            hkToolText) },
        { OverlayTool::MosaicSquare, L"MSq", FormatLabelWithShortcut(L"Mosaic square",       hkToolMosaicSq,  L" — pixelation level is affected by size selection") },
        { OverlayTool::MosaicCircle, L"MCi", FormatLabelWithShortcut(L"Mosaic circle",       hkToolMosaicCir, L" — pixelation level is affected by size selection") }
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
    struct ActEntry { int id; int w; const wchar_t* lbl; std::wstring tip; bool primary; };
    ActEntry acts[] = {
        { DBTN_ACT_DRAG_HUD, toolBtnW, L"", L"Drag to move toolbar (resets when selection moves)", false },
        { DBTN_ACT_UNDO,     toolBtnW, L"", FormatLabelWithShortcut(L"Undo",                     hkActUndo),    false },
        { DBTN_ACT_REDO,     toolBtnW, L"", FormatLabelWithShortcut(L"Redo",                     hkActRedo),    false },
        { DBTN_ACT_CLEAR,    toolBtnW, L"", FormatLabelWithShortcut(L"Clear all annotations",    hkActClear),   false },
        { DBTN_ACT_OPTIONS,  toolBtnW, L"", FormatLabelWithShortcut(L"Options",                  hkActOptions), false },
        { DBTN_ACT_PIN,      toolBtnW, L"", FormatLabelWithShortcut(L"Pin on top",               hkActPin),     false },
        { DBTN_ACT_OCR,      toolBtnW, L"", FormatLabelWithShortcut(L"Extract text with OCR",    hkActOcr),     false },
        { DBTN_ACT_SAVE_AS,  toolBtnW, L"", FormatLabelWithShortcut(L"Save as JPG/PNG/WEBP/BMP", hkActSaveAs),  false },
        { DBTN_ACT_SAVE,     toolBtnW, L"", FormatLabelWithShortcut(L"Quick save",               hkActSave),    false },
        { DBTN_ACT_COPY,     toolBtnW, L"", FormatLabelWithShortcut(L"Copy to clipboard",        hkActCopy),    true },
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
        int bottomExtra = (pillRight && pillBottom) ? 36 : 0;
        int topExtra    = (pillRight && !pillBottom) ? 36 : 0;

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
            b.tooltip = FormatLabelWithShortcut(L"Numbering arrow", hkToolNumber) + L" — next: " + std::to_wstring(nextStepNum);
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
            rb.tooltip = FormatLabelWithShortcut(L"Reset numbering arrow counter to 1", hkToolResetNum) + L" (next: " + std::to_wstring(nextStepNum) + L")";
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

static void GetPinnedScaleBounds(const PinnedWindowData* data, float& outMinScale, float& outMaxScale) {
    int ow = (data && data->origW > 0) ? data->origW : 32;
    int oh = (data && data->origH > 0) ? data->origH : 32;
    outMinScale = std::max(0.01f, std::max(8.0f / (float)ow, 8.0f / (float)oh));
    outMaxScale = std::max(outMinScale, std::min(64.0f, std::min(8192.0f / (float)ow, 8192.0f / (float)oh)));
}

static void GetPinnedScaledDims(const PinnedWindowData* data, int& outW, int& outH) {
    if (!data || !data->bmp) {
        outW = 32;
        outH = 32;
        return;
    }
    int ow = std::max(1, data->origW);
    int oh = std::max(1, data->origH);
    float minScale = 0.01f, maxScale = 64.0f;
    GetPinnedScaleBounds(data, minScale, maxScale);
    double s = std::max((double)minScale, std::min((double)maxScale, (double)data->scale));
    outW = std::max(1, (int)std::round(ow * s));
    outH = std::max(1, (int)std::round(oh * s));
}

static void FreePinnedWindowCache(PinnedWindowData* data) {
    if (!data) return;
    if (data->hCacheDC) {
        if (data->hCacheOldBmp) {
            SelectObject(data->hCacheDC, data->hCacheOldBmp);
            data->hCacheOldBmp = nullptr;
        }
        DeleteDC(data->hCacheDC);
        data->hCacheDC = nullptr;
    }
    if (data->hCacheBmp) {
        DeleteObject(data->hCacheBmp);
        data->hCacheBmp = nullptr;
    }
    data->cachePixels = nullptr;
    data->cachedImgW = 0;
    data->cachedImgH = 0;
    data->cachedFrameIdx = (size_t)-1;
    data->cachedHq = false;
}

static RECT GetPinnedMonitorWorkArea(HMONITOR hMon) {
    RECT rcSafe = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcSafe, 0);
    MONITORINFO mi = { sizeof(mi) };
    if (hMon && GetMonitorInfoW(hMon, &mi)) {
        rcSafe = mi.rcWork;
        const wchar_t* tbClasses[2] = { L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd" };
        for (int i = 0; i < 2; ++i) {
            HWND hTb = nullptr;
            while ((hTb = FindWindowExW(nullptr, hTb, tbClasses[i], nullptr)) != nullptr) {
                if (!IsWindowVisible(hTb)) continue;
                if (MonitorFromWindow(hTb, MONITOR_DEFAULTTONEAREST) != hMon) continue;
                RECT rcTb = {0};
                if (!GetWindowRect(hTb, &rcTb)) continue;
                int tbW = rcTb.right - rcTb.left;
                int tbH = rcTb.bottom - rcTb.top;
                int monW = mi.rcMonitor.right - mi.rcMonitor.left;
                int monH = mi.rcMonitor.bottom - mi.rcMonitor.top;
                if (tbW >= monW / 2 && tbH < monH / 2) {
                    if (rcTb.top > mi.rcMonitor.top && rcTb.bottom >= mi.rcMonitor.bottom - 4) {
                        rcSafe.bottom = std::min(rcSafe.bottom, rcTb.top);
                    } else if (rcTb.bottom < mi.rcMonitor.bottom && rcTb.top <= mi.rcMonitor.top + 4) {
                        rcSafe.top = std::max(rcSafe.top, rcTb.bottom);
                    }
                } else if (tbH >= monH / 2 && tbW < monW / 2) {
                    if (rcTb.left > mi.rcMonitor.left && rcTb.right >= mi.rcMonitor.right - 4) {
                        rcSafe.right = std::min(rcSafe.right, rcTb.left);
                    } else if (rcTb.right < mi.rcMonitor.right && rcTb.left <= mi.rcMonitor.left + 4) {
                        rcSafe.left = std::max(rcSafe.left, rcTb.right);
                    }
                }
            }
        }
    }
    return rcSafe;
}

static HWND g_hPinTooltipWnd = nullptr;

static void HidePinnedBubbleTooltip() {
    if (g_hPinTooltipWnd && IsWindow(g_hPinTooltipWnd)) {
        ShowWindow(g_hPinTooltipWnd, SW_HIDE);
    }
}

static void ShowPinnedBubbleTooltip(HWND hPinWnd, const DockButton& b) {
    if (!hPinWnd || b.tooltip.empty()) {
        HidePinnedBubbleTooltip();
        return;
    }
    static bool s_tipReg = false;
    if (!s_tipReg) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = g_Daemon.hInst;
        wc.lpszClassName = L"PepperSnapPinBubbleTipWnd";
        RegisterClassExW(&wc);
        s_tipReg = true;
    }
    if (!g_hPinTooltipWnd || !IsWindow(g_hPinTooltipWnd)) {
        g_hPinTooltipWnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
            L"PepperSnapPinBubbleTipWnd", L"",
            WS_POPUP, 0, 0, 1, 1,
            nullptr, nullptr, g_Daemon.hInst, nullptr
        );
    }
    if (!g_hPinTooltipWnd) return;

    HDC hScreen = GetDC(nullptr);
    if (!hScreen) return;

    FontFamily ff(L"Segoe UI");
    Font tipFont(&ff, 11.5f, FontStyleBold, UnitPixel);
    RectF measured;
    {
        Graphics gMeasure(hScreen);
        gMeasure.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        gMeasure.MeasureString(b.tooltip.c_str(), -1, &tipFont, PointF(0, 0), &measured);
    }

    int bw = (int)std::ceil(measured.Width) + 18;
    int bh = 26;
    int tailH = 7;
    int totalW = bw + 4;
    int totalH = bh + tailH + 4;

    POINT ptTopLeft = { b.rect.left, b.rect.top };
    POINT ptBotRight = { b.rect.right, b.rect.bottom };
    ClientToScreen(hPinWnd, &ptTopLeft);
    ClientToScreen(hPinWnd, &ptBotRight);

    int btnMidX = (ptTopLeft.x + ptBotRight.x) / 2;
    HMONITOR hMon = MonitorFromWindow(hPinWnd, MONITOR_DEFAULTTONEAREST);
    RECT rcScreen = GetPinnedMonitorWorkArea(hMon);

    // Prefer placing pop-up bubble below the pin toolbar unless near the bottom/taskbar
    bool placeBelow = (ptBotRight.y + totalH + 4 <= rcScreen.bottom);
    int winX = std::max((int)rcScreen.left + 6, std::min((int)rcScreen.right - totalW - 6, btnMidX - totalW / 2));
    int winY = placeBelow ? (ptBotRight.y + 2) : (ptTopLeft.y - totalH - 2);
    float localMidX = (float) std::max(10, std::min(totalW - 10, btnMidX - winX));

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = totalW;
    bmi.bmiHeader.biHeight = -totalH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* pvBits = nullptr;
    HDC memDC = CreateCompatibleDC(hScreen);
    HBITMAP hBmp = CreateDIBSection(hScreen, &bmi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
    if (memDC && hBmp && pvBits) {
        HGDIOBJ oldBmp = SelectObject(memDC, hBmp);
        memset(pvBits, 0, (size_t)totalW * (size_t)totalH * sizeof(DWORD));
        {
            Graphics g(memDC);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

            float rx = 2.0f;
            float ry = placeBelow ? (float)tailH : 2.0f;
            float rw = (float)bw;
            float rh = (float)bh;

            PointF tail[3];
            if (placeBelow) {
                tail[0] = PointF(localMidX, 1.0f);
                tail[1] = PointF(localMidX - 5.5f, ry + 1.0f);
                tail[2] = PointF(localMidX + 5.5f, ry + 1.0f);
            } else {
                tail[0] = PointF(localMidX, ry + rh + (float)tailH - 1.0f);
                tail[1] = PointF(localMidX - 5.5f, ry + rh - 1.0f);
                tail[2] = PointF(localMidX + 5.5f, ry + rh - 1.0f);
            }

            SolidBrush bubbleBg(Color(248, 15, 23, 42));
            Pen bubbleBorder(Color(255, 239, 68, 68), 1.4f);
            g.FillPolygon(&bubbleBg, tail, 3);
            g.FillRectangle(&bubbleBg, rx, ry, rw, rh);
            g.DrawRectangle(&bubbleBorder, rx, ry, rw, rh);

            StringFormat sf;
            sf.SetAlignment(StringAlignmentCenter);
            sf.SetLineAlignment(StringAlignmentCenter);
            SolidBrush textBr(Color(255, 248, 250, 252));
            RectF textRc(rx, ry, rw, rh);
            g.DrawString(b.tooltip.c_str(), -1, &tipFont, textRc, &sf, &textBr);
        }
        GdiFlush();
        DWORD* pxBuf = (DWORD*)pvBits;
        size_t totalPx = (size_t)totalW * (size_t)totalH;
        for (size_t idx = 0; idx < totalPx; ++idx) {
            DWORD p = pxBuf[idx];
            if ((p >> 24) == 0 && (p & 0x00FFFFFFu) != 0) {
                DWORD rCh = ((p >> 16) & 0xFFu) * 248u / 255u;
                DWORD gCh = ((p >> 8) & 0xFFu) * 248u / 255u;
                DWORD bCh = (p & 0xFFu) * 248u / 255u;
                pxBuf[idx] = (248u << 24) | (rCh << 16) | (gCh << 8) | bCh;
            }
        }

        POINT ptSrc = { 0, 0 };
        POINT ptDst = { winX, winY };
        SIZE szWnd = { totalW, totalH };
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        UpdateLayeredWindow(g_hPinTooltipWnd, hScreen, &ptDst, &szWnd, memDC, &ptSrc, 0, &bf, ULW_ALPHA);
        SetWindowPos(g_hPinTooltipWnd, HWND_TOPMOST, winX, winY, totalW, totalH, SWP_NOACTIVATE | SWP_SHOWWINDOW);

        SelectObject(memDC, oldBmp);
    }
    if (hBmp) DeleteObject(hBmp);
    if (memDC) DeleteDC(memDC);
    ReleaseDC(nullptr, hScreen);
}

static void ShowPinnedImageHoverTooltip(HWND hPinWnd, const PinnedWindowData* data, POINT ptCursorScreen) {
    if (!hPinWnd || !data) {
        HidePinnedBubbleTooltip();
        return;
    }
    bool isAnim = (data->animFrames.size() > 1);
    std::vector<const wchar_t*> lines = {
        data->hideToolbar ? L"Double-click to show toolbar" : L"Double-click to hide toolbar",
        L"Double right-click to close",
        L"Scroll to resize",
        L"Middle-click to reset size"
    };
    if (isAnim) {
        lines.push_back(data->animPaused ? L"Ctrl + click to play" : L"Ctrl + click to pause");
    }
    int shiftStartIdx = (int)lines.size();
    lines.push_back(L"Hold Shift + action / key");
    lines.push_back(L"to apply to all pinned images");
    int lineCount = (int)lines.size();
    static bool s_tipReg = false;
    if (!s_tipReg) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = g_Daemon.hInst;
        wc.lpszClassName = L"PepperSnapPinBubbleTipWnd";
        RegisterClassExW(&wc);
        s_tipReg = true;
    }
    if (!g_hPinTooltipWnd || !IsWindow(g_hPinTooltipWnd)) {
        g_hPinTooltipWnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
            L"PepperSnapPinBubbleTipWnd", L"",
            WS_POPUP, 0, 0, 1, 1,
            nullptr, nullptr, g_Daemon.hInst, nullptr
        );
    }
    if (!g_hPinTooltipWnd) return;

    HDC hScreen = GetDC(nullptr);
    if (!hScreen) return;

    FontFamily ff(L"Segoe UI");
    Font tipFont(&ff, 11.5f, FontStyleBold, UnitPixel);
    float maxLineW = 0.0f;
    {
        Graphics gMeasure(hScreen);
        gMeasure.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        for (int i = 0; i < lineCount; ++i) {
            RectF measured;
            gMeasure.MeasureString(lines[i], -1, &tipFont, PointF(0, 0), &measured);
            if (measured.Width > maxLineW) maxLineW = measured.Width;
        }
    }

    int bw = (int)std::ceil(maxLineW) + 18;
    int bh = 8 + lineCount * 18;
    int tailH = 7;
    int totalW = bw + 4;
    int totalH = bh + tailH + 4;

    HMONITOR hMon = MonitorFromPoint(ptCursorScreen, MONITOR_DEFAULTTONEAREST);
    RECT rcScreen = GetPinnedMonitorWorkArea(hMon);

    bool placeBelow = (ptCursorScreen.y + 18 + totalH <= rcScreen.bottom);
    int winX = std::max((int)rcScreen.left + 6, std::min((int)rcScreen.right - totalW - 6, (int)ptCursorScreen.x - totalW / 2));
    int winY = placeBelow ? (ptCursorScreen.y + 18) : (ptCursorScreen.y - totalH - 8);
    float localMidX = (float)std::max(10, std::min(totalW - 10, (int)ptCursorScreen.x - winX));

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = totalW;
    bmi.bmiHeader.biHeight = -totalH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* pvBits = nullptr;
    HDC memDC = CreateCompatibleDC(hScreen);
    HBITMAP hBmp = CreateDIBSection(hScreen, &bmi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
    if (memDC && hBmp && pvBits) {
        HGDIOBJ oldBmp = SelectObject(memDC, hBmp);
        memset(pvBits, 0, (size_t)totalW * (size_t)totalH * sizeof(DWORD));
        {
            Graphics g(memDC);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

            float rx = 2.0f;
            float ry = placeBelow ? (float)tailH : 2.0f;
            float rw = (float)bw;
            float rh = (float)bh;

            PointF tail[3];
            if (placeBelow) {
                tail[0] = PointF(localMidX, 1.0f);
                tail[1] = PointF(localMidX - 5.5f, ry + 1.0f);
                tail[2] = PointF(localMidX + 5.5f, ry + 1.0f);
            } else {
                tail[0] = PointF(localMidX, ry + rh + (float)tailH - 1.0f);
                tail[1] = PointF(localMidX - 5.5f, ry + rh - 1.0f);
                tail[2] = PointF(localMidX + 5.5f, ry + rh - 1.0f);
            }

            SolidBrush bubbleBg(Color(248, 15, 23, 42));
            Pen bubbleBorder(Color(255, 239, 68, 68), 1.4f);
            g.FillPolygon(&bubbleBg, tail, 3);
            g.FillRectangle(&bubbleBg, rx, ry, rw, rh);
            g.DrawRectangle(&bubbleBorder, rx, ry, rw, rh);

            StringFormat sf;
            sf.SetAlignment(StringAlignmentCenter);
            sf.SetLineAlignment(StringAlignmentCenter);
            SolidBrush textBr(Color(255, 248, 250, 252));
            SolidBrush accentBr(Color(255, 252, 165, 165));
            for (int i = 0; i < lineCount; ++i) {
                RectF lineRc(rx, ry + 4.0f + i * 18.0f, rw, 18.0f);
                g.DrawString(lines[i], -1, &tipFont, lineRc, &sf, (i >= shiftStartIdx) ? &accentBr : &textBr);
            }
        }
        GdiFlush();
        DWORD* pxBuf = (DWORD*)pvBits;
        size_t totalPx = (size_t)totalW * (size_t)totalH;
        for (size_t idx = 0; idx < totalPx; ++idx) {
            DWORD p = pxBuf[idx];
            if ((p >> 24) == 0 && (p & 0x00FFFFFFu) != 0) {
                DWORD rCh = ((p >> 16) & 0xFFu) * 248u / 255u;
                DWORD gCh = ((p >> 8) & 0xFFu) * 248u / 255u;
                DWORD bCh = (p & 0xFFu) * 248u / 255u;
                pxBuf[idx] = (248u << 24) | (rCh << 16) | (gCh << 8) | bCh;
            }
        }

        POINT ptSrc = { 0, 0 };
        POINT ptDst = { winX, winY };
        SIZE szWnd = { totalW, totalH };
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        UpdateLayeredWindow(g_hPinTooltipWnd, hScreen, &ptDst, &szWnd, memDC, &ptSrc, 0, &bf, ULW_ALPHA);
        SetWindowPos(g_hPinTooltipWnd, HWND_TOPMOST, winX, winY, totalW, totalH, SWP_NOACTIVATE | SWP_SHOWWINDOW);

        SelectObject(memDC, oldBmp);
    }
    if (hBmp) DeleteObject(hBmp);
    if (memDC) DeleteDC(memDC);
    ReleaseDC(nullptr, hScreen);
}

static void EnsurePinnedWindowCache(PinnedWindowData* data, int imgW, int imgH, bool highQuality) {
    if (!data || !data->bmp || imgW <= 0 || imgH <= 0) return;
    bool hideOutline = data->hideOutline;
    bool smooth = data->smoothImage;
    if (data->hCacheDC && data->hCacheBmp && data->cachePixels &&
        data->cachedImgW == imgW && data->cachedImgH == imgH &&
        data->cachedFrameIdx == data->curFrameIdx &&
        data->cachedHideOutline == hideOutline &&
        data->cachedSmooth == smooth &&
        (!smooth || data->cachedHq || !highQuality)) {
        return;
    }

    if (!data->hCacheDC || !data->hCacheBmp || !data->cachePixels || data->cachedImgW != imgW || data->cachedImgH != imgH) {
        FreePinnedWindowCache(data);
        HDC hScreen = GetDC(nullptr);
        data->hCacheDC = CreateCompatibleDC(hScreen);
        BITMAPINFO bmi = {0};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = imgW;
        bmi.bmiHeader.biHeight = -imgH;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void* pvBits = nullptr;
        data->hCacheBmp = CreateDIBSection(hScreen, &bmi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
        data->cachePixels = (DWORD*)pvBits;
        if (hScreen) ReleaseDC(nullptr, hScreen);
        if (!data->hCacheDC || !data->hCacheBmp || !data->cachePixels) {
            FreePinnedWindowCache(data);
            return;
        }
        data->hCacheOldBmp = SelectObject(data->hCacheDC, data->hCacheBmp);
    }

    memset(data->cachePixels, 0, (size_t)imgW * (size_t)imgH * sizeof(DWORD));
    {
        Bitmap cacheSurface(imgW, imgH, imgW * 4, PixelFormat32bppPARGB, (BYTE*)data->cachePixels);
        Graphics g(&cacheSurface);
        g.SetCompositingMode(CompositingModeSourceCopy);
        g.SetPixelOffsetMode(PixelOffsetModeHalf);

        long long totalPixels = (long long)imgW * (long long)imgH;
        if (!smooth) {
            // Button OFF (default): image is unfiltered so zoomed pixels stay crisp and pixelated
            g.SetInterpolationMode(InterpolationModeNearestNeighbor);
        } else if (!highQuality) {
            // Button ON: smooth filter active
            g.SetInterpolationMode(InterpolationModeBilinear);
        } else {
            g.SetInterpolationMode(totalPixels > (2560LL * 1440LL) ? InterpolationModeBilinear : InterpolationModeHighQualityBicubic);
        }

        ImageAttributes ia;
        ia.SetWrapMode(WrapModeTileFlipXY);
        int srcW = std::max(1, (int)data->bmp->GetWidth());
        int srcH = std::max(1, (int)data->bmp->GetHeight());
        double fitScale = std::min((double)imgW / (double)srcW, (double)imgH / (double)srcH);
        int drawW = std::max(1, std::min(imgW, (int)std::round(srcW * fitScale)));
        int drawH = std::max(1, std::min(imgH, (int)std::round(srcH * fitScale)));
        int drawX = (imgW - drawW) / 2;
        int drawY = (imgH - drawH) / 2;
        g.DrawImage(
            data->bmp,
            Rect(drawX, drawY, drawW, drawH),
            0, 0, srcW, srcH,
            UnitPixel,
            &ia
        );

        if (!hideOutline) {
            g.SetCompositingMode(CompositingModeSourceOver);
            g.SetSmoothingMode(SmoothingModeNone);
            g.SetPixelOffsetMode(PixelOffsetModeNone);
            Pen border1px(Color(255, 239, 68, 68), 1.0f);
            g.DrawRectangle(&border1px, 0, 0, imgW - 1, imgH - 1);
            if (imgW >= 4 && imgH >= 4) {
                g.DrawRectangle(&border1px, 1, 1, imgW - 3, imgH - 3);
            }
        }
    }

    // Ensure transparent pixels inside the image rect have alpha >= 1 (0.39% opacity, visually 100% transparent
    // over desktop while remaining mouse-interactive for dragging, zooming, and double-clicking)
    size_t totalPx = (size_t)imgW * (size_t)imgH;
    for (size_t i = 0; i < totalPx; ++i) {
        if ((data->cachePixels[i] >> 24) == 0) {
            data->cachePixels[i] = 0x01000000u;
        }
    }

    data->cachedImgW = imgW;
    data->cachedImgH = imgH;
    data->cachedFrameIdx = data->curFrameIdx;
    data->cachedHideOutline = hideOutline;
    data->cachedSmooth = smooth;
    data->cachedHq = !smooth ? true : highQuality;
}

static void RenderPinnedLayeredWindow(HWND hWnd, PinnedWindowData* data) {
    if (!hWnd || !IsWindow(hWnd) || !data || !data->bmp) return;
    int imgW = 32, imgH = 32;
    GetPinnedScaledDims(data, imgW, imgH);
    EnsurePinnedWindowCache(data, imgW, imgH, data->cachedHq || ((long long)imgW * (long long)imgH <= 960LL * 540LL));
    if (!data->cachePixels) return;

    int winW = std::max(1, data->winW);
    int winH = std::max(1, data->winH);

    HDC hScreen = GetDC(nullptr);
    if (!hScreen) return;
    HDC memDC = CreateCompatibleDC(hScreen);
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = winW;
    bmi.bmiHeader.biHeight = -winH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* pvBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hScreen, &bmi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
    if (memDC && hBmp && pvBits) {
        HGDIOBJ oldBmp = SelectObject(memDC, hBmp);
        DWORD* dstPixels = (DWORD*)pvBits;
        memset(dstPixels, 0, (size_t)winW * (size_t)winH * sizeof(DWORD));

        int imgLeft = data->imgRect.left;
        int imgTop  = data->imgRect.top;
        for (int y = 0; y < imgH; ++y) {
            int dy = imgTop + y;
            if (dy < 0 || dy >= winH) continue;
            int copyX = std::max(0, imgLeft);
            int srcX  = copyX - imgLeft;
            int copyW = std::min(imgW - srcX, winW - copyX);
            if (copyW > 0) {
                memcpy(dstPixels + (size_t)dy * winW + copyX,
                       data->cachePixels + (size_t)y * imgW + srcX,
                       (size_t)copyW * sizeof(DWORD));
            }
        }

        bool suppressToolbar = data->hideToolbar || (data->dragging && data->dragMoved);
        if (!suppressToolbar && !data->buttons.empty()) {
            Bitmap surface(winW, winH, winW * 4, PixelFormat32bppPARGB, (BYTE*)pvBits);
            Graphics g(&surface);
            g.SetCompositingMode(CompositingModeSourceOver);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

            for (const auto& b : data->buttons) {
                int bx = (int)b.rect.left;
                int by = (int)b.rect.top;
                int bw = (int)(b.rect.right - b.rect.left);
                int bh = (int)(b.rect.bottom - b.rect.top);
                RectF rf((float)bx, (float)by, (float)bw, (float)bh);
                bool hovered = (b.id == data->hoveredBtnId);
                bool pressed = (b.id == data->pressedBtnId && hovered);
                bool activeToggleBtn = (b.id == DBTN_ACT_PIN_OUTLINE && !data->hideOutline) ||
                                       (b.id == DBTN_ACT_PIN_UNFILTER && data->smoothImage);
                Color bgCol = Color(245, 15, 23, 42);
                if (b.isPrimaryAction) {
                    bgCol = pressed ? Color(255, 185, 28, 28) : (hovered ? Color(255, 220, 38, 38) : Color(255, 239, 68, 68));
                } else if (activeToggleBtn) {
                    bgCol = pressed ? Color(255, 71, 85, 105) : (hovered ? Color(255, 51, 65, 85) : Color(255, 30, 41, 59));
                } else if (pressed) {
                    bgCol = Color(255, 71, 85, 105);
                } else if (hovered) {
                    bgCol = Color(255, 51, 65, 85);
                }
                SolidBrush btnBg(bgCol);
                Pen btnBorder(activeToggleBtn ? Color(255, 239, 68, 68) : Color(235, 51, 65, 85), 1.0f);

                GraphicsState stBtn = g.Save();
                g.SetSmoothingMode(SmoothingModeNone);
                g.SetPixelOffsetMode(PixelOffsetModeNone);
                g.FillRectangle(&btnBg, bx, by, bw, bh);
                g.Restore(stBtn);

                PepperSnapDaemon::DrawDockButtonIcon(g, b, rf);

                GraphicsState stOutline = g.Save();
                g.SetSmoothingMode(SmoothingModeNone);
                g.SetPixelOffsetMode(PixelOffsetModeNone);
                g.DrawRectangle(&btnBorder, bx, by, bw - 1, bh - 1);
                g.Restore(stOutline);
            }

            if (data->sizeRowRect.right > data->sizeRowRect.left) {
                int rx = data->sizeRowRect.left;
                int ry = data->sizeRowRect.top;
                int rw = data->sizeRowRect.right - data->sizeRowRect.left;
                int rh = data->sizeRowRect.bottom - data->sizeRowRect.top;

                int sbx = data->sizeBoxRect.left;
                int sbw = data->sizeBoxRect.right - data->sizeBoxRect.left;
                int pbx = data->pctBoxRect.left;
                int pbw = data->pctBoxRect.right - data->pctBoxRect.left;

                Color sizeBgCol = data->isEditingSize
                    ? Color(252, 30, 41, 59)
                    : (data->hoveredSizeBox ? Color(250, 30, 41, 59) : Color(245, 15, 23, 42));
                Color pctBgCol = data->isEditingPct
                    ? Color(252, 30, 41, 59)
                    : (data->hoveredPctBox ? Color(250, 30, 41, 59) : Color(245, 15, 23, 42));

                SolidBrush sizeBg(sizeBgCol);
                SolidBrush pctBg(pctBgCol);
                Pen outerBorder(Color(235, 51, 65, 85), 1.0f);
                Pen divPen(Color(235, 71, 85, 105), 1.0f);

                GraphicsState stBoxes = g.Save();
                g.SetSmoothingMode(SmoothingModeNone);
                g.SetPixelOffsetMode(PixelOffsetModeNone);
                g.FillRectangle(&sizeBg, sbx, ry, sbw, rh);
                g.FillRectangle(&pctBg, pbx, ry, pbw, rh);
                g.DrawRectangle(&outerBorder, rx, ry, rw - 1, rh - 1);
                g.DrawLine(&divPen, pbx, ry + 4, pbx, ry + rh - 5);

                if (data->isEditingSize || data->hoveredSizeBox) {
                    Pen hiPen(data->isEditingSize ? Color(255, 56, 189, 248) : Color(255, 239, 68, 68), 1.0f);
                    g.DrawRectangle(&hiPen, sbx, ry, sbw - 1, rh - 1);
                }
                if (data->isEditingPct || data->hoveredPctBox) {
                    Pen hiPen(data->isEditingPct ? Color(255, 56, 189, 248) : Color(255, 239, 68, 68), 1.0f);
                    g.DrawRectangle(&hiPen, pbx, ry, pbw - 1, rh - 1);
                }
                g.Restore(stBoxes);

                std::wstring dimValStr = data->isEditingSize
                    ? data->editingSizeText
                    : (std::to_wstring(imgW) + L"\x00D7" + std::to_wstring(imgH));
                PepperSnapDaemon::DrawEditablePillText(
                    g, data->sizeBoxRect,
                    dimValStr, L" px",
                    data->isEditingSize, data->sizeCaretPos, data->sizeSelAnchor
                );

                int pctVal = std::max(1, (int)std::round(data->scale * 100.0f));
                std::wstring pctValStr = data->isEditingPct
                    ? data->editingPctText
                    : std::to_wstring(pctVal);
                PepperSnapDaemon::DrawEditablePillText(
                    g, data->pctBoxRect,
                    pctValStr, L"%",
                    data->isEditingPct, data->pctCaretPos, data->pctSelAnchor
                );
            }
        }

        POINT ptSrc = { 0, 0 };
        POINT ptDst = { data->winX, data->winY };
        SIZE szWnd = { winW, winH };
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        UpdateLayeredWindow(hWnd, hScreen, &ptDst, &szWnd, memDC, &ptSrc, 0, &bf, ULW_ALPHA);

        SelectObject(memDC, oldBmp);
    }
    if (hBmp) DeleteObject(hBmp);
    if (memDC) DeleteDC(memDC);
    ReleaseDC(nullptr, hScreen);
}

static void UpdatePinnedWindowLayout(HWND hWnd, PinnedWindowData* data, bool resizeWindow = true) {
    if (!data || !data->bmp) return;
    int imgW = 32, imgH = 32;
    GetPinnedScaledDims(data, imgW, imgH);

    const int toolBtnW = 32;
    const int actBtnH  = 28;
    const int gap      = 3;
    const int hudGap   = 8; // Same 8px gap between toolbar and custom area

    struct PinBtnSpec { int id; int w; const wchar_t* lbl; std::wstring tip; bool primary; };
    const wchar_t* outlineLbl = data->hideOutline ? L"Show outline" : L"Hide outline";
    PinBtnSpec specs[6] = {
        { DBTN_ACT_PIN_OUTLINE,  toolBtnW,           L"",     FormatLabelWithShortcut(outlineLbl,                  g_Daemon.hkPinOutline),  false },
        { DBTN_ACT_PIN_UNFILTER, toolBtnW,           L"",     FormatLabelWithShortcut(L"Smooth the image",         g_Daemon.hkPinSmooth),   false },
        { DBTN_ACT_SAVE_AS,      toolBtnW,           L"",     FormatLabelWithShortcut(L"Save as JPG/PNG/WEBP/BMP", g_Daemon.hkPinSaveAs),   false },
        { DBTN_ACT_SAVE,         toolBtnW,           L"",     FormatLabelWithShortcut(L"Quick save",               g_Daemon.hkPinSave),     false },
        { DBTN_ACT_COPY,         toolBtnW * 2 + gap, L"Copy", FormatLabelWithShortcut(L"Copy to clipboard",        g_Daemon.hkPinCopy),     true  },
        { DBTN_ACT_CLOSE,        toolBtnW,           L"",     FormatLabelWithShortcut(L"Close pinned image",       g_Daemon.hkPinClose),    false }
    };

    int stripW = 0;
    for (int i = 0; i < 6; ++i) {
        stripW += specs[i].w + (i > 0 ? gap : 0);
    }
    int totalHudH = actBtnH * 2 + gap;

    int sImgL = data->screenImgX;
    int sImgT = data->screenImgY;
    int sImgR = sImgL + imgW;
    int sImgB = sImgT + imgH;

    RECT rcImgScreen = { sImgL, sImgT, sImgR, sImgB };
    HMONITOR hMon = MonitorFromRect(&rcImgScreen, MONITOR_DEFAULTTONEAREST);
    RECT rcScreen = GetPinnedMonitorWorkArea(hMon);

    int sStripR = std::max((int)rcScreen.left + stripW + 6, std::min((int)rcScreen.right - 6, sImgR));
    int sStripT = sImgB + hudGap;

    bool hasSpaceBelow = (sImgB + hudGap + totalHudH <= rcScreen.bottom - 6) && (sImgB + hudGap >= rcScreen.top + 6);
    bool hasSpaceAbove = (sImgT - hudGap - totalHudH >= rcScreen.top + 6) && (sImgT - hudGap <= rcScreen.bottom - 6);

    if (hasSpaceBelow) {
        // 1. Default: snap below bottom-right corner of pinned image
        sStripT = sImgB + hudGap;
    } else if (hasSpaceAbove) {
        // 2. No space below -> snap above top-right corner of pinned image
        sStripT = sImgT - hudGap - totalHudH;
    } else {
        // 3. No space below or above -> place inside bottom-right of pinned image (clamped to screen bounds)
        sStripR = std::max((int)rcScreen.left + stripW + 6, std::min((int)rcScreen.right - 6, sImgR - 8));
        sStripT = std::max((int)rcScreen.top + 6, std::min((int)rcScreen.bottom - totalHudH - 6, sImgB - totalHudH - 8));
    }
    int sStripL = sStripR - stripW;
    int sStripB = sStripT + totalHudH;

    bool suppressToolbar = data->hideToolbar || (data->dragging && data->dragMoved);
    int winL = suppressToolbar ? sImgL : std::min(sImgL, sStripL);
    int winT = suppressToolbar ? sImgT : std::min(sImgT, sStripT);
    int winR = suppressToolbar ? sImgR : std::max(sImgR, sStripR);
    int winB = suppressToolbar ? sImgB : std::max(sImgB, sStripB);
    int winW = std::max(1, winR - winL);
    int winH = std::max(1, winB - winT);

    data->winX = winL;
    data->winY = winT;
    data->winW = winW;
    data->winH = winH;
    data->imgRect = { sImgL - winL, sImgT - winT, sImgR - winL, sImgB - winT };

    RECT curStripRect = { 0, 0, 0, 0 };
    data->buttons.clear();
    if (!suppressToolbar) {
        int bx = sStripL - winL;
        int by = sStripT - winT;
        curStripRect = { bx, by, bx + stripW, by + totalHudH };
        for (int i = 0; i < 6; ++i) {
            DockButton b;
            b.id = specs[i].id;
            b.rect = { bx, by, bx + specs[i].w, by + actBtnH };
            b.label = specs[i].lbl;
            b.tooltip = specs[i].tip;
            b.isPrimaryAction = specs[i].primary;
            data->buttons.push_back(b);
            bx += specs[i].w + gap;
        }
        int rx = sStripL - winL;
        int ry = by + actBtnH + gap;
        int sizeBoxW = 158;
        data->sizeRowRect = { rx, ry, rx + stripW, ry + actBtnH };
        data->sizeBoxRect = { rx, ry, rx + sizeBoxW, ry + actBtnH };
        data->pctBoxRect  = { rx + sizeBoxW, ry, rx + stripW, ry + actBtnH };
    } else {
        data->sizeRowRect = { 0, 0, 0, 0 };
        data->sizeBoxRect = { 0, 0, 0, 0 };
        data->pctBoxRect  = { 0, 0, 0, 0 };
        data->hoveredBtnId = -1;
        data->pressedBtnId = -1;
        data->hoveredSizeBox = false;
        data->hoveredPctBox = false;
        HidePinnedBubbleTooltip();
    }

    if (hWnd) {
        bool rgnChanged = !data->hasAppliedRgn ||
                          data->lastRgnHideToolbar != suppressToolbar ||
                          !EqualRect(&data->lastRgnImgRect, &data->imgRect) ||
                          !EqualRect(&data->lastRgnStripRect, &curStripRect);
        if (rgnChanged) {
            data->lastRgnImgRect = data->imgRect;
            data->lastRgnStripRect = curStripRect;
            data->lastRgnHideToolbar = suppressToolbar;
            data->hasAppliedRgn = true;
            RenderPinnedLayeredWindow(hWnd, data);
        } else if (resizeWindow) {
            SetWindowPos(hWnd, nullptr, winL, winT, winW, winH, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);
        } else {
            RenderPinnedLayeredWindow(hWnd, data);
        }
    }
}

static void SetPinnedWindowExactScale(HWND hWnd, PinnedWindowData* data, float targetScale) {
    if (!data || !data->bmp) return;
    int oldImgW = 32, oldImgH = 32;
    GetPinnedScaledDims(data, oldImgW, oldImgH);

    float minScale = 0.01f, maxScale = 64.0f;
    GetPinnedScaleBounds(data, minScale, maxScale);
    float nextScale = std::max(minScale, std::min(maxScale, targetScale));
    if (std::abs(nextScale - data->scale) < 1e-6f) {
        if (hWnd) RenderPinnedLayeredWindow(hWnd, data);
        return;
    }
    data->scale = nextScale;

    int imgW = 32, imgH = 32;
    GetPinnedScaledDims(data, imgW, imgH);

    int oldImgLeft   = data->screenImgX;
    int oldImgTop    = data->screenImgY;
    int oldImgRight  = oldImgLeft + oldImgW;
    int oldImgBottom = oldImgTop + oldImgH;

    RECT rcImgScreen = { oldImgLeft, oldImgTop, oldImgRight, oldImgBottom };
    HMONITOR hMon = MonitorFromRect(&rcImgScreen, MONITOR_DEFAULTTONEAREST);
    RECT rcScreen = GetPinnedMonitorWorkArea(hMon);

    int anchorX = oldImgRight;
    if (anchorX > rcScreen.right) anchorX = rcScreen.right - 8;
    if (anchorX < rcScreen.left + 32) anchorX = rcScreen.left + 32;

    int anchorY = oldImgBottom;
    if (anchorY > rcScreen.bottom) anchorY = rcScreen.bottom - 8;
    if (anchorY < rcScreen.top + 32) anchorY = rcScreen.top + 32;

    double fx = (oldImgW > 0) ? std::max(0.0, std::min(1.0, (double)(anchorX - oldImgLeft) / (double)oldImgW)) : 1.0;
    double fy = (oldImgH > 0) ? std::max(0.0, std::min(1.0, (double)(anchorY - oldImgTop) / (double)oldImgH)) : 1.0;

    data->screenImgX = (int)std::round(anchorX - fx * imgW);
    data->screenImgY = (int)std::round(anchorY - fy * imgH);

    bool fastInteractive = ((long long)imgW * (long long)imgH > 960LL * 540LL);
    EnsurePinnedWindowCache(data, imgW, imgH, !fastInteractive);

    UpdatePinnedWindowLayout(hWnd, data, true);
    if (hWnd) {
        if (fastInteractive) {
            SetTimer(hWnd, 1, 110, nullptr);
        } else {
            KillTimer(hWnd, 1);
        }
        POINT cur;
        if (GetCursorPos(&cur) && ScreenToClient(hWnd, &cur)) {
            int newHover = -1;
            for (const auto& b : data->buttons) {
                if (PtInRect(&b.rect, cur)) {
                    newHover = b.id;
                    break;
                }
            }
            data->hoveredBtnId = newHover;
            data->hoveredSizeBox = (newHover == -1 && PtInRect(&data->sizeBoxRect, cur) != FALSE);
            data->hoveredPctBox  = (newHover == -1 && !data->hoveredSizeBox && PtInRect(&data->pctBoxRect, cur) != FALSE);
            SetCursor(LoadCursorW(nullptr, (data->hoveredSizeBox || data->hoveredPctBox) ? IDC_IBEAM : ((newHover != -1) ? IDC_HAND : IDC_SIZEALL)));
            if (newHover != -1) {
                for (const auto& b : data->buttons) {
                    if (b.id == newHover) {
                        ShowPinnedBubbleTooltip(hWnd, b);
                        break;
                    }
                }
            } else {
                HidePinnedBubbleTooltip();
            }
        }
        RenderPinnedLayeredWindow(hWnd, data);
    }
}

static void ApplyPinnedWindowZoom(HWND hWnd, PinnedWindowData* data, float factor, bool resetToOriginal = false) {
    if (!data || !data->bmp) return;
    float targetScale = resetToOriginal ? 1.0f : (data->scale * factor);
    SetPinnedWindowExactScale(hWnd, data, targetScale);
}

static void CancelPinnedInlineEdits(PinnedWindowData* data) {
    if (!data) return;
    data->isEditingSize = false;
    data->isDraggingSizeText = false;
    data->editingSizeText.clear();
    data->sizeCaretPos = data->sizeSelAnchor = 0;
    data->isEditingPct = false;
    data->isDraggingPctText = false;
    data->editingPctText.clear();
    data->pctCaretPos = data->pctSelAnchor = 0;
}

static void CommitPinnedSizeInput(HWND hWnd, PinnedWindowData* data, bool applyToAll = false) {
    if (!data || !data->isEditingSize) return;
    std::wstring raw = data->editingSizeText;
    data->isEditingSize = false;
    data->isDraggingSizeText = false;
    data->editingSizeText.clear();
    data->sizeCaretPos = data->sizeSelAnchor = 0;

    auto isSep = [](wchar_t c) {
        return c == L'x' || c == L'X' || c == L'*' || c == L',' || c == L'\x00D7';
    };

    size_t firstNonSpace = 0;
    while (firstNonSpace < raw.size() && raw[firstNonSpace] == L' ') firstNonSpace++;
    bool startsWithSep = (firstNonSpace < raw.size() && isSep(raw[firstNonSpace]));

    for (wchar_t& c : raw) {
        if (isSep(c)) c = L' ';
    }
    std::wstringstream ss(raw);
    int n1 = 0, n2 = 0;
    if (ss >> n1 && n1 > 0) {
        bool hasSecond = (bool)(ss >> n2) && (n2 > 0);
        int curW = 32, curH = 32;
        GetPinnedScaledDims(data, curW, curH);

        auto computeScaleForPin = [&](const PinnedWindowData* d) -> float {
            int ow = std::max(1, d->origW);
            int oh = std::max(1, d->origH);
            if (!hasSecond) {
                if (startsWithSep) {
                    // e.g. "xx200" or "x200" -> resize by height = n1
                    return (float)n1 / (float)oh;
                }
                // e.g. "200" or "200x" -> resize by width = n1
                return (float)n1 / (float)ow;
            }
            int w = n1, h = n2;
            if (w != curW && h == curH) {
                return (float)w / (float)ow;
            }
            if (h != curH && w == curW) {
                return (float)h / (float)oh;
            }
            // Fit inside W x H so the image keeps its exact aspect ratio and never distorts
            return std::min((float)w / (float)ow, (float)h / (float)oh);
        };

        if (applyToAll && g_Daemon.pinnedWindows.size() > 1) {
            std::vector<HWND> pins = g_Daemon.pinnedWindows;
            for (HWND hp : pins) {
                if (hp && IsWindow(hp)) {
                    PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hp, GWLP_USERDATA);
                    if (d && d->bmp) {
                        CancelPinnedInlineEdits(d);
                        SetPinnedWindowExactScale(hp, d, computeScaleForPin(d));
                    }
                }
            }
        } else {
            SetPinnedWindowExactScale(hWnd, data, computeScaleForPin(data));
        }
    } else if (hWnd) {
        RenderPinnedLayeredWindow(hWnd, data);
    }
}

static void CommitPinnedPctInput(HWND hWnd, PinnedWindowData* data, bool applyToAll = false) {
    if (!data || !data->isEditingPct) return;
    std::wstring raw = data->editingPctText;
    data->isEditingPct = false;
    data->isDraggingPctText = false;
    data->editingPctText.clear();
    data->pctCaretPos = data->pctSelAnchor = 0;

    std::wstring clean;
    for (wchar_t c : raw) {
        if ((c >= L'0' && c <= L'9') || c == L'.') clean.push_back(c);
    }
    if (!clean.empty()) {
        wchar_t* endPtr = nullptr;
        double val = std::wcstod(clean.c_str(), &endPtr);
        if (endPtr != clean.c_str() && val > 0.0) {
            float targetScale = (float)(val / 100.0);
            if (applyToAll && g_Daemon.pinnedWindows.size() > 1) {
                std::vector<HWND> pins = g_Daemon.pinnedWindows;
                for (HWND hp : pins) {
                    if (hp && IsWindow(hp)) {
                        PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hp, GWLP_USERDATA);
                        if (d && d->bmp) {
                            CancelPinnedInlineEdits(d);
                            SetPinnedWindowExactScale(hp, d, targetScale);
                        }
                    }
                }
            } else {
                SetPinnedWindowExactScale(hWnd, data, targetScale);
            }
            return;
        }
    }
    if (hWnd) {
        RenderPinnedLayeredWindow(hWnd, data);
    }
}

static void UpdatePinnedDragPosition(HWND hWnd, PinnedWindowData* data) {
    if (!hWnd || !data || !data->dragging) return;
    POINT cur;
    GetCursorPos(&cur);
    if (!data->dragMoved) {
        if (std::abs(cur.x - data->dragStartMouse.x) < 3 && std::abs(cur.y - data->dragStartMouse.y) < 3) {
            return;
        }
        data->dragMoved = true;
    }
    int targetImgX = data->dragStartWnd.x + (cur.x - data->dragStartMouse.x);
    int targetImgY = data->dragStartWnd.y + (cur.y - data->dragStartMouse.y);

    bool shiftHeld = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) ||
                     ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) ||
                     ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0);

    if (!shiftHeld) {
        int imgW = 32, imgH = 32;
        GetPinnedScaledDims(data, imgW, imgH);

        int imgLeft   = targetImgX;
        int imgRight  = targetImgX + imgW;
        int imgTop    = targetImgY;
        int imgBottom = targetImgY + imgH;

        HMONITOR hMon = MonitorFromPoint(cur, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        if (hMon && GetMonitorInfoW(hMon, &mi)) {
            const int snapDist = 14;
            int distLeft  = std::abs(imgLeft - mi.rcMonitor.left);
            int distRight = std::abs(imgRight - mi.rcMonitor.right);
            if (distLeft <= snapDist && distLeft <= distRight) {
                targetImgX = mi.rcMonitor.left;
            } else if (distRight <= snapDist) {
                targetImgX = mi.rcMonitor.right - imgW;
            }

            int distTop    = std::abs(imgTop - mi.rcMonitor.top);
            int distBottom = std::abs(imgBottom - mi.rcMonitor.bottom);
            if (distTop <= snapDist && distTop <= distBottom) {
                targetImgY = mi.rcMonitor.top;
            } else if (distBottom <= snapDist) {
                targetImgY = mi.rcMonitor.bottom - imgH;
            }
        }
    }

    data->screenImgX = targetImgX;
    data->screenImgY = targetImgY;
    UpdatePinnedWindowLayout(hWnd, data, true);
}

static bool IsPinShiftHeld() {
    return ((GetKeyState(VK_SHIFT) & 0x8000) != 0) ||
           ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) ||
           ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) ||
           ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0);
}

static bool IsPinCtrlHeld() {
    return ((GetKeyState(VK_CONTROL) & 0x8000) != 0) ||
           ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) ||
           ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0) ||
           ((GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0);
}

static void SetPinnedWindowPaused(HWND hPin, PinnedWindowData* d, bool paused) {
    if (!hPin || !IsWindow(hPin) || !d || d->animFrames.size() <= 1) return;
    d->animPaused = paused;
    if (paused) {
        KillTimer(hPin, 3);
        // Keep the exact current frame (d->curFrameIdx), never reset to first frame
        if (d->curFrameIdx < d->animFrames.size()) {
            d->bmp = d->animFrames[d->curFrameIdx];
        }
    } else {
        // Resume playing from the exact current frame (d->curFrameIdx)
        if (d->curFrameIdx < d->animFrames.size()) {
            d->bmp = d->animFrames[d->curFrameIdx];
        }
        int nextDelay = (d->curFrameIdx < d->animDelaysMs.size())
            ? std::max(20, d->animDelaysMs[d->curFrameIdx])
            : 80;
        SetTimer(hPin, 3, (UINT)nextDelay, nullptr);
    }
    int imgW = 32, imgH = 32;
    GetPinnedScaledDims(d, imgW, imgH);
    EnsurePinnedWindowCache(d, imgW, imgH, true);
    RenderPinnedLayeredWindow(hPin, d);
}

static void TogglePinnedWindowAnimation(HWND hWnd, PinnedWindowData* data, bool applyToAll) {
    if (!hWnd || !data) return;
    if (applyToAll) {
        bool targetPaused = true;
        if (data->animFrames.size() > 1) {
            targetPaused = !data->animPaused;
        } else {
            for (HWND hPin : g_Daemon.pinnedWindows) {
                if (hPin && IsWindow(hPin)) {
                    PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
                    if (d && d->animFrames.size() > 1) {
                        targetPaused = !d->animPaused;
                        break;
                    }
                }
            }
        }
        std::vector<HWND> pins = g_Daemon.pinnedWindows;
        for (HWND hPin : pins) {
            if (hPin && IsWindow(hPin)) {
                PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
                if (d && d->animFrames.size() > 1) {
                    SetPinnedWindowPaused(hPin, d, targetPaused);
                }
            }
        }
    } else if (data->animFrames.size() > 1) {
        SetPinnedWindowPaused(hWnd, data, !data->animPaused);
    }
}

static bool MatchesPinnedShortcutWithOptionalShift(const HotkeyBinding& hk, UINT vk, bool ctrl, bool shift, bool alt, bool* outApplyAll) {
    if (hk.vk == 0 || vk == 0) return false;
    bool reqCtrl  = (hk.modifiers & MOD_CONTROL) != 0;
    bool reqShift = (hk.modifiers & MOD_SHIFT)   != 0;
    bool reqAlt   = (hk.modifiers & MOD_ALT)     != 0;
    if (ctrl != reqCtrl || alt != reqAlt) return false;
    bool keyMatch = false;
    if (vk == hk.vk) {
        keyMatch = true;
    } else if ((hk.vk == VK_OEM_PLUS  && vk == VK_ADD)      || (hk.vk == VK_ADD      && vk == VK_OEM_PLUS)) {
        keyMatch = true;
    } else if ((hk.vk == VK_OEM_MINUS && vk == VK_SUBTRACT) || (hk.vk == VK_SUBTRACT && vk == VK_OEM_MINUS)) {
        keyMatch = true;
    } else if ((hk.vk == '0'          && vk == VK_NUMPAD0)  || (hk.vk == VK_NUMPAD0  && vk == '0')) {
        keyMatch = true;
    }
    if (!keyMatch) return false;
    if (shift == reqShift) {
        if (outApplyAll) *outApplyAll = reqShift;
        return true;
    }
    if (shift && !reqShift) {
        if (outApplyAll) *outApplyAll = true;
        return true;
    }
    return false;
}

static void ExecutePinnedWindowAction(HWND hWnd, PinnedWindowData* data, int btnId, bool applyToAll = false) {
    if (!hWnd || !data) return;
    switch (btnId) {
        case DBTN_ACT_ZOOM_OUT:
            SetFocus(hWnd);
            if (applyToAll && g_Daemon.pinnedWindows.size() > 1) {
                std::vector<HWND> pins = g_Daemon.pinnedWindows;
                for (HWND hPin : pins) {
                    if (hPin && IsWindow(hPin)) {
                        PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
                        if (d) ApplyPinnedWindowZoom(hPin, d, 1.0f / 1.15f, false);
                    }
                }
            } else {
                ApplyPinnedWindowZoom(hWnd, data, 1.0f / 1.15f, false);
            }
            return;
        case DBTN_ACT_ZOOM_IN:
            SetFocus(hWnd);
            if (applyToAll && g_Daemon.pinnedWindows.size() > 1) {
                std::vector<HWND> pins = g_Daemon.pinnedWindows;
                for (HWND hPin : pins) {
                    if (hPin && IsWindow(hPin)) {
                        PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
                        if (d) ApplyPinnedWindowZoom(hPin, d, 1.15f, false);
                    }
                }
            } else {
                ApplyPinnedWindowZoom(hWnd, data, 1.15f, false);
            }
            return;
        case DBTN_ACT_ZOOM_RESET:
            SetFocus(hWnd);
            if (applyToAll && g_Daemon.pinnedWindows.size() > 1) {
                std::vector<HWND> pins = g_Daemon.pinnedWindows;
                for (HWND hPin : pins) {
                    if (hPin && IsWindow(hPin)) {
                        PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
                        if (d) ApplyPinnedWindowZoom(hPin, d, 1.0f, true);
                    }
                }
            } else {
                ApplyPinnedWindowZoom(hWnd, data, 1.0f, true);
            }
            return;
        case DBTN_ACT_PIN_OUTLINE: {
            SetFocus(hWnd);
            bool nextHideOutline = !data->hideOutline;
            g_Daemon.hidePinnedOutline = nextHideOutline;
            g_Daemon.SaveSettings();
            if (applyToAll) {
                std::vector<HWND> pins = g_Daemon.pinnedWindows;
                for (HWND hPin : pins) {
                    if (hPin && IsWindow(hPin)) {
                        PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
                        if (d && d->bmp) {
                            d->hideOutline = nextHideOutline;
                            int imgW = 32, imgH = 32;
                            GetPinnedScaledDims(d, imgW, imgH);
                            EnsurePinnedWindowCache(d, imgW, imgH, true);
                            UpdatePinnedWindowLayout(hPin, d, false);
                            RenderPinnedLayeredWindow(hPin, d);
                        }
                    }
                }
            } else {
                data->hideOutline = nextHideOutline;
                int imgW = 32, imgH = 32;
                GetPinnedScaledDims(data, imgW, imgH);
                EnsurePinnedWindowCache(data, imgW, imgH, true);
                UpdatePinnedWindowLayout(hWnd, data, false);
                RenderPinnedLayeredWindow(hWnd, data);
            }
            if (data->hoveredBtnId == DBTN_ACT_PIN_OUTLINE) {
                for (const auto& btn : data->buttons) {
                    if (btn.id == DBTN_ACT_PIN_OUTLINE) {
                        ShowPinnedBubbleTooltip(hWnd, btn);
                        break;
                    }
                }
            }
            return;
        }
        case DBTN_ACT_PIN_HIDE_TOOLBAR: {
            SetFocus(hWnd);
            HidePinnedBubbleTooltip();
            bool nextHideToolbar = !data->hideToolbar;
            g_Daemon.hidePinnedToolbar = nextHideToolbar;
            g_Daemon.SaveSettings();
            if (applyToAll) {
                std::vector<HWND> pins = g_Daemon.pinnedWindows;
                for (HWND hPin : pins) {
                    if (hPin && IsWindow(hPin)) {
                        PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
                        if (d && d->bmp) {
                            d->hideToolbar = nextHideToolbar;
                            UpdatePinnedWindowLayout(hPin, d, true);
                            RenderPinnedLayeredWindow(hPin, d);
                        }
                    }
                }
            } else {
                data->hideToolbar = nextHideToolbar;
                UpdatePinnedWindowLayout(hWnd, data, true);
                RenderPinnedLayeredWindow(hWnd, data);
            }
            return;
        }
        case DBTN_ACT_PIN_UNFILTER: {
            SetFocus(hWnd);
            bool nextSmooth = !data->smoothImage;
            g_Daemon.smoothPinnedImage = nextSmooth;
            g_Daemon.SaveSettings();
            if (applyToAll) {
                std::vector<HWND> pins = g_Daemon.pinnedWindows;
                for (HWND hPin : pins) {
                    if (hPin && IsWindow(hPin)) {
                        PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
                        if (d && d->bmp) {
                            d->smoothImage = nextSmooth;
                            int imgW = 32, imgH = 32;
                            GetPinnedScaledDims(d, imgW, imgH);
                            EnsurePinnedWindowCache(d, imgW, imgH, true);
                            RenderPinnedLayeredWindow(hPin, d);
                        }
                    }
                }
            } else {
                data->smoothImage = nextSmooth;
                int imgW = 32, imgH = 32;
                GetPinnedScaledDims(data, imgW, imgH);
                EnsurePinnedWindowCache(data, imgW, imgH, true);
                RenderPinnedLayeredWindow(hWnd, data);
            }
            return;
        }
        case DBTN_ACT_CLOSE:
            HidePinnedBubbleTooltip();
            if (applyToAll) {
                std::vector<HWND> pinsToClose = g_Daemon.pinnedWindows;
                g_Daemon.pinnedWindows.clear();
                for (HWND hp : pinsToClose) {
                    if (hp && IsWindow(hp)) {
                        DestroyWindow(hp);
                    }
                }
            } else {
                DestroyWindow(hWnd);
            }
            return;
        case DBTN_ACT_COPY:
            SetFocus(hWnd);
            if (applyToAll && g_Daemon.pinnedWindows.size() > 1) {
                std::vector<Bitmap*> allBmps;
                for (HWND hPin : g_Daemon.pinnedWindows) {
                    if (hPin && IsWindow(hPin)) {
                        PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
                        if (d && d->bmp) {
                            allBmps.push_back(d->bmp);
                        }
                    }
                }
                if (!allBmps.empty()) {
                    g_Daemon.CopyMultipleBitmapsToClipboard(allBmps);
                    g_Daemon.ShowTrayToast(
                        L"Copied " + std::to_wstring(allBmps.size()) + L" pinned image to clipboard",
                        IsWindowsClipboardHistoryEnabled()
                            ? L"Press Windows + V to select them."
                            : L"Enable clipboard history to see each of them"
                    );
                }
            } else if (data->bmp) {
                g_Daemon.CopyBitmapToClipboard(data->bmp);
                g_Daemon.ShowTrayToast(
                    L"Copied to clipboard (" + std::to_wstring(data->origW) + L"×" + std::to_wstring(data->origH) + L" px)",
                    L"Ready to paste (Ctrl+V)."
                );
            }
            return;
        case DBTN_ACT_SAVE:
            SetFocus(hWnd);
            if (applyToAll && g_Daemon.pinnedWindows.size() > 1) {
                CreateDirectoryW(g_Daemon.saveFolder.c_str(), nullptr);
                int savedCount = 0;
                std::wstring lastSaved;
                int totalPins = (int)g_Daemon.pinnedWindows.size();
                int idx = 1;
                std::wstring ext = PepperSnapDaemon::GetFormatExtension(g_Daemon.regionFormat);
                for (HWND hPin : g_Daemon.pinnedWindows) {
                    if (hPin && IsWindow(hPin)) {
                        PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
                        if (d && d->bmp) {
                            std::wstring stem = g_Daemon.FormatFilename(g_Daemon.captureCounter++);
                            std::wstring full = BuildUniqueSaveFilePath(g_Daemon.saveFolder, stem, ext, idx++, totalPins);
                            if (g_Daemon.SaveBitmapToPath(d->bmp, full)) {
                                lastSaved = full;
                                savedCount++;
                            }
                        }
                    }
                }
                g_Daemon.SaveSettings();
                if (savedCount > 0) {
                    g_Daemon.lastSavedFilePath = lastSaved;
                    g_Daemon.ShowTrayToast(
                        L"Saved " + std::to_wstring(savedCount) + L" pinned images",
                        L"Saved to: " + g_Daemon.saveFolder + L"\n(click to reveal in Explorer)"
                    );
                }
            } else if (data->bmp) {
                CreateDirectoryW(g_Daemon.saveFolder.c_str(), nullptr);
                std::wstring stem = g_Daemon.FormatFilename(g_Daemon.captureCounter++);
                std::wstring ext = PepperSnapDaemon::GetFormatExtension(g_Daemon.regionFormat);
                g_Daemon.SaveSettings();
                std::wstring full = BuildUniqueSaveFilePath(g_Daemon.saveFolder, stem, ext, 0, 1);
                if (g_Daemon.SaveBitmapToPath(data->bmp, full)) {
                    g_Daemon.lastSavedFilePath = full;
                    g_Daemon.ShowTrayToast(
                        L"Capture saved",
                        L"Saved to: " + full + L"\n(click to reveal in Explorer)"
                    );
                }
            }
            return;
        case DBTN_ACT_SAVE_AS:
            if (applyToAll && g_Daemon.pinnedWindows.size() > 1) {
                WCHAR szFile[MAX_PATH] = {0};
                std::wstring defName = g_Daemon.FormatFilename(g_Daemon.captureCounter++) +
                                       PepperSnapDaemon::GetFormatExtension(g_Daemon.regionFormat);
                g_Daemon.SaveSettings();
                wcsncpy_s(szFile, defName.c_str(), _TRUNCATE);

                OPENFILENAMEW ofn = {0};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hWnd;
                ofn.lpstrTitle = L"Save All Pinned Images As";
                ofn.lpstrInitialDir = g_Daemon.saveFolder.c_str();
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"JPEG image (*.jpg)\0*.jpg\0PNG image (*.png)\0*.png\0WebP image (*.webp)\0*.webp\0BMP bitmap (*.bmp)\0*.bmp\0";
                ofn.nFilterIndex = (DWORD)g_Daemon.regionFormat + 1;
                const wchar_t* defExts[4] = { L"jpg", L"png", L"webp", L"bmp" };
                ofn.lpstrDefExt = defExts[(int)g_Daemon.regionFormat & 3];
                ofn.Flags = OFN_PATHMUSTEXIST;

                if (GetSaveFileNameW(&ofn)) {
                    std::wstring chosenPath(szFile);
                    std::wstring chosenFolder = g_Daemon.saveFolder;
                    std::wstring filePart = chosenPath;
                    size_t slashPos = chosenPath.find_last_of(L"\\/");
                    if (slashPos != std::wstring::npos) {
                        chosenFolder = chosenPath.substr(0, slashPos);
                        filePart = chosenPath.substr(slashPos + 1);
                    }
                    std::wstring chosenStem = filePart;
                    std::wstring chosenExt;
                    size_t dotPos = filePart.find_last_of(L'.');
                    if (dotPos != std::wstring::npos && dotPos > 0) {
                        chosenStem = filePart.substr(0, dotPos);
                        chosenExt = filePart.substr(dotPos);
                    } else {
                        const wchar_t* filterExts[4] = { L".jpg", L".png", L".webp", L".bmp" };
                        int fIdx = (ofn.nFilterIndex >= 1 && ofn.nFilterIndex <= 4)
                            ? ((int)ofn.nFilterIndex - 1)
                            : ((int)g_Daemon.regionFormat & 3);
                        chosenExt = filterExts[fIdx];
                    }

                    std::vector<HWND> pins = g_Daemon.pinnedWindows;
                    int totalPins = 0;
                    for (HWND hPin : pins) {
                        if (hPin && IsWindow(hPin)) {
                            PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
                            if (d && d->bmp) totalPins++;
                        }
                    }

                    int savedCount = 0;
                    int idx = 1;
                    std::wstring lastSaved;
                    for (HWND hPin : pins) {
                        if (hPin && IsWindow(hPin)) {
                            PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
                            if (d && d->bmp) {
                                std::wstring full = BuildUniqueSaveFilePath(chosenFolder, chosenStem, chosenExt, idx++, totalPins);
                                if (g_Daemon.SaveBitmapToPath(d->bmp, full)) {
                                    lastSaved = full;
                                    savedCount++;
                                }
                            }
                        }
                    }
                    if (savedCount > 0) {
                        g_Daemon.lastSavedFilePath = lastSaved;
                        g_Daemon.ShowTrayToast(
                            L"Saved " + std::to_wstring(savedCount) + L" pinned images",
                            L"Saved to: " + chosenFolder + L"\n(click to reveal in Explorer)"
                        );
                    }
                }
                if (IsWindow(hWnd)) SetFocus(hWnd);
            } else if (data->bmp) {
                WCHAR szFile[MAX_PATH] = {0};
                std::wstring defName = g_Daemon.FormatFilename(g_Daemon.captureCounter++) +
                                       PepperSnapDaemon::GetFormatExtension(g_Daemon.regionFormat);
                g_Daemon.SaveSettings();
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
                if (IsWindow(hWnd)) SetFocus(hWnd);
            }
            return;
    }
}

static LRESULT CALLBACK PinWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PinnedWindowData* data = (PinnedWindowData*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_KILLFOCUS:
            if (data && (data->isEditingSize || data->isEditingPct)) {
                if (data->isEditingSize) CommitPinnedSizeInput(hWnd, data, false);
                if (data->isEditingPct)  CommitPinnedPctInput(hWnd, data, false);
            }
            break;
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT && data) {
                if (data->hoveredSizeBox || data->hoveredPctBox || data->isDraggingSizeText || data->isDraggingPctText) {
                    SetCursor(LoadCursorW(nullptr, IDC_IBEAM));
                } else {
                    SetCursor(LoadCursorW(nullptr, (data->hoveredBtnId != -1) ? IDC_HAND : IDC_SIZEALL));
                }
                return TRUE;
            }
            break;
        case WM_LBUTTONDOWN:
            if (data) {
                KillTimer(hWnd, 2);
                data->showingImgHoverTip = false;
                POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                bool shiftHeld = IsPinShiftHeld();

                if (!data->hideToolbar && PtInRect(&data->sizeBoxRect, pt)) {
                    HidePinnedBubbleTooltip();
                    if (data->isEditingPct) CommitPinnedPctInput(hWnd, data, false);
                    if (!data->isEditingSize) {
                        int curW = 32, curH = 32;
                        GetPinnedScaledDims(data, curW, curH);
                        data->isEditingSize = true;
                        data->editingSizeText = std::to_wstring(curW) + L"x" + std::to_wstring(curH);
                    }
                    size_t idx = PepperSnapDaemon::HitTestMonoIndex(pt.x, data->sizeBoxRect, data->editingSizeText, L" px");
                    data->sizeCaretPos = idx;
                    if (!shiftHeld) data->sizeSelAnchor = idx;
                    data->isDraggingSizeText = true;
                    SetCapture(hWnd);
                    SetFocus(hWnd);
                    SetCursor(LoadCursorW(nullptr, IDC_IBEAM));
                    RenderPinnedLayeredWindow(hWnd, data);
                    return 0;
                }
                if (data->isEditingSize) {
                    CommitPinnedSizeInput(hWnd, data, false);
                }

                if (!data->hideToolbar && PtInRect(&data->pctBoxRect, pt)) {
                    HidePinnedBubbleTooltip();
                    if (data->isEditingSize) CommitPinnedSizeInput(hWnd, data, false);
                    if (!data->isEditingPct) {
                        int pctVal = std::max(1, (int)std::round(data->scale * 100.0f));
                        data->isEditingPct = true;
                        data->editingPctText = std::to_wstring(pctVal);
                    }
                    size_t idx = PepperSnapDaemon::HitTestMonoIndex(pt.x, data->pctBoxRect, data->editingPctText, L"%");
                    data->pctCaretPos = idx;
                    if (!shiftHeld) data->pctSelAnchor = idx;
                    data->isDraggingPctText = true;
                    SetCapture(hWnd);
                    SetFocus(hWnd);
                    SetCursor(LoadCursorW(nullptr, IDC_IBEAM));
                    RenderPinnedLayeredWindow(hWnd, data);
                    return 0;
                }
                if (data->isEditingPct) {
                    CommitPinnedPctInput(hWnd, data, false);
                }

                for (const auto& b : data->buttons) {
                    if (PtInRect(&b.rect, pt)) {
                        data->pressedBtnId = b.id;
                        data->pressedBtnShift = shiftHeld;
                        data->hoveredBtnId = b.id;
                        SetCapture(hWnd);
                        SetFocus(hWnd);
                        RenderPinnedLayeredWindow(hWnd, data);
                        return 0;
                    }
                }
                data->pressedBtnId = -1;
                data->pressedBtnShift = false;
                HidePinnedBubbleTooltip();
                data->dragging = true;
                data->dragMoved = false;
                GetCursorPos(&data->dragStartMouse);
                data->dragStartWnd = { data->screenImgX, data->screenImgY };
                SetCapture(hWnd);
                SetFocus(hWnd);
            }
            return 0;
        case WM_LBUTTONDBLCLK:
            if (data) {
                KillTimer(hWnd, 2);
                data->showingImgHoverTip = false;
                POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

                if (!data->hideToolbar && PtInRect(&data->sizeBoxRect, pt)) {
                    HidePinnedBubbleTooltip();
                    if (data->isEditingPct) CommitPinnedPctInput(hWnd, data, false);
                    if (!data->isEditingSize) {
                        int curW = 32, curH = 32;
                        GetPinnedScaledDims(data, curW, curH);
                        data->isEditingSize = true;
                        data->editingSizeText = std::to_wstring(curW) + L"x" + std::to_wstring(curH);
                    }
                    size_t idx = PepperSnapDaemon::HitTestMonoIndex(pt.x, data->sizeBoxRect, data->editingSizeText, L" px");
                    const std::wstring& s = data->editingSizeText;
                    if (idx < s.size() && (s[idx] >= L'0' && s[idx] <= L'9')) {
                        size_t l = idx, r = idx;
                        while (l > 0 && s[l - 1] >= L'0' && s[l - 1] <= L'9') l--;
                        while (r < s.size() && s[r] >= L'0' && s[r] <= L'9') r++;
                        data->sizeSelAnchor = l;
                        data->sizeCaretPos = r;
                    } else {
                        data->sizeSelAnchor = 0;
                        data->sizeCaretPos = s.size();
                    }
                    data->isDraggingSizeText = false;
                    SetFocus(hWnd);
                    RenderPinnedLayeredWindow(hWnd, data);
                    return 0;
                }

                if (!data->hideToolbar && PtInRect(&data->pctBoxRect, pt)) {
                    HidePinnedBubbleTooltip();
                    if (data->isEditingSize) CommitPinnedSizeInput(hWnd, data, false);
                    if (!data->isEditingPct) {
                        int pctVal = std::max(1, (int)std::round(data->scale * 100.0f));
                        data->isEditingPct = true;
                        data->editingPctText = std::to_wstring(pctVal);
                    }
                    data->pctSelAnchor = 0;
                    data->pctCaretPos = data->editingPctText.size();
                    data->isDraggingPctText = false;
                    SetFocus(hWnd);
                    RenderPinnedLayeredWindow(hWnd, data);
                    return 0;
                }

                for (const auto& b : data->buttons) {
                    if (PtInRect(&b.rect, pt)) {
                        data->pressedBtnId = b.id;
                        data->pressedBtnShift = IsPinShiftHeld();
                        data->hoveredBtnId = b.id;
                        SetCapture(hWnd);
                        SetFocus(hWnd);
                        RenderPinnedLayeredWindow(hWnd, data);
                        return 0;
                    }
                }
                HidePinnedBubbleTooltip();
                if (IsPinCtrlHeld()) {
                    // Rapid Ctrl + click (or Ctrl + Shift + click) toggles pause/play on release instead of toggling toolbar
                    data->pendingLeftDblClickToggleToolbar = false;
                    data->dragging = true;
                    data->dragMoved = false;
                    GetCursorPos(&data->dragStartMouse);
                    data->dragStartWnd = { data->screenImgX, data->screenImgY };
                    SetCapture(hWnd);
                    SetFocus(hWnd);
                    return 0;
                }
                if (data->dragging) {
                    data->dragging = false;
                    data->dragMoved = false;
                }
                data->pendingLeftDblClickToggleToolbar = true;
                SetCapture(hWnd);
                SetFocus(hWnd);
            }
            return 0;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
            if (data) {
                KillTimer(hWnd, 2);
                if (data->showingImgHoverTip) {
                    data->showingImgHoverTip = false;
                    HidePinnedBubbleTooltip();
                }
                if (data->isEditingSize) CommitPinnedSizeInput(hWnd, data, false);
                if (data->isEditingPct)  CommitPinnedPctInput(hWnd, data, false);
                POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                bool onBtn = (PtInRect(&data->sizeRowRect, pt) != FALSE);
                for (const auto& b : data->buttons) {
                    if (PtInRect(&b.rect, pt)) {
                        onBtn = true;
                        break;
                    }
                }
                if (!onBtn) {
                    data->pendingMiddleClickReset = true;
                    SetCapture(hWnd);
                    SetFocus(hWnd);
                }
            }
            return 0;
        case WM_MBUTTONUP:
            if (data && data->pendingMiddleClickReset) {
                data->pendingMiddleClickReset = false;
                if (GetCapture() == hWnd && !data->dragging && data->pressedBtnId == -1 && !data->pendingLeftDblClickToggleToolbar && !data->pendingRightDblClickClose) {
                    ReleaseCapture();
                }
                POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                if (PtInRect(&data->imgRect, pt)) {
                    ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_ZOOM_RESET, IsPinShiftHeld());
                    if (IsWindow(hWnd) && PtInRect(&data->imgRect, pt)) {
                        SetTimer(hWnd, 2, 500, nullptr);
                    }
                }
            }
            return 0;
        case WM_RBUTTONDOWN:
            if (data) {
                KillTimer(hWnd, 2);
                if (data->showingImgHoverTip) {
                    data->showingImgHoverTip = false;
                    HidePinnedBubbleTooltip();
                }
                if (data->isEditingSize) CommitPinnedSizeInput(hWnd, data, false);
                if (data->isEditingPct)  CommitPinnedPctInput(hWnd, data, false);
                data->pendingRightDblClickClose = false;
            }
            return 0;
        case WM_RBUTTONDBLCLK:
            if (data) {
                data->pendingRightDblClickClose = true;
                SetCapture(hWnd);
            }
            return 0;
        case WM_RBUTTONUP:
            if (data && data->pendingRightDblClickClose) {
                data->pendingRightDblClickClose = false;
                if (GetCapture() == hWnd) {
                    ReleaseCapture();
                }
                POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                if (PtInRect(&data->imgRect, pt)) {
                    ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_CLOSE, IsPinShiftHeld());
                }
                return 0;
            }
            return 0;
        case WM_CHAR: {
            if (data && data->isEditingSize) {
                wchar_t ch = (wchar_t)wParam;
                if (ch == L'\r') {
                    CommitPinnedSizeInput(hWnd, data, IsPinShiftHeld());
                } else if ((ch >= L'0' && ch <= L'9') || ch == L'x' || ch == L'X' || ch == L'*' || ch == L',' || ch == L' ') {
                    PepperSnapDaemon::ApplyInlineEditChar(
                        ch == L'X' ? L'x' : ch,
                        data->editingSizeText, data->sizeCaretPos, data->sizeSelAnchor, 15
                    );
                    RenderPinnedLayeredWindow(hWnd, data);
                }
                return 0;
            }
            if (data && data->isEditingPct) {
                wchar_t ch = (wchar_t)wParam;
                if (ch == L'\r') {
                    CommitPinnedPctInput(hWnd, data, IsPinShiftHeld());
                } else if ((ch >= L'0' && ch <= L'9') || ch == L'.') {
                    PepperSnapDaemon::ApplyInlineEditChar(
                        ch, data->editingPctText, data->pctCaretPos, data->pctSelAnchor, 6
                    );
                    RenderPinnedLayeredWindow(hWnd, data);
                }
                return 0;
            }
            break;
        }
        case WM_KEYDOWN: {
            if (data && data->dragging && (wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT)) {
                UpdatePinnedDragPosition(hWnd, data);
                return 0;
            }
            if (data) {
                bool ctrl  = ((GetKeyState(VK_CONTROL) & 0x8000) != 0) || ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
                bool shift = IsPinShiftHeld();
                bool alt   = ((GetKeyState(VK_MENU)    & 0x8000) != 0) || ((GetAsyncKeyState(VK_MENU)    & 0x8000) != 0);

                if (data->isEditingSize) {
                    if (wParam == VK_ESCAPE) {
                        CancelPinnedInlineEdits(data);
                        RenderPinnedLayeredWindow(hWnd, data);
                        return 0;
                    }
                    if (wParam == VK_RETURN) {
                        CommitPinnedSizeInput(hWnd, data, shift);
                        return 0;
                    }
                    PepperSnapDaemon::ApplyInlineEditKeyDown(
                        wParam, ctrl, shift,
                        data->editingSizeText, data->sizeCaretPos, data->sizeSelAnchor,
                        15, true, hWnd
                    );
                    RenderPinnedLayeredWindow(hWnd, data);
                    return 0;
                }

                if (data->isEditingPct) {
                    if (wParam == VK_ESCAPE) {
                        CancelPinnedInlineEdits(data);
                        RenderPinnedLayeredWindow(hWnd, data);
                        return 0;
                    }
                    if (wParam == VK_RETURN) {
                        CommitPinnedPctInput(hWnd, data, shift);
                        return 0;
                    }
                    PepperSnapDaemon::ApplyInlineEditKeyDown(
                        wParam, ctrl, shift,
                        data->editingPctText, data->pctCaretPos, data->pctSelAnchor,
                        6, false, hWnd
                    );
                    RenderPinnedLayeredWindow(hWnd, data);
                    return 0;
                }

                UINT vk = (UINT)wParam;
                bool applyAll = false;
                if (MatchesPinnedShortcutWithOptionalShift(g_Daemon.hkPinClose, vk, ctrl, shift, alt, &applyAll)) {
                    ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_CLOSE, applyAll);
                    return 0;
                }
                if (MatchesPinnedShortcutWithOptionalShift(g_Daemon.hkPinZoomOut, vk, ctrl, shift, alt, &applyAll)) {
                    ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_ZOOM_OUT, applyAll);
                    return 0;
                }
                if (MatchesPinnedShortcutWithOptionalShift(g_Daemon.hkPinZoomIn, vk, ctrl, shift, alt, &applyAll)) {
                    ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_ZOOM_IN, applyAll);
                    return 0;
                }
                if (MatchesPinnedShortcutWithOptionalShift(g_Daemon.hkPinZoomReset, vk, ctrl, shift, alt, &applyAll)) {
                    ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_ZOOM_RESET, applyAll);
                    return 0;
                }
                if (MatchesPinnedShortcutWithOptionalShift(g_Daemon.hkPinOutline, vk, ctrl, shift, alt, &applyAll)) {
                    ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_PIN_OUTLINE, applyAll);
                    return 0;
                }
                if (MatchesPinnedShortcutWithOptionalShift(g_Daemon.hkPinHideToolbar, vk, ctrl, shift, alt, &applyAll)) {
                    ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_PIN_HIDE_TOOLBAR, applyAll);
                    return 0;
                }
                if (MatchesPinnedShortcutWithOptionalShift(g_Daemon.hkPinSmooth, vk, ctrl, shift, alt, &applyAll)) {
                    ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_PIN_UNFILTER, applyAll);
                    return 0;
                }
                // Check Quick Save (including Shift + Quick Save when multiple pinned images are open,
                // or Save As when Save As is explicitly invoked on a single pinned image)
                if (MatchesPinnedShortcutWithOptionalShift(g_Daemon.hkPinSaveAs, vk, ctrl, shift, alt, &applyAll)) {
                    bool alsoMatchesShiftQuickSave = shift &&
                        ((g_Daemon.hkPinSave.modifiers & MOD_SHIFT) == 0) &&
                        MatchesPinnedShortcutWithOptionalShift(g_Daemon.hkPinSave, vk, ctrl, shift, alt, nullptr);
                    if (alsoMatchesShiftQuickSave && g_Daemon.pinnedWindows.size() > 1) {
                        ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_SAVE, true);
                    } else {
                        ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_SAVE_AS, applyAll);
                    }
                    return 0;
                }
                if (MatchesPinnedShortcutWithOptionalShift(g_Daemon.hkPinSave, vk, ctrl, shift, alt, &applyAll)) {
                    ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_SAVE, applyAll);
                    return 0;
                }
                if (MatchesPinnedShortcutWithOptionalShift(g_Daemon.hkPinCopy, vk, ctrl, shift, alt, &applyAll)) {
                    ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_COPY, applyAll);
                    return 0;
                }
            }
            break;
        }
        case WM_KEYUP:
            if (data && data->dragging && (wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT)) {
                UpdatePinnedDragPosition(hWnd, data);
                return 0;
            }
            break;
        case WM_TIMER:
            if (wParam == 1 && data) {
                KillTimer(hWnd, 1);
                int imgW = 32, imgH = 32;
                GetPinnedScaledDims(data, imgW, imgH);
                if (!data->cachedHq) {
                    EnsurePinnedWindowCache(data, imgW, imgH, true);
                    RenderPinnedLayeredWindow(hWnd, data);
                }
                return 0;
            }
            if (wParam == 2 && data) {
                KillTimer(hWnd, 2);
                if (!data->dragging && data->hoveredBtnId == -1 && data->pressedBtnId == -1 &&
                    !data->hoveredSizeBox && !data->hoveredPctBox) {
                    POINT ptScreen;
                    if (GetCursorPos(&ptScreen)) {
                        POINT ptClient = ptScreen;
                        if (ScreenToClient(hWnd, &ptClient) && PtInRect(&data->imgRect, ptClient)) {
                            data->showingImgHoverTip = true;
                            ShowPinnedImageHoverTooltip(hWnd, data, ptScreen);
                        }
                    }
                }
                return 0;
            }
            if (wParam == 3 && data && data->animFrames.size() > 1 && !data->animPaused) {
                data->curFrameIdx = (data->curFrameIdx + 1) % data->animFrames.size();
                data->bmp = data->animFrames[data->curFrameIdx];
                int nextDelay = (data->curFrameIdx < data->animDelaysMs.size())
                    ? std::max(20, data->animDelaysMs[data->curFrameIdx])
                    : 80;
                SetTimer(hWnd, 3, (UINT)nextDelay, nullptr);
                int imgW = 32, imgH = 32;
                GetPinnedScaledDims(data, imgW, imgH);
                bool fastInteractive = ((long long)imgW * (long long)imgH > 960LL * 540LL);
                EnsurePinnedWindowCache(data, imgW, imgH, !fastInteractive);
                RenderPinnedLayeredWindow(hWnd, data);
                return 0;
            }
            break;
        case WM_MOUSEMOVE:
            if (data) {
                POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                if (data->isDraggingSizeText && data->isEditingSize) {
                    data->sizeCaretPos = PepperSnapDaemon::HitTestMonoIndex(pt.x, data->sizeBoxRect, data->editingSizeText, L" px");
                    SetCursor(LoadCursorW(nullptr, IDC_IBEAM));
                    RenderPinnedLayeredWindow(hWnd, data);
                    return 0;
                }
                if (data->isDraggingPctText && data->isEditingPct) {
                    data->pctCaretPos = PepperSnapDaemon::HitTestMonoIndex(pt.x, data->pctBoxRect, data->editingPctText, L"%");
                    SetCursor(LoadCursorW(nullptr, IDC_IBEAM));
                    RenderPinnedLayeredWindow(hWnd, data);
                    return 0;
                }
                if (data->pressedBtnId != -1) {
                    KillTimer(hWnd, 2);
                    if (data->showingImgHoverTip) {
                        data->showingImgHoverTip = false;
                        HidePinnedBubbleTooltip();
                    }
                    int newHover = -1;
                    for (const auto& b : data->buttons) {
                        if (b.id == data->pressedBtnId && PtInRect(&b.rect, pt)) {
                            newHover = b.id;
                            break;
                        }
                    }
                    if (newHover != data->hoveredBtnId) {
                        data->hoveredBtnId = newHover;
                        SetCursor(LoadCursorW(nullptr, (newHover != -1) ? IDC_HAND : IDC_SIZEALL));
                        if (newHover != -1) {
                            for (const auto& b : data->buttons) {
                                if (b.id == newHover) {
                                    ShowPinnedBubbleTooltip(hWnd, b);
                                    break;
                                }
                            }
                        } else {
                            HidePinnedBubbleTooltip();
                        }
                        RenderPinnedLayeredWindow(hWnd, data);
                    }
                    return 0;
                }
                if (data->dragging) {
                    KillTimer(hWnd, 2);
                    if (data->showingImgHoverTip) {
                        data->showingImgHoverTip = false;
                        HidePinnedBubbleTooltip();
                    }
                    UpdatePinnedDragPosition(hWnd, data);
                    return 0;
                }
                if (!data->trackingMouseLeave) {
                    TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
                    TrackMouseEvent(&tme);
                    data->trackingMouseLeave = true;
                }
                int newHover = -1;
                for (const auto& b : data->buttons) {
                    if (PtInRect(&b.rect, pt)) {
                        newHover = b.id;
                        break;
                    }
                }
                bool newSizeHov = (newHover == -1 && !data->hideToolbar && PtInRect(&data->sizeBoxRect, pt) != FALSE);
                bool newPctHov  = (newHover == -1 && !newSizeHov && !data->hideToolbar && PtInRect(&data->pctBoxRect, pt) != FALSE);

                if (newHover != -1 || newSizeHov || newPctHov) {
                    KillTimer(hWnd, 2);
                    data->showingImgHoverTip = false;
                    data->lastHoverMouse = { -10000, -10000 };
                } else if (PtInRect(&data->imgRect, pt)) {
                    bool movedMouse = (pt.x != data->lastHoverMouse.x || pt.y != data->lastHoverMouse.y);
                    data->lastHoverMouse = pt;
                    if (!data->showingImgHoverTip && movedMouse) {
                        SetTimer(hWnd, 2, 500, nullptr);
                    }
                } else {
                    KillTimer(hWnd, 2);
                    data->lastHoverMouse = { -10000, -10000 };
                    if (data->showingImgHoverTip) {
                        data->showingImgHoverTip = false;
                        HidePinnedBubbleTooltip();
                    }
                }
                if (newHover != data->hoveredBtnId || newSizeHov != data->hoveredSizeBox || newPctHov != data->hoveredPctBox) {
                    data->hoveredBtnId = newHover;
                    data->hoveredSizeBox = newSizeHov;
                    data->hoveredPctBox = newPctHov;
                    SetCursor(LoadCursorW(nullptr, (newSizeHov || newPctHov) ? IDC_IBEAM : ((newHover != -1) ? IDC_HAND : IDC_SIZEALL)));
                    if (newHover != -1) {
                        for (const auto& b : data->buttons) {
                            if (b.id == newHover) {
                                ShowPinnedBubbleTooltip(hWnd, b);
                                break;
                            }
                        }
                    } else if (newSizeHov && !data->isEditingSize) {
                        DockButton fakeTip;
                        fakeTip.rect = data->sizeBoxRect;
                        fakeTip.tooltip = L"Image size \x2014 click to type WxH, W (or Wx), or xxH (px)";
                        ShowPinnedBubbleTooltip(hWnd, fakeTip);
                    } else if (newPctHov && !data->isEditingPct) {
                        DockButton fakeTip;
                        fakeTip.rect = data->pctBoxRect;
                        fakeTip.tooltip = L"Zoom scale \x2014 click to type percentage (%)";
                        ShowPinnedBubbleTooltip(hWnd, fakeTip);
                    } else if (!data->showingImgHoverTip) {
                        HidePinnedBubbleTooltip();
                    }
                    RenderPinnedLayeredWindow(hWnd, data);
                }
            }
            return 0;
        case WM_MOUSELEAVE:
            if (data) {
                data->trackingMouseLeave = false;
                KillTimer(hWnd, 2);
                data->showingImgHoverTip = false;
                data->lastHoverMouse = { -10000, -10000 };
                HidePinnedBubbleTooltip();
                if (data->hoveredBtnId != -1 || data->hoveredSizeBox || data->hoveredPctBox) {
                    data->hoveredBtnId = -1;
                    data->hoveredSizeBox = false;
                    data->hoveredPctBox = false;
                    RenderPinnedLayeredWindow(hWnd, data);
                }
            }
            return 0;
        case WM_LBUTTONUP:
            if (data) {
                if (data->isDraggingSizeText) {
                    data->isDraggingSizeText = false;
                    if (GetCapture() == hWnd) ReleaseCapture();
                    RenderPinnedLayeredWindow(hWnd, data);
                    return 0;
                }
                if (data->isDraggingPctText) {
                    data->isDraggingPctText = false;
                    if (GetCapture() == hWnd) ReleaseCapture();
                    RenderPinnedLayeredWindow(hWnd, data);
                    return 0;
                }
                if (data->pendingLeftDblClickToggleToolbar) {
                    data->pendingLeftDblClickToggleToolbar = false;
                    if (GetCapture() == hWnd) {
                        ReleaseCapture();
                    }
                    POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                    if (PtInRect(&data->imgRect, pt)) {
                        ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_PIN_HIDE_TOOLBAR, IsPinShiftHeld());
                        if (IsWindow(hWnd) && PtInRect(&data->imgRect, pt)) {
                            SetTimer(hWnd, 2, 500, nullptr);
                        }
                    }
                    return 0;
                }
                if (data->pressedBtnId != -1) {
                    int releasedBtnId = data->pressedBtnId;
                    bool releasedWithShift = data->pressedBtnShift || IsPinShiftHeld();
                    data->pressedBtnId = -1;
                    data->pressedBtnShift = false;
                    ReleaseCapture();
                    POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                    bool hit = false;
                    int newHover = -1;
                    for (const auto& b : data->buttons) {
                        if (PtInRect(&b.rect, pt)) {
                            newHover = b.id;
                            if (b.id == releasedBtnId) {
                                hit = true;
                            }
                            break;
                        }
                    }
                    data->hoveredBtnId = newHover;
                    RenderPinnedLayeredWindow(hWnd, data);
                    if (hit) {
                        ExecutePinnedWindowAction(hWnd, data, releasedBtnId, releasedWithShift);
                    }
                    return 0;
                }
                if (data->dragging) {
                    bool wasMoved = data->dragMoved;
                    data->dragging = false;
                    data->dragMoved = false;
                    ReleaseCapture();
                    POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                    if (wasMoved) {
                        UpdatePinnedWindowLayout(hWnd, data, true);
                        RenderPinnedLayeredWindow(hWnd, data);
                    } else if (PtInRect(&data->imgRect, pt) && IsPinCtrlHeld()) {
                        TogglePinnedWindowAnimation(hWnd, data, IsPinShiftHeld());
                    }
                    if (PtInRect(&data->imgRect, pt)) {
                        SetTimer(hWnd, 2, 500, nullptr);
                    }
                }
            }
            return 0;
        case WM_MOUSEWHEEL:
            if (data) {
                KillTimer(hWnd, 2);
                if (data->showingImgHoverTip) {
                    data->showingImgHoverTip = false;
                    HidePinnedBubbleTooltip();
                }
                if (data->isEditingSize) CommitPinnedSizeInput(hWnd, data, false);
                if (data->isEditingPct)  CommitPinnedPctInput(hWnd, data, false);
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (delta != 0) {
                    float steps = (float)delta / (float)WHEEL_DELTA;
                    float factor = std::pow(1.15f, steps);
                    if (IsPinShiftHeld() && g_Daemon.pinnedWindows.size() > 1) {
                        std::vector<HWND> pins = g_Daemon.pinnedWindows;
                        for (HWND hPin : pins) {
                            if (hPin && IsWindow(hPin)) {
                                PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
                                if (d) ApplyPinnedWindowZoom(hPin, d, factor);
                            }
                        }
                    } else {
                        ApplyPinnedWindowZoom(hWnd, data, factor);
                    }
                }
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            KillTimer(hWnd, 1);
            KillTimer(hWnd, 2);
            KillTimer(hWnd, 3);
            HidePinnedBubbleTooltip();
            g_Daemon.pinnedWindows.erase(
                std::remove(g_Daemon.pinnedWindows.begin(), g_Daemon.pinnedWindows.end(), hWnd),
                g_Daemon.pinnedWindows.end()
            );
            if (data) {
                FreePinnedWindowCache(data);
                if (!data->animFrames.empty()) {
                    for (Bitmap* f : data->animFrames) {
                        delete f;
                    }
                    data->animFrames.clear();
                    data->bmp = nullptr;
                } else {
                    delete data->bmp;
                    data->bmp = nullptr;
                }
                delete data;
            }
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

HWND PepperSnapDaemon::CreatePinnedWindow(Bitmap* bmp, int x, int y, float initialScale, std::vector<Bitmap*> animFrames, std::vector<int> animDelaysMs) {
    if (!bmp) return nullptr;
    int sw = (int)bmp->GetWidth();
    int sh = (int)bmp->GetHeight();

    PinnedWindowData* data = new PinnedWindowData();
    data->bmp = bmp;
    data->animFrames = std::move(animFrames);
    data->animDelaysMs = std::move(animDelaysMs);
    data->curFrameIdx = 0;
    data->origW = sw;
    data->origH = sh;
    data->scale = initialScale;
    data->screenImgX = x;
    data->screenImgY = y;
    data->hideToolbar = hidePinnedToolbar;
    data->hideOutline = hidePinnedOutline;
    data->smoothImage = smoothPinnedImage;

    int imgW = 32, imgH = 32;
    GetPinnedScaledDims(data, imgW, imgH);
    EnsurePinnedWindowCache(data, imgW, imgH, true);

    HWND hPin = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        L"PepperSnapPinWnd", L"PepperSnap Pin",
        WS_POPUP, x, y, imgW, imgH,
        nullptr, nullptr, hInst, nullptr
    );
    SetWindowLongPtrW(hPin, GWLP_USERDATA, (LONG_PTR)data);
    UpdatePinnedWindowLayout(hPin, data, true);
    ShowWindow(hPin, SW_SHOW);
    BringWindowToTop(hPin);
    SetForegroundWindow(hPin);
    SetFocus(hPin);
    RenderPinnedLayeredWindow(hPin, data);
    if (data->animFrames.size() > 1) {
        int firstDelay = (!data->animDelaysMs.empty() && data->animDelaysMs[0] >= 20) ? data->animDelaysMs[0] : 80;
        SetTimer(hPin, 3, (UINT)firstDelay, nullptr);
    }
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
    bool keepCursor = false;
    bool autoHideFrames = true;
    bool nonStackingHi = true;
    bool penSmoothEnabled = true;
    int penSmoothStrength = 15;
    bool autoUpdateEnabled = true;
    UpdateCheckInterval updateInterval = UpdateCheckInterval::EveryDay;
    HotkeyBinding hkRegion{ MOD_CONTROL, VK_SNAPSHOT };
    HotkeyBinding hkFull{ MOD_SHIFT, VK_SNAPSHOT };
    HotkeyBinding hkPrev{ MOD_CONTROL | MOD_SHIFT, VK_SNAPSHOT };
    HotkeyBinding hkToolSelect{ 0, 'V' };
    HotkeyBinding hkToolPen{ 0, 'P' };
    HotkeyBinding hkToolStabilo{ 0, 'H' };
    HotkeyBinding hkToolLine{ 0, 'L' };
    HotkeyBinding hkToolArrow{ 0, 'A' };
    HotkeyBinding hkToolNumber{ 0, 'N' };
    HotkeyBinding hkToolResetNum{ 0, 0 };
    HotkeyBinding hkToolRect{ 0, 'R' };
    HotkeyBinding hkToolEllipse{ 0, 'E' };
    HotkeyBinding hkToolText{ 0, 'T' };
    HotkeyBinding hkToolMosaicSq{ 0, 'X' };
    HotkeyBinding hkToolMosaicCir{ 0, 'M' };
    HotkeyBinding hkActUndo{ MOD_CONTROL, 'Z' };
    HotkeyBinding hkActRedo{ MOD_CONTROL, 'Y' };
    HotkeyBinding hkActClear{ 0, 0 };
    HotkeyBinding hkActOptions{ 0, 0 };
    HotkeyBinding hkActPin{ 0, 'F' };
    HotkeyBinding hkActOcr{ 0, 'O' };
    HotkeyBinding hkActSaveAs{ MOD_CONTROL | MOD_SHIFT, 'S' };
    HotkeyBinding hkActSave{ MOD_CONTROL, 'S' };
    HotkeyBinding hkActCopy{ MOD_CONTROL, 'C' };
    HotkeyBinding hkFramePlayPause{ 0, VK_SPACE };
    HotkeyBinding hkFrameToggleStrip{ 0, 0 };
    HotkeyBinding hkPinZoomOut{ 0, VK_OEM_MINUS };
    HotkeyBinding hkPinZoomIn{ 0, VK_OEM_PLUS };
    HotkeyBinding hkPinZoomReset{ 0, '0' };
    HotkeyBinding hkPinOutline{ 0, 'O' };
    HotkeyBinding hkPinHideToolbar{ 0, 'H' };
    HotkeyBinding hkPinSmooth{ 0, 'S' };
    HotkeyBinding hkPinSaveAs{ 0, 0 };
    HotkeyBinding hkPinSave{ 0, 0 };
    HotkeyBinding hkPinCopy{ MOD_CONTROL, 'C' };
    HotkeyBinding hkPinClose{ 0, VK_ESCAPE };
    bool confirmed = false;
    bool openedAppData = false;
    bool restoredAllDefaults = false;
    int scrollY = 0;
    int scrollContentHeight = 716;
    int footerHeight = 56;
    int fixedWinWidth = 580;
    int fullWinHeight = 772;
    int savedWinHeight = 772;

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
    HWND hKeepCursorChk = nullptr;
    HWND hAutoHideFramesChk = nullptr;
    HWND hNonStackingChk = nullptr;
    HWND hPenSmoothChk = nullptr;
    HWND hPenSmoothSlider = nullptr;
    HWND hPenSmoothValLbl = nullptr;
    HWND hAutoUpdateChk = nullptr;
    HWND hComboUpdateInterval = nullptr;
    HWND hCheckUpdateNowBtn = nullptr;
    HWND hCustomizeKeysBtn = nullptr;
    HWND hRestoreAllBtn = nullptr;
    HWND hFooterBg = nullptr;
    HWND hAppDataInfo = nullptr;
    HWND hOkBtn = nullptr;
    HWND hCancelBtn = nullptr;
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
#define IDC_OPT_CUSTOMIZE_KEYS   1019
#define IDC_OPT_SAVE_PIN_CHK     1023
#define IDC_OPT_SAVE_OCR_CHK     1024
#define IDC_OPT_RESTORE_ALL_DEF  1025
#define IDC_OPT_NAMING_SYNTAX    1026
#define IDC_OPT_KEEP_CURSOR_CHK  1027
#define IDC_OPT_AUTOHIDE_FRM_CHK 1028

#define IDC_CK_BTN_BASE          2100
#define IDC_CK_RESTORE_DEFAULTS  2201

struct NamingSyntaxRow {
    std::wstring syntax;
    std::wstring explanation;
    std::wstring example;
};

struct NamingSyntaxDlgState {
    int scrollY = 0;
    int scrollContentHeight = 620;
    int headerHeight = 36;
    int footerHeight = 50;
    int fixedWinWidth = 490;
    int fullWinHeight = 706;
    int savedWinHeight = 706;
    int hoveredRowIdx = -1;
    int pressedRowIdx = -1;
    std::wstring copiedSyntax;
    HWND hOkBtn = nullptr;
    HWND hListViewport = nullptr;
    std::vector<HWND> hSyntaxEdits;
    HFONT hCodeFont = nullptr;
};

static const NamingSyntaxRow kNamingSyntaxRows[] = {
    { L"{YYYY}", L"4 digit year",                  L"2040"       },
    { L"{YY}",   L"2 digit year",                  L"40"         },
    { L"{MMMM}", L"Full month name",               L"October"    },
    { L"{MMM}",  L"Short month name",              L"Oct"        },
    { L"{MM}",   L"2 digit month (01–12)",         L"10"         },
    { L"{M}",    L"Month (1–12)",                  L"10"         },
    { L"{DDDD}", L"Full day name",                 L"Wednesday"  },
    { L"{DDD}",  L"Short day name",                L"Wed"        },
    { L"{DD}",   L"2 digit day (01–31)",           L"25"         },
    { L"{D}",    L"Day (1–31)",                    L"25"         },
    { L"{HH}",   L"2 digit hour, 24-hour (00–23)", L"14"         },
    { L"{H}",    L"Hour, 24-hour (0–23)",          L"14"         },
    { L"{hh}",   L"2 digit hour, 12-hour (01–12)", L"02"         },
    { L"{h}",    L"Hour, 12-hour (1–12)",          L"2"          },
    { L"{AP}",   L"AM / PM uppercase",             L"PM"         },
    { L"{ap}",   L"am / pm lowercase",             L"pm"         },
    { L"{mm}",   L"2 digit minute (00–59)",        L"30"         },
    { L"{m}",    L"Minute (0–59)",                 L"30"         },
    { L"{ss}",   L"2 digit second (00–59)",        L"05"         },
    { L"{s}",    L"Second (0–59)",                 L"5"          },
    { L"{ms}",   L"3 digit millisecond (000–999)", L"128"        },
    { L"{UNIX}", L"Unix epoch timestamp (sec)",    L"1791412205" },
    { L"{NNN}",  L"3 digit capture counter",       L"001"        },
    { L"{N}",    L"Capture counter (no leading 0)",L"1"          }
};

static void CopySyntaxStringToClipboard(HWND hOwner, const std::wstring& text) {
    if (text.empty()) return;
    if (!OpenClipboard(hOwner)) return;
    EmptyClipboard();
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hMem) {
        void* ptr = GlobalLock(hMem);
        if (ptr) {
            memcpy(ptr, text.c_str(), bytes);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        } else {
            GlobalFree(hMem);
        }
    }
    CloseClipboard();
}

static LRESULT CALLBACK NamingSyntaxEditSubclassProc(HWND hEdit, UINT msg, WPARAM wParam, LPARAM lParam) {
    WNDPROC origProc = (WNDPROC)GetPropW(hEdit, L"PepperSnapOrigSyntaxEditProc");
    HWND hViewport = GetParent(hEdit);
    HWND hDlg = hViewport ? GetParent(hViewport) : nullptr;
    NamingSyntaxDlgState* st = hDlg ? (NamingSyntaxDlgState*)GetWindowLongPtrW(hDlg, GWLP_USERDATA) : nullptr;
    int rowIdx = (int)(INT_PTR)GetPropW(hEdit, L"PepperSnapSyntaxRowIdx") - 1;

    switch (msg) {
        case WM_MOUSEWHEEL:
            if (hDlg) {
                return SendMessageW(hDlg, WM_MOUSEWHEEL, wParam, lParam);
            }
            break;
        case WM_KEYDOWN:
            if ((wParam == VK_ESCAPE || wParam == VK_RETURN) && hDlg) {
                DestroyWindow(hDlg);
                return 0;
            }
            if (wParam == 'C' && (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
                LRESULT r = origProc ? CallWindowProcW(origProc, hEdit, msg, wParam, lParam) : DefWindowProcW(hEdit, msg, wParam, lParam);
                if (st && rowIdx >= 0 && rowIdx < (int)(sizeof(kNamingSyntaxRows) / sizeof(kNamingSyntaxRows[0]))) {
                    DWORD selS = 0, selE = 0;
                    SendMessageW(hEdit, EM_GETSEL, (WPARAM)&selS, (LPARAM)&selE);
                    if (selS == selE) {
                        CopySyntaxStringToClipboard(hDlg, kNamingSyntaxRows[rowIdx].syntax);
                    }
                    st->copiedSyntax = kNamingSyntaxRows[rowIdx].syntax;
                    if (hDlg) InvalidateRect(hDlg, nullptr, FALSE);
                }
                return r;
            }
            break;
        case WM_LBUTTONDOWN:
            if (st) {
                st->pressedRowIdx = rowIdx;
            }
            break;
        case WM_LBUTTONUP: {
            LRESULT res = origProc ? CallWindowProcW(origProc, hEdit, msg, wParam, lParam) : DefWindowProcW(hEdit, msg, wParam, lParam);
            if (st && st->pressedRowIdx == rowIdx && rowIdx >= 0 && rowIdx < (int)(sizeof(kNamingSyntaxRows) / sizeof(kNamingSyntaxRows[0]))) {
                st->pressedRowIdx = -1;
                POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                RECT rcE;
                GetClientRect(hEdit, &rcE);
                if (PtInRect(&rcE, pt)) {
                    DWORD selS = 0, selE = 0;
                    SendMessageW(hEdit, EM_GETSEL, (WPARAM)&selS, (LPARAM)&selE);
                    if (selS == selE) {
                        SendMessageW(hEdit, EM_SETSEL, 0, -1);
                    }
                    CopySyntaxStringToClipboard(hDlg, kNamingSyntaxRows[rowIdx].syntax);
                    st->copiedSyntax = kNamingSyntaxRows[rowIdx].syntax;
                    if (hDlg) InvalidateRect(hDlg, nullptr, FALSE);
                }
            } else if (st) {
                st->pressedRowIdx = -1;
            }
            return res;
        }
        case WM_NCDESTROY:
            RemovePropW(hEdit, L"PepperSnapOrigSyntaxEditProc");
            RemovePropW(hEdit, L"PepperSnapSyntaxRowIdx");
            break;
    }
    return origProc ? CallWindowProcW(origProc, hEdit, msg, wParam, lParam) : DefWindowProcW(hEdit, msg, wParam, lParam);
}

static LRESULT CALLBACK NamingSyntaxViewportWndProc(HWND hViewport, UINT msg, WPARAM wParam, LPARAM lParam) {
    HWND hDlg = GetParent(hViewport);
    NamingSyntaxDlgState* st = hDlg ? (NamingSyntaxDlgState*)GetWindowLongPtrW(hDlg, GWLP_USERDATA) : nullptr;
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEWHEEL:
            if (hDlg) return SendMessageW(hDlg, WM_MOUSEWHEEL, wParam, lParam);
            break;
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, RGB(30, 41, 59));
            SetBkColor(hdcEdit, GetSysColor(COLOR_BTNFACE));
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }
        case WM_LBUTTONDOWN: {
            if (st) {
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                const int padX = 16;
                const int col1W = 84;
                const int sep1X = padX + col1W;
                const int rowH = 26;
                const int rowCount = (int)(sizeof(kNamingSyntaxRows) / sizeof(kNamingSyntaxRows[0]));
                int idx = (my + st->scrollY - 4) / rowH;
                if (mx >= padX && mx < sep1X && idx >= 0 && idx < rowCount) {
                    st->pressedRowIdx = idx;
                    SetCapture(hViewport);
                    return 0;
                }
                st->pressedRowIdx = -1;
            }
            break;
        }
        case WM_LBUTTONUP: {
            if (st && st->pressedRowIdx >= 0) {
                int pressedIdx = st->pressedRowIdx;
                st->pressedRowIdx = -1;
                if (GetCapture() == hViewport) {
                    ReleaseCapture();
                }
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                const int padX = 16;
                const int col1W = 84;
                const int sep1X = padX + col1W;
                const int rowH = 26;
                const int rowCount = (int)(sizeof(kNamingSyntaxRows) / sizeof(kNamingSyntaxRows[0]));
                int idx = (my + st->scrollY - 4) / rowH;
                if (mx >= padX && mx < sep1X && idx == pressedIdx && idx >= 0 && idx < rowCount) {
                    if (idx < (int)st->hSyntaxEdits.size() && st->hSyntaxEdits[idx]) {
                        SetFocus(st->hSyntaxEdits[idx]);
                        SendMessageW(st->hSyntaxEdits[idx], EM_SETSEL, 0, -1);
                    }
                    CopySyntaxStringToClipboard(hDlg, kNamingSyntaxRows[idx].syntax);
                    st->copiedSyntax = kNamingSyntaxRows[idx].syntax;
                    if (hDlg) InvalidateRect(hDlg, nullptr, FALSE);
                }
                return 0;
            }
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hViewport, &ps);
            RECT rcV;
            GetClientRect(hViewport, &rcV);
            int vw = std::max(1, (int)(rcV.right - rcV.left));
            int vh = std::max(1, (int)(rcV.bottom - rcV.top));

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, vw, vh);
            HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

            FillRect(memDC, &rcV, GetSysColorBrush(COLOR_BTNFACE));
            SetBkMode(memDC, TRANSPARENT);

            HFONT hCodeFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
            HFONT hRowFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            HGDIOBJ hOldF = SelectObject(memDC, hRowFont);

            const int padX = 16;
            const int col1X = padX;
            const int col1W = 84;
            const int sep1X = col1X + col1W;
            const int col2X = sep1X + 12;
            const int col2W = 224;
            const int sep2X = col2X + col2W;
            const int col3X = sep2X + 12;
            const int rightEdge = vw - padX;
            const int rowH = 26;
            const int rowCount = (int)(sizeof(kNamingSyntaxRows) / sizeof(kNamingSyntaxRows[0]));
            const int sy = st ? st->scrollY : 0;

            HPEN hRowPen = CreatePen(PS_SOLID, 1, RGB(214, 218, 224));
            HGDIOBJ hOldP = SelectObject(memDC, hRowPen);

            for (int i = 0; i < rowCount; ++i) {
                int y = 4 + i * rowH - sy;
                if (y + rowH < 0 || y > vh) continue;
                if (i > 0) {
                    SelectObject(memDC, hRowPen);
                    MoveToEx(memDC, padX, y, nullptr);
                    LineTo(memDC, rightEdge, y);
                }

                SelectObject(memDC, hRowFont);
                SetTextColor(memDC, GetSysColor(COLOR_WINDOWTEXT));
                RECT rcC2 = { col2X, y, sep2X - 6, y + rowH };
                DrawTextW(memDC, kNamingSyntaxRows[i].explanation.c_str(), -1, &rcC2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                SelectObject(memDC, hCodeFont);
                SetTextColor(memDC, RGB(30, 41, 59));
                RECT rcC3 = { col3X, y, rightEdge, y + rowH };
                DrawTextW(memDC, kNamingSyntaxRows[i].example.c_str(), -1, &rcC3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }

            SelectObject(memDC, hRowPen);
            MoveToEx(memDC, sep1X, 0, nullptr);
            LineTo(memDC, sep1X, vh - 4);
            MoveToEx(memDC, sep2X, 0, nullptr);
            LineTo(memDC, sep2X, vh - 4);

            SelectObject(memDC, hOldP);
            SelectObject(memDC, hOldF);
            DeleteObject(hRowPen);
            DeleteObject(hRowFont);
            DeleteObject(hCodeFont);

            BitBlt(hdc, 0, 0, vw, vh, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);

            EndPaint(hViewport, &ps);
            return 0;
        }
    }
    return DefWindowProcW(hViewport, msg, wParam, lParam);
}

static void UpdateNamingSyntaxScroll(HWND hWnd, NamingSyntaxDlgState* st, int newScrollY) {
    if (!hWnd || !st) return;
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int clientW = std::max(1, (int)(rcClient.right - rcClient.left));
    int clientH = std::max(1, (int)(rcClient.bottom - rcClient.top));
    int viewportH = std::max(1, clientH - st->headerHeight - st->footerHeight);
    int maxScroll = std::max(0, st->scrollContentHeight - viewportH);
    int clamped = std::max(0, std::min(newScrollY, maxScroll));
    int dy = st->scrollY - clamped;
    st->scrollY = clamped;
    if (st->hListViewport) {
        SetWindowPos(st->hListViewport, HWND_TOP, 0, st->headerHeight, clientW, viewportH, SWP_NOACTIVATE);
        const int col1X = 16;
        const int col1W = 84;
        const int rowH = 26;
        for (size_t i = 0; i < st->hSyntaxEdits.size(); ++i) {
            if (st->hSyntaxEdits[i]) {
                int y = 4 + (int)i * rowH - st->scrollY;
                SetWindowPos(st->hSyntaxEdits[i], nullptr, col1X, y + 4, col1W - 8, rowH - 7,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
    }
    if (st->hOkBtn) {
        int footerTop = std::max(0, clientH - st->footerHeight);
        SetWindowPos(st->hOkBtn, HWND_TOP, clientW - 16 - 88, footerTop + 10, 88, 30, SWP_NOACTIVATE);
    }
    SCROLLINFO si = { sizeof(SCROLLINFO) };
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    si.nMin = 0;
    si.nMax = st->scrollContentHeight - 1;
    si.nPage = (UINT)viewportH;
    si.nPos = st->scrollY;
    SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
    if (dy != 0) {
        if (st->hListViewport) {
            RedrawWindow(st->hListViewport, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        }
        InvalidateRect(hWnd, nullptr, FALSE);
    }
}

static LRESULT CALLBACK NamingSyntaxDlgWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    NamingSyntaxDlgState* st = (NamingSyntaxDlgState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            st = (NamingSyntaxDlgState*)cs->lpCreateParams;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);

            HFONT hBoldFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            HFONT hCodeFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
            RECT rcC;
            GetClientRect(hWnd, &rcC);
            int clientW = rcC.right - rcC.left;
            int clientH = rcC.bottom - rcC.top;

            static bool vpReg = false;
            if (!vpReg) {
                WNDCLASSEXW vpwc = { sizeof(WNDCLASSEXW) };
                vpwc.lpfnWndProc = NamingSyntaxViewportWndProc;
                vpwc.hInstance = g_Daemon.hInst;
                vpwc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
                vpwc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
                vpwc.lpszClassName = L"PepperSnapNamingSyntaxViewport";
                RegisterClassExW(&vpwc);
                vpReg = true;
            }

            if (st) {
                st->hCodeFont = hCodeFont;
                int vpH = std::max(1, clientH - st->headerHeight - st->footerHeight);
                st->hListViewport = CreateWindowExW(
                    0, L"PepperSnapNamingSyntaxViewport", L"",
                    WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                    0, st->headerHeight, clientW, vpH,
                    hWnd, nullptr, g_Daemon.hInst, nullptr
                );

                const int rowCount = (int)(sizeof(kNamingSyntaxRows) / sizeof(kNamingSyntaxRows[0]));
                const int col1X = 16;
                const int col1W = 84;
                const int rowH = 26;
                st->hSyntaxEdits.resize(rowCount, nullptr);
                for (int i = 0; i < rowCount; ++i) {
                    int y = 4 + i * rowH;
                    HWND hEd = CreateWindowExW(
                        0, L"EDIT", kNamingSyntaxRows[i].syntax.c_str(),
                        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | ES_READONLY | ES_LEFT,
                        col1X, y + 4, col1W - 8, rowH - 7,
                        st->hListViewport, (HMENU)(INT_PTR)(3000 + i), g_Daemon.hInst, nullptr
                    );
                    SendMessageW(hEd, WM_SETFONT, (WPARAM)hCodeFont, TRUE);
                    WNDPROC origEditProc = (WNDPROC)SetWindowLongPtrW(hEd, GWLP_WNDPROC, (LONG_PTR)NamingSyntaxEditSubclassProc);
                    SetPropW(hEd, L"PepperSnapOrigSyntaxEditProc", (HANDLE)origEditProc);
                    SetPropW(hEd, L"PepperSnapSyntaxRowIdx", (HANDLE)(INT_PTR)(i + 1));
                    st->hSyntaxEdits[i] = hEd;
                }
            }

            HWND hOk = CreateWindowExW(0, L"BUTTON", L"OK",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_DEFPUSHBUTTON,
                clientW - 16 - 88, clientH - 40, 88, 30, hWnd, (HMENU)IDOK, nullptr, nullptr);
            SendMessageW(hOk, WM_SETFONT, (WPARAM)hBoldFont, TRUE);
            if (st) {
                st->hOkBtn = hOk;
                UpdateNamingSyntaxScroll(hWnd, st, 0);
            }
            return 0;
        }
        case WM_DESTROY:
            if (st && st->hCodeFont) {
                DeleteObject(st->hCodeFont);
                st->hCodeFont = nullptr;
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            if (st && st->fixedWinWidth > 0) {
                mmi->ptMinTrackSize.x = st->fixedWinWidth;
                mmi->ptMaxTrackSize.x = st->fixedWinWidth;
                mmi->ptMinTrackSize.y = 260;
                int maxH = std::min(st->fullWinHeight, std::max(300, GetSystemMetrics(SM_CYSCREEN) - 24));
                mmi->ptMaxTrackSize.y = std::max(260, maxH);
            }
            return 0;
        }
        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProcW(hWnd, msg, wParam, lParam);
            if (hit == HTLEFT || hit == HTRIGHT) return HTBORDER;
            if (hit == HTTOPLEFT || hit == HTTOPRIGHT) return HTTOP;
            if (hit == HTBOTTOMLEFT || hit == HTBOTTOMRIGHT) return HTBOTTOM;
            return hit;
        }
        case WM_SIZE: {
            if (st && wParam != SIZE_MINIMIZED) {
                UpdateNamingSyntaxScroll(hWnd, st, st->scrollY);
                InvalidateRect(hWnd, nullptr, FALSE);
                RECT rcW;
                if (GetWindowRect(hWnd, &rcW)) {
                    st->savedWinHeight = rcW.bottom - rcW.top;
                }
            }
            return 0;
        }
        case WM_VSCROLL: {
            if (st && lParam == 0) {
                RECT rcC;
                GetClientRect(hWnd, &rcC);
                int viewportH = std::max(60, (int)(rcC.bottom - rcC.top) - st->headerHeight - st->footerHeight);
                int pageH = std::max(40, viewportH - 26);
                int target = st->scrollY;
                switch (LOWORD(wParam)) {
                    case SB_LINEUP:        target -= 26; break;
                    case SB_LINEDOWN:      target += 26; break;
                    case SB_PAGEUP:        target -= pageH; break;
                    case SB_PAGEDOWN:      target += pageH; break;
                    case SB_TOP:           target = 0; break;
                    case SB_BOTTOM:        target = st->scrollContentHeight; break;
                    case SB_THUMBTRACK:
                    case SB_THUMBPOSITION: {
                        SCROLLINFO si = { sizeof(SCROLLINFO), SIF_TRACKPOS };
                        if (GetScrollInfo(hWnd, SB_VERT, &si)) {
                            target = si.nTrackPos;
                        } else {
                            target = HIWORD(wParam);
                        }
                        break;
                    }
                    default: break;
                }
                UpdateNamingSyntaxScroll(hWnd, st, target);
                return 0;
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            if (st) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (delta != 0) {
                    int step = -(delta * 52) / WHEEL_DELTA;
                    UpdateNamingSyntaxScroll(hWnd, st, st->scrollY + step);
                }
                return 0;
            }
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            int clientW = std::max(1, (int)(rcClient.right - rcClient.left));
            int clientH = std::max(1, (int)(rcClient.bottom - rcClient.top));

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, clientW, clientH);
            HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

            FillRect(memDC, &rcClient, GetSysColorBrush(COLOR_BTNFACE));
            SetBkMode(memDC, TRANSPARENT);

            HFONT hHeaderFont = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            HFONT hCodeFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
            HFONT hRowFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            HGDIOBJ hOldF = SelectObject(memDC, hHeaderFont);

            const int padX = 16;
            const int col1X = padX;
            const int col1W = 84;
            const int sep1X = col1X + col1W;
            const int col2X = sep1X + 12;
            const int col2W = 224;
            const int sep2X = col2X + col2W;
            const int col3X = sep2X + 12;
            const int rightEdge = clientW - padX;

            const int headerH = st ? st->headerHeight : 36;
            const int footerH = st ? st->footerHeight : 50;
            const int footerTop = std::max(headerH, clientH - footerH);
            const int rowH = 26;
            const int rowCount = (int)(sizeof(kNamingSyntaxRows) / sizeof(kNamingSyntaxRows[0]));
            const int sy = st ? st->scrollY : 0;

            HPEN hHeaderLinePen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
            HPEN hRowPen = CreatePen(PS_SOLID, 1, RGB(214, 218, 224));
            HGDIOBJ hOldP = SelectObject(memDC, hHeaderLinePen);

            // Pinned top header row
            SelectObject(memDC, hHeaderFont);
            SetTextColor(memDC, RGB(220, 38, 38));
            RECT rcH1 = { col1X, 6, sep1X - 6, headerH - 2 };
            RECT rcH2 = { col2X, 6, sep2X - 6, headerH - 2 };
            RECT rcH3 = { col3X, 6, rightEdge, headerH - 2 };
            DrawTextW(memDC, L"Syntax",      -1, &rcH1, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            DrawTextW(memDC, L"Explanation", -1, &rcH2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            DrawTextW(memDC, L"Example",     -1, &rcH3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            SelectObject(memDC, hHeaderLinePen);
            MoveToEx(memDC, padX, headerH - 1, nullptr);
            LineTo(memDC, rightEdge, headerH - 1);

            // Scrollable table body
            int savedClip = SaveDC(memDC);
            IntersectClipRect(memDC, 0, headerH, clientW, footerTop);

            for (int i = 0; i < rowCount; ++i) {
                int y = headerH + 4 + i * rowH - sy;
                if (y + rowH < headerH || y > footerTop) continue;
                if (i > 0) {
                    SelectObject(memDC, hRowPen);
                    MoveToEx(memDC, padX, y, nullptr);
                    LineTo(memDC, rightEdge, y);
                }

                SelectObject(memDC, hCodeFont);
                SetTextColor(memDC, RGB(30, 41, 59));
                RECT rcC1 = { col1X, y, sep1X - 6, y + rowH };
                DrawTextW(memDC, kNamingSyntaxRows[i].syntax.c_str(), -1, &rcC1, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                SelectObject(memDC, hRowFont);
                SetTextColor(memDC, GetSysColor(COLOR_WINDOWTEXT));
                RECT rcC2 = { col2X, y, sep2X - 6, y + rowH };
                DrawTextW(memDC, kNamingSyntaxRows[i].explanation.c_str(), -1, &rcC2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                SelectObject(memDC, hCodeFont);
                SetTextColor(memDC, RGB(30, 41, 59));
                RECT rcC3 = { col3X, y, rightEdge, y + rowH };
                DrawTextW(memDC, kNamingSyntaxRows[i].example.c_str(), -1, &rcC3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }

            // Vertical column separator lines
            SelectObject(memDC, hRowPen);
            MoveToEx(memDC, sep1X, 8, nullptr);
            LineTo(memDC, sep1X, footerTop - 4);
            MoveToEx(memDC, sep2X, 8, nullptr);
            LineTo(memDC, sep2X, footerTop - 4);

            RestoreDC(memDC, savedClip);

            // Vertical dividers in header as well
            SelectObject(memDC, hRowPen);
            MoveToEx(memDC, sep1X, 8, nullptr);
            LineTo(memDC, sep1X, headerH - 2);
            MoveToEx(memDC, sep2X, 8, nullptr);
            LineTo(memDC, sep2X, headerH - 2);

            // Pinned bottom footer bar
            RECT rcFooter = { 0, footerTop, clientW, clientH };
            FillRect(memDC, &rcFooter, GetSysColorBrush(COLOR_BTNFACE));
            SelectObject(memDC, hHeaderLinePen);
            MoveToEx(memDC, 0, footerTop, nullptr);
            LineTo(memDC, clientW, footerTop);

            SelectObject(memDC, hRowFont);
            RECT rcHint = { padX, footerTop + 10, clientW - 116, clientH - 10 };
            if (st && !st->copiedSyntax.empty()) {
                SetTextColor(memDC, RGB(22, 163, 74));
                std::wstring copiedMsg = L"\x2713 Copied " + st->copiedSyntax + L" to clipboard";
                DrawTextW(memDC, copiedMsg.c_str(), -1, &rcHint, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            } else {
                SetTextColor(memDC, GetSysColor(COLOR_GRAYTEXT));
                DrawTextW(memDC, L"Click or select any syntax to copy", -1, &rcHint, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }

            SelectObject(memDC, hOldP);
            SelectObject(memDC, hOldF);
            DeleteObject(hRowPen);
            DeleteObject(hHeaderLinePen);
            DeleteObject(hRowFont);
            DeleteObject(hCodeFont);
            DeleteObject(hHeaderFont);

            BitBlt(hdc, 0, 0, clientW, clientH, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);

            EndPaint(hWnd, &ps);
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

static void ShowNamingSyntaxModal(HWND hParentOptWnd) {
    if (g_Daemon.hNamingSyntaxWnd && IsWindow(g_Daemon.hNamingSyntaxWnd)) {
        BringWindowToTop(g_Daemon.hNamingSyntaxWnd);
        SetForegroundWindow(g_Daemon.hNamingSyntaxWnd);
        return;
    }

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = NamingSyntaxDlgWndProc;
        wc.hInstance = g_Daemon.hInst;
        wc.hIcon = LoadIconW(g_Daemon.hInst, MAKEINTRESOURCEW(IDI_APPICON));
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"PepperSnapNamingSyntaxModal";
        RegisterClassExW(&wc);
        reg = true;
    }

    NamingSyntaxDlgState st;
    const int rowCount = (int)(sizeof(kNamingSyntaxRows) / sizeof(kNamingSyntaxRows[0]));
    st.headerHeight = 36;
    st.footerHeight = 50;
    st.scrollContentHeight = 4 + rowCount * 26 + 6;

    DWORD dwStyle = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VSCROLL | WS_VISIBLE | WS_CLIPCHILDREN;
    DWORD dwExStyle = WS_EX_TOPMOST;
    const int clientW = 468;
    const int clientH = st.headerHeight + st.scrollContentHeight + st.footerHeight;
    RECT rcWin = { 0, 0, clientW, clientH };
    AdjustWindowRectEx(&rcWin, dwStyle, FALSE, dwExStyle);
    int fixedW = (rcWin.right - rcWin.left) + GetSystemMetrics(SM_CXVSCROLL);
    int fullWinH = rcWin.bottom - rcWin.top;

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    int maxAllowedH = std::min(fullWinH, std::max(300, sh - 36));
    int winH = std::max(260, std::min(maxAllowedH, g_Daemon.namingSyntaxWindowHeight));
    st.fixedWinWidth = fixedW;
    st.fullWinHeight = fullWinH;
    st.savedWinHeight = winH;

    int posX = (sw - fixedW) / 2;
    int posY = (sh - winH) / 2;
    if (hParentOptWnd && IsWindow(hParentOptWnd)) {
        RECT rcP;
        if (GetWindowRect(hParentOptWnd, &rcP)) {
            posX = rcP.left + ((rcP.right - rcP.left) - fixedW) / 2;
            posY = std::max(12, (int)(rcP.top + ((rcP.bottom - rcP.top) - winH) / 2));
            if (posY + winH > sh - 12) posY = std::max(12, sh - 12 - winH);
        }
        EnableWindow(hParentOptWnd, FALSE);
    }

    HWND hDlg = CreateWindowExW(
        dwExStyle, L"PepperSnapNamingSyntaxModal",
        L"PepperSnap — naming pattern syntax",
        dwStyle,
        posX, std::max(10, posY), fixedW, winH,
        hParentOptWnd, nullptr, g_Daemon.hInst, &st
    );
    g_Daemon.hNamingSyntaxWnd = hDlg;
    BringWindowToTop(hDlg);
    SetForegroundWindow(hDlg);

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_MOUSEWHEEL && (msg.hwnd == hDlg || IsChild(hDlg, msg.hwnd))) {
            SendMessageW(hDlg, WM_MOUSEWHEEL, msg.wParam, msg.lParam);
            continue;
        }
        if (msg.hwnd == hDlg || IsChild(hDlg, msg.hwnd)) {
            if (msg.message == WM_KEYDOWN && (msg.wParam == VK_RETURN || msg.wParam == VK_ESCAPE)) {
                DestroyWindow(hDlg);
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_Daemon.hNamingSyntaxWnd = nullptr;
    if (st.savedWinHeight >= 260 && st.savedWinHeight != g_Daemon.namingSyntaxWindowHeight) {
        g_Daemon.namingSyntaxWindowHeight = st.savedWinHeight;
        g_Daemon.SaveSettings();
    }
    if (hParentOptWnd && IsWindow(hParentOptWnd)) {
        EnableWindow(hParentOptWnd, TRUE);
        BringWindowToTop(hParentOptWnd);
        SetForegroundWindow(hParentOptWnd);
        SetActiveWindow(hParentOptWnd);
    }
}

struct CustomizeKeyRow {
    int slot = 0; // 1..20, or 0 for section separator
    bool isSeparator = false;
    bool hasIcon = false;
    DockButton iconBtn;
    std::wstring label;
    HotkeyBinding* bindingPtr = nullptr;
    HotkeyBinding defaultBinding{ 0, 0 };
    int y = 0;
    int h = 32;
    HWND hBtn = nullptr;
};

struct CustomizeKeysDlgState {
    HotkeyBinding hkRegion;
    HotkeyBinding hkFull;
    HotkeyBinding hkPrev;
    HotkeyBinding hkToolSelect;
    HotkeyBinding hkToolPen;
    HotkeyBinding hkToolStabilo;
    HotkeyBinding hkToolLine;
    HotkeyBinding hkToolArrow;
    HotkeyBinding hkToolNumber;
    HotkeyBinding hkToolResetNum;
    HotkeyBinding hkToolRect;
    HotkeyBinding hkToolEllipse;
    HotkeyBinding hkToolText;
    HotkeyBinding hkToolMosaicSq;
    HotkeyBinding hkToolMosaicCir;
    HotkeyBinding hkActUndo;
    HotkeyBinding hkActRedo;
    HotkeyBinding hkActClear;
    HotkeyBinding hkActOptions;
    HotkeyBinding hkActPin;
    HotkeyBinding hkActOcr;
    HotkeyBinding hkActSaveAs;
    HotkeyBinding hkActSave;
    HotkeyBinding hkActCopy;
    HotkeyBinding hkFramePlayPause;
    HotkeyBinding hkFrameToggleStrip;
    HotkeyBinding hkPinZoomOut;
    HotkeyBinding hkPinZoomIn;
    HotkeyBinding hkPinZoomReset;
    HotkeyBinding hkPinOutline;
    HotkeyBinding hkPinHideToolbar;
    HotkeyBinding hkPinSmooth;
    HotkeyBinding hkPinSaveAs;
    HotkeyBinding hkPinSave;
    HotkeyBinding hkPinCopy;
    HotkeyBinding hkPinClose;

    std::vector<CustomizeKeyRow> rows;
    bool confirmed = false;
    int scrollY = 0;
    int scrollContentHeight = 680;
    int footerHeight = 50;
    int fixedWinWidth = 460;
    int fullWinHeight = 730;
    int savedWinHeight = 560;

    HWND hFooterBg = nullptr;
    HWND hRestoreBtn = nullptr;
    HWND hSaveBtn = nullptr;
    HWND hCancelBtn = nullptr;
};

static void UpdateCustomizeKeysRecordingTooltip(HWND hWnd, CustomizeKeysDlgState* ck) {
    if (!hWnd || !ck || g_RecordingHotkeySlot <= 0) {
        HidePinnedBubbleTooltip();
        return;
    }
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int footerTop = std::max(0, (int)(rcClient.bottom - rcClient.top) - ck->footerHeight);
    for (const auto& r : ck->rows) {
        if (!r.isSeparator && r.slot == g_RecordingHotkeySlot && r.hBtn && IsWindow(r.hBtn)) {
            RECT rcBtnClient;
            GetWindowRect(r.hBtn, &rcBtnClient);
            MapWindowPoints(HWND_DESKTOP, hWnd, (LPPOINT)&rcBtnClient, 2);
            if (rcBtnClient.bottom <= 0 || rcBtnClient.top >= footerTop) {
                HidePinnedBubbleTooltip();
                return;
            }
            DockButton tipBtn;
            tipBtn.rect = rcBtnClient;
            tipBtn.tooltip = L"Esc to cancel, and Delete to leave it empty";
            ShowPinnedBubbleTooltip(hWnd, tipBtn);
            return;
        }
    }
    HidePinnedBubbleTooltip();
}

static void RefreshCustomizeKeysButtonLabels(HWND hWnd, CustomizeKeysDlgState* ck) {
    if (!ck) return;
    for (const auto& r : ck->rows) {
        if (r.isSeparator || !r.hBtn || !r.bindingPtr) continue;
        std::wstring txt = (g_RecordingHotkeySlot == r.slot)
            ? L"Press key... "
            : PepperSnapDaemon::FormatHotkeyString(*r.bindingPtr);
        SetWindowTextW(r.hBtn, txt.c_str());
    }
    UpdateCustomizeKeysRecordingTooltip(hWnd, ck);
}

static void LayoutCustomizeKeysFooter(HWND hWnd, CustomizeKeysDlgState* ck) {
    if (!hWnd || !ck) return;
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int clientW = std::max(1, (int)(rcClient.right - rcClient.left));
    int clientH = std::max(1, (int)(rcClient.bottom - rcClient.top));
    int footerTop = std::max(0, clientH - ck->footerHeight);
    if (ck->hFooterBg) {
        SetWindowPos(ck->hFooterBg, HWND_TOP, 0, footerTop, clientW, ck->footerHeight, SWP_NOACTIVATE);
    }
    if (ck->hRestoreBtn) {
        SetWindowPos(ck->hRestoreBtn, HWND_TOP, 16, footerTop + 9, 128, 32, SWP_NOACTIVATE);
    }
    if (ck->hSaveBtn) {
        SetWindowPos(ck->hSaveBtn, HWND_TOP, clientW - 16 - 88 - 8 - 88, footerTop + 9, 88, 32, SWP_NOACTIVATE);
    }
    if (ck->hCancelBtn) {
        SetWindowPos(ck->hCancelBtn, HWND_TOP, clientW - 16 - 88, footerTop + 9, 88, 32, SWP_NOACTIVATE);
    }
}

static void UpdateCustomizeKeysScroll(HWND hWnd, CustomizeKeysDlgState* ck, int newScrollY) {
    if (!hWnd || !ck) return;
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int clientW = std::max(1, (int)(rcClient.right - rcClient.left));
    int clientH = std::max(1, (int)(rcClient.bottom - rcClient.top));
    int viewportH = std::max(1, clientH - ck->footerHeight);
    int maxScroll = std::max(0, ck->scrollContentHeight - viewportH);
    int clamped = std::max(0, std::min(newScrollY, maxScroll));
    int dy = ck->scrollY - clamped;
    ck->scrollY = clamped;
    if (dy != 0) {
        for (const auto& r : ck->rows) {
            if (!r.isSeparator && r.hBtn) {
                int btnY = r.y + (r.h - 26) / 2 - ck->scrollY;
                SetWindowPos(r.hBtn, nullptr, 256, btnY, 156, 26, SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
    }
    LayoutCustomizeKeysFooter(hWnd, ck);
    SCROLLINFO si = { sizeof(SCROLLINFO) };
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    si.nMin = 0;
    si.nMax = ck->scrollContentHeight - 1;
    si.nPage = (UINT)viewportH;
    si.nPos = ck->scrollY;
    SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
    UpdateCustomizeKeysRecordingTooltip(hWnd, ck);
    if (dy != 0) {
        RECT rcInval = { 0, 0, clientW, clientH };
        RedrawWindow(hWnd, &rcInval, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
}

static LRESULT CALLBACK CustomizeKeysDlgWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CustomizeKeysDlgState* ck = (CustomizeKeysDlgState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            ck = (CustomizeKeysDlgState*)cs->lpCreateParams;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)ck);

            HFONT hFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            HFONT hBoldFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

            if (ck) {
                for (auto& r : ck->rows) {
                    if (r.isSeparator) continue;
                    int btnY = r.y + (r.h - 26) / 2;
                    r.hBtn = CreateWindowExW(0, L"BUTTON", L"",
                        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                        256, btnY, 156, 26, hWnd, (HMENU)(INT_PTR)(IDC_CK_BTN_BASE + r.slot), nullptr, nullptr);
                    SendMessageW(r.hBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
                }

                ck->hFooterBg = CreateWindowExW(0, L"STATIC", L"",
                    WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 500, 428, ck->footerHeight, hWnd, nullptr, nullptr, nullptr);

                ck->hRestoreBtn = CreateWindowExW(0, L"BUTTON", L"Restore defaults",
                    WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 16, 509, 128, 32, hWnd, (HMENU)IDC_CK_RESTORE_DEFAULTS, nullptr, nullptr);
                SendMessageW(ck->hRestoreBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

                ck->hSaveBtn = CreateWindowExW(0, L"BUTTON", L"Save",
                    WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_DEFPUSHBUTTON, 228, 509, 88, 32, hWnd, (HMENU)IDOK, nullptr, nullptr);
                SendMessageW(ck->hSaveBtn, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

                ck->hCancelBtn = CreateWindowExW(0, L"BUTTON", L"Cancel",
                    WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 324, 509, 88, 32, hWnd, (HMENU)IDCANCEL, nullptr, nullptr);
                SendMessageW(ck->hCancelBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

                RefreshCustomizeKeysButtonLabels(hWnd, ck);
                LayoutCustomizeKeysFooter(hWnd, ck);
                UpdateCustomizeKeysScroll(hWnd, ck, 0);
            }
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            int clientW = std::max(1, (int)(rcClient.right - rcClient.left));
            int clientH = std::max(1, (int)(rcClient.bottom - rcClient.top));
            if (ck) {
                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP memBmp = CreateCompatibleBitmap(hdc, clientW, clientH);
                HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

                RECT rcAll = { 0, 0, clientW, clientH };
                FillRect(memDC, &rcAll, GetSysColorBrush(COLOR_BTNFACE));

                int footerTop = std::max(0, clientH - ck->footerHeight);
                int savedDC = SaveDC(memDC);
                IntersectClipRect(memDC, 0, 0, clientW, footerTop);

                SetBkMode(memDC, TRANSPARENT);
                HFONT hRowFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                HGDIOBJ hOldF = SelectObject(memDC, hRowFont);

                HPEN hSepShadow = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
                HPEN hSepHilight = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DHILIGHT));
                HPEN hRowSep = CreatePen(PS_SOLID, 1, RGB(218, 222, 228));
                HGDIOBJ hOldP = SelectObject(memDC, hSepShadow);

                int sy = ck->scrollY;
                int padX = 16;
                int rightEdge = clientW - padX;

                for (size_t i = 0; i < ck->rows.size(); ++i) {
                    const auto& r = ck->rows[i];
                    int drawY = r.y - sy;
                    if (drawY + r.h < 0 || drawY > footerTop) continue;

                    if (r.isSeparator) {
                        int lineY = drawY + r.h / 2;
                        SelectObject(memDC, hSepShadow);
                        MoveToEx(memDC, padX, lineY, nullptr);
                        LineTo(memDC, rightEdge, lineY);
                        SelectObject(memDC, hSepHilight);
                        MoveToEx(memDC, padX, lineY + 1, nullptr);
                        LineTo(memDC, rightEdge, lineY + 1);
                    } else {
                        if (i > 0 && !ck->rows[i - 1].isSeparator) {
                            SelectObject(memDC, hRowSep);
                            MoveToEx(memDC, padX, drawY, nullptr);
                            LineTo(memDC, rightEdge, drawY);
                        }

                        int textLeft = padX;
                        if (r.hasIcon) {
                            Graphics gIcon(memDC);
                            gIcon.SetSmoothingMode(SmoothingModeAntiAlias);
                            gIcon.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
                            const int iconW = 26;
                            const int iconH = 22;
                            int iconX = padX;
                            int iconY = drawY + (r.h - iconH) / 2;
                            RectF rfIcon((float)iconX, (float)iconY, (float)iconW, (float)iconH);
                            Color bgCol = r.iconBtn.isPrimaryAction ? Color(255, 239, 68, 68) : Color(245, 15, 23, 42);
                            SolidBrush btnBg(bgCol);
                            Pen btnBorder(r.iconBtn.isPrimaryAction ? Color(255, 252, 165, 165) : Color(220, 51, 65, 85), 1.0f);
                            GraphicsState stB = gIcon.Save();
                            gIcon.SetSmoothingMode(SmoothingModeNone);
                            gIcon.SetPixelOffsetMode(PixelOffsetModeNone);
                            gIcon.FillRectangle(&btnBg, iconX, iconY, iconW, iconH);
                            gIcon.Restore(stB);
                            PepperSnapDaemon::DrawDockButtonIcon(gIcon, r.iconBtn, rfIcon);
                            GraphicsState stO = gIcon.Save();
                            gIcon.SetSmoothingMode(SmoothingModeNone);
                            gIcon.SetPixelOffsetMode(PixelOffsetModeNone);
                            gIcon.DrawRectangle(&btnBorder, iconX, iconY, iconW - 1, iconH - 1);
                            gIcon.Restore(stO);
                            textLeft += iconW + 8;
                        }

                        SelectObject(memDC, hRowFont);
                        SetTextColor(memDC, GetSysColor(COLOR_WINDOWTEXT));
                        RECT rcLbl = { textLeft, drawY, 250, drawY + r.h };
                        DrawTextW(memDC, r.label.c_str(), -1, &rcLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                    }
                }

                SelectObject(memDC, hOldP);
                SelectObject(memDC, hOldF);
                DeleteObject(hRowSep);
                DeleteObject(hSepHilight);
                DeleteObject(hSepShadow);
                DeleteObject(hRowFont);

                RestoreDC(memDC, savedDC);

                RECT rcFooter = { 0, footerTop, clientW, clientH };
                FillRect(memDC, &rcFooter, GetSysColorBrush(COLOR_BTNFACE));
                HPEN hLinePen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
                HGDIOBJ hOldFooterPen = SelectObject(memDC, hLinePen);
                MoveToEx(memDC, 0, footerTop, nullptr);
                LineTo(memDC, clientW, footerTop);
                SelectObject(memDC, hOldFooterPen);
                DeleteObject(hLinePen);

                BitBlt(hdc, 0, 0, clientW, clientH, memDC, 0, 0, SRCCOPY);
                SelectObject(memDC, oldBmp);
                DeleteObject(memBmp);
                DeleteDC(memDC);
            }
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            if (ck && ck->hFooterBg && (HWND)lParam == ck->hFooterBg) {
                HDC hdcStatic = (HDC)wParam;
                RECT rcF;
                GetClientRect(ck->hFooterBg, &rcF);
                FillRect(hdcStatic, &rcF, GetSysColorBrush(COLOR_BTNFACE));
                HPEN hLinePen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
                HGDIOBJ hOldPen = SelectObject(hdcStatic, hLinePen);
                MoveToEx(hdcStatic, 0, 0, nullptr);
                LineTo(hdcStatic, rcF.right, 0);
                SelectObject(hdcStatic, hOldPen);
                DeleteObject(hLinePen);
                return (LRESULT)GetStockObject(NULL_BRUSH);
            }
            break;
        }
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            if (ck && ck->fixedWinWidth > 0) {
                mmi->ptMinTrackSize.x = ck->fixedWinWidth;
                mmi->ptMaxTrackSize.x = ck->fixedWinWidth;
                mmi->ptMinTrackSize.y = 280;
                int maxH = std::min(ck->fullWinHeight, std::max(320, GetSystemMetrics(SM_CYSCREEN) - 24));
                mmi->ptMaxTrackSize.y = std::max(280, maxH);
            }
            return 0;
        }
        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProcW(hWnd, msg, wParam, lParam);
            if (hit == HTLEFT || hit == HTRIGHT) return HTBORDER;
            if (hit == HTTOPLEFT || hit == HTTOPRIGHT) return HTTOP;
            if (hit == HTBOTTOMLEFT || hit == HTBOTTOMRIGHT) return HTBOTTOM;
            return hit;
        }
        case WM_SIZE: {
            if (ck && wParam != SIZE_MINIMIZED) {
                LayoutCustomizeKeysFooter(hWnd, ck);
                UpdateCustomizeKeysScroll(hWnd, ck, ck->scrollY);
                InvalidateRect(hWnd, nullptr, FALSE);
                RECT rcW;
                if (GetWindowRect(hWnd, &rcW)) {
                    ck->savedWinHeight = rcW.bottom - rcW.top;
                }
            }
            return 0;
        }
        case WM_MOVE: {
            if (ck && g_RecordingHotkeySlot > 0) {
                UpdateCustomizeKeysRecordingTooltip(hWnd, ck);
            }
            return 0;
        }
        case WM_VSCROLL: {
            if (ck && lParam == 0) {
                RECT rcC;
                GetClientRect(hWnd, &rcC);
                int viewportH = std::max(60, (int)(rcC.bottom - rcC.top) - ck->footerHeight);
                int pageH = std::max(40, viewportH - 32);
                int target = ck->scrollY;
                switch (LOWORD(wParam)) {
                    case SB_LINEUP:        target -= 32; break;
                    case SB_LINEDOWN:      target += 32; break;
                    case SB_PAGEUP:        target -= pageH; break;
                    case SB_PAGEDOWN:      target += pageH; break;
                    case SB_TOP:           target = 0; break;
                    case SB_BOTTOM:        target = ck->scrollContentHeight; break;
                    case SB_THUMBTRACK:
                    case SB_THUMBPOSITION: {
                        SCROLLINFO si = { sizeof(SCROLLINFO), SIF_TRACKPOS };
                        if (GetScrollInfo(hWnd, SB_VERT, &si)) {
                            target = si.nTrackPos;
                        } else {
                            target = HIWORD(wParam);
                        }
                        break;
                    }
                    default: break;
                }
                UpdateCustomizeKeysScroll(hWnd, ck, target);
                return 0;
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            if (ck) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (delta != 0) {
                    int step = -(delta * 48) / WHEEL_DELTA;
                    UpdateCustomizeKeysScroll(hWnd, ck, ck->scrollY + step);
                }
                return 0;
            }
            break;
        }
        case WM_SHORTCUT_RECORDED: {
            if (!ck) return 0;
            UINT mods = (UINT)wParam;
            UINT vk = (UINT)lParam;
            if (vk == VK_SNAPSHOT) {
                return 0;
            }
            int slot = g_RecordingHotkeySlot;
            g_RecordingHotkeySlot = 0;
            HidePinnedBubbleTooltip();
            if (slot > 0 && vk != 0 && vk != VK_ESCAPE) {
                for (auto& r : ck->rows) {
                    if (!r.isSeparator && r.slot == slot && r.bindingPtr) {
                        if (vk == VK_DELETE && mods == 0) {
                            *r.bindingPtr = { 0, 0 };
                        } else {
                            *r.bindingPtr = { mods, vk };
                        }
                        break;
                    }
                }
            }
            RefreshCustomizeKeysButtonLabels(hWnd, ck);
            return 0;
        }
        case WM_COMMAND: {
            if (!ck) break;
            if (HIWORD(wParam) != BN_CLICKED) break;
            WORD id = LOWORD(wParam);
            if (id >= IDC_CK_BTN_BASE + 1 && id <= IDC_CK_BTN_BASE + 50) {
                int clickedSlot = (int)(id - IDC_CK_BTN_BASE);
                g_RecordingHotkeySlot = (g_RecordingHotkeySlot == clickedSlot) ? 0 : clickedSlot;
                RefreshCustomizeKeysButtonLabels(hWnd, ck);
                SetFocus(hWnd);
                return 0;
            }
            if (id == IDC_CK_RESTORE_DEFAULTS) {
                g_RecordingHotkeySlot = 0;
                HidePinnedBubbleTooltip();
                RefreshCustomizeKeysButtonLabels(hWnd, ck);
                int ans = MessageBoxW(
                    hWnd,
                    L"Are you sure you want to restore all shortcut keys to default?",
                    L"PepperSnap — restore default shortcut keys",
                    MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 | MB_TOPMOST | MB_SETFOREGROUND
                );
                if (ans != IDYES) {
                    SetFocus(hWnd);
                    return 0;
                }
                for (auto& r : ck->rows) {
                    if (!r.isSeparator && r.bindingPtr) {
                        *r.bindingPtr = r.defaultBinding;
                    }
                }
                RefreshCustomizeKeysButtonLabels(hWnd, ck);
                SetFocus(hWnd);
                return 0;
            }
            if (id == IDOK) {
                g_RecordingHotkeySlot = 0;
                HidePinnedBubbleTooltip();
                ck->confirmed = true;
                DestroyWindow(hWnd);
                return 0;
            }
            if (id == IDCANCEL) {
                g_RecordingHotkeySlot = 0;
                HidePinnedBubbleTooltip();
                DestroyWindow(hWnd);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            g_RecordingHotkeySlot = 0;
            HidePinnedBubbleTooltip();
            DestroyWindow(hWnd);
            return 0;
        case WM_DESTROY:
            g_RecordingHotkeySlot = 0;
            HidePinnedBubbleTooltip();
            break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void ShowCustomizeKeysModal(HWND hParentOptWnd, OptionsDlgState* optSt) {
    if (!optSt) return;
    if (g_Daemon.hCustomizeKeysWnd && IsWindow(g_Daemon.hCustomizeKeysWnd)) {
        BringWindowToTop(g_Daemon.hCustomizeKeysWnd);
        SetForegroundWindow(g_Daemon.hCustomizeKeysWnd);
        return;
    }

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = CustomizeKeysDlgWndProc;
        wc.hInstance = g_Daemon.hInst;
        wc.hIcon = LoadIconW(g_Daemon.hInst, MAKEINTRESOURCEW(IDI_APPICON));
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"PepperSnapCustomizeKeysModal";
        RegisterClassExW(&wc);
        reg = true;
    }

    g_RecordingHotkeySlot = 0;
    HidePinnedBubbleTooltip();

    CustomizeKeysDlgState ck;
    ck.hkRegion        = optSt->hkRegion;
    ck.hkFull          = optSt->hkFull;
    ck.hkPrev          = optSt->hkPrev;
    ck.hkToolSelect    = optSt->hkToolSelect;
    ck.hkToolPen       = optSt->hkToolPen;
    ck.hkToolStabilo   = optSt->hkToolStabilo;
    ck.hkToolLine      = optSt->hkToolLine;
    ck.hkToolArrow     = optSt->hkToolArrow;
    ck.hkToolNumber    = optSt->hkToolNumber;
    ck.hkToolResetNum  = optSt->hkToolResetNum;
    ck.hkToolRect      = optSt->hkToolRect;
    ck.hkToolEllipse   = optSt->hkToolEllipse;
    ck.hkToolText      = optSt->hkToolText;
    ck.hkToolMosaicSq  = optSt->hkToolMosaicSq;
    ck.hkToolMosaicCir = optSt->hkToolMosaicCir;
    ck.hkActUndo          = optSt->hkActUndo;
    ck.hkActRedo          = optSt->hkActRedo;
    ck.hkActClear         = optSt->hkActClear;
    ck.hkActOptions       = optSt->hkActOptions;
    ck.hkActPin           = optSt->hkActPin;
    ck.hkActOcr           = optSt->hkActOcr;
    ck.hkActSaveAs        = optSt->hkActSaveAs;
    ck.hkActSave          = optSt->hkActSave;
    ck.hkActCopy          = optSt->hkActCopy;
    ck.hkFramePlayPause   = optSt->hkFramePlayPause;
    ck.hkFrameToggleStrip = optSt->hkFrameToggleStrip;
    ck.hkPinZoomOut       = optSt->hkPinZoomOut;
    ck.hkPinZoomIn     = optSt->hkPinZoomIn;
    ck.hkPinZoomReset   = optSt->hkPinZoomReset;
    ck.hkPinOutline     = optSt->hkPinOutline;
    ck.hkPinHideToolbar = optSt->hkPinHideToolbar;
    ck.hkPinSmooth      = optSt->hkPinSmooth;
    ck.hkPinSaveAs     = optSt->hkPinSaveAs;
    ck.hkPinSave       = optSt->hkPinSave;
    ck.hkPinCopy       = optSt->hkPinCopy;
    ck.hkPinClose      = optSt->hkPinClose;

    int nextSlot = 1;
    auto addGlobalRow = [&](const std::wstring& lbl, HotkeyBinding* ptr, HotkeyBinding defHk) {
        CustomizeKeyRow r;
        r.slot = nextSlot++;
        r.label = lbl;
        r.bindingPtr = ptr;
        r.defaultBinding = defHk;
        ck.rows.push_back(r);
    };
    auto addSep = [&]() {
        CustomizeKeyRow r;
        r.isSeparator = true;
        ck.rows.push_back(r);
    };
    auto addToolRow = [&](OverlayTool tool, const std::wstring& lbl, HotkeyBinding* ptr, HotkeyBinding defHk) {
        CustomizeKeyRow r;
        r.slot = nextSlot++;
        r.hasIcon = true;
        r.iconBtn.isTool = true;
        r.iconBtn.tool = tool;
        r.label = lbl;
        r.bindingPtr = ptr;
        r.defaultBinding = defHk;
        ck.rows.push_back(r);
    };
    auto addActRow = [&](int btnId, bool primary, const std::wstring& lbl, HotkeyBinding* ptr, HotkeyBinding defHk) {
        CustomizeKeyRow r;
        r.slot = nextSlot++;
        r.hasIcon = true;
        r.iconBtn.isTool = false;
        r.iconBtn.id = btnId;
        r.iconBtn.isPrimaryAction = primary;
        r.label = lbl;
        r.bindingPtr = ptr;
        r.defaultBinding = defHk;
        ck.rows.push_back(r);
    };

    // First 3: Global capture shortcuts
    addGlobalRow(L"Custom area",                       &ck.hkRegion, { MOD_CONTROL, VK_SNAPSHOT });
    addGlobalRow(L"Instant fullscreen",                &ck.hkFull,   { MOD_SHIFT, VK_SNAPSHOT });
    addGlobalRow(L"Instant save previous custom area", &ck.hkPrev,   { MOD_CONTROL | MOD_SHIFT, VK_SNAPSHOT });

    addSep();

    // Custom area annotation tools
    addToolRow(OverlayTool::SelectMove,   L"Select mode",         &ck.hkToolSelect,    { 0, 'V' });
    addToolRow(OverlayTool::Pen,          L"Pen",                 &ck.hkToolPen,       { 0, 'P' });
    addToolRow(OverlayTool::Highlighter,  L"Stabilo highlighter", &ck.hkToolStabilo,   { 0, 'H' });
    addToolRow(OverlayTool::Line,         L"Line",                &ck.hkToolLine,      { 0, 'L' });
    addToolRow(OverlayTool::Arrow,        L"Arrow",               &ck.hkToolArrow,     { 0, 'A' });
    addToolRow(OverlayTool::NumberArrow,  L"Numbering arrow",     &ck.hkToolNumber,    { 0, 'N' });
    addActRow(DBTN_ACT_RESET_NUM,  false, L"Reset counter to 1",  &ck.hkToolResetNum,  { 0, 0 });
    addToolRow(OverlayTool::Rectangle,    L"Square / rectangle",  &ck.hkToolRect,      { 0, 'R' });
    addToolRow(OverlayTool::Ellipse,      L"Circle / ellipse",    &ck.hkToolEllipse,   { 0, 'E' });
    addToolRow(OverlayTool::TextBox,      L"Text box",            &ck.hkToolText,      { 0, 'T' });
    addToolRow(OverlayTool::MosaicSquare, L"Mosaic square",       &ck.hkToolMosaicSq,  { 0, 'X' });
    addToolRow(OverlayTool::MosaicCircle, L"Mosaic circle",       &ck.hkToolMosaicCir, { 0, 'M' });

    addSep();

    // Custom area toolbar actions, multi-frame editor & export
    addActRow(DBTN_ACT_UNDO,             false, L"Undo",                          &ck.hkActUndo,          { MOD_CONTROL, 'Z' });
    addActRow(DBTN_ACT_REDO,             false, L"Redo",                          &ck.hkActRedo,          { MOD_CONTROL, 'Y' });
    addActRow(DBTN_ACT_CLEAR,            false, L"Clear all annotations",         &ck.hkActClear,         { 0, 0 });
    addActRow(DBTN_ACT_OPTIONS,          false, L"Options",                       &ck.hkActOptions,       { 0, 0 });
    addActRow(DBTN_ACT_PIN,              false, L"Pin on top",                    &ck.hkActPin,           { 0, 'F' });
    addActRow(DBTN_ACT_OCR,              false, L"Extract text with OCR",         &ck.hkActOcr,           { 0, 'O' });
    addActRow(DBTN_ACT_SAVE_AS,          false, L"Save as JPG/PNG/WEBP/BMP",      &ck.hkActSaveAs,        { MOD_CONTROL | MOD_SHIFT, 'S' });
    addActRow(DBTN_ACT_SAVE,             false, L"Quick save",                    &ck.hkActSave,          { MOD_CONTROL, 'S' });
    addActRow(DBTN_ACT_COPY,             true,  L"Copy to clipboard",             &ck.hkActCopy,          { MOD_CONTROL, 'C' });
    addActRow(DBTN_ACT_FRAME_PLAY_PAUSE, false, L"Play / Pause frames (GIF/WebP)",&ck.hkFramePlayPause,   { 0, VK_SPACE });
    addActRow(DBTN_ACT_FRAME_TOGGLE,     false, L"Hide / Show frames (GIF/WebP)", &ck.hkFrameToggleStrip, { 0, 0 });

    addSep();

    // Pinned image toolbar actions & export
    addActRow(DBTN_ACT_ZOOM_OUT,         false, L"Zoom out",                      &ck.hkPinZoomOut,       { 0, VK_OEM_MINUS });
    addActRow(DBTN_ACT_ZOOM_IN,          false, L"Zoom in",                       &ck.hkPinZoomIn,        { 0, VK_OEM_PLUS });
    addActRow(DBTN_ACT_ZOOM_RESET,       false, L"Reset to original size",        &ck.hkPinZoomReset,     { 0, '0' });
    addActRow(DBTN_ACT_PIN_OUTLINE,      false, L"Show / Hide outline",           &ck.hkPinOutline,       { 0, 'O' });
    addActRow(DBTN_ACT_PIN_HIDE_TOOLBAR, false, L"Hide / Show toolbar",           &ck.hkPinHideToolbar,   { 0, 'H' });
    addActRow(DBTN_ACT_PIN_UNFILTER,     false, L"Smooth the image",              &ck.hkPinSmooth,        { 0, 'S' });
    addActRow(DBTN_ACT_SAVE_AS,          false, L"Save as JPG/PNG/WEBP/BMP",      &ck.hkPinSaveAs,        { 0, 0 });
    addActRow(DBTN_ACT_SAVE,             false, L"Quick save",                    &ck.hkPinSave,          { 0, 0 });
    addActRow(DBTN_ACT_COPY,             true,  L"Copy to clipboard",             &ck.hkPinCopy,          { MOD_CONTROL, 'C' });
    addActRow(DBTN_ACT_CLOSE,            false, L"Close pinned image",            &ck.hkPinClose,         { 0, VK_ESCAPE });

    int curY = 8;
    for (auto& r : ck.rows) {
        r.y = curY;
        r.h = r.isSeparator ? 14 : 32;
        curY += r.h;
    }
    ck.scrollContentHeight = curY + 8;
    ck.footerHeight = 50;

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    DWORD dwStyle = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VSCROLL | WS_VISIBLE | WS_CLIPCHILDREN;
    DWORD dwExStyle = WS_EX_TOPMOST;
    const int totalClientW = 428;
    const int totalClientH = ck.scrollContentHeight + ck.footerHeight;
    RECT rcWin = { 0, 0, totalClientW, totalClientH };
    AdjustWindowRectEx(&rcWin, dwStyle, FALSE, dwExStyle);
    int fixedW = (rcWin.right - rcWin.left) + GetSystemMetrics(SM_CXVSCROLL);
    int fullWinH = rcWin.bottom - rcWin.top;
    int maxAllowedH = std::min(fullWinH, std::max(320, sh - 24));
    int initH = std::max(280, std::min(maxAllowedH, g_Daemon.customizeKeysWindowHeight));
    ck.fixedWinWidth = fixedW;
    ck.fullWinHeight = fullWinH;
    ck.savedWinHeight = initH;

    if (hParentOptWnd && IsWindow(hParentOptWnd)) {
        EnableWindow(hParentOptWnd, FALSE);
    }

    HWND hDlg = CreateWindowExW(
        dwExStyle, L"PepperSnapCustomizeKeysModal",
        L"PepperSnap — customize keys",
        dwStyle,
        (sw - fixedW) / 2, std::max(10, (sh - initH) / 2), fixedW, initH,
        hParentOptWnd, nullptr, g_Daemon.hInst, &ck
    );
    g_Daemon.hCustomizeKeysWnd = hDlg;
    BringWindowToTop(hDlg);
    SetForegroundWindow(hDlg);

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (g_RecordingHotkeySlot != 0 && (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)) {
            DWORD vk = (DWORD)msg.wParam;
            if (vk == VK_SNAPSHOT) {
                continue;
            }
            if (vk == VK_ESCAPE) {
                SendMessageW(hDlg, WM_SHORTCUT_RECORDED, 0, VK_ESCAPE);
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
        if (msg.message == WM_MOUSEWHEEL && (msg.hwnd == hDlg || IsChild(hDlg, msg.hwnd))) {
            SendMessageW(hDlg, WM_MOUSEWHEEL, msg.wParam, msg.lParam);
            continue;
        }
        if (msg.hwnd == hDlg || IsChild(hDlg, msg.hwnd)) {
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) { SendMessageW(hDlg, WM_COMMAND, IDOK, 0); continue; }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) { SendMessageW(hDlg, WM_COMMAND, IDCANCEL, 0); continue; }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_RecordingHotkeySlot = 0;
    HidePinnedBubbleTooltip();
    g_Daemon.hCustomizeKeysWnd = nullptr;

    if (hParentOptWnd && IsWindow(hParentOptWnd)) {
        EnableWindow(hParentOptWnd, TRUE);
        BringWindowToTop(hParentOptWnd);
        SetForegroundWindow(hParentOptWnd);
        SetActiveWindow(hParentOptWnd);
    }

    if (ck.savedWinHeight >= 280 && ck.savedWinHeight != g_Daemon.customizeKeysWindowHeight) {
        g_Daemon.customizeKeysWindowHeight = ck.savedWinHeight;
        g_Daemon.SaveSettings();
    }

    if (ck.confirmed) {
        optSt->hkRegion        = ck.hkRegion;
        optSt->hkFull          = ck.hkFull;
        optSt->hkPrev          = ck.hkPrev;
        optSt->hkToolSelect    = ck.hkToolSelect;
        optSt->hkToolPen       = ck.hkToolPen;
        optSt->hkToolStabilo   = ck.hkToolStabilo;
        optSt->hkToolLine      = ck.hkToolLine;
        optSt->hkToolArrow     = ck.hkToolArrow;
        optSt->hkToolNumber    = ck.hkToolNumber;
        optSt->hkToolResetNum  = ck.hkToolResetNum;
        optSt->hkToolRect      = ck.hkToolRect;
        optSt->hkToolEllipse   = ck.hkToolEllipse;
        optSt->hkToolText      = ck.hkToolText;
        optSt->hkToolMosaicSq  = ck.hkToolMosaicSq;
        optSt->hkToolMosaicCir = ck.hkToolMosaicCir;
        optSt->hkActUndo          = ck.hkActUndo;
        optSt->hkActRedo          = ck.hkActRedo;
        optSt->hkActClear         = ck.hkActClear;
        optSt->hkActOptions       = ck.hkActOptions;
        optSt->hkActPin           = ck.hkActPin;
        optSt->hkActOcr           = ck.hkActOcr;
        optSt->hkActSaveAs        = ck.hkActSaveAs;
        optSt->hkActSave          = ck.hkActSave;
        optSt->hkActCopy          = ck.hkActCopy;
        optSt->hkFramePlayPause   = ck.hkFramePlayPause;
        optSt->hkFrameToggleStrip = ck.hkFrameToggleStrip;
        optSt->hkPinZoomOut       = ck.hkPinZoomOut;
        optSt->hkPinZoomIn        = ck.hkPinZoomIn;
        optSt->hkPinZoomReset     = ck.hkPinZoomReset;
        optSt->hkPinOutline       = ck.hkPinOutline;
        optSt->hkPinHideToolbar   = ck.hkPinHideToolbar;
        optSt->hkPinSmooth        = ck.hkPinSmooth;
        optSt->hkPinSaveAs        = ck.hkPinSaveAs;
        optSt->hkPinSave          = ck.hkPinSave;
        optSt->hkPinCopy          = ck.hkPinCopy;
        optSt->hkPinClose         = ck.hkPinClose;

        g_Daemon.hkRegionSnip       = ck.hkRegion;
        g_Daemon.hkFullSnap         = ck.hkFull;
        g_Daemon.hkPrevRegion       = ck.hkPrev;
        g_Daemon.hkToolSelect       = ck.hkToolSelect;
        g_Daemon.hkToolPen          = ck.hkToolPen;
        g_Daemon.hkToolStabilo      = ck.hkToolStabilo;
        g_Daemon.hkToolLine         = ck.hkToolLine;
        g_Daemon.hkToolArrow        = ck.hkToolArrow;
        g_Daemon.hkToolNumber       = ck.hkToolNumber;
        g_Daemon.hkToolResetNum     = ck.hkToolResetNum;
        g_Daemon.hkToolRect         = ck.hkToolRect;
        g_Daemon.hkToolEllipse      = ck.hkToolEllipse;
        g_Daemon.hkToolText         = ck.hkToolText;
        g_Daemon.hkToolMosaicSq     = ck.hkToolMosaicSq;
        g_Daemon.hkToolMosaicCir    = ck.hkToolMosaicCir;
        g_Daemon.hkActUndo          = ck.hkActUndo;
        g_Daemon.hkActRedo          = ck.hkActRedo;
        g_Daemon.hkActClear         = ck.hkActClear;
        g_Daemon.hkActOptions       = ck.hkActOptions;
        g_Daemon.hkActPin           = ck.hkActPin;
        g_Daemon.hkActOcr           = ck.hkActOcr;
        g_Daemon.hkActSaveAs        = ck.hkActSaveAs;
        g_Daemon.hkActSave          = ck.hkActSave;
        g_Daemon.hkActCopy          = ck.hkActCopy;
        g_Daemon.hkFramePlayPause   = ck.hkFramePlayPause;
        g_Daemon.hkFrameToggleStrip = ck.hkFrameToggleStrip;
        g_Daemon.hkPinZoomOut       = ck.hkPinZoomOut;
        g_Daemon.hkPinZoomIn      = ck.hkPinZoomIn;
        g_Daemon.hkPinZoomReset   = ck.hkPinZoomReset;
        g_Daemon.hkPinOutline     = ck.hkPinOutline;
        g_Daemon.hkPinHideToolbar = ck.hkPinHideToolbar;
        g_Daemon.hkPinSmooth      = ck.hkPinSmooth;
        g_Daemon.hkPinSaveAs     = ck.hkPinSaveAs;
        g_Daemon.hkPinSave       = ck.hkPinSave;
        g_Daemon.hkPinCopy       = ck.hkPinCopy;
        g_Daemon.hkPinClose      = ck.hkPinClose;

        g_Daemon.ApplyGlobalHotkeys();
        g_Daemon.SaveSettings();
        if (g_Daemon.hOverlayWnd && IsWindow(g_Daemon.hOverlayWnd)) {
            g_Daemon.BuildDockedHUD();
            InvalidateRect(g_Daemon.hOverlayWnd, nullptr, FALSE);
        }
        for (HWND hPin : g_Daemon.pinnedWindows) {
            if (hPin && IsWindow(hPin)) {
                PinnedWindowData* pData = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
                if (pData) {
                    UpdatePinnedWindowLayout(hPin, pData, false);
                    RenderPinnedLayeredWindow(hPin, pData);
                }
            }
        }
        std::wstring appDataDir = PepperSnapDaemon::GetAppDataSettingsDir();
        g_Daemon.ShowTrayToast(
            L"Custom shortcut keys saved",
            L"settings.ini is saved in " + appDataDir,
            appDataDir
        );
    }
}

static void LayoutOptionsFooter(HWND hWnd, OptionsDlgState* st) {
    if (!hWnd || !st) return;
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int clientW = std::max(1, (int)(rcClient.right - rcClient.left));
    int clientH = std::max(1, (int)(rcClient.bottom - rcClient.top));
    int footerTop = std::max(0, clientH - st->footerHeight);
    if (st->hFooterBg) {
        SetWindowPos(st->hFooterBg, HWND_TOP, 0, footerTop, clientW, st->footerHeight, SWP_NOACTIVATE);
    }
    if (st->hAppDataInfo) {
        SetWindowPos(st->hAppDataInfo, HWND_TOP, 18, footerTop + 18, 296, 24, SWP_NOACTIVATE);
    }
    if (st->hOkBtn) {
        SetWindowPos(st->hOkBtn, HWND_TOP, 318, footerTop + 11, 108, 34, SWP_NOACTIVATE);
    }
    if (st->hCancelBtn) {
        SetWindowPos(st->hCancelBtn, HWND_TOP, 434, footerTop + 11, 100, 34, SWP_NOACTIVATE);
    }
}

static void UpdateOptionsScroll(HWND hWnd, OptionsDlgState* st, int newScrollY) {
    if (!hWnd || !st) return;
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int clientW = std::max(1, (int)(rcClient.right - rcClient.left));
    int clientH = std::max(1, (int)(rcClient.bottom - rcClient.top));
    int viewportH = std::max(1, clientH - st->footerHeight);
    int maxScroll = std::max(0, st->scrollContentHeight - viewportH);
    int clamped = std::max(0, std::min(newScrollY, maxScroll));
    int dy = st->scrollY - clamped;
    st->scrollY = clamped;
    if (dy != 0) {
        for (HWND hChild = GetWindow(hWnd, GW_CHILD); hChild != nullptr; hChild = GetWindow(hChild, GW_HWNDNEXT)) {
            if (hChild == st->hFooterBg || hChild == st->hAppDataInfo || hChild == st->hOkBtn || hChild == st->hCancelBtn) continue;
            RECT rcCh;
            if (GetWindowRect(hChild, &rcCh)) {
                MapWindowPoints(HWND_DESKTOP, hWnd, (LPPOINT)&rcCh, 2);
                SetWindowPos(hChild, nullptr, rcCh.left, rcCh.top + dy, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
    }
    LayoutOptionsFooter(hWnd, st);
    SCROLLINFO si = { sizeof(SCROLLINFO) };
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    si.nMin = 0;
    si.nMax = st->scrollContentHeight - 1;
    si.nPage = (UINT)viewportH;
    si.nPos = st->scrollY;
    SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
    if (dy != 0) {
        RECT rcInval = { 0, 0, clientW, clientH };
        RedrawWindow(hWnd, &rcInval, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
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

static LRESULT CALLBACK AppDataLinkStaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WNDPROC origProc = (WNDPROC)GetPropW(hWnd, L"PepperSnapOrigStaticProc");
    switch (msg) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            SetPropW(hWnd, L"PepperSnapLinkPressed", (HANDLE)1);
            SetCapture(hWnd);
            return 0;
        case WM_LBUTTONUP: {
            bool wasPressed = (GetPropW(hWnd, L"PepperSnapLinkPressed") != nullptr);
            RemovePropW(hWnd, L"PepperSnapLinkPressed");
            if (GetCapture() == hWnd) {
                ReleaseCapture();
            }
            if (wasPressed) {
                POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                RECT rc;
                GetClientRect(hWnd, &rc);
                if (PtInRect(&rc, pt)) {
                    HWND hParent = GetParent(hWnd);
                    if (hParent) {
                        SendMessageW(hParent, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hWnd), STN_CLICKED), (LPARAM)hWnd);
                    }
                }
            }
            return 0;
        }
        case WM_NCDESTROY:
            RemovePropW(hWnd, L"PepperSnapLinkPressed");
            RemovePropW(hWnd, L"PepperSnapOrigStaticProc");
            break;
    }
    return origProc ? CallWindowProcW(origProc, hWnd, msg, wParam, lParam) : DefWindowProcW(hWnd, msg, wParam, lParam);
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

            // Timestamp Naming Pattern + Restore Default Button + Syntax Info (ⓘ) Button
            HWND hLblNaming = CreateWindowExW(0, L"STATIC",
                L"Naming pattern:",
                WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 18, 244, 516, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblNaming, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            st->hNamingEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", st->naming.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 18, 270, 352, 26, hWnd, (HMENU)IDC_OPT_NAMING_EDIT, nullptr, nullptr);
            SendMessageW(st->hNamingEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hRestoreNaming = CreateWindowExW(0, L"BUTTON", L"Restore default",
                WS_CHILD | WS_VISIBLE, 376, 269, 122, 28, hWnd, (HMENU)IDC_OPT_NAMING_RESTORE, nullptr, nullptr);
            SendMessageW(hRestoreNaming, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hNamingSyntax = CreateWindowExW(0, L"BUTTON", L"\x24D8",
                WS_CHILD | WS_VISIBLE, 504, 269, 30, 28, hWnd, (HMENU)IDC_OPT_NAMING_SYNTAX, nullptr, nullptr);
            SendMessageW(hNamingSyntax, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            st->hPreviewLbl = CreateWindowExW(0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 18, 302, 516, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(st->hPreviewLbl, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Shortcut keys: [Customize keys] button
            HWND hLblShortcuts = CreateWindowExW(0, L"STATIC",
                L"Shortcut keys:",
                WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 18, 338, 108, 24, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblShortcuts, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            st->hCustomizeKeysBtn = CreateWindowExW(0, L"BUTTON", L"Customize keys",
                WS_CHILD | WS_VISIBLE, 130, 334, 136, 28, hWnd, (HMENU)IDC_OPT_CUSTOMIZE_KEYS, nullptr, nullptr);
            SendMessageW(st->hCustomizeKeysBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Toggles at Bottom of Option Window (below Shortcut keys row):
            // 1. Check for updates (GitHub Releases)
            st->hAutoUpdateChk = CreateWindowExW(0, L"BUTTON",
                L"Automatically check for updates (GitHub releases):",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 376, 310, 24, hWnd, (HMENU)IDC_OPT_AUTOUPDATE_CHK, nullptr, nullptr);
            SendMessageW(st->hAutoUpdateChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hAutoUpdateChk, BM_SETCHECK, st->autoUpdateEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

            st->hComboUpdateInterval = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 332, 374, 98, 160, hWnd, (HMENU)IDC_OPT_UPDATE_INTERVAL, nullptr, nullptr);
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
                WS_CHILD | WS_VISIBLE, 436, 373, 98, 28, hWnd, (HMENU)IDC_OPT_CHECK_UPDATE_NOW, nullptr, nullptr);
            SendMessageW(st->hCheckUpdateNowBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

            // 2. Non-Stacking Highlighter
            st->hNonStackingChk = CreateWindowExW(0, L"BUTTON",
                L"Non-stacking highlighter (prevent overlapping highlighter strokes from darkening)",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 408, 516, 24, hWnd, (HMENU)IDC_OPT_NONSTACK_CHK, nullptr, nullptr);
            SendMessageW(st->hNonStackingChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hNonStackingChk, BM_SETCHECK, st->nonStackingHi ? BST_CHECKED : BST_UNCHECKED, 0);

            // 3. Also save screenshot when copying selected area
            st->hAutoSaveChk = CreateWindowExW(0, L"BUTTON",
                L"Also save screenshot when copying selected area",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 436, 516, 24, hWnd, (HMENU)IDC_OPT_AUTOSAVE_CHK, nullptr, nullptr);
            SendMessageW(st->hAutoSaveChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hAutoSaveChk, BM_SETCHECK, st->autoSaveCopy ? BST_CHECKED : BST_UNCHECKED, 0);

            // 4. Also copy instant fullscreen
            st->hCopyFullChk = CreateWindowExW(0, L"BUTTON",
                L"Also copy instant fullscreen",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 464, 516, 24, hWnd, (HMENU)IDC_OPT_NOCOPY_SAVE_CHK, nullptr, nullptr);
            SendMessageW(st->hCopyFullChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hCopyFullChk, BM_SETCHECK, st->alsoCopyFull ? BST_CHECKED : BST_UNCHECKED, 0);

            // 5. Also save pinned screenshot
            st->hSavePinChk = CreateWindowExW(0, L"BUTTON",
                L"Also save pinned screenshot",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 492, 516, 24, hWnd, (HMENU)IDC_OPT_SAVE_PIN_CHK, nullptr, nullptr);
            SendMessageW(st->hSavePinChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hSavePinChk, BM_SETCHECK, st->alsoSavePin ? BST_CHECKED : BST_UNCHECKED, 0);

            // 6. Also save copied text
            st->hSaveOcrChk = CreateWindowExW(0, L"BUTTON",
                L"Also save copied text",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 520, 516, 24, hWnd, (HMENU)IDC_OPT_SAVE_OCR_CHK, nullptr, nullptr);
            SendMessageW(st->hSaveOcrChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hSaveOcrChk, BM_SETCHECK, st->alsoSaveOcr ? BST_CHECKED : BST_UNCHECKED, 0);

            // 7. Keep cursor on screenshot
            st->hKeepCursorChk = CreateWindowExW(0, L"BUTTON",
                L"Keep cursor on screenshot",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 548, 516, 24, hWnd, (HMENU)IDC_OPT_KEEP_CURSOR_CHK, nullptr, nullptr);
            SendMessageW(st->hKeepCursorChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hKeepCursorChk, BM_SETCHECK, st->keepCursor ? BST_CHECKED : BST_UNCHECKED, 0);

            // 8. Auto-hide frame list
            st->hAutoHideFramesChk = CreateWindowExW(0, L"BUTTON",
                L"Auto-hide frame list",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 576, 516, 24, hWnd, (HMENU)IDC_OPT_AUTOHIDE_FRM_CHK, nullptr, nullptr);
            SendMessageW(st->hAutoHideFramesChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hAutoHideFramesChk, BM_SETCHECK, st->autoHideFrames ? BST_CHECKED : BST_UNCHECKED, 0);

            // 9. Enable pen and highlighter smoothing.
            st->hPenSmoothChk = CreateWindowExW(0, L"BUTTON",
                L"Enable pen and highlighter smoothing.",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 604, 516, 24, hWnd, (HMENU)IDC_OPT_PENSMOOTH_CHK, nullptr, nullptr);
            SendMessageW(st->hPenSmoothChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hPenSmoothChk, BM_SETCHECK, st->penSmoothEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

            std::wstring psText = FormatPenSmoothLabel(st->penSmoothEnabled, st->penSmoothStrength);
            st->hPenSmoothValLbl = CreateWindowExW(0, L"STATIC", psText.c_str(),
                WS_CHILD | WS_VISIBLE, 18, 636, 224, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(st->hPenSmoothValLbl, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hPenSmoothSlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS, 244, 632, 290, 32, hWnd, (HMENU)IDC_OPT_PENSMOOTH_SLIDER, nullptr, nullptr);
            SendMessageW(st->hPenSmoothSlider, TBM_SETRANGE, TRUE, MAKELONG(5, 100));
            SendMessageW(st->hPenSmoothSlider, TBM_SETTICFREQ, 10, 0);
            SendMessageW(st->hPenSmoothSlider, TBM_SETPOS, TRUE, st->penSmoothStrength);
            EnableWindow(st->hPenSmoothSlider, st->penSmoothEnabled ? TRUE : FALSE);

            // 10. Restore everything to default button (below smoothing strength, NOT in row with Save options)
            st->hRestoreAllBtn = CreateWindowExW(0, L"BUTTON", L"Restore everything to default",
                WS_CHILD | WS_VISIBLE, 18, 672, 218, 30, hWnd, (HMENU)IDC_OPT_RESTORE_ALL_DEF, nullptr, nullptr);
            SendMessageW(st->hRestoreAllBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Permanent pinned footer bar: clickable %appdata%\PepperSnap persistence link + Save options + Cancel
            st->hFooterBg = CreateWindowExW(0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 716, 552, 56, hWnd, nullptr, nullptr, nullptr);

            HFONT hLinkFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, TRUE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            st->hAppDataInfo = CreateWindowExW(0, L"STATIC",
                L"\x24D8 settings.ini is saved in %appdata%\\PepperSnap",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_NOPREFIX | SS_LEFTNOWORDWRAP | SS_NOTIFY, 18, 734, 296, 24, hWnd, (HMENU)IDC_OPT_OPEN_APPDATA, nullptr, nullptr);
            SendMessageW(st->hAppDataInfo, WM_SETFONT, (WPARAM)hLinkFont, TRUE);
            WNDPROC origStaticProc = (WNDPROC)SetWindowLongPtrW(st->hAppDataInfo, GWLP_WNDPROC, (LONG_PTR)AppDataLinkStaticWndProc);
            SetPropW(st->hAppDataInfo, L"PepperSnapOrigStaticProc", (HANDLE)origStaticProc);

            st->hOkBtn = CreateWindowExW(0, L"BUTTON", L"Save options",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_DEFPUSHBUTTON, 318, 727, 108, 34, hWnd, (HMENU)IDOK, nullptr, nullptr);
            SendMessageW(st->hOkBtn, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            st->hCancelBtn = CreateWindowExW(0, L"BUTTON", L"Cancel",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 434, 727, 100, 34, hWnd, (HMENU)IDCANCEL, nullptr, nullptr);
            SendMessageW(st->hCancelBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

            for (HWND hCh = GetWindow(hWnd, GW_CHILD); hCh != nullptr; hCh = GetWindow(hCh, GW_HWNDNEXT)) {
                LONG stBits = GetWindowLongW(hCh, GWL_STYLE);
                SetWindowLongW(hCh, GWL_STYLE, stBits | WS_CLIPSIBLINGS);
            }

            st->scrollContentHeight = 716;
            st->footerHeight = 56;
            UpdateOptionsPreviewLabel(st);
            LayoutOptionsFooter(hWnd, st);
            UpdateOptionsScroll(hWnd, st, 0);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            if (st) {
                RECT rcClient;
                GetClientRect(hWnd, &rcClient);
                int clientW = rcClient.right - rcClient.left;
                int clientH = rcClient.bottom - rcClient.top;
                int footerTop = std::max(0, clientH - st->footerHeight);
                RECT rcFooter = { 0, footerTop, clientW, clientH };
                FillRect(hdc, &rcFooter, GetSysColorBrush(COLOR_BTNFACE));
                HPEN hLinePen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
                HGDIOBJ hOldPen = SelectObject(hdc, hLinePen);
                MoveToEx(hdc, 0, footerTop, nullptr);
                LineTo(hdc, clientW, footerTop);
                SelectObject(hdc, hOldPen);
                DeleteObject(hLinePen);
            }
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            if (st && st->fixedWinWidth > 0) {
                mmi->ptMinTrackSize.x = st->fixedWinWidth;
                mmi->ptMaxTrackSize.x = st->fixedWinWidth;
                mmi->ptMinTrackSize.y = 320;
                int maxH = std::min(st->fullWinHeight, std::max(360, GetSystemMetrics(SM_CYSCREEN) - 24));
                mmi->ptMaxTrackSize.y = std::max(320, maxH);
            }
            return 0;
        }
        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProcW(hWnd, msg, wParam, lParam);
            if (hit == HTLEFT || hit == HTRIGHT) return HTBORDER;
            if (hit == HTTOPLEFT || hit == HTTOPRIGHT) return HTTOP;
            if (hit == HTBOTTOMLEFT || hit == HTBOTTOMRIGHT) return HTBOTTOM;
            return hit;
        }
        case WM_SIZE: {
            if (st && wParam != SIZE_MINIMIZED) {
                LayoutOptionsFooter(hWnd, st);
                UpdateOptionsScroll(hWnd, st, st->scrollY);
                InvalidateRect(hWnd, nullptr, TRUE);
                RECT rcW;
                if (GetWindowRect(hWnd, &rcW)) {
                    st->savedWinHeight = rcW.bottom - rcW.top;
                }
            }
            return 0;
        }
        case WM_VSCROLL: {
            if (st && lParam == 0) {
                RECT rcC;
                GetClientRect(hWnd, &rcC);
                int viewportH = std::max(60, (int)(rcC.bottom - rcC.top) - st->footerHeight);
                int pageH = std::max(40, viewportH - 36);
                int target = st->scrollY;
                switch (LOWORD(wParam)) {
                    case SB_LINEUP:        target -= 36; break;
                    case SB_LINEDOWN:      target += 36; break;
                    case SB_PAGEUP:        target -= pageH; break;
                    case SB_PAGEDOWN:      target += pageH; break;
                    case SB_TOP:           target = 0; break;
                    case SB_BOTTOM:        target = st->scrollContentHeight; break;
                    case SB_THUMBTRACK:
                    case SB_THUMBPOSITION: {
                        SCROLLINFO si = { sizeof(SCROLLINFO), SIF_TRACKPOS };
                        if (GetScrollInfo(hWnd, SB_VERT, &si)) {
                            target = si.nTrackPos;
                        } else {
                            target = HIWORD(wParam);
                        }
                        break;
                    }
                    default: break;
                }
                UpdateOptionsScroll(hWnd, st, target);
                return 0;
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            if (st) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (delta != 0) {
                    int step = -(delta * 48) / WHEEL_DELTA;
                    UpdateOptionsScroll(hWnd, st, st->scrollY + step);
                }
                return 0;
            }
            break;
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
            if (st && st->hFooterBg && (HWND)lParam == st->hFooterBg) {
                HDC hdcStatic = (HDC)wParam;
                RECT rcF;
                GetClientRect(st->hFooterBg, &rcF);
                FillRect(hdcStatic, &rcF, GetSysColorBrush(COLOR_BTNFACE));
                HPEN hLinePen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
                HGDIOBJ hOldPen = SelectObject(hdcStatic, hLinePen);
                MoveToEx(hdcStatic, 0, 0, nullptr);
                LineTo(hdcStatic, rcF.right, 0);
                SelectObject(hdcStatic, hOldPen);
                DeleteObject(hLinePen);
                return (LRESULT)GetStockObject(NULL_BRUSH);
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
            if (id == IDC_OPT_CUSTOMIZE_KEYS) {
                ShowCustomizeKeysModal(hWnd, st);
                return 0;
            }
            if (id == IDC_OPT_RESTORE_ALL_DEF) {
                int ans = MessageBoxW(
                    hWnd,
                    L"Are you sure you want to restore all options and shortcut keys to default?",
                    L"PepperSnap — restore everything to default",
                    MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 | MB_TOPMOST | MB_SETFOREGROUND
                );
                if (ans != IDYES) {
                    return 0;
                }
                st->restoredAllDefaults = true;
                st->folder = PepperSnapDaemon::GetDefaultDesktopFolder();
                SetWindowTextW(st->hFolderEdit, st->folder.c_str());

                st->regionFmt = ImageFormat::JPEG;
                st->fullFmt   = ImageFormat::JPEG;
                st->copyFmt   = ImageFormat::JPEG;
                SendMessageW(st->hComboRegion, CB_SETCURSEL, (WPARAM)st->regionFmt, 0);
                SendMessageW(st->hComboFull,   CB_SETCURSEL, (WPARAM)st->fullFmt, 0);
                SendMessageW(st->hComboCopy,   CB_SETCURSEL, (WPARAM)st->copyFmt, 0);

                st->jpgQuality = 90;
                SendMessageW(st->hQualitySlider, TBM_SETPOS, TRUE, st->jpgQuality);
                SetWindowTextW(st->hQualityValLbl, L"JPEG quality: 90%");

                st->naming = PepperSnapDaemon::DEFAULT_NAMING_PATTERN;
                SetWindowTextW(st->hNamingEdit, st->naming.c_str());
                UpdateOptionsPreviewLabel(st);

                st->hkRegion        = { MOD_CONTROL, VK_SNAPSHOT };
                st->hkFull          = { MOD_SHIFT, VK_SNAPSHOT };
                st->hkPrev          = { MOD_CONTROL | MOD_SHIFT, VK_SNAPSHOT };
                st->hkToolSelect    = { 0, 'V' };
                st->hkToolPen       = { 0, 'P' };
                st->hkToolStabilo   = { 0, 'H' };
                st->hkToolLine      = { 0, 'L' };
                st->hkToolArrow     = { 0, 'A' };
                st->hkToolNumber    = { 0, 'N' };
                st->hkToolResetNum  = { 0, 0 };
                st->hkToolRect      = { 0, 'R' };
                st->hkToolEllipse   = { 0, 'E' };
                st->hkToolText      = { 0, 'T' };
                st->hkToolMosaicSq  = { 0, 'X' };
                st->hkToolMosaicCir = { 0, 'M' };
                st->hkActUndo          = { MOD_CONTROL, 'Z' };
                st->hkActRedo          = { MOD_CONTROL, 'Y' };
                st->hkActClear         = { 0, 0 };
                st->hkActOptions       = { 0, 0 };
                st->hkActPin           = { 0, 'F' };
                st->hkActOcr           = { 0, 'O' };
                st->hkActSaveAs        = { MOD_CONTROL | MOD_SHIFT, 'S' };
                st->hkActSave          = { MOD_CONTROL, 'S' };
                st->hkActCopy          = { MOD_CONTROL, 'C' };
                st->hkFramePlayPause   = { 0, VK_SPACE };
                st->hkFrameToggleStrip = { 0, 0 };
                st->hkPinZoomOut       = { 0, VK_OEM_MINUS };
                st->hkPinZoomIn     = { 0, VK_OEM_PLUS };
                st->hkPinZoomReset   = { 0, '0' };
                st->hkPinOutline     = { 0, 'O' };
                st->hkPinHideToolbar = { 0, 'H' };
                st->hkPinSmooth      = { 0, 'S' };
                st->hkPinSaveAs     = { 0, 0 };
                st->hkPinSave       = { 0, 0 };
                st->hkPinCopy       = { MOD_CONTROL, 'C' };
                st->hkPinClose      = { 0, VK_ESCAPE };

                st->autoUpdateEnabled = true;
                st->updateInterval = UpdateCheckInterval::EveryDay;
                SendMessageW(st->hAutoUpdateChk, BM_SETCHECK, BST_CHECKED, 0);
                SendMessageW(st->hComboUpdateInterval, CB_SETCURSEL, (WPARAM)st->updateInterval, 0);
                EnableWindow(st->hComboUpdateInterval, TRUE);

                st->nonStackingHi = true;
                st->autoSaveCopy  = true;
                st->alsoCopyFull  = true;
                st->alsoSavePin   = true;
                st->alsoSaveOcr   = true;
                st->keepCursor    = false;
                st->autoHideFrames = true;
                SendMessageW(st->hNonStackingChk,    BM_SETCHECK, BST_CHECKED, 0);
                SendMessageW(st->hAutoSaveChk,       BM_SETCHECK, BST_CHECKED, 0);
                SendMessageW(st->hCopyFullChk,       BM_SETCHECK, BST_CHECKED, 0);
                SendMessageW(st->hSavePinChk,        BM_SETCHECK, BST_CHECKED, 0);
                SendMessageW(st->hSaveOcrChk,        BM_SETCHECK, BST_CHECKED, 0);
                SendMessageW(st->hKeepCursorChk,     BM_SETCHECK, BST_UNCHECKED, 0);
                SendMessageW(st->hAutoHideFramesChk, BM_SETCHECK, BST_CHECKED, 0);

                st->penSmoothEnabled = true;
                st->penSmoothStrength = 15;
                SendMessageW(st->hPenSmoothChk, BM_SETCHECK, BST_CHECKED, 0);
                EnableWindow(st->hPenSmoothSlider, TRUE);
                SendMessageW(st->hPenSmoothSlider, TBM_SETPOS, TRUE, st->penSmoothStrength);
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
                int ans = MessageBoxW(
                    hWnd,
                    L"Are you sure you want to restore the naming pattern to default?",
                    L"PepperSnap — restore default naming pattern",
                    MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 | MB_TOPMOST | MB_SETFOREGROUND
                );
                if (ans != IDYES) {
                    return 0;
                }
                SetWindowTextW(st->hNamingEdit, PepperSnapDaemon::DEFAULT_NAMING_PATTERN);
                UpdateOptionsPreviewLabel(st);
                return 0;
            }
            if (id == IDC_OPT_NAMING_SYNTAX) {
                ShowNamingSyntaxModal(hWnd);
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
                bool pickedModern = false;
                IFileOpenDialog* pFileOpen = nullptr;
                HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileOpenDialog, (void**)&pFileOpen);
                if (SUCCEEDED(hr) && pFileOpen) {
                    DWORD dwOptions = 0;
                    if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions))) {
                        pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
                    }
                    pFileOpen->SetTitle(L"Select default screenshot save folder");
                    WCHAR curFolder[MAX_PATH] = {0};
                    GetWindowTextW(st->hFolderEdit, curFolder, MAX_PATH - 1);
                    if (wcslen(curFolder) > 0) {
                        IShellItem* psiFolder = nullptr;
                        if (SUCCEEDED(SHCreateItemFromParsingName(curFolder, nullptr, IID_IShellItem, (void**)&psiFolder)) && psiFolder) {
                            pFileOpen->SetFolder(psiFolder);
                            psiFolder->Release();
                        }
                    }
                    pickedModern = true;
                    if (SUCCEEDED(pFileOpen->Show(hWnd))) {
                        IShellItem* pItem = nullptr;
                        if (SUCCEEDED(pFileOpen->GetResult(&pItem)) && pItem) {
                            PWSTR pszFilePath = nullptr;
                            if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)) && pszFilePath) {
                                SetWindowTextW(st->hFolderEdit, pszFilePath);
                                CoTaskMemFree(pszFilePath);
                            }
                            pItem->Release();
                        }
                    }
                    pFileOpen->Release();
                }
                if (!pickedModern) {
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
                st->keepCursor    = (SendMessageW(st->hKeepCursorChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->autoHideFrames = (SendMessageW(st->hAutoHideFramesChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
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
    st.keepCursor   = keepCursorOnScreenshot;
    st.autoHideFrames = autoHideFrameList;
    st.nonStackingHi = nonStackingHighlighter;
    st.penSmoothEnabled = penSmoothingEnabled;
    st.penSmoothStrength = penSmoothingStrength;
    st.autoUpdateEnabled = autoCheckUpdates;
    st.updateInterval = updateInterval;
    st.hkRegion        = hkRegionSnip;
    st.hkFull          = hkFullSnap;
    st.hkPrev          = hkPrevRegion;
    st.hkToolSelect    = hkToolSelect;
    st.hkToolPen       = hkToolPen;
    st.hkToolStabilo   = hkToolStabilo;
    st.hkToolLine      = hkToolLine;
    st.hkToolArrow     = hkToolArrow;
    st.hkToolNumber    = hkToolNumber;
    st.hkToolResetNum  = hkToolResetNum;
    st.hkToolRect      = hkToolRect;
    st.hkToolEllipse   = hkToolEllipse;
    st.hkToolText      = hkToolText;
    st.hkToolMosaicSq  = hkToolMosaicSq;
    st.hkToolMosaicCir = hkToolMosaicCir;
    st.hkActUndo          = hkActUndo;
    st.hkActRedo          = hkActRedo;
    st.hkActClear         = hkActClear;
    st.hkActOptions       = hkActOptions;
    st.hkActPin           = hkActPin;
    st.hkActOcr           = hkActOcr;
    st.hkActSaveAs        = hkActSaveAs;
    st.hkActSave          = hkActSave;
    st.hkActCopy          = hkActCopy;
    st.hkFramePlayPause   = hkFramePlayPause;
    st.hkFrameToggleStrip = hkFrameToggleStrip;
    st.hkPinZoomOut       = hkPinZoomOut;
    st.hkPinZoomIn     = hkPinZoomIn;
    st.hkPinZoomReset   = hkPinZoomReset;
    st.hkPinOutline     = hkPinOutline;
    st.hkPinHideToolbar = hkPinHideToolbar;
    st.hkPinSmooth      = hkPinSmooth;
    st.hkPinSaveAs     = hkPinSaveAs;
    st.hkPinSave       = hkPinSave;
    st.hkPinCopy       = hkPinCopy;
    st.hkPinClose      = hkPinClose;

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    DWORD dwOptStyle = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VSCROLL | WS_VISIBLE | WS_CLIPCHILDREN;
    DWORD dwOptExStyle = WS_EX_TOPMOST;
    const int totalOptClientH = 716 + 56;
    RECT rcOptFull = { 0, 0, 552, totalOptClientH };
    AdjustWindowRectEx(&rcOptFull, dwOptStyle, FALSE, dwOptExStyle);
    int fixedW = (rcOptFull.right - rcOptFull.left) + GetSystemMetrics(SM_CXVSCROLL);
    int fullWinH = rcOptFull.bottom - rcOptFull.top;
    int maxAllowedH = std::min(fullWinH, std::max(360, sh - 24));
    int initH = std::max(320, std::min(maxAllowedH, optionsWindowHeight));
    st.fixedWinWidth = fixedW;
    st.fullWinHeight = fullWinH;
    st.savedWinHeight = initH;

    HWND hParent = hTrayWnd;
    std::wstring optTitle = L"PepperSnap v" + std::wstring(APP_VERSION) + L" — options";
    HWND hDlg = CreateWindowExW(
        dwOptExStyle, L"PepperSnapOptionsModal",
        optTitle.c_str(),
        dwOptStyle,
        (sw - fixedW) / 2, std::max(10, (sh - initH) / 2), fixedW, initH, hParent, nullptr, hInst, &st
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
        if (msg.message == WM_MOUSEWHEEL && (msg.hwnd == hDlg || IsChild(hDlg, msg.hwnd))) {
            bool comboDropped = (
                (msg.hwnd == st.hComboRegion || msg.hwnd == st.hComboFull ||
                 msg.hwnd == st.hComboCopy   || msg.hwnd == st.hComboUpdateInterval) &&
                SendMessageW(msg.hwnd, CB_GETDROPPEDSTATE, 0, 0) != 0
            );
            if (!comboDropped) {
                SendMessageW(hDlg, WM_MOUSEWHEEL, msg.wParam, msg.lParam);
                continue;
            }
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
    if (st.savedWinHeight >= 320 && st.savedWinHeight != optionsWindowHeight) {
        optionsWindowHeight = st.savedWinHeight;
        SaveSettings();
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
        alsoCopyFullscreen = st.alsoCopyFull;
        alsoSavePinned = st.alsoSavePin;
        alsoSaveOcrText = st.alsoSaveOcr;
        keepCursorOnScreenshot = st.keepCursor;
        autoHideFrameList = st.autoHideFrames;
        nonStackingHighlighter = st.nonStackingHi;
        penSmoothingEnabled = st.penSmoothEnabled;
        penSmoothingStrength = st.penSmoothStrength;
        autoCheckUpdates = st.autoUpdateEnabled;
        updateInterval = st.updateInterval;
        hkRegionSnip    = st.hkRegion;
        hkFullSnap      = st.hkFull;
        hkPrevRegion    = st.hkPrev;
        hkToolSelect    = st.hkToolSelect;
        hkToolPen       = st.hkToolPen;
        hkToolStabilo   = st.hkToolStabilo;
        hkToolLine      = st.hkToolLine;
        hkToolArrow     = st.hkToolArrow;
        hkToolNumber    = st.hkToolNumber;
        hkToolResetNum  = st.hkToolResetNum;
        hkToolRect      = st.hkToolRect;
        hkToolEllipse   = st.hkToolEllipse;
        hkToolText      = st.hkToolText;
        hkToolMosaicSq  = st.hkToolMosaicSq;
        hkToolMosaicCir = st.hkToolMosaicCir;
        hkActUndo          = st.hkActUndo;
        hkActRedo          = st.hkActRedo;
        hkActClear         = st.hkActClear;
        hkActOptions       = st.hkActOptions;
        hkActPin           = st.hkActPin;
        hkActOcr           = st.hkActOcr;
        hkActSaveAs        = st.hkActSaveAs;
        hkActSave          = st.hkActSave;
        hkActCopy          = st.hkActCopy;
        hkFramePlayPause   = st.hkFramePlayPause;
        hkFrameToggleStrip = st.hkFrameToggleStrip;
        hkPinZoomOut       = st.hkPinZoomOut;
        hkPinZoomIn     = st.hkPinZoomIn;
        hkPinZoomReset   = st.hkPinZoomReset;
        hkPinOutline     = st.hkPinOutline;
        hkPinHideToolbar = st.hkPinHideToolbar;
        hkPinSmooth      = st.hkPinSmooth;
        hkPinSaveAs      = st.hkPinSaveAs;
        hkPinSave        = st.hkPinSave;
        hkPinCopy        = st.hkPinCopy;
        hkPinClose       = st.hkPinClose;
        if (st.restoredAllDefaults) {
            hidePinnedToolbar = false;
            hidePinnedOutline = false;
            smoothPinnedImage = false;
            activeColor = Color(255, 239, 68, 68);
            activeStroke = 4.0f;
        }
        ApplyGlobalHotkeys();
        CreateDirectoryW(saveFolder.c_str(), nullptr);
        SaveSettings();
        if (hOverlayWnd) {
            BuildDockedHUD();
            InvalidateRect(hOverlayWnd, nullptr, FALSE);
        }
        for (HWND hPin : pinnedWindows) {
            if (hPin && IsWindow(hPin)) {
                PinnedWindowData* pData = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
                if (pData) {
                    if (st.restoredAllDefaults) {
                        pData->hideToolbar = false;
                        pData->hideOutline = false;
                        pData->smoothImage = false;
                        int imgW = 32, imgH = 32;
                        GetPinnedScaledDims(pData, imgW, imgH);
                        EnsurePinnedWindowCache(pData, imgW, imgH, true);
                    }
                    UpdatePinnedWindowLayout(hPin, pData, st.restoredAllDefaults);
                    RenderPinnedLayeredWindow(hPin, pData);
                }
            }
        }
        if (hShortcutsWnd && IsWindow(hShortcutsWnd)) {
            DestroyWindow(hShortcutsWnd);
            hShortcutsWnd = nullptr;
        }
        std::wstring appDataDir = PepperSnapDaemon::GetAppDataSettingsDir();
        ShowTrayToast(
            L"PepperSnap options saved",
            L"settings.ini is saved in " + appDataDir,
            appDataDir
        );
    }
}

struct ShortcutEntry {
    bool isSectionHeader = false;
    bool hasIcon = false;
    DockButton iconBtn;
    std::wstring keyCol;
    std::wstring descCol;
    int y = 0;
    int h = 28;
    int textH = 20;
};

struct ShortcutsDlgParams {
    std::wstring mainTitle;
    std::vector<ShortcutEntry> entries;
    int leftColX = 22;
    int leftColW = 240;
    int rightColX = 276;
    int rightColW = 480;
    int scrollY = 0;
    int scrollContentHeight = 580;
    int footerHeight = 52;
    int minWinWidth = 740;
    int maxWinWidth = 1080;
    int fullWinHeight = 680;
    int savedWinWidth = 880;
    int savedWinHeight = 580;
    HWND hFooterBg = nullptr;
    HWND hOkBtn = nullptr;
};

static void RecalcShortcutsLayout(HWND hWnd, ShortcutsDlgParams* p, int clientW) {
    if (!p) return;
    int padX = 18;
    int rightEdge = std::max(p->rightColX + 240, clientW - padX);
    int availDescW = std::max(240, rightEdge - p->rightColX);
    p->rightColW = availDescW;

    HDC hdc = hWnd ? GetDC(hWnd) : GetDC(nullptr);
    HFONT hDescFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HGDIOBJ hOldF = hdc ? SelectObject(hdc, hDescFont) : nullptr;

    int curY = 48;
    for (size_t i = 0; i < p->entries.size(); ++i) {
        auto& e = p->entries[i];
        e.y = curY;
        if (e.isSectionHeader) {
            e.h = (i == 0) ? 28 : 38;
            e.textH = 20;
        } else {
            int measuredH = 20;
            if (hdc && !e.descCol.empty()) {
                RECT rcD = { 0, 0, availDescW, 0 };
                DrawTextW(hdc, e.descCol.c_str(), -1, &rcD, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
                measuredH = std::max(18, (int)(rcD.bottom - rcD.top));
            }
            e.textH = measuredH;
            e.h = std::max(28, measuredH + 10);
        }
        curY += e.h;
    }

    if (hdc) {
        SelectObject(hdc, hOldF);
        ReleaseDC(hWnd, hdc);
    }
    DeleteObject(hDescFont);

    p->scrollContentHeight = curY + 12;

    DWORD dwStyle = hWnd ? (DWORD)GetWindowLongPtrW(hWnd, GWL_STYLE)
                         : (WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VSCROLL | WS_VISIBLE | WS_CLIPCHILDREN);
    DWORD dwExStyle = hWnd ? (DWORD)GetWindowLongPtrW(hWnd, GWL_EXSTYLE) : WS_EX_TOPMOST;
    RECT rcFull = { 0, 0, std::max(320, clientW), p->scrollContentHeight + p->footerHeight };
    AdjustWindowRectEx(&rcFull, dwStyle, FALSE, dwExStyle);
    p->fullWinHeight = rcFull.bottom - rcFull.top;
}

static void LayoutShortcutsFooter(HWND hWnd, ShortcutsDlgParams* p) {
    if (!hWnd || !p) return;
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int clientW = std::max(1, (int)(rcClient.right - rcClient.left));
    int clientH = std::max(1, (int)(rcClient.bottom - rcClient.top));
    int footerTop = std::max(0, clientH - p->footerHeight);
    if (p->hOkBtn) {
        int btnX = std::max(16, clientW - 18 - 92);
        SetWindowPos(p->hOkBtn, HWND_TOP, btnX, footerTop + 11, 92, 30, SWP_NOACTIVATE);
    }
}

static void UpdateShortcutsScroll(HWND hWnd, ShortcutsDlgParams* p, int newScrollY) {
    if (!hWnd || !p) return;
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int clientW = std::max(1, (int)(rcClient.right - rcClient.left));
    int clientH = std::max(1, (int)(rcClient.bottom - rcClient.top));
    int viewportH = std::max(1, clientH - p->footerHeight);
    int maxScroll = std::max(0, p->scrollContentHeight - viewportH);
    int clamped = std::max(0, std::min(newScrollY, maxScroll));
    int dy = p->scrollY - clamped;
    p->scrollY = clamped;
    LayoutShortcutsFooter(hWnd, p);
    SCROLLINFO si = { sizeof(SCROLLINFO) };
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    si.nMin = 0;
    si.nMax = p->scrollContentHeight - 1;
    si.nPage = (UINT)viewportH;
    si.nPos = p->scrollY;
    SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
    if (dy != 0) {
        RECT rcInval = { 0, 0, clientW, clientH };
        RedrawWindow(hWnd, &rcInval, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    }
}

static LRESULT CALLBACK ShortcutsDlgWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ShortcutsDlgParams* p = (ShortcutsDlgParams*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            p = (ShortcutsDlgParams*)cs->lpCreateParams;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)p);

            HFONT hBoldFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

            HWND hOk = CreateWindowExW(0, L"BUTTON", L"OK",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_DEFPUSHBUTTON, 400, 500, 92, 30, hWnd, (HMENU)IDOK, nullptr, nullptr);
            SendMessageW(hOk, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            if (p) {
                p->hOkBtn = hOk;
                RECT rcC;
                GetClientRect(hWnd, &rcC);
                int clientW = std::max(1, (int)(rcC.right - rcC.left));
                RecalcShortcutsLayout(hWnd, p, clientW);
                LayoutShortcutsFooter(hWnd, p);
                UpdateShortcutsScroll(hWnd, p, 0);
            }
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            int clientW = std::max(1, (int)(rcClient.right - rcClient.left));
            int clientH = std::max(1, (int)(rcClient.bottom - rcClient.top));
            if (p) {
                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP memBmp = CreateCompatibleBitmap(hdc, clientW, clientH);
                HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

                RECT rcAll = { 0, 0, clientW, clientH };
                FillRect(memDC, &rcAll, GetSysColorBrush(COLOR_BTNFACE));

                int footerTop = std::max(0, clientH - p->footerHeight);
                int savedDC = SaveDC(memDC);
                IntersectClipRect(memDC, 0, 0, clientW, footerTop);

                SetBkMode(memDC, TRANSPARENT);
                HFONT hTitleFont = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                HFONT hSectionFont = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                HFONT hKeyFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                HFONT hDescFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

                HPEN hSepShadow = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
                HPEN hSepHilight = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DHILIGHT));
                HPEN hRowSep = CreatePen(PS_SOLID, 1, RGB(218, 222, 228));

                int sy = p->scrollY;
                int padX = 18;
                int rightEdge = std::max(p->rightColX + 240, clientW - padX);

                // Main header title + horizontal separator line
                HGDIOBJ hOldF = SelectObject(memDC, hTitleFont);
                SetTextColor(memDC, GetSysColor(COLOR_WINDOWTEXT));
                RECT rcTitle = { padX, 14 - sy, rightEdge, 38 - sy };
                DrawTextW(memDC, p->mainTitle.c_str(), -1, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                HGDIOBJ hOldP = SelectObject(memDC, hSepShadow);
                MoveToEx(memDC, padX, 42 - sy, nullptr);
                LineTo(memDC, rightEdge, 42 - sy);
                SelectObject(memDC, hSepHilight);
                MoveToEx(memDC, padX, 43 - sy, nullptr);
                LineTo(memDC, rightEdge, 43 - sy);

                for (size_t i = 0; i < p->entries.size(); ++i) {
                    const auto& e = p->entries[i];
                    int drawY = e.y - sy;
                    if (drawY + e.h < 0 || drawY > footerTop) continue;

                    if (e.isSectionHeader) {
                        if (i > 0) {
                            int lineY = drawY + 6;
                            SelectObject(memDC, hSepShadow);
                            MoveToEx(memDC, padX, lineY, nullptr);
                            LineTo(memDC, rightEdge, lineY);
                            SelectObject(memDC, hSepHilight);
                            MoveToEx(memDC, padX, lineY + 1, nullptr);
                            LineTo(memDC, rightEdge, lineY + 1);
                        }
                        SelectObject(memDC, hSectionFont);
                        SetTextColor(memDC, RGB(15, 23, 42));
                        int hdrTop = (i > 0) ? (drawY + 14) : (drawY + 4);
                        RECT rcHdr = { padX, hdrTop, rightEdge, drawY + e.h };
                        DrawTextW(memDC, e.keyCol.c_str(), -1, &rcHdr, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
                    } else {
                        // Subtle row separator between items inside the same section
                        if (i > 0 && !p->entries[i - 1].isSectionHeader) {
                            SelectObject(memDC, hRowSep);
                            MoveToEx(memDC, p->leftColX, drawY, nullptr);
                            LineTo(memDC, rightEdge, drawY);
                        }

                        int keyTextLeft = p->leftColX;
                        if (e.hasIcon) {
                            Graphics gIcon(memDC);
                            gIcon.SetSmoothingMode(SmoothingModeAntiAlias);
                            gIcon.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
                            const int iconW = 26;
                            const int iconH = 22;
                            int iconX = p->leftColX;
                            int iconY = drawY + (e.h - iconH) / 2;
                            RectF rfIcon((float)iconX, (float)iconY, (float)iconW, (float)iconH);
                            Color bgCol = e.iconBtn.isPrimaryAction ? Color(255, 239, 68, 68) : Color(245, 15, 23, 42);
                            SolidBrush btnBg(bgCol);
                            Pen btnBorder(e.iconBtn.isPrimaryAction ? Color(255, 252, 165, 165) : Color(220, 51, 65, 85), 1.0f);
                            GraphicsState stB = gIcon.Save();
                            gIcon.SetSmoothingMode(SmoothingModeNone);
                            gIcon.SetPixelOffsetMode(PixelOffsetModeNone);
                            gIcon.FillRectangle(&btnBg, iconX, iconY, iconW, iconH);
                            gIcon.Restore(stB);
                            PepperSnapDaemon::DrawDockButtonIcon(gIcon, e.iconBtn, rfIcon);
                            GraphicsState stO = gIcon.Save();
                            gIcon.SetSmoothingMode(SmoothingModeNone);
                            gIcon.SetPixelOffsetMode(PixelOffsetModeNone);
                            gIcon.DrawRectangle(&btnBorder, iconX, iconY, iconW - 1, iconH - 1);
                            gIcon.Restore(stO);
                            keyTextLeft += iconW + 8;
                        }

                        SelectObject(memDC, hKeyFont);
                        SetTextColor(memDC, RGB(30, 41, 59));
                        RECT rcKey = { keyTextLeft, drawY, p->leftColX + p->leftColW, drawY + e.h };
                        DrawTextW(memDC, e.keyCol.c_str(), -1, &rcKey, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                        SelectObject(memDC, hDescFont);
                        SetTextColor(memDC, GetSysColor(COLOR_WINDOWTEXT));
                        int descTop = drawY + std::max(2, (e.h - e.textH) / 2);
                        RECT rcDesc = { p->rightColX, descTop, rightEdge, drawY + e.h };
                        DrawTextW(memDC, e.descCol.c_str(), -1, &rcDesc, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
                    }
                }

                SelectObject(memDC, hOldP);
                SelectObject(memDC, hOldF);
                DeleteObject(hRowSep);
                DeleteObject(hSepHilight);
                DeleteObject(hSepShadow);
                DeleteObject(hDescFont);
                DeleteObject(hKeyFont);
                DeleteObject(hSectionFont);
                DeleteObject(hTitleFont);

                RestoreDC(memDC, savedDC);

                // Permanent bottom footer bar + separating line
                RECT rcFooter = { 0, footerTop, clientW, clientH };
                FillRect(memDC, &rcFooter, GetSysColorBrush(COLOR_BTNFACE));
                HPEN hLinePen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
                HGDIOBJ hOldFooterPen = SelectObject(memDC, hLinePen);
                MoveToEx(memDC, 0, footerTop, nullptr);
                LineTo(memDC, clientW, footerTop);
                SelectObject(memDC, hOldFooterPen);
                DeleteObject(hLinePen);

                BitBlt(hdc, 0, 0, clientW, clientH, memDC, 0, 0, SRCCOPY);
                SelectObject(memDC, oldBmp);
                DeleteObject(memBmp);
                DeleteDC(memDC);
            }
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            if (p && p->minWinWidth > 0) {
                int sw = GetSystemMetrics(SM_CXSCREEN);
                int sh = GetSystemMetrics(SM_CYSCREEN);
                int maxW = std::min(p->maxWinWidth, std::max(p->minWinWidth, sw - 24));
                mmi->ptMinTrackSize.x = p->minWinWidth;
                mmi->ptMaxTrackSize.x = std::max(p->minWinWidth, maxW);
                mmi->ptMinTrackSize.y = 280;
                int maxH = std::min(p->fullWinHeight, std::max(320, sh - 24));
                mmi->ptMaxTrackSize.y = std::max(280, maxH);
            }
            return 0;
        }
        case WM_SIZE: {
            if (p && wParam != SIZE_MINIMIZED) {
                RECT rcC;
                GetClientRect(hWnd, &rcC);
                int clientW = std::max(1, (int)(rcC.right - rcC.left));
                RecalcShortcutsLayout(hWnd, p, clientW);
                LayoutShortcutsFooter(hWnd, p);
                UpdateShortcutsScroll(hWnd, p, p->scrollY);
                InvalidateRect(hWnd, nullptr, FALSE);
                RECT rcW;
                if (GetWindowRect(hWnd, &rcW)) {
                    p->savedWinWidth  = rcW.right - rcW.left;
                    p->savedWinHeight = rcW.bottom - rcW.top;
                }
            }
            return 0;
        }
        case WM_VSCROLL: {
            if (p && lParam == 0) {
                RECT rcC;
                GetClientRect(hWnd, &rcC);
                int viewportH = std::max(60, (int)(rcC.bottom - rcC.top) - p->footerHeight);
                int pageH = std::max(40, viewportH - 36);
                int target = p->scrollY;
                switch (LOWORD(wParam)) {
                    case SB_LINEUP:        target -= 36; break;
                    case SB_LINEDOWN:      target += 36; break;
                    case SB_PAGEUP:        target -= pageH; break;
                    case SB_PAGEDOWN:      target += pageH; break;
                    case SB_TOP:           target = 0; break;
                    case SB_BOTTOM:        target = p->scrollContentHeight; break;
                    case SB_THUMBTRACK:
                    case SB_THUMBPOSITION: {
                        SCROLLINFO si = { sizeof(SCROLLINFO), SIF_TRACKPOS };
                        if (GetScrollInfo(hWnd, SB_VERT, &si)) {
                            target = si.nTrackPos;
                        } else {
                            target = HIWORD(wParam);
                        }
                        break;
                    }
                    default: break;
                }
                UpdateShortcutsScroll(hWnd, p, target);
                return 0;
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            if (p) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (delta != 0) {
                    int step = -(delta * 48) / WHEEL_DELTA;
                    UpdateShortcutsScroll(hWnd, p, p->scrollY + step);
                }
                return 0;
            }
            break;
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

    ShortcutsDlgParams params;
    params.mainTitle = L"PepperSnap v" + std::wstring(APP_VERSION) + L" — complete hotkeys & mechanisms reference";

    auto addSection = [&](const std::wstring& title) {
        ShortcutEntry e;
        e.isSectionHeader = true;
        e.keyCol = title;
        params.entries.push_back(e);
    };
    auto addRow = [&](const std::wstring& key, const std::wstring& desc) {
        ShortcutEntry e;
        e.isSectionHeader = false;
        e.hasIcon = false;
        e.keyCol = L"•  " + key;
        e.descCol = desc;
        params.entries.push_back(e);
    };
    auto addToolRow = [&](OverlayTool tool, const std::wstring& key, const std::wstring& desc) {
        ShortcutEntry e;
        e.isSectionHeader = false;
        e.hasIcon = true;
        e.iconBtn.isTool = true;
        e.iconBtn.tool = tool;
        e.keyCol = key;
        e.descCol = desc;
        params.entries.push_back(e);
    };
    auto addActionRow = [&](int btnId, bool primary, const std::wstring& key, const std::wstring& desc) {
        ShortcutEntry e;
        e.isSectionHeader = false;
        e.hasIcon = true;
        e.iconBtn.isTool = false;
        e.iconBtn.id = btnId;
        e.iconBtn.isPrimaryAction = primary;
        e.keyCol = key;
        e.descCol = desc;
        params.entries.push_back(e);
    };

    addSection(L"Global capture hotkeys & system integration");
    addRow(FormatLabelWithShortcut(L"Custom area", hkRegionSnip),                       L"Start custom area snip & annotate (customizable in Customize keys)");
    addRow(FormatLabelWithShortcut(L"Instant fullscreen", hkFullSnap),                  L"Capture & save all monitors immediately (customizable in Customize keys)");
    addRow(FormatLabelWithShortcut(L"Instant save previous custom area", hkPrevRegion), L"Capture & save previous custom area (customizable in Customize keys)");
    addRow(L"Options \x2192 Customize keys",            L"Customize any shortcut key (Esc to cancel, Delete to leave it empty; PrintScreen reserved)");
    addRow(L"Options \x2192 Naming pattern (\x24D8)",   L"Click \x24D8 beside Restore default for acceptable syntax ({YYYY}, {MMMM}, {DDDD}, etc. — click or select any syntax to copy)");
    addRow(L"Options \x2192 Restore everything to default", L"Reset all Options settings and shortcut keys back to factory defaults (with confirmation)");
    addRow(L"Left-click system tray icon",   L"Start custom area capture");
    addRow(L"Right-click system tray icon",  L"Open tray menu (Open image to edit / pin (supports multi-select), Smooth / Unsmooth all pinned image, Show / Hide toolbar on all pinned image, Show / Hide outline on all pinned image, Close all pinned image, Options, etc.)");
    addRow(L"Right-click image in Explorer", L"\"Edit with PepperSnap\" (single image; shows selectable bottom frame list for multi-frame .gif / .webp) or \"Pin on top\" (single or multi-selected images)");
    addRow(L"Multi-frame GIF / WebP editor", L"When editing a multi-frame .gif or .webp, a horizontally scrollable & vertically resizable frame list appears at the bottom (on top of custom area & toolbar, auto-hides when focusing custom area/toolbar if \"Auto-hide frame list\" is enabled in Options, with a persistent dropdown button & Play / Pause icon on the right side)");
    addActionRow(DBTN_ACT_FRAME_PLAY_PAUSE, false, FormatLabelWithShortcut(L"Play / Pause frames (GIF/WebP)", hkFramePlayPause),   L"Play or pause multi-frame .gif / .webp animation in \"Edit with PepperSnap\" (or click the Play / Pause icon on the right side of the Hide / Show frames button)");
    addActionRow(DBTN_ACT_FRAME_TOGGLE,     false, FormatLabelWithShortcut(L"Hide / Show frames (GIF/WebP)",  hkFrameToggleStrip), L"Collapse or expand the bottom frame list in \"Edit with PepperSnap\" (or click the Hide / Show frames dropdown button)");
    addRow(L"Resize this info window",       L"Drag any window edge or corner to adjust width and height (text wraps & size persists)");

    addSection(L"Custom area — window selection, aiming & size input");
    addRow(L"Hold Shift then click (before selecting)",  L"Override pixel-grid snapping (with 1px crosshair gap), copy pixel RGB hex code (#RRGGBB) under magnifier & close");
    addRow(L"Hold Shift then scroll (before selecting)", L"Increase or decrease magnifier zoom (default 5×, minimum 100% / 1×, up to maximum zoom)");
    addRow(L"Arrow keys (before or while dragging)",     L"Jump 1 pixel up, down, left, or right before selecting or while holding left/right click to drag/resize selection");
    addRow(L"Hold Ctrl then click (before selecting)",   L"Auto-detect, highlight & select any window, notification pop-up, or sidebar");
    addRow(L"Hold Ctrl (after selecting)",   L"Temporarily switch active tool to Select mode while Ctrl is held");
    addRow(L"Right-click drag",              L"Constrain selection or resize to a 1:1 square (magnifier sticks to the opposite point of the current anchor point)");
    addRow(L"Drag 8 resize handles",         L"Resize existing selected area with a 1:1 square magnification window at the active resize point");
    addRow(L"Click W × H px indicator",      L"Type custom W×H, width only (200 or 200x), or height only (xx200 or x200), then press Enter (Esc cancels)");

    addSection(L"Custom area — Row 1: annotation tools");
    addToolRow(OverlayTool::SelectMove,     FormatLabelWithShortcut(L"Select mode",         hkToolSelect),    L"Select, multi-select, move, resize, or recolor annotations, or move selection (or hold Ctrl after creating a custom area)");
    addRow(L"Multi-select annotations (Select mode)", L"Hold Shift + click (or Ctrl + click) annotations, Shift + drag a selection box, or press Ctrl + A to select multiple annotations at once (move, recolor, resize stroke, or delete together)");
    addToolRow(OverlayTool::Pen,            FormatLabelWithShortcut(L"Pen",                 hkToolPen),       L"Freehand pen (hold Shift while drawing for 15° straight lines)");
    addToolRow(OverlayTool::Highlighter,    FormatLabelWithShortcut(L"Stabilo highlighter", hkToolStabilo),   L"Chisel-tip marker (hold Shift while drawing for 15° straight lines)");
    addToolRow(OverlayTool::Line,           FormatLabelWithShortcut(L"Line",                hkToolLine),      L"Straight line (hold Shift for 15° angle snapping)");
    addToolRow(OverlayTool::Arrow,          FormatLabelWithShortcut(L"Arrow",               hkToolArrow),     L"Arrow pointer (hold Shift for 15° angle snapping)");
    addToolRow(OverlayTool::NumberArrow,    FormatLabelWithShortcut(L"Numbering arrow",     hkToolNumber),    L"Auto-incrementing step badge + arrow (hold Shift for 15° angles)");
    addActionRow(DBTN_ACT_RESET_NUM, false, FormatLabelWithShortcut(L"Reset counter to 1",  hkToolResetNum),  L"Appears beside Numbering arrow to reset next step number to 1");
    addToolRow(OverlayTool::Rectangle,      FormatLabelWithShortcut(L"Square / rectangle",  hkToolRect),      L"Rectangle outline (hold Shift for 1:1 square)");
    addToolRow(OverlayTool::Ellipse,        FormatLabelWithShortcut(L"Circle / ellipse",    hkToolEllipse),   L"Ellipse outline (hold Shift for 1:1 circle)");
    addToolRow(OverlayTool::TextBox,        FormatLabelWithShortcut(L"Text box",            hkToolText),      L"In-place text (Enter commits, Shift + Enter new line, Esc exits)");
    addToolRow(OverlayTool::MosaicSquare,   FormatLabelWithShortcut(L"Mosaic square",       hkToolMosaicSq),  L"Pixelate rectangular area (hold Shift for 1:1 square)");
    addToolRow(OverlayTool::MosaicCircle,   FormatLabelWithShortcut(L"Mosaic circle",       hkToolMosaicCir), L"Pixelate elliptical area (hold Shift for 1:1 circle)");

    addSection(L"Custom area — Row 2: colors & stroke sizes");
    addRow(L"5 color swatches",              L"Set active tool color or recolor selected annotation(s)");
    addRow(L"S / M / L / XL buttons",        L"Preset stroke / text / mosaic sizes (2px, 4px, 8px, 14px)");
    addRow(L"Click custom size box (px)",    L"Type exact size (1–120 px) right of XL, then press Enter");
    addRow(L"Mouse scroll wheel",            L"Adjust active or selected annotation(s) stroke size (1–120 px)");
    addRow(L"Delete / Backspace",            L"Delete currently selected annotation(s) (in Select mode)");

    addSection(L"Custom area — Row 3: toolbar actions & export shortcuts");
    addActionRow(DBTN_ACT_DRAG_HUD, false,  L"Drag toolbar handle",                                                     L"Drag 4-way arrow button to move toolbar (resets when selection moves)");
    addActionRow(DBTN_ACT_UNDO,     false,  FormatLabelWithShortcut(L"Undo",                      hkActUndo),           L"Undo last annotation action");
    addActionRow(DBTN_ACT_REDO,     false,  FormatLabelWithShortcut(L"Redo",                      hkActRedo),           L"Redo undone annotation action");
    addActionRow(DBTN_ACT_CLEAR,    false,  FormatLabelWithShortcut(L"Clear all annotations",     hkActClear),          L"Remove all annotations from current selection");
    addActionRow(DBTN_ACT_OPTIONS,  false,  FormatLabelWithShortcut(L"Options",                   hkActOptions),        L"Open PepperSnap Options window");
    addActionRow(DBTN_ACT_PIN,      false,  FormatLabelWithShortcut(L"Pin on top",                hkActPin),            L"Pin selected capture as a floating topmost desktop window");
    addActionRow(DBTN_ACT_OCR,      false,  FormatLabelWithShortcut(L"Extract text with OCR",     hkActOcr),            L"Recognize text in selection via Windows OCR & copy to clipboard");
    addActionRow(DBTN_ACT_SAVE_AS,  false,  FormatLabelWithShortcut(L"Save as JPG/PNG/WEBP/BMP",  hkActSaveAs),         L"Open Save As dialog for selected region");
    addActionRow(DBTN_ACT_SAVE,     false,  FormatLabelWithShortcut(L"Quick save",                hkActSave),           L"Save capture directly to default folder & close");
    addActionRow(DBTN_ACT_COPY,     true,   FormatLabelWithShortcut(L"Copy to clipboard",         hkActCopy),           L"Copy capture to clipboard & close");
    addActionRow(DBTN_ACT_CLOSE,    false,  L"Close / Select mode (Esc)",                                               L"Switch active drawing tool to Select mode, or exit custom area");

    addSection(L"Pin on top — floating window toolbar & mechanisms");
    addRow(L"You can pin multiple image at once", L"Pin multiple captures, multi-select images in \"Open image to pin on top...\", or multi-select images in Explorer and click \"Pin on top\" (auto-tiled so they don't overlap, and fit-to-screen capped at 40% of screen size)");
    addRow(L"Hold Shift + any action / key / button", L"Apply any pinned image button, mouse action, size/percentage input, or shortcut key (including Shift + Save As) to all pinned images at once (e.g. Shift + Copy copies all pinned images to clipboard; Shift + double right-click closes all)");
    addRow(L"Animated GIF & WebP support",   L"Pinned .gif and animated .webp images play continuously; .png, .webp & .gif transparency is preserved over the desktop");
    addRow(L"Ctrl + Click / Ctrl + Shift + Click", L"Pause or play pinned animation on current frame (only shown & active on animated .gif / multi-frame animated .webp; hold Ctrl + Shift + click to pause/play all pinned animations)");
    addRow(L"Click W × H px or % indicator", L"Second toolbar row shows editable [W×H px | %] — click to type W×H, width only (200 or 200x), height only (xx200 or x200), or zoom % (keeps aspect ratio; Shift + Enter applies to all pinned images)");
    addRow(L"Hover over pinned image (500ms)", L"Pop-up shows \"Double-click to hide/show toolbar\", \"Double right-click to close\", \"Scroll to resize\", \"Middle-click to reset size\", \"Ctrl + Click\" (on animations only), and Shift multi-pin hint");
    addRow(L"Drag pinned image",             L"Move floating window (temporarily hides toolbar while dragging, snaps to screen edges; image can overflow screen)");
    addRow(L"Hold Shift + drag",             L"Temporarily disable screen-edge snapping while moving");
    addRow(L"Double-click pinned image",     L"Hide or show the pin-on-top toolbar (hold Shift to apply to all pinned images; status persists; default: shown)");
    addRow(L"Double right-click pinned image", L"Close pinned image (hold Shift + double right-click to close all pinned images)");
    addRow(L"Scroll / Shift + Scroll",       L"Zoom in or out (hold Shift + scroll to zoom all pinned images; anchored to bottom-right corner)");
    addRow(L"Middle-click pinned image",     L"Reset pinned image to 100% original pixel size on click release (hold Shift + middle-click to reset all pinned images)");
    addRow(FormatLabelWithShortcut(L"Zoom out",               hkPinZoomOut),     L"Scale pinned image down via shortcut key or scroll down (hold Shift to apply to all pinned images)");
    addRow(FormatLabelWithShortcut(L"Zoom in",                hkPinZoomIn),      L"Scale pinned image up via shortcut key or scroll up (hold Shift to apply to all pinned images)");
    addRow(FormatLabelWithShortcut(L"Reset to original size", hkPinZoomReset),   L"Restore 100% original pixel size via shortcut key or middle-click (hold Shift to apply to all pinned images)");
    addRow(FormatLabelWithShortcut(L"Hide / Show toolbar",    hkPinHideToolbar), L"Hide or show floating toolbar on pinned image via shortcut key or double-click (hold Shift to apply to all pinned images)");
    addRow(L"Smooth / Unsmooth all pinned image", L"Tray menu button that cycles smoothing or unsmoothing the image filter across all pinned images (keeps tray menu open)");
    addRow(L"Show / Hide toolbar on all pinned image", L"Tray menu button that cycles showing or hiding the toolbar across all pinned images (keeps tray menu open)");
    addRow(L"Show / Hide outline on all pinned image", L"Tray menu button that cycles showing or hiding the red outline across all pinned images (keeps tray menu open)");
    addRow(L"Smart toolbar positioning",     L"Toolbar moves above or inside image when out of space and stays above taskbar");
    addActionRow(DBTN_ACT_PIN_OUTLINE,      false, FormatLabelWithShortcut(L"Show / Hide outline",      hkPinOutline),     L"Toggle red border outline around pinned image (hold Shift to apply to all pinned images; default: on)");
    addActionRow(DBTN_ACT_PIN_UNFILTER,     false, FormatLabelWithShortcut(L"Smooth the image",         hkPinSmooth),      L"Smooth image with filter when on; unfiltered (pixelated) when off (hold Shift to apply to all pinned images; default: off)");
    addActionRow(DBTN_ACT_SAVE_AS,      false, FormatLabelWithShortcut(L"Save as JPG/PNG/WEBP/BMP", hkPinSaveAs),    L"Open Save As dialog for pinned image (hold Shift to Save As all pinned images; shortcut empty by default, customizable in Options)");
    addActionRow(DBTN_ACT_SAVE,         false, FormatLabelWithShortcut(L"Quick save",               hkPinSave),      L"Save pinned image directly to default folder (hold Shift to quick-save all pinned images; shortcut empty by default, customizable in Options)");
    addActionRow(DBTN_ACT_COPY,         true,  FormatLabelWithShortcut(L"Copy to clipboard",        hkPinCopy),      L"Copy pinned image to clipboard (hold Shift to copy all pinned images to clipboard)");
    addActionRow(DBTN_ACT_CLOSE,        false, FormatLabelWithShortcut(L"Close pinned image",       hkPinClose),     L"Close pinned image (hold Shift to close all pinned images, or double right-click, or use tray menu)");

    int maxKeyW = 220;
    int maxDescW = 360;
    int maxHeaderW = 480;
    HDC hdcScreen = GetDC(nullptr);
    if (hdcScreen) {
        HFONT hTitleFont = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HFONT hKeyFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HFONT hDescFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HGDIOBJ hOldF = SelectObject(hdcScreen, hTitleFont);
        RECT rcT = { 0, 0, 0, 0 };
        DrawTextW(hdcScreen, params.mainTitle.c_str(), -1, &rcT, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
        maxHeaderW = std::max(maxHeaderW, (int)(rcT.right - rcT.left) + 36);

        for (const auto& e : params.entries) {
            if (e.isSectionHeader) {
                RECT rcS = { 0, 0, 0, 0 };
                SelectObject(hdcScreen, hTitleFont);
                DrawTextW(hdcScreen, e.keyCol.c_str(), -1, &rcS, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
                maxHeaderW = std::max(maxHeaderW, (int)(rcS.right - rcS.left) + 36);
            } else {
                RECT rcK = { 0, 0, 0, 0 };
                SelectObject(hdcScreen, hKeyFont);
                DrawTextW(hdcScreen, e.keyCol.c_str(), -1, &rcK, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
                int iconExtra = e.hasIcon ? 34 : 0;
                maxKeyW = std::max(maxKeyW, (int)(rcK.right - rcK.left) + iconExtra + 12);

                RECT rcD = { 0, 0, 0, 0 };
                SelectObject(hdcScreen, hDescFont);
                DrawTextW(hdcScreen, e.descCol.c_str(), -1, &rcD, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
                maxDescW = std::max(maxDescW, (int)(rcD.right - rcD.left) + 8);
            }
        }
        SelectObject(hdcScreen, hOldF);
        DeleteObject(hDescFont);
        DeleteObject(hKeyFont);
        DeleteObject(hTitleFont);
        ReleaseDC(nullptr, hdcScreen);
    }

    params.leftColX = 22;
    params.leftColW = maxKeyW;
    params.rightColX = params.leftColX + params.leftColW + 14;

    int minDescColW = 340;
    int defDescColW = 470;
    int maxDescColW = std::max(540, std::min(maxDescW, 660));
    params.rightColW = defDescColW;
    params.footerHeight = 52;

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    DWORD dwStyle = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VSCROLL | WS_VISIBLE | WS_CLIPCHILDREN;
    DWORD dwExStyle = WS_EX_TOPMOST;
    int vScrollW = GetSystemMetrics(SM_CXVSCROLL);

    auto calcWinW = [&](int clientWidth) {
        RECT rcW = { 0, 0, clientWidth, 400 };
        AdjustWindowRectEx(&rcW, dwStyle, FALSE, dwExStyle);
        return (int)(rcW.right - rcW.left) + vScrollW;
    };

    int minClientW = std::max(maxHeaderW, params.rightColX + minDescColW + 20);
    int defClientW = std::max(minClientW, params.rightColX + defDescColW + 20);
    int maxClientW = std::max(defClientW, params.rightColX + maxDescColW + 20);

    int minWinW = calcWinW(minClientW);
    int defWinW = calcWinW(defClientW);
    int maxWinW = std::max(minWinW, std::min(calcWinW(maxClientW), std::max(minWinW, sw - 24)));
    int initW = std::max(minWinW, std::min(maxWinW, (shortcutsWindowWidth > 0) ? shortcutsWindowWidth : defWinW));

    RECT rcFrame = { 0, 0, 0, 0 };
    AdjustWindowRectEx(&rcFrame, dwStyle, FALSE, dwExStyle);
    int nonClientW = (rcFrame.right - rcFrame.left) + vScrollW;
    int initClientW = std::max(minClientW, initW - nonClientW);

    RecalcShortcutsLayout(nullptr, &params, initClientW);

    int fullWinH = params.fullWinHeight;
    int maxAllowedH = std::min(fullWinH, std::max(320, sh - 24));
    int initH = std::max(280, std::min(maxAllowedH, shortcutsWindowHeight));
    params.minWinWidth = minWinW;
    params.maxWinWidth = maxWinW;
    params.savedWinWidth = initW;
    params.savedWinHeight = initH;

    HWND hOwner = (hOptionsWnd && IsWindow(hOptionsWnd))
        ? hOptionsWnd
        : hTrayWnd;

    HWND hDlg = CreateWindowExW(
        dwExStyle, L"PepperSnapShortcutsModal",
        L"PepperSnap — keyboard shortcuts and info",
        dwStyle,
        std::max(10, (sw - initW) / 2), std::max(10, (sh - initH) / 2), initW, initH, hOwner, nullptr, hInst, &params
    );
    hShortcutsWnd = hDlg;
    BringWindowToTop(hDlg);
    SetForegroundWindow(hDlg);
    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_MOUSEWHEEL && (msg.hwnd == hDlg || IsChild(hDlg, msg.hwnd))) {
            SendMessageW(hDlg, WM_MOUSEWHEEL, msg.wParam, msg.lParam);
            continue;
        }
        if (!hOverlayWnd && (msg.hwnd == hDlg || IsChild(hDlg, msg.hwnd)) &&
            msg.message == WM_KEYDOWN && (msg.wParam == VK_RETURN || msg.wParam == VK_ESCAPE)) {
            DestroyWindow(hDlg);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    hShortcutsWnd = nullptr;
    bool sizeChanged = false;
    if (params.savedWinWidth >= params.minWinWidth && params.savedWinWidth != shortcutsWindowWidth) {
        shortcutsWindowWidth = params.savedWinWidth;
        sizeChanged = true;
    }
    if (params.savedWinHeight >= 280 && params.savedWinHeight != shortcutsWindowHeight) {
        shortcutsWindowHeight = params.savedWinHeight;
        sizeChanged = true;
    }
    if (sizeChanged) {
        SaveSettings();
    }
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
    ClearAnnotationSelection();
    if (hOverlayWnd) InvalidateRect(hOverlayWnd, nullptr, FALSE);
}

void PepperSnapDaemon::Redo() {
    CommitActiveTextBox();
    if (redoStack.empty()) return;
    undoStack.push_back(annotations);
    annotations = redoStack.back();
    redoStack.pop_back();
    RecalcNextStepNum();
    ClearAnnotationSelection();
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

    StopOverlayFramePlayback();
    isSavingAsModalOpen = true;

    HWND hSaveOwner = (hOverlayWnd && IsWindow(hOverlayWnd)) ? hOverlayWnd : hTrayWnd;
    if (hOverlayWnd && IsWindow(hOverlayWnd)) {
        SetForegroundWindow(hOverlayWnd);
    }

    WCHAR szFile[MAX_PATH] = {0};
    std::wstring defName = FormatFilename(captureCounter++) + GetFormatExtension(regionFormat);
    wcsncpy_s(szFile, defName.c_str(), _TRUNCATE);

    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hSaveOwner;
    ofn.lpstrInitialDir = saveFolder.c_str();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"JPEG image (*.jpg)\0*.jpg\0PNG image (*.png)\0*.png\0WebP image (*.webp)\0*.webp\0BMP bitmap (*.bmp)\0*.bmp\0";
    ofn.nFilterIndex = (DWORD)regionFormat + 1;
    const wchar_t* defExts[4] = { L"jpg", L"png", L"webp", L"bmp" };
    ofn.lpstrDefExt = defExts[(int)regionFormat & 3];
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;

    BOOL savedOk = GetSaveFileNameW(&ofn);
    isSavingAsModalOpen = false;

    if (savedOk) {
        if (SaveBitmapToPath(bmp, szFile)) {
            lastSavedFilePath = szFile;
            ShowTrayToast(L"Saved capture", std::wstring(szFile) + L"\n(click to open in Explorer)");
        }
        delete bmp;
        CloseRegionSnipOverlay();
    } else {
        delete bmp;
        if (hOverlayWnd && IsWindow(hOverlayWnd)) {
            SetForegroundWindow(hOverlayWnd);
            SetFocus(hOverlayWnd);
            InvalidateRect(hOverlayWnd, nullptr, FALSE);
        }
    }
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
        SaveSettings();
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
                bool isSel = g_Daemon.IsAnnotationSelected(ann.id);
                if (ann.tool == OverlayTool::Highlighter) {
                    if (isSel) {
                        Pen selPen(Color(255, 56, 189, 248), 1.5f);
                        selPen.SetDashStyle(DashStyleDash);
                        float x = std::min(ann.startPt.x, ann.endPt.x) - 6.0f;
                        float y = std::min(ann.startPt.y, ann.endPt.y) - 6.0f;
                        float w = std::max(16.0f, std::abs(ann.endPt.x - ann.startPt.x) + 12.0f);
                        float h = std::max(16.0f, std::abs(ann.endPt.y - ann.startPt.y) + 12.0f);
                        g.DrawRectangle(&selPen, x, y, w, h);
                    }
                } else if (ann.tool != OverlayTool::MosaicSquare && ann.tool != OverlayTool::MosaicCircle) {
                    PepperSnapDaemon::DrawVectorAnnotation(g, ann, 0, 0, isSel, false);
                } else {
                    // 1. Keep the temporary outline for Mosaic Rectangle & Mosaic Circle until save so it stays as an indicator!
                    float mx0 = std::min(ann.startPt.x, ann.endPt.x);
                    float my0 = std::min(ann.startPt.y, ann.endPt.y);
                    float mw  = std::abs(ann.endPt.x - ann.startPt.x);
                    float mh  = std::abs(ann.endPt.y - ann.startPt.y);
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
            if (g_Daemon.dragMode == DragMode::MarqueeSelectingAnnotations && g_Daemon.hasMarqueeBox) {
                int mx0 = std::min(g_Daemon.marqueeRect.left, g_Daemon.marqueeRect.right);
                int my0 = std::min(g_Daemon.marqueeRect.top, g_Daemon.marqueeRect.bottom);
                int mw0 = std::abs(g_Daemon.marqueeRect.right - g_Daemon.marqueeRect.left);
                int mh0 = std::abs(g_Daemon.marqueeRect.bottom - g_Daemon.marqueeRect.top);
                if (mw0 >= 2 || mh0 >= 2) {
                    SolidBrush mqFill(Color(42, 56, 189, 248));
                    Pen mqBorder(Color(235, 56, 189, 248), 1.5f);
                    mqBorder.SetDashStyle(DashStyleDash);
                    g.FillRectangle(&mqFill, mx0, my0, mw0, mh0);
                    g.DrawRectangle(&mqBorder, mx0, my0, mw0, mh0);
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
            //    Sized equal to 3 toolbar buttons (3 * 32 + 2 * 3 = 102px wide, 28px high).
            bool pillRight = false, pillBottom = false;
            g_Daemon.GetDimensionPillAnchor(pillRight, pillBottom);
            const int pillBtnW = 32, pillBtnH = 28, pillBtnGap = 3;
            int pillW = pillBtnW * 3 + pillBtnGap * 2;
            int pillH = pillBtnH;
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
                : (std::to_wstring(sw) + L"\x00D7" + std::to_wstring(sh));
            PepperSnapDaemon::DrawEditablePillText(
                g, g_Daemon.dimPillRect,
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
            bool pressed = (b.id == g_Daemon.pressedBtnId && hovered);

            if (b.isColor) {
                SolidBrush swB(b.swatchColor);
                g.FillRectangle(&swB, rf);
                bool actCol = (b.swatchColor.GetValue() == g_Daemon.activeColor.GetValue());
                bool isWhiteSwatch = (b.swatchColor.GetR() > 235 && b.swatchColor.GetG() > 235 && b.swatchColor.GetB() > 235);
                Color borderCol = actCol
                    ? (isWhiteSwatch ? Color(255, 239, 68, 68) : Color(255, 255, 255, 255))
                    : (hovered ? Color(255, 203, 213, 225) : Color(220, 71, 85, 105));
                Pen swP(borderCol, (actCol || pressed) ? 2.2f : 1.0f);
                g.DrawRectangle(&swP, rf.X, rf.Y, rf.Width, rf.Height);
                continue;
            }

            OverlayTool effectiveTool = g_Daemon.GetEffectiveTool();
            bool activeState = (b.isTool && b.tool == effectiveTool) ||
                               (b.isStroke && std::abs(b.strokeVal - g_Daemon.activeStroke) < 0.5f);
            Color bgCol = Color(240, 15, 23, 42);
            if (b.isPrimaryAction || activeState) {
                bgCol = pressed ? Color(255, 185, 28, 28) : (hovered ? Color(255, 220, 38, 38) : Color(255, 239, 68, 68));
            } else if (pressed) {
                bgCol = Color(255, 71, 85, 105);
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
                g, g_Daemon.customStrokeRect,
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

                const int pillBtnW = 32, pillBtnH = 28, pillBtnGap = 3;
                int pillW = pillBtnW * 3 + pillBtnGap * 2;
                int pillH = pillBtnH;
                const int outlineGap = 8;
                int pillX = std::max(5, std::min(W - pillW - 5, wx + 5));
                int pillY = (wy >= pillH + outlineGap + 4) ? (wy - outlineGap - pillH) : (wy + 6);
                pillY = std::max(4, std::min(H - pillH - 4, pillY));

                SolidBrush pillBg(Color(235, 15, 23, 42));
                g.FillRectangle(&pillBg, pillX, pillY, pillW, pillH);
                Pen pillBorder(Color(160, 71, 85, 105), 1.4f);
                g.DrawRectangle(&pillBorder, pillX, pillY, pillW, pillH);

                RECT hoverPillRect = { pillX, pillY, pillX + pillW, pillY + pillH };
                std::wstring dimValStr = std::to_wstring(ww) + L"\x00D7" + std::to_wstring(wh);
                PepperSnapDaemon::DrawEditablePillText(
                    g, hoverPillRect,
                    dimValStr, L" px", false, 0, 0
                );
            }
        } else {
            Pen crossPen(Color(150, 239, 68, 68), 1.0f);
            crossPen.SetDashStyle(DashStyleDash);
            bool shiftHeldCross = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) ||
                                  ((GetKeyState(VK_SHIFT) & 0x8000) != 0) ||
                                  ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) ||
                                  ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0);
            if (shiftHeldCross) {
                GraphicsState stScreenCross = g.Save();
                g.SetClip(Rect(g_Daemon.mousePt.x, g_Daemon.mousePt.y, 1, 1), CombineModeExclude);
                g.DrawLine(&crossPen, g_Daemon.mousePt.x, 0, g_Daemon.mousePt.x, H);
                g.DrawLine(&crossPen, 0, g_Daemon.mousePt.y, W, g_Daemon.mousePt.y);
                g.Restore(stScreenCross);
            } else {
                g.DrawLine(&crossPen, g_Daemon.mousePt.x, 0, g_Daemon.mousePt.x, H);
                g.DrawLine(&crossPen, 0, g_Daemon.mousePt.y, W, g_Daemon.mousePt.y);
            }
        }
    }

    // Magnifier Loupe when aiming, creating a selection, or resizing an existing selection
    bool isResizingSelection =
        (g_Daemon.dragMode >= DragMode::ResizeTL && g_Daemon.dragMode <= DragMode::ResizeL);
    if (!g_Daemon.hasSelection || g_Daemon.dragMode == DragMode::CreatingSelection || isResizingSelection) {
        int mx = g_Daemon.mousePt.x;
        int my = g_Daemon.mousePt.y;
        if (isResizingSelection || (g_Daemon.dragMode == DragMode::CreatingSelection && g_Daemon.isRightClickSelectionDrag)) {
            mx = g_Daemon.selectionTargetPt.x;
            my = g_Daemon.selectionTargetPt.y;
        }
        int cMx = std::max(0, std::min(W - 1, mx));
        int cMy = std::max(0, std::min(H - 1, my));
        BYTE pr = 0, pg = 0, pb = 0;
        if (g_Daemon.brightDibPixels) {
            DWORD raw = g_Daemon.brightDibPixels[(size_t)cMy * W + cMx];
            pb = (BYTE)(raw & 0xFF);
            pg = (BYTE)((raw >> 8) & 0xFF);
            pr = (BYTE)((raw >> 16) & 0xFF);
        } else if (g_Daemon.frozenDesktopBmp) {
            Color pxCol;
            g_Daemon.frozenDesktopBmp->GetPixel(cMx, cMy, &pxCol);
            pr = pxCol.GetR();
            pg = pxCol.GetG();
            pb = pxCol.GetB();
        }
        wchar_t hexBuf[64];
        swprintf_s(hexBuf, L"(%d,%d) #%02X%02X%02X", mx, my, pr, pg, pb);

        const wchar_t* hintRow1 = L"Hold Shift then click to pick color";
        const wchar_t* hintRow2 = L"Hold Ctrl then click to select window";
        const wchar_t* hintRow3 = L"Press Esc to exit";
        RectF bHex, b1, b2, b3;
        int imgW = 110;
        int imgH = 110;
        int loupeW = imgW + 12;
        int loupeH = imgH + 12;
        if (!isResizingSelection) {
            g.MeasureString(hexBuf, -1, &monoFont, PointF(0.0f, 0.0f), &bHex);
            g.MeasureString(hintRow1, -1, &smallFont, PointF(0.0f, 0.0f), &b1);
            g.MeasureString(hintRow2, -1, &smallFont, PointF(0.0f, 0.0f), &b2);
            g.MeasureString(hintRow3, -1, &smallFont, PointF(0.0f, 0.0f), &b3);
            int maxHintW = (int)std::ceil(std::max(b1.Width, std::max(b2.Width, b3.Width))) + 4;
            int minHexRowW = (int)std::ceil(bHex.Width) + 44;
            imgW = std::max(120, ((std::max(maxHintW, minHexRowW) + 2) + 1) & ~1);
            imgH = 90;
            loupeW = imgW + 12;
            loupeH = 170;
        }

        int lx = (mx + 24 + loupeW < W) ? (mx + 24) : (mx - loupeW - 24);
        int ly = (my + 24 + loupeH < H) ? (my + 24) : (my - loupeH - 24);

        SolidBrush loupeBg(Color(240, 15, 23, 42));
        Pen loupeBorder(Color(255, 239, 68, 68), 1.5f);
        g.FillRectangle(&loupeBg, lx, ly, loupeW, loupeH);

        if (g_Daemon.frozenDesktopBmp) {
            bool ctrlHeldNow = ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) ||
                               ((GetKeyState(VK_CONTROL) & 0x8000) != 0) ||
                               ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0) ||
                               ((GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0);
            bool shiftHeldNow = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) ||
                                ((GetKeyState(VK_SHIFT) & 0x8000) != 0) ||
                                ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) ||
                                ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0);
            bool overrideGridSnap = (!g_Daemon.hasSelection && shiftHeldNow && !ctrlHeldNow);
            int mag = g_Daemon.hasSelection ? 5 : std::max(1, std::min(210, g_Daemon.loupeMagnification));
            double scaleX = 1.0 / (double)mag;
            double scaleY = 1.0 / (double)mag;
            double subShift = (mag & 1) ? 0.0 : 0.5;

            if (g_Daemon.brightDibPixels && g_Daemon.backBufferPixels && !tempBackBuffer) {
                g.Flush(FlushIntentionSync);
                int dstX0 = lx + 6;
                int dstY0 = ly + 6;
                double halfW = (double)(imgW / 2);
                double halfH = (double)(imgH / 2);
                for (int dy = 0; dy < imgH; ++dy) {
                    int dstY = dstY0 + dy;
                    if (dstY < 0 || dstY >= H) continue;
                    double relY = overrideGridSnap
                        ? (((double)dy + subShift - halfH) * scaleY)
                        : (((double)dy - halfH) * scaleY);
                    double srcYf = (overrideGridSnap || mag <= 1) ? ((double)my + 0.5 + relY) : ((double)my + relY);
                    int sy = std::max(0, std::min(H - 1, (int)std::floor(srcYf)));
                    const DWORD* srcRow = g_Daemon.brightDibPixels + (size_t)sy * W;
                    DWORD* dstRow = g_Daemon.backBufferPixels + (size_t)dstY * W;
                    for (int dx = 0; dx < imgW; ++dx) {
                        int dstX = dstX0 + dx;
                        if (dstX < 0 || dstX >= W) continue;
                        double relX = overrideGridSnap
                            ? (((double)dx + subShift - halfW) * scaleX)
                            : (((double)dx - halfW) * scaleX);
                        double srcXf = (overrideGridSnap || mag <= 1) ? ((double)mx + 0.5 + relX) : ((double)mx + relX);
                        int sx = std::max(0, std::min(W - 1, (int)std::floor(srcXf)));
                        dstRow[dstX] = srcRow[sx];
                    }
                }
            } else {
                GraphicsState st = g.Save();
                g.SetInterpolationMode(InterpolationModeNearestNeighbor);
                g.SetPixelOffsetMode(PixelOffsetModeHalf);
                float srcW = (float)imgW / (float)mag;
                float srcH = (float)imgH / (float)mag;
                float srcXf = (overrideGridSnap || mag <= 1) ? ((float)mx + 0.5f - srcW * 0.5f) : ((float)mx - srcW * 0.5f);
                float srcYf = (overrideGridSnap || mag <= 1) ? ((float)my + 0.5f - srcH * 0.5f) : ((float)my - srcH * 0.5f);
                g.DrawImage(g_Daemon.frozenDesktopBmp, RectF((float)(lx + 6), (float)(ly + 6), (float)imgW, (float)imgH), srcXf, srcYf, srcW, srcH, UnitPixel);
                g.Restore(st);
            }

            Pen centerCross(Color(200, 239, 68, 68), 1.5f);
            int crossX = lx + 6 + imgW / 2;
            int crossY = ly + 6 + imgH / 2;
            if (overrideGridSnap) {
                int pxLeft = crossX - (mag / 2);
                int pxTop  = crossY - (mag / 2);
                GraphicsState stCross = g.Save();
                g.SetClip(Rect(lx + 6, ly + 6, imgW, imgH), CombineModeIntersect);
                g.SetClip(Rect(pxLeft, pxTop, mag, mag), CombineModeExclude);
                g.DrawLine(&centerCross, crossX, ly + 6, crossX, ly + 6 + imgH);
                g.DrawLine(&centerCross, lx + 6, crossY, lx + 6 + imgW, crossY);
                g.Restore(stCross);
            } else {
                g.DrawLine(&centerCross, crossX, ly + 6, crossX, ly + 6 + imgH);
                g.DrawLine(&centerCross, lx + 6, crossY, lx + 6 + imgW, crossY);
            }

            if (!isResizingSelection) {
                g.DrawString(hexBuf, -1, &monoFont, PointF((float)(lx + 7), (float)(ly + 101)), &whiteBrush);
                int stripX = lx + 7 + (int)std::ceil(bHex.Width) + 3;
                int stripRight = lx + 6 + imgW;
                if (stripRight > stripX + 4) {
                    int stripW = stripRight - stripX;
                    int stripY = ly + 102;
                    int stripH = 12;
                    GraphicsState stStrip = g.Save();
                    g.SetSmoothingMode(SmoothingModeNone);
                    g.SetPixelOffsetMode(PixelOffsetModeHalf);
                    SolidBrush pickColorBrush(Color(255, pr, pg, pb));
                    g.FillRectangle(&pickColorBrush, stripX, stripY, stripW, stripH);
                    g.Restore(stStrip);
                }
                g.DrawString(hintRow1, -1, &smallFont, PointF((float)(lx + 7), (float)(ly + 118)), &mutedBrush);
                g.DrawString(hintRow2, -1, &smallFont, PointF((float)(lx + 7), (float)(ly + 134)), &mutedBrush);
                g.DrawString(hintRow3, -1, &smallFont, PointF((float)(lx + 7), (float)(ly + 150)), &mutedBrush);
            }
        }
        g.DrawRectangle(&loupeBorder, lx, ly, loupeW, loupeH);
    }

    // Render Multi-Frame GIF / WebP Selector Strip ON TOP of custom area and its toolbar!
    if (g_Daemon.HasOverlayMultiFrames()) {
        g_Daemon.RenderOverlayFrameStrip(g, W, H);
    }
    } // Graphics g(memDC) scope ends and flushes before BitBlt to screen

    BitBlt(hdc, 0, 0, W, H, memDC, 0, 0, SRCCOPY);
    if (tempBackBuffer) {
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);
    }
}

static void ExecuteOverlayDockButtonAction(HWND hWnd, const DockButton& b) {
    g_Daemon.CommitActiveTextBox();
    if (b.isTool) {
        g_Daemon.activeTool = b.tool;
        if (IsWindow(hWnd)) InvalidateRect(hWnd, nullptr, FALSE);
        return;
    }
    if (b.isColor) {
        g_Daemon.activeColor = b.swatchColor;
        g_Daemon.SaveSettings();
        if (g_Daemon.HasSelectedAnnotations()) {
            bool pushed = false;
            for (auto& a : g_Daemon.annotations) {
                if (g_Daemon.IsAnnotationSelected(a.id)) {
                    if (!pushed && a.color.GetValue() != b.swatchColor.GetValue()) {
                        g_Daemon.PushUndo();
                        pushed = true;
                    }
                    a.color = b.swatchColor;
                }
            }
        }
        if (g_Daemon.isEditingText) {
            g_Daemon.editingTextAnn.color = b.swatchColor;
        }
        if (IsWindow(hWnd)) InvalidateRect(hWnd, nullptr, FALSE);
        return;
    }
    if (b.isStroke) {
        g_Daemon.ApplyStrokeToSelectedAnnotation(b.strokeVal, true);
        g_Daemon.SaveSettings();
        if (IsWindow(hWnd)) InvalidateRect(hWnd, nullptr, FALSE);
        return;
    }
    switch (b.id) {
        case DBTN_ACT_COPY:      g_Daemon.ActionCopyAndClose(); return;
        case DBTN_ACT_SAVE:      g_Daemon.ActionQuickSaveAndClose(); return;
        case DBTN_ACT_SAVE_AS:   g_Daemon.ActionSaveAsAndClose(); return;
        case DBTN_ACT_PIN:       g_Daemon.ActionPinToDesktop(); return;
        case DBTN_ACT_OCR:       g_Daemon.ActionOcrAndClose(); return;
        case DBTN_ACT_OPTIONS:
            g_Daemon.ShowOptionsModal();
            if (g_Daemon.hOverlayWnd && IsWindow(hWnd)) InvalidateRect(hWnd, nullptr, FALSE);
            return;
        case DBTN_ACT_UNDO:      g_Daemon.Undo(); return;
        case DBTN_ACT_REDO:      g_Daemon.Redo(); return;
        case DBTN_ACT_RESET_NUM:
            g_Daemon.nextStepNum = 1;
            if (IsWindow(hWnd)) InvalidateRect(hWnd, nullptr, FALSE);
            return;
        case DBTN_ACT_CLEAR:
            g_Daemon.PushUndo();
            g_Daemon.annotations.clear();
            g_Daemon.nextStepNum = 1;
            g_Daemon.ClearAnnotationSelection();
            if (IsWindow(hWnd)) InvalidateRect(hWnd, nullptr, FALSE);
            return;
        case DBTN_ACT_CLOSE:     g_Daemon.CloseRegionSnipOverlay(); return;
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
                if (g_Daemon.isSavingAsModalOpen) {
                    return 0;
                }
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
                if (!g_Daemon.hasSelection && (g_Daemon.dragMode == DragMode::None || g_Daemon.dragMode == DragMode::PendingOutsideSelection)) {
                    g_Daemon.lastCtrlTempSelect = false;
                    bool curShiftColorPick = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) ||
                                             ((GetKeyState(VK_SHIFT) & 0x8000) != 0) ||
                                             ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) ||
                                             ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0);
                    bool shiftChanged = (curShiftColorPick != g_Daemon.lastShiftColorPick);
                    g_Daemon.lastShiftColorPick = curShiftColorPick;
                    if (g_Daemon.UpdateCtrlWindowHover() || shiftChanged) {
                        g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                        InvalidateRect(hWnd, nullptr, FALSE);
                    }
                } else if (g_Daemon.HasCreatedCustomArea()) {
                    bool curCtrlTempSelect = g_Daemon.IsCtrlTempSelectActive();
                    if (curCtrlTempSelect != g_Daemon.lastCtrlTempSelect) {
                        g_Daemon.lastCtrlTempSelect = curCtrlTempSelect;
                        g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                        InvalidateRect(hWnd, nullptr, FALSE);
                    }
                }
            } else if (wParam == 3) {
                KillTimer(hWnd, 3);
                if (g_Daemon.HasOverlayMultiFrames() && g_Daemon.overlayFramesPlaying) {
                    size_t nextIdx = (g_Daemon.overlayActiveFrameIdx + 1) % g_Daemon.overlayFrames.size();
                    g_Daemon.SelectOverlayFrame(nextIdx);
                    if (!g_Daemon.overlayFrameStripCollapsed) {
                        g_Daemon.EnsureActiveFrameVisibleInStrip();
                        g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                    }
                    int delayMs = (nextIdx < g_Daemon.overlayFrameDelaysMs.size()) ? g_Daemon.overlayFrameDelaysMs[nextIdx] : 80;
                    delayMs = std::max(20, std::min(60000, delayMs));
                    SetTimer(hWnd, 3, (UINT)delayMs, nullptr);
                    InvalidateRect(hWnd, nullptr, FALSE);
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

        case WM_RBUTTONDOWN: {
            SetFocus(hWnd);
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);
            POINT pt{ mx, my };
            g_Daemon.mousePt = pt;

            if (g_Daemon.HasOverlayMultiFrames()) {
                g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                if (PtInRect(&g_Daemon.overlayFrameStripToggleBtnRect, pt) ||
                    (!g_Daemon.overlayFrameStripCollapsed &&
                     (PtInRect(&g_Daemon.overlayFrameStripResizeRect, pt) ||
                      PtInRect(&g_Daemon.overlayFrameStripPanelRect, pt)))) {
                    return 0;
                }
            }
            if (g_Daemon.hasSelection &&
                (PtInRect(&g_Daemon.customStrokeRect, pt) || PtInRect(&g_Daemon.dimPillRect, pt))) {
                return 0;
            }
            for (const auto& b : g_Daemon.dockButtons) {
                if (PtInRect(&b.rect, pt)) {
                    return 0;
                }
            }

            auto autoHideFrameStripOnFocus = [&]() {
                if (g_Daemon.autoHideFrameList && g_Daemon.HasOverlayMultiFrames() && !g_Daemon.overlayFrameStripCollapsed) {
                    g_Daemon.overlayFrameStripCollapsed = true;
                    g_Daemon.overlayHoveredFrameIdx = -1;
                    g_Daemon.overlayTopResizeHovered = false;
                    g_Daemon.overlayScrollbarHovered = false;
                    g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                }
            };

            if (g_Daemon.isEditingStroke) g_Daemon.CommitActiveStrokeInput();
            if (g_Daemon.isEditingSize)   g_Daemon.CommitActiveSizeInput();
            if (g_Daemon.isEditingText)   g_Daemon.CommitActiveTextBox();

            // 1. Right-click drag on any of the 8 selection resize handles constrains resize to a 1:1 square
            DragMode handleHit = g_Daemon.HitTestSelectionHandles(mx, my);
            if (handleHit != DragMode::None) {
                autoHideFrameStripOnFocus();
                g_Daemon.isRightClickSelectionDrag = true;
                g_Daemon.dragMode = handleHit;
                g_Daemon.dragStartPt = pt;
                g_Daemon.dragOrigRect = {
                    std::min(g_Daemon.selRect.left, g_Daemon.selRect.right),
                    std::min(g_Daemon.selRect.top, g_Daemon.selRect.bottom),
                    std::max(g_Daemon.selRect.left, g_Daemon.selRect.right),
                    std::max(g_Daemon.selRect.top, g_Daemon.selRect.bottom)
                };
                g_Daemon.selectionTargetPt = pt;
                SetCapture(hWnd);
                SendMessageW(hWnd, WM_MOUSEMOVE, MK_RBUTTON, MAKELPARAM(mx, my));
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }

            // 2. If inside existing selection box, ignore right-click
            if (g_Daemon.hasSelection) {
                RECT normSel = {
                    std::min(g_Daemon.selRect.left, g_Daemon.selRect.right),
                    std::min(g_Daemon.selRect.top, g_Daemon.selRect.bottom),
                    std::max(g_Daemon.selRect.left, g_Daemon.selRect.right),
                    std::max(g_Daemon.selRect.top, g_Daemon.selRect.bottom)
                };
                if (PtInRect(&normSel, pt)) {
                    return 0;
                }
            }

            // 3. Outside selection box (or before selecting): Right-click drag constrains new selection to a 1:1 square
            g_Daemon.ClearAnnotationSelection();
            g_Daemon.isRightClickSelectionDrag = true;
            g_Daemon.pendingShiftColorPick = false;
            g_Daemon.pendingCtrlWindowSelect = false;
            g_Daemon.dragMode = DragMode::PendingOutsideSelection;
            g_Daemon.dragStartPt = pt;
            SetCapture(hWnd);
            g_Daemon.UpdateOverlayCursor(mx, my);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_RBUTTONUP: {
            if (g_Daemon.isRightClickSelectionDrag) {
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                g_Daemon.mousePt = { mx, my };
                if (g_Daemon.dragMode == DragMode::CreatingSelection) {
                    int w = std::abs(g_Daemon.selRect.right - g_Daemon.selRect.left);
                    int h = std::abs(g_Daemon.selRect.bottom - g_Daemon.selRect.top);
                    if (w < 1 || h < 1) g_Daemon.hasSelection = false;
                }
                g_Daemon.isRightClickSelectionDrag = false;
                g_Daemon.pendingShiftColorPick = false;
                g_Daemon.pendingCtrlWindowSelect = false;
                g_Daemon.dragMode = DragMode::None;
                ReleaseCapture();
                g_Daemon.UpdateOverlayCursor(mx, my);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (g_Daemon.HasOverlayMultiFrames() && !g_Daemon.overlayFrameStripCollapsed &&
                PtInRect(&g_Daemon.overlayFrameStripPanelRect, g_Daemon.mousePt)) {
                float step = (delta > 0) ? -108.0f : 108.0f;
                g_Daemon.overlayFrameScrollX = std::max(0.0f, std::min(g_Daemon.overlayFrameMaxScrollX, g_Daemon.overlayFrameScrollX + step));
                g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            bool shiftHeld = ((GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT) != 0) ||
                             ((GetKeyState(VK_SHIFT) & 0x8000) != 0) ||
                             ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) ||
                             ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) ||
                             ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0);
            if (!g_Daemon.hasSelection && shiftHeld && delta != 0) {
                int mag = g_Daemon.loupeMagnification;
                if (delta > 0) {
                    if (mag < 10) mag += 1;
                    else if (mag < 20) mag += 2;
                    else if (mag < 50) mag += 5;
                    else if (mag < 90) mag += 10;
                    else mag += 30;
                    mag = std::min(210, mag);
                } else {
                    if (mag > 90) mag -= 30;
                    else if (mag > 50) mag -= 10;
                    else if (mag > 20) mag -= 5;
                    else if (mag > 10) mag -= 2;
                    else mag -= 1;
                    mag = std::max(1, mag);
                }
                if (mag != g_Daemon.loupeMagnification) {
                    g_Daemon.loupeMagnification = mag;
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
            if (!g_Daemon.hasSelection) {
                return 0;
            }
            float nextS = std::max(1.0f, std::min(120.0f, g_Daemon.activeStroke + (delta > 0 ? 1.5f : -1.5f)));
            g_Daemon.ApplyStrokeToSelectedAnnotation(nextS, false);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_MOUSEHWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (g_Daemon.HasOverlayMultiFrames() && !g_Daemon.overlayFrameStripCollapsed &&
                PtInRect(&g_Daemon.overlayFrameStripPanelRect, g_Daemon.mousePt)) {
                float step = (delta > 0) ? 108.0f : -108.0f;
                g_Daemon.overlayFrameScrollX = std::max(0.0f, std::min(g_Daemon.overlayFrameMaxScrollX, g_Daemon.overlayFrameScrollX + step));
                g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            break;
        }

        case WM_LBUTTONDBLCLK: {
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);
            POINT pt{ mx, my };
            if (g_Daemon.HasOverlayMultiFrames()) {
                g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                if (PtInRect(&g_Daemon.overlayFrameStripToggleBtnRect, pt)) {
                    g_Daemon.overlayFramePressedItem = PtInRect(&g_Daemon.overlayFrameStripPlayBtnRect, pt)
                        ? PepperSnapDaemon::FrameStripPressedItem::PlayPause
                        : PepperSnapDaemon::FrameStripPressedItem::ToggleCollapse;
                    SetCapture(hWnd);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                if (!g_Daemon.overlayFrameStripCollapsed && PtInRect(&g_Daemon.overlayFrameStripPanelRect, pt)) {
                    if (PtInRect(&g_Daemon.overlayFrameStripScrollLeftRect, pt)) {
                        g_Daemon.overlayFramePressedItem = PepperSnapDaemon::FrameStripPressedItem::ScrollLeft;
                        SetCapture(hWnd);
                        InvalidateRect(hWnd, nullptr, FALSE);
                        return 0;
                    }
                    if (PtInRect(&g_Daemon.overlayFrameStripScrollRightRect, pt)) {
                        g_Daemon.overlayFramePressedItem = PepperSnapDaemon::FrameStripPressedItem::ScrollRight;
                        SetCapture(hWnd);
                        InvalidateRect(hWnd, nullptr, FALSE);
                        return 0;
                    }
                    for (size_t i = 0; i < g_Daemon.overlayFrameCardRects.size(); ++i) {
                        if (PtInRect(&g_Daemon.overlayFrameCardRects[i], pt)) {
                            g_Daemon.overlayFramePressedItem = PepperSnapDaemon::FrameStripPressedItem::FrameCard;
                            g_Daemon.overlayFramePressedCardIdx = (int)i;
                            SetCapture(hWnd);
                            InvalidateRect(hWnd, nullptr, FALSE);
                            return 0;
                        }
                    }
                    return 0;
                }
            }
            for (const auto& b : g_Daemon.dockButtons) {
                if (PtInRect(&b.rect, pt) && b.id != DBTN_ACT_DRAG_HUD) {
                    g_Daemon.pressedBtnId = b.id;
                    g_Daemon.hoveredBtnId = b.id;
                    SetCapture(hWnd);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
            }
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
                size_t idx = PepperSnapDaemon::HitTestMonoIndex(mx, g_Daemon.dimPillRect, g_Daemon.editingSizeText, L" px");
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

            // -1. Multi-Frame GIF / WebP Selector Strip (on top of custom area and its toolbar)
            if (g_Daemon.HasOverlayMultiFrames()) {
                g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                if (PtInRect(&g_Daemon.overlayFrameStripToggleBtnRect, pt)) {
                    g_Daemon.CommitActiveTextBox();
                    g_Daemon.CommitActiveSizeInput();
                    g_Daemon.CommitActiveStrokeInput();
                    g_Daemon.overlayFramePressedItem = PtInRect(&g_Daemon.overlayFrameStripPlayBtnRect, pt)
                        ? PepperSnapDaemon::FrameStripPressedItem::PlayPause
                        : PepperSnapDaemon::FrameStripPressedItem::ToggleCollapse;
                    SetCapture(hWnd);
                    g_Daemon.UpdateOverlayCursor(mx, my);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                if (!g_Daemon.overlayFrameStripCollapsed) {
                    if (PtInRect(&g_Daemon.overlayFrameStripResizeRect, pt)) {
                        g_Daemon.CommitActiveTextBox();
                        g_Daemon.CommitActiveSizeInput();
                        g_Daemon.CommitActiveStrokeInput();
                        g_Daemon.dragMode = DragMode::ResizingFrameStrip;
                        g_Daemon.dragFrameStripStartY = my;
                        g_Daemon.dragFrameStripOrigH = g_Daemon.overlayFrameStripHeight;
                        SetCapture(hWnd);
                        g_Daemon.UpdateOverlayCursor(mx, my);
                        InvalidateRect(hWnd, nullptr, FALSE);
                        return 0;
                    }
                    if (PtInRect(&g_Daemon.overlayFrameStripPanelRect, pt)) {
                        g_Daemon.CommitActiveTextBox();
                        g_Daemon.CommitActiveSizeInput();
                        g_Daemon.CommitActiveStrokeInput();
                        if (PtInRect(&g_Daemon.overlayFrameStripScrollLeftRect, pt)) {
                            g_Daemon.overlayFramePressedItem = PepperSnapDaemon::FrameStripPressedItem::ScrollLeft;
                            SetCapture(hWnd);
                            InvalidateRect(hWnd, nullptr, FALSE);
                            return 0;
                        }
                        if (PtInRect(&g_Daemon.overlayFrameStripScrollRightRect, pt)) {
                            g_Daemon.overlayFramePressedItem = PepperSnapDaemon::FrameStripPressedItem::ScrollRight;
                            SetCapture(hWnd);
                            InvalidateRect(hWnd, nullptr, FALSE);
                            return 0;
                        }
                        if (PtInRect(&g_Daemon.overlayFrameStripScrollThumbRect, pt)) {
                            g_Daemon.dragMode = DragMode::ScrollingFrameStrip;
                            g_Daemon.dragFrameScrollStartX = mx;
                            g_Daemon.dragFrameScrollOrigX = g_Daemon.overlayFrameScrollX;
                            SetCapture(hWnd);
                            g_Daemon.UpdateOverlayCursor(mx, my);
                            InvalidateRect(hWnd, nullptr, FALSE);
                            return 0;
                        }
                        if (PtInRect(&g_Daemon.overlayFrameStripScrollTrackRect, pt)) {
                            int trackW = std::max(1, (int)(g_Daemon.overlayFrameStripScrollTrackRect.right - g_Daemon.overlayFrameStripScrollTrackRect.left));
                            int thumbW = std::max(24, (int)(g_Daemon.overlayFrameStripScrollThumbRect.right - g_Daemon.overlayFrameStripScrollThumbRect.left));
                            float ratio = (float)(mx - g_Daemon.overlayFrameStripScrollTrackRect.left - thumbW / 2) / (float)std::max(1, trackW - thumbW);
                            ratio = std::max(0.0f, std::min(1.0f, ratio));
                            g_Daemon.overlayFrameScrollX = ratio * g_Daemon.overlayFrameMaxScrollX;
                            g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                            g_Daemon.dragMode = DragMode::ScrollingFrameStrip;
                            g_Daemon.dragFrameScrollStartX = mx;
                            g_Daemon.dragFrameScrollOrigX = g_Daemon.overlayFrameScrollX;
                            SetCapture(hWnd);
                            InvalidateRect(hWnd, nullptr, FALSE);
                            return 0;
                        }
                        for (size_t i = 0; i < g_Daemon.overlayFrameCardRects.size(); ++i) {
                            if (PtInRect(&g_Daemon.overlayFrameCardRects[i], pt)) {
                                g_Daemon.overlayFramePressedItem = PepperSnapDaemon::FrameStripPressedItem::FrameCard;
                                g_Daemon.overlayFramePressedCardIdx = (int)i;
                                SetCapture(hWnd);
                                InvalidateRect(hWnd, nullptr, FALSE);
                                return 0;
                            }
                        }
                        return 0;
                    }
                }
            }

            // Auto-hide the frame list whenever the user focuses on the custom area or its toolbar (when enabled in Options)
            auto autoHideFrameStripOnFocus = [&]() {
                if (g_Daemon.autoHideFrameList && g_Daemon.HasOverlayMultiFrames() && !g_Daemon.overlayFrameStripCollapsed) {
                    g_Daemon.overlayFrameStripCollapsed = true;
                    g_Daemon.overlayHoveredFrameIdx = -1;
                    g_Daemon.overlayTopResizeHovered = false;
                    g_Daemon.overlayScrollbarHovered = false;
                    g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                }
            };

            // 0a. Check Typeable Custom Stroke / Text Size Input Box (Right of [XL])
            if (g_Daemon.hasSelection && PtInRect(&g_Daemon.customStrokeRect, pt)) {
                autoHideFrameStripOnFocus();
                g_Daemon.CommitActiveTextBox();
                g_Daemon.CommitActiveSizeInput();
                if (!g_Daemon.isEditingStroke) {
                    g_Daemon.isEditingStroke = true;
                    g_Daemon.editingStrokeText = std::to_wstring((int)std::round(g_Daemon.activeStroke));
                }
                size_t idx = PepperSnapDaemon::HitTestMonoIndex(mx, g_Daemon.customStrokeRect, g_Daemon.editingStrokeText, L" px");
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
                autoHideFrameStripOnFocus();
                g_Daemon.CommitActiveTextBox();
                if (!g_Daemon.isEditingSize) {
                    int sw = std::abs(g_Daemon.selRect.right - g_Daemon.selRect.left);
                    int sh = std::abs(g_Daemon.selRect.bottom - g_Daemon.selRect.top);
                    g_Daemon.isEditingSize = true;
                    g_Daemon.editingSizeText = std::to_wstring(sw) + L"x" + std::to_wstring(sh);
                }
                size_t idx = PepperSnapDaemon::HitTestMonoIndex(mx, g_Daemon.dimPillRect, g_Daemon.editingSizeText, L" px");
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
                    autoHideFrameStripOnFocus();
                    if (b.id == DBTN_ACT_DRAG_HUD) {
                        g_Daemon.CommitActiveTextBox();
                        if (!g_Daemon.hasCustomHudPos) {
                            g_Daemon.customHudPos = { g_Daemon.hudBoundsRect.right, g_Daemon.hudBoundsRect.top };
                            g_Daemon.hasCustomHudPos = true;
                        }
                        g_Daemon.hoveredBtnId = -1;
                        g_Daemon.pressedBtnId = -1;
                        g_Daemon.dragMode = DragMode::DraggingHUD;
                        g_Daemon.dragHudStartMouse = pt;
                        g_Daemon.dragHudOrigPos = g_Daemon.customHudPos;
                        SetCapture(hWnd);
                        g_Daemon.UpdateOverlayCursor(mx, my);
                        InvalidateRect(hWnd, nullptr, FALSE);
                        return 0;
                    }
                    g_Daemon.pressedBtnId = b.id;
                    g_Daemon.hoveredBtnId = b.id;
                    SetCapture(hWnd);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
            }

            // If currently editing a text box, check if clicking inside it to move caret / drag-select!
            if (g_Daemon.isEditingText) {
                RECT tbRc = g_Daemon.GetEditingTextBoxRect();
                if (PtInRect(&tbRc, pt)) {
                    autoHideFrameStripOnFocus();
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
                autoHideFrameStripOnFocus();
                g_Daemon.isRightClickSelectionDrag = false;
                g_Daemon.dragMode = handleHit;
                g_Daemon.dragStartPt = pt;
                g_Daemon.dragOrigRect = {
                    std::min(g_Daemon.selRect.left, g_Daemon.selRect.right),
                    std::min(g_Daemon.selRect.top, g_Daemon.selRect.bottom),
                    std::max(g_Daemon.selRect.left, g_Daemon.selRect.right),
                    std::max(g_Daemon.selRect.top, g_Daemon.selRect.bottom)
                };
                g_Daemon.selectionTargetPt = pt;
                SetCapture(hWnd);
                SendMessageW(hWnd, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(mx, my));
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
                    autoHideFrameStripOnFocus();
                    OverlayTool effectiveTool = g_Daemon.GetEffectiveTool();
                    if (effectiveTool == OverlayTool::SelectMove) {
                        bool multiMod = shiftHeld;
                        DragMode annDragMode = DragMode::MovingAnnotation;
                        int hitId = g_Daemon.HitTestAnnotation((float)mx, (float)my, &annDragMode);
                        g_Daemon.moveAnnUndoPushed = false;
                        g_Daemon.clickedAlreadySelectedAnn = false;
                        g_Daemon.clickedAlreadySelectedAnnId = -1;
                        if (hitId != -1) {
                            if (multiMod) {
                                g_Daemon.ToggleAnnotationSelection(hitId);
                                if (g_Daemon.IsAnnotationSelected(hitId)) {
                                    g_Daemon.dragMode = DragMode::MovingAnnotation;
                                    g_Daemon.dragStartPt = pt;
                                    for (const auto& a : g_Daemon.annotations) {
                                        if (a.id == hitId) {
                                            g_Daemon.activeStroke = a.strokeWidth;
                                            g_Daemon.activeColor = a.color;
                                            g_Daemon.moveAnnOffset = { (float)mx - a.startPt.x, (float)my - a.startPt.y };
                                            break;
                                        }
                                    }
                                } else {
                                    g_Daemon.dragMode = DragMode::None;
                                }
                            } else {
                                if (g_Daemon.IsAnnotationSelected(hitId) && g_Daemon.selectedAnnotationIds.size() > 1) {
                                    // Keep the multi-selection intact so dragging moves all selected annotations together;
                                    // if the user releases without dragging, select only this clicked annotation.
                                    g_Daemon.selectedAnnotationId = hitId;
                                    g_Daemon.clickedAlreadySelectedAnn = true;
                                    g_Daemon.clickedAlreadySelectedAnnId = hitId;
                                    g_Daemon.dragMode = DragMode::MovingAnnotation;
                                } else {
                                    g_Daemon.SelectSingleAnnotation(hitId);
                                    g_Daemon.dragMode = annDragMode;
                                }
                                g_Daemon.dragStartPt = pt;
                                for (const auto& a : g_Daemon.annotations) {
                                    if (a.id == hitId) {
                                        g_Daemon.activeStroke = a.strokeWidth;
                                        g_Daemon.activeColor = a.color;
                                        g_Daemon.moveAnnOffset = { (float)mx - a.startPt.x, (float)my - a.startPt.y };
                                        break;
                                    }
                                }
                            }
                        } else if (multiMod) {
                            // Shift + drag (or Ctrl + drag) on empty space inside custom area starts marquee multi-selection!
                            g_Daemon.dragMode = DragMode::MarqueeSelectingAnnotations;
                            g_Daemon.marqueeStartPt = pt;
                            g_Daemon.marqueeRect = { pt.x, pt.y, pt.x, pt.y };
                            g_Daemon.hasMarqueeBox = true;
                        } else {
                            g_Daemon.ClearAnnotationSelection();
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
                    g_Daemon.ClearAnnotationSelection();
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

            // 4. Outside Selection Box -> Enter PendingOutsideSelection
            // (Shift + click picks pixel RGB color; Ctrl + click selects window on WM_LBUTTONUP release if not dragged;
            //  manual region creation starts in WM_MOUSEMOVE only if dragged >= 5px)
            g_Daemon.ClearAnnotationSelection();
            g_Daemon.isRightClickSelectionDrag = false;
            g_Daemon.dragMode = DragMode::PendingOutsideSelection;
            g_Daemon.dragStartPt = pt;
            bool ctrlHeldOnDown = ((GetKeyState(VK_CONTROL) & 0x8000) != 0) ||
                                  ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) ||
                                  ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0) ||
                                  ((GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0);
            bool shiftHeldOnDown = ((GetKeyState(VK_SHIFT) & 0x8000) != 0) ||
                                   ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) ||
                                   ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) ||
                                   ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0);
            if (!g_Daemon.hasSelection && shiftHeldOnDown && !ctrlHeldOnDown) {
                g_Daemon.pendingShiftColorPick = true;
                g_Daemon.pendingCtrlWindowSelect = false;
            } else if (!g_Daemon.hasSelection && ctrlHeldOnDown) {
                g_Daemon.pendingShiftColorPick = false;
                g_Daemon.UpdateCtrlWindowHover();
                g_Daemon.pendingCtrlWindowSelect = g_Daemon.hasCtrlHoverWindow;
                g_Daemon.pendingCtrlWindowRect = g_Daemon.ctrlHoverWindowRect;
            } else {
                g_Daemon.pendingShiftColorPick = false;
                g_Daemon.pendingCtrlWindowSelect = false;
            }
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

            if (g_Daemon.pressedBtnId != -1) {
                int newHover = -1;
                for (const auto& b : g_Daemon.dockButtons) {
                    if (b.id == g_Daemon.pressedBtnId && PtInRect(&b.rect, pt)) {
                        newHover = b.id;
                        break;
                    }
                }
                if (newHover != g_Daemon.hoveredBtnId) {
                    g_Daemon.hoveredBtnId = newHover;
                    g_Daemon.UpdateOverlayCursor(mx, my);
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }

            if (g_Daemon.isDraggingTextBoxSel && g_Daemon.isEditingText) {
                g_Daemon.textCaretPos = g_Daemon.HitTestTextBoxIndex(mx, my);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.isDraggingStrokeText) {
                g_Daemon.strokeCaretPos = PepperSnapDaemon::HitTestMonoIndex(
                    mx, g_Daemon.customStrokeRect, g_Daemon.editingStrokeText, L" px"
                );
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.isDraggingSizeText) {
                g_Daemon.sizeCaretPos = PepperSnapDaemon::HitTestMonoIndex(
                    mx, g_Daemon.dimPillRect, g_Daemon.editingSizeText, L" px"
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
                bool overFrameStripUI = false;
                bool frameStripHoverChanged = false;
                if (g_Daemon.HasOverlayMultiFrames()) {
                    g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                    bool inToggleSq = (PtInRect(&g_Daemon.overlayFrameStripToggleBtnRect, pt) != FALSE);
                    bool newPlayHov   = (inToggleSq && PtInRect(&g_Daemon.overlayFrameStripPlayBtnRect, pt) != FALSE);
                    bool newToggleHov = (inToggleSq && !newPlayHov);
                    bool newResizeHov = (!g_Daemon.overlayFrameStripCollapsed && !inToggleSq && PtInRect(&g_Daemon.overlayFrameStripResizeRect, pt) != FALSE);
                    bool newScrollHov = (!g_Daemon.overlayFrameStripCollapsed && PtInRect(&g_Daemon.overlayFrameStripScrollTrackRect, pt) != FALSE);
                    bool newLeftHov   = (!g_Daemon.overlayFrameStripCollapsed && PtInRect(&g_Daemon.overlayFrameStripScrollLeftRect, pt) != FALSE);
                    bool newRightHov  = (!g_Daemon.overlayFrameStripCollapsed && PtInRect(&g_Daemon.overlayFrameStripScrollRightRect, pt) != FALSE);
                    int newCardHov = -1;
                    if (!g_Daemon.overlayFrameStripCollapsed && !newResizeHov && !newScrollHov && !newLeftHov && !newRightHov &&
                        PtInRect(&g_Daemon.overlayFrameStripPanelRect, pt)) {
                        for (size_t i = 0; i < g_Daemon.overlayFrameCardRects.size(); ++i) {
                            if (PtInRect(&g_Daemon.overlayFrameCardRects[i], pt)) {
                                newCardHov = (int)i;
                                break;
                            }
                        }
                    }
                    overFrameStripUI = inToggleSq || newResizeHov ||
                                       (!g_Daemon.overlayFrameStripCollapsed && PtInRect(&g_Daemon.overlayFrameStripPanelRect, pt));
                    if (newToggleHov != g_Daemon.overlayToggleBtnHovered ||
                        newPlayHov != g_Daemon.overlayPlayBtnHovered ||
                        newResizeHov != g_Daemon.overlayTopResizeHovered ||
                        newScrollHov != g_Daemon.overlayScrollbarHovered ||
                        newLeftHov != g_Daemon.overlayScrollLeftBtnHovered ||
                        newRightHov != g_Daemon.overlayScrollRightBtnHovered ||
                        newCardHov != g_Daemon.overlayHoveredFrameIdx) {
                        frameStripHoverChanged = true;
                        g_Daemon.overlayToggleBtnHovered = newToggleHov;
                        g_Daemon.overlayPlayBtnHovered = newPlayHov;
                        g_Daemon.overlayTopResizeHovered = newResizeHov;
                        g_Daemon.overlayScrollbarHovered = newScrollHov;
                        g_Daemon.overlayScrollLeftBtnHovered = newLeftHov;
                        g_Daemon.overlayScrollRightBtnHovered = newRightHov;
                        g_Daemon.overlayHoveredFrameIdx = newCardHov;
                    }
                }
                int newHover = -1;
                if (!overFrameStripUI) {
                    for (const auto& b : g_Daemon.dockButtons) {
                        if (PtInRect(&b.rect, pt)) {
                            newHover = b.id;
                            break;
                        }
                    }
                }
                bool pillHover = !overFrameStripUI && g_Daemon.hasSelection && ( PtInRect(&g_Daemon.dimPillRect, pt) != FALSE );
                bool strokeHover = !overFrameStripUI && g_Daemon.hasSelection && ( PtInRect(&g_Daemon.customStrokeRect, pt) != FALSE );
                bool hoverChanged = frameStripHoverChanged ||
                                    (newHover != g_Daemon.hoveredBtnId) ||
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
            bool selSquareConstrain = g_Daemon.isRightClickSelectionDrag || ((wParam & MK_RBUTTON) != 0);

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
                g_Daemon.selectionTargetPt = { tx, ty };
            };

            switch (g_Daemon.dragMode) {
                case DragMode::ResizingFrameStrip: {
                    int maxStripH = std::max(160, (int)(g_Daemon.vScreenH * 0.48));
                    int newH = g_Daemon.dragFrameStripOrigH + (g_Daemon.dragFrameStripStartY - my);
                    g_Daemon.overlayFrameStripHeight = std::max(92, std::min(maxStripH, newH));
                    g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                    break;
                }
                case DragMode::ScrollingFrameStrip: {
                    int trackW = std::max(1, (int)(g_Daemon.overlayFrameStripScrollTrackRect.right - g_Daemon.overlayFrameStripScrollTrackRect.left));
                    int thumbW = std::max(24, (int)(g_Daemon.overlayFrameStripScrollThumbRect.right - g_Daemon.overlayFrameStripScrollThumbRect.left));
                    int usableTrack = std::max(1, trackW - thumbW);
                    float deltaScroll = ((float)(mx - g_Daemon.dragFrameScrollStartX) / (float)usableTrack) * g_Daemon.overlayFrameMaxScrollX;
                    g_Daemon.overlayFrameScrollX = std::max(0.0f, std::min(g_Daemon.overlayFrameMaxScrollX, g_Daemon.dragFrameScrollOrigX + deltaScroll));
                    g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                    break;
                }
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
                case DragMode::PendingOutsideSelection: {
                    if (!g_Daemon.isRightClickSelectionDrag) {
                        bool ctrlHeldMove = ((GetKeyState(VK_CONTROL) & 0x8000) != 0) ||
                                            ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) ||
                                            ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0) ||
                                            ((GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0);
                        bool shiftHeldMove = ((GetKeyState(VK_SHIFT) & 0x8000) != 0) ||
                                             ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) ||
                                             ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) ||
                                             ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0);
                        if (!g_Daemon.hasSelection && (shiftHeldMove || g_Daemon.pendingShiftColorPick) && !ctrlHeldMove) {
                            InvalidateRect(hWnd, nullptr, FALSE);
                            return 0;
                        }
                        if (!g_Daemon.hasSelection && ctrlHeldMove) {
                            if (g_Daemon.UpdateCtrlWindowHover()) {
                                g_Daemon.pendingCtrlWindowSelect = g_Daemon.hasCtrlHoverWindow;
                                g_Daemon.pendingCtrlWindowRect = g_Daemon.ctrlHoverWindowRect;
                                g_Daemon.UpdateOverlayCursor(mx, my);
                                InvalidateRect(hWnd, nullptr, FALSE);
                            }
                            return 0;
                        }
                    }
                    if (std::abs(dx) >= 5 || std::abs(dy) >= 5) {
                        g_Daemon.pendingShiftColorPick = false;
                        g_Daemon.pendingCtrlWindowSelect = false;
                        if (g_Daemon.autoHideFrameList && g_Daemon.HasOverlayMultiFrames() && !g_Daemon.overlayFrameStripCollapsed) {
                            g_Daemon.overlayFrameStripCollapsed = true;
                            g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                        }
                        g_Daemon.hasSelection = true;
                        g_Daemon.hasCustomHudPos = false;
                        g_Daemon.activeTool = OverlayTool::SelectMove;
                        g_Daemon.annotations.clear();
                        g_Daemon.undoStack.clear();
                        g_Daemon.redoStack.clear();
                        g_Daemon.nextStepNum = 1;
                        g_Daemon.ClearAnnotationSelection();
                        g_Daemon.dragMode = DragMode::CreatingSelection;
                        int startX = std::max(0, std::min(g_Daemon.vScreenW, (int)g_Daemon.dragStartPt.x));
                        int startY = std::max(0, std::min(g_Daemon.vScreenH, (int)g_Daemon.dragStartPt.y));
                        g_Daemon.dragOrigRect = { startX, startY, startX, startY };
                        if (selSquareConstrain) {
                            applySquareCorner(startX, startY);
                        } else {
                            g_Daemon.selRect = { startX, startY, clampedMx, clampedMy };
                            g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
                        }
                    } else {
                        return 0;
                    }
                    break;
                }
                case DragMode::CreatingSelection:
                    g_Daemon.hasCustomHudPos = false;
                    if (selSquareConstrain) {
                        int startX = std::max(0, std::min(g_Daemon.vScreenW, (int)g_Daemon.dragStartPt.x));
                        int startY = std::max(0, std::min(g_Daemon.vScreenH, (int)g_Daemon.dragStartPt.y));
                        applySquareCorner(startX, startY);
                    } else {
                        int startX = std::max(0, std::min(g_Daemon.vScreenW, (int)g_Daemon.dragStartPt.x));
                        int startY = std::max(0, std::min(g_Daemon.vScreenH, (int)g_Daemon.dragStartPt.y));
                        g_Daemon.selRect = { startX, startY, clampedMx, clampedMy };
                        g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
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
                    if (selSquareConstrain) {
                        applySquareCorner(g_Daemon.dragOrigRect.right, g_Daemon.dragOrigRect.bottom);
                    } else {
                        g_Daemon.selRect = { clampedMx, clampedMy, g_Daemon.dragOrigRect.right, g_Daemon.dragOrigRect.bottom };
                        g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
                    }
                    break;
                case DragMode::ResizeT:
                    g_Daemon.hasCustomHudPos = false;
                    if (selSquareConstrain) {
                        int anchorX = g_Daemon.dragOrigRect.left, anchorY = g_Daemon.dragOrigRect.bottom;
                        int sdy = clampedMy - anchorY;
                        int side = std::min(std::abs(sdy), std::min(g_Daemon.vScreenW - anchorX, sdy >= 0 ? (g_Daemon.vScreenH - anchorY) : anchorY));
                        int ty = anchorY + (sdy >= 0 ? side : -side);
                        int tx = anchorX + side;
                        g_Daemon.selRect = { anchorX, std::min(anchorY, ty), tx, std::max(anchorY, ty) };
                        g_Daemon.selectionTargetPt = { tx, ty };
                    } else {
                        g_Daemon.selRect = { g_Daemon.dragOrigRect.left, clampedMy, g_Daemon.dragOrigRect.right, g_Daemon.dragOrigRect.bottom };
                        g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
                    }
                    break;
                case DragMode::ResizeTR:
                    g_Daemon.hasCustomHudPos = false;
                    if (selSquareConstrain) {
                        applySquareCorner(g_Daemon.dragOrigRect.left, g_Daemon.dragOrigRect.bottom);
                    } else {
                        g_Daemon.selRect = { g_Daemon.dragOrigRect.left, clampedMy, clampedMx, g_Daemon.dragOrigRect.bottom };
                        g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
                    }
                    break;
                case DragMode::ResizeR:
                    g_Daemon.hasCustomHudPos = false;
                    if (selSquareConstrain) {
                        int anchorX = g_Daemon.dragOrigRect.left, anchorY = g_Daemon.dragOrigRect.top;
                        int sdx = clampedMx - anchorX;
                        int side = std::min(std::abs(sdx), std::min(g_Daemon.vScreenH - anchorY, sdx >= 0 ? (g_Daemon.vScreenW - anchorX) : anchorX));
                        int tx = anchorX + (sdx >= 0 ? side : -side);
                        int ty = anchorY + side;
                        g_Daemon.selRect = { std::min(anchorX, tx), anchorY, std::max(anchorX, tx), ty };
                        g_Daemon.selectionTargetPt = { tx, ty };
                    } else {
                        g_Daemon.selRect = { g_Daemon.dragOrigRect.left, g_Daemon.dragOrigRect.top, clampedMx, g_Daemon.dragOrigRect.bottom };
                        g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
                    }
                    break;
                case DragMode::ResizeBR:
                    g_Daemon.hasCustomHudPos = false;
                    if (selSquareConstrain) {
                        applySquareCorner(g_Daemon.dragOrigRect.left, g_Daemon.dragOrigRect.top);
                    } else {
                        g_Daemon.selRect = { g_Daemon.dragOrigRect.left, g_Daemon.dragOrigRect.top, clampedMx, clampedMy };
                        g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
                    }
                    break;
                case DragMode::ResizeB:
                    g_Daemon.hasCustomHudPos = false;
                    if (selSquareConstrain) {
                        int anchorX = g_Daemon.dragOrigRect.left, anchorY = g_Daemon.dragOrigRect.top;
                        int sdy = clampedMy - anchorY;
                        int side = std::min(std::abs(sdy), std::min(g_Daemon.vScreenW - anchorX, sdy >= 0 ? (g_Daemon.vScreenH - anchorY) : anchorY));
                        int ty = anchorY + (sdy >= 0 ? side : -side);
                        int tx = anchorX + side;
                        g_Daemon.selRect = { anchorX, std::min(anchorY, ty), tx, std::max(anchorY, ty) };
                        g_Daemon.selectionTargetPt = { tx, ty };
                    } else {
                        g_Daemon.selRect = { g_Daemon.dragOrigRect.left, g_Daemon.dragOrigRect.top, g_Daemon.dragOrigRect.right, clampedMy };
                        g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
                    }
                    break;
                case DragMode::ResizeBL:
                    g_Daemon.hasCustomHudPos = false;
                    if (selSquareConstrain) {
                        applySquareCorner(g_Daemon.dragOrigRect.right, g_Daemon.dragOrigRect.top);
                    } else {
                        g_Daemon.selRect = { clampedMx, g_Daemon.dragOrigRect.top, g_Daemon.dragOrigRect.right, clampedMy };
                        g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
                    }
                    break;
                case DragMode::ResizeL:
                    g_Daemon.hasCustomHudPos = false;
                    if (selSquareConstrain) {
                        int anchorX = g_Daemon.dragOrigRect.right, anchorY = g_Daemon.dragOrigRect.top;
                        int sdx = clampedMx - anchorX;
                        int side = std::min(std::abs(sdx), std::min(g_Daemon.vScreenH - anchorY, sdx >= 0 ? (g_Daemon.vScreenW - anchorX) : anchorX));
                        int tx = anchorX + (sdx >= 0 ? side : -side);
                        int ty = anchorY + side;
                        g_Daemon.selRect = { std::min(anchorX, tx), anchorY, std::max(anchorX, tx), ty };
                        g_Daemon.selectionTargetPt = { tx, ty };
                    } else {
                        g_Daemon.selRect = { clampedMx, g_Daemon.dragOrigRect.top, g_Daemon.dragOrigRect.right, g_Daemon.dragOrigRect.bottom };
                        g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
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
                case DragMode::MarqueeSelectingAnnotations: {
                    g_Daemon.marqueeRect = {
                        std::min((LONG)g_Daemon.marqueeStartPt.x, (LONG)clampedMx),
                        std::min((LONG)g_Daemon.marqueeStartPt.y, (LONG)clampedMy),
                        std::max((LONG)g_Daemon.marqueeStartPt.x, (LONG)clampedMx),
                        std::max((LONG)g_Daemon.marqueeStartPt.y, (LONG)clampedMy)
                    };
                    g_Daemon.hasMarqueeBox = true;
                    g_Daemon.selectedAnnotationIds.clear();
                    g_Daemon.selectedAnnotationId = -1;
                    for (const auto& a : g_Daemon.annotations) {
                        if (PepperSnapDaemon::AnnotationIntersectsRect(a, g_Daemon.marqueeRect)) {
                            g_Daemon.selectedAnnotationIds.push_back(a.id);
                            g_Daemon.selectedAnnotationId = a.id;
                        }
                    }
                    break;
                }
                case DragMode::MovingAnnotation:
                case DragMode::DraggingAnnotationStart:
                case DragMode::DraggingAnnotationEnd: {
                    bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    if (!g_Daemon.moveAnnUndoPushed) {
                        if (std::abs(mx - g_Daemon.dragStartPt.x) >= 1 || std::abs(my - g_Daemon.dragStartPt.y) >= 1) {
                            g_Daemon.PushUndo();
                            g_Daemon.moveAnnUndoPushed = true;
                            g_Daemon.clickedAlreadySelectedAnn = false;
                        } else {
                            break;
                        }
                    }
                    if (g_Daemon.dragMode == DragMode::DraggingAnnotationStart ||
                        g_Daemon.dragMode == DragMode::DraggingAnnotationEnd) {
                        for (auto& a : g_Daemon.annotations) {
                            if (a.id == g_Daemon.selectedAnnotationId) {
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
                                break;
                            }
                        }
                    } else {
                        float adx = 0.0f, ady = 0.0f;
                        bool foundPrimary = false;
                        for (const auto& a : g_Daemon.annotations) {
                            if (a.id == g_Daemon.selectedAnnotationId) {
                                adx = ((float)mx - g_Daemon.moveAnnOffset.x) - a.startPt.x;
                                ady = ((float)my - g_Daemon.moveAnnOffset.y) - a.startPt.y;
                                foundPrimary = true;
                                break;
                            }
                        }
                        if (foundPrimary) {
                            for (auto& a : g_Daemon.annotations) {
                                if (g_Daemon.IsAnnotationSelected(a.id)) {
                                    a.startPt.x += adx;
                                    a.startPt.y += ady;
                                    a.endPt.x   += adx;
                                    a.endPt.y   += ady;
                                    for (auto& p : a.points) { p.x += adx; p.y += ady; }
                                }
                            }
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
            if (g_Daemon.overlayFramePressedItem != PepperSnapDaemon::FrameStripPressedItem::None) {
                auto pressedItem = g_Daemon.overlayFramePressedItem;
                int pressedCard = g_Daemon.overlayFramePressedCardIdx;
                g_Daemon.overlayFramePressedItem = PepperSnapDaemon::FrameStripPressedItem::None;
                g_Daemon.overlayFramePressedCardIdx = -1;
                ReleaseCapture();
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                POINT pt{ mx, my };
                g_Daemon.mousePt = pt;
                if (g_Daemon.HasOverlayMultiFrames()) {
                    g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                    switch (pressedItem) {
                        case PepperSnapDaemon::FrameStripPressedItem::PlayPause:
                            if (PtInRect(&g_Daemon.overlayFrameStripToggleBtnRect, pt) &&
                                PtInRect(&g_Daemon.overlayFrameStripPlayBtnRect, pt)) {
                                g_Daemon.ToggleOverlayFramePlayback();
                            }
                            break;
                        case PepperSnapDaemon::FrameStripPressedItem::ToggleCollapse:
                            if (PtInRect(&g_Daemon.overlayFrameStripToggleBtnRect, pt) &&
                                !PtInRect(&g_Daemon.overlayFrameStripPlayBtnRect, pt)) {
                                g_Daemon.overlayFrameStripCollapsed = !g_Daemon.overlayFrameStripCollapsed;
                                if (!g_Daemon.overlayFrameStripCollapsed) {
                                    g_Daemon.EnsureActiveFrameVisibleInStrip();
                                }
                                g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                            }
                            break;
                        case PepperSnapDaemon::FrameStripPressedItem::ScrollLeft:
                            if (!g_Daemon.overlayFrameStripCollapsed &&
                                PtInRect(&g_Daemon.overlayFrameStripScrollLeftRect, pt)) {
                                g_Daemon.overlayFrameScrollX = std::max(0.0f, g_Daemon.overlayFrameScrollX - 160.0f);
                                g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                            }
                            break;
                        case PepperSnapDaemon::FrameStripPressedItem::ScrollRight:
                            if (!g_Daemon.overlayFrameStripCollapsed &&
                                PtInRect(&g_Daemon.overlayFrameStripScrollRightRect, pt)) {
                                g_Daemon.overlayFrameScrollX = std::min(g_Daemon.overlayFrameMaxScrollX, g_Daemon.overlayFrameScrollX + 160.0f);
                                g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                            }
                            break;
                        case PepperSnapDaemon::FrameStripPressedItem::FrameCard:
                            if (!g_Daemon.overlayFrameStripCollapsed &&
                                pressedCard >= 0 && pressedCard < (int)g_Daemon.overlayFrameCardRects.size() &&
                                PtInRect(&g_Daemon.overlayFrameCardRects[pressedCard], pt)) {
                                g_Daemon.StopOverlayFramePlayback();
                                g_Daemon.SelectOverlayFrame((size_t)pressedCard);
                            }
                            break;
                        default:
                            break;
                    }
                }
                g_Daemon.UpdateOverlayCursor(mx, my);
                if (IsWindow(hWnd)) {
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
            if (g_Daemon.pressedBtnId != -1) {
                int releasedBtnId = g_Daemon.pressedBtnId;
                g_Daemon.pressedBtnId = -1;
                ReleaseCapture();
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                POINT pt{ mx, my };
                g_Daemon.mousePt = pt;
                const DockButton* hitBtn = nullptr;
                int newHover = -1;
                for (const auto& b : g_Daemon.dockButtons) {
                    if (PtInRect(&b.rect, pt)) {
                        newHover = b.id;
                        if (b.id == releasedBtnId) {
                            hitBtn = &b;
                        }
                        break;
                    }
                }
                g_Daemon.hoveredBtnId = newHover;
                if (hitBtn) {
                    DockButton copyBtn = *hitBtn;
                    ExecuteOverlayDockButtonAction(hWnd, copyBtn);
                } else if (IsWindow(hWnd)) {
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
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
                } else if (g_Daemon.dragMode == DragMode::PendingOutsideSelection) {
                    int mx = GET_X_LPARAM(lParam);
                    int my = GET_Y_LPARAM(lParam);
                    g_Daemon.mousePt = { mx, my };
                    g_Daemon.dragMode = DragMode::None;
                    bool ctrlHeld = ((GetKeyState(VK_CONTROL) & 0x8000) != 0) ||
                                    ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) ||
                                    ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0) ||
                                    ((GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0);
                    bool shiftHeld = ((GetKeyState(VK_SHIFT) & 0x8000) != 0) ||
                                     ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) ||
                                     ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) ||
                                     ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0);
                    if (!g_Daemon.hasSelection && (shiftHeld || g_Daemon.pendingShiftColorPick) && !ctrlHeld) {
                        g_Daemon.pendingShiftColorPick = false;
                        g_Daemon.pendingCtrlWindowSelect = false;
                        ReleaseCapture();
                        g_Daemon.ActionCopyPixelColorAndClose();
                        return 0;
                    }
                    if (!g_Daemon.hasSelection && (ctrlHeld || g_Daemon.pendingCtrlWindowSelect)) {
                        if (ctrlHeld) {
                            g_Daemon.UpdateCtrlWindowHover();
                        }
                        RECT winRc = (ctrlHeld && g_Daemon.hasCtrlHoverWindow)
                            ? g_Daemon.ctrlHoverWindowRect
                            : g_Daemon.pendingCtrlWindowRect;
                        bool hasWin = (ctrlHeld && g_Daemon.hasCtrlHoverWindow) ||
                                      (g_Daemon.pendingCtrlWindowSelect &&
                                       (winRc.right > winRc.left) && (winRc.bottom > winRc.top));
                        if (hasWin) {
                            g_Daemon.hasSelection = true;
                            g_Daemon.selRect = winRc;
                            g_Daemon.hasCtrlHoverWindow = false;
                            g_Daemon.hasCustomHudPos = false;
                            g_Daemon.activeTool = OverlayTool::SelectMove;
                            g_Daemon.annotations.clear();
                            g_Daemon.undoStack.clear();
                            g_Daemon.redoStack.clear();
                            g_Daemon.nextStepNum = 1;
                            g_Daemon.ClearAnnotationSelection();
                        }
                    }
                    g_Daemon.pendingShiftColorPick = false;
                    g_Daemon.pendingCtrlWindowSelect = false;
                    g_Daemon.UpdateOverlayCursor(mx, my);
                } else if (g_Daemon.dragMode == DragMode::MarqueeSelectingAnnotations) {
                    g_Daemon.hasMarqueeBox = false;
                } else if (g_Daemon.dragMode == DragMode::MovingAnnotation &&
                           g_Daemon.clickedAlreadySelectedAnn && !g_Daemon.moveAnnUndoPushed) {
                    g_Daemon.SelectSingleAnnotation(g_Daemon.clickedAlreadySelectedAnnId);
                    g_Daemon.clickedAlreadySelectedAnn = false;
                    g_Daemon.clickedAlreadySelectedAnnId = -1;
                } else if (g_Daemon.dragMode == DragMode::ResizingFrameStrip) {
                    g_Daemon.SaveSettings();
                }
                g_Daemon.moveAnnUndoPushed = false;
                g_Daemon.clickedAlreadySelectedAnn = false;
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
            if ((wParam == VK_DELETE || wParam == VK_BACK) && g_Daemon.HasSelectedAnnotations()) {
                g_Daemon.PushUndo();
                g_Daemon.annotations.erase(
                    std::remove_if(g_Daemon.annotations.begin(), g_Daemon.annotations.end(),
                                   [](const Annotation& a) { return g_Daemon.IsAnnotationSelected(a.id); }),
                    g_Daemon.annotations.end()
                );
                g_Daemon.RecalcNextStepNum();
                g_Daemon.ClearAnnotationSelection();
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (ctrl && !shift && wParam == 'A' && g_Daemon.hasSelection && g_Daemon.GetEffectiveTool() == OverlayTool::SelectMove) {
                g_Daemon.selectedAnnotationIds.clear();
                g_Daemon.selectedAnnotationId = -1;
                for (const auto& a : g_Daemon.annotations) {
                    g_Daemon.selectedAnnotationIds.push_back(a.id);
                    g_Daemon.selectedAnnotationId = a.id;
                }
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (wParam == VK_CONTROL) {
                if (!g_Daemon.hasSelection && (g_Daemon.dragMode == DragMode::None || g_Daemon.dragMode == DragMode::PendingOutsideSelection)) {
                    g_Daemon.lastCtrlTempSelect = false;
                    if (g_Daemon.UpdateCtrlWindowHover()) {
                        g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                        InvalidateRect(hWnd, nullptr, FALSE);
                    }
                } else if (g_Daemon.HasCreatedCustomArea()) {
                    bool curCtrlTempSelect = g_Daemon.IsCtrlTempSelectActive();
                    if (curCtrlTempSelect != g_Daemon.lastCtrlTempSelect) {
                        g_Daemon.lastCtrlTempSelect = curCtrlTempSelect;
                        g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                        InvalidateRect(hWnd, nullptr, FALSE);
                    }
                }
                return 0;
            }
            bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0 || (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            UINT vk = (UINT)wParam;
            bool isSelectionDragActive =
                (g_Daemon.dragMode == DragMode::PendingOutsideSelection ||
                 g_Daemon.dragMode == DragMode::CreatingSelection ||
                 g_Daemon.dragMode == DragMode::MovingSelection ||
                 g_Daemon.dragMode == DragMode::ResizeTL ||
                 g_Daemon.dragMode == DragMode::ResizeT  ||
                 g_Daemon.dragMode == DragMode::ResizeTR ||
                 g_Daemon.dragMode == DragMode::ResizeR  ||
                 g_Daemon.dragMode == DragMode::ResizeBR ||
                 g_Daemon.dragMode == DragMode::ResizeB  ||
                 g_Daemon.dragMode == DragMode::ResizeBL ||
                 g_Daemon.dragMode == DragMode::ResizeL);
            if ((!g_Daemon.hasSelection || isSelectionDragActive) && !shift && !ctrl && !alt &&
                (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_LEFT || wParam == VK_RIGHT)) {
                int stepX = (wParam == VK_LEFT) ? -1 : ((wParam == VK_RIGHT) ? 1 : 0);
                int stepY = (wParam == VK_UP)   ? -1 : ((wParam == VK_DOWN)  ? 1 : 0);
                int nextX = std::max(0, std::min(g_Daemon.vScreenW - 1, (int)g_Daemon.mousePt.x + stepX));
                int nextY = std::max(0, std::min(g_Daemon.vScreenH - 1, (int)g_Daemon.mousePt.y + stepY));

                if (g_Daemon.dragMode == DragMode::PendingOutsideSelection) {
                    if (g_Daemon.autoHideFrameList && g_Daemon.HasOverlayMultiFrames() && !g_Daemon.overlayFrameStripCollapsed) {
                        g_Daemon.overlayFrameStripCollapsed = true;
                        g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                    }
                    g_Daemon.pendingShiftColorPick = false;
                    g_Daemon.pendingCtrlWindowSelect = false;
                    g_Daemon.hasSelection = true;
                    g_Daemon.hasCustomHudPos = false;
                    g_Daemon.activeTool = OverlayTool::SelectMove;
                    g_Daemon.annotations.clear();
                    g_Daemon.undoStack.clear();
                    g_Daemon.redoStack.clear();
                    g_Daemon.nextStepNum = 1;
                    g_Daemon.ClearAnnotationSelection();
                    g_Daemon.dragMode = DragMode::CreatingSelection;
                    int startX = std::max(0, std::min(g_Daemon.vScreenW, (int)g_Daemon.dragStartPt.x));
                    int startY = std::max(0, std::min(g_Daemon.vScreenH, (int)g_Daemon.dragStartPt.y));
                    g_Daemon.dragOrigRect = { startX, startY, startX, startY };
                }

                // When creating or corner-resizing a 1:1 square via right-click drag, step from the active constrained
                // square corner so every arrow key press changes the square size by 1 pixel immediately.
                if (g_Daemon.isRightClickSelectionDrag &&
                    (g_Daemon.dragMode == DragMode::CreatingSelection ||
                     g_Daemon.dragMode == DragMode::ResizeTL ||
                     g_Daemon.dragMode == DragMode::ResizeTR ||
                     g_Daemon.dragMode == DragMode::ResizeBR ||
                     g_Daemon.dragMode == DragMode::ResizeBL)) {
                    int anchorX = std::max(0, std::min(g_Daemon.vScreenW, (int)g_Daemon.dragStartPt.x));
                    int anchorY = std::max(0, std::min(g_Daemon.vScreenH, (int)g_Daemon.dragStartPt.y));
                    if (g_Daemon.dragMode == DragMode::ResizeTL) {
                        anchorX = g_Daemon.dragOrigRect.right;
                        anchorY = g_Daemon.dragOrigRect.bottom;
                    } else if (g_Daemon.dragMode == DragMode::ResizeTR) {
                        anchorX = g_Daemon.dragOrigRect.left;
                        anchorY = g_Daemon.dragOrigRect.bottom;
                    } else if (g_Daemon.dragMode == DragMode::ResizeBR) {
                        anchorX = g_Daemon.dragOrigRect.left;
                        anchorY = g_Daemon.dragOrigRect.top;
                    } else if (g_Daemon.dragMode == DragMode::ResizeBL) {
                        anchorX = g_Daemon.dragOrigRect.right;
                        anchorY = g_Daemon.dragOrigRect.top;
                    }
                    int curSdx = g_Daemon.mousePt.x - anchorX;
                    int curSdy = g_Daemon.mousePt.y - anchorY;
                    int curSide = std::max(std::abs(curSdx), std::abs(curSdy));
                    int dirX = (curSdx >= 0) ? 1 : -1;
                    int dirY = (curSdy >= 0) ? 1 : -1;
                    if (curSide == 0) {
                        if (stepX != 0) dirX = stepX;
                        if (stepY != 0) dirY = stepY;
                    }
                    int deltaSide = (stepX != 0) ? (stepX * dirX) : (stepY * dirY);
                    int nextSide = std::max(1, curSide + deltaSide);
                    int maxSideX = (dirX >= 0) ? (g_Daemon.vScreenW - anchorX) : anchorX;
                    int maxSideY = (dirY >= 0) ? (g_Daemon.vScreenH - anchorY) : anchorY;
                    nextSide = std::min(nextSide, std::min(maxSideX, maxSideY));
                    nextX = std::max(0, std::min(g_Daemon.vScreenW - 1, anchorX + dirX * nextSide));
                    nextY = std::max(0, std::min(g_Daemon.vScreenH - 1, anchorY + dirY * nextSide));
                }

                g_Daemon.mousePt.x = nextX;
                g_Daemon.mousePt.y = nextY;
                SetCursorPos(g_Daemon.vScreenX + nextX, g_Daemon.vScreenY + nextY);

                WPARAM moveWParam = 0;
                if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) moveWParam |= MK_LBUTTON;
                if ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0 || g_Daemon.isRightClickSelectionDrag) moveWParam |= MK_RBUTTON;
                SendMessageW(hWnd, WM_MOUSEMOVE, moveWParam, MAKELPARAM(nextX, nextY));
                g_Daemon.UpdateOverlayCursor(nextX, nextY);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }

            if (g_Daemon.HasOverlayMultiFrames()) {
                if (MatchesOverlayShortcut(g_Daemon.hkFramePlayPause, vk, ctrl, shift, alt)) {
                    g_Daemon.ToggleOverlayFramePlayback();
                    return 0;
                }
                if (MatchesOverlayShortcut(g_Daemon.hkFrameToggleStrip, vk, ctrl, shift, alt)) {
                    g_Daemon.overlayFrameStripCollapsed = !g_Daemon.overlayFrameStripCollapsed;
                    if (!g_Daemon.overlayFrameStripCollapsed) {
                        g_Daemon.EnsureActiveFrameVisibleInStrip();
                    }
                    g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
            }

            if (MatchesOverlayShortcut(g_Daemon.hkActCopy, vk, ctrl, shift, alt)) {
                if (g_Daemon.hasSelection &&
                    g_Daemon.dragMode != DragMode::CreatingSelection &&
                    g_Daemon.dragMode != DragMode::PendingOutsideSelection) {
                    g_Daemon.ActionCopyAndClose();
                }
                return 0;
            }
            if (MatchesOverlayShortcut(g_Daemon.hkActSaveAs, vk, ctrl, shift, alt)) {
                if (g_Daemon.hasSelection) {
                    g_Daemon.ActionSaveAsAndClose();
                }
                return 0;
            }
            if (MatchesOverlayShortcut(g_Daemon.hkActSave, vk, ctrl, shift, alt)) {
                g_Daemon.ActionQuickSaveAndClose();
                return 0;
            }
            if (MatchesOverlayShortcut(g_Daemon.hkActUndo, vk, ctrl, shift, alt)) {
                g_Daemon.Undo();
                return 0;
            }
            if (MatchesOverlayShortcut(g_Daemon.hkActRedo, vk, ctrl, shift, alt)) {
                g_Daemon.Redo();
                return 0;
            }
            if (MatchesOverlayShortcut(g_Daemon.hkActClear, vk, ctrl, shift, alt)) {
                if (g_Daemon.hasSelection) {
                    g_Daemon.PushUndo();
                    g_Daemon.annotations.clear();
                    g_Daemon.nextStepNum = 1;
                    g_Daemon.ClearAnnotationSelection();
                    g_Daemon.BuildDockedHUD();
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
            if (MatchesOverlayShortcut(g_Daemon.hkActOptions, vk, ctrl, shift, alt)) {
                g_Daemon.ShowOptionsModal();
                if (g_Daemon.hOverlayWnd && IsWindow(hWnd)) InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (MatchesOverlayShortcut(g_Daemon.hkActPin, vk, ctrl, shift, alt)) {
                g_Daemon.ActionPinToDesktop();
                return 0;
            }
            if (MatchesOverlayShortcut(g_Daemon.hkActOcr, vk, ctrl, shift, alt)) {
                g_Daemon.ActionOcrAndClose();
                return 0;
            }
            if (MatchesOverlayShortcut(g_Daemon.hkToolResetNum, vk, ctrl, shift, alt)) {
                if (g_Daemon.hasSelection) {
                    g_Daemon.nextStepNum = 1;
                    g_Daemon.BuildDockedHUD();
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }

            if (MatchesOverlayShortcut(g_Daemon.hkToolSelect, vk, ctrl, shift, alt))         g_Daemon.activeTool = OverlayTool::SelectMove;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolPen, vk, ctrl, shift, alt))       g_Daemon.activeTool = OverlayTool::Pen;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolStabilo, vk, ctrl, shift, alt))   g_Daemon.activeTool = OverlayTool::Highlighter;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolLine, vk, ctrl, shift, alt))      g_Daemon.activeTool = OverlayTool::Line;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolArrow, vk, ctrl, shift, alt))     g_Daemon.activeTool = OverlayTool::Arrow;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolNumber, vk, ctrl, shift, alt))    g_Daemon.activeTool = OverlayTool::NumberArrow;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolRect, vk, ctrl, shift, alt))      g_Daemon.activeTool = OverlayTool::Rectangle;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolEllipse, vk, ctrl, shift, alt))   g_Daemon.activeTool = OverlayTool::Ellipse;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolText, vk, ctrl, shift, alt))      g_Daemon.activeTool = OverlayTool::TextBox;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolMosaicSq, vk, ctrl, shift, alt))  g_Daemon.activeTool = OverlayTool::MosaicSquare;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolMosaicCir, vk, ctrl, shift, alt)) g_Daemon.activeTool = OverlayTool::MosaicCircle;

            g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_KEYUP: {
            if (wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT) {
                g_Daemon.lastShiftColorPick = false;
                if (!g_Daemon.hasSelection) {
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
            if (wParam == VK_CONTROL) {
                bool changed = g_Daemon.UpdateCtrlWindowHover();
                if (g_Daemon.HasCreatedCustomArea()) {
                    bool curCtrlTempSelect = g_Daemon.IsCtrlTempSelectActive();
                    if (curCtrlTempSelect != g_Daemon.lastCtrlTempSelect) {
                        g_Daemon.lastCtrlTempSelect = curCtrlTempSelect;
                        changed = true;
                    }
                } else {
                    g_Daemon.lastCtrlTempSelect = false;
                }
                if (changed) {
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
            KillTimer(hWnd, 3);
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

    bool ctrlHeld = ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) ||
                    ((GetKeyState(VK_CONTROL) & 0x8000) != 0) ||
                    ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0) ||
                    ((GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0);
    if (!ctrlHeld || hasSelection || (dragMode != DragMode::None && dragMode != DragMode::PendingOutsideSelection)) {
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
        ClearOverlayFrames();
        frozenDesktopBmp = CaptureVirtualDesktop();
        hasSelection = false;
        SnapshotDesktopWindows();
    }
    if (!frozenDesktopBmp) return;
    BuildOverlaySurfaceCache();

    isSavingAsModalOpen = false;
    dragMode = DragMode::None;
    hoveredBtnId = -1;
    pressedBtnId = -1;
    hasCustomHudPos = false;
    lastPillHover = false;
    lastStrokeHover = false;
    activeTool = OverlayTool::SelectMove;
    lastCtrlTempSelect = false;
    lastShiftColorPick = false;
    pendingShiftColorPick = false;
    pendingCtrlWindowSelect = false;
    pendingCtrlWindowRect = { 0, 0, 0, 0 };
    loupeMagnification = 5;
    isRightClickSelectionDrag = false;
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
    selectionTargetPt = mousePt;
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
    isSavingAsModalOpen = false;
    RecordLastCustomSelection();
    SaveSettings();
    hasCustomHudPos = false;
    activeTool = OverlayTool::SelectMove;
    lastCtrlTempSelect = false;
    lastShiftColorPick = false;
    pendingShiftColorPick = false;
    pendingCtrlWindowSelect = false;
    pendingCtrlWindowRect = { 0, 0, 0, 0 };
    loupeMagnification = 5;
    isRightClickSelectionDrag = false;
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
    ClearOverlayFrames();
    if (frozenDesktopBmp) {
        delete frozenDesktopBmp;
        frozenDesktopBmp = nullptr;
    }
    hasSelection = false;
    hasCtrlHoverWindow = false;
    desktopWindowRects.clear();
    dragMode = DragMode::None;
    hoveredBtnId = -1;
    pressedBtnId = -1;
}

static Bitmap* DecodePepperSnapLosslessWebP(const std::wstring& filePath) {
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return nullptr;
    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize < 25 || fileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return nullptr;
    }
    std::vector<BYTE> buf(fileSize);
    DWORD bytesRead = 0;
    bool ok = ReadFile(hFile, buf.data(), fileSize, &bytesRead, nullptr) && (bytesRead == fileSize);
    CloseHandle(hFile);
    if (!ok) return nullptr;

    if (memcmp(buf.data(), "RIFF", 4) != 0 || memcmp(buf.data() + 8, "WEBP", 4) != 0) return nullptr;
    size_t pos = 12;
    while (pos + 8 <= buf.size()) {
        uint32_t chunkSize = 0;
        memcpy(&chunkSize, buf.data() + pos + 4, 4);
        if (memcmp(buf.data() + pos, "VP8L", 4) == 0 && pos + 8 + chunkSize <= buf.size() && chunkSize >= 5) {
            const BYTE* vp8l = buf.data() + pos + 8;
            struct BitReader {
                const BYTE* data;
                size_t size;
                size_t bytePos = 0;
                uint64_t acc = 0;
                int bits = 0;
                uint32_t Read(int n) {
                    while (bits < n && bytePos < size) {
                        acc |= uint64_t(data[bytePos++]) << bits;
                        bits += 8;
                    }
                    if (bits < n) return 0;
                    uint32_t val = (uint32_t)(acc & ((1ULL << n) - 1ULL));
                    acc >>= n;
                    bits -= n;
                    return val;
                }
            } br{ vp8l, chunkSize };

            if (br.Read(8) != 0x2F) return nullptr;
            UINT w = br.Read(14) + 1;
            UINT h = br.Read(14) + 1;
            uint32_t hasAlpha = br.Read(1);
            uint32_t ver = br.Read(3);
            if (ver != 0 || w == 0 || h == 0 || w > 16383 || h > 16383) return nullptr;
            if (br.Read(1) != 0 || br.Read(1) != 0 || br.Read(1) != 0) return nullptr;

            auto skipIdentityTree = [&](int alphabetSize) -> bool {
                if (br.Read(1) != 0) return false;
                uint32_t numCl = br.Read(4) + 4;
                for (uint32_t i = 0; i < numCl; ++i) br.Read(3);
                if (br.Read(1) != 0) return false;
                for (int i = 0; i < alphabetSize; ++i) br.Read(1);
                return true;
            };
            if (!skipIdentityTree(280) || !skipIdentityTree(256) || !skipIdentityTree(256)) return nullptr;
            if (hasAlpha) {
                if (!skipIdentityTree(256)) return nullptr;
            } else {
                if (br.Read(1) != 1) return nullptr;
                br.Read(1); br.Read(1); br.Read(8);
            }
            if (br.Read(1) != 1) return nullptr;
            br.Read(3);

            BYTE rev8[256];
            for (int i = 0; i < 256; ++i) {
                BYTE r = 0;
                for (int b = 0; b < 8; ++b) r = (BYTE)((r << 1) | ((i >> b) & 1));
                rev8[i] = r;
            }

            Bitmap* bmp = new Bitmap((INT)w, (INT)h, PixelFormat32bppARGB);
            Rect lockRc(0, 0, (INT)w, (INT)h);
            BitmapData bd = {};
            if (bmp->LockBits(&lockRc, ImageLockModeWrite, PixelFormat32bppARGB, &bd) != Ok) {
                delete bmp;
                return nullptr;
            }
            for (UINT y = 0; y < h; ++y) {
                DWORD* row = (DWORD*)((BYTE*)bd.Scan0 + (size_t)y * bd.Stride);
                for (UINT x = 0; x < w; ++x) {
                    BYTE g = rev8[br.Read(8) & 0xFF];
                    BYTE r = rev8[br.Read(8) & 0xFF];
                    BYTE b = rev8[br.Read(8) & 0xFF];
                    BYTE a = hasAlpha ? rev8[br.Read(8) & 0xFF] : 255;
                    row[x] = (DWORD(a) << 24) | (DWORD(r) << 16) | (DWORD(g) << 8) | DWORD(b);
                }
            }
            bmp->UnlockBits(&bd);
            return bmp;
        }
        pos += 8 + chunkSize + (chunkSize & 1u);
    }
    return nullptr;
}

struct WebPAnimFrameInfo {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    int durationMs = 80;
    bool disposeBg = false;
    bool noBlend = false;
};

static bool ParseWebPAnimationHeader(const std::wstring& filePath, int& outCanvasW, int& outCanvasH, std::vector<WebPAnimFrameInfo>& outAnmfList) {
    outCanvasW = 0;
    outCanvasH = 0;
    outAnmfList.clear();

    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize < 30 || fileSize == INVALID_FILE_SIZE || fileSize > 256 * 1024 * 1024) {
        CloseHandle(hFile);
        return false;
    }
    std::vector<BYTE> buf(fileSize);
    DWORD bytesRead = 0;
    bool ok = ReadFile(hFile, buf.data(), fileSize, &bytesRead, nullptr) && (bytesRead == fileSize);
    CloseHandle(hFile);
    if (!ok) return false;

    if (memcmp(buf.data(), "RIFF", 4) != 0 || memcmp(buf.data() + 8, "WEBP", 4) != 0) return false;

    auto readU24LE = [](const BYTE* p) -> int {
        return (int)p[0] | ((int)p[1] << 8) | ((int)p[2] << 16);
    };

    size_t pos = 12;
    while (pos + 8 <= buf.size()) {
        uint32_t chunkSize = 0;
        memcpy(&chunkSize, buf.data() + pos + 4, 4);
        size_t payloadPos = pos + 8;
        if (payloadPos + chunkSize > buf.size()) break;

        if (memcmp(buf.data() + pos, "VP8X", 4) == 0 && chunkSize >= 10) {
            const BYTE* p = buf.data() + payloadPos;
            outCanvasW = readU24LE(p + 4) + 1;
            outCanvasH = readU24LE(p + 7) + 1;
        } else if (memcmp(buf.data() + pos, "ANMF", 4) == 0 && chunkSize >= 16) {
            const BYTE* p = buf.data() + payloadPos;
            WebPAnimFrameInfo info;
            info.x = readU24LE(p + 0) * 2;
            info.y = readU24LE(p + 3) * 2;
            info.w = readU24LE(p + 6) + 1;
            info.h = readU24LE(p + 9) + 1;
            info.durationMs = readU24LE(p + 12);
            if (info.durationMs <= 10) info.durationMs = 80;
            info.durationMs = std::max(20, std::min(60000, info.durationMs));
            BYTE flags = p[15];
            info.disposeBg = (flags & 0x01) != 0;
            info.noBlend   = (flags & 0x02) != 0;
            outAnmfList.push_back(info);
        }
        pos += 8 + (size_t)chunkSize + ((size_t)chunkSize & 1u);
    }
    return (outCanvasW > 0 && outCanvasH > 0);
}

static bool LoadImageFramesWithWIC(const std::wstring& filePath, std::vector<Bitmap*>& outFrames, std::vector<int>& outDelaysMs) {
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needCoUninit = SUCCEEDED(hrCo);

    int webpCanvasW = 0, webpCanvasH = 0;
    std::vector<WebPAnimFrameInfo> webpAnmf;
    ParseWebPAnimationHeader(filePath, webpCanvasW, webpCanvasH, webpAnmf);

    const CLSID clsidWicFactory2  = { 0x317d06e8, 0x5f24, 0x433d, { 0xbd, 0xf7, 0x79, 0xce, 0x68, 0xd8, 0xab, 0xc2 } };
    const CLSID clsidWicFactory1  = { 0xcacaf262, 0x9370, 0x4615, { 0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a } };
    const IID   iidWicFactory     = { 0xec5ec8a9, 0xc395, 0x4314, { 0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70 } };
    const CLSID clsidWebpDecoder  = { 0x7693e886, 0x51c9, 0x4070, { 0x84, 0x19, 0x9f, 0x70, 0x73, 0x8e, 0xc8, 0xfa } };
    const IID   iidWicDecoder     = { 0x9edde9e7, 0x8dee, 0x47ea, { 0x99, 0xdf, 0xe6, 0xfa, 0xf2, 0xed, 0x44, 0xbf } };
    const GUID  fmt32bppBGRA      = { 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0f } };

    IWICImagingFactory* pFactory = nullptr;
    if (FAILED(CoCreateInstance(clsidWicFactory2, nullptr, CLSCTX_INPROC_SERVER, iidWicFactory, (void**)&pFactory)) || !pFactory) {
        CoCreateInstance(clsidWicFactory1, nullptr, CLSCTX_INPROC_SERVER, iidWicFactory, (void**)&pFactory);
    }

    if (pFactory) {
        IWICBitmapDecoder* pDecoder = nullptr;
        IWICStream* pStream = nullptr;
        if (FAILED(pFactory->CreateDecoderFromFilename(filePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &pDecoder)) || !pDecoder) {
            if (SUCCEEDED(pFactory->CreateStream(&pStream)) && pStream) {
                if (SUCCEEDED(pStream->InitializeFromFilename(filePath.c_str(), GENERIC_READ))) {
                    if (FAILED(pFactory->CreateDecoderFromStream(pStream, nullptr, WICDecodeMetadataCacheOnDemand, &pDecoder)) || !pDecoder) {
                        if (SUCCEEDED(CoCreateInstance(clsidWebpDecoder, nullptr, CLSCTX_INPROC_SERVER, iidWicDecoder, (void**)&pDecoder)) && pDecoder) {
                            LARGE_INTEGER zero = {0};
                            pStream->Seek(zero, STREAM_SEEK_SET, nullptr);
                            if (FAILED(pDecoder->Initialize(pStream, WICDecodeMetadataCacheOnDemand))) {
                                pDecoder->Release();
                                pDecoder = nullptr;
                            }
                        }
                    }
                }
            }
        }

        if (pDecoder) {
            UINT frameCount = 0;
            if (FAILED(pDecoder->GetFrameCount(&frameCount)) || frameCount < 1) frameCount = 1;

            int canvasW = (webpCanvasW > 0) ? webpCanvasW : 0;
            int canvasH = (webpCanvasH > 0) ? webpCanvasH : 0;
            std::vector<DWORD> accumCanvas;
            if (frameCount > 1 && canvasW > 0 && canvasH > 0) {
                accumCanvas.assign((size_t)canvasW * (size_t)canvasH, 0u);
            }

            for (UINT i = 0; i < frameCount; ++i) {
                IWICBitmapFrameDecode* pFrame = nullptr;
                if (SUCCEEDED(pDecoder->GetFrame(i, &pFrame)) && pFrame) {
                    UINT w = 0, h = 0;
                    if (SUCCEEDED(pFrame->GetSize(&w, &h)) && w > 0 && h > 0) {
                        if (canvasW <= 0 || canvasH <= 0) {
                            canvasW = (int)w;
                            canvasH = (int)h;
                            if (frameCount > 1) {
                                accumCanvas.assign((size_t)canvasW * (size_t)canvasH, 0u);
                            }
                        }

                        int delayMs = (i < webpAnmf.size()) ? webpAnmf[i].durationMs : 80;
                        int frameX  = (i < webpAnmf.size()) ? webpAnmf[i].x : 0;
                        int frameY  = (i < webpAnmf.size()) ? webpAnmf[i].y : 0;
                        bool disposeBg = (i < webpAnmf.size()) ? webpAnmf[i].disposeBg : false;
                        bool noBlend   = (i < webpAnmf.size()) ? webpAnmf[i].noBlend   : false;

                        if (frameCount > 1) {
                            IWICMetadataQueryReader* pMeta = nullptr;
                            if (SUCCEEDED(pFrame->GetMetadataQueryReader(&pMeta)) && pMeta) {
                                PROPVARIANT pv;
                                PropVariantInit(&pv);
                                if (SUCCEEDED(pMeta->GetMetadataByName(L"/ANMF/FrameDuration", &pv))) {
                                    if (pv.vt == VT_UI4 || pv.vt == VT_I4) delayMs = (int)pv.ulVal;
                                    else if (pv.vt == VT_UI2 || pv.vt == VT_I2) delayMs = (int)pv.uiVal;
                                } else if (SUCCEEDED(pMeta->GetMetadataByName(L"/grctlext/Delay", &pv))) {
                                    if (pv.vt == VT_UI2 || pv.vt == VT_I2) delayMs = (int)pv.uiVal * 10;
                                }
                                PropVariantClear(&pv);
                                pMeta->Release();
                            }
                            if (delayMs <= 10) delayMs = 80;
                            delayMs = std::max(20, std::min(60000, delayMs));
                        }

                        IWICFormatConverter* pConv = nullptr;
                        if (SUCCEEDED(pFactory->CreateFormatConverter(&pConv)) && pConv) {
                            if (SUCCEEDED(pConv->Initialize(pFrame, fmt32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
                                bool useCanvasComp = (frameCount > 1 && !accumCanvas.empty() && canvasW > 0 && canvasH > 0 &&
                                                      (!webpAnmf.empty() || (int)w != canvasW || (int)h != canvasH));
                                if (useCanvasComp) {
                                    std::vector<DWORD> rawPx((size_t)w * (size_t)h, 0u);
                                    HRESULT hrCopy = pConv->CopyPixels(nullptr, w * 4u, (UINT)(rawPx.size() * 4u), (BYTE*)rawPx.data());
                                    if (SUCCEEDED(hrCopy)) {
                                        // If WIC already returned a full-canvas frame while ANMF had an offset, place at (0,0)
                                        int dstOffX = ((int)w == canvasW && (int)h == canvasH) ? 0 : frameX;
                                        int dstOffY = ((int)w == canvasW && (int)h == canvasH) ? 0 : frameY;
                                        for (UINT sy = 0; sy < h; ++sy) {
                                            int dy = dstOffY + (int)sy;
                                            if (dy < 0 || dy >= canvasH) continue;
                                            const DWORD* srcRow = rawPx.data() + (size_t)sy * w;
                                            DWORD* dstRow = accumCanvas.data() + (size_t)dy * canvasW;
                                            for (UINT sx = 0; sx < w; ++sx) {
                                                int dx = dstOffX + (int)sx;
                                                if (dx < 0 || dx >= canvasW) continue;
                                                DWORD sp = srcRow[sx];
                                                if (noBlend || i == 0) {
                                                    dstRow[dx] = sp;
                                                } else {
                                                    DWORD sa = (sp >> 24) & 0xFFu;
                                                    if (sa == 255u) {
                                                        dstRow[dx] = sp;
                                                    } else if (sa > 0u) {
                                                        DWORD dp = dstRow[dx];
                                                        DWORD da = (dp >> 24) & 0xFFu;
                                                        if (da == 0u) {
                                                            dstRow[dx] = sp;
                                                        } else {
                                                            DWORD invSa = 255u - sa;
                                                            DWORD outA = sa + (da * invSa + 127u) / 255u;
                                                            if (outA > 0u) {
                                                                DWORD sr = (sp >> 16) & 0xFFu, sg = (sp >> 8) & 0xFFu, sb = sp & 0xFFu;
                                                                DWORD dr = (dp >> 16) & 0xFFu, dg = (dp >> 8) & 0xFFu, db = dp & 0xFFu;
                                                                DWORD outR = (sr * sa * 255u + dr * da * invSa + (outA * 255u) / 2u) / (outA * 255u);
                                                                DWORD outG = (sg * sa * 255u + dg * da * invSa + (outA * 255u) / 2u) / (outA * 255u);
                                                                DWORD outB = (sb * sa * 255u + db * da * invSa + (outA * 255u) / 2u) / (outA * 255u);
                                                                dstRow[dx] = (std::min((DWORD)255, outA) << 24) |
                                                                             (std::min((DWORD)255, outR) << 16) |
                                                                             (std::min((DWORD)255, outG) << 8)  |
                                                                             std::min((DWORD)255, outB);
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        Bitmap* frameBmp = new Bitmap(canvasW, canvasH, PixelFormat32bppARGB);
                                        Rect lockRc(0, 0, canvasW, canvasH);
                                        BitmapData bd = {};
                                        if (frameBmp->LockBits(&lockRc, ImageLockModeWrite, PixelFormat32bppARGB, &bd) == Ok) {
                                            for (int y = 0; y < canvasH; ++y) {
                                                memcpy((BYTE*)bd.Scan0 + (size_t)y * bd.Stride,
                                                       accumCanvas.data() + (size_t)y * canvasW,
                                                       (size_t)canvasW * 4u);
                                            }
                                            frameBmp->UnlockBits(&bd);
                                            outFrames.push_back(frameBmp);
                                            outDelaysMs.push_back(delayMs);
                                        } else {
                                            delete frameBmp;
                                        }

                                        if (disposeBg) {
                                            for (UINT sy = 0; sy < h; ++sy) {
                                                int dy = dstOffY + (int)sy;
                                                if (dy < 0 || dy >= canvasH) continue;
                                                DWORD* dstRow = accumCanvas.data() + (size_t)dy * canvasW;
                                                for (UINT sx = 0; sx < w; ++sx) {
                                                    int dx = dstOffX + (int)sx;
                                                    if (dx >= 0 && dx < canvasW) dstRow[dx] = 0u;
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    Bitmap* frameBmp = new Bitmap((INT)w, (INT)h, PixelFormat32bppARGB);
                                    Rect lockRc(0, 0, (INT)w, (INT)h);
                                    BitmapData bd = {};
                                    if (frameBmp->LockBits(&lockRc, ImageLockModeWrite, PixelFormat32bppARGB, &bd) == Ok) {
                                        HRESULT hrCopy = pConv->CopyPixels(nullptr, (UINT)bd.Stride, (UINT)((size_t)bd.Stride * h), (BYTE*)bd.Scan0);
                                        frameBmp->UnlockBits(&bd);
                                        if (SUCCEEDED(hrCopy)) {
                                            outFrames.push_back(frameBmp);
                                            outDelaysMs.push_back(delayMs);
                                        } else {
                                            delete frameBmp;
                                        }
                                    } else {
                                        delete frameBmp;
                                    }
                                }
                            }
                            pConv->Release();
                        }
                    }
                    pFrame->Release();
                }
            }
            pDecoder->Release();
        }
        if (pStream) pStream->Release();
        pFactory->Release();
    }

    if (needCoUninit) CoUninitialize();
    return !outFrames.empty();
}

static bool LoadImageFramesFromFile(const std::wstring& filePath, std::vector<Bitmap*>& outFrames, std::vector<int>& outDelaysMs) {
    outFrames.clear();
    outDelaysMs.clear();
    if (filePath.empty()) return false;

    std::wstring ext = PathFindExtensionW(filePath.c_str());
    for (wchar_t& c : ext) c = (wchar_t)towlower(c);

    // 1. For non-WebP formats (including animated .gif, .png, .jpg, .bmp, .tiff, .ico), use GDI+ first
    //    so multi-frame GIF disposal methods and frame sub-rects composite accurately with full alpha
    if (ext != L".webp") {
        Bitmap loaded(filePath.c_str());
        if (loaded.GetLastStatus() == Ok && loaded.GetWidth() > 0 && loaded.GetHeight() > 0) {
            int imgW = (int)loaded.GetWidth();
            int imgH = (int)loaded.GetHeight();
            GUID timeDim = FrameDimensionTime;
            UINT frameCount = loaded.GetFrameCount(&timeDim);
            if (frameCount < 1) frameCount = 1;

            std::vector<int> delays(frameCount, 100);
            if (frameCount > 1) {
                UINT propSize = loaded.GetPropertyItemSize(PropertyTagFrameDelay);
                if (propSize > sizeof(PropertyItem)) {
                    std::vector<BYTE> propBuf(propSize);
                    PropertyItem* pItem = (PropertyItem*)propBuf.data();
                    if (loaded.GetPropertyItem(PropertyTagFrameDelay, propSize, pItem) == Ok && pItem->value) {
                        const UINT* rawDelays = (const UINT*)pItem->value;
                        size_t count = pItem->length / sizeof(UINT);
                        for (UINT i = 0; i < frameCount; ++i) {
                            UINT cs = (i < count) ? rawDelays[i] : 10;
                            int ms = (int)cs * 10;
                            if (ms <= 10) ms = 100;
                            delays[i] = std::max(20, std::min(60000, ms));
                        }
                    }
                }
            }

            for (UINT i = 0; i < frameCount; ++i) {
                if (frameCount > 1) {
                    loaded.SelectActiveFrame(&timeDim, i);
                }
                Bitmap* frameBmp = new Bitmap(imgW, imgH, PixelFormat32bppARGB);
                Rect lockRc(0, 0, imgW, imgH);
                BitmapData srcBd = {}, dstBd = {};
                if (loaded.LockBits(&lockRc, ImageLockModeRead, PixelFormat32bppARGB, &srcBd) == Ok) {
                    if (frameBmp->LockBits(&lockRc, ImageLockModeWrite, PixelFormat32bppARGB, &dstBd) == Ok) {
                        for (int y = 0; y < imgH; ++y) {
                            memcpy((BYTE*)dstBd.Scan0 + (size_t)y * dstBd.Stride,
                                   (const BYTE*)srcBd.Scan0 + (size_t)y * srcBd.Stride,
                                   (size_t)imgW * 4);
                        }
                        frameBmp->UnlockBits(&dstBd);
                    }
                    loaded.UnlockBits(&srcBd);
                } else {
                    Graphics g(frameBmp);
                    g.Clear(Color(0, 0, 0, 0));
                    g.SetCompositingMode(CompositingModeSourceOver);
                    g.SetInterpolationMode(InterpolationModeNearestNeighbor);
                    g.SetPixelOffsetMode(PixelOffsetModeHalf);
                    g.DrawImage(&loaded, Rect(0, 0, imgW, imgH), 0, 0, imgW, imgH, UnitPixel);
                }
                outFrames.push_back(frameBmp);
                outDelaysMs.push_back(delays[i]);
            }
            return !outFrames.empty();
        }
    }

    // 2. Decode via Windows Imaging Component (WIC — supports .webp lossy/lossless/alpha/animated + all Windows codecs)
    if (LoadImageFramesWithWIC(filePath, outFrames, outDelaysMs)) {
        return true;
    }

    // 3. Fallback: Native PepperSnap lossless VP8L WebP decoder
    if (Bitmap* vp8lBmp = DecodePepperSnapLosslessWebP(filePath)) {
        outFrames.push_back(vp8lBmp);
        outDelaysMs.push_back(100);
        return true;
    }

    return false;
}

void PepperSnapDaemon::ClearOverlayFrames() {
    if (overlaySingleImageBmp) {
        delete overlaySingleImageBmp;
        overlaySingleImageBmp = nullptr;
    }
    for (Bitmap* f : overlayFrames) {
        delete f;
    }
    overlayFrames.clear();
    overlayFrameDelaysMs.clear();
    overlayFrameCardRects.clear();
    overlayActiveFrameIdx = 0;
    overlayImgDrawRect = { 0, 0, 0, 0 };
    overlayFrameStripCollapsed = false;
    overlayFrameScrollX = 0.0f;
    overlayFrameMaxScrollX = 0.0f;
    overlayHoveredFrameIdx = -1;
    overlayToggleBtnHovered = false;
    overlayPlayBtnHovered = false;
    if (overlayFramesPlaying && hOverlayWnd && IsWindow(hOverlayWnd)) {
        KillTimer(hOverlayWnd, 3);
    }
    overlayFramesPlaying = false;
    overlayTopResizeHovered = false;
    overlayScrollbarHovered = false;
    overlayScrollLeftBtnHovered = false;
    overlayScrollRightBtnHovered = false;
    overlayFrameStripPanelRect = { 0, 0, 0, 0 };
    overlayFrameStripToggleBtnRect = { 0, 0, 0, 0 };
    overlayFrameStripPlayBtnRect = { 0, 0, 0, 0 };
    overlayFrameStripResizeRect = { 0, 0, 0, 0 };
    overlayFrameStripScrollTrackRect = { 0, 0, 0, 0 };
    overlayFrameStripScrollThumbRect = { 0, 0, 0, 0 };
    overlayFrameStripScrollLeftRect = { 0, 0, 0, 0 };
    overlayFrameStripScrollRightRect = { 0, 0, 0, 0 };
}

void PepperSnapDaemon::EnsureActiveFrameVisibleInStrip() {
    if (!HasOverlayMultiFrames() || overlayFrames.empty() || !overlayFrames[0]) return;
    int W = vScreenW > 0 ? vScreenW : GetSystemMetrics(SM_CXSCREEN);
    int H = vScreenH > 0 ? vScreenH : GetSystemMetrics(SM_CYSCREEN);
    int maxStripH = std::max(160, (int)(H * 0.48));
    int stripH = std::max(92, std::min(maxStripH, overlayFrameStripHeight));
    int cardH = std::max(54, stripH - 30);
    int thumbH = std::max(32, cardH - 22);
    int origW = (int)overlayFrames[0]->GetWidth();
    int origH = (int)overlayFrames[0]->GetHeight();
    double aspect = (origH > 0) ? ((double)origW / (double)origH) : 1.0;
    int thumbW = std::max(44, std::min(320, (int)std::round(thumbH * aspect)));
    int cardW = thumbW + 12;
    int cardGap = 8;
    int sidePad = 14;
    int cardLeft = sidePad + (int)overlayActiveFrameIdx * (cardW + cardGap);
    int cardRight = cardLeft + cardW;
    if ((float)cardLeft < overlayFrameScrollX + sidePad) {
        overlayFrameScrollX = (float)std::max(0, cardLeft - sidePad);
    } else if ((float)cardRight > overlayFrameScrollX + W - sidePad) {
        overlayFrameScrollX = (float)std::max(0, cardRight - W + sidePad);
    }
}

void PepperSnapDaemon::LayoutOverlayFrameStrip(int screenW, int screenH) {
    overlayFrameCardRects.clear();
    if (!HasOverlayMultiFrames() || screenW <= 0 || screenH <= 0) {
        overlayFrameStripPanelRect = { 0, 0, 0, 0 };
        overlayFrameStripToggleBtnRect = { 0, 0, 0, 0 };
        overlayFrameStripPlayBtnRect = { 0, 0, 0, 0 };
        overlayFrameStripResizeRect = { 0, 0, 0, 0 };
        overlayFrameStripScrollTrackRect = { 0, 0, 0, 0 };
        overlayFrameStripScrollThumbRect = { 0, 0, 0, 0 };
        overlayFrameStripScrollLeftRect = { 0, 0, 0, 0 };
        overlayFrameStripScrollRightRect = { 0, 0, 0, 0 };
        return;
    }

    int maxStripH = std::max(160, (int)(screenH * 0.48));
    overlayFrameStripHeight = std::max(92, std::min(maxStripH, overlayFrameStripHeight));
    int toggleW = 248;
    int toggleH = 26;
    int playW = 30;
    int toggleLeft = (screenW - toggleW) / 2;

    if (overlayFrameStripCollapsed) {
        overlayFrameStripPanelRect = { 0, 0, 0, 0 };
        overlayFrameStripResizeRect = { 0, 0, 0, 0 };
        overlayFrameStripScrollTrackRect = { 0, 0, 0, 0 };
        overlayFrameStripScrollThumbRect = { 0, 0, 0, 0 };
        overlayFrameStripScrollLeftRect = { 0, 0, 0, 0 };
        overlayFrameStripScrollRightRect = { 0, 0, 0, 0 };
        overlayFrameStripToggleBtnRect = { toggleLeft, screenH - toggleH, toggleLeft + toggleW, screenH };
        overlayFrameStripPlayBtnRect = { toggleLeft + toggleW - playW, screenH - toggleH, toggleLeft + toggleW, screenH };
        return;
    }

    int panelTop = screenH - overlayFrameStripHeight;
    overlayFrameStripPanelRect = { 0, panelTop, screenW, screenH };
    overlayFrameStripToggleBtnRect = { toggleLeft, panelTop - toggleH, toggleLeft + toggleW, panelTop };
    overlayFrameStripPlayBtnRect = { toggleLeft + toggleW - playW, panelTop - toggleH, toggleLeft + toggleW, panelTop };
    overlayFrameStripResizeRect = { 0, panelTop - 5, screenW, panelTop + 6 };

    int cardsTop = panelTop + 8;
    int cardsBottom = screenH - 22;
    int cardH = std::max(54, cardsBottom - cardsTop);
    int thumbH = std::max(32, cardH - 22);
    int origW = overlayFrames[0] ? (int)overlayFrames[0]->GetWidth() : 64;
    int origH = overlayFrames[0] ? (int)overlayFrames[0]->GetHeight() : 64;
    double aspect = (origH > 0) ? ((double)origW / (double)origH) : 1.0;
    int thumbW = std::max(44, std::min(320, (int)std::round(thumbH * aspect)));
    int cardW = thumbW + 12;
    int cardGap = 8;
    int sidePad = 14;

    int n = (int)overlayFrames.size();
    int totalFramesW = sidePad * 2 + n * cardW + std::max(0, n - 1) * cardGap;
    overlayFrameMaxScrollX = (float)std::max(0, totalFramesW - screenW);
    overlayFrameScrollX = std::max(0.0f, std::min(overlayFrameMaxScrollX, overlayFrameScrollX));

    overlayFrameCardRects.resize(overlayFrames.size());
    int scrollInt = (int)std::round(overlayFrameScrollX);
    for (int i = 0; i < n; ++i) {
        int cx = sidePad + i * (cardW + cardGap) - scrollInt;
        overlayFrameCardRects[i] = { cx, cardsTop, cx + cardW, cardsTop + cardH };
    }

    int sbTop = screenH - 18;
    int sbBot = screenH - 3;
    overlayFrameStripScrollLeftRect  = { 10, sbTop, 30, sbBot };
    overlayFrameStripScrollRightRect = { screenW - 30, sbTop, screenW - 10, sbBot };
    int trackLeft = 34;
    int trackRight = std::max(trackLeft + 40, screenW - 34);
    overlayFrameStripScrollTrackRect = { trackLeft, sbTop, trackRight, sbBot };

    int trackW = trackRight - trackLeft;
    if (overlayFrameMaxScrollX > 0.5f && totalFramesW > 0) {
        int thumbSpan = std::max(36, std::min(trackW, (int)std::round((double)trackW * (double)screenW / (double)totalFramesW)));
        int thumbX = trackLeft + (int)std::round(((double)overlayFrameScrollX / (double)overlayFrameMaxScrollX) * (trackW - thumbSpan));
        overlayFrameStripScrollThumbRect = { thumbX, sbTop + 1, thumbX + thumbSpan, sbBot - 1 };
    } else {
        overlayFrameStripScrollThumbRect = { trackLeft, sbTop + 1, trackRight, sbBot - 1 };
    }
}

void PepperSnapDaemon::SelectOverlayFrame(size_t frameIdx) {
    if (frameIdx >= overlayFrames.size() || !overlayFrames[frameIdx] || !frozenDesktopBmp) return;
    overlayActiveFrameIdx = frameIdx;
    Bitmap* frameBmp = overlayFrames[frameIdx];
    int imgW = (int)frameBmp->GetWidth();
    int imgH = (int)frameBmp->GetHeight();
    int drawX = overlayImgDrawRect.left;
    int drawY = overlayImgDrawRect.top;
    int drawW = overlayImgDrawRect.right - overlayImgDrawRect.left;
    int drawH = overlayImgDrawRect.bottom - overlayImgDrawRect.top;
    if (imgW > 0 && imgH > 0 && drawW > 0 && drawH > 0) {
        Graphics g(frozenDesktopBmp);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(PixelOffsetModeHalf);
        SolidBrush bg(Color(255, 15, 23, 42));
        g.FillRectangle(&bg, drawX, drawY, drawW, drawH);
        ImageAttributes ia;
        ia.SetWrapMode(WrapModeTileFlipXY);
        double fitScale = std::min((double)drawW / (double)imgW, (double)drawH / (double)imgH);
        int fitW = std::max(1, std::min(drawW, (int)std::round(imgW * fitScale)));
        int fitH = std::max(1, std::min(drawH, (int)std::round(imgH * fitScale)));
        int fitX = drawX + (drawW - fitW) / 2;
        int fitY = drawY + (drawH - fitH) / 2;
        g.DrawImage(frameBmp, Rect(fitX, fitY, fitW, fitH), 0, 0, imgW, imgH, UnitPixel, &ia);
        BuildOverlaySurfaceCache();
    }
    if (hOverlayWnd && IsWindow(hOverlayWnd)) {
        InvalidateRect(hOverlayWnd, nullptr, FALSE);
    }
}

void PepperSnapDaemon::StopOverlayFramePlayback() {
    if (!overlayFramesPlaying) return;
    overlayFramesPlaying = false;
    if (hOverlayWnd && IsWindow(hOverlayWnd)) {
        KillTimer(hOverlayWnd, 3);
        InvalidateRect(hOverlayWnd, nullptr, FALSE);
    }
}

void PepperSnapDaemon::ToggleOverlayFramePlayback() {
    if (!HasOverlayMultiFrames() || !hOverlayWnd || !IsWindow(hOverlayWnd)) return;
    overlayFramesPlaying = !overlayFramesPlaying;
    if (overlayFramesPlaying) {
        int delayMs = (overlayActiveFrameIdx < overlayFrameDelaysMs.size()) ? overlayFrameDelaysMs[overlayActiveFrameIdx] : 80;
        delayMs = std::max(20, std::min(60000, delayMs));
        SetTimer(hOverlayWnd, 3, (UINT)delayMs, nullptr);
    } else {
        KillTimer(hOverlayWnd, 3);
    }
    InvalidateRect(hOverlayWnd, nullptr, FALSE);
}

void PepperSnapDaemon::RenderOverlayFrameStrip(Graphics& g, int screenW, int screenH) {
    if (!HasOverlayMultiFrames()) return;
    LayoutOverlayFrameStrip(screenW, screenH);

    FontFamily ff(L"Segoe UI");
    Font btnFont(&ff, 11.5f, FontStyleBold, UnitPixel);
    Font cardFont(&ff, 10.5f, FontStyleBold, UnitPixel);
    StringFormat centerSf;
    centerSf.SetAlignment(StringAlignmentCenter);
    centerSf.SetLineAlignment(StringAlignmentCenter);

    // 1. Render Dropdown Toggle Button + Play/Pause Icon inside the same square (Always visible)
    {
        RECT tr = overlayFrameStripToggleBtnRect;
        RECT playRc = overlayFrameStripPlayBtnRect;
        float playW = (float)(playRc.right - playRc.left);
        RectF trf((float)tr.left, (float)tr.top, (float)(tr.right - tr.left), (float)(tr.bottom - tr.top));
        bool anySqHover = overlayToggleBtnHovered || overlayPlayBtnHovered;
        SolidBrush btnBg(Color(245, 15, 23, 42));
        g.FillRectangle(&btnBg, trf);

        if (overlayToggleBtnHovered) {
            SolidBrush leftHovBg(Color(252, 30, 41, 59));
            g.FillRectangle(&leftHovBg, trf.X, trf.Y, trf.Width - playW, trf.Height);
        }
        if (overlayPlayBtnHovered) {
            SolidBrush rightHovBg(Color(252, 30, 41, 59));
            g.FillRectangle(&rightHovBg, (float)playRc.left, (float)playRc.top, playW, trf.Height);
        } else if (overlayFramesPlaying) {
            SolidBrush activePlayBg(Color(220, 45, 20, 32));
            g.FillRectangle(&activePlayBg, (float)playRc.left, (float)playRc.top, playW, trf.Height);
        }

        // Subtle inner divider separating Hide/Show frames text from the Play/Pause button on the right
        Pen divPen(Color(210, 71, 85, 105), 1.0f);
        g.DrawLine(&divPen, (float)playRc.left, trf.Y + 4.0f, (float)playRc.left, trf.Y + trf.Height - 4.0f);

        // Draw Up/Down Chevron Icon on the left side
        float chevCx = trf.X + 16.0f;
        float chevCy = trf.Y + trf.Height * 0.5f;
        Pen chevPen(Color(255, 239, 68, 68), 2.0f);
        chevPen.SetStartCap(LineCapRound);
        chevPen.SetEndCap(LineCapRound);
        if (overlayFrameStripCollapsed) {
            // Up chevron (click to show frame list)
            g.DrawLine(&chevPen, chevCx - 4.5f, chevCy + 2.0f, chevCx, chevCy - 2.5f);
            g.DrawLine(&chevPen, chevCx, chevCy - 2.5f, chevCx + 4.5f, chevCy + 2.0f);
        } else {
            // Down chevron (click to hide frame list)
            g.DrawLine(&chevPen, chevCx - 4.5f, chevCy - 2.0f, chevCx, chevCy + 2.5f);
            g.DrawLine(&chevPen, chevCx, chevCy + 2.5f, chevCx + 4.5f, chevCy - 2.0f);
        }

        // Draw Label in the center between dropdown chevron and play/pause icon
        std::wstring toggleLbl = (overlayFrameStripCollapsed ? L"Show frames (" : L"Hide frames (") +
                                 std::to_wstring(overlayActiveFrameIdx + 1) + L" / " +
                                 std::to_wstring(overlayFrames.size()) + L")";
        SolidBrush txtBr(Color(255, 248, 250, 252));
        RectF lblRc(trf.X + 26.0f, trf.Y, (trf.Width - playW) - 28.0f, trf.Height);
        g.DrawString(toggleLbl.c_str(), -1, &btnFont, lblRc, &centerSf, &txtBr);

        // Draw Play / Pause Icon on the right side literally opposite the dropdown chevron inside the square
        float pcx = (playRc.left + playRc.right) * 0.5f;
        float pcy = (playRc.top + playRc.bottom) * 0.5f;
        Color iconCol = (overlayPlayBtnHovered || overlayFramesPlaying)
            ? Color(255, 239, 68, 68)
            : Color(255, 248, 250, 252);
        SolidBrush iconBr(iconCol);
        if (overlayFramesPlaying) {
            // Pause icon (two vertical bars)
            g.FillRectangle(&iconBr, pcx - 4.5f, pcy - 5.0f, 3.0f, 10.0f);
            g.FillRectangle(&iconBr, pcx + 1.5f, pcy - 5.0f, 3.0f, 10.0f);
        } else {
            // Play icon (right-pointing triangle)
            PointF tri[3] = {
                PointF(pcx - 3.5f, pcy - 5.5f),
                PointF(pcx - 3.5f, pcy + 5.5f),
                PointF(pcx + 5.5f, pcy)
            };
            g.FillPolygon(&iconBr, tri, 3);
        }

        Pen btnBorder(anySqHover ? Color(255, 239, 68, 68) : Color(230, 71, 85, 105), 1.5f);
        g.DrawRectangle(&btnBorder, trf.X, trf.Y, trf.Width, trf.Height);

        if (anySqHover && dragMode == DragMode::None) {
            DockButton tipBtn;
            if (overlayPlayBtnHovered) {
                tipBtn.rect = playRc;
                tipBtn.tooltip = FormatLabelWithShortcut(overlayFramesPlaying ? L"Pause animation" : L"Play animation", hkFramePlayPause);
            } else {
                tipBtn.rect = { tr.left, tr.top, playRc.left, tr.bottom };
                tipBtn.tooltip = FormatLabelWithShortcut(overlayFrameStripCollapsed ? L"Show frames" : L"Hide frames", hkFrameToggleStrip);
            }
            DrawHoverBubbleTooltip(g, tipBtn, screenW, screenH);
        }
    }

    if (overlayFrameStripCollapsed) return;

    // 2. Render Expanded Bottom Frame Strip Panel
    RECT pr = overlayFrameStripPanelRect;
    SolidBrush panelBg(Color(246, 15, 23, 42));
    g.FillRectangle(&panelBg, (INT)pr.left, (INT)pr.top, (INT)(pr.right - pr.left), (INT)(pr.bottom - pr.top));

    bool topResizeActive = (dragMode == DragMode::ResizingFrameStrip) || overlayTopResizeHovered;
    Pen topBorderPen(topResizeActive ? Color(255, 239, 68, 68) : Color(220, 71, 85, 105), topResizeActive ? 2.2f : 1.5f);
    g.DrawLine(&topBorderPen, (INT)0, (INT)pr.top, (INT)screenW, (INT)pr.top);

    // Subtle vertical-resize grip indicators on both sides of the dropdown tab
    Pen gripPen(topResizeActive ? Color(255, 248, 250, 252) : Color(160, 148, 163, 184), 1.2f);
    int leftGripCx = overlayFrameStripToggleBtnRect.left - 36;
    int rightGripCx = overlayFrameStripToggleBtnRect.right + 36;
    if (leftGripCx > 24) {
        g.DrawLine(&gripPen, (INT)(leftGripCx - 14), (INT)(pr.top + 3), (INT)(leftGripCx + 14), (INT)(pr.top + 3));
        g.DrawLine(&gripPen, (INT)(leftGripCx - 14), (INT)(pr.top + 5), (INT)(leftGripCx + 14), (INT)(pr.top + 5));
    }
    if (rightGripCx < screenW - 24) {
        g.DrawLine(&gripPen, (INT)(rightGripCx - 14), (INT)(pr.top + 3), (INT)(rightGripCx + 14), (INT)(pr.top + 3));
        g.DrawLine(&gripPen, (INT)(rightGripCx - 14), (INT)(pr.top + 5), (INT)(rightGripCx + 14), (INT)(pr.top + 5));
    }

    // 3. Render Frame Cards (Clipped horizontally to screen bounds)
    GraphicsState clipSt = g.Save();
    g.SetClip(Rect(0, (INT)(pr.top + 6), screenW, (INT)((pr.bottom - pr.top) - 25)), CombineModeReplace);

    SolidBrush chkDark(Color(255, 30, 41, 59));
    SolidBrush chkLight(Color(255, 51, 65, 85));

    for (size_t i = 0; i < overlayFrameCardRects.size() && i < overlayFrames.size(); ++i) {
        const RECT& cr = overlayFrameCardRects[i];
        if (cr.right < 0 || cr.left > screenW) continue;

        bool isSel = (i == overlayActiveFrameIdx);
        bool isHov = ((int)i == overlayHoveredFrameIdx);
        RectF crf((float)cr.left, (float)cr.top, (float)(cr.right - cr.left), (float)(cr.bottom - cr.top));

        Color cardBgCol = isSel
            ? Color(255, 45, 26, 38)
            : (isHov ? Color(250, 51, 65, 85) : Color(242, 30, 41, 59));
        SolidBrush cardBg(cardBgCol);
        g.FillRectangle(&cardBg, crf);

        int tx = cr.left + 6;
        int ty = cr.top + 5;
        int tw = std::max(8, (int)(cr.right - cr.left) - 12);
        int th = std::max(8, (int)(cr.bottom - cr.top) - 23);

        // Checkerboard backdrop inside thumbnail for transparent GIF/WebP frames
        g.FillRectangle(&chkDark, tx, ty, tw, th);
        const int cell = 8;
        for (int cy = 0; cy < th; cy += cell) {
            int ch = std::min(cell, th - cy);
            for (int cx = 0; cx < tw; cx += cell) {
                if (((cx / cell) + (cy / cell)) & 1) {
                    int cw = std::min(cell, tw - cx);
                    g.FillRectangle(&chkLight, tx + cx, ty + cy, cw, ch);
                }
            }
        }

        if (Bitmap* fb = overlayFrames[i]) {
            int fw = (int)fb->GetWidth();
            int fh = (int)fb->GetHeight();
            if (fw > 0 && fh > 0) {
                double sc = std::min((double)tw / (double)fw, (double)th / (double)fh);
                int dw = std::max(1, (int)std::round(fw * sc));
                int dh = std::max(1, (int)std::round(fh * sc));
                int dx = tx + (tw - dw) / 2;
                int dy = ty + (th - dh) / 2;
                g.DrawImage(fb, Rect(dx, dy, dw, dh), 0, 0, fw, fh, UnitPixel);
            }
        }

        int delayMs = (i < overlayFrameDelaysMs.size()) ? overlayFrameDelaysMs[i] : 100;
        std::wstring cardLbl = (cr.right - cr.left >= 82)
            ? (L"#" + std::to_wstring(i + 1) + L" \x00B7 " + std::to_wstring(delayMs) + L"ms")
            : (L"#" + std::to_wstring(i + 1));
        SolidBrush lblBr(isSel ? Color(255, 254, 202, 202) : Color(255, 226, 232, 240));
        RectF lblRc((float)cr.left + 2.0f, (float)(cr.bottom - 18), (float)(cr.right - cr.left - 4), 17.0f);
        g.DrawString(cardLbl.c_str(), -1, &cardFont, lblRc, &centerSf, &lblBr);

        Color borderCol = isSel
            ? Color(255, 239, 68, 68)
            : (isHov ? Color(255, 56, 189, 248) : Color(210, 71, 85, 105));
        Pen borderPen(borderCol, isSel ? 2.2f : (isHov ? 1.5f : 1.0f));
        g.DrawRectangle(&borderPen, crf.X, crf.Y, crf.Width, crf.Height);
    }

    g.Restore(clipSt);

    // 4. Render Horizontal Scrollbar & Left/Right Step Buttons at Bottom of Strip
    auto drawStepBtn = [&](const RECT& r, bool isLeft, bool isHov) {
        SolidBrush bBg(isHov ? Color(255, 51, 65, 85) : Color(235, 30, 41, 59));
        Pen bPen(Color(200, 71, 85, 105), 1.0f);
        g.FillRectangle(&bBg, (INT)r.left, (INT)r.top, (INT)(r.right - r.left), (INT)(r.bottom - r.top));
        g.DrawRectangle(&bPen, (INT)r.left, (INT)r.top, (INT)(r.right - r.left), (INT)(r.bottom - r.top));
        float cx = (r.left + r.right) * 0.5f;
        float cy = (r.top + r.bottom) * 0.5f;
        Pen arrPen(Color(255, 226, 232, 240), 1.6f);
        if (isLeft) {
            g.DrawLine(&arrPen, cx + 2.0f, cy - 3.5f, cx - 2.0f, cy);
            g.DrawLine(&arrPen, cx - 2.0f, cy, cx + 2.0f, cy + 3.5f);
        } else {
            g.DrawLine(&arrPen, cx - 2.0f, cy - 3.5f, cx + 2.0f, cy);
            g.DrawLine(&arrPen, cx + 2.0f, cy, cx - 2.0f, cy + 3.5f);
        }
    };
    drawStepBtn(overlayFrameStripScrollLeftRect, true, overlayScrollLeftBtnHovered);
    drawStepBtn(overlayFrameStripScrollRightRect, false, overlayScrollRightBtnHovered);

    RECT trk = overlayFrameStripScrollTrackRect;
    SolidBrush trkBg(Color(220, 9, 13, 22));
    Pen trkBorder(Color(180, 51, 65, 85), 1.0f);
    g.FillRectangle(&trkBg, (INT)trk.left, (INT)trk.top, (INT)(trk.right - trk.left), (INT)(trk.bottom - trk.top));
    g.DrawRectangle(&trkBorder, (INT)trk.left, (INT)trk.top, (INT)(trk.right - trk.left), (INT)(trk.bottom - trk.top));

    RECT thm = overlayFrameStripScrollThumbRect;
    bool thumbActive = (dragMode == DragMode::ScrollingFrameStrip) || overlayScrollbarHovered;
    SolidBrush thmBg(thumbActive ? Color(255, 239, 68, 68) : Color(240, 100, 116, 139));
    g.FillRectangle(&thmBg, (INT)thm.left, (INT)thm.top, (INT)(thm.right - thm.left), (INT)(thm.bottom - thm.top));
}

void PepperSnapDaemon::OpenImageFileIntoOverlay(const std::wstring& filePath) {
    if (filePath.empty()) return;
    std::vector<Bitmap*> frames;
    std::vector<int> delays;
    if (!LoadImageFramesFromFile(filePath, frames, delays) || frames.empty()) return;

    if (hOverlayWnd) {
        CloseRegionSnipOverlay();
    }
    ClearOverlayFrames();

    Bitmap* firstFrame = frames[0];
    int sw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int sh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (sw <= 0 || sh <= 0) {
        sw = GetSystemMetrics(SM_CXSCREEN);
        sh = GetSystemMetrics(SM_CYSCREEN);
    }

    int imgW = (int)firstFrame->GetWidth();
    int imgH = (int)firstFrame->GetHeight();
    if (imgW <= 0 || imgH <= 0) {
        for (Bitmap* f : frames) delete f;
        return;
    }

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
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    SolidBrush bg(Color(255, 15, 23, 42));
    g.FillRectangle(&bg, 0, 0, sw, sh);
    ImageAttributes ia;
    ia.SetWrapMode(WrapModeTileFlipXY);
    g.DrawImage(firstFrame, Rect(imgX, imgY, drawW, drawH), 0, 0, imgW, imgH, UnitPixel, &ia);

    overlayImgDrawRect = { imgX, imgY, imgX + drawW, imgY + drawH };
    if (frames.size() > 1) {
        overlayFrames = std::move(frames);
        overlayFrameDelaysMs = std::move(delays);
        overlayActiveFrameIdx = 0;
        overlayFrameStripCollapsed = false;
        overlayFrameScrollX = 0.0f;
        LayoutOverlayFrameStrip(sw, sh);
    } else {
        overlaySingleImageBmp = firstFrame;
    }

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

static RECT GetPinnedSlotScreenBounds(int imgX, int imgY, int dispW, int dispH, bool hideToolbar, const RECT& rcScreen) {
    const int stripW = 242;
    const int totalHudH = 28 * 2 + 3;
    const int hudGap = 8;
    int sImgL = imgX;
    int sImgT = imgY;
    int sImgR = sImgL + dispW;
    int sImgB = sImgT + dispH;
    if (hideToolbar) {
        return { sImgL, sImgT, sImgR, sImgB };
    }
    int sStripR = std::max((int)rcScreen.left + stripW + 6, std::min((int)rcScreen.right - 6, sImgR));
    int sStripT = sImgB + hudGap;
    bool hasSpaceBelow = (sImgB + hudGap + totalHudH <= rcScreen.bottom - 6) && (sImgB + hudGap >= rcScreen.top + 6);
    bool hasSpaceAbove = (sImgT - hudGap - totalHudH >= rcScreen.top + 6) && (sImgT - hudGap <= rcScreen.bottom - 6);
    if (hasSpaceBelow) {
        sStripT = sImgB + hudGap;
    } else if (hasSpaceAbove) {
        sStripT = sImgT - hudGap - totalHudH;
    } else {
        sStripR = std::max((int)rcScreen.left + stripW + 6, std::min((int)rcScreen.right - 6, sImgR - 8));
        sStripT = std::max((int)rcScreen.top + 6, std::min((int)rcScreen.bottom - totalHudH - 6, sImgB - totalHudH - 8));
    }
    int sStripL = sStripR - stripW;
    int sStripB = sStripT + totalHudH;
    return {
        std::min(sImgL, sStripL),
        std::min(sImgT, sStripT),
        std::max(sImgR, sStripR),
        std::max(sImgB, sStripB)
    };
}

static void FindNonOverlappingPinPosition(
    const std::vector<HWND>& existingPins,
    int dispW,
    int dispH,
    bool hideToolbar,
    const RECT& rcScreen,
    int& outImgX,
    int& outImgY
) {
    const int stripW = 242;
    const int hudExtraH = hideToolbar ? 0 : (8 + 28 * 2 + 3);
    const int pad = 14;

    int leftOverhang = hideToolbar ? 0 : std::max(0, stripW - dispW);
    int minImgX = rcScreen.left + 12 + leftOverhang;
    int maxImgX = std::max(minImgX, (int)rcScreen.right - 12 - dispW);
    int minImgY = rcScreen.top + 12;
    int maxImgY = std::max(minImgY, (int)rcScreen.bottom - 12 - dispH - hudExtraH);

    int centerImgX = std::max(minImgX, std::min(maxImgX, (int)(rcScreen.left + ((rcScreen.right - rcScreen.left) - dispW) / 2)));
    int centerImgY = std::max(minImgY, std::min(maxImgY, (int)(rcScreen.top + ((rcScreen.bottom - rcScreen.top) - (dispH + hudExtraH)) / 2)));

    std::vector<RECT> occupied;
    for (HWND hp : existingPins) {
        if (!hp || !IsWindow(hp)) continue;
        PinnedWindowData* pData = (PinnedWindowData*)GetWindowLongPtrW(hp, GWLP_USERDATA);
        if (!pData || !pData->bmp) continue;
        int pW = 32, pH = 32;
        GetPinnedScaledDims(pData, pW, pH);
        RECT r = GetPinnedSlotScreenBounds(pData->screenImgX, pData->screenImgY, pW, pH, pData->hideToolbar, rcScreen);
        r.left   -= pad;
        r.top    -= pad;
        r.right  += pad;
        r.bottom += pad;
        occupied.push_back(r);
    }

    if (occupied.empty()) {
        outImgX = centerImgX;
        outImgY = centerImgY;
        return;
    }

    auto calcOverlapArea = [&](int candX, int candY) -> long long {
        RECT cand = GetPinnedSlotScreenBounds(candX, candY, dispW, dispH, hideToolbar, rcScreen);
        long long totalOverlap = 0;
        for (const RECT& occ : occupied) {
            RECT inter = {0};
            if (IntersectRect(&inter, &cand, &occ)) {
                totalOverlap += (long long)(inter.right - inter.left) * (long long)(inter.bottom - inter.top);
            }
        }
        return totalOverlap;
    };

    // 1. Try smart relative positions adjacent to existing pinned windows (right, left, below, above)
    std::vector<POINT> candidates;
    candidates.push_back({ centerImgX, centerImgY });
    for (auto it = existingPins.rbegin(); it != existingPins.rend(); ++it) {
        HWND hp = *it;
        if (!hp || !IsWindow(hp)) continue;
        PinnedWindowData* pData = (PinnedWindowData*)GetWindowLongPtrW(hp, GWLP_USERDATA);
        if (!pData || !pData->bmp) continue;
        int pW = 32, pH = 32;
        GetPinnedScaledDims(pData, pW, pH);
        RECT pb = GetPinnedSlotScreenBounds(pData->screenImgX, pData->screenImgY, pW, pH, pData->hideToolbar, rcScreen);
        int rightX = pb.right + pad + leftOverhang;
        int leftX  = pb.left - pad - dispW;
        int belowY = pb.bottom + pad;
        int aboveY = pb.top - pad - dispH - hudExtraH;
        if (rightX >= minImgX && rightX <= maxImgX) {
            candidates.push_back({ rightX, std::max(minImgY, std::min(maxImgY, pData->screenImgY)) });
        }
        if (leftX >= minImgX && leftX <= maxImgX) {
            candidates.push_back({ leftX, std::max(minImgY, std::min(maxImgY, pData->screenImgY)) });
        }
        if (belowY >= minImgY && belowY <= maxImgY) {
            candidates.push_back({ std::max(minImgX, std::min(maxImgX, pData->screenImgX)), belowY });
        }
        if (aboveY >= minImgY && aboveY <= maxImgY) {
            candidates.push_back({ std::max(minImgX, std::min(maxImgX, pData->screenImgX)), aboveY });
        }
    }

    for (const POINT& pt : candidates) {
        if (calcOverlapArea(pt.x, pt.y) == 0) {
            outImgX = pt.x;
            outImgY = pt.y;
            return;
        }
    }

    // 2. Scan work area grid for a zero-overlap spot (or minimum overlap if screen is completely packed)
    int bestX = centerImgX;
    int bestY = centerImgY;
    long long bestOverlap = calcOverlapArea(bestX, bestY);
    const int step = 20;
    for (int y = minImgY; y <= maxImgY; y += step) {
        for (int x = minImgX; x <= maxImgX; x += step) {
            long long ov = calcOverlapArea(x, y);
            if (ov == 0) {
                outImgX = x;
                outImgY = y;
                return;
            }
            if (ov < bestOverlap) {
                bestOverlap = ov;
                bestX = x;
                bestY = y;
            }
        }
    }
    outImgX = bestX;
    outImgY = bestY;
}

void PepperSnapDaemon::OpenImageFilesToPinOnTop(const std::vector<std::wstring>& filePaths) {
    struct LoadedPinItem {
        std::vector<Bitmap*> frames;
        std::vector<int> delays;
        int origW = 0;
        int origH = 0;
        float scale = 1.0f;
        int dispW = 32;
        int dispH = 32;
    };

    std::vector<LoadedPinItem> items;
    for (const auto& path : filePaths) {
        if (path.empty()) continue;
        LoadedPinItem item;
        if (LoadImageFramesFromFile(path, item.frames, item.delays) && !item.frames.empty() && item.frames[0]) {
            item.origW = (int)item.frames[0]->GetWidth();
            item.origH = (int)item.frames[0]->GetHeight();
            if (item.origW > 0 && item.origH > 0) {
                items.push_back(std::move(item));
            } else {
                for (Bitmap* f : item.frames) delete f;
            }
        }
    }
    if (items.empty()) return;

    POINT ptCur = { 0, 0 };
    GetCursorPos(&ptCur);
    HMONITOR hMon = MonitorFromPoint(ptCur, MONITOR_DEFAULTTOPRIMARY);
    RECT rcScreen = GetPinnedMonitorWorkArea(hMon);
    MONITORINFO mi = { sizeof(mi) };
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    if (hMon && GetMonitorInfoW(hMon, &mi)) {
        sw = std::max(1, (int)(mi.rcMonitor.right - mi.rcMonitor.left));
        sh = std::max(1, (int)(mi.rcMonitor.bottom - mi.rcMonitor.top));
    }

    // Limit pinned image size on open to 0.4 of screen size, fitting proportionally so aspect ratio is never distorted
    int maxOpenW = std::max(64, (int)std::round(sw * 0.40));
    int maxOpenH = std::max(64, (int)std::round(sh * 0.40));

    int n = (int)items.size();
    bool hasExistingPins = false;
    for (HWND hp : pinnedWindows) {
        if (hp && IsWindow(hp)) {
            hasExistingPins = true;
            break;
        }
    }

    const int stripW = 242;
    const int hudExtraH = hidePinnedToolbar ? 0 : (8 + 28 * 2 + 3);
    const int gap = 16;

    if (n > 1 && !hasExistingPins) {
        int cols = (int)std::ceil(std::sqrt((double)n));
        if (n == 2) cols = 2;
        else if (n == 3) cols = 3;
        int rows = (n + cols - 1) / cols;

        int workW = std::max(320, (int)(rcScreen.right - rcScreen.left) - 24);
        int workH = std::max(240, (int)(rcScreen.bottom - rcScreen.top) - 24);
        if (cols == 3 && workW < cols * (stripW + gap)) {
            cols = 2;
            rows = (n + cols - 1) / cols;
        }

        int cellMaxW = std::min(maxOpenW, std::max(64, (workW - (cols - 1) * gap) / cols));
        int cellMaxH = std::min(maxOpenH, std::max(64, (workH - (rows - 1) * gap) / rows - hudExtraH));

        for (auto& it : items) {
            float sc = 1.0f;
            if (it.origW > cellMaxW || it.origH > cellMaxH) {
                sc = (float)std::min((double)cellMaxW / (double)it.origW, (double)cellMaxH / (double)it.origH);
            }
            sc = std::max(0.01f, sc);
            it.scale = sc;
            it.dispW = std::max(1, (int)std::round(it.origW * sc));
            it.dispH = std::max(1, (int)std::round(it.origH * sc));
        }

        std::vector<int> rowHeights(rows, 0);
        std::vector<int> rowWidths(rows, 0);
        for (int r = 0; r < rows; ++r) {
            int firstIdx = r * cols;
            int lastIdx = std::min(n, firstIdx + cols);
            int rW = 0;
            int rH = 0;
            for (int idx = firstIdx; idx < lastIdx; ++idx) {
                int slotW = hidePinnedToolbar ? items[idx].dispW : std::max(items[idx].dispW, stripW);
                int slotH = items[idx].dispH + hudExtraH;
                rW += slotW + (idx > firstIdx ? gap : 0);
                rH = std::max(rH, slotH);
            }
            rowWidths[r] = rW;
            rowHeights[r] = rH;
        }

        int totalGridH = 0;
        for (int r = 0; r < rows; ++r) {
            totalGridH += rowHeights[r] + (r > 0 ? gap : 0);
        }

        int curY = std::max((int)rcScreen.top + 12, (int)(rcScreen.top + ((rcScreen.bottom - rcScreen.top) - totalGridH) / 2));
        for (int r = 0; r < rows; ++r) {
            int firstIdx = r * cols;
            int lastIdx = std::min(n, firstIdx + cols);
            int curX = std::max((int)rcScreen.left + 12, (int)(rcScreen.left + ((rcScreen.right - rcScreen.left) - rowWidths[r]) / 2));
            for (int idx = firstIdx; idx < lastIdx; ++idx) {
                auto& it = items[idx];
                int slotW = hidePinnedToolbar ? it.dispW : std::max(it.dispW, stripW);
                // When dispW < stripW, the toolbar extends to the left of the image's right edge
                int imgX = curX + (slotW - it.dispW);
                int imgY = curY + (rowHeights[r] - hudExtraH - it.dispH) / 2;
                Bitmap* firstBmp = it.frames[0];
                if (it.frames.size() > 1) {
                    CreatePinnedWindow(firstBmp, imgX, imgY, it.scale, std::move(it.frames), std::move(it.delays));
                } else {
                    CreatePinnedWindow(firstBmp, imgX, imgY, it.scale);
                }
                curX += slotW + gap;
            }
            curY += rowHeights[r] + gap;
        }
        return;
    }

    for (auto& it : items) {
        float initScale = 1.0f;
        if (it.origW > maxOpenW || it.origH > maxOpenH) {
            initScale = (float)std::min((double)maxOpenW / (double)it.origW, (double)maxOpenH / (double)it.origH);
        }
        initScale = std::max(0.01f, initScale);
        it.scale = initScale;
        it.dispW = std::max(1, (int)std::round(it.origW * initScale));
        it.dispH = std::max(1, (int)std::round(it.origH * initScale));

        int px = 0, py = 0;
        FindNonOverlappingPinPosition(pinnedWindows, it.dispW, it.dispH, hidePinnedToolbar, rcScreen, px, py);

        Bitmap* firstBmp = it.frames[0];
        if (it.frames.size() > 1) {
            CreatePinnedWindow(firstBmp, px, py, it.scale, std::move(it.frames), std::move(it.delays));
        } else {
            CreatePinnedWindow(firstBmp, px, py, it.scale);
        }
    }
}

void PepperSnapDaemon::OpenImageFileToPinOnTop(const std::wstring& filePath) {
    if (filePath.empty()) return;
    OpenImageFilesToPinOnTop({ filePath });
}

void PepperSnapDaemon::OpenImageToPinOnTop() {
    std::vector<WCHAR> szFileBuf(65536, 0);
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hTrayWnd;
    ofn.lpstrFile = szFileBuf.data();
    ofn.nMaxFile = (DWORD)szFileBuf.size();
    ofn.lpstrFilter = L"Image files (*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.gif;*.tif;*.tiff;*.ico)\0*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.gif;*.tif;*.tiff;*.ico\0All files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    if (GetOpenFileNameW(&ofn)) {
        std::vector<std::wstring> selectedFiles;
        const WCHAR* p = szFileBuf.data();
        std::wstring firstPart(p);
        p += firstPart.size() + 1;
        if (*p == L'\0') {
            if (!firstPart.empty()) selectedFiles.push_back(firstPart);
        } else {
            std::wstring dir = firstPart;
            if (!dir.empty() && dir.back() != L'\\' && dir.back() != L'/') {
                dir += L"\\";
            }
            while (*p != L'\0') {
                std::wstring fileName(p);
                p += fileName.size() + 1;
                if (!fileName.empty()) {
                    selectedFiles.push_back(dir + fileName);
                }
            }
        }
        if (!selectedFiles.empty()) {
            OpenImageFilesToPinOnTop(selectedFiles);
        }
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

static LRESULT CALLBACK TrayDaemonWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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
            if (cds && (cds->dwData == COPYDATA_OPEN_IMAGE || cds->dwData == COPYDATA_PIN_IMAGE) &&
                cds->lpData && cds->cbData >= sizeof(wchar_t)) {
                const wchar_t* wpath = (const wchar_t*)cds->lpData;
                size_t maxChars = cds->cbData / sizeof(wchar_t);
                std::wstring path(wpath, wcsnlen(wpath, maxChars));
                if (!path.empty()) {
                    if (cds->dwData == COPYDATA_PIN_IMAGE) {
                        g_Daemon.pendingPinFiles.push_back(path);
                        SetTimer(hWnd, TIMER_PROCESS_PIN_QUEUE, 65, nullptr);
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
            std::vector<std::wstring> toPin;
            toPin.swap(g_Daemon.pendingPinFiles);
            if (!toPin.empty()) {
                g_Daemon.OpenImageFilesToPinOnTop(toPin);
            }
            if (!g_Daemon.pendingEditFile.empty()) {
                std::wstring editPath = g_Daemon.pendingEditFile;
                g_Daemon.pendingEditFile.clear();
                g_Daemon.OpenImageFileIntoOverlay(editPath);
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
        case WM_HOTKEY:
            if (g_RecordingHotkeySlot != 0) return 0;
            if (wParam == HK_REGION_SNIP) PostMessageW(hWnd, WM_TRIGGER_REGION_SNIP, 0, 0);
            else if (wParam == HK_FULL_SNAP) PostMessageW(hWnd, WM_TRIGGER_FULL_SNAP, 0, 0);
            else if (wParam == HK_PREV_REGION) PostMessageW(hWnd, WM_TRIGGER_PREV_REGION, 0, 0);
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
    std::vector<std::wstring> cliPinFilePaths;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            std::wstring low = arg;
            for (wchar_t& c : low) c = (wchar_t)towlower(c);
            if (low == L"--noelevate" || low == L"/noelevate") {
                continue;
            }
            if ((low == L"--pin" || low == L"/pin") && i + 1 < argc) {
                while (i + 1 < argc) {
                    std::wstring nextArg = argv[i + 1];
                    std::wstring nextLow = nextArg;
                    for (wchar_t& c : nextLow) c = (wchar_t)towlower(c);
                    if (nextLow == L"--noelevate" || nextLow == L"/noelevate") {
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
            } else if ((low == L"--edit" || low == L"/edit" || low == L"--open" || low == L"/open") && i + 1 < argc) {
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
        if (FindWindowW(L"PepperSnapTrayDaemonClass", currentDaemonTitle.c_str()) == hExisting) {
            return true;
        }
        WCHAR existingTitle[128] = {0};
        GetWindowTextW(hExisting, existingTitle, 127);
        return (wcscmp(existingTitle, currentDaemonTitle.c_str()) == 0);
    };

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

    // Serialize multi-select Explorer CLI invocations so concurrent "--pin" processes forward cleanly in order
    HANDLE hCliForwardMutex = nullptr;
    if (cmdArgs.find(L"--noelevate") == std::wstring::npos) {
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

    // If an existing PepperSnap instance of the SAME version is already running, forward CLI commands immediately
    HWND hExistingPreCheck = FindWindowW(L"PepperSnapTrayDaemonClass", currentDaemonTitle.c_str());
    if (!hExistingPreCheck) {
        HWND hAny = FindWindowW(L"PepperSnapTrayDaemonClass", nullptr);
        if (hAny && isSameVersionDaemon(hAny)) {
            hExistingPreCheck = hAny;
        }
    }
    if (hExistingPreCheck) {
        forwardToRunningDaemon(hExistingPreCheck);
        releaseCliForwardMutex();
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
                // Wait briefly for the newly elevated daemon to create its tray window before releasing the CLI mutex
                for (int w = 0; w < 125; ++w) {
                    if (FindWindowW(L"PepperSnapTrayDaemonClass", currentDaemonTitle.c_str()) != nullptr) break;
                    Sleep(40);
                }
                releaseCliForwardMutex();
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
        HWND hExisting = nullptr;
        for (int w = 0; w < 50; ++w) {
            hExisting = FindWindowW(L"PepperSnapTrayDaemonClass", nullptr);
            if (hExisting) break;
            Sleep(40);
        }
        if (hExisting) {
            if (isSameVersionDaemon(hExisting)) {
                forwardToRunningDaemon(hExisting);
                releaseCliForwardMutex();
                if (hMutex) CloseHandle(hMutex);
                return 0;
            }
            shutdownOlderDaemon(hExisting);
        }
    }
    if (cliMsg == WM_COMMAND && cliWParam == IDM_TRAY_EXIT) {
        releaseCliForwardMutex();
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
    ChangeWindowMessageFilterEx(g_Daemon.hTrayWnd, WM_COMMAND, MSGFLT_ALLOW, nullptr);

    PepperSnapDaemon::RegisterImageContextMenu();
    g_Daemon.InitTrayIcon();
    g_Daemon.ApplyGlobalHotkeys();

    HANDLE hInputThread = CreateThread(nullptr, 0, InputHookWorkerThread, nullptr, 0, &g_InputHookThreadId);

    // Periodic background timer (every 30 minutes) + initial startup check for scheduled GitHub Releases updates
    SetTimer(g_Daemon.hTrayWnd, TIMER_AUTO_UPDATE_CHECK, 30 * 60 * 1000, nullptr);
    g_Daemon.MaybeRunScheduledUpdateCheck();

    releaseCliForwardMutex();

    if (!cliPinFilePaths.empty()) {
        g_Daemon.OpenImageFilesToPinOnTop(cliPinFilePaths);
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
