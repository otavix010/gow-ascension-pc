/* spu_interp.c — SPU interpreter core. See spu_interp.h.
 *
 * Decode tables are generated from the validated Python decoder
 * (tools/gen_spu_interp.py -> spu_interp_tables.inc); execute calls the SAME
 * spu_<mnemonic> helpers the lifter emits, so interpreted and lifted code are
 * bit-identical. Uncommon opcodes trap loudly (never silently wrong).
 */
#include "spu_interp.h"
#include "spu_helpers.h"
#include <stdio.h>
#include <stdlib.h>

/* Channel ABI (runtime/spu/spu_channels.c); the lifter emits these same protos
 * into each generated spu_recomp.h — declared here to stay header-independent. */
u128 spu_rdch(spu_context* ctx, uint32_t channel);
void spu_wrch(spu_context* ctx, uint32_t channel, u128 value);
uint32_t spu_rchcnt(spu_context* ctx, uint32_t channel);

#include "spu_interp_tables.inc"   /* spu_op enum, spu_op_name[], spu_dec_* */
static int spu_wkl_is_wws(const spu_context* ctx, uint32_t wid);   /* GOWA_SPU_NUM_REMAP */

/* Work descriptor for the next SYS_SPU_THREAD_STOP_RECEIVE_EVENT service, set
 * by the event-port send that wakes a sim SPU. Single-slot: dispatch is
 * synchronous -- the sending PPU thread runs the SPU inline. */
uint32_t g_spu_pending_evt[3];
int      g_spu_pending_evt_valid;

/* ---- decode: 32-bit insn -> fields (mirrors spu_disasm.spu_decode order) ---- */
/* 16 bytes: it is also the decode-cache entry (GOWA, see spu_step), read in
 * place by the executor. No op has both an immediate and a target, so they
 * share storage. */
typedef struct {
    uint32_t raw;     /* decode cache: the word this entry was decoded from */
    uint16_t op;      /* spu_op */
    uint8_t  rt, ra, rb, rc, ch;
    uint8_t  valid;   /* decode cache: entry filled */
    union {
        int32_t  imm; /* sign/zero-extended immediate or shift, per format */
        uint32_t tgt; /* absolute LS target (br family, lqa/lqr/stqa/stqr) */
    };
} spu_ins;

static int32_t sx(uint32_t v, int bits) {
    uint32_t m = 1u << (bits - 1);
    return (int32_t)((v ^ m) - m);
}

static void spu_decode1(uint32_t insn, uint32_t pc, spu_ins* d) {
    uint32_t op4 = (insn >> 28) & 0xF,  op7 = (insn >> 25) & 0x7F;
    uint32_t op8 = (insn >> 24) & 0xFF, op9 = (insn >> 23) & 0x1FF;
    uint32_t op10 = (insn >> 22) & 0x3FF, op11 = (insn >> 21) & 0x7FF;
    d->rt = insn & 0x7F; d->ra = (insn >> 7) & 0x7F;
    d->rb = (insn >> 14) & 0x7F; d->rc = (insn >> 21) & 0x7F;
    d->ch = (insn >> 7) & 0x7F; d->imm = 0; d->tgt = 0;
    int32_t i10 = sx((insn >> 14) & 0x3FF, 10);
    int32_t i16 = sx((insn >> 7) & 0xFFFF, 16);
    uint32_t i18 = (insn >> 7) & 0x3FFFF;

    /* RRR (4) */
    if ((d->op = spu_dec_rrr(op4)) != SPU_word) return;
    /* RI18 (7) */
    if ((d->op = spu_dec_ri18(op7)) != SPU_word) {
        d->imm = (d->op == SPU_ila) ? (int32_t)i18 : (int32_t)(i18 & 0xFFFF);
        return;
    }
    /* RI16 (9) — before RI10 (shared top-8-bit opcodes) */
    if ((d->op = spu_dec_ri16(op9)) != SPU_word) {
        switch (d->op) {
        case SPU_lqa: case SPU_stqa: d->tgt = ((uint32_t)(i16 & 0xFFFF) << 2) & 0x3FFF0; break;
        case SPU_lqr: case SPU_stqr: d->tgt = ((uint32_t)(i16 * 4) + pc) & 0x3FFF0; break;
        case SPU_br: case SPU_brsl: case SPU_brz: case SPU_brnz:
        case SPU_brhz: case SPU_brhnz: d->tgt = ((uint32_t)(i16 * 4) + pc) & 0x3FFFF; break;
        case SPU_bra: case SPU_brasl: d->tgt = ((uint32_t)(i16 * 4)) & 0x3FFFF; break;
        case SPU_il: d->imm = i16; break;
        default: d->imm = (int32_t)(i16 & 0xFFFF); break; /* ilh/ilhu/iohl */
        }
        return;
    }
    /* channel ops (op11) — before RI10 (wrch/shli op8 clash) */
    if (op11 == 0x00D) { d->op = SPU_rdch;   return; }
    if (op11 == 0x10D) { d->op = SPU_wrch;   return; }
    if (op11 == 0x00F) { d->op = SPU_rchcnt; return; }
    /* RI10 (8) */
    if ((d->op = spu_dec_ri10(op8)) != SPU_word) {
        d->imm = (d->op == SPU_lqd || d->op == SPU_stqd) ? (i10 << 4) : i10;
        return;
    }
    /* RI8 (10) float<->int conversions */
    if ((d->op = spu_dec_ri8(op10)) != SPU_word) { d->imm = (insn >> 14) & 0xFF; return; }
    /* RR (11) */
    if ((d->op = spu_dec_rr(op11)) != SPU_word) {
        if (d->op == SPU_stop) d->imm = insn & 0x3FFF;
        return;
    }
    /* RI7 (11) rotate/shift/gen-control immediates */
    if ((d->op = spu_dec_ri7(op11)) != SPU_word) {
        d->imm = spu_ri7_unsigned(d->op) ? d->rb : sx(d->rb, 7);
        return;
    }
    /* LSX (redundant with RR's lqx/stqx, kept for order-parity) */
    if ((d->op = spu_dec_lsx(op11)) != SPU_word) return;
    d->op = SPU_word;
}

/* spu_lifted_lookup() is defined by spu_fn_registry.c (the single owner of the
 * lifted-function table + eviction). Declared in spu_interp.h. */

/* ---- execute one instruction; returns 1 if the SPU stopped ---- */
#define A   (ctx->gpr[d.ra])
#define B   (ctx->gpr[d.rb])
#define T   (ctx->gpr[d.rt])          /* also the 3rd source in RRR */
#define DST (ctx->gpr[d.rt])          /* default destination */
#define DSTC (ctx->gpr[d.rc])         /* RRR destination */
#define I   (d.imm)
#define PREF(r) ((r)._u32[0])

/* GOWA: decode cache, one entry per LS word, per host thread. An entry is
 * used only while the word in LS still equals the one it was decoded from,
 * so code streamed over (job modules, plugins at varying addresses) needs no
 * invalidation: the next fetch sees a different word and decodes again. The
 * decode depends only on (word, pc) -- pc for the br-family targets -- and the
 * entry is indexed by pc. Sequential table probing per instruction was most
 * of spu_step. GOWA_NO_DECODE_CACHE=1 turns it off. */
/* The cache is an array of spu_ins (16 bytes, four to a cache line); a hit
 * is executed straight from the entry, no copy. */
typedef spu_ins spu_dc_ent;
static SPU_THREAD_LOCAL spu_dc_ent* t_spu_dc;
static __attribute__((noinline)) void spu_dc_fill(spu_ins* e, uint32_t insn, uint32_t pc)
{
    spu_decode1(insn, pc, e);
    e->raw = insn; e->valid = 1;
}
/* Inlined into the run loop: as its own function every interpreted
 * instruction paid a full prologue/epilogue (six GPRs, xmm6-xmm15 -- Win64
 * callee-saved -- and the /GS cookie). dc: this thread's decode cache, or
 * NULL (GOWA_NO_DECODE_CACHE=1). */
static inline __attribute__((always_inline)) int spu_step(spu_context* ctx, spu_dc_ent* dc) {
    uint32_t pc = ctx->pc, m = pc & SPU_LS_MASK;
    uint32_t w; memcpy(&w, ctx->ls + m, 4);
    const uint32_t insn = __builtin_bswap32(w);
    spu_ins dloc; const spu_ins* dp;
    if (dc) {
        spu_ins* e = &dc[m >> 2];
        if (!(e->raw == insn && e->valid)) spu_dc_fill(e, insn, pc);
        dp = e;
    } else { spu_decode1(insn, pc, &dloc); dp = &dloc; }
#define d (*dp)
    uint32_t next = (pc + 4) & 0x3FFFC;

    switch (d.op) {
    /* immediates / loads */
    case SPU_il:  DST = spu_il(I); break;
    case SPU_ilh: DST = spu_ilh((uint16_t)I); break;
    case SPU_ilhu: DST = spu_splat_u32((uint32_t)I << 16); break;
    case SPU_ila:  DST = spu_splat_u32((uint32_t)I & 0x3FFFF); break;
    case SPU_iohl: DST = spu_ori(DST, (int32_t)((uint32_t)I & 0xFFFF)); break;
    case SPU_fsmbi: DST = spu_fsmbi(I); break;
    case SPU_lqd: case SPU_lqx: case SPU_lqa: case SPU_lqr: {
        uint32_t a = (d.op==SPU_lqd) ? PREF(A)+ (uint32_t)I
                   : (d.op==SPU_lqx) ? PREF(A)+PREF(B) : d.tgt;
        DST = spu_ls_read128(ctx, a);
        /* GOWA_SPU_NUM_REMAP: spuNum as a user workload sees it (spu_context.h) */
        if (__builtin_expect(g_spu_num_remap, 0) && (a & SPU_LS_MASK & ~0xFu) == 0x1C0u &&
            (ctx->pc & SPU_LS_MASK) >= 0xA00u && ctx->ls[0x1DF] < 32u && DST._u32[2] - 6u < 2u &&
            spu_wkl_is_wws(ctx, ctx->ls[0x1DF]))
            DST._u32[2] -= (uint32_t)g_spu_num_remap;
        break; }
    case SPU_stqd: case SPU_stqx: case SPU_stqa: case SPU_stqr: {
        uint32_t a = (d.op==SPU_stqd) ? PREF(A)+ (uint32_t)I
                   : (d.op==SPU_stqx) ? PREF(A)+PREF(B) : d.tgt;
        spu_ls_write128(ctx, a, DST); break; }
    /* integer arithmetic */
    case SPU_a:  DST = spu_a(A,B); break;
    case SPU_ai: DST = spu_ai(A,I); break;
    case SPU_sf: DST = spu_sf(A,B); break;
    case SPU_sfi:DST = spu_sfi(A,I); break;
    case SPU_ah: DST = spu_ah(A,B); break;
    case SPU_ahi:DST = spu_ahi(A,I); break;
    case SPU_sfh:DST = spu_sfh(A,B); break;
    case SPU_sfhi:DST = spu_sfhi(A,I); break;
    case SPU_addx: DST = spu_addx(A,B,DST); break;
    case SPU_sfx:  DST = spu_sfx(A,B,DST); break;
    case SPU_cg:   DST = spu_cg(A,B); break;
    case SPU_cgx:  DST = spu_cgx(A,B,DST); break;
    case SPU_mpyi: DST = spu_mpyi(A,I); break;
    case SPU_mpyui: DST = spu_mpyui(A,I); break;
    /* logical */
    case SPU_and: DST = spu_and(A,B); break;
    case SPU_or:  DST = spu_or(A,B); break;
    case SPU_xor: DST = spu_xor(A,B); break;
    case SPU_nand:DST = spu_nand(A,B); break;
    case SPU_nor: DST = spu_nor(A,B); break;
    case SPU_andc:DST = spu_andc(A,B); break;
    case SPU_orc: DST = spu_orc(A,B); break;
    case SPU_eqv: DST = spu_eqv(A,B); break;
    case SPU_andi:DST = spu_andi(A,I); break;
    case SPU_ori: DST = spu_ori(A,I); break;
    case SPU_xori:DST = spu_xori(A,I); break;
    case SPU_andbi:DST = spu_andbi(A,I); break;
    case SPU_andhi:DST = spu_andhi(A,I); break;
    case SPU_orhi: DST = spu_orhi(A,I); break;
    case SPU_orbi: DST = spu_orbi(A,I); break;
    case SPU_xorhi:DST = spu_xorhi(A,I); break;
    case SPU_xorbi:DST = spu_xorbi(A,I); break;
    case SPU_orx:  DST = spu_orx(A); break;
    /* compares */
    case SPU_ceq: DST = spu_ceq(A,B); break;
    case SPU_ceqh:DST = spu_ceqh(A,B); break;
    case SPU_ceqb:DST = spu_ceqb(A,B); break;
    case SPU_ceqi:DST = spu_ceqi(A,I); break;
    case SPU_cgt: DST = spu_cgt(A,B); break;
    case SPU_cgth:DST = spu_cgth(A,B); break;
    case SPU_cgtb:DST = spu_cgtb(A,B); break;
    case SPU_cgti:DST = spu_cgti(A,I); break;
    case SPU_clgt: DST = spu_clgt(A,B); break;
    case SPU_clgth:DST = spu_clgth(A,B); break;
    case SPU_clgtb:DST = spu_clgtb(A,B); break;
    case SPU_clgti:DST = spu_clgti(A,I); break;
    /* shifts / rotates */
    case SPU_shl:  DST = spu_shl(A,B); break;
    case SPU_shli: DST = spu_shli(A,I); break;
    case SPU_shlh: DST = spu_shlh(A,B); break;
    case SPU_shlhi:DST = spu_shlhi(A,I); break;
    case SPU_rot:  DST = spu_rot(A,B); break;
    case SPU_roti: DST = spu_roti(A,I); break;
    case SPU_rotm: DST = spu_rotm(A,B); break;
    case SPU_rotmi:DST = spu_rotmi(A,I); break;
    case SPU_rotma:DST = spu_rotma(A,B); break;
    case SPU_rotmai:DST = spu_rotmai(A,I); break;
    case SPU_roth: DST = spu_roth(A,B); break;
    case SPU_rothi:DST = spu_rothi(A,I); break;
    case SPU_shlqbi: DST = spu_shlqbi(A,B); break;
    case SPU_shlqbii:DST = spu_shlqbii(A,I); break;
    case SPU_shlqby: DST = spu_shlqby(A,B); break;
    case SPU_shlqbyi:DST = spu_shlqbyi(A,I); break;
    case SPU_rotqbi: DST = spu_rotqbi(A,B); break;
    case SPU_rotqbii:DST = spu_rotqbii(A,I); break;
    case SPU_rotqby: DST = spu_rotqby(A,B); break;
    case SPU_rotqbyi:DST = spu_rotqbyi(A,I); break;
    case SPU_rotqmby: DST = spu_rotqmby(A,B); break;
    case SPU_rotqmbyi:DST = spu_rotqmbyi(A,I); break;
    /* select / shuffle / masks / bitcount (RRR: dest=rc, srcs=ra,rb,rt) */
    case SPU_selb:  DSTC = spu_selb(A,B,T); break;
    case SPU_shufb: DSTC = spu_shufb(A,B,T); break;
    case SPU_fsm:  DST = spu_fsm(A); break;
    case SPU_fsmb: DST = spu_fsmb(A); break;
    case SPU_fsmh: DST = spu_fsmh(A); break;
    case SPU_gb:   DST = spu_gb(A); break;
    case SPU_gbb:  DST = spu_gbb(A); break;
    case SPU_gbh:  DST = spu_gbh(A); break;
    case SPU_clz:  DST = spu_clz(A); break;
    case SPU_cntb: DST = spu_cntb(A); break;
    /* gen-controls for insertion */
    case SPU_cbd: DST = spu_cbd(A,I); break;
    case SPU_chd: DST = spu_chd(A,I); break;
    case SPU_cwd: DST = spu_cwd(A,I); break;
    case SPU_cdd: DST = spu_cdd(A,I); break;
    case SPU_cbx: DST = spu_cbx(A,B); break;
    case SPU_chx: DST = spu_chx(A,B); break;
    case SPU_cwx: DST = spu_cwx(A,B); break;
    case SPU_cdx: DST = spu_cdx(A,B); break;
    /* multiply */
    case SPU_mpy:  DST = spu_mpy(A,B); break;
    case SPU_mpyu: DST = spu_mpyu(A,B); break;
    case SPU_mpyh: DST = spu_mpyh(A,B); break;
    case SPU_mpya: DSTC = spu_mpya(A,B,T); break;
    /* sign-extend */
    case SPU_xsbh: DST = spu_xsbh(A); break;
    case SPU_xshw: DST = spu_xshw(A); break;
    case SPU_xswd: DST = spu_xswd(A); break;
    /* float */
    case SPU_fa: DST = spu_fa(A,B); break;
    case SPU_fs: DST = spu_fs(A,B); break;
    case SPU_fm: DST = spu_fm(A,B); break;
    case SPU_fcgt: DST = spu_fcgt(A,B); break;
    case SPU_fceq: DST = spu_fceq(A,B); break;
    case SPU_cflts: DST = spu_cflts(A,I); break;
    case SPU_cfltu: DST = spu_cfltu(A,I); break;
    case SPU_csflt: DST = spu_csflt(A,I); break;
    case SPU_cuflt: DST = spu_cuflt(A,I); break;
    case SPU_frest:  DST = spu_frest(A); break;
    case SPU_frsqest:DST = spu_frsqest(A); break;
    case SPU_fi:     DST = spu_fi(A,B); break;
    case SPU_fcmgt:  DST = spu_fcmgt(A,B); break;
    case SPU_fcmeq:  DST = spu_fcmeq(A,B); break;
    case SPU_fesd:   DST = spu_fesd(A); break;
    case SPU_frds:   DST = spu_frds(A); break;
    case SPU_fma:  DSTC = spu_fma(A,B,T); break;    /* RRR: dest=rc */
    case SPU_fms:  DSTC = spu_fms(A,B,T); break;
    case SPU_fnms: DSTC = spu_fnms(A,B,T); break;
    /* immediate byte/halfword compares */
    case SPU_ceqbi: DST = spu_ceqbi(A,I); break;
    case SPU_cgtbi: DST = spu_cgtbi(A,I); break;
    case SPU_clgtbi:DST = spu_clgtbi(A,I); break;
    case SPU_ceqhi: DST = spu_ceqhi(A,I); break;
    case SPU_cgthi: DST = spu_cgthi(A,I); break;
    case SPU_clgthi:DST = spu_clgthi(A,I); break;
    /* integer / byte extras */
    case SPU_bg:    DST = spu_bg(A,B); break;
    case SPU_bgx:   DST = spu_bgx(A,B,DST); break;   /* RR: rt is accumulator + dest */
    case SPU_absdb: DST = spu_absdb(A,B); break;
    case SPU_avgb:  DST = spu_avgb(A,B); break;
    case SPU_sumb:  DST = spu_sumb(A,B); break;
    case SPU_mpys:  DST = spu_mpys(A,B); break;
    case SPU_mpyhh: DST = spu_mpyhh(A,B); break;
    case SPU_mpyhhu:DST = spu_mpyhhu(A,B); break;
    case SPU_mpyhha: DST = spu_mpyhha(A,B,DST); break;   /* RR (0x346): rt is accumulator + dest, NOT RRR */
    case SPU_mpyhhau:DST = spu_mpyhhau(A,B,DST); break;
    /* rotate / shift extras */
    case SPU_rothm:    DST = spu_rothm(A,B); break;
    case SPU_rothmi:   DST = spu_rothmi(A,I); break;
    case SPU_rotmah:   DST = spu_rothma(A,B); break;
    case SPU_rotmahi:  DST = spu_rotmahi(A,I); break;
    case SPU_rotqmbi:  DST = spu_rotqmbi(A,B); break;
    case SPU_rotqmbii: DST = spu_rotqmbii(A,(int)I); break;
    case SPU_rotqbybi: DST = spu_rotqbybi(A,B); break;
    case SPU_rotqmbybi:DST = spu_rotqmbybi(A,B); break;
    case SPU_shlqbybi: DST = spu_shlqbybi(A,B); break;
    /* channels */
    case SPU_wrch: spu_wrch(ctx, d.ch, DST); break;
    case SPU_rdch: DST = spu_rdch(ctx, d.ch); break;
    case SPU_rchcnt: DST = spu_pref_u32(spu_rchcnt(ctx, d.ch)); break;   /* count in the preferred word, rest zero (as the lifter) */
    /* hints / no-ops */
    case SPU_nop: case SPU_lnop: case SPU_sync: case SPU_dsync:
    case SPU_hbr: case SPU_hbra: case SPU_hbrr: case SPU_mtspr:
    case SPU_fscrwr: break;
    case SPU_mfspr: DST = spu_mfspr(A); break;      /* SPRs read as zero */
    case SPU_fscrrd: DST = spu_fscrrd(A); break;    /* FPSCR reads as zero */
    /* double precision */
    case SPU_dfa:  DST = spu_dfa(A,B); break;
    case SPU_dfs:  DST = spu_dfs(A,B); break;
    case SPU_dfm:  DST = spu_dfm(A,B); break;
    case SPU_dfma: DST = spu_dfma(A,B,DST); break;   /* RR: rt is accumulator + dest */
    case SPU_dfms: DST = spu_dfms(A,B,DST); break;
    case SPU_dfnms:DST = spu_dfnms(A,B,DST); break;
    case SPU_dfnma:DST = spu_dfnma(A,B,DST); break;
    case SPU_dfceq:  DST = spu_dfceq(A,B); break;
    case SPU_dfcgt:  DST = spu_dfcgt(A,B); break;
    case SPU_dfcmeq: DST = spu_dfcmeq(A,B); break;
    case SPU_dfcmgt: DST = spu_dfcmgt(A,B); break;
    case SPU_dftsv:  DST = spu_dftsv(A, (int32_t)(d.rb & 0x7F)); break;   /* I7 sits in the rb field */
    /* control flow */
    case SPU_br: case SPU_bra: next = d.tgt; break;
    case SPU_brsl: case SPU_brasl: DST = spu_link((pc + 4) & 0x3FFFC); next = d.tgt;
        { extern void spu_trace_call(uint32_t,uint32_t); spu_trace_call(pc, d.tgt); } break;
    case SPU_brz:  if (PREF(DST) == 0) next = d.tgt; break;
    case SPU_brnz: if (PREF(DST) != 0) next = d.tgt; break;
    case SPU_brhz: if ((PREF(DST) & 0xFFFF) == 0) next = d.tgt; break;
    case SPU_brhnz:if ((PREF(DST) & 0xFFFF) != 0) next = d.tgt; break;
    /* bi/iret carry the interrupt enable/disable bits (E = 0x40000 sets
     * int_enable, D = 0x80000 clears it), effective when the branch is taken,
     * exactly as the lifter decodes them. */
    #define SPU_IED_BITS() do { \
        if (insn & 0x40000u) ctx->int_enable = 1; \
        else if (insn & 0x80000u) ctx->int_enable = 0; \
    } while (0)
    case SPU_bi:   next = PREF(A) & 0x3FFFC; SPU_IED_BITS(); break;
    case SPU_bisl: { uint32_t tg = PREF(A) & 0x3FFFC;   /* read ra BEFORE the link write: rt may alias ra */
        DST = spu_link((pc + 4) & 0x3FFFC); next = tg; break; }
    case SPU_bisled: { uint32_t tg = PREF(A) & 0x3FFFC;   /* branch when an enabled event is pending */
        DST = spu_link((pc + 4) & 0x3FFFC);
        if ((__atomic_load_n(&ctx->event_status, __ATOMIC_ACQUIRE) & ctx->event_mask) != 0) {
            next = tg;
            SPU_IED_BITS(); }   /* E/D effective when taken, like the lift */
        break; }
    case SPU_iret: next = ctx->srr0 & 0x3FFFC;
        /* The handler preserves every register: on a return to the point the
         * interrupt was taken, restore the register file saved there. The
         * lifted dispatch path does this in spu_irq_regs_maybe_restore; an iret
         * executed here has to as well, or the snapshot stays armed and later
         * lands on an unrelated dispatch. */
        SPU_IED_BITS();
        if (ctx->irq_saved && next == (ctx->irq_resume_pc & SPU_LS_MASK)) {
            memcpy(ctx->gpr, ctx->irq_gpr, sizeof ctx->irq_gpr);
            ctx->irq_saved = 0;
        }
        break;
    case SPU_biz:  if (PREF(DST) == 0) next = PREF(A) & 0x3FFFC; break;
    case SPU_binz: if (PREF(DST) != 0) next = PREF(A) & 0x3FFFC; break;
    case SPU_bihz: if ((PREF(DST) & 0xFFFF) == 0) next = PREF(A) & 0x3FFFC; break;
    case SPU_bihnz:if ((PREF(DST) & 0xFFFF) != 0) next = PREF(A) & 0x3FFFC; break;
    case SPU_stop: case SPU_stopd:
        ctx->pc = next; ctx->stop_code = (d.op == SPU_stopd) ? 0x3FFFu : (uint32_t)I;   /* stopd carries no code field: RPCS3 reports 0x3FFF */
        ctx->status = SPU_STATUS_STOPPED_BY_STOP; return 1;
    /* conditional halts (assertions): stop the SPU when the condition holds,
     * else continue. The preferred word of ra is compared. */
    case SPU_heq: case SPU_heqi: case SPU_hgt: case SPU_hgti:
    case SPU_hlgt: case SPU_hlgti: {
        uint32_t a = PREF(A), rhs = (d.op==SPU_heq||d.op==SPU_hgt||d.op==SPU_hlgt) ? PREF(B) : (uint32_t)I;
        int hit = (d.op==SPU_heq||d.op==SPU_heqi)   ? (a == rhs)
                : (d.op==SPU_hgt||d.op==SPU_hgti)   ? ((int32_t)a > (int32_t)rhs)
                :                                     (a > rhs);      /* hlgt/hlgti */
        if (hit) { extern void spu_trace_dump(uint32_t); spu_trace_dump(pc);
                   ctx->pc = pc; ctx->stop_code = 0x1000; ctx->status = SPU_STATUS_STOPPED_BY_HALT; return 1; }
        break; }
    default:
        fprintf(stderr, "[spu_interp] unimplemented op '%s' (0x%08X) at LS 0x%05X\n",
                d.op < SPU_OP_COUNT ? spu_op_name[d.op] : "?", insn, pc);
        ctx->pc = pc; ctx->status = SPU_STATUS_STOPPED_BY_HALT; return 1;
    }
    ctx->pc = next;
    return 0;
#undef d
}

/* GOWA: por thread. Eram globais gravados em toda chamada do interpretador por
 * todas as SPUs (a linha de cache pulava entre os núcleos); só são lidos na
 * mesma thread, depois de spu_run_interp_job (lv2_register.c). */
SPU_THREAD_LOCAL uint32_t g_spu_interp_last_pc = 0;
SPU_THREAD_LOCAL uint64_t g_spu_interp_steps   = 0;
static SPU_THREAD_LOCAL uint64_t t_spu_isteps;   /* GOWA: steps of the last run_until, this thread */

/* Call-trace ring buffer for diagnosing SPU asserts (env SPU_CALLTRACE). */
#define SPU_TRACE_N 32
static struct { uint32_t from, to; } s_trace[SPU_TRACE_N];
static unsigned s_trace_i = 0;
void spu_trace_call(uint32_t from, uint32_t to) {
    s_trace[s_trace_i % SPU_TRACE_N].from = from;
    s_trace[s_trace_i % SPU_TRACE_N].to   = to;
    s_trace_i++;
}
void spu_trace_dump(uint32_t at) {
    if (!getenv("SPU_CALLTRACE")) return;
    fprintf(stderr, "[SPU-TRACE] halt at LS 0x%05X; last %d calls (from->to):\n", at, SPU_TRACE_N);
    unsigned n = s_trace_i < SPU_TRACE_N ? s_trace_i : SPU_TRACE_N;
    for (unsigned k = 0; k < n; k++) {
        unsigned idx = (s_trace_i - n + k) % SPU_TRACE_N;
        fprintf(stderr, "   0x%05X -> 0x%05X\n", s_trace[idx].from, s_trace[idx].to);
    }
}

uint32_t spu_interp_run(spu_context* ctx, uint32_t start_lsa) {
    return spu_interp_run_until(ctx, start_lsa, 0);
}


/* GOWA: arquivo de rastro do PM (out/pmtrace_spuN.txt), compartilhado entre o
 * rastro de desvios do interpretador e os registros de DMA/interrupção.
 *   GOWA_PM_TRACE=<wid>          workload rastreado (wklCurrentId, LS 0x1DF)
 *   GOWA_PM_TRACE_FROM_MS=<ms>   só a partir desse tempo (timebase)
 *   GOWA_PM_TRACE_MAX=<linhas>   limite por SPU (padrão 400000)
 * Devolve NULL quando o SPU não está nesse workload / fora da janela. */
FILE* gowa_pmtrace_file(spu_context* ctx)
{
    static int s_wid = -2; static unsigned long long s_from; static long s_max;
    if (s_wid == -2) {
        const char* e = getenv("GOWA_PM_TRACE"); s_wid = e ? atoi(e) : -1;
        e = getenv("GOWA_PM_TRACE_FROM_MS"); s_from = e ? strtoull(e, 0, 0) : 0;
        e = getenv("GOWA_PM_TRACE_MAX"); s_max = e ? atol(e) : 400000;
    }
    if (s_wid < 0 || ctx->ls[0x1DF] != (uint8_t)s_wid) return NULL;
    static SPU_THREAD_LOCAL FILE* t_f; static SPU_THREAD_LOCAL long t_n;
    if (t_n >= s_max) return NULL;
    if (s_from) { extern uint64_t ppu_timebase_now(void);
        if (ppu_timebase_now() / 79800ull < s_from) return NULL; }
    if (!t_f) { char fn[256];
        snprintf(fn, sizeof fn, "out/pmtrace_spu%u.txt", (unsigned)ctx->spu_id);
        t_f = fopen(fn, "w"); if (!t_f) return NULL; }
    t_n++;
    return t_f;
}

/* GOWA: registro dos contextos SPU e de "está no interpretador", para o
 * amostrador de tempo por workload do port (GOWA_SPU_SAMPLER). */
/* GOWA: uma linha de cache (128 bytes, com a vizinha do prefetch) por SPU:
 * o par é gravado em toda entrada no interpretador e duas vezes em toda
 * chamada de função quente; num vetor compacto as 6 SPUs disputavam a mesma
 * linha. */
typedef struct { spu_context* volatile ctx; volatile int in_interp; char pad[128 - sizeof(void*) - sizeof(int)]; } gowa_spu_slot;
#if defined(_MSC_VER)
__declspec(align(128)) static gowa_spu_slot g_gowa_spu_slot[8];
#else
static gowa_spu_slot g_gowa_spu_slot[8] __attribute__((aligned(128)));
#endif
static uint32_t spu_interp_run_until_inner(spu_context* ctx, uint32_t start_lsa, uint32_t stop_lsa);
/* GOWA bisection of a lifted job module: while non-zero on this host thread,
 * the interpreter runs straight to stop_lsa instead of rejoining lifted code at
 * the first lifted address -- a "skipped" function and everything it calls run
 * interpreted. */
static __declspec(thread) int g_spu_interp_norejoin;
void spu_interp_norejoin_push(void) { g_spu_interp_norejoin++; }
void spu_interp_norejoin_pop(void)  { g_spu_interp_norejoin--; }

/* GOWA_FNSKIP=lo-hi[,lo-hi...] (LS, hex). scripts/gancho_fnskip.py puts
 * `if (gowa_fnskip(ctx, ADDR)) return;` at the entry of every function of a
 * lifted job module; a function whose entry falls in a range runs in the
 * interpreter (with everything it calls) up to its link r0, and the lifted body
 * is skipped. Without the variable it costs one test. Bisects which function
 * of a module diverges when lifted (God of War Ascension: 0x6FC080, 0x7B6680). */
/* GOWA_FNDIFF=<addr hex>[:n]: differential run of one lifted function (first n
 * calls, default 4). At its entry: snapshot registers + LS, run the INTERPRETER
 * to the link (reference), restore the snapshot, run the LIFTED function and
 * drain to the link, then print where registers / LS differ. The lifted run is
 * the one that stays. Side effects (DMA, channels) run twice: only for
 * functions where that is harmless. */
extern void spu_drain_call(spu_context* ctx, uint32_t return_pc);
static __declspec(thread) int g_fndiff_busy;
static int gowa_fndiff(spu_context* ctx, unsigned addr)
{
    static int on = -1; static unsigned target, maxn = 4, done = 0;
    if (on < 0) {
        const char* e = getenv("GOWA_FNDIFF"); on = 0;
        if (e && *e) { char* q = NULL; target = (unsigned)strtoul(e, &q, 16); if (q && *q == ':') maxn = (unsigned)strtoul(q + 1, NULL, 10); on = 1; }
    }
    if (!on || addr != target || g_fndiff_busy || done >= maxn) return 0;
    spu_lifted_fn f = spu_lifted_lookup(ctx, addr);
    if (!f) f = spu_lifted_lookup_resident(ctx, addr);
    if (!f) return 0;
    g_fndiff_busy = 1;
    const unsigned idx = __atomic_fetch_add(&done, 1u, __ATOMIC_RELAXED);
    /* per host thread: several SPUs run the same function at once */
    static __declspec(thread) uint8_t* ls_s; static __declspec(thread) uint8_t* ls_i;
    if (!ls_s) { ls_s = (uint8_t*)malloc(SPU_LS_SIZE); ls_i = (uint8_t*)malloc(SPU_LS_SIZE); }
    u128 r_s[128], r_i[128];
    const uint32_t link = ctx->gpr[0]._u32[0] & 0x3FFFFu;
    const uint32_t ie_s = ctx->int_enable;
    memcpy(r_s, ctx->gpr, sizeof r_s); memcpy(ls_s, ctx->ls, SPU_LS_SIZE);
    /* reference */
    g_spu_interp_norejoin++;
    spu_interp_run_until(ctx, addr, link);
    g_spu_interp_norejoin--;
    const uint32_t pc_i = ctx->pc & 0x3FFFFu, ie_i = ctx->int_enable;
    memcpy(r_i, ctx->gpr, sizeof r_i); memcpy(ls_i, ctx->ls, SPU_LS_SIZE);
    /* lifted, from the same state */
    memcpy(ctx->gpr, r_s, sizeof r_s); memcpy(ctx->ls, ls_s, SPU_LS_SIZE);
    ctx->int_enable = ie_s;
    ctx->pc = addr;
    f(ctx);
    spu_drain_call(ctx, link);
    const uint32_t pc_l = ctx->pc & 0x3FFFFu;
    int nd = 0;
    const int show = pc_i != pc_l || ie_i != ctx->int_enable || memcmp(r_i, ctx->gpr, sizeof r_i) ||
                     memcmp(ls_i, ctx->ls, SPU_LS_SIZE) || (idx % 1000) == 0;
    if (show)
        fprintf(stderr, "[fndiff] #%u 0x%05X link=0x%05X pc interp=0x%05X lifted=0x%05X ie %u/%u\n",
                idx, addr, link, pc_i, pc_l, ie_i, ctx->int_enable);
    for (int r = 0; r < 128; r++)
        if (memcmp(&r_i[r], &ctx->gpr[r], 16)) {
            if (nd++ < 24)
                fprintf(stderr, "[fndiff]   r%-3d interp %08X %08X %08X %08X  lifted %08X %08X %08X %08X  (entrada %08X %08X %08X %08X)\n",
                        r, r_i[r]._u32[0], r_i[r]._u32[1], r_i[r]._u32[2], r_i[r]._u32[3],
                        ctx->gpr[r]._u32[0], ctx->gpr[r]._u32[1], ctx->gpr[r]._u32[2], ctx->gpr[r]._u32[3],
                        r_s[r]._u32[0], r_s[r]._u32[1], r_s[r]._u32[2], r_s[r]._u32[3]);
        }
    int nr = 0; uint32_t a = 0;
    while (a < SPU_LS_SIZE) {
        if (ls_i[a] == ctx->ls[a]) { a++; continue; }
        uint32_t b = a; while (b < SPU_LS_SIZE && (ls_i[b] != ctx->ls[b] || (b + 1 < SPU_LS_SIZE && ls_i[b + 1] != ctx->ls[b + 1]))) b++;
        if (nr++ < 16) {
            fprintf(stderr, "[fndiff]   LS 0x%05X..0x%05X:", a, b);
            for (uint32_t k = a; k < b && k < a + 16; k++) fprintf(stderr, " %02X/%02X", ls_i[k], ctx->ls[k]);
            fprintf(stderr, "\n");
        }
        a = b + 1;
    }
    if (show)
        fprintf(stderr, "[fndiff] #%u: %d registradores e %d faixas de LS diferentes (interp/lifted)\n", idx, nd, nr);
    g_fndiff_busy = 0;
    return 1;
}

int gowa_fnskip(spu_context* ctx, unsigned addr)
{
    if (gowa_fndiff(ctx, addr)) return 1;
    static int on = -1; static unsigned lo[16], hi[16]; static int n = 0;
    if (on < 0) {
        const char* e = getenv("GOWA_FNSKIP");
        while (e && *e && n < 16) {
            char* q = NULL;
            lo[n] = (unsigned)strtoul(e, &q, 16);
            hi[n] = (q && *q == '-') ? (unsigned)strtoul(q + 1, &q, 16) : lo[n] + 1;
            n++;
            e = (q && *q == ',') ? q + 1 : NULL;
        }
        on = n > 0;
        if (on) fprintf(stderr, "[fnskip] %d faixa(s), 1a: 0x%05X-0x%05X\n", n, lo[0], hi[0]);
    }
    if (!on || g_spu_interp_norejoin) return 0;
    int hit = 0;
    for (int i = 0; i < n; i++) if (addr >= lo[i] && addr < hi[i]) { hit = 1; break; }
    if (!hit) return 0;
    const uint32_t link = ctx->gpr[0]._u32[0] & 0x3FFFFu;
    g_spu_interp_norejoin++;
    spu_interp_run_until(ctx, addr, link);
    g_spu_interp_norejoin--;
    return 1;
}

/* GOWA_INTERP_ENTRY_LOG=1: where interpretation STARTS (each run_until), by
 * (workload, start LS) with the instructions it ran; every ~10 s the 20
 * biggest by instructions, with the EA of the first 32 bytes at the start
 * (searched 0x00600000-0x00A00000) and the 4 bytes before it. For finding
 * code entered at an address the lift does not know as a function (a plugin
 * called in the middle). Racy counters: a diagnostic. */
extern uint8_t* vm_base;
#define GOWA_IEL_N 4096
static struct { uint32_t key; uint32_t n; uint64_t steps; uint8_t b[32]; } s_iel[GOWA_IEL_N];
static __attribute__((noinline)) void gowa_interp_entry_note(const spu_context* ctx, uint32_t start, uint64_t steps)
{
    uint32_t w = ctx->ls[0x1DF];
    if (ctx->ls[0x1DC] | ctx->ls[0x1DD] | ctx->ls[0x1DE] || w > 32u) w = 33u;
    const uint32_t key = (w << 18) | (start & SPU_LS_MASK & ~3u);
    uint32_t h = (key * 2654435761u) >> 20;
    for (int p = 0; p < 64; p++, h = (h + 1) & (GOWA_IEL_N - 1)) {
        if (s_iel[h].n && s_iel[h].key != key) continue;
        if (!s_iel[h].n) { s_iel[h].key = key; memcpy(s_iel[h].b, ctx->ls + (start & SPU_LS_MASK & ~3u), 32); }
        s_iel[h].n++; s_iel[h].steps += steps;
        break;
    }
    extern uint64_t ppu_timebase_now(void);
    static volatile long long s_last;
    const long long now = (long long)ppu_timebase_now();
    long long last = s_last;
    if (!last) { __atomic_compare_exchange_n(&s_last, &last, now, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED); return; }
    if (now - last < 10LL * 79800000LL ||
        !__atomic_compare_exchange_n(&s_last, &last, now, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) return;
    uint64_t tot = 0;
    for (int i = 0; i < GOWA_IEL_N; i++) tot += s_iel[i].steps;
    fprintf(stderr, "[interp-entry] %.1f M instr em 10 s\n", tot / 1e6);
    for (int r = 0; r < 40 && tot; r++) {
        int bi = -1; uint64_t best = 0;
        for (int i = 0; i < GOWA_IEL_N; i++) if (s_iel[i].steps > best) { best = s_iel[i].steps; bi = i; }
        if (bi < 0) break;
        uint32_t ea = 0;
        if (vm_base)
            for (uint32_t e = 0x00600000u; e + 32 <= 0x00A00000u; e += 4)
                if (vm_base[e] == s_iel[bi].b[0] && !memcmp(vm_base + e, s_iel[bi].b, 32)) { ea = e; break; }
        /* the 32 bytes in full (21/10): plugin code is copied to a different LS per job and
         * the same library routine sits in several plugins -- scripts/interp_plugins.py finds
         * the plugin and the offset from them */
        char hx[65];
        for (int k = 0; k < 32; k++) snprintf(hx + 2 * k, 3, "%02X", s_iel[bi].b[k]);
        fprintf(stderr, "[interp-entry]   w%-2u LS 0x%05X: %5.2f%% (%u entradas, %.0f instr/entrada) ea=0x%08X %s\n",
                s_iel[bi].key >> 18, s_iel[bi].key & 0x3FFFFu, best * 100.0 / tot, s_iel[bi].n,
                (double)best / s_iel[bi].n, ea, hx);
        s_iel[bi].steps = 0;
    }
    memset(s_iel, 0, sizeof s_iel);
}

uint32_t spu_interp_run_until(spu_context* ctx, uint32_t start_lsa, uint32_t stop_lsa) {
    unsigned _s = (unsigned)ctx->spu_id & 7u;
    g_gowa_spu_slot[_s].ctx = ctx;
    int _prev = g_gowa_spu_slot[_s].in_interp;
    g_gowa_spu_slot[_s].in_interp = 1;
    /* GOWA_INTERP_RATE=1: interpreter speed, ns per interpreted instruction
     * (wall time inside run_until / steps), all SPUs, printed every ~5 s.
     * Busy-wait loops count too, so it measures speed, not useful work. */
    static int s_rate = -1; if (s_rate < 0) s_rate = getenv("GOWA_INTERP_RATE") ? 1 : 0;
    extern uint64_t ppu_timebase_now(void);
    const uint64_t q0 = s_rate ? ppu_timebase_now() : 0;
    t_spu_isteps = 0;
    uint32_t r = spu_interp_run_until_inner(ctx, start_lsa, stop_lsa);
    { static int s_iel_on = -1; if (s_iel_on < 0) s_iel_on = getenv("GOWA_INTERP_ENTRY_LOG") ? 1 : 0;
      if (__builtin_expect(s_iel_on, 0)) gowa_interp_entry_note(ctx, start_lsa, t_spu_isteps); }
    if (s_rate) {
        const uint64_t q1 = ppu_timebase_now();       /* 79.8 MHz */
        static volatile long long s_steps, s_ticks, s_last;
        __atomic_add_fetch(&s_steps, (long long)t_spu_isteps, __ATOMIC_RELAXED);
        __atomic_add_fetch(&s_ticks, (long long)(q1 - q0), __ATOMIC_RELAXED);
        long long last = s_last;
        if ((long long)q1 - last > 5 * 79800000LL &&
            __atomic_compare_exchange_n(&s_last, &last, (long long)q1, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED) && last) {
            const long long st = __atomic_exchange_n(&s_steps, 0, __ATOMIC_RELAXED);
            const long long tk = __atomic_exchange_n(&s_ticks, 0, __ATOMIC_RELAXED);
            fprintf(stderr, "[interp-rate] %.1f M instr/s somando SPUs, %.2f ns/instr, %.2f SPUs no interp\n",
                    st / (((long long)q1 - last) / 79.8e6) / 1e6,
                    st ? tk / 0.0798 / (double)st : 0.0, tk / (double)((long long)q1 - last));
        }
    }
    g_spu_interp_last_pc = ctx->pc;
    g_gowa_spu_slot[_s].in_interp = _prev;
    return r;
}
/* GOWA: "hot" lifted functions of code that otherwise stays interpreted (a
 * job module whose full lift misbehaves). When interpretation branches to one
 * of these LS addresses and the bytes there are that function's (compared with
 * its source in main memory -- another module may sit at the same address),
 * the interpreter calls the lifted function inline and resumes interpreting
 * wherever it leaves off: ctx->pc after a tail transfer (the trampoline is
 * dropped) or after a return. No host frame outlives the call, so the guest
 * call stack and the host frames cannot drift apart the way a full rejoin
 * can. Meant for leaf loops: whatever the function calls runs lifted too. */
typedef struct { uint32_t lsa, len, ea, mod_lsa, mod_ea; void (*fn)(spu_context*); } spu_hot_fn;
#define SPU_HOT_SLOTS 8192u                     /* open addressing by LS address */
static spu_hot_fn s_hot[SPU_HOT_SLOTS];
static int s_hot_n;
static uint8_t s_hot_bit[SPU_LS_SIZE / 4 / 8];  /* LS words that start a hot function */
extern uint8_t* vm_base;
static inline uint32_t spu_hot_slot(uint32_t lsa) { return ((lsa >> 2) * 2654435761u) >> 19; }
/* fn's code is len bytes at LS lsa, loaded from main memory ea; mod_lsa/mod_ea
 * name the module it belongs to (its first 0x40 bytes are checked too). */
void spu_interp_hot_register(uint32_t lsa, uint32_t len, uint32_t ea, uint32_t mod_lsa, uint32_t mod_ea,
                             void (*fn)(spu_context*))
{
    lsa &= SPU_LS_MASK;
    if (!fn || !len || s_hot_n >= (int)(SPU_HOT_SLOTS * 3 / 4)) return;
    for (uint32_t k = spu_hot_slot(lsa), i = 0; i < SPU_HOT_SLOTS; i++, k = (k + 1) & (SPU_HOT_SLOTS - 1))
        if (!s_hot[k].fn) {
            s_hot[k].lsa = lsa; s_hot[k].len = len; s_hot[k].ea = ea;
            s_hot[k].mod_lsa = mod_lsa & SPU_LS_MASK; s_hot[k].mod_ea = mod_ea; s_hot[k].fn = fn;
            s_hot_bit[lsa >> 5] |= (uint8_t)(1u << ((lsa >> 2) & 7));
            s_hot_n++;
            return;
        }
}
/* Relocatable hot functions: position-independent leaf code that is DMA'd to
 * a different LS address on each use (God of War Ascension's job plugins).
 * Found by content -- first word filter, then the whole function's bytes --
 * and called with g_spu_hot_reloc = actual LS - LS it was lifted at; the
 * post-processed lift adds it to its pc-relative (lqr/stqr) addresses. A tail
 * transfer leaves the lifted-space pc in ctx->pc, which is relocated back; a
 * return (bi to a register) leaves a real address. */
typedef struct { uint32_t w0, len, ea, lifted; void (*fn)(spu_context*); } spu_hot_rel;
static spu_hot_rel s_rel[512];
static int s_rel_n;
static uint8_t s_rel_filter[65536 / 8];
SPU_THREAD_LOCAL int32_t g_spu_hot_reloc;
static inline uint32_t spu_rel_hash(uint32_t w) { return (w * 2654435761u) >> 16; }
void spu_interp_hot_register_reloc(uint32_t lifted_lsa, uint32_t len, uint32_t ea, uint32_t w0,
                                   void (*fn)(spu_context*))
{
    if (s_rel_n >= 512 || !fn || !len) return;
    s_rel[s_rel_n].w0 = w0; s_rel[s_rel_n].len = len; s_rel[s_rel_n].ea = ea;
    s_rel[s_rel_n].lifted = lifted_lsa & SPU_LS_MASK; s_rel[s_rel_n].fn = fn;
    s_rel_filter[spu_rel_hash(w0) >> 3] |= (uint8_t)(1u << (spu_rel_hash(w0) & 7));
    s_rel_n++;
}
/* GOWA_HOTDIFF=N: every Nth relocatable call, run it natively AND interpreted
 * from the same registers/LS and report where they differ (debug). */
static SPU_THREAD_LOCAL int t_hot_off;
static __attribute__((noinline)) void spu_hot_diff(spu_context* ctx, const spu_hot_rel* h, int32_t rel)
{
    static SPU_THREAD_LOCAL uint8_t* ls0; static SPU_THREAD_LOCAL uint8_t* ls1;
    static SPU_THREAD_LOCAL u128 g0[128], g1[128];
    static volatile long s_rep;
    if (!ls0) { ls0 = (uint8_t*)malloc(SPU_LS_SIZE); ls1 = (uint8_t*)malloc(SPU_LS_SIZE); }
    if (!ls0 || !ls1) return;
    const uint32_t pc0 = ctx->pc & SPU_LS_MASK;
    memcpy(ls0, ctx->ls, SPU_LS_SIZE); memcpy(g0, ctx->gpr, sizeof g0);
    void (*saved)(spu_context*) = g_spu_trampoline_fn;
    g_spu_hot_reloc = rel; g_spu_trampoline_fn = 0;
    h->fn(ctx);
    if (g_spu_trampoline_fn && g_spu_trampoline_fn != spu_indirect_branch)
        ctx->pc = (uint32_t)(ctx->pc + (uint32_t)rel) & SPU_LS_MASK;
    g_spu_trampoline_fn = saved;
    const uint32_t pc1 = ctx->pc & SPU_LS_MASK;
    memcpy(ls1, ctx->ls, SPU_LS_SIZE); memcpy(g1, ctx->gpr, sizeof g1);
    memcpy(ctx->ls, ls0, SPU_LS_SIZE); memcpy(ctx->gpr, g0, sizeof g0);
    t_hot_off = 1;
    spu_interp_run_until(ctx, pc0, pc1);              /* reference: interpreted to the same exit */
    t_hot_off = 0;
    int nd = 0; char b[1600]; int p = 0;
    for (int r = 0; r < 128 && p < 1400; r++)
        if (memcmp(&ctx->gpr[r], &g1[r], 16)) {
            nd++;
            p += snprintf(b + p, sizeof b - p, " r%d nat=%08X.%08X.%08X.%08X int=%08X.%08X.%08X.%08X", r,
                          g1[r]._u32[0], g1[r]._u32[1], g1[r]._u32[2], g1[r]._u32[3],
                          ctx->gpr[r]._u32[0], ctx->gpr[r]._u32[1], ctx->gpr[r]._u32[2], ctx->gpr[r]._u32[3]);
        }
    int nl = 0; uint32_t first = 0;
    for (uint32_t a = 0; a < SPU_LS_SIZE; a += 16) if (memcmp(ctx->ls + a, ls1 + a, 16)) { if (!nl) first = a; nl++; }
    { static volatile long s_cmp, s_bad;
      const long c = __atomic_add_fetch(&s_cmp, 1, __ATOMIC_RELAXED);
      if (nd || nl || (ctx->pc & SPU_LS_MASK) != pc1) __atomic_add_fetch(&s_bad, 1, __ATOMIC_RELAXED);
      if ((c & (c - 1)) == 0 || (c % 20000) == 0)
          fprintf(stderr, "[hotdiff] %ld comparações, %ld divergentes\n", c, s_bad); }
    if ((nd || nl || (ctx->pc & SPU_LS_MASK) != pc1) && s_rep < 40) {
        s_rep++;
        fprintf(stderr, "[hotdiff] ea=0x%08X lifted=0x%05X LS 0x%05X->0x%05X (interp parou em 0x%05X): %d regs, %d linhas LS (1a 0x%05X)%s\n",
                h->ea, h->lifted, pc0, pc1, ctx->pc & SPU_LS_MASK, nd, nl, first, b);
        if (nl) { fprintf(stderr, "[hotdiff]   LS 0x%05X nat:", first);
                  for (int k = 0; k < 16; k++) fprintf(stderr, "%02X", ls1[first + k]);
                  fprintf(stderr, " int:"); for (int k = 0; k < 16; k++) fprintf(stderr, "%02X", ctx->ls[first + k]);
                  fprintf(stderr, "\n"); }
    }
}
static __attribute__((noinline)) int spu_hot_try_reloc(spu_context* ctx, uint32_t w)
{
    const uint32_t pc = ctx->pc & SPU_LS_MASK;
    if (!vm_base || t_hot_off) return 0;
    { static int s_hd = -1; if (s_hd < 0) { const char* e = getenv("GOWA_HOTDIFF"); s_hd = e ? atoi(e) : 0; }
      static SPU_THREAD_LOCAL unsigned t_n;
      if (s_hd > 0 && (++t_n % (unsigned)s_hd) == 0) {
          for (int i = 0; i < s_rel_n; i++) {
              const spu_hot_rel* h = &s_rel[i];
              if (h->w0 == w && pc + h->len <= SPU_LS_SIZE && !memcmp(ctx->ls + pc, vm_base + h->ea, h->len)) {
                  spu_hot_diff(ctx, h, (int32_t)(pc - h->lifted));
                  return 1;
              }
          }
      } }
    for (int i = 0; i < s_rel_n; i++) {
        const spu_hot_rel* h = &s_rel[i];
        if (h->w0 != w || pc + h->len > SPU_LS_SIZE || memcmp(ctx->ls + pc, vm_base + h->ea, h->len)) continue;
        void (*saved)(spu_context*) = g_spu_trampoline_fn;
        const int32_t saved_rel = g_spu_hot_reloc;
        const int32_t rel = (int32_t)(pc - h->lifted);
        g_spu_hot_reloc = rel;
        g_spu_trampoline_fn = 0;
        /* the sampler counts this as native (it was all "interpreted") */
        volatile int* const in_i = &g_gowa_spu_slot[(unsigned)ctx->spu_id & 7u].in_interp;
        const int in_prev = *in_i; *in_i = 0;
        h->fn(ctx);
        *in_i = in_prev;
        if (g_spu_trampoline_fn && g_spu_trampoline_fn != spu_indirect_branch)
            ctx->pc = (uint32_t)(ctx->pc + (uint32_t)rel) & SPU_LS_MASK;   /* tail transfer: lifted space */
        g_spu_trampoline_fn = saved;
        g_spu_hot_reloc = saved_rel;
        return 1;
    }
    /* GOWA_HOTREL_DIAG=1: first word matched, bytes did not -- which plugin
     * and where they first differ (rate-limited). */
    { static int s_dg = -1; if (s_dg < 0) s_dg = getenv("GOWA_HOTREL_DIAG") ? 1 : 0;
      if (s_dg) for (int i = 0; i < s_rel_n; i++) {
          const spu_hot_rel* h = &s_rel[i];
          if (h->w0 != w) continue;
          uint32_t k = 0;
          if (pc + h->len <= SPU_LS_SIZE) while (k < h->len && ctx->ls[pc + k] == vm_base[h->ea + k]) k++;
          static volatile long s_n;
          const long n = __atomic_add_fetch(&s_n, 1, __ATOMIC_RELAXED);
          if (n <= 200 || (n & (n - 1)) == 0)
              fprintf(stderr, "[hotrel-miss] #%ld ea=0x%08X lifted=0x%05X len=0x%X pc=0x%05X difere em +0x%X (ls %02X mem %02X)\n",
                      n, h->ea, h->lifted, h->len, pc, k, k < h->len ? ctx->ls[pc + k] : 0, k < h->len ? vm_base[h->ea + k] : 0);
      } }
    return 0;
}
static __attribute__((noinline)) int spu_hot_try(spu_context* ctx)
{
    const uint32_t pc = ctx->pc & SPU_LS_MASK;
    if (!vm_base || t_hot_off) return 0;
    for (uint32_t k = spu_hot_slot(pc), i = 0; i < SPU_HOT_SLOTS && s_hot[k].fn; i++, k = (k + 1) & (SPU_HOT_SLOTS - 1)) {
        const spu_hot_fn* h = &s_hot[k];
        if (h->lsa != pc) continue;
        if (pc + h->len > SPU_LS_SIZE || h->mod_lsa + 0x40u > SPU_LS_SIZE) continue;
        /* mod_ea 0: no module check -- a leaf's bytes at a given LS address
         * fix its behaviour whichever module they belong to */
        if ((h->mod_ea && memcmp(ctx->ls + h->mod_lsa, vm_base + h->mod_ea, 0x40)) ||
            memcmp(ctx->ls + pc, vm_base + h->ea, h->len)) continue;
        void (*saved)(spu_context*) = g_spu_trampoline_fn;
        g_spu_trampoline_fn = 0;
        volatile int* const in_i = &g_gowa_spu_slot[(unsigned)ctx->spu_id & 7u].in_interp;
        const int in_prev = *in_i; *in_i = 0;
        h->fn(ctx);                              /* runs natively; ctx->pc = where it left off */
        *in_i = in_prev;
        g_spu_trampoline_fn = saved;
        return 1;
    }
    return 0;
}

/* GOWA_SPU_NUM_REMAP: is workload wid run by the WWS job manager (policy
 * module 0x79E500)? Read from the SPURS instance (LS 0x1C4 = the kernel
 * context's spurs pointer; wklInfo1 at +0xB00, wklInfo2 at +0x1000, 0x20 each,
 * addr low word at +4). Only on the rare lq of LS 0x1C0. */
static int spu_wkl_is_wws(const spu_context* ctx, uint32_t wid)
{
    if (!vm_base || wid >= 32u) return 0;
    const uint8_t* l = ctx->ls + 0x1C4;
    const uint32_t sp = ((uint32_t)l[0] << 24) | ((uint32_t)l[1] << 16) | ((uint32_t)l[2] << 8) | l[3];
    if (sp < 0x10000u) return 0;
    const uint8_t* w = vm_base + sp + (wid < 16u ? 0xB00u + wid * 0x20u : 0x1000u + (wid - 16u) * 0x20u) + 4u;
    return (((uint32_t)w[0] << 24) | ((uint32_t)w[1] << 16) | ((uint32_t)w[2] << 8) | w[3]) == 0x0079E500u;
}
/* GOWA_INTERP_HIST=1: interpreted instructions by (workload, 256-byte LS
 * block), all SPUs; every ~10 s the 24 biggest, with where the block's code
 * comes from (its first 32 bytes searched in main memory 0x00600000-0x00A00000,
 * the job modules and plugins) and the share of each workload. Counters are
 * plain (racy) increments: a diagnostic. */
static uint32_t s_ih_cnt[34][1024];
static uint8_t s_ih_bytes[34][1024][32];   /* the block's first 32 bytes when first counted */
static __attribute__((noinline)) void gowa_interp_hist(const spu_context* ctx)
{
    const uint32_t pc = ctx->pc & SPU_LS_MASK;
    const uint8_t* ls = ctx->ls;
    uint32_t w = ls[0x1DF];
    if (ls[0x1DC] | ls[0x1DD] | ls[0x1DE] || w > 32u) w = 33u;
    if (!s_ih_cnt[w][pc >> 8]++) memcpy(s_ih_bytes[w][pc >> 8], ls + (pc & ~0xFFu), 32);
    static SPU_THREAD_LOCAL uint32_t t_n;
    if ((++t_n & 0xFFFFu) != 0) return;
    extern uint64_t ppu_timebase_now(void);
    static volatile long long s_last;
    const long long now = (long long)ppu_timebase_now();
    long long last = s_last;
    if (!last) { __atomic_compare_exchange_n(&s_last, &last, now, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED); return; }
    if (now - last < 10LL * 79800000LL ||
        !__atomic_compare_exchange_n(&s_last, &last, now, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) return;
    static uint32_t snap[34][1024];
    memcpy(snap, s_ih_cnt, sizeof snap);
    memset(s_ih_cnt, 0, sizeof s_ih_cnt);
    unsigned long long tot = 0, pw[34] = {0};
    for (int a = 0; a < 34; a++) for (int b = 0; b < 1024; b++) { tot += snap[a][b]; pw[a] += snap[a][b]; }
    if (!tot) return;
    char line[600]; int q = snprintf(line, sizeof line, "[interp-hist] %.1f M instr/s:", tot / 10.0 / 1e6);
    for (int a = 0; a < 34; a++) if (pw[a] * 100 >= tot) q += snprintf(line + q, sizeof line - q, " w%d=%llu%%", a, pw[a] * 100 / tot);
    fprintf(stderr, "%s\n", line);
    for (int r = 0; r < 24; r++) {
        uint32_t best = 0; int ba = -1, bb = -1;
        for (int a = 0; a < 34; a++) for (int b = 0; b < 1024; b++) if (snap[a][b] > best) { best = snap[a][b]; ba = a; bb = b; }
        if (ba < 0) break;
        snap[ba][bb] = 0;
        const uint32_t lsa = (uint32_t)bb << 8;
        uint32_t ea = 0;
        if (vm_base) {   /* origin: the block's first 32 bytes in the job/plugin region */
            const uint8_t* k = s_ih_bytes[ba][bb];
            for (uint32_t e = 0x00600000u; e + 32 <= 0x00A00000u; e += 4)
                if (vm_base[e] == k[0] && !memcmp(vm_base + e, k, 32)) { ea = e; break; }
        }
        fprintf(stderr, "[interp-hist]   w%-2d LS 0x%05X: %5.2f%%  ea=0x%08X  %02X%02X%02X%02X %02X%02X%02X%02X\n",
                ba, lsa, best * 100.0 / tot, ea, s_ih_bytes[ba][bb][0], s_ih_bytes[ba][bb][1], s_ih_bytes[ba][bb][2],
                s_ih_bytes[ba][bb][3], s_ih_bytes[ba][bb][4], s_ih_bytes[ba][bb][5], s_ih_bytes[ba][bb][6], s_ih_bytes[ba][bb][7]);
    }
}
static uint32_t spu_interp_run_until_inner(spu_context* ctx, uint32_t start_lsa, uint32_t stop_lsa) {
    spu_dc_ent* dc = t_spu_dc;
    {   static int s_dc = -1; if (s_dc < 0) s_dc = getenv("GOWA_NO_DECODE_CACHE") ? 0 : 1;
        if (!dc && s_dc) dc = t_spu_dc = (spu_dc_ent*)calloc(SPU_LS_SIZE / 4, sizeof *dc); }
    ctx->pc = start_lsa & 0x3FFFC;
    ctx->status = SPU_STATUS_RUNNING;
    uint64_t steps = 0;
    /* YDKJ_SPU_TRACE=N: log the last N PCs into a ring buffer and dump them when the
     * interp halts -- shows the path to a branch-to-0 (the cri task/policy wall). */
    static int _tr=-1; if(_tr<0){const char*e=getenv("YDKJ_SPU_TRACE");_tr=e?atoi(e):0;}
    static uint64_t _cap=0; { static int _ci=0; if(!_ci){_ci=1; const char*e=getenv("SPU_STEPCAP"); _cap=e?strtoull(e,0,0):0;} }
    /* YDKJ_CRI_GATE1TRACE=N: cri decode task busy-spins in the validator func_00026E80
     * (LS 0x26E80..0x26F14), a straight-line leaf that returns r3 = 0 / 0x8041090F /
     * 0x80410909. Hand-decoding its selb/fsm/gb/ceqh lanes proved unreliable, so log the
     * ACTUAL runtime result + inputs the first N times it returns. Fires on range-exit
     * (pc left the fn), when r3 and the leaf's non-restored intermediates are still live. */
    static int _g1=-1; if(_g1<0){const char*e=getenv("YDKJ_CRI_GATE1TRACE");_g1=e?atoi(e):0;}
    static int _g1in=0;
    uint32_t ring[64]; int rc=0, rn=0;
    uint32_t prev_pc = 0xFFFFFFF0u;   /* GOWA: last interpreted pc (sequential-probe skip) */
    for (;;) {
        /* GOWA: a run of stqa/lqa (a-form quadword store/load) near the top of
         * LS -- the register save/restore sequence the WWS job manager writes
         * there and re-patches between stqa and lqa, so it can only be
         * interpreted (~17% of the interpreted instructions in the level) --
         * runs in one tight loop, without the per-instruction probes.
         * GOWA_QRUN=0 turns it off. */
        { static int s_qrun = -1; if (s_qrun < 0) { const char* e = getenv("GOWA_QRUN"); s_qrun = !(e && *e == '0'); }
          uint32_t qpc = ctx->pc & SPU_LS_MASK;
          if (s_qrun && qpc >= 0x3F000u) {
              uint32_t n = 0;
              while (qpc < SPU_LS_SIZE && !(stop_lsa && qpc == (stop_lsa & SPU_LS_MASK))) {
                  const uint8_t* q = ctx->ls + qpc;
                  const uint32_t w = ((uint32_t)q[0] << 24) | ((uint32_t)q[1] << 16) | ((uint32_t)q[2] << 8) | q[3];
                  const uint32_t op9 = w >> 23, qa = (((w >> 7) & 0xFFFFu) << 2) & 0x3FFF0u, rt = w & 0x7Fu;
                  if (qa == 0x1C0u) break;                       /* kernel context: normal path */
                  if (op9 == 0x041u) spu_ls_write128(ctx, qa, ctx->gpr[rt]);
                  else if (op9 == 0x061u) ctx->gpr[rt] = spu_ls_read128(ctx, qa);
                  else break;
                  qpc += 4; n++;
              }
              if (n) { ctx->pc = qpc & SPU_LS_MASK; prev_pc = qpc - 4u; steps += n; continue; }
          } }
        /* Match the lifted path: only synthetic HLE tasks may bypass the
         * resident policy. Real tasksets also store 0xA70 in their API table.
         * Preserve the legacy CRI return convention while invoking the HLE. */
        if (ctx->pc == 0xA70u) {
            uint32_t sc = ((uint32_t)ctx->ls[0x27C4]<<24)|((uint32_t)ctx->ls[0x27C5]<<16)
                        | ((uint32_t)ctx->ls[0x27C6]<<8) | ctx->ls[0x27C7];
            if (ctx->image_id == 22 || (ctx->policy_mode && sc == 0xA70u)) {
                extern void spu_spurs_taskset_syscall(spu_context*);
                int _save = ctx->image_id; ctx->image_id = 22;
                spu_spurs_taskset_syscall(ctx);
                ctx->image_id = _save;
                ctx->pc = ctx->gpr[0]._u32[0] & SPU_LS_MASK;   /* resume at link r0 */
                continue;
            }
        }
        /* Rejoin the compiled fast path only for images that HAVE lifted
         * functions (image_id >= 0). image_id < 0 = pure interpretation: an
         * un-lifted image (e.g. a title's raw SPU jobs) must never rejoin
         * another image's functions that happen to share an LS address. */
        /* ponytail: pc-only match; a recursive call through the same site would stop early -- compare r1 too if one shows up. */
        /* GOWA: probe the lifted registry only where control can enter a
         * function -- after a taken branch (pc not the previous pc + 4) -- not
         * on every sequential instruction. spu_lookup walks a hash chain that,
         * with ~30 job modules all based at LS 0x4000, holds one entry per
         * module for a hot address, and it ran per interpreted instruction
         * (plus the resident-region probe): several times the cost of the
         * instruction itself. Falling sequentially into a lifted entry now just
         * keeps interpreting, which is correct. GOWA_INTERP_SEQPROBE=1 restores
         * the per-instruction probe. */
        static int s_seqprobe = -1; if (s_seqprobe < 0) s_seqprobe = getenv("GOWA_INTERP_SEQPROBE") ? 1 : 0;
        const int probe = s_seqprobe || ctx->pc != prev_pc + 4u;
        if (probe && s_hot_n && (s_hot_bit[(ctx->pc & SPU_LS_MASK) >> 5] >> ((ctx->pc >> 2) & 7) & 1) &&
            !(stop_lsa && ctx->pc == stop_lsa) && spu_hot_try(ctx)) {
            prev_pc = 0xFFFFFFF0u;               /* the next pc is a fresh entry: probe it */
            continue;
        }
        if (probe && s_rel_n && !(stop_lsa && ctx->pc == stop_lsa)) {
            uint32_t w; memcpy(&w, ctx->ls + (ctx->pc & SPU_LS_MASK & ~3u), 4); w = __builtin_bswap32(w);
            if ((s_rel_filter[spu_rel_hash(w) >> 3] >> (spu_rel_hash(w) & 7) & 1) && spu_hot_try_reloc(ctx, w)) {
                prev_pc = 0xFFFFFFF0u;
                continue;
            }
        }
        if (ctx->image_id >= 0 && ((stop_lsa && ctx->pc == stop_lsa) ||
            (probe && !g_spu_interp_norejoin && (spu_lifted_lookup(ctx, ctx->pc) || spu_lifted_lookup_resident(ctx, ctx->pc))))) { g_spu_interp_steps = t_spu_isteps = steps; g_spu_interp_last_pc = ctx->pc; return ctx->pc; }  /* rejoin fast path */
        /* (g_spu_interp_last_pc: set once per call by the wrapper -- a global
         * stored on every step by every SPU thread kept its cache line bouncing) */
        const uint32_t prev_pc_in = prev_pc;     /* GOWA_INTERP_DUMP: the pc before this one */
        prev_pc = ctx->pc;
        steps++;
        { static int s_ih = -1; if (s_ih < 0) s_ih = getenv("GOWA_INTERP_HIST") ? 1 : 0;
          if (__builtin_expect(s_ih, 0)) gowa_interp_hist(ctx); }
        /* GOWA: every diagnostic below behind one flag, read once -- they were
         * a dozen tests on each interpreted instruction. */
        static int s_diag = -1;
        if (s_diag < 0) s_diag = (getenv("YDKJ_SPU_TRACE") || getenv("YDKJ_CRI_GATE1TRACE") || getenv("GOWA_PCLOG") ||
                                  getenv("GOWA_PM_TRACE") || getenv("GOWA_PM_WID") || getenv("GOWA_SPU_PROF") ||
                                  getenv("GOWA_INTERP_DUMP")) ? 1 : 0;
        if (__builtin_expect(s_diag, 0)) {
        /* GOWA_INTERP_DUMP=lo-hi (hex LS): the first 6 times interpretation
         * enters that range from outside it, dump 256 bytes from the entry's
         * 256-byte block, the previous pc, r0 and the workload. */
        { static int _id = -1; static uint32_t _lo, _hi; static volatile long _n;
          if (_id < 0) { const char* e = getenv("GOWA_INTERP_DUMP"); _id = e ? 1 : 0;
              if (e) { _lo = (uint32_t)strtoul(e, 0, 16); const char* d = strchr(e, '-'); _hi = d ? (uint32_t)strtoul(d + 1, 0, 16) : _lo + 0x100; } }
          static int _iw = -2; if (_iw == -2) { const char* e = getenv("GOWA_INTERP_DUMP_WID"); _iw = e ? atoi(e) : -1; }
          if (_id && ctx->pc >= _lo && ctx->pc < _hi && !(prev_pc_in >= _lo && prev_pc_in < _hi) &&
              (_iw < 0 || ctx->ls[0x1DF] == (uint8_t)_iw) &&
              __atomic_add_fetch(&_n, 1, __ATOMIC_RELAXED) <= 6) {
              const uint32_t b = ctx->pc & ~0xFFu & SPU_LS_MASK;
              char t[700]; int q = 0;
              for (uint32_t k = 0; k < 256 && b + k < SPU_LS_SIZE; k += 4)
                  q += snprintf(t + q, sizeof t - q, "%02X%02X%02X%02X%s", ctx->ls[b + k], ctx->ls[b + k + 1],
                                ctx->ls[b + k + 2], ctx->ls[b + k + 3], (k & 31) == 28 ? "|" : " ");
              fprintf(stderr, "[interp-dump] spu%u wid=%u pc=0x%05X prev=0x%05X r0=0x%05X r1=0x%05X r3=%08X LS 0x%05X: %s\n",
                      (unsigned)(ctx->spu_id & 7u), ctx->ls[0x1DF], ctx->pc, prev_pc_in & 0x3FFFFu,
                      ctx->gpr[0]._u32[0] & 0x3FFFFu, ctx->gpr[1]._u32[0] & 0x3FFFFu, ctx->gpr[3]._u32[0], b, t);
          } }
        if (_g1>0) {
            int inr = (ctx->pc >= 0x26E80u && ctx->pc < 0x26F14u);
            if (inr) { _g1in = 1; }
            else if (_g1in) {
                _g1in = 0;
                #define GLB(o) (((uint32_t)ctx->ls[(o)]<<24)|((uint32_t)ctx->ls[(o)+1]<<16)|((uint32_t)ctx->ls[(o)+2]<<8)|ctx->ls[(o)+3])
                fprintf(stderr,"[gate1] func_00026E80 ret r3=%08X | LS2FB0=%08X LS2FB4=%08X LS2FB8=%08X | LS1E0=%08X LS1E4=%08X LS1E8=%08X | r1=%05X r29=%08X r32=%08X r16=%08X r22=%08X r8=%08X r6=%08X\n",
                    ctx->gpr[3]._u32[0], GLB(0x2FB0),GLB(0x2FB4),GLB(0x2FB8), GLB(0x1E0),GLB(0x1E4),GLB(0x1E8),
                    ctx->gpr[1]._u32[0], ctx->gpr[29]._u32[0], ctx->gpr[32]._u32[0],
                    ctx->gpr[16]._u32[0], ctx->gpr[22]._u32[0], ctx->gpr[8]._u32[0], ctx->gpr[6]._u32[0]);
                #undef GLB
                fflush(stderr); _g1--;
            }
        }
        if (_tr>0) { ring[rc&63]=ctx->pc; rc++; if(rn<64)rn++; }
        /* GOWA_PM_WID=1: conta as entradas de policy module (pc 0xA00) por
         * workload (LS 0x1DC = wklCurrentId do kernel) e imprime o total a
         * cada ~3 s. Responde "o kernel despacha o workload X?". */
        /* GOWA_PCLOG=pc[,pc...] (hex, até 8): registra cada passagem do
         * interpretador por esses pcs (spu, wid, r3..r6, r0), até 4000 linhas. */
        { static int _pl = -1; static uint32_t _pls[8]; static int _pln;
          if (_pl < 0) { const char* e = getenv("GOWA_PCLOG"); _pl = e ? 1 : 0;
              if (e) { char b[128]; strncpy(b, e, 127); b[127] = 0;
                  for (char* t = strtok(b, ","); t && _pln < 8; t = strtok(NULL, ",")) _pls[_pln++] = (uint32_t)strtoul(t, 0, 16); } }
          static int _plw = -2; if (_plw == -2) { const char* e = getenv("GOWA_PCLOG_WID"); _plw = e ? atoi(e) : -1; }
          if (_pl && (_plw < 0 || ctx->ls[0x1DF] == (uint8_t)_plw)) for (int k = 0; k < _pln; k++) if (ctx->pc == _pls[k]) {
              static volatile long _nn; if (__atomic_add_fetch(&_nn, 1, __ATOMIC_RELAXED) <= 4000) {
                  extern uint64_t ppu_timebase_now(void);
                  fprintf(stderr, "[pclog] t=%llu spu%u wid=%u pc=0x%05X r3=%08X.%08X r4=%08X r5=%08X r6=%08X r0=%05X int=%d irqsaved=%d resume=%05X\n",
                          (unsigned long long)(ppu_timebase_now() / 79800ull), (unsigned)(ctx->spu_id & 7u), ctx->ls[0x1DF],
                          ctx->pc, ctx->gpr[3]._u32[0], ctx->gpr[3]._u32[1], ctx->gpr[4]._u32[0], ctx->gpr[5]._u32[0],
                          ctx->gpr[6]._u32[0], ctx->gpr[0]._u32[0] & 0x3FFFF, ctx->int_enable, ctx->irq_saved,
                          ctx->irq_resume_pc); }
              break; } }
        /* GOWA_PM_TRACE=<wid>: grava em out/pmtrace_spuN.txt cada desvio tomado
         * (pc anterior -> pc novo) enquanto esse workload está em execução, com
         * r3 no destino. Até 400 mil linhas por SPU. */
        { static int _pt = -2; if (_pt == -2) { const char* e = getenv("GOWA_PM_TRACE"); _pt = e ? atoi(e) : -1; }
          if (_pt >= 0) {
              static SPU_THREAD_LOCAL uint32_t prev = 0xFFFFFFFFu;
              /* Mudanças no estado do pipeline do PM WWS (loadJobState 0x12A0,
               * nextLoadJob 0x12C0, loadJob 0x12D0, runJob 0x12E0, storeJob 0x12F0). */
              if (ctx->ls[0x1DF] == (uint8_t)_pt) { static SPU_THREAD_LOCAL uint32_t st[5]; static SPU_THREAD_LOCAL int st_ok;
                static const uint16_t off[5] = { 0x12A0, 0x12C0, 0x12D0, 0x12E0, 0x12F0 };
                uint32_t cur[5]; int mud = 0;
                for (int k = 0; k < 5; k++) {
                    const uint8_t* q = ctx->ls + off[k];
                    cur[k] = ((uint32_t)q[0] << 24) | ((uint32_t)q[1] << 16) | ((uint32_t)q[2] << 8) | q[3];
                    mud |= cur[k] != st[k];
                }
                if (mud && st_ok) {
                    FILE* tfs = gowa_pmtrace_file(ctx);
                    if (tfs) { fprintf(tfs, "STATE pc=0x%05X prev=0x%05X 12A0=%08X 12C0=%08X 12D0=%08X 12E0=%08X 12F0=%08X\n",
                                       ctx->pc, prev & 0x3FFFF, cur[0], cur[1], cur[2], cur[3], cur[4]); fflush(tfs); }
                }
                memcpy(st, cur, sizeof st); st_ok = 1; }
              /* Escritas nas tabelas de buffers do PM WWS: registros dos conjuntos
               * (0xDF0..0xEAF), anel de preparação (0xEB0..0xF2F) e máscaras de
               * ocupação (0x1270..0x129F). Uma linha por palavra alterada. */
              if (ctx->ls[0x1DF] == (uint8_t)_pt) { static SPU_THREAD_LOCAL uint8_t snap[0x1D0]; static SPU_THREAD_LOCAL int sn_ok;
                static const struct { uint16_t a, n; } reg[3] = { { 0xDF0, 0xC0 }, { 0xEB0, 0x80 }, { 0x1270, 0x30 } };
                uint8_t* sp = snap; FILE* tfw = NULL;
                for (int k = 0; k < 3; k++) {
                    const uint8_t* q = ctx->ls + reg[k].a;
                    if (sn_ok && memcmp(sp, q, reg[k].n)) {
                        if (!tfw) tfw = gowa_pmtrace_file(ctx);
                        if (tfw) for (int o = 0; o < reg[k].n; o += 4)
                            if (memcmp(sp + o, q + o, 4))
                                fprintf(tfw, "LSW pc=0x%05X prev=0x%05X [%04X] %02X%02X%02X%02X -> %02X%02X%02X%02X\n",
                                        ctx->pc, prev & 0x3FFFF, reg[k].a + o, sp[o], sp[o+1], sp[o+2], sp[o+3],
                                        q[o], q[o+1], q[o+2], q[o+3]);
                    }
                    memcpy(sp, q, reg[k].n); sp += reg[k].n;
                }
                if (tfw) fflush(tfw);
                sn_ok = 1; }
                                          FILE* tf = (ctx->pc != prev + 4) ? gowa_pmtrace_file(ctx) : NULL;
              if (tf) {
                  extern uint64_t ppu_timebase_now(void);
                  if (tf) { fprintf(tf, "%05X>%05X r3=%08X r4=%08X t=%llu\n", prev & 0x3FFFF, ctx->pc,
                                    ctx->gpr[3]._u32[0], ctx->gpr[4]._u32[0],
                                    (unsigned long long)(ppu_timebase_now() / 79800ull));
                            fflush(tf); }
                  /* GOWA_PM_TRACE_DUMPPC=<pc hex>: na 100ª passagem por esse pc,
                   * grava o LS e os registradores (uma vez por SPU). */
                  { static int _dp = -2; if (_dp == -2) { const char* e = getenv("GOWA_PM_TRACE_DUMPPC"); _dp = e ? (int)strtoul(e, 0, 16) : -1; }
                    static SPU_THREAD_LOCAL int nd = 0;
                    static int _dn = -1; if (_dn < 0) { const char* e = getenv("GOWA_PM_TRACE_DUMPN"); _dn = e ? atoi(e) : 100; }
                    if (_dp >= 0 && ctx->pc == (uint32_t)_dp && ++nd == _dn) {
                        char fn[256];
                        snprintf(fn, sizeof fn, "out/pmdump_spu%u.bin", (unsigned)ctx->spu_id);
                        FILE* f = fopen(fn, "wb"); if (f) { fwrite(ctx->ls, 1, 0x40000, f); fclose(f); }
                        snprintf(fn, sizeof fn, "out/pmdump_spu%u.txt", (unsigned)ctx->spu_id);
                        f = fopen(fn, "w");
                        if (f) { fprintf(f, "pc=0x%05X srr0=0x%05X int=%d evst=0x%X evmask=0x%X\n", ctx->pc, ctx->srr0,
                                         ctx->int_enable, ctx->event_status, ctx->event_mask);
                                 fprintf(f, "mfc: tag_mask=0x%08X tag_status=0x%08X list_stall_mask=0x%08X list_stall_stat=0x%08X\n",
                                         ctx->mfc_tag_mask, ctx->mfc_tag_status, ctx->list_stall_mask, ctx->list_stall_stat);
                                 for (int t = 0; t < 32; t++) if (ctx->list_stall_mask & (1u << t))
                                     fprintf(f, "  stall tag %d: next_elem_lsa=0x%05X left=%u dest=0x%05X ea_base=0x%llX cmd=0x%X\n", t,
                                             ctx->list_stall_elem_lsa[t], ctx->list_stall_remaining[t], ctx->list_stall_dest_lsa[t],
                                             (unsigned long long)ctx->list_stall_ea_base[t], ctx->list_stall_cmd[t]);
                                 for (int r = 0; r < 128; r++)
                                     fprintf(f, "r%-3d %08X %08X %08X %08X\n", r, ctx->gpr[r]._u32[0],
                                             ctx->gpr[r]._u32[1], ctx->gpr[r]._u32[2], ctx->gpr[r]._u32[3]);
                                 fclose(f); }
                    } }
              }
              prev = ctx->pc;
          } }
        if ((steps & 0x3FF) == 0) {
            static int _ps = -1; if (_ps < 0) _ps = getenv("GOWA_PM_WID") ? 1 : 0;
            if (_ps) {
                static volatile long amo[8][32]; static uint64_t last2;
                __atomic_add_fetch(&amo[ctx->spu_id & 7][ctx->ls[0x1DF] & 31], 1, __ATOMIC_RELAXED);
                extern uint64_t ppu_timebase_now(void);
                uint64_t ms = ppu_timebase_now() / 79800ull;
                if (ms - last2 >= 5000) {
                    last2 = ms;
                    for (int sp = 0; sp < 8; sp++) {
                        char b[512]; int p = 0;
                        for (int w = 0; w < 32; w++)
                            if (amo[sp][w]) p += snprintf(b + p, sizeof b - p, " w%d:%ld", w, amo[sp][w]);
                        if (p) fprintf(stderr, "[pm-wid] t=%llums spu%d amostras do interp por wid:%s pc=0x%05X\n",
                                       (unsigned long long)ms, sp, b, ctx->spu_id == sp ? ctx->pc : 0);
                    }
                }
            }
        }
        if (steps == 1 && ctx->pc >= 0xA00u && ctx->pc < 0xA40u) {
            static int _pw = -1; if (_pw < 0) _pw = getenv("GOWA_PM_WID") ? 1 : 0;
            if (_pw) {
                static volatile long cnt[32]; static uint64_t last;
                __atomic_add_fetch(&cnt[ctx->ls[0x1DF] & 31], 1, __ATOMIC_RELAXED);
                extern uint64_t ppu_timebase_now(void);
                uint64_t ms = ppu_timebase_now() / 79800ull;
                if (ms - last >= 3000) {
                    last = ms;
                    char b[512]; int p = 0;
                    for (int w = 0; w < 32; w++)
                        if (cnt[w]) p += snprintf(b + p, sizeof b - p, " w%d:%ld", w, cnt[w]);
                    fprintf(stderr, "[pm-wid] t=%llums entradas no PM por wid:%s\n", (unsigned long long)ms, b);
                }
            }
        }
        /* GOWA_SPU_PROF=1: where interpreted SPU code spends its time. One
         * histogram per SPU over 128-byte LS buckets; roughly every 5 s the
         * busiest buckets are printed with the image id that owns the run. */
        { static int _pf = -1; if (_pf < 0) _pf = getenv("GOWA_SPU_PROF") ? 1 : 0;
          static int _pfw = -2; if (_pfw == -2) { const char* e = getenv("GOWA_SPU_PROF_WID"); _pfw = e ? atoi(e) : -1; }
          if (_pf && (_pfw < 0 || ctx->ls[0x1DF] == (uint8_t)_pfw)) {
              static uint32_t h[8][2048]; static uint64_t last_ms[8];
              unsigned s = (unsigned)ctx->spu_id & 7u;
              h[s][(ctx->pc & 0x3FFFFu) >> 7]++;
              if ((steps & 0x3FFFF) == 0) {
                  extern uint64_t ppu_timebase_now(void);
                  uint64_t ms = ppu_timebase_now() / 79800ull;
                  if (ms - last_ms[s] >= 5000) {
                      last_ms[s] = ms;
                      /* LS 0x1DC = SpursKernelContext.wklCurrentId (RPCS3 layout) */
                      fprintf(stderr, "[spuprof] spu%u img=%d wid=%u pc=0x%05X top:", s, ctx->image_id,
                              ctx->ls[0x1DF], ctx->pc);
                      for (int k = 0; k < 10; k++) {
                          uint32_t best = 0, bi = 0;
                          for (uint32_t i = 0; i < 2048; i++) if (h[s][i] > best) { best = h[s][i]; bi = i; }
                          if (!best) break;
                          fprintf(stderr, " %05X:%u", bi << 7, best);
                          h[s][bi] = 0;
                      }
                      fprintf(stderr, "\n");
                      memset(h[s], 0, sizeof h[s]);
                      /* GOWA_SPU_LSDUMP=<dir>: junto com o perfil, grava o LS inteiro
                       * desse SPU (spuN_<ms>.bin) e os registradores (texto). */
                      { const char* dd = getenv("GOWA_SPU_LSDUMP");
                        if (dd) { char fn[512];
                            snprintf(fn, sizeof fn, "%s/spu%u_%llu.bin", dd, s, (unsigned long long)ms);
                            FILE* f = fopen(fn, "wb");
                            if (f) { fwrite(ctx->ls, 1, 0x40000, f); fclose(f); }
                            snprintf(fn, sizeof fn, "%s/spu%u_%llu.txt", dd, s, (unsigned long long)ms);
                            f = fopen(fn, "w");
                            if (f) { fprintf(f, "pc=0x%05X img=%d srr0=0x%05X int=%d evst=0x%X evmask=0x%X\n",
                                             ctx->pc, ctx->image_id, ctx->srr0, ctx->int_enable,
                                             ctx->event_status, ctx->event_mask);
                                     for (int r = 0; r < 128; r++)
                                         fprintf(f, "r%-3d %08X %08X %08X %08X\n", r, ctx->gpr[r]._u32[0],
                                                 ctx->gpr[r]._u32[1], ctx->gpr[r]._u32[2], ctx->gpr[r]._u32[3]);
                                     fclose(f); } } }
                  }
              }
          } }
        /* SPU_STEPCAP=N: a task that never halts (infinite work/wait loop) never
         * dumps its ring. Force a one-shot dump after N steps to see where it loops. */
        if (_tr>0 && _cap && steps == _cap) {
            fprintf(stderr,"[spu-trace] STEPCAP pc=0x%05X after %llu steps; last %d PCs:",
                    ctx->pc, (unsigned long long)steps, rn);
            for(int k=rn;k>0;k--) fprintf(stderr," %05X", ring[(rc-k)&63]);
            fprintf(stderr,"\n");
            /* dump the LS 0x2700 SpursTasksetContext the poll reads -- compare to the
             * working RPCS3 SPU0 dump (RUNNING/READY/ENABLED=0x80000000, SIGNALLED=0). */
            #define LB(o) (((uint32_t)ctx->ls[(o)]<<24)|((uint32_t)ctx->ls[(o)+1]<<16)|((uint32_t)ctx->ls[(o)+2]<<8)|ctx->ls[(o)+3])
            fprintf(stderr,"[spu-trace] LS ctx: RUN=%08X READY=%08X PEND=%08X ENA=%08X SIG=%08X WAIT=%08X | 2790=%08X 2794=%08X 27D8=%08X 2FB0=%08X\n",
                LB(0x2700),LB(0x2710),LB(0x2720),LB(0x2730),LB(0x2740),LB(0x2750),LB(0x2790),LB(0x2794),LB(0x27D8),LB(0x2FB0));
            #undef LB
            fflush(stderr); _tr--; g_spu_interp_steps = t_spu_isteps = steps; return 0x2000u;
        }
        /* Asynchronous interrupt delivery for INTERPRETED code. The lifted path
         * takes pending enabled events in spu_drain_call (spu_take_interrupt);
         * the interpreter only honoured bisled/iret, so a policy module that
         * runs here -- God of War Ascension's WWS job manager, loaded raw from
         * the title's data into LS 0xA00 -- installed its handler at LS 0, set
         * SPU_WrEventMask = SN and spun forever on state only that handler
         * advances. Same contract as spu_take_interrupt: LS 0 holds a br/bra to
         * the handler, srr0 = interrupted pc, interrupts disabled until iret,
         * register file snapshotted so the iret below restores it.
         * GOWA_SPU_NOIRQ=1 turns it off. */
        }
        /* Event latency: our DMA completes inside the wrch that issued it, so
         * an MFC event (list stall-and-notify, tag update) is pending at the
         * very next instruction. On hardware the transfer -- and a barriered
         * list behind it -- takes hundreds of cycles, and code relies on that:
         * God of War Ascension's WWS policy issues the null stall-notify list
         * for a job's load commands (LS 0x3228) and THEN clears the job's
         * buffer-set records (0x322C..0x325C); the handler that binds those
         * records must run after the clear. Taking the interrupt at the next
         * instruction ran it before, the clear wiped the bindings, and the job
         * waited forever on an unbound buffer set. An event therefore becomes
         * an interrupt only after it has been pending for GOWA_IRQ_DELAY
         * interpreted instructions (default 256; 0 = old behaviour). */
        static int _irqd = -1; if (_irqd < 0) { const char* e = getenv("GOWA_IRQ_DELAY"); _irqd = e ? atoi(e) : 256; }
        static SPU_THREAD_LOCAL uint64_t t_isteps, t_pend_since; static SPU_THREAD_LOCAL int t_pend;
        t_isteps++;
        { const uint32_t pend_now = __atomic_load_n(&ctx->event_status, __ATOMIC_ACQUIRE) & ctx->event_mask;
          if (pend_now) { if (!t_pend) { t_pend = 1; t_pend_since = t_isteps; } }
          else if (t_pend) t_pend = 0; }
        { static int _irq = -1; if (_irq < 0) _irq = getenv("GOWA_SPU_NOIRQ") ? 0 : 1;
          if (_irq && ctx->int_enable && t_pend && t_isteps - t_pend_since >= (uint64_t)_irqd &&
              (__atomic_load_n(&ctx->event_status, __ATOMIC_ACQUIRE) & ctx->event_mask)) {
              const uint8_t* v = ctx->ls;
              uint32_t w = ((uint32_t)v[0] << 24) | ((uint32_t)v[1] << 16) | ((uint32_t)v[2] << 8) | v[3];
              uint32_t op9 = w >> 23, target = 0; int ok = 0;
              if (op9 == 0x060)      { target = ((w >> 7) & 0xFFFF) << 2; ok = 1; }            /* bra */
              else if (op9 == 0x064) { target = (((w >> 7) & 0xFFFF) << 2) & 0x3FFFC; ok = 1; } /* br from LS 0 */
              if (ok) {
                  ctx->srr0 = ctx->pc;
                  ctx->int_enable = 0;
                  ctx->irq_saved = 1;
                  ctx->irq_resume_pc = ctx->srr0;
                  memcpy(ctx->irq_gpr, ctx->gpr, sizeof ctx->irq_gpr);
                  /* GOWA_IRQLOG=n: the first n takes (interp and lifted) with srr0. */
                  { static int _n = 0; static int lim = -1;
                    if (lim < 0) { const char* e = getenv("GOWA_IRQLOG"); lim = e ? atoi(e) : 8; }
                    if (_n++ < lim)
                      fprintf(stderr, "[spu-int] interp TAKEN img=%d events=0x%X&0x%X -> 0x%05X (srr0=0x%05X)\n",
                              ctx->image_id, ctx->event_status, ctx->event_mask, target, ctx->srr0); }
                  { FILE* tf2 = gowa_pmtrace_file(ctx); if (tf2) { fprintf(tf2, "INT ev=0x%X mask=0x%X srr0=0x%05X -> 0x%05X\n", ctx->event_status, ctx->event_mask, ctx->srr0, target); fflush(tf2); } }
                  ctx->pc = target;
              }
          } }
        if (spu_step(ctx, dc)) {
            /* stop 0x110 = SYS_SPU_THREAD_STOP_RECEIVE_EVENT. The worker writes
             * the SPU queue number to its out-mailbox, stops, and lv2 replies
             * with {CELL_OK, data1, data2, data3} in the in-mailbox -- which is
             * how these sim SPUs receive each frame's work-descriptor EA.
             * Treating the stop as "job finished" let them resume with nothing
             * in that structure and DMA their results to address 0.
             * Service it and keep running; with no event pending, fall through
             * to the old behaviour so nothing waits forever. */
            if (ctx->stop_code == 0x110 && g_spu_pending_evt_valid) {
                spu_channel_read(&ctx->ch_out_mbox);      /* the SPU queue key */
                ctx->rcv_evt[0] = 0;                      /* CELL_OK */
                ctx->rcv_evt[1] = g_spu_pending_evt[0];
                ctx->rcv_evt[2] = g_spu_pending_evt[1];
                ctx->rcv_evt[3] = g_spu_pending_evt[2];
                ctx->rcv_evt_n = 4; ctx->rcv_evt_i = 0;
                g_spu_pending_evt_valid = 0;
                ctx->status = SPU_STATUS_RUNNING;
                continue;
            }
            g_spu_interp_steps = t_spu_isteps = steps;
            if (_tr>0) { fprintf(stderr,"[spu-trace] halt stop=0x%X pc=0x%05X after %llu steps; last %d PCs:",
                    ctx->stop_code, ctx->pc, (unsigned long long)steps, rn);
                for(int k=rn;k>0;k--) fprintf(stderr," %05X", ring[(rc-k)&63]);
                fprintf(stderr,"\n"); fflush(stderr); _tr--; }
            return ctx->stop_code; }
    }
}

void spu_dispatch(spu_context* ctx, uint32_t target) {
    for (;;) {
        target &= 0x3FFFC;
        spu_lifted_fn fn = spu_lifted_lookup(ctx, target);
        if (fn) { ctx->pc = target; fn(ctx); return; }
        uint32_t next = spu_interp_run(ctx, target);
        if (ctx->status & (SPU_STATUS_STOPPED_BY_STOP | SPU_STATUS_STOPPED_BY_HALT))
            return;
        if ((next & 0x3FFFC) == target) return;   /* no-progress guard */
        target = next;
    }
}

/* GOWA: o LS de um SPU, para o amostrador identificar o módulo carregado. */
const uint8_t* gowa_spu_ls(int s)
{
    spu_context* c = g_gowa_spu_slot[s & 7].ctx;
    return c ? c->ls : NULL;
}
/* GOWA: leitura do estado de um SPU para o amostrador do port. */
int gowa_spu_sample(int s, unsigned* wid, unsigned* pc, int* interp)
{
    spu_context* c = g_gowa_spu_slot[s & 7].ctx;
    if (!c) return 0;
    *wid = c->ls[0x1DF];
    *pc = (unsigned)c->pc & 0x3FFFFu;
    *interp = g_gowa_spu_slot[s & 7].in_interp;
    return 1;
}
/* GOWA_TL (22/10, linha do tempo): também a imagem em execução (módulo de job, PM, tarefa)
 * e o estado (SPU_STATUS_WAITING_CHANNEL = bloqueada num canal). */
int gowa_spu_sample2(int s, unsigned* wid, unsigned* pc, int* interp, int* img, unsigned* status)
{
    spu_context* c = g_gowa_spu_slot[s & 7].ctx;
    if (!c) return 0;
    *wid = c->ls[0x1DF];
    *pc = (unsigned)c->pc & 0x3FFFFu;
    *interp = g_gowa_spu_slot[s & 7].in_interp;
    *img = c->image_id;
    *status = (unsigned)c->status;
    return 1;
}
