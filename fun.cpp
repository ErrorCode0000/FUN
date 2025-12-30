#include <windows.h>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <thread>
#include <atomic>
#include <string>

// === AYARLAR ===
const float GRAVITY = 0.5f;
const float BOUNCE_FACTOR = 0.6f;
const float FRICTION = 0.98f;
const int UPDATE_INTERVAL = 30;

// === GLOBAL DEGISKENLER ===
std::atomic<bool> isRunning(false); 
std::atomic<int> currentMode(0);    
std::thread workerThread;           
HWND hGuiWnd;                       

// Pencere Nesnesi
struct WindowObj {
    HWND hwnd;
    float x, y;
    float vx, vy;
    int width, height;
    bool isBeingHeld;
};

std::vector<WindowObj> windows;
RECT workArea;

// --- YARDIMCI FONKSIYONLAR ---

bool IsWindowInList(HWND hwnd) {
    for (const auto& win : windows) {
        if (win.hwnd == hwnd) return true;
    }
    return false;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (IsWindowInList(hwnd)) return TRUE;
    if (hwnd == hGuiWnd) return TRUE; 

    if (IsWindowVisible(hwnd) && !IsIconic(hwnd)) {
        char title[256];
        char className[256];
        GetWindowTextA(hwnd, title, sizeof(title));
        GetClassNameA(hwnd, className, sizeof(className));

        if (strlen(title) > 0 && 
            strcmp(className, "Shell_TrayWnd") != 0 && 
            strcmp(className, "Progman") != 0 && 
            strcmp(className, "WorkerW") != 0) {
            
            RECT rect;
            GetWindowRect(hwnd, &rect);
            
            WindowObj obj;
            obj.hwnd = hwnd;
            obj.x = (float)rect.left;
            obj.y = (float)rect.top;
            obj.width = rect.right - rect.left;
            obj.height = rect.bottom - rect.top;
            obj.vx = 0.0f;
            obj.vy = 0.0f;
            obj.isBeingHeld = false;

            windows.push_back(obj);
        }
    }
    return TRUE;
}

// --- MOD 1: FIZIK MOTORU ---
void RunPhysicsEngine() {
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    POINT cursorPos, prevCursorPos;
    GetCursorPos(&prevCursorPos);
    int updateTimer = 0;
    windows.clear(); 

    while (isRunning && currentMode == 1) {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            isRunning = false;
            break;
        }

        updateTimer++;
        if (updateTimer > UPDATE_INTERVAL) {
            SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
            windows.erase(std::remove_if(windows.begin(), windows.end(), 
                [](const WindowObj& w) { return !IsWindow(w.hwnd); }), windows.end());
            EnumWindows(EnumWindowsProc, 0);
            updateTimer = 0;
        }

        GetCursorPos(&cursorPos);
        bool isLButtonDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        
        float mouseVx = (float)(cursorPos.x - prevCursorPos.x);
        float mouseVy = (float)(cursorPos.y - prevCursorPos.y);
        
        HWND hoveredHwnd = WindowFromPoint(cursorPos);
        HWND rootHwnd = GetAncestor(hoveredHwnd, GA_ROOT);

        for (auto &win : windows) {
            if (win.hwnd == hGuiWnd) continue; 

            RECT currentRect;
            if(GetWindowRect(win.hwnd, &currentRect)) {
                win.width = currentRect.right - currentRect.left;
                win.height = currentRect.bottom - currentRect.top;
            }

            if (isLButtonDown && win.hwnd == rootHwnd) {
                win.isBeingHeld = true;
            } else if (!isLButtonDown) {
                if (win.isBeingHeld) {
                    win.vx = mouseVx;
                    win.vy = mouseVy;
                }
                win.isBeingHeld = false;
            }

            if (win.isBeingHeld) {
                GetWindowRect(win.hwnd, &currentRect);
                win.x = (float)currentRect.left;
                win.y = (float)currentRect.top;
                win.vx = 0; 
                win.vy = 0;
            } else {
                win.vy += GRAVITY;
                win.vx *= FRICTION;
                win.vy *= FRICTION;
                win.x += win.vx;
                win.y += win.vy;

                int floorLevel = workArea.bottom; 
                if (win.y + win.height > floorLevel) {
                    win.y = (float)(floorLevel - win.height);
                    if (abs(win.vy) < GRAVITY) win.vy = 0;
                    else win.vy = -win.vy * BOUNCE_FACTOR;
                    win.vx *= 0.90f; 
                }
                if (win.x + win.width > workArea.right) {
                    win.x = (float)(workArea.right - win.width);
                    win.vx = -win.vx * BOUNCE_FACTOR;
                }
                if (win.x < workArea.left) {
                    win.x = (float)workArea.left;
                    win.vx = -win.vx * BOUNCE_FACTOR;
                }
                SetWindowPos(win.hwnd, NULL, (int)win.x, (int)win.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
        }
        prevCursorPos = cursorPos;
        Sleep(10); 
    }
}

// --- MOD 2: WAVE DISCO (YENI: SOLDAN SAGA AKIS) ---
void RunDiscoWave() {
    // Sira: NumLock -> CapsLock -> ScrollLock -> CapsLock -> (Basa Don)
    // Bu, isigin soldan saga gidip, sonra geri donmesini saglar (Knight Rider efekti)
    int wavePattern[] = { VK_NUMLOCK, VK_CAPITAL, VK_SCROLL, VK_CAPITAL };
    int step = 0;

    while (isRunning && currentMode == 2) {
        // Siradaki tusa bas (Ac/Kapa)
        int key = wavePattern[step];
        
        // Tusa basma simulasyonu
        keybd_event(key, 0x45, KEYEVENTF_EXTENDEDKEY, 0);
        keybd_event(key, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);

        // Siradaki isiga gecmeden once bekle (Hiz ayari burasi: 120ms ideal smoothluk)
        Sleep(120);

        // Dizide bir sonraki adima gec
        step++;
        if (step >= 4) step = 0; // Basa sar
    }
}

// --- MOD 3: DRUNKEN MOUSE ---
void RunDrunkenMouse() {
    double t = 0;
    POINT p;
    while (isRunning && currentMode == 3) {
        GetCursorPos(&p);
        int moveX = (int)(sin(t) * 8); 
        int moveY = (int)(cos(t * 1.5) * 8);
        SetCursorPos(p.x + moveX, p.y + moveY);
        t += 0.1;
        Sleep(15);
    }
}

// --- THREAD YONETIMI ---
void StartMode(int mode) {
    isRunning = false;
    if (workerThread.joinable()) workerThread.join();

    currentMode = mode;
    isRunning = true;

    if (mode == 1) workerThread = std::thread(RunPhysicsEngine);
    else if (mode == 2) workerThread = std::thread(RunDiscoWave); // Yeni fonksiyon
    else if (mode == 3) workerThread = std::thread(RunDrunkenMouse);
}

void StopAll() {
    isRunning = false;
    if (workerThread.joinable()) workerThread.join();
    currentMode = 0;
    SetWindowText(hGuiWnd, "Physics Sandbox - HAZIR");
}

// --- UI EVENTLERI ---
#define ID_BTN_STOP    100
#define ID_BTN_PHYSICS 101
#define ID_BTN_DISCO   102
#define ID_BTN_MOUSE   103

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            CreateWindow("BUTTON", "HER SEYI DURDUR", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 
                10, 10, 260, 50, hwnd, (HMENU)ID_BTN_STOP, NULL, NULL);

            CreateWindow("BUTTON", "1. YER CEKIMI (PHYSICS)", WS_VISIBLE | WS_CHILD, 
                10, 70, 260, 40, hwnd, (HMENU)ID_BTN_PHYSICS, NULL, NULL);

            CreateWindow("BUTTON", "2. DALGALI KLAVYE (WAVE)", WS_VISIBLE | WS_CHILD, 
                10, 120, 260, 40, hwnd, (HMENU)ID_BTN_DISCO, NULL, NULL);

            CreateWindow("BUTTON", "3. SARHOS MOUSE", WS_VISIBLE | WS_CHILD, 
                10, 170, 260, 40, hwnd, (HMENU)ID_BTN_MOUSE, NULL, NULL);
            break;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_BTN_STOP:   
                    StopAll(); 
                    break;
                case ID_BTN_PHYSICS: 
                    SetWindowText(hwnd, "MOD: PHYSICS ENGAGE");
                    StartMode(1); 
                    break;
                case ID_BTN_DISCO:  
                    SetWindowText(hwnd, "MOD: KNIGHT RIDER KEYBOARD");
                    StartMode(2); 
                    break;
                case ID_BTN_MOUSE:  
                    SetWindowText(hwnd, "MOD: DRUNK MOUSE");
                    StartMode(3); 
                    break;
            }
            break;

        case WM_DESTROY:
            StopAll();
            PostQuitMessage(0);
            return 0;

        case WM_CTLCOLORBTN:
            return (LRESULT)GetStockObject(LTGRAY_BRUSH);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    srand(GetTickCount());
    const char* CLASS_NAME = "PhysicsSandboxClass";
    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    hGuiWnd = CreateWindowEx(
        WS_EX_TOPMOST, 
        CLASS_NAME, 
        "PHYSICS SANDBOX", 
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        50, 50, 300, 270, 
        NULL, NULL, hInstance, NULL
    );

    if (hGuiWnd == NULL) return 0;

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}