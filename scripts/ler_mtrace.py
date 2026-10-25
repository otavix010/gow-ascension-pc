"""Mostra um trecho do [mtrace] (GOWA_MTRACE) com nomes dos métodos NV4097 mais úteis.
uso: python ler_mtrace.py run.log [inicio_regex] [max_linhas]
inicio_regex: começa na 1ª linha que casar (ex.: 'a=0x00A8C000')."""
import re, sys

NOMES = {
    0x0180: 'CTX_DMA_NOTIFY', 0x0184: 'CTX_DMA_A', 0x0188: 'CTX_DMA_B', 0x018C: 'CTX_DMA_COLOR_B',
    0x0194: 'CTX_DMA_COLOR_A', 0x0198: 'CTX_DMA_ZETA', 0x0200: 'SURFACE_CLIP_H', 0x0204: 'SURFACE_CLIP_V',
    0x0208: 'SURFACE_FORMAT', 0x020C: 'SURFACE_PITCH_A', 0x0210: 'SURFACE_COLOR_AOFFSET',
    0x0214: 'SURFACE_ZETA_OFFSET', 0x0218: 'SURFACE_COLOR_BOFFSET', 0x021C: 'SURFACE_PITCH_B',
    0x0220: 'SURFACE_COLOR_TARGET', 0x022C: 'SURFACE_PITCH_Z', 0x0280: 'SURFACE_PITCH_C',
    0x0284: 'SURFACE_PITCH_D', 0x0288: 'SURFACE_COLOR_COFFSET', 0x028C: 'SURFACE_COLOR_DOFFSET',
    0x02B8: 'WINDOW_OFFSET', 0x0304: 'ALPHA_TEST_EN', 0x0308: 'ALPHA_FUNC', 0x030C: 'ALPHA_REF',
    0x0310: 'BLEND_EN', 0x0324: 'COLOR_MASK', 0x0328: 'STENCIL_TEST_EN', 0x0370: 'COLOR_MASK_MRT',
    0x0380: 'DEPTH_BOUNDS_EN', 0x0394: 'CLIP_MIN', 0x0398: 'CLIP_MAX', 0x08C0: 'SCISSOR_H', 0x08C4: 'SCISSOR_V',
    0x08E4: 'SHADER_PROGRAM', 0x0A00: 'VIEWPORT_H', 0x0A04: 'VIEWPORT_V', 0x0A6C: 'DEPTH_FUNC',
    0x0A70: 'DEPTH_MASK', 0x0A74: 'DEPTH_TEST_EN', 0x1714: 'SHADER_WINDOW?', 0x1808: 'BEGIN_END',
    0x1814: 'DRAW_ARRAYS', 0x1824: 'DRAW_INDEX_ARRAY', 0x181C: 'INDEX_ARRAY_ADDR', 0x1820: 'INDEX_ARRAY_DMA',
    0x1830: 'CULL_FACE', 0x1834: 'FRONT_FACE', 0x183C: 'CULL_EN', 0x1D6C: 'SEMAPHORE_OFFSET',
    0x1D70: 'BACK_END_WRITE_SEMA', 0x1D74: 'TEXTURE_READ_SEMA', 0x1D8C: 'ZSTENCIL_CLEAR_VAL',
    0x1D90: 'COLOR_CLEAR_VAL', 0x1D94: 'CLEAR_SURFACE', 0x1D88: 'SHADER_WINDOW', 0x1DAC: 'ZCULL_EN?',
    0x1E94: 'ZCULL?', 0x1FD8: 'TRANSFORM_TIMEOUT', 0x0050: 'NV406E_REFERENCE', 0x0060: 'NV406E_SEMA_OFFSET',
    0x0064: 'NV406E_SEMA_ACQUIRE', 0x0068: 'NV406E_SEMA_RELEASE', 0x006C: 'NV406E_SEMA_RELEASE2',
    0x1680: 'VTX_ATTR0_OFFSET', 0x1740: 'VTX_ATTR0_FORMAT', 0x1A00: 'TEX0_OFFSET', 0x1A20: 'TEX1_OFFSET',
    0x1A40: 'TEX2_OFFSET', 0x1A60: 'TEX3_OFFSET', 0x0A60: 'POLY_OFFSET_FILL?', 0x0A68: 'POLY_OFFSET_SCALE',
    0x0A78: 'POLY_OFFSET_BIAS', 0x1ED4: 'TRANSFORM_PROGRAM_LOAD', 0x1EE0: 'TRANSFORM_PROGRAM_START?',
    0x1D7C: 'ANTI_ALIAS?', 0x1DA4: 'ZPASS_PIXEL_COUNT_EN', 0x1DA8: 'SET_ZCULL_STATS?', 0x1800: 'GET_REPORT?',
    0x1D9C: 'ZCULL_SYNC?', 0x1DB0: 'CLEAR_REPORT?',
}

def main():
    path = sys.argv[1]
    start = re.compile(sys.argv[2]) if len(sys.argv) > 2 else None
    mx = int(sys.argv[3]) if len(sys.argv) > 3 else 200
    on = start is None
    n = 0
    rx = re.compile(r'\[mtrace\] f=(\d+) m=0x([0-9A-F]+) a=0x([0-9A-F]+)')
    for ln in open(path, encoding='utf-8', errors='replace'):
        if '[mtrace]' not in ln:
            continue
        if not on and start.search(ln):
            on = True
        if not on:
            continue
        m = rx.search(ln)
        if m:
            f, meth, a = int(m.group(1)), int(m.group(2), 16), int(m.group(3), 16)
            c = meth & 0x1FFC
            sub = (meth >> 13) & 7
            nome = NOMES.get(c, '')
            print(f'{f} sub{sub} {c:04X} {a:08X} {nome}')
        else:
            print(ln.strip())
        n += 1
        if n >= mx:
            break

main()
