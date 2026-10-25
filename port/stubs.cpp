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

extern "C" void ps3_load_prx_modules(void)
{
    gowa_early_register();   /* antes do ppu_sysprx_register: o 1º registro de um NID vence */
    const char* off = getenv("PS3_LIBSRE");
    if (off && off[0] == '0') {
        fprintf(stderr, "[gowa] libsre LLE desligada (PS3_LIBSRE=0)\n");
        return;
    }
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
    m.exports      = libsre_exports;
    m.export_count = libsre_export_count;
    prx_load_result r = prx_load_module(&m, gowa_prx_register);
    fprintf(stderr, "[gowa] libsre LLE: ok=%d funcs=%u\n", r.ok, r.funcs_registered);
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
