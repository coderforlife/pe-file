@echo off

:: This builds using MSVC VS100 (.NET 4.0)

set FLAGS=/nologo /MD /D NDEBUG /W4 /wd4201 /wd4480 /O2 /GS /GL /EHa /D _UNICODE /D UNICODE
set FILES=PEFile.cpp PEFileResources.cpp PEDataSource.cpp PEVersion.cpp

echo Compiling 32-bit...
call "%VS100COMNTOOLS%\..\..\VC\vcvarsall.bat" x86
cl %FLAGS% /c %FILES%
lib /nologo /ltcg /out:PEFile.lib *.obj
del /F /Q *.obj >NUL 2>&1
echo.

echo Compiling 64-bit...
call "%VS100COMNTOOLS%\..\..\VC\vcvarsall.bat" x64
cl %FLAGS% /c %FILES%
lib /nologo /ltcg /out:PEFile64.lib *.obj
del /F /Q *.obj >NUL 2>&1
pause
