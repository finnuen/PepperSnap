#include "PepperSnapInternal.h"

bool RegQueryStringValueDynamic(HKEY hKey, const wchar_t* subKey, const wchar_t* valueName, std::wstring& outVal) {
    outVal.clear();
    DWORD cbData = 0;
    LSTATUS st = RegGetValueW(hKey, subKey, valueName, RRF_RT_REG_SZ, nullptr, nullptr, &cbData);
    if (st != ERROR_SUCCESS || cbData < sizeof(wchar_t)) return false;
    std::vector<WCHAR> buf((cbData / sizeof(wchar_t)) + 2, L'\0');
    DWORD cbActual = (DWORD)(buf.size() * sizeof(wchar_t));
    st = RegGetValueW(hKey, subKey, valueName, RRF_RT_REG_SZ, nullptr, buf.data(), &cbActual);
    while (st == ERROR_MORE_DATA && buf.size() <= 32768) {
        buf.resize(buf.size() * 2, L'\0');
        cbActual = (DWORD)(buf.size() * sizeof(wchar_t));
        st = RegGetValueW(hKey, subKey, valueName, RRF_RT_REG_SZ, nullptr, buf.data(), &cbActual);
    }
    if (st != ERROR_SUCCESS) return false;
    outVal.assign(buf.data(), wcsnlen(buf.data(), buf.size()));
    return true;
}

void RegEnumSubKeyNamesDynamic(HKEY hKey, std::vector<std::wstring>& outNames) {
    outNames.clear();
    DWORD maxSubKeyLen = 256;
    RegQueryInfoKeyW(hKey, nullptr, nullptr, nullptr, nullptr, &maxSubKeyLen, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    size_t cap = std::max<size_t>(256, (size_t)maxSubKeyLen + 2);
    std::vector<WCHAR> subName(cap, L'\0');
    for (DWORD idx = 0;; ++idx) {
        DWORD subLen = (DWORD)subName.size();
        LSTATUS st = RegEnumKeyExW(hKey, idx, subName.data(), &subLen, nullptr, nullptr, nullptr, nullptr);
        while (st == ERROR_MORE_DATA && subName.size() <= 32768) {
            subName.resize(subName.size() * 2, L'\0');
            subLen = (DWORD)subName.size();
            st = RegEnumKeyExW(hKey, idx, subName.data(), &subLen, nullptr, nullptr, nullptr, nullptr);
        }
        if (st != ERROR_SUCCESS) break;
        outNames.emplace_back(subName.data(), subLen);
    }
}


// ---------------------------------------------------------------------------
// Launch at Windows startup
//
// This is a plain HKCU\...\Run entry, and it stays that way on purpose.
//
// Windows starts Run entries with the user's filtered (non-elevated) token, so
// a Run entry followed by the usual self-elevation means a UAC prompt at logon.
// That is annoying, and the tempting fixes - a Task Scheduler logon task with
// RunLevel=HighestAvailable, and/or the same task driven on demand via
// `schtasks /Run` so a manual launch never asks either - DO NOT WORK here. Both
// were built and shipped into a test build, and Microsoft Defender flagged the
// resulting executable:
//
//     Trojan:Win32/Bearfoos.A!ml     (Alert level: Severe)
//
// The detection was behavioural, not merely string-based: Defender attached it
// to the exe *and* to C:\Windows\System32\Tasks\PepperSnap plus its
// TaskCache registry keys - that is, to the act of registering an elevated task
// from an unsigned portable binary. That is exactly the pattern this family is
// defined by, so no renaming or restructuring makes it safe: it would only be
// evasion, and it would still be caught. Do not reintroduce a scheduled task, a
// service, or an IFEO / silent-exit trick for this. Live with the prompt.
//
// The one legitimate reduction is in PepperSnap.cpp: when a daemon is already
// running elevated, a second launch hands its request to that instance instead
// of re-launching, so no elevation is requested at all.
// ---------------------------------------------------------------------------

bool PepperSnapDaemon::IsRunAtStartupEnabled() const {
    ScopedHKey hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, hKey.put()) != ERROR_SUCCESS) {
        return false;
    }
    std::wstring val;
    return RegQueryStringValueDynamic(hKey, nullptr, L"PepperSnap", val) && !val.empty();
}

void PepperSnapDaemon::ToggleRunAtStartup() {
    ScopedHKey hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, hKey.put()) != ERROR_SUCCESS) {
        return;
    }
    if (IsRunAtStartupEnabled()) {
        RegDeleteValueW(hKey, L"PepperSnap");
        ShowTrayToast(L"Startup disabled", L"PepperSnap will no longer launch automatically at login.");
    } else {
        std::wstring exePath = GetModuleFilePathDynamic(nullptr);
        if (exePath.empty()) return;
        std::wstring quoted = L"\"" + exePath + L"\"";
        RegSetValueExW(hKey, L"PepperSnap", 0, REG_SZ, (const BYTE*)quoted.c_str(), (DWORD)((quoted.size() + 1) * sizeof(wchar_t)));
        ShowTrayToast(L"Startup enabled", L"PepperSnap will start in the system tray when Windows boots.");
    }
}

void PepperSnapDaemon::RegisterImageContextMenu() {
    std::wstring exePath = GetModuleFilePathDynamic(nullptr);
    if (exePath.empty()) return;

    std::wstring editLabel   = L"Edit with PepperSnap";
    std::wstring searchLabel = L"Search image on web";
    std::wstring pinLabel    = L"Pin on top";
    std::wstring iconVal     = L"\"" + exePath + L"\",0";
    std::wstring editCmd     = L"\"" + exePath + L"\" --edit \"%1\"";
    std::wstring searchCmd   = L"\"" + exePath + L"\" --search \"%1\"";
    std::wstring pinCmd      = L"\"" + exePath + L"\" --pin \"%1\"";

    // If "Launch PepperSnap at Windows startup" is already enabled by the user in HKCU, keep its path updated to this executable
    {
        ScopedHKey hRunKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ | KEY_WRITE, hRunKey.put()) == ERROR_SUCCESS) {
            std::wstring runVal;
            if (RegQueryStringValueDynamic(hRunKey, nullptr, L"PepperSnap", runVal) && !runVal.empty()) {
                std::wstring desired = L"\"" + exePath + L"\"";
                if (_wcsicmp(runVal.c_str(), desired.c_str()) != 0) {
                    RegSetValueExW(hRunKey, L"PepperSnap", 0, REG_SZ, (const BYTE*)desired.c_str(), (DWORD)((desired.size() + 1) * sizeof(wchar_t)));
                }
            }
        }
    }

    // Helper: check whether a shell verb subkey's command value explicitly points at this executable's own path.
    // Why: ensures legacy cleanup only removes verbs previously registered by this exact executable and never touches other apps' entries.
    auto commandPointsToThisExe = [&](HKEY hShell, const std::wstring& verbSubKey) -> bool {
        std::wstring cmdVal;
        if (RegQueryStringValueDynamic(hShell, (verbSubKey + L"\\command").c_str(), nullptr, cmdVal)) {
            return (wcsstr(cmdVal.c_str(), exePath.c_str()) != nullptr);
        }
        return false;
    };

    // Clean any legacy verb names on HKCU image shell keys only if their command data points at this app's own exe path
    auto cleanLegacyImageShellVerbs = [&](const std::wstring& baseKeyPath) {
        std::wstring shellPath = baseKeyPath + L"\\shell";
        ScopedHKey hShell;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, shellPath.c_str(), 0, KEY_READ | KEY_WRITE, hShell.put()) != ERROR_SUCCESS) return;
        std::vector<std::wstring> subKeys;
        RegEnumSubKeyNamesDynamic(hShell, subKeys);
        std::vector<std::wstring> toRemove;
        for (const auto& s : subKeys) {
            bool isCurrentVerb = (_wcsicmp(s.c_str(), L"PepperSnapEdit") == 0 ||
                                  _wcsicmp(s.c_str(), L"PepperSnapImageSearch") == 0 ||
                                  _wcsicmp(s.c_str(), L"PepperSnapPin") == 0);
            if (!isCurrentVerb && commandPointsToThisExe(hShell, s)) {
                toRemove.push_back(s);
            }
        }
        for (const auto& k : toRemove) {
            std::wstring full = shellPath + L"\\" + k;
            SHDeleteKeyW(HKEY_CURRENT_USER, full.c_str());
        }
    };

    auto writeSingleVerb = [&](const std::wstring& verbKeyPath, const std::wstring& lbl, const std::wstring& cmd, const wchar_t* multiSelectModel) {
        ScopedHKey hVerbKey;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, verbKeyPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, hVerbKey.put(), nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hVerbKey, nullptr, 0, REG_SZ, (const BYTE*)lbl.c_str(), (DWORD)((lbl.size() + 1) * sizeof(wchar_t)));
            RegSetValueExW(hVerbKey, L"Icon", 0, REG_SZ, (const BYTE*)iconVal.c_str(), (DWORD)((iconVal.size() + 1) * sizeof(wchar_t)));
            if (multiSelectModel && multiSelectModel[0] != L'\0') {
                RegSetValueExW(hVerbKey, L"MultiSelectModel", 0, REG_SZ, (const BYTE*)multiSelectModel, (DWORD)((wcslen(multiSelectModel) + 1) * sizeof(wchar_t)));
            }
        }
        std::wstring cmdKeyPath = verbKeyPath + L"\\command";
        ScopedHKey hCmdKey;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, cmdKeyPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, hCmdKey.put(), nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hCmdKey, nullptr, 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.size() + 1) * sizeof(wchar_t)));
        }
    };

    auto writeShellVerbs = [&](const std::wstring& baseKeyPath) {
        cleanLegacyImageShellVerbs(baseKeyPath);
        writeSingleVerb(baseKeyPath + L"\\shell\\PepperSnapEdit", editLabel, editCmd, L"Single");
        if (g_Daemon.enableWebImageSearch) {
            writeSingleVerb(baseKeyPath + L"\\shell\\PepperSnapImageSearch", searchLabel, searchCmd, L"Single");
        } else {
            SHDeleteKeyW(HKEY_CURRENT_USER, (baseKeyPath + L"\\shell\\PepperSnapImageSearch").c_str());
        }
        writeSingleVerb(baseKeyPath + L"\\shell\\PepperSnapPin", pinLabel, pinCmd, L"Player");
    };

    // Register ONLY for selected image files via SystemFileAssociations\image and common image extensions
    writeShellVerbs(L"Software\\Classes\\SystemFileAssociations\\image");
    const wchar_t* imgExts[] = {
        L".png", L".jpg", L".jpeg", L".webp", L".bmp", L".gif", L".tif", L".tiff", L".ico"
    };
    const wchar_t* perceivedImg = L"image";
    for (const wchar_t* ext : imgExts) {
        writeShellVerbs(std::wstring(L"Software\\Classes\\SystemFileAssociations\\") + ext);
        ScopedHKey hExtKey;
        std::wstring extClassPath = std::wstring(L"Software\\Classes\\") + ext;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, extClassPath.c_str(), 0, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, hExtKey.put(), nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hExtKey, L"PerceivedType", 0, REG_SZ, (const BYTE*)perceivedImg, (DWORD)((wcslen(perceivedImg) + 1) * sizeof(wchar_t)));
            std::wstring progId;
            if (RegQueryStringValueDynamic(hExtKey, nullptr, nullptr, progId) && !progId.empty()) {
                writeShellVerbs(L"Software\\Classes\\" + progId);
            }
        }
    }
}

void PepperSnapDaemon::UnregisterImageContextMenu() {
    std::wstring exePath = GetModuleFilePathDynamic(nullptr);
    if (exePath.empty()) return;

    auto removeOwnVerbsUnderBase = [&](const std::wstring& baseKeyPath) {
        std::wstring shellPath = baseKeyPath + L"\\shell";
        ScopedHKey hShell;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, shellPath.c_str(), 0, KEY_READ | KEY_WRITE, hShell.put()) != ERROR_SUCCESS) return;
        const wchar_t* ownVerbs[] = { L"PepperSnapEdit", L"PepperSnapImageSearch", L"PepperSnapPin" };
        for (const wchar_t* v : ownVerbs) {
            std::wstring cmdVal;
            if (RegQueryStringValueDynamic(hShell, (std::wstring(v) + L"\\command").c_str(), nullptr, cmdVal)) {
                if (wcsstr(cmdVal.c_str(), exePath.c_str()) != nullptr) {
                    SHDeleteKeyW(hShell, v);
                }
            }
        }
    };

    removeOwnVerbsUnderBase(L"Software\\Classes\\SystemFileAssociations\\image");
    const wchar_t* imgExts[] = {
        L".png", L".jpg", L".jpeg", L".webp", L".bmp", L".gif", L".tif", L".tiff", L".ico"
    };
    for (const wchar_t* ext : imgExts) {
        removeOwnVerbsUnderBase(std::wstring(L"Software\\Classes\\SystemFileAssociations\\") + ext);
        ScopedHKey hExtKey;
        std::wstring extClassPath = std::wstring(L"Software\\Classes\\") + ext;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, extClassPath.c_str(), 0, KEY_READ, hExtKey.put()) == ERROR_SUCCESS) {
            std::wstring progId;
            if (RegQueryStringValueDynamic(hExtKey, nullptr, nullptr, progId) && !progId.empty()) {
                removeOwnVerbsUnderBase(L"Software\\Classes\\" + progId);
            }
        }
    }

    // Deliberately does NOT touch the HKCU\...\Run entry. This runs when the
    // user ticks "Disable right-click integration", and deleting the startup
    // value here silently switched "Launch PepperSnap at Windows startup" off
    // as well - a separate setting the user never asked to change.
}
