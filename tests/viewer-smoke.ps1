#requires -Version 7.0
<# Verify the six launcher routes, raw animation and menu scene lifetimes. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$viewerExe = Join-Path $projectRoot 'bin/Debug/GunBrosViewer.exe'
$outputDirectory = Join-Path $PSScriptRoot 'out/viewer-smoke'
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$viewerConfig = Join-Path $outputDirectory 'viewer.cfg'
# Use distinct dimensions so every scene must consume the viewer's own settings.
Set-Content -LiteralPath $viewerConfig -Value "WindowWidth=800`nWindowHeight=600`nEffectsVolume=7"
$gameConfig = Join-Path $projectRoot 'bin/Debug/GunBrosRe.cfg'
$gameConfigHash = ''
if (Test-Path -LiteralPath $gameConfig) { $gameConfigHash = (Get-FileHash -LiteralPath $gameConfig).Hash }

function Invoke-Viewer {
    param([string]$Name, [string[]]$Options, [string]$InputText = '', [int]$ExpectedExit = 0)
    $start = [Diagnostics.ProcessStartInfo]::new($viewerExe)
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.RedirectStandardInput = $true
    foreach ($argument in $Options) { $start.ArgumentList.Add($argument) }
    if ('--config' -notin $Options) {
        $start.ArgumentList.Add('--config')
        $start.ArgumentList.Add($viewerConfig)
    }
    $start.ArgumentList.Add('--big')
    $start.ArgumentList.Add((Join-Path $projectRoot 'big'))
    $start.ArgumentList.Add('--mute')
    $process = [Diagnostics.Process]::Start($start)
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    if ($InputText) { $process.StandardInput.Write($InputText) }
    $process.StandardInput.Close()
    if (-not $process.WaitForExit(60000)) {
        $process.Kill($true)
        throw "Viewer timed out: $Name"
    }
    $log = $stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()
    Set-Content -LiteralPath (Join-Path $outputDirectory "$Name.log") -Value $log
    $exitCode = $process.ExitCode
    $process.Dispose()
    if ($exitCode -ne $ExpectedExit) { throw "Viewer $Name exit=$exitCode expected=$ExpectedExit" }
    Write-Host "[viewer-smoke] $Name passed exit=$exitCode"
    return $log
}

$cases = [ordered]@{
    map = @('--map', 'pack2', '0', '--collisions')
    mesh = @('--mesh', '0')
    enemy = @('--enemy', '16')
    weapon = @('--weapon', '65', '--fire')
    armor = @('--armor', '0')
    arena = @('--arena', '16', '--gun', '0')
}
foreach ($name in $cases.Keys) {
    $image = Join-Path $outputDirectory "$name.png"
    $log = Invoke-Viewer $name ($cases[$name] + @('--screenshot', $image, '--advance', '400'))
    if (-not (Test-Path -LiteralPath $image)) { throw "Missing capture: $name" }
    if ($log -notmatch '\[viewer-window\] GunBrosViewer - .* requested=800x600') { throw "Viewer settings missing: $name" }
    if ($log -notmatch '\[viewer-controls\] scene=400x600 panel=400 expanded' -or
        $log -notmatch 'Return / Esc: Return to menu|Playback / return / Esc: Return to menu') {
        throw "Viewer controls missing: $name"
    }
    $png = [IO.File]::ReadAllBytes($image)
    $width = ([int]$png[16] -shl 24) + ([int]$png[17] -shl 16) + ([int]$png[18] -shl 8) + [int]$png[19]
    $height = ([int]$png[20] -shl 24) + ([int]$png[21] -shl 16) + ([int]$png[22] -shl 8) + [int]$png[23]
    if ($width -ne 800 -or $height -ne 600) { throw "Viewer dimensions not applied: $name ${width}x$height" }
    if ($name -eq 'enemy' -and $log -notmatch '2 configs -> 2 parts') { throw 'Shared turret assembly missing' }
    if ($name -eq 'weapon' -and $log -notmatch 'shots=[1-9]') { throw 'Weapon presentation did not fire' }
}
$still = Join-Path $outputDirectory 'mesh-still.png'
$null = Invoke-Viewer 'mesh-still' @('--mesh', '0', '--screenshot', $still)
if ((Get-FileHash $still).Hash -eq (Get-FileHash (Join-Path $outputDirectory 'mesh.png')).Hash) {
    throw 'Raw mesh animation did not change the rendered pose'
}
$menuImage = Join-Path $outputDirectory 'menu.png'
$menu = Invoke-Viewer 'menu-return' @('--screenshot', $menuImage) "1`n2`n3`n4`n5`n6`n0`n"
if ([regex]::Matches($menu, '=== GunBrosViewer ===').Count -ne 7) { throw 'A scene did not return to the launcher' }
if ([regex]::Matches($menu, '\[png\] wrote').Count -ne 6) { throw 'A menu route did not render' }
$null = Invoke-Viewer 'invalid-mode' @('--m1') '' 1
$null = Invoke-Viewer 'conflicting-modes' @('--mesh', '--enemy') '' 1
$null = Invoke-Viewer 'invalid-map' @('--map', 'missing-pack', '0') '' 1
$null = Invoke-Viewer 'invalid-mesh' @('--mesh', '999999') '' 1
$null = Invoke-Viewer 'invalid-enemy-state' @('--enemy', '0', '--state', '999999') '' 1
# Default creation, legacy fields and malformed values must stay isolated from game settings.
$newConfig = Join-Path $outputDirectory ('new-' + [Guid]::NewGuid().ToString('N') + '.cfg')
$null = Invoke-Viewer 'new-config' @('--config', $newConfig) "0`n"
$generated = Get-Content -LiteralPath $newConfig -Raw
if ($generated -notmatch 'WindowWidth=1600' -or $generated -notmatch 'WindowHeight=1200' -or
    $generated -match 'DebugMode|IsConnected') { throw 'New viewer configuration has the wrong schema' }
$legacyConfig = Join-Path $outputDirectory 'legacy.cfg'
Set-Content -LiteralPath $legacyConfig -Value "EffectsVolume=5`nDebugMode=1`nIsConnected=1"
$legacyHash = (Get-FileHash -LiteralPath $legacyConfig).Hash
$legacy = Invoke-Viewer 'legacy-config' @('--config', $legacyConfig) "0`n"
if ($legacy -notmatch 'effects-volume=5' -or $legacy -notmatch 'ignored setting: DebugMode') { throw 'Legacy viewer configuration failed' }
if ((Get-FileHash -LiteralPath $legacyConfig).Hash -ne $legacyHash) { throw 'Existing configuration was rewritten' }
$invalidConfig = Join-Path $outputDirectory 'invalid.cfg'
foreach ($invalid in @('WindowWidth=0', 'WindowHeight=-1', 'EffectsVolume=11', 'WindowWidth=800junk')) {
    Set-Content -LiteralPath $invalidConfig -Value $invalid
    $null = Invoke-Viewer ('invalid-config-' + $invalid.Split('=')[0]) @('--config', $invalidConfig) '' 1
}
if ($gameConfigHash -and (Get-FileHash -LiteralPath $gameConfig).Hash -ne $gameConfigHash) {
    throw 'Viewer modified the game configuration'
}
Write-Host "[viewer-smoke] all checks passed; artifacts=$outputDirectory"
