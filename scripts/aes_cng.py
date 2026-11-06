"""AES pelo CNG do Windows (bcrypt.dll), com a parte da interface do pycryptodome que as ferramentas usam:
AES.new(chave, AES.MODE_ECB | AES.MODE_CBC | AES.MODE_CTR, iv=..., counter=...).encrypt/decrypt e
Counter.new(128, initial_value=...).

Reports 331 e 334 (29/10): o Controle Inteligente de Aplicativos do Windows 11 bloqueia as DLLs sem assinatura
do pycryptodome (_raw_ctr.pyd, _cpuid_c.pyd: "WinError 4551"), e a geração parava ao decriptar o jogo ou o
update. O bcrypt.dll é do próprio Windows (assinado), então não é bloqueado. Só Windows.

Uso nas ferramentas:
    AES, Counter = aes_cng.carregar()   # o pycryptodome se ele funcionar, senão este módulo
GOWA_AES_CNG=1 força este módulo (para testar)."""
import ctypes
import os

_bcrypt = None
_algs = {}


def _st(r, onde):
    if r != 0:
        raise OSError(f'bcrypt {onde} falhou: NTSTATUS 0x{r & 0xFFFFFFFF:08X}')


def _alg(modo):
    global _bcrypt
    if _bcrypt is None:
        _bcrypt = ctypes.WinDLL('bcrypt')
        for nome in ('BCryptOpenAlgorithmProvider', 'BCryptSetProperty', 'BCryptGenerateSymmetricKey',
                     'BCryptEncrypt', 'BCryptDecrypt', 'BCryptDestroyKey'):
            getattr(_bcrypt, nome).restype = ctypes.c_long
    if modo not in _algs:
        h = ctypes.c_void_p()
        _st(_bcrypt.BCryptOpenAlgorithmProvider(ctypes.byref(h), ctypes.c_wchar_p('AES'), None, 0), 'OpenAlgorithmProvider')
        v = ctypes.create_unicode_buffer(modo)
        _st(_bcrypt.BCryptSetProperty(h, ctypes.c_wchar_p('ChainingMode'), v, ctypes.sizeof(v), 0), 'SetProperty')
        _algs[modo] = h
    return _algs[modo]


class _Cifra:
    def __init__(self, chave, modo, iv=None, counter=None):
        self.modo = modo
        self.iv = bytes(iv) if iv is not None else None
        self.counter = counter
        cng = 'ChainingModeCBC' if modo == AES.MODE_CBC else 'ChainingModeECB'
        self.h = ctypes.c_void_p()
        k = ctypes.create_string_buffer(bytes(chave), len(chave))
        _st(_bcrypt_alg_key(cng, self.h, k, len(chave)), 'GenerateSymmetricKey')

    def __del__(self):
        try:
            if self.h:
                _bcrypt.BCryptDestroyKey(self.h)
        except Exception:
            pass

    def _bloco(self, dados, cifrar):
        n = len(dados)
        if n % 16:
            raise ValueError('o tamanho tem que ser múltiplo de 16 neste modo')
        if not n:
            return b''
        ent = ctypes.create_string_buffer(bytes(dados), n)
        sai = ctypes.create_string_buffer(n)
        res = ctypes.c_ulong()
        if self.modo == AES.MODE_CBC:
            ivb = ctypes.create_string_buffer(self.iv, 16)
            iv_p, iv_n = ivb, 16
        else:
            iv_p, iv_n = None, 0
        f = _bcrypt.BCryptEncrypt if cifrar else _bcrypt.BCryptDecrypt
        _st(f(self.h, ent, n, None, iv_p, iv_n, sai, n, ctypes.byref(res), 0), 'Encrypt' if cifrar else 'Decrypt')
        if self.modo == AES.MODE_CBC:
            self.iv = ivb.raw[:16]   # o CNG deixa no buffer o último bloco (encadeamento)
        return sai.raw[:res.value]

    def _ctr(self, dados):
        n = len(dados)
        nb = (n + 15) // 16
        base = self.counter.proximo(nb)
        mask = (1 << 128) - 1
        out = bytearray()
        PASSO = 65536
        for i in range(0, nb, PASSO):
            k = min(PASSO, nb - i)
            ctrs = b''.join(((base + i + j) & mask).to_bytes(16, 'big') for j in range(k))
            ks = self._ecb.encrypt(ctrs)
            pedaco = dados[16 * i:16 * i + 16 * k]
            ks = ks[:len(pedaco)]
            out += (int.from_bytes(pedaco, 'big') ^ int.from_bytes(ks, 'big')).to_bytes(len(pedaco), 'big')
        return bytes(out)

    def encrypt(self, dados):
        if self.modo == AES.MODE_CTR:
            return self._ctr(dados)
        return self._bloco(dados, True)

    def decrypt(self, dados):
        if self.modo == AES.MODE_CTR:
            return self._ctr(dados)
        return self._bloco(dados, False)


def _bcrypt_alg_key(cng, h, k, n):
    a = _alg(cng)   # carrega o bcrypt.dll antes de usar _bcrypt
    return _bcrypt.BCryptGenerateSymmetricKey(a, ctypes.byref(h), None, 0, k, n, 0)


class _Contador:
    def __init__(self, bits, initial_value):
        if bits != 128:
            raise ValueError('só contador de 128 bits')
        self.v = initial_value

    def proximo(self, nb):
        v = self.v
        self.v += nb
        return v


class Counter:
    @staticmethod
    def new(nbits, initial_value=0, **_):
        return _Contador(nbits, initial_value)


class AES:
    MODE_ECB = 1
    MODE_CBC = 2
    MODE_CTR = 6

    @staticmethod
    def new(chave, modo, iv=None, counter=None, **kw):
        if 'IV' in kw and iv is None:
            iv = kw['IV']
        c = _Cifra(chave, modo, iv=iv, counter=counter)
        if modo == AES.MODE_CTR:
            c._ecb = _Cifra(chave, AES.MODE_ECB)
        return c


def carregar():
    """(AES, Counter): o pycryptodome se carregar e funcionar, senão o CNG do Windows"""
    if os.environ.get('GOWA_AES_CNG') != '1':
        try:
            from Crypto.Cipher import AES as A
            from Crypto.Util import Counter as C
            A.new(bytes(16), A.MODE_ECB).encrypt(bytes(16))   # as DLLs só carregam de verdade aqui
            A.new(bytes(16), A.MODE_CTR, counter=C.new(128, initial_value=1)).decrypt(bytes(16))
            return A, C
        except Exception:
            pass
    return AES, Counter
