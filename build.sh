rm -rf ./build-release-static
meson setup -Dbuildtype=release -Dstatic=prebuilt build-release-static
cd ./build-release-static
meson compile
./powder
cd ..