"""Analisa os fragmentos de plugin SPU (código carregado em LS variável pelo
módulo de job 0x72DF80): conta as instruções que impedem rodar a tradução em
outro endereço de LS (relativas ao pc com valor: brsl/brasl/lqr/stqr; ila que
aponta para dentro do próprio fragmento) e os desvios para fora dele.

Uso: python plugins_spu_analise.py out\\plugins_getcode.txt
"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import collections
import os
import struct
import sys

R = RAIZ
sys.path.insert(0, os.path.join(R, 'ps3recomp', 'tools'))
from spu_disasm import disassemble_spu  # noqa: E402

d = open(os.path.join(R, 'jogo', 'GOWA.elf'), 'rb').read()
phoff = struct.unpack_from('>Q', d, 0x20)[0]
phnum = struct.unpack_from('>H', d, 0x38)[0]
phsz = struct.unpack_from('>H', d, 0x36)[0]
segs = []
for i in range(phnum):
    p = phoff + i * phsz
    t = struct.unpack_from('>I', d, p)[0]
    off, va, _pa, fsz = struct.unpack_from('>QQQQ', d, p + 8)
    if t == 1 and fsz:
        segs.append((off, va, fsz))


def va2f(v):
    for off, va, fsz in segs:
        if va <= v < va + fsz:
            return off + (v - va)


frag = collections.Counter()
for linha in open(sys.argv[1]):
    ea, sz, ls, wid = linha.split()
    if wid in ('19', '20', '21'):
        frag[(int(ea, 16), int(sz, 16))] += 1
tot = collections.Counter()
for (ea, sz), n in sorted(frag.items()):
    if 0x741B00 <= ea < 0x745F60:
        continue                                  # o próprio módulo 0x741B00
    o = va2f(ea)
    ins = disassemble_spu(d[o:o + sz], base_addr=0)
    c = collections.Counter()
    for x in ins:
        m = x.mnemonic
        if m in ('brsl', 'brasl', 'lqr', 'stqr', 'bra', 'lqa', 'stqa', 'bisl', 'bi', 'iret', 'bisled',
                 'rdch', 'wrch', 'rchcnt', 'stop', 'stopd', 'hbra', 'hbrr'):
            c[m] += 1
        if m == 'ila':
            c['ila'] += 1
        if m in ('br', 'brz', 'brnz', 'brhz', 'brhnz') and hasattr(x, 'target') and x.target is not None:
            if not (0 <= x.target < sz):
                c['br-fora'] += 1
    tot.update(c)
    print(f'0x{ea:06X} 0x{sz:04X} {len(ins):4d} instr  usos={n:3d}  ' + ' '.join(f'{k}={v}' for k, v in sorted(c.items())))
print('total:', dict(tot))
