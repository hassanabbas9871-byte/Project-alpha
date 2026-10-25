@echo off
rem Supply the same raylib and GCC runtime DLL path that the build used.
rem This changes PATH for this game only, not your global Windows settings.
setlocal
pushd "%~dp0.." || exit /b 1
if not exist "build-ucrt64\flux.exe" (
    echo Build FLUX first in UCRT64: bash scripts/build-ucrt64.sh
    goto :launch_error
)
set "ucrt_bin="
if exist "build-ucrt64\ucrt-bin.txt" set /p "ucrt_bin="<"build-ucrt64\ucrt-bin.txt"
rem An explicit override is useful after moving an MSYS2 installation.
if defined MSYS2_ROOT set "ucrt_bin=%MSYS2_ROOT%\ucrt64\bin"
if not defined ucrt_bin (
    echo No saved runtime path. Rerun scripts/build-ucrt64.sh in UCRT64 once.
    goto :launch_error
)
if not exist "%ucrt_bin%\g++.exe" (
    echo The saved UCRT64 installation was not found. Rebuild using its current path.
    goto :launch_error
)
set "PATH=%ucrt_bin%;%PATH%"
"build-ucrt64\flux.exe" %*
set "app_result=%errorlevel%"
popd
if not "%app_result%"=="0" pause
exit /b %app_result%

:launch_error
popd
pause
exit /b 1
