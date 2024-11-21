cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE="Release" -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH};/usr/lib/cmake/" -DINCLUDE_DIRECTORIES="${INCLUDE_DIRECTORIES};/usr/include/qt6/"
cmake --build build/
