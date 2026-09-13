#requires -Version 7.0
<# Exercise shipped shortcuts through Win32 messages; restore the cfg byte-for-byte. #>
[CmdletBinding()]
param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$runtimeDirectory = Join-Path $repoRoot "bin/$Configuration"
$executable = Join-Path $runtimeDirectory 'GunBrosRe.exe'
$configPath = Join-Path $runtimeDirectory 'GunBrosRe.cfg'
$reportDirectory = Join-Path $PSScriptRoot "out/debug-keys-$Configuration"
New-Item -ItemType Directory -Force -Path $reportDirectory | Out-Null
$configExisted = Test-Path -LiteralPath $configPath
$configBytes = $null
if ($configExisted) { $configBytes = [IO.File]::ReadAllBytes($configPath) }
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class DebugKeyWindow {
    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr window, uint message, IntPtr key, IntPtr data);
    [DllImport("user32.dll")]
    private static extern uint MapVirtualKey(uint key, uint type);
    private static void Key(IntPtr window, uint key, bool released) {
        long data = 1L | ((long)MapVirtualKey(key, 0) << 16);
        uint message = 0x0100;
        if (released) { data |= 3L << 30; message = 0x0101; }
        PostMessage(window, message, new IntPtr(key), new IntPtr(data));
    }
    public static void Press(IntPtr window, uint key, bool shift) {
        if (shift) { Key(window, 0x10, false); }
        Key(window, key, false);
        Key(window, key, true);
        if (shift) { Key(window, 0x10, true); }
    }
}
'@
function Count-Log([string]$Path, [string]$Pattern) {
    if (-not (Test-Path -LiteralPath $Path)) { return 0 }
    return @(Select-String -LiteralPath $Path -SimpleMatch $Pattern).Count
}
function Wait-Log($Process, [string]$Path, [string]$Pattern, [int]$Count = 1) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ((Count-Log $Path $Pattern) -lt $Count) {
        if ($Process.HasExited -or $timer.Elapsed.TotalSeconds -gt 30) { throw "Missing log: $Pattern (count=$Count)" }
        Start-Sleep -Milliseconds 100
    }
}
try {
    foreach ($enabled in @(0, 1)) {
        [IO.File]::WriteAllText($configPath, "DebugMode=$enabled`nDrawFPS=0`nEffectsVolume=0`nIsConnected=0`n")
        $stdout = Join-Path $reportDirectory "mode-$enabled.log"
        $stderr = Join-Path $reportDirectory "mode-$enabled-error.log"
        $profile = Join-Path $reportDirectory "profile-$enabled"
        $process = Start-Process -FilePath $executable -ArgumentList @('--mute', '--skip-intro', '--profile', ('"' + $profile + '"')) -WorkingDirectory $runtimeDirectory -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
        try {
            Wait-Log $process $stdout '[menu] ready'
            $process.Refresh()
            $window = $process.MainWindowHandle
            if ($window -eq [IntPtr]::Zero) { throw 'Game window is missing' }
            # Neither the old binding nor an unmodified M may open the picker.
            [DebugKeyWindow]::Press($window, 0x72, $true)
            [DebugKeyWindow]::Press($window, 0x4D, $false)
            Start-Sleep -Milliseconds 300
            if ((Count-Log $stdout '[debug-maps] maps=') -ne 0) { throw 'Old or unmodified map key is active' }
            [DebugKeyWindow]::Press($window, 0x4D, $true)
            if ($enabled -eq 0) {
                [DebugKeyWindow]::Press($window, 0x54, $true)
                Start-Sleep -Milliseconds 700
                if ((Count-Log $stdout '[debug-maps] maps=') -ne 0 -or (Count-Log $stdout '[debug-tutorial] start') -ne 0) { throw 'DebugMode=0 did not disable shortcuts' }
            } else {
                Wait-Log $process $stdout '[debug-maps] maps='
                [DebugKeyWindow]::Press($window, 0x0D, $false)
                Wait-Log $process $stdout '[debug-maps] launch'
                Wait-Log $process $stdout '[survival] WASD'
                [DebugKeyWindow]::Press($window, 0x43, $true)
                [DebugKeyWindow]::Press($window, 0x49, $true)
                Wait-Log $process $stdout '[debug] collisions=1'
                Wait-Log $process $stdout '[debug] info=0'
                # Open and dismiss the picker while already playing.
                [DebugKeyWindow]::Press($window, 0x4D, $true)
                Wait-Log $process $stdout '[debug-maps] maps=' 2
                [DebugKeyWindow]::Press($window, 0x1B, $false)
                Start-Sleep -Milliseconds 200
                [DebugKeyWindow]::Press($window, 0x1B, $false)
                Wait-Log $process $stdout '[menu] ready' 2
                [DebugKeyWindow]::Press($window, 0x54, $true)
                Wait-Log $process $stdout '[debug-tutorial] start'
                Wait-Log $process $stdout '[survival] WASD' 2
                [DebugKeyWindow]::Press($window, 0x1B, $false)
                Wait-Log $process $stdout '[debug-tutorial] return to menu'
                # The next menu must finish loading before requesting a normal close.
                Wait-Log $process $stdout '[menu] ready' 3
            }
            $null = [DebugKeyWindow]::PostMessage($window, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
            if (-not $process.WaitForExit(10000) -or $process.ExitCode -ne 0) { throw 'Game did not exit normally' }
            Write-Output "[debug-keys-runtime] configuration=$Configuration DebugMode=$enabled passed exit=0"
        } finally {
            if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
            $process.Dispose()
        }
    }
} finally {
    if ($configExisted) { [IO.File]::WriteAllBytes($configPath, $configBytes) }
    else { Remove-Item -LiteralPath $configPath -ErrorAction SilentlyContinue }
}
