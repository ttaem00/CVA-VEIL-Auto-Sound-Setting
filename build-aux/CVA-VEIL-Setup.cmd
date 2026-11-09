@echo off
if exist "%~dp0CVA-VEIL-Setup.exe" (
  start "" "%~dp0CVA-VEIL-Setup.exe"
  exit /b
)
start "" "%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -STA -WindowStyle Hidden -ExecutionPolicy Bypass -File "%~dp0CVA-VEIL-Setup.ps1"
exit /b
