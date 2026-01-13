#include <windows.h>
#include <gl/GL.h>
#include <vector>
#include <cmath>

// Auto-link libraries
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")

// Configuration: 750x750 = 562,500 Particles (Sweet Spot)
const int GRID_RES = 750;
const int TOTAL_PARTICLES = GRID_RES * GRID_RES;

struct Particle {
    float x, y;   // Current position
    float vx, vy; // Velocity
    float ox, oy; // Original position (Home)
};

// Global Variables
Particle* particles = nullptr;
float mouseX = 0, mouseY = 0;
bool isTouching = false;
float timeTick = 0.0f;

// Window & Viewport Variables
int windowWidth = 800;
int windowHeight = 600;
int viewportX = 0, viewportY = 0;
int viewportSize = 0;

// Fullscreen State
bool isFullscreen = false;
WINDOWPLACEMENT g_wpPrev = { sizeof(g_wpPrev) };

void InitializeParticles() {
    particles = new Particle[TOTAL_PARTICLES];
    for (int y = 0; y < GRID_RES; y++) {
        for (int x = 0; x < GRID_RES; x++) {
            // Map grid to coordinate system (-0.9 to 0.9)
            float px = ((float)x / GRID_RES) * 1.8f - 0.9f;
            float py = ((float)y / GRID_RES) * 1.8f - 0.9f;
            int idx = y * GRID_RES + x;
            particles[idx] = { px, py, 0, 0, px, py };
        }
    }
}

void UpdatePhysics() {
    for (int i = 0; i < TOTAL_PARTICLES; i++) {
        Particle& p = particles[i];

        // 1. Elasticity (Return to home)
        p.vx += (p.ox - p.x) * 0.012f;
        p.vy += (p.oy - p.y) * 0.012f;

        // 2. Repulsion (Reverse Magnet)
        if (isTouching) {
            float dx = p.x - mouseX;
            float dy = p.y - mouseY;
            float distSq = dx * dx + dy * dy;
            
            if (distSq < 0.09f) { // Radius ~0.3
                float dist = sqrt(distSq) + 0.0001f;
                float force = (0.3f - dist) * 0.12f;
                p.vx += (dx / dist) * force;
                p.vy += (dy / dist) * force;
            }
        }

        // 3. Friction
        p.vx *= 0.92f;
        p.vy *= 0.92f;
        p.x += p.vx;
        p.y += p.vy;

        // 4. Boundary Bounce
        if (p.x > 0.98f) { p.x = 0.98f; p.vx *= -0.8f; }
        if (p.x < -0.98f) { p.x = -0.98f; p.vx *= -0.8f; }
        if (p.y > 0.98f) { p.y = 0.98f; p.vy *= -0.8f; }
        if (p.y < -0.98f) { p.y = -0.98f; p.vy *= -0.8f; }
    }
}

void RenderScene() {
    // Determine the largest square viewport that fits in the window
    viewportSize = (windowWidth < windowHeight) ? windowWidth : windowHeight;
    viewportX = (windowWidth - viewportSize) / 2;
    viewportY = (windowHeight - viewportSize) / 2;

    // Set OpenGL viewport to center the simulation
    glViewport(viewportX, viewportY, viewportSize, viewportSize);

    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    timeTick += 0.008f;
    glPointSize(2.2f);

    glBegin(GL_POINTS);
    for (int i = 0; i < TOTAL_PARTICLES; i++) {
        Particle& p = particles[i];
        
        // Rainbow Math
        float wave = p.ox * 2.0f + p.oy * 2.0f + timeTick;
        float r = sin(wave) * 0.5f + 0.5f;
        float g = sin(wave + 2.09f) * 0.5f + 0.5f;
        float b = sin(wave + 4.18f) * 0.5f + 0.5f;

        // Speed Glow
        float speed = abs(p.vx) + abs(p.vy);
        float alpha = 0.6f + (speed * 5.0f); 
        if(alpha > 1.0f) alpha = 1.0f;

        glColor4f(r, g, b, alpha);
        glVertex2f(p.x, p.y);
    }
    glEnd();
}

void ToggleFullscreen(HWND hwnd) {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
    if (dwStyle & WS_OVERLAPPEDWINDOW) {
        // Switch to Fullscreen
        MONITORINFO mi = { sizeof(mi) };
        if (GetWindowPlacement(hwnd, &g_wpPrev) && GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(hwnd, HWND_TOP,
                mi.rcMonitor.left, mi.rcMonitor.top,
                mi.rcMonitor.right - mi.rcMonitor.left,
                mi.rcMonitor.bottom - mi.rcMonitor.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
        isFullscreen = true;
    } else {
        // Restore Windowed
        SetWindowLong(hwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &g_wpPrev);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        isFullscreen = false;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            if (particles) delete[] particles;
            PostQuitMessage(0);
            return 0;

        case WM_SIZE:
            windowWidth = LOWORD(lParam);
            windowHeight = HIWORD(lParam);
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_F11) {
                ToggleFullscreen(hwnd);
            }
            if (wParam == VK_ESCAPE) {
                PostQuitMessage(0);
            }
            return 0;

        case WM_LBUTTONDOWN: isTouching = true; return 0;
        case WM_LBUTTONUP: isTouching = false; return 0;
        
        case WM_MOUSEMOVE:
            // Correct mouse mapping based on the centered viewport
            // We need to shift coordinate relative to viewportX/Y and scale by viewportSize
            float rawX = (float)LOWORD(lParam);
            float rawY = (float)HIWORD(lParam);
            
            // Map to -1.0 to 1.0 range based ONLY on the square viewport
            mouseX = (rawX - viewportX) / viewportSize * 2.0f - 1.0f;
            mouseY = -((rawY - viewportY) / viewportSize * 2.0f - 1.0f);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmdLine, int nShow) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "SatisfyingAppClass";
    wc.hCursor = LoadCursor(NULL, IDC_HAND);
    RegisterClass(&wc);

    HWND hwnd = CreateWindow("SatisfyingAppClass", "Satisfying App", 
        WS_OVERLAPPEDWINDOW, 
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, 
        NULL, NULL, hInstance, NULL);

    if (!hwnd) return 0;

    HDC hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
    SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
    wglMakeCurrent(hdc, wglCreateContext(hdc));

    InitializeParticles();
    ShowWindow(hwnd, nShow);

    MSG msg = {0};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            UpdatePhysics();
            RenderScene();
            SwapBuffers(hdc);
        }
    }
    return 0;
}