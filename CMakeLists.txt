cmake_minimum_required(VERSION 3.16)
project(PepperSnap VERSION 3.8.2 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(PepperSnap WIN32
    PepperSnap.cpp
    PepperSnap.h
    peppersnap.rc
)

target_compile_definitions(PepperSnap PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(PepperSnap PRIVATE /W4 /O2 /EHsc /utf-8)
    set_property(TARGET PepperSnap PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_compile_options(PepperSnap PRIVATE -O2 -Wall -Wextra -municode)
    target_link_options(PepperSnap PRIVATE -municode -mwindows -static -Wl,--subsystem,windows)
endif()

target_link_libraries(PepperSnap PRIVATE
    gdiplus
    gdi32
    user32
    shell32
    ole32
    uuid
    comdlg32
    comctl32
    dwmapi
    shlwapi
    advapi32
    wininet
    windowscodecs
)
