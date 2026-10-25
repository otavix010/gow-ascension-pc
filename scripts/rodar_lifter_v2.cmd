@echo off
for %%I in ("%~dp0..") do set "RAIZ=%%~fI"
set "RAIZ_F=%RAIZ:\=/%"
rem Lift v2: igual ao rodar_lifter.cmd, mas com --code-end no fim do .text (0x661C14)
rem para o .rodata/.data do segmento R-X não virar "funções", e --chunks para TUs menores.
set R=%RAIZ%
cd /d %R%\ps3recomp
echo inicio %date% %time% > %R%\out\lifter_v2.log
python tools\ppu_lifter.py %R%\jogo\GOWA.elf --functions %R%\out\GOWA.functions.json --hle-stubs %R%\out\GOWA.imports.json --code-end 0x661C14 --chunks 48 --output %R%\port\recompiled -j 16 >> %R%\out\lifter_v2.log 2>&1
echo exit=%errorlevel% >> %R%\out\lifter_v2.log
python tools\gen_hle_nids.py --all --out %R%\port\recompiled\ppu_hle_nids.cpp >> %R%\out\lifter_v2.log 2>&1
echo fim %date% %time% exit=%errorlevel% >> %R%\out\lifter_v2.log
