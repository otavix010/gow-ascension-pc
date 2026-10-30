"""Laços de um bloco só que o laco_local.py aceita num módulo, sem vk antes do rótulo; os maiores primeiro.
uso: python -I candidatos_ll.py <spu_recomp.c> [mínimo de linhas]"""
import os
import re
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import laco_local

s = open(sys.argv[1], encoding='latin-1').read()
minimo = int(sys.argv[2]) if len(sys.argv) > 2 else 8
res = []
for m in re.finditer(r'^static SPU_PN void (\w+__b)\(spu_context\* ctx\) \{\n', s, re.M):
    fn = m.group(1)
    corpo = s[m.end():s.index('\n}\n', m.end())]
    ls = corpo.split('\n')
    for k, l in enumerate(ls):
        r = re.fullmatch(r'(loc_[0-9A-F]{8}):', l.strip())
        if not r or corpo.count(f'goto {r.group(1)};') != 1:
            continue
        rot = r.group(1)
        try:
            c, d = laco_local.corpo_do_laco(s, fn, rot)
        except Exception:
            continue
        if not re.fullmatch(r'if \((ctx->gpr\[\d+\]\._u(?:32|16)\[0\] (?:!=|==) 0)\) goto ' + rot + ';', d):
            continue
        ok = True
        for t in c:
            if t.startswith('/*'):
                continue
            g = re.sub(r'ctx->gpr\[\d+\]', 'G', t)
            if not (re.match(r'G = ', g) or g.startswith('spu_ls_write128(')) or not all(
                    g[:x.start()].endswith(('spu_ls_read128(', 'spu_ls_write128(', 'spu_ls_const128('))
                    for x in re.finditer(r'\bctx\b', g)):
                ok = False
                break
        vk = k > 0 and '/*VK' in ls[k - 1]
        if ok and not vk and len(c) >= minimo:
            res.append((len(c), fn, rot))
for n, fn, rot in sorted(res, reverse=True):
    print(n, fn, rot)
