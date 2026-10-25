/* SPU_DRAIN trampoline model -- runtime state + stub hooks (faithful-adopt).
 *
 * Milestone 1 lands the execution-model plumbing; the per-transfer hooks
 * (lockstep gate, flight recorder, SPURS task-launch) are inert stubs here and
 * gain real bodies in their own milestones. Keeping them as real out-of-line
 * functions (not macros) means the SPU_DRAIN/SPU_RET call sites are already in
 * place -- later milestones only replace the bodies. */
#include "spu_context.h"
#include <setjmp.h>
#include "spu_sjlj.h"   /* GOWA: the light setjmp/longjmp pair */

/* See spu_context.h `irq_frame`. */
typedef struct spu_irq_frame {
    spu_jmpbuf            env;
    struct spu_irq_frame* prev;
    uint32_t              depth;
    int                   image_id;
} spu_irq_frame;

/* Pending cross-function transfer target for this host thread's SPU context. */
SPU_THREAD_LOCAL void (*g_spu_trampoline_fn)(spu_context*) = 0;
SPU_THREAD_LOCAL uint32_t g_spu_pch[8];
SPU_THREAD_LOCAL unsigned g_spu_pch_n;

/* yz_lockstep_tick now has its real body in spu_lockstep.c (milestone 2). */

/* SPURS task-launch interception at a trampoline hop (milestone: SPURS kernel).
 * No-op until then. */

/* PM flow trace (SPURS_PM_FLOW=1): record every cross-function transfer of ONE
 * policy-module run (the ctx spurs_policy.c arms) so the post-claim decision
 * path can be reconstructed offline. Written by the drain-loop hook below;
 * armed/dumped by spu_run_policy_module. */
uint32_t          g_pm_flow_buf[8192];
volatile unsigned g_pm_flow_n = 0;
void* volatile    g_pm_flow_ctx = 0;

#include <stdlib.h>
#include <stdio.h>

/* Sync-model ring-drain: the wwsjob PM's job ring drains only when jobs mark
 * their record's "done" bit (0x8000 in the record's first halfword @LS 0xDF0).
 * On hardware the async load-completion INTERRUPT sets it; our loads complete
 * synchronously, so a job whose code never got a ring slot stays "pending"
 * forever and the ring deadlocks (scan re-bails at LS 0x2318). When we observe
 * that bail spin, the in-flight loads HAVE in fact completed in our model, so
 * set the done bit on the pending records -- the faithful sync equivalent of
 * the completion interrupt. Env SPU_JOBDRAIN (default off while validating). */
extern void spu_halt(spu_context*);
/* Last 32 drain steps on this host thread (pc), for post-mortems of a run that
 * ended where it should not have (spurs_policy.c, [pm-end]). */
static _Thread_local uint32_t t_recent[32];
static _Thread_local uint32_t t_recent_n;
unsigned spu_recent_pcs(uint32_t* out, unsigned max)
{
    unsigned n = t_recent_n < 32 ? t_recent_n : 32, k = n < max ? n : max;
    for (unsigned i = 0; i < k; i++) out[i] = t_recent[(t_recent_n - k + i) & 31];
    return k;
}

/* GOWA: decided on the first step (spu_task_launch_check); read inline by
 * spu_drain_step_hooks. 1 = the full per-step checks below (any of the
 * diagnostics, or GOWA_TLC_FAST=0); 0 = the inline fast path, which comes
 * here only when there is something to check. */
int g_spu_tlc_slow = -1;

/* GOWA: SPU_TAIL budget per drain step (spu_context.h). GOWA_SPU_TAIL=<n>,
 * 0 = every direct transfer through the trampoline. Set at the first
 * spu_drain_call; until then the default. */
uint32_t g_spu_tail_budget = 32;

void spu_task_launch_ls0_end(spu_context* ctx)
{
    static int _n = 0;
    if (_n < 8 && _n++ < 8)
        fprintf(stderr, "[spurs-job] img=%d branched to LS 0 -- job complete\n", ctx->image_id);
    spu_halt(ctx);
}

/* GOWA_DRAIN_HIST=1: every trampoline step (both drain loops) counted by
 * (image, target pc), with the lifted function's RVA in the exe (resolve with
 * port/build/gowa.map); every ~10 s the 30 biggest. Turns g_spu_tlc_slow to 2,
 * which sends each step here; the fast path below is unchanged. Racy counters:
 * a diagnostic. */
#define SPU_DH_N 8192
static struct { uint32_t key, img; uintptr_t fn; uint32_t n; } s_dh[SPU_DH_N];
static __attribute__((noinline)) void spu_drain_hist_note(spu_context* ctx, void* fn)
{
    const uint32_t pc = (uint32_t)ctx->pc & SPU_LS_MASK, img = (uint32_t)ctx->image_id;
    const uint32_t key = pc ^ (img << 18);
    uint32_t h = (key * 2654435761u) >> 19;
    for (int p = 0; p < 64; p++, h = (h + 1) & (SPU_DH_N - 1)) {
        if (s_dh[h].n && (s_dh[h].key != key || s_dh[h].img != img)) continue;
        if (!s_dh[h].n) { s_dh[h].key = key; s_dh[h].img = img; s_dh[h].fn = (uintptr_t)fn; }
        s_dh[h].n++;
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
    for (int i = 0; i < SPU_DH_N; i++) tot += s_dh[i].n;
    extern char __ImageBase;
    fprintf(stderr, "[drain-hist] %.2f M passos em 10 s\n", tot / 1e6);
    for (int r = 0; r < 30 && tot; r++) {
        int bi = -1; uint32_t best = 0;
        for (int i = 0; i < SPU_DH_N; i++) if (s_dh[i].n > best) { best = s_dh[i].n; bi = i; }
        if (bi < 0) break;
        fprintf(stderr, "[drain-hist]   img=%d pc=0x%05X fn=RVA 0x%08llX  %5.2f%%\n", (int)s_dh[bi].img,
                (s_dh[bi].key ^ (s_dh[bi].img << 18)) & SPU_LS_MASK,
                (unsigned long long)(s_dh[bi].fn - (uintptr_t)&__ImageBase), best * 100.0 / tot);
        s_dh[bi].n = 0;
    }
    memset(s_dh, 0, sizeof s_dh);
}

void spu_task_launch_check(spu_context* ctx, void* fn)
{
    extern void spu_check_stack_reset(spu_context*, void (*)(spu_context*));
    extern volatile unsigned g_spu_stack_reset_count;
    if (__builtin_expect(g_spu_tlc_slow < 0, 0)) {
        const char* f = getenv("GOWA_TLC_FAST");
        g_spu_tlc_slow = (getenv("SPU_NO_LS0_END") || getenv("SPU_EXITTRACE") || getenv("SPU_STEPTRACE") ||
                          getenv("SPU_JOBDRAIN") || (f && *f == '0')) ? 1 : 0;
        if (!g_spu_tlc_slow && getenv("GOWA_DRAIN_HIST")) g_spu_tlc_slow = 2;
    }
    if (__builtin_expect(g_spu_tlc_slow != 1, 1)) {
        if (g_spu_tlc_slow == 2) spu_drain_hist_note(ctx, fn);
        if (g_spu_stack_reset_count) spu_check_stack_reset(ctx, (void (*)(spu_context*))fn);
        if (g_pm_flow_ctx == (void*)ctx && g_pm_flow_n < 8192) g_pm_flow_buf[g_pm_flow_n++] = ctx->pc;
        if (ctx->steps++ && (ctx->pc & SPU_LS_MASK) == 0 && !ctx->policy_mode && ctx->image_id > 0)
            spu_task_launch_ls0_end(ctx);
        return;
    }
    t_recent[t_recent_n++ & 31] = (uint32_t)ctx->pc & SPU_LS_MASK;
    spu_check_stack_reset(ctx, (void (*)(spu_context*))fn);
    /* A SPURS job returning to LS 0 is finished -- its crt tail-jumps to the
     * resident job manager, and with the job loaded at 0 that lands on its own
     * entry. Planting a return address in r0 catches the jobs that get there
     * via `bi $r0`, but some branch to 0 DIRECTLY, which the lifter turns into
     * a plain trampoline that never reaches spu_indirect_branch. The drain sees
     * every step, so catch it here too. Step 0 is the real entry. */
    static int s_no_ls0 = -1;
    if (s_no_ls0 < 0) s_no_ls0 = getenv("SPU_NO_LS0_END") ? 1 : 0;
    /* SPU_EXITTRACE=<img>: remember the last PCs this image executed and dump
     * them when the job ends. "Why did it exit here" is a question about the
     * path taken, and a forward trace from the entry never reaches far enough
     * to show it. */
    static int64_t s_et = -2;
    if (s_et == -2) { const char* e = getenv("SPU_EXITTRACE");
                      s_et = e ? strtol(e, 0, 0) : -1; }
    static uint32_t s_ring[64]; static uint32_t s_ri;
    if (s_et >= 0 && ctx->image_id == s_et)
        s_ring[s_ri++ & 63] = (uint32_t)ctx->pc & SPU_LS_MASK;

    if (!s_no_ls0 && ctx->steps++ && (ctx->pc & SPU_LS_MASK) == 0 &&
        !ctx->policy_mode && ctx->image_id > 0) {
        static int _n = 0;
        if (_n < 8 && _n++ < 8)
            fprintf(stderr, "[spurs-job] img=%d branched to LS 0 -- job complete\n",
                    ctx->image_id);
        if (s_et >= 0 && ctx->image_id == s_et) {
            fprintf(stderr, "[spu-exit] img=%d last PCs:", ctx->image_id);
            for (int i = 32; i >= 1; i--)
                fprintf(stderr, " %05X", s_ring[(s_ri - i) & 63]);
            fputc(10, stderr); fflush(stderr);
        }
        spu_halt(ctx);
        return;
    }
    /* SPU_STEPTRACE=<img>: pc and the argument registers at every trampoline
     * step. This runs on each drain iteration, so it is the finest-grained
     * view of a register file changing under a running job. */
    { static int64_t s_t = -2;
      if (s_t == -2) { const char* e = getenv("SPU_STEPTRACE"); s_t = e ? strtol(e,0,0) : -1; }
      static int64_t s_from = -2;
      if (s_from == -2) { const char* e = getenv("SPU_STEPTRACE_FROM");
                          s_from = e ? strtol(e,0,16) : -1; }
      static int s_armed = 0;
      if (s_t >= 0 && ctx->image_id == s_t) {
          if (s_from >= 0 && !s_armed &&
              ((uint32_t)ctx->pc & SPU_LS_MASK) == (uint32_t)s_from) s_armed = 1;
          static int n = 0;
          if ((s_from < 0 || s_armed) && n++ < 40) {
              fprintf(stderr, "[step] pc=0x%05X r1=0x%05X r2=0x%08X r3=0x%08X r4=0x%08X\n",
                      (uint32_t)ctx->pc & SPU_LS_MASK, ctx->gpr[1]._u32[0],
                      ctx->gpr[2]._u32[0], ctx->gpr[3]._u32[0], ctx->gpr[4]._u32[0]);
              fflush(stderr);
          }
      } }
    if (g_pm_flow_ctx == (void*)ctx && g_pm_flow_n < 8192)
        g_pm_flow_buf[g_pm_flow_n++] = ctx->pc;

    static int s_on = -1;
    if (s_on < 0) s_on = getenv("SPU_JOBDRAIN") ? 1 : 0;
    if (!s_on || ctx->image_id != 2 || !ctx->policy_mode) return;

    /* 0x2318 = the type-2 load's ring-full bail (resets scan, retries). */
    if ((ctx->pc & 0x3FFFF) != 0x2318) return;

    static unsigned s_bail = 0;
    if (++s_bail < 4096) return;      /* only after a real spin, not one pass */
    s_bail = 0;

    /* Set bit 31 (the 0x8000 first-halfword done flag) on each pending record
     * the release sweep scans (16 slots, 4-byte stride at LS 0xDF0), for any
     * record that is non-empty and not already done. */
    int marked = 0;
    for (int i = 0; i < 16; i++) {
        uint8_t* r = &ctx->ls[0xDF0 + i * 4];
        uint32_t w = ((uint32_t)r[0] << 24) | ((uint32_t)r[1] << 16) |
                     ((uint32_t)r[2] << 8) | r[3];
        if (w == 0 || (w & 0x80000000u)) continue;
        w |= 0x80000000u;
        r[0] = (uint8_t)(w >> 24); r[1] = (uint8_t)(w >> 16);
        r[2] = (uint8_t)(w >> 8);  r[3] = (uint8_t)w;
        marked++;
    }
    static int _n = 0;
    if (marked && _n++ < 12)
        fprintf(stderr, "[pm-jobdrain] ring-full spin: marked %d pending records done\n", marked);
}

/* Restore the active image after a lifted call bracket. The brsl/bisl emission
 * saves image_id in a call-site local and hands it back here so an image
 * adopted inside the callee cannot leak into the caller's continuation. A
 * persistent LS-0xA00 workload module (module_img_a00) is re-applied since the
 * plain restore would undo it. */
void spu_img_restore(spu_context* ctx, int32_t saved_img)
{
    ctx->image_id = ctx->module_img_a00 ? ctx->module_img_a00 : saved_img;
}

/* spu_ch_wake now has its real body in spu_channels.c (milestone 3). */

/* --- SPU interrupt dispatch (drain-loop hook) ------------------------------
 * Called when int_enable && (event_status & event_mask): architectural SPU
 * interrupt. srr0 <- the interrupted continuation (ctx->pc already holds the
 * pending transfer target), interrupts disable, and control vectors through
 * the guest-planted branch instruction at LS 0 (the WWS jobmanager's entry
 * writes `bra 0xA2C` there; its handler drives the DMA-list job pipeline and
 * returns via iret). If LS 0 holds no br/bra, the interrupt is NOT taken --
 * we log once and resume normal flow rather than jump into zeros. */
#include <stdio.h>
#include <stdlib.h>

/* ---- Deferred DMA-list stall-and-notify delivery --------------------------
 * Real MFC hardware processes a queued list command (the WWS job manager's
 * barriered null list, MFC_GETLB) ASYNCHRONOUSLY: the stall-and-notify -- and
 * the interrupt it raises -- land after the SPU has run on past the code that
 * armed it. We execute the list synchronously inside `wrch MFC_Cmd`, so the
 * interrupt was delivered before the job manager had stored
 * g_WwsJob_loadJobState = kReadCommands (LS 0x12A0).
 *
 * Its interrupt handler gates the Load->Run advance on BOTH the ch25 stall mask
 * carrying tag 0 (kLoadJob_readCommands) AND loadJobState == kReadCommands. With
 * the state still kNone it correctly declined to advance -- and because
 * MFC_RdListStallStat is "accumulative, clear on read" that single stall was
 * consumed, so no later interrupt could ever advance it: the job never ran and
 * LBP's loading thread deadlocked on a completion that never came.
 *
 * Holding the interrupt off for N drain ticks was tried and DISPROVEN: a probe
 * at the stall site shows loadJobState is ALREADY 0 when the GETLB is issued, so
 * the arming store never ran on this path at all -- it is not an interrupt-timing
 * race. (The pm takes trychangefreetoloadjob's early-exit `ceqhi jobHeader,1 /
 * brhz cond1,Exit` to LS 0x2D30, skipping the Load setup.) Default OFF; kept
 * env-gated for experiments: SPU_SN_DEFER=<n> ticks (0 = immediate, faithful
 * to the current synchronous list execution). */
void* volatile   g_sn_defer_ctx = 0;
volatile unsigned g_sn_defer     = 0;

unsigned spu_sn_defer_ticks(void)
{
    static int s_n = -1;
    if (s_n < 0) { const char* e = getenv("SPU_SN_DEFER"); s_n = e ? atoi(e) : 0; }
    return (unsigned)(s_n < 0 ? 0 : s_n);
}

/* ---- Interrupt register preservation --------------------------------------
 * Hardware contract: an SPU interrupt handler preserves every register (the
 * WWS jobmanager guarantees it with a save/restore shim it installs at the
 * TOP of LS, ~0x3FEC0 -- interrupthandlerasm.spu.s). That shim lies OUTSIDE
 * the PM image our lifter processed, so the lifted handler runs WITHOUT the
 * save/restore and eats the interrupted code's live registers. Measured
 * mid-GetBufferTag: the fsmbi mask registers (r13/r15) arrived zeroed at the
 * packing selbs, the BufferTag's lanes collapsed, the job saw a NULL buffer
 * and bailed -- LBP's loading freeze.
 *
 * Enforce the contract host-side: snapshot the full GPR file when the
 * interrupt is taken, restore it when the handler irets (detected in
 * spu_indirect_branch: pc == saved srr0 with interrupts re-enabled -- during
 * the handler int_enable stays 0, so the first enabled dispatch at srr0 IS
 * the irete). State the handler legitimately publishes lives in LS/channels
 * and is untouched. Per-ctx slots; no nesting (interrupts stay disabled
 * until iret). */
static void spu_irq_regs_save(spu_context* ctx)
{
    ctx->irq_saved = 1;
    ctx->irq_resume_pc = ctx->srr0;
    memcpy(ctx->irq_gpr, ctx->gpr, sizeof ctx->irq_gpr);
}

/* Called from spu_indirect_branch on every dispatch. Restores + clears when
 * the iret lands. Returns 1 if a restore happened (diagnostic). */
/* Drop any saved register file belonging to this context. The save slots are
 * keyed by RAW POINTER, and an spu_context is typically a stack local -- so a
 * later, unrelated run lands on the same address and inherits the earlier
 * context's registers wholesale. A fresh run must never do that. */
void spu_irq_regs_forget(spu_context* ctx)
{
    ctx->irq_saved = 0;
}

int spu_irq_regs_maybe_restore(spu_context* ctx)
{
    {
        if (ctx->irq_saved) {
            if ((ctx->pc & SPU_LS_MASK) == (ctx->irq_resume_pc & SPU_LS_MASK) &&
                ctx->int_enable) {
                memcpy(ctx->gpr, ctx->irq_gpr, sizeof ctx->irq_gpr);
                ctx->irq_saved = 0;
                /* The iret completed below the frame that took the interrupt:
                 * abandon the handler's host frames and resume there. */
                spu_irq_frame* f = (spu_irq_frame*)ctx->irq_frame;
                if (f && ctx->host_depth <= f->depth)
                    ctx->irq_frame = f->prev;      /* returned to its loop the ordinary way */
                if (f && ctx->host_depth > f->depth) {
                    { static int s_it = -1; if (s_it < 0) s_it = getenv("SPU_IRQTRACE") ? 1 : 0;
                      static int _n = 0;
                      if (s_it && _n++ < 200)
                          fprintf(stderr, "[irq] IRET at depth %u unwinds to taking frame depth %u (srr0=0x%05X)\n",
                                  ctx->host_depth, f->depth, ctx->pc & SPU_LS_MASK); }
                    SPU_HALT_LONGJMP(f->env, 1);
                }
                if (!f && ctx->host_depth > 0) {
                    /* Taken by the top-level driver loop (depth 0), which
                     * keeps no frame: the driver's own restart re-enters at
                     * srr0 with the host stack empty, which is that frame. */
                    { static int s_it = -1; if (s_it < 0) s_it = getenv("SPU_IRQTRACE") ? 1 : 0;
                      static int _n = 0;
                      if (s_it && _n++ < 200)
                          fprintf(stderr, "[irq] IRET at depth %u unwinds to the driver (srr0=0x%05X)\n",
                                  ctx->host_depth, ctx->pc & SPU_LS_MASK); }
                    extern void spu_restart_dispatch(spu_context*);
                    spu_restart_dispatch(ctx);
                }
                return 1;
            }
            return 0;
        }
    }
    return 0;
}

void (*spu_take_interrupt(spu_context* ctx,
                          void (*tf)(spu_context*)))(spu_context*)
{
    /* Stall-and-notify still settling: let the SPU run on (see above). */
    if (g_sn_defer && g_sn_defer_ctx == (void*)ctx) {
        if (--g_sn_defer == 0) g_sn_defer_ctx = 0;
        return tf;
    }
    const uint8_t* v = ctx->ls;      /* vector word at LS 0, big-endian */
    uint32_t w = ((uint32_t)v[0] << 24) | ((uint32_t)v[1] << 16) |
                 ((uint32_t)v[2] << 8) | v[3];
    uint32_t op9 = w >> 23;          /* top 9 opcode bits */
    uint32_t target;
    if (op9 == 0x060) {              /* bra i16 (absolute) */
        target = ((w >> 7) & 0xFFFF) << 2;
    } else if (op9 == 0x064) {       /* br i16 (relative to LS 0) */
        target = (((w >> 7) & 0xFFFF) << 2) & SPU_LS_MASK;
    } else {
        static int _w = 0;
        if (_w < 4 && _w++ < 4)
            fprintf(stderr, "[spu-int] pending (st=0x%X mask=0x%X) but LS0 word "
                    "0x%08X is no branch -- not taken\n",
                    ctx->event_status, ctx->event_mask, w);
        return tf;
    }
    ctx->srr0 = ctx->pc;             /* resume point for iret */
    ctx->int_enable = 0;
    spu_irq_regs_save(ctx);          /* hardware contract: handler preserves regs */
    { static int s_it = -1; if (s_it < 0) s_it = getenv("SPU_IRQTRACE") ? 1 : 0;
      if (s_it) { static int _n = 0; if (_n < 200 && _n++ < 200)
        fprintf(stderr, "[irq] TAKE srr0=0x%05X depth=%d r13=%08X r15=%08X\n",
                ctx->srr0 & SPU_LS_MASK, ctx->host_depth,
                ctx->gpr[13]._u32[0], ctx->gpr[15]._u32[0]); } }
    ctx->pc = target & SPU_LS_MASK;
    { static int _n = 0;
      if (_n < 16 && _n++ < 16) {
          /* WWS Load->Run diagnosis: the job-manager interrupt handler advances
           * g_WwsJob_loadJobState kReadCommands(1)->kExecuteCommands(2) only if
           * (a) the ch25 stall mask has tag 0 (kLoadJob_readCommands) AND
           * (b) loadJobState's HALFWORDS compare equal to 1 (ceqhi).
           * Log both at interrupt entry so we can see which test fails. */
          const uint8_t* js = ctx->ls + 0x12A0;   /* g_WwsJob_loadJobState quadword */
          fprintf(stderr, "[spu-int] TAKEN img=%d events=0x%X&0x%X vector->0x%05X "
                  "(srr0=0x%05X) stallstat=0x%X parked=0x%X loadJobState@12A0=%02X%02X%02X%02X "
                  "%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X\n", ctx->image_id,
                  ctx->event_status, ctx->event_mask, ctx->pc, ctx->srr0,
                  ctx->list_stall_stat, ctx->list_stall_mask,
                  js[0],js[1],js[2],js[3], js[4],js[5],js[6],js[7],
                  js[8],js[9],js[10],js[11], js[12],js[13],js[14],js[15]); } }
    return spu_indirect_branch;
}

/* See the declaration in spu_context.h. */
uint32_t g_spu_depth_max = 0;   /* see spu_depth_guard (spu_context.h) */
void spu_depth_guard_slow(spu_context* ctx)
{
    static uint32_t s_max = 0;
    static int s_ring_on = -1;
    if (!s_max) {
        const char* e = getenv("SPU_HOST_DEPTH_MAX");
        s_max = e ? (uint32_t)strtoul(e, 0, 0) : 2000u;
        if (!s_max) s_max = 2000u;
        s_ring_on = getenv("SPU_DEPTH_RING") ? 1 : 0;
        g_spu_depth_max = s_ring_on ? 0u : s_max;
    }

    /* Ring of recent drain sites. When the depth trips, the useful question is
     * not "where are we" but "what cycle got us here" -- one pc names a point,
     * a ring names the loop. GOWA: only with SPU_DEPTH_RING=1. */
    enum { RING = 24 };
    /* GOWA: per thread. As plain statics every SPU thread wrote the same
     * cache line on every drain call, ~2% of each in God of War Ascension. */
    static SPU_THREAD_LOCAL uint32_t s_ring[RING];
    static SPU_THREAD_LOCAL uint32_t s_n;
    if (s_ring_on > 0) {
        s_ring[s_n % RING] = ((uint32_t)ctx->pc & SPU_LS_MASK);
        s_n++;
    }

    if (ctx->host_depth < s_max) return;
    static int s_reported = 0;
    if (s_reported < 1) {   /* one report: five SPU threads all trip together */
        s_reported++;
        fprintf(stderr, "[spu-depth] img=%d pc=0x%05X lr=0x%05X host_depth=%u"
                        " -- lifted call recursion, halting the SPU\n",
                ctx->image_id, (uint32_t)ctx->pc & SPU_LS_MASK,
                ctx->gpr[0]._u32[0] & SPU_LS_MASK, ctx->host_depth);
        fprintf(stderr, "[spu-depth] recent drain sites (oldest first):");
        uint32_t start = (s_n > RING) ? (s_n - RING) : 0;
        for (uint32_t k = start; k < s_n; k++)
            fprintf(stderr, " %05X", s_ring[k % RING]);
        fprintf(stderr, "\n");
        fflush(stderr);
    }
    { extern void spu_halt(spu_context*); spu_halt(ctx); }
}

/* See spu_context.h. Env-gated: SPU_TAILRET=1. */
int g_spu_tailret_on = -1;
int spu_tailret_enabled(void)
{
    if (g_spu_tailret_on < 0) { const char* e = getenv("SPU_TAILRET"); g_spu_tailret_on = (e && e[0] == 0x31) ? 1 : 0; }
    return g_spu_tailret_on;
}

/* SPU_PCHIST (see spu_pchist_tick in spu_context.h) */
int g_spu_pchist_on = -1;
void spu_pchist_tick_slow(const spu_context* ctx)
{
    if (g_spu_pchist_on < 0) g_spu_pchist_on = getenv("SPU_PCHIST") ? 1 : 0;
    if (!g_spu_pchist_on) return;
    static unsigned long long b[8][8], n;
    const unsigned sp = (unsigned)(ctx->spu_id & 7u);
    const uint32_t pc = (uint32_t)ctx->pc & SPU_LS_MASK;
    const int k = pc < 0x00188u ? 0
                : pc < 0x00350u ? 1     /* BlockClear */
                : pc < 0x00758u ? 2     /* Host2Local_Body */
                : pc < 0x00928u ? 3     /* Host2Local */
                : pc < 0x010F8u ? 4     /* Local2Local */
                : pc < 0x051E8u ? 5     /* main */
                : pc < 0x08700u ? 6     /* DrawRect */
                : pc < 0x1493Cu ? 7     /* DrawEdge */
                : 0;
    b[sp][k]++;
    if ((++n % 4000000ull) == 0) {
        static const char* nm[8] = { "other", "BlockClear", "H2L_Body",
            "Host2Local", "Local2Local", "main", "DrawRect", "DrawEdge" };
        fprintf(stderr, "[pchist] %llu hops\n", n);
        for (unsigned q = 0; q < 8; q++) {
            int any = 0;
            for (int j = 0; j < 8; j++) if (b[q][j]) any = 1;
            if (!any) continue;
            fprintf(stderr, "   spu%u", q);
            for (int j = 0; j < 8; j++)
                if (b[q][j]) fprintf(stderr, " %s=%llu", nm[j], b[q][j]);
            fprintf(stderr, "\n");
        }
        fflush(stderr);
    }
}


/* A return may use any register, not only r0. Running that branch target
 * inside this drain executes the caller's continuation twice (once here,
 * once when its real host frame resumes), including its stack adjustment.
 *
 * One kind of "wrong" pc is safe to run here, and must be: a lifted ENTRY
 * that has no host frame of its own. Every WWS job module starts with
 *
 *     ila $r0, entry+0xAC ; a $r0, $r0, $r126 ; br body
 *
 * so the body ends `bi $r0` at a point the entry function tail-jumped to and
 * whose host frame is therefore gone: the continuation exists only as the
 * lifted function seeded at that address. Unwinding instead (the restart
 * below) leaves the manager unable to resume at ITS return point, which is a
 * plain r0 drain with no entry, and the job ends on a garbage r0. So when the
 * callee comes back on a lifted entry, run it in this drain until the pc
 * reaches return_pc; only a pc with no lifted code behind it is unwound. */
typedef void (*spu_drain_fn)(spu_context*);
extern spu_drain_fn spu_lookup(uint32_t addr, int image_id);

int g_spu_drain_inline = 1;   /* see spu_drain_call_inl (spu_context.h) */
/* GOWA_TRAMP_STATS=<start s>:<duration s> (diagnostic): count the drain
 * loop's trampoline targets (every lifted-function transfer that went back to
 * this loop instead of a direct call) in that window and print the top 40 as
 * exe RVAs ([tramp]; resolve with gowa.map) -- which transfers to merge. */
#include <windows.h>
static int g_tramp_stats = -1;
static volatile LONGLONG s_tramp_t0, s_tramp_t1;
static void* volatile s_tramp_key[16384];
static volatile LONG s_tramp_n[16384];
static volatile LONG s_tramp_total, s_tramp_dumped;
/* GOWA (24/10): the indirect branches also by target -- (image or job overlay, LS pc) */
static volatile LONG64 s_tib_key[8192];
static volatile LONG s_tib_n[8192];
static volatile LONG s_tib_total;
static void spu_tramp_note(void* fn, const spu_context* ctx)
{
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    if (now.QuadPart < s_tramp_t0) return;
    if (now.QuadPart > s_tramp_t1) {
        if (InterlockedExchange(&s_tramp_dumped, 1)) return;
        HMODULE self = GetModuleHandleA(NULL);
        fprintf(stderr, "[tramp] %ld transferencias pelo dreno na janela; destinos mais frequentes:\n", s_tramp_total);
        for (int r = 0; r < 40; r++) {
            int best = -1;
            for (int k = 0; k < 16384; k++) if (s_tramp_n[k] && (best < 0 || s_tramp_n[k] > s_tramp_n[best])) best = k;
            if (best < 0) break;
            fprintf(stderr, "[tramp] %5.2f%%  rva 0x%08llX\n", 100.0 * s_tramp_n[best] / (s_tramp_total ? s_tramp_total : 1),
                    (unsigned long long)((uintptr_t)s_tramp_key[best] - (uintptr_t)self));
            s_tramp_n[best] = 0;
        }
        fprintf(stderr, "[tramp-ib] %ld desvios indiretos; destinos (imagem, LS) mais frequentes:\n", s_tib_total);
        for (int r = 0; r < 30; r++) {
            int best = -1;
            for (int k = 0; k < 8192; k++) if (s_tib_n[k] && (best < 0 || s_tib_n[k] > s_tib_n[best])) best = k;
            if (best < 0) break;
            fprintf(stderr, "[tramp-ib] %5.2f%%  img %d LS 0x%05X\n", 100.0 * s_tib_n[best] / (s_tib_total ? s_tib_total : 1),
                    (int)(s_tib_key[best] >> 32), (unsigned)(s_tib_key[best] & 0x3FFFF));
            s_tib_n[best] = 0;
        }
        return;
    }
    if (fn == (void*)spu_indirect_branch && ctx) {
        InterlockedIncrement(&s_tib_total);
        const LONG64 key = ((LONG64)(ctx->resident_ovl ? ctx->resident_ovl : ctx->image_id) << 32) | (ctx->pc & SPU_LS_MASK) | 0x40000;
        uint32_t h = (uint32_t)(((uint64_t)key * 0x9E3779B97F4A7C15ull) >> 51) & 8191u;
        for (int p = 0; p < 64; p++, h = (h + 1) & 8191u) {
            const LONG64 k = s_tib_key[h];
            if (k == key) { InterlockedIncrement(&s_tib_n[h]); break; }
            if (!k) {
                if (InterlockedCompareExchange64(&s_tib_key[h], key, 0) == 0 || s_tib_key[h] == key) { InterlockedIncrement(&s_tib_n[h]); break; }
            }
        }
    }
    InterlockedIncrement(&s_tramp_total);
    uint32_t h = (uint32_t)(((uintptr_t)fn * 0x9E3779B97F4A7C15ull) >> 50) & 16383u;
    for (int p = 0; p < 64; p++, h = (h + 1) & 16383u) {
        void* k = s_tramp_key[h];
        if (k == fn) { InterlockedIncrement(&s_tramp_n[h]); return; }
        if (!k) {
            if (InterlockedCompareExchangePointer((void* volatile*)&s_tramp_key[h], fn, NULL) == NULL ||
                s_tramp_key[h] == fn) { InterlockedIncrement(&s_tramp_n[h]); return; }
        }
    }
}
static void spu_tramp_init(void)
{
    const char* e = getenv("GOWA_TRAMP_STATS");
    if (!e) { g_tramp_stats = 0; return; }
    LARGE_INTEGER f, now; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&now);
    const char* c = strchr(e, ':');
    s_tramp_t0 = now.QuadPart + (LONGLONG)(atof(e) * (double)f.QuadPart);
    s_tramp_t1 = s_tramp_t0 + (LONGLONG)((c ? atof(c + 1) : 10.0) * (double)f.QuadPart);
    g_tramp_stats = 1;
}
void spu_drain_call(spu_context* ctx, uint32_t return_pc)
{
    { static int s_tb = 0;
      if (__builtin_expect(!s_tb, 0)) { const char* e = getenv("GOWA_SPU_TAIL");
                                        if (e) g_spu_tail_budget = (uint32_t)strtoul(e, 0, 10);
                                        e = getenv("GOWA_DRAIN_INLINE"); if (e && *e == '0') g_spu_drain_inline = 0;
                                        s_tb = 1; } }
    spu_depth_guard(ctx);
    /* The frame an interrupt taken in THIS loop returns to. It stays armed
     * after the handler's first host function returns -- the handler keeps
     * running as trampolines of this loop -- until its iret fires or the
     * loop exits, whichever first. */
    spu_irq_frame f;
    f.prev = 0; f.depth = 0; f.image_id = 0;
    uint32_t given = 0;   /* GOWA_TAIL_TICKS: the budget handed to the last step */
#define SPU_DRAIN_POP_IRQ() do { if (ctx->irq_frame == &f) ctx->irq_frame = f.prev; ctx->drain_ret_pc = 0; } while (0)
    for (;;) {
        while (g_spu_trampoline_fn) {
            if (g_spu_trampoline_fn == spu_indirect_branch &&
                (ctx->pc & SPU_LS_MASK) == (return_pc & SPU_LS_MASK)) {
                g_spu_trampoline_fn = 0;
                SPU_DRAIN_POP_IRQ();
                return;
            }
            void (*fn)(spu_context*) = g_spu_trampoline_fn;
            g_spu_trampoline_fn = 0;
            if (__builtin_expect(g_tramp_stats != 0, 0)) {
                if (g_tramp_stats < 0) spu_tramp_init();
                if (g_tramp_stats > 0) spu_tramp_note((void*)fn, ctx);
            }
            ctx->drain_ret_pc = return_pc & SPU_LS_MASK;   /* re-set: a nested drain changed it */
            spu_drain_step_hooks(ctx, (void*)fn);
            /* Event latency on the LIFTED path too (see the interpreter's
             * GOWA_IRQ_DELAY in spu_interp.c): our DMA completes inside the wrch
             * that issued it, so a tag/list event is pending at the very next
             * dispatch, where hardware would still be transferring. God of War
             * Ascension's job module 0x6FC080 (function 0x12380) waits on a tag
             * with interrupts off and re-enables them with `bi [E]`; taken right
             * there, the WWS handler ran before the code that follows and the
             * job looped forever, which is why that module had to stay
             * interpreted. An event becomes an interrupt only after it has been
             * pending for GOWA_IRQ_DELAY_LIFTED drain ticks (default 64;
             * 0 = take it at once, the old behaviour). */
            static int s_lirqd = -1;
            if (s_lirqd < 0) { const char* e = getenv("GOWA_IRQ_DELAY_LIFTED"); s_lirqd = e ? atoi(e) : 64; }
            static SPU_THREAD_LOCAL uint64_t t_lticks, t_lpend_since; static SPU_THREAD_LOCAL int t_lpend;
            if (given) {   /* GOWA_TAIL_TICKS: the last step's direct transfers */
                t_lticks += given > ctx->tail_budget ? given - ctx->tail_budget : 0u;
                given = 0;
            }
            t_lticks++;
            { const uint32_t pend_now = ctx->event_status & ctx->event_mask;
              if (pend_now) { if (!t_lpend) { t_lpend = 1; t_lpend_since = t_lticks; } }
              else t_lpend = 0; }
            if (ctx->int_enable && (ctx->event_status & ctx->event_mask) &&
                (s_lirqd <= 0 || t_lticks - t_lpend_since >= (uint64_t)s_lirqd)) {
                void (*vf)(spu_context*) = spu_take_interrupt(ctx, fn);
                if (!ctx->int_enable) {   /* taken (a take clears the enable; a deferral leaves it) */
                    /* Interrupt taken here: this loop is the frame the iret
                     * comes back to, however deep the handler leaves from. */
                    if (ctx->irq_frame != &f) {
                        f.prev = (spu_irq_frame*)ctx->irq_frame;
                        f.depth = ctx->host_depth;
                        f.image_id = ctx->image_id;
                        ctx->irq_frame = &f;
                    }
                    if (SPU_HALT_SETJMP(f.env) == 0) {
                        vf(ctx);
                    } else {
                        /* iret fired deeper: registers restored, pc = srr0 */
                        ctx->host_depth = f.depth;
                        ctx->image_id = f.image_id;
                        g_spu_trampoline_fn = spu_indirect_branch;
                        SPU_DRAIN_POP_IRQ();
                    }
                    continue;
                }
            }
            /* GOWA (21/10, SPU_TAILX): an event pending = budget 0, so the lean direct
             * transfers come back here (the interrupt delay counts drain steps) */
            /* GOWA (24/10): GOWA_TAIL_TICKS=1 -- with an event pending the budget was 0, so
             * every transfer came back here: the EDGE job's DMA waits (tag event pending,
             * interrupts off: 5E48..5E6C) and the GOWA_IRQ_DELAY_LIFTED window after it
             * enables them (53B0..53E0) ran one instruction per drain step, ~37 M drain
             * transfers a second in chapter 16. Now the direct transfers of a step count as
             * ticks (above), so the budget can run to the end of the delay -- the interrupt
             * comes the same number of transfers after the event as before (A/B abtail,
             * 21/10: direct transfers NOT counted made it come far later, 34 -> 22 FPS) --
             * and with interrupts off nothing can be taken: a full budget. */
            { static int s_tt = -1;
              if (s_tt < 0) { const char* e = getenv("GOWA_TAIL_TICKS"); s_tt = e && *e == '1'; }
              uint32_t bud = g_spu_tail_budget;
              if (ctx->event_status & ctx->event_mask) {
                  if (!s_tt) bud = 0;
                  else if (ctx->int_enable) {
                      const uint64_t el = t_lpend ? t_lticks - t_lpend_since : 0;
                      bud = (s_lirqd > 0 && el < (uint64_t)s_lirqd) ? (uint32_t)((uint64_t)s_lirqd - el) : 0u;
                      if (bud > g_spu_tail_budget) bud = g_spu_tail_budget;
                  }
              }
              ctx->tail_budget = bud;
              given = s_tt ? bud : 0u; }
            { extern int g_gowa_slow_spu; extern void gowa_slow_tick(int);
              if (__builtin_expect(g_gowa_slow_spu != 0, 0)) {
                  if (g_gowa_slow_spu < 0) { const char* e = getenv("GOWA_SLOW_SPU_PCT"); g_gowa_slow_spu = e ? atoi(e) : 0; }
                  if (g_gowa_slow_spu > 0) { extern int gowa_slow_sel(const void*); extern void gowa_slow_tick_sel(int, int);
                                             gowa_slow_tick_sel(g_gowa_slow_spu, gowa_slow_sel(ctx)); }
              } }
            fn(ctx);
        }
        if ((ctx->pc & SPU_LS_MASK) == (return_pc & SPU_LS_MASK)) {
            SPU_DRAIN_POP_IRQ();
            return;
        }
        {
            uint32_t pc = ctx->pc & SPU_LS_MASK;
            spu_drain_fn fn = ctx->resident_ovl ? spu_lookup(pc, ctx->resident_ovl) : 0;
            if (!fn) fn = spu_lookup(pc, ctx->image_id);
            if (fn) {
                { static int _n = 0; if (_n < 8 && _n++ < 8)
                    fprintf(stderr, "[spu] drain-resume return_pc=0x%05X at lifted entry 0x%05X img=%d depth=%d ovl=%d\n",
                            (unsigned)(return_pc & SPU_LS_MASK), pc, ctx->image_id, ctx->host_depth,
                            (int)ctx->resident_ovl); }
                g_spu_trampoline_fn = spu_indirect_branch;
                continue;
            }
        }
        {
            extern void spu_restart_dispatch(spu_context*);
            SPU_DRAIN_POP_IRQ();
            spu_restart_dispatch(ctx);
            return;
        }
    }
#undef SPU_DRAIN_POP_IRQ
}

/* GOWA: MXCSR.DAZ on the calling host thread (see SPU_SX_DAZ in
 * spu_helpers.h). Called at the start of every host thread that runs lifted
 * SPU code. GOWA_SPU_DAZ=0 leaves MXCSR alone -- only valid with lifted code
 * built without SPU_SX_DAZ. */
#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#endif
int g_spu_num_remap;   /* GOWA_SPU_NUM_REMAP, see spu_ls_read128_kc (spu_context.h) */
void spu_daz_enter(void)
{
#if defined(__x86_64__) || defined(_M_X64)
    static int s_on = -1;
    if (s_on < 0) { const char* e = getenv("GOWA_SPU_DAZ"); s_on = !(e && *e == '0'); }
    if (s_on) _mm_setcsr(_mm_getcsr() | 0x0040u);
#if defined(SPU_SX_RZ)
    /* GOWA (24/10): build with GOWA_SPU_RZ -- the SPU float helpers (lifted code and
     * interpreter) rely on round toward zero on every SPU host thread (spu_helpers.h) */
    _mm_setcsr((_mm_getcsr() & ~0x6000u) | 0x6000u);
#endif
#endif
    { extern void gowa_dma_ea_parse(void); gowa_dma_ea_parse(); }
    { static int done; if (!done) { done = 1; const char* e = getenv("GOWA_SPU_NUM_REMAP");
                                     g_spu_num_remap = !e ? 0 : *e == '1' ? 2 : *e == '2' ? 6 : *e == '3' ? 3 : 0; } }   /* GOWA (24/10): =3 -> SPU 6 reads 3 */
#ifdef _WIN32
    { extern __declspec(dllimport) unsigned long __stdcall GetCurrentThreadId(void);
      fprintf(stderr, "[spu-host] thread SPU host tid=%lu\n", GetCurrentThreadId()); }
#endif
}

/* GOWA_DMA_EA_LOG=lo-hi: see spu_dma.h. Parsed by spu_daz_enter (every SPU
 * host thread start). */
int g_gowa_dma_ea_log;
static uint32_t s_dea_lo, s_dea_hi;
static volatile long s_dea_cnt[4096];
static volatile uint32_t s_dea_key[4096];
static volatile long long s_dea_last;
void gowa_dma_ea_parse(void)
{
    static int done;
    if (done) return;
    done = 1;
    const char* e = getenv("GOWA_DMA_EA_LOG");
    if (!e) return;
    s_dea_lo = (uint32_t)strtoul(e, 0, 16);
    const char* d = strchr(e, '-');
    s_dea_hi = d ? (uint32_t)strtoul(d + 1, 0, 16) : s_dea_lo + 0x1000u;
    g_gowa_dma_ea_log = 1;
}
void gowa_dma_ea_note(uint64_t ea, uint32_t size, uint32_t cmd)
{
    (void)size;
    const uint32_t e = (uint32_t)ea;
    if (e < s_dea_lo || e >= s_dea_hi) return;
    const uint32_t k = e >> 4;
    uint32_t h = (k * 2654435761u) >> 20;
    for (int p = 0; p < 32; p++, h = (h + 1) & 4095u) {
        if (s_dea_key[h] == k + 1u) { __atomic_add_fetch(&s_dea_cnt[h], 1, __ATOMIC_RELAXED); break; }
        if (!s_dea_key[h]) { s_dea_key[h] = k + 1u; __atomic_add_fetch(&s_dea_cnt[h], 1, __ATOMIC_RELAXED); break; }
    }
    extern uint64_t ppu_timebase_now(void);          /* 79.8 MHz */
    const long long now = (long long)ppu_timebase_now();
    long long last = s_dea_last;
    if (now - last > 5LL * 79800000LL && last &&
        __atomic_compare_exchange_n(&s_dea_last, &last, now, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
        char b[512]; int n = 0;
        for (int r = 0; r < 8; r++) {
            long best = 0; int bi = -1;
            for (int i = 0; i < 4096; i++) if (s_dea_cnt[i] > best) { best = s_dea_cnt[i]; bi = i; }
            if (bi < 0) break;
            n += snprintf(b + n, sizeof b - n, " %08X:%ld", (s_dea_key[bi] - 1u) << 4, best);
            s_dea_cnt[bi] = -s_dea_cnt[bi];   /* skip it in the next pass of this report */
        }
        for (int i = 0; i < 4096; i++) s_dea_cnt[i] = 0;
        fprintf(stderr, "[dma-ea] 5 s (cmd 0x%X):%s\n", cmd, b);
    } else if (!last) {
        long long z = 0;
        __atomic_compare_exchange_n(&s_dea_last, &z, now, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    }
}

/* GOWA_SLOW_SPU_PCT=N / GOWA_SLOW_PPU_PCT=N (sensitivity test): the calling
 * thread spins N% of the busy time since its previous call (gaps over ~2 ms,
 * where it was blocked, are not counted) -- "this side N% slower", to see
 * which side the frame rate follows. */
#include <immintrin.h>
#if defined(_MSC_VER) || defined(__clang__)
#include <intrin.h>
#endif
int g_gowa_slow_spu = -1, g_gowa_slow_ppu = -1, g_gowa_slow_rsx = -1;
void gowa_slow_tick_sel(int pct, int sel);
void gowa_slow_tick(int pct)
{
    gowa_slow_tick_sel(pct, 1);
}
/* sel 0: only advance the clock (time not charged). GOWA_SLOW_WIDS=<hex mask
 * of workload ids> / GOWA_SLOW_SPUS=<hex mask of SPU ids> pick what
 * gowa_slow_sel charges. */
int gowa_slow_sel(const void* vctx)
{
    static unsigned long long wm = 0, sm = 0; static int init;
    if (!init) {
        const char* e = getenv("GOWA_SLOW_WIDS"); wm = e ? strtoull(e, 0, 16) : ~0ull;
        e = getenv("GOWA_SLOW_SPUS"); sm = e ? strtoull(e, 0, 16) : ~0ull;
        init = 1;
    }
    const spu_context* ctx = (const spu_context*)vctx;
    return ((sm >> ((unsigned)ctx->spu_id & 63u)) & 1u) && ((wm >> (ctx->ls[0x1DF] & 63u)) & 1u);
}
void gowa_slow_tick_sel(int pct, int sel)
{
    static SPU_THREAD_LOCAL unsigned long long last;
    /* GOWA_SLOW_FROM=<s>: only from that many seconds after start (the level,
     * not the boot: a slower boot desynchronises the scripted pad presses) */
    { extern uint64_t ppu_timebase_now(void);
      static long long from = -1;
      if (from < 0) { const char* e = getenv("GOWA_SLOW_FROM"); from = e ? atoll(e) * 79800000LL : 0; }
      if (from && (long long)ppu_timebase_now() < from) return; }
    const unsigned long long now = __rdtsc();
    const unsigned long long el = now - last;
    if (el < 100000ull) return;                     /* ~20 us between charges */
    last = now;
    if (el > 10000000ull) return;                   /* ~2 ms: the thread was idle */
    if (!sel) return;
    const unsigned long long until = now + el * (unsigned)pct / 100u;
    while (__rdtsc() < until) _mm_pause();
    last = __rdtsc();
}
