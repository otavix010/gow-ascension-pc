/*
 * ps3recomp - cellPad HLE implementation
 *
 * Reads real gamepad input from the host and translates to PS3 pad format.
 *
 * Backend selection:
 *   - Windows default: XInput (no extra dependencies)
 *   - Everywhere else / if PS3RECOMP_PAD_USE_SDL2 is defined: SDL2 GameController
 *
 * Define PS3RECOMP_PAD_USE_SDL2 to force SDL2 backend on Windows.
 */

#include <time.h>
#include "cellPad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../../runtime/platform/win32_compat.h"   /* GetTickCount64 on POSIX */
#include "../../runtime/ppu/ppu_memory.h"   /* GUEST_PTR, vm_write*: guest EA -> host */

/* ---------------------------------------------------------------------------
 * Backend selection
 * -----------------------------------------------------------------------*/

#if defined(PS3RECOMP_PAD_USE_SDL2)
  #define PAD_BACKEND_SDL2  1
  #define PAD_BACKEND_XINPUT 0
#elif defined(_WIN32)
  #define PAD_BACKEND_SDL2  0
  #define PAD_BACKEND_XINPUT 1
#else
  #define PAD_BACKEND_SDL2  1
  #define PAD_BACKEND_XINPUT 0
#endif

#if PAD_BACKEND_XINPUT
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <xinput.h>
  #pragma comment(lib, "xinput.lib")
#endif

#if PAD_BACKEND_SDL2
  #include <SDL2/SDL.h>
#endif

/* ---------------------------------------------------------------------------
 * Internal state
 * -----------------------------------------------------------------------*/

#define PAD_MAX_HOST_PORTS  4  /* XInput supports max 4; SDL may support more */

typedef struct {
    int  connected;
    u16  buttons;           /* CELL_PAD_CTRL_* bitmask */
    u8   analog_lx;         /* 0-255, center=128 */
    u8   analog_ly;
    u8   analog_rx;
    u8   analog_ry;
    u8   trigger_l2;        /* 0-255 */
    u8   trigger_r2;        /* 0-255 */
    /* Pressure-sensitive face buttons (0-255) */
    u8   press_right;
    u8   press_left;
    u8   press_up;
    u8   press_down;
    u8   press_triangle;
    u8   press_circle;
    u8   press_cross;
    u8   press_square;
    u8   press_l1;
    u8   press_r1;
} PadHostState;

static int           s_pad_initialized = 0;
static u32           s_max_connect = 0;
static u32           s_port_setting[CELL_PAD_MAX_PORT_NUM];
static PadHostState  s_host_state[PAD_MAX_HOST_PORTS];

/* GOWA (26/10, report 37): what the player used last, the keyboard and mouse (1) or a controller (0), for the HUD's
 * button pictures (rsx_live_draw.c swaps the PS3 buttons for the keys). Before any input: the keyboard when no
 * controller is plugged in. */
static volatile int s_ultimo_teclado = -1;
static volatile int s_xinput_ligado;
int cellPad_teclado_ultimo(void)
{
    /* tests: GOWA_TESTE_TECLADO=1 (or 0) fixes the answer, for a run with no real key pressed; "1:<s>" turns to
     * the controller <s> seconds after the first call (the swap back to the PS3 buttons) */
    static int teste = -2;
    static ULONGLONG troca;
    if (teste == -2) {
        const char* e = getenv("GOWA_TESTE_TECLADO");
        teste = e ? atoi(e) != 0 : -1;
        const char* c = e ? strchr(e, ':') : NULL;
        if (c) troca = GetTickCount64() + (ULONGLONG)atoi(c + 1) * 1000u;
    }
    if (teste >= 0) return troca && GetTickCount64() >= troca ? 0 : teste;
    const int u = s_ultimo_teclado;
    return u >= 0 ? u : !s_xinput_ligado;
}
/* Per-port "a report has arrived since your last read" flag; see cellPadGetData. */
static int s_data_fresh[PAD_MAX_HOST_PORTS];

#if PAD_BACKEND_SDL2
static SDL_GameController* s_sdl_controllers[PAD_MAX_HOST_PORTS];
static int s_sdl_inited = 0;
#endif

/* ---------------------------------------------------------------------------
 * XInput backend
 * -----------------------------------------------------------------------*/

#if PAD_BACKEND_XINPUT

/* Deadzone for analog sticks (same as XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) */
#define PAD_STICK_DEADZONE  7849
#define PAD_TRIGGER_THRESHOLD 30

/* GOWA (26/10): one stick, both axes together (XInput orientation, Y up; the caller flips Y), 1..255 around 128.
 * The deadzone was applied to each axis on its own: with XInput's round gate a stick pushed fully to a diagonal
 * read only 61% on each axis, and the dodge (a flick of the right stick) failed on diagonals -- a player's "the
 * roll does not respond every time". Now a radial deadzone, rescaled to the full range, and RPCS3's squircle
 * (PadHandlerBase::ConvertToSquircle, its default factor 8000) toward the DualShock 3's squarer range: a full
 * diagonal reads 80% per axis. GOWA_SQUIRCLE=<factor> changes it (smaller = squarer), 0 turns it off. */
static void pad_xinput_stick(short raw_x, short raw_y, short deadzone, u8* out_x, u8* out_y)
{
    static int fator = -1;
    if (fator < 0) { const char* e = getenv("GOWA_SQUIRCLE"); fator = (e && *e) ? atoi(e) : 8000; if (fator < 0) fator = 0; }
    float x = (float)raw_x / 32767.0f, y = (float)raw_y / 32767.0f;
    const float m = sqrtf(x * x + y * y), dz = (float)deadzone / 32767.0f;
    if (m <= dz) { *out_x = *out_y = 128; return; }
    float len = ((m > 1.0f ? 1.0f : m) - dz) / (1.0f - dz);
    const float ang = atan2f(y, x);
    if (fator > 0) { const float s = sinf(2.0f * ang); len *= 1.0f + s * s / ((float)fator / 1000.0f); }
    int vx = 128 + (int)lroundf(len * cosf(ang) * 127.0f), vy = 128 + (int)lroundf(len * sinf(ang) * 127.0f);
    /* 1..255, symmetric around 128: the caller's 256 - y must not wrap a 0 into 0 */
    *out_x = (u8)(vx < 1 ? 1 : vx > 255 ? 255 : vx);
    *out_y = (u8)(vy < 1 ? 1 : vy > 255 ? 255 : vy);
}

/* GOWA (26/10): a tap or a flick the game would miss. The pad was read only when the game asked, once a frame:
 * at 15-25 fps a quick press, or a flick of the right stick (the dodge) that is back at the centre before the next
 * read, never reached the game. Two parts, both stamped with the game's frame count (cellGcm_flip_request_count):
 *  - the game's reads (controller and keyboard together): a press, or the right stick off the centre, that the last
 *    read saw and this one does not is still reported until two frames have begun since it began (or 250 ms), so
 *    the game's logic sees it at least once; a long hold is released as soon as it ends;
 *  - a sampler thread reads the controller every ~4 ms and keeps what began AND ended between two game reads; the
 *    next read reports it, and from there the rule above holds it.
 * Only port 0, the player. GOWA_PAD_AMOSTRA=0 turns both off. */
extern u32 cellGcm_flip_request_count(void);
#define PAD_FLICK_MIN 64          /* right stick off the centre by half its range, either axis */
typedef struct { int on; u8 x, y; u32 flip; ULONGLONG ms; } PadFlick;
static SRWLOCK   s_tap_lock = SRWLOCK_INIT;
/* the game's reads */
static u16       s_tap_last;        /* buttons at the last read */
static u32       s_tap_flip[16];    /* the frame each button's press began in */
static ULONGLONG s_tap_ms[16];
static PadFlick  s_flick;           /* the right stick at the last read */
/* the sampler's (controller only) */
static u16       s_amo_last, s_amo_pend;   /* presses begun since the last game read */
static u32       s_amo_flip[16];
static ULONGLONG s_amo_ms[16];
static PadFlick  s_amo_flick;       /* the right stick at the last sample */
static int       s_amo_flick_pend;  /* a flick begun since the last game read */
static PadFlick  s_amo_flick_ini;
static volatile LONG s_amostra_viva;
static HANDLE    s_amostra_thread;

static int pad_desvio(u8 x, u8 y)
{
    const int dx = x > 128 ? x - 128 : 128 - x, dy = y > 128 ? y - 128 : 128 - y;
    return dx > dy ? dx : dy;
}

static int pad_amostra_ligada(void)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_PAD_AMOSTRA"); on = (e && *e == '0') ? 0 : 1; }
    return on;
}

/* a read of the right stick: a flick that begins now is stamped; the latest direction kept while it is out */
static void pad_flick_ver(PadFlick* f, u8 x, u8 y, u32 flip, ULONGLONG ms)
{
    if (pad_desvio(x, y) >= PAD_FLICK_MIN) {
        if (!f->on) { f->flip = flip; f->ms = ms; }
        f->on = 1; f->x = x; f->y = y;
    } else f->on = 0;
}

/* the game's read of port 0, after the controller and the keyboard */
static void pad_tap_segurar(PadHostState* hs)
{
    if (!pad_amostra_ligada() || !hs->connected) return;
    const u32 flip = cellGcm_flip_request_count();
    const ULONGLONG ms = GetTickCount64();
    u16 extra = 0;
    AcquireSRWLockExclusive(&s_tap_lock);
    /* what the sampler saw begin and end since the last read, as if this read had seen it */
    const u16 perdidos = (u16)(s_amo_pend & ~hs->buttons & ~s_tap_last);
    for (int b = 0; b < 16; b++)
        if (perdidos & (1u << b)) { s_tap_flip[b] = s_amo_flip[b]; s_tap_ms[b] = s_amo_ms[b]; }
    u16 antes = (u16)(s_tap_last | perdidos);
    s_amo_pend = 0;
    if (s_amo_flick_pend && !s_flick.on && pad_desvio(hs->analog_rx, hs->analog_ry) < PAD_FLICK_MIN)
        s_flick = s_amo_flick_ini;
    s_amo_flick_pend = 0;
    /* presses that begin now */
    const u16 novos = (u16)(hs->buttons & ~antes);
    for (int b = 0; b < 16; b++)
        if (novos & (1u << b)) { s_tap_flip[b] = flip; s_tap_ms[b] = ms; }
    /* short presses that ended: still down */
    for (int b = 0; b < 16; b++) {
        const u16 bit = (u16)(1u << b);
        if ((antes & bit) && !(hs->buttons & bit) && flip - s_tap_flip[b] < 2 && ms - s_tap_ms[b] < 250) extra |= bit;
    }
    s_tap_last = (u16)(hs->buttons | extra);
    /* the right stick: a short flick that ended stays out */
    const PadFlick f = s_flick;
    pad_flick_ver(&s_flick, hs->analog_rx, hs->analog_ry, flip, ms);
    if (f.on && !s_flick.on && flip - f.flip < 2 && ms - f.ms < 250) {
        hs->analog_rx = f.x; hs->analog_ry = f.y;
        s_flick = f;
    }
    ReleaseSRWLockExclusive(&s_tap_lock);
    if (!extra) return;
    hs->buttons |= extra;
    if (extra & CELL_PAD_CTRL_TRIANGLE) hs->press_triangle = 255;
    if (extra & CELL_PAD_CTRL_CIRCLE)   hs->press_circle = 255;
    if (extra & CELL_PAD_CTRL_CROSS)    hs->press_cross = 255;
    if (extra & CELL_PAD_CTRL_SQUARE)   hs->press_square = 255;
    if (extra & CELL_PAD_CTRL_L1)       hs->press_l1 = 255;
    if (extra & CELL_PAD_CTRL_R1)       hs->press_r1 = 255;
    if (extra & CELL_PAD_CTRL_L2)       hs->trigger_l2 = 255;
    if (extra & CELL_PAD_CTRL_R2)       hs->trigger_r2 = 255;
}

static u16 pad_xinput_botoes(const XINPUT_GAMEPAD* gp);

static DWORD WINAPI pad_amostra_func(LPVOID arg)
{
    (void)arg;
    while (InterlockedCompareExchange(&s_amostra_viva, 1, 1)) {
        XINPUT_STATE st;
        memset(&st, 0, sizeof st);
        if (XInputGetState(0, &st) != ERROR_SUCCESS) { Sleep(250); continue; }   /* no controller: look now and then */
        u8 rx, ry;
        pad_xinput_stick(st.Gamepad.sThumbRX, st.Gamepad.sThumbRY, PAD_STICK_DEADZONE, &rx, &ry);
        ry = (u8)(256 - ry);
        const u16 btns = pad_xinput_botoes(&st.Gamepad);
        const u32 flip = cellGcm_flip_request_count();
        const ULONGLONG ms = GetTickCount64();
        AcquireSRWLockExclusive(&s_tap_lock);
        const u16 novos = (u16)(btns & ~s_amo_last);
        for (int b = 0; b < 16; b++)
            if (novos & (1u << b)) { s_amo_flip[b] = flip; s_amo_ms[b] = ms; }
        s_amo_pend |= novos;
        s_amo_last = btns;
        const int estava = s_amo_flick.on;
        pad_flick_ver(&s_amo_flick, rx, ry, flip, ms);
        if (s_amo_flick.on) {
            if (!estava) s_amo_flick_pend = 1;
            if (s_amo_flick_pend) s_amo_flick_ini = s_amo_flick;   /* the latest direction of the flick */
        }
        ReleaseSRWLockExclusive(&s_tap_lock);
        Sleep(4);
    }
    return 0;
}

/* Keyboard fallback for port 0.
 *
 * The only backend here is XInput, so on a machine with no controller plugged
 * in a title gets a pad that is reported present and never presses anything --
 * it can be watched but not played. That is what The Simpsons Arcade Game hit:
 * it reached its attract loop and no input existed to start a game.
 *
 * Only fills in for port 0, only when XInput found nothing there, so a real
 * controller always wins and nothing changes for a port that has one. Keys are
 * read only while a window of THIS process is in the foreground, so typing in
 * another application does not drive the game. Set PAD_NO_KEYBOARD=1 to
 * disable it entirely.
 *
 * Arrows = d-pad, Z/X/A/S = cross/circle/square/triangle, Q/W = L1/R1,
 * 1/2 = L2/R2, Enter = START, Tab = SELECT. The left stick mirrors the d-pad
 * so a title that reads the stick instead is playable too. */
#ifdef _WIN32
static int pad_host_window_focused(void)
{
    HWND fg = GetForegroundWindow();
    if (!fg) return 0;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

static void pad_poll_keyboard(void)
{
    static int off = -1;
    if (off < 0) off = getenv("PAD_NO_KEYBOARD") ? 1 : 0;
    if (off) return;

    PadHostState* hs = &s_host_state[0];
    if (!pad_host_window_focused()) {
        /* Release everything on focus loss, or a key held while alt-tabbing
         * would stay down forever. */
        hs->buttons = 0;
        hs->analog_lx = hs->analog_ly = 128;
        hs->analog_rx = hs->analog_ry = 128;
        hs->connected = 1;
        return;
    }

    static const struct { int vk; u16 btn; } map[] = {
        { VK_UP,     CELL_PAD_CTRL_UP },      { VK_DOWN,  CELL_PAD_CTRL_DOWN },
        { VK_LEFT,   CELL_PAD_CTRL_LEFT },    { VK_RIGHT, CELL_PAD_CTRL_RIGHT },
        { 'Z',       CELL_PAD_CTRL_CROSS },   { 'X',      CELL_PAD_CTRL_CIRCLE },
        { 'A',       CELL_PAD_CTRL_SQUARE },  { 'S',      CELL_PAD_CTRL_TRIANGLE },
        { 'Q',       CELL_PAD_CTRL_L1 },      { 'W',      CELL_PAD_CTRL_R1 },
        { '1',       CELL_PAD_CTRL_L2 },      { '2',      CELL_PAD_CTRL_R2 },
        { VK_RETURN, CELL_PAD_CTRL_START },   { VK_TAB,   CELL_PAD_CTRL_SELECT },
    };

    u16 btns = 0;
    for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++)
        if (GetAsyncKeyState(map[i].vk) & 0x8000) btns |= map[i].btn;

    hs->buttons   = btns;
    hs->connected = 1;
    hs->analog_lx = (u8)((btns & CELL_PAD_CTRL_LEFT) ? 0 :
                         (btns & CELL_PAD_CTRL_RIGHT) ? 255 : 128);
    hs->analog_ly = (u8)((btns & CELL_PAD_CTRL_UP) ? 0 :
                         (btns & CELL_PAD_CTRL_DOWN) ? 255 : 128);
    hs->analog_rx = hs->analog_ry = 128;
    hs->trigger_l2 = (u8)((btns & CELL_PAD_CTRL_L2) ? 255 : 0);
    hs->trigger_r2 = (u8)((btns & CELL_PAD_CTRL_R2) ? 255 : 0);

    { static int said = 0;
      if (!said && btns) { said = 1;
          printf("[cellPad] keyboard fallback active on port 0 (no XInput device)\n");
          fflush(stdout); } }
}
#endif

#ifdef _WIN32
/* GOWA (25/10): teclado e mouse de jogo de PC, SOMADOS ao controle no port 0 (pedido
 * de jogador). Só com a janela do jogo em foco. GOWA_TECLADO=0 volta ao modo de
 * emergência acima (só sem controle, setas + Z/X/A/S).
 *   WASD = analógico esquerdo        Shift + WASD = analógico direito (esquiva)
 *   Espaço = X (pular)                mouse esquerdo = quadrado (ataque leve)
 *   mouse direito = triângulo (forte) E, Backspace ou botão do meio = círculo
 *   Q = L1   F = R1   C = L2   R = R2   Z = L3   V = R3   setas = direcional
 *   Esc ou Enter = Start (pausa)      Tab = Select */
/* port/otavix_legenda.c, compilado no executável: a legenda das teclas por cima do jogo */
void otx_legenda_iniciar(void);
void otx_legenda_mostrar(int sim);

static int pad_teclado_modo(void)
{
    static int m = -1;
    if (m < 0) { const char* e = getenv("GOWA_TECLADO"); m = (e && *e == '0') ? 0 : 1; }
    return m;
}
static void pad_poll_teclado_mouse(void)
{
    PadHostState* hs = &s_host_state[0];
    if (!hs->connected) {                  /* sem controle: parte do neutro */
        memset(hs, 0, sizeof *hs);
        hs->analog_lx = hs->analog_ly = hs->analog_rx = hs->analog_ry = 128;
        hs->connected = 1;
    }
    /* GOWA_LEGENDA=1 (testes): a legenda das teclas ligada desde o início, sem precisar de tecla */
    { static int visto;
      if (!visto) { visto = 1;
          const char* e = getenv("GOWA_LEGENDA");
          if (e && *e == '1') { otx_legenda_iniciar(); otx_legenda_mostrar(1); } } }
    if (!pad_host_window_focused()) return;
#define PAD_K(vk) ((GetAsyncKeyState(vk) & 0x8000) != 0)
    u16 b = 0;
    if (PAD_K(VK_SPACE))                                        b |= CELL_PAD_CTRL_CROSS;
    if (PAD_K(VK_LBUTTON))                                      b |= CELL_PAD_CTRL_SQUARE;
    if (PAD_K(VK_RBUTTON))                                      b |= CELL_PAD_CTRL_TRIANGLE;
    if (PAD_K('E') || PAD_K(VK_BACK) || PAD_K(VK_MBUTTON))      b |= CELL_PAD_CTRL_CIRCLE;
    if (PAD_K('Q'))                                             b |= CELL_PAD_CTRL_L1;
    if (PAD_K('F'))                                             b |= CELL_PAD_CTRL_R1;
    if (PAD_K('C'))                                             b |= CELL_PAD_CTRL_L2;
    if (PAD_K('R'))                                             b |= CELL_PAD_CTRL_R2;
    if (PAD_K('Z'))                                             b |= CELL_PAD_CTRL_L3;
    if (PAD_K('V'))                                             b |= CELL_PAD_CTRL_R3;
    if (PAD_K(VK_UP))                                           b |= CELL_PAD_CTRL_UP;
    if (PAD_K(VK_DOWN))                                         b |= CELL_PAD_CTRL_DOWN;
    if (PAD_K(VK_LEFT))                                         b |= CELL_PAD_CTRL_LEFT;
    if (PAD_K(VK_RIGHT))                                        b |= CELL_PAD_CTRL_RIGHT;
    if (PAD_K(VK_ESCAPE) || PAD_K(VK_RETURN))                   b |= CELL_PAD_CTRL_START;
    if (PAD_K(VK_TAB))                                          b |= CELL_PAD_CTRL_SELECT;
    const int dx = (PAD_K('D') ? 1 : 0) - (PAD_K('A') ? 1 : 0);
    const int dy = (PAD_K('S') ? 1 : 0) - (PAD_K('W') ? 1 : 0);
    const int shift = PAD_K(VK_SHIFT);
#undef PAD_K
    hs->buttons |= b;
    if (b || dx || dy) s_ultimo_teclado = 1;   /* the HUD shows the keys (see cellPad_teclado_ultimo) */
    /* GOWA (25/10): a legenda das teclas (pedido de jogador: o jogo só desenha os botões do controle do PS3). F1
     * mostra e esconde; a primeira tecla de jogo a mostra por 12 s. */
    { static int dito, f1_antes, ligada;
      static ULONGLONG ate;
      if (!dito && (b || dx || dy)) {
          dito = 1;
          printf("[teclado] teclado e mouse ativos (primeira tecla: botoes 0x%04X, direcao %d,%d%s)\n",
                 (unsigned)b, dx, dy, shift ? ", com Shift" : "");
          fflush(stdout);
          ligada = 1;
          ate = GetTickCount64() + 12000;
          otx_legenda_iniciar();
          otx_legenda_mostrar(1);
      }
      const int f1 = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
      if (f1 && !f1_antes) {
          ligada = !ligada;
          ate = 0;
          otx_legenda_iniciar();
          otx_legenda_mostrar(ligada);
      }
      f1_antes = f1;
      if (ate && GetTickCount64() >= ate) {
          ate = 0;
          ligada = 0;
          otx_legenda_mostrar(0);
      } }
    if (dx || dy) {
        const u8 x = (u8)(dx < 0 ? 0 : dx > 0 ? 255 : 128), y = (u8)(dy < 0 ? 0 : dy > 0 ? 255 : 128);
        if (shift) { hs->analog_rx = x; hs->analog_ry = y; }
        else       { hs->analog_lx = x; hs->analog_ly = y; }
    }
    if (b & CELL_PAD_CTRL_L2) hs->trigger_l2 = 255;
    if (b & CELL_PAD_CTRL_R2) hs->trigger_r2 = 255;
    const u16 t = hs->buttons;
    hs->press_up       = (t & CELL_PAD_CTRL_UP)       ? 255 : hs->press_up;
    hs->press_down     = (t & CELL_PAD_CTRL_DOWN)     ? 255 : hs->press_down;
    hs->press_left     = (t & CELL_PAD_CTRL_LEFT)     ? 255 : hs->press_left;
    hs->press_right    = (t & CELL_PAD_CTRL_RIGHT)    ? 255 : hs->press_right;
    hs->press_triangle = (t & CELL_PAD_CTRL_TRIANGLE) ? 255 : hs->press_triangle;
    hs->press_circle   = (t & CELL_PAD_CTRL_CIRCLE)   ? 255 : hs->press_circle;
    hs->press_cross    = (t & CELL_PAD_CTRL_CROSS)    ? 255 : hs->press_cross;
    hs->press_square   = (t & CELL_PAD_CTRL_SQUARE)   ? 255 : hs->press_square;
    hs->press_l1       = (t & CELL_PAD_CTRL_L1)       ? 255 : hs->press_l1;
    hs->press_r1       = (t & CELL_PAD_CTRL_R1)       ? 255 : hs->press_r1;
}
#endif

/* GOWA (27/10): Sony controllers read straight from HID, next to XInput. XInput sees only Xbox pads (and the
 * virtual ones of DS4Windows/DSX in Xbox mode): a DualSense, a DualSense Edge or a DualShock 4 plugged in as it is
 * (or DSX in DualSense mode) reached nothing. A thread looks for them every 2 s (USB and Bluetooth, VID 054C) and
 * one thread per controller reads its reports: the DualSense's USB report 0x01 and Bluetooth 0x31, the DualShock
 * 4's 0x01 and 0x11 (and over Bluetooth, the short 0x01 both send until someone asks for the full one, laid out as
 * the DualShock 4's). Buttons pressed between two polls are kept for the next one, and a right-stick flick that is
 * back at the centre too. They take the host ports XInput leaves free. GOWA_PAD_SONY=0 turns it off. */
#include <setupapi.h>
#include <wctype.h>
#include <hidsdi.h>
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")
#define PAD_SONY_MAX       4
#define PAD_SONY_DEADZONE  5000   /* the Sony sticks rest closer to the centre than an Xbox pad's */
typedef struct {
    volatile LONG vivo;     /* a reader thread owns the slot */
    int  ds5, bt, dados;    /* DualSense layout; Bluetooth; a report arrived */
    WCHAR caminho[512];
    SRWLOCK lock;
    PadHostState st;
    u16  apertados;         /* buttons seen pressed since the last poll */
    u8   flick_x, flick_y;  /* the right stick's farthest point since the last poll */
} PadSony;
static PadSony s_sony[PAD_SONY_MAX];
static volatile LONG s_sony_vivo;

static int pad_sony_ler(const u8* r, DWORD n, int ds5, int bt, PadHostState* o)
{
    const u8 *ax, *b;   /* LX LY RX RY; the three button bytes */
    u8 l2, r2;
    if (r[0] == 0x01 && ds5 && !bt && n >= 11)  { ax = r + 1; l2 = r[5];  r2 = r[6];  b = r + 8; }
    else if (r[0] == 0x31 && ds5 && n >= 12)    { ax = r + 2; l2 = r[6];  r2 = r[7];  b = r + 9; }
    else if (r[0] == 0x01 && n >= 10)           { ax = r + 1; b = r + 5;  l2 = r[8];  r2 = r[9]; }
    else if (r[0] == 0x11 && n >= 12)           { ax = r + 3; b = r + 7;  l2 = r[10]; r2 = r[11]; }
    else return 0;
    static const u16 hat[8] = { CELL_PAD_CTRL_UP, CELL_PAD_CTRL_UP | CELL_PAD_CTRL_RIGHT, CELL_PAD_CTRL_RIGHT,
        CELL_PAD_CTRL_DOWN | CELL_PAD_CTRL_RIGHT, CELL_PAD_CTRL_DOWN, CELL_PAD_CTRL_DOWN | CELL_PAD_CTRL_LEFT,
        CELL_PAD_CTRL_LEFT, CELL_PAD_CTRL_UP | CELL_PAD_CTRL_LEFT };
    u16 btns = (b[0] & 0x0F) < 8 ? hat[b[0] & 0x0F] : 0;
    if (b[0] & 0x10) btns |= CELL_PAD_CTRL_SQUARE;
    if (b[0] & 0x20) btns |= CELL_PAD_CTRL_CROSS;
    if (b[0] & 0x40) btns |= CELL_PAD_CTRL_CIRCLE;
    if (b[0] & 0x80) btns |= CELL_PAD_CTRL_TRIANGLE;
    if (b[1] & 0x01) btns |= CELL_PAD_CTRL_L1;
    if (b[1] & 0x02) btns |= CELL_PAD_CTRL_R1;
    if (b[1] & 0x10) btns |= CELL_PAD_CTRL_SELECT;      /* Share / Create */
    if (b[1] & 0x20) btns |= CELL_PAD_CTRL_START;       /* Options */
    if (b[1] & 0x40) btns |= CELL_PAD_CTRL_L3;
    if (b[1] & 0x80) btns |= CELL_PAD_CTRL_R3;
    if (b[2] & 0x02) btns |= CELL_PAD_CTRL_SELECT;      /* the touchpad's click, as in RPCS3 */
    if (l2 > PAD_TRIGGER_THRESHOLD) btns |= CELL_PAD_CTRL_L2;
    if (r2 > PAD_TRIGGER_THRESHOLD) btns |= CELL_PAD_CTRL_R2;
    o->connected = 1;
    o->buttons = btns;
    /* Sony's Y grows downward like the PS3's: into XInput's signed range (Y up) for the same deadzone and squircle,
     * then back as the XInput path does */
    #define SONY_EIXO(v) ((short)(((int)(v) - 128) * 258 > 32767 ? 32767 : ((int)(v) - 128) * 258 < -32768 ? -32768 : ((int)(v) - 128) * 258))
    pad_xinput_stick(SONY_EIXO(ax[0]), SONY_EIXO(256 - ax[1]), PAD_SONY_DEADZONE, &o->analog_lx, &o->analog_ly);
    o->analog_ly = (u8)(256 - o->analog_ly);
    pad_xinput_stick(SONY_EIXO(ax[2]), SONY_EIXO(256 - ax[3]), PAD_SONY_DEADZONE, &o->analog_rx, &o->analog_ry);
    o->analog_ry = (u8)(256 - o->analog_ry);
    #undef SONY_EIXO
    o->trigger_l2 = l2;
    o->trigger_r2 = r2;
    o->press_up       = (btns & CELL_PAD_CTRL_UP)       ? 255 : 0;
    o->press_down     = (btns & CELL_PAD_CTRL_DOWN)     ? 255 : 0;
    o->press_left     = (btns & CELL_PAD_CTRL_LEFT)     ? 255 : 0;
    o->press_right    = (btns & CELL_PAD_CTRL_RIGHT)    ? 255 : 0;
    o->press_triangle = (btns & CELL_PAD_CTRL_TRIANGLE) ? 255 : 0;
    o->press_circle   = (btns & CELL_PAD_CTRL_CIRCLE)   ? 255 : 0;
    o->press_cross    = (btns & CELL_PAD_CTRL_CROSS)    ? 255 : 0;
    o->press_square   = (btns & CELL_PAD_CTRL_SQUARE)   ? 255 : 0;
    o->press_l1       = (btns & CELL_PAD_CTRL_L1)       ? 255 : 0;
    o->press_r1       = (btns & CELL_PAD_CTRL_R1)       ? 255 : 0;
    return 1;
}

static int pad_sony_longe(u8 x, u8 y) { return abs((int)x - 128) + abs((int)y - 128); }

static DWORD WINAPI pad_sony_leitor(LPVOID arg)
{
    PadSony* s = (PadSony*)arg;
    HANDLE h = CreateFileW(s->caminho, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        h = CreateFileW(s->caminho, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        u8 r[128];
        DWORD n;
        PadHostState st;
        int primeiro = 1;
        while (s_sony_vivo && ReadFile(h, r, sizeof r, &n, NULL)) {
            memset(&st, 0, sizeof st);
            if (!pad_sony_ler(r, n, s->ds5, s->bt, &st)) continue;
            if (primeiro) { printf("[cellPad] controle Sony lendo (%s, %s, relatório 0x%02X)\n", s->ds5 ? "DualSense" : "DualShock 4", s->bt ? "Bluetooth" : "USB", r[0]); primeiro = 0; }
            AcquireSRWLockExclusive(&s->lock);
            s->st = st;
            s->dados = 1;
            s->apertados |= st.buttons;
            if (pad_sony_longe(st.analog_rx, st.analog_ry) > pad_sony_longe(s->flick_x, s->flick_y)) { s->flick_x = st.analog_rx; s->flick_y = st.analog_ry; }
            ReleaseSRWLockExclusive(&s->lock);
        }
        CloseHandle(h);
    }
    AcquireSRWLockExclusive(&s->lock);
    s->dados = 0;
    ReleaseSRWLockExclusive(&s->lock);
    InterlockedExchange(&s->vivo, 0);   /* unplugged: the next search opens it again */
    return 0;
}

static int s_sony_buscas;   /* quantas buscas de HID já foram feitas (o log dos não suportados só na primeira) */
static DWORD WINAPI pad_sony_busca(LPVOID arg)
{
    (void)arg;
    GUID hid;
    HidD_GetHidGuid(&hid);
    while (s_sony_vivo) {
        HDEVINFO lista = SetupDiGetClassDevsW(&hid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (lista != INVALID_HANDLE_VALUE) {
            SP_DEVICE_INTERFACE_DATA di = { sizeof di };
            for (DWORD i = 0; SetupDiEnumDeviceInterfaces(lista, NULL, &hid, i, &di); i++) {
                union { SP_DEVICE_INTERFACE_DETAIL_DATA_W d; u8 b[sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) + 1024]; } det;
                det.d.cbSize = sizeof det.d;
                if (!SetupDiGetDeviceInterfaceDetailW(lista, &di, &det.d, sizeof det, NULL, NULL)) continue;
                const WCHAR* p = det.d.DevicePath;
                /* 28/10: "vid_054c" over USB, but "..._VID&0002054c_PID&0ce6..." over Bluetooth: the old test
                 * skipped every Bluetooth controller (a DualSense only worked by cable or through DSX). Any path
                 * naming 054c, then the vendor in the attributes below. */
                {
                    WCHAR baixo[512];
                    size_t i = 0;
                    for (; p[i] && i < 511; i++) baixo[i] = (WCHAR)towlower(p[i]);
                    baixo[i] = 0;
                    if (!wcsstr(baixo, L"054c")) {
                        /* 30/10 (reports 422/423): na primeira busca, os controles HID que não são da Sony (DirectInput:
                         * 8BitDo em modo D, Switch Pro, genéricos USB) vão para o log como não suportados */
                        if (s_sony_buscas == 0) {
                            HANDLE g = CreateFileW(p, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
                            if (g != INVALID_HANDLE_VALUE) {
                                HIDD_ATTRIBUTES ga = { sizeof ga };
                                PHIDP_PREPARSED_DATA gp = NULL;
                                HIDP_CAPS gc;
                                if (HidD_GetAttributes(g, &ga) && HidD_GetPreparsedData(g, &gp) &&
                                    HidP_GetCaps(gp, &gc) == HIDP_STATUS_SUCCESS && gc.UsagePage == 0x01 &&
                                    (gc.Usage == 0x04 || gc.Usage == 0x05))
                                    fprintf(stderr, "[controle] HID %04X:%04X (%s) sem suporte direto: use o modo XInput do controle, "
                                            "o Steam Input ou o DS4Windows\n", ga.VendorID, ga.ProductID,
                                            gc.Usage == 0x05 ? "gamepad" : "joystick");
                                if (gp) HidD_FreePreparsedData(gp);
                                CloseHandle(g);
                            }
                        }
                        continue;
                    }
                }
                int aberto = 0;
                for (int k = 0; k < PAD_SONY_MAX; k++) aberto |= s_sony[k].vivo && !_wcsicmp(s_sony[k].caminho, p);
                if (aberto) continue;
                /* who it is (attributes need no access rights) and over what: USB reports are 64 bytes */
                HANDLE h = CreateFileW(p, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
                if (h == INVALID_HANDLE_VALUE) continue;
                HIDD_ATTRIBUTES at = { sizeof at };
                PHIDP_PREPARSED_DATA pp = NULL;
                HIDP_CAPS caps;
                int ok = HidD_GetAttributes(h, &at) && HidD_GetPreparsedData(h, &pp) && HidP_GetCaps(pp, &caps) == HIDP_STATUS_SUCCESS;
                if (pp) HidD_FreePreparsedData(pp);
                CloseHandle(h);
                if (!ok || at.VendorID != 0x054C || caps.UsagePage != 0x01 || (caps.Usage != 0x05 && caps.Usage != 0x04)) continue;
                const int ds5 = at.ProductID == 0x0CE6 || at.ProductID == 0x0DF2;
                const int ds4 = at.ProductID == 0x05C4 || at.ProductID == 0x09CC || at.ProductID == 0x0BA0;
                if (!ds5 && !ds4) {
                    if (s_sony_buscas == 0)
                        fprintf(stderr, "[controle] Sony %04X:%04X sem suporte (só DualShock 4 e DualSense)\n", at.VendorID, at.ProductID);
                    continue;
                }
                fprintf(stderr, "[controle] %s por %s\n", ds5 ? "DualSense" : "DualShock 4", caps.InputReportByteLength != 64 ? "Bluetooth" : "USB");
                for (int k = 0; k < PAD_SONY_MAX; k++) {
                    if (InterlockedCompareExchange(&s_sony[k].vivo, 1, 0) != 0) continue;
                    PadSony* s = &s_sony[k];
                    wcsncpy(s->caminho, p, 511);
                    s->caminho[511] = 0;
                    s->ds5 = ds5;
                    s->bt = caps.InputReportByteLength != 64;
                    HANDLE t = CreateThread(NULL, 0, pad_sony_leitor, s, 0, NULL);
                    if (t) CloseHandle(t); else InterlockedExchange(&s->vivo, 0);
                    break;
                }
            }
            SetupDiDestroyDeviceInfoList(lista);
        }
        s_sony_buscas++;
        Sleep(2000);
    }
    return 0;
}

/* the Sony controllers into the host ports XInput left free, in order */
static void pad_poll_sony(void)
{
    int porta = 0;
    for (int k = 0; k < PAD_SONY_MAX; k++) {
        PadSony* s = &s_sony[k];
        if (!s->vivo) continue;
        AcquireSRWLockExclusive(&s->lock);
        PadHostState st = s->st;
        const int tem = s->dados;
        const u16 extra = s->apertados;
        const u8 fx = s->flick_x, fy = s->flick_y;
        s->apertados = 0;
        s->flick_x = s->flick_y = 128;
        ReleaseSRWLockExclusive(&s->lock);
        if (!tem) continue;
        while (porta < PAD_MAX_HOST_PORTS && s_host_state[porta].connected) porta++;
        if (porta >= PAD_MAX_HOST_PORTS) return;
        /* a press, or a flick, that began and ended between two polls still reaches this one */
        st.buttons |= extra;
        if (pad_sony_longe(st.analog_rx, st.analog_ry) < 64 && pad_sony_longe(fx, fy) >= 64) { st.analog_rx = fx; st.analog_ry = fy; }
        s_host_state[porta] = st;
        if (porta == 0) {
            s_xinput_ligado = 1;
            if (st.buttons || st.trigger_l2 > 40 || st.trigger_r2 > 40 || pad_sony_longe(st.analog_lx, st.analog_ly) > 64 ||
                pad_sony_longe(st.analog_rx, st.analog_ry) > 64)
                s_ultimo_teclado = 0;
        }
        porta++;
    }
}

/* Map XInput buttons to PS3 CELL_PAD_CTRL_* (also read by the sampler thread above) */
static u16 pad_xinput_botoes(const XINPUT_GAMEPAD* gp)
{
    u16 btns = 0;
    if (gp->wButtons & XINPUT_GAMEPAD_BACK)           btns |= CELL_PAD_CTRL_SELECT;
    if (gp->wButtons & XINPUT_GAMEPAD_LEFT_THUMB)     btns |= CELL_PAD_CTRL_L3;
    if (gp->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)    btns |= CELL_PAD_CTRL_R3;
    if (gp->wButtons & XINPUT_GAMEPAD_START)          btns |= CELL_PAD_CTRL_START;
    if (gp->wButtons & XINPUT_GAMEPAD_DPAD_UP)        btns |= CELL_PAD_CTRL_UP;
    if (gp->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)     btns |= CELL_PAD_CTRL_RIGHT;
    if (gp->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)      btns |= CELL_PAD_CTRL_DOWN;
    if (gp->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)      btns |= CELL_PAD_CTRL_LEFT;
    if (gp->bLeftTrigger > PAD_TRIGGER_THRESHOLD)     btns |= CELL_PAD_CTRL_L2;
    if (gp->bRightTrigger > PAD_TRIGGER_THRESHOLD)    btns |= CELL_PAD_CTRL_R2;
    if (gp->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)  btns |= CELL_PAD_CTRL_L1;
    if (gp->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) btns |= CELL_PAD_CTRL_R1;
    if (gp->wButtons & XINPUT_GAMEPAD_Y)              btns |= CELL_PAD_CTRL_TRIANGLE;
    if (gp->wButtons & XINPUT_GAMEPAD_B)              btns |= CELL_PAD_CTRL_CIRCLE;
    if (gp->wButtons & XINPUT_GAMEPAD_A)              btns |= CELL_PAD_CTRL_CROSS;
    if (gp->wButtons & XINPUT_GAMEPAD_X)              btns |= CELL_PAD_CTRL_SQUARE;
    return btns;
}

static void pad_poll_xinput(void)
{
    for (int i = 0; i < PAD_MAX_HOST_PORTS; i++) {
        XINPUT_STATE state;
        memset(&state, 0, sizeof(state));

        DWORD result = XInputGetState((DWORD)i, &state);
        if (result != ERROR_SUCCESS) {
            s_host_state[i].connected = 0;
            continue;
        }

        /* 30/10 (reports 422/423, "controle não reconhecido"): o log diz quais controles o jogo achou */
        { static int visto[PAD_MAX_HOST_PORTS];
          if (!visto[i]) { visto[i] = 1; fprintf(stderr, "[controle] XInput no slot %d (Xbox ou compatível)\n", i); } }
        s_host_state[i].connected = 1;
        XINPUT_GAMEPAD* gp = &state.Gamepad;

        const u16 btns = pad_xinput_botoes(gp);
        s_host_state[i].buttons = btns;

        /* Analog sticks. PS3 Y axis is inverted vs XInput (up = 0). Reflect about
         * 128 (256 - x), NOT 255 - x: the latter turns a centered stick into 127,
         * whose bits (0x7F) alias SELECT+START and can make the guest self-exit.
         * (via sagemono, PR #42) */
        pad_xinput_stick(gp->sThumbLX, gp->sThumbLY, PAD_STICK_DEADZONE, &s_host_state[i].analog_lx, &s_host_state[i].analog_ly);
        s_host_state[i].analog_ly = (u8)(256 - s_host_state[i].analog_ly);
        pad_xinput_stick(gp->sThumbRX, gp->sThumbRY, PAD_STICK_DEADZONE, &s_host_state[i].analog_rx, &s_host_state[i].analog_ry);
        s_host_state[i].analog_ry = (u8)(256 - s_host_state[i].analog_ry);

        /* Triggers */
        s_host_state[i].trigger_l2 = gp->bLeftTrigger;
        s_host_state[i].trigger_r2 = gp->bRightTrigger;

        /* the controller used (a button, a trigger or a stick well off the centre): the HUD shows its buttons */
        if (i == 0) {
            s_xinput_ligado = 1;
            const PadHostState* h = &s_host_state[0];
            if (btns || gp->bLeftTrigger > 40 || gp->bRightTrigger > 40 ||
                h->analog_lx < 64 || h->analog_lx > 192 || h->analog_ly < 64 || h->analog_ly > 192 ||
                h->analog_rx < 64 || h->analog_rx > 192 || h->analog_ry < 64 || h->analog_ry > 192)
                s_ultimo_teclado = 0;
        }

        /* Pressure-sensitive buttons: XInput has digital only, so 0 or 255 */
        s_host_state[i].press_up       = (btns & CELL_PAD_CTRL_UP)       ? 255 : 0;
        s_host_state[i].press_down     = (btns & CELL_PAD_CTRL_DOWN)     ? 255 : 0;
        s_host_state[i].press_left     = (btns & CELL_PAD_CTRL_LEFT)     ? 255 : 0;
        s_host_state[i].press_right    = (btns & CELL_PAD_CTRL_RIGHT)    ? 255 : 0;
        s_host_state[i].press_triangle = (btns & CELL_PAD_CTRL_TRIANGLE) ? 255 : 0;
        s_host_state[i].press_circle   = (btns & CELL_PAD_CTRL_CIRCLE)   ? 255 : 0;
        s_host_state[i].press_cross    = (btns & CELL_PAD_CTRL_CROSS)    ? 255 : 0;
        s_host_state[i].press_square   = (btns & CELL_PAD_CTRL_SQUARE)   ? 255 : 0;
        s_host_state[i].press_l1       = (btns & CELL_PAD_CTRL_L1)       ? 255 : 0;
        s_host_state[i].press_r1       = (btns & CELL_PAD_CTRL_R1)       ? 255 : 0;
    }
    pad_poll_sony();
}

static void pad_init_backend(void)
{
    const char* sony = getenv("GOWA_PAD_SONY");
    if (!(sony && *sony == '0') && !InterlockedExchange(&s_sony_vivo, 1)) {
        for (int k = 0; k < PAD_SONY_MAX; k++) InitializeSRWLock(&s_sony[k].lock);
        HANDLE t = CreateThread(NULL, 0, pad_sony_busca, NULL, 0, NULL);
        if (t) CloseHandle(t);
        printf("[cellPad] controles Sony (DualSense, DualShock 4) pelo HID: %s\n", t ? "procurando" : "falhou");
    }
    /* XInput needs no explicit init; the sampler thread (pad_tap_segurar) starts here */
    if (pad_amostra_ligada() && !InterlockedExchange(&s_amostra_viva, 1)) {
        s_amostra_thread = CreateThread(NULL, 0, pad_amostra_func, NULL, 0, NULL);
        printf("[cellPad] amostragem do controle a cada ~4 ms: %s\n", s_amostra_thread ? "ligada" : "falhou");
    }
}

static void pad_shutdown_backend(void)
{
    if (InterlockedExchange(&s_amostra_viva, 0) && s_amostra_thread) {
        WaitForSingleObject(s_amostra_thread, 1000);
        CloseHandle(s_amostra_thread);
        s_amostra_thread = NULL;
    }
}

#endif /* PAD_BACKEND_XINPUT */

/* ---------------------------------------------------------------------------
 * SDL2 backend
 * -----------------------------------------------------------------------*/

#if PAD_BACKEND_SDL2

static u8 pad_sdl_axis_to_u8(int raw)
{
    /* SDL axis: -32768..32767 -> 0..255 with center at 128 */
    int val = ((raw + 32768) * 255) / 65535;
    if (val < 0) val = 0;
    if (val > 255) val = 255;
    return (u8)val;
}

static u8 pad_sdl_trigger_to_u8(int raw)
{
    /* SDL trigger: 0..32767 -> 0..255 */
    int val = (raw * 255) / 32767;
    if (val < 0) val = 0;
    if (val > 255) val = 255;
    return (u8)val;
}

static void pad_poll_sdl2(void)
{
    SDL_GameControllerUpdate();

    for (int i = 0; i < PAD_MAX_HOST_PORTS; i++) {
        if (!s_sdl_controllers[i]) {
            /* Try to open newly connected controllers */
            if (SDL_IsGameController(i)) {
                s_sdl_controllers[i] = SDL_GameControllerOpen(i);
            }
        }

        SDL_GameController* gc = s_sdl_controllers[i];
        if (!gc || !SDL_GameControllerGetAttached(gc)) {
            s_host_state[i].connected = 0;
            if (gc) {
                SDL_GameControllerClose(gc);
                s_sdl_controllers[i] = NULL;
            }
            continue;
        }

        s_host_state[i].connected = 1;

        u16 btns = 0;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_BACK))          btns |= CELL_PAD_CTRL_SELECT;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_LEFTSTICK))     btns |= CELL_PAD_CTRL_L3;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_RIGHTSTICK))    btns |= CELL_PAD_CTRL_R3;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_START))         btns |= CELL_PAD_CTRL_START;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_UP))       btns |= CELL_PAD_CTRL_UP;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))    btns |= CELL_PAD_CTRL_RIGHT;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_DOWN))     btns |= CELL_PAD_CTRL_DOWN;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_LEFT))     btns |= CELL_PAD_CTRL_LEFT;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))  btns |= CELL_PAD_CTRL_L1;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) btns |= CELL_PAD_CTRL_R1;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_Y))             btns |= CELL_PAD_CTRL_TRIANGLE;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_B))             btns |= CELL_PAD_CTRL_CIRCLE;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_A))             btns |= CELL_PAD_CTRL_CROSS;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_X))             btns |= CELL_PAD_CTRL_SQUARE;

        /* Triggers via axis */
        int lt = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
        int rt = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        if (lt > 3000) btns |= CELL_PAD_CTRL_L2;
        if (rt > 3000) btns |= CELL_PAD_CTRL_R2;

        s_host_state[i].buttons = btns;

        /* Analog sticks */
        s_host_state[i].analog_lx = pad_sdl_axis_to_u8(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX));
        s_host_state[i].analog_ly = pad_sdl_axis_to_u8(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY));
        s_host_state[i].analog_rx = pad_sdl_axis_to_u8(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTX));
        s_host_state[i].analog_ry = pad_sdl_axis_to_u8(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTY));

        /* Triggers */
        s_host_state[i].trigger_l2 = pad_sdl_trigger_to_u8(lt);
        s_host_state[i].trigger_r2 = pad_sdl_trigger_to_u8(rt);

        /* Pressure: SDL has digital buttons, so 0 or 255 */
        s_host_state[i].press_up       = (btns & CELL_PAD_CTRL_UP)       ? 255 : 0;
        s_host_state[i].press_down     = (btns & CELL_PAD_CTRL_DOWN)     ? 255 : 0;
        s_host_state[i].press_left     = (btns & CELL_PAD_CTRL_LEFT)     ? 255 : 0;
        s_host_state[i].press_right    = (btns & CELL_PAD_CTRL_RIGHT)    ? 255 : 0;
        s_host_state[i].press_triangle = (btns & CELL_PAD_CTRL_TRIANGLE) ? 255 : 0;
        s_host_state[i].press_circle   = (btns & CELL_PAD_CTRL_CIRCLE)   ? 255 : 0;
        s_host_state[i].press_cross    = (btns & CELL_PAD_CTRL_CROSS)    ? 255 : 0;
        s_host_state[i].press_square   = (btns & CELL_PAD_CTRL_SQUARE)   ? 255 : 0;
        s_host_state[i].press_l1       = (btns & CELL_PAD_CTRL_L1)       ? 255 : 0;
        s_host_state[i].press_r1       = (btns & CELL_PAD_CTRL_R1)       ? 255 : 0;
    }
}

static void pad_init_backend(void)
{
    if (!s_sdl_inited) {
        if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0) {
            SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
        }
        s_sdl_inited = 1;
    }
    memset(s_sdl_controllers, 0, sizeof(s_sdl_controllers));

    /* Open any controllers already connected */
    int num = SDL_NumJoysticks();
    for (int i = 0; i < num && i < PAD_MAX_HOST_PORTS; i++) {
        if (SDL_IsGameController(i)) {
            s_sdl_controllers[i] = SDL_GameControllerOpen(i);
        }
    }
}

static void pad_shutdown_backend(void)
{
    for (int i = 0; i < PAD_MAX_HOST_PORTS; i++) {
        if (s_sdl_controllers[i]) {
            SDL_GameControllerClose(s_sdl_controllers[i]);
            s_sdl_controllers[i] = NULL;
        }
    }
}

#endif /* PAD_BACKEND_SDL2 */

/* ---------------------------------------------------------------------------
 * Poll dispatcher
 * -----------------------------------------------------------------------*/

/* Re-poll the host at most every PAD_POLL_INTERVAL_MS; serve cached state in
 * between.
 *
 * XInputGetState is a driver call, and on a slot with nothing plugged in it is
 * an expensive one: measured at 97.6 us for a sweep of the 7 host ports on a
 * machine with no controller attached. cellPadGetData ran that sweep on EVERY
 * call, and a guest that polls the pad from a spin loop -- The Simpsons Arcade
 * Game does, in its input stage -- therefore spends its whole main thread
 * inside the input driver.
 *
 * 4 ms is 250 Hz, well above the 60 Hz a title can actually observe (and above
 * a real DualShock 3's own report rate), so no game can tell the difference,
 * including one sampling for button-press edges. */
#define PAD_POLL_INTERVAL_MS 4

static unsigned long long pad_now_ms(void)
{
#ifdef _WIN32
    return (unsigned long long)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000ull +
           (unsigned long long)ts.tv_nsec / 1000000ull;
#endif
}

static void pad_poll_backend(void)
{
    /* Benign race: two threads may poll in the same tick. That costs one extra
     * sweep, never a missed or stale-forever read. */
    static unsigned long long s_last_ms = 0;
    unsigned long long now = pad_now_ms();
    if (s_last_ms && now - s_last_ms < PAD_POLL_INTERVAL_MS) return;
    s_last_ms = now;
    /* A real pad streams reports at roughly this rate whether or not anything
     * changed, and libpad buffers them; a fresh sweep is a fresh report for
     * every port. cellPadGetData hands each one out exactly once. */
    for (int _i = 0; _i < PAD_MAX_HOST_PORTS; _i++) s_data_fresh[_i] = 1;

#if PAD_BACKEND_XINPUT
    pad_poll_xinput();
#elif PAD_BACKEND_SDL2
    pad_poll_sdl2();
#endif
#ifdef _WIN32
    if (pad_teclado_modo()) pad_poll_teclado_mouse();
    else if (!s_host_state[0].connected) pad_poll_keyboard();
#endif
#if PAD_BACKEND_XINPUT
    pad_tap_segurar(&s_host_state[0]);
#endif
}

/* ---------------------------------------------------------------------------
 * API implementations
 * -----------------------------------------------------------------------*/

s32 cellPadInit(u32 max_connect)
{
    printf("[cellPad] Init(max_connect=%u)\n", max_connect);

    if (s_pad_initialized)
        return CELL_PAD_ERROR_ALREADY_OPENED;

    /* The bound is the 127-slot pad table, not the 7 ports: larger requests
     * are accepted and clamped. Resistance: Fall of Man asks for 8; rejected,
     * the library stayed closed, every GetInfo failed and the title never
     * polled the pad -- stuck on "Press the START button". */
    if (max_connect == 0 || max_connect > 127)
        return CELL_PAD_ERROR_INVALID_PARAMETER;

    s_pad_initialized = 1;
    s_max_connect = max_connect > CELL_PAD_MAX_PORT_NUM ? CELL_PAD_MAX_PORT_NUM : max_connect;
    memset(s_port_setting, 0, sizeof(s_port_setting));
    memset(s_host_state, 0, sizeof(s_host_state));

    pad_init_backend();

    /* Do an initial poll to detect connected controllers */
    pad_poll_backend();

    return CELL_OK;
}

s32 cellPadEnd(void)
{
    printf("[cellPad] End()\n");

    if (!s_pad_initialized)
        return CELL_PAD_ERROR_NOT_OPENED;

    pad_shutdown_backend();

    s_pad_initialized = 0;
    s_max_connect = 0;
    return CELL_OK;
}

void cellPad_poll(void)
{
    if (s_pad_initialized) {
        pad_poll_backend();
    }
}

/* HLE args are GUEST effective addresses; guest structs are BIG-ENDIAN. The
 * vm_write helpers from ppu_memory.h translate + byte-swap. (cellPad was
 * written for host pointers and was never actually invoked until its NIDs
 * were fixed.) These used to be redeclared here with a 64-bit address
 * parameter, which conflicts with the header now that it is included. */

s32 cellPadGetData(u32 port_no, CellPadData* data_guest)
{
    { static int _once = 0;
      if (!_once++) printf("[cellPad] GetData polling begins (port %u)\n", port_no); }
    if (!s_pad_initialized)
        return CELL_PAD_ERROR_NOT_OPENED;

    if (port_no >= s_max_connect || !data_guest)
        return CELL_PAD_ERROR_INVALID_PARAMETER;

    /* Build in a local host struct, then copy to guest memory big-endian. */
    CellPadData _d; CellPadData* data = &_d;
    memset(data, 0, sizeof(CellPadData));

    /* Poll fresh state */
    pad_poll_backend();

    /* YDKJ_INJECT_PAD: the game's wait loop polls the pad (cellPadSetActDirect /
     * GetData) -- if it's parked on a "press button" prompt after loading, no
     * host input means it waits forever. Inject a real button PULSE (CROSS+START+
     * CIRCLE) so it advances. Legit input simulation, not forged pixels. */
    /* NOTE: cache getenv() -- this runs on every poll from multiple threads, and the
     * uncached version crashed deterministically (varying fault address = race). Every
     * other probe in this tree uses the cached `static int x=-1` pattern; match it. */
    static int  s_inj   = -1;
    static u16  s_btn   = 0x0008u;   /* default START */
    static long s_delay = 3000;
    if (s_inj < 0) {
        s_inj = getenv("YDKJ_INJECT_PAD") ? 1 : 0;
        const char* be = getenv("YDKJ_PAD_BTN");   if (be) s_btn   = (u16)strtoul(be,0,0);
        const char* de = getenv("YDKJ_PAD_DELAY"); if (de) s_delay = strtol(de,0,0);
    }
    /* Gate on WALL-CLOCK, not poll count: the game polls the pad hundreds of thousands
     * of times inside an early wait loop, so any count-based delay fires during init
     * (crash: guest ctr=0x0005F8A0 r3=0 -- button handler on an unconstructed object).
     * YDKJ_PAD_DELAY is now SECONDS to wait before the first injected press. */
    if (s_inj && port_no < PAD_MAX_HOST_PORTS) {
        static long _pc = 0; _pc++;
        /* Cheap counter gate FIRST: clock() is a slow call and this poll loop is hammered
         * by the guest from multiple threads; sampling it every poll perturbs timing. Only
         * consult the clock once every 256 polls, and never after arming. */
        static int     s_armed = 0;
        static clock_t s_t0    = 0;
        if (!s_armed) {
            if ((_pc & 0xFF) == 0) {
                if (!s_t0) s_t0 = clock();
                else if ((double)(clock() - s_t0) / (double)CLOCKS_PER_SEC >= (double)s_delay) s_armed = 1;
            }
            goto skip_inject;
        }
        u16 btn = s_btn; long delay = 0;
        /* Delay past init, then a single clean pulse every ~180 calls (press 15, release 165). */
        if (_pc > delay) {
            long ph = (_pc - delay) % 180;
            if (ph < 15) s_host_state[port_no].buttons |= btn;
            else         s_host_state[port_no].buttons &= ~btn;
            s_host_state[port_no].connected = 1;
        }
    }
skip_inject: ;

    if (port_no >= PAD_MAX_HOST_PORTS || (!s_host_state[port_no].connected && port_no != 0)) {
        goto emit;   /* data stays zeroed -> len=0 */
    }
    /* port 0 is always a valid (virtual) pad; s_host_state[0] is neutral if no
     * physical device, which yields no-buttons-pressed data below. */

    {
    PadHostState* hs = &s_host_state[port_no];
    u32 setting = s_port_setting[port_no];

    /* Determine data length based on port settings */
    s32 len = CELL_PAD_LEN_CHANGE_DEFAULT;
    if (setting & CELL_PAD_SETTING_SENSOR_ON)
        len = CELL_PAD_LEN_CHANGE_SENSOR_ON;
    else if (setting & CELL_PAD_SETTING_PRESS_ON)
        len = CELL_PAD_LEN_CHANGE_PRESS_ON;

    data->len = len;
    /* button[0] is reserved and 0 on hardware; button[1] carries the pad mode
     * in its high nibble (0x7) and len/2 in its low one. GH3 drops any packet
     * whose button[0] is non-zero -- with len (8) there, every press was
     * received and thrown away, and "Press any button" never advanced. */
    data->button[0] = 0;
    data->button[1] = (u16)(0x70 | ((len / 2) & 0xF));

    /* Digital buttons. hs->buttons packs both halves into one 16-bit mask
     * (SELECT=bit0..LEFT=bit7 = DIGITAL1; L2=bit8..SQUARE=bit15 = DIGITAL2).
     * Split correctly — writing the whole value into DIGITAL1 with DIGITAL2=0
     * left every face button (cross/circle/triangle/square, L1/L2/R1/R2) dead.
     * (via sagemono, PR #42) */
    data->button[CELL_PAD_BTN_OFFSET_DIGITAL1] = (u16)(hs->buttons & 0xFF);
    data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] = (u16)((hs->buttons >> 8) & 0xFF);

    /* PAD_AUTOPRESS: headless bring-up input -- pulse CROSS then START every few
     * seconds of polling so boot screens that wait for input advance without a
     * human at the pad. Env-gated test scaffolding, off by default. */
    { static int s_ap = -1; static unsigned s_apn = 0;
      if (s_ap < 0) s_ap = getenv("PAD_AUTOPRESS") ? 1 : 0;
      if (s_ap && port_no == 0) {
          unsigned ph = s_apn++ % 240;
          if (ph < 12)
              data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] |= 0x40;   /* CROSS */
          else if (ph >= 120 && ph < 132)
              data->button[CELL_PAD_BTN_OFFSET_DIGITAL1] |= 0x08;   /* START */
      } }

    /* PAD_SCRIPT="<sec>:<mask>,<sec>:<mask>,..." -- press a NAMED button at a
     * given wall-clock second, each held ~250 ms. PAD_AUTOPRESS only pulses
     * CROSS and START, which is enough to clear a "press start" screen but not
     * to navigate a menu: You Don't Know Jack needs DOWN to move off its name
     * field before CROSS means anything. Masks are CELL_PAD_CTRL_* packed as
     * (DIGITAL2 << 8) | DIGITAL1: START 0x0008, UP 0x0010, DOWN 0x0040,
     * LEFT 0x0080, RIGHT 0x0020, CROSS 0x4000, CIRCLE 0x2000, TRIANGLE 0x1000.
     * Legit input simulation on the same footing as PAD_AUTOPRESS/PAD_STICK. */
    { static int s_sc = -1;
      static struct { double t; unsigned mask; } ev[32]; static int n_ev = 0;
      static unsigned long long t0 = 0;   /* not ULONGLONG: this builds on POSIX too */
      if (s_sc < 0) {
          s_sc = 0;
          const char* e = getenv("PAD_SCRIPT");
          if (e && *e) {
              s_sc = 1; t0 = GetTickCount64();
              const char* p = e;
              while (*p && n_ev < 32) {
                  double t = strtod(p, (char**)&p);
                  if (*p == ':') p++;
                  unsigned m = (unsigned)strtoul(p, (char**)&p, 0);
                  ev[n_ev].t = t; ev[n_ev].mask = m; n_ev++;
                  while (*p == ',' || *p == ' ') p++;
              }
              printf("[cellPad] PAD_SCRIPT: %d event(s)\n", n_ev);
          }
      }
      if (s_sc && port_no == 0) {
          double now = (double)(GetTickCount64() - t0) / 1000.0;
          /* Latch, do not window. A fixed time window is missed whenever the
           * title stops polling the pad for longer than the window (it does
           * exactly that on a loading screen), so the press silently never
           * happens. Fire on the first poll at or after the event time and
           * hold it for a fixed number of polls, which is what a real press
           * looks like regardless of poll rate. */
          static int fired[32], held[32];
          for (int i = 0; i < n_ev; i++) {
              if (!fired[i] && now >= ev[i].t) { fired[i] = 1; held[i] = 40; }
              if (held[i] > 0) {
                  held[i]--;
                  data->button[CELL_PAD_BTN_OFFSET_DIGITAL1] |= (u16)(ev[i].mask & 0xFF);
                  data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] |= (u16)((ev[i].mask >> 8) & 0xFF);
                  static int said[32];
                  if (!said[i]) { said[i] = 1;
                      printf("[cellPad] PAD_SCRIPT t=%.1fs press 0x%04X\n", ev[i].t, ev[i].mask); }
              }
          }
      } }

    /* PAD_FILE=<path> -- closed-loop input. Each poll, if the file holds a
     * button mask (same encoding as PAD_SCRIPT), press it for a fixed number
     * of polls and empty the file. A driver outside the process can then look
     * at the log or a frame dump, decide which screen the title is on, and
     * write the next button -- which a fixed time schedule cannot do, because
     * a menu system with several screens wanders when driven blind.
     *
     *   echo 0x0008 > pad.txt    # START
     *   echo 0x4000 > pad.txt    # CROSS
     *   echo 0x0040 4 > pad.txt  # DOWN for 4 polls (menus auto-repeat a
     *                            # long hold; the default is 40)
     */
    { static int s_pf = -1; static char pf_path[512];
      static unsigned held_mask = 0; static int held_n = 0;
      if (s_pf < 0) { const char* e = getenv("PAD_FILE");
                      s_pf = (e && *e) ? 1 : 0;
                      if (s_pf) { strncpy(pf_path, e, sizeof pf_path - 1); pf_path[sizeof pf_path - 1] = 0; } }
      if (s_pf && port_no == 0) {
          if (held_n <= 0) {
              FILE* f = fopen(pf_path, "rb");
              if (f) {
                  char buf[64]; buf[0] = 0;
                  if (fgets(buf, sizeof buf, f)) {
                      char* rest;
                      unsigned m = (unsigned)strtoul(buf, &rest, 0);
                      int n = (int)strtol(rest, 0, 0);
                      if (m) { held_mask = m; held_n = n > 0 ? n : 40;
                               printf("[cellPad] PAD_FILE press 0x%04X\n", m); fflush(stdout); }
                  }
                  fclose(f);
                  if (held_n > 0) { FILE* w = fopen(pf_path, "wb"); if (w) fclose(w); }
              }
          }
          if (held_n > 0) { held_n--;
              data->button[CELL_PAD_BTN_OFFSET_DIGITAL1] |= (u16)(held_mask & 0xFF);
              data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] |= (u16)((held_mask >> 8) & 0xFF); }
      } }

    /* Analog sticks */
    /* PAD_STICK="lx,ly,rx,ry" (0-255, 128 = centred): hold the analog sticks at
     * fixed positions. Rubber Ducky drives its camera from the sticks and never
     * moves on its own, so with no host pad the view is frozen wherever it
     * started -- which can leave the subject of the demo off-screen or too far
     * away to make out. Legit input simulation, same footing as YDKJ_INJECT_PAD. */
    { static int s_st = -1;
      static unsigned char s_v[4] = {128,128,128,128};
      /* Optional 5th/6th fields: the seconds the hold starts and ends, so the menus
       * before gameplay are not driven by it (PAD_STICK="128,0,128,128,300,320"). */
      static double s_from = 0, s_until = 1e30; static unsigned long long s_t0 = 0;
      if (!s_t0) s_t0 = GetTickCount64();
      if (s_st < 0) { const char* e = getenv("PAD_STICK");
        s_st = e ? 1 : 0;
        if (e) { int a=128,b=128,c=128,d=128;
                 sscanf(e, "%d,%d,%d,%d,%lf,%lf", &a,&b,&c,&d,&s_from,&s_until);
                 s_v[0]=(unsigned char)a; s_v[1]=(unsigned char)b;
                 s_v[2]=(unsigned char)c; s_v[3]=(unsigned char)d; } }
      if (s_st && (double)(GetTickCount64() - s_t0) / 1000.0 >= s_from && (double)(GetTickCount64() - s_t0) / 1000.0 < s_until) { hs->analog_lx = s_v[0]; hs->analog_ly = s_v[1];
                  hs->analog_rx = s_v[2]; hs->analog_ry = s_v[3];
                  hs->connected = 1; }
      /* PAD_STICKS="t:lx,ly,rx,ry:dur;t:...": timed stick holds (seconds since
       * the first poll; up to 32). PAD_STICK holds one position for one window;
       * a tutorial that asks for a flick, then another, needs a list. */
      { static int ss = -1; static int n = 0;
        static struct { double t, d; unsigned char v[4]; } evs[32];
        static unsigned long long st0 = 0;
        if (ss < 0) { const char* e = getenv("PAD_STICKS"); ss = (e && *e) ? 1 : 0;
          st0 = GetTickCount64();
          const char* p = e;
          while (ss && p && *p && n < 32) {
              double t = 0, d = 0.5; int a = 128, b = 128, c = 128, dd = 128, used = 0;
              if (sscanf(p, "%lf:%d,%d,%d,%d:%lf%n", &t, &a, &b, &c, &dd, &d, &used) >= 5 && used > 0) {
                  evs[n].t = t; evs[n].d = d;
                  evs[n].v[0] = (unsigned char)a; evs[n].v[1] = (unsigned char)b;
                  evs[n].v[2] = (unsigned char)c; evs[n].v[3] = (unsigned char)dd; n++;
              }
              p = strchr(p, ';'); if (p) p++;
          }
          if (ss) printf("[cellPad] PAD_STICKS: %d evento(s)\n", n); }
        if (ss) {
            const double now = (double)(GetTickCount64() - st0) / 1000.0;
            for (int i = 0; i < n; i++)
                if (now >= evs[i].t && now < evs[i].t + evs[i].d) {
                    hs->analog_lx = evs[i].v[0]; hs->analog_ly = evs[i].v[1];
                    hs->analog_rx = evs[i].v[2]; hs->analog_ry = evs[i].v[3];
                    hs->connected = 1;
                    static int said[32];
                    if (!said[i]) { said[i] = 1;
                        printf("[cellPad] PAD_STICKS t=%.1fs l=(%u,%u) r=(%u,%u)\n", evs[i].t,
                               evs[i].v[0], evs[i].v[1], evs[i].v[2], evs[i].v[3]); }
                }
        } }
      /* PAD_SWEEP=<seconds per step>: walk the sticks through a fixed set of
       * deflections instead of holding one. Guessing a single stick position and
       * re-running costs ~5 minutes a try; a sweep answers "can input move the
       * camera anywhere useful" in one run, and the frame dumps say which step
       * paid off. Steps: centre, each left-stick direction, then each right. */
      { static int sw = -1; static clock_t t0 = 0;
        if (sw < 0) { const char* e = getenv("PAD_SWEEP");
                      sw = e ? atoi(e) : 0; if (sw < 0) sw = 0; }
        if (sw) {
            static const unsigned char steps[9][4] = {
                {128,128,128,128},
                {128,  0,128,128}, {128,255,128,128},
                {  0,128,128,128}, {255,128,128,128},
                {128,128,128,  0}, {128,128,128,255},
                {128,128,  0,128}, {128,128,255,128},
            };
            if (!t0) t0 = clock();
            long el = (long)((double)(clock() - t0) / (double)CLOCKS_PER_SEC);
            int k = (int)((el / (sw > 0 ? sw : 1)) % 9);
            hs->analog_lx = steps[k][0]; hs->analog_ly = steps[k][1];
            hs->analog_rx = steps[k][2]; hs->analog_ry = steps[k][3];
            hs->connected = 1;
            { static int lastk = -1;
              if (k != lastk) { lastk = k;
                fprintf(stderr, "[PADSWEEP] step %d: l=(%u,%u) r=(%u,%u)%c", k,
                        steps[k][0], steps[k][1], steps[k][2], steps[k][3], 10); } }
        } } }
    data->button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X] = hs->analog_rx;
    data->button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y] = hs->analog_ry;
    data->button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X]  = hs->analog_lx;
    data->button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y]  = hs->analog_ly;

    /* Pressure-sensitive buttons (only meaningful if PRESS_ON) */
    if (setting & CELL_PAD_SETTING_PRESS_ON) {
        data->button[CELL_PAD_BTN_OFFSET_PRESS_RIGHT]    = hs->press_right;
        data->button[CELL_PAD_BTN_OFFSET_PRESS_LEFT]     = hs->press_left;
        data->button[CELL_PAD_BTN_OFFSET_PRESS_UP]       = hs->press_up;
        data->button[CELL_PAD_BTN_OFFSET_PRESS_DOWN]     = hs->press_down;
        data->button[CELL_PAD_BTN_OFFSET_PRESS_TRIANGLE] = hs->press_triangle;
        data->button[CELL_PAD_BTN_OFFSET_PRESS_CIRCLE]   = hs->press_circle;
        data->button[CELL_PAD_BTN_OFFSET_PRESS_CROSS]    = hs->press_cross;
        data->button[CELL_PAD_BTN_OFFSET_PRESS_SQUARE]   = hs->press_square;
        data->button[CELL_PAD_BTN_OFFSET_PRESS_L1]       = hs->press_l1;
        data->button[CELL_PAD_BTN_OFFSET_PRESS_R1]       = hs->press_r1;
        data->button[CELL_PAD_BTN_OFFSET_PRESS_L2]       = hs->trigger_l2;
        data->button[CELL_PAD_BTN_OFFSET_PRESS_R2]       = hs->trigger_r2;
        /* A button whose digital bit is set reads full pressure. Injected
         * input (PAD_FILE/PAD_SCRIPT/PAD_AUTOPRESS, keyboard) only sets the
         * digital bits, and the host values above zeroed any pressure it had
         * written -- so a title keying its menus off the pressure words (GH3,
         * Tornado Outbreak's title screen) never saw the press. */
        static const struct { u8 word; u8 bit; u8 off; } pm[] = {
            { CELL_PAD_BTN_OFFSET_DIGITAL1, 0x20, CELL_PAD_BTN_OFFSET_PRESS_RIGHT },
            { CELL_PAD_BTN_OFFSET_DIGITAL1, 0x80, CELL_PAD_BTN_OFFSET_PRESS_LEFT },
            { CELL_PAD_BTN_OFFSET_DIGITAL1, 0x10, CELL_PAD_BTN_OFFSET_PRESS_UP },
            { CELL_PAD_BTN_OFFSET_DIGITAL1, 0x40, CELL_PAD_BTN_OFFSET_PRESS_DOWN },
            { CELL_PAD_BTN_OFFSET_DIGITAL2, 0x10, CELL_PAD_BTN_OFFSET_PRESS_TRIANGLE },
            { CELL_PAD_BTN_OFFSET_DIGITAL2, 0x20, CELL_PAD_BTN_OFFSET_PRESS_CIRCLE },
            { CELL_PAD_BTN_OFFSET_DIGITAL2, 0x40, CELL_PAD_BTN_OFFSET_PRESS_CROSS },
            { CELL_PAD_BTN_OFFSET_DIGITAL2, 0x80, CELL_PAD_BTN_OFFSET_PRESS_SQUARE },
            { CELL_PAD_BTN_OFFSET_DIGITAL2, 0x04, CELL_PAD_BTN_OFFSET_PRESS_L1 },
            { CELL_PAD_BTN_OFFSET_DIGITAL2, 0x08, CELL_PAD_BTN_OFFSET_PRESS_R1 },
            { CELL_PAD_BTN_OFFSET_DIGITAL2, 0x01, CELL_PAD_BTN_OFFSET_PRESS_L2 },
            { CELL_PAD_BTN_OFFSET_DIGITAL2, 0x02, CELL_PAD_BTN_OFFSET_PRESS_R2 } };
        for (unsigned k = 0; k < sizeof pm / sizeof pm[0]; k++)
            if ((data->button[pm[k].word] & pm[k].bit) && data->button[pm[k].off] < 255)
                data->button[pm[k].off] = 255;
    }

    /* Sensor data (only meaningful if SENSOR_ON) */
    if (setting & CELL_PAD_SETTING_SENSOR_ON) {
        /* Default sensor values: accelerometer at rest (512 = 1g center) */
        data->button[CELL_PAD_BTN_OFFSET_SENSOR_X] = 512;
        data->button[CELL_PAD_BTN_OFFSET_SENSOR_Y] = 399; /* gravity */
        data->button[CELL_PAD_BTN_OFFSET_SENSOR_Z] = 512;
        data->button[CELL_PAD_BTN_OFFSET_SENSOR_G] = 512;
    }
    }  /* end connected block */

emit:
    /* A non-zero len means "here is a CHANGE packet" -- the constant is literally
     * named CELL_PAD_LEN_CHANGE_DEFAULT. Hardware reports len = 0 once the port
     * has nothing new, and titles DRAIN on that: The Simpsons Arcade Game's
     * input stage is
     *
     *     while (cellPadGetData(port, &d) == CELL_OK && d.len > 0) { ... }
     *
     * (func_0013CFA8, loop 0x13D648 -> 0x13D040). Reporting a change on every
     * call made that loop infinite: the main thread pegged one core inside
     * libpad and the title never got past its first rendered screen.
     *
     * So hand out one packet per host report and report len = 0 for any read
     * after that until the next one arrives (pad_poll_backend sets the flag when
     * it actually re-polls, every PAD_POLL_INTERVAL_MS). That is what the
     * hardware does: a pad streams reports at a fixed rate whether or not
     * anything changed, and libpad buffers them.
     *
     * Reporting only on CHANGE instead is wrong and was tried: a HELD button
     * produces exactly one packet, so a title that reads button[] only when
     * len > 0 sees the press for a single frame and then sees nothing. In this
     * game that came out as jump being the only control that seemed to work and
     * attack firing once.
     *
     * PAD_ALWAYS_CHANGE=1 restores the old always-a-packet behaviour. */
    {
        static int always = -1;
        if (always < 0) always = getenv("PAD_ALWAYS_CHANGE") ? 1 : 0;
        if (!always && port_no < PAD_MAX_HOST_PORTS && data->len) {
            if (!s_data_fresh[port_no]) data->len = 0;   /* nothing new: end the drain */
            else                        s_data_fresh[port_no] = 0;
        }
    }
#ifdef _WIN32
    /* GOWA (26/10): GOWA_PAD_STATS=1 -- how the game reads the pad, every 5 s: reads, those that got len = 0, reads
     * in one frame (cellGcm_flip_request_count), the threads that read, and the reads that saw the right stick off
     * the centre, with and without new data (the dodge that "works sometimes"). */
    { static int on = -1;
      if (on < 0) { const char* e = getenv("GOWA_PAD_STATS"); on = (e && *e == '1') ? 1 : 0; }
      if (on && port_no == 0) {
          static unsigned long long t0;
          static unsigned leituras, len0, quadro_max, quadro_n, dir_len, dir_len0, nthr;
          static u32 quadro;
          static DWORD thr[8];
          const unsigned long long agora = pad_now_ms();
          const u32 fc = cellGcm_flip_request_count();
          if (!t0) t0 = agora;
          leituras++;
          if (!data->len) len0++;
          if (fc != quadro) { quadro = fc; quadro_n = 0; }
          if (++quadro_n > quadro_max) quadro_max = quadro_n;
          if (pad_desvio((u8)data->button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X], (u8)data->button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y]) >= PAD_FLICK_MIN) {
              if (data->len) dir_len++; else dir_len0++;
          }
          const DWORD tid = GetCurrentThreadId();
          int visto = 0;
          for (unsigned i = 0; i < nthr; i++) if (thr[i] == tid) visto = 1;
          if (!visto && nthr < 8) thr[nthr++] = tid;
          if (agora - t0 >= 5000) {
              printf("[pad-stats] %.1f s: leituras=%u len0=%u (%.0f%%) por-quadro-max=%u threads=%u direito-fora: com-dados=%u sem-dados=%u quadro=%u\n",
                     (agora - t0) / 1000.0, leituras, len0, leituras ? 100.0 * len0 / leituras : 0.0, quadro_max, nthr,
                     dir_len, dir_len0, (unsigned)fc);
              fflush(stdout);
              t0 = agora; leituras = len0 = quadro_max = dir_len = dir_len0 = 0;
          }
      } }
#endif
    {
        unsigned int ea = (unsigned int)(uintptr_t)data_guest;
        vm_write32((unsigned long long)ea + 0, (unsigned int)data->len);   /* len (s32) */
        for (int _i = 0; _i < CELL_PAD_MAX_CODES; _i++)                    /* button[] u16 */
            vm_write16((unsigned long long)ea + 4 + (unsigned int)_i * 2, data->button[_i]);
    }
    return CELL_OK;
}

/* cellPadGetInfo (v1 API, NID 0x3AAAD464) — used by PSL1GHT's ioPadGetInfo.
 * Layout (RPCS3 cellPad.h CellPadInfo, all big-endian):
 *   +0x00 u32 max_connect        +0x04 u32 now_connect   +0x08 u32 system_info
 *   +0x0C u16 vendor_id[127]     +0x10A u16 product_id[127] +0x208 u8 status[127]
 *   (CELL_MAX_PADS / PSL1GHT MAX_PADS = 127 -- NOT 7; see below)
 * Leaving this unimplemented let the guest read stack garbage as pad state and
 * run off into the weeds before ever reaching gcm init. */
s32 cellPadGetInfo(CellPadInfo2* info)
{
    uint32_t gaddr = (uint32_t)(uintptr_t)info;
    if (!s_pad_initialized)
        return CELL_PAD_ERROR_NOT_OPENED;
    if (!gaddr)
        return CELL_PAD_ERROR_INVALID_PARAMETER;

    pad_poll_backend();

    /* Present a virtual pad on port 0 when nothing physical is attached, the
     * same as cellPadGetInfo2 does.
     *
     * The two entry points disagreed: GetInfo2 carried the fallback and GetInfo
     * did not, so which one a title imports decided whether it believed a
     * controller existed. The Orange Box's chooser imports GetInfo (v1) and
     * never GetInfo2, so it saw zero pads, opened "SIXAXIS wireless controller
     * not detected", took the auto-OK, and opened it again -- forever, before
     * it ever finished booting. Both entry points describe the same hardware
     * and have to answer the same way. */
    u32 connected = 0;
    for (u32 i = 0; i < s_max_connect && i < PAD_MAX_HOST_PORTS; i++)
        if (s_host_state[i].connected || i == 0) connected++;

    vm_write32(gaddr + 0x00, s_max_connect);
    vm_write32(gaddr + 0x04, connected);
    vm_write32(gaddr + 0x08, 0);                       /* system_info */
    /* CellPadInfo (v1) sizes its arrays by CELL_MAX_PADS = 127, not by the 7
     * ports: vendor_id[127] @0x0C, product_id[127] @0x10A, status[127] @0x208.
     * Packed for 7 ports, status landed at 0x28 and GH3 -- which reads
     * info+0x208+port -- saw port 0 disconnected and never polled the pad. */
    for (u32 i = 0; i < CELL_PAD_MAX_PORT_NUM; i++) {
        int on = (i < s_max_connect && i < PAD_MAX_HOST_PORTS &&
                  (s_host_state[i].connected || i == 0));
        vm_write16(gaddr + 0x00C + i * 2, on ? 0x054C : 0);   /* vendor: Sony  */
        vm_write16(gaddr + 0x10A + i * 2, on ? 0x0268 : 0);   /* product: DS3  */
        vm_write8(gaddr + 0x208 + i, on ? CELL_PAD_STATUS_CONNECTED : 0);
    }
    return CELL_OK;
}

s32 cellPadGetInfo2(CellPadInfo2* info_guest)
{
    if (!s_pad_initialized)
        return CELL_PAD_ERROR_NOT_OPENED;

    if (!info_guest)
        return CELL_PAD_ERROR_INVALID_PARAMETER;

    /* Poll to get latest connection state */
    pad_poll_backend();

    /* Build in a local host struct, then copy to guest memory big-endian. */
    CellPadInfo2 _in; CellPadInfo2* info = &_in;
    memset(info, 0, sizeof(CellPadInfo2));
    info->max_connect = s_max_connect;

    u32 connected = 0;
    for (u32 i = 0; i < s_max_connect && i < PAD_MAX_HOST_PORTS; i++) {
        /* Always present a virtual pad on port 0 (standard emulator behavior) so
         * boot flows that block until a controller is connected can proceed even
         * with no physical device attached. */
        if (s_host_state[i].connected || i == 0) {
            info->port_status[i]       = CELL_PAD_STATUS_CONNECTED;
            info->port_setting[i]      = s_port_setting[i];
            info->device_capability[i] = CELL_PAD_CAPABILITY_PS3_CONFORMITY
                                       | CELL_PAD_CAPABILITY_PRESS_MODE
                                       | CELL_PAD_CAPABILITY_SENSOR_MODE
                                       | CELL_PAD_CAPABILITY_HP_ANALOG_STICK
                                       | CELL_PAD_CAPABILITY_ACTUATOR;
            info->device_type[i]       = CELL_PAD_DEV_TYPE_STANDARD;
            connected++;
        } else {
            info->port_status[i] = CELL_PAD_STATUS_DISCONNECTED;
        }
    }
    info->now_connect = connected;

    /* copy to guest big-endian (CellPadInfo2 is all u32 fields) */
    {
        unsigned int ea = (unsigned int)(uintptr_t)info_guest;
        for (unsigned int _o = 0; _o < sizeof(CellPadInfo2); _o += 4)
            vm_write32((unsigned long long)ea + _o, *(u32*)((char*)info + _o));
    }
    return CELL_OK;
}

/* cellPadPeriphGetInfo -- the peripheral-class view of the pad ports (NID
 * 0x4CC9B68D). LBP polls this during boot; leaving it unresolved returned
 * CELL_OK with the caller's CellPadPeriphInfo left as uninitialised stack
 * garbage, so the game read junk port_status/pclass_type for all 7 ports.
 *
 * Layout (SDK cell/pad/pad_codes.h) -- all u32, so the whole struct marshals
 * big-endian in one sweep like cellPadGetInfo2:
 *   max_connect, now_connect, system_info,
 *   port_status[7], port_setting[7], device_capability[7],
 *   device_type[7], pclass_type[7], pclass_profile[7]
 * We report a standard DUALSHOCK-class pad (pclass_type STANDARD = 0, no
 * profile bits) mirroring the ports cellPadGetInfo2 already reports. */
s32 cellPadPeriphGetInfo(CellPadPeriphInfo* info_guest)
{
    if (!s_pad_initialized)
        return CELL_PAD_ERROR_NOT_OPENED;
    if (!info_guest)
        return CELL_PAD_ERROR_INVALID_PARAMETER;

    pad_poll_backend();

    CellPadPeriphInfo _in; CellPadPeriphInfo* info = &_in;
    memset(info, 0, sizeof(CellPadPeriphInfo));
    info->max_connect = s_max_connect;
    info->system_info = 0;

    u32 connected = 0;
    for (u32 i = 0; i < s_max_connect && i < CELL_PAD_MAX_PORT_NUM; i++) {
        /* Same virtual-pad-on-port-0 rule as cellPadGetInfo2, so both views of
         * the ports agree (a game that cross-checks them must see one truth). */
        if ((i < PAD_MAX_HOST_PORTS && s_host_state[i].connected) || i == 0) {
            info->port_status[i]       = CELL_PAD_STATUS_CONNECTED;
            info->port_setting[i]      = s_port_setting[i];
            info->device_capability[i] = CELL_PAD_CAPABILITY_PS3_CONFORMITY
                                       | CELL_PAD_CAPABILITY_PRESS_MODE
                                       | CELL_PAD_CAPABILITY_SENSOR_MODE
                                       | CELL_PAD_CAPABILITY_HP_ANALOG_STICK
                                       | CELL_PAD_CAPABILITY_ACTUATOR;
            info->device_type[i]       = CELL_PAD_DEV_TYPE_STANDARD;
            info->pclass_type[i]       = CELL_PAD_PCLASS_TYPE_STANDARD;
            info->pclass_profile[i]    = 0;
            connected++;
        } else {
            info->port_status[i] = CELL_PAD_STATUS_DISCONNECTED;
        }
    }
    info->now_connect = connected;

    { unsigned int ea = (unsigned int)(uintptr_t)info_guest;
      for (unsigned int _o = 0; _o < sizeof(CellPadPeriphInfo); _o += 4)
          vm_write32((unsigned long long)ea + _o, *(u32*)((char*)info + _o)); }

    { static int _n = 0;
      if (_n++ < 2)
          printf("[cellPad] PeriphGetInfo(max=%u now=%u) -> STANDARD class\n",
                 info->max_connect, connected); }
    return CELL_OK;
}

s32 cellPadSetPortSetting(u32 port_no, u32 port_setting)
{
    printf("[cellPad] SetPortSetting(port=%u, setting=0x%X)\n",
           port_no, port_setting);

    if (!s_pad_initialized)
        return CELL_PAD_ERROR_NOT_OPENED;

    if (port_no >= CELL_PAD_MAX_PORT_NUM)
        return CELL_PAD_ERROR_INVALID_PARAMETER;

    s_port_setting[port_no] = port_setting;
    return CELL_OK;
}

/* cellPadInfoPressMode (NID 0x0E2DFAAD) / cellPadSetPressMode (NID 0xF83F8182)
 *
 * Press mode makes the digital buttons report an analog 0..255 pressure in the
 * extended part of CellPadData. A DS3 supports it, and the virtual pad reported
 * by cellPadGetInfo/GetInfo2 already advertises CELL_PAD_CAPABILITY_PRESS_MODE,
 * so these have to agree with that claim rather than fall through to the
 * unresolved-NID path.
 *
 * Info returns 1 when the port supports press mode; Set toggles it, recorded in
 * the same s_port_setting word cellPadSetPortSetting uses so a title that reads
 * the setting back sees what it asked for. */
s32 cellPadInfoPressMode(u32 port_no)
{
    if (!s_pad_initialized)
        return CELL_PAD_ERROR_NOT_OPENED;
    if (port_no >= CELL_PAD_MAX_PORT_NUM)
        return CELL_PAD_ERROR_INVALID_PARAMETER;
    return 1;                                  /* supported */
}

s32 cellPadSetPressMode(u32 port_no, u32 mode)
{
    if (!s_pad_initialized)
        return CELL_PAD_ERROR_NOT_OPENED;
    if (port_no >= CELL_PAD_MAX_PORT_NUM)
        return CELL_PAD_ERROR_INVALID_PARAMETER;
    if (mode)
        s_port_setting[port_no] |= CELL_PAD_SETTING_PRESS_ON;
    else
        s_port_setting[port_no] &= ~(u32)CELL_PAD_SETTING_PRESS_ON;
    return CELL_OK;
}

s32 cellPadGetCapabilityInfo(u32 port_no, CellPadCapabilityInfo* info)
{
    if (!s_pad_initialized)
        return CELL_PAD_ERROR_NOT_OPENED;

    if (port_no >= CELL_PAD_MAX_PORT_NUM || !info)
        return CELL_PAD_ERROR_INVALID_PARAMETER;

    u32 info_ea = (u32)(uintptr_t)info;
    for (u32 i = 0; i < CELL_PAD_MAX_CODES; i++)
        vm_write32(info_ea + i * 4, 0);

    /* Report standard DualShock 3 capabilities */
    vm_write32(info_ea, CELL_PAD_CAPABILITY_PS3_CONFORMITY
                        | CELL_PAD_CAPABILITY_PRESS_MODE
                        | CELL_PAD_CAPABILITY_SENSOR_MODE
                        | CELL_PAD_CAPABILITY_HP_ANALOG_STICK
                        | CELL_PAD_CAPABILITY_ACTUATOR);

    return CELL_OK;
}

s32 cellPadSetActDirect(u32 port_no, CellPadActParam* param)
{
    if (!s_pad_initialized)
        return CELL_PAD_ERROR_NOT_OPENED;

    if (port_no >= CELL_PAD_MAX_PORT_NUM || !param)
        return CELL_PAD_ERROR_INVALID_PARAMETER;

    param = GUEST_PTR(param, CellPadActParam*);

#if PAD_BACKEND_XINPUT
    /* Map to XInput vibration */
    if (port_no < PAD_MAX_HOST_PORTS && s_host_state[port_no].connected) {
        XINPUT_VIBRATION vib;
        vib.wLeftMotorSpeed  = (WORD)(param->motor[CELL_PAD_ACTUATOR_PARAM_LARGE] * 257);
        vib.wRightMotorSpeed = (WORD)(param->motor[CELL_PAD_ACTUATOR_PARAM_SMALL] * 257);
        XInputSetState((DWORD)port_no, &vib);
    }
#endif

#if PAD_BACKEND_SDL2
    if (port_no < PAD_MAX_HOST_PORTS && s_sdl_controllers[port_no]) {
        SDL_GameControllerRumble(
            s_sdl_controllers[port_no],
            (Uint16)(param->motor[CELL_PAD_ACTUATOR_PARAM_LARGE] * 257),
            (Uint16)(param->motor[CELL_PAD_ACTUATOR_PARAM_SMALL] * 257),
            100 /* duration ms */
        );
    }
#endif

    return CELL_OK;
}

s32 cellPadClearBuf(u32 port_no)
{
    if (!s_pad_initialized)
        return CELL_PAD_ERROR_NOT_OPENED;

    if (port_no >= CELL_PAD_MAX_PORT_NUM)
        return CELL_PAD_ERROR_INVALID_PARAMETER;

    /* Nothing to clear in our implementation -- state is polled fresh */
    return CELL_OK;
}
