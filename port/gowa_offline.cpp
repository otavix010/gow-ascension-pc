/*
 * GOWA: cobertura de imports e respostas "sem rede / sem conta".
 *
 * O objetivo do port é jogar offline. Para as bibliotecas de rede, PSN, voz,
 * HTTP/SSL e EULA, o que importa é que cada import que o jogo chama responda
 * como um PS3 desconectado, em vez de cair no default do runtime para NID sem
 * handler (r3 = 0, "sucesso"), que faz o jogo seguir com handles e tickets
 * inexistentes.
 *
 * gowa_offline_register() roda depois de ppu_hle_init/ppu_sysprx_register (ver
 * main.cpp), então ps3_hle_has() já sabe o que o runtime implementa. Só os
 * NIDs SEM handler (nem HLE, nem libsre LLE) ganham um handler daqui; o que o
 * runtime já implementa fica como está.
 *
 * GOWA_IMPORTS=1 lista, no boot, todo import que continua sem handler.
 */
#include "ppu_recomp.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
void     ps3_hle_register_ctx(uint32_t nid, const char* name, void (*fn)(ppu_context*));
int      ps3_hle_has(uint32_t nid);
uint32_t prx_resolve_export(uint32_t nid);
}

#include "gowa_imports.inc"

#define N_IMPORTS (sizeof(g_gowa_imports) / sizeof(g_gowa_imports[0]))

/* Uma página de rascunho no espaço do guest para ponteiros que o jogo
 * desreferencia (h_errno). Fica entre o topo da pilha principal (0x0FF00000)
 * e 0x10000000, região que o runtime não usa. */
static const uint32_t GOWA_SCRATCH = 0x0FFF0000u;

static void set_r3(ppu_context* ctx, uint32_t v) { ctx->gpr[3] = (uint64_t)(int64_t)(int32_t)v; }

/* ---- respostas por família ------------------------------------------- */
/* Códigos: os mesmos que o RPCS3 usa quando o console está offline. */
#define SYS_NET_FAIL                    0xFFFFFFFFu  /* -1; errno fica a cargo do _sys_net_errno_loc */
#define SCE_NP_ERROR_INVALID_STATE      0x8002AA0Au
#define SCE_NP_COMMUNITY_ERROR_NOT_INIT 0x8002A101u
#define SCE_NP_COMMERCE2_ERROR_NOT_INIT 0x80023001u
#define CELL_NET_CTL_ERROR_NOT_CONNECTED 0x80130106u
#define CELL_HTTP_ERROR_NOT_INITIALIZED 0x80710001u
#define CELL_SSL_ERROR_NOT_INITIALIZED  0x80740001u
#define CELL_VOICE_ERROR_NOT_INIT       0x80310801u
#define CELL_SYSUTIL_ERROR_NOT_SUPPORTED 0x8002B10Au

static void h_net_fail(ppu_context* ctx)    { set_r3(ctx, SYS_NET_FAIL); }
static void h_np_offline(ppu_context* ctx)  { set_r3(ctx, SCE_NP_ERROR_INVALID_STATE); }
static void h_tus_offline(ppu_context* ctx) { set_r3(ctx, SCE_NP_COMMUNITY_ERROR_NOT_INIT); }
static void h_com2_offline(ppu_context* ctx){ set_r3(ctx, SCE_NP_COMMERCE2_ERROR_NOT_INIT); }
static void h_netctl_off(ppu_context* ctx)  { set_r3(ctx, CELL_NET_CTL_ERROR_NOT_CONNECTED); }
static void h_http_off(ppu_context* ctx)    { set_r3(ctx, CELL_HTTP_ERROR_NOT_INITIALIZED); }
static void h_ssl_off(ppu_context* ctx)     { set_r3(ctx, CELL_SSL_ERROR_NOT_INITIALIZED); }
static void h_voice_off(ppu_context* ctx)   { set_r3(ctx, CELL_VOICE_ERROR_NOT_INIT); }
static void h_eula_off(ppu_context* ctx)    { set_r3(ctx, CELL_SYSUTIL_ERROR_NOT_SUPPORTED); }
static void h_ok(ppu_context* ctx)          { set_r3(ctx, 0); }

/* _sys_net_h_errno_loc: devolve um ponteiro válido (o jogo escreve/lê nele). */
static void h_h_errno_loc(ppu_context* ctx)
{
    ctx->gpr[3] = GOWA_SCRATCH + 0x10;
}

/* inet_addr(const char*): conversão de verdade, sem rede envolvida. */
static void h_inet_addr(ppu_context* ctx)
{
    uint32_t p = (uint32_t)ctx->gpr[3];
    char s[32] = {0};
    for (int i = 0; i < 31; i++) { s[i] = (char)vm_read8(p + i); if (!s[i]) break; }
    unsigned a, b, c, d;
    char extra;
    uint32_t r = 0xFFFFFFFFu;   /* INADDR_NONE */
    if (sscanf(s, "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) == 4 && a < 256 && b < 256 && c < 256 && d < 256)
        r = (a << 24) | (b << 16) | (c << 8) | d;   /* ordem de rede = big-endian = valor do guest */
    ctx->gpr[3] = r;
}

/* cellVoiceCreateNotifyEventQueue(u32* id, u64* key): o runtime não tem; a
 * família cellVoice devolvia NOT_INIT e o jogo registrava "cellVoiceCreate-
 * NotifyEventQueue failed" ao entrar no menu. O RPCS3 cria a fila e devolve OK
 * (log de um PC de teste, 31:56). Aqui: uma fila lv2 de verdade com chave própria. */
extern "C" uint32_t sys_event_queue_create_direct(uint64_t key, int32_t size);
static void h_voice_create_queue(ppu_context* ctx)
{
    const uint32_t id_ea = (uint32_t)ctx->gpr[3], key_ea = (uint32_t)ctx->gpr[4];
    static uint64_t next_key = 0x80004D494F320001ull;
    const uint64_t k = next_key++;
    const uint32_t qid = sys_event_queue_create_direct(k, 64);
    if (!qid || !id_ea || !key_ea) { set_r3(ctx, 0x80310805u); return; }   /* CELL_VOICE_ERROR_RESOURCE_INSUFFICIENT */
    vm_write32(id_ea, qid);
    vm_write32(key_ea, (uint32_t)(k >> 32));
    vm_write32(key_ea + 4, (uint32_t)k);
    fprintf(stderr, "[cellVoice] CreateNotifyEventQueue -> id=%u key=0x%llX\n", qid, (unsigned long long)k);
    set_r3(ctx, 0);
}

typedef void (*hfn)(ppu_context*);

/* Handler específico por NID (tem prioridade sobre o da família). */
static hfn handler_especifico(uint32_t nid)
{
    switch (nid) {
    case 0xC9157D30u: return h_h_errno_loc;      /* _sys_net_h_errno_loc */
    case 0xDABBC2C0u: return h_inet_addr;        /* inet_addr */
    case 0x8CCF05EDu:                            /* sys_net_abort_resolver */
    case 0x8D1B77FBu: return h_ok;               /* sys_net_abort_socket */
    case 0xCE4374F6u: return h_ok;               /* cellGamePatchCheck: sem patch pendente */
    case 0x61B2BADEu: return h_ok;               /* cellHttpEndCookie */
    case 0x48BD97C7u: return h_ok;               /* sceNpTrophyAbortHandle */
    case 0x2A01013Eu: return h_voice_create_queue; /* cellVoiceCreateNotifyEventQueue */
    case 0x35D84910u:                            /* cellVoiceSetNotifyEventQueue */
    case 0xDD000886u:                            /* cellVoiceRemoveNotifyEventQueue */
    case 0xFF0FA43Au: return h_ok;               /* cellVoiceResetPort */
    case 0x2E162A62u:                            /* sceNpTusDestroyTitleCtx */
    case 0x44ECA8B4u: return h_ok;               /* sceNpTusDestroyTransactionCtx */
    default: return nullptr;
    }
}

static hfn handler_familia(const char* lib)
{
    if (!strcmp(lib, "sys_net"))            return h_net_fail;
    if (!strcmp(lib, "sceNpTus"))           return h_tus_offline;
    if (!strcmp(lib, "sceNpCommerce2"))     return h_com2_offline;
    if (!strncmp(lib, "sceNp", 5))          return h_np_offline;   /* sceNp, sceNp2, sceNpTrophy... */
    if (!strcmp(lib, "cellNetCtl"))         return h_netctl_off;
    if (!strncmp(lib, "cellHttp", 8))       return h_http_off;     /* cellHttp, cellHttpUtil */
    if (!strcmp(lib, "cellSsl"))            return h_ssl_off;
    if (!strcmp(lib, "cellVoice"))          return h_voice_off;
    if (!strcmp(lib, "cellSysutilNpEula"))  return h_eula_off;
    return nullptr;
}

#include "win32_compat.h"
#include <timeapi.h>
#pragma comment(lib, "winmm")
/* GOWA_SLOW_JOBMOD=<EA do módulo de job, hex>:<µs> (experimento de caminho
 * crítico): gira esse tempo a cada execução da entrada do módulo (o gancho é
 * inserido à mão na função de entrada traduzida, ex. 0x72DF80 em 0xAD98). Se
 * o FPS cair quase na proporção do tempo somado, o módulo está no caminho
 * crítico do quadro. GOWA_JOBMOD_COUNT=1: conta as execuções (log a cada 5 s). */
extern "C" void gowa_slow_jobmod(unsigned ea)
{
    static long us = -1; static unsigned want, count_ea; static double qpc_us; static int count;
    if (us < 0) {
        const char* e = getenv("GOWA_SLOW_JOBMOD");
        long u = 0;
        if (e) { want = (unsigned)strtoul(e, nullptr, 16); const char* c = strchr(e, ':'); u = c ? atol(c + 1) : 0; }
        /* GOWA_JOBMOD_COUNT=1: all hooks together; =<EA hex>: only that module */
        { const char* c = getenv("GOWA_JOBMOD_COUNT"); count = c ? 1 : 0; count_ea = c ? (unsigned)strtoul(c, nullptr, 16) : 0; if (count_ea == 1) count_ea = 0; }
        LARGE_INTEGER f; QueryPerformanceFrequency(&f); qpc_us = (double)f.QuadPart / 1e6;
        us = u;
    }
    if (count && (!count_ea || ea == count_ea)) {
        static volatile long n; static volatile LONGLONG t0;
        const long k = InterlockedIncrement(&n);
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        if (!t0) t0 = now.QuadPart;
        if ((now.QuadPart - t0) > (LONGLONG)(5e6 * qpc_us) && InterlockedExchange(&n, 0)) {
            fprintf(stderr, "[jobmod] 0x%X: %ld execucoes em %.1f s\n", ea, k, (now.QuadPart - t0) / (qpc_us * 1e6));
            t0 = now.QuadPart;
        }
    }
    if (us <= 0 || ea != want) return;
    LARGE_INTEGER a, b; QueryPerformanceCounter(&a);
    const LONGLONG until = a.QuadPart + (LONGLONG)(us * qpc_us);
    do { YieldProcessor(); QueryPerformanceCounter(&b); } while (b.QuadPart < until);
}
/* Build de checagem do achatamento preguiçoso (GOWA_SPU_CRU_CHECK): uma gravação
 * na LS levou o veneno 0x007BAD5A de um resultado cru -- a análise de
 * scripts/flush_preguicoso.py deixou cru um valor que sai do float. Imprime o
 * RVA de retorno (dentro da função SPU traduzida; resolver com o .map). */
#include <intrin.h>
extern "C" __declspec(noinline) void spu_cru_check_store(void* ctx, uint32_t lsa, const void* val)
{
    (void)ctx;
    static volatile long n;
    const long k = InterlockedIncrement(&n);
    if (k > 40) return;
    const uint32_t* w = (const uint32_t*)val;
    const uintptr_t base = (uintptr_t)GetModuleHandleA(nullptr);
    fprintf(stderr, "[cru-check] #%ld LS 0x%05X = %08X %08X %08X %08X  rva=0x%llX\n", k, lsa,
            w[0], w[1], w[2], w[3], (unsigned long long)((uintptr_t)_ReturnAddress() - base));
}
/* GOWA_FNDUMP=<N>[:<início s>] (21/10, reescrita nativa do job 0x72DF80): nas N
 * primeiras chamadas de uma função SPU traduzida (a partir de <início s> segundos
 * depois da primeira), grava registradores (128 x 16 B) + LS inteira (0x40000 B,
 * no formato do ls_store) antes da chamada (fase 0) e depois do dreno que a
 * completa (fase 1) em out/fndump/<fn>_<k>_in|out.bin -- o contrato de entrada e
 * saída da função. O par entrada/saída é casado por thread. Só em builds de teste
 * (a chamada é posta à mão no ponto de chamada). */
#include "spu_context.h"   /* GOWA_JOBDMA: lê registradores e a LS do contexto */
/* GOWA_EDGE_NOCULL=1 (21/10, experimento; scripts/nocull_remendo.py): o 0x72DF80 pula o
 * descarte de triângulos onde o próprio jogo permite (o caminho do bit 4096 das flags do
 * desenho) -- a GPU descarta. Lido uma vez na partida. */
extern "C" { volatile int g_gowa_jogando = 0; }   /* 25/10: ver o gancho do log do jogo */
extern "C" { int g_gowa_edge_nocull = [] { const char* e = getenv("GOWA_EDGE_NOCULL"); return e ? atoi(e) : 0; }(); }
/* GOWA_PULA_LUZ=1: o 0x72DF80 pula a iluminação por vértice (0x73E0 + 6AC8/7040) -- experimento de
 * teto do EDGE na GPU (opção (a)): a luz sai errada na tela, o resto do job igual. */
extern "C" { int g_gowa_pula_luz = [] { const char* e = getenv("GOWA_PULA_LUZ"); return e ? atoi(e) : 0; }(); }

/* GOWA_LUZ_GPU (22/10, EDGE na GPU, opção (a); padrão 1): na 0x73E0, logo depois do preparo das luzes
 * (gancho antes do ninho), guarda as luzes do segmento no anel de rsx_edge_luz.h, marca o
 * atributo 7 (o arranjo A) de cada vértice iluminado com (GOWA_LUZ_MAGICO, id, gen & 0xFFFF) e
 * devolve 1 -- a tradução pula as contas e a 0x6AC8 (vai a 0x8210). O motor live refaz a luz
 * no vertex shader. Fica na SPU (devolve 0) quando o segmento usa a 2a passada da 0x6AC8
 * (pilha +0x40) ou a 0x7040 (pilha +0x50 & 0x10000), ou tem luzes demais. */
#include "rsx_edge_luz.h"
/* padrão LIGADO desde 22/10 (A/B ablg2 cap. 16 +2,0%, fase 1 empate, imagem igual); =0 volta à SPU */
static int s_luz_gpu_init = [] { const char* e = getenv("GOWA_LUZ_GPU"); g_gowa_luz_gpu = e ? atoi(e) : 1; return 0; }();
extern "C" int gowa_luz_gpu(void* vctx)
{
    spu_context* ctx = (spu_context*)vctx;
    const uint8_t* ls = ctx->ls;
    auto be32 = [ls](uint32_t a) -> uint32_t { a &= 0x3FFFCu;
        return (uint32_t)ls[a] << 24 | (uint32_t)ls[a + 1] << 16 | (uint32_t)ls[a + 2] << 8 | ls[a + 3]; };
    auto bef = [&](uint32_t a) -> float { const uint32_t u = be32(a); float f; memcpy(&f, &u, 4); return f; };
    /* contagem a cada 5 s: segmentos na GPU e os que ficaram na SPU, por motivo */
    static volatile long s_ok, s_7040, s_pass2, s_mais, s_perdido;
    { static volatile LONGLONG t0; LARGE_INTEGER now, f; QueryPerformanceCounter(&now); QueryPerformanceFrequency(&f);
      const LONGLONG old = t0;
      if (!old) t0 = now.QuadPart;
      else if (now.QuadPart - old > 5 * f.QuadPart && InterlockedCompareExchange64(&t0, now.QuadPart, old) == old)
          fprintf(stderr, "[luzgpu-spu] 5 s: na GPU %ld (nunca vistos pelo motor live %ld), na SPU por 7040 %ld, por 2a passada %ld, por contagem %ld\n",
                  InterlockedExchange(&s_ok, 0), InterlockedExchange(&s_perdido, 0), InterlockedExchange(&s_7040, 0), InterlockedExchange(&s_pass2, 0), InterlockedExchange(&s_mais, 0)); }
    const uint32_t sp = ctx->gpr[1]._u32[0];
    const uint32_t s50 = be32(sp + 0x50), s40 = be32(sp + 0x40);
    if (s50 & 0x10000u) { InterlockedIncrement(&s_7040); return 0; }
    if (s40) { InterlockedIncrement(&s_pass2); return 0; }
    const uint32_t n = ctx->gpr[107]._u32[0];
    const uint32_t hdr = ctx->gpr[115]._u32[0];
    const uint32_t c0 = be32(hdr + 0x10), c1 = be32(hdr + 0x14), c2 = be32(hdr + 0x18);
    const uint32_t nl = c0 + c1, tot = nl + c2;
    if (!n || n > 4096 || tot > GOWA_LUZ_MAX || c0 > GOWA_LUZ_MAX || c1 > GOWA_LUZ_MAX || c2 > GOWA_LUZ_MAX) { InterlockedIncrement(&s_mais); return 0; }
    InterlockedIncrement(&s_ok);
    const uint32_t lp = hdr + be32(hdr), lc = hdr + be32(hdr + 4), ld = hdr + be32(hdr + 8);
    const uint32_t g = (uint32_t)InterlockedIncrement(&g_gowa_luz_prox);
    const uint32_t id = g & (GOWA_LUZ_ANEL - 1u);
    gowa_luz_slot* sl = &g_gowa_luz_anel[id];
    if (sl->gen && sl->visto != sl->gen) InterlockedIncrement(&s_perdido);   /* o motor live nunca viu o anterior */
    gowa_luz_set* s = &sl->s;
    s->info[0] = (float)c0; s->info[1] = (float)nl; s->info[2] = (float)c2; s->info[3] = (s50 & 256u) ? 0.5f : 0.0f;
    s->info2[0] = ctx->gpr[93]._f32[0]; s->info2[1] = s->info2[2] = s->info2[3] = 0.0f;
    for (uint32_t l = 0; l < tot; l++)
        for (int k = 0; k < 4; k++) {
            s->p[l][k] = bef(lp + 16u * l + 4u * k);
            s->c[l][k] = bef(lc + 16u * l + 4u * k);
            s->d[l][k] = l < c0 ? bef(ld + 16u * l + 4u * k) : 0.0f;
        }
    _ReadWriteBarrier();
    sl->gen = g;
    /* a marca no A de TODOS os vértices do segmento (float3 big-endian; o w fica): o motor live
     * lê o elemento 0 antes de buscar os vértices. r122 palavra 0 = vértices arredondados a 8,
     * limitado ao tamanho do arranjo A (até o B, r91). */
    const uint32_t A = ctx->gpr[92]._u32[0], B = ctx->gpr[91]._u32[0];
    uint32_t nv = ctx->gpr[122]._u32[0];
    if (B > A && nv > (B - A) / 16u) nv = (B - A) / 16u;
    const float fid = (float)id, fck = (float)(g & 0xFFFFu);
    uint32_t wid, wck; memcpy(&wid, &fid, 4); memcpy(&wck, &fck, 4);
    const uint32_t w3[3] = { _byteswap_ulong(GOWA_LUZ_MAGICO), _byteswap_ulong(wid), _byteswap_ulong(wck) };
    for (uint32_t k = 0; k < nv; k++)
        memcpy(ctx->ls + ((A + 16u * k) & 0x3FFF0u), w3, 12);
    return 1;
}

/* GOWA_JOBDMA=<início s>:<jobs> (21/10, mapa de entradas e saídas do 0x72DF80): a partir de
 * <início s>, os <jobs> primeiros jobs do módulo (gancho gowa_job_inicio na entrada 0xAD98)
 * têm cada DMA da thread anotado em out/jobdma_<pid>.txt até o próximo job daquela thread --
 * com a imagem de quem pediu (o PM e os plugins usam a mesma thread), o comando, LS, EA,
 * tamanho, os elementos das listas e os primeiros bytes da origem. Linhas:
 *   J <job> tid=<tid> img=<img> r3..r8=<palavra 0 de cada>
 *   D <job> img=<img> cmd=<cmd> lsa=<lsa> ea=<ea> size=<size> tag=<tag> src=<32 bytes hex>
 *   L <job>   elem ea=<ea> size=<size> (lista) */
extern "C" { int g_gowa_jobdma_on = 0; }
static FILE* s_jd_f;
static volatile LONG s_jd_jobs;
static int s_jd_max;
static ULONGLONG s_jd_t0, s_jd_ini;
static thread_local int t_jd_job = -1;
static thread_local unsigned t_jd_n;
static int s_jd_init = [] {
    const char* e = getenv("GOWA_JOBDMA");
    if (!e || !*e) return 0;
    s_jd_ini = (ULONGLONG)(atof(e) * 1000.0);
    const char* c = strchr(e, ':');
    s_jd_max = c ? atoi(c + 1) : 20;
    char p[MAX_PATH];
    snprintf(p, sizeof p, "out/jobdma_%lu.txt", (unsigned long)GetCurrentProcessId());
    s_jd_f = fopen(p, "w");
    if (s_jd_f) { setvbuf(s_jd_f, nullptr, _IOFBF, 1 << 20); g_gowa_jobdma_on = 1; }
    s_jd_t0 = GetTickCount64();   /* relógio a partir da partida do processo */
    return 1;
}();
/* cada registro vai num único fwrite (várias threads escrevem no mesmo arquivo) */
static void jd_hex(char* b, int* p, int cap, const uint8_t* s, uint32_t n)
{
    static const char H[] = "0123456789ABCDEF";
    for (uint32_t i = 0; i < n && *p < cap - 3; i++) { b[(*p)++] = H[s[i] >> 4]; b[(*p)++] = H[s[i] & 15]; }
}
extern "C" void gowa_job_inicio(void* vctx)
{
    if (!g_gowa_jobdma_on) return;
    spu_context* ctx = (spu_context*)vctx;
    const ULONGLONG agora = GetTickCount64();
    if (agora - s_jd_t0 < s_jd_ini || s_jd_jobs >= s_jd_max) { t_jd_job = -1; return; }
    const int k = (int)InterlockedIncrement(&s_jd_jobs) - 1;
    if (k >= s_jd_max) { t_jd_job = -1; if (k == s_jd_max) fflush(s_jd_f); return; }
    t_jd_job = k; t_jd_n = 0;
    char b[512]; int p = 0;
    p += snprintf(b + p, sizeof b - p, "J %d tid=%lu img=%d", k, (unsigned long)GetCurrentThreadId(), (int)ctx->image_id);
    for (int r = 3; r <= 8; r++) p += snprintf(b + p, sizeof b - p, " r%d=%08X", r, ctx->gpr[r]._u32[0]);
    b[p++] = '\n';
    fwrite(b, 1, (size_t)p, s_jd_f);
    fflush(s_jd_f);   /* o processo é morto no fim da execução */
}
/* GOWA (22/10, EDGE na GPU): entrada de um estágio do 0x72DF80 (gancho inserido por
 * scripts/tracar_estagios.py) nos jobs que o GOWA_JOBDMA está registrando:
 *   F <job> <fn> r0=<link> r1=<sp> r3..r8=<palavra 0>
 * GOWA_FNTRACE_LS=<fn>,<fn>... (hex): também grava a LS inteira + os 128 registradores na
 * entrada dessas funções nos 3 primeiros jobs registrados, em out/fntrace/<fn>_j<job>_<n>.bin
 * (2 KB de registradores + 256 KB de LS). */
extern "C" void gowa_fntrace(void* vctx, unsigned fn)
{
    if (t_jd_job < 0 || t_jd_n >= 20000) return;
    spu_context* ctx = (spu_context*)vctx;
    char b[256]; int p = 0;
    p += snprintf(b + p, sizeof b - p, "F %d %04X r0=%05X r1=%05X", t_jd_job, fn, ctx->gpr[0]._u32[0] & 0x3FFFFu, ctx->gpr[1]._u32[0] & 0x3FFFFu);
    for (int r = 3; r <= 8; r++) p += snprintf(b + p, sizeof b - p, " r%d=%08X", r, ctx->gpr[r]._u32[0]);
    b[p++] = '\n';
    fwrite(b, 1, (size_t)p, s_jd_f);
    t_jd_n++;
    static int s_nls = -1; static unsigned s_ls[32];
    /* GOWA_FNTRACE_LS=* : LS em TODA entrada de estágio, só no job GOWA_FNTRACE_JOB (padrão 0),
     * numeradas na ordem: out/fntrace/seq_<n>_<fn>.bin */
    static int s_all = -1, s_job = 0;
    if (s_all < 0) {
        const char* e = getenv("GOWA_FNTRACE_LS");
        s_all = (e && *e == '*') ? 1 : 0;
        const char* j = getenv("GOWA_FNTRACE_JOB"); s_job = j ? atoi(j) : 0;
        if (s_all) CreateDirectoryA("out/fntrace", nullptr);
    }
    if (s_all) {
        if (t_jd_job != s_job) return;
        static int s_seq = 0;
        char path[MAX_PATH];
        snprintf(path, sizeof path, "out/fntrace/seq_%03d_%04X.bin", s_seq++, fn);
        FILE* f = fopen(path, "wb");
        if (f) { fwrite(ctx->gpr, 16, 128, f); fwrite(ctx->ls_store, 1, 0x40000, f); fclose(f); }
        return;
    }
    if (s_nls < 0) {
        s_nls = 0;
        const char* e = getenv("GOWA_FNTRACE_LS");
        while (e && *e && s_nls < 32) { char* q; s_ls[s_nls++] = (unsigned)strtoul(e, &q, 16); if (q == e) break; e = (*q == ',') ? q + 1 : q; }
        if (s_nls) CreateDirectoryA("out/fntrace", nullptr);
    }
    /* GOWA_FNTRACE_LS_N=<n> (padrão 6): ocorrências gravadas por função, em qualquer job
     * registrado com binários (GOWA_JOBDMA_BIN) */
    static int s_lsn = -1; static int s_binjobs = -1;
    if (s_lsn < 0) { const char* e = getenv("GOWA_FNTRACE_LS_N"); s_lsn = e ? atoi(e) : 6; }
    if (s_binjobs < 0) { const char* e = getenv("GOWA_JOBDMA_BIN"); s_binjobs = e ? (atoi(e) > 1 ? atoi(e) : 3) : 3; }
    if (t_jd_job >= s_binjobs) return;
    for (int i = 0; i < s_nls; i++) {
        if (s_ls[i] != fn) continue;
        static volatile LONG s_cnt[32];
        const LONG c = InterlockedIncrement(&s_cnt[i]) - 1;
        if (c >= s_lsn) return;
        char path[MAX_PATH];
        snprintf(path, sizeof path, "out/fntrace/%04X_j%d_%d.bin", fn, t_jd_job, (int)c);
        FILE* f = fopen(path, "wb");
        if (f) { fwrite(ctx->gpr, 16, 128, f); fwrite(ctx->ls_store, 1, 0x40000, f); fclose(f); }
    }
}
extern "C" void gowa_jobdma_note(void* vctx, uint32_t cmd, uint32_t lsa, uint64_t ea, uint32_t size, uint32_t tag)
{
    if (t_jd_job < 0 || t_jd_n >= 20000) return;
    spu_context* ctx = (spu_context*)vctx;
    char* b = (char*)malloc(65536);
    if (!b) return;
    const int cap = 65536;
    int p = snprintf(b, cap, "D %d img=%d cmd=0x%02X lsa=0x%05X ea=0x%09llX size=0x%X tag=%u pc=0x%05X src=", t_jd_job, (int)ctx->image_id,
                     cmd, lsa & 0x3FFFF, (unsigned long long)ea, size, tag, (unsigned)(ctx->pc & 0x3FFFF));
    /* origem: GET lê a memória principal, PUT lê a LS; descritores pequenos inteiros */
    const int is_get = (cmd & 0xF0) == 0x40;
    const int is_list = (cmd & 0xF4) == 0x24 || (cmd & 0xF4) == 0x44;
    if (is_list) {
        b[p++] = '-'; b[p++] = '\n';
        const uint32_t la = (uint32_t)ea & 0x3FFF0u;
        uint32_t dst = lsa & 0x3FFFF;
        for (uint32_t i = 0; i < size / 8 && i < 512 && p < cap - 256; i++) {
            const uint8_t* q = ctx->ls_store + ((la + 8 * i) & 0x3FFFFu);
            const uint32_t sz = ((uint32_t)q[2] << 8) | q[3];
            const uint32_t eal = ((uint32_t)q[4] << 24) | ((uint32_t)q[5] << 16) | ((uint32_t)q[6] << 8) | q[7];
            p += snprintf(b + p, cap - p, "L %d   elem ea=0x%08X size=0x%X lsa=0x%05X src=", t_jd_job, eal, sz, dst);
            if (sz && vm_base && eal < 0xF0000000u) jd_hex(b, &p, cap, vm_base + eal, sz < 32 ? sz : 32);
            b[p++] = '\n';
            dst = (dst + sz + 15) & ~15u;   /* GETL: cada elemento segue o anterior, alinhado a 16 */
        }
    } else {
        const uint8_t* s = nullptr;
        if (is_get) { if (vm_base && (uint32_t)ea < 0xF0000000u) s = vm_base + (uint32_t)ea; }
        else s = ctx->ls_store + (lsa & 0x3FFF0u);
        if (s) jd_hex(b, &p, cap, s, size <= 0x200 ? size : 64);
        b[p++] = '\n';
    }
    fwrite(b, 1, (size_t)p, s_jd_f);
    if ((++t_jd_n & 63) == 0) fflush(s_jd_f);
    free(b);
    /* GOWA_JOBDMA_BIN=1 (22/10): nos 3 primeiros jobs registrados, os bytes inteiros de cada
     * transferência (GET: a origem na memória principal; PUT: a origem na LS; lista: cada
     * elemento) em out/jobdma_bin/j<job>_<seq>_<cmd>_<ea>_<lsa>.bin */
    /* GOWA_JOBDMA_BIN=<n jobs> (1 = 3) */
    static int s_bin = -1;
    if (s_bin < 0) { const char* e = getenv("GOWA_JOBDMA_BIN"); s_bin = e ? (atoi(e) > 1 ? atoi(e) : 3) : 0; if (s_bin) CreateDirectoryA("out/jobdma_bin", nullptr); }
    if (s_bin && t_jd_job < s_bin && vm_base) {
        static thread_local int t_seq = 0; static thread_local int t_seq_job = -1;
        if (t_seq_job != t_jd_job) { t_seq = 0; t_seq_job = t_jd_job; }
        auto grava = [&](uint32_t e_ea, uint32_t e_lsa, uint32_t n, const uint8_t* src) {
            char path[MAX_PATH];
            snprintf(path, sizeof path, "out/jobdma_bin/j%d_%04d_%02X_%08X_%05X.bin", t_jd_job, t_seq++, cmd, e_ea, e_lsa);
            FILE* f = fopen(path, "wb");
            if (f) { fwrite(src, 1, n, f); fclose(f); }
        };
        if (is_list) {
            const uint32_t la = (uint32_t)ea & 0x3FFF0u;
            uint32_t dst = lsa & 0x3FFFF;
            for (uint32_t i = 0; i < size / 8 && i < 512; i++) {
                const uint8_t* q = ctx->ls_store + ((la + 8 * i) & 0x3FFFFu);
                const uint32_t sz = ((uint32_t)q[2] << 8) | q[3];
                const uint32_t eal = ((uint32_t)q[4] << 24) | ((uint32_t)q[5] << 16) | ((uint32_t)q[6] << 8) | q[7];
                if (sz && eal < 0xF0000000u) {
                    if (is_get) grava(eal, dst, sz, vm_base + eal);
                    else grava(eal, dst, sz, ctx->ls_store + (dst & 0x3FFF0u));
                }
                dst = (dst + sz + 15) & ~15u;
            }
        } else if (size && size <= 0x40000) {
            if (is_get) { if ((uint32_t)ea < 0xF0000000u) grava((uint32_t)ea, lsa & 0x3FFFF, size, vm_base + (uint32_t)ea); }
            else grava((uint32_t)ea, lsa & 0x3FFFF, size, ctx->ls_store + (lsa & 0x3FFF0u));
        }
    }
}

extern "C" __declspec(noinline) void gowa_fndump(const void* gpr, const void* ls, unsigned fn, int fase)
{
    static int lim = -1; static ULONGLONG t_ini, t0;
    if (lim < 0) {
        const char* e = getenv("GOWA_FNDUMP");
        lim = e ? atoi(e) : 0;
        const char* c = e ? strchr(e, ':') : nullptr;
        t_ini = c ? (ULONGLONG)(atof(c + 1) * 1000.0) : 0;
        CreateDirectoryA("out/fndump", nullptr);
    }
    if (!lim) return;
    static volatile LONG n;
    static thread_local int t_k = -1;
    int k;
    if (fase == 0) {
        const ULONGLONG agora = GetTickCount64();
        if (!t0) t0 = agora;
        if (agora - t0 < t_ini || n >= lim) { t_k = -1; return; }
        k = (int)InterlockedIncrement(&n) - 1;
        if (k >= lim) { t_k = -1; return; }
        t_k = k;
    } else {
        k = t_k; t_k = -1;
        if (k < 0) return;
    }
    char path[MAX_PATH];
    snprintf(path, sizeof path, "out/fndump/%04X_%03d_%s.bin", fn, k, fase ? "out" : "in");
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fwrite(gpr, 16, 128, f);
    fwrite(ls, 1, 0x40000, f);
    fclose(f);
}
/* GOWA_CNTWATCH=<ea1>,<ea2>:<início s>:<duração s> (hex; ex. 9C66E8,9C66FC:205:3):
 * lê os dois u32 do guest em laço apertado durante a janela e grava cada
 * mudança com o tempo em µs (até 64 mil); no fim imprime as primeiras 400.
 * Ocupa uma thread do host durante a janela. Para medir a latência das tarefas
 * de taskset que a thread principal espera em func_003CC6B4. */
static DWORD WINAPI cntwatch_thread(LPVOID)
{
    const char* e = getenv("GOWA_CNTWATCH");
    uint32_t a1 = (uint32_t)strtoul(e, nullptr, 16), a2 = 0;
    unsigned st = 200, du = 3;
    if (const char* c = strchr(e, ',')) a2 = (uint32_t)strtoul(c + 1, nullptr, 16);
    if (const char* c = strchr(e, ':')) { st = (unsigned)strtoul(c + 1, nullptr, 10);
        if (const char* d = strchr(c + 1, ':')) du = (unsigned)strtoul(d + 1, nullptr, 10); }
    Sleep(st * 1000u);
    LARGE_INTEGER fq, t0, t; QueryPerformanceFrequency(&fq); QueryPerformanceCounter(&t0);
    struct Ev { uint32_t us, v1, v2; };
    static Ev ev[65536]; int n = 0;
    uint32_t p1 = vm_read32(a1), p2 = vm_read32(a2);
    ev[n++] = { 0, p1, p2 };
    for (;;) {
        QueryPerformanceCounter(&t);
        const uint64_t us = (uint64_t)(t.QuadPart - t0.QuadPart) * 1000000ull / (uint64_t)fq.QuadPart;
        if (us >= du * 1000000ull || n >= 65536) break;
        const uint32_t v1 = vm_read32(a1), v2 = vm_read32(a2);
        if (v1 != p1 || v2 != p2) { ev[n++] = { (uint32_t)us, v1, v2 }; p1 = v1; p2 = v2; }
        _mm_pause();
    }
    fprintf(stderr, "[cntwatch] %d mudanças de 0x%08X/0x%08X em %u s\n", n, a1, a2, du);
    for (int i = 0; i < n && i < 400; i++)
        fprintf(stderr, "[cntwatch]   t=%7u us  %08X=%u  %08X=%u\n", ev[i].us, a1, ev[i].v1, a2, ev[i].v2);
    return 0;
}

/* GOWA_SPURS_MON=<ea hex>: a cada 2 s, o estado da instância CellSpurs (layout do
 * RPCS3 cellSpurs.h): readyCount, contenção atual/pendente, estado de cada
 * workload, wklEnabled, mensagens do system service. Responde "o kernel SPU está
 * escalonando os workloads?" sem mexer no runtime. */
static DWORD WINAPI spurs_mon(LPVOID p)
{
    uint32_t s = (uint32_t)(uintptr_t)p;
    for (int it = 0;; it++) {
        Sleep(2000);
        char l1[256], l2[256], l3[256], l4[256];
        int a = 0, b = 0, c = 0, d = 0;
        for (int w = 0; w < 16; w++) {
            a += snprintf(l1 + a, sizeof l1 - a, "%02X%02X ", vm_read8(s + 0x00 + w), vm_read8(s + 0x10 + w));
            b += snprintf(l2 + b, sizeof l2 - b, "%02X%02X ", vm_read8(s + 0x20 + w), vm_read8(s + 0x30 + w));
            c += snprintf(l3 + c, sizeof l3 - c, "%02X%02X ", vm_read8(s + 0x80 + w), vm_read8(s + 0x90 + w));
            d += snprintf(l4 + d, sizeof l4 - d, "%02X ", vm_read8(s + 0x50 + w));
        }
        fprintf(stderr, "[spursmon] t=%ds enabled=%08X sig1=%04X sig2=%04X sysmsg=%02X idling=%02X flags1=%02X nspu=%02X recv=%02X handler d/w/e=%08X/%08X/%08X\n",
                2 * (it + 1), vm_read32(s + 0xB0), vm_read16(s + 0x70), vm_read16(s + 0x78), vm_read8(s + 0x72),
                vm_read8(s + 0x73), vm_read8(s + 0x74), vm_read8(s + 0x76), vm_read8(s + 0x77),
                vm_read32(s + 0xD64), vm_read32(s + 0xD68), vm_read32(s + 0xD6C));
        fprintf(stderr, "[spursmon]   ready1|ready2 : %s\n", l1);
        fprintf(stderr, "[spursmon]   cur|pend      : %s\n", l2);
        fprintf(stderr, "[spursmon]   state1|state2 : %s\n", l3);
        fprintf(stderr, "[spursmon]   maxcont       : %s\n", l4);
        /* GOWA_MEMMON=ea:len[,ea:len...] (hex): hexdump periódico de objetos
         * do guest (filas, event flags) junto com o estado do SPURS. */
        if (const char* mm = getenv("GOWA_MEMMON")) {
            char buf[256]; strncpy(buf, mm, sizeof buf - 1); buf[sizeof buf - 1] = 0;
            for (char* tok = strtok(buf, ","); tok; tok = strtok(nullptr, ",")) {
                uint32_t ea = (uint32_t)strtoul(tok, nullptr, 16), len = 0x40;
                if (const char* c = strchr(tok, ':')) len = (uint32_t)strtoul(c + 1, nullptr, 16);
                for (uint32_t o = 0; o < len; o += 32) {
                    char ln[160]; int p = snprintf(ln, sizeof ln, "[memmon]   %08X:", ea + o);
                    for (uint32_t k = 0; k < 32 && o + k < len; k += 4)
                        p += snprintf(ln + p, sizeof ln - p, " %08X", vm_read32(ea + o + k));
                    fprintf(stderr, "%s\n", ln);
                }
            }
        }
        /* GOWA_DUMPMEM=ea:len[,ea:len] (hex): grava a região em
         * out/mem_<ea>_t<seg>.bin a cada passagem (ex.: o anel do FIFO). */
        if (const char* dm = getenv("GOWA_DUMPMEM")) {
            char buf[256]; strncpy(buf, dm, sizeof buf - 1); buf[sizeof buf - 1] = 0;
            for (char* tok = strtok(buf, ","); tok; tok = strtok(nullptr, ",")) {
                uint32_t ea = (uint32_t)strtoul(tok, nullptr, 16), len = 0x1000;
                if (const char* c = strchr(tok, ':')) len = (uint32_t)strtoul(c + 1, nullptr, 16);
                char fn[256];
                snprintf(fn, sizeof fn, "out/mem_%08X_t%03d.bin", ea, 2 * (it + 1));
                if (FILE* f = fopen(fn, "wb")) { fwrite(vm_base + ea, 1, len, f); fclose(f); }
            }
        }
        /* GOWA_DUMPQ=<dir>: a cada passagem, grava em <dir> a fila do inflate
         * (12 entradas de 0x20 em 0x15E67B00) e, para cada entrada de saída de
         * 64 KB, o buffer de saída -- para conferir offline contra zlib. */
        if (const char* dq = getenv("GOWA_DUMPQ")) {
            char fn[512];
            snprintf(fn, sizeof fn, "%s/fila_t%03d.bin", dq, 2 * (it + 1));
            if (FILE* f = fopen(fn, "wb")) {
                for (uint32_t o = 0; o < 12 * 0x20; o++) { uint8_t b = vm_read8(0x15E67B00u + o); fwrite(&b, 1, 1, f); }
                fclose(f);
            }
            for (int e = 0; e < 12; e++) {
                uint32_t q = 0x15E67B00u + e * 0x20, dst = vm_read32(q + 4), dsz = vm_read32(q + 12);
                if (dsz != 0x10000u || !dst) continue;
                snprintf(fn, sizeof fn, "%s/buf_t%03d_%08X.bin", dq, 2 * (it + 1), dst);
                if (FILE* f = fopen(fn, "wb")) { fwrite(vm_base + dst, 1, 0x10000, f); fclose(f); }
            }
        }
        /* Tasksets (wid 1..5 com PM de taskset): CellSpursTaskset (layout do
         * RPCS3) = bitsets u128 running@0 ready@0x10 pending@0x20 enabled@0x30
         * signalled@0x40 waiting@0x50 (task 0 = bit mais alto). */
        for (int w = 0; w < 16; w++) {
            uint32_t wi = s + 0xB00 + w * 0x20;
            if ((uint32_t)vm_read64(wi) != 0x30023680u) continue;
            uint32_t ts = (uint32_t)vm_read64(wi + 8);
            fprintf(stderr, "[spursmon]   taskset wid %d @0x%08X run=%08X ready=%08X pend=%08X ena=%08X sig=%08X wait=%08X last=%02X\n",
                    w, ts, vm_read32(ts), vm_read32(ts + 0x10), vm_read32(ts + 0x20), vm_read32(ts + 0x30),
                    vm_read32(ts + 0x40), vm_read32(ts + 0x50), vm_read8(ts + 0x73));
        }
        if (it == 0 || it == 7 || it == 12) {
            /* wklInfo1[16] em +0xB00 e wklInfo2[16] em +0x1000 (SPURS2), 0x20 cada:
             * u64 addr (policy module), u64 arg, u32 size, u8 uniqueId, ... */
            for (int w = 0; w < 32; w++) {
                uint32_t wi = s + (w < 16 ? 0xB00 + w * 0x20 : 0x1000 + (w - 16) * 0x20);
                uint32_t pm = (uint32_t)vm_read64(wi), arg = (uint32_t)vm_read64(wi + 8), sz = vm_read32(wi + 0x10);
                if (!pm && !sz) continue;
                fprintf(stderr, "[spursmon]   wid %2d: pm=0x%08X size=0x%05X arg=0x%08X uid=%02X prio=%08X%08X\n",
                        w, pm, sz, arg, vm_read8(wi + 0x14), vm_read32(wi + 0x18), vm_read32(wi + 0x1C));
            }
        }
    }
    return 0;
}

/* GOWA_PEEK=ea,ea,... (hex, até 16 palavras de 32 bits): imprime os valores a
 * cada 5 s -- a evolução de um contador (ex.: blocos do heap de VRAM). */
static DWORD WINAPI peek_thread(LPVOID)
{
    uint32_t ea[16]; int n = 0;
    char buf[256]; strncpy(buf, getenv("GOWA_PEEK"), sizeof buf - 1); buf[sizeof buf - 1] = 0;
    for (char* tok = strtok(buf, ","); tok && n < 16; tok = strtok(nullptr, ","))
        ea[n++] = (uint32_t)strtoul(tok, nullptr, 16);
    ULONGLONG t0 = GetTickCount64();
    for (;;) {
        Sleep(5000);
        char o[512]; int p = 0;
        for (int i = 0; i < n; i++) p += snprintf(o + p, sizeof o - p, " %08X=%08X", ea[i], vm_read32(ea[i]));
        fprintf(stderr, "[peek] t=%llus%s\n", (GetTickCount64() - t0) / 1000, o);
    }
    return 0;
}

/* GOWA_WATCH=ea[:len],... (hex, len <= 8): a cada ~1 ms compara os bytes e
 * registra cada mudança com o tempo -- quem zera um readyCount, quando um
 * flag muda. */
static DWORD WINAPI watch_thread(LPVOID)
{
    struct { uint32_t ea, len; uint8_t v[8]; } w[16]; int n = 0;
    char buf[256]; strncpy(buf, getenv("GOWA_WATCH"), sizeof buf - 1); buf[sizeof buf - 1] = 0;
    for (char* tok = strtok(buf, ","); tok && n < 16; tok = strtok(nullptr, ",")) {
        w[n].ea = (uint32_t)strtoul(tok, nullptr, 16); w[n].len = 1;
        if (const char* c = strchr(tok, ':')) w[n].len = (uint32_t)strtoul(c + 1, nullptr, 16);
        if (w[n].len > 8) w[n].len = 8;
        for (uint32_t k = 0; k < w[n].len; k++) w[n].v[k] = vm_read8(w[n].ea + k);
        n++;
    }
    ULONGLONG t0 = GetTickCount64(); long total = 0;
    timeBeginPeriod(1);
    for (;;) {
        Sleep(1);
        for (int i = 0; i < n; i++) {
            uint8_t nv[8]; int mud = 0;
            for (uint32_t k = 0; k < w[i].len; k++) { nv[k] = vm_read8(w[i].ea + k); mud |= nv[k] != w[i].v[k]; }
            if (!mud) continue;
            if (total++ < 4000) {
                char a[24], b[24]; int pa = 0, pb = 0;
                for (uint32_t k = 0; k < w[i].len; k++) { pa += snprintf(a + pa, 4, "%02X", w[i].v[k]); pb += snprintf(b + pb, 4, "%02X", nv[k]); }
                fprintf(stderr, "[watch] t=%llums %08X: %s -> %s\n", GetTickCount64() - t0, w[i].ea, a, b);
            }
            memcpy(w[i].v, nv, w[i].len);
        }
    }
    return 0;
}


/* GOWA_SPU_SAMPLER=1: a cada 1 ms anota, por SPU, o workload corrente
 * (wklCurrentId) e se o SPU está no interpretador; a cada 5 s imprime a
 * fração de tempo por wid. Responde "onde vai o tempo de SPU". */
extern "C" int gowa_spu_sample(int s, unsigned* wid, unsigned* pc, int* interp);
extern "C" const uint8_t* gowa_spu_ls(int s);
extern "C" uint8_t* vm_base;
/* Qual módulo de job WWS está no buffer de código (LS 0x4000) e contém pc:
 * compara o começo do LS com a imagem do módulo na memória. 0 = nenhum. */
static uint32_t sampler_jobmod(int s, unsigned pc)
{
    static const struct { uint32_t va, bytes; } mods[] = { { 0x006FC080u, 0x14180u }, { 0x007B6680u, 0x20BC0u } };
    const uint8_t* ls = gowa_spu_ls(s);
    if (!ls || !vm_base) return 0;
    for (const auto& m : mods)
        if (pc >= 0x4000u && pc < 0x4000u + m.bytes && memcmp(ls + 0x4000, vm_base + m.va, 0x40) == 0) return m.va;
    return 0;
}
/* Amostras interpretadas por (wid, região): pc < 0x4000 = código do policy
 * module / kernel; senão a "impressão" dos primeiros 0x40 bytes do buffer de
 * código de job (LS 0x4000). Na impressão, procura esses bytes na memória
 * principal para dar o EA de origem (o módulo carregado). */
struct SampKey { unsigned wid; uint32_t fp; unsigned n; uint32_t va; int searched; unsigned pcs[4]; unsigned pcn[4]; uint8_t pcb[4][32]; };
/* EA na memória principal (0..256 MB) onde estão os 32 bytes b; 0 se não achar.
 * Cache dos últimos resultados para não varrer de novo a cada impressão. */
static uint32_t sampler_find(const uint8_t* b)
{
    static struct { uint8_t k[32]; uint32_t ea; } c[64]; static int cn = 0;
    for (int i = 0; i < cn; i++) if (memcmp(c[i].k, b, 32) == 0) return c[i].ea;
    uint32_t r = 0;
    if (vm_base) for (uint32_t ea = 0x10000u; ea + 32u < 0x10000000u; ea += 4u)
        if (vm_base[ea] == b[0] && memcmp(vm_base + ea, b, 32) == 0) { r = ea; break; }
    if (cn < 64) { memcpy(c[cn].k, b, 32); c[cn].ea = r; cn++; }
    return r;
}
static SampKey s_sk[128];
static uint32_t sampler_fp(const uint8_t* ls)
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < 0x40; i++) h = (h ^ ls[0x4000 + i]) * 16777619u;
    return h | 1u;
}
static void sampler_note(unsigned wid, const uint8_t* ls, unsigned pc)
{
    const uint32_t fp = pc < 0x4000u ? 0u : sampler_fp(ls);
    for (auto& k : s_sk) {
        if (k.n && (k.wid != wid || k.fp != fp)) continue;
        if (!k.n) { k.wid = wid; k.fp = fp; k.va = 0; k.searched = 0; memset(k.pcn, 0, sizeof k.pcn);
                    if (fp && vm_base) {     /* EA de origem, uma vez por chave */
                        for (uint32_t ea = 0x10000u; ea + 0x40u < 0x02000000u; ea += 0x10u)
                            if (vm_base[ea] == ls[0x4000] && memcmp(vm_base + ea, ls + 0x4000, 0x40) == 0) { k.va = ea; break; }
                        k.searched = 1; } }
        k.n++;
        const unsigned b = pc & ~0xFFu;
        int f = -1; for (int i = 0; i < 4; i++) if (k.pcn[i] && k.pcs[i] == b) { f = i; break; }
        if (f < 0) { for (int i = 0; i < 4; i++) if (!k.pcn[i]) { f = i; break; } }
        if (f >= 0) { if (!k.pcn[f]) memcpy(k.pcb[f], ls + (b < 0x3FFE0u ? b : 0x3FFE0u), 32);
                      k.pcs[f] = b; k.pcn[f]++; }
        return;
    }
}
static DWORD WINAPI sampler_thread(LPVOID)
{
    static unsigned hm[3];        /* interpretado: 6FC080, 7B6680, outro */
    static unsigned h[8][33][2];
    static unsigned hpm[8][32];   /* pc < 0x4000 (policy module) per workload */
    static unsigned hp[0x4000];
    static unsigned hi[0x4000];   /* interpreted samples, any wid < 32, by pc/16 */
    static unsigned hn[0x4000];   /* native samples, any wid < 32, by pc/16 (last dispatched pc) */
    timeBeginPeriod(1);
    ULONGLONG t0 = GetTickCount64(), last = t0;
    for (;;) {
        Sleep(1);
        for (int s = 0; s < 8; s++) {
            unsigned wid, pc; int in;
            if (!gowa_spu_sample(s, &wid, &pc, &in)) continue;
            h[s][wid < 32 ? wid : 32][in ? 1 : 0]++;
            /* GOWA: per workload, samples whose pc is below LS 0x4000 -- the
             * policy module (job manager), not a job: polling for work */
            if (wid < 32 && pc < 0x4000u) hpm[s][wid]++;
            if (wid >= 32) hp[(pc >> 4) & 0x3FFF]++;
            else if (in) { hi[(pc >> 4) & 0x3FFF]++;
                           const uint32_t m = sampler_jobmod(s, pc);
                           hm[m == 0x006FC080u ? 0 : m == 0x007B6680u ? 1 : 2]++;
                           if (const uint8_t* ls = gowa_spu_ls(s)) sampler_note(wid, ls, pc); }
            /* GOWA_SAMPLER_WID=<n>: o histograma de pcs nativos só desse workload */
            else { static int s_wf = -2; if (s_wf == -2) { const char* e = getenv("GOWA_SAMPLER_WID"); s_wf = e ? atoi(e) : -1; }
                   if (s_wf < 0 || wid == (unsigned)s_wf) hn[(pc >> 4) & 0x3FFF]++; }
        }
        ULONGLONG now = GetTickCount64();
        if (now - last >= 5000) {
            last = now;
            /* uma vez: nome de cada wid (wklH1/wklH2 da instância SPURS, cujo EA
             * é o u64 em LS 0x1C0 do contexto do kernel) */
            { static int s_named = 0;
              for (int s = 0; s < 8 && !s_named && vm_base; s++) {
                  const uint8_t* ls = gowa_spu_ls(s);
                  if (!ls) continue;
                  const uint32_t sp = (uint32_t)ls[0x1C4] << 24 | (uint32_t)ls[0x1C5] << 16 | (uint32_t)ls[0x1C6] << 8 | ls[0x1C7];
                  if (sp < 0x10000u || sp >= 0x40000000u) continue;
                  s_named = 1;
                  char b[1600]; int p = 0;
                  for (unsigned w = 0; w < 32; w++) {
                      const uint32_t hh = sp + (w < 16 ? 0xE00u + w * 0x10u : 0x1A00u + (w - 16) * 0x10u);
                      const uint32_t nc = vm_read32(hh + 4), ni = vm_read32(hh + 12);
                      if (!nc && !ni) continue;
                      p += snprintf(b + p, sizeof b - p, " w%u=%s/%s", w,
                                    nc && nc < 0x40000000u ? (const char*)(vm_base + nc) : "-",
                                    ni && ni < 0x40000000u ? (const char*)(vm_base + ni) : "-");
                      if (p > (int)sizeof b - 80) break;
                  }
                  fprintf(stderr, "[sampler] spurs=0x%08X nomes:%s\n", sp, b);
                  s_named = (int)sp;
              }
              /* prioridades atuais (wklInfo[wid].priority[8], +0x18), a cada
               * impressão: o balanceador do jogo as muda o tempo todo */
              if (s_named > 1 && vm_base) {
                  const uint32_t sp = (uint32_t)s_named;
                  char b[1200]; int p = 0;
                  for (unsigned w = 0; w < 23; w++) {
                      const uint32_t wi = sp + (w < 16 ? 0xB00u + w * 0x20u : 0x1000u + (w - 16) * 0x20u) + 0x18u;
                      p += snprintf(b + p, sizeof b - p, " w%u=%02X%02X%02X%02X%02X%02X%02X", w,
                                    vm_base[wi], vm_base[wi + 1], vm_base[wi + 2], vm_base[wi + 3],
                                    vm_base[wi + 4], vm_base[wi + 5], vm_base[wi + 6]);
                  }
                  fprintf(stderr, "[sampler] prio:%s\n", b);
              }
              /* GOWA_SAMPLER_DUMP=<ea hex>[,<bytes>]: essa memória a cada impressão */
              { static int s_d = -1; static uint32_t s_dea, s_dn;
                if (s_d < 0) { const char* e = getenv("GOWA_SAMPLER_DUMP"); s_d = e ? 1 : 0;
                               if (e) { s_dea = (uint32_t)strtoul(e, nullptr, 16); const char* c = strchr(e, ',');
                                        s_dn = c ? (uint32_t)strtoul(c + 1, nullptr, 0) : 0x80u; if (s_dn > 0x200u) s_dn = 0x200u; } }
                if (s_d && vm_base) {
                    char b[1400]; int p = 0;
                    for (uint32_t o = 0; o < s_dn; o += 4) p += snprintf(b + p, sizeof b - p, "%s%08X", (o & 0xF) ? " " : (o ? " | " : ""), vm_read32(s_dea + o));
                    fprintf(stderr, "[sampler] mem 0x%08X: %s\n", s_dea, b);
                } } }
            for (int s = 0; s < 8; s++) {
                unsigned tot = 0; for (int w = 0; w < 33; w++) tot += h[s][w][0] + h[s][w][1];
                if (!tot) continue;
                char b[600]; int p = 0;
                for (int w = 0; w < 33; w++) {
                    unsigned n = h[s][w][0] + h[s][w][1];
                    if (n * 100 >= tot) p += snprintf(b + p, sizeof b - p, " w%d:%u%%(i%u%%,pm%u%%)", w, n * 100 / tot, h[s][w][1] * 100 / tot,
                                                      w < 32 ? hpm[s][w] * 100 / tot : 0u);
                }
                fprintf(stderr, "[sampler] t=%llus spu%d%s\n", (now - t0) / 1000, s, b);
            }
            { char b[400]; int p = 0;
              for (int k = 0; k < 10; k++) { unsigned best = 0, bi = 0;
                  for (unsigned i = 0; i < 0x4000; i++) if (hp[i] > best) { best = hp[i]; bi = i; }
                  if (!best) break; p += snprintf(b + p, sizeof b - p, " %05X:%u", bi << 4, best); hp[bi] = 0; }
              fprintf(stderr, "[sampler] sysservice pcs:%s\n", b); }
            { char b[600]; int p = 0; unsigned tot = 0;
              for (unsigned i = 0; i < 0x4000; i++) tot += hi[i];
              for (int k = 0; k < 16; k++) { unsigned best = 0, bi = 0;
                  for (unsigned i = 0; i < 0x4000; i++) if (hi[i] > best) { best = hi[i]; bi = i; }
                  if (!best) break; p += snprintf(b + p, sizeof b - p, " %05X:%u", bi << 4, best); hi[bi] = 0; }
              fprintf(stderr, "[sampler] interp pcs (tot %u):%s\n", tot, b); }
            memset(hi, 0, sizeof hi);
            { char b[600]; int p = 0; unsigned tot = 0;
              for (unsigned i = 0; i < 0x4000; i++) tot += hn[i];
              for (int k = 0; k < 12; k++) { unsigned best = 0, bi = 0;
                  for (unsigned i = 0; i < 0x4000; i++) if (hn[i] > best) { best = hn[i]; bi = i; }
                  if (!best) break; p += snprintf(b + p, sizeof b - p, " %05X:%u", bi << 4, best); hn[bi] = 0; }
              fprintf(stderr, "[sampler] native pcs (tot %u):%s\n", tot, b); }
            { const unsigned t = hm[0] + hm[1] + hm[2];
              if (t) fprintf(stderr, "[sampler] interp por modulo: 6FC080=%u%% 7B6680=%u%% outro=%u%% (tot %u)\n",
                             hm[0] * 100 / t, hm[1] * 100 / t, hm[2] * 100 / t, t);
              memset(hm, 0, sizeof hm); }
            for (int r = 0; r < 10; r++) {
                SampKey* b = NULL;
                for (auto& k : s_sk) if (k.n && (!b || k.n > b->n)) b = &k;
                if (!b) break;
                fprintf(stderr, "[sampler]   interp w%u %s EA=0x%08X: %u  (pcs %05X:%u %05X:%u %05X:%u %05X:%u)\n", b->wid,
                        b->fp ? "jobbuf" : "pm<4000", b->va, b->n, b->pcs[0], b->pcn[0], b->pcs[1], b->pcn[1],
                        b->pcs[2], b->pcn[2], b->pcs[3], b->pcn[3]);
                if (b->fp && b->n > 50) {   /* de onde vem o código fora do módulo */
                    char o[400]; int p = 0;
                    for (int i = 0; i < 4; i++) if (b->pcn[i]) {
                        const uint32_t ea = sampler_find(b->pcb[i]);
                        p += snprintf(o + p, sizeof o - p, " LS%05X->EA%08X", b->pcs[i], ea);
                    }
                    fprintf(stderr, "[sampler]     origem:%s\n", o);
                }
                b->n = 0;
            }
            /* once, from 60 s: the SPURS instance seen by SPU 0's kernel (LS
             * 0x1C4 = SpursKernelContext.spurs, low word) -- nSpus and, per workload,
             * the per-SPU priorities and min/max contention */
            { static int done;
              const uint8_t* ls = gowa_spu_ls(0);
              if (done < 2 && now - t0 >= (done ? 200000u : 60000u) && ls && vm_base) {
                  done++;
                  const uint32_t se = ((uint32_t)ls[0x1C4] << 24) | ((uint32_t)ls[0x1C5] << 16) |
                                      ((uint32_t)ls[0x1C6] << 8) | ls[0x1C7];
                  const uint8_t* sp = vm_base + se;
                  fprintf(stderr, "[spurs] ea=0x%08X nSpus=%u spuIdling=%02X flags1=%02X\n",
                          se, sp[0x76], sp[0x73], sp[0x74]);
                  for (int w = 0; w < 32; w++) {
                      const uint8_t* wi = sp + (w < 16 ? 0xB00 + w * 0x20 : 0x1000 + (w - 16) * 0x20);
                      const uint32_t addr = ((uint32_t)wi[4] << 24) | ((uint32_t)wi[5] << 16) | ((uint32_t)wi[6] << 8) | wi[7];
                      if (!addr) continue;
                      const uint8_t mx = w < 16 ? (sp[0x50 + w] & 0xF) : (sp[0x50 + w - 16] >> 4);
                      const uint8_t mn = sp[0x40 + (w & 15)];
                      fprintf(stderr, "[spurs]  w%-2d pm=0x%08X prio=%02X %02X %02X %02X %02X %02X %02X %02X min=%u max=%u\n",
                              w, addr, wi[0x18], wi[0x19], wi[0x1A], wi[0x1B], wi[0x1C], wi[0x1D], wi[0x1E], wi[0x1F],
                              mn, mx);
                  }
              } }
            memset(s_sk, 0, sizeof s_sk);
            memset(hn, 0, sizeof hn);
            memset(hp, 0, sizeof hp);
            memset(h, 0, sizeof h); memset(hpm, 0, sizeof hpm);
        }
    }
    return 0;
}

/* sys_mmapper_map_memory / sys_mmapper_unmap_memory. O scaffold responde OK sem
 * fazer nada; o GOWA (gerenciador de memória em páginas de 1 MB, ~200 blocos)
 * faz unmap(addr, &mem_id) e usa o id devolvido. Aqui: tabela endereço -> id,
 * unmap devolve o id, e um map de um bloco num endereço diferente do último
 * copia o conteúdo (no PS3 é a mesma memória física mudando de endereço).
 * Tamanho do bloco: ps3_mmapper_block_size() do scaffold. */
extern "C" uint32_t ps3_mmapper_block_size(uint32_t mem_id);
extern "C" uint8_t* vm_base;
#define GOWA_MM_MAX 1024
static struct { uint32_t id, addr, last_addr; int mapped; } s_mm[GOWA_MM_MAX];
static int s_mm_n = 0;
static int mm_find_id(uint32_t id)
{
    for (int i = 0; i < s_mm_n; i++) if (s_mm[i].id == id) return i;
    if (s_mm_n >= GOWA_MM_MAX) return -1;
    s_mm[s_mm_n].id = id; s_mm[s_mm_n].addr = 0; s_mm[s_mm_n].last_addr = 0; s_mm[s_mm_n].mapped = 0;
    return s_mm_n++;
}
static void h_mmapper_map(ppu_context* ctx)
{
    uint32_t addr = (uint32_t)ctx->gpr[3], id = (uint32_t)ctx->gpr[4];
    int i = mm_find_id(id);
    if (i >= 0) {
        uint32_t sz = ps3_mmapper_block_size(id);
        if (s_mm[i].last_addr && s_mm[i].last_addr != addr && sz && vm_base) {
            memmove(vm_base + addr, vm_base + s_mm[i].last_addr, sz);
            static int n = 0; if (n++ < 8)
                fprintf(stderr, "[gowa-mm] mem_id 0x%X remapeado 0x%08X -> 0x%08X (%u KB copiados)\n",
                        id, s_mm[i].last_addr, addr, sz >> 10);
        }
        s_mm[i].addr = addr; s_mm[i].mapped = 1;
    }
    ctx->gpr[3] = 0;
}
static void h_mmapper_unmap(ppu_context* ctx)
{
    uint32_t addr = (uint32_t)ctx->gpr[3], out = (uint32_t)ctx->gpr[4];
    for (int i = 0; i < s_mm_n; i++)
        if (s_mm[i].mapped && s_mm[i].addr == addr) {
            s_mm[i].mapped = 0; s_mm[i].last_addr = addr;
            if (out) vm_write32(out, s_mm[i].id);
            ctx->gpr[3] = 0;
            return;
        }
    ctx->gpr[3] = (uint64_t)(int64_t)(int32_t)0x80010002u;   /* EINVAL: nada mapeado ali */
}

/* Tarefas do taskset "wid 3" (5 tarefas, ELF em 0x6C8500 = spu_8e4f...): o PM
 * de taskset real (libsre, interpretado) faz GET do segmento de código
 * (EA 0x6C8600, 0x9900 bytes) para o LS 0x3000. Registrar a tradução dessa
 * imagem como região residente faz o runtime usar o código nativo quando o PM
 * desvia para a tarefa -- antes ela era 100% interpretada, lenta a ponto de os
 * sinais do job WWS (wid 22) se acumularem e se perderem (deadlock do frame).
 * GOWA_NO_TASKLIFT=1 desliga. */
#define GOWA_IMG_TASK_8E4F 600001
extern "C" void spu_begin_image(int image_id);
extern "C" void spu_overlay_register_region(uint32_t content_ea, uint32_t span, int image_id);
extern "C" void spu_8e4f213617fb468dc09ae1a03a0b333b6f22ccbb4c24ee36d3a0e11c0a94b4d8_spu_recomp_register(void);
/* Módulos de job WWS (C0DEC0DE do GOWA.elf), traduzidos por
 * scripts/lift_gowa_jobmods.py: cada um vira uma imagem e uma região residente
 * pelo endereço de origem -- o GET que leva o módulo para o buffer de código
 * de job (LS 0x4000) troca a interpretação pelo código nativo.
 * GOWA_NO_JOBLIFT=1 desliga; GOWA_JOBLIFT_SKIP=va,va (hex) pula módulos. */
#include <unordered_map>
#include "gowa_jobmods.inc"
#include "gowa_jobmods_hot.inc"
#include "gowa_plugins.inc"
#include "gowa_plugins_w11.inc"
#include "gowa_tspm_hot.inc"
extern "C" void gowanat_tspm_zera_2238(void*);   /* port/spu/gowanat_tspm */
#include "gowa_pm_hot.inc"
extern "C" void gowapm_0079E500_spu_recomp_register(void);   /* GOWA_PM_NATIVE */
extern "C" void gowass_30021480_spu_recomp_register(void);   /* GOWA_SYSSRV_NATIVE */
extern "C" void gowatspm_30023680_spu_recomp_register(void); /* GOWA_TSPM_NATIVE */
#define GOWA_IMG_JOBMOD_BASE 620000
extern "C" void spu_interp_hot_register(uint32_t lsa, uint32_t len, uint32_t ea, uint32_t mod_lsa, uint32_t mod_ea, void (*fn)(void*));
extern "C" void spu_interp_hot_register_reloc(uint32_t lifted_lsa, uint32_t len, uint32_t ea, uint32_t w0, void (*fn)(void*));
extern "C" uint32_t spu_registry_size(void);
extern "C" int spu_registry_entry_img(uint32_t i, void** host, uint32_t* ls_addr, int* image_id);
static void gowa_register_jobmod_lifts(void)
{
    if (getenv("GOWA_NO_JOBLIFT")) return;
    const char* skip = getenv("GOWA_JOBLIFT_SKIP");
    int n = 0;
    for (size_t i = 0; i < sizeof g_gowa_jobmods / sizeof g_gowa_jobmods[0]; i++) {
        char key[16]; snprintf(key, sizeof key, "%X", g_gowa_jobmods[i].va);
        if (skip && strstr(skip, key)) continue;
        int img = GOWA_IMG_JOBMOD_BASE + (int)i;
        spu_begin_image(img);
        g_gowa_jobmods[i].reg();
        spu_begin_image(0);
        spu_overlay_register_region(g_gowa_jobmods[i].va, g_gowa_jobmods[i].bytes, img);
        n++;
    }
    fprintf(stderr, "[gowa] %d módulos de job WWS registrados como overlay nativo\n", n);
    /* GOWA_JOBLIFT_HOT=VA[,VA...] (padrão 6FC080; 0 desliga): de um módulo
     * que fica interpretado (JOBLIFT_SKIP), as funções folha seguras
     * (gowa_jobmods_hot.inc, scripts/folhas_spu.py) rodam nativas, chamadas em
     * linha pelo interpretador (spu_interp_hot_register). O módulo é
     * registrado como imagem, sem região residente. */
    const char* hot = getenv("GOWA_JOBLIFT_HOT");
    if (!hot) hot = "6FC080";
    if (*hot && strcmp(hot, "0") != 0) {
        for (size_t i = 0; i < sizeof g_gowa_jobmods / sizeof g_gowa_jobmods[0]; i++) {
            const uint32_t va = g_gowa_jobmods[i].va;
            char key[16]; snprintf(key, sizeof key, "%X", va);
            if (!strstr(hot, key) || !skip || !strstr(skip, key)) continue;
            const int img = GOWA_IMG_JOBMOD_BASE + (int)i;
            spu_begin_image(img); g_gowa_jobmods[i].reg(); spu_begin_image(0);
            /* LS -> função traduzida desta imagem (uma passada no registro) */
            std::unordered_map<uint32_t, void*> por_ls;
            for (uint32_t r = 0, nr = spu_registry_size(); r < nr; r++) {
                void* h; uint32_t l; int im;
                if (spu_registry_entry_img(r, &h, &l, &im) && im == img && h) por_ls.emplace(l, h);
            }
            int nh = 0;
            for (size_t k = 0; k < sizeof g_gowa_hot / sizeof g_gowa_hot[0]; k++) {
                if (g_gowa_hot[k].va != va) continue;
                auto it = por_ls.find(g_gowa_hot[k].ls);
                if (it == por_ls.end()) continue;
                void* fn = it->second;
                spu_interp_hot_register(g_gowa_hot[k].ls, g_gowa_hot[k].len, va + (g_gowa_hot[k].ls - 0x4000u),
                                        0, 0, (void (*)(void*))fn);   /* folha: basta conferir os bytes dela */
                nh++;
            }
            fprintf(stderr, "[gowa] módulo 0x%X interpretado com %d funções folha nativas (GOWA_JOBLIFT_HOT)\n", va, nh);
        }
    }
    /* Policy module WWS (EA 0x79E500 em LS 0x0A00): folhas >= 32 bytes ou com
     * laço, nativas em linha (gowa_pm_hot.inc, scripts/lift_gowa_pm.py).
     * GOWA_PM_HOT=0 desliga. */
    { const char* e = getenv("GOWA_PM_HOT");
      if (!(e && e[0] == '0')) {
          for (size_t k = 0; k < sizeof g_gowa_pm_fns / sizeof g_gowa_pm_fns[0]; k++)
              spu_interp_hot_register(g_gowa_pm_fns[k].ls, g_gowa_pm_fns[k].len,
                                      g_gowa_pm_ea + (g_gowa_pm_fns[k].ls - g_gowa_pm_ls), 0, 0, g_gowa_pm_fns[k].fn);
          fprintf(stderr, "[gowa] %d funções folha do policy module WWS nativas\n",
                  (int)(sizeof g_gowa_pm_fns / sizeof g_gowa_pm_fns[0]));
      } }
    /* GOWA_PM_NATIVE=1: o PM inteiro nativo. A tradução completa vira uma
     * imagem com região residente: o GET do kernel (0x79E500 -> LS 0xA00,
     * 0x3000 bytes) a marca residente, e o despachante e o interpretador
     * (spu_lifted_lookup_resident) entram nas funções nativas dele. Na fase,
     * o PM interpretado era ~45% do tempo interpretado das SPUs 0-3. */
    { const char* e = getenv("GOWA_PM_NATIVE");
      if (e && e[0] == '1') {
          const int img = GOWA_IMG_JOBMOD_BASE + 900;
          spu_begin_image(img); gowapm_0079E500_spu_recomp_register(); spu_begin_image(0);
          spu_overlay_register_region(g_gowa_pm_ea, 0x3000u, img);
          fprintf(stderr, "[gowa] policy module WWS nativo (imagem %d, região 0x%X)\n", img, g_gowa_pm_ea);
      } }
    /* GOWA_SYSSRV_NATIVE=1 (24/10): o system service do SPURS (workload 32; libsre EA
     * 0x30021480, que o kernel copia para LS 0xA00) inteiro nativo, como o PM WWS
     * (scripts/lift_gowa_syssrv.py). No cap. 16 ele era ~90% das instruções SPU
     * interpretadas (~63 M/s): toda SPU que fica sem trabalho passa por ele. */
    { const char* e = getenv("GOWA_SYSSRV_NATIVE");
      if (e && e[0] == '1') {
          const int img = GOWA_IMG_JOBMOD_BASE + 901;
          spu_begin_image(img); gowass_30021480_spu_recomp_register(); spu_begin_image(0);
          spu_overlay_register_region(0x30021480u, 0x2200u, img);
          fprintf(stderr, "[gowa] system service do SPURS nativo (imagem %d, região 0x30021480)\n", img);
      } }
    /* GOWA_TSPM_NATIVE=1 (24/10, experimento): o PM de taskset do libsre (EA
     * 0x30023680, 0x1E40 bytes, LS 0xA00; w1/w2/w3/w5) inteiro nativo, como o
     * system service -- a tradução completa (scripts/lift_gowa_tspm.py) já ia no
     * executável, só as folhas eram usadas (GOWA_TSPM_HOT). No cap. 16 ~330 mil
     * entradas por segundo no micro-interpretador em LS 0x2238 (GOWA_SMC_HIST). */
    { const char* e = getenv("GOWA_TSPM_NATIVE");
      if (e && e[0] == '1') {
          const int img = GOWA_IMG_JOBMOD_BASE + 902;
          spu_begin_image(img); gowatspm_30023680_spu_recomp_register(); spu_begin_image(0);
          spu_overlay_register_region(0x30023680u, 0x1E40u, img);
          fprintf(stderr, "[gowa] PM de taskset do libsre nativo inteiro (imagem %d, região 0x30023680)\n", img);
      } }
    /* Plugins do módulo 0x72DF80 (código carregado em LS variável): funções
     * folha relocáveis, achadas pelo conteúdo (gowa_plugins.inc,
     * scripts/lift_gowa_plugins.py). GOWA_PLUGINS_NATIVE=0 desliga. */
    { const char* e = getenv("GOWA_PLUGINS_NATIVE");
      if (!(e && e[0] == '0')) {
          for (size_t k = 0; k < sizeof g_gowa_plugin_fns / sizeof g_gowa_plugin_fns[0]; k++)
              spu_interp_hot_register_reloc(g_gowa_plugin_fns[k].ls, g_gowa_plugin_fns[k].len, g_gowa_plugin_fns[k].ea,
                                            g_gowa_plugin_fns[k].w0, g_gowa_plugin_fns[k].fn);
          fprintf(stderr, "[gowa] %d funções de plugin SPU relocáveis nativas\n",
                  (int)(sizeof g_gowa_plugin_fns / sizeof g_gowa_plugin_fns[0]));
      } }
    /* Os três fragmentos de código que o w11 (BatchJobList) traz por DMA para
     * LS variável (0x6E5830, 0x6E8130, 0x70F630; gowa_plugins_w11.inc): eram
     * ~27% do código interpretado na fase. Ligado por padrão desde 20/10
     * (GOWA_PLUGINS_W11=0 desliga): o "pior" de 19/10 e a trava aos 23 s eram o
     * link do truque PIC (brsl para a instrução seguinte) saindo no espaço
     * traduzido; retraduzidos com lift_gowa_plugins.py --naofolha (spu_link e
     * volta do dreno relocados, funções que chamam o próprio fragmento),
     * GOWA_HOTDIFF sem divergências, A/B abw11 +1,3%. */
    { const char* e = getenv("GOWA_PLUGINS_W11");
      if (!(e && e[0] == '0')) {
          /* GOWA_PLUGINS_W11_FAIXA=a-b: só as entradas [a, b) da tabela (bissecção) */
          size_t fa = 0, fb = (size_t)-1;
          if (const char* fx = getenv("GOWA_PLUGINS_W11_FAIXA")) {
              fa = (size_t)strtoul(fx, nullptr, 10);
              if (const char* h = strchr(fx, '-')) fb = (size_t)strtoul(h + 1, nullptr, 10);
          }
          for (size_t k = 0; k < sizeof g_gowa_plugin_fns_w11 / sizeof g_gowa_plugin_fns_w11[0]; k++)
              if (k >= fa && k < fb)
              spu_interp_hot_register_reloc(g_gowa_plugin_fns_w11[k].ls, g_gowa_plugin_fns_w11[k].len, g_gowa_plugin_fns_w11[k].ea,
                                            g_gowa_plugin_fns_w11[k].w0, g_gowa_plugin_fns_w11[k].fn);
          fprintf(stderr, "[gowa] %d funções de plugin do w11 relocáveis nativas\n",
                  (int)(sizeof g_gowa_plugin_fns_w11 / sizeof g_gowa_plugin_fns_w11[0]));
      } }
    /* Policy module de taskset do libsre (EA 0x30023680 em LS 0x0A00; w1/w2/
     * w3/w5): o laço que zera a área da tarefa (port/spu/gowanat_tspm, escrito
     * à mão) e as folhas seguras (gowa_tspm_hot.inc, scripts/lift_gowa_tspm.py),
     * nativos em linha. A thread principal espera as tarefas desses tasksets
     * em func_003CC6B4. GOWA_TSPM_HOT=0 desliga. */
    { const char* e = getenv("GOWA_TSPM_HOT");
      if (!(e && e[0] == '0')) {
          spu_interp_hot_register(0x2238u, 0x30u, g_gowa_tspm_ea + (0x2238u - g_gowa_tspm_ls), 0, 0, gowanat_tspm_zera_2238);
          for (size_t k = 0; k < sizeof g_gowa_tspm_fns / sizeof g_gowa_tspm_fns[0]; k++)
              spu_interp_hot_register(g_gowa_tspm_fns[k].ls, g_gowa_tspm_fns[k].len,
                                      g_gowa_tspm_ea + (g_gowa_tspm_fns[k].ls - g_gowa_tspm_ls), 0, 0, g_gowa_tspm_fns[k].fn);
          fprintf(stderr, "[gowa] PM de taskset do libsre: laço de zerar + %d folhas nativas\n",
                  (int)(sizeof g_gowa_tspm_fns / sizeof g_gowa_tspm_fns[0]));
      } }
}

/* As outras três ELFs de tarefa do GOWA.elf (VA = offset + 0x10000; texto no
 * offset 0x100, carregado em LS 0x3000). Já traduzidas em port/spu/ (o
 * build_spu_workloads registra a 0x6BA800 por impressão digital, o que não
 * vale para tarefas que o policy module do taskset carrega por DMA), mas sem
 * região residente rodavam interpretadas: no menu, a última tarefa de cada
 * fase dos wid 1/2 chegava 40-160 ms depois das outras e a main esperava por
 * ela em func_003CC6B4 (95% do frame). GOWA_TASKLIFT_SKIP=va,... pula. */
extern "C" void spu_82b41c80ff19cc8445ab1d0ecdb41126539611704b5675fd09c7e85823130695_spu_recomp_register(void);
extern "C" void spu_343cf6fab66b578189b842f4e3cb731a476a2df591aa1c8c53c06789c73e341f_spu_recomp_register(void);
extern "C" void spu_d08b1ad818d026818f1c7cbb2bef448d242a9ae51aab4c7bce69f2ad14308355_spu_recomp_register(void);
static const struct { uint32_t elf_va, text, bytes; int img; void (*reg)(void); } g_gowa_tasks[] = {
    { 0x006BA800u, 0x006BA900u, 0xDAA0u, 600002, spu_82b41c80ff19cc8445ab1d0ecdb41126539611704b5675fd09c7e85823130695_spu_recomp_register },
    { 0x006D1F80u, 0x006D2080u, 0xA130u, 600003, spu_343cf6fab66b578189b842f4e3cb731a476a2df591aa1c8c53c06789c73e341f_spu_recomp_register },
    { 0x006DC300u, 0x006DC400u, 0x9350u, 600004, spu_d08b1ad818d026818f1c7cbb2bef448d242a9ae51aab4c7bce69f2ad14308355_spu_recomp_register },
};

static void gowa_register_task_lifts(void)
{
    gowa_register_jobmod_lifts();
    if (getenv("GOWA_NO_TASKLIFT")) return;
    spu_begin_image(GOWA_IMG_TASK_8E4F);
    spu_8e4f213617fb468dc09ae1a03a0b333b6f22ccbb4c24ee36d3a0e11c0a94b4d8_spu_recomp_register();
    spu_begin_image(0);
    spu_overlay_register_region(0x006C8600u, 0x9900u, GOWA_IMG_TASK_8E4F);
    fprintf(stderr, "[gowa] tarefas do taskset (ELF 0x6C8500) registradas como overlay nativo (imagem %d)\n",
            GOWA_IMG_TASK_8E4F);
    const char* skip = getenv("GOWA_TASKLIFT_SKIP");
    for (size_t i = 0; i < sizeof g_gowa_tasks / sizeof g_gowa_tasks[0]; i++) {
        char key[16]; snprintf(key, sizeof key, "%X", g_gowa_tasks[i].elf_va);
        if (skip && strstr(skip, key)) continue;
        spu_begin_image(g_gowa_tasks[i].img);
        g_gowa_tasks[i].reg();
        spu_begin_image(0);
        spu_overlay_register_region(g_gowa_tasks[i].text, g_gowa_tasks[i].bytes, g_gowa_tasks[i].img);
        fprintf(stderr, "[gowa] tarefa ELF 0x%X registrada como overlay nativo (imagem %d)\n",
                g_gowa_tasks[i].elf_va, g_gowa_tasks[i].img);
    }
}

extern "C" void gowa_early_register(void)
{
    gowa_register_task_lifts();
    if (getenv("GOWA_NO_MMAPPER")) return;
    ps3_hle_register_ctx(0xDC578057u, "sys_mmapper_map_memory(gowa)", h_mmapper_map);
    ps3_hle_register_ctx(0x4643BA6Eu, "sys_mmapper_unmap_memory(gowa)", h_mmapper_unmap);
}

/* Espera real pelo job SPU "UpdateTexStreaming" (remendo do jogo, inserido no
 * código lifted por scripts/remendos_ppu.py).
 *
 * O job (módulo 0x6EDC00, disparado em func_0038D80C a cada frame) faz GET da
 * tabela de estado do heap de VRAM inteira (*(u32*)0x9B1718, 0xA28 x 16 B),
 * atualiza e devolve tudo por PUT. O disparo soma 1 ao contador u16 em
 * 0x9AA4A4 (lwarx/stwcx em func_003DA3E0) e o gerenciador de jobs da SPU
 * subtrai no fim. Só que as esperas do próprio jogo (func_0038D9A0 no meio do
 * frame, e a do início de func_0038DA00, antes da manutenção do heap) leem o
 * u16 em 0x9AA4A8, onde nada escreve: não esperam nada. No console o job
 * termina em ~100 us, muito antes de o PPU voltar a mexer na tabela; aqui as
 * SPUs são o gargalo e o job fica na fila por milissegundos, e o PUT final
 * apaga o que o PPU gravou nesse meio-tempo (alocações, compactação). Isso
 * corrompe a contabilidade do heap (MoveLocalMemory "offset error", blocos
 * que só crescem, texturas com lixo, travas).
 *
 * Esta espera devolve a garantia de tempo do console: antes da manutenção,
 * no ponto de espera oficial e na entrada de quem grava a tabela, o job em voo
 * termina primeiro. GOWA_TSWAIT=0 desliga; GOWA_TSWAIT_LOG=1 mostra o custo. */
/* Remendo "pausa" (scripts/remendos_ppu.py): PAUSE nas esperas ativas da
 * thread principal. GOWA_PPU_PAUSE=0 desliga.
 * GOWA (24/10): modo 2 = PAUSE e, a cada 256 voltas seguidas na mesma espera
 * (~20-40 us), SwitchToThread -- cede a fatia a uma thread pronta na mesma CPU.
 * Padrão com menos de 12 CPUs lógicas: no notebook (4 núcleos/8 threads, ~9
 * threads ocupadas) a principal passa ~30% do quadro nessas voltas, disputando
 * a CPU com as SPUs, que são o gargalo. GOWA_PPU_PAUSE=1|2 força o modo. */
extern "C" { int g_gowa_ppu_pause = 1; }
static const int s_gowa_ppu_pause_init = [] {
    const char* e = getenv("GOWA_PPU_PAUSE");
    if (e && *e) g_gowa_ppu_pause = atoi(e);
    else { SYSTEM_INFO si; GetSystemInfo(&si); if (si.dwNumberOfProcessors < 12) g_gowa_ppu_pause = 2; }
    return 0;
}();
extern "C" void gowa_ppu_espera(void)
{
    static thread_local unsigned long long t_ult;
    static thread_local unsigned t_n;
    const unsigned long long agora = __rdtsc();
    if (agora - t_ult > 200000ull) t_n = 0;   /* mais de ~50 us sem voltar aqui: outra espera */
    t_ult = agora;
    if (++t_n >= 256) {
        t_n = 0;
        /* GOWA (26/10): modo 3 = dorme 1 ms em vez de ceder a fatia. Com mais threads
         * ocupadas que CPUs (notebook 4/8), o SwitchToThread só cede a uma thread pronta na
         * MESMA CPU, e a principal segue girando (~80% de uma CPU) enquanto as SPUs esperam
         * CPU. Os jobs que ela espera levam milissegundos lá. GOWA_PRIO_AB 'y' alterna. */
        if (g_gowa_ppu_pause == 3) Sleep(1); else SwitchToThread();
        return;
    }
    _mm_pause();
}
extern "C" void gowa_texstream_wait(void)
{
    static int s_on = -1, s_log;
    if (s_on < 0) { const char* e = getenv("GOWA_TSWAIT"); s_on = (e && *e == '0') ? 0 : 1;
                    s_log = getenv("GOWA_TSWAIT_LOG") ? 1 : 0; }
    if (!s_on || !vm_base) return;
    const volatile uint8_t* c = (const volatile uint8_t*)(vm_base + 0x9AA4A4);
    if (!(c[0] | c[1])) return;
    LARGE_INTEGER f, t0, t1; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t0);
    int k = 0;
    for (;;) {
        if (!(c[0] | c[1])) break;
        if (++k < 4000) YieldProcessor();
        else {
            SwitchToThread();
            QueryPerformanceCounter(&t1);
            if ((t1.QuadPart - t0.QuadPart) * 1000 / f.QuadPart > 500) {   /* nunca travar por isto */
                static volatile long s_to; if (_InterlockedIncrement(&s_to) <= 8)
                    fprintf(stderr, "[tswait] job não terminou em 500 ms (contador %02X%02X); seguindo\n", c[0], c[1]);
                break;
            }
        }
    }
    if (s_log) {
        QueryPerformanceCounter(&t1);
        static volatile long long s_us; static volatile long s_n; static ULONGLONG s_last;
        _InterlockedExchangeAdd64(&s_us, (t1.QuadPart - t0.QuadPart) * 1000000 / f.QuadPart);
        _InterlockedIncrement(&s_n);
        const ULONGLONG now = GetTickCount64();
        if (now - s_last >= 5000) { s_last = now;
            fprintf(stderr, "[tswait] t=%llus %ld esperas, %.1f ms no total (5 s)\n", now / 1000, s_n, s_us / 1000.0);
            s_n = 0; s_us = 0; }
    }
}

/* Cronômetros de scripts/sonda.py cron: tempo dentro de cada chamada marcada,
 * por ponto (o lr da chamada), somado e impresso a cada ~5 s. Aninháveis:
 * uma pilha de inícios por thread (com um início só, uma chamada cronometrada
 * dentro de outra zerava a de fora). */
static thread_local unsigned long long t_gowa_tm0[16];
static thread_local int t_gowa_tmd;   /* aninhamento: um início por nível */
extern "C" void gowa_tm_begin(void) { const int d = t_gowa_tmd++; if (d < 16) t_gowa_tm0[d] = __rdtsc(); }
extern "C" void gowa_tm_end(unsigned int lr)
{
    const int d = --t_gowa_tmd;
    if (d < 0) { t_gowa_tmd = 0; return; }
    if (d >= 16) return;
    const unsigned long long dt = __rdtsc() - t_gowa_tm0[d];
    static struct { unsigned lr; volatile long long n, sum, mx; } st[32]; static volatile long nst;
    static SRWLOCK lk = SRWLOCK_INIT; static ULONGLONG last; static double tsc_us;
    AcquireSRWLockExclusive(&lk);
    if (!tsc_us) {   /* ciclos de TSC por µs, medido uma vez */
        LARGE_INTEGER f, a, b; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&a);
        const unsigned long long c0 = __rdtsc(); do QueryPerformanceCounter(&b); while (b.QuadPart - a.QuadPart < f.QuadPart / 50);
        tsc_us = (double)(__rdtsc() - c0) / ((double)(b.QuadPart - a.QuadPart) * 1e6 / (double)f.QuadPart);
        last = GetTickCount64();
    }
    int i;
    for (i = 0; i < nst; i++) if (st[i].lr == lr) break;
    if (i == nst && nst < 32) { st[i].lr = lr; nst++; }
    if (i < 32) { st[i].n++; st[i].sum += (long long)dt; if ((long long)dt > st[i].mx) st[i].mx = (long long)dt; }
    const ULONGLONG now = GetTickCount64();
    if (now - last >= 5000) {
        const double win_ms = (double)(now - last);
        last = now;
        char b[1600]; int p = 0;
        for (int k = 0; k < nst; k++) {
            if (!st[k].n) continue;
            p += snprintf(b + p, sizeof b - p, " %08X:%lldx %.0fms(%.1f%%) med %.0fus max %.0fus", st[k].lr, st[k].n,
                          st[k].sum / tsc_us / 1000.0, st[k].sum / tsc_us / 10.0 / win_ms,
                          st[k].sum / tsc_us / (double)st[k].n, st[k].mx / tsc_us);
            st[k].n = st[k].sum = st[k].mx = 0;
        }
        fprintf(stderr, "[cron] t=%llus%s\n", now / 1000, b);
    }
    ReleaseSRWLockExclusive(&lk);
}

/* Sonda de depuração inserida no código lifted por scripts/sonda.py. */
extern "C" void gowa_probe(unsigned int addr, ppu_context* ctx)
{
    /* GOWA_PROBE_COUNT=1: só conta passagens por endereço e imprime o total a
     * cada ~5 s (para medir em qual laço de espera uma thread gasta o tempo). */
    { static int s_cnt = -1; if (s_cnt < 0) s_cnt = getenv("GOWA_PROBE_COUNT") ? 1 : 0;
      if (s_cnt) {
          static struct { unsigned a; volatile long n; } cnt[64]; static volatile long ncnt;
          static ULONGLONG last;
          int i2;
          for (i2 = 0; i2 < ncnt; i2++) if (cnt[i2].a == addr) break;
          if (i2 == ncnt && ncnt < 64) { cnt[i2].a = addr; ncnt++; }
          if (i2 < 64) _InterlockedIncrement(&cnt[i2].n);
          ULONGLONG now = GetTickCount64();
          if (now - last >= 5000) { last = now;
              char b[1200]; int p = 0;
              for (int k = 0; k < ncnt && p < 1100; k++) p += snprintf(b + p, sizeof b - p, " %08X:%ld", cnt[k].a, cnt[k].n);
              fprintf(stderr, "[probe-count] t=%llus%s\n", now / 1000, b); }
          return;
      } }
    /* GOWA_PROBE_R3=1: histograma de (endereço, r3) -- qual objeto (lista de
     * job, fila) cada laço de espera está esperando; os 12 mais vistos a cada
     * ~5 s. */
    { static int s_r3 = -1; if (s_r3 < 0) s_r3 = getenv("GOWA_PROBE_R3") ? 1 : 0;
      if (s_r3 && addr != 0x001C05A4u) {
          static struct { unsigned a, r3; volatile long n; } hh[256]; static volatile long nh;
          static SRWLOCK lk = SRWLOCK_INIT; static ULONGLONG last;
          const unsigned r3 = (unsigned)ctx->gpr[3];
          AcquireSRWLockExclusive(&lk);
          int i2;
          for (i2 = 0; i2 < nh; i2++) if (hh[i2].a == addr && hh[i2].r3 == r3) break;
          if (i2 == nh && nh < 256) { hh[i2].a = addr; hh[i2].r3 = r3; hh[i2].n = 0; nh++; }
          if (i2 < 256) hh[i2].n++;
          const ULONGLONG now = GetTickCount64();
          if (now - last >= 5000) { last = now;
              char b[1400]; int p = 0;
              for (int k = 0; k < 12; k++) {
                  int bi = -1; long best = 0;
                  for (int j = 0; j < nh; j++) if (hh[j].n > best) { best = hh[j].n; bi = j; }
                  if (bi < 0) break;
                  p += snprintf(b + p, sizeof b - p, " %08X/r3=%08X:%ld", hh[bi].a, hh[bi].r3, best);
                  hh[bi].n = -hh[bi].n;   /* marcado: já impresso */
              }
              for (int j = 0; j < nh; j++) hh[j].n = 0;
              fprintf(stderr, "[probe-r3] t=%llus%s\n", now / 1000, b); }
          ReleaseSRWLockExclusive(&lk);
          return;
      } }
    /* log de depuração do jogo (FUN_001c05a4(nível, categoria, formato, ...)),
     * vazio na versão final: r4 = categoria, r5 = mensagem. Sempre impresso,
     * sem a linha [sonda] nem o limite de GOWA_PROBE_MAX -- antes era preciso
     * GOWA_PROBE_MAX=100000 para vê-lo inteiro, e com isso a sonda 0x415914
     * (thread principal) escrevia ~100 mil linhas por execução. */
    if (addr == 0x001C05A4u) {
        char c[64], m[200]; uint32_t pc_ = (uint32_t)ctx->gpr[4], pm = (uint32_t)ctx->gpr[5];
        for (int k = 0; k < 63; k++) { c[k] = pc_ ? (char)vm_read8(pc_ + k) : 0; if (!c[k]) break; } c[63] = 0;
        /* 25/10: "go to playing" e os outros estados ("go to loading ui", "go to game
         * shell", ...): o resumo de desempenho (desempenho.json) só conta o tempo jogando */
        if (!strncmp(c, "go to ", 6)) { extern volatile int g_gowa_jogando; g_gowa_jogando = !strcmp(c, "go to playing"); }
        for (int k = 0; k < 199; k++) { m[k] = pm ? (char)vm_read8(pm + k) : 0; if (!m[k]) break; } m[199] = 0;
        extern uint64_t ppu_timebase_now(void);
        fprintf(stderr, "[jogo-log] t=%llums tid=%u [%s] %s (r6=0x%08X r7=0x%08X)\n",
                (unsigned long long)(ppu_timebase_now() / 79800ull), (unsigned)ctx->thread_id, c, m,
                (uint32_t)ctx->gpr[6], (uint32_t)ctx->gpr[7]);
        return;
    }
    /* 8 passagens por (endereço, thread) */
    static struct { unsigned int a; unsigned t; int n; } vistos[1024];
    static int nv = 0;
    int i;
    unsigned tid = (unsigned)ctx->thread_id;
    for (i = 0; i < nv; i++) if (vistos[i].a == addr && vistos[i].t == tid) break;
    if (i == nv) { if (nv >= 1024) return; vistos[nv].a = addr; vistos[nv].t = tid; vistos[nv].n = 0; nv++; }
    static int maxp = -1;
    if (maxp < 0) { const char* e = getenv("GOWA_PROBE_MAX"); maxp = e ? atoi(e) : 8; }
    if (vistos[i].n++ >= maxp) return;
    fprintf(stderr, "[sonda] 0x%08X #%d tid=%u r3=0x%08X r4=0x%08X r5=0x%08X r6=0x%08X lr=0x%08X cr=0x%08X\n",
            addr, vistos[i].n, (unsigned)ctx->thread_id, (uint32_t)ctx->gpr[3], (uint32_t)ctx->gpr[4],
            (uint32_t)ctx->gpr[5], (uint32_t)ctx->gpr[6], (uint32_t)ctx->lr, ctx->cr);
    /* open do FIOS (func_004C4FB0): r5 = caminho */
    if (addr == 0x004C4FB0u) {
        char str[128]; uint32_t p = (uint32_t)ctx->gpr[5];
        for (int k = 0; k < 127; k++) { str[k] = (char)vm_read8(p + k); if (!str[k]) break; }
        str[127] = 0;
        fprintf(stderr, "[sonda]    caminho='%s'\n", str);
    }
}

/* cellVdecQueryAttrEx / cellVdecOpenEx: o runtime só tem as versões sem "Ex".
 * O layout que importa é o mesmo (CellVdecTypeEx começa com codecType; o
 * atributo é idêntico; OpenEx(type, res, cb, handle) tem os mesmos quatro
 * argumentos, e o recurso é ignorado). Sem isto o jogo chamava QueryAttrEx ao
 * entrar no menu ("ready to accept XMB invites"), recebia r3=0 sem atributo
 * preenchido e nunca abria o decodificador do vídeo do menu (o RPCS3 abre:
 * QueryAttrEx -> OpenEx -> StartSeq, 32:59-33:31 no log de um PC de teste). */
extern "C" void ps3_hle_register(unsigned int nid, const char* name, void* handler);
extern "C" void cellVdecQueryAttr(void);
extern "C" void cellVdecOpen(void);

extern "C" void gowa_offline_register(void)
{
    if (!ps3_hle_has(0xC982A84Au)) ps3_hle_register(0xC982A84Au, "cellVdecQueryAttrEx(gowa)", (void*)cellVdecQueryAttr);
    if (!ps3_hle_has(0x0053E2D8u)) ps3_hle_register(0x0053E2D8u, "cellVdecOpenEx(gowa)", (void*)cellVdecOpen);
    const int listar = getenv("GOWA_IMPORTS") != nullptr;
    int sem_handler = 0, instalados = 0;
    static hfn escolhido[N_IMPORTS];
    for (size_t i = 0; i < N_IMPORTS; i++) {
        uint32_t nid = g_gowa_imports[i].nid;
        if (ps3_hle_has(nid) || prx_resolve_export(nid)) continue;
        sem_handler++;
        hfn f = handler_especifico(nid);
        if (!f) f = handler_familia(g_gowa_imports[i].lib);
        escolhido[i] = f;
        if (listar)
            fprintf(stderr, "[gowa] import sem handler: %-22s 0x%08X%s\n", g_gowa_imports[i].lib, nid,
                    f ? "  -> resposta offline" : "  -> (continua no default do runtime: r3=0)");
    }
    /* Registra depois de listar: ps3_hle_has() passaria a ver os nossos. */
    for (size_t i = 0; i < N_IMPORTS; i++)
        if (escolhido[i]) { ps3_hle_register_ctx(g_gowa_imports[i].nid, g_gowa_imports[i].lib, escolhido[i]); instalados++; }
    fprintf(stderr, "[gowa] imports: %u no total, %d sem handler do runtime, %d com resposta offline\n",
            (unsigned)N_IMPORTS, sem_handler, instalados);

    if (const char* m = getenv("GOWA_SPURS_MON"))
        CreateThread(NULL, 0, spurs_mon, (LPVOID)(uintptr_t)strtoul(m, nullptr, 16), 0, NULL);
    if (getenv("GOWA_CNTWATCH"))
        CreateThread(NULL, 0, cntwatch_thread, NULL, 0, NULL);
    if (getenv("GOWA_WATCH"))
        CreateThread(NULL, 0, watch_thread, NULL, 0, NULL);
    if (getenv("GOWA_PEEK"))
        CreateThread(NULL, 0, peek_thread, NULL, 0, NULL);
    if (getenv("GOWA_SPU_SAMPLER"))
        CreateThread(NULL, 0, sampler_thread, NULL, 0, NULL);
}
