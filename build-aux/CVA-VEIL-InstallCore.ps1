# CVA VEIL modification, 2026-11-09. GPL-2.0; see LICENSE.
# Shared by the command-line preview and the graphical installer.
$ErrorActionPreference = 'Stop'

function Get-CvaFileHash([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose(); $stream.Dispose() }
}
function Throw-CvaInstallError([string]$Code, [string]$Detail) {
    $errorObject = New-Object InvalidOperationException($Detail)
    $errorObject.Data['CvaCode'] = $Code
    throw $errorObject
}
function Get-CvaInstallErrorCode($ErrorObject) {
    $current = $ErrorObject
    while ($current) {
        if ($current.Data -and $current.Data['CvaCode']) { return [string]$current.Data['CvaCode'] }
        if ($current -is [UnauthorizedAccessException]) { return 'Permission' }
        $current = $current.InnerException
    }
    return 'InstallFailed'
}
function Test-CvaObsRunning { return [bool](Get-Process -Name obs64,obs32 -ErrorAction SilentlyContinue) }
function Assert-CvaOrdinaryPath([string]$Path) {
    $itemPath = [IO.Path]::GetFullPath($Path)
    while ($itemPath) {
        if (Test-Path -LiteralPath $itemPath) {
            $item = Get-Item -LiteralPath $itemPath -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                Throw-CvaInstallError 'UnsafePath' 'An installation path contains a link or redirected folder.'
            }
        }
        $parentPath = Split-Path -Parent $itemPath
        if ($parentPath -eq $itemPath) { break }
        $itemPath = $parentPath
    }
}
function Get-CvaPackageFiles([string]$Root) {
    $rootPath = [IO.Path]::GetFullPath($Root)
    $manifestFile = Join-Path $rootPath 'package-manifest.json'
    if (-not (Test-Path -LiteralPath $manifestFile -PathType Leaf)) { Throw-CvaInstallError 'PackageMissing' 'The package manifest is missing.' }
    try { $manifest = Get-Content -LiteralPath $manifestFile -Raw -Encoding UTF8 | ConvertFrom-Json }
    catch { Throw-CvaInstallError 'PackageInvalid' 'The package manifest cannot be read.' }
    if ($manifest.product -ne 'CVA-VEIL-Auto-Sound-Setting' -or $manifest.version -ne '1.3.0') {
        Throw-CvaInstallError 'PackageInvalid' 'This installer requires the matching CVA VEIL 1.3.0 package.'
    }
    $relativeFiles = @('loudness-balance-monitor.dll','data/locale/en-US.ini','data/locale/ja-JP.ini','data/locale/ko-KR.ini','data/i18n.json')
    $files = @()
    foreach ($relative in $relativeFiles) {
        $entries = @($manifest.files | Where-Object { $_.path -ceq ('loudness-balance-monitor/' + $relative) })
        if ($entries.Count -ne 1 -or [string]$entries[0].sha256 -notmatch '^[a-fA-F0-9]{64}$') { Throw-CvaInstallError 'PackageInvalid' ('Unexpected manifest entry: ' + $relative) }
        $sourcePath = Join-Path (Join-Path $rootPath 'loudness-balance-monitor') $relative
        Assert-CvaOrdinaryPath $sourcePath
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) { Throw-CvaInstallError 'PackageMissing' ('A package file is missing: ' + $relative) }
        $item = Get-Item -LiteralPath $sourcePath
        if ($item.Length -ne $entries[0].bytes -or (Get-CvaFileHash $sourcePath) -ne $entries[0].sha256) { Throw-CvaInstallError 'PackageChanged' ('A package file did not match its checksum: ' + $relative) }
        $files += [pscustomobject]@{ Relative=$relative; Source=$sourcePath; Bytes=$item.Length; Hash=[string]$entries[0].sha256 }
    }
    return $files
}
function Set-CvaAtomicFile([string]$Prepared, [string]$Target) {
    if (Test-Path -LiteralPath $Target -PathType Leaf) { [IO.File]::Replace($Prepared,$Target,[System.Management.Automation.Language.NullString]::Value) }
    else { [IO.File]::Move($Prepared,$Target) }
}
function Invoke-CvaInstall {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Root,
        [Parameter(Mandatory)][string]$Target,
        [switch]$Apply,
        # Test dependencies are injected only by isolated tests, never by the public entry.
        [scriptblock]$ObsRunningProbe = { Test-CvaObsRunning },
        [scriptblock]$BeforeReplace,
        [string]$BackupDirectory=(Join-Path $env:LOCALAPPDATA 'CVA-VEIL\InstallerBackups')
    )
    $rootPath = [IO.Path]::GetFullPath($Root)
    $targetPath = [IO.Path]::GetFullPath($Target).TrimEnd([IO.Path]::DirectorySeparatorChar)
    if ([IO.Path]::GetFileName($targetPath) -ne 'loudness-balance-monitor') { Throw-CvaInstallError 'UnsafePath' 'Choose the loudness-balance-monitor plugin folder.' }
    Assert-CvaOrdinaryPath $targetPath
    $files = @(Get-CvaPackageFiles $rootPath)
    $running = [bool](& $ObsRunningProbe)
    if (-not $Apply) { return [pscustomobject]@{ Status='Preview'; Version='1.3.0'; Destination=$targetPath; Files=$files.Count; ObsRunning=$running; Changed=$false } }
    if ($running) { Throw-CvaInstallError 'ObsRunning' 'OBS is open. Close it normally, then try again.' }
    $mutex = New-Object Threading.Mutex($false, 'Local\CVA-VEIL-PluginInstall')
    $locked = $false; $stagePath = $null; $rollbackPath = $null
    $changed = New-Object Collections.Generic.List[string]
    try {
        $locked = $mutex.WaitOne(0)
        if (-not $locked) { Throw-CvaInstallError 'Busy' 'Another CVA VEIL installation is already running.' }
        $parentPath = Split-Path -Parent $targetPath
        try {
            [IO.Directory]::CreateDirectory($parentPath) | Out-Null
            Assert-CvaOrdinaryPath $parentPath
            $stagePath = Join-Path $parentPath ('.cva-stage-' + [Guid]::NewGuid().ToString('N'))
            [IO.Directory]::CreateDirectory($stagePath) | Out-Null
        } catch [UnauthorizedAccessException] { Throw-CvaInstallError 'Permission' 'Windows permission is needed to write to the plugin folder.' }
        $backupBase = [IO.Path]::GetFullPath($BackupDirectory)
        Assert-CvaOrdinaryPath $backupBase
        $rollbackPath = Join-Path $backupBase ((Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N'))
        [IO.Directory]::CreateDirectory($rollbackPath) | Out-Null
        $before = @()
        foreach ($file in $files) {
            $staged = Join-Path $stagePath $file.Relative
            [IO.Directory]::CreateDirectory((Split-Path -Parent $staged)) | Out-Null
            [IO.File]::Copy($file.Source,$staged,$false)
            if ((Get-CvaFileHash $staged) -ne $file.Hash) { Throw-CvaInstallError 'PackageChanged' 'A prepared file failed verification.' }
            $existing = Join-Path $targetPath $file.Relative
            Assert-CvaOrdinaryPath $existing
            $exists = Test-Path -LiteralPath $existing -PathType Leaf
            $entry = [ordered]@{ Relative=$file.Relative; Existed=[bool]$exists; Hash=$null }
            if ($exists) {
                $backupFile = Join-Path $rollbackPath $file.Relative
                [IO.Directory]::CreateDirectory((Split-Path -Parent $backupFile)) | Out-Null
                [IO.File]::Copy($existing,$backupFile,$false)
                $entry.Hash = Get-CvaFileHash $existing
                if ((Get-CvaFileHash $backupFile) -ne $entry.Hash) { Throw-CvaInstallError 'BackupFailed' 'The previous plugin could not be backed up.' }
            }
            $before += [pscustomobject]$entry
        }
        $before | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $rollbackPath 'previous-files.json') -Encoding UTF8
        if ([bool](& $ObsRunningProbe)) { Throw-CvaInstallError 'ObsRunning' 'OBS was opened before installation.' }
        [IO.Directory]::CreateDirectory($targetPath) | Out-Null
        $replaceIndex = 0
        try {
            foreach ($file in $files) {
                if ([bool](& $ObsRunningProbe)) { Throw-CvaInstallError 'ObsRunning' 'OBS was opened during installation.' }
                if ($BeforeReplace) { & $BeforeReplace $replaceIndex $file.Relative }
                $targetFile = Join-Path $targetPath $file.Relative
                [IO.Directory]::CreateDirectory((Split-Path -Parent $targetFile)) | Out-Null
                Set-CvaAtomicFile (Join-Path $stagePath $file.Relative) $targetFile
                $changed.Add($file.Relative)
                if ((Get-CvaFileHash $targetFile) -ne $file.Hash) { Throw-CvaInstallError 'ReadbackFailed' 'An installed file failed verification.' }
                $replaceIndex++
            }
            $receipt = [pscustomobject]@{ Status='Installed'; Version='1.3.0'; Destination=$targetPath; Files=$files.Count; BackupPath=$rollbackPath; Changed=$true; Verified=$true }
            $receipt | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $rollbackPath 'installation.json') -Encoding UTF8
        } catch {
            $originalError = $_.Exception
            $rollbackErrors = @()
            for ($index=$changed.Count-1; $index -ge 0; $index--) {
                $relative = $changed[$index]
                try {
                    $entry = $before | Where-Object Relative -eq $relative
                    $targetFile = Join-Path $targetPath $relative
                    if ($entry.Existed) {
                        $preparedRestore = Join-Path (Split-Path -Parent $targetFile) ('.cva-restore-' + [Guid]::NewGuid().ToString('N'))
                        [IO.File]::Copy((Join-Path $rollbackPath $relative),$preparedRestore,$false)
                        Set-CvaAtomicFile $preparedRestore $targetFile
                        if ((Get-CvaFileHash $targetFile) -ne $entry.Hash) { throw 'Restored checksum mismatch.' }
                    } else {
                        if (-not $targetFile.StartsWith($targetPath + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Restore path escaped target.' }
                        Remove-Item -LiteralPath $targetFile -Force
                    }
                } catch { $rollbackErrors += ($relative + ': ' + $_.Exception.Message) }
            }
            if ($rollbackErrors.Count) { Throw-CvaInstallError 'RollbackFailed' ('Restore the previous files from ' + $rollbackPath + '. ' + ($rollbackErrors -join '; ')) }
            $wrapped = New-Object InvalidOperationException('Installation stopped. Previous plugin files were restored.', $originalError)
            $wrapped.Data['CvaCode'] = Get-CvaInstallErrorCode $originalError
            $wrapped.Data['CvaRollback'] = 'Restored'
            $wrapped.Data['CvaBackupPath'] = $rollbackPath
            throw $wrapped
        }
        return $receipt
    } finally {
        if ($stagePath -and (Test-Path -LiteralPath $stagePath)) {
            $resolvedStage = [IO.Path]::GetFullPath($stagePath)
            $expectedParent = [IO.Path]::GetFullPath((Split-Path -Parent $targetPath))
            if ((Split-Path -Parent $resolvedStage) -eq $expectedParent -and [IO.Path]::GetFileName($resolvedStage) -match '^\.cva-stage-[a-f0-9]{32}$') {
                Remove-Item -LiteralPath $resolvedStage -Recurse -Force -ErrorAction SilentlyContinue
            }
        }
        if ($locked) { $mutex.ReleaseMutex() }
        $mutex.Dispose()
    }
}
