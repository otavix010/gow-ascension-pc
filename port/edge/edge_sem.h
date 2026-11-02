/* GOWA (02/11, depuração do EDGE nativo): GOWA_EDGE_SEM=<nome>,<nome>,... desliga só os nativos cujo nome (o da
 * função nativa chamada no gancho, ex. fn_000073E0_83e8_nativo) contém um dos pedaços; os outros seguem ligados.
 * Serve para achar, por divisão ao meio, qual nativo muda a imagem numa cena. Lido uma vez por ponto de chamada
 * (GOWA_NAT, nos ganchos do scripts_edge/edge_nativo.py). */
#ifndef GOWA_EDGE_SEM_H
#define GOWA_EDGE_SEM_H
#include <stdlib.h>
#include <string.h>

static int gowa_edge_sem(const char* nome)
{
    const char* e = getenv("GOWA_EDGE_SEM");
    if (!e) return 0;
    while (*e) {
        const char* v = strchr(e, ',');
        const size_t n = v ? (size_t)(v - e) : strlen(e);
        if (n && n < 64) {
            char t[64];
            memcpy(t, e, n); t[n] = 0;
            if (strstr(nome, t)) return 1;
        }
        if (!v) break;
        e = v + 1;
    }
    return 0;
}

#define GOWA_NAT(f, c) ({ static int _sem = -1; if (_sem < 0) _sem = gowa_edge_sem(#f); _sem ? 0 : f(c); })
#endif
