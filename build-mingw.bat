@echo off

:: This builds using MinGW-w64 for 32 and 64 bit (http://mingw-w64.sourceforge.net/)
:: Make sure both mingw-w32\bin and mingw-w64\bin are in the PATH

:: -s
set FLAGS=-Wall -Wno-unknown-pragmas -static-libgcc -static-libstdc++ -O3 -D UNICODE -D _UNICODE
set FILES=PEFile.cpp PEFileResources.cpp

echo Compiling 32-bit...
i686-w64-mingw32-g++ %FLAGS% -c %FILES% -lVersion
i686-w64-mingw32-ar rcs libPEFile.a *.o
del /F /Q *.o >NUL 2>&1
echo.

echo Compiling 64-bit...
x86_64-w64-mingw32-g++ %FLAGS% -c %FILES% -lVersion
x86_64-w64-mingw32-ar rcs libPEFile64.a *.o
del /F /Q *.o >NUL 2>&1
pause
