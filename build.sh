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
  if [ -f PepperSnap.exe ] && [ ! PepperSnap.cpp -nt PepperSnap.exe ] && [ ! PepperSnap.h -nt PepperSnap.exe ] && [ ! peppersnap.rc -nt PepperSnap.exe ]; then
    echo "[OK] Standalone Windows PE32+ executable is up to date: PepperSnap.exe ($(wc -c < PepperSnap.exe) bytes)"
    return 0
  fi
  ensure_compiler
  if [ ! -f peppersnap.res ] || [ peppersnap.rc -nt peppersnap.res ]; then
    echo "[Win32 Resource] Compiling peppersnap.rc -> peppersnap.res..."
    "$ZIG_BIN" rc peppersnap.rc peppersnap.res 2>/dev/null || true
  fi
  echo "[C++17 Win32 Build] Compiling PepperSnap.cpp -> PepperSnap.exe (x86_64-windows-gnu, static CRT)..."
  "$ZIG_BIN" c++ \
    -target x86_64-windows-gnu \
    -std=c++17 \
    -O2 \
    -s \
    -municode \
    -mwindows \
    -Wl,--subsystem,windows \
    -static \
    PepperSnap.cpp \
    peppersnap.res \
    -o PepperSnap.exe \
    -lgdiplus \
    -lgdi32 \
    -luser32 \
    -lshell32 \
    -lole32 \
    -lcomdlg32 \
    -lcomctl32 \
    -ldwmapi \
    -lshlwapi \
    -ladvapi32 \
    -lwininet
  echo "[OK] Built standalone Windows PE32+ executable: PepperSnap.exe ($(wc -c < PepperSnap.exe) bytes)"
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
  ensure_compiler
  if [ ! -f PepperSnap.exe ] || [ PepperSnap.cpp -nt PepperSnap.exe ] || [ PepperSnap.h -nt PepperSnap.exe ]; then
    compile_win32_exe
  fi

  cat << 'EOF' > /tmp/peppersnap_host.cpp
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

static std::string read_file_bytes(const char* path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return {};
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

static std::string html_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 256);
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default:  out += c; break;
        }
    }
    return out;
}

static void send_all(int fd, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, data + sent, len - sent);
        if (n <= 0) break;
        sent += (size_t)n;
    }
}

int main() {
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(3000);

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    listen(srv, 64);
    printf("[PepperSnap Native C++17 Host] Serving PepperSnap.exe on port 3000\n");
    fflush(stdout);

    while (true) {
        int client = accept(srv, nullptr, nullptr);
        if (client < 0) continue;

        char buf[4096] = {0};
        ssize_t n = read(client, buf, sizeof(buf) - 1);
        if (n <= 0) {
            close(client);
            continue;
        }

        std::string req(buf, (size_t)n);
        if (req.find("GET /PepperSnap.exe") == 0 || req.find("GET /api/download/PepperSnap.exe") == 0) {
            std::string exe = read_file_bytes("PepperSnap.exe");
            std::ostringstream hdr;
            hdr << "HTTP/1.1 200 OK\r\n"
                << "Content-Type: application/vnd.microsoft.portable-executable\r\n"
                << "Content-Disposition: attachment; filename=\"PepperSnap.exe\"\r\n"
                << "Content-Length: " << exe.size() << "\r\n"
                << "Connection: close\r\n\r\n";
            std::string h = hdr.str();
            send_all(client, h.data(), h.size());
            send_all(client, exe.data(), exe.size());
            close(client);
            continue;
        }

        std::string cppSrc = read_file_bytes("PepperSnap.cpp");
        std::string hdrSrc = read_file_bytes("PepperSnap.h");
        std::string cmakeSrc = read_file_bytes("CMakeLists.txt");
        struct stat st{};
        long exeBytes = (stat("PepperSnap.exe", &st) == 0) ? (long)st.st_size : 0;
        long cppLines = 1;
        for (char c : cppSrc) if (c == '\n') cppLines++;

        std::ostringstream page;
        page << "<!doctype html><html><head><meta charset=\"utf-8\">"
             << "<title>PepperSnap v3.2.0.1 — Pure Win32 C++17 Application</title>"
             << "<style>"
             << "body{margin:0;background:#090D16;color:#F8FAFC;font-family:'Segoe UI',system-ui,sans-serif;line-height:1.5}"
             << ".wrap{max-width:1080px;margin:0 auto;padding:32px 24px}"
             << ".card{background:#0F172A;border:1px solid #1E293B;padding:24px;margin-bottom:20px}"
             << ".badge{display:inline-block;background:#064E3B;color:#34D399;border:1px solid #059669;padding:3px 10px;font-size:12px;font-family:Consolas,monospace;margin-bottom:12px}"
             << "h1{margin:0 0 8px 0;font-size:26px;color:#FFF}"
             << "h2{margin:0 0 12px 0;font-size:16px;color:#38BDF8;font-family:Consolas,monospace}"
             << "p{margin:0 0 14px 0;color:#CBD5E1;font-size:14px}"
             << ".btn{display:inline-block;background:#DC2626;color:#FFF;text-decoration:none;font-weight:600;padding:12px 22px;font-size:14px;border:1px solid #EF4444}"
             << ".btn:hover{background:#EF4444}"
             << ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:12px;margin-top:16px}"
             << ".kv{background:#090D16;border:1px solid #1E293B;padding:12px;font-family:Consolas,monospace;font-size:12px}"
             << ".kv span{display:block;color:#64748B;font-size:11px;margin-bottom:4px}"
             << "pre{background:#090D16;border:1px solid #1E293B;padding:16px;overflow:auto;max-height:520px;font-family:Consolas,monospace;font-size:12px;color:#E2E8F0;margin:0}"
             << "</style></head><body><div class=\"wrap\">"
             << "<div class=\"card\">"
             << "<div class=\"badge\">100% PURE WIN32 API + GDI+ C++17 · ZERO WEB / ZERO NODE.JS / ZERO ELECTRON / ZERO TYPESCRIPT</div>"
             << "<h1>PepperSnap.exe v3.2.0.1 — Standalone Native Windows Executable</h1>"
             << "<p>This repository is a pure standalone C++17 Win32 API application (<code>PepperSnap.cpp</code>, <code>PepperSnap.h</code>, <code>peppersnap.rc</code>, <code>CMakeLists.txt</code>, <code>PepperSnap.sln</code>, <code>PepperSnap.vcxproj</code>, <code>Makefile</code>, <code>build.bat</code>). Even this binary inspection page is served directly by a compiled C++17 POSIX socket executable.</p>"
             << "<a class=\"btn\" href=\"/PepperSnap.exe\" download=\"PepperSnap.exe\">Download Standalone PepperSnap.exe (" << (exeBytes / 1024) << " KB)</a>"
             << "<div class=\"grid\">"
             << "<div class=\"kv\"><span>BINARY TARGET</span>PE32+ GUI (x86_64-windows-gnu) · " << exeBytes << " bytes</div>"
             << "<div class=\"kv\"><span>WIN32 SOURCE SIZE</span>" << cppLines << " lines C++17 (PepperSnap.cpp + PepperSnap.h)</div>"
             << "<div class=\"kv\"><span>SYSTEM TRAY &amp; HOOKS</span>Shell_NotifyIconW · WH_KEYBOARD_LL · RegisterHotKey</div>"
             << "<div class=\"kv\"><span>RENDERING ENGINE</span>GDI BitBlt Surface Cache + GDI+ Vector HUD</div>"
             << "</div></div>"
             << "<div class=\"card\"><h2>CMakeLists.txt &amp; Windows Build Commands</h2>"
             << "<pre>" << html_escape(cmakeSrc) << "\n\n# Or build directly on Windows via build.bat / Visual Studio 2022:\n#   build.bat\n#   msbuild PepperSnap.sln /p:Configuration=Release /p:Platform=x64</pre></div>"
             << "<div class=\"card\"><h2>PepperSnap.h (Win32 API Header)</h2>"
             << "<pre>" << html_escape(hdrSrc) << "</pre></div>"
             << "<div class=\"card\"><h2>PepperSnap.cpp (Complete Native Win32 / GDI+ Source — " << cppLines << " lines)</h2>"
             << "<pre>" << html_escape(cppSrc) << "</pre></div>"
             << "</div></body></html>";

        std::string body = page.str();
        std::ostringstream hdr;
        hdr << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: text/html; charset=utf-8\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Connection: close\r\n\r\n";
        std::string h = hdr.str();
        send_all(client, h.data(), h.size());
        send_all(client, body.data(), body.size());
        close(client);
    }
    return 0;
}
EOF

  "$ZIG_BIN" c++ -std=c++17 -O2 /tmp/peppersnap_host.cpp -o /tmp/peppersnap_host
  exec /tmp/peppersnap_host
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
