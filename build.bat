@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
rd /s /q .\build-release-static
meson setup -Dbuildtype=release -Dstatic=prebuilt -Db_vscrt=static_from_buildtype build-release-static
cd .\build-release-static
ninja
powder.exe
cd..
TIMEOUT /T 8
exit