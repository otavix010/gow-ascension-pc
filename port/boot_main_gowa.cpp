/* GOWA: copia de ps3recomp/runtime/ppu/tests/boot_main.cpp (v0.15.0) usada como runner do port.
 * Mudancas: includes relativos -> pelo include path, e a chamada gowa_offline_register(). */
/*
 * ps3recomp - integrated PPU boot harness.
 *
 * Links the whole PPU runtime half into one executable and starts executing a
 * recompiled program's entry point:
 *
 *   lifted code (ppu_recomp.c) + loader (ppu_loader.cpp) + HLE bridge
 *   (ppu_hle.cpp + generated NID table) + HLE libs (cellGcmSys, rsx_commands)
 *
 * It loads the PPU ELF named on the command line, registers the lifted
 * functions and the HLE NID handlers, starts the frame clock, then dispatches
 * the entry OPD. Execution runs real guest code until it reaches a function
 * outside the lifted subset (logged by the unlifted stub), an unimplemented
 * firmware import (logged by ps3_hle_call), or an lv2 syscall (logged by
 * lv2_syscall) -- telling us exactly what to implement next.
 *
 * Nothing here is title-specific: the ELF path, the VFS root derived from it
 * and the window title all come from the command line or the environment. Two
 * things drive it today -- a game port, which supplies its own lifted
 * ppu_recomp.c and its own HLE overrides, and the smoke title in
 * tests/smoke/, which is the same harness with a synthetic program in it and
 * is what proves this path runs at all on POSIX.
 */
#include <stdarg.h>
#include "ppu_recomp.h"
#include "ps3emu/milestone.h"   /* ps3_ms / ps3_ms_kv -- boot milestone log */
/* PPU_THREAD_LOCAL only -- NOT ppu_context.h, which would redefine the struct
 * a generated ppu_recomp.h already declares. Ports generated before the
 * qualifier existed do not define it, so the scaffold must carry its own
 * reachable definition. */
#include "ppu_tls.h"
#include "win32_compat.h"      /* Sleep, GetTickCount64, threads on POSIX */
#include "win32_backtrace.h"   /* RtlCaptureStackBackTrace / GetModuleHandleA on POSIX */
/* The lifted ppu_recomp.h already defines `struct ppu_context` (identical to
 * runtime/ppu/ppu_context.h by design -- "keep the two in sync"). A syscall
 * header included later (sys_ppu_thread.h -> lv2_syscall_table.h -> ppu_context.h)
 * would redefine it, so claim ppu_context.h's guard now: this TU has the struct,
 * and boot_main uses none of that header's PPU_CR or XER bit macros. */
#ifndef PPU_CONTEXT_H
#define PPU_CONTEXT_H
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
uint32_t ppu_load_elf(const char* path);
void     ppu_recomp_register(void);
void     ppu_hle_init(void);
uint32_t ps3_hle_count(void);   /* handlers actually registered, for the milestone log */
void     ppu_sysprx_register(void);
void     ppu_fs_register(void);
int      ppu_run(uint32_t entry_opd, uint32_t stack_top);
extern const char* ppu_vfs_root;   /* host dir that PS3 mount points map into */
/* The status a guest handed to sys_process_exit, published by
 * libs/system/sysPrxForUser.c before it calls the host exit(). A title that
 * exits that way never returns through ppu_run, so this is only read on the
 * path where the guest fell out of its entry function instead. */
extern int     g_sys_process_exit_called;
extern int32_t g_sys_process_exit_code;
void     cellGame_init_from_paramsfo(const char* sfo_path);  /* libs/system/cellGame.c */
const char* cellGame_get_title_id(void);                     /* libs/system/cellGame.c */
void     gowa_offline_register(void);                        /* GOWA: gowa_offline.cpp */
int      gowa_conf_replay(const char* dir);                  /* GOWA: gowa_confere.cpp */
void     gowa_perf_jogo_liga(void);                          /* GOWA: gowa_confere.cpp (GOWA_PERF_JOGO) */
/* Optional hook: load real system PRX modules (libsre = cellSpurs/cellSync) into
 * guest RAM and register their exports. Weak default is a no-op; a title that
 * links a lifted PRX defines a strong version. Called after the lifted function
 * table is registered and vm_base is live, before the game runs. */
void     ps3_load_prx_modules(void);   /* GOWA: definida em stubs.cpp (libsre LLE) */
}

#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>   /* _ReturnAddress */
/* timeBeginPeriod: windows.h is included with WIN32_LEAN_AND_MEAN, which
 * excludes the multimedia timer API, so it must be asked for by name -- and
 * AFTER windows.h, since timeapi.h uses UINT and friends. */
#include <timeapi.h>

/* Process-wide getenv cache. The runtime has ~700 `if (getenv("X"))` diagnostic
 * gates, many on per-draw / per-syscall paths, and the UCRT getenv takes the
 * environment lock and scans every variable each time: profiled at ~30% of
 * GH3's main PPU thread and ~35% of the RSX thread in gameplay. Defining the
 * import slot here (an object in the link beats the import library's lazy
 * definition) routes every dllimport'ed getenv call through this cache.
 * Nothing in the runtime mutates the environment after startup, so a value is
 * looked up once and the pointer stays valid forever. Reads are lock-free:
 * a slot's key is published last, after its value.
 * ponytail: 1024 open-addressed slots, fixed; names past that fall back to the OS lookup. */
namespace {
struct EnvSlot { const char* volatile key; const char* val; };
EnvSlot  s_env[1024];
SRWLOCK  s_env_lock = SRWLOCK_INIT;

unsigned env_hash(const char* s)
{
    unsigned h = 2166136261u;
    for (; *s; s++) h = (h ^ (unsigned char)(*s >= 'a' && *s <= 'z' ? *s - 32 : *s)) * 16777619u;
    return h;
}

const char* env_lookup_os(const char* name)
{
    DWORD n = GetEnvironmentVariableA(name, NULL, 0);
    if (!n) return NULL;
    char* v = (char*)malloc(n);
    if (!v || GetEnvironmentVariableA(name, v, n) >= n) { free(v); return NULL; }
    return v;
}

char* __cdecl cached_getenv(const char* name)
{
    if (!name || !*name) return NULL;
    const unsigned mask = 1023, h = env_hash(name);
    for (unsigned i = h & mask, n = 0; n <= mask; n++, i = (i + 1) & mask) {
        const char* k = s_env[i].key;
        MemoryBarrier();
        if (!k) break;
        if (!_stricmp(k, name)) {
            /* GETENV_STATS=1: which gate is being polled on a hot path. */
            static int st = -1;
            if (st < 0) st = env_lookup_os("GETENV_STATS") ? 1 : 0;
            if (st) {
                static unsigned cnt[1024]; static void* ra[1024]; static unsigned long long tot;
                cnt[i]++; ra[i] = _ReturnAddress();
                if ((++tot & ((1u << 22) - 1)) == 0) {
                    const uintptr_t exe = (uintptr_t)GetModuleHandleA(NULL);
                    for (int r = 0; r < 5; r++) {
                        unsigned b = 0;
                        for (unsigned j = 1; j <= mask; j++) if (cnt[j] > cnt[b]) b = j;
                        if (!cnt[b]) break;
                        fprintf(stderr, "[getenv-stats] %-28s %u calls  caller rva=0x%llX%c",
                                s_env[b].key, cnt[b], (unsigned long long)((uintptr_t)ra[b] - exe), 10);
                        cnt[b] = 0;
                    }
                    memset(cnt, 0, sizeof cnt);
                }
            }
            return (char*)s_env[i].val;
        }
    }
    AcquireSRWLockExclusive(&s_env_lock);
    const char* val = NULL; int found = 0;
    unsigned i = h & mask, n = 0;
    for (; n <= mask; n++, i = (i + 1) & mask) {
        if (!s_env[i].key) break;
        if (!_stricmp(s_env[i].key, name)) { val = s_env[i].val; found = 1; break; }
    }
    if (!found) {
        val = env_lookup_os(name);
        if (n <= mask) {
            s_env[i].val = val;
            MemoryBarrier();
            s_env[i].key = _strdup(name);
        }
    }
    ReleaseSRWLockExclusive(&s_env_lock);
    return (char*)val;
}
}
extern "C" char* (__cdecl* __imp_getenv)(const char*) = cached_getenv;

/* Last-chance crash reporter: vm_base accesses are bounds-guarded, so a real
 * access violation means a HOST pointer deref (e.g. a bad function pointer or a
 * runtime-struct walk). Print the faulting address and the RIP as a module
 * offset (RVA) so it can be symbolized with llvm-symbolizer against the PDB. */
extern "C" uint32_t    g_last_hle_nid;    /* ppu_hle.cpp breadcrumb */
extern "C" const char* g_last_hle_name;

extern "C" PPU_THREAD_LOCAL ppu_context* g_active_ctx;

/* GOWA (29/10, reports 276/281/309): os jogadores compilam o port no PC deles e não há PDB, então o RVA sozinho
 * não diz a função. O linker sempre grava o gowa.map ao lado do exe (CMakeLists, /MAP): no crash ele é lido e
 * cada endereço do exe sai como função+deslocamento. */
#define CRASH_NOME 112
static void crash_nomes(HMODULE self, const uint64_t* rva, int n, char (*nome)[CRASH_NOME])
{
    uint64_t melhor[32] = {0};
    for (int i = 0; i < n; i++) nome[i][0] = 0;
    char caminho[MAX_PATH + 16];
    DWORD k = GetModuleFileNameA(self, caminho, MAX_PATH);
    if (!k || k >= MAX_PATH) return;
    char* barra = strrchr(caminho, '\\');
    if (!barra) return;
    strcpy(barra + 1, "gowa.map");
    FILE* f = fopen(caminho, "rb");
    if (!f) return;
    uint64_t base = 0x140000000ull;
    char ln[1024];
    while (fgets(ln, sizeof ln, f)) {
        const char* p = strstr(ln, "Preferred load address is ");
        if (p) { base = strtoull(p + 26, NULL, 16); continue; }
        p = ln;
        while (*p == ' ') p++;
        /* " 0001:07c69dc0       nome      0000000147c6adc0     lib:obj" */
        if (!(isxdigit((unsigned char)p[0]) && p[4] == ':' && isxdigit((unsigned char)p[5]))) continue;
        p += 13;
        while (*p == ' ') p++;
        const char* n0 = p;
        while (*p && *p != ' ') p++;
        const size_t nl = (size_t)(p - n0);
        while (*p == ' ') p++;
        char* fim = NULL;
        const uint64_t va = strtoull(p, &fim, 16);
        if (fim != p + 16 || va < base) continue;
        const uint64_t ini = va - base;
        for (int i = 0; i < n; i++)
            if (ini <= rva[i] && (!nome[i][0] || ini > melhor[i])) {
                melhor[i] = ini;
                const size_t c = nl < CRASH_NOME - 1 ? nl : CRASH_NOME - 1;
                memcpy(nome[i], n0, c); nome[i][c] = 0;
            }
    }
    fclose(f);
    for (int i = 0; i < n; i++)
        if (nome[i][0]) {
            const size_t l = strlen(nome[i]);
            if (l < CRASH_NOME - 20) snprintf(nome[i] + l, CRASH_NOME - l, "+0x%llX", (unsigned long long)(rva[i] - melhor[i]));
        }
}
static const char* crash_modulo(HMODULE m, char* buf, DWORD n)
{
    if (!m || !GetModuleFileNameA(m, buf, n)) return "?";
    const char* b = strrchr(buf, '\\');
    return b ? b + 1 : buf;
}

static LONG WINAPI ydkj_crash_filter(EXCEPTION_POINTERS* ep)
{
    EXCEPTION_RECORD* er = ep->ExceptionRecord;
    fprintf(stderr, "\n[CRASH] code=0x%08lX rip=%p\n",
            (unsigned long)er->ExceptionCode, er->ExceptionAddress);
    fprintf(stderr, "[CRASH] last HLE NID 0x%08X (%s)\n",
            g_last_hle_nid, g_last_hle_name ? g_last_hle_name : "");
    if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
        fprintf(stderr, "[CRASH] %s fault address 0x%llX\n",
                er->ExceptionInformation[0] ? "write" : "read",
                (unsigned long long)er->ExceptionInformation[1]);
    if (g_active_ctx) fprintf(stderr, "[CRASH] guest ctr=0x%08X lr=0x%08X r3=0x%08X\n",
          (uint32_t)g_active_ctx->ctr, (uint32_t)g_active_ctx->lr, (uint32_t)g_active_ctx->gpr[3]);
    HMODULE mod = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)er->ExceptionAddress, &mod);
    char mb[MAX_PATH];
    fprintf(stderr, "[CRASH] module=%p (%s) rva=0x%llX\n",
            (void*)mod, crash_modulo(mod, mb, MAX_PATH),
            (unsigned long long)((char*)er->ExceptionAddress - (char*)mod));
    /* Host call stack: every frame with its module; the exe's frames get function+offset from gowa.map. */
    HMODULE self = GetModuleHandleA(NULL);
    void* frames[24];
    USHORT n = RtlCaptureStackBackTrace(0, 24, frames, NULL);
    uint64_t rvas[25]; HMODULE mods[25];
    rvas[0] = (uint64_t)((char*)er->ExceptionAddress - (char*)self);
    for (USHORT i = 0; i < n; i++) {
        mods[i + 1] = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)frames[i], &mods[i + 1]);
        rvas[i + 1] = (uint64_t)((char*)frames[i] - (char*)mods[i + 1]);
    }
    static char nomes[25][CRASH_NOME];
    crash_nomes(self, rvas, n + 1, nomes);
    if (mod == self)
        fprintf(stderr, "[CRASH] em %s\n", nomes[0][0] ? nomes[0] : "(sem gowa.map)");
    for (USHORT i = 0; i < n; i++) {
        HMODULE m = mods[i + 1];
        if (m == self)
            fprintf(stderr, "[CRASH]   #%-2u rva=0x%llX %s\n", i, (unsigned long long)rvas[i + 1], nomes[i + 1]);
        else
            fprintf(stderr, "[CRASH]   #%-2u %s+0x%llX\n", i, crash_modulo(m, mb, MAX_PATH), (unsigned long long)rvas[i + 1]);
    }
    fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

#ifdef _WIN32
/* abort()/exit(3) reporter: the recompiled CRT (or a failed invariant) can call
 * abort() — Windows turns that into exit code 3 with no message. Capture a host
 * backtrace (RVAs) + the last HLE NID so the aborting caller can be symbolized. */
static void ydkj_abort_handler(int)
{
    fprintf(stderr, "\n[ABORT] SIGABRT raised; last HLE NID 0x%08X (%s)\n",
            g_last_hle_nid, g_last_hle_name ? g_last_hle_name : "");
    void* frames[32];
    USHORT n = RtlCaptureStackBackTrace(0, 32, frames, NULL);
    HMODULE self = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)&ydkj_abort_handler, &self);
    for (USHORT i = 0; i < n; i++) {
        HMODULE m = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)frames[i], &m);
        if (m == self)
            fprintf(stderr, "[ABORT]   #%-2u rva=0x%llX\n", i,
                    (unsigned long long)((char*)frames[i] - (char*)m));
    }
    fflush(stderr);
    _exit(3);
}
#endif

/* Derive the VFS root (the dir containing PS3_GAME) from the EBOOT path
 * <root>/PS3_GAME/USRDIR/EBOOT.elf  -> <root>. $PS3_VFS_ROOT overrides. */
static char s_vfs_root[1024];
static void derive_vfs_root(const char* eboot)
{
    const char* env = getenv("PS3_VFS_ROOT");
    if (env && *env) { ppu_vfs_root = env; return; }
    strncpy(s_vfs_root, eboot, sizeof s_vfs_root - 1);
    for (char* p = s_vfs_root; *p; p++) if (*p == '\\') *p = '/';
    /* strip three trailing components: EBOOT.elf / USRDIR / PS3_GAME */
    for (int i = 0; i < 3; i++) { char* s = strrchr(s_vfs_root, '/'); if (s) *s = 0; }
    if (!s_vfs_root[0]) strcpy(s_vfs_root, ".");
    ppu_vfs_root = s_vfs_root;
}

/* Host-provided symbols the runtime + HLE libs need. */
extern "C" uint8_t* vm_base = nullptr;
extern "C" { int g_gowa_vm_ww = 0; }   /* GOWA_TEX_WW: a reserva tem MEM_WRITE_WATCH */
extern "C" uint32_t ppu_vm_size;   /* defined in ppu_loader.cpp (OOB guard) */
extern "C" void lv2_init_syscalls(void);   /* runtime/syscalls/lv2_register.c */
extern "C" void np_psnr_setup(const char* username, const char* server);  /* libs/network/np_psnr.c */

/* Guest-callback dispatch + RSX vblank/flip driver.
 *
 * g_ps3_guest_caller (defined NULL by libs/system/cellSysutil.c) is the hook the
 * HLE runtime uses to call back into recompiled code -- cellSysutil events and
 * the GCM vblank/flip handlers. ppu_guest_call (ppu_loader.cpp) does the OPD ->
 * dispatch. On real hardware the RSX fires a vblank interrupt ~60x/s that drives
 * the game's frame loop; with no RSX we synthesize it from a host timer thread
 * calling cellGcmTickVBlank()/TickFlip(), which invoke the registered handlers.
 * Without this the game inits, registers its handlers, and then waits forever
 * for a vblank that never comes. */
typedef void (*ps3_guest_caller_fn)(uint32_t, uint64_t, uint64_t, uint64_t, uint64_t,
                                    uint64_t, uint64_t, uint64_t, uint64_t);
extern "C" ps3_guest_caller_fn g_ps3_guest_caller;        /* libs/system/cellSysutil.c */
extern "C" uint64_t ppu_guest_call(uint32_t, uint64_t, uint64_t, uint64_t, uint64_t,
                                   uint64_t, uint64_t, uint64_t, uint64_t);
extern "C" void cellGcmTickVBlank(void);
extern "C" void cellGcmTickFlip(void);
extern "C" int  cellGcm_take_flip_pending(void);  /* hoisted to file scope: clang-cl rejects block-scope extern "C" */
extern "C" int  cellGcm_flip_hold_us(void);       /* cellGcmSys.c: GOWA_FLIP_HZ */

static void harness_guest_caller(uint32_t opd, uint64_t a0, uint64_t a1,
                                 uint64_t a2, uint64_t a3, uint64_t a4,
                                 uint64_t a5, uint64_t a6, uint64_t a7)
{ ppu_guest_call(opd, a0, a1, a2, a3, a4, a5, a6, a7); }

/* RSX present backend. D3D12 on Windows, Metal on Apple, and the null
 * backend's headless software path anywhere else -- the same selection
 * runtime/host/host_posix.c makes, so the ticker below is backend-agnostic
 * and a POSIX host gets a frame clock instead of no clock at all. */
#if defined(_WIN32)
extern "C" int  rsx_d3d12_backend_init(uint32_t w, uint32_t h, const char* title);
extern "C" void rsx_d3d12_backend_present(void);
extern "C" int  rsx_d3d12_backend_pump_messages(void);
#  define rsx_backend_init    rsx_d3d12_backend_init
#  define rsx_backend_present rsx_d3d12_backend_present
#  define rsx_backend_pump    rsx_d3d12_backend_pump_messages
#elif defined(__APPLE__)
extern "C" int  rsx_metal_backend_init(uint32_t w, uint32_t h, const char* title);
extern "C" void rsx_metal_backend_present(void);
extern "C" int  rsx_metal_backend_pump_messages(void);
#  define rsx_backend_init    rsx_metal_backend_init
#  define rsx_backend_present rsx_metal_backend_present
#  define rsx_backend_pump    rsx_metal_backend_pump_messages
#else
extern "C" int  rsx_null_backend_init(uint32_t w, uint32_t h, const char* title);
extern "C" void rsx_null_backend_present(void);
extern "C" int  rsx_null_backend_pump_messages(void);
#  define rsx_backend_init    rsx_null_backend_init
#  define rsx_backend_present rsx_null_backend_present
#  define rsx_backend_pump    rsx_null_backend_pump_messages
#endif
extern "C" void cellGcm_rsx_process_fifo(void);   /* cellGcmSys.c: drain get->put */
extern "C" int rsx_live_draw_framegen_wait_us(void);   /* rsx_live_draw.c: GOWA_FRAMEGEN */
extern "C" void* cellGcm_fifo_kick_event(void);          /* cellGcmSys.c */
extern "C" void rsx_live_draw_framegen_tick(void);
extern "C" void cellGcm_fifo_kick_wait(unsigned ms); /* idle wait, woken by a kick */
extern "C" int cellGcm_report_pace_pending(void);   /* GOWA: report pacing pending */
extern "C" unsigned cellGcm_flip_request_count(void);
extern "C" long cellGcm_flip_catchup_count(void);   /* cellGcmSys.c: flips passed in FIFO catch-up */
extern "C" unsigned cellGcmGetCurrentDisplayBufferId(void);
extern "C" int sys_event_queue_inject(unsigned int, unsigned long long, unsigned long long, unsigned long long, unsigned long long);

/* Live NV4097->D3D12 engine (libs/video/rsx_live_draw.c, from caner /
 * canersaka's Yakuza: Dead Souls port). Opt-in with RSX_LIVE_DRAW=1: the null
 * backend opens the window, the engine binds its swap chain to that HWND and
 * owns presentation, so rsx_d3d12_backend is left out entirely rather than run
 * alongside it. Unset, every path below is exactly what it was. */
extern "C" int   rsx_live_draw_enabled(void);
extern "C" int   rsx_live_draw_init(void* hwnd, uint32_t w, uint32_t h,
                                    const uint8_t* (*guest_ptr)(void*, uint32_t,
                                                                uint32_t, uint32_t),
                                    void* user);
extern "C" void  rsx_live_draw_present(uint32_t buffer_id);
extern "C" uint32_t rsx_live_draw_get_frames(void);
extern "C" uint32_t rsx_live_draw_get_last_draws(void);
extern "C" int   rsx_null_backend_init(uint32_t w, uint32_t h, const char* title);
extern "C" int   rsx_null_backend_pump_messages(void);
extern "C" void* rsx_null_backend_get_hwnd(void);
extern "C" void  rsx_null_backend_suppress_present(int on);
extern "C" uint32_t cellGcmResolveLocated(int local, uint32_t offset);
extern "C" uint32_t cellGcmResolveIO(uint32_t offset);

static int s_rsx_live = 0;   /* live engine selected AND up */

/* Resolve (location, offset) to host memory for the engine. location 0 is RSX
 * local VRAM, 1 is main/IO memory; the engine promises its callers the whole
 * min_bytes span is readable, so validate the interval, not just its start.
 * The location comes straight from the RSX DMA context selector and is
 * authoritative -- cellGcmResolveOffset()'s heuristic prefers VRAM for any page
 * the guest ever derived from a local EA, which maps a title's MAIN-memory
 * textures into local memory where they read as garbage (caner hit exactly
 * that in the Yakuza port; the geometry came out flat white). */
static const uint8_t* rsx_live_guest_ptr(void* user, uint32_t location,
                                         uint32_t offset, uint32_t min_bytes)
{
    (void)user;
    if (!vm_base) return nullptr;
    uint32_t ea;
    if (location == 0) {
        ea = cellGcmResolveLocated(1, offset);          /* RSX local VRAM */
    } else {
        ea = cellGcmResolveIO(offset);                  /* main, via the IO table */
        if (!ea) ea = cellGcmResolveLocated(0, offset); /* unmapped: old behaviour */
    }
    if (!ea || ea == 0xFFFFFFFFu ||
        (uint64_t)ea + min_bytes > 0x100000000ull) return nullptr;
    return (const uint8_t*)vm_base + ea;
}

/* Present / pump through whichever backend is live. */
static void rsx_present_frame(void)
{
    /* Present the buffer the guest actually flipped to, not buffer 0.
     *
     * rsx_live_draw_present() looks up the surface registered for the display
     * buffer it is given, so hardcoding 0 presented buffer 0's surface for
     * EVERY flip. A double-buffered title flips 0,1,0,1..., so half its frames
     * showed the previous image instead of the one just drawn -- content
     * appearing and vanishing at ~30 Hz, and black wherever buffer 0 had not
     * been drawn yet. The Simpsons Arcade Game flips 0,1 alternately and that
     * is exactly what it looked like.
     *
     * cellGcmGetCurrentDisplayBufferId() is set by cellGcmSetFlipCommand,
     * which the FIFO walker calls on this same thread before setting the
     * pending flag we are responding to, so it is current here. */
    if (s_rsx_live) rsx_live_draw_present(cellGcmGetCurrentDisplayBufferId());
    else            rsx_backend_present();   /* platform backend: D3D12 / Metal / null */
}
static int rsx_pump_messages(void)
{ return s_rsx_live ? rsx_null_backend_pump_messages() : rsx_backend_pump(); }

/* Frames handed to the backend at a guest FLIP boundary -- one per frame the
 * guest actually finished, which is the number a port means by "is anything
 * reaching the screen". The presents the ticker makes before the guest's first
 * flip (so a fresh window is not left white through a long boot) are
 * deliberately not counted: they carry no guest frame. */
static volatile LONG g_frames_presented = 0;
extern "C" unsigned ppu_boot_frames_presented(void) { return (unsigned)g_frames_presented; }
/* GOWA_PRESENT_LOG=1: apresentações por origem a cada ~5 s (0/1 = flip
 * pendente no tique / na drenagem externa, 2 = contador de flips mudou) */
static volatile long s_pres_src[3];
static void present_src_note(int k)
{
    static int on = -1; static ULONGLONG t0;
    if (on < 0) on = getenv("GOWA_PRESENT_LOG") ? 1 : 0;
    if (!on) return;
    InterlockedIncrement(&s_pres_src[k]);
    const ULONGLONG now = GetTickCount64();
    if (!t0) t0 = now;
    if (now - t0 >= 5000) {
        t0 = now;
        fprintf(stderr, "[present] 5 s: tique=%ld drenagem=%ld contador=%ld (flips do jogo=%u, pulados no atraso=%ld)\n",
                InterlockedExchange(&s_pres_src[0], 0), InterlockedExchange(&s_pres_src[1], 0),
                InterlockedExchange(&s_pres_src[2], 0), cellGcm_flip_request_count(), cellGcm_flip_catchup_count());
    }
}
static void present_guest_frame(void)
{
    rsx_present_frame();
    /* The ticker increments, a guest thread reads: interlocked rather than a
     * volatile ++, which is neither atomic nor a fence on arm64. */
    InterlockedIncrement(&g_frames_presented);
}

/* ---- Guest-PC sampling profiler (PS3_GUEST_PROF=1) -----------------------
 * Samples every guest thread's ctx.cia every ~5ms and dumps the top sites
 * every 5s. cia is refreshed at every syscall, and guest spin loops issue a
 * syscall per iteration (yield/usleep), so a thread stuck in a wait loop
 * shows its loop's PC dominating the histogram. This is the tool that
 * answers "where does thread X spend its wall time" without a debugger. */
#include "sys_ppu_thread.h"
static DWORD WINAPI guest_prof_thread(LPVOID)
{
    struct Site { uint32_t tid, cia; uint32_t hits; };
    static Site sites[256];
    int nsites = 0;
    ULONGLONG t0 = GetTickCount64();
    unsigned total = 0;
    for (;;) {
        Sleep(5);
        for (int i = 0; i < PPU_THREAD_MAX; i++) {
            ppu_thread_info* t = &g_ppu_threads[i];
            if (t->state != 1 /*RUNNING-ish; harmless if enum differs*/ && t->state != 2) continue;
            uint32_t cia = (uint32_t)t->ctx.cia;
            if (!cia) continue;
            uint32_t tid = (uint32_t)(i + 1);
            total++;
            int f = -1;
            for (int k = 0; k < nsites; k++)
                if (sites[k].tid == tid && sites[k].cia == cia) { f = k; break; }
            if (f < 0 && nsites < 256) { f = nsites++; sites[f].tid = tid; sites[f].cia = cia; sites[f].hits = 0; }
            if (f >= 0) sites[f].hits++;
        }
        ULONGLONG now = GetTickCount64();
        if (now - t0 >= 5000) {
            /* top 10 by hits */
            fprintf(stderr, "[gprof] %u samples/5s; top sites:\n", total);
            for (int rank = 0; rank < 10; rank++) {
                int best = -1; uint32_t bh = 0;
                for (int k = 0; k < nsites; k++)
                    if (sites[k].hits > bh) { bh = sites[k].hits; best = k; }
                if (best < 0 || bh == 0) break;
                fprintf(stderr, "[gprof]   tid=%-3u cia=0x%08X  %5.1f%%  (%s)\n",
                        sites[best].tid, sites[best].cia,
                        100.0 * sites[best].hits / (total ? total : 1),
                        g_ppu_threads[sites[best].tid - 1].name);
                sites[best].hits = 0;   /* consume */
            }
            fflush(stderr);
            nsites = 0; total = 0; t0 = now;
        }
    }
}

#include <tlhelp32.h>
#include <algorithm>
#include <vector>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs")
/* GOWA (25/10): o ícone da janela (barra de tarefas, Alt+Tab) é o ICON0.PNG da cópia do
 * jogador (PS3_VFS_ROOT\PS3_GAME\ICON0.PNG, 320x176), lido pela WIC, em quadrado com
 * faixas transparentes. Sem o arquivo, fica o ícone de antes. */
static HICON gowa_icone_png(const wchar_t* caminho, int lado)
{
    IWICImagingFactory* fab = NULL; IWICBitmapDecoder* dec = NULL; IWICBitmapFrameDecode* qd = NULL;
    IWICBitmapScaler* esc = NULL; IWICFormatConverter* cnv = NULL; HICON ic = NULL;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fab)))) return NULL;
    UINT w = 0, h = 0;
    if (SUCCEEDED(fab->CreateDecoderFromFilename(caminho, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec)) &&
        SUCCEEDED(dec->GetFrame(0, &qd)) && SUCCEEDED(qd->GetSize(&w, &h)) && w && h) {
        UINT tw = (UINT)lado, th = (UINT)((double)lado * h / w + 0.5);
        if (th > (UINT)lado) { th = (UINT)lado; tw = (UINT)((double)lado * w / h + 0.5); }
        if (!tw) tw = 1;
        if (!th) th = 1;
        if (SUCCEEDED(fab->CreateBitmapScaler(&esc)) &&
            SUCCEEDED(esc->Initialize(qd, tw, th, WICBitmapInterpolationModeHighQualityCubic)) &&
            SUCCEEDED(fab->CreateFormatConverter(&cnv)) &&
            SUCCEEDED(cnv->Initialize(esc, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom))) {
            std::vector<uint32_t> px((size_t)tw * th);
            if (SUCCEEDED(cnv->CopyPixels(NULL, tw * 4, (UINT)(px.size() * 4), (BYTE*)px.data()))) {
                BITMAPV5HEADER bh; memset(&bh, 0, sizeof bh);
                bh.bV5Size = sizeof bh; bh.bV5Width = lado; bh.bV5Height = -lado; bh.bV5Planes = 1; bh.bV5BitCount = 32;
                bh.bV5Compression = BI_BITFIELDS; bh.bV5RedMask = 0x00FF0000; bh.bV5GreenMask = 0x0000FF00;
                bh.bV5BlueMask = 0x000000FF; bh.bV5AlphaMask = 0xFF000000;
                void* bits = NULL; HDC dc = GetDC(NULL);
                HBITMAP cor = CreateDIBSection(dc, (BITMAPINFO*)&bh, DIB_RGB_COLORS, &bits, NULL, 0);
                ReleaseDC(NULL, dc);
                std::vector<BYTE> zeros((size_t)((lado + 15) / 16) * 2 * lado, 0);
                HBITMAP masc = CreateBitmap(lado, lado, 1, 1, zeros.data());
                if (cor && masc && bits) {
                    uint32_t* dst = (uint32_t*)bits;
                    memset(dst, 0, (size_t)lado * lado * 4);
                    const UINT ox = ((UINT)lado - tw) / 2, oy = ((UINT)lado - th) / 2;
                    for (UINT y = 0; y < th; y++) memcpy(dst + (size_t)(oy + y) * lado + ox, px.data() + (size_t)y * tw, tw * 4);
                    ICONINFO ii; memset(&ii, 0, sizeof ii); ii.fIcon = TRUE; ii.hbmColor = cor; ii.hbmMask = masc;
                    ic = CreateIconIndirect(&ii);
                }
                if (cor) DeleteObject(cor);
                if (masc) DeleteObject(masc);
            }
        }
    }
    if (cnv) cnv->Release();
    if (esc) esc->Release();
    if (qd) qd->Release();
    if (dec) dec->Release();
    fab->Release();
    return ic;
}
static void gowa_icone_janela(HWND hw)
{
    if (!hw) return;
    wchar_t raiz[MAX_PATH]; const DWORD n = GetEnvironmentVariableW(L"PS3_VFS_ROOT", raiz, MAX_PATH);
    if (!n || n >= MAX_PATH - 32) return;
    wchar_t cam[MAX_PATH]; swprintf(cam, MAX_PATH, L"%s\\PS3_GAME\\ICON0.PNG", raiz);
    if (GetFileAttributesW(cam) == INVALID_FILE_ATTRIBUTES) return;
    const HRESULT co = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    HICON grande = gowa_icone_png(cam, GetSystemMetrics(SM_CXICON) * 3 / 2);   /* a barra de tarefas reduz o grande */
    HICON pequeno = gowa_icone_png(cam, GetSystemMetrics(SM_CXSMICON));
    if (grande) SendMessageW(hw, WM_SETICON, ICON_BIG, (LPARAM)grande);
    if (pequeno) SendMessageW(hw, WM_SETICON, ICON_SMALL, (LPARAM)pequeno);
    fprintf(stderr, "[janela] ícone do jogo: %s\n", grande || pequeno ? "ICON0.PNG" : "não carregou");
    if (SUCCEEDED(co)) CoUninitialize();
}
extern "C" { int g_gowa_pin_on = -1; }   /* 25/10: fixação de threads ligada? (desempenho.json) */
/* ---- GOWA: one heavy thread per physical core (GOWA_PIN=1) -------------
 * The frame needs ~9 busy threads (6-7 SPUs, the main PPU thread, the RSX /
 * present thread) on 8 cores, and Windows stacked them on SMT siblings with a
 * core left idle (chapter 16: cores 1/3/5 at ~150% of one logical CPU, core 7
 * at 28%). Every 5 s this measures each thread's CPU time over 1 s and pins
 * the heaviest ones (> 25% of a CPU), one per physical core, to that core's
 * first logical CPU; the rest stay free (the siblings). Role-agnostic: it
 * follows whatever is heavy (loading, menu, gameplay). A thread that drops out
 * gets the process mask back. GOWA_PIN_START=<s> (default 30). */
/* Núcleos físicos (RelationProcessorCore), para decidir o padrão do GOWA_PIN. */
static int gowa_nucleos_fisicos(void)
{
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, NULL, &len);
    if (!len) return 0;
    uint8_t* buf = (uint8_t*)malloc(len);
    int n = 0;
    if (buf && GetLogicalProcessorInformationEx(RelationProcessorCore, (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)buf, &len))
        for (DWORD off = 0; off < len; off += ((const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)(buf + off))->Size) n++;
    free(buf);
    return n;
}
static DWORD WINAPI gowa_pin_thread(LPVOID)
{
    const char* st = getenv("GOWA_PIN_START");
    Sleep((st ? (DWORD)atoi(st) : 30u) * 1000u);
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, NULL, &len);
    if (!len) return 0;
    uint8_t* buf = (uint8_t*)malloc(len);
    if (!buf || !GetLogicalProcessorInformationEx(RelationProcessorCore,
                    (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)buf, &len)) { free(buf); return 0; }
    DWORD_PTR core_first[64], core_all[64]; int ncores = 0;
    for (DWORD off = 0; off < len && ncores < 64; ) {
        const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* e = (const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)(buf + off);
        const KAFFINITY m = e->Processor.GroupMask[0].Mask;
        if (m) { core_all[ncores] = (DWORD_PTR)m; core_first[ncores++] = (DWORD_PTR)(m & (~m + 1)); }   /* lowest logical CPU of the core */
        off += e->Size;
    }
    free(buf);
    DWORD_PTR proc_mask = 0, sys_mask = 0;
    GetProcessAffinityMask(GetCurrentProcess(), &proc_mask, &sys_mask);
    fprintf(stderr, "[pin] %d nucleos fisicos; fixando as threads mais pesadas, uma por nucleo\n", ncores);
    struct pin_t { DWORD tid; ULONGLONG t0; ULONGLONG used; int core; };
    static pin_t th[512];
    DWORD pinned_tid[64] = {0};
    const DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    for (int round = 0; ; round++) {
        int n = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;
        THREADENTRY32 te; te.dwSize = sizeof te;
        for (BOOL ok = Thread32First(snap, &te); ok && n < 512; ok = Thread32Next(snap, &te)) {
            if (te.th32OwnerProcessID != pid || te.th32ThreadID == self) continue;
            HANDLE h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID);
            if (!h) continue;
            FILETIME c, x, k, u;
            if (GetThreadTimes(h, &c, &x, &k, &u)) {
                th[n].tid = te.th32ThreadID;
                th[n].t0 = (((ULONGLONG)k.dwHighDateTime << 32) | k.dwLowDateTime) +
                           (((ULONGLONG)u.dwHighDateTime << 32) | u.dwLowDateTime);
                th[n].used = 0; th[n].core = -1; n++;
            }
            CloseHandle(h);
        }
        CloseHandle(snap);
        Sleep(1000);
        for (int i = 0; i < n; i++) {
            HANDLE h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, th[i].tid);
            if (!h) continue;
            FILETIME c, x, k, u;
            if (GetThreadTimes(h, &c, &x, &k, &u)) {
                const ULONGLONG t1 = (((ULONGLONG)k.dwHighDateTime << 32) | k.dwLowDateTime) +
                                     (((ULONGLONG)u.dwHighDateTime << 32) | u.dwLowDateTime);
                th[i].used = t1 - th[i].t0;   /* 100 ns units over ~1 s */
            }
            CloseHandle(h);
        }
        /* heaviest first */
        std::sort(th, th + n, [](const pin_t& a, const pin_t& b) { return a.used > b.used; });
        int heavy = 0;
        while (heavy < n && heavy < ncores && th[heavy].used > 2500000ull) heavy++;   /* > 25% of 1 s */
        /* keep a thread on the core it already has; give the free cores to the new ones */
        int core_of[64]; for (int k2 = 0; k2 < 64; k2++) core_of[k2] = -1;
        DWORD new_pinned[64] = {0};
        for (int i = 0; i < heavy; i++)
            for (int k2 = 0; k2 < ncores; k2++)
                if (pinned_tid[k2] == th[i].tid) { th[i].core = k2; new_pinned[k2] = th[i].tid; }
        for (int i = 0; i < heavy; i++) {
            if (th[i].core >= 0) continue;
            for (int k2 = 0; k2 < ncores; k2++)
                if (!new_pinned[k2]) { th[i].core = k2; new_pinned[k2] = th[i].tid; break; }
        }
        (void)core_of;
        /* threads that lost their core get the process mask back */
        for (int k2 = 0; k2 < ncores; k2++) {
            if (!pinned_tid[k2]) continue;
            int still = 0;
            for (int k3 = 0; k3 < ncores; k3++) if (new_pinned[k3] == pinned_tid[k2]) still = 1;
            if (!still) {
                HANDLE h = OpenThread(THREAD_SET_INFORMATION | THREAD_QUERY_INFORMATION, FALSE, pinned_tid[k2]);
                if (h) { SetThreadAffinityMask(h, proc_mask); CloseHandle(h); }
            }
        }
        for (int i = 0; i < heavy; i++) {
            if (th[i].core < 0) continue;
            int was = 0;
            for (int k2 = 0; k2 < ncores; k2++) if (pinned_tid[k2] == th[i].tid && k2 == th[i].core) was = 1;
            if (was) continue;
            HANDLE h = OpenThread(THREAD_SET_INFORMATION | THREAD_QUERY_INFORMATION, FALSE, th[i].tid);
            if (h) { SetThreadAffinityMask(h, core_first[th[i].core] & proc_mask); CloseHandle(h); }
        }
        memcpy(pinned_tid, new_pinned, sizeof pinned_tid);
        DWORD_PTR keep_off_all = 0;   /* logical CPUs the threads that are not pinned keep off */
        /* GOWA_PIN=2: the threads NOT pinned keep off the SMT siblings of the
         * cores of the 4 heaviest ones (the EDGE SPUs, the frame's critical
         * path): an SMT neighbour there slowed them ~30%. They get the other
         * siblings (and the cores of lighter pinned threads stay shared). */
        { static int mode = -1; if (mode < 0) { const char* e = getenv("GOWA_PIN"); mode = (e && *e == '2') ? 2 : (e && *e == '3') ? 3 : 1; }
          /* GOWA_PIN=3 (24/10): the threads NOT pinned keep off the SMT siblings of the
           * EDGE SPUs (threads named SPU0-SPU3) and of the main PPU thread (the heavy one
           * with no name). Chapter 16 has 9 heavy threads for 8 cores (main, RSX, SPU0-6):
           * the 9th floated onto whichever sibling, and which one it was held for the whole
           * run -- the per-run bimodal FPS (~59 or ~63-64). Here it and the light ones
           * share the cores of SPU 4/5/6 (which have slack); the RSX keeps its own core. */
          if (mode == 3 && heavy >= 4) {
              DWORD_PTR keep_off = 0;
              for (int i = 0; i < heavy; i++) {
                  const int c = th[i].core;
                  if (c < 0) continue;
                  WCHAR nm[16] = L"";
                  HANDLE h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, th[i].tid);
                  if (h) { PWSTR w = NULL; if (SUCCEEDED(GetThreadDescription(h, &w)) && w) { wcsncpy(nm, w, 15); LocalFree(w); } CloseHandle(h); }
                  const int edge = nm[0] == L'S' && nm[1] == L'P' && nm[2] == L'U' && nm[3] >= L'0' && nm[3] <= L'3' && !nm[4];
                  if (edge || !nm[0])
                      keep_off |= core_all[c] & ~core_first[c];
              }
              keep_off_all |= keep_off;
              if (round < 3) fprintf(stderr, "[pin] modo 3: fora dos irmaos 0x%llX\n", (unsigned long long)keep_off);
          }
          if (mode == 2 && heavy >= 4) {
              DWORD_PTR keep_off = 0;
              for (int i = 0; i < 4 && i < heavy; i++) {
                  const int c = th[i].core;
                  if (c < 0) continue;
                  /* the core's other logical CPUs: its mask minus the first */
                  keep_off |= core_all[c] & ~core_first[c];
              }
              keep_off_all |= keep_off;   /* applied below, together with GOWA_PIN_RSX_SOLO (22/10) */
              if (round < 3) fprintf(stderr, "[pin] modo 2: fora dos irmaos 0x%llX\n", (unsigned long long)keep_off);
          } }
        /* GOWA_PIN_RSX_SOLO=1 (21/10): the RSX-present thread gets its physical core to
         * itself -- every thread that is not pinned is kept off BOTH logical CPUs of that
         * core. Chapter 16 runs bimodal (~40 / ~36 FPS for a whole run) with the same work
         * per frame (2,920 draws); in the slow runs the PPU and the SPUs both wait twice as
         * long, i.e. on the RSX thread, which is ~98% busy and ~16% slower per draw. */
        { static int solo = -1; if (solo < 0) { const char* e = getenv("GOWA_PIN_RSX_SOLO"); solo = (e && *e == '1') ? 1 : 0; }
          static DWORD rsx_tid = 0;
          if (solo) {
              int c_rsx = -1;
              for (int i = 0; i < heavy && c_rsx < 0; i++) {
                  if (!rsx_tid) {
                      HANDLE h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, th[i].tid);
                      if (h) { PWSTR w = NULL; if (SUCCEEDED(GetThreadDescription(h, &w)) && w) { if (!wcscmp(w, L"RSX-present")) rsx_tid = th[i].tid; LocalFree(w); } CloseHandle(h); }
                  }
                  if (rsx_tid && th[i].tid == rsx_tid) c_rsx = th[i].core;
              }
              if (c_rsx >= 0) {
                  keep_off_all |= core_all[c_rsx];
                  const DWORD_PTR light = proc_mask & ~keep_off_all;
                  static int said_core = -1;
                  if (said_core != c_rsx) { said_core = c_rsx; fprintf(stderr, "[pin] RSX sozinha no nucleo %d (rodada %d): leves em 0x%llX\n", c_rsx, round, (unsigned long long)light); }
              }
          } }
        if (keep_off_all && (proc_mask & ~keep_off_all)) {
            const DWORD_PTR light = proc_mask & ~keep_off_all;
            for (int i = heavy; i < n; i++) {
                HANDLE h = OpenThread(THREAD_SET_INFORMATION | THREAD_QUERY_INFORMATION, FALSE, th[i].tid);
                if (h) { SetThreadAffinityMask(h, light); CloseHandle(h); }
            }
        }
        if (round < 3 || (round % 6) == 0) {
            fprintf(stderr, "[pin] rodada %d:", round);
            for (int i = 0; i < heavy; i++) {
                char nm[40] = "";
                HANDLE h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, th[i].tid);
                if (h) { PWSTR w = NULL; if (SUCCEEDED(GetThreadDescription(h, &w)) && w) { WideCharToMultiByte(CP_UTF8, 0, w, -1, nm, sizeof nm, NULL, NULL); LocalFree(w); } CloseHandle(h); }
                fprintf(stderr, " %s:tid%lu=%.0f%%->n%d", nm[0] ? nm : "?", (unsigned long)th[i].tid, th[i].used / 100000.0, th[i].core);
            }
            fprintf(stderr, "\n");
        }
        Sleep(4000);
    }
}

/* ---- GOWA_TL=<início s>[:<duração s>[:<período µs>]] (22/10): linha do tempo ------------
 * Caminho crítico do quadro: a cada período (padrão 500 µs, timer de alta resolução) anota o
 * estado de cada SPU (workload LS 0x1DF, imagem em execução, bloqueada num canal, pc), da RSX
 * (posição do walker, flips decodificados, estacionada num self-JUMP) e das 16 primeiras
 * threads PPU (syscall/HLE em que estão, último ponto de chamada). Grava tudo no fim em
 * out/tl_<pid>.bin (scripts/tl_analise.py). Só diagnóstico; desligado por padrão. */
extern "C" int gowa_spu_sample2(int s, unsigned* wid, unsigned* pc, int* interp, int* img, unsigned* status);
extern "C" uint32_t gowa_tl_rsx(uint32_t* getoff, uint32_t* flips);
extern "C" uint32_t g_sc_inflight[64];      /* ppu_loader.cpp */
extern "C" const char* g_hle_inflight[64];  /* ppu_hle.cpp */
extern "C" const uint8_t* gowa_spu_ls(int s);
extern "C" uint8_t* vm_base;
/* os módulos de job WWS (port/gowa_jobmods.inc) */
static const uint32_t s_tl_mods[] = { 0x006EB400u, 0x006EDC00u, 0x006EF200u, 0x006FC080u, 0x00710200u, 0x00712C00u,
    0x00712F00u, 0x0071EE00u, 0x00727200u, 0x0072C100u, 0x0072DF80u, 0x0073A400u, 0x0073E000u, 0x0073EE00u,
    0x0073FA80u, 0x00740B80u, 0x00741B00u, 0x0075CC80u, 0x00770F80u, 0x00781D80u, 0x00789F00u, 0x00790F80u,
    0x00792E00u, 0x0079C680u, 0x0079DC00u, 0x007A1800u, 0x007B6680u, 0x007D8600u, 0x007F5C80u, 0x0080F000u,
    0x0084BCC0u };
#pragma pack(push, 1)
struct gowa_tl_spu { uint8_t wid, st, interp, ok; int32_t img; uint32_t pc; };
struct gowa_tl_ppu { uint16_t sc; uint8_t hle, state; uint32_t pc; };
struct gowa_tl_amostra {
    uint32_t t_us, rsx_get, rsx_flips; uint8_t rsx_park, pad[3];
    gowa_tl_spu spu[8]; gowa_tl_ppu ppu[16];
    uint32_t main_fn; uint8_t main_via, pad2[3];   /* função PPU traduzida da thread principal (RIP) */
    uint64_t cyc_rsx, cyc_main;                    /* QueryThreadCycleTime acumulado (RSX-present, principal) */
    uint32_t rsx_rva, rsx_via;                     /* RIP da RSX-present no exe (ou 0xFFFFFFFF) e o 1o retorno no exe da pilha */
    uint32_t rsx_mod, pad3;                        /* fora do exe: índice do módulo (nomes no fim do arquivo) */
};
#pragma pack(pop)
static DWORD WINAPI gowa_tl_thread(LPVOID arg)
{
    /* a thread principal do jogo é a do processo (quem cria esta), fora do g_ppu_threads */
    const DWORD tid_main = (DWORD)(uintptr_t)arg;
    const char* e = getenv("GOWA_TL");
    unsigned ini = (unsigned)strtoul(e, NULL, 10), dur = 20, per = 500;
    if (const char* c = strchr(e, ':')) { dur = (unsigned)strtoul(c + 1, NULL, 10);
        if (const char* c2 = strchr(c + 1, ':')) per = (unsigned)strtoul(c2 + 1, NULL, 10); }
    if (per < 100) per = 100;
    const size_t cap = (size_t)dur * 1000000u / per + 1024;
    gowa_tl_amostra* a = (gowa_tl_amostra*)calloc(cap, sizeof *a);
    if (!a) return 0;
    const char* nomes[255]; int nn = 0;          /* índice 1.. dos nomes de HLE */
    Sleep(ini * 1000u);
    /* RIP da thread principal -> função PPU traduzida (como o GOWA_HOSTPROF); fora do código
     * traduzido, o primeiro endereço de retorno traduzido na pilha (via = 1) */
    struct tl_fn { uintptr_t h; uint32_t g; };
    const size_t nf = (size_t)function_table_count;
    tl_fn* ft = (tl_fn*)malloc(sizeof(tl_fn) * (nf ? nf : 1));
    for (size_t q = 0; q < nf; q++) { ft[q].h = (uintptr_t)function_table[q].func; ft[q].g = function_table[q].addr; }
    std::sort(ft, ft + nf, [](const tl_fn& x, const tl_fn& y) { return x.h < y.h; });
    const uintptr_t flo = nf ? ft[0].h : 0, fhi = nf ? ft[nf - 1].h + 0x40000u : 0;
    auto flook = [&](uintptr_t pc) -> uint32_t {
        if (!nf || pc < flo || pc >= fhi) return 0;
        size_t a = 0, b = nf;
        while (a + 1 < b) { size_t m = (a + b) / 2; if (ft[m].h <= pc) a = m; else b = m; }
        return (pc - ft[a].h) < 0x40000u ? ft[a].g : 0;
    };
    HANDLE hmain = NULL, hrsx = NULL;
    HMODULE mods[32]; int nmods = 0;
    uintptr_t ex_lo = 0, ex_hi = 0;
    { HMODULE self = GetModuleHandleA(NULL); const uint8_t* bb = (const uint8_t*)self;
      const IMAGE_NT_HEADERS64* nt = (const IMAGE_NT_HEADERS64*)(bb + ((const IMAGE_DOS_HEADER*)bb)->e_lfanew);
      ex_lo = (uintptr_t)bb; ex_hi = ex_lo + nt->OptionalHeader.SizeOfImage; }
    /* a thread RSX-present (nome dado no vblank_ticker) */
    { HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0); THREADENTRY32 te; te.dwSize = sizeof te;
      if (snap != INVALID_HANDLE_VALUE && Thread32First(snap, &te)) do {
          if (te.th32OwnerProcessID != GetCurrentProcessId()) continue;
          HANDLE h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID);
          if (!h) continue;
          PWSTR w = NULL; int e = 0;
          if (SUCCEEDED(GetThreadDescription(h, &w)) && w) { e = !wcscmp(w, L"RSX-present"); LocalFree(w); }
          if (e && !hrsx) { CloseHandle(h); hrsx = OpenThread(THREAD_QUERY_LIMITED_INFORMATION | THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID); }
          else CloseHandle(h);
      } while (Thread32Next(snap, &te));
      if (snap != INVALID_HANDLE_VALUE) CloseHandle(snap); }
    HANDLE tm = CreateWaitableTimerExW(NULL, NULL, 0x2 /*CREATE_WAITABLE_TIMER_HIGH_RESOLUTION*/, TIMER_ALL_ACCESS);
    LARGE_INTEGER f, t0, now; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t0);
    size_t n = 0;
    for (;;) {
        QueryPerformanceCounter(&now);
        const double us = (double)(now.QuadPart - t0.QuadPart) * 1e6 / (double)f.QuadPart;
        if (us >= dur * 1e6 || n >= cap) break;
        gowa_tl_amostra* s = &a[n++];
        s->t_us = (uint32_t)us;
        s->rsx_park = (uint8_t)gowa_tl_rsx(&s->rsx_get, &s->rsx_flips);
        for (int k = 0; k < 8; k++) {
            unsigned wid, pc, st; int in, img;
            if (!gowa_spu_sample2(k, &wid, &pc, &in, &img, &st)) continue;
            s->spu[k].ok = 1; s->spu[k].wid = (uint8_t)wid; s->spu[k].interp = (uint8_t)in;
            s->spu[k].st = (uint8_t)st; s->spu[k].pc = pc;
            /* módulo de job no buffer de código (LS 0x4000): impressão dos 32 primeiros bytes,
             * comparada no fim com a de cada módulo na memória principal */
            { const uint8_t* ls = gowa_spu_ls(k); uint32_t h = 2166136261u;
              if (ls) for (int q = 0; q < 32; q++) h = (h ^ ls[0x4000 + q]) * 16777619u;
              s->spu[k].img = (int32_t)h; (void)img; }
        }
        for (int k = 0; k < 16 && k < PPU_THREAD_MAX; k++) {
            const ppu_thread_info* t = &g_ppu_threads[k];
            const unsigned tid = (unsigned)(k + 1);
            s->ppu[k].state = (uint8_t)t->state;
            s->ppu[k].sc = (uint16_t)g_sc_inflight[tid & 63];
            s->ppu[k].pc = t->prof_pc;
            const char* h = g_hle_inflight[tid & 63];
            uint8_t hi = 0;
            if (h) { for (int q = 0; q < nn; q++) if (nomes[q] == h) { hi = (uint8_t)(q + 1); break; }
                     if (!hi && nn < 255) { nomes[nn] = h; hi = (uint8_t)(++nn); } }
            s->ppu[k].hle = hi;
        }
        if (!hmain && tid_main)
            hmain = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid_main);
        if (hmain && SuspendThread(hmain) != (DWORD)-1) {
            CONTEXT c; c.ContextFlags = CONTEXT_CONTROL;
            if (GetThreadContext(hmain, &c)) {
                uint32_t g = flook((uintptr_t)c.Rip); uint8_t via = 0;
                if (!g) {
                    MEMORY_BASIC_INFORMATION mbi; const uint64_t* sp = (const uint64_t*)c.Rsp;
                    if (VirtualQuery(sp, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
                        const uint64_t end = (uint64_t)mbi.BaseAddress + mbi.RegionSize;
                        for (int q = 0; q < 2048 && (uint64_t)(sp + q + 1) <= end; q++)
                            if ((g = flook((uintptr_t)sp[q])) != 0) { via = 1; break; }
                    }
                }
                s->main_fn = g; s->main_via = via;
            }
            ResumeThread(hmain);
        }
        /* onde está a RSX-present: RVA no exe, ou o módulo + o 1o endereço de retorno do exe na pilha */
        if (hrsx && SuspendThread(hrsx) != (DWORD)-1) {
            CONTEXT c; c.ContextFlags = CONTEXT_CONTROL;
            s->rsx_rva = s->rsx_via = 0xFFFFFFFFu;
            if (GetThreadContext(hrsx, &c)) {
                const uintptr_t rip = (uintptr_t)c.Rip;
                if (rip >= ex_lo && rip < ex_hi) s->rsx_rva = (uint32_t)(rip - ex_lo);
                else {
                    HMODULE m = NULL;
                    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)rip, &m);
                    int mi = -1; for (int q = 0; q < nmods; q++) if (mods[q] == m) { mi = q; break; }
                    if (mi < 0 && nmods < 32) { mods[nmods] = m; mi = nmods++; }
                    s->rsx_mod = (uint32_t)mi;
                    MEMORY_BASIC_INFORMATION mbi; const uint64_t* sp = (const uint64_t*)c.Rsp;
                    if (VirtualQuery(sp, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
                        const uint64_t end = (uint64_t)mbi.BaseAddress + mbi.RegionSize;
                        for (int q = 0; q < 4096 && (uint64_t)(sp + q + 1) <= end; q++)
                            if (sp[q] >= ex_lo && sp[q] < ex_hi) { s->rsx_via = (uint32_t)(sp[q] - ex_lo); break; }
                    }
                }
            }
            ResumeThread(hrsx);
        }
        { ULONG64 c = 0; if (hrsx && QueryThreadCycleTime(hrsx, &c)) s->cyc_rsx = c;
          c = 0; if (hmain && QueryThreadCycleTime(hmain, &c)) s->cyc_main = c; }
        if (tm) { LARGE_INTEGER due; due.QuadPart = -(LONGLONG)per * 10; SetWaitableTimer(tm, &due, 0, NULL, NULL, FALSE); WaitForSingleObject(tm, 50); }
        else Sleep(1);
    }
    char p[MAX_PATH];
    snprintf(p, sizeof p, "out/tl_%lu.bin", (unsigned long)GetCurrentProcessId());
    if (FILE* fo = fopen(p, "wb")) {
        /* cabeçalho: "GOWATL1\0", n, tamanho do registro, nomes de HLE, nomes das threads PPU */
        const uint32_t hdr[2] = { (uint32_t)n, (uint32_t)sizeof *a };
        fwrite("GOWATL5", 1, 8, fo); fwrite(hdr, 4, 2, fo);
        const uint32_t nh = (uint32_t)nn; fwrite(&nh, 4, 1, fo);
        for (int q = 0; q < nn; q++) { char b[64] = { 0 }; strncpy(b, nomes[q], 63); fwrite(b, 1, 64, fo); }
        for (int k = 0; k < 16; k++) { char b[32] = { 0 }; if (k < PPU_THREAD_MAX) strncpy(b, g_ppu_threads[k].name, 31); fwrite(b, 1, 32, fo); }
        /* impressões dos módulos de job (mesma conta sobre a imagem na memória) */
        { const uint32_t nm = (uint32_t)(sizeof s_tl_mods / sizeof s_tl_mods[0]); fwrite(&nm, 4, 1, fo);
          for (uint32_t q = 0; q < nm; q++) { uint32_t h = 2166136261u;
              if (vm_base) for (int z = 0; z < 32; z++) h = (h ^ vm_base[s_tl_mods[q] + z]) * 16777619u;
              const uint32_t par[2] = { s_tl_mods[q], h }; fwrite(par, 4, 2, fo); } }
        fwrite(a, sizeof *a, n, fo);
        /* módulos da RSX fora do exe */
        { const uint32_t q = (uint32_t)nmods; fwrite(&q, 4, 1, fo);
          for (int k = 0; k < nmods; k++) { char nm[64] = { 0 }, path[MAX_PATH] = { 0 };
              if (mods[k]) GetModuleFileNameA(mods[k], path, MAX_PATH);
              const char* bn = strrchr(path, '\\'); strncpy(nm, bn ? bn + 1 : path, 63); fwrite(nm, 1, 64, fo); } }
        fclose(fo);
        fprintf(stderr, "[tl] %zu amostras (%u µs) gravadas em %s\n", n, per, p);
    }
    free(a); free(ft);
    if (hmain) CloseHandle(hmain);
    if (hrsx) CloseHandle(hrsx);
    return 0;
}

/* ---- GOWA: host CPU sampler (GOWA_HOSTPROF=<start_s>[:<dur_s>]) ---------
 * Every ~2 ms suspends each thread of the process, reads RIP and charges the
 * sample to the lifted guest function containing it -- or, when RIP is in
 * runtime code / another DLL, to the first lifted return address on the stack
 * (marked "via"). Unlike PS3_GUEST_PROF (ctx.cia, refreshed only at
 * syscalls) this shows where the CPU actually goes. One report per thread at
 * the end of the window. Host tid -> guest thread name via g_ppu_threads. */
struct gowa_hp_fn { uintptr_t h; uint32_t g; };
static int gowa_hp_cmp(const void* a, const void* b)
{
    const uintptr_t x = ((const gowa_hp_fn*)a)->h, y = ((const gowa_hp_fn*)b)->h;
    return x < y ? -1 : x > y ? 1 : 0;
}
extern "C" uint32_t spu_registry_size(void);
extern "C" int spu_registry_entry_img(uint32_t i, void** host, uint32_t* ls_addr, int* image_id);
struct gowa_hp_sfn { uintptr_t h; uint32_t ls; int img; };
static int gowa_hp_scmp(const void* a, const void* b)
{
    const uintptr_t x = ((const gowa_hp_sfn*)a)->h, y = ((const gowa_hp_sfn*)b)->h;
    return x < y ? -1 : x > y ? 1 : 0;
}
/* "module!nearest_export+0x.." for a pc in a DLL (export table walk). */
static void gowa_hp_dllsym(uintptr_t pc, char* out, size_t n)
{
    HMODULE m = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)pc, &m) || !m) { snprintf(out, n, "?%p", (void*)pc); return; }
    char path[MAX_PATH]; path[0] = 0; GetModuleFileNameA(m, path, sizeof path);
    const char* bn = strrchr(path, '\\'); bn = bn ? bn + 1 : path;
    const uint8_t* b = (const uint8_t*)m;
    const IMAGE_NT_HEADERS64* nt = (const IMAGE_NT_HEADERS64*)(b + ((const IMAGE_DOS_HEADER*)b)->e_lfanew);
    const IMAGE_DATA_DIRECTORY dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    const uint32_t rva = (uint32_t)(pc - (uintptr_t)b);
    const char* best = NULL; uint32_t brva = 0;
    if (dd.VirtualAddress) {
        const IMAGE_EXPORT_DIRECTORY* ed = (const IMAGE_EXPORT_DIRECTORY*)(b + dd.VirtualAddress);
        const uint32_t* fn = (const uint32_t*)(b + ed->AddressOfFunctions);
        const uint32_t* nm = (const uint32_t*)(b + ed->AddressOfNames);
        const uint16_t* od = (const uint16_t*)(b + ed->AddressOfNameOrdinals);
        for (uint32_t i = 0; i < ed->NumberOfNames; i++) {
            const uint32_t f = fn[od[i]];
            if (f <= rva && (!best || f > brva)) { brva = f; best = (const char*)(b + nm[i]); }
        }
    }
    if (best) snprintf(out, n, "%s!%s+0x%X", bn, best, rva - brva);
    else snprintf(out, n, "%s+0x%X", bn, rva);
}
/* 25/10: lidas antes da amostragem -- um getenv com a thread amostrada suspensa
 * espera para sempre se ela estiver segurando a trava do ambiente da CRT (o perfil
 * por instrução derrubava o jogo no notebook, sempre na mesma janela) */
static int s_hp_rip = -1, s_hp_rip2 = -1;
static DWORD WINAPI gowa_hostprof_thread(LPVOID)
{
    const char* e = getenv("GOWA_HOSTPROF");
    { const char* r = getenv("GOWA_HOSTPROF_RIP"); s_hp_rip = r ? 1 : 0; s_hp_rip2 = r && *r == '2'; }
    unsigned start_s = (unsigned)strtoul(e, NULL, 10), dur_s = 10;
    if (const char* c = strchr(e, ':')) dur_s = (unsigned)strtoul(c + 1, NULL, 10);
    Sleep(start_s * 1000u);
    const uint64_t nf = function_table_count;
    gowa_hp_fn* ft = (gowa_hp_fn*)malloc(sizeof(gowa_hp_fn) * (size_t)nf);
    if (!ft) return 0;
    for (uint64_t q = 0; q < nf; q++) { ft[q].h = (uintptr_t)function_table[q].func; ft[q].g = function_table[q].addr; }
    qsort(ft, (size_t)nf, sizeof *ft, gowa_hp_cmp);
    const uintptr_t lo = ft[0].h, hi = ft[nf - 1].h;
    /* lifted SPU functions too: an SPU thread's RIP is charged to the SPU
     * function (image, LS address) whose host body contains it -- the only way
     * to see where a NATIVE SPU loop spins (ctx->pc is not kept up to date in
     * straight-line lifted code). */
    const uint32_t ns = spu_registry_size();
    gowa_hp_sfn* st = (gowa_hp_sfn*)malloc(sizeof(gowa_hp_sfn) * (size_t)(ns ? ns : 1));
    uint32_t nsv = 0;
    for (uint32_t q = 0; st && q < ns; q++) { void* h; uint32_t ls; int img;
        if (spu_registry_entry_img(q, &h, &ls, &img) && h) { st[nsv].h = (uintptr_t)h; st[nsv].ls = ls; st[nsv].img = img; nsv++; } }
    if (st && nsv) qsort(st, nsv, sizeof *st, gowa_hp_scmp);
    auto slookup = [&](uintptr_t pc, uint32_t* key) -> int {
        if (!st || !nsv || pc < st[0].h || pc >= st[nsv - 1].h + 0x40000u) return 0;
        /* exact function start from the x64 unwind table: "nearest registered
         * function below" charged runtime code that follows a module's last
         * function to that function */
        DWORD64 base = 0;
        PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry((DWORD64)pc, &base, NULL);
        if (!rf) return 0;
        const uintptr_t fs = (uintptr_t)(base + rf->BeginAddress);
        size_t a = 0, b = nsv;
        while (a + 1 < b) { size_t m = (a + b) / 2; if (st[m].h <= fs) a = m; else b = m; }
        if (st[a].h != fs) return 0;
        *key = 0x80000000u | ((uint32_t)(st[a].img & 0x1FFF) << 18) | (st[a].ls & 0x3FFFFu);
        return 1;
    };
    /* host pc -> guest function, or 0 when outside the lifted code */
    auto lookup = [&](uintptr_t pc) -> uint32_t {
        if (pc < lo || pc >= hi) return 0;
        size_t a = 0, b = (size_t)nf - 1;
        while (a + 1 < b) { size_t m = (a + b) / 2; if (ft[m].h <= pc) a = m; else b = m; }
        return (pc - ft[a].h) < 0x40000u ? ft[a].g : 0;
    };
    struct Thr { DWORD tid; HANDLE h; unsigned total, inlift, via, rt, other; };
    static Thr thr[96]; int nthr = 0;
    struct Key { DWORD tid; uint32_t g; uint8_t via; unsigned n; };
    const int HN = 1 << 15; static Key hist[1 << 15];
    struct RipKey { uint32_t rva; unsigned n; };
    static RipKey riph[4096]; unsigned rip_total = 0;
    static RipKey riph2[65536]; unsigned rip2_total = 0;
    /* samples whose RIP is in another DLL, by RIP (function start when the
     * DLL has unwind data), to name what the lifted caller is waiting in */
    struct DKey { DWORD tid; uintptr_t pc; unsigned n; };
    const int DN = 1 << 12; static DKey dhist[1 << 12];
    /* 25/10: for a DLL sample, the first frame back in this exe (who called the wait) */
    static DKey dvia[1 << 12];
    /* GOWA_HOSTPROF_CHAIN=<func hex>[,<func hex>...]: quem chama essas funções
     * (cadeias do guest pelo desenrolar da pilha do host) */
    struct ChainKey { uint32_t c[5]; unsigned n; };
    static ChainKey s_chain[1024]; static unsigned s_chain_total;
    static uint32_t s_chain_t[8]; static int s_chain_n;
    if (const char* ce = getenv("GOWA_HOSTPROF_CHAIN"))
        for (const char* q = ce; *q && s_chain_n < 8; ) {
            s_chain_t[s_chain_n++] = (uint32_t)strtoul(q, (char**)&q, 16);
            if (*q == ',') q++; else break;
        }
    const DWORD me = GetCurrentThreadId(), pid = GetCurrentProcessId();
    HMODULE self = GetModuleHandleA(NULL);
    ULONGLONG t0 = GetTickCount64(), last_snap = 0;
    /* 25/10: "está no exe?" pela faixa da imagem -- o GetModuleHandleExA pega a trava do
     * carregador, e com a thread amostrada suspensa isso pode esperar para sempre */
    const uintptr_t self_lo = (uintptr_t)self;
    const uintptr_t self_hi = self_lo + ((const IMAGE_NT_HEADERS64*)((const uint8_t*)self +
                                         ((const IMAGE_DOS_HEADER*)self)->e_lfanew))->OptionalHeader.SizeOfImage;
    while (GetTickCount64() - t0 < dur_s * 1000ull) {
        if (GetTickCount64() - last_snap > 1000) {
            last_snap = GetTickCount64();
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            THREADENTRY32 te; te.dwSize = sizeof te;
            if (snap != INVALID_HANDLE_VALUE && Thread32First(snap, &te)) {
                do {
                    if (te.th32OwnerProcessID != pid || te.th32ThreadID == me) continue;
                    int f = -1; for (int i = 0; i < nthr; i++) if (thr[i].tid == te.th32ThreadID) { f = i; break; }
                    if (f >= 0 || nthr >= 96) continue;
                    HANDLE th = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                    if (!th) continue;
                    thr[nthr].tid = te.th32ThreadID; thr[nthr].h = th; thr[nthr].total = thr[nthr].inlift = thr[nthr].via = thr[nthr].rt = thr[nthr].other = 0; nthr++;
                } while (Thread32Next(snap, &te));
            }
            if (snap != INVALID_HANDLE_VALUE) CloseHandle(snap);
        }
        for (int i = 0; i < nthr; i++) {
            if (SuspendThread(thr[i].h) == (DWORD)-1) continue;
            CONTEXT ctx; ctx.ContextFlags = s_chain_n ? CONTEXT_FULL : CONTEXT_CONTROL;
            uint32_t g = 0; uint8_t via = 0; int ok = 0, inself = 0;
            if (GetThreadContext(thr[i].h, &ctx)) {
                ok = 1;
                g = lookup((uintptr_t)ctx.Rip);
                /* GOWA_HOSTPROF_CHAIN: amostra numa das funções-alvo -> desenrola
                 * a pilha do host (o código PPU traduzido tem quadros de verdade)
                 * e guarda as 4 funções do guest acima dela */
                if (g && s_chain_n) {
                    int tg = 0; for (int q = 0; q < s_chain_n; q++) if (s_chain_t[q] == g) tg = 1;
                    if (tg) {
                        uint32_t ch[5] = { g, 0, 0, 0, 0 }; int nc = 1;
                        CONTEXT uc = ctx;
                        for (int fr = 0; fr < 64 && nc < 5; fr++) {
                            DWORD64 ib = 0;
                            PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry(uc.Rip, &ib, NULL);
                            if (!rf) break;
                            void* hd = NULL; DWORD64 ef = 0;
                            RtlVirtualUnwind(UNW_FLAG_NHANDLER, ib, uc.Rip, rf, &uc, &hd, &ef, NULL);
                            if (!uc.Rip) break;
                            const uint32_t gg = lookup((uintptr_t)uc.Rip);
                            if (gg && gg != ch[nc - 1]) ch[nc++] = gg;
                        }
                        const uint32_t hh = (ch[0] * 2654435761u ^ ch[1] * 40503u ^ ch[2] * 9973u ^ ch[3]) & 1023u;
                        for (int pr = 0; pr < 64; pr++) {
                            ChainKey* k = &s_chain[(hh + pr) & 1023];
                            if (!k->n) { memcpy(k->c, ch, sizeof ch); k->n = 1; break; }
                            if (!memcmp(k->c, ch, sizeof ch)) { k->n++; break; }
                        }
                        s_chain_total++;
                    }
                }
                if (!g) { uint32_t sk; if (slookup((uintptr_t)ctx.Rip, &sk)) { g = sk; via = 5; } }
                if (!g) {
                    inself = (uintptr_t)ctx.Rip >= self_lo && (uintptr_t)ctx.Rip < self_hi;
                    MEMORY_BASIC_INFORMATION mbi; const uint64_t* sp = (const uint64_t*)ctx.Rsp;
                    if (VirtualQuery(sp, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
                        const uint64_t end = (uint64_t)mbi.BaseAddress + mbi.RegionSize;
                        for (int k = 0; k < 4096 && (uint64_t)(sp + k + 1) <= end; k++) {
                            const uint32_t gg = lookup((uintptr_t)sp[k]);
                            if (gg) { g = gg; via = inself ? 1 : 2; break; }
                        }
                    }
                    if (!g) via = inself ? 3 : 4;
                    if (!inself) {
                        DWORD64 base = 0; uintptr_t key = (uintptr_t)ctx.Rip;
                        PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry((DWORD64)ctx.Rip, &base, NULL);
                        if (rf) key = (uintptr_t)(base + rf->BeginAddress);
                        const uint32_t dh = (uint32_t)((thr[i].tid * 2654435761u) ^ (uint32_t)(key >> 4)) & (DN - 1);
                        for (int pr = 0; pr < 64; pr++) {
                            DKey* k = &dhist[(dh + pr) & (DN - 1)];
                            if (k->n == 0) { k->tid = thr[i].tid; k->pc = key; k->n = 1; break; }
                            if (k->tid == thr[i].tid && k->pc == key) { k->n++; break; }
                        }
                        /* unwind out of the DLL to the first frame in this exe. Not with
                         * GOWA_HOSTPROF_RIP=2 (25/10): that mode only wants the exe's RIPs, and
                         * two runs of it died ~15 s into the window with a fault inside ntdll
                         * (read of 0x140) -- the suspect is this unwinding of a thread stopped
                         * inside a DLL */
                        CONTEXT uc = ctx; uintptr_t cr = 0;
                        for (int d = 0; d < 24 && s_hp_rip2 <= 0; d++) {
                            DWORD64 ib = 0;
                            PRUNTIME_FUNCTION uf = RtlLookupFunctionEntry(uc.Rip, &ib, NULL);
                            if (!uf) { uc.Rip = *(DWORD64*)uc.Rsp; uc.Rsp += 8; }
                            else { void* hd = NULL; DWORD64 ef = 0; RtlVirtualUnwind(UNW_FLAG_NHANDLER, ib, uc.Rip, uf, &uc, &hd, &ef, NULL); }
                            if (!uc.Rip) break;
                            if ((uintptr_t)uc.Rip >= self_lo && (uintptr_t)uc.Rip < self_hi) {
                                DWORD64 sb = 0; PRUNTIME_FUNCTION sf = RtlLookupFunctionEntry(uc.Rip, &sb, NULL);
                                cr = sf ? (uintptr_t)sf->BeginAddress : (uintptr_t)(uc.Rip - (DWORD64)self);
                                break;
                            }
                        }
                        if (cr) {
                            const uint32_t vh = (uint32_t)((thr[i].tid * 2654435761u) ^ (uint32_t)(cr >> 4)) & (DN - 1);
                            for (int pr = 0; pr < 64; pr++) {
                                DKey* k = &dvia[(vh + pr) & (DN - 1)];
                                if (k->n == 0) { k->tid = thr[i].tid; k->pc = cr; k->n = 1; break; }
                                if (k->tid == thr[i].tid && k->pc == cr) { k->n++; break; }
                            }
                        }
                    }
                    /* runtime code with no lifted caller (SPU threads, RSX):
                     * charge it to the runtime function that contains RIP,
                     * by its RVA (resolve with build/gowa.map) */
                    if (!g && inself) {
                        DWORD64 base = 0;
                        PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry((DWORD64)ctx.Rip, &base, NULL);
                        if (rf) { g = 0x40000000u | (rf->BeginAddress & 0x3FFFFFFFu); via = 6; }
                        /* GOWA_HOSTPROF_RIP=1: also the exact RVA of RIP (all
                         * threads), to read the hot instructions of a runtime
                         * function with llvm-objdump */
                        if (s_hp_rip > 0) {
                            const uint32_t rva = (uint32_t)((uintptr_t)ctx.Rip - (uintptr_t)self);
                            const uint32_t hh = (rva * 2654435761u) >> 20;     /* 4096 slots */
                            for (int pr = 0; pr < 64; pr++) {
                                RipKey* k = &riph[(hh + pr) & 4095];
                                if (k->n == 0) { k->rva = rva; k->n = 1; break; }
                                if (k->rva == rva) { k->n++; break; }
                            }
                            rip_total++;
                        }
                    }
                }
            }
                /* GOWA_HOSTPROF_RIP=2: every RIP of the exe (lifted SPU code
                 * included), all of them written to GOWA_HOSTPROF_RIPFILE
                 * (default out/hostprof_rip.txt) -- the hot instructions of a
                 * lifted SPU function, resolved with build/gowa.map */
                { if (s_hp_rip2 > 0 && ok && (via == 5 || via == 0 || via == 6)) {
                      const uint32_t rva = (uint32_t)((uintptr_t)ctx.Rip - (uintptr_t)self);
                      const uint32_t hh = (rva * 2654435761u) >> 16;
                      for (int pr = 0; pr < 256; pr++) {
                          RipKey* k = &riph2[(hh + pr) & 65535];
                          if (k->n == 0) { k->rva = rva; k->n = 1; break; }
                          if (k->rva == rva) { k->n++; break; }
                      }
                      rip2_total++;
                  } }
            ResumeThread(thr[i].h);
            if (!ok) continue;
            thr[i].total++;
            if (via == 0) thr[i].inlift++; else if (via <= 2) thr[i].via++; else if (via == 3 || via == 5 || via == 6) thr[i].rt++; else thr[i].other++;
            uint32_t hsh = (uint32_t)((thr[i].tid * 2654435761u) ^ (g * 40503u) ^ via) & (HN - 1);
            for (int pr = 0; pr < 64; pr++) {
                Key* k = &hist[(hsh + pr) & (HN - 1)];
                if (k->n == 0) { k->tid = thr[i].tid; k->g = g; k->via = via; k->n = 1; break; }
                if (k->tid == thr[i].tid && k->g == g && k->via == via) { k->n++; break; }
            }
        }
        Sleep(2);
    }
    if (rip_total) {
        fprintf(stderr, "[hostprof-rip] %u amostras de runtime sem PPU; RVAs mais quentes:\n", rip_total);
        for (int r = 0; r < 60; r++) {
            RipKey* b = NULL;
            for (int k = 0; k < 4096; k++) if (riph[k].n && (!b || riph[k].n > b->n)) b = &riph[k];
            if (!b) break;
            fprintf(stderr, "[hostprof-rip] 0x%08X %5.2f%%\n", b->rva, 100.0 * b->n / rip_total);
            b->n = 0;
        }
    }
    if (rip2_total) {
        const char* fnm = getenv("GOWA_HOSTPROF_RIPFILE");
        FILE* f = fopen(fnm ? fnm : "out\\hostprof_rip.txt", "w");
        if (f) {
            fprintf(f, "# %u amostras (codigo traduzido SPU e PPU), rva n\n", rip2_total);
            for (int k = 0; k < 65536; k++) if (riph2[k].n) fprintf(f, "%08X %u\n", riph2[k].rva, riph2[k].n);
            fclose(f);
            fprintf(stderr, "[hostprof-rip] %u amostras de codigo traduzido gravadas\n", rip2_total);
        }
    }
    if (s_chain_total) {
        fprintf(stderr, "[hostprof-chain] %u amostras nas funções-alvo; cadeias (alvo <- chamador <- ...):\n", s_chain_total);
        for (int r = 0; r < 20; r++) {
            ChainKey* b = NULL;
            for (int k = 0; k < 1024; k++) if (s_chain[k].n && (!b || s_chain[k].n > b->n)) b = &s_chain[k];
            if (!b || b->n * 200u < s_chain_total) break;
            fprintf(stderr, "[hostprof-chain] %5.1f%%  %08X <- %08X <- %08X <- %08X <- %08X\n", 100.0 * b->n / s_chain_total,
                    b->c[0], b->c[1], b->c[2], b->c[3], b->c[4]);
            b->n = 0;
        }
    }
    static const char* vianame[] = { "", "via-rt", "via-dll", "rt", "dll", "spu", "rtfn" };
    fprintf(stderr, "[hostprof] janela %us a partir de %us\n", dur_s, start_s);
    for (int i = 0; i < nthr; i++) {
        if (thr[i].total == 0) continue;
        const char* name = "";
        for (int q = 0; q < PPU_THREAD_MAX; q++) if (g_ppu_threads[q].host_tid == thr[i].tid) { name = g_ppu_threads[q].name; break; }
        if (thr[i].inlift + thr[i].via + thr[i].rt < thr[i].total / 50) continue;   /* idle host-only threads */
        /* rt = runtime code of the exe with no lifted PPU caller: SPU threads
         * (lifted SPU code is not in the PPU function table), the RSX walker,
         * the present thread. */
        fprintf(stderr, "[hostprof] tid %lu (%s): %u amostras, %u%% PPU traduzido, %u%% runtime/dll com chamador PPU, %u%% runtime sem PPU (SPU/RSX), %u%% fora (dll)\n",
                (unsigned long)thr[i].tid, name, thr[i].total, 100u * thr[i].inlift / thr[i].total,
                100u * thr[i].via / thr[i].total, 100u * thr[i].rt / thr[i].total, 100u * thr[i].other / thr[i].total);
        unsigned nspu = 0; for (int k = 0; k < HN; k++) if (hist[k].n && hist[k].tid == thr[i].tid && (hist[k].via == 5 || hist[k].via == 6)) nspu += hist[k].n;
        if (thr[i].inlift + thr[i].via + nspu < thr[i].total / 50) continue;
        /* GOWA_HOSTPROF_TOP=N: entradas por thread (padrão 12) */
        static int s_top = -1; if (s_top < 0) { const char* e = getenv("GOWA_HOSTPROF_TOP"); s_top = e ? atoi(e) : 12; if (s_top < 1) s_top = 12; }
        for (int rank = 0; rank < s_top; rank++) {
            Key* best = NULL;
            for (int k = 0; k < HN; k++) if (hist[k].n && hist[k].tid == thr[i].tid && (!best || hist[k].n > best->n)) best = &hist[k];
            if (!best) break;
            if (best->via == 6)
                fprintf(stderr, "[hostprof]    %5.1f%%  rt RVA 0x%08X\n", 100.0 * best->n / thr[i].total, best->g & 0x3FFFFFFFu);
            else if (best->via == 5)
                fprintf(stderr, "[hostprof]    %5.1f%%  spu img %u LS 0x%05X\n", 100.0 * best->n / thr[i].total,
                        (best->g >> 18) & 0x1FFFu, best->g & 0x3FFFFu);
            else
                fprintf(stderr, "[hostprof]    %5.1f%%  %s%sfunc_%08X\n", 100.0 * best->n / thr[i].total,
                        vianame[best->via], best->via ? " " : "", best->g);
            best->n = 0;
        }
        for (int rank = 0; rank < 6; rank++) {
            DKey* best = NULL;
            for (int k = 0; k < DN; k++) if (dhist[k].n && dhist[k].tid == thr[i].tid && (!best || dhist[k].n > best->n)) best = &dhist[k];
            if (!best || best->n * 100u < thr[i].total) break;
            char sym[300]; gowa_hp_dllsym(best->pc, sym, sizeof sym);
            fprintf(stderr, "[hostprof]      dll %5.1f%%  %s\n", 100.0 * best->n / thr[i].total, sym);
            best->n = 0;
        }
        for (int rank = 0; rank < 6; rank++) {
            DKey* best = NULL;
            for (int k = 0; k < DN; k++) if (dvia[k].n && dvia[k].tid == thr[i].tid && (!best || dvia[k].n > best->n)) best = &dvia[k];
            if (!best || best->n * 100u < thr[i].total) break;
            fprintf(stderr, "[hostprof]      dll-via %5.1f%%  rt RVA 0x%08X\n", 100.0 * best->n / thr[i].total, (unsigned)best->pc);
            best->n = 0;
        }
    }
    fflush(stderr);
    return 0;
}

/* ---- GOWA: guest stack sampler (GOWA_GUESTPROF=<start_s>:<dur_s>[:<tid>]) --
 * Every ~1 ms reads guest thread <tid> (default 1, the main thread): its lr and
 * the first return addresses up the guest stack. Prints the most frequent
 * (lr, caller, caller's caller) triples and, separately, the most frequent
 * caller at each depth -- "where does the main thread wait" with the call
 * site, which the host-side sampler cannot see through an HLE wait. */
extern "C" int ppu_guest_sample(uint32_t tid, uint32_t* out, int n);
static DWORD WINAPI gowa_guestprof_thread(LPVOID)
{
    const char* e = getenv("GOWA_GUESTPROF");
    unsigned start_s = (unsigned)strtoul(e, NULL, 10), dur_s = 10, tid = 1;
    const char* c = strchr(e, ':');
    if (c) { dur_s = (unsigned)strtoul(c + 1, NULL, 10); c = strchr(c + 1, ':'); if (c) tid = (unsigned)strtoul(c + 1, NULL, 10); }
    Sleep(start_s * 1000u);
    struct K { uint32_t a[3]; unsigned n; };
    const int HN = 1 << 14; static K h[1 << 14];
    struct D { uint32_t a; unsigned n; };
    static D dep[6][512]; static int nd[6];
    unsigned total = 0;
    ULONGLONG t0 = GetTickCount64();
    while (GetTickCount64() - t0 < dur_s * 1000ull) {
        uint32_t v[8] = {0};
        const int k = ppu_guest_sample(tid, v, 8);
        if (k > 0) {
            total++;
            uint32_t hs = (v[0] * 2654435761u) ^ (v[1] * 40503u) ^ (v[2] * 69069u);
            for (int pr = 0; pr < 64; pr++) {
                K* q = &h[(hs + pr) & (HN - 1)];
                if (!q->n) { q->a[0] = v[0]; q->a[1] = v[1]; q->a[2] = v[2]; q->n = 1; break; }
                if (q->a[0] == v[0] && q->a[1] == v[1] && q->a[2] == v[2]) { q->n++; break; }
            }
            for (int d = 0; d < 6 && d < k; d++) {
                int f = -1;
                for (int j = 0; j < nd[d]; j++) if (dep[d][j].a == v[d]) { f = j; break; }
                if (f < 0 && nd[d] < 512) { f = nd[d]++; dep[d][f].a = v[d]; dep[d][f].n = 0; }
                if (f >= 0) dep[d][f].n++;
            }
        }
        Sleep(1);
    }
    fprintf(stderr, "[guestprof] guest-tid %u, %u amostras em %us (a partir de %us)\n", tid, total, dur_s, start_s);
    for (int r = 0; r < 25 && total; r++) {
        K* b = NULL;
        for (int j = 0; j < HN; j++) if (h[j].n && (!b || h[j].n > b->n)) b = &h[j];
        if (!b) break;
        fprintf(stderr, "[guestprof]   %5.1f%%  lr=%08X <- %08X <- %08X\n", 100.0 * b->n / total, b->a[0], b->a[1], b->a[2]);
        b->n = 0;
    }
    for (int d = 0; d < 6 && total; d++) {
        fprintf(stderr, "[guestprof]   profundidade %d:", d);
        for (int r = 0; r < 6; r++) {
            D* b = NULL;
            for (int j = 0; j < nd[d]; j++) if (dep[d][j].n && (!b || dep[d][j].n > b->n)) b = &dep[d][j];
            if (!b) break;
            fprintf(stderr, " %08X(%.0f%%)", b->a, 100.0 * b->n / total);
            b->n = 0;
        }
        fprintf(stderr, "\n");
    }
    fflush(stderr);
    return 0;
}

/* ---- GOWA: write guard on guest memory (GOWA_GUARD=ea[:len][@seg]) --------
 * After <seg> seconds (default 60) the guest range [ea, ea+len) (default one
 * 4 KB page) becomes read-only on the host. The first write that faults is
 * reported -- thread, RIP, the gowa.exe return addresses on its stack (RVAs:
 * resolve with port/build/gowa.map) -- and the range is made writable again
 * (one shot). Finds who scribbles over data that should never change, e.g.
 * the OPD at 0x833840 that a stray write trashed in gameplay (run392/393). */
static uintptr_t s_guard_lo, s_guard_hi;
static volatile LONG s_guard_hits;
static LONG CALLBACK gowa_guard_handler(EXCEPTION_POINTERS* ep)
{
    const EXCEPTION_RECORD* er = ep->ExceptionRecord;
    if (er->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || er->NumberParameters < 2) return EXCEPTION_CONTINUE_SEARCH;
    const uintptr_t a = (uintptr_t)er->ExceptionInformation[1];
    if (!s_guard_lo || a < s_guard_lo || a >= s_guard_hi) return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&s_guard_hits) == 1) {
        HMODULE self = GetModuleHandleA(NULL);
        const DWORD tid = GetCurrentThreadId();
        const char* name = "";
        for (int q = 0; q < PPU_THREAD_MAX; q++) if (g_ppu_threads[q].host_tid == tid) { name = g_ppu_threads[q].name; break; }
        fprintf(stderr, "[guard] ESCRITA em guest 0x%08X (host %p) tid=%lu (%s) RIP=%p RVA=0x%08llX tipo=%llu\n",
                (unsigned)(a - (uintptr_t)vm_base), (void*)a, (unsigned long)tid, name, (void*)ep->ContextRecord->Rip,
                (unsigned long long)(ep->ContextRecord->Rip - (DWORD64)(uintptr_t)self),
                (unsigned long long)er->ExceptionInformation[0]);
        const uint64_t* sp = (const uint64_t*)ep->ContextRecord->Rsp;
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(sp, &mbi, sizeof mbi)) {
            const uint64_t end = (uint64_t)mbi.BaseAddress + mbi.RegionSize;
            int n = 0;
            fprintf(stderr, "[guard] pilha (RVAs do exe):");
            for (int k = 0; k < 2048 && (uint64_t)(sp + k + 1) <= end && n < 24; k++) {
                HMODULE m = NULL;
                if (sp[k] > 0x10000 && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                                          (LPCSTR)(uintptr_t)sp[k], &m) && m == self) {
                    fprintf(stderr, " 0x%08llX", (unsigned long long)(sp[k] - (uint64_t)(uintptr_t)self)); n++;
                }
            }
            fprintf(stderr, "\n");
        }
        fflush(stderr);
    }
    DWORD old;
    VirtualProtect((void*)s_guard_lo, s_guard_hi - s_guard_lo, PAGE_READWRITE, &old);
    return EXCEPTION_CONTINUE_EXECUTION;
}
static DWORD WINAPI gowa_guard_thread(LPVOID)
{
    const char* e = getenv("GOWA_GUARD");
    char* q = NULL;
    const uint32_t ea = (uint32_t)strtoul(e, &q, 16);
    uint32_t len = 0x1000, seg = 60;
    if (q && *q == ':') len = (uint32_t)strtoul(q + 1, &q, 16);
    if (q && *q == '@') seg = (uint32_t)strtoul(q + 1, &q, 10);
    Sleep(seg * 1000u);
    if (!vm_base) return 0;
    const uintptr_t lo = ((uintptr_t)vm_base + ea) & ~(uintptr_t)0xFFF;
    const uintptr_t hi = ((uintptr_t)vm_base + ea + len + 0xFFF) & ~(uintptr_t)0xFFF;
    AddVectoredExceptionHandler(1, gowa_guard_handler);
    s_guard_lo = lo; s_guard_hi = hi;
    DWORD old;
    const BOOL ok = VirtualProtect((void*)lo, hi - lo, PAGE_READONLY, &old);
    fprintf(stderr, "[guard] guest 0x%08X..0x%08X só-leitura (%s, prot antiga 0x%lX)\n",
            (unsigned)(lo - (uintptr_t)vm_base), (unsigned)(hi - (uintptr_t)vm_base), ok ? "ok" : "FALHOU", (unsigned long)old);
    return 0;
}

/* ---- GOWA: write watch on the executable image (GOWA_IMGGUARD=<s>) --------
 * <s> seconds after boot, the ELF's text (0x10000..0x815000) and OPD table
 * pages (0x828000..0x838000) become read-only on the host. A user-mode write
 * there faults: the writer (RIP RVA, guest address, gowa.exe return addresses
 * on its stack) is reported once per distinct RIP, the page is opened, the
 * instruction single-steps, the page is restored from the copy taken when the
 * guard was armed, and closed again -- the image stays exactly as loaded (no
 * write to it is legitimate once the game runs; none showed up in clean runs)
 * and every writer gets named. A kernel write (ReadFile into guest memory)
 * just fails. Stray copies from God of War Ascension's VRAM manager running
 * out of room (NULL + offset) landed DXT blocks on the OPD at 0x833840. */
static uintptr_t s_ig_ranges[2][2];
static uint8_t* s_ig_pristine[2];          /* copy of each range taken when armed */
static thread_local uintptr_t t_ig_page;
/* the write just single-stepped is undone: the image stays what was loaded */
static void gowa_imgguard_restore(uintptr_t page)
{
    for (int r = 0; r < 2; r++)
        if (s_ig_pristine[r] && page >= s_ig_ranges[r][0] && page < s_ig_ranges[r][1])
            memcpy((void*)page, s_ig_pristine[r] + (page - s_ig_ranges[r][0]), 0x1000);
}
static LONG CALLBACK gowa_imgguard_handler(EXCEPTION_POINTERS* ep)
{
    EXCEPTION_RECORD* er = ep->ExceptionRecord;
    if (er->ExceptionCode == EXCEPTION_SINGLE_STEP && t_ig_page) {
        gowa_imgguard_restore(t_ig_page);
        DWORD old; VirtualProtect((void*)t_ig_page, 0x1000, PAGE_READONLY, &old);
        t_ig_page = 0;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (er->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || er->NumberParameters < 2 || er->ExceptionInformation[0] != 1)
        return EXCEPTION_CONTINUE_SEARCH;
    const uintptr_t a = (uintptr_t)er->ExceptionInformation[1];
    int in = 0;
    for (int r = 0; r < 2; r++) if (a >= s_ig_ranges[r][0] && a < s_ig_ranges[r][1]) in = 1;
    if (!in) return EXCEPTION_CONTINUE_SEARCH;
    HMODULE self = GetModuleHandleA(NULL);
    const uint64_t rva = (uint64_t)ep->ContextRecord->Rip - (uint64_t)(uintptr_t)self;
    static uint64_t seen[64]; static volatile LONG nseen;
    int known = 0; for (LONG i = 0; i < nseen && i < 64; i++) if (seen[i] == rva) { known = 1; break; }
    if (!known) {
        const LONG k = InterlockedIncrement(&nseen) - 1;
        if (k < 64) seen[k] = rva;
        if (k < 32) {
            const DWORD tid = GetCurrentThreadId();
            const char* name = "";
            for (int q = 0; q < PPU_THREAD_MAX; q++) if (g_ppu_threads[q].host_tid == tid) { name = g_ppu_threads[q].name; break; }
            fprintf(stderr, "[imgguard] escrita em guest 0x%08X por RVA 0x%08llX tid=%lu (%s); pilha:",
                    (unsigned)(a - (uintptr_t)vm_base), (unsigned long long)rva, (unsigned long)tid, name);
            const uint64_t* sp = (const uint64_t*)ep->ContextRecord->Rsp;
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery(sp, &mbi, sizeof mbi)) {
                const uint64_t end = (uint64_t)mbi.BaseAddress + mbi.RegionSize;
                int n = 0;
                for (int j = 0; j < 2048 && (uint64_t)(sp + j + 1) <= end && n < 16; j++) {
                    HMODULE m = NULL;
                    if (sp[j] > 0x10000 && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                                              (LPCSTR)(uintptr_t)sp[j], &m) && m == self) {
                        fprintf(stderr, " 0x%08llX", (unsigned long long)(sp[j] - (uint64_t)(uintptr_t)self)); n++;
                    }
                }
            }
            fprintf(stderr, "\n"); fflush(stderr);
        }
    }
    const uintptr_t page = a & ~(uintptr_t)0xFFF;
    DWORD old; VirtualProtect((void*)page, 0x1000, PAGE_READWRITE, &old);
    t_ig_page = page;
    ep->ContextRecord->EFlags |= 0x100;          /* trap after this one instruction */
    return EXCEPTION_CONTINUE_EXECUTION;
}
static DWORD WINAPI gowa_imgguard_thread(LPVOID)
{
    const unsigned seg = (unsigned)strtoul(getenv("GOWA_IMGGUARD"), NULL, 10);
    Sleep((seg ? seg : 30) * 1000u);
    if (!vm_base) return 0;
    const uint32_t r[2][2] = { { 0x00010000u, 0x00815000u }, { 0x00828000u, 0x00838000u } };
    AddVectoredExceptionHandler(1, gowa_imgguard_handler);
    for (int i = 0; i < 2; i++) {
        s_ig_ranges[i][0] = (uintptr_t)vm_base + r[i][0];
        s_ig_ranges[i][1] = (uintptr_t)vm_base + r[i][1];
        s_ig_pristine[i] = (uint8_t*)malloc(r[i][1] - r[i][0]);
        if (s_ig_pristine[i]) memcpy(s_ig_pristine[i], (void*)s_ig_ranges[i][0], r[i][1] - r[i][0]);
        DWORD old;
        const BOOL ok = VirtualProtect((void*)s_ig_ranges[i][0], r[i][1] - r[i][0], PAGE_READONLY, &old);
        fprintf(stderr, "[imgguard] guest 0x%08X..0x%08X só-leitura (%s)\n", r[i][0], r[i][1], ok ? "ok" : "FALHOU");
    }
    return 0;
}

/* ---- GOWA: who writes this guest word? (GOWA_WGUARD=ea[:len]) -------------
 * The page holding [ea, ea+len) is read-only on the host from the moment guest
 * memory exists. Every write that lands inside the range is reported (first
 * 32: RVA + the gowa.exe return addresses on the stack -- the lifted callers,
 * resolve with port/build/gowa.map); writes elsewhere on the page just pass
 * (single step, page closed again). */
static uintptr_t s_wg_page, s_wg_lo, s_wg_hi;
static thread_local int t_wg_step;
static LONG CALLBACK gowa_wguard_handler(EXCEPTION_POINTERS* ep)
{
    EXCEPTION_RECORD* er = ep->ExceptionRecord;
    if (er->ExceptionCode == EXCEPTION_SINGLE_STEP && t_wg_step) {
        t_wg_step = 0;
        DWORD old; VirtualProtect((void*)s_wg_page, 0x1000, PAGE_READONLY, &old);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (er->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || er->NumberParameters < 2 || er->ExceptionInformation[0] != 1)
        return EXCEPTION_CONTINUE_SEARCH;
    const uintptr_t a = (uintptr_t)er->ExceptionInformation[1];
    if ((a & ~(uintptr_t)0xFFF) != s_wg_page) return EXCEPTION_CONTINUE_SEARCH;
    if (a >= s_wg_lo && a < s_wg_hi) {
        /* one report per distinct writer (RIP): a hot DMA copy would otherwise
         * use up the budget before the rarer writers show */
        static volatile uint64_t s_seen[64]; static volatile LONG n;
        const uint64_t rip = (uint64_t)ep->ContextRecord->Rip;
        int fresh = 1;
        for (int k = 0; k < 64 && s_seen[k]; k++) if (s_seen[k] == rip) { fresh = 0; break; }
        if (fresh) { const LONG slot = InterlockedIncrement(&n) - 1; if (slot < 64) s_seen[slot] = rip; else fresh = 0; }
        if (fresh) {
            HMODULE self = GetModuleHandleA(NULL);
            fprintf(stderr, "[wguard] escrita em guest 0x%08X RVA 0x%08llX tid=%lu pilha:", (unsigned)(a - (uintptr_t)vm_base),
                    (unsigned long long)((uint64_t)ep->ContextRecord->Rip - (uint64_t)(uintptr_t)self), (unsigned long)GetCurrentThreadId());
            const uint64_t* sp = (const uint64_t*)ep->ContextRecord->Rsp;
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery(sp, &mbi, sizeof mbi)) {
                const uint64_t end = (uint64_t)mbi.BaseAddress + mbi.RegionSize; int k2 = 0;
                for (int j = 0; j < 4096 && (uint64_t)(sp + j + 1) <= end && k2 < 12; j++) {
                    HMODULE m = NULL;
                    if (sp[j] > 0x10000 && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                                              (LPCSTR)(uintptr_t)sp[j], &m) && m == self) {
                        fprintf(stderr, " 0x%08llX", (unsigned long long)(sp[j] - (uint64_t)(uintptr_t)self)); k2++;
                    }
                }
            }
            fprintf(stderr, "\n"); fflush(stderr);
        }
    }
    DWORD old; VirtualProtect((void*)s_wg_page, 0x1000, PAGE_READWRITE, &old);
    t_wg_step = 1;
    ep->ContextRecord->EFlags |= 0x100;
    return EXCEPTION_CONTINUE_EXECUTION;
}
static DWORD WINAPI gowa_wguard_thread(LPVOID)
{
    const char* e = getenv("GOWA_WGUARD"); char* q = NULL;
    const uint32_t ea = (uint32_t)strtoul(e, &q, 16);
    const uint32_t len = (q && *q == ':') ? (uint32_t)strtoul(q + 1, NULL, 16) : 4;
    while (!vm_base) Sleep(1);
    s_wg_lo = (uintptr_t)vm_base + ea; s_wg_hi = s_wg_lo + len; s_wg_page = s_wg_lo & ~(uintptr_t)0xFFF;
    AddVectoredExceptionHandler(1, gowa_wguard_handler);
    DWORD old; BOOL ok; int tries = 0;
    /* heap pages are committed on first allocation -- keep trying */
    while (!(ok = VirtualProtect((void*)s_wg_page, 0x1000, PAGE_READONLY, &old)) && tries++ < 6000) Sleep(100);
    fprintf(stderr, "[wguard] vigiando guest 0x%08X..0x%08X (%s, %d tentativas)\n", ea, ea + len, ok ? "ok" : "FALHOU", tries);
    return 0;
}

extern "C" const char* cellGame_get_title(void);   /* PARAM.SFO TITLE, for the caption */

/* GOWA: os handlers de vblank/flip do jogo rodam numa thread própria. No
 * ticker original eles eram chamados na MESMA thread que drena o FIFO do RSX;
 * o handler de vblank do God of War Ascension (func_0039D9D8 -> func_00413F38)
 * espera progresso da GPU, e como a GPU (o dreno) estava parada esperando o
 * handler voltar, o relógio de frames congelava ~25 s depois do boot.
 * GOWA_VBLANK_THREAD=0 volta ao comportamento original. */
static int s_vblank_thread = 0;
static DWORD WINAPI gowa_vblank_thread(LPVOID)
{
    /* GOWA_VBLANK_HZ=<n>: ritmo do vblank/flip emulado (padrão 60). O jogo
     * espera 2 vblanks por frame (30 FPS); um ritmo maior destrava o limite.
     * Agendado por QPC (o Sleep(1) com resolução de timer padrão dá ~15 ms). */
    { const char* e = getenv("GOWA_VBLANK_HZ");
      const double hz = e ? atof(e) : 0.0;
      if (hz > 0.0 && hz != 60.0) {
          timeBeginPeriod(1);
          LARGE_INTEGER f, t; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t);
          const double per = (double)f.QuadPart / hz;
          double nextq = (double)t.QuadPart;
          fprintf(stderr, "[gowa] vblank emulado a %.0f Hz\n", hz);
          for (;;) {
              QueryPerformanceCounter(&t);
              const double rem = nextq - (double)t.QuadPart;
              if (rem > 0) { if (rem > (double)f.QuadPart / 500.0) Sleep(1); else SwitchToThread(); continue; }
              cellGcmTickVBlank();
              cellGcmTickFlip();
              nextq += per;
              if ((double)t.QuadPart - nextq > (double)f.QuadPart) nextq = (double)t.QuadPart;   /* muito atrasado */
          }
      } }
    ULONGLONG next = GetTickCount64();
    for (;;) {
        ULONGLONG now = GetTickCount64();
        if ((long long)(now - next) < 0) { Sleep(1); continue; }
        cellGcmTickVBlank();
        cellGcmTickFlip();
        next += 16;
        if ((long long)(now - next) > 1000) next = now;   /* muito atrasado: ressincroniza */
    }
    return 0;
}

/* GOWA_PRIO_AB=<s>[:<modo>] (26/10): A/B de prioridade de threads DENTRO de uma rodada. A cada
 * <s> segundos (padrão 25) alterna entre a fase 0 (todas normais) e a fase 1 (<modo>, padrão
 * "mr"): m = principal do jogo ABOVE_NORMAL, r = RSX-present ABOVE_NORMAL, e = SPU0-3 (EDGE)
 * ABOVE_NORMAL, s = todas as SPUs BELOW_NORMAL, u = usleep curto (< 200 us) dorme num timer em vez
 * de girar (GOWA_USLEEP_SPIN_MAX=0), y = esperas ativas da principal dormem 1 ms a cada 256 voltas
 * (GOWA_PPU_PAUSE=3) em vez de SwitchToThread (Y: em vez do giro puro, o padrão com 12+ CPUs). Marca "[prio-ab] fase=N" no log; as amostras
 * [fps] de cada fase saem pareadas no tempo (deriva térmica e carga de fundo dos dois lados).
 * No notebook (4 núcleos/8 lógicas) a variação ENTRE rodadas (±5%) esconde ganhos pequenos. */
static DWORD g_gowa_main_tid, g_gowa_rsx_tid;
extern "C" long g_gowa_usleep_spin_max;   /* sys_timer.c */
extern "C" int g_gowa_ppu_pause;          /* gowa_offline.cpp */
static void gowa_prio_aplica(int fase, const char* modo)
{
    if (strchr(modo, 'u')) g_gowa_usleep_spin_max = fase ? 0 : 200;
    if (strchr(modo, 'y')) g_gowa_ppu_pause = fase ? 3 : 2;
    if (strchr(modo, 'Y')) g_gowa_ppu_pause = fase ? 3 : 1;   /* 12+ CPUs: o padrão é o giro puro (1) */
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    const DWORD pid = GetCurrentProcessId();
    THREADENTRY32 te; te.dwSize = sizeof te;
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid) continue;
        HANDLE h = OpenThread(THREAD_SET_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID);
        if (!h) continue;
        WCHAR nm[16] = L"";
        { PWSTR w = NULL; if (SUCCEEDED(GetThreadDescription(h, &w)) && w) { wcsncpy(nm, w, 15); LocalFree(w); } }
        const int spu = nm[0] == L'S' && nm[1] == L'P' && nm[2] == L'U' && nm[3] >= L'0' && nm[3] <= L'9' && !nm[4];
        const int papel = te.th32ThreadID == g_gowa_main_tid ? 'm' : te.th32ThreadID == g_gowa_rsx_tid ? 'r' : spu ? 's' : 0;
        if (papel) {
            int pr = THREAD_PRIORITY_NORMAL;
            if (fase) {
                if (papel == 'm' && strchr(modo, 'm')) pr = THREAD_PRIORITY_ABOVE_NORMAL;
                if (papel == 'r' && strchr(modo, 'r')) pr = THREAD_PRIORITY_ABOVE_NORMAL;
                if (papel == 's' && strchr(modo, 's')) pr = THREAD_PRIORITY_BELOW_NORMAL;
                if (papel == 's' && strchr(modo, 'e') && nm[3] <= L'3') pr = THREAD_PRIORITY_ABOVE_NORMAL;
            }
            SetThreadPriority(h, pr);
        }
        CloseHandle(h);
    }
    CloseHandle(snap);
}
static DWORD WINAPI gowa_prio_ab_thread(LPVOID)
{
    const char* e = getenv("GOWA_PRIO_AB");
    const int seg = atoi(e) > 0 ? atoi(e) : 25;
    const char* c = strchr(e, ':');
    char modo[16] = "mr";
    if (c && c[1]) { strncpy(modo, c + 1, sizeof modo - 1); modo[sizeof modo - 1] = 0; }
    Sleep(20000);
    for (int fase = 0; ; fase ^= 1) {
        gowa_prio_aplica(fase, modo);
        fprintf(stderr, "[prio-ab] fase=%d modo=%s t=%llu ms\n", fase, modo, (unsigned long long)GetTickCount64());
        Sleep((DWORD)seg * 1000u);
    }
    return 0;
}

static DWORD WINAPI vblank_ticker(LPVOID)
{
    SetThreadDescription(GetCurrentThread(), L"RSX-present");   /* GOWA: named for [pin]/[hostprof] */
    g_gowa_rsx_tid = GetCurrentThreadId();
    /* GOWA_RSX_PRIO=<n> (21/10, experiment): SetThreadPriority for this thread (1 = ABOVE_NORMAL,
     * 2 = HIGHEST) -- the RSX thread is ~98% busy and the frame waits on it in the slow runs. */
    { const char* e = getenv("GOWA_RSX_PRIO"); if (e && atoi(e)) SetThreadPriority(GetCurrentThread(), atoi(e)); }
    /* Window caption: $PS3_TITLE wins, else the TITLE field PARAM.SFO already
     * gave cellGame, else a neutral name. It used to fall back to a specific
     * other title, so every port announced itself as that game. */
    const char* _title = getenv("PS3_TITLE");
    if (!_title || !*_title) {
        const char* sfo = cellGame_get_title();
        if (sfo && *sfo && strcmp(sfo, "Unknown Title") != 0) _title = sfo;
    }
    if (!_title || !*_title) _title = "ps3recomp";
    /* The live engine's RT-as-backbuffer rescue only treats a surface as the
     * backbuffer when its clip EQUALS the backend size, so a title that renders
     * into something other than 1280x720 must say so: RSX_W / RSX_H. */
    uint32_t rsx_w = 1280, rsx_h = 720;
    if (const char* e = getenv("RSX_W")) rsx_w = (uint32_t)strtoul(e, 0, 0);
    if (const char* e = getenv("RSX_H")) rsx_h = (uint32_t)strtoul(e, 0, 0);
    int rsx_ok;
    if (rsx_live_draw_enabled()) {
        rsx_ok = (rsx_null_backend_init(rsx_w, rsx_h, _title) == 0);
        if (rsx_ok && rsx_live_draw_init(rsx_null_backend_get_hwnd(), rsx_w, rsx_h,
                                         rsx_live_guest_ptr, nullptr) == 0) {
            s_rsx_live = 1;
            gowa_icone_janela((HWND)rsx_null_backend_get_hwnd());   /* 25/10: ícone na barra de tarefas */
            rsx_null_backend_suppress_present(1);
            fprintf(stderr, "[rsx] live-draw engine up (D3D12); GDI present suppressed\n");
        } else {
            fprintf(stderr, "[rsx] live-draw init FAILED -- falling back to the D3D12 backend\n");
            rsx_ok = (rsx_backend_init(rsx_w, rsx_h, _title) == 0);
        }
    } else {
        rsx_ok = (rsx_backend_init(rsx_w, rsx_h, _title) == 0);
    }
    fprintf(stderr, "[rsx] backend init %s\n", rsx_ok ? "OK -- window open" : "FAILED");
    unsigned last_flip = 0;
    /* The game's frame pacing (vblank/flip handlers -> display frame counter) must
     * advance at ~60Hz regardless of how long present() blocks. On a hidden/occluded
     * window DXGI Present throttles hard, which previously stalled these ticks and
     * paced the game's main loop to ~0.5fps. Drive the ticks off REAL elapsed time
     * and catch up in a bounded burst so present latency never slows the game. */
    ULONGLONG next_tick = GetTickCount64();
    for (;;) {
        /* Was Sleep(4). The FIFO kick event existed, was signalled on a dry
         * fence queue, and had no waiter -- so a guest starved for `get` or a
         * fence still paid the whole tick. Waiting on it keeps the same 4 ms
         * ceiling when nothing kicks, and drains on demand when something
         * does. */
        /* GOWA: report pacing pending (cellGcmSys.c) -> come back in ~1 ms to
         * release the next GET_REPORT, instead of the 4 ms idle cadence. */
        /* GOWA_FRAMEGEN: a real frame waiting for its half-interval slot
         * (rsx_live_draw.c) -- sleep exactly until then, then present it */
        /* GOWA_FLIP_HZ: a held flip -- sleep exactly until it is due */
        { const int fh = cellGcm_flip_hold_us();
          if (fh > 0) {
              static HANDLE s_fht = CreateWaitableTimerExW(NULL, NULL, 0x00000002 /* HIGH_RESOLUTION */, TIMER_ALL_ACCESS);
              if (s_fht) {
                  LARGE_INTEGER due; due.QuadPart = -(LONGLONG)(fh < 4000 ? fh : 4000) * 10;
                  if (SetWaitableTimer(s_fht, &due, 0, NULL, NULL, FALSE))
                      WaitForSingleObject(s_fht, 5);
              }
          } }
        { const int fgw = rsx_live_draw_framegen_wait_us();
          if (cellGcm_flip_hold_us() >= 0) {
              /* a held flip was just waited for above: present it below */
          } else if (fgw >= 0 && fgw < 4000) {
              static HANDLE s_fgt = CreateWaitableTimerExW(NULL, NULL, 0x00000002 /* HIGH_RESOLUTION */, TIMER_ALL_ACCESS);
              if (fgw > 0 && s_fgt) {
                  /* at most GOWA_FG_SLICE_US (default 200) per sleep: this
                   * loop also walks the FIFO, and the game never writes put
                   * (no kick) -- a whole half-interval asleep held its
                   * SET_REFERENCE waiters back (59 -> 44 FPS of real frames) */
                  static int s_slice = -1;
                  if (s_slice < 0) { const char* e = getenv("GOWA_FG_SLICE_US"); s_slice = e ? atoi(e) : 200; }
                  const int w = (s_slice > 0 && fgw > s_slice) ? s_slice : fgw;
                  LARGE_INTEGER due; due.QuadPart = -(LONGLONG)w * 10;
                  if (SetWaitableTimer(s_fgt, &due, 0, NULL, NULL, FALSE)) {
                      /* a kick (new commands for the walker) still wakes us */
                      HANDLE hs[2] = { s_fgt, (HANDLE)cellGcm_fifo_kick_event() };
                      WaitForMultipleObjects(hs[1] ? 2 : 1, hs, FALSE, 5);
                  }
              }
          } else
              cellGcm_fifo_kick_wait(cellGcm_report_pace_pending() ? 1 : 4);
          rsx_live_draw_framegen_tick(); }
        ULONGLONG now = GetTickCount64();
        /* GOWA_MLAA_SKIP=<rows> (1 = 80; with GOWA_MLAA_BYPASS, whose output
         * nobody reads): the SPU MLAA tasks get an image of a few rows -- the
         * u16 at 0xA1794E that func_00435D10 writes once (width 0x500 at +0x1C,
         * height 0x2D0 at +0x1E, pitch 0x1400 at +0x20 of the argument block
         * 0xA17930) -- so each round keeps its protocol (mailbox, flags,
         * dependency) but processes almost nothing. Height 0 hangs the loading
         * screen. The block is printed once before the change. */
        { static int s_ms = -1; static unsigned s_rows = 80;
          if (s_ms < 0) { const char* e = getenv("GOWA_MLAA_SKIP"); s_ms = e && *e && *e != '0';
                          if (s_ms && atoi(e) > 1) s_rows = (unsigned)atoi(e); }
          if (s_ms && vm_base) {
              uint8_t* a = vm_base + 0xA17930u;
              const unsigned w = (a[0x1C] << 8) | a[0x1D], h = (a[0x1E] << 8) | a[0x1F], p = (a[0x20] << 8) | a[0x21];
              if (w == 0x500u && h && h != s_rows && p == 0x1400u) {
                  static int said;
                  if (!said) { said = 1; char b[200]; int q = 0;
                      for (int i = 0; i < 0x30; i++) q += snprintf(b + q, sizeof b - q, "%02X%s", a[i], (i & 3) == 3 ? " " : "");
                      fprintf(stderr, "[mlaa-skip] bloco 0xA17930: %s-> altura %u\n", b, s_rows); }
                  a[0x1E] = (uint8_t)(s_rows >> 8); a[0x1F] = (uint8_t)s_rows;
              }
          } }
        int fired = 0;
        while ((long long)(now - next_tick) >= 0 && fired < 240) {
            if (!s_vblank_thread) {
                cellGcmTickVBlank();
                cellGcmTickFlip();
            }
            /* Present a pending flip BEFORE draining further: the flip fires
             * at a get==put frame boundary (guest thread), so the batch held
             * right now is exactly the completed frame. Presenting on a raw
             * flip-count change after the drain raced the guest's next-frame
             * writes and showed empty or mixed batches. */
            {
                if (rsx_ok && cellGcm_take_flip_pending()) {
                    present_src_note(0);
                    present_guest_frame();
                    last_flip = cellGcm_flip_request_count();
                }
            }
            /* Drain the game's GCM FIFO every tick -- this writes the RSX sync-fence
             * labels (e.g. @0x03000410) that the game's per-frame logic blocks on.
             * Doing it here (not after present) keeps those fences advancing at 60Hz
             * even when present() throttles on a hidden/occluded window. */
            if (rsx_ok) cellGcm_rsx_process_fifo();
            next_tick += 16;                 /* ~60 Hz */
            fired++;
        }
        if (fired >= 240) next_tick = now;   /* fell too far behind -> resync */
        /* Drain at the outer ~4ms cadence too (not just the 16ms tick):
         * titles fence EVERY render pass on an RSX label the drain writes;
         * at 16ms per fence wave's six passes paced the guest to ~7 fps.
         * The real RSX writes those fences in microseconds. */
        if (rsx_ok) {
            if (cellGcm_take_flip_pending()) {
                present_src_note(1);
                present_guest_frame();
                last_flip = cellGcm_flip_request_count();
            }
            cellGcm_rsx_process_fifo();
        }
        /* YDKJ_INJECT_Q3: the main thread polls q3 (load/state-complete) each frame but
         * it's never posted (producer signals a condvar, never enqueues). Inject a q3
         * event periodically to test whether delivering the completion advances the
         * game's state machine to instantiate + display the (force-parsed) menu movie. */
        if (getenv("YDKJ_INJECT_Q3")) {
            static int s_q3=0; const char* qe=getenv("YDKJ_INJECT_Q3"); uint32_t qid=(uint32_t)strtoul(qe,0,0); if(qid==0||qid==1) qid=3;
            if(++s_q3 % 8 == 0){ int r=sys_event_queue_inject(qid, 0x1234, 0, 0, 0);
              static int _n=0; if(_n++<12) fprintf(stderr,"[INJQ3] injected q%u event rc=%d\n",qid,r); } }
        if (rsx_ok) {
            if (rsx_pump_messages() != 0) {
                rsx_ok = 0;
                fprintf(stderr, "[janela] a janela fechou; o jogo vai encerrar\n");
                /* GOWA (25/10): the guest threads (PPU, SPUs, audio) never stop by themselves, and this
                 * loop has no way out: with the window gone the process kept running with no window,
                 * eating CPU and playing sound, and stayed behind after the launcher closed (a player's
                 * "it opens and closes" and "the PC is slow afterwards"). End it here; the saves are
                 * whole files written as they go and the launcher's performance file is rewritten every
                 * ~30 s, so nothing waits for an orderly exit. */
                fflush(stdout);
                fflush(stderr);
#ifdef _WIN32
                TerminateProcess(GetCurrentProcess(), 0);
#else
                _exit(0);
#endif
            }
            if (getenv("YDKJ_PACETRACE")) {
                static ULONGLONG s_win=0; static int s_pf=0, s_pres=0; static ULONGLONG s_presms=0;
                s_pf += fired; s_pres++;
                ULONGLONG t0=GetTickCount64(); rsx_present_frame(); ULONGLONG t1=GetTickCount64();
                s_presms += (t1-t0);
                if (s_win==0) s_win=now;
                if (now - s_win >= 1000) {
                    fprintf(stderr,"[PACE] process_fifo=%d/s  present=%d/s  present_total=%llums/s (avg %llums)\n",
                            s_pf, s_pres, s_presms, s_pres? s_presms/s_pres:0);
                    s_pf=0; s_pres=0; s_presms=0; s_win=now;
                }
            } else {
                /* Present only on a guest flip (frame boundary). A fixed-clock
                 * present can catch the drain mid-frame -- notably while the
                 * guest is blocked in the FIFO-wrap recycle callback -- and
                 * flash a partial frame (clear + a few draws). Before the
                 * first flip present freely so the window isn't stuck white
                 * during boot. */
                /* GOWA: depois do primeiro flip, só o flip pendente apresenta
                 * (cellGcm_take_flip_pending, acima). Apresentar também quando
                 * o contador muda mostrava cada quadro duas vezes -- uma antes
                 * do pendente ser consumido, sem respeitar o GOWA_FLIP_HZ, e
                 * outra pelo pendente --, ~1,9 apresentação por flip do God of
                 * War Ascension (o [fps] contava as duas). GOWA_PRESENT_ON_COUNT=1
                 * volta ao antigo. */
                static int s_poc = -1;
                if (s_poc < 0) { const char* e = getenv("GOWA_PRESENT_ON_COUNT"); s_poc = (e && *e == '1') ? 1 : 0; }
                unsigned fc = cellGcm_flip_request_count();
                if ((s_poc && fc != last_flip) || fc == 0) {
                    if (fc) { present_src_note(2); present_guest_frame(); } else rsx_present_frame();
                    last_flip = fc;
                } else if (fc == 0) {
                    rsx_backend_present();
                }
            }
        }
    }
    return 0;
}

extern "C" uint32_t    g_last_hle_nid;
extern "C" const char* g_last_hle_name;

/* Windows-only from here to the end of hang_watchdog. Snapshotting every
 * OTHER thread and reading its instruction pointer needs tlhelp32 plus
 * SuspendThread/GetThreadContext; POSIX has no equivalent that does not
 * amount to writing a debugger. The vblank ticker above, which is what the
 * frame loop actually depends on, is deliberately outside this. */
#ifdef _WIN32
#include <tlhelp32.h>
/* When the boot wedges, snapshot every other thread's instruction pointer as a
 * module RVA (symbolize with llvm-symbolizer) so a guest spin/wait is pinned to
 * an exact lifted function -- the HLE breadcrumb only covers HLE calls. */
/* Snapshot every other thread's RIP. For threads in the boot module (lifted
 * guest code) print the RVA (symbolizable) + a couple of stack-return RVAs;
 * for threads parked in a DLL (OS waits / FMOD) print the module name so they
 * are not mistaken for guest spins. Called twice so the caller can diff which
 * guest thread is genuinely parked (same RIP) vs. still progressing. */
/* Defined with the debug console below; writes to stderr and, when the
 * console is servicing a command, also to its response file. */
static void dbg_printf(const char* fmt, ...);
/* While dump_threads holds a thread suspended, dbg_printf formats into this
 * buffer instead of a stream: the suspended thread may own stderr's CRT lock,
 * and writing then deadlocks the watchdog -- and with it every thread that
 * logs. GH3 froze solid (0% CPU, log silent mid-boot) exactly that way. */
static __declspec(thread) char   s_dbg_capbuf[16384];
static __declspec(thread) char*  s_dbg_cap = NULL;
static __declspec(thread) size_t s_dbg_capn = 0;
/* GOWA_FREEZE_DUMP (21/10): while set, dbg_printf writes here with WriteFile and never
 * touches a CRT stream -- in the boot freeze of 21/10 not even the watchdog printed:
 * a stuck thread may own stderr's lock. */
static HANDLE s_dbg_raw = NULL;

extern "C" const char* g_hle_inflight[];   /* ppu_hle.cpp; 64 entries */
extern "C" void ppu_report_guest_lrs(void); /* ppu_loader.cpp */

static void dump_threads(const char* label, HMODULE self)
{
    dbg_printf( "[WATCHDOG] %s; last HLE call = 0x%08X (%s)\n",
            label, g_last_hle_nid, g_last_hle_name ? g_last_hle_name : "");

    /* Which HLE each guest thread is INSIDE right now.
     *
     * ppu_hle.cpp has maintained this array for a while and nothing ever read
     * it. It is the only way to see a thread parked in an HLE wait:
     * sys_lwmutex_lock and sys_lwcond_wait are sysPrxForUser handlers, not lv2
     * syscalls, so PS3_SCBLOCK_PROF cannot see them block, the usleep
     * histogram cannot see them (they do not usleep), and the sampler just
     * reports ntdll. A thread stuck and invisible to all three is exactly the
     * case this answers. */
    { int any = 0;
      for (unsigned t = 0; t < 64u; t++) {
          const char* n = g_hle_inflight[t];
          if (!n) continue;
          if (!any) { dbg_printf("[WATCHDOG] guest threads inside an HLE:\n"); any = 1; }
          dbg_printf("[WATCHDOG]   guest-tid %-3u in %s\n", t, n);
      }
      if (!any) dbg_printf("[WATCHDOG] no guest thread is inside an HLE\n"); }
    /* 30/10: a trava rara da tela de carregamento (notebook, cap. 16): a thread principal fica no laço de
     * sys_timer_usleep(30) da func_0041587C (0x415914) esperando o u16 em 0xA03240 + 4*i zerar. Os 8 contadores,
     * os 8 bytes de estado em 0xA03C00 e as 8 palavras em 0xA03BE0, para saber qual trabalho não terminou. */
    if (vm_base) {
        const uint8_t* g = vm_base;
        auto be32 = [g](uint32_t ea) { return ((uint32_t)g[ea] << 24) | ((uint32_t)g[ea + 1] << 16) | ((uint32_t)g[ea + 2] << 8) | g[ea + 3]; };
        dbg_printf("[WATCHDOG] espera 0x415914: contadores");
        for (uint32_t i = 0; i < 8; i++) dbg_printf(" %08X", be32(0xA03240u + 4u * i));
        dbg_printf(" | estado");
        for (uint32_t i = 0; i < 8; i++) dbg_printf(" %02X", g[0xA03C00u + i]);
        dbg_printf(" | 3BE0");
        for (uint32_t i = 0; i < 8; i++) dbg_printf(" %08X", be32(0xA03BE0u + 4u * i));
        dbg_printf("\n");
    }
    dbg_printf("[WATCHDOG] guest threads, by guest lr:\n");
    if (!s_dbg_raw) ppu_report_guest_lrs();   /* fprintf(stderr) inside: in raw mode it runs last */
    DWORD me = GetCurrentThreadId(), pid = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te; te.dwSize = sizeof te;
    if (snap != INVALID_HANDLE_VALUE && Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID != pid || te.th32ThreadID == me) continue;
            HANDLE th = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME,
                                   FALSE, te.th32ThreadID);
            if (!th) continue;
            s_dbg_capn = 0; s_dbg_capbuf[0] = 0; s_dbg_cap = s_dbg_capbuf;
            SuspendThread(th);
            CONTEXT ctx; ctx.ContextFlags = CONTEXT_CONTROL;
            if (GetThreadContext(th, &ctx)) {
                HMODULE m = NULL;
                GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCSTR)ctx.Rip, &m);
                if (m == self) {
                    dbg_printf( "[WATCHDOG]   tid %5lu BOOT rip rva=0x%llX\n",
                            (unsigned long)te.th32ThreadID,
                            (unsigned long long)((char*)ctx.Rip - (char*)self));
                } else {
                    char path[MAX_PATH] = "?";
                    if (m) GetModuleFileNameA(m, path, sizeof path);
                    const char* base = strrchr(path, '\\');
                    dbg_printf( "[WATCHDOG]   tid %5lu in %s\n",
                            (unsigned long)te.th32ThreadID, base ? base + 1 : path);
                }
                /* Scan the suspended thread's stack for boot-module return
                 * addresses to reconstruct the lifted call chain — done for ALL
                 * threads (even when RIP is parked in ntdll inside a CriticalSection
                 * call), since that's exactly where the busy-spin's lwmutex churn
                 * lands the main thread. Map RVAs -> func_ names via flow.map.
                 * (some false positives expected — these are stack-scan hits.) */
                {
                    uint64_t* sp = (uint64_t*)ctx.Rsp;
                    /* Bound the scan to the committed stack region so we never read
                     * past the guard page. If the query fails or the region is not
                     * committed+readable (thread mid-create, guard page, garbage
                     * Rsp), skip the scan entirely -- a diagnostic AV here gets
                     * caught by the crash filter and kills the whole run. */
                    MEMORY_BASIC_INFORMATION mbi;
                    uint64_t region_end = (uint64_t)sp;
                    if (VirtualQuery((LPCVOID)sp, &mbi, sizeof mbi) &&
                        mbi.State == MEM_COMMIT &&
                        !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
                        (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE |
                                        PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
                        region_end = (uint64_t)mbi.BaseAddress + mbi.RegionSize;
                    int maxk = (int)((region_end - (uint64_t)sp) / 8);
                    if (maxk > 0x200000 / 8) maxk = 0x200000 / 8;   /* lifted SPU frames are big */
                    int found = 0;
                    for (int k = 0; k < maxk && found < 40; k++) {
                        uint64_t v = sp[k];
                        if (v < (uint64_t)self) continue;
                        HMODULE mm = NULL;
                        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                           (LPCSTR)v, &mm);
                        if (mm == self) {
                            /* Resolve to the guest function containing this return
                             * address: the function_table entry with the largest host
                             * pointer <= it. A raw RVA needs symbols a release build
                             * does not have, and "ret rva=0x519F66" says nothing about
                             * where a hung title actually is. Same mapping the
                             * unresolved-call dump uses. */
                            uint32_t bestGuest = 0;
                            uintptr_t bestHost = 0, nextHost = 0, maxHost = 0;
                            for (uint64_t q = 0; q < function_table_count; q++) {
                                uintptr_t h = (uintptr_t)function_table[q].func;
                                if (h > maxHost) maxHost = h;
                                if (h <= (uintptr_t)v && h > bestHost) {
                                    bestHost = h; bestGuest = function_table[q].addr;
                                }
                            }
                            for (uint64_t q = 0; q < function_table_count; q++) {
                                uintptr_t h = (uintptr_t)function_table[q].func;
                                if (h > bestHost && (nextHost == 0 || h < nextHost)) nextHost = h;
                            }
                            /* The highest-addressed lifted function has no next, so its
                             * extent is unknown -- and EVERY runtime/CRT address above it
                             * was attributed to it. Refuse to guess there. A confident
                             * wrong frame is worse than no frame. */
                            if (bestHost == maxHost ||
                                (uintptr_t)v - bestHost > 0x4000u) bestGuest = 0;
                            dbg_printf( bestGuest
                                    ? "[WATCHDOG]       tid %5lu ret rva=0x%llX  func_%08X+0x%llX\n"
                                    : "[WATCHDOG]       tid %5lu ret rva=0x%llX\n",
                                    (unsigned long)te.th32ThreadID,
                                    (unsigned long long)(v - (uint64_t)self),
                                    bestGuest, (unsigned long long)((uintptr_t)v - bestHost));
                            found++;
                        }
                    }
                }
            }
            ResumeThread(th);
            s_dbg_cap = NULL;
            if (s_dbg_capn) dbg_printf("%s", s_dbg_capbuf);
            CloseHandle(th);
        } while (Thread32Next(snap, &te));
    }
    if (snap != INVALID_HANDLE_VALUE) CloseHandle(snap);
    if (!s_dbg_raw) fflush(stderr);
}

/* ── Debug console ───────────────────────────────────────────────────────────
 * PS3_DEBUG=<path> opens a file-based command channel, so a title that is
 * already running can be asked what it is doing without a rebuild.
 *
 * Write one command into <path>; the console executes it, appends the answer
 * to <path>.out, and truncates <path> ready for the next one.
 *
 *     echo threads    > dbg.txt     # symbolised stack of every guest thread
 *     echo "mem 10200 64" > dbg.txt # hexdump of guest memory
 *     echo stat       > dbg.txt     # flips, HLE breadcrumb, uptime
 *
 * A file and not a socket on purpose: no listening port inside a game
 * process, nothing for a firewall to prompt about, and it drives from a shell
 * script exactly the way PAD_FILE already does.
 *
 * Read-only by design apart from poke32. Changing a diagnostic knob while the
 * title runs is NOT possible here: the large majority are read once at first
 * use and cached in a function-local static (see docs/DIAGNOSTICS.md), so they
 * are launch-time settings. `knobs` reports which ones this run was started
 * with, which is the useful half of that.
 */
static FILE* s_dbg_out = NULL;

static void dbg_printf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (s_dbg_cap) {             /* a thread is suspended: format only, no stream lock */
        int n = vsnprintf(s_dbg_cap + s_dbg_capn, sizeof s_dbg_capbuf - s_dbg_capn, fmt, ap);
        if (n > 0) s_dbg_capn += (size_t)n;
        if (s_dbg_capn >= sizeof s_dbg_capbuf) s_dbg_capn = sizeof s_dbg_capbuf - 1;
        va_end(ap);
        return;
    }
    if (s_dbg_raw) {             /* GOWA_FREEZE_DUMP: own file, no CRT stream lock */
        char b[16384];
        const int n = vsnprintf(b, sizeof b, fmt, ap);
        if (n > 0) { DWORD w; WriteFile(s_dbg_raw, b, (DWORD)(n < (int)sizeof b ? n : (int)sizeof b - 1), &w, NULL); }
        va_end(ap);
        return;
    }
    if (s_dbg_out) { va_list c; va_copy(c, ap); vfprintf(s_dbg_out, fmt, c); va_end(c); }
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static void dbg_mem(uint32_t ea, uint32_t len)
{
    if (!vm_base) { dbg_printf("  vm_base not live yet%c", 10); return; }
    if (len == 0 || len > 4096) len = 64;
    for (uint32_t off = 0; off < len; off += 16) {
        char asc[17];
        dbg_printf("  0x%08X ", ea + off);
        for (uint32_t i = 0; i < 16; i++) {
            if (off + i < len) {
                uint8_t b = vm_base[(size_t)(ea + off + i)];
                dbg_printf("%02X ", b);
                asc[i] = (b >= 32 && b < 127) ? (char)b : '.';
            } else { dbg_printf("   "); asc[i] = ' '; }
        }
        asc[16] = 0;
        dbg_printf(" |%s|%c", asc, 10);
    }
}

static void dbg_knobs(const char* prefix)
{
    /* Which diagnostics this run was actually started with. */
    LPCH env = GetEnvironmentStringsA();
    if (!env) return;
    int n = 0;
    for (LPCH p = env; *p; p += strlen(p) + 1) {
        const char* eq = strchr(p, '=');
        if (!eq || eq == p) continue;
        /* Only the runtime's own namespaces, not the whole shell environment. */
        static const char* known[] = {
            "PS3_", "SPU_", "SPURS_", "GCM_", "RSX_", "LD_", "YZ_", "YDKJ_",
            "LBP_", "FLOW_", "RD_", "TEX_", "FP_", "VP_", "PPU_", "PS1_",
            "CELLMARK_", "RTT_", "PAD_", "FS_", "WATCHDOG_", "SYNC_", NULL };
        int match = 0;
        for (int k = 0; known[k]; k++)
            if (strncmp(p, known[k], strlen(known[k])) == 0) { match = 1; break; }
        if (!match) continue;
        if (prefix && *prefix && strncmp(p, prefix, strlen(prefix)) != 0) continue;
        dbg_printf("  %s%c", p, 10);
        n++;
    }
    FreeEnvironmentStringsA(env);
    dbg_printf("  (%d set; docs/DIAGNOSTICS.md lists all of them)%c", n, 10);
}

/* "prof <host-tid> [sec]": sample ONE thread at ~1 kHz and unwind it through
 * .pdata (exact, unlike dump_threads' stack scan), then print the hottest
 * functions exclusive (leaf) and inclusive (anywhere on the stack), keyed by
 * function start. exe entries print as rva=; resolve against the link map.
 * Only lock-free work happens while the thread is suspended.
 * ponytail: fixed 4096-slot tables; a thread touching more functions drops the rest. */
#define PROF_SLOTS 4096
static uint64_t s_prof_key[3][PROF_SLOTS];
static uint32_t s_prof_ct[3][PROF_SLOTS];
static void prof_hit(int tab, uint64_t key)
{
    for (uint32_t i = (uint32_t)((key >> 4) * 2654435761u) % PROF_SLOTS, n = 0; n < PROF_SLOTS;
         n++, i = (i + 1) % PROF_SLOTS) {
        if (s_prof_key[tab][i] == key) { s_prof_ct[tab][i]++; return; }
        if (!s_prof_key[tab][i]) { s_prof_key[tab][i] = key; s_prof_ct[tab][i] = 1; return; }
    }
}
static void dbg_prof(DWORD tid, unsigned sec)
{
    /* Suspending ourselves never resumes: the console would hang for good. */
    if (tid == GetCurrentThreadId()) { dbg_printf("  tid %lu is the console%c", (unsigned long)tid, 10); return; }
    HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tid);
    if (!h) { dbg_printf("  cannot open tid %lu%c", (unsigned long)tid, 10); return; }
    memset(s_prof_key, 0, sizeof s_prof_key); memset(s_prof_ct, 0, sizeof s_prof_ct);
    const uintptr_t exe = (uintptr_t)GetModuleHandleA(NULL);
    unsigned samples = 0;
    const ULONGLONG end = GetTickCount64() + sec * 1000ull;
    while (GetTickCount64() < end) {
        if (SuspendThread(h) == (DWORD)-1) break;
        CONTEXT c; c.ContextFlags = CONTEXT_FULL;
        if (GetThreadContext(h, &c)) {
            samples++;
            uint64_t seen[24]; int ns = 0, got = 0;
            for (int d = 0; d < 24 && c.Rip; d++) {
                DWORD64 base = 0;
                PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry(c.Rip, &base, NULL);
                uint64_t key = rf ? base + rf->BeginAddress : c.Rip;
                if (d == 0) prof_hit(0, key);
                /* first exe frame of a sample whose leaf is in a DLL: who called out */
                if (!got && (uintptr_t)(key - exe) < 0x40000000u) { got = 1; if (d > 0) prof_hit(2, key); }
                int dup = 0; for (int k = 0; k < ns; k++) dup |= seen[k] == key;
                if (!dup) { seen[ns++] = key; prof_hit(1, key); }
                if (!rf) break;                       /* leaf without unwind data: stop */
                void* hd; DWORD64 ef;
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, c.Rip, rf, &c, &hd, &ef, NULL);
            }
        }
        ResumeThread(h);
        Sleep(1);
    }
    CloseHandle(h);
    dbg_printf("  prof tid %lu: %u samples over %us%c", (unsigned long)tid, samples, sec, 10);
    for (int tab = 0; tab < 3; tab++) {
        static const char* tn[3] = { "exclusive", "inclusive", "exe caller of DLL leaf" };
        dbg_printf("  --- %s ---%c", tn[tab], 10);
        for (int rank = 0; rank < 40; rank++) {
            uint32_t best = 0; int bi = -1;
            for (int k = 0; k < PROF_SLOTS; k++) if (s_prof_ct[tab][k] > best) { best = s_prof_ct[tab][k]; bi = k; }
            if (bi < 0) break;
            uint64_t key = s_prof_key[tab][bi];
            HMODULE m = NULL; char path[MAX_PATH] = "?";
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)key, &m);
            if (m) GetModuleFileNameA(m, path, sizeof path);
            const char* b = strrchr(path, 92); b = b ? b + 1 : path;
            if ((uintptr_t)m == exe) dbg_printf("  %5.1f%%  rva=0x%llX%c", 100.0 * best / (samples ? samples : 1),
                                                (unsigned long long)(key - exe), 10);
            else dbg_printf("  %5.1f%%  %s+0x%llX%c", 100.0 * best / (samples ? samples : 1), b,
                            (unsigned long long)(key - (uintptr_t)m), 10);
            s_prof_ct[tab][bi] = 0;
        }
    }
}

static DWORD WINAPI debug_console(LPVOID param)
{
    const char* path = (const char*)param;
    char outpath[1024];
    snprintf(outpath, sizeof outpath, "%s.out", path);

    HMODULE self = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)&debug_console, &self);
    const DWORD t0 = GetTickCount();
    fprintf(stderr, "[dbg] console on '%s' -- write a command, read '%s'%c",
            path, outpath, 10);

    for (;;) {
        Sleep(100);
        FILE* f = fopen(path, "rb");
        if (!f) continue;
        char cmd[512] = {0};
        if (!fgets(cmd, sizeof cmd, f)) { fclose(f); continue; }
        fclose(f);
        char* nl = strpbrk(cmd, "\r\n"); if (nl) *nl = 0;
        if (!cmd[0]) continue;
        FILE* t = fopen(path, "wb"); if (t) fclose(t);   /* consume it */

        s_dbg_out = fopen(outpath, "ab");
        dbg_printf("%c[dbg] > %s%c", 10, cmd, 10);

        char verb[64] = {0};
        unsigned a = 0, b = 0;
        sscanf(cmd, "%63s", verb);

        if (!strcmp(verb, "help")) {
            dbg_printf("  threads          stacks of every guest thread, symbolised%c", 10);
            dbg_printf("  prof <tid> [s]   sample one host thread ~1kHz, hottest functions%c", 10);
            dbg_printf("  hle              last HLE call the runtime dispatched%c", 10);
            dbg_printf("  stat             flips, HLE breadcrumb, uptime%c", 10);
            dbg_printf("  mem <hex> [len]  hexdump guest memory%c", 10);
            dbg_printf("  poke32 <hex> <v> write a guest u32%c", 10);
            dbg_printf("  knobs [prefix]   diagnostics this run was started with%c", 10);
        } else if (!strcmp(verb, "threads")) {
            dump_threads("console", self);
        } else if (!strcmp(verb, "find32") && sscanf(cmd, "%*s %x", &a) == 1) {
            /* find32 <hex>: every aligned big-endian u32 equal to <hex> in committed
             * guest memory below 0xC0000000 (first 4096 hits). */
            const uint32_t be = ((a & 0xFF) << 24) | ((a & 0xFF00) << 8) | ((a >> 8) & 0xFF00) | (a >> 24);
            int hits = 0;
            for (uintptr_t p = 0; p < 0xC0000000u && hits < 4096 && vm_base; ) {
                MEMORY_BASIC_INFORMATION mi;
                if (!VirtualQuery(vm_base + p, &mi, sizeof mi)) break;
                const uintptr_t end = (uintptr_t)((uint8_t*)mi.BaseAddress + mi.RegionSize) - (uintptr_t)vm_base;
                if (mi.State == MEM_COMMIT && !(mi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
                    for (uintptr_t q = p & ~3u; q + 4 <= end && q < 0xC0000000u && hits < 4096; q += 4)
                        if (*(const uint32_t*)(vm_base + q) == be) { dbg_printf("  0x%08X%c", (unsigned)q, 10); hits++; }
                p = end;
            }
            dbg_printf("  %d hit(s)%c", hits, 10);
        } else if (!strcmp(verb, "qbset") && sscanf(cmd, "%*s %x %x", &a, &b) == 2) {
            /* qbset <name-crc> <value>: set every integer QB struct member with
             * that name. A member is 16 bytes: xx 01|81 00 00 | name | value | next,
             * big-endian (GH3: bot_play, unlocked). Prints how many it set. */
            const uint32_t be_name = ((a & 0xFF) << 24) | ((a & 0xFF00) << 8) | ((a >> 8) & 0xFF00) | (a >> 24);
            const uint32_t be_val  = ((b & 0xFF) << 24) | ((b & 0xFF00) << 8) | ((b >> 8) & 0xFF00) | (b >> 24);
            unsigned n = 0;
            for (uintptr_t p = 0; p < 0xC0000000u && vm_base; ) {
                MEMORY_BASIC_INFORMATION mi;
                if (!VirtualQuery(vm_base + p, &mi, sizeof mi)) break;
                const uintptr_t end = (uintptr_t)((uint8_t*)mi.BaseAddress + mi.RegionSize) - (uintptr_t)vm_base;
                if (mi.State == MEM_COMMIT && (mi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)))
                    for (uintptr_t q = (p + 4) & ~3u; q + 8 <= end; q += 4)
                        if (*(const uint32_t*)(vm_base + q) == be_name) {
                            /* type word: any flags byte, then 0x01 or 0x81 (integer), 00 00;
                             * only 0/1 values change -- an unlocked tag is a flag. */
                            const uint8_t* t = vm_base + q - 4;
                            const uint32_t v = *(const uint32_t*)(vm_base + q + 4);
                            if ((t[1] == 0x01 || t[1] == 0x81) && !t[2] && !t[3] &&
                                (v == 0 || v == 0x01000000u)) {
                                *(uint32_t*)(vm_base + q + 4) = be_val; n++;
                            }
                        }
                p = end;
            }
            dbg_printf("  qbset 0x%08X = %u: %u member(s)%c", a, b, n, 10);
        } else if (!strcmp(verb, "prof") && sscanf(cmd, "%*s %u %u", &a, &b) >= 1) {
            dbg_prof((DWORD)a, b ? b : 5);
        } else if (!strcmp(verb, "hle")) {
            dbg_printf("  last HLE = 0x%08X (%s)%c", g_last_hle_nid,
                       g_last_hle_name ? g_last_hle_name : "", 10);
        } else if (!strcmp(verb, "stat")) {
            dbg_printf("  uptime   %.1f s%c", (GetTickCount() - t0) / 1000.0, 10);
            dbg_printf("  flips    %u%c", cellGcm_flip_request_count(), 10);
            dbg_printf("  last HLE 0x%08X (%s)%c", g_last_hle_nid,
                       g_last_hle_name ? g_last_hle_name : "", 10);
            dbg_printf("  vm_base  %s%c", vm_base ? "live" : "not mapped", 10);
        } else if (!strcmp(verb, "mem") && sscanf(cmd, "%*s %x %u", &a, &b) >= 1) {
            dbg_mem(a, b);
        } else if (!strcmp(verb, "poke32") && sscanf(cmd, "%*s %x %x", &a, &b) == 2) {
            if (vm_base) {
                uint32_t be = ((b & 0xFF) << 24) | ((b & 0xFF00) << 8) |
                              ((b >> 8) & 0xFF00) | ((b >> 24) & 0xFF);
                memcpy(vm_base + a, &be, 4);
                dbg_printf("  [0x%08X] = 0x%08X%c", a, b, 10);
            } else dbg_printf("  vm_base not live yet%c", 10);
        } else if (!strcmp(verb, "knobs")) {
            char pfx[64] = {0};
            sscanf(cmd, "%*s %63s", pfx);
            dbg_knobs(pfx);
        } else {
            dbg_printf("  ? '%s' -- try 'help'%c", verb, 10);
        }

        if (s_dbg_out) { fclose(s_dbg_out); s_dbg_out = NULL; }
    }
    return 0;
}

/* GOWA_FREEZE_DUMP=<s> (21/10): freeze detector. When the game's own flip requests
 * (cellGcm_flip_request_count) do not move for <s> seconds after the first 10, dump every
 * thread (dump_threads, twice, 2 s apart) to out/freeze_<pid>.txt through WriteFile only --
 * the boot freeze at ~21 s of 21/10 left no watchdog output at all, consistent with a stuck
 * thread owning stderr's CRT lock. Once per run; the guest-LR report (plain fprintf) goes
 * last, after the file is closed. */
extern "C" u32 cellGcm_flip_request_count(void);
static DWORD WINAPI freeze_watch(LPVOID p)
{
    const int lim = (int)(intptr_t)p > 0 ? (int)(intptr_t)p : 12;
    HMODULE self = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)&freeze_watch, &self);
    Sleep(10000);
    u32 last = cellGcm_flip_request_count();
    int parado = 0;
    for (;;) {
        Sleep(1000);
        const u32 now = cellGcm_flip_request_count();
        if (now != last) { last = now; parado = 0; continue; }
        if (++parado < lim) continue;
        char path[MAX_PATH];
        snprintf(path, sizeof path, "out/freeze_%lu.txt", (unsigned long)GetCurrentProcessId());
        HANDLE h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) return 0;
        s_dbg_raw = h;
        char lbl[96];
        snprintf(lbl, sizeof lbl, "congelado: flips parados ha %d s (flips=%u)", parado, (unsigned)now);
        dump_threads(lbl, self);
        Sleep(2000);
        dump_threads("2o despejo (+2 s)", self);
        s_dbg_raw = NULL;
        CloseHandle(h);
        ppu_report_guest_lrs();
        return 0;
    }
}

static DWORD WINAPI hang_watchdog(LPVOID)
{
    HMODULE self = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)&hang_watchdog, &self);
    /* ponytail: two fixed samples cover a boot wedge, but a hang that happens
     * minutes in (e.g. after a level load) needs them moved. WATCHDOG_AT="a,b"
     * gives the two sample times in seconds. */
    /* GOWA (25/10): only when WATCHDOG_AT asks. The fixed 8 s and 15 s samples dumped every thread in every
     * player's log while the game loaded normally (two thousand lines, and the log a report attaches is its last
     * 256 KB); a real freeze is still dumped by the flip watchdog above. */
    int t1 = 8, t2 = 15;
    { const char* e = getenv("WATCHDOG_AT"); int a = 0, b = 0;
      if (!e || !*e) return 0;
      if (sscanf(e, "%d,%d", &a, &b) == 2 && a > 0 && b > a) { t1 = a; t2 = b; } }
    char lbl[64];
    Sleep((DWORD)t1 * 1000);
    snprintf(lbl, sizeof lbl, "%ds sample", t1); dump_threads(lbl, self);
    Sleep((DWORD)(t2 - t1) * 1000);
    snprintf(lbl, sizeof lbl, "%ds sample", t2); dump_threads(lbl, self);
    return 0;
}
#endif

/* The flat VM treats every address as valid RAM, so it must span every region
 * the PS3 memory map uses. The game's heap maps at 0x20000000+ and reaches
 * ~0x50000000, but sys_ppu_thread_create allocates thread stacks in the PS3
 * stack region at 0xD0000000-0xDFFFFFFF (vm.h: VM_STACK_BASE). Without covering
 * that, every spawned thread's stack access is OOB (reads 0 / writes dropped)
 * and the thread crashes. Size to include the stack region: ~3.75 GB, lazily
 * committed by the OS (only touched pages are backed). */
#define VM_SIZE    0x100010000ull /* full 32-bit guest space + 64K guard (top-edge reads), demand-committed */
#define STACK_TOP  0x0FF00000u   /* main-thread stack, below the 0x10000000 segment */

#ifdef _WIN32
/* Demand-paging for the flat VM: reserve the full 4 GB guest space up front (no
 * commit cost) and commit each 64 KB page on first access. This makes EVERY
 * 32-bit guest offset valid -- a garbage guest pointer reads as zero instead of
 * crashing the process (essential now that the recompiled engine runs deep and
 * worker threads touch incomplete state). Out-of-arena faults fall through to
 * the crash reporter. */
/* Guard-page-violation reporter: STATUS_GUARD_PAGE_VIOLATION (0x80000001)
 * bypasses every ACCESS_VIOLATION-only handler and kills the process SILENTLY
 * (observed the moment the Bink SPU decoder first really ran). Log full context
 * before letting the chain continue. */
static LONG WINAPI guard_report_veh(EXCEPTION_POINTERS* ep)
{
    if (ep->ExceptionRecord->ExceptionCode == 0x80000001u /*STATUS_GUARD_PAGE_VIOLATION*/) {
        static LONG s_n = 0;
        if (InterlockedIncrement(&s_n) <= 8) {
            char* mbase = (char*)GetModuleHandleA(NULL);
            fprintf(stderr, "\n[GUARDVIOLATION] tid=%lu %s fault=0x%llX rip-rva=0x%llX\n",
                    GetCurrentThreadId(),
                    ep->ExceptionRecord->ExceptionInformation[0] ? "write" : "read",
                    (unsigned long long)ep->ExceptionRecord->ExceptionInformation[1],
                    (unsigned long long)((char*)ep->ExceptionRecord->ExceptionAddress - mbase));
            void* fr[24]; USHORT n = RtlCaptureStackBackTrace(0, 24, fr, NULL);
            fprintf(stderr, "[GUARDVIOLATION] bt-rva:");
            for (USHORT i = 0; i < n; i++)
                fprintf(stderr, " %llX", (unsigned long long)((char*)fr[i] - mbase));
            fprintf(stderr, "\n"); fflush(stderr);
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static LONG WINAPI vm_commit_veh(EXCEPTION_POINTERS* ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        ULONG_PTR fault = ep->ExceptionRecord->ExceptionInformation[1];
        uintptr_t base  = (uintptr_t)vm_base;
        if (vm_base && fault >= base && fault < base + VM_SIZE) {
            void* page = (void*)(fault & ~(uintptr_t)0xFFFF);
            if (VirtualAlloc(page, 0x10000, MEM_COMMIT, PAGE_READWRITE))
                return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

int main(int argc, char** argv)
{
#ifdef _WIN32
    /* GOWA: padrões do port, antes de qualquer getenv (o cache de getenv acima
     * guarda o primeiro valor lido). Uma variável já definida no ambiente vence.
     *  YDKJ_NO_REENTRY_ISO=1 -- a ponte HLE->libsre desloca r1 em 0x1000 por
     *  padrão, o que corrompe o 9º argumento (pilha) de funções como
     *  _cellSpursWorkloadAttributeInitialize; o GOWA tem TOC fixo (TOCFIX) e
     *  não precisa do isolamento. */
    if (!GetEnvironmentVariableA("YDKJ_NO_REENTRY_ISO", NULL, 0))
        SetEnvironmentVariableA("YDKJ_NO_REENTRY_ISO", "1");
    /*  RSX_LIVE_DRAW=1 -- o GOWA desenha (logos da intro) com INLINE_ARRAY, que
     *  só o motor live-draw trata; o caminho padrão do backend D3D12 fica em
     *  "draws: 0" e tela preta. */
    if (!GetEnvironmentVariableA("RSX_LIVE_DRAW", NULL, 0))
        SetEnvironmentVariableA("RSX_LIVE_DRAW", "1");
    /*  GCM_PARK_GRACE_MS=infinito -- no loop do jogo o GOWA deixa `put` fixo no
     *  fim do anel (0xAF8800) e controla a GPU só com self-JUMPs que ele mesmo
     *  abre. O resync por "park parado 50 ms" pulava os GET_REPORT seguintes
     *  (0x1ED40/0x1ED50), e o job WWS do wid 22 esperava esses reports para
     *  sempre. */
    if (!GetEnvironmentVariableA("GCM_PARK_GRACE_MS", NULL, 0))
        SetEnvironmentVariableA("GCM_PARK_GRACE_MS", "999999999");
    /*  GCM_SEMA_ACQUIRE_US=2000000 (24/10) -- o leitor do FIFO espera o
     *  SEMAPHORE_ACQUIRE como a RSX (rótulos 0x510/0x520, que a MeshProc libera
     *  depois de escrever o que vem em seguida), no máximo 2 s (estourou = segue
     *  como antes). Ignorado, numa máquina lenta o leitor chegava dezenas de ms
     *  antes da SPU e lia o trecho do quadro anterior: desvio para IO não mapeado
     *  e jogo parado em ~1 de cada 3 rodadas no notebook. Medido: 0 estouros
     *  (notebook e aqui), mais de 4 mil esperas cumpridas por execução até aqui,
     *  FPS igual. GCM_SEMA_ACQUIRE_US=-1 volta ao antigo. */
    if (!GetEnvironmentVariableA("GCM_SEMA_ACQUIRE_US", NULL, 0))
        SetEnvironmentVariableA("GCM_SEMA_ACQUIRE_US", "2000000");
    /*  GOWA_MAILBOX_GATE=A17930:5:5000 -- cada GET_REPORT libera um job WWS
     *  (wid 22) que grava a caixa de correio única das 5 tarefas MeshProc
     *  (0x00A17930) e as sinaliza. Sem a latência real da GPU a rodada seguinte
     *  chegava no meio da anterior, a cadeia de sinais entre as tarefas se
     *  embaralhava e um contador 0xA032xx nunca zerava (trava no 4º frame, ou
     *  ~1 fps com o ritmo de reports). O PUT na caixa espera as 5 leituras e o
     *  contador da rodada anterior zerar (spu_channels.c).
     *  GOWA_SIG_BACKPRESSURE=11037280:3:5000 -- sinal de taskset reenviado com o
     *  bit ainda ligado falha o PUTLLC e é tentado de novo, em vez de se perder.
     *  GOWA_REPORT_PACE_MS=0 -- o ritmo fixo de reports (paliativo antigo) fica
     *  desligado. */
    if (!GetEnvironmentVariableA("GOWA_MAILBOX_GATE", NULL, 0))
        SetEnvironmentVariableA("GOWA_MAILBOX_GATE", "A17930:5:5000");
    if (!GetEnvironmentVariableA("GOWA_SIG_BACKPRESSURE", NULL, 0))
        SetEnvironmentVariableA("GOWA_SIG_BACKPRESSURE", "11037280:3:5000,15E66100:5:2000");   /* + taskset do inflate (wid 5) */
    if (!GetEnvironmentVariableA("GOWA_REPORT_PACE_MS", NULL, 0))
        SetEnvironmentVariableA("GOWA_REPORT_PACE_MS", "0");
    /*  SPU_INTERP_UNLIFTED=1 -- código SPU sem tradução (plugins copiados em
     *  tempo de execução) roda no interpretador e volta ao nativo.
     *  GOWA_JOBLIFT_SKIP=7B6680 -- módulos de job que ficam interpretados:
     *  0x7B6680 (som, SPU 5: nativo, o boot levava ~11 s a mais e a abertura
     *  travava ~1/3 das vezes -- a reavaliar depois do opt-out de timer).
     *  0x6FC080 saiu da lista em 17/10: as travas dele na fase ("HOTREAD" +
     *  CopyLocalMemory) eram o estouro do heap de VRAM (job UpdateTexStreaming
     *  sem espera, ver gowa_texstream_wait); nativo, a fase roda a 17-18 FPS
     *  (13-15 interpretado). GOWA_JOBLIFT_SKIP=6FC080,7B6680 volta ao antigo. */
    if (!GetEnvironmentVariableA("SPU_INTERP_UNLIFTED", NULL, 0))
        SetEnvironmentVariableA("SPU_INTERP_UNLIFTED", "1");
    /*  Uma 7a SPU só para os tasksets (GeneralPurpose/High e as 5 tarefas
     *  MeshProc), que saem das SPUs 0-3, as únicas do EDGE: A/Bs abtsaway e
     *  abtsaway2 (20/10), +2,5 FPS em 4 pares. Os jobs WWS continuam em 6 SPUs
     *  (o cabeçalho das listas de job só tem lugar para 6). GOWA_SPURS_NSPU=6
     *  volta ao antigo.
     *  30/10: só com mais de 8 CPUs lógicas. Num i5-10300H (4 núcleos / 8 lógicas) a 7a SPU disputa a CPU com a
     *  PPU e a RSX: cap. 16 com 6 SPUs 11,0 / 10,4 / 10,2 x 7 SPUs 10,4 / 9,6 / 9,8 (+6%, 3 de 3 pares). */
    { SYSTEM_INFO si_ns; GetSystemInfo(&si_ns);
      if (!GetEnvironmentVariableA("GOWA_SPURS_NSPU", NULL, 0) && si_ns.dwNumberOfProcessors <= 8) {
          SetEnvironmentVariableA("GOWA_SPURS_NSPU", "6");
          fprintf(stderr, "[spurs] 6 SPUs (%lu CPUs lógicas; a 7a só com mais de 8; GOWA_SPURS_NSPU=7 força)\n",
                  (unsigned long)si_ns.dwNumberOfProcessors);
      } }
    if (!GetEnvironmentVariableA("GOWA_SPURS_NSPU", NULL, 0)) {
        SetEnvironmentVariableA("GOWA_SPURS_NSPU", "7");
        if (!GetEnvironmentVariableA("GOWA_SPURS_NSPU_WKL", NULL, 0))
            SetEnvironmentVariableA("GOWA_SPURS_NSPU_WKL", "ts:");
        if (!GetEnvironmentVariableA("GOWA_SPURS_TS_AWAY", NULL, 0))
            SetEnvironmentVariableA("GOWA_SPURS_TS_AWAY", "F");
    }
    /*  GOWA_TASKLIFT_SKIP -- a tarefa ELF 0x6DC300 (inflate do edgezlib) ficava
     *  interpretada porque "travava a abertura de vez em quando". A trava era a
     *  reserva lwarx/stwcx por palavra (ppu_resv_break_line, 17/10); com ela
     *  corrigida a tarefa nativa carrega a 1a fase em 29 s em vez de 80 s.
     *  Nenhuma tarefa pulada por padrão; GOWA_TASKLIFT_SKIP=6DC300 volta ao antigo. */
    /* PS3_VERBOSE=0: the per-event lines ([WAIT] event_queue_receive, [evt]
     * port_send) are ~160/s once the sound workload runs (SPU 5 alive, 17/10)
     * -- 157k lines in 8 min, all through one stderr lock. PS3_VERBOSE=1
     * brings them back. */
    if (!GetEnvironmentVariableA("PS3_VERBOSE", NULL, 0))
        SetEnvironmentVariableA("PS3_VERBOSE", "0");
    /* 18/10: nenhum módulo de job fica interpretado. O 0x7B6680 (som) parava a
     * SPU 5 no boot (desvio de volta para código carregado em tempo de execução
     * com p == link, corrigido em spu_channels.c) -- era isso os ~10 s a mais;
     * nativo, "registered level variables" sai aos 0,55 s.
     * GOWA_JOBLIFT_SKIP=7B6680 (ou 6FC080,7B6680) volta ao antigo. */
    if (!GetEnvironmentVariableA("GOWA_JOBLIFT_SKIP", NULL, 0))
        SetEnvironmentVariableA("GOWA_JOBLIFT_SKIP", "NONE");
    /* GOWA_PM_NATIVE=1: o policy module WWS inteiro nativo (região residente
     * em LS 0xA00). GOWA_UNLIFTED_REJOIN=1: o código sem tradução (o trecho em
     * 0x3FEC0 com que o PM entra nos jobs, os plugins) pode voltar ao nativo
     * no caminho, parando onde o colchete nativo espera. Juntos: fase de 23,7
     * para 26,5 FPS (runs 468/469). =0 em qualquer um volta ao antigo. */
    if (!GetEnvironmentVariableA("GOWA_PM_NATIVE", NULL, 0))
        SetEnvironmentVariableA("GOWA_PM_NATIVE", "1");
    if (!GetEnvironmentVariableA("GOWA_UNLIFTED_REJOIN", NULL, 0))
        SetEnvironmentVariableA("GOWA_UNLIFTED_REJOIN", "1");
    /* SPU_CH_BLOCK=1 (18/10): uma SPU que lê um canal vazio (o kernel do SPURS
     * ocioso esperando a linha mudar em rdch SPU_RdEventStat) dorme em vez de
     * girar; quem escreve a linha a acorda. Fase 28,2 -> 29,6 FPS (run506 x
     * run507), menu bem mais rápido. =0 volta ao giro. */
    if (!GetEnvironmentVariableA("SPU_CH_BLOCK", NULL, 0))
        SetEnvironmentVariableA("SPU_CH_BLOCK", "1");
    /* GOWA_MLAA_BYPASS=1 (18/10): o quadro que o jogo copia para 1:0xB20000
     * para o MLAA das SPUs fica na GPU (sem leitura de volta nem reenvio por
     * quadro) e a imagem sai sem o MLAA. Fase 29,6 -> 30,5 FPS (run508) e é o
     * que deixa subir a resolução interna depois. =0 volta ao MLAA. */
    if (!GetEnvironmentVariableA("GOWA_MLAA_BYPASS", NULL, 0))
        SetEnvironmentVariableA("GOWA_MLAA_BYPASS", "1");
    /* SPU_DMA_REPEAT_LIMIT=0: sem o detector que para uma SPU após N DMAs
     * idênticos seguidos. O kernel do SPURS consulta um label da RSX
     * (0x20000530) em laço enquanto espera a GPU; com as SPUs mais rápidas e as
     * ociosas dormindo, ele disparava com 256 e travava o quadro (run514). */
    if (!GetEnvironmentVariableA("SPU_DMA_REPEAT_LIMIT", NULL, 0))
        SetEnvironmentVariableA("SPU_DMA_REPEAT_LIMIT", "0");
    /* GCM_FIFO_CIRCLE_PASSES=0 (21/10): sem o detector de "circling" do leitor
     * do FIFO (4 passes seguidos queimando o orçamento inteiro -> pula para o
     * put). Ele vem de outro jogo (AMGL); no GoW o put fica fixo no fim do anel
     * e a RSX para em parks, e um passe que queima o orçamento é o leitor
     * ATRASADO nos primeiros segundos pesados da jogabilidade (~600 mil pacotes
     * por quadro no cap. 16), não um ciclo: o pulo descartava comandos e cercas
     * e o jogo travava logo depois do carregamento (a trava intermitente de
     * 19/10-21/10; [fifo-hist] mostrou o rastro linear). =4 volta ao antigo. */
    if (!GetEnvironmentVariableA("GCM_FIFO_CIRCLE_PASSES", NULL, 0))
        SetEnvironmentVariableA("GCM_FIFO_CIRCLE_PASSES", "0");
    /* GCM_FIFO_CATCHUP_KB (21/10): the walker drained past flips whenever put - get was
     * above 64 KB ("catch-up"), and the frames in between were never presented -- a
     * chapter-16 frame is far above 64 KB of commands and put often sits in another
     * segment of the ring, so 6-18 of the game's ~40 frames a second reached the screen
     * (A/B abcatch). Off by default now: every flip is presented (screen = game rate). */
    if (!GetEnvironmentVariableA("GCM_FIFO_CATCHUP_KB", NULL, 0))
        SetEnvironmentVariableA("GCM_FIFO_CATCHUP_KB", "1048576");
    /* GOWA_SPURS_PRIO (22/10): General6 (the 0x727200 jobs) at priority 2 on SPUs 0-5. The
     * frame's critical chain (GOWA_TL) is: the EDGE batch ends -> the main thread wakes ->
     * General6 -> RenJobSet -> RenderJobs -> flip; with the game's priority (13 on SPUs 0-3,
     * below EDGE's 3/8) General6 ran almost only on SPUs 4/5 while 0-3 ran ahead on the next
     * EDGE batch. A/B abprio/abprio2/abprio3 (chapter 16, 4 runs each): 40.88 -> 42.28;
     * raising RenJobSet/RenderJobs too is worse. =none turns it off.
     * Later the same day (flip without the RSX wait) SPUs 0-3 are saturated (21.2 of a
     * 21.1 ms frame) and the limit is the total work on them, not the chain's latency:
     * General6 at 4 there (below the EDGE in turn, 3) and RenJobSet/RenderJobs at 15 (they
     * run on 0-3 only when nothing else is ready; their own priority on SPUs 4/5). A/B
     * abprio5 (chapter 16, 4 runs each): 48.33 -> 50.00.
     * 23/10: General6 off SPUs 0-3 entirely -- on 4/5 and on the 7th SPU (6, read as 4 by
     * the job manager with GOWA_SPU_NUM_REMAP=1, so slot 4 of the job list header gets
     * priority 0 there). A/Bs abg6/abvc1 (chapter 16): 48.75 (General6 at 4 on 0-3 too)
     * -> 49.65, 49.18 -> 49.63, 50.65 -> 51.38 (with the 0x727200 4-call kernel).
     * 24/10: with the RSX lighter, SPUs 0-3 are the limit again (~92% busy: EDGE ~57%, job
     * manager ~15%, RenderJobs/RenJobSet ~10%). GOWA_SPU_NUM_REMAP=3 makes the 7th SPU (6)
     * read 3, so it takes slot 3 of the job lists that leave SPU 3: General6, RenJobSet and
     * RenderJobs go off SPU 3 and onto SPU 6 (half idle), and SPU 3 is left to the EDGE.
     * Triage abren7 (chapter 16, 4 runs each): 62.78 -> 64.93; render off all of SPUs 0-3
     * 62.98 (SPUs 4-6 cannot take all of it). */
    if (!GetEnvironmentVariableA("GOWA_SPURS_PRIO", NULL, 0))
        SetEnvironmentVariableA("GOWA_SPURS_PRIO",
            "General6=0F0F0F0000020200;RenJobSet=0F0F0F0008080800;RenderJobs=0F0F0F000A0A0A00");
    if (!GetEnvironmentVariableA("GOWA_SPU_NUM_REMAP", NULL, 0))
        SetEnvironmentVariableA("GOWA_SPU_NUM_REMAP", "3");
    /* GOWA_TAIL_TICKS=1 (24/10): the direct transfers between lifted functions count as
     * ticks of the lifted-path interrupt delay, so they keep going while an event is
     * pending (spu_drain.c) -- the EDGE job's DMA waits and the delay window after it
     * enables interrupts ran one instruction per drain step (~37 M drain transfers a
     * second). A/B abtt8 (chapter 16, 6 runs each, with the remap above): 65.23
     * (64.7-65.8) -> 67.05 (66.2-67.7), every run above every run. =0 turns it off. */
    if (!GetEnvironmentVariableA("GOWA_TAIL_TICKS", NULL, 0))
        SetEnvironmentVariableA("GOWA_TAIL_TICKS", "1");
    /* GOWA_SPU_PUT_HOOK=0 (23/10): no SPU PUT bookkeeping for the FIFO resync
     * (gcm_note_spu_put). This title never resyncs (no "FIFO resync" in any run),
     * and every EDGE PUT into RSX-visible memory wrote a table shared by the four
     * EDGE SPUs. =1 turns it back on. */
    if (!GetEnvironmentVariableA("GOWA_SPU_PUT_HOOK", NULL, 0))
        SetEnvironmentVariableA("GOWA_SPU_PUT_HOOK", "0");
    /* GOWA_LL_STRIPES=1 (23/10): one lock-line lock per 128-byte line (64 stripes,
     * spu_coherency.c) instead of the single lock. A/B abll3 (chapter 16, 6 runs
     * each): 59.43 -> 61.03, and no run dipped (without it 3 of 6 fell to 46-52
     * mid-window, the single lock contended when atomic traffic peaks). =0 off. */
    if (!GetEnvironmentVariableA("GOWA_LL_STRIPES", NULL, 0))
        SetEnvironmentVariableA("GOWA_LL_STRIPES", "1");
    /* GOWA_PIN_RSX_SOLO (21/10): the RSX-present thread keeps its physical core to itself
     * (gowa_pin_thread). A/B absolo, chapter 16, 6 runs each: 37.70 (33.7-39.9, bimodal)
     * -> 39.90 (38.4-40.5); the slow mode was that thread sharing its core. =0 turns it off. */
    if (!GetEnvironmentVariableA("GOWA_PIN_RSX_SOLO", NULL, 0))
        SetEnvironmentVariableA("GOWA_PIN_RSX_SOLO", "1");
    /* GOWA_IMGGUARD: texto + OPD do ELF só-leitura 30 s após o boot (custo zero;
     * bloqueia/nomeia escritas perdidas como a que corrompia o OPD 0x833840). */
    /* PS3_GCM_LOCAL_SIZE=0F900000: o tamanho de VRAM que um console de verdade
     * informa (249 MB; o RPCS3 usa o mesmo). Com os 256 MB do runtime, o
     * desfragmentador de VRAM do jogo (MoveLocalMemory/CopyLocalMemory) passava do
     * fim da memória local ("dst offset error"), alocações voltavam nulas e cópias
     * para NULL+offset caíam no executável: OPD 0x833840 corrompido, ruído de
     * textura e texto do HUD embaralhado na fase (6 de 9 execuções; 0 de 3 assim). */
    if (!GetEnvironmentVariableA("PS3_GCM_LOCAL_SIZE", NULL, 0))
        SetEnvironmentVariableA("PS3_GCM_LOCAL_SIZE", "0F900000");
    if (!GetEnvironmentVariableA("GOWA_IMGGUARD", NULL, 0))
        SetEnvironmentVariableA("GOWA_IMGGUARD", "30");
    /*  PS3_HDD0_ROOT=<PS3_VFS_ROOT> -- sem ele o sys_fs (usado pelo FIOS)
     *  "achata" qualquer caminho com USRDIR/ para <vfs>/USRDIR/..., e
     *  /dev_hdd0/game/BCUS98232/USRDIR/{PATCHES,TPPS.PSARC} (o update 1.04)
     *  nunca era encontrado: o executável 1.04 rodava com dados sem patch, o
     *  R_Multiplayer.wad não carregava e o jogo parava em "waiting for network
     *  layer to be connected". */
    /*  PS3_BOOT_PATCH=1 -- o GOWA.elf é o executável do update 1.04: o
     *  cellGameBootCheck precisa informar CELL_GAME_ATTRIBUTE_PATCH para o jogo
     *  pôr /dev_hdd0/game/BCUS98232/USRDIR (PATCHES, TPPS.PSARC) na busca do
     *  FIOS (func_002C31E8 testa o bit 0x1 dos attributes). */
    if (!GetEnvironmentVariableA("PS3_BOOT_PATCH", NULL, 0))
        SetEnvironmentVariableA("PS3_BOOT_PATCH", "1");
    /*  Report 340 (29/10): o disco em outro drive, onde a junção e o link físico não funcionam, e sem espaço para
     *  copiar os 34 GB. O gerador então grava em <vfs>\disco_direto.txt (UTF-8) a pasta que contém o PS3_GAME do
     *  jogador: o disco (/dev_bdvd) é lido direto de lá e o update (/dev_hdd0) continua no vfs do port. */
    {   wchar_t vw[MAX_PATH]; const DWORD nv = GetEnvironmentVariableW(L"PS3_VFS_ROOT", vw, MAX_PATH);
        if (nv && nv < MAX_PATH - 32) {
            wchar_t arq[MAX_PATH]; swprintf(arq, MAX_PATH, L"%s\\disco_direto.txt", vw);
            FILE* f = _wfopen(arq, L"rb");
            if (f) {
                char u8[2048] = {0}; const size_t k = fread(u8, 1, sizeof u8 - 1, f); fclose(f);
                u8[k] = 0;
                char* p = u8; if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) p += 3;
                size_t L = strlen(p); while (L && (p[L - 1] == '\r' || p[L - 1] == '\n' || p[L - 1] == ' ')) p[--L] = 0;
                wchar_t dw[MAX_PATH] = {0};
                if (L && MultiByteToWideChar(CP_UTF8, 0, p, -1, dw, MAX_PATH)) {
                    wchar_t usr[MAX_PATH]; swprintf(usr, MAX_PATH, L"%s\\PS3_GAME\\USRDIR", dw);
                    if (GetFileAttributesW(usr) != INVALID_FILE_ATTRIBUTES) {
                        if (!GetEnvironmentVariableW(L"PS3_HDD0_ROOT", NULL, 0)) SetEnvironmentVariableW(L"PS3_HDD0_ROOT", vw);
                        if (!GetEnvironmentVariableW(L"PS3_HDD1_ROOT", NULL, 0)) SetEnvironmentVariableW(L"PS3_HDD1_ROOT", vw);
                        /* o runtime lê as variáveis em ANSI: com acento no caminho, o nome curto (8.3) evita perder letras */
                        wchar_t curto[MAX_PATH]; const DWORD nc = GetShortPathNameW(dw, curto, MAX_PATH);
                        SetEnvironmentVariableW(L"PS3_VFS_ROOT", (nc && nc < MAX_PATH) ? curto : dw);
                        fprintf(stderr, "[disco] lido direto de %s (o update fica no vfs do port)\n", p);
                    } else {
                        /* o vfs\PS3_GAME é só a pasta mínima (PARAM.SFO e ícones): sem o disco o jogo sairia calado */
                        fprintf(stderr, "[disco] o disco_direto.txt aponta para %s, mas ali não há PS3_GAME\\USRDIR "
                                        "(a pasta mudou de lugar ou o drive está desligado)\n", p);
                        fflush(stderr);
                        wchar_t msg[2400];
                        swprintf(msg, sizeof msg / sizeof msg[0],
                                 L"O jogo não achou o disco do God of War: Ascension em:\n%ls\n\n"
                                 L"O port lê o disco direto dessa pasta, e ela foi movida, apagada ou está num HD desligado.\n\n"
                                 L"Ligue o HD de novo ou, no Otavix Launcher, abra o God of War: Ascension e clique em "
                                 L"\"Escolher o disco de novo\".\n\n"
                                 L"The game could not find the God of War: Ascension disc in the folder above: it was moved, "
                                 L"deleted or is on a drive that is off. Turn the drive on or, in the Otavix Launcher, open "
                                 L"the game and click \"Pick the disc again\".", dw);
                        MessageBoxW(NULL, msg, L"God of War: Ascension", MB_ICONERROR | MB_OK | MB_SETFOREGROUND);
                        ExitProcess(3);
                    }
                }
            }
        } }
    if (!GetEnvironmentVariableA("PS3_HDD0_ROOT", NULL, 0)) {
        char vroot[1024];
        DWORD n = GetEnvironmentVariableA("PS3_VFS_ROOT", vroot, sizeof vroot);
        if (n > 0 && n < sizeof vroot) SetEnvironmentVariableA("PS3_HDD0_ROOT", vroot);
    }
#endif
    /* <PPU ELF> [--username NAME] [--psnr host[:port]], flags in any order. */
    const char* elf = NULL;
    const char* username = NULL;
    const char* psnr = NULL;
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        const char** flag = !strncmp(a, "--username", 10) ? &username
                          : !strncmp(a, "--psnr", 6)      ? &psnr : NULL;
        if (!flag) { if (!elf) elf = a; continue; }
        const char* eq = strchr(a, '=');
        if (eq) *flag = eq + 1;
        else if (i + 1 < argc) *flag = argv[++i];
    }
    if (!elf) {
        printf("usage: %s <PPU ELF> [--username NAME] [--psnr host[:port]]\n", argv[0]);
        return 2;
    }
    np_psnr_setup(username, psnr);

#ifdef _WIN32
#pragma comment(lib, "winmm.lib")
    timeBeginPeriod(1);   /* 1ms timer resolution: the default ~15.6ms granularity
                           * inflates every sub-15ms wait (the game's event polls,
                           * usleeps) and throttled the whole title. */
    /* Windows 11 stops honouring timeBeginPeriod for a process whose windows
     * are all occluded or minimized, and may move it to EcoQoS. The test window
     * lives on the second monitor, where it is easily covered. Opt out of both
     * (GOWA_TIMER_OPTOUT=0 keeps the default behaviour, for comparison). */
    { const char* e = getenv("GOWA_TIMER_OPTOUT");
      if (!(e && *e == '0')) {
          PROCESS_POWER_THROTTLING_STATE pt = {};
          pt.Version     = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
          pt.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED | 0x4u /* IGNORE_TIMER_RESOLUTION */;
          pt.StateMask   = 0;
          const BOOL ok = SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &pt, sizeof pt);
          fprintf(stderr, "[timer] opt-out de EcoQoS/resolução com janela encoberta: %s\n", ok ? "ok" : "FALHOU");
      } }
    /* GOWA_SLEEP_PROBE=1: every 10 s, how long Sleep(1) really takes. */
    if (getenv("GOWA_SLEEP_PROBE"))
        CreateThread(NULL, 0, [](LPVOID) -> DWORD {
            LARGE_INTEGER f; QueryPerformanceFrequency(&f);
            for (;;) {
                Sleep(10000);
                LARGE_INTEGER a, b; QueryPerformanceCounter(&a);
                for (int i = 0; i < 20; i++) Sleep(1);
                QueryPerformanceCounter(&b);
                fprintf(stderr, "[sleep1] Sleep(1) real = %.2f ms (média de 20)\n",
                        (b.QuadPart - a.QuadPart) * 1000.0 / f.QuadPart / 20);
            }
        }, NULL, 0, NULL);
    SetUnhandledExceptionFilter(ydkj_crash_filter);
    AddVectoredExceptionHandler(0 /*last*/, [](EXCEPTION_POINTERS* ep)->LONG{
        if (ep->ExceptionRecord->ExceptionCode == 0xC00000FDu /*STACK_OVERFLOW*/) {
            fprintf(stderr,"\n[STACKOVERFLOW] infinite recursion detected; backtrace (RVAs):\n");
            HMODULE mod=GetModuleHandleA(0); void* fr[62]; USHORT n=RtlCaptureStackBackTrace(0,62,fr,0);
            for(USHORT i=0;i<n;i++) fprintf(stderr," %llX",(unsigned long long)((char*)fr[i]-(char*)mod));
            fprintf(stderr,"\n"); fflush(stderr); ExitProcess(7);
        }
        return EXCEPTION_CONTINUE_SEARCH; });
    { ULONG g=256*1024; SetThreadStackGuarantee(&g); }  /* reserve stack so the SO handler can run */
    signal(SIGABRT, ydkj_abort_handler);
    setvbuf(stdout, NULL, _IONBF, 0);   /* unbuffered: don't lose prints on kill */
#endif

    /* Flat VM: one host buffer, guest addr -> vm_base + addr. This maps the
     * FULL 32-bit guest space uniformly (page 0, the 0x60000000..0xD0000000
     * range, everything) -- which native-VA mapping can't on Windows, because
     * the OS reserves the low 64 KB and DLLs occupy parts of the mid range.
     * On real PS3 those addresses are RAM, and the game writes to them (its
     * null-object inits land on page 0); calloc backs them so the game runs.
     * HLE functions that take guest pointers must translate via vm_base /
     * vm_write* (which also byte-swap) -- a raw *guest_ptr would deref the host
     * buffer's offset incorrectly. */
#ifdef _WIN32
    /* Reserve the full 4 GB guest space; pages commit on first touch via the VEH. */
    AddVectoredExceptionHandler(1, guard_report_veh);  /* diagnose 0x80000001 silent deaths */
    AddVectoredExceptionHandler(1, vm_commit_veh);
    /* GOWA_TEX_WW (22/10, padrão 1): reserva com MEM_WRITE_WATCH -- o motor live pergunta ao
     * Windows quais páginas de uma textura foram escritas (GetWriteWatch) e pula o hash das
     * intactas (rsx_live_draw.c, ld_tex_paginas_sujas). =0 reserva como antes. */
    { const char* e = getenv("GOWA_TEX_WW");
      if (!(e && *e == '0')) {
          vm_base = (uint8_t*)VirtualAlloc(NULL, VM_SIZE, MEM_RESERVE | MEM_WRITE_WATCH, PAGE_READWRITE);
          if (vm_base) g_gowa_vm_ww = 1;
      }
      if (!vm_base) vm_base = (uint8_t*)VirtualAlloc(NULL, VM_SIZE, MEM_RESERVE, PAGE_READWRITE);
      fprintf(stderr, "[boot] memória do guest %s write-watch\n", g_gowa_vm_ww ? "com" : "sem"); }
    ppu_vm_size = 0;   /* full 32-bit space backed -> OOB guard unnecessary */
#else
    vm_base = (uint8_t*)calloc(1, 0xE0000000u);
    ppu_vm_size = 0xE0000000u;
#endif
    if (!vm_base) { printf("vm alloc failed\n"); return 1; }

    uint32_t entry = ppu_load_elf(elf);
    if (!entry) { printf("load failed\n"); return 1; }

    derive_vfs_root(elf);
    printf("[boot] VFS root: %s\n", ppu_vfs_root);

    /* Real title id, from the game's own PARAM.SFO. cellGame has been able to
     * read this since 2026-06-21, but only a title's own main() ever called it,
     * so every port that moved to this harness silently kept the BLES00000
     * placeholder -- and that id is what cellGame / cellSaveData / trophy build
     * their /dev_hdd0/game/<id> paths from, so saves landed in a directory
     * belonging to no title. Same shape as the live-draw engine before 1a050be:
     * a working facility with no caller in the shared harness. */
    {
        /* PARAM.SFO sits in a different place depending on how the VFS is rooted.
         * A disc-style root has it under PS3_GAME/; a title whose content is
         * opened by RELATIVE path needs the root AT USRDIR, and then PARAM.SFO is
         * one level up instead. Only the first layout was tried, so rooting at
         * USRDIR silently lost the title id and name -- which is how the window
         * caption ended up on its hardcoded fallback. Try each and take the
         * first that exists. */
        static const char* const kSfoLayouts[] = {
            "%s/PS3_GAME/PARAM.SFO",   /* vfs root = disc root      */
            "%s/../PARAM.SFO",         /* vfs root = .../USRDIR     */
            "%s/PARAM.SFO",            /* vfs root = the game dir   */
        };
        char sfo[1100];
        int found = 0;
        for (size_t i = 0; i < sizeof kSfoLayouts / sizeof kSfoLayouts[0]; i++) {
            snprintf(sfo, sizeof sfo, kSfoLayouts[i], ppu_vfs_root);
            FILE* f = fopen(sfo, "rb");
            if (f) { fclose(f); found = 1; break; }
        }
        if (!found)
            snprintf(sfo, sizeof sfo, "%s/PS3_GAME/PARAM.SFO", ppu_vfs_root);
#ifdef _WIN32
        /* GOWA (25/10): a disc copy with its USRDIR but without PS3_GAME/PARAM.SFO (an incomplete copy) is still the
         * game's data; the title id and name then come from an installed update's PARAM.SFO (<root>/game/<id>/,
         * the 1.04 the generator puts there), so the game does not fall back to BLES00000 and quit. */
        if (!found) {
            char usr[1100];
            snprintf(usr, sizeof usr, "%s/PS3_GAME/USRDIR", ppu_vfs_root);
            const DWORD a = GetFileAttributesA(usr);
            if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) {
                char pat[1100];
                snprintf(pat, sizeof pat, "%s/game/*", ppu_vfs_root);
                WIN32_FIND_DATAA fd;
                HANDLE h = FindFirstFileA(pat, &fd);
                if (h != INVALID_HANDLE_VALUE) {
                    do {
                        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.') continue;
                        char cand[1100];
                        snprintf(cand, sizeof cand, "%s/game/%s/PARAM.SFO", ppu_vfs_root, fd.cFileName);
                        FILE* f = fopen(cand, "rb");
                        if (!f) continue;
                        fclose(f);
                        snprintf(sfo, sizeof sfo, "%s", cand);
                        found = 1;
                        fprintf(stderr, "[boot] o disco nao tem PS3_GAME/PARAM.SFO; o titulo vem do update (%s)\n", cand);
                        break;
                    } while (FindNextFileA(h, &fd));
                    FindClose(h);
                }
            }
        }
        /* GOWA (25/10): no disc. The port does not copy the 34 GB disc: vfs\PS3_GAME is a junction to the folder
         * picked when the game was made, and when that folder is moved, deleted or on a drive that is off, the game
         * ran on without its data, found nothing and quit with no word (a player's "it opens and crashes"). Said
         * plainly now, with the way back: picking the disc again in the launcher remakes the junction. */
        if (!found) {
            fprintf(stderr, "[boot] disco do jogo nao encontrado (%s): a pasta do disco foi movida ou apagada\n", sfo);
            fflush(stderr);
            wchar_t msg[2400];
            swprintf(msg, sizeof msg / sizeof msg[0],
                     L"O jogo não achou o disco do God of War: Ascension.\n\n"
                     L"O port não copia o disco: ele usa a pasta do disco escolhida ao gerar o jogo, e essa pasta foi "
                     L"movida, apagada ou está num HD desligado.\n\n"
                     L"No Otavix Launcher, abra o God of War: Ascension e clique em \"Escolher o disco de novo\" "
                     L"(leva segundos; não precisa gerar o jogo de novo).\n\n"
                     L"The game could not find the God of War: Ascension disc: the disc folder picked when the game "
                     L"was made was moved, deleted or is on a drive that is off. In the Otavix Launcher, open the game "
                     L"and click \"Pick the disc again\".\n\n(%hs)", sfo);
            MessageBoxW(NULL, msg, L"God of War: Ascension", MB_ICONERROR | MB_OK | MB_SETFOREGROUND);
            ExitProcess(3);
        }
#endif
        cellGame_init_from_paramsfo(sfo);
#ifdef _WIN32
        /* GOWA (26/10): a disc of another region. The port is the US game (BCUS98232, with the 1.04 update the
         * generator installs); a European disc (BCES01741, report 31) made the game too, since the update comes
         * apart, and the game read its files, waited and quit with no word. Said plainly now. BLES00000 is
         * cellGame's default: the SFO was not read, and that case goes on as before. */
        {
            const char* id = cellGame_get_title_id();
            if (id && *id && strcmp(id, "BCUS98232") != 0 && strcmp(id, "BLES00000") != 0) {
                const bool eu = !strncmp(id, "BCES", 4) || !strncmp(id, "BLES", 4);
                const bool as = !strncmp(id, "BCAS", 4) || !strncmp(id, "BLAS", 4);
                const bool jp = !strncmp(id, "BCJS", 4) || !strncmp(id, "BLJS", 4) || !strncmp(id, "BCJM", 4) || !strncmp(id, "BLJM", 4);
                fprintf(stderr, "[boot] disco de outra regiao (%s): o port so funciona com o BCUS98232\n", id);
                fflush(stderr);
                wchar_t msg[2400];
                swprintf(msg, sizeof msg / sizeof msg[0],
                         L"Este disco é a versão %ls do God of War: Ascension (%hs).\n\n"
                         L"O port só funciona com a versão americana, BCUS98232: é dela o update 1.04 que o gerador usa, "
                         L"e os arquivos das outras versões são diferentes (o jogo abriria e fecharia logo).\n\n"
                         L"No Otavix Launcher, abra o God of War: Ascension, clique em \"Escolher o disco de novo\" e "
                         L"escolha uma cópia do disco americano (BCUS98232). Não precisa gerar o jogo de novo.\n\n"
                         L"This God of War: Ascension disc is %ls version (%hs). The port only works with the US "
                         L"version, BCUS98232, whose 1.04 update the generator uses. In the Otavix Launcher, open the "
                         L"game, click \"Pick the disc again\" and pick a copy of the US disc.",
                         eu ? L"europeia" : as ? L"asiática" : jp ? L"japonesa" : L"de outra região", id,
                         eu ? L"the European" : as ? L"the Asian" : jp ? L"the Japanese" : L"another region's", id);
                MessageBoxW(NULL, msg, L"God of War: Ascension", MB_ICONERROR | MB_OK | MB_SETFOREGROUND);
                ExitProcess(4);
            }
        }
        /* GOWA (29/10, reports 379 e 389): tela preta e o jogo fecha sozinho, sem desenhar nada. Faltava um arquivo
         * do disco (USRDIR/ARCHIVES num; no outro o CYN.PSARC abria e a leitura voltava 0 bytes). O gerador confere a
         * cópia (dados/disco_bcus98232.tsv), mas o disco pode mudar depois: pasta trocada, arquivo apagado pelo
         * antivírus, OneDrive que deixou só o atalho na nuvem. A mesma lista, conferida aqui a cada abertura: nome,
         * tamanho e os 4 primeiros bytes dos .PSARC ("PSAR"). GOWA_SEM_CONFERIR_DISCO=1 pula. */
        if (!getenv("GOWA_SEM_CONFERIR_DISCO")) {
            char lista[MAX_PATH * 2] = {0};
            {
                char exe[MAX_PATH]; const DWORD ne = GetModuleFileNameA(NULL, exe, MAX_PATH);
                char cand[2][MAX_PATH * 2]; int nc = 0;
                if (ne && ne < MAX_PATH) {   /* <raiz>\port\build_*\gowa.exe */
                    char* s = strrchr(exe, '\\'); if (s) *s = 0;
                    s = strrchr(exe, '\\'); if (s) *s = 0;
                    s = strrchr(exe, '\\'); if (s) *s = 0;
                    snprintf(cand[nc++], sizeof cand[0], "%s\\dados\\disco_bcus98232.tsv", exe);
                }
                snprintf(cand[nc++], sizeof cand[0], "%s/../dados/disco_bcus98232.tsv", ppu_vfs_root);
                for (int i = 0; i < nc && !lista[0]; i++)
                    if (GetFileAttributesA(cand[i]) != INVALID_FILE_ATTRIBUTES) snprintf(lista, sizeof lista, "%s", cand[i]);
            }
            FILE* fl = lista[0] ? fopen(lista, "rb") : NULL;
            if (fl) {
                wchar_t ruins[1600] = {0}; size_t nr = 0; int nruins = 0;
                char linha[512];
                while (fgets(linha, sizeof linha, fl)) {
                    char* tab = strchr(linha, '\t');
                    if (!tab) continue;
                    *tab = 0;
                    const unsigned long long tam = _strtoui64(tab + 1, NULL, 10);
                    char alvo[1400];
                    snprintf(alvo, sizeof alvo, "%s/PS3_GAME/%s", ppu_vfs_root, linha);
                    const wchar_t* motivo = NULL;
                    WIN32_FILE_ATTRIBUTE_DATA ad;
                    if (!GetFileAttributesExA(alvo, GetFileExInfoStandard, &ad)) motivo = L"falta / missing";
                    else if ((((unsigned long long)ad.nFileSizeHigh << 32) | ad.nFileSizeLow) != tam) motivo = L"tamanho errado / wrong size";
                    else if (ad.dwFileAttributes & (0x00400000 /* RECALL_ON_DATA_ACCESS */ | FILE_ATTRIBUTE_OFFLINE))
                        motivo = L"só na nuvem (OneDrive) / cloud only";
                    else if (strstr(linha, ".PSARC")) {
                        unsigned char m4[4] = {0}; DWORD lidos = 0;
                        HANDLE h = CreateFileA(alvo, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
                        if (h == INVALID_HANDLE_VALUE || !ReadFile(h, m4, 4, &lidos, NULL) || lidos != 4)
                            motivo = L"não se lê / unreadable";
                        else if (memcmp(m4, "PSAR", 4)) motivo = L"conteúdo errado ou criptografado / wrong or encrypted content";
                        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
                    }
                    if (!motivo) continue;
                    fprintf(stderr, "[disco] %s: %ls\n", alvo, motivo);
                    if (++nruins <= 6 && nr < sizeof ruins / sizeof ruins[0] - 200)
                        nr += swprintf(ruins + nr, sizeof ruins / sizeof ruins[0] - nr, L"  %hs (%ls)\n", linha, motivo);
                }
                fclose(fl);
                if (nruins) {
                    fflush(stderr);
                    wchar_t msg[3200];
                    swprintf(msg, sizeof msg / sizeof msg[0],
                             L"A cópia do disco do God of War: Ascension está incompleta (%d arquivo(s)):\n%ls%ls\n"
                             L"Sem eles o jogo fica com a tela preta e fecha. Extraia ou copie o disco de novo (confira o "
                             L"espaço livre, se o antivírus não apagou nada e, no OneDrive, use \"Sempre manter neste "
                             L"dispositivo\") e, no Otavix Launcher, clique em \"Escolher o disco de novo\".\n\n"
                             L"The God of War: Ascension disc copy is incomplete (files above). Without them the game "
                             L"stays on a black screen and closes. Extract or copy the disc again and, in the Otavix "
                             L"Launcher, click \"Pick the disc again\".\n\n(%hs/PS3_GAME)",
                             nruins, ruins, nruins > 6 ? L"  ...\n" : L"", ppu_vfs_root);
                    MessageBoxW(NULL, msg, L"God of War: Ascension", MB_ICONERROR | MB_OK | MB_SETFOREGROUND);
                    ExitProcess(5);
                }
            }
        }
#endif
    }

    fprintf(stderr,"[boot-dbg] before ppu_recomp_register\n"); fflush(stderr);
    ppu_recomp_register();   /* lifted function table -> address map */
    fprintf(stderr,"[boot-dbg] after ppu_recomp_register; before ps3_load_prx_modules\n"); fflush(stderr);
    ps3_load_prx_modules();  /* real system PRX (libsre) -> guest RAM + exports */
    fprintf(stderr,"[boot-dbg] after prx; before ppu_hle_init\n"); fflush(stderr);
    ppu_hle_init();          /* firmware import NID -> HLE handlers */
    ppu_sysprx_register();   /* boot-critical CRT (sys_initialize_tls, ...) */
    ppu_fs_register();       /* cellFs VFS over the real game directory */
    fprintf(stderr,"[boot-dbg] before lv2_init_syscalls\n"); fflush(stderr);
    lv2_init_syscalls();     /* real lv2 syscall table (semaphore/memory/fs/...) */
    gowa_offline_register(); /* GOWA: imports sem handler -> respostas "sem rede" */
    /* GOWA_CONF_REPLAY=<pasta>: reproduz os jobs de SPU gravados e sai (gowa_confere.cpp) */
    if (const char* cr = getenv("GOWA_CONF_REPLAY")) {
        return gowa_conf_replay(cr);
    }
    gowa_perf_jogo_liga();

    /* Two scalars that make a milestone diff interpretable: a lift that found
     * fewer functions, or a build that registered fewer HLE handlers, explains
     * a whole run going missing without anyone having to bisect the stream. */
    ps3_ms_kv("load:functions", (long long)function_table_count);
    ps3_ms_kv("load:hle_registered", (long long)ps3_hle_count());
    ps3_ms("boot:init_done");
    fprintf(stderr,"[boot-dbg] after lv2_init_syscalls\n"); fflush(stderr);

    /* Install the guest-callback hook and start the synthetic RSX vblank driver
     * so the game's frame loop advances (it no-ops until the game registers its
     * vblank/flip handlers during init). */
    g_ps3_guest_caller = harness_guest_caller;
    /* CreateThread is a pthread wrapper off Windows (win32_compat.h), so the
     * frame clock now runs everywhere. Without it a POSIX host never ticks
     * vblank or drains the FIFO, and the guest waits on fences forever. */
    { const char* vt = getenv("GOWA_VBLANK_THREAD");
      s_vblank_thread = !(vt && vt[0] == '0');
      if (s_vblank_thread) CreateThread(NULL, 4u * 1024 * 1024, gowa_vblank_thread, NULL, 0, NULL); }
    CreateThread(NULL, 4u * 1024 * 1024, vblank_ticker, NULL, 0, NULL);
    if (getenv("PS3_GUEST_PROF"))
        CreateThread(NULL, 0, guest_prof_thread, NULL, 0, NULL);
    if (getenv("GOWA_HOSTPROF"))
        CreateThread(NULL, 0, gowa_hostprof_thread, NULL, 0, NULL);
    if (getenv("GOWA_TL"))
        CreateThread(NULL, 0, gowa_tl_thread, (LPVOID)(uintptr_t)GetCurrentThreadId(), 0, NULL);
    /* GOWA_PIN: padrão ligado desde 20/10 (A/B no 9800X3D, 8 núcleos com SMT: cap. 16
     * +8,6%, fase 1 +8,3%); =0 desliga, =1 força.
     * 25/10: com ~9 threads pesadas, a fixação só ajuda com 8 núcleos físicos ou mais e
     * SMT. Num i5-10300H (4 núcleos / 8 lógicas), sem fixar: cap. 16 8,06 x 7,75 (+4%,
     * 3 pares). Num i5-11400F (6 núcleos / 12 lógicas, AVX-512), sem fixar: 32,80 / 32,54
     * / 32,78 x 30,41 / 31,34 / 31,23 (+5%, 3 pares, mesma carga de fundo). */
    { const char* pe = getenv("GOWA_PIN");
      SYSTEM_INFO si; GetSystemInfo(&si);
      const int nucleos = gowa_nucleos_fisicos();
      const int pin = pe ? (*pe != '0') : (nucleos >= 8 && (int)si.dwNumberOfProcessors > nucleos);
      { extern int g_gowa_pin_on; g_gowa_pin_on = pin; }   /* desempenho.json */
      if (pin) CreateThread(NULL, 0, gowa_pin_thread, NULL, 0, NULL);
      else if (!pe) fprintf(stderr, "[pin] desligado: %d núcleos físicos, %lu CPUs lógicas (fixa só com 8 núcleos ou mais e SMT; GOWA_PIN=1 força)\n",
                            nucleos, (unsigned long)si.dwNumberOfProcessors); }
    if (getenv("GOWA_GUESTPROF"))
        CreateThread(NULL, 0, gowa_guestprof_thread, NULL, 0, NULL);
    if (getenv("GOWA_GUARD"))
        CreateThread(NULL, 0, gowa_guard_thread, NULL, 0, NULL);
    if (getenv("GOWA_IMGGUARD"))
        CreateThread(NULL, 0, gowa_imgguard_thread, NULL, 0, NULL);
    if (getenv("GOWA_WGUARD"))
        CreateThread(NULL, 0, gowa_wguard_thread, NULL, 0, NULL);
#ifdef _WIN32
    CreateThread(NULL, 0, hang_watchdog, NULL, 0, NULL);
    if (const char* fz = getenv("GOWA_FREEZE_DUMP"))
        CreateThread(NULL, 0, freeze_watch, (LPVOID)(intptr_t)atoi(fz), 0, NULL);
    /* PS3_DEBUG=<file>: ask a running title what it is doing. */
    { static char dbgpath[1024];
      const char* dp = getenv("PS3_DEBUG");
      if (dp && *dp) { snprintf(dbgpath, sizeof dbgpath, "%s", dp);
                       CreateThread(NULL, 0, debug_console, dbgpath, 0, NULL); } }   /* tlhelp32-based */
#endif

    ps3_ms("boot:entry");
#ifdef _WIN32
    { extern void ps3_sampler_start(void); ps3_sampler_start(); } /* PS3_SAMPLE=<ms> */
#endif
    /* PS3_MAIN_STACK_LV2=1: give the main thread its stack where lv2 does, in
     * the 0xD0000000 stack region (the first allocation there, which is also
     * what sys_ppu_thread_get_stack_information already reports for it).
     * Guest code can tell: Tornado Outbreak's SPU memcpy (func_000D60E0) copies
     * on the PPU when the destination shares the stack's top nibble, then waits
     * for an SPU reply only the SPU path sends -- with the stack at 0x0FF00000,
     * every low address "is on the stack" and the boot hangs. Opt-in until the
     * regression gate has run with it on. */
    uint32_t stack_top = STACK_TOP;
    { const char* ms = getenv("PS3_MAIN_STACK_LV2");
      if (ms && *ms && *ms != '0') {
          uint32_t base = vm_stack_allocate(&g_vm_stack_alloc, VM_PPU_STACK_SIZE);
          if (base) stack_top = base + VM_PPU_STACK_SIZE;
      } }
    printf("\n[boot] dispatching entry OPD 0x%08X (stack top 0x%08X)\n\n", entry, stack_top);
#ifdef _WIN32
    fprintf(stderr, "[boot] MAIN guest thread tid=%lu\n", (unsigned long)GetCurrentThreadId());
    g_gowa_main_tid = GetCurrentThreadId();
    if (getenv("GOWA_PRIO_AB")) CreateThread(NULL, 0, gowa_prio_ab_thread, NULL, 0, NULL);
#endif
    int rc = ppu_run(entry, stack_top);
    printf("\n[boot] ppu_run returned %d (entry function unwound)\n", rc);
    /* A guest that called sys_process_exit never reaches this line: that path
     * ends in the host exit(). Getting here means the entry function returned
     * instead, so hand back the status the guest published if it published one
     * -- returning a hardcoded 0 reported success for a run that never got
     * anywhere, which is exactly the kind of green a boot harness must not
     * produce. */
    return g_sys_process_exit_called ? (int)g_sys_process_exit_code : 0;
}
