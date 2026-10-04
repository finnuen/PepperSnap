# PepperSnap v3.0.0.0 — Pure Standalone Win32 / GDI+ C++17 Application
<img width="708" height="570" alt="Untitled" src="https://github.com/user-attachments/assets/f682dae7-4bd8-4d63-ac4a-b3439f5752e1" />

PepperSnap is a 100% native standalone Windows screenshot, region snip, and vector annotation application written exclusively in **C++17** using the **Win32 API** and **GDI+**. It uses zero web technologies, zero Node.js, zero Electron, and zero external third-party runtime dependencies.

---

## Project Structure

- **`PepperSnap.cpp`** — Complete standalone Win32 API & GDI+ C++17 source code (~5,300 lines).
- **`PepperSnap.h`** — Win32 headers, constants, tool/annotation structs, and MSVC `#pragma comment(lib)` directives.
- **`peppersnap.rc` / `peppersnap.ico` / `peppersnap.res`** — Native Windows application icon and `VERSIONINFO` resource script.
- **`PepperSnap.exe`** — Pre-compiled 64-bit standalone Windows PE32+ GUI executable (`IMAGE_SUBSYSTEM_WINDOWS_GUI`, statically linked CRT).
- **`CMakeLists.txt`** — Standard CMake configuration for MSVC, MinGW-w64, and Clang.
- **`PepperSnap.sln` / `PepperSnap.vcxproj`** — Visual Studio 2022 C++ solution and project files.
- **`Makefile`** — GNU Make / MinGW-w64 build file.
- **`build.bat`** — Native Windows batch build script (auto-detects MSVC `cl.exe`, MinGW `g++.exe`, or `clang++.exe`).
- **`build.sh`** — POSIX cross-compilation script targeting `x86_64-windows-gnu`.

---

## Core Win32 Architecture & Features

1. **System Tray Resident Daemon (`Shell_NotifyIconW`)**
   - Runs silently in the Windows System Tray (`PepperSnapTrayDaemonClass`) with single-instance mutex protection (`Global\PepperSnap_TrayDaemon_Mutex`).
   - Intercepts **`Ctrl + PrintScreen`** (Region Snip & Annotate) and **`Shift + PrintScreen`** (Instant Fullscreen Capture) via both `WH_KEYBOARD_LL` (`SetWindowsHookExW`) and `RegisterHotKey`.
2. **Hardware-Accelerated Fullscreen Overlay (`BitBlt` + Top-Down DIB Section Cache)**
   - Captures the multi-monitor virtual desktop (`SM_XVIRTUALSCREEN`..`SM_CYVIRTUALSCREEN`) using `BitBlt(..., SRCCOPY | CAPTUREBLT)`.
   - Pre-computes bright and 52%-dimmed 32-bpp DIB surfaces once on capture startup so mouse-move and annotation redraws execute with zero per-frame alpha-blend lag.
3. **In-Place Vector & Pixelation Annotation Studio**
   - **11 Native Tools**: Select/Move (`V`), Freehand Pen with Gaussian + Chaikin spline smoothing (`P`), Non-Stacking Stabilo Highlighter (`H`), Line (`L`), Arrow (`A`), Auto-Incrementing Numbering Arrow (`N`) with `↺1` counter reset, Rectangle (`R`), Ellipse (`E`), Multi-line In-Place Text Box (`T`), Mosaic Square (`X`), and Mosaic Circle (`M`).
   - **Typeable Dimension & Stroke Pills**: Click the `W × H px` selection badge or custom stroke size box to type exact pixel dimensions in place with full caret/selection support.
   - **5× Magnifier Loupe & Pixel Color Picker**: Live magnified crosshair with `(X,Y) #RRGGBB` readout; press `Ctrl+C` before selecting a region to copy the exact hex color under the cursor.
   - **Pin to Desktop (`F`)**: Floats any annotated region as a topmost borderless Win32 window (`PepperSnapPinWnd`) with mouse-wheel scaling (`25%–300%`) and drag positioning.
4. **Zero-Metadata Image Encoders (`JPEG`, `PNG`, `WebP`, `BMP`)**
   - Strips 100% of EXIF, XMP, ICC, and ancillary chunks from saved `.jpg` and `.png` streams, includes a dependency-free native VP8L lossless `.webp` encoder, and writes clean 24-bit `.bmp` files.
   - Persists configuration in UTF-16LE `%APPDATA%\PepperSnap\settings.ini`.

---

## Building from Source on Windows

### Option 1: One-Click `build.bat` (MSVC, MinGW-w64, or Clang)
```bat
build.bat
```

### Option 2: Visual Studio 2022
Open `PepperSnap.sln` in Visual Studio 2022, select **Release | x64**, and press **Build Solution (`Ctrl+Shift+B`)**.

### Option 3: CMake
```bat
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Option 4: MinGW-w64 / MSYS2
```bash
make
```

---

## Command-Line Switches

```bat
PepperSnap.exe              :: Start in System Tray (or trigger Region Snip if already running)
PepperSnap.exe --region     :: Trigger interactive Region Snip & Annotate overlay
PepperSnap.exe --fullscreen :: Capture instant fullscreen screenshot
PepperSnap.exe --delay      :: Capture fullscreen screenshot after 3-second delay
PepperSnap.exe --options    :: Open unified Win32 Options dialog
PepperSnap.exe --open       :: Open an image file into the annotation overlay
PepperSnap.exe --help       :: Show keyboard shortcuts reference
PepperSnap.exe --exit       :: Gracefully exit running System Tray instance
```
