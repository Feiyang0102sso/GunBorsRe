<#
.SYNOPSIS
逐项运行永久研究入口，保留静音回归日志和原存档完整性结果。
.EXAMPLE
.\test-muted.ps1 -Configuration Debug -Phase Core
.EXAMPLE
.\test-muted.ps1 -Phase LongRun
#>
[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    [ValidateSet('Core', 'UI', 'Campaign', 'Boundary', 'LongRun')]
    [string]$Phase = 'Core',
    [string]$ExecutablePath = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$gameExe = Join-Path $projectRoot "bin/x64/$Configuration/gun_bros_re.exe"
if ($ExecutablePath) { $gameExe = (Resolve-Path -LiteralPath $ExecutablePath).Path }
if (-not (Test-Path -LiteralPath $gameExe -PathType Leaf)) {
    throw "Build the $Configuration executable first: $gameExe"
}
$runName = '{0}-{1}-{2}' -f (Get-Date -Format 'yyyyMMdd-HHmmss-fff'), $Configuration, $Phase
$reportDirectory = Join-Path $projectRoot "out/validation/$runName"
New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null

# Snapshot source saves and real accounts. Tests write only their own out/ files.
function Get-ProtectedFiles {
    $snapshot = [System.Collections.Generic.List[object]]::new()
    foreach ($folderName in @('saves', 'userdata')) {
        $folder = Join-Path $projectRoot $folderName
        if (-not (Test-Path -LiteralPath $folder)) { continue }
        foreach ($file in (Get-ChildItem -LiteralPath $folder -File -Recurse | Sort-Object FullName)) {
            $snapshot.Add([pscustomobject]@{
                Path = $file.FullName
                Hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
            })
        }
    }
    return $snapshot.ToArray()
}

$checks = [System.Collections.Generic.List[object]]::new()
function Add-Check {
    param([string]$Name, [string[]]$Arguments, [int]$ExpectedExit = 0)
    $checks.Add([pscustomobject]@{ Name = $Name; Arguments = $Arguments; ExpectedExit = $ExpectedExit })
}

if ($Phase -eq 'Core') {
    Add-Check 'resources' @('--m1')
    Add-Check 'weapons' @('--weapon-check')
    Add-Check 'armor-data' @('--armor-check')
    Add-Check 'enemies' @('--arena-check')
    Add-Check 'pickups' @('--pickup-check')
    Add-Check 'props' @('--prop-check') 1
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
} elseif ($Phase -eq 'UI') {
    Add-Check 'media' @('--media-check')
    Add-Check 'intro' @('--intro')
    Add-Check 'movies' @('--movie-check')
    Add-Check 'movie-gallery' @('--movie-gallery')
    Add-Check 'hud' @('--hud-check')
    Add-Check 'menu' @('--game-menu-check')
    Add-Check 'horde-first' @('--horde-check', '0', '--weapon', '80')
    Add-Check 'horde-last' @('--horde-check', '9', '--weapon', '80')
} elseif ($Phase -eq 'Campaign') {
    foreach ($mission in @(10, 11, 12, 13, 14)) {
        Add-Check "campaign-pack2-$mission" @('--campaign-check', 'pack2', "$mission", '--weapon', '65')
    }
    Add-Check 'campaign-pack7-0' @('--campaign-check', 'pack7', '0', '--weapon', '65')
} elseif ($Phase -eq 'Boundary') {
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

$before = @(Get-ProtectedFiles)
$before | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $reportDirectory 'protected-before.json') -Encoding UTF8
$results = [System.Collections.Generic.List[object]]::new()
$failed = $false
$knownIssues = 0
Write-Output "[validation] $Configuration $Phase logs=$reportDirectory"
foreach ($check in $checks) {
    $arguments = @('--mute') + $check.Arguments
    $stdout = Join-Path $reportDirectory ($check.Name + '.log')
    $stderr = Join-Path $reportDirectory ($check.Name + '.err')
    Write-Output "[validation] begin $($check.Name)"
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process -FilePath $gameExe -ArgumentList $arguments -WorkingDirectory $projectRoot `
        -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru -Wait
    $timer.Stop()
    $status = 'Passed'
    if ($process.ExitCode -ne $check.ExpectedExit) { $status = 'Failed' }
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
        Name = $check.Name
        Executable = $gameExe
        Arguments = $arguments
        ExitCode = $process.ExitCode
        ExpectedExit = $check.ExpectedExit
        Seconds = [Math]::Round($timer.Elapsed.TotalSeconds, 3)
        Status = $status
        Log = $stdout
        ErrorLog = $stderr
    })
    $results.ToArray() | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $reportDirectory 'results.json') -Encoding UTF8
    Write-Output "[validation] $($check.Name) $status exit=$($process.ExitCode) seconds=$([Math]::Round($timer.Elapsed.TotalSeconds, 1))"
    if ($status -eq 'Failed') { $failed = $true; break }
}

$after = @(Get-ProtectedFiles)
$after | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $reportDirectory 'protected-after.json') -Encoding UTF8
$changed = @(Compare-Object -ReferenceObject $before -DifferenceObject $after -Property Path, Hash)
if ($changed.Count -ne 0) {
    $changed | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $reportDirectory 'protected-changes.json') -Encoding UTF8
    Write-Output '[validation] FAILED: protected save files changed'
    $failed = $true
}
Write-Output "[validation] completed=$($results.Count)/$($checks.Count) known-data-issues=$knownIssues protected-changes=$($changed.Count)"
if ($failed) { exit 1 }
exit 0
