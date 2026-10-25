@echo off
for %%I in ("%~dp0..") do set "RAIZ=%%~fI"
set "RAIZ_F=%RAIZ:\=/%"
rem Roda ninja no port\build com o ambiente do clang-cl. Argumentos vão direto para o ninja.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\x64\bin;%PATH%"
cd /d %RAIZ%\port\build
ninja %*
echo exit=%ERRORLEVEL%
