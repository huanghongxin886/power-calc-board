@echo off
rem ===========================================================================
rem  J-Link connection test for U4 (MSPM0G3507) on the "type-C power meter" board
rem  - Read only: it ONLY connects and reads a few words, it does NOT erase.
rem  - Close Keil first (Keil locks the J-Link while debug/download is active).
rem
rem  What you should see if everything is OK:
rem     S/N: xxxxxxxx                       <- probe detected
rem     Found SW-DP with ID 0x6BA02477      <- MCU detected
rem     Reading 4 bytes from address 0x00000000
rem     ... = 60 01 20 20 6D 01 00 00       <- current firmware vector table
rem
rem  If it says "Failed to initialize DAP" / "No J-Link found":
rem     -> check probe plugged in, DIO/CLK not swapped, GND connected, board powered
rem
rem  NOTE: this .bat uses the NEW J-Link V9.28 software, which no longer supports
rem        very old probes (e.g. "J-Link ARM-OB STM32" clones with 2012 firmware).
rem        With such a probe this script may report nothing / hang.
rem        In that case judge the probe from Keil: Options for Target - Debug -
rem        Settings - "SW Device" box (should show ARM CoreSight SW-DP).
rem ===========================================================================

set JLINK="C:\Program Files\SEGGER\JLink_V928\JLink.exe"
if not exist %JLINK% (
  echo Cannot find JLink.exe at %JLINK%
  echo Edit this .bat and point it to your J-Link installation.
  pause
  exit /b 1
)

%JLINK% -device MSPM0G3507 -if SWD -speed 1000 -CommanderScript "%~dp0jlink_test.jlink"
echo.
echo ---------------------------------------------------------------
echo To ERASE the chip (only if download keeps failing), run:
echo   JLink.exe -device MSPM0G3507 -if SWD -speed 1000 -autoconnect 1
echo   then type:  erase    (and  exit )
echo ---------------------------------------------------------------
pause
