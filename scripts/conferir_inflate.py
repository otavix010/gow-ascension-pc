"""Confere a saída do task SPU de inflate (Edge zlib) contra o zlib do host.

Lê os despejos de GOWA_DUMPQ (fila_tNNN.bin = 12 entradas de 0x20 da LFQueue;
buf_tNNN_<dst>.bin = buffer de saída de 64 KB), acha nos PSARC abertos pelo jogo
o bloco com o mesmo tamanho comprimido, descomprime com zlib e compara.

Uso: python conferir_inflate.py PASTA_DUMP
"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from psarc import Psarc  # noqa: E402

USR = Path(_os_raiz.path.join(RAIZ, r'vfs\PS3_GAME\USRDIR'))
ARQS = ['GAME.PSARC', 'SHELL.PSARC', 'INTROCIN.PSARC', 'RESOURCES.PSARC', 'HECA.PSARC',
        'ENGLISH.PSARC', 'MP-ENGLISH.PSARC']


def indice_blocos():
    """tamanho comprimido -> lista de (arquivo, offset do bloco no arquivo)."""
    idx = {}
    for nome in ARQS:
        p = Psarc(USR / nome)
        # offset de cada bloco: percorre as entradas e soma os tamanhos dos blocos
        for _md5, bloco, tam, off, _n in p.entradas:
            b, o, falta = bloco, off, tam
            while falta > 0 and b < len(p.blocos):
                c = p.blocos[b] or p.block_size
                idx.setdefault(c, []).append((nome, o))
                o += c
                falta -= p.block_size
                b += 1
    return idx


def main():
    pasta = Path(sys.argv[1])
    idx = indice_blocos()
    vistos = ok = ruim = semref = 0
    for fila in sorted(pasta.glob('fila_t*.bin')):
        t = fila.stem.split('_t')[1]
        d = fila.read_bytes()
        for e in range(12):
            src, dst, csz, dsz = struct.unpack_from('>IIII', d, e * 0x20)
            if dsz != 0x10000:
                continue
            buf = pasta / f'buf_t{t}_{dst:08X}.bin'
            if not buf.exists():
                continue
            saida = buf.read_bytes()
            vistos += 1
            cands = idx.get(csz, [])
            achou = None
            for nome, off in cands:
                with open(USR / nome, 'rb') as f:
                    f.seek(off)
                    cru = f.read(csz)
                try:
                    ref = zlib.decompress(cru)
                except zlib.error:
                    continue
                if ref[:64] == saida[:64]:
                    achou = (nome, off, ref)
                    break
            if not achou:
                semref += 1
                print(f't={t} dst={dst:08X} csz={csz:#x}: nenhum bloco de referência com o mesmo início '
                      f'({len(cands)} candidatos) primeiros bytes {saida[:16].hex()}')
                continue
            nome, off, ref = achou
            if ref == saida[:len(ref)]:
                ok += 1
            else:
                ruim += 1
                dif = next(i for i in range(len(ref)) if ref[i] != saida[i])
                print(f't={t} dst={dst:08X} {nome}@{off:#x}: DIFERENTE a partir do byte {dif:#x} '
                      f'(ref {ref[dif:dif+8].hex()} vs spu {saida[dif:dif+8].hex()})')
    print(f'{vistos} buffers conferidos: {ok} iguais ao zlib, {ruim} diferentes, {semref} sem referência')


if __name__ == '__main__':
    main()
