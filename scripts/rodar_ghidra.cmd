@echo off
for %%I in ("%~dp0..") do set "RAIZ=%%~fI"
set "RAIZ_F=%RAIZ:\=/%"
rem Análise headless do Ghidra no GOWA.elf (nomes de funções, strings, decompilação)
set R=%RAIZ%
set JAVA_HOME=C:\Program Files\Eclipse Adoptium\jdk-21.0.12.101-hotspot
set PATH=%JAVA_HOME%\bin;%PATH%
if not exist %R%\ghidra-proj mkdir %R%\ghidra-proj
cd /d %R%\ps3recomp
echo inicio %date% %time% > %R%\out\ghidra.log
python tools\ghidra_analyze.py %R%\jogo\GOWA.elf -o %R%\out\ghidra --ghidra-dir %R%\tools\ghidra_12.1.4_PUBLIC --decompile --keep-project --project-dir %R%\ghidra-proj >> %R%\out\ghidra.log 2>&1
echo fim %date% %time% exit=%errorlevel% >> %R%\out\ghidra.log
