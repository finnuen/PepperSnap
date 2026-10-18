#include "PepperSnapInternal.h"

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

    if (hOverlayWnd && IsWindow(hOverlayWnd)) {
        UpdateWindow(hOverlayWnd);
        GdiFlush();
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

static const GUID GUID_ContainerFormatWebp_Local = 
    { 0xe0eeb8f9, 0xd2d6, 0x40c1, { 0xbf, 0xc4, 0x65, 0x45, 0xf6, 0x80, 0xc4, 0x63 } };
static const CLSID CLSID_WICImagingFactory1_Local =
    { 0xcacaf262, 0x9370, 0x4615, { 0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a } };
static const CLSID CLSID_WICImagingFactory2_Local =
    { 0x317d06e8, 0x5f24, 0x433d, { 0xbd, 0xf7, 0x79, 0xce, 0x68, 0xd8, 0xab, 0xc2 } };
static const IID IID_IWICImagingFactory_Local =
    { 0xec5ec8a9, 0xc395, 0x4314, { 0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70 } };
static const GUID GUID_WICPixelFormat32bppBGRA_Local =
    { 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0f } };

bool PepperSnapDaemon::SaveBitmapAsWebPViaWIC(Bitmap* bmp, const std::wstring& path, int quality) {
    if (!bmp) return false;
    UINT w = bmp->GetWidth();
    UINT h = bmp->GetHeight();
    if (w == 0 || h == 0 || w > 16383 || h > 16383) return false;

    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needCoUninit = SUCCEEDED(hrCo);

    IWICImagingFactory* pFactory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory2_Local, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory_Local, (void**)&pFactory)) || !pFactory) {
        CoCreateInstance(CLSID_WICImagingFactory1_Local, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory_Local, (void**)&pFactory);
    }
    if (!pFactory) {
        if (needCoUninit) CoUninitialize();
        return false;
    }

    IWICStream* pStream = nullptr;
    IWICBitmapEncoder* pEncoder = nullptr;
    IWICBitmapFrameEncode* pFrame = nullptr;
    IPropertyBag2* pPropertyBag = nullptr;
    bool success = false;

    do {
        if (FAILED(pFactory->CreateStream(&pStream)) || !pStream) break;
        if (FAILED(pStream->InitializeFromFilename(path.c_str(), GENERIC_WRITE))) break;

        if (FAILED(pFactory->CreateEncoder(GUID_ContainerFormatWebp_Local, nullptr, &pEncoder)) || !pEncoder) break;
        if (FAILED(pEncoder->Initialize(pStream, WICBitmapEncoderNoCache))) break;

        if (FAILED(pEncoder->CreateNewFrame(&pFrame, &pPropertyBag)) || !pFrame) break;

        if (pPropertyBag) {
            PROPBAG2 option = { 0 };
            option.pstrName = (LPOLESTR)L"ImageQuality";
            VARIANT varValue;
            VariantInit(&varValue);
            varValue.vt = VT_R4;
            float qVal = (float)std::max(1, std::min(100, quality)) / 100.0f;
            varValue.fltVal = qVal;
            pPropertyBag->Write(1, &option, &varValue);
        }

        if (FAILED(pFrame->Initialize(pPropertyBag))) break;
        if (FAILED(pFrame->SetSize(w, h))) break;

        WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppBGRA_Local;
        if (FAILED(pFrame->SetPixelFormat(&pixelFormat))) break;

        Rect lockRc(0, 0, (INT)w, (INT)h);
        BitmapData bd;
        if (bmp->LockBits(&lockRc, ImageLockModeRead, PixelFormat32bppARGB, &bd) != Ok) break;

        UINT cbStride = (UINT)std::abs(bd.Stride);
        UINT cbBufferSize = cbStride * h;
        HRESULT hrWrite = pFrame->WritePixels(h, cbStride, cbBufferSize, (BYTE*)bd.Scan0);
        bmp->UnlockBits(&bd);

        if (FAILED(hrWrite)) break;

        if (FAILED(pFrame->Commit())) break;
        if (FAILED(pEncoder->Commit())) break;

        success = true;
    } while (false);

    if (pPropertyBag) pPropertyBag->Release();
    if (pFrame) pFrame->Release();
    if (pEncoder) pEncoder->Release();
    if (pStream) pStream->Release();
    pFactory->Release();

    if (needCoUninit) {
        CoUninitialize();
    }

    if (!success) {
        DeleteFileW(path.c_str());
    }

    return success;
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

// Strip all metadata segments (EXIF, XMP, ICC Profile, IPTC, Photoshop, Comments) from JPEG stream
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

// Strip all ancillary metadata chunks (tEXt, zTXt, iTXt, eXIf, tIME, pHYs, iCCP, sRGB, gAMA, cHRM) from PNG stream
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

// Pure metadata-free 24-bit BMP writer (54-byte header + BGR pixels only, zero DPI/profile metadata)
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

    // Do NOT save any metadata into the file at all (for WebP, BMP, JPEG, and PNG)
    if (ext == L".webp") {
        if (SaveBitmapAsWebPViaWIC(bmp, path, jpegQuality)) {
            return true;
        }
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
    bool hasTransparency = false;
    for (int y = 0; y < bh; ++y) {
        const BYTE* srcRow = (const BYTE*)bd.Scan0 + (size_t)(bh - 1 - y) * bd.Stride;
        memcpy(dstBase + (size_t)y * rowBytes, srcRow, rowBytes);
        if (!hasTransparency) {
            for (int x = 0; x < bw; ++x) {
                if (srcRow[x * 4 + 3] < 255) {
                    hasTransparency = true;
                    break;
                }
            }
        }
    }

    GlobalUnlock(hDIB);
    bmp->UnlockBits(&bd);

    HGLOBAL hPngClip = nullptr;
    UINT cfPng = RegisterClipboardFormatW(L"PNG");
    if ((hasTransparency || copyFormat == ImageFormat::PNG) && cfPng) {
        CLSID pngClsid;
        if (GetGdiPlusEncoderClsid(L"image/png", &pngClsid)) {
            IStream* memStream = nullptr;
            if (SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &memStream)) && memStream) {
                if (bmp->Save(memStream, &pngClsid, nullptr) == Ok) {
                    HGLOBAL hStreamMem = nullptr;
                    if (SUCCEEDED(GetHGlobalFromStream(memStream, &hStreamMem)) && hStreamMem) {
                        SIZE_T pngSz = GlobalSize(hStreamMem);
                        const void* pPngSrc = GlobalLock(hStreamMem);
                        if (pPngSrc && pngSz > 0) {
                            hPngClip = GlobalAlloc(GMEM_MOVEABLE, pngSz);
                            if (hPngClip) {
                                void* pPngDst = GlobalLock(hPngClip);
                                if (pPngDst) {
                                    memcpy(pPngDst, pPngSrc, pngSz);
                                    GlobalUnlock(hPngClip);
                                } else {
                                    GlobalFree(hPngClip);
                                    hPngClip = nullptr;
                                }
                            }
                        }
                        if (pPngSrc) GlobalUnlock(hStreamMem);
                    }
                }
                memStream->Release();
            }
        }
    }

    if (!OpenClipboard(hTrayWnd)) {
        if (hPngClip) GlobalFree(hPngClip);
        GlobalFree(hDIB);
        return false;
    }
    EmptyClipboard();
    if (hPngClip && cfPng) {
        SetClipboardData(cfPng, hPngClip);
    }
    SetClipboardData(CF_DIB, hDIB);
    CloseClipboard();
    return true;
}

bool IsWindowsClipboardHistoryEnabled() {
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

std::wstring BuildUniqueSaveFilePath(const std::wstring& folder, const std::wstring& stem, const std::wstring& ext, int batchIdx, int batchTotal) {
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
    if (hOverlayWnd && IsWindow(hOverlayWnd)) {
        if (!IsEditingOpenedImage()) {
            CloseRegionSnipOverlay();
        }
        if (hOverlayWnd && IsWindow(hOverlayWnd)) {
            KillTimer(hOverlayWnd, 4);
            showingCustomAreaHoverTip = false;
            CommitActiveTextBox();
            CommitActiveSizeInput();
            CommitActiveRatioInput();
            CommitActiveStrokeInput();
            InvalidateRect(hOverlayWnd, nullptr, FALSE);
            UpdateWindow(hOverlayWnd);
            GdiFlush();
        }
    }

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
    if (!hasSelection || IsEditingOpenedImage()) return;
    int sx0 = std::max(0, (int)std::min(selRect.left, selRect.right));
    int sy0 = std::max(0, (int)std::min(selRect.top, selRect.bottom));
    int sx1 = std::min(vScreenW, (int)std::max(selRect.left, selRect.right));
    int sy1 = std::min(vScreenH, (int)std::max(selRect.top, selRect.bottom));
    if (sx1 - sx0 >= 1 && sy1 - sy0 >= 1) {
        hasLastCustomSel = true;
        lastCustomSelRect = { sx0, sy0, sx1, sy1 };
        lastCustomSelHasQuad = hasCustomQuad;
        lastCustomSelHasEllipse = hasEllipseSelection;
        GetSelectionQuadPoints(lastCustomSelQuad);
        SaveSettings();
    }
}

void PepperSnapDaemon::InstantPreviousRegionCapture() {
    if (hOverlayWnd && IsWindow(hOverlayWnd)) {
        if (!IsEditingOpenedImage()) {
            RecordLastCustomSelection();
            CloseRegionSnipOverlay();
        }
        if (hOverlayWnd && IsWindow(hOverlayWnd)) {
            CommitActiveTextBox();
            CommitActiveSizeInput();
            CommitActiveRatioInput();
            CommitActiveStrokeInput();
            InvalidateRect(hOverlayWnd, nullptr, FALSE);
            UpdateWindow(hOverlayWnd);
            GdiFlush();
        }
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
    if (lastCustomSelHasEllipse) {
        ApplyEllipseMaskToBitmap(cropped);
    } else if (lastCustomSelHasQuad) {
        ApplyQuadPolygonMaskToBitmap(cropped, lastCustomSelQuad, rx, ry);
    }

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

