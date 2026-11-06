"""O tradutor das SPUs da Otavix Engine PS3 para os scripts de tradução do port (lift_gowa_*.py).

O tradutor vem compilado no pacote da engine (engine\bin, que o gerar.ps1 baixa e confere): o spu_traduz.exe
traduz uma imagem, e o spu_ponte.exe dá o que estes scripts usavam dos módulos dele (o ELF em volta do código
cru e os alvos de desvio que a descoberta de funções não acha sozinha). As funções aqui têm os nomes e os
resultados das do tradutor.

Onde fica: OTX_SPU_LIFTER e OTX_SPU_PONTE (o gerar.ps1 define as duas), senão engine\bin na pasta do port."""
import os
import subprocess
import sys
import tempfile

_RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_BIN = os.path.join(_RAIZ, 'engine', 'bin')
LIFTER = os.environ.get('OTX_SPU_LIFTER') or os.path.join(_BIN, 'spu_traduz.exe')
PONTE = os.environ.get('OTX_SPU_PONTE') or os.path.join(_BIN, 'spu_ponte.exe')


def comando_lifter():
    """o começo da linha de comando do tradutor (um .exe roda direto; um .py, pelo Python)"""
    return [LIFTER] if LIFTER.lower().endswith('.exe') else [sys.executable, LIFTER]


def _roda(args):
    r = subprocess.run([PONTE] + [str(a) for a in args], capture_output=True)
    if r.returncode != 0:
        raise RuntimeError(f'{os.path.basename(PONTE)} {args[0]} falhou ({r.returncode}): '
                           f'{r.stderr.decode("utf-8", "replace")[-400:]}')
    return r.stdout.decode('utf-8', 'replace')


def _com_arquivo(cru, faz):
    fd, cam = tempfile.mkstemp(suffix='.bin', prefix='otx_ponte_')
    try:
        with os.fdopen(fd, 'wb') as f:
            f.write(bytes(cru))
        return faz(cam)
    finally:
        try:
            os.remove(cam)
        except OSError:
            pass


def embrulha(codigo, base=0, entrada=None):
    """o ELF da SPU com `codigo` no endereço `base` da memória local (bytes)"""
    def faz(cam):
        saida = cam + '.elf'
        try:
            _roda(['embrulha', cam, hex(base), '-' if entrada is None else hex(entrada), saida])
            with open(saida, 'rb') as f:
                return f.read()
        finally:
            if os.path.exists(saida):
                os.remove(saida)
    return _com_arquivo(codigo, faz)


_alvos_cache = {}


def _alvos(cru):
    chave = bytes(cru)
    if chave not in _alvos_cache:
        linhas = dict(l.split(':', 1) for l in _com_arquivo(cru, lambda cam: _roda(['alvos', cam])).splitlines()
                      if ':' in l)
        _alvos_cache[chave] = tuple([int(x, 16) for x in linhas[k].strip().split(',') if x]
                                    for k in ('retornos', 'casos'))
    return _alvos_cache[chave]


def retornos_montados(cru):
    """os endereços de retorno montados pelo código (alvos.retornos_montados do tradutor)"""
    return list(_alvos(cru)[0])


def casos_de_switch(cru):
    """os casos das tabelas de salto (alvos.casos_de_switch do tradutor)"""
    return list(_alvos(cru)[1])
