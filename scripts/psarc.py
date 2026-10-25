"""Lista e extrai arquivos PSARC (formato de arquivo da Sony, PS3).

Uso:
  python psarc.py listar ARQUIVO.psarc
  python psarc.py extrair ARQUIVO.psarc PASTA_SAIDA [--filtro TEXTO]

Formato (público, big-endian):
  cabeçalho 0x20: 'PSAR', versão u16+u16, compressão 'zlib'/'lzma', toc_length u32,
  toc_entry_size u32 (30), toc_entries u32, block_size u32, archive_flags u32
  (bit 1 = nomes relativos ignorando maiúsculas, bit 2 = caminhos absolutos).
  Entradas do TOC (30 bytes): md5 do nome (16), índice do 1º bloco u32,
  tamanho descomprimido u40, offset no arquivo u40.
  Depois do TOC vem a tabela de tamanhos comprimidos de bloco (u16/u24/u32
  conforme block_size). Tamanho 0 = bloco cheio sem compressão.
  A entrada 0 é o manifesto: os nomes dos demais arquivos, um por linha.
"""
import argparse
import hashlib
import lzma
import struct
import zlib
from pathlib import Path


class Psarc:
    def __init__(self, caminho):
        self.f = open(caminho, 'rb')
        cab = self.f.read(0x20)
        magic, vmaj, vmin, comp, toc_len, ent_size, n_ent, bsize, flags = struct.unpack('>4sHH4sIIIII', cab)
        assert magic == b'PSAR', 'não é PSARC'
        self.versao = (vmaj, vmin)
        self.compressao = comp.decode()
        self.block_size = bsize
        self.flags = flags
        toc = self.f.read(toc_len - 0x20)
        self.entradas = []
        for i in range(n_ent):
            e = toc[i * ent_size:(i + 1) * ent_size]
            md5 = e[:16]
            bloco = struct.unpack('>I', e[16:20])[0]
            tam = int.from_bytes(e[20:25], 'big')
            off = int.from_bytes(e[25:30], 'big')
            self.entradas.append([md5, bloco, tam, off, None])
        # tabela de tamanhos de bloco
        resto = toc[n_ent * ent_size:]
        nb = 2 if bsize <= 0x10000 else (3 if bsize <= 0x1000000 else 4)
        self.blocos = [int.from_bytes(resto[i:i + nb], 'big') for i in range(0, len(resto) - nb + 1, nb)]
        # manifesto
        nomes = self.ler(0).decode('utf-8', 'replace').replace('\r', '').split('\n') if n_ent else []
        nomes = [n for n in nomes if n]
        for i, n in enumerate(nomes, start=1):
            if i < len(self.entradas):
                self.entradas[i][4] = n

    def ler(self, idx):
        _md5, bloco, tam, off, _nome = self.entradas[idx]
        self.f.seek(off)
        partes, falta = [], tam
        while falta > 0:
            ctam = self.blocos[bloco]
            esperado = min(self.block_size, falta)
            if ctam == 0:
                dados = self.f.read(self.block_size)
            else:
                cru = self.f.read(ctam)
                if ctam == esperado:
                    dados = cru  # bloco guardado sem compressão
                elif self.compressao == 'zlib' and cru[:1] == b'\x78':
                    dados = zlib.decompress(cru)
                elif self.compressao == 'lzma':
                    dados = lzma.decompress(cru, format=lzma.FORMAT_ALONE)
                else:
                    dados = cru
            partes.append(dados[:esperado])
            falta -= len(dados[:esperado])
            bloco += 1
        return b''.join(partes)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('acao', choices=['listar', 'extrair'])
    ap.add_argument('arquivo')
    ap.add_argument('saida', nargs='?')
    ap.add_argument('--filtro')
    a = ap.parse_args()
    p = Psarc(a.arquivo)
    print(f'PSARC v{p.versao[0]}.{p.versao[1]} {p.compressao} bloco={p.block_size} flags={p.flags:#x} '
          f'arquivos={len(p.entradas) - 1}')
    for i, (md5, _b, tam, off, nome) in enumerate(p.entradas[1:], start=1):
        if a.filtro and (not nome or a.filtro.lower() not in nome.lower()):
            continue
        if a.acao == 'listar':
            print(f'{tam:10d}  {nome}')
        else:
            dados = p.ler(i)
            rel = (nome or md5.hex()).lstrip('/')
            alvo = Path(a.saida) / rel
            alvo.parent.mkdir(parents=True, exist_ok=True)
            alvo.write_bytes(dados)
            ok = hashlib.md5((nome or '').encode()).digest() == md5 or hashlib.md5((nome or '').upper().encode()).digest() == md5
            print(f'{tam:10d}  {rel}{"" if ok else "  (md5 do nome não confere)"}')


if __name__ == '__main__':
    main()
