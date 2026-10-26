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
#include <string.h>

#include "otavix_legenda.h"

#define LEG_FONTE_1080   16     /* altura da fonte (px) quando a área do jogo tem 1080 linhas */
#define LEG_MARGEM_1080  10     /* distância do canto */
#define LEG_FOLGA_1080   8      /* espaço entre o texto e a borda do fundo */
#define LEG_FUNDO_ALFA   140    /* fundo preto a ~55% */

/* o mapa de cellPad.c (pad_poll_teclado_mouse), na ordem em que o jogo pede os botões */
static const wchar_t TEXTO[] =
    L"Teclado e mouse  (F1 mostra e esconde)\n"
    L"WASD: andar        Shift + WASD: esquivar\n"
    L"Cruz (X): Espaço        Quadrado: mouse esquerdo\n"
    L"Triângulo: mouse direito        Círculo: E\n"
    L"L1: Q    R1: F    L2: C    R2: R    L3: Z    R3: V\n"
    L"Direcional: setas    Start: Esc ou Enter    Select: Tab";

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
