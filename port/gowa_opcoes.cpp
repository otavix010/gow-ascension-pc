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
 *   GOWA_SEM_MLAA=1      (padrão quando GOWA_MLAA_BYPASS=1, desde 14/11) o jogo não pede o MLAA às SPUs (a palavra 0x83CE74 = 0 e as duas chamadas em 0x390724 e
 *                        0x390754 puladas): libera as SPUs, além do desvio do MLAA na GPU (GOWA_MLAA_BYPASS).
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

/* GOWA_MLAA_BYPASS: o boot_main_gowa.cpp põe o padrão "1" com SetEnvironmentVariableA, que o getenv da CRT do
 * Windows não enxerga (só o do Wine) -- então lê pelo sistema */
#ifdef _WIN32
extern "C" __declspec(dllimport) unsigned long __stdcall GetEnvironmentVariableA(const char*, char*, unsigned long);
#endif
int mlaa_bypass()
{
#ifdef _WIN32
    char b[8] = {0};
    return GetEnvironmentVariableA("GOWA_MLAA_BYPASS", b, sizeof b) && b[0] == '1';
#else
    const char* e = getenv("GOWA_MLAA_BYPASS");
    return e && e[0] == '1';
#endif
}

const Opcoes& opcoes()
{
    static Opcoes o;
    static int lido = 0;
    if (!lido) {
        /* 14/11: com o bypass (padrão), a imagem já sai sem o MLAA e ninguém lê o que as SPUs calculam; então o
         * pedido do MLAA às SPUs sai junto, sozinho (Legion 15 W: +6%, imagem igual). GOWA_SEM_MLAA=0/1 explícito
         * manda; GOWA_MLAA_BYPASS=0 (MLAA de volta) deixa as SPUs fazerem o MLAA. */
        o.sem_mlaa = getenv("GOWA_SEM_MLAA") ? liga("GOWA_SEM_MLAA") : mlaa_bypass();
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
    if (vt_controle((uint32_t)ctx->gpr[criatura]) == VT_JOGADOR) ctx->gpr[reg] &= ~(uint64_t)BIT_PULO_USADO;
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
    switch (addr) {
    /* efeitos: o "ligado" do processo lido como 0 (lwz rN,0x20(r31) -> li rN,0) */
    case 0x004342BCu: if (o.sem_dof) { ctx->gpr[5] = 0; return 1; } return 0;
    case 0x00437810u: if (o.sem_mblur) { ctx->gpr[5] = 0; return 1; } return 0;
    case 0x0036F864u: if (o.sem_sombras) { ctx->gpr[4] = 0; return 1; } return 0;
    /* MLAA: as duas chamadas que o pedem (bl 0x3903D8) */
    case 0x00390724u:
    case 0x00390754u: return o.sem_mlaa;
    /* dano: entrada da função de dano (r3 criatura, f1 dano) -- o que o Kratos recebe */
    case 0x000873A0u:
        if (o.dano_recebido != 1.0f && vt_controle((uint32_t)ctx->gpr[3]) == VT_JOGADOR) escala_dano(ctx, o.dano_recebido);
        return 0;
    /* dano: as duas chamadas com atacante (r31) -- o que o Kratos causa */
    case 0x001553ACu:
    case 0x00152FB8u:
        if (o.mata || o.dano_causado != 1.0f) dano_causado(ctx, o);
        return 0;
    /* medidores: depois da chamada virtual que devolve o máximo em f1, o valor novo (f31) vira o máximo */
    case 0x002437ECu: if (o.orbes) ctx->fpr[31] = ctx->fpr[1]; return 0;   /* tPowerUpMeter */
    case 0x00244F04u: if (o.magia) ctx->fpr[31] = ctx->fpr[1]; return 0;   /* tMagicMeterPlayer */
    case 0x0024417Cu: if (o.itens) ctx->fpr[31] = ctx->fpr[1]; return 0;   /* tItemMeter */
    case 0x00243A2Cu: if (o.furia) ctx->fpr[31] = ctx->fpr[1]; return 0;   /* tRageMeter */
    /* pulos: antes do teste do bit "já usado" de +0x1628 */
    case 0x000B623Cu: if (o.pulo) pulo(ctx, 4, 31); return 0;
    case 0x0026C6ACu: if (o.pulo) pulo(ctx, 3, 23); return 0;
    case 0x0026DDE4u: if (o.pulo) pulo(ctx, 3, 31); return 0;
    default: return 0;
    }
}
