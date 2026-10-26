@echo off
rem Supply the same raylib and GCC runtime DLLs used for the successful build.
rem setlocal prevents this PATH change from changing Windows permanently.
setlocal
pushd "%~dp0.." || exit /b 1
if not exist "build-ucrt64\ludo.exe" (
    echo Build first in UCRT64: bash scripts/build-ucrt64.sh
    goto :launch_error
)
set "ucrt_bin="
if exist "build-ucrt64\ucrt-bin.txt" set /p "ucrt_bin="<"build-ucrt64\ucrt-bin.txt"
rem This optional override supports an MSYS2 installation moved after building.
if defined MSYS2_ROOT set "ucrt_bin=%MSYS2_ROOT%\ucrt64\bin"
if not defined ucrt_bin (
    echo No saved runtime path. Rerun the build helper in UCRT64 once.
    goto :launch_error
)
if not exist "%ucrt_bin%\g++.exe" (
    echo The saved UCRT64 installation was not found. Rebuild using its current path.
    goto :launch_error
)
set "PATH=%ucrt_bin%;%PATH%"
"build-ucrt64\ludo.exe" %*
set "game_result=%errorlevel%"
popd
if not "%game_result%"=="0" pause
exit /b %game_result%

:launch_error
popd
pause
exit /b 1
