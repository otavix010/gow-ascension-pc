@echo off
for %%I in ("%~dp0..") do set "RAIZ=%%~fI"
set "RAIZ_F=%RAIZ:\=/%"
rem Estágio 2 do ps3recomp: traduz o GOWA.elf para C dentro de port\recompiled
set R=%RAIZ%
if not exist %R%\port robocopy %R%\ps3recomp\templates\project %R%\port /E /NFL /NDL /NJH /NJS
cd /d %R%\ps3recomp
echo inicio %date% %time% > %R%\out\lifter.log
python tools\ppu_lifter.py %R%\jogo\GOWA.elf --functions %R%\out\GOWA.functions.json --hle-stubs %R%\out\GOWA.imports.json --output %R%\port\recompiled -j 16 >> %R%\out\lifter.log 2>&1
echo exit=%errorlevel% >> %R%\out\lifter.log
python tools\gen_hle_nids.py --all --out %R%\port\recompiled\ppu_hle_nids.cpp >> %R%\out\lifter.log 2>&1
echo fim %date% %time% exit=%errorlevel% >> %R%\out\lifter.log
