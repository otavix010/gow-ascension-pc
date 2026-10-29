@echo off
rem Reproduz as gravações do EDGE (out\confere) com o exe da bancada. Uso: scripts_edge\confere.cmd [exe] [reps]
setlocal
set "RAIZ=%~dp0.."
set "EXE=%~1"
if "%EXE%"=="" set "EXE=%RAIZ%\port\build_native\gowa.exe"
set "GOWA_CONF_REPS=%~2"
if "%GOWA_CONF_REPS%"=="" set "GOWA_CONF_REPS=5"
set "PS3_VFS_ROOT=%RAIZ%\vfs"
set "PS3_LIBSRE_IMAGE=%RAIZ%\fw\lle\libsre_image.bin"
set "GOWA_CONF_REPLAY=%RAIZ%\out\confere"
cd /d "%RAIZ%\port\build_native"
"%EXE%" "%RAIZ%\jogo\GOWA.elf" > "%RAIZ%\out\confere_run.log" 2>&1
echo exit=%ERRORLEVEL%
