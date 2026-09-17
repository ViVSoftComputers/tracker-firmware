@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM Seeed SenseCAP T1000-E Cached Phone Tracker Controller
REM ============================================================================

set "SCRIPT_DIR=%~dp0"
set "TOOL_PY=%SCRIPT_DIR%tracker_tool.py"

if not exist "%TOOL_PY%" (
    echo [ERROR] tracker_tool.py not found at "%TOOL_PY%"
    exit /b 1
)

REM If arguments were passed directly from command line (e.g. tracker.bat status)
if not "%~1"=="" (
    python "%TOOL_PY%" %*
    exit /b %ERRORLEVEL%
)

:MENU
cls
echo ============================================================================
echo         SEEED SENSECAP T1000-E CACHED TRACKER CONTROLLER
echo ============================================================================
echo.
echo   [1] Status       - Check tracking mode, cache count, GPS lock, battery
echo   [2] Tracker ON   - Enable 60s background logging (Solid LED, Chime)
echo   [3] Tracker OFF  - Disable background logging (LED off, Descending chime)
echo   [4] Sync to Phone- Replay cached positions as Meshtastic Position packets
echo   [5] Dump to GPX  - Export all cached points to tracklog.gpx file
echo   [6] Clear Cache  - Wipe flash ring buffer (/tracker_points.dat)
echo   [7] Hardware Test- Beep buzzer and blink LED
echo.
echo   [8] Flash T1000-E- Copy firmware-tracker-t1000e-v3.0.2.uf2 to drive
echo   [9] Device Info  - Run meshtastic --info
echo.
echo   [0] Exit
echo.
echo ============================================================================
set /p "CHOICE=Select an option [0-9]: "

if "%CHOICE%"=="1" goto CMD_STATUS
if "%CHOICE%"=="2" goto CMD_ON
if "%CHOICE%"=="3" goto CMD_OFF
if "%CHOICE%"=="4" goto CMD_SYNC
if "%CHOICE%"=="5" goto CMD_DUMP
if "%CHOICE%"=="6" goto CMD_CLEAR
if "%CHOICE%"=="7" goto CMD_TEST
if "%CHOICE%"=="8" goto CMD_FLASH
if "%CHOICE%"=="9" goto CMD_INFO
if "%CHOICE%"=="0" goto EXIT

echo.
echo [!] Invalid selection. Try again.
timeout /t 2 >nul
goto MENU

:CMD_STATUS
echo.
echo [*] Querying tracker status...
python "%TOOL_PY%" status
goto PAUSE_MENU

:CMD_ON
echo.
echo [*] Enabling Tracker Mode...
python "%TOOL_PY%" on
goto PAUSE_MENU

:CMD_OFF
echo.
echo [*] Disabling Tracker Mode...
python "%TOOL_PY%" off
goto PAUSE_MENU

:CMD_SYNC
echo.
echo [*] Replaying cached positions to phone queue...
python "%TOOL_PY%" sync
goto PAUSE_MENU

:CMD_DUMP
echo.
set /p "OUTFILE=Enter output GPX filename [default: tracklog.gpx]: "
if "%OUTFILE%"=="" set "OUTFILE=tracklog.gpx"
echo [*] Dumping cached positions to %OUTFILE%...
python "%TOOL_PY%" dump -o "%OUTFILE%"
goto PAUSE_MENU

:CMD_CLEAR
echo.
echo [!] WARNING: This will erase all cached GPS points on the T1000-E.
set /p "CONFIRM=Are you sure? (Y/N): "
if /i not "%CONFIRM%"=="Y" (
    echo [*] Operation canceled.
    goto PAUSE_MENU
)
echo [*] Clearing cache...
python "%TOOL_PY%" clear
goto PAUSE_MENU

:CMD_TEST
echo.
echo [*] Triggering T1000-E hardware test (buzzer and LED)...
python "%TOOL_PY%" test
goto PAUSE_MENU

:CMD_FLASH
echo.
echo ============================================================================
echo FLASH FIRMWARE (T1000-E)
echo ============================================================================
echo Put your T1000-E into UF2 bootloader mode:
echo   - Plug device into USB
echo   - Double-click the button quickly (or hold while plugging in)
echo   - A removable USB drive will appear (e.g. D:, E:, F:)
echo.
set /p "DRIVE=Enter drive letter for T1000-E bootloader (e.g. E): "
if "%DRIVE%"=="" (
    echo [!] No drive specified. Aborting flash.
    goto PAUSE_MENU
)
set "DRIVE=!DRIVE:~0,1!"
set "UF2_SRC=%SCRIPT_DIR%artifacts\firmware-tracker-t1000e-v3.0.2.uf2"

if not exist "%UF2_SRC%" (
    echo [ERROR] UF2 not found at "%UF2_SRC%"
    goto PAUSE_MENU
)

if not exist "%DRIVE%:\" (
    echo [ERROR] Drive %DRIVE%:\ not detected. Make sure the bootloader is active.
    goto PAUSE_MENU
)

echo [*] Copying %UF2_SRC% to %DRIVE%:\...
copy /y "%UF2_SRC%" "%DRIVE%:\"
if %ERRORLEVEL% EQU 0 (
    echo [+] Flashed successfully! Device will reboot automatically.
) else (
    echo [-] Failed to copy UF2 file.
)
goto PAUSE_MENU

:CMD_INFO
echo.
echo [*] Querying Meshtastic device info...
python -m meshtastic --info
goto PAUSE_MENU

:PAUSE_MENU
echo.
pause
goto MENU

:EXIT
echo.
echo Bye!
exit /b 0
