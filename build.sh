if [ ! -d "./build/" ]; then
    rm -rf build
fi
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE="../../vcpkg/scripts/buildsystems/vcpkg.cmake"
make -B
export LC_ALL=C
unset LANGUAGE
./messaging_system/unittest/unittest
#make install