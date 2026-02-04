/*
 * THE GRANDMASTER BOT
 * ALL FEATURES COMBINED: WINDOW PHYSICS + SYSTEM ADMIN + EMOTIONAL AI
 */

// --- HEADER SIRALAMASI KRİTİK ---
#include <winsock2.h>
#include <iphlpapi.h>
#include <windows.h>
// --------------------------------

#include <shlobj.h>
#include <math.h>
#include <string>
#include <vector>
#include <dwmapi.h>
#include <cstdio>
#include <psapi.h>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define CHROMA_KEY      RGB(255, 0, 255)

// --- GLOBAL STATE ---
struct BotState {
    float x, y;
    float vx, vy;
    float angle;
    int   size;
    bool  isDragging;
    POINT dragOffset;
    POINT prevMouse;
    bool  zeroGravity;
    float bounceFactor;
    float currentMoodValue; 
    HWND  selfHwnd; 
};

BotState g_bot = { 500, 300, 0, 0, 0, 130, false, {0,0}, {0,0}, false, -0.70f, 20.0f, NULL };
RECT g_workArea = {0};

// --- CPU & SYSTEM MONITORING ---
FILETIME preIdleTime, preKernelTime, preUserTime;
void InitCPU() { GetSystemTimes(&preIdleTime, &preKernelTime, &preUserTime); }

double GetCPULoad() {
    FILETIME idle, kernel, user; GetSystemTimes(&idle, &kernel, &user);
    ULARGE_INTEGER i, k, u, pi, pk, pu;
    i.LowPart=idle.dwLowDateTime; i.HighPart=idle.dwHighDateTime;
    k.LowPart=kernel.dwLowDateTime; k.HighPart=kernel.dwHighDateTime;
    u.LowPart=user.dwLowDateTime; u.HighPart=user.dwHighDateTime;
    pi.LowPart=preIdleTime.dwLowDateTime; pi.HighPart=preIdleTime.dwHighDateTime;
    pk.LowPart=preKernelTime.dwLowDateTime; pk.HighPart=preKernelTime.dwHighDateTime;
    pu.LowPart=preUserTime.dwLowDateTime; pu.HighPart=preUserTime.dwHighDateTime;
    ULONGLONG sys = (k.QuadPart - pk.QuadPart) + (u.QuadPart - pu.QuadPart);
    ULONGLONG div = sys - (i.QuadPart - pi.QuadPart);
    preIdleTime=idle; preKernelTime=kernel; preUserTime=user;
    return sys==0 ? 0 : (double)div * 100.0 / (double)sys;
}

string GetGPUName() { DISPLAY_DEVICE dd; dd.cb=sizeof(dd); if(EnumDisplayDevices(NULL,0,&dd,0)) return string(dd.DeviceString); return "Unknown"; }
bool HasDedicatedGPU() { string n=GetGPUName(); return (n.find("Basic")==string::npos && n.find("Software")==string::npos); }
int CalculateScore(double cpu) { int s=100-(int)cpu; if(!HasDedicatedGPU()) s-=25; return max(0,min(100,s)); }
string GetIP() { char h[256]; if(gethostname(h,sizeof(h))==0) { struct hostent* he=gethostbyname(h); if(he) return string(inet_ntoa(*((struct in_addr*)he->h_addr_list[0]))); } return "N/A"; }
string GetHostName() { char b[MAX_COMPUTERNAME_LENGTH+1]; DWORD s=sizeof(b); GetComputerName(b,&s); return string(b); }
string GetRam() { MEMORYSTATUSEX m; m.dwLength=sizeof(m); GlobalMemoryStatusEx(&m); stringstream ss; ss<<"RAM: "<<m.dwMemoryLoad<<"%"; return ss.str(); }

// --- WINDOW COLLISION LOGIC ---
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (hwnd == g_bot.selfHwnd) return TRUE;
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;

    RECT r;
    if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(r)))) GetWindowRect(hwnd, &r);

    int w = r.right - r.left; int h = r.bottom - r.top;
    if (w < 50 || h < 50) return TRUE; 
    if (w > g_workArea.right - 50 && h > g_workArea.bottom - 50) return TRUE; 

    float bx = g_bot.x, by = g_bot.y, br = bx + g_bot.size, bb = by + g_bot.size;

    if (bx < r.right && br > r.left && by < r.bottom && bb > r.top) {
        float bcX = bx + g_bot.size/2.0f, bcY = by + g_bot.size/2.0f;
        float wcX = r.left + w/2.0f, wcY = r.top + h/2.0f;
        float ovX = (g_bot.size/2.0f + w/2.0f) - abs(bcX - wcX);
        float ovY = (g_bot.size/2.0f + h/2.0f) - abs(bcY - wcY);

        if (ovX < ovY) {
            g_bot.vx *= -1;
            if (bcX > wcX) g_bot.x += ovX; else g_bot.x -= ovX;
        } else {
            g_bot.vy *= g_bot.bounceFactor;
            if (bcY > wcY) g_bot.y += ovY; else g_bot.y -= ovY;
        }
    }
    return TRUE;
}

// --- VISUALS ---
void UpdateWorkArea() { SystemParametersInfo(SPI_GETWORKAREA, 0, &g_workArea, 0); }
COLORREF GetDisco() { float h=(GetTickCount()%3000)/3000.0f*6.28f; return RGB((int)(sin(h)*127+128), (int)(sin(h+2)*127+128), (int)(sin(h+4)*127+128)); }
POINT Rotate(float x, float y, float cx, float cy, float theta) { float s=sin(theta), c=cos(theta); x-=cx; y-=cy; return {(long)(x*c-y*s+cx), (long)(x*s+y*c+cy)}; }

// --- RENDERER ---
void DrawBot(HDC hdc, int w, int h) {
    HDC memDC = CreateCompatibleDC(hdc); HBITMAP memBM = CreateCompatibleBitmap(hdc, w, h); SelectObject(memDC, memBM);
    HBRUSH bg = CreateSolidBrush(CHROMA_KEY); RECT r={0,0,w,h}; FillRect(memDC, &r, bg); DeleteObject(bg);

    HBRUSH body = CreateSolidBrush(GetDisco()); HPEN pen = CreatePen(PS_SOLID, 3, RGB(10,10,10));
    SelectObject(memDC, body); SelectObject(memDC, pen);
    Ellipse(memDC, (int)g_bot.x, (int)g_bot.y, (int)(g_bot.x+g_bot.size), (int)(g_bot.y+g_bot.size));
    DeleteObject(body); DeleteObject(pen);

    float cx = g_bot.x + g_bot.size/2.0f; float cy = g_bot.y + g_bot.size/2.0f;
    POINT lEye = Rotate(cx-20, cy-10, cx, cy, g_bot.angle); POINT rEye = Rotate(cx+20, cy-10, cx, cy, g_bot.angle);
    HBRUSH white = CreateSolidBrush(RGB(255,255,255)); HBRUSH black = CreateSolidBrush(RGB(0,0,0));
    SelectObject(memDC, white); Ellipse(memDC, lEye.x-14, lEye.y-14, lEye.x+14, lEye.y+14); Ellipse(memDC, rEye.x-14, rEye.y-14, rEye.x+14, rEye.y+14);
    SelectObject(memDC, black); 
    float lx = g_bot.vx*0.5f; if(lx>6)lx=6; if(lx<-6)lx=-6; float ly = g_bot.vy*0.5f; if(ly>6)ly=6; if(ly<-6)ly=-6;
    Ellipse(memDC, lEye.x-6+(int)lx, lEye.y-6+(int)ly, lEye.x+6+(int)lx, lEye.y+6+(int)ly); Ellipse(memDC, rEye.x-6+(int)lx, rEye.y-6+(int)ly, rEye.x+6+(int)lx, rEye.y+6+(int)ly);

    // EMOTION LOGIC
    double cpu = GetCPULoad(); int score = CalculateScore(cpu);
    float targetMood = (float)((score - 50) * 0.4);
    g_bot.currentMoodValue += (targetMood - g_bot.currentMoodValue) * 0.05f;

    POINT mL=Rotate(cx-15,cy+15,cx,cy,g_bot.angle); POINT mR=Rotate(cx+15,cy+15,cx,cy,g_bot.angle);
    POINT mC1=Rotate(cx-5,cy+15+g_bot.currentMoodValue,cx,cy,g_bot.angle); POINT mC2=Rotate(cx+5,cy+15+g_bot.currentMoodValue,cx,cy,g_bot.angle);
    HPEN mPen = CreatePen(PS_SOLID, 3, RGB(0,0,0)); SelectObject(memDC, mPen); POINT bez[4]={mL,mC1,mC2,mR}; PolyBezier(memDC, bez, 4);
    DeleteObject(white); DeleteObject(black); DeleteObject(mPen);

    // HUD
    SetBkMode(memDC, TRANSPARENT);
    COLORREF hudColor = (score<40)?RGB(255,50,50):((score<70)?RGB(255,255,0):RGB(0,255,0));
    SetTextColor(memDC, hudColor); HFONT font = CreateFont(14,0,0,0,FW_BOLD,0,0,0,0,0,0,0,0,"Consolas"); SelectObject(memDC, font);
    int tx=(int)g_bot.x+g_bot.size+15, ty=(int)g_bot.y;
    RECT rBox={tx-5,ty-5,tx+190,ty+115}; HBRUSH boxBr=CreateSolidBrush(RGB(15,15,15)); FillRect(memDC, &rBox, boxBr); DeleteObject(boxBr);
    HPEN boxPen=CreatePen(PS_SOLID, 1, hudColor); SelectObject(memDC, boxPen); Rectangle(memDC, rBox.left, rBox.top, rBox.right, rBox.bottom); DeleteObject(boxPen);

    stringstream ss; ss<<"SCORE: "<<score<<"/100"; TextOut(memDC, tx, ty, ss.str().c_str(), ss.str().length());
    ss.str(""); ss<<"CPU: "<<(int)cpu<<"%"; TextOut(memDC, tx, ty+20, ss.str().c_str(), ss.str().length());
    string s=GetRam(); TextOut(memDC, tx, ty+35, s.c_str(), s.length());
    s=HasDedicatedGPU()?"GPU: OK":"GPU: NONE (-25)"; TextOut(memDC, tx, ty+50, s.c_str(), s.length());
    s="HOST: "+GetHostName(); TextOut(memDC, tx, ty+65, s.c_str(), s.length());
    s="IP: "+GetIP(); TextOut(memDC, tx, ty+80, s.c_str(), s.length());
    if(g_bot.zeroGravity) { SetTextColor(memDC, RGB(0,255,255)); TextOut(memDC, tx, ty+95, "[ ZERO GRAVITY ]", 16); }

    DeleteObject(font); BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY); DeleteObject(memBM); DeleteDC(memDC);
}

// --- LOGIC ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE: SetTimer(hwnd, 1, 16, NULL); InitCPU(); g_bot.selfHwnd = hwnd; break;
        case WM_TIMER:
            if(g_bot.isDragging) {
                POINT pt; GetCursorPos(&pt);
                g_bot.vx=(float)(pt.x-g_bot.prevMouse.x); g_bot.vy=(float)(pt.y-g_bot.prevMouse.y);
                g_bot.x=(float)(pt.x-g_bot.dragOffset.x); g_bot.y=(float)(pt.y-g_bot.dragOffset.y);
                g_bot.prevMouse=pt; g_bot.angle+=g_bot.vx*0.1f;
            } else {
                if(!g_bot.zeroGravity) { g_bot.vy+=0.8f; g_bot.vx*=0.98f; } else { g_bot.vx*=0.995f; g_bot.vy*=0.995f; }
                g_bot.x+=g_bot.vx; g_bot.y+=g_bot.vy;

                // Screen Boundaries
                if(g_bot.y+g_bot.size > g_workArea.bottom) { g_bot.y=(float)(g_workArea.bottom-g_bot.size); g_bot.vy*=g_bot.bounceFactor; }
                if(g_bot.y<0) { g_bot.y=0; g_bot.vy*=g_bot.bounceFactor; }
                if(g_bot.x<g_workArea.left) { g_bot.x=(float)g_workArea.left; g_bot.vx*=g_bot.bounceFactor; }
                if(g_bot.x+g_bot.size>g_workArea.right) { g_bot.x=(float)(g_workArea.right-g_bot.size); g_bot.vx*=g_bot.bounceFactor; }
                
                // Window Collisions
                EnumWindows(EnumWindowsProc, 0);

                g_bot.angle+=g_bot.vx/65.0f;
            }
            InvalidateRect(hwnd, NULL, FALSE); break;
        case WM_LBUTTONDOWN: {
            POINT pt; GetCursorPos(&pt);
            float cx=g_bot.x+g_bot.size/2, cy=g_bot.y+g_bot.size/2;
            if(sqrt(pow(pt.x-cx,2)+pow(pt.y-cy,2)) < g_bot.size/2) {
                g_bot.isDragging=true; SetCapture(hwnd); g_bot.prevMouse=pt;
                g_bot.dragOffset.x=pt.x-(long)g_bot.x; g_bot.dragOffset.y=pt.y-(long)g_bot.y;
                g_bot.vx=0; g_bot.vy=0;
            }
        } break;
        case WM_LBUTTONUP: if(g_bot.isDragging) { g_bot.isDragging=false; ReleaseCapture(); g_bot.vx*=1.4f; g_bot.vy*=1.4f; } break;
        case WM_RBUTTONUP: {
            POINT pt; GetCursorPos(&pt); HMENU hMenu=CreatePopupMenu();
            
            HMENU hAdmin = CreatePopupMenu();
            AppendMenu(hAdmin, MF_STRING, 101, "RegEdit"); AppendMenu(hAdmin, MF_STRING, 102, "Services");
            AppendMenu(hAdmin, MF_STRING, 103, "Event Viewer"); AppendMenu(hAdmin, MF_STRING, 104, "Control Panel");
            AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hAdmin, "Sys Admin >");

            HMENU hNet = CreatePopupMenu();
            AppendMenu(hNet, MF_STRING, 201, "IP Config"); AppendMenu(hNet, MF_STRING, 202, "Ping Google");
            AppendMenu(hNet, MF_STRING, 203, "Connections");
            AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hNet, "Net Ops >");

            HMENU hPhys = CreatePopupMenu();
            AppendMenu(hPhys, MF_STRING, 301, g_bot.zeroGravity?"Gravity: ENABLE":"Gravity: DISABLE");
            AppendMenu(hPhys, MF_STRING, 302, "Stop Motion");
            AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hPhys, "Physics >");
            
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hMenu, MF_STRING, 400, "Lock Workstation"); AppendMenu(hMenu, MF_STRING, 999, "Exit");

            int sel=TrackPopupMenu(hMenu, TPM_RETURNCMD|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            if(sel==101) ShellExecute(NULL,"open","regedit.exe",NULL,NULL,SW_SHOW);
            if(sel==102) ShellExecute(NULL,"open","services.msc",NULL,NULL,SW_SHOW);
            if(sel==103) ShellExecute(NULL,"open","eventvwr.msc",NULL,NULL,SW_SHOW);
            if(sel==104) ShellExecute(NULL,"open","control.exe",NULL,NULL,SW_SHOW);
            if(sel==201) ShellExecute(NULL,"open","cmd.exe","/k ipconfig /all",NULL,SW_SHOW);
            if(sel==202) ShellExecute(NULL,"open","cmd.exe","/k ping 8.8.8.8 -t",NULL,SW_SHOW);
            if(sel==203) ShellExecute(NULL,"open","ncpa.cpl",NULL,NULL,SW_SHOW);
            if(sel==301) g_bot.zeroGravity=!g_bot.zeroGravity;
            if(sel==302) { g_bot.vx=0; g_bot.vy=0; }
            if(sel==400) LockWorkStation();
            if(sel==999) PostQuitMessage(0); 
            DestroyMenu(hMenu);
        } break;
        case WM_PAINT: { PAINTSTRUCT ps; HDC hdc=BeginPaint(hwnd, &ps); RECT rc; GetClientRect(hwnd, &rc); DrawBot(hdc, rc.right, rc.bottom); EndPaint(hwnd, &ps); } break;
        case WM_DESTROY: WSACleanup(); PostQuitMessage(0); break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WSADATA wsaData; WSAStartup(MAKEWORD(2,2), &wsaData); UpdateWorkArea();
    WNDCLASSEX wc={0}; wc.cbSize=sizeof(WNDCLASSEX); wc.lpfnWndProc=WndProc; wc.hInstance=hInstance; wc.lpszClassName="GM_Bot"; wc.hCursor=LoadCursor(NULL, IDC_ARROW); wc.hbrBackground=(HBRUSH)CreateSolidBrush(CHROMA_KEY); RegisterClassEx(&wc);
    int w=GetSystemMetrics(SM_CXSCREEN), h=GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd=CreateWindowEx(WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_APPWINDOW, "GM_Bot", "Bot", WS_POPUP|WS_VISIBLE, 0, 0, w, h, NULL, NULL, hInstance, NULL);
    SetLayeredWindowAttributes(hwnd, CHROMA_KEY, 0, LWA_COLORKEY);
    MSG msg; while(GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); } return 0;
}