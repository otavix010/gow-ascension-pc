/*
 * otavix_legenda: a legenda das teclas do God of War: Ascension (pedido de jogador: o jogo só mostra os botões do
 * controle do PS3). Um painel no canto de baixo à esquerda da janela do jogo, do mesmo jeito que o medidor padrão
 * (otavix_osd.c): janela própria em camadas, dona da do jogo, transparente ao mouse, sem pegar o foco, texto na
 * resolução real da tela no tamanho da referência de 1080p (Segoe UI Semibold, branco sobre preto a 55%).
 *
 * F1 mostra e esconde; a primeira tecla de jogo mostra por 12 s (cellPad.c). Não aparece sobre tela cheia exclusiva.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "otavix_legenda.h"

#define LEG_FONTE_1080   16     /* altura da fonte (px) quando a área do jogo tem 1080 linhas */
#define LEG_MARGEM_1080  10     /* distância do canto */
#define LEG_FOLGA_1080   8      /* espaço entre o texto e a borda do fundo */
#define LEG_FUNDO_ALFA   140    /* fundo preto a ~55% */

/* 18/11: o texto segue o mapa de teclas do jogador. As mesmas variáveis e os mesmos padrões de cellPad.c
 * (GOWA_TECLAS="id=nomes;id=nomes", GOWA_MOUSE): a legenda mostra até duas teclas de cada botão. */
static wchar_t TEXTO[1024];

static const struct { const char *id, *padrao; } LEG_PADRAO[] = {
    { "cross", "space" }, { "circle", "e backspace mouse3" }, { "square", "mouse1" }, { "triangle", "mouse2" },
    { "l1", "q" }, { "r1", "f" }, { "l2", "c" }, { "r2", "r" }, { "l3", "z" }, { "r3", "v" },
    { "start", "escape enter" }, { "select", "tab" },
    { "up", "up" }, { "down", "down" }, { "left", "left" }, { "right", "right" },
    { "stick_up", "w" }, { "stick_down", "s" }, { "stick_left", "a" }, { "stick_right", "d" },
    { "rstick_up", "i" }, { "rstick_down", "k" }, { "rstick_left", "j" }, { "rstick_right", "l" },
};

/* os nomes do launcher para um botão: os da GOWA_TECLAS, senão os do padrão ("" = nenhuma tecla) */
static void leg_nomes(const char *id, char *sai, size_t cap)
{
    sai[0] = 0;
    const size_t nid = strlen(id);
    for (const char *e = getenv("GOWA_TECLAS"); e && *e;) {
        const char *fim = strchr(e, ';');
        const size_t tam = fim ? (size_t)(fim - e) : strlen(e);
        if (tam > nid && e[nid] == '=' && !_strnicmp(e, id, nid)) {
            size_t n = tam - nid - 1;
            if (n >= cap) n = cap - 1;
            memcpy(sai, e + nid + 1, n);
            sai[n] = 0;
            return;
        }
        e = fim ? fim + 1 : NULL;
    }
    for (size_t k = 0; k < sizeof LEG_PADRAO / sizeof LEG_PADRAO[0]; k++)
        if (!strcmp(LEG_PADRAO[k].id, id)) { strncpy(sai, LEG_PADRAO[k].padrao, cap - 1); sai[cap - 1] = 0; return; }
}

/* um nome do launcher como o jogador o lê; as letras e os números pela posição, com o layout do teclado dele */
static void leg_tecla(const char *nome, wchar_t *sai, size_t cap)
{
    static const struct { const char *nome; const wchar_t *texto; } fixos[] = {
        { "space", L"Espaço" }, { "escape", L"Esc" }, { "esc", L"Esc" }, { "enter", L"Enter" }, { "tab", L"Tab" },
        { "backspace", L"Backspace" }, { "capslock", L"Caps Lock" }, { "lshift", L"Shift esq." }, { "rshift", L"Shift dir." },
        { "lctrl", L"Ctrl esq." }, { "rctrl", L"Ctrl dir." }, { "lalt", L"Alt esq." }, { "ralt", L"Alt dir." },
        { "up", L"\x2191" }, { "down", L"\x2193" }, { "left", L"\x2190" }, { "right", L"\x2192" },
        { "mouse1", L"mouse esquerdo" }, { "mouse2", L"mouse direito" }, { "mouse3", L"botão do meio" },
        { "mouse4", L"mouse lateral 1" }, { "mouse5", L"mouse lateral 2" },
        { "home", L"Home" }, { "end", L"End" }, { "pageup", L"Page Up" }, { "pagedown", L"Page Down" },
        { "insert", L"Insert" }, { "delete", L"Delete" }, { "numenter", L"Enter (num.)" },
    };
    /* scan codes (conjunto 1) das teclas de letra, número e pontuação: o caractere vem do layout */
    static const struct { const char *nome; UINT scan; } pos[] = {
        { "1", 0x02 }, { "2", 0x03 }, { "3", 0x04 }, { "4", 0x05 }, { "5", 0x06 }, { "6", 0x07 }, { "7", 0x08 }, { "8", 0x09 },
        { "9", 0x0A }, { "0", 0x0B }, { "minus", 0x0C }, { "equals", 0x0D }, { "q", 0x10 }, { "w", 0x11 }, { "e", 0x12 },
        { "r", 0x13 }, { "t", 0x14 }, { "y", 0x15 }, { "u", 0x16 }, { "i", 0x17 }, { "o", 0x18 }, { "p", 0x19 },
        { "lbracket", 0x1A }, { "rbracket", 0x1B }, { "a", 0x1E }, { "s", 0x1F }, { "d", 0x20 }, { "f", 0x21 }, { "g", 0x22 },
        { "h", 0x23 }, { "j", 0x24 }, { "k", 0x25 }, { "l", 0x26 }, { "semicolon", 0x27 }, { "apostrophe", 0x28 },
        { "grave", 0x29 }, { "backslash", 0x2B }, { "z", 0x2C }, { "x", 0x2D }, { "c", 0x2E }, { "v", 0x2F }, { "b", 0x30 },
        { "n", 0x31 }, { "m", 0x32 }, { "comma", 0x33 }, { "period", 0x34 }, { "slash", 0x35 }, { "iso", 0x56 }, { "abnt", 0x73 },
    };
    for (size_t k = 0; k < sizeof fixos / sizeof fixos[0]; k++)
        if (!_stricmp(fixos[k].nome, nome)) { wcsncpy(sai, fixos[k].texto, cap - 1); sai[cap - 1] = 0; return; }
    for (size_t k = 0; k < sizeof pos / sizeof pos[0]; k++)
        if (!_stricmp(pos[k].nome, nome)) {
            const UINT vk = MapVirtualKeyW(pos[k].scan, 1 /* MAPVK_VSC_TO_VK */);
            const UINT ch = vk ? (MapVirtualKeyW(vk, 2 /* MAPVK_VK_TO_CHAR */) & 0x7FFF) : 0;
            if (ch > 32) { sai[0] = (wchar_t)towupper((wint_t)ch); sai[1] = 0; return; }
            break;
        }
    size_t i = 0;                            /* o resto (F1..F12, teclado numérico): o próprio nome, em maiúsculas */
    for (; nome[i] && i + 1 < cap; i++) sai[i] = (wchar_t)towupper((wint_t)(unsigned char)nome[i]);
    sai[i] = 0;
}

/* "Tecla" ou "Tecla ou Tecla" de um botão (um travessão se não tem tecla); sem o mouse, os botões dele saem */
static void leg_botao(const char *id, int mouse, wchar_t *sai, size_t cap)
{
    char nomes[128];
    leg_nomes(id, nomes, sizeof nomes);
    sai[0] = 0;
    int n = 0;
    for (char *p = strtok(nomes, " \t"); p && n < 2; p = strtok(NULL, " \t")) {
        if (!mouse && !_strnicmp(p, "mouse", 5)) continue;
        wchar_t t[32];
        leg_tecla(p, t, sizeof t / sizeof t[0]);
        if (n) wcsncat(sai, L" ou ", cap - wcslen(sai) - 1);
        wcsncat(sai, t, cap - wcslen(sai) - 1);
        n++;
    }
    if (!n) wcsncpy(sai, L"\x2014", cap - 1);
}

static void leg_monta(void)
{
    static int feito;
    if (feito) return;
    feito = 1;
    const char *m = getenv("GOWA_MOUSE");
    const int mouse = !(m && *m == '0');
    static wchar_t b[24][72];
    static const char *ids[24] = { "cross", "circle", "square", "triangle", "l1", "r1", "l2", "r2", "l3", "r3", "start", "select",
                                   "up", "down", "left", "right", "stick_up", "stick_left", "stick_down", "stick_right",
                                   "rstick_up", "rstick_left", "rstick_down", "rstick_right" };
    for (int k = 0; k < 24; k++) leg_botao(ids[k], mouse, b[k], 72);
    _snwprintf(TEXTO, sizeof TEXTO / sizeof TEXTO[0] - 1,
               L"Teclado e mouse  (F1 mostra e esconde)\n"
               L"Andar: %ls %ls %ls %ls        Analógico direito: %ls%ls %ls %ls %ls\n"
               L"Cruz (X): %ls        Quadrado: %ls\n"
               L"Triângulo: %ls        Círculo: %ls\n"
               L"L1: %ls    R1: %ls    L2: %ls    R2: %ls    L3: %ls    R3: %ls\n"
               L"Direcional: %ls %ls %ls %ls    Start: %ls    Select: %ls",
               b[16], b[17], b[18], b[19], mouse ? L"mouse ou " : L"", b[20], b[21], b[22], b[23],
               b[0], b[2], b[3], b[1], b[4], b[5], b[6], b[7], b[8], b[9], b[12], b[14], b[13], b[15], b[10], b[11]);
    TEXTO[sizeof TEXTO / sizeof TEXTO[0] - 1] = 0;
}

static volatile LONG s_mostrar;
static LONG s_iniciado;
static const wchar_t CLASSE[] = L"OtavixLegenda";

void otx_legenda_mostrar(int sim)
{
    InterlockedExchange(&s_mostrar, sim ? 1 : 0);
}

/* a maior janela visível deste processo sem dona (o jogo; o medidor e esta são donas dele) */
typedef struct { HWND melhor; LONG area; } Busca;

static BOOL CALLBACK busca_janela(HWND h, LPARAM lp)
{
    Busca *b = (Busca *)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(h) || GetWindow(h, GW_OWNER)) return TRUE;
    RECT r;
    if (!GetClientRect(h, &r)) return TRUE;
    const LONG a = (r.right - r.left) * (r.bottom - r.top);
    if (a > b->area) { b->area = a; b->melhor = h; }
    return TRUE;
}

static HWND acha_jogo(void)
{
    Busca b = { NULL, 64 * 64 };
    EnumWindows(busca_janela, (LPARAM)&b);
    return b.melhor;
}

/* desenha o painel e devolve o bitmap (premultiplicado); o tamanho vai em w e h */
static HBITMAP desenha(HDC mem, int px, int folga, int *w, int *h)
{
    HFONT f = CreateFontW(-px, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                          CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS,
                          L"Segoe UI");
    HGDIOBJ f0 = SelectObject(mem, f);
    leg_monta();
    RECT r = { 0, 0, 0, 0 };
    DrawTextW(mem, TEXTO, -1, &r, DT_CALCRECT | DT_NOPREFIX);
    *w = (r.right - r.left) + 2 * folga;
    *h = (r.bottom - r.top) + 2 * folga;
    BITMAPINFO bi;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = *w;
    bi.bmiHeader.biHeight = -*h;             /* de cima para baixo */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp || !bits) {
        SelectObject(mem, f0);
        DeleteObject(f);
        if (bmp) DeleteObject(bmp);
        return NULL;
    }
    SelectObject(mem, bmp);
    memset(bits, 0, (size_t)*w * (size_t)*h * 4);
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, RGB(255, 255, 255));
    RECT t = { folga, folga, *w - folga, *h - folga };
    DrawTextW(mem, TEXTO, -1, &t, DT_NOPREFIX);
    /* branco sobre preto -> cobertura; texto branco por cima do fundo preto translúcido */
    uint32_t *p = (uint32_t *)bits;
    for (int i = 0, n = *w * *h; i < n; i++) {
        const uint32_t c = p[i];
        uint32_t cob = c & 0xFF;
        if (((c >> 8) & 0xFF) > cob) cob = (c >> 8) & 0xFF;
        if (((c >> 16) & 0xFF) > cob) cob = (c >> 16) & 0xFF;
        const uint32_t a = cob + LEG_FUNDO_ALFA * (255 - cob) / 255;
        p[i] = (a << 24) | (cob << 16) | (cob << 8) | cob;
    }
    SelectObject(mem, f0);
    DeleteObject(f);
    return bmp;
}

static DWORD WINAPI thread_legenda(LPVOID arg)
{
    (void)arg;
    /* pixels reais da tela, mesmo com o jogo "DPI unaware" (Windows 10 1607+) */
    typedef void *(WINAPI *SetCtx)(void *);
    SetCtx set = (SetCtx)(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"),
                                                "SetThreadDpiAwarenessContext");
    if (set) set((void *)(intptr_t)-4);      /* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 */

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = CLASSE;
    RegisterClassExW(&wc);

    HWND jogo = NULL, painel = NULL;
    HDC tela = GetDC(NULL);
    HDC mem = CreateCompatibleDC(tela);
    HBITMAP bmp = NULL;
    int px_feito = 0, bw = 0, bh = 0, visivel = 0;
    POINT pos_feita = { -99999, -99999 };

    for (;;) {
        MSG m;
        while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&m);
        MsgWaitForMultipleObjects(0, NULL, FALSE, 100, QS_ALLINPUT);

        const int mostrar = InterlockedCompareExchange(&s_mostrar, 0, 0) != 0;
        if (!jogo || !IsWindow(jogo)) {
            if (painel) { DestroyWindow(painel); painel = NULL; visivel = 0; }
            jogo = mostrar ? acha_jogo() : NULL;
        }
        RECT cr;
        if (!mostrar || !jogo || IsIconic(jogo) || !GetClientRect(jogo, &cr) || cr.bottom < 16) {
            if (painel && visivel) { ShowWindow(painel, SW_HIDE); visivel = 0; }
            continue;
        }
        if (!painel) {
            painel = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                     CLASSE, L"", WS_POPUP, 0, 0, 1, 1, jogo, NULL, wc.hInstance, NULL);
            if (!painel) { jogo = NULL; continue; }
            px_feito = 0;
        }
        const double esc = cr.bottom / 1080.0;
        int px = (int)lround(LEG_FONTE_1080 * esc);
        if (px < 11) px = 11;
        const int folga = (int)lround(LEG_FOLGA_1080 * esc) > 3 ? (int)lround(LEG_FOLGA_1080 * esc) : 3;
        const int margem = (int)lround(LEG_MARGEM_1080 * esc);
        if (px != px_feito) {
            HBITMAP novo = desenha(mem, px, folga, &bw, &bh);
            if (!novo) continue;
            if (bmp) DeleteObject(bmp);
            bmp = novo;
            px_feito = px;
            pos_feita.x = -99999;                 /* força o UpdateLayeredWindow */
        }
        POINT pos = { margem, cr.bottom - margem - bh };
        ClientToScreen(jogo, &pos);
        if (pos.x != pos_feita.x || pos.y != pos_feita.y || !visivel) {
            SIZE sz = { bw, bh };
            POINT zero = { 0, 0 };
            BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
            SelectObject(mem, bmp);
            UpdateLayeredWindow(painel, tela, &pos, &sz, mem, &zero, 0, &bf, ULW_ALPHA);
            pos_feita = pos;
            if (!visivel) {
                ShowWindow(painel, SW_SHOWNOACTIVATE);
                visivel = 1;
            }
        }
    }
    return 0;
}

void otx_legenda_iniciar(void)
{
    if (InterlockedExchange(&s_iniciado, 1)) return;
    HANDLE t = CreateThread(NULL, 0, thread_legenda, NULL, 0, NULL);
    if (t) CloseHandle(t);
}
