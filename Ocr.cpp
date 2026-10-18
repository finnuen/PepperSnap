#include "PepperSnapInternal.h"

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
            // Allocate Windows.Storage.Streams.Buffer and populate with raw BGRA8 pixels
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

            // Instantiate Windows.Graphics.Imaging.SoftwareBitmap from IBuffer (BitmapPixelFormat::Bgra8 = 87)
            if (!activateFactory(L"Windows.Graphics.Imaging.SoftwareBitmap", IID_ISoftwareBitmapStatics_ABI, &pSbStatics)) break;
            typedef HRESULT (STDMETHODCALLTYPE *FnCreateCopyFromBuffer)(void*, void*, INT32, INT32, INT32, void**);
            if (FAILED(((FnCreateCopyFromBuffer)GetVtbl(pSbStatics)[9])(
                    pSbStatics, pBuffer, 87 /* Bgra8 */, (INT32)work->width, (INT32)work->height, &pSoftwareBitmap)) || !pSoftwareBitmap) {
                break;
            }

            // Obtain Windows.Media.Ocr.OcrEngine instance with language fallback hierarchy
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

            // Execute asynchronous OCR recognition on background worker thread
            typedef HRESULT (STDMETHODCALLTYPE *FnRecognizeAsync)(void*, void*, void**);
            if (FAILED(((FnRecognizeAsync)GetVtbl(pOcrEngine)[6])(pOcrEngine, pSoftwareBitmap, &pAsyncOp)) || !pAsyncOp) break;

            if (FAILED(CallQueryInterface(pAsyncOp, IID_IAsyncInfo_ABI, &pAsyncInfo)) || !pAsyncInfo) break;
            typedef HRESULT (STDMETHODCALLTYPE *FnGetStatus)(void*, INT32*);
            INT32 status = 0; // AsyncStatus: 0 = Started, 1 = Completed, 2 = Canceled, 3 = Error
            for (int waitIter = 0; waitIter < 1500; ++waitIter) {
                if (FAILED(((FnGetStatus)GetVtbl(pAsyncInfo)[7])(pAsyncInfo, &status))) break;
                if (status != 0) break;
                Sleep(10);
            }
            if (status != 1) break;

            typedef HRESULT (STDMETHODCALLTYPE *FnGetResults)(void*, void**);
            if (FAILED(((FnGetResults)GetVtbl(pAsyncOp)[8])(pAsyncOp, &pOcrResult)) || !pOcrResult) break;

            // Extract multi-line text lines from IOcrResult (preserving line breaks) with fallback to get_Text
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
            L"Windows OCR did not find any readable text in the custom area."
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

