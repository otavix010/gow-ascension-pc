@echo off
for %%I in ("%~dp0..") do set "RAIZ=%%~fI"
set "RAIZ_F=%RAIZ:\=/%"
rem Configura o port\ com clang-cl + Ninja (ambiente x64 Native Tools).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\x64\bin;%PATH%"
cd /d %RAIZ%\port
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl ^
  -DPS3RECOMP_DIR=%RAIZ_F%/ps3recomp ^
  -DRECOMP_DIR=%RAIZ_F%/port/recompiled %*
echo exit=%ERRORLEVEL%
