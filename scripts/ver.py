"""Mostra uma função lifted de forma compacta (rN em vez de ctx->gpr[N], sem o ruído de XER/CR).

Uso: python ver.py func_0036E5F4 [mais funções...]
"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import re
import sys
from pathlib import Path

DIR = Path(_os_raiz.path.join(RAIZ, r'port\recompiled'))

SUBS = [
    # addi com carry: { uint64_t _a = ctx->gpr[A]; uint64_t _r = _a + (uint64_t)(int64_t)(IMM); ctx->gpr[D] = _r; ... }
    (re.compile(r'\{ uint64_t _a = ctx->gpr\[(\d+)\]; uint64_t _r = _a \+ \(uint64_t\)\(int64_t\)\(([^)]+)\); ctx->gpr\[(\d+)\] = _r;.*\}$'),
     lambda m: f'r{m[3]} = r{m[1]} + {m[2]}'),
    (re.compile(r'\{ uint64_t result = ctx->gpr\[(\d+)\] \+ ctx->gpr\[(\d+)\];.*ctx->gpr\[(\d+)\] = result; \}$'),
     lambda m: f'r{m[3]} = r{m[1]} + r{m[2]}'),
    (re.compile(r'\{ int64_t a = \(int32_t\)ctx->gpr\[(\d+)\]; int64_t b = \(int64_t\)([^;]+);.*cr_val << (\d+)\); \}$'),
     lambda m: f'cmpw cr{7 - int(m[3]) // 4}, r{m[1]}, {m[2]}'),
    (re.compile(r'\{ int64_t a = \(int32_t\)ctx->gpr\[(\d+)\]; int64_t b = \(int32_t\)ctx->gpr\[(\d+)\];.*cr_val << (\d+)\); \}$'),
     lambda m: f'cmpw cr{7 - int(m[3]) // 4}, r{m[1]}, r{m[2]}'),
    (re.compile(r'\{ uint64_t a = \(uint32_t\)ctx->gpr\[(\d+)\]; uint64_t b = \(uint64_t\)([^;]+);.*cr_val << (\d+)\); \}$'),
     lambda m: f'cmplw cr{7 - int(m[3]) // 4}, r{m[1]}, {m[2]}'),
    (re.compile(r'\{ int64_t a = \(int64_t\)ctx->gpr\[(\d+)\]; int64_t b = \(int64_t\)([^;]+);.*cr_val << (\d+)\); \}$'),
     lambda m: f'cmpd cr{7 - int(m[3]) // 4}, r{m[1]}, {m[2]}'),
    (re.compile(r'\(int64_t\)\(int32_t\)\(\(uint32_t\)(0x[0-9A-Fa-f]+|\d+) << 16\)'),
     lambda m: f'({m[1]}<<16)'),
    (re.compile(r'\(int64_t\)\(int32_t\)\(([-0-9A-Fa-fx]+)\)'), lambda m: m[1]),
    (re.compile(r'ctx->gpr\[(\d+)\]'), lambda m: f'r{m[1]}'),
    (re.compile(r'ctx->fpr\[(\d+)\]'), lambda m: f'f{m[1]}'),
    (re.compile(r'ctx->vr\[(\d+)\]'), lambda m: f'v{m[1]}'),
    (re.compile(r'; DRAIN_TRAMPOLINE\(ctx\);'), lambda m: ''),
    (re.compile(r'\(\(ctx->cr >> 29\) & 1\)'), lambda m: 'cr0.eq'),
    (re.compile(r'\(\(ctx->cr >> 30\) & 1\)'), lambda m: 'cr0.gt'),
    (re.compile(r'\(\(ctx->cr >> 31\) & 1\)'), lambda m: 'cr0.lt'),
    (re.compile(r'\(\(ctx->cr >> (\d+)\) & 1\)'),
     lambda m: f'cr{(31 - int(m[1])) // 4}.{["lt","gt","eq","so"][(31 - int(m[1])) % 4]}'),
]


def acha(nome):
    for p in sorted(DIR.glob('ppu_recomp_[0-9][0-9][0-9].cpp')):
        txt = p.read_bytes().decode('latin-1')
        i = txt.find('\nvoid ' + nome + '(ppu_context* ctx) {')
        if i >= 0:
            m = re.compile(r'\n}\r?\n').search(txt, i)
            return txt[i + 1:m.end()] if m else txt[i + 1:]
    return None


def compacta(corpo):
    out = []
    for ln in corpo.splitlines():
        s = ln.strip()
        if not s or s == '/* nop */;' or s.startswith('uint64_t _cs_') or s.startswith('ctx->gpr[') and '= _cs_' in s:
            continue
        for rx, f in SUBS:
            s = rx.sub(f, s) if rx.pattern.endswith('$') is False else (rx.sub(f, s))
        if s.endswith(':'):
            out.append(s)
        else:
            out.append('    ' + s)
    return '\n'.join(out)


if __name__ == '__main__':
    for n in sys.argv[1:]:
        c = acha(n)
        print(compacta(c) if c else f'// {n} não encontrada')
        print()
