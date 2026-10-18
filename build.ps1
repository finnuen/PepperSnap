<#
.SYNOPSIS
    PepperSnap build script - the only one in this repository.

.DESCRIPTION
    Builds build\PepperSnap.exe as a single self-contained Win32 executable,
    using Zig 0.13.0 as the C++17 cross-compiler for x86_64-windows-gnu.

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
        PRODUCTVERSION is the release version and should match the git tag
        (e.g. 4.0.0.0 for tag 4.0.0).
        FILEVERSION's 4th field is a build number. It is set automatically
        from the git commit count, so it needs no discipline and a given
        commit always produces the same number. A build made outside a git
        checkout leaves the existing number alone.

.PARAMETER Zig
    Directory containing zig.exe, or the path to zig.exe itself.
    Defaults to $env:ZIG_HOME, then to C:\Users\kuuha\zig-0.13.0.

.PARAMETER NoVersionBump
    Skip the FILEVERSION build-number update.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\build.ps1
#>
[CmdletBinding()]
param(
    [string] $Zig,
    [switch] $NoVersionBump
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root

$RequiredZig   = '0.13.0'
$RcFile        = Join-Path $root 'peppersnap.rc'
$OutDir        = Join-Path $root 'build'
$OutExe        = Join-Path $OutDir 'PepperSnap.exe'
$OutRes        = Join-Path $OutDir 'peppersnap.res'

$Sources = @(
    'PepperSnap.cpp'
    'Settings.cpp'
    'Registry.cpp'
    'InputHook.cpp'
    'Capture.cpp'
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

# ------------------------------------------------------------- version bump
function Set-BuildNumber {
    if ($NoVersionBump) { Write-Warn2 'version bump skipped (-NoVersionBump)'; return $null }

    $inside = $false
    try { & git rev-parse --is-inside-work-tree *> $null; $inside = ($LASTEXITCODE -eq 0) } catch { $inside = $false }
    if (-not $inside) {
        Write-Warn2 'not a git checkout - leaving FILEVERSION build number unchanged'
        return $null
    }

    # The 4th field is the ordinal of the commit that will contain this build.
    $count = [int]((& git rev-list --count HEAD).Trim())
    $buildNumber = $count + 1

    $text = [System.IO.File]::ReadAllText($RcFile)
    $m = [regex]::Match($text, 'FILEVERSION\s+(\d+),(\d+),(\d+),(\d+)')
    if (-not $m.Success) { throw "Could not find FILEVERSION in $RcFile" }
    $major = $m.Groups[1].Value; $minor = $m.Groups[2].Value; $patch = $m.Groups[3].Value

    $expectedNow = "$major.$minor.$patch.$buildNumber"
    $numericOk = ($m.Groups[4].Value -eq [string]$buildNumber)
    $stringOk  = ($text -match ('"FileVersion",\s*"' + [regex]::Escape($expectedNow)))
    if ($numericOk -and $stringOk) {
        Write-Ok "FILEVERSION already $major,$minor,$patch,$buildNumber"
        return $buildNumber
    }

    $text = $text -replace 'FILEVERSION\s+\d+,\d+,\d+,\d+', "FILEVERSION $major,$minor,$patch,$buildNumber"
    # The resource script writes these with a trailing escape ("4.0.0.5\0"), so match
    # the dotted version without requiring a closing quote straight after it.
    $text = $text -replace '("FileVersion",\s*")\d+(\.\d+){3}', "`${1}$major.$minor.$patch.$buildNumber"

    # Guard: never let a half-applied rewrite through again.
    $expected = "$major.$minor.$patch.$buildNumber"
    if ($text -notmatch ('FILEVERSION\s+' + [regex]::Escape("$major,$minor,$patch,$buildNumber"))) {
        throw "version rewrite failed: FILEVERSION does not read $expected in $RcFile"
    }
    if ($text -notmatch ('"FileVersion",\s*"' + [regex]::Escape($expected))) {
        throw "version rewrite failed: the FileVersion string does not read $expected in $RcFile"
    }

    [System.IO.File]::WriteAllText($RcFile, $text, (New-Object System.Text.UTF8Encoding($false)))
    Write-Ok "FILEVERSION -> $major,$minor,$patch,$buildNumber  (file version $expected)"
    return $buildNumber
}

# ---------------------------------------------------------------- compile
Write-Step 'compiling resources'
if (-not (Test-Path -LiteralPath $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

$buildNumber = Set-BuildNumber

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
Write-Ok ("PepperSnap.exe  {0:N0} bytes ({1:N2} MiB)" -f $sizeBytes, ($sizeBytes / 1MB))

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
