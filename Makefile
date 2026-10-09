# =============================================================================
# PepperSnap v3.8.3 — Standalone Native Win32 / GDI+ C++17 Makefile
# Supports MinGW-w64 (x86_64-w64-mingw32-g++), Clang++, and Zig C++
# =============================================================================

CXX      ?= x86_64-w64-mingw32-g++
WINDRES  ?= x86_64-w64-mingw32-windres
CXXFLAGS := -std=c++17 -O2 -s -Wall -Wextra -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -municode
LDFLAGS  := -mwindows -Wl,--subsystem,windows -static
LDLIBS   := -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -luuid -lcomdlg32 -lcomctl32 -ldwmapi -lshlwapi -ladvapi32 -lwininet -lwindowscodecs

TARGET   := PepperSnap.exe
SRCS     := PepperSnap.cpp
HDRS     := PepperSnap.h
RES      := peppersnap.res

.PHONY: all clean

all: $(TARGET)

$(RES): peppersnap.rc peppersnap.ico
	$(WINDRES) peppersnap.rc -O coff -o $(RES)

$(TARGET): $(SRCS) $(HDRS) $(RES)
	$(CXX) $(CXXFLAGS) $(SRCS) $(RES) -o $(TARGET) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TARGET) *.o
