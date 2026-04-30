@echo off
setlocal EnableDelayedExpansion

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat">nul 2>&1

set "PATH=%LOCALAPPDATA%\Packages\PythonSoftwareFoundation.Python.3.13_qbz5n2kfra8p0\LocalCache\local-packages\Python313\Scripts;C:\tools\sccache;%PATH%"
set "SCCACHE_DIR=%USERPROFILE%\.sccache"
set "SCCACHE_CACHE_SIZE=4GiB"
set CC=sccache cl
set CXX=sccache cl

if not exist src\VcsTag.h (
    set t=unknown&set r=unknown
    if exist .git for /f %%i in ('git describe --tags --always 2^>nul')do set t=%%i
    if exist .git for /f %%i in ('git rev-parse --short HEAD 2^>nul')do set r=%%i
    (
        echo #pragma once
        echo #define VCS_TAG "!t!"
        echo #define VCS_SHORT_REVISION "!r!"
        echo #define VCS_LONG_REVISION "!r!"
        echo #define VCS_WC_MODIFIED 0
        echo #define BUILD_DATE __DATE__
        echo #define BUILD_TIME __TIME__
    )>src\VcsTag.h
)

rd /s/q build-release-static 2>nul

meson setup --reconfigure ^
    -Dbuildtype=release ^
    -Dstatic=prebuilt ^
    -Db_vscrt=static_from_buildtype ^
    build-release-static && ^
cd build-release-static && ^
ninja -j%NUMBER_OF_PROCESSORS% && ^
copy/y powder.exe ..\>nul && ^
cd .. && ^
start "" powder.exe 2>nul

sccache --show-stats
timeout /t 8 >nul 