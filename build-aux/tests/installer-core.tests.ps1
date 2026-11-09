# CVA VEIL isolated installer verification. Does not touch an installed OBS.
[CmdletBinding()]
param([string]$WorkRoot='')
$ErrorActionPreference='Stop'
if ([string]::IsNullOrWhiteSpace($WorkRoot)) { $WorkRoot=Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'work\installer-tests' }
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'CVA-VEIL-InstallCore.ps1')
$testRoot=Join-Path ([IO.Path]::GetFullPath($WorkRoot)) ([Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($testRoot) | Out-Null
$package=Join-Path $testRoot 'package'
$plugin=Join-Path $package 'loudness-balance-monitor'
$backups=Join-Path $testRoot 'backups'
$names=@('loudness-balance-monitor.dll','data/locale/en-US.ini','data/locale/ja-JP.ini','data/locale/ko-KR.ini','data/i18n.json')
$entries=@();$checks=@()
foreach($name in $names){
 $file=Join-Path $plugin $name
 [IO.Directory]::CreateDirectory((Split-Path -Parent $file)) | Out-Null
 [IO.File]::WriteAllText($file,'new-'+$name)
 $entries+=@{path='loudness-balance-monitor/'+$name;bytes=(Get-Item -LiteralPath $file).Length;sha256=(Get-CvaFileHash $file)}
}
@{product='CVA-VEIL-Auto-Sound-Setting';version='1.3.0';files=$entries} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $package 'package-manifest.json') -Encoding UTF8
function Assert-CvaTest([bool]$Condition,[string]$Name){if(-not $Condition){throw ('FAIL: '+$Name)};$script:checks+=@{name=$Name;status='PASS'}}
function Expect-CvaFailure([scriptblock]$Action,[string]$Code){
 $caught=$false
 try { & $Action | Out-Null } catch { $caught=$true;if((Get-CvaInstallErrorCode $_.Exception) -ne $Code){throw} }
 if(-not $caught){throw ('Expected failure: '+$Code)}
}
$target=Join-Path $testRoot 'preview\loudness-balance-monitor'
$preview=Invoke-CvaInstall -Root $package -Target $target -ObsRunningProbe {$false}
Assert-CvaTest ($preview.Status -eq 'Preview' -and -not(Test-Path -LiteralPath $target)) 'Preview changes no files'
$runningPreview=Invoke-CvaInstall -Root $package -Target $target -ObsRunningProbe {$true}
Assert-CvaTest $runningPreview.ObsRunning 'Preview reports open OBS'
Expect-CvaFailure {Invoke-CvaInstall -Root $package -Target $target -Apply -ObsRunningProbe {$true} -BackupDirectory $backups} 'ObsRunning'
Assert-CvaTest (-not(Test-Path -LiteralPath $target)) 'Open OBS blocks installation without touching target'
$firstFile=Join-Path $plugin $names[0];$original=[IO.File]::ReadAllBytes($firstFile)
[IO.File]::AppendAllText($firstFile,'tampered')
Expect-CvaFailure {Invoke-CvaInstall -Root $package -Target $target -Apply -ObsRunningProbe {$false} -BackupDirectory $backups} 'PackageChanged'
Assert-CvaTest (-not(Test-Path -LiteralPath $target)) 'Checksum failure changes no target files'
[IO.File]::WriteAllBytes($firstFile,$original)
$freshTarget=Join-Path $testRoot 'fresh\loudness-balance-monitor'
$installed=Invoke-CvaInstall -Root $package -Target $freshTarget -Apply -ObsRunningProbe {$false} -BackupDirectory $backups
Assert-CvaTest ($installed.Status -eq 'Installed' -and $installed.Verified) 'Fresh installation returns verified completion'
Assert-CvaTest (@($names | Where-Object {(Get-CvaFileHash (Join-Path $freshTarget $_)) -ne (Get-CvaFileHash (Join-Path $plugin $_))}).Count -eq 0) 'All five installed files match package'
$oldTarget=Join-Path $testRoot 'rollback\loudness-balance-monitor'
foreach($name in $names){$file=Join-Path $oldTarget $name;[IO.Directory]::CreateDirectory((Split-Path -Parent $file))|Out-Null;[IO.File]::WriteAllText($file,'previous-'+$name)}
$before=@{};foreach($name in $names){$before[$name]=Get-CvaFileHash (Join-Path $oldTarget $name)}
[IO.File]::WriteAllText((Join-Path $oldTarget 'unmanaged.txt'),'keep this file')
Expect-CvaFailure {Invoke-CvaInstall -Root $package -Target $oldTarget -Apply -ObsRunningProbe {$false} -BackupDirectory $backups -BeforeReplace {param($index,$relative) if($index -eq 2){throw (New-Object UnauthorizedAccessException('Simulated third-file write failure.'))}}} 'Permission'
Assert-CvaTest (@($names | Where-Object {(Get-CvaFileHash (Join-Path $oldTarget $_)) -ne $before[$_]}).Count -eq 0) 'Mid-install write failure restores every previous file'
Assert-CvaTest ([IO.File]::ReadAllText((Join-Path $oldTarget 'unmanaged.txt')) -eq 'keep this file') 'Unmanaged files are preserved'
$partialTarget=Join-Path $testRoot 'partial\loudness-balance-monitor'
[IO.Directory]::CreateDirectory($partialTarget)|Out-Null
[IO.File]::WriteAllText((Join-Path $partialTarget $names[0]),'previous DLL')
Expect-CvaFailure {Invoke-CvaInstall -Root $package -Target $partialTarget -Apply -ObsRunningProbe {$false} -BackupDirectory $backups -BeforeReplace {param($index,$relative) if($index -eq 2){Throw-CvaInstallError 'ReadbackFailed' 'Simulated failure after introducing a new file.'}}} 'ReadbackFailed'
Assert-CvaTest ([IO.File]::ReadAllText((Join-Path $partialTarget $names[0])) -eq 'previous DLL' -and -not(Test-Path -LiteralPath (Join-Path $partialTarget $names[1]))) 'Rollback removes newly introduced files and restores existing file'
Assert-CvaTest (@(Get-ChildItem -LiteralPath $testRoot -Directory -Recurse -Filter '.cva-stage-*').Count -eq 0) 'Temporary installation stages are cleaned'
$report=[pscustomobject]@{schema='cva-installer-tests.v1';status='PASS';checks=$checks;synthetic_package_only=$true;production_obs_changed=$false;test_folder=$testRoot}
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $testRoot 'result.json') -Encoding UTF8
$report | ConvertTo-Json -Depth 5
