/*
 * PROJECT: RED HORIZON
 * Platform: Windows (Native) & Linux (via Wine)
 * Description: Space Shooter Game Engine using GDI
 */

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
#include <algorithm>

// Linker directives for Visual Studio / cl.exe
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "msimg32.lib")

// --- ENGINE CONSTANTS ---
const int SCREEN_W = 800;
const int SCREEN_H = 600;

// GÜNCELLEME BURADA: 5.0f yerine 30.0f yaptık.
const float BOMB_COOLDOWN_MAX = 30.0f; 

const float PLAYER_SPEED = 550.0f;
const float BULLET_SPEED = 1100.0f;

struct Vec2 {
    float x, y;
    Vec2(float _x = 0, float _y = 0) : x(_x), y(_y) {}
    Vec2 operator+(const Vec2& v) const { return Vec2(x + v.x, y + v.y); }
    Vec2 operator-(const Vec2& v) const { return Vec2(x - v.x, y - v.y); }
    Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
};

struct Bullet {
    Vec2 pos;
    bool active = true;
};

struct Asteroid {
    Vec2 pos, vel;
    float size;
    int tier = 3; // 3: Large, 2: Medium, 1: Small
    bool active = true;
};

// --- GLOBAL GAME STATE ---
Vec2 shipPos(50, 300);
std::vector<Bullet> bullets;
std::vector<Asteroid> asteroids;

// Game Logic Variables
float shootCooldown = 0;
float bombTimer = 0;      // 0 means ready
bool isBombActive = false;
float bombRadius = 0;
int score = 0;
bool isGameOver = false;

// Control State
bool isMouseMode = false; // Toggled by 'M'
POINT cursorPos = { 0, 0 };

// Rendering Resources
HDC hdcBack, hdcMid;
HBITMAP bmpBack, bmpMid;
HWND hGameWnd;

// --- UTILITIES ---
void ResetEngine() {
    shipPos = Vec2(50, 300);
    bullets.clear();
    asteroids.clear();
    score = 0;
    isGameOver = false;
    shootCooldown = 0;
    bombTimer = 0;
    isBombActive = false;
}

float Dist(Vec2 a, Vec2 b) {
    return sqrt(pow(a.x - b.x, 2) + pow(a.y - b.y, 2));
}

// --- CORE LOOPS ---
void Update(float dt) {
    if (isGameOver) {
        if (GetAsyncKeyState(VK_RETURN) & 0x8000) ResetEngine();
        return;
    }

    // 1. INPUT HANDLING & MODE TOGGLE
    static bool mKeyPressed = false;
    if (GetAsyncKeyState('M') & 0x8000) {
        if (!mKeyPressed) {
            isMouseMode = !isMouseMode;
            mKeyPressed = true;
        }
    } else {
        mKeyPressed = false;
    }

    // Cooldown Management
    if (shootCooldown > 0) shootCooldown -= dt;
    if (bombTimer > 0) bombTimer -= dt;

    // Movement & Action Logic
    if (isMouseMode) {
        // --- MOUSE CONTROL ---
        GetCursorPos(&cursorPos);
        ScreenToClient(hGameWnd, &cursorPos);
        
        // Direct tracking
        shipPos.x = (float)cursorPos.x - 15;
        shipPos.y = (float)cursorPos.y - 10;

        // Shoot (Left Click)
        if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) && shootCooldown <= 0) {
            bullets.push_back({ Vec2(shipPos.x + 35, shipPos.y + 10) });
            shootCooldown = 0.12f;
        }

        // Bomb (Right Click)
        if ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) && bombTimer <= 0) {
            isBombActive = true;
            bombRadius = 10.0f;
            bombTimer = BOMB_COOLDOWN_MAX;
        }

    } else {
        // --- KEYBOARD CONTROL (WASD + ARROWS) ---
        if (GetAsyncKeyState(VK_UP) || GetAsyncKeyState('W'))    shipPos.y -= PLAYER_SPEED * dt;
        if (GetAsyncKeyState(VK_DOWN) || GetAsyncKeyState('S'))  shipPos.y += PLAYER_SPEED * dt;
        if (GetAsyncKeyState(VK_LEFT) || GetAsyncKeyState('A'))  shipPos.x -= PLAYER_SPEED * dt;
        if (GetAsyncKeyState(VK_RIGHT) || GetAsyncKeyState('D')) shipPos.x += PLAYER_SPEED * dt;

        // Shoot (Space)
        if ((GetAsyncKeyState(VK_SPACE) & 0x8000) && shootCooldown <= 0) {
            bullets.push_back({ Vec2(shipPos.x + 35, shipPos.y + 10) });
            shootCooldown = 0.12f;
        }
        
        // Bomb (B Key)
        if ((GetAsyncKeyState('B') & 0x8000) && bombTimer <= 0) {
            isBombActive = true;
            bombRadius = 10.0f;
            bombTimer = BOMB_COOLDOWN_MAX;
        }
    }

    // Screen Clamping
    if (shipPos.x < 0) shipPos.x = 0;
    if (shipPos.x > SCREEN_W - 30) shipPos.x = (float)SCREEN_W - 30;
    if (shipPos.y < 0) shipPos.y = 0;
    if (shipPos.y > SCREEN_H - 20) shipPos.y = (float)SCREEN_H - 20;

    // 2. BOMB MECHANIC
    if (isBombActive) {
        bombRadius += 1000.0f * dt; 
        
        for (auto& a : asteroids) {
            if (a.active && Dist(shipPos, a.pos) < bombRadius) {
                a.active = false;
                score += 5;
            }
        }
        if (bombRadius > SCREEN_W) isBombActive = false;
    }

    // 3. BULLET PHYSICS
    for (auto& b : bullets) {
        if (!b.active) continue;
        b.pos.x += BULLET_SPEED * dt;
        if (b.pos.x > SCREEN_W) b.active = false;

        for (auto& a : asteroids) {
            if (!a.active) continue;
            if (Dist(b.pos, a.pos) < a.size + 5) {
                b.active = false; 
                a.active = false; 
                score += 20;
                
                // Fragmentation
                if (a.tier > 1) {
                    for (int j = 0; j < 2; j++) {
                        Asteroid sub;
                        sub.pos = a.pos;
                        sub.size = a.size * 0.6f;
                        sub.tier = a.tier - 1;
                        sub.vel = Vec2(a.vel.x * 1.2f, (float)(rand() % 400 - 200));
                        asteroids.push_back(sub);
                    }
                }
            }
        }
    }

    // 4. ASTEROID SPAWNING
    if (rand() % 40 == 0) {
        Asteroid a;
        a.pos = Vec2(SCREEN_W + 60, (float)(rand() % SCREEN_H));
        a.vel = Vec2(-(250.0f + (score * 0.2f)), (float)(rand() % 100 - 50));
        a.size = 38.0f;
        asteroids.push_back(a);
    }

    for (auto& a : asteroids) {
        if (!a.active) continue;
        a.pos = a.pos + a.vel * dt;

        // Player Collision
        if (Dist(Vec2(shipPos.x + 15, shipPos.y + 10), a.pos) < (15 + a.size)) isGameOver = true;
        if (a.pos.x < -100) a.active = false;
    }

    // Cleanup
    asteroids.erase(std::remove_if(asteroids.begin(), asteroids.end(), [](const Asteroid& a){ return !a.active; }), asteroids.end());
    bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [](const Bullet& b){ return !b.active; }), bullets.end());
}

void Render(HDC hdc) {
    // Clear Screen (Double/Triple Buffering)
    FillRect(hdcBack, &RECT{0, 0, SCREEN_W, SCREEN_H}, (HBRUSH)GetStockObject(BLACK_BRUSH));

    // Draw Bomb Shockwave
    if (isBombActive) {
        HBRUSH bombBrush = CreateSolidBrush(RGB(255, 100, 0));
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdcBack, bombBrush);
        Ellipse(hdcBack, 
            (int)(shipPos.x + 15 - bombRadius), (int)(shipPos.y + 10 - bombRadius),
            (int)(shipPos.x + 15 + bombRadius), (int)(shipPos.y + 10 + bombRadius));
        SelectObject(hdcBack, oldBrush);
        DeleteObject(bombBrush);
    }

    // Draw Bullets
    HPEN bulletPen = CreatePen(PS_SOLID, 3, RGB(0, 255, 255));
    HPEN oldPen = (HPEN)SelectObject(hdcBack, bulletPen);
    for (const auto& b : bullets) if (b.active) {
        MoveToEx(hdcBack, (int)b.pos.x, (int)b.pos.y, NULL);
        LineTo(hdcBack, (int)b.pos.x + 15, (int)b.pos.y);
    }
    SelectObject(hdcBack, oldPen);
    DeleteObject(bulletPen);

    // Draw Asteroids
    for (const auto& a : asteroids) if (a.active) {
        COLORREF col = (a.tier == 3) ? RGB(100, 100, 110) : (a.tier == 2 ? RGB(200, 50, 50) : RGB(255, 200, 0));
        HBRUSH br = CreateSolidBrush(col);
        HBRUSH oldBr = (HBRUSH)SelectObject(hdcBack, br);
        Ellipse(hdcBack, (int)(a.pos.x - a.size), (int)(a.pos.y - a.size), (int)(a.pos.x + a.size), (int)(a.pos.y + a.size));
        SelectObject(hdcBack, oldBr);
        DeleteObject(br);
    }

    // Draw Ship
    if (!isGameOver) {
        HBRUSH shipBr = CreateSolidBrush(RGB(240, 240, 255));
        HBRUSH oldBr = (HBRUSH)SelectObject(hdcBack, shipBr);
        Rectangle(hdcBack, (int)shipPos.x, (int)shipPos.y, (int)shipPos.x + 30, (int)shipPos.y + 20);
        
        // Mouse Mode Indicator
        if (isMouseMode) {
             SetPixel(hdcBack, (int)shipPos.x+15, (int)shipPos.y+10, RGB(0,255,0));
             SetPixel(hdcBack, (int)shipPos.x+16, (int)shipPos.y+10, RGB(0,255,0));
        }
        SelectObject(hdcBack, oldBr);
        DeleteObject(shipBr);
    }

    // Draw UI Text
    SetTextColor(hdcBack, RGB(255, 255, 255));
    SetBkMode(hdcBack, TRANSPARENT);
    std::wstring ui = isGameOver ? L"CRITICAL FAILURE - PRESS ENTER TO RESTART" : 
                      L"SCORE: " + std::to_wstring(score) + 
                      (isMouseMode ? L" | CONTROL: MOUSE" : L" | CONTROL: KEYBOARD");
    TextOut(hdcBack, 15, 15, ui.c_str(), (int)ui.length());

    // Draw Bomb Cooldown Bar
    HBRUSH barBg = CreateSolidBrush(RGB(50, 50, 50));
    RECT barRect = { 200, 570, 600, 580 };
    FillRect(hdcBack, &barRect, barBg);
    DeleteObject(barBg);

    if (bombTimer <= 0) {
        HBRUSH barReady = CreateSolidBrush(RGB(0, 255, 0));
        FillRect(hdcBack, &barRect, barReady);
        DeleteObject(barReady);
    } else {
        float pct = 1.0f - (bombTimer / BOMB_COOLDOWN_MAX);
        RECT fillRect = { 200, 570, 200 + (int)(400 * pct), 580 };
        HBRUSH barWait = CreateSolidBrush(RGB(255, 0, 0));
        FillRect(hdcBack, &fillRect, barWait);
        DeleteObject(barWait);
    }

    // Flip Buffer
    BitBlt(hdcMid, 0, 0, SCREEN_W, SCREEN_H, hdcBack, 0, 0, SRCCOPY);
    BitBlt(hdc, 0, 0, SCREEN_W, SCREEN_H, hdcMid, 0, 0, SRCCOPY);
}

// --- WINDOWS BOILERPLATE ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    
    // Hide cursor in game window when in mouse mode
    if (uMsg == WM_SETCURSOR && isMouseMode && !isGameOver) {
        SetCursor(NULL);
        return true;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hI, HINSTANCE, PWSTR, int) {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc; wc.hInstance = hI; wc.lpszClassName = L"RedHorizonEngine";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);
    
    // Adjust window size to match client area
    RECT r = {0, 0, SCREEN_W, SCREEN_H};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW | WS_VISIBLE, FALSE);
    
    // Create Window with generic title
    hGameWnd = CreateWindowEx(0, L"RedHorizonEngine", L"RED HORIZON: SPACE SHOOTER", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, r.right - r.left, r.bottom - r.top, 0, 0, hI, 0);

    HDC hdc = GetDC(hGameWnd);
    hdcBack = CreateCompatibleDC(hdc);
    bmpBack = CreateCompatibleBitmap(hdc, SCREEN_W, SCREEN_H);
    SelectObject(hdcBack, bmpBack);
    hdcMid = CreateCompatibleDC(hdc);
    bmpMid = CreateCompatibleBitmap(hdc, SCREEN_W, SCREEN_H);
    SelectObject(hdcMid, bmpMid);

    MSG msg = { 0 };
    auto lastTime = std::chrono::high_resolution_clock::now();
    
    // Main Loop
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
            Sleep(1);
        }
    }
    return 0;
}