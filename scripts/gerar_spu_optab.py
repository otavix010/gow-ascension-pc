"""Gera spu_optab.h (tabela de opcodes SPU) a partir do SPUOpcodes.h do RPCS3, como o
ps3recomp/runtime/spu/tests/rpcs3_oracle/extract.py faz, sem precisar do resto da árvore.
uso: python gerar_spu_optab.py <SPUOpcodes.h> <saida spu_optab.h>"""
import re
import sys

H = open(sys.argv[1], encoding='utf-8').read()
ents = re.findall(r"\{ (\d), (0x[0-9a-f]+), GET\((\w+)\) \}", H)
linhas = ["/* GENERATED (gowa) from RPCS3 SPUOpcodes.h. X(magn, value, NAME): insn>>21 == value<<magn | low magn bits */",
          "#define SPU_OPTAB(X) \\"]
linhas.append(" \\\n".join(f"    X({m}, {v}, {n})" for m, v, n in ents))
open(sys.argv[2], 'w').write("\n".join(linhas) + "\n")
print(len(ents), 'opcodes')
