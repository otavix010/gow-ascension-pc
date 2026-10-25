"""Tira o libsre.sprx do arquivo oficial de atualização do PS3 (PS3UPDAT.PUP).

Licença: GPL-2.0 (veja COPYING nesta pasta). Segue o instalador de firmware do RPCS3
(pup_object.cpp e o SCEDecrypter do unself.cpp, https://github.com/RPCS3/rpcs3).

O PUP é o arquivo que a PlayStation distribui de graça no site e no servidor de
atualização. Dentro dele, o update_files.tar traz os pacotes dev_flash_*: cada um é
um contêiner SCE criptografado (chaves SCEPKG do key_vault.h do RPCS3, lidas da
pasta --vault como no unself.py) cuja terceira seção é um TAR com arquivos do
dev_flash. O libsre.sprx fica em dev_flash/sys/external/.

uso:
  python pup_libsre.py PS3UPDAT.PUP SAIDA.sprx --vault PASTA [--baixar]
"""
import argparse
import io
import re
import struct
import sys
import tarfile
import zlib
from pathlib import Path

try:
    from Crypto.Cipher import AES
    from Crypto.Util import Counter
except ImportError:
    sys.exit('falta o pycryptodome: python -m pip install pycryptodome')

sys.path.insert(0, str(Path(__file__).resolve().parent))
from unself import baixar_vault   # noqa: E402

ALVO = 'dev_flash/sys/external/libsre.sprx'
ID_UPDATE_FILES = 0x300


def u(fmt, buf, off):
    return struct.unpack_from('>' + fmt, buf, off)


def chaves_pkg(pasta):
    h = (Path(pasta) / 'key_vault.h').read_text(encoding='utf-8', errors='replace')

    def array(nome):
        corpo = re.search(r'constexpr u8 ' + nome + r'\[[^\]]*\]\s*=\s*\{([^}]*)\}', h).group(1)
        return bytes(int(x, 16) for x in re.findall(r'0x[0-9A-Fa-f]{2}', corpo))

    return array('SCEPKG_ERK'), array('SCEPKG_RIV')


def update_files(pup):
    """O update_files.tar de dentro do PUP (entrada 0x300)."""
    magic, _pkg_ver, _img_ver, n, _hlen, _dlen = u('8sQQQQQ', pup, 0)
    if magic != b'SCEUF\0\0\0':
        raise SystemExit('não é um PUP do PS3 (falta SCEUF no começo); use o PS3UPDAT.PUP do site da PlayStation')
    for i in range(n):
        ident, off, tam = u('QQQ', pup, 0x30 + i * 0x20)
        if ident == ID_UPDATE_FILES:
            return pup[off:off + tam]
    raise SystemExit('o PUP não tem o update_files.tar')


def decripta_pkg(dados, erk, riv):
    """Seções de um contêiner SCE de pacote (SCEDecrypter do RPCS3)."""
    magic, _hver, _krev, _tipo, se_meta, se_hsize, _dlen = u('IIHHIQQ', dados, 0)
    if magic != 0x53434500:
        return []
    mi = AES.new(erk, AES.MODE_CBC, iv=riv).decrypt(dados[se_meta + 0x20:se_meta + 0x60])
    key, key_pad, iv, iv_pad = mi[:16], mi[16:32], mi[32:48], mi[48:64]
    if key_pad[0] != 0 or iv_pad[0] != 0:
        raise SystemExit('falha ao decriptar um pacote do firmware (chave SCEPKG errada?)')
    ctr = Counter.new(128, initial_value=int.from_bytes(iv, 'big'))
    hdrs = AES.new(key, AES.MODE_CTR, counter=ctr).decrypt(dados[se_meta + 0x60:se_hsize])
    _sig_len, _u1, n_sec, n_chaves, _opt, _u2, _u3 = u('QIIIIII', hdrs, 0)
    secoes = [u('QQIIIIIIII', hdrs, 0x20 + i * 0x30) for i in range(n_sec)]
    base = 0x20 + n_sec * 0x30
    data_keys = hdrs[base:base + n_chaves * 16]
    saida = []
    for (d_off, d_size, _t, _p, _h, _s, cripto, kidx, ividx, comprimido) in secoes:
        bloco = dados[d_off:d_off + d_size]
        if cripto == 3:
            k = data_keys[kidx * 16:kidx * 16 + 16]
            v = data_keys[ividx * 16:ividx * 16 + 16]
            bloco = AES.new(k, AES.MODE_CTR, counter=Counter.new(128, initial_value=int.from_bytes(v, 'big'))).decrypt(bloco)
        if comprimido == 2:
            bloco = zlib.decompress(bloco)
        saida.append(bloco)
    return saida


def acha_libsre(pup_bytes, erk, riv):
    externo = tarfile.open(fileobj=io.BytesIO(update_files(pup_bytes)))
    for m in externo.getmembers():
        if not m.name.startswith('dev_flash_') or not m.isfile():
            continue
        for secao in decripta_pkg(externo.extractfile(m).read(), erk, riv):
            if len(secao) < 512 or secao[257:262] != b'ustar':
                continue
            interno = tarfile.open(fileobj=io.BytesIO(secao))
            for mm in interno.getmembers():
                if mm.name.lstrip('./') == ALVO:
                    return interno.extractfile(mm).read(), m.name
    raise SystemExit('não achei o libsre.sprx no PUP')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('pup')
    ap.add_argument('saida')
    ap.add_argument('--vault', required=True, help='pasta com key_vault.cpp e key_vault.h do RPCS3')
    ap.add_argument('--baixar', action='store_true', help='baixa os dois arquivos do RPCS3 se faltarem')
    a = ap.parse_args()
    if a.baixar:
        baixar_vault(a.vault)
    erk, riv = chaves_pkg(a.vault)
    sprx, origem = acha_libsre(Path(a.pup).read_bytes(), erk, riv)
    Path(a.saida).write_bytes(sprx)
    print(f'ok: {ALVO} de {origem} -> {a.saida} ({len(sprx)} bytes)')


if __name__ == '__main__':
    main()
