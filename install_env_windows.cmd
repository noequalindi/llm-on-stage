@echo off
setlocal
rem ExecutionPolicy applies only to this process, not to the machine.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\install_env_windows.ps1"
set "kit_result=%ERRORLEVEL%"
echo.
if not "%kit_result%"=="0" echo La preparacion fallo. Revisar el error anterior y volver a ejecutar.
pause
exit /b %kit_result%
