#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdio>

#define I int
#define F float
#define B bool
#define V void
#define S struct
#define C const

S Z_01 { F x, y; };
S Z_02 { Z_01 p; B a; };
S Z_03 { Z_01 p, v; F s; I t; B a; };

HINSTANCE H_01; HWND H_02; HDC H_03; HDC H_04; HBITMAP H_05;
Z_01 G_01 = { 50.0f, 300.0f };
std::vector<Z_02> G_02;
std::vector<Z_03> G_03;
I G_04 = 0; I G_05 = 0;
B G_06 = false;
F G_07 = 0.0f; F G_08 = 0.0f;
B G_09 = false; F G_10 = 0.0f;
B G_11 = false; 

F Z_SCL = 1.0f; I Z_OX = 0, Z_OY = 0;

V Z_CALC_AR() {
    RECT r; GetClientRect(H_02, &r);
    F sw = (F)r.right / 800.0f;
    F sh = (F)r.bottom / 600.0f;
    Z_SCL = (sw < sh) ? sw : sh;
    Z_OX = (I)((r.right - (800.0f * Z_SCL)) / 2.0f);
    Z_OY = (I)((r.bottom - (600.0f * Z_SCL)) / 2.0f);
}

V Z_F11() {
    static B f = false; static WINDOWPLACEMENT w = { sizeof(WINDOWPLACEMENT) };
    DWORD s = GetWindowLong(H_02, GWL_STYLE);
    if (f) {
        SetWindowLong(H_02, GWL_STYLE, s | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(H_02, &w);
        SetWindowPos(H_02, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        ShowCursor(TRUE);
    } else {
        GetWindowPlacement(H_02, &w);
        SetWindowLong(H_02, GWL_STYLE, s & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(H_02, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_FRAMECHANGED);
        ShowCursor(FALSE);
    }
    f = !f;
}

V Z_X1() {
    if (G_04 > G_05) {
        G_05 = G_04;
        C char* f = "sys_log.dat"; SetFileAttributesA(f, FILE_ATTRIBUTE_NORMAL);
        FILE* p = fopen(f, "wb");
        if (p) {
            unsigned I k = 0x55AA55AA; unsigned I e = (unsigned I)G_05 ^ k;
            fwrite(&e, sizeof(I), 1, p); fclose(p);
            SetFileAttributesA(f, FILE_ATTRIBUTE_HIDDEN);
        }
    }
}

V Z_X2() {
    FILE* p = fopen("sys_log.dat", "rb");
    if (p) {
        unsigned I k = 0x55AA55AA; unsigned I e = 0;
        fread(&e, sizeof(I), 1, p); G_05 = (I)(e ^ k); fclose(p);
    }
}

B Z_X3(F x1, F y1, F r1, F x2, F y2, F r2) {
    F dx = x1 - x2; F dy = y1 - y2; return (dx*dx + dy*dy) <= ((r1+r2)*(r1+r2));
}

V Z_LOOP() {
    Z_CALC_AR();
    if (GetAsyncKeyState(VK_F11) & 1) Z_F11();
    if (G_06) {
        if (GetAsyncKeyState(VK_RETURN) & 0x8000) {
            G_01 = { 50.0f, 300.0f }; G_02.clear(); G_03.clear();
            G_04 = 0; G_06 = false; G_08 = 0; G_09 = false; G_11 = false;
        } return;
    }
    if (GetAsyncKeyState('M') & 1) { G_11 = !G_11; ShowCursor(!G_11); }

    F spd = 400.0f * G_07;
    if (G_11) {
        POINT p; GetCursorPos(&p); ScreenToClient(H_02, &p);
        G_01.x = ((F)(p.x - Z_OX) / Z_SCL) - 15;
        G_01.y = ((F)(p.y - Z_OY) / Z_SCL) - 10;
    } else {
        if (GetAsyncKeyState(VK_UP) & 0x8000) G_01.y -= spd;
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) G_01.y += spd;
        if (GetAsyncKeyState(VK_LEFT) & 0x8000) G_01.x -= spd;
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) G_01.x += spd;
    }

    if (G_01.y < 0) G_01.y = 0; if (G_01.y > 580) G_01.y = 580;
    if (G_01.x < 0) G_01.x = 0; if (G_01.x > 770) G_01.x = 770;

    static F cd = 0; if (cd > 0) cd -= G_07;
    B shoot = (GetAsyncKeyState(VK_SPACE) & 0x8000) || (G_11 && (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
    if (shoot && cd <= 0) {
        G_02.push_back({ {G_01.x + 30, G_01.y + 10}, true }); cd = 0.15f;
    }

    if (G_08 > 0) G_08 -= G_07;
    B bomb = (GetAsyncKeyState('B') & 0x8000) || (GetAsyncKeyState(VK_RBUTTON) & 0x8000);
    if (bomb && G_08 <= 0) { G_09 = true; G_10 = 10.0f; G_08 = 30.0f; }

    if (G_09) {
        G_10 += 1200.0f * G_07;
        for (auto& a : G_03) {
            if (a.a && Z_X3(G_01.x + 15, G_01.y + 10, G_10, a.p.x, a.p.y, a.s)) { a.a = false; G_04 += 5; }
        }
        if (G_10 > 1200.0f) G_09 = false;
    }

    for (auto& b : G_02) {
        if (!b.a) continue; b.p.x += 800.0f * G_07; if (b.p.x > 800) b.a = false;
    }

    if (rand() % 50 == 0) {
        F vy = (F)((rand() % 200) - 100);
        G_03.push_back({ { 850, (F)(rand() % 600) }, { -200.0f - (G_04 / 10.0f), vy }, 30.0f, 3, true });
    }

    for (auto& a : G_03) {
        if (!a.a) continue; a.p.x += a.v.x * G_07; a.p.y += a.v.y * G_07;
        if (a.p.x < -50) a.a = false;
        if (Z_X3(G_01.x + 15, G_01.y + 10, 15, a.p.x, a.p.y, a.s)) { G_06 = true; Z_X1(); }
        for (auto& b : G_02) {
            if (b.a && a.a && Z_X3(b.p.x, b.p.y, 5, a.p.x, a.p.y, a.s)) {
                b.a = false; a.a = false; G_04 += 10;
                if (a.t > 1) {
                    for(I i=0; i<2; i++) G_03.push_back({ a.p, { a.v.x * 1.2f, (F)((rand()%300)-150) }, a.s / 2, a.t - 1, true });
                }
            }
        }
    }
    G_02.erase(std::remove_if(G_02.begin(), G_02.end(), [](C Z_02& b){ return !b.a; }), G_02.end());
    G_03.erase(std::remove_if(G_03.begin(), G_03.end(), [](C Z_03& a){ return !a.a; }), G_03.end());
}

V Z_DRAW() {
    RECT r = {0, 0, 800, 600}; FillRect(H_04, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
    if (!G_06) {
        if (G_09) {
            HPEN p = CreatePen(PS_SOLID, 5, RGB(255, 100, 0)); SelectObject(H_04, p); SelectObject(H_04, GetStockObject(NULL_BRUSH));
            Ellipse(H_04, (I)(G_01.x+15-G_10), (I)(G_01.y+10-G_10), (I)(G_01.x+15+G_10), (I)(G_01.y+10+G_10)); DeleteObject(p);
        }
        HBRUSH sb = CreateSolidBrush(RGB(0, 200, 255)); RECT sr = { (I)G_01.x, (I)G_01.y, (I)G_01.x + 30, (I)G_01.y + 20 };
        FillRect(H_04, &sr, sb); DeleteObject(sb);

        HPEN bp = CreatePen(PS_SOLID, 2, RGB(255, 255, 0)); SelectObject(H_04, bp);
        for (C auto& b : G_02) { MoveToEx(H_04, (I)b.p.x, (I)b.p.y, NULL); LineTo(H_04, (I)b.p.x+10, (I)b.p.y); } DeleteObject(bp);

        HBRUSH rb = CreateSolidBrush(RGB(200, 50, 50)); SelectObject(H_04, rb);
        for (C auto& a : G_03) Ellipse(H_04, (I)(a.p.x-a.s), (I)(a.p.y-a.s), (I)(a.p.x+a.s), (I)(a.p.y+a.s)); DeleteObject(rb);

        RECT bb = { 200, 575, 600, 590 }; HBRUSH gb = CreateSolidBrush(RGB(50, 50, 50)); FillRect(H_04, &bb, gb); DeleteObject(gb);
        F pc = 1.0f - (G_08 / 30.0f); if(pc<0) pc=0; if(pc>1) pc=1;
        RECT fb = { 200, 575, 200 + (I)(400 * pc), 590 };
        HBRUSH cb = CreateSolidBrush(pc >= 1.0f ? RGB(0, 255, 0) : RGB(255, 0, 0)); FillRect(H_04, &fb, cb); DeleteObject(cb);

        SetBkMode(H_04, TRANSPARENT); SetTextColor(H_04, RGB(255, 255, 255));
        char t[64]; sprintf(t, "SCORE: %d  HI: %d%s", G_04, G_05, G_11 ? " [MOUSE]" : "");
        TextOutA(H_04, 10, 10, t, (I)strlen(t));
    } else {
        SetBkMode(H_04, TRANSPARENT); SetTextColor(H_04, RGB(255, 0, 0));
        TextOutA(H_04, 350, 280, "FAIL", 4); SetTextColor(H_04, RGB(200, 200, 200));
        TextOutA(H_04, 330, 310, "ENTER TO RESTART", 16);
    }
    
    // FLICKER FIX: Arka planı elle silme, sadece boşlukları siyah yap
    RECT wr; GetClientRect(H_02, &wr);
    
    // Üst Bant
    RECT r1 = {0, 0, wr.right, Z_OY}; FillRect(H_03, &r1, (HBRUSH)GetStockObject(BLACK_BRUSH));
    // Alt Bant
    RECT r2 = {0, wr.bottom - Z_OY, wr.right, wr.bottom}; FillRect(H_03, &r2, (HBRUSH)GetStockObject(BLACK_BRUSH));
    // Sol Bant
    RECT r3 = {0, Z_OY, Z_OX, wr.bottom - Z_OY}; FillRect(H_03, &r3, (HBRUSH)GetStockObject(BLACK_BRUSH));
    // Sağ Bant
    RECT r4 = {wr.right - Z_OX, Z_OY, wr.right, wr.bottom - Z_OY}; FillRect(H_03, &r4, (HBRUSH)GetStockObject(BLACK_BRUSH));

    SetStretchBltMode(H_03, COLORONCOLOR);
    StretchBlt(H_03, Z_OX, Z_OY, (I)(800*Z_SCL), (I)(600*Z_SCL), H_04, 0, 0, 800, 600, SRCCOPY);
}

// BURASI KRİTİK: WM_ERASEBKGND YAKALAMA
LRESULT CALLBACK Z_PROC(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
    if (m == WM_ERASEBKGND) return 1; // FLICKER ÖNLEYİCİ SİHİRLİ KOD
    return DefWindowProc(h, m, w, l);
}

I WINAPI WinMain(HINSTANCE h, HINSTANCE p, LPSTR c, I n) {
    H_01 = h; WNDCLASSA wc = { 0 }; wc.lpfnWndProc = Z_PROC; wc.hInstance = h; wc.lpszClassName = "Z_CLS";
    RegisterClassA(&wc);
    H_02 = CreateWindowA("Z_CLS", "Z_APP", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, 100, 100, 800, 630, NULL, NULL, h, NULL);
    H_03 = GetDC(H_02); H_04 = CreateCompatibleDC(H_03);
    H_05 = CreateCompatibleBitmap(H_03, 800, 600); SelectObject(H_04, H_05);
    ShowWindow(H_02, n); Z_X2();
    MSG m = { 0 }; LARGE_INTEGER f, l, cr; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&l);
    while (m.message != WM_QUIT) {
        if (PeekMessage(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessage(&m); }
        else {
            QueryPerformanceCounter(&cr); G_07 = (F)(cr.QuadPart - l.QuadPart) / f.QuadPart; l = cr;
            Z_LOOP(); Z_DRAW(); Sleep(1);
        }
    } Z_X1(); return 0;
}
