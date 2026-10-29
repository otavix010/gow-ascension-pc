@echo off
rem Compilação incremental (port\build_native) com 4 jobs, em BelowNormal.
setlocal
set "RAIZ=%~dp0.."
set "PATH=%PATH:"=%"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\x64\bin;%PATH%"
cd /d "%RAIZ%\port\build_native"
start /belownormal /wait /b ninja -j 4 gowa.exe > "%RAIZ%\out\ninja.log" 2>&1
echo exit=%ERRORLEVEL%
