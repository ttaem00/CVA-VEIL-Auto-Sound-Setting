# Build the console-free Windows installer host using the installed .NET Framework.
[CmdletBinding()]
param([string]$OutputDirectory='')
$ErrorActionPreference='Stop'
if([string]::IsNullOrWhiteSpace($OutputDirectory)){$OutputDirectory=Join-Path (Split-Path -Parent $PSScriptRoot) 'work\installer-host'}
$compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$windowsPowerShell=Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe'
$automation=[string](& $windowsPowerShell -NoLogo -NoProfile -Command '[System.Management.Automation.PSObject].Assembly.Location')
$automation=$automation.Trim()
if(-not(Test-Path -LiteralPath $compiler -PathType Leaf) -or -not(Test-Path -LiteralPath $automation -PathType Leaf)){throw 'The Windows .NET Framework compiler and Windows PowerShell 5.1 are required.'}
$destination=[IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($destination) | Out-Null
$executable=Join-Path $destination 'CVA-VEIL-Setup.exe'
& $compiler /nologo /target:winexe /platform:x64 /optimize+ /codepage:65001 ('/out:'+$executable) ('/reference:'+$automation) /reference:System.Windows.Forms.dll (Join-Path $PSScriptRoot 'SetupHost.cs')
if($LASTEXITCODE -ne 0){throw 'Installer host compilation failed.'}
[pscustomobject]@{Status='Built';Executable=$executable;Runtime='Windows .NET Framework + Windows PowerShell 5.1';Installed=$false} | Format-List
