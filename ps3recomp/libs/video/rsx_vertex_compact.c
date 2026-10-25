#include "rsx_vertex_compact.h"
#include "rsx_commands.h"
#include "rsx_vertex_formats.h"

#include <limits.h>
#include <immintrin.h>   /* GOWA: AVX2 in rsx_vertex_remap_build */
#include <stdlib.h>
#include <string.h>

static float rsx_compact_be_f32(const u8* p)
{
    const u32 w = ((u32)p[0] << 24) | ((u32)p[1] << 16) |
                  ((u32)p[2] << 8) | (u32)p[3];
    float f;
    memcpy(&f, &w, sizeof(f));
    return f;
}

static float rsx_compact_be_f16(const u8* p)
{
    const u16 h = (u16)((p[0] << 8) | p[1]);
    const u32 sign = (u32)(h >> 15) << 31;
    const u32 exp = (h >> 10) & 0x1F;
    const u32 man = h & 0x3FF;
    u32 out;
    if (exp == 0)
        out = sign;
    else if (exp == 31)
        out = sign | 0x7F800000u | (man << 13);
    else
        out = sign | ((exp + 112) << 23) | (man << 13);
    float f;
    memcpy(&f, &out, sizeof(f));
    return f;
}

int rsx_vertex_decode_element(
    u32 type, u32 size, const u8* source, float out[4])
{
    if (!source || !out)
        return 0;
    out[0] = out[1] = out[2] = 0.0f;
    out[3] = 1.0f;
    for (u32 component = 0; component < size && component < 4;
         component++) {
        switch (type) {
        case RSX_VTX_TYPE_FLOAT:
            out[component] = rsx_compact_be_f32(source + component * 4);
            break;
        case RSX_VTX_TYPE_HALF:
            out[component] = rsx_compact_be_f16(source + component * 2);
            break;
        case RSX_VTX_TYPE_UNORM8:
            out[component] = source[component] / 255.0f;
            break;
        case RSX_VTX_TYPE_UINT8:
            out[component] = (float)source[component];
            break;
        case RSX_VTX_TYPE_SNORM16: {
            const s16 value = (s16)(
                (source[component * 2] << 8) | source[component * 2 + 1]);
            out[component] = value / 32767.0f;
            break;
        }
        case RSX_VTX_TYPE_SINT16: {
            const s16 value = (s16)(
                (source[component * 2] << 8) | source[component * 2 + 1]);
            out[component] = (float)value;
            break;
        }
        case RSX_VTX_TYPE_CMP32: {
            const u32 word =
                ((u32)source[0] << 24) | ((u32)source[1] << 16) |
                ((u32)source[2] << 8) | (u32)source[3];
            s32 x = (s32)(word & 0x7FF);
            s32 y = (s32)((word >> 11) & 0x7FF);
            s32 z = (s32)((word >> 22) & 0x3FF);
            if (x & 0x400) x -= 0x800;
            if (y & 0x400) y -= 0x800;
            if (z & 0x200) z -= 0x400;
            out[0] = (float)x / 1023.0f;
            out[1] = (float)y / 1023.0f;
            out[2] = (float)z / 511.0f;
            out[3] = 1.0f;
            return 1;
        }
        default:
            return 0;
        }
    }
    return 1;
}

void rsx_vertex_layout_plan_init(
    rsx_vertex_layout_plan* plan, u32 input_mask)
{
    if (!plan)
        return;
    memset(plan, 0, sizeof(*plan));
    plan->mask = input_mask & 0xFFFFu;
    for (u32 attr = 0; attr < RSX_DSP_NUM_VERTEX_ATTR; attr++)
        plan->slot_by_attr[attr] = -1;
    for (u32 attr = 0; attr < RSX_DSP_NUM_VERTEX_ATTR; attr++) {
        if (!(plan->mask & (1u << attr)))
            continue;
        const u32 slot = plan->count++;
        plan->attrs[slot] = (u8)attr;
        plan->slot_by_attr[attr] = (s8)slot;
    }
    plan->stride = (u32)plan->count * 16u;
}

void rsx_vertex_fetch_plan_init(
    rsx_vertex_fetch_plan* plan, const rsx_dispatch* rsx,
    const rsx_vertex_layout_plan* layout,
    rsx_vertex_guest_ptr_fn guest_ptr, void* guest_user)
{
    if (!plan || !rsx || !layout)
        return;
    memset(plan, 0, sizeof(*plan));
    plan->layout = *layout;
    plan->base_offset = rsx_dsp_vertex_data_base_offset(rsx);
    plan->divider_mask = rsx_dsp_reg(rsx, 0x1FC0);
    plan->guest_ptr = guest_ptr;
    plan->guest_user = guest_user;
    for (u32 attr = 0; attr < RSX_DSP_NUM_VERTEX_ATTR; attr++) {
        rsx_vertex_fetch_attr* fetch = &plan->attr[attr];
        rsx_dsp_get_vertex_attr(rsx, attr, &fetch->desc);
        fetch->elem_size =
            rsx_vertex_attrib_size(fetch->desc.type, fetch->desc.size);
        fetch->stride =
            fetch->desc.stride ? fetch->desc.stride : fetch->elem_size;
        rsx_dsp_vertex_default(rsx, attr, fetch->default_value);
        if (attr == 3 &&
            fetch->default_value[0] == 0.0f &&
            fetch->default_value[1] == 0.0f &&
            fetch->default_value[2] == 0.0f &&
            fetch->default_value[3] == 1.0f) {
            fetch->default_value[0] = 1.0f;
            fetch->default_value[1] = 1.0f;
            fetch->default_value[2] = 1.0f;
        }
    }
}

u32 rsx_vertex_inline_layout(
    const rsx_dispatch* rsx, u32 offsets[RSX_DSP_NUM_VERTEX_ATTR])
{
    u32 packed = 0, declared = 0;
    for (u32 attr = 0; attr < RSX_DSP_NUM_VERTEX_ATTR; attr++) {
        rsx_dsp_vertex_attr desc;
        rsx_dsp_get_vertex_attr(rsx, attr, &desc);
        if (offsets)
            offsets[attr] = packed;
        if (!desc.type || !desc.size)
            continue;
        packed += rsx_vertex_attrib_size(desc.type, desc.size);
        if (desc.stride > declared)
            declared = desc.stride;
    }
    return declared ? declared : packed;
}

void rsx_vertex_fetch_plan_set_inline(
    rsx_vertex_fetch_plan* plan, const rsx_dispatch* rsx,
    const u8* data, u32 bytes)
{
    if (!plan)
        return;
    plan->inline_data = NULL;
    plan->inline_bytes = 0;
    if (!data || !bytes || !rsx)
        return;
    plan->inline_stride = rsx_vertex_inline_layout(rsx, plan->inline_off);
    plan->inline_data = data;
    plan->inline_bytes = bytes;
}

void rsx_vertex_fetch_plan_prepare(
    rsx_vertex_fetch_plan* plan, const rsx_vertex_ref* refs, u32 count)
{
    if (!plan)
        return;
    int norm_ok = 0;
    u32 norm_first = 0, norm_last = 0;
    for (u32 slot = 0; slot < plan->layout.count; slot++) {
        const u32 attr = plan->layout.attrs[slot];
        rsx_vertex_fetch_attr* fetch = &plan->attr[attr];
        fetch->stable_base = NULL;
        fetch->stable_first = 0;
        fetch->stable_last = 0;
        if (!refs || !count ||
            !fetch->desc.type || !fetch->desc.size || !fetch->elem_size)
            continue;

        /* INLINE_ARRAY: the stream is already one contiguous, correctly
         * strided array per attribute, which is exactly what stable_base
         * describes -- so it needs no resolver and no bounds fallback, only
         * the right base. */
        if (plan->inline_data) {
            const u32 stride =
                plan->inline_stride ? plan->inline_stride : fetch->stride;
            const u32 off = plan->inline_off[attr];
            if (!stride || off + fetch->elem_size > stride)
                continue;
            const u32 verts = plan->inline_bytes / stride;
            if (!verts)
                continue;
            fetch->stride = stride;
            fetch->stable_base = plan->inline_data + off;
            fetch->stable_first = 0;
            fetch->stable_last = verts - 1;
            continue;
        }

        if (!plan->guest_ptr)
            continue;

        u32 first = UINT_MAX;
        u32 last = 0;
        /* GOWA (21/10): an ordinary-frequency element is the same for every
         * attribute ((vertex_id + base_index) & 0xFFFFF) -- its range is
         * computed once per draw instead of once per attribute */
        if (fetch->desc.frequency <= 1 && norm_ok) {
            first = norm_first;
            last = norm_last;
        } else {
            for (u32 i = 0; i < count; i++) {
                const u32 element = rsx_vertex_element_index(
                    refs[i].vertex_id, refs[i].base_index,
                    fetch->desc.frequency,
                    (plan->divider_mask >> attr) & 1u);
                if (element < first) first = element;
                if (element > last) last = element;
            }
            if (fetch->desc.frequency <= 1) {
                norm_ok = 1;
                norm_first = first;
                norm_last = last;
            }
        }

        const u64 prefix =
            (u64)plan->base_offset + (u64)fetch->desc.offset;
        const u64 start = prefix + (u64)first * fetch->stride;
        const u64 end =
            prefix + (u64)last * fetch->stride + fetch->elem_size;
        if (start > UINT_MAX || end > 0x100000000ull || end <= start ||
            end - start > UINT_MAX)
            continue;

        const u8* base = plan->guest_ptr(
            plan->guest_user, fetch->desc.location, (u32)start,
            (u32)(end - start));
        if (!base)
            continue;
        fetch->stable_base = base;
        fetch->stable_first = first;
        fetch->stable_last = last;
    }
}

/* GOWA: every vertex of a draw, attribute by attribute: one type-specialised
 * loop per attribute instead of rsx_vertex_fetch_one's per-vertex walk over
 * the attributes with a type switch per component. Only through the stable
 * bases rsx_vertex_fetch_plan_prepare set up; returns 0 (nothing usable
 * written) whenever an attribute needs the general path -- no stable base, an
 * element outside it, an unknown type -- and the caller then runs
 * rsx_vertex_fetch_one per vertex as before. Same values as fetch_one: the
 * components it does not cover keep (0, 0, 0, 1). */
static inline float rsx_fa_be_f32(const u8* p) { u32 v; memcpy(&v, p, 4); v = __builtin_bswap32(v); float f; memcpy(&f, &v, 4); return f; }
static inline s16 rsx_fa_be_s16(const u8* p) { return (s16)((p[0] << 8) | p[1]); }
static int rsx_vertex_fetch_all_ref(
    const rsx_vertex_fetch_plan* plan, const rsx_vertex_ref* refs, u32 count,
    u8* out, u32 out_stride)
{
    if (!plan || !refs || !out || !out_stride || plan->inline_data)
        return 0;
    for (u32 slot = 0; slot < plan->layout.count; slot++) {
        const u32 attr = plan->layout.attrs[slot];
        const rsx_vertex_fetch_attr* f = &plan->attr[attr];
        u8* dst = out + slot * 16u;
        if (!f->desc.type || !f->desc.size) {
            for (u32 i = 0; i < count; i++)
                memcpy(dst + (size_t)i * out_stride, f->default_value, 16);
            continue;
        }
        if (!f->stable_base)
            return 0;
        const u32 freq = f->desc.frequency, mod = (plan->divider_mask >> attr) & 1u;
        const u32 size = f->desc.size < 4 ? f->desc.size : 4;
        const u32 first = f->stable_first, last = f->stable_last, stride = f->stride;
        const u8* base = f->stable_base;
#define FA_LOOP(...) \
        for (u32 i = 0; i < count; i++) { \
            const u32 e = rsx_vertex_element_index(refs[i].vertex_id, refs[i].base_index, freq, mod); \
            if (e < first || e > last) return 0; \
            const u8* s = base + (u64)(e - first) * stride; \
            float* v = (float*)(dst + (size_t)i * out_stride); \
            v[0] = v[1] = v[2] = 0.0f; v[3] = 1.0f; \
            __VA_ARGS__ \
        }
        switch (f->desc.type) {
        case RSX_VTX_TYPE_FLOAT:
            FA_LOOP(for (u32 c = 0; c < size; c++) v[c] = rsx_fa_be_f32(s + c * 4);)
            break;
        case RSX_VTX_TYPE_HALF:
            FA_LOOP(for (u32 c = 0; c < size; c++) v[c] = rsx_compact_be_f16(s + c * 2);)
            break;
        case RSX_VTX_TYPE_UNORM8:
            FA_LOOP(for (u32 c = 0; c < size; c++) v[c] = s[c] / 255.0f;)
            break;
        case RSX_VTX_TYPE_UINT8:
            FA_LOOP(for (u32 c = 0; c < size; c++) v[c] = (float)s[c];)
            break;
        case RSX_VTX_TYPE_SNORM16:
            FA_LOOP(for (u32 c = 0; c < size; c++) v[c] = rsx_fa_be_s16(s + c * 2) / 32767.0f;)
            break;
        case RSX_VTX_TYPE_SINT16:
            FA_LOOP(for (u32 c = 0; c < size; c++) v[c] = (float)rsx_fa_be_s16(s + c * 2);)
            break;
        case RSX_VTX_TYPE_CMP32:   /* 11:11:10 signed, as rsx_vertex_decode_element */
            FA_LOOP({
                u32 w; memcpy(&w, s, 4); w = __builtin_bswap32(w);
                s32 x = (s32)(w & 0x7FF), y = (s32)((w >> 11) & 0x7FF), z = (s32)((w >> 22) & 0x3FF);
                if (x & 0x400) x -= 0x800;
                if (y & 0x400) y -= 0x800;
                if (z & 0x200) z -= 0x400;
                v[0] = (float)x / 1023.0f; v[1] = (float)y / 1023.0f; v[2] = (float)z / 511.0f; v[3] = 1.0f;
            })
            break;
        default:
            return 0;
        }
#undef FA_LOOP
    }
    return 1;
}

int rsx_vertex_fetch_one(
    const rsx_vertex_fetch_plan* plan, const rsx_vertex_ref* ref, u8* out)
{
    if (!plan || !ref || (!out && plan->layout.stride))
        return 0;
    for (u32 slot = 0; slot < plan->layout.count; slot++) {
        const u32 attr = plan->layout.attrs[slot];
        const rsx_vertex_fetch_attr* fetch = &plan->attr[attr];
        float* value = (float*)(out + slot * 16u);
        if (!fetch->desc.type || !fetch->desc.size) {
            memcpy(value, fetch->default_value, 16);
            continue;
        }

        const u32 element = rsx_vertex_element_index(
            ref->vertex_id, ref->base_index, fetch->desc.frequency,
            (plan->divider_mask >> attr) & 1u);
        const u8* source = NULL;
        if (fetch->stable_base &&
            element >= fetch->stable_first && element <= fetch->stable_last) {
            source = fetch->stable_base +
                (u64)(element - fetch->stable_first) * fetch->stride;
        } else if (plan->inline_data) {
            /* An inline draw has no vertex array to fall back to: the array
             * offsets still hold whatever the last ordinary draw bound, and
             * reading them would feed this draw another mesh's vertices.
             * An attribute the stream does not supply gets its default,
             * which is what the hardware feeds. */
            source = NULL;
        } else if (plan->guest_ptr) {
            const u32 offset =
                plan->base_offset + fetch->desc.offset +
                element * fetch->stride;
            source = plan->guest_ptr(
                plan->guest_user, fetch->desc.location, offset,
                fetch->elem_size);
        }

        if (source && rsx_vertex_decode_element(
                fetch->desc.type, fetch->desc.size, source, value))
            continue;
        if (attr == 0)
            return 0;
        memcpy(value, fetch->default_value, 16);
    }
    return 1;
}

static u64 rsx_vertex_remap_hash(u64 value)
{
    value ^= value >> 33;
    value *= 0xff51afd7ed558ccdull;
    value ^= value >> 33;
    value *= 0xc4ceb9fe1a85ec53ull;
    value ^= value >> 33;
    return value;
}

/* GOWA (24/10): 1 when the last rsx_vertex_remap_build took the dense path -- the
 * unique references are then exactly lo..hi with one base index, in order */
int g_rsx_remap_dense;

int rsx_vertex_remap_build(
    rsx_vertex_remap* remap, rsx_vertex_ref* refs, u32 count,
    u32* unique_count)
{
    g_rsx_remap_dense = 0;
    if (!remap || (!refs && count) || !unique_count)
        return 0;
    *unique_count = count;
    if (!count)
        return 1;

    if (count > remap->occurrence_capacity) {
        u32* occurrences = (u32*)realloc(
            remap->occurrence_to_unique, (size_t)count * sizeof(u32));
        if (!occurrences)
            return 0;
        remap->occurrence_to_unique = occurrences;
        remap->occurrence_capacity = count;
    }

    /* GOWA: a dense index range needs no hashing. When every reference shares
     * one base index and max - min + 1 is SMALLER than the reference count,
     * the unique vertices are just [min, max] (a few may go unused) and each
     * occurrence maps to vertex_id - min. Edge geometry draws -- thousands per
     * frame in God of War Ascension -- are almost all like that; the hash
     * dedupe was ~8% of the RSX thread. Strictly smaller, so the caller's
     * "remapped" test (unique < count) holds whenever the order changed.
     * RSX_NO_RANGE_REMAP=1 always hashes. */
    {
        static int s_off = -1;
        if (s_off < 0) s_off = getenv("RSX_NO_RANGE_REMAP") ? 1 : 0;
        if (!s_off) {
            u32 lo = refs[0].vertex_id, hi = lo;
            const u32 base = refs[0].base_index;
            u32 same = 1;
            u32 o0 = 1;
#if defined(__AVX2__)
            /* GOWA (21/10): 4 refs (vertex_id, base_index) per 256-bit load */
            {
                __m256i vmin = _mm256_set1_epi32((int)lo), vmax = vmin;
                const __m256i vb = _mm256_set1_epi32((int)base);
                __m256i bad = _mm256_setzero_si256();
                const __m256i odd = _mm256_setr_epi32(0, -1, 0, -1, 0, -1, 0, -1);
                for (o0 = 0; o0 + 4 <= count; o0 += 4) {
                    const __m256i r = _mm256_loadu_si256((const __m256i*)&refs[o0]);
                    /* even lanes vertex_id, odd lanes base_index: min/max only the even ones
                     * (odd lanes replaced by lo so they never win) */
                    const __m256i vid = _mm256_blendv_epi8(r, vmin, odd);
                    vmin = _mm256_min_epu32(vmin, vid);
                    vmax = _mm256_max_epu32(vmax, _mm256_blendv_epi8(r, _mm256_setzero_si256(), odd));
                    bad = _mm256_or_si256(bad, _mm256_and_si256(odd, _mm256_xor_si256(r, vb)));
                }
                u32 t[8];
                _mm256_storeu_si256((__m256i*)t, vmin);
                for (int k = 0; k < 8; k += 2) lo = t[k] < lo ? t[k] : lo;
                _mm256_storeu_si256((__m256i*)t, vmax);
                for (int k = 0; k < 8; k += 2) hi = t[k] > hi ? t[k] : hi;
                same = _mm256_testz_si256(bad, bad);
            }
#endif
            for (u32 o = o0; o < count; o++) {
                const u32 v = refs[o].vertex_id;
                lo = v < lo ? v : lo;
                hi = v > hi ? v : hi;
                same &= refs[o].base_index == base;
            }
            const u64 range = (u64)hi - lo + 1u;
            if (same && range < count) {
                u32 o = 0;
#if defined(__AVX2__)
                {
                    const __m256i vlo = _mm256_set1_epi32((int)lo);
                    const __m256i pick = _mm256_setr_epi32(0, 2, 4, 6, 1, 3, 5, 7);
                    for (; o + 4 <= count; o += 4) {
                        const __m256i r = _mm256_permutevar8x32_epi32(
                            _mm256_loadu_si256((const __m256i*)&refs[o]), pick);
                        _mm_storeu_si128((__m128i*)&remap->occurrence_to_unique[o],
                                         _mm256_castsi256_si128(_mm256_sub_epi32(r, vlo)));
                    }
                }
#endif
                for (; o < count; o++)
                    remap->occurrence_to_unique[o] = refs[o].vertex_id - lo;
                for (u32 i = 0; i < (u32)range; i++) {
                    refs[i].vertex_id = lo + i;
                    refs[i].base_index = base;
                }
                *unique_count = (u32)range;
                g_rsx_remap_dense = 1;
                return 1;
            }
        }
    }

    if (count > UINT_MAX / 2u)
        return 0;
    u32 slot_capacity = 16u;
    const u32 required_slots = count * 2u;
    while (slot_capacity < required_slots) {
        if (slot_capacity > UINT_MAX / 2u)
            return 0;
        slot_capacity *= 2u;
    }
    if (slot_capacity > remap->slot_capacity) {
        rsx_vertex_remap_slot* slots = (rsx_vertex_remap_slot*)realloc(
            remap->slots, (size_t)slot_capacity * sizeof(*slots));
        if (!slots)
            return 0;
        remap->slots = slots;
        remap->slot_capacity = slot_capacity;
        memset(remap->slots, 0,
               (size_t)remap->slot_capacity * sizeof(*remap->slots));
        remap->generation = 1u;
    } else {
        remap->generation++;
        if (!remap->generation) {
            memset(remap->slots, 0,
                   (size_t)remap->slot_capacity * sizeof(*remap->slots));
            remap->generation = 1u;
        }
    }

    u32 unique = 0;
    const u32 mask = remap->slot_capacity - 1u;
    for (u32 occurrence = 0; occurrence < count; occurrence++) {
        const rsx_vertex_ref ref = refs[occurrence];
        const u64 key = ((u64)ref.base_index << 32) | ref.vertex_id;
        u32 slot_index = (u32)rsx_vertex_remap_hash(key) & mask;
        for (;;) {
            rsx_vertex_remap_slot* slot = &remap->slots[slot_index];
            if (slot->generation != remap->generation) {
                slot->key = key;
                slot->value = unique;
                slot->generation = remap->generation;
                refs[unique] = ref;
                remap->occurrence_to_unique[occurrence] = unique;
                unique++;
                break;
            }
            if (slot->key == key) {
                remap->occurrence_to_unique[occurrence] = slot->value;
                break;
            }
            slot_index = (slot_index + 1u) & mask;
        }
    }
    *unique_count = unique;
    return 1;
}

u32 rsx_vertex_remap_index(
    const rsx_vertex_remap* remap, u32 occurrence)
{
    return remap && occurrence < remap->occurrence_capacity
        ? remap->occurrence_to_unique[occurrence] : occurrence;
}

void rsx_vertex_remap_destroy(rsx_vertex_remap* remap)
{
    if (!remap)
        return;
    free(remap->occurrence_to_unique);
    free(remap->slots);
    memset(remap, 0, sizeof(*remap));
}

int rsx_vertex_topology_plan(
    u32 primitive, int refs_remapped, int* indexed)
{
    if (!indexed)
        return 0;
    switch (primitive) {
    case RSX_PRIMITIVE_TRIANGLES:
        *indexed = refs_remapped != 0;
        return 1;
    case RSX_PRIMITIVE_TRIANGLE_STRIP:
    case RSX_PRIMITIVE_TRIANGLE_FAN:
    case RSX_PRIMITIVE_QUADS:
        *indexed = 1;
        return 1;
    default:
        *indexed = 0;
        return 0;
    }
}

#include "rsx_vertex_fetch_simd.h"   /* GOWA: rsx_vertex_fetch_all (SSE) */
