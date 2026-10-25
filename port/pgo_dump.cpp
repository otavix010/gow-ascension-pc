// GOWA_PGO=gen (CMake): grava o perfil de instrumentação do clang depois de
// GOWA_PGO_DUMP_S segundos (padrão 230, já no trecho pesado da fase 1). As
// rodadas de teste terminam matando o processo, então o atexit do runtime de
// perfil nunca rodaria. O arquivo sai em LLVM_PROFILE_FILE (o runtime de
// perfil lê a variável). Só entra no executável instrumentado.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" int __llvm_profile_write_file(void);

static DWORD WINAPI pgo_dump_thread(LPVOID)
{
    const char* e = getenv("GOWA_PGO_DUMP_S");
    const unsigned s = e ? (unsigned)strtoul(e, nullptr, 10) : 230u;
    Sleep(s * 1000u);
    const int r = __llvm_profile_write_file();
    fprintf(stderr, "[pgo] perfil gravado aos %u s (r=%d, LLVM_PROFILE_FILE=%s)\n", s, r,
            getenv("LLVM_PROFILE_FILE") ? getenv("LLVM_PROFILE_FILE") : "default.profraw");
    return 0;
}

static struct PgoDumpInit {
    PgoDumpInit() { CreateThread(nullptr, 0, pgo_dump_thread, nullptr, 0, nullptr); }
} s_pgo_dump_init;
