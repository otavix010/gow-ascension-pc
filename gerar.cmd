@echo off
rem Clique duas vezes: gera e compila o God of War: Ascension a partir da sua copia (veja o README).
rem Pede as pastas em janelas; tambem aceita os parametros do gerar.ps1 (-Disco -Update -Firmware ...).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0gerar.ps1" %*
echo.
pause
