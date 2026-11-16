# Builds the Windows PC test build of Shadow-nx (from sheroes-nx's xbe-recomp branch): XboxXBE-Recomp's Windows host with the Switch's
# Vulkan renderer (build-pc\SHADOW-vk.exe) and the D3D12 one (build-pc\SHADOW-d3d12.exe). Every step's full output
# goes to out\logs\.
#
# Usage (from C:\Shadow-nx): powershell -ExecutionPolicy Bypass -File tools\win\build_pc.ps1 [-Jobs N] [-Clean]
#   -Jobs   parallel compile jobs (default: half the logical CPUs, at most 8)
#   -Clean  delete build-pc first
#
# Steps: copy default.xbe (assets\xbe\default.xbe, else assets\game_root\default.xbe) into game\assets (the recompiler reads it there; it stays out of git),
# configure, build the tools, recompile default.xbe into game\generated\game, compile and link both executables.
# At run time the game is read from C:\Shadow-nx\assets (the .iso there, or default.xbe with its folders).

param(
    [int]$Jobs = 0,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$logDir = Join-Path $repo 'out\logs'
$build = Join-Path $repo 'build-pc'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

function Step($name, $logName, [scriptblock]$body) {
    $log = Join-Path $logDir $logName
    Write-Host ''
    Write-Host "== $name  (log: out\logs\$logName)"
    $start = Get-Date
    $sw = [System.IO.StreamWriter]::new($log, $false, [System.Text.UTF8Encoding]::new($false))
    $sw.AutoFlush = $true
    try {
        & { $ErrorActionPreference = 'Continue'; & $body } 2>&1 | ForEach-Object { "$_" } |
            ForEach-Object -Begin { $n = 0 } -Process {
                $sw.WriteLine($_)
                $n++
                if ($_ -match '^\[(\d+)/(\d+)\]') {
                    if (([int]$matches[1] % 25) -eq 0 -or $matches[1] -eq $matches[2]) { Write-Host "   $($matches[0]) steps" }
                } elseif ($_ -match 'error:|FAILED') { Write-Host "   $_" }
            }
    } finally {
        $sw.Close()
    }
    if ($LASTEXITCODE -ne 0) {
        Write-Host ''
        Write-Host "FAILED at: $name. Tell Claude; the log is out\logs\$logName"
        exit 1
    }
    Write-Host ("   done in {0:n1} min" -f ((Get-Date) - $start).TotalMinutes)
}

. (Join-Path $PSScriptRoot 'env.ps1')

# Shadow-nx: default.xbe taken out of the disc image into assets\xbe\ (2026-11-16; not assets\ itself, where the
# runtime would take a lone default.xbe for an extracted disc ahead of the .iso), or the extracted disc's
$xbeSrc = Join-Path $repo 'assets\xbe\default.xbe'
if (-not (Test-Path $xbeSrc)) { $xbeSrc = Join-Path $repo 'assets\game_root\default.xbe' }
$xbeDst = Join-Path $repo 'game\assets\default.xbe'
if (-not (Test-Path $xbeSrc)) {
    Write-Host 'No default.xbe in assets\xbe\ or assets\game_root\.'
    exit 1
}
New-Item -ItemType Directory -Force -Path (Split-Path $xbeDst) | Out-Null
if (-not (Test-Path $xbeDst) -or (Get-FileHash $xbeSrc).Hash -ne (Get-FileHash $xbeDst).Hash) {
    Copy-Item $xbeSrc $xbeDst -Force
}
if ($Jobs -le 0) { $Jobs = [math]::Max(1, [math]::Min(8, [int]([Environment]::ProcessorCount / 2))) }
if ($Clean -and (Test-Path $build)) { Remove-Item -Recurse -Force $build }

Step 'Configure' 'pc_configure.log' {
    cmake -S (Join-Path $repo 'pc') -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DSHADOW_VERBOSE_LOG=OFF
}
Step 'Compile (recompiles default.xbe first)' 'pc_build.log' {
    ninja -C $build -k 0 "-j$Jobs" SHADOW-vk SHADOW-d3d12
}
Write-Host ''
Write-Host 'Built build-pc\SHADOW-vk.exe and build-pc\SHADOW-d3d12.exe'
