#include "PepperSnapInternal.h"

// ----------------------------------------------------------------------------
// Pinned Desktop Window & Modals (Naming Pattern, Folder, Shortcuts)
// ----------------------------------------------------------------------------

static void GetPinnedScaleBounds(const PinnedWindowData* data, float& outMinScale, float& outMaxScale) {
    int ow = (data && data->origW > 0) ? data->origW : 32;
    int oh = (data && data->origH > 0) ? data->origH : 32;
    outMinScale = std::max(0.01f, std::max(8.0f / (float)ow, 8.0f / (float)oh));
    outMaxScale = std::max(outMinScale, std::min(64.0f, std::min(16384.0f / (float)ow, 16384.0f / (float)oh)));
}

void GetPinnedScaledDims(const PinnedWindowData* data, int& outW, int& outH) {
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
    data->cachedViewX = 0;
    data->cachedViewY = 0;
    data->cachedViewW = 0;
    data->cachedViewH = 0;
    data->cachedFrameIdx = (size_t)-1;
    data->cachedHq = false;
}

static void GetPinnedVisibleSlice(const PinnedWindowData* data, int imgW, int imgH, int& outViewX, int& outViewY, int& outViewW, int& outViewH) {
    if (!data || imgW <= 0 || imgH <= 0) {
        outViewX = 0;
        outViewY = 0;
        outViewW = 1;
        outViewH = 1;
        return;
    }
    int vL = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vT = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vH = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (vW <= 0 || vH <= 0) {
        vL = 0;
        vT = 0;
        vW = GetSystemMetrics(SM_CXSCREEN);
        vH = GetSystemMetrics(SM_CYSCREEN);
    }
    int vR = vL + vW;
    int vB = vT + vH;

    int sImgL = data->screenImgX;
    int sImgT = data->screenImgY;
    int sImgR = sImgL + imgW;
    int sImgB = sImgT + imgH;

    int visL = std::max(sImgL, vL);
    int visT = std::max(sImgT, vT);
    int visR = std::min(sImgR, vR);
    int visB = std::min(sImgB, vB);

    if (visR <= visL) {
        if (sImgR <= vL) {
            visL = sImgR - 1;
            visR = sImgR;
        } else {
            visL = sImgL;
            visR = sImgL + 1;
        }
    }
    if (visB <= visT) {
        if (sImgB <= vT) {
            visT = sImgB - 1;
            visB = sImgB;
        } else {
            visT = sImgT;
            visB = sImgT + 1;
        }
    }

    outViewX = visL - sImgL;
    outViewY = visT - sImgT;
    outViewW = std::max(1, visR - visL);
    outViewH = std::max(1, visB - visT);
}

RECT GetPinnedMonitorWorkArea(HMONITOR hMon) {
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

HWND g_hPinTooltipWnd = nullptr;

void HidePinnedBubbleTooltip() {
    if (g_hPinTooltipWnd && IsWindow(g_hPinTooltipWnd)) {
        ShowWindow(g_hPinTooltipWnd, SW_HIDE);
    }
}

void ShowPinnedBubbleTooltip(HWND hPinWnd, const DockButton& b) {
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

    std::vector<std::wstring> lines;
    size_t start = 0;
    while (start <= b.tooltip.size()) {
        size_t nl = b.tooltip.find(L'\n', start);
        if (nl == std::wstring::npos) {
            lines.push_back(b.tooltip.substr(start));
            break;
        }
        lines.push_back(b.tooltip.substr(start, nl - start));
        start = nl + 1;
    }

    float maxLineW = 0.0f;
    {
        Graphics gMeasure(hScreen);
        gMeasure.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        for (const auto& ln : lines) {
            RectF measured;
            gMeasure.MeasureString(ln.c_str(), -1, &tipFont, PointF(0, 0), &measured);
            if (measured.Width > maxLineW) maxLineW = measured.Width;
        }
    }

    int bw = (int)std::ceil(maxLineW) + 18;
    int bh = 8 + (int)lines.size() * 18;
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
            for (size_t i = 0; i < lines.size(); ++i) {
                RectF lineRc(rx, ry + 4.0f + (float)i * 18.0f, rw, 18.0f);
                g.DrawString(lines[i].c_str(), -1, &tipFont, lineRc, &sf, &textBr);
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
        L"Middle-click to reset size",
        L"Ctrl + middle-click",
        L"to reset position"
    };
    if (data->isSelected) {
        lines.push_back(L"Ctrl + click to deselect");
    } else {
        lines.push_back(L"Ctrl + click to");
        lines.push_back(L"multi-select (drag together)");
    }
    if (isAnim) {
        lines.push_back(data->animPaused ? L"Alt + click to play" : L"Alt + click to pause");
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

void EnsurePinnedWindowCache(PinnedWindowData* data, int imgW, int imgH, bool highQuality) {
    if (!data || !data->bmp || imgW <= 0 || imgH <= 0) return;
    int viewX = 0, viewY = 0, viewW = imgW, viewH = imgH;
    GetPinnedVisibleSlice(data, imgW, imgH, viewX, viewY, viewW, viewH);

    bool hideOutline = data->hideOutline;
    bool selected = data->isSelected;
    bool smooth = data->smoothImage;
    if (data->hCacheDC && data->hCacheBmp && data->cachePixels &&
        data->cachedImgW == imgW && data->cachedImgH == imgH &&
        data->cachedViewX == viewX && data->cachedViewY == viewY &&
        data->cachedViewW == viewW && data->cachedViewH == viewH &&
        data->cachedFrameIdx == data->curFrameIdx &&
        data->cachedHideOutline == hideOutline &&
        data->cachedSelected == selected &&
        data->cachedSmooth == smooth &&
        (!smooth || data->cachedHq || !highQuality)) {
        return;
    }

    if (!data->hCacheDC || !data->hCacheBmp || !data->cachePixels || data->cachedViewW != viewW || data->cachedViewH != viewH) {
        FreePinnedWindowCache(data);
        HDC hScreen = GetDC(nullptr);
        data->hCacheDC = CreateCompatibleDC(hScreen);
        BITMAPINFO bmi = {0};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = viewW;
        bmi.bmiHeader.biHeight = -viewH;
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

    memset(data->cachePixels, 0, (size_t)viewW * (size_t)viewH * sizeof(DWORD));
    {
        Bitmap cacheSurface(viewW, viewH, viewW * 4, PixelFormat32bppPARGB, (BYTE*)data->cachePixels);
        Graphics g(&cacheSurface);
        g.SetCompositingMode(CompositingModeSourceCopy);
        g.SetPixelOffsetMode(PixelOffsetModeHalf);

        long long visiblePixels = (long long)viewW * (long long)viewH;
        if (!smooth) {
            // Button OFF (default): image is unfiltered so zoomed pixels stay crisp and pixelated
            g.SetInterpolationMode(InterpolationModeNearestNeighbor);
        } else if (!highQuality) {
            // Button ON: smooth filter active
            g.SetInterpolationMode(InterpolationModeBilinear);
        } else {
            g.SetInterpolationMode(visiblePixels > (2560LL * 1440LL) ? InterpolationModeBilinear : InterpolationModeHighQualityBicubic);
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

        // Only scale and render the visible sub-region of the source bitmap that intersects [0..viewW, 0..viewH]
        double dstFullX = (double)(drawX - viewX);
        double dstFullY = (double)(drawY - viewY);
        double dstFullW = (double)drawW;
        double dstFullH = (double)drawH;

        double clipX0 = std::max(0.0, dstFullX);
        double clipY0 = std::max(0.0, dstFullY);
        double clipX1 = std::min((double)viewW, dstFullX + dstFullW);
        double clipY1 = std::min((double)viewH, dstFullY + dstFullH);

        if (clipX1 > clipX0 && clipY1 > clipY0) {
            double srcX0 = (clipX0 - dstFullX) * (double)srcW / dstFullW;
            double srcY0 = (clipY0 - dstFullY) * (double)srcH / dstFullH;
            double srcX1 = (clipX1 - dstFullX) * (double)srcW / dstFullW;
            double srcY1 = (clipY1 - dstFullY) * (double)srcH / dstFullH;

            int sLeft   = std::max(0, (int)std::floor(srcX0) - 2);
            int sTop    = std::max(0, (int)std::floor(srcY0) - 2);
            int sRight  = std::min(srcW, (int)std::ceil(srcX1) + 2);
            int sBottom = std::min(srcH, (int)std::ceil(srcY1) + 2);
            int subSrcW = sRight - sLeft;
            int subSrcH = sBottom - sTop;

            if (subSrcW > 0 && subSrcH > 0) {
                REAL subDstX = (REAL)(dstFullX + (double)sLeft * dstFullW / (double)srcW);
                REAL subDstY = (REAL)(dstFullY + (double)sTop  * dstFullH / (double)srcH);
                REAL subDstW = (REAL)((double)subSrcW * dstFullW / (double)srcW);
                REAL subDstH = (REAL)((double)subSrcH * dstFullH / (double)srcH);
                g.DrawImage(
                    data->bmp,
                    RectF(subDstX, subDstY, subDstW, subDstH),
                    (REAL)sLeft, (REAL)sTop, (REAL)subSrcW, (REAL)subSrcH,
                    UnitPixel,
                    &ia
                );
            }
        }

        if (selected || !hideOutline) {
            g.SetCompositingMode(CompositingModeSourceOver);
            Color outlineCol = selected ? Color(255, 59, 130, 246) : Color(255, 239, 68, 68);
            if (data->hasEllipseMask) {
                g.SetSmoothingMode(SmoothingModeAntiAlias);
                g.SetPixelOffsetMode(PixelOffsetModeHalf);
                Pen border2px(outlineCol, 2.0f);
                g.DrawEllipse(&border2px, 1.0f - (float)viewX, 1.0f - (float)viewY,
                              (float)std::max(1, imgW - 2), (float)std::max(1, imgH - 2));
            } else if (data->hasCustomQuad) {
                g.SetSmoothingMode(SmoothingModeAntiAlias);
                g.SetPixelOffsetMode(PixelOffsetModeHalf);
                PointF polyPts[4];
                int polyCount = 0;
                for (int i = 0; i < 4; ++i) {
                    PointF pt(
                        data->normQuad[i].X * (float)std::max(1, imgW - 1) - (float)viewX,
                        data->normQuad[i].Y * (float)std::max(1, imgH - 1) - (float)viewY
                    );
                    bool dup = false;
                    for (int j = 0; j < polyCount; ++j) {
                        if (std::abs(pt.X - polyPts[j].X) < 0.1f && std::abs(pt.Y - polyPts[j].Y) < 0.1f) {
                            dup = true;
                            break;
                        }
                    }
                    if (!dup) {
                        polyPts[polyCount++] = pt;
                    }
                }
                if (polyCount >= 3) {
                    Pen border2px(outlineCol, 2.0f);
                    border2px.SetLineJoin(LineJoinRound);
                    g.DrawPolygon(&border2px, polyPts, polyCount);
                }
            } else {
                g.SetSmoothingMode(SmoothingModeNone);
                g.SetPixelOffsetMode(PixelOffsetModeNone);
                Pen border1px(outlineCol, 1.0f);
                g.DrawRectangle(&border1px, -viewX, -viewY, imgW - 1, imgH - 1);
                if (imgW >= 4 && imgH >= 4) {
                    g.DrawRectangle(&border1px, 1 - viewX, 1 - viewY, imgW - 3, imgH - 3);
                }
            }
        }
    }

    // Ensure transparent pixels inside the visible image slice have alpha >= 1 (0.39% opacity, visually 100% transparent
    // over desktop while remaining mouse-interactive for dragging, zooming, and double-clicking)
    if (!data->hasCustomQuad && !data->hasEllipseMask) {
        size_t totalPx = (size_t)viewW * (size_t)viewH;
        for (size_t i = 0; i < totalPx; ++i) {
            if ((data->cachePixels[i] >> 24) == 0) {
                data->cachePixels[i] = 0x01000000u;
            }
        }
    }

    data->cachedImgW = imgW;
    data->cachedImgH = imgH;
    data->cachedViewX = viewX;
    data->cachedViewY = viewY;
    data->cachedViewW = viewW;
    data->cachedViewH = viewH;
    data->cachedFrameIdx = data->curFrameIdx;
    data->cachedHideOutline = hideOutline;
    data->cachedSelected = selected;
    data->cachedSmooth = smooth;
    data->cachedHq = !smooth ? true : highQuality;
}

void RenderPinnedLayeredWindow(HWND hWnd, PinnedWindowData* data) {
    if (!hWnd || !IsWindow(hWnd) || !data || !data->bmp) return;
    int imgW = 32, imgH = 32;
    GetPinnedScaledDims(data, imgW, imgH);
    EnsurePinnedWindowCache(data, imgW, imgH, data->cachedHq || ((long long)imgW * (long long)imgH <= 960LL * 540LL));
    if (!data->cachePixels) return;
    int viewW = std::max(1, data->cachedViewW);
    int viewH = std::max(1, data->cachedViewH);

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

        if (!data->hideImage) {
            int imgLeft = data->imgRect.left;
            int imgTop  = data->imgRect.top;
            for (int y = 0; y < viewH; ++y) {
                int dy = imgTop + y;
                if (dy < 0 || dy >= winH) continue;
                int copyX = std::max(0, imgLeft);
                int srcX  = copyX - imgLeft;
                int copyW = std::min(viewW - srcX, winW - copyX);
                if (copyW > 0) {
                    memcpy(dstPixels + (size_t)dy * winW + copyX,
                           data->cachePixels + (size_t)y * viewW + srcX,
                           (size_t)copyW * sizeof(DWORD));
                }
            }
        }

        bool suppressToolbar = !data->hideImage && (data->hideToolbar || (data->dragging && data->dragMoved));
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
                int rbx = data->pctResetBtnRect.left;
                int rbw = data->pctResetBtnRect.right - data->pctResetBtnRect.left;
                bool resetHovered = data->hoveredPctResetBtn;
                bool resetPressed = data->pressedPctResetBtn && resetHovered;

                Color sizeBgCol = data->isEditingSize
                    ? Color(252, 30, 41, 59)
                    : (data->hoveredSizeBox ? Color(250, 30, 41, 59) : Color(245, 15, 23, 42));
                Color pctBgCol = data->isEditingPct
                    ? Color(252, 30, 41, 59)
                    : (data->hoveredPctBox ? Color(250, 30, 41, 59) : Color(245, 15, 23, 42));
                Color resetBgCol = resetPressed
                    ? Color(255, 71, 85, 105)
                    : (resetHovered ? Color(255, 51, 65, 85) : Color(245, 15, 23, 42));

                SolidBrush sizeBg(sizeBgCol);
                SolidBrush pctBg(pctBgCol);
                SolidBrush resetBg(resetBgCol);
                Pen outerBorder(Color(235, 51, 65, 85), 1.0f);
                Pen divPen(Color(235, 71, 85, 105), 1.0f);

                GraphicsState stBoxes = g.Save();
                g.SetSmoothingMode(SmoothingModeNone);
                g.SetPixelOffsetMode(PixelOffsetModeNone);
                g.FillRectangle(&sizeBg, sbx, ry, sbw, rh);
                g.FillRectangle(&pctBg, pbx, ry, pbw, rh);
                if (rbw > 0) {
                    g.FillRectangle(&resetBg, rbx, ry, rbw, rh);
                }
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
                if (rbw > 0 && resetHovered) {
                    Pen hiPen(Color(255, 239, 68, 68), 1.0f);
                    g.DrawRectangle(&hiPen, rbx, ry, rbw - 1, rh - 1);
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

                if (rbw > 0) {
                    DockButton resetIconBtn;
                    resetIconBtn.id = DBTN_ACT_SCALE_RESET_100;
                    resetIconBtn.isTool = false;
                    RectF resetRf((float)rbx, (float)ry, (float)rbw, (float)rh);
                    PepperSnapDaemon::DrawDockButtonIcon(g, resetIconBtn, resetRf);
                }
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

// Where a pinned image's toolbar strip goes: right-aligned to the image's right
// edge where there is room, below the image by default, above it when the bottom
// of the screen is too close, and nudged inside the image's bottom-right corner
// when neither side has room. Returns the strip's top-left corner in screen
// coordinates.
static POINT GetPinnedStripOrigin(const PinnedWindowData* data, int imgW, int imgH,
                                  int stripW, int stripH) {
    const int hudGap = 8; // Same 8px gap between toolbar and custom area

    int sImgL = data->screenImgX;
    int sImgT = data->screenImgY;
    int sImgR = sImgL + imgW;
    int sImgB = sImgT + imgH;

    RECT rcImgScreen = { sImgL, sImgT, sImgR, sImgB };
    HMONITOR hMon = MonitorFromRect(&rcImgScreen, MONITOR_DEFAULTTONEAREST);
    RECT rcScreen = GetPinnedMonitorWorkArea(hMon);

    int sStripR = std::max((int)rcScreen.left + stripW + 6, std::min((int)rcScreen.right - 6, sImgR));
    int sStripT = sImgB + hudGap;

    bool hasSpaceBelow = (sImgB + hudGap + stripH <= rcScreen.bottom - 6) && (sImgB + hudGap >= rcScreen.top + 6);
    bool hasSpaceAbove = (sImgT - hudGap - stripH >= rcScreen.top + 6) && (sImgT - hudGap <= rcScreen.bottom - 6);

    if (hasSpaceBelow) {
        // Default: snap below bottom-right corner of pinned image
        sStripT = sImgB + hudGap;
    } else if (hasSpaceAbove) {
        // No space below -> snap above top-right corner of pinned image
        sStripT = sImgT - hudGap - stripH;
    } else {
        // No space below or above -> place inside bottom-right of pinned image (clamped to screen bounds)
        sStripR = std::max((int)rcScreen.left + stripW + 6, std::min((int)rcScreen.right - 6, sImgR - 8));
        sStripT = std::max((int)rcScreen.top + 6, std::min((int)rcScreen.bottom - stripH - 6, sImgB - stripH - 8));
    }

    POINT pt = { sStripR - stripW, sStripT };
    return pt;
}

void UpdatePinnedWindowLayout(HWND hWnd, PinnedWindowData* data, bool resizeWindow) {
    if (!data || !data->bmp) return;
    int imgW = 32, imgH = 32;
    GetPinnedScaledDims(data, imgW, imgH);

    const int toolBtnW = 32;
    const int actBtnH  = 28;
    const int gap      = 3;

    struct PinBtnSpec { int id; int w; const wchar_t* lbl; std::wstring tip; bool primary; };
    const wchar_t* outlineLbl = data->hideOutline ? L"Show outline" : L"Hide outline";
    std::wstring pinCopyTip = FormatLabelWithShortcut(L"Copy to clipboard", g_Daemon.hkPinCopy);
    PinBtnSpec specs[7] = {
        { DBTN_ACT_PIN_HIDE_IMAGE, toolBtnW, L"", L"Hide pinned image", false },
        { DBTN_ACT_PIN_OUTLINE,    toolBtnW, L"", FormatLabelWithShortcut(outlineLbl,                  g_Daemon.hkPinOutline),  false },
        { DBTN_ACT_PIN_UNFILTER,   toolBtnW, L"", FormatLabelWithShortcut(L"Smooth the image",         g_Daemon.hkPinSmooth),   false },
        { DBTN_ACT_SAVE_AS,        toolBtnW, L"", FormatLabelWithShortcut(L"Save as JPG/PNG/WEBP/BMP", g_Daemon.hkPinSaveAs),   false },
        { DBTN_ACT_SAVE,           toolBtnW, L"", FormatLabelWithShortcut(L"Quick save",               g_Daemon.hkPinSave),     false },
        { DBTN_ACT_COPY,           toolBtnW, L"", pinCopyTip,                                                                   true  },
        { DBTN_ACT_CLOSE,          toolBtnW, L"", FormatLabelWithShortcut(L"Close pinned image",       g_Daemon.hkPinClose),    false }
    };

    int stripW = 0;
    for (int i = 0; i < 7; ++i) {
        stripW += specs[i].w + (i > 0 ? gap : 0);
    }
    int totalHudH = actBtnH * 2 + gap;

    int sImgL = data->screenImgX;
    int sImgT = data->screenImgY;

    // The strip's origin has to be known before the hidden-image branch below,
    // because that state parks its mini strip on the toolbar's own first button.
    // Both states ask for the same placement.
    POINT stripPt = GetPinnedStripOrigin(data, imgW, imgH, stripW, totalHudH);
    int sStripL = stripPt.x;
    int sStripT = stripPt.y;
    int sStripR = sStripL + stripW;
    int sStripB = sStripT + totalHudH;

    if (data->hideImage) {
        const int miniW = toolBtnW * 2 + gap;
        const int miniH = actBtnH;
        int winL = sStripL;
        int winT = sStripT;
        int winW = miniW;
        int winH = miniH;
        data->winX = winL;
        data->winY = winT;
        data->winW = winW;
        data->winH = winH;
        data->imgRect = { 0, 0, 0, 0 };
        data->buttons.clear();

        // The eye is the mini strip's first button, which is exactly where it sits
        // in the toolbar's first row, so hiding the image leaves it under the
        // pointer that just pressed it - and pressing it again does the same thing
        // in reverse. The drag handle takes the slot the outline button had. The
        // strip therefore stays inside the toolbar's own footprint; it used to jump
        // to the pinned image's top-left corner instead.
        DockButton bShow;
        bShow.id = DBTN_ACT_PIN_HIDE_IMAGE;
        bShow.rect = { 0, 0, toolBtnW, actBtnH };
        bShow.label = L"";
        bShow.tooltip = L"Show pinned image";
        bShow.isPrimaryAction = false;
        data->buttons.push_back(bShow);

        DockButton bDrag;
        bDrag.id = DBTN_ACT_DRAG_HUD;
        bDrag.rect = { toolBtnW + gap, 0, miniW, actBtnH };
        bDrag.label = L"";
        bDrag.tooltip = L"Drag hidden pinned image";
        bDrag.isPrimaryAction = false;
        data->buttons.push_back(bDrag);

        data->sizeRowRect     = { 0, 0, 0, 0 };
        data->sizeBoxRect     = { 0, 0, 0, 0 };
        data->pctBoxRect      = { 0, 0, 0, 0 };
        data->pctResetBtnRect = { 0, 0, 0, 0 };

        if (hWnd) {
            SetWindowPos(hWnd, nullptr, winL, winT, winW, winH, SWP_NOACTIVATE | SWP_NOZORDER);
            RenderPinnedLayeredWindow(hWnd, data);
        }
        return;
    }

    int viewX = 0, viewY = 0, viewW = imgW, viewH = imgH;
    GetPinnedVisibleSlice(data, imgW, imgH, viewX, viewY, viewW, viewH);
    int visImgL = sImgL + viewX;
    int visImgT = sImgT + viewY;
    int visImgR = visImgL + viewW;
    int visImgB = visImgT + viewH;

    bool suppressToolbar = data->hideToolbar || (data->dragging && data->dragMoved);
    int winL = suppressToolbar ? visImgL : std::min(visImgL, sStripL);
    int winT = suppressToolbar ? visImgT : std::min(visImgT, sStripT);
    int winR = suppressToolbar ? visImgR : std::max(visImgR, sStripR);
    int winB = suppressToolbar ? visImgB : std::max(visImgB, sStripB);
    int winW = std::max(1, winR - winL);
    int winH = std::max(1, winB - winT);

    data->winX = winL;
    data->winY = winT;
    data->winW = winW;
    data->winH = winH;
    data->imgRect = { visImgL - winL, visImgT - winT, visImgR - winL, visImgB - winT };
    RECT curSliceRect = { viewX, viewY, viewX + viewW, viewY + viewH };

    RECT curStripRect = { 0, 0, 0, 0 };
    data->buttons.clear();
    if (!suppressToolbar) {
        int bx = sStripL - winL;
        int by = sStripT - winT;
        curStripRect = { bx, by, bx + stripW, by + totalHudH };
        for (int i = 0; i < 7; ++i) {
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
        // "Search on web" heads the image-size row; the W x H box gives up one
        // button plus that button's gap so the row keeps the strip's width and
        // the percentage box and 100% reset stay exactly where they were.
        DockButton bSearch;
        bSearch.id = DBTN_ACT_PIN_SEARCH_GOOGLE;
        bSearch.rect = { rx, ry, rx + toolBtnW, ry + actBtnH };
        bSearch.label = L"";
        bSearch.tooltip = FormatLabelWithShortcut(L"Search on web", g_Daemon.hkPinSearchGoogle);
        bSearch.isPrimaryAction = false;
        data->buttons.push_back(bSearch);

        int rowX = rx + toolBtnW + gap;
        int sizeBoxW = 158 - toolBtnW - gap;
        int resetBtnW = 26;
        data->sizeRowRect     = { rowX, ry, rx + stripW, ry + actBtnH };
        data->sizeBoxRect     = { rowX, ry, rowX + sizeBoxW, ry + actBtnH };
        data->pctBoxRect      = { rowX + sizeBoxW, ry, rx + stripW - resetBtnW, ry + actBtnH };
        data->pctResetBtnRect = { rx + stripW - resetBtnW, ry, rx + stripW, ry + actBtnH };
    } else {
        data->sizeRowRect     = { 0, 0, 0, 0 };
        data->sizeBoxRect     = { 0, 0, 0, 0 };
        data->pctBoxRect      = { 0, 0, 0, 0 };
        data->pctResetBtnRect = { 0, 0, 0, 0 };
        data->hoveredBtnId = -1;
        data->pressedBtnId = -1;
        data->hoveredSizeBox = false;
        data->hoveredPctBox = false;
        data->hoveredPctResetBtn = false;
        data->pressedPctResetBtn = false;
        data->pressedPctResetShift = false;
        HidePinnedBubbleTooltip();
    }

    if (hWnd) {
        bool rgnChanged = !data->hasAppliedRgn ||
                          data->lastRgnHideToolbar != suppressToolbar ||
                          !EqualRect(&data->lastRgnImgRect, &data->imgRect) ||
                          !EqualRect(&data->lastRgnSliceRect, &curSliceRect) ||
                          !EqualRect(&data->lastRgnStripRect, &curStripRect);
        if (rgnChanged) {
            data->lastRgnImgRect = data->imgRect;
            data->lastRgnSliceRect = curSliceRect;
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

static void UpdatePinnedSnapModeFromPosition(PinnedWindowData* d, HMONITOR hExplicitMon = nullptr, bool disallowSnap = false) {
    if (!d) return;
    d->lastZoomCursorPt = { -999999, -999999 };
    if (disallowSnap) {
        d->zoomSnapMode = PinnedSnapMode::None;
        return;
    }
    int imgW = 32, imgH = 32;
    GetPinnedScaledDims(d, imgW, imgH);
    int left   = d->screenImgX;
    int top    = d->screenImgY;
    int right  = left + imgW;
    int bottom = top + imgH;

    RECT rcPin = { left, top, right, bottom };
    HMONITOR hMon = hExplicitMon ? hExplicitMon : MonitorFromRect(&rcPin, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    if (!hMon || !GetMonitorInfoW(hMon, &mi)) {
        d->zoomSnapMode = PinnedSnapMode::None;
        return;
    }

    int distLeft   = std::abs(left   - (int)mi.rcMonitor.left);
    int distRight  = std::abs(right  - (int)mi.rcMonitor.right);
    int distTop    = std::abs(top    - (int)mi.rcMonitor.top);
    int distBottom = std::abs(bottom - (int)mi.rcMonitor.bottom);

    bool snappedLeft   = (left   == mi.rcMonitor.left)   && (distLeft <= distRight);
    bool snappedRight  = (right  == mi.rcMonitor.right)  && !snappedLeft;
    bool snappedTop    = (top    == mi.rcMonitor.top)    && (distTop <= distBottom);
    bool snappedBottom = (bottom == mi.rcMonitor.bottom) && !snappedTop;

    if (snappedLeft && snappedBottom) {
        d->zoomSnapMode = PinnedSnapMode::CornerBottomLeft;
    } else if (snappedLeft && snappedTop) {
        d->zoomSnapMode = PinnedSnapMode::CornerTopLeft;
    } else if (snappedRight && snappedTop) {
        d->zoomSnapMode = PinnedSnapMode::CornerTopRight;
    } else if (snappedRight && snappedBottom) {
        d->zoomSnapMode = PinnedSnapMode::CornerBottomRight;
    } else if (snappedLeft) {
        d->zoomSnapMode = PinnedSnapMode::SideLeft;
        d->zoomSideCenterY = (double)top + (double)imgH * 0.5;
    } else if (snappedRight) {
        d->zoomSnapMode = PinnedSnapMode::SideRight;
        d->zoomSideCenterY = (double)top + (double)imgH * 0.5;
    } else if (snappedTop) {
        d->zoomSnapMode = PinnedSnapMode::SideTop;
        d->zoomSideCenterX = (double)left + (double)imgW * 0.5;
    } else if (snappedBottom) {
        d->zoomSnapMode = PinnedSnapMode::SideBottom;
        d->zoomSideCenterX = (double)left + (double)imgW * 0.5;
    } else {
        d->zoomSnapMode = PinnedSnapMode::None;
    }
}

static void SetPinnedWindowExactScale(HWND hWnd, PinnedWindowData* data, float targetScale, bool useCursorAnchorWhenUnsnapped = false, const POINT* pCursorScreenPt = nullptr) {
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

    // Determine active snap mode:
    // - CornerBottomLeft:  expand upward and rightward
    // - CornerTopLeft:     expand downward and rightward
    // - CornerTopRight:    expand downward and leftward
    // - CornerBottomRight: expand upward and leftward (1a)
    // - SideLeft:          expand upward, rightward, downward (1b)
    // - SideRight:         expand upward, leftward, downward (1c)
    // - SideTop:           expand leftward, downward, rightward (1c)
    // - SideBottom:        expand leftward, upward, rightward (1c)
    // - None:              when not snapped to any edge/corner, use the cursor as the anchor point (1)
    PinnedSnapMode mode = data->zoomSnapMode;

    if (mode != PinnedSnapMode::None) {
        data->lastZoomCursorPt = { -999999, -999999 };
        switch (mode) {
            case PinnedSnapMode::CornerBottomLeft:
                data->screenImgX = oldImgLeft;
                data->screenImgY = oldImgBottom - imgH;
                break;
            case PinnedSnapMode::CornerTopLeft:
                data->screenImgX = oldImgLeft;
                data->screenImgY = oldImgTop;
                break;
            case PinnedSnapMode::CornerTopRight:
                data->screenImgX = oldImgRight - imgW;
                data->screenImgY = oldImgTop;
                break;
            case PinnedSnapMode::CornerBottomRight:
                data->screenImgX = oldImgRight - imgW;
                data->screenImgY = oldImgBottom - imgH;
                break;
            case PinnedSnapMode::SideLeft:
                data->screenImgX = oldImgLeft;
                data->screenImgY = (int)std::round(data->zoomSideCenterY - (double)imgH * 0.5);
                break;
            case PinnedSnapMode::SideRight:
                data->screenImgX = oldImgRight - imgW;
                data->screenImgY = (int)std::round(data->zoomSideCenterY - (double)imgH * 0.5);
                break;
            case PinnedSnapMode::SideTop:
                data->screenImgX = (int)std::round(data->zoomSideCenterX - (double)imgW * 0.5);
                data->screenImgY = oldImgTop;
                break;
            case PinnedSnapMode::SideBottom:
                data->screenImgX = (int)std::round(data->zoomSideCenterX - (double)imgW * 0.5);
                data->screenImgY = oldImgBottom - imgH;
                break;
            default:
                break;
        }
    } else {
        POINT curPt = { 0, 0 };
        bool hasCur = false;
        if (pCursorScreenPt) {
            curPt = *pCursorScreenPt;
            hasCur = true;
        } else if (GetCursorPos(&curPt)) {
            hasCur = true;
        }

        bool cursorInsideImg = hasCur &&
            curPt.x >= oldImgLeft && curPt.x <= oldImgRight &&
            curPt.y >= oldImgTop  && curPt.y <= oldImgBottom;

        if (hasCur && (useCursorAnchorWhenUnsnapped || cursorInsideImg)) {
            if (curPt.x != data->lastZoomCursorPt.x || curPt.y != data->lastZoomCursorPt.y) {
                double rx = (double)(curPt.x - oldImgLeft) / (double)std::max(1, oldImgW);
                double ry = (double)(curPt.y - oldImgTop)  / (double)std::max(1, oldImgH);
                data->zoomCursorRelX = std::max(0.0, std::min(1.0, rx));
                data->zoomCursorRelY = std::max(0.0, std::min(1.0, ry));
                data->lastZoomCursorPt = curPt;
            }
            double anchorScreenX = (cursorInsideImg)
                ? (double)curPt.x
                : ((double)oldImgLeft + data->zoomCursorRelX * (double)oldImgW);
            double anchorScreenY = (cursorInsideImg)
                ? (double)curPt.y
                : ((double)oldImgTop  + data->zoomCursorRelY * (double)oldImgH);
            data->screenImgX = (int)std::round(anchorScreenX - data->zoomCursorRelX * (double)imgW);
            data->screenImgY = (int)std::round(anchorScreenY - data->zoomCursorRelY * (double)imgH);
        } else {
            data->lastZoomCursorPt = { -999999, -999999 };
            double centerX = (double)oldImgLeft + (double)oldImgW * 0.5;
            double centerY = (double)oldImgTop  + (double)oldImgH * 0.5;
            data->screenImgX = (int)std::round(centerX - (double)imgW * 0.5);
            data->screenImgY = (int)std::round(centerY - (double)imgH * 0.5);
        }
    }

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
            data->hoveredPctResetBtn = (newHover == -1 && !data->hoveredSizeBox && !data->hoveredPctBox && PtInRect(&data->pctResetBtnRect, cur) != FALSE);
            SetCursor(LoadCursorW(nullptr, (data->hoveredSizeBox || data->hoveredPctBox) ? IDC_IBEAM : ((newHover != -1 || data->hoveredPctResetBtn) ? IDC_HAND : IDC_SIZEALL)));
            if (newHover != -1) {
                for (const auto& b : data->buttons) {
                    if (b.id == newHover) {
                        ShowPinnedBubbleTooltip(hWnd, b);
                        break;
                    }
                }
            } else if (data->hoveredPctResetBtn) {
                DockButton fakeTip;
                fakeTip.rect = data->pctResetBtnRect;
                fakeTip.tooltip = L"Set to 100%";
                ShowPinnedBubbleTooltip(hWnd, fakeTip);
            } else {
                HidePinnedBubbleTooltip();
            }
        }
        RenderPinnedLayeredWindow(hWnd, data);
    }
}

static void ApplyPinnedWindowZoom(HWND hWnd, PinnedWindowData* data, float factor, bool resetToOriginal = false, bool useCursorAnchorWhenUnsnapped = false, const POINT* pCursorScreenPt = nullptr) {
    if (!data || !data->bmp) return;
    float targetScale = (data->initialScale > 0.0f) ? data->initialScale : 1.0f;
    if (!resetToOriginal) {
        float curScale = data->scale;
        targetScale = curScale * factor;

        int ow = std::max(1, data->origW);
        int oh = std::max(1, data->origH);
        int curW = 32, curH = 32;
        GetPinnedScaledDims(data, curW, curH);

        RECT rcImgScreen = { data->screenImgX, data->screenImgY, data->screenImgX + curW, data->screenImgY + curH };
        HMONITOR hMon = MonitorFromRect(&rcImgScreen, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        if (hMon && GetMonitorInfoW(hMon, &mi)) {
            screenW = std::max(1, (int)(mi.rcMonitor.right - mi.rcMonitor.left));
            screenH = std::max(1, (int)(mi.rcMonitor.bottom - mi.rcMonitor.top));
        }

        float scaleFitW = (float)((double)screenW / (double)ow);
        float scaleFitH = (float)((double)screenH / (double)oh);

        struct FitMilestone {
            float scale;
            bool isWidth;
        };
        FitMilestone milestones[2] = {
            { scaleFitW, true },
            { scaleFitH, false }
        };
        if (milestones[0].scale > milestones[1].scale) {
            std::swap(milestones[0], milestones[1]);
        }

        int nextW = std::max(1, (int)std::round((double)ow * (double)targetScale));
        int nextH = std::max(1, (int)std::round((double)oh * (double)targetScale));

        if (factor > 1.0f) {
            for (int i = 0; i < 2; ++i) {
                bool belowNow = milestones[i].isWidth ? (curW < screenW) : (curH < screenH);
                bool reachesNext = (targetScale >= milestones[i].scale) ||
                                   (milestones[i].isWidth ? (nextW >= screenW) : (nextH >= screenH));
                if (belowNow && curScale < milestones[i].scale - 1e-5f && reachesNext) {
                    targetScale = milestones[i].scale;
                    break;
                }
            }
        } else if (factor < 1.0f) {
            for (int i = 1; i >= 0; --i) {
                bool aboveNow = milestones[i].isWidth ? (curW > screenW) : (curH > screenH);
                bool reachesNext = (targetScale <= milestones[i].scale) ||
                                   (milestones[i].isWidth ? (nextW <= screenW) : (nextH <= screenH));
                if (aboveNow && curScale > milestones[i].scale + 1e-5f && reachesNext) {
                    targetScale = milestones[i].scale;
                    break;
                }
            }
        }
    }
    SetPinnedWindowExactScale(hWnd, data, targetScale, useCursorAnchorWhenUnsnapped, pCursorScreenPt);
}

static void ResetPinnedWindowSizeAndPosition(HWND hWnd, PinnedWindowData* data) {
    if (!data || !data->bmp) return;
    float minScale = 0.01f, maxScale = 64.0f;
    GetPinnedScaleBounds(data, minScale, maxScale);
    float initScale = (data->initialScale > 0.0f) ? data->initialScale : 1.0f;
    data->scale = std::max(minScale, std::min(maxScale, initScale));
    data->screenImgX = data->initialScreenImgX;
    data->screenImgY = data->initialScreenImgY;
    UpdatePinnedSnapModeFromPosition(data, nullptr, false);

    int imgW = 32, imgH = 32;
    GetPinnedScaledDims(data, imgW, imgH);
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
            data->hoveredPctResetBtn = (newHover == -1 && !data->hoveredSizeBox && !data->hoveredPctBox && PtInRect(&data->pctResetBtnRect, cur) != FALSE);
            SetCursor(LoadCursorW(nullptr, (data->hoveredSizeBox || data->hoveredPctBox) ? IDC_IBEAM : ((newHover != -1 || data->hoveredPctResetBtn) ? IDC_HAND : IDC_SIZEALL)));
            if (newHover != -1) {
                for (const auto& b : data->buttons) {
                    if (b.id == newHover) {
                        ShowPinnedBubbleTooltip(hWnd, b);
                        break;
                    }
                }
            } else if (data->hoveredPctResetBtn) {
                DockButton fakeTip;
                fakeTip.rect = data->pctResetBtnRect;
                fakeTip.tooltip = L"Set to 100%";
                ShowPinnedBubbleTooltip(hWnd, fakeTip);
            } else {
                HidePinnedBubbleTooltip();
            }
        }
        RenderPinnedLayeredWindow(hWnd, data);
    }
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
    int rawDx = cur.x - data->dragStartMouse.x;
    int rawDy = cur.y - data->dragStartMouse.y;

    bool shiftHeld = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) ||
                     ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) ||
                     ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0);

    std::vector<std::pair<HWND, PinnedWindowData*>> draggedPins;
    draggedPins.push_back({ hWnd, data });
    if (data->isSelected && g_Daemon.pinnedWindows.size() > 1) {
        for (HWND hp : g_Daemon.pinnedWindows) {
            if (hp && hp != hWnd && IsWindow(hp)) {
                PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hp, GWLP_USERDATA);
                if (d && d->isSelected && d->dragging) {
                    draggedPins.push_back({ hp, d });
                }
            }
        }
    }

    int snapAdjustX = 0;
    int snapAdjustY = 0;
    if (!shiftHeld) {
        const int snapDist = 14;
        int bestDistX = snapDist + 1;
        int bestDistY = snapDist + 1;

        for (const auto& item : draggedPins) {
            PinnedWindowData* d = item.second;
            int dImgW = 32, dImgH = 32;
            GetPinnedScaledDims(d, dImgW, dImgH);
            int candLeft   = d->dragStartWnd.x + rawDx;
            int candRight  = candLeft + dImgW;
            int candTop    = d->dragStartWnd.y + rawDy;
            int candBottom = candTop + dImgH;

            RECT rcCand = { candLeft, candTop, candRight, candBottom };
            HMONITOR hMon = (d == data)
                ? MonitorFromPoint(cur, MONITOR_DEFAULTTONEAREST)
                : MonitorFromRect(&rcCand, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(mi) };
            if (hMon && GetMonitorInfoW(hMon, &mi)) {
                int distLeft  = std::abs(candLeft  - (int)mi.rcMonitor.left);
                int distRight = std::abs(candRight - (int)mi.rcMonitor.right);
                if (distLeft <= snapDist && distLeft <= distRight && distLeft < bestDistX) {
                    bestDistX = distLeft;
                    snapAdjustX = (int)mi.rcMonitor.left - candLeft;
                } else if (distRight <= snapDist && distRight < bestDistX) {
                    bestDistX = distRight;
                    snapAdjustX = (int)mi.rcMonitor.right - candRight;
                }

                int distTop    = std::abs(candTop    - (int)mi.rcMonitor.top);
                int distBottom = std::abs(candBottom - (int)mi.rcMonitor.bottom);
                if (distTop <= snapDist && distTop <= distBottom && distTop < bestDistY) {
                    bestDistY = distTop;
                    snapAdjustY = (int)mi.rcMonitor.top - candTop;
                } else if (distBottom <= snapDist && distBottom < bestDistY) {
                    bestDistY = distBottom;
                    snapAdjustY = (int)mi.rcMonitor.bottom - candBottom;
                }
            }
        }
    }

    int appliedDx = rawDx + snapAdjustX;
    int appliedDy = rawDy + snapAdjustY;

    for (const auto& item : draggedPins) {
        HWND hp = item.first;
        PinnedWindowData* d = item.second;
        d->dragMoved = true;
        d->screenImgX = d->dragStartWnd.x + appliedDx;
        d->screenImgY = d->dragStartWnd.y + appliedDy;

        if (!shiftHeld) {
            int dImgW = 32, dImgH = 32;
            GetPinnedScaledDims(d, dImgW, dImgH);
            RECT rcPin = { d->screenImgX, d->screenImgY, d->screenImgX + dImgW, d->screenImgY + dImgH };
            HMONITOR hMon = (d == data)
                ? MonitorFromPoint(cur, MONITOR_DEFAULTTONEAREST)
                : MonitorFromRect(&rcPin, MONITOR_DEFAULTTONEAREST);
            UpdatePinnedSnapModeFromPosition(d, hMon, false);
        } else {
            UpdatePinnedSnapModeFromPosition(d, nullptr, true);
        }
        UpdatePinnedWindowLayout(hp, d, true);
    }
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

static bool IsPinAltHeld() {
    return ((GetKeyState(VK_MENU) & 0x8000) != 0) ||
           ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) ||
           ((GetAsyncKeyState(VK_LMENU) & 0x8000) != 0) ||
           ((GetAsyncKeyState(VK_RMENU) & 0x8000) != 0);
}

void SetPinnedWindowSelected(HWND hPin, PinnedWindowData* d, bool selected) {
    if (!hPin || !IsWindow(hPin) || !d || !d->bmp) return;
    if (d->isSelected == selected) return;
    d->isSelected = selected;
    int imgW = 32, imgH = 32;
    GetPinnedScaledDims(d, imgW, imgH);
    EnsurePinnedWindowCache(d, imgW, imgH, true);
    RenderPinnedLayeredWindow(hPin, d);
}

void ClearAllSelectedPinnedWindows(HWND hExcept) {
    for (HWND hp : g_Daemon.pinnedWindows) {
        if (hp && hp != hExcept && IsWindow(hp)) {
            PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hp, GWLP_USERDATA);
            if (d && d->isSelected) {
                SetPinnedWindowSelected(hp, d, false);
            }
        }
    }
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
        case DBTN_ACT_SCALE_RESET_100:
            SetFocus(hWnd);
            if (applyToAll && g_Daemon.pinnedWindows.size() > 1) {
                std::vector<HWND> pins = g_Daemon.pinnedWindows;
                for (HWND hPin : pins) {
                    if (hPin && IsWindow(hPin)) {
                        PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
                        if (d && d->bmp) {
                            CancelPinnedInlineEdits(d);
                            SetPinnedWindowExactScale(hPin, d, 1.0f);
                        }
                    }
                }
            } else {
                CancelPinnedInlineEdits(data);
                SetPinnedWindowExactScale(hWnd, data, 1.0f);
            }
            return;
        case DBTN_ACT_POS_RESET:
            SetFocus(hWnd);
            if (applyToAll && g_Daemon.pinnedWindows.size() > 1) {
                std::vector<HWND> pins = g_Daemon.pinnedWindows;
                for (HWND hPin : pins) {
                    if (hPin && IsWindow(hPin)) {
                        PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
                        if (d) ResetPinnedWindowSizeAndPosition(hPin, d);
                    }
                }
            } else {
                ResetPinnedWindowSizeAndPosition(hWnd, data);
            }
            return;
        case DBTN_ACT_PIN_HIDE_IMAGE: {
            SetFocus(hWnd);
            HidePinnedBubbleTooltip();
            bool nextHide = !data->hideImage;
            if (applyToAll) {
                std::vector<HWND> pins = g_Daemon.pinnedWindows;
                for (HWND hPin : pins) {
                    if (hPin && IsWindow(hPin)) {
                        PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
                        if (d && d->bmp) {
                            d->hideImage = nextHide;
                            UpdatePinnedWindowLayout(hPin, d, true);
                            RenderPinnedLayeredWindow(hPin, d);
                        }
                    }
                }
            } else {
                data->hideImage = nextHide;
                UpdatePinnedWindowLayout(hWnd, data, true);
                RenderPinnedLayeredWindow(hWnd, data);
            }
            return;
        }
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
        case DBTN_ACT_PIN_SEARCH_GOOGLE:
            SetFocus(hWnd);
            if (data->bmp) {
                g_Daemon.CopyBitmapToClipboard(data->bmp);
                g_Daemon.SearchClipboardImageOnGoogle();
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
                            std::wstring stem = g_Daemon.NextFreeCaptureStem(ext);
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
                std::wstring ext = PepperSnapDaemon::GetFormatExtension(g_Daemon.regionFormat);
                std::wstring stem = g_Daemon.NextFreeCaptureStem(ext);
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
                std::vector<WCHAR> szFile(4096, L'\0');
                std::wstring pinAllExt = PepperSnapDaemon::GetFormatExtension(g_Daemon.regionFormat);
                std::wstring defName = g_Daemon.NextFreeCaptureStem(pinAllExt) + pinAllExt;
                g_Daemon.SaveSettings();
                wcsncpy_s(szFile.data(), szFile.size(), defName.c_str(), _TRUNCATE);

                OPENFILENAMEW ofn = {0};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hWnd;
                ofn.lpstrTitle = L"Save All Pinned Images As";
                ofn.lpstrInitialDir = g_Daemon.saveFolder.c_str();
                ofn.lpstrFile = szFile.data();
                ofn.nMaxFile = (DWORD)szFile.size();
                ofn.lpstrFilter = L"JPEG image (*.jpg)\0*.jpg\0PNG image (*.png)\0*.png\0WebP image (*.webp)\0*.webp\0BMP bitmap (*.bmp)\0*.bmp\0";
                ofn.nFilterIndex = (DWORD)g_Daemon.regionFormat + 1;
                const wchar_t* defExts[4] = { L"jpg", L"png", L"webp", L"bmp" };
                ofn.lpstrDefExt = defExts[(int)g_Daemon.regionFormat & 3];
                ofn.Flags = OFN_PATHMUSTEXIST;

                if (GetSaveFileNameW(&ofn)) {
                    std::wstring chosenPath(szFile.data());
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
                std::vector<WCHAR> szFile(4096, L'\0');
                std::wstring pinOneExt = PepperSnapDaemon::GetFormatExtension(g_Daemon.regionFormat);
                std::wstring defName = g_Daemon.NextFreeCaptureStem(pinOneExt) + pinOneExt;
                g_Daemon.SaveSettings();
                wcsncpy_s(szFile.data(), szFile.size(), defName.c_str(), _TRUNCATE);

                OPENFILENAMEW ofn = {0};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hWnd;
                ofn.lpstrInitialDir = g_Daemon.saveFolder.c_str();
                ofn.lpstrFile = szFile.data();
                ofn.nMaxFile = (DWORD)szFile.size();
                ofn.lpstrFilter = L"JPEG image (*.jpg)\0*.jpg\0PNG image (*.png)\0*.png\0WebP image (*.webp)\0*.webp\0BMP bitmap (*.bmp)\0*.bmp\0";
                ofn.nFilterIndex = (DWORD)g_Daemon.regionFormat + 1;
                const wchar_t* defExts[4] = { L"jpg", L"png", L"webp", L"bmp" };
                ofn.lpstrDefExt = defExts[(int)g_Daemon.regionFormat & 3];
                ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

                if (GetSaveFileNameW(&ofn)) {
                    if (g_Daemon.SaveBitmapToPath(data->bmp, szFile.data())) {
                        g_Daemon.lastSavedFilePath = szFile.data();
                        g_Daemon.ShowTrayToast(
                            L"Saved capture",
                            std::wstring(szFile.data()) + L"\n(click to open in Explorer)"
                        );
                    }
                }
                if (IsWindow(hWnd)) SetFocus(hWnd);
            }
            return;
    }
}

LRESULT CALLBACK PinWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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
                } else if (data->hoveredBtnId == DBTN_ACT_DRAG_HUD) {
                    SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
                } else {
                    SetCursor(LoadCursorW(nullptr, (data->hoveredBtnId != -1 || data->hoveredPctResetBtn) ? IDC_HAND : IDC_SIZEALL));
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

                if (!data->hideToolbar && PtInRect(&data->pctResetBtnRect, pt)) {
                    HidePinnedBubbleTooltip();
                    if (data->isEditingSize) CommitPinnedSizeInput(hWnd, data, false);
                    if (data->isEditingPct)  CommitPinnedPctInput(hWnd, data, false);
                    data->pressedPctResetBtn = true;
                    data->pressedPctResetShift = shiftHeld;
                    data->hoveredPctResetBtn = true;
                    SetCapture(hWnd);
                    SetFocus(hWnd);
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    RenderPinnedLayeredWindow(hWnd, data);
                    return 0;
                }

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
                        if (b.id == DBTN_ACT_DRAG_HUD) {
                            data->dragging = true;
                            data->dragMoved = false;
                            GetCursorPos(&data->dragStartMouse);
                            data->dragStartWnd = { data->screenImgX, data->screenImgY };
                            SetCapture(hWnd);
                            SetFocus(hWnd);
                            return 0;
                        }
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
                data->pressedPctResetBtn = false;
                data->pressedPctResetShift = false;
                HidePinnedBubbleTooltip();

                if (IsPinCtrlHeld() && PtInRect(&data->imgRect, pt)) {
                    SetPinnedWindowSelected(hWnd, data, !data->isSelected);
                    SetFocus(hWnd);
                    return 0;
                }
                if (!data->isSelected && !IsPinCtrlHeld() && !IsPinAltHeld()) {
                    ClearAllSelectedPinnedWindows();
                }

                data->dragging = true;
                data->dragMoved = false;
                GetCursorPos(&data->dragStartMouse);
                data->dragStartWnd = { data->screenImgX, data->screenImgY };
                if (data->isSelected && g_Daemon.pinnedWindows.size() > 1) {
                    for (HWND hp : g_Daemon.pinnedWindows) {
                        if (hp && hp != hWnd && IsWindow(hp)) {
                            PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hp, GWLP_USERDATA);
                            if (d && d->isSelected) {
                                d->dragging = true;
                                d->dragMoved = false;
                                d->dragStartMouse = data->dragStartMouse;
                                d->dragStartWnd = { d->screenImgX, d->screenImgY };
                            }
                        }
                    }
                }
                SetCapture(hWnd);
                SetFocus(hWnd);
            }
            return 0;
        case WM_LBUTTONDBLCLK:
            if (data) {
                KillTimer(hWnd, 2);
                data->showingImgHoverTip = false;
                POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

                if (!data->hideToolbar && PtInRect(&data->pctResetBtnRect, pt)) {
                    HidePinnedBubbleTooltip();
                    if (data->isEditingSize) CommitPinnedSizeInput(hWnd, data, false);
                    if (data->isEditingPct)  CommitPinnedPctInput(hWnd, data, false);
                    data->pressedPctResetBtn = true;
                    data->pressedPctResetShift = IsPinShiftHeld();
                    data->hoveredPctResetBtn = true;
                    SetCapture(hWnd);
                    SetFocus(hWnd);
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    RenderPinnedLayeredWindow(hWnd, data);
                    return 0;
                }

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
                if (IsPinCtrlHeld() && PtInRect(&data->imgRect, pt)) {
                    SetPinnedWindowSelected(hWnd, data, !data->isSelected);
                    SetFocus(hWnd);
                    return 0;
                }
                if (IsPinAltHeld()) {
                    // Rapid Alt + click (or Alt + Shift + click) toggles pause/play on release instead of toggling toolbar
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
                    for (HWND hp : g_Daemon.pinnedWindows) {
                        if (hp && hp != hWnd && IsWindow(hp)) {
                            PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hp, GWLP_USERDATA);
                            if (d && d->dragging) {
                                d->dragging = false;
                                d->dragMoved = false;
                            }
                        }
                    }
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
                    bool shiftHeld = IsPinShiftHeld();
                    if (IsPinCtrlHeld()) {
                        ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_POS_RESET, shiftHeld);
                    } else {
                        ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_ZOOM_RESET, shiftHeld);
                    }
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
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN: {
            if (data && data->dragging && (wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT)) {
                UpdatePinnedDragPosition(hWnd, data);
                return 0;
            }
            if (wParam == VK_MENU || wParam == VK_LMENU || wParam == VK_RMENU) {
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
                if (MatchesPinnedShortcutWithOptionalShift(g_Daemon.hkPinPosReset, vk, ctrl, shift, alt, &applyAll)) {
                    ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_POS_RESET, applyAll);
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
                if (MatchesOverlayShortcut(g_Daemon.hkPinSearchGoogle, vk, ctrl, shift, alt)) {
                    ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_PIN_SEARCH_GOOGLE, false);
                    return 0;
                }
                if (MatchesPinnedShortcutWithOptionalShift(g_Daemon.hkPinCopy, vk, ctrl, shift, alt, &applyAll)) {
                    ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_COPY, applyAll);
                    return 0;
                }
            }
            break;
        }
        case WM_SYSKEYUP:
        case WM_KEYUP:
            if (data && data->dragging && (wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT)) {
                UpdatePinnedDragPosition(hWnd, data);
                return 0;
            }
            if (wParam == VK_MENU || wParam == VK_LMENU || wParam == VK_RMENU) {
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
                    !data->hoveredSizeBox && !data->hoveredPctBox && !data->hoveredPctResetBtn && !data->pressedPctResetBtn) {
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
                if (data->pressedPctResetBtn) {
                    KillTimer(hWnd, 2);
                    if (data->showingImgHoverTip) {
                        data->showingImgHoverTip = false;
                        HidePinnedBubbleTooltip();
                    }
                    bool newResetHov = (!data->hideToolbar && PtInRect(&data->pctResetBtnRect, pt) != FALSE);
                    if (newResetHov != data->hoveredPctResetBtn) {
                        data->hoveredPctResetBtn = newResetHov;
                        SetCursor(LoadCursorW(nullptr, newResetHov ? IDC_HAND : IDC_SIZEALL));
                        if (newResetHov) {
                            DockButton fakeTip;
                            fakeTip.rect = data->pctResetBtnRect;
                            fakeTip.tooltip = L"Set to 100%";
                            ShowPinnedBubbleTooltip(hWnd, fakeTip);
                        } else {
                            HidePinnedBubbleTooltip();
                        }
                        RenderPinnedLayeredWindow(hWnd, data);
                    }
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
                bool newPctResetHov = (newHover == -1 && !newSizeHov && !newPctHov && !data->hideToolbar && PtInRect(&data->pctResetBtnRect, pt) != FALSE);

                if (newHover != -1 || newSizeHov || newPctHov || newPctResetHov) {
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
                if (newHover != data->hoveredBtnId || newSizeHov != data->hoveredSizeBox || newPctHov != data->hoveredPctBox || newPctResetHov != data->hoveredPctResetBtn) {
                    data->hoveredBtnId = newHover;
                    data->hoveredSizeBox = newSizeHov;
                    data->hoveredPctBox = newPctHov;
                    data->hoveredPctResetBtn = newPctResetHov;
                    SetCursor(LoadCursorW(nullptr, (newSizeHov || newPctHov) ? IDC_IBEAM : ((newHover != -1 || newPctResetHov) ? IDC_HAND : IDC_SIZEALL)));
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
                    } else if (newPctResetHov) {
                        DockButton fakeTip;
                        fakeTip.rect = data->pctResetBtnRect;
                        fakeTip.tooltip = L"Set to 100%";
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
                if (data->hoveredBtnId != -1 || data->hoveredSizeBox || data->hoveredPctBox || data->hoveredPctResetBtn) {
                    data->hoveredBtnId = -1;
                    data->hoveredSizeBox = false;
                    data->hoveredPctBox = false;
                    data->hoveredPctResetBtn = false;
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
                if (data->pressedPctResetBtn) {
                    bool releasedWithShift = data->pressedPctResetShift || IsPinShiftHeld();
                    data->pressedPctResetBtn = false;
                    data->pressedPctResetShift = false;
                    ReleaseCapture();
                    POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                    bool hit = (!data->hideToolbar && PtInRect(&data->pctResetBtnRect, pt) != FALSE);
                    data->hoveredPctResetBtn = hit;
                    RenderPinnedLayeredWindow(hWnd, data);
                    if (hit) {
                        ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_SCALE_RESET_100, releasedWithShift);
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
                        if (releasedBtnId == DBTN_ACT_COPY && releasedWithShift) {
                            // Shift + click Copy means every pinned image, which is
                            // what Shift means to every other button here and to the
                            // Copy shortcut key. It is no longer a search.
                            ExecutePinnedWindowAction(hWnd, data, DBTN_ACT_COPY, true);
                        } else {
                            ExecutePinnedWindowAction(hWnd, data, releasedBtnId, releasedWithShift);
                        }
                    }
                    return 0;
                }
                if (data->dragging) {
                    bool wasMoved = data->dragMoved;
                    data->dragging = false;
                    data->dragMoved = false;
                    for (HWND hp : g_Daemon.pinnedWindows) {
                        if (hp && hp != hWnd && IsWindow(hp)) {
                            PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hp, GWLP_USERDATA);
                            if (d && d->dragging) {
                                bool dMoved = d->dragMoved;
                                d->dragging = false;
                                d->dragMoved = false;
                                if (dMoved) {
                                    UpdatePinnedWindowLayout(hp, d, true);
                                    RenderPinnedLayeredWindow(hp, d);
                                }
                            }
                        }
                    }
                    ReleaseCapture();
                    POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                    if (wasMoved) {
                        UpdatePinnedWindowLayout(hWnd, data, true);
                        RenderPinnedLayeredWindow(hWnd, data);
                    } else if (PtInRect(&data->imgRect, pt) && IsPinAltHeld()) {
                        TogglePinnedWindowAnimation(hWnd, data, IsPinShiftHeld());
                    } else if (PtInRect(&data->imgRect, pt) && !IsPinCtrlHeld() && data->isSelected) {
                        ClearAllSelectedPinnedWindows();
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
                    POINT wheelPt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                    GetCursorPos(&wheelPt);
                    float steps = (float)delta / (float)WHEEL_DELTA;
                    float factor = std::pow(1.15f, steps);
                    if (IsPinShiftHeld() && g_Daemon.pinnedWindows.size() > 1) {
                        std::vector<HWND> pins = g_Daemon.pinnedWindows;
                        for (HWND hPin : pins) {
                            if (hPin && IsWindow(hPin)) {
                                PinnedWindowData* d = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
                                if (d) {
                                    bool isHoveredPin = (hPin == hWnd);
                                    ApplyPinnedWindowZoom(hPin, d, factor, false, isHoveredPin, isHoveredPin ? &wheelPt : nullptr);
                                }
                            }
                        }
                    } else {
                        ApplyPinnedWindowZoom(hWnd, data, factor, false, true, &wheelPt);
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

HWND PepperSnapDaemon::CreatePinnedWindow(Bitmap* bmp, int x, int y, float initialScale, std::vector<Bitmap*> animFrames, std::vector<int> animDelaysMs, bool initiallySelected) {
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
    data->initialScale = initialScale;
    data->screenImgX = x;
    data->screenImgY = y;
    data->initialScreenImgX = x;
    data->initialScreenImgY = y;
    data->hideToolbar = hidePinnedToolbar;
    data->hideOutline = hidePinnedOutline;
    data->smoothImage = smoothPinnedImage;
    data->isSelected = initiallySelected;
    UpdatePinnedSnapModeFromPosition(data, nullptr, false);

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
    if (hOverlayWnd && IsWindow(hOverlayWnd) && IsEditingOpenedImage()) {
        if (pinnedImageAboveEdit) {
            RaisePinnedWindowsAboveOverlay();
        } else {
            SetWindowPos(hOverlayWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            SetForegroundWindow(hOverlayWnd);
            SetFocus(hOverlayWnd);
        }
    }
    return hPin;
}

void PepperSnapDaemon::ActionPinToDesktop() {
    // Off the screen before the render and the optional file write, both of which
    // are slow enough to read as a frozen toolbar (see HideOverlayForPendingWork).
    // Everything below reads daemon state, not the window.
    bool hidOverlay = HideOverlayForPendingWork();
    Bitmap* cropped = RenderCroppedRegionBitmap();
    if (!cropped) {
        RestoreOverlayAfterPendingWork(hidOverlay);
        return;
    }
    int rx = std::max(0, (int)std::min(selRect.left, selRect.right));
    int ry = std::max(0, (int)std::min(selRect.top, selRect.bottom));
    int rw = std::max(1, (int)cropped->GetWidth());
    int rh = std::max(1, (int)cropped->GetHeight());
    int sx = vScreenX + rx;
    int sy = vScreenY + ry;
    bool pinHasCustomQuad = hasCustomQuad;
    bool pinHasEllipseMask = hasEllipseSelection;
    PointF pinNormQuad[4];
    if (pinHasCustomQuad) {
        for (int i = 0; i < 4; ++i) {
            pinNormQuad[i].X = std::max(0.0f, std::min(1.0f, (float)(selQuad[i].x - rx) / (float)std::max(1, rw - 1)));
            pinNormQuad[i].Y = std::max(0.0f, std::min(1.0f, (float)(selQuad[i].y - ry) / (float)std::max(1, rh - 1)));
        }
    }

    // Take the overlay down before the optional file write, but after the pin's
    // geometry has been read above.
    CloseRegionSnipOverlay();

    std::wstring savedNote;
    if (alsoSavePinned) {
        CreateDirectoryW(saveFolder.c_str(), nullptr);
        std::wstring pinExt = GetFormatExtension(regionFormat);
        std::wstring full = saveFolder + L"\\" + NextFreeCaptureStem(pinExt) + pinExt;
        if (SaveBitmapToPath(cropped, full)) {
            lastSavedFilePath = full;
            savedNote = L"Saved to: " + full + L"\n";
        }
    }

    HWND hPin = CreatePinnedWindow(cropped, sx, sy, 1.0f);
    if (hPin && (pinHasCustomQuad || pinHasEllipseMask)) {
        PinnedWindowData* pData = (PinnedWindowData*)GetWindowLongPtrW(hPin, GWLP_USERDATA);
        if (pData) {
            pData->hasCustomQuad = pinHasCustomQuad;
            pData->hasEllipseMask = pinHasEllipseMask;
            if (pinHasCustomQuad) {
                for (int i = 0; i < 4; ++i) pData->normQuad[i] = pinNormQuad[i];
            }
            FreePinnedWindowCache(pData);
            RenderPinnedLayeredWindow(hPin, pData);
        }
    }
    ShowTrayToast(L"Pinned on top", savedNote + L"Drag to move · Scroll wheel to scale.");
}

// Unified Options Modal (Save Folder, Default Formats, JPEG Quality Slider, Naming Pattern + Restore Default, Non-Stacking Highlighter)

#define IDC_SC_SEARCH_EDIT  3901
#define IDC_SC_SEARCH_CLEAR 3902
#define IDM_SC_CTX_COPY     3910
#define IDM_SC_CTX_SELALL   3911
#define TIMER_SC_DRAG_SCROLL 88

struct ShortcutEntry {
    bool isSectionHeader = false;
    bool hasIcon = false;
    DockButton iconBtn;
    int iconAtlasY = -1;
    std::wstring sectionTitle;
    std::wstring keyCol;
    std::wstring descCol;
    bool visible = true;
    int y = 0;
    int h = 28;
    int textH = 20;
};

struct ShortcutsVisualLine {
    int entryIdx = -1;
    bool isSection = false;
    bool isKeyCol = false;
    int x = 0;
    int y = 0;
    int h = 20;
    size_t docStart = 0;
    size_t docEnd = 0;
    std::wstring text;
    std::vector<int> charX;
};

struct ShortcutsDlgParams {
    std::wstring mainTitle;
    std::vector<ShortcutEntry> entries;
    std::vector<ShortcutsVisualLine> visualLines;
    std::wstring fullDocText;
    std::wstring searchQuery;
    std::vector<std::wstring> searchTokens;
    int matchRowCount = 0;
    size_t selAnchor = 0;
    size_t selCaret = 0;
    bool isSelectingText = false;
    std::wstring copiedStatusText;

    int leftColX = 22;
    int leftColW = 240;
    int rightColX = 276;
    int rightColW = 480;
    int headerHeight = 76;
    int scrollY = 0;
    int scrollContentHeight = 580;
    int footerHeight = 52;
    int minWinWidth = 740;
    int maxWinWidth = 1080;
    int fullWinHeight = 680;
    int savedWinWidth = 900;
    int savedWinHeight = 620;
    int lastLayoutClientW = -1;
    int lastOkBtnX = -1;
    int lastOkBtnY = -1;
    int lastSearchEditW = -1;
    int lastClearBtnX = -1;
    HWND hSearchEdit = nullptr;
    HWND hClearSearchBtn = nullptr;
    HWND hFooterBg = nullptr;
    HWND hOkBtn = nullptr;

    // Cached GDI resources, pre-rendered icon atlas, and full content bitmap cache for zero-lag resize & scroll
    HFONT hTitleFont = nullptr;
    HFONT hSectionFont = nullptr;
    HFONT hKeyFont = nullptr;
    HFONT hDescFont = nullptr;
    HPEN hSepShadow = nullptr;
    HPEN hSepHilight = nullptr;
    HPEN hRowSep = nullptr;
    HPEN hFooterLinePen = nullptr;
    HBRUSH hSearchMatchBrush = nullptr;
    HBRUSH hSelHighlightBrush = nullptr;
    HDC hMeasureDC = nullptr;
    HGDIOBJ hMeasureOldFont = nullptr;
    HDC hIconAtlasDC = nullptr;
    HBITMAP hIconAtlasBmp = nullptr;
    HGDIOBJ hIconAtlasOldBmp = nullptr;
    HDC hContentDC = nullptr;
    HBITMAP hContentBmp = nullptr;
    HGDIOBJ hContentOldBmp = nullptr;
    int contentCapW = 0;
    int contentCapH = 0;
    int renderedContentW = -1;
    int renderedContentH = -1;
    HDC hBackDC = nullptr;
    HBITMAP hBackBmp = nullptr;
    HGDIOBJ hBackOldBmp = nullptr;
    int backCapW = 0;
    int backCapH = 0;

    bool HasSelection() const {
        return selAnchor != selCaret && !fullDocText.empty();
    }

    std::wstring GetSelectedText() const {
        if (!HasSelection()) return L"";
        size_t sMin = std::min(selAnchor, selCaret);
        size_t sMax = std::max(selAnchor, selCaret);
        if (sMin >= fullDocText.size()) return L"";
        sMax = std::min(sMax, fullDocText.size());
        return fullDocText.substr(sMin, sMax - sMin);
    }

    static std::wstring ToLowerStr(const std::wstring& s) {
        std::wstring res = s;
        for (wchar_t& ch : res) ch = (wchar_t)towlower(ch);
        return res;
    }

    void UpdateSearchFilter(const std::wstring& query) {
        searchQuery = query;
        searchTokens.clear();
        std::wstring lowQ = ToLowerStr(query);
        size_t pos = 0;
        while (pos < lowQ.size()) {
            while (pos < lowQ.size() && iswspace(lowQ[pos])) ++pos;
            if (pos >= lowQ.size()) break;
            size_t start = pos;
            while (pos < lowQ.size() && !iswspace(lowQ[pos])) ++pos;
            searchTokens.push_back(lowQ.substr(start, pos - start));
        }

        matchRowCount = 0;
        for (auto& e : entries) {
            if (e.isSectionHeader) {
                e.visible = false;
                continue;
            }
            if (searchTokens.empty()) {
                e.visible = true;
                matchRowCount++;
            } else {
                std::wstring haystack = ToLowerStr(e.keyCol + L" " + e.descCol + L" " + e.sectionTitle);
                bool allMatch = true;
                for (const auto& tok : searchTokens) {
                    if (haystack.find(tok) == std::wstring::npos) {
                        allMatch = false;
                        break;
                    }
                }
                e.visible = allMatch;
                if (allMatch) matchRowCount++;
            }
        }

        for (size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].isSectionHeader) {
                bool anyChildVisible = false;
                for (size_t j = i + 1; j < entries.size() && !entries[j].isSectionHeader; ++j) {
                    if (entries[j].visible) {
                        anyChildVisible = true;
                        break;
                    }
                }
                entries[i].visible = anyChildVisible;
            }
        }

        selAnchor = selCaret = 0;
        isSelectingText = false;
        renderedContentW = -1;
    }

    void InitGdiCache(HDC hdcRef) {
        if (hTitleFont) return;
        hTitleFont = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        hSectionFont = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        hKeyFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        hDescFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        hSepShadow = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
        hSepHilight = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DHILIGHT));
        hRowSep = CreatePen(PS_SOLID, 1, RGB(218, 222, 228));
        hFooterLinePen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
        hSearchMatchBrush = CreateSolidBrush(RGB(254, 240, 138));
        hSelHighlightBrush = CreateSolidBrush(RGB(37, 99, 235));

        hMeasureDC = CreateCompatibleDC(hdcRef);
        if (hMeasureDC && hDescFont) {
            hMeasureOldFont = SelectObject(hMeasureDC, hDescFont);
        }

        // Pre-render all row icons once into a GDI bitmap atlas so content cache build does fast BitBlt
        const int iconW = 26;
        const int iconH = 22;
        int iconCount = 0;
        for (auto& e : entries) {
            if (!e.isSectionHeader && e.hasIcon) {
                e.iconAtlasY = iconCount * iconH;
                iconCount++;
            } else {
                e.iconAtlasY = -1;
            }
        }
        if (iconCount > 0) {
            int atlasH = iconCount * iconH;
            hIconAtlasDC = CreateCompatibleDC(hdcRef);
            hIconAtlasBmp = CreateCompatibleBitmap(hdcRef, iconW, atlasH);
            hIconAtlasOldBmp = SelectObject(hIconAtlasDC, hIconAtlasBmp);
            RECT rcAtlas = { 0, 0, iconW, atlasH };
            FillRect(hIconAtlasDC, &rcAtlas, GetSysColorBrush(COLOR_BTNFACE));

            Graphics gAtlas(hIconAtlasDC);
            gAtlas.SetSmoothingMode(SmoothingModeAntiAlias);
            gAtlas.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
            for (const auto& e : entries) {
                if (!e.isSectionHeader && e.hasIcon && e.iconAtlasY >= 0) {
                    int iy = e.iconAtlasY;
                    RectF rfIcon(0.0f, (float)iy, (float)iconW, (float)iconH);
                    Color bgCol = e.iconBtn.isPrimaryAction ? Color(255, 239, 68, 68) : Color(245, 15, 23, 42);
                    SolidBrush btnBg(bgCol);
                    Pen btnBorder(e.iconBtn.isPrimaryAction ? Color(255, 252, 165, 165) : Color(220, 51, 65, 85), 1.0f);
                    GraphicsState stB = gAtlas.Save();
                    gAtlas.SetSmoothingMode(SmoothingModeNone);
                    gAtlas.SetPixelOffsetMode(PixelOffsetModeNone);
                    gAtlas.FillRectangle(&btnBg, 0, iy, iconW, iconH);
                    gAtlas.Restore(stB);
                    PepperSnapDaemon::DrawDockButtonIcon(gAtlas, e.iconBtn, rfIcon);
                    GraphicsState stO = gAtlas.Save();
                    gAtlas.SetSmoothingMode(SmoothingModeNone);
                    gAtlas.SetPixelOffsetMode(PixelOffsetModeNone);
                    gAtlas.DrawRectangle(&btnBorder, 0, iy, iconW - 1, iconH - 1);
                    gAtlas.Restore(stO);
                }
            }
        }
    }

    void DrawLineSearchHighlights(HDC hdc, const ShortcutsVisualLine& ln) const {
        if (searchTokens.empty() || ln.text.empty() || ln.charX.size() <= ln.text.size() || !hSearchMatchBrush) return;
        std::wstring lowLine = ToLowerStr(ln.text);
        for (const auto& tok : searchTokens) {
            if (tok.empty()) continue;
            size_t pos = 0;
            while ((pos = lowLine.find(tok, pos)) != std::wstring::npos) {
                size_t endPos = std::min(ln.text.size(), pos + tok.size());
                RECT rcM = { ln.x + ln.charX[pos], ln.y, ln.x + ln.charX[endPos], ln.y + ln.h };
                FillRect(hdc, &rcM, hSearchMatchBrush);
                pos += tok.size();
            }
        }
    }

    void EnsureContentRendered(HDC hdcRef, int clientW) {
        if (!hTitleFont) {
            InitGdiCache(hdcRef);
        }
        int contentH = std::max(64, scrollContentHeight);
        if (hContentDC && renderedContentW == clientW && renderedContentH == contentH) {
            return;
        }
        if (!hContentDC || clientW > contentCapW || contentH > contentCapH) {
            if (!hContentDC) {
                hContentDC = CreateCompatibleDC(hdcRef);
            } else if (hContentOldBmp) {
                SelectObject(hContentDC, hContentOldBmp);
                if (hContentBmp) DeleteObject(hContentBmp);
            }
            contentCapW = std::max(clientW + 128, maxWinWidth);
            contentCapH = std::max(contentH + 256, 2800);
            hContentBmp = CreateCompatibleBitmap(hdcRef, contentCapW, contentCapH);
            hContentOldBmp = SelectObject(hContentDC, hContentBmp);
        }

        renderedContentW = clientW;
        renderedContentH = contentH;

        RECT rcFill = { 0, 0, clientW, contentH };
        FillRect(hContentDC, &rcFill, GetSysColorBrush(COLOR_BTNFACE));
        SetBkMode(hContentDC, TRANSPARENT);

        int padX = 18;
        int rightEdge = std::max(rightColX + 240, clientW - padX);

        HGDIOBJ hOldF = SelectObject(hContentDC, hTitleFont);
        HGDIOBJ hOldP = SelectObject(hContentDC, hSepShadow);

        const int iconW = 26;
        const int iconH = 22;

        if (visualLines.empty() && !searchTokens.empty()) {
            SelectObject(hContentDC, hDescFont);
            SetTextColor(hContentDC, GetSysColor(COLOR_GRAYTEXT));
            RECT rcEmpty = { padX, 28, rightEdge, 64 };
            DrawTextW(hContentDC, L"No matching keyboard shortcuts or info found.", -1, &rcEmpty, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        bool firstVisibleSection = true;
        bool prevWasSection = false;
        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& e = entries[i];
            if (!e.visible) continue;
            int drawY = e.y;

            if (e.isSectionHeader) {
                if (!firstVisibleSection) {
                    int lineY = drawY + 6;
                    SelectObject(hContentDC, hSepShadow);
                    MoveToEx(hContentDC, padX, lineY, nullptr);
                    LineTo(hContentDC, rightEdge, lineY);
                    SelectObject(hContentDC, hSepHilight);
                    MoveToEx(hContentDC, padX, lineY + 1, nullptr);
                    LineTo(hContentDC, rightEdge, lineY + 1);
                }
                firstVisibleSection = false;
                prevWasSection = true;
            } else {
                if (!prevWasSection) {
                    SelectObject(hContentDC, hRowSep);
                    MoveToEx(hContentDC, leftColX, drawY, nullptr);
                    LineTo(hContentDC, rightEdge, drawY);
                }
                prevWasSection = false;

                if (e.hasIcon) {
                    int iconX = leftColX;
                    int iconY = drawY + (e.h - iconH) / 2;
                    if (hIconAtlasDC && e.iconAtlasY >= 0) {
                        BitBlt(hContentDC, iconX, iconY, iconW, iconH, hIconAtlasDC, 0, e.iconAtlasY, SRCCOPY);
                    }
                } else {
                    SelectObject(hContentDC, hKeyFont);
                    SetTextColor(hContentDC, RGB(30, 41, 59));
                    RECT rcBullet = { leftColX, drawY, leftColX + 18, drawY + e.h };
                    DrawTextW(hContentDC, L"•", -1, &rcBullet, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                }
            }
        }

        for (const auto& ln : visualLines) {
            DrawLineSearchHighlights(hContentDC, ln);
            if (ln.isSection) {
                SelectObject(hContentDC, hSectionFont);
                SetTextColor(hContentDC, RGB(15, 23, 42));
            } else if (ln.isKeyCol) {
                SelectObject(hContentDC, hKeyFont);
                SetTextColor(hContentDC, RGB(30, 41, 59));
            } else {
                SelectObject(hContentDC, hDescFont);
                SetTextColor(hContentDC, GetSysColor(COLOR_WINDOWTEXT));
            }
            RECT rcLn = { ln.x, ln.y, rightEdge, ln.y + ln.h };
            DrawTextW(hContentDC, ln.text.c_str(), (int)ln.text.size(), &rcLn, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        SelectObject(hContentDC, hOldP);
        SelectObject(hContentDC, hOldF);
    }

    void FreeGdiCache() {
        if (hContentDC) {
            if (hContentOldBmp) SelectObject(hContentDC, hContentOldBmp);
            if (hContentBmp) DeleteObject(hContentBmp);
            DeleteDC(hContentDC);
            hContentDC = nullptr;
            hContentBmp = nullptr;
            hContentOldBmp = nullptr;
            contentCapW = contentCapH = 0;
            renderedContentW = renderedContentH = -1;
        }
        if (hBackDC) {
            if (hBackOldBmp) SelectObject(hBackDC, hBackOldBmp);
            if (hBackBmp) DeleteObject(hBackBmp);
            DeleteDC(hBackDC);
            hBackDC = nullptr;
            hBackBmp = nullptr;
            hBackOldBmp = nullptr;
            backCapW = backCapH = 0;
        }
        if (hIconAtlasDC) {
            if (hIconAtlasOldBmp) SelectObject(hIconAtlasDC, hIconAtlasOldBmp);
            if (hIconAtlasBmp) DeleteObject(hIconAtlasBmp);
            DeleteDC(hIconAtlasDC);
            hIconAtlasDC = nullptr;
            hIconAtlasBmp = nullptr;
            hIconAtlasOldBmp = nullptr;
        }
        if (hMeasureDC) {
            if (hMeasureOldFont) SelectObject(hMeasureDC, hMeasureOldFont);
            DeleteDC(hMeasureDC);
            hMeasureDC = nullptr;
            hMeasureOldFont = nullptr;
        }
        if (hSelHighlightBrush) { DeleteObject(hSelHighlightBrush); hSelHighlightBrush = nullptr; }
        if (hSearchMatchBrush)  { DeleteObject(hSearchMatchBrush);  hSearchMatchBrush = nullptr; }
        if (hFooterLinePen) { DeleteObject(hFooterLinePen); hFooterLinePen = nullptr; }
        if (hRowSep)        { DeleteObject(hRowSep);        hRowSep = nullptr; }
        if (hSepHilight)    { DeleteObject(hSepHilight);    hSepHilight = nullptr; }
        if (hSepShadow)     { DeleteObject(hSepShadow);     hSepShadow = nullptr; }
        if (hDescFont)      { DeleteObject(hDescFont);      hDescFont = nullptr; }
        if (hKeyFont)       { DeleteObject(hKeyFont);       hKeyFont = nullptr; }
        if (hSectionFont)   { DeleteObject(hSectionFont);   hSectionFont = nullptr; }
        if (hTitleFont)     { DeleteObject(hTitleFont);     hTitleFont = nullptr; }
    }
};

static void ComputeVisualLineCharOffsets(HDC hdc, HFONT hFont, ShortcutsVisualLine& ln) {
    ln.charX.assign(ln.text.size() + 1, 0);
    if (!hdc || ln.text.empty()) return;
    HGDIOBJ hOld = hFont ? SelectObject(hdc, hFont) : nullptr;
    std::vector<INT> dx(ln.text.size(), 0);
    SIZE sz = { 0, 0 };
    if (GetTextExtentExPointW(hdc, ln.text.c_str(), (int)ln.text.size(), 0, nullptr, dx.data(), &sz)) {
        for (size_t i = 0; i < ln.text.size(); ++i) {
            ln.charX[i + 1] = dx[i];
        }
    }
    if (hFont && hOld) SelectObject(hdc, hOld);
}

static std::vector<std::wstring> WrapShortcutsDescText(HDC hdc, HFONT hFont, const std::wstring& text, int maxWidth) {
    std::vector<std::wstring> lines;
    if (text.empty()) {
        lines.push_back(L"");
        return lines;
    }
    HGDIOBJ hOld = (hdc && hFont) ? SelectObject(hdc, hFont) : nullptr;
    auto measureW = [&](const std::wstring& s) -> int {
        if (!hdc || s.empty()) return 0;
        SIZE sz = { 0, 0 };
        GetTextExtentPoint32W(hdc, s.c_str(), (int)s.size(), &sz);
        return sz.cx;
    };

    size_t n = text.size();
    size_t i = 0;
    std::wstring curLine;
    while (i < n) {
        if (text[i] == L'\n') {
            lines.push_back(curLine);
            curLine.clear();
            ++i;
            continue;
        }
        while (i < n && text[i] == L' ') ++i;
        if (i >= n) break;
        size_t wordStart = i;
        while (i < n && text[i] != L' ' && text[i] != L'\n') ++i;
        std::wstring word = text.substr(wordStart, i - wordStart);

        std::wstring candidate = curLine.empty() ? word : (curLine + L" " + word);
        if (!curLine.empty() && measureW(candidate) > maxWidth) {
            lines.push_back(curLine);
            curLine = word;
        } else {
            curLine = candidate;
        }
    }
    if (!curLine.empty() || lines.empty()) {
        lines.push_back(curLine);
    }
    if (hdc && hFont && hOld) SelectObject(hdc, hOld);
    return lines;
}

static void RecalcShortcutsLayout(HWND hWnd, ShortcutsDlgParams* p, int clientW, bool forceRecalc = false) {
    if (!p) return;
    if (!forceRecalc && clientW == p->lastLayoutClientW && p->scrollContentHeight > 0) {
        return;
    }
    p->lastLayoutClientW = clientW;
    p->renderedContentW = -1;

    int padX = 18;
    int rightEdge = std::max(p->rightColX + 240, clientW - padX);
    int availDescW = std::max(240, rightEdge - p->rightColX);
    p->rightColW = availDescW;

    HDC hdcMeasure = p->hMeasureDC;
    bool tempDC = false;
    HFONT hTempSectionFont = nullptr;
    HFONT hTempKeyFont = nullptr;
    HFONT hTempDescFont = nullptr;
    HGDIOBJ hOldF = nullptr;
    if (!hdcMeasure) {
        hdcMeasure = hWnd ? GetDC(hWnd) : GetDC(nullptr);
        tempDC = true;
        hTempSectionFont = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        hTempKeyFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        hTempDescFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        if (hdcMeasure) hOldF = SelectObject(hdcMeasure, hTempDescFont);
    }

    HFONT useSectionFont = p->hSectionFont ? p->hSectionFont : hTempSectionFont;
    HFONT useKeyFont     = p->hKeyFont     ? p->hKeyFont     : hTempKeyFont;
    HFONT useDescFont    = p->hDescFont    ? p->hDescFont    : hTempDescFont;

    p->visualLines.clear();
    p->fullDocText.clear();

    const int lineH = 20;
    const int iconW = 26;
    int curY = 6;
    bool firstVisibleSection = true;

    for (size_t i = 0; i < p->entries.size(); ++i) {
        auto& e = p->entries[i];
        if (!e.visible) {
            e.y = curY;
            e.h = 0;
            continue;
        }
        e.y = curY;
        if (e.isSectionHeader) {
            e.h = firstVisibleSection ? 28 : 38;
            e.textH = lineH;
            int hdrTop = firstVisibleSection ? (e.y + 4) : (e.y + 14);
            firstVisibleSection = false;

            ShortcutsVisualLine ln;
            ln.entryIdx = (int)i;
            ln.isSection = true;
            ln.isKeyCol = false;
            ln.x = padX;
            ln.y = hdrTop;
            ln.h = lineH;
            ln.text = e.keyCol;
            ln.docStart = p->fullDocText.size();
            p->fullDocText += ln.text;
            ln.docEnd = p->fullDocText.size();
            p->fullDocText += L"\r\n";
            ComputeVisualLineCharOffsets(hdcMeasure, useSectionFont, ln);
            p->visualLines.push_back(std::move(ln));
        } else {
            std::vector<std::wstring> wrapped = WrapShortcutsDescText(hdcMeasure, useDescFont, e.descCol, availDescW);
            int measuredH = std::max(lineH, (int)wrapped.size() * lineH);
            e.textH = measuredH;
            e.h = std::max(28, measuredH + 10);

            int keyTextLeft = p->leftColX + (e.hasIcon ? (iconW + 8) : 16);
            ShortcutsVisualLine keyLn;
            keyLn.entryIdx = (int)i;
            keyLn.isSection = false;
            keyLn.isKeyCol = true;
            keyLn.x = keyTextLeft;
            keyLn.y = e.y + (e.h - lineH) / 2;
            keyLn.h = lineH;
            keyLn.text = e.keyCol;
            keyLn.docStart = p->fullDocText.size();
            p->fullDocText += keyLn.text;
            keyLn.docEnd = p->fullDocText.size();
            p->fullDocText += L"\t";
            ComputeVisualLineCharOffsets(hdcMeasure, useKeyFont, keyLn);
            p->visualLines.push_back(std::move(keyLn));

            int descTop = e.y + std::max(2, (e.h - e.textH) / 2);
            for (size_t lnIdx = 0; lnIdx < wrapped.size(); ++lnIdx) {
                ShortcutsVisualLine descLn;
                descLn.entryIdx = (int)i;
                descLn.isSection = false;
                descLn.isKeyCol = false;
                descLn.x = p->rightColX;
                descLn.y = descTop + (int)lnIdx * lineH;
                descLn.h = lineH;
                descLn.text = wrapped[lnIdx];
                descLn.docStart = p->fullDocText.size();
                p->fullDocText += descLn.text;
                descLn.docEnd = p->fullDocText.size();
                p->fullDocText += (lnIdx + 1 < wrapped.size()) ? L" " : L"\r\n";
                ComputeVisualLineCharOffsets(hdcMeasure, useDescFont, descLn);
                p->visualLines.push_back(std::move(descLn));
            }
        }
        curY += e.h;
    }

    if (tempDC) {
        if (hdcMeasure) {
            SelectObject(hdcMeasure, hOldF);
            ReleaseDC(hWnd, hdcMeasure);
        }
        if (hTempSectionFont) DeleteObject(hTempSectionFont);
        if (hTempKeyFont)     DeleteObject(hTempKeyFont);
        if (hTempDescFont)    DeleteObject(hTempDescFont);
    }

    p->scrollContentHeight = std::max(80, curY + 12);

    DWORD dwStyle = hWnd ? (DWORD)GetWindowLongPtrW(hWnd, GWL_STYLE)
                         : (WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VSCROLL | WS_VISIBLE | WS_CLIPCHILDREN);
    DWORD dwExStyle = hWnd ? (DWORD)GetWindowLongPtrW(hWnd, GWL_EXSTYLE) : WS_EX_TOPMOST;
    RECT rcFull = { 0, 0, std::max(320, clientW), p->headerHeight + p->scrollContentHeight + p->footerHeight };
    AdjustWindowRectEx(&rcFull, dwStyle, FALSE, dwExStyle);
    p->fullWinHeight = rcFull.bottom - rcFull.top;
}

static size_t HitTestShortcutsDocPos(const ShortcutsDlgParams* p, int mx, int contentY) {
    if (!p || p->visualLines.empty()) return 0;
    if (contentY < p->visualLines.front().y) return 0;
    if (contentY > p->visualLines.back().y + p->visualLines.back().h + 8) return p->fullDocText.size();

    // Find the visible entry at contentY
    int bestEntryIdx = -1;
    int bestEntryDist = 100000000;
    for (size_t i = 0; i < p->entries.size(); ++i) {
        const auto& e = p->entries[i];
        if (!e.visible || e.h <= 0) continue;
        int dist = 0;
        if (contentY < e.y) dist = e.y - contentY;
        else if (contentY >= e.y + e.h) dist = contentY - (e.y + e.h) + 1;
        if (dist < bestEntryDist) {
            bestEntryDist = dist;
            bestEntryIdx = (int)i;
            if (dist == 0) break;
        }
    }
    if (bestEntryIdx < 0) return 0;

    // Within that entry, choose keyCol vs descCol based on mx, then closest wrapped line by contentY
    bool preferKeyCol = (mx < p->rightColX - 8);
    const ShortcutsVisualLine* bestLine = nullptr;
    int bestLineDist = 100000000;
    for (const auto& ln : p->visualLines) {
        if (ln.entryIdx != bestEntryIdx) continue;
        if (!ln.isSection && ln.isKeyCol != preferKeyCol) continue;
        int dist = 0;
        if (contentY < ln.y) dist = ln.y - contentY;
        else if (contentY >= ln.y + ln.h) dist = contentY - (ln.y + ln.h) + 1;
        if (dist < bestLineDist) {
            bestLineDist = dist;
            bestLine = &ln;
        }
    }
    if (!bestLine) {
        for (const auto& ln : p->visualLines) {
            if (ln.entryIdx == bestEntryIdx) {
                bestLine = &ln;
                break;
            }
        }
    }
    if (!bestLine) return 0;

    // Find character offset inside bestLine
    int relX = mx - bestLine->x;
    if (relX <= 0 || bestLine->text.empty()) return bestLine->docStart;
    for (size_t c = 0; c < bestLine->text.size(); ++c) {
        int leftX = bestLine->charX[c];
        int rightX = bestLine->charX[c + 1];
        int midX = (leftX + rightX) / 2;
        if (relX < midX) {
            return bestLine->docStart + c;
        }
    }
    return bestLine->docEnd;
}

static bool CopyShortcutsSelectionToClipboard(HWND hWnd, ShortcutsDlgParams* p) {
    if (!p || !p->HasSelection()) return false;
    std::wstring text = p->GetSelectedText();
    if (text.empty()) return false;
    if (!OpenClipboard(hWnd)) return false;
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
    p->copiedStatusText = L"\x2713 Copied selected text to clipboard";
    if (hWnd) InvalidateRect(hWnd, nullptr, FALSE);
    return true;
}

static void LayoutShortcutsFooter(HWND hWnd, ShortcutsDlgParams* p) {
    if (!hWnd || !p) return;
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int clientW = std::max(1, (int)(rcClient.right - rcClient.left));
    int clientH = std::max(1, (int)(rcClient.bottom - rcClient.top));
    int footerTop = std::max(p->headerHeight, clientH - p->footerHeight);

    int clearBtnX = std::max(160, clientW - 18 - 64);
    int searchEditW = std::max(120, clearBtnX - 8 - 76);
    if (p->hSearchEdit && searchEditW != p->lastSearchEditW) {
        p->lastSearchEditW = searchEditW;
        SetWindowPos(p->hSearchEdit, HWND_TOP, 76, 39, searchEditW, 26, SWP_NOACTIVATE | SWP_NOCOPYBITS);
    }
    if (p->hClearSearchBtn && clearBtnX != p->lastClearBtnX) {
        p->lastClearBtnX = clearBtnX;
        SetWindowPos(p->hClearSearchBtn, HWND_TOP, clearBtnX, 38, 64, 28, SWP_NOACTIVATE | SWP_NOCOPYBITS);
    }
    if (p->hOkBtn) {
        int btnX = std::max(16, clientW - 18 - 92);
        int btnY = footerTop + 11;
        if (btnX != p->lastOkBtnX || btnY != p->lastOkBtnY) {
            p->lastOkBtnX = btnX;
            p->lastOkBtnY = btnY;
            SetWindowPos(p->hOkBtn, HWND_TOP, btnX, btnY, 92, 30, SWP_NOACTIVATE | SWP_NOCOPYBITS);
        }
    }
}

static void UpdateShortcutsScroll(HWND hWnd, ShortcutsDlgParams* p, int newScrollY, bool redrawOnScroll = true) {
    if (!hWnd || !p) return;
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int clientW = std::max(1, (int)(rcClient.right - rcClient.left));
    int clientH = std::max(1, (int)(rcClient.bottom - rcClient.top));
    int viewportH = std::max(1, clientH - p->headerHeight - p->footerHeight);
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
    if (dy != 0 && redrawOnScroll) {
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

            if (p) {
                HDC hdc = GetDC(hWnd);
                p->InitGdiCache(hdc);
                if (hdc) ReleaseDC(hWnd, hdc);
            }

            HWND hSearch = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | ES_AUTOHSCROLL,
                76, 39, 400, 26, hWnd, (HMENU)IDC_SC_SEARCH_EDIT, nullptr, nullptr
            );
            if (p && p->hDescFont) {
                SendMessageW(hSearch, WM_SETFONT, (WPARAM)p->hDescFont, TRUE);
            }
            SendMessageW(hSearch, EM_SETCUEBANNER, TRUE, (LPARAM)L"Search shortcuts, tools, actions, or descriptions... (Ctrl + F)");

            HWND hClear = CreateWindowExW(
                0, L"BUTTON", L"Clear",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_DISABLED,
                484, 38, 64, 28, hWnd, (HMENU)IDC_SC_SEARCH_CLEAR, nullptr, nullptr
            );
            if (p && p->hDescFont) {
                SendMessageW(hClear, WM_SETFONT, (WPARAM)p->hDescFont, TRUE);
            }

            HWND hOk = CreateWindowExW(0, L"BUTTON", L"OK",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_DEFPUSHBUTTON, 400, 500, 92, 30, hWnd, (HMENU)IDOK, nullptr, nullptr);
            if (p && p->hKeyFont) {
                SendMessageW(hOk, WM_SETFONT, (WPARAM)p->hKeyFont, TRUE);
            }

            if (p) {
                p->hSearchEdit = hSearch;
                p->hClearSearchBtn = hClear;
                p->hOkBtn = hOk;
                RECT rcC;
                GetClientRect(hWnd, &rcC);
                int clientW = std::max(1, (int)(rcC.right - rcC.left));
                RecalcShortcutsLayout(hWnd, p, clientW, true);
                HDC hdc = GetDC(hWnd);
                if (hdc) {
                    p->EnsureContentRendered(hdc, clientW);
                    ReleaseDC(hWnd, hdc);
                }
                LayoutShortcutsFooter(hWnd, p);
                UpdateShortcutsScroll(hWnd, p, 0, false);
            }
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_SETCURSOR: {
            if (p && (HWND)wParam == hWnd && LOWORD(lParam) == HTCLIENT) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hWnd, &pt);
                RECT rcC;
                GetClientRect(hWnd, &rcC);
                int footerTop = std::max(p->headerHeight, (int)(rcC.bottom - rcC.top) - p->footerHeight);
                if (pt.y >= p->headerHeight && pt.y < footerTop) {
                    SetCursor(LoadCursorW(nullptr, IDC_IBEAM));
                    return TRUE;
                }
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            if (p) {
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                RECT rcC;
                GetClientRect(hWnd, &rcC);
                int footerTop = std::max(p->headerHeight, (int)(rcC.bottom - rcC.top) - p->footerHeight);
                if (my >= p->headerHeight && my < footerTop) {
                    SetFocus(hWnd);
                    p->copiedStatusText.clear();
                    int contentY = my - p->headerHeight + p->scrollY;
                    size_t pos = HitTestShortcutsDocPos(p, mx, contentY);
                    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
                        p->selCaret = pos;
                    } else {
                        p->selAnchor = pos;
                        p->selCaret = pos;
                    }
                    p->isSelectingText = true;
                    SetCapture(hWnd);
                    SetTimer(hWnd, TIMER_SC_DRAG_SCROLL, 30, nullptr);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
            }
            break;
        }
        case WM_LBUTTONDBLCLK: {
            if (p && !p->fullDocText.empty()) {
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                RECT rcC;
                GetClientRect(hWnd, &rcC);
                int footerTop = std::max(p->headerHeight, (int)(rcC.bottom - rcC.top) - p->footerHeight);
                if (my >= p->headerHeight && my < footerTop) {
                    SetFocus(hWnd);
                    int contentY = my - p->headerHeight + p->scrollY;
                    size_t pos = std::min(HitTestShortcutsDocPos(p, mx, contentY), p->fullDocText.size() - 1);
                    auto isWordChar = [](wchar_t ch) {
                        return iswalnum(ch) || ch == L'_' || ch == L'-' || ch == L'+' || ch == L'{' || ch == L'}' || ch == L'#' || ch == L'%';
                    };
                    if (isWordChar(p->fullDocText[pos])) {
                        size_t s = pos, e = pos;
                        while (s > 0 && isWordChar(p->fullDocText[s - 1])) --s;
                        while (e < p->fullDocText.size() && isWordChar(p->fullDocText[e])) ++e;
                        p->selAnchor = s;
                        p->selCaret = e;
                        InvalidateRect(hWnd, nullptr, FALSE);
                    }
                    return 0;
                }
            }
            break;
        }
        case WM_MOUSEMOVE: {
            if (p && p->isSelectingText) {
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                RECT rcC;
                GetClientRect(hWnd, &rcC);
                int footerTop = std::max(p->headerHeight, (int)(rcC.bottom - rcC.top) - p->footerHeight);
                int clampedMy = std::max(p->headerHeight, std::min(my, footerTop));
                int contentY = clampedMy - p->headerHeight + p->scrollY;
                size_t newCaret = HitTestShortcutsDocPos(p, mx, contentY);
                if (newCaret != p->selCaret) {
                    p->selCaret = newCaret;
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
            break;
        }
        case WM_LBUTTONUP: {
            if (p && p->isSelectingText) {
                p->isSelectingText = false;
                KillTimer(hWnd, TIMER_SC_DRAG_SCROLL);
                if (GetCapture() == hWnd) {
                    ReleaseCapture();
                }
                return 0;
            }
            break;
        }
        case WM_TIMER: {
            if (wParam == TIMER_SC_DRAG_SCROLL && p && p->isSelectingText) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hWnd, &pt);
                RECT rcC;
                GetClientRect(hWnd, &rcC);
                int footerTop = std::max(p->headerHeight, (int)(rcC.bottom - rcC.top) - p->footerHeight);
                int scrollDelta = 0;
                if (pt.y < p->headerHeight) {
                    scrollDelta = -std::min(48, std::max(8, (p->headerHeight - (int)pt.y) / 2));
                } else if (pt.y > footerTop) {
                    scrollDelta = std::min(48, std::max(8, ((int)pt.y - footerTop) / 2));
                }
                if (scrollDelta != 0) {
                    UpdateShortcutsScroll(hWnd, p, p->scrollY + scrollDelta, false);
                    int clampedMy = std::max(p->headerHeight, std::min((int)pt.y, footerTop));
                    p->selCaret = HitTestShortcutsDocPos(p, pt.x, clampedMy - p->headerHeight + p->scrollY);
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
            break;
        }
        case WM_CONTEXTMENU: {
            if (p) {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                if (pt.x == -1 && pt.y == -1) {
                    GetCursorPos(&pt);
                }
                POINT clientPt = pt;
                ScreenToClient(hWnd, &clientPt);
                RECT rcC;
                GetClientRect(hWnd, &rcC);
                int footerTop = std::max(p->headerHeight, (int)(rcC.bottom - rcC.top) - p->footerHeight);
                if (clientPt.y >= p->headerHeight && clientPt.y < footerTop) {
                    HMENU hMenu = CreatePopupMenu();
                    AppendMenuW(hMenu, MF_STRING | (p->HasSelection() ? MF_ENABLED : MF_GRAYED), IDM_SC_CTX_COPY, L"Copy\tCtrl+C");
                    AppendMenuW(hMenu, MF_STRING | (!p->fullDocText.empty() ? MF_ENABLED : MF_GRAYED), IDM_SC_CTX_SELALL, L"Select all\tCtrl+A");
                    int cmd = (int)TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, hWnd, nullptr);
                    DestroyMenu(hMenu);
                    if (cmd == IDM_SC_CTX_COPY) {
                        CopyShortcutsSelectionToClipboard(hWnd, p);
                    } else if (cmd == IDM_SC_CTX_SELALL) {
                        p->selAnchor = 0;
                        p->selCaret = p->fullDocText.size();
                        p->copiedStatusText.clear();
                        InvalidateRect(hWnd, nullptr, FALSE);
                    }
                    return 0;
                }
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
            if (p) {
                p->EnsureContentRendered(hdc, clientW);
                if (!p->hBackDC || clientW > p->backCapW || clientH > p->backCapH) {
                    if (!p->hBackDC) {
                        p->hBackDC = CreateCompatibleDC(hdc);
                    } else if (p->hBackOldBmp) {
                        SelectObject(p->hBackDC, p->hBackOldBmp);
                        if (p->hBackBmp) DeleteObject(p->hBackBmp);
                    }
                    p->backCapW = std::max(clientW + 128, p->maxWinWidth);
                    p->backCapH = std::max(clientH + 128, GetSystemMetrics(SM_CYSCREEN));
                    p->hBackBmp = CreateCompatibleBitmap(hdc, p->backCapW, p->backCapH);
                    p->hBackOldBmp = SelectObject(p->hBackDC, p->hBackBmp);
                }
                HDC memDC = p->hBackDC;
                SetBkMode(memDC, TRANSPARENT);

                int hdrH = p->headerHeight;
                int footerTop = std::max(hdrH, clientH - p->footerHeight);
                int viewportH = std::max(0, footerTop - hdrH);
                int sy = p->scrollY;
                int copyH = 0;
                if (p->hContentDC && p->renderedContentH > sy && viewportH > 0) {
                    copyH = std::max(0, std::min(viewportH, p->renderedContentH - sy));
                    if (copyH > 0) {
                        BitBlt(memDC, 0, hdrH, clientW, copyH, p->hContentDC, 0, sy, SRCCOPY);
                    }
                }
                if (copyH < viewportH) {
                    RECT rcRem = { 0, hdrH + copyH, clientW, footerTop };
                    FillRect(memDC, &rcRem, GetSysColorBrush(COLOR_BTNFACE));
                }

                // Render active text selection highlights inside the scrollable viewport
                if (p->HasSelection() && p->hSelHighlightBrush && viewportH > 0) {
                    size_t sMin = std::min(p->selAnchor, p->selCaret);
                    size_t sMax = std::max(p->selAnchor, p->selCaret);
                    HGDIOBJ hOldSelFont = SelectObject(memDC, p->hDescFont);
                    SetTextColor(memDC, RGB(255, 255, 255));
                    for (const auto& ln : p->visualLines) {
                        if (ln.docEnd <= sMin || ln.docStart >= sMax || ln.text.empty()) continue;
                        int screenY = hdrH + ln.y - sy;
                        if (screenY + ln.h <= hdrH || screenY >= footerTop) continue;
                        size_t hlStart = (sMin > ln.docStart) ? (sMin - ln.docStart) : 0;
                        size_t hlEnd = std::min(ln.text.size(), sMax - ln.docStart);
                        if (hlStart >= hlEnd || hlEnd >= ln.charX.size()) continue;

                        int x1 = ln.x + ln.charX[hlStart];
                        int x2 = ln.x + ln.charX[hlEnd];
                        RECT rcSel = { x1, screenY, x2, screenY + ln.h };
                        FillRect(memDC, &rcSel, p->hSelHighlightBrush);

                        if (ln.isSection) SelectObject(memDC, p->hSectionFont);
                        else if (ln.isKeyCol) SelectObject(memDC, p->hKeyFont);
                        else SelectObject(memDC, p->hDescFont);

                        std::wstring sub = ln.text.substr(hlStart, hlEnd - hlStart);
                        RECT rcSub = { x1, screenY, x2 + 8, screenY + ln.h };
                        DrawTextW(memDC, sub.c_str(), (int)sub.size(), &rcSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                    }
                    SelectObject(memDC, hOldSelFont);
                }

                // Permanent pinned top header bar (Title + Search bar)
                int padX = 18;
                int rightEdge = clientW - padX;
                RECT rcHeader = { 0, 0, clientW, hdrH };
                FillRect(memDC, &rcHeader, GetSysColorBrush(COLOR_BTNFACE));

                HGDIOBJ hOldF = SelectObject(memDC, p->hTitleFont);
                SetTextColor(memDC, GetSysColor(COLOR_WINDOWTEXT));
                RECT rcTitle = { padX, 8, rightEdge - 130, 32 };
                DrawTextW(memDC, p->mainTitle.c_str(), (int)p->mainTitle.size(), &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                if (!p->searchTokens.empty()) {
                    SelectObject(memDC, p->hDescFont);
                    SetTextColor(memDC, (p->matchRowCount > 0) ? RGB(30, 64, 175) : RGB(220, 38, 38));
                    std::wstring matchStr = std::to_wstring(p->matchRowCount) + ((p->matchRowCount == 1) ? L" match" : L" matches");
                    RECT rcMatch = { rightEdge - 140, 8, rightEdge, 32 };
                    DrawTextW(memDC, matchStr.c_str(), (int)matchStr.size(), &rcMatch, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                }

                SelectObject(memDC, p->hKeyFont);
                SetTextColor(memDC, RGB(30, 41, 59));
                RECT rcSearchLbl = { padX, 39, 74, 65 };
                DrawTextW(memDC, L"Search:", -1, &rcSearchLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                HGDIOBJ hOldP = SelectObject(memDC, p->hSepShadow);
                MoveToEx(memDC, 0, hdrH - 2, nullptr);
                LineTo(memDC, clientW, hdrH - 2);
                SelectObject(memDC, p->hSepHilight);
                MoveToEx(memDC, 0, hdrH - 1, nullptr);
                LineTo(memDC, clientW, hdrH - 1);

                // Permanent bottom footer bar + separating line + copy status hint
                RECT rcFooter = { 0, footerTop, clientW, clientH };
                FillRect(memDC, &rcFooter, GetSysColorBrush(COLOR_BTNFACE));
                SelectObject(memDC, p->hFooterLinePen);
                MoveToEx(memDC, 0, footerTop, nullptr);
                LineTo(memDC, clientW, footerTop);

                SelectObject(memDC, p->hDescFont);
                RECT rcFooterHint = { padX, footerTop + 10, std::max(padX + 80, clientW - 126), clientH - 10 };
                if (!p->copiedStatusText.empty()) {
                    SetTextColor(memDC, RGB(22, 163, 74));
                    DrawTextW(memDC, p->copiedStatusText.c_str(), -1, &rcFooterHint, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                } else {
                    SetTextColor(memDC, GetSysColor(COLOR_GRAYTEXT));
                    DrawTextW(memDC, L"Drag to select any text (or double-click word) and press Ctrl+C or right-click to copy", -1, &rcFooterHint, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                }

                SelectObject(memDC, hOldP);
                SelectObject(memDC, hOldF);

                BitBlt(hdc, 0, 0, clientW, clientH, memDC, 0, 0, SRCCOPY);
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
                int maxH = std::max(360, sh - 24);
                if (p->searchTokens.empty()) {
                    maxH = std::min(p->fullWinHeight, maxH);
                }
                mmi->ptMaxTrackSize.y = std::max(280, maxH);
            }
            return 0;
        }
        case WM_SIZE: {
            if (p && wParam != SIZE_MINIMIZED) {
                RECT rcC;
                GetClientRect(hWnd, &rcC);
                int clientW = std::max(1, (int)(rcC.right - rcC.left));
                RecalcShortcutsLayout(hWnd, p, clientW, false);
                UpdateShortcutsScroll(hWnd, p, p->scrollY, false);
                InvalidateRect(hWnd, nullptr, FALSE);
                UpdateWindow(hWnd);
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
                int viewportH = std::max(60, (int)(rcC.bottom - rcC.top) - p->headerHeight - p->footerHeight);
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
                UpdateShortcutsScroll(hWnd, p, target, true);
                return 0;
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            if (p) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (delta != 0) {
                    int step = -(delta * 48) / WHEEL_DELTA;
                    UpdateShortcutsScroll(hWnd, p, p->scrollY + step, true);
                }
                return 0;
            }
            break;
        }
        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            WORD code = HIWORD(wParam);
            if (id == IDC_SC_SEARCH_EDIT && code == EN_CHANGE && p && p->hSearchEdit) {
                WCHAR qBuf[256] = { 0 };
                GetWindowTextW(p->hSearchEdit, qBuf, 255);
                std::wstring newQ = qBuf;
                if (p->hClearSearchBtn) {
                    EnableWindow(p->hClearSearchBtn, newQ.empty() ? FALSE : TRUE);
                }
                p->UpdateSearchFilter(newQ);
                RECT rcC;
                GetClientRect(hWnd, &rcC);
                int clientW = std::max(1, (int)(rcC.right - rcC.left));
                RecalcShortcutsLayout(hWnd, p, clientW, true);
                UpdateShortcutsScroll(hWnd, p, 0, false);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (id == IDC_SC_SEARCH_CLEAR && p && p->hSearchEdit) {
                SetWindowTextW(p->hSearchEdit, L"");
                SetFocus(p->hSearchEdit);
                return 0;
            }
            if (id == IDOK || id == IDCANCEL) {
                DestroyWindow(hWnd);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;
        case WM_NCDESTROY:
            KillTimer(hWnd, TIMER_SC_DRAG_SCROLL);
            if (p) {
                p->FreeGdiCache();
            }
            break;
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

    std::wstring curSectionTitle;
    auto addSection = [&](const std::wstring& title) {
        curSectionTitle = title;
        ShortcutEntry e;
        e.isSectionHeader = true;
        e.sectionTitle = title;
        e.keyCol = title;
        params.entries.push_back(e);
    };
    auto addRow = [&](const std::wstring& key, const std::wstring& desc) {
        ShortcutEntry e;
        e.isSectionHeader = false;
        e.hasIcon = false;
        e.sectionTitle = curSectionTitle;
        e.keyCol = key;
        e.descCol = desc;
        params.entries.push_back(e);
    };
    auto addToolRow = [&](OverlayTool tool, const std::wstring& key, const std::wstring& desc) {
        ShortcutEntry e;
        e.isSectionHeader = false;
        e.hasIcon = true;
        e.iconBtn.isTool = true;
        e.iconBtn.tool = tool;
        e.sectionTitle = curSectionTitle;
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
        e.sectionTitle = curSectionTitle;
        e.keyCol = key;
        e.descCol = desc;
        params.entries.push_back(e);
    };

    addSection(L"Global capture hotkeys & system integration");
    addRow(FormatLabelWithShortcut(L"Custom area", hkRegionSnip),                       L"Start custom area snip & annotate (customizable in Customize keys)");
    addRow(FormatLabelWithShortcut(L"Instant fullscreen", hkFullSnap),                  L"Capture & save all monitors immediately (customizable in Customize keys)");
    addRow(FormatLabelWithShortcut(L"Instant save previous custom area", hkPrevRegion), L"Capture & save previous custom area — preserves rectangle, irregular quad / triangle, or circle / ellipse mask (customizable in Customize keys)");
    addRow(FormatLabelWithShortcut(L"Pin the image in clipboard on top", hkPinClipboard), L"Pin the image currently in the clipboard directly on top as a floating window (also in tray menu; customizable in Customize keys)");
    addRow(FormatLabelWithShortcut(L"Edit the image in clipboard", hkEditClipboard),       L"Open the image currently in the clipboard directly in the PepperSnap editor on a checkerboard background (also in tray menu; customizable in Customize keys)");
    addRow(FormatLabelWithShortcut(L"Search on web the image in clipboard", hkSearchGoogle), L"Open the default browser at https://lens.google.com/, then press Ctrl + V there to paste the image from the clipboard (also in tray menu; customizable in Customize keys)");
    addRow(L"Left-click system tray icon",   L"Start custom area capture");
    addRow(L"Right-click system tray icon",  L"Open tray menu (Pin the image in clipboard on top, Edit the image in clipboard, Search on web the image in clipboard, Open image to edit / pin (supports multi-select), Smooth / Unsmooth all pinned image, Show / Hide toolbar on all pinned image, Show / Hide outline on all pinned image, Close all pinned image, Options, Keyboard shortcuts and info, etc.)");
    addRow(L"Launch PepperSnap at Windows startup", L"Tray menu toggle that starts PepperSnap in the system tray when you log on. It is an ordinary per-user startup entry, so if \"Run as administrator\" is also on, logging on shows the usual UAC prompt; turn either setting off to avoid it");
    addRow(L"Right-click image in Explorer", L"\"Edit with PepperSnap\" (single image on a checkerboard transparency background; shows selectable bottom frame list for multi-frame .gif / .webp; when \"Put pinned image above edit window\" is enabled in Options, pinned images overlay the edit window and any pinned image in the custom area is included when saving/copying), \"Search image on web\" (copies image to clipboard and opens https://lens.google.com/, where Ctrl + V pastes it), or \"Pin on top\" (single or multi-selected images)");
    addRow(L"Multi-frame GIF / WebP editor", L"When editing a multi-frame .gif or .webp, a horizontally scrollable & vertically resizable frame list appears at the bottom (on top of custom area & toolbar, auto-hides when focusing custom area/toolbar if \"Auto-hide frame list\" is enabled in Options, with a persistent dropdown button & Play / Pause icon on the right side)");
    addActionRow(DBTN_ACT_FRAME_PLAY_PAUSE, false, FormatLabelWithShortcut(L"Play / Pause frames (GIF/WebP)", hkFramePlayPause),   L"Play or pause multi-frame .gif / .webp animation in \"Edit with PepperSnap\" (or click the Play / Pause icon on the right side of the Hide / Show frames button)");
    addActionRow(DBTN_ACT_FRAME_TOGGLE,     false, FormatLabelWithShortcut(L"Hide / Show frames (GIF/WebP)",  hkFrameToggleStrip), L"Collapse or expand the bottom frame list in \"Edit with PepperSnap\" (or click the Hide / Show frames dropdown button)");
    addRow(L"Search & copy in this info window", L"Use the top Search bar (or press Ctrl + F) to filter shortcuts & info in real time, and drag to select any text (or double-click a word) and press Ctrl + C or right-click to copy; drag any window edge or corner to resize");

    addSection(L"Options window & sub-option windows");
    addRow(L"Save screenshot to... / Save format", L"Choose default save folder (Browse... or Use desktop), independent formats ordered as Copy format (default: PNG), Custom area format (default: PNG), and Instant fullscreen format (default: JPEG) across JPEG, PNG, WebP, and BMP, plus JPEG quality slider (10%–100%)");
    addRow(L"Naming pattern & (\x24D8) syntax window", L"Customize screenshot filename pattern with live preview, Restore default, or click \x24D8 to open the Naming pattern syntax sub-option window ({YYYY}, {MMMM}, {DDDD}, {h}, {ap}, {N}, etc. — click any row or select text to copy)");
    addRow(L"Shortcut keys \x2192 Customize keys & info", L"Click Customize keys to open the sub-option window and rebind any global, custom area tool/action, multi-frame editor, or pinned image shortcut (Esc to cancel, Delete to leave it empty, or Restore defaults), or click Keyboard shortcuts and info");
    addRow(L"Automatically check for updates", L"Toggle automatic GitHub release checks (Every day, 3-day, Every week, 2-weekly, Every month) or click Check now");
    addRow(L"Non-stacking highlighter",      L"Prevent overlapping Stabilo highlighter strokes from darkening when drawing over existing highlights (on by default)");
    addRow(L"Auto-save & auto-copy toggles", L"Toggle \"Also save screenshot when copying custom area\", \"Also copy instant fullscreen\", \"Also save pinned screenshot\", and \"Also save copied text\" (OCR .txt)");
    addRow(L"Keep cursor & Auto-hide frames", L"Toggle \"Keep cursor on screenshot\" (captures mouse cursor in screenshots) and \"Auto-hide frame list\" (auto-collapses GIF/WebP frame list when focusing custom area/toolbar)");
    addRow(L"Put pinned image above edit window", L"Checkbox in Options (on by default) that keeps pinned images above the \"Edit with PepperSnap\" window so any pinned image inside the custom area is included when saving or copying");
    addRow(L"Keep edit window open after save and copy", L"Checkbox in Options that keeps the \"Edit with PepperSnap\" window open after Quick save, Save as, or Copy to clipboard instead of closing");
    addRow(L"Run as administrator (admin permission)", L"Checkbox in Options (on by default) that runs PepperSnap with admin permission, so shortcuts and capture keep working while an app running as administrator has focus; without it a shortcut pressed over such a window silently does nothing. Windows asks for that permission with its usual UAC prompt each time PepperSnap starts, because PepperSnap installs nothing and registers no scheduled task or service to avoid it. If the prompt is declined, or the account has no admin permission, PepperSnap simply runs without it. With PepperSnap already running, starting it again - or using its right-click entries - needs no new permission and shows no prompt. Turning this off never runs the app elevated");
    addRow(L"Disable right-click integration", L"Checkbox in Options that removes the PepperSnap entries (\"Edit with PepperSnap\", \"Search image on web\", \"Pin on top\") from the image right-click menu in Explorer");
    addRow(L"Disable magnifier window", L"Checkbox in Options (off by default) that disables the floating loupe magnification window when aiming, selecting, or resizing custom areas");
    addRow(L"Enable pen & highlighter smoothing", L"Toggle freehand stroke smoothing and adjust smoothing strength slider (5%–100%: weak, balanced, strong, ultra smooth)");
    addRow(L"Restore everything to default", L"Reset all Options settings, toggles, naming pattern, and shortcut keys back to factory defaults (with confirmation)");
    addRow(L"\x24D8 settings.ini in %appdata%\\PepperSnap", L"Click the link in the Options footer to open the %appdata%\\PepperSnap folder in Explorer where settings.ini is persisted");

    addSection(L"Custom area — window selection, aiming & size input");
    addRow(L"Hold Shift and click (before selecting)",  L"Override pixel-grid snapping (with 1px crosshair gap), copy pixel RGB hex code (#RRGGBB) under magnifier & close (holding Shift before selecting shows \"Scroll to control magnification\" and \"Arrow keys to jump pixel-by-pixel\" in the magnifier window)");
    addRow(L"Hold Shift then scroll (before selecting)", L"Scroll to control magnification (default 5×, minimum 100% / 1×, up to maximum zoom)");
    addRow(L"Arrow keys (before or while dragging)",     L"Jump pixel-by-pixel up, down, left, or right before selecting custom area or while holding left-click to drag/resize selection");
    addRow(L"Hold Ctrl and click (before selecting)",   L"Auto-detect, highlight & select any window, notification pop-up, or sidebar (or press Enter to capture window)");
    addRow(L"Hold Ctrl (after selecting)",   L"Temporarily switch active tool to Select mode while Ctrl is held");
    addRow(L"Hold Alt and drag (when selecting)",          L"Create a circle / ellipse custom area instead of a rectangle (saved, copied & pinned with anti-aliased transparent corners)");
    addRow(L"Hold Alt + Shift + drag (when selecting)",  L"Create a 1:1 circle custom area");
    addRow(L"Hold Shift (after anchor point placed)", L"Constrain selection or resize to 1:1 square ratio (or 1:1 circle when holding Alt) after the anchor point is placed in custom size mode (releasing Shift returns to free ratio; magnifier sticks to the opposite point of the current anchor point)");
    addRow(L"Left-click + Right-click",      L"Constrain selection or resize to 1:1 square ratio (releasing right-click returns to free ratio; magnifier sticks to the opposite point of the current anchor point)");
    addRow(L"1 + Left-click",                L"Constrain selection or resize to 16:9 ratio (releasing 1 returns to free ratio; magnifier sticks to the opposite point of the current anchor point)");
    addRow(L"2 + Left-click",                L"Constrain selection or resize to 9:16 ratio (releasing 2 returns to free ratio; magnifier sticks to the opposite point of the current anchor point)");
    addRow(L"3 + Left-click",                L"Constrain selection or resize to 1:2 ratio (releasing 3 returns to free ratio; magnifier sticks to the opposite point of the current anchor point)");
    addRow(L"4 + Left-click",                L"Constrain selection or resize to 2:1 ratio (releasing 4 returns to free ratio; magnifier sticks to the opposite point of the current anchor point)");
    addRow(L"Drag 8 resize handles",         L"Resize existing custom area (rectangle, irregular quad / triangle bounding box, or circle / ellipse) with a 1:1 square magnification window at the active resize point showing \"Press arrow keys to jump pixel-by-pixel while holding left-click\"");
    addRow(L"Ctrl + Left-click corner point", L"Move any corner point individually to reshape the capture rectangle into an irregular quad, or snap a corner point onto another corner point to create a triangle without distorting the image");
    addRow(L"Click [W × H px | ratio] indicator", L"Dynamic pill permanently shows [W×H px | ratio] (whole-integer ratio) — click W×H px to type custom W×H, width only (200 or 200x), or height only (xx200 or x200); after placing a custom area, click ratio to type a custom ratio (e.g. 16:9, ratio only like 4 or 4:, or height ratio like ::3 or :3), which turns the ratio indicator red (#EF4444) and locks the aspect ratio until you click the Reset ratio trash icon on the right side");
    addActionRow(DBTN_ACT_CLEAR, false, L"Reset ratio (beside custom ratio)", L"Appears on the right side of the ratio indicator ([W×H px | ratio [trash icon]]) after inputting a custom ratio — click to reset (unlink) the custom ratio while keeping the current custom area size");

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
    addToolRow(OverlayTool::TextBox,        FormatLabelWithShortcut(L"Text box",            hkToolText),      L"In-place text in Calibri Light, with a glyph outline chosen for contrast so it reads over any background (Enter commits, Shift + Enter new line, Esc exits)");
    addToolRow(OverlayTool::MosaicSquare,   FormatLabelWithShortcut(L"Mosaic square",       hkToolMosaicSq),  L"Pixelate rectangular area (hold Shift for 1:1 square)");
    addToolRow(OverlayTool::MosaicCircle,   FormatLabelWithShortcut(L"Mosaic circle",       hkToolMosaicCir), L"Pixelate elliptical area (hold Shift for 1:1 circle)");

    addSection(L"Custom area — Row 2: colors & stroke sizes");
    addRow(L"5 color swatches",              L"Set active tool color or recolor selected annotation(s)");
    addRow(L"S / M / L / XL buttons",        L"Preset stroke / text / mosaic sizes (2px, 4px, 8px, 14px)");
    addRow(L"Click custom size box (px)",    L"Type exact size (1–120 px) right of XL, then press Enter");
    addRow(L"Mouse scroll wheel",            L"Adjust active or selected annotation(s) stroke size (1–120 px)");
    addRow(L"Delete / Backspace",            L"Delete currently selected annotation(s) (in Select mode)");

    addSection(L"Custom area — Row 3: toolbar actions & export shortcuts");
    addRow(FormatLabelWithShortcut(L"Undo", hkActUndo), L"Undo the last annotation action. Keyboard only - the row has no Undo button any more, so this shortcut (Ctrl + Z by default) is the way to it");
    addRow(FormatLabelWithShortcut(L"Redo", hkActRedo), L"Redo an undone annotation action. Keyboard only, like Undo (Ctrl + Y by default)");
    addActionRow(DBTN_ACT_DRAG_HUD, false,  L"Drag toolbar handle",                                                     L"Drag 4-way arrow button to move toolbar (resets when selection moves)");
    addActionRow(DBTN_ACT_CLEAR,    false,  FormatLabelWithShortcut(L"Clear all annotations",     hkActClear),          L"Remove all annotations from current custom area");
    addActionRow(DBTN_ACT_TRANSLATE, false, L"Translate on web",                                                        L"Copy custom area to clipboard and open the default browser at https://translate.google.com/ in its image-translation view, translating into the language chosen under Options; press Ctrl + V there to paste the image");
    addActionRow(DBTN_ACT_SEARCH_GOOGLE, false, FormatLabelWithShortcut(L"Search on web",          hkActSearchGoogle),   L"Copy custom area to clipboard and open the default browser at https://lens.google.com/; press Ctrl + V there to paste the image (customizable in Customize keys)");
    addActionRow(DBTN_ACT_PIN,      false,  FormatLabelWithShortcut(L"Pin on top",                hkActPin),            L"Pin custom area capture as a floating topmost desktop window (preserves rectangle, irregular quad / triangle, or circle / ellipse transparency)");
    addActionRow(DBTN_ACT_OCR,      false,  FormatLabelWithShortcut(L"Extract text with OCR",     hkActOcr),            L"Recognize text in custom area via Windows OCR & copy to clipboard");
    addActionRow(DBTN_ACT_SAVE_AS,  false,  FormatLabelWithShortcut(L"Save as JPG/PNG/WEBP/BMP",  hkActSaveAs),         L"Open Save As dialog for custom area");
    addActionRow(DBTN_ACT_SAVE,     false,  FormatLabelWithShortcut(L"Quick save",                hkActSave),           L"Save capture directly to default folder & close");
    addActionRow(DBTN_ACT_COPY,     true,   FormatLabelWithShortcut(L"Copy to clipboard",         hkActCopy),           L"Copy capture to clipboard & close");
    addActionRow(DBTN_ACT_CLOSE,    false,  L"Close / Select mode (Esc)",                                               L"Switch active drawing tool to Select mode, or exit custom area");

    addSection(L"Pin on top — floating window toolbar & mechanisms");
    addRow(L"You can pin multiple image at once", L"Pin multiple captures, multi-select images in \"Open image to pin on top...\", use \"Pin the image in clipboard on top\" (" + FormatHotkeyString(hkPinClipboard) + L" or tray menu), or multi-select images in Explorer and click \"Pin on top\" (auto-tiled so they don't overlap, fit-to-screen capped at 40% of screen size, and multi-selected by default so you can immediately move them all together)");
    addRow(L"Hold Shift + any action / key / button", L"Apply any pinned image button, mouse action, size/percentage input, or shortcut key (including Shift + Save As) to all pinned images at once (e.g. Shift + Quick save saves all pinned images; Shift + click Copy button copies all of them to the clipboard; Shift + double right-click closes all)");
    addRow(L"Animated GIF & WebP support",   L"Pinned .gif and animated .webp images play continuously; .png, .webp & .gif transparency is preserved over the desktop");
    addRow(L"Ctrl + Left-click pinned image", L"Select or deselect pinned images for multi-selection (shows a blue outline on selected pinned images; pinning multiple images at once multi-selects them by default; drag any selected pinned image to move all selected pinned images together with group screen-edge snapping; left-click without Ctrl to clear selection)");
    addRow(L"Alt + Click / Alt + Shift + Click", L"Pause or play pinned animation on current frame (only shown & active on animated .gif / multi-frame animated .webp; hold Alt + Shift + click to pause/play all pinned animations)");
    addActionRow(DBTN_ACT_PIN_SEARCH_GOOGLE, false, FormatLabelWithShortcut(L"Search on web",      hkPinSearchGoogle),  L"Leftmost button of the [W\x00D7H px | %] row: copy pinned image to clipboard and open the default browser at https://lens.google.com/; press Ctrl + V there to paste the image (customizable in Customize keys)");
    addRow(L"Click W × H px or % indicator", L"Second toolbar row shows editable [W×H px | %] — click to type W×H, width only (200 or 200x), height only (xx200 or x200), or zoom % (keeps aspect ratio; Shift + Enter applies to all pinned images)");
    addActionRow(DBTN_ACT_SCALE_RESET_100, false, L"Set to 100% (beside % indicator)", L"On the right side of the % scale indicator ([W×H px | % [reset icon]]) — click to set scale to 100% (hold Shift + click to set all pinned images to 100%)");
    addRow(L"Hover over pinned image (500ms)", L"Pop-up shows \"Double-click to hide/show toolbar\", \"Double right-click to close\", \"Scroll to resize\", \"Middle-click to reset size\", \"Ctrl + middle-click to reset position\", \"Ctrl + click to multi-select (drag together)\", \"Alt + click to pause/play\" (on animations only), and Shift multi-pin hint");
    addRow(L"Drag pinned image",             L"Move floating window (or all selected pinned images together; temporarily hides toolbar while dragging, snaps to screen edges; image can overflow screen)");
    addRow(L"Hold Shift + drag",             L"Temporarily disable screen-edge snapping while moving");
    addRow(L"Double-click pinned image",     L"Hide or show the pin-on-top toolbar (hold Shift to apply to all pinned images; status persists; default: shown)");
    addRow(L"Double right-click pinned image", L"Close pinned image (hold Shift + double right-click to close all pinned images)");
    addRow(L"Scroll / Shift + Scroll (smart anchor)", L"Zoom in or out (hits vertical & horizontal screen-fit resolutions; hold Shift + scroll to zoom all pinned images): when not snapped to any screen edge, uses the cursor position as the anchor point; when snapped to a corner, anchors that corner (bottom-right resizes upward & leftward, bottom-left upward & rightward, top-left downward & rightward, top-right downward & leftward); when snapped to one side, anchors the center of that side (left side resizes upward, rightward & downward; right side upward, leftward & downward; top side leftward, downward & rightward; bottom side leftward, upward & rightward)");
    addRow(L"Middle-click pinned image",     L"Reset pinned image to its original size when first pinned on click release (hold Shift + middle-click to reset all pinned images)");
    addRow(L"Ctrl + Middle-click pinned image", L"Reset pinned image to its original coordinate and size when first pinned (hold Ctrl + Shift + middle-click to reset coordinate and size of all pinned images)");
    addRow(FormatLabelWithShortcut(L"Zoom out",                   hkPinZoomOut),     L"Scale pinned image down via shortcut key or scroll down (hold Shift to apply to all pinned images)");
    addRow(FormatLabelWithShortcut(L"Zoom in",                    hkPinZoomIn),      L"Scale pinned image up via shortcut key or scroll up (hold Shift to apply to all pinned images)");
    addRow(FormatLabelWithShortcut(L"Reset to original size",     hkPinZoomReset),   L"Restore original size when first pinned via shortcut key or middle-click (hold Shift to apply to all pinned images)");
    addRow(FormatLabelWithShortcut(L"Reset to original position", hkPinPosReset),    L"Restore original coordinate and size when first pinned via shortcut key or Ctrl + middle-click (hold Shift to apply to all pinned images)");
    addRow(FormatLabelWithShortcut(L"Hide / Show toolbar",        hkPinHideToolbar), L"Hide or show floating toolbar on pinned image via shortcut key or double-click (hold Shift to apply to all pinned images)");
    addRow(L"Smooth / Unsmooth all pinned image", L"Tray menu button that cycles smoothing or unsmoothing the image filter across all pinned images (keeps tray menu open)");
    addRow(L"Show / Hide toolbar on all pinned image", L"Tray menu button that cycles showing or hiding the toolbar across all pinned images (keeps tray menu open)");
    addRow(L"Show / Hide outline on all pinned image", L"Tray menu button that cycles showing or hiding the red outline across all pinned images (keeps tray menu open)");
    addRow(L"Smart toolbar positioning",     L"Toolbar moves above or inside image when out of space and stays above taskbar");
    addActionRow(DBTN_ACT_PIN_HIDE_IMAGE,   false, L"Hide pinned image", L"Hide pinned image and toolbar, replaced with [eye | drag] buttons in place to show or drag (hold Shift to apply to all pinned images)");
    addActionRow(DBTN_ACT_PIN_OUTLINE,      false, FormatLabelWithShortcut(L"Show / Hide outline",      hkPinOutline),     L"Toggle red border outline around pinned image (hold Shift to apply to all pinned images; default: on)");
    addActionRow(DBTN_ACT_PIN_UNFILTER,     false, FormatLabelWithShortcut(L"Smooth the image",         hkPinSmooth),      L"Smooth image with filter when on; unfiltered (pixelated) when off (hold Shift to apply to all pinned images; default: off)");
    addActionRow(DBTN_ACT_SAVE_AS,      false, FormatLabelWithShortcut(L"Save as JPG/PNG/WEBP/BMP", hkPinSaveAs),    L"Open Save As dialog for pinned image (hold Shift to Save As all pinned images; customizable in Customize keys)");
    addActionRow(DBTN_ACT_SAVE,         false, FormatLabelWithShortcut(L"Quick save",               hkPinSave),      L"Save pinned image directly to default folder (hold Shift to quick-save all pinned images; customizable in Customize keys)");
    addActionRow(DBTN_ACT_COPY,         true,  FormatLabelWithShortcut(L"Copy to clipboard",        hkPinCopy),          L"Copy pinned image to clipboard (hold Shift + click to copy every pinned image at once)");
    addActionRow(DBTN_ACT_CLOSE,        false, FormatLabelWithShortcut(L"Close pinned image",       hkPinClose),         L"Close pinned image (hold Shift to close all pinned images, or double right-click, or use tray menu)");

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
                int iconExtra = e.hasIcon ? 34 : 16;
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

    RecalcShortcutsLayout(nullptr, &params, initClientW, true);

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

    std::wstring scWinTitle = L"PepperSnap v" + std::wstring(APP_VERSION) + L" — keyboard shortcuts and info";
    if (hOptionsWnd && IsWindow(hOptionsWnd)) {
        EnableWindow(hOptionsWnd, FALSE);
    }

    HWND hDlg = CreateWindowExW(
        dwExStyle, L"PepperSnapShortcutsModal",
        scWinTitle.c_str(),
        dwStyle,
        std::max(10, (sw - initW) / 2), std::max(10, (sh - initH) / 2), initW, initH, hOwner, nullptr, hInst, &params
    );
    hShortcutsWnd = hDlg;
    BringWindowToTop(hDlg);
    SetForegroundWindow(hDlg);
    MSG msg;
    while (IsWindow(hDlg) && PepperSnapDaemon::GetMessageKeepQuit(&msg)) {
        if (msg.message == WM_MOUSEWHEEL && (msg.hwnd == hDlg || IsChild(hDlg, msg.hwnd))) {
            SendMessageW(hDlg, WM_MOUSEWHEEL, msg.wParam, msg.lParam);
            continue;
        }
        if ((msg.hwnd == hDlg || IsChild(hDlg, msg.hwnd)) && msg.message == WM_KEYDOWN) {
            bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (ctrlDown && msg.wParam == 'F' && params.hSearchEdit) {
                SetFocus(params.hSearchEdit);
                SendMessageW(params.hSearchEdit, EM_SETSEL, 0, -1);
                continue;
            }
            if (ctrlDown && msg.wParam == 'C') {
                if (params.HasSelection()) {
                    CopyShortcutsSelectionToClipboard(hDlg, &params);
                    continue;
                }
            }
            if (ctrlDown && msg.wParam == 'A' && msg.hwnd != params.hSearchEdit) {
                params.selAnchor = 0;
                params.selCaret = params.fullDocText.size();
                params.copiedStatusText.clear();
                InvalidateRect(hDlg, nullptr, FALSE);
                continue;
            }
            if (msg.wParam == VK_ESCAPE) {
                if (params.HasSelection()) {
                    params.selAnchor = params.selCaret = 0;
                    InvalidateRect(hDlg, nullptr, FALSE);
                    continue;
                }
                if (!params.searchQuery.empty() && params.hSearchEdit) {
                    SetWindowTextW(params.hSearchEdit, L"");
                    continue;
                }
                if (!hOverlayWnd) {
                    DestroyWindow(hDlg);
                    continue;
                }
            }
            if (msg.wParam == VK_RETURN && !hOverlayWnd && msg.hwnd != params.hSearchEdit) {
                DestroyWindow(hDlg);
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    hShortcutsWnd = nullptr;
    if (hOptionsWnd && IsWindow(hOptionsWnd)) {
        EnableWindow(hOptionsWnd, TRUE);
        BringWindowToTop(hOptionsWnd);
        SetForegroundWindow(hOptionsWnd);
        SetActiveWindow(hOptionsWnd);
    }
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

