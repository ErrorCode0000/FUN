#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <vector>
#include <cmath>
#include <string>
#include <chrono>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "msimg32.lib")

// --- ENGINE CONSTANTS ---
const int SCREEN_W = 800;
const int SCREEN_H = 600;

struct Vec2 {
    float x, y;
    Vec2(float _x = 0, float _y = 0) : x(_x), y(_y) {}
    Vec2 operator+(const Vec2& v) const { return Vec2(x + v.x, y + v.y); }
    Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
};

struct Bullet {
    Vec2 pos;
    bool active = true;
};

struct Asteroid {
    Vec2 pos, vel;
    float size;
    int tier = 3; // Triple Tampering: 3 -> 2 -> 1
    bool active = true;
};

// --- GLOBAL ENGINE STATE ---
Vec2 shipPos(50, 300);
std::vector<Bullet> bullets;
std::vector<Asteroid> asteroids;
float shootCooldown = 0;
int score = 0;
bool isGameOver = false;

// Triple Buffering Handles (GPU Optimized)
HDC hdcBack, hdcMid;
HBITMAP bmpBack, bmpMid;

void ResetEngine() {
    shipPos = Vec2(50, 300);
    bullets.clear();
    asteroids.clear();
    score = 0;
    isGameOver = false;
    shootCooldown = 0;
}

void Update(float dt) {
    if (isGameOver) {
        if (GetAsyncKeyState(VK_RETURN)) ResetEngine();
        return;
    }

    // 1. Mermi Sistemi & Cooldown
    if (shootCooldown > 0) shootCooldown -= dt;
    if (GetAsyncKeyState(VK_SPACE) && shootCooldown <= 0) {
        bullets.push_back({ Vec2(shipPos.x + 35, shipPos.y + 10) });
        shootCooldown = 0.12f; 
    }

    // 2. 4-Yönlü Hareket Control
    float shipSpeed = 550.0f;
    if (GetAsyncKeyState(VK_UP))    shipPos.y -= shipSpeed * dt;
    if (GetAsyncKeyState(VK_DOWN))  shipPos.y += shipSpeed * dt;
    if (GetAsyncKeyState(VK_LEFT))  shipPos.x -= shipSpeed * dt;
    if (GetAsyncKeyState(VK_RIGHT)) shipPos.x += shipSpeed * dt;

    // --- SCREEN BOUNDARY CLAMP (Ekran dışına çıkışı engeller) ---
    if (shipPos.x < 0) shipPos.x = 0;
    if (shipPos.x > SCREEN_W - 30) shipPos.x = (float)SCREEN_W - 30;
    if (shipPos.y < 0) shipPos.y = 0;
    if (shipPos.y > SCREEN_H - 20) shipPos.y = (float)SCREEN_H - 20;

    // 3. Mermi Fiziği & Collision (Triple Tampering)
    for (size_t i = 0; i < bullets.size(); i++) {
        if (!bullets[i].active) continue;
        bullets[i].pos.x += 1100.0f * dt;
        
        if (bullets[i].pos.x > SCREEN_W) bullets[i].active = false;

        for (auto& a : asteroids) {
            if (!a.active) continue;
            float dx = bullets[i].pos.x - a.pos.x;
            float dy = bullets[i].pos.y - a.pos.y;
            if (sqrt(dx*dx + dy*dy) < a.size) {
                bullets[i].active = false; 
                a.active = false; 
                score += 20;
                
                // Parçalanma Mekaniği
                if (a.tier > 1) {
                    for (int j = 0; j < 3; j++) {
                        Asteroid sub;
                        sub.pos = a.pos;
                        sub.size = a.size * 0.55f;
                        sub.tier = a.tier - 1;
                        sub.vel = Vec2(a.vel.x * 1.25f, (float)(rand() % 500 - 250));
                        asteroids.push_back(sub);
                    }
                }
            }
        }
    }

    // 4. Asteroid Spawning & Physics
    if (rand() % 35 == 0) {
        Asteroid a;
        a.pos = Vec2(SCREEN_W + 60, (float)(rand() % SCREEN_H));
        a.vel = Vec2(-(280.0f + (score * 0.4f)), (float)(rand() % 80 - 40));
        a.size = 38.0f;
        asteroids.push_back(a);
    }

    for (auto& a : asteroids) {
        if (!a.active) continue;
        a.pos = a.pos + a.vel * dt;

        // Gemi Çarpışma Kontrolü
        float dx = (shipPos.x + 15) - a.pos.x;
        float dy = (shipPos.y + 10) - a.pos.y;
        if (sqrt(dx*dx + dy*dy) < (14 + a.size)) isGameOver = true;

        if (a.pos.x < -100) a.active = false;
    }

    // Temizlik: Aktif olmayan nesneleri temizle (Performans için)
    if (asteroids.size() > 200) {
        asteroids.erase(std::remove_if(asteroids.begin(), asteroids.end(), [](const Asteroid& a){ return !a.active; }), asteroids.end());
    }
}

void Render(HDC hdc) {
    // TRIPLE BUFFERING START: Clear Back Buffer
    FillRect(hdcBack, &RECT{0, 0, SCREEN_W, SCREEN_H}, (HBRUSH)GetStockObject(BLACK_BRUSH));

    // Mermileri Çiz (Neon Efekti)
    HPEN bulletPen = CreatePen(PS_SOLID, 3, RGB(0, 255, 255));
    SelectObject(hdcBack, bulletPen);
    for (const auto& b : bullets) if (b.active) {
        MoveToEx(hdcBack, (int)b.pos.x, (int)b.pos.y, NULL);
        LineTo(hdcBack, (int)b.pos.x + 15, (int)b.pos.y);
    }
    DeleteObject(bulletPen);

    // Asteroidleri Çiz (Tier tabanlı renk değişimi)
    for (const auto& a : asteroids) if (a.active) {
        COLORREF col = (a.tier == 3) ? RGB(90, 90, 95) : (a.tier == 2 ? RGB(220, 60, 20) : RGB(255, 230, 0));
        HBRUSH br = CreateSolidBrush(col);
        SelectObject(hdcBack, br);
        Ellipse(hdcBack, (int)(a.pos.x - a.size), (int)(a.pos.y - a.size), (int)(a.pos.x + a.size), (int)(a.pos.y + a.size));
        DeleteObject(br);
    }

    // Gemiyi Çiz
    if (!isGameOver) {
        HBRUSH shipBr = CreateSolidBrush(RGB(240, 240, 255));
        SelectObject(hdcBack, shipBr);
        Rectangle(hdcBack, (int)shipPos.x, (int)shipPos.y, (int)shipPos.x + 30, (int)shipPos.y + 20);
        DeleteObject(shipBr);
    }

    // UI Overlay
    SetTextColor(hdcBack, RGB(255, 255, 255));
    SetBkMode(hdcBack, TRANSPARENT);
    std::wstring ui = isGameOver ? L"CRITICAL FAILURE - PRESS ENTER TO REBOOT" : L"RED HORIZON V1.0 | SCORE: " + std::to_wstring(score);
    TextOut(hdcBack, 15, 15, ui.c_str(), (int)ui.length());

    // TRIPLE BUFFER FLIP (GPU Optimized Transfer)
    BitBlt(hdcMid, 0, 0, SCREEN_W, SCREEN_H, hdcBack, 0, 0, SRCCOPY);
    BitBlt(hdc, 0, 0, SCREEN_W, SCREEN_H, hdcMid, 0, 0, SRCCOPY);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hI, HINSTANCE, PWSTR, int) {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc; wc.hInstance = hI; wc.lpszClassName = L"RedHorizonEngine";
    RegisterClass(&wc);
    HWND hwnd = CreateWindowEx(0, L"RedHorizonEngine", L"PROJECT: RED HORIZON V1.0", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 816, 639, 0, 0, hI, 0);

    HDC hdc = GetDC(hwnd);
    // Triple Buffer Setup
    hdcBack = CreateCompatibleDC(hdc);
    bmpBack = CreateCompatibleBitmap(hdc, SCREEN_W, SCREEN_H);
    SelectObject(hdcBack, bmpBack);
    hdcMid = CreateCompatibleDC(hdc);
    bmpMid = CreateCompatibleBitmap(hdc, SCREEN_W, SCREEN_H);
    SelectObject(hdcMid, bmpMid);

    MSG msg = { 0 };
    auto lastTime = std::chrono::high_resolution_clock::now();
    
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            auto now = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(now - lastTime).count();
            lastTime = now;

            Update(deltaTime);
            Render(hdc);
            Sleep(1); // Anti-CPU hogging
        }
    }
    
    // Cleanup
    DeleteObject(bmpBack); DeleteDC(hdcBack);
    DeleteObject(bmpMid); DeleteDC(hdcMid);
    ReleaseDC(hwnd, hdc);
    return 0;
}