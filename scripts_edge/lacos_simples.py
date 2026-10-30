"""Laços de um bloco só (o rótulo desvia para si mesmo no fim, contador decrementado de 1) num módulo traduzido,
candidatos ao laco_local.py: função, rótulo, linhas do corpo, contador e se já há vk antes do rótulo.
uso: python -I lacos_simples.py <spu_recomp.c> [prefixo das funções]"""
import re
import sys

s = open(sys.argv[1], encoding='latin-1').read()
pref = sys.argv[2] if len(sys.argv) > 2 else ''
for m in re.finditer(r'^static SPU_PN void (\w+)\(spu_context\* ctx\) \{\n', s, re.M):
    fn = m.group(1)
    if pref and pref not in fn:
        continue
    corpo = s[m.end():s.index('\n}\n', m.end())]
    linhas = corpo.split('\n')
    for k, l in enumerate(linhas):
        r = re.fullmatch(r'(loc_[0-9A-F]{8}):', l.strip())
        if not r:
            continue
        rot = r.group(1)
        n, ok, vk = 0, False, k > 0 and '/*VK' in linhas[k - 1]
        for t in linhas[k + 1:]:
            t = t.strip()
            if re.fullmatch(r'loc_[0-9A-F]{8}:', t) or 'goto' in t and rot not in t or 'SPU_' in t and 'spu_' not in t:
                break
            if f'goto {rot};' in t:
                mm = re.fullmatch(r'if \(ctx->gpr\[(\d+)\]\._u32\[0\] != 0\) goto ' + rot + ';', t)
                if mm and re.search(rf'ctx->gpr\[{mm.group(1)}\] = spu_ai\(ctx->gpr\[{mm.group(1)}\], -1\);', '\n'.join(linhas[k + 1:k + 2 + n])):
                    ok = True
                    print(f'{fn} {rot} {n} linhas, contador r{mm.group(1)}{" (com vk)" if vk else ""}')
                break
            n += 1
