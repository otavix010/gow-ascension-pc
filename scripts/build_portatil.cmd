@echo off
for %%I in ("%~dp0..") do set "RAIZ=%%~fI"
set "RAIZ_F=%RAIZ:\=/%"
rem Build portátil do gowa.exe em port\build_v3: código SPU traduzido em x86-64-v3 (AVX2, sem
rem AVX-512) em vez de znver5 -- roda em qualquer CPU x86-64 com AVX2 (Intel Haswell+ / AMD Zen+).
rem Os núcleos AVX-512 (.inc) caem no código traduzido (scripts\guarda_avx512.py, marca
rem GOWA_AVX512_GUARDA). Mesmo perfil PGO do build principal.
rem Uso: scripts\build_portatil.cmd [march] [rz]   (padrão x86-64-v3; ex.: x86-64-v4 para AVX-512)
rem rz: GOWA_SPU_RZ=ON (contas de float do SPU com o MXCSR em para-zero, para CPUs sem AVX-512),
rem     na pasta com sufixo rz (port\build_v3rz)
set "MARCH=%~1"
if "%MARCH%"=="" set "MARCH=x86-64-v3"
set "DIR=%RAIZ%\port\build_v3"
if not "%MARCH%"=="x86-64-v3" set "DIR=%RAIZ%\port\build_%MARCH%"
set "RZ=OFF"
if /i "%~2"=="rz" set "RZ=ON"
if /i "%~2"=="rz" set "DIR=%DIR%rz"
rem PGO só se houver perfil (port\pgo\gowa.profdata: gerado por quem roda o jogo com GOWA_PGO=gen)
set "PGO=-DGOWA_PGO="
if exist "%RAIZ%\port\pgo\gowa.profdata" set "PGO=-DGOWA_PGO=use -DGOWA_PGO_PROFILE=%RAIZ_F%/port/pgo/gowa.profdata"
rem FFmpeg 9 (shared, LGPL): FFMPEG_DIR = pasta com include\ e bin\ (padrão: dl\ffmpeg9\...)
if "%FFMPEG_DIR%"=="" set "FFMPEG_DIR=%RAIZ%\dl\ffmpeg9\ffmpeg-n9.0-latest-win64-lgpl-shared-9.0"
set "FFMPEG_F=%FFMPEG_DIR:\=/%"
rem GOWA_VS = pasta do Visual Studio 2022 com "C++ Clang tools" (o gerar.ps1 acha com o vswhere)
if "%GOWA_VS%"=="" set "GOWA_VS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
rem Tira as aspas do PATH: entrada como "C:\Program Files (x86)\ZeroTier\One\" quebra o vcvars64.bat
rem ("\ZeroTier\One\ was unexpected at this time"; reports 268, 273, 274, 283, 316, 318)
set "PATH=%PATH:"=%"
call "%GOWA_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=%GOWA_VS%\VC\Tools\Llvm\x64\bin;%PATH%"
if not exist "%DIR%\build.ninja" (
  cmake -S "%RAIZ%\port" -B "%DIR%" -G Ninja ^
    -DCMAKE_MAKE_PROGRAM=ninja ^
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl ^
    %PGO% ^
    -DGOWA_PPU_MARCH=x86-64-v3 -DGOWA_SPU_X86_V3=ON -DGOWA_SPU_MARCH=%MARCH% -DGOWA_HOST_FMA=ON ^
    -DGOWA_RUNTIME_FAST=ON -DGOWA_SPU_DAZ=ON -DGOWA_SPU_RZ=%RZ% ^
    -DPS3RECOMP_DIR=%RAIZ_F%/ps3recomp -DPS3RECOMP_FFMPEG_RUNTIME=ON -DPS3RECOMP_BUILD_TESTS=OFF ^
    -DPS3RECOMP_FFMPEG_INCLUDE_DIR=%FFMPEG_F%/include
  if errorlevel 1 goto fim
)
cd /d "%DIR%"
if "%GOWA_JOBS%"=="" (ninja gowa.exe) else (ninja -j %GOWA_JOBS% gowa.exe)
:fim
echo exit=%ERRORLEVEL%
