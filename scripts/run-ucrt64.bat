@echo off
rem Explorer starts without the UCRT64 terminal's PATH. Read the same DLL
rem directories that were selected by the successful build helper.
setlocal
rem %~dp0 is the script's folder; pushd also supports a folder with spaces.
pushd "%~dp0.." || exit /b 1
if not exist "build-ucrt64\afterlight.exe" (
    echo Build the game first in MSYS2 UCRT64: bash scripts/build-ucrt64.sh
    goto :launch_error
)

rem set /p reads one line as data, rather than executing a generated script.
set "sfml_bin="
set "ucrt_bin="
if exist "build-ucrt64\sfml-bin.txt" set /p "sfml_bin="<"build-ucrt64\sfml-bin.txt"
if exist "build-ucrt64\ucrt-bin.txt" set /p "ucrt_bin="<"build-ucrt64\ucrt-bin.txt"
rem Explicit environment overrides are still supported for custom installs.
if defined SFML_ROOT set "sfml_bin=%SFML_ROOT%\bin"
if defined MSYS2_ROOT set "ucrt_bin=%MSYS2_ROOT%\ucrt64\bin"
if not defined ucrt_bin set "ucrt_bin=C:\msys64\ucrt64\bin"
if not defined sfml_bin (
    echo No saved SFML runtime path. Rerun the updated build helper once.
    echo For a custom SFML install, pass its folder to the helper.
    goto :launch_error
)
if not exist "%sfml_bin%\" (
    echo The saved SFML bin folder no longer exists. Rebuild with its current path.
    goto :launch_error
)
if not exist "%ucrt_bin%\g++.exe" (
    echo The UCRT64 tools were not found. Rebuild using your current MSYS2 install.
    goto :launch_error
)
rem The selected SFML must precede any older SFML package on PATH.
set "PATH=%sfml_bin%;%ucrt_bin%;%PATH%"
"build-ucrt64\afterlight.exe" %*
set "game_result=%errorlevel%"
popd
if not "%game_result%"=="0" pause
exit /b %game_result%

:launch_error
popd
pause
exit /b 1
