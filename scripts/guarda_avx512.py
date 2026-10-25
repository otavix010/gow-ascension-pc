import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
# Protege os núcleos AVX-512 (.inc gerados por vetorizar_laco.py e afins) para builds sem AVX-512:
# sem __AVX512F__/BW/VL/DQ, cada função pública vira "não fiz nada" (int -> 0, void -> vazio), que é
# exatamente o que ela devolve com GOWA_VK=0: o código traduzido faz o laço inteiro.
# uso: guarda_avx512.py aplicar|tirar
import glob, os, re, sys
R = _os_raiz.path.join(RAIZ, r'port\spu')
MARCA = '/* GOWA_AVX512_GUARDA */'
COND = '#if (defined(__AVX512F__) && defined(__AVX512BW__) && defined(__AVX512VL__) && defined(__AVX512DQ__)) || defined(GOWA_VK_AVX2)'
modo = sys.argv[1]
n = 0
for p in sorted(glob.glob(os.path.join(R, '*', '*.inc'))):
    raw = open(p, 'rb').read()
    b = raw.decode('latin-1')
    if 'spu_vk.h' not in b and '_mm512' not in b:
        continue
    nl = '\r\n' if b.count('\r\n') == b.count('\n') and '\r\n' in b else '\n'
    t = b.replace('\r\n', '\n')
    if modo == 'tirar':
        if MARCA not in t: continue
        i = t.index(MARCA)
        j = t.index('\n', i) + 1           # a linha da marca + a do #if
        j = t.index('\n', j) + 1
        k = t.rindex('\n#else /* GOWA_AVX512_GUARDA */')
        t = t[:i] + t[j:k + 1]
    else:
        if MARCA in t: continue
        sigs = re.findall(r'^((?:int|void) gowa_[A-Za-z0-9_]+\([^;{]*\))\s*\{', t, re.M)
        if not sigs:
            print('SEM FUNÇÕES PÚBLICAS', p); continue
        fb = []
        for s in sigs:
            fb.append(s + (' { return 0; }' if s.startswith('int ') else ' { }'))
        if not t.endswith('\n'): t += '\n'
        t = (MARCA + '\n' + COND + '\n' + t +
             '#else /* GOWA_AVX512_GUARDA */\n/* sem AVX-512: o código traduzido faz tudo (como GOWA_VK=0) */\n' +
             '\n'.join(fb) + '\n#endif /* GOWA_AVX512_GUARDA */\n')
    open(p, 'wb').write(t.replace('\n', nl).encode('latin-1'))
    n += 1
    print(modo, os.path.relpath(p, R))
print(n, 'arquivos')
