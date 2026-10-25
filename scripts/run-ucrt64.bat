@echo off
rem A batch file lets Explorer start the game with the matching runtime DLLs.
rem Change MSYS2_ROOT here, or set it in your environment, for a custom install.
setlocal
if not defined MSYS2_ROOT set "MSYS2_ROOT=C:\msys64"
if not exist "%MSYS2_ROOT%\ucrt64\bin\g++.exe" (
    echo UCRT64 was not found at "%MSYS2_ROOT%\ucrt64\bin".
    echo Set MSYS2_ROOT to your MSYS2 install folder and try again.
    pause
    exit /b 1
)
set "PATH=%MSYS2_ROOT%\ucrt64\bin;%PATH%"
rem For a custom build, set SFML_ROOT to that same SFML installation folder.
rem Its DLLs must come before any older MSYS2 SFML DLLs on PATH.
if defined SFML_ROOT set "PATH=%SFML_ROOT%\bin;%PATH%"
rem %~dp0 is the script's folder; pushd also supports a folder with spaces.
pushd "%~dp0.." || exit /b 1
if not exist "build-ucrt64\afterlight.exe" (
    echo Build the game first in MSYS2 UCRT64: bash scripts/build-ucrt64.sh
    popd
    pause
    exit /b 1
)
"build-ucrt64\afterlight.exe" %*
set "game_result=%errorlevel%"
popd
if not "%game_result%"=="0" pause
exit /b %game_result%
