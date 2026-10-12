#!/bin/sh
set -e

ZIG_DIR="/tmp/zig"
ZIG_BIN="/tmp/zig/zig-linux-x86_64-0.13.0/zig"

ensure_compiler() {
  if [ ! -x "$ZIG_BIN" ]; then
    echo "[Toolchain] Fetching Clang/Zig C++17 Win32 cross-compiler..."
    mkdir -p "$ZIG_DIR"
    python3 -c '
import os, urllib.request, tarfile
zig_bin = "/tmp/zig/zig-linux-x86_64-0.13.0/zig"
if not os.path.exists(zig_bin):
    url = "https://ziglang.org/download/0.13.0/zig-linux-x86_64-0.13.0.tar.xz"
    tar_path = "/tmp/zig/zig.tar.xz"
    urllib.request.urlretrieve(url, tar_path)
    with tarfile.open(tar_path, "r:xz") as tf:
        tf.extractall("/tmp/zig")
    os.remove(tar_path)
'
  fi
}

compile_win32_exe() {
  ensure_compiler
  echo "[Win32 Resource] Compiling peppersnap.rc -> /tmp/peppersnap.res..."
  "$ZIG_BIN" rc -- peppersnap.rc /tmp/peppersnap.res
  echo "[C++17 Win32 Build] Compiling PepperSnap.cpp -> /tmp/PepperSnap.exe (x86_64-windows-gnu, static CRT)..."
  "$ZIG_BIN" c++ \
    -target x86_64-windows-gnu \
    -std=c++17 \
    -O2 \
    -s \
    -municode \
    -Wl,--subsystem,windows \
    -static \
    PepperSnap.cpp \
    /tmp/peppersnap.res \
    -o /tmp/PepperSnap.exe.tmp \
    -lgdiplus \
    -lgdi32 \
    -luser32 \
    -lshell32 \
    -lole32 \
    -luuid \
    -lcomdlg32 \
    -lcomctl32 \
    -ldwmapi \
    -lshlwapi \
    -ladvapi32 \
    -lwininet \
    -lwindowscodecs
  mv -f /tmp/PepperSnap.exe.tmp /tmp/PepperSnap.exe
  sha256sum PepperSnap.cpp PepperSnap.h peppersnap.rc peppersnap.ico > /tmp/peppersnap_build.sha256
  echo "[OK] Built standalone Windows PE32+ executable: PepperSnap.exe ($(wc -c < /tmp/PepperSnap.exe) bytes)"
}

needs_rebuild() {
  if [ ! -f /tmp/PepperSnap.exe ] || [ ! -f /tmp/peppersnap_build.sha256 ]; then
    return 0
  fi
  CURRENT_HASH="$(sha256sum PepperSnap.cpp PepperSnap.h peppersnap.rc peppersnap.ico 2>/dev/null || true)"
  SAVED_HASH="$(cat /tmp/peppersnap_build.sha256 2>/dev/null || true)"
  if [ "$CURRENT_HASH" != "$SAVED_HASH" ]; then
    return 0
  fi
  return 1
}

check_win32_syntax() {
  ensure_compiler
  echo "[C++17 Win32 Lint] Checking PepperSnap.cpp & PepperSnap.h syntax and types..."
  "$ZIG_BIN" c++ \
    -target x86_64-windows-gnu \
    -std=c++17 \
    -c \
    -municode \
    PepperSnap.cpp \
    -o /tmp/PepperSnap_lint.o
  rm -f /tmp/PepperSnap_lint.o
  echo "[OK] Zero C++17 syntax or Win32 type errors."
}

run_native_cpp_host() {
  if needs_rebuild; then
    (
      if mkdir /tmp/peppersnap_building.lock 2>/dev/null; then
        trap 'rmdir /tmp/peppersnap_building.lock 2>/dev/null || true' EXIT
        compile_win32_exe
      fi
    ) &
  fi

  exec python3 -u -c '
import html
import http.server
import os
import socketserver
import time

def read_text(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            return f.read()
    except Exception:
        return ""

def get_exe_path():
    for p in ("/tmp/PepperSnap.exe", "PepperSnap.exe"):
        if os.path.isfile(p) and os.path.getsize(p) > 0:
            return p
    return None

class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def do_HEAD(self):
        self.handle_request(head_only=True)

    def do_GET(self):
        self.handle_request(head_only=False)

    def handle_request(self, head_only=False):
        path = self.path.split("?", 1)[0]
        if path in ("/PepperSnap.exe", "/api/download/PepperSnap.exe"):
            for _ in range(120):
                exe_path = get_exe_path()
                if exe_path and not os.path.exists("/tmp/peppersnap_building.lock"):
                    break
                time.sleep(0.5)
            exe_path = get_exe_path()
            if not exe_path:
                msg = b"PepperSnap.exe is currently compiling, please refresh in a few seconds."
                self.send_response(503)
                self.send_header("Content-Type", "text/plain; charset=utf-8")
                self.send_header("Retry-After", "3")
                self.send_header("Content-Length", str(len(msg)))
                self.end_headers()
                if not head_only:
                    self.wfile.write(msg)
                return

            with open(exe_path, "rb") as f:
                data = f.read()
            self.send_response(200)
            self.send_header("Content-Type", "application/vnd.microsoft.portable-executable")
            self.send_header("Content-Disposition", "attachment; filename=\"PepperSnap.exe\"")
            self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
            self.send_header("Pragma", "no-cache")
            self.send_header("Expires", "0")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            if not head_only:
                self.wfile.write(data)
            return

        cpp_src = read_text("PepperSnap.cpp")
        hdr_src = read_text("PepperSnap.h")
        cmake_src = read_text("CMakeLists.txt")
        exe_path = get_exe_path()
        exe_bytes = os.path.getsize(exe_path) if exe_path else 0
        cpp_lines = cpp_src.count("\n") + 1 if cpp_src else 0
        status_label = f"{exe_bytes // 1024} KB" if exe_bytes > 0 else "Building..."

        body = (
            "<!doctype html><html><head><meta charset=\"utf-8\">"
            "<title>PepperSnap v3.11.2 — Pure Win32 C++17 Application</title>"
            "<style>"
            "body{margin:0;background:#090D16;color:#F8FAFC;font-family:\x27Segoe UI\x27,system-ui,sans-serif;line-height:1.5}"
            ".wrap{max-width:1080px;margin:0 auto;padding:32px 24px}"
            ".card{background:#0F172A;border:1px solid #1E293B;padding:24px;margin-bottom:20px}"
            ".badge{display:inline-block;background:#064E3B;color:#34D399;border:1px solid #059669;padding:3px 10px;font-size:12px;font-family:Consolas,monospace;margin-bottom:12px}"
            "h1{margin:0 0 8px 0;font-size:26px;color:#FFF}"
            "h2{margin:0 0 12px 0;font-size:16px;color:#38BDF8;font-family:Consolas,monospace}"
            "p{margin:0 0 14px 0;color:#CBD5E1;font-size:14px}"
            ".btn{display:inline-block;background:#DC2626;color:#FFF;text-decoration:none;font-weight:600;padding:12px 22px;font-size:14px;border:1px solid #EF4444}"
            ".btn:hover{background:#EF4444}"
            ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:12px;margin-top:16px}"
            ".kv{background:#090D16;border:1px solid #1E293B;padding:12px;font-family:Consolas,monospace;font-size:12px}"
            ".kv span{display:block;color:#64748B;font-size:11px;margin-bottom:4px}"
            "pre{background:#090D16;border:1px solid #1E293B;padding:16px;overflow:auto;max-height:520px;font-family:Consolas,monospace;font-size:12px;color:#E2E8F0;margin:0}"
            "</style></head><body><div class=\"wrap\">"
            "<div class=\"card\">"
            "<div class=\"badge\">100% PURE WIN32 API + GDI+ C++17 · ZERO WEB / ZERO NODE.JS / ZERO ELECTRON / ZERO TYPESCRIPT</div>"
            "<h1>PepperSnap.exe v3.11.2 — Standalone Native Windows Executable</h1>"
            "<p>This repository is a pure standalone C++17 Win32 API application (<code>PepperSnap.cpp</code>, <code>PepperSnap.h</code>, <code>peppersnap.rc</code>, <code>CMakeLists.txt</code>, <code>PepperSnap.sln</code>, <code>PepperSnap.vcxproj</code>, <code>Makefile</code>, <code>build.bat</code>).</p>"
            f"<a class=\"btn\" href=\"/PepperSnap.exe?v=3.11.2\" download=\"PepperSnap.exe\">Download Standalone PepperSnap.exe v3.11.2 ({status_label})</a>"
            "<div class=\"grid\">"
            f"<div class=\"kv\"><span>BINARY TARGET</span>PE32+ GUI (x86_64-windows-gnu) · {exe_bytes} bytes</div>"
            f"<div class=\"kv\"><span>WIN32 SOURCE SIZE</span>{cpp_lines} lines C++17 (PepperSnap.cpp + PepperSnap.h)</div>"
            "<div class=\"kv\"><span>SYSTEM TRAY &amp; HOOKS</span>Shell_NotifyIconW · WH_KEYBOARD_LL · RegisterHotKey</div>"
            "<div class=\"kv\"><span>RENDERING ENGINE</span>GDI BitBlt Surface Cache + GDI+ Vector HUD</div>"
            "</div></div>"
            "<div class=\"card\"><h2>CMakeLists.txt &amp; Windows Build Commands</h2>"
            f"<pre>{html.escape(cmake_src)}\n\n# Or build directly on Windows via build.bat / Visual Studio 2022:\n#   build.bat\n#   msbuild PepperSnap.sln /p:Configuration=Release /p:Platform=x64</pre></div>"
            "<div class=\"card\"><h2>PepperSnap.h (Win32 API Header)</h2>"
            f"<pre>{html.escape(hdr_src)}</pre></div>"
            f"<div class=\"card\"><h2>PepperSnap.cpp (Complete Native Win32 / GDI+ Source — {cpp_lines} lines)</h2>"
            f"<pre>{html.escape(cpp_src)}</pre></div>"
            "</div></body></html>"
        ).encode("utf-8")

        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if not head_only:
            self.wfile.write(body)

class ReusableTCPServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    allow_reuse_address = True
    daemon_threads = True

srv = ReusableTCPServer(("0.0.0.0", 3000), Handler)
print("[PepperSnap Host] Serving PepperSnap.exe v3.11.2 on port 3000", flush=True)
srv.serve_forever()
'
}

case "$1" in
  --check)
    check_win32_syntax
    ;;
  --serve)
    run_native_cpp_host
    ;;
  *)
    compile_win32_exe
    ;;
esac
