# auto_publish.ps1 - automatic publish of project changes to git repo.
# Watches project files and on change runs commit + push to origin/main.
# Usage:
#   -Once                     - run one publish cycle and exit
#   powershell -File ...      - background watcher (for autostart at logon)

param([switch]$Once)

$ErrorActionPreference = 'Continue'
$root = Split-Path -Parent $PSScriptRoot

# log outside the repo so it never pollutes `git add -A`
$logDir = Join-Path $env:LOCALAPPDATA 'SuperLauncherAutoPublish'
$log = Join-Path $logDir 'auto_publish.log'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

function Write-Log($msg) {
    $line = "{0}  {1}" -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $msg
    Add-Content -Path $log -Value $line -Encoding UTF8
}

# non-interactive git (background run)
$env:GIT_TERMINAL_PROMPT = '0'
$env:GCM_INTERACTIVE = 'Never'

function Invoke-Publish {
    Push-Location $root
    try {
        git add -A 2>&1 | ForEach-Object { Write-Log "add: $_" }
        $staged = @(git status --porcelain)
        if ($staged.Count -gt 0) {
            $msg = "auto: sync ($($staged.Count) file(s))"
            git commit -m $msg 2>&1 | ForEach-Object { Write-Log "commit: $_" }
            Write-Log "Committed: $msg"
        }
        git push origin main 2>&1 | ForEach-Object { Write-Log "push: $_" }
    } catch {
        Write-Log "Error: $_"
    } finally {
        Pop-Location
    }
}

if ($Once) {
    Invoke-Publish
    exit 0
}

# single instance guard
$mutex = New-Object System.Threading.Mutex($false, 'Global\SuperLauncherAutoPublish')
if (-not $mutex.WaitOne(0)) {
    Write-Log 'Another instance is running, exiting'
    exit 0
}

$watcher = New-Object System.IO.FileSystemWatcher
$watcher.Path = $root
$watcher.IncludeSubdirectories = $true
$watcher.NotifyFilter = [System.IO.NotifyFilters]::LastWrite -bor `
                        [System.IO.NotifyFilters]::FileName -bor `
                        [System.IO.NotifyFilters]::DirectoryName -bor `
                        [System.IO.NotifyFilters]::Size

$script:Dirty = $false
$script:LastChange = [DateTime]::MinValue

$filter = {
    $fp = $EventArgs.FullPath
    # ignore the git repo itself
    if ($fp -like '*\.git\*') { return }
    $script:Dirty = $true
    $script:LastChange = [DateTime]::Now
}

$evCreated = Register-ObjectEvent -InputObject $watcher -EventName 'Created' -Action $filter
$evChanged = Register-ObjectEvent -InputObject $watcher -EventName 'Changed' -Action $filter
$evRenamed = Register-ObjectEvent -InputObject $watcher -EventName 'Renamed' -Action $filter
$evDeleted = Register-ObjectEvent -InputObject $watcher -EventName 'Deleted' -Action $filter
$watcher.EnableRaisingEvents = $true

Write-Log "Watcher started: $root"
$lastRetry = [DateTime]::MinValue

while ($true) {
    Start-Sleep -Seconds 1
    $now = [DateTime]::Now

    # 3s quiet period after the last change (avoid spamming during builds)
    if ($script:Dirty -and ($now - $script:LastChange).TotalSeconds -ge 3) {
        $script:Dirty = $false
        Write-Log 'Changes detected, publishing...'
        Invoke-Publish
        continue
    }

    # safety net: every 5 minutes push any unpushed commits
    if (($now - $lastRetry).TotalMinutes -ge 5) {
        $lastRetry = $now
        $ahead = 0
        Push-Location $root
        try { $ahead = [int](git rev-list --count 'HEAD@{upstream}'..HEAD 2>$null) } catch { $ahead = 0 }
        Pop-Location
        if ($ahead -gt 0) { Invoke-Publish }
    }
}
