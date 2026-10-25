/*
 * otavix_osd: o medidor padrão dos ports Otavix (ver otavix_osd.h).
 *
 * Uma thread própria acha a janela principal do processo (a maior visível), cria uma janela
 * popup em camadas DONA dela (fica sempre logo acima do jogo, some quando ele minimiza, não
 * fica por cima de outros programas), transparente ao mouse e que nunca pega o foco, e a
 * cada 100 ms acompanha a posição e o tamanho da área do jogo. O texto é desenhado com GDI
 * (Segoe UI Semibold, suavizado em tons de cinza) num bitmap de 32 bits e entregue por
 * UpdateLayeredWindow com alfa por pixel: branco sobre um fundo preto a 55%.
 *
 * A thread se declara "per-monitor DPI aware v2": as medidas saem em pixels reais da tela,
 * mesmo que o jogo não seja DPI aware, e o texto não é esticado pelo Windows.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "otavix_osd.h"

#define OSD_FONTE_1080   16     /* altura da fonte (px) quando a área do jogo tem 1080 linhas */
#define OSD_MARGEM_1080  10     /* distância do canto */
#define OSD_FOLGA_1080   6      /* espaço entre o texto e a borda do fundo */
#define OSD_FUNDO_ALFA   140    /* fundo preto a ~55% */

static CRITICAL_SECTION s_cs;
static wchar_t s_texto[256];
static LONG s_versao;           /* muda a cada texto novo */
static LONG s_iniciado;

void otx_osd_texto(const char *linha)
{
    wchar_t w[256];
    w[0] = 0;
    if (linha && *linha)
        if (!MultiByteToWideChar(CP_UTF8, 0, linha, -1, w, 256)) w[0] = 0;
    if (!InterlockedCompareExchange(&s_iniciado, 0, 0)) return;
    EnterCriticalSection(&s_cs);
    if (wcscmp(w, s_texto) != 0) {
        wcscpy(s_texto, w);
        InterlockedIncrement(&s_versao);
    }
    LeaveCriticalSection(&s_cs);
}

/* a maior janela visível deste processo que não é a do OSD */
typedef struct { HWND melhor; LONG area; } Busca;
static const wchar_t CLASSE[] = L"OtavixOSD";

static BOOL CALLBACK busca_janela(HWND h, LPARAM lp)
{
    Busca *b = (Busca *)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(h) || GetWindow(h, GW_OWNER)) return TRUE;
    wchar_t cls[32];
    if (GetClassNameW(h, cls, 32) && wcscmp(cls, CLASSE) == 0) return TRUE;
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

/* desenha o texto e devolve o bitmap (premultiplicado) no DC de memória; o tamanho vai em w e h */
static HBITMAP desenha(HDC mem, const wchar_t *txt, int px, int folga, int *w, int *h)
{
    HFONT f = CreateFontW(-px, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                          CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS,
                          L"Segoe UI");
    HGDIOBJ f0 = SelectObject(mem, f);
    RECT r = { 0, 0, 0, 0 };
    DrawTextW(mem, txt, -1, &r, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
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
    DrawTextW(mem, txt, -1, &t, DT_SINGLELINE | DT_NOPREFIX);
    /* branco sobre preto -> cobertura; texto branco por cima do fundo preto translúcido */
    uint32_t *p = (uint32_t *)bits;
    for (int i = 0, n = *w * *h; i < n; i++) {
        const uint32_t c = p[i];
        uint32_t cob = c & 0xFF;
        if (((c >> 8) & 0xFF) > cob) cob = (c >> 8) & 0xFF;
        if (((c >> 16) & 0xFF) > cob) cob = (c >> 16) & 0xFF;
        const uint32_t a = cob + OSD_FUNDO_ALFA * (255 - cob) / 255;
        p[i] = (a << 24) | (cob << 16) | (cob << 8) | cob;
    }
    SelectObject(mem, f0);
    DeleteObject(f);
    return bmp;
}

static DWORD WINAPI thread_osd(LPVOID arg)
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

    HWND jogo = NULL, osd = NULL;
    HDC tela = GetDC(NULL);
    HDC mem = CreateCompatibleDC(tela);
    HBITMAP bmp = NULL;
    LONG versao_feita = -1;
    int px_feito = 0, bw = 0, bh = 0, visivel = 0;
    POINT pos_feita = { -99999, -99999 };

    for (;;) {
        MSG m;
        while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&m);
        MsgWaitForMultipleObjects(0, NULL, FALSE, 100, QS_ALLINPUT);

        wchar_t txt[256];
        EnterCriticalSection(&s_cs);
        wcscpy(txt, s_texto);
        const LONG versao = s_versao;
        LeaveCriticalSection(&s_cs);

        if (!jogo || !IsWindow(jogo)) {
            if (osd) { DestroyWindow(osd); osd = NULL; visivel = 0; }
            jogo = txt[0] ? acha_jogo() : NULL;
            versao_feita = -1;
        }
        RECT cr;
        if (!txt[0] || !jogo || IsIconic(jogo) || !GetClientRect(jogo, &cr) || cr.bottom < 16) {
            if (osd && visivel) { ShowWindow(osd, SW_HIDE); visivel = 0; }
            continue;
        }
        if (!osd) {
            osd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                  CLASSE, L"", WS_POPUP, 0, 0, 1, 1, jogo, NULL, wc.hInstance, NULL);
            if (!osd) { jogo = NULL; continue; }
            versao_feita = -1;
        }
        const double esc = cr.bottom / 1080.0;
        int px = (int)lround(OSD_FONTE_1080 * esc);
        if (px < 11) px = 11;
        const int folga = (int)lround(OSD_FOLGA_1080 * esc) > 2 ? (int)lround(OSD_FOLGA_1080 * esc) : 2;
        const int margem = (int)lround(OSD_MARGEM_1080 * esc);
        POINT pos = { margem, margem };
        ClientToScreen(jogo, &pos);

        if (versao != versao_feita || px != px_feito) {
            HBITMAP novo = desenha(mem, txt, px, folga, &bw, &bh);
            if (!novo) continue;
            if (bmp) DeleteObject(bmp);
            bmp = novo;
            versao_feita = versao;
            px_feito = px;
            pos_feita.x = -99999;                 /* força o UpdateLayeredWindow */
        }
        if (pos.x != pos_feita.x || pos.y != pos_feita.y || !visivel) {
            SIZE sz = { bw, bh };
            POINT zero = { 0, 0 };
            BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
            SelectObject(mem, bmp);
            UpdateLayeredWindow(osd, tela, &pos, &sz, mem, &zero, 0, &bf, ULW_ALPHA);
            pos_feita = pos;
            if (!visivel) {
                ShowWindow(osd, SW_SHOWNOACTIVATE);
                visivel = 1;
            }
        }
    }
    return 0;
}

void otx_osd_iniciar(void)
{
    static LONG uma;
    if (InterlockedExchange(&uma, 1)) return;
    InitializeCriticalSection(&s_cs);
    s_texto[0] = 0;
    InterlockedExchange(&s_iniciado, 1);
    HANDLE t = CreateThread(NULL, 0, thread_osd, NULL, 0, NULL);
    if (t) CloseHandle(t);
}
