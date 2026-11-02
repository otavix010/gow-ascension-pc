/* bancada sintética dos plugins do EDGE: o plugin traduzido (incluído antes) e o nativo sobre as mesmas LS.
 * O arquivo de cada plugin define: PL_ENTRADA (função 0x30000), PL_LACO (a função onde o nativo entra), PL_NATIVO,
 * PL_IMG (caminho da imagem do plugin: os bytes do GOWA.elf) e monta(spu_context*, semente) com a entrada. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>

SPU_THREAD_LOCAL void (*g_spu_trampoline_fn)(spu_context*);
SPU_THREAD_LOCAL int32_t g_spu_hot_reloc;
void spu_indirect_branch(spu_context* c) { (void)c; }
void spu_register_function(uint32_t a, void (*f)(spu_context*)) { (void)a; (void)f; }

static spu_context A, B;
static uint8_t img[0x1000];
static size_t img_n;

static uint32_t rnd_s = 1;
static uint32_t rnd(void) { rnd_s = rnd_s * 1103515245u + 12345u; return (rnd_s >> 8) ^ (rnd_s << 13); }

static void monta(spu_context* c, uint32_t semente);
/* um float "de vértice": quase sempre em [-1,2; 1,2], às vezes bits quaisquer (denormal, grande, NaN) ou zero */
static uint32_t fvert(void)
{
    const uint32_t t = rnd() % 100u;
    if (t < 6) return rnd();
    if (t < 9) return (rnd() & 0x807FFFFFu);   /* denormal */
    if (t < 11) return 0u;
    float f = ((float)(rnd() & 0xFFFFFFu) / 16777216.0f) * 2.4f - 1.2f;
    uint32_t w; memcpy(&w, &f, 4); return w;
}
static void be32(uint8_t* p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }

static void le_img(void)
{
    FILE* f = fopen(PL_IMG, "rb");
    if (!f) { fprintf(stderr, "sem imagem %s\n", PL_IMG); exit(2); }
    img_n = fread(img, 1, sizeof img, f);
    fclose(f);
}

static void zera(spu_context* c)
{
    memset(c, 0, sizeof *c);
    c->ls = c->ls_store;
    for (uint32_t i = 0; i < SPU_LS_SIZE; i += 4) { const uint32_t v = rnd(); memcpy(c->ls_store + i, &v, 4); }
    memcpy(c->ls_store + 0x30000, img, img_n);
    c->host_depth = 1;
}

int main(int argc, char** argv)
{
    const int casos = argc > 1 ? atoi(argv[1]) : 200;
#ifdef SPU_SX_RZ
    _mm_setcsr((_mm_getcsr() & ~0xE040u) | 0x6000u | 0x0040u);   /* como a thread da SPU: DAZ + para zero, sem FTZ */
#else
    _mm_setcsr((_mm_getcsr() & ~0xE040u) | 0x0040u);             /* build nativa: só DAZ */
#endif
    le_img();
    int feitos = 0, recusas = 0, dif = 0, sem_laco = 0;
    for (int k = 0; k < casos; k++) {
        rnd_s = 0x9E3779B9u * (uint32_t)(k + 1);
        zera(&A);
        monta(&A, (uint32_t)k);
        int achou = 0;
        g_spu_trampoline_fn = 0;
#ifdef PL_NA_ENTRADA
        memcpy(&B, &A, sizeof A); B.ls = B.ls_store; achou = 1;
#endif
        PL_ENTRADA(&A);
        while (g_spu_trampoline_fn) {
            void (*f)(spu_context*) = g_spu_trampoline_fn;
            g_spu_trampoline_fn = 0;
            if (f == PL_LACO && !achou) { memcpy(&B, &A, sizeof A); B.ls = B.ls_store; achou = 1; }
            f(&A);
        }
#ifdef BANC_SPILL
        { const uint32_t sp = 0x3F000u - 0x360u; printf("caso %d n=%u:", k, A.gpr[4]._u32[0]); for (uint32_t o = 0x20; o <= 0x50; o += 16) { printf(" "); for (int b = 0; b < 16; b++) printf("%02X", A.ls_store[sp + o + b]); } printf("\n"); }
#endif
        if (!achou) { sem_laco++; continue; }
        if (!PL_NATIVO(&B)) { recusas++; continue; }
        feitos++;
        int d = 0;
        for (uint32_t i = 0; i < SPU_LS_SIZE; i++)
            if (A.ls_store[i] != B.ls_store[i]) {
                if (d < (getenv("BANC_V") ? 100000 : 4)) fprintf(stderr, "caso %d: LS 0x%05X traduzido %02X nativo %02X\n", k, i, A.ls_store[i], B.ls_store[i]);
                d++;
            }
        for (int r = 0; r < 128; r++) {
            if (r != 1 && r != 3 && r < 80) continue;   /* temporários ficam diferentes (como GOWA_CONF_REGS_ABI=1) */
            if (memcmp(&A.gpr[r], &B.gpr[r], 16)) { fprintf(stderr, "caso %d: r%d difere\n", k, r); d++; }
        }
        if (d) dif++;
    }
    printf("%d casos: %d feitos, %d iguais, %d diferentes, %d recusados, %d sem o laço\n", casos, feitos, feitos - dif, dif, recusas, sem_laco);
    return dif ? 1 : 0;
}
