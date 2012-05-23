@echo off

:: This builds using MSVC VS90 (.NET 2.0)

set FLAGS=/nologo /MDd /D _DEBUG /Zi /W4 /wd4201 /wd4480 /O2 /GS /GL /EHa /D _UNICODE /D UNICODE
set FILES=PEFile.cpp PEFileResources.cpp PEDataSource.cpp PEVersion.cpp

echo Compiling 32-bit...
call "%VS90COMNTOOLS%\..\..\VC\vcvarsall.bat" x86
cl %FLAGS% /FdPEFile_d.pdb /c %FILES%
lib /nologo /ltcg /out:PEFile_d.lib *.obj
del /F /Q *.obj >NUL 2>&1
echo.

echo Compiling 64-bit...
call "%VS90COMNTOOLS%\..\..\VC\vcvarsall.bat" x64
cl %FLAGS% /FdPEFile64_d.pdb /c %FILES%
lib /nologo /ltcg /out:PEFile64_d.lib *.obj
del /F /Q *.obj >NUL 2>&1
pause
