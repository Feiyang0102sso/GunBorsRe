#requires -Version 7.0
<#
.SYNOPSIS
Run the permanent checks individually, retaining muted regression logs and fixture integrity results.
.EXAMPLE
pwsh -File tests/run.ps1 -Configuration Debug -Phase Core
.EXAMPLE
pwsh -File tests/run.ps1 -Phase LongRun
.EXAMPLE
pwsh -File tests/run.ps1 -Phase Core,UI,OriginalUI,Extended,Boundary
.EXAMPLE
pwsh -File tests/run.ps1 -Phase Core -Case original-saves -List
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug')]
    [string]$Configuration = 'Debug',
    [string[]]$Phase = @('Core','UI','OriginalUI','Extended','Smoke','Boundary','Campaign','LongRun'),
    [string]$ExecutablePath = '',
    [string[]]$Case = @('resources'),
    [ValidateRange(1, 7200)]
    [int]$TimeoutSeconds = 300,
    [switch]$List,
    [switch]$NoBuild
)

# Accept both PowerShell arrays and comma-separated arguments from -File/MSBuild.
$Case = @($Case -split ',')
$Phase = @($Phase -split ',')
$validPhases = @('Core','UI','OriginalUI','Extended','Smoke','Campaign','Boundary','LongRun')
foreach ($suite in $Phase) {
    if ($suite -notin $validPhases) { throw "Unknown suite: $suite" }
}
if (-not $PSBoundParameters.ContainsKey('Case') -and ($List -or $PSBoundParameters.ContainsKey('Phase'))) { $Case = @() }

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
# Checks belong to the permanent companion. The GUI is verified separately.
$gameExe = Join-Path $projectRoot "bin/$Configuration/GunBrosTests.exe"
$formalExe = Join-Path $projectRoot "bin/$Configuration/GunBrosRe.exe"
if ($ExecutablePath) { $gameExe = (Resolve-Path -LiteralPath $ExecutablePath).Path }
$reportDirectory = [System.IO.Path]::GetFullPath((Join-Path $projectRoot 'tests/out'))

# Snapshot source saves and real accounts. Tests write only their own out/ files.
# Stage 1: out/ now means tests/out; BIG and the host cfg are also protected.
function Get-ProtectedFiles {
    $snapshot = [System.Collections.Generic.List[object]]::new()
    foreach ($folderName in @('tests/fixtures/saves', 'userdata', 'big')) {
        $folder = Join-Path $projectRoot $folderName
        if (-not (Test-Path -LiteralPath $folder)) { continue }
        foreach ($file in (Get-ChildItem -LiteralPath $folder -File -Recurse | Sort-Object FullName)) {
            $snapshot.Add([pscustomobject]@{
                Path = $file.FullName
                Hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
            })
        }
    }
    $config = Join-Path $projectRoot "bin/$Configuration/GunBrosRe.cfg"
    if (Test-Path -LiteralPath $config -PathType Leaf) {
        $snapshot.Add([pscustomobject]@{
            Path = $config
            Hash = (Get-FileHash -LiteralPath $config -Algorithm SHA256).Hash
        })
    }
    return $snapshot.ToArray()
}

$checks = [System.Collections.Generic.List[object]]::new()
function Add-Check {
    param([string]$Name, [string[]]$Arguments, [int]$ExpectedExit = 0, [switch]$Game)
    $checks.Add([pscustomobject]@{ Suite = $suite; Name = $Name; Arguments = $Arguments; ExpectedExit = $ExpectedExit; Game = $Game.IsPresent })
}

foreach ($suite in ($Phase | Select-Object -Unique)) {
    if ($suite -eq 'Core') {
        Add-Check 'debug-input' @('--debug-input-check')
        Add-Check 'fontbitmap' @('--fontbitmap')
        Add-Check 'viewer-controls' @('--viewer-controls-check')
        Add-Check 'map-turret' @('--map-turret-check')
        Add-Check 'big-version' @('--big-version-check')
        Add-Check 'asset-sample' @('--asset-sample-check')
        Add-Check 'resources' @('--m1')
        Add-Check 'weapons' @('--weapon-check')
        Add-Check 'weapon-effects' @('--weapon-effects-check')
        Add-Check 'mines' @('--mine-check')
        Add-Check 'audio-transitions' @('--audio-transitions-check')
        Add-Check 'postgame-presentation' @('--postgame-presentation-check')
        Add-Check 'player-death' @('--player-death-check')
        Add-Check 'local-live' @('--local-live-check')
        Add-Check 'armor-data' @('--armor-check')
        Add-Check 'enemies' @('--arena-check')
        Add-Check 'boss' @('--boss-check')
        Add-Check 'map-occlusion' @('--map-occlusion-check')
        Add-Check 'pickups' @('--pickup-check')
        Add-Check 'props' @('--prop-check') 1
        Add-Check 'prop-combat' @('--prop-combat-check')
        Add-Check 'actor-feedback' @('--actor-feedback-check')
        Add-Check 'powerups' @('--powerup-check')
        Add-Check 'missions' @('--mission-check')
        Add-Check 'level-flow' @('--level-flow-check')
        Add-Check 'progress' @('--progress-check')
        Add-Check 'daily-bonus' @('--daily-bonus-check')
        Add-Check 'armor-render' @('--armor-render-check')
        Add-Check 'pickup-render' @('--pickup-render-check')
        Add-Check 'profile-play' @('--profile-play-check')
        Add-Check 'brother' @('--brother-check')
        Add-Check 'tutorial' @('--tutorial-check')
        Add-Check 'powerup-play' @('--powerup-play-check')
        Add-Check 'menu' @('--game-menu-check')
        Add-Check 'original-saves' @('--original-save-check')
        Add-Check 'original-profile' @('--original-profile-check')
    } elseif ($suite -eq 'UI') {
        Add-Check 'media' @('--media-check')
        Add-Check 'intro' @('--intro')
        Add-Check 'movies' @('--movie-check')
        Add-Check 'movie-gallery' @('--movie-gallery')
        Add-Check 'hud' @('--hud-check')
        Add-Check 'menu' @('--game-menu-check')
        Add-Check 'horde-first' @('--horde-check', '0', '--weapon', '80')
        Add-Check 'horde-last' @('--horde-check', '9', '--weapon', '80')
    } elseif ($suite -eq 'OriginalUI') {
        # Native resources, real hit regions, timeline boundaries and save reloads.
        Add-Check 'native-profile' @('--native-profile-check')
        Add-Check 'native-profile-play' @('--native-profile-play-check')
        Add-Check 'tutorial-native' @('--tutorial-check')
        Add-Check 'store-cards' @('--store-card-check')
        Add-Check 'dual-weapon' @('--dual-weapon-check')
        Add-Check 'combat-feedback' @('--combat-feedback-check')
        Add-Check 'mastery-upgrade' @('--upgrade-popup-check')
        Add-Check 'bank' @('--bank-check')
        Add-Check 'options' @('--options-check')
        Add-Check 'offline-social' @('--social-check')
        Add-Check 'planet-menu' @('--planet-menu-check')
        Add-Check 'mission-menu' @('--mission-menu-check')
        Add-Check 'header' @('--header-check')
        Add-Check 'refinery-menu' @('--refinery-menu-check')
        Add-Check 'greeting' @('--greeting-check')
        Add-Check 'player-select' @('--player-select-check')
        Add-Check 'postgame-menu' @('--postgame-menu-check')
        Add-Check 'pause-menu' @('--pause-check')
        Add-Check 'original-hud' @('--original-hud-check')
        Add-Check 'powerup-selector' @('--powerup-selector-check')
    } elseif ($suite -eq 'Extended') {
        # Existing permanent checks that were missing from the original suite lists.
        Add-Check 'play-interaction' @('--play-interaction-check')
        Add-Check 'store-template' @('--store-template-check')
        Add-Check 'ui-feedback' @('--ui-feedback-check')
        Add-Check 'package-purchase' @('--package-purchase-check')
        Add-Check 'promotion' @('--promotion-check')
        Add-Check 'loading-wipe' @('--loading-wipe-check')
        Add-Check 'scene-transition' @('--scene-transition-check')
        Add-Check 'dialog' @('--dialog-check')
        Add-Check 'performance' @('--performance-check')
        Add-Check 'spawn-performance' @('--spawn-performance-check')
        Add-Check 'spawn-performance-realtime' @('--spawn-performance-realtime-check')
        Add-Check 'path-cache' @('--path-cache-check')
        Add-Check 'flock' @('--flock-check')
        Add-Check 'flock-performance' @('--flock-performance-check')
    } elseif ($suite -eq 'Smoke') {
        # The actual GUI entry uses explicit screenshot/profile arguments. Pipe
        # redirection keeps OpenGameLog from writing to the real userdata directory.
        Add-Check 'game-menu' @('--skip-intro', '--menu-page', '2') -Game
    } elseif ($suite -eq 'Campaign') {
        Add-Check 'campaign-doors' @('--campaign-door-check')
        Add-Check 'debug-map-profile' @('--debug-map-profile-check')
        Add-Check 'campaign-content' @('--campaign-content-check')
        Add-Check 'campaign-targets' @('--campaign-target-check')
        Add-Check 'campaign-progression' @('--campaign-progression-check')
        Add-Check 'campaign-rescue' @('--campaign-rescue-check')
        Add-Check 'campaign-portal' @('--campaign-portal-check')
        Add-Check 'campaign-cache' @('--campaign-cache-check')
        foreach ($mission in @(10, 11, 12, 13, 14)) {
            Add-Check "campaign-pack2-$mission" @('--campaign-check', 'pack2', "$mission", '--weapon', '65')
        }
        Add-Check 'campaign-pack7-0' @('--campaign-check', 'pack7', '0', '--weapon', '65')
    } elseif ($suite -eq 'Boundary') {
        Add-Check 'final-pack2' @('--survival-check', '--map', 'pack2', '7', '--weapon', '65', '--start-wave', '499', '--check-waves', '2')
        Add-Check 'final-pack7' @('--survival-check', '--map', 'pack7', '6', '--weapon', '65', '--start-wave', '499', '--check-waves', '2')
        Add-Check 'final-pack9' @('--survival-check', '--map', 'pack9', '0', '--weapon', '65', '--start-wave', '499', '--check-waves', '2')
        Add-Check 'final-pack12' @('--survival-check', '--map', 'pack12', '0', '--weapon', '65', '--start-wave', '499', '--check-waves', '2')
    } else {
        Add-Check 'survival-pack2' @('--survival-check', '--map', 'pack2', '7', '--weapon', '65', '--check-waves', '500')
        Add-Check 'survival-pack7' @('--survival-check', '--map', 'pack7', '6', '--weapon', '65', '--check-waves', '500')
        Add-Check 'survival-pack9' @('--survival-check', '--map', 'pack9', '0', '--weapon', '65', '--check-waves', '500')
        Add-Check 'survival-pack12' @('--survival-check', '--map', 'pack12', '0', '--weapon', '65', '--check-waves', '500')
    }
}

if ($Case.Count -gt 0) {
    foreach ($name in $Case) {
        if ($name -notin $checks.Name) { throw "Unknown case in selected suites: $name" }
    }
    $selected = [System.Collections.Generic.List[object]]::new()
    foreach ($check in $checks) {
        if ($check.Name -in $Case) { $selected.Add($check) }
    }
    $checks = $selected
}
if ($List) { $checks.ToArray(); exit 0 }


# Debug tests are built on demand; product Debug targets use -NoBuild after building this dependency.
if (-not $NoBuild) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
    if (-not $msbuild) { throw 'MSBuild not found' }
    & $msbuild (Join-Path $projectRoot 'GunBrosTests.vcxproj') /p:Configuration=Debug /p:Platform=x64 /p:SkipAutoTests=true /m /v:minimal /nologo
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
if (-not (Test-Path -LiteralPath $gameExe -PathType Leaf)) {
    throw "Build the Debug test target first: $gameExe"
}

# Validate inputs before touching previous evidence. No source assets are moved.
foreach ($inputName in @('big', 'tests/fixtures/saves', 'assets/shaders', 'assets/audio', 'assets/startup')) {
    if (-not (Test-Path -LiteralPath (Join-Path $projectRoot $inputName) -PathType Container)) {
        throw "Missing test input directory: $inputName"
    }
}
foreach ($recordName in @('-1_1000', '-1_1001', '-1_1002', '-1_1003', '-1_1013')) {
    if (-not (Test-Path -LiteralPath (Join-Path $projectRoot "tests/fixtures/saves/$recordName") -PathType Leaf)) {
        throw "Missing standard save fixture: $recordName"
    }
}
if ($gameExe.StartsWith($reportDirectory + [System.IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The executable cannot live inside the test cleanup directory'
}

# One lock spans cleanup, execution and integrity checks, including other suites.
$lockPath = Join-Path $projectRoot 'tests/run.lock'
$mutexName = 'Local\GunBrosTests-' + [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($projectRoot))).Substring(0, 16)
$runLock = [Threading.Mutex]::new($false, $mutexName)
Write-Output '[validation] waiting for the test workspace'
try { $null = $runLock.WaitOne() }
catch [Threading.AbandonedMutexException] { Write-Output '[validation] recovered interrupted test lock' }
$before = $null
$failed = $false
$knownIssues = 0
$results = [System.Collections.Generic.List[object]]::new()
try {
    foreach ($running in (Get-Process -ErrorAction SilentlyContinue)) {
        if ($running.ProcessName -match '^(GunBrosRe|GunBrosTests|GunBrosRe|GunBrosViewer)$' -or
            $running.Path -eq $gameExe -or $running.Path -eq $formalExe) {
            throw "Close the existing game/research process before validation: $($running.Id)"
        }
    }

    # A stale companion may ignore the new output argument and write to old paths.
    # Its help is read-only; reject it before clearing the previous run.
    $usage = & $gameExe --mute --help
    if ($LASTEXITCODE -ne 0 -or ($usage -join "`n") -notmatch '--test-output') {
        throw 'Rebuild the research executable: --test-output is not supported'
    }

    # Only this exact repository-local target may be recursively removed. Reject
    # junctions/symlinks in the directory tree instead of following their targets.
    $expectedOutput = [System.IO.Path]::GetFullPath((Join-Path $projectRoot 'tests/out'))
    if ($reportDirectory -ne $expectedOutput) { throw 'Unexpected cleanup target' }
    $testDirectory = Get-Item -LiteralPath (Join-Path $projectRoot 'tests')
    if ($testDirectory.Attributes -band [System.IO.FileAttributes]::ReparsePoint) { throw 'Tests directory is a reparse point' }
    if (Test-Path -LiteralPath $reportDirectory) {
        $outputItem = Get-Item -LiteralPath $reportDirectory
        if ($outputItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) { throw 'Output directory is a reparse point' }
        foreach ($item in (Get-ChildItem -LiteralPath $reportDirectory -Recurse -Force)) {
            if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) { throw "Reparse point in test output: $($item.FullName)" }
        }
        Write-Output "[validation] cleanup=$reportDirectory"
        Remove-Item -LiteralPath $reportDirectory -Recurse -Force
    }
    New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
    $before = @(Get-ProtectedFiles)
    $before | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $reportDirectory 'protected-before.json') -Encoding UTF8
    $checks.ToArray() | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $reportDirectory 'plan.json') -Encoding UTF8
    Write-Output "[validation] $Configuration $Phase logs=$reportDirectory"
    foreach ($check in $checks) {
        $caseDirectory = Join-Path $reportDirectory "$($check.Suite)/$($check.Name)"
        $logDirectory = Join-Path $caseDirectory 'logs'
        New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
        $caseExe = $gameExe
        $arguments = @('--mute', '--fixtures', (Join-Path $projectRoot 'tests/fixtures/saves'), '--test-output', $caseDirectory) + $check.Arguments
        if ($check.Game) {
            $caseExe = $formalExe
            $imageDirectory = Join-Path $caseDirectory 'images'
            New-Item -ItemType Directory -Path $imageDirectory -Force | Out-Null
            $arguments = @('--mute', '--profile', (Join-Path $caseDirectory 'saves'),
                '--screenshot', (Join-Path $imageDirectory 'menu.png')) + $check.Arguments
        }
        $stdout = Join-Path $logDirectory 'stdout.log'
        $stderr = Join-Path $logDirectory 'stderr.log'
        Write-Output "[validation] begin $($check.Suite)/$($check.Name)"
        $timer = [System.Diagnostics.Stopwatch]::StartNew()
        # ArgumentList preserves spaces without hand-quoting a Windows command line.
        $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
        $startInfo.FileName = $caseExe
        $startInfo.WorkingDirectory = $caseDirectory
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $startInfo.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
        $startInfo.RedirectStandardOutput = $true
        $startInfo.RedirectStandardError = $true
        foreach ($argument in $arguments) { $startInfo.ArgumentList.Add($argument) }
        $process = [System.Diagnostics.Process]::Start($startInfo)
        $stdoutFile = [System.IO.File]::Open($stdout, 'Create', 'Write', 'Read')
        $stderrFile = [System.IO.File]::Open($stderr, 'Create', 'Write', 'Read')
        try {
            # Drain both pipes concurrently, writing logs even if a check stalls.
            $stdoutCopy = $process.StandardOutput.BaseStream.CopyToAsync($stdoutFile)
            $stderrCopy = $process.StandardError.BaseStream.CopyToAsync($stderrFile)
            $completed = $process.WaitForExit($TimeoutSeconds * 1000)
            if (-not $completed) { $process.Kill($true); $process.WaitForExit() }
            $null = $stdoutCopy.GetAwaiter().GetResult()
            $null = $stderrCopy.GetAwaiter().GetResult()
            $exitCode = $process.ExitCode
        } finally {
            if (-not $process.HasExited) { $process.Kill($true); $process.WaitForExit() }
            $stdoutFile.Dispose()
            $stderrFile.Dispose()
            $process.Dispose()
        }
        $timer.Stop()
        $status = 'Passed'
        if ($exitCode -ne $check.ExpectedExit) { $status = 'Failed' }
        if (-not $completed) { $status = 'TimedOut' }
        if ($check.Name -eq 'props' -and $status -eq 'Passed') {
            # Do not allow exit 1 to hide another resource error or a crash.
            $diagnostic = Get-Content -LiteralPath $stdout -Raw
            if ($diagnostic -match '\[prop-check\] invalid pack9:43 remaining=0' -and
                $diagnostic -match '\[prop-check\] templates=264 scripts=15 moves=35 actions=35 failures=1' -and
                $diagnostic -notmatch '\[prop-check\] malformed-reference') {
                $status = 'KnownDataIssue'
                ++$knownIssues
            } else { $status = 'Failed' }
        }
        $results.Add([pscustomobject]@{
            Suite = $check.Suite
            Name = $check.Name
            Executable = $caseExe
            ExecutableHash = (Get-FileHash -LiteralPath $caseExe -Algorithm SHA256).Hash
            Arguments = $arguments
            ExitCode = $exitCode
            ExpectedExit = $check.ExpectedExit
            Seconds = [Math]::Round($timer.Elapsed.TotalSeconds, 3)
            Status = $status
            Log = $stdout
            ErrorLog = $stderr
            OutputDirectory = $caseDirectory
        })
        $results.ToArray() | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $reportDirectory 'results.json') -Encoding UTF8
        Write-Output "[validation] $($check.Name) $status exit=$exitCode seconds=$([Math]::Round($timer.Elapsed.TotalSeconds, 1))"
        if ($status -eq 'Failed' -or $status -eq 'TimedOut') { $failed = $true; break }
    }
} finally {
    try {
        if ($null -ne $before) {
            $after = @(Get-ProtectedFiles)
            $after | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $reportDirectory 'protected-after.json') -Encoding UTF8
            $changed = @(Compare-Object -ReferenceObject $before -DifferenceObject $after -Property Path, Hash)
            if ($changed.Count -ne 0) {
                $changed | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $reportDirectory 'protected-changes.json') -Encoding UTF8
                Write-Output '[validation] FAILED: protected save files changed'
                $failed = $true
            }
            Write-Output "[validation] completed=$($results.Count)/$($checks.Count) known-data-issues=$knownIssues protected-changes=$($changed.Count)"
            [pscustomobject]@{
                Configuration = $Configuration
                Phases = $Phase
                Completed = $results.Count
                Planned = $checks.Count
                KnownDataIssues = $knownIssues
                ProtectedChanges = $changed.Count
                Failed = $failed -or ($results.Count -ne $checks.Count)
                ExecutableHash = (Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash
                FinishedAt = (Get-Date).ToUniversalTime().ToString('o')
            } | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $reportDirectory 'summary.json') -Encoding UTF8
        }
    } finally { $runLock.ReleaseMutex(); $runLock.Dispose() }
}
if ($failed) { exit 1 }
exit 0


