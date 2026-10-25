"""Lista os ELFs SPU embutidos no GOWA.elf (VA = offset no arquivo + 0x10000,
como no resto do projeto): VA, tamanho estimado pelos segmentos, entrada e
segmentos PT_LOAD (endereço LS, tamanho em arquivo/memória)."""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import struct
import sys

ELF = _os_raiz.path.join(RAIZ, r'jogo\GOWA.elf')
data = open(ELF, 'rb').read()
pos = 0
while True:
    pos = data.find(b'\x7fELF', pos)
    if pos < 0:
        break
    try:
        ei_class = data[pos + 4]
        e_machine = struct.unpack('>H', data[pos + 18:pos + 20])[0]
        if ei_class == 1 and e_machine == 0x17:   # ELF32, SPU
            e_entry, e_phoff = struct.unpack('>II', data[pos + 24:pos + 32])
            e_phentsize, e_phnum = struct.unpack('>HH', data[pos + 42:pos + 46])
            segs = []
            end = 0
            for i in range(e_phnum):
                o = pos + e_phoff + i * e_phentsize
                p_type, p_off, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = struct.unpack('>8I', data[o:o + 32])
                if p_type == 1:
                    segs.append((p_off, p_vaddr, p_filesz, p_memsz, p_flags))
                end = max(end, p_off + p_filesz)
            print(f'VA 0x{pos + 0x10000:08X}  tam~0x{end:X}  entrada 0x{e_entry:05X}  ' +
                  '  '.join(f'[off 0x{a:X} LS 0x{b:05X} arq 0x{c:X} mem 0x{d:X} fl {e}]' for a, b, c, d, e in segs))
    except struct.error:
        pass
    pos += 4
