cmake -B build -S . ^
    -G "Ninja" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX="%LIBRARY_PREFIX%" ^
    -DBZIP2_INCLUDE_DIR="%LIBRARY_INC%" ^
    -DBZIP2_LIBRARIES="%LIBRARY_LIB%\bz2.lib"
if errorlevel 1 exit 1

cmake --build build
if errorlevel 1 exit 1

cmake --install build
if errorlevel 1 exit 1
