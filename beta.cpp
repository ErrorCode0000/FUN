/*
 * Developer: CBA (Software Genius)
 * Engine: CBA 100x VISUALS (Quantum Glow Edition)
 * Visuals: Additive Blending, Gaussian Falloff, Hyper-Neon Colors
 */

#include <windows.h>
#include <GL/gl.h>
#include <cmath>
#include <vector>

// --- NVIDIA & AMD GPU ZORLAMA ---
extern "C" {
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

// --- OPENGL TANIMLARI ---
#define GL_COMPUTE_SHADER                 0x91B9
#define GL_STATIC_DRAW                    0x88E4
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_SHADER_STORAGE_BUFFER          0x90D2
#define GL_SHADER_STORAGE_BARRIER_BIT     0x2000
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_PROGRAM_POINT_SIZE             0x8642
#define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB      0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB  0x00000001

typedef ptrdiff_t GLsizeiptr; typedef char GLchar;
typedef void (APIENTRY *PFNGLGENBUFFERSPROC) (GLsizei n, GLuint *buffers);
typedef void (APIENTRY *PFNGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
typedef void (APIENTRY *PFNGLBUFFERDATAPROC) (GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef GLuint (APIENTRY *PFNGLCREATESHADERPROC) (GLenum type);
typedef void (APIENTRY *PFNGLSHADERSOURCEPROC) (GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void (APIENTRY *PFNGLCOMPILESHADERPROC) (GLuint shader);
typedef GLuint (APIENTRY *PFNGLCREATEPROGRAMPROC) (void);
typedef void (APIENTRY *PFNGLATTACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (APIENTRY *PFNGLLINKPROGRAMPROC) (GLuint program);
typedef void (APIENTRY *PFNGLUSEPROGRAMPROC) (GLuint program);
typedef void (APIENTRY *PFNGLGETSHADERIVPROC) (GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef GLint (APIENTRY *PFNGLGETUNIFORMLOCATIONPROC) (GLuint program, const GLchar *name);
typedef void (APIENTRY *PFNGLUNIFORM1FPROC) (GLint location, GLfloat v0);
typedef void (APIENTRY *PFNGLUNIFORM2FPROC) (GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRY *PFNGLUNIFORM1IPROC) (GLint location, GLint v0);
typedef void (APIENTRY *PFNGLGENVERTEXARRAYSPROC) (GLsizei n, GLuint *arrays);
typedef void (APIENTRY *PFNGLBINDVERTEXARRAYPROC) (GLuint array);
typedef void (APIENTRY *PFNGLDISPATCHCOMPUTEPROC) (GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
typedef void (APIENTRY *PFNGLMEMORYBARRIERPROC) (GLbitfield barriers);
typedef void (APIENTRY *PFNGLBINDBUFFERBASEPROC) (GLenum target, GLuint index, GLuint buffer);
typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC) (HDC hDC, HGLRC hShareContext, const int *attribList);
typedef BOOL (WINAPI *PFNWGLSWAPINTERVALEXTPROC) (int interval);

PFNGLGENBUFFERSPROC glGenBuffers; PFNGLBINDBUFFERPROC glBindBuffer; PFNGLBUFFERDATAPROC glBufferData;
PFNGLCREATESHADERPROC glCreateShader; PFNGLSHADERSOURCEPROC glShaderSource; PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLCREATEPROGRAMPROC glCreateProgram; PFNGLATTACHSHADERPROC glAttachShader; PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLUSEPROGRAMPROC glUseProgram; PFNGLGETSHADERIVPROC glGetShaderiv; PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation; PFNGLUNIFORM1FPROC glUniform1f; PFNGLUNIFORM2FPROC glUniform2f;
PFNGLUNIFORM1IPROC glUniform1i; PFNGLGENVERTEXARRAYSPROC glGenVertexArrays; PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
PFNGLDISPATCHCOMPUTEPROC glDispatchCompute; PFNGLMEMORYBARRIERPROC glMemoryBarrier; PFNGLBINDBUFFERBASEPROC glBindBufferBase;
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB; PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

// --- AYARLAR ---
const int TEX_SIZE = 1024; // 1 Milyon Parçacık
const int NUM = TEX_SIZE * TEX_SIZE;
int scrW = 1920, scrH = 1080;

// --- 1. FİZİK (CIVA GİBİ AKIŞKAN) ---
const char* cs_src = R"(
#version 430
layout(local_size_x = 256) in;

struct P { vec2 pos; vec2 vel; };
layout(std430, binding = 0) buffer D { P p[]; };

uniform vec2 mouse;
uniform int mode; 
uniform float aspect;

void main() {
    uint i = gl_GlobalInvocationID.x;
    if(i >= 1048576) return; 

    vec2 pos = p[i].pos;
    vec2 vel = p[i].vel;

    // Home (Grid)
    float tx = float(i % 1024) / 1024.0;
    float ty = float(i / 1024) / 1024.0;
    vec2 home = vec2(tx * 2.0 - 1.0, ty * 2.0 - 1.0);
    home.x *= aspect; 

    // Yay (Biraz daha sert, sıvı yüzey gerilimi gibi)
    vec2 diff = home - pos;
    vel += diff * 0.005; 

    // Mouse
    if(mode > 0) {
        vec2 dir = mouse - pos;
        float dist = length(dir);
        float radius = (mode == 3) ? 0.7 : 0.5; 

        if(dist < radius) {
            float f = pow((radius - dist) * 2.5, 2.0); // Kuvvetin karesini al (Daha patlayıcı)
            
            if(mode == 1) vel += normalize(dir) * f * 0.01;
            else if(mode == 2) {
                vec2 tangent = vec2(-dir.y, dir.x); 
                vel += normalize(tangent) * f * 0.04; 
                vel += normalize(dir) * f * 0.005;
            }
            else if(mode == 3) vel -= normalize(dir) * f * 0.08; 
        }
    }

    vel *= 0.95; // Biraz daha fazla sürtünme (Tok his)
    pos += vel;

    // Edge Clamp
    float limitX = aspect; 
    float limitY = 1.0;    
    if (pos.x < -limitX) { pos.x = -limitX; vel.x = 0.0; } 
    if (pos.x >  limitX) { pos.x =  limitX; vel.x = 0.0; } 
    if (pos.y < -limitY) { pos.y = -limitY; vel.y = 0.0; } 
    if (pos.y >  limitY) { pos.y =  limitY; vel.y = 0.0; } 

    p[i].pos = pos;
    p[i].vel = vel;
}
)";

// --- 2. GÖRSEL (100x UPGRADE: HYPER-NEON) ---
const char* vs_src = R"(
#version 430
struct P { vec2 pos; vec2 vel; };
layout(std430, binding = 0) buffer D { P p[]; };
uniform float aspect;
out vec3 color;
out float speed;

// Cosine Based Palette (IQ Style)
vec3 palette(float t) {
    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.263, 0.416, 0.557); // Cyberpunk Base
    return a + b * cos(6.28318 * (c * t + d));
}

void main() {
    vec2 pos = p[gl_VertexID].pos;
    vec2 vel = p[gl_VertexID].vel;
    gl_Position = vec4(pos.x / aspect, pos.y, 0.0, 1.0);
    
    speed = length(vel) * 40.0; // Hızı fragment shader'a gönder
    
    // Hıza göre renk kayması
    vec3 glowCol = palette(speed * 0.5 + 0.1);
    
    // Hızlanınca beyaza çalar (Akkor etkisi)
    color = mix(glowCol, vec3(1.2, 1.2, 1.5), min(speed * 0.6, 1.0));
    
    // Sabit, net boyut (Fragment shader halledecek gerisini)
    gl_PointSize = 3.0; 
}
)";

// --- 3. GLOW RENDER (QUANTUM GLOW) ---
const char* fs_src = R"(
#version 430
in vec3 color;
in float speed;
out vec4 f;
void main() { 
    // Koordinatı merkeze al (-0.5 ile 0.5 arası)
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);
    
    // Dairenin dışını at
    if(dist > 0.5) discard;
    
    // GAUSSIAN GLOW:
    // Doğrusal değil, üstel (exponential) bir parlaklık düşüşü.
    // Bu, noktanın merkezini çok parlak, kenarlarını sisli yapar.
    float glow = exp(-dist * 4.0); 
    
    // Hızlı gidenler daha opak, duranlar daha hayaletimsi
    float alpha = glow * (0.6 + min(speed, 0.4));
    
    f = vec4(color, alpha); 
}
)";

// --- SİSTEM ---
void LoadGL() {
    glGenBuffers = (PFNGLGENBUFFERSPROC)wglGetProcAddress("glGenBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)wglGetProcAddress("glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)wglGetProcAddress("glBufferData");
    glCreateShader = (PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader");
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram");
    glUseProgram = (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)wglGetProcAddress("glGetShaderiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)wglGetProcAddress("glGetShaderInfoLog");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation");
    glUniform1f = (PFNGLUNIFORM1FPROC)wglGetProcAddress("glUniform1f");
    glUniform2f = (PFNGLUNIFORM2FPROC)wglGetProcAddress("glUniform2f");
    glUniform1i = (PFNGLUNIFORM1IPROC)wglGetProcAddress("glUniform1i");
    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)wglGetProcAddress("glGenVertexArrays");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)wglGetProcAddress("glBindVertexArray");
    glDispatchCompute = (PFNGLDISPATCHCOMPUTEPROC)wglGetProcAddress("glDispatchCompute");
    glMemoryBarrier = (PFNGLMEMORYBARRIERPROC)wglGetProcAddress("glMemoryBarrier");
    glBindBufferBase = (PFNGLBINDBUFFERBASEPROC)wglGetProcAddress("glBindBufferBase");
    wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
}

GLuint Shader(GLenum t, const char* s) {
    GLuint o = glCreateShader(t); glShaderSource(o,1,&s,0); glCompileShader(o);
    return o;
}

bool run = true;
int mMode = 0;
float mx = 0, my = 0;
bool isFullscreen = false;
WINDOWPLACEMENT g_wpPrev = { sizeof(g_wpPrev) };

void ToggleFullscreen(HWND hwnd) {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
    if (dwStyle & WS_OVERLAPPEDWINDOW) {
        MONITORINFO mi = { sizeof(mi) };
        if (GetWindowPlacement(hwnd, &g_wpPrev) && GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
        isFullscreen = true;
    } else {
        SetWindowLong(hwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &g_wpPrev);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        isFullscreen = false;
    }
}

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch(m) {
        case WM_CLOSE: run = false; break;
        case WM_SIZE: scrW=LOWORD(l); scrH=HIWORD(l); glViewport(0,0,scrW,scrH); break;
        case WM_KEYDOWN: 
            if(w==VK_ESCAPE) run=false; 
            if(w==VK_F11) ToggleFullscreen(h);
            break;
        case WM_LBUTTONDOWN: 
            if(GetKeyState(VK_SHIFT) & 0x8000) mMode = 3; else mMode = 1; SetCapture(h); break;
        case WM_RBUTTONDOWN: mMode = 2; SetCapture(h); break; 
        case WM_LBUTTONUP:
        case WM_RBUTTONUP: mMode = 0; ReleaseCapture(); break;
        case WM_MOUSEMOVE: {
            float asp = (float)scrW/scrH;
            mx = ((float)LOWORD(l)/scrW*2.0f-1.0f)*asp;
            my = -((float)HIWORD(l)/scrH*2.0f-1.0f);
        } break;
        default: return DefWindowProc(h,m,w,l);
    }
    return 0;
}

int main() {
    WNDCLASS wc = {0}; wc.lpfnWndProc=WndProc; wc.hInstance=GetModuleHandle(0);
    wc.lpszClassName="CBA_100X"; RegisterClass(&wc);
    HWND hwnd = CreateWindowEx(0,"CBA_100X","CBA 100x VISUALS",WS_OVERLAPPEDWINDOW|WS_VISIBLE,0,0,1280,720,0,0,wc.hInstance,0);
    ToggleFullscreen(hwnd); // Auto Fullscreen

    HDC hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {sizeof(pfd),1,PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,PFD_TYPE_RGBA,32};
    SetPixelFormat(hdc,ChoosePixelFormat(hdc,&pfd),&pfd);
    HGLRC t = wglCreateContext(hdc); wglMakeCurrent(hdc,t);
    LoadGL();
    int a[]={2091,4,2092,3,9126,1,0}; 
    HGLRC hrc = wglCreateContextAttribsARB(hdc,0,a);
    wglMakeCurrent(hdc,hrc); wglDeleteContext(t);
    if(wglSwapIntervalEXT) wglSwapIntervalEXT(0);

    GLuint cp=glCreateProgram(); glAttachShader(cp,Shader(GL_COMPUTE_SHADER,cs_src)); glLinkProgram(cp);
    GLuint rp=glCreateProgram(); glAttachShader(rp,Shader(GL_VERTEX_SHADER,vs_src)); glAttachShader(rp,Shader(GL_FRAGMENT_SHADER,fs_src)); glLinkProgram(rp);

    struct P { float px,py,vx,vy; };
    std::vector<P> pts(NUM);
    float asp = (float)scrW/scrH;
    
    // STRICT GRID (No Randomness)
    for(int i=0; i<NUM; ++i) {
        float tx = float(i%TEX_SIZE)/TEX_SIZE;
        float ty = float(i/TEX_SIZE)/TEX_SIZE;
        pts[i].px = (tx*2.0f-1.0f)*asp; 
        pts[i].py = (ty*2.0f-1.0f);
    }

    GLuint sb; glGenBuffers(1,&sb); glBindBuffer(GL_SHADER_STORAGE_BUFFER,sb);
    glBufferData(GL_SHADER_STORAGE_BUFFER,sizeof(P)*NUM,pts.data(),GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,sb);
    GLuint vao; glGenVertexArrays(1,&vao);

    // --- VISUAL MAGIC: ADDITIVE BLENDING ---
    // Bu ayar, üst üste binen noktaların ışığını toplar.
    // Çok fazla nokta üst üste gelirse orası BEMBEYAZ yanar (Bloom Effect).
    glEnable(GL_BLEND); 
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); 
    
    glEnable(GL_PROGRAM_POINT_SIZE);

    MSG msg;
    float time = 0.0f;

    while(run) {
        if(PeekMessage(&msg,0,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        else {
            time += 0.01f;
            float curAsp = (float)scrW/scrH;

            glUseProgram(cp);
            glUniform2f(glGetUniformLocation(cp,"mouse"),mx,my);
            glUniform1i(glGetUniformLocation(cp,"mode"),mMode);
            glUniform1f(glGetUniformLocation(cp,"aspect"),curAsp);
            glDispatchCompute((NUM+255)/256,1,1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            glViewport(0,0,scrW,scrH);
            
            // Background: Derin, neredeyse siyah Lacivert
            glClearColor(0.005f, 0.005f, 0.02f, 1.0f);
            
            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(rp);
            glUniform1f(glGetUniformLocation(rp,"aspect"),curAsp);
            glBindVertexArray(vao);
            glDrawArrays(GL_POINTS,0,NUM);

            SwapBuffers(hdc);
        }
    }
    return 0;
}
