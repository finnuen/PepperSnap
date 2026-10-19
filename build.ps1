<#
.SYNOPSIS
    PepperSnap build script - the only one in this repository.

.DESCRIPTION
    Builds build\PepperSnap_v<version>.exe as a single self-contained Win32
    executable, using Zig 0.13.0 as the C++17 cross-compiler for
    x86_64-windows-gnu.

    WHY ZIG 0.13.0 SPECIFICALLY
        The flags below are 0.13-era. Newer Zig releases (winget currently
        installs 0.17.0) changed the target/flags surface and may reject them.
        The script hard-fails on a version mismatch rather than building
        something subtly wrong.

    WHY -static IS NOT OPTIONAL
        -static against x86_64-windows-gnu is the entire reason the exe is
        ~1.2 MB and dependency-free. Remove it (or build with a plain MinGW
        g++ without -static-libgcc -static-libstdc++) and the binary starts
        requiring libstdc++-6.dll and libgcc_s_seh-1.dll, which silently
        destroys the project's headline feature. The script verifies the
        import table afterwards to catch exactly that regression.

    VERSIONING
        One visible version, MAJOR.MINOR.PATCH, kept in sync in three places:
        FILEVERSION / PRODUCTVERSION and their strings in peppersnap.rc, and
        APP_VERSION in PepperSnapInternal.h. The exe is named after it, so the
        release asset, the git tag and the version the app reports all agree.

        PATCH is the build counter and advances by one on every build, which
        makes each build its own release. The fourth field is always 0 - it
        used to carry the git commit count.

.PARAMETER Zig
    Directory containing zig.exe, or the path to zig.exe itself.
    Defaults to $env:ZIG_HOME, then to C:\Users\kuuha\zig-0.13.0.

.PARAMETER Version
    Build this version instead of incrementing, e.g. 4.1.1. Use it for the
    first build of a new minor or major series.

.PARAMETER NoVersionBump
    Build the version already recorded in peppersnap.rc, unchanged. This is
    what a repeat build of the same source uses, so the number cannot creep.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\build.ps1
#>
[CmdletBinding()]
param(
    [string] $Zig,
    [string] $Version,
    [switch] $NoVersionBump
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root

$RequiredZig   = '0.13.0'
$RcFile        = Join-Path $root 'peppersnap.rc'
$HeaderFile    = Join-Path $root 'PepperSnapInternal.h'
$OutDir        = Join-Path $root 'build'
$OutRes        = Join-Path $OutDir 'peppersnap.res'

$Sources = @(
    'PepperSnap.cpp'
    'Settings.cpp'
    'Registry.cpp'
    'InputHook.cpp'
    'Capture.cpp'
    'WebPEncode.cpp'
    'Overlay.cpp'
    'Pinned.cpp'
    'Ocr.cpp'
    'Tray.cpp'
)

$Libs = @(
    '-lgdiplus', '-lgdi32', '-luser32', '-lshell32', '-lole32', '-loleaut32',
    '-luuid', '-lcomdlg32', '-lcomctl32', '-ldwmapi', '-lshlwapi',
    '-ladvapi32', '-lwininet', '-lwindowscodecs'
)

function Write-Step($msg) { Write-Host "[build] $msg" -ForegroundColor Cyan }
function Write-Ok($msg)   { Write-Host "[  ok  ] $msg" -ForegroundColor Green }
function Write-Warn2($msg){ Write-Host "[ warn ] $msg" -ForegroundColor Yellow }

# ---------------------------------------------------------------- toolchain
function Find-Zig {
    $candidates = New-Object System.Collections.Generic.List[string]
    if ($Zig) {
        if ((Get-Item -LiteralPath $Zig -ErrorAction SilentlyContinue).PSIsContainer) {
            $candidates.Add((Join-Path $Zig 'zig.exe'))
        } else {
            $candidates.Add($Zig)
        }
    }
    if ($env:ZIG_HOME) {
        $candidates.Add((Join-Path $env:ZIG_HOME 'zig.exe'))
        $candidates.Add($env:ZIG_HOME)
    }
    $candidates.Add('C:\Users\kuuha\zig-0.13.0\zig.exe')

    foreach ($c in $candidates) {
        if ($c -and (Test-Path -LiteralPath $c -PathType Leaf)) { return (Resolve-Path -LiteralPath $c).Path }
    }
    return $null
}

$zigExe = Find-Zig
if (-not $zigExe) {
    throw "Zig not found. Looked at -Zig, ZIG_HOME and C:\Users\kuuha\zig-0.13.0. " +
          "Download zig-windows-x86_64-$RequiredZig.zip from https://ziglang.org/download/ and extract it."
}

$zigVersion = (& $zigExe version).Trim()
if ($zigVersion -ne $RequiredZig) {
    throw "Zig $RequiredZig required but found $zigVersion ($zigExe). " +
          "Do not use the winget 'zig.zig' package - it is 0.17.0 and the flags here are 0.13-era."
}
Write-Ok "zig $zigVersion  ($zigExe)"

# ------------------------------------------------------------- version stamp
function Set-Version {
    $rc = [System.IO.File]::ReadAllText($RcFile)
    $m = [regex]::Match($rc, 'FILEVERSION\s+(\d+),(\d+),(\d+),(\d+)')
    if (-not $m.Success) { throw "Could not find FILEVERSION in $RcFile" }

    $major = [int]$m.Groups[1].Value
    $minor = [int]$m.Groups[2].Value
    $patch = [int]$m.Groups[3].Value

    if ($Version) {
        if ($Version -notmatch '^(\d+)\.(\d+)\.(\d+)$') {
            throw "-Version must look like 4.1.1 (got '$Version')"
        }
        $major = [int]$Matches[1]; $minor = [int]$Matches[2]; $patch = [int]$Matches[3]
        Write-Step "version set explicitly to $major.$minor.$patch"
    } elseif ($NoVersionBump) {
        Write-Warn2 "version left at $major.$minor.$patch (-NoVersionBump)"
    } else {
        $patch++
        Write-Step "version bumped to $major.$minor.$patch"
    }

    $dotted  = "$major.$minor.$patch"
    $full    = "$dotted.0"
    $exeName = "PepperSnap_v$dotted.exe"

    $rc = $rc -replace 'FILEVERSION\s+\d+,\d+,\d+,\d+',       "FILEVERSION $major,$minor,$patch,0"
    $rc = $rc -replace 'PRODUCTVERSION\s+\d+,\d+,\d+,\d+',    "PRODUCTVERSION $major,$minor,$patch,0"
    # The resource script writes these with a trailing escape ("4.1.1.0\0"), so the
    # match stops at the dotted version rather than requiring a closing quote after it.
    $rc = $rc -replace '("FileVersion",\s*")\d+(\.\d+){3}',    "`${1}$full"
    $rc = $rc -replace '("ProductVersion",\s*")\d+(\.\d+){3}', "`${1}$full"
    # OriginalFilename is what installers and scanners read back, so it has to be
    # the name the file actually ships under, not a stale constant.
    $rc = $rc -replace '("OriginalFilename",\s*")[^"\\]+',    "`${1}$exeName"

    # Never let a half-applied rewrite through again: every field is checked by name.
    $checks = @(
        @{ Needle = "FILEVERSION $major,$minor,$patch,0";    What = 'FILEVERSION' },
        @{ Needle = "PRODUCTVERSION $major,$minor,$patch,0"; What = 'PRODUCTVERSION' },
        @{ Needle = '"FileVersion", "'    + $full;           What = 'FileVersion string' },
        @{ Needle = '"ProductVersion", "' + $full;           What = 'ProductVersion string' },
        @{ Needle = '"OriginalFilename", "' + $exeName;      What = 'OriginalFilename' }
    )
    foreach ($c in $checks) {
        if ($rc -notmatch [regex]::Escape($c.Needle)) {
            throw "version rewrite failed: $($c.What) does not read '$($c.Needle)' in peppersnap.rc"
        }
    }
    [System.IO.File]::WriteAllText($RcFile, $rc, (New-Object System.Text.UTF8Encoding($false)))

    # APP_VERSION is what the tray, the About box and the update check report.
    $hdr = [System.IO.File]::ReadAllText($HeaderFile)
    $hdr = $hdr -replace '(APP_VERSION\s*=\s*L")\d+(\.\d+){2}"', "`${1}$dotted`""
    if ($hdr -notmatch ('APP_VERSION\s*=\s*L"' + [regex]::Escape($dotted) + '"')) {
        throw "version rewrite failed: APP_VERSION does not read $dotted in PepperSnapInternal.h"
    }
    [System.IO.File]::WriteAllText($HeaderFile, $hdr, (New-Object System.Text.UTF8Encoding($false)))

    Write-Ok "version $dotted  ->  FILEVERSION $major,$minor,$patch,0, APP_VERSION, $exeName"
    return $dotted
}

# ---------------------------------------------------------------- compile
if (-not (Test-Path -LiteralPath $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

# Stamp the version before the resources are compiled, so the icon-less .res
# carries it and the output file is named after it.
$version = Set-Version
$OutExe  = Join-Path $OutDir "PepperSnap_v$version.exe"

Write-Step 'compiling resources'
& $zigExe rc -- peppersnap.rc $OutRes
if ($LASTEXITCODE -ne 0) { throw "resource compilation failed (exit $LASTEXITCODE)" }
Write-Ok "peppersnap.res  ($((Get-Item $OutRes).Length) bytes)"

Write-Step "compiling $($Sources.Count) translation units -> $OutExe"
$cppArgs = @(
    'c++'
    '-target', 'x86_64-windows-gnu'
    '-std=c++17'
    '-O2'
    '-s'
    '-municode'
    '-Wl,--subsystem,windows'
    '-static'
) + $Sources + @($OutRes, '-o', $OutExe) + $Libs

& $zigExe @cppArgs
if ($LASTEXITCODE -ne 0) { throw "C++ compilation failed (exit $LASTEXITCODE)" }
if (-not (Test-Path -LiteralPath $OutExe)) { throw "compiler reported success but $OutExe is missing" }

# ---------------------------------------------------------------- verify
$sizeBytes = (Get-Item $OutExe).Length
Write-Ok ("{0}  {1:N0} bytes ({2:N2} MiB)" -f (Split-Path -Leaf $OutExe), $sizeBytes, ($sizeBytes / 1MB))

Write-Step 'checking import table for runtime dependencies'
$bytes = [System.IO.File]::ReadAllBytes($OutExe)
$forbidden = @('libstdc++-6.dll', 'libgcc_s_seh-1.dll', 'libwinpthread-1.dll')
$found = @()
foreach ($name in $forbidden) {
    $needle = [System.Text.Encoding]::ASCII.GetBytes($name)
    $hit = $false
    for ($i = 0; $i -le ($bytes.Length - $needle.Length); $i++) {
        if ($bytes[$i] -eq $needle[0]) {
            $match = $true
            for ($j = 1; $j -lt $needle.Length; $j++) {
                if ($bytes[$i + $j] -ne $needle[$j]) { $match = $false; break }
            }
            if ($match) { $hit = $true; break }
        }
    }
    if ($hit) { $found += $name }
}

if ($found.Count -gt 0) {
    Write-Host ''
    Write-Host "[FAIL] the exe references: $($found -join ', ')" -ForegroundColor Red
    Write-Host "       -static did not take effect. The 'no dependencies' claim is broken." -ForegroundColor Red
    exit 1
}
Write-Ok 'no libstdc++-6.dll / libgcc_s_seh-1.dll / libwinpthread-1.dll references'

$vi = (Get-Item $OutExe).VersionInfo
Write-Host ''
Write-Host "  FileVersion    $($vi.FileVersion)"
Write-Host "  ProductVersion $($vi.ProductVersion)"
Write-Host "  Output         $OutExe"
Write-Host ''
Write-Ok 'build complete'

exit 0
