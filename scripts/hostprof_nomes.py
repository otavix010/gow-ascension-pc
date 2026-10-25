import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
# Traduz os "rt RVA 0x..." de um log com GOWA_HOSTPROF para nomes de função,
# pelo gowa.map (públicos e estáticos). O RVA é o início do bloco de unwind
# (.pdata), então cai na função que o contém. Os logs têm que ser do MESMO
# executável que o mapa.
# Uso: python -I hostprof_nomes.py out\hp_E.log [port\build\gowa.map] [N]
import bisect, re, sys

log = sys.argv[1]
mapf = sys.argv[2] if len(sys.argv) > 2 else _os_raiz.path.join(RAIZ, r'port\build\gowa.map')
top = int(sys.argv[3]) if len(sys.argv) > 3 else 12

base = 0x140000000
syms = []
re_base = re.compile(r'Preferred load address is ([0-9a-fA-F]+)')
re_sym = re.compile(r'^\s*[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{16})\s+(?:f\s+)?(?:i\s+)?(\S+)?')
for ln in open(mapf, encoding='utf-8', errors='replace'):
    m = re_base.search(ln)
    if m:
        base = int(m.group(1), 16)
        continue
    m = re_sym.match(ln)
    if m:
        va = int(m.group(2), 16)
        if va >= base:
            syms.append((va - base, m.group(1), (m.group(3) or '').split(':')[-1]))
syms.sort()
addrs = [a for a, _, _ in syms]


def nome(rva):
    i = bisect.bisect_right(addrs, rva) - 1
    if i < 0:
        return '?'
    a, n, obj = syms[i]
    return '%s+0x%X (%s)' % (n, rva - a, obj)


cab = re.compile(r'^\[hostprof\] tid (\d+) \(([^)]*)\): (\d+) amostras, (.*)$')
lin = re.compile(r'^\[hostprof\]\s+([\d.]+)%\s+rt RVA 0x([0-9A-Fa-f]+)')
atual = None
for ln in open(log, encoding='utf-8', errors='replace'):
    ln = ln.rstrip('\n')
    m = cab.match(ln)
    if m:
        atual = m
        print('\n== tid %s (%s): %s' % (m.group(1), m.group(2), m.group(4)))
        continue
    m = lin.match(ln)
    if m and atual:
        print('   %5s%%  %s' % (m.group(1), nome(int(m.group(2), 16))))
        continue
    if atual and ln.startswith('[hostprof]') and ' dll ' in ln:
        print('   ' + ln[len('[hostprof]'):].strip())
