/*
 * GOWA (27/10): conferência dos jobs de SPU -- trocar o código de um módulo de job (lifter
 * novo, EDGE nativo) sem mudar o que ele produz, byte a byte.
 *
 * Gravação, no jogo: GOWA_CONF=<VA do módulo>:<LS da entrada>:<início s>:<jobs>[:<1 a cada k>]
 * (ex. 72DF80:AD98:200:300:5). A entrada do módulo é trocada no registro por um invólucro
 * que grava, por job: o contexto inteiro da SPU na entrada (registradores, LS, estado do
 * MFC), cada transferência do job (os bytes lidos da memória principal e os escritos nela),
 * as operações atômicas, as leituras do decrementador e o contexto no fim do job, em
 * <GOWA_CONF_DIR, padrão out/confere>/j<n>.gcj. Enquanto grava, o DMA vai todo pelo caminho
 * completo (o enxuto não passa pelo gancho) -- o jogo fica um pouco mais lento.
 *
 * Reprodução, fora do jogo: GOWA_CONF_REPLAY=<pasta> carrega o ELF e registra os módulos,
 * mas não roda o jogo. Para cada job gravado: põe na memória do guest os bytes que o job
 * leu, restaura o contexto, roda a entrada do módulo (o código DESTE exe) e confere com o
 * gravado: as escritas na memória principal, a LS e os registradores do fim. Com o exe do
 * código velho, tudo igual valida a bancada; com o do novo, é a conferência.
 * GOWA_CONF_REPS=<n> (padrão 5): roda cada job n vezes e anota o menor tempo (µs), para
 * comparar o mesmo job no código velho e no novo. Resultado em <pasta>/conferencia.txt.
 *
 * Jobs que a reprodução não reproduz por natureza (marcados na gravação): leram caixa de
 * correio, sinal ou eventos, tomaram interrupção, ou usaram um comando do MFC que não é
 * GET/PUT/atômico. Uma leitura que viu bytes diferentes da mesma memória sem o job ter
 * escrito nela (outra SPU ou a PPU mexendo durante o job) aparece como "entrada instável".
 */
#include "spu_context.h"
#include "win32_compat.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <csetjmp>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" uint8_t* vm_base;
typedef void (*conf_fn)(spu_context*);
extern "C" int spu_registry_patch(int image_id, uint32_t addr, conf_fn fn, conf_fn* old);
extern "C" conf_fn spu_lookup(uint32_t addr, int image_id);
extern "C" void spu_coh_unregister(spu_context* ctx);
extern "C" void spu_halt_arm_replay(int on);
extern "C" void spu_daz_enter(void);

/* 0 = desligado, 1 = gravando, 2 = reproduzindo (lido pelos ganchos do runtime) */
extern "C" { int g_gowa_conf_on = 0; }

namespace {

enum : uint8_t { EV_GET = 1, EV_PUT, EV_GETLLAR, EV_PUTLLC, EV_PUTLLUC, EV_ZERO, EV_DEC, EV_CANAL, EV_LUZ };
enum : uint32_t { F_CANAL = 1, F_INTERRUPCAO = 2, F_CMD = 4, F_GRANDE = 8 };
enum : uint16_t { EF_FALHOU = 1 };   /* atômico: o PUTLLC seguinte falhou (GETLLAR) / falhou (PUTLLC) */

#pragma pack(push, 1)
struct Cab { char magic[4]; uint32_t versao, ctx_bytes, va, entrada, job, nev, flags; int32_t img; };
struct Ev { uint8_t tipo, cmd; uint16_t flags; uint32_t ea, lsa, size; };   /* + size bytes */
#pragma pack(pop)

uint32_t s_va, s_entrada, s_max = 0, s_cada = 1;
ULONGLONG s_ini_ms, s_t0;
int s_img = -1;
conf_fn s_velha;
std::string s_dir = "out/confere";
volatile LONG s_vistos, s_gravados, s_entradas, s_perdidos;

struct Rec {
    spu_context* ctx;
    std::vector<uint8_t> buf;
    uint32_t nev = 0, flags = 0;
    size_t ultimo_getllar = (size_t)-1;
};
thread_local Rec* t_rec;

void ev_add(Rec* r, uint8_t tipo, uint8_t cmd, uint16_t fl, uint32_t ea, uint32_t lsa, uint32_t size, const void* dados)
{
    if (r->buf.size() > (256u << 20)) { r->flags |= F_GRANDE; return; }
    Ev e{tipo, cmd, fl, ea, lsa, size};
    const uint8_t* p = (const uint8_t*)&e;
    r->buf.insert(r->buf.end(), p, p + sizeof e);
    if (size && dados) { const uint8_t* d = (const uint8_t*)dados; r->buf.insert(r->buf.end(), d, d + size); }
    else if (size) r->buf.resize(r->buf.size() + size);
    r->nev++;
}

void ctx_add(std::vector<uint8_t>& b, const spu_context* ctx)
{
    const size_t o = b.size();
    b.resize(o + sizeof(spu_context));
    memcpy(&b[o], ctx, sizeof(spu_context));
    if (ctx->ls != ctx->ls_store)   /* LS fora do contexto: a cópia vai no lugar da ls_store */
        memcpy(&b[o + offsetof(spu_context, ls_store)], ctx->ls, SPU_LS_SIZE);
}

void fecha(Rec* rp, spu_context* ctx, uint32_t pend);
struct Rep;
extern Rep* s_rep;
bool rep_xfers_feitas();

/* O mesmo laço do SPU_DRAIN, até o job voltar ao endereço de retorno da entrada. Volta com
 * a transferência de volta (se houver) ainda pendente, para o chamador seguir como antes.
 * Uma interrupção no meio fecha a gravação do job ali, antes do tratador (o job acabou: o
 * kernel da SPURS passa ao próximo); o laço segue como o SPU_DRAIN seguiria. Na reprodução,
 * parar = o pc em que a interrupção veio no jogo: volta true no primeiro passo ali depois de
 * todas as transferências gravadas. (Não o número do passo: com o evento pendente, o
 * SPU_TAILX do jogo recusa a chamada direta e passa pelo dreno -- passos a mais.) */
bool drena_ate(spu_context* ctx, uint32_t link, Rec* r, uint32_t parar = 0)
{
    while (g_spu_trampoline_fn && ((ctx->pc ^ link) & SPU_LS_MASK)) {
        void (*tf)(spu_context*) = g_spu_trampoline_fn;
        g_spu_trampoline_fn = 0;
        spu_drain_step_hooks(ctx, (void*)tf);
        if (parar && !((ctx->pc ^ parar) & SPU_LS_MASK) && rep_xfers_feitas()) {
            g_spu_trampoline_fn = tf;
            return true;
        }
        if (ctx->int_enable && (ctx->event_status & ctx->event_mask)) {
            if (r && t_rec == r) {
                g_spu_trampoline_fn = tf;   /* o contexto como estava antes do tratador */
                t_rec = nullptr;
                fecha(r, ctx, 3);
                delete r;
                r = nullptr;
                g_spu_trampoline_fn = 0;
            }
            tf = spu_take_interrupt(ctx, tf);
        }
        ctx->tail_budget = g_spu_tail_budget;
        tf(ctx);
    }
    return false;
}

/* fim do job: o contexto de saída e como ele saiu (0 = voltou, 1 = voltou com a transferência
 * de volta pendente, 2 = saiu por um salto não local -- reinício da pilha do guest, parada) */
void fecha(Rec* rp, spu_context* ctx, uint32_t pend)
{
    Rec& r = *rp;
    Cab c;
    memcpy(&c, &r.buf[0], sizeof c);
    static const char fim[4] = {'F', 'I', 'M', '!'};
    r.buf.insert(r.buf.end(), fim, fim + 4);
    ctx_add(r.buf, ctx);
    r.buf.insert(r.buf.end(), (const uint8_t*)&pend, (const uint8_t*)&pend + 4);
    c.job = (uint32_t)InterlockedIncrement(&s_gravados) - 1;
    c.nev = r.nev; c.flags = r.flags;
    memcpy(&r.buf[0], &c, sizeof c);
    char p[MAX_PATH];
    snprintf(p, sizeof p, "%s/j%04u.gcj", s_dir.c_str(), c.job);
    if (FILE* f = fopen(p, "wb")) { fwrite(r.buf.data(), 1, r.buf.size(), f); fclose(f); }
    if (c.job % 50 == 0 || c.job + 1 == s_max)
        fprintf(stderr, "[confere] job %u gravado aos %.1f s (%ld entradas do módulo desde o início)\n",
                c.job, (GetTickCount64() - s_t0) / 1000.0, (long)s_entradas);
    if (c.job + 1 == s_max)
        fprintf(stderr, "[confere] %u jobs do 0x%X gravados em %s (%ld perdidos)\n", s_max, s_va, s_dir.c_str(), (long)s_perdidos);
}

/* o registro fica no heap: um job que sai por salto não local abandona este quadro da pilha */
void grava_job(spu_context* ctx)
{
    Rec* r = new Rec; r->ctx = ctx;
    r->buf.reserve(1u << 20);
    Cab c{{'G', 'C', 'J', '1'}, 2, (uint32_t)sizeof(spu_context), s_va, s_entrada, 0, 0, 0, s_img};
    r->buf.resize(sizeof c);
    memcpy(&r->buf[0], &c, sizeof c);
    ctx_add(r->buf, ctx);
    const uint32_t link = ctx->gpr[0]._u32[0];
    t_rec = r;
    s_velha(ctx);
    drena_ate(ctx, link, r);
    if (t_rec && t_rec == r) {
        t_rec = nullptr;
        fecha(r, ctx, g_spu_trampoline_fn ? 1u : 0u);
        delete r;
    }
}

void entrada_conf(spu_context* ctx)
{
    InterlockedIncrement(&s_entradas);
    if (t_rec) {   /* o job anterior desta thread saiu por um caminho sem gancho */
        delete t_rec; t_rec = nullptr;
        InterlockedIncrement(&s_perdidos);
    }
    if (g_gowa_conf_on == 1 && !t_rec && s_gravados < (LONG)s_max && GetTickCount64() - s_t0 >= s_ini_ms) {
        const LONG k = InterlockedIncrement(&s_vistos) - 1;
        if (k % (LONG)s_cada == 0 && s_gravados < (LONG)s_max) { grava_job(ctx); return; }
    }
    s_velha(ctx);
}

/* ---- reprodução ------------------------------------------------------------------- */
struct Job {
    std::string nome;
    std::vector<uint8_t> raw;
    Cab cab;
    const uint8_t *ctx_in, *ctx_out;
    uint32_t pend;
    struct E { Ev e; const uint8_t* d; };
    std::vector<E> evs;
};
struct Rep {
    const Job* j;
    size_t i_dec = 0, i_get = 0, i_atom = 0, i_canal = 0, i_luz = 0;
    uint32_t canal_val = 0; bool canal_pend = false;
    std::vector<size_t> xfers;   /* índices de evs que são transferências */
    uint32_t diverge = 0, sem_par = 0, n_xfer = 0;
    int primeiro_div = -1;
};
Rep* s_rep;
jmp_buf s_jb;
bool rep_xfers_feitas() { return s_rep && s_rep->n_xfer >= s_rep->xfers.size(); }
   /* reprodução: o salto não local do job volta aqui */

bool carrega(const char* path, Job& j)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); const long n = ftell(f); fseek(f, 0, SEEK_SET);
    j.raw.resize((size_t)n);
    const bool ok = fread(j.raw.data(), 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    if (!ok || n < (long)(sizeof(Cab) + 2 * sizeof(spu_context))) return false;
    memcpy(&j.cab, j.raw.data(), sizeof(Cab));
    if (memcmp(j.cab.magic, "GCJ1", 4) || j.cab.versao != 2 || j.cab.ctx_bytes != sizeof(spu_context)) return false;
    size_t o = sizeof(Cab);
    j.ctx_in = &j.raw[o]; o += sizeof(spu_context);
    for (uint32_t i = 0; i < j.cab.nev; i++) {
        if (o + sizeof(Ev) > j.raw.size()) return false;
        Job::E e; memcpy(&e.e, &j.raw[o], sizeof(Ev)); o += sizeof(Ev);
        e.d = &j.raw[o]; o += e.e.size;
        j.evs.push_back(e);
    }
    if (o + 4 + sizeof(spu_context) + 4 > j.raw.size() || memcmp(&j.raw[o], "FIM!", 4)) return false;
    j.ctx_out = &j.raw[o + 4];
    memcpy(&j.pend, &j.raw[o + 4 + sizeof(spu_context)], 4);
    return true;
}

/* bytes que o job leu, na memória do guest; de trás para frente, para valer a primeira
 * leitura de cada byte (o que o job escreveu antes de ler de novo ele mesmo reescreve) */
void instala(const Job& j)
{
    for (size_t i = j.evs.size(); i-- > 0;) {
        const Job::E& e = j.evs[i];
        if (e.e.tipo == EV_GET || (e.e.tipo == EV_GETLLAR && !(e.e.flags & EF_FALHOU)))
            memcpy(vm_base + e.e.ea, e.d, e.e.size);
    }
}

void restaura(spu_context* ctx, const uint8_t* snap)
{
    memcpy(ctx, snap, sizeof(spu_context));
    ctx->ls = ctx->ls_store;
    ctx->ch_wait_cv = ctx->ch_wait_lock = nullptr;
    ctx->irq_frame = nullptr;
    ctx->mfc_engine_cache = nullptr;
    ctx->ch_out_mbox.lock = ctx->ch_in_mbox.lock = ctx->ch_out_intr_mbox.lock = 0;
    ctx->ch_sig_notify[0].lock = ctx->ch_sig_notify[1].lock = 0;
}

/* GOWA_CONF_PERF=1: amostra o RIP da thread da reprodução a cada ~20 µs (outra thread a
 * suspende e lê o contexto) e grava <pasta>/perfil.txt: "<rva hex> <amostras>" -- resolver
 * com o gowa.map do build (scripts/perfil_map.py). Com GOWA_CONF_REPS alto (200). */
struct Amostrador {
    HANDLE alvo = nullptr, th = nullptr;
    volatile LONG parar = 0, dentro = 0;   /* só amostra com dentro = 1 (o job rodando) */
    std::unordered_map<uint32_t, uint32_t> hist;
    uintptr_t base = 0;
    static DWORD WINAPI laco(LPVOID a)
    {
        Amostrador* m = (Amostrador*)a;
        LARGE_INTEGER f, t, u; QueryPerformanceFrequency(&f);
        while (!m->parar) {
            if (m->dentro && SuspendThread(m->alvo) != (DWORD)-1) {
                CONTEXT c; c.ContextFlags = CONTEXT_CONTROL;
                if (m->dentro && GetThreadContext(m->alvo, &c))   /* fora do exe (memcpy do CRT, ntdll): 0xFFFFFFFF */
                    m->hist[c.Rip - m->base < 0x10000000u ? (uint32_t)(c.Rip - m->base) : 0xFFFFFFFFu]++;
                ResumeThread(m->alvo);
            }
            QueryPerformanceCounter(&t);
            do { YieldProcessor(); QueryPerformanceCounter(&u); } while ((u.QuadPart - t.QuadPart) * 1000000 / f.QuadPart < 20);
        }
        return 0;
    }
    void liga()
    {
        base = (uintptr_t)GetModuleHandleA(nullptr);
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &alvo, 0, FALSE, DUPLICATE_SAME_ACCESS);
        th = CreateThread(nullptr, 0, laco, this, 0, nullptr);
    }
    void desliga(const std::string& path)
    {
        parar = 1;
        WaitForSingleObject(th, INFINITE);
        if (FILE* f = fopen(path.c_str(), "w")) {
            for (const auto& kv : hist) fprintf(f, "%X %u\n", kv.first, kv.second);
            fclose(f);
        }
    }
};

int replay(const char* dir)
{
    int reps = 5;
    if (const char* e = getenv("GOWA_CONF_REPS")) reps = (std::max)(1, atoi(e));
    std::vector<std::string> arqs;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((std::string(dir) + "/*.gcj").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do arqs.push_back(fd.cFileName); while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    std::sort(arqs.begin(), arqs.end());
    FILE* out = fopen((std::string(dir) + "/conferencia.txt").c_str(), "w");
    auto diz = [&](const char* fmt, auto... a) {
        fprintf(stderr, fmt, a...);
        if (out) { fprintf(out, fmt, a...); fflush(out); }
    };
    Amostrador amostra;
    const bool perf = getenv("GOWA_CONF_PERF") != nullptr;
    if (perf) amostra.liga();
    spu_daz_enter();   /* o MXCSR das threads das SPUs no jogo (DAZ, e RZ no build GOWA_SPU_RZ) */
    spu_context* ctx = (spu_context*)_aligned_malloc(sizeof(spu_context), 64);
    LARGE_INTEGER qf; QueryPerformanceFrequency(&qf);
    int n_ok = 0, n_int = 0, n_dif = 0, n_pulo = 0;
    double soma_us = 0;
    for (const std::string& a : arqs) {
        Job j; j.nome = a;
        if (!carrega((std::string(dir) + "/" + a).c_str(), j)) { diz("%s ilegível (outro build do runtime?)\n", a.c_str()); n_pulo++; continue; }
        if (j.cab.flags) {
            diz("%s pulado:%s%s%s%s\n", a.c_str(), (j.cab.flags & F_CANAL) ? " leu canal" : "",
                (j.cab.flags & F_INTERRUPCAO) ? " interrupção" : "", (j.cab.flags & F_CMD) ? " comando do MFC" : "",
                (j.cab.flags & F_GRANDE) ? " gravação grande demais" : "");
            n_pulo++; continue;
        }
        conf_fn entrada = (conf_fn)spu_lookup(j.cab.entrada, j.cab.img);
        if (entrada == entrada_conf) entrada = s_velha;
        if (!entrada) { diz("%s: nenhuma função traduzida na entrada 0x%X\n", a.c_str(), j.cab.entrada); n_pulo++; continue; }
        Rep rp; rp.j = &j;
        for (size_t i = 0; i < j.evs.size(); i++)
            if (j.evs[i].e.tipo <= EV_PUT || j.evs[i].e.tipo == EV_ZERO) rp.xfers.push_back(i);
        double melhor = 1e30;
        for (int r = 0; r < reps; r++) {
            Rep rr = rp;
            s_rep = &rr;
            instala(j);
            restaura(ctx, j.ctx_in);
            const uint32_t link = ctx->gpr[0]._u32[0];
            LARGE_INTEGER t0, t1;
            volatile uint32_t pend = 2;
            spu_halt_arm_replay(1);
            QueryPerformanceCounter(&t0);
            const uint32_t parar = j.pend == 3 ? (((const spu_context*)j.ctx_out)->pc | 0x80000000u) : 0u;
            amostra.dentro = 1;
            if (setjmp(s_jb) == 0) {
                entrada(ctx);
                pend = drena_ate(ctx, link, nullptr, parar) ? 3u : g_spu_trampoline_fn ? 1u : 0u;
            }
            amostra.dentro = 0;
            QueryPerformanceCounter(&t1);
            spu_halt_arm_replay(0);
            g_spu_trampoline_fn = 0;
            s_rep = nullptr;
            melhor = (std::min)(melhor, (t1.QuadPart - t0.QuadPart) * 1e6 / qf.QuadPart);
            if (r + 1 < reps) { spu_mfc_release(ctx); spu_coh_unregister(ctx); continue; }
            /* --- conferência (na última repetição) --- */
            /* o que o job deixou na memória: as escritas gravadas, em ordem */
            std::unordered_map<uint32_t, uint8_t> esperado;
            for (const Job::E& e : j.evs) {
                const bool esc = e.e.tipo == EV_PUT || e.e.tipo == EV_PUTLLUC || e.e.tipo == EV_ZERO ||
                                 (e.e.tipo == EV_PUTLLC && !(e.e.flags & EF_FALHOU));
                if (!esc) continue;
                for (uint32_t k = 0; k < e.e.size; k++) esperado[e.e.ea + k] = e.e.tipo == EV_ZERO ? 0 : e.d[k];
            }
            uint32_t dif_mem = 0, dif_mem_ea = 0;
            for (const auto& kv : esperado)
                if (vm_base[kv.first] != kv.second && dif_mem++ == 0) dif_mem_ea = kv.first;
            const spu_context* fim = (const spu_context*)j.ctx_out;
            uint32_t dif_ls = 0, dif_ls_a = 0;
            for (uint32_t k = 0; k < SPU_LS_SIZE; k++)
                if (ctx->ls_store[k] != fim->ls_store[k] && dif_ls++ == 0) dif_ls_a = k;
            /* GOWA_CONF_REGS_ABI=1 (EDGE nativo): os temporários r2-r79 não contam -- o código nativo não
             * refaz o lixo que o pipeline da SPU deixa neles, e quem chamou não os lê; saem à parte */
            static const int regs_abi = getenv("GOWA_CONF_REGS_ABI") != nullptr;
            uint32_t dif_reg = 0, dif_tmp = 0; int reg1 = -1;
            for (int k = 0; k < 128; k++)
                if (memcmp(&ctx->gpr[k], &fim->gpr[k], 16)) {
                    if (regs_abi && k >= 2 && k <= 79) { dif_tmp++; continue; }
                    if (dif_reg++ == 0) reg1 = k;
                }
            const bool pc_ok = ((ctx->pc ^ fim->pc) & SPU_LS_MASK) == 0 && pend == j.pend;
            /* interrompido no jogo (uma interrupção assíncrona de outra SPU parou o job ali) e não
             * aqui: confere as escritas e as transferências até onde o jogo foi; a LS e os
             * registradores do fim não são comparáveis */
            const bool interrompido = j.pend == 3 && pend != 3;
            const bool ok = !dif_mem && !rr.diverge && !rr.sem_par && (interrompido || (!dif_ls && !dif_reg && pc_ok));
            ok ? (interrompido ? n_int++ : n_ok++) : n_dif++;
            soma_us += melhor;
            diz("%s %s %8.1f us | transf %u/%u%s | escritas %zu B, %u dif", a.c_str(), ok ? (interrompido ? "OK~" : "OK ") : "DIF", melhor,
                rr.n_xfer, (unsigned)rp.xfers.size(), rr.diverge ? " DIVERGE" : "", esperado.size(), dif_mem);
            if (dif_mem) diz(" (1a em 0x%08X)", dif_mem_ea);
            diz(" | LS %u dif", dif_ls);
            if (dif_ls) diz(" (1a em 0x%05X, sp 0x%05X)", dif_ls_a, fim->gpr[1]._u32[0] & SPU_LS_MASK);
            diz(" | regs %u dif", dif_reg);
            if (dif_reg) diz(" (r%d)", reg1);
            if (dif_tmp) diz(" + %u temporários", dif_tmp);
            if (!pc_ok) diz(" | fim em 0x%05X (saída %u), gravado 0x%05X (saída %u)", ctx->pc & SPU_LS_MASK, (unsigned)pend,
                            fim->pc & SPU_LS_MASK, j.pend);
            if (rr.diverge) diz(" | 1a divergência na transf %d", rr.primeiro_div);
            if (rr.sem_par) diz(" | %u leituras sem par na gravação", rr.sem_par);
            diz("\n");
            spu_mfc_release(ctx); spu_coh_unregister(ctx);
        }
    }
    diz("[confere] %d jobs: %d iguais, %d iguais até a interrupção do jogo, %d diferentes, %d pulados; soma dos menores tempos %.1f ms\n",
        n_ok + n_int + n_dif + n_pulo, n_ok, n_int, n_dif, n_pulo, soma_us / 1000.0);
    if (perf) amostra.desliga(std::string(dir) + "/perfil.txt");
    if (out) fclose(out);
    _aligned_free(ctx);
    return n_dif ? 2 : 0;
}

int s_init = [] {
    if (const char* d = getenv("GOWA_CONF_DIR")) s_dir = d;
    if (getenv("GOWA_CONF_REPLAY")) { g_gowa_conf_on = 2; return 0; }
    const char* e = getenv("GOWA_CONF");
    if (!e || !*e) return 0;
    char* q;
    s_va = (uint32_t)strtoul(e, &q, 16);
    if (*q == ':') s_entrada = (uint32_t)strtoul(q + 1, &q, 16);
    if (*q == ':') s_ini_ms = (ULONGLONG)(strtod(q + 1, &q) * 1000.0);
    s_max = 100;
    if (*q == ':') s_max = (uint32_t)strtoul(q + 1, &q, 10);
    if (*q == ':') s_cada = (std::max)(1u, (uint32_t)strtoul(q + 1, &q, 10));
    if (!s_va || !s_entrada || !s_max) return 0;
    CreateDirectoryA("out", nullptr);
    CreateDirectoryA(s_dir.c_str(), nullptr);
    s_t0 = GetTickCount64();
    return 0;
}();

} // namespace

/* gowa_offline.cpp, ao registrar cada módulo de job: troca a entrada do módulo escolhido */
extern "C" void gowa_conf_registrou(uint32_t va, int img)
{
    if (!s_va || va != s_va) return;
    if (!spu_registry_patch(img, s_entrada, entrada_conf, &s_velha)) {
        fprintf(stderr, "[confere] o módulo 0x%X não tem função na LS 0x%X\n", va, s_entrada);
        return;
    }
    s_img = img;
    g_gowa_conf_on = 1;
    fprintf(stderr, "[confere] gravando %u jobs do 0x%X (entrada 0x%X), a partir de %.0f s, 1 a cada %u, em %s\n",
            s_max, va, s_entrada, s_ini_ms / 1000.0, s_cada, s_dir.c_str());
}

/* spu_dma.h, mfc_do_transfer: cada transferência (as de lista, uma por elemento) */
extern "C" void gowa_conf_xfer(void* vctx, uint32_t lsa, uint64_t ea, uint32_t size, uint32_t cmd)
{
    spu_context* ctx = (spu_context*)vctx;
    const uint32_t c = cmd & 0xFFu, e32 = (uint32_t)ea;
    const uint8_t tipo = (c & 0xF0u) == 0x40u ? EV_GET : (c & 0xF0u) == 0x20u ? EV_PUT : c == 0x89u ? EV_ZERO : 0;
    if (g_gowa_conf_on == 1) {
        Rec* r = t_rec;
        if (!r || r->ctx != ctx) return;
        if (!tipo || (ea >> 32) || e32 >= 0xF0000000u || (lsa & SPU_LS_MASK) + size > SPU_LS_SIZE) { r->flags |= F_CMD; return; }
        const void* d = tipo == EV_GET ? (const void*)(vm_base + e32)
                      : tipo == EV_PUT ? (const void*)(ctx->ls + (lsa & SPU_LS_MASK)) : nullptr;
        ev_add(r, tipo, (uint8_t)c, 0, e32, lsa & SPU_LS_MASK, size, d);
        return;
    }
    Rep* rp = s_rep;
    if (!rp) return;
    const uint32_t k = rp->n_xfer++;
    if (k >= rp->xfers.size() && rp->j->pend == 3) return;
    const bool igual = k < rp->xfers.size() && [&] {
        const Ev& g = rp->j->evs[rp->xfers[k]].e;
        return g.tipo == tipo && g.ea == e32 && g.size == size && g.lsa == (lsa & SPU_LS_MASK);
    }();
    if (!igual && !rp->diverge++) rp->primeiro_div = (int)k;
    /* leitura: a memória recebe o que o job viu no jogo nessa leitura (a próxima gravada com o
     * mesmo endereço e tamanho -- vale também para um código novo que reordene as leituras) */
    if (tipo != EV_GET) return;
    const auto& evs = rp->j->evs;
    for (size_t i = rp->i_get; i < evs.size(); i++)
        if (evs[i].e.tipo == EV_GET && evs[i].e.ea == e32 && evs[i].e.size == size) {
            memcpy(vm_base + e32, evs[i].d, size);
            rp->i_get = i + 1;
            return;
        }
    rp->sem_par++;
}

/* spu_channels.c, antes (fase 0) e depois (fase 1) de cada operação atômica. Gravando: a linha
 * de cada uma, depois. Reproduzindo, antes: o GETLLAR lê a linha que leu no jogo; um PUTLLC que
 * falhou no jogo (outra SPU escreveu na linha) falha de novo -- a memória recebe a linha que o
 * GETLLAR seguinte viu. */
extern "C" void gowa_conf_atom(void* vctx, uint32_t cmd, int fase)
{
    Rec* r = t_rec;
    spu_context* ctx = (spu_context*)vctx;
    const uint32_t ea = ctx->mfc_eal & ~127u, lsa = ctx->mfc_lsa & SPU_LS_MASK & ~127u;
    if (g_gowa_conf_on == 2 && fase == 0) {
        Rep* rp = s_rep;
        if (!rp) return;
        const auto& evs = rp->j->evs;
        const uint8_t tipo = cmd == 0xD0 ? EV_GETLLAR : cmd == 0xB4 ? EV_PUTLLC : EV_PUTLLUC;
        size_t i = rp->i_atom;
        while (i < evs.size() && !(evs[i].e.tipo == tipo && evs[i].e.ea == ea)) i++;
        if (i == evs.size()) { rp->sem_par++; return; }
        rp->i_atom = i + 1;
        if (tipo == EV_GETLLAR) memcpy(vm_base + ea, evs[i].d, 128);
        else if (tipo == EV_PUTLLC && (evs[i].e.flags & EF_FALHOU)) {
            size_t k = i + 1;
            while (k < evs.size() && !(evs[k].e.tipo == EV_GETLLAR && evs[k].e.ea == ea)) k++;
            if (k < evs.size()) memcpy(vm_base + ea, evs[k].d, 128);
        }
        return;
    }
    if (g_gowa_conf_on != 1 || fase != 1 || !r || r->ctx != ctx) return;
    const uint8_t* linha = ctx->ls + lsa;
    switch (cmd) {
    case 0xD0:   /* GETLLAR: a LS já tem a linha que veio da memória */
        r->ultimo_getllar = r->buf.size();
        ev_add(r, EV_GETLLAR, (uint8_t)cmd, 0, ea, lsa, 128, linha);
        break;
    case 0xB4: {   /* PUTLLC */
        const bool falhou = (ctx->atomic_stat & 1u) != 0;
        if (falhou && r->ultimo_getllar != (size_t)-1) {
            Ev e; memcpy(&e, &r->buf[r->ultimo_getllar], sizeof e);
            e.flags |= EF_FALHOU;
            memcpy(&r->buf[r->ultimo_getllar], &e, sizeof e);
        }
        ev_add(r, EV_PUTLLC, (uint8_t)cmd, falhou ? EF_FALHOU : 0, ea, lsa, 128, linha);
        break; }
    default:     /* PUTLLUC / PUTQLLUC */
        ev_add(r, EV_PUTLLUC, (uint8_t)cmd, 0, ea, lsa, 128, linha);
        break;
    }
}

/* spu_channels.c, spu_rdch: o decrementador gravado volta igual na reprodução; caixa de
 * correio, sinais e eventos marcam o job como não reproduzível */
extern "C" uint32_t gowa_conf_rdch(void* vctx, uint32_t ch, uint32_t v)
{
    spu_context* ctx = (spu_context*)vctx;
    if (g_gowa_conf_on == 1) {
        Rec* r = t_rec;
        if (!r || r->ctx != ctx) return v;
        if (ch == SPU_RdDec) ev_add(r, EV_DEC, 0, 0, v, 0, 0, nullptr);
        else if (ch == SPU_RdEventStat) ev_add(r, EV_CANAL, (uint8_t)ch, 0, v, 0, 0, nullptr);
        else if (ch == SPU_RdInMbox || ch == SPU_RdSigNotify1 || ch == SPU_RdSigNotify2) {
            ev_add(r, EV_CANAL, (uint8_t)ch, 0, v, 0, 0, nullptr);
            r->flags |= F_CANAL;
        }
        return v;
    }
    Rep* rp = s_rep;
    if (!rp) return v;
    if (ch == SPU_RdEventStat && rp->canal_pend) { rp->canal_pend = false; return rp->canal_val; }
    if (ch != SPU_RdDec) return v;
    const auto& evs = rp->j->evs;
    while (rp->i_dec < evs.size() && evs[rp->i_dec].e.tipo != EV_DEC) rp->i_dec++;
    return rp->i_dec < evs.size() ? evs[rp->i_dec++].e.ea : v;
}

/* gowa_offline.cpp, gowa_luz_gpu: o id da luz do segmento (contador global de todas as SPUs,
 * marcado nos vértices) -- gravado e devolvido igual na reprodução */
extern "C" uint32_t gowa_conf_luz(void* vctx, uint32_t g)
{
    if (g_gowa_conf_on == 1) {
        Rec* r = t_rec;
        if (r && r->ctx == (spu_context*)vctx) ev_add(r, EV_LUZ, 0, 0, g, 0, 0, nullptr);
        return g;
    }
    Rep* rp = s_rep;
    if (!rp) return g;
    const auto& evs = rp->j->evs;
    while (rp->i_luz < evs.size() && evs[rp->i_luz].e.tipo != EV_LUZ) rp->i_luz++;
    if (rp->i_luz == evs.size()) { rp->sem_par++; return g; }
    return evs[rp->i_luz++].e.ea;
}

/* spu_channels.c, antes de cada salto não local de uma SPU (reinício da pilha do guest, parada) */
extern "C" void gowa_conf_salto(void* vctx)
{
    spu_context* ctx = (spu_context*)vctx;
    if (g_gowa_conf_on == 1) {
        Rec* r = t_rec;
        if (!r || r->ctx != ctx) return;
        t_rec = nullptr;
        fecha(r, ctx, 2);
        delete r;
        return;
    }
    if (s_rep) longjmp(s_jb, 1);
}

/* spu_channels.c, no começo do spu_rdch, só reproduzindo: os eventos que o job leu no jogo (a
 * espera pelo evento de reserva perdida do anel de saída do EDGE) já estão lá, sem bloquear */
extern "C" void gowa_conf_rdch_antes(void* vctx, uint32_t ch)
{
    Rep* rp = s_rep;
    if (!rp || ch != SPU_RdEventStat) return;
    const auto& evs = rp->j->evs;
    while (rp->i_canal < evs.size() && !(evs[rp->i_canal].e.tipo == EV_CANAL && evs[rp->i_canal].e.cmd == SPU_RdEventStat)) rp->i_canal++;
    if (rp->i_canal == evs.size()) { rp->sem_par++; return; }
    rp->canal_val = evs[rp->i_canal++].e.ea;
    rp->canal_pend = true;
    ((spu_context*)vctx)->event_status |= rp->canal_val;
}

/* boot_main_gowa.cpp, depois do registro: GOWA_CONF_REPLAY=<pasta> roda a conferência e sai */
extern "C" int gowa_conf_replay(const char* dir)
{
    return replay(dir);
}
