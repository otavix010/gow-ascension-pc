/* Ouroboros, segunda levantada da ponte: o soft lock em que a câmera fica presa e a cena do Oráculo não sai.
 *
 * O que acontece (11/11, bancada com a gravação do report 1011, ~1 em 4 repetições):
 * - Os eventos de animação do Kratos passam pela lista pendente da fila de eventos (func_001FED58):
 *   0x272A = começou a animação r4 (hash), 0x272B = terminou; r7 = o objeto.
 * - Ciclo normal do amuleto: 0x75F9E2DF (erguer) -> 0x9D8D30EE (segurar, em laço) -> a ponte fecha e o roteiro
 *   liga a trava 0 (0x8DCD48) -> 0x75F9E2DF -> o roteiro solta a trava -> 0xB6DC4C64 (abaixar). É o evento
 *   0xB6DC4C64 que o roteiro espera para começar a cena; ela sai no mesmo quadro.
 * - Se o jogador solta o analógico nos últimos quadros antes de a ponte fechar, o Kratos já cancelou o amuleto
 *   (0xDB278381) quando a trava liga; ao soltar a trava ele toca 0xFF486CA2 e vai para o parado (0xBAB45560), e o
 *   0xB6DC4C64 nunca vem: o roteiro fica esperando para sempre com a câmera do amuleto.
 *
 * Correção: nesse caso exato (o objeto estava em 0xDB278381 enquanto a trava 0 estava ligada, e agora, com a trava
 * desligada, começa 0xFF486CA2), posta antes o evento 0xB6DC4C64 com os mesmos argumentos, como no ciclo normal.
 * GOWA_OUROBOROS=0 desliga; GOWA_OUROBOROS_LOG=1 mostra quando age. */
#include "ppu_recomp.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" uint32_t cellGcm_flip_request_count(void);

namespace {
constexpr uint32_t EV_COMECA = 0x272A;
constexpr uint32_t AN_CANCELA = 0xDB278381u;   /* o amuleto cancelado */
constexpr uint32_t AN_SOLTA_FORA = 0xFF486CA2u; /* sair da trava sem estar no amuleto */
constexpr uint32_t AN_ABAIXA = 0xB6DC4C64u;     /* abaixar o amuleto: o que o roteiro espera */
constexpr uint32_t TRAVA0 = 0x8DCD48u;

struct Estado { uint32_t obj, anim; int cancelou_na_trava; };
Estado g_est[8];
int g_lig = -1, g_log = 0;
thread_local int t_dentro = 0;

/* só entra na tabela quem cancelou o amuleto; sai quando toca outra animação */
Estado* achar(uint32_t obj, int criar)
{
    for (auto& e : g_est) if (e.obj == obj) return &e;
    if (criar) for (auto& e : g_est) if (!e.obj) { e = Estado{obj, 0, 0}; return &e; }
    return nullptr;
}
}

/* chamada na entrada de func_001FED58 (todo evento postado); o original segue depois */
extern "C" void gowa_ouroboros_posta(ppu_context* ctx)
{
    if (g_lig < 0) {
        const char* v = getenv("GOWA_OUROBOROS");
        g_lig = !(v && v[0] == '0');
        g_log = getenv("GOWA_OUROBOROS_LOG") != nullptr;
    }
    if (!g_lig || t_dentro) return;
    const int trava = vm_read32(TRAVA0) != 0;
    /* a trava ligada com o objeto no amuleto cancelado (vale qualquer evento postado nesse quadro, de qualquer objeto) */
    if (trava) for (auto& x : g_est) if (x.obj && x.anim == AN_CANCELA) x.cancelou_na_trava = 1;
    if ((uint32_t)ctx->gpr[3] != EV_COMECA) return;
    const uint32_t obj = (uint32_t)ctx->gpr[7], anim = (uint32_t)ctx->gpr[4];
    if (!obj) return;
    Estado* e = achar(obj, anim == AN_CANCELA);
    if (!e) return;
    if (anim == AN_SOLTA_FORA && !trava && e->anim == AN_CANCELA && e->cancelou_na_trava) {
        if (g_log) fprintf(stderr, "[ouroboros] q=%u obj=%08X saiu da trava fora do amuleto: posto 0x%08X antes de 0x%08X\n",
                           cellGcm_flip_request_count(), obj, AN_ABAIXA, AN_SOLTA_FORA);
        ppu_context salvo;
        memcpy(&salvo, ctx, sizeof salvo);
        ctx->gpr[4] = AN_ABAIXA;
        t_dentro = 1;
        func_001FED58(ctx);
        t_dentro = 0;
        memcpy(ctx, &salvo, sizeof salvo);
    }
    if (anim != AN_CANCELA) { *e = Estado{}; return; }
    e->anim = anim;
    if (trava) e->cancelou_na_trava = 1;
}
