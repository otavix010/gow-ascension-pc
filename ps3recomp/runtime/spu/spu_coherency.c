/*
 * ps3recomp - PPU <-> SPU lock-line coherence
 *
 * See spu_coherency.h for what this is for. The state here is deliberately
 * plain: a bit per 128-byte line, a small table of the SPU contexts that have
 * ever reserved one, and the lock-line spinlock. Everything that WRITES it runs
 * with the lock-line lock held (the SPU's GETLLAR, the PPU's coherent store),
 * so the read-modify-write on a shared bitmap byte is serialized. Only
 * spu_coh_is_reserved reads without the lock, which is the entire point: it is
 * on every PPU store.
 */

#include <stdlib.h>
#include "spu_coherency.h"

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===========================================================================
 * The lock-line lock
 *
 * Moved here from spu_channels.c, where it was a file-static and so could
 * serialize SPU against SPU but not SPU against PPU -- which left the PPU free
 * to write a line in the middle of a PUTLLC's compare-and-commit. Same
 * spinlock, same semantics, now shared. _InterlockedExchange is a clang-cl/MSVC
 * intrinsic (no runtime library symbol needed); elsewhere use the C11
 * equivalent, which lowers to the same LL/SC or lock-xchg on every target.
 * ===========================================================================*/
#if defined(_MSC_VER)
#include <intrin.h>
#include <windows.h>   /* SwitchToThread */
/* Test-and-test-and-set: spin on a plain read (no cache-line ping-pong) with a
 * pause, and yield after a long wait. The bare exchange loop let GH3's four
 * idle job-policy pollers saturate the line and delay every other SPU atomic
 * -- FMOD's mixer task among them, 25-40 ms at a time. */
/* GOWA_LOCKSTAT=1 (diagnostic, 23/10): acquisitions and pause spins per caller
 * (return address as an exe RVA), the top 16 every ~10 s. */
static int g_lockstat = -1;
static void* volatile s_ls_key[256];
static volatile long long s_ls_n[256], s_ls_spin[256];
static __attribute__((noinline)) void spu_lockstat_note(void* ra, unsigned long long spins)
{
    if (g_lockstat < 0) { g_lockstat = getenv("GOWA_LOCKSTAT") ? 1 : 0; if (!g_lockstat) return; }
    uint32_t h = (uint32_t)(((uintptr_t)ra * 0x9E3779B97F4A7C15ull) >> 56);
    for (int p = 0; p < 256; p++, h = (h + 1) & 255u) {
        void* k = s_ls_key[h];
        if (!k) { InterlockedCompareExchangePointer((void* volatile*)&s_ls_key[h], ra, NULL); k = s_ls_key[h]; }
        if (k == ra) { InterlockedIncrement64(&s_ls_n[h]); InterlockedExchangeAdd64(&s_ls_spin[h], (long long)spins); break; }
    }
    static volatile LONGLONG s_t0;
    LARGE_INTEGER now, f; QueryPerformanceCounter(&now); QueryPerformanceFrequency(&f);
    LONGLONG t0 = s_t0;
    if (!t0) { InterlockedCompareExchange64(&s_t0, now.QuadPart, 0); return; }
    if (now.QuadPart - t0 < 10 * f.QuadPart || InterlockedCompareExchange64(&s_t0, now.QuadPart, t0) != t0) return;
    HMODULE self = GetModuleHandleA(NULL);
    long long tn = 0, ts = 0;
    for (int i = 0; i < 256; i++) { tn += s_ls_n[i]; ts += s_ls_spin[i]; }
    fprintf(stderr, "[lockstat] %.0f/s aquisicoes, %.0f voltas de espera/s\n", tn / 10.0, ts / 10.0);
    for (int r = 0; r < 16; r++) {
        int b = -1;
        for (int i = 0; i < 256; i++) if (s_ls_key[i] && s_ls_n[i] && (b < 0 || s_ls_spin[i] + s_ls_n[i] > s_ls_spin[b] + s_ls_n[b])) b = i;
        if (b < 0) break;
        fprintf(stderr, "[lockstat]   rva 0x%08llX  %.0f/s  espera %.0f voltas/s\n",
                (unsigned long long)((uintptr_t)s_ls_key[b] - (uintptr_t)self), s_ls_n[b] / 10.0, s_ls_spin[b] / 10.0);
        s_ls_n[b] = 0; s_ls_spin[b] = 0;
    }
    for (int i = 0; i < 256; i++) { s_ls_n[i] = 0; s_ls_spin[i] = 0; }
}
/* ... and with it the atomics by (command, line, workload, pc), top 20 every ~10 s */
static volatile uint64_t s_ll_key[1024];
static volatile long long s_ll_n[1024];
int spu_lockstat_on(void) { if (g_lockstat < 0) g_lockstat = getenv("GOWA_LOCKSTAT") ? 1 : 0; return g_lockstat; }
void spu_llstat_note(uint32_t cmd, uint32_t ea, uint32_t wid, uint32_t pc)
{
    const uint64_t key = ((uint64_t)(cmd & 0xFF) << 56) | ((uint64_t)(wid & 0xFF) << 48) |
                         ((uint64_t)(pc & 0x3FFFF) << 30) | ((ea & ~127u) >> 2);
    uint32_t h = (uint32_t)((key * 0x9E3779B97F4A7C15ull) >> 54);
    for (int p = 0; p < 1024; p++, h = (h + 1) & 1023u) {
        uint64_t k = s_ll_key[h];
        if (!k) { InterlockedCompareExchange64((volatile LONG64*)&s_ll_key[h], (LONG64)key, 0); k = s_ll_key[h]; }
        if (k == key) { InterlockedIncrement64(&s_ll_n[h]); break; }
    }
    static volatile LONGLONG s_t0;
    LARGE_INTEGER now, f; QueryPerformanceCounter(&now); QueryPerformanceFrequency(&f);
    LONGLONG t0 = s_t0;
    if (!t0) { InterlockedCompareExchange64(&s_t0, now.QuadPart, 0); return; }
    if (now.QuadPart - t0 < 10 * f.QuadPart || InterlockedCompareExchange64(&s_t0, now.QuadPart, t0) != t0) return;
    for (int r = 0; r < 20; r++) {
        int b = -1;
        for (int i = 0; i < 1024; i++) if (s_ll_n[i] && (b < 0 || s_ll_n[i] > s_ll_n[b])) b = i;
        if (b < 0) break;
        const uint64_t k = s_ll_key[b];
        fprintf(stderr, "[llstat] cmd=0x%02X wid=%u pc=0x%05X linha=0x%08X  %.0f/s\n", (unsigned)(k >> 56),
                (unsigned)((k >> 48) & 0xFF), (unsigned)((k >> 30) & 0x3FFFF), (unsigned)((k & 0x3FFFFFFFu) << 2),
                s_ll_n[b] / 10.0);
        s_ll_n[b] = 0;
    }
    for (int i = 0; i < 1024; i++) { s_ll_n[i] = 0; s_ll_key[i] = 0; }
}
/* GOWA (23/10): one lock per line group instead of one for the whole process.
 * God of War Ascension does ~2.7 million lock-line transactions a second (the
 * three EDGE job managers claiming jobs on their own job-list lines, the render
 * workloads, the SPURS kernel, the PPU's stores and stwcx to reserved lines),
 * nearly all on DIFFERENT lines, and the single lock made them all wait on
 * each other: ~8 million pause spins a second, 2-3.6% of every EDGE SPU
 * thread. Every transaction on ONE line -- GETLLAR, PUTLLC/PUTLLUC, a coherent
 * PPU store or stwcx, the notify of a DMA PUT -- takes that line's stripe, so
 * two transactions on the same line are serialized exactly as before; the
 * multi-line ones (SDCRZ, reserved-span PUT, ranged PPU copies) and the
 * removals from the reserving set take every stripe, in order. What spans
 * stripes is made safe on its own: the bitmap bit is set with an atomic OR,
 * the reserving set only grows by an atomic push (removals hold every stripe),
 * and the LR event bit is set/acknowledged with atomic OR/AND. A notifier of
 * another line can still drop a reservation its holder has just moved (it read
 * the old line): a spurious PUTLLC failure the guest retries, which hardware
 * allows too. A/B abll1 (chapter 16, 6 runs each): 53.25 single x 53.13 striped,
 * spins down 79% but no frame time -- OFF by default; GOWA_LL_STRIPES=1 turns it on
 * (with it off, stripe 0 is the single lock and the atomic pieces stay). */
#define SPU_LL_STRIPES 64
static __declspec(align(64)) volatile long g_ll[SPU_LL_STRIPES][16];   /* one cache line each */
static int g_ll_mask = -1;
static int spu_ll_mask(void)
{
    int m = g_ll_mask;
    if (__builtin_expect(m < 0, 0)) {
        const char* e = getenv("GOWA_LL_STRIPES");
        m = (e && *e == '1') ? SPU_LL_STRIPES - 1 : 0;   /* off by default: A/B abll1 even (53.25 x 53.13) */
        if (g_ll_mask < 0) fprintf(stderr, "[lockline] %s\n", m ? "one lock per line (64 stripes)" : "single lock");
        g_ll_mask = m;
    }
    return m;
}
static inline unsigned spu_ll_stripe(uint32_t ea)
{
    return (unsigned)((((ea >> 7) * 0x9E3779B1u) >> 26) & (unsigned)spu_ll_mask());
}
/* GOWA (24/10): pauses before a waiter gives up its time slice. 4096 suits this
 * port's main machine (16 logical CPUs for ~9 busy threads: the holder is running
 * and lets go within microseconds). With fewer logical CPUs than busy threads the
 * holder is often descheduled, and on Intel cores since Skylake a pause is ~140
 * cycles, so 4096 of them burnt ~0.1 ms of a slice for nothing (lock line 4-8% of
 * each SPU thread on a 4-core i5-10300H). Under 12 logical CPUs: 64.
 * GOWA_LL_SPIN=<n> sets it. */
static unsigned g_ll_spin;
static unsigned spu_ll_spin_init(void)
{
    const char* e = getenv("GOWA_LL_SPIN");
    unsigned n = e ? (unsigned)strtoul(e, NULL, 0) : 0;
    if (!n) { SYSTEM_INFO si; GetSystemInfo(&si); n = si.dwNumberOfProcessors < 12 ? 64u : 4096u; }
    g_ll_spin = n;
    return n;
}
static inline __attribute__((always_inline)) void spu_ll_acquire(volatile long* l, void* ra)
{
    unsigned spins = 0;
    unsigned long long tot = 0;
    while (_InterlockedExchange(l, 1)) {
        const unsigned lim = g_ll_spin ? g_ll_spin : spu_ll_spin_init();
        do {
            _mm_pause();
            tot++;
            if (++spins >= lim) { spins = 0; SwitchToThread(); }
        } while (*l);
    }
    if (__builtin_expect(g_lockstat != 0, 0)) spu_lockstat_note(ra, tot);
}
void spu_lockline_lock_ea(uint32_t ea)   { spu_ll_acquire(g_ll[spu_ll_stripe(ea)], __builtin_return_address(0)); }
void spu_lockline_unlock_ea(uint32_t ea) { _InterlockedExchange(g_ll[spu_ll_stripe(ea)], 0); }
void spu_lockline_lock(void)
{
    const int m = spu_ll_mask();
    for (int i = 0; i <= m; i++) spu_ll_acquire(g_ll[i], __builtin_return_address(0));
}
void spu_lockline_unlock(void)
{
    for (int i = spu_ll_mask(); i >= 0; i--) _InterlockedExchange(g_ll[i], 0);
}
#else
#include <stdatomic.h>
static atomic_flag g_lockline = ATOMIC_FLAG_INIT;
void spu_lockline_lock(void)   { while (atomic_flag_test_and_set_explicit(&g_lockline, memory_order_acquire)) { } }
void spu_lockline_unlock(void) { atomic_flag_clear_explicit(&g_lockline, memory_order_release); }
void spu_lockline_lock_ea(uint32_t ea)   { (void)ea; spu_lockline_lock(); }
void spu_lockline_unlock_ea(uint32_t ea) { (void)ea; spu_lockline_unlock(); }
#endif

/* ===========================================================================
 * The reserved-line bitmap
 *
 * One bit per 128-byte line over the whole 32-bit guest address space: 4 MiB of
 * zero-initialized BSS, of which only the pages around addresses an SPU has
 * actually reserved are ever touched. The reference implementation scopes its
 * bitmap to the one window a single title's SPURS structures live in; this
 * runtime hands out guest memory from several disjoint regions (the ELF image,
 * the sys_memory window at 0x40000000, the raw-SPU local-store windows), and a
 * line outside a hardcoded window would be silently non-coherent, which is the
 * failure this whole file exists to remove.
 * ===========================================================================*/
#define SPU_COH_LINE_SHIFT  7                                  /* 128-byte lines */
#define SPU_COH_LINES       (1u << (32 - SPU_COH_LINE_SHIFT))   /* 2^25 lines */
#define SPU_COH_BITMAP_SZ   (SPU_COH_LINES / 8)                 /* 4 MiB */

/* GOWA: external, read inline by the lifted PPU stores (ppu_recomp.h). */
unsigned char spu_coh_bitmap[SPU_COH_BITMAP_SZ];
#define s_coh_bitmap spu_coh_bitmap

/* Zero until the first GETLLAR anywhere. Keeps the PPU fast path off the
 * bitmap entirely for a title that runs no SPU code, and keeps every existing
 * test that stores through vm_write* paying one compare against a hot global.
 * Written once, under the lock; read unlocked, where a stale zero costs at most
 * one missed event on the very first reservation of the run. */
int spu_coh_armed;
#define s_coh_armed spu_coh_armed

/* Every live virtual context may retain a reservation between scheduling
 * quanta; physical SPU count is not a bound on this set. All access is under
 * the lock-line lock. Nodes are removed before their context's lifetime ends. */
typedef struct spu_coh_entry {
    spu_context* ctx;
    struct spu_coh_entry* next;
} spu_coh_entry;
static spu_coh_entry* s_coh_ctxs;

unsigned long g_spu_lr_raise = 0;

void spu_coh_reserve(spu_context* ctx, uint32_t ea)
{
    uint32_t line = ea >> SPU_COH_LINE_SHIFT;
    /* GOWA: store only on a change. Unconditional, every GETLLAR of every SPU
     * wrote these two shared lines -- a polling SPURS kernel re-reserves the
     * same line millions of times -- and each write took the line away from
     * the other cores. */
    const unsigned char bit = (unsigned char)(1u << (line & 7));
    /* GOWA: atomic -- with the striped lock another line of the same byte can
     * be marked at the same time */
    if (!(s_coh_bitmap[line >> 3] & bit)) _InterlockedOr8((volatile char*)&s_coh_bitmap[line >> 3], (char)bit);
    if (!s_coh_armed) s_coh_armed = 1;

    if (!ctx) return;
    for (spu_coh_entry* entry = s_coh_ctxs; entry; entry = entry->next)
        if (entry->ctx == ctx) return;
    spu_coh_entry* entry = (spu_coh_entry*)malloc(sizeof(*entry));
    if (!entry) {
        fprintf(stderr, "[spu-coh] cannot register reservation holder %p\n", (void*)ctx);
        fflush(stderr);
        abort(); /* A missed holder loses invalidation events, including ABA. */
    }
    entry->ctx = ctx;
    /* GOWA: pushed atomically (another stripe may push at the same time; the
     * walkers see the old head or the complete new entry; removals hold every
     * stripe, so none runs concurrently) */
    spu_coh_entry* head;
    do {
        head = s_coh_ctxs;
        entry->next = head;
    } while (_InterlockedCompareExchangePointer((void* volatile*)&s_coh_ctxs, entry, head) != head);
}

/* Drop a context from the reserving set.
 *
 * There was no way to do this, and the contexts that most need it are
 * TRANSIENT: spu_run_lifted_job_abi runs each job on a STACK-LOCAL
 * spu_context, which registered itself here on its first reservation and then
 * returned. The entry outlived the object, so spu_coh_notify_write walked a
 * dangling pointer -- reading resv_valid, writing event_status and calling
 * spu_ch_wake on reclaimed stack. Undefined behaviour, and observable: Guitar
 * Hero III showed THREE live-looking contexts for one dispatch, stealing each
 * other`s reservations, with every PUTLLC failing "no reservation". */
void spu_coh_unregister(spu_context* ctx)
{
    if (!ctx) return;
    /* Under the lock-line lock, which every walker of s_coh_ctxs holds: a
     * notifier that loaded this pointer just before removal would
     * otherwise go on dereferencing it after the job returned and its stack
     * (or its whole async thread) was gone. GH3 at -O2 crashed that way in
     * spu_coh_notify_write, reading a freed thread stack. */
    spu_lockline_lock();
    for (spu_coh_entry** link = &s_coh_ctxs; *link; link = &(*link)->next) {
        if ((*link)->ctx != ctx) continue;
        spu_coh_entry* entry = *link;
        *link = entry->next;
        free(entry);
        break;
    }
    spu_lockline_unlock();
}

/* Drop every registered context that lies in [lo, hi) -- a host thread's
 * stack, called as that thread exits. A stack-local context still registered
 * then has leaked past its unregister; the next walker would read an unmapped
 * stack (GH3 at the end of a Havok task: spu_coh_notify_write faulting).
 * ponytail: safety net that also names the leak; find the path that skips
 * spu_coh_unregister if it keeps firing. */
void spu_coh_forget_range(uintptr_t lo, uintptr_t hi)
{
    spu_lockline_lock();
    for (spu_coh_entry** link = &s_coh_ctxs; *link;) {
        spu_coh_entry* entry = *link;
        uintptr_t c = (uintptr_t)entry->ctx;
        if (c >= lo && c < hi) {
            static int _n = 0;
            if (_n++ < 16)
                fprintf(stderr, "[spu-coh] leaked ctx %p (img=%d) dropped at thread exit\n",
                        (void*)c, entry->ctx->image_id);
            *link = entry->next;
            free(entry);
        } else {
            link = &entry->next;
        }
    }
    spu_lockline_unlock();
}

int spu_coh_is_reserved(uint32_t addr)
{
    if (!s_coh_armed) return 0;
    uint32_t line = addr >> SPU_COH_LINE_SHIFT;
    return (s_coh_bitmap[line >> 3] >> (line & 7)) & 1u;
}

/* A DMA PUT is issued BY an SPU, and hardware does not take that SPU's own
 * reservation away for its own MFC write -- the MFC is part of the same SPE.
 * Our notify had no way to say "everyone but me", so an SPU that reserved a
 * line and then DMA'd a buffer overlapping it killed its own reservation and
 * its next PUTLLC could never succeed. Guitar Hero III's job policy module
 * livelocks exactly there: 16 million atomic ops on one line, every PUTLLC
 * failing for "no reservation", 600k of them self-inflicted.
 *
 * The PUTLLC path already guards this case by dropping its own reservation
 * before notifying; this gives the DMA path the same ability. */
extern void ppu_resv_break_line(uint32_t ea);   /* ppu_loader.cpp */

void spu_coh_notify_write_except(uint32_t ea, const void* self)
{
    uint32_t line = ea & ~127u;
    ppu_resv_break_line(ea);
    for (spu_coh_entry* entry = s_coh_ctxs; entry; entry = entry->next) {
        spu_context* c = entry->ctx;
        if (!c || (const void*)c == self) continue;
        if (c->resv_valid && (c->resv_ea & ~127u) == line) {
            _InterlockedOr((volatile long*)&c->event_status, (long)SPU_EVENT_LR);   /* GOWA: atomic (striped lock) */
            c->tail_budget = 0;   /* SPU_TAILX: event pending (racy store, bounded by the budget) */
            c->resv_valid = 0;
            g_spu_lr_raise++;
            spu_ch_wake(c);
        }
    }
}

void spu_coh_notify_write(uint32_t ea)
{
    uint32_t line = ea & ~127u;
    /* The PPU's lwarx reservations die on a line write too (128-byte granule). */
    ppu_resv_break_line(ea);
    for (spu_coh_entry* entry = s_coh_ctxs; entry; entry = entry->next) {
        spu_context* c = entry->ctx;
        if (!c) continue;
        if (c->resv_valid && (c->resv_ea & ~127u) == line) {
            _InterlockedOr((volatile long*)&c->event_status, (long)SPU_EVENT_LR);   /* GOWA: atomic (striped lock) */
            c->tail_budget = 0;   /* SPU_TAILX: event pending (racy store, bounded by the budget) */
            c->resv_valid = 0;          /* reservation lost, PUTLLC must fail */
            /* SPU_PUTLLC_WHY=1: name the agent that killed it. A PUTLLC that
             * always fails for "no reservation" is useless without knowing who
             * took it away -- a peer SPU, or the PPU committing to the line. */
            { static int s_w = -1;
              if (s_w < 0) s_w = getenv("SPU_PUTLLC_WHY") ? 1 : 0;
              if (s_w) { static unsigned long long n;
                  if ((++n % 200000) == 1)
                      fprintf(stderr, "[resv-lost] %llu: line 0x%08X taken from img=%d ctx=%p\n",
                              n, line, c->image_id, (void*)c); } }
            g_spu_lr_raise++;
            /* The event_status store has to be visible before the wake. The
             * waiter re-polls its predicate on a timeout as well, so a
             * straggling store costs latency and not the wakeup itself. */
            spu_ch_wake(c);
        }
    }
}

#ifdef __cplusplus
}
#endif
