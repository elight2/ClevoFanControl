cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE="Release" -DCMAKE_CXX_COMPILER="D:\\\\Applications\\\\Qt\\\\Tools\\\\mingw1310_64\\\\bin\\\\g++.exe" -DINCLUDE_DIRECTORIES="${INCLUDE_DIRECTORIES};D:\\\\Applications\\\\Qt\\\\6.8.0\\\\mingw_64\\\\include" -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH};D:\\\\Applications\\\\Qt\\\\6.8.0\\\\mingw_64\\\\lib\\\\cmake"
cmake --build build
