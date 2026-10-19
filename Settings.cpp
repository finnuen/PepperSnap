#include "PepperSnapInternal.h"

std::wstring GetPrivateProfileStringDynamic(
    const wchar_t* section,
    const wchar_t* key,
    const wchar_t* defVal,
    const std::wstring& iniPath
) {
    std::vector<WCHAR> buf(512, L'\0');
    while (buf.size() <= 32768) {
        DWORD got = GetPrivateProfileStringW(section, key, defVal ? defVal : L"", buf.data(), (DWORD)buf.size(), iniPath.c_str());
        if (got < buf.size() - 1) {
            return std::wstring(buf.data(), got);
        }
        buf.resize(buf.size() * 2, L'\0');
    }
    return defVal ? std::wstring(defVal) : L"";
}

std::wstring PepperSnapDaemon::GetDefaultDesktopFolder() {
    PWSTR pszPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &pszPath)) && pszPath) {
        std::wstring res(pszPath);
        CoTaskMemFree(pszPath);
        if (!res.empty()) return res;
    }
    WCHAR deskPath[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, SHGFP_TYPE_CURRENT, deskPath)) && wcslen(deskPath) > 0) {
        return std::wstring(deskPath);
    }
    DWORD req = GetCurrentDirectoryW(0, nullptr);
    if (req > 0 && req <= 32768) {
        std::vector<WCHAR> cwd(req + 1, L'\0');
        DWORD got = GetCurrentDirectoryW(req + 1, cwd.data());
        if (got > 0 && got <= req) return std::wstring(cwd.data(), got);
    }
    return L".";
}

std::wstring PepperSnapDaemon::GetAppDataSettingsDir() {
    PWSTR pszPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &pszPath)) && pszPath) {
        std::wstring base(pszPath);
        CoTaskMemFree(pszPath);
        if (!base.empty()) {
            std::wstring dir = base + L"\\PepperSnap";
            CreateDirectoryW(dir.c_str(), nullptr);
            return dir;
        }
    }
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
    WritePrivateProfileStringW(L"PepperSnap", L"PinnedImageAboveEdit", pinnedImageAboveEdit ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"KeepEditWindowOpen", keepEditWindowOpen ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"RunElevated", runElevated ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"DisableRightClickIntegration", disableRightClickIntegration ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"DisableMagnifierWindow", disableMagnifierWindow ? L"1" : L"0", iniPath.c_str());
    if (disableRightClickIntegration) {
        PepperSnapDaemon::UnregisterImageContextMenu();
    } else {
        PepperSnapDaemon::RegisterImageContextMenu();
    }
    WritePrivateProfileStringW(L"PepperSnap", L"EnableWebImageSearch", enableWebImageSearch ? L"1" : L"0", iniPath.c_str());
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
    WritePrivateProfileStringW(L"PepperSnap", L"AutoCheckUpdates", autoCheckUpdates ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"AutoUpdatePromptShown", autoUpdatePromptShown ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"PromptClipboardHistory", promptClipboardHistory ? L"1" : L"0", iniPath.c_str());
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
    writeHk(L"HkPinClipboardMod",  L"HkPinClipboardVk",  hkPinClipboard);
    writeHk(L"HkEditClipboardMod", L"HkEditClipboardVk", hkEditClipboard);
    writeHk(L"HkSearchGoogleMod",  L"HkSearchGoogleVk",  hkSearchGoogle);
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
    writeHk(L"HkActPinMod",        L"HkActPinVk",        hkActPin);
    writeHk(L"HkActOcrMod",        L"HkActOcrVk",        hkActOcr);
    writeHk(L"HkActSaveAsMod",     L"HkActSaveAsVk",     hkActSaveAs);
    writeHk(L"HkActSaveMod",       L"HkActSaveVk",       hkActSave);
    writeHk(L"HkActCopyMod",       L"HkActCopyVk",       hkActCopy);
    writeHk(L"HkActSearchGoogleMod", L"HkActSearchGoogleVk", hkActSearchGoogle);
    writeHk(L"HkFramePlayMod",     L"HkFramePlayVk",     hkFramePlayPause);
    writeHk(L"HkFrameToggleMod",   L"HkFrameToggleVk",   hkFrameToggleStrip);
    writeHk(L"HkPinZoomOutMod",    L"HkPinZoomOutVk",    hkPinZoomOut);
    writeHk(L"HkPinZoomInMod",     L"HkPinZoomInVk",     hkPinZoomIn);
    writeHk(L"HkPinZoomResetMod",   L"HkPinZoomResetVk",   hkPinZoomReset);
    writeHk(L"HkPinPosResetMod",    L"HkPinPosResetVk",    hkPinPosReset);
    writeHk(L"HkPinOutlineMod",     L"HkPinOutlineVk",     hkPinOutline);
    writeHk(L"HkPinHideToolbarMod", L"HkPinHideToolbarVk", hkPinHideToolbar);
    writeHk(L"HkPinSmoothMod",      L"HkPinSmoothVk",      hkPinSmooth);
    writeHk(L"HkPinSaveAsMod",     L"HkPinSaveAsVk",     hkPinSaveAs);
    writeHk(L"HkPinSaveMod",       L"HkPinSaveVk",       hkPinSave);
    writeHk(L"HkPinCopyMod",       L"HkPinCopyVk",       hkPinCopy);
    writeHk(L"HkPinSearchGoogleMod", L"HkPinSearchGoogleVk", hkPinSearchGoogle);
    writeHk(L"HkPinCloseMod",      L"HkPinCloseVk",      hkPinClose);
    WritePrivateProfileStringW(L"PepperSnap", L"HasLastCustomSel", hasLastCustomSel ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"LastCustomSelLeft", std::to_wstring(lastCustomSelRect.left).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"LastCustomSelTop", std::to_wstring(lastCustomSelRect.top).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"LastCustomSelRight", std::to_wstring(lastCustomSelRect.right).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"LastCustomSelBottom", std::to_wstring(lastCustomSelRect.bottom).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"LastCustomSelHasQuad", lastCustomSelHasQuad ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"PepperSnap", L"LastCustomSelHasEllipse", lastCustomSelHasEllipse ? L"1" : L"0", iniPath.c_str());
    for (int i = 0; i < 4; ++i) {
        std::wstring kx = L"LastCustomQuadX" + std::to_wstring(i);
        std::wstring ky = L"LastCustomQuadY" + std::to_wstring(i);
        WritePrivateProfileStringW(L"PepperSnap", kx.c_str(), std::to_wstring(lastCustomSelQuad[i].x).c_str(), iniPath.c_str());
        WritePrivateProfileStringW(L"PepperSnap", ky.c_str(), std::to_wstring(lastCustomSelQuad[i].y).c_str(), iniPath.c_str());
    }
}

bool IsValidGithubRepoSlug(const std::wstring& s) {
    if (s.empty() || s.size() > 200) return false;
    auto isAllowedChar = [](wchar_t c) -> bool {
        return (c >= L'A' && c <= L'Z') ||
               (c >= L'a' && c <= L'z') ||
               (c >= L'0' && c <= L'9') ||
               c == L'.' || c == L'_' || c == L'-';
    };
    size_t slashPos = s.find(L'/');
    if (slashPos == std::wstring::npos || slashPos == 0 || slashPos + 1 >= s.size()) return false;
    if (s.find(L'/', slashPos + 1) != std::wstring::npos) return false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i == slashPos) continue;
        if (!isAllowedChar(s[i])) return false;
    }
    return true;
}

void PepperSnapDaemon::LoadSettings() {
    std::wstring iniPath = GetAppDataSettingsFile();
    if (GetFileAttributesW(iniPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        SaveSettings();
        return;
    }
    std::wstring loadedFolder = GetPrivateProfileStringDynamic(L"PepperSnap", L"SaveFolder", saveFolder.c_str(), iniPath);
    if (!loadedFolder.empty()) saveFolder = loadedFolder;

    std::wstring loadedNaming = GetPrivateProfileStringDynamic(L"PepperSnap", L"NamingPattern", DEFAULT_NAMING_PATTERN, iniPath);
    if (!loadedNaming.empty()) namingPattern = loadedNaming;

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
    pinnedImageAboveEdit = (GetPrivateProfileIntW(L"PepperSnap", L"PinnedImageAboveEdit", 1, iniPath.c_str()) != 0);
    keepEditWindowOpen = (GetPrivateProfileIntW(L"PepperSnap", L"KeepEditWindowOpen", 0, iniPath.c_str()) != 0);
    runElevated = (GetPrivateProfileIntW(L"PepperSnap", L"RunElevated", 1, iniPath.c_str()) != 0);
    disableRightClickIntegration = (GetPrivateProfileIntW(L"PepperSnap", L"DisableRightClickIntegration", 0, iniPath.c_str()) != 0);
    disableMagnifierWindow = (GetPrivateProfileIntW(L"PepperSnap", L"DisableMagnifierWindow", 0, iniPath.c_str()) != 0);
    enableWebImageSearch = (GetPrivateProfileIntW(L"PepperSnap", L"EnableWebImageSearch", 1, iniPath.c_str()) != 0);
    int optWinH = (int)GetPrivateProfileIntW(L"PepperSnap", L"OptionsWindowHeight", 944, iniPath.c_str());
    optionsWindowHeight = std::clamp(optWinH, 320, 2160);
    int ckWinH = (int)GetPrivateProfileIntW(L"PepperSnap", L"CustomizeKeysWindowHeight", 620, iniPath.c_str());
    customizeKeysWindowHeight = std::clamp(ckWinH, 280, 2160);
    int scWinW = (int)GetPrivateProfileIntW(L"PepperSnap", L"ShortcutsWindowWidth", 900, iniPath.c_str());
    shortcutsWindowWidth = std::clamp(scWinW, 680, 3840);
    int scWinH = (int)GetPrivateProfileIntW(L"PepperSnap", L"ShortcutsWindowHeight", 781, iniPath.c_str());
    shortcutsWindowHeight = std::clamp(scWinH, 260, 2160);
    int nsWinH = (int)GetPrivateProfileIntW(L"PepperSnap", L"NamingSyntaxWindowHeight", 760, iniPath.c_str());
    namingSyntaxWindowHeight = std::clamp(nsWinH, 260, 2160);
    int fsH = (int)GetPrivateProfileIntW(L"PepperSnap", L"FrameStripHeight", 142, iniPath.c_str());
    overlayFrameStripHeight = std::clamp(fsH, 92, 600);

    std::wstring colStr = GetPrivateProfileStringDynamic(L"PepperSnap", L"ActiveColorARGB", L"", iniPath);
    if (!colStr.empty()) {
        ARGB parsedCol = (ARGB)_wcstoui64(colStr.c_str(), nullptr, 10);
        if ((parsedCol >> 24) != 0) activeColor = Color(parsedCol);
    }
    int savedStroke = (int)GetPrivateProfileIntW(L"PepperSnap", L"ActiveStrokeWidth", 4, iniPath.c_str());
    activeStroke = (float)std::clamp(savedStroke, 1, 120);
    int savedCounter = (int)GetPrivateProfileIntW(L"PepperSnap", L"CaptureCounter", 1, iniPath.c_str());
    captureCounter = std::max(1, savedCounter);
    penSmoothingEnabled = (GetPrivateProfileIntW(L"PepperSnap", L"PenSmoothingEnabled", 1, iniPath.c_str()) != 0);
    int pSmooth = (int)GetPrivateProfileIntW(L"PepperSnap", L"PenSmoothingStrength", 15, iniPath.c_str());
    penSmoothingStrength = std::clamp(pSmooth, 5, 100);

    autoCheckUpdates = (GetPrivateProfileIntW(L"PepperSnap", L"AutoCheckUpdates", 1, iniPath.c_str()) != 0);
    autoUpdatePromptShown = (GetPrivateProfileIntW(L"PepperSnap", L"AutoUpdatePromptShown", 1, iniPath.c_str()) != 0);
    promptClipboardHistory = (GetPrivateProfileIntW(L"PepperSnap", L"PromptClipboardHistory", 1, iniPath.c_str()) != 0);
    int uInt = (int)GetPrivateProfileIntW(L"PepperSnap", L"UpdateCheckInterval", (INT)UpdateCheckInterval::EveryDay, iniPath.c_str());
    if (uInt >= 0 && uInt <= 4) updateInterval = (UpdateCheckInterval)uInt;

    std::wstring timeStr = GetPrivateProfileStringDynamic(L"PepperSnap", L"LastUpdateCheckTime", L"0", iniPath);
    lastUpdateCheckTime = _wtoi64(timeStr.c_str());

    std::wstring repoStr = GetPrivateProfileStringDynamic(L"PepperSnap", L"UpdateGithubRepo", DEFAULT_GITHUB_REPO, iniPath);
    std::wstring candidateRepo = !repoStr.empty() ? repoStr : std::wstring(DEFAULT_GITHUB_REPO);
    updateGithubRepo = IsValidGithubRepoSlug(candidateRepo) ? candidateRepo : std::wstring(DEFAULT_GITHUB_REPO);

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
    readHk(L"HkPinClipboardMod",  L"HkPinClipboardVk",  hkPinClipboard,  MOD_ALT | MOD_SHIFT,     VK_SNAPSHOT);
    readHk(L"HkEditClipboardMod", L"HkEditClipboardVk", hkEditClipboard, MOD_CONTROL | MOD_ALT, VK_SNAPSHOT);
    readHk(L"HkSearchGoogleMod",  L"HkSearchGoogleVk",  hkSearchGoogle,  MOD_ALT,                 VK_SNAPSHOT);
    readHk(L"HkToolSelectMod",    L"HkToolSelectVk",    hkToolSelect,    0,                       0);
    readHk(L"HkToolPenMod",       L"HkToolPenVk",       hkToolPen,       0,                       0);
    readHk(L"HkToolStabiloMod",   L"HkToolStabiloVk",   hkToolStabilo,   0,                       0);
    readHk(L"HkToolLineMod",      L"HkToolLineVk",      hkToolLine,      0,                       0);
    readHk(L"HkToolArrowMod",     L"HkToolArrowVk",     hkToolArrow,     0,                       0);
    readHk(L"HkToolNumberMod",    L"HkToolNumberVk",    hkToolNumber,    0,                       0);
    readHk(L"HkToolResetNumMod",  L"HkToolResetNumVk",  hkToolResetNum,  0,                       0);
    readHk(L"HkToolRectMod",      L"HkToolRectVk",      hkToolRect,      0,                       0);
    readHk(L"HkToolEllipseMod",   L"HkToolEllipseVk",   hkToolEllipse,   0,                       0);
    readHk(L"HkToolTextMod",      L"HkToolTextVk",      hkToolText,      0,                       0);
    readHk(L"HkToolMosaicSqMod",  L"HkToolMosaicSqVk",  hkToolMosaicSq,  0,                       0);
    readHk(L"HkToolMosaicCirMod", L"HkToolMosaicCirVk", hkToolMosaicCir, 0,                       0);
    readHk(L"HkActUndoMod",       L"HkActUndoVk",       hkActUndo,          MOD_CONTROL,             'Z');
    readHk(L"HkActRedoMod",       L"HkActRedoVk",       hkActRedo,          MOD_CONTROL,             'Y');
    readHk(L"HkActClearMod",      L"HkActClearVk",      hkActClear,         MOD_CONTROL,             'D');
    readHk(L"HkActPinMod",        L"HkActPinVk",        hkActPin,           0,                       0);
    readHk(L"HkActOcrMod",        L"HkActOcrVk",        hkActOcr,           0,                       0);
    readHk(L"HkActSaveAsMod",     L"HkActSaveAsVk",     hkActSaveAs,        MOD_CONTROL | MOD_SHIFT, 'S');
    readHk(L"HkActSaveMod",       L"HkActSaveVk",       hkActSave,          MOD_CONTROL,             'S');
    readHk(L"HkActCopyMod",       L"HkActCopyVk",       hkActCopy,          MOD_CONTROL,             'C');
    readHk(L"HkActSearchGoogleMod", L"HkActSearchGoogleVk", hkActSearchGoogle, 0,                     0);
    readHk(L"HkFramePlayMod",     L"HkFramePlayVk",     hkFramePlayPause,   0,                       VK_SPACE);
    readHk(L"HkFrameToggleMod",   L"HkFrameToggleVk",   hkFrameToggleStrip, 0,                       0);
    readHk(L"HkPinZoomOutMod",    L"HkPinZoomOutVk",    hkPinZoomOut,       0,                       0);
    readHk(L"HkPinZoomInMod",     L"HkPinZoomInVk",     hkPinZoomIn,        0,                       0);
    readHk(L"HkPinZoomResetMod",   L"HkPinZoomResetVk",   hkPinZoomReset,   0,                       0);
    readHk(L"HkPinPosResetMod",    L"HkPinPosResetVk",    hkPinPosReset,    0,                       0);
    readHk(L"HkPinOutlineMod",     L"HkPinOutlineVk",     hkPinOutline,     0,                       0);
    readHk(L"HkPinHideToolbarMod", L"HkPinHideToolbarVk", hkPinHideToolbar, 0,                       0);
    readHk(L"HkPinSmoothMod",      L"HkPinSmoothVk",      hkPinSmooth,      0,                       0);
    readHk(L"HkPinSaveAsMod",     L"HkPinSaveAsVk",     hkPinSaveAs,     MOD_CONTROL | MOD_SHIFT, 'S');
    readHk(L"HkPinSaveMod",       L"HkPinSaveVk",       hkPinSave,       MOD_CONTROL,             'S');
    readHk(L"HkPinCopyMod",       L"HkPinCopyVk",       hkPinCopy,       MOD_CONTROL,             'C');
    readHk(L"HkPinSearchGoogleMod", L"HkPinSearchGoogleVk", hkPinSearchGoogle, 0,                0);
    readHk(L"HkPinCloseMod",      L"HkPinCloseVk",      hkPinClose,      0,                       VK_ESCAPE);

    hasLastCustomSel = (GetPrivateProfileIntW(L"PepperSnap", L"HasLastCustomSel", 0, iniPath.c_str()) != 0);
    lastCustomSelRect.left   = (LONG)GetPrivateProfileIntW(L"PepperSnap", L"LastCustomSelLeft",   0, iniPath.c_str());
    lastCustomSelRect.top    = (LONG)GetPrivateProfileIntW(L"PepperSnap", L"LastCustomSelTop",    0, iniPath.c_str());
    lastCustomSelRect.right  = (LONG)GetPrivateProfileIntW(L"PepperSnap", L"LastCustomSelRight",  0, iniPath.c_str());
    lastCustomSelRect.bottom = (LONG)GetPrivateProfileIntW(L"PepperSnap", L"LastCustomSelBottom", 0, iniPath.c_str());
    lastCustomSelHasQuad = (GetPrivateProfileIntW(L"PepperSnap", L"LastCustomSelHasQuad", 0, iniPath.c_str()) != 0);
    lastCustomSelHasEllipse = (GetPrivateProfileIntW(L"PepperSnap", L"LastCustomSelHasEllipse", 0, iniPath.c_str()) != 0);
    for (int i = 0; i < 4; ++i) {
        std::wstring kx = L"LastCustomQuadX" + std::to_wstring(i);
        std::wstring ky = L"LastCustomQuadY" + std::to_wstring(i);
        lastCustomSelQuad[i].x = (LONG)GetPrivateProfileIntW(L"PepperSnap", kx.c_str(), 0, iniPath.c_str());
        lastCustomSelQuad[i].y = (LONG)GetPrivateProfileIntW(L"PepperSnap", ky.c_str(), 0, iniPath.c_str());
    }
    if (lastCustomSelRect.right <= lastCustomSelRect.left || lastCustomSelRect.bottom <= lastCustomSelRect.top) {
        hasLastCustomSel = false;
        lastCustomSelHasQuad = false;
        lastCustomSelHasEllipse = false;
    }
}

void PepperSnapDaemon::InitPaths() {
    // Save screenshot to Desktop by default, then load persisted settings from %APPDATA%\PepperSnap
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

// Expand the naming pattern and step the counter past any name already on disk,
// so a capture never silently overwrites an earlier one. This matters because
// the default pattern is counter based (PepperSnap_{N}) and the counter is
// restored from settings.ini: point the save folder at a machine that already
// holds PepperSnap_1.png and the first capture must become PepperSnap_2.png.
// A pattern built only from timestamps renders the same name for every counter
// value, so bail out as soon as the name stops changing instead of spinning.
std::wstring PepperSnapDaemon::NextFreeCaptureStem(const std::wstring& extension) {
    int seq = captureCounter;
    std::wstring stem = FormatFilename(seq);
    while (GetFileAttributesW((saveFolder + L"\\" + stem + extension).c_str()) != INVALID_FILE_ATTRIBUTES) {
        int nextSeq = seq + 1;
        std::wstring nextStem = FormatFilename(nextSeq);
        if (nextStem == stem) break;
        seq = nextSeq;
        stem = nextStem;
    }
    captureCounter = seq + 1;
    return stem;
}

// Unified Options Modal (Save Folder, Default Formats, JPEG Quality Slider, Naming Pattern + Restore Default, Non-Stacking Highlighter)
struct OptionsDlgState {
    std::wstring folder;
    ImageFormat regionFmt = ImageFormat::PNG;
    ImageFormat fullFmt = ImageFormat::JPEG;
    ImageFormat copyFmt = ImageFormat::PNG;
    int jpgQuality = 90;
    std::wstring naming;
    bool autoSaveCopy = true;
    bool alsoCopyFull = true;
    bool alsoSavePin = true;
    bool alsoSaveOcr = true;
    bool keepCursor = false;
    bool autoHideFrames = true;
    bool pinAboveEdit = true;
    bool keepEditOpen = false;
    bool runElevated = false;
    bool disableRightClickIntegration = false;
    bool disableMagnifierWindow = false;
    bool nonStackingHi = true;
    bool penSmoothEnabled = true;
    int penSmoothStrength = 15;
    bool autoUpdateEnabled = false;
    UpdateCheckInterval updateInterval = UpdateCheckInterval::EveryDay;
    HotkeyBinding hkRegion{ MOD_CONTROL, VK_SNAPSHOT };
    HotkeyBinding hkFull{ MOD_SHIFT, VK_SNAPSHOT };
    HotkeyBinding hkPrev{ MOD_CONTROL | MOD_SHIFT, VK_SNAPSHOT };
    HotkeyBinding hkPinClipboard{ MOD_ALT | MOD_SHIFT, VK_SNAPSHOT };
    HotkeyBinding hkEditClipboard{ MOD_CONTROL | MOD_ALT, VK_SNAPSHOT };
    HotkeyBinding hkSearchGoogle{ MOD_ALT, VK_SNAPSHOT };
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
    HotkeyBinding hkActUndo{ MOD_CONTROL, 'Z' };
    HotkeyBinding hkActRedo{ MOD_CONTROL, 'Y' };
    HotkeyBinding hkActClear{ MOD_CONTROL, 'D' };
    HotkeyBinding hkActPin{ 0, 0 };
    HotkeyBinding hkActOcr{ 0, 0 };
    HotkeyBinding hkActSaveAs{ MOD_CONTROL | MOD_SHIFT, 'S' };
    HotkeyBinding hkActSave{ MOD_CONTROL, 'S' };
    HotkeyBinding hkActCopy{ MOD_CONTROL, 'C' };
    HotkeyBinding hkActSearchGoogle{ 0, 0 };
    HotkeyBinding hkFramePlayPause{ 0, VK_SPACE };
    HotkeyBinding hkFrameToggleStrip{ 0, 0 };
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
    bool confirmed = false;
    bool openedAppData = false;
    bool restoredAllDefaults = false;
    int scrollY = 0;
    int scrollContentHeight = 820;
    int footerHeight = 56;
    int fixedWinWidth = 580;
    int fullWinHeight = 936;
    int savedWinHeight = 936;
    int lastFooterTop = -1;

    HWND hViewport = nullptr;
    std::vector<std::pair<HWND, int>> scrollControls;
    HFONT hFont = nullptr;
    HFONT hBoldFont = nullptr;
    HFONT hLinkFont = nullptr;
    HPEN hFooterLinePen = nullptr;

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
    HWND hPinAboveEditChk = nullptr;
    HWND hKeepEditOpenChk = nullptr;
    HWND hRunElevatedChk = nullptr;
    HWND hDisableRightClickChk = nullptr;
    HWND hDisableMagnifierChk = nullptr;
    HWND hRunElevatedNoteLbl = nullptr;
    HWND hNonStackingChk = nullptr;
    HWND hPenSmoothChk = nullptr;
    HWND hPenSmoothSlider = nullptr;
    HWND hPenSmoothValLbl = nullptr;
    HWND hAutoUpdateChk = nullptr;
    HWND hComboUpdateInterval = nullptr;
    HWND hCheckUpdateNowBtn = nullptr;
    HWND hCustomizeKeysBtn = nullptr;
    HWND hShortcutsInfoBtn = nullptr;
    HWND hRestoreAllBtn = nullptr;
    HWND hFooterBg = nullptr;
    HWND hAppDataInfo = nullptr;
    HWND hOkBtn = nullptr;
    HWND hCancelBtn = nullptr;

    void InitGdiCache() {
        if (hFont) return;
        hFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        hBoldFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        hLinkFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, TRUE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        hFooterLinePen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
    }

    void FreeGdiCache() {
        if (hFooterLinePen) { DeleteObject(hFooterLinePen); hFooterLinePen = nullptr; }
        if (hLinkFont)      { DeleteObject(hLinkFont);      hLinkFont = nullptr; }
        if (hBoldFont)      { DeleteObject(hBoldFont);      hBoldFont = nullptr; }
        if (hFont)          { DeleteObject(hFont);          hFont = nullptr; }
    }
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
#define IDC_OPT_PIN_ABOVE_EDIT_CHK 1029
#define IDC_OPT_KEEP_EDIT_OPEN_CHK 1030
#define IDC_OPT_SHORTCUTS_INFO     1031
#define IDC_OPT_RUN_ELEVATED_CHK   1032
#define IDC_OPT_DISABLE_RIGHT_CLICK_CHK 1033
#define IDC_OPT_DISABLE_MAGNIFIER_CHK   1034

#define IDC_CK_BTN_BASE          2100
#define IDC_CK_RESTORE_DEFAULTS  2201

struct NamingSyntaxRow {
    std::wstring syntax;
    std::wstring explanation;
    std::wstring example;
};

struct NamingSyntaxDlgState {
    int scrollY = 0;
    int scrollContentHeight = 634;
    int headerHeight = 36;
    int footerHeight = 50;
    int fixedWinWidth = 490;
    int fullWinHeight = 760;
    int savedWinHeight = 760;
    int lastFooterTop = -1;
    int hoveredRowIdx = -1;
    int pressedRowIdx = -1;
    std::wstring copiedSyntax;
    HWND hOkBtn = nullptr;
    HWND hListViewport = nullptr;
    std::vector<HWND> hSyntaxEdits;

    HFONT hHeaderFont = nullptr;
    HFONT hCodeFont = nullptr;
    HFONT hRowFont = nullptr;
    HFONT hBoldFont = nullptr;
    HPEN hHeaderLinePen = nullptr;
    HPEN hRowPen = nullptr;
    HDC hContentDC = nullptr;
    HBITMAP hContentBmp = nullptr;
    HGDIOBJ hContentOldBmp = nullptr;
    int contentW = 0;
    int contentH = 0;
    HDC hBackDC = nullptr;
    HBITMAP hBackBmp = nullptr;
    HGDIOBJ hBackOldBmp = nullptr;
    int backCapW = 0;
    int backCapH = 0;

    void FreeGdiCache() {
        if (hBackDC) {
            if (hBackOldBmp) SelectObject(hBackDC, hBackOldBmp);
            if (hBackBmp) DeleteObject(hBackBmp);
            DeleteDC(hBackDC);
            hBackDC = nullptr;
            hBackBmp = nullptr;
            hBackOldBmp = nullptr;
            backCapW = backCapH = 0;
        }
        if (hContentDC) {
            if (hContentOldBmp) SelectObject(hContentDC, hContentOldBmp);
            if (hContentBmp) DeleteObject(hContentBmp);
            DeleteDC(hContentDC);
            hContentDC = nullptr;
            hContentBmp = nullptr;
            hContentOldBmp = nullptr;
            contentW = contentH = 0;
        }
        if (hRowPen)        { DeleteObject(hRowPen);        hRowPen = nullptr; }
        if (hHeaderLinePen) { DeleteObject(hHeaderLinePen); hHeaderLinePen = nullptr; }
        if (hBoldFont)      { DeleteObject(hBoldFont);      hBoldFont = nullptr; }
        if (hRowFont)       { DeleteObject(hRowFont);       hRowFont = nullptr; }
        if (hCodeFont)      { DeleteObject(hCodeFont);      hCodeFont = nullptr; }
        if (hHeaderFont)    { DeleteObject(hHeaderFont);    hHeaderFont = nullptr; }
    }
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

static void RebuildNamingSyntaxContentBitmap(HDC hdcRef, NamingSyntaxDlgState* st, int clientW) {
    if (!st || !hdcRef) return;
    int cw = std::max(320, clientW);
    int ch = std::max(100, st->scrollContentHeight);
    if (st->hContentDC && st->contentW == cw && st->contentH == ch) return;

    if (!st->hContentDC) {
        st->hContentDC = CreateCompatibleDC(hdcRef);
    } else if (st->hContentOldBmp) {
        SelectObject(st->hContentDC, st->hContentOldBmp);
        if (st->hContentBmp) DeleteObject(st->hContentBmp);
    }
    st->contentW = cw;
    st->contentH = ch;
    st->hContentBmp = CreateCompatibleBitmap(hdcRef, cw, ch);
    st->hContentOldBmp = SelectObject(st->hContentDC, st->hContentBmp);

    RECT rcAll = { 0, 0, cw, ch };
    FillRect(st->hContentDC, &rcAll, GetSysColorBrush(COLOR_BTNFACE));
    SetBkMode(st->hContentDC, TRANSPARENT);

    const int padX = 16;
    const int col1X = padX;
    const int col1W = 84;
    const int sep1X = col1X + col1W;
    const int col2X = sep1X + 12;
    const int col2W = 224;
    const int sep2X = col2X + col2W;
    const int col3X = sep2X + 12;
    const int rightEdge = cw - padX;
    const int rowH = 26;
    const int rowCount = (int)(sizeof(kNamingSyntaxRows) / sizeof(kNamingSyntaxRows[0]));

    HGDIOBJ hOldF = SelectObject(st->hContentDC, st->hRowFont);
    HGDIOBJ hOldP = SelectObject(st->hContentDC, st->hRowPen);

    for (int i = 0; i < rowCount; ++i) {
        int y = 4 + i * rowH;
        if (i > 0) {
            SelectObject(st->hContentDC, st->hRowPen);
            MoveToEx(st->hContentDC, padX, y, nullptr);
            LineTo(st->hContentDC, rightEdge, y);
        }

        SelectObject(st->hContentDC, st->hRowFont);
        SetTextColor(st->hContentDC, GetSysColor(COLOR_WINDOWTEXT));
        RECT rcC2 = { col2X, y, sep2X - 6, y + rowH };
        DrawTextW(st->hContentDC, kNamingSyntaxRows[i].explanation.c_str(), -1, &rcC2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(st->hContentDC, st->hCodeFont);
        SetTextColor(st->hContentDC, RGB(30, 41, 59));
        RECT rcC3 = { col3X, y, rightEdge, y + rowH };
        DrawTextW(st->hContentDC, kNamingSyntaxRows[i].example.c_str(), -1, &rcC3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    SelectObject(st->hContentDC, st->hRowPen);
    MoveToEx(st->hContentDC, sep1X, 0, nullptr);
    LineTo(st->hContentDC, sep1X, ch - 4);
    MoveToEx(st->hContentDC, sep2X, 0, nullptr);
    LineTo(st->hContentDC, sep2X, ch - 4);

    SelectObject(st->hContentDC, hOldP);
    SelectObject(st->hContentDC, hOldF);
}

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
            if (st) {
                RebuildNamingSyntaxContentBitmap(hdc, st, vw);
                if (st->hContentDC) {
                    int sy = std::max(0, std::min(st->scrollY, std::max(0, st->contentH - vh)));
                    BitBlt(hdc, 0, 0, vw, vh, st->hContentDC, 0, sy, SRCCOPY);
                } else {
                    FillRect(hdc, &rcV, GetSysColorBrush(COLOR_BTNFACE));
                }
            } else {
                FillRect(hdc, &rcV, GetSysColorBrush(COLOR_BTNFACE));
            }
            EndPaint(hViewport, &ps);
            return 0;
        }
    }
    return DefWindowProcW(hViewport, msg, wParam, lParam);
}

static void UpdateNamingSyntaxScroll(HWND hWnd, NamingSyntaxDlgState* st, int newScrollY, bool forceEditPos = false) {
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
        SetWindowPos(st->hListViewport, nullptr, 0, st->headerHeight, clientW, viewportH,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
        if (dy != 0 || forceEditPos) {
            const int col1X = 16;
            const int col1W = 84;
            const int rowH = 26;
            HDWP hdwp = BeginDeferWindowPos((int)st->hSyntaxEdits.size());
            for (size_t i = 0; i < st->hSyntaxEdits.size(); ++i) {
                if (st->hSyntaxEdits[i]) {
                    int y = 4 + (int)i * rowH - st->scrollY;
                    if (hdwp) {
                        hdwp = DeferWindowPos(hdwp, st->hSyntaxEdits[i], nullptr, col1X, y + 4, col1W - 8, rowH - 7,
                                              SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
                    } else {
                        SetWindowPos(st->hSyntaxEdits[i], nullptr, col1X, y + 4, col1W - 8, rowH - 7,
                                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
                    }
                }
            }
            if (hdwp) EndDeferWindowPos(hdwp);
        }
    }
    int footerTop = std::max(0, clientH - st->footerHeight);
    if (st->hOkBtn && footerTop != st->lastFooterTop) {
        st->lastFooterTop = footerTop;
        SetWindowPos(st->hOkBtn, HWND_TOP, clientW - 16 - 88, footerTop + 10, 88, 30, SWP_NOACTIVATE | SWP_NOCOPYBITS);
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
                vpwc.hbrBackground = nullptr;
                vpwc.lpszClassName = L"PepperSnapNamingSyntaxViewport";
                RegisterClassExW(&vpwc);
                vpReg = true;
            }

            if (st) {
                st->hHeaderFont = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                st->hBoldFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                st->hCodeFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
                st->hRowFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                st->hHeaderLinePen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
                st->hRowPen = CreatePen(PS_SOLID, 1, RGB(214, 218, 224));

                HDC hdc = GetDC(hWnd);
                if (hdc) {
                    RebuildNamingSyntaxContentBitmap(hdc, st, clientW);
                    ReleaseDC(hWnd, hdc);
                }

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
                    SendMessageW(hEd, WM_SETFONT, (WPARAM)st->hCodeFont, TRUE);
                    WNDPROC origEditProc = (WNDPROC)SetWindowLongPtrW(hEd, GWLP_WNDPROC, (LONG_PTR)NamingSyntaxEditSubclassProc);
                    SetPropW(hEd, L"PepperSnapOrigSyntaxEditProc", (HANDLE)origEditProc);
                    SetPropW(hEd, L"PepperSnapSyntaxRowIdx", (HANDLE)(INT_PTR)(i + 1));
                    st->hSyntaxEdits[i] = hEd;
                }
            }

            HWND hOk = CreateWindowExW(0, L"BUTTON", L"OK",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_DEFPUSHBUTTON,
                clientW - 16 - 88, clientH - 40, 88, 30, hWnd, (HMENU)IDOK, nullptr, nullptr);
            if (st && st->hBoldFont) {
                SendMessageW(hOk, WM_SETFONT, (WPARAM)st->hBoldFont, TRUE);
            }
            if (st) {
                st->hOkBtn = hOk;
                UpdateNamingSyntaxScroll(hWnd, st, 0, true);
            }
            return 0;
        }
        case WM_NCDESTROY:
            if (st) {
                st->FreeGdiCache();
            }
            break;
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
                UpdateNamingSyntaxScroll(hWnd, st, st->scrollY, false);
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
                UpdateNamingSyntaxScroll(hWnd, st, target, false);
                return 0;
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            if (st) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (delta != 0) {
                    int step = -(delta * 52) / WHEEL_DELTA;
                    UpdateNamingSyntaxScroll(hWnd, st, st->scrollY + step, false);
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
            if (st) {
                if (!st->hBackDC || clientW > st->backCapW || clientH > st->backCapH) {
                    if (!st->hBackDC) {
                        st->hBackDC = CreateCompatibleDC(hdc);
                    } else if (st->hBackOldBmp) {
                        SelectObject(st->hBackDC, st->hBackOldBmp);
                        if (st->hBackBmp) DeleteObject(st->hBackBmp);
                    }
                    st->backCapW = std::max(clientW + 64, 520);
                    st->backCapH = std::max(clientH + 64, GetSystemMetrics(SM_CYSCREEN));
                    st->hBackBmp = CreateCompatibleBitmap(hdc, st->backCapW, st->backCapH);
                    st->hBackOldBmp = SelectObject(st->hBackDC, st->hBackBmp);
                }
                HDC memDC = st->hBackDC;
                RECT rcAll = { 0, 0, clientW, clientH };
                FillRect(memDC, &rcAll, GetSysColorBrush(COLOR_BTNFACE));
                SetBkMode(memDC, TRANSPARENT);

                const int padX = 16;
                const int col1X = padX;
                const int col1W = 84;
                const int sep1X = col1X + col1W;
                const int col2X = sep1X + 12;
                const int col2W = 224;
                const int sep2X = col2X + col2W;
                const int col3X = sep2X + 12;
                const int rightEdge = clientW - padX;

                const int headerH = st->headerHeight;
                const int footerH = st->footerHeight;
                const int footerTop = std::max(headerH, clientH - footerH);

                HGDIOBJ hOldF = SelectObject(memDC, st->hHeaderFont);
                HGDIOBJ hOldP = SelectObject(memDC, st->hHeaderLinePen);

                // Pinned top header row
                SetTextColor(memDC, RGB(220, 38, 38));
                RECT rcH1 = { col1X, 6, sep1X - 6, headerH - 2 };
                RECT rcH2 = { col2X, 6, sep2X - 6, headerH - 2 };
                RECT rcH3 = { col3X, 6, rightEdge, headerH - 2 };
                DrawTextW(memDC, L"Syntax",      -1, &rcH1, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                DrawTextW(memDC, L"Explanation", -1, &rcH2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                DrawTextW(memDC, L"Example",     -1, &rcH3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                SelectObject(memDC, st->hHeaderLinePen);
                MoveToEx(memDC, padX, headerH - 1, nullptr);
                LineTo(memDC, rightEdge, headerH - 1);

                // Vertical dividers in header
                SelectObject(memDC, st->hRowPen);
                MoveToEx(memDC, sep1X, 8, nullptr);
                LineTo(memDC, sep1X, headerH - 2);
                MoveToEx(memDC, sep2X, 8, nullptr);
                LineTo(memDC, sep2X, headerH - 2);

                // Pinned bottom footer bar
                RECT rcFooter = { 0, footerTop, clientW, clientH };
                FillRect(memDC, &rcFooter, GetSysColorBrush(COLOR_BTNFACE));
                SelectObject(memDC, st->hHeaderLinePen);
                MoveToEx(memDC, 0, footerTop, nullptr);
                LineTo(memDC, clientW, footerTop);

                SelectObject(memDC, st->hRowFont);
                RECT rcHint = { padX, footerTop + 10, clientW - 116, clientH - 10 };
                if (!st->copiedSyntax.empty()) {
                    SetTextColor(memDC, RGB(22, 163, 74));
                    std::wstring copiedMsg = L"\x2713 Copied " + st->copiedSyntax + L" to clipboard";
                    DrawTextW(memDC, copiedMsg.c_str(), -1, &rcHint, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                } else {
                    SetTextColor(memDC, GetSysColor(COLOR_GRAYTEXT));
                    DrawTextW(memDC, L"Click or select any syntax to copy", -1, &rcHint, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                }

                SelectObject(memDC, hOldP);
                SelectObject(memDC, hOldF);

                BitBlt(hdc, 0, 0, clientW, clientH, memDC, 0, 0, SRCCOPY);
            }
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
        wc.hbrBackground = nullptr;
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
    }

    std::wstring syntaxTitle = L"PepperSnap v" + std::wstring(PepperSnapDaemon::APP_VERSION) + L" — naming pattern syntax";
    HWND hDlg = CreateWindowExW(
        dwExStyle, L"PepperSnapNamingSyntaxModal",
        syntaxTitle.c_str(),
        dwStyle,
        posX, std::max(10, posY), fixedW, winH,
        hParentOptWnd, nullptr, g_Daemon.hInst, &st
    );
    g_Daemon.hNamingSyntaxWnd = hDlg;
    BringWindowToTop(hDlg);
    SetForegroundWindow(hDlg);

    MSG msg;
    while (IsWindow(hDlg) && PepperSnapDaemon::GetMessageKeepQuit(&msg)) {
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
        BringWindowToTop(hParentOptWnd);
        SetForegroundWindow(hParentOptWnd);
        SetActiveWindow(hParentOptWnd);
    }
}

struct CustomizeKeyRow {
    int slot = 0; // 1..50, or 0 for section header/separator
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
    HotkeyBinding hkPinClipboard;
    HotkeyBinding hkEditClipboard;
    HotkeyBinding hkSearchGoogle;
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
    HotkeyBinding hkActPin;
    HotkeyBinding hkActOcr;
    HotkeyBinding hkActSaveAs;
    HotkeyBinding hkActSave;
    HotkeyBinding hkActCopy;
    HotkeyBinding hkActSearchGoogle;
    HotkeyBinding hkFramePlayPause;
    HotkeyBinding hkFrameToggleStrip;
    HotkeyBinding hkPinZoomOut;
    HotkeyBinding hkPinZoomIn;
    HotkeyBinding hkPinZoomReset;
    HotkeyBinding hkPinPosReset;
    HotkeyBinding hkPinOutline;
    HotkeyBinding hkPinHideToolbar;
    HotkeyBinding hkPinSmooth;
    HotkeyBinding hkPinSaveAs;
    HotkeyBinding hkPinSave;
    HotkeyBinding hkPinCopy;
    HotkeyBinding hkPinSearchGoogle;
    HotkeyBinding hkPinClose;

    std::vector<CustomizeKeyRow> rows;
    bool confirmed = false;
    int scrollY = 0;
    int scrollContentHeight = 680;
    int footerHeight = 50;
    int fixedWinWidth = 460;
    int fullWinHeight = 730;
    int savedWinHeight = 620;
    int lastFooterTop = -1;

    HWND hViewport = nullptr;
    HWND hFooterBg = nullptr;
    HWND hRestoreBtn = nullptr;
    HWND hSaveBtn = nullptr;
    HWND hCancelBtn = nullptr;

    HFONT hFont = nullptr;
    HFONT hBoldFont = nullptr;
    HPEN hSepShadow = nullptr;
    HPEN hSepHilight = nullptr;
    HPEN hRowSep = nullptr;
    HDC hContentDC = nullptr;
    HBITMAP hContentBmp = nullptr;
    HGDIOBJ hContentOldBmp = nullptr;
    int contentBmpW = 0;
    int contentBmpH = 0;

    void InitGdiCache(HDC hdcRef, int clientW) {
        if (!hFont) {
            hFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            hBoldFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            hSepShadow = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
            hSepHilight = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DHILIGHT));
            hRowSep = CreatePen(PS_SOLID, 1, RGB(218, 222, 228));
        }
        int targetW = std::max(428, clientW);
        int targetH = std::max(64, scrollContentHeight);
        if (!hContentDC || contentBmpW != targetW || contentBmpH != targetH) {
            if (!hContentDC) {
                hContentDC = CreateCompatibleDC(hdcRef);
            } else if (hContentOldBmp) {
                SelectObject(hContentDC, hContentOldBmp);
                if (hContentBmp) DeleteObject(hContentBmp);
            }
            contentBmpW = targetW;
            contentBmpH = targetH;
            hContentBmp = CreateCompatibleBitmap(hdcRef, contentBmpW, contentBmpH);
            hContentOldBmp = SelectObject(hContentDC, hContentBmp);

            RECT rcAll = { 0, 0, contentBmpW, contentBmpH };
            FillRect(hContentDC, &rcAll, GetSysColorBrush(COLOR_BTNFACE));
            SetBkMode(hContentDC, TRANSPARENT);

            HGDIOBJ hOldF = SelectObject(hContentDC, hFont);
            HGDIOBJ hOldP = SelectObject(hContentDC, hSepShadow);

            int padX = 16;
            int rightEdge = contentBmpW - padX;
            const int iconW = 26;
            const int iconH = 22;

            Graphics gIcon(hContentDC);
            gIcon.SetSmoothingMode(SmoothingModeAntiAlias);
            gIcon.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

            for (size_t i = 0; i < rows.size(); ++i) {
                const auto& r = rows[i];
                int drawY = r.y;

                if (r.isSeparator) {
                    if (i > 0) {
                        int lineY = drawY + 6;
                        SelectObject(hContentDC, hSepShadow);
                        MoveToEx(hContentDC, padX, lineY, nullptr);
                        LineTo(hContentDC, rightEdge, lineY);
                        SelectObject(hContentDC, hSepHilight);
                        MoveToEx(hContentDC, padX, lineY + 1, nullptr);
                        LineTo(hContentDC, rightEdge, lineY + 1);
                    }
                    if (!r.label.empty()) {
                        SelectObject(hContentDC, hBoldFont);
                        SetTextColor(hContentDC, RGB(15, 23, 42));
                        int hdrTop = (i > 0) ? (drawY + 12) : (drawY + 2);
                        RECT rcHdr = { padX, hdrTop, rightEdge, drawY + r.h };
                        DrawTextW(hContentDC, r.label.c_str(), -1, &rcHdr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                    }
                } else {
                    if (i > 0 && !rows[i - 1].isSeparator) {
                        SelectObject(hContentDC, hRowSep);
                        MoveToEx(hContentDC, padX, drawY, nullptr);
                        LineTo(hContentDC, rightEdge, drawY);
                    }

                    int textLeft = padX;
                    if (r.hasIcon) {
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

                    SelectObject(hContentDC, hFont);
                    SetTextColor(hContentDC, GetSysColor(COLOR_WINDOWTEXT));
                    RECT rcLbl = { textLeft, drawY, 252, drawY + r.h };
                    DrawTextW(hContentDC, r.label.c_str(), -1, &rcLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                }
            }

            SelectObject(hContentDC, hOldP);
            SelectObject(hContentDC, hOldF);
        }
    }

    void FreeGdiCache() {
        if (hContentDC) {
            if (hContentOldBmp) SelectObject(hContentDC, hContentOldBmp);
            if (hContentBmp) DeleteObject(hContentBmp);
            DeleteDC(hContentDC);
            hContentDC = nullptr;
            hContentBmp = nullptr;
            hContentOldBmp = nullptr;
            contentBmpW = contentBmpH = 0;
        }
        if (hRowSep)     { DeleteObject(hRowSep);     hRowSep = nullptr; }
        if (hSepHilight) { DeleteObject(hSepHilight); hSepHilight = nullptr; }
        if (hSepShadow)  { DeleteObject(hSepShadow);  hSepShadow = nullptr; }
        if (hBoldFont)   { DeleteObject(hBoldFont);   hBoldFont = nullptr; }
        if (hFont)       { DeleteObject(hFont);       hFont = nullptr; }
    }
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
    if (footerTop == ck->lastFooterTop) return;
    ck->lastFooterTop = footerTop;

    if (ck->hViewport) {
        SetWindowPos(ck->hViewport, nullptr, 0, 0, clientW, footerTop, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    HDWP hdwp = BeginDeferWindowPos(4);
    if (hdwp) {
        if (ck->hFooterBg) {
            hdwp = DeferWindowPos(hdwp, ck->hFooterBg, HWND_TOP, 0, footerTop, clientW, ck->footerHeight, SWP_NOACTIVATE);
        }
        if (ck->hRestoreBtn) {
            hdwp = DeferWindowPos(hdwp, ck->hRestoreBtn, HWND_TOP, 16, footerTop + 9, 128, 32, SWP_NOACTIVATE);
        }
        if (ck->hSaveBtn) {
            hdwp = DeferWindowPos(hdwp, ck->hSaveBtn, HWND_TOP, clientW - 16 - 88 - 8 - 88, footerTop + 9, 88, 32, SWP_NOACTIVATE);
        }
        if (ck->hCancelBtn) {
            hdwp = DeferWindowPos(hdwp, ck->hCancelBtn, HWND_TOP, clientW - 16 - 88, footerTop + 9, 88, 32, SWP_NOACTIVATE);
        }
        EndDeferWindowPos(hdwp);
    }
}

static void UpdateCustomizeKeysScroll(HWND hWnd, CustomizeKeysDlgState* ck, int newScrollY, bool forceReposition = false) {
    if (!hWnd || !ck) return;
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int clientH = std::max(1, (int)(rcClient.bottom - rcClient.top));
    int viewportH = std::max(1, clientH - ck->footerHeight);
    int maxScroll = std::max(0, ck->scrollContentHeight - viewportH);
    int clamped = std::max(0, std::min(newScrollY, maxScroll));
    int dy = ck->scrollY - clamped;
    ck->scrollY = clamped;

    LayoutCustomizeKeysFooter(hWnd, ck);

    if (dy != 0 || forceReposition) {
        int btnCount = 0;
        for (const auto& r : ck->rows) {
            if (!r.isSeparator && r.hBtn) btnCount++;
        }
        if (btnCount > 0) {
            HDWP hdwp = BeginDeferWindowPos(btnCount);
            if (hdwp) {
                for (const auto& r : ck->rows) {
                    if (!r.isSeparator && r.hBtn) {
                        int btnY = r.y + (r.h - 26) / 2 - ck->scrollY;
                        hdwp = DeferWindowPos(hdwp, r.hBtn, nullptr, 256, btnY, 156, 26, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
                    }
                }
                EndDeferWindowPos(hdwp);
            }
        }
    }

    SCROLLINFO si = { sizeof(SCROLLINFO) };
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    si.nMin = 0;
    si.nMax = ck->scrollContentHeight - 1;
    si.nPage = (UINT)viewportH;
    si.nPos = ck->scrollY;
    SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
    UpdateCustomizeKeysRecordingTooltip(hWnd, ck);

    if (dy != 0 && ck->hViewport) {
        RedrawWindow(ck->hViewport, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
}

static LRESULT CALLBACK CustomizeKeysViewportWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CustomizeKeysDlgState* ck = (CustomizeKeysDlgState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            ck = (CustomizeKeysDlgState*)cs->lpCreateParams;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)ck);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rcV;
            GetClientRect(hWnd, &rcV);
            int vw = std::max(1, (int)(rcV.right - rcV.left));
            int vh = std::max(1, (int)(rcV.bottom - rcV.top));
            if (ck) {
                ck->InitGdiCache(hdc, vw);
                if (ck->hContentDC) {
                    int sy = ck->scrollY;
                    int copyH = std::max(0, std::min(vh, ck->contentBmpH - sy));
                    if (copyH > 0) {
                        BitBlt(hdc, 0, 0, vw, copyH, ck->hContentDC, 0, sy, SRCCOPY);
                    }
                    if (copyH < vh) {
                        RECT rcRem = { 0, copyH, vw, vh };
                        FillRect(hdc, &rcRem, GetSysColorBrush(COLOR_BTNFACE));
                    }
                } else {
                    FillRect(hdc, &rcV, GetSysColorBrush(COLOR_BTNFACE));
                }
            }
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_COMMAND:
        case WM_MOUSEWHEEL: {
            HWND hParent = GetParent(hWnd);
            if (hParent) {
                return SendMessageW(hParent, msg, wParam, lParam);
            }
            break;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static LRESULT CALLBACK CustomizeKeysDlgWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CustomizeKeysDlgState* ck = (CustomizeKeysDlgState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            ck = (CustomizeKeysDlgState*)cs->lpCreateParams;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)ck);

            if (ck) {
                HDC hdc = GetDC(hWnd);
                ck->InitGdiCache(hdc, 428);
                if (hdc) ReleaseDC(hWnd, hdc);

                ck->hViewport = CreateWindowExW(0, L"PepperSnapCustomizeKeysViewport", L"",
                    WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                    0, 0, 428, 500, hWnd, nullptr, g_Daemon.hInst, ck);

                HWND hBtnParent = ck->hViewport ? ck->hViewport : hWnd;
                for (auto& r : ck->rows) {
                    if (r.isSeparator) continue;
                    int btnY = r.y + (r.h - 26) / 2;
                    r.hBtn = CreateWindowExW(0, L"BUTTON", L"",
                        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                        256, btnY, 156, 26, hBtnParent, (HMENU)(INT_PTR)(IDC_CK_BTN_BASE + r.slot), nullptr, nullptr);
                    SendMessageW(r.hBtn, WM_SETFONT, (WPARAM)ck->hFont, TRUE);
                }

                ck->hFooterBg = CreateWindowExW(0, L"STATIC", L"",
                    WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 500, 428, ck->footerHeight, hWnd, nullptr, nullptr, nullptr);

                ck->hRestoreBtn = CreateWindowExW(0, L"BUTTON", L"Restore defaults",
                    WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 16, 509, 128, 32, hWnd, (HMENU)IDC_CK_RESTORE_DEFAULTS, nullptr, nullptr);
                SendMessageW(ck->hRestoreBtn, WM_SETFONT, (WPARAM)ck->hFont, TRUE);

                ck->hSaveBtn = CreateWindowExW(0, L"BUTTON", L"Save",
                    WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_DEFPUSHBUTTON, 228, 509, 88, 32, hWnd, (HMENU)IDOK, nullptr, nullptr);
                SendMessageW(ck->hSaveBtn, WM_SETFONT, (WPARAM)ck->hBoldFont, TRUE);

                ck->hCancelBtn = CreateWindowExW(0, L"BUTTON", L"Cancel",
                    WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 324, 509, 88, 32, hWnd, (HMENU)IDCANCEL, nullptr, nullptr);
                SendMessageW(ck->hCancelBtn, WM_SETFONT, (WPARAM)ck->hFont, TRUE);

                RefreshCustomizeKeysButtonLabels(hWnd, ck);
                ck->lastFooterTop = -1;
                LayoutCustomizeKeysFooter(hWnd, ck);
                UpdateCustomizeKeysScroll(hWnd, ck, 0, true);
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
                int footerTop = std::max(0, clientH - ck->footerHeight);
                RECT rcFooter = { 0, footerTop, clientW, clientH };
                FillRect(hdc, &rcFooter, GetSysColorBrush(COLOR_BTNFACE));
                if (ck->hSepShadow) {
                    HGDIOBJ hOldFooterPen = SelectObject(hdc, ck->hSepShadow);
                    MoveToEx(hdc, 0, footerTop, nullptr);
                    LineTo(hdc, clientW, footerTop);
                    SelectObject(hdc, hOldFooterPen);
                }
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
                if (ck->hSepShadow) {
                    HGDIOBJ hOldPen = SelectObject(hdcStatic, ck->hSepShadow);
                    MoveToEx(hdcStatic, 0, 0, nullptr);
                    LineTo(hdcStatic, rcF.right, 0);
                    SelectObject(hdcStatic, hOldPen);
                }
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
                UpdateCustomizeKeysScroll(hWnd, ck, ck->scrollY, false);
                if (ck->hViewport) {
                    InvalidateRect(ck->hViewport, nullptr, FALSE);
                }
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
                UpdateCustomizeKeysScroll(hWnd, ck, target, false);
                return 0;
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            if (ck) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (delta != 0) {
                    int step = -(delta * 48) / WHEEL_DELTA;
                    UpdateCustomizeKeysScroll(hWnd, ck, ck->scrollY + step, false);
                }
                return 0;
            }
            break;
        }
        case WM_SHORTCUT_RECORDED: {
            if (!ck) return 0;
            UINT mods = (UINT)wParam;
            UINT vk = (UINT)lParam;
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
            if (ck) {
                ck->FreeGdiCache();
            }
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
        WNDCLASSEXW wcVp = { sizeof(WNDCLASSEXW) };
        wcVp.lpfnWndProc = CustomizeKeysViewportWndProc;
        wcVp.hInstance = g_Daemon.hInst;
        wcVp.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wcVp.hbrBackground = nullptr;
        wcVp.lpszClassName = L"PepperSnapCustomizeKeysViewport";
        RegisterClassExW(&wcVp);

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
    ck.hkPinClipboard  = optSt->hkPinClipboard;
    ck.hkEditClipboard = optSt->hkEditClipboard;
    ck.hkSearchGoogle  = optSt->hkSearchGoogle;
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
    ck.hkActPin           = optSt->hkActPin;
    ck.hkActOcr           = optSt->hkActOcr;
    ck.hkActSaveAs        = optSt->hkActSaveAs;
    ck.hkActSave          = optSt->hkActSave;
    ck.hkActCopy          = optSt->hkActCopy;
    ck.hkActSearchGoogle  = optSt->hkActSearchGoogle;
    ck.hkFramePlayPause   = optSt->hkFramePlayPause;
    ck.hkFrameToggleStrip = optSt->hkFrameToggleStrip;
    ck.hkPinZoomOut       = optSt->hkPinZoomOut;
    ck.hkPinZoomIn     = optSt->hkPinZoomIn;
    ck.hkPinZoomReset   = optSt->hkPinZoomReset;
    ck.hkPinPosReset    = optSt->hkPinPosReset;
    ck.hkPinOutline     = optSt->hkPinOutline;
    ck.hkPinHideToolbar = optSt->hkPinHideToolbar;
    ck.hkPinSmooth      = optSt->hkPinSmooth;
    ck.hkPinSaveAs     = optSt->hkPinSaveAs;
    ck.hkPinSave       = optSt->hkPinSave;
    ck.hkPinCopy       = optSt->hkPinCopy;
    ck.hkPinSearchGoogle = optSt->hkPinSearchGoogle;
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
    auto addSectionHdr = [&](const std::wstring& title) {
        CustomizeKeyRow r;
        r.isSeparator = true;
        r.label = title;
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

    // Global capture & clipboard shortcuts
    addSectionHdr(L"Global capture & clipboard shortcuts");
    addGlobalRow(L"Custom area",                       &ck.hkRegion,       { MOD_CONTROL, VK_SNAPSHOT });
    addGlobalRow(L"Instant fullscreen",                &ck.hkFull,         { MOD_SHIFT, VK_SNAPSHOT });
    addGlobalRow(L"Instant save previous custom area", &ck.hkPrev,         { MOD_CONTROL | MOD_SHIFT, VK_SNAPSHOT });
    addGlobalRow(L"Pin the image in clipboard on top", &ck.hkPinClipboard, { MOD_ALT | MOD_SHIFT, VK_SNAPSHOT });
    addGlobalRow(L"Edit the image in clipboard",       &ck.hkEditClipboard,{ MOD_CONTROL | MOD_ALT, VK_SNAPSHOT });
    addGlobalRow(L"Search on web the image in clipboard", &ck.hkSearchGoogle, { MOD_ALT, VK_SNAPSHOT });

    // Custom area annotation tools
    addSectionHdr(L"Custom area — annotation tools");
    addToolRow(OverlayTool::SelectMove,   L"Select mode",         &ck.hkToolSelect,    { 0, 0 });
    addToolRow(OverlayTool::Pen,          L"Pen",                 &ck.hkToolPen,       { 0, 0 });
    addToolRow(OverlayTool::Highlighter,  L"Stabilo highlighter", &ck.hkToolStabilo,   { 0, 0 });
    addToolRow(OverlayTool::Line,         L"Line",                &ck.hkToolLine,      { 0, 0 });
    addToolRow(OverlayTool::Arrow,        L"Arrow",               &ck.hkToolArrow,     { 0, 0 });
    addToolRow(OverlayTool::NumberArrow,  L"Numbering arrow",     &ck.hkToolNumber,    { 0, 0 });
    addActRow(DBTN_ACT_RESET_NUM,  false, L"Reset counter to 1",  &ck.hkToolResetNum,  { 0, 0 });
    addToolRow(OverlayTool::Rectangle,    L"Square / rectangle",  &ck.hkToolRect,      { 0, 0 });
    addToolRow(OverlayTool::Ellipse,      L"Circle / ellipse",    &ck.hkToolEllipse,   { 0, 0 });
    addToolRow(OverlayTool::TextBox,      L"Text box",            &ck.hkToolText,      { 0, 0 });
    addToolRow(OverlayTool::MosaicSquare, L"Mosaic square",       &ck.hkToolMosaicSq,  { 0, 0 });
    addToolRow(OverlayTool::MosaicCircle, L"Mosaic circle",       &ck.hkToolMosaicCir, { 0, 0 });

    // Custom area toolbar actions, multi-frame editor & export
    addSectionHdr(L"Custom area — toolbar actions & multi-frame editor");
    addActRow(DBTN_ACT_UNDO,             false, L"Undo",                          &ck.hkActUndo,          { MOD_CONTROL, 'Z' });
    addActRow(DBTN_ACT_REDO,             false, L"Redo",                          &ck.hkActRedo,          { MOD_CONTROL, 'Y' });
    addActRow(DBTN_ACT_CLEAR,            false, L"Clear all annotations",         &ck.hkActClear,         { MOD_CONTROL, 'D' });
    addActRow(DBTN_ACT_PIN,              false, L"Pin on top",                    &ck.hkActPin,           { 0, 0 });
    addActRow(DBTN_ACT_OCR,              false, L"Extract text with OCR",         &ck.hkActOcr,           { 0, 0 });
    addActRow(DBTN_ACT_SAVE_AS,          false, L"Save as JPG/PNG/WEBP/BMP",      &ck.hkActSaveAs,        { MOD_CONTROL | MOD_SHIFT, 'S' });
    addActRow(DBTN_ACT_SAVE,             false, L"Quick save",                    &ck.hkActSave,          { MOD_CONTROL, 'S' });
    addActRow(DBTN_ACT_COPY,             true,  L"Copy to clipboard",             &ck.hkActCopy,          { MOD_CONTROL, 'C' });
    addActRow(DBTN_ACT_SEARCH_GOOGLE,    false, L"Search on web",                 &ck.hkActSearchGoogle,  { 0, 0 });
    addActRow(DBTN_ACT_FRAME_PLAY_PAUSE, false, L"Play / Pause frames (GIF/WebP)",&ck.hkFramePlayPause,   { 0, VK_SPACE });
    addActRow(DBTN_ACT_FRAME_TOGGLE,     false, L"Hide / Show frames (GIF/WebP)", &ck.hkFrameToggleStrip, { 0, 0 });

    // Pinned image toolbar actions & export
    addSectionHdr(L"Pin on top — toolbar actions & export");
    addActRow(DBTN_ACT_ZOOM_OUT,         false, L"Zoom out",                      &ck.hkPinZoomOut,       { 0, 0 });
    addActRow(DBTN_ACT_ZOOM_IN,          false, L"Zoom in",                       &ck.hkPinZoomIn,        { 0, 0 });
    addActRow(DBTN_ACT_ZOOM_RESET,       false, L"Reset to original size",        &ck.hkPinZoomReset,     { 0, 0 });
    addActRow(DBTN_ACT_POS_RESET,        false, L"Reset to original position",    &ck.hkPinPosReset,      { 0, 0 });
    addActRow(DBTN_ACT_PIN_OUTLINE,      false, L"Show / Hide outline",           &ck.hkPinOutline,       { 0, 0 });
    addActRow(DBTN_ACT_PIN_HIDE_TOOLBAR, false, L"Hide / Show toolbar",           &ck.hkPinHideToolbar,   { 0, 0 });
    addActRow(DBTN_ACT_PIN_UNFILTER,     false, L"Smooth the image",              &ck.hkPinSmooth,        { 0, 0 });
    addActRow(DBTN_ACT_SAVE_AS,          false, L"Save as JPG/PNG/WEBP/BMP",      &ck.hkPinSaveAs,        { MOD_CONTROL | MOD_SHIFT, 'S' });
    addActRow(DBTN_ACT_SAVE,             false, L"Quick save",                    &ck.hkPinSave,          { MOD_CONTROL, 'S' });
    addActRow(DBTN_ACT_COPY,             true,  L"Copy to clipboard",             &ck.hkPinCopy,          { MOD_CONTROL, 'C' });
    addActRow(DBTN_ACT_PIN_SEARCH_GOOGLE,false, L"Search on web",                 &ck.hkPinSearchGoogle,  { 0, 0 });
    addActRow(DBTN_ACT_CLOSE,            false, L"Close pinned image",            &ck.hkPinClose,         { 0, VK_ESCAPE });

    int curY = 6;
    for (size_t i = 0; i < ck.rows.size(); ++i) {
        auto& r = ck.rows[i];
        r.y = curY;
        r.h = r.isSeparator ? ((i == 0) ? 28 : 36) : 32;
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

    std::wstring ckTitle = L"PepperSnap v" + std::wstring(PepperSnapDaemon::APP_VERSION) + L" — customize keys";
    HWND hDlg = CreateWindowExW(
        dwExStyle, L"PepperSnapCustomizeKeysModal",
        ckTitle.c_str(),
        dwStyle,
        (sw - fixedW) / 2, std::max(10, (sh - initH) / 2), fixedW, initH,
        hParentOptWnd, nullptr, g_Daemon.hInst, &ck
    );
    g_Daemon.hCustomizeKeysWnd = hDlg;
    BringWindowToTop(hDlg);
    SetForegroundWindow(hDlg);

    MSG msg;
    while (IsWindow(hDlg) && PepperSnapDaemon::GetMessageKeepQuit(&msg)) {
        if (g_RecordingHotkeySlot != 0 &&
            (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN ||
             ((msg.message == WM_KEYUP || msg.message == WM_SYSKEYUP) && msg.wParam == VK_SNAPSHOT))) {
            DWORD vk = (DWORD)msg.wParam;
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
        optSt->hkPinClipboard  = ck.hkPinClipboard;
        optSt->hkEditClipboard = ck.hkEditClipboard;
        optSt->hkSearchGoogle  = ck.hkSearchGoogle;
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
        optSt->hkActPin           = ck.hkActPin;
        optSt->hkActOcr           = ck.hkActOcr;
        optSt->hkActSaveAs        = ck.hkActSaveAs;
        optSt->hkActSave          = ck.hkActSave;
        optSt->hkActCopy          = ck.hkActCopy;
        optSt->hkActSearchGoogle  = ck.hkActSearchGoogle;
        optSt->hkFramePlayPause   = ck.hkFramePlayPause;
        optSt->hkFrameToggleStrip = ck.hkFrameToggleStrip;
        optSt->hkPinZoomOut       = ck.hkPinZoomOut;
        optSt->hkPinZoomIn        = ck.hkPinZoomIn;
        optSt->hkPinZoomReset     = ck.hkPinZoomReset;
        optSt->hkPinPosReset      = ck.hkPinPosReset;
        optSt->hkPinOutline       = ck.hkPinOutline;
        optSt->hkPinHideToolbar   = ck.hkPinHideToolbar;
        optSt->hkPinSmooth        = ck.hkPinSmooth;
        optSt->hkPinSaveAs        = ck.hkPinSaveAs;
        optSt->hkPinSave          = ck.hkPinSave;
        optSt->hkPinCopy          = ck.hkPinCopy;
        optSt->hkPinSearchGoogle  = ck.hkPinSearchGoogle;
        optSt->hkPinClose         = ck.hkPinClose;

        g_Daemon.hkRegionSnip       = ck.hkRegion;
        g_Daemon.hkFullSnap         = ck.hkFull;
        g_Daemon.hkPrevRegion       = ck.hkPrev;
        g_Daemon.hkPinClipboard     = ck.hkPinClipboard;
        g_Daemon.hkEditClipboard    = ck.hkEditClipboard;
        g_Daemon.hkSearchGoogle     = ck.hkSearchGoogle;
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
        g_Daemon.hkActPin           = ck.hkActPin;
        g_Daemon.hkActOcr           = ck.hkActOcr;
        g_Daemon.hkActSaveAs        = ck.hkActSaveAs;
        g_Daemon.hkActSave          = ck.hkActSave;
        g_Daemon.hkActCopy          = ck.hkActCopy;
        g_Daemon.hkActSearchGoogle  = ck.hkActSearchGoogle;
        g_Daemon.hkFramePlayPause   = ck.hkFramePlayPause;
        g_Daemon.hkFrameToggleStrip = ck.hkFrameToggleStrip;
        g_Daemon.hkPinZoomOut       = ck.hkPinZoomOut;
        g_Daemon.hkPinZoomIn      = ck.hkPinZoomIn;
        g_Daemon.hkPinZoomReset   = ck.hkPinZoomReset;
        g_Daemon.hkPinPosReset    = ck.hkPinPosReset;
        g_Daemon.hkPinOutline     = ck.hkPinOutline;
        g_Daemon.hkPinHideToolbar = ck.hkPinHideToolbar;
        g_Daemon.hkPinSmooth      = ck.hkPinSmooth;
        g_Daemon.hkPinSaveAs     = ck.hkPinSaveAs;
        g_Daemon.hkPinSave       = ck.hkPinSave;
        g_Daemon.hkPinCopy       = ck.hkPinCopy;
        g_Daemon.hkPinSearchGoogle = ck.hkPinSearchGoogle;
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
        if (g_Daemon.hShortcutsWnd && IsWindow(g_Daemon.hShortcutsWnd)) {
            DestroyWindow(g_Daemon.hShortcutsWnd);
            g_Daemon.hShortcutsWnd = nullptr;
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
    if (footerTop == st->lastFooterTop) return;
    st->lastFooterTop = footerTop;

    if (st->hViewport) {
        SetWindowPos(st->hViewport, nullptr, 0, 0, clientW, footerTop, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    HDWP hdwp = BeginDeferWindowPos(4);
    if (hdwp) {
        if (st->hFooterBg) {
            hdwp = DeferWindowPos(hdwp, st->hFooterBg, HWND_TOP, 0, footerTop, clientW, st->footerHeight, SWP_NOACTIVATE);
        }
        if (st->hAppDataInfo) {
            hdwp = DeferWindowPos(hdwp, st->hAppDataInfo, HWND_TOP, 18, footerTop + 18, 296, 24, SWP_NOACTIVATE);
        }
        if (st->hOkBtn) {
            hdwp = DeferWindowPos(hdwp, st->hOkBtn, HWND_TOP, 318, footerTop + 11, 108, 34, SWP_NOACTIVATE);
        }
        if (st->hCancelBtn) {
            hdwp = DeferWindowPos(hdwp, st->hCancelBtn, HWND_TOP, 434, footerTop + 11, 100, 34, SWP_NOACTIVATE);
        }
        EndDeferWindowPos(hdwp);
    }
}

static void UpdateOptionsScroll(HWND hWnd, OptionsDlgState* st, int newScrollY, bool forceReposition = false) {
    if (!hWnd || !st) return;
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int clientH = std::max(1, (int)(rcClient.bottom - rcClient.top));
    int viewportH = std::max(1, clientH - st->footerHeight);
    int maxScroll = std::max(0, st->scrollContentHeight - viewportH);
    int clamped = std::max(0, std::min(newScrollY, maxScroll));
    int dy = st->scrollY - clamped;
    st->scrollY = clamped;

    LayoutOptionsFooter(hWnd, st);

    if ((dy != 0 || forceReposition) && !st->scrollControls.empty()) {
        HDWP hdwp = BeginDeferWindowPos((int)st->scrollControls.size());
        if (hdwp) {
            HWND hRef = st->hViewport ? st->hViewport : hWnd;
            for (const auto& item : st->scrollControls) {
                HWND hCh = item.first;
                int baseTop = item.second;
                if (!hCh || !IsWindow(hCh)) continue;
                RECT rcCh;
                if (GetWindowRect(hCh, &rcCh)) {
                    MapWindowPoints(HWND_DESKTOP, hRef, (LPPOINT)&rcCh, 2);
                    hdwp = DeferWindowPos(hdwp, hCh, nullptr, rcCh.left, baseTop - st->scrollY, 0, 0,
                                          SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                }
            }
            EndDeferWindowPos(hdwp);
        }
    }

    SCROLLINFO si = { sizeof(SCROLLINFO) };
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    si.nMin = 0;
    si.nMax = st->scrollContentHeight - 1;
    si.nPage = (UINT)viewportH;
    si.nPos = st->scrollY;
    SetScrollInfo(hWnd, SB_VERT, &si, TRUE);

    if (dy != 0 && st->hViewport) {
        RedrawWindow(st->hViewport, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
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
    std::wstring pat = GetWindowTextDynamic(st->hNamingEdit);
    int selFmt = st->hComboRegion ? (int)SendMessageW(st->hComboRegion, CB_GETCURSEL, 0, 0) : (int)st->regionFmt;
    if (selFmt < 0 || selFmt > 3) selFmt = (int)ImageFormat::PNG;
    std::wstring preview = L"Live preview:  " +
        PepperSnapDaemon::FormatFilenameWithPattern(pat, g_Daemon.captureCounter) +
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

static LRESULT CALLBACK OptionsViewportWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND:
        case WM_HSCROLL:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
        case WM_MOUSEWHEEL: {
            HWND hParent = GetParent(hWnd);
            if (hParent) {
                return SendMessageW(hParent, msg, wParam, lParam);
            }
            break;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static LRESULT CALLBACK OptionsDlgWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    OptionsDlgState* st = (OptionsDlgState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            st = (OptionsDlgState*)cs->lpCreateParams;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);
            if (!st) return 0;

            st->InitGdiCache();
            HFONT hFont = st->hFont;
            HFONT hBoldFont = st->hBoldFont;
            HFONT hLinkFont = st->hLinkFont;

            st->hViewport = CreateWindowExW(0, L"PepperSnapOptionsViewport", L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                0, 0, 552, 772, hWnd, nullptr, g_Daemon.hInst, st);
            HWND hVp = st->hViewport ? st->hViewport : hWnd;

            // Save Folder Location
            HWND hLblFolder = CreateWindowExW(0, L"STATIC", L"Save screenshot to...",
                WS_CHILD | WS_VISIBLE, 18, 14, 516, 22, hVp, nullptr, nullptr, nullptr);
            SendMessageW(hLblFolder, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            st->hFolderEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", st->folder.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 18, 40, 320, 26, hVp, (HMENU)IDC_OPT_FOLDER_EDIT, nullptr, nullptr);
            SendMessageW(st->hFolderEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hBrowse = CreateWindowExW(0, L"BUTTON", L"Browse...",
                WS_CHILD | WS_VISIBLE, 344, 39, 84, 28, hVp, (HMENU)IDC_OPT_FOLDER_BROWSE, nullptr, nullptr);
            SendMessageW(hBrowse, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hDeskDef = CreateWindowExW(0, L"BUTTON", L"Use desktop",
                WS_CHILD | WS_VISIBLE, 434, 39, 100, 28, hVp, (HMENU)IDC_OPT_FOLDER_DEFAULT, nullptr, nullptr);
            SendMessageW(hDeskDef, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Save Format & JPEG Quality Slider
            HWND hLblFormats = CreateWindowExW(0, L"STATIC", L"Save format",
                WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 18, 80, 516, 22, hVp, nullptr, nullptr, nullptr);
            SendMessageW(hLblFormats, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            HWND hLblCopy = CreateWindowExW(0, L"STATIC", L"Copy format:",
                WS_CHILD | WS_VISIBLE, 18, 108, 155, 22, hVp, nullptr, nullptr, nullptr);
            SendMessageW(hLblCopy, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hComboCopy = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 176, 104, 175, 140, hVp, (HMENU)IDC_OPT_COMBO_COPY, nullptr, nullptr);
            SendMessageW(st->hComboCopy, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hLblReg = CreateWindowExW(0, L"STATIC", L"Custom area format:",
                WS_CHILD | WS_VISIBLE, 18, 140, 155, 22, hVp, nullptr, nullptr, nullptr);
            SendMessageW(hLblReg, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hComboRegion = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 176, 136, 175, 140, hVp, (HMENU)IDC_OPT_COMBO_REGION, nullptr, nullptr);
            SendMessageW(st->hComboRegion, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hLblFull = CreateWindowExW(0, L"STATIC", L"Instant fullscreen format:",
                WS_CHILD | WS_VISIBLE, 18, 172, 155, 22, hVp, nullptr, nullptr, nullptr);
            SendMessageW(hLblFull, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hComboFull = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 176, 168, 175, 140, hVp, (HMENU)IDC_OPT_COMBO_FULL, nullptr, nullptr);
            SendMessageW(st->hComboFull, WM_SETFONT, (WPARAM)hFont, TRUE);

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
                WS_CHILD | WS_VISIBLE, 18, 206, 150, 22, hVp, nullptr, nullptr, nullptr);
            SendMessageW(st->hQualityValLbl, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hQualitySlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS, 172, 202, 362, 32, hVp, (HMENU)IDC_OPT_QUALITY_SLIDER, nullptr, nullptr);
            SendMessageW(st->hQualitySlider, TBM_SETRANGE, TRUE, MAKELONG(10, 100));
            SendMessageW(st->hQualitySlider, TBM_SETTICFREQ, 10, 0);
            SendMessageW(st->hQualitySlider, TBM_SETPOS, TRUE, st->jpgQuality);

            // Timestamp Naming Pattern + Restore Default Button + Syntax Info (ⓘ) Button
            HWND hLblNaming = CreateWindowExW(0, L"STATIC",
                L"Naming pattern:",
                WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 18, 244, 516, 22, hVp, nullptr, nullptr, nullptr);
            SendMessageW(hLblNaming, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            st->hNamingEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", st->naming.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 18, 270, 352, 26, hVp, (HMENU)IDC_OPT_NAMING_EDIT, nullptr, nullptr);
            SendMessageW(st->hNamingEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hRestoreNaming = CreateWindowExW(0, L"BUTTON", L"Restore default",
                WS_CHILD | WS_VISIBLE, 376, 269, 122, 28, hVp, (HMENU)IDC_OPT_NAMING_RESTORE, nullptr, nullptr);
            SendMessageW(hRestoreNaming, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hNamingSyntax = CreateWindowExW(0, L"BUTTON", L"\x24D8",
                WS_CHILD | WS_VISIBLE, 504, 269, 30, 28, hVp, (HMENU)IDC_OPT_NAMING_SYNTAX, nullptr, nullptr);
            SendMessageW(hNamingSyntax, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            st->hPreviewLbl = CreateWindowExW(0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 18, 302, 516, 22, hVp, nullptr, nullptr, nullptr);
            SendMessageW(st->hPreviewLbl, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Shortcut keys: [Customize keys] + [Keyboard shortcuts and info] buttons
            HWND hLblShortcuts = CreateWindowExW(0, L"STATIC",
                L"Shortcut keys:",
                WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 18, 338, 108, 24, hVp, nullptr, nullptr, nullptr);
            SendMessageW(hLblShortcuts, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            st->hCustomizeKeysBtn = CreateWindowExW(0, L"BUTTON", L"Customize keys",
                WS_CHILD | WS_VISIBLE, 130, 334, 136, 28, hVp, (HMENU)IDC_OPT_CUSTOMIZE_KEYS, nullptr, nullptr);
            SendMessageW(st->hCustomizeKeysBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hShortcutsInfoBtn = CreateWindowExW(0, L"BUTTON", L"Keyboard shortcuts and info",
                WS_CHILD | WS_VISIBLE, 274, 334, 210, 28, hVp, (HMENU)IDC_OPT_SHORTCUTS_INFO, nullptr, nullptr);
            SendMessageW(st->hShortcutsInfoBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Toggles at Bottom of Option Window (below Shortcut keys row):
            // Check for updates (GitHub Releases)
            st->hAutoUpdateChk = CreateWindowExW(0, L"BUTTON",
                L"Automatically check for updates (GitHub releases):",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 376, 310, 24, hVp, (HMENU)IDC_OPT_AUTOUPDATE_CHK, nullptr, nullptr);
            SendMessageW(st->hAutoUpdateChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hAutoUpdateChk, BM_SETCHECK, st->autoUpdateEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

            st->hComboUpdateInterval = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 332, 374, 98, 160, hVp, (HMENU)IDC_OPT_UPDATE_INTERVAL, nullptr, nullptr);
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
                WS_CHILD | WS_VISIBLE, 436, 373, 98, 28, hVp, (HMENU)IDC_OPT_CHECK_UPDATE_NOW, nullptr, nullptr);
            SendMessageW(st->hCheckUpdateNowBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Non-Stacking Highlighter
            st->hNonStackingChk = CreateWindowExW(0, L"BUTTON",
                L"Non-stacking highlighter (prevent overlapping highlighter strokes from darkening)",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 408, 516, 24, hVp, (HMENU)IDC_OPT_NONSTACK_CHK, nullptr, nullptr);
            SendMessageW(st->hNonStackingChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hNonStackingChk, BM_SETCHECK, st->nonStackingHi ? BST_CHECKED : BST_UNCHECKED, 0);

            // Also save screenshot when copying custom area
            st->hAutoSaveChk = CreateWindowExW(0, L"BUTTON",
                L"Also save screenshot when copying custom area",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 436, 516, 24, hVp, (HMENU)IDC_OPT_AUTOSAVE_CHK, nullptr, nullptr);
            SendMessageW(st->hAutoSaveChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hAutoSaveChk, BM_SETCHECK, st->autoSaveCopy ? BST_CHECKED : BST_UNCHECKED, 0);

            // Also copy instant fullscreen
            st->hCopyFullChk = CreateWindowExW(0, L"BUTTON",
                L"Also copy instant fullscreen",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 464, 516, 24, hVp, (HMENU)IDC_OPT_NOCOPY_SAVE_CHK, nullptr, nullptr);
            SendMessageW(st->hCopyFullChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hCopyFullChk, BM_SETCHECK, st->alsoCopyFull ? BST_CHECKED : BST_UNCHECKED, 0);

            // Also save pinned screenshot
            st->hSavePinChk = CreateWindowExW(0, L"BUTTON",
                L"Also save pinned screenshot",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 492, 516, 24, hVp, (HMENU)IDC_OPT_SAVE_PIN_CHK, nullptr, nullptr);
            SendMessageW(st->hSavePinChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hSavePinChk, BM_SETCHECK, st->alsoSavePin ? BST_CHECKED : BST_UNCHECKED, 0);

            // Also save copied text
            st->hSaveOcrChk = CreateWindowExW(0, L"BUTTON",
                L"Also save copied text",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 520, 516, 24, hVp, (HMENU)IDC_OPT_SAVE_OCR_CHK, nullptr, nullptr);
            SendMessageW(st->hSaveOcrChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hSaveOcrChk, BM_SETCHECK, st->alsoSaveOcr ? BST_CHECKED : BST_UNCHECKED, 0);

            // Keep cursor on screenshot
            st->hKeepCursorChk = CreateWindowExW(0, L"BUTTON",
                L"Keep cursor on screenshot",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 548, 516, 24, hVp, (HMENU)IDC_OPT_KEEP_CURSOR_CHK, nullptr, nullptr);
            SendMessageW(st->hKeepCursorChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hKeepCursorChk, BM_SETCHECK, st->keepCursor ? BST_CHECKED : BST_UNCHECKED, 0);

            // Auto-hide frame list
            st->hAutoHideFramesChk = CreateWindowExW(0, L"BUTTON",
                L"Auto-hide frame list",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 576, 516, 24, hVp, (HMENU)IDC_OPT_AUTOHIDE_FRM_CHK, nullptr, nullptr);
            SendMessageW(st->hAutoHideFramesChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hAutoHideFramesChk, BM_SETCHECK, st->autoHideFrames ? BST_CHECKED : BST_UNCHECKED, 0);

            // Put pinned image above edit window (above Enable pen and highlighter smoothing)
            st->hPinAboveEditChk = CreateWindowExW(0, L"BUTTON",
                L"Put pinned image above edit window",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 604, 516, 24, hVp, (HMENU)IDC_OPT_PIN_ABOVE_EDIT_CHK, nullptr, nullptr);
            SendMessageW(st->hPinAboveEditChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hPinAboveEditChk, BM_SETCHECK, st->pinAboveEdit ? BST_CHECKED : BST_UNCHECKED, 0);

            // Keep edit window open after save and copy
            st->hKeepEditOpenChk = CreateWindowExW(0, L"BUTTON",
                L"Keep edit window open after save and copy",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 632, 516, 24, hVp, (HMENU)IDC_OPT_KEEP_EDIT_OPEN_CHK, nullptr, nullptr);
            SendMessageW(st->hKeepEditOpenChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hKeepEditOpenChk, BM_SETCHECK, st->keepEditOpen ? BST_CHECKED : BST_UNCHECKED, 0);

            // Run as administrator (enable if shortcut doesnt work or fails to capture)
            st->hRunElevatedChk = CreateWindowExW(0, L"BUTTON",
                L"Run as administrator (enable if shortcut doesnt work or fails to capture)",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 660, 516, 24, hVp, (HMENU)IDC_OPT_RUN_ELEVATED_CHK, nullptr, nullptr);
            SendMessageW(st->hRunElevatedChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hRunElevatedChk, BM_SETCHECK, st->runElevated ? BST_CHECKED : BST_UNCHECKED, 0);

            // Disable right-click integration
            st->hDisableRightClickChk = CreateWindowExW(0, L"BUTTON",
                L"Disable right-click integration",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 688, 516, 24, hVp, (HMENU)IDC_OPT_DISABLE_RIGHT_CLICK_CHK, nullptr, nullptr);
            SendMessageW(st->hDisableRightClickChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hDisableRightClickChk, BM_SETCHECK, st->disableRightClickIntegration ? BST_CHECKED : BST_UNCHECKED, 0);

            // Disable magnifier window
            st->hDisableMagnifierChk = CreateWindowExW(0, L"BUTTON",
                L"Disable magnifier window",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 716, 516, 24, hVp, (HMENU)IDC_OPT_DISABLE_MAGNIFIER_CHK, nullptr, nullptr);
            SendMessageW(st->hDisableMagnifierChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hDisableMagnifierChk, BM_SETCHECK, st->disableMagnifierWindow ? BST_CHECKED : BST_UNCHECKED, 0);

            // Enable pen and highlighter smoothing.
            st->hPenSmoothChk = CreateWindowExW(0, L"BUTTON",
                L"Enable pen and highlighter smoothing.",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 744, 516, 24, hVp, (HMENU)IDC_OPT_PENSMOOTH_CHK, nullptr, nullptr);
            SendMessageW(st->hPenSmoothChk, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessageW(st->hPenSmoothChk, BM_SETCHECK, st->penSmoothEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

            std::wstring psText = FormatPenSmoothLabel(st->penSmoothEnabled, st->penSmoothStrength);
            st->hPenSmoothValLbl = CreateWindowExW(0, L"STATIC", psText.c_str(),
                WS_CHILD | WS_VISIBLE, 18, 776, 224, 22, hVp, nullptr, nullptr, nullptr);
            SendMessageW(st->hPenSmoothValLbl, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->hPenSmoothSlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS, 244, 772, 290, 32, hVp, (HMENU)IDC_OPT_PENSMOOTH_SLIDER, nullptr, nullptr);
            SendMessageW(st->hPenSmoothSlider, TBM_SETRANGE, TRUE, MAKELONG(5, 100));
            SendMessageW(st->hPenSmoothSlider, TBM_SETTICFREQ, 10, 0);
            SendMessageW(st->hPenSmoothSlider, TBM_SETPOS, TRUE, st->penSmoothStrength);
            EnableWindow(st->hPenSmoothSlider, st->penSmoothEnabled ? TRUE : FALSE);

            // Restore everything to default button (below smoothing strength, NOT in row with Save options)
            st->hRestoreAllBtn = CreateWindowExW(0, L"BUTTON", L"Restore everything to default",
                WS_CHILD | WS_VISIBLE, 18, 812, 218, 30, hVp, (HMENU)IDC_OPT_RESTORE_ALL_DEF, nullptr, nullptr);
            SendMessageW(st->hRestoreAllBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->scrollControls.clear();
            for (HWND hCh = GetWindow(hVp, GW_CHILD); hCh != nullptr; hCh = GetWindow(hCh, GW_HWNDNEXT)) {
                LONG stBits = GetWindowLongW(hCh, GWL_STYLE);
                SetWindowLongW(hCh, GWL_STYLE, stBits | WS_CLIPSIBLINGS);
                RECT rcCh;
                if (GetWindowRect(hCh, &rcCh)) {
                    MapWindowPoints(HWND_DESKTOP, hVp, (LPPOINT)&rcCh, 2);
                    st->scrollControls.push_back({ hCh, rcCh.top });
                }
            }

            // Permanent pinned footer bar: clickable %appdata%\PepperSnap persistence link + Save options + Cancel
            st->hFooterBg = CreateWindowExW(0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 856, 552, 56, hWnd, nullptr, nullptr, nullptr);

            st->hAppDataInfo = CreateWindowExW(0, L"STATIC",
                L"\x24D8 settings.ini is saved in %appdata%\\PepperSnap",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_NOPREFIX | SS_LEFTNOWORDWRAP | SS_NOTIFY, 18, 846, 296, 24, hWnd, (HMENU)IDC_OPT_OPEN_APPDATA, nullptr, nullptr);
            SendMessageW(st->hAppDataInfo, WM_SETFONT, (WPARAM)hLinkFont, TRUE);
            WNDPROC origStaticProc = (WNDPROC)SetWindowLongPtrW(st->hAppDataInfo, GWLP_WNDPROC, (LONG_PTR)AppDataLinkStaticWndProc);
            SetPropW(st->hAppDataInfo, L"PepperSnapOrigStaticProc", (HANDLE)origStaticProc);

            st->hOkBtn = CreateWindowExW(0, L"BUTTON", L"Save options",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_DEFPUSHBUTTON, 318, 839, 108, 34, hWnd, (HMENU)IDOK, nullptr, nullptr);
            SendMessageW(st->hOkBtn, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

            st->hCancelBtn = CreateWindowExW(0, L"BUTTON", L"Cancel",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 434, 839, 100, 34, hWnd, (HMENU)IDCANCEL, nullptr, nullptr);
            SendMessageW(st->hCancelBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->scrollContentHeight = 856;
            st->footerHeight = 56;
            st->lastFooterTop = -1;
            UpdateOptionsPreviewLabel(st);
            LayoutOptionsFooter(hWnd, st);
            UpdateOptionsScroll(hWnd, st, 0, false);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
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
                if (st->hFooterLinePen) {
                    HGDIOBJ hOldPen = SelectObject(hdc, st->hFooterLinePen);
                    MoveToEx(hdc, 0, footerTop, nullptr);
                    LineTo(hdc, clientW, footerTop);
                    SelectObject(hdc, hOldPen);
                }
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
                UpdateOptionsScroll(hWnd, st, st->scrollY, false);
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
                UpdateOptionsScroll(hWnd, st, target, false);
                return 0;
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            if (st) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (delta != 0) {
                    int step = -(delta * 48) / WHEEL_DELTA;
                    UpdateOptionsScroll(hWnd, st, st->scrollY + step, false);
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
                if (st->hFooterLinePen) {
                    HGDIOBJ hOldPen = SelectObject(hdcStatic, st->hFooterLinePen);
                    MoveToEx(hdcStatic, 0, 0, nullptr);
                    LineTo(hdcStatic, rcF.right, 0);
                    SelectObject(hdcStatic, hOldPen);
                }
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
            if (id == IDC_OPT_SHORTCUTS_INFO) {
                g_Daemon.ShowShortcutsModal();
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

                st->regionFmt = ImageFormat::PNG;
                st->fullFmt   = ImageFormat::JPEG;
                st->copyFmt   = ImageFormat::PNG;
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
                st->hkPinClipboard  = { MOD_ALT | MOD_SHIFT, VK_SNAPSHOT };
                st->hkEditClipboard = { MOD_CONTROL | MOD_ALT, VK_SNAPSHOT };
                st->hkSearchGoogle  = { MOD_ALT, VK_SNAPSHOT };
                st->hkToolSelect    = { 0, 0 };
                st->hkToolPen       = { 0, 0 };
                st->hkToolStabilo   = { 0, 0 };
                st->hkToolLine      = { 0, 0 };
                st->hkToolArrow     = { 0, 0 };
                st->hkToolNumber    = { 0, 0 };
                st->hkToolResetNum  = { 0, 0 };
                st->hkToolRect      = { 0, 0 };
                st->hkToolEllipse   = { 0, 0 };
                st->hkToolText      = { 0, 0 };
                st->hkToolMosaicSq  = { 0, 0 };
                st->hkToolMosaicCir = { 0, 0 };
                st->hkActUndo          = { MOD_CONTROL, 'Z' };
                st->hkActRedo          = { MOD_CONTROL, 'Y' };
                st->hkActClear         = { MOD_CONTROL, 'D' };
                st->hkActPin           = { 0, 0 };
                st->hkActOcr           = { 0, 0 };
                st->hkActSaveAs        = { MOD_CONTROL | MOD_SHIFT, 'S' };
                st->hkActSave          = { MOD_CONTROL, 'S' };
                st->hkActCopy          = { MOD_CONTROL, 'C' };
                st->hkActSearchGoogle  = { 0, 0 };
                st->hkFramePlayPause   = { 0, VK_SPACE };
                st->hkFrameToggleStrip = { 0, 0 };
                st->hkPinZoomOut       = { 0, 0 };
                st->hkPinZoomIn     = { 0, 0 };
                st->hkPinZoomReset   = { 0, 0 };
                st->hkPinPosReset    = { 0, 0 };
                st->hkPinOutline     = { 0, 0 };
                st->hkPinHideToolbar = { 0, 0 };
                st->hkPinSmooth      = { 0, 0 };
                st->hkPinSaveAs     = { MOD_CONTROL | MOD_SHIFT, 'S' };
                st->hkPinSave       = { MOD_CONTROL, 'S' };
                st->hkPinCopy       = { MOD_CONTROL, 'C' };
                st->hkPinSearchGoogle = { 0, 0 };
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
                st->keepCursor     = false;
                st->autoHideFrames = true;
                st->pinAboveEdit   = true;
                st->keepEditOpen   = false;
                st->runElevated    = true;
                st->disableRightClickIntegration = false;
                st->disableMagnifierWindow = false;
                SendMessageW(st->hNonStackingChk,    BM_SETCHECK, BST_CHECKED, 0);
                SendMessageW(st->hAutoSaveChk,       BM_SETCHECK, BST_CHECKED, 0);
                SendMessageW(st->hCopyFullChk,       BM_SETCHECK, BST_CHECKED, 0);
                SendMessageW(st->hSavePinChk,        BM_SETCHECK, BST_CHECKED, 0);
                SendMessageW(st->hSaveOcrChk,        BM_SETCHECK, BST_CHECKED, 0);
                SendMessageW(st->hKeepCursorChk,     BM_SETCHECK, BST_UNCHECKED, 0);
                SendMessageW(st->hAutoHideFramesChk, BM_SETCHECK, BST_CHECKED, 0);
                SendMessageW(st->hPinAboveEditChk,   BM_SETCHECK, BST_CHECKED, 0);
                SendMessageW(st->hKeepEditOpenChk,   BM_SETCHECK, BST_UNCHECKED, 0);
                SendMessageW(st->hRunElevatedChk,    BM_SETCHECK, BST_CHECKED, 0);
                SendMessageW(st->hDisableRightClickChk, BM_SETCHECK, BST_UNCHECKED, 0);
                SendMessageW(st->hDisableMagnifierChk, BM_SETCHECK, BST_UNCHECKED, 0);

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
                std::wstring iniPath = PepperSnapDaemon::GetAppDataSettingsFile();
                std::wstring appDataDir = PepperSnapDaemon::GetAppDataSettingsDir();
                CreateDirectoryW(appDataDir.c_str(), nullptr);
                if (GetFileAttributesW(iniPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
                    g_Daemon.SaveSettings();
                }
                SetWindowLongPtrW(hWnd, GWL_EXSTYLE, GetWindowLongPtrW(hWnd, GWL_EXSTYLE) & ~WS_EX_TOPMOST);
                SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
                std::wstring param = L"/select,\"" + iniPath + L"\"";
                ShellExecuteW(nullptr, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);
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
                    std::wstring curFolder = GetWindowTextDynamic(st->hFolderEdit);
                    if (!curFolder.empty()) {
                        IShellItem* psiFolder = nullptr;
                        if (SUCCEEDED(SHCreateItemFromParsingName(curFolder.c_str(), nullptr, IID_IShellItem, (void**)&psiFolder)) && psiFolder) {
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
                std::wstring fStr = GetWindowTextDynamic(st->hFolderEdit);
                if (!fStr.empty()) st->folder = fStr;

                std::wstring nStr = GetWindowTextDynamic(st->hNamingEdit);
                st->naming = !nStr.empty() ? nStr : PepperSnapDaemon::DEFAULT_NAMING_PATTERN;

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
                st->keepCursor     = (SendMessageW(st->hKeepCursorChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->autoHideFrames = (SendMessageW(st->hAutoHideFramesChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->pinAboveEdit   = (SendMessageW(st->hPinAboveEditChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->keepEditOpen   = (SendMessageW(st->hKeepEditOpenChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->runElevated    = (SendMessageW(st->hRunElevatedChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->disableRightClickIntegration = (SendMessageW(st->hDisableRightClickChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->disableMagnifierWindow = (SendMessageW(st->hDisableMagnifierChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
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
            if (st) {
                st->FreeGdiCache();
            }
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
        WNDCLASSEXW wcVp = { sizeof(WNDCLASSEXW) };
        wcVp.lpfnWndProc = OptionsViewportWndProc;
        wcVp.hInstance = hInst;
        wcVp.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wcVp.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wcVp.lpszClassName = L"PepperSnapOptionsViewport";
        RegisterClassExW(&wcVp);

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
    st.keepCursor     = keepCursorOnScreenshot;
    st.autoHideFrames = autoHideFrameList;
    st.pinAboveEdit   = pinnedImageAboveEdit;
    st.keepEditOpen   = keepEditWindowOpen;
    st.runElevated    = runElevated;
    st.disableRightClickIntegration = disableRightClickIntegration;
    st.disableMagnifierWindow = disableMagnifierWindow;
    st.nonStackingHi  = nonStackingHighlighter;
    st.penSmoothEnabled = penSmoothingEnabled;
    st.penSmoothStrength = penSmoothingStrength;
    st.autoUpdateEnabled = autoCheckUpdates;
    st.updateInterval = updateInterval;
    st.hkRegion        = hkRegionSnip;
    st.hkFull          = hkFullSnap;
    st.hkPrev          = hkPrevRegion;
    st.hkPinClipboard  = hkPinClipboard;
    st.hkEditClipboard = hkEditClipboard;
    st.hkSearchGoogle  = hkSearchGoogle;
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
    st.hkActPin           = hkActPin;
    st.hkActOcr           = hkActOcr;
    st.hkActSaveAs        = hkActSaveAs;
    st.hkActSave          = hkActSave;
    st.hkActCopy          = hkActCopy;
    st.hkActSearchGoogle  = hkActSearchGoogle;
    st.hkFramePlayPause   = hkFramePlayPause;
    st.hkFrameToggleStrip = hkFrameToggleStrip;
    st.hkPinZoomOut       = hkPinZoomOut;
    st.hkPinZoomIn     = hkPinZoomIn;
    st.hkPinZoomReset   = hkPinZoomReset;
    st.hkPinPosReset    = hkPinPosReset;
    st.hkPinOutline     = hkPinOutline;
    st.hkPinHideToolbar = hkPinHideToolbar;
    st.hkPinSmooth      = hkPinSmooth;
    st.hkPinSaveAs     = hkPinSaveAs;
    st.hkPinSave       = hkPinSave;
    st.hkPinCopy       = hkPinCopy;
    st.hkPinSearchGoogle = hkPinSearchGoogle;
    st.hkPinClose      = hkPinClose;

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    DWORD dwOptStyle = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VSCROLL | WS_VISIBLE | WS_CLIPCHILDREN;
    DWORD dwOptExStyle = 0;
    const int totalOptClientH = 856 + 56;
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
    while (IsWindow(hDlg) && PepperSnapDaemon::GetMessageKeepQuit(&msg)) {
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
        pinnedImageAboveEdit = st.pinAboveEdit;
        keepEditWindowOpen = st.keepEditOpen;
        runElevated = st.runElevated;
        disableRightClickIntegration = st.disableRightClickIntegration;
        disableMagnifierWindow = st.disableMagnifierWindow;
        nonStackingHighlighter = st.nonStackingHi;
        penSmoothingEnabled = st.penSmoothEnabled;
        penSmoothingStrength = st.penSmoothStrength;
        autoCheckUpdates = st.autoUpdateEnabled;
        updateInterval = st.updateInterval;
        hkRegionSnip    = st.hkRegion;
        hkFullSnap      = st.hkFull;
        hkPrevRegion    = st.hkPrev;
        hkPinClipboard  = st.hkPinClipboard;
        hkEditClipboard = st.hkEditClipboard;
        hkSearchGoogle  = st.hkSearchGoogle;
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
        hkActPin           = st.hkActPin;
        hkActOcr           = st.hkActOcr;
        hkActSaveAs        = st.hkActSaveAs;
        hkActSave          = st.hkActSave;
        hkActCopy          = st.hkActCopy;
        hkActSearchGoogle  = st.hkActSearchGoogle;
        hkFramePlayPause   = st.hkFramePlayPause;
        hkFrameToggleStrip = st.hkFrameToggleStrip;
        hkPinZoomOut       = st.hkPinZoomOut;
        hkPinZoomIn     = st.hkPinZoomIn;
        hkPinZoomReset   = st.hkPinZoomReset;
        hkPinPosReset    = st.hkPinPosReset;
        hkPinOutline     = st.hkPinOutline;
        hkPinHideToolbar = st.hkPinHideToolbar;
        hkPinSmooth      = st.hkPinSmooth;
        hkPinSaveAs      = st.hkPinSaveAs;
        hkPinSave        = st.hkPinSave;
        hkPinCopy        = st.hkPinCopy;
        hkPinSearchGoogle = st.hkPinSearchGoogle;
        hkPinClose       = st.hkPinClose;
        if (st.restoredAllDefaults) {
            hidePinnedToolbar = false;
            hidePinnedOutline = false;
            smoothPinnedImage = false;
            pinnedImageAboveEdit = true;
            keepEditWindowOpen = false;
            runElevated = true;
            disableRightClickIntegration = false;
            disableMagnifierWindow = false;
            activeColor = Color(255, 239, 68, 68);
            activeStroke = 4.0f;
        }
        ApplyGlobalHotkeys();
        CreateDirectoryW(saveFolder.c_str(), nullptr);
        SaveSettings();
        if (hOverlayWnd && IsWindow(hOverlayWnd)) {
            if (IsEditingOpenedImage()) {
                if (pinnedImageAboveEdit) {
                    RaisePinnedWindowsAboveOverlay();
                } else {
                    HidePinnedBubbleTooltip();
                    SetWindowPos(hOverlayWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                }
            }
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

bool EnableWindowsClipboardHistory() {
    ScopedHKey hKey;
    DWORD dwDisp = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Clipboard", 0, nullptr, 0, KEY_WRITE, nullptr, hKey.put(), &dwDisp) == ERROR_SUCCESS) {
        DWORD val = 1;
        if (RegSetValueExW(hKey, L"EnableClipboardHistory", 0, REG_DWORD, (const BYTE*)&val, sizeof(val)) == ERROR_SUCCESS) {
            DWORD_PTR dwResult = 0;
            SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Clipboard", SMTO_ABORTIFHUNG, 1000, &dwResult);
            return true;
        }
    }
    return false;
}

namespace {
struct ClipHistoryDlgState {
    HWND hDoNotAskChk = nullptr;
    HWND hTurnOnBtn = nullptr;
    HWND hKeepOffBtn = nullptr;
    HFONT hTitleFont = nullptr;
    HFONT hSubFont = nullptr;
    HFONT hFont = nullptr;
    HFONT hBoldFont = nullptr;
    bool turnOn = false;
    bool doNotAsk = false;
};

LRESULT CALLBACK ClipHistoryPromptWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ClipHistoryDlgState* st = (ClipHistoryDlgState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            st = (ClipHistoryDlgState*)cs->lpCreateParams;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);

            st->hTitleFont = CreateFontW(-19, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            st->hSubFont = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            st->hFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            st->hBoldFont = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            HWND hTitle = CreateWindowExW(0, L"STATIC", L"Clipboard history is disabled",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 24, 20, 392, 26, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hTitle, WM_SETFONT, (WPARAM)st->hTitleFont, TRUE);

            HWND hSub = CreateWindowExW(0, L"STATIC", L"It is recommended to turn it on",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 24, 48, 392, 22, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hSub, WM_SETFONT, (WPARAM)st->hSubFont, TRUE);

            HWND hDesc = CreateWindowExW(0, L"STATIC",
                L"Turning on Windows Clipboard History (Win + V) allows PepperSnap and Windows to preserve your recent captures and copied snippets.",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 24, 76, 392, 60, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hDesc, WM_SETFONT, (WPARAM)st->hFont, TRUE);

            st->hDoNotAskChk = CreateWindowExW(0, L"BUTTON", L"Do not ask again",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 24, 146, 160, 26, hWnd, (HMENU)101, nullptr, nullptr);
            SendMessageW(st->hDoNotAskChk, WM_SETFONT, (WPARAM)st->hFont, TRUE);

            st->hTurnOnBtn = CreateWindowExW(0, L"BUTTON", L"Turn on",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 206, 142, 106, 34, hWnd, (HMENU)IDOK, nullptr, nullptr);
            SendMessageW(st->hTurnOnBtn, WM_SETFONT, (WPARAM)st->hBoldFont, TRUE);

            st->hKeepOffBtn = CreateWindowExW(0, L"BUTTON", L"Keep it off",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 320, 142, 106, 34, hWnd, (HMENU)IDCANCEL, nullptr, nullptr);
            SendMessageW(st->hKeepOffBtn, WM_SETFONT, (WPARAM)st->hFont, TRUE);

            SetFocus(st->hTurnOnBtn);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDOK) {
                if (st) {
                    st->turnOn = true;
                    st->doNotAsk = (SendMessageW(st->hDoNotAskChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                }
                DestroyWindow(hWnd);
                return 0;
            }
            if (id == IDCANCEL) {
                if (st) {
                    st->turnOn = false;
                    st->doNotAsk = (SendMessageW(st->hDoNotAskChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                }
                DestroyWindow(hWnd);
                return 0;
            }
            break;
        }
        case WM_CLOSE: {
            if (st) {
                st->turnOn = false;
                st->doNotAsk = (SendMessageW(st->hDoNotAskChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }
            DestroyWindow(hWnd);
            return 0;
        }
        case WM_DESTROY: {
            // Deliberately no PostQuitMessage(0). The loop that runs this dialog
            // ends when the window is gone, so a WM_QUIT posted here would never
            // be consumed: it would stay in the thread queue and the caller's own
            // loop (PepperSnap.cpp, immediately after CheckAndPromptClipboardHistory)
            // would pick it up and shut the whole app down the moment the user
            // answered. Losing that message cost a build where "Turn on" and
            // "Keep it off" both quit PepperSnap.
            return 0;
        }
        case WM_NCDESTROY: {
            if (st) {
                if (st->hTitleFont) DeleteObject(st->hTitleFont);
                if (st->hSubFont) DeleteObject(st->hSubFont);
                if (st->hFont) DeleteObject(st->hFont);
                if (st->hBoldFont) DeleteObject(st->hBoldFont);
            }
            break;
        }
        default: break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // namespace

void PepperSnapDaemon::CheckAndPromptClipboardHistory() {
    if (!promptClipboardHistory) return;
    if (IsWindowsClipboardHistoryEnabled()) return;

    static bool s_ClassRegistered = false;
    if (!s_ClassRegistered) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = ClipHistoryPromptWndProc;
        wc.hInstance = hInst;
        wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APPICON));
        if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"PepperSnapClipHistoryPromptClass";
        RegisterClassExW(&wc);
        s_ClassRegistered = true;
    }

    ClipHistoryDlgState st;
    DWORD dwStyle = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    DWORD dwExStyle = WS_EX_TOPMOST;
    RECT rcWin = { 0, 0, 450, 196 };
    AdjustWindowRectEx(&rcWin, dwStyle, FALSE, dwExStyle);
    int winW = rcWin.right - rcWin.left;
    int winH = rcWin.bottom - rcWin.top;
    int posX = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
    int posY = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;

    HWND hWnd = CreateWindowExW(
        dwExStyle,
        L"PepperSnapClipHistoryPromptClass",
        L"Clipboard history is disabled",
        dwStyle,
        posX, posY, winW, winH,
        nullptr, nullptr, hInst, &st
    );
    if (!hWnd) return;

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);
    SetForegroundWindow(hWnd);

    MSG msg;
    while (IsWindow(hWnd) && PepperSnapDaemon::GetMessageKeepQuit(&msg)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), 0);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (st.turnOn) {
        EnableWindowsClipboardHistory();
        ShowTrayToast(
            L"Clipboard history enabled",
            L"Windows Clipboard History (Win + V) has been enabled."
        );
    } else if (st.doNotAsk) {
        promptClipboardHistory = false;
        SaveSettings();
    }
}

