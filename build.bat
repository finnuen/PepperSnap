@echo off
setlocal enabledelayedexpansion

echo ============================================================================
echo  PepperSnap v3.2.0.0 — Pure Standalone Win32 / GDI+ C++17 Build Script
echo ============================================================================

where cl.exe >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo [MSVC] Found cl.exe — compiling PepperSnap.exe with Microsoft Visual C++...
    where rc.exe >nul 2>nul
    if %ERRORLEVEL% EQU 0 (
        rc.exe /nologo /fo peppersnap.res peppersnap.rc
    )
    cl.exe /nologo /std:c++17 /O2 /MT /EHsc /utf-8 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX ^
        PepperSnap.cpp peppersnap.res ^
        /Fe:PepperSnap.exe ^
        /link /SUBSYSTEM:WINDOWS ^
        gdiplus.lib gdi32.lib user32.lib shell32.lib ole32.lib comdlg32.lib comctl32.lib dwmapi.lib shlwapi.lib advapi32.lib wininet.lib
    if %ERRORLEVEL% EQU 0 (
        echo [OK] Successfully built standalone PepperSnap.exe ^(MSVC Static CRT^)
        exit /b 0
    )
)

where g++.exe >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo [MinGW-w64] Found g++.exe — compiling PepperSnap.exe...
    where windres.exe >nul 2>nul
    if %ERRORLEVEL% EQU 0 (
        windres.exe peppersnap.rc -O coff -o peppersnap.res
    )
    g++.exe -std=c++17 -O2 -s -municode -mwindows -Wl,--subsystem,windows -static ^
        PepperSnap.cpp peppersnap.res -o PepperSnap.exe ^
        -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -lcomdlg32 -lcomctl32 -ldwmapi -lshlwapi -ladvapi32 -lwininet
    if %ERRORLEVEL% EQU 0 (
        echo [OK] Successfully built standalone PepperSnap.exe ^(MinGW-w64 Static^)
        exit /b 0
    )
)

where clang++.exe >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo [Clang++] Found clang++.exe — compiling PepperSnap.exe...
    clang++.exe -std=c++17 -O2 -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX ^
        PepperSnap.cpp peppersnap.res -o PepperSnap.exe ^
        -Wl,/SUBSYSTEM:WINDOWS ^
        -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -lcomdlg32 -lcomctl32 -ldwmapi -lshlwapi -ladvapi32 -lwininet
    if %ERRORLEVEL% EQU 0 (
        echo [OK] Successfully built standalone PepperSnap.exe ^(Clang++^)
        exit /b 0
    )
)

echo [ERROR] No C++17 compiler ^(cl.exe, g++.exe, or clang++.exe^) found in PATH.
echo Please run from a Visual Studio x64 Native Tools Command Prompt or install MinGW-w64.
exit /b 1
