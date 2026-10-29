@echo off
setlocal
set "RAIZ=%~dp0.."
set "PS3_VFS_ROOT=%RAIZ%\vfs"
set "PS3_LIBSRE_IMAGE=%RAIZ%\fw\lle\libsre_image.bin"
set "GOWA_CONF_REPLAY=%~2"
set "GOWA_CONF_REPS=%~3"
cd /d "%RAIZ%\port\build_native"
"%~1" "%RAIZ%\jogo\GOWA.elf" > "%RAIZ%\out\confere_run.log" 2>&1
echo exit=%ERRORLEVEL%
