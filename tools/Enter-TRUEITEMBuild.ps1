# TRUEITEM local build environment. Dot-source this file in the build process.
# No machine/user environment variables, registry values, or installations are changed.
$trueitemBuildRoot = $PSScriptRoot
$trueitemMsvcRoot = Join-Path $trueitemBuildRoot 'portable-msvc\VC\Tools\MSVC\14.51.36231'
$trueitemSdkRoot = Join-Path $trueitemBuildRoot 'sdk\c'
$trueitemSdkVersion = '10.0.26100.0'
$trueitemCompilerBin = Join-Path $trueitemMsvcRoot 'bin\Hostx64\x64'
$trueitemSdkBin = Join-Path $trueitemSdkRoot ('bin\' + $trueitemSdkVersion + '\x64')
$trueitemCmakeBin = Join-Path $trueitemBuildRoot 'python\cmake\data\bin'
$trueitemNinjaBin = Join-Path $trueitemBuildRoot 'python\bin'
$env:PATH = (@($trueitemCompilerBin, $trueitemSdkBin, $trueitemCmakeBin, $trueitemNinjaBin, $env:PATH) -join ';')
$env:INCLUDE = (@((Join-Path $trueitemMsvcRoot 'include'), (Join-Path $trueitemSdkRoot ('Include\' + $trueitemSdkVersion + '\ucrt')), (Join-Path $trueitemSdkRoot ('Include\' + $trueitemSdkVersion + '\shared')), (Join-Path $trueitemSdkRoot ('Include\' + $trueitemSdkVersion + '\um')), (Join-Path $trueitemSdkRoot ('Include\' + $trueitemSdkVersion + '\winrt'))) -join ';')
$env:LIB = (@((Join-Path $trueitemMsvcRoot 'lib\x64'), (Join-Path $trueitemMsvcRoot 'lib\onecore\x64'), (Join-Path $trueitemSdkRoot 'ucrt\x64'), (Join-Path $trueitemSdkRoot 'um\x64')) -join ';')
$env:LIBPATH = $env:LIB
$env:VCToolsInstallDir = $trueitemMsvcRoot + '\'
$env:WindowsSdkDir = $trueitemSdkRoot + '\'
$env:WindowsSDKVersion = $trueitemSdkVersion + '\'
$env:VSCMD_ARG_HOST_ARCH = 'x64'
$env:VSCMD_ARG_TGT_ARCH = 'x64'
$env:CC = 'cl.exe'
$env:CXX = 'cl.exe'
Write-Output 'TRUEITEM portable C++ build environment is ready for this process.'
