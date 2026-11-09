# CVA VEIL modification, 2026-11-09. GPL-2.0; see LICENSE.
[CmdletBinding()]
param(
    [switch]$Apply,
    [string]$Destination = (Join-Path $env:PROGRAMDATA 'obs-studio\plugins\loudness-balance-monitor'),
    [string]$PackageRoot = '',
    [string]$ResultPath
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($PackageRoot)) { $PackageRoot=$PSScriptRoot }
. (Join-Path $PSScriptRoot 'CVA-VEIL-InstallCore.ps1')
$fullResult=$null
if ($ResultPath) {
    $sessionRoot = [IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'CVA-VEIL\InstallerSessions')).TrimEnd('\') + '\'
    $fullResult = [IO.Path]::GetFullPath($ResultPath)
    if (-not $fullResult.StartsWith($sessionRoot,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($fullResult) -notmatch '^[a-f0-9]{32}\.json$') { throw 'Unexpected installer result path.' }
    Assert-CvaOrdinaryPath (Split-Path -Parent $fullResult)
    [IO.Directory]::CreateDirectory((Split-Path -Parent $fullResult)) | Out-Null
}
$result = $null; $exitCode = 0
try {
    $result = Invoke-CvaInstall -Root $PackageRoot -Target $Destination -Apply:$Apply
    if (-not $ResultPath) {
        $result | Format-List
        if (-not $Apply) { Write-Output '미리보기입니다. 파일은 바꾸지 않았습니다. 설치하려면 OBS를 완전히 종료한 뒤 -Apply를 지정하세요.' }
    }
} catch {
    $exitCode = 1
    $failureCode=Get-CvaInstallErrorCode $_.Exception
    $result = [pscustomobject]@{
        Status='Failed'; Code=$failureCode; Detail=$_.Exception.ToString()
        Changed=$(if($failureCode -eq 'RollbackFailed'){'Unknown'}else{$false})
        Rollback=[string]$_.Exception.Data['CvaRollback']; BackupPath=[string]$_.Exception.Data['CvaBackupPath']
    }
    if (-not $ResultPath) { throw }
}
if ($ResultPath) {
    $result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $fullResult -Encoding UTF8
    exit $exitCode
}
