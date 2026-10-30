/* GOWA (30/10, EDGE nativo, marco E3): o laço A950 da função 0xA880 do job 0x72DF80 escrito para o PC.
 *
 * Normais a partir de dois fluxos de vetores (a em r81, b em r82; 8 por volta, sem software pipeline entre voltas):
 *     c  = b * a.yzxx - a * b.yzxx            (fm_r, fnms)          -> n = (c.y, c.z, c.x, 0) * b.w   (fm)
 *     d  = (n.x² + n.y²) + n.z²               (fm, fa_r, fa)
 *     se d > 0x0DA2425F: y = fi(d, frsqest(d)); y += (1 - d*y*y) * (y * 0,5)   (fm_r, fnms, fm_r, fma) -> n * y (fm)
 *     senão: n                                                   (fcgt, selb); w = 0 na saída
 * Contas: AVX-512 (RZ embutido, 4 vetores por registro) ou, na build portátil, AVX2 com o MXCSR em RZ (SPU_SX_RZ);
 * frsqest/fi pela mesma tabela e aritmética inteira do runtime (como no plugin 0x752710). Um bloco com algum valor
 * fora do alcance do float do PC (|r| >= 0x7F7FFFFF; o host saturou ou havia o expoente 255, que na SPU é número)
 * é refeito pelas primitivas do runtime na ordem do original. Temporários r2-r79 (e r80, que o epílogo restaura)
 * ficam diferentes; GOWA_CONF_REGS_ABI=1. GOWA_EDGE_NATIVO=1 liga. Gancho: antes do vk do laço, saltando para depois
 * dele (scripts_edge/edge_nativo.py). */
#ifndef GOWA_FN_0000A880_H
#define GOWA_FN_0000A880_H
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>

#ifndef GOWA_EDGE_NATIVO_ON_DEF
#define GOWA_EDGE_NATIVO_ON_DEF
static int gowa_edge_nativo_on(void)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_EDGE_NATIVO"); on = e && *e == '1'; }
    return on;
}
#endif

static inline u128 a880_k(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { u128 r; r._u32[0] = a; r._u32[1] = b; r._u32[2] = c; r._u32[3] = d; return r; }

/* um vetor pelas primitivas do runtime, como o original; k = posição na volta (as formas cruas variam com ela) */
static inline u128 a880_exato(u128 a, u128 b, int k)
{
    const u128 c47 = a880_k(0x04050607u, 0x08090A0Bu, 0x00010203u, 0x00010203u);
    const u128 c39 = a880_k(0x00010203u, 0x00010203u, 0x00010203u, 0x00010203u);
    const u128 c67 = a880_k(0x04050607u, 0x08090A0Bu, 0x0C0D0E0Fu, 0x10111213u);
    const u128 c38 = a880_k(0x08090A0Bu, 0x0C0D0E0Fu, 0x10111213u, 0x14151617u);
    const u128 m18 = a880_k(0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0);
    const u128 eps = a880_k(0x0DA2425Fu, 0x0DA2425Fu, 0x0DA2425Fu, 0x0DA2425Fu);
    const u128 um = a880_k(0x3F800000u, 0x3F800000u, 0x3F800000u, 0x3F800000u);
    const u128 meio = a880_k(0x3F000000u, 0x3F000000u, 0x3F000000u, 0x3F000000u);
    static const int fnms_cru[8] = { 1, 0, 1, 1, 0, 1, 0, 0 }, h_cru[8] = { 1, 1, 1, 1, 1, 1, 0, 0 };
    const u128 p = spu_fm_r(b, spu_shufb(a, a, c47));
    const u128 c = spu_fnms(a, spu_shufb(b, b, c47), p);
    const u128 bw = spu_rotqbyi(b, 12);
    const u128 n = spu_fm(spu_shufb(bw, bw, c39), spu_and(spu_shufb(c, c, c47), m18));
    const u128 s = spu_fm(n, n);
    const u128 dd = spu_fa(spu_fa_r(s, spu_shufb(s, s, c67)), spu_shufb(s, s, c38));
    const u128 d = spu_shufb(dd, dd, c39);
    const u128 gt = spu_fcgt(d, eps);
    const u128 y = spu_fi(d, spu_frsqest(d));
    const u128 t = spu_fm_r(y, y);
    const u128 u = fnms_cru[k] ? spu_fnms_r(d, t, um) : spu_fnms(d, t, um);
    const u128 h = h_cru[k] ? spu_fm_r(y, meio) : spu_fm(y, meio);
    const u128 o = spu_fm(n, spu_fma(u, h, y));
    return spu_and(spu_selb(n, spu_and(gt, o), gt), m18);
}

static const uint32_t a880_lut[64] = {   /* frsqest (a mesma do runtime e do pl_00752710.h) */
    0x350160, 0x34E954, 0x2F993D, 0x2F993D, 0x2AA523, 0x2AA523, 0x26190D, 0x26190D,
    0x21E4F9, 0x21E4F9, 0x1E00E9, 0x1E00E9, 0x1A5CD9, 0x1A5CD9, 0x16F8CB, 0x16F8CB,
    0x13CCC0, 0x13CCC0, 0x10CCB3, 0x10CCB3, 0x0E00AA, 0x0E00AA, 0x0B58A1, 0x0B58A1,
    0x08D498, 0x08D498, 0x067491, 0x067491, 0x043089, 0x043089, 0x020C83, 0x020C83,
    0x7FFDF4, 0x7FD1DE, 0x7859C8, 0x783DBA, 0x71559C, 0x71559C, 0x6AE57C, 0x6AE57C,
    0x64F561, 0x64F561, 0x5F7149, 0x5F7149, 0x5A4D33, 0x5A4D33, 0x55811F, 0x55811F,
    0x51050F, 0x51050F, 0x4CC8FE, 0x4CC8FE, 0x48D0F0, 0x48D0F0, 0x4510E4, 0x4510E4,
    0x4180D7, 0x4180D7, 0x3E24CC, 0x3E24CC, 0x3AF4C3, 0x3AF4C3, 0x37E8BA, 0x37E8BA};

#if defined(__AVX512F__) && defined(__AVX512BW__) && defined(__AVX512VL__)
#define A880_VETOR 1
#define A880_RZ (_MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)
static inline __m512 a880_z(__m512 x)
{
    const __m512i i = _mm512_castps_si512(x);
    return _mm512_castsi512_ps(_mm512_maskz_mov_epi32(_mm512_test_epi32_mask(i, _mm512_set1_epi32(0x7F800000)), i));
}
static inline __mmask16 a880_ruim(__m512 r)
{
    return _mm512_cmpge_epu32_mask(_mm512_and_si512(_mm512_castps_si512(r), _mm512_set1_epi32(0x7FFFFFFF)), _mm512_set1_epi32(0x7F7FFFFF));
}
static inline __m512i a880_bswap(__m512i v)
{
    return _mm512_shuffle_epi8(v, _mm512_broadcast_i32x4(_mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12)));
}
/* frsqest + fi (inteiros, como o runtime) para d positivo normal */
static inline __m512 a880_rsq(__m512 d)
{
    const __m512i ui = _mm512_castps_si512(d);
    const __m512i e = _mm512_and_si512(_mm512_srli_epi32(ui, 23), _mm512_set1_epi32(0xFF));
    const __m512i idx = _mm512_and_si512(_mm512_srli_epi32(ui, 18), _mm512_set1_epi32(0x3F));
    const __m512i lo = _mm512_permutex2var_epi32(_mm512_loadu_si512(&a880_lut[0]), idx, _mm512_loadu_si512(&a880_lut[16]));
    const __m512i hi = _mm512_permutex2var_epi32(_mm512_loadu_si512(&a880_lut[32]), idx, _mm512_loadu_si512(&a880_lut[48]));
    const __m512i fr = _mm512_mask_blend_epi32(_mm512_test_epi32_mask(idx, _mm512_set1_epi32(32)), lo, hi);
    const __m512i rexp = _mm512_mask_blend_epi32(_mm512_cmpeq_epi32_mask(e, _mm512_setzero_si512()),
                                                 _mm512_sub_epi32(_mm512_set1_epi32(190), _mm512_srli_epi32(_mm512_add_epi32(e, _mm512_set1_epi32(1)), 1)),
                                                 _mm512_set1_epi32(0xFF));
    const __m512i est = _mm512_or_si512(_mm512_slli_epi32(rexp, 23), _mm512_and_si512(fr, _mm512_set1_epi32(0x7FFFFF)));
    const __m512i fe = _mm512_and_si512(_mm512_srli_epi32(est, 23), _mm512_set1_epi32(0xFF));
    const __m512i base = _mm512_or_si512(_mm512_and_si512(_mm512_srli_epi32(est, 10), _mm512_set1_epi32(0x1FFF)), _mm512_set1_epi32(0x2000));
    const __m512i prod = _mm512_mullo_epi32(_mm512_and_si512(est, _mm512_set1_epi32(0x3FF)), _mm512_and_si512(ui, _mm512_set1_epi32(0x7FFFF)));
    const __m512i m8 = _mm512_sub_epi32(_mm512_slli_epi32(base, 11), _mm512_srli_epi32(_mm512_add_epi32(prod, _mm512_set1_epi32(255)), 8));
    const __mmask16 baixo = _mm512_cmplt_epi32_mask(m8, _mm512_set1_epi32(1 << 24));
    const __m512i frac = _mm512_and_si512(_mm512_mask_blend_epi32(baixo, _mm512_srli_epi32(m8, 1), m8), _mm512_set1_epi32(0x7FFFFF));
    const __m512i e2 = _mm512_mask_sub_epi32(fe, baixo, fe, _mm512_set1_epi32(1));
    __m512i fiv = _mm512_or_si512(_mm512_and_si512(est, _mm512_set1_epi32((int)0x80000000u)), _mm512_or_si512(_mm512_slli_epi32(e2, 23), frac));
    fiv = _mm512_maskz_mov_epi32(~(_mm512_cmpeq_epi32_mask(fe, _mm512_setzero_si512()) | _mm512_cmpeq_epi32_mask(e2, _mm512_setzero_si512())), fiv);
    return _mm512_castsi512_ps(fiv);
}
/* 4 vetores (64 bytes de cada fluxo); devolve a máscara de faixas ruins (0 = tudo certo, saída gravada em o) */
static inline __mmask16 a880_quatro(const uint8_t* pa, const uint8_t* pb, uint8_t* o)
{
    const __m512 a = a880_z(_mm512_castsi512_ps(a880_bswap(_mm512_loadu_si512(pa))));
    const __m512 b = a880_z(_mm512_castsi512_ps(a880_bswap(_mm512_loadu_si512(pb))));
    const __m512 a1 = _mm512_permute_ps(a, _MM_SHUFFLE(0, 0, 2, 1)), b1 = _mm512_permute_ps(b, _MM_SHUFFLE(0, 0, 2, 1));
    __m512 p = _mm512_mul_round_ps(b, a1, A880_RZ);
    __mmask16 ruim = a880_ruim(p);
    const __m512 c = a880_z(_mm512_fnmadd_round_ps(a, b1, a880_z(p), A880_RZ));
    ruim |= a880_ruim(c);
    const __m512 c1 = _mm512_castsi512_ps(_mm512_maskz_mov_epi32(0x7777, _mm512_castps_si512(_mm512_permute_ps(c, _MM_SHUFFLE(0, 0, 2, 1)))));
    const __m512 bw = _mm512_permute_ps(b, _MM_SHUFFLE(3, 3, 3, 3));
    const __m512 n = a880_z(_mm512_mul_round_ps(bw, c1, A880_RZ));
    ruim |= a880_ruim(n);
    const __m512 s = a880_z(_mm512_mul_round_ps(n, n, A880_RZ));
    ruim |= a880_ruim(s);
    const __m512 q = _mm512_add_round_ps(s, _mm512_permute_ps(s, _MM_SHUFFLE(0, 3, 2, 1)), A880_RZ);
    ruim |= a880_ruim(q);
    const __m512 dd = a880_z(_mm512_add_round_ps(a880_z(q), _mm512_permute_ps(s, _MM_SHUFFLE(1, 0, 3, 2)), A880_RZ));
    ruim |= a880_ruim(dd);
    const __m512 d = _mm512_permute_ps(dd, _MM_SHUFFLE(0, 0, 0, 0));
    /* d > épsilon (os dois positivos e no alcance: a comparação de inteiros serve) */
    const __mmask16 gt = _mm512_cmpgt_epi32_mask(_mm512_castps_si512(d), _mm512_set1_epi32(0x0DA2425F));
    const __m512 y = a880_rsq(d);
    const __m512 t = _mm512_mul_round_ps(y, y, A880_RZ);
    __mmask16 r2 = a880_ruim(t);
    const __m512 u = a880_z(_mm512_fnmadd_round_ps(d, a880_z(t), _mm512_set1_ps(1.0f), A880_RZ));
    r2 |= a880_ruim(u);
    const __m512 h = a880_z(_mm512_mul_round_ps(a880_z(y), _mm512_set1_ps(0.5f), A880_RZ));
    const __m512 y2 = a880_z(_mm512_fmadd_round_ps(u, h, a880_z(y), A880_RZ));
    r2 |= a880_ruim(y2) | a880_ruim(y);
    const __m512 out = a880_z(_mm512_mul_round_ps(n, y2, A880_RZ));
    r2 |= a880_ruim(out);
    ruim |= r2 & gt;   /* com d <= épsilon a saída é n: o resto não conta */
    const __m512 r = _mm512_castsi512_ps(_mm512_maskz_mov_epi32(0x7777, _mm512_castps_si512(_mm512_mask_blend_ps(gt, n, out))));
    if (!ruim) _mm512_storeu_si512(o, a880_bswap(_mm512_castps_si512(r)));
    return ruim;
}
#else
#define A880_VETOR 0
#endif

/* GOWA_A880_STATS=1: chamadas e vetores refeitos pelo caminho exato */
static int fn_0000A880_nativo(spu_context* ctx)
{
    if (!A880_VETOR || !gowa_edge_nativo_on()) return 0;   /* sem AVX-512 fica o vk (só o caminho exato perderia dele) */
    u128* r = ctx->gpr;
    const int32_t cont = (int32_t)r[83]._u32[0];
    if (cont <= 0 || cont > 0x4000 || r[52]._u32[0] != 0u || r[51]._u32[0] != 0u) return 0;
    if (r[47]._u32[0] != 0x04050607u || r[47]._u32[1] != 0x08090A0Bu || r[47]._u32[2] != 0x00010203u || r[47]._u32[3] != 0x00010203u)
        return 0;
    const uint32_t g = ((uint32_t)cont + 7u) / 8u, pa = r[49]._u32[0], pb = r[50]._u32[0], po = r[48]._u32[0], tam = 128u * g;
    if (pa != r[81]._u32[0] || pb != r[82]._u32[0] || po != r[53]._u32[0]) return 0;
    if ((pa | pb | po) & 15u || pa + tam > SPU_LS_SIZE || pb + tam > SPU_LS_SIZE || po + tam > SPU_LS_SIZE) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    long refeitos = 0;
    for (uint32_t j = 0; j < g; j++) {
        /* lê os dois blocos do grupo antes de gravar (a saída pode ser um dos fluxos) */
        uint8_t ea[128], eb[128], o[128];
        memcpy(ea, ls + pa + 128u * j, 128);
        memcpy(eb, ls + pb + 128u * j, 128);
        int bom[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
#if A880_VETOR == 1
        for (int q = 0; q < 2; q++) {
            const __mmask16 m = a880_quatro(ea + 64 * q, eb + 64 * q, o + 64 * q);
            for (int v = 0; v < 4; v++) bom[4 * q + v] = m == 0;
        }
#endif
        for (int v = 0; v < 8; v++) {
            if (bom[v]) continue;
            u128 a, b;
            uint32_t w[4];
            for (int i = 0; i < 4; i++) { memcpy(&w[i], ea + 16 * v + 4 * i, 4); a._u32[i] = __builtin_bswap32(w[i]); }
            for (int i = 0; i < 4; i++) { memcpy(&w[i], eb + 16 * v + 4 * i, 4); b._u32[i] = __builtin_bswap32(w[i]); }
            const u128 x = a880_exato(a, b, v);
            for (int i = 0; i < 4; i++) { const uint32_t be = __builtin_bswap32(x._u32[i]); memcpy(o + 16 * v + 4 * i, &be, 4); }
            refeitos++;
        }
        memcpy(ls + po + 128u * j, o, 128);
    }
    static int est = -1; static volatile long nch, nref;
    if (est < 0) est = getenv("GOWA_A880_STATS") ? 1 : 0;
    if (est) { const long n = ++nch; nref += refeitos; if ((n & (n - 1)) == 0) fprintf(stderr, "[a880] %ld chamadas, %ld vetores refeitos\n", n, nref); }
    return 1;
}
#endif
