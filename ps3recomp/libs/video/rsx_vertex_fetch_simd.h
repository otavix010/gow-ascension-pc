/* GOWA (21/10): rsx_vertex_fetch_all com SSE -- um float4 por vértice e
 * atributo em vez de componente a componente. Mesmos bits que a versão
 * escalar (rsx_vertex_fetch_all_ref, conferida em
 * out/spudiff/teste_vtx_fetch.c): bswap de float, meia precisão com a regra
 * de rsx_compact_be_f16 (denormal -> zero com sinal, expoente 31 sem tornar o
 * NaN silencioso -- não é o F16C), divisões exatas por 255 e 32767. As cargas
 * leem só os `size` componentes do atributo. Incluído no fim de
 * rsx_vertex_compact.c. GOWA_VTX_SIMD=0 volta à versão escalar. */
#include <immintrin.h>

static inline __m128i fa_ld_bytes(const u8* s, u32 n)   /* n em 1..16, sem ler além */
{
    switch (n) {
    case 16: return _mm_loadu_si128((const __m128i*)s);
    case 12: { u64 a; u32 b; memcpy(&a, s, 8); memcpy(&b, s + 8, 4);
               return _mm_insert_epi32(_mm_cvtsi64_si128((long long)a), (int)b, 2); }
    case 8:  { u64 a; memcpy(&a, s, 8); return _mm_cvtsi64_si128((long long)a); }
    case 6:  { u32 a; u16 b; memcpy(&a, s, 4); memcpy(&b, s + 4, 2);
               return _mm_insert_epi16(_mm_cvtsi32_si128((int)a), b, 2); }
    case 4:  { u32 a; memcpy(&a, s, 4); return _mm_cvtsi32_si128((int)a); }
    case 3:  { u16 a; memcpy(&a, s, 2); return _mm_cvtsi32_si128((int)((u32)a | ((u32)s[2] << 16))); }
    case 2:  { u16 a; memcpy(&a, s, 2); return _mm_cvtsi32_si128((int)a); }
    default: return _mm_cvtsi32_si128((int)s[0]);
    }
}

/* faixas >= size recebem (0, 0, 0, 1) */
static inline __m128 fa_fill(__m128 v, u32 size)
{
    static const u32 m[5][4] = { {0, 0, 0, 0}, {~0u, 0, 0, 0}, {~0u, ~0u, 0, 0}, {~0u, ~0u, ~0u, 0}, {~0u, ~0u, ~0u, ~0u} };
    const __m128 k = _mm_castsi128_ps(_mm_loadu_si128((const __m128i*)m[size]));
    return _mm_or_ps(_mm_and_ps(k, v), _mm_andnot_ps(k, _mm_setr_ps(0.0f, 0.0f, 0.0f, 1.0f)));
}

static inline __m128 fa_half4(__m128i h16)   /* 4 meias (já em ordem do host) nas palavras baixas */
{
    const __m128i h = _mm_cvtepu16_epi32(h16);
    const __m128i sign = _mm_slli_epi32(_mm_srli_epi32(h, 15), 31);
    const __m128i x = _mm_and_si128(h, _mm_set1_epi32(0x7FFF));
    const __m128i e = _mm_srli_epi32(x, 10);
    const __m128i xs = _mm_slli_epi32(x, 13);
    const __m128i nor = _mm_add_epi32(xs, _mm_set1_epi32(112 << 23));
    const __m128i inf = _mm_or_si128(xs, _mm_set1_epi32(0x7F800000));
    __m128i r = _mm_blendv_epi8(nor, inf, _mm_cmpeq_epi32(e, _mm_set1_epi32(31)));
    r = _mm_andnot_si128(_mm_cmpeq_epi32(e, _mm_setzero_si128()), r);
    return _mm_castsi128_ps(_mm_or_si128(r, sign));
}

/* one attribute (GOWA 24/10: split out of rsx_vertex_fetch_all_simd for the per-slot
 * upload, rsx_vertex_fetch_attr_all): float4 per vertex at dst, dst + out_stride, ... */
static int rsx_vertex_fetch_attr_simd(
    const rsx_vertex_fetch_plan* plan, u32 attr, const rsx_vertex_ref* refs, u32 count,
    u8* dst, u32 out_stride)
{
    const __m128i bs32 = _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);
    const __m128i bs16 = _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
    {
        const rsx_vertex_fetch_attr* f = &plan->attr[attr];
        if (!f->desc.type || !f->desc.size) {
            const __m128 d = _mm_loadu_ps(f->default_value);
            for (u32 i = 0; i < count; i++)
                _mm_storeu_ps((float*)(dst + (size_t)i * out_stride), d);
            return 1;
        }
        if (!f->stable_base)
            return 0;
        const u32 freq = f->desc.frequency, mod = (plan->divider_mask >> attr) & 1u;
        const u32 size = f->desc.size < 4 ? f->desc.size : 4;
        const u32 first = f->stable_first, last = f->stable_last, stride = f->stride;
        const u8* base = f->stable_base;
/* GOWA (22/10): the attribute size picked outside the vertex loop (a switch per
 * vertex before: the load width and the (0,0,0,1) fill are now constants), and
 * the ordinary-frequency element computed inline. */
#define FS_LOOP(EXPR) \
        if (freq <= 1) { \
            for (u32 i = 0; i < count; i++) { \
                const u32 e = (refs[i].vertex_id + refs[i].base_index) & 0x000FFFFFu; \
                if (e < first || e > last) return 0; \
                const u8* s = base + (u64)(e - first) * stride; \
                _mm_storeu_ps((float*)(dst + (size_t)i * out_stride), fa_fill((EXPR), SZ)); \
            } \
        } else { \
            for (u32 i = 0; i < count; i++) { \
                const u32 e = rsx_vertex_element_index(refs[i].vertex_id, refs[i].base_index, freq, mod); \
                if (e < first || e > last) return 0; \
                const u8* s = base + (u64)(e - first) * stride; \
                _mm_storeu_ps((float*)(dst + (size_t)i * out_stride), fa_fill((EXPR), SZ)); \
            } \
        }
#define FS_SIZED(EXPR) \
        switch (size) { \
        case 1: { enum { SZ = 1 }; FS_LOOP(EXPR) break; } \
        case 2: { enum { SZ = 2 }; FS_LOOP(EXPR) break; } \
        case 3: { enum { SZ = 3 }; FS_LOOP(EXPR) break; } \
        default: { enum { SZ = 4 }; FS_LOOP(EXPR) break; } \
        }
        switch (f->desc.type) {
        case RSX_VTX_TYPE_FLOAT:
            FS_SIZED(_mm_castsi128_ps(_mm_shuffle_epi8(fa_ld_bytes(s, SZ * 4), bs32)))
            break;
        case RSX_VTX_TYPE_HALF:
            FS_SIZED(fa_half4(_mm_shuffle_epi8(fa_ld_bytes(s, SZ * 2), bs16)))
            break;
        case RSX_VTX_TYPE_UNORM8:
            FS_SIZED(_mm_div_ps(_mm_cvtepi32_ps(_mm_cvtepu8_epi32(fa_ld_bytes(s, SZ))), _mm_set1_ps(255.0f)))
            break;
        case RSX_VTX_TYPE_UINT8:
            FS_SIZED(_mm_cvtepi32_ps(_mm_cvtepu8_epi32(fa_ld_bytes(s, SZ))))
            break;
        case RSX_VTX_TYPE_SNORM16:
            FS_SIZED(_mm_div_ps(_mm_cvtepi32_ps(_mm_cvtepi16_epi32(_mm_shuffle_epi8(fa_ld_bytes(s, SZ * 2), bs16))), _mm_set1_ps(32767.0f)))
            break;
        case RSX_VTX_TYPE_SINT16:
            FS_SIZED(_mm_cvtepi32_ps(_mm_cvtepi16_epi32(_mm_shuffle_epi8(fa_ld_bytes(s, SZ * 2), bs16))))
            break;
        case RSX_VTX_TYPE_CMP32:   /* raro: o caminho escalar, para este atributo */
            for (u32 i = 0; i < count; i++) {
                const u32 e = rsx_vertex_element_index(refs[i].vertex_id, refs[i].base_index, freq, mod);
                if (e < first || e > last) return 0;
                const u8* s = base + (u64)(e - first) * stride;
                float* v = (float*)(dst + (size_t)i * out_stride);
                u32 w; memcpy(&w, s, 4); w = __builtin_bswap32(w);
                s32 x = (s32)(w & 0x7FF), y = (s32)((w >> 11) & 0x7FF), z = (s32)((w >> 22) & 0x3FF);
                if (x & 0x400) x -= 0x800;
                if (y & 0x400) y -= 0x800;
                if (z & 0x200) z -= 0x400;
                v[0] = (float)x / 1023.0f; v[1] = (float)y / 1023.0f; v[2] = (float)z / 511.0f; v[3] = 1.0f;
            }
            break;
        default:
            return 0;
        }
#undef FS_SIZED
#undef FS_LOOP
    }
    return 1;
}

static int rsx_vertex_fetch_all_simd(
    const rsx_vertex_fetch_plan* plan, const rsx_vertex_ref* refs, u32 count,
    u8* out, u32 out_stride)
{
    if (!plan || !refs || !out || !out_stride || plan->inline_data)
        return 0;
    for (u32 slot = 0; slot < plan->layout.count; slot++)
        if (!rsx_vertex_fetch_attr_simd(plan, plan->layout.attrs[slot], refs, count, out + slot * 16u, out_stride))
            return 0;
    return 1;
}

int rsx_vertex_fetch_attr_all(
    const rsx_vertex_fetch_plan* plan, u32 attr, const rsx_vertex_ref* refs, u32 count,
    u8* out, u32 out_stride)
{
    if (!plan || !refs || !out || !out_stride || plan->inline_data || attr >= RSX_DSP_NUM_VERTEX_ATTR)
        return 0;
    return rsx_vertex_fetch_attr_simd(plan, attr, refs, count, out, out_stride);
}

int rsx_vertex_fetch_all(
    const rsx_vertex_fetch_plan* plan, const rsx_vertex_ref* refs, u32 count,
    u8* out, u32 out_stride)
{
    static int simd = -1;
    if (simd < 0) { const char* e = getenv("GOWA_VTX_SIMD"); simd = !(e && *e == '0'); }
    return simd ? rsx_vertex_fetch_all_simd(plan, refs, count, out, out_stride)
                : rsx_vertex_fetch_all_ref(plan, refs, count, out, out_stride);
}
