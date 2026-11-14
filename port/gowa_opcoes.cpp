/* GOWA (13/11): opções do jogo -- desligar efeitos gráficos e trapaças -- por ganchos por instrução.
 *
 * O tradutor do PPU (--insn-hooks, dados/ganchos_ppu.json) põe `if (!ppu_insn_gancho(ctx, END)) { <instrução> }` só
 * nos endereços da lista; o gancho roda antes da instrução e, devolvendo 1, ela não roda. Com tudo desligado o gancho
 * só confere uma tabela e devolve 0: o jogo roda o próprio código. A lista confere a palavra original de cada
 * endereço: num executável diferente do BCUS98232 a tradução para em vez de enganchar no lugar errado.
 *
 * Os pontos foram achados no nosso executável (BCUS98232) pelas mesmas assinaturas que o Ascension-R (MIT, para a
 * versão digital NPUA80918 1.12) descreve -- campos da criatura, vtables por RTTI, a palavra do MLAA --, com os
 * endereços do nosso executável e conferidos instrução por instrução. A ideia é deles; o código é nosso.
 *
 * Variáveis (o launcher grava; 0/ausente = desligado):
 *   GOWA_SEM_MLAA=1      o jogo não pede o MLAA às SPUs (a palavra 0x83CE74 = 0 e as duas chamadas em 0x390724 e
 *                        0x390754 puladas): libera as SPUs, além do desvio do MLAA na GPU (GOWA_MLAA_BYPASS).
 *                        MUDA A IMAGEM mesmo com o bypass: some o brilho difuso em volta das luzes e a cena fica
 *                        mais escura (RTX 5080, 14/11: fundo parado difere ~3,5 contra ~0,6 entre rodadas iguais;
 *                        portão 105 -> 98 de brilho). Por isso é opção, nunca padrão.
 *   GOWA_SEM_DOF=1       sem profundidade de campo  (fxDoFProcess, 0x4342BC: o "ligado" lido como 0)
 *   GOWA_SEM_MBLUR=1     sem motion blur            (fxMotionBlurProcess, 0x437810)
 *   GOWA_SEM_SOMBRAS=1   sem sombras dinâmicas      (fxNewShadows, 0x36F864)
 *   GOWA_DANO_RECEBIDO=<f>  multiplica o dano que o Kratos leva (0 = vida infinita; 0,25; 0,5)
 *   GOWA_DANO_CAUSADO=<f>|mata  multiplica o dano que o Kratos causa (2, 5) ou mata com um golpe
 *   GOWA_ORBES_MAX=1, GOWA_MAGIA_INF=1, GOWA_ITENS_INF=1, GOWA_FURIA_INF=1  medidores sempre cheios
 *   GOWA_PULO_INF=1      pulos no ar sem limite
 */
#include "ppu_recomp.h"
#include "vm.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

/* vtables do nosso executável (RTTI) */
constexpr uint32_t VT_JOGADOR = 0x6890D8u;     /* goPlayer */
constexpr uint32_t VT_PASSIVO = 0x6954E0u;     /* goPassiveAI (objetos) */
constexpr uint32_t VT_NEOIO = 0x695388u;       /* goNeoIO (objetos) */
/* campos da criatura */
constexpr uint32_t C_VIDA = 0x214u, C_VIDA_MAX = 0x218u, C_CONTROLE = 0x1304u;
constexpr uint32_t BIT_PULO_USADO = 0x4u;      /* em +0x1628: o pulo no ar já foi usado */
constexpr uint32_t MLAA_PALAVRA = 0x83CE74u;

struct Opcoes {
    int sem_mlaa, sem_dof, sem_mblur, sem_sombras;
    float dano_recebido;                        /* 1 = normal */
    float dano_causado;                         /* 1 = normal */
    int mata, orbes, magia, itens, furia, pulo;
};

int liga(const char* n)
{
    const char* e = getenv(n);
    return e && *e && *e != '0';
}

float escala(const char* n, float padrao, float lo, float hi)
{
    const char* e = getenv(n);
    if (!e || !*e) return padrao;
    char b[32]; size_t k = 0;
    for (; e[k] && k + 1 < sizeof b; k++) b[k] = e[k] == ',' ? '.' : e[k];   /* "0,5" também vale */
    b[k] = 0;
    char* fim = nullptr;
    const float v = strtof(b, &fim);
    if (fim == b || !std::isfinite(v)) return padrao;
    return v < lo ? lo : v > hi ? hi : v;
}

const Opcoes& opcoes()
{
    static Opcoes o;
    static int lido = 0;
    if (!lido) {
        o.sem_mlaa = liga("GOWA_SEM_MLAA");
        o.sem_dof = liga("GOWA_SEM_DOF");
        o.sem_mblur = liga("GOWA_SEM_MBLUR");
        o.sem_sombras = liga("GOWA_SEM_SOMBRAS");
        o.dano_recebido = escala("GOWA_DANO_RECEBIDO", 1.0f, 0.0f, 1.0f);
        const char* c = getenv("GOWA_DANO_CAUSADO");
        o.mata = c && (strcmp(c, "mata") == 0 || strcmp(c, "kill") == 0);
        o.dano_causado = o.mata ? 1.0f : escala("GOWA_DANO_CAUSADO", 1.0f, 1.0f, 100.0f);
        o.orbes = liga("GOWA_ORBES_MAX");
        o.magia = liga("GOWA_MAGIA_INF");
        o.itens = liga("GOWA_ITENS_INF");
        o.furia = liga("GOWA_FURIA_INF");
        o.pulo = liga("GOWA_PULO_INF");
        lido = 1;
        fprintf(stderr, "[opcoes] sem: mlaa=%d dof=%d mblur=%d sombras=%d | dano recebido x%.2f, causado %s%.2f | "
                        "orbes=%d magia=%d itens=%d furia=%d pulo=%d\n",
                o.sem_mlaa, o.sem_dof, o.sem_mblur, o.sem_sombras, o.dano_recebido, o.mata ? "mata " : "x",
                o.dano_causado, o.orbes, o.magia, o.itens, o.furia, o.pulo);
    }
    return o;
}

/* 14/11: quantas vezes cada opção agiu de fato, no log a cada 30 s (só com alguma opção ligada e algo contado):
 * mostra no log do jogador que a trapaça está funcionando, e serve de prova nos testes */
enum { K_DOF, K_MBLUR, K_SOMBRAS, K_MLAA, K_DANO_RECEBIDO, K_DANO_CAUSADO, K_ORBES, K_MAGIA, K_ITENS, K_FURIA, K_PULO, K_N };
const char* const K_NOMES[K_N] = { "dof", "mblur", "sombras", "mlaa", "dano recebido", "dano causado", "orbes",
                                   "magia", "itens", "furia", "pulo" };
std::atomic<uint32_t> g_conta[K_N];
std::atomic<uint32_t> g_chamadas;
std::atomic<uint64_t> g_ultimo_log;

void conta(int k)
{
    g_conta[k].fetch_add(1u, std::memory_order_relaxed);
}

/* a cada 1024 ganchos olha o relógio; passados 30 s, uma linha com o que agiu (e zera) */
void talvez_registra()
{
    if ((g_chamadas.fetch_add(1u, std::memory_order_relaxed) & 1023u) != 0u) return;
    const uint64_t agora = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    uint64_t antes = g_ultimo_log.load(std::memory_order_relaxed);
    if (!antes) {
        g_ultimo_log.compare_exchange_strong(antes, agora);
        return;
    }
    if (agora - antes < 30000u || !g_ultimo_log.compare_exchange_strong(antes, agora)) return;
    char linha[384];
    int n = snprintf(linha, sizeof linha, "[opcoes] 30 s:");
    int algo = 0;
    for (int k = 0; k < K_N && n > 0 && n < (int)sizeof linha; k++) {
        const uint32_t v = g_conta[k].exchange(0u, std::memory_order_relaxed);
        if (!v) continue;
        algo = 1;
        n += snprintf(linha + n, sizeof linha - (size_t)n, " %s %u", K_NOMES[k], v);
    }
    if (algo) fprintf(stderr, "%s\n", linha);
}

/* um objeto do jogo que dá para ler: alinhado e dentro da memória do guest */
bool ea_ok(uint32_t ea, uint32_t tam)
{
    return ea && !(ea & 3u) && vm_is_valid_addr(ea) && vm_is_valid_addr(ea + tam - 1u);
}

/* a vtable do controlador da criatura (0 = não é uma criatura que dê para ler) */
uint32_t vt_controle(uint32_t criatura)
{
    if (!ea_ok(criatura, C_CONTROLE + 4u)) return 0;
    const uint32_t c = vm_read32(criatura + C_CONTROLE);
    return ea_ok(c, 4u) ? vm_read32(c) : 0u;
}

float le_f32(uint32_t ea)
{
    const uint32_t w = vm_read32(ea);
    float f; memcpy(&f, &w, 4);
    return f;
}

/* f1 = dano. Arredonda para float, como o lfs/fmuls do jogo. */
void escala_dano(ppu_context* ctx, float k)
{
    const float d = (float)ctx->fpr[1];
    if (d > 0.0f) ctx->fpr[1] = (double)(d * k);
}

/* o atacante (r31) é o Kratos e a vítima (r3) uma criatura que não é jogador nem objeto */
void dano_causado(ppu_context* ctx, const Opcoes& o)
{
    if (vt_controle((uint32_t)ctx->gpr[31]) != VT_JOGADOR) return;
    const uint32_t vitima = (uint32_t)ctx->gpr[3];
    const uint32_t vt = vt_controle(vitima);
    if (!vt || vt == VT_JOGADOR || vt == VT_PASSIVO || vt == VT_NEOIO) return;
    conta(K_DANO_CAUSADO);
    if (o.mata) {
        const float vida = le_f32(vitima + C_VIDA), max = le_f32(vitima + C_VIDA_MAX);
        if (std::isfinite(vida) && std::isfinite(max) && (float)ctx->fpr[1] > 0.0f)
            ctx->fpr[1] = (double)(vida + max + 1.0f);
    } else {
        escala_dano(ctx, o.dano_causado);
    }
}

/* pulo: o bit "já usado" sai do registrador antes do teste, só para o Kratos (a memória fica como está) */
void pulo(ppu_context* ctx, int reg, int criatura)
{
    if (vt_controle((uint32_t)ctx->gpr[criatura]) == VT_JOGADOR) {
        conta(K_PULO);
        ctx->gpr[reg] &= ~(uint64_t)BIT_PULO_USADO;
    }
}

}  // namespace

/* chamado pelo boot antes de o jogo rodar: a palavra do MLAA (só se ainda for a do executável, 1) */
extern "C" void gowa_opcoes_boot(void)
{
    const Opcoes& o = opcoes();
    if (o.sem_mlaa) {
        const uint32_t v = vm_read32(MLAA_PALAVRA);
        if (v == 1u) vm_write32(MLAA_PALAVRA, 0u);
        fprintf(stderr, "[opcoes] MLAA do jogo: palavra 0x%08X era %u%s\n", MLAA_PALAVRA, v, v == 1u ? ", agora 0" : " (não mexi)");
    }
}

int ppu_insn_gancho(ppu_context* ctx, uint32_t addr)
{
    const Opcoes& o = opcoes();
    talvez_registra();
    switch (addr) {
    /* efeitos: o "ligado" do processo lido como 0 (lwz rN,0x20(r31) -> li rN,0) */
    case 0x004342BCu: if (o.sem_dof) { conta(K_DOF); ctx->gpr[5] = 0; return 1; } return 0;
    case 0x00437810u: if (o.sem_mblur) { conta(K_MBLUR); ctx->gpr[5] = 0; return 1; } return 0;
    case 0x0036F864u: if (o.sem_sombras) { conta(K_SOMBRAS); ctx->gpr[4] = 0; return 1; } return 0;
    /* MLAA: as duas chamadas que o pedem (bl 0x3903D8) */
    case 0x00390724u:
    case 0x00390754u:
        if (o.sem_mlaa) conta(K_MLAA);
        return o.sem_mlaa;
    /* dano: entrada da função de dano (r3 criatura, f1 dano) -- o que o Kratos recebe */
    case 0x000873A0u:
        if (o.dano_recebido != 1.0f && vt_controle((uint32_t)ctx->gpr[3]) == VT_JOGADOR) {
            conta(K_DANO_RECEBIDO);
            escala_dano(ctx, o.dano_recebido);
        }
        return 0;
    /* dano: as duas chamadas com atacante (r31) -- o que o Kratos causa */
    case 0x001553ACu:
    case 0x00152FB8u:
        if (o.mata || o.dano_causado != 1.0f) dano_causado(ctx, o);
        return 0;
    /* medidores: depois da chamada virtual que devolve o máximo em f1, o valor novo (f31) vira o máximo */
    case 0x002437ECu: if (o.orbes) { conta(K_ORBES); ctx->fpr[31] = ctx->fpr[1]; } return 0;   /* tPowerUpMeter */
    case 0x00244F04u: if (o.magia) { conta(K_MAGIA); ctx->fpr[31] = ctx->fpr[1]; } return 0;   /* tMagicMeterPlayer */
    case 0x0024417Cu: if (o.itens) { conta(K_ITENS); ctx->fpr[31] = ctx->fpr[1]; } return 0;   /* tItemMeter */
    case 0x00243A2Cu: if (o.furia) { conta(K_FURIA); ctx->fpr[31] = ctx->fpr[1]; } return 0;   /* tRageMeter */
    /* pulos: antes do teste do bit "já usado" de +0x1628 */
    case 0x000B623Cu: if (o.pulo) pulo(ctx, 4, 31); return 0;
    case 0x0026C6ACu: if (o.pulo) pulo(ctx, 3, 23); return 0;
    case 0x0026DDE4u: if (o.pulo) pulo(ctx, 3, 31); return 0;
    default: return 0;
    }
}
