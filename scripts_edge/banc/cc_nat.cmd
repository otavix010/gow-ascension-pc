@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\x64\bin;%PATH%"
cd /d %~dp0
clang-cl /nologo -D_CRT_SECURE_NO_WARNINGS -DSPU_LS_DIRECT -DSPU_LS_NO_PROBES -DSPU_SX_DAZ /O2 /std:c17 -MD -Wno-unused-parameter -Wno-unused-function -fno-strict-aliasing -fwrapv /GS- /clang:-march=native /clang:-ffp-contract=off -I%~dp0..\..\ps3recomp\runtime\spu -I%~dp0..\..\ps3recomp\include -I%~dp0..\..\ps3recomp\runtime\ppu -I%~dp0..\..\ps3recomp\runtime\memory -I%~dp0..\..\port %1.c /Fe%1_nat.exe /link /STACK:8000000 %2 %3 %4
echo exit=%ERRORLEVEL%
