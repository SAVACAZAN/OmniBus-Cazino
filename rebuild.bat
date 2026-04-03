@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d "C:\Kits work\limaje de programare\OmniBus Cazino"
cd build
cmake --build . -j8
echo.
echo BUILD DONE
echo.
dir OmniBusCazino.exe
pause
