/* GOWA (22/10): iluminação por vértice do EDGE na GPU (opção (a) do PROGRESSO.md).
 *
 * O job 0x72DF80 (o montador por desenho do EDGE) junta as luzes de cada segmento numa luz
 * dominante por vértice (0x73E0) e soma a última luz à parte (0x6AC8): saídas A (float3,
 * atributo 7), B/C (half3, 8/14) e D (half3, 15) -- modelo conferido bit a bit em
 * out/spudiff/modelo_73e0.h e modelo_6ac8.h. Com GOWA_LUZ_GPU=1 a SPU pula essa conta:
 * guarda as luzes do segmento num anel daqui (gowa_luz_anel) e marca o atributo 7 de cada
 * vértice com (GOWA_LUZ_MAGICO, id, verificação); o motor live reconhece a marca depois de
 * buscar os vértices, sobe as luzes num CBV (b2) e usa a variante do vertex shader com o
 * prólogo que refaz A/B/C/D a partir da posição (atributo 0) e da normal N = (T × B)·T.w
 * (atributos 4 e 5). */
#pragma once
#include <stdint.h>

#define GOWA_LUZ_MAX     40          /* luzes por segmento (as do 0x73E0 + as da 0x6AC8) */
#define GOWA_LUZ_ANEL    8192        /* conjuntos guardados (~2 quadros de segmentos com luz) */
#define GOWA_LUZ_MAGICO  0x7F61E0A5u /* bits de A.x: ~3e38, nunca uma posição de luz real */

/* layout igual ao cbuffer EdgeLuz do prólogo (rsx_vp_decompiler.c) */
typedef struct gowa_luz_set {
    float info[4];                   /* nfixa (direção da lista), nluz (juntas na dominante), n6 (somadas à parte), acc inicial da 6AC8 */
    float info2[4];                  /* ganho, 0, 0, 0 */
    float p[GOWA_LUZ_MAX][4];        /* posição.xyz, 1/raio */
    float c[GOWA_LUZ_MAX][4];        /* cor.rgb, intensidade (sem o ganho) */
    float d[GOWA_LUZ_MAX][4];        /* direção.xyz (só as nfixa primeiras) */
} gowa_luz_set;                      /* 32 + 3 * 640 = 1952 bytes */

typedef struct gowa_luz_slot {
    volatile uint32_t gen;           /* conferência: a marca leva (gen & 0xFFFF) */
    volatile uint32_t visto;         /* = gen quando o motor live reconheceu o segmento */
    uint32_t pad[2];
    gowa_luz_set s;
} gowa_luz_slot;

#ifdef __cplusplus
extern "C" {
#endif
extern gowa_luz_slot g_gowa_luz_anel[GOWA_LUZ_ANEL];
extern volatile long g_gowa_luz_prox;
extern int g_gowa_luz_gpu;           /* GOWA_LUZ_GPU=1 */
#ifdef __cplusplus
}
#endif
