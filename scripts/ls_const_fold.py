"""Constantes do pool dos módulos de job no código SPU traduzido.

Os módulos de job WWS (port/spu/gowajm_<VA>/) rodam a partir da LS 0x4000 e
leem máscaras de shufb e outras constantes de endereços fixos da própria
imagem (lqa/lqr, traduzidos como spu_ls_read128(ctx, 0xADDR)). Este script troca
cada uma dessas leituras, quando o endereço cai dentro da imagem, por
spu_ls_const128(ctx, 0xADDR, w0, w1, w2, w3) com os bytes do GOWA.elf: como
constante, o compilador dobra o valor nos usos (um shufb de controle conhecido
vira um só embaralhamento). Build com GOWA_LS_CONST_CHECK=ON confere cada uma
contra a LS e avisa ([ls-const]) se o job escrever ali.

Uso:
  python ls_const_fold.py aplicar   (idempotente; backup <arquivo>.sem_const;
                                     só as que parecem máscara de shufb, ou
                                     todas com --todas -- inseguro, ver abaixo)
  python ls_const_fold.py tirar     (volta às leituras da LS)
  python ls_const_fold.py ver       (quantas por módulo)
"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import os
import re
import shutil
import struct
import sys

R = RAIZ
ELF = os.path.join(R, 'jogo', 'GOWA.elf')
SPU = os.path.join(R, 'port', 'spu')
INC = os.path.join(R, 'port', 'gowa_jobmods.inc')
BASE = 0x4000

LER = re.compile(r'spu_ls_read128\(ctx, 0x([0-9A-Fa-f]+)\)')
CONST = re.compile(r'spu_ls_const128\(ctx, 0x([0-9A-Fa-f]+), 0x[0-9A-F]{8}u, 0x[0-9A-F]{8}u, 0x[0-9A-F]{8}u, 0x[0-9A-F]{8}u\)')


def segmentos(d):
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
    return segs


def modulos():
    """(va, bytes, nome) de gowa_jobmods.inc"""
    txt = open(INC, encoding='utf-8').read()
    out = []
    for m in re.finditer(r'\{ 0x([0-9A-F]{8})u, 0x([0-9A-F]+)u, (gowajm_[0-9A-F]{8})_spu_recomp_register \}', txt):
        out.append((int(m.group(1), 16), int(m.group(2), 16), m.group(3)))
    return out


def imagem(d, segs, va, n):
    for off, sva, fsz in segs:
        if sva <= va < sva + fsz:
            o = off + (va - sva)
            return d[o:o + n]
    return None


def parece_mascara(w):
    """Só quadwords com cara de controle de shufb: todo byte é um seletor
    (< 0x20) ou um especial exato (0x80/0xC0/0xE0), no máximo 4 bytes zero e
    não tudo zero. A imagem também tem .bss (zeros) e .data que o job ou o
    carregamento reescrevem (tabelas de índice que viram ponteiros): a
    conferência (GOWA_LS_CONST_CHECK, run756) achou 37 endereços assim entre
    1694, e nenhum passa neste filtro."""
    b = b''.join(x.to_bytes(4, 'big') for x in w)
    if not any(b) or b.count(0) > 4:
        return False
    return all(x < 0x20 or x in (0x80, 0xC0, 0xE0) for x in b)


def aplicar():
    todas = '--todas' in sys.argv
    d = open(ELF, 'rb').read()
    segs = segmentos(d)
    total = 0
    for va, n, nome in modulos():
        img = imagem(d, segs, va, n)
        src = os.path.join(SPU, nome, 'spu_recomp.c')
        if img is None or not os.path.exists(src):
            print('sem imagem ou fonte:', nome)
            continue
        s = open(src, 'rb').read().decode('latin-1')
        cont = [0]

        def troca(m):
            a = int(m.group(1), 16) & 0x3FFF0
            if not (BASE <= a and a + 16 <= BASE + len(img)):
                return m.group(0)
            o = a - BASE
            w = struct.unpack_from('>IIII', img, o)
            if not todas and not parece_mascara(w):
                return m.group(0)
            cont[0] += 1
            return ('spu_ls_const128(ctx, 0x%X, 0x%08Xu, 0x%08Xu, 0x%08Xu, 0x%08Xu)'
                    % (int(m.group(1), 16), w[0], w[1], w[2], w[3]))
        s2 = LER.sub(troca, s)
        if cont[0]:
            bak = src + '.sem_const'
            if not os.path.exists(bak):
                shutil.copyfile(src, bak)
            open(src, 'wb').write(s2.encode('latin-1'))
            print(f'{nome}: {cont[0]} constantes (imagem 0x{len(img):X} bytes)')
            total += cont[0]
    print('total', total)


def segmentos_texto(elf):
    """PT_LOAD sem escrita (o texto) de uma ELF SPU de 32 bits: (va, bytes)"""
    d = open(elf, 'rb').read()
    phoff = struct.unpack_from('>I', d, 0x1C)[0]
    phnum = struct.unpack_from('>H', d, 0x2C)[0]
    phsz = struct.unpack_from('>H', d, 0x2A)[0]
    out = []
    for i in range(phnum):
        t, off, va, _pa, fsz, _msz, fl, _al = struct.unpack_from('>8I', d, phoff + i * phsz)
        if t == 1 and fsz and not (fl & 2):
            out.append((va, d[off:off + fsz]))
    return out


def aplicar_elfs():
    """As imagens de tarefa (port/spu/spu_<sha>/image.elf, carregadas no
    endereço da própria ELF): só o segmento de texto, que é somente leitura --
    ali não precisa do filtro de máscara."""
    total = 0
    for nome in sorted(os.listdir(SPU)):
        elf = os.path.join(SPU, nome, 'image.elf')
        src = os.path.join(SPU, nome, 'spu_recomp.c')
        if not (nome.startswith('spu_') and os.path.exists(elf) and os.path.exists(src)):
            continue
        txt = segmentos_texto(elf)
        s = open(src, 'rb').read().decode('latin-1')
        cont = [0]

        def troca(m):
            a = int(m.group(1), 16) & 0x3FFF0
            for va, b in txt:
                if va <= a and a + 16 <= va + len(b):
                    w = struct.unpack_from('>IIII', b, a - va)
                    cont[0] += 1
                    return ('spu_ls_const128(ctx, 0x%X, 0x%08Xu, 0x%08Xu, 0x%08Xu, 0x%08Xu)'
                            % (int(m.group(1), 16), w[0], w[1], w[2], w[3]))
            return m.group(0)
        s2 = LER.sub(troca, s)
        if cont[0]:
            bak = src + '.sem_const'
            if not os.path.exists(bak):
                shutil.copyfile(src, bak)
            open(src, 'wb').write(s2.encode('latin-1'))
            print(f'{nome[:12]}...: {cont[0]} constantes do texto')
            total += cont[0]
    print('total (tarefas)', total)


def fontes():
    for va, n, nome in modulos():
        yield nome, os.path.join(SPU, nome, 'spu_recomp.c')
    for nome in sorted(os.listdir(SPU)):
        if nome.startswith('spu_') and os.path.exists(os.path.join(SPU, nome, 'image.elf')):
            yield nome, os.path.join(SPU, nome, 'spu_recomp.c')


def tirar():
    for nome, src in fontes():
        if not os.path.exists(src):
            continue
        s = open(src, 'rb').read().decode('latin-1')
        s2, k = CONST.subn(lambda m: 'spu_ls_read128(ctx, 0x%s)' % m.group(1), s)
        if k:
            open(src, 'wb').write(s2.encode('latin-1'))
            print(f'{nome}: {k} constantes voltaram a ler a LS')


def ver():
    for nome, src in fontes():
        if os.path.exists(src):
            s = open(src, 'rb').read().decode('latin-1')
            k = len(CONST.findall(s))
            if k:
                print(f'{nome}: {k}')


if __name__ == '__main__':
    # 'tarefas': também as imagens ELF de tarefa. A/B abtask (20/10): sem ganho
    # (37,70 sem x ~36,7 com) e uma das três rodadas travou aos 23 s -- fora do
    # padrão.
    {'aplicar': aplicar, 'tarefas': aplicar_elfs, 'tirar': tirar, 'ver': ver}[sys.argv[1] if len(sys.argv) > 1 else 'ver']()
