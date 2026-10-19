#pragma once

#include "PepperSnap.h"

// ----------------------------------------------------------------------------
// Internal declarations & shared state for PepperSnap translation units
// ----------------------------------------------------------------------------

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
    std::wstring pendingSearchFile;

    static constexpr const wchar_t* APP_VERSION = L"4.1.19";
    static constexpr const wchar_t* DEFAULT_GITHUB_REPO = L"finnuen/PepperSnap";

    // Configuration (Desktop default, PNG default for custom area & copy, JPEG for fullscreen, unified Options modal)
    std::wstring saveFolder;
    std::wstring namingPattern = L"PepperSnap_{N}";
    static constexpr const wchar_t* DEFAULT_NAMING_PATTERN = L"PepperSnap_{N}";
    ImageFormat regionFormat = ImageFormat::PNG;
    ImageFormat fullscreenFormat = ImageFormat::JPEG;
    ImageFormat copyFormat = ImageFormat::PNG;
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
    bool pinnedImageAboveEdit = true;
    bool keepEditWindowOpen = false;
    bool runElevated = true;
    bool disableRightClickIntegration = false;
    bool disableMagnifierWindow = false;
    bool enableWebImageSearch = true;
    // What the shell context-menu verbs were last written for. SaveSettings() runs
    // on every overlay close (twice, via RecordLastCustomSelection), and rewriting
    // the verbs is a few hundred registry writes; they only depend on these three
    // inputs, so they are only rewritten when one of them has changed. See
    // SyncImageContextMenuRegistration in Registry.cpp.
    mutable bool ctxMenuSynced = false;
    mutable bool ctxMenuSyncedRegistered = false;
    mutable bool ctxMenuSyncedWebSearch = false;
    mutable std::wstring ctxMenuSyncedExePath;
    // Target language for "Shift + click Search on web" (Google Translate's
    // tl= code). Lives in settings.ini as TranslateTargetLang.
    std::wstring translateTargetLang = L"en";
    int optionsWindowHeight = 944;
    int customizeKeysWindowHeight = 620;
    int shortcutsWindowWidth = 900;
    int shortcutsWindowHeight = 781;
    int namingSyntaxWindowHeight = 760;
    bool autoCheckUpdates = true;
    bool autoUpdatePromptShown = true;
    bool promptClipboardHistory = true;
    UpdateCheckInterval updateInterval = UpdateCheckInterval::EveryDay;
    long long lastUpdateCheckTime = 0;
    std::wstring updateGithubRepo = DEFAULT_GITHUB_REPO;
    bool isCheckingUpdate = false;

    // Custom Global Hotkey Bindings (Defaults: Ctrl+PrtScn, Shift+PrtScn, Ctrl+Shift+PrtScn, Shift+Alt+PrtScn, Alt+PrtScn)
    HotkeyBinding hkRegionSnip{ MOD_CONTROL, VK_SNAPSHOT };
    HotkeyBinding hkFullSnap{ MOD_SHIFT, VK_SNAPSHOT };
    HotkeyBinding hkPrevRegion{ MOD_CONTROL | MOD_SHIFT, VK_SNAPSHOT };
    HotkeyBinding hkPinClipboard{ MOD_ALT | MOD_SHIFT, VK_SNAPSHOT };
    HotkeyBinding hkEditClipboard{ MOD_CONTROL | MOD_ALT, VK_SNAPSHOT };
    HotkeyBinding hkSearchGoogle{ MOD_ALT, VK_SNAPSHOT };

    // Custom Area Tool Shortcut Bindings
    // Unbound out of the box: single letters clash with in-place text editing
    // and with the tools' own modifier behaviour, so they are opt-in.
    HotkeyBinding hkToolSelect{ 0, 0 };
    HotkeyBinding hkToolPen{ 0, 0 };
    HotkeyBinding hkToolStabilo{ 0, 0 };
    HotkeyBinding hkToolLine{ 0, 0 };
    HotkeyBinding hkToolArrow{ 0, 0 };
    HotkeyBinding hkToolNumber{ 0, 0 };
    HotkeyBinding hkToolResetNum{ 0, 0 };
    HotkeyBinding hkToolRect{ 0, 0 };
    HotkeyBinding hkToolEllipse{ 0, 0 };
    HotkeyBinding hkToolText{ 0, 0 };
    HotkeyBinding hkToolMosaicSq{ 0, 0 };
    HotkeyBinding hkToolMosaicCir{ 0, 0 };

    // Custom Area Toolbar Action Shortcut Bindings
    HotkeyBinding hkActUndo{ MOD_CONTROL, 'Z' };
    HotkeyBinding hkActRedo{ MOD_CONTROL, 'Y' };
    HotkeyBinding hkActClear{ MOD_CONTROL, 'D' };
    HotkeyBinding hkActPin{ 0, 0 };
    HotkeyBinding hkActOcr{ 0, 0 };
    HotkeyBinding hkActSaveAs{ MOD_CONTROL | MOD_SHIFT, 'S' };
    HotkeyBinding hkActSave{ MOD_CONTROL, 'S' };
    HotkeyBinding hkActCopy{ MOD_CONTROL, 'C' };
    HotkeyBinding hkActSearchGoogle{ 0, 0 };

    // Multi-Frame GIF / WebP Editor Shortcut Bindings
    HotkeyBinding hkFramePlayPause{ 0, VK_SPACE };
    HotkeyBinding hkFrameToggleStrip{ 0, 0 };

    // Pinned Image Toolbar Shortcut Bindings
    HotkeyBinding hkPinZoomOut{ 0, 0 };
    HotkeyBinding hkPinZoomIn{ 0, 0 };
    HotkeyBinding hkPinZoomReset{ 0, 0 };
    HotkeyBinding hkPinPosReset{ 0, 0 };
    HotkeyBinding hkPinOutline{ 0, 0 };
    HotkeyBinding hkPinHideToolbar{ 0, 0 };
    HotkeyBinding hkPinSmooth{ 0, 0 };
    HotkeyBinding hkPinSaveAs{ MOD_CONTROL | MOD_SHIFT, 'S' };
    HotkeyBinding hkPinSave{ MOD_CONTROL, 'S' };
    HotkeyBinding hkPinCopy{ MOD_CONTROL, 'C' };
    HotkeyBinding hkPinSearchGoogle{ 0, 0 };
    HotkeyBinding hkPinClose{ 0, VK_ESCAPE };

    // Previously Selected Custom Window Size & Position
    bool hasLastCustomSel = false;
    RECT lastCustomSelRect{ 0, 0, 0, 0 };
    bool lastCustomSelHasQuad = false;
    bool lastCustomSelHasEllipse = false;
    POINT lastCustomSelQuad[4] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};

    // Virtual Screen Metrics
    int vScreenX = 0;
    int vScreenY = 0;
    int vScreenW = 1920;
    int vScreenH = 1080;

    // Active Overlay Session State
    Bitmap* frozenDesktopBmp = nullptr;
    bool hasSelection = false;
    RECT selRect{0, 0, 0, 0};
    bool hasCustomQuad = false;
    bool hasEllipseSelection = false;
    bool altHeldOnSelectionStart = false;
    POINT selQuad[4] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    POINT dragOrigQuad[4] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    bool isDraggingSingleCorner = false;
    std::vector<RECT> desktopWindowRects;
    std::vector<HWND> suppressedAboveOverlayWindows;
    bool hasCtrlHoverWindow = false;
    RECT ctrlHoverWindowRect{0, 0, 0, 0};
    RECT dimPillRect{0, 0, 0, 0};
    RECT ratioPillRect{0, 0, 0, 0};
    RECT ratioResetBtnRect{0, 0, 0, 0};
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

    // Typeable Custom Ratio Indicator State (With Caret, Partial Selection & Locked Custom Ratio)
    bool isEditingRatio = false;
    bool isDraggingRatioText = false;
    std::wstring editingRatioText;
    size_t ratioCaretPos = 0;
    size_t ratioSelAnchor = 0;
    bool hasCustomRatio = false;
    int customRatioW = 0;
    int customRatioH = 0;
    bool ratioResetBtnPressed = false;
    bool lastRatioHover = false;
    bool lastRatioResetHover = false;

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
    bool isRightMouseHeld = false;
    int activeSelectionRatioW = 0;
    int activeSelectionRatioH = 0;
    POINT selectionTargetPt{0, 0};

    static int ComputeIntGcd(int a, int b) {
        a = std::abs(a);
        b = std::abs(b);
        while (b != 0) {
            int t = a % b;
            a = b;
            b = t;
        }
        return std::max(1, a);
    }

    std::wstring FormatSelectionRatioString(int sw, int sh) const {
        if (activeSelectionRatioW > 0 && activeSelectionRatioH > 0) {
            return std::to_wstring(activeSelectionRatioW) + L":" + std::to_wstring(activeSelectionRatioH);
        }
        if (hasCustomRatio && customRatioW > 0 && customRatioH > 0) {
            return std::to_wstring(customRatioW) + L":" + std::to_wstring(customRatioH);
        }
        int w = std::max(1, sw);
        int h = std::max(1, sh);
        int g = ComputeIntGcd(w, h);
        return std::to_wstring(w / g) + L":" + std::to_wstring(h / g);
    }

    bool IsSelectionAnchorPlaced() const {
        if (dragMode == DragMode::CreatingSelection) return true;
        if (dragMode >= DragMode::ResizeTL && dragMode <= DragMode::ResizeL) return true;
        if (dragMode == DragMode::PendingOutsideSelection && !pendingShiftColorPick && !pendingCtrlWindowSelect) return true;
        return false;
    }

    bool GetActiveSelectionAspectRatio(WPARAM mouseWParam, int& outRatioW, int& outRatioH, bool* outIsTempOverride = nullptr) const {
        if (outIsTempOverride) *outIsTempOverride = false;
        bool rButtonDown = isRightMouseHeld || ((mouseWParam & MK_RBUTTON) != 0);
        bool shiftDownAfterAnchor = IsSelectionAnchorPlaced() &&
            (((mouseWParam & MK_SHIFT) != 0) || ((GetKeyState(VK_SHIFT) & 0x8000) != 0));
        if (rButtonDown || shiftDownAfterAnchor) {
            outRatioW = 1;
            outRatioH = 1;
            if (outIsTempOverride) *outIsTempOverride = true;
            return true;
        }
        auto isKeyDown = [](int vk1, int vk2) -> bool {
            return ((GetKeyState(vk1) & 0x8000) != 0) ||
                   ((GetKeyState(vk2) & 0x8000) != 0);
        };
        if (isKeyDown('1', VK_NUMPAD1)) {
            outRatioW = 16;
            outRatioH = 9;
            if (outIsTempOverride) *outIsTempOverride = true;
            return true;
        }
        if (isKeyDown('2', VK_NUMPAD2)) {
            outRatioW = 9;
            outRatioH = 16;
            if (outIsTempOverride) *outIsTempOverride = true;
            return true;
        }
        if (isKeyDown('3', VK_NUMPAD3)) {
            outRatioW = 1;
            outRatioH = 2;
            if (outIsTempOverride) *outIsTempOverride = true;
            return true;
        }
        if (isKeyDown('4', VK_NUMPAD4)) {
            outRatioW = 2;
            outRatioH = 1;
            if (outIsTempOverride) *outIsTempOverride = true;
            return true;
        }
        if (hasCustomRatio && customRatioW > 0 && customRatioH > 0) {
            outRatioW = customRatioW;
            outRatioH = customRatioH;
            return true;
        }
        outRatioW = 0;
        outRatioH = 0;
        return false;
    }

    bool HasCreatedCustomArea() const {
        if (!hasSelection) return false;
        if (dragMode == DragMode::CreatingSelection || dragMode == DragMode::PendingOutsideSelection) return false;
        return (selRect.right != selRect.left) && (selRect.bottom != selRect.top);
    }

    void SyncQuadFromSelRect() {
        int x1 = std::min(selRect.left, selRect.right);
        int y1 = std::min(selRect.top, selRect.bottom);
        int x2 = std::max(selRect.left, selRect.right);
        int y2 = std::max(selRect.top, selRect.bottom);
        selQuad[0] = { x1, y1 };
        selQuad[1] = { x2, y1 };
        selQuad[2] = { x2, y2 };
        selQuad[3] = { x1, y2 };
        hasCustomQuad = false;
    }

    void SyncSelRectFromQuad() {
        int minX = std::min(std::min((int)selQuad[0].x, (int)selQuad[1].x), std::min((int)selQuad[2].x, (int)selQuad[3].x));
        int maxX = std::max(std::max((int)selQuad[0].x, (int)selQuad[1].x), std::max((int)selQuad[2].x, (int)selQuad[3].x));
        int minY = std::min(std::min((int)selQuad[0].y, (int)selQuad[1].y), std::min((int)selQuad[2].y, (int)selQuad[3].y));
        int maxY = std::max(std::max((int)selQuad[0].y, (int)selQuad[1].y), std::max((int)selQuad[2].y, (int)selQuad[3].y));
        selRect = { minX, minY, maxX, maxY };
        hasCustomQuad = !(selQuad[0].x == minX && selQuad[0].y == minY &&
                          selQuad[1].x == maxX && selQuad[1].y == minY &&
                          selQuad[2].x == maxX && selQuad[2].y == maxY &&
                          selQuad[3].x == minX && selQuad[3].y == maxY);
    }

    void ScaleQuadFromOrigRect(const POINT origQuad[4], const RECT& origRect, const RECT& newRect) {
        int ox0 = std::min(origRect.left, origRect.right);
        int oy0 = std::min(origRect.top, origRect.bottom);
        int ow  = std::max(1, (int)std::abs(origRect.right - origRect.left));
        int oh  = std::max(1, (int)std::abs(origRect.bottom - origRect.top));
        int nx0 = std::min(newRect.left, newRect.right);
        int ny0 = std::min(newRect.top, newRect.bottom);
        int nw  = std::max(1, (int)std::abs(newRect.right - newRect.left));
        int nh  = std::max(1, (int)std::abs(newRect.bottom - newRect.top));
        for (int i = 0; i < 4; ++i) {
            double u = (double)(origQuad[i].x - ox0) / (double)ow;
            double v = (double)(origQuad[i].y - oy0) / (double)oh;
            selQuad[i].x = std::max(0, std::min(vScreenW, nx0 + (int)std::round(u * nw)));
            selQuad[i].y = std::max(0, std::min(vScreenH, ny0 + (int)std::round(v * nh)));
        }
        SyncSelRectFromQuad();
    }

    void GetSelectionQuadPoints(POINT outPts[4]) const {
        if (hasCustomQuad) {
            for (int i = 0; i < 4; ++i) outPts[i] = selQuad[i];
        } else {
            int x1 = std::min(selRect.left, selRect.right);
            int y1 = std::min(selRect.top, selRect.bottom);
            int x2 = std::max(selRect.left, selRect.right);
            int y2 = std::max(selRect.top, selRect.bottom);
            outPts[0] = { x1, y1 };
            outPts[1] = { x2, y1 };
            outPts[2] = { x2, y2 };
            outPts[3] = { x1, y2 };
        }
    }

    void GetSelectionHandlePoints(POINT outHandles[8]) const {
        POINT q[4];
        GetSelectionQuadPoints(q);
        outHandles[0] = q[0];                                                   // TL
        outHandles[1] = { (q[0].x + q[1].x) / 2, (q[0].y + q[1].y) / 2 };       // T
        outHandles[2] = q[1];                                                   // TR
        outHandles[3] = { (q[1].x + q[2].x) / 2, (q[1].y + q[2].y) / 2 };       // R
        outHandles[4] = q[2];                                                   // BR
        outHandles[5] = { (q[2].x + q[3].x) / 2, (q[2].y + q[3].y) / 2 };       // B
        outHandles[6] = q[3];                                                   // BL
        outHandles[7] = { (q[3].x + q[0].x) / 2, (q[3].y + q[0].y) / 2 };       // L
    }

    void CanonicalizeSnappedQuad() {
        auto samePt = [](POINT a, POINT b) { return a.x == b.x && a.y == b.y; };
        if (samePt(selQuad[0], selQuad[2]) && !samePt(selQuad[1], selQuad[3])) {
            selQuad[0] = selQuad[1];
        } else if (samePt(selQuad[1], selQuad[3]) && !samePt(selQuad[0], selQuad[2])) {
            selQuad[1] = selQuad[2];
        }
    }

    int GetSelectionPolygonPoints(POINT outPts[4]) const {
        POINT q[4];
        GetSelectionQuadPoints(q);
        int count = 0;
        for (int i = 0; i < 4; ++i) {
            bool dup = false;
            for (int j = 0; j < count; ++j) {
                if (q[i].x == outPts[j].x && q[i].y == outPts[j].y) {
                    dup = true;
                    break;
                }
            }
            if (!dup) {
                outPts[count++] = q[i];
            }
        }
        return count;
    }

    static long long QuadSignedArea2(const POINT q[4]) {
        long long s = 0;
        for (int i = 0; i < 4; ++i) {
            int j = (i + 1) & 3;
            s += (long long)q[i].x * (long long)q[j].y - (long long)q[j].x * (long long)q[i].y;
        }
        return s;
    }

    static bool SegmentsProperlyIntersect(POINT a, POINT b, POINT c, POINT d) {
        auto cross = [](POINT o, POINT p, POINT q) -> long long {
            return (long long)(p.x - o.x) * (long long)(q.y - o.y) -
                   (long long)(p.y - o.y) * (long long)(q.x - o.x);
        };
        long long d1 = cross(c, d, a);
        long long d2 = cross(c, d, b);
        long long d3 = cross(a, b, c);
        long long d4 = cross(a, b, d);
        return (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
                ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0)));
    }

    static bool IsQuadSelfIntersecting(const POINT q[4]) {
        return SegmentsProperlyIntersect(q[0], q[1], q[2], q[3]) ||
               SegmentsProperlyIntersect(q[1], q[2], q[3], q[0]);
    }

    bool IsPointInSelection(POINT pt) const {
        if (!hasSelection) return false;
        if (hasEllipseSelection) {
            int x1 = std::min(selRect.left, selRect.right);
            int y1 = std::min(selRect.top, selRect.bottom);
            int x2 = std::max(selRect.left, selRect.right);
            int y2 = std::max(selRect.top, selRect.bottom);
            double rx = (double)(x2 - x1) * 0.5;
            double ry = (double)(y2 - y1) * 0.5;
            if (rx < 0.5 || ry < 0.5) return false;
            double cx = (double)x1 + rx;
            double cy = (double)y1 + ry;
            double nx = ((double)pt.x + 0.5 - cx) / rx;
            double ny = ((double)pt.y + 0.5 - cy) / ry;
            return (nx * nx + ny * ny) <= 1.0;
        }
        if (!hasCustomQuad) {
            RECT normSel = {
                std::min(selRect.left, selRect.right),
                std::min(selRect.top, selRect.bottom),
                std::max(selRect.left, selRect.right),
                std::max(selRect.top, selRect.bottom)
            };
            return PtInRect(&normSel, pt) != FALSE;
        }
        POINT polyPts[4];
        int polyCount = GetSelectionPolygonPoints(polyPts);
        if (polyCount < 3) return false;
        bool inside = false;
        double px = (double)pt.x + 0.5;
        double py = (double)pt.y + 0.5;
        for (int i = 0, j = polyCount - 1; i < polyCount; j = i++) {
            double xi = (double)polyPts[i].x, yi = (double)polyPts[i].y;
            double xj = (double)polyPts[j].x, yj = (double)polyPts[j].y;
            if (((yi > py) != (yj > py)) &&
                (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) {
                inside = !inside;
            }
        }
        return inside;
    }

    bool IsHoveringSelectedCustomArea(POINT pt) const {
        if (!HasCreatedCustomArea() || hasEllipseSelection) return false;
        if (dragMode != DragMode::None) return false;
        if (isEditingText || isEditingSize || isEditingRatio || isEditingStroke) return false;
        if (isDraggingStrokeText || isDraggingSizeText || isDraggingRatioText || isDraggingTextBoxSel) return false;
        if (hoveredBtnId != -1 || pressedBtnId != -1) return false;
        if (lastPillHover || lastRatioResetHover || lastStrokeHover) return false;
        if (PtInRect(&dimPillRect, pt) || PtInRect(&ratioPillRect, pt) ||
            (hasCustomRatio && PtInRect(&ratioResetBtnRect, pt)) ||
            PtInRect(&customStrokeRect, pt)) {
            return false;
        }
        for (const auto& b : dockButtons) {
            if (PtInRect(&b.rect, pt)) return false;
        }
        if (HasOverlayMultiFrames()) {
            if (PtInRect(&overlayFrameStripToggleBtnRect, pt)) return false;
            if (!overlayFrameStripCollapsed &&
                (PtInRect(&overlayFrameStripResizeRect, pt) || PtInRect(&overlayFrameStripPanelRect, pt))) {
                return false;
            }
        }
        return IsPointInSelection(pt) || (HitTestSelectionHandles(pt.x, pt.y) != DragMode::None);
    }

    static bool IsAltModifierHeld() {
        return ((GetKeyState(VK_MENU) & 0x8000) != 0) ||
               ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) ||
               ((GetAsyncKeyState(VK_LMENU) & 0x8000) != 0) ||
               ((GetAsyncKeyState(VK_RMENU) & 0x8000) != 0);
    }

    static void ApplyEllipseMaskToBitmap(Bitmap* bmp) {
        if (!bmp) return;
        int rw = (int)bmp->GetWidth();
        int rh = (int)bmp->GetHeight();
        if (rw < 1 || rh < 1) return;

        Bitmap maskBmp(rw, rh, PixelFormat32bppARGB);
        {
            Graphics gMask(&maskBmp);
            gMask.SetCompositingMode(CompositingModeSourceCopy);
            gMask.Clear(Color(0, 0, 0, 0));
            gMask.SetCompositingMode(CompositingModeSourceOver);
            gMask.SetSmoothingMode(SmoothingModeAntiAlias);
            gMask.SetPixelOffsetMode(PixelOffsetModeHalf);
            SolidBrush whiteBrush(Color(255, 255, 255, 255));
            gMask.FillEllipse(&whiteBrush, 0.0f, 0.0f, (REAL)rw, (REAL)rh);
        }

        Rect lockRc(0, 0, rw, rh);
        BitmapData bdOut = {}, bdMask = {};
        if (bmp->LockBits(&lockRc, ImageLockModeRead | ImageLockModeWrite, PixelFormat32bppARGB, &bdOut) == Ok) {
            if (maskBmp.LockBits(&lockRc, ImageLockModeRead, PixelFormat32bppARGB, &bdMask) == Ok) {
                for (int y = 0; y < rh; ++y) {
                    DWORD* outRow = (DWORD*)((BYTE*)bdOut.Scan0 + (size_t)y * bdOut.Stride);
                    const DWORD* maskRow = (const DWORD*)((const BYTE*)bdMask.Scan0 + (size_t)y * bdMask.Stride);
                    for (int x = 0; x < rw; ++x) {
                        DWORD mAlpha = (maskRow[x] >> 24) & 0xFFu;
                        if (mAlpha == 0) {
                            outRow[x] = 0x00000000u;
                        } else if (mAlpha < 255u) {
                            DWORD origA = (outRow[x] >> 24) & 0xFFu;
                            DWORD newA = (origA * mAlpha + 127u) / 255u;
                            outRow[x] = (outRow[x] & 0x00FFFFFFu) | (newA << 24);
                        }
                    }
                }
                maskBmp.UnlockBits(&bdMask);
            }
            bmp->UnlockBits(&bdOut);
        }
    }

    static void ApplyQuadPolygonMaskToBitmap(Bitmap* bmp, const POINT quadPts[4], int rx, int ry) {
        if (!bmp) return;
        int rw = (int)bmp->GetWidth();
        int rh = (int)bmp->GetHeight();
        if (rw < 1 || rh < 1) return;

        PointF localPts[4];
        int polyCount = 0;
        for (int i = 0; i < 4; ++i) {
            PointF pt((REAL)(quadPts[i].x - rx), (REAL)(quadPts[i].y - ry));
            bool dup = false;
            for (int j = 0; j < polyCount; ++j) {
                if (std::abs(pt.X - localPts[j].X) < 0.01f && std::abs(pt.Y - localPts[j].Y) < 0.01f) {
                    dup = true;
                    break;
                }
            }
            if (!dup) {
                localPts[polyCount++] = pt;
            }
        }
        if (polyCount < 3) return;

        Bitmap maskBmp(rw, rh, PixelFormat32bppARGB);
        {
            Graphics gMask(&maskBmp);
            gMask.SetCompositingMode(CompositingModeSourceCopy);
            gMask.Clear(Color(0, 0, 0, 0));
            gMask.SetCompositingMode(CompositingModeSourceOver);
            gMask.SetSmoothingMode(SmoothingModeAntiAlias);
            gMask.SetPixelOffsetMode(PixelOffsetModeHalf);
            SolidBrush whiteBrush(Color(255, 255, 255, 255));
            gMask.FillPolygon(&whiteBrush, localPts, polyCount, FillModeWinding);
        }

        Rect lockRc(0, 0, rw, rh);
        BitmapData bdOut = {}, bdMask = {};
        if (bmp->LockBits(&lockRc, ImageLockModeRead | ImageLockModeWrite, PixelFormat32bppARGB, &bdOut) == Ok) {
            if (maskBmp.LockBits(&lockRc, ImageLockModeRead, PixelFormat32bppARGB, &bdMask) == Ok) {
                for (int y = 0; y < rh; ++y) {
                    DWORD* outRow = (DWORD*)((BYTE*)bdOut.Scan0 + (size_t)y * bdOut.Stride);
                    const DWORD* maskRow = (const DWORD*)((const BYTE*)bdMask.Scan0 + (size_t)y * bdMask.Stride);
                    for (int x = 0; x < rw; ++x) {
                        DWORD mAlpha = (maskRow[x] >> 24) & 0xFFu;
                        if (mAlpha == 0) {
                            outRow[x] = 0x00000000u;
                        } else if (mAlpha < 255u) {
                            DWORD origA = (outRow[x] >> 24) & 0xFFu;
                            DWORD newA = (origA * mAlpha + 127u) / 255u;
                            outRow[x] = (outRow[x] & 0x00FFFFFFu) | (newA << 24);
                        }
                    }
                }
                maskBmp.UnlockBits(&bdMask);
            }
            bmp->UnlockBits(&bdOut);
        }
    }

    bool IsCtrlTempSelectActive() const {
        if (!HasCreatedCustomArea() || isEditingText || isEditingSize || isEditingRatio || isEditingStroke) return false;
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
    bool showingCustomAreaHoverTip = false;
    POINT lastCustomAreaHoverMouse{ -10000, -10000 };
    POINT customAreaHoverTipPt{ 0, 0 };

    struct SavedEditOverlaySession {
        bool active = false;
        Bitmap* frozenDesktopBmp = nullptr;
        Bitmap* overlaySingleImageBmp = nullptr;
        std::vector<Bitmap*> overlayFrames;
        std::vector<int> overlayFrameDelaysMs;
        size_t overlayActiveFrameIdx = 0;
        RECT overlayImgDrawRect{0, 0, 0, 0};
        bool overlayFrameStripCollapsed = false;
        int overlayFrameStripHeight = 142;
        float overlayFrameScrollX = 0.0f;
        float overlayFrameMaxScrollX = 0.0f;
        bool overlayFramesPlaying = false;
        bool hasSelection = false;
        RECT selRect{0, 0, 0, 0};
        bool hasCustomQuad = false;
        bool hasEllipseSelection = false;
        POINT selQuad[4] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
        std::vector<Annotation> annotations;
        std::vector<std::vector<Annotation>> undoStack;
        std::vector<std::vector<Annotation>> redoStack;
        int nextStepNum = 1;
        int nextAnnotationId = 1;
        int selectedAnnotationId = -1;
        std::vector<int> selectedAnnotationIds;
        OverlayTool activeTool = OverlayTool::SelectMove;
        bool hasCustomHudPos = false;
        POINT customHudPos{0, 0};
        bool hasCustomRatio = false;
        int customRatioW = 0;
        int customRatioH = 0;
    };
    SavedEditOverlaySession savedEditSession;
    void DiscardSavedEditSession();

    // Methods
    void InitPaths();
    static std::wstring GetDefaultDesktopFolder();
    static std::wstring GetAppDataSettingsDir();
    static std::wstring GetAppDataSettingsFile();
    void LoadSettings();
    void SaveSettings() const;
    static std::wstring GetFormatExtension(ImageFormat fmt);
    HICON CreateCustomTrayIcon();
    void InitCustomCursors();
    void DestroyCustomCursors();
    void UpdateOverlayCursor(int mx, int my);
    void InitTrayIcon();
    void UpdateTrayTooltip();
    void ShowTrayToast(const std::wstring& title, const std::wstring& message, const std::wstring& clickFolder = L"");
    void RemoveTrayIcon();
    std::wstring FormatFilename(int seqNum) const;
    static std::wstring FormatFilenameWithPattern(const std::wstring& pattern, int seqNum);
    std::wstring NextFreeCaptureStem(const std::wstring& extension);
    bool IsRunAtStartupEnabled() const;
    void ToggleRunAtStartup();
    void ApplyGlobalHotkeys();
    static std::wstring FormatHotkeyString(const HotkeyBinding& hk);
    // GetMessageW for the windows that run their own modal message loop; a
    // WM_QUIT is handed back to the thread queue instead of being swallowed.
    static BOOL GetMessageKeepQuit(MSG* msg);
    static void RegisterImageContextMenu();
    static void UnregisterImageContextMenu();
    // Registers or unregisters the verbs only when the two settings that decide
    // them, or the executable's own path, have actually changed since last time.
    void SyncImageContextMenuRegistration() const;

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
    // Takes the overlay off the screen before work that outlasts the click, so a
    // heavy dock button does not read as a frozen toolbar. Returns whether it hid
    // the window (false when closing would hand the window back to an open edit
    // session instead of taking it away).
    bool HideOverlayForPendingWork();
    void RestoreOverlayAfterPendingWork(bool wasHidden);
    void OpenImageIntoOverlay();
    void OpenImageFileIntoOverlay(const std::wstring& filePath);
    bool IsEditingOpenedImage() const { return overlaySingleImageBmp != nullptr || !overlayFrames.empty(); }
    bool IsPinnedWindow(HWND hWnd) const;
    HWND GetLowestPinnedWindow() const;
    void RaisePinnedWindowsAboveOverlay();
    bool HasOverlayMultiFrames() const { return overlayFrames.size() > 1; }
    void ClearOverlayFrames();
    void SelectOverlayFrame(size_t frameIdx);
    void ToggleOverlayFramePlayback();
    void StopOverlayFramePlayback();
    void EnsureActiveFrameVisibleInStrip();
    void LayoutOverlayFrameStrip(int screenW, int screenH);
    void RenderOverlayFrameStrip(Graphics& g, int screenW, int screenH);
    void OpenImageToPinOnTop();
    void PinClipboardImageOnTop();
    void EditClipboardImage();
    void SearchClipboardImageOnGoogle(bool translateToTarget = false);
    void SearchImageFileOnGoogle(const std::wstring& filePath);
    static void OpenGoogleLensInBrowser();
    static void OpenGoogleTranslateInBrowser(const std::wstring& targetLang);
    void OpenImageFilesToPinOnTop(const std::vector<std::wstring>& filePaths);
    HWND CreatePinnedWindow(Bitmap* bmp, int x, int y, float initialScale = 1.0f, std::vector<Bitmap*> animFrames = {}, std::vector<int> animDelaysMs = {}, bool initiallySelected = false);

    void CommitActiveTextBox();
    void CommitActiveSizeInput();
    void CommitActiveRatioInput();
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
    void ActionCopyAndClose(WebOpenTarget webTarget = WebOpenTarget::None);
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
    static void DrawEditablePillText(Graphics& g, const RECT& boxRect, const std::wstring& text, const std::wstring& suffix, bool isEditing, size_t caretPos, size_t selAnchor, const Color* customTextColor = nullptr);
    static void ApplyInlineEditKeyDown(WPARAM wParam, bool ctrl, bool shift, std::wstring& text, size_t& caret, size_t& anchor, size_t maxLen, bool isSizeField, HWND hWnd);
    static void ApplyInlineEditChar(wchar_t ch, std::wstring& text, size_t& caret, size_t& anchor, size_t maxLen);
    RECT GetEditingTextBoxRect() const;
    size_t HitTestTextBoxIndex(int mouseX, int mouseY) const;
    void ApplyTextBoxKeyDown(WPARAM wParam, bool ctrl, bool shift, HWND hWnd);
    static void ApplyPixelateFilter(Bitmap* bmp, int rx, int ry, int rw, int rh, int blockSize, bool elliptical = false);
    static void DrawVectorAnnotation(Graphics& g, const Annotation& ann, int offsetX, int offsetY, bool isSelected, bool isEditingCaret, bool forceOpaqueHighlighter = false);
    static void RenderHighlighterLayer(Graphics& destG, int regionX, int regionY, int regionW, int regionH, const std::vector<Annotation>& anns, const Annotation* draft, bool nonStacking);
    static void DrawDockButtonIcon(Graphics& g, const DockButton& b, const RectF& rf);
    static void DrawHoverBubbleTooltip(Graphics& g, const DockButton& b, int screenW);
    void GetDimensionPillAnchor(bool& outRightAlign, bool& outBottomAlign) const;
    void BuildDockedHUD();
    DragMode HitTestSelectionHandles(int mx, int my) const;
    static bool AnnotationIntersectsRect(const Annotation& a, const RECT& rc);
    int HitTestAnnotation(float x, float y, DragMode* outHandleMode) const;
    void CheckAndPromptClipboardHistory();
};

// Global daemon instance
extern PepperSnapDaemon g_Daemon;

// Shared global recording slot
extern int g_RecordingHotkeySlot;

// Shared input hook worker thread & hotkey debounce tick
extern DWORD g_InputHookThreadId;
extern DWORD g_LastHotkeyTriggerTick;
DWORD WINAPI InputHookWorkerThread(LPVOID param);

// Shared utility functions
void ReplaceStrW(std::wstring& str, const std::wstring& from, const std::wstring& to);
std::wstring Utf8ToWideStr(const std::string& s);
std::wstring GetModuleFilePathDynamic(HMODULE hMod = nullptr);
std::wstring QueryProcessFullImagePathDynamic(HANDLE hProc);
std::wstring GetWindowTextDynamic(HWND hWnd);
std::wstring GetPrivateProfileStringDynamic(
    const wchar_t* section,
    const wchar_t* key,
    const wchar_t* defVal,
    const std::wstring& iniPath
);
bool RegQueryStringValueDynamic(HKEY hKey, const wchar_t* subKey, const wchar_t* valueName, std::wstring& outVal);
void RegEnumSubKeyNamesDynamic(HKEY hKey, std::vector<std::wstring>& outNames);
bool IsValidGithubRepoSlug(const std::wstring& s);
std::wstring FormatLabelWithShortcut(const std::wstring& label, const HotkeyBinding& hk, const std::wstring& suffix = L"");
bool IsModifierVk(DWORD vk);
UINT GetCurrentHardwareModifiers();
bool MatchesOverlayShortcut(const HotkeyBinding& hk, UINT vk, bool ctrl, bool shift, bool alt);
bool MatchesOverlayShortcutWithOptionalShift(const HotkeyBinding& hk, UINT vk, bool ctrl, bool shift, bool alt);
bool MatchesAnyPinnedShortcut(UINT vk, bool ctrl, bool shift, bool alt);
bool LoadImageFramesFromFile(const std::wstring& filePath, std::vector<Bitmap*>& outFrames, std::vector<int>& outDelaysMs);
bool IsValidExistingImageFilePath(const std::wstring& rawPath, std::wstring* outResolvedPath = nullptr);
bool IsWindowsClipboardHistoryEnabled();
bool EnableWindowsClipboardHistory();
std::wstring BuildUniqueSaveFilePath(const std::wstring& folder, const std::wstring& stem, const std::wstring& ext, int batchIdx = 0, int batchTotal = 1);
void GetPinnedScaledDims(const PinnedWindowData* data, int& outW, int& outH);
void EnsurePinnedWindowCache(PinnedWindowData* data, int imgW, int imgH, bool highQuality);
void UpdatePinnedWindowLayout(HWND hWnd, PinnedWindowData* data, bool resizeWindow = true);
void RenderPinnedLayeredWindow(HWND hWnd, PinnedWindowData* data);
void ShowPinnedBubbleTooltip(HWND hPinWnd, const DockButton& b);
void HidePinnedBubbleTooltip();
void ClearAllSelectedPinnedWindows(HWND hExcept = nullptr);
void SetPinnedWindowSelected(HWND hPin, PinnedWindowData* d, bool selected);
RECT GetPinnedMonitorWorkArea(HMONITOR hMon);
extern HWND g_hPinTooltipWnd;
LRESULT CALLBACK PinWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK TrayDaemonWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// RAII helper types
struct ScopedHKey {
    HKEY h = nullptr;
    ScopedHKey() = default;
    explicit ScopedHKey(HKEY k) : h(k) {}
    ~ScopedHKey() { if (h) RegCloseKey(h); }
    ScopedHKey(const ScopedHKey&) = delete;
    ScopedHKey& operator=(const ScopedHKey&) = delete;
    HKEY* put() {
        if (h) { RegCloseKey(h); h = nullptr; }
        return &h;
    }
    operator HKEY() const { return h; }
};

struct ScopedHandle {
    HANDLE h = nullptr;
    ScopedHandle() = default;
    explicit ScopedHandle(HANDLE handle) : h(handle) {}
    ~ScopedHandle() {
        if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    operator HANDLE() const { return h; }
};
