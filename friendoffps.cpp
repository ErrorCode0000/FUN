#include <windows.h>
#include <gl/GL.h>
#include <vector>
#include <cmath>
#include <thread>
#include <vector>

// Link necessary libraries automatically
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")

// --- CONFIGURATION ---
// 1000x1000 = 1,000,000 Particles.
// If you have a high-end CPU, you can increase this.
const int GRID_RES = 1000; 
const int TOTAL_PARTICLES = GRID_RES * GRID_RES;

// --- DATA STRUCTURES ---

// Data sent to GPU (Must be compact)
struct RenderData {
    float x, y;    // Position
    float r, g, b; // Color
};

// Data used for Physics calculations (CPU only)
struct PhysicsData {
    float vx, vy; // Velocity
    float ox, oy; // Original Origin (Home)
};

// --- GLOBAL VARIABLES ---
std::vector<RenderData> renderBuffer;
std::vector<PhysicsData> physicsBuffer;

// Interaction
float mouseX = 0, mouseY = 0;
bool isLeftDown = false;
bool isRightDown = false;
float timeTick = 0.0f;

// Window
int windowWidth = 1280;
int windowHeight = 720;
int viewportSize = 0, viewportX = 0, viewportY = 0;
bool isFullscreen = false;
WINDOWPLACEMENT g_wpPrev = { sizeof(g_wpPrev) };

// Threading
const int THREAD_COUNT = std::thread::hardware_concurrency(); // Detects CPU cores

// --- INITIALIZATION ---
void InitializeSystem() {
    renderBuffer.resize(TOTAL_PARTICLES);
    physicsBuffer.resize(TOTAL_PARTICLES);

    for (int y = 0; y < GRID_RES; y++) {
        for (int x = 0; x < GRID_RES; x++) {
            int i = y * GRID_RES + x;

            // Normalize coordinates to -1.0 to 1.0
            float px = ((float)x / GRID_RES) * 2.0f - 1.0f;
            float py = ((float)y / GRID_RES) * 2.0f - 1.0f;

            // Render Data (Start Blue/Purple)
            renderBuffer[i].x = px;
            renderBuffer[i].y = py;
            renderBuffer[i].r = 0.1f;
            renderBuffer[i].g = 0.5f;
            renderBuffer[i].b = 1.0f;

            // Physics Data
            physicsBuffer[i].vx = 0.0f;
            physicsBuffer[i].vy = 0.0f;
            physicsBuffer[i].ox = px;
            physicsBuffer[i].oy = py;
        }
    }
}

// --- PHYSICS ENGINE (THREAD WORKER) ---
void UpdatePhysicsChunk(int startIdx, int endIdx) {
    for (int i = startIdx; i < endIdx; i++) {
        RenderData& r = renderBuffer[i];
        PhysicsData& p = physicsBuffer[i];

        // 1. Elasticity (Return to original position)
        // Spring force calculation
        float dx_home = p.ox - r.x;
        float dy_home = p.oy - r.y;
        
        p.vx += dx_home * 0.015f; // Stiffness
        p.vy += dy_home * 0.015f;

        // 2. Mouse Interaction
        if (isLeftDown || isRightDown) {
            float dx = r.x - mouseX;
            float dy = r.y - mouseY;
            // Use squared distance to avoid expensive sqrt() when possible
            float distSq = dx * dx + dy * dy;

            if (distSq < 0.2f) { // Interaction Radius
                float dist = sqrt(distSq) + 0.0001f;
                float force = (0.45f - dist) * 0.12f;

                if (isRightDown) force = -force; // Invert for Black Hole effect

                p.vx += (dx / dist) * force;
                p.vy += (dy / dist) * force;
            }
        }

        // 3. Friction (Damping)
        // Prevents particles from oscillating forever
        p.vx *= 0.92f;
        p.vy *= 0.92f;

        // 4. Apply Velocity
        r.x += p.vx;
        r.y += p.vy;

        // 5. Dynamic Coloring (Visuals)
        // Change color based on velocity (Kinetic Energy)
        float speed = abs(p.vx) + abs(p.vy);
        
        // Base color (Deep Blue) + Speed (Cyan/White)
        r.r = 0.1f + (speed * 10.0f);
        r.g = 0.3f + (speed * 5.0f);
        r.b = 0.8f + (r.x * 0.2f); // Slight gradient based on X pos
    }
}

// --- MAIN UPDATE LOOP ---
void Update() {
    // Distribute workload across CPU cores
    std::vector<std::thread> workers;
    int chunk = TOTAL_PARTICLES / THREAD_COUNT;

    for (int i = 0; i < THREAD_COUNT; i++) {
        int start = i * chunk;
        int end = (i == THREAD_COUNT - 1) ? TOTAL_PARTICLES : start + chunk;
        workers.emplace_back(UpdatePhysicsChunk, start, end);
    }

    // Wait for all threads to finish
    for (auto& t : workers) {
        t.join();
    }
}

// --- RENDERING ---
void Render() {
    // Handle Aspect Ratio (Keep particles square)
    viewportSize = (windowWidth < windowHeight) ? windowWidth : windowHeight;
    viewportX = (windowWidth - viewportSize) / 2;
    viewportY = (windowHeight - viewportSize) / 2;

    glViewport(viewportX, viewportY, viewportSize, viewportSize);
    
    // Clear Screen (Void Color)
    glClearColor(0.01f, 0.01f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Optimized OpenGL Rendering
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    // Point OpenGL to our data structure
    // Stride is sizeof(RenderData) to skip to the next particle correctly
    glVertexPointer(2, GL_FLOAT, sizeof(RenderData), &renderBuffer[0].x);
    glColorPointer(3, GL_FLOAT, sizeof(RenderData), &renderBuffer[0].r);

    // Additive Blending (Makes particles glow when they stack)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); 
    
    glPointSize(1.1f); // Fine particles

    // Draw 1 Million points in a single draw call
    glDrawArrays(GL_POINTS, 0, TOTAL_PARTICLES);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
}

// --- WINDOW MANAGEMENT ---
void ToggleFullscreen(HWND hwnd) {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
    if (dwStyle & WS_OVERLAPPEDWINDOW) {
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
        SetWindowLong(hwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &g_wpPrev);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        isFullscreen = false;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY: PostQuitMessage(0); return 0;
        case WM_SIZE:
            windowWidth = LOWORD(lParam);
            windowHeight = HIWORD(lParam);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) PostQuitMessage(0);
            if (wParam == VK_F11) ToggleFullscreen(hwnd);
            return 0;
        case WM_LBUTTONDOWN: isLeftDown = true; return 0;
        case WM_LBUTTONUP: isLeftDown = false; return 0;
        case WM_RBUTTONDOWN: isRightDown = true; return 0;
        case WM_RBUTTONUP: isRightDown = false; return 0;
        case WM_MOUSEMOVE:
            // Calculate mouse position relative to the centered viewport
            float rawX = (float)LOWORD(lParam);
            float rawY = (float)HIWORD(lParam);
            mouseX = (rawX - viewportX) / viewportSize * 2.0f - 1.0f;
            mouseY = -((rawY - viewportY) / viewportSize * 2.0f - 1.0f);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmdLine, int nShow) {
    // Initialization
    InitializeSystem();

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "ParticleEngine";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    HWND hwnd = CreateWindow("ParticleEngine", "High Performance Particle Simulation", 
        WS_OVERLAPPEDWINDOW, 
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768, 
        NULL, NULL, hInstance, NULL);

    // Setup OpenGL
    HDC hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
    SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
    wglMakeCurrent(hdc, wglCreateContext(hdc));

    ShowWindow(hwnd, nShow);

    // Game Loop
    MSG msg = {0};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            Update();
            Render();
            SwapBuffers(hdc);
        }
    }
    return 0;
}