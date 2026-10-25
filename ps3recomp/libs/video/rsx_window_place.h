/* rsx_window_place.h -- where the RSX backends open their window.
 *
 * PS3_WINDOW_MONITOR=2 (any value but 0/1): open on the first non-primary
 * monitor, scaled down (aspect kept) to fit its work area, and shown without
 * taking focus -- test runs stay out of the way of whoever is using the main
 * screen. The presenters stretch to the client area (DXGI_SCALING_STRETCH /
 * StretchDIBits), so a smaller window only scales the picture.
 */
#ifndef RSX_WINDOW_PLACE_H
#define RSX_WINDOW_PLACE_H
#ifdef _WIN32
#include <windows.h>
#include <stdlib.h>

struct rsx_mon_pick { RECT work; int found; };
static BOOL CALLBACK rsx_mon_pick_cb(HMONITOR m, HDC dc, LPRECT r, LPARAM lp)
{
    (void)dc; (void)r;
    struct rsx_mon_pick* pick = (struct rsx_mon_pick*)lp;
    MONITORINFO mi; mi.cbSize = sizeof mi;
    if (GetMonitorInfoA(m, &mi) && !(mi.dwFlags & MONITORINFOF_PRIMARY)) {
        pick->work = mi.rcWork; pick->found = 1;
        return FALSE;
    }
    return TRUE;
}

/* Creates the window (WS_OVERLAPPEDWINDOW, client width x height) and shows it. */
static HWND rsx_create_placed_window(const char* cls, const char* title, unsigned width, unsigned height)
{
    RECT wr = {0, 0, (LONG)width, (LONG)height};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
    int w = wr.right - wr.left, h = wr.bottom - wr.top;
    int other = 0;
    { const char* e = getenv("PS3_WINDOW_MONITOR"); other = e && e[0] && e[0] != '1' && e[0] != '0'; }
    if (other) {
        struct rsx_mon_pick pick = {{0}, 0};
        EnumDisplayMonitors(NULL, NULL, rsx_mon_pick_cb, (LPARAM)&pick);
        if (pick.found) {
            const int bw = w - (int)width, bh = h - (int)height;    /* frame size */
            const int aw = pick.work.right - pick.work.left - bw, ah = pick.work.bottom - pick.work.top - bh;
            int cw = (int)width, ch = (int)height;
            if (cw > aw) { ch = (int)((long long)ch * aw / cw); cw = aw; }
            if (ch > ah) { cw = (int)((long long)cw * ah / ch); ch = ah; }
            w = cw + bw; h = ch + bh;
            x = pick.work.left + (pick.work.right - pick.work.left - w) / 2;
            y = pick.work.top + (pick.work.bottom - pick.work.top - h) / 2;
        } else other = 0;
    }
    HWND hwnd = CreateWindowExA(0, cls, title, WS_OVERLAPPEDWINDOW | (other ? 0 : WS_VISIBLE),
                                x, y, w, h, NULL, NULL, GetModuleHandle(NULL), NULL);
    if (hwnd && other) ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    return hwnd;
}
#endif
#endif
