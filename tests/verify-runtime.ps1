#requires -Version 7.0
<# Verify the Release executables in bin/Release without creating another EXE directory. #>
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$runtimeDirectory = Join-Path $root 'bin/Release'
$reportDirectory = Join-Path $PSScriptRoot 'out/runtime'
New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
$viewer = Join-Path $runtimeDirectory 'GunBrosViewer.exe'
Push-Location $env:TEMP
try {
    & $viewer --mute --m1 > (Join-Path $reportDirectory 'resources.log')
    if ($LASTEXITCODE -ne 0) { throw "Resource check failed: $LASTEXITCODE" }
    & $viewer --mute --help > (Join-Path $reportDirectory 'viewer-help.log')
    if ($LASTEXITCODE -ne 0) { throw 'Release viewer help failed' }
    foreach ($option in @('--weapon-check', '--screenshot', '--start-wave', '--powerup-study')) {
        & $viewer --mute $option > (Join-Path $reportDirectory 'rejected-option.log')
        if ($LASTEXITCODE -eq 0) { throw "Release accepted $option" }
    }
} finally { Pop-Location }

# Close the game through its normal window message handler after the menu is ready.
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class RuntimeWindow {
    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
}
'@
$stdout = Join-Path $reportDirectory 'game-stdout.log'
$stderr = Join-Path $reportDirectory 'game-stderr.log'
$profileDirectory = Join-Path $reportDirectory 'saves'
$process = Start-Process -FilePath (Join-Path $runtimeDirectory 'GunBrosRe.exe') -ArgumentList @('--mute', '--skip-intro', '--profile', ('"' + $profileDirectory + '"')) -WorkingDirectory $env:TEMP -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
$timer = [Diagnostics.Stopwatch]::StartNew()
$loaded = $false
try {
    while (-not $process.WaitForExit(200) -and $timer.Elapsed.TotalSeconds -lt 30) {
        $process.Refresh()
        if ((Test-Path -LiteralPath $stdout) -and (Select-String -LiteralPath $stdout -SimpleMatch '[menu] ready' -Quiet) -and $process.MainWindowHandle -ne [IntPtr]::Zero) {
            $loaded = $true
            $null = [RuntimeWindow]::PostMessage($process.MainWindowHandle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
            break
        }
    }
    if (-not $process.WaitForExit(10000)) { throw 'Game did not close after WM_CLOSE' }
    if (-not $loaded -or $process.ExitCode -ne 0) { throw "Game failed: loaded=$loaded exit=$($process.ExitCode)" }
    if (-not (Test-Path -LiteralPath (Join-Path $profileDirectory '-1_1000'))) { throw 'Game did not save its test account' }
    [pscustomobject]@{ Resources = 'Passed'; OtherWorkingDirectory = 'Passed'; Menu = 'Passed'; Save = 'Passed'; ReleaseRejectsTests = 'Passed'; ExitCode = $process.ExitCode } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $reportDirectory 'result.json')
    Write-Output '[runtime] resources, working directory, menu, save, and release command checks passed'
} finally {
    if (-not $process.HasExited) { $process.Kill($true); $process.WaitForExit() }
    $process.Dispose()
}
exit 0
