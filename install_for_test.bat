@echo off
setlocal
rem Installs Step Fader for the current user (no admin rights needed).
set "DEST=%LOCALAPPDATA%\Programs\Common\VST3"
pushd "%~dp0"
if not exist "Step Fader.vst3" (
  echo [ERROR] "Step Fader.vst3" was not found next to this file.
  popd
  pause
  exit /b 1
)
if not exist "%DEST%" mkdir "%DEST%"
if exist "%DEST%\Step Fader.vst3" rmdir /s /q "%DEST%\Step Fader.vst3"
xcopy /e /i /y /q "Step Fader.vst3" "%DEST%\Step Fader.vst3" >nul
if errorlevel 1 (
  echo [ERROR] Copy failed. Close the DAW and try again.
) else (
  echo Installed to: %DEST%\Step Fader.vst3
  echo Rescan plug-ins in your DAW.
)
popd
pause
