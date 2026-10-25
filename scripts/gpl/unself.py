"""Decripta um SELF/SPRX de PS3 (APP ou NPDRM) para ELF.

Licença: GPL-2.0 (veja COPYING nesta pasta). Segue o unself.cpp do RPCS3
(https://github.com/RPCS3/rpcs3), por isso fica fora da licença MIT do resto do
projeto. É um programa separado, chamado pelo gerar.ps1.

As chaves NÃO estão neste arquivo. Elas são lidas do key_vault.cpp/key_vault.h do
código-fonte do RPCS3: de uma pasta indicada em --vault ou, com --baixar, baixadas
do repositório do RPCS3 (versão fixa em COMMIT_RPCS3) para essa pasta.
A KLIC do GOWA.SELF sai da própria cópia do jogador (--klic-de EBOOT.elf).

uso:
  python unself.py ENTRADA.SELF SAIDA.elf --vault PASTA [--baixar] [--klic HEX32 | --klic-de EBOOT.elf]
"""
import argparse
import re
import struct
import sys
import urllib.request
import zlib
from pathlib import Path

try:
    from Crypto.Cipher import AES
    from Crypto.Util import Counter
except ImportError:
    sys.exit('falta o pycryptodome: python -m pip install pycryptodome')

KEY_APP, KEY_NPDRM = 4, 8
COMMIT_RPCS3 = '8b9ee60569753db8406b66d135cce112156172a1'
URL = 'https://raw.githubusercontent.com/RPCS3/rpcs3/' + COMMIT_RPCS3 + '/rpcs3/Crypto/'


def baixar_vault(pasta):
    pasta = Path(pasta)
    pasta.mkdir(parents=True, exist_ok=True)
    for nome in ('key_vault.cpp', 'key_vault.h'):
        destino = pasta / nome
        if destino.exists():
            continue
        print(f'baixando {nome} do RPCS3 ({URL}{nome})')
        with urllib.request.urlopen(URL + nome, timeout=60) as r:
            destino.write_bytes(r.read())


def carregar_vault(pasta):
    cpp = (Path(pasta) / 'key_vault.cpp').read_text(encoding='utf-8', errors='replace')
    h = (Path(pasta) / 'key_vault.h').read_text(encoding='utf-8', errors='replace')

    def tabela(funcao):
        corpo = re.search(r'void KeyVault::' + funcao + r'\(\)\s*\{(.*?)\n\}', cpp, re.S).group(1)
        chaves = {}
        for m in re.finditer(r'emplace_back\(\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+),\s*\w+,\s*"([0-9A-Fa-f]*)",\s*"([0-9A-Fa-f]*)"', corpo):
            rev = int(m.group(3), 16)
            chaves.setdefault(rev, (bytes.fromhex(m.group(4)), bytes.fromhex(m.group(5))))
        return chaves

    def array(nome):
        corpo = re.search(r'constexpr u8 ' + nome + r'\[[^\]]*\]\s*=\s*\{([^}]*)\}', h).group(1)
        return bytes(int(x, 16) for x in re.findall(r'0x[0-9A-Fa-f]{2}', corpo))

    return {
        KEY_APP: tabela('LoadSelfAPPKeys'),
        KEY_NPDRM: tabela('LoadSelfNPDRMKeys'),
        'NP_KLIC_KEY': array('NP_KLIC_KEY'),
        'NP_KLIC_FREE': array('NP_KLIC_FREE'),
    }


def u(fmt, buf, off):
    return struct.unpack_from('>' + fmt, buf, off)


class Cabecalho:
    def __init__(self, dados):
        magic, _hver, self.se_flags, _se_type, self.se_meta, self.se_hsize, _esize = u('IIHHIQQ', dados, 0)
        if magic != 0x53434500:
            raise SystemExit('não é SELF (magic SCE)')
        (_ver, prog_id_off, self.ehdr_off, self.phdr_off, self.shdr_off, _seg_ext_off,
         _version_off, supp_off, supp_size, _pad) = u('10Q', dados, 0x20)
        _auth_id, _vendor, self.program_type, _sceversion, _ = u('QIIQQ', dados, prog_id_off)
        self.tem_npdrm, self.licenca, off = False, None, supp_off
        while off < supp_off + supp_size:
            tipo, tam, _nxt = u('IIQ', dados, off)
            if tipo == 3:
                self.tem_npdrm = True
                self.licenca = u('I', dados, off + 0x10 + 8)[0]   # NPD: magic, version, license, type...
            if tam == 0:
                break
            off += tam


def meta_info(dados, cab, vault, klic):
    chaves = vault.get(cab.program_type) if cab.program_type in (KEY_APP, KEY_NPDRM) else None
    if chaves is None:
        raise SystemExit(f'tipo de programa {cab.program_type} não suportado')
    if cab.se_flags not in chaves:
        raise SystemExit(f'revisão de chave 0x{cab.se_flags:x} não encontrada no key_vault')
    erk, riv = chaves[cab.se_flags]
    mi = dados[cab.se_meta + 0x20:cab.se_meta + 0x60]
    if not (cab.se_flags & 0x8000):
        if cab.tem_npdrm:
            k = AES.new(vault['NP_KLIC_KEY'], AES.MODE_ECB).decrypt(klic)
            mi = AES.new(k, AES.MODE_CBC, iv=bytes(16)).decrypt(mi)
        mi = AES.new(erk, AES.MODE_CBC, iv=riv).decrypt(mi)
    return mi


def precisa_klic(cab):
    return cab.tem_npdrm and not (cab.se_flags & 0x8000) and cab.licenca != 3


def achar_klic(dados, cab, vault, eboot_elf):
    """A KLIC está nos segmentos do EBOOT decriptado; vale a que deixa os 32 bytes de
    preenchimento do metadata info zerados (falso positivo: 2^-256)."""
    elf = Path(eboot_elf).read_bytes()
    if elf[:4] != b'\x7fELF':
        raise SystemExit(f'{eboot_elf} não é ELF')
    phoff = u('Q', elf, 0x20)[0]
    phentsize, phnum = u('HH', elf, 0x36)
    vistas = set()
    for i in range(phnum):
        p_type, _f, p_offset, _va, _pa, p_filesz = u('IIQQQQ', elf, phoff + i * phentsize)
        if p_type != 1 or not p_filesz:
            continue
        seg = elf[p_offset:p_offset + p_filesz]
        for off in range(0, len(seg) - 15, 4):
            c = seg[off:off + 16]
            if len(set(c)) < 10 or c in vistas:
                continue
            vistas.add(c)
            mi = meta_info(dados, cab, vault, c)
            if mi[16:32] == bytes(16) and mi[48:64] == bytes(16):
                return c
    raise SystemExit(f'KLIC não encontrada em {eboot_elf} ({len(vistas)} candidatas)')


def unself(dados, vault, klic=None):
    cab = Cabecalho(dados)
    if cab.tem_npdrm and cab.licenca == 3 and klic is None:
        klic = vault['NP_KLIC_FREE']
    if precisa_klic(cab) and klic is None:
        raise SystemExit(f'NPDRM com licença {cab.licenca}: precisa de --klic ou --klic-de')
    mi = meta_info(dados, cab, vault, klic)
    key, key_pad, iv, iv_pad = mi[:16], mi[16:32], mi[32:48], mi[48:64]
    if key_pad[0] != 0 or iv_pad[0] != 0:
        raise SystemExit('falha ao decriptar o metadata info (KLIC ou chave errada)')

    ehdr = dados[cab.ehdr_off:cab.ehdr_off + 0x40]
    if ehdr[:4] != b'\x7fELF' or ehdr[4] != 2:
        raise SystemExit('só ELF64')
    e_phoff, e_shoff = u('QQ', ehdr, 0x20)
    e_phentsize, e_phnum, e_shentsize, e_shnum = u('HHHH', ehdr, 0x36)
    phdrs = [u('IIQQQQQQ', dados, cab.phdr_off + i * 0x38) for i in range(e_phnum)]

    ini = cab.se_meta + 0x60
    hdrs = AES.new(key, AES.MODE_CTR, counter=Counter.new(128, initial_value=int.from_bytes(iv, 'big'))).decrypt(dados[ini:cab.se_hsize])
    _sig_len, _u1, section_count, key_count, _opt, _u2, _u3 = u('QIIIIII', hdrs, 0)
    secoes = [u('QQIIIIIIII', hdrs, 0x20 + i * 0x30) for i in range(section_count)]
    kbase = 0x20 + section_count * 0x30
    data_keys = hdrs[kbase:kbase + key_count * 16]

    tamanho = max([e_shoff + e_shnum * e_shentsize] + [p[2] + p[5] for p in phdrs])
    elf = bytearray(tamanho)
    elf[0:0x40] = ehdr
    elf[e_phoff:e_phoff + e_phnum * e_phentsize] = dados[cab.phdr_off:cab.phdr_off + e_phnum * e_phentsize]
    for (d_off, d_size, tipo, prog_idx, _hashed, _sha1, encrypted, kidx, ividx, compressed) in secoes:
        if tipo != 2:
            continue
        bloco = dados[d_off:d_off + d_size]
        if encrypted == 3:
            k = data_keys[kidx * 16:kidx * 16 + 16]
            v = data_keys[ividx * 16:ividx * 16 + 16]
            bloco = AES.new(k, AES.MODE_CTR, counter=Counter.new(128, initial_value=int.from_bytes(v, 'big'))).decrypt(bloco)
        p = phdrs[prog_idx]
        if compressed == 2:
            bloco = zlib.decompress(bloco)
        elf[p[2]:p[2] + len(bloco)] = bloco
    if cab.shdr_off and e_shnum:
        elf[e_shoff:e_shoff + e_shnum * e_shentsize] = dados[cab.shdr_off:cab.shdr_off + e_shnum * e_shentsize]
    return bytes(elf), cab


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('entrada')
    ap.add_argument('saida')
    ap.add_argument('--vault', required=True, help='pasta com key_vault.cpp e key_vault.h do RPCS3')
    ap.add_argument('--baixar', action='store_true', help='baixa os dois arquivos do RPCS3 se faltarem')
    ap.add_argument('--klic', help='KLIC em hex (32 caracteres)')
    ap.add_argument('--klic-de', help='EBOOT.elf decriptado de onde tirar a KLIC')
    a = ap.parse_args()
    if a.baixar:
        baixar_vault(a.vault)
    vault = carregar_vault(a.vault)
    dados = Path(a.entrada).read_bytes()
    klic = bytes.fromhex(a.klic.removeprefix('0x')) if a.klic else None
    if klic is None and a.klic_de:
        cab = Cabecalho(dados)
        if cab.tem_npdrm and not (cab.se_flags & 0x8000):
            # a "licença livre" do cabeçalho nem sempre vale: o GOWA.SELF do update 1.04 diz
            # licença 3 e usa a KLIC do próprio jogo
            mi = meta_info(dados, cab, vault, vault['NP_KLIC_FREE'])
            if not (mi[16:32] == bytes(16) and mi[48:64] == bytes(16)):
                klic = achar_klic(dados, cab, vault, a.klic_de)
                print('KLIC encontrada no', a.klic_de)
    elf, cab = unself(dados, vault, klic)
    Path(a.saida).write_bytes(elf)
    print(f'ok: tipo={cab.program_type} revisão=0x{cab.se_flags:x} npdrm={cab.tem_npdrm} licença={cab.licenca} -> {a.saida} ({len(elf)} bytes)')


if __name__ == '__main__':
    main()
