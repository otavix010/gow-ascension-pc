/*
 * ps3recomp game project -- game-specific hooks and overrides.
 *
 * main.cpp is the runner and is game-agnostic. This file is where a port puts
 * everything that is not: the PRX loading hook it must answer, overrides for
 * firmware functions the HLE library gets wrong for this title, and patches
 * over recompiled code.
 *
 * It starts out as the empty version of all of that, which is what a game with
 * no lifted PRX and no overrides needs.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>   /* GetModuleFileNameA: caminho padrão da libsre ao lado do exe */

#include "ps3emu/ps3types.h"
#include "ps3emu/error_codes.h"

/* ---------------------------------------------------------------------------
 * PRX loading
 *
 * main.cpp calls this once, after the lifted function table is registered and
 * vm_base is live, before the game runs. A title that lifts a real system PRX
 * -- libsre, which is cellSpurs and cellSync -- loads its image into guest RAM
 * here and registers its exports. Most ports never need to: the HLE library
 * answers those modules instead.
 * -----------------------------------------------------------------------*/

/* GOWA: libsre (cellSpurs/cellSync/cellDaisy...) por LLE. O jogo importa
 * cellSpursInitializeWithAttribute2, CreateTaskset2 etc., que o HLE não tem.
 * A imagem relocada em 0x30000000 (tools/lift_prx.py) é lida do disco; o código
 * lifted (--symbol-prefix libsre_) está em lle_libsre/libsre_recomp.cpp.
 * PS3_LIBSRE=0 desliga (volta ao HLE puro); PS3_LIBSRE_IMAGE troca o caminho. */
#include "prx_loader.h"
#include "ppu_context.h"   /* a ponte PS3_LIBSRE=p mexe nos registradores (r2) */

extern "C" {
struct ppu_context;
extern const prx_func_entry libsre_function_table[];
extern const uint64_t       libsre_function_table_count;
extern const prx_export     libsre_exports[];
extern const uint32_t       libsre_export_count;
void ppu_register_function(uint64_t addr, void (*fn)(ppu_context*));
}

static void gowa_prx_register(uint32_t guest_addr, void (*host)(void*))
{
    ppu_register_function(guest_addr, (void (*)(ppu_context*))host);
}

extern "C" void gowa_early_register(void);   /* gowa_offline.cpp: HLE do port com prioridade */

/* GOWA (03/11, fase 2 do SPURS próprio): PS3_LIBSRE=p ("ponte") = SPURS em HLE (cellSpurs.c), mas
 * com a imagem da libsre carregada e as funções dela traduzidas registradas, SEM os exports. Assim
 * uma função do HLE que ainda não tem implementação própria pode chamar a da libsre real, exata,
 * enquanto o resto é nosso. Primeira: _cellSpursLFQueuePushBody (a fila sem trava que o consumidor
 * na SPU lê direto da memória; o protocolo tem de ser o real). */
extern "C" void ps3_hle_register_ctx(uint32_t nid, const char* name, void (*fn)(ppu_context*));
struct spu_context;
extern "C" void spu_begin_image(int image_id);
extern "C" void gowapm_0079E500_spu_recomp_register(void);
extern "C" void gowapm_0079E500_spu_func_00000A00(void*);
extern "C" void gowa_pm_entrada(spu_context* ctx);   /* gowa_offline.cpp: GOWA_PM_DUMP */
extern "C" void spu_8e4f213617fb468dc09ae1a03a0b333b6f22ccbb4c24ee36d3a0e11c0a94b4d8_spu_func_00003050(spu_context*);
extern "C" void spu_343cf6fab66b578189b842f4e3cb731a476a2df591aa1c8c53c06789c73e341f_spu_func_00003070(spu_context*);
extern "C" void spu_d08b1ad818d026818f1c7cbb2bef448d242a9ae51aab4c7bce69f2ad14308355_spu_func_00003050(spu_context*);
extern "C" void spu_workload_register_img(uint64_t fingerprint, void (*fn)(spu_context*), int image_id,
                                          const char* name);
/* NID -> código da função na libsre (o descritor de cada exportação; fw/lle/libsre_exports.*).
 * A fila sem trava inteira vai pela libsre: a criação do HLE não monta o estado que o envio real
 * confere (o envio devolvia 0x80410909 numa fila criada pelo HLE). */
static struct { uint32_t nid, code; const char* nome; void (*fn)(void*); } s_ponte[] = {
    { 0x011EE38Bu, 0x30017134u, "_cellSpursLFQueueInitialize", nullptr },
    { 0x8A85674Du, 0x300171B8u, "_cellSpursLFQueuePushBody", nullptr },
    { 0x73E06F91u, 0x300178D8u, "cellSpursLFQueueDetachLv2EventQueue", nullptr },
    /* 04/11: o Attach real também -- ele liga a fila de eventos às portas do grupo do SPURS pela syscall
     * connect_event_all_threads, que no modo ponte agora cai na tabela de portas do HLE (lv2_register.c) */
    { 0x1656D49Fu, 0x300174F8u, "cellSpursLFQueueAttachLv2EventQueue", nullptr },
};
template <int K> static void h_ponte(ppu_context* ctx)
{
    const uint32_t a3 = (uint32_t)ctx->gpr[3], a4 = (uint32_t)ctx->gpr[4];
    const uint64_t r2 = ctx->gpr[2];
    ctx->gpr[2] = 0x30039AB0u;            /* TOC da libsre (o descritor da exportação faria isso) */
    s_ponte[K].fn(ctx);
    ctx->gpr[2] = r2;
    static int n = 0;
    if (n++ < 8) fprintf(stderr, "[gowa] ponte libsre: %s(0x%08X, 0x%08X) -> 0x%X\n", s_ponte[K].nome, a3, a4,
                         (uint32_t)ctx->gpr[3]);
}
static void (*const s_ponte_h[])(ppu_context*) = { h_ponte<0>, h_ponte<1>, h_ponte<2>, h_ponte<3> };

extern "C" void ps3_load_prx_modules(void)
{
    gowa_early_register();   /* antes do ppu_sysprx_register: o 1º registro de um NID vence */
    const char* off = getenv("PS3_LIBSRE");
    if (off && off[0] == '0') {
        fprintf(stderr, "[gowa] libsre LLE desligada (PS3_LIBSRE=0)\n");
        return;
    }
    const bool ponte = off && off[0] == 'p';
    const char* path = getenv("PS3_LIBSRE_IMAGE");
    /* sem a variável: fw\lle\libsre_image.bin ao lado da pasta do exe (pacote: build\gowa.exe)
     * ou dois níveis acima (árvore de desenvolvimento: port\build\gowa.exe) */
    static char padrao[1024];
    if (!path || !*path) {
        char exe[MAX_PATH] = {0};
        GetModuleFileNameA(NULL, exe, MAX_PATH);
        char* barra = strrchr(exe, '\\'); if (barra) *barra = 0;
        snprintf(padrao, sizeof padrao, "%s\\..\\fw\\lle\\libsre_image.bin", exe);
        if (GetFileAttributesA(padrao) == INVALID_FILE_ATTRIBUTES)
            snprintf(padrao, sizeof padrao, "%s\\..\\..\\fw\\lle\\libsre_image.bin", exe);
        path = padrao;
    }

    uint32_t size = 0;
    uint8_t* img = prx_image_load_file(path, &size);
    if (!img) {
        fprintf(stderr, "[gowa] libsre: não abriu %s -- seguindo só com HLE\n", path);
        return;
    }
    prx_module m;
    memset(&m, 0, sizeof m);
    m.name         = "libsre";
    m.base         = 0x30000000u;
    m.image        = img;
    m.image_size   = size;
    m.funcs        = libsre_function_table;
    m.func_count   = libsre_function_table_count;
    m.exports      = ponte ? NULL : libsre_exports;
    m.export_count = ponte ? 0u : libsre_export_count;
    prx_load_result r = prx_load_module(&m, gowa_prx_register);
    fprintf(stderr, "[gowa] libsre %s: ok=%d funcs=%u\n", ponte ? "PONTE (sem exports, SPURS em HLE)" : "LLE",
            r.ok, r.funcs_registered);
    if (ponte) {
        static_assert(sizeof s_ponte / sizeof s_ponte[0] == sizeof s_ponte_h / sizeof s_ponte_h[0], "ponte");
        for (size_t k = 0; k < sizeof s_ponte / sizeof s_ponte[0]; k++) {
            for (uint64_t i = 0; i < libsre_function_table_count; i++)
                if (libsre_function_table[i].addr == s_ponte[k].code) s_ponte[k].fn = libsre_function_table[i].func;
            if (s_ponte[k].fn) ps3_hle_register_ctx(s_ponte[k].nid, s_ponte[k].nome, s_ponte_h[k]);
            else fprintf(stderr, "[gowa] ponte libsre: não achei 0x%08X (%s)\n", s_ponte[k].code, s_ponte[k].nome);
        }
        /* O kernel do HLE acha o policy module de cada workload pela impressão digital dos bytes
         * (spu_workload_find_img). O PM do WWS do GoW (EA 0x79E500, 0x3000 bytes) é traduzido à
         * parte (lift_gowa_pm.py) e não estava nessa tabela: "wid=0 PM NOT LIFTED
         * (fp=0xC8D774963A96DCF2)" e nenhum job rodava (tela preta). Mesma imagem do GOWA_PM_NATIVE. */
        const int img = 620000 + 900;   /* GOWA_IMG_JOBMOD_BASE + 900 (gowa_offline.cpp) */
        { const char* pn = getenv("GOWA_PM_NATIVE");   /* com ele, o gowa_offline.cpp já registrou a imagem */
          if (!(pn && pn[0] == '1')) { spu_begin_image(img); gowapm_0079E500_spu_recomp_register(); spu_begin_image(0); } }
        spu_workload_register_img(0xC8D774963A96DCF2ull,
                                  getenv("GOWA_PM_DUMP") ? gowa_pm_entrada
                                                         : (void (*)(spu_context*))gowapm_0079E500_spu_func_00000A00,
                                  img, "gowa PM WWS 0x79E500");
        fprintf(stderr, "[gowa] ponte: PM WWS 0x79E500 registrado no SPURS em HLE (imagem %d)\n", img);
        /* As tasks dos tasksets: o CreateTask do HLE as procura pela impressão digital do ELF
         * (spu_workload_dispatch_async). Elas já estão traduzidas e registradas por endereço como
         * imagens de overlay (gowa_offline.cpp, g_gowa_tasks), mas não pela impressão digital: a
         * task de áudio (ELF 0x6DC300) dava "async dispatch MISS fp=0x00E729804474449A" e nunca
         * rodava, e o jogo esperava por ela (tela preta). A 0x6BA800 (82b4) já está no spu_workloads.c.
         * Impressões calculadas como o runtime (tamanho pelos cabeçalhos do ELF, FNV-1a-64). */
        spu_workload_register_img(0xF4C2A58BDE92CE5Eull, spu_8e4f213617fb468dc09ae1a03a0b333b6f22ccbb4c24ee36d3a0e11c0a94b4d8_spu_func_00003050,
                                  600001, "task 0x6C8500 (8e4f)");
        spu_workload_register_img(0x4DA69B4166D997ADull, spu_343cf6fab66b578189b842f4e3cb731a476a2df591aa1c8c53c06789c73e341f_spu_func_00003070,
                                  600003, "task 0x6D1F80 (343c)");
        spu_workload_register_img(0x00E729804474449Aull, spu_d08b1ad818d026818f1c7cbb2bef448d242a9ae51aab4c7bce69f2ad14308355_spu_func_00003050,
                                  600004, "task 0x6DC300 (d08b, áudio)");
    }
    /* a imagem foi copiada para a RAM do guest; o buffer pode ficar (uma vez só) */
}

/* ---------------------------------------------------------------------------
 * Example: Override a specific NID
 *
 * The module system resolves function imports by NID.  To replace a default
 * HLE stub with custom logic, register your function at init time.
 * -----------------------------------------------------------------------*/

/*
 * Example: override cellGcmSetFlipMode (NID 0xA53D12AE) to force vsync.
 *
 *   static void my_cellGcmSetFlipMode(u32 mode)
 *   {
 *       printf("[hook] cellGcmSetFlipMode: forcing VSYNC (was %u)\n", mode);
 *       // Force vsync regardless of what the game requested
 *       cellGcmSetFlipMode(CELL_GCM_DISPLAY_VSYNC);
 *   }
 *
 * Then in your init code:
 *
 *   ps3::modules::override_nid(0xA53D12AE,
 *       reinterpret_cast<void*>(my_cellGcmSetFlipMode));
 */

/* ---------------------------------------------------------------------------
 * Example: Hook into module loading
 *
 * You can intercept cellSysmoduleLoadModule to perform custom actions
 * when a particular module is loaded.
 * -----------------------------------------------------------------------*/

/*
 *   static s32 my_loadmodule_hook(u16 id)
 *   {
 *       printf("[hook] Module 0x%04X loaded\n", id);
 *
 *       if (id == CELL_SYSMODULE_AUDIO) {
 *           // Perform custom audio backend initialization
 *           init_my_audio_backend();
 *       }
 *
 *       // Call the original implementation
 *       return cellSysmoduleLoadModule(id);
 *   }
 */

/* ---------------------------------------------------------------------------
 * Game-specific patches
 *
 * Sometimes you need to patch recompiled code at specific addresses.
 * Use the patch system to modify behavior without editing generated code.
 * -----------------------------------------------------------------------*/

/*
 *   // Skip an integrity check at guest address 0x00812FA0
 *   ps3::patches::nop_range(0x00812FA0, 0x00812FB0);
 *
 *   // Replace a function call with a direct return
 *   ps3::patches::force_return(0x009A1000, 0);  // return 0
 */
