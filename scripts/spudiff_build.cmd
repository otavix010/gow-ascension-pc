@echo off
for %%I in ("%~dp0..") do set "RAIZ=%%~fI"
set "RAIZ_F=%RAIZ:\=/%"
rem SPU differential test without the RPCS3 tree:
rem stub oracle (out\spudiff\stub_oracle.c): every trial uses the hand-written ISA reference.
rem   spudiff_build.cmd          -> interp_diff.exe (our interpreter vs reference)
rem   spudiff_build.cmd lifted   -> also lifted_diff.exe (lifter C vs reference)
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\x64\bin;%PATH%"
set T=%RAIZ%\ps3recomp\runtime\spu\tests
set O=%RAIZ%\out\spudiff
cd /d %O%
clang -std=gnu11 -O2 -w -D_CRT_SECURE_NO_WARNINGS -I"%T%\.." -I"%T%\rpcs3_oracle" -I"%O%" -c "%T%\spu_interp_diff_test.c" -o diff_test.o || goto :err
clang -std=gnu11 -O2 -w -I"%T%\rpcs3_oracle" -c stub_oracle.c -o stub_oracle.o || goto :err
clang diff_test.o stub_oracle.o -o interp_diff.exe || goto :err
echo built interp_diff.exe
if /i not "%1"=="lifted" goto :ok
"%O%\interp_diff.exe" --emit-cases 1 > cases.txt || goto :err
python "%T%\gen_lifted_cases.py" cases.txt > lifted_cases.inc || goto :err
clang -std=gnu11 -O2 -fwrapv -w -D_CRT_SECURE_NO_WARNINGS -DLIFTED_CASES_INC=\"lifted_cases.inc\" -I"%T%\.." -I"%T%\rpcs3_oracle" -I"%O%" -c "%T%\spu_interp_diff_test.c" -o diff_test_lifted.o || goto :err
clang diff_test_lifted.o stub_oracle.o -o lifted_diff.exe || goto :err
echo built lifted_diff.exe
:ok
echo exit=0
exit /b 0
:err
echo exit=1
exit /b 1
