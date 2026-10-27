"""Pacote de shaders pré-compilados: junta caches rsx_shader_cache_v2 de vários PCs num zip.

Cada arquivo do cache (<V|P>_<fnv1a64 do HLSL>_<tamanho>.bin) tem um cabeçalho de 32 bytes
(magic, version, stage, source_length, blob_length, reserved, source_hash), o HLSL gerado e o
DXBC. O DXBC não depende da placa (D3DCompile com as opções 0, as mesmas do jogo); o jogo
confere cabeçalho e HLSL na carga, então uma entrada de outro decompilador só fica sem uso.
A biblioteca de PSO (rsx_pso_lib_*.bin) é por placa e driver e não entra.

uso: python pacote_shaders.py <saída.zip> <pasta do cache> [<pasta> ...] [--conferir N]
  --conferir N: recompila N shaders sorteados e exige o mesmo DXBC (só no Windows)"""
import hashlib
import json
import os
import random
import struct
import sys
import zipfile

MAGIC_VERSAO = 2
CAB = struct.Struct('<6IQ')


def fnv1a64(b):
    h = 1469598103934665603
    for x in b:
        h = ((h ^ x) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def le(path):
    """(estágio, hash, HLSL, DXBC, bytes do arquivo) ou None se não vale"""
    b = open(path, 'rb').read()
    if len(b) < CAB.size:
        return None
    magic, ver, st, sl, bl, _, h = CAB.unpack_from(b)
    if ver != MAGIC_VERSAO or st not in (ord('V'), ord('P')) or len(b) != CAB.size + sl + bl:
        return None
    src = b[CAB.size:CAB.size + sl]
    dx = b[CAB.size + sl:]
    nome = '%s_%016X_%08X.bin' % (chr(st), h, sl)
    if os.path.basename(path).upper() != nome.upper() or fnv1a64(src) != h or dx[:4] != b'DXBC':
        return None
    return chr(st), h, src, dx, b


def compila(src, stage):
    import ctypes
    from ctypes import wintypes
    d3d = ctypes.WinDLL('d3dcompiler_47')
    blob, err = ctypes.c_void_p(), ctypes.c_void_p()
    hr = d3d.D3DCompile(src, len(src), b'x', None, None, b'main', b'vs_5_0' if stage == 'V' else b'ps_5_0',
                        0, 0, ctypes.byref(blob), ctypes.byref(err))
    if hr != 0 or not blob.value:
        return None
    vt = ctypes.cast(ctypes.cast(blob, ctypes.POINTER(ctypes.c_void_p))[0], ctypes.POINTER(ctypes.c_void_p))
    ptr = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p)(vt[3])(blob)
    n = ctypes.CFUNCTYPE(ctypes.c_size_t, ctypes.c_void_p)(vt[4])(blob)
    out = ctypes.string_at(ptr, n)
    ctypes.CFUNCTYPE(wintypes.ULONG, ctypes.c_void_p)(vt[2])(blob)
    return out


def main():
    args = sys.argv[1:]
    conferir = 0
    if '--conferir' in args:
        i = args.index('--conferir'); conferir = int(args[i + 1]); del args[i:i + 2]
    saida, pastas = args[0], args[1:]
    entradas, ruins, repetidos = {}, 0, 0
    for p in pastas:
        for nome in sorted(os.listdir(p)):
            if not nome.endswith('.bin'):
                continue
            r = le(os.path.join(p, nome))
            if not r:
                ruins += 1
                continue
            chave = (r[0], r[1], len(r[2]))
            if chave in entradas:
                repetidos += 1
                if entradas[chave][3] != r[3]:
                    sys.exit('DXBC diferente para o mesmo HLSL: %s' % nome)
                continue
            entradas[chave] = r
    if conferir:
        for k in random.sample(sorted(entradas), min(conferir, len(entradas))):
            st, _, src, dx, _ = entradas[k]
            if compila(src, st) != dx:
                sys.exit('a recompilação não deu o mesmo DXBC: %s_%016X' % (st, k[1]))
        print('%d recompilados: DXBC igual' % min(conferir, len(entradas)))
    indice = []
    with zipfile.ZipFile(saida, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for (st, h, sl), r in sorted(entradas.items()):
            nome = '%s_%016X_%08X.bin' % (st, h, sl)
            z.writestr('rsx_shader_cache_v2/' + nome, r[4])
            indice.append(nome)
        z.writestr('rsx_shader_cache_v2/indice.json', json.dumps(
            {'versao_cache': MAGIC_VERSAO, 'shaders': len(indice),
             'vs': sum(1 for n in indice if n[0] == 'V'), 'ps': sum(1 for n in indice if n[0] == 'P')}))
    sha = hashlib.sha256(open(saida, 'rb').read()).hexdigest()
    print('%s: %d shaders (%d repetidos, %d ruins descartados), %.1f MB, sha256 %s' % (
        saida, len(indice), repetidos, ruins, os.path.getsize(saida) / 1048576.0, sha))


if __name__ == '__main__':
    main()
