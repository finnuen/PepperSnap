#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <objidl.h>
#include <uiautomation.h>
#include <wininet.h>
#include <gdiplus.h>

#if defined(_MSC_VER)
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "wininet.lib")
#endif

#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <memory>

using namespace Gdiplus;

#define IDI_APPICON            101

#define WM_TRAYICON            (WM_USER + 101)
#define WM_TRIGGER_REGION_SNIP (WM_USER + 102)
#define WM_TRIGGER_FULL_SNAP   (WM_USER + 103)
#define WM_TRIGGER_PREV_REGION (WM_USER + 104)
#define WM_UPDATE_CHECK_RESULT (WM_USER + 105)
#define WM_SHORTCUT_RECORDED   (WM_USER + 106)

#define COPYDATA_OPEN_IMAGE        0x5053494DUL
#define COPYDATA_PIN_IMAGE         0x5053504EUL
#define TIMER_AUTO_UPDATE_CHECK    77
#define TIMER_TRAY_DELAY_REGION    78
#define TIMER_TRAY_DELAY_FULL      79
#define TIMER_TRAY_DELAY_PREV      80

#define HK_REGION_SNIP         9101
#define HK_FULL_SNAP           9102
#define HK_PREV_REGION         9103

struct HotkeyBinding {
    UINT modifiers = MOD_CONTROL;
    UINT vk = VK_SNAPSHOT;
};

enum class ImageFormat {
    JPEG = 0,
    PNG  = 1,
    WEBP = 2,
    BMP  = 3
};

enum class UpdateCheckInterval {
    EveryDay    = 0,
    Every3Days  = 1,
    EveryWeek   = 2,
    Every2Weeks = 3,
    EveryMonth  = 4
};

enum TrayMenuID {
    IDM_TRAY_REGION = 2001,
    IDM_TRAY_FULL,
    IDM_TRAY_PREV_REGION,
    IDM_TRAY_OPEN_IMAGE,
    IDM_TRAY_OPEN_PIN_IMAGE,
    IDM_TRAY_OPEN_FOLDER,
    IDM_TRAY_OPTIONS,
    IDM_TRAY_STARTUP_RUN,
    IDM_TRAY_CLOSE_PINS,
    IDM_TRAY_CHECK_UPDATE,
    IDM_TRAY_SHORTCUTS,
    IDM_TRAY_EXIT
};

enum class OverlayTool {
    SelectMove = 0,
    Pen,
    Highlighter,
    Line,
    Arrow,
    NumberArrow,
    Rectangle,
    Ellipse,
    TextBox,
    MosaicSquare,
    MosaicCircle
};

enum class DragMode {
    None = 0,
    PendingOutsideSelection,
    CreatingSelection,
    MovingSelection,
    ResizeTL, ResizeT, ResizeTR,
    ResizeR,  ResizeBR, ResizeB,
    ResizeBL, ResizeL,
    DrawingAnnotation,
    MovingAnnotation,
    DraggingAnnotationStart,
    DraggingAnnotationEnd,
    DraggingHUD
};

struct PointF2D {
    float x = 0.0f;
    float y = 0.0f;
};

struct Annotation {
    int id = 0;
    OverlayTool tool = OverlayTool::Arrow;
    Color color = Color(255, 239, 68, 68);
    float strokeWidth = 4.0f;
    std::vector<PointF2D> points;
    PointF2D startPt{0.0f, 0.0f};
    PointF2D endPt{0.0f, 0.0f};
    std::wstring text;
    int stepNumber = 1;
};

struct DockButton {
    int id = 0;
    RECT rect{0, 0, 0, 0};
    std::wstring label;
    std::wstring tooltip;
    bool isTool = false;
    OverlayTool tool = OverlayTool::Arrow;
    bool isColor = false;
    Color swatchColor = Color(255, 239, 68, 68);
    bool isStroke = false;
    float strokeVal = 4.0f;
    bool isPrimaryAction = false;
};

enum DockBtnID {
    DBTN_TOOL_BASE = 300,
    DBTN_COLOR_BASE = 350,
    DBTN_STROKE_BASE = 380,
    DBTN_ACT_COPY = 400,
    DBTN_ACT_SAVE,
    DBTN_ACT_SAVE_AS,
    DBTN_ACT_PIN,
    DBTN_ACT_OCR,
    DBTN_ACT_ZOOM_OUT,
    DBTN_ACT_ZOOM_IN,
    DBTN_ACT_ZOOM_RESET,
    DBTN_ACT_PIN_OUTLINE,
    DBTN_ACT_PIN_UNFILTER,
    DBTN_ACT_UNDO,
    DBTN_ACT_REDO,
    DBTN_ACT_CLEAR,
    DBTN_ACT_RESET_NUM,
    DBTN_ACT_OPTIONS,
    DBTN_ACT_HELP,
    DBTN_ACT_CLOSE,
    DBTN_ACT_DRAG_HUD
};

struct PinnedWindowData {
    Bitmap* bmp = nullptr;
    int origW = 0;
    int origH = 0;
    float scale = 1.0f;
    int screenImgX = 0;
    int screenImgY = 0;
    RECT imgRect{0, 0, 0, 0};
    RECT lastRgnImgRect{0, 0, 0, 0};
    RECT lastRgnStripRect{0, 0, 0, 0};
    bool lastRgnHideToolbar = false;
    bool hasAppliedRgn = false;
    bool hideToolbar = false;
    bool hideOutline = false;
    bool smoothImage = false;
    bool dragging = false;
    POINT dragStartMouse{0, 0};
    POINT dragStartWnd{0, 0};
    int hoveredBtnId = -1;
    int pressedBtnId = -1;
    bool pendingRightDblClickClose = false;
    bool trackingMouseLeave = false;
    std::vector<DockButton> buttons;
    HDC hCacheDC = nullptr;
    HBITMAP hCacheBmp = nullptr;
    HGDIOBJ hCacheOldBmp = nullptr;
    int cachedImgW = 0;
    int cachedImgH = 0;
    bool cachedHideOutline = false;
    bool cachedSmooth = false;
    bool cachedHq = false;
};
