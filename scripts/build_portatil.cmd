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
set "PGO_MODO=-DGOWA_PGO="
set "PGO_PERFIL=-DGOWA_PGO_PROFILE="
if exist "%RAIZ%\port\pgo\gowa.profdata" set "PGO_MODO=-DGOWA_PGO=use"
if exist "%RAIZ%\port\pgo\gowa.profdata" set "PGO_PERFIL=-DGOWA_PGO_PROFILE=%RAIZ_F%/port/pgo/gowa.profdata"
rem FFmpeg 9 (shared, LGPL): FFMPEG_DIR = pasta com include\ e bin\ (padrão: dl\ffmpeg9\...)
if "%FFMPEG_DIR%"=="" set "FFMPEG_DIR=%RAIZ%\dl\ffmpeg9\ffmpeg-n9.0-latest-win64-lgpl-shared-9.0"
set "FFMPEG_F=%FFMPEG_DIR:\=/%"
rem GOWA_VS = pasta do Visual Studio 2022 com "C++ Clang tools" (o gerar.ps1 acha com o vswhere)
if "%GOWA_VS%"=="" set "GOWA_VS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
rem Tira as aspas do PATH: entrada como "C:\Program Files (x86)\ZeroTier\One\" quebra o vcvars64.bat
rem ("\ZeroTier\One\ was unexpected at this time"; reports 268, 273, 274, 283, 316, 318)
set "PATH=%PATH:"=%"
rem Reports 398 e 402 (30/10): mesmo assim o vcvars64.bat falhava com o PATH de alguns jogadores ("A sintaxe do
rem nome do arquivo... está incorreta" e depois "'cmake' não é reconhecido", ou "could not open 'kernel32.lib'").
rem Agora ele roda com o PATH só do Windows (a compilação só usa do PATH do jogador o Python, que volta depois) e a
rem saída dele fica no log em vez de ir para o nul.
set "PATH_JOGADOR=%PATH%"
set "PATH=%SystemRoot%\system32;%SystemRoot%;%SystemRoot%\System32\Wbem;%SystemRoot%\System32\WindowsPowerShell\v1.0;%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
echo -- vcvars64: "%GOWA_VS%\VC\Auxiliary\Build\vcvars64.bat"
call "%GOWA_VS%\VC\Auxiliary\Build\vcvars64.bat"
set "PATH=%GOWA_VS%\VC\Tools\Llvm\x64\bin;%PATH%"
if not "%GOWA_PY_DIR%"=="" set "PATH=%PATH%;%GOWA_PY_DIR%;%GOWA_PY_DIR%\Scripts"
if "%GOWA_PY_DIR%"=="" set "PATH=%PATH%;%PATH_JOGADOR%"
rem O cmake do próprio Visual Studio, pelo caminho completo (report 398: sem o vcvars ele nem estava no PATH)
set "CMAKE=%GOWA_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
rem Report 452 (30/10): sem o componente "C++ CMake tools" no Visual Studio, o cmake vinha de outra instalação no PATH
rem do jogador, que o PATH limpo acima tirou ("'cmake' não é reconhecido"). Procura no PATH dele e na pasta padrão.
if not exist "%CMAKE%" for %%X in (cmake.exe) do set "CMAKE=%%~$PATH_JOGADOR:X"
if "%CMAKE%"=="" if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "CMAKE=%ProgramFiles%\CMake\bin\cmake.exe"
if "%CMAKE%"=="" goto sem_cmake
if not defined WindowsSdkDir (
  echo GOWA_ERRO_SDK: o vcvars64.bat nao achou o Windows SDK ^(kernel32.lib^): falta o componente Windows 11 SDK no Visual Studio
  cmd /c exit 2
  goto fim
)
rem O ninja que vem com o Visual Studio, pelo caminho completo: o primeiro do PATH pode ser de outro
rem programa ou estar quebrado (report 332: 'ninja --version' terminou com 0xc000003e)
set "NINJA=%GOWA_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if not exist "%NINJA%" for %%X in (ninja.exe) do set "NINJA=%%~$PATH_JOGADOR:X"
if "%NINJA%"=="" goto sem_cmake
set "NINJA_F=%NINJA:\=/%"
rem Report 427 (30/10): uma pasta com parênteses ("GodofWarAscension(USA)") fechava o bloco "if (...)" no meio do
rem comando do cmake ("...ps3recomp foi inesperado neste momento"). Sem bloco, com goto, e os caminhos entre aspas
rem (uma pasta com espaço partia o argumento em dois).
rem O Python que o gerar.ps1 escolheu (GOWA_PY_EXE) para o find_package(Python3); sem ele, a busca padrão do cmake
set "PY_ARG=-DPython3_FIND_REGISTRY=FIRST"
if not "%GOWA_PY_EXE%"=="" set "PY_ARG=-DPython3_EXECUTABLE=%GOWA_PY_EXE:\=/%"
if exist "%DIR%\build.ninja" goto compila
"%CMAKE%" -S "%RAIZ%\port" -B "%DIR%" -G Ninja ^
  "-DCMAKE_MAKE_PROGRAM=%NINJA_F%" ^
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl ^
  "%PGO_MODO%" "%PGO_PERFIL%" "%PY_ARG%" ^
  -DGOWA_PPU_MARCH=x86-64-v3 -DGOWA_SPU_X86_V3=ON "-DGOWA_SPU_MARCH=%MARCH%" -DGOWA_HOST_FMA=ON ^
  -DGOWA_RUNTIME_FAST=ON -DGOWA_SPU_DAZ=ON "-DGOWA_SPU_RZ=%RZ%" ^
  "-DPS3RECOMP_DIR=%RAIZ_F%/ps3recomp" -DPS3RECOMP_FFMPEG_RUNTIME=ON -DPS3RECOMP_BUILD_TESTS=OFF ^
  "-DPS3RECOMP_FFMPEG_INCLUDE_DIR=%FFMPEG_F%/include"
if errorlevel 1 goto fim
:compila
cd /d "%DIR%"
if "%GOWA_JOBS%"=="" ("%NINJA%" gowa.exe) else ("%NINJA%" -j %GOWA_JOBS% gowa.exe)
goto fim
:sem_cmake
echo GOWA_ERRO_CMAKE: nem o cmake/ninja do Visual Studio nem outro no PATH: falta o componente C++ CMake tools no Visual Studio
cmd /c exit 3
:fim
echo exit=%ERRORLEVEL%
